#include "Player/HSGameMode.h"

#include "Player/HSPlayerCharacter.h"

AHSGameMode::AHSGameMode()
{
	DefaultPawnClass = AHSPlayerCharacter::StaticClass();
}
