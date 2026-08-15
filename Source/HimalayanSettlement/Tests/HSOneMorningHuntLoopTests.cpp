#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalActor.h"
#include "Animals/HSAnimalDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "Hunting/HSHuntingComponent.h"
#include "Interaction/HSInteractable.h"
#include "NPC/HSNPCActor.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Quests/HSQuestDefinition.h"
#include "Quests/HSQuestHandoffComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSRestPointActor.h"
#include "Settlement/HSSettlementUnlockComponent.h"

namespace
{
UHSQuestDefinition* CreateOneMorningHuntLoopQuest(UObject* Outer)
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

UHSAnimalDefinition* CreateOneMorningHuntLoopDeer(UObject* Outer)
{
	UHSAnimalDefinition* Deer = NewObject<UHSAnimalDefinition>(Outer);
	Deer->DisplayName = FText::FromString(TEXT("Deer"));
	Deer->SpeciesTag = TAG_HS_Animal_Deer;
	Deer->MaxHealth = 70.0f;
	Deer->HarvestDrops = {
		FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 },
		FHSItemStack{ TAG_HS_Resource_Hide_Deer, 1 }
	};
	return Deer;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSOneMorningHuntNativeLoopTest,
	"HimalayanSettlement.Milestone.OneMorningHuntNativeLoopCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSOneMorningHuntNativeLoopTest::RunTest(const FString& Parameters)
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
	AHSAnimalActor* Deer = World->SpawnActor<AHSAnimalActor>(FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	AHSRestPointActor* RestPoint = World->SpawnActor<AHSRestPointActor>(FVector(-100.0f, 150.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);

	TestNotNull(TEXT("Player should spawn in the transient test world."), Player);
	TestNotNull(TEXT("Hunter should spawn in the transient test world."), Hunter);
	TestNotNull(TEXT("Deer should spawn in the transient test world."), Deer);
	TestNotNull(TEXT("Rest point should spawn in the transient test world."), RestPoint);
	if (!Player || !Hunter || !Deer || !RestPoint)
	{
		World->DestroyWorld(false);
		return false;
	}

	UHSQuestDefinition* Quest = CreateOneMorningHuntLoopQuest(Hunter);
	UHSAnimalDefinition* DeerDefinition = CreateOneMorningHuntLoopDeer(Deer);
	Hunter->SetOfferedQuest(Quest);
	Deer->SetAnimalDefinition(DeerDefinition);

	UHSQuestHandoffComponent* Handoff = Hunter->GetQuestHandoffComponent();
	UHSHuntingComponent* Hunting = Player->GetHuntingComponent();
	UHSResourceCarryComponent* Carry = Player->GetResourceCarryComponent();
	UHSPlayerProgressComponent* Progress = Player->GetPlayerProgressComponent();
	UHSSettlementUnlockComponent* Unlocks = Player->GetSettlementUnlockComponent();

	TestNotNull(TEXT("Hunter should own the quest handoff component."), Handoff);
	TestNotNull(TEXT("Player should own the hunting component."), Hunting);
	TestNotNull(TEXT("Player should own the resource carry component."), Carry);
	TestNotNull(TEXT("Player should own the progress component."), Progress);
	TestNotNull(TEXT("Player should own the settlement unlock component."), Unlocks);

	TestTrue(TEXT("Hunter should be interactable before the quest is accepted."), HSInteraction::CanInteract(Hunter, Player));
	HSInteraction::Interact(Hunter, Player);
	TestEqual(TEXT("First hunter interaction should accept One Morning Hunt."), Handoff ? Handoff->GetHandoffState() : EHSQuestHandoffState::Unavailable, EHSQuestHandoffState::Accepted);

	TestTrue(TEXT("Player hunting action should damage the configured deer."), Hunting && Hunting->ApplyDamageToTarget(Deer));
	TestTrue(TEXT("One Morning Hunt deer should die from the prototype hunting action."), Deer->IsDead());
	TestTrue(TEXT("Dead deer should become harvestable."), HSInteraction::CanInteract(Deer, Player));

	HSInteraction::Interact(Deer, Player);
	TestTrue(TEXT("Deer should record its one-shot harvest state."), Deer->HasBeenHarvested());
	TestEqual(TEXT("Harvested deer meat should be carried by the player."), Carry ? Carry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer) : 0, 2);
	TestEqual(TEXT("Harvested deer hide should be carried by the player."), Carry ? Carry->GetQuantityForTag(TAG_HS_Resource_Hide_Deer) : 0, 1);

	HSInteraction::Interact(Hunter, Player);
	TestEqual(TEXT("Second hunter interaction should complete One Morning Hunt."), Handoff ? Handoff->GetHandoffState() : EHSQuestHandoffState::Unavailable, EHSQuestHandoffState::Completed);
	TestTrue(TEXT("Quest completion should mark the player milestone."), Progress && Progress->HasCompletedMilestone(TAG_HS_Quest_OneMorningHunt));
	TestTrue(TEXT("Quest completion should grant basic crafting knowledge."), Handoff && Handoff->GetGrantedKnowledgeTags().HasTagExact(TAG_HS_Knowledge_Settlement_BasicCrafting));
	TestTrue(TEXT("Quest completion should unlock the basic drying rack."), Unlocks && Unlocks->HasUnlock(TAG_HS_Building_BasicDryingRack));
	TestEqual(TEXT("Completed turn-in should consume carried deer meat."), Carry ? Carry->GetQuantityForTag(TAG_HS_Resource_Meat_Deer) : -1, 0);
	TestEqual(TEXT("Completed turn-in should consume carried deer hide."), Carry ? Carry->GetQuantityForTag(TAG_HS_Resource_Hide_Deer) : -1, 0);
	TestEqual(TEXT("Native loop should require exactly two hunter interactions."), Hunter->GetInteractionCount(), 2);

	TestTrue(TEXT("Completed hunt should unlock the rest endpoint."), RestPoint->IsReadyToRest(Player));
	HSInteraction::Interact(RestPoint, Player);
	TestTrue(TEXT("Rest endpoint should mark the end-of-day milestone."), Progress && Progress->HasCompletedMilestone(TAG_HS_Milestone_OneMorningHunt_DayEnded));

	World->DestroyWorld(false);
	return true;
}

#endif
