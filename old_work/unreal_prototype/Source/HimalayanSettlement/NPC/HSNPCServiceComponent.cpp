#include "NPC/HSNPCServiceComponent.h"

#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "NPC/HSNPCActor.h"
#include "Building/HSBuildingDefinition.h"
#include "Resources/HSItemStack.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Player/HSPlayerHealthComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

#define LOCTEXT_NAMESPACE "HSNPCServiceComponent"

UHSNPCServiceComponent::UHSNPCServiceComponent()
	: SelectedServiceIndex(0)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSNPCServiceComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToWorldTime();
}

void UHSNPCServiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHSWorldTimeComponent* WorldTime = BoundWorldTime.Get())
	{
		WorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSNPCServiceComponent::HandleTimeAdvanced);
	}

	BoundWorldTime.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UHSNPCServiceComponent::HasAnyAvailableService(const AActor* Interactor) const
{
	FHSNPCServiceOption ServiceOption;
	return GetBestServiceOption(Interactor, ServiceOption);
}

bool UHSNPCServiceComponent::GetBestServiceOption(const AActor* Interactor, FHSNPCServiceOption& OutServiceOption) const
{
	TArray<FHSNPCServiceOption> ServiceOptions;
	QueryServiceOptions(Interactor, ServiceOptions);

	bool bFoundService = false;
	for (const FHSNPCServiceOption& ServiceOption : ServiceOptions)
	{
		if (!ServiceOption.bAvailable)
		{
			continue;
		}

		if (!bFoundService || ServiceOption.Priority > OutServiceOption.Priority)
		{
			OutServiceOption = ServiceOption;
			bFoundService = true;
		}
	}

	return bFoundService;
}

FText UHSNPCServiceComponent::GetCurrentRoutineLabel() const
{
	const AHSNPCActor* NPC = Cast<AHSNPCActor>(GetOwner());
	const UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	const bool bNight = WorldTime && WorldTime->GetTimePhase() == EHSTimePhase::Night;
	if (!NPC)
	{
		return FText::GetEmpty();
	}

	if (NPC->GetProfessionTag() == TAG_HS_NPC_Profession_Builder)
	{
		return bNight ? LOCTEXT("BuilderNightRoutine", "Resting before build work") : LOCTEXT("BuilderDayRoutine", "Working near the build yard");
	}
	if (NPC->GetProfessionTag() == TAG_HS_NPC_Profession_Hunter)
	{
		return bNight ? LOCTEXT("HunterNightRoutine", "Checking trail reports") : LOCTEXT("HunterDayRoutine", "Scouting animal signs");
	}
	if (NPC->GetProfessionTag() == TAG_HS_NPC_Profession_Herbalist)
	{
		return bNight ? LOCTEXT("HerbalistNightRoutine", "Drying herbs") : LOCTEXT("HerbalistDayRoutine", "Tending useful plants");
	}
	if (NPC->GetProfessionTag() == TAG_HS_NPC_Profession_Soldier)
	{
		return bNight ? LOCTEXT("SoldierNightRoutine", "Keeping night watch") : LOCTEXT("SoldierDayRoutine", "Maintaining weapons");
	}

	return FText::GetEmpty();
}

bool UHSNPCServiceComponent::GetServiceOption(FGameplayTag ServiceTag, const AActor* Interactor, FHSNPCServiceOption& OutServiceOption) const
{
	if (!ServiceTag.IsValid())
	{
		return false;
	}

	TArray<FHSNPCServiceOption> ServiceOptions;
	QueryServiceOptions(Interactor, ServiceOptions);
	for (const FHSNPCServiceOption& ServiceOption : ServiceOptions)
	{
		if (ServiceOption.ServiceTag == ServiceTag)
		{
			OutServiceOption = ServiceOption;
			return true;
		}
	}

	return false;
}

