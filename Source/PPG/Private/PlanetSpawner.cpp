// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#include "PlanetSpawner.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Engine/TextureRenderTarget2D.h"
//#include "RealtimeMeshSimple.h"
#include "Engine/GameEngine.h"
#include "Engine/StaticMeshActor.h"
#include "ChunkComponent.h"
#include "FoliageSpawner.h"
#include "Components/InstancedStaticMeshComponent.h"

// Sets default values
APlanetSpawner::APlanetSpawner()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

}

// Called when the game starts or when spawned
void APlanetSpawner::BeginPlay()
{
	SetActorTickEnabled(true);

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

	if (Character == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("No Character found! Please assign a Character or Pawn to Character variable.")));
	}
	
	if (SafetyCheck())
	{
		PrecomputeChunkData();
		SaveToGenerate = true;
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
	//UseEditorTick = false;
	SetActorTickEnabled(false);
	UE_LOG(LogTemp, Warning, TEXT("PIE is about to start. Clearing editor-generated chunks."));
	ClearComponents();
}

#endif

void APlanetSpawner::ClearComponents()
{
	
	//Destroy chunks
	for (UChunkComponent* Chunk : Chunks)
	{
		Chunk->ChunksToRemove.Empty();
		Chunk->AbortAsync = true;
		Chunk->FreeComponents();
		Chunk->SelfDestruct();
	}
	Chunks.Empty();

	//Destroy foliage
	if (FoliageActor != nullptr)
	{
		//Destroy InstancedStaticMeshComponents
		TArray<UInstancedStaticMeshComponent*> ISMCs;
		FoliageActor->GetComponents<UInstancedStaticMeshComponent>(ISMCs);
		for (UInstancedStaticMeshComponent* ISMC : ISMCs)
		{
			ISMC->UnregisterComponent();
			ISMC->DestroyComponent();
		}
		ISMCs.Empty();
		//Destroy the actor
		FoliageActor->Destroy();
	}
	FoliageActor = nullptr;
	FoliageISMCPool.Empty();

	//Destroy StaticMeshComponents
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


// Called every frame
void APlanetSpawner::Tick(float DeltaTime)
{
	BuildPlanet();

	Super::Tick(DeltaTime);
}

bool APlanetSpawner::ShouldTickIfViewportsOnly() const
{
	return (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor && UseEditorTick);
}

bool APlanetSpawner::SafetyCheck()
{
	if (PlanetData == nullptr)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("No Planet Data! Please assign a Planet Data asset.")));
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
	else if (AsyncInitBody == true && IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.EnableAsyncInitBody"))->GetBool() == false)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("AsyncInitBody is enabled but Chaos AsyncInitBody is disabled! Please add '[ConsoleVariables] p.Chaos.EnableAsyncInitBody = true' to DefaultEngine.ini or disable AsyncInitBody in Planet Spawner.")));
		return false;
	}
	else if (AsyncInitBody == false && IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.EnableAsyncInitBody"))->GetBool() == true)
	{
		//print error on screen
		GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Red, FString::Printf(TEXT("AsyncInitBody is disabled but Chaos AsyncInitBody is enabled! Please set '[ConsoleVariables] p.Chaos.EnableAsyncInitBody = false' in DefaultEngine.ini or enable AsyncInitBody in Planet Spawner.")));
		return false;
	}
	else
	{
		return true;
	}
	return false;
}

AActor* APlanetSpawner::getFoliageActor()
{
	return FoliageActor;
}



