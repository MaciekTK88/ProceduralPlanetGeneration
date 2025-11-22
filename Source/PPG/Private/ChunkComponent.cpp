// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#include "ChunkComponent.h"
#include "Engine/GameEngine.h"
#include "Math/Vector.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
//#include "RealtimeMeshSimple.h"
#include <MeshCardBuild.h>

#include "Kismet/KismetMathLibrary.h"
#include "PlanetNaniteBuilder.h"
#include "VoxelMinimal.h"
#include "Async/TaskGraphInterfaces.h"
#include "PhysicsEngine/BodySetup.h"
#include "VoxelChaosTriangleMeshCooker.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "ComputeShader/Public/PlanetComputeShader/PlanetComputeShader.h"


UChunkComponent::UChunkComponent()
{
	
}



void UChunkComponent::GenerationComplete()
{
	if (IsInGameThread())
	{
		//ChunksToFinish->Add(this);
		if (AbortAsync == false)
		{
			AssignComponents();
			ChunkStatus = EChunkStatus::READY;
		}
		else
		{
			ChunkStatus = EChunkStatus::ABORTED;
		}

	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			//ChunksToFinish->Add(this);
			if (AbortAsync == false)
			{
				AssignComponents();
				ChunkStatus = EChunkStatus::READY;
			}
			else
			{
				ChunkStatus = EChunkStatus::ABORTED;
			}
		});
	}
}


void UChunkComponent::SetFoliageActor(AActor* NewFoliageActor)
{
	FoliageActor = NewFoliageActor;
}

void UChunkComponent::UploadFoliageData()
{

	for (int i = 0; i < FoliageRuntimeData.Num(); i++)
	{
		if (GenerateFoliage == true && FoliageRuntimeData[i].LocalFoliageTransforms.Num() > 0)
		{
			UInstancedStaticMeshComponent* Ismc;
			/*if ((*FoliageISMCPool).IsEmpty() == false)
			{
				Ismc = (*FoliageISMCPool).Pop();
				Ismc->SetVisibility(true);
			}
			else
			{*/
				Ismc = NewObject<UInstancedStaticMeshComponent>(FoliageActor, NAME_None, RF_Transient);
				Ismc->SetupAttachment(FoliageActor->GetRootComponent());
				Ismc->SetMobility(EComponentMobility::Movable);
				Ismc->SetGenerateOverlapEvents(false);
				Ismc->WorldPositionOffsetDisableDistance = FoliageRuntimeData[i].Foliage.WPODisableDistance;
				Ismc->SetEvaluateWorldPositionOffset(FoliageRuntimeData[i].Foliage.WPO);
				Ismc->bAffectDistanceFieldLighting = false;
				Ismc->InstanceEndCullDistance = FoliageRuntimeData[i].Foliage.CullingDistance;
				Ismc->bWorldPositionOffsetWritesVelocity = FoliageRuntimeData[i].Foliage.WPO;
				Ismc->bDisableCollision = true;
				Ismc->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
				Ismc->SetCanEverAffectNavigation(false);
			//}



			if (FoliageRuntimeData[i].Foliage.ShadowDisableDistance <= PlanetData->maxRecursionLevel - recursionLevel && FoliageRuntimeData[i].Foliage.ShadowDisableDistance != 0)
			{
				Ismc->CastShadow = false;
				Ismc->bCastContactShadow = false;
				Ismc->SetVisibleInRayTracing(false);
			}
			
			if (FoliageRuntimeData[i].Foliage.CollisionEnableDistance >= PlanetData->maxRecursionLevel - recursionLevel && FoliageRuntimeData[i].Foliage.CollisionEnableDistance != 0 && FoliageRuntimeData[i].Foliage.Collision == true)
			{
				Ismc->bDisableCollision = false;
			}

			if (FoliageRuntimeData[i].Foliage.LowPolyActivation <= PlanetData->maxRecursionLevel - recursionLevel && FoliageRuntimeData[i].Foliage.LowPolyMesh != nullptr)
			{
				Ismc->SetStaticMesh(FoliageRuntimeData[i].Foliage.LowPolyMesh);
				Ismc->SetEvaluateWorldPositionOffset(FoliageRuntimeData[i].Foliage.FarFoliageWPO);
				Ismc->bAffectDistanceFieldLighting = false;
				Ismc->SetVisibleInRayTracing(false);
				Ismc->bDisableCollision = true;

			}
			else
			{
				Ismc->SetStaticMesh(FoliageRuntimeData[i].Foliage.FoliageMesh);
			}

			//hismc->bIsAsyncBuilding = true;
			Ismc->SetRelativeTransform(FTransform(ChunkSMC->GetRelativeRotation(), ChunkLocation, FVector(1.0f, 1.0f, 1.0f)));
			Ismc->AddInstances(FoliageRuntimeData[i].LocalFoliageTransforms, false, false, false);
			Ismc->RegisterComponent();
			FoliageRuntimeData[i].Ismc = Ismc;
	
		}
	}
}