bool UHSNPCServiceComponent::GetSelectedServiceOption(const AActor* Interactor, FHSNPCServiceOption& OutServiceOption) const
{
	TArray<FHSNPCServiceOption> ServiceOptions;
	QueryAvailableServiceOptions(Interactor, ServiceOptions);
	if (ServiceOptions.Num() <= 0)
	{
		return false;
	}

	OutServiceOption = ServiceOptions[FMath::Clamp(SelectedServiceIndex, 0, ServiceOptions.Num() - 1)];
	return true;
}

void UHSNPCServiceComponent::QueryServiceOptions(const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const
{
	OutServiceOptions.Reset();

	const AHSNPCActor* NPC = Cast<AHSNPCActor>(GetOwner());
	if (!NPC || !NPC->GetProfessionTag().IsValid())
	{
		return;
	}

	AddBuilderService(*NPC, Interactor, OutServiceOptions);
	AddHunterService(*NPC, Interactor, OutServiceOptions);
	AddHerbalistService(*NPC, Interactor, OutServiceOptions);
	AddSoldierService(*NPC, Interactor, OutServiceOptions);
}

void UHSNPCServiceComponent::QueryAvailableServiceOptions(const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const
{
	TArray<FHSNPCServiceOption> ServiceOptions;
	QueryServiceOptions(Interactor, ServiceOptions);

	OutServiceOptions.Reset();
	for (const FHSNPCServiceOption& ServiceOption : ServiceOptions)
	{
		if (ServiceOption.bAvailable)
		{
			OutServiceOptions.Add(ServiceOption);
		}
	}

	OutServiceOptions.Sort([](const FHSNPCServiceOption& Left, const FHSNPCServiceOption& Right)
	{
		return Left.Priority > Right.Priority;
	});
}

bool UHSNPCServiceComponent::SelectServiceDelta(int32 Delta, const AActor* Interactor)
{
	TArray<FHSNPCServiceOption> ServiceOptions;
	QueryAvailableServiceOptions(Interactor, ServiceOptions);
	if (ServiceOptions.Num() <= 0)
	{
		SelectedServiceIndex = 0;
		return false;
	}

	SelectedServiceIndex = FMath::Clamp(SelectedServiceIndex + Delta, 0, ServiceOptions.Num() - 1);
	return true;
}

bool UHSNPCServiceComponent::TryExecuteBestService(AActor* Interactor, FHSNPCServiceOption* OutExecutedService)
{
	FHSNPCServiceOption ServiceOption;
	if (!GetBestServiceOption(Interactor, ServiceOption))
	{
		return false;
	}

	const bool bExecuted = TryExecuteService(ServiceOption.ServiceTag, Interactor);
	if (bExecuted && OutExecutedService)
	{
		*OutExecutedService = ServiceOption;
	}

	return bExecuted;
}

bool UHSNPCServiceComponent::TryExecuteSelectedService(AActor* Interactor, FHSNPCServiceOption* OutExecutedService)
{
	FHSNPCServiceOption ServiceOption;
	if (!GetSelectedServiceOption(Interactor, ServiceOption))
	{
		return false;
	}

	const bool bExecuted = TryExecuteService(ServiceOption.ServiceTag, Interactor);
	if (bExecuted && OutExecutedService)
	{
		*OutExecutedService = ServiceOption;
	}

	return bExecuted;
}

bool UHSNPCServiceComponent::TryExecuteService(FGameplayTag ServiceTag, AActor* Interactor)
{
	FHSNPCServiceOption ServiceOption;
	if (!GetServiceOption(ServiceTag, Interactor, ServiceOption) || !ServiceOption.bAvailable)
	{
		return false;
	}

	if (ServiceTag == TAG_HS_NPC_Service_Builder_StartConstruction)
	{
		return ExecuteBuilderService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Builder_PlanMinorUpgrade)
	{
		return ExecuteBuilderMinorUpgradeService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Hunter_ReportAnimalLead)
	{
		return ExecuteHunterService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Hunter_RefreshAnimalLead)
	{
		return ExecuteHunterRefreshLeadService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Herbalist_TeachSpeedHerb)
	{
		return ExecuteHerbalistPlantService(Interactor, TAG_HS_Plant_SpeedHerb);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Herbalist_TeachScentHerb)
	{
		return ExecuteHerbalistPlantService(Interactor, TAG_HS_Plant_ScentHerb);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Herbalist_TeachSenseHerb)
	{
		return ExecuteHerbalistPlantService(Interactor, TAG_HS_Plant_SenseHerb);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Herbalist_RestoreHealth)
	{
		return ExecuteHerbalistRestoreHealthService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Soldier_ProvideBasicWeapons)
	{
		return ExecuteSoldierService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Soldier_ProvideArrows)
	{
		return ExecuteSoldierArrowsService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Soldier_ProvideSpear)
	{
		return ExecuteSoldierSpearService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Soldier_ProvideReinforcedBow)
	{
		return ExecuteSoldierReinforcedBowService(Interactor);
	}

	return false;
}

void UHSNPCServiceComponent::BindToWorldTime()
{
	UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	if (!WorldTime || BoundWorldTime.Get() == WorldTime)
	{
		return;
	}

	if (UHSWorldTimeComponent* PreviousWorldTime = BoundWorldTime.Get())
	{
		PreviousWorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSNPCServiceComponent::HandleTimeAdvanced);
	}

	BoundWorldTime = WorldTime;
	WorldTime->OnTimeAdvanced.AddUniqueDynamic(this, &UHSNPCServiceComponent::HandleTimeAdvanced);
}

void UHSNPCServiceComponent::HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays)
{
	OnServicesRefreshed.Broadcast();
}

