#include "Settlement/HSConstructionComponent.h"

#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Settlement/HSCompletedBuildingActor.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

UHSConstructionComponent::UHSConstructionComponent()
	: HouseConstructionDays(1.0f)
	, LastObservedElapsedDays(0.0f)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSConstructionComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToWorldTime();
}

void UHSConstructionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHSWorldTimeComponent* WorldTime = BoundWorldTime.Get())
	{
		WorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSConstructionComponent::HandleTimeAdvanced);
	}

	BoundWorldTime.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UHSConstructionComponent::StartNextPendingSchema(UHSSettlementPlanComponent* SettlementPlan)
{
	if (!SettlementPlan || HasActiveJob() || !SettlementPlan->HasAnyPendingSchema())
	{
		return false;
	}

	for (const FHSPendingBuildingSchema& PendingSchema : SettlementPlan->GetPendingSchemas())
	{
		if (PendingSchema.BuildingTag.IsValid() && !HasCompletedBuilding(PendingSchema.BuildingTag))
		{
			FHSPendingBuildingSchema ConsumedSchema;
			if (!SettlementPlan->ConsumePendingSchema(PendingSchema.BuildingTag, ConsumedSchema))
			{
				return false;
			}

			FHSConstructionJob ConstructionJob;
			ConstructionJob.BuildingTag = ConsumedSchema.BuildingTag;
			ConstructionJob.Location = ConsumedSchema.Location;
			ConstructionJob.YawDegrees = ConsumedSchema.YawDegrees;
			ConstructionJob.RequiredDays = ResolveRequiredDays(ConsumedSchema.BuildingTag);
			ConstructionJob.ProgressDays = 0.0f;
			ConstructionJob.State = EHSConstructionJobState::Active;

			ConstructionJobs.Add(ConstructionJob);
			OnConstructionJobStarted.Broadcast(ConstructionJobs.Last());
			UE_LOG(LogHSSettlement, Log, TEXT("%s started construction job for %s."), *GetNameSafe(GetOwner()), *ConstructionJob.BuildingTag.ToString());
			return true;
		}
	}

	return false;
}

bool UHSConstructionComponent::AdvanceConstruction(float DeltaDays)
{
	if (DeltaDays <= 0.0f)
	{
		return false;
	}

	FHSConstructionJob* ActiveJob = FindActiveJob();
	if (!ActiveJob)
	{
		return false;
	}

	ActiveJob->ProgressDays = FMath::Min(ActiveJob->RequiredDays, ActiveJob->ProgressDays + DeltaDays);
	if (ActiveJob->ProgressDays < ActiveJob->RequiredDays)
	{
		UE_LOG(LogHSSettlement, Verbose, TEXT("%s progressed %s construction to %.2f / %.2f days."), *GetNameSafe(GetOwner()), *ActiveJob->BuildingTag.ToString(), ActiveJob->ProgressDays, ActiveJob->RequiredDays);
		return true;
	}

	ActiveJob->State = EHSConstructionJobState::Complete;
	CompletedBuildings.AddTag(ActiveJob->BuildingTag);
	SpawnCompletedBuildingVisual(*ActiveJob);
	OnConstructionJobCompleted.Broadcast(*ActiveJob);
	UE_LOG(LogHSSettlement, Log, TEXT("%s completed construction job for %s."), *GetNameSafe(GetOwner()), *ActiveJob->BuildingTag.ToString());
	return true;
}

void UHSConstructionComponent::BindToWorldTime()
{
	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	if (!WorldTime || BoundWorldTime.Get() == WorldTime)
	{
		return;
	}

	if (UHSWorldTimeComponent* PreviousWorldTime = BoundWorldTime.Get())
	{
		PreviousWorldTime->OnTimeAdvanced.RemoveDynamic(this, &UHSConstructionComponent::HandleTimeAdvanced);
	}

	BoundWorldTime = WorldTime;
	LastObservedElapsedDays = WorldTime->GetElapsedDays();
	WorldTime->OnTimeAdvanced.AddUniqueDynamic(this, &UHSConstructionComponent::HandleTimeAdvanced);
}

bool UHSConstructionComponent::HasActiveJob() const
{
	return FindActiveJob() != nullptr;
}

bool UHSConstructionComponent::HasCompletedBuilding(FGameplayTag BuildingTag) const
{
	return BuildingTag.IsValid() && CompletedBuildings.HasTagExact(BuildingTag);
}

