// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#include "PlanetSpawner.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/TextureRenderTarget2D.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif
#include "Engine/GameEngine.h"
#include "Engine/StaticMeshActor.h"
#include "ChunkObject.h"
#include "FoliageSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/Material.h"


void FChunkTree::GenerateChunks(int32 RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* Planet, FChunkTree* ParentGeneratedChunk)
{

	bool bChildChunksReady = true;
	
	if (ChunkObject != nullptr)
	{
		TArray<FChunkTree*> ChildChunks;
		FindConfiguredChunks(ChildChunks, false, true);
		
		if (ChildChunks.IsEmpty())
		{
			bChildChunksReady = false;
		}
		else
		{
			for (int32 i = 0; i < ChildChunks.Num(); i++)
			{
				if (ChildChunks[i]->ChunkObject->ChunkStatus != UChunkObject::EChunkStatus::READY)
				{
					bChildChunksReady = false;
					break;
				}
			}
		}
	}
	
	FVector ChunkLocalOrigin = FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f);
	FVector ChunkOriginLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, ChunkLocalOrigin);

	// Transform ChunkOriginLocation to -1, 1 range
	float RootChunkSize = Planet->PlanetData->PlanetRadius * 2.0 / sqrt(2.0);
	ChunkOriginLocation = ChunkOriginLocation / (RootChunkSize / 2.0f);

	// Apply deformation (makes distribution on sphere more uniform)
	float deformation = 0.75;
	ChunkOriginLocation.X = tan(ChunkOriginLocation.X * PI * deformation / 4.0);
	ChunkOriginLocation.Y = tan(ChunkOriginLocation.Y * PI * deformation / 4.0);
	ChunkOriginLocation.Z = tan(ChunkOriginLocation.Z * PI * deformation / 4.0);
	
	// Normalize to make it a unit sphere
	ChunkOriginLocation.Normalize();
	
	if (ChunkObject != nullptr && (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY || ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_ASSIGN))
	{
		// If we have a ready chunk, use its max height
		MaxChunkHeight = ChunkObject->GetChunkMaxHeight();
	}
	else if (MaxChunkHeight < 0.01f && Child1 != nullptr && Child1->ChunkObject != nullptr && (Child1->MaxChunkHeight > 0.01f || Child2->MaxChunkHeight > 0.01f || Child3->MaxChunkHeight > 0.01f || Child4->MaxChunkHeight > 0.01f))
	{
		// If we don't have a ready chunk, but we have children, use their max heights
		MaxChunkHeight = FMath::Max(Child1->MaxChunkHeight, Child2->MaxChunkHeight, Child3->MaxChunkHeight, Child4->MaxChunkHeight);
	}
	else if (MaxChunkHeight < 0.01f && ParentGeneratedChunk != nullptr)
	{
		// If we don't have a ready chunk or children, use parent's max height
		MaxChunkHeight = ParentGeneratedChunk->MaxChunkHeight;
	}
	
	
	// Calculate distance from character to chunk center on the sphere.
	FVector ChunkWorldPos = ChunkOriginLocation * (Planet->PlanetData->PlanetRadius + MaxChunkHeight);
	float Distance = FVector::Dist(Planet->ViewLocation, ChunkWorldPos);

	// Scale back to planet radius
	ChunkOriginLocation *= Planet->PlanetData->PlanetRadius;
	
	bool IsWithinLODDistance = (Distance - LocalChunkSize / 2 < LocalChunkSize);
	if ((RecursionLevel < Planet->PlanetData->MaxRecursionLevel && IsWithinLODDistance) || RecursionLevel < Planet->PlanetData->MinRecursionLevel)
	{
		// If we are within LOD distance, or below min recursion level, split chunk
		
		if (Child1 == nullptr)
		{
			Child1 = MakeShared<FChunkTree>();
			Child2 = MakeShared<FChunkTree>();
			Child3 = MakeShared<FChunkTree>();
			Child4 = MakeShared<FChunkTree>();
		}
		
		FChunkTree* NewParentGeneratedChunk = ParentGeneratedChunk;
		if (ChunkObject != nullptr && ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY)
		{
			NewParentGeneratedChunk = this;
			ParentGeneratedChunk = nullptr;
		}
		
		if (ChunkObject != nullptr)
		{
			// Ready to destroy immediately
			if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::ABORTED || 
				ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_ASSIGN || 
				ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::WAITING_FOR_GPU ||
				ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_GENERATION)
			{
				ChunkObject->SelfDestruct();
				ChunkObject = nullptr;
			}
			// Parent is READY, waiting for children
			else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY && bChildChunksReady)
			{
				ChunkObject->SelfDestruct();
				ChunkObject = nullptr;
			}
			// Active work needs to be aborted
			else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::GENERATING)
			{
				ChunkObject->BeginSelfDestruct();
			}
		}
		
		// Generate Children
		Child1->GenerateChunks(RecursionLevel + 1, ChunkRotation, ChunkLocation, LocalChunkSize / 2, Planet, NewParentGeneratedChunk);

		FVector NewChunkLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, 0.0f, 0.0f));
		Child2->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, Planet, NewParentGeneratedChunk);

		NewChunkLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(0.0f, LocalChunkSize / 2, 0.0f));
		Child3->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, Planet, NewParentGeneratedChunk);

		NewChunkLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f));
		Child4->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, Planet, NewParentGeneratedChunk);
		
	}
	else
	{
		if (ChunkObject == nullptr)
		{
			
			ChunkObject = NewObject<UChunkObject>(Planet, UChunkObject::StaticClass(), NAME_None, RF_Transient);
			PendingFrames = 0;
			
			
			ChunkObject->PlanetData = Planet->PlanetData;
			ChunkObject->SetSharedResources(&Planet->ChunkSMCPool, &Planet->FoliageISMCPool, &Planet->WaterSMCPool, &Planet->Triangles);
			ChunkObject->InitializeChunk(Planet->ChunkQuality, LocalChunkSize, RecursionLevel, ChunkLocation, ChunkOriginLocation, ChunkRotation, MaxChunkHeight, Planet->MaterialLayersNum, Planet->CloseWaterMesh, Planet->FarWaterMesh);
			ChunkObject->SetFoliageActor(Planet->GetFoliageActor());
			ChunkObject->bGenerateCollisions = Planet->bGenerateCollisions;
			ChunkObject->bGenerateFoliage = Planet->bGenerateFoliage;
			ChunkObject->bGenerateRayTracingProxy = Planet->bGenerateRayTracingProxy;
			ChunkObject->bNaniteLandscape = Planet->bNaniteLandscape;
			ChunkObject->CollisionDisableDistance = Planet->CollisionDisableDistance;
			ChunkObject->FoliageDensityScale = Planet->GlobalFoliageDensityScale;
			ChunkObject->CollisionSetup = Planet->CollisionSetup;
		}
		else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_GENERATION)
		{
			// Delay generation start based on recursion level:
			// Higher recursion chunks wait more frames to prioritize closer LODs.
			// Delay = floor(RecursionLevel / 2) frames (e.g. level 2 → 1, level 10 → 5)
			int32 RequiredDelay = RecursionLevel / 2;
			if (PendingFrames >= RequiredDelay)
			{
				ChunkObject->GenerateChunk();
			}
			else
			{
				PendingFrames++;
			}
		}
		else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::ABORTED)
		{
			ChunkObject->SelfDestruct();
			ChunkObject = nullptr;
		}
		else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_ASSIGN)
		{
			if (CompletionsThisFrame < Planet->MaxChunkCompletionsPerFrame)
			{
				// Process pending chunks with rate limiting
				ChunkObject->AssignComponents();
				CompletionsThisFrame++;
			}
		}
		else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::WAITING_FOR_GPU)
		{
			ChunkObject->TickGPUReadback();
		}
		
		if (ChunkObject != nullptr)
		{
			TArray<FChunkTree*> ChildChunks;
			FindConfiguredChunks(ChildChunks, false, false);
			
			if (ChildChunks.IsEmpty())
			{
				Child1.Reset();
				Child2.Reset();
				Child3.Reset();
				Child4.Reset();
			}
			else
			{
				for (int32 i = 0; i < ChildChunks.Num(); i++)
				{
					if (ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::ABORTED || ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_ASSIGN || ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::WAITING_FOR_GPU || ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_GENERATION)
					{
						ChildChunks[i]->ChunkObject->SelfDestruct();
						ChildChunks[i]->ChunkObject = nullptr;
					}
					else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY && ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY)
					{
						ChildChunks[i]->ChunkObject->SelfDestruct();
						ChildChunks[i]->ChunkObject = nullptr;
					}
					else if (ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::GENERATING)
					{
						ChildChunks[i]->ChunkObject->BeginSelfDestruct();
					}
				}
			}
		}
	}
}

