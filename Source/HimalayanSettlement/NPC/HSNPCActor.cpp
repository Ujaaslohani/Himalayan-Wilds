#include "NPC/HSNPCActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "NPC/HSNPCDefinition.h"
#include "Quests/HSQuestHandoffComponent.h"
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
	UE_LOG(
		LogHSInteraction,
		Log,
		TEXT("%s started conversation with %s. Construction started: %s. Quest advanced: %s. Interaction count: %d."),
		*GetNameSafe(Interactor),
		*GetNameSafe(this),
		bStartedConstruction ? TEXT("true") : TEXT("false"),
		bAdvancedQuest ? TEXT("true") : TEXT("false"),
		InteractionCount);
}

#undef LOCTEXT_NAMESPACE
