#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/HSInteractable.h"
#include "HSRestPointActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSRestPointActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSRestPointActor();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Rest")
	FGameplayTag GetRequiredMilestoneTag() const { return RequiredMilestoneTag; }

	UFUNCTION(BlueprintPure, Category = "Rest")
	FGameplayTag GetCompletionMilestoneTag() const { return CompletionMilestoneTag; }

	UFUNCTION(BlueprintPure, Category = "Rest")
	bool HasRested(const AActor* Interactor) const;

	UFUNCTION(BlueprintPure, Category = "Rest")
	bool IsReadyToRest(const AActor* Interactor) const;

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
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rest", meta = (AllowPrivateAccess = "true"))
	FGameplayTag RequiredMilestoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rest", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CompletionMilestoneTag;
};
