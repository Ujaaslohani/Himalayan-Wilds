#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "Interaction/HSInteractable.h"
#include "NPC/HSNPCActor.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Quests/HSQuestDefinition.h"
#include "Quests/HSQuestHandoffComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSSettlementUnlockComponent.h"

namespace
{
UHSQuestDefinition* CreateValidQuest(UObject* Outer)
{
	UHSQuestDefinition* Quest = NewObject<UHSQuestDefinition>(Outer);
	Quest->DisplayName = FText::FromString(TEXT("One Morning Hunt"));
	Quest->QuestTag = TAG_HS_Quest_OneMorningHunt;
	Quest->TurnInRequirement.RequiredItems = {
		FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 },
		FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 }
	};
	Quest->RewardKnowledgeTags.AddTag(TAG_HS_Knowledge_Settlement_BasicCrafting);
	Quest->RewardUnlockTags.AddTag(TAG_HS_Building_BasicDryingRack);
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
	TestFalse(TEXT("Handoff without a quest should not complete."), Handoff->HasCompletedQuest());
	TestTrue(TEXT("Handoff without a quest should not replace NPC prompts."), Handoff->GetInteractionPrompt(nullptr).IsEmpty());
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
	TestTrue(TEXT("Configured handoff should have a player-facing prompt."), !Handoff->GetInteractionPrompt(nullptr).IsEmpty());
	TestTrue(TEXT("First offer should accept the quest."), Handoff->TryOfferQuest(nullptr));
	TestTrue(TEXT("Quest should be recorded as accepted."), Handoff->HasAcceptedQuest());
	TestEqual(TEXT("Accepted handoff should report accepted state."), Handoff->GetHandoffState(), EHSQuestHandoffState::Accepted);
	TestFalse(TEXT("Second offer should not re-accept the quest."), Handoff->TryOfferQuest(nullptr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSQuestHandoffCompletesWithResourcesTest,
	"HimalayanSettlement.Quests.HandoffCompletesWithRequiredResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSQuestHandoffCompletesWithResourcesTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Interactor = NewObject<AHSPlayerCharacter>();
	UHSResourceCarryComponent* Carry = Interactor->GetResourceCarryComponent();
	TestNotNull(TEXT("Player interactor should have carry state."), Carry);
	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 });

	UHSQuestHandoffComponent* Handoff = NewObject<UHSQuestHandoffComponent>();
	Handoff->SetQuestDefinition(CreateValidQuest(Handoff));

	TestTrue(TEXT("Quest should accept before completion."), Handoff->TryOfferQuest(Interactor));
	TestTrue(TEXT("Accepted quest should complete with required resources."), Handoff->TryCompleteQuest(Interactor));
	TestTrue(TEXT("Quest should record completed state."), Handoff->HasCompletedQuest());
	TestEqual(TEXT("Completed handoff should report completed state."), Handoff->GetHandoffState(), EHSQuestHandoffState::Completed);
	TestTrue(TEXT("Completion should grant knowledge reward tags."), Handoff->GetGrantedKnowledgeTags().HasTagExact(TAG_HS_Knowledge_Settlement_BasicCrafting));
	TestTrue(TEXT("Completion should grant unlock reward tags."), Handoff->GetGrantedUnlockTags().HasTagExact(TAG_HS_Building_BasicDryingRack));
	TestTrue(TEXT("Completion should mark the player milestone tag."), Interactor->GetPlayerProgressComponent()->HasCompletedMilestone(TAG_HS_Quest_OneMorningHunt));
	TestTrue(TEXT("Completion should grant the player settlement unlock tag."), Interactor->GetSettlementUnlockComponent()->HasUnlock(TAG_HS_Building_BasicDryingRack));
	TestFalse(TEXT("Completed quest should not complete twice."), Handoff->TryCompleteQuest(Interactor));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSQuestHandoffBlocksMissingResourcesTest,
	"HimalayanSettlement.Quests.HandoffBlocksMissingRequiredResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSQuestHandoffBlocksMissingResourcesTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Interactor = NewObject<AHSPlayerCharacter>();
	UHSResourceCarryComponent* Carry = Interactor->GetResourceCarryComponent();
	TestNotNull(TEXT("Player interactor should have carry state."), Carry);
	Carry->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 });

	UHSQuestHandoffComponent* Handoff = NewObject<UHSQuestHandoffComponent>();
	Handoff->SetQuestDefinition(CreateValidQuest(Handoff));

	TestTrue(TEXT("Quest should accept before missing-resource completion check."), Handoff->TryOfferQuest(Interactor));
	TestFalse(TEXT("Accepted quest should not complete without all required resources."), Handoff->TryCompleteQuest(Interactor));
	TestEqual(TEXT("Missing resources should leave quest accepted."), Handoff->GetHandoffState(), EHSQuestHandoffState::Accepted);
	TestTrue(TEXT("Missing resources should keep discussion prompt."), Handoff->GetInteractionPrompt(Interactor).ToString().Contains(TEXT("Discuss")));

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
