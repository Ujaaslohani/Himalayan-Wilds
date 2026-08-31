#include "Settlement/HSSettlementPopulationComponent.h"

#include "Core/HSGameplayTags.h"

UHSSettlementPopulationComponent::UHSSettlementPopulationComponent()
	: HouseCapacity(0)
	, VillageExpansionDay(0)
	, AccumulatedExpansionDays(0.0f)
	, bDefenseSystemEnabled(false)
	, bRaidSystemEnabled(false)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSSettlementPopulationComponent::RecruitNPC(FName NPCId, FGameplayTag ProfessionTag)
{
	if (NPCId.IsNone() || !ProfessionTag.IsValid() || FindAssignment(NPCId))
	{
		return false;
	}

	FHSNPCSettlementAssignment Assignment;
	Assignment.NPCId = NPCId;
	Assignment.ProfessionTag = ProfessionTag;
	Assignments.Add(Assignment);
	return true;
}

bool UHSSettlementPopulationComponent::AssignNPCToBuilding(FName NPCId, FGameplayTag BuildingTag)
{
	FHSNPCSettlementAssignment* Assignment = FindAssignment(NPCId);
	if (!Assignment || !BuildingTag.IsValid())
	{
		return false;
	}

	Assignment->AssignedBuildingTag = BuildingTag;
	return true;
}

void UHSSettlementPopulationComponent::RefreshFromCompletedBuildings(const FGameplayTagContainer& CompletedBuildings)
{
	HouseCapacity = CalculateHouseCapacity(CompletedBuildings);
	bDefenseSystemEnabled = bDefenseSystemEnabled || CompletedBuildings.HasTagExact(TAG_HS_Building_WatchPost);
}

void UHSSettlementPopulationComponent::AdvanceSettlementDay(float DeltaDays)
{
	if (DeltaDays <= 0.0f)
	{
		return;
	}

	AccumulatedExpansionDays += DeltaDays;
	while (AccumulatedExpansionDays >= 1.0f)
	{
		AccumulatedExpansionDays -= 1.0f;
		++VillageExpansionDay;
	}
}

void UHSSettlementPopulationComponent::SetDefenseSystemEnabled(bool bNewEnabled)
{
	bDefenseSystemEnabled = bNewEnabled;
	bRaidSystemEnabled = false;
}

FText UHSSettlementPopulationComponent::GetSettlementStatusText() const
{
	return FText::Format(
		NSLOCTEXT("HSSettlementPopulation", "SettlementStatus", "Population {0}/{1}, expansion day {2}, defense {3}"),
		FText::AsNumber(GetPopulation()),
		FText::AsNumber(HouseCapacity),
		FText::AsNumber(VillageExpansionDay),
		bDefenseSystemEnabled ? NSLOCTEXT("HSSettlementPopulation", "DefenseOn", "on") : NSLOCTEXT("HSSettlementPopulation", "DefenseOff", "off"));
}

bool UHSSettlementPopulationComponent::CanSupportBuilding(FGameplayTag BuildingTag, const FGameplayTagContainer& CompletedBuildings, FText* OutReason) const
{
	if (BuildingTag == TAG_HS_Building_BasicHouse)
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSSettlementPopulation", "HouseAllowed", "Basic House is the first settlement dependency.");
		}
		return true;
	}

	if (!CompletedBuildings.HasTagExact(TAG_HS_Building_BasicHouse))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSSettlementPopulation", "NeedsHouse", "Complete the Basic House before expansion buildings.");
		}
		return false;
	}

	if (BuildingTag == TAG_HS_Building_Workshop && !CompletedBuildings.HasTagExact(TAG_HS_Building_StorageHut))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSSettlementPopulation", "WorkshopNeedsStorage", "Complete Storage Hut before Workshop.");
		}
		return false;
	}

	if (OutReason)
	{
		*OutReason = NSLOCTEXT("HSSettlementPopulation", "DependencyAllowed", "Building dependencies met.");
	}
	return true;
}

void UHSSettlementPopulationComponent::RestoreSettlementPopulation(const TArray<FHSNPCSettlementAssignment>& NewAssignments, int32 NewHouseCapacity, int32 NewVillageExpansionDay, bool bNewDefenseSystemEnabled, bool bNewRaidSystemEnabled)
{
	Assignments.Reset();
	for (const FHSNPCSettlementAssignment& Assignment : NewAssignments)
	{
		if (Assignment.IsValid() && !FindAssignment(Assignment.NPCId))
		{
			Assignments.Add(Assignment);
		}
	}

	HouseCapacity = FMath::Max(0, NewHouseCapacity);
	VillageExpansionDay = FMath::Max(0, NewVillageExpansionDay);
	bDefenseSystemEnabled = bNewDefenseSystemEnabled;
	bRaidSystemEnabled = bNewRaidSystemEnabled && bDefenseSystemEnabled;
	AccumulatedExpansionDays = 0.0f;
}

FHSNPCSettlementAssignment* UHSSettlementPopulationComponent::FindAssignment(FName NPCId)
{
	for (FHSNPCSettlementAssignment& Assignment : Assignments)
	{
		if (Assignment.NPCId == NPCId)
		{
			return &Assignment;
		}
	}
	return nullptr;
}

const FHSNPCSettlementAssignment* UHSSettlementPopulationComponent::FindAssignment(FName NPCId) const
{
	for (const FHSNPCSettlementAssignment& Assignment : Assignments)
	{
		if (Assignment.NPCId == NPCId)
		{
			return &Assignment;
		}
	}
	return nullptr;
}

int32 UHSSettlementPopulationComponent::CalculateHouseCapacity(const FGameplayTagContainer& CompletedBuildings) const
{
	int32 Capacity = 0;
	if (CompletedBuildings.HasTagExact(TAG_HS_Building_BasicHouse))
	{
		Capacity = 2;
	}
	if (CompletedBuildings.HasTagExact(TAG_HS_Building_BasicHouseMinorUpgrade))
	{
		Capacity += 1;
	}
	if (CompletedBuildings.HasTagExact(TAG_HS_Building_BasicHouseMidUpgrade))
	{
		Capacity += 2;
	}
	if (CompletedBuildings.HasTagExact(TAG_HS_Building_BasicHouseMajorUpgrade))
	{
		Capacity += 3;
	}
	return Capacity;
}
