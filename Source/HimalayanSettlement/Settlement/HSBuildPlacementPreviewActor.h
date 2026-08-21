#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSBuildPlacementPreviewActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSBuildPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AHSBuildPlacementPreviewActor();

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	UStaticMeshComponent* GetPreviewMesh() const { return PreviewMesh; }

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	void SetPlacementValid(bool bNewPlacementValid);

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	bool IsPlacementValid() const { return bPlacementValid; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	TObjectPtr<UStaticMeshComponent> PreviewMesh;

	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	bool bPlacementValid;
};
