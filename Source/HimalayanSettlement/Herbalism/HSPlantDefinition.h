#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "HSPlantDefinition.generated.h"

UCLASS(BlueprintType)
class HIMALAYANSETTLEMENT_API UHSPlantDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UHSPlantDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Plant")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Plant")
	FGameplayTag PlantTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Plant|Effect")
	FGameplayTag EffectTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Plant|Effect", meta = (ClampMin = "1.0"))
	float MovementSpeedMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Plant|Effect", meta = (ClampMin = "0.1", Units = "days"))
	float DurationDays;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsDefinitionValid(FText* OutReason = nullptr) const;
};
