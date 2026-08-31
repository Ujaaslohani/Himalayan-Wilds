#include "Interaction/HSInteractionSensorComponent.h"

#include "Camera/CameraComponent.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/HSInteractable.h"
#include "TimerManager.h"

UHSInteractionSensorComponent::UHSInteractionSensorComponent()
	: InteractionDistance(300.0f)
	, TraceChannel(ECC_Visibility)
	, FocusRefreshInterval(0.1f)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSInteractionSensorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FocusRefreshTimerHandle, this, &UHSInteractionSensorComponent::RefreshFocus, FocusRefreshInterval, true);
	}
}

void UHSInteractionSensorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusRefreshTimerHandle);
	}

	ClearFocus();
	Super::EndPlay(EndPlayReason);
}

bool UHSInteractionSensorComponent::TryInteract()
{
	RefreshFocus();

	AActor* InteractableActor = FocusedInteractable.Get();
	if (!InteractableActor)
	{
		UE_LOG(LogHSInteraction, Verbose, TEXT("%s found no interactable target."), *GetNameSafe(GetOwner()));
		return false;
	}

	UE_LOG(LogHSInteraction, Verbose, TEXT("%s interacted with %s."), *GetNameSafe(GetOwner()), *GetNameSafe(InteractableActor));
	HSInteraction::Interact(InteractableActor, GetOwner());
	RefreshFocus();
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

	return HSInteraction::CanInteract(HitActor, GetOwner()) ? HitActor : nullptr;
}

void UHSInteractionSensorComponent::RefreshFocus()
{
	FHitResult Hit;
	AActor* InteractableActor = FindInteractable(Hit);
	if (!InteractableActor)
	{
		ClearFocus();
		return;
	}

	FocusedInteractable = InteractableActor;
	FocusedInteractionPrompt = HSInteraction::GetInteractionPrompt(InteractableActor, GetOwner());
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

void UHSInteractionSensorComponent::ClearFocus()
{
	FocusedInteractable.Reset();
	FocusedInteractionPrompt = FText::GetEmpty();
}
