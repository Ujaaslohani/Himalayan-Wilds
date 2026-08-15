#include "UI/HSHUD.h"

#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Progression/HSPlayerProgressComponent.h"
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

	CompletionMessage = FText::Format(NSLOCTEXT("HimalayanSettlementHUD", "MilestoneCompleted", "Completed: {Milestone}"), FText::FromString(MilestoneTag.ToString()));
	CompletionMessageExpireTime = FPlatformTime::Seconds() + CompletionMessageDuration;
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

void AHSHUD::DrawCenteredTextLine(const FString& Text, float Y, float Scale, const FLinearColor& Color)
{
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, GEngine->GetMediumFont(), Scale);

	const float X = (Canvas->ClipX - TextWidth) * 0.5f;
	DrawText(Text, Color, X, Y, GEngine->GetMediumFont(), Scale, false);
}
