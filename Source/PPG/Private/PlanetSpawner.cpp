// Fill out your copyright notice in the Description page of Project Settings.

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
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

}

// Called when the game starts or when spawned
void APlanetSpawner::BeginPlay()
{
	SetActorTickEnabled(true);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FoliageActor = GetWorld()->SpawnActor<AFoliageSpawner>(AFoliageSpawner::StaticClass(), GetActorLocation(), FRotator(0.0f, 0.0f, 0.0f), SpawnParams);
	FoliageActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

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

#if WITH_EDITOR
void APlanetSpawner::PostInitProperties()
{
	Super::PostInitProperties();
	// Bind our function to the PreBeginPIE delegate
	PreBeginPIEDelegateHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &APlanetSpawner::OnPreBeginPIE);
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddUObject(this, &APlanetSpawner::OnEndPIE);
}

void APlanetSpawner::BeginDestroy()
{
	// It's crucial to unbind the delegate when the actor is destroyed
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEDelegateHandle);
	Super::BeginDestroy();
}

void APlanetSpawner::OnPreBeginPIE(const bool bIsSimulating)
{
	InEditor = false;
	UseEditorTick = false;
	SetActorTickEnabled(false);
	UE_LOG(LogTemp, Warning, TEXT("PIE is about to start. Clearing editor-generated chunks."));
	TArray<UChunkComponent*> Chunks;
	// Get all child ChunkComponents
	GetComponents<UChunkComponent>(Chunks);
	
	for (UChunkComponent* Chunk : Chunks)
	{
		Chunk->ChunksToRemoveQueue = &ChunksToRemove;
		Chunk->SelfDestruct();
	}
	Chunks.Empty();

	//Destroy Water Chunks
	for (auto& Elem : WaterChunkTransforms)
	{
		if (Elem.Value != nullptr)
		{
			Elem.Value->UnregisterComponent();
			Elem.Value->DestroyComponent();
		}
	}
	WaterChunkTransforms.Empty();

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
	
	DestroyChunkTrees();
}

void APlanetSpawner::OnEndPIE(const bool bIsSimulating)
{
	InEditor = true;
	UE_LOG(LogTemp, Warning, TEXT("PIE has ended. You can regenerate chunks in the editor."));
}
#endif


