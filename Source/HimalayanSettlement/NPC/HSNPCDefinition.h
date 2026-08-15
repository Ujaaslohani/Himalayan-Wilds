#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "GameplayTagContainer.h"
#include "HSNPCDefinition.generated.h"

UCLASS(BlueprintType)
class HIMALAYANSETTLEMENT_API UHSNPCDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	FGameplayTag ProfessionTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Knowledge")
	FGameplayTagContainer KnowledgeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Unlocks")
	FGameplayTagContainer UnlockTags;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsDefinitionValid(FText* OutReason = nullptr) const;
};
