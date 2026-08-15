#include "Interaction/HSInteractionTarget.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSInteractionTarget"

AHSInteractionTarget::AHSInteractionTarget()
	: InteractionPrompt(LOCTEXT("DefaultInteractionPrompt", "Interact"))
	, bInteractionEnabled(true)
	, InteractionCount(0)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(45.0f, 45.0f, 60.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(0.65f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(SphereMesh.Object);
	}
}

FText AHSInteractionTarget::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	return InteractionPrompt;
}

bool AHSInteractionTarget::CanInteract_Implementation(const AActor* Interactor) const
{
	return bInteractionEnabled;
}

void AHSInteractionTarget::Interact_Implementation(AActor* Interactor)
{
	++InteractionCount;
	UE_LOG(LogHSInteraction, Log, TEXT("%s used %s. Interaction count: %d."), *GetNameSafe(Interactor), *GetNameSafe(this), InteractionCount);
}

#undef LOCTEXT_NAMESPACE
