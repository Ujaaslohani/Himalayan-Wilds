#include "Settlement/HSBuildPlacementComponent.h"

#include "Building/HSBuildingDefinition.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Settlement/HSBuildPlacementPreviewActor.h"

#define LOCTEXT_NAMESPACE "HSBuildPlacementComponent"

UHSBuildPlacementComponent::UHSBuildPlacementComponent()
	: BasicHouseDefinition(TSoftObjectPtr<UHSBuildingDefinition>(FSoftObjectPath(TEXT("/Game/Data/Buildings/DA_Building_BasicHouse.DA_Building_BasicHouse"))))
	, PlacementDistance(550.0f)
	, PlacementTraceDistance(1800.0f)
	, RotationStepDegrees(15.0f)
	, PlacementHeightOffset(45.0f)
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

bool UHSBuildPlacementComponent::ConfirmPlacement()
{
	if (!bPlacementActive || !ActiveBuildingDefinition)
	{
		return false;
	}

	UpdatePreview();
	if (!bCurrentPlacementValid)
	{
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
	if (!bPlacementActive || !ActiveBuildingDefinition)
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
	if (World && World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		PreviewLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, PlacementHeightOffset);
	}
	else
	{
		PreviewLocation = ViewLocation + ViewRotation.Vector() * PlacementDistance;
		PreviewLocation.Z = GetOwner() ? GetOwner()->GetActorLocation().Z + PlacementHeightOffset : PreviewLocation.Z;
	}

	FText FailureReason;
	UHSSettlementPlanComponent* SettlementPlan = ResolveSettlementPlan();
	bCurrentPlacementValid = SettlementPlan && SettlementPlan->CanPlaceSchema(ActiveBuildingDefinition, PreviewLocation, &FailureReason);
	PlacementStatusText = bCurrentPlacementValid
		? FText::Format(LOCTEXT("BuildPlacementValid", "{0} placement ready"), ActiveBuildingDefinition->DisplayName)
		: FailureReason;

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

UHSSettlementPlanComponent* UHSBuildPlacementComponent::ResolveSettlementPlan() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
