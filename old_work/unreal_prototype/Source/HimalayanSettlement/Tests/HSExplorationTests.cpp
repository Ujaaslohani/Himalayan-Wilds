#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Exploration/HSExplorationMarkerActor.h"
#include "Exploration/HSExplorationStateComponent.h"
#include "Interaction/HSInteractable.h"
#include "Player/HSPlayerCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSExplorationMarkerDefaultsTest,
	"HimalayanSettlement.Exploration.MarkerDefaultsAreCheap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSExplorationMarkerDefaultsTest::RunTest(const FString& Parameters)
{
	AHSExplorationMarkerActor* Marker = NewObject<AHSExplorationMarkerActor>();

	TestNotNull(TEXT("Exploration marker should be constructible."), Marker);
	TestFalse(TEXT("Exploration marker should not tick."), Marker->PrimaryActorTick.bCanEverTick);
	TestNotNull(TEXT("Exploration marker should own a prototype mesh."), Marker->GetPrototypeMesh());
	TestEqual(TEXT("Exploration marker mesh should use query-only collision for interaction traces."), Marker->GetPrototypeMesh() ? Marker->GetPrototypeMesh()->GetCollisionEnabled() : ECollisionEnabled::NoCollision, ECollisionEnabled::QueryOnly);
	TestFalse(TEXT("Exploration marker should start without a gameplay tag."), Marker->GetMarkerTag().IsValid());

	Marker->ConfigureMarker(FText::FromString(TEXT("Forest Edge")), TAG_HS_Location_Forest);
	TestTrue(TEXT("Configured marker should store its location tag."), Marker->GetMarkerTag() == TAG_HS_Location_Forest);
	TestEqual(TEXT("Configured marker should store display name."), Marker->GetDisplayName().ToString(), FString(TEXT("Forest Edge")));
	TestEqual(TEXT("Forest marker should use a tall readable cone scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.55f, 0.55f, 1.65f));

	Marker->ConfigureMarker(FText::FromString(TEXT("Trail Post")), TAG_HS_Location_Trail);
	TestEqual(TEXT("Trail marker should use a narrow post scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.22f, 0.22f, 1.75f));

	Marker->ConfigureMarker(FText::FromString(TEXT("Animal Ridge")), TAG_HS_Location_AnimalArea);
	TestEqual(TEXT("Animal marker should use a rounded sign scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.85f, 0.85f, 0.85f));

	Marker->ConfigureMarker(FText::FromString(TEXT("Village Core")), TAG_HS_Location_Village);
	TestEqual(TEXT("Village marker should use a broader post scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.7f, 0.7f, 1.15f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSExplorationStateDiscoversLocationsTest,
	"HimalayanSettlement.Exploration.StateDiscoversLocationsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSExplorationStateDiscoversLocationsTest::RunTest(const FString& Parameters)
{
	UHSExplorationStateComponent* Exploration = NewObject<UHSExplorationStateComponent>();

	TestNotNull(TEXT("Exploration state should be constructible."), Exploration);
	TestFalse(TEXT("Exploration state should not tick."), Exploration->PrimaryComponentTick.bCanEverTick);
	TestTrue(TEXT("Low-end streaming should default on."), Exploration->IsLowEndStreamingEnabled());
	TestTrue(TEXT("Road discovery should succeed."), Exploration->DiscoverLocation(TEXT("Road_01"), FText::FromString(TEXT("Road")), TAG_HS_Location_Road, FVector(100.0f, 0.0f, 0.0f)));
	TestFalse(TEXT("Duplicate discovery should be ignored."), Exploration->DiscoverLocation(TEXT("Road_01"), FText::FromString(TEXT("Road")), TAG_HS_Location_Road, FVector(200.0f, 0.0f, 0.0f)));
	TestEqual(TEXT("Only one discovered location should be stored."), Exploration->GetDiscoveredLocationCount(), 1);
	TestEqual(TEXT("Road discovery should be countable by tag."), Exploration->CountDiscoveredByTag(TAG_HS_Location_Road), 1);

	Exploration->SetLowEndStreamingEnabled(false);
	TestFalse(TEXT("Low-end streaming flag should be configurable."), Exploration->IsLowEndStreamingEnabled());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSExplorationMarkerInteractionDiscoversLocationTest,
	"HimalayanSettlement.Exploration.MarkerInteractionDiscoversLocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSExplorationMarkerInteractionDiscoversLocationTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	AHSExplorationMarkerActor* Marker = NewObject<AHSExplorationMarkerActor>();
	UHSExplorationStateComponent* Exploration = Player ? Player->GetExplorationStateComponent() : nullptr;

	TestNotNull(TEXT("Player should own exploration state."), Exploration);
	TestNotNull(TEXT("Marker should be constructible."), Marker);
	if (!Exploration || !Marker)
	{
		return false;
	}

	Marker->ConfigureMarker(FText::FromString(TEXT("Cold Stream")), TAG_HS_Location_River);
	TestTrue(TEXT("Configured marker should be interactable."), HSInteraction::CanInteract(Marker, Player));
	HSInteraction::Interact(Marker, Player);
	TestEqual(TEXT("Marker interaction should discover one location."), Exploration->GetDiscoveredLocationCount(), 1);
	TestEqual(TEXT("River discovery should be countable."), Exploration->CountDiscoveredByTag(TAG_HS_Location_River), 1);
	TestFalse(TEXT("Discovered marker should no longer be interactable."), HSInteraction::CanInteract(Marker, Player));

	return true;
}

#endif
