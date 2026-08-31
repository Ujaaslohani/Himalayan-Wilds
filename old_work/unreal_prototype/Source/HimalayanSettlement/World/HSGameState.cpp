#include "World/HSGameState.h"

#include "World/HSWorldTimeComponent.h"

AHSGameState::AHSGameState()
{
	WorldTimeComponent = CreateDefaultSubobject<UHSWorldTimeComponent>(TEXT("WorldTime"));
}
