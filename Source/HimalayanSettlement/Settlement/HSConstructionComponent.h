#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSConstructionComponent.generated.h"

class UHSSettlementPlanComponent;
enum class EHSTimePhase : uint8;

UENUM(BlueprintType)
enum class EHSConstructionJobState : uint8
{
	Active,
	Complete
};

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSConstructionJob
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction")
	FGameplayTag BuildingTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction")
	float RequiredDays = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction")
	float ProgressDays = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction")
	EHSConstructionJobState State = EHSConstructionJobState::Active;

	bool IsValid() const
	{
		return BuildingTag.IsValid() && RequiredDays > 0.0f;
	}

	bool IsComplete() const
	{
		return State == EHSConstructionJobState::Complete;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSConstructionJobSignature, const FHSConstructionJob&, ConstructionJob);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSConstructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSConstructionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "Construction")
	FHSConstructionJobSignature OnConstructionJobStarted;

	UPROPERTY(BlueprintAssignable, Category = "Construction")
	FHSConstructionJobSignature OnConstructionJobCompleted;

	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool StartNextPendingSchema(UHSSettlementPlanComponent* SettlementPlan);

	UFUNCTION(BlueprintCallable, Category = "Construction")
	bool AdvanceConstruction(float DeltaDays);

	UFUNCTION(BlueprintCallable, Category = "Construction")
	void BindToWorldTime();

	UFUNCTION(BlueprintPure, Category = "Construction")
	bool HasActiveJob() const;

	UFUNCTION(BlueprintPure, Category = "Construction")
	bool HasCompletedBuilding(FGameplayTag BuildingTag) const;

	UFUNCTION(BlueprintPure, Category = "Construction")
	float GetHouseConstructionDays() const { return HouseConstructionDays; }

	UFUNCTION(BlueprintPure, Category = "Construction")
	const TArray<FHSConstructionJob>& GetConstructionJobs() const { return ConstructionJobs; }

private:
	UFUNCTION()
	void HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays);

	float ResolveRequiredDays(FGameplayTag BuildingTag) const;
	FHSConstructionJob* FindActiveJob();
	const FHSConstructionJob* FindActiveJob() const;

	UPROPERTY(EditDefaultsOnly, Category = "Construction", meta = (ClampMin = "0.1", Units = "days"))
	float HouseConstructionDays;

	UPROPERTY(VisibleAnywhere, Category = "Construction")
	TArray<FHSConstructionJob> ConstructionJobs;

	UPROPERTY(VisibleAnywhere, Category = "Construction")
	FGameplayTagContainer CompletedBuildings;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSWorldTimeComponent> BoundWorldTime;

	float LastObservedElapsedDays;
};
