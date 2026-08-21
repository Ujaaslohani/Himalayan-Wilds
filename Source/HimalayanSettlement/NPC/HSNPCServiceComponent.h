#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSNPCServiceComponent.generated.h"

class AHSNPCActor;
enum class EHSTimePhase : uint8;

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSNPCServiceOption
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC Service")
	FGameplayTag ServiceTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC Service")
	FGameplayTag ProfessionTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC Service")
	FText Prompt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC Service")
	bool bAvailable = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC Service")
	int32 Priority = 0;

	bool IsValid() const
	{
		return ServiceTag.IsValid() && ProfessionTag.IsValid();
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHSNPCServicesRefreshedSignature);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSNPCServiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSNPCServiceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "NPC Service")
	FHSNPCServicesRefreshedSignature OnServicesRefreshed;

	UFUNCTION(BlueprintPure, Category = "NPC Service")
	bool HasAnyAvailableService(const AActor* Interactor) const;

	UFUNCTION(BlueprintPure, Category = "NPC Service")
	int32 GetSelectedServiceIndex() const { return SelectedServiceIndex; }

	bool GetBestServiceOption(const AActor* Interactor, FHSNPCServiceOption& OutServiceOption) const;
	bool GetSelectedServiceOption(const AActor* Interactor, FHSNPCServiceOption& OutServiceOption) const;
	bool GetServiceOption(FGameplayTag ServiceTag, const AActor* Interactor, FHSNPCServiceOption& OutServiceOption) const;
	void QueryServiceOptions(const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const;
	void QueryAvailableServiceOptions(const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const;
	bool SelectServiceDelta(int32 Delta, const AActor* Interactor);
	bool TryExecuteBestService(AActor* Interactor, FHSNPCServiceOption* OutExecutedService = nullptr);
	bool TryExecuteSelectedService(AActor* Interactor, FHSNPCServiceOption* OutExecutedService = nullptr);
	bool TryExecuteService(FGameplayTag ServiceTag, AActor* Interactor);

	void BindToWorldTime();

private:
	UFUNCTION()
	void HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays);

	void AddBuilderService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const;
	void AddHunterService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const;
	void AddHerbalistService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const;
	void AddSoldierService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const;

	bool ExecuteBuilderService(AActor* Interactor) const;
	bool ExecuteHunterService(AActor* Interactor) const;
	bool ExecuteHerbalistService(AActor* Interactor) const;
	bool ExecuteSoldierService(AActor* Interactor) const;

	class UHSWorldTimeComponent* ResolveWorldTime() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSWorldTimeComponent> BoundWorldTime;

	UPROPERTY(VisibleAnywhere, Category = "NPC Service")
	int32 SelectedServiceIndex;
};
