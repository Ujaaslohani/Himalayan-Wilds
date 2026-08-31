#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "HSWoundClueActor.generated.h"

class AHSAnimalActor;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSWoundClueActor : public AActor
{
	GENERATED_BODY()

public:
	AHSWoundClueActor();

	UFUNCTION(BlueprintCallable, Category = "Hunting|Tracking")
	void ConfigureWoundClue(AHSAnimalActor* NewSourceAnimal, float NewLifetimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Hunting|Tracking")
	void ConfigureTrackingClue(AHSAnimalActor* NewSourceAnimal, FGameplayTag NewClueTag, float NewLifetimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Hunting|Tracking")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Hunting|Tracking")
	AHSAnimalActor* GetSourceAnimal() const { return SourceAnimal.Get(); }

	UFUNCTION(BlueprintPure, Category = "Hunting|Tracking")
	FGameplayTag GetClueTag() const { return ClueTag; }

private:
	void ApplyClueVisuals();

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHSAnimalActor> SourceAnimal;

	UPROPERTY(VisibleAnywhere, Category = "Hunting|Tracking")
	FGameplayTag ClueTag;
};
