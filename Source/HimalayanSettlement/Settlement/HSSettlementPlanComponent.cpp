#include "Settlement/HSSettlementPlanComponent.h"

#include "Building/HSBuildingDefinition.h"
#include "Core/HSLogCategories.h"

#define LOCTEXT_NAMESPACE "HSSettlementPlanComponent"

UHSSettlementPlanComponent::UHSSettlementPlanComponent()
	: bHasSettlementClaim(false)
	, SettlementClaimCenter(FVector::ZeroVector)
	, ClaimRadius(1500.0f)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSSettlementPlanComponent::ClaimSettlement(FVector ClaimCenter)
{
	if (bHasSettlementClaim)
	{
		return false;
	}

	bHasSettlementClaim = true;
	SettlementClaimCenter = ClaimCenter;
	OnSettlementClaimed.Broadcast(SettlementClaimCenter);
	UE_LOG(LogHSSettlement, Log, TEXT("%s claimed settlement center at %s."), *GetNameSafe(GetOwner()), *SettlementClaimCenter.ToCompactString());
	return true;
}

bool UHSSettlementPlanComponent::CanPlaceSchema(const UHSBuildingDefinition* BuildingDefinition, FVector Location, FText* OutReason) const
{
	if (!bHasSettlementClaim)
	{
		if (OutReason)
		{
			*OutReason = LOCTEXT("NoSettlementClaim", "Claim a settlement site first.");
		}
		return false;
	}

	if (!BuildingDefinition || !BuildingDefinition->IsDefinitionValid())
	{
		if (OutReason)
		{
			*OutReason = LOCTEXT("InvalidBuildingDefinition", "Schema has no valid building definition.");
		}
		return false;
	}

	if (HasPendingSchema(BuildingDefinition->BuildingTag))
	{
		if (OutReason)
		{
			*OutReason = LOCTEXT("DuplicatePendingSchema", "That schema is already pending construction.");
		}
		return false;
	}

	if (FVector::DistSquared2D(SettlementClaimCenter, Location) > FMath::Square(ClaimRadius))
	{
		if (OutReason)
		{
			*OutReason = LOCTEXT("SchemaOutsideClaim", "Schema is outside the claimed settlement area.");
		}
		return false;
	}

	return true;
}

bool UHSSettlementPlanComponent::PlaceSchema(const UHSBuildingDefinition* BuildingDefinition, FVector Location, float YawDegrees, FHSPendingBuildingSchema& OutPendingSchema)
{
	FText FailureReason;
	if (!CanPlaceSchema(BuildingDefinition, Location, &FailureReason))
	{
		UE_LOG(LogHSSettlement, Verbose, TEXT("%s could not place schema: %s"), *GetNameSafe(GetOwner()), *FailureReason.ToString());
		return false;
	}

	OutPendingSchema = FHSPendingBuildingSchema();
	OutPendingSchema.BuildingTag = BuildingDefinition->BuildingTag;
	OutPendingSchema.Location = Location;
	OutPendingSchema.YawDegrees = YawDegrees;
	PendingSchemas.Add(OutPendingSchema);
	OnBuildingSchemaPlaced.Broadcast(OutPendingSchema);

	UE_LOG(
		LogHSSettlement,
		Log,
		TEXT("%s placed pending schema %s at %s."),
		*GetNameSafe(GetOwner()),
		*OutPendingSchema.BuildingTag.ToString(),
		*OutPendingSchema.Location.ToCompactString());

	return true;
}

bool UHSSettlementPlanComponent::HasPendingSchema(FGameplayTag BuildingTag) const
{
	if (!BuildingTag.IsValid())
	{
		return false;
	}

	for (const FHSPendingBuildingSchema& PendingSchema : PendingSchemas)
	{
		if (PendingSchema.BuildingTag == BuildingTag)
		{
			return true;
		}
	}

	return false;
}

bool UHSSettlementPlanComponent::ConsumePendingSchema(FGameplayTag BuildingTag, FHSPendingBuildingSchema& OutPendingSchema)
{
	if (!BuildingTag.IsValid())
	{
		return false;
	}

	for (int32 Index = 0; Index < PendingSchemas.Num(); ++Index)
	{
		if (PendingSchemas[Index].BuildingTag == BuildingTag)
		{
			OutPendingSchema = PendingSchemas[Index];
			PendingSchemas.RemoveAt(Index);
			UE_LOG(LogHSSettlement, Log, TEXT("%s promoted pending schema %s into construction."), *GetNameSafe(GetOwner()), *BuildingTag.ToString());
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
