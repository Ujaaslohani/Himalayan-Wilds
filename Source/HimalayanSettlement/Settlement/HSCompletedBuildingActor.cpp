#include "Settlement/HSCompletedBuildingActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AHSCompletedBuildingActor::AHSCompletedBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetupAttachment(SceneRoot);
	WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallMesh->SetGenerateOverlapEvents(false);
	WallMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	WallMesh->SetRelativeScale3D(FVector(2.3f, 1.65f, 1.15f));

	RoofMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoofMesh"));
	RoofMesh->SetupAttachment(SceneRoot);
	RoofMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoofMesh->SetGenerateOverlapEvents(false);
	RoofMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 145.0f));
	RoofMesh->SetRelativeScale3D(FVector(2.55f, 1.9f, 0.22f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		WallMesh->SetStaticMesh(CubeMesh.Object);
		RoofMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AHSCompletedBuildingActor::ConfigureCompletedBuilding(FGameplayTag NewBuildingTag)
{
	BuildingTag = NewBuildingTag;
}
