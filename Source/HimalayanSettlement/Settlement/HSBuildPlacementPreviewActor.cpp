#include "Settlement/HSBuildPlacementPreviewActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AHSBuildPlacementPreviewActor::AHSBuildPlacementPreviewActor()
	: bPlacementValid(false)
{
	PrimaryActorTick.bCanEverTick = false;

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	RootComponent = PreviewMesh;
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 37.5f));
	PreviewMesh->SetRelativeScale3D(FVector(2.2f, 1.6f, 0.75f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PreviewMesh->SetStaticMesh(CubeMesh.Object);
	}

	SetPlacementValid(false);
}

void AHSBuildPlacementPreviewActor::SetPlacementValid(bool bNewPlacementValid)
{
	bPlacementValid = bNewPlacementValid;
	PreviewMesh->SetRenderCustomDepth(bPlacementValid);
	PreviewMesh->SetVisibility(true, true);
}
