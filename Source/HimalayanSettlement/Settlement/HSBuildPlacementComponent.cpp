#include "Settlement/HSBuildPlacementComponent.h"

#include "Building/HSBuildingDefinition.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Settlement/HSBuildPlacementPreviewActor.h"
#include "Settlement/HSTemporaryShelterActor.h"

#define LOCTEXT_NAMESPACE "HSBuildPlacementComponent"

UHSBuildPlacementComponent::UHSBuildPlacementComponent()
	: BasicHouseDefinition(TSoftObjectPtr<UHSBuildingDefinition>(FSoftObjectPath(TEXT("/Game/Data/Buildings/DA_Building_BasicHouse.DA_Building_BasicHouse"))))
	, PlacementDistance(550.0f)
	, PlacementTraceDistance(1800.0f)
	, RotationStepDegrees(15.0f)
	, PlacementHeightOffset(0.0f)
	, MaxPlacementSlopeDegrees(35.0f)
	, PlacementMode(EHSBuildPlacementMode::None)
	, bPlacementActive(false)
	, bCurrentPlacementValid(false)
	, PreviewLocation(FVector::ZeroVector)
	, PreviewYawDegrees(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHSBuildPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
}

void UHSBuildPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPreview();
	Super::EndPlay(EndPlayReason);
}

void UHSBuildPlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bPlacementActive)
	{
		UpdatePreview();
	}
}

bool UHSBuildPlacementComponent::StartBasicHousePlacement()
{
	return StartPlacement(ResolveBasicHouseDefinition());
}

bool UHSBuildPlacementComponent::StartBasicHouseMinorUpgradePlacement()
{
	return StartPlacement(ResolveBasicHouseMinorUpgradeDefinition());
}

bool UHSBuildPlacementComponent::StartBasicCampsitePlacement()
{
	return StartTemporaryShelterPlacement();
}

bool UHSBuildPlacementComponent::StartPlacement(UHSBuildingDefinition* BuildingDefinition)
{
	if (!BuildingDefinition || !BuildingDefinition->IsDefinitionValid())
	{
		PlacementStatusText = LOCTEXT("InvalidBuildSchema", "No valid building schema selected");
		OnBuildPlacementBlocked.Broadcast(PlacementStatusText);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	ActiveBuildingDefinition = BuildingDefinition;
	PlacementMode = EHSBuildPlacementMode::BuildingSchema;
	bPlacementActive = true;
	bCurrentPlacementValid = false;
	PreviewYawDegrees = GetOwner() ? GetOwner()->GetActorRotation().Yaw : 0.0f;

	if (!PreviewActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = World->SpawnActor<AHSBuildPlacementPreviewActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	}

	SetComponentTickEnabled(true);
	UpdatePreview();
	PlacementStatusText = FText::Format(LOCTEXT("BuildPlacementStarted", "Placing {0}"), BuildingDefinition->DisplayName);
	OnBuildPlacementStarted.Broadcast(PlacementStatusText);
	UE_LOG(LogHSSettlement, Log, TEXT("%s started free-build placement for %s."), *GetNameSafe(GetOwner()), *BuildingDefinition->BuildingTag.ToString());
	return true;
}

bool UHSBuildPlacementComponent::StartTemporaryShelterPlacement()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	ActiveBuildingDefinition = nullptr;
	PlacementMode = EHSBuildPlacementMode::TemporaryShelter;
	bPlacementActive = true;
	bCurrentPlacementValid = false;
	PreviewYawDegrees = GetOwner() ? GetOwner()->GetActorRotation().Yaw : 0.0f;

	if (!PreviewActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = World->SpawnActor<AHSBuildPlacementPreviewActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	}

	SetComponentTickEnabled(true);
	UpdatePreview();
	PlacementStatusText = LOCTEXT("CampsitePlacementStarted", "Placing Basic Campsite");
	OnBuildPlacementStarted.Broadcast(PlacementStatusText);
	UE_LOG(LogHSSettlement, Log, TEXT("%s started free-build placement for a Basic Campsite."), *GetNameSafe(GetOwner()));
	return true;
}

bool UHSBuildPlacementComponent::ConfirmPlacement()
{
	if (!bPlacementActive)
	{
		return false;
	}

	UpdatePreview();
	if (!bCurrentPlacementValid)
	{
		OnBuildPlacementBlocked.Broadcast(PlacementStatusText);
		return false;
	}

	if (PlacementMode == EHSBuildPlacementMode::TemporaryShelter)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			PlacementStatusText = LOCTEXT("CampsitePlacementFailed", "Could not place campsite");
			OnBuildPlacementBlocked.Broadcast(PlacementStatusText);
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AHSTemporaryShelterActor>(PreviewLocation + FVector(0.0f, 0.0f, 45.0f), FRotator(0.0f, PreviewYawDegrees, 0.0f), SpawnParameters);

		const FText ConfirmedMessage = LOCTEXT("CampsitePlacementConfirmed", "Basic Campsite placed");
		OnBuildPlacementActorConfirmed.Broadcast(ConfirmedMessage);
		UE_LOG(LogHSSettlement, Log, TEXT("%s confirmed free-build Basic Campsite at %s."), *GetNameSafe(GetOwner()), *PreviewLocation.ToCompactString());
		DestroyPreview();
		bPlacementActive = false;
		bCurrentPlacementValid = false;
		ActiveBuildingDefinition = nullptr;
		PlacementMode = EHSBuildPlacementMode::None;
		SetComponentTickEnabled(false);
		return true;
	}

	if (!ActiveBuildingDefinition)
	{
		PlacementStatusText = LOCTEXT("BuildPlacementMissingDefinition", "No building schema selected");
		OnBuildPlacementBlocked.Broadcast(PlacementStatusText);
		return false;
	}

	UHSSettlementPlanComponent* SettlementPlan = ResolveSettlementPlan();
	FHSPendingBuildingSchema PendingSchema;
	if (!SettlementPlan || !SettlementPlan->PlaceSchema(ActiveBuildingDefinition, PreviewLocation, PreviewYawDegrees, PendingSchema))
	{
		PlacementStatusText = LOCTEXT("BuildPlacementFailed", "Could not place schema");
		OnBuildPlacementBlocked.Broadcast(PlacementStatusText);
		return false;
	}

	OnBuildPlacementConfirmed.Broadcast(PendingSchema);
	UE_LOG(LogHSSettlement, Log, TEXT("%s confirmed free-build schema %s at %s."), *GetNameSafe(GetOwner()), *PendingSchema.BuildingTag.ToString(), *PendingSchema.Location.ToCompactString());
	DestroyPreview();
	bPlacementActive = false;
	bCurrentPlacementValid = false;
	ActiveBuildingDefinition = nullptr;
	PlacementMode = EHSBuildPlacementMode::None;
	SetComponentTickEnabled(false);
	return true;
}

