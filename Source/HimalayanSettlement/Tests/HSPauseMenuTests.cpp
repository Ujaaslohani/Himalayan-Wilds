#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/HSPlayerCharacter.h"

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

	return true;
}

#endif
