#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalDefinition.h"
#include "Building/HSBuildingDefinition.h"
#include "NPC/HSNPCDefinition.h"
#include "Quests/HSQuestDefinition.h"
#include "Resources/HSItemDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSDefinitionValidationRejectsEmptyItem,
	"HimalayanSettlement.DataDefinitions.ItemRejectsMissingRequiredFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSDefinitionValidationRejectsEmptyItem::RunTest(const FString& Parameters)
{
	const UHSItemDefinition* ItemDefinition = NewObject<UHSItemDefinition>();
	TestFalse(TEXT("A blank item definition should not validate."), ItemDefinition->IsDefinitionValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSDefinitionValidationRejectsEmptyAnimal,
	"HimalayanSettlement.DataDefinitions.AnimalRejectsMissingRequiredFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSDefinitionValidationRejectsEmptyAnimal::RunTest(const FString& Parameters)
{
	const UHSAnimalDefinition* AnimalDefinition = NewObject<UHSAnimalDefinition>();
	TestFalse(TEXT("A blank animal definition should not validate."), AnimalDefinition->IsDefinitionValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSDefinitionValidationRejectsEmptyNPC,
	"HimalayanSettlement.DataDefinitions.NPCRejectsMissingRequiredFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSDefinitionValidationRejectsEmptyNPC::RunTest(const FString& Parameters)
{
	const UHSNPCDefinition* NPCDefinition = NewObject<UHSNPCDefinition>();
	TestFalse(TEXT("A blank NPC definition should not validate."), NPCDefinition->IsDefinitionValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSDefinitionValidationRejectsEmptyBuilding,
	"HimalayanSettlement.DataDefinitions.BuildingRejectsMissingRequiredFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSDefinitionValidationRejectsEmptyBuilding::RunTest(const FString& Parameters)
{
	const UHSBuildingDefinition* BuildingDefinition = NewObject<UHSBuildingDefinition>();
	TestFalse(TEXT("A blank building definition should not validate."), BuildingDefinition->IsDefinitionValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSDefinitionValidationRejectsEmptyQuest,
	"HimalayanSettlement.DataDefinitions.QuestRejectsMissingRequiredFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSDefinitionValidationRejectsEmptyQuest::RunTest(const FString& Parameters)
{
	const UHSQuestDefinition* QuestDefinition = NewObject<UHSQuestDefinition>();
	TestFalse(TEXT("A blank quest definition should not validate."), QuestDefinition->IsDefinitionValid());
	return true;
}

#endif
