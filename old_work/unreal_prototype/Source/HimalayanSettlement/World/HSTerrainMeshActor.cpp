#include "World/HSTerrainMeshActor.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AHSTerrainMeshActor::AHSTerrainMeshActor()
	: HalfSizeX(2100.0f)
	, HalfSizeY(1700.0f)
	, QuadsX(48)
	, QuadsY(40)
	, HeightScale(42.0f)
{
	PrimaryActorTick.bCanEverTick = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerrainMesh->SetCollisionObjectType(ECC_WorldStatic);
	TerrainMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TerrainMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	TerrainMesh->SetGenerateOverlapEvents(false);
	TerrainMesh->SetCanEverAffectNavigation(false);
	TerrainMesh->CanCharacterStepUpOn = ECB_Yes;
	TerrainMesh->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Default, 0.0f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicColorMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicColorMaterialFinder.Succeeded())
	{
		BasicColorMaterial = BasicColorMaterialFinder.Object;
	}

	RebuildTerrain();
}

void AHSTerrainMeshActor::ConfigureTerrain(float NewHalfSizeX, float NewHalfSizeY, int32 NewQuadsX, int32 NewQuadsY, float NewHeightScale)
{
	HalfSizeX = FMath::Max(100.0f, NewHalfSizeX);
	HalfSizeY = FMath::Max(100.0f, NewHalfSizeY);
	QuadsX = FMath::Max(2, NewQuadsX);
	QuadsY = FMath::Max(2, NewQuadsY);
	HeightScale = FMath::Max(0.0f, NewHeightScale);
	RebuildTerrain();
}

float AHSTerrainMeshActor::GetTerrainHeightAtWorldLocation(FVector WorldLocation) const
{
	const FVector LocalLocation = GetActorTransform().InverseTransformPosition(WorldLocation);
	return GetActorLocation().Z + SampleLocalHeight(LocalLocation.X, LocalLocation.Y);
}

void AHSTerrainMeshActor::RebuildTerrain()
{
	if (!TerrainMesh)
	{
		return;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;

	const int32 VertCountX = QuadsX + 1;
	const int32 VertCountY = QuadsY + 1;
	Vertices.Reserve(VertCountX * VertCountY);
	Normals.Init(FVector::UpVector, VertCountX * VertCountY);
	UVs.Reserve(VertCountX * VertCountY);
	Tangents.Init(FProcMeshTangent(1.0f, 0.0f, 0.0f), VertCountX * VertCountY);
	VertexColors.Init(FLinearColor(0.10f, 0.24f, 0.10f, 1.0f), VertCountX * VertCountY);

	for (int32 Y = 0; Y < VertCountY; ++Y)
	{
		const float V = static_cast<float>(Y) / static_cast<float>(QuadsY);
		const float LocalY = FMath::Lerp(-HalfSizeY, HalfSizeY, V);
		for (int32 X = 0; X < VertCountX; ++X)
		{
			const float U = static_cast<float>(X) / static_cast<float>(QuadsX);
			const float LocalX = FMath::Lerp(-HalfSizeX, HalfSizeX, U);
			Vertices.Add(FVector(LocalX, LocalY, SampleLocalHeight(LocalX, LocalY)));
			UVs.Add(FVector2D(U * 8.0f, V * 8.0f));
		}
	}

	Triangles.Reserve(QuadsX * QuadsY * 6);
	for (int32 Y = 0; Y < QuadsY; ++Y)
	{
		for (int32 X = 0; X < QuadsX; ++X)
		{
			const int32 A = Y * VertCountX + X;
			const int32 B = A + 1;
			const int32 C = A + VertCountX;
			const int32 D = C + 1;
			Triangles.Append({ A, B, C, B, D, C });
		}
	}

	for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); TriangleIndex += 3)
	{
		const int32 IndexA = Triangles[TriangleIndex];
		const int32 IndexB = Triangles[TriangleIndex + 1];
		const int32 IndexC = Triangles[TriangleIndex + 2];
		const FVector FaceNormal = FVector::CrossProduct(Vertices[IndexB] - Vertices[IndexA], Vertices[IndexC] - Vertices[IndexA]).GetSafeNormal();
		Normals[IndexA] += FaceNormal;
		Normals[IndexB] += FaceNormal;
		Normals[IndexC] += FaceNormal;
	}
	for (FVector& Normal : Normals)
	{
		Normal = Normal.GetSafeNormal();
	}

	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
	if (BasicColorMaterial)
	{
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BasicColorMaterial, this);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.24f, 0.10f, 1.0f));
			TerrainMesh->SetMaterial(0, DynamicMaterial);
		}
	}
}

float AHSTerrainMeshActor::SampleLocalHeight(float LocalX, float LocalY) const
{
	const float SettlementFlatten = FMath::Clamp(1.0f - (FMath::Abs(LocalX + 260.0f) / 520.0f), 0.0f, 1.0f)
		* FMath::Clamp(1.0f - (FMath::Abs(LocalY + 120.0f) / 420.0f), 0.0f, 1.0f);
	const float BroadRoll = FMath::Sin(LocalX / 360.0f) * 0.48f + FMath::Cos(LocalY / 310.0f) * 0.36f;
	const float Ridge = FMath::Sin((LocalX + LocalY) / 520.0f) * 0.22f;
	const float RawHeight = (BroadRoll + Ridge) * HeightScale;
	return FMath::Lerp(RawHeight, 0.0f, SettlementFlatten * 0.86f);
}
