#include "HSBootstrapOneMorningHuntCommandlet.h"

#include "Animals/HSAnimalDefinition.h"
#include "Animals/HSAnimalActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Building/HSBuildingDefinition.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Exploration/HSExplorationMarkerActor.h"
#include "Factories/WorldFactory.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayTagsManager.h"
#include "Herbalism/HSPlantActor.h"
#include "Herbalism/HSPlantDefinition.h"
#include "Interaction/HSInteractionTarget.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCDefinition.h"
#include "Quests/HSQuestDefinition.h"
#include "Resources/HSItemDefinition.h"
#include "Settlement/HSRestPointActor.h"
#include "Settlement/HSSettlementClaimActor.h"
#include "Settlement/HSTemporaryShelterActor.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr TCHAR OneMorningHuntMapPackage[] = TEXT("/Game/Maps/L_OneMorningHunt");

FGameplayTag Tag(const TCHAR* TagName)
{
	return UGameplayTagsManager::Get().RequestGameplayTag(FName(TagName), true);
}

FGameplayTagContainer Tags(std::initializer_list<const TCHAR*> TagNames)
{
	FGameplayTagContainer Container;
	for (const TCHAR* TagName : TagNames)
	{
		Container.AddTag(Tag(TagName));
	}
	return Container;
}

FHSItemStack Stack(const TCHAR* ItemTag, int32 Quantity)
{
	FHSItemStack Result;
	Result.ItemTag = Tag(ItemTag);
	Result.Quantity = Quantity;
	return Result;
}

bool SaveAssetPackage(UObject* Asset)
{
	UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
	if (!Package)
	{
		return false;
	}

	const FString PackageName = Package->GetName();
	const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
}

bool SaveMapPackage(UWorld* World)
{
	UPackage* Package = World ? World->GetOutermost() : nullptr;
	if (!Package)
	{
		return false;
	}

	const FString PackageName = Package->GetName();
	const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, World, *FileName, SaveArgs);
}

template <typename T>
T* GetOrCreateAsset(const TCHAR* LongPackageName, const TCHAR* AssetName)
{
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), LongPackageName, AssetName);
	if (T* Existing = FindObject<T>(nullptr, *ObjectPath))
	{
		return Existing;
	}

	FString ExistingPackageFileName;
	if (FPackageName::DoesPackageExist(LongPackageName, &ExistingPackageFileName))
	{
		if (T* Existing = LoadObject<T>(nullptr, *ObjectPath))
		{
			return Existing;
		}
	}

	UPackage* Package = CreatePackage(LongPackageName);
	T* Asset = NewObject<T>(Package, FName(AssetName), RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();
	return Asset;
}

template <typename T>
T* ConfigureAsset(const TCHAR* LongPackageName, const TCHAR* AssetName)
{
	T* Asset = GetOrCreateAsset<T>(LongPackageName, AssetName);
	check(Asset);
	return Asset;
}

