#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSPrototypeVisualActor.generated.h"

class UStaticMesh;
class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;

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

	UFUNCTION(BlueprintCallable, Category = "Prototype Visual")
	void ConfigureStaticMesh(UStaticMesh* NewStaticMesh, FVector NewScale, FLinearColor NewColor = FLinearColor(0.24f, 0.42f, 0.20f, 1.0f));

	UFUNCTION(BlueprintCallable, Category = "Prototype Collision")
	void ConfigureSimpleCollisionBox(FVector BoxExtent, FVector BoxOffset = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Prototype Collision")
	void DisableSimpleCollision();

	UFUNCTION(BlueprintPure, Category = "Prototype Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "Prototype Collision")
	UBoxComponent* GetSimpleCollisionBox() const { return SimpleCollisionBox; }

	UFUNCTION(BlueprintPure, Category = "Prototype Visual")
	EHSPrototypeVisualShape GetVisualShape() const { return VisualShape; }

private:
	void ApplyVisual();

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(VisibleAnywhere, Category = "Collision")
	TObjectPtr<UBoxComponent> SimpleCollisionBox;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BasicColorMaterial;

	UPROPERTY(EditAnywhere, Category = "Visual")
	TObjectPtr<UStaticMesh> CustomStaticMesh;

	UPROPERTY(EditAnywhere, Category = "Visual")
	EHSPrototypeVisualShape VisualShape;

	UPROPERTY(EditAnywhere, Category = "Visual")
	FLinearColor VisualColor;
};
