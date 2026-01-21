// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once

#include "CoreMinimal.h"

#include "ChunkObject.h"
#include "GameFramework/Actor.h"
#include "PlanetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PlanetSpawner.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlanetGenerationFinished);

USTRUCT()
struct FChunkTree
{
	GENERATED_BODY()
	
	// Child chunks
	TSharedPtr<FChunkTree> Child1;
	TSharedPtr<FChunkTree> Child2;
	TSharedPtr<FChunkTree> Child3;
	TSharedPtr<FChunkTree> Child4;
	
	UPROPERTY(Transient)
	TObjectPtr<UChunkObject> ChunkObject = nullptr;
	
	// Height of the highest vertex in this chunk
	float MaxChunkHeight = 0;

	void GenerateChunks(int RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* Planet, FChunkTree* ParentMesh);
	
	// Traverse the tree and find chunks with non-empty ChunkObject
	void FindConfiguredChunks(TArray<FChunkTree*>& Chunks, bool IncludeSelf, bool bFindFirst = true);

	// Delete all child chunks by resetting the unique pointers
	void Reset();

	void AddReferencedObjects(FReferenceCollector& Collector);
	
	inline static int32 CompletionsThisFrame = 0;
};



UCLASS()
class PPG_API APlanetSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	APlanetSpawner();

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

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
	void RegeneratePlanet(bool bRecompileShaders);
	
	UFUNCTION(BlueprintCallable, Category = "Planet|Spawning")
	float GetCurrentFOV();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Planet|Spawning")
	void ApplyShaderChanges();


#if WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
	void OnPreBeginPIE(const bool bIsSimulating);
	void OnEndPIE(const bool bIsSimulating);
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);
	
	FDelegateHandle PreBeginPIEDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;
	FDelegateHandle OnObjectPropertyChangedDelegateHandle;
#endif

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Editor")
	bool bUseEditorTick = true;

	UPROPERTY(BlueprintAssignable, Category = "Planet|Events")
	FOnPlanetGenerationFinished OnPlanetGenerationFinished;

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
	bool bNaniteLandscape = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet|Setup")
	float GlobalFoliageDensityScale = 1.0f;

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	FCollisionResponseContainer CollisionSetup;
	
	FVector CharacterLocation;
	bool bIsLoading = true;
	bool bIsRegenerating = false;

private:
	FChunkTree ChunkTree1, ChunkTree2, ChunkTree3, ChunkTree4, ChunkTree5, ChunkTree6;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FoliageActor;
};
