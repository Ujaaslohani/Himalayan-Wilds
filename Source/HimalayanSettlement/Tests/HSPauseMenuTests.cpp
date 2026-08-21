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

	Player->OpenPauseMenu();
	TestTrue(TEXT("OpenPauseMenu should open the pause menu."), Player->IsPauseMenuOpen());
	TestFalse(TEXT("Opening pause menu should not open options by default."), Player->IsPauseOptionsOpen());

	Player->TogglePauseOptions();
	TestTrue(TEXT("Options should toggle open while the pause menu is open."), Player->IsPauseOptionsOpen());

	Player->ClosePauseMenu();
	TestFalse(TEXT("ClosePauseMenu should close the pause menu."), Player->IsPauseMenuOpen());
	TestFalse(TEXT("Closing pause menu should also close options."), Player->IsPauseOptionsOpen());

	return true;
}

#endif
