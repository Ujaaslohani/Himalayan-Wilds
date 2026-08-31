#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSTerrainMeshActor.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSTerrainMeshActor : public AActor
{
	GENERATED_BODY()

public:
	AHSTerrainMeshActor();

	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void ConfigureTerrain(float NewHalfSizeX, float NewHalfSizeY, int32 NewQuadsX, int32 NewQuadsY, float NewHeightScale);

	UFUNCTION(BlueprintPure, Category = "Terrain")
	UProceduralMeshComponent* GetTerrainMesh() const { return TerrainMesh; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	float GetTerrainHeightAtWorldLocation(FVector WorldLocation) const;

private:
	void RebuildTerrain();
	float SampleLocalHeight(float LocalX, float LocalY) const;

	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BasicColorMaterial;

	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "100.0"))
	float HalfSizeX;

	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "100.0"))
	float HalfSizeY;

	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "2"))
	int32 QuadsX;

	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "2"))
	int32 QuadsY;

	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "0.0"))
	float HeightScale;
};
