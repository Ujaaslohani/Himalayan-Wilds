#include "World/HSWorldTimeComponent.h"

#define LOCTEXT_NAMESPACE "HSWorldTimeComponent"

UHSWorldTimeComponent::UHSWorldTimeComponent()
	: TimePhase(EHSTimePhase::Day)
	, ElapsedHalfDays(0)
{
	PrimaryComponentTick.bCanEverTick = false;
}

FText UHSWorldTimeComponent::GetTimePhaseText() const
{
	return TimePhase == EHSTimePhase::Day ? LOCTEXT("DayPhase", "Day") : LOCTEXT("NightPhase", "Night");
}

void UHSWorldTimeComponent::SetTimePhase(EHSTimePhase NewTimePhase)
{
	TimePhase = NewTimePhase;
	OnTimeAdvanced.Broadcast(TimePhase, GetElapsedDays());
}

void UHSWorldTimeComponent::SetTimeState(EHSTimePhase NewTimePhase, int32 NewElapsedHalfDays)
{
	TimePhase = NewTimePhase;
	ElapsedHalfDays = FMath::Max(0, NewElapsedHalfDays);
	OnTimeAdvanced.Broadcast(TimePhase, GetElapsedDays());
}

EHSTimePhase UHSWorldTimeComponent::AdvanceHalfDay()
{
	TimePhase = TimePhase == EHSTimePhase::Day ? EHSTimePhase::Night : EHSTimePhase::Day;
	++ElapsedHalfDays;
	OnTimeAdvanced.Broadcast(TimePhase, GetElapsedDays());
	return TimePhase;
}

#undef LOCTEXT_NAMESPACE
