#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"
#include "Resources/HSItemStack.h"
#include "HSResourcePickupActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSResourcePickupActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSResourcePickupActor();

	UFUNCTION(BlueprintCallable, Category = "Resources")
	void SetItemStack(const FHSItemStack& NewItemStack);

	UFUNCTION(BlueprintPure, Category = "Resources")
	const FHSItemStack& GetItemStack() const { return ItemStack; }

	UFUNCTION(BlueprintPure, Category = "Resources")
	bool IsPickedUp() const { return bPickedUp; }

	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	FText GetInteractionPrompt(const AActor* Interactor) const;
	virtual FText GetInteractionPrompt_Implementation(const AActor* Interactor) const override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	bool CanInteract(const AActor* Interactor) const;
	virtual bool CanInteract_Implementation(const AActor* Interactor) const override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Resources")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, Category = "Resources")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(EditAnywhere, Category = "Resources")
	FHSItemStack ItemStack;

	UPROPERTY(VisibleAnywhere, Category = "Resources")
	bool bPickedUp;
};
