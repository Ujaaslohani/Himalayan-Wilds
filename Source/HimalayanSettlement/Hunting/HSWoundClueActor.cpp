#include "Hunting/HSWoundClueActor.h"

#include "Animals/HSAnimalActor.h"
#include "Components/StaticMeshComponent.h"
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
	SourceAnimal = NewSourceAnimal;
	SetLifeSpan(FMath::Max(1.0f, NewLifetimeSeconds));
}
