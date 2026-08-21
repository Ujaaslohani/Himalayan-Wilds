#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "HSCompletedBuildingActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSCompletedBuildingActor : public AActor
{
	GENERATED_BODY()

public:
	AHSCompletedBuildingActor();

	UFUNCTION(BlueprintCallable, Category = "Building")
	void ConfigureCompletedBuilding(FGameplayTag NewBuildingTag);

	UFUNCTION(BlueprintPure, Category = "Building")
	FGameplayTag GetBuildingTag() const { return BuildingTag; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetWallMesh() const { return WallMesh; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetRoofMesh() const { return RoofMesh; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> WallMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> RoofMesh;

	UPROPERTY(VisibleAnywhere, Category = "Building")
	FGameplayTag BuildingTag;
};
