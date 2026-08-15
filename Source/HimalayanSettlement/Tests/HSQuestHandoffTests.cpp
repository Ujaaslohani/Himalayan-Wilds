#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "Interaction/HSInteractable.h"
#include "NPC/HSNPCActor.h"
#include "Quests/HSQuestDefinition.h"
#include "Quests/HSQuestHandoffComponent.h"

namespace
{
UHSQuestDefinition* CreateValidQuest(UObject* Outer)
{
	UHSQuestDefinition* Quest = NewObject<UHSQuestDefinition>(Outer);
	Quest->DisplayName = FText::FromString(TEXT("One Morning Hunt"));
	Quest->QuestTag = TAG_HS_Quest_OneMorningHunt;
	return Quest;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSQuestHandoffDefaultsTest,
	"HimalayanSettlement.Quests.HandoffDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSQuestHandoffDefaultsTest::RunTest(const FString& Parameters)
{
	UHSQuestHandoffComponent* Handoff = NewObject<UHSQuestHandoffComponent>();

	TestNotNull(TEXT("Quest handoff component should be constructible."), Handoff);
	TestFalse(TEXT("Quest handoff component should not tick by default."), Handoff->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("Handoff without a quest should be unavailable."), Handoff->GetHandoffState(), EHSQuestHandoffState::Unavailable);
	TestFalse(TEXT("Handoff without a quest should not accept."), Handoff->HasAcceptedQuest());
	TestTrue(TEXT("Handoff without a quest should not replace NPC prompts."), Handoff->GetInteractionPrompt().IsEmpty());
	TestFalse(TEXT("Handoff without a quest cannot offer."), Handoff->TryOfferQuest(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSQuestHandoffOfferOnceTest,
	"HimalayanSettlement.Quests.HandoffOffersQuestOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSQuestHandoffOfferOnceTest::RunTest(const FString& Parameters)
{
	UHSQuestHandoffComponent* Handoff = NewObject<UHSQuestHandoffComponent>();
	UHSQuestDefinition* Quest = CreateValidQuest(Handoff);

	Handoff->SetQuestDefinition(Quest);

	TestEqual(TEXT("Configured handoff should be available."), Handoff->GetHandoffState(), EHSQuestHandoffState::Available);
	TestTrue(TEXT("Configured handoff should have a player-facing prompt."), !Handoff->GetInteractionPrompt().IsEmpty());
	TestTrue(TEXT("First offer should accept the quest."), Handoff->TryOfferQuest(nullptr));
	TestTrue(TEXT("Quest should be recorded as accepted."), Handoff->HasAcceptedQuest());
	TestEqual(TEXT("Accepted handoff should report accepted state."), Handoff->GetHandoffState(), EHSQuestHandoffState::Accepted);
	TestFalse(TEXT("Second offer should not re-accept the quest."), Handoff->TryOfferQuest(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSNPCQuestHandoffIntegrationTest,
	"HimalayanSettlement.Quests.NPCUsesQuestHandoffPromptAndState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSNPCQuestHandoffIntegrationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	AHSNPCActor* NPC = World->SpawnActor<AHSNPCActor>();
	TestNotNull(TEXT("NPC should spawn in the transient test world."), NPC);
	if (!NPC)
	{
		World->DestroyWorld(false);
		return false;
	}

	UHSQuestDefinition* Quest = CreateValidQuest(NPC);

	NPC->SetOfferedQuest(Quest);

	UHSQuestHandoffComponent* Handoff = NPC->GetQuestHandoffComponent();
	TestNotNull(TEXT("NPC should own a quest handoff component."), Handoff);
	TestEqual(TEXT("NPC quest handoff should start available."), Handoff ? Handoff->GetHandoffState() : EHSQuestHandoffState::Unavailable, EHSQuestHandoffState::Available);
	TestTrue(TEXT("NPC prompt should use the quest handoff when configured."), HSInteraction::GetInteractionPrompt(NPC, nullptr).ToString().Contains(TEXT("One Morning Hunt")));

	HSInteraction::Interact(NPC, nullptr);

	TestEqual(TEXT("NPC interaction should accept the handoff quest."), Handoff ? Handoff->GetHandoffState() : EHSQuestHandoffState::Unavailable, EHSQuestHandoffState::Accepted);
	TestEqual(TEXT("NPC should still record interaction count."), NPC->GetInteractionCount(), 1);

	World->DestroyWorld(false);
	return true;
}

#endif
