#include "NPC/HSNPCActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "NPC/HSNPCDefinition.h"
#include "NPC/HSNPCServiceComponent.h"
#include "Quests/HSQuestHandoffComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSNPCActor"

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
	NPCServiceComponent = CreateDefaultSubobject<UHSNPCServiceComponent>(TEXT("NPCService"));
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
	if (NPCServiceComponent)
	{
		FHSNPCServiceOption ServiceOption;
		if (NPCServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Builder_StartConstruction, Interactor, ServiceOption) && ServiceOption.bAvailable)
		{
			return ServiceOption.Prompt;
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

	if (NPCServiceComponent)
	{
		FHSNPCServiceOption ServiceOption;
		if (NPCServiceComponent->GetSelectedServiceOption(Interactor, ServiceOption) || NPCServiceComponent->GetBestServiceOption(Interactor, ServiceOption))
		{
			return ServiceOption.Prompt;
		}
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Name"), GetDisplayName());
	const FText BasePrompt = FText::Format(InteractionPromptFormat, Arguments);
	if (NPCServiceComponent)
	{
		const FText RoutineLabel = NPCServiceComponent->GetCurrentRoutineLabel();
		if (!RoutineLabel.IsEmpty())
		{
			return FText::Format(LOCTEXT("NPCPromptWithRoutine", "{0}: {1}"), BasePrompt, RoutineLabel);
		}
	}
	return BasePrompt;
}

bool AHSNPCActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return bCanTalk;
}

void AHSNPCActor::Interact_Implementation(AActor* Interactor)
{
	++InteractionCount;
	FHSNPCServiceOption ExecutedService;
	const bool bExecutedServiceBeforeQuest = NPCServiceComponent && NPCServiceComponent->TryExecuteService(TAG_HS_NPC_Service_Builder_StartConstruction, Interactor);
	if (bExecutedServiceBeforeQuest)
	{
		NPCServiceComponent->GetServiceOption(TAG_HS_NPC_Service_Builder_StartConstruction, Interactor, ExecutedService);
	}
	const bool bAdvancedQuest = !bExecutedServiceBeforeQuest && QuestHandoffComponent && QuestHandoffComponent->TryAdvanceQuest(Interactor);
	const bool bExecutedServiceAfterQuest = !bExecutedServiceBeforeQuest && !bAdvancedQuest && NPCServiceComponent && NPCServiceComponent->TryExecuteSelectedService(Interactor, &ExecutedService);
	const bool bExecutedService = bExecutedServiceBeforeQuest || bExecutedServiceAfterQuest;
	UE_LOG(
		LogHSInteraction,
		Log,
		TEXT("%s started conversation with %s. Service executed: %s (%s). Quest advanced: %s. Interaction count: %d."),
		*GetNameSafe(Interactor),
		*GetNameSafe(this),
		bExecutedService ? TEXT("true") : TEXT("false"),
		*ExecutedService.ServiceTag.ToString(),
		bAdvancedQuest ? TEXT("true") : TEXT("false"),
		InteractionCount);
}

#undef LOCTEXT_NAMESPACE
