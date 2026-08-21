#include "UI/HSHUD.h"

#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Settlement/HSBuildPlacementComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

AHSHUD::AHSHUD()
	: PromptVerticalOffset(120.0f)
	, PromptScale(1.15f)
	, CompletionMessageDuration(4.0f)
	, CompletionMessageExpireTime(0.0)
{
}

void AHSHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !PlayerOwner)
	{
		return;
	}

	BindProgressComponent();
	BindAnimalIntelComponent();
	BindPlantEffectComponent();
	BindBuildPlacementComponent();
	BindSettlementPlanComponent();
	BindConstructionComponent();

	if (const AHSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AHSGameState>() : nullptr)
	{
		if (const UHSWorldTimeComponent* WorldTime = GameState->GetWorldTimeComponent())
		{
			const FString TimeText = FString::Printf(TEXT("Time: %s"), *WorldTime->GetTimePhaseText().ToString());
			DrawCenteredTextLine(TimeText, 28.0f, 1.0f, FLinearColor(0.85f, 0.92f, 1.0f, 1.0f));
		}
	}

	const APawn* Pawn = PlayerOwner->GetPawn();
	const UHSBuildPlacementComponent* BuildPlacementComponent = Pawn ? Pawn->FindComponentByClass<UHSBuildPlacementComponent>() : nullptr;
	if (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive())
	{
		const FString BuildText = FString::Printf(TEXT("[Left Mouse/E] Place house   [Q/R] Rotate   [Esc/B] Cancel   %s"), *BuildPlacementComponent->GetPlacementStatusText().ToString());
		const float BuildY = FMath::Max(0.0f, Canvas->ClipY - PromptVerticalOffset);
		DrawCenteredTextLine(BuildText, BuildY, 0.95f, BuildPlacementComponent->CanConfirmPlacement() ? FLinearColor(0.55f, 1.0f, 0.55f, 1.0f) : FLinearColor(1.0f, 0.55f, 0.45f, 1.0f));
	}
	else
	{
		const UHSInteractionSensorComponent* Sensor = Pawn ? Pawn->FindComponentByClass<UHSInteractionSensorComponent>() : nullptr;
		if (Sensor)
		{
			const FText Prompt = Sensor->GetFocusedInteractionPrompt();
			if (!Prompt.IsEmpty())
			{
				const FString PromptText = FString::Printf(TEXT("[E] %s"), *Prompt.ToString());
				const float PromptY = FMath::Max(0.0f, Canvas->ClipY - PromptVerticalOffset);
				DrawCenteredTextLine(PromptText, PromptY, PromptScale, FLinearColor::White);
			}
		}
	}

	if (HasActiveCompletionMessage())
	{
		const float MessageY = FMath::Max(0.0f, Canvas->ClipY - PromptVerticalOffset - 36.0f);
		DrawCenteredTextLine(CompletionMessage.ToString(), MessageY, PromptScale, FLinearColor(0.55f, 1.0f, 0.55f, 1.0f));
	}
}

bool AHSHUD::HasActiveCompletionMessage() const
{
	return !CompletionMessage.IsEmpty() && FPlatformTime::Seconds() < CompletionMessageExpireTime;
}

void AHSHUD::HandleMilestoneCompleted(FGameplayTag MilestoneTag)
{
	if (!MilestoneTag.IsValid())
	{
		return;
	}

	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "MilestoneCompleted", "Completed: {0}"), FText::FromString(MilestoneTag.ToString())));
}

void AHSHUD::HandleSettlementClaimed(FVector ClaimCenter)
{
	ShowCompletionMessage(NSLOCTEXT("HimalayanSettlementHUD", "SettlementClaimed", "Settlement site claimed"));
}

void AHSHUD::HandleBuildingSchemaPlaced(const FHSPendingBuildingSchema& PendingSchema)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "SchemaPending", "Schema pending: {0}"), FText::FromString(PendingSchema.BuildingTag.ToString())));
}

void AHSHUD::HandleConstructionJobStarted(const FHSConstructionJob& ConstructionJob)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "ConstructionStarted", "Construction started: {0}"), FText::FromString(ConstructionJob.BuildingTag.ToString())));
}

void AHSHUD::HandleConstructionJobCompleted(const FHSConstructionJob& ConstructionJob)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "ConstructionComplete", "Construction complete: {0}"), FText::FromString(ConstructionJob.BuildingTag.ToString())));
}

void AHSHUD::HandleAnimalLeadReported(const FHSAnimalLead& AnimalLead)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "AnimalLeadReported", "Animal spotted: {0}"), FText::FromString(AnimalLead.AnimalTag.ToString())));
}

void AHSHUD::HandleAnimalLeadExpired(const FHSAnimalLead& AnimalLead)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "AnimalLeadExpired", "Animal lead expired: {0}"), FText::FromString(AnimalLead.AnimalTag.ToString())));
}

