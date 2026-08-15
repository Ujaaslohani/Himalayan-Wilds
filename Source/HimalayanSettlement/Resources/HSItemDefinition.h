#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "HSItemDefinition.generated.h"

UCLASS(BlueprintType)
class HIMALAYANSETTLEMENT_API UHSItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FGameplayTag ItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer Categories;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0.0"))
	float Weight = 0.0f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsDefinitionValid(FText* OutReason = nullptr) const;
};
