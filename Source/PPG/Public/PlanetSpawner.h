// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once

#include "CoreMinimal.h"

#include "ChunkComponent.h"
#include "GameFramework/Actor.h"
#include "PlanetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PlanetSpawner.generated.h"



struct FChunkTree
{


	TUniquePtr<FChunkTree> Child1;
	TUniquePtr<FChunkTree> Child2;
	TUniquePtr<FChunkTree> Child3;
	TUniquePtr<FChunkTree> Child4;

	UChunkComponent* ChunkMesh = nullptr;
	
	float MaxChunkHeight = 0;

	void GenerateChunks(int RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* Planet, FChunkTree* ParentMesh);
	
	void AddChunksToRemove(TArray<FChunkTree*>& AllChildChunks, bool remove)
	{
		if (remove && ChunkMesh != nullptr)
		{
			AllChildChunks.Add(this);
			return;
		}
		if (Child1)
		{
			Child1->AddChunksToRemove(AllChildChunks, true);
			Child2->AddChunksToRemove(AllChildChunks, true);
			Child3->AddChunksToRemove(AllChildChunks, true);
			Child4->AddChunksToRemove(AllChildChunks, true);
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
	AActor* GetFoliageActor();

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UInstancedStaticMeshComponent* WaterMeshInst;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UInstancedStaticMeshComponent* CloseWaterMeshInst;

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void ClearComponents();


	//TArray<AActor*> ChunkArray;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

#if WITH_EDITOR
	// Called after properties are initialized
	virtual void PostRegisterAllComponents() override;

	// Called when the actor is being destroyed
	virtual void BeginDestroy() override;

	// The function that will be called before PIE starts
	void OnPreBeginPIE(const bool bIsSimulating);
	

	// Delegate handle to allow us to unbind our function safely
	FDelegateHandle PreBeginPIEDelegateHandle;
	
#endif

public:
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUseEditorTick = true;

	UFUNCTION(BlueprintCallable, Category = "BuildChunk")
	void DestroyChunkTrees();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkSetup")
	AActor* Character;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChunkSetup")
	UPlanetData* PlanetData;

	UPROPERTY()
	int ChunkQuality = 191;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool bGenerateCollisions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool bGenerateFoliage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool bGenerateRayTracingProxy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	int CollisionDisableDistance = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	bool bAsyncInitBody = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChunkSetup")
	float GlobalFoliageDensityScale = 1.0f;

	UPROPERTY()
	UTexture2D* GPUBiomeData;

	UPROPERTY()
	TArray<UChunkComponent*> ChunkArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	UStaticMesh* FarWaterMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	UStaticMesh* CloseWaterMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	int32 MaxChunkCompletionsPerFrame = 2;

	UPROPERTY()
	TArray<uint32> Triangles;

	UPROPERTY()
	uint8 MaterialLayersNum = 0;

	UPROPERTY(BlueprintReadWrite)
	TArray<UChunkComponent*> Chunks;

	UPROPERTY(BlueprintReadWrite)
	TArray<UStaticMeshComponent*> ChunkSMCPool;

	UPROPERTY(BlueprintReadWrite)
	TArray<UInstancedStaticMeshComponent*> FoliageISMCPool;

	UPROPERTY(BlueprintReadWrite)
	TArray<UStaticMeshComponent*> WaterSMCPool;
	
	FVector CharacterLocation;
	
	bool bIsLoading = true;

	//virtual void Shutdown();

private:
	FChunkTree ChunkTree1, ChunkTree2, ChunkTree3, ChunkTree4, ChunkTree5, ChunkTree6;

	UPROPERTY()
	AActor* FoliageActor;

	UPROPERTY()
	TArray<UChunkComponent*> GeneratedChunks;

};