void AHSHUD::HandlePlantKnowledgeLearned(FGameplayTag PlantTag)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "PlantKnowledgeLearned", "Plant learned: {0}"), FText::FromString(PlantTag.ToString())));
}

void AHSHUD::HandlePlantEffectApplied(const FHSPlantEffectState& PlantEffect)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "PlantEffectApplied", "Plant effect active: {0}"), FText::FromString(PlantEffect.EffectTag.ToString())));
}

void AHSHUD::HandlePlantEffectExpired(const FHSPlantEffectState& PlantEffect)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "PlantEffectExpired", "Plant effect expired: {0}"), FText::FromString(PlantEffect.EffectTag.ToString())));
}

void AHSHUD::HandleBuildPlacementStarted(FText Message)
{
	ShowCompletionMessage(Message);
}

void AHSHUD::HandleBuildPlacementCanceled(FText Message)
{
	ShowCompletionMessage(Message);
}

void AHSHUD::HandleBuildPlacementBlocked(FText Message)
{
	ShowCompletionMessage(Message);
}

void AHSHUD::HandleBuildPlacementConfirmed(const FHSPendingBuildingSchema& PendingSchema)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "BuildPlacementConfirmed", "Schema placed: {0}"), FText::FromString(PendingSchema.BuildingTag.ToString())));
}

void AHSHUD::BindProgressComponent()
{
	if (!PlayerOwner)
	{
		return;
	}

	APawn* Pawn = PlayerOwner->GetPawn();
	UHSPlayerProgressComponent* ProgressComponent = Pawn ? Pawn->FindComponentByClass<UHSPlayerProgressComponent>() : nullptr;
	if (!ProgressComponent || BoundProgressComponent.Get() == ProgressComponent)
	{
		return;
	}

	if (UHSPlayerProgressComponent* PreviousProgressComponent = BoundProgressComponent.Get())
	{
		PreviousProgressComponent->OnMilestoneCompleted.RemoveDynamic(this, &AHSHUD::HandleMilestoneCompleted);
	}

	BoundProgressComponent = ProgressComponent;
	ProgressComponent->OnMilestoneCompleted.AddUniqueDynamic(this, &AHSHUD::HandleMilestoneCompleted);
}

void AHSHUD::BindAnimalIntelComponent()
{
	if (!PlayerOwner)
	{
		return;
	}

	APawn* Pawn = PlayerOwner->GetPawn();
	UHSAnimalIntelComponent* AnimalIntelComponent = Pawn ? Pawn->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;
	if (!AnimalIntelComponent || BoundAnimalIntelComponent.Get() == AnimalIntelComponent)
	{
		return;
	}

	if (UHSAnimalIntelComponent* PreviousAnimalIntelComponent = BoundAnimalIntelComponent.Get())
	{
		PreviousAnimalIntelComponent->OnAnimalLeadReported.RemoveDynamic(this, &AHSHUD::HandleAnimalLeadReported);
		PreviousAnimalIntelComponent->OnAnimalLeadExpired.RemoveDynamic(this, &AHSHUD::HandleAnimalLeadExpired);
	}

	BoundAnimalIntelComponent = AnimalIntelComponent;
	AnimalIntelComponent->OnAnimalLeadReported.AddUniqueDynamic(this, &AHSHUD::HandleAnimalLeadReported);
	AnimalIntelComponent->OnAnimalLeadExpired.AddUniqueDynamic(this, &AHSHUD::HandleAnimalLeadExpired);
}

void AHSHUD::BindPlantEffectComponent()
{
	if (!PlayerOwner)
	{
		return;
	}

	APawn* Pawn = PlayerOwner->GetPawn();
	UHSPlantEffectComponent* PlantEffectComponent = Pawn ? Pawn->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;
	if (!PlantEffectComponent || BoundPlantEffectComponent.Get() == PlantEffectComponent)
	{
		return;
	}

	if (UHSPlantEffectComponent* PreviousPlantEffectComponent = BoundPlantEffectComponent.Get())
	{
		PreviousPlantEffectComponent->OnPlantKnowledgeLearned.RemoveDynamic(this, &AHSHUD::HandlePlantKnowledgeLearned);
		PreviousPlantEffectComponent->OnPlantEffectApplied.RemoveDynamic(this, &AHSHUD::HandlePlantEffectApplied);
		PreviousPlantEffectComponent->OnPlantEffectExpired.RemoveDynamic(this, &AHSHUD::HandlePlantEffectExpired);
	}

	BoundPlantEffectComponent = PlantEffectComponent;
	PlantEffectComponent->OnPlantKnowledgeLearned.AddUniqueDynamic(this, &AHSHUD::HandlePlantKnowledgeLearned);
	PlantEffectComponent->OnPlantEffectApplied.AddUniqueDynamic(this, &AHSHUD::HandlePlantEffectApplied);
	PlantEffectComponent->OnPlantEffectExpired.AddUniqueDynamic(this, &AHSHUD::HandlePlantEffectExpired);
}