// Called every frame
void APlanetSpawner::Tick(float DeltaTime)
{

	BuildPlanet();

	
	if (ChunksToFinish.Num() > 0)
	{
		
		for (int x = ChunksToFinish.Num() - 1; x >= 0; x--)
		{
			//int x = ChunksToFinish.Num() - 1;
			//print on screen
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("ChunksToFinish = %d"), ChunksToFinish.Num()));

			//if (ChunksToFinish[x]->AbortAsync == false)
			//{
				//ChunksToFinish[x]->SpawnMesh();
				//ChunksToFinish[x]->ReadyToRemove = true;
			//}

			
			if (ChunksToFinish[x]->ParentChunk == nullptr)
			{
				for (int i = 0; i < ChunksToFinish[x]->ChunksToRemove.Num(); i++)
				{
					if (ChunksToFinish[x]->ChunksToRemove[i] != nullptr && ChunksToFinish[x]->ChunksToRemove[i]->AbortAsync == false)
					{
						ChunksToFinish[x]->ChunksToRemove[i]->BeginSelfDestruct();
					}
				}
				ChunksToFinish[x]->ChunksToRemove.Empty();
				ChunksToFinish[x]->ReadyToRemove = true;
			}
			else
			{
				if (ChunksToFinish[x]->ParentChunk->ReadyToRemoveCounter >= 3 && ChunksToFinish[x]->ChunkRemover == true)
				{
					for (int i = 0; i < ChunksToFinish[x]->ChunksToRemove.Num(); i++)
					{
						if (ChunksToFinish[x]->ChunksToRemove[i] != nullptr && ChunksToFinish[x]->ChunksToRemove[i]->AbortAsync == false)
						{
							ChunksToFinish[x]->ChunksToRemove[i]->BeginSelfDestruct();
						}
					}
					ChunksToFinish[x]->ChunksToRemove.Empty();
					ChunksToFinish[x]->ReadyToRemove = true;
				}
				else if (ChunksToFinish[x]->ChunkRemover == false)
				{
					ChunksToFinish[x]->ParentChunk->ReadyToRemoveCounter++;
					ChunksToFinish[x]->ReadyToRemove = true;
					ChunksToFinish[x]->ChunksToRemove.Empty();
				}
			}
			
			if (ChunksToFinish[x]->ReadyToRemove == true)
			{
				
				if (Loading == true)
				{
					ChunkSpawned++;
					//GeneratedChunks.Add(ChunksToFinish[x]);
				}
			
				if (ChunksToFinish[x]->ChunkMinHeight < 0 && PlanetData->GenerateWater && ChunksToFinish[x]->AbortAsync == false)
				{
					// For a given face, define its local "no rotation" forward.
					FVector FaceDefaultForward = FVector(0, 0, 1);

					// Get the desired direction for the chunk's edge on that face.
					FVector InputDir = FVector(ChunksToFinish[x]->PlanetSpaceRotation);
					InputDir = InputDir.GetSafeNormal();

					// Compute the quaternion that rotates from the face's default forward to the input direction.
					FQuat FaceRotation = FQuat::FindBetweenNormals(FaceDefaultForward, InputDir);
					AddWaterChunk(ChunksToFinish[x], FTransform(FaceRotation, ChunksToFinish[x]->ChunkLocation, FVector(ChunksToFinish[x]->ChunkSize / 100000, ChunksToFinish[x]->ChunkSize / 100000, ChunksToFinish[x]->ChunkSize / 100000)), ChunksToFinish[x]->PlanetSpaceLocation, ChunksToFinish[x]->recursionLevel, ChunksToFinish[x]->PlanetSpaceLocation - ChunksToFinish[x]->ChunkLocation);
				}
				ChunksToFinish[x]->ready = true;
				
				ChunksToFinish.RemoveAt(x);
				//break;
			}
		}
	}
	

	if (ChunkSpawned == ChunkCounter)
	{
		bool allReady = false;
		for (int i = 0; i < GeneratedChunks.Num(); i++)
		{
			if (GeneratedChunks[i]->ready == true)
			{
				allReady = true;
			}
			else
			{
				allReady = false;
				break;
			}
		}

		if (allReady == true)
		{
			GeneratedChunks.Empty();
			ChunkSpawned = -1;
			ChunkCounter = 0;
			Loading = false;
			ready = true;
		}
	}

	if (ChunksToRemove.Num() > 0)
	{
		for (int i = ChunksToRemove.Num() - 1; i >= 0; i--)
		{
			//print on screen
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("ChunksToRemove = %d"), ChunksToRemove.Num()));
			
			if (ChunksToRemove[i]->ReadyToRemove == true)
			{
				if (PlanetData->GenerateWater)
				{
					RemoveWaterChunk(ChunksToRemove[i]);
				}
				ChunksToRemove[i]->SelfDestruct();
				ChunksToRemove.RemoveAt(i);
			}
		}
	}
	
	
	Super::Tick(DeltaTime);
}

