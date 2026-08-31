#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HSItemStack.generated.h"

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Quantity = 0;

	bool IsValidStack() const
	{
		return ItemTag.IsValid() && Quantity > 0;
	}
};
