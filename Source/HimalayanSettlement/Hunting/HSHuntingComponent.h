#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSHuntingComponent.generated.h"

class AHSAnimalActor;
class AHSArrowProjectileActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSHuntingFeedbackSignature, FText, Message);

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

	UFUNCTION(BlueprintCallable, Category = "Hunting")
	AHSArrowProjectileActor* FireArrow(FVector TraceStart, FVector TraceDirection);

	UFUNCTION(BlueprintPure, Category = "Hunting")
	float GetHuntDistance() const { return HuntDistance; }

	UFUNCTION(BlueprintPure, Category = "Hunting")
	float GetHuntDamage() const { return HuntDamage; }

	UFUNCTION(BlueprintPure, Category = "Hunting")
	TEnumAsByte<ECollisionChannel> GetTraceChannel() const { return TraceChannel; }

	UFUNCTION(BlueprintPure, Category = "Hunting")
	float GetArrowSpeed() const { return ArrowSpeed; }

	UFUNCTION(BlueprintPure, Category = "Hunting")
	TSubclassOf<AHSArrowProjectileActor> GetArrowProjectileClass() const { return ArrowProjectileClass; }

	UPROPERTY(BlueprintAssignable, Category = "Hunting")
	FHSHuntingFeedbackSignature OnHuntingFeedback;

private:
	bool ResolveTraceView(FVector& OutStart, FVector& OutDirection) const;
	AHSAnimalActor* FindHuntTargetFromView(FVector TraceStart, FVector TraceDirection, FHitResult& OutHit) const;

	UFUNCTION()
	void HandleArrowFeedback(FText Message);

	UPROPERTY(EditDefaultsOnly, Category = "Hunting", meta = (ClampMin = "50.0", Units = "cm"))
	float HuntDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting", meta = (ClampMin = "0.0"))
	float HuntDamage;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting")
	TSubclassOf<AHSArrowProjectileActor> ArrowProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting", meta = (ClampMin = "50.0", Units = "cm/s"))
	float ArrowSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Hunting", meta = (ClampMin = "0.0", Units = "cm"))
	float ArrowSpawnForwardOffset;
};
