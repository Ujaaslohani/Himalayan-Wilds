#include "Quests/HSQuestHandoffComponent.h"

#include "Core/HSLogCategories.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Quests/HSQuestDefinition.h"
#include "Resources/HSResourceCarryComponent.h"

#define LOCTEXT_NAMESPACE "HSQuestHandoffComponent"

UHSQuestHandoffComponent::UHSQuestHandoffComponent()
	: bAccepted(false)
	, bCompleted(false)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSQuestHandoffComponent::SetQuestDefinition(UHSQuestDefinition* NewQuestDefinition)
{
	QuestDefinition = NewQuestDefinition;
	bAccepted = false;
	bCompleted = false;
	GrantedKnowledgeTags.Reset();
	GrantedUnlockTags.Reset();
}

EHSQuestHandoffState UHSQuestHandoffComponent::GetHandoffState() const
{
	if (!QuestDefinition)
	{
		return EHSQuestHandoffState::Unavailable;
	}

	if (bCompleted)
	{
		return EHSQuestHandoffState::Completed;
	}

	return bAccepted ? EHSQuestHandoffState::Accepted : EHSQuestHandoffState::Available;
}

FText UHSQuestHandoffComponent::GetInteractionPrompt(const AActor* Interactor) const
{
	if (!QuestDefinition)
	{
		return FText::GetEmpty();
	}

	if (bCompleted)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("QuestName"), QuestDefinition->DisplayName);
		return FText::Format(LOCTEXT("CompletedQuestPrompt", "Completed {QuestName}"), Arguments);
	}

	if (bAccepted && CanInteractorCompleteQuest(Interactor))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("QuestName"), QuestDefinition->DisplayName);
		return FText::Format(LOCTEXT("ReturnQuestPrompt", "Return {QuestName}"), Arguments);
	}

	if (bAccepted)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("QuestName"), QuestDefinition->DisplayName);
		return FText::Format(LOCTEXT("AcceptedQuestPrompt", "Discuss {QuestName}"), Arguments);
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("QuestName"), QuestDefinition->DisplayName);
	return FText::Format(LOCTEXT("OfferQuestPrompt", "Accept {QuestName}"), Arguments);
}

bool UHSQuestHandoffComponent::TryOfferQuest(AActor* Interactor)
{
	if (!QuestDefinition || bCompleted)
	{
		return false;
	}

	if (bAccepted)
	{
		UE_LOG(
			LogHSQuest,
			Verbose,
			TEXT("%s revisited accepted quest %s from %s."),
			*GetNameSafe(Interactor),
			*GetNameSafe(QuestDefinition),
			*GetNameSafe(GetOwner()));
		return false;
	}

	bAccepted = true;
	UE_LOG(
		LogHSQuest,
		Log,
		TEXT("%s accepted quest %s from %s."),
		*GetNameSafe(Interactor),
		*GetNameSafe(QuestDefinition),
		*GetNameSafe(GetOwner()));
	return true;
}

bool UHSQuestHandoffComponent::TryCompleteQuest(AActor* Interactor)
{
	if (!QuestDefinition || !bAccepted || bCompleted || !CanInteractorCompleteQuest(Interactor))
	{
		return false;
	}

	bCompleted = true;
	GrantedKnowledgeTags = QuestDefinition->RewardKnowledgeTags;
	GrantedUnlockTags = QuestDefinition->RewardUnlockTags;
	if (UHSPlayerProgressComponent* ProgressComponent = Interactor ? Interactor->FindComponentByClass<UHSPlayerProgressComponent>() : nullptr)
	{
		ProgressComponent->MarkMilestoneCompleted(QuestDefinition->QuestTag);
	}
	UE_LOG(
		LogHSQuest,
		Log,
		TEXT("%s completed quest %s with %s."),
		*GetNameSafe(Interactor),
		*GetNameSafe(QuestDefinition),
		*GetNameSafe(GetOwner()));
	return true;
}

bool UHSQuestHandoffComponent::TryAdvanceQuest(AActor* Interactor)
{
	if (TryCompleteQuest(Interactor))
	{
		return true;
	}

	return TryOfferQuest(Interactor);
}

bool UHSQuestHandoffComponent::CanInteractorCompleteQuest(const AActor* Interactor) const
{
	if (!QuestDefinition || !Interactor)
	{
		return false;
	}

	const UHSResourceCarryComponent* CarryComponent = Interactor->FindComponentByClass<UHSResourceCarryComponent>();
	if (!CarryComponent)
	{
		return false;
	}

	for (const FHSItemStack& RequiredStack : QuestDefinition->TurnInRequirement.RequiredItems)
	{
		if (!CarryComponent->HasAtLeast(RequiredStack))
		{
			return false;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