void UHSNPCServiceComponent::AddBuilderService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const
{
	if (NPC.GetProfessionTag() != TAG_HS_NPC_Profession_Builder)
	{
		return;
	}

	const UHSConstructionComponent* ConstructionComponent = Interactor ? Interactor->FindComponentByClass<UHSConstructionComponent>() : nullptr;
	const UHSSettlementPlanComponent* SettlementPlanComponent = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;

	FHSNPCServiceOption ServiceOption;
	ServiceOption.ServiceTag = TAG_HS_NPC_Service_Builder_StartConstruction;
	ServiceOption.ProfessionTag = TAG_HS_NPC_Profession_Builder;
	ServiceOption.Prompt = LOCTEXT("StartBuilderConstructionPrompt", "Start construction");
	ServiceOption.bAvailable = ConstructionComponent && SettlementPlanComponent && SettlementPlanComponent->HasAnyPendingSchema() && !ConstructionComponent->HasActiveJob();
	ServiceOption.Priority = 100;
	OutServiceOptions.Add(ServiceOption);

	FHSNPCServiceOption UpgradeOption;
	UpgradeOption.ServiceTag = TAG_HS_NPC_Service_Builder_PlanMinorUpgrade;
	UpgradeOption.ProfessionTag = TAG_HS_NPC_Profession_Builder;
	UpgradeOption.Prompt = LOCTEXT("PlanMinorUpgradePrompt", "Plan minor house upgrade");
	UpgradeOption.bAvailable = ConstructionComponent
		&& SettlementPlanComponent
		&& ConstructionComponent->HasCompletedBuilding(TAG_HS_Building_BasicHouse)
		&& !ConstructionComponent->HasCompletedBuilding(TAG_HS_Building_BasicHouseMinorUpgrade)
		&& !ConstructionComponent->HasActiveJob()
		&& !SettlementPlanComponent->HasPendingSchema(TAG_HS_Building_BasicHouseMinorUpgrade);
	UpgradeOption.Priority = 80;
	OutServiceOptions.Add(UpgradeOption);
}

