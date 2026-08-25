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
#include "Exploration/HSPrototypeVisualActor.h"
#include "Factories/WorldFactory.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
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

	UHSItemDefinition* ReinforcedBow = ConfigureAsset<UHSItemDefinition>(TEXT("/Game/Data/Items/DA_Item_ReinforcedBow"), TEXT("DA_Item_ReinforcedBow"));
	ReinforcedBow->DisplayName = FText::FromString(TEXT("Reinforced Bow"));
	ReinforcedBow->ItemTag = Tag(TEXT("Item.Bow.Reinforced"));
	ReinforcedBow->Weight = 1.35f;
	SaveAssetPackage(ReinforcedBow);

	UHSItemDefinition* Spear = ConfigureAsset<UHSItemDefinition>(TEXT("/Game/Data/Items/DA_Item_BasicSpear"), TEXT("DA_Item_BasicSpear"));
	Spear->DisplayName = FText::FromString(TEXT("Basic Spear"));
	Spear->ItemTag = Tag(TEXT("Item.Spear.Basic"));
	Spear->Weight = 1.8f;
	SaveAssetPackage(Spear);

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

	UHSAnimalDefinition* Boar = ConfigureAsset<UHSAnimalDefinition>(TEXT("/Game/Data/Animals/DA_Animal_Boar"), TEXT("DA_Animal_Boar"));
	Boar->DisplayName = FText::FromString(TEXT("Boar"));
	Boar->SpeciesTag = Tag(TEXT("Animal.Boar"));
	Boar->MaxHealth = 115.0f;
	Boar->WalkSpeed = 145.0f;
	Boar->FleeSpeed = 500.0f;
	Boar->SightRadius = 1450.0f;
	Boar->HarvestDrops = { Stack(TEXT("Resource.Meat.Deer"), 3), Stack(TEXT("Resource.Hide.Deer"), 1) };
	SaveAssetPackage(Boar);

	UHSAnimalDefinition* Hare = ConfigureAsset<UHSAnimalDefinition>(TEXT("/Game/Data/Animals/DA_Animal_Hare"), TEXT("DA_Animal_Hare"));
	Hare->DisplayName = FText::FromString(TEXT("Hare"));
	Hare->SpeciesTag = Tag(TEXT("Animal.Hare"));
	Hare->MaxHealth = 25.0f;
	Hare->WalkSpeed = 240.0f;
	Hare->FleeSpeed = 720.0f;
	Hare->SightRadius = 900.0f;
	Hare->HarvestDrops = { Stack(TEXT("Resource.Meat.Deer"), 1) };
	SaveAssetPackage(Hare);

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
	Herbalist->KnowledgeTags = Tags({ TEXT("Plant.SpeedHerb"), TEXT("Plant.ScentHerb"), TEXT("Plant.SenseHerb") });
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

	UHSPlantDefinition* ScentHerb = ConfigureAsset<UHSPlantDefinition>(TEXT("/Game/Data/Plants/DA_Plant_ScentHerb"), TEXT("DA_Plant_ScentHerb"));
	ScentHerb->DisplayName = FText::FromString(TEXT("Scent Herb"));
	ScentHerb->PlantTag = Tag(TEXT("Plant.ScentHerb"));
	ScentHerb->EffectTag = Tag(TEXT("Effect.Plant.Scent"));
	ScentHerb->MovementSpeedMultiplier = 1.0f;
	ScentHerb->DurationDays = 0.5f;
	SaveAssetPackage(ScentHerb);

	UHSPlantDefinition* SenseHerb = ConfigureAsset<UHSPlantDefinition>(TEXT("/Game/Data/Plants/DA_Plant_SenseHerb"), TEXT("DA_Plant_SenseHerb"));
	SenseHerb->DisplayName = FText::FromString(TEXT("Sense Herb"));
	SenseHerb->PlantTag = Tag(TEXT("Plant.SenseHerb"));
	SenseHerb->EffectTag = Tag(TEXT("Effect.Plant.Sense"));
	SenseHerb->MovementSpeedMultiplier = 1.0f;
	SenseHerb->DurationDays = 0.5f;
	SaveAssetPackage(SenseHerb);

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

	struct FBuildingAssetSeed
	{
		const TCHAR* PackageName;
		const TCHAR* AssetName;
		const TCHAR* DisplayName;
		const TCHAR* BuildingTag;
	};

	const FBuildingAssetSeed BuildingSeeds[] = {
		{ TEXT("/Game/Data/Buildings/DA_Building_BasicHouseMinorUpgrade"), TEXT("DA_Building_BasicHouseMinorUpgrade"), TEXT("Minor House Upgrade"), TEXT("Building.BasicHouse.MinorUpgrade") },
		{ TEXT("/Game/Data/Buildings/DA_Building_BasicHouseMidUpgrade"), TEXT("DA_Building_BasicHouseMidUpgrade"), TEXT("Mid House Upgrade"), TEXT("Building.BasicHouse.MidUpgrade") },
		{ TEXT("/Game/Data/Buildings/DA_Building_BasicHouseMajorUpgrade"), TEXT("DA_Building_BasicHouseMajorUpgrade"), TEXT("Major House Upgrade"), TEXT("Building.BasicHouse.MajorUpgrade") },
		{ TEXT("/Game/Data/Buildings/DA_Building_StorageHut"), TEXT("DA_Building_StorageHut"), TEXT("Storage Hut"), TEXT("Building.StorageHut") },
		{ TEXT("/Game/Data/Buildings/DA_Building_Workshop"), TEXT("DA_Building_Workshop"), TEXT("Workshop"), TEXT("Building.Workshop") },
		{ TEXT("/Game/Data/Buildings/DA_Building_WatchPost"), TEXT("DA_Building_WatchPost"), TEXT("Watch Post"), TEXT("Building.WatchPost") },
		{ TEXT("/Game/Data/Buildings/DA_Building_HerbalHut"), TEXT("DA_Building_HerbalHut"), TEXT("Herbal Hut"), TEXT("Building.HerbalHut") }
	};

	for (const FBuildingAssetSeed& Seed : BuildingSeeds)
	{
		UHSBuildingDefinition* Building = ConfigureAsset<UHSBuildingDefinition>(Seed.PackageName, Seed.AssetName);
		Building->DisplayName = FText::FromString(Seed.DisplayName);
		Building->BuildingTag = Tag(Seed.BuildingTag);
		Building->RequiredKnowledgeTags = Tags({ TEXT("Knowledge.Settlement.BasicCrafting") });
		Building->ResourceCosts.Reset();
		SaveAssetPackage(Building);
	}

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

