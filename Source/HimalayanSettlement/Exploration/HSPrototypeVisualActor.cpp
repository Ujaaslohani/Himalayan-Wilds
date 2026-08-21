#include "Exploration/HSPrototypeVisualActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AHSPrototypeVisualActor::AHSPrototypeVisualActor()
	: VisualShape(EHSPrototypeVisualShape::Cube)
	, VisualColor(FLinearColor(0.45f, 0.45f, 0.42f, 1.0f))
{
	PrimaryActorTick.bCanEverTick = false;

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	RootComponent = PrototypeMesh;
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetGenerateOverlapEvents(false);
	PrototypeMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		CubeMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		CylinderMesh = CylinderMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		ConeMesh = ConeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		SphereMesh = SphereMeshFinder.Object;
	}

	if (CubeMesh)
	{
		PrototypeMesh->SetStaticMesh(CubeMesh);
	}
}

void AHSPrototypeVisualActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyVisual();
}

void AHSPrototypeVisualActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisual();
}

void AHSPrototypeVisualActor::ConfigureVisual(EHSPrototypeVisualShape NewShape, FLinearColor NewColor, FVector NewScale)
{
	VisualShape = NewShape;
	VisualColor = NewColor;
	SetActorScale3D(NewScale);
	ApplyVisual();
}

void AHSPrototypeVisualActor::ApplyVisual()
{
	if (!PrototypeMesh)
	{
		return;
	}

	UStaticMesh* SelectedMesh = CubeMesh;
	switch (VisualShape)
	{
	case EHSPrototypeVisualShape::Cylinder:
		SelectedMesh = CylinderMesh ? CylinderMesh : CubeMesh;
		break;
	case EHSPrototypeVisualShape::Cone:
		SelectedMesh = ConeMesh ? ConeMesh : CubeMesh;
		break;
	case EHSPrototypeVisualShape::Sphere:
		SelectedMesh = SphereMesh ? SphereMesh : CubeMesh;
		break;
	case EHSPrototypeVisualShape::Cube:
	default:
		SelectedMesh = CubeMesh;
		break;
	}

	if (SelectedMesh)
	{
		PrototypeMesh->SetStaticMesh(SelectedMesh);
	}

	if (!HasAnyFlags(RF_ClassDefaultObject) && GetWorld() && GetWorld()->IsGameWorld())
	{
		UMaterialInstanceDynamic* DynamicMaterial = PrototypeMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), VisualColor);
		}
	}
}
