#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
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

private:
	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.0"))
	float PromptVerticalOffset;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.1"))
	float PromptScale;
};
