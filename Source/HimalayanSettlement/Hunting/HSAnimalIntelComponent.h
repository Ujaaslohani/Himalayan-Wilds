#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSAnimalIntelComponent.generated.h"

class AHSAnimalLeadMarkerActor;
enum class EHSTimePhase : uint8;

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSAnimalLead
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Intel")
	FGameplayTag AnimalTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Intel")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Intel")
	int32 ReportDay = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Intel")
	bool bActive = false;

	bool IsValid() const
	{
		return bActive && AnimalTag.IsValid() && ReportDay >= 0;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSAnimalLeadSignature, const FHSAnimalLead&, AnimalLead);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSAnimalIntelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSAnimalIntelComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "Animal Intel")
	FHSAnimalLeadSignature OnAnimalLeadReported;

	UPROPERTY(BlueprintAssignable, Category = "Animal Intel")
	FHSAnimalLeadSignature OnAnimalLeadExpired;

	UFUNCTION(BlueprintCallable, Category = "Animal Intel")
	bool RequestDailyAnimalLead();

	UFUNCTION(BlueprintCallable, Category = "Animal Intel")
	bool RefreshActiveAnimalLead();

	UFUNCTION(BlueprintCallable, Category = "Animal Intel")
	void ExpireActiveLead();

	UFUNCTION(BlueprintCallable, Category = "Animal Intel")
	void BindToWorldTime();

	UFUNCTION(BlueprintPure, Category = "Animal Intel")
	bool CanRequestDailyAnimalLead() const;

	UFUNCTION(BlueprintPure, Category = "Animal Intel")
	bool HasActiveLead() const { return ActiveLead.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Animal Intel")
	const FHSAnimalLead& GetActiveLead() const { return ActiveLead; }

	UFUNCTION(BlueprintPure, Category = "Animal Intel")
	int32 GetLastReportDay() const { return LastReportDay; }

	void RestoreAnimalIntelState(const FHSAnimalLead& NewActiveLead, int32 NewLastReportDay);

private:
	UFUNCTION()
	void HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays);

	int32 ResolveCurrentDay() const;
	AHSAnimalLeadMarkerActor* SpawnOrUpdateLeadMarker();
	class UHSWorldTimeComponent* ResolveWorldTime() const;

	UPROPERTY(EditDefaultsOnly, Category = "Animal Intel")
	FGameplayTag DefaultAnimalTag;

	UPROPERTY(EditDefaultsOnly, Category = "Animal Intel")
	FVector DefaultLeadLocation;

	UPROPERTY(VisibleAnywhere, Category = "Animal Intel")
	FHSAnimalLead ActiveLead;

	UPROPERTY(VisibleAnywhere, Category = "Animal Intel")
	int32 LastReportDay;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHSAnimalLeadMarkerActor> ActiveLeadMarker;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSWorldTimeComponent> BoundWorldTime;
};
