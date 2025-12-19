// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#include "PlanetSpawner.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Engine/GameEngine.h"
#include "Engine/StaticMeshActor.h"
#include "ChunkComponent.h"
#include "FoliageSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"

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
	ChunkTree1.AddChunksToRemove(ChunksToRemove, false);
	ChunkTree2.AddChunksToRemove(ChunksToRemove, false);
	ChunkTree3.AddChunksToRemove(ChunksToRemove, false);
	ChunkTree4.AddChunksToRemove(ChunksToRemove, false);
	ChunkTree5.AddChunksToRemove(ChunksToRemove, false);
	ChunkTree6.AddChunksToRemove(ChunksToRemove, false);

	for (int i = 0; i < ChunksToRemove.Num(); i++)
	{
		if (ChunksToRemove[i]->ChunkMesh->ChunkStatus != UChunkComponent::EChunkStatus::REMOVING)
		{
			ChunksToRemove[i]->ChunkMesh->BeginSelfDestruct();
		}
	}
	
	if (Character == nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("No Character found! Please assign a Character or Pawn to Character variable.")));
	}
	else if (SafetyCheck())
	{
		PrecomputeChunkData();
		SetActorTickEnabled(true);
	}
	

	Super::BeginPlay();
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
}

void APlanetSpawner::BeginDestroy()
{
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEDelegateHandle);
	Super::BeginDestroy();
}

void APlanetSpawner::OnPreBeginPIE(const bool bIsSimulating)
{
	//bUseEditorTick = false;
	SetActorTickEnabled(false);
	UE_LOG(LogTemp, Warning, TEXT("PIE is about to start. Clearing editor-generated chunks."));
	ClearComponents();
}

#endif

void APlanetSpawner::ClearComponents()
{
	// Destroy chunks
	for (UChunkComponent* Chunk : Chunks)
	{
		Chunk->SetAbortAsync(true);
		Chunk->ConditionalBeginDestroy();
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
		SMC->UnregisterComponent();
		SMC->DestroyComponent();
	}
	WaterSMCPool.Empty();
	
	DestroyChunkTrees();
}

bool APlanetSpawner::ShouldTickIfViewportsOnly() const
{
	return (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor && bUseEditorTick);
}

// Called every frame
void APlanetSpawner::Tick(float DeltaTime)
{
	
	BuildPlanet();
	
	// Print Chunks number
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green, FString::Printf(TEXT("Chunks: %d"), Chunks.Num()));
	
	// Print WaterSMCPool size
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Blue, FString::Printf(TEXT("WaterSMCPool Size: %d"), WaterSMCPool.Num()));

	// Process pending chunks with rate limiting
	int32 CompletionsThisFrame = 0;
	for (UChunkComponent* Chunk : Chunks)
	{
		if (Chunk && Chunk->ChunkStatus == UChunkComponent::EChunkStatus::PENDING_ASSIGN)
		{
			if (CompletionsThisFrame < MaxChunkCompletionsPerFrame)
			{
				Chunk->AssignComponents();
				Chunk->ChunkStatus = UChunkComponent::EChunkStatus::READY;
				CompletionsThisFrame++;
			}
			else
			{
				// Reached limit for this frame
				break;
			}
		}
	}

	Super::Tick(DeltaTime);
}