void FChunkTree::FindConfiguredChunks(TArray<FChunkTree*>& Chunks, bool IncludeSelf, bool bFindFirst)
{
	if (IncludeSelf && ChunkObject != nullptr)
	{
		Chunks.Add(this);
		
		if (bFindFirst)
		{
			return;
		}
	}
	if (Child1)
	{
		Child1->FindConfiguredChunks(Chunks, true, bFindFirst);
		Child2->FindConfiguredChunks(Chunks, true, bFindFirst);
		Child3->FindConfiguredChunks(Chunks, true, bFindFirst);
		Child4->FindConfiguredChunks(Chunks, true, bFindFirst);
	}
}
		
void FChunkTree::Reset()
{
	Child1.Reset();
	Child2.Reset();
	Child3.Reset();
	Child4.Reset();
}

void FChunkTree::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (ChunkObject)
	{
		Collector.AddReferencedObject(ChunkObject);
	}

	if (Child1) Child1->AddReferencedObjects(Collector);
	if (Child2) Child2->AddReferencedObjects(Collector);
	if (Child3) Child3->AddReferencedObjects(Collector);
	if (Child4) Child4->AddReferencedObjects(Collector);
}


/*
 * APlanetSpawner
 */


// Sets default values
APlanetSpawner::APlanetSpawner()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