void APlanetSpawner::BuildPlanet()
{
	if (SaveToGenerate == false)
	{
		return;
	}
	
	if (UGameplayStatics::GetGlobalTimeDilation(GetWorld()) == 1 || GetWorld()->WorldType == EWorldType::Editor)
	{
		Chunkcount = 0;
		
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
		float chunkSize = (PlanetData->PlanetRadius * 2.0f) / FMath::Sqrt(2.0f);
		WorldLocation = FVector(0,0,0) - chunkSize / 2;

		ChunkTree1.GenerateChunks(0, FIntVector(-1.0f, 0.0f, 0.0f), WorldLocation, chunkSize, this, nullptr);
		ChunkTree2.GenerateChunks(0, FIntVector(0.0f, 0.0f, -1.0f), FVector(WorldLocation.X + chunkSize, WorldLocation.Y, WorldLocation.Z), chunkSize, this, nullptr);
		ChunkTree3.GenerateChunks(0, FIntVector(0.0f, -1.0f, 0.0f), WorldLocation, chunkSize, this, nullptr);
		ChunkTree4.GenerateChunks(0, FIntVector(0.0f, 0.0f, 1.0f), FVector(WorldLocation.X, WorldLocation.Y, WorldLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree5.GenerateChunks(0, FIntVector(0.0f, 1.0f, 0.0f), FVector(WorldLocation.X, WorldLocation.Y + chunkSize, WorldLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree6.GenerateChunks(0, FIntVector(1.0f, 0.0f, 0.0f), FVector(WorldLocation.X + chunkSize, WorldLocation.Y, WorldLocation.Z + chunkSize), chunkSize, this, nullptr);
	}
}


void FChunkTree::GenerateChunks(int RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* planet, FChunkTree* ParentGeneratedChunk)
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
	
	FVector planetSpaceVertex = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f));

	//transform planetSpaceVertex to -1, 1 range
	float OriginalChunkSize = planet->PlanetData->PlanetRadius * 2.0 / sqrt(2.0);
	planetSpaceVertex = planetSpaceVertex / (OriginalChunkSize / 2.0f);

	//apply deformation
	float deformation = 0.75;
	planetSpaceVertex.X = tan(planetSpaceVertex.X * PI * deformation / 4.0);
	planetSpaceVertex.Y = tan(planetSpaceVertex.Y * PI * deformation / 4.0);
	planetSpaceVertex.Z = tan(planetSpaceVertex.Z * PI * deformation / 4.0);
	
	planetSpaceVertex.Normalize();
	
	
	if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY)
	{
		MaxChunkHeight = ChunkMesh->ChunkMaxHeight;
	}
	else if (MaxChunkHeight < 0.01f && Child1 != nullptr && Child1->ChunkMesh != nullptr && (Child1->MaxChunkHeight > 0.01f || Child2->MaxChunkHeight > 0.01f || Child3->MaxChunkHeight > 0.01f || Child4->MaxChunkHeight > 0.01f))
	{
		MaxChunkHeight = FMath::Max(Child1->MaxChunkHeight, Child2->MaxChunkHeight, Child3->MaxChunkHeight, Child4->MaxChunkHeight);
	}
	else if (MaxChunkHeight < 0.01f && ParentGeneratedChunk != nullptr)
	{
		MaxChunkHeight = ParentGeneratedChunk->MaxChunkHeight;
	}
	

	FVector CharacterPlanetPos = UKismetMathLibrary::InverseTransformLocation(planet->GetActorTransform(), planet->CharacterLocation);
	FVector CharacterSpherePos = CharacterPlanetPos.GetSafeNormal();
	
	double Distance = planet->PlanetData->PlanetRadius * FMath::Acos(FVector::DotProduct(planetSpaceVertex, CharacterSpherePos));
	Distance = FMath::Sqrt(FMath::Pow(Distance,2) + FMath::Pow((CharacterPlanetPos.Size() - (MaxChunkHeight + planet->PlanetData->PlanetRadius)), 2));

	planetSpaceVertex *= planet->PlanetData->PlanetRadius;
	


	if ((RecursionLevel < planet->PlanetData->maxRecursionLevel && (Distance - LocalChunkSize / 2 < LocalChunkSize)) || RecursionLevel < planet->PlanetData->minRecursionLevel)
	{
		
		if (Child1 == nullptr)
		{
			Child1 = new FChunkTree();
			Child2 = new FChunkTree();
			Child3 = new FChunkTree();
			Child4 = new FChunkTree();
		}
		
		if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY && ChildChunksReady)
		{
			ParentGeneratedChunk = this;
		}
		
		if (ChunkMesh != nullptr && (ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY || ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::ABORTED) && ChildChunksReady)
		{
			planet->Chunks.RemoveSwap(ChunkMesh);
			ChunkMesh->SelfDestruct();
			ChunkMesh = nullptr;
		}
		else if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::GENERATING && ChildChunksReady)
		{
			ChunkMesh->BeginSelfDestruct();
		}
		
		Child1->GenerateChunks(RecursionLevel + 1, ChunkRotation, ChunkLocation, LocalChunkSize / 2, planet, ParentGeneratedChunk);


		FVector NewChunkLocation = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, 0.0f, 0.0f));
		Child2->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, planet, ParentGeneratedChunk);


		NewChunkLocation = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(0.0f, LocalChunkSize / 2, 0.0f));
		Child3->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, planet, ParentGeneratedChunk);


		NewChunkLocation = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f));
		Child4->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, planet, ParentGeneratedChunk);
		
	}
	else
	{
		if (ChunkMesh == nullptr)
		{
			
			UChunkComponent* Chunk = NewObject<UChunkComponent>(planet, UChunkComponent::StaticClass(), NAME_None, RF_Transient);
			planet->Chunks.Add(Chunk);
			
			
			
			Chunk->PlanetData = planet->PlanetData;
			Chunk->ChunkSMCPool = &planet->ChunkSMCPool;
			Chunk->FoliageISMCPool = &planet->FoliageISMCPool;
			Chunk->WaterSMCPool = &planet->WaterSMCPool;
			Chunk->ChunkSize = LocalChunkSize;
			Chunk->ChunkQuality = planet->ChunkQuality;
			Chunk->recursionLevel = RecursionLevel;
			Chunk->PlanetType = planet->PlanetData->PlanetType;
			Chunk->ChunkLocation = planetSpaceVertex;
			Chunk->PlanetSpaceLocation = ChunkLocation;
			Chunk->PlanetSpaceRotation = ChunkRotation;
			Chunk->Triangles = &planet->Triangles;
			Chunk->SetFoliageActor(planet->getFoliageActor());
			Chunk->ChunkMaxHeight = MaxChunkHeight;
			Chunk->GenerateCollisions = planet->GenerateCollisions;
			Chunk->GenerateFoliage = planet->GenerateFoliage;
			Chunk->GenerateRayTracingProxy = planet->GenerateRayTracingProxy;
			Chunk->CollisionDisableDistance = planet->CollisionDisableDistance;
			Chunk->FoliageDensityScale = planet->GlobalFoliageDensityScale;
			Chunk->GPUBiomeData = planet->GPUBiomeData;
			Chunk->MaterialLayersNum = planet->MaterialLayersNum;
			Chunk->CloseWaterMesh = planet->CloseWaterMesh;
			Chunk->FarWaterMesh = planet->FarWaterMesh;

			ChunkMesh = Chunk;
					
			Chunk->GenerateChunk();
		}
		else if (ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::ABORTED)
		{
			planet->Chunks.RemoveSwap(ChunkMesh);
			ChunkMesh->SelfDestruct();
			ChunkMesh = nullptr;
		}
		
		if (ChunkMesh != nullptr && ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY)
		{
			if (ChildChunks.IsEmpty())
			{
				delete Child1;
				delete Child2;
				delete Child3;
				delete Child4;

				Child1 = nullptr;
				Child2 = nullptr;
				Child3 = nullptr;
				Child4 = nullptr;
			}
			else
			{
				for (int i = 0; i < ChildChunks.Num(); i++)
				{
					if (ChildChunks[i]->ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::READY || ChildChunks[i]->ChunkMesh->ChunkStatus == UChunkComponent::EChunkStatus::ABORTED)
					{
						planet->Chunks.RemoveSwap(ChildChunks[i]->ChunkMesh);
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
	if (Child1 != nullptr)
	{
		delete Child1;
		delete Child2;
		delete Child3;
		delete Child4;
		Child1 = nullptr;
		Child2 = nullptr;
		Child3 = nullptr;
		Child4 = nullptr;
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
			RawImageData[y * ParameterCount + 3] = PlanetData->BiomeData[y].GenerateForest;
			RawImageData[y * ParameterCount + 4] = PlanetData->BiomeData[y].MaterialLayerIndex;

			UniqueLayers.AddUnique(PlanetData->BiomeData[y].MaterialLayerIndex);
		}

		ImageData.Unlock();

		// Free CPU bulk after upload
		GPUBiomeData->UpdateResource();
		MipMap.BulkData.RemoveBulkData();

		MaterialLayersNum = UniqueLayers.Num();

		// Debug
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow,
			FString::Printf(TEXT("GPU Biome Data Initialized with %d biomes."), PlanetData->BiomeData.Num()));
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
		//Destroy InstancedStaticMeshComponents
		TArray<UInstancedStaticMeshComponent*> ISMCs;
		FoliageActor->GetComponents<UInstancedStaticMeshComponent>(ISMCs);
		for (UInstancedStaticMeshComponent* ISMC : ISMCs)
		{
			ISMC->DestroyComponent();
		}
		ISMCs.Empty();
		//Destroy the actor
		FoliageActor->Destroy();
	}
	FoliageActor = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FoliageActor = GetWorld()->SpawnActor<AFoliageSpawner>(AFoliageSpawner::StaticClass(), GetActorLocation(), FRotator(0.0f, 0.0f, 0.0f), SpawnParams);
	FoliageActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

}



