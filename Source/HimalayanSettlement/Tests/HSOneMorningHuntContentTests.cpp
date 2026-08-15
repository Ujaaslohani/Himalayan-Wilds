#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalDefinition.h"
#include "Building/HSBuildingDefinition.h"
#include "Engine/World.h"
#include "Interaction/HSInteractionTarget.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "Quests/HSQuestDefinition.h"
#include "Resources/HSItemDefinition.h"

namespace
{
template <typename T>
T* LoadRequiredAsset(FAutomationTestBase& Test, const TCHAR* ObjectPath)
{
	T* Asset = LoadObject<T>(nullptr, ObjectPath);
	Test.TestNotNull(FString::Printf(TEXT("Required asset should load: %s"), ObjectPath), Asset);
	return Asset;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSOneMorningHuntDefinitionsValidateTest,
	"HimalayanSettlement.Content.OneMorningHuntDefinitionsValidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSOneMorningHuntDefinitionsValidateTest::RunTest(const FString& Parameters)
{
	const UHSItemDefinition* Bow = LoadRequiredAsset<UHSItemDefinition>(*this, TEXT("/Game/Data/Items/DA_Item_BasicBow.DA_Item_BasicBow"));
	const UHSItemDefinition* Arrow = LoadRequiredAsset<UHSItemDefinition>(*this, TEXT("/Game/Data/Items/DA_Item_BasicArrow.DA_Item_BasicArrow"));
	const UHSItemDefinition* Meat = LoadRequiredAsset<UHSItemDefinition>(*this, TEXT("/Game/Data/Items/DA_Item_DeerMeat.DA_Item_DeerMeat"));
	const UHSItemDefinition* Hide = LoadRequiredAsset<UHSItemDefinition>(*this, TEXT("/Game/Data/Items/DA_Item_DeerHide.DA_Item_DeerHide"));
	const UHSAnimalDefinition* Deer = LoadRequiredAsset<UHSAnimalDefinition>(*this, TEXT("/Game/Data/Animals/DA_Animal_Deer.DA_Animal_Deer"));
	const UHSNPCDefinition* Hunter = LoadRequiredAsset<UHSNPCDefinition>(*this, TEXT("/Game/Data/NPCs/DA_NPC_Hunter.DA_NPC_Hunter"));
	const UHSBuildingDefinition* DryingRack = LoadRequiredAsset<UHSBuildingDefinition>(*this, TEXT("/Game/Data/Buildings/DA_Building_BasicDryingRack.DA_Building_BasicDryingRack"));
	const UHSQuestDefinition* Quest = LoadRequiredAsset<UHSQuestDefinition>(*this, TEXT("/Game/Data/Quests/DA_Quest_OneMorningHunt.DA_Quest_OneMorningHunt"));

	if (Bow) { TestTrue(TEXT("Basic bow definition should validate."), Bow->IsDefinitionValid()); }
	if (Arrow) { TestTrue(TEXT("Basic arrow definition should validate."), Arrow->IsDefinitionValid()); }
	if (Meat) { TestTrue(TEXT("Deer meat definition should validate."), Meat->IsDefinitionValid()); }
	if (Hide) { TestTrue(TEXT("Deer hide definition should validate."), Hide->IsDefinitionValid()); }
	if (Deer) { TestTrue(TEXT("Deer definition should validate."), Deer->IsDefinitionValid()); }
	if (Hunter) { TestTrue(TEXT("Hunter definition should validate."), Hunter->IsDefinitionValid()); }
	if (DryingRack) { TestTrue(TEXT("Drying rack definition should validate."), DryingRack->IsDefinitionValid()); }
	if (Quest) { TestTrue(TEXT("One Morning Hunt quest definition should validate."), Quest->IsDefinitionValid()); }

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSOneMorningHuntMapLoadsTest,
	"HimalayanSettlement.Content.OneMorningHuntMapLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSOneMorningHuntMapLoadsTest::RunTest(const FString& Parameters)
{
	const UWorld* World = LoadRequiredAsset<UWorld>(*this, TEXT("/Game/Maps/L_OneMorningHunt.L_OneMorningHunt"));
	TestNotNull(TEXT("One Morning Hunt map should have a persistent level."), World ? World->PersistentLevel.Get() : nullptr);

	bool bFoundHunter = false;
	bool bFoundInteractionTarget = false;
	if (World && World->PersistentLevel)
	{
		for (const AActor* Actor : World->PersistentLevel->Actors)
		{
			bFoundHunter |= Actor && Actor->IsA<AHSNPCActor>();
			bFoundInteractionTarget |= Actor && Actor->IsA<AHSInteractionTarget>();
		}
	}

	TestTrue(TEXT("One Morning Hunt map should contain a native NPC actor."), bFoundHunter);
	TestTrue(TEXT("One Morning Hunt map should contain a native interaction target."), bFoundInteractionTarget);

	return true;
}

#endif