void UChunkComponent::GenerateChunk()
{
	ChunkStatus = EChunkStatus::GENERATING;
	
	//clear all arrays
	Vertices.Empty();
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
	if (GenerateCollisions && recursionLevel >= PlanetData->maxRecursionLevel - CollisionDisableDistance)
	{
		Collisions = true;
	}

	
	//create PF_G16 UTextureRenderTarget2D
	BiomeMap = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	BiomeMap->bCanCreateUAV = true;
	BiomeMap->bSupportsUAV = true;
	BiomeMap->InitCustomFormat(VerticesAmount, VerticesAmount * 2, PF_R8G8B8A8, true);
	BiomeMap->RenderTargetFormat = RTF_RGBA8;
	BiomeMap->bAutoGenerateMips = false;
	BiomeMap->SRGB = false;
	BiomeMap->Filter = TF_Nearest;
	BiomeMap->AddressX = TA_Clamp;
	BiomeMap->AddressY = TA_Clamp;
	//RenderTarget->ClearColor = FLinearColor::Black; // Optional: Set default clear color
	BiomeMap->UpdateResourceImmediate(); // Must be called after setting UAV flags

	
	// Create a dispatch parameters struct and fill it the input array with our args
	FPlanetComputeShaderDispatchParams Params(VerticesAmount, VerticesAmount, 1);
	Params.ChunkLocation = FVector3f(PlanetSpaceLocation);
	Params.ChunkRotation = PlanetSpaceRotation;
	Params.ChunkOriginLocation = FVector3f(ChunkLocation);
	Params.ChunkSize = ChunkSize;
	Params.PlanetRadius = PlanetData->PlanetRadius;
	Params.NoiseHeight = PlanetData->noiseHeight;
	Params.BiomeMap = BiomeMap;
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
	ForestVertices.Reserve(VerticesAmount * VerticesAmount);
	
	FPlanetComputeShaderInterface::Dispatch(Params, [this](TArray<float> OutputVal, TArray<uint8> OutputVCVal) {

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

					
					uint8 BiomeIndex = OutputVCVal[(x + y * VerticesAmount) * 3 + 2];
					uint8 ShaderForestNoise = OutputVCVal[(x + y * VerticesAmount) * 3 + 1];

					bool ForestVertex = false;

					if (PlanetData->BiomeData[BiomeIndex].GenerateForest == true && PlanetData->BiomeData[BiomeIndex].ForestFoliageData != nullptr)
					{
						float VertexRandom = HashFVectorToVector2D(FVector(Vertex) + ChunkLocation, 0, 255, Size * 2.0f).X;
						//float VertexRandom = (FMath::PerlinNoise3D((FVector(Vertex) + ChunkLocation) * 0.001f) + 1) / 2.0f * 255.0f;
						if ((ShaderForestNoise <= VertexRandom || noiseheight > PlanetData->BiomeData[BiomeIndex].ForestFoliageData->FoliageList[0].MaxHeight || noiseheight < PlanetData->BiomeData[BiomeIndex].ForestFoliageData->FoliageList[0].MinHeight) && PlanetData->BiomeData[BiomeIndex].FoliageData != nullptr)
						{
							FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].FoliageData);
							ShaderForestNoise = 0;
						}
						else if (PlanetData->BiomeData[BiomeIndex].ForestFoliageData != nullptr)
						{
							FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].ForestFoliageData);
							ForestVertex = true;
						}
					}
					else
					{
						FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].FoliageData);
						ShaderForestNoise = 0;
					}
					VertexColors.Add(FColor(OutputVCVal[(x + y * VerticesAmount) * 3], ShaderForestNoise, BiomeIndex, 0));
					
					VertexHeight.Add(noiseheight);
					Slopes.Add(0);
					Biomes.Add(BiomeIndex);
					ForestVertices.Add(ForestVertex);
					
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
				
					if (FoliageBiomes[x]->FoliageList[i].FoliageDistance >= PlanetData->maxRecursionLevel - recursionLevel)
					{
						FFoliageRuntimeDataS Biome;
						Biome.Foliage = FoliageBiomes[x]->FoliageList[i];
						int index = FoliageRuntimeData.Add(Biome);
						

						int Density = Biome.Foliage.FoliageDensity;
						
						
						if (Biome.Foliage.ScalableDensity == true)
						{
							Density = Density * FoliageDensityScale;
						}

						if (Biome.Foliage.LowPolyActivation <= PlanetData->maxRecursionLevel - recursionLevel && Biome.Foliage.LowPolyMesh != nullptr)
						{
							Density = Density * Biome.Foliage.FarFoliageDensityScale;
						}

						//LocalFoliageTransform.Empty();
						int localdensity = Density * FMath::Pow(2, float(PlanetData->maxRecursionLevel - recursionLevel));
						
						for (int y = 0; y < localdensity; y++)
						{
							for (int z = 0; z < localdensity; z++)
							{
							
								//FVector2f ChunkUVpos = PlanetData->InversePlanetTransformLocation(PlanetSpaceRotation, PlanetSpaceLocation);
								float xlocalpos = y * minchunkSize / (float)(Density);
								float ylocalpos = z * minchunkSize / (float)(Density);
								
							
								FVector PlanetSpaceFoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));
								FVector NormalizedfoliageLocation = PlanetSpaceFoliageLocation.GetSafeNormal() * PlanetData->PlanetRadius;

								//uint32 SeedValue = FCrc::MemCrc32(&NormalizedfoliageLocation, sizeof(FVector));
								FRandomStream ScaleStream((int)(NormalizedfoliageLocation.X / 10000) + (int)(NormalizedfoliageLocation.Y / 10000) + (int)(NormalizedfoliageLocation.Z / 10000));

								FVector2d NewLocalPos = HashFVectorToVector2D(NormalizedfoliageLocation + x * 100 + i * 100, 0.0f, minchunkSize, 10.0f);
								NewLocalPos += FVector2d((int)((xlocalpos / minchunkSize)) * minchunkSize, (int)((ylocalpos / minchunkSize)) * minchunkSize);
								
								
								xlocalpos = NewLocalPos.X;
								ylocalpos = NewLocalPos.Y;

								int vertexX = FMath::RoundToInt(xlocalpos / ChunkSize * float(ChunkQuality));
								int vertexY = FMath::RoundToInt(ylocalpos / ChunkSize * float(ChunkQuality));
								int vertexIndex = vertexX + vertexY * VerticesAmount;
								vertexIndex = FMath::Clamp(vertexIndex, 0, Vertices.Num() - 1);

								bool ShouldSpawn = true;
								
								if (ForestVertices[vertexIndex] == true && PlanetData->BiomeData[Biomes[vertexIndex]].FoliageData == FoliageBiomes[x])
								{
									ShouldSpawn = false;
								}

								if (ForestVertices[vertexIndex] == false && PlanetData->BiomeData[Biomes[vertexIndex]].ForestFoliageData == FoliageBiomes[x])
								{
									ShouldSpawn = false;
								}

								if (PlanetData->BiomeData[Biomes[vertexIndex]].FoliageData != FoliageBiomes[x] && PlanetData->BiomeData[Biomes[vertexIndex]].ForestFoliageData != FoliageBiomes[x])
								{
									ShouldSpawn = false;
								}

								if (ShouldSpawn == true)
								{
									PlanetSpaceFoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));

									//transform planetSpaceVertex to -1, 1 range
									float OriginalChunkSize = PlanetData->PlanetRadius * 2.0 / sqrt(2.0);
									PlanetSpaceFoliageLocation = PlanetSpaceFoliageLocation / (OriginalChunkSize / 2.0f);

									//apply deformation
									float deformation = 0.75;
									PlanetSpaceFoliageLocation.X = tan(PlanetSpaceFoliageLocation.X * PI * deformation / 4.0);
									PlanetSpaceFoliageLocation.Y = tan(PlanetSpaceFoliageLocation.Y * PI * deformation / 4.0);
									PlanetSpaceFoliageLocation.Z = tan(PlanetSpaceFoliageLocation.Z * PI * deformation / 4.0);
									
									PlanetSpaceFoliageLocation.Normalize();
								
									double Height = VertexHeight[vertexIndex];

									if (Biome.Foliage.AbsoluteHeight == true)
									{
										Height = 0;
									}
						
			

									PlanetSpaceFoliageLocation *= (PlanetData->PlanetRadius + Height - Biome.Foliage.DepthOffset);
									PlanetSpaceFoliageLocation = PlanetSpaceFoliageLocation - (ChunkLocation - PlanetSpaceLocation);
							
						

									if (Height > Biome.Foliage.MaxHeight || Height < Biome.Foliage.MinHeight)
									{
										ShouldSpawn = false;
										//VertexColors[vertexIndex].G = 255;
									}

									if (Slopes[vertexIndex] > 8 && Biome.Foliage.isSlopeFoliage == false)
									{
										ShouldSpawn = false;
										//VertexColors[vertexIndex].G = 255;
									}
						
						
							
									if (ShouldSpawn == true)
									{
										FRotator Rot = FRotator(0, 0, 0);
						
										if (Biome.Foliage.AlignToTarrain == false)
										{						
											Rot = UKismetMathLibrary::FindLookAtRotation(FVector(0,0, 0), PlanetSpaceFoliageLocation);
											Rot = FRotator(Rot.Pitch - 90, Rot.Yaw, Rot.Roll);
										}
										else if (Vertices.IsValidIndex(vertexIndex) && Normals.IsValidIndex(vertexIndex))
										{
											FVector v1 = FVector(Vertices[vertexIndex]);

											FVector v2 = FVector(Vertices[vertexIndex]) + FVector(Normals[vertexIndex]);

											Rot = UKismetMathLibrary::FindLookAtRotation(v1, v2);
											Rot = FRotator(Rot.Pitch - 90, Rot.Yaw, Rot.Roll);
										}

										FTransform transform = FTransform(Rot, PlanetSpaceFoliageLocation - PlanetSpaceLocation, FVector(1, 1, 1));
										Rot = FRotator(0, ScaleStream.FRandRange(0, 360), 0);
										Rot = UKismetMathLibrary::TransformRotation(transform, Rot);
									
										transform = FTransform(Rot,PlanetSpaceFoliageLocation - PlanetSpaceLocation,FVector(
												ScaleStream.FRandRange(Biome.Foliage.minScale, Biome.Foliage.maxScale),
												ScaleStream.FRandRange(Biome.Foliage.minScale, Biome.Foliage.maxScale),
												ScaleStream.FRandRange(Biome.Foliage.minScale, Biome.Foliage.maxScale))
												);
																					
										FoliageRuntimeData[index].LocalFoliageTransforms.Add(transform);
									}
								}
								
							}
						}
				
					}
				}
			}
		}
	
		UploadChunk();

}



