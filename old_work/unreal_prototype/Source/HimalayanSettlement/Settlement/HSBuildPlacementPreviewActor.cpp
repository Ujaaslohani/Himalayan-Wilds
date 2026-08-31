#include "Settlement/HSBuildPlacementPreviewActor.h"

#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
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

	RoofMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoofMesh"));
	RoofMesh->SetupAttachment(RootComponent);
	RoofMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoofMesh->SetGenerateOverlapEvents(false);
	RoofMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 118.0f));
	RoofMesh->SetRelativeScale3D(FVector(2.45f, 1.85f, 0.2f));

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(RootComponent);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetGenerateOverlapEvents(false);
	MarkerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	MarkerMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.18f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PreviewMesh->SetStaticMesh(CubeMesh.Object);
		RoofMesh->SetStaticMesh(CubeMesh.Object);
		MarkerMesh->SetStaticMesh(CubeMesh.Object);
	}

	ConfigureForBuilding(FGameplayTag(), false);
	SetPlacementValid(false);
}

void AHSBuildPlacementPreviewActor::SetPlacementValid(bool bNewPlacementValid)
{
	bPlacementValid = bNewPlacementValid;
	PreviewMesh->SetRenderCustomDepth(bPlacementValid);
	RoofMesh->SetRenderCustomDepth(bPlacementValid);
	MarkerMesh->SetRenderCustomDepth(bPlacementValid);
	PreviewMesh->SetVisibility(true, true);
}

void AHSBuildPlacementPreviewActor::ConfigureForBuilding(FGameplayTag BuildingTag, bool bTemporaryShelter)
{
	PreviewMesh->SetVisibility(true, true);
	RoofMesh->SetVisibility(false, true);
	MarkerMesh->SetVisibility(false, true);

	if (bTemporaryShelter)
	{
		PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
		PreviewMesh->SetRelativeScale3D(FVector(1.35f, 1.1f, 0.28f));
		RoofMesh->SetVisibility(true, true);
		RoofMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 62.0f));
		RoofMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 18.0f));
		RoofMesh->SetRelativeScale3D(FVector(1.55f, 1.25f, 0.12f));
		return;
	}

	if (BuildingTag == TAG_HS_Building_BasicHouse || BuildingTag == TAG_HS_Building_BasicHouseMinorUpgrade || BuildingTag == TAG_HS_Building_BasicHouseMidUpgrade || BuildingTag == TAG_HS_Building_BasicHouseMajorUpgrade)
	{
		const float UpgradeScale = BuildingTag == TAG_HS_Building_BasicHouseMinorUpgrade ? 1.12f : (BuildingTag == TAG_HS_Building_BasicHouseMidUpgrade ? 1.22f : (BuildingTag == TAG_HS_Building_BasicHouseMajorUpgrade ? 1.35f : 1.0f));
		PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f * UpgradeScale));
		PreviewMesh->SetRelativeScale3D(FVector(2.2f, 1.6f, 0.75f) * UpgradeScale);
		RoofMesh->SetVisibility(true, true);
		RoofMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 122.0f * UpgradeScale));
		RoofMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		RoofMesh->SetRelativeScale3D(FVector(2.55f, 1.9f, 0.22f) * UpgradeScale);
		return;
	}

	if (BuildingTag == TAG_HS_Building_WatchPost)
	{
		PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
		PreviewMesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 2.3f));
		RoofMesh->SetVisibility(true, true);
		RoofMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 245.0f));
		RoofMesh->SetRelativeScale3D(FVector(1.45f, 1.45f, 0.18f));
		return;
	}

	if (BuildingTag == TAG_HS_Building_BasicDryingRack)
	{
		PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f));
		PreviewMesh->SetRelativeScale3D(FVector(1.65f, 0.18f, 0.95f));
		MarkerMesh->SetVisibility(true, true);
		MarkerMesh->SetRelativeLocation(FVector(0.0f, 50.0f, 52.0f));
		MarkerMesh->SetRelativeScale3D(FVector(1.55f, 0.12f, 0.75f));
		return;
	}

	if (BuildingTag == TAG_HS_Building_StorageHut || BuildingTag == TAG_HS_Building_Workshop || BuildingTag == TAG_HS_Building_HerbalHut)
	{
		const FVector BodyScale = BuildingTag == TAG_HS_Building_Workshop ? FVector(2.65f, 1.75f, 0.72f) : FVector(1.65f, 1.35f, 0.62f);
		PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 44.0f));
		PreviewMesh->SetRelativeScale3D(BodyScale);
		RoofMesh->SetVisibility(true, true);
		RoofMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 102.0f));
		RoofMesh->SetRelativeScale3D(FVector(BodyScale.X + 0.25f, BodyScale.Y + 0.25f, 0.18f));
		return;
	}
}
