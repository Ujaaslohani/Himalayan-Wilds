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

	UFUNCTION(BlueprintCallable, Category = "Resources")
	bool TryConsumeStack(const FHSItemStack& Stack);

	UFUNCTION(BlueprintCallable, Category = "Resources")
	bool TryConsumeStacks(const TArray<FHSItemStack>& Stacks);

	UFUNCTION(BlueprintCallable, Category = "Resources")
	bool TryConsumeStackAtIndex(int32 StackIndex, int32 Quantity, FHSItemStack& OutConsumedStack);

	UFUNCTION(BlueprintCallable, Category = "Resources")
	bool GetStackAtIndex(int32 StackIndex, FHSItemStack& OutStack) const;

	UFUNCTION(BlueprintPure, Category = "Resources")
	int32 GetQuantityForTag(FGameplayTag ItemTag) const;

	UFUNCTION(BlueprintPure, Category = "Resources")
	bool HasAtLeast(const FHSItemStack& Stack) const;

	UFUNCTION(BlueprintPure, Category = "Resources")
	const TArray<FHSItemStack>& GetCarriedItems() const { return CarriedItems; }

	void ReplaceCarriedItems(const TArray<FHSItemStack>& NewCarriedItems);

private:
	UPROPERTY(VisibleAnywhere, Category = "Resources")
	TArray<FHSItemStack> CarriedItems;
};
