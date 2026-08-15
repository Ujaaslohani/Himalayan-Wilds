#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSQuestHandoffComponent.generated.h"

class UHSQuestDefinition;

UENUM(BlueprintType)
enum class EHSQuestHandoffState : uint8
{
	Unavailable,
	Available,
	Accepted
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
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TryOfferQuest(AActor* Interactor);

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSQuestDefinition> QuestDefinition;

	UPROPERTY(VisibleAnywhere, Category = "Quest")
	bool bAccepted;
};
