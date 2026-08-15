#include "Quests/HSQuestDefinition.h"

FPrimaryAssetId UHSQuestDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("HSQuestDefinition"), GetFName());
}

bool UHSQuestDefinition::IsDefinitionValid(FText* OutReason) const
{
	if (DisplayName.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidQuestMissingName", "Quest definition is missing a display name.");
		}
		return false;
	}

	if (!QuestTag.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidQuestMissingTag", "Quest definition is missing a quest tag.");
		}
		return false;
	}

	if (!TurnInRequirement.IsValidRequirement())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidQuestBadTurnIn", "Quest definition contains an invalid turn-in requirement.");
		}
		return false;
	}

	return true;
}
