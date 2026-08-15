#include "Interaction/HSInteractionSensorComponent.h"

#include "Camera/CameraComponent.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"

UHSInteractionSensorComponent::UHSInteractionSensorComponent()
	: InteractionDistance(300.0f)
	, TraceChannel(ECC_Visibility)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSInteractionSensorComponent::TryInteract()
{
	FHitResult Hit;
	AActor* InteractableActor = FindInteractable(Hit);
	if (!InteractableActor)
	{
		UE_LOG(LogHSInteraction, Verbose, TEXT("%s found no interactable target."), *GetNameSafe(GetOwner()));
		return false;
	}

	UE_LOG(LogHSInteraction, Verbose, TEXT("%s interacted with %s."), *GetNameSafe(GetOwner()), *GetNameSafe(InteractableActor));
	IHSInteractable::Execute_Interact(InteractableActor, GetOwner());
	return true;
}

AActor* UHSInteractionSensorComponent::FindInteractable(FHitResult& OutHit) const
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

	const FVector TraceEnd = TraceStart + TraceDirection * InteractionDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HSInteractionTrace), false, GetOwner());
	QueryParams.bReturnPhysicalMaterial = false;

	if (!World->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, TraceChannel, QueryParams))
	{
		return nullptr;
	}

	AActor* HitActor = OutHit.GetActor();
	if (!HitActor || !HitActor->GetClass()->ImplementsInterface(UHSInteractable::StaticClass()))
	{
		return nullptr;
	}

	return IHSInteractable::Execute_CanInteract(HitActor, GetOwner()) ? HitActor : nullptr;
}

bool UHSInteractionSensorComponent::ResolveTraceView(FVector& OutStart, FVector& OutDirection) const
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
