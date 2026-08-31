#include "Exploration/HSExplorationMarkerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Exploration/HSExplorationStateComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSExplorationMarkerActor"

AHSExplorationMarkerActor::AHSExplorationMarkerActor()
	: DisplayName(LOCTEXT("DefaultExplorationMarkerName", "Exploration Marker"))
	, bDiscovered(false)
{
	PrimaryActorTick.bCanEverTick = false;

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	RootComponent = PrototypeMesh;
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PrototypeMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PrototypeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PrototypeMesh->SetGenerateOverlapEvents(false);
	PrototypeMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.4f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		this->ConeMesh = ConeMeshFinder.Object;
		PrototypeMesh->SetStaticMesh(this->ConeMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		this->CubeMesh = CubeMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		this->CylinderMesh = CylinderMeshFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		this->SphereMesh = SphereMeshFinder.Object;
	}
}

FText AHSExplorationMarkerActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	return bDiscovered
		? FText::Format(LOCTEXT("ExplorationMarkerDiscovered", "Discovered {0}"), DisplayName)
		: FText::Format(LOCTEXT("ExploreMarkerPrompt", "Discover {0}"), DisplayName);
}

bool AHSExplorationMarkerActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return !bDiscovered && MarkerTag.IsValid();
}

void AHSExplorationMarkerActor::Interact_Implementation(AActor* Interactor)
{
	UHSExplorationStateComponent* ExplorationState = Interactor ? Interactor->FindComponentByClass<UHSExplorationStateComponent>() : nullptr;
	if (!ExplorationState)
	{
		return;
	}

	bDiscovered = ExplorationState->DiscoverLocation(GetFName(), DisplayName, MarkerTag, GetActorLocation()) || bDiscovered;
}

void AHSExplorationMarkerActor::ConfigureMarker(FText NewDisplayName, FGameplayTag NewMarkerTag)
{
	DisplayName = NewDisplayName;
	MarkerTag = NewMarkerTag;
	ApplyMarkerVisuals();
}

void AHSExplorationMarkerActor::ApplyMarkerVisuals()
{
	if (!PrototypeMesh)
	{
		return;
	}

	if (MarkerTag == TAG_HS_Location_Trail)
	{
		PrototypeMesh->SetStaticMesh(CubeMesh ? CubeMesh : PrototypeMesh->GetStaticMesh());
		PrototypeMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 1.75f));
		return;
	}

	if (MarkerTag == TAG_HS_Location_Village)
	{
		PrototypeMesh->SetStaticMesh(CylinderMesh ? CylinderMesh : PrototypeMesh->GetStaticMesh());
		PrototypeMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.15f));
		return;
	}

	if (MarkerTag == TAG_HS_Location_AnimalArea)
	{
		PrototypeMesh->SetStaticMesh(SphereMesh ? SphereMesh : PrototypeMesh->GetStaticMesh());
		PrototypeMesh->SetRelativeScale3D(FVector(0.85f, 0.85f, 0.85f));
		return;
	}

	PrototypeMesh->SetStaticMesh(ConeMesh ? ConeMesh : PrototypeMesh->GetStaticMesh());
	PrototypeMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.65f));
}

#undef LOCTEXT_NAMESPACE
