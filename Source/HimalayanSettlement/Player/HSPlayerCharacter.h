#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HSPlayerCharacter.generated.h"

class UCameraComponent;
class UHSInteractionSensorComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

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

private:
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void Interact(const FInputActionValue& InputValue);
	void ApplyInputMappingContext();

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Player")
	TObjectPtr<UHSInteractionSensorComponent> InteractionSensor;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;
};