bool APlanetSpawner::ShouldTickIfViewportsOnly() const
{
	if (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor && UseEditorTick)
	{
		return true;
	}
	else
	{
		return false;
	}
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
			FEditorViewportClient* editorViewClient = (activeViewport != nullptr) ? (FEditorViewportClient*)activeViewport->GetClient() : nullptr;
			if( editorViewClient )
			{
				CharacterLocation = editorViewClient->GetViewLocation();
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
		planetCenterWS = GetActorLocation();
		WorldLocation = FVector(0,0,0) - chunkSize / 2;

		ChunkTree1.GenerateChunks(0, FIntVector(-1.0f, 0.0f, 0.0f), WorldLocation, chunkSize, this, nullptr);
		ChunkTree2.GenerateChunks(0, FIntVector(0.0f, 0.0f, -1.0f), FVector(WorldLocation.X + chunkSize, WorldLocation.Y, WorldLocation.Z), chunkSize, this, nullptr);
		ChunkTree3.GenerateChunks(0, FIntVector(0.0f, -1.0f, 0.0f), WorldLocation, chunkSize, this, nullptr);
		ChunkTree4.GenerateChunks(0, FIntVector(0.0f, 0.0f, 1.0f), FVector(WorldLocation.X, WorldLocation.Y, WorldLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree5.GenerateChunks(0, FIntVector(0.0f, 1.0f, 0.0f), FVector(WorldLocation.X, WorldLocation.Y + chunkSize, WorldLocation.Z + chunkSize), chunkSize, this, nullptr);
		ChunkTree6.GenerateChunks(0, FIntVector(1.0f, 0.0f, 0.0f), FVector(WorldLocation.X + chunkSize, WorldLocation.Y, WorldLocation.Z + chunkSize), chunkSize, this, nullptr);
	}
}


void FChunkTree::GenerateChunks(int RecursionLevel, FIntVector ChunkRotation, FVector ChunkLocation, double LocalChunkSize, APlanetSpawner* planet, UChunkComponent* ParentMesh)
{
	if (working == false)
	{
		working = true;
		FVector planetSpaceVertex = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f));
		//FVector planetSpaceVertex = worldSpaceVertex - planet->planetCenterWS;
		planetSpaceVertex.Normalize();
		
		float MaxChunkHeight = 0;

		if (Child1 != nullptr)
		{
			TArray<UChunkComponent*> Chunks;
			AddChunksToRemove(Chunks, true);
			
			float childmaxheight = 0;
			for (int i = 0; i < Chunks.Num(); i++)
			{
				if (Chunks[i]->ChunkMaxHeight > childmaxheight)
				{
					childmaxheight = Chunks[i]->ChunkMaxHeight;
				}
			}
			//if (childmaxheight > MaxChunkHeight)
			//{
				MaxChunkHeight = childmaxheight;
			//}
		}
		else if (ParentMesh != nullptr)
        {
            MaxChunkHeight = ParentMesh->ChunkMaxHeight;
        }
		else if (ChunkMesh != nullptr)
		{
			MaxChunkHeight = ChunkMesh->ChunkMaxHeight;
		}
		

		FVector CharacterPlanetPos = UKismetMathLibrary::InverseTransformLocation(planet->GetActorTransform(), planet->CharacterLocation);
		FVector CharacterSpherePos = CharacterPlanetPos.GetSafeNormal();
		
		double Distance = planet->PlanetData->PlanetRadius * FMath::Acos(FVector::DotProduct(planetSpaceVertex, CharacterSpherePos));
		Distance = FMath::Sqrt(FMath::Pow(Distance,2) + FMath::Pow((CharacterPlanetPos.Size() - (MaxChunkHeight + planet->PlanetData->PlanetRadius)), 2));
		//print distance
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("Distance = %f"), Distance));

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

			if (ChunkMesh != nullptr)
			{
				ParentMesh = ChunkMesh;
				ChunkMesh = nullptr;
			}


			Child1->Chunkremover = false;
			Child1->GenerateChunks(RecursionLevel + 1, ChunkRotation, ChunkLocation, LocalChunkSize / 2, planet, ParentMesh);


			FVector NewChunkLocation = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, 0.0f, 0.0f));
			Child2->Chunkremover = false;
			Child2->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, planet, ParentMesh);


			NewChunkLocation = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(0.0f, LocalChunkSize / 2, 0.0f));
			Child3->Chunkremover = false;
			Child3->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, planet, ParentMesh);


			NewChunkLocation = planet->PlanetData->PlanetTransformLocation(ChunkLocation, ChunkRotation, FVector(LocalChunkSize / 2, LocalChunkSize / 2, 0.0f));
			Child4->Chunkremover = true;
			Child4->GenerateChunks(RecursionLevel + 1, ChunkRotation, NewChunkLocation, LocalChunkSize / 2, planet, ParentMesh);
			
		}
		else
		{
			if (ChunkMesh == nullptr)
			{
				//print lastchunkheight
				//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("LastChunkHeight = %f"), MaxChunkHeight));
				
				UChunkComponent* Chunk = NewObject<UChunkComponent>(planet, planet->ChunkComponentToSpawn, NAME_None, RF_Transient);
				Chunk->SetupAttachment(planet->GetRootComponent());
				Chunk->SetMobility(EComponentMobility::Movable);
				Chunk->SetupAttachment(planet->GetRootComponent());
				Chunk->SetRelativeLocation(planetSpaceVertex);

				//FRotator RChunkrotation = FRotator(ChunkRotation.X, ChunkRotation.Y, ChunkRotation.Z);




				
				//planet->AddWaterChunk(Chunk, FTransform(FaceRotation, ChunkLocation, FVector(LocalChunkSize / 10000, LocalChunkSize / 10000, LocalChunkSize / 10000)), RecursionLevel);
				
				

				//UChunkComponent* ChunkC = Cast<UChunkComponent>(Chunk);
				//print PlanetData pointer
				//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("PlanetData = %p"), planet->PlanetData));
				
				
				Chunk->PlanetData = planet->PlanetData;
				Chunk->ChunkSize = LocalChunkSize;
				Chunk->ChunkQuality = planet->ChunkQuality;
				Chunk->planetCenterWS = planet->planetCenterWS;
				Chunk->recursionLevel = RecursionLevel;
				Chunk->PlanetType = planet->PlanetData->PlanetType;
				Chunk->ChunkLocation = planetSpaceVertex;
				Chunk->PlanetSpaceLocation = ChunkLocation;
				Chunk->PlanetSpaceRotation = ChunkRotation;
				//Chunk->ChunksToUpload = &planet->ChunksToUpload;
				Chunk->ChunksToRemoveQueue = &planet->ChunksToRemove;
				Chunk->ChunksToFinish = &planet->ChunksToFinish;
				//Chunk->WaterMeshP = planet->WaterMeshInst;
				Chunk->Triangles = &planet->Triangles;
				Chunk->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
				Chunk->SetFoliageActor(planet->getFoliageActor());
				Chunk->ParentChunk = ParentMesh;
				Chunk->ChunkRemover = Chunkremover;
				Chunk->ChunkMaxHeight = MaxChunkHeight;
				Chunk->GenerateCollisions = planet->GenerateCollisions;
				Chunk->GenerateFoliage = planet->GenerateFoliage;
				Chunk->GenerateRayTracingProxy = planet->GenerateRayTracingProxy;
				Chunk->FoliageDensityScale = planet->GlobalFoliageDensityScale;
				Chunk->GPUBiomeData = planet->GPUBiomeData;

				//Chunk->EventGenerate();

				ChunkMesh = Chunk;
				if (planet->Loading == true)
				{
					planet->ChunkCounter++;
				}

				if(ParentMesh != nullptr && Chunkremover == true)
				{
					ChunkMesh->ChunksToRemove.Add(ParentMesh);
					//planet->RemoveWaterChunk(ParentMesh);
				}




				planet->Chunkcount++;
				//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("Chunkcount: %d"), planet->Chunkcount));
				//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("Character Location: %f"), Distance));


				if (Child1 != nullptr)
				{

					AddChunksToRemove(ChunkMesh->ChunksToRemove, false);


					delete Child1;
					delete Child2;
					delete Child3;
					delete Child4;

					Child1 = nullptr;
					Child2 = nullptr;
					Child3 = nullptr;
					Child4 = nullptr;
				}
				//Chunk->SetDistanceFieldMode(EDynamicMeshComponentDistanceFieldMode::NoDistanceField);
				//Chunk->SetMeshDrawPath(EDynamicMeshDrawPath::StaticDraw);
				//planet->AddInstanceComponent(Chunk);
				//Chunk->UploadFoliageData();
				//Chunk->RegisterComponent();
				Chunk->GenerateChunk();
				//Chunk->SetComponentTickEnabled(true);
			}
		}
		working = false;
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
	if (ChunkMesh != nullptr)
	{
		ChunkMesh = nullptr;
	}
	working = false;
	Chunkremover = true;
}