bool APlanetSpawner::SafetyCheck()
{
	if (PlanetData == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("No Planet Data! Please assign a Planet Data asset.")));
		UE_LOG(LogTemp, Error, TEXT("Planet Spawner: SafetyCheck Failed - No Planet Data assigned."));
		return false;
	}
	else if (PlanetData->BiomeData.IsEmpty())
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("No Biomes in Planet Data! Please add at least one Biome.")));
		return false;
	}
	else if (PlanetData->CurveAtlas == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("No Curve Atlas in Planet Data! Please assign a Curve Atlas texture.")));
		return false;
	}
	else if (bAsyncInitBody == true && IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.EnableAsyncInitBody"))->GetBool() == false)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("AsyncInitBody is enabled but Chaos AsyncInitBody is disabled! Please add '[ConsoleVariables] p.Chaos.EnableAsyncInitBody = true' to DefaultEngine.ini or disable AsyncInitBody in Planet Spawner.")));
		return false;
	}
	else if (bAsyncInitBody == false && IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.EnableAsyncInitBody"))->GetBool() == true)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("AsyncInitBody is disabled but Chaos AsyncInitBody is enabled! Please set '[ConsoleVariables] p.Chaos.EnableAsyncInitBody = false' in DefaultEngine.ini or enable AsyncInitBody in Planet Spawner.")));
		return false;
	}
	else
	{
		return true;
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
		CharacterLocation = Character->GetActorLocation();
#endif
		
		CharacterLocation = UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), CharacterLocation);
		
		float chunkSize = (PlanetData->PlanetRadius * 2.0f) / FMath::Sqrt(2.0f);

		FVector ChunkLocation = FVector(0,0,0) - chunkSize / 2;

		ChunkTree1.GenerateChunks(0, FIntVector(-1, 0, 0), ChunkLocation, chunkSize, this, nullptr);
		ChunkTree2.GenerateChunks(0, FIntVector(0, 0, -1), FVector(ChunkLocation.X + chunkSize, ChunkLocation.Y, ChunkLocation.Z), chunkSize, this, nullptr);
		ChunkTree3.GenerateChunks(0, FIntVector(0, -1, 0), ChunkLocation, chunkSize, this, nullptr);
		ChunkTree4.GenerateChunks(0, FIntVector(0, 0, 1), FVector(ChunkLocation.X, ChunkLocation.Y, ChunkLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree5.GenerateChunks(0, FIntVector(0, 1, 0), FVector(ChunkLocation.X, ChunkLocation.Y + chunkSize, ChunkLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree6.GenerateChunks(0, FIntVector(1, 0, 0), FVector(ChunkLocation.X + chunkSize, ChunkLocation.Y, ChunkLocation.Z + chunkSize), chunkSize, this, nullptr);
	}
}


void FChunkTree::GenerateChunks(int RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* Planet, FChunkTree* ParentGeneratedChunk)
{
	TArray<FChunkTree*> ChildChunks;
	bool ChildChunksReady = true;
	
	if (ChunkMesh != nullptr)
	{
		AddChunksToRemove(ChildChunks, false);
		
		if (ChildChunks.IsEmpty())
		{
			ChildChunksReady = false;
		}
		else
		{
			for (int i = 0; i < ChildChunks.Num(); i++)
			{
				if (ChildChunks[i]->ChunkMesh->ChunkStatus != UChunkComponent::EChunkStatus::READY)
				{
					ChildChunksReady = false;
					break;
				}
			}
		}
	}
	
	FVector ChunkOriginLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f));

	// Transform planetSpaceVertex to -1, 1 range
	float OriginalChunkSize = Planet->PlanetData->PlanetRadius * 2.0 / sqrt(2.0);
	ChunkOriginLocation = ChunkOriginLocation / (OriginalChunkSize / 2.0f);

	// Apply deformation
	float deformation = 0.75;
	ChunkOriginLocation.X = tan(ChunkOriginLocation.X * PI * deformation / 4.0);
	ChunkOriginLocation.Y = tan(ChunkOriginLocation.Y * PI * deformation / 4.0);
	ChunkOriginLocation.Z = tan(ChunkOriginLocation.Z * PI * deformation / 4.0);
	
	ChunkOriginLocation.Normalize();
	
	
	if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY)
	{
		MaxChunkHeight = ChunkMesh->GetChunkMaxHeight();
	}
	else if (MaxChunkHeight < 0.01f && Child1 != nullptr && Child1->ChunkMesh != nullptr && (Child1->MaxChunkHeight > 0.01f || Child2->MaxChunkHeight > 0.01f || Child3->MaxChunkHeight > 0.01f || Child4->MaxChunkHeight > 0.01f))
	{
		MaxChunkHeight = FMath::Max(Child1->MaxChunkHeight, Child2->MaxChunkHeight, Child3->MaxChunkHeight, Child4->MaxChunkHeight);
	}
	else if (MaxChunkHeight < 0.01f && ParentGeneratedChunk != nullptr)
	{
		MaxChunkHeight = ParentGeneratedChunk->MaxChunkHeight;
	}
	
	
	FVector CharacterSpherePos = Planet->CharacterLocation.GetSafeNormal();
	
	double Distance = Planet->PlanetData->PlanetRadius * FMath::Acos(FVector::DotProduct(ChunkOriginLocation, CharacterSpherePos));
	Distance = FMath::Sqrt(FMath::Pow(Distance,2) + FMath::Pow((Planet->CharacterLocation.Size() - (MaxChunkHeight + Planet->PlanetData->PlanetRadius)), 2));

	ChunkOriginLocation *= Planet->PlanetData->PlanetRadius;
	

	if ((RecursionLevel < Planet->PlanetData->MaxRecursionLevel && (Distance - LocalChunkSize / 2 < LocalChunkSize)) || RecursionLevel < Planet->PlanetData->MinRecursionLevel)
	{
		// if (RecursionLevel < 2) 
		// {
		// 	UE_LOG(LogTemp, Display, TEXT("LOD Split: Level %d Dist %f ChunkSize %f (Thresh %f)"), RecursionLevel, Distance, LocalChunkSize, LocalChunkSize * 1.5f);
		// }
		
		if (Child1 == nullptr)
		{
			Child1 = MakeUnique<FChunkTree>();
			Child2 = MakeUnique<FChunkTree>();
			Child3 = MakeUnique<FChunkTree>();
			Child4 = MakeUnique<FChunkTree>();
		}
		
		if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY && ChildChunksReady)
		{
			ParentGeneratedChunk = this;
		}
		
		if (ChunkMesh != nullptr && (ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY || ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::ABORTED || ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::PENDING_ASSIGN) && ChildChunksReady)
		{
			Planet->Chunks.RemoveSwap(ChunkMesh);
			ChunkMesh->SelfDestruct();
			ChunkMesh = nullptr;
		}
		else if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::GENERATING && ChildChunksReady)
		{
			ChunkMesh->BeginSelfDestruct();
		}
		
		// Generate Children
		Child1->GenerateChunks(RecursionLevel + 1, ChunkRotation, ChunkLocation, LocalChunkSize / 2, Planet, ParentGeneratedChunk);

		FVector NewChunkLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, 0.0f, 0.0f));
		Child2->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, Planet, ParentGeneratedChunk);

		NewChunkLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(0.0f, LocalChunkSize / 2, 0.0f));
		Child3->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, Planet, ParentGeneratedChunk);

		NewChunkLocation = Planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f));
		Child4->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, Planet, ParentGeneratedChunk);
	}
	else
	{
		if (ChunkMesh == nullptr)
		{
			UChunkComponent* Chunk = NewObject<UChunkComponent>(Planet, UChunkComponent::StaticClass(), NAME_None, RF_Transient);
			Planet->Chunks.Add(Chunk);
			
			
			
			Chunk->PlanetData = Planet->PlanetData;
			Chunk->SetSharedResources(&Planet->ChunkSMCPool, &Planet->FoliageISMCPool, &Planet->WaterSMCPool, &Planet->Triangles);
			Chunk->InitializeChunk(LocalChunkSize, RecursionLevel, Planet->PlanetData->PlanetType, ChunkLocation, ChunkOriginLocation, ChunkRotation, MaxChunkHeight, Planet->MaterialLayersNum, Planet->CloseWaterMesh, Planet->FarWaterMesh);
			Chunk->SetFoliageActor(Planet->GetFoliageActor());
			Chunk->bGenerateCollisions = Planet->bGenerateCollisions;
			Chunk->bGenerateFoliage = Planet->bGenerateFoliage;
			Chunk->bGenerateRayTracingProxy = Planet->bGenerateRayTracingProxy;
			Chunk->CollisionDisableDistance = Planet->CollisionDisableDistance;
			Chunk->FoliageDensityScale = Planet->GlobalFoliageDensityScale;
			Chunk->GPUBiomeData = Planet->GPUBiomeData;


			ChunkMesh = Chunk;
					
			Chunk->GenerateChunk();
		}
		else if (ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::ABORTED)
		{
			Planet->Chunks.RemoveSwap(ChunkMesh);
			ChunkMesh->SelfDestruct();
			ChunkMesh = nullptr;
		}
		
		if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY)
		{
			if (ChildChunks.IsEmpty())
			{
				Child1.Reset();
				Child2.Reset();
				Child3.Reset();
				Child4.Reset();
			}
			else
			{
				for (int i = 0; i < ChildChunks.Num(); i++)
				{
					if (ChildChunks[i]->ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY || ChildChunks[i]->ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::ABORTED || ChildChunks[i]->ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::PENDING_ASSIGN)
					{
						Planet->Chunks.RemoveSwap(ChildChunks[i]->ChunkMesh);
						ChildChunks[i]->ChunkMesh->SelfDestruct();
						ChildChunks[i]->ChunkMesh = nullptr;
					}
					else if (ChildChunks[i]->ChunkMesh->ChunkStatus != UChunkComponent::EChunkStatus::REMOVING)
					{
						ChildChunks[i]->ChunkMesh->BeginSelfDestruct();
					}
				}
			}
		}
	}
}
		