void UChunkComponent::UploadChunk()
{
	if (NaniteLandscape)
	{

		//convert vertices to Positions
		TConstVoxelArrayView<FVector3f> Positions(Vertices.GetData(), Vertices.Num());

		TConstVoxelArrayView<FVector2f> TextureCoordinatesV(UVs.GetData(), UVs.Num());

		TVoxelArray<TConstVoxelArrayView<FVector2f>> TextureCoordinatesVV;
		TextureCoordinatesVV.Add(TextureCoordinatesV);
		TArray<int32> IntTriangles = TArray<int32>((*Triangles));

		TConstVoxelArrayView<int32> Indices(IntTriangles.GetData(), IntTriangles.Num());
		
		NaniteBuilder.PositionPrecision = -(PlanetData->maxRecursionLevel - recursionLevel) + 3;
		NaniteBuilder.Mesh.Indices = Indices;
		NaniteBuilder.Mesh.Positions = Positions;
		NaniteBuilder.bCompressVertices = true;
		NaniteBuilder.Mesh.Normals = Octahedrons;
		NaniteBuilder.Mesh.Colors = VertexColors;
		NaniteBuilder.Mesh.TextureCoordinates = TextureCoordinatesVV;
		
		
		Chaos::FTriangleMeshImplicitObjectPtr ChaosMeshData;
		if (Collisions)
		{
			TConstVoxelArrayView<uint16> FaceMaterials;
			ChaosMeshData = Chaos::FTriangleMeshImplicitObjectPtr (FVoxelChaosTriangleMeshCooker::Create(TArray<int32>(*Triangles), Positions, FaceMaterials));
		}

		// Generate the render data
		TVoxelArray<int32> VertexOffsets;
		TVoxelArray<int32> ClusteredIndices;
		RenderData = NaniteBuilder.CreateRenderData(AbortAsync, bRaytracing);
		

		if (AbortAsync == true)
		{
			GenerationComplete();
			return;
		}
		
		
		
		AsyncTask(ENamedThreads::GameThread, [this, ChaosMeshData]()
		{
			if (AbortAsync == true)
			{
				GenerationComplete();
				return;
			}
			
			ChunkStaticMesh = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
			ChunkStaticMesh->bDoFastBuild = true;
			ChunkStaticMesh->bGenerateMeshDistanceField = false;
			ChunkStaticMesh->SetStaticMaterials({ FStaticMaterial() });
			ChunkStaticMesh->bSupportRayTracing = bRaytracing;
								
						
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

							
				//ChaosMeshData->SetDoCollide(false);
				BodySetup->TriMeshGeometries = { ChaosMeshData };

							
				//BodySetup->BodySetupGuid           = FGuid::NewGuid();
				BodySetup->bHasCookedCollisionData = true;
				BodySetup->bCreatedPhysicsMeshes = true;
			}
					
			ChunkStaticMesh->SetRenderData(MoveTemp(RenderData));

			#if WITH_EDITOR
			ChunkStaticMesh->NaniteSettings.bEnabled = true;
			#endif

			ChunkStaticMesh->CalculateExtendedBounds();
			ChunkStaticMesh->InitResources();
			
			
			GenerationComplete();
			
		});
	}
	
}