void APlanetSpawner::DestroyChunkTrees() {
	ChunksToFinish.Empty();
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
		uint8 ParameterCount = 6;

		GPUBiomeData = UTexture2D::CreateTransient(ParameterCount, PlanetData->BiomeData.Num(), PF_R16F);
		GPUBiomeData->MipGenSettings = TMGS_NoMipmaps;
		GPUBiomeData->SRGB = false;
		GPUBiomeData->Filter = TF_Nearest;

		FTexture2DMipMap& MipMap = GPUBiomeData->GetPlatformData()->Mips[0];
		FByteBulkData& ImageData = MipMap.BulkData;
		FFloat16* RawImageData = (FFloat16*)ImageData.Lock(LOCK_READ_WRITE);

		for (int32 y = 0; y < PlanetData->BiomeData.Num(); y++)
		{
			RawImageData[y * ParameterCount + 0] = PlanetData->BiomeData[y].MinTemperature;
			RawImageData[y * ParameterCount + 1] = PlanetData->BiomeData[y].MaxTemperature;
			RawImageData[y * ParameterCount + 2] = PlanetData->BiomeData[y].TerrainCurveIndex;
			RawImageData[y * ParameterCount + 3] = PlanetData->BiomeData[y].GenerateForest;
		}

		ImageData.Unlock();

		// Free CPU bulk after upload
		GPUBiomeData->UpdateResource();
		MipMap.BulkData.RemoveBulkData();

		// Debug
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow,
			FString::Printf(TEXT("GPU Biome Data Initialized with %d biomes."), PlanetData->BiomeData.Num()));
	}



	Triangles.Empty();
	float VerticesAmount = ChunkQuality + 1;

	for (int y = 0; y < VerticesAmount - 1; y++)
	{
		for (int x = 0; x < VerticesAmount - 1; x++)
		{
			Triangles.Add(x + 1 + y * VerticesAmount);
			Triangles.Add(x + y * VerticesAmount);
			Triangles.Add(x + VerticesAmount + y * VerticesAmount);

			Triangles.Add(x + VerticesAmount + y * VerticesAmount);
			Triangles.Add(x + VerticesAmount + 1 + y * VerticesAmount);
			Triangles.Add(x + 1 + y * VerticesAmount);


		}
	}
}

