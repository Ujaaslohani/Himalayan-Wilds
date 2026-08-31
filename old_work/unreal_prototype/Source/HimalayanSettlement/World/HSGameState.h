#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "HSGameState.generated.h"

class UHSWorldTimeComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AHSGameState();

	UFUNCTION(BlueprintPure, Category = "World Time")
	UHSWorldTimeComponent* GetWorldTimeComponent() const { return WorldTimeComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "World Time")
	TObjectPtr<UHSWorldTimeComponent> WorldTimeComponent;
};
