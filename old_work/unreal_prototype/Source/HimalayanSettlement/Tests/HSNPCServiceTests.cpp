#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Building/HSBuildingDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "NPC/HSNPCServiceComponent.h"
#include "Player/HSPlayerCharacter.h"
#include "Player/HSPlayerHealthComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

namespace
{
UHSNPCDefinition* CreateServiceTestNPCDefinition(UObject* Outer, FGameplayTag ProfessionTag, const TCHAR* DisplayName)
{
	UHSNPCDefinition* NPCDefinition = NewObject<UHSNPCDefinition>(Outer);
	NPCDefinition->DisplayName = FText::FromString(DisplayName);
	NPCDefinition->ProfessionTag = ProfessionTag;
	return NPCDefinition;
}

UHSBuildingDefinition* CreateServiceTestHouseDefinition(UObject* Outer)
{
	UHSBuildingDefinition* BuildingDefinition = NewObject<UHSBuildingDefinition>(Outer);
	BuildingDefinition->DisplayName = FText::FromString(TEXT("Basic House"));
	BuildingDefinition->BuildingTag = TAG_HS_Building_BasicHouse;
	return BuildingDefinition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCServiceDefaultsTest,
	"HimalayanSettlement.NPCServices.DefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCServiceDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSNPCServiceComponent* ServiceComponent = NewObject<UHSNPCServiceComponent>();
	const AHSNPCActor* NPC = GetDefault<AHSNPCActor>();

	TestNotNull(TEXT("NPC service component should be constructible."), ServiceComponent);
	TestFalse(TEXT("NPC service component should not tick."), ServiceComponent->PrimaryComponentTick.bCanEverTick);
	TestNotNull(TEXT("NPC actor should own native service component."), NPC ? NPC->GetNPCServiceComponent() : nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCServiceBuilderStartsConstructionTest,
	"HimalayanSettlement.NPCServices.BuilderServiceStartsConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCServiceBuilderStartsConstructionTest::RunTest(const FString& Parameters)
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
	AHSNPCActor* Builder = World->SpawnActor<AHSNPCActor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	Builder->SetNPCDefinition(CreateServiceTestNPCDefinition(Builder, TAG_HS_NPC_Profession_Builder, TEXT("Builder")));

	UHSSettlementPlanComponent* SettlementPlan = Player ? Player->GetSettlementPlanComponent() : nullptr;
	UHSConstructionComponent* Construction = Player ? Player->GetConstructionComponent() : nullptr;
	UHSNPCServiceComponent* ServiceComponent = Builder ? Builder->GetNPCServiceComponent() : nullptr;
	UHSBuildingDefinition* HouseDefinition = CreateServiceTestHouseDefinition(Player);

	TestNotNull(TEXT("Builder should own service component."), ServiceComponent);
	if (!Player || !SettlementPlan || !Construction || !ServiceComponent || !HouseDefinition)
	{
		World->DestroyWorld(false);
		return false;
	}

	FHSPendingBuildingSchema PendingSchema;
	TestTrue(TEXT("Settlement should be claimed."), SettlementPlan->ClaimSettlement(FVector::ZeroVector));
	TestTrue(TEXT("House schema should be pending."), SettlementPlan->PlaceSchema(HouseDefinition, FVector(250.0f, 0.0f, 0.0f), 15.0f, PendingSchema));

	FHSNPCServiceOption ServiceOption;
	TestTrue(TEXT("Builder service should be queryable."), ServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Builder_StartConstruction, Player, ServiceOption));
	TestTrue(TEXT("Builder service should be available with a pending schema."), ServiceOption.bAvailable);
	TestTrue(TEXT("Executing builder service should start construction."), ServiceComponent->TryExecuteService(TAG_HS_NPC_Service_Builder_StartConstruction, Player));
	TestTrue(TEXT("Construction should now have an active job."), Construction->HasActiveJob());
	TestFalse(TEXT("Pending schema should be consumed by builder service."), SettlementPlan->HasPendingSchema(TAG_HS_Building_BasicHouse));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCServiceBuilderPlansMinorUpgradeTest,
	"HimalayanSettlement.NPCServices.BuilderPlansMinorHouseUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCServiceBuilderPlansMinorUpgradeTest::RunTest(const FString& Parameters)
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
	AHSNPCActor* Builder = World->SpawnActor<AHSNPCActor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	Builder->SetNPCDefinition(CreateServiceTestNPCDefinition(Builder, TAG_HS_NPC_Profession_Builder, TEXT("Builder")));

	UHSSettlementPlanComponent* SettlementPlan = Player ? Player->GetSettlementPlanComponent() : nullptr;
	UHSConstructionComponent* Construction = Player ? Player->GetConstructionComponent() : nullptr;
	UHSNPCServiceComponent* ServiceComponent = Builder ? Builder->GetNPCServiceComponent() : nullptr;
	TestNotNull(TEXT("Builder should own service component."), ServiceComponent);
	if (!Player || !SettlementPlan || !Construction || !ServiceComponent)
	{
		World->DestroyWorld(false);
		return false;
	}

	FGameplayTagContainer CompletedBuildings;
	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouse);
	FHSConstructionJob CompletedHouseJob;
	CompletedHouseJob.BuildingTag = TAG_HS_Building_BasicHouse;
	CompletedHouseJob.State = EHSConstructionJobState::Complete;
	Construction->RestoreConstructionState({ CompletedHouseJob }, CompletedBuildings);
	TestTrue(TEXT("Settlement should be claimed."), SettlementPlan->ClaimSettlement(FVector::ZeroVector));

