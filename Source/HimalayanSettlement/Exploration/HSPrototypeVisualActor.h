#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSPrototypeVisualActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EHSPrototypeVisualShape : uint8
{
	Cube,
	Cylinder,
	Cone,
	Sphere
};

UCLASS()
class HIMALAYANSETTLEMENT_API AHSPrototypeVisualActor : public AActor
{
	GENERATED_BODY()

public:
	AHSPrototypeVisualActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Prototype Visual")
	void ConfigureVisual(EHSPrototypeVisualShape NewShape, FLinearColor NewColor, FVector NewScale);

	UFUNCTION(BlueprintPure, Category = "Prototype Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Prototype Visual")
	EHSPrototypeVisualShape GetVisualShape() const { return VisualShape; }

private:
	void ApplyVisual();

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(EditAnywhere, Category = "Visual")
	EHSPrototypeVisualShape VisualShape;

	UPROPERTY(EditAnywhere, Category = "Visual")
	FLinearColor VisualColor;
};
