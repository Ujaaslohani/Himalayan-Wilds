#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Player/HSPlayerCharacter.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
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

#endif
