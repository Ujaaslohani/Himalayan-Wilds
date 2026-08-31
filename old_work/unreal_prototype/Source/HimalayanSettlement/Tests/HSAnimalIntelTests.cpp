#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "Engine/World.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Hunting/HSAnimalLeadMarkerActor.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "Player/HSPlayerCharacter.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

namespace
{
UHSNPCDefinition* CreateHunterDefinition(UObject* Outer)
{
	UHSNPCDefinition* HunterDefinition = NewObject<UHSNPCDefinition>(Outer);
	HunterDefinition->DisplayName = FText::FromString(TEXT("Hunter"));
	HunterDefinition->ProfessionTag = TAG_HS_NPC_Profession_Hunter;
	return HunterDefinition;
}

bool WorldHasActiveAnimalLeadMarker(const UWorld* World)
{
	if (!World || !World->PersistentLevel)
	{
		return false;
	}

	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		const AHSAnimalLeadMarkerActor* Marker = Cast<AHSAnimalLeadMarkerActor>(Actor);
		if (Marker && !Marker->IsExpired() && Marker->GetAnimalTag() == TAG_HS_Animal_Deer)
		{
			return true;
		}
	}

	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSAnimalIntelDefaultsTest,
	"HimalayanSettlement.HunterIntel.DefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSAnimalIntelDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSAnimalIntelComponent* AnimalIntel = NewObject<UHSAnimalIntelComponent>();

	TestNotNull(TEXT("Animal intel component should be constructible."), AnimalIntel);
	TestFalse(TEXT("Animal intel component should not tick by default."), AnimalIntel->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Animal intel should start without an active lead."), AnimalIntel->HasActiveLead());
	TestEqual(TEXT("Animal intel should start without a report day."), AnimalIntel->GetLastReportDay(), INDEX_NONE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSAnimalIntelDailyReportAndExpiryTest,
	"HimalayanSettlement.HunterIntel.DailyReportSpawnsAndExpiresLead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSAnimalIntelDailyReportAndExpiryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSGameState* GameState = World->SpawnActor<AHSGameState>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	World->SetGameState(GameState);
	AHSPlayerCharacter* Player = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);

	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	UHSAnimalIntelComponent* AnimalIntel = Player ? Player->GetAnimalIntelComponent() : nullptr;

	TestNotNull(TEXT("World time should exist."), WorldTime);
	TestNotNull(TEXT("Player should own animal intel component."), AnimalIntel);
	if (!WorldTime || !AnimalIntel)
	{
		World->DestroyWorld(false);
		return false;
	}

	AnimalIntel->BindToWorldTime();
	TestTrue(TEXT("First daily lead should be available."), AnimalIntel->CanRequestDailyAnimalLead());
	TestTrue(TEXT("First daily lead should report."), AnimalIntel->RequestDailyAnimalLead());
	TestTrue(TEXT("Reported lead should be active."), AnimalIntel->HasActiveLead());
	TestTrue(TEXT("Reported lead should be deer."), AnimalIntel->GetActiveLead().AnimalTag == TAG_HS_Animal_Deer);
	TestEqual(TEXT("Reported lead day should be day zero."), AnimalIntel->GetActiveLead().ReportDay, 0);
	TestFalse(TEXT("Second report on same day should be blocked."), AnimalIntel->RequestDailyAnimalLead());
	TestTrue(TEXT("Reported lead should spawn an active marker."), WorldHasActiveAnimalLeadMarker(World));

	WorldTime->AdvanceHalfDay();
	TestTrue(TEXT("Half-day advance should not expire same-day lead."), AnimalIntel->HasActiveLead());
	WorldTime->AdvanceHalfDay();
	TestFalse(TEXT("Next day should expire the old lead."), AnimalIntel->HasActiveLead());
	TestTrue(TEXT("Next day should allow a fresh report."), AnimalIntel->CanRequestDailyAnimalLead());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHunterNPCReportsDailyAnimalLeadTest,
	"HimalayanSettlement.HunterIntel.HunterNPCReportsDailyAnimalLead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHunterNPCReportsDailyAnimalLeadTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSGameState* GameState = World->SpawnActor<AHSGameState>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	World->SetGameState(GameState);
	AHSPlayerCharacter* Player = World->SpawnActor<AHSPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	AHSNPCActor* Hunter = World->SpawnActor<AHSNPCActor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	Hunter->SetNPCDefinition(CreateHunterDefinition(Hunter));

	UHSAnimalIntelComponent* AnimalIntel = Player ? Player->GetAnimalIntelComponent() : nullptr;
	TestNotNull(TEXT("Player should own animal intel component."), AnimalIntel);
	if (!Player || !Hunter || !AnimalIntel)
	{
		World->DestroyWorld(false);
		return false;
	}

	AnimalIntel->BindToWorldTime();
	TestTrue(TEXT("Hunter prompt should offer animal signs."), Hunter->GetInteractionPrompt_Implementation(Player).ToString().Contains(TEXT("animal")));
	Hunter->Interact_Implementation(Player);
	TestTrue(TEXT("Hunter interaction should report active animal lead."), AnimalIntel->HasActiveLead());
	TestTrue(TEXT("Active animal lead summary should mention the reported animal."), AnimalIntel->DescribeActiveLead().ToString().Contains(TEXT("Animal.Deer")));
	TestFalse(TEXT("Hunter should not report twice in the same day."), AnimalIntel->CanRequestDailyAnimalLead());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasAnimalIntelComponentTest,
	"HimalayanSettlement.HunterIntel.PlayerHasNativeAnimalIntelComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasAnimalIntelComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own native animal intel component."), Player ? Player->GetAnimalIntelComponent() : nullptr);

	return true;
}

#endif
