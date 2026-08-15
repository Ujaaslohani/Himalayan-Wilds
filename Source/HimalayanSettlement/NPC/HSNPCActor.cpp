#include "NPC/HSNPCActor.h"

#include "Components/BoxComponent.h"
#include "Core/HSLogCategories.h"
#include "NPC/HSNPCDefinition.h"
#include "Quests/HSQuestHandoffComponent.h"

#define LOCTEXT_NAMESPACE "HSNPCActor"

AHSNPCActor::AHSNPCActor()
	: FallbackDisplayName(LOCTEXT("DefaultHunterName", "Hunter"))
	, InteractionPromptFormat(LOCTEXT("TalkToNPCPrompt", "Talk to {Name}"))
	, bCanTalk(true)
	, InteractionCount(0)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(40.0f, 40.0f, 90.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	QuestHandoffComponent = CreateDefaultSubobject<UHSQuestHandoffComponent>(TEXT("QuestHandoff"));
}

FText AHSNPCActor::GetDisplayName() const
{
	return NPCDefinition && !NPCDefinition->DisplayName.IsEmpty() ? NPCDefinition->DisplayName : FallbackDisplayName;
}

void AHSNPCActor::SetNPCDefinition(UHSNPCDefinition* NewNPCDefinition)
{
	NPCDefinition = NewNPCDefinition;
}

FGameplayTag AHSNPCActor::GetProfessionTag() const
{
	return NPCDefinition ? NPCDefinition->ProfessionTag : FGameplayTag();
}

void AHSNPCActor::SetOfferedQuest(UHSQuestDefinition* NewQuestDefinition)
{
	if (QuestHandoffComponent)
	{
		QuestHandoffComponent->SetQuestDefinition(NewQuestDefinition);
	}
}

FText AHSNPCActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	if (QuestHandoffComponent)
	{
		const FText QuestPrompt = QuestHandoffComponent->GetInteractionPrompt();
		if (!QuestPrompt.IsEmpty())
		{
			return QuestPrompt;
		}
	}

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
	const bool bOfferedQuest = QuestHandoffComponent && QuestHandoffComponent->TryOfferQuest(Interactor);
	UE_LOG(
		LogHSInteraction,
		Log,
		TEXT("%s started conversation with %s. Quest offered: %s. Interaction count: %d."),
		*GetNameSafe(Interactor),
		*GetNameSafe(this),
		bOfferedQuest ? TEXT("true") : TEXT("false"),
		InteractionCount);
}

#undef LOCTEXT_NAMESPACE