void UHSNPCServiceComponent::AddHunterService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const
{
	if (NPC.GetProfessionTag() != TAG_HS_NPC_Profession_Hunter)
	{
		return;
	}

	const UHSAnimalIntelComponent* AnimalIntelComponent = Interactor ? Interactor->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;

	FHSNPCServiceOption ServiceOption;
	ServiceOption.ServiceTag = TAG_HS_NPC_Service_Hunter_ReportAnimalLead;
	ServiceOption.ProfessionTag = TAG_HS_NPC_Profession_Hunter;
	ServiceOption.Prompt = LOCTEXT("AskHunterAnimalSignsPrompt", "Ask about animal signs");
	ServiceOption.bAvailable = AnimalIntelComponent && AnimalIntelComponent->CanRequestDailyAnimalLead();
	ServiceOption.Priority = 40;
	OutServiceOptions.Add(ServiceOption);

	FHSNPCServiceOption RefreshOption;
	RefreshOption.ServiceTag = TAG_HS_NPC_Service_Hunter_RefreshAnimalLead;
	RefreshOption.ProfessionTag = TAG_HS_NPC_Profession_Hunter;
	RefreshOption.Prompt = LOCTEXT("RefreshHunterAnimalSignsPrompt", "Refresh animal marker");
	RefreshOption.bAvailable = AnimalIntelComponent && AnimalIntelComponent->HasActiveLead();
	RefreshOption.Priority = 35;
	OutServiceOptions.Add(RefreshOption);
}

void UHSNPCServiceComponent::AddHerbalistService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const
{
	if (NPC.GetProfessionTag() != TAG_HS_NPC_Profession_Herbalist)
	{
		return;
	}

	const UHSPlantEffectComponent* PlantEffectComponent = Interactor ? Interactor->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;

	FHSNPCServiceOption ServiceOption;
	ServiceOption.ServiceTag = TAG_HS_NPC_Service_Herbalist_TeachSpeedHerb;
	ServiceOption.ProfessionTag = TAG_HS_NPC_Profession_Herbalist;
	ServiceOption.Prompt = LOCTEXT("LearnPlantKnowledgePrompt", "Learn plant knowledge");
	ServiceOption.bAvailable = PlantEffectComponent && !PlantEffectComponent->HasPlantKnowledge(TAG_HS_Plant_SpeedHerb);
	ServiceOption.Priority = 30;
	OutServiceOptions.Add(ServiceOption);

	FHSNPCServiceOption ScentOption;
	ScentOption.ServiceTag = TAG_HS_NPC_Service_Herbalist_TeachScentHerb;
	ScentOption.ProfessionTag = TAG_HS_NPC_Profession_Herbalist;
	ScentOption.Prompt = LOCTEXT("LearnScentHerbPrompt", "Learn scent herb");
	ScentOption.bAvailable = PlantEffectComponent && !PlantEffectComponent->HasPlantKnowledge(TAG_HS_Plant_ScentHerb);
	ScentOption.Priority = 29;
	OutServiceOptions.Add(ScentOption);

	FHSNPCServiceOption SenseOption;
	SenseOption.ServiceTag = TAG_HS_NPC_Service_Herbalist_TeachSenseHerb;
	SenseOption.ProfessionTag = TAG_HS_NPC_Profession_Herbalist;
	SenseOption.Prompt = LOCTEXT("LearnSenseHerbPrompt", "Learn sense herb");
	SenseOption.bAvailable = PlantEffectComponent && !PlantEffectComponent->HasPlantKnowledge(TAG_HS_Plant_SenseHerb);
	SenseOption.Priority = 28;
	OutServiceOptions.Add(SenseOption);

	const UHSPlayerHealthComponent* HealthComponent = Interactor ? Interactor->FindComponentByClass<UHSPlayerHealthComponent>() : nullptr;
	FHSNPCServiceOption HealOption;
	HealOption.ServiceTag = TAG_HS_NPC_Service_Herbalist_RestoreHealth;
	HealOption.ProfessionTag = TAG_HS_NPC_Profession_Herbalist;
	HealOption.Prompt = LOCTEXT("RestoreHealthPrompt", "Restore health");
	HealOption.bAvailable = HealthComponent && !HealthComponent->IsDead() && HealthComponent->GetCurrentHealth() < HealthComponent->GetMaxHealth();
	HealOption.Priority = 45;
	OutServiceOptions.Add(HealOption);
}

