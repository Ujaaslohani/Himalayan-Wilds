#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSInteractionSensorComponent.generated.h"

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSInteractionSensorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSInteractionSensorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* FindInteractable(FHitResult& OutHit) const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshFocus();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetFocusedInteractionPrompt() const { return FocusedInteractionPrompt; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetInteractionDistance() const { return InteractionDistance; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> GetTraceChannel() const { return TraceChannel; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetFocusRefreshInterval() const { return FocusRefreshInterval; }

private:
	bool ResolveTraceView(FVector& OutStart, FVector& OutDirection) const;
	void ClearFocus();

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.05", Units = "s"))
	float FocusRefreshInterval;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> FocusedInteractable;

	UPROPERTY(Transient)
	FText FocusedInteractionPrompt;

	FTimerHandle FocusRefreshTimerHandle;
};
