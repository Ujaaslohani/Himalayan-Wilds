#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"
#include "HSPlantActor.generated.h"

class UBoxComponent;
class UHSPlantDefinition;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSPlantActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSPlantActor();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Plant")
	UHSPlantDefinition* GetPlantDefinition() const { return PlantDefinition; }

	UFUNCTION(BlueprintCallable, Category = "Plant")
	void SetPlantDefinition(UHSPlantDefinition* NewPlantDefinition);

	UFUNCTION(BlueprintPure, Category = "Plant")
	bool IsConsumed() const { return bConsumed; }

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plant", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSPlantDefinition> PlantDefinition;

	UPROPERTY(VisibleAnywhere, Category = "Plant")
	bool bConsumed;
};
