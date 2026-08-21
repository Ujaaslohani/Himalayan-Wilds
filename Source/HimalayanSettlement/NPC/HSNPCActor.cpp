#include "NPC/HSNPCActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "NPC/HSNPCDefinition.h"
#include "Quests/HSQuestHandoffComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Resources/HSItemStack.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSNPCActor"

namespace
{
bool IsBuilderNPC(const AHSNPCActor& NPC)
{
	return NPC.GetProfessionTag() == TAG_HS_NPC_Profession_Builder;
}

bool IsHunterNPC(const AHSNPCActor& NPC)
{
	return NPC.GetProfessionTag() == TAG_HS_NPC_Profession_Hunter;
}

bool IsHerbalistNPC(const AHSNPCActor& NPC)
{
	return NPC.GetProfessionTag() == TAG_HS_NPC_Profession_Herbalist;
}

bool IsSoldierNPC(const AHSNPCActor& NPC)
{
	return NPC.GetProfessionTag() == TAG_HS_NPC_Profession_Soldier;
}

bool TryStartBuilderConstruction(const AHSNPCActor& Builder, AActor* Interactor)
{
	if (!IsBuilderNPC(Builder) || !Interactor)
	{
		return false;
	}

	UHSConstructionComponent* ConstructionComponent = Interactor->FindComponentByClass<UHSConstructionComponent>();
	UHSSettlementPlanComponent* SettlementPlanComponent = Interactor->FindComponentByClass<UHSSettlementPlanComponent>();
	return ConstructionComponent && SettlementPlanComponent && ConstructionComponent->StartNextPendingSchema(SettlementPlanComponent);
}

bool TryReportHunterAnimalLead(const AHSNPCActor& Hunter, AActor* Interactor)
{
	if (!IsHunterNPC(Hunter) || !Interactor)
	{
		return false;
	}

	UHSAnimalIntelComponent* AnimalIntelComponent = Interactor->FindComponentByClass<UHSAnimalIntelComponent>();
	return AnimalIntelComponent && AnimalIntelComponent->RequestDailyAnimalLead();
}

bool TryTeachHerbalistPlantKnowledge(const AHSNPCActor& Herbalist, AActor* Interactor)
{
	if (!IsHerbalistNPC(Herbalist) || !Interactor)
	{
		return false;
	}

	UHSPlantEffectComponent* PlantEffectComponent = Interactor->FindComponentByClass<UHSPlantEffectComponent>();
	return PlantEffectComponent && PlantEffectComponent->LearnPlant(TAG_HS_Plant_SpeedHerb);
}

bool TryProvideSoldierWeaponSupplies(const AHSNPCActor& Soldier, AActor* Interactor)
{
	if (!IsSoldierNPC(Soldier) || !Interactor)
	{
		return false;
	}

	UHSResourceCarryComponent* ResourceCarry = Interactor->FindComponentByClass<UHSResourceCarryComponent>();
	if (!ResourceCarry || ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) > 0)
	{
		return false;
	}

	ResourceCarry->AddStack(FHSItemStack{ TAG_HS_Item_Bow_Basic, 1 });
	ResourceCarry->AddStack(FHSItemStack{ TAG_HS_Item_Arrow_Basic, 12 });
	return true;
}
}

AHSNPCActor::AHSNPCActor()
	: FallbackDisplayName(LOCTEXT("DefaultHunterName", "Hunter"))
	, InteractionPromptFormat(LOCTEXT("TalkToNPCPrompt", "Talk to {Name}"))
	, bCanTalk(true)
	, InteractionCount(0)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(40.0f, 40.0f, 90.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.8f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(CylinderMesh.Object);
	}

	QuestHandoffComponent = CreateDefaultSubobject<UHSQuestHandoffComponent>(TEXT("QuestHandoff"));
}

FText AHSNPCActor::GetDisplayName() const
{
	return NPCDefinition && !NPCDefinition->DisplayName.IsEmpty() ? NPCDefinition->DisplayName : FallbackDisplayName;
}

void AHSNPCActor::SetNPCDefinition(UHSNPCDefinition* NewNPCDefinition)
{
	NPCDefinition = NewNPCDefinition;
}

