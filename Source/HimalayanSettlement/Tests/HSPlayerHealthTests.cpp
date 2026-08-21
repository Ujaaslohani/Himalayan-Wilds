#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/HSPlayerCharacter.h"
#include "Player/HSPlayerHealthComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHealthDefaultsTest,
	"HimalayanSettlement.Player.HealthDefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHealthDefaultsTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();
	const UHSPlayerHealthComponent* Health = Player ? Player->GetPlayerHealthComponent() : nullptr;

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own a native health component."), Health);
	TestFalse(TEXT("Health component should not tick."), Health ? Health->PrimaryComponentTick.bCanEverTick : true);
	TestEqual(TEXT("Player max health should default to 100."), Health ? Health->GetMaxHealth() : 0.0f, 100.0f);
	TestEqual(TEXT("Player current health should start full."), Health ? Health->GetCurrentHealth() : 0.0f, 100.0f);
	TestFalse(TEXT("Player should not start dead."), Health && Health->IsDead());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHealthDamageAndHealTest,
	"HimalayanSettlement.Player.HealthDamageAndHeal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHealthDamageAndHealTest::RunTest(const FString& Parameters)
{
	AHSPlayerCharacter* Player = NewObject<AHSPlayerCharacter>();
	UHSPlayerHealthComponent* Health = Player ? Player->GetPlayerHealthComponent() : nullptr;

	TestNotNull(TEXT("Player should be constructible."), Player);
	TestNotNull(TEXT("Player should own health component."), Health);
	if (!Health)
	{
		return false;
	}

	TestEqual(TEXT("Negative damage should be ignored."), Health->ApplyHealthDamage(-5.0f), 0.0f);
	TestEqual(TEXT("Positive damage should apply."), Health->ApplyHealthDamage(35.0f), 35.0f);
	TestEqual(TEXT("Damage should reduce current health."), Health->GetCurrentHealth(), 65.0f);
	TestEqual(TEXT("Healing should restore missing health."), Health->RestoreHealth(20.0f), 20.0f);
	TestEqual(TEXT("Healing should update current health."), Health->GetCurrentHealth(), 85.0f);
	TestEqual(TEXT("Over-healing should only restore to max."), Health->RestoreHealth(999.0f), 15.0f);
	TestEqual(TEXT("Health should clamp to max."), Health->GetCurrentHealth(), Health->GetMaxHealth());
	TestEqual(TEXT("Lethal damage should only apply remaining health."), Health->ApplyHealthDamage(999.0f), 100.0f);
	TestTrue(TEXT("Lethal damage should mark player dead."), Health->IsDead());
	TestEqual(TEXT("Dead player should not heal through normal restore."), Health->RestoreHealth(10.0f), 0.0f);
	Health->ResetHealth();
	TestFalse(TEXT("Reset should revive to full health."), Health->IsDead());
	TestEqual(TEXT("Reset should restore full health."), Health->GetCurrentHealth(), Health->GetMaxHealth());

	return true;
}

#endif
