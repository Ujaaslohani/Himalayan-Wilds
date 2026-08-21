#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Player/HSPlayerCharacter.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPauseMenuStateTest,
	"HimalayanSettlement.UI.PauseMenuStateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPauseMenuStateTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player should be constructible."), Player);
	if (!Player)
	{
		return false;
	}

	TestFalse(TEXT("Pause menu should start closed."), Player->IsPauseMenuOpen());
	TestFalse(TEXT("Pause options should start closed."), Player->IsPauseOptionsOpen());
	TestFalse(TEXT("Inventory should start closed."), Player->IsInventoryOpen());
	TestFalse(TEXT("Build menu should start closed."), Player->IsBuildMenuOpen());

	Player->OpenPauseMenu();
	TestTrue(TEXT("OpenPauseMenu should open the pause menu."), Player->IsPauseMenuOpen());
	TestFalse(TEXT("Opening pause menu should not open options by default."), Player->IsPauseOptionsOpen());

	Player->TogglePauseOptions();
	TestTrue(TEXT("Options should toggle open while the pause menu is open."), Player->IsPauseOptionsOpen());

	Player->ClosePauseMenu();
	TestFalse(TEXT("ClosePauseMenu should close the pause menu."), Player->IsPauseMenuOpen());
	TestFalse(TEXT("Closing pause menu should also close options."), Player->IsPauseOptionsOpen());

	Player->ToggleInventoryMenu();
	TestTrue(TEXT("Inventory should open from toggle."), Player->IsInventoryOpen());
	TestFalse(TEXT("Opening inventory should leave build menu closed."), Player->IsBuildMenuOpen());

	Player->ToggleBuildMenu();
	TestTrue(TEXT("Build menu should open from toggle."), Player->IsBuildMenuOpen());
	TestFalse(TEXT("Opening build menu should close inventory."), Player->IsInventoryOpen());

	Player->OpenPauseMenu();
	TestTrue(TEXT("Pause menu should open over other menus."), Player->IsPauseMenuOpen());
	TestFalse(TEXT("Opening pause should close build menu."), Player->IsBuildMenuOpen());
	TestFalse(TEXT("Opening pause should keep inventory closed."), Player->IsInventoryOpen());
	Player->ClosePauseMenu();

	const int32 InitialPreset = Player->GetGraphicsQualityPreset();
	const FString InitialPresetName = Player->GetGraphicsQualityPresetName();
	Player->CycleGraphicsQualityPreset();
	TestNotEqual(TEXT("Cycling graphics should change preset index."), Player->GetGraphicsQualityPreset(), InitialPreset);
	TestNotEqual(TEXT("Cycling graphics should change preset label."), Player->GetGraphicsQualityPresetName(), InitialPresetName);
	TestTrue(TEXT("Cycling graphics should produce visible menu feedback."), Player->HasActiveMenuFeedback());
	TestTrue(TEXT("Graphics feedback should name the preset."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Graphics preset")));

	TestFalse(TEXT("Performance overlay should start disabled."), Player->IsPerformanceOverlayEnabled());
	Player->TogglePerformanceOverlay();
	TestTrue(TEXT("Performance overlay should toggle on."), Player->IsPerformanceOverlayEnabled());
	TestTrue(TEXT("Performance overlay toggle should produce feedback."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Performance overlay: On")));
	Player->TogglePerformanceOverlay();
	TestFalse(TEXT("Performance overlay should toggle off."), Player->IsPerformanceOverlayEnabled());
	TestTrue(TEXT("Performance overlay off toggle should produce feedback."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Performance overlay: Off")));

	TestFalse(TEXT("Emergency low-end mode should start disabled."), Player->IsEmergencyLowEndModeEnabled());
	Player->ToggleEmergencyLowEndMode();
	TestTrue(TEXT("Emergency low-end mode should toggle on."), Player->IsEmergencyLowEndModeEnabled());
	TestTrue(TEXT("Emergency low-end feedback should report enabled state."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Emergency low-end mode: On")));
	Player->ToggleEmergencyLowEndMode();
	TestFalse(TEXT("Emergency low-end mode should toggle off."), Player->IsEmergencyLowEndModeEnabled());
	TestTrue(TEXT("Emergency low-end feedback should report disabled state."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Emergency low-end mode: Off")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSInventoryMenuActionsTest,
	"HimalayanSettlement.UI.InventoryMenuActionsInspectAndDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSInventoryMenuActionsTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player should be constructible."), Player);
	TestNotNull(TEXT("Player should own resource carry."), Player ? Player->GetResourceCarryComponent() : nullptr);
	if (!Player || !Player->GetResourceCarryComponent())
	{
		return false;
	}

	Player->GetResourceCarryComponent()->AddStack(FHSItemStack{ TAG_HS_Resource_Meat_Deer, 2 });

	TestTrue(TEXT("Inspecting the first carried stack should succeed."), Player->InspectInventoryStack(0));
	TestTrue(TEXT("Inspect action should expose menu feedback."), Player->HasActiveMenuFeedback());
	TestTrue(TEXT("Inspect feedback should mention the item tag."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Resource.Meat.Deer")));

	TestTrue(TEXT("Dropping one item from the first stack should succeed."), Player->DropInventoryStackQuantity(0, 1));
	TestEqual(TEXT("One meat should remain after dropping one."), Player->GetResourceCarryComponent()->GetQuantityForTag(TAG_HS_Resource_Meat_Deer), 1);
	TestTrue(TEXT("Drop feedback should mention the dropped item."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Dropped Resource.Meat.Deer")));

	TestTrue(TEXT("Dropping the remaining stack should succeed."), Player->DropInventoryStackQuantity(0, 999));
	TestEqual(TEXT("No meat should remain after dropping the stack."), Player->GetResourceCarryComponent()->GetQuantityForTag(TAG_HS_Resource_Meat_Deer), 0);
	TestFalse(TEXT("Inspecting an empty row should fail."), Player->InspectInventoryStack(0));
	TestTrue(TEXT("Empty inspect should explain the missing slot."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("No carried item")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSBuildMenuAvailabilityFeedbackTest,
	"HimalayanSettlement.UI.BuildMenuAvailabilityExplainsBlockedHouse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSBuildMenuAvailabilityFeedbackTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	FText Reason;

	TestNotNull(TEXT("Player should be constructible."), Player);
	TestNotNull(TEXT("Player should own settlement plan."), Player ? Player->GetSettlementPlanComponent() : nullptr);
	if (!Player || !Player->GetSettlementPlanComponent())
	{
		return false;
	}

	TestFalse(TEXT("Basic house should be blocked before claim."), Player->CanStartBasicHouseFromBuildMenu(&Reason));
	TestTrue(TEXT("Blocked build reason should mention settlement claim."), Reason.ToString().Contains(TEXT("Claim")));

	Player->GetSettlementPlanComponent()->RestoreSettlementPlan(true, FVector::ZeroVector, {});
	TestTrue(TEXT("Basic house should be available after claim."), Player->CanStartBasicHouseFromBuildMenu(&Reason));
	TestTrue(TEXT("Available build reason should mention readiness."), Reason.ToString().Contains(TEXT("Ready")));

	FHSPendingBuildingSchema PendingSchema;
	PendingSchema.BuildingTag = TAG_HS_Building_BasicHouse;
	Player->GetSettlementPlanComponent()->RestoreSettlementPlan(true, FVector::ZeroVector, { PendingSchema });
	TestFalse(TEXT("Basic house should be blocked when schema is already pending."), Player->CanStartBasicHouseFromBuildMenu(&Reason));
	TestTrue(TEXT("Pending build reason should mention Builder."), Reason.ToString().Contains(TEXT("Builder")));

	FHSConstructionJob CompletedJob;
	CompletedJob.BuildingTag = TAG_HS_Building_BasicHouse;
	CompletedJob.State = EHSConstructionJobState::Complete;
	FGameplayTagContainer CompletedBuildings;
	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouse);
	Player->GetConstructionComponent()->RestoreConstructionState({ CompletedJob }, CompletedBuildings);
	Player->GetSettlementPlanComponent()->RestoreSettlementPlan(true, FVector::ZeroVector, {});
	TestFalse(TEXT("Basic house should be blocked once complete."), Player->CanStartBasicHouseFromBuildMenu(&Reason));
	TestTrue(TEXT("Completed build reason should mention completion."), Reason.ToString().Contains(TEXT("complete")));

	return true;
}

#endif
