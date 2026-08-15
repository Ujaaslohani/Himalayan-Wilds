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

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(const AActor* Interactor) const;
	virtual FText GetInteractionPrompt_Implementation(const AActor* Interactor) const override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(const AActor* Interactor) const;
	virtual bool CanInteract_Implementation(const AActor* Interactor) const override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);
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