void FChunkTree::Reset()
{
	Child1.Reset();
	Child2.Reset();
	Child3.Reset();
	Child4.Reset();
}

void APlanetSpawner::DestroyChunkTrees() {
	Chunks.Empty();
	ChunkTree1.Reset();
	ChunkTree2.Reset();
	ChunkTree3.Reset();
	ChunkTree4.Reset();
	ChunkTree5.Reset();
	ChunkTree6.Reset();
}

void APlanetSpawner::PrecomputeChunkData()
{
	if (GPUBiomeData != nullptr)
	{
		GPUBiomeData->ConditionalBeginDestroy();
		GPUBiomeData = nullptr;
	}


	if (PlanetData->BiomeData.Num() > 0)
	{
		uint8 ParameterCount = 5;

		GPUBiomeData = UTexture2D::CreateTransient(ParameterCount, PlanetData->BiomeData.Num(), PF_R16F);
		GPUBiomeData->MipGenSettings = TMGS_NoMipmaps;
		GPUBiomeData->SRGB = false;
		GPUBiomeData->Filter = TF_Nearest;

		FTexture2DMipMap& MipMap = GPUBiomeData->GetPlatformData()->Mips[0];
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
		GPUBiomeData->UpdateResource();
		MipMap.BulkData.RemoveBulkData();

		MaterialLayersNum = UniqueLayers.Num();
		
		UE_LOG(LogTemp, Warning, TEXT("PrecomputeChunkData: Created Texture %dx%d. Valid: %s Resource: %s"), 
			ParameterCount, PlanetData->BiomeData.Num(), 
			GPUBiomeData ? TEXT("Yes") : TEXT("No"),
			(GPUBiomeData && GPUBiomeData->GetResource()) ? TEXT("Yes") : TEXT("No")
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

}





