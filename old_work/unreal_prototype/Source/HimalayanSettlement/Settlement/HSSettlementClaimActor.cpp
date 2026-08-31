#include "Settlement/HSSettlementClaimActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSSettlementClaimActor"

AHSSettlementClaimActor::AHSSettlementClaimActor()
	: bClaimed(false)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(90.0f, 90.0f, 70.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.12f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

FText AHSSettlementClaimActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	return bClaimed
		? LOCTEXT("SettlementClaimedPrompt", "Settlement site claimed")
		: LOCTEXT("ClaimSettlementPrompt", "Claim settlement site");
}

bool AHSSettlementClaimActor::CanInteract_Implementation(const AActor* Interactor) const
{
	const UHSSettlementPlanComponent* SettlementPlan = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	return SettlementPlan && !bClaimed && !SettlementPlan->HasSettlementClaim();
}

void AHSSettlementClaimActor::Interact_Implementation(AActor* Interactor)
{
	UHSSettlementPlanComponent* SettlementPlan = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	if (!SettlementPlan || bClaimed)
	{
		return;
	}

	bClaimed = SettlementPlan->ClaimSettlement(GetActorLocation());
	UE_LOG(LogHSSettlement, Log, TEXT("%s interacted with settlement claim %s. Claimed: %s."), *GetNameSafe(Interactor), *GetNameSafe(this), bClaimed ? TEXT("true") : TEXT("false"));
}

#undef LOCTEXT_NAMESPACE
