#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "UI/HSHUD.h"
#include "World/HSGameState.h"

namespace
{
bool HasLineContaining(const TArray<FString>& Lines, const TCHAR* Text)
{
	for (const FString& Line : Lines)
	{
		if (Line.Contains(Text))
		{
			return true;
		}
	}

	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHUDStatusLinesTest,
	"HimalayanSettlement.UI.HUDStatusLinesSummarizeVerticalSliceState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHUDStatusLinesTest::RunTest(const FString& Parameters)
{
	const AHSHUD* HUD = NewObject<AHSHUD>();
	const AHSGameState* GameState = NewObject<AHSGameState>();
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();

	TestNotNull(TEXT("HUD should be constructible."), HUD);
	TestNotNull(TEXT("Game state should be constructible."), GameState);
	TestNotNull(TEXT("Player should be constructible."), Player);
	if (!HUD || !GameState || !Player)
	{
		return false;
	}

	Player->GetResourceCarryComponent()->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 });

	FHSAnimalLead AnimalLead;
	AnimalLead.AnimalTag = TAG_HS_Animal_Deer;
	AnimalLead.Location = FVector(320.0f, -140.0f, 0.0f);
	AnimalLead.ReportDay = 0;
	AnimalLead.bActive = true;
	Player->GetAnimalIntelComponent()->RestoreAnimalIntelState(AnimalLead, 0);

	FHSPlantEffectState PlantEffect;
	PlantEffect.PlantTag = TAG_HS_Plant_SpeedHerb;
	PlantEffect.EffectTag = TAG_HS_Effect_Plant_Speed;
	PlantEffect.MovementSpeedMultiplier = 1.25f;
	PlantEffect.StartElapsedDays = 0.0f;
	PlantEffect.DurationDays = 0.5f;
	PlantEffect.bActive = true;
	Player->GetPlantEffectComponent()->RestorePlantState(FGameplayTagContainer(TAG_HS_Plant_SpeedHerb), PlantEffect);

	FHSConstructionJob ConstructionJob;
	ConstructionJob.BuildingTag = TAG_HS_Building_BasicHouse;
	ConstructionJob.Location = FVector(100.0f, 100.0f, 0.0f);
	ConstructionJob.RequiredDays = 1.0f;
	ConstructionJob.ProgressDays = 0.5f;
	ConstructionJob.State = EHSConstructionJobState::Active;
	Player->GetConstructionComponent()->RestoreConstructionState({ ConstructionJob }, FGameplayTagContainer());

	const TArray<FString> Lines = HUD->BuildStatusLines(GameState, Player);

	TestTrue(TEXT("HUD should show current time."), HasLineContaining(Lines, TEXT("Time:")));
	TestTrue(TEXT("HUD should show carried resources."), HasLineContaining(Lines, TEXT("Resource.Meat.Deer x2")));
	TestTrue(TEXT("HUD should show active animal lead."), HasLineContaining(Lines, TEXT("Lead: Animal.Deer")));
	TestTrue(TEXT("HUD should show active plant effect."), HasLineContaining(Lines, TEXT("Plant: Effect.Plant.Speed")));
	TestTrue(TEXT("HUD should show active construction progress."), HasLineContaining(Lines, TEXT("Build: Building.BasicHouse 50%")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHUDObjectiveLinesTest,
	"HimalayanSettlement.UI.HUDObjectiveLinesGuideEarlyVerticalSlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHUDObjectiveLinesTest::RunTest(const FString& Parameters)
{
	const AHSHUD* HUD = NewObject<AHSHUD>();
	const AHSGameState* GameState = NewObject<AHSGameState>();
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();

	TestNotNull(TEXT("HUD should be constructible."), HUD);
	TestNotNull(TEXT("Game state should be constructible."), GameState);
	TestNotNull(TEXT("Player should be constructible."), Player);
	if (!HUD || !GameState || !Player)
	{
		return false;
	}

	TestTrue(
		TEXT("Default objective should start with Hunter guidance."),
		HasLineContaining(HUD->BuildObjectiveLines(GameState, Player), TEXT("Talk to the Hunter")));

	Player->GetResourceCarryComponent()->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 });
	Player->GetResourceCarryComponent()->AddStack(FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 });
	TestTrue(
		TEXT("Carrying hunt resources should point back to the Hunter."),
		HasLineContaining(HUD->BuildObjectiveLines(GameState, Player), TEXT("Return deer meat and hide")));

	Player->GetPlayerProgressComponent()->MarkMilestoneCompleted(TAG_HS_Quest_OneMorningHunt);
	TestTrue(
		TEXT("Completed morning hunt should point to settlement claim."),
		HasLineContaining(HUD->BuildObjectiveLines(GameState, Player), TEXT("Claim the settlement site")));

	Player->GetSettlementPlanComponent()->RestoreSettlementPlan(true, FVector::ZeroVector, {});
	TestTrue(
		TEXT("Claimed settlement should point to house placement."),
		HasLineContaining(HUD->BuildObjectiveLines(GameState, Player), TEXT("Press B to place")));

	FHSPendingBuildingSchema PendingSchema;
	PendingSchema.BuildingTag = TAG_HS_Building_BasicHouse;
	PendingSchema.Location = FVector(120.0f, 120.0f, 0.0f);
	Player->GetSettlementPlanComponent()->RestoreSettlementPlan(true, FVector::ZeroVector, { PendingSchema });
	TestTrue(
		TEXT("Pending schema should point to Builder."),
		HasLineContaining(HUD->BuildObjectiveLines(GameState, Player), TEXT("Ask the Builder")));

	FHSConstructionJob ActiveJob;
	ActiveJob.BuildingTag = TAG_HS_Building_BasicHouse;
	ActiveJob.RequiredDays = 1.0f;
	ActiveJob.ProgressDays = 0.5f;
	ActiveJob.State = EHSConstructionJobState::Active;
	Player->GetConstructionComponent()->RestoreConstructionState({ ActiveJob }, FGameplayTagContainer());
	TestTrue(
		TEXT("Active construction should point to sleep/time advancement."),
		HasLineContaining(HUD->BuildObjectiveLines(GameState, Player), TEXT("Sleep to advance")));

	FHSConstructionJob CompletedJob = ActiveJob;
	CompletedJob.ProgressDays = 1.0f;
	CompletedJob.State = EHSConstructionJobState::Complete;
	FGameplayTagContainer CompletedBuildings;
	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouse);
	Player->GetConstructionComponent()->RestoreConstructionState({ CompletedJob }, CompletedBuildings);
	TestTrue(
		TEXT("Completed house should point to NPC services and exploration."),
		HasLineContaining(HUD->BuildObjectiveLines(GameState, Player), TEXT("Basic house complete")));

	return true;
}

#endif