void AHSHUD::BindBuildPlacementComponent()
{
	if (!PlayerOwner)
	{
		return;
	}

	APawn* Pawn = PlayerOwner->GetPawn();
	UHSBuildPlacementComponent* BuildPlacementComponent = Pawn ? Pawn->FindComponentByClass<UHSBuildPlacementComponent>() : nullptr;
	if (!BuildPlacementComponent || BoundBuildPlacementComponent.Get() == BuildPlacementComponent)
	{
		return;
	}

	if (UHSBuildPlacementComponent* PreviousBuildPlacementComponent = BoundBuildPlacementComponent.Get())
	{
		PreviousBuildPlacementComponent->OnBuildPlacementStarted.RemoveDynamic(this, &AHSHUD::HandleBuildPlacementStarted);
		PreviousBuildPlacementComponent->OnBuildPlacementCanceled.RemoveDynamic(this, &AHSHUD::HandleBuildPlacementCanceled);
		PreviousBuildPlacementComponent->OnBuildPlacementBlocked.RemoveDynamic(this, &AHSHUD::HandleBuildPlacementBlocked);
		PreviousBuildPlacementComponent->OnBuildPlacementConfirmed.RemoveDynamic(this, &AHSHUD::HandleBuildPlacementConfirmed);
	}

	BoundBuildPlacementComponent = BuildPlacementComponent;
	BuildPlacementComponent->OnBuildPlacementStarted.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementStarted);
	BuildPlacementComponent->OnBuildPlacementCanceled.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementCanceled);
	BuildPlacementComponent->OnBuildPlacementBlocked.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementBlocked);
	BuildPlacementComponent->OnBuildPlacementConfirmed.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementConfirmed);
}

void AHSHUD::BindSettlementPlanComponent()
{
	if (!PlayerOwner)
	{
		return;
	}

	APawn* Pawn = PlayerOwner->GetPawn();
	UHSSettlementPlanComponent* SettlementPlanComponent = Pawn ? Pawn->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	if (!SettlementPlanComponent || BoundSettlementPlanComponent.Get() == SettlementPlanComponent)
	{
		return;
	}

	if (UHSSettlementPlanComponent* PreviousSettlementPlanComponent = BoundSettlementPlanComponent.Get())
	{
		PreviousSettlementPlanComponent->OnSettlementClaimed.RemoveDynamic(this, &AHSHUD::HandleSettlementClaimed);
		PreviousSettlementPlanComponent->OnBuildingSchemaPlaced.RemoveDynamic(this, &AHSHUD::HandleBuildingSchemaPlaced);
	}

	BoundSettlementPlanComponent = SettlementPlanComponent;
	SettlementPlanComponent->OnSettlementClaimed.AddUniqueDynamic(this, &AHSHUD::HandleSettlementClaimed);
	SettlementPlanComponent->OnBuildingSchemaPlaced.AddUniqueDynamic(this, &AHSHUD::HandleBuildingSchemaPlaced);
}

void AHSHUD::BindConstructionComponent()
{
	if (!PlayerOwner)
	{
		return;
	}

	APawn* Pawn = PlayerOwner->GetPawn();
	UHSConstructionComponent* ConstructionComponent = Pawn ? Pawn->FindComponentByClass<UHSConstructionComponent>() : nullptr;
	if (!ConstructionComponent || BoundConstructionComponent.Get() == ConstructionComponent)
	{
		return;
	}

	if (UHSConstructionComponent* PreviousConstructionComponent = BoundConstructionComponent.Get())
	{
		PreviousConstructionComponent->OnConstructionJobStarted.RemoveDynamic(this, &AHSHUD::HandleConstructionJobStarted);
		PreviousConstructionComponent->OnConstructionJobCompleted.RemoveDynamic(this, &AHSHUD::HandleConstructionJobCompleted);
	}

	BoundConstructionComponent = ConstructionComponent;
	ConstructionComponent->OnConstructionJobStarted.AddUniqueDynamic(this, &AHSHUD::HandleConstructionJobStarted);
	ConstructionComponent->OnConstructionJobCompleted.AddUniqueDynamic(this, &AHSHUD::HandleConstructionJobCompleted);
}

void AHSHUD::ShowCompletionMessage(FText Message)
{
	CompletionMessage = Message;
	CompletionMessageExpireTime = FPlatformTime::Seconds() + CompletionMessageDuration;
}

void AHSHUD::DrawCenteredTextLine(const FString& Text, float Y, float Scale, const FLinearColor& Color)
{
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, GEngine->GetMediumFont(), Scale);

	const float X = (Canvas->ClipX - TextWidth) * 0.5f;
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale, false);
}
