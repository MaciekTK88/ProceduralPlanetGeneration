// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#include "PlanetSpawner.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LevelEditorViewport.h"
#include "Engine/GameEngine.h"
#include "Engine/StaticMeshActor.h"
#include "ChunkObject.h"
#include "FoliageSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"


void FChunkTree::GenerateChunks(int32 RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* Planet, FChunkTree* ParentGeneratedChunk)
{

	bool bChildChunksReady = true;
	
	if (ChunkObject != nullptr)
	{
		TArray<FChunkTree*> ChildChunks;
		FindConfiguredChunks(ChildChunks, false);
		
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
	
	if (ChunkObject && ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::WAITING_FOR_GPU)
	{
		ChunkObject->TickGPUReadback();
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
	
	/*
	bool ShouldSubdivide = false;
	FVector ChunkVolumeOriginLocation = ChunkOriginLocation * (Planet->PlanetData->PlanetRadius + MaxChunkHeight / 2.0f);
	
	ChunkOriginLocation *= Planet->PlanetData->PlanetRadius;

	// Safety check for root chunk at planet center
	if (ChunkVolumeOriginLocation.IsNearlyZero())
	{
		ChunkVolumeOriginLocation = FVector(0, 0, 1); // Default up direction for safety
	}

	FVector ChunkNormal = ChunkVolumeOriginLocation.GetSafeNormal();

	// Vector from camera to chunk center (world-space)
	FVector CameraToChunk = ChunkOriginLocation - Planet->CharacterLocation;
	float DistToCenter = CameraToChunk.Size();
	
	// Height-aware bounding radius (approx) - compute early for proximity checks
	float ChunkRadius = LocalChunkSize * 0.70710678f; // sqrt(2)/2
	float DistToSurface = DistToCenter - ChunkRadius;

	// If camera is inside or very close to chunk bounds, always subdivide
	if (DistToCenter < KINDA_SMALL_NUMBER)
	{
		ShouldSubdivide = true;
	}
	else if (DistToSurface < LocalChunkSize * 0.5f) // Within half chunk size of bounds
	{
		// Camera is very close to or inside chunk - always subdivide
		ShouldSubdivide = true;
	}
	// Recursion limits
	else if (RecursionLevel < Planet->PlanetData->MinRecursionLevel)
	{
		ShouldSubdivide = true;
	}
	else if (RecursionLevel >= Planet->PlanetData->MaxRecursionLevel)
	{
		ShouldSubdivide = false;
	}
	else
	{
		// Horizon / Backface Culling
		FVector ViewDir = CameraToChunk / DistToCenter;
		float DotResult = FVector::DotProduct(ChunkNormal, ViewDir);
		constexpr float HorizonBias = 0.5f; // Slightly more permissive

		// Cull only if not root AND chunk is definitely facing away
		if (RecursionLevel > 0 && DotResult > HorizonBias)
		{
			ShouldSubdivide = false;
		}
		else
		{
			// Screen-Space LOD
			// GetCurrentFOV() returns DEGREES - convert to radians
			float FOVDegrees = Planet->GetCurrentFOV();
			float FOVRad = FMath::DegreesToRadians(FOVDegrees);
			
			float TanHalfFOV = FMath::Tan(FOVRad * 0.5f);
			if (TanHalfFOV < KINDA_SMALL_NUMBER)
			{
				ShouldSubdivide = false;
			}
			else
			{
				// This represents what fraction of screen height the chunk occupies
				float ScreenOccupancy =
					(LocalChunkSize / FMath::Max(DistToSurface, 1.0f)) / (2.0f * TanHalfFOV);
				
				// Subdivide if chunk takes up more than 40% of screen
				constexpr float MaxScreenUV = 0.4f;

				ShouldSubdivide = (ScreenOccupancy > MaxScreenUV);
			}
		}
	}
	*/
	
	
	FVector CharacterSpherePos = Planet->CharacterLocation.GetSafeNormal();
	// Calculate distance from character to chunk on sphere surface
	float Distance = Planet->PlanetData->PlanetRadius * FMath::Acos(FVector::DotProduct(ChunkOriginLocation, CharacterSpherePos));
	Distance = FMath::Sqrt(FMath::Pow(Distance,2) + FMath::Pow((Planet->CharacterLocation.Size() - (MaxChunkHeight + Planet->PlanetData->PlanetRadius)), 2));

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
			else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::GENERATING || 
					 ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::WAITING_FOR_GPU ||
					 ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::REMOVING)
			{
				// 1. Ensure GPU readback continues ticking so it can detect the abort flag
				ChunkObject->TickGPUReadback();

				// 2. Trigger abort if not already triggered
				if (ChunkObject->ChunkStatus != UChunkObject::EChunkStatus::REMOVING && !ChunkObject->GetAbortAsync())
				{
					ChunkObject->BeginSelfDestruct(false);
				}
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
			
			
			ChunkObject->PlanetData = Planet->PlanetData;
			ChunkObject->SetSharedResources(&Planet->ChunkSMCPool, &Planet->FoliageISMCPool, &Planet->WaterSMCPool, &Planet->Triangles);
			ChunkObject->InitializeChunk(LocalChunkSize, RecursionLevel, Planet->PlanetData->PlanetType, ChunkLocation, ChunkOriginLocation, ChunkRotation, MaxChunkHeight, Planet->MaterialLayersNum, Planet->CloseWaterMesh, Planet->FarWaterMesh);
			ChunkObject->SetFoliageActor(Planet->GetFoliageActor());
			ChunkObject->bGenerateCollisions = Planet->bGenerateCollisions;
			ChunkObject->bGenerateFoliage = Planet->bGenerateFoliage;
			ChunkObject->bGenerateRayTracingProxy = Planet->bGenerateRayTracingProxy;
			ChunkObject->CollisionDisableDistance = Planet->CollisionDisableDistance;
			ChunkObject->FoliageDensityScale = Planet->GlobalFoliageDensityScale;
		}
		else if (ChunkObject != nullptr && !ChunkObject->IsValidLowLevel())
		{
			// ChunkObject was garbage collected, clear the reference
			ChunkObject = nullptr;
		}
		else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_GENERATION && ChunkObject->GetAbortAsync() == false)
		{
			ChunkObject->GenerateChunk();
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
					if (ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::ABORTED || ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_ASSIGN || ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::PENDING_GENERATION)
					{
						ChildChunks[i]->ChunkObject->SelfDestruct();
						ChildChunks[i]->ChunkObject = nullptr;
					}
					else if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY && ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY)
					{
						ChildChunks[i]->ChunkObject->SelfDestruct();
						ChildChunks[i]->ChunkObject = nullptr;
					}
					else if (ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::GENERATING || 
					ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::WAITING_FOR_GPU ||
					ChildChunks[i]->ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::REMOVING)
					{
						ChildChunks[i]->ChunkObject->TickGPUReadback();
						
						if (ChildChunks[i]->ChunkObject->ChunkStatus != UChunkObject::EChunkStatus::REMOVING && !ChildChunks[i]->ChunkObject->GetAbortAsync())
						{
							if (ChunkObject->ChunkStatus == UChunkObject::EChunkStatus::READY)
							{
								ChildChunks[i]->ChunkObject->BeginSelfDestruct(true);
							}
							else
							{
								ChildChunks[i]->ChunkObject->BeginSelfDestruct(false);
							}
						}
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

	// Check for character
	if (Character == nullptr)
	{
		if (UGameplayStatics::GetPlayerCharacter(GetWorld(), 0) != nullptr)
		{
			Character = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
		}
		else if (UGameplayStatics::GetPlayerPawn(GetWorld(), 0) != nullptr)
		{
			Character = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		}
	}
	
	

	// Check for chunks that need to be removed
	TArray<FChunkTree*> ChunksToRemove;
	ChunkTree1.FindConfiguredChunks(ChunksToRemove, false);
	ChunkTree2.FindConfiguredChunks(ChunksToRemove, false);
	ChunkTree3.FindConfiguredChunks(ChunksToRemove, false);
	ChunkTree4.FindConfiguredChunks(ChunksToRemove, false);
	ChunkTree5.FindConfiguredChunks(ChunksToRemove, false);
	ChunkTree6.FindConfiguredChunks(ChunksToRemove, false);


	bIsLoading = true;
	
	for (int32 i = 0; i < ChunksToRemove.Num(); i++)
	{
		if (ChunksToRemove[i]->ChunkObject->ChunkStatus != UChunkObject::EChunkStatus::REMOVING)
		{
			ChunksToRemove[i]->ChunkObject->BeginSelfDestruct(true);
		}
	}
	
	if (Character == nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("No Character found! Please assign a Character or Pawn to Character variable.")));
	}
	else if (SafetyCheck())
	{
		bIsRegenerating = true;
		PrecomputeChunkData();
		bIsRegenerating = false;
		SetActorTickEnabled(true);
	}
	

	Super::BeginPlay();
}

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
	
	RegeneratePlanet(false);
}

