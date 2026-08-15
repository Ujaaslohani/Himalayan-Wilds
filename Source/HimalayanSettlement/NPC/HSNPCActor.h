#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/HSInteractable.h"
#include "HSNPCActor.generated.h"

class UHSNPCDefinition;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSNPCActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSNPCActor();

	UFUNCTION(BlueprintPure, Category = "NPC")
	const UHSNPCDefinition* GetNPCDefinition() const { return NPCDefinition; }

	UFUNCTION(BlueprintPure, Category = "NPC")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "NPC")
	FGameplayTag GetProfessionTag() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	int32 GetInteractionCount() const { return InteractionCount; }

	virtual FText GetInteractionPrompt_Implementation(const AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(const AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSNPCDefinition> NPCDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	FText FallbackDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	FText InteractionPromptFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	bool bCanTalk;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	int32 InteractionCount;
};