void UHSNPCServiceComponent::AddSoldierService(const AHSNPCActor& NPC, const AActor* Interactor, TArray<FHSNPCServiceOption>& OutServiceOptions) const
{
	if (NPC.GetProfessionTag() != TAG_HS_NPC_Profession_Soldier)
	{
		return;
	}

	const UHSResourceCarryComponent* ResourceCarry = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;

	FHSNPCServiceOption ServiceOption;
	ServiceOption.ServiceTag = TAG_HS_NPC_Service_Soldier_ProvideBasicWeapons;
	ServiceOption.ProfessionTag = TAG_HS_NPC_Profession_Soldier;
	ServiceOption.Prompt = LOCTEXT("RequestBasicWeaponsPrompt", "Request basic weapons");
	ServiceOption.bAvailable = ResourceCarry && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) <= 0;
	ServiceOption.Priority = 30;
	OutServiceOptions.Add(ServiceOption);

	FHSNPCServiceOption ArrowOption;
	ArrowOption.ServiceTag = TAG_HS_NPC_Service_Soldier_ProvideArrows;
	ArrowOption.ProfessionTag = TAG_HS_NPC_Profession_Soldier;
	ArrowOption.Prompt = LOCTEXT("RequestExtraArrowsPrompt", "Request extra arrows");
	ArrowOption.bAvailable = ResourceCarry && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) > 0 && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Arrow_Basic) < 6;
	ArrowOption.Priority = 20;
	OutServiceOptions.Add(ArrowOption);

	FHSNPCServiceOption SpearOption;
	SpearOption.ServiceTag = TAG_HS_NPC_Service_Soldier_ProvideSpear;
	SpearOption.ProfessionTag = TAG_HS_NPC_Profession_Soldier;
	SpearOption.Prompt = LOCTEXT("RequestSpearPrompt", "Request spear");
	SpearOption.bAvailable = ResourceCarry && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) > 0 && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Spear_Basic) <= 0;
	SpearOption.Priority = 18;
	OutServiceOptions.Add(SpearOption);

	FHSNPCServiceOption ReinforcedBowOption;
	ReinforcedBowOption.ServiceTag = TAG_HS_NPC_Service_Soldier_ProvideReinforcedBow;
	ReinforcedBowOption.ProfessionTag = TAG_HS_NPC_Profession_Soldier;
	ReinforcedBowOption.Prompt = LOCTEXT("RequestReinforcedBowPrompt", "Request reinforced bow");
	ReinforcedBowOption.bAvailable = ResourceCarry && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) > 0 && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Reinforced) <= 0;
	ReinforcedBowOption.Priority = 16;
	OutServiceOptions.Add(ReinforcedBowOption);
}

bool UHSNPCServiceComponent::ExecuteBuilderService(AActor* Interactor) const
{
	UHSConstructionComponent* ConstructionComponent = Interactor ? Interactor->FindComponentByClass<UHSConstructionComponent>() : nullptr;
	UHSSettlementPlanComponent* SettlementPlanComponent = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	return ConstructionComponent && SettlementPlanComponent && ConstructionComponent->StartNextPendingSchema(SettlementPlanComponent);
}

bool UHSNPCServiceComponent::ExecuteBuilderMinorUpgradeService(AActor* Interactor) const
{
	UHSConstructionComponent* ConstructionComponent = Interactor ? Interactor->FindComponentByClass<UHSConstructionComponent>() : nullptr;
	UHSSettlementPlanComponent* SettlementPlanComponent = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	if (!ConstructionComponent || !SettlementPlanComponent)
	{
		return false;
	}

	UHSBuildingDefinition* MinorUpgradeDefinition = NewObject<UHSBuildingDefinition>(const_cast<UHSNPCServiceComponent*>(this));
	MinorUpgradeDefinition->DisplayName = LOCTEXT("MinorUpgradeDefinitionName", "Minor House Upgrade");
	MinorUpgradeDefinition->BuildingTag = TAG_HS_Building_BasicHouseMinorUpgrade;

	FHSPendingBuildingSchema PendingSchema;
	const FVector UpgradeLocation = SettlementPlanComponent->GetSettlementClaimCenter() + FVector(220.0f, 120.0f, 0.0f);
	return SettlementPlanComponent->PlaceSchema(MinorUpgradeDefinition, UpgradeLocation, 0.0f, PendingSchema);
}