void APlanetSpawner::RegeneratePlanet(bool bRecompileShaders = true)
{
	SetActorTickEnabled(false);

#if WITH_EDITOR
		if (GEngine && bRecompileShaders)
		{
			GEngine->Exec(NULL, TEXT("recompileshaders changed"));
		}
#endif
	
	ClearComponents();
	
	if (SafetyCheck())
	{
		bIsRegenerating = true;
		PrecomputeChunkData();
		bIsRegenerating = false;
		SetActorTickEnabled(true);
	}
}

void APlanetSpawner::ApplyShaderChanges()
{
	RegeneratePlanet(true);
}


void APlanetSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearComponents();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void APlanetSpawner::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	PreBeginPIEDelegateHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &APlanetSpawner::OnPreBeginPIE);
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddUObject(this, &APlanetSpawner::OnEndPIE);
	OnObjectPropertyChangedDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &APlanetSpawner::OnObjectPropertyChanged);
}

void APlanetSpawner::BeginDestroy()
{
	SetActorTickEnabled(false);
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEDelegateHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnObjectPropertyChangedDelegateHandle);
	
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
	SetActorTickEnabled(true);
	if (SafetyCheck())
	{
		bIsRegenerating = true;
		PrecomputeChunkData();
		bIsRegenerating = false;
	}
}

