// Fill out your copyright notice in the Description page of Project Settings.

#include "ChunkComponent.h"
#include "Engine/GameEngine.h"
#include "Math/Vector.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
//#include "RealtimeMeshSimple.h"
#include "Kismet/KismetMathLibrary.h"
#include "PlanetNaniteBuilder.h"
#include "VoxelMinimal.h"
//#include "ProceduralMeshComponent.h"
#include "Async/TaskGraphInterfaces.h"
#include "PhysicsEngine/BodySetup.h"
//#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include <MeshCardBuild.h>

#include "VoxelChaosTriangleMeshCooker.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "ComputeShader/Public/PlanetComputeShader/PlanetComputeShader.h"


UChunkComponent::UChunkComponent()
{
		PrimaryComponentTick.bCanEverTick = false;
		PrimaryComponentTick.bStartWithTickEnabled = false;
}

// Called when the game starts
void UChunkComponent::BeginPlay()
{
	
	Super::BeginPlay();
	// ...
}


void UChunkComponent::GenerationComplete()
{
	if (IsInGameThread())
	{
		ChunksToFinish->Add(this);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			ChunksToFinish->Add(this);
		});
	}
	

	/*
	if (ParentChunk == nullptr)
	{
		for (int i = 0; i < ChunksToRemove.Num(); i++)
		{
			ChunksToRemove[i]->BeginSelfDestruct();
		}
		ChunksToRemove.Empty();
		ReadyToRemove = true;
	}
	else
	{
		if (ParentChunk->ReadyToRemoveCounter >= 3 && ChunkRemover == true)
		{
			for (int i = 0; i < ChunksToRemove.Num(); i++)
			{
				ChunksToRemove[i]->BeginSelfDestruct();
			}
			ChunksToRemove.Empty();

			ReadyToRemove = true;
		}
		else if (ChunkRemover == false)
		{
			ParentChunk->ReadyToRemoveCounter++;
			ReadyToRemove = true;
		}
	}
	ready = true;
	*/
}


void UChunkComponent::SetFoliageActor(AActor* NewFoliageActor)
{
	FoliageActor = NewFoliageActor;
}

void UChunkComponent::UploadFoliageData()
{
	//print foliage actor name
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("FoliageActor = %s"), *FoliageActor->GetName()));
	

	for (int i = 0; i < FoliageRuntimeData.Num(); i++)
	{
		if (GenerateFoliage == true && FoliageRuntimeData[i].LocalFoliageTransforms.Num() > 0)
		{
			UInstancedStaticMeshComponent* hismc;
			hismc = NewObject<UInstancedStaticMeshComponent>(FoliageActor, NAME_None, RF_Transient);
			hismc->SetupAttachment(FoliageActor->GetRootComponent());
			hismc->SetFlags(RF_Transactional);
			hismc->SetMobility(EComponentMobility::Movable);
			hismc->SetGenerateOverlapEvents(false);
			hismc->WorldPositionOffsetDisableDistance = FoliageRuntimeData[i].Foliage.WPODisableDistance;
			hismc->SetEvaluateWorldPositionOffset(FoliageRuntimeData[i].Foliage.WPO);
			hismc->bAffectDistanceFieldLighting = false;
			hismc->InstanceEndCullDistance = FoliageRuntimeData[i].Foliage.CullingDistance;
			hismc->bWorldPositionOffsetWritesVelocity = FoliageRuntimeData[i].Foliage.WPO;
			hismc->bDisableCollision = true;
			hismc->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
			hismc->SetCollisionProfileName(TEXT("Custom"));
			hismc->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
			hismc->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel2, ECollisionResponse::ECR_Block);
			//hismc->SetVisibility(false);

			if (FoliageRuntimeData[i].Foliage.ShadowDisableDistance >= recursionLevel && FoliageRuntimeData[i].Foliage.ShadowDisableDistance != 0)
			{
				hismc->CastShadow = false;
				hismc->bCastContactShadow = false;
				hismc->SetVisibleInRayTracing(false);
			}
			
			if (FoliageRuntimeData[i].Foliage.CollisionEnableDistance <= recursionLevel && FoliageRuntimeData[i].Foliage.CollisionEnableDistance != 0 && FoliageRuntimeData[i].Foliage.Collision == true)
			{
				hismc->bDisableCollision = false;
				hismc->bCastContactShadow = false;
				//hismc->bUseDefaultCollision = true;
			}

			if (FoliageRuntimeData[i].Foliage.LowPolyActivation >= recursionLevel)
			{
				hismc->SetStaticMesh(FoliageRuntimeData[i].Foliage.LowPolyMesh);
				//hismc->CastShadow = false;
				hismc->SetEvaluateWorldPositionOffset(FoliageRuntimeData[i].Foliage.FarFoliageWPO);
				//hismc->bCastContactShadow = true;
				//hismc->WorldPositionOffsetDisableDistance = 0;
				hismc->bAffectDistanceFieldLighting = false;
				hismc->SetVisibleInRayTracing(false);

			}
			else
			{
				hismc->SetStaticMesh(FoliageRuntimeData[i].Foliage.FoliageMesh);
				hismc->bDisableCollision = not FoliageRuntimeData[i].Foliage.Collision;
			}

			//hismc->bIsAsyncBuilding = true;
			hismc->SetRelativeTransform(FTransform(GetRelativeRotation(), ChunkLocation, FVector(1.0f, 1.0f, 1.0f)));
			hismc->RegisterComponent();
			//FoliageActor->AddInstanceComponent(hismc);
			FoliageRuntimeData[i].hismc = hismc;
			
			FoliageRuntimeData[i].hismc->AddInstances(FoliageRuntimeData[i].LocalFoliageTransforms, false, false, false);
			//PlanetData->FoliageList[i].hismc->SetActive(false);
	
		}
	}
}


