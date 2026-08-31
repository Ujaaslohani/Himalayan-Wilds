#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerProgressDefaultsTest,
	"HimalayanSettlement.Progression.PlayerProgressDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerProgressDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSPlayerProgressComponent* Progress = NewObject<UHSPlayerProgressComponent>();

	TestNotNull(TEXT("Player progress component should be constructible."), Progress);
	TestFalse(TEXT("Player progress component should not tick by default."), Progress->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Progress should not report invalid tags as complete."), Progress->HasCompletedMilestone(FGameplayTag()));
	TestEqual(TEXT("Progress should start with no completed milestones."), Progress->GetCompletedMilestones().Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerProgressMarksMilestoneOnceTest,
	"HimalayanSettlement.Progression.PlayerProgressMarksMilestoneOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerProgressMarksMilestoneOnceTest::RunTest(const FString& Parameters)
{
	UHSPlayerProgressComponent* Progress = NewObject<UHSPlayerProgressComponent>();

	TestTrue(TEXT("Valid milestone tag should be marked complete."), Progress->MarkMilestoneCompleted(TAG_HS_Quest_OneMorningHunt));
	TestTrue(TEXT("Marked milestone should be queryable."), Progress->HasCompletedMilestone(TAG_HS_Quest_OneMorningHunt));
	TestFalse(TEXT("Duplicate milestone should not be marked twice."), Progress->MarkMilestoneCompleted(TAG_HS_Quest_OneMorningHunt));
	TestEqual(TEXT("Duplicate milestone should not add a second tag."), Progress->GetCompletedMilestones().Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasProgressComponentTest,
	"HimalayanSettlement.Progression.PlayerHasNativeProgressComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasProgressComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own the native progress component."), Player ? Player->GetPlayerProgressComponent() : nullptr);

	return true;
}

#endif
