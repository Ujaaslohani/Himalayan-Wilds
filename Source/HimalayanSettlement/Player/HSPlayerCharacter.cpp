#include "Player/HSPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/HSLogCategories.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Hunting/HSHuntingComponent.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Settlement/HSSettlementUnlockComponent.h"

AHSPlayerCharacter::AHSPlayerCharacter()
	: PlayerStance(EHSPlayerStance::Standing)
	, StandingWalkSpeed(350.0f)
	, CrouchWalkSpeed(190.0f)
	, ProneMoveSpeed(85.0f)
	, StandingCapsuleHalfHeight(88.0f)
	, CrouchCapsuleHalfHeight(48.0f)
	, ProneCapsuleHalfHeight(36.0f)
	, StandingCameraHeight(64.0f)
	, CrouchCameraHeight(40.0f)
	, ProneCameraHeight(24.0f)
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	GetCapsuleComponent()->SetCapsuleHalfHeight(StandingCapsuleHalfHeight);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = StandingWalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchWalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 420.0f;
	GetCharacterMovement()->AirControl = 0.25f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, StandingCameraHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	InteractionSensor = CreateDefaultSubobject<UHSInteractionSensorComponent>(TEXT("InteractionSensor"));
	HuntingComponent = CreateDefaultSubobject<UHSHuntingComponent>(TEXT("HuntingComponent"));
	PlayerProgressComponent = CreateDefaultSubobject<UHSPlayerProgressComponent>(TEXT("PlayerProgressComponent"));
	ResourceCarryComponent = CreateDefaultSubobject<UHSResourceCarryComponent>(TEXT("ResourceCarryComponent"));
	SettlementUnlockComponent = CreateDefaultSubobject<UHSSettlementUnlockComponent>(TEXT("SettlementUnlockComponent"));

	PlayerMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("PlayerMappingContext"));

	MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveAction"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	PlayerMappingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(PlayerMappingContext));
	FEnhancedActionKeyMapping& BackwardMapping = PlayerMappingContext->MapKey(MoveAction, EKeys::S);
	BackwardMapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(PlayerMappingContext));
	BackwardMapping.Modifiers.Add(NewObject<UInputModifierNegate>(PlayerMappingContext));
	FEnhancedActionKeyMapping& LeftMapping = PlayerMappingContext->MapKey(MoveAction, EKeys::A);
	LeftMapping.Modifiers.Add(NewObject<UInputModifierNegate>(PlayerMappingContext));
	PlayerMappingContext->MapKey(MoveAction, EKeys::D);
	PlayerMappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("LookAction"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	PlayerMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	PlayerMappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);

	InteractAction = CreateDefaultSubobject<UInputAction>(TEXT("InteractAction"));
	InteractAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(InteractAction, EKeys::E);
	PlayerMappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);

	HuntAction = CreateDefaultSubobject<UInputAction>(TEXT("HuntAction"));
	HuntAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(HuntAction, EKeys::LeftMouseButton);
	PlayerMappingContext->MapKey(HuntAction, EKeys::Gamepad_RightTrigger);

	JumpAction = CreateDefaultSubobject<UInputAction>(TEXT("JumpAction"));
	JumpAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	PlayerMappingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Top);

	CrouchAction = CreateDefaultSubobject<UInputAction>(TEXT("CrouchAction"));
	CrouchAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	PlayerMappingContext->MapKey(CrouchAction, EKeys::Gamepad_LeftThumbstick);

	ProneAction = CreateDefaultSubobject<UInputAction>(TEXT("ProneAction"));
	ProneAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(ProneAction, EKeys::Z);
	PlayerMappingContext->MapKey(ProneAction, EKeys::Gamepad_FaceButton_Right);
}

void AHSPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ApplyInputMappingContext();
}

void AHSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHSPlayerCharacter::Move);
	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHSPlayerCharacter::Look);
	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::Interact);
	EnhancedInputComponent->BindAction(HuntAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::Hunt);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::StartJump);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHSPlayerCharacter::StopJump);
	EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::ToggleCrouchStance);
	EnhancedInputComponent->BindAction(ProneAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::ToggleProneStance);
}

void AHSPlayerCharacter::Move(const FInputActionValue& InputValue)
{
	if (PlayerStance == EHSPlayerStance::Prone && GetCharacterMovement()->IsFalling())
	{
		return;
	}

	const FVector2D MovementInput = InputValue.Get<FVector2D>();
	const FRotator ControlRotation = Controller ? Controller->GetControlRotation() : FRotator::ZeroRotator;
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementInput.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementInput.X);
}

void AHSPlayerCharacter::Look(const FInputActionValue& InputValue)
{
	const FVector2D LookInput = InputValue.Get<FVector2D>();
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void AHSPlayerCharacter::Interact(const FInputActionValue& InputValue)
{
	if (InteractionSensor)
	{
		InteractionSensor->TryInteract();
	}
	else
	{
		UE_LOG(LogHSInteraction, Warning, TEXT("%s has no interaction sensor."), *GetNameSafe(this));
	}
}

void AHSPlayerCharacter::Hunt(const FInputActionValue& InputValue)
{
	if (HuntingComponent)
	{
		HuntingComponent->TryHunt();
	}
	else
	{
		UE_LOG(LogHSAI, Warning, TEXT("%s has no hunting component."), *GetNameSafe(this));
	}
}

void AHSPlayerCharacter::StartJump(const FInputActionValue& InputValue)
{
	if (PlayerStance == EHSPlayerStance::Prone)
	{
		SetPlayerStance(EHSPlayerStance::Standing);
	}

	Jump();
}

void AHSPlayerCharacter::StopJump(const FInputActionValue& InputValue)
{
	StopJumping();
}

void AHSPlayerCharacter::ToggleCrouchStance(const FInputActionValue& InputValue)
{
	SetPlayerStance(PlayerStance == EHSPlayerStance::Crouching ? EHSPlayerStance::Standing : EHSPlayerStance::Crouching);
}

void AHSPlayerCharacter::ToggleProneStance(const FInputActionValue& InputValue)
{
	SetPlayerStance(PlayerStance == EHSPlayerStance::Prone ? EHSPlayerStance::Standing : EHSPlayerStance::Prone);
}

void AHSPlayerCharacter::SetPlayerStance(EHSPlayerStance NewStance)
{
	if (PlayerStance == NewStance)
	{
		return;
	}

	PlayerStance = NewStance;
	ApplyStanceMovementSettings();
}

void AHSPlayerCharacter::ApplyInputMappingContext()
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (InputSubsystem)
	{
		InputSubsystem->RemoveMappingContext(PlayerMappingContext);
		InputSubsystem->AddMappingContext(PlayerMappingContext, 0);
	}
}

void AHSPlayerCharacter::ApplyStanceMovementSettings()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!MovementComponent || !Capsule || !FirstPersonCamera)
	{
		return;
	}

	switch (PlayerStance)
	{
	case EHSPlayerStance::Crouching:
		UnCrouch(false);
		Capsule->SetCapsuleHalfHeight(CrouchCapsuleHalfHeight, true);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, CrouchCameraHeight));
		MovementComponent->MaxWalkSpeed = CrouchWalkSpeed;
		Crouch(false);
		break;

	case EHSPlayerStance::Prone:
		UnCrouch(false);
		Capsule->SetCapsuleHalfHeight(ProneCapsuleHalfHeight, true);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, ProneCameraHeight));
		MovementComponent->MaxWalkSpeed = ProneMoveSpeed;
		break;

	case EHSPlayerStance::Standing:
	default:
		UnCrouch(false);
		Capsule->SetCapsuleHalfHeight(StandingCapsuleHalfHeight, true);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, StandingCameraHeight));
		MovementComponent->MaxWalkSpeed = StandingWalkSpeed;
		break;
	}
}