void UChunkComponent::GenerateChunk()
{
	//clear all arrays
	Vertices.Empty();
	//Triangles.Empty();
	Normals.Empty();
	Octahedrons.Empty();
	UVs.Empty();
	VertexColors.Empty();
	VertexHeight.Empty();

	//Calculate veritces amount and size based on chunk size and quality
	VerticesAmount = ChunkQuality + 1;
	Size = ChunkSize / ChunkQuality;

	//RenderData = MakeUnique<FStaticMeshRenderData>();

	

	if (GenerateRayTracingProxy && recursionLevel >= PlanetData->maxRecursionLevel - 1)
	{
		bRaytracing = true;
	}
	if (GenerateCollisions && recursionLevel >= PlanetData->maxRecursionLevel - 3)
	{
		Collisions = true;
	}

	
	//create PF_G16 UTextureRenderTarget2D
	RenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	RenderTarget->bCanCreateUAV = true;
	RenderTarget->bSupportsUAV = true;
	RenderTarget->InitCustomFormat(VerticesAmount, VerticesAmount * 2, PF_R8G8B8A8, true);
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->SRGB = false;
	RenderTarget->Filter = TF_Nearest;
	RenderTarget->AddressX = TA_Clamp;
	RenderTarget->AddressY = TA_Clamp;
	//RenderTarget->ClearColor = FLinearColor::Black; // Optional: Set default clear color
	RenderTarget->UpdateResourceImmediate(); // Must be called after setting UAV flags

	
	// Create a dispatch parameters struct and fill it the input array with our args
	FPlanetComputeShaderDispatchParams Params(VerticesAmount, VerticesAmount, 1);
	Params.Input[0] = FVector3f(PlanetSpaceLocation);
	Params.Input[1] = FVector3f(PlanetSpaceRotation.X, PlanetSpaceRotation.Y, PlanetSpaceRotation.Z);
	Params.Input[2] = FVector3f(ChunkLocation);
	Params.Input[3] = FVector3f(ChunkSize, PlanetData->PlanetRadius, PlanetData->noiseHeight);
	Params.InputRenderTarget = RenderTarget;
	Params.CurveAtlas = PlanetData->CurveAtlas;
	Params.BiomeDataTexture = GPUBiomeData;
	Params.BiomeCount = PlanetData->BiomeData.Num();
	Params.ChunkQuality = ChunkQuality;
	Params.X = VerticesAmount;
	Params.Y = VerticesAmount;
	Params.Z = 1;
	Params.PlanetType = PlanetType;

	Vertices.Reserve(VerticesAmount * VerticesAmount);
	UVs.Reserve(VerticesAmount * VerticesAmount);
	VertexColors.Reserve(VerticesAmount * VerticesAmount);
	VertexHeight.Reserve(VerticesAmount * VerticesAmount);
	Slopes.Reserve(VerticesAmount * VerticesAmount);
	Biomes.Reserve(VerticesAmount * VerticesAmount);
	
	FPlanetComputeShaderInterface::Dispatch(Params, [this](TArray<float> OutputVal, TArray<uint8> OutputVCVal) {
		//this->Completed.Broadcast(OutputVal);
		//print OutputVal
		//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("OutputVal = %f"), OutputVal[1]));
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, OutputVal, OutputVCVal]()
		{
			if (AbortAsync == true)
			{
				GenerationComplete();
				return;
			}

			float LocalChunkMaxHeight = 0;

			for (int y = 0; y < VerticesAmount; y++)
			{
				for (int x = 0; x < VerticesAmount; x++)
				{
					// add noise to vertex
					FVector3f Vertex = FVector3f(OutputVal[(x + y * VerticesAmount) * 3], OutputVal[(x + y * VerticesAmount) * 3 + 1], OutputVal[(x + y * VerticesAmount) * 3 + 2]);
					Vertices.Add(Vertex);
				
					UVs.Add(FVector2f((float)x / (float)VerticesAmount, (float)y / (float)VerticesAmount));

					float noise = ((FVector(Vertices[x + y * VerticesAmount]) + ChunkLocation).Size() - PlanetData->PlanetRadius) / PlanetData->noiseHeight;
				
					float noiseheight = noise * PlanetData->noiseHeight;
					if (noiseheight > LocalChunkMaxHeight)
					{
						LocalChunkMaxHeight = noiseheight;
					}

					if (noiseheight < ChunkMinHeight)
					{
						ChunkMinHeight = noiseheight;
					}


					//uint8 Temperature = OutputVCVal[(x + y * VerticesAmount) * 3 + 1];
					uint8 BiomeIndex = OutputVCVal[(x + y * VerticesAmount) * 3 + 2];
					uint8 ShaderForestNoise = OutputVCVal[(x + y * VerticesAmount) * 3 + 1];
					uint8 ForestTextureIndex = 255;
					//FVector PlanetSpaceVertex = PlanetData->PlanetTransformLocation(ChunkLocation, PlanetSpaceRotation,FVector(Vertex));
					//uint8 CurrentRandomForest = uint8(((FMath::PerlinNoise3D((FVector(Vertex) + ChunkLocation) * 0.0002) + 1) / 2) * 180);//HashFVectorToVector2D(FVector(PlanetSpaceVertex),0,120).X);
					//RandomForest.Add(CurrentRandomForest);

					if ((ShaderForestNoise <= 0.5 || noiseheight > PlanetData->BiomeData[BiomeIndex].ForestHeight) && PlanetData->BiomeData[BiomeIndex].FoliageData != nullptr)
					{
						FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].FoliageData);
						ShaderForestNoise = 0;
					}
					else if (PlanetData->BiomeData[BiomeIndex].ForestFoliageData != nullptr)
					{
						FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].ForestFoliageData);
						ForestTextureIndex = PlanetData->BiomeData[BiomeIndex].ForestTextureIndex;
					}
				
					//uint8 height = FMath::Clamp((int)((noise + 1.0) * 127.5 * 0.7), 0, 255);
					VertexColors.Add(FColor(PlanetData->BiomeData[BiomeIndex].SlopeTextureIndex, OutputVCVal[(x + y * VerticesAmount) * 3], ForestTextureIndex, PlanetData->BiomeData[BiomeIndex].GroundTextureIndex));
					//VertexColors.Add(FColor(0, 0, 0, 255));

					VertexHeight.Add(noiseheight);
					Slopes.Add(0);
					Biomes.Add(BiomeIndex);
					ForestNoise.Add(ShaderForestNoise);
				

					//FVector3f normal = (Vertices[x + y * VerticesAmount] + FVector3f(ChunkLocation)).GetSafeNormal();
					//Normals.Add(normal);
					//Octahedrons.Add(FVoxelOctahedron(normal));
				}
			}
			ChunkMaxHeight = LocalChunkMaxHeight;
			CompleteChunkGeneration();
		});
	});
}
void UChunkComponent::CompleteChunkGeneration()
{
	for (int y = 0; y < VerticesAmount; y++)
	{
		if (AbortAsync == true)
		{
			GenerationComplete();
			return;
		}
		
		if (y == VerticesAmount - 1)
		{
			for (int x = 0; x < VerticesAmount - 1; x++)
			{
				FVector3f v1 = Vertices[x - VerticesAmount + y * VerticesAmount];
				FVector3f v2 = Vertices[x + 1 + y * VerticesAmount];
				FVector3f v3 = Vertices[x + y * VerticesAmount];
				FVector3f normal = FVector3f::CrossProduct(v3 - v2, v1 - v3).GetSafeNormal();

				Normals.Add(normal);
				Octahedrons.Add(FVoxelOctahedron(normal));

				float c1 = VertexHeight[x - VerticesAmount + y * VerticesAmount];
				float c2 = VertexHeight[x + 1 + y * VerticesAmount];
				float c3 = VertexHeight[x + y * VerticesAmount];

				float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
				Slopes[x + y * VerticesAmount] = slope;

			}
			int x = VerticesAmount - 1;
			FVector3f v1 = Vertices[x - VerticesAmount + y * VerticesAmount];
			FVector3f v2 = Vertices[x - 1 + y * VerticesAmount];
			FVector3f v3 = Vertices[x + y * VerticesAmount];
			FVector3f normal = FVector3f::CrossProduct(v2 - v3, v1 - v3).GetSafeNormal();

			Normals.Add(normal);
			Octahedrons.Add(FVoxelOctahedron(normal));


			float c1 = VertexHeight[x - VerticesAmount + y * VerticesAmount];
			float c2 = VertexHeight[x - 1 + y * VerticesAmount];
			float c3 = VertexHeight[x + y * VerticesAmount];

			float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
			Slopes[x + y * VerticesAmount] = slope;
		}
		else
		{
			for (int x = 0; x < VerticesAmount - 1; x++)
			{

				FVector3f v1 = Vertices[x + VerticesAmount + y * VerticesAmount];
				FVector3f v2 = Vertices[x + 1 + y * VerticesAmount];
				FVector3f v3 = Vertices[x + y * VerticesAmount];
				FVector3f normal = FVector3f::CrossProduct(v3 - v2, v3 - v1).GetSafeNormal();

				Normals.Add(normal);
				Octahedrons.Add(FVoxelOctahedron(normal));


				float c1 = VertexHeight[x + VerticesAmount + y * VerticesAmount];
				float c2 = VertexHeight[x + 1 + y * VerticesAmount];
				float c3 = VertexHeight[x + y * VerticesAmount];

				float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
				Slopes[x + y * VerticesAmount] = slope;
			}
			int x = VerticesAmount - 1;
			FVector3f v1 = Vertices[x + VerticesAmount + y * VerticesAmount];
			FVector3f v2 = Vertices[x - 1 + y * VerticesAmount];
			FVector3f v3 = Vertices[x + y * VerticesAmount];
			FVector3f normal = FVector3f::CrossProduct(v2 - v3, v3 - v1).GetSafeNormal();

			Normals.Add(normal);
			Octahedrons.Add(FVoxelOctahedron(normal));


			float c1 = VertexHeight[x + VerticesAmount + y * VerticesAmount];
			float c2 = VertexHeight[x - 1 + y * VerticesAmount];
			float c3 = VertexHeight[x + y * VerticesAmount];

			float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
			Slopes[x + y * VerticesAmount] = slope;
		}
	}


		if (GenerateFoliage)
		{
			float minchunkSize = (PlanetData->PlanetRadius * 2.0f) / FMath::Sqrt(2.0f) / FMath::Pow(2.0f, PlanetData->maxRecursionLevel);
			//DataGuard.Lock();
			for (int x = 0; x < FoliageBiomes.Num(); x++)
			{

				for (int i = 0; i < FoliageBiomes[x]->FoliageList.Num(); i++)
				{
					if (AbortAsync == true)
					{
						GenerationComplete();
						return;
					}
				
					if (FoliageBiomes[x]->FoliageList[i].FoliageDistance <= recursionLevel)
					{
						FFoliageRuntimeDataS Biome;
						Biome.Foliage = FoliageBiomes[x]->FoliageList[i];
						int index = FoliageRuntimeData.Add(Biome);

						//FoliageRuntimeData[i].LocalFoliageTransforms.Empty();
						//FRandomStream Stream((i + 2) * (x + 3));
				

						int Density = Biome.Foliage.FoliageDensity;
						

						//if (Biome.Foliage.LowPolyActivation < recursionLevel)
						//{
							//Density *= ChunkSize / 100000;
						//}
					

						if (Biome.Foliage.ScalableDensity == true)
						{
							Density = Density * FoliageDensityScale;
						}

						if (Biome.Foliage.FarFoliageDensityBoost == true && Biome.Foliage.LowPolyActivation >= recursionLevel)
						{
							Density = Density * FarFoliageDensityScale;
						}

						//LocalFoliageTransform.Empty();
						int localdensity = Density * FMath::Pow(2, PlanetData->maxRecursionLevel - recursionLevel);
						
						for (int y = 0; y < localdensity; y++)
						{
							for (int z = 0; z < localdensity; z++)
							{
							
								//FVector2f ChunkUVpos = PlanetData->InversePlanetTransformLocation(PlanetSpaceRotation, PlanetSpaceLocation);
								float xlocalpos = y * minchunkSize / (float)(Density);
								float ylocalpos = z * minchunkSize / (float)(Density);
								
							
								FVector planetSpacefoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));
								FVector NormalizedfoliageLocation = planetSpacefoliageLocation.GetSafeNormal() * PlanetData->PlanetRadius;

								//uint32 SeedValue = FCrc::MemCrc32(&NormalizedfoliageLocation, sizeof(FVector));
								FRandomStream ScaleStream((int)(NormalizedfoliageLocation.X / 10000) + (int)(NormalizedfoliageLocation.Y / 10000) + (int)(NormalizedfoliageLocation.Z / 10000));

								NormalizedfoliageLocation = FVector(
								FMath::RoundToFloat(NormalizedfoliageLocation.X / 10),
								FMath::RoundToFloat(NormalizedfoliageLocation.Y / 10),
								FMath::RoundToFloat(NormalizedfoliageLocation.Z / 10)
								);
								

								FVector2d NewLocalPos = HashFVectorToVector2D(NormalizedfoliageLocation + x * 100 + i * 100, 0.0f, minchunkSize);
								NewLocalPos += FVector2d((int)((xlocalpos / minchunkSize)) * minchunkSize, (int)((ylocalpos / minchunkSize)) * minchunkSize);
								
								
								xlocalpos = NewLocalPos.X;
								ylocalpos = NewLocalPos.Y;

								int vertexX = FMath::RoundToInt(xlocalpos / ChunkSize * float(ChunkQuality));
								int vertexY = FMath::RoundToInt(ylocalpos / ChunkSize * float(ChunkQuality));
								int vertexIndex = vertexX + vertexY * VerticesAmount;
								vertexIndex = FMath::Clamp(vertexIndex, 0, Vertices.Num() - 1);

								bool ShouldSpawn = true;
								
								if (ForestNoise[vertexIndex] > 0.5 && PlanetData->BiomeData[Biomes[vertexIndex]].FoliageData == FoliageBiomes[x])
								{
									ShouldSpawn = false;
								}

								if (ForestNoise[vertexIndex] <= 0.5 && PlanetData->BiomeData[Biomes[vertexIndex]].ForestFoliageData == FoliageBiomes[x])
								{
									ShouldSpawn = false;
								}

								if (PlanetData->BiomeData[Biomes[vertexIndex]].FoliageData != FoliageBiomes[x] && PlanetData->BiomeData[Biomes[vertexIndex]].ForestFoliageData != FoliageBiomes[x])
								{
									ShouldSpawn = false;
								}

								if (ShouldSpawn == true)
								{
									planetSpacefoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));
									planetSpacefoliageLocation.Normalize();

									//double noise6 = PlanetData->fbm(planetSpacefoliageLocation * PlanetData->noiseScale / 2, 2.0244, 0.454, 0.454, 6);//FMath::PerlinNoise3D(planetSpacefoliageLocation * noiseScale / 4) * 4;
									//noise6 = ((noise6 - 0.25) * 40) - 6;
									//noise6 = FMath::Clamp(noise6, -1, 1);
								
									double Height = VertexHeight[vertexIndex];

									if (Biome.Foliage.AbsoluteHeight == true)
									{
										Height = 0;
									}
						
			

									planetSpacefoliageLocation *= (PlanetData->PlanetRadius + Height - Biome.Foliage.DepthOffset);
									planetSpacefoliageLocation = planetSpacefoliageLocation - (ChunkLocation - PlanetSpaceLocation);
									//WorldfoliageLocation = planetSpacefoliageLocation + planetCenterWS;
							
						

									if (Height > Biome.Foliage.MaxHeight || Height < Biome.Foliage.MinHeight)
									{
										ShouldSpawn = false;
										VertexColors[vertexIndex].B = 255;
									}

									if (Slopes[vertexIndex] > 8 && Biome.Foliage.isSlopeFoliage == false)
									{
										ShouldSpawn = false;
										VertexColors[vertexIndex].B = 255;
									}

						

									//double Forestnoise = FMath::PerlinNoise3D(WorldfoliageLocation / 100000);

									//FVector localfoliagelocation = UKismetMathLibrary::InverseTransformLocation(this->GetActorTransform(), WorldfoliageLocation);
						
						
							
									if (ShouldSpawn == true)
									{
										FRotator Rot = FRotator(0, 0, 0);
						
										if (Biome.Foliage.AlignToTarrain == false)
										{						
											Rot = UKismetMathLibrary::FindLookAtRotation(FVector(0,0, 0), planetSpacefoliageLocation);
											Rot = FRotator(Rot.Pitch - 90, Rot.Yaw, Rot.Roll);
										}
										else if (Vertices.IsValidIndex(vertexIndex) && Normals.IsValidIndex(vertexIndex))
										{
											FVector v1 = FVector(Vertices[vertexIndex]);

											FVector v2 = FVector(Vertices[vertexIndex]) + FVector(Normals[vertexIndex]);

											Rot = UKismetMathLibrary::FindLookAtRotation(v1, v2);
											Rot = FRotator(Rot.Pitch - 90, Rot.Yaw, Rot.Roll);
										}

										FTransform transform = FTransform(Rot, planetSpacefoliageLocation - PlanetSpaceLocation, FVector(1, 1, 1));
										Rot = FRotator(0, ScaleStream.FRandRange(0, 360), 0);
										Rot = UKismetMathLibrary::TransformRotation(transform, Rot);
									
										transform = FTransform(Rot,planetSpacefoliageLocation - PlanetSpaceLocation,FVector(
												ScaleStream.FRandRange(Biome.Foliage.minScale, Biome.Foliage.maxScale) / 10,
												ScaleStream.FRandRange(Biome.Foliage.minScale, Biome.Foliage.maxScale) / 10,
												ScaleStream.FRandRange(Biome.Foliage.minScale, Biome.Foliage.maxScale) / 10)
												);
																					
										FoliageRuntimeData[index].LocalFoliageTransforms.Add(transform);
										//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("bbbbbb")));
										//VertexColors[vertexIndex] = FColor(VertexColors[vertexIndex].R, VertexColors[vertexIndex].G, noise6 * 255, 255);
									}
								}
								
							}
						}
				
					}
				}
			}
		}

		//ChunksToUpload->Add(this);
		//DataGenerated = true;
		UploadChunk();


}



