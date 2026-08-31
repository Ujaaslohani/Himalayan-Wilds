#include "NPC/HSNPCDefinition.h"

FPrimaryAssetId UHSNPCDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("HSNPCDefinition"), GetFName());
}

bool UHSNPCDefinition::IsDefinitionValid(FText* OutReason) const
{
	if (DisplayName.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidNPCMissingName", "NPC definition is missing a display name.");
		}
		return false;
	}

	if (!ProfessionTag.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidNPCMissingProfession", "NPC definition is missing a profession tag.");
		}
		return false;
	}

	return true;
}
