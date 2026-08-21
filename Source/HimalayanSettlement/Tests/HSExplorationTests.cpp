#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Exploration/HSExplorationMarkerActor.h"

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
	TestEqual(TEXT("Exploration marker mesh should not use collision."), Marker->GetPrototypeMesh() ? Marker->GetPrototypeMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Exploration marker should start without a gameplay tag."), Marker->GetMarkerTag().IsValid());

	Marker->ConfigureMarker(FText::FromString(TEXT("Forest Edge")), TAG_HS_Location_Forest);
	TestTrue(TEXT("Configured marker should store its location tag."), Marker->GetMarkerTag() == TAG_HS_Location_Forest);
	TestEqual(TEXT("Configured marker should store display name."), Marker->GetDisplayName().ToString(), FString(TEXT("Forest Edge")));

	return true;
}

#endif
