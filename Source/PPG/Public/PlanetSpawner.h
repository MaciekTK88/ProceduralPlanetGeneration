// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once

#include "CoreMinimal.h"

#include "ChunkObject.h"
#include "GameFramework/Actor.h"
#include "PlanetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PlanetSpawner.generated.h"


USTRUCT()
struct FChunkTree
{
	GENERATED_BODY()
	
	// Child chunks
	TSharedPtr<FChunkTree> Child1;
	TSharedPtr<FChunkTree> Child2;
	TSharedPtr<FChunkTree> Child3;
	TSharedPtr<FChunkTree> Child4;
	
	UPROPERTY()
	TObjectPtr<UChunkObject> ChunkObject = nullptr;
	
	// Height of the highest vertex in this chunk
	float MaxChunkHeight = 0;

	void GenerateChunks(int RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* Planet, FChunkTree* ParentMesh);
	
	// Traverse the tree and find chunks with non-empty ChunkObject
	void FindConfiguredChunks(TArray<FChunkTree*>& Chunks, bool IncludeSelf, bool bFindFirst = true);

	// Delete all child chunks by resetting the unique pointers
	void Reset();
	
	inline static int32 CompletionsThisFrame = 0;
};



UCLASS()
class PPG_API APlanetSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	APlanetSpawner();

	UFUNCTION(BlueprintCallable, Category = "Planet|Spawning")
	void BuildPlanet();

	UFUNCTION(BlueprintCallable, Category = "Planet|Spawning")
	void PrecomputeChunkData();

	UFUNCTION(BlueprintCallable, Category = "Planet|Spawning")
	bool SafetyCheck();

	UFUNCTION(BlueprintCallable, Category = "Planet|Foliage")
	AActor* GetFoliageActor();

	UFUNCTION(BlueprintCallable, Category = "Planet|Spawning")
	void ClearComponents();
	
	UFUNCTION(BlueprintCallable, Category = "Planet|Spawning")
	float GetCurrentFOV();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

#if WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
	void OnPreBeginPIE(const bool bIsSimulating);
	
	FDelegateHandle PreBeginPIEDelegateHandle;
#endif

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Editor")
	bool bUseEditorTick = true;

	UFUNCTION(BlueprintCallable, Category = "Planet|Spawning")
	void DestroyChunkTrees();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Setup")
	TObjectPtr<AActor> Character;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Setup")
	TObjectPtr<UPlanetData> PlanetData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet|Setup")
	int32 ChunkQuality = 191;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Setup")
	bool bGenerateCollisions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Setup")
	bool bGenerateFoliage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Setup")
	bool bGenerateRayTracingProxy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Setup")
	int32 CollisionDisableDistance = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Setup")
	bool bAsyncInitBody = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Setup")
	float GlobalFoliageDensityScale = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Internal")
	TObjectPtr<UTexture2D> GPUBiomeData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Water")
	TObjectPtr<UStaticMesh> FarWaterMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Water")
	TObjectPtr<UStaticMesh> CloseWaterMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Performance")
	int32 MaxChunkCompletionsPerFrame = 2;

	UPROPERTY()
	TArray<uint32> Triangles;

	UPROPERTY(BlueprintReadOnly, Category = "Planet|Internal")
	uint8 MaterialLayersNum = 0;
	

	UPROPERTY(BlueprintReadWrite, Category = "Planet|Pools")
	TArray<TObjectPtr<UStaticMeshComponent>> ChunkSMCPool;

	UPROPERTY(BlueprintReadWrite, Category = "Planet|Pools")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> FoliageISMCPool;

	UPROPERTY(BlueprintReadWrite, Category = "Planet|Pools")
	TArray<TObjectPtr<UStaticMeshComponent>> WaterSMCPool;
	
	FVector CharacterLocation;
	bool bIsLoading = true;

private:
	FChunkTree ChunkTree1, ChunkTree2, ChunkTree3, ChunkTree4, ChunkTree5, ChunkTree6;

	UPROPERTY()
	TObjectPtr<AActor> FoliageActor;
};