void CreateDefinitions()
{
	UHSItemDefinition* Bow = ConfigureAsset<UHSItemDefinition>(TEXT("/Game/Data/Items/DA_Item_BasicBow"), TEXT("DA_Item_BasicBow"));
	Bow->DisplayName = FText::FromString(TEXT("Basic Bow"));
	Bow->ItemTag = Tag(TEXT("Item.Bow.Basic"));
	Bow->Weight = 1.2f;
	SaveAssetPackage(Bow);

	UHSItemDefinition* Arrow = ConfigureAsset<UHSItemDefinition>(TEXT("/Game/Data/Items/DA_Item_BasicArrow"), TEXT("DA_Item_BasicArrow"));
	Arrow->DisplayName = FText::FromString(TEXT("Basic Arrow"));
	Arrow->ItemTag = Tag(TEXT("Item.Arrow.Basic"));
	Arrow->Weight = 0.05f;
	SaveAssetPackage(Arrow);

	UHSItemDefinition* Meat = ConfigureAsset<UHSItemDefinition>(TEXT("/Game/Data/Items/DA_Item_DeerMeat"), TEXT("DA_Item_DeerMeat"));
	Meat->DisplayName = FText::FromString(TEXT("Deer Meat"));
	Meat->ItemTag = Tag(TEXT("Resource.Meat.Deer"));
	Meat->Weight = 0.6f;
	SaveAssetPackage(Meat);

	UHSItemDefinition* Hide = ConfigureAsset<UHSItemDefinition>(TEXT("/Game/Data/Items/DA_Item_DeerHide"), TEXT("DA_Item_DeerHide"));
	Hide->DisplayName = FText::FromString(TEXT("Deer Hide"));
	Hide->ItemTag = Tag(TEXT("Resource.Hide.Deer"));
	Hide->Weight = 1.0f;
	SaveAssetPackage(Hide);

	UHSAnimalDefinition* Deer = ConfigureAsset<UHSAnimalDefinition>(TEXT("/Game/Data/Animals/DA_Animal_Deer"), TEXT("DA_Animal_Deer"));
	Deer->DisplayName = FText::FromString(TEXT("Deer"));
	Deer->SpeciesTag = Tag(TEXT("Animal.Deer"));
	Deer->MaxHealth = 70.0f;
	Deer->WalkSpeed = 180.0f;
	Deer->FleeSpeed = 560.0f;
	Deer->SightRadius = 1800.0f;
	Deer->HarvestDrops = { Stack(TEXT("Resource.Meat.Deer"), 2), Stack(TEXT("Resource.Hide.Deer"), 1) };
	SaveAssetPackage(Deer);

	UHSNPCDefinition* Hunter = ConfigureAsset<UHSNPCDefinition>(TEXT("/Game/Data/NPCs/DA_NPC_Hunter"), TEXT("DA_NPC_Hunter"));
	Hunter->DisplayName = FText::FromString(TEXT("Hunter"));
	Hunter->ProfessionTag = Tag(TEXT("NPC.Profession.Hunter"));
	Hunter->KnowledgeTags = Tags({ TEXT("Knowledge.Hunting.Basic") });
	Hunter->UnlockTags = Tags({ TEXT("Building.BasicDryingRack") });
	SaveAssetPackage(Hunter);

	UHSNPCDefinition* Builder = ConfigureAsset<UHSNPCDefinition>(TEXT("/Game/Data/NPCs/DA_NPC_Builder"), TEXT("DA_NPC_Builder"));
	Builder->DisplayName = FText::FromString(TEXT("Builder"));
	Builder->ProfessionTag = Tag(TEXT("NPC.Profession.Builder"));
	Builder->KnowledgeTags = Tags({ TEXT("Knowledge.Settlement.BasicCrafting") });
	Builder->UnlockTags.Reset();
	SaveAssetPackage(Builder);

	UHSNPCDefinition* Herbalist = ConfigureAsset<UHSNPCDefinition>(TEXT("/Game/Data/NPCs/DA_NPC_Herbalist"), TEXT("DA_NPC_Herbalist"));
	Herbalist->DisplayName = FText::FromString(TEXT("Herbalist"));
	Herbalist->ProfessionTag = Tag(TEXT("NPC.Profession.Herbalist"));
	Herbalist->KnowledgeTags = Tags({ TEXT("Plant.SpeedHerb") });
	Herbalist->UnlockTags.Reset();
	SaveAssetPackage(Herbalist);

	UHSNPCDefinition* Soldier = ConfigureAsset<UHSNPCDefinition>(TEXT("/Game/Data/NPCs/DA_NPC_Soldier"), TEXT("DA_NPC_Soldier"));
	Soldier->DisplayName = FText::FromString(TEXT("Soldier"));
	Soldier->ProfessionTag = Tag(TEXT("NPC.Profession.Soldier"));
	Soldier->KnowledgeTags.Reset();
	Soldier->UnlockTags.Reset();
	SaveAssetPackage(Soldier);

	UHSPlantDefinition* SpeedHerb = ConfigureAsset<UHSPlantDefinition>(TEXT("/Game/Data/Plants/DA_Plant_SpeedHerb"), TEXT("DA_Plant_SpeedHerb"));
	SpeedHerb->DisplayName = FText::FromString(TEXT("Speed Herb"));
	SpeedHerb->PlantTag = Tag(TEXT("Plant.SpeedHerb"));
	SpeedHerb->EffectTag = Tag(TEXT("Effect.Plant.Speed"));
	SpeedHerb->MovementSpeedMultiplier = 1.25f;
	SpeedHerb->DurationDays = 0.5f;
	SaveAssetPackage(SpeedHerb);

	UHSBuildingDefinition* Rack = ConfigureAsset<UHSBuildingDefinition>(TEXT("/Game/Data/Buildings/DA_Building_BasicDryingRack"), TEXT("DA_Building_BasicDryingRack"));
	Rack->DisplayName = FText::FromString(TEXT("Basic Drying Rack"));
	Rack->BuildingTag = Tag(TEXT("Building.BasicDryingRack"));
	Rack->RequiredKnowledgeTags = Tags({ TEXT("Knowledge.Settlement.BasicCrafting") });
	Rack->ResourceCosts = { Stack(TEXT("Resource.Hide.Deer"), 1) };
	SaveAssetPackage(Rack);

	UHSBuildingDefinition* House = ConfigureAsset<UHSBuildingDefinition>(TEXT("/Game/Data/Buildings/DA_Building_BasicHouse"), TEXT("DA_Building_BasicHouse"));
	House->DisplayName = FText::FromString(TEXT("Basic House"));
	House->BuildingTag = Tag(TEXT("Building.BasicHouse"));
	House->RequiredKnowledgeTags = Tags({ TEXT("Knowledge.Settlement.BasicCrafting") });
	House->ResourceCosts.Reset();
	SaveAssetPackage(House);

	UHSQuestDefinition* Quest = ConfigureAsset<UHSQuestDefinition>(TEXT("/Game/Data/Quests/DA_Quest_OneMorningHunt"), TEXT("DA_Quest_OneMorningHunt"));
	Quest->DisplayName = FText::FromString(TEXT("One Morning Hunt"));
	Quest->QuestTag = Tag(TEXT("Quest.OneMorningHunt"));
	Quest->TurnInRequirement.RequiredItems = { Stack(TEXT("Resource.Meat.Deer"), 2), Stack(TEXT("Resource.Hide.Deer"), 1) };
	Quest->RewardKnowledgeTags = Tags({ TEXT("Knowledge.Settlement.BasicCrafting") });
	Quest->RewardUnlockTags = Tags({ TEXT("Building.BasicDryingRack") });
	SaveAssetPackage(Quest);
}

