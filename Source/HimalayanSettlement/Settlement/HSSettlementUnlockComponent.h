#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSSettlementUnlockComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSSettlementUnlockGrantedSignature, FGameplayTag, UnlockTag);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSSettlementUnlockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSSettlementUnlockComponent();

	UPROPERTY(BlueprintAssignable, Category = "Settlement")
	FHSSettlementUnlockGrantedSignature OnSettlementUnlockGranted;

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	bool GrantUnlock(FGameplayTag UnlockTag);

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	int32 GrantUnlocks(const FGameplayTagContainer& UnlockTags);

	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool HasUnlock(FGameplayTag UnlockTag) const;

	UFUNCTION(BlueprintPure, Category = "Settlement")
	const FGameplayTagContainer& GetGrantedUnlocks() const { return GrantedUnlocks; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	FGameplayTagContainer GrantedUnlocks;
};
