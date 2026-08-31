#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "HSBuildPlacementComponent.generated.h"

class AHSBuildPlacementPreviewActor;
class UHSBuildingDefinition;

UENUM(BlueprintType)
enum class EHSBuildPlacementMode : uint8
{
	None,
	BuildingSchema,
	TemporaryShelter
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSBuildPlacementMessageSignature, FText, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSBuildPlacementSchemaSignature, const FHSPendingBuildingSchema&, PendingSchema);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSBuildPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSBuildPlacementComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintAssignable, Category = "Build Placement")
	FHSBuildPlacementMessageSignature OnBuildPlacementStarted;

	UPROPERTY(BlueprintAssignable, Category = "Build Placement")
	FHSBuildPlacementMessageSignature OnBuildPlacementCanceled;

	UPROPERTY(BlueprintAssignable, Category = "Build Placement")
	FHSBuildPlacementMessageSignature OnBuildPlacementBlocked;

	UPROPERTY(BlueprintAssignable, Category = "Build Placement")
	FHSBuildPlacementSchemaSignature OnBuildPlacementConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Build Placement")
	FHSBuildPlacementMessageSignature OnBuildPlacementActorConfirmed;

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	bool StartBasicHousePlacement();

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	bool StartBasicHouseMinorUpgradePlacement();

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	bool StartBuildingPlacement(FGameplayTag BuildingTag, FText DisplayName);

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	bool StartBasicCampsitePlacement();

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	bool StartPlacement(UHSBuildingDefinition* BuildingDefinition);

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	bool ConfirmPlacement();

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	void CancelPlacement();

	UFUNCTION(BlueprintCallable, Category = "Build Placement")
	void RotatePlacement(float Direction);

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	bool IsPlacementActive() const { return bPlacementActive; }

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	bool CanConfirmPlacement() const { return bCurrentPlacementValid; }

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	EHSBuildPlacementMode GetPlacementMode() const { return PlacementMode; }

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	FText GetPlacementStatusText() const { return PlacementStatusText; }

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	FVector GetPreviewLocation() const { return PreviewLocation; }

	UFUNCTION(BlueprintPure, Category = "Build Placement")
	float GetPreviewYawDegrees() const { return PreviewYawDegrees; }

private:
	bool StartTemporaryShelterPlacement();
	void UpdatePreview();
	void DestroyPreview();
	UHSBuildingDefinition* ResolveBasicHouseDefinition();
	UHSBuildingDefinition* ResolveBasicHouseMinorUpgradeDefinition();
	UHSBuildingDefinition* ResolveRuntimeBuildingDefinition(FGameplayTag BuildingTag, FText DisplayName);
	UHSSettlementPlanComponent* ResolveSettlementPlan() const;

	UPROPERTY(EditDefaultsOnly, Category = "Build Placement")
	TSoftObjectPtr<UHSBuildingDefinition> BasicHouseDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Build Placement", meta = (ClampMin = "100.0", Units = "cm"))
	float PlacementDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Build Placement", meta = (ClampMin = "100.0", Units = "cm"))
	float PlacementTraceDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Build Placement", meta = (ClampMin = "1.0", Units = "deg"))
	float RotationStepDegrees;

	UPROPERTY(EditDefaultsOnly, Category = "Build Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float PlacementHeightOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Build Placement", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxPlacementSlopeDegrees;

	UPROPERTY(Transient)
	TObjectPtr<UHSBuildingDefinition> ActiveBuildingDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UHSBuildingDefinition> RuntimeMinorUpgradeDefinition;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHSBuildingDefinition>> RuntimeBuildingDefinitions;

	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	EHSBuildPlacementMode PlacementMode;

	UPROPERTY(Transient)
	TObjectPtr<AHSBuildPlacementPreviewActor> PreviewActor;

	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	bool bPlacementActive;

	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	bool bCurrentPlacementValid;

	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	FVector PreviewLocation;

	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	float PreviewYawDegrees;

	UPROPERTY(VisibleAnywhere, Category = "Build Placement")
	FText PlacementStatusText;
};
