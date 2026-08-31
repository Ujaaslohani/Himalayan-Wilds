#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "Interaction/HSInteractable.h"
#include "Player/HSGameMode.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Settlement/HSTemporaryShelterActor.h"
#include "UI/HSHUD.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSWorldTimeDefaultsAndAdvanceTest,
	"HimalayanSettlement.Time.WorldTimeDefaultsAndAdvances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSWorldTimeDefaultsAndAdvanceTest::RunTest(const FString& Parameters)
{
	UHSWorldTimeComponent* WorldTime = NewObject<UHSWorldTimeComponent>();

	TestNotNull(TEXT("World time component should be constructible."), WorldTime);
	TestFalse(TEXT("World time should not tick by default."), WorldTime->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("World time should start in day phase."), WorldTime->GetTimePhase(), EHSTimePhase::Day);
	TestEqual(TEXT("World time should start at zero elapsed half-days."), WorldTime->GetElapsedHalfDays(), 0);
	TestEqual(TEXT("World time should start at zero elapsed days."), WorldTime->GetElapsedDays(), 0.0f);

	TestEqual(TEXT("First half-day advance should enter night."), WorldTime->AdvanceHalfDay(), EHSTimePhase::Night);
	TestEqual(TEXT("One half-day should equal 0.5 elapsed days."), WorldTime->GetElapsedDays(), 0.5f);
	TestEqual(TEXT("Second half-day advance should return to day."), WorldTime->AdvanceHalfDay(), EHSTimePhase::Day);
	TestEqual(TEXT("Two half-days should equal one elapsed day."), WorldTime->GetElapsedDays(), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSGameModeUsesWorldTimeGameStateTest,
	"HimalayanSettlement.Time.GameModeUsesWorldTimeGameState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSGameModeUsesWorldTimeGameStateTest::RunTest(const FString& Parameters)
{
	const AHSGameMode* GameMode = GetDefault<AHSGameMode>();
	const AHSGameState* GameState = GetDefault<AHSGameState>();

	TestNotNull(TEXT("Game mode class should have a valid default object."), GameMode);
	TestEqual(TEXT("Game mode should use native game state."), GameMode ? GameMode->GameStateClass.Get() : nullptr, AHSGameState::StaticClass());
	TestNotNull(TEXT("Game state class should have a valid default object."), GameState);
	TestNotNull(TEXT("Game state should own native world time component."), GameState ? GameState->GetWorldTimeComponent() : nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSTemporaryShelterSleepExpiresTest,
	"HimalayanSettlement.Time.TemporaryShelterSleepFlipsTimeAndExpires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSTemporaryShelterSleepExpiresTest::RunTest(const FString& Parameters)
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
	AHSTemporaryShelterActor* Shelter = World->SpawnActor<AHSTemporaryShelterActor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);

	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	UHSPlayerProgressComponent* Progress = Player ? Player->GetPlayerProgressComponent() : nullptr;

	TestNotNull(TEXT("Game state should spawn in the transient test world."), GameState);
	TestNotNull(TEXT("Player should spawn in the transient test world."), Player);
	TestNotNull(TEXT("Shelter should spawn in the transient test world."), Shelter);
	TestNotNull(TEXT("Spawned game state should own world time."), WorldTime);
	if (!GameState || !Player || !Shelter || !WorldTime)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestFalse(TEXT("Temporary shelter should not tick by default."), Shelter->PrimaryActorTick.bCanEverTick);
	TestTrue(TEXT("Fresh shelter should allow sleep."), Shelter->CanSleep(Player));
	TestTrue(TEXT("Fresh day shelter should prompt sleep until night."), HSInteraction::GetInteractionPrompt(Shelter, Player).ToString().Contains(TEXT("night")));

	HSInteraction::Interact(Shelter, Player);

	TestEqual(TEXT("Sleeping at shelter should flip day to night."), WorldTime->GetTimePhase(), EHSTimePhase::Night);
	TestEqual(TEXT("Sleeping at shelter should advance half a day."), WorldTime->GetElapsedDays(), 0.5f);
	TestTrue(TEXT("Temporary shelter should expire after one sleep."), Shelter->IsExpired());
	TestEqual(TEXT("Temporary shelter should have no sleep uses left."), Shelter->GetRemainingSleepUses(), 0);
	TestFalse(TEXT("Expired shelter should not allow another sleep."), Shelter->CanSleep(Player));
	TestTrue(TEXT("Shelter sleep should mark player milestone for feedback."), Progress && Progress->HasCompletedMilestone(TAG_HS_Milestone_TemporaryShelter_Slept));

	HSInteraction::Interact(Shelter, Player);

	TestEqual(TEXT("Second shelter interaction should not advance time again."), WorldTime->GetElapsedDays(), 0.5f);

	World->DestroyWorld(false);
	return true;
}

#endif