void UChunkComponent::UploadChunk()
{
	if (NaniteLandscape)
	{

		//convert vertices to Positions
		TConstVoxelArrayView<FVector3f> Positions(Vertices.GetData(), Vertices.Num());
		
		//TConstVoxelArrayView<FVoxelOctahedron> Octahedron(Octahedrons.GetData(), Vertices.Num());

		TConstVoxelArrayView<FVector2f> TextureCoordinatesV(UVs.GetData(), UVs.Num());

		TVoxelArray<TConstVoxelArrayView<FVector2f>> TextureCoordinatesVV;
		TextureCoordinatesVV.Add(TextureCoordinatesV);

		FPlanetNaniteBuilder NaniteBuilder;
		NaniteBuilder.PositionPrecision = -(PlanetData->maxRecursionLevel - recursionLevel) + 3;

		NaniteBuilder.Mesh.Positions = Positions;
		
		NaniteBuilder.Mesh.Positions3f = &Vertices;
		NaniteBuilder.Mesh.Normals = Octahedrons;
		NaniteBuilder.Mesh.Normals3f = &Normals;
		NaniteBuilder.Mesh.Colors = VertexColors;
		NaniteBuilder.Mesh.TextureCoordinates = TextureCoordinatesVV;
		NaniteBuilder.TrianglesM = Triangles;
		
		

		if (Collisions)
		{
			TConstVoxelArrayView<uint16> FaceMaterials;
			ChaosMeshData = Chaos::FTriangleMeshImplicitObjectPtr (FVoxelChaosTriangleMeshCooker::Create(TArray<int32>(*Triangles), Positions, FaceMaterials));
		}

		// Generate the render data
		TVoxelArray<int32> VertexOffsets;
		RenderData = NaniteBuilder.CreateRenderData(VertexOffsets, bRaytracing);
		

		if (AbortAsync == true)
		{
			GenerationComplete();
			return;
		}
		
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			ChunkStaticMesh = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
			ChunkStaticMesh->bDoFastBuild = true;
			ChunkStaticMesh->bGenerateMeshDistanceField = false;
			ChunkStaticMesh->SetStaticMaterials({ FStaticMaterial() });
			ChunkStaticMesh->bSupportRayTracing = bRaytracing;
/*
			//if (bRaytracing)
			//{
				// Ensure UStaticMesh::HasValidRenderData returns true
				// Use MAX_flt to try to not have the vertex picked by vertex snapping
				//const TVoxelArray<FVector3f> DummyPositions = { FVector3f(MAX_flt) };
				const int32 NumVertices = Vertices.Num();
				LODResourcesArray[0].VertexBuffers.PositionVertexBuffer.Init(Vertices);
				LODResourcesArray[0].VertexBuffers.StaticMeshVertexBuffer.Init(NumVertices, 1);
				LODResourcesArray[0].VertexBuffers.ColorVertexBuffer.Init(VertexColors.Num());
				
				for (int32 i = 0; i < NumVertices; i++)
				{
					FVector3f Normal = (Normals)[i];
					FVector3f TangentX, TangentY;
					Normal.FindBestAxisVectors(TangentX, TangentY);
					LODResourcesArray[0].VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, TangentX, TangentY, Normal);
					LODResourcesArray[0].VertexBuffers.ColorVertexBuffer.VertexColor(i) = VertexColors[i];
					LODResourcesArray[0].VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, UVs[i]);
				}

				if (bRaytracing)
				{
					ChunkStaticMesh->GetRenderData()->InitializeRayTracingRepresentationFromRenderingLODs();
						
					TPimplPtr<FCardRepresentationData> MeshCards;
					FBox Box = Bounds.ToFBox();


					MeshCards = MakePimpl<FCardRepresentationData>();


					*MeshCards = FCardRepresentationData();
					FMeshCardsBuildData& CardData = MeshCards->MeshCardsBuildData;

					CardData.Bounds = Box;

					CardData.bMostlyTwoSided = false;

					MeshCardRepresentation::SetCardsFromBounds(CardData);
					LODResourcesArray[0].CardRepresentationData = new FCardRepresentationData();
					LODResourcesArray[0].CardRepresentationData->MeshCardsBuildData = CardData;
				}
				
				// Ensure FStaticMeshChunkStaticMesh->GetRenderData()::GetFirstValidLODIdx doesn't return -1
				LODResourcesArray[0].Sections.Emplace();
				LODResourcesArray[0].IndexBuffer.SetIndices(*Triangles, EIndexBufferStride::Force16Bit);
				LODResourcesArray[0].Sections[0].FirstIndex = 0;
				LODResourcesArray[0].Sections[0].NumTriangles = Triangles->Num() / 3;

				LODResourcesArray[0].Sections[0].FirstIndex = 0;
				LODResourcesArray[0].Sections[0].MinVertexIndex = 0;
				LODResourcesArray[0].Sections[0].MaxVertexIndex = NumVertices - 1;
				LODResourcesArray[0].Sections[0].MaterialIndex = 0;			
				//LODResourcesArray[0].Sections[0].bEnableCollision = false;
				//LODResourcesArray[0].Sections[0].bVisibleInRayTracing = true;

				LODResourcesArray[0].BuffersSize = NumVertices * sizeof(FVector3f);
				
				//LODResourcesArray[0].IndexBuffer.TrySetAllowCPUAccess(false);
				
			//}
			//else
			//{
				FStaticMeshLODResourcesArray& LODResourcesArray = ChunkStaticMesh->GetRenderData()->LODResources;
				LODResourcesArray[0].bBuffersInlined = true;
				LODResourcesArray[0].Sections.Emplace();
				LODResourcesArray[0].Sections[0].MaterialIndex = 0;
				// Ensure UStaticMesh::HasValidChunkStaticMesh->GetRenderData() returns true
				// Use MAX_flt to try to not have the vertex picked by vertex snapping
				const TVoxelArray<FVector3f> DummyPositions = { FVector3f(MAX_flt) };
				
				LODResourcesArray[0].VertexBuffers.StaticMeshVertexBuffer.Init(DummyPositions.Num(), 1);
				LODResourcesArray[0].VertexBuffers.PositionVertexBuffer.Init(DummyPositions);
				LODResourcesArray[0].VertexBuffers.ColorVertexBuffer.Init(DummyPositions.Num());

				// Ensure FStaticMeshChunkStaticMesh->GetRenderData()::GetFirstValidLODIdx doesn't return -1
				LODResourcesArray[0].BuffersSize = 1;
				//LODResourcesArray[0].IndexBuffer.TrySetAllowCPUAccess(false);

			//}
			*/

			//TUniquePtr<FStaticMeshChunkStaticMesh->GetRenderData()> MChunkStaticMesh->GetRenderData() = MakeUnique<FStaticMeshRenderData>();
			//MChunkStaticMesh->GetRenderData()->Bounds = Bounds.ToFBox();
			//MChunkStaticMesh->GetRenderData()->NumInlinedLODs = 1;
			  //ChunkStaticMesh->GetRenderData()->NaniteResourcesPtr = MakePimpl<Nanite::FResources>(MoveTemp(Resources));
			//MLODResourcesArray.Add(LODResource);
			//MChunkStaticMesh->GetRenderData()->LODVertexFactories.Add(FStaticMeshVertexFactories(GMaxRHIFeatureLevel));
			
			
			
			//NaniteBuilder.ApplyChunkStaticMesh->GetRenderData()(*ChunkStaticMesh, MoveTemp(ChunkStaticMesh->GetRenderData()), bRaytracing, Collisions, ChaosMeshData);
			
			

			
			if (Collisions)
			{
				ChunkStaticMesh->CreateBodySetup();

				//UStaticMesh* StaticMeshP = &StaticMesh;
				//BodySetup->BodySetupGuid = FGuid::NewGuid();
				UBodySetup* BodySetup = ChunkStaticMesh->GetBodySetup();
				BodySetup->bGenerateMirroredCollision = false;
				BodySetup->bDoubleSidedGeometry = false;
				BodySetup->bSupportUVsAndFaceRemap = false;
				BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

				
				ChaosMeshData->SetDoCollide(false);
				BodySetup->TriMeshGeometries = { ChaosMeshData };

				
				BodySetup->BodySetupGuid           = FGuid::NewGuid();
				BodySetup->bHasCookedCollisionData = true;
				BodySetup->bCreatedPhysicsMeshes = true;
			}
			
			
			//ChunkStaticMesh->LODForCollision = 0;
			//ChunkStaticMesh->bAllowCPUAccess = false;
			ChunkStaticMesh->SetRenderData(MoveTemp(RenderData));

		#if WITH_EDITOR
			ChunkStaticMesh->NaniteSettings.bEnabled = true;
		#endif

			ChunkStaticMesh->CalculateExtendedBounds();
			ChunkStaticMesh->InitResources();
			this->SetStaticMesh(ChunkStaticMesh);
			
			
			
			MaterialInst = this->CreateDynamicMaterialInstance(0, PlanetData->PlanetMaterial);
			MaterialInst->SetTextureParameterValue("BiomeMap", RenderTarget);
			MaterialInst->SetTextureParameterValue("BiomeData", GPUBiomeData);
			MaterialInst->SetScalarParameterValue("recursionLevel", PlanetData->maxRecursionLevel - recursionLevel);
			MaterialInst->SetScalarParameterValue("PlanetRadius", PlanetData->PlanetRadius);
			MaterialInst->SetScalarParameterValue("NoiseHeight", PlanetData->noiseHeight);
			MaterialInst->SetScalarParameterValue("ChunkSize", ChunkSize);
			MaterialInst->SetVectorParameterValue("ComponentLocation", ChunkLocation);
			
			
			for (int i = 0; i < PlanetData->BiomeData.Num(); i++)
			{
				FMaterialParameterInfo LayerInfo;
				LayerInfo.Name = "LayerIndex";
				LayerInfo.Association = EMaterialParameterAssociation::BlendParameter;
				LayerInfo.Index = FMath::Max(i - 1,0);
				MaterialInst->SetScalarParameterValueByInfo(LayerInfo, i + 0.01f);

				FMaterialParameterInfo BiomeCountInfo;
				BiomeCountInfo.Name = "BiomeCount";
				BiomeCountInfo.Association = EMaterialParameterAssociation::BlendParameter;
				BiomeCountInfo.Index = FMath::Max(i - 1,0);
				MaterialInst->SetScalarParameterValueByInfo(BiomeCountInfo, float(PlanetData->BiomeData.Num()) + 0.01f);

				FMaterialParameterInfo BlendInfo;
				BlendInfo.Name = "BiomeMap";
				BlendInfo.Association = EMaterialParameterAssociation::BlendParameter;
				BlendInfo.Index = FMath::Max(i - 1,0);
				MaterialInst->SetTextureParameterValueByInfo(BlendInfo, RenderTarget);

				FMaterialParameterInfo PlanetRadiusInfo;
				PlanetRadiusInfo.Name = "PlanetRadius";
				PlanetRadiusInfo.Association = EMaterialParameterAssociation::LayerParameter;
				PlanetRadiusInfo.Index = i;
				MaterialInst->SetScalarParameterValueByInfo(PlanetRadiusInfo, PlanetData->PlanetRadius);

				FMaterialParameterInfo NoiseHeightInfo;
				NoiseHeightInfo.Name = "NoiseHeight";
				NoiseHeightInfo.Association = EMaterialParameterAssociation::LayerParameter;
				NoiseHeightInfo.Index = i;
				MaterialInst->SetScalarParameterValueByInfo(NoiseHeightInfo, PlanetData->noiseHeight);

				FMaterialParameterInfo ChunkSizeInfo;
				ChunkSizeInfo.Name = "ChunkSize";
				ChunkSizeInfo.Association = EMaterialParameterAssociation::LayerParameter;
				ChunkSizeInfo.Index = i;
				MaterialInst->SetScalarParameterValueByInfo(ChunkSizeInfo, ChunkSize);

				FMaterialParameterInfo LocationInfo;
				LocationInfo.Name = "ComponentLocation";
				LocationInfo.Association = EMaterialParameterAssociation::LayerParameter;
				LocationInfo.Index = i;
				MaterialInst->SetVectorParameterValueByInfo(LocationInfo, ChunkLocation);

				FMaterialParameterInfo RecursionInfo;
				RecursionInfo.Name = "recursionLevel";
				RecursionInfo.Association = EMaterialParameterAssociation::LayerParameter;
				RecursionInfo.Index = i;
				MaterialInst->SetScalarParameterValueByInfo(RecursionInfo, PlanetData->maxRecursionLevel - recursionLevel);
			}
			this->RegisterComponent();
			if (GenerateFoliage)
			{
				UploadFoliageData();
			}
			

			GenerationComplete();
		});
		//});
	}
	
}

