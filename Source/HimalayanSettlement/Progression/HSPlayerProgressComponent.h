#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSPlayerProgressComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSMilestoneCompletedSignature, FGameplayTag, MilestoneTag);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSPlayerProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSPlayerProgressComponent();

	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FHSMilestoneCompletedSignature OnMilestoneCompleted;

	UFUNCTION(BlueprintCallable, Category = "Progression")
	bool MarkMilestoneCompleted(FGameplayTag MilestoneTag);

	UFUNCTION(BlueprintPure, Category = "Progression")
	bool HasCompletedMilestone(FGameplayTag MilestoneTag) const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	const FGameplayTagContainer& GetCompletedMilestones() const { return CompletedMilestones; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Progression")
	FGameplayTagContainer CompletedMilestones;
};
