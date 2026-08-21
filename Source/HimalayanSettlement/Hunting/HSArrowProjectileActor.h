#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSArrowProjectileActor.generated.h"

class AHSAnimalActor;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSArrowFeedbackSignature, FText, Message);

UCLASS()
class HIMALAYANSETTLEMENT_API AHSArrowProjectileActor : public AActor
{
	GENERATED_BODY()

public:
	AHSArrowProjectileActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Hunting|Arrow")
	void LaunchArrow(AActor* NewShooter, FVector StartLocation, FVector Direction, float NewDamage, float NewMaxRange, float NewSpeed);

	UFUNCTION(BlueprintPure, Category = "Hunting|Arrow")
	bool IsFlying() const { return bFlying; }

	UFUNCTION(BlueprintPure, Category = "Hunting|Arrow")
	float GetDamage() const { return Damage; }

	UFUNCTION(BlueprintPure, Category = "Hunting|Arrow")
	float GetSpeed() const { return Speed; }

	UPROPERTY(BlueprintAssignable, Category = "Hunting|Arrow")
	FHSArrowFeedbackSignature OnArrowFeedback;

private:
	void HandleTraceHit(const FHitResult& Hit);
	void StopFlightAt(FVector ImpactLocation);

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<USceneComponent> ArrowRoot;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> ArrowShaftMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> ArrowHeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> ArrowFletchingTopMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> ArrowFletchingSideMesh;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Shooter;

	FVector FlightDirection;
	FVector PreviousLocation;
	float Damage;
	float MaxRange;
	float Speed;
	float DistanceTraveled;
	bool bFlying;
};
