#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "GameplayTagContainer.h"
#include "Resources/HSItemStack.h"
#include "HSBuildingDefinition.generated.h"

UCLASS(BlueprintType)
class HIMALAYANSETTLEMENT_API UHSBuildingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	FGameplayTag BuildingTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Requirements")
	FGameplayTagContainer RequiredKnowledgeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Cost")
	TArray<FHSItemStack> ResourceCosts;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsDefinitionValid(FText* OutReason = nullptr) const;
};
