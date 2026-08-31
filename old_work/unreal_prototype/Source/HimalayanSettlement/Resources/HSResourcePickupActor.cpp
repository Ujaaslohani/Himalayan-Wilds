#include "Resources/HSResourcePickupActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "Resources/HSResourceCarryComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSResourcePickupActor"

AHSResourcePickupActor::AHSResourcePickupActor()
	: bPickedUp(false)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(28.0f, 28.0f, 24.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.18f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AHSResourcePickupActor::SetItemStack(const FHSItemStack& NewItemStack)
{
	ItemStack = NewItemStack;
}

FText AHSResourcePickupActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	(void)Interactor;

	if (bPickedUp)
	{
		return LOCTEXT("PickedUpPrompt", "Picked up");
	}

	if (!ItemStack.IsValidStack())
	{
		return LOCTEXT("EmptyPickupPrompt", "Empty pickup");
	}

	return FText::Format(
		LOCTEXT("PickupPrompt", "Pick up {0} x{1}"),
		FText::FromString(ItemStack.ItemTag.ToString()),
		FText::AsNumber(ItemStack.Quantity));
}

bool AHSResourcePickupActor::CanInteract_Implementation(const AActor* Interactor) const
{
	(void)Interactor;
	return !bPickedUp && ItemStack.IsValidStack();
}

void AHSResourcePickupActor::Interact_Implementation(AActor* Interactor)
{
	if (bPickedUp || !ItemStack.IsValidStack())
	{
		return;
	}

	UHSResourceCarryComponent* CarryComponent = Interactor ? Interactor->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
	if (!CarryComponent)
	{
		return;
	}

	CarryComponent->AddStack(ItemStack);
	bPickedUp = true;
	SetActorHiddenInGame(true);
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UE_LOG(LogHSInteraction, Log, TEXT("%s picked up %s x%d."), *GetNameSafe(Interactor), *ItemStack.ItemTag.ToString(), ItemStack.Quantity);
}

#undef LOCTEXT_NAMESPACE
