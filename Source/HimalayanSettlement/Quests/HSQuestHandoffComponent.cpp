#include "Quests/HSQuestHandoffComponent.h"

#include "Core/HSLogCategories.h"
#include "Quests/HSQuestDefinition.h"

#define LOCTEXT_NAMESPACE "HSQuestHandoffComponent"

UHSQuestHandoffComponent::UHSQuestHandoffComponent()
	: bAccepted(false)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSQuestHandoffComponent::SetQuestDefinition(UHSQuestDefinition* NewQuestDefinition)
{
	QuestDefinition = NewQuestDefinition;
	bAccepted = false;
}

EHSQuestHandoffState UHSQuestHandoffComponent::GetHandoffState() const
{
	if (!QuestDefinition)
	{
		return EHSQuestHandoffState::Unavailable;
	}

	return bAccepted ? EHSQuestHandoffState::Accepted : EHSQuestHandoffState::Available;
}

FText UHSQuestHandoffComponent::GetInteractionPrompt() const
{
	if (!QuestDefinition)
	{
		return FText::GetEmpty();
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
	if (!QuestDefinition)
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

#undef LOCTEXT_NAMESPACE
