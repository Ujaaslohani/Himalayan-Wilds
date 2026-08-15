#include "UI/HSHUD.h"

#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/HSInteractionSensorComponent.h"

AHSHUD::AHSHUD()
	: PromptVerticalOffset(120.0f)
	, PromptScale(1.15f)
{
}

void AHSHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !PlayerOwner)
	{
		return;
	}

	const APawn* Pawn = PlayerOwner->GetPawn();
	const UHSInteractionSensorComponent* Sensor = Pawn ? Pawn->FindComponentByClass<UHSInteractionSensorComponent>() : nullptr;
	if (!Sensor)
	{
		return;
	}

	const FText Prompt = Sensor->GetFocusedInteractionPrompt();
	if (Prompt.IsEmpty())
	{
		return;
	}

	const FString PromptText = FString::Printf(TEXT("[E] %s"), *Prompt.ToString());
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(PromptText, TextWidth, TextHeight, GEngine->GetMediumFont(), PromptScale);

	const float X = (Canvas->ClipX - TextWidth) * 0.5f;
	const float Y = FMath::Max(0.0f, Canvas->ClipY - PromptVerticalOffset);
	DrawText(PromptText, FLinearColor::White, X, Y, GEngine->GetMediumFont(), PromptScale, false);
}