// Called when the game starts or when spawned
void APlanetSpawner::BeginPlay()
{
	Super::BeginPlay();

	bIsLoading = true;
	
	
	if (SafetyCheck())
	{
		bIsRegenerating = true;
		PrecomputeChunkData();
		bIsRegenerating = false;
		SetActorTickEnabled(true);
	}
	

	Super::BeginPlay();
}


void APlanetSpawner::RegeneratePlanet()
{
	// Safety check: Don't regenerate if material is compiling
	if (PlanetData && PlanetData->GenerationMaterial)
	{
		UMaterial* BaseMat = PlanetData->GenerationMaterial->GetMaterial();
		if (BaseMat && BaseMat->IsCompilingOrHadCompileError(GMaxRHIFeatureLevel))
		{
			UE_LOG(LogTemp, Warning, TEXT("PlanetSpawner: Skipping regeneration because material is compiling."));
			return;
		}
	}

	SetActorTickEnabled(false);
	ClearComponents();
	
	if (SafetyCheck())
	{
		bIsRegenerating = true;
		PrecomputeChunkData();
		bIsRegenerating = false;
		SetActorTickEnabled(true);
	}
}


void APlanetSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearComponents();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR

void APlanetSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetActorTickEnabled(false);
	
	for (FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
	{
		if (ViewportClient->IsLevelEditorClient())
		{
			ViewportClient->SetShowStats(true);
		}
	}

	// Ensure generation material is compiled before first use
	if (PlanetData && PlanetData->GenerationMaterial)
	{
		UMaterialInterface* GenMaterial = PlanetData->GenerationMaterial;
		
		// Get the base material (in case it's a material instance)
		UMaterial* BaseMaterial = GenMaterial->GetMaterial();
		if (BaseMaterial && !BaseMaterial->IsCompilingOrHadCompileError(GMaxRHIFeatureLevel))
		{
			BaseMaterial->ForceRecompileForRendering();
			BaseMaterial->MarkPackageDirty();
		}
	}
	
	RegeneratePlanet();
}

#else

void APlanetSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

#endif

#if WITH_EDITOR

void APlanetSpawner::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	PreBeginPIEDelegateHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &APlanetSpawner::OnPreBeginPIE);
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddUObject(this, &APlanetSpawner::OnEndPIE);
	OnObjectPropertyChangedDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &APlanetSpawner::OnObjectPropertyChanged);
	OnMaterialCompilationFinishedDelegateHandle = UMaterial::OnMaterialCompilationFinished().AddUObject(this, &APlanetSpawner::OnMaterialCompilationFinished);
}

void APlanetSpawner::BeginDestroy()
{
	SetActorTickEnabled(false);
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEDelegateHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnObjectPropertyChangedDelegateHandle);
	UMaterial::OnMaterialCompilationFinished().Remove(OnMaterialCompilationFinishedDelegateHandle);
	
	ClearComponents();
	
	Super::BeginDestroy();
}

void APlanetSpawner::OnPreBeginPIE(const bool bIsSimulating)
{
	SetActorTickEnabled(false);
	UE_LOG(LogTemp, Warning, TEXT("PIE is about to start. Clearing editor-generated chunks."));
	ClearComponents();
}

void APlanetSpawner::OnEndPIE(const bool bIsSimulating)
{
	if (SafetyCheck())
	{
		bIsRegenerating = true;
		PrecomputeChunkData();
		SetActorTickEnabled(true);
		bIsRegenerating = false;
	}
}

