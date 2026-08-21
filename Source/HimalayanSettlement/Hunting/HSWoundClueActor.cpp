#include "Hunting/HSWoundClueActor.h"

#include "Animals/HSAnimalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

AHSWoundClueActor::AHSWoundClueActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	RootComponent = PrototypeMesh;
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetGenerateOverlapEvents(false);
	PrototypeMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.025f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(SphereMesh.Object);
	}

	SetLifeSpan(45.0f);
}

void AHSWoundClueActor::ConfigureWoundClue(AHSAnimalActor* NewSourceAnimal, float NewLifetimeSeconds)
{
	ConfigureTrackingClue(NewSourceAnimal, TAG_HS_Tracking_Blood, NewLifetimeSeconds);
}

void AHSWoundClueActor::ConfigureTrackingClue(AHSAnimalActor* NewSourceAnimal, FGameplayTag NewClueTag, float NewLifetimeSeconds)
{
	SourceAnimal = NewSourceAnimal;
	ClueTag = NewClueTag.IsValid() ? NewClueTag : TAG_HS_Tracking_Blood;
	ApplyClueVisuals();
	SetLifeSpan(FMath::Max(1.0f, NewLifetimeSeconds));
}

void AHSWoundClueActor::ApplyClueVisuals()
{
	if (!PrototypeMesh)
	{
		return;
	}

	if (ClueTag == TAG_HS_Tracking_Footprint)
	{
		PrototypeMesh->SetRelativeScale3D(FVector(0.18f, 0.32f, 0.018f));
		return;
	}

	if (ClueTag == TAG_HS_Tracking_BrokenGrass)
	{
		PrototypeMesh->SetRelativeScale3D(FVector(0.65f, 0.18f, 0.018f));
		return;
	}

	PrototypeMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.025f));
}