void UChunkComponent::AssignComponents()
{
	
	if ((*ChunkSMCPool).IsEmpty() == false)
	{
		//get avaliable ChunkSMC
		ChunkSMC = (*ChunkSMCPool).Pop();
		ChunkSMC->SetRelativeLocation(ChunkLocation);
		ChunkSMC->GetComponentVelocity() = FVector::ZeroVector;
		ChunkSMC->ResetSceneVelocity();
		ChunkSMC->SetVisibility(true);
	}
	else
	{
		ChunkSMC = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), NAME_None, RF_Transient);
		ChunkSMC->SetupAttachment(Cast<AActor>(GetOuter())->GetRootComponent());
		ChunkSMC->SetMobility(EComponentMobility::Movable);
		ChunkSMC->SetCanEverAffectNavigation(false);
		ChunkSMC->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
		ChunkSMC->bWorldPositionOffsetWritesVelocity = false;
		ChunkSMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ChunkSMC->SetRelativeLocation(ChunkLocation);
		ChunkSMC->GetComponentVelocity() = FVector::ZeroVector;
		ChunkSMC->ResetSceneVelocity();
	}




	
	MaterialInst = ChunkSMC->CreateDynamicMaterialInstance(0, PlanetData->PlanetMaterial);
	MaterialInst->SetTextureParameterValue("BiomeMap", BiomeMap);
	MaterialInst->SetTextureParameterValue("BiomeData", GPUBiomeData);
	MaterialInst->SetScalarParameterValue("recursionLevel", PlanetData->maxRecursionLevel - recursionLevel);
	MaterialInst->SetScalarParameterValue("PlanetRadius", PlanetData->PlanetRadius);
	MaterialInst->SetScalarParameterValue("NoiseHeight", PlanetData->noiseHeight);
	MaterialInst->SetScalarParameterValue("ChunkSize", ChunkSize);
	MaterialInst->SetVectorParameterValue("ComponentLocation", ChunkLocation);
	
	
	for (int i = 0; i < MaterialLayersNum; i++)
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
		MaterialInst->SetScalarParameterValueByInfo(BiomeCountInfo, float(MaterialLayersNum) + 0.01f);

		FMaterialParameterInfo BlendInfo;
		BlendInfo.Name = "BiomeMap";
		BlendInfo.Association = EMaterialParameterAssociation::BlendParameter;
		BlendInfo.Index = FMath::Max(i - 1,0);
		MaterialInst->SetTextureParameterValueByInfo(BlendInfo, BiomeMap);

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
	

	if (GenerateFoliage)
	{
		UploadFoliageData();
	}
	
	if (ChunkMinHeight < 0 && PlanetData->GenerateWater)
	{
		AddWaterChunk();
	}
	ChunkSMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChunkSMC->SetStaticMesh(ChunkStaticMesh);
	ChunkSMC->ResetSceneVelocity();
	ChunkSMC->RegisterComponent();
}