void APlanetSpawner::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	
	for (FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
	{
		if (ViewportClient->IsLevelEditorClient())
		{
			ViewportClient->SetShowStats(true);
		}
	}
	
	if (Event.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	// Prevent infinite recursion if RegeneratePlanet modifies properties
	if (bIsRegenerating)
	{
		return;
	}

	// Filter out Transient property changes
	if (Event.Property && Event.Property->HasAnyPropertyFlags(CPF_Transient))
	{
		return;
	}

	// Check if PlanetData reference changed on the spawner itself
	if (Object == this && Event.Property)
	{
		FName PropertyName = Event.Property->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(APlanetSpawner, PlanetData))
		{
			// PlanetData asset was switched - compile the new generation material
			if (PlanetData && PlanetData->GenerationMaterial)
			{
				UMaterial* BaseMaterial = PlanetData->GenerationMaterial->GetMaterial();
				if (BaseMaterial)
				{
					UE_LOG(LogTemp, Log, TEXT("PlanetData switched - compiling generation material"));
					BaseMaterial->ForceRecompileForRendering();
					BaseMaterial->MarkPackageDirty();
				}
			}
			return;
		}
	}

	if (Object == PlanetData)
	{
		// Check if GenerationMaterial property changed
		if (Event.Property)
		{
			FName PropertyName = Event.Property->GetFName();
			if (PropertyName == GET_MEMBER_NAME_CHECKED(UPlanetData, GenerationMaterial))
			{
				// Generation material was switched - compile it
				if (PlanetData->GenerationMaterial)
				{
					UMaterial* BaseMaterial = PlanetData->GenerationMaterial->GetMaterial();
					if (BaseMaterial)
					{
						UE_LOG(LogTemp, Log, TEXT("GenerationMaterial switched - compiling material"));
						BaseMaterial->ForceRecompileForRendering();
						BaseMaterial->MarkPackageDirty();
					}
				}
				return;
			}
		}
		
		bIsRegenerating = true;
		RegeneratePlanet();
		bIsRegenerating = false;
	}
	else if (PlanetData && PlanetData->BiomeData.Num() > 0)
	{
		// Check if it's one of our foliage data assets or terrain curves
		for (const FBiomeData& Biome : PlanetData->BiomeData)
		{
			if (Object == Biome.FoliageData || Object == Biome.ForestFoliageData || Object == Biome.TerrainCurve)
			{
				bIsRegenerating = true;
				RegeneratePlanet();
				bIsRegenerating = false;
				return;
			}
		}
	}
}

void APlanetSpawner::OnMaterialCompilationFinished(UMaterialInterface* Material)
{
	// Check if the compiled material is our generation material
	if (!PlanetData || !PlanetData->GenerationMaterial || bIsRegenerating || !Material)
	{
		return;
	}

	UMaterialInterface* GenMaterial = PlanetData->GenerationMaterial;
	
	// Quick check: if the base materials are different, no relationship possible
	UMaterial* CompiledBaseMaterial = Material->GetMaterial();
	UMaterial* GenBaseMaterial = GenMaterial->GetMaterial();
	if (CompiledBaseMaterial != GenBaseMaterial)
	{
		return; // Early out - different base materials, no relationship
	}

	// Check if the material that finished compiling is the generation material (or its parent)
	UMaterialInterface* CurrentMaterial = Material;

	// Walk up the parent chain to check if this material is related to our generation material
	while (CurrentMaterial)
	{
		if (CurrentMaterial == GenMaterial)
		{
			UE_LOG(LogTemp, Log, TEXT("Generation material recompiled - regenerating planet"));
			bIsRegenerating = true;
			RegeneratePlanet();
			bIsRegenerating = false;
			return;
		}
		
		// If GenMaterial is a material instance, get its parent
		if (UMaterialInstance* MatInstance = Cast<UMaterialInstance>(CurrentMaterial))
		{
			CurrentMaterial = MatInstance->Parent;
		}
		else
		{
			break;
		}
	}

	// Also check if GenMaterial is an instance and Material is its base
	UMaterialInterface* GenCurrent = GenMaterial;
	while (GenCurrent)
	{
		if (GenCurrent == Material)
		{
			UE_LOG(LogTemp, Log, TEXT("Generation material parent recompiled - regenerating planet"));
			bIsRegenerating = true;
			RegeneratePlanet();
			bIsRegenerating = false;
			return;
		}
		
		if (UMaterialInstance* MatInstance = Cast<UMaterialInstance>(GenCurrent))
		{
			GenCurrent = MatInstance->Parent;
		}
		else
		{
			break;
		}
	}
}

#endif

void APlanetSpawner::ClearComponents()
{
	FlushRenderingCommands();
	// Destroy chunks
	TArray<FChunkTree*> Chunks;
	ChunkTree1.FindConfiguredChunks(Chunks, true, false);
	ChunkTree2.FindConfiguredChunks(Chunks, true, false);
	ChunkTree3.FindConfiguredChunks(Chunks, true, false);
	ChunkTree4.FindConfiguredChunks(Chunks, true, false);
	ChunkTree5.FindConfiguredChunks(Chunks, true, false);
	ChunkTree6.FindConfiguredChunks(Chunks, true, false);
	
	for (FChunkTree* Chunk : Chunks)
	{
		Chunk->ChunkObject->SetAbortAsync(true);
		Chunk->ChunkObject->SelfDestruct();
		Chunk->ChunkObject = nullptr;
	}
	Chunks.Empty();

	// Destroy foliage
	if (FoliageActor != nullptr)
	{
		TArray<UInstancedStaticMeshComponent*> ISMCs;
		FoliageActor->GetComponents<UInstancedStaticMeshComponent>(ISMCs);
		for (UInstancedStaticMeshComponent* ISMC : ISMCs)
		{
			ISMC->UnregisterComponent();
			ISMC->DestroyComponent();
		}
		ISMCs.Empty();
		
		FoliageActor->Destroy();
	}
	FoliageActor = nullptr;
	FoliageISMCPool.Empty();

	// Destroy Static Mesh Pools
	TArray<UStaticMeshComponent*> SMCs;
	GetComponents<UStaticMeshComponent>(SMCs);
	for (UStaticMeshComponent* SMC : SMCs)
	{
		SMC->UnregisterComponent();
		SMC->DestroyComponent();
	}
	SMCs.Empty();
	
	for (UStaticMeshComponent* SMC : ChunkSMCPool)
	{
		SMC->UnregisterComponent();
		SMC->DestroyComponent();
	}
	ChunkSMCPool.Empty();
	
	for (UStaticMeshComponent* SMC : WaterSMCPool)
	{
		if (SMC->IsValidLowLevel())
		{
			SMC->UnregisterComponent();
			SMC->DestroyComponent();
		}
	}
	WaterSMCPool.Empty();
	
	DestroyChunkTrees();
	
	// Flush all pending render commands to ensure deferred object destruction completes
	FlushRenderingCommands();
}