AHSPrototypeVisualActor* SpawnVisual(UWorld* World, const TCHAR* Name, const FVector& Location, EHSPrototypeVisualShape Shape, FLinearColor Color, FVector Scale, const FRotator& Rotation = FRotator::ZeroRotator)
{
	AHSPrototypeVisualActor* Visual = SpawnNamedActor<AHSPrototypeVisualActor>(World, Name, Location, Rotation);
	Visual->ConfigureVisual(Shape, Color, Scale);
	return Visual;
}

UStaticMesh* LoadNatureMesh(const TCHAR* MeshName)
{
	const FString ObjectPath = FString::Printf(TEXT("/Game/LowPolyNatureLite/Assets/Models/%s.%s"), MeshName, MeshName);
	return LoadObject<UStaticMesh>(nullptr, *ObjectPath);
}

FLinearColor GetNatureMeshTint(const TCHAR* MeshName)
{
	const FString Name(MeshName);
	if (Name.Contains(TEXT("Rock")) || Name.Contains(TEXT("Stone")) || Name.Contains(TEXT("Mountain")) || Name.Contains(TEXT("Hills")))
	{
		return FLinearColor(0.36f, 0.38f, 0.34f, 1.0f);
	}
	if (Name.Contains(TEXT("Log")) || Name.Contains(TEXT("Fence")) || Name.Contains(TEXT("Dead")))
	{
		return FLinearColor(0.34f, 0.22f, 0.12f, 1.0f);
	}
	if (Name.Contains(TEXT("Tent_Blue")))
	{
		return FLinearColor(0.18f, 0.32f, 0.48f, 1.0f);
	}
	if (Name.Contains(TEXT("Tent_Red")))
	{
		return FLinearColor(0.48f, 0.18f, 0.14f, 1.0f);
	}
	if (Name.Contains(TEXT("Grass")) || Name.Contains(TEXT("Bush")) || Name.Contains(TEXT("Tree")) || Name.Contains(TEXT("Pine")))
	{
		return FLinearColor(0.14f, 0.38f, 0.16f, 1.0f);
	}
	return FLinearColor(0.24f, 0.42f, 0.20f, 1.0f);
}

