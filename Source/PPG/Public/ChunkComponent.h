// Fill out your copyright notice in the Description page of Project Settings.

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

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PPG_API UChunkComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

	UPROPERTY()
	int VerticesAmount = 192.0f;

	UPROPERTY()
	float Size = 100.0f;

	UPROPERTY()
	bool collision = true;


public:
	// Sets default values for this actor's properties
	UChunkComponent();
	float queue = 0;

	UPROPERTY()
	bool NaniteLandscape = true;

	UPROPERTY()
	UPlanetData* PlanetData;

	UPROPERTY()
	UTexture2D* GPUBiomeData;

	UPROPERTY()
	float ChunkSize = 200;

	UPROPERTY()
	int ChunkQuality = 200;

	UPROPERTY()
	bool GenerateCollisions = true;

	UPROPERTY()
	bool GenerateRayTracingProxy = true;

	UPROPERTY()
	int CollisionDisableDistance = 3;

	UPROPERTY()
	bool GenerateFoliage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	FVector planetCenterWS;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkSetup")
	float FoliageDensityScale = 1.0f;
	

	//UPROPERTY(EditAnywhere, BlueprintReadWrite)
	//UInstancedStaticMeshComponent* WaterMeshP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool ready = false;

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void GenerateChunk();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void CompleteChunkGeneration();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void UploadChunk();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void UploadFoliageData();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void UploadFoliage();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void AddWaterChunk();

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void GenerationComplete();

	UFUNCTION(BlueprintImplementableEvent, Category = "Foliage")
	void EventGenerate();

	UFUNCTION(BlueprintImplementableEvent, Category = "Foliage")
	void EventFoliage();

	UFUNCTION(BlueprintCallable, Category = "Foliage")
	void SetFoliageActor(AActor* NewFoliageActor);

	UFUNCTION(BlueprintCallable, Category = "DestroyChunk")
	void BeginSelfDestruct();

	UFUNCTION(BlueprintCallable, Category = "DestroyChunk")
	void SelfDestruct();

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	//TArray<UChunkComponent*>* ChunksToUpload;

	TArray<UChunkComponent*>* ChunksToFinish;

	TArray<UChunkComponent*>* ChunksToRemoveQueue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool ReadyToRemove = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage")
	int SpawnAtOnce = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	FVector ChunkLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	int recursionLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	int PlanetType = 0;

	UPROPERTY()
	TArray<UChunkComponent*> ChunksToRemove;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	float ChunkMaxHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	double ChunkMinHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	FVector PlanetSpaceLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	FIntVector PlanetSpaceRotation;

	UPROPERTY()
	UTextureRenderTarget2D* BiomeMap;

	UPROPERTY()
	UChunkComponent* ParentChunk;

	UPROPERTY()
	bool ChunkRemover = false;

	UPROPERTY()
	int ReadyToRemoveCounter = 0;
	
	UPROPERTY()
	bool DataGenerated = false;

	UPROPERTY()
	bool AbortAsync = false;

	UPROPERTY()
	uint8 MaterialLayersNum = 0;

	UPROPERTY()
	UStaticMesh* FarWaterMesh;

	UPROPERTY()
	UStaticMesh* CloseWaterMesh;



	
	TArray<uint32>* Triangles;


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	// Called when the mesh generation should happen. This could be called in the
	// editor for placed actors, or at runtime for spawned actors.
	//virtual void OnGenerateMesh_Implementation() override;

private:
	//URealtimeMeshSimple* ProceduralMesh;
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
	TArray<bool> ForestVertices;
	UPROPERTY()
	TArray<uint8> RandomForest;

	UPROPERTY()
	UStaticMesh* ChunkStaticMesh;

	UPROPERTY(EditAnywhere)
	TArray<UFoliageData*> FoliageBiomes;

	UPROPERTY()
	bool bRaytracing = false;

	UPROPERTY()
	bool Collisions = false;

	UPROPERTY()
	UStaticMeshComponent* WaterChunk;
	
	TUniquePtr<FStaticMeshRenderData> RenderData;
	//Nanite::FResources Resources;
	//FStaticMeshLODResources* LODResource;
	//FVoxelBox Bounds;
	
	
	//FPlanetNaniteBuilder NaniteBuilder;
	
	Chaos::FTriangleMeshImplicitObjectPtr ChaosMeshData;

	UPROPERTY()
	UMaterialInstanceDynamic* MaterialInst;
	
	UPROPERTY()
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

	// core 3D -> [0,1] hash, optional salt for getting independent hashes
	static inline float Hash3DToUnit(const FVector& P, uint32_t Salt = 0u)
	{
		const uint32_t ux = FloatAsUint(P.X);
		const uint32_t uy = FloatAsUint(P.Y);
		const uint32_t uz = FloatAsUint(P.Z);

		uint32_t h = (ux * 0x9e3779b1u) ^ (uy * 0x85ebca6bu) ^ (uz * 0xc2b2ae35u) ^ Salt;
		h = fmix32(h);

		// map 0..0xFFFFFFFF -> 0.0 .. 1.0
		return static_cast<float>(h) / 4294967295.0f;
	}

	// final function requested: returns two independent components in [RangeMin, RangeMax]
	static inline FVector2D HashFVectorToVector2D(const FVector& Input, float RangeMin, float RangeMax)
	{
		// produce two independent hashes by using different salts
		const float hx = Hash3DToUnit(Input, 0x243F6A88u); // arbitrary salt (example: part of PI's fractional)
		const float hy = Hash3DToUnit(Input, 0x85A308D3u); // different arbitrary salt

		const float x = FMath::Lerp(RangeMin, RangeMax, hx);
		const float y = FMath::Lerp(RangeMin, RangeMax, hy);

		return FVector2D(x, y);
	}
};