void UChunkComponent::AddWaterChunk()
{
	FVector FaceDefaultForward = FVector(0, 0, 1);
				
	FVector InputDir = FVector(PlanetSpaceRotation);
	InputDir = InputDir.GetSafeNormal();

	// Compute the quaternion that rotates from the face's default forward to the input direction.
	FQuat FaceRotation = FQuat::FindBetweenNormals(FaceDefaultForward, InputDir);
	FTransform WaterTransform = FTransform(FaceRotation, ChunkLocation, FVector(ChunkSize / 100000, ChunkSize / 100000, ChunkSize / 100000));

	FVector ChunkPositionOffset = PlanetSpaceLocation - ChunkLocation;

	if (WaterSMCPool->IsEmpty() == false)
	{
		WaterChunk = WaterSMCPool->Pop();
		WaterChunk->SetVisibility(true);
	}
	else
	{
		//spawn static mesh component for water chunk
		WaterChunk = NewObject<UStaticMeshComponent>(GetOuter(), NAME_None, RF_Transient);
		WaterChunk->SetupAttachment(Cast<AActor>(GetOuter())->GetRootComponent());
		WaterChunk->SetRenderCustomDepth(true);
		WaterChunk->SetMobility(EComponentMobility::Movable);
		WaterChunk->SetCanEverAffectNavigation(false);
		WaterChunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WaterChunk->CastShadow = false;
		WaterChunk->SetVisibleInRayTracing(false);
		WaterChunk->RegisterComponent();
	}
	WaterChunk->SetRelativeTransform(WaterTransform);
	WaterChunk->ResetSceneVelocity();
	
	
	if (recursionLevel != PlanetData->maxRecursionLevel)
	{
		if (WaterChunk->GetStaticMesh() != FarWaterMesh)
		{
			WaterChunk->SetStaticMesh(FarWaterMesh);
		}
	}
	else
	{
		if (WaterChunk->GetStaticMesh() != CloseWaterMesh)
		{
			WaterChunk->SetStaticMesh(CloseWaterMesh);
		}
	}

	
	if (recursionLevel >= PlanetData->RecursionLevelForMaterialChange)
	{
		WaterMaterialInst = WaterChunk->CreateDynamicMaterialInstance(0, PlanetData->CloseWaterMaterial);
	}
	else
	{
		WaterMaterialInst = WaterChunk->CreateDynamicMaterialInstance(0, PlanetData->FarWaterMaterial);
	}
	
	WaterMaterialInst->SetTextureParameterValue("HeightMap", BiomeMap);
	WaterMaterialInst->SetScalarParameterValue("PlanetRadius", PlanetData->PlanetRadius);
	WaterMaterialInst->SetVectorParameterValue("PositionOffset", UKismetMathLibrary::InverseTransformLocation(WaterTransform, ChunkPositionOffset));
}

