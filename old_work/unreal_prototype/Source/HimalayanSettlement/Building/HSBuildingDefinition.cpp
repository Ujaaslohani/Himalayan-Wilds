#include "Building/HSBuildingDefinition.h"

FPrimaryAssetId UHSBuildingDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("HSBuildingDefinition"), GetFName());
}

bool UHSBuildingDefinition::IsDefinitionValid(FText* OutReason) const
{
	if (DisplayName.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidBuildingMissingName", "Building definition is missing a display name.");
		}
		return false;
	}

	if (!BuildingTag.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidBuildingMissingTag", "Building definition is missing a building tag.");
		}
		return false;
	}

	for (const FHSItemStack& Cost : ResourceCosts)
	{
		if (!Cost.IsValidStack())
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidBuildingBadCost", "Building definition contains an invalid resource cost.");
			}
			return false;
		}
	}

	return true;
}
