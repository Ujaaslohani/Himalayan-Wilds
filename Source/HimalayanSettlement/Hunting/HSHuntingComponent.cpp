#include "Hunting/HSHuntingComponent.h"

#include "Animals/HSAnimalActor.h"
#include "Camera/CameraComponent.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UHSHuntingComponent::UHSHuntingComponent()
	: HuntDistance(1200.0f)
	, HuntDamage(75.0f)
	, TraceChannel(ECC_Visibility)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSHuntingComponent::TryHunt()
{
	FHitResult Hit;
	AHSAnimalActor* Target = FindHuntTarget(Hit);
	if (!Target)
	{
		UE_LOG(LogHSAI, Verbose, TEXT("%s found no hunting target."), *GetNameSafe(GetOwner()));
		return false;
	}

	return ApplyDamageToTarget(Target);
}

AHSAnimalActor* UHSHuntingComponent::FindHuntTarget(FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	if (!ResolveTraceView(TraceStart, TraceDirection))
	{
		return nullptr;
	}

	const FVector TraceEnd = TraceStart + TraceDirection * HuntDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HSHuntingTrace), false, GetOwner());
	QueryParams.bReturnPhysicalMaterial = false;

	if (!World->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, TraceChannel, QueryParams))
	{
		return nullptr;
	}

	AHSAnimalActor* Animal = Cast<AHSAnimalActor>(OutHit.GetActor());
	return Animal && !Animal->IsDead() ? Animal : nullptr;
}

bool UHSHuntingComponent::ApplyDamageToTarget(AHSAnimalActor* Target)
{
	if (!Target || Target->IsDead())
	{
		return false;
	}

	const float AppliedDamage = Target->ApplyHuntingDamage(HuntDamage, GetOwner());
	UE_LOG(
		LogHSAI,
		Log,
		TEXT("%s applied %.2f hunting damage to %s."),
		*GetNameSafe(GetOwner()),
		AppliedDamage,
		*GetNameSafe(Target));
	return AppliedDamage > 0.0f;
}

bool UHSHuntingComponent::ResolveTraceView(FVector& OutStart, FVector& OutDirection) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	if (const UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>())
	{
		OutStart = Camera->GetComponentLocation();
		OutDirection = Camera->GetForwardVector();
		return true;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	Owner->GetActorEyesViewPoint(OutStart, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}
