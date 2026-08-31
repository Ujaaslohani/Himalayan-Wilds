#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/HSPlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerMovementDefaultsTest,
	"HimalayanSettlement.Player.MovementDefaultsSupportGroundedTraversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerMovementDefaultsTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();
	const UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should have a movement component."), Movement);
	TestEqual(TEXT("Player should start standing."), Player ? Player->GetPlayerStance() : EHSPlayerStance::Prone, EHSPlayerStance::Standing);
	TestTrue(TEXT("Player movement should support crouch."), Movement && Movement->CanEverCrouch());
	TestEqual(TEXT("Standing speed should be applied by default."), Movement ? Movement->MaxWalkSpeed : -1.0f, Player ? Player->GetStandingWalkSpeed() : -1.0f);
	TestTrue(TEXT("Jump velocity should support basic traversal."), Movement && Movement->JumpZVelocity > 0.0f);
	TestTrue(TEXT("Ground friction should resist slope sliding after jumps."), Movement && Movement->GroundFriction >= 10.0f);
	TestTrue(TEXT("Air control should stay restrained for low-slide slope jumps."), Movement && Movement->AirControl <= 0.1f);
	TestNotNull(TEXT("First-person camera should exist for stance height changes."), Player ? Player->GetFirstPersonCamera() : nullptr);
	TestTrue(TEXT("First-person camera should use the UE template's slight rear offset."), Player && Player->GetFirstPersonCamera() && Player->GetFirstPersonCamera()->GetRelativeLocation().X < 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerMovementStanceTransitionsTest,
	"HimalayanSettlement.Player.MovementStanceTransitionsApplySpeedAndHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerMovementStanceTransitionsTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
	UCapsuleComponent* Capsule = Player ? Player->GetCapsuleComponent() : nullptr;
	UCameraComponent* Camera = Player ? Player->GetFirstPersonCamera() : nullptr;

	TestNotNull(TEXT("Player should be constructible."), Player);
	TestNotNull(TEXT("Movement component should exist."), Movement);
	TestNotNull(TEXT("Capsule component should exist."), Capsule);
	TestNotNull(TEXT("Camera component should exist."), Camera);
	if (!Player || !Movement || !Capsule || !Camera)
	{
		return false;
	}

	const float StandingHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float StandingCameraZ = Camera->GetRelativeLocation().Z;

	Player->SetPlayerStance(EHSPlayerStance::Crouching);
	TestEqual(TEXT("Player should enter crouching stance."), Player->GetPlayerStance(), EHSPlayerStance::Crouching);
	TestEqual(TEXT("Crouch should apply crouch speed."), Movement->MaxWalkSpeed, Player->GetCrouchWalkSpeed());
	TestTrue(TEXT("Crouch should lower capsule height."), Capsule->GetUnscaledCapsuleHalfHeight() < StandingHalfHeight);
	TestTrue(TEXT("Crouch should lower camera height."), Camera->GetRelativeLocation().Z < StandingCameraZ);

	const float CrouchHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float CrouchCameraZ = Camera->GetRelativeLocation().Z;

	Player->SetPlayerStance(EHSPlayerStance::Prone);
	TestEqual(TEXT("Player should enter prone stance."), Player->GetPlayerStance(), EHSPlayerStance::Prone);
	TestTrue(TEXT("Prone query should be true in prone stance."), Player->IsProne());
	TestEqual(TEXT("Prone should apply prone speed."), Movement->MaxWalkSpeed, Player->GetProneMoveSpeed());
	TestTrue(TEXT("Prone should lower capsule below crouch."), Capsule->GetUnscaledCapsuleHalfHeight() < CrouchHalfHeight);
	TestTrue(TEXT("Prone should lower camera below crouch."), Camera->GetRelativeLocation().Z < CrouchCameraZ);

	Player->SetPlayerStance(EHSPlayerStance::Standing);
	TestEqual(TEXT("Player should return to standing stance."), Player->GetPlayerStance(), EHSPlayerStance::Standing);
	TestFalse(TEXT("Prone query should be false when standing."), Player->IsProne());
	TestEqual(TEXT("Standing should restore standing speed."), Movement->MaxWalkSpeed, Player->GetStandingWalkSpeed());
	TestEqual(TEXT("Standing should restore original capsule height."), Capsule->GetUnscaledCapsuleHalfHeight(), StandingHalfHeight);
	TestEqual(TEXT("Standing should restore original camera height."), static_cast<float>(Camera->GetRelativeLocation().Z), StandingCameraZ);

	return true;
}

#endif
