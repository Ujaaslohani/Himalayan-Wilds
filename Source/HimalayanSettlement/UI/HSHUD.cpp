#include "UI/HSHUD.h"

#include "Core/HSGameplayTags.h"
#include "Engine/Canvas.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "GameFramework/PlayerController.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Hunting/HSHuntingComponent.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Player/HSPlayerCharacter.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSBuildPlacementComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

AHSHUD::AHSHUD()
	: PromptVerticalOffset(120.0f)
	, PromptScale(1.15f)
	, CompletionMessageDuration(4.0f)
	, StatusPanelX(24.0f)
	, StatusPanelY(24.0f)
	, StatusLineHeight(20.0f)
	, StatusScale(0.85f)
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
	BindWorldTimeComponent();
	BindHuntingComponent();

	const AHSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AHSGameState>() : nullptr;
	const APawn* Pawn = PlayerOwner->GetPawn();
	const AHSPlayerCharacter* PlayerCharacter = Cast<AHSPlayerCharacter>(Pawn);

	const TArray<FString> StatusLines = BuildStatusLines(GameState, Pawn);
	for (int32 LineIndex = 0; LineIndex < StatusLines.Num(); ++LineIndex)
	{
		DrawStatusTextLine(StatusLines[LineIndex], StatusPanelY + (StatusLineHeight * LineIndex), FLinearColor(0.85f, 0.92f, 1.0f, 1.0f));
	}

	const TArray<FString> ObjectiveLines = BuildObjectiveLines(GameState, Pawn);
	const float ObjectiveStartY = StatusPanelY + (StatusLineHeight * (StatusLines.Num() + 1));
	for (int32 LineIndex = 0; LineIndex < ObjectiveLines.Num(); ++LineIndex)
	{
		DrawStatusTextLine(ObjectiveLines[LineIndex], ObjectiveStartY + (StatusLineHeight * LineIndex), FLinearColor(0.75f, 1.0f, 0.72f, 1.0f));
	}

	if (PlayerCharacter && PlayerCharacter->IsPauseMenuOpen())
	{
		DrawPauseMenu(PlayerCharacter);
		return;
	}

	if (PlayerCharacter && PlayerCharacter->IsInventoryOpen())
	{
		DrawInventoryMenu(PlayerCharacter);
		return;
	}

	if (PlayerCharacter && PlayerCharacter->IsBuildMenuOpen())
	{
		DrawBuildMenu(PlayerCharacter);
		return;
	}

	const UHSBuildPlacementComponent* BuildPlacementComponent = Pawn ? Pawn->FindComponentByClass<UHSBuildPlacementComponent>() : nullptr;
	if (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive())
	{
		const FString BuildText = FString::Printf(TEXT("[Left Mouse/E] Place   [Q/R] Rotate   [Esc/B] Cancel   %s"), *BuildPlacementComponent->GetPlacementStatusText().ToString());
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

	if (PlayerCharacter && PlayerCharacter->HasActiveMenuFeedback())
	{
		const float MessageY = FMath::Max(0.0f, Canvas->ClipY - PromptVerticalOffset - 36.0f);
		DrawCenteredTextLine(PlayerCharacter->GetLastMenuFeedback().ToString(), MessageY, PromptScale, FLinearColor(0.75f, 1.0f, 0.72f, 1.0f));
	}
	else if (HasActiveCompletionMessage())
	{
		const float MessageY = FMath::Max(0.0f, Canvas->ClipY - PromptVerticalOffset - 36.0f);
		DrawCenteredTextLine(CompletionMessage.ToString(), MessageY, PromptScale, FLinearColor(0.55f, 1.0f, 0.55f, 1.0f));
	}
}

bool AHSHUD::HasActiveCompletionMessage() const
{
	return !CompletionMessage.IsEmpty() && FPlatformTime::Seconds() < CompletionMessageExpireTime;
}

