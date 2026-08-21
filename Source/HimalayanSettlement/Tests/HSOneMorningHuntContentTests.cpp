#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animals/HSAnimalActor.h"
#include "Animals/HSAnimalDefinition.h"
#include "Building/HSBuildingDefinition.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Exploration/HSExplorationMarkerActor.h"
#include "Exploration/HSPrototypeVisualActor.h"
#include "Herbalism/HSPlantActor.h"
#include "Herbalism/HSPlantDefinition.h"
#include "Interaction/HSInteractionTarget.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "Quests/HSQuestDefinition.h"
#include "Quests/HSQuestHandoffComponent.h"
#include "Resources/HSItemDefinition.h"
#include "Settlement/HSRestPointActor.h"
#include "Settlement/HSSettlementClaimActor.h"
#include "Settlement/HSTemporaryShelterActor.h"

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
	const UHSAnimalDefinition* Boar = LoadRequiredAsset<UHSAnimalDefinition>(*this, TEXT("/Game/Data/Animals/DA_Animal_Boar.DA_Animal_Boar"));
	const UHSAnimalDefinition* Hare = LoadRequiredAsset<UHSAnimalDefinition>(*this, TEXT("/Game/Data/Animals/DA_Animal_Hare.DA_Animal_Hare"));
	const UHSNPCDefinition* Hunter = LoadRequiredAsset<UHSNPCDefinition>(*this, TEXT("/Game/Data/NPCs/DA_NPC_Hunter.DA_NPC_Hunter"));
	const UHSNPCDefinition* Builder = LoadRequiredAsset<UHSNPCDefinition>(*this, TEXT("/Game/Data/NPCs/DA_NPC_Builder.DA_NPC_Builder"));
	const UHSNPCDefinition* Herbalist = LoadRequiredAsset<UHSNPCDefinition>(*this, TEXT("/Game/Data/NPCs/DA_NPC_Herbalist.DA_NPC_Herbalist"));
	const UHSNPCDefinition* Soldier = LoadRequiredAsset<UHSNPCDefinition>(*this, TEXT("/Game/Data/NPCs/DA_NPC_Soldier.DA_NPC_Soldier"));
	const UHSPlantDefinition* SpeedHerb = LoadRequiredAsset<UHSPlantDefinition>(*this, TEXT("/Game/Data/Plants/DA_Plant_SpeedHerb.DA_Plant_SpeedHerb"));
	const UHSBuildingDefinition* DryingRack = LoadRequiredAsset<UHSBuildingDefinition>(*this, TEXT("/Game/Data/Buildings/DA_Building_BasicDryingRack.DA_Building_BasicDryingRack"));
	const UHSBuildingDefinition* BasicHouse = LoadRequiredAsset<UHSBuildingDefinition>(*this, TEXT("/Game/Data/Buildings/DA_Building_BasicHouse.DA_Building_BasicHouse"));
	const UHSQuestDefinition* Quest = LoadRequiredAsset<UHSQuestDefinition>(*this, TEXT("/Game/Data/Quests/DA_Quest_OneMorningHunt.DA_Quest_OneMorningHunt"));

	if (Bow) { TestTrue(TEXT("Basic bow definition should validate."), Bow->IsDefinitionValid()); }
	if (Arrow) { TestTrue(TEXT("Basic arrow definition should validate."), Arrow->IsDefinitionValid()); }
	if (Meat) { TestTrue(TEXT("Deer meat definition should validate."), Meat->IsDefinitionValid()); }
	if (Hide) { TestTrue(TEXT("Deer hide definition should validate."), Hide->IsDefinitionValid()); }
	if (Deer) { TestTrue(TEXT("Deer definition should validate."), Deer->IsDefinitionValid()); }
	if (Boar) { TestTrue(TEXT("Boar definition should validate."), Boar->IsDefinitionValid()); }
	if (Hare) { TestTrue(TEXT("Hare definition should validate."), Hare->IsDefinitionValid()); }
	if (Hunter) { TestTrue(TEXT("Hunter definition should validate."), Hunter->IsDefinitionValid()); }
	if (Builder) { TestTrue(TEXT("Builder definition should validate."), Builder->IsDefinitionValid()); }
	if (Herbalist) { TestTrue(TEXT("Herbalist definition should validate."), Herbalist->IsDefinitionValid()); }
	if (Soldier) { TestTrue(TEXT("Soldier definition should validate."), Soldier->IsDefinitionValid()); }
	if (SpeedHerb) { TestTrue(TEXT("Speed herb definition should validate."), SpeedHerb->IsDefinitionValid()); }
	if (DryingRack) { TestTrue(TEXT("Drying rack definition should validate."), DryingRack->IsDefinitionValid()); }
	if (BasicHouse) { TestTrue(TEXT("Basic house definition should validate."), BasicHouse->IsDefinitionValid()); }
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
	bool bFoundHunterQuestHandoff = false;
	bool bFoundBuilder = false;
	bool bFoundHerbalist = false;
	bool bFoundSoldier = false;
	bool bFoundSpeedHerb = false;
	bool bFoundConfiguredDeer = false;
	bool bFoundConfiguredBoar = false;
	bool bFoundConfiguredHare = false;
	bool bFoundInteractionTarget = false;
	bool bFoundRestPoint = false;
	bool bFoundTemporaryShelter = false;
	bool bFoundSettlementClaim = false;
	bool bFoundVillageMarker = false;
	bool bFoundForestMarker = false;
	bool bFoundAnimalAreaMarker = false;
	bool bFoundRoadMarker = false;
	bool bFoundRiverMarker = false;
	bool bFoundLandmarkMarker = false;
	bool bFoundPlantPatchMarker = false;
	int32 TrailMarkerCount = 0;
	bool bFoundCheapPrototypeSun = false;
	bool bFoundCheapPrototypeSkyLight = false;
	int32 PrototypeVisualCount = 0;
	bool bPrototypeVisualsAreCheap = true;
	if (World && World->PersistentLevel)
	{
		for (const AActor* Actor : World->PersistentLevel->Actors)
		{
			if (const AHSNPCActor* Hunter = Cast<AHSNPCActor>(Actor))
			{
				bFoundHunter |= Hunter->GetProfessionTag() == TAG_HS_NPC_Profession_Hunter;
				bFoundBuilder |= Hunter->GetProfessionTag() == TAG_HS_NPC_Profession_Builder;
				bFoundHerbalist |= Hunter->GetProfessionTag() == TAG_HS_NPC_Profession_Herbalist;
				bFoundSoldier |= Hunter->GetProfessionTag() == TAG_HS_NPC_Profession_Soldier;
				const UHSQuestHandoffComponent* Handoff = Hunter->GetQuestHandoffComponent();
				bFoundHunterQuestHandoff |= Hunter->GetProfessionTag() == TAG_HS_NPC_Profession_Hunter
					&& Handoff
					&& Handoff->GetQuestDefinition()
					&& Handoff->GetHandoffState() == EHSQuestHandoffState::Available;
			}
			if (const AHSAnimalActor* Deer = Cast<AHSAnimalActor>(Actor))
			{
				bFoundConfiguredDeer |= Deer->GetAnimalDefinition()
					&& Deer->GetSpeciesTag() == TAG_HS_Animal_Deer
					&& Deer->GetRuntimeState() == EHSAnimalRuntimeState::Idle;
				bFoundConfiguredBoar |= Deer->GetAnimalDefinition()
					&& Deer->GetSpeciesTag() == TAG_HS_Animal_Boar
					&& Deer->GetRuntimeState() == EHSAnimalRuntimeState::Idle;
				bFoundConfiguredHare |= Deer->GetAnimalDefinition()
					&& Deer->GetSpeciesTag() == TAG_HS_Animal_Hare
					&& Deer->GetRuntimeState() == EHSAnimalRuntimeState::Idle;
			}
			if (const AHSPlantActor* Plant = Cast<AHSPlantActor>(Actor))
			{
				bFoundSpeedHerb |= Plant->GetPlantDefinition()
					&& Plant->GetPlantDefinition()->PlantTag == TAG_HS_Plant_SpeedHerb
					&& !Plant->IsConsumed();
			}
			if (const AHSExplorationMarkerActor* ExplorationMarker = Cast<AHSExplorationMarkerActor>(Actor))
			{
				bFoundVillageMarker |= ExplorationMarker->GetMarkerTag() == TAG_HS_Location_Village && !ExplorationMarker->PrimaryActorTick.bCanEverTick;
				bFoundForestMarker |= ExplorationMarker->GetMarkerTag() == TAG_HS_Location_Forest && !ExplorationMarker->PrimaryActorTick.bCanEverTick;
				bFoundAnimalAreaMarker |= ExplorationMarker->GetMarkerTag() == TAG_HS_Location_AnimalArea && !ExplorationMarker->PrimaryActorTick.bCanEverTick;
				bFoundRoadMarker |= ExplorationMarker->GetMarkerTag() == TAG_HS_Location_Road && !ExplorationMarker->PrimaryActorTick.bCanEverTick;
				bFoundRiverMarker |= ExplorationMarker->GetMarkerTag() == TAG_HS_Location_River && !ExplorationMarker->PrimaryActorTick.bCanEverTick;
				bFoundLandmarkMarker |= ExplorationMarker->GetMarkerTag() == TAG_HS_Location_Landmark && !ExplorationMarker->PrimaryActorTick.bCanEverTick;
				bFoundPlantPatchMarker |= ExplorationMarker->GetMarkerTag() == TAG_HS_Location_PlantPatch && !ExplorationMarker->PrimaryActorTick.bCanEverTick;
				TrailMarkerCount += ExplorationMarker->GetMarkerTag() == TAG_HS_Location_Trail && !ExplorationMarker->PrimaryActorTick.bCanEverTick ? 1 : 0;
			}
			if (const AHSPrototypeVisualActor* PrototypeVisual = Cast<AHSPrototypeVisualActor>(Actor))
			{
				++PrototypeVisualCount;
				const UStaticMeshComponent* PrototypeMesh = PrototypeVisual->GetPrototypeMesh();
				bPrototypeVisualsAreCheap &= !PrototypeVisual->PrimaryActorTick.bCanEverTick
					&& PrototypeMesh
					&& PrototypeMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
			}
			bFoundInteractionTarget |= Actor && Actor->IsA<AHSInteractionTarget>();
			bFoundRestPoint |= Actor && Actor->IsA<AHSRestPointActor>();
			bFoundTemporaryShelter |= Actor && Actor->IsA<AHSTemporaryShelterActor>();
			bFoundSettlementClaim |= Actor && Actor->IsA<AHSSettlementClaimActor>();
			if (const ADirectionalLight* Sun = Cast<ADirectionalLight>(Actor))
			{
				const ULightComponent* SunComponent = Sun->GetLightComponent();
				bFoundCheapPrototypeSun |= SunComponent
					&& SunComponent->Mobility == EComponentMobility::Movable
					&& !SunComponent->CastShadows;
			}
			if (const ASkyLight* SkyLight = Cast<ASkyLight>(Actor))
			{
				const USkyLightComponent* SkyLightComponent = SkyLight->GetLightComponent();
				bFoundCheapPrototypeSkyLight |= SkyLightComponent
					&& SkyLightComponent->Mobility == EComponentMobility::Movable
					&& !SkyLightComponent->CastShadows;
			}
		}
	}

	TestTrue(TEXT("One Morning Hunt map should contain a native NPC actor."), bFoundHunter);
	TestTrue(TEXT("One Morning Hunt hunter should offer the first quest handoff."), bFoundHunterQuestHandoff);
	TestTrue(TEXT("One Morning Hunt map should contain a native builder NPC actor."), bFoundBuilder);
	TestTrue(TEXT("One Morning Hunt map should contain a native herbalist NPC actor."), bFoundHerbalist);
	TestTrue(TEXT("One Morning Hunt map should contain a native soldier NPC actor."), bFoundSoldier);
	TestTrue(TEXT("One Morning Hunt map should contain a configured native speed herb actor."), bFoundSpeedHerb);
	TestTrue(TEXT("One Morning Hunt map should contain a configured native deer actor."), bFoundConfiguredDeer);
	TestTrue(TEXT("One Morning Hunt map should contain a configured native boar actor."), bFoundConfiguredBoar);
	TestTrue(TEXT("One Morning Hunt map should contain a configured native hare actor."), bFoundConfiguredHare);
	TestTrue(TEXT("One Morning Hunt map should contain a native interaction target."), bFoundInteractionTarget);
	TestTrue(TEXT("One Morning Hunt map should contain a native end-of-day rest point."), bFoundRestPoint);
	TestTrue(TEXT("One Morning Hunt map should contain a native temporary shelter marker."), bFoundTemporaryShelter);
	TestTrue(TEXT("One Morning Hunt map should contain a native settlement claim marker."), bFoundSettlementClaim);
	TestTrue(TEXT("One Morning Hunt map should contain a village exploration marker."), bFoundVillageMarker);
	TestTrue(TEXT("One Morning Hunt map should contain a forest exploration marker."), bFoundForestMarker);
	TestTrue(TEXT("One Morning Hunt map should contain an animal-area exploration marker."), bFoundAnimalAreaMarker);
	TestTrue(TEXT("One Morning Hunt map should contain a road exploration marker."), bFoundRoadMarker);
	TestTrue(TEXT("One Morning Hunt map should contain a river exploration marker."), bFoundRiverMarker);
	TestTrue(TEXT("One Morning Hunt map should contain a landmark exploration marker."), bFoundLandmarkMarker);
	TestTrue(TEXT("One Morning Hunt map should contain a plant-patch exploration marker."), bFoundPlantPatchMarker);
	TestTrue(TEXT("One Morning Hunt map should contain a readable trail route."), TrailMarkerCount >= 3);
	TestTrue(TEXT("Expanded map should contain cheap native prototype visuals."), PrototypeVisualCount >= 30);
	TestTrue(TEXT("Prototype visuals should stay no-tick and no-collision."), bPrototypeVisualsAreCheap);
	TestTrue(TEXT("One Morning Hunt prototype sun should use the cheap no-shadow baseline."), bFoundCheapPrototypeSun);
	TestTrue(TEXT("One Morning Hunt prototype skylight should use the cheap no-shadow baseline."), bFoundCheapPrototypeSkyLight);

	return true;
}

#endif
