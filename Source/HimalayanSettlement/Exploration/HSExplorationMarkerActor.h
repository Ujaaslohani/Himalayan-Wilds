#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/HSInteractable.h"
#include "HSExplorationMarkerActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSExplorationMarkerActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSExplorationMarkerActor();

	UFUNCTION(BlueprintPure, Category = "Exploration")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	FGameplayTag GetMarkerTag() const { return MarkerTag; }

	UFUNCTION(BlueprintCallable, Category = "Exploration")
	void ConfigureMarker(FText NewDisplayName, FGameplayTag NewMarkerTag);

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
	void ApplyMarkerVisuals();

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exploration", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exploration", meta = (AllowPrivateAccess = "true"))
	FGameplayTag MarkerTag;

	UPROPERTY(VisibleAnywhere, Category = "Exploration")
	bool bDiscovered;
};
