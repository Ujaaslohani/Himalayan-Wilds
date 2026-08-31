#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSSettlementPopulationComponent.generated.h"

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSNPCSettlementAssignment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	FName NPCId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	FGameplayTag ProfessionTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	FGameplayTag AssignedBuildingTag;

	bool IsValid() const
	{
		return !NPCId.IsNone() && ProfessionTag.IsValid();
	}
};

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSSettlementPopulationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSSettlementPopulationComponent();

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	bool RecruitNPC(FName NPCId, FGameplayTag ProfessionTag);

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	bool AssignNPCToBuilding(FName NPCId, FGameplayTag BuildingTag);

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	void RefreshFromCompletedBuildings(const FGameplayTagContainer& CompletedBuildings);

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	void AdvanceSettlementDay(float DeltaDays);

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	void SetDefenseSystemEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool IsDefenseSystemEnabled() const { return bDefenseSystemEnabled; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool IsRaidSystemEnabled() const { return bRaidSystemEnabled; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	int32 GetPopulation() const { return Assignments.Num(); }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	int32 GetHouseCapacity() const { return HouseCapacity; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	int32 GetVillageExpansionDay() const { return VillageExpansionDay; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	const TArray<FHSNPCSettlementAssignment>& GetAssignments() const { return Assignments; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	FText GetSettlementStatusText() const;

	bool CanSupportBuilding(FGameplayTag BuildingTag, const FGameplayTagContainer& CompletedBuildings, FText* OutReason = nullptr) const;

	void RestoreSettlementPopulation(const TArray<FHSNPCSettlementAssignment>& NewAssignments, int32 NewHouseCapacity, int32 NewVillageExpansionDay, bool bNewDefenseSystemEnabled, bool bNewRaidSystemEnabled);

private:
	FHSNPCSettlementAssignment* FindAssignment(FName NPCId);
	const FHSNPCSettlementAssignment* FindAssignment(FName NPCId) const;
	int32 CalculateHouseCapacity(const FGameplayTagContainer& CompletedBuildings) const;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	TArray<FHSNPCSettlementAssignment> Assignments;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	int32 HouseCapacity;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	int32 VillageExpansionDay;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	float AccumulatedExpansionDays;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	bool bDefenseSystemEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	bool bRaidSystemEnabled;
};
