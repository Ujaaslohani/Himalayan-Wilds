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
	TestEqual(TEXT("Forest marker should use a tall readable cone scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.55f, 0.55f, 1.65f));

	Marker->ConfigureMarker(FText::FromString(TEXT("Trail Post")), TAG_HS_Location_Trail);
	TestEqual(TEXT("Trail marker should use a narrow post scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.22f, 0.22f, 1.75f));

	Marker->ConfigureMarker(FText::FromString(TEXT("Animal Ridge")), TAG_HS_Location_AnimalArea);
	TestEqual(TEXT("Animal marker should use a rounded sign scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.85f, 0.85f, 0.85f));

	Marker->ConfigureMarker(FText::FromString(TEXT("Village Core")), TAG_HS_Location_Village);
	TestEqual(TEXT("Village marker should use a broader post scale."), Marker->GetPrototypeMesh()->GetRelativeScale3D(), FVector(0.7f, 0.7f, 1.15f));

	return true;
}

#endif
