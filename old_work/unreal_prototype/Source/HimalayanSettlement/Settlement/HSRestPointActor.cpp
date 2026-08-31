#include "Settlement/HSRestPointActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSRestPointActor"

AHSRestPointActor::AHSRestPointActor()
	: RequiredMilestoneTag(TAG_HS_Quest_OneMorningHunt)
	, CompletionMilestoneTag(TAG_HS_Milestone_OneMorningHunt_DayEnded)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(70.0f, 70.0f, 45.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.25f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

bool AHSRestPointActor::HasRested(const AActor* Interactor) const
{
	const UHSPlayerProgressComponent* ProgressComponent = Interactor ? Interactor->FindComponentByClass<UHSPlayerProgressComponent>() : nullptr;
	return ProgressComponent && ProgressComponent->HasCompletedMilestone(CompletionMilestoneTag);
}

bool AHSRestPointActor::IsReadyToRest(const AActor* Interactor) const
{
	const UHSPlayerProgressComponent* ProgressComponent = Interactor ? Interactor->FindComponentByClass<UHSPlayerProgressComponent>() : nullptr;
	return ProgressComponent && ProgressComponent->HasCompletedMilestone(RequiredMilestoneTag);
}

FText AHSRestPointActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	if (HasRested(Interactor))
	{
		return LOCTEXT("RestCompletePrompt", "Morning hunt complete");
	}

	if (!IsReadyToRest(Interactor))
	{
		return LOCTEXT("RestBlockedPrompt", "Finish the hunt before resting");
	}

	return LOCTEXT("RestReadyPrompt", "Rest until evening");
}

bool AHSRestPointActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return true;
}

void AHSRestPointActor::Interact_Implementation(AActor* Interactor)
{
	if (HasRested(Interactor) || !IsReadyToRest(Interactor))
	{
		UE_LOG(LogHSInteraction, Verbose, TEXT("%s checked rest point %s without completing rest."), *GetNameSafe(Interactor), *GetNameSafe(this));
		return;
	}

	if (UHSPlayerProgressComponent* ProgressComponent = Interactor ? Interactor->FindComponentByClass<UHSPlayerProgressComponent>() : nullptr)
	{
		ProgressComponent->MarkMilestoneCompleted(CompletionMilestoneTag);
		UE_LOG(LogHSInteraction, Log, TEXT("%s ended One Morning Hunt at %s."), *GetNameSafe(Interactor), *GetNameSafe(this));
	}
}

#undef LOCTEXT_NAMESPACE
