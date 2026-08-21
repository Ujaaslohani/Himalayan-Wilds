#include "Persistence/HSSettlementSaveGame.h"

UHSSettlementSaveGame::UHSSettlementSaveGame()
	: SaveVersion(CurrentSaveVersion)
	, TimePhase(EHSTimePhase::Day)
	, ElapsedHalfDays(0)
	, bHasSettlementClaim(false)
	, SettlementClaimCenter(FVector::ZeroVector)
{
}

bool UHSSettlementSaveGame::IsSupportedVersion() const
{
	return SaveVersion > 0 && SaveVersion <= CurrentSaveVersion;
}
