#include "UI/HSHUD.h"

#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Progression/HSPlayerProgressComponent.h"
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

	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "MilestoneCompleted", "Completed: {Milestone}"), FText::FromString(MilestoneTag.ToString())));
}

void AHSHUD::HandleSettlementClaimed(FVector ClaimCenter)
{
	ShowCompletionMessage(NSLOCTEXT("HimalayanSettlementHUD", "SettlementClaimed", "Settlement site claimed"));
}

void AHSHUD::HandleBuildingSchemaPlaced(const FHSPendingBuildingSchema& PendingSchema)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "SchemaPending", "Schema pending: {Building}"), FText::FromString(PendingSchema.BuildingTag.ToString())));
}

void AHSHUD::HandleConstructionJobStarted(const FHSConstructionJob& ConstructionJob)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "ConstructionStarted", "Construction started: {Building}"), FText::FromString(ConstructionJob.BuildingTag.ToString())));
}

void AHSHUD::HandleConstructionJobCompleted(const FHSConstructionJob& ConstructionJob)
{
	ShowCompletionMessage(FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "ConstructionComplete", "Construction complete: {Building}"), FText::FromString(ConstructionJob.BuildingTag.ToString())));
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
