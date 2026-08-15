#include "NPC/HSNPCActor.h"

#include "Core/HSLogCategories.h"
#include "NPC/HSNPCDefinition.h"

#define LOCTEXT_NAMESPACE "HSNPCActor"

AHSNPCActor::AHSNPCActor()
	: FallbackDisplayName(LOCTEXT("DefaultHunterName", "Hunter"))
	, InteractionPromptFormat(LOCTEXT("TalkToNPCPrompt", "Talk to {Name}"))
	, bCanTalk(true)
	, InteractionCount(0)
{
	PrimaryActorTick.bCanEverTick = false;
}

FText AHSNPCActor::GetDisplayName() const
{
	return NPCDefinition && !NPCDefinition->DisplayName.IsEmpty() ? NPCDefinition->DisplayName : FallbackDisplayName;
}

FGameplayTag AHSNPCActor::GetProfessionTag() const
{
	return NPCDefinition ? NPCDefinition->ProfessionTag : FGameplayTag();
}

FText AHSNPCActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Name"), GetDisplayName());
	return FText::Format(InteractionPromptFormat, Arguments);
}

bool AHSNPCActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return bCanTalk;
}

void AHSNPCActor::Interact_Implementation(AActor* Interactor)
{
	++InteractionCount;
	UE_LOG(
		LogHSInteraction,
		Log,
		TEXT("%s started conversation with %s. Interaction count: %d."),
		*GetNameSafe(Interactor),
		*GetNameSafe(this),
		InteractionCount);
}

#undef LOCTEXT_NAMESPACE
