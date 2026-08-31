#include "Persistence/HSSettlementSaveGame.h"

UHSSettlementSaveGame::UHSSettlementSaveGame()
	: SaveVersion(CurrentSaveVersion)
	, TimePhase(EHSTimePhase::Day)
	, ElapsedHalfDays(0)
	, PlayerHealth(100.0f)
	, bBowEquipped(false)
	, EquippedWeaponMode(EHSPlayerWeaponMode::Unarmed)
	, bEmergencyLowEndModeEnabled(false)
	, bLowEndStreamingEnabled(true)
	, GraphicsQualityPreset(0)
	, MouseSensitivityPreset(1)
	, MasterVolumePreset(2)
	, bHasSettlementClaim(false)
	, SettlementClaimCenter(FVector::ZeroVector)
	, HouseCapacity(0)
	, VillageExpansionDay(0)
	, bDefenseSystemEnabled(false)
	, bRaidSystemEnabled(false)
	, LastAnimalLeadReportDay(INDEX_NONE)
{
}

bool UHSSettlementSaveGame::IsSupportedVersion() const
{
	return SaveVersion > 0 && SaveVersion <= CurrentSaveVersion;
}
