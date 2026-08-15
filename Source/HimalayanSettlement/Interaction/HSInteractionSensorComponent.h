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

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* FindInteractable(FHitResult& OutHit) const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetInteractionDistance() const { return InteractionDistance; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> GetTraceChannel() const { return TraceChannel; }

private:
	bool ResolveTraceView(FVector& OutStart, FVector& OutDirection) const;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel;
};
