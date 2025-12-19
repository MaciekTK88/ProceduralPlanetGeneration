// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoxelMinimal.h"
#include "PlanetData.h"
#include "FoliageData.h"
#include "PlanetNaniteBuilder.h"
#include "Rendering/NaniteResources.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "ChunkComponent.generated.h"


/**
 * 
 */
USTRUCT()
struct FFoliageRuntimeDataS
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleDefaultsOnly)
	UInstancedStaticMeshComponent* Ismc = nullptr;
	
	UPROPERTY(VisibleDefaultsOnly)
	FFoliageListS Foliage;

	UPROPERTY(EditAnywhere)
	TArray<FTransform> LocalFoliageTransforms;
};

UCLASS(Blueprintable, BlueprintType)
class PPG_API UChunkComponent : public UObject
{
	GENERATED_BODY()

	UPROPERTY()
	int VerticesCount = 192;

	UPROPERTY()
	float TriangleSize = 100.0f;




public:
	// Sets default values for this actor's properties
	UChunkComponent();

	UPROPERTY()
	bool bNaniteLandscape = true;

	UPROPERTY()
	UPlanetData* PlanetData;
	
	enum class EChunkStatus : uint8 {
		EMPTY = 0,
		GENERATING = 1,
		PENDING_ASSIGN = 2,
		READY = 3,
		REMOVING = 4,
		ABORTED = 5
	};
	

	EChunkStatus ChunkStatus = EChunkStatus::EMPTY;

	UPROPERTY()
	UTexture2D* GPUBiomeData;

	UPROPERTY()
	bool bGenerateCollisions = true;

	UPROPERTY()
	bool bGenerateRayTracingProxy = true;

	UPROPERTY()
	int CollisionDisableDistance = 3;

	UPROPERTY()
	bool bGenerateFoliage = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkSetup")
	float FoliageDensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool bIsReady = false;

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void GenerateChunk();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void CompleteChunkGeneration();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void UploadChunk();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void UploadFoliageData();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void AddWaterChunk();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void GenerationComplete();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void AssignComponents();

	UFUNCTION(BlueprintCallable, Category = "Foliage")
	void SetFoliageActor(AActor* NewFoliageActor);

	UFUNCTION(BlueprintCallable, Category = "DestroyChunk")
	void BeginSelfDestruct();
	
	UFUNCTION(BlueprintCallable, Category = "DestroyChunk")
	void FreeComponents();

	UFUNCTION(BlueprintCallable, Category = "DestroyChunk")
	void SelfDestruct();

	void SetSharedResources(TArray<UStaticMeshComponent*>* InChunkSMCPool, TArray<UInstancedStaticMeshComponent*>* InFoliageISMCPool, TArray<UStaticMeshComponent*>* InWaterSMCPool, TArray<uint32>* InTriangles);
	void InitializeChunk(float InChunkWorldSize, int InRecursionLevel, int InPlanetType, FVector InChunkLocation, FVector InPlanetSpaceLocation, FIntVector InPlanetSpaceRotation, float InChunkMaxHeight, uint8 InMaterialLayersNum, UStaticMesh* InCloseWaterMesh, UStaticMesh* InFarWaterMesh);

	void SetAbortAsync(bool bInAbortAsync) { bAbortAsync = bInAbortAsync; }

protected:
	void ProcessChunkData(const TArray<float>& OutputVal, const TArray<uint8>& OutputVCVal);

	UPROPERTY()
	int SpawnAtOnce = 10;

	UPROPERTY()
	FVector ChunkOriginLocation;

	UPROPERTY()
	int RecursionLevel = 0;

	UPROPERTY()
	int PlanetType = 0;

	UPROPERTY()
	float ChunkMaxHeight = 0;
	
	UPROPERTY()
	float ChunkSize = 200;

	UPROPERTY()
	int ChunkQuality = 191;

public:
	float GetChunkMaxHeight() const { return ChunkMaxHeight; }
protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	double ChunkMinHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	FVector PlanetSpaceLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	FIntVector PlanetSpaceRotation;

	UPROPERTY()
	UTextureRenderTarget2D* BiomeMap;
	
	UPROPERTY()
	bool bDataGenerated = false;

	UPROPERTY()
	bool bAbortAsync = false;

	UPROPERTY()
	uint8 MaterialLayersNum = 0;

	UPROPERTY()
	UStaticMesh* FarWaterMesh;

	UPROPERTY()
	UStaticMesh* CloseWaterMesh;

	UPROPERTY()
	UStaticMeshComponent* ChunkSMC;

protected:
	TArray<UStaticMeshComponent*>* ChunkSMCPool;
	TArray<UInstancedStaticMeshComponent*>* FoliageISMCPool;
	TArray<UStaticMeshComponent*>* WaterSMCPool;

public:

