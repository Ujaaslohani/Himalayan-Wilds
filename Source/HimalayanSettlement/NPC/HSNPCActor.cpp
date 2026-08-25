#include "NPC/HSNPCActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	PrototypeMesh->SetMobility(EComponentMobility::Movable);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
	PrototypeMesh->SetRelativeScale3D(FVector(0.42f, 0.28f, 0.70f));

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(InteractionBounds);
	HeadMesh->SetMobility(EComponentMobility::Movable);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	HeadMesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.28f));

	LegMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LegMesh"));
	LegMesh->SetupAttachment(InteractionBounds);
	LegMesh->SetMobility(EComponentMobility::Movable);
	LegMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LegMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -15.0f));
	LegMesh->SetRelativeScale3D(FVector(0.30f, 0.20f, 0.58f));

	HatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMesh"));
	HatMesh->SetupAttachment(InteractionBounds);
	HatMesh->SetMobility(EComponentMobility::Movable);
	HatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HatMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 118.0f));
	HatMesh->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.22f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		CubeMesh = CubeMeshFinder.Object;
		PrototypeMesh->SetStaticMesh(CubeMesh);
		LegMesh->SetStaticMesh(CubeMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		SphereMesh = SphereMeshFinder.Object;
		HeadMesh->SetStaticMesh(SphereMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshFinder.Succeeded())
	{
		ConeMesh = ConeMeshFinder.Object;
		HatMesh->SetStaticMesh(ConeMesh);
	}

	QuestHandoffComponent = CreateDefaultSubobject<UHSQuestHandoffComponent>(TEXT("QuestHandoff"));
	NPCServiceComponent = CreateDefaultSubobject<UHSNPCServiceComponent>(TEXT("NPCService"));
}

void AHSNPCActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyProfessionVisuals();
}

FText AHSNPCActor::GetDisplayName() const
{
	return NPCDefinition && !NPCDefinition->DisplayName.IsEmpty() ? NPCDefinition->DisplayName : FallbackDisplayName;
}

void AHSNPCActor::SetNPCDefinition(UHSNPCDefinition* NewNPCDefinition)
{
	NPCDefinition = NewNPCDefinition;
	ApplyProfessionVisuals();
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

void AHSNPCActor::ApplyProfessionVisuals()
{
	FLinearColor TunicColor(0.24f, 0.33f, 0.22f, 1.0f);
	FLinearColor HatColor(0.18f, 0.14f, 0.09f, 1.0f);
	if (GetProfessionTag() == TAG_HS_NPC_Profession_Builder)
	{
		TunicColor = FLinearColor(0.46f, 0.30f, 0.13f, 1.0f);
	}
	else if (GetProfessionTag() == TAG_HS_NPC_Profession_Herbalist)
	{
		TunicColor = FLinearColor(0.16f, 0.42f, 0.24f, 1.0f);
	}
	else if (GetProfessionTag() == TAG_HS_NPC_Profession_Soldier)
	{
		TunicColor = FLinearColor(0.35f, 0.35f, 0.38f, 1.0f);
		HatColor = FLinearColor(0.12f, 0.12f, 0.13f, 1.0f);
	}
	else if (GetProfessionTag() == TAG_HS_NPC_Profession_Hunter)
	{
		TunicColor = FLinearColor(0.22f, 0.38f, 0.19f, 1.0f);
	}

	ApplyMeshColor(PrototypeMesh, TunicColor);
	ApplyMeshColor(LegMesh, FLinearColor(0.16f, 0.11f, 0.07f, 1.0f));
	ApplyMeshColor(HeadMesh, FLinearColor(0.50f, 0.34f, 0.22f, 1.0f));
	ApplyMeshColor(HatMesh, HatColor);
}

void AHSNPCActor::ApplyMeshColor(UStaticMeshComponent* MeshComponent, FLinearColor Color)
{
	if (!MeshComponent || HasAnyFlags(RF_ClassDefaultObject) || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

#undef LOCTEXT_NAMESPACE
