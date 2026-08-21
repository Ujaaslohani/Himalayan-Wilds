#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/HSGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Herbalism/HSPlantActor.h"
#include "Herbalism/HSPlantDefinition.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "Player/HSPlayerCharacter.h"
#include "World/HSGameState.h"
#include "World/HSWorldTimeComponent.h"

namespace
{
UHSNPCDefinition* CreateHerbalistDefinition(UObject* Outer)
{
	UHSNPCDefinition* HerbalistDefinition = NewObject<UHSNPCDefinition>(Outer);
	HerbalistDefinition->DisplayName = FText::FromString(TEXT("Herbalist"));
	HerbalistDefinition->ProfessionTag = TAG_HS_NPC_Profession_Herbalist;
	return HerbalistDefinition;
}

UHSPlantDefinition* CreateSpeedHerbDefinition(UObject* Outer)
{
	UHSPlantDefinition* PlantDefinition = NewObject<UHSPlantDefinition>(Outer);
	PlantDefinition->DisplayName = FText::FromString(TEXT("Speed Herb"));
	PlantDefinition->PlantTag = TAG_HS_Plant_SpeedHerb;
	PlantDefinition->EffectTag = TAG_HS_Effect_Plant_Speed;
	PlantDefinition->MovementSpeedMultiplier = 1.25f;
	PlantDefinition->DurationDays = 0.5f;
	return PlantDefinition;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlantEffectDefaultsTest,
	"HimalayanSettlement.Herbalism.DefaultsAreConservative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlantEffectDefaultsTest::RunTest(const FString& Parameters)
{
	const UHSPlantEffectComponent* PlantEffect = NewObject<UHSPlantEffectComponent>();

	TestNotNull(TEXT("Plant effect component should be constructible."), PlantEffect);
	TestFalse(TEXT("Plant effect component should not tick by default."), PlantEffect->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Plant effect should start inactive."), PlantEffect->HasActiveEffect());
	TestEqual(TEXT("Plant speed multiplier should start neutral."), PlantEffect->GetMovementSpeedMultiplier(), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSHerbalistTeachesPlantAndPlantAppliesEffectTest,
	"HimalayanSettlement.Herbalism.HerbalistTeachesKnownPlantEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSHerbalistTeachesPlantAndPlantAppliesEffectTest::RunTest(const FString& Parameters)
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
	AHSNPCActor* Herbalist = World->SpawnActor<AHSNPCActor>(FVector(180.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);
	AHSPlantActor* Plant = World->SpawnActor<AHSPlantActor>(FVector(260.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParameters);

	UHSPlantDefinition* PlantDefinition = CreateSpeedHerbDefinition(Plant);
	Herbalist->SetNPCDefinition(CreateHerbalistDefinition(Herbalist));
	Plant->SetPlantDefinition(PlantDefinition);

	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	UHSPlantEffectComponent* PlantEffect = Player ? Player->GetPlantEffectComponent() : nullptr;
	UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;

	TestNotNull(TEXT("World time should exist."), WorldTime);
	TestNotNull(TEXT("Player should own plant effect component."), PlantEffect);
	TestNotNull(TEXT("Player should own movement component."), Movement);
	if (!WorldTime || !Player || !Herbalist || !Plant || !PlantEffect || !Movement)
	{
		World->DestroyWorld(false);
		return false;
	}

	PlantEffect->BindToWorldTime();
	TestFalse(TEXT("Speed herb should start unknown."), PlantEffect->HasPlantKnowledge(TAG_HS_Plant_SpeedHerb));
	TestTrue(TEXT("Herbalist prompt should offer plant knowledge."), Herbalist->GetInteractionPrompt_Implementation(Player).ToString().Contains(TEXT("plant")));
	Herbalist->Interact_Implementation(Player);
	TestTrue(TEXT("Herbalist should teach speed herb."), PlantEffect->HasPlantKnowledge(TAG_HS_Plant_SpeedHerb));

	const float BaseStandingSpeed = Player->GetStandingWalkSpeed();
	TestTrue(TEXT("Known plant should apply its effect."), Plant->CanInteract_Implementation(Player));
	Plant->Interact_Implementation(Player);
	TestTrue(TEXT("Plant should be consumed after successful use."), Plant->IsConsumed());
	TestTrue(TEXT("Plant effect should be active."), PlantEffect->HasActiveEffect());
	Player->SetPlayerStance(EHSPlayerStance::Crouching);
	Player->SetPlayerStance(EHSPlayerStance::Standing);
	TestEqual(TEXT("Plant should increase standing movement speed."), Movement->MaxWalkSpeed, BaseStandingSpeed * PlantDefinition->MovementSpeedMultiplier);

	WorldTime->AdvanceHalfDay();
	TestFalse(TEXT("Plant effect should expire after its duration."), PlantEffect->HasActiveEffect());
	Player->SetPlayerStance(EHSPlayerStance::Crouching);
	Player->SetPlayerStance(EHSPlayerStance::Standing);
	TestEqual(TEXT("Expired plant effect should restore neutral standing speed."), Movement->MaxWalkSpeed, BaseStandingSpeed);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHSPlayerHasPlantEffectComponentTest,
	"HimalayanSettlement.Herbalism.PlayerHasNativePlantEffectComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSPlayerHasPlantEffectComponentTest::RunTest(const FString& Parameters)
{
	const AHSPlayerCharacter* Player = GetDefault<AHSPlayerCharacter>();

	TestNotNull(TEXT("Player class should have a valid default object."), Player);
	TestNotNull(TEXT("Player should own native plant effect component."), Player ? Player->GetPlantEffectComponent() : nullptr);

	return true;
}

#endif
