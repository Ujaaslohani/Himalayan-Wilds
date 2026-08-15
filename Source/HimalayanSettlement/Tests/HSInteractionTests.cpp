#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Interaction/HSInteractable.h"
#include "Interaction/HSInteractionTarget.h"
#include "NPC/HSNPCActor.h"
#include "Player/HSGameMode.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Settlement/HSRestPointActor.h"
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
	TestNotNull(TEXT("Default NPC should have a prototype visual mesh component."), NPC->GetPrototypeMesh());
	TestNotNull(TEXT("Default NPC prototype visual should have a mesh asset."), NPC->GetPrototypeMesh() && NPC->GetPrototypeMesh()->GetStaticMesh() ? NPC->GetPrototypeMesh()->GetStaticMesh().Get() : nullptr);
	TestEqual(TEXT("Default NPC prototype visual should not handle collision."), NPC->GetPrototypeMesh() ? NPC->GetPrototypeMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSInteractionTargetVisualDefaultsTest,
	"HimalayanSettlement.Interaction.TargetVisualMarkerDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSInteractionTargetVisualDefaultsTest::RunTest(const FString& Parameters)
{
	const AHSInteractionTarget* Target = GetDefault<AHSInteractionTarget>();

	TestNotNull(TEXT("Interaction target class should have a valid default object."), Target);
	TestNotNull(TEXT("Interaction target should have a prototype visual mesh component."), Target ? Target->GetPrototypeMesh() : nullptr);
	TestNotNull(TEXT("Interaction target prototype visual should have a mesh asset."), Target && Target->GetPrototypeMesh() && Target->GetPrototypeMesh()->GetStaticMesh() ? Target->GetPrototypeMesh()->GetStaticMesh().Get() : nullptr);
	TestEqual(TEXT("Interaction target prototype visual should not handle collision."), Target && Target->GetPrototypeMesh() ? Target->GetPrototypeMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSRestPointMilestoneGateTest,
	"HimalayanSettlement.Interaction.RestPointRequiresHuntMilestone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRestPointMilestoneGateTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	AHSRestPointActor* RestPoint = NewObject<AHSRestPointActor>();
	UHSPlayerProgressComponent* Progress = Player->GetPlayerProgressComponent();

	TestNotNull(TEXT("Rest point should be constructible."), RestPoint);
	TestFalse(TEXT("Rest point should not tick by default."), RestPoint ? RestPoint->PrimaryActorTick.bCanEverTick : true);
	TestTrue(TEXT("Rest point should remain interactable so blocked prompts can be shown."), HSInteraction::CanInteract(RestPoint, Player));
	TestFalse(TEXT("Player should not be ready to rest before completing the hunt."), RestPoint && RestPoint->IsReadyToRest(Player));

	HSInteraction::Interact(RestPoint, Player);
	TestFalse(TEXT("Blocked rest should not mark the end-of-day milestone."), Progress && Progress->HasCompletedMilestone(TAG_HS_Milestone_OneMorningHunt_DayEnded));

	Progress->MarkMilestoneCompleted(TAG_HS_Quest_OneMorningHunt);
	TestTrue(TEXT("Completed hunt should allow the player to rest."), RestPoint && RestPoint->IsReadyToRest(Player));

	HSInteraction::Interact(RestPoint, Player);
	TestTrue(TEXT("Rest should mark the end-of-day milestone."), Progress && Progress->HasCompletedMilestone(TAG_HS_Milestone_OneMorningHunt_DayEnded));
	TestTrue(TEXT("Rest point should report rested state after completion."), RestPoint && RestPoint->HasRested(Player));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHUDCompletionMessageDefaultsTest,
	"HimalayanSettlement.Interaction.HUDCompletionMessageDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHUDCompletionMessageDefaultsTest::RunTest(const FString& Parameters)
{
	AHSHUD* HUD = NewObject<AHSHUD>();

	TestNotNull(TEXT("HUD should be constructible."), HUD);
	TestEqual(TEXT("HUD completion message duration should be short."), HUD->GetCompletionMessageDuration(), 4.0f);
	TestFalse(TEXT("HUD should start without an active completion message."), HUD->HasActiveCompletionMessage());

	HUD->HandleMilestoneCompleted(TAG_HS_Quest_OneMorningHunt);

	TestTrue(TEXT("HUD should activate completion messages for valid milestone tags."), HUD->HasActiveCompletionMessage());
	TestFalse(TEXT("HUD completion message should be player-facing."), HUD->GetCompletionMessage().IsEmpty());

	return true;
}

#endif