bool APlanetSpawner::ShouldTickIfViewportsOnly() const
{
	return (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor && bUseEditorTick);
}

// Called every frame
void APlanetSpawner::Tick(float DeltaTime)
{
	BuildPlanet();
	FChunkTree::CompletionsThisFrame = 0;

	TArray<FChunkTree*> AllChunks;
	ChunkTree1.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree2.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree3.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree4.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree5.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree6.FindConfiguredChunks(AllChunks, true, false);
	
#if WITH_EDITOR
	// Print all chunks count
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green, FString::Printf(TEXT("Total Chunks: %d"), AllChunks.Num()));
#endif

	if (bIsLoading)
	{
		bool bAllReady = true;

		if (AllChunks.Num() == 0)
		{
			bAllReady = false;
		}
		
		for (FChunkTree* CheckChunk : AllChunks)
		{
			if (CheckChunk->ChunkObject != nullptr && CheckChunk->ChunkObject->ChunkStatus != UChunkObject::EChunkStatus::READY)
			{
				bAllReady = false;
				break;
			}
		}

		if (bAllReady)
		{
			bIsLoading = false;
			OnPlanetGenerationFinished.Broadcast();
		}
	}

	Super::Tick(DeltaTime);
}

bool APlanetSpawner::SafetyCheck()
{
	GEngine->ClearOnScreenDebugMessages();
	
	float DisplayTime = 12.0f;
	
	if (PlanetData == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("No Planet Data! Please assign a Planet Data asset.")));
		return false;
	}
	else if (PlanetData->BiomeData.IsEmpty())
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("No Biomes in Planet Data! Please add at least one Biome.")));
		return false;
	}
	else if (PlanetData->PlanetMaterial == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("No Planet Material assigned in Planet Data! Please assign a Planet Material.")));
		return false;
	}
	else if (PlanetData->GenerationMaterial == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("No Generation Material assigned in Planet Data! Please assign a Generation Material with 'Used with Lidar Point Cloud' enabled.")));
		return false;
	}
	else if (PlanetData->bGenerateWater == true && (CloseWaterMesh == nullptr || FarWaterMesh == nullptr))
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("Water generation is enabled but CloseWaterMesh or FarWaterMesh is not assigned! Please assign both meshes.")));
		return false;
	}
	else if (PlanetData->bGenerateWater == true && PlanetData->WaterMaterial == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("Water generation is enabled but WaterMaterial is not assigned in Planet Data! Please assign a Water Material.")));
		return false;
	}
	else if (PlanetData->PlanetRadius <= 0)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("Planet Radius must be greater than 0! Please set a valid Planet Radius.")));
		return false;
	}
	else if (PlanetData->MinRecursionLevel < 0 || PlanetData->MaxRecursionLevel < 0 || PlanetData->MinRecursionLevel > PlanetData->MaxRecursionLevel)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("Invalid Recursion Levels! Please ensure MinRecursionLevel >= 0, MaxRecursionLevel >= 0 and MinRecursionLevel <= MaxRecursionLevel.")));
		return false;
	}
	else if (bAsyncInitBody == true && IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.EnableAsyncInitBody"))->GetBool() == false)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("AsyncInitBody is enabled but Chaos AsyncInitBody is disabled! Please add '[ConsoleVariables] p.Chaos.EnableAsyncInitBody = true' to DefaultEngine.ini or disable AsyncInitBody in Planet Spawner.")));
		return false;
	}
	else if (bAsyncInitBody == false && IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.EnableAsyncInitBody"))->GetBool() == true)
	{
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("AsyncInitBody is disabled but Chaos AsyncInitBody is enabled! Please set '[ConsoleVariables] p.Chaos.EnableAsyncInitBody = false' in DefaultEngine.ini or enable AsyncInitBody in Planet Spawner.")));
		return false;
	}
	else
	{
		// Check if any biome has TerrainCurve assigned (CurveAtlas will be generated in PrecomputeChunkData)
		bool bHasTerrainCurve = false;
		for (const FBiomeData& Biome : PlanetData->BiomeData)
		{
			if (Biome.TerrainCurve != nullptr)
			{
				bHasTerrainCurve = true;
				break;
			}
		}
		if (!bHasTerrainCurve)
		{
			//print error on screen
			GEngine->AddOnScreenDebugMessage(-1, DisplayTime, FColor::Red, FString::Printf(TEXT("No TerrainCurve assigned to any Biome! Please assign TerrainCurve to at least one Biome.")));
			return false;
		}
		else
		{
			return true;
		}
	}
	return false;
}