TArray<FString> AHSHUD::BuildStatusLines(const AHSGameState* GameState, const APawn* Pawn) const
{
	TArray<FString> Lines;

	if (const UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr)
	{
		Lines.Add(FString::Printf(TEXT("Time: %s  Day %.1f"), *WorldTime->GetTimePhaseText().ToString(), WorldTime->GetElapsedDays()));
	}

	const UHSResourceCarryComponent* ResourceCarryComponent = Pawn ? Pawn->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
	const UHSPlayerEquipmentComponent* EquipmentComponent = Pawn ? Pawn->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr;
	if (EquipmentComponent)
	{
		const FString BowState = EquipmentComponent->IsBowEquipped() ? TEXT("equipped") : TEXT("stored");
		const UHSHuntingComponent* HuntingComponent = Pawn ? Pawn->FindComponentByClass<UHSHuntingComponent>() : nullptr;
		const float CooldownRemaining = HuntingComponent ? HuntingComponent->GetShotCooldownRemaining() : 0.0f;
		const FString ReadyState = EquipmentComponent->CanUseBow()
			? (CooldownRemaining > 0.0f ? FString::Printf(TEXT("nocking %.1fs"), CooldownRemaining) : TEXT("ready"))
			: TEXT("not ready");
		Lines.Add(FString::Printf(TEXT("Equipment: Bow %s, arrows %d, %s"), *BowState, EquipmentComponent->GetArrowCount(), *ReadyState));
	}

	if (ResourceCarryComponent)
	{
		const TArray<FHSItemStack>& CarriedItems = ResourceCarryComponent->GetCarriedItems();
		if (CarriedItems.Num() > 0)
		{
			TArray<FString> ResourceParts;
			for (const FHSItemStack& Stack : CarriedItems)
			{
				if (Stack.IsValidStack())
				{
					ResourceParts.Add(FString::Printf(TEXT("%s x%d"), *Stack.ItemTag.ToString(), Stack.Quantity));
				}
			}

			if (ResourceParts.Num() > 0)
			{
				Lines.Add(FString::Printf(TEXT("Carry: %s"), *FString::Join(ResourceParts, TEXT(", "))));
			}
		}
	}

	const UHSAnimalIntelComponent* AnimalIntelComponent = Pawn ? Pawn->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;
	if (AnimalIntelComponent && AnimalIntelComponent->HasActiveLead())
	{
		const FHSAnimalLead& Lead = AnimalIntelComponent->GetActiveLead();
		Lines.Add(FString::Printf(TEXT("Lead: %s near %.0f, %.0f"), *Lead.AnimalTag.ToString(), Lead.Location.X, Lead.Location.Y));
	}

	const UHSPlantEffectComponent* PlantEffectComponent = Pawn ? Pawn->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;
	if (PlantEffectComponent && PlantEffectComponent->HasActiveEffect())
	{
		const FHSPlantEffectState& PlantEffect = PlantEffectComponent->GetActiveEffect();
		const UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
		const float CurrentElapsedDays = WorldTime ? WorldTime->GetElapsedDays() : PlantEffect.StartElapsedDays;
		const float RemainingDays = FMath::Max(0.0f, PlantEffect.DurationDays - (CurrentElapsedDays - PlantEffect.StartElapsedDays));
		Lines.Add(FString::Printf(TEXT("Plant: %s %.1fx %.1fd"), *PlantEffect.EffectTag.ToString(), PlantEffect.MovementSpeedMultiplier, RemainingDays));
	}

	const UHSConstructionComponent* ConstructionComponent = Pawn ? Pawn->FindComponentByClass<UHSConstructionComponent>() : nullptr;
	if (ConstructionComponent)
	{
		for (const FHSConstructionJob& Job : ConstructionComponent->GetConstructionJobs())
		{
			if (Job.State == EHSConstructionJobState::Active)
			{
				const float Percent = Job.RequiredDays > 0.0f ? FMath::Clamp(Job.ProgressDays / Job.RequiredDays, 0.0f, 1.0f) * 100.0f : 0.0f;
				Lines.Add(FString::Printf(TEXT("Build: %s %.0f%%"), *Job.BuildingTag.ToString(), Percent));
				break;
			}
		}
	}

	return Lines;
}

