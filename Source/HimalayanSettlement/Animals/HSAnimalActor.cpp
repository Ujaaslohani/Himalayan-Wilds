#include "Animals/HSAnimalActor.h"

#include "Animals/HSAnimalDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Engine/World.h"
#include "Hunting/HSWoundClueActor.h"
#include "Resources/HSResourceCarryComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HSWorldCleanupSubsystem.h"

#define LOCTEXT_NAMESPACE "HSAnimalActor"

AHSAnimalActor::AHSAnimalActor()
	: FallbackDisplayName(LOCTEXT("DefaultDeerName", "Deer"))
	, CurrentHealth(1.0f)
	, RuntimeState(EHSAnimalRuntimeState::Idle)
	, bHarvested(false)
	, AlertStepDistance(60.0f)
	, FleeStepDistance(180.0f)
	, WoundClueClass(AHSWoundClueActor::StaticClass())
	, WoundClueLifetimeSeconds(45.0f)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(55.0f, 35.0f, 70.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
	PrototypeMesh->SetRelativeScale3D(FVector(1.1f, 0.65f, 0.55f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AHSAnimalActor::SetAnimalDefinition(UHSAnimalDefinition* NewAnimalDefinition)
{
	AnimalDefinition = NewAnimalDefinition;
	ResetRuntimeStateFromDefinition();
}

FText AHSAnimalActor::GetDisplayName() const
{
	return AnimalDefinition && !AnimalDefinition->DisplayName.IsEmpty() ? AnimalDefinition->DisplayName : FallbackDisplayName;
}

FGameplayTag AHSAnimalActor::GetSpeciesTag() const
{
	return AnimalDefinition ? AnimalDefinition->SpeciesTag : FGameplayTag();
}

float AHSAnimalActor::ApplyHuntingDamage(float DamageAmount, AActor* DamageInstigator)
{
	if (DamageAmount <= 0.0f || IsDead())
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	const float AppliedDamage = PreviousHealth - CurrentHealth;

	if (CurrentHealth <= 0.0f)
	{
		RuntimeState = EHSAnimalRuntimeState::Dead;
		UE_LOG(LogHSAI, Log, TEXT("%s killed %s."), *GetNameSafe(DamageInstigator), *GetNameSafe(this));
	}
	else
	{
		const FVector WoundClueLocation = GetActorLocation();
		RuntimeState = CurrentHealth <= (GetMaxHealth() * 0.5f) ? EHSAnimalRuntimeState::Fleeing : EHSAnimalRuntimeState::Alert;
		SpawnWoundClue(WoundClueLocation);
		ReactToNonlethalDamage(DamageInstigator);
		UE_LOG(LogHSAI, Verbose, TEXT("%s wounded %s; state is now %d."), *GetNameSafe(DamageInstigator), *GetNameSafe(this), static_cast<int32>(RuntimeState));
	}

	return AppliedDamage;
}

bool AHSAnimalActor::TryHarvest(AActor* Harvester, TArray<FHSItemStack>& OutDrops)
{
	OutDrops.Reset();
	if (!IsDead() || bHarvested || !AnimalDefinition)
	{
		return false;
	}

	HarvestedDrops = AnimalDefinition->HarvestDrops;
	OutDrops = HarvestedDrops;
	bHarvested = true;

	UE_LOG(
		LogHSInteraction,
		Log,
		TEXT("%s harvested %s and received %d drop stack(s)."),
		*GetNameSafe(Harvester),
		*GetNameSafe(this),
		OutDrops.Num());
	return true;
}

FText AHSAnimalActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	if (IsDead() && !bHarvested)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Name"), GetDisplayName());
		return FText::Format(LOCTEXT("HarvestAnimalPrompt", "Harvest {Name}"), Arguments);
	}

	if (IsDead())
	{
		return LOCTEXT("AnimalHarvestedPrompt", "Harvested");
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Name"), GetDisplayName());
	return FText::Format(LOCTEXT("ObserveAnimalPrompt", "Observe {Name}"), Arguments);
}

bool AHSAnimalActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return IsDead() && !bHarvested && AnimalDefinition;
}

void AHSAnimalActor::Interact_Implementation(AActor* Interactor)
{
	TArray<FHSItemStack> Drops;
	if (TryHarvest(Interactor, Drops))
	{
		if (UHSResourceCarryComponent* CarryComponent = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr)
		{
			CarryComponent->AddStacks(Drops);
		}
	}
}

void AHSAnimalActor::ResetRuntimeStateFromDefinition()
{
	CurrentHealth = GetMaxHealth();
	RuntimeState = EHSAnimalRuntimeState::Idle;
	bHarvested = false;
	HarvestedDrops.Reset();
}

float AHSAnimalActor::GetMaxHealth() const
{
	return AnimalDefinition ? FMath::Max(1.0f, AnimalDefinition->MaxHealth) : 1.0f;
}

void AHSAnimalActor::ReactToNonlethalDamage(AActor* DamageInstigator)
{
	FVector ReactionDirection = GetActorForwardVector();
	if (DamageInstigator)
	{
		ReactionDirection = GetActorLocation() - DamageInstigator->GetActorLocation();
		ReactionDirection.Z = 0.0f;
		if (ReactionDirection.IsNearlyZero())
		{
			ReactionDirection = -DamageInstigator->GetActorForwardVector();
		}
	}

	ReactionDirection.Z = 0.0f;
	ReactionDirection = ReactionDirection.GetSafeNormal();
	if (ReactionDirection.IsNearlyZero())
	{
		ReactionDirection = FVector::ForwardVector;
	}

	SetActorRotation(ReactionDirection.Rotation());
	const float StepDistance = RuntimeState == EHSAnimalRuntimeState::Fleeing ? FleeStepDistance : AlertStepDistance;
	AddActorWorldOffset(ReactionDirection * StepDistance, true);
}

void AHSAnimalActor::SpawnWoundClue(FVector ClueLocation)
{
	UWorld* World = GetWorld();
	if (!World || !WoundClueClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSWoundClueActor* WoundClue = World->SpawnActor<AHSWoundClueActor>(WoundClueClass, ClueLocation, FRotator::ZeroRotator, SpawnParameters);
	if (WoundClue)
	{
		WoundClue->ConfigureWoundClue(this, WoundClueLifetimeSeconds);
		if (UHSWorldCleanupSubsystem* CleanupSubsystem = World->GetSubsystem<UHSWorldCleanupSubsystem>())
		{
			CleanupSubsystem->RegisterTemporaryActor(WoundClue, WoundClueLifetimeSeconds, TEXT("WoundClue"));
		}
	}
}

#undef LOCTEXT_NAMESPACE
