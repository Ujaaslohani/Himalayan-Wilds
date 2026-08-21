#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Resources/HSItemStack.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSWorldTimeComponent.h"
#include "HSSettlementSaveGame.generated.h"

UCLASS()
class HIMALAYANSETTLEMENT_API UHSSettlementSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 3;

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
