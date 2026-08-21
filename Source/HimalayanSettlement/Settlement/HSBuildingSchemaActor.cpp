#include "Settlement/HSBuildingSchemaActor.h"

#include "Building/HSBuildingDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSBuildingSchemaActor"

AHSBuildingSchemaActor::AHSBuildingSchemaActor()
	: bSchemaPlaced(false)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(130.0f, 100.0f, 80.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(1.8f, 1.25f, 0.55f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AHSBuildingSchemaActor::SetBuildingDefinition(UHSBuildingDefinition* NewBuildingDefinition)
{
	BuildingDefinition = NewBuildingDefinition;
}

FText AHSBuildingSchemaActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	if (bSchemaPlaced)
	{
		return LOCTEXT("SchemaPendingPrompt", "House schema pending");
	}

	if (!BuildingDefinition)
	{
		return LOCTEXT("SchemaUnavailablePrompt", "Schema unavailable");
	}

	return FText::Format(LOCTEXT("PlaceSchemaPrompt", "Place {0} schema"), BuildingDefinition->DisplayName);
}

bool AHSBuildingSchemaActor::CanInteract_Implementation(const AActor* Interactor) const
{
	if (bSchemaPlaced)
	{
		return false;
	}

	const UHSSettlementPlanComponent* SettlementPlan = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	return SettlementPlan && SettlementPlan->CanPlaceSchema(BuildingDefinition, GetActorLocation());
}

void AHSBuildingSchemaActor::Interact_Implementation(AActor* Interactor)
{
	UHSSettlementPlanComponent* SettlementPlan = Interactor ? Interactor->FindComponentByClass<UHSSettlementPlanComponent>() : nullptr;
	if (!SettlementPlan || bSchemaPlaced)
	{
		return;
	}

	FHSPendingBuildingSchema PendingSchema;
	bSchemaPlaced = SettlementPlan->PlaceSchema(BuildingDefinition, GetActorLocation(), GetActorRotation().Yaw, PendingSchema);
	UE_LOG(LogHSSettlement, Log, TEXT("%s interacted with schema %s. Placed: %s."), *GetNameSafe(Interactor), *GetNameSafe(this), bSchemaPlaced ? TEXT("true") : TEXT("false"));
}

#undef LOCTEXT_NAMESPACE
