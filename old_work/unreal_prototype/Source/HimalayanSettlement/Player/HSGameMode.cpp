#include "Player/HSGameMode.h"

#include "Player/HSPlayerCharacter.h"
#include "UI/HSHUD.h"
#include "World/HSGameState.h"

AHSGameMode::AHSGameMode()
{
	DefaultPawnClass = AHSPlayerCharacter::StaticClass();
	GameStateClass = AHSGameState::StaticClass();
	HUDClass = AHSHUD::StaticClass();
}
