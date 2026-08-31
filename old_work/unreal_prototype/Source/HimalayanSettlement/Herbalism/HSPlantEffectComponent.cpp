#include "Herbalism/HSPlantEffectComponent.h"

#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "Herbalism/HSPlantDefinition.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

UHSPlantEffectComponent::UHSPlantEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSPlantEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToWorldTime();
}

void UHSPlantEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHSWorldTimeComponent* WorldTime = BoundWorldTime.Get())
	{
		WorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSPlantEffectComponent::HandleTimeAdvanced);
	}

	BoundWorldTime.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UHSPlantEffectComponent::LearnPlant(FGameplayTag PlantTag)
{
	if (!PlantTag.IsValid() || KnownPlants.HasTagExact(PlantTag))
	{
		return false;
	}

	KnownPlants.AddTag(PlantTag);
	OnPlantKnowledgeLearned.Broadcast(PlantTag);
	UE_LOG(LogHSSettlement, Log, TEXT("%s learned plant %s."), *GetNameSafe(GetOwner()), *PlantTag.ToString());
	return true;
}

bool UHSPlantEffectComponent::HasPlantKnowledge(FGameplayTag PlantTag) const
{
	return PlantTag.IsValid() && KnownPlants.HasTagExact(PlantTag);
}

bool UHSPlantEffectComponent::ApplyPlantEffect(const UHSPlantDefinition* PlantDefinition)
{
	if (!PlantDefinition || !PlantDefinition->IsDefinitionValid() || !HasPlantKnowledge(PlantDefinition->PlantTag))
	{
		return false;
	}

	ActiveEffect = FHSPlantEffectState();
	ActiveEffect.PlantTag = PlantDefinition->PlantTag;
	ActiveEffect.EffectTag = PlantDefinition->EffectTag;
	ActiveEffect.MovementSpeedMultiplier = PlantDefinition->MovementSpeedMultiplier;
	ActiveEffect.StartElapsedDays = ResolveElapsedDays();
	ActiveEffect.DurationDays = PlantDefinition->DurationDays;
	ActiveEffect.bActive = true;

	BindToWorldTime();
	OnPlantEffectApplied.Broadcast(ActiveEffect);
	UE_LOG(LogHSSettlement, Log, TEXT("%s applied plant effect %s from %s."), *GetNameSafe(GetOwner()), *ActiveEffect.EffectTag.ToString(), *ActiveEffect.PlantTag.ToString());
	return true;
}

void UHSPlantEffectComponent::ExpireActiveEffect()
{
	if (!ActiveEffect.IsValid())
	{
		return;
	}

	const FHSPlantEffectState ExpiredEffect = ActiveEffect;
	ActiveEffect.bActive = false;
	OnPlantEffectExpired.Broadcast(ExpiredEffect);
	UE_LOG(LogHSSettlement, Log, TEXT("%s plant effect expired: %s."), *GetNameSafe(GetOwner()), *ExpiredEffect.EffectTag.ToString());
}

void UHSPlantEffectComponent::BindToWorldTime()
{
	UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	if (!WorldTime || BoundWorldTime.Get() == WorldTime)
	{
		return;
	}

	if (UHSWorldTimeComponent* PreviousWorldTime = BoundWorldTime.Get())
	{
		PreviousWorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSPlantEffectComponent::HandleTimeAdvanced);
	}

	BoundWorldTime = WorldTime;
	WorldTime->OnTimeAdvanced.AddUniqueDynamic(this, &UHSPlantEffectComponent::HandleTimeAdvanced);
}

float UHSPlantEffectComponent::GetMovementSpeedMultiplier() const
{
	return ActiveEffect.IsValid() ? ActiveEffect.MovementSpeedMultiplier : 1.0f;
}

void UHSPlantEffectComponent::RestorePlantState(const FGameplayTagContainer& NewKnownPlants, const FHSPlantEffectState& NewActiveEffect)
{
	KnownPlants.Reset();
	for (const FGameplayTag& PlantTag : NewKnownPlants)
	{
		if (PlantTag.IsValid())
		{
			KnownPlants.AddTag(PlantTag);
		}
	}

	ActiveEffect = NewActiveEffect.IsValid() ? NewActiveEffect : FHSPlantEffectState();
	BindToWorldTime();
	HandleTimeAdvanced(EHSTimePhase::Day, ResolveElapsedDays());
}

void UHSPlantEffectComponent::HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays)
{
	if (ActiveEffect.IsValid() && ElapsedDays - ActiveEffect.StartElapsedDays >= ActiveEffect.DurationDays)
	{
		ExpireActiveEffect();
	}
}

float UHSPlantEffectComponent::ResolveElapsedDays() const
{
	const UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	return WorldTime ? WorldTime->GetElapsedDays() : 0.0f;
}

UHSWorldTimeComponent* UHSPlantEffectComponent::ResolveWorldTime() const
{
	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	return GameState ? GameState->GetWorldTimeComponent() : nullptr;
}
