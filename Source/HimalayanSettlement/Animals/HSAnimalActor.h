#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/HSInteractable.h"
#include "Resources/HSItemStack.h"
#include "HSAnimalActor.generated.h"

class UBoxComponent;
class UHSAnimalDefinition;

UENUM(BlueprintType)
enum class EHSAnimalRuntimeState : uint8
{
	Idle,
	Dead
};

UCLASS()
class HIMALAYANSETTLEMENT_API AHSAnimalActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSAnimalActor();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Animal")
	const UHSAnimalDefinition* GetAnimalDefinition() const { return AnimalDefinition; }

	UFUNCTION(BlueprintCallable, Category = "Animal")
	void SetAnimalDefinition(UHSAnimalDefinition* NewAnimalDefinition);

	UFUNCTION(BlueprintPure, Category = "Animal")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Animal")
	FGameplayTag GetSpeciesTag() const;

	UFUNCTION(BlueprintPure, Category = "Animal|Vitals")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Animal|State")
	EHSAnimalRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category = "Animal|State")
	bool IsDead() const { return RuntimeState == EHSAnimalRuntimeState::Dead; }

	UFUNCTION(BlueprintPure, Category = "Animal|Harvest")
	bool HasBeenHarvested() const { return bHarvested; }

	UFUNCTION(BlueprintPure, Category = "Animal|Harvest")
	const TArray<FHSItemStack>& GetHarvestedDrops() const { return HarvestedDrops; }

	UFUNCTION(BlueprintCallable, Category = "Animal|Hunting")
	float ApplyHuntingDamage(float DamageAmount, AActor* DamageInstigator);

	UFUNCTION(BlueprintCallable, Category = "Animal|Harvest")
	bool TryHarvest(AActor* Harvester, TArray<FHSItemStack>& OutDrops);

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
	void ResetRuntimeStateFromDefinition();
	float GetMaxHealth() const;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSAnimalDefinition> AnimalDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animal", meta = (AllowPrivateAccess = "true"))
	FText FallbackDisplayName;

	UPROPERTY(VisibleAnywhere, Category = "Animal|Vitals")
	float CurrentHealth;

	UPROPERTY(VisibleAnywhere, Category = "Animal|State")
	EHSAnimalRuntimeState RuntimeState;

	UPROPERTY(VisibleAnywhere, Category = "Animal|Harvest")
	bool bHarvested;

	UPROPERTY(VisibleAnywhere, Category = "Animal|Harvest")
	TArray<FHSItemStack> HarvestedDrops;
};
