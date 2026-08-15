#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "GameplayTagContainer.h"
#include "Resources/HSItemStack.h"
#include "HSQuestDefinition.generated.h"

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSQuestTurnInRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	TArray<FHSItemStack> RequiredItems;

	bool IsValidRequirement() const
	{
		for (const FHSItemStack& Stack : RequiredItems)
		{
			if (!Stack.IsValidStack())
			{
				return false;
			}
		}
		return true;
	}
};

UCLASS(BlueprintType)
class HIMALAYANSETTLEMENT_API UHSQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FGameplayTag QuestTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Objectives")
	FHSQuestTurnInRequirement TurnInRequirement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Rewards")
	FGameplayTagContainer RewardKnowledgeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Rewards")
	FGameplayTagContainer RewardUnlockTags;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool IsDefinitionValid(FText* OutReason = nullptr) const;
};
