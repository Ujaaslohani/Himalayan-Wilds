#include "Hunting/HSArrowProjectileActor.h"

#include "Animals/HSAnimalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSArrowProjectileActor"

AHSArrowProjectileActor::AHSArrowProjectileActor()
	: FlightDirection(FVector::ForwardVector)
	, PreviousLocation(FVector::ZeroVector)
	, Damage(0.0f)
	, MaxRange(1200.0f)
	, Speed(3200.0f)
	, DistanceTraveled(0.0f)
	, bFlying(false)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	RootComponent = ArrowMesh;
	ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	ArrowMesh->SetRelativeScale3D(FVector(0.035f, 0.035f, 0.7f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ArrowMesh->SetStaticMesh(CylinderMesh.Object);
	}

	SetLifeSpan(8.0f);
}

void AHSArrowProjectileActor::LaunchArrow(AActor* NewShooter, FVector StartLocation, FVector Direction, float NewDamage, float NewMaxRange, float NewSpeed)
{
	Shooter = NewShooter;
	FlightDirection = Direction.GetSafeNormal();
	if (FlightDirection.IsNearlyZero())
	{
		FlightDirection = FVector::ForwardVector;
	}

	Damage = FMath::Max(0.0f, NewDamage);
	MaxRange = FMath::Max(50.0f, NewMaxRange);
	Speed = FMath::Max(50.0f, NewSpeed);
	DistanceTraveled = 0.0f;
	bFlying = true;

	SetActorLocation(StartLocation);
	SetActorRotation(FlightDirection.Rotation());
	PreviousLocation = StartLocation;
	SetActorTickEnabled(true);

	UE_LOG(LogHSAI, Verbose, TEXT("%s launched arrow from %s."), *GetNameSafe(NewShooter), *StartLocation.ToString());
}

void AHSArrowProjectileActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFlying)
	{
		return;
	}

	const float StepDistance = Speed * FMath::Max(0.0f, DeltaSeconds);
	const FVector NextLocation = PreviousLocation + (FlightDirection * StepDistance);

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HSArrowProjectileTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	if (AActor* ShooterActor = Shooter.Get())
	{
		QueryParams.AddIgnoredActor(ShooterActor);
	}

	UWorld* World = GetWorld();
	if (World && World->LineTraceSingleByChannel(Hit, PreviousLocation, NextLocation, ECC_Visibility, QueryParams))
	{
		HandleTraceHit(Hit);
		return;
	}

	SetActorLocation(NextLocation);
	PreviousLocation = NextLocation;
	DistanceTraveled += StepDistance;
	if (DistanceTraveled >= MaxRange)
	{
		Destroy();
	}
}

void AHSArrowProjectileActor::HandleTraceHit(const FHitResult& Hit)
{
	StopFlightAt(Hit.ImpactPoint);

	AHSAnimalActor* Animal = Cast<AHSAnimalActor>(Hit.GetActor());
	if (!Animal || Animal->IsDead())
	{
		OnArrowFeedback.Broadcast(LOCTEXT("ArrowHitWorld", "Arrow struck."));
		return;
	}

	const float AppliedDamage = Animal->ApplyHuntingDamage(Damage, Shooter.Get());
	if (AppliedDamage <= 0.0f)
	{
		OnArrowFeedback.Broadcast(LOCTEXT("ArrowGlanced", "Arrow glanced off."));
		return;
	}

	OnArrowFeedback.Broadcast(Animal->IsDead()
		? FText::Format(LOCTEXT("ArrowKilledAnimal", "Arrow killed {0}."), Animal->GetDisplayName())
		: FText::Format(LOCTEXT("ArrowHitAnimal", "Arrow hit {0}."), Animal->GetDisplayName()));
}

void AHSArrowProjectileActor::StopFlightAt(FVector ImpactLocation)
{
	bFlying = false;
	SetActorTickEnabled(false);
	SetActorLocation(ImpactLocation);
	PreviousLocation = ImpactLocation;
	SetLifeSpan(4.0f);
}

#undef LOCTEXT_NAMESPACE
