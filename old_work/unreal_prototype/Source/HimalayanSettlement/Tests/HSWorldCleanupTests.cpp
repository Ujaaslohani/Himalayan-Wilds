#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "World/HSWorldCleanupSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSWorldCleanupRegistersAndCollectsTest,
	"HimalayanSettlement.World.CleanupRegistersAndCollectsTemporaryActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSWorldCleanupRegistersAndCollectsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient test world should be created."), World);
	if (!World)
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	UHSWorldCleanupSubsystem* CleanupSubsystem = World->GetSubsystem<UHSWorldCleanupSubsystem>();
	TestNotNull(TEXT("World cleanup subsystem should exist."), CleanupSubsystem);
	if (!CleanupSubsystem)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* TemporaryActor = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);

	TestTrue(TEXT("Temporary actor should register with cleanup."), CleanupSubsystem->RegisterTemporaryActor(TemporaryActor, 0.01f, TEXT("TestTemporary")));
	TestTrue(TEXT("Registered actor should be tracked."), CleanupSubsystem->IsActorRegistered(TemporaryActor));
	TestEqual(TEXT("One actor should be tracked."), CleanupSubsystem->GetTrackedActorCount(), 1);

	World->Tick(LEVELTICK_All, 0.02f);
	TestEqual(TEXT("Expired actor should be collected."), CleanupSubsystem->CollectExpiredActors(), 1);
	TestTrue(TEXT("Actor should begin destruction after cleanup."), TemporaryActor && TemporaryActor->IsActorBeingDestroyed());
	TestEqual(TEXT("Cleanup list should be empty after collection."), CleanupSubsystem->GetTrackedActorCount(), 0);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
