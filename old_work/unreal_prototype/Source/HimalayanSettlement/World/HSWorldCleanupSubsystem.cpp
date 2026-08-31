#include "World/HSWorldCleanupSubsystem.h"

#include "Engine/World.h"
#include "TimerManager.h"

UHSWorldCleanupSubsystem::UHSWorldCleanupSubsystem()
	: CleanupIntervalSeconds(5.0f)
{
}

void UHSWorldCleanupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartCleanupTimer();
}

void UHSWorldCleanupSubsystem::Deinitialize()
{
	StopCleanupTimer();
	CleanupEntries.Reset();
	Super::Deinitialize();
}

bool UHSWorldCleanupSubsystem::RegisterTemporaryActor(AActor* Actor, float LifetimeSeconds, FName Reason)
{
	if (!Actor || LifetimeSeconds <= 0.0f)
	{
		return false;
	}

	const float ExpireAtWorldSeconds = GetCurrentWorldSeconds() + LifetimeSeconds;
	for (FHSTemporaryActorCleanupEntry& Entry : CleanupEntries)
	{
		if (Entry.Actor.Get() == Actor)
		{
			Entry.ExpireAtWorldSeconds = ExpireAtWorldSeconds;
			Entry.Reason = Reason;
			return true;
		}
	}

	FHSTemporaryActorCleanupEntry Entry;
	Entry.Actor = Actor;
	Entry.ExpireAtWorldSeconds = ExpireAtWorldSeconds;
	Entry.Reason = Reason;
	CleanupEntries.Add(Entry);
	return true;
}

int32 UHSWorldCleanupSubsystem::CollectExpiredActors()
{
	const float CurrentSeconds = GetCurrentWorldSeconds();
	int32 DestroyedCount = 0;

	for (int32 EntryIndex = CleanupEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		AActor* Actor = CleanupEntries[EntryIndex].Actor.Get();
		if (!Actor)
		{
			CleanupEntries.RemoveAtSwap(EntryIndex);
			continue;
		}

		if (CleanupEntries[EntryIndex].ExpireAtWorldSeconds <= CurrentSeconds)
		{
			Actor->Destroy();
			CleanupEntries.RemoveAtSwap(EntryIndex);
			++DestroyedCount;
		}
	}

	return DestroyedCount;
}

bool UHSWorldCleanupSubsystem::IsActorRegistered(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	for (const FHSTemporaryActorCleanupEntry& Entry : CleanupEntries)
	{
		if (Entry.Actor.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

void UHSWorldCleanupSubsystem::HandleCleanupTimer()
{
	CollectExpiredActors();
}

void UHSWorldCleanupSubsystem::StartCleanupTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CleanupTimerHandle, this, &UHSWorldCleanupSubsystem::HandleCleanupTimer, CleanupIntervalSeconds, true);
	}
}

void UHSWorldCleanupSubsystem::StopCleanupTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimerHandle);
	}
}

float UHSWorldCleanupSubsystem::GetCurrentWorldSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}
