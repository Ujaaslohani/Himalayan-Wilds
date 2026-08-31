#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Resources/HSItemStack.h"
#include "HSAnimalDefinition.generated.h"

UCLASS(BlueprintType)
class HIMALAYANSETTLEMENT_API UHSAnimalDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal")
	FGameplayTag SpeciesTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal|Vitals", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal|Movement", meta = (ClampMin = "0.0"))
	float FleeSpeed = 520.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal|Senses", meta = (ClampMin = "0.0"))
	float SightRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animal|Harvest")
	TArray<FHSItemStack> HarvestDrops;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsDefinitionValid(FText* OutReason = nullptr) const;
};
