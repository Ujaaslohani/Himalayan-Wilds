#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "World/HSWorldTimeComponent.h"
#include "HSPlayerCharacter.generated.h"

class UCameraComponent;
class UHSAnimalIntelComponent;
class UHSBuildPlacementComponent;
class UHSHuntingComponent;
class UHSInteractionSensorComponent;
class UHSSettlementPersistenceComponent;
class UHSPlayerProgressComponent;
class UHSResourceCarryComponent;
class UHSSettlementPlanComponent;
class UHSSettlementUnlockComponent;
class UInputAction;
class UInputMappingContext;
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
	UHSBuildPlacementComponent* GetBuildPlacementComponent() const { return BuildPlacementComponent; }
	UHSPlantEffectComponent* GetPlantEffectComponent() const { return PlantEffectComponent; }
	UHSHuntingComponent* GetHuntingComponent() const { return HuntingComponent; }
	UHSConstructionComponent* GetConstructionComponent() const { return ConstructionComponent; }
	UHSSettlementPersistenceComponent* GetSettlementPersistenceComponent() const { return SettlementPersistenceComponent; }
	UHSPlayerProgressComponent* GetPlayerProgressComponent() const { return PlayerProgressComponent; }
	UHSResourceCarryComponent* GetResourceCarryComponent() const { return ResourceCarryComponent; }
	UHSSettlementPlanComponent* GetSettlementPlanComponent() const { return SettlementPlanComponent; }
	UHSSettlementUnlockComponent* GetSettlementUnlockComponent() const { return SettlementUnlockComponent; }

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

	UFUNCTION(BlueprintCallable, Category = "Player|Persistence")
	bool AutosaveSettlementState(const FString& Reason);

private:
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void Interact(const FInputActionValue& InputValue);
	void Hunt(const FInputActionValue& InputValue);
	void StartJump(const FInputActionValue& InputValue);
	void StopJump(const FInputActionValue& InputValue);
	void ToggleCrouchStance(const FInputActionValue& InputValue);
	void ToggleProneStance(const FInputActionValue& InputValue);
	void ToggleBuildPlacement(const FInputActionValue& InputValue);
	void RotateBuildPlacementLeft(const FInputActionValue& InputValue);
	void RotateBuildPlacementRight(const FInputActionValue& InputValue);
	void HandleEscapeAction(const FInputActionValue& InputValue);
	void HandlePauseMenuResume(const FInputActionValue& InputValue);
	void HandlePauseMenuSave(const FInputActionValue& InputValue);
	void HandlePauseMenuLoad(const FInputActionValue& InputValue);
	void HandlePauseMenuOptions(const FInputActionValue& InputValue);
	void HandlePauseMenuExit(const FInputActionValue& InputValue);
	void SaveSettlementState(const FInputActionValue& InputValue);
	void LoadSettlementState(const FInputActionValue& InputValue);
	void ApplyInputMappingContext();
	void ApplyStanceMovementSettings();
	void SetPauseMenuOpen(bool bNewOpen);
	void ExitGameFromPauseMenu() const;
	void BindAutosaveSources();

	UFUNCTION()
	void HandleAutosaveMilestoneCompleted(FGameplayTag MilestoneTag);

	UFUNCTION()
	void HandleAutosaveConstructionChanged(const FHSConstructionJob& ConstructionJob);

	UFUNCTION()
	void HandleAutosaveTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays);

	UFUNCTION()
	void HandlePlantEffectMovementChanged(const struct FHSPlantEffectState& PlantEffect);

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSInteractionSensorComponent> InteractionSensor;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSAnimalIntelComponent> AnimalIntelComponent;

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
	TObjectPtr<UHSPlayerProgressComponent> PlayerProgressComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSResourceCarryComponent> ResourceCarryComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSSettlementPlanComponent> SettlementPlanComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSSettlementUnlockComponent> SettlementUnlockComponent;

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

	UPROPERTY(Transient)
	bool bAutosaveSuppressed;

	UPROPERTY(Transient)
	TWeakObjectPtr<UHSWorldTimeComponent> BoundAutosaveWorldTime;
};
