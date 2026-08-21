#include "Hunting/HSArrowProjectileActor.h"

#include "Animals/HSAnimalActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HSWorldCleanupSubsystem.h"

#define LOCTEXT_NAMESPACE "HSArrowProjectileActor"

AHSArrowProjectileActor::AHSArrowProjectileActor()
	: FlightDirection(FVector::ForwardVector)
	, PreviousLocation(FVector::ZeroVector)
	, Damage(0.0f)
	, MaxRange(1200.0f)
	, Speed(3200.0f)
	, DistanceTraveled(0.0f)
	, bFlying(false)
	, bImpacted(false)
	, ImpactLocation(FVector::ZeroVector)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ArrowRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArrowRoot"));
	RootComponent = ArrowRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	ArrowShaftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowShaftMesh"));
	ArrowShaftMesh->SetupAttachment(ArrowRoot);
	ArrowShaftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowShaftMesh->SetRelativeLocation(FVector::ZeroVector);
	ArrowShaftMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	ArrowShaftMesh->SetRelativeScale3D(FVector(0.018f, 0.018f, 0.78f));
	if (CylinderMesh.Succeeded())
	{
		ArrowShaftMesh->SetStaticMesh(CylinderMesh.Object);
	}

	ArrowHeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowHeadMesh"));
	ArrowHeadMesh->SetupAttachment(ArrowRoot);
	ArrowHeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowHeadMesh->SetRelativeLocation(FVector(44.0f, 0.0f, 0.0f));
	ArrowHeadMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	ArrowHeadMesh->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.14f));
	if (ConeMesh.Succeeded())
	{
		ArrowHeadMesh->SetStaticMesh(ConeMesh.Object);
	}

	ArrowFletchingTopMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowFletchingTopMesh"));
	ArrowFletchingTopMesh->SetupAttachment(ArrowRoot);
	ArrowFletchingTopMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowFletchingTopMesh->SetRelativeLocation(FVector(-40.0f, 0.0f, 4.5f));
	ArrowFletchingTopMesh->SetRelativeScale3D(FVector(0.12f, 0.012f, 0.035f));
	if (CubeMesh.Succeeded())
	{
		ArrowFletchingTopMesh->SetStaticMesh(CubeMesh.Object);
	}

	ArrowFletchingSideMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowFletchingSideMesh"));
	ArrowFletchingSideMesh->SetupAttachment(ArrowRoot);
	ArrowFletchingSideMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowFletchingSideMesh->SetRelativeLocation(FVector(-40.0f, 4.5f, 0.0f));
	ArrowFletchingSideMesh->SetRelativeScale3D(FVector(0.12f, 0.035f, 0.012f));
	if (CubeMesh.Succeeded())
	{
		ArrowFletchingSideMesh->SetStaticMesh(CubeMesh.Object);
	}

	ArrowTrailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowTrailMesh"));
	ArrowTrailMesh->SetupAttachment(ArrowRoot);
	ArrowTrailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowTrailMesh->SetGenerateOverlapEvents(false);
	ArrowTrailMesh->SetRelativeLocation(FVector(-78.0f, 0.0f, 0.0f));
	ArrowTrailMesh->SetRelativeScale3D(FVector(0.7f, 0.006f, 0.006f));
	ArrowTrailMesh->SetHiddenInGame(true);
	if (CubeMesh.Succeeded())
	{
		ArrowTrailMesh->SetStaticMesh(CubeMesh.Object);
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
	bImpacted = false;
	ImpactLocation = FVector::ZeroVector;

	SetActorLocation(StartLocation);
	SetActorRotation(FlightDirection.Rotation());
	PreviousLocation = StartLocation;
	SetActorTickEnabled(true);
	if (ArrowTrailMesh)
	{
		ArrowTrailMesh->SetHiddenInGame(false);
	}
	if (UWorld* World = GetWorld())
	{
		if (UHSWorldCleanupSubsystem* CleanupSubsystem = World->GetSubsystem<UHSWorldCleanupSubsystem>())
		{
			CleanupSubsystem->RegisterTemporaryActor(this, 8.0f, TEXT("FlyingArrow"));
		}
	}

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

void AHSArrowProjectileActor::StopFlightAt(FVector NewImpactLocation)
{
	bFlying = false;
	bImpacted = true;
	ImpactLocation = NewImpactLocation;
	SetActorTickEnabled(false);
	SetActorLocation(NewImpactLocation);
	PreviousLocation = NewImpactLocation;
	if (ArrowTrailMesh)
	{
		ArrowTrailMesh->SetHiddenInGame(true);
	}
	SetLifeSpan(4.0f);
	if (UWorld* World = GetWorld())
	{
		if (UHSWorldCleanupSubsystem* CleanupSubsystem = World->GetSubsystem<UHSWorldCleanupSubsystem>())
		{
			CleanupSubsystem->RegisterTemporaryActor(this, 4.0f, TEXT("StoppedArrow"));
		}
	}
}

#undef LOCTEXT_NAMESPACE
