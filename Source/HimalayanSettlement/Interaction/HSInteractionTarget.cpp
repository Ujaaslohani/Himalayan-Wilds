#include "Interaction/HSInteractionTarget.h"

#include "Core/HSLogCategories.h"

#define LOCTEXT_NAMESPACE "HSInteractionTarget"

AHSInteractionTarget::AHSInteractionTarget()
	: InteractionPrompt(LOCTEXT("DefaultInteractionPrompt", "Interact"))
	, bInteractionEnabled(true)
	, InteractionCount(0)
{
	PrimaryActorTick.bCanEverTick = false;
}

FText AHSInteractionTarget::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	return InteractionPrompt;
}

bool AHSInteractionTarget::CanInteract_Implementation(const AActor* Interactor) const
{
	return bInteractionEnabled;
}

void AHSInteractionTarget::Interact_Implementation(AActor* Interactor)
{
	++InteractionCount;
	UE_LOG(LogHSInteraction, Log, TEXT("%s used %s. Interaction count: %d."), *GetNameSafe(Interactor), *GetNameSafe(this), InteractionCount);
}

#undef LOCTEXT_NAMESPACE
