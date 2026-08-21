#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Building/HSBuildingDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "Exploration/HSExplorationStateComponent.h"
#include "Herbalism/HSPlantDefinition.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Persistence/HSSettlementPersistenceComponent.h"
#include "Persistence/HSSettlementSaveGame.h"
#include "Player/HSPlayerCharacter.h"
#include "Player/HSPlayerHealthComponent.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "Settlement/HSSettlementPopulationComponent.h"
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

UHSPlantDefinition* CreatePersistenceTestPlant(UObject* Outer)
{
	UHSPlantDefinition* PlantDefinition = NewObject<UHSPlantDefinition>(Outer);
	PlantDefinition->DisplayName = FText::FromString(TEXT("Speed Herb"));
	PlantDefinition->PlantTag = TAG_HS_Plant_SpeedHerb;
	PlantDefinition->EffectTag = TAG_HS_Effect_Plant_Speed;
	PlantDefinition->MovementSpeedMultiplier = 1.25f;
	PlantDefinition->DurationDays = 0.5f;
	return PlantDefinition;
}

bool SeedPersistentSettlementState(AHSPlayerCharacter& Player, UHSWorldTimeComponent& WorldTime)
{
	UHSSettlementPlanComponent* SettlementPlan = Player.GetSettlementPlanComponent();
	UHSConstructionComponent* Construction = Player.GetConstructionComponent();
	UHSPlayerProgressComponent* Progress = Player.GetPlayerProgressComponent();
	UHSResourceCarryComponent* Carry = Player.GetResourceCarryComponent();
	UHSPlayerHealthComponent* Health = Player.GetPlayerHealthComponent();
	UHSPlayerEquipmentComponent* Equipment = Player.GetPlayerEquipmentComponent();
	UHSSettlementUnlockComponent* Unlocks = Player.GetSettlementUnlockComponent();
	UHSAnimalIntelComponent* AnimalIntel = Player.GetAnimalIntelComponent();
	UHSPlantEffectComponent* PlantEffect = Player.GetPlantEffectComponent();
	UHSSettlementPopulationComponent* Population = Player.GetSettlementPopulationComponent();
	UHSExplorationStateComponent* Exploration = Player.GetExplorationStateComponent();
	if (!SettlementPlan || !Construction || !Progress || !Carry || !Health || !Equipment || !Unlocks || !AnimalIntel || !PlantEffect || !Population || !Exploration)
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
	Carry->AddStack(FHSItemStack{ TAG_HS_Item_Bow_Basic, 1 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Item_Arrow_Basic, 6 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Item_Spear_Basic, 1 });
	Health->ApplyHealthDamage(35.0f);
	Equipment->SetSpearEquipped(true);
	Player.SetEmergencyLowEndModeEnabled(true);
	Player.CycleGraphicsQualityPreset();
	Player.CycleMouseSensitivityPreset();
	Player.CycleMasterVolumePreset();
	Unlocks->GrantUnlock(TAG_HS_Building_BasicHouse);
	AnimalIntel->RequestDailyAnimalLead();
	PlantEffect->LearnPlant(TAG_HS_Plant_SpeedHerb);
	PlantEffect->ApplyPlantEffect(CreatePersistenceTestPlant(&Player));
	Population->RecruitNPC(TEXT("Hunter_01"), TAG_HS_NPC_Profession_Hunter);
	Population->AssignNPCToBuilding(TEXT("Hunter_01"), TAG_HS_Building_BasicHouse);
	FGameplayTagContainer CompletedBuildings;
	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouse);
	CompletedBuildings.AddTag(TAG_HS_Building_WatchPost);
	Population->RefreshFromCompletedBuildings(CompletedBuildings);
	Population->AdvanceSettlementDay(1.0f);
	Exploration->DiscoverLocation(TEXT("Road_01"), FText::FromString(TEXT("Road")), TAG_HS_Location_Road, FVector(800.0f, -120.0f, 0.0f));

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
	TestEqual(TEXT("Default save should start with full player health."), SaveGame->PlayerHealth, 100.0f);
	TestFalse(TEXT("Default save should not equip a bow."), SaveGame->bBowEquipped);
	TestEqual(TEXT("Default save should start unarmed."), SaveGame->EquippedWeaponMode, EHSPlayerWeaponMode::Unarmed);
	TestFalse(TEXT("Default save should not enable emergency low-end mode."), SaveGame->bEmergencyLowEndModeEnabled);
	TestTrue(TEXT("Default save should keep low-end streaming conservative."), SaveGame->bLowEndStreamingEnabled);
	TestEqual(TEXT("Default save should use low graphics preset."), SaveGame->GraphicsQualityPreset, 0);
	TestEqual(TEXT("Default save should use medium mouse sensitivity."), SaveGame->MouseSensitivityPreset, 1);
	TestEqual(TEXT("Default save should use high master volume."), SaveGame->MasterVolumePreset, 2);

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
	TestEqual(TEXT("Snapshot should store carried item stacks."), Snapshot->CarriedItems.Num(), 4);
	TestEqual(TEXT("Snapshot should store player health."), Snapshot->PlayerHealth, 65.0f);
	TestFalse(TEXT("Snapshot should store legacy bow flag as false for spear mode."), Snapshot->bBowEquipped);
	TestEqual(TEXT("Snapshot should store equipped spear mode."), Snapshot->EquippedWeaponMode, EHSPlayerWeaponMode::Spear);
	TestTrue(TEXT("Snapshot should store emergency low-end mode state."), Snapshot->bEmergencyLowEndModeEnabled);
	TestTrue(TEXT("Snapshot should store low-end streaming state."), Snapshot->bLowEndStreamingEnabled);
	TestEqual(TEXT("Snapshot should store graphics quality preset."), Snapshot->GraphicsQualityPreset, SourcePlayer->GetGraphicsQualityPreset());
	TestEqual(TEXT("Snapshot should store mouse sensitivity preset."), Snapshot->MouseSensitivityPreset, SourcePlayer->GetMouseSensitivityPreset());
	TestEqual(TEXT("Snapshot should store master volume preset."), Snapshot->MasterVolumePreset, SourcePlayer->GetMasterVolumePreset());
	TestTrue(TEXT("Snapshot should store active animal lead."), Snapshot->ActiveAnimalLead.IsValid());
	TestTrue(TEXT("Snapshot should store known speed herb."), Snapshot->KnownPlants.HasTagExact(TAG_HS_Plant_SpeedHerb));
	TestTrue(TEXT("Snapshot should store active plant effect."), Snapshot->ActivePlantEffect.IsValid());
	TestEqual(TEXT("Snapshot should store NPC assignment state."), Snapshot->NPCSettlementAssignments.Num(), 1);
	TestEqual(TEXT("Snapshot should store house capacity."), Snapshot->HouseCapacity, 2);
	TestEqual(TEXT("Snapshot should store village expansion day."), Snapshot->VillageExpansionDay, 1);
	TestTrue(TEXT("Snapshot should store defense foundation state."), Snapshot->bDefenseSystemEnabled);
	TestEqual(TEXT("Snapshot should store discovered locations."), Snapshot->DiscoveredLocations.Num(), 1);

	TestTrue(TEXT("Snapshot should restore successfully."), RestoredPersistence->RestoreFromSaveGame(Snapshot));

	UHSSettlementPlanComponent* RestoredPlan = RestoredPlayer->GetSettlementPlanComponent();
	UHSConstructionComponent* RestoredConstruction = RestoredPlayer->GetConstructionComponent();
	UHSPlayerProgressComponent* RestoredProgress = RestoredPlayer->GetPlayerProgressComponent();
	UHSResourceCarryComponent* RestoredCarry = RestoredPlayer->GetResourceCarryComponent();
	UHSPlayerHealthComponent* RestoredHealth = RestoredPlayer->GetPlayerHealthComponent();
	UHSPlayerEquipmentComponent* RestoredEquipment = RestoredPlayer->GetPlayerEquipmentComponent();
	UHSSettlementUnlockComponent* RestoredUnlocks = RestoredPlayer->GetSettlementUnlockComponent();
	UHSAnimalIntelComponent* RestoredAnimalIntel = RestoredPlayer->GetAnimalIntelComponent();
	UHSPlantEffectComponent* RestoredPlantEffect = RestoredPlayer->GetPlantEffectComponent();
	UHSSettlementPopulationComponent* RestoredPopulation = RestoredPlayer->GetSettlementPopulationComponent();
	UHSExplorationStateComponent* RestoredExploration = RestoredPlayer->GetExplorationStateComponent();

	TestTrue(TEXT("Restored plan should keep settlement claim."), RestoredPlan && RestoredPlan->HasSettlementClaim());
	TestTrue(TEXT("Restored plan should keep pending drying rack schema."), RestoredPlan && RestoredPlan->HasPendingSchema(TAG_HS_Building_BasicDryingRack));
	TestTrue(TEXT("Restored construction should keep active house job."), RestoredConstruction && RestoredConstruction->HasActiveJob());
	TestEqual(TEXT("Restored construction should keep half-day progress."), RestoredConstruction ? RestoredConstruction->GetConstructionJobs()[0].ProgressDays : 0.0f, 0.5f);
	TestTrue(TEXT("Restored progress should keep completed milestone."), RestoredProgress && RestoredProgress->HasCompletedMilestone(TAG_HS_Milestone_OneMorningHunt_DayEnded));
	TestEqual(TEXT("Restored carry should keep meat quantity."), RestoredCarry ? RestoredCarry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer) : 0, 3);
	TestEqual(TEXT("Restored carry should keep arrow quantity."), RestoredCarry ? RestoredCarry->GetQuantityForTag(TAG_HS_Item_Arrow_Basic) : 0, 6);
	TestEqual(TEXT("Restored carry should keep spear quantity."), RestoredCarry ? RestoredCarry->GetQuantityForTag(TAG_HS_Item_Spear_Basic) : 0, 1);
	TestEqual(TEXT("Restored health should keep saved current health."), RestoredHealth ? RestoredHealth->GetCurrentHealth() : 0.0f, 65.0f);
	TestTrue(TEXT("Restored equipment should keep spear equipped."), RestoredEquipment && RestoredEquipment->IsSpearEquipped());
	TestTrue(TEXT("Restored player should keep emergency low-end mode."), RestoredPlayer->IsEmergencyLowEndModeEnabled());
	TestEqual(TEXT("Restored player should keep graphics quality preset."), RestoredPlayer->GetGraphicsQualityPreset(), SourcePlayer->GetGraphicsQualityPreset());
	TestEqual(TEXT("Restored player should keep mouse sensitivity preset."), RestoredPlayer->GetMouseSensitivityPreset(), SourcePlayer->GetMouseSensitivityPreset());
	TestEqual(TEXT("Restored player should keep master volume preset."), RestoredPlayer->GetMasterVolumePreset(), SourcePlayer->GetMasterVolumePreset());
	TestTrue(TEXT("Restored unlocks should keep house unlock."), RestoredUnlocks && RestoredUnlocks->HasUnlock(TAG_HS_Building_BasicHouse));
	TestTrue(TEXT("Restored animal intel should keep active deer lead."), RestoredAnimalIntel && RestoredAnimalIntel->HasActiveLead() && RestoredAnimalIntel->GetActiveLead().AnimalTag == TAG_HS_Animal_Deer);
	TestTrue(TEXT("Restored plant knowledge should keep speed herb."), RestoredPlantEffect && RestoredPlantEffect->HasPlantKnowledge(TAG_HS_Plant_SpeedHerb));
	TestTrue(TEXT("Restored plant effect should keep active speed effect."), RestoredPlantEffect && RestoredPlantEffect->HasActiveEffect() && RestoredPlantEffect->GetActiveEffect().EffectTag == TAG_HS_Effect_Plant_Speed);
	TestEqual(TEXT("Restored population should keep one assignment."), RestoredPopulation ? RestoredPopulation->GetPopulation() : 0, 1);
	TestEqual(TEXT("Restored population should keep house capacity."), RestoredPopulation ? RestoredPopulation->GetHouseCapacity() : 0, 2);
	TestEqual(TEXT("Restored population should keep expansion day."), RestoredPopulation ? RestoredPopulation->GetVillageExpansionDay() : 0, 1);
	TestTrue(TEXT("Restored population should keep defense state."), RestoredPopulation && RestoredPopulation->IsDefenseSystemEnabled());
	TestEqual(TEXT("Restored exploration should keep discovered road."), RestoredExploration ? RestoredExploration->CountDiscoveredByTag(TAG_HS_Location_Road) : 0, 1);
	TestTrue(TEXT("Restored exploration should keep low-end streaming enabled."), RestoredExploration && RestoredExploration->IsLowEndStreamingEnabled());

	WorldTime->AdvanceHalfDay();
	TestTrue(TEXT("Restored active construction should complete after another half-day."), RestoredConstruction && RestoredConstruction->HasCompletedBuilding(TAG_HS_Building_BasicHouse));
	TestFalse(TEXT("Restored active plant effect should expire with world time."), RestoredPlantEffect && RestoredPlantEffect->HasActiveEffect());

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPersistenceManualAndAutosaveSlotsAreSeparateTest,
	"HimalayanSettlement.Persistence.ManualAndAutosaveSlotsAreSeparate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPersistenceManualAndAutosaveSlotsAreSeparateTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();
	const UHSSettlementPersistenceComponent* Persistence = Player ? Player->GetSettlementPersistenceComponent() : nullptr;

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own persistence component."), Persistence);
	if (!Persistence)
	{
		return false;
	}

	TestFalse(TEXT("Manual save slot should not be empty."), Persistence->GetDefaultSlotName().IsEmpty());
	TestFalse(TEXT("Autosave slot should not be empty."), Persistence->GetAutosaveSlotName().IsEmpty());
	TestNotEqual(TEXT("Autosave should not overwrite the manual save slot."), Persistence->GetAutosaveSlotName(), Persistence->GetDefaultSlotName());
	TestFalse(TEXT("Selected manual save slot should not be empty."), Player->GetSelectedManualSaveSlotName().IsEmpty());
	TestEqual(TEXT("First selected manual save slot should use default slot."), Player->GetSelectedManualSaveSlotName(), Persistence->GetDefaultSlotName());

	return true;
}

#endif