AActor* APlanetSpawner::GetFoliageActor()
{
	return FoliageActor;
}


void APlanetSpawner::BuildPlanet()
{
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) == 1 || GetWorld()->WorldType == EWorldType::Editor)
	{
		FVector PlayerViewLocation = FVector::ZeroVector;
		FRotator PlayerViewRotation = FRotator::ZeroRotator;
		bool bHasViewPoint = false;

		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->GetPlayerViewPoint(PlayerViewLocation, PlayerViewRotation);
			bHasViewPoint = true;
		}

#if WITH_EDITOR
		if (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor)
		{
			FViewport* activeViewport = GEditor->GetActiveViewport();
			FEditorViewportClient* EditorViewClient = (activeViewport != nullptr) ? (FEditorViewportClient*)activeViewport->GetClient() : nullptr;
			if(EditorViewClient)
			{
				ViewLocation = EditorViewClient->GetViewLocation();
			}
		}
		else
		{
			ViewLocation = bHasViewPoint ? PlayerViewLocation : FVector::ZeroVector;
		}
#else
		ViewLocation = bHasViewPoint ? PlayerViewLocation : FVector::ZeroVector;
#endif
		
		ViewLocation = UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), ViewLocation);
		
		float chunkSize = (PlanetData->PlanetRadius * 2.0f) / FMath::Sqrt(2.0f);

		// Center the planet at (0,0,0)
		FVector ChunkLocation = FVector(0,0,0) - chunkSize / 2;

		ChunkTree1.GenerateChunks(0, FIntVector(-1, 0, 0), ChunkLocation, chunkSize, this, nullptr);
		ChunkTree2.GenerateChunks(0, FIntVector(0, 0, -1), FVector(ChunkLocation.X + chunkSize, ChunkLocation.Y, ChunkLocation.Z), chunkSize, this, nullptr);
		ChunkTree3.GenerateChunks(0, FIntVector(0, -1, 0), ChunkLocation, chunkSize, this, nullptr);
		ChunkTree4.GenerateChunks(0, FIntVector(0, 0, 1), FVector(ChunkLocation.X, ChunkLocation.Y, ChunkLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree5.GenerateChunks(0, FIntVector(0, 1, 0), FVector(ChunkLocation.X, ChunkLocation.Y + chunkSize, ChunkLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree6.GenerateChunks(0, FIntVector(1, 0, 0), FVector(ChunkLocation.X + chunkSize, ChunkLocation.Y, ChunkLocation.Z + chunkSize), chunkSize, this, nullptr);
	}
}


void APlanetSpawner::DestroyChunkTrees() {
	ChunkTree1.Reset();
	ChunkTree2.Reset();
	ChunkTree3.Reset();
	ChunkTree4.Reset();
	ChunkTree5.Reset();
	ChunkTree6.Reset();
}


void APlanetSpawner::PrecomputeChunkData()
{
	//==========================================================================
	// Cleanup Existing GPU Resources
	//==========================================================================
	if (PlanetData->GPUBiomeData != nullptr)
	{
		PlanetData->GPUBiomeData->ConditionalBeginDestroy();
		PlanetData->GPUBiomeData = nullptr;
	}

	if (PlanetData->CurveAtlas != nullptr)
	{
		PlanetData->CurveAtlas->ConditionalBeginDestroy();
		PlanetData->CurveAtlas = nullptr;
	}
	
	FlushRenderingCommands();

	//==========================================================================
	// Generate Biome GPU Textures
	//==========================================================================
	if (PlanetData->BiomeData.Num() > 0)
	{
		GenerateCurveAtlas();
		GenerateGPUBiomeData();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PrecomputeChunkData: BiomeData is Empty!"));
	}

	//==========================================================================
	// Generate Triangle Index Buffer
	//==========================================================================
	Triangles.Empty();
	const int32 VerticesPerEdge = ChunkQuality + 1;

	for (int32 y = 0; y < VerticesPerEdge - 1; y++)
	{
		for (int32 x = 0; x < VerticesPerEdge - 1; x++)
		{
			const int32 V0 = x + y * VerticesPerEdge;
			const int32 V1 = V0 + 1;
			const int32 V2 = V0 + VerticesPerEdge;
			const int32 V3 = V2 + 1;

			// First triangle (V0 -> V2 -> V1)
			Triangles.Add(V0);
			Triangles.Add(V2);
			Triangles.Add(V1);

			// Second triangle (V1 -> V2 -> V3)
			Triangles.Add(V1);
			Triangles.Add(V2);
			Triangles.Add(V3);
		}
	}

	//==========================================================================
	// Initialize Foliage Actor
	//==========================================================================
	if (FoliageActor != nullptr)
	{
		TArray<UInstancedStaticMeshComponent*> ISMCs;
		FoliageActor->GetComponents<UInstancedStaticMeshComponent>(ISMCs);
		for (UInstancedStaticMeshComponent* ISMC : ISMCs)
		{
			ISMC->DestroyComponent();
		}
		FoliageActor->Destroy();
	}
	FoliageActor = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FoliageActor = GetWorld()->SpawnActor<AFoliageSpawner>(
		AFoliageSpawner::StaticClass(), 
		GetActorLocation(), 
		FRotator::ZeroRotator, 
		SpawnParams
	);
	FoliageActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	
	FlushRenderingCommands();
}

//------------------------------------------------------------------------------
// Generate CurveAtlas Texture
// Creates a texture atlas containing all unique TerrainCurve assets.
// Format: PF_A32B32G32R32F, Width = CurveAtlasWidth, Height = UniqueCurveCount
// Each row stores one curve sampled at CurveAtlasWidth points with full float precision.
//------------------------------------------------------------------------------
void APlanetSpawner::GenerateCurveAtlas()
{
	// Build list of unique curves and assign indices to biomes
	TArray<UCurveVector*> UniqueCurves;
	
	for (int32 BiomeIdx = 0; BiomeIdx < PlanetData->BiomeData.Num(); BiomeIdx++)
	{
		FBiomeData& Biome = PlanetData->BiomeData[BiomeIdx];
		
		if (Biome.TerrainCurve != nullptr)
		{
			int32 CurveIdx = UniqueCurves.Find(Biome.TerrainCurve);
			if (CurveIdx == INDEX_NONE)
			{
				CurveIdx = UniqueCurves.Add(Biome.TerrainCurve);
			}
			Biome.TerrainCurveIndex = static_cast<uint8>(CurveIdx);
		}
		else
		{
			Biome.TerrainCurveIndex = 0;
		}
	}

	if (UniqueCurves.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateCurveAtlas: No TerrainCurve assets assigned to biomes"));
		return;
	}

	// Create texture with full 32-bit float precision
	const int32 AtlasWidth = PlanetData->CurveAtlasWidth;
	const int32 AtlasHeight = UniqueCurves.Num();

	PlanetData->CurveAtlas = UTexture2D::CreateTransient(AtlasWidth, AtlasHeight, PF_A32B32G32R32F);
	PlanetData->CurveAtlas->SRGB = false;
	PlanetData->CurveAtlas->Filter = TF_Nearest;
	PlanetData->CurveAtlas->AddressX = TA_Clamp;
	PlanetData->CurveAtlas->AddressY = TA_Clamp;

	// Fill texture data (A32B32G32R32F = 4 channels x 32-bit float)
	FTexture2DMipMap& MipMap = PlanetData->CurveAtlas->GetPlatformData()->Mips[0];
	const int32 DataSize = AtlasWidth * AtlasHeight * 4 * sizeof(float);
	
	FByteBulkData& BulkData = MipMap.BulkData;
	BulkData.Lock(LOCK_READ_WRITE);
	float* TexData = (float*)BulkData.Realloc(DataSize);

	for (int32 CurveIdx = 0; CurveIdx < UniqueCurves.Num(); CurveIdx++)
	{
		UCurveVector* Curve = UniqueCurves[CurveIdx];
		if (!IsValid(Curve))
		{
			continue;
		}
		
		for (int32 x = 0; x < AtlasWidth; x++)
		{
			// Map texture coordinate [0, AtlasWidth-1] to curve input [-1, 1]
			const float Time = (static_cast<float>(x) / static_cast<float>(AtlasWidth - 1)) * 2.0f - 1.0f;
			const FVector VectorValue = Curve->GetVectorValue(Time);
			
			const int32 PixelIdx = (CurveIdx * AtlasWidth + x) * 4;
			TexData[PixelIdx + 0] = static_cast<float>(VectorValue.X);
			TexData[PixelIdx + 1] = static_cast<float>(VectorValue.Y);
			TexData[PixelIdx + 2] = static_cast<float>(VectorValue.Z);
			TexData[PixelIdx + 3] = 1.0f;
		}
	}

	BulkData.Unlock();
	PlanetData->CurveAtlas->UpdateResource();

	UE_LOG(LogTemp, Log, TEXT("GenerateCurveAtlas: Created %dx%d atlas from %d curves (32-bit float)"), 
		AtlasWidth, AtlasHeight, UniqueCurves.Num());
}

//------------------------------------------------------------------------------
// Generate GPUBiomeData Texture
// Creates a texture containing per-biome configuration data.
// Format: PF_R32_UINT, Width = ParameterCount, Height = BiomeCount
// Layout per row: [Curve][Forest][Material][MaskCount][Mask0..MaskN]
//------------------------------------------------------------------------------
void APlanetSpawner::GenerateGPUBiomeData()
{
	const int32 BiomeCount = PlanetData->BiomeData.Num();
	
	// Calculate max mask count to determine texture width
	int32 MaxMaskCount = 0;
	for (const FBiomeData& Biome : PlanetData->BiomeData)
	{
		MaxMaskCount = FMath::Max(MaxMaskCount, Biome.BiomeMaskIndices.Num());
	}

	// Texture layout: [Curve][Forest][Material][MaskCount][Mask0..MaskN]
	const uint8 FixedParams = 4;
	const uint8 ParameterCount = FixedParams + MaxMaskCount;

	// Create texture with integer format to avoid float-to-int precision issues
	PlanetData->GPUBiomeData = UTexture2D::CreateTransient(ParameterCount, BiomeCount, PF_R32_UINT);
	PlanetData->GPUBiomeData->SRGB = false;
	PlanetData->GPUBiomeData->Filter = TF_Nearest;

	// Fill texture data (R32_UINT = 1 channel x 32-bit uint per texel)
	FTexture2DMipMap& MipMap = PlanetData->GPUBiomeData->GetPlatformData()->Mips[0];
	const int32 DataSize = ParameterCount * BiomeCount * sizeof(uint32);
	FByteBulkData& BulkData = MipMap.BulkData;
	BulkData.Lock(LOCK_READ_WRITE);
	uint32* TexData = (uint32*)BulkData.Realloc(DataSize);

	for (int32 BiomeIdx = 0; BiomeIdx < BiomeCount; BiomeIdx++)
	{
		const FBiomeData& Biome = PlanetData->BiomeData[BiomeIdx];
		const int32 RowOffset = BiomeIdx * ParameterCount;
		const int32 MaskCount = Biome.BiomeMaskIndices.Num();

		// Fixed parameters (stored as exact integers)
		TexData[RowOffset + 0] = static_cast<uint32>(Biome.TerrainCurveIndex);
		TexData[RowOffset + 1] = Biome.bGenerateForest ? 1u : 0u;
		TexData[RowOffset + 2] = static_cast<uint32>(Biome.MaterialLayerIndex);
		TexData[RowOffset + 3] = static_cast<uint32>(MaskCount);

		// Mask indices (padded with zeros)
		for (int32 m = 0; m < MaxMaskCount; m++)
		{
			TexData[RowOffset + FixedParams + m] = (m < MaskCount) ? static_cast<uint32>(Biome.BiomeMaskIndices[m]) : 0u;
		}
	}

	BulkData.Unlock();
	PlanetData->GPUBiomeData->UpdateResource();
	MipMap.BulkData.RemoveBulkData(); // Free CPU memory after GPU upload

	// Calculate MaterialLayersNum (total layers in material stack)
	// Equals MaxMaterialLayerIndex + 1 since indices are 0-based
	uint8 MaxLayerIndex = 0;
	for (const FBiomeData& Biome : PlanetData->BiomeData)
	{
		MaxLayerIndex = FMath::Max(MaxLayerIndex, Biome.MaterialLayerIndex);
	}
	MaterialLayersNum = MaxLayerIndex + 1;
	
	UE_LOG(LogTemp, Log, TEXT("GenerateGPUBiomeData: Created %dx%d texture, MaterialLayersNum=%d"), 
		ParameterCount, BiomeCount, MaterialLayersNum);
}

float APlanetSpawner::GetCurrentFOV() // Returns degrees
{
	float FOVDegrees = 90.0f;

#if WITH_EDITOR
	if (GEditor)
	{
		for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
		{
			if (LevelVC && LevelVC->ViewportType == LVT_Perspective)
			{
				FOVDegrees = LevelVC->FOVAngle;
				break;
			}
		}
	}
#endif

	// Runtime (PIE, Simulate, Standalone)
	if (FOVDegrees == 90.0f && GetWorld()) // Avoid overriding good editor value
	{
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (const APlayerCameraManager* PCM = PC->PlayerCameraManager)
			{
				FOVDegrees = PCM->GetFOVAngle();
			}
		}
	}

	return FOVDegrees;
}

void APlanetSpawner::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	
	APlanetSpawner* This = CastChecked<APlanetSpawner>(InThis);
	
	This->ChunkTree1.AddReferencedObjects(Collector);
	This->ChunkTree2.AddReferencedObjects(Collector);
	This->ChunkTree3.AddReferencedObjects(Collector);
	This->ChunkTree4.AddReferencedObjects(Collector);
	This->ChunkTree5.AddReferencedObjects(Collector);
	This->ChunkTree6.AddReferencedObjects(Collector);
}