void ClearMapActors(UWorld* World)
{
	TArray<AActor*> ActorsToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->IsA<AWorldSettings>())
		{
			ActorsToDestroy.Add(*It);
		}
	}

	for (AActor* Actor : ActorsToDestroy)
	{
		World->DestroyActor(Actor);
	}
}

template <typename T>
T* SpawnNamedActor(UWorld* World, const TCHAR* Name, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
{
	FActorSpawnParameters SpawnParams;
	T* Actor = World->SpawnActor<T>(Location, Rotation, SpawnParams);
	check(Actor);
	Actor->SetActorLabel(Name);
	return Actor;
}

AHSExplorationMarkerActor* SpawnExplorationMarker(UWorld* World, const TCHAR* Name, const FVector& Location, const FText& DisplayName, const TCHAR* MarkerTag)
{
	AHSExplorationMarkerActor* Marker = SpawnNamedActor<AHSExplorationMarkerActor>(World, Name, Location);
	Marker->ConfigureMarker(DisplayName, Tag(MarkerTag));
	return Marker;
}

UWorld* CreateOrLoadMap()
{
	if (UWorld* ExistingWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/Maps/L_OneMorningHunt.L_OneMorningHunt")))
	{
		return ExistingWorld;
	}

	UPackage* Package = CreatePackage(OneMorningHuntMapPackage);
	UWorldFactory* WorldFactory = NewObject<UWorldFactory>();
	UObject* CreatedAsset = WorldFactory->FactoryCreateNew(UWorld::StaticClass(), Package, FName(TEXT("L_OneMorningHunt")), RF_Public | RF_Standalone, nullptr, GWarn);
	UWorld* World = CastChecked<UWorld>(CreatedAsset);
	FAssetRegistryModule::AssetCreated(World);
	Package->MarkPackageDirty();
	return World;
}

void CreateMap()
{
	UWorld* World = CreateOrLoadMap();
	check(World);

	ClearMapActors(World);

	AStaticMeshActor* Ground = SpawnNamedActor<AStaticMeshActor>(World, TEXT("Prototype_Ground"), FVector::ZeroVector);
	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		Ground->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
	}
	Ground->SetActorScale3D(FVector(20.0f, 20.0f, 1.0f));

	SpawnNamedActor<APlayerStart>(World, TEXT("PlayerStart_OneMorningHunt"), FVector(-300.0f, 0.0f, 90.0f));
	ADirectionalLight* Sun = SpawnNamedActor<ADirectionalLight>(World, TEXT("Sun_Morning"), FVector(0.0f, 0.0f, 400.0f), FRotator(-35.0f, -40.0f, 0.0f));
	if (ULightComponent* SunComponent = Sun->GetLightComponent())
	{
		SunComponent->SetMobility(EComponentMobility::Movable);
		SunComponent->SetCastShadows(false);
	}

	ASkyLight* SkyLight = SpawnNamedActor<ASkyLight>(World, TEXT("SkyLight_Morning"), FVector(0.0f, 0.0f, 250.0f));
	if (USkyLightComponent* SkyLightComponent = SkyLight->GetLightComponent())
	{
		SkyLightComponent->SetMobility(EComponentMobility::Movable);
		SkyLightComponent->SetCastShadows(false);
	}

	AHSNPCActor* Hunter = SpawnNamedActor<AHSNPCActor>(World, TEXT("NPC_Hunter"), FVector(200.0f, 0.0f, 90.0f), FRotator(0.0f, 180.0f, 0.0f));
	if (UHSNPCDefinition* HunterDefinition = LoadObject<UHSNPCDefinition>(nullptr, TEXT("/Game/Data/NPCs/DA_NPC_Hunter.DA_NPC_Hunter")))
	{
		Hunter->SetNPCDefinition(HunterDefinition);
	}
	if (UHSQuestDefinition* QuestDefinition = LoadObject<UHSQuestDefinition>(nullptr, TEXT("/Game/Data/Quests/DA_Quest_OneMorningHunt.DA_Quest_OneMorningHunt")))
	{
		Hunter->SetOfferedQuest(QuestDefinition);
	}

	AHSNPCActor* Builder = SpawnNamedActor<AHSNPCActor>(World, TEXT("NPC_Builder"), FVector(-360.0f, -260.0f, 90.0f), FRotator(0.0f, 80.0f, 0.0f));
	Builder->SetNPCDefinition(GetOrCreateAsset<UHSNPCDefinition>(TEXT("/Game/Data/NPCs/DA_NPC_Builder"), TEXT("DA_NPC_Builder")));

	AHSNPCActor* Herbalist = SpawnNamedActor<AHSNPCActor>(World, TEXT("NPC_Herbalist"), FVector(-460.0f, 120.0f, 90.0f), FRotator(0.0f, 65.0f, 0.0f));
	Herbalist->SetNPCDefinition(GetOrCreateAsset<UHSNPCDefinition>(TEXT("/Game/Data/NPCs/DA_NPC_Herbalist"), TEXT("DA_NPC_Herbalist")));

	AHSNPCActor* Soldier = SpawnNamedActor<AHSNPCActor>(World, TEXT("NPC_Soldier"), FVector(-520.0f, -80.0f, 90.0f), FRotator(0.0f, 45.0f, 0.0f));
	Soldier->SetNPCDefinition(GetOrCreateAsset<UHSNPCDefinition>(TEXT("/Game/Data/NPCs/DA_NPC_Soldier"), TEXT("DA_NPC_Soldier")));

	AHSAnimalActor* Deer = SpawnNamedActor<AHSAnimalActor>(World, TEXT("Animal_Deer_Prototype"), FVector(650.0f, -180.0f, 70.0f), FRotator(0.0f, -35.0f, 0.0f));
	if (UHSAnimalDefinition* DeerDefinition = LoadObject<UHSAnimalDefinition>(nullptr, TEXT("/Game/Data/Animals/DA_Animal_Deer.DA_Animal_Deer")))
	{
		Deer->SetAnimalDefinition(DeerDefinition);
	}

	SpawnNamedActor<AHSInteractionTarget>(World, TEXT("Interaction_Target_Practice"), FVector(200.0f, 160.0f, 90.0f), FRotator(0.0f, 180.0f, 0.0f));
	SpawnNamedActor<AHSRestPointActor>(World, TEXT("Camp_RestPoint_EndMorning"), FVector(-80.0f, 220.0f, 45.0f), FRotator::ZeroRotator);
	SpawnNamedActor<AHSTemporaryShelterActor>(World, TEXT("Temporary_Shelter_Sleep"), FVector(-230.0f, 220.0f, 55.0f), FRotator(0.0f, 25.0f, 0.0f));
	SpawnNamedActor<AHSSettlementClaimActor>(World, TEXT("Settlement_Claim_Marker"), FVector(-80.0f, -260.0f, 55.0f), FRotator::ZeroRotator);

	AHSPlantActor* SpeedHerb = SpawnNamedActor<AHSPlantActor>(World, TEXT("Plant_SpeedHerb"), FVector(360.0f, 230.0f, 45.0f), FRotator::ZeroRotator);
	SpeedHerb->SetPlantDefinition(GetOrCreateAsset<UHSPlantDefinition>(TEXT("/Game/Data/Plants/DA_Plant_SpeedHerb"), TEXT("DA_Plant_SpeedHerb")));

	SpawnExplorationMarker(World, TEXT("Explore_Village_Core"), FVector(-640.0f, -230.0f, 100.0f), FText::FromString(TEXT("Village Core")), TEXT("Location.Village"));
	SpawnExplorationMarker(World, TEXT("Explore_Forest_Edge"), FVector(900.0f, -360.0f, 100.0f), FText::FromString(TEXT("Forest Edge")), TEXT("Location.Forest"));
	SpawnExplorationMarker(World, TEXT("Explore_Animal_Ridge"), FVector(720.0f, -420.0f, 100.0f), FText::FromString(TEXT("Animal Ridge")), TEXT("Location.AnimalArea"));
	SpawnExplorationMarker(World, TEXT("Trail_Settlement_01"), FVector(-260.0f, -245.0f, 95.0f), FText::FromString(TEXT("Village Trail")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Trail_Forest_02"), FVector(180.0f, -300.0f, 95.0f), FText::FromString(TEXT("Forest Trail")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Trail_Forest_03"), FVector(560.0f, -340.0f, 95.0f), FText::FromString(TEXT("Ridge Trail")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Trail_Ridge_04"), FVector(780.0f, -520.0f, 95.0f), FText::FromString(TEXT("Ridge Return")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Explore_Herb_Patch"), FVector(360.0f, 305.0f, 100.0f), FText::FromString(TEXT("Herb Patch")), TEXT("Location.Forest"));
	SpawnExplorationMarker(World, TEXT("Explore_Builder_Yard"), FVector(-430.0f, -330.0f, 100.0f), FText::FromString(TEXT("Builder Yard")), TEXT("Location.Village"));

	World->MarkPackageDirty();
	SaveMapPackage(World);
}
}

UHSBootstrapOneMorningHuntCommandlet::UHSBootstrapOneMorningHuntCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UHSBootstrapOneMorningHuntCommandlet::Main(const FString& Params)
{
	UGameplayTagsManager::Get().AddTagIniSearchPath(FPaths::ProjectConfigDir() / TEXT("Tags"));
	UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();
	CreateDefinitions();
	CreateMap();
	UE_LOG(LogTemp, Display, TEXT("One Morning Hunt bootstrap complete."));
	return 0;
}
