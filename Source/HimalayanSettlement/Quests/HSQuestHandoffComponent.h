#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSQuestHandoffComponent.generated.h"

class UHSQuestDefinition;

UENUM(BlueprintType)
enum class EHSQuestHandoffState : uint8
{
	Unavailable,
	Available,
	Accepted,
	Completed
};

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSQuestHandoffComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSQuestHandoffComponent();

	UFUNCTION(BlueprintPure, Category = "Quest")
	UHSQuestDefinition* GetQuestDefinition() const { return QuestDefinition; }

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void SetQuestDefinition(UHSQuestDefinition* NewQuestDefinition);

	UFUNCTION(BlueprintPure, Category = "Quest")
	EHSQuestHandoffState GetHandoffState() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool HasAcceptedQuest() const { return bAccepted; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool HasCompletedQuest() const { return bCompleted; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	const FGameplayTagContainer& GetGrantedKnowledgeTags() const { return GrantedKnowledgeTags; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	const FGameplayTagContainer& GetGrantedUnlockTags() const { return GrantedUnlockTags; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetInteractionPrompt(const AActor* Interactor) const;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TryOfferQuest(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TryCompleteQuest(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TryAdvanceQuest(AActor* Interactor);

private:
	bool CanInteractorCompleteQuest(const AActor* Interactor) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSQuestDefinition> QuestDefinition;

	UPROPERTY(VisibleAnywhere, Category = "Quest")
	bool bAccepted;

	UPROPERTY(VisibleAnywhere, Category = "Quest")
	bool bCompleted;

	UPROPERTY(VisibleAnywhere, Category = "Quest")
	FGameplayTagContainer GrantedKnowledgeTags;

	UPROPERTY(VisibleAnywhere, Category = "Quest")
	FGameplayTagContainer GrantedUnlockTags;
};
