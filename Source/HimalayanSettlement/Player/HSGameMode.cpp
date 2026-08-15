#include "Player/HSGameMode.h"

#include "Player/HSPlayerCharacter.h"
#include "UI/HSHUD.h"

AHSGameMode::AHSGameMode()
{
	DefaultPawnClass = AHSPlayerCharacter::StaticClass();
	HUDClass = AHSHUD::StaticClass();
}
