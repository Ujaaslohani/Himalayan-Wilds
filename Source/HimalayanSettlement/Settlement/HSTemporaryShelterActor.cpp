#include "Settlement/HSTemporaryShelterActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

#define LOCTEXT_NAMESPACE "HSTemporaryShelterActor"

AHSTemporaryShelterActor::AHSTemporaryShelterActor()
	: MaxSleepUses(1)
	, RemainingSleepUses(1)
	, bExpired(false)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(95.0f, 70.0f, 50.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(1.35f, 0.85f, 0.35f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(ConeMesh.Object);
	}
}

bool AHSTemporaryShelterActor::CanSleep(const AActor* Interactor) const
{
	return !bExpired && RemainingSleepUses > 0 && ResolveWorldTime() != nullptr;
}

FText AHSTemporaryShelterActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	if (bExpired || RemainingSleepUses <= 0)
	{
		return LOCTEXT("ShelterExpiredPrompt", "Temporary shelter expired");
	}

	const UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	if (!WorldTime)
	{
		return LOCTEXT("ShelterUnavailablePrompt", "Shelter unavailable");
	}

	return WorldTime->GetTimePhase() == EHSTimePhase::Day
		? LOCTEXT("SleepUntilNightPrompt", "Sleep until night")
		: LOCTEXT("SleepUntilDayPrompt", "Sleep until day");
}

bool AHSTemporaryShelterActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return true;
}

void AHSTemporaryShelterActor::Interact_Implementation(AActor* Interactor)
{
	UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	if (!WorldTime || bExpired || RemainingSleepUses <= 0)
	{
		UE_LOG(LogHSInteraction, Verbose, TEXT("%s checked unavailable temporary shelter %s."), *GetNameSafe(Interactor), *GetNameSafe(this));
		return;
	}

	const EHSTimePhase NewTimePhase = WorldTime->AdvanceHalfDay();
	--RemainingSleepUses;
	bExpired = RemainingSleepUses <= 0;

	if (UHSPlayerProgressComponent* ProgressComponent = Interactor ? Interactor->FindComponentByClass<UHSPlayerProgressComponent>() : nullptr)
	{
		ProgressComponent->MarkMilestoneCompleted(TAG_HS_Milestone_TemporaryShelter_Slept);
	}

	UE_LOG(
		LogHSInteraction,
		Log,
		TEXT("%s slept at %s. New time phase: %s. Shelter expired: %s."),
		*GetNameSafe(Interactor),
		*GetNameSafe(this),
		NewTimePhase == EHSTimePhase::Day ? TEXT("Day") : TEXT("Night"),
		bExpired ? TEXT("true") : TEXT("false"));
}

UHSWorldTimeComponent* AHSTemporaryShelterActor::ResolveWorldTime() const
{
	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	return GameState ? GameState->GetWorldTimeComponent() : nullptr;
}

#undef LOCTEXT_NAMESPACE
