#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalActor.h"
#include "Animals/HSAnimalDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Interaction/HSInteractable.h"

namespace
{
UHSAnimalDefinition* CreateValidDeerDefinition(UObject* Outer)
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
	FHSAnimalActorDefaultsTest,
	"HimalayanSettlement.Animals.AnimalActorDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSAnimalActorDefaultsTest::RunTest(const FString& Parameters)
{
	const AHSAnimalActor* Animal = GetDefault<AHSAnimalActor>();

	TestNotNull(TEXT("Animal actor class should have a valid default object."), Animal);
	TestFalse(TEXT("Animal actors should not tick by default."), Animal->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("Live animal without authored data should not be harvestable."), HSInteraction::CanInteract(Animal, nullptr));
	TestEqual(TEXT("Animal should begin idle."), Animal->GetRuntimeState(), EHSAnimalRuntimeState::Idle);
	TestNotNull(TEXT("Animal should have a prototype visual mesh component."), Animal->GetPrototypeMesh());
	TestNotNull(TEXT("Animal prototype visual should have a mesh asset."), Animal->GetPrototypeMesh() && Animal->GetPrototypeMesh()->GetStaticMesh() ? Animal->GetPrototypeMesh()->GetStaticMesh().Get() : nullptr);
	TestEqual(TEXT("Animal prototype visual should not handle collision."), Animal->GetPrototypeMesh() ? Animal->GetPrototypeMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSAnimalActorDamageAndHarvestTest,
	"HimalayanSettlement.Animals.DeerDamageAndHarvestOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSAnimalActorDamageAndHarvestTest::RunTest(const FString& Parameters)
{
	AHSAnimalActor* Deer = NewObject<AHSAnimalActor>();
	UHSAnimalDefinition* DeerDefinition = CreateValidDeerDefinition(Deer);
	Deer->SetAnimalDefinition(DeerDefinition);

	TestEqual(TEXT("Configured deer should start at definition max health."), Deer->GetCurrentHealth(), 70.0f);
	TestFalse(TEXT("Live deer should not be harvestable."), HSInteraction::CanInteract(Deer, nullptr));
	TestEqual(TEXT("Negative hunting damage should be ignored."), Deer->ApplyHuntingDamage(-5.0f, nullptr), 0.0f);
	TestEqual(TEXT("Positive hunting damage should reduce health."), Deer->ApplyHuntingDamage(25.0f, nullptr), 25.0f);
	TestEqual(TEXT("Damaged deer should keep remaining health."), Deer->GetCurrentHealth(), 45.0f);
	TestFalse(TEXT("Damaged deer should not be dead yet."), Deer->IsDead());

	TestEqual(TEXT("Lethal damage should only apply remaining health."), Deer->ApplyHuntingDamage(100.0f, nullptr), 45.0f);
	TestTrue(TEXT("Lethal damage should mark deer dead."), Deer->IsDead());
	TestTrue(TEXT("Dead deer should be harvestable."), HSInteraction::CanInteract(Deer, nullptr));
	TestTrue(TEXT("Dead deer prompt should mention harvesting."), HSInteraction::GetInteractionPrompt(Deer, nullptr).ToString().Contains(TEXT("Harvest")));

	TArray<FHSItemStack> Drops;
	TestTrue(TEXT("Dead deer should harvest once."), Deer->TryHarvest(nullptr, Drops));
	TestEqual(TEXT("Deer harvest should produce configured drop stacks."), Drops.Num(), 2);
	TestTrue(TEXT("Deer should record harvested state."), Deer->HasBeenHarvested());
	TestFalse(TEXT("Harvested deer should no longer be interactable."), HSInteraction::CanInteract(Deer, nullptr));

	TArray<FHSItemStack> SecondDrops;
	TestFalse(TEXT("Deer should not harvest twice."), Deer->TryHarvest(nullptr, SecondDrops));
	TestEqual(TEXT("Second harvest should produce no drops."), SecondDrops.Num(), 0);

	return true;
}

#endif
