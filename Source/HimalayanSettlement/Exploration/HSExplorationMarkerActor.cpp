#include "Exploration/HSExplorationMarkerActor.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSExplorationMarkerActor"

AHSExplorationMarkerActor::AHSExplorationMarkerActor()
	: DisplayName(LOCTEXT("DefaultExplorationMarkerName", "Exploration Marker"))
{
	PrimaryActorTick.bCanEverTick = false;

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	RootComponent = PrototypeMesh;
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetGenerateOverlapEvents(false);
	PrototypeMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.4f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(ConeMesh.Object);
	}
}

void AHSExplorationMarkerActor::ConfigureMarker(FText NewDisplayName, FGameplayTag NewMarkerTag)
{
	DisplayName = NewDisplayName;
	MarkerTag = NewMarkerTag;
}

#undef LOCTEXT_NAMESPACE