void UHSBuildPlacementComponent::CancelPlacement()
{
	if (!bPlacementActive)
	{
		return;
	}

	const FText CanceledMessage = LOCTEXT("BuildPlacementCanceled", "Build placement canceled");
	DestroyPreview();
	bPlacementActive = false;
	bCurrentPlacementValid = false;
	ActiveBuildingDefinition = nullptr;
	PlacementMode = EHSBuildPlacementMode::None;
	SetComponentTickEnabled(false);
	PlacementStatusText = CanceledMessage;
	OnBuildPlacementCanceled.Broadcast(CanceledMessage);
}

void UHSBuildPlacementComponent::RotatePlacement(float Direction)
{
	if (!bPlacementActive || FMath::IsNearlyZero(Direction))
	{
		return;
	}

	PreviewYawDegrees = FRotator::NormalizeAxis(PreviewYawDegrees + (FMath::Sign(Direction) * RotationStepDegrees));
	UpdatePreview();
}

void UHSBuildPlacementComponent::UpdatePreview()
{
	if (!bPlacementActive)
	{
		return;
	}

	FVector ViewLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	FRotator ViewRotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		if (const AController* Controller = PawnOwner->GetController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}

	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd = TraceStart + ViewRotation.Vector() * PlacementTraceDistance;
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HSBuildPlacementTrace), false, GetOwner());

	UWorld* World = GetWorld();
	bool bHasGroundHit = false;
	FVector GroundNormal = FVector::UpVector;
	if (World && World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		PreviewLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, PlacementHeightOffset);
		GroundNormal = Hit.ImpactNormal.GetSafeNormal();
		bHasGroundHit = true;
	}
	else
	{
		PreviewLocation = ViewLocation + ViewRotation.Vector() * PlacementDistance;
		PreviewLocation.Z = GetOwner() ? GetOwner()->GetActorLocation().Z : PreviewLocation.Z;
	}

	const float MinGroundNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxPlacementSlopeDegrees));
	const bool bGroundIsUsable = bHasGroundHit && GroundNormal.Z >= MinGroundNormalZ;
	if (PlacementMode == EHSBuildPlacementMode::TemporaryShelter)
	{
		bCurrentPlacementValid = bGroundIsUsable;
		PlacementStatusText = bCurrentPlacementValid
			? LOCTEXT("CampsitePlacementValid", "Basic Campsite placement ready")
			: LOCTEXT("CampsiteNeedsGround", "Aim at flatter ground for campsite placement");
	}
	else
	{
		FText FailureReason;
		UHSSettlementPlanComponent* SettlementPlan = ResolveSettlementPlan();
		bCurrentPlacementValid = bGroundIsUsable && ActiveBuildingDefinition && SettlementPlan && SettlementPlan->CanPlaceSchema(ActiveBuildingDefinition, PreviewLocation, &FailureReason);
		PlacementStatusText = bCurrentPlacementValid
			? FText::Format(LOCTEXT("BuildPlacementValid", "{0} placement ready"), ActiveBuildingDefinition->DisplayName)
			: (!bGroundIsUsable ? LOCTEXT("BuildPlacementNeedsGround", "Aim at flatter ground for schema placement") : FailureReason);
	}

	if (PreviewActor)
	{
		PreviewActor->SetActorLocationAndRotation(PreviewLocation, FRotator(0.0f, PreviewYawDegrees, 0.0f));
		PreviewActor->SetPlacementValid(bCurrentPlacementValid);
	}
}

void UHSBuildPlacementComponent::DestroyPreview()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

UHSBuildingDefinition* UHSBuildPlacementComponent::ResolveBasicHouseDefinition()
{
	if (BasicHouseDefinition.IsNull())
	{
		return nullptr;
	}

	return BasicHouseDefinition.LoadSynchronous();
}

UHSBuildingDefinition* UHSBuildPlacementComponent::ResolveBasicHouseMinorUpgradeDefinition()
{
	if (!RuntimeMinorUpgradeDefinition)
	{
		RuntimeMinorUpgradeDefinition = NewObject<UHSBuildingDefinition>(this, TEXT("RuntimeBasicHouseMinorUpgradeDefinition"));
		RuntimeMinorUpgradeDefinition->DisplayName = NSLOCTEXT("HSBuildPlacementComponent", "MinorUpgradeDisplayName", "Minor House Upgrade");
		RuntimeMinorUpgradeDefinition->BuildingTag = TAG_HS_Building_BasicHouseMinorUpgrade;
	}

	return RuntimeMinorUpgradeDefinition;
}

UHSSettlementPlanComponent* UHSBuildPlacementComponent::ResolveSettlementPlan() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