TArray<FString> AHSHUD::BuildObjectiveLines(const AHSGameState* GameState, const APawn* Pawn) const
{
	(void)GameState;

	TArray<FString> Lines;
	Lines.Add(TEXT("Objective: Get ready for the settlement day"));

	const UHSPlayerProgressComponent* ProgressComponent = Pawn ? Pawn->FindComponentByClass<UHSPlayerProgressComponent>() : nullptr;
	const UHSResourceCarryComponent* ResourceCarryComponent = Pawn ? Pawn->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
	const UHSAnimalIntelComponent* AnimalIntelComponent = Pawn ? Pawn->FindComponentByClass<UHSAnimalIntelComponent>() : nullptr;
	const UHSSettlementPlanComponent* SettlementPlanComponent = Pawn ? Pawn->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	const UHSConstructionComponent* ConstructionComponent = Pawn ? Pawn->FindComponentByClass<UHSConstructionComponent>() : nullptr;

	const bool bCompletedMorningHunt = ProgressComponent && ProgressComponent->HasCompletedMilestone(TAG_HS_Quest_OneMorningHunt);
	const int32 DeerMeat = ResourceCarryComponent ? ResourceCarryComponent->GetQuantityForTag(TAG_HS_Resource_Meat_Deer) : 0;
	const int32 DeerHide = ResourceCarryComponent ? ResourceCarryComponent->GetQuantityForTag(TAG_HS_Resource_Hide_Deer) : 0;
	const bool bHasTurnInResources = DeerMeat >= 2 && DeerHide >= 1;

	if (!bCompletedMorningHunt)
	{
		if (bHasTurnInResources)
		{
			Lines.Add(TEXT("Return deer meat and hide to the Hunter"));
		}
		else if (AnimalIntelComponent && AnimalIntelComponent->HasActiveLead())
		{
			Lines.Add(TEXT("Follow the animal lead, hunt the deer, then harvest it"));
		}
		else
		{
			Lines.Add(TEXT("Talk to the Hunter, hunt one deer, and harvest it"));
		}

		return Lines;
	}

	if (!SettlementPlanComponent || !SettlementPlanComponent->HasSettlementClaim())
	{
		Lines.Add(TEXT("Claim the settlement site"));
		return Lines;
	}

	if (ConstructionComponent && ConstructionComponent->HasCompletedBuilding(TAG_HS_Building_BasicHouse))
	{
		Lines.Add(TEXT("Basic house complete: use NPC services and explore the forest trail"));
		return Lines;
	}

	if (ConstructionComponent && ConstructionComponent->HasActiveJob())
	{
		Lines.Add(TEXT("Sleep to advance the Builder's house construction"));
		return Lines;
	}

	if (SettlementPlanComponent->HasAnyPendingSchema())
	{
		Lines.Add(TEXT("Ask the Builder to start the pending house schema"));
		return Lines;
	}

	Lines.Add(TEXT("Press B to place a basic house schema near the claim"));
	return Lines;
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

void AHSHUD::HandleBuildPlacementActorConfirmed(FText Message)
{
	ShowCompletionMessage(Message);
}

void AHSHUD::HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays)
{
	const FText PhaseText = NewTimePhase == EHSTimePhase::Day
		? NSLOCTEXT("HimalayanSettlementHUD", "TimePhaseDay", "Day")
		: NSLOCTEXT("HimalayanSettlementHUD", "TimePhaseNight", "Night");

	ShowCompletionMessage(FText::Format(
		NSLOCTEXT("HimalayanSettlementHUD", "TimeAdvanced", "Time advanced: {0} ({1} days). Services refreshed."),
		PhaseText,
		FText::AsNumber(ElapsedDays)));
}

