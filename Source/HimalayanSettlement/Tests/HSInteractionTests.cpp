#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/HSInteractionSensorComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSInteractionSensorDefaultsTest,
	"HimalayanSettlement.Interaction.SensorDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSInteractionSensorDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSInteractionSensorComponent* Sensor = NewObject<UHSInteractionSensorComponent>();

	TestNotNull(TEXT("Interaction sensor should be constructible."), Sensor);
	TestFalse(TEXT("Interaction sensor should not tick by default."), Sensor->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("Interaction distance should stay short for first-person use."), Sensor->GetInteractionDistance(), 300.0f);
	TestEqual(TEXT("Interaction traces should use visibility by default."), Sensor->GetTraceChannel().GetValue(), static_cast<uint8>(ECC_Visibility));

	return true;
}

#endif
