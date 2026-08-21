#include "Hunting/HSHuntingComponent.h"

#include "Animals/HSAnimalActor.h"
#include "Camera/CameraComponent.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "HSHuntingComponent"

UHSHuntingComponent::UHSHuntingComponent()
	: HuntDistance(1200.0f)
	, HuntDamage(75.0f)
	, TraceChannel(ECC_Visibility)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSHuntingComponent::TryHunt()
{
	const AActor* Owner = GetOwner();
	const UHSPlayerEquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr;
	if (!Equipment || !Equipment->CanUseBow())
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "BowRequired", "Equip a bow and arrows first."));
		UE_LOG(LogHSAI, Verbose, TEXT("%s tried to hunt without an equipped bow and arrows."), *GetNameSafe(GetOwner()));
		return false;
	}

	FHitResult Hit;
	AHSAnimalActor* Target = FindHuntTarget(Hit);
	if (!Target)
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "NoTarget", "No animal target."));
		UE_LOG(LogHSAI, Verbose, TEXT("%s found no hunting target."), *GetNameSafe(GetOwner()));
		return false;
	}

	UHSPlayerEquipmentComponent* MutableEquipment = Owner ? Owner->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr;
	if (!MutableEquipment || !MutableEquipment->ConsumeArrow())
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "ArrowConsumeFailed", "No arrow ready."));
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
	if (AppliedDamage > 0.0f)
	{
		OnHuntingFeedback.Broadcast(Target->IsDead()
			? FText::Format(LOCTEXT("AnimalKilled", "Killed {0}."), Target->GetDisplayName())
			: FText::Format(LOCTEXT("AnimalHit", "Hit {0}."), Target->GetDisplayName()));
	}

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

#undef LOCTEXT_NAMESPACE
