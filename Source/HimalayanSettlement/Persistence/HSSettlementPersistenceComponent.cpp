#include "Persistence/HSSettlementPersistenceComponent.h"

#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Persistence/HSSettlementSaveGame.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "Settlement/HSSettlementUnlockComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

UHSSettlementPersistenceComponent::UHSSettlementPersistenceComponent()
	: DefaultSlotName(TEXT("HimalayanSettlement_Prototype"))
{
	PrimaryComponentTick.bCanEverTick = false;
}

UHSSettlementSaveGame* UHSSettlementPersistenceComponent::CreateSnapshot() const
{
	UHSSettlementSaveGame* SaveGame = Cast<UHSSettlementSaveGame>(UGameplayStatics::CreateSaveGameObject(UHSSettlementSaveGame::StaticClass()));
	return CaptureToSaveGame(SaveGame) ? SaveGame : nullptr;
}

bool UHSSettlementPersistenceComponent::CaptureToSaveGame(UHSSettlementSaveGame* SaveGame) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !SaveGame)
	{
		return false;
	}

	SaveGame->SaveVersion = UHSSettlementSaveGame::CurrentSaveVersion;

	if (const UHSWorldTimeComponent* WorldTime = ResolveWorldTime())
	{
		SaveGame->TimePhase = WorldTime->GetTimePhase();
		SaveGame->ElapsedHalfDays = WorldTime->GetElapsedHalfDays();
	}

	if (const UHSPlayerProgressComponent* Progress = Owner->FindComponentByClass<UHSPlayerProgressComponent>())
	{
		SaveGame->CompletedMilestones = Progress->GetCompletedMilestones();
	}

	if (const UHSResourceCarryComponent* ResourceCarry = Owner->FindComponentByClass<UHSResourceCarryComponent>())
	{
		SaveGame->CarriedItems = ResourceCarry->GetCarriedItems();
	}

	if (const UHSSettlementUnlockComponent* Unlocks = Owner->FindComponentByClass<UHSSettlementUnlockComponent>())
	{
		SaveGame->GrantedUnlocks = Unlocks->GetGrantedUnlocks();
	}

	if (const UHSSettlementPlanComponent* SettlementPlan = Owner->FindComponentByClass<UHSSettlementPlanComponent>())
	{
		SaveGame->bHasSettlementClaim = SettlementPlan->HasSettlementClaim();
		SaveGame->SettlementClaimCenter = SettlementPlan->GetSettlementClaimCenter();
		SaveGame->PendingSchemas = SettlementPlan->GetPendingSchemas();
	}

	if (const UHSConstructionComponent* Construction = Owner->FindComponentByClass<UHSConstructionComponent>())
	{
		SaveGame->ConstructionJobs = Construction->GetConstructionJobs();
		SaveGame->CompletedBuildings = Construction->GetCompletedBuildings();
	}

	if (const UHSAnimalIntelComponent* AnimalIntel = Owner->FindComponentByClass<UHSAnimalIntelComponent>())
	{
		SaveGame->ActiveAnimalLead = AnimalIntel->GetActiveLead();
		SaveGame->LastAnimalLeadReportDay = AnimalIntel->GetLastReportDay();
	}

	if (const UHSPlantEffectComponent* PlantEffect = Owner->FindComponentByClass<UHSPlantEffectComponent>())
	{
		SaveGame->KnownPlants = PlantEffect->GetKnownPlants();
		SaveGame->ActivePlantEffect = PlantEffect->GetActiveEffect();
	}

	return true;
}

bool UHSSettlementPersistenceComponent::RestoreFromSaveGame(const UHSSettlementSaveGame* SaveGame)
{
	AActor* Owner = GetOwner();
	if (!Owner || !SaveGame || !SaveGame->IsSupportedVersion())
	{
		UE_LOG(LogHSSettlement, Warning, TEXT("%s rejected invalid settlement save data."), *GetNameSafe(Owner));
		return false;
	}

	if (UHSWorldTimeComponent* WorldTime = ResolveWorldTime())
	{
		WorldTime->SetTimeState(SaveGame->TimePhase, SaveGame->ElapsedHalfDays);
	}

	if (UHSPlayerProgressComponent* Progress = Owner->FindComponentByClass<UHSPlayerProgressComponent>())
	{
		Progress->ReplaceCompletedMilestones(SaveGame->CompletedMilestones);
	}

	if (UHSResourceCarryComponent* ResourceCarry = Owner->FindComponentByClass<UHSResourceCarryComponent>())
	{
		ResourceCarry->ReplaceCarriedItems(SaveGame->CarriedItems);
	}

	if (UHSSettlementUnlockComponent* Unlocks = Owner->FindComponentByClass<UHSSettlementUnlockComponent>())
	{
		Unlocks->ReplaceGrantedUnlocks(SaveGame->GrantedUnlocks);
	}

	if (UHSSettlementPlanComponent* SettlementPlan = Owner->FindComponentByClass<UHSSettlementPlanComponent>())
	{
		SettlementPlan->RestoreSettlementPlan(SaveGame->bHasSettlementClaim, SaveGame->SettlementClaimCenter, SaveGame->PendingSchemas);
	}

	if (UHSConstructionComponent* Construction = Owner->FindComponentByClass<UHSConstructionComponent>())
	{
		Construction->RestoreConstructionState(SaveGame->ConstructionJobs, SaveGame->CompletedBuildings);
		Construction->BindToWorldTime();
	}

	if (UHSAnimalIntelComponent* AnimalIntel = Owner->FindComponentByClass<UHSAnimalIntelComponent>())
	{
		AnimalIntel->RestoreAnimalIntelState(SaveGame->ActiveAnimalLead, SaveGame->LastAnimalLeadReportDay);
		AnimalIntel->BindToWorldTime();
	}

	if (UHSPlantEffectComponent* PlantEffect = Owner->FindComponentByClass<UHSPlantEffectComponent>())
	{
		PlantEffect->RestorePlantState(SaveGame->KnownPlants, SaveGame->ActivePlantEffect);
		PlantEffect->BindToWorldTime();
	}

	return true;
}

bool UHSSettlementPersistenceComponent::SaveToSlot(const FString& SlotName, int32 UserIndex) const
{
	const FString EffectiveSlotName = SlotName.IsEmpty() ? DefaultSlotName : SlotName;
	UHSSettlementSaveGame* SaveGame = CreateSnapshot();
	if (!SaveGame)
	{
		return false;
	}

	return UGameplayStatics::SaveGameToSlot(SaveGame, EffectiveSlotName, UserIndex);
}

bool UHSSettlementPersistenceComponent::LoadFromSlot(const FString& SlotName, int32 UserIndex)
{
	const FString EffectiveSlotName = SlotName.IsEmpty() ? DefaultSlotName : SlotName;
	USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(EffectiveSlotName, UserIndex);
	return RestoreFromSaveGame(Cast<UHSSettlementSaveGame>(LoadedSave));
}

UHSWorldTimeComponent* UHSSettlementPersistenceComponent::ResolveWorldTime() const
{
	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	return GameState ? GameState->GetWorldTimeComponent() : nullptr;
}
