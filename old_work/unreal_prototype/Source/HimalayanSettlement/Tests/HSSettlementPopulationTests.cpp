#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Settlement/HSSettlementPopulationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementPopulationDefaultsTest,
	"HimalayanSettlement.Settlement.PopulationDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementPopulationDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSSettlementPopulationComponent* Population = NewObject<UHSSettlementPopulationComponent>();

	TestNotNull(TEXT("Population component should be constructible."), Population);
	TestFalse(TEXT("Population component should not tick."), Population->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("Population should start empty."), Population->GetPopulation(), 0);
	TestEqual(TEXT("House capacity should start empty."), Population->GetHouseCapacity(), 0);
	TestFalse(TEXT("Defense should start disabled."), Population->IsDefenseSystemEnabled());
	TestFalse(TEXT("Raid system should stay disabled for the current approved scope."), Population->IsRaidSystemEnabled());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementPopulationAssignmentsAndCapacityTest,
	"HimalayanSettlement.Settlement.PopulationAssignmentsAndHouseCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementPopulationAssignmentsAndCapacityTest::RunTest(const FString& Parameters)
{
	UHSSettlementPopulationComponent* Population = NewObject<UHSSettlementPopulationComponent>();
	FGameplayTagContainer CompletedBuildings;

	TestTrue(TEXT("Recruiting a hunter should succeed."), Population->RecruitNPC(TEXT("Hunter_01"), TAG_HS_NPC_Profession_Hunter));
	TestFalse(TEXT("Recruiting the same NPC twice should fail."), Population->RecruitNPC(TEXT("Hunter_01"), TAG_HS_NPC_Profession_Hunter));
	TestTrue(TEXT("Assigning recruited NPC to house should succeed."), Population->AssignNPCToBuilding(TEXT("Hunter_01"), TAG_HS_Building_BasicHouse));
	TestEqual(TEXT("Population should contain one assignment."), Population->GetPopulation(), 1);
	TestTrue(TEXT("Assigned building should be stored."), Population->GetAssignments()[0].AssignedBuildingTag == TAG_HS_Building_BasicHouse);

	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouse);
	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouseMinorUpgrade);
	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouseMidUpgrade);
	Population->RefreshFromCompletedBuildings(CompletedBuildings);
	TestEqual(TEXT("House capacity should include base, minor, and mid upgrades."), Population->GetHouseCapacity(), 5);

	Population->AdvanceSettlementDay(0.5f);
	TestEqual(TEXT("Half day should not advance village expansion day."), Population->GetVillageExpansionDay(), 0);
	Population->AdvanceSettlementDay(0.5f);
	TestEqual(TEXT("One full day should advance village expansion day."), Population->GetVillageExpansionDay(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementPopulationBuildingDependenciesTest,
	"HimalayanSettlement.Settlement.PopulationBuildingDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementPopulationBuildingDependenciesTest::RunTest(const FString& Parameters)
{
	UHSSettlementPopulationComponent* Population = NewObject<UHSSettlementPopulationComponent>();
	FGameplayTagContainer CompletedBuildings;
	FText Reason;

	TestTrue(TEXT("Basic house should be allowed as the first dependency."), Population->CanSupportBuilding(TAG_HS_Building_BasicHouse, CompletedBuildings, &Reason));
	TestFalse(TEXT("Workshop should require a basic house first."), Population->CanSupportBuilding(TAG_HS_Building_Workshop, CompletedBuildings, &Reason));

	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouse);
	TestFalse(TEXT("Workshop should require storage hut after house."), Population->CanSupportBuilding(TAG_HS_Building_Workshop, CompletedBuildings, &Reason));

	CompletedBuildings.AddTag(TAG_HS_Building_StorageHut);
	TestTrue(TEXT("Workshop should be allowed after storage hut."), Population->CanSupportBuilding(TAG_HS_Building_Workshop, CompletedBuildings, &Reason));

	CompletedBuildings.AddTag(TAG_HS_Building_WatchPost);
	Population->RefreshFromCompletedBuildings(CompletedBuildings);
	TestTrue(TEXT("Watch post should enable defense foundation."), Population->IsDefenseSystemEnabled());
	TestFalse(TEXT("Raid system should remain disabled until separately approved."), Population->IsRaidSystemEnabled());

	return true;
}

#endif
