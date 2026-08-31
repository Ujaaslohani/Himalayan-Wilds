#include "Exploration/HSPrototypeVisualActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AHSPrototypeVisualActor::AHSPrototypeVisualActor()
	: VisualShape(EHSPrototypeVisualShape::Cube)
	, VisualColor(FLinearColor(0.45f, 0.45f, 0.42f, 1.0f))
{
	PrimaryActorTick.bCanEverTick = false;

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	RootComponent = PrototypeMesh;
	PrototypeMesh->SetMobility(EComponentMobility::Movable);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetGenerateOverlapEvents(false);
	PrototypeMesh->SetCanEverAffectNavigation(false);

	SimpleCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SimpleCollisionBox"));
	SimpleCollisionBox->SetupAttachment(PrototypeMesh);
	SimpleCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SimpleCollisionBox->SetGenerateOverlapEvents(false);
	SimpleCollisionBox->SetCanEverAffectNavigation(false);
	SimpleCollisionBox->CanCharacterStepUpOn = ECB_No;
	SimpleCollisionBox->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.0f));

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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicColorMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicColorMaterialFinder.Succeeded())
	{
		BasicColorMaterial = BasicColorMaterialFinder.Object;
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
	CustomStaticMesh = nullptr;
	SetActorScale3D(NewScale);
	ApplyVisual();
}

void AHSPrototypeVisualActor::ConfigureStaticMesh(UStaticMesh* NewStaticMesh, FVector NewScale, FLinearColor NewColor)
{
	CustomStaticMesh = NewStaticMesh;
	VisualColor = NewColor;
	SetActorScale3D(NewScale);
	ApplyVisual();
}

void AHSPrototypeVisualActor::ConfigureSimpleCollisionBox(FVector BoxExtent, FVector BoxOffset)
{
	if (!SimpleCollisionBox)
	{
		return;
	}

	const FVector ActorScale = GetActorScale3D();
	const FVector SafeScale(
		FMath::IsNearlyZero(ActorScale.X) ? 1.0f : ActorScale.X,
		FMath::IsNearlyZero(ActorScale.Y) ? 1.0f : ActorScale.Y,
		FMath::IsNearlyZero(ActorScale.Z) ? 1.0f : ActorScale.Z);
	SimpleCollisionBox->SetRelativeScale3D(FVector(1.0f / SafeScale.X, 1.0f / SafeScale.Y, 1.0f / SafeScale.Z));
	SimpleCollisionBox->SetBoxExtent(BoxExtent);
	SimpleCollisionBox->SetRelativeLocation(FVector(BoxOffset.X / SafeScale.X, BoxOffset.Y / SafeScale.Y, BoxOffset.Z / SafeScale.Z));
	SimpleCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SimpleCollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	SimpleCollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
	SimpleCollisionBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SimpleCollisionBox->SetGenerateOverlapEvents(false);
	SimpleCollisionBox->SetCanEverAffectNavigation(false);
	SimpleCollisionBox->CanCharacterStepUpOn = ECB_No;
	SimpleCollisionBox->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.0f));
}

void AHSPrototypeVisualActor::DisableSimpleCollision()
{
	if (SimpleCollisionBox)
	{
		SimpleCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SimpleCollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
}

void AHSPrototypeVisualActor::ApplyVisual()
{
	if (!PrototypeMesh)
	{
		return;
	}

	UStaticMesh* SelectedMesh = CustomStaticMesh ? CustomStaticMesh.Get() : CubeMesh.Get();
	if (!CustomStaticMesh)
	{
		switch (VisualShape)
		{
		case EHSPrototypeVisualShape::Cylinder:
			SelectedMesh = CylinderMesh ? CylinderMesh.Get() : CubeMesh.Get();
			break;
		case EHSPrototypeVisualShape::Cone:
			SelectedMesh = ConeMesh ? ConeMesh.Get() : CubeMesh.Get();
			break;
		case EHSPrototypeVisualShape::Sphere:
			SelectedMesh = SphereMesh ? SphereMesh.Get() : CubeMesh.Get();
			break;
		case EHSPrototypeVisualShape::Cube:
		default:
			SelectedMesh = CubeMesh.Get();
			break;
		}
	}

	if (SelectedMesh)
	{
		PrototypeMesh->SetStaticMesh(SelectedMesh);
	}

	if (!HasAnyFlags(RF_ClassDefaultObject) && GetWorld() && GetWorld()->IsGameWorld())
	{
		const int32 MaterialSlotCount = SelectedMesh ? FMath::Max(SelectedMesh->GetStaticMaterials().Num(), 1) : 1;
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
		{
			if (BasicColorMaterial)
			{
				PrototypeMesh->SetMaterial(MaterialIndex, BasicColorMaterial);
			}
			UMaterialInstanceDynamic* DynamicMaterial = PrototypeMesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
			if (DynamicMaterial)
			{
				DynamicMaterial->SetVectorParameterValue(TEXT("Color"), VisualColor);
			}
		}
	}
}
