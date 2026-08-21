#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Building/HSBuildingDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Persistence/HSSettlementPersistenceComponent.h"
#include "Persistence/HSSettlementSaveGame.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "Settlement/HSSettlementUnlockComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

namespace
{
UHSBuildingDefinition* CreatePersistenceTestBuilding(UObject* Outer, FGameplayTag BuildingTag, const TCHAR* DisplayName)
{
	UHSBuildingDefinition* BuildingDefinition = NewObject<UHSBuildingDefinition>(Outer);
	BuildingDefinition->DisplayName = FText::FromString(DisplayName);
	BuildingDefinition->BuildingTag = BuildingTag;
	return BuildingDefinition;
}

bool SeedPersistentSettlementState(AHSPlayerCharacter& Player, UHSWorldTimeComponent& WorldTime)
{
	UHSSettlementPlanComponent* SettlementPlan = Player.GetSettlementPlanComponent();
	UHSConstructionComponent* Construction = Player.GetConstructionComponent();
	UHSPlayerProgressComponent* Progress = Player.GetPlayerProgressComponent();
	UHSResourceCarryComponent* Carry = Player.GetResourceCarryComponent();
	UHSSettlementUnlockComponent* Unlocks = Player.GetSettlementUnlockComponent();
	if (!SettlementPlan || !Construction || !Progress || !Carry || !Unlocks)
	{
		return false;
	}

	UHSBuildingDefinition* HouseDefinition = CreatePersistenceTestBuilding(&Player, TAG_HS_Building_BasicHouse, TEXT("Basic House"));
	UHSBuildingDefinition* RackDefinition = CreatePersistenceTestBuilding(&Player, TAG_HS_Building_BasicDryingRack, TEXT("Basic Drying Rack"));
	FHSPendingBuildingSchema PendingHouse;
	FHSPendingBuildingSchema PendingRack;

	const bool bSeededSettlement = SettlementPlan->ClaimSettlement(FVector(100.0f, 200.0f, 0.0f))
		&& SettlementPlan->PlaceSchema(HouseDefinition, FVector(240.0f, 200.0f, 0.0f), 15.0f, PendingHouse)
		&& Construction->StartNextPendingSchema(SettlementPlan)
		&& SettlementPlan->PlaceSchema(RackDefinition, FVector(320.0f, 200.0f, 0.0f), 25.0f, PendingRack);

	Construction->BindToWorldTime();
	WorldTime.AdvanceHalfDay();
	Progress->MarkMilestoneCompleted(TAG_HS_Milestone_OneMorningHunt_DayEnded);
	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 3 });
	Unlocks->GrantUnlock(TAG_HS_Building_BasicHouse);

	return bSeededSettlement;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSaveGameDefaultsTest,
	"HimalayanSettlement.Persistence.SaveGameDefaultsAreVersioned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSaveGameDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSSettlementSaveGame* SaveGame = NewObject<UHSSettlementSaveGame>();

	TestNotNull(TEXT("Settlement save game should be constructible."), SaveGame);
	TestEqual(TEXT("Save game should use current version."), SaveGame->SaveVersion, UHSSettlementSaveGame::CurrentSaveVersion);
	TestTrue(TEXT("Default save version should be supported."), SaveGame->IsSupportedVersion());
	TestEqual(TEXT("Default save should start in day phase."), SaveGame->TimePhase, EHSTimePhase::Day);
	TestEqual(TEXT("Default save should start with zero elapsed half-days."), SaveGame->ElapsedHalfDays, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPersistenceSnapshotRestoresSettlementStateTest,
	"HimalayanSettlement.Persistence.SnapshotRestoresSettlementAndConstructionState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPersistenceSnapshotRestoresSettlementStateTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSGameState* GameState = World->SpawnActor<AHSGameState>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	World->SetGameState(GameState);
	AHSPlayerCharacter* SourcePlayer = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	AHSPlayerCharacter* RestoredPlayer = World->SpawnActor<AHSPlayerCharacter>(FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);

	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	UHSSettlementPersistenceComponent* SourcePersistence = SourcePlayer ? SourcePlayer->GetSettlementPersistenceComponent() : nullptr;
	UHSSettlementPersistenceComponent* RestoredPersistence = RestoredPlayer ? RestoredPlayer->GetSettlementPersistenceComponent() : nullptr;

	TestNotNull(TEXT("Game state should spawn."), GameState);
	TestNotNull(TEXT("World time should exist."), WorldTime);
	TestNotNull(TEXT("Source player should spawn."), SourcePlayer);
	TestNotNull(TEXT("Restored player should spawn."), RestoredPlayer);
	TestNotNull(TEXT("Source player should own persistence component."), SourcePersistence);
	TestNotNull(TEXT("Restored player should own persistence component."), RestoredPersistence);
	if (!WorldTime || !SourcePlayer || !RestoredPlayer || !SourcePersistence || !RestoredPersistence)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("Source state should be seeded."), SeedPersistentSettlementState(*SourcePlayer, *WorldTime));

	UHSSettlementSaveGame* Snapshot = SourcePersistence->CreateSnapshot();
	TestNotNull(TEXT("Snapshot should be created."), Snapshot);
	if (!Snapshot)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("Snapshot should store elapsed half-days."), Snapshot->ElapsedHalfDays, 1);
	TestTrue(TEXT("Snapshot should store settlement claim."), Snapshot->bHasSettlementClaim);
	TestEqual(TEXT("Snapshot should store one pending schema."), Snapshot->PendingSchemas.Num(), 1);
	TestEqual(TEXT("Snapshot should store one construction job."), Snapshot->ConstructionJobs.Num(), 1);
	TestEqual(TEXT("Snapshot should store carried meat."), Snapshot->CarriedItems.Num(), 1);

	TestTrue(TEXT("Snapshot should restore successfully."), RestoredPersistence->RestoreFromSaveGame(Snapshot));

	UHSSettlementPlanComponent* RestoredPlan = RestoredPlayer->GetSettlementPlanComponent();
	UHSConstructionComponent* RestoredConstruction = RestoredPlayer->GetConstructionComponent();
	UHSPlayerProgressComponent* RestoredProgress = RestoredPlayer->GetPlayerProgressComponent();
	UHSResourceCarryComponent* RestoredCarry = RestoredPlayer->GetResourceCarryComponent();
	UHSSettlementUnlockComponent* RestoredUnlocks = RestoredPlayer->GetSettlementUnlockComponent();

	TestTrue(TEXT("Restored plan should keep settlement claim."), RestoredPlan && RestoredPlan->HasSettlementClaim());
	TestTrue(TEXT("Restored plan should keep pending drying rack schema."), RestoredPlan && RestoredPlan->HasPendingSchema(TAG_HS_Building_BasicDryingRack));
	TestTrue(TEXT("Restored construction should keep active house job."), RestoredConstruction && RestoredConstruction->HasActiveJob());
	TestEqual(TEXT("Restored construction should keep half-day progress."), RestoredConstruction ? RestoredConstruction->GetConstructionJobs()[0].ProgressDays : 0.0f, 0.5f);
	TestTrue(TEXT("Restored progress should keep completed milestone."), RestoredProgress && RestoredProgress->HasCompletedMilestone(TAG_HS_Milestone_OneMorningHunt_DayEnded));
	TestEqual(TEXT("Restored carry should keep meat quantity."), RestoredCarry ? RestoredCarry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer) : 0, 3);
	TestTrue(TEXT("Restored unlocks should keep house unlock."), RestoredUnlocks && RestoredUnlocks->HasUnlock(TAG_HS_Building_BasicHouse));

	WorldTime->AdvanceHalfDay();
	TestTrue(TEXT("Restored active construction should complete after another half-day."), RestoredConstruction && RestoredConstruction->HasCompletedBuilding(TAG_HS_Building_BasicHouse));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPersistenceRejectsInvalidSaveTest,
	"HimalayanSettlement.Persistence.RejectsInvalidSaveData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPersistenceRejectsInvalidSaveTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	UHSSettlementPersistenceComponent* Persistence = Player->GetSettlementPersistenceComponent();
	UHSSettlementSaveGame* InvalidSave = NewObject<UHSSettlementSaveGame>();
	InvalidSave->SaveVersion = -1;
	InvalidSave->bHasSettlementClaim = true;

	TestNotNull(TEXT("Player should own persistence component."), Persistence);
	TestFalse(TEXT("Null save data should be rejected."), Persistence && Persistence->RestoreFromSaveGame(nullptr));
	TestFalse(TEXT("Unsupported save data should be rejected."), Persistence && Persistence->RestoreFromSaveGame(InvalidSave));
	TestFalse(TEXT("Rejected save should not apply settlement claim."), Player->GetSettlementPlanComponent()->HasSettlementClaim());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasPersistenceComponentTest,
	"HimalayanSettlement.Persistence.PlayerHasNativePersistenceComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasPersistenceComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own native persistence component."), Player ? Player->GetSettlementPersistenceComponent() : nullptr);

	return true;
}

#endif
