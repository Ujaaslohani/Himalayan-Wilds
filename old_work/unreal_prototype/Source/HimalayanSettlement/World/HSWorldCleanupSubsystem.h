#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HSWorldCleanupSubsystem.generated.h"

USTRUCT()
struct HIMALAYANSETTLEMENT_API FHSTemporaryActorCleanupEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	float ExpireAtWorldSeconds = 0.0f;

	UPROPERTY()
	FName Reason;
};

UCLASS()
class HIMALAYANSETTLEMENT_API UHSWorldCleanupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UHSWorldCleanupSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "World Cleanup")
	bool RegisterTemporaryActor(AActor* Actor, float LifetimeSeconds, FName Reason = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "World Cleanup")
	int32 CollectExpiredActors();

	UFUNCTION(BlueprintPure, Category = "World Cleanup")
	bool IsActorRegistered(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "World Cleanup")
	int32 GetTrackedActorCount() const { return CleanupEntries.Num(); }

private:
	void HandleCleanupTimer();
	void StartCleanupTimer();
	void StopCleanupTimer();
	float GetCurrentWorldSeconds() const;

	UPROPERTY()
	TArray<FHSTemporaryActorCleanupEntry> CleanupEntries;

	FTimerHandle CleanupTimerHandle;
	float CleanupIntervalSeconds;
};
