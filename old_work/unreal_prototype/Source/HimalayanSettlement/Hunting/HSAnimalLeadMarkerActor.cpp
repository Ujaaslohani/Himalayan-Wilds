#include "Hunting/HSAnimalLeadMarkerActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSLogCategories.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HSAnimalLeadMarkerActor"

AHSAnimalLeadMarkerActor::AHSAnimalLeadMarkerActor()
	: ReportDay(INDEX_NONE)
	, bExpired(false)
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	RootComponent = InteractionBounds;
	InteractionBounds->SetBoxExtent(FVector(80.0f, 80.0f, 80.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PrototypeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeMesh"));
	PrototypeMesh->SetupAttachment(InteractionBounds);
	PrototypeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrototypeMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PrototypeMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void AHSAnimalLeadMarkerActor::ConfigureLead(FGameplayTag NewAnimalTag, int32 NewReportDay)
{
	AnimalTag = NewAnimalTag;
	ReportDay = NewReportDay;
	bExpired = false;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

void AHSAnimalLeadMarkerActor::ExpireLead()
{
	bExpired = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	UE_LOG(LogHSAI, Log, TEXT("%s animal lead expired."), *GetNameSafe(this));
}

FText AHSAnimalLeadMarkerActor::GetInteractionPrompt_Implementation(const AActor* Interactor) const
{
	return bExpired
		? LOCTEXT("ExpiredAnimalLeadPrompt", "Animal lead expired")
		: FText::Format(LOCTEXT("AnimalLeadPrompt", "Animal sign: {Animal} nearby"), FText::FromString(AnimalTag.ToString()));
}

bool AHSAnimalLeadMarkerActor::CanInteract_Implementation(const AActor* Interactor) const
{
	return !bExpired && AnimalTag.IsValid();
}

void AHSAnimalLeadMarkerActor::Interact_Implementation(AActor* Interactor)
{
	UE_LOG(LogHSAI, Log, TEXT("%s checked animal lead %s for %s."), *GetNameSafe(Interactor), *GetNameSafe(this), *AnimalTag.ToString());
}

#undef LOCTEXT_NAMESPACE
