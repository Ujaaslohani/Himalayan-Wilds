#include "Resources/HSItemDefinition.h"

FPrimaryAssetId UHSItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("HSItemDefinition"), GetFName());
}

bool UHSItemDefinition::IsDefinitionValid(FText* OutReason) const
{
	if (DisplayName.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidItemMissingName", "Item definition is missing a display name.");
		}
		return false;
	}

	if (!ItemTag.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidItemMissingTag", "Item definition is missing an item tag.");
		}
		return false;
	}

	return true;
}
