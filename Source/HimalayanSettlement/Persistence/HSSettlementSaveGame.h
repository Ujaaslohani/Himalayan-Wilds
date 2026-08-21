#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "Exploration/HSExplorationStateComponent.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Resources/HSItemStack.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "Settlement/HSSettlementPopulationComponent.h"
#include "World/HSWorldTimeComponent.h"
#include "HSSettlementSaveGame.generated.h"

UCLASS()
class HIMALAYANSETTLEMENT_API UHSSettlementSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 7;

	UHSSettlementSaveGame();

	UPROPERTY(VisibleAnywhere, Category = "Save")
	int32 SaveVersion;

	UPROPERTY(VisibleAnywhere, Category = "Save|Time")
	EHSTimePhase TimePhase;

	UPROPERTY(VisibleAnywhere, Category = "Save|Time")
	int32 ElapsedHalfDays;

	UPROPERTY(VisibleAnywhere, Category = "Save|Progression")
	FGameplayTagContainer CompletedMilestones;

	UPROPERTY(VisibleAnywhere, Category = "Save|Resources")
	TArray<FHSItemStack> CarriedItems;

	UPROPERTY(VisibleAnywhere, Category = "Save|Player")
	float PlayerHealth;

	UPROPERTY(VisibleAnywhere, Category = "Save|Equipment")
	bool bBowEquipped;

	UPROPERTY(VisibleAnywhere, Category = "Save|Equipment")
	EHSPlayerWeaponMode EquippedWeaponMode;

	UPROPERTY(VisibleAnywhere, Category = "Save|Options")
	bool bEmergencyLowEndModeEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Save|Options")
	bool bLowEndStreamingEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Save|Options")
	int32 GraphicsQualityPreset;

	UPROPERTY(VisibleAnywhere, Category = "Save|Options")
	int32 MouseSensitivityPreset;

	UPROPERTY(VisibleAnywhere, Category = "Save|Options")
	int32 MasterVolumePreset;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	FGameplayTagContainer GrantedUnlocks;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	bool bHasSettlementClaim;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	FVector SettlementClaimCenter;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	TArray<FHSPendingBuildingSchema> PendingSchemas;

	UPROPERTY(VisibleAnywhere, Category = "Save|Construction")
	TArray<FHSConstructionJob> ConstructionJobs;

	UPROPERTY(VisibleAnywhere, Category = "Save|Construction")
	FGameplayTagContainer CompletedBuildings;

	UPROPERTY(VisibleAnywhere, Category = "Save|NPC")
	FGameplayTagContainer RecruitedNPCProfessionTags;

	UPROPERTY(VisibleAnywhere, Category = "Save|NPC")
	TArray<FHSNPCSettlementAssignment> NPCSettlementAssignments;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	int32 HouseCapacity;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	int32 VillageExpansionDay;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	bool bDefenseSystemEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Save|Settlement")
	bool bRaidSystemEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Save|Exploration")
	TArray<FHSDiscoveredLocation> DiscoveredLocations;

	UPROPERTY(VisibleAnywhere, Category = "Save|Hunter")
	FHSAnimalLead ActiveAnimalLead;

	UPROPERTY(VisibleAnywhere, Category = "Save|Hunter")
	int32 LastAnimalLeadReportDay;

	UPROPERTY(VisibleAnywhere, Category = "Save|Herbalism")
	FGameplayTagContainer KnownPlants;

	UPROPERTY(VisibleAnywhere, Category = "Save|Herbalism")
	FHSPlantEffectState ActivePlantEffect;

	bool IsSupportedVersion() const;
};