	UPROPERTY()
	UStaticMesh* ChunkStaticMesh;
	
	TArray<uint32>* Triangles;


protected:


private:

	void SpawnFoliageComponent(FFoliageRuntimeDataS& Data);

	UPROPERTY()
	TArray<FVector3f> Vertices;
	UPROPERTY()
	TArray<FVector3f> Normals;
	UPROPERTY()
	TArray<FVector2f> UVs;
	UPROPERTY()
	TArray<FColor> VertexColors;
	UPROPERTY()
	TArray<float>VertexHeight;
	UPROPERTY()
	TArray<uint8> Slopes;
	UPROPERTY()
	TArray<uint8> Biomes;
	UPROPERTY()
	TSet<uint8> FoliageBiomeIndices;
	UPROPERTY()
	TArray<uint8> ForestStrength;

	//UPROPERTY(EditAnywhere)
	//TArray<UFoliageData*> FoliageBiomes;

	UPROPERTY()
	bool bRayTracing = false;

	UPROPERTY()
	bool bCollisions = false;

	UPROPERTY()
	UStaticMeshComponent* WaterChunk;

	TUniquePtr<FStaticMeshRenderData> RenderData;
	
	
	FPlanetNaniteBuilder NaniteBuilder;

	UPROPERTY()
	UMaterialInstanceDynamic* MaterialInst;

	UPROPERTY()
	UMaterialInstanceDynamic* WaterMaterialInst;
	
	UPROPERTY()
	UStaticMesh* GeneratedMesh;
	
	TArray<FFoliageRuntimeDataS> FoliageRuntimeData;

	UPROPERTY()
	AActor* FoliageActor;

	UPROPERTY()
	TArray<FVoxelOctahedron> Octahedrons;
	
	
	


	

	
	static inline uint32_t fmix32(uint32_t h)
	{
		h ^= h >> 16;
		h *= 0x85ebca6bU;
		h ^= h >> 13;
		h *= 0xc2b2ae35U;
		h ^= h >> 16;
		return h;
	}

	// safe, portable reinterpret of float bits -> uint32_t
	static inline uint32_t FloatAsUint(float f)
	{
		uint32_t u;
		std::memcpy(&u, &f, sizeof(u));
		return u;
	}

	// helper: quantize a float so that values within `Range` map to same quantized bucket
	static float QuantizeFloat(float f, float Range)
	{
		if (Range <= 0.0f)
			return f; // no quantization

		// Round to nearest multiple of Range
		return FMath::RoundToFloat(f / Range) * Range;
	}
	
	//QuantizeDouble
	static double QuantizeDouble(double d, double Range)
	{
		if (Range <= 0.0)
			return d; // no quantization
		
		// Round to nearest multiple of Range
		return FMath::RoundToDouble(d / Range) * Range;
	}

	// core 3D -> [0,1] hash, optional salt for getting independent hashes
	static float Hash3DToUnit(const FVector& P, uint32_t Salt = 0u)
	{
		const uint32_t ux = FloatAsUint(P.X);
		const uint32_t uy = FloatAsUint(P.Y);
		const uint32_t uz = FloatAsUint(P.Z);

		uint32_t h = (ux * 0x9e3779b1u) ^ (uy * 0x85ebca6bu) ^ (uz * 0xc2b2ae35u) ^ Salt;
		h = fmix32(h);

		// map 0..0xFFFFFFFF -> 0.0 .. 1.0
		return static_cast<float>(h) / 4294967295.0f;
	}

	// final function: returns two independent components in [RangeMin, RangeMax]
	// RangeTolerance controls when nearby inputs are considered the same
	static FVector2D HashFVectorToVector2D(const FVector& Input, float RangeMin, float RangeMax, float RangeTolerance = 0.0f)
	{
		FVector QuantizedInput = Input;

		if (RangeTolerance > 0.0f)
		{
			QuantizedInput.X = QuantizeFloat(Input.X, RangeTolerance);
			QuantizedInput.Y = QuantizeFloat(Input.Y, RangeTolerance);
			QuantizedInput.Z = QuantizeFloat(Input.Z, RangeTolerance);
		}

		// produce two independent hashes by using different salts
		const float hx = Hash3DToUnit(QuantizedInput, 0x243F6A88u); // arbitrary salt
		const float hy = Hash3DToUnit(QuantizedInput, 0x85A308D3u); // different arbitrary salt

		const float x = FMath::Lerp(RangeMin, RangeMax, hx);
		const float y = FMath::Lerp(RangeMin, RangeMax, hy);

		return FVector2D(x, y);
	}

};
