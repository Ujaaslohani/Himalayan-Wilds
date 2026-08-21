#include "Herbalism/HSPlantActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "Herbalism/HSPlantDefinition.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSPlantActor"

AHSPlantActor::AHSPlantActor()
	: bConsumed(false)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(35.0f, 35.0f, 45.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.7f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(ConeMesh.Object);
	}
}

void AHSPlantActor::SetPlantDefinition(UHSPlantDefinition* NewPlantDefinition)
{
	PlantDefinition = NewPlantDefinition;
}

FText AHSPlantActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	if (bConsumed)
	{
		return LOCTEXT("PlantConsumedPrompt", "Plant gathered");
	}

	if (!PlantDefinition || !PlantDefinition->IsDefinitionValid())
	{
		return LOCTEXT("UnknownPlantPrompt", "Unknown plant");
	}

	const UHSPlantEffectComponent* PlantEffectComponent = Interactor ? Interactor->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;
	if (!PlantEffectComponent || !PlantEffectComponent->HasPlantKnowledge(PlantDefinition->PlantTag))
	{
		return LOCTEXT("NeedHerbalistKnowledgePrompt", "Ask herbalist about this plant");
	}

	return FText::Format(LOCTEXT("UseKnownPlantPrompt", "Use {Plant}"), PlantDefinition->DisplayName);
}

bool AHSPlantActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return true;
}

void AHSPlantActor::Interact_Implementation(AActor* Interactor)
{
	if (bConsumed || !PlantDefinition)
	{
		return;
	}

	UHSPlantEffectComponent* PlantEffectComponent = Interactor ? Interactor->FindComponentByClass<UHSPlantEffectComponent>() : nullptr;
	if (PlantEffectComponent && PlantEffectComponent->ApplyPlantEffect(PlantDefinition))
	{
		bConsumed = true;
		SetActorHiddenInGame(true);
		InteractionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogHSInteraction, Log, TEXT("%s used plant %s."), *GetNameSafe(Interactor), *GetNameSafe(this));
	}
}

#undef LOCTEXT_NAMESPACE
