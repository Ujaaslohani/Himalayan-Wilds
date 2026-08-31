#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Player/HSPlayerCharacter.h"
#include "Settlement/HSSettlementUnlockComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementUnlockDefaultsTest,
	"HimalayanSettlement.Settlement.UnlockDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementUnlockDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSSettlementUnlockComponent* Unlocks = NewObject<UHSSettlementUnlockComponent>();

	TestNotNull(TEXT("Settlement unlock component should be constructible."), Unlocks);
	TestFalse(TEXT("Settlement unlock component should not tick by default."), Unlocks->PrimaryComponentTick.bCanEverTick);
	TestEqual(TEXT("Settlement unlock component should start empty."), Unlocks->GetGrantedUnlocks().Num(), 0);
	TestFalse(TEXT("Invalid tags should not be reported as unlocked."), Unlocks->HasUnlock(FGameplayTag()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSSettlementUnlockGrantsOnceTest,
	"HimalayanSettlement.Settlement.UnlockGrantsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSSettlementUnlockGrantsOnceTest::RunTest(const FString& Parameters)
{
	UHSSettlementUnlockComponent* Unlocks = NewObject<UHSSettlementUnlockComponent>();

	TestTrue(TEXT("Valid unlock should be granted."), Unlocks->GrantUnlock(TAG_HS_Building_BasicDryingRack));
	TestTrue(TEXT("Granted unlock should be queryable."), Unlocks->HasUnlock(TAG_HS_Building_BasicDryingRack));
	TestFalse(TEXT("Duplicate unlock should not be granted again."), Unlocks->GrantUnlock(TAG_HS_Building_BasicDryingRack));
	TestEqual(TEXT("Duplicate unlock should not add another tag."), Unlocks->GetGrantedUnlocks().Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasSettlementUnlockComponentTest,
	"HimalayanSettlement.Settlement.PlayerHasNativeUnlockComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasSettlementUnlockComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own the native settlement unlock component."), Player ? Player->GetSettlementUnlockComponent() : nullptr);

	return true;
}

#endif
