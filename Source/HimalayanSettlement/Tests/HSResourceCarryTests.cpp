#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalActor.h"
#include "Animals/HSAnimalDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Interaction/HSInteractable.h"
#include "Player/HSPlayerCharacter.h"
#include "Resources/HSResourceCarryComponent.h"

namespace
{
UHSAnimalDefinition* CreateCarryTestDeer(UObject* Outer)
{
	UHSAnimalDefinition* Deer = NewObject<UHSAnimalDefinition>(Outer);
	Deer->DisplayName = FText::FromString(TEXT("Deer"));
	Deer->SpeciesTag = TAG_HS_Animal_Deer;
	Deer->MaxHealth = 70.0f;
	Deer->HarvestDrops = {
		FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 },
		FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 }
	};
	return Deer;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSResourceCarryDefaultsTest,
	"HimalayanSettlement.Resources.CarryDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSResourceCarryDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSResourceCarryComponent* Carry = NewObject<UHSResourceCarryComponent>();

	TestNotNull(TEXT("Resource carry component should be constructible."), Carry);
	TestFalse(TEXT("Resource carry component should not tick by default."), Carry->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("Carry should start empty."), Carry->GetCarriedItems().Num(), 0);
	TestEqual(TEXT("Invalid tag quantity should be zero."), Carry->GetQuantityForTag(FGameplayTag()), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSResourceCarryMergesStacksTest,
	"HimalayanSettlement.Resources.CarryMergesStacksByTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSResourceCarryMergesStacksTest::RunTest(const FString& Parameters)
{
	UHSResourceCarryComponent* Carry = NewObject<UHSResourceCarryComponent>();

	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 3 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 });
	Carry->AddStack(FHSItemStack{ FGameplayTag(), 99 });

	TestEqual(TEXT("Carry should merge matching item tags."), Carry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer), 5);
	TestEqual(TEXT("Carry should keep separate item tags."), Carry->GetQuantityForTag(TAG_HS_Resource_Hide_Deer), 1);
	TestEqual(TEXT("Carry should ignore invalid stacks."), Carry->GetCarriedItems().Num(), 2);
	TestTrue(TEXT("Carry should satisfy valid requirements."), Carry->HasAtLeast(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 4 }));
	TestFalse(TEXT("Carry should reject missing requirements."), Carry->HasAtLeast(FHSItemStack{ TAG_HS_Resource_Hide_Deer, 2 }));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSResourceCarryConsumesStacksAtomicallyTest,
	"HimalayanSettlement.Resources.CarryConsumesStacksAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSResourceCarryConsumesStacksAtomicallyTest::RunTest(const FString& Parameters)
{
	UHSResourceCarryComponent* Carry = NewObject<UHSResourceCarryComponent>();

	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 3 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 });

	TestFalse(
		TEXT("Missing stack in a multi-stack consume should fail before removing anything."),
		Carry->TryConsumeStacks({
			FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 },
			FHSItemStack{ TAG_HS_Resource_Hide_Deer, 2 }
		}));
	TestEqual(TEXT("Failed consume should leave meat untouched."), Carry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer), 3);
	TestEqual(TEXT("Failed consume should leave hide untouched."), Carry->GetQuantityForTag(TAG_HS_Resource_Hide_Deer), 1);

	TestTrue(
		TEXT("Valid multi-stack consume should remove the requested quantities."),
		Carry->TryConsumeStacks({
			FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 },
			FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 }
		}));
	TestEqual(TEXT("Partial remaining meat should stay carried."), Carry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer), 1);
	TestEqual(TEXT("Fully consumed hide stack should be removed."), Carry->GetQuantityForTag(TAG_HS_Resource_Hide_Deer), 0);
	TestEqual(TEXT("Only the remaining meat stack should stay in carry state."), Carry->GetCarriedItems().Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSAnimalHarvestTransfersToPlayerCarryTest,
	"HimalayanSettlement.Resources.HarvestTransfersDropsToPlayerCarry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSAnimalHarvestTransfersToPlayerCarryTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	AHSAnimalActor* Deer = NewObject<AHSAnimalActor>();
	Deer->SetAnimalDefinition(CreateCarryTestDeer(Deer));
	Deer->ApplyHuntingDamage(75.0f, Player);

	UHSResourceCarryComponent* Carry = Player->GetResourceCarryComponent();
	TestNotNull(TEXT("Player should have a resource carry component."), Carry);
	TestTrue(TEXT("Dead deer should be interactable for harvest."), HSInteraction::CanInteract(Deer, Player));

	HSInteraction::Interact(Deer, Player);

	TestEqual(TEXT("Harvested meat should transfer to player carry."), Carry ? Carry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer) : 0, 2);
	TestEqual(TEXT("Harvested hide should transfer to player carry."), Carry ? Carry->GetQuantityForTag(TAG_HS_Resource_Hide_Deer) : 0, 1);
	TestFalse(TEXT("Harvested deer should no longer be interactable."), HSInteraction::CanInteract(Deer, Player));

	return true;
}

#endif
