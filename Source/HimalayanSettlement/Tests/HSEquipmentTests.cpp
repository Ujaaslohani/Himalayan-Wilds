#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "Player/HSPlayerCharacter.h"
#include "Resources/HSResourceCarryComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSEquipmentConsumesArrowsForEquippedBowTest,
	"HimalayanSettlement.Equipment.ConsumesArrowsForEquippedBow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSEquipmentConsumesArrowsForEquippedBowTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	UHSPlayerEquipmentComponent* Equipment = Player ? Player->GetPlayerEquipmentComponent() : nullptr;
	UHSResourceCarryComponent* Carry = Player ? Player->GetResourceCarryComponent() : nullptr;

	TestNotNull(TEXT("Player should own equipment component."), Equipment);
	TestNotNull(TEXT("Player should own resource carry component."), Carry);
	if (!Equipment || !Carry)
	{
		return false;
	}

	TestFalse(TEXT("Bow should start stored and unavailable."), Equipment->IsBowEquipped());
	TestFalse(TEXT("Bow should not equip when missing from inventory."), Equipment->SetBowEquipped(true));

	Carry->AddStack(FHSItemStack{ TAG_HS_Item_Bow_Basic, 1 });
	Carry->AddStack(FHSItemStack{ TAG_HS_Item_Arrow_Basic, 2 });

	TestTrue(TEXT("Bow should equip once carried."), Equipment->SetBowEquipped(true));
	TestTrue(TEXT("Equipped bow with arrows should be usable."), Equipment->CanUseBow());
	TestTrue(TEXT("Consuming one arrow should succeed."), Equipment->ConsumeArrow());
	TestEqual(TEXT("One arrow should remain."), Equipment->GetArrowCount(), 1);
	TestTrue(TEXT("Consuming last arrow should succeed."), Equipment->ConsumeArrow());
	TestEqual(TEXT("No arrows should remain."), Equipment->GetArrowCount(), 0);
	TestFalse(TEXT("Bow should not be usable without arrows."), Equipment->CanUseBow());
	TestFalse(TEXT("Consuming with no arrows should fail."), Equipment->ConsumeArrow());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasEquipmentComponentTest,
	"HimalayanSettlement.Equipment.PlayerHasNativeEquipmentComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasEquipmentComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own native equipment component."), Player ? Player->GetPlayerEquipmentComponent() : nullptr);
	TestNotNull(TEXT("Player should own a first-person bow grip mesh."), Player ? Player->GetBowGripMesh() : nullptr);
	TestNotNull(TEXT("Player should own a first-person upper bow limb mesh."), Player ? Player->GetBowUpperLimbMesh() : nullptr);
	TestNotNull(TEXT("Player should own a first-person lower bow limb mesh."), Player ? Player->GetBowLowerLimbMesh() : nullptr);
	TestNotNull(TEXT("Player should own a first-person bow string mesh."), Player ? Player->GetBowStringMesh() : nullptr);
	TestEqual(TEXT("Bow grip should not use collision."), Player && Player->GetBowGripMesh() ? Player->GetBowGripMesh()->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics, ECollisionEnabled::NoCollision);

	return true;
}

#endif
