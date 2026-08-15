#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalActor.h"
#include "Animals/HSAnimalDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Hunting/HSHuntingComponent.h"
#include "Player/HSPlayerCharacter.h"
#include "Resources/HSResourceCarryComponent.h"

namespace
{
UHSAnimalDefinition* CreateHuntingTestDeer(UObject* Outer)
{
	UHSAnimalDefinition* Deer = NewObject<UHSAnimalDefinition>(Outer);
	Deer->DisplayName = FText::FromString(TEXT("Deer"));
	Deer->SpeciesTag = TAG_HS_Animal_Deer;
	Deer->MaxHealth = 70.0f;
	Deer->HarvestDrops = { FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 } };
	return Deer;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHuntingComponentDefaultsTest,
	"HimalayanSettlement.Hunting.ComponentDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHuntingComponentDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSHuntingComponent* Hunting = NewObject<UHSHuntingComponent>();

	TestNotNull(TEXT("Hunting component should be constructible."), Hunting);
	TestFalse(TEXT("Hunting component should not tick by default."), Hunting->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("Prototype hunt distance should stay short."), Hunting->GetHuntDistance(), 1200.0f);
	TestEqual(TEXT("Prototype hunt damage should kill the first deer definition."), Hunting->GetHuntDamage(), 75.0f);
	TestEqual(TEXT("Hunting traces should use visibility by default."), Hunting->GetTraceChannel().GetValue(), static_cast<uint8>(ECC_Visibility));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHuntingAppliesDamageTest,
	"HimalayanSettlement.Hunting.ComponentAppliesDamageToAnimal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHuntingAppliesDamageTest::RunTest(const FString& Parameters)
{
	UHSHuntingComponent* Hunting = NewObject<UHSHuntingComponent>();
	AHSAnimalActor* Deer = NewObject<AHSAnimalActor>();
	Deer->SetAnimalDefinition(CreateHuntingTestDeer(Deer));

	TestTrue(TEXT("Hunting component should apply damage to a live deer."), Hunting->ApplyDamageToTarget(Deer));
	TestTrue(TEXT("Prototype damage should kill the test deer."), Deer->IsDead());
	TestFalse(TEXT("Hunting component should not damage an already-dead deer."), Hunting->ApplyDamageToTarget(Deer));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasHuntingComponentTest,
	"HimalayanSettlement.Hunting.PlayerHasNativeHuntingComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasHuntingComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own the native hunting component."), Player ? Player->GetHuntingComponent() : nullptr);
	TestNotNull(TEXT("Player should own the native resource carry component."), Player ? Player->GetResourceCarryComponent() : nullptr);

	return true;
}

#endif
