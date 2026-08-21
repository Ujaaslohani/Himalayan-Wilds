#include "Persistence/HSSettlementSaveGame.h"

UHSSettlementSaveGame::UHSSettlementSaveGame()
	: SaveVersion(CurrentSaveVersion)
	, TimePhase(EHSTimePhase::Day)
	, ElapsedHalfDays(0)
	, PlayerHealth(100.0f)
	, bBowEquipped(false)
	, bEmergencyLowEndModeEnabled(false)
	, MouseSensitivityPreset(1)
	, MasterVolumePreset(2)
	, bHasSettlementClaim(false)
	, SettlementClaimCenter(FVector::ZeroVector)
	, LastAnimalLeadReportDay(INDEX_NONE)
{
}

bool UHSSettlementSaveGame::IsSupportedVersion() const
{
	return SaveVersion > 0 && SaveVersion <= CurrentSaveVersion;
}
