#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"
#include "HSSettlementClaimActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSSettlementClaimActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSSettlementClaimActor();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool IsClaimed() const { return bClaimed; }

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

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	bool bClaimed;
};
