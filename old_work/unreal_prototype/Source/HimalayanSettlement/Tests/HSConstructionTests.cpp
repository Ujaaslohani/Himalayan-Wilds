#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Building/HSBuildingDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "Player/HSPlayerCharacter.h"
#include "Settlement/HSCompletedBuildingActor.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

namespace
{
UHSBuildingDefinition* CreateConstructionTestHouseDefinition()
{
	UHSBuildingDefinition* BuildingDefinition = NewObject<UHSBuildingDefinition>();
	BuildingDefinition->DisplayName = FText::FromString(TEXT("Basic House"));
	BuildingDefinition->BuildingTag = TAG_HS_Building_BasicHouse;
	return BuildingDefinition;
}

UHSBuildingDefinition* CreateConstructionTestMinorUpgradeDefinition()
{
	UHSBuildingDefinition* BuildingDefinition = NewObject<UHSBuildingDefinition>();
	BuildingDefinition->DisplayName = FText::FromString(TEXT("Minor House Upgrade"));
	BuildingDefinition->BuildingTag = TAG_HS_Building_BasicHouseMinorUpgrade;
	return BuildingDefinition;
}

UHSBuildingDefinition* CreateConstructionTestDefinition(FGameplayTag BuildingTag, const TCHAR* DisplayName)
{
	UHSBuildingDefinition* BuildingDefinition = NewObject<UHSBuildingDefinition>();
	BuildingDefinition->DisplayName = FText::FromString(DisplayName);
	BuildingDefinition->BuildingTag = BuildingTag;
	return BuildingDefinition;
}

bool PlacePendingHouseSchema(UHSSettlementPlanComponent& SettlementPlan)
{
	UHSBuildingDefinition* BuildingDefinition = CreateConstructionTestHouseDefinition();
	FHSPendingBuildingSchema PendingSchema;
	return SettlementPlan.ClaimSettlement(FVector::ZeroVector)
		&& SettlementPlan.PlaceSchema(BuildingDefinition, FVector(250.0f, 0.0f, 0.0f), 0.0f, PendingSchema);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSConstructionDefaultsTest,
	"HimalayanSettlement.Construction.DefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSConstructionDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSConstructionComponent* Construction = NewObject<UHSConstructionComponent>();

	TestNotNull(TEXT("Construction component should be constructible."), Construction);
	TestFalse(TEXT("Construction component should not tick by default."), Construction->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Construction component should not start with an active job."), Construction->HasActiveJob());
	TestEqual(TEXT("Construction component should start with no jobs."), Construction->GetConstructionJobs().Num(), 0);
	TestEqual(TEXT("Basic house construction should take one day at first."), Construction->GetHouseConstructionDays(), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSCompletedBuildingVisualDefaultsTest,
	"HimalayanSettlement.Construction.CompletedBuildingVisualDefaultsAreCheap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSCompletedBuildingVisualDefaultsTest::RunTest(const FString& Parameters)
{
	AHSCompletedBuildingActor* CompletedBuilding = NewObject<AHSCompletedBuildingActor>();

	TestNotNull(TEXT("Completed building actor should be constructible."), CompletedBuilding);
	TestFalse(TEXT("Completed building actor should not tick."), CompletedBuilding ? CompletedBuilding->PrimaryActorTick.bCanEverTick : true);
	TestNotNull(TEXT("Completed building should own a wall mesh."), CompletedBuilding ? CompletedBuilding->GetWallMesh() : nullptr);
	TestNotNull(TEXT("Completed building should own a roof mesh."), CompletedBuilding ? CompletedBuilding->GetRoofMesh() : nullptr);
	TestNotNull(TEXT("Completed building should own a door mesh."), CompletedBuilding ? CompletedBuilding->GetDoorMesh() : nullptr);
	TestNotNull(TEXT("Completed building should own a chimney mesh."), CompletedBuilding ? CompletedBuilding->GetChimneyMesh() : nullptr);
	TestEqual(TEXT("Wall mesh should not use collision yet."), CompletedBuilding && CompletedBuilding->GetWallMesh() ? CompletedBuilding->GetWallMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Roof mesh should not use collision yet."), CompletedBuilding && CompletedBuilding->GetRoofMesh() ? CompletedBuilding->GetRoofMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Door mesh should not use collision yet."), CompletedBuilding && CompletedBuilding->GetDoorMesh() ? CompletedBuilding->GetDoorMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Chimney mesh should not use collision yet."), CompletedBuilding && CompletedBuilding->GetChimneyMesh() ? CompletedBuilding->GetChimneyMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);
	const float WallBottomZ = CompletedBuilding && CompletedBuilding->GetWallMesh()
		? static_cast<float>(CompletedBuilding->GetWallMesh()->GetRelativeLocation().Z - (CompletedBuilding->GetWallMesh()->GetRelativeScale3D().Z * 50.0))
		: -1.0f;
	const float DoorBottomZ = CompletedBuilding && CompletedBuilding->GetDoorMesh()
		? static_cast<float>(CompletedBuilding->GetDoorMesh()->GetRelativeLocation().Z - (CompletedBuilding->GetDoorMesh()->GetRelativeScale3D().Z * 50.0))
		: -1.0f;
	TestEqual(TEXT("Wall bottom should sit on the actor root plane."), WallBottomZ, 0.0f);
	TestEqual(TEXT("Door bottom should sit on the actor root plane."), DoorBottomZ, 0.0f);

	CompletedBuilding->ConfigureCompletedBuilding(TAG_HS_Building_BasicHouse);
	TestTrue(TEXT("Completed building should store configured tag."), CompletedBuilding->GetBuildingTag() == TAG_HS_Building_BasicHouse);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSConstructionStartsFromPendingSchemaTest,
	"HimalayanSettlement.Construction.BuilderStartsPendingHouseSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSConstructionStartsFromPendingSchemaTest::RunTest(const FString& Parameters)
{
	UHSSettlementPlanComponent* SettlementPlan = NewObject<UHSSettlementPlanComponent>();
	UHSConstructionComponent* Construction = NewObject<UHSConstructionComponent>();

	TestTrue(TEXT("Pending house schema should be placed for construction test."), PlacePendingHouseSchema(*SettlementPlan));
	TestTrue(TEXT("Construction should start from first pending schema."), Construction->StartNextPendingSchema(SettlementPlan));
	TestTrue(TEXT("Construction should now have an active job."), Construction->HasActiveJob());
	TestFalse(TEXT("Pending house schema should be consumed when construction starts."), SettlementPlan->HasPendingSchema(TAG_HS_Building_BasicHouse));
	TestEqual(TEXT("Exactly one construction job should be recorded."), Construction->GetConstructionJobs().Num(), 1);
	TestTrue(TEXT("Active job should be for the basic house."), Construction->GetConstructionJobs()[0].BuildingTag == TAG_HS_Building_BasicHouse);
	TestEqual(TEXT("Basic house job should require one day."), Construction->GetConstructionJobs()[0].RequiredDays, 1.0f);
	TestFalse(TEXT("A second active job should be blocked."), Construction->StartNextPendingSchema(SettlementPlan));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSConstructionMinorUpgradeTakesQuarterDayTest,
	"HimalayanSettlement.Construction.MinorHouseUpgradeTakesQuarterDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSConstructionMinorUpgradeTakesQuarterDayTest::RunTest(const FString& Parameters)
{
	UHSSettlementPlanComponent* SettlementPlan = NewObject<UHSSettlementPlanComponent>();
	UHSConstructionComponent* Construction = NewObject<UHSConstructionComponent>();
	UHSBuildingDefinition* MinorUpgradeDefinition = CreateConstructionTestMinorUpgradeDefinition();

	FHSPendingBuildingSchema PendingSchema;
	TestTrue(TEXT("Settlement should be claimed."), SettlementPlan->ClaimSettlement(FVector::ZeroVector));
	TestTrue(TEXT("Minor upgrade schema should be placed."), SettlementPlan->PlaceSchema(MinorUpgradeDefinition, FVector(220.0f, 120.0f, 0.0f), 0.0f, PendingSchema));
	TestTrue(TEXT("Construction should start from minor upgrade schema."), Construction->StartNextPendingSchema(SettlementPlan));
	TestEqual(TEXT("Minor upgrade should require a quarter day."), Construction->GetConstructionJobs()[0].RequiredDays, 0.25f);
	TestTrue(TEXT("Quarter-day progress should complete the minor upgrade."), Construction->AdvanceConstruction(0.25f));
	TestTrue(TEXT("Minor upgrade should be marked complete."), Construction->HasCompletedBuilding(TAG_HS_Building_BasicHouseMinorUpgrade));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSConstructionCoreSchemaDurationsTest,
	"HimalayanSettlement.Construction.CoreSchemaDurationsMatchBuilderRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSConstructionCoreSchemaDurationsTest::RunTest(const FString& Parameters)
{
	struct FDurationExpectation
	{
		FGameplayTag BuildingTag;
		const TCHAR* DisplayName;
		float ExpectedDays;
	};

	const FDurationExpectation Expectations[] = {
		{ TAG_HS_Building_BasicHouseMidUpgrade, TEXT("Mid House Upgrade"), 0.5f },
		{ TAG_HS_Building_BasicHouseMajorUpgrade, TEXT("Major House Upgrade"), 1.0f },
		{ TAG_HS_Building_StorageHut, TEXT("Storage Hut"), 0.5f },
		{ TAG_HS_Building_Workshop, TEXT("Workshop"), 1.0f },
		{ TAG_HS_Building_WatchPost, TEXT("Watch Post"), 0.5f },
		{ TAG_HS_Building_HerbalHut, TEXT("Herbal Hut"), 0.5f },
		{ TAG_HS_Building_BasicDryingRack, TEXT("Drying Rack"), 0.25f }
	};

	for (const FDurationExpectation& Expectation : Expectations)
	{
		UHSSettlementPlanComponent* SettlementPlan = NewObject<UHSSettlementPlanComponent>();
		UHSConstructionComponent* Construction = NewObject<UHSConstructionComponent>();
		UHSBuildingDefinition* BuildingDefinition = CreateConstructionTestDefinition(Expectation.BuildingTag, Expectation.DisplayName);

		FHSPendingBuildingSchema PendingSchema;
		TestTrue(FString::Printf(TEXT("%s test settlement should be claimed."), Expectation.DisplayName), SettlementPlan->ClaimSettlement(FVector::ZeroVector));
		TestTrue(FString::Printf(TEXT("%s schema should be placed."), Expectation.DisplayName), SettlementPlan->PlaceSchema(BuildingDefinition, FVector(300.0f, 0.0f, 0.0f), 0.0f, PendingSchema));
		TestTrue(FString::Printf(TEXT("%s construction should start."), Expectation.DisplayName), Construction->StartNextPendingSchema(SettlementPlan));
		TestEqual(FString::Printf(TEXT("%s duration should match builder rule."), Expectation.DisplayName), Construction->GetConstructionJobs()[0].RequiredDays, Expectation.ExpectedDays);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSConstructionProgressCompletesAfterOneDayTest,
	"HimalayanSettlement.Construction.HouseCompletesAfterOneDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSConstructionProgressCompletesAfterOneDayTest::RunTest(const FString& Parameters)
{
	UHSSettlementPlanComponent* SettlementPlan = NewObject<UHSSettlementPlanComponent>();
	UHSConstructionComponent* Construction = NewObject<UHSConstructionComponent>();

	TestTrue(TEXT("Pending house schema should be placed."), PlacePendingHouseSchema(*SettlementPlan));
	TestTrue(TEXT("Construction should start."), Construction->StartNextPendingSchema(SettlementPlan));
	TestTrue(TEXT("First half-day should progress construction."), Construction->AdvanceConstruction(0.5f));
	TestTrue(TEXT("Job should still be active after half a day."), Construction->HasActiveJob());
	TestEqual(TEXT("Progress should be half a day."), Construction->GetConstructionJobs()[0].ProgressDays, 0.5f);

	TestTrue(TEXT("Second half-day should complete construction."), Construction->AdvanceConstruction(0.5f));
	TestFalse(TEXT("No active job should remain after completion."), Construction->HasActiveJob());
	TestTrue(TEXT("Basic house should be marked completed."), Construction->HasCompletedBuilding(TAG_HS_Building_BasicHouse));
	TestTrue(TEXT("Recorded job should be complete."), Construction->GetConstructionJobs()[0].IsComplete());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSConstructionWorldTimeAdvancesActiveJobTest,
	"HimalayanSettlement.Construction.WorldTimeAdvancesActiveJob",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSConstructionWorldTimeAdvancesActiveJobTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSGameState* GameState = World->SpawnActor<AHSGameState>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	World->SetGameState(GameState);
	AHSPlayerCharacter* Player = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);

	UHSSettlementPlanComponent* SettlementPlan = Player ? Player->GetSettlementPlanComponent() : nullptr;
	UHSConstructionComponent* Construction = Player ? Player->GetConstructionComponent() : nullptr;
	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;

	TestNotNull(TEXT("Game state should spawn."), GameState);
	TestNotNull(TEXT("Player should spawn."), Player);
	TestNotNull(TEXT("Player should own settlement plan."), SettlementPlan);
	TestNotNull(TEXT("Player should own construction component."), Construction);
	TestNotNull(TEXT("Game state should own world time."), WorldTime);
	if (!GameState || !Player || !SettlementPlan || !Construction || !WorldTime)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("Pending house schema should be placed."), PlacePendingHouseSchema(*SettlementPlan));
	TestTrue(TEXT("Construction should start."), Construction->StartNextPendingSchema(SettlementPlan));
	Construction->BindToWorldTime();

	WorldTime->AdvanceHalfDay();
	TestEqual(TEXT("One time advance should progress half a day."), Construction->GetConstructionJobs()[0].ProgressDays, 0.5f);
	WorldTime->AdvanceHalfDay();
	TestTrue(TEXT("Second time advance should complete the house."), Construction->HasCompletedBuilding(TAG_HS_Building_BasicHouse));
	TestEqual(TEXT("Completing construction should spawn one completed building visual."), Construction->GetSpawnedCompletedBuildingActors().Num(), 1);
	const AHSCompletedBuildingActor* CompletedBuilding = Construction->GetSpawnedCompletedBuildingActors().Num() > 0 ? Construction->GetSpawnedCompletedBuildingActors()[0].Get() : nullptr;
	TestNotNull(TEXT("Completed building visual should be valid."), CompletedBuilding);
	TestTrue(TEXT("Completed building visual should store the house tag."), CompletedBuilding && CompletedBuilding->GetBuildingTag() == TAG_HS_Building_BasicHouse);
	TestEqual(TEXT("Completed building visual should spawn at schema location."), CompletedBuilding ? CompletedBuilding->GetActorLocation() : FVector::ZeroVector, FVector(250.0f, 0.0f, 0.0f));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSBuilderNPCStartsConstructionTest,
	"HimalayanSettlement.Construction.BuilderNPCStartsConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSBuilderNPCStartsConstructionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSPlayerCharacter* Player = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	AHSNPCActor* Builder = World->SpawnActor<AHSNPCActor>(FVector(150.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	UHSNPCDefinition* BuilderDefinition = NewObject<UHSNPCDefinition>();
	BuilderDefinition->DisplayName = FText::FromString(TEXT("Builder"));
	BuilderDefinition->ProfessionTag = TAG_HS_NPC_Profession_Builder;
	Builder->SetNPCDefinition(BuilderDefinition);

	UHSSettlementPlanComponent* SettlementPlan = Player ? Player->GetSettlementPlanComponent() : nullptr;
	UHSConstructionComponent* Construction = Player ? Player->GetConstructionComponent() : nullptr;
	TestNotNull(TEXT("Player should own settlement plan."), SettlementPlan);
	TestNotNull(TEXT("Player should own construction component."), Construction);
	if (!Player || !Builder || !SettlementPlan || !Construction)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("Pending house schema should be placed."), PlacePendingHouseSchema(*SettlementPlan));
	Builder->Interact_Implementation(Player);
	TestTrue(TEXT("Builder interaction should start construction."), Construction->HasActiveJob());

	World->DestroyWorld(false);
	return true;
}

#endif
