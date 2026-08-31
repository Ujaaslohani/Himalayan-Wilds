#include "Animals/HSAnimalDefinition.h"

FPrimaryAssetId UHSAnimalDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("HSAnimalDefinition"), GetFName());
}

bool UHSAnimalDefinition::IsDefinitionValid(FText* OutReason) const
{
	if (DisplayName.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidAnimalMissingName", "Animal definition is missing a display name.");
		}
		return false;
	}

	if (!SpeciesTag.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidAnimalMissingSpeciesTag", "Animal definition is missing a species tag.");
		}
		return false;
	}

	if (HarvestDrops.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidAnimalMissingHarvestDrops", "Animal definition needs at least one harvest drop.");
		}
		return false;
	}

	for (const FHSItemStack& Drop : HarvestDrops)
	{
		if (!Drop.IsValidStack())
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidAnimalBadHarvestDrop", "Animal definition contains an invalid harvest drop.");
			}
			return false;
		}
	}

	return true;
}