void APlanetSpawner::AddWaterChunk(UChunkComponent* Chunk, FTransform WaterTransform, FVector OriginalLocation, int RecursionLevel, FVector ChunkPositionOffset)
{
	if (PlanetData->GenerateWater)
	{
		//spawn static mesh component for water chunk
		UStaticMeshComponent* WaterMesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		WaterMesh->SetMobility(EComponentMobility::Movable);
		WaterMesh->SetupAttachment(GetRootComponent());
		WaterMesh->SetRelativeTransform(WaterTransform);
		WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WaterMesh->CastShadow = false;
		WaterMesh->SetVisibleInRayTracing(false);
		
		if (RecursionLevel != PlanetData->maxRecursionLevel)
        {
			WaterMesh->SetStaticMesh(FarWaterMesh);
        }
        else
        {
        	WaterMesh->SetStaticMesh(CloseWaterMesh);
        }

		if (RecursionLevel >= PlanetData->RecursionLevelForMaterialChange)
		{
			WaterMesh->SetMaterial(0, PlanetData->CloseWaterMaterial);
		}
		else
		{
			WaterMesh->SetMaterial(0, PlanetData->FarWaterMaterial);
		}

		UMaterialInstanceDynamic* MaterialInst = WaterMesh->CreateDynamicMaterialInstance(0, WaterMesh->GetMaterial(0));
		MaterialInst->SetTextureParameterValue("HeightMap", Chunk->RenderTarget);
		MaterialInst->SetScalarParameterValue("PlanetRadius", PlanetData->PlanetRadius);
		MaterialInst->SetVectorParameterValue("PositionOffset", UKismetMathLibrary::InverseTransformLocation(WaterTransform, ChunkPositionOffset));
		//WaterMesh->SetMaterial(0, MaterialInst);
		WaterMesh->RegisterComponent();
		//this->AddInstanceComponent(WaterMesh);

		WaterChunkTransforms.Add(Chunk, WaterMesh);
	}
}

void APlanetSpawner::RemoveWaterChunk(UChunkComponent* Chunk)
{
	if (WaterChunkTransforms.Contains(Chunk))
	{
		WaterChunkTransforms[Chunk]->UnregisterComponent();
		WaterChunkTransforms[Chunk]->DestroyComponent();
		WaterChunkTransforms.Remove(Chunk);
	}
	else
	{
		//print error
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("bbbbbb")));

	}
}

