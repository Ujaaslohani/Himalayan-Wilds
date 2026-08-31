#include "Hunting/HSHuntingComponent.h"

#include "Animals/HSAnimalActor.h"
#include "Camera/CameraComponent.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "GameFramework/Actor.h"
#include "Hunting/HSArrowProjectileActor.h"

#define LOCTEXT_NAMESPACE "HSHuntingComponent"

UHSHuntingComponent::UHSHuntingComponent()
	: HuntDistance(1200.0f)
	, HuntDamage(75.0f)
	, SpearDamage(45.0f)
	, SpearRange(180.0f)
	, TraceChannel(ECC_Visibility)
	, ArrowProjectileClass(AHSArrowProjectileActor::StaticClass())
	, ArrowSpeed(3200.0f)
	, ArrowSpawnForwardOffset(45.0f)
	, ShotCooldown(1.2f)
	, LastShotTime(-TNumericLimits<double>::Max())
	, DrawStartTime(0.0)
	, bBowDrawn(false)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSHuntingComponent::TryHunt()
{
	const AActor* Owner = GetOwner();
	const UHSPlayerEquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr;
	if (Equipment && Equipment->GetEquippedWeaponMode() == EHSPlayerWeaponMode::Spear)
	{
		return TrySpearThrust();
	}

	return StartBowDraw() && ReleaseBowShot();
}

bool UHSHuntingComponent::StartBowDraw()
{
	const AActor* Owner = GetOwner();
	const UHSPlayerEquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr;
	if (!Equipment || !Equipment->CanUseBow())
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "BowRequired", "Equip a bow and arrows first."));
		UE_LOG(LogHSAI, Verbose, TEXT("%s tried to hunt without an equipped bow and arrows."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!IsShotReady())
	{
		OnHuntingFeedback.Broadcast(FText::Format(
			NSLOCTEXT("HSHuntingComponent", "BowNocking", "Nocking arrow: {0}s."),
			FText::AsNumber(GetShotCooldownRemaining())));
		return false;
	}

	bBowDrawn = true;
	DrawStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "BowDrawing", "Bow drawn."));
	return true;
}

bool UHSHuntingComponent::ReleaseBowShot()
{
	if (!bBowDrawn)
	{
		return false;
	}

	bBowDrawn = false;

	const AActor* Owner = GetOwner();
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	if (!ResolveTraceView(TraceStart, TraceDirection))
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "NoAim", "No clear aim."));
		return false;
	}

	UHSPlayerEquipmentComponent* MutableEquipment = Owner ? Owner->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr;
	if (!MutableEquipment || !MutableEquipment->ConsumeArrow())
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "ArrowConsumeFailed", "No arrow ready."));
		return false;
	}

	AHSArrowProjectileActor* Arrow = FireArrow(TraceStart, TraceDirection);
	if (!Arrow)
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "ArrowSpawnFailed", "Could not loose arrow."));
		return false;
	}

	LastShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "ArrowLoosed", "Arrow loosed."));
	return true;
}

bool UHSHuntingComponent::TrySpearThrust()
{
	const AActor* Owner = GetOwner();
	const UHSPlayerEquipmentComponent* Equipment = Owner ? Owner->FindComponentByClass<UHSPlayerEquipmentComponent>() : nullptr;
	if (!Equipment || Equipment->GetEquippedWeaponMode() != EHSPlayerWeaponMode::Spear)
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "SpearRequired", "Equip a spear first."));
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	if (!ResolveTraceView(TraceStart, TraceDirection))
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "SpearNoAim", "No clear thrust."));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	const FVector TraceEnd = TraceStart + TraceDirection.GetSafeNormal() * SpearRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HSSpearTrace), false, GetOwner());
	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, TraceChannel, QueryParams))
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "SpearMiss", "Spear missed."));
		return false;
	}

	AHSAnimalActor* Animal = Cast<AHSAnimalActor>(Hit.GetActor());
	if (!Animal || Animal->IsDead())
	{
		OnHuntingFeedback.Broadcast(NSLOCTEXT("HSHuntingComponent", "SpearHitWorld", "Spear struck."));
		return true;
	}

	const float AppliedDamage = Animal->ApplyHuntingDamage(SpearDamage, GetOwner());
	OnHuntingFeedback.Broadcast(Animal->IsDead()
		? FText::Format(LOCTEXT("SpearKilledAnimal", "Spear killed {0}."), Animal->GetDisplayName())
		: FText::Format(LOCTEXT("SpearHitAnimal", "Spear hit {0}."), Animal->GetDisplayName()));
	return AppliedDamage > 0.0f;
}

