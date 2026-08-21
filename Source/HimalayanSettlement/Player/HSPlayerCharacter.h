#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Resources/HSItemStack.h"
#include "Settlement/HSConstructionComponent.h"
#include "World/HSWorldTimeComponent.h"
#include "HSPlayerCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UHSAnimalIntelComponent;
class UHSBuildPlacementComponent;
class UHSExplorationStateComponent;
class UHSPlayerEquipmentComponent;
class UHSHuntingComponent;
class UHSInteractionSensorComponent;
class UHSSettlementPersistenceComponent;
class UHSSettlementPopulationComponent;
class UHSPlayerHealthComponent;
class UHSPlayerProgressComponent;
class UHSResourceCarryComponent;
class UHSSettlementPlanComponent;
class UHSSettlementUnlockComponent;
class UInputAction;
class UInputMappingContext;
class AHSResourcePickupActor;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EHSPlayerStance : uint8
{
	Standing,
	Crouching,
	Prone
};

UCLASS()
class HIMALAYANSETTLEMENT_API AHSPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHSPlayerCharacter();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UHSInteractionSensorComponent* GetInteractionSensor() const { return InteractionSensor; }
	UHSAnimalIntelComponent* GetAnimalIntelComponent() const { return AnimalIntelComponent; }
	UHSExplorationStateComponent* GetExplorationStateComponent() const { return ExplorationStateComponent; }
	UHSBuildPlacementComponent* GetBuildPlacementComponent() const { return BuildPlacementComponent; }
	UHSPlantEffectComponent* GetPlantEffectComponent() const { return PlantEffectComponent; }
	UHSHuntingComponent* GetHuntingComponent() const { return HuntingComponent; }
	UHSConstructionComponent* GetConstructionComponent() const { return ConstructionComponent; }
	UHSSettlementPersistenceComponent* GetSettlementPersistenceComponent() const { return SettlementPersistenceComponent; }
	UHSPlayerHealthComponent* GetPlayerHealthComponent() const { return PlayerHealthComponent; }
	UHSPlayerProgressComponent* GetPlayerProgressComponent() const { return PlayerProgressComponent; }
	UHSResourceCarryComponent* GetResourceCarryComponent() const { return ResourceCarryComponent; }
	UHSSettlementPlanComponent* GetSettlementPlanComponent() const { return SettlementPlanComponent; }
	UHSSettlementUnlockComponent* GetSettlementUnlockComponent() const { return SettlementUnlockComponent; }
	UHSSettlementPopulationComponent* GetSettlementPopulationComponent() const { return SettlementPopulationComponent; }
	UHSPlayerEquipmentComponent* GetPlayerEquipmentComponent() const { return PlayerEquipmentComponent; }
	UStaticMeshComponent* GetBowGripMesh() const { return BowGripMesh; }
	UStaticMeshComponent* GetBowUpperLimbMesh() const { return BowUpperLimbMesh; }
	UStaticMeshComponent* GetBowLowerLimbMesh() const { return BowLowerLimbMesh; }
	UStaticMeshComponent* GetBowStringMesh() const { return BowStringMesh; }

	UFUNCTION(BlueprintPure, Category = "Player|Movement")
	EHSPlayerStance GetPlayerStance() const { return PlayerStance; }

	UFUNCTION(BlueprintPure, Category = "Player|Movement")
	bool IsProne() const { return PlayerStance == EHSPlayerStance::Prone; }

	UFUNCTION(BlueprintPure, Category = "Player|Movement")
	float GetStandingWalkSpeed() const { return StandingWalkSpeed; }

	UFUNCTION(BlueprintPure, Category = "Player|Movement")
	float GetCrouchWalkSpeed() const { return CrouchWalkSpeed; }

	UFUNCTION(BlueprintPure, Category = "Player|Movement")
	float GetProneMoveSpeed() const { return ProneMoveSpeed; }

	UFUNCTION(BlueprintCallable, Category = "Player|Movement")
	void SetPlayerStance(EHSPlayerStance NewStance);

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	bool IsPauseOptionsOpen() const { return bPauseOptionsOpen; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	bool IsBuildMenuOpen() const { return bBuildMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	int32 GetSelectedInventoryIndex() const { return SelectedInventoryIndex; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	int32 GetSelectedBuildMenuIndex() const { return SelectedBuildMenuIndex; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	int32 GetBuildMenuEntryCount() const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FText GetBuildMenuEntryLabel(int32 EntryIndex) const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FText GetBuildMenuEntryCategory(int32 EntryIndex) const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FText GetBuildMenuEntryDescription(int32 EntryIndex) const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	float GetBuildMenuEntryRequiredDays(int32 EntryIndex) const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FGameplayTag GetBuildMenuEntryTag(int32 EntryIndex) const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	int32 GetGraphicsQualityPreset() const { return GraphicsQualityPreset; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	int32 GetMouseSensitivityPreset() const { return MouseSensitivityPreset; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	int32 GetMasterVolumePreset() const { return MasterVolumePreset; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	int32 GetSelectedManualSaveSlotIndex() const { return SelectedManualSaveSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FString GetSelectedManualSaveSlotName() const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FString GetSelectedManualSaveSlotLabel() const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	bool IsPerformanceOverlayEnabled() const { return bPerformanceOverlayEnabled; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	bool IsEmergencyLowEndModeEnabled() const { return bEmergencyLowEndModeEnabled; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FString GetGraphicsQualityPresetName() const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FString GetMouseSensitivityPresetName() const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	FString GetMasterVolumePresetName() const;

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	const FText& GetLastMenuFeedback() const { return LastMenuFeedback; }

	UFUNCTION(BlueprintPure, Category = "Player|Menu")
	bool HasActiveMenuFeedback() const;

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void ClosePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool SaveFromPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool LoadFromPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void TogglePauseOptions();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void ToggleInventoryMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void ToggleBuildMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void StartBasicHouseFromBuildMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void StartBasicHouseMinorUpgradeFromBuildMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void StartBasicCampsiteFromBuildMenu();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void StartSelectedBuildMenuEntry();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool StartBuildMenuEntry(int32 EntryIndex);

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void SelectBuildMenuDelta(int32 Delta);

	bool CanStartBuildMenuEntry(int32 EntryIndex, FText* OutReason = nullptr) const;

	bool CanStartBasicHouseFromBuildMenu(FText* OutReason = nullptr) const;

	bool CanStartBasicHouseMinorUpgradeFromBuildMenu(FText* OutReason = nullptr) const;

	bool CanStartBasicCampsiteFromBuildMenu(FText* OutReason = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool InspectInventoryStack(int32 StackIndex);

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool DropInventoryStackQuantity(int32 StackIndex, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool InspectSelectedInventoryStack();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool DropSelectedInventoryStackQuantity(int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	bool UseSelectedInventoryStack();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void SelectInventoryDelta(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void CycleGraphicsQualityPreset();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void CycleMouseSensitivityPreset();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void CycleMasterVolumePreset();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void CycleManualSaveSlot();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void TogglePerformanceOverlay();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void ToggleEmergencyLowEndMode();

	UFUNCTION(BlueprintCallable, Category = "Player|Menu")
	void SetEmergencyLowEndModeEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintCallable, Category = "Player|Persistence")
	bool AutosaveSettlementState(const FString& Reason);

private:
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void Interact(const FInputActionValue& InputValue);
	void Hunt(const FInputActionValue& InputValue);
	void ReleaseHunt(const FInputActionValue& InputValue);
	void StartJump(const FInputActionValue& InputValue);
	void StopJump(const FInputActionValue& InputValue);
	void ToggleCrouchStance(const FInputActionValue& InputValue);
	void ToggleProneStance(const FInputActionValue& InputValue);
	void ToggleBuildPlacement(const FInputActionValue& InputValue);
	void RotateBuildPlacementLeft(const FInputActionValue& InputValue);
	void RotateBuildPlacementRight(const FInputActionValue& InputValue);
	void ToggleInventory(const FInputActionValue& InputValue);
	void HandleEscapeAction(const FInputActionValue& InputValue);
	void HandlePauseMenuResume(const FInputActionValue& InputValue);
	void HandlePauseMenuSave(const FInputActionValue& InputValue);
	void HandlePauseMenuLoad(const FInputActionValue& InputValue);
	void HandlePauseMenuOptions(const FInputActionValue& InputValue);
	void HandlePauseMenuExit(const FInputActionValue& InputValue);
	void HandleMenuUtilityAction(const FInputActionValue& InputValue);
	void HandleMenuSensitivityAction(const FInputActionValue& InputValue);
	void HandleMenuVolumeAction(const FInputActionValue& InputValue);
	void HandleMenuSaveSlotAction(const FInputActionValue& InputValue);
	void HandlePerformanceOverlayAction(const FInputActionValue& InputValue);
	void HandleEmergencyLowEndModeAction(const FInputActionValue& InputValue);
	void HandleEquipmentAction(const FInputActionValue& InputValue);
	void SaveSettlementState(const FInputActionValue& InputValue);
	void LoadSettlementState(const FInputActionValue& InputValue);
	void ApplyInputMappingContext();
	void ApplyStanceMovementSettings();
	void SetPauseMenuOpen(bool bNewOpen);
	void SetInventoryOpen(bool bNewOpen);
	void SetBuildMenuOpen(bool bNewOpen);
	void CloseNonPauseMenus();
	void ExitGameFromPauseMenu() const;
	bool SelectFocusedNPCServiceDelta(int32 Delta);
	void BindAutosaveSources();
	void SetMenuFeedback(FText Feedback);
	void RefreshBowVisuals();
	bool SpawnDroppedInventoryStack(const FHSItemStack& DroppedStack);
	void ClampSelectedBuildMenuIndex();
	float GetMouseSensitivityScale() const;
	float GetMasterVolumeScale() const;
	void ApplyMasterVolumePreset();
	void ClampSelectedInventoryIndex();

	UFUNCTION()
	void HandleAutosaveMilestoneCompleted(FGameplayTag MilestoneTag);

	UFUNCTION()
	void HandleAutosaveConstructionChanged(const FHSConstructionJob& ConstructionJob);

	UFUNCTION()
	void HandleAutosaveTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays);

	UFUNCTION()
	void HandlePlantEffectMovementChanged(const struct FHSPlantEffectState& PlantEffect);

	UFUNCTION()
	void HandleBowEquippedChanged(bool bBowEquipped);

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> BowGripMesh;

	UPROPERTY(VisibleAnywhere, Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> BowUpperLimbMesh;

	UPROPERTY(VisibleAnywhere, Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> BowLowerLimbMesh;

	UPROPERTY(VisibleAnywhere, Category = "Player|Equipment")
	TObjectPtr<UStaticMeshComponent> BowStringMesh;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSInteractionSensorComponent> InteractionSensor;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSAnimalIntelComponent> AnimalIntelComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSExplorationStateComponent> ExplorationStateComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSBuildPlacementComponent> BuildPlacementComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSPlantEffectComponent> PlantEffectComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSHuntingComponent> HuntingComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSConstructionComponent> ConstructionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSSettlementPersistenceComponent> SettlementPersistenceComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSPlayerHealthComponent> PlayerHealthComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSPlayerProgressComponent> PlayerProgressComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSResourceCarryComponent> ResourceCarryComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSSettlementPlanComponent> SettlementPlanComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSSettlementUnlockComponent> SettlementUnlockComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSSettlementPopulationComponent> SettlementPopulationComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSPlayerEquipmentComponent> PlayerEquipmentComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Inventory")
	TSubclassOf<AHSResourcePickupActor> ResourcePickupActorClass;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HuntAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ProneAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BuildPlacementAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RotateBuildLeftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RotateBuildRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EscapeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuResumeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuSaveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuLoadAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuOptionsAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuExitAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuUtilityAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuSensitivityAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuVolumeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MenuSaveSlotAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PerformanceOverlayAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EmergencyLowEndModeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EquipmentAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SaveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadAction;

	UPROPERTY(VisibleAnywhere, Category = "Player|Movement")
	EHSPlayerStance PlayerStance;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "0.0"))
	float StandingWalkSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "0.0"))
	float CrouchWalkSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "0.0"))
	float ProneMoveSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "1.0"))
	float StandingCapsuleHalfHeight;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "1.0"))
	float CrouchCapsuleHalfHeight;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "1.0"))
	float ProneCapsuleHalfHeight;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement")
	float StandingCameraHeight;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement")
	float CrouchCameraHeight;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement")
	float ProneCameraHeight;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	bool bPauseMenuOpen;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	bool bPauseOptionsOpen;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	bool bInventoryOpen;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	bool bBuildMenuOpen;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	int32 SelectedInventoryIndex;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	int32 SelectedBuildMenuIndex;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	int32 GraphicsQualityPreset;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	int32 MouseSensitivityPreset;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	int32 MasterVolumePreset;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	int32 SelectedManualSaveSlotIndex;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	bool bPerformanceOverlayEnabled;

	UPROPERTY(VisibleAnywhere, Category = "Player|Menu")
	bool bEmergencyLowEndModeEnabled;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Menu", meta = (ClampMin = "0.1", Units = "s"))
	float MenuFeedbackDuration;

	UPROPERTY(Transient)
	FText LastMenuFeedback;

	double MenuFeedbackExpireTime;

	UPROPERTY(Transient)
	bool bAutosaveSuppressed;

	UPROPERTY(Transient)
	TWeakObjectPtr<UHSWorldTimeComponent> BoundAutosaveWorldTime;
};