AHSPrototypeVisualActor* SpawnNatureVisual(UWorld* World, const TCHAR* Name, const FVector& Location, const TCHAR* MeshName, FVector Scale, const FRotator& Rotation = FRotator::ZeroRotator)
{
	AHSPrototypeVisualActor* Visual = SpawnNamedActor<AHSPrototypeVisualActor>(World, Name, Location, Rotation);
	if (UStaticMesh* NatureMesh = LoadNatureMesh(MeshName))
	{
		Visual->ConfigureStaticMesh(NatureMesh, Scale, GetNatureMeshTint(MeshName));
	}
	else
	{
		Visual->ConfigureVisual(EHSPrototypeVisualShape::Cube, FLinearColor(0.30f, 0.45f, 0.24f, 1.0f), Scale);
	}
	return Visual;
}

void SpawnTreeCluster(UWorld* World, const TCHAR* NamePrefix, const TArray<FVector>& Locations)
{
	int32 Index = 0;
	for (const FVector& Location : Locations)
	{
		const FString TreeName = FString::Printf(TEXT("%s_Tree_%02d"), NamePrefix, Index);
		const TCHAR* MeshName = Index % 3 == 0 ? TEXT("SM_Tree04") : TEXT("SM_Pine01");
		const float Scale = Index % 2 == 0 ? 1.15f : 0.92f;
		SpawnNatureVisual(World, *TreeName, Location + FVector(0.0f, 0.0f, 8.0f), MeshName, FVector(Scale), FRotator(0.0f, 23.0f * Index, 0.0f));
		++Index;
	}
}