	FHSNPCServiceOption UpgradeService;
	TestTrue(TEXT("Builder minor upgrade service should be queryable."), ServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Builder_PlanMinorUpgrade, Player, UpgradeService));
	TestTrue(TEXT("Builder minor upgrade service should be available after house completion."), UpgradeService.bAvailable);
	TestTrue(TEXT("Executing builder minor upgrade service should place an upgrade schema."), ServiceComponent->TryExecuteService(TAG_HS_NPC_Service_Builder_PlanMinorUpgrade, Player));
	TestTrue(TEXT("Minor upgrade schema should be pending."), SettlementPlan->HasPendingSchema(TAG_HS_Building_BasicHouseMinorUpgrade));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCServiceRejectsWrongProfessionExecutionTest,
	"HimalayanSettlement.NPCServices.RejectsWrongProfessionExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCServiceRejectsWrongProfessionExecutionTest::RunTest(const FString& Parameters)
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
	AHSNPCActor* Hunter = World->SpawnActor<AHSNPCActor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	Hunter->SetNPCDefinition(CreateServiceTestNPCDefinition(Hunter, TAG_HS_NPC_Profession_Hunter, TEXT("Hunter")));

	UHSSettlementPlanComponent* SettlementPlan = Player ? Player->GetSettlementPlanComponent() : nullptr;
	UHSConstructionComponent* Construction = Player ? Player->GetConstructionComponent() : nullptr;
	UHSNPCServiceComponent* ServiceComponent = Hunter ? Hunter->GetNPCServiceComponent() : nullptr;
	UHSBuildingDefinition* HouseDefinition = CreateServiceTestHouseDefinition(Player);

	if (!Player || !SettlementPlan || !Construction || !ServiceComponent || !HouseDefinition)
	{
		World->DestroyWorld(false);
		return false;
	}

	FHSPendingBuildingSchema PendingSchema;
	TestTrue(TEXT("Settlement should be claimed."), SettlementPlan->ClaimSettlement(FVector::ZeroVector));
	TestTrue(TEXT("House schema should be pending."), SettlementPlan->PlaceSchema(HouseDefinition, FVector(250.0f, 0.0f, 0.0f), 15.0f, PendingSchema));

	TestFalse(TEXT("Hunter service component should reject direct builder service execution."), ServiceComponent->TryExecuteService(TAG_HS_NPC_Service_Builder_StartConstruction, Player));
	TestFalse(TEXT("Wrong-profession service should not start construction."), Construction->HasActiveJob());
	TestTrue(TEXT("Wrong-profession service should not consume pending schema."), SettlementPlan->HasPendingSchema(TAG_HS_Building_BasicHouse));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCServiceDailyHunterRefreshTest,
	"HimalayanSettlement.NPCServices.HunterServiceRefreshesDaily",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCServiceDailyHunterRefreshTest::RunTest(const FString& Parameters)
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
	AHSNPCActor* Hunter = World->SpawnActor<AHSNPCActor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	Hunter->SetNPCDefinition(CreateServiceTestNPCDefinition(Hunter, TAG_HS_NPC_Profession_Hunter, TEXT("Hunter")));

	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	UHSAnimalIntelComponent* AnimalIntel = Player ? Player->GetAnimalIntelComponent() : nullptr;
	UHSNPCServiceComponent* ServiceComponent = Hunter ? Hunter->GetNPCServiceComponent() : nullptr;
	TestNotNull(TEXT("World time should exist."), WorldTime);
	TestNotNull(TEXT("Hunter should own service component."), ServiceComponent);
	if (!WorldTime || !Player || !AnimalIntel || !ServiceComponent)
	{
		World->DestroyWorld(false);
		return false;
	}

	AnimalIntel->BindToWorldTime();
	ServiceComponent->BindToWorldTime();

	FHSNPCServiceOption ServiceOption;
	TestTrue(TEXT("Hunter service should be queryable."), ServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Hunter_ReportAnimalLead, Player, ServiceOption));
	TestTrue(TEXT("Hunter service should be available at start of day."), ServiceOption.bAvailable);
	TestTrue(TEXT("Executing hunter service should report a lead."), ServiceComponent->TryExecuteService(TAG_HS_NPC_Service_Hunter_ReportAnimalLead, Player));

	TestTrue(TEXT("Hunter service should still be queryable after use."), ServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Hunter_ReportAnimalLead, Player, ServiceOption));
	TestFalse(TEXT("Hunter service should be unavailable after same-day report."), ServiceOption.bAvailable);
	TestTrue(TEXT("Hunter marker refresh should be queryable while lead is active."), ServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Hunter_RefreshAnimalLead, Player, ServiceOption));
	TestTrue(TEXT("Hunter marker refresh should be available while lead is active."), ServiceOption.bAvailable);
	TestTrue(TEXT("Executing hunter marker refresh should succeed."), ServiceComponent->TryExecuteService(TAG_HS_NPC_Service_Hunter_RefreshAnimalLead, Player));

	WorldTime->AdvanceHalfDay();
	WorldTime->AdvanceHalfDay();
	TestTrue(TEXT("Hunter service should be queryable on next day."), ServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Hunter_ReportAnimalLead, Player, ServiceOption));
	TestTrue(TEXT("Hunter service should refresh on next day."), ServiceOption.bAvailable);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCServiceHerbalistAndSoldierSecondServicesTest,
	"HimalayanSettlement.NPCServices.HerbalistAndSoldierExposeSecondServices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCServiceHerbalistAndSoldierSecondServicesTest::RunTest(const FString& Parameters)
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
	AHSNPCActor* Herbalist = World->SpawnActor<AHSNPCActor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	AHSNPCActor* Soldier = World->SpawnActor<AHSNPCActor>(FVector(250.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	Herbalist->SetNPCDefinition(CreateServiceTestNPCDefinition(Herbalist, TAG_HS_NPC_Profession_Herbalist, TEXT("Herbalist")));
	Soldier->SetNPCDefinition(CreateServiceTestNPCDefinition(Soldier, TAG_HS_NPC_Profession_Soldier, TEXT("Soldier")));

	UHSPlayerHealthComponent* Health = Player ? Player->GetPlayerHealthComponent() : nullptr;
	UHSResourceCarryComponent* Carry = Player ? Player->GetResourceCarryComponent() : nullptr;
	UHSNPCServiceComponent* HerbalistServices = Herbalist ? Herbalist->GetNPCServiceComponent() : nullptr;
	UHSNPCServiceComponent* SoldierServices = Soldier ? Soldier->GetNPCServiceComponent() : nullptr;
	if (!Player || !Health || !Carry || !HerbalistServices || !SoldierServices)
	{
		World->DestroyWorld(false);
		return false;
	}

	Health->ApplyHealthDamage(40.0f);
	FHSNPCServiceOption ServiceOption;
	TestTrue(TEXT("Herbalist restore health service should be queryable."), HerbalistServices->GetServiceOption(TAG_HS_NPC_Service_Herbalist_RestoreHealth, Player, ServiceOption));
	TestTrue(TEXT("Herbalist restore health service should be available when wounded."), ServiceOption.bAvailable);
	TestTrue(TEXT("Executing herbalist restore health service should heal."), HerbalistServices->TryExecuteService(TAG_HS_NPC_Service_Herbalist_RestoreHealth, Player));
	TestGreaterThan(TEXT("Health should rise after herbalist service."), Health->GetCurrentHealth(), 60.0f);

	Carry->AddStack(FHSItemStack{ TAG_HS_Item_Bow_Basic, 1 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Item_Arrow_Basic, 2 });
	TestTrue(TEXT("Soldier arrow service should be queryable."), SoldierServices->GetServiceOption(TAG_HS_NPC_Service_Soldier_ProvideArrows, Player, ServiceOption));
	TestTrue(TEXT("Soldier arrow service should be available with low arrows."), ServiceOption.bAvailable);
	TestTrue(TEXT("Executing soldier arrow service should add arrows."), SoldierServices->TryExecuteService(TAG_HS_NPC_Service_Soldier_ProvideArrows, Player));
	TestEqual(TEXT("Soldier should add six arrows."), Carry->GetQuantityForTag(TAG_HS_Item_Arrow_Basic), 8);
	TestTrue(TEXT("Soldier spear service should be queryable."), SoldierServices->GetServiceOption(TAG_HS_NPC_Service_Soldier_ProvideSpear, Player, ServiceOption));
	TestTrue(TEXT("Soldier spear service should be available after basic bow."), ServiceOption.bAvailable);
	TestTrue(TEXT("Executing soldier spear service should add spear."), SoldierServices->TryExecuteService(TAG_HS_NPC_Service_Soldier_ProvideSpear, Player));
	TestEqual(TEXT("Soldier should add one spear."), Carry->GetQuantityForTag(TAG_HS_Item_Spear_Basic), 1);
	TestTrue(TEXT("Soldier reinforced bow service should be queryable."), SoldierServices->GetServiceOption(TAG_HS_NPC_Service_Soldier_ProvideReinforcedBow, Player, ServiceOption));
	TestTrue(TEXT("Soldier reinforced bow service should be available after basic bow."), ServiceOption.bAvailable);
	TestTrue(TEXT("Executing soldier reinforced bow service should add reinforced bow."), SoldierServices->TryExecuteService(TAG_HS_NPC_Service_Soldier_ProvideReinforcedBow, Player));
	TestEqual(TEXT("Soldier should add one reinforced bow."), Carry->GetQuantityForTag(TAG_HS_Item_Bow_Reinforced), 1);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCServicesCoverInitialRolesTest,
	"HimalayanSettlement.NPCServices.InitialRolesExposeConsistentServices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCServicesCoverInitialRolesTest::RunTest(const FString& Parameters)
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

	struct FRoleServiceExpectation
	{
		FGameplayTag ProfessionTag;
		FGameplayTag ServiceTag;
		const TCHAR* DisplayName;
	};

	const FRoleServiceExpectation Expectations[] = {
		{ TAG_HS_NPC_Profession_Builder, TAG_HS_NPC_Service_Builder_StartConstruction, TEXT("Builder") },
		{ TAG_HS_NPC_Profession_Hunter, TAG_HS_NPC_Service_Hunter_ReportAnimalLead, TEXT("Hunter") },
		{ TAG_HS_NPC_Profession_Herbalist, TAG_HS_NPC_Service_Herbalist_TeachSpeedHerb, TEXT("Herbalist") },
		{ TAG_HS_NPC_Profession_Soldier, TAG_HS_NPC_Service_Soldier_ProvideBasicWeapons, TEXT("Soldier") }
	};

	for (const FRoleServiceExpectation& Expectation : Expectations)
	{
		AHSNPCActor* NPC = World->SpawnActor<AHSNPCActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		NPC->SetNPCDefinition(CreateServiceTestNPCDefinition(NPC, Expectation.ProfessionTag, Expectation.DisplayName));

		TArray<FHSNPCServiceOption> ServiceOptions;
		UHSNPCServiceComponent* ServiceComponent = NPC->GetNPCServiceComponent();
		ServiceComponent->QueryServiceOptions(Player, ServiceOptions);

		bool bFoundExpectedService = false;
		for (const FHSNPCServiceOption& ServiceOption : ServiceOptions)
		{
			bFoundExpectedService |= ServiceOption.ServiceTag == Expectation.ServiceTag
				&& ServiceOption.ProfessionTag == Expectation.ProfessionTag
				&& !ServiceOption.Prompt.IsEmpty();
		}

		TestTrue(FString::Printf(TEXT("%s should expose a consistent role service option."), Expectation.DisplayName), bFoundExpectedService);
	}

	World->DestroyWorld(false);
	return true;
}

#endif
