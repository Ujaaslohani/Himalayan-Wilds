#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"
#include "HSTemporaryShelterActor.generated.h"

class UBoxComponent;
class UHSWorldTimeComponent;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSTemporaryShelterActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSTemporaryShelterActor();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Shelter")
	bool IsExpired() const { return bExpired; }

	UFUNCTION(BlueprintPure, Category = "Shelter")
	int32 GetRemainingSleepUses() const { return RemainingSleepUses; }

	UFUNCTION(BlueprintPure, Category = "Shelter")
	bool CanSleep(const AActor* Interactor) const;

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
	UHSWorldTimeComponent* ResolveWorldTime() const;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelter", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxSleepUses;

	UPROPERTY(VisibleAnywhere, Category = "Shelter")
	int32 RemainingSleepUses;

	UPROPERTY(VisibleAnywhere, Category = "Shelter")
	bool bExpired;
};
