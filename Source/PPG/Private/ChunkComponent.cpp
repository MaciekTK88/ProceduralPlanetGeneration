// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#include "ChunkComponent.h"
#include "Engine/GameEngine.h"
#include "Math/Vector.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"

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
		if (AbortAsync == false)
		{
			// Rate-limit component assignment
			ChunkStatus = EChunkStatus::PENDING_ASSIGN;
		}
		else
		{
			// Cleanup aborted data
			Vertices.Empty();
			Normals.Empty();
			Octahedrons.Empty();
			UVs.Empty();
			VertexColors.Empty();
			VertexHeight.Empty();
			Slopes.Empty();
			ForestStrength.Empty();
			Biomes.Empty();
			FoliageBiomeIndices.Empty();
			ChunkStatus = EChunkStatus::ABORTED;
		}

	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			if (AbortAsync == false)
			{
				// Rate-limit component assignment
				ChunkStatus = EChunkStatus::PENDING_ASSIGN;
			}
			else
			{
				// Cleanup aborted data
				Vertices.Empty();
				Normals.Empty();
				Octahedrons.Empty();
				UVs.Empty();
				VertexColors.Empty();
				VertexHeight.Empty();
				Slopes.Empty();
				ForestStrength.Empty();
				Biomes.Empty();
				FoliageBiomeIndices.Empty();
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
			SpawnFoliageComponent(FoliageRuntimeData[i]);
		}
	}
}

void UChunkComponent::SpawnFoliageComponent(FFoliageRuntimeDataS& Data)
{
	UInstancedStaticMeshComponent* Ismc = NewObject<UInstancedStaticMeshComponent>(FoliageActor, NAME_None, RF_Transient);
	Ismc->SetupAttachment(FoliageActor->GetRootComponent());
	Ismc->SetMobility(EComponentMobility::Movable);
	Ismc->SetGenerateOverlapEvents(false);
	Ismc->WorldPositionOffsetDisableDistance = Data.Foliage.WPODisableDistance;
	Ismc->SetEvaluateWorldPositionOffset(Data.Foliage.WPO);
	Ismc->bAffectDistanceFieldLighting = false;
	Ismc->InstanceEndCullDistance = Data.Foliage.CullingDistance;
	Ismc->bWorldPositionOffsetWritesVelocity = Data.Foliage.WPO;
	Ismc->bDisableCollision = true;
	Ismc->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
	Ismc->SetCanEverAffectNavigation(false);

	int DistanceStep = PlanetData->MaxRecursionLevel - recursionLevel;

	UStaticMesh* SelectedMesh = Data.Foliage.FoliageMesh;
	bool SelectedWPO = Data.Foliage.WPO;
	bool IsLOD = false;

	// Check for Multi-LODs
	if (Data.Foliage.LODs.Num() > 0)
	{
		int BestStep = -1;
		for (const FFoliageLOD& LOD : Data.Foliage.LODs)
		{
			if (LOD.Mesh && DistanceStep >= LOD.ActivationStep)
			{
				if (LOD.ActivationStep > BestStep)
				{
					BestStep = LOD.ActivationStep;
					SelectedMesh = LOD.Mesh;
					SelectedWPO = LOD.EnableWPO;
					IsLOD = true;
				}
			}
		}
	}
	
	Ismc->SetStaticMesh(SelectedMesh);
	Ismc->SetEvaluateWorldPositionOffset(SelectedWPO);

	// Apply Shadows based on Distance
	if (Data.Foliage.ShadowDisableDistance <= DistanceStep && Data.Foliage.ShadowDisableDistance != 0)
	{
		Ismc->CastShadow = false;
		Ismc->bCastContactShadow = false;
		Ismc->SetVisibleInRayTracing(false);
	}

	if (IsLOD)
	{
		Ismc->bAffectDistanceFieldLighting = false;
		Ismc->SetVisibleInRayTracing(false);
		Ismc->bDisableCollision = true;
	}
	else
	{
		// Base Mesh Collision Logic
		if (Data.Foliage.CollisionEnableDistance >= DistanceStep && Data.Foliage.CollisionEnableDistance != 0 && Data.Foliage.Collision == true)
		{
			Ismc->bDisableCollision = false;
		}
	}

	//hismc->bIsAsyncBuilding = true;
	Ismc->SetRelativeTransform(FTransform(ChunkSMC->GetRelativeRotation(), ChunkLocation, FVector(1.0f, 1.0f, 1.0f)));
	Ismc->AddInstances(Data.LocalFoliageTransforms, false, false, false);
	Ismc->RegisterComponent();
	Data.Ismc = Ismc;
}


