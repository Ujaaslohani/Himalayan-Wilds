#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HSPlayerCharacter.generated.h"

class UCameraComponent;
class UHSHuntingComponent;
class UHSConstructionComponent;
class UHSInteractionSensorComponent;
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

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UHSInteractionSensorComponent* GetInteractionSensor() const { return InteractionSensor; }
	UHSHuntingComponent* GetHuntingComponent() const { return HuntingComponent; }
	UHSConstructionComponent* GetConstructionComponent() const { return ConstructionComponent; }
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

private:
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void Interact(const FInputActionValue& InputValue);
	void Hunt(const FInputActionValue& InputValue);
	void StartJump(const FInputActionValue& InputValue);
	void StopJump(const FInputActionValue& InputValue);
	void ToggleCrouchStance(const FInputActionValue& InputValue);
	void ToggleProneStance(const FInputActionValue& InputValue);
	void ApplyInputMappingContext();
	void ApplyStanceMovementSettings();

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSInteractionSensorComponent> InteractionSensor;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSHuntingComponent> HuntingComponent;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSConstructionComponent> ConstructionComponent;

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
};
