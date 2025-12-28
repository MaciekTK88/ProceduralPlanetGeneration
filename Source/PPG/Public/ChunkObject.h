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
#include "ComputeShader/Public/PlanetComputeShader/PlanetComputeShader.h"
#include "ChunkObject.generated.h"


/**
 * 
 */
USTRUCT()
struct FFoliageRuntimeData
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleDefaultsOnly, Category = "Foliage")
	TObjectPtr<UInstancedStaticMeshComponent> Ismc = nullptr;
	
	UPROPERTY(VisibleDefaultsOnly, Category = "Foliage")
	FFoliageList Foliage;

	UPROPERTY(EditAnywhere, Category = "Foliage")
	TArray<FTransform> LocalFoliageTransforms;
};

/**
 * Component-like object representing a single terrain chunk.
 */
UCLASS(Blueprintable, BlueprintType)
class PPG_API UChunkObject : public UObject
{
	GENERATED_BODY()

public:
	UChunkObject();
	
	enum class EChunkStatus : uint8 {
		PENDING_GENERATION = 0,
		GENERATING = 1,
		WAITING_FOR_GPU = 2,
		PENDING_ASSIGN = 3,
		READY = 4,
		REMOVING = 5,
		ABORTED = 6,
	};
	
	EChunkStatus ChunkStatus = EChunkStatus::PENDING_GENERATION;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Setup")
	bool bNaniteLandscape = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Setup")
	TObjectPtr<UPlanetData> PlanetData;
	
	UPROPERTY(BlueprintReadOnly, Category = "Chunk|Render")
	TObjectPtr<UTexture2D> GPUBiomeData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Setup")
	bool bGenerateCollisions = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Setup")
	bool bGenerateRayTracingProxy = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Setup")
	int32 CollisionDisableDistance = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chunk|Setup")
	bool bGenerateFoliage = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chunk|Setup")
	float FoliageDensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Status")
	bool bIsReady = false;

	UFUNCTION(BlueprintCallable, Category = "Chunk|Generation")
	void GenerateChunk();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Generation")
	void CompleteChunkGeneration();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Generation")
	void UploadChunk();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Generation")
	void UploadFoliageData();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Generation")
	void AddWaterChunk();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Generation")
	void GenerationComplete();

	void TickGPUReadback();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Generation")
	void AssignComponents();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Foliage")
	void SetFoliageActor(AActor* NewFoliageActor);

	UFUNCTION(BlueprintCallable, Category = "Chunk|Lifecycle")
	void BeginSelfDestruct(bool bFreeComponents);
	
	UFUNCTION(BlueprintCallable, Category = "Chunk|Lifecycle")
	void FreeComponents();

	UFUNCTION(BlueprintCallable, Category = "Chunk|Lifecycle")
	void SelfDestruct();

	void SetSharedResources(TArray<TObjectPtr<UStaticMeshComponent>>* InChunkSMCPool, TArray<TObjectPtr<UInstancedStaticMeshComponent>>* InFoliageISMCPool, TArray<TObjectPtr<UStaticMeshComponent>>* InWaterSMCPool, TArray<uint32>* InTriangles);
	void InitializeChunk(float InChunkWorldSize, int32 InRecursionLevel, int32 InPlanetType, FVector InChunkLocation, FVector InPlanetSpaceLocation, FIntVector InPlanetSpaceRotation, float InChunkMaxHeight, uint8 InMaterialLayersNum, UStaticMesh* InCloseWaterMesh, UStaticMesh* InFarWaterMesh);

	void SetAbortAsync(bool bInAbortAsync) { bAbortAsync = bInAbortAsync; }
	bool GetAbortAsync() const { return bAbortAsync; }
	float GetChunkMaxHeight() const { return ChunkMaxHeight; }

protected:
	void ProcessChunkData(const TArray<float>& OutputVal, const TArray<uint8>& OutputVCVal);
	void SpawnFoliageComponent(FFoliageRuntimeData& Data);

	UPROPERTY()
	int32 SpawnAtOnce = 10;

	UPROPERTY()
	FVector ChunkOriginLocation;

	UPROPERTY()
	int32 RecursionLevel = 0;

	UPROPERTY()
	int32 PlanetType = 0;

	UPROPERTY()
	float ChunkMaxHeight = 0.0f;
	
	UPROPERTY()
	float ChunkSize = 200.0f;

	UPROPERTY()
	int32 ChunkQuality = 191;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Dimensions")
	double ChunkMinHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Dimensions")
	FVector PlanetSpaceLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chunk|Dimensions")
	FIntVector PlanetSpaceRotation;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> BiomeMap;
	
	UPROPERTY()
	bool bDataGenerated = false;

	UPROPERTY()
	bool bAbortAsync = false;

	UPROPERTY()
	uint8 MaterialLayersNum = 0;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FarWaterMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CloseWaterMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ChunkSMC;

	TArray<TObjectPtr<UStaticMeshComponent>>* ChunkSMCPool;
	TArray<TObjectPtr<UInstancedStaticMeshComponent>>* FoliageISMCPool;
	TArray<TObjectPtr<UStaticMeshComponent>>* WaterSMCPool;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ChunkStaticMesh;
	
	TArray<uint32>* Triangles;

	FPlanetComputeShaderReadback GPUReadback;

private:
	UPROPERTY()
	TArray<FVector3f> Vertices;
	UPROPERTY()
	TArray<FVector3f> Normals;
	UPROPERTY()
	TArray<FVector2f> UVs;
	UPROPERTY()
	TArray<FColor> VertexColors;
	UPROPERTY()
	TArray<float> VertexHeight;
	UPROPERTY()
	TArray<uint8> Slopes;
	UPROPERTY()
	TArray<uint8> Biomes;
	UPROPERTY()
	TSet<uint8> FoliageBiomeIndices;
	UPROPERTY()
	TArray<uint8> ForestStrength;

	UPROPERTY()
	bool bRayTracing = false;

	UPROPERTY()
	bool bCollisions = false;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> WaterChunk;
	
	TArray<FFoliageRuntimeData> FoliageRuntimeData;

	UPROPERTY()
	TObjectPtr<AActor> FoliageActor;

	UPROPERTY()
	TArray<FVoxelOctahedron> Octahedrons;

	UPROPERTY()
	int32 VerticesCount = 192;

	UPROPERTY()
	float TriangleSize = 100.0f;

	// Hashing and math helpers
	static uint32_t fmix32(uint32_t h);
	static uint32_t FloatAsUint(float f);
	static float QuantizeFloat(float f, float Range);
	static double QuantizeDouble(double d, double Range);
	static float Hash3DToUnit(const FVector& P, uint32_t Salt = 0u);
	static FVector2D HashFVectorToVector2D(const FVector& Input, float RangeMin, float RangeMax, float RangeTolerance = 0.0f);
};
