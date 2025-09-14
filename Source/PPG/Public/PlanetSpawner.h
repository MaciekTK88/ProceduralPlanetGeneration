// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
//#include "RealtimeMeshActor.h"
#include "ChunkComponent.h"
#include "GameFramework/Actor.h"
#include "PlanetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PlanetSpawner.generated.h"


USTRUCT()
struct FChunkTree
{
	GENERATED_BODY()

	FChunkTree* Child1 = nullptr;
	FChunkTree* Child2 = nullptr;
	FChunkTree* Child3 = nullptr;
	FChunkTree* Child4 = nullptr;

	bool Chunkremover = true;

	UChunkComponent* ChunkMesh = nullptr;

	bool working = false;

	void GenerateChunks(int RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* planet, UChunkComponent* ParentMesh);

	~FChunkTree()
	{
		if (Child1 != nullptr)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("zzzzzz")));
			delete Child1;
			delete Child2;
			delete Child3;
			delete Child4;

			Child1 = nullptr;
			Child2 = nullptr;
			Child3 = nullptr;
			Child4 = nullptr;
		}

		if (ChunkMesh != nullptr)
		{
			ChunkMesh = nullptr;
		}

	}

	void AddChunksToRemove(TArray<UChunkComponent*>& AllChildChunks, bool remove)
	{
		if (Child1 != nullptr)
		{
			Child1->AddChunksToRemove(AllChildChunks, true);
			Child2->AddChunksToRemove(AllChildChunks, true);
			Child3->AddChunksToRemove(AllChildChunks, true);
			Child4->AddChunksToRemove(AllChildChunks, true);
		}

		if (remove && ChunkMesh != nullptr)
		{
			AllChildChunks.Add(ChunkMesh);
		}
	}

	void Reset();
};



UCLASS()
class PPG_API APlanetSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APlanetSpawner();

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void BuildPlanet();

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void PrecomputeChunkData();

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	bool SafetyCheck();

	UFUNCTION(BlueprintCallable, Category = "Foliage")
	AActor* getFoliageActor();

	UFUNCTION(BlueprintCallable, Category = "Water")
	void AddWaterChunk(UChunkComponent* Chunk, FTransform WaterTransform, FVector OriginalLocation, int RecursionLevel, FVector ChunkPositionOffset);

	UFUNCTION(BlueprintCallable, Category = "Water")
	void RemoveWaterChunk(UChunkComponent* Chunk);

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UInstancedStaticMeshComponent* WaterMeshInst;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UInstancedStaticMeshComponent* CloseWaterMeshInst;


	//TArray<AActor*> ChunkArray;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

#if WITH_EDITOR
	// Called after properties are initialized
	virtual void PostInitProperties() override;

	// Called when the actor is being destroyed
	virtual void BeginDestroy() override;

	// The function that will be called before PIE starts
	void OnPreBeginPIE(const bool bIsSimulating);

	// The function that will be called when PIE ends
	void OnEndPIE(const bool bIsSimulating);

	// Delegate handle to allow us to unbind our function safely
	FDelegateHandle PreBeginPIEDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;
	
#endif

public:
	
	virtual bool ShouldTickIfViewportsOnly() const override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool UseEditorTick = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool InEditor = true;

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void DestroyChunkTrees();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkSetup")
	AActor* Character;

	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<UChunkComponent> ChunkComponentToSpawn = UChunkComponent::StaticClass();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkSetup")
	UPlanetData* PlanetData;

	UPROPERTY()
	int ChunkQuality = 191;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool GenerateCollisions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool GenerateFoliage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool GenerateRayTracingProxy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	float GlobalFoliageDensityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkSetup")
	UTexture2D* GPUBiomeData;

	UPROPERTY()
	TArray<UChunkComponent*> ChunkArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	FVector planetCenterWS;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	UStaticMesh* WaterMeshToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	UMaterialInterface* FarWaterMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	UStaticMesh* CloseWaterMeshToSpawn;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	//TArray<UChunkComponent*> ChunksToUpload;

	UPROPERTY(BlueprintReadWrite)
	TArray<UChunkComponent*> ChunksToFinish;

	UPROPERTY(BlueprintReadWrite)
    TArray<UChunkComponent*> ChunksToRemove;

	UPROPERTY(BlueprintReadWrite)
	bool SaveToGenerate = false;

	UPROPERTY()
	TArray<uint32> Triangles;

	UPROPERTY()
	TMap<UChunkComponent*, UStaticMeshComponent*> WaterChunkTransforms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool ready = false;


	FVector WorldLocation;
	TArray<UChunkComponent*> AllChildChunks;
	int Chunkcount = 0;
	FVector CharacterLocation;

	int ChunkCounter = 0;
	int ChunkSpawned = 0;
	bool Loading = true;

	//virtual void Shutdown();

private:
	FChunkTree ChunkTree1, ChunkTree2, ChunkTree3, ChunkTree4, ChunkTree5, ChunkTree6;
	
	UPROPERTY()
	AActor* FoliageActor;

	UPROPERTY()
	TArray<UChunkComponent*> GeneratedChunks;

};
