#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Player/HSPlayerCharacter.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSBuildPlacementComponent.h"
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
	TestEqual(TEXT("Build menu should start on the first entry."), Player->GetSelectedBuildMenuIndex(), 0);
	Player->SelectBuildMenuDelta(1);
	TestEqual(TEXT("Build menu selection should move to minor upgrade."), Player->GetSelectedBuildMenuIndex(), 1);
	Player->SelectBuildMenuDelta(1);
	TestEqual(TEXT("Build menu selection should move to campsite."), Player->GetSelectedBuildMenuIndex(), 2);
	Player->SelectBuildMenuDelta(-1);
	TestEqual(TEXT("Build menu selection should move back to minor upgrade."), Player->GetSelectedBuildMenuIndex(), 1);
	Player->SelectBuildMenuDelta(-1);
	TestEqual(TEXT("Build menu selection should move back to house."), Player->GetSelectedBuildMenuIndex(), 0);

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

	const int32 InitialSensitivityPreset = Player->GetMouseSensitivityPreset();
	const FString InitialSensitivityName = Player->GetMouseSensitivityPresetName();
	Player->CycleMouseSensitivityPreset();
	TestNotEqual(TEXT("Cycling mouse sensitivity should change preset index."), Player->GetMouseSensitivityPreset(), InitialSensitivityPreset);
	TestNotEqual(TEXT("Cycling mouse sensitivity should change preset label."), Player->GetMouseSensitivityPresetName(), InitialSensitivityName);
	TestTrue(TEXT("Mouse sensitivity feedback should name the setting."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Mouse sensitivity")));

	const int32 InitialVolumePreset = Player->GetMasterVolumePreset();
	const FString InitialVolumeName = Player->GetMasterVolumePresetName();
	Player->CycleMasterVolumePreset();
	TestNotEqual(TEXT("Cycling master volume should change preset index."), Player->GetMasterVolumePreset(), InitialVolumePreset);
	TestNotEqual(TEXT("Cycling master volume should change preset label."), Player->GetMasterVolumePresetName(), InitialVolumeName);
	TestTrue(TEXT("Master volume feedback should name the setting."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Master volume")));

	const FString InitialManualSaveSlotName = Player->GetSelectedManualSaveSlotName();
	const FString InitialManualSaveSlotLabel = Player->GetSelectedManualSaveSlotLabel();
	Player->CycleManualSaveSlot();
	TestNotEqual(TEXT("Cycling manual save slot should change slot name."), Player->GetSelectedManualSaveSlotName(), InitialManualSaveSlotName);
	TestNotEqual(TEXT("Cycling manual save slot should change slot label."), Player->GetSelectedManualSaveSlotLabel(), InitialManualSaveSlotLabel);
	TestTrue(TEXT("Manual save slot feedback should name the setting."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Manual save slot")));

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
	Player->GetResourceCarryComponent()->AddStack(FHSItemStack{ TAG_HS_Resource_Hide_Deer, 2 });

	TestTrue(TEXT("Inspecting the first carried stack should succeed."), Player->InspectInventoryStack(0));
	TestTrue(TEXT("Inspect action should expose menu feedback."), Player->HasActiveMenuFeedback());
	TestTrue(TEXT("Inspect feedback should mention the item tag."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Resource.Meat.Deer")));

	TestEqual(TEXT("Inventory selection should start on the first stack."), Player->GetSelectedInventoryIndex(), 0);
	Player->SelectInventoryDelta(1);
	TestEqual(TEXT("Inventory selection should move to the second stack."), Player->GetSelectedInventoryIndex(), 1);
	TestTrue(TEXT("Inspecting the selected carried stack should succeed."), Player->InspectSelectedInventoryStack());
	TestTrue(TEXT("Selected inspect feedback should mention the selected item tag."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Resource.Hide.Deer")));

	TestTrue(TEXT("Dropping one item from the selected stack should succeed."), Player->DropSelectedInventoryStackQuantity(1));
	TestEqual(TEXT("Meat should remain untouched after dropping selected hide."), Player->GetResourceCarryComponent()->GetQuantityForTag(TAG_HS_Resource_Meat_Deer), 2);
	TestEqual(TEXT("One hide should remain after dropping one selected hide."), Player->GetResourceCarryComponent()->GetQuantityForTag(TAG_HS_Resource_Hide_Deer), 1);
	TestTrue(TEXT("Drop feedback should mention the selected dropped item."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Dropped Resource.Hide.Deer")));

	TestTrue(TEXT("Dropping the remaining selected stack should succeed."), Player->DropSelectedInventoryStackQuantity(999));
	TestEqual(TEXT("No hide should remain after dropping the selected stack."), Player->GetResourceCarryComponent()->GetQuantityForTag(TAG_HS_Resource_Hide_Deer), 0);
	TestEqual(TEXT("Inventory selection should clamp back to the remaining stack."), Player->GetSelectedInventoryIndex(), 0);
	TestFalse(TEXT("Using an unsupported resource stack should fail cleanly."), Player->UseSelectedInventoryStack());
	TestTrue(TEXT("Unsupported use feedback should explain the item cannot be used."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("cannot be used")));

	TestTrue(TEXT("Dropping the remaining stack should succeed."), Player->DropInventoryStackQuantity(0, 999));
	TestEqual(TEXT("No meat should remain after dropping the stack."), Player->GetResourceCarryComponent()->GetQuantityForTag(TAG_HS_Resource_Meat_Deer), 0);
	TestFalse(TEXT("Inspecting an empty row should fail."), Player->InspectInventoryStack(0));
	TestTrue(TEXT("Empty inspect should explain the missing slot."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("No carried item")));

	Player->GetResourceCarryComponent()->AddStack(FHSItemStack{ TAG_HS_Item_Bow_Basic, 1 });
	TestTrue(TEXT("Using the bow stack should equip the bow."), Player->UseSelectedInventoryStack());
	TestTrue(TEXT("Player equipment should report bow equipped."), Player->GetPlayerEquipmentComponent() && Player->GetPlayerEquipmentComponent()->IsBowEquipped());
	TestTrue(TEXT("Bow use feedback should report equipped state."), Player->GetLastMenuFeedback().ToString().Contains(TEXT("Bow equipped")));

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
	TestFalse(TEXT("Minor upgrade should be blocked before claim."), Player->CanStartBasicHouseMinorUpgradeFromBuildMenu(&Reason));
	TestTrue(TEXT("Blocked upgrade reason should mention settlement claim."), Reason.ToString().Contains(TEXT("Claim")));
	TestTrue(TEXT("Basic campsite should be available without a settlement claim."), Player->CanStartBasicCampsiteFromBuildMenu(&Reason));
	TestTrue(TEXT("Campsite reason should mention temporary placement."), Reason.ToString().Contains(TEXT("temporary")));
	TestEqual(TEXT("Build menu should expose the core foundation catalog."), Player->GetBuildMenuEntryCount(), 10);
	TestTrue(TEXT("Build menu entry 3 should remain the campsite shortcut."), Player->GetBuildMenuEntryLabel(2).ToString().Contains(TEXT("Campsite")));
	TestTrue(TEXT("Build menu should include storage hut schema."), Player->GetBuildMenuEntryTag(5) == TAG_HS_Building_StorageHut);
	TestTrue(TEXT("Build menu should include drying rack schema."), Player->GetBuildMenuEntryTag(9) == TAG_HS_Building_BasicDryingRack);

	Player->GetSettlementPlanComponent()->RestoreSettlementPlan(true, FVector::ZeroVector, {});
	TestTrue(TEXT("Basic house should be available after claim."), Player->CanStartBasicHouseFromBuildMenu(&Reason));
	TestTrue(TEXT("Available build reason should mention readiness."), Reason.ToString().Contains(TEXT("Ready")));
	TestFalse(TEXT("Minor upgrade should be blocked before a completed house."), Player->CanStartBasicHouseMinorUpgradeFromBuildMenu(&Reason));
	TestTrue(TEXT("Blocked upgrade reason should mention a prerequisite."), Reason.ToString().Contains(TEXT("Complete")));
	Player->SelectBuildMenuDelta(1);
	Player->SelectBuildMenuDelta(1);
	TestEqual(TEXT("Build menu selection should support the campsite entry."), Player->GetSelectedBuildMenuIndex(), 2);

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
	TestTrue(TEXT("Minor upgrade should be available after house completion."), Player->CanStartBasicHouseMinorUpgradeFromBuildMenu(&Reason));
	TestTrue(TEXT("Available upgrade reason should mention readiness."), Reason.ToString().Contains(TEXT("Ready")));
	TestFalse(TEXT("Mid upgrade should wait for completed minor upgrade."), Player->CanStartBuildMenuEntry(3, &Reason));
	TestTrue(TEXT("Mid upgrade block should mention prerequisite."), Reason.ToString().Contains(TEXT("Complete")));
	TestTrue(TEXT("Storage hut should be available after basic house."), Player->CanStartBuildMenuEntry(5, &Reason));

	FHSConstructionJob CompletedMinorJob;
	CompletedMinorJob.BuildingTag = TAG_HS_Building_BasicHouseMinorUpgrade;
	CompletedMinorJob.State = EHSConstructionJobState::Complete;
	CompletedBuildings.AddTag(TAG_HS_Building_BasicHouseMinorUpgrade);
	Player->GetConstructionComponent()->RestoreConstructionState({ CompletedJob, CompletedMinorJob }, CompletedBuildings);
	TestTrue(TEXT("Mid upgrade should be available after minor upgrade completion."), Player->CanStartBuildMenuEntry(3, &Reason));
	TestFalse(TEXT("Major upgrade should wait for completed mid upgrade."), Player->CanStartBuildMenuEntry(4, &Reason));

	return true;
}

#endif
