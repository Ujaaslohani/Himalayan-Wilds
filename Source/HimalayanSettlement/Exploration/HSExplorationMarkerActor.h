#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "HSExplorationMarkerActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSExplorationMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	AHSExplorationMarkerActor();

	UFUNCTION(BlueprintPure, Category = "Exploration")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	FGameplayTag GetMarkerTag() const { return MarkerTag; }

	UFUNCTION(BlueprintCallable, Category = "Exploration")
	void ConfigureMarker(FText NewDisplayName, FGameplayTag NewMarkerTag);

private:
	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exploration", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exploration", meta = (AllowPrivateAccess = "true"))
	FGameplayTag MarkerTag;
};
