#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"
#include "HSInteractionTarget.generated.h"

UCLASS()
class HIMALAYANSETTLEMENT_API AHSInteractionTarget : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSInteractionTarget();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	class UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	int32 GetInteractionCount() const { return InteractionCount; }

	virtual FText GetInteractionPrompt_Implementation(const AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(const AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<class UBoxComponent> InteractionBounds;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	FText InteractionPrompt;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	bool bInteractionEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	int32 InteractionCount;
};