void UChunkComponent::FreeComponents()
{
	//Add components to pools
	if (ChunkSMC != nullptr)
	{
		/*
		ChunkSMC->SetStaticMesh(nullptr);
		ChunkSMC->SetVisibility(false);
		ChunkSMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		(*ChunkSMCPool).Add(ChunkSMC);
		ChunkSMC = nullptr;*/
		ChunkSMC->SetStaticMesh(nullptr);
		ChunkSMC->DestroyComponent();
		//print on screen ChunkSMCPool size
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("ChunkSMCPool Size: %d"), WaterSMCPool->Num()));
	}

	//clear static mesh
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

		UStaticMesh* LocalStaticMesh = ChunkStaticMesh;
		LocalStaticMesh->ReleaseResources();
		FStaticMeshRenderData* LocalRenderData = ChunkStaticMesh->GetRenderData();

		ENQUEUE_RENDER_COMMAND(ReleaseNaniteResources)(
			[LocalStaticMesh, LocalRenderData](FRHICommandListImmediate& RHICmdList)
			{
				if (LocalRenderData)
				{
					LocalRenderData->ReleaseResources();
					LocalStaticMesh->NaniteSettings.bEnabled = false;
					LocalRenderData->NaniteResourcesPtr->ReleaseResources();
					LocalRenderData->NaniteResourcesPtr.Reset();
				}
				
			});

		//ChunkStaticMesh->MarkAsGarbage();
		ChunkStaticMesh->ConditionalBeginDestroy();
		
		if (MaterialInst)
		{
			MaterialInst->ConditionalBeginDestroy();
			MaterialInst = nullptr;
		}
		ChunkStaticMesh = nullptr;
	}

	
	if (WaterChunk != nullptr)
	{
		WaterChunk->SetVisibility(false);
		(*WaterSMCPool).Add(WaterChunk);
		WaterChunk = nullptr;
	}
	if (WaterMaterialInst)
	{
		WaterMaterialInst->ConditionalBeginDestroy();
		WaterMaterialInst = nullptr;
	}
	
	for (int i = 0; i < FoliageRuntimeData.Num(); i++)
	{
		if (FoliageRuntimeData[i].Ismc != nullptr)
		{
			//FoliageRuntimeData[i].Ismc->SetVisibility(false);
			//FoliageRuntimeData[i].Ismc->ClearInstances();
			//(*FoliageISMCPool).Add(FoliageRuntimeData[i].Ismc);
			//FoliageRuntimeData[i].Ismc = nullptr;
			FoliageRuntimeData[i].Ismc->ClearInstances();
			FoliageRuntimeData[i].Ismc->DestroyComponent();
		}
	}

	for (UChunkComponent* Chunk : ChunksToRemove)
	{
		Chunk->FreeComponents();
	}
}


void UChunkComponent::BeginSelfDestruct()
{
	AbortAsync = true;
	FreeComponents();
	ChunkStatus = EChunkStatus::REMOVING;
	//ChunksToRemoveQueue->Add(this);
	
}

void UChunkComponent::SelfDestruct()
{
	FreeComponents();
	ChunksToRemove.Empty();


	FoliageBiomes.Empty();
	WaterChunk = nullptr;
	
	

	//clear all arrays
	Vertices.Empty();
	Slopes.Empty();
	VertexHeight.Empty();
	Biomes.Empty();
	Normals.Empty();
	Octahedrons.Empty();
	UVs.Empty();
	VertexColors.Empty();
	ForestVertices.Empty();
	RandomForest.Empty();

	FoliageRuntimeData.Empty();
	GPUBiomeData = nullptr;

	//clear biome map
	if (BiomeMap != nullptr)
    {
        BiomeMap->ConditionalBeginDestroy();
    }
	ConditionalBeginDestroy();
}