void AHSHUD::HandleHuntingFeedback(FText Message)
{
	ShowCompletionMessage(Message);
}

void AHSHUD::BindWorldTimeComponent()
{
	const AHSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AHSGameState>() : nullptr;
	UHSWorldTimeComponent* WorldTimeComponent = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	if (!WorldTimeComponent || BoundWorldTimeComponent.Get() == WorldTimeComponent)
	{
		return;
	}

	if (UHSWorldTimeComponent* PreviousWorldTimeComponent = BoundWorldTimeComponent.Get())
	{
		PreviousWorldTimeComponent->OnTimeAdvanced.RemoveDynamic(this, &AHSHUD::HandleTimeAdvanced);
	}

	BoundWorldTimeComponent = WorldTimeComponent;
	WorldTimeComponent->OnTimeAdvanced.AddUniqueDynamic(this, &AHSHUD::HandleTimeAdvanced);
}

void AHSHUD::BindHuntingComponent()
{
	if (!PlayerOwner)
	{
		return;
	}

	APawn* Pawn = PlayerOwner->GetPawn();
	UHSHuntingComponent* HuntingComponent = Pawn ? Pawn->FindComponentByClass<UHSHuntingComponent>() : nullptr;
	if (!HuntingComponent || BoundHuntingComponent.Get() == HuntingComponent)
	{
		return;
	}

	if (UHSHuntingComponent* PreviousHuntingComponent = BoundHuntingComponent.Get())
	{
		PreviousHuntingComponent->OnHuntingFeedback.RemoveDynamic(this, &AHSHUD::HandleHuntingFeedback);
	}

	BoundHuntingComponent = HuntingComponent;
	HuntingComponent->OnHuntingFeedback.AddUniqueDynamic(this, &AHSHUD::HandleHuntingFeedback);
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
		PreviousBuildPlacementComponent->OnBuildPlacementActorConfirmed.RemoveDynamic(this, &AHSHUD::HandleBuildPlacementActorConfirmed);
	}

	BoundBuildPlacementComponent = BuildPlacementComponent;
	BuildPlacementComponent->OnBuildPlacementStarted.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementStarted);
	BuildPlacementComponent->OnBuildPlacementCanceled.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementCanceled);
	BuildPlacementComponent->OnBuildPlacementBlocked.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementBlocked);
	BuildPlacementComponent->OnBuildPlacementConfirmed.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementConfirmed);
	BuildPlacementComponent->OnBuildPlacementActorConfirmed.AddUniqueDynamic(this, &AHSHUD::HandleBuildPlacementActorConfirmed);
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

void AHSHUD::DrawStatusTextLine(const FString& Text, float Y, const FLinearColor& Color)
{
	DrawText(Text, Color, StatusPanelX, Y, GEngine->GetSmallFont(), StatusScale, false);
}