void SpawnFenceRun(UWorld* World, const TCHAR* NamePrefix, const TArray<FVector>& Locations, bool bHorizontal)
{
	int32 Index = 0;
	for (const FVector& Location : Locations)
	{
		const FString PostName = FString::Printf(TEXT("%s_Post_%02d"), NamePrefix, Index);
		SpawnVisual(World, *PostName, Location + FVector(0.0f, 0.0f, 45.0f), EHSPrototypeVisualShape::Cylinder, FLinearColor(0.36f, 0.25f, 0.15f, 1.0f), FVector(0.08f, 0.08f, 0.9f));
		if (Index > 0)
		{
			const FVector PreviousLocation = Locations[Index - 1];
			const FVector Midpoint = (PreviousLocation + Location) * 0.5f + FVector(0.0f, 0.0f, 58.0f);
			const float Length = FVector::Dist2D(PreviousLocation, Location) / 100.0f;
			const FString RailName = FString::Printf(TEXT("%s_Rail_%02d"), NamePrefix, Index);
			SpawnNatureVisual(World, *RailName, Midpoint, TEXT("SM_Fence"), bHorizontal ? FVector(Length * 0.62f, 0.22f, 0.28f) : FVector(0.22f, Length * 0.62f, 0.28f), bHorizontal ? FRotator::ZeroRotator : FRotator(0.0f, 90.0f, 0.0f));
		}
		++Index;
	}
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
	World->GetWorldSettings()->bForceNoPrecomputedLighting = true;

	AStaticMeshActor* Ground = SpawnNamedActor<AStaticMeshActor>(World, TEXT("Prototype_Ground"), FVector::ZeroVector);
	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		Ground->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
	}
	Ground->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ground->GetStaticMeshComponent()->SetVisibility(false);
	Ground->SetActorHiddenInGame(true);
	Ground->SetActorScale3D(FVector(42.0f, 34.0f, 1.0f));

	SpawnVisual(World, TEXT("Ground_Grass_Visual"), FVector(0.0f, 0.0f, -2.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.17f, 0.30f, 0.13f, 1.0f), FVector(42.0f, 34.0f, 0.02f));
	SpawnVisual(World, TEXT("Region_Settlement_Dirt"), FVector(-360.0f, -220.0f, 2.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.42f, 0.34f, 0.23f, 1.0f), FVector(7.2f, 4.6f, 0.025f));
	SpawnVisual(World, TEXT("Region_Village_Dirt"), FVector(-720.0f, -180.0f, 3.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), FVector(5.0f, 3.2f, 0.025f));
	SpawnVisual(World, TEXT("Region_Forest_Floor"), FVector(720.0f, -420.0f, 4.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.12f, 0.22f, 0.11f, 1.0f), FVector(10.5f, 5.8f, 0.025f));
	SpawnVisual(World, TEXT("Region_Herb_Glade"), FVector(360.0f, 300.0f, 5.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.16f, 0.30f, 0.13f, 1.0f), FVector(4.0f, 2.8f, 0.025f));

	SpawnNamedActor<APlayerStart>(World, TEXT("PlayerStart_OneMorningHunt"), FVector(-300.0f, 0.0f, 90.0f));
	ADirectionalLight* Sun = SpawnNamedActor<ADirectionalLight>(World, TEXT("Sun_Morning"), FVector(0.0f, 0.0f, 400.0f), FRotator(-35.0f, -40.0f, 0.0f));
	if (ULightComponent* SunComponent = Sun->GetLightComponent())
	{
		SunComponent->SetMobility(EComponentMobility::Movable);
		SunComponent->SetCastShadows(false);
		SunComponent->SetIntensity(0.85f);
	}

	ASkyLight* SkyLight = SpawnNamedActor<ASkyLight>(World, TEXT("SkyLight_Morning"), FVector(0.0f, 0.0f, 250.0f));
	if (USkyLightComponent* SkyLightComponent = SkyLight->GetLightComponent())
	{
		SkyLightComponent->SetMobility(EComponentMobility::Movable);
		SkyLightComponent->SetCastShadows(false);
		SkyLightComponent->SetIntensity(0.25f);
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
	AHSAnimalActor* Boar = SpawnNamedActor<AHSAnimalActor>(World, TEXT("Animal_Boar_Prototype"), FVector(980.0f, -620.0f, 70.0f), FRotator(0.0f, -110.0f, 0.0f));
	if (UHSAnimalDefinition* BoarDefinition = LoadObject<UHSAnimalDefinition>(nullptr, TEXT("/Game/Data/Animals/DA_Animal_Boar.DA_Animal_Boar")))
	{
		Boar->SetAnimalDefinition(BoarDefinition);
	}
	AHSAnimalActor* Hare = SpawnNamedActor<AHSAnimalActor>(World, TEXT("Animal_Hare_Prototype"), FVector(460.0f, 470.0f, 55.0f), FRotator(0.0f, -20.0f, 0.0f));
	if (UHSAnimalDefinition* HareDefinition = LoadObject<UHSAnimalDefinition>(nullptr, TEXT("/Game/Data/Animals/DA_Animal_Hare.DA_Animal_Hare")))
	{
		Hare->SetAnimalDefinition(HareDefinition);
		Hare->SetActorScale3D(FVector(0.55f, 0.55f, 0.55f));
	}

	SpawnNamedActor<AHSInteractionTarget>(World, TEXT("Interaction_Target_Practice"), FVector(200.0f, 160.0f, 90.0f), FRotator(0.0f, 180.0f, 0.0f));
	SpawnNamedActor<AHSRestPointActor>(World, TEXT("Camp_RestPoint_EndMorning"), FVector(-80.0f, 220.0f, 45.0f), FRotator::ZeroRotator);
	SpawnNamedActor<AHSTemporaryShelterActor>(World, TEXT("Temporary_Shelter_Sleep"), FVector(-230.0f, 220.0f, 55.0f), FRotator(0.0f, 25.0f, 0.0f));
	SpawnNamedActor<AHSSettlementClaimActor>(World, TEXT("Settlement_Claim_Marker"), FVector(-80.0f, -260.0f, 55.0f), FRotator::ZeroRotator);

	SpawnVisual(World, TEXT("Village_Hut_A"), FVector(-720.0f, -300.0f, 65.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.46f, 0.37f, 0.25f, 1.0f), FVector(1.6f, 1.05f, 1.3f));
	SpawnVisual(World, TEXT("Village_Hut_A_Roof"), FVector(-720.0f, -300.0f, 150.0f), EHSPrototypeVisualShape::Cone, FLinearColor(0.18f, 0.20f, 0.15f, 1.0f), FVector(1.9f, 1.35f, 0.55f));
	SpawnVisual(World, TEXT("Village_Hut_B"), FVector(-860.0f, -80.0f, 55.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.43f, 0.34f, 0.23f, 1.0f), FVector(1.35f, 0.9f, 1.1f), FRotator(0.0f, 18.0f, 0.0f));
	SpawnVisual(World, TEXT("Village_Hut_B_Roof"), FVector(-860.0f, -80.0f, 130.0f), EHSPrototypeVisualShape::Cone, FLinearColor(0.17f, 0.19f, 0.14f, 1.0f), FVector(1.6f, 1.15f, 0.48f), FRotator(0.0f, 18.0f, 0.0f));
	SpawnVisual(World, TEXT("Builder_Workbench"), FVector(-430.0f, -345.0f, 42.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.37f, 0.24f, 0.12f, 1.0f), FVector(1.25f, 0.45f, 0.25f), FRotator(0.0f, 8.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Builder_Log_Pile_A"), FVector(-520.0f, -360.0f, 25.0f), TEXT("SM_Log"), FVector(0.9f), FRotator(0.0f, 15.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Builder_Log_Pile_B"), FVector(-520.0f, -325.0f, 25.0f), TEXT("SM_Log"), FVector(0.8f), FRotator(0.0f, -18.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Village_Tent_Blue"), FVector(-610.0f, -130.0f, 8.0f), TEXT("SM_Tent_Blue"), FVector(0.9f), FRotator(0.0f, 18.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Village_Tent_Red"), FVector(-785.0f, 95.0f, 8.0f), TEXT("SM_Tent_Red"), FVector(0.78f), FRotator(0.0f, -12.0f, 0.0f));
	SpawnVisual(World, TEXT("Trail_Road_Settlement"), FVector(-170.0f, -295.0f, 7.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.32f, 0.27f, 0.18f, 1.0f), FVector(8.8f, 0.35f, 0.02f), FRotator(0.0f, -8.0f, 0.0f));
	SpawnVisual(World, TEXT("Trail_Road_Forest"), FVector(420.0f, -350.0f, 8.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.29f, 0.25f, 0.17f, 1.0f), FVector(8.2f, 0.28f, 0.02f), FRotator(0.0f, -6.0f, 0.0f));
	SpawnVisual(World, TEXT("Stream_Performance_Test"), FVector(500.0f, 90.0f, 9.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.10f, 0.24f, 0.32f, 1.0f), FVector(6.5f, 0.22f, 0.018f), FRotator(0.0f, -16.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Landmark_StandingStone"), FVector(1010.0f, -145.0f, 18.0f), TEXT("SM_Stone02"), FVector(1.25f, 0.9f, 1.75f), FRotator(0.0f, 10.0f, 0.0f));

	SpawnFenceRun(World, TEXT("Settlement_Fence_North"), { FVector(-650.0f, -430.0f, 0.0f), FVector(-480.0f, -430.0f, 0.0f), FVector(-310.0f, -430.0f, 0.0f), FVector(-140.0f, -430.0f, 0.0f) }, true);
	SpawnFenceRun(World, TEXT("Settlement_Fence_West"), { FVector(-650.0f, -430.0f, 0.0f), FVector(-650.0f, -250.0f, 0.0f), FVector(-650.0f, -70.0f, 0.0f) }, false);

	AHSPlantActor* SpeedHerb = SpawnNamedActor<AHSPlantActor>(World, TEXT("Plant_SpeedHerb"), FVector(360.0f, 230.0f, 45.0f), FRotator::ZeroRotator);
	SpeedHerb->SetPlantDefinition(GetOrCreateAsset<UHSPlantDefinition>(TEXT("/Game/Data/Plants/DA_Plant_SpeedHerb"), TEXT("DA_Plant_SpeedHerb")));
	AHSPlantActor* ScentHerb = SpawnNamedActor<AHSPlantActor>(World, TEXT("Plant_ScentHerb"), FVector(430.0f, 315.0f, 45.0f), FRotator::ZeroRotator);
	ScentHerb->SetPlantDefinition(GetOrCreateAsset<UHSPlantDefinition>(TEXT("/Game/Data/Plants/DA_Plant_ScentHerb"), TEXT("DA_Plant_ScentHerb")));
	AHSPlantActor* SenseHerb = SpawnNamedActor<AHSPlantActor>(World, TEXT("Plant_SenseHerb"), FVector(300.0f, 340.0f, 45.0f), FRotator::ZeroRotator);
	SenseHerb->SetPlantDefinition(GetOrCreateAsset<UHSPlantDefinition>(TEXT("/Game/Data/Plants/DA_Plant_SenseHerb"), TEXT("DA_Plant_SenseHerb")));

	SpawnNatureVisual(World, TEXT("HerbGlade_Bush_A"), FVector(275.0f, 270.0f, 7.0f), TEXT("SM_Bush_Berries_blue"), FVector(0.95f), FRotator(0.0f, 15.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("HerbGlade_Bush_B"), FVector(480.0f, 365.0f, 7.0f), TEXT("SM_Bush_Berries_Red"), FVector(0.85f), FRotator(0.0f, -28.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("HerbGlade_Grass_A"), FVector(220.0f, 345.0f, 5.0f), TEXT("SM_Grass_Array01"), FVector(1.4f), FRotator(0.0f, 42.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("HerbGlade_Grass_B"), FVector(420.0f, 235.0f, 5.0f), TEXT("SM_Grass01"), FVector(1.2f), FRotator(0.0f, -12.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Forest_Bush_A"), FVector(760.0f, -255.0f, 6.0f), TEXT("SM_Bush_Simple"), FVector(1.05f), FRotator(0.0f, 18.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Forest_DeadTree_A"), FVector(1180.0f, -520.0f, 7.0f), TEXT("SM_Dead_Tree"), FVector(1.1f), FRotator(0.0f, -35.0f, 0.0f));

	SpawnExplorationMarker(World, TEXT("Explore_Village_Core"), FVector(-640.0f, -230.0f, 100.0f), FText::FromString(TEXT("Village Core")), TEXT("Location.Village"));
	SpawnExplorationMarker(World, TEXT("Explore_Forest_Edge"), FVector(900.0f, -360.0f, 100.0f), FText::FromString(TEXT("Forest Edge")), TEXT("Location.Forest"));
	SpawnExplorationMarker(World, TEXT("Explore_Animal_Ridge"), FVector(720.0f, -420.0f, 100.0f), FText::FromString(TEXT("Animal Ridge")), TEXT("Location.AnimalArea"));
	SpawnExplorationMarker(World, TEXT("Trail_Settlement_01"), FVector(-260.0f, -245.0f, 95.0f), FText::FromString(TEXT("Village Trail")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Road_Settlement_01"), FVector(-120.0f, -310.0f, 95.0f), FText::FromString(TEXT("Settlement Road")), TEXT("Location.Road"));
	SpawnExplorationMarker(World, TEXT("Trail_Forest_02"), FVector(180.0f, -300.0f, 95.0f), FText::FromString(TEXT("Forest Trail")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Trail_Forest_03"), FVector(560.0f, -340.0f, 95.0f), FText::FromString(TEXT("Ridge Trail")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Trail_Ridge_04"), FVector(780.0f, -520.0f, 95.0f), FText::FromString(TEXT("Ridge Return")), TEXT("Location.Trail"));
	SpawnExplorationMarker(World, TEXT("Explore_Herb_Patch"), FVector(360.0f, 305.0f, 100.0f), FText::FromString(TEXT("Herb Patch")), TEXT("Location.PlantPatch"));
	SpawnExplorationMarker(World, TEXT("Explore_Stream"), FVector(500.0f, 90.0f, 95.0f), FText::FromString(TEXT("Cold Stream")), TEXT("Location.River"));
	SpawnExplorationMarker(World, TEXT("Explore_StandingStone"), FVector(1010.0f, -145.0f, 150.0f), FText::FromString(TEXT("Standing Stone")), TEXT("Location.Landmark"));
	SpawnExplorationMarker(World, TEXT("Explore_Builder_Yard"), FVector(-430.0f, -330.0f, 100.0f), FText::FromString(TEXT("Builder Yard")), TEXT("Location.Village"));

	SpawnTreeCluster(World, TEXT("ForestEdge"), {
		FVector(520.0f, -620.0f, 0.0f),
		FVector(690.0f, -690.0f, 0.0f),
		FVector(890.0f, -610.0f, 0.0f),
		FVector(1040.0f, -470.0f, 0.0f),
		FVector(1110.0f, -280.0f, 0.0f),
		FVector(950.0f, -130.0f, 0.0f)
	});
	SpawnTreeCluster(World, TEXT("HerbGlade"), {
		FVector(180.0f, 420.0f, 0.0f),
		FVector(340.0f, 520.0f, 0.0f),
		FVector(530.0f, 430.0f, 0.0f)
	});
	SpawnNatureVisual(World, TEXT("Ridge_Rock_A"), FVector(650.0f, -500.0f, 20.0f), TEXT("SM_Rock02"), FVector(1.1f, 0.8f, 0.7f), FRotator(0.0f, 22.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Ridge_Rock_B"), FVector(820.0f, -560.0f, 18.0f), TEXT("SM_Stones02"), FVector(0.9f), FRotator(0.0f, -18.0f, 0.0f));
	SpawnVisual(World, TEXT("Ridge_Outcrop"), FVector(910.0f, -420.0f, 45.0f), EHSPrototypeVisualShape::Cube, FLinearColor(0.30f, 0.31f, 0.29f, 1.0f), FVector(1.8f, 0.65f, 0.65f), FRotator(0.0f, -24.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Boundary_Mountain_North_A"), FVector(120.0f, -1350.0f, 15.0f), TEXT("SM_Mountain01"), FVector(5.0f, 2.2f, 2.0f), FRotator(0.0f, 4.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Boundary_Mountain_North_B"), FVector(660.0f, -1320.0f, 15.0f), TEXT("SM_Hills01"), FVector(4.2f, 2.0f, 1.8f), FRotator(0.0f, -12.0f, 0.0f));
	SpawnNatureVisual(World, TEXT("Boundary_Mountain_East"), FVector(1500.0f, -260.0f, 15.0f), TEXT("SM_Hills02"), FVector(2.2f, 5.0f, 2.0f), FRotator(0.0f, -82.0f, 0.0f));

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