bool UHSNPCServiceComponent::ExecuteHunterService(AActor* Interactor) const
{
	UHSAnimalIntelComponent* AnimalIntelComponent = Interactor ? Interactor->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;
	return AnimalIntelComponent && AnimalIntelComponent->RequestDailyAnimalLead();
}

bool UHSNPCServiceComponent::ExecuteHunterRefreshLeadService(AActor* Interactor) const
{
	UHSAnimalIntelComponent* AnimalIntelComponent = Interactor ? Interactor->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;
	return AnimalIntelComponent && AnimalIntelComponent->RefreshActiveAnimalLead();
}

bool UHSNPCServiceComponent::ExecuteHerbalistService(AActor* Interactor) const
{
	return ExecuteHerbalistPlantService(Interactor, TAG_HS_Plant_SpeedHerb);
}

bool UHSNPCServiceComponent::ExecuteHerbalistPlantService(AActor* Interactor, FGameplayTag PlantTag) const
{
	UHSPlantEffectComponent* PlantEffectComponent = Interactor ? Interactor->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;
	return PlantEffectComponent && PlantEffectComponent->LearnPlant(PlantTag);
}

bool UHSNPCServiceComponent::ExecuteHerbalistRestoreHealthService(AActor* Interactor) const
{
	UHSPlayerHealthComponent* HealthComponent = Interactor ? Interactor->FindComponentByClass<UHSPlayerHealthComponent>() : nullptr;
	return HealthComponent && HealthComponent->RestoreHealth(35.0f) > 0.0f;
}

bool UHSNPCServiceComponent::ExecuteSoldierService(AActor* Interactor) const
{
	UHSResourceCarryComponent* ResourceCarry = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
	if (!ResourceCarry || ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) > 0)
	{
		return false;
	}

	ResourceCarry->AddStack(FHSItemStack{ TAG_HS_Item_Bow_Basic, 1 });
	ResourceCarry->AddStack(FHSItemStack{ TAG_HS_Item_Arrow_Basic, 12 });
	if (UHSPlayerEquipmentComponent* Equipment = Interactor ? Interactor->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr)
	{
		Equipment->SetBowEquipped(true);
	}

	return true;
}

bool UHSNPCServiceComponent::ExecuteSoldierArrowsService(AActor* Interactor) const
{
	UHSResourceCarryComponent* ResourceCarry = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
	if (!ResourceCarry || ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) <= 0)
	{
		return false;
	}

	ResourceCarry->AddStack(FHSItemStack{ TAG_HS_Item_Arrow_Basic, 6 });
	return true;
}

bool UHSNPCServiceComponent::ExecuteSoldierSpearService(AActor* Interactor) const
{
	UHSResourceCarryComponent* ResourceCarry = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
	if (!ResourceCarry || ResourceCarry->GetQuantityForTag(TAG_HS_Item_Spear_Basic) > 0)
	{
		return false;
	}

	ResourceCarry->AddStack(FHSItemStack{ TAG_HS_Item_Spear_Basic, 1 });
	return true;
}

bool UHSNPCServiceComponent::ExecuteSoldierReinforcedBowService(AActor* Interactor) const
{
	UHSResourceCarryComponent* ResourceCarry = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
	if (!ResourceCarry || ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Reinforced) > 0 || ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) <= 0)
	{
		return false;
	}

	ResourceCarry->AddStack(FHSItemStack{ TAG_HS_Item_Bow_Reinforced, 1 });
	return true;
}

UHSWorldTimeComponent* UHSNPCServiceComponent::ResolveWorldTime() const
{
	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	return GameState ? GameState->GetWorldTimeComponent() : nullptr;
}

#undef LOCTEXT_NAMESPACE
