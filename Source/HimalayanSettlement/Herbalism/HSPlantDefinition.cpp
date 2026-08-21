#include "Herbalism/HSPlantDefinition.h"

UHSPlantDefinition::UHSPlantDefinition()
	: MovementSpeedMultiplier(1.0f)
	, DurationDays(0.5f)
{
}

FPrimaryAssetId UHSPlantDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("HSPlantDefinition"), GetFName());
}

bool UHSPlantDefinition::IsDefinitionValid(FText* OutReason) const
{
	if (DisplayName.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidPlantMissingName", "Plant definition is missing a display name.");
		}
		return false;
	}

	if (!PlantTag.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidPlantMissingTag", "Plant definition is missing a plant tag.");
		}
		return false;
	}

	if (!EffectTag.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidPlantMissingEffect", "Plant definition is missing an effect tag.");
		}
		return false;
	}

	if (MovementSpeedMultiplier < 1.0f || DurationDays <= 0.0f)
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HimalayanSettlement", "InvalidPlantBadEffectValues", "Plant definition has invalid effect values.");
		}
		return false;
	}

	return true;
}
