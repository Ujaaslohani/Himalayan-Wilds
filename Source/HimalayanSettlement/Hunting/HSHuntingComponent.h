#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSHuntingComponent.generated.h"

class AHSAnimalActor;

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSHuntingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSHuntingComponent();

	UFUNCTION(BlueprintCallable, Category = "Hunting")
	bool TryHunt();

	UFUNCTION(BlueprintCallable, Category = "Hunting")
	AHSAnimalActor* FindHuntTarget(FHitResult& OutHit) const;

	UFUNCTION(BlueprintCallable, Category = "Hunting")
	bool ApplyDamageToTarget(AHSAnimalActor* Target);

	UFUNCTION(BlueprintPure, Category = "Hunting")
	float GetHuntDistance() const { return HuntDistance; }

	UFUNCTION(BlueprintPure, Category = "Hunting")
	float GetHuntDamage() const { return HuntDamage; }

	UFUNCTION(BlueprintPure, Category = "Hunting")
	TEnumAsByte<ECollisionChannel> GetTraceChannel() const { return TraceChannel; }

private:
	bool ResolveTraceView(FVector& OutStart, FVector& OutDirection) const;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting", meta = (ClampMin = "50.0", Units = "cm"))
	float HuntDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting", meta = (ClampMin = "0.0"))
	float HuntDamage;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting")
	TEnumAsByte<ECollisionChannel> TraceChannel;
};
