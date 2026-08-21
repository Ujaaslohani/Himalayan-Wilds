#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalActor.h"
#include "Animals/HSAnimalDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "Hunting/HSArrowProjectileActor.h"
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
	TestNotNull(TEXT("Hunting component should spawn native arrow projectiles."), Hunting->GetArrowProjectileClass().Get());
	TestEqual(TEXT("Prototype arrow speed should stay readable."), Hunting->GetArrowSpeed(), 3200.0f);

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
	TestNotNull(TEXT("Player should own the native equipment component."), Player ? Player->GetPlayerEquipmentComponent() : nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSArrowProjectileDamagesAnimalOnImpactTest,
	"HimalayanSettlement.Hunting.ArrowProjectileDamagesAnimalOnImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSArrowProjectileDamagesAnimalOnImpactTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSAnimalActor* Deer = World->SpawnActor<AHSAnimalActor>(FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	AHSArrowProjectileActor* Arrow = World->SpawnActor<AHSArrowProjectileActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);

	TestNotNull(TEXT("Deer should spawn."), Deer);
	TestNotNull(TEXT("Arrow should spawn."), Arrow);
	if (!Deer || !Arrow)
	{
		World->DestroyWorld(false);
		return false;
	}

	Deer->SetAnimalDefinition(CreateHuntingTestDeer(Deer));
	Arrow->LaunchArrow(nullptr, FVector::ZeroVector, FVector::ForwardVector, 75.0f, 1200.0f, 3200.0f);
	TestTrue(TEXT("Arrow should fly after launch."), Arrow->IsFlying());

	Arrow->Tick(0.1f);
	TestTrue(TEXT("Arrow impact should kill the deer."), Deer->IsDead());
	TestFalse(TEXT("Arrow should stop after impact."), Arrow->IsFlying());

	World->DestroyWorld(false);
	return true;
}

#endif