FGameplayTag AHSNPCActor::GetProfessionTag() const
{
	return NPCDefinition ? NPCDefinition->ProfessionTag : FGameplayTag();
}

void AHSNPCActor::SetOfferedQuest(UHSQuestDefinition* NewQuestDefinition)
{
	if (QuestHandoffComponent)
	{
		QuestHandoffComponent->SetQuestDefinition(NewQuestDefinition);
	}
}

FText AHSNPCActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	if (IsBuilderNPC(*this))
	{
		const UHSConstructionComponent* ConstructionComponent = Interactor ? Interactor->FindComponentByClass<UHSConstructionComponent>() : nullptr;
		const UHSSettlementPlanComponent* SettlementPlanComponent = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
		if (ConstructionComponent && SettlementPlanComponent && SettlementPlanComponent->HasAnyPendingSchema() && !ConstructionComponent->HasActiveJob())
		{
			return LOCTEXT("StartBuilderConstructionPrompt", "Start construction");
		}
	}

	if (QuestHandoffComponent)
	{
		const FText QuestPrompt = QuestHandoffComponent->GetInteractionPrompt(Interactor);
		if (!QuestPrompt.IsEmpty())
		{
			return QuestPrompt;
		}
	}

	if (IsHunterNPC(*this))
	{
		const UHSAnimalIntelComponent* AnimalIntelComponent = Interactor ? Interactor->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;
		if (AnimalIntelComponent && AnimalIntelComponent->CanRequestDailyAnimalLead())
		{
			return LOCTEXT("AskHunterAnimalSignsPrompt", "Ask about animal signs");
		}
	}

	if (IsHerbalistNPC(*this))
	{
		const UHSPlantEffectComponent* PlantEffectComponent = Interactor ? Interactor->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;
		if (PlantEffectComponent && !PlantEffectComponent->HasPlantKnowledge(TAG_HS_Plant_SpeedHerb))
		{
			return LOCTEXT("LearnPlantKnowledgePrompt", "Learn plant knowledge");
		}
	}

	if (IsSoldierNPC(*this))
	{
		const UHSResourceCarryComponent* ResourceCarry = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
		if (ResourceCarry && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) <= 0)
		{
			return LOCTEXT("RequestBasicWeaponsPrompt", "Request basic weapons");
		}
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Name"), GetDisplayName());
	return FText::Format(InteractionPromptFormat, Arguments);
}

bool AHSNPCActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return bCanTalk;
}

void AHSNPCActor::Interact_Implementation(AActor* Interactor)
{
	++InteractionCount;
	const bool bStartedConstruction = TryStartBuilderConstruction(*this, Interactor);
	const bool bAdvancedQuest = !bStartedConstruction && QuestHandoffComponent && QuestHandoffComponent->TryAdvanceQuest(Interactor);
	const bool bReportedAnimalLead = !bStartedConstruction && !bAdvancedQuest && TryReportHunterAnimalLead(*this, Interactor);
	const bool bTaughtPlantKnowledge = !bStartedConstruction && !bAdvancedQuest && !bReportedAnimalLead && TryTeachHerbalistPlantKnowledge(*this, Interactor);
	const bool bProvidedWeaponSupplies = !bStartedConstruction && !bAdvancedQuest && !bReportedAnimalLead && !bTaughtPlantKnowledge && TryProvideSoldierWeaponSupplies(*this, Interactor);
	UE_LOG(
		LogHSInteraction,
		Log,
		TEXT("%s started conversation with %s. Construction started: %s. Quest advanced: %s. Animal lead reported: %s. Plant knowledge taught: %s. Weapon supplies provided: %s. Interaction count: %d."),
		*GetNameSafe(Interactor),
		*GetNameSafe(this),
		bStartedConstruction ? TEXT("true") : TEXT("false"),
		bAdvancedQuest ? TEXT("true") : TEXT("false"),
		bReportedAnimalLead ? TEXT("true") : TEXT("false"),
		bTaughtPlantKnowledge ? TEXT("true") : TEXT("false"),
		bProvidedWeaponSupplies ? TEXT("true") : TEXT("false"),
		InteractionCount);
}

#undef LOCTEXT_NAMESPACE