void UHSConstructionComponent::RestoreConstructionState(const TArray<FHSConstructionJob>& NewConstructionJobs, const FGameplayTagContainer& NewCompletedBuildings)
{
	ConstructionJobs.Reset();
	bool bRestoredActiveJob = false;

	for (FHSConstructionJob ConstructionJob : NewConstructionJobs)
	{
		if (!ConstructionJob.IsValid())
		{
			continue;
		}

		ConstructionJob.ProgressDays = FMath::Clamp(ConstructionJob.ProgressDays, 0.0f, ConstructionJob.RequiredDays);
		if (ConstructionJob.State == EHSConstructionJobState::Complete || ConstructionJob.ProgressDays >= ConstructionJob.RequiredDays)
		{
			ConstructionJob.State = EHSConstructionJobState::Complete;
			ConstructionJob.ProgressDays = ConstructionJob.RequiredDays;
			ConstructionJobs.Add(ConstructionJob);
			continue;
		}

		if (!bRestoredActiveJob)
		{
			ConstructionJob.State = EHSConstructionJobState::Active;
			ConstructionJobs.Add(ConstructionJob);
			bRestoredActiveJob = true;
		}
	}

	CompletedBuildings.Reset();
	for (const FGameplayTag& BuildingTag : NewCompletedBuildings)
	{
		if (BuildingTag.IsValid())
		{
			CompletedBuildings.AddTag(BuildingTag);
		}
	}

	for (const FHSConstructionJob& ConstructionJob : ConstructionJobs)
	{
		if (ConstructionJob.IsComplete())
		{
			CompletedBuildings.AddTag(ConstructionJob.BuildingTag);
		}
	}

	RebuildCompletedBuildingVisuals();
}

void UHSConstructionComponent::HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays)
{
	const float DeltaDays = FMath::Max(0.0f, ElapsedDays - LastObservedElapsedDays);
	LastObservedElapsedDays = ElapsedDays;
	AdvanceConstruction(DeltaDays);
}

float UHSConstructionComponent::ResolveRequiredDays(FGameplayTag BuildingTag) const
{
	if (BuildingTag == TAG_HS_Building_BasicHouseMinorUpgrade || BuildingTag == TAG_HS_Building_BasicDryingRack)
	{
		return 0.25f;
	}

	if (BuildingTag == TAG_HS_Building_BasicHouseMidUpgrade || BuildingTag == TAG_HS_Building_StorageHut || BuildingTag == TAG_HS_Building_WatchPost || BuildingTag == TAG_HS_Building_HerbalHut)
	{
		return 0.5f;
	}

	return HouseConstructionDays;
}

void UHSConstructionComponent::SpawnCompletedBuildingVisual(const FHSConstructionJob& ConstructionJob)
{
	if (!ConstructionJob.IsComplete() || !ConstructionJob.BuildingTag.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const TWeakObjectPtr<AHSCompletedBuildingActor>& ExistingActor : SpawnedCompletedBuildingActors)
	{
		if (const AHSCompletedBuildingActor* CompletedActor = ExistingActor.Get())
		{
			if (CompletedActor->GetBuildingTag() == ConstructionJob.BuildingTag)
			{
				return;
			}
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.Name = MakeUniqueObjectName(World, AHSCompletedBuildingActor::StaticClass(), FName(TEXT("CompletedBuilding")));
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSCompletedBuildingActor* CompletedBuilding = World->SpawnActor<AHSCompletedBuildingActor>(
		ConstructionJob.Location,
		FRotator(0.0f, ConstructionJob.YawDegrees, 0.0f),
		SpawnParameters);

	if (CompletedBuilding)
	{
		CompletedBuilding->ConfigureCompletedBuilding(ConstructionJob.BuildingTag);
		SpawnedCompletedBuildingActors.Add(CompletedBuilding);
	}
}

void UHSConstructionComponent::RebuildCompletedBuildingVisuals()
{
	for (const TWeakObjectPtr<AHSCompletedBuildingActor>& ExistingActor : SpawnedCompletedBuildingActors)
	{
		if (AHSCompletedBuildingActor* CompletedActor = ExistingActor.Get())
		{
			CompletedActor->Destroy();
		}
	}

	SpawnedCompletedBuildingActors.Reset();

	for (const FHSConstructionJob& ConstructionJob : ConstructionJobs)
	{
		if (ConstructionJob.IsComplete())
		{
			SpawnCompletedBuildingVisual(ConstructionJob);
		}
	}
}

FHSConstructionJob* UHSConstructionComponent::FindActiveJob()
{
	for (FHSConstructionJob& ConstructionJob : ConstructionJobs)
	{
		if (ConstructionJob.State == EHSConstructionJobState::Active)
		{
			return &ConstructionJob;
		}
	}

	return nullptr;
}

const FHSConstructionJob* UHSConstructionComponent::FindActiveJob() const
{
	for (const FHSConstructionJob& ConstructionJob : ConstructionJobs)
	{
		if (ConstructionJob.State == EHSConstructionJobState::Active)
		{
			return &ConstructionJob;
		}
	}

	return nullptr;
}
