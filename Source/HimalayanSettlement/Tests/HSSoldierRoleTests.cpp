#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "Player/HSPlayerCharacter.h"
#include "Resources/HSResourceCarryComponent.h"

namespace
{
UHSNPCDefinition* CreateSoldierDefinition(UObject* Outer)
{
	UHSNPCDefinition* SoldierDefinition = NewObject<UHSNPCDefinition>(Outer);
	SoldierDefinition->DisplayName = FText::FromString(TEXT("Soldier"));
	SoldierDefinition->ProfessionTag = TAG_HS_NPC_Profession_Soldier;
	return SoldierDefinition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSoldierProvidesBasicWeaponSuppliesTest,
	"HimalayanSettlement.Soldier.ProvidesBasicWeaponSupplies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSoldierProvidesBasicWeaponSuppliesTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSPlayerCharacter* Player = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	AHSNPCActor* Soldier = World->SpawnActor<AHSNPCActor>(FVector(220.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	Soldier->SetNPCDefinition(CreateSoldierDefinition(Soldier));

	UHSResourceCarryComponent* ResourceCarry = Player ? Player->GetResourceCarryComponent() : nullptr;
	TestNotNull(TEXT("Player should spawn."), Player);
	TestNotNull(TEXT("Soldier should spawn."), Soldier);
	TestNotNull(TEXT("Player should own resource carry component."), ResourceCarry);
	if (!Player || !Soldier || !ResourceCarry)
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("Soldier prompt should offer weapons before the player has a bow."), Soldier->GetInteractionPrompt_Implementation(Player).ToString().Contains(TEXT("weapons")));
	Soldier->Interact_Implementation(Player);
	TestEqual(TEXT("Soldier should grant one basic bow."), ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic), 1);
	TestEqual(TEXT("Soldier should grant a basic arrow bundle."), ResourceCarry->GetQuantityForTag(TAG_HS_Item_Arrow_Basic), 12);

	Soldier->Interact_Implementation(Player);
	TestEqual(TEXT("Soldier should not duplicate bow supplies while player already has a bow."), ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic), 1);
	TestEqual(TEXT("Soldier should not duplicate arrows while player already has a bow."), ResourceCarry->GetQuantityForTag(TAG_HS_Item_Arrow_Basic), 12);

	World->DestroyWorld(false);
	return true;
}

#endif
