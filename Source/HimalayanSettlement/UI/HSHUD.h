#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "HSHUD.generated.h"

UCLASS()
class HIMALAYANSETTLEMENT_API AHSHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHSHUD();

	virtual void DrawHUD() override;

	UFUNCTION(BlueprintPure, Category = "HUD")
	float GetPromptVerticalOffset() const { return PromptVerticalOffset; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	float GetCompletionMessageDuration() const { return CompletionMessageDuration; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	const FText& GetCompletionMessage() const { return CompletionMessage; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool HasActiveCompletionMessage() const;

	UFUNCTION()
	void HandleMilestoneCompleted(FGameplayTag MilestoneTag);

private:
	void BindProgressComponent();
	void DrawCenteredTextLine(const FString& Text, float Y, float Scale, const FLinearColor& Color);

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.0"))
	float PromptVerticalOffset;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.1"))
	float PromptScale;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.1", Units = "s"))
	float CompletionMessageDuration;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSPlayerProgressComponent> BoundProgressComponent;

	UPROPERTY(Transient)
	FText CompletionMessage;

	double CompletionMessageExpireTime;
};