void UChunkComponent::SpawnMesh()
{
			
}


void UChunkComponent::UploadFoliage()
{
	
		UploadFoliageData();

}

void UChunkComponent::BeginSelfDestruct()
{
	AbortAsync = true;
	ChunksToRemoveQueue->Add(this);
	
}

void UChunkComponent::SelfDestruct()
{
	if (ChunksToFinish != nullptr)
	{
		ChunksToFinish->Remove(this);
	}
	
	
	for (int i = 0; i < ChunksToRemove.Num(); i++)
	{
			ChunksToRemove[i]->BeginSelfDestruct();
	}
	ChunksToRemove.Empty();
	for(int i = 0; i < FoliageRuntimeData.Num(); i++)
	{
		if (FoliageRuntimeData[i].hismc != nullptr)
		{
			FoliageRuntimeData[i].hismc->UnregisterComponent();
			FoliageRuntimeData[i].hismc->ClearInstances();
			//FoliageRuntimeData[i].hismc->GetBodySetup()->ConditionalBeginDestroy();
			FoliageRuntimeData[i].hismc->DestroyComponent();
		}
	}
	FoliageRuntimeData.Empty();
	FoliageBiomes.Empty();

	//clear all arrays
	Vertices.Empty();
	Slopes.Empty();
	VertexHeight.Empty();
	Biomes.Empty();
	Normals.Empty();
	Octahedrons.Empty();
	UVs.Empty();
	VertexColors.Empty();
	VertexHeight.Empty();
	ForestNoise.Empty();
	RandomForest.Empty();

	GPUBiomeData = nullptr;

	//if (PCGComp != nullptr)
	//{
		//PCGComp->StopGenerationInProgress();
		//PCGComp->DestroyComponent();
	//}
	
	//UnregisterComponent();
	SetStaticMesh(nullptr);

	//if (IsRegistered())
	//{
	//	UnregisterComponent();
	//}
	
	ChaosMeshData = nullptr;
	//ChunkStaticMesh->ReleaseResources();
	//delete ChunkStaticMesh->GetRenderData();

	if (MaterialInst)
	{
		MaterialInst->ConditionalBeginDestroy();
		MaterialInst = nullptr;
	}

	
	if (ChunkStaticMesh != nullptr)
	{
		if (ChunkStaticMesh->GetMaterial(0))
		{
			ChunkStaticMesh->GetMaterial(0)->ConditionalBeginDestroy();
		}
		if (ChunkStaticMesh->GetBodySetup())
		{
			ChunkStaticMesh->GetBodySetup()->ClearPhysicsMeshes();
			ChunkStaticMesh->GetBodySetup()->ConditionalBeginDestroy();
		}

		ChunkStaticMesh->ReleaseResources();
		FlushRenderingCommands();
		//ChunkStaticMesh->GetRenderData()->ReleaseResources();
		ChunkStaticMesh->NaniteSettings.bEnabled = false;
	//	ChunkStaticMesh->GetRenderData()->NaniteResourcesPtr->ReleaseResources();
		if (ChunkStaticMesh->GetRenderData()->NaniteResourcesPtr.IsValid())
		{
			ChunkStaticMesh->GetRenderData()->NaniteResourcesPtr->ReleaseResources();
			ChunkStaticMesh->GetRenderData()->NaniteResourcesPtr.Reset();
		}
		//ChunkStaticMesh->InitResources();

		ChunkStaticMesh->MarkAsGarbage();
		ChunkStaticMesh->ConditionalBeginDestroy();
		ChunkStaticMesh = nullptr;
	}
	
	if (RenderTarget != nullptr)
    {
        RenderTarget->ConditionalBeginDestroy();
    }
	//ConditionalBeginDestroy();
	ChunksToFinish = nullptr;
	ChunksToRemoveQueue = nullptr;
	DestroyComponent();
	//CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}


