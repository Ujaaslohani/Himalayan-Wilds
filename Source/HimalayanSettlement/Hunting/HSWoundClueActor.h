#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	UFUNCTION(BlueprintPure, Category = "Hunting|Tracking")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Hunting|Tracking")
	AHSAnimalActor* GetSourceAnimal() const { return SourceAnimal.Get(); }

private:
	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHSAnimalActor> SourceAnimal;
};
