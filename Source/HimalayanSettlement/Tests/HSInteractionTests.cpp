#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Interaction/HSInteractable.h"
#include "NPC/HSNPCActor.h"
#include "Player/HSGameMode.h"
#include "UI/HSHUD.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSInteractionSensorDefaultsTest,
	"HimalayanSettlement.Interaction.SensorDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSInteractionSensorDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSInteractionSensorComponent* Sensor = NewObject<UHSInteractionSensorComponent>();

	TestNotNull(TEXT("Interaction sensor should be constructible."), Sensor);
	TestFalse(TEXT("Interaction sensor should not tick by default."), Sensor->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("Interaction distance should stay short for first-person use."), Sensor->GetInteractionDistance(), 300.0f);
	TestEqual(TEXT("Interaction traces should use visibility by default."), Sensor->GetTraceChannel().GetValue(), static_cast<uint8>(ECC_Visibility));
	TestEqual(TEXT("Interaction focus should refresh on a short timer."), Sensor->GetFocusRefreshInterval(), 0.1f);
	TestNull(TEXT("Interaction focus should start empty."), Sensor->GetFocusedInteractable());
	TestTrue(TEXT("Interaction prompt should start empty."), Sensor->GetFocusedInteractionPrompt().IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCActorDefaultsTest,
	"HimalayanSettlement.Interaction.NPCDefaultsSupportHunterConversation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCActorDefaultsTest::RunTest(const FString& Parameters)
{
	AHSNPCActor* NPC = GetMutableDefault<AHSNPCActor>();

	TestNotNull(TEXT("NPC actor class should have a valid default object."), NPC);
	TestFalse(TEXT("NPC actors should not tick by default."), NPC->PrimaryActorTick.bCanEverTick);
	TestTrue(TEXT("Default NPC should be interactable."), HSInteraction::CanInteract(NPC, nullptr));
	TestFalse(TEXT("Default NPC prompt should be usable without authored data."), HSInteraction::GetInteractionPrompt(NPC, nullptr).IsEmpty());
	TestEqual(TEXT("Default NPC starts without recorded interactions."), NPC->GetInteractionCount(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHUDConfiguredByGameModeTest,
	"HimalayanSettlement.Interaction.HUDConfiguredForPromptFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHUDConfiguredByGameModeTest::RunTest(const FString& Parameters)
{
	const AHSGameMode* GameMode = GetDefault<AHSGameMode>();

	TestNotNull(TEXT("Game mode class should have a valid default object."), GameMode);
	TestEqual(TEXT("Game mode should use the native prompt HUD."), GameMode->HUDClass.Get(), AHSHUD::StaticClass());

	return true;
}

#endif