void UChunkComponent::GenerateChunk()
{
	ChunkStatus = EChunkStatus::GENERATING;
	
	// Reset
	Vertices.Empty();
	Normals.Empty();
	Octahedrons.Empty();
	UVs.Empty();
	VertexColors.Empty();
	VertexHeight.Empty();

	// Init Stats
	VerticesAmount = ChunkQuality + 1;
	Size = ChunkSize / ChunkQuality;



	

	if (GenerateRayTracingProxy && recursionLevel >= PlanetData->MaxRecursionLevel - 1)
	{
		bRaytracing = true;
	}
	if (GenerateCollisions && recursionLevel >= PlanetData->MaxRecursionLevel - CollisionDisableDistance)
	{
		Collisions = true;
	}

	
	// Init BiomeMap
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

	
	// Compute Params
	FPlanetComputeShaderDispatchParams Params(VerticesAmount, VerticesAmount, 1);
	Params.ChunkLocation = FVector3f(PlanetSpaceLocation);
	Params.ChunkRotation = PlanetSpaceRotation;
	Params.ChunkOriginLocation = FVector3f(ChunkLocation);
	Params.ChunkSize = ChunkSize;
	Params.PlanetRadius = PlanetData->PlanetRadius;
	Params.NoiseHeight = PlanetData->NoiseHeight;
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
	ForestStrength.Reserve(VerticesAmount * VerticesAmount);
	
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

					float noise = ((FVector(Vertices[x + y * VerticesAmount]) + ChunkLocation).Size() - PlanetData->PlanetRadius) / PlanetData->NoiseHeight;
				
					float noiseheight = noise * PlanetData->NoiseHeight;
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
					

					if (PlanetData->BiomeData[BiomeIndex].GenerateForest == true && PlanetData->BiomeData[BiomeIndex].ForestFoliageData != nullptr)
					{
						float VertexRandom = HashFVectorToVector2D(FVector(Vertex) + ChunkLocation, 0, 255, Size * 2.0f).X;
						//float VertexRandom = (FMath::PerlinNoise3D((FVector(Vertex) + ChunkLocation) * 0.001f) + 1) / 2.0f * 255.0f;
						if ((ShaderForestNoise <= VertexRandom || noiseheight > PlanetData->BiomeData[BiomeIndex].ForestFoliageData->FoliageList[0].MaxHeight || noiseheight < PlanetData->BiomeData[BiomeIndex].ForestFoliageData->FoliageList[0].MinHeight) && PlanetData->BiomeData[BiomeIndex].FoliageData != nullptr)
						{
							//FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].FoliageData);
							ShaderForestNoise = 0;
						}
						else if (PlanetData->BiomeData[BiomeIndex].ForestFoliageData != nullptr)
						{
							//FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].ForestFoliageData);
						}
					}
					else
					{
						//FoliageBiomes.AddUnique(PlanetData->BiomeData[BiomeIndex].FoliageData);
						ShaderForestNoise = 0;
					}
					VertexColors.Add(FColor(OutputVCVal[(x + y * VerticesAmount) * 3], ShaderForestNoise, BiomeIndex, 0));
					
					VertexHeight.Add(noiseheight);
					Slopes.Add(0);
					if (PlanetData->BiomeData[BiomeIndex].FoliageData != nullptr || PlanetData->BiomeData[BiomeIndex].ForestFoliageData != nullptr)
					{
						FoliageBiomeIndices.Add(BiomeIndex);
					}
					ForestStrength.Add(OutputVCVal[(x + y * VerticesAmount) * 3 + 1]);
					Biomes.Add(BiomeIndex);
					
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
			for (uint8 BiomeIdx : FoliageBiomeIndices)
			{
				const FBiomeDataS& FoliageBiome = PlanetData->BiomeData[BiomeIdx];
				for (UFoliageData* FoliageData : { FoliageBiome.FoliageData, FoliageBiome.ForestFoliageData })
				{
					if (!FoliageData) continue;
					
					for (const FFoliageListS& Foliage : FoliageData->FoliageList)
					{
						if (AbortAsync == true)
						{
							GenerationComplete();
							return;
						}
				
						if (Foliage.FoliageDistance >= PlanetData->MaxRecursionLevel - recursionLevel)
						{
							FFoliageRuntimeDataS RuntimeData;
							
							float Density = Foliage.FoliageDensity;
						
						
							if (Foliage.ScalableDensity == true)
							{
								Density = Density * FoliageDensityScale;
							}

							if (Foliage.LODs.Num() > 0)
							{
								int DistanceStep = PlanetData->MaxRecursionLevel - recursionLevel;
								int BestStep = -1;
								float CurrentLODDensityScale = 1.0f;

								for (const FFoliageLOD& LOD : Foliage.LODs)
								{
									if (DistanceStep >= LOD.ActivationStep)
									{
										if (LOD.ActivationStep > BestStep)
										{
											BestStep = LOD.ActivationStep;
											CurrentLODDensityScale = LOD.DensityScale;
										}
									}
								}
								Density = Density * CurrentLODDensityScale;
							}

							//LocalFoliageTransform.Empty();
							//Density = 1;
							float FoliageSpacing = (10000 / Density);
							int LocalDensity = FMath::Max(ChunkSize / FoliageSpacing + 2.01, 1);
						
							for (int y = 0; y < LocalDensity; y++)
							{
								for (int z = 0; z < LocalDensity; z++)
								{
								
									float xlocalpos = y * FoliageSpacing;
									float ylocalpos = z * FoliageSpacing;
								
									FVector PlanetSpaceFoliageLocation;
									PlanetSpaceFoliageLocation.X = QuantizeDouble(PlanetSpaceLocation.X, FoliageSpacing);
									PlanetSpaceFoliageLocation.Y = QuantizeDouble(PlanetSpaceLocation.Y, FoliageSpacing);
									PlanetSpaceFoliageLocation.Z = QuantizeDouble(PlanetSpaceLocation.Z, FoliageSpacing);
								
									PlanetSpaceFoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceFoliageLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));
								
								
								
									FVector2f localpos = PlanetData->InversePlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, PlanetSpaceFoliageLocation);
									if (localpos.X > ChunkSize || localpos.Y > ChunkSize || localpos.X <= 0 || localpos.Y <= 0)
									{
										continue;
									}
								
									xlocalpos = localpos.X;
									ylocalpos = localpos.Y;
								
									PlanetSpaceFoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));

									//uint32 SeedValue = FCrc::MemCrc32(&NormalizedfoliageLocation, sizeof(FVector));
									FRandomStream ScaleStream(GetTypeHash(PlanetSpaceFoliageLocation));

									FVector2d NewLocalPos = HashFVectorToVector2D(PlanetSpaceFoliageLocation, -FoliageSpacing / 2, FoliageSpacing / 2, 10.0f);
									xlocalpos = (NewLocalPos.X + xlocalpos);
									ylocalpos = (NewLocalPos.Y + ylocalpos);
									
									PlanetSpaceFoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));

									int vertexX = FMath::RoundToInt(FMath::Clamp(xlocalpos / ChunkSize,0,1) * float(ChunkQuality));
									int vertexY = FMath::RoundToInt(FMath::Clamp(ylocalpos / ChunkSize,0,1) * float(ChunkQuality));
									int vertexIndex = vertexX + vertexY * VerticesAmount;
									vertexIndex = FMath::Clamp(vertexIndex, 0, Vertices.Num() - 1);
								
									float VertexRandom = abs(NewLocalPos.X / FoliageSpacing * 255.0f);
									
									//Forest strength average from surrounding vertices
									float Forest = ForestStrength[vertexIndex];
									int32 Count = 1;

									if (vertexX + 1 < VerticesAmount)
									{
										Forest += ForestStrength[vertexIndex + 1];
										Count++;
									}
									if (vertexY + 1 < VerticesAmount)
									{
										Forest += ForestStrength[vertexIndex + VerticesAmount];
										Count++;
									}
									if (vertexX + 1 < VerticesAmount && vertexY + 1 < VerticesAmount)
									{
										Forest += ForestStrength[vertexIndex + 1 + VerticesAmount];
										Count++;
									}

									Forest /= Count;
								
									if (Forest >= VertexRandom && FoliageData == FoliageBiome.FoliageData)
									{
										continue;
									}

									if (Forest < VertexRandom && FoliageData == FoliageBiome.ForestFoliageData)
									{
										continue;
									}

									if (PlanetData->BiomeData[Biomes[vertexIndex]].FoliageData == FoliageData && FoliageData == FoliageBiome.ForestFoliageData)
									{
										continue;
									}
									
									if (PlanetData->BiomeData[Biomes[vertexIndex]].ForestFoliageData == FoliageData && FoliageData == FoliageBiome.FoliageData)
									{
										continue;
									}

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

									if (Foliage.AbsoluteHeight == true)
									{
										Height = 0;
									}
					
		

									PlanetSpaceFoliageLocation *= (PlanetData->PlanetRadius + Height - Foliage.DepthOffset);
									PlanetSpaceFoliageLocation = PlanetSpaceFoliageLocation - (ChunkLocation - PlanetSpaceLocation);
						
					

									if (Height > Foliage.MaxHeight || Height < Foliage.MinHeight)
									{
										continue;
									}

									if (Slopes[vertexIndex] > Foliage.MaxSlope || Slopes[vertexIndex] < Foliage.MinSlope)
									{
										continue;
									}
					
					
						

									FRotator Rot = FRotator(0, 0, 0);
				
									if (Foliage.AlignToTarrain == false)
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
											ScaleStream.FRandRange(Foliage.minScale, Foliage.maxScale),
											ScaleStream.FRandRange(Foliage.minScale, Foliage.maxScale),
											ScaleStream.FRandRange(Foliage.minScale, Foliage.maxScale))
											);
																			
									RuntimeData.LocalFoliageTransforms.Add(transform);
								
								}
							}
							RuntimeData.Foliage = Foliage;
							FoliageRuntimeData.Add(RuntimeData);
				
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
		
		NaniteBuilder.PositionPrecision = -(PlanetData->MaxRecursionLevel - recursionLevel) + 3;
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
	MaterialInst->SetScalarParameterValue("recursionLevel", PlanetData->MaxRecursionLevel - recursionLevel);
	MaterialInst->SetScalarParameterValue("PlanetRadius", PlanetData->PlanetRadius);
	MaterialInst->SetScalarParameterValue("NoiseHeight", PlanetData->NoiseHeight);
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
		MaterialInst->SetScalarParameterValueByInfo(NoiseHeightInfo, PlanetData->NoiseHeight);

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
		MaterialInst->SetScalarParameterValueByInfo(RecursionInfo, PlanetData->MaxRecursionLevel - recursionLevel);
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
	
	
	if (recursionLevel != PlanetData->MaxRecursionLevel)
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

		// Disable ray tracing support BEFORE releasing to ensure RT resources are cleaned
		ChunkStaticMesh->bSupportRayTracing = false;

		UStaticMesh* LocalStaticMesh = ChunkStaticMesh;
		LocalStaticMesh->ReleaseResources();
		FStaticMeshRenderData* LocalRenderData = ChunkStaticMesh->GetRenderData();
		UMaterialInstanceDynamic* LocalMaterialInst = MaterialInst;

		ENQUEUE_RENDER_COMMAND(ReleaseNaniteResources)(
			[LocalStaticMesh, LocalRenderData, LocalMaterialInst](FRHICommandListImmediate& RHICmdList)
			{
				if (LocalRenderData)
				{
					LocalRenderData->ReleaseResources();
					LocalStaticMesh->NaniteSettings.bEnabled = false;
					LocalRenderData->NaniteResourcesPtr->ReleaseResources();
					LocalRenderData->NaniteResourcesPtr.Reset();
				}
				
				// Defer destruction to game thread AFTER render resources are released
				AsyncTask(ENamedThreads::GameThread, [LocalStaticMesh, LocalMaterialInst]()
				{
					LocalStaticMesh->ConditionalBeginDestroy();
					if (LocalMaterialInst)
					{
						LocalMaterialInst->ConditionalBeginDestroy();
					}
				});
			});

		MaterialInst = nullptr;
		ChunkStaticMesh = nullptr;
	}

	// Reset ray tracing flag for potential chunk reuse
	bRaytracing = false;

	// Clean up BiomeMap render target
	if (BiomeMap != nullptr)
	{
		BiomeMap->ConditionalBeginDestroy();
		BiomeMap = nullptr;
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
	WaterChunk = nullptr;
	

	//clear all arrays
	Vertices.Empty();
	Slopes.Empty();
	VertexHeight.Empty();
	FoliageBiomeIndices.Empty();
	Normals.Empty();
	Octahedrons.Empty();
	UVs.Empty();
	VertexColors.Empty();
	ForestStrength.Empty();
	Biomes.Empty();
	FoliageRuntimeData.Empty();
	GPUBiomeData = nullptr;

	//clear biome map
	if (BiomeMap != nullptr)
    {
        BiomeMap->ConditionalBeginDestroy();
    }
	ConditionalBeginDestroy();
}


