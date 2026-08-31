#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"
#include "HSBuildingSchemaActor.generated.h"

class UBoxComponent;
class UHSBuildingDefinition;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSBuildingSchemaActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSBuildingSchemaActor();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Building")
	UHSBuildingDefinition* GetBuildingDefinition() const { return BuildingDefinition; }

	UFUNCTION(BlueprintCallable, Category = "Building")
	void SetBuildingDefinition(UHSBuildingDefinition* NewBuildingDefinition);

	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsSchemaPlaced() const { return bSchemaPlaced; }

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSBuildingDefinition> BuildingDefinition;

	UPROPERTY(VisibleAnywhere, Category = "Building")
	bool bSchemaPlaced;
};