void AHSHUD::DrawPauseMenu(const AHSPlayerCharacter* PlayerCharacter)
{
	if (!Canvas || !PlayerCharacter)
	{
		return;
	}

	const float PanelWidth = FMath::Min(420.0f, Canvas->ClipX - 48.0f);
	const float PanelHeight = PlayerCharacter->IsPauseOptionsOpen() ? 350.0f : 240.0f;
	const float PanelX = (Canvas->ClipX - PanelWidth) * 0.5f;
	const float PanelY = (Canvas->ClipY - PanelHeight) * 0.5f;

	DrawRect(FLinearColor(0.02f, 0.025f, 0.025f, 0.82f), PanelX, PanelY, PanelWidth, PanelHeight);

	float LineY = PanelY + 26.0f;
	DrawCenteredTextLine(TEXT("Paused"), LineY, 1.25f, FLinearColor(0.95f, 0.95f, 0.86f, 1.0f));
	LineY += 42.0f;

	TArray<FString> MenuLines = {
		TEXT("[1] Resume"),
		TEXT("[2] Save"),
		TEXT("[3] Load"),
		TEXT("[4] Options"),
		TEXT("[5] Exit")
	};

	if (PlayerCharacter->IsPauseOptionsOpen())
	{
		MenuLines.Add(FString::Printf(TEXT("[6] Graphics: %s"), *PlayerCharacter->GetGraphicsQualityPresetName()));
		MenuLines.Add(FString::Printf(TEXT("[7] Mouse: %s"), *PlayerCharacter->GetMouseSensitivityPresetName()));
		MenuLines.Add(FString::Printf(TEXT("[8] Volume: %s"), *PlayerCharacter->GetMasterVolumePresetName()));
	}

	for (const FString& MenuLine : MenuLines)
	{
		DrawText(MenuLine, FLinearColor::White, PanelX + 58.0f, LineY, GEngine->GetMediumFont(), 0.95f, false);
		LineY += 28.0f;
	}

	if (PlayerCharacter->IsPauseOptionsOpen())
	{
		LineY += 12.0f;
		DrawText(TEXT("Options"), FLinearColor(0.75f, 1.0f, 0.72f, 1.0f), PanelX + 58.0f, LineY, GEngine->GetMediumFont(), 0.9f, false);
		LineY += 28.0f;
		DrawText(TEXT("Press 6/7/8 to cycle option presets."), FLinearColor(0.82f, 0.86f, 0.86f, 1.0f), PanelX + 58.0f, LineY, GEngine->GetSmallFont(), 0.85f, false);
	}

	DrawPlayerMenuFeedback(PlayerCharacter, PanelY + PanelHeight - 22.0f);
}

void AHSHUD::DrawInventoryMenu(const AHSPlayerCharacter* PlayerCharacter)
{
	if (!Canvas || !PlayerCharacter)
	{
		return;
	}

	const float PanelWidth = FMath::Min(520.0f, Canvas->ClipX - 48.0f);
	const float PanelHeight = 320.0f;
	const float PanelX = (Canvas->ClipX - PanelWidth) * 0.5f;
	const float PanelY = (Canvas->ClipY - PanelHeight) * 0.5f;

	DrawRect(FLinearColor(0.02f, 0.025f, 0.025f, 0.82f), PanelX, PanelY, PanelWidth, PanelHeight);

	float LineY = PanelY + 26.0f;
	DrawCenteredTextLine(TEXT("Inventory"), LineY, 1.25f, FLinearColor(0.95f, 0.95f, 0.86f, 1.0f));
	LineY += 46.0f;

	const UHSResourceCarryComponent* ResourceCarryComponent = PlayerCharacter->GetResourceCarryComponent();
	if (!ResourceCarryComponent || ResourceCarryComponent->GetCarriedItems().Num() == 0)
	{
		DrawText(TEXT("No carried items."), FLinearColor::White, PanelX + 58.0f, LineY, GEngine->GetMediumFont(), 0.95f, false);
	}
	else
	{
		const int32 SelectedInventoryIndex = PlayerCharacter->GetSelectedInventoryIndex();
		const TArray<FHSItemStack>& CarriedItems = ResourceCarryComponent->GetCarriedItems();
		for (int32 StackIndex = 0; StackIndex < CarriedItems.Num(); ++StackIndex)
		{
			const FHSItemStack& Stack = CarriedItems[StackIndex];
			if (Stack.IsValidStack())
			{
				const bool bSelected = StackIndex == SelectedInventoryIndex;
				const FString SelectionMarker = bSelected ? TEXT(">") : TEXT(" ");
				const FLinearColor StackColor = bSelected ? FLinearColor(0.75f, 1.0f, 0.72f, 1.0f) : FLinearColor::White;
				DrawText(FString::Printf(TEXT("%s %s   x%d"), *SelectionMarker, *Stack.ItemTag.ToString(), Stack.Quantity), StackColor, PanelX + 58.0f, LineY, GEngine->GetMediumFont(), 0.9f, false);
				LineY += 28.0f;
			}
		}
	}

	DrawText(TEXT("[Q/R] Select   [1] Inspect   [2] Drop one   [3] Drop stack"), FLinearColor(0.82f, 0.86f, 0.86f, 1.0f), PanelX + 58.0f, PanelY + PanelHeight - 70.0f, GEngine->GetSmallFont(), 0.85f, false);
	DrawPlayerMenuFeedback(PlayerCharacter, PanelY + PanelHeight - 54.0f);
	DrawText(TEXT("[I/Tab/Esc] Close"), FLinearColor(0.82f, 0.86f, 0.86f, 1.0f), PanelX + 58.0f, PanelY + PanelHeight - 42.0f, GEngine->GetSmallFont(), 0.85f, false);
}

