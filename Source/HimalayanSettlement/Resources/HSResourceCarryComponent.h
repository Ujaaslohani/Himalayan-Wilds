#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Resources/HSItemStack.h"
#include "HSResourceCarryComponent.generated.h"

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSResourceCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSResourceCarryComponent();

	UFUNCTION(BlueprintCallable, Category = "Resources")
	void AddStack(const FHSItemStack& Stack);

	UFUNCTION(BlueprintCallable, Category = "Resources")
	void AddStacks(const TArray<FHSItemStack>& Stacks);

	UFUNCTION(BlueprintPure, Category = "Resources")
	int32 GetQuantityForTag(FGameplayTag ItemTag) const;

	UFUNCTION(BlueprintPure, Category = "Resources")
	bool HasAtLeast(const FHSItemStack& Stack) const;

	UFUNCTION(BlueprintPure, Category = "Resources")
	const TArray<FHSItemStack>& GetCarriedItems() const { return CarriedItems; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Resources")
	TArray<FHSItemStack> CarriedItems;
};