void APlanetSpawner::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
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

	if (Object == PlanetData)
	{
		bIsRegenerating = true;
		RegeneratePlanet(false);
		bIsRegenerating = false;
	}
	else if (PlanetData && PlanetData->BiomeData.Num() > 0)
	{
		// Check if it's one of our foliage data assets
		for (const FBiomeData& Biome : PlanetData->BiomeData)
		{
			if (Object == Biome.FoliageData || Object == Biome.ForestFoliageData)
			{
				bIsRegenerating = true;
				RegeneratePlanet(false);
				bIsRegenerating = false;
				return;
			}
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
	
	// Print all chunks count
	TArray<FChunkTree*> AllChunks;
	ChunkTree1.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree2.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree3.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree4.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree5.FindConfiguredChunks(AllChunks, true, false);
	ChunkTree6.FindConfiguredChunks(AllChunks, true, false);
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green, FString::Printf(TEXT("Total Chunks: %d"), AllChunks.Num()));

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
		
#if WITH_EDITOR
		if (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor)
		{
			FViewport* activeViewport = GEditor->GetActiveViewport();
			FEditorViewportClient* EditorViewClient = (activeViewport != nullptr) ? (FEditorViewportClient*)activeViewport->GetClient() : nullptr;
			if(EditorViewClient)
			{
				CharacterLocation = EditorViewClient->GetViewLocation();
			}
		}
		else
		{
			CharacterLocation = Character->GetActorLocation();
		}
#else
#endif
		
		CharacterLocation = UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), CharacterLocation);
		
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
	if (PlanetData->GPUBiomeData != nullptr)
	{
		PlanetData->GPUBiomeData->ConditionalBeginDestroy();
		PlanetData->GPUBiomeData = nullptr;
	}

	// Generate CurveAtlas from biome TerrainCurve assets
	if (PlanetData->CurveAtlas != nullptr)
	{
		PlanetData->CurveAtlas->ConditionalBeginDestroy();
		PlanetData->CurveAtlas = nullptr;
	}
	
	FlushRenderingCommands();
	

	if (PlanetData->BiomeData.Num() > 0)
	{
		// Calculate TerrainCurveIndex for each biome based on unique curves
		TArray<UCurveLinearColor*> UniqueCurves;
		for (int32 i = 0; i < PlanetData->BiomeData.Num(); i++)
		{
			FBiomeData& Biome = PlanetData->BiomeData[i];
			UE_LOG(LogTemp, Warning, TEXT("PrecomputeChunkData: Biome[%d] TerrainCurve = %s"), 
				i, Biome.TerrainCurve ? *Biome.TerrainCurve->GetName() : TEXT("NULL"));
			
			if (Biome.TerrainCurve != nullptr)
			{
				int32 CurveIndex = UniqueCurves.Find(Biome.TerrainCurve);
				if (CurveIndex == INDEX_NONE)
				{
					CurveIndex = UniqueCurves.Add(Biome.TerrainCurve);
				}
				Biome.TerrainCurveIndex = static_cast<uint8>(CurveIndex);
			}
			else
			{
				Biome.TerrainCurveIndex = 0;
			}
		}

		// Generate CurveAtlas texture
		if (UniqueCurves.Num() > 0)
		{
			int32 AtlasWidth = PlanetData->CurveAtlasWidth;
			int32 AtlasHeight = UniqueCurves.Num();

			PlanetData->CurveAtlas = UTexture2D::CreateTransient(AtlasWidth, AtlasHeight, PF_FloatRGBA);
			PlanetData->CurveAtlas->MipGenSettings = TMGS_NoMipmaps;
			PlanetData->CurveAtlas->SRGB = false;
			PlanetData->CurveAtlas->Filter = TF_Nearest;
			PlanetData->CurveAtlas->AddressX = TA_Clamp;
			PlanetData->CurveAtlas->AddressY = TA_Clamp;

			FTexture2DMipMap& CurveMipMap = PlanetData->CurveAtlas->GetPlatformData()->Mips[0];
			
			// Calculate the required size for float16 RGBA (8 bytes per pixel - 4 channels x 2 bytes each)
			int32 DataSize = AtlasWidth * AtlasHeight * 4 * sizeof(FFloat16);
			
			FByteBulkData& CurveImageData = CurveMipMap.BulkData;
			CurveImageData.Lock(LOCK_READ_WRITE);
			FFloat16* RawCurveData = (FFloat16*)CurveImageData.Realloc(DataSize);

			for (int32 CurveIdx = 0; CurveIdx < UniqueCurves.Num(); CurveIdx++)
			{
				UCurveLinearColor* Curve = UniqueCurves[CurveIdx];
				
				// Skip if curve became invalid during the process
				if (!IsValid(Curve))
				{
					continue;
				}
				
				for (int32 x = 0; x < AtlasWidth; x++)
				{
					float Time = static_cast<float>(x) / static_cast<float>(AtlasWidth - 1);
					FLinearColor Color = Curve->GetLinearColorValue(Time);
					
					int32 PixelIndex = (CurveIdx * AtlasWidth + x) * 4;
					RawCurveData[PixelIndex + 0] = FFloat16(Color.R);
					RawCurveData[PixelIndex + 1] = FFloat16(Color.G);
					RawCurveData[PixelIndex + 2] = FFloat16(Color.B);
					RawCurveData[PixelIndex + 3] = FFloat16(Color.A);
				}
			}

			CurveImageData.Unlock();
			PlanetData->CurveAtlas->UpdateResource();

			UE_LOG(LogTemp, Warning, TEXT("PrecomputeChunkData: Generated CurveAtlas %dx%d from %d unique curves (float16 format, %d bytes)"),
				AtlasWidth, AtlasHeight, UniqueCurves.Num(), DataSize);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PrecomputeChunkData: No TerrainCurve assets assigned to biomes!"));
		}

		uint8 ParameterCount = 5;

		PlanetData->GPUBiomeData = UTexture2D::CreateTransient(ParameterCount, PlanetData->BiomeData.Num(), PF_R16F);

		PlanetData->GPUBiomeData->MipGenSettings = TMGS_NoMipmaps;
		PlanetData->GPUBiomeData->SRGB = false;
		PlanetData->GPUBiomeData->Filter = TF_Nearest;

		FTexture2DMipMap& MipMap = PlanetData->GPUBiomeData->GetPlatformData()->Mips[0];
		FByteBulkData& ImageData = MipMap.BulkData;
		FFloat16* RawImageData = (FFloat16*)ImageData.Lock(LOCK_READ_WRITE);

		TArray<uint8> UniqueLayers;

		for (int32 y = 0; y < PlanetData->BiomeData.Num(); y++)
		{
			RawImageData[y * ParameterCount + 0] = PlanetData->BiomeData[y].MinTemperature;
			RawImageData[y * ParameterCount + 1] = PlanetData->BiomeData[y].MaxTemperature;
			RawImageData[y * ParameterCount + 2] = PlanetData->BiomeData[y].TerrainCurveIndex;
			RawImageData[y * ParameterCount + 3] = PlanetData->BiomeData[y].bGenerateForest;
			RawImageData[y * ParameterCount + 4] = PlanetData->BiomeData[y].MaterialLayerIndex;

			UniqueLayers.AddUnique(PlanetData->BiomeData[y].MaterialLayerIndex);
		}


		ImageData.Unlock();

		// Free CPU bulk after upload
		PlanetData->GPUBiomeData->UpdateResource();
		MipMap.BulkData.RemoveBulkData();

		MaterialLayersNum = UniqueLayers.Num();
		
		UE_LOG(LogTemp, Warning, TEXT("PrecomputeChunkData: Created Texture %dx%d. Valid: %s Resource: %s"), 
			ParameterCount, PlanetData->BiomeData.Num(), 
			PlanetData->GPUBiomeData ? TEXT("Yes") : TEXT("No"),
			(PlanetData->GPUBiomeData && PlanetData->GPUBiomeData->GetResource()) ? TEXT("Yes") : TEXT("No")
		);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PrecomputeChunkData: BiomeData is Empty!"));
	}



	Triangles.Empty();
	int32 VerticesAmount = ChunkQuality + 1;

	for (int y = 0; y < VerticesAmount - 1; y++)
	{
		for (int x = 0; x < VerticesAmount - 1; x++)
		{
			int32 v0 = x + y * VerticesAmount;
			int32 v1 = v0 + 1;
			int32 v2 = v0 + VerticesAmount;
			int32 v3 = v2 + 1;

			// Triangle 1
			Triangles.Add(v0);
			Triangles.Add(v2);
			Triangles.Add(v1);

			// Triangle 2
			Triangles.Add(v1);
			Triangles.Add(v2);
			Triangles.Add(v3);
		}
	}

	if (FoliageActor != nullptr)
	{
		// Destroy InstancedStaticMeshComponents
		TArray<UInstancedStaticMeshComponent*> ISMCs;
		FoliageActor->GetComponents<UInstancedStaticMeshComponent>(ISMCs);
		for (UInstancedStaticMeshComponent* ISMC : ISMCs)
		{
			ISMC->DestroyComponent();
		}
		ISMCs.Empty();
		
		// Destroy the actor
		FoliageActor->Destroy();
	}
	FoliageActor = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FoliageActor = GetWorld()->SpawnActor<AFoliageSpawner>(AFoliageSpawner::StaticClass(), GetActorLocation(), FRotator(0.0f, 0.0f, 0.0f), SpawnParams);
	FoliageActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	
	FlushRenderingCommands();

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