float UHSHuntingComponent::GetShotCooldownRemaining() const
{
	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	return FMath::Max(0.0f, ShotCooldown - static_cast<float>(CurrentTime - LastShotTime));
}

AHSAnimalActor* UHSHuntingComponent::FindHuntTarget(FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ForwardVector;
	if (!ResolveTraceView(TraceStart, TraceDirection))
	{
		return nullptr;
	}

	return FindHuntTargetFromView(TraceStart, TraceDirection, OutHit);
}

AHSAnimalActor* UHSHuntingComponent::FindHuntTargetFromView(FVector TraceStart, FVector TraceDirection, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector TraceEnd = TraceStart + TraceDirection.GetSafeNormal() * HuntDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HSHuntingTrace), false, GetOwner());
	QueryParams.bReturnPhysicalMaterial = false;

	if (!World->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, TraceChannel, QueryParams))
	{
		return nullptr;
	}

	AHSAnimalActor* Animal = Cast<AHSAnimalActor>(OutHit.GetActor());
	return Animal && !Animal->IsDead() ? Animal : nullptr;
}

AHSArrowProjectileActor* UHSHuntingComponent::FireArrow(FVector TraceStart, FVector TraceDirection)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !ArrowProjectileClass)
	{
		return nullptr;
	}

	const FVector LaunchDirection = TraceDirection.GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Owner;
	SpawnParameters.Instigator = Cast<APawn>(Owner);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector SpawnLocation = TraceStart + (LaunchDirection * ArrowSpawnForwardOffset);
	AHSArrowProjectileActor* Arrow = World->SpawnActor<AHSArrowProjectileActor>(ArrowProjectileClass, SpawnLocation, LaunchDirection.Rotation(), SpawnParameters);
	if (!Arrow)
	{
		return nullptr;
	}

	Arrow->OnArrowFeedback.AddUniqueDynamic(this, &UHSHuntingComponent::HandleArrowFeedback);
	Arrow->LaunchArrow(Owner, SpawnLocation, LaunchDirection, HuntDamage, HuntDistance, ArrowSpeed);
	return Arrow;
}

bool UHSHuntingComponent::ApplyDamageToTarget(AHSAnimalActor* Target)
{
	if (!Target || Target->IsDead())
	{
		return false;
	}

	const float AppliedDamage = Target->ApplyHuntingDamage(HuntDamage, GetOwner());
	if (AppliedDamage > 0.0f)
	{
		OnHuntingFeedback.Broadcast(Target->IsDead()
			? FText::Format(LOCTEXT("AnimalKilled", "Killed {0}."), Target->GetDisplayName())
			: FText::Format(LOCTEXT("AnimalHit", "Hit {0}."), Target->GetDisplayName()));
	}

	UE_LOG(
		LogHSAI,
		Log,
		TEXT("%s applied %.2f hunting damage to %s."),
		*GetNameSafe(GetOwner()),
		AppliedDamage,
		*GetNameSafe(Target));
	return AppliedDamage > 0.0f;
}

void UHSHuntingComponent::HandleArrowFeedback(FText Message)
{
	OnHuntingFeedback.Broadcast(Message);
}

bool UHSHuntingComponent::ResolveTraceView(FVector& OutStart, FVector& OutDirection) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	if (const UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>())
	{
		OutStart = Camera->GetComponentLocation();
		OutDirection = Camera->GetForwardVector();
		return true;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	Owner->GetActorEyesViewPoint(OutStart, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}

#undef LOCTEXT_NAMESPACE
