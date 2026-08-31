#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "World/HSWorldTimeComponent.h"
#include "HSHUD.generated.h"

struct FHSPendingBuildingSchema;
struct FHSConstructionJob;
class AHSGameState;

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

	TArray<FString> BuildStatusLines(const AHSGameState* GameState, const APawn* Pawn) const;
	TArray<FString> BuildObjectiveLines(const AHSGameState* GameState, const APawn* Pawn) const;

	UFUNCTION()
	void HandleMilestoneCompleted(FGameplayTag MilestoneTag);

	UFUNCTION()
	void HandleSettlementClaimed(FVector ClaimCenter);

	UFUNCTION()
	void HandleBuildingSchemaPlaced(const FHSPendingBuildingSchema& PendingSchema);

	UFUNCTION()
	void HandleConstructionJobStarted(const FHSConstructionJob& ConstructionJob);

	UFUNCTION()
	void HandleConstructionJobCompleted(const FHSConstructionJob& ConstructionJob);

	UFUNCTION()
	void HandleAnimalLeadReported(const FHSAnimalLead& AnimalLead);

	UFUNCTION()
	void HandleAnimalLeadExpired(const FHSAnimalLead& AnimalLead);

	UFUNCTION()
	void HandlePlantKnowledgeLearned(FGameplayTag PlantTag);

	UFUNCTION()
	void HandlePlantEffectApplied(const FHSPlantEffectState& PlantEffect);

	UFUNCTION()
	void HandlePlantEffectExpired(const FHSPlantEffectState& PlantEffect);

	UFUNCTION()
	void HandleBuildPlacementStarted(FText Message);

	UFUNCTION()
	void HandleBuildPlacementCanceled(FText Message);

	UFUNCTION()
	void HandleBuildPlacementBlocked(FText Message);

	UFUNCTION()
	void HandleBuildPlacementConfirmed(const FHSPendingBuildingSchema& PendingSchema);

	UFUNCTION()
	void HandleBuildPlacementActorConfirmed(FText Message);

	UFUNCTION()
	void HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays);

	UFUNCTION()
	void HandleHuntingFeedback(FText Message);

private:
	void BindWorldTimeComponent();
	void BindProgressComponent();
	void BindAnimalIntelComponent();
	void BindPlantEffectComponent();
	void BindBuildPlacementComponent();
	void BindSettlementPlanComponent();
	void BindConstructionComponent();
	void BindHuntingComponent();
	void ShowCompletionMessage(FText Message);
	void DrawCenteredTextLine(const FString& Text, float Y, float Scale, const FLinearColor& Color);
	void DrawStatusTextLine(const FString& Text, float Y, const FLinearColor& Color);
	void DrawStatusPanel(const TArray<FString>& StatusLines, const TArray<FString>& ObjectiveLines);
	void DrawPauseMenu(const class AHSPlayerCharacter* PlayerCharacter);
	void DrawInventoryMenu(const class AHSPlayerCharacter* PlayerCharacter);
	void DrawBuildMenu(const class AHSPlayerCharacter* PlayerCharacter);
	void DrawPlayerMenuFeedback(const class AHSPlayerCharacter* PlayerCharacter, float Y);

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.0"))
	float PromptVerticalOffset;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.1"))
	float PromptScale;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.1", Units = "s"))
	float CompletionMessageDuration;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.0"))
	float StatusPanelX;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.0"))
	float StatusPanelY;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.1"))
	float StatusLineHeight;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0.1"))
	float StatusScale;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSPlayerProgressComponent> BoundProgressComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSAnimalIntelComponent> BoundAnimalIntelComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSPlantEffectComponent> BoundPlantEffectComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSBuildPlacementComponent> BoundBuildPlacementComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSSettlementPlanComponent> BoundSettlementPlanComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSConstructionComponent> BoundConstructionComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSWorldTimeComponent> BoundWorldTimeComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSHuntingComponent> BoundHuntingComponent;

	UPROPERTY(Transient)
	FText CompletionMessage;

	double CompletionMessageExpireTime;
};