void AHSHUD::DrawBuildMenu(const AHSPlayerCharacter* PlayerCharacter)
{
	if (!Canvas || !PlayerCharacter)
	{
		return;
	}

	const float PanelWidth = FMath::Min(520.0f, Canvas->ClipX - 48.0f);
	const float PanelHeight = 320.0f;
	const float PanelX = (Canvas->ClipX - PanelWidth) * 0.5f;
	const float PanelY = (Canvas->ClipY - PanelHeight) * 0.5f;

	DrawRect(FLinearColor(0.02f, 0.025f, 0.025f, 0.82f), PanelX, PanelY, PanelWidth, PanelHeight);

	float LineY = PanelY + 26.0f;
	DrawCenteredTextLine(TEXT("Build"), LineY, 1.25f, FLinearColor(0.95f, 0.95f, 0.86f, 1.0f));
	LineY += 50.0f;

	FText BuildReason;
	const bool bCanStartBasicHouse = PlayerCharacter->CanStartBasicHouseFromBuildMenu(&BuildReason);
	const FLinearColor SchemaColor = bCanStartBasicHouse ? FLinearColor::White : FLinearColor(1.0f, 0.55f, 0.45f, 1.0f);

	DrawText(TEXT("[1] Basic House"), SchemaColor, PanelX + 58.0f, LineY, GEngine->GetMediumFont(), 0.95f, false);
	LineY += 32.0f;
	DrawText(BuildReason.ToString(), FLinearColor(0.82f, 0.86f, 0.86f, 1.0f), PanelX + 58.0f, LineY, GEngine->GetSmallFont(), 0.85f, false);
	LineY += 38.0f;

	DrawText(TEXT("[2] Basic Campsite"), FLinearColor::White, PanelX + 58.0f, LineY, GEngine->GetMediumFont(), 0.95f, false);
	LineY += 32.0f;
	DrawText(TEXT("Place a temporary one-use sleep point."), FLinearColor(0.82f, 0.86f, 0.86f, 1.0f), PanelX + 58.0f, LineY, GEngine->GetSmallFont(), 0.85f, false);

	DrawPlayerMenuFeedback(PlayerCharacter, PanelY + PanelHeight - 60.0f);
	DrawText(TEXT("[B/Esc] Close"), FLinearColor(0.82f, 0.86f, 0.86f, 1.0f), PanelX + 58.0f, PanelY + PanelHeight - 42.0f, GEngine->GetSmallFont(), 0.85f, false);
}

void AHSHUD::DrawPlayerMenuFeedback(const AHSPlayerCharacter* PlayerCharacter, float Y)
{
	if (!PlayerCharacter || !PlayerCharacter->HasActiveMenuFeedback())
	{
		return;
	}

	DrawCenteredTextLine(PlayerCharacter->GetLastMenuFeedback().ToString(), Y, 0.85f, FLinearColor(0.75f, 1.0f, 0.72f, 1.0f));
}
