#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Building/HSBuildingDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interaction/HSInteractable.h"
#include "Player/HSPlayerCharacter.h"
#include "Settlement/HSBuildPlacementComponent.h"
#include "Settlement/HSBuildingSchemaActor.h"
#include "Settlement/HSSettlementClaimActor.h"
#include "Settlement/HSSettlementPlanComponent.h"

namespace
{
UHSBuildingDefinition* CreateValidBasicHouseDefinition()
{
	UHSBuildingDefinition* BuildingDefinition = NewObject<UHSBuildingDefinition>();
	BuildingDefinition->DisplayName = FText::FromString(TEXT("Basic House"));
	BuildingDefinition->BuildingTag = TAG_HS_Building_BasicHouse;
	return BuildingDefinition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementPlanDefaultsTest,
	"HimalayanSettlement.Settlement.PlanDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementPlanDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSSettlementPlanComponent* SettlementPlan = NewObject<UHSSettlementPlanComponent>();

	TestNotNull(TEXT("Settlement plan component should be constructible."), SettlementPlan);
	TestFalse(TEXT("Settlement plan should not tick by default."), SettlementPlan->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Settlement plan should not start claimed."), SettlementPlan->HasSettlementClaim());
	TestEqual(TEXT("Settlement plan should start with no pending schemas."), SettlementPlan->GetPendingSchemas().Num(), 0);
	TestFalse(TEXT("Invalid building tag should not be pending."), SettlementPlan->HasPendingSchema(FGameplayTag()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementPlanClaimAndPlaceSchemaTest,
	"HimalayanSettlement.Settlement.ClaimAndPlaceHouseSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementPlanClaimAndPlaceSchemaTest::RunTest(const FString& Parameters)
{
	UHSSettlementPlanComponent* SettlementPlan = NewObject<UHSSettlementPlanComponent>();
	const UHSBuildingDefinition* BasicHouse = CreateValidBasicHouseDefinition();

	const FVector ClaimCenter(100.0f, 200.0f, 0.0f);
	const FVector HouseLocation(450.0f, 250.0f, 0.0f);

	TestTrue(TEXT("First claim should succeed."), SettlementPlan->ClaimSettlement(ClaimCenter));
	TestTrue(TEXT("Settlement should report a claim."), SettlementPlan->HasSettlementClaim());
	TestEqual(TEXT("Claim center should be stored."), SettlementPlan->GetSettlementClaimCenter(), ClaimCenter);
	TestFalse(TEXT("Second claim should be blocked."), SettlementPlan->ClaimSettlement(FVector(900.0f, 900.0f, 0.0f)));
	TestTrue(TEXT("House schema should be valid inside the claim radius."), SettlementPlan->CanPlaceSchema(BasicHouse, HouseLocation));

	FHSPendingBuildingSchema PendingSchema;
	TestTrue(TEXT("House schema should be recorded as pending construction."), SettlementPlan->PlaceSchema(BasicHouse, HouseLocation, 30.0f, PendingSchema));
	TestTrue(TEXT("Pending schema should be valid."), PendingSchema.IsValid());
	TestTrue(TEXT("Pending schema should store the house tag."), PendingSchema.BuildingTag == TAG_HS_Building_BasicHouse);
	TestEqual(TEXT("Pending schema should store placement location."), PendingSchema.Location, HouseLocation);
	TestEqual(TEXT("Pending schema should store placement yaw."), PendingSchema.YawDegrees, 30.0f);
	TestEqual(TEXT("Exactly one pending schema should be stored."), SettlementPlan->GetPendingSchemas().Num(), 1);
	TestTrue(TEXT("House schema should be queryable as pending."), SettlementPlan->HasPendingSchema(TAG_HS_Building_BasicHouse));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementPlanBlocksInvalidPlacementTest,
	"HimalayanSettlement.Settlement.BlocksInvalidHouseSchemaPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementPlanBlocksInvalidPlacementTest::RunTest(const FString& Parameters)
{
	UHSSettlementPlanComponent* SettlementPlan = NewObject<UHSSettlementPlanComponent>();
	const UHSBuildingDefinition* BasicHouse = CreateValidBasicHouseDefinition();

	FHSPendingBuildingSchema PendingSchema;
	TestFalse(TEXT("Schema placement should be blocked before a settlement claim."), SettlementPlan->PlaceSchema(BasicHouse, FVector::ZeroVector, 0.0f, PendingSchema));

	TestTrue(TEXT("Claim should succeed."), SettlementPlan->ClaimSettlement(FVector::ZeroVector));
	TestFalse(TEXT("Null building definition should be blocked."), SettlementPlan->CanPlaceSchema(nullptr, FVector::ZeroVector));
	TestFalse(TEXT("Far outside placement should be blocked."), SettlementPlan->CanPlaceSchema(BasicHouse, FVector(SettlementPlan->GetClaimRadius() + 500.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("First valid house schema should place."), SettlementPlan->PlaceSchema(BasicHouse, FVector(250.0f, 0.0f, 0.0f), 0.0f, PendingSchema));
	TestFalse(TEXT("Duplicate house schema should be blocked while pending."), SettlementPlan->CanPlaceSchema(BasicHouse, FVector(350.0f, 0.0f, 0.0f)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSFreeBuildPlacementRecordsHouseSchemaTest,
	"HimalayanSettlement.Settlement.FreeBuildPlacementRecordsHouseSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSFreeBuildPlacementRecordsHouseSchemaTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSPlayerCharacter* Player = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	UHSBuildingDefinition* BasicHouse = CreateValidBasicHouseDefinition();
	UHSSettlementPlanComponent* SettlementPlan = Player ? Player->GetSettlementPlanComponent() : nullptr;
	UHSBuildPlacementComponent* BuildPlacement = Player ? Player->GetBuildPlacementComponent() : nullptr;

	TestNotNull(TEXT("Player should spawn."), Player);
	TestNotNull(TEXT("Player should own settlement plan component."), SettlementPlan);
	TestNotNull(TEXT("Player should own free-build placement component."), BuildPlacement);
	if (!Player || !SettlementPlan || !BuildPlacement)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}

	TestFalse(TEXT("Free-build placement should start inactive."), BuildPlacement->IsPlacementActive());
	TestFalse(TEXT("Free-build placement component should not tick while inactive."), BuildPlacement->IsComponentTickEnabled());
	TestTrue(TEXT("Settlement claim should succeed."), SettlementPlan->ClaimSettlement(FVector::ZeroVector));
	TestTrue(TEXT("Free-build placement should start for basic house."), BuildPlacement->StartPlacement(BasicHouse));
	TestTrue(TEXT("Free-build placement should become active."), BuildPlacement->IsPlacementActive());
	TestTrue(TEXT("Free-build placement component should tick only while active."), BuildPlacement->IsComponentTickEnabled());
	TestTrue(TEXT("House placement should be confirmable inside claim."), BuildPlacement->CanConfirmPlacement());

	const float InitialYaw = BuildPlacement->GetPreviewYawDegrees();
	BuildPlacement->RotatePlacement(1.0f);
	TestNotEqual(TEXT("Rotating placement should change yaw."), BuildPlacement->GetPreviewYawDegrees(), InitialYaw);

	TestTrue(TEXT("Confirming free-build placement should record a pending schema."), BuildPlacement->ConfirmPlacement());
	TestFalse(TEXT("Free-build placement should deactivate after confirm."), BuildPlacement->IsPlacementActive());
	TestFalse(TEXT("Free-build placement component should stop ticking after confirm."), BuildPlacement->IsComponentTickEnabled());
	TestTrue(TEXT("House should be pending after free-build confirmation."), SettlementPlan->HasPendingSchema(TAG_HS_Building_BasicHouse));
	TestEqual(TEXT("Exactly one pending schema should be stored."), SettlementPlan->GetPendingSchemas().Num(), 1);

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSFreeBuildPlacementBlocksBeforeSettlementClaimTest,
	"HimalayanSettlement.Settlement.FreeBuildPlacementBlocksBeforeClaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSFreeBuildPlacementBlocksBeforeSettlementClaimTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSPlayerCharacter* Player = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	UHSBuildingDefinition* BasicHouse = CreateValidBasicHouseDefinition();
	UHSBuildPlacementComponent* BuildPlacement = Player ? Player->GetBuildPlacementComponent() : nullptr;

	TestNotNull(TEXT("Player should spawn."), Player);
	TestNotNull(TEXT("Player should own free-build placement component."), BuildPlacement);
	if (!Player || !BuildPlacement)
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}

	TestTrue(TEXT("Free-build placement can preview before claim."), BuildPlacement->StartPlacement(BasicHouse));
	TestFalse(TEXT("House placement should not be confirmable before claim."), BuildPlacement->CanConfirmPlacement());
	TestFalse(TEXT("Confirming before claim should be blocked."), BuildPlacement->ConfirmPlacement());
	TestTrue(TEXT("Blocked placement should remain active for adjustment."), BuildPlacement->IsPlacementActive());
	BuildPlacement->CancelPlacement();
	TestFalse(TEXT("Cancel should stop blocked placement."), BuildPlacement->IsPlacementActive());

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementPlanActorInteractionsTest,
	"HimalayanSettlement.Settlement.ClaimAndSchemaActorsRecordIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementPlanActorInteractionsTest::RunTest(const FString& Parameters)
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
	AHSSettlementClaimActor* ClaimActor = World->SpawnActor<AHSSettlementClaimActor>(FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	AHSBuildingSchemaActor* SchemaActor = World->SpawnActor<AHSBuildingSchemaActor>(FVector(500.0f, 0.0f, 0.0f), FRotator(0.0f, 45.0f, 0.0f), SpawnParameters);
	UHSBuildingDefinition* BasicHouse = CreateValidBasicHouseDefinition();
	SchemaActor->SetBuildingDefinition(BasicHouse);

	UHSSettlementPlanComponent* SettlementPlan = Player ? Player->GetSettlementPlanComponent() : nullptr;
	TestNotNull(TEXT("Player should spawn in the transient test world."), Player);
	TestNotNull(TEXT("Player should own settlement plan component."), SettlementPlan);
	TestNotNull(TEXT("Claim actor should spawn."), ClaimActor);
	TestNotNull(TEXT("Schema actor should spawn."), SchemaActor);
	if (!Player || !SettlementPlan || !ClaimActor || !SchemaActor)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("Claim actor should be interactable before claim."), HSInteraction::CanInteract(ClaimActor, Player));
	HSInteraction::Interact(ClaimActor, Player);
	TestTrue(TEXT("Interacting with claim actor should claim settlement."), SettlementPlan->HasSettlementClaim());
	TestTrue(TEXT("Claim actor should mark itself claimed."), ClaimActor->IsClaimed());

	TestTrue(TEXT("Schema actor should be interactable after claim."), HSInteraction::CanInteract(SchemaActor, Player));
	HSInteraction::Interact(SchemaActor, Player);
	TestTrue(TEXT("Schema actor should mark itself placed."), SchemaActor->IsSchemaPlaced());
	TestTrue(TEXT("House should be pending after schema interaction."), SettlementPlan->HasPendingSchema(TAG_HS_Building_BasicHouse));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasSettlementPlanComponentTest,
	"HimalayanSettlement.Settlement.PlayerHasNativePlanComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasSettlementPlanComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own the native settlement plan component."), Player ? Player->GetSettlementPlanComponent() : nullptr);

	return true;
}

#endif
