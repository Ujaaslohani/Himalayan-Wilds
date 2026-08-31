#include "Exploration/HSExplorationStateComponent.h"

UHSExplorationStateComponent::UHSExplorationStateComponent()
	: NaturalBoundaryRadius(4200.0f)
	, bLowEndStreamingEnabled(true)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSExplorationStateComponent::DiscoverLocation(FName LocationId, FText DisplayName, FGameplayTag LocationTag, FVector Location)
{
	if (LocationId.IsNone() || !LocationTag.IsValid() || HasDiscoveredLocation(LocationId))
	{
		return false;
	}

	FHSDiscoveredLocation DiscoveredLocation;
	DiscoveredLocation.LocationId = LocationId;
	DiscoveredLocation.DisplayName = DisplayName;
	DiscoveredLocation.LocationTag = LocationTag;
	DiscoveredLocation.Location = Location;
	DiscoveredLocations.Add(DiscoveredLocation);
	return true;
}

void UHSExplorationStateComponent::SetLowEndStreamingEnabled(bool bNewEnabled)
{
	bLowEndStreamingEnabled = bNewEnabled;
}

int32 UHSExplorationStateComponent::CountDiscoveredByTag(FGameplayTag LocationTag) const
{
	int32 Count = 0;
	for (const FHSDiscoveredLocation& Location : DiscoveredLocations)
	{
		if (Location.LocationTag == LocationTag)
		{
			++Count;
		}
	}
	return Count;
}

FText UHSExplorationStateComponent::GetExplorationStatusText() const
{
	return FText::Format(
		NSLOCTEXT("HSExplorationState", "ExplorationStatus", "Discovered {0} places, low-end streaming {1}"),
		FText::AsNumber(DiscoveredLocations.Num()),
		bLowEndStreamingEnabled ? NSLOCTEXT("HSExplorationState", "StreamingOn", "on") : NSLOCTEXT("HSExplorationState", "StreamingOff", "off"));
}

void UHSExplorationStateComponent::RestoreExplorationState(const TArray<FHSDiscoveredLocation>& NewDiscoveredLocations, bool bNewLowEndStreamingEnabled)
{
	DiscoveredLocations.Reset();
	for (const FHSDiscoveredLocation& Location : NewDiscoveredLocations)
	{
		if (Location.IsValid() && !HasDiscoveredLocation(Location.LocationId))
		{
			DiscoveredLocations.Add(Location);
		}
	}
	bLowEndStreamingEnabled = bNewLowEndStreamingEnabled;
}

bool UHSExplorationStateComponent::HasDiscoveredLocation(FName LocationId) const
{
	for (const FHSDiscoveredLocation& Location : DiscoveredLocations)
	{
		if (Location.LocationId == LocationId)
		{
			return true;
		}
	}
	return false;
}
