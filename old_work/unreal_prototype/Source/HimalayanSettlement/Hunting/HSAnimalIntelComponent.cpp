#include "Hunting/HSAnimalIntelComponent.h"

#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "Hunting/HSAnimalLeadMarkerActor.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

UHSAnimalIntelComponent::UHSAnimalIntelComponent()
	: DefaultAnimalTag(TAG_HS_Animal_Deer)
	, DefaultLeadLocation(FVector(720.0f, -420.0f, 70.0f))
	, LastReportDay(INDEX_NONE)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSAnimalIntelComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToWorldTime();
}

void UHSAnimalIntelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHSWorldTimeComponent* WorldTime = BoundWorldTime.Get())
	{
		WorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSAnimalIntelComponent::HandleTimeAdvanced);
	}

	BoundWorldTime.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UHSAnimalIntelComponent::RequestDailyAnimalLead()
{
	if (!CanRequestDailyAnimalLead())
	{
		return false;
	}

	ActiveLead = FHSAnimalLead();
	ActiveLead.AnimalTag = DefaultAnimalTag;
	ActiveLead.Location = DefaultLeadLocation;
	ActiveLead.ReportDay = ResolveCurrentDay();
	ActiveLead.bActive = true;
	LastReportDay = ActiveLead.ReportDay;

	SpawnOrUpdateLeadMarker();
	OnAnimalLeadReported.Broadcast(ActiveLead);
	UE_LOG(LogHSAI, Log, TEXT("%s received daily animal lead %s at %s for day %d."), *GetNameSafe(GetOwner()), *ActiveLead.AnimalTag.ToString(), *ActiveLead.Location.ToCompactString(), ActiveLead.ReportDay);
	return true;
}

bool UHSAnimalIntelComponent::RefreshActiveAnimalLead()
{
	if (!ActiveLead.IsValid())
	{
		return RequestDailyAnimalLead();
	}

	SpawnOrUpdateLeadMarker();
	OnAnimalLeadReported.Broadcast(ActiveLead);
	UE_LOG(LogHSAI, Log, TEXT("%s refreshed animal lead %s at %s."), *GetNameSafe(GetOwner()), *ActiveLead.AnimalTag.ToString(), *ActiveLead.Location.ToCompactString());
	return true;
}

void UHSAnimalIntelComponent::ExpireActiveLead()
{
	if (!ActiveLead.IsValid())
	{
		return;
	}

	const FHSAnimalLead ExpiredLead = ActiveLead;
	ActiveLead.bActive = false;
	if (AHSAnimalLeadMarkerActor* Marker = ActiveLeadMarker.Get())
	{
		Marker->ExpireLead();
	}

	OnAnimalLeadExpired.Broadcast(ExpiredLead);
	UE_LOG(LogHSAI, Log, TEXT("%s expired animal lead %s from day %d."), *GetNameSafe(GetOwner()), *ExpiredLead.AnimalTag.ToString(), ExpiredLead.ReportDay);
}

void UHSAnimalIntelComponent::BindToWorldTime()
{
	UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	if (!WorldTime || BoundWorldTime.Get() == WorldTime)
	{
		return;
	}

	if (UHSWorldTimeComponent* PreviousWorldTime = BoundWorldTime.Get())
	{
		PreviousWorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSAnimalIntelComponent::HandleTimeAdvanced);
	}

	BoundWorldTime = WorldTime;
	WorldTime->OnTimeAdvanced.AddUniqueDynamic(this, &UHSAnimalIntelComponent::HandleTimeAdvanced);
}

bool UHSAnimalIntelComponent::CanRequestDailyAnimalLead() const
{
	return DefaultAnimalTag.IsValid() && LastReportDay != ResolveCurrentDay();
}

FText UHSAnimalIntelComponent::DescribeActiveLead() const
{
	if (!ActiveLead.IsValid())
	{
		return NSLOCTEXT("HSAnimalIntelComponent", "NoActiveLead", "No active animal report.");
	}

	return FText::Format(
		NSLOCTEXT("HSAnimalIntelComponent", "ActiveLeadSummary", "{0} signs near X {1}, Y {2}. Report day {3}."),
		FText::FromString(ActiveLead.AnimalTag.ToString()),
		FText::AsNumber(FMath::RoundToInt(ActiveLead.Location.X)),
		FText::AsNumber(FMath::RoundToInt(ActiveLead.Location.Y)),
		FText::AsNumber(ActiveLead.ReportDay));
}

void UHSAnimalIntelComponent::RestoreAnimalIntelState(const FHSAnimalLead& NewActiveLead, int32 NewLastReportDay)
{
	LastReportDay = NewLastReportDay;
	ActiveLead = NewActiveLead.IsValid() ? NewActiveLead : FHSAnimalLead();
	if (ActiveLead.IsValid())
	{
		SpawnOrUpdateLeadMarker();
	}
}

void UHSAnimalIntelComponent::HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays)
{
	if (ActiveLead.IsValid() && ResolveCurrentDay() > ActiveLead.ReportDay)
	{
		ExpireActiveLead();
	}
}

int32 UHSAnimalIntelComponent::ResolveCurrentDay() const
{
	const UHSWorldTimeComponent* WorldTime = ResolveWorldTime();
	return WorldTime ? FMath::FloorToInt(WorldTime->GetElapsedDays()) : 0;
}

AHSAnimalLeadMarkerActor* UHSAnimalIntelComponent::SpawnOrUpdateLeadMarker()
{
	UWorld* World = GetWorld();
	if (!World || !ActiveLead.IsValid())
	{
		return nullptr;
	}

	AHSAnimalLeadMarkerActor* Marker = ActiveLeadMarker.Get();
	if (!Marker)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Marker = World->SpawnActor<AHSAnimalLeadMarkerActor>(ActiveLead.Location, FRotator::ZeroRotator, SpawnParameters);
		if (Marker)
		{
			Marker->SetActorLabel(TEXT("AnimalLead_Daily"));
			ActiveLeadMarker = Marker;
		}
	}

	if (Marker)
	{
		Marker->SetActorLocation(ActiveLead.Location);
		Marker->ConfigureLead(ActiveLead.AnimalTag, ActiveLead.ReportDay);
	}

	return Marker;
}

UHSWorldTimeComponent* UHSAnimalIntelComponent::ResolveWorldTime() const
{
	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	return GameState ? GameState->GetWorldTimeComponent() : nullptr;
}
