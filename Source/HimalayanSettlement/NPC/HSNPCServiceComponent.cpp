#include "NPC/HSNPCServiceComponent.h"

#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "NPC/HSNPCActor.h"
#include "Resources/HSItemStack.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

#define LOCTEXT_NAMESPACE "HSNPCServiceComponent"

UHSNPCServiceComponent::UHSNPCServiceComponent()
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

	if (ServiceTag == TAG_HS_NPC_Service_Hunter_ReportAnimalLead)
	{
		return ExecuteHunterService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Herbalist_TeachSpeedHerb)
	{
		return ExecuteHerbalistService(Interactor);
	}

	if (ServiceTag == TAG_HS_NPC_Service_Soldier_ProvideBasicWeapons)
	{
		return ExecuteSoldierService(Interactor);
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
}

bool UHSNPCServiceComponent::ExecuteBuilderService(AActor* Interactor) const
{
	UHSConstructionComponent* ConstructionComponent = Interactor ? Interactor->FindComponentByClass<UHSConstructionComponent>() : nullptr;
	UHSSettlementPlanComponent* SettlementPlanComponent = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	return ConstructionComponent && SettlementPlanComponent && ConstructionComponent->StartNextPendingSchema(SettlementPlanComponent);
}

bool UHSNPCServiceComponent::ExecuteHunterService(AActor* Interactor) const
{
	UHSAnimalIntelComponent* AnimalIntelComponent = Interactor ? Interactor->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;
	return AnimalIntelComponent && AnimalIntelComponent->RequestDailyAnimalLead();
}

bool UHSNPCServiceComponent::ExecuteHerbalistService(AActor* Interactor) const
{
	UHSPlantEffectComponent* PlantEffectComponent = Interactor ? Interactor->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;
	return PlantEffectComponent && PlantEffectComponent->LearnPlant(TAG_HS_Plant_SpeedHerb);
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

UHSWorldTimeComponent* UHSNPCServiceComponent::ResolveWorldTime() const
{
	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	return GameState ? GameState->GetWorldTimeComponent() : nullptr;
}

#undef LOCTEXT_NAMESPACE
