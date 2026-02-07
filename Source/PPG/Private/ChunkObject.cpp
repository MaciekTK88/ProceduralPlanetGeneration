// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski

#include "ChunkObject.h"
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
#include "Materials/Material.h"
#include "ComputeShader/Public/PlanetComputeShader/PlanetComputeShader.h"


UChunkObject::UChunkObject()
{
	
}



void UChunkObject::GenerationComplete()
{
	if (IsInGameThread())
	{
		if (bAbortAsync == false)
		{
			// Rate-limit component assignment
			ChunkStatus = EChunkStatus::PENDING_ASSIGN;
		}
		else
		{
			// Cleanup aborted data - including any objects created during generation
			FreeComponents();
			ChunkStatus = EChunkStatus::ABORTED;
		}

	}
	else
	{
	TWeakObjectPtr<UChunkObject> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis]()
	{
		UChunkObject* StrongThis = WeakThis.Get();
		if (!StrongThis)
		{
			return;
		}

		if (StrongThis->bAbortAsync == false)
		{
			// Rate-limit component assignment
			StrongThis->ChunkStatus = EChunkStatus::PENDING_ASSIGN;
		}
		else
		{
			// Cleanup aborted data - including any objects created during generation
			StrongThis->FreeComponents();
			StrongThis->ChunkStatus = EChunkStatus::ABORTED;
		}
	});
	}
}


void UChunkObject::SetFoliageActor(AActor* NewFoliageActor)
{
	FoliageActor = NewFoliageActor;
}

void UChunkObject::UploadFoliageData()
{
	for (int32 i = 0; i < FoliageRuntimeData.Num(); i++)
	{
		if (bGenerateFoliage == true && FoliageRuntimeData[i].LocalFoliageTransforms.Num() > 0)
		{
			SpawnFoliageComponent(FoliageRuntimeData[i]);
		}
	}
}

void UChunkObject::SpawnFoliageComponent(FFoliageRuntimeData& Data)
{
	if (!FoliageActor)
	{
		return;
	}

	UInstancedStaticMeshComponent* Ismc = NewObject<UInstancedStaticMeshComponent>(FoliageActor, NAME_None, RF_Transient);
	Ismc->SetupAttachment(FoliageActor->GetRootComponent());
	Ismc->SetMobility(EComponentMobility::Movable);
	Ismc->SetGenerateOverlapEvents(false);
	Ismc->WorldPositionOffsetDisableDistance = Data.Foliage.WPODisableDistance;
	Ismc->SetEvaluateWorldPositionOffset(Data.Foliage.bEnableWPO);
	Ismc->bAffectDistanceFieldLighting = false;
	Ismc->InstanceEndCullDistance = Data.Foliage.CullingDistance;
	Ismc->bWorldPositionOffsetWritesVelocity = Data.Foliage.bEnableWPO;
	Ismc->bDisableCollision = true;
	Ismc->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
	Ismc->SetCanEverAffectNavigation(false);

	TObjectPtr<UStaticMesh> SelectedMesh = Data.Foliage.FoliageMesh;
	bool SelectedWPO = Data.Foliage.bEnableWPO;
	bool IsLOD = false;
	
	int DistanceStep = PlanetData->MaxRecursionLevel - RecursionLevel;

	// Check for Multi-LODs
	if (Data.Foliage.LODs.Num() > 0)
	{
		int BestStep = -1;
		for (const FFoliageLOD& LOD : Data.Foliage.LODs)
		{
			if (LOD.Mesh && DistanceStep >= LOD.ActivationDistance)
			{
				if (LOD.ActivationDistance > BestStep)
				{
					BestStep = LOD.ActivationDistance;
					SelectedMesh = LOD.Mesh;
					SelectedWPO = LOD.bEnableWPO;
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
		if (Data.Foliage.CollisionDisableDistance >= DistanceStep && Data.Foliage.CollisionDisableDistance != 0 && Data.Foliage.bEnableCollision == true)
		{
			Ismc->bDisableCollision = false;
		}
	}

	Ismc->SetRelativeTransform(FTransform(ChunkSMC->GetRelativeRotation(), ChunkOriginLocation, FVector(1.0f, 1.0f, 1.0f)));
	Ismc->AddInstances(Data.LocalFoliageTransforms, false, false, false);
	Ismc->RegisterComponent();
	Data.Ismc = Ismc;
}


void UChunkObject::GenerateChunk()
{
	ChunkStatus = EChunkStatus::GENERATING;

	// Initialize chunk metrics
	VerticesCount = ChunkQuality + 1;
	TriangleSize = ChunkSize / ChunkQuality;

	// Enable raytracing proxy for high-detail chunks
	if (bGenerateRayTracingProxy && RecursionLevel >= PlanetData->MaxRecursionLevel - 1)
	{
		bRayTracing = true;
	}
	
	// Enable collisions based on recursion depth
	if (bGenerateCollisions && RecursionLevel >= PlanetData->MaxRecursionLevel - CollisionDisableDistance)
	{
		bCollisions = true;
	}

	//--------------------------------------------------------------------------
	// Create BiomeMap Render Target
	// Layout: Top half = Material indices + elevation, Bottom half = Strengths
	//--------------------------------------------------------------------------
	BiomeMap = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	BiomeMap->bCanCreateUAV = true;
	BiomeMap->bSupportsUAV = true;
	BiomeMap->InitCustomFormat(VerticesCount, VerticesCount * 2, PF_R8G8B8A8, true);
	BiomeMap->RenderTargetFormat = RTF_RGBA8;
	BiomeMap->bAutoGenerateMips = false;
	BiomeMap->SRGB = false;
	BiomeMap->Filter = TF_Nearest;
	BiomeMap->AddressX = TA_Clamp;
	BiomeMap->AddressY = TA_Clamp;
	BiomeMap->UpdateResourceImmediate();

	//--------------------------------------------------------------------------
	// Setup Compute Shader Parameters
	//--------------------------------------------------------------------------
	FPlanetComputeShaderDispatchParams Params(VerticesCount, VerticesCount, 1);
	Params.ChunkLocation = FVector3f(PlanetSpaceLocation);
	Params.ChunkRotation = PlanetSpaceRotation;
	Params.ChunkOriginLocation = FVector3f(ChunkOriginLocation);
	Params.ChunkSize = ChunkSize;
	Params.PlanetRadius = PlanetData->PlanetRadius;
	Params.NoiseHeight = PlanetData->NoiseHeight;
	Params.BiomeMap = BiomeMap;
	Params.CurveAtlas = PlanetData->CurveAtlas;
	Params.BiomeDataTexture = PlanetData->GPUBiomeData;
	Params.BiomeCount = PlanetData->BiomeData.Num();
	Params.ChunkQuality = ChunkQuality;
	Params.X = VerticesCount;
	Params.Y = VerticesCount;
	Params.Z = 1;

	// Use generation material or fallback to default
	Params.GenerationMaterial = PlanetData->GenerationMaterial 
		? PlanetData->GenerationMaterial 
		: UMaterial::GetDefaultMaterial(MD_Surface);
	Params.MaterialRenderProxy = Params.GenerationMaterial->GetRenderProxy();

	// Pre-allocate vertex buffers
	const int32 TotalVertices = VerticesCount * VerticesCount;
	Vertices.Reserve(TotalVertices);
	UVs.Reserve(TotalVertices);
	VertexColors.Reserve(TotalVertices);
	VertexHeight.Reserve(VerticesCount * VerticesCount);
	Slopes.Reserve(VerticesCount * VerticesCount);
	ForestStrength.Reserve(VerticesCount * VerticesCount);
	
	FPlanetComputeShaderReadback Readback;
	FPlanetComputeShaderInterface::Dispatch(Params, [this](FPlanetComputeShaderReadback Readback)
	{
		// Validate the readback has valid data before proceeding
		if (Readback.NumVertices > 0 && Readback.OutputBuffer.IsValid() && Readback.OutputVCBuffer.IsValid())
		{
			GPUReadback = Readback;
			ChunkStatus = EChunkStatus::WAITING_FOR_GPU;
		}
		else
		{
			// GPU dispatch failed or returned empty data - abort this chunk
			UE_LOG(LogTemp, Warning, TEXT("Chunk GPU dispatch returned invalid readback (NumVertices: %d, OutputBuffer: %s, OutputVCBuffer: %s). Aborting chunk."),
				Readback.NumVertices,
				Readback.OutputBuffer.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
				Readback.OutputVCBuffer.IsValid() ? TEXT("Valid") : TEXT("Invalid"));
			bAbortAsync = true;
			GenerationComplete();
		}
	});
}

void UChunkObject::TickGPUReadback()
{
	if (ChunkStatus != EChunkStatus::WAITING_FOR_GPU && bAbortAsync == false)
	{
		return;
	}

	if (bAbortAsync)
	{
		// Clear our reference to the readback buffers
		GPUReadback.OutputBuffer.Reset();
		GPUReadback.OutputVCBuffer.Reset();
		GenerationComplete();
		return;
	}

	if (GPUReadback.OutputBuffer.IsValid() && GPUReadback.OutputVCBuffer.IsValid() &&
		GPUReadback.OutputBuffer->IsReady() && GPUReadback.OutputVCBuffer->IsReady())
	{
		// Buffers are ready, dispatch read and process
		ChunkStatus = EChunkStatus::GENERATING;

		ENQUEUE_RENDER_COMMAND(ReadChunkData)(
			[this, Readback = GPUReadback](FRHICommandListImmediate& RHICmdList)
			{
				const int32 NumVertices = Readback.NumVertices;
				
				// Safety check: abort if no data to read
				if (NumVertices <= 0)
				{
					TWeakObjectPtr<UChunkObject> WeakThis(this);
					AsyncTask(ENamedThreads::GameThread, [WeakThis]()
					{
						if (UChunkObject* StrongThis = WeakThis.Get())
						{
							StrongThis->bAbortAsync = true;
							StrongThis->GenerationComplete();
						}
					});
					return;
				}
				
				// Read position buffer (3 floats per vertex: x, y, z)
				const int32 NumPositionFloats = NumVertices * 3;
				float* Buffer = (float*)Readback.OutputBuffer->Lock(NumPositionFloats * sizeof(float));
				TArray<float> OutVal;
				OutVal.SetNumUninitialized(NumPositionFloats);
				FMemory::Memcpy(OutVal.GetData(), Buffer, NumPositionFloats * sizeof(float));
				Readback.OutputBuffer->Unlock();

				// Read vertex color buffer (4 bytes per vertex: RGBA)
				const int32 NumColorBytes = NumVertices * 4;
				uint8* BufferVC = (uint8*)Readback.OutputVCBuffer->Lock(NumColorBytes * sizeof(uint8));
				TArray<uint8> OutValVC;
				OutValVC.SetNumUninitialized(NumColorBytes);
				FMemory::Memcpy(OutValVC.GetData(), BufferVC, NumColorBytes * sizeof(uint8));
				Readback.OutputVCBuffer->Unlock();

				TWeakObjectPtr<UChunkObject> WeakThis(this);
				AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [WeakThis, OutVal = MoveTemp(OutVal), OutValVC = MoveTemp(OutValVC)]()
				{
					UChunkObject* StrongThis = WeakThis.Get();
					if (!StrongThis)
					{
						return;
					}

					if (StrongThis->GetAbortAsync())
					{
						StrongThis->GenerationComplete();
						return;
					}
					StrongThis->ProcessChunkData(OutVal, OutValVC);
				});
			});
			
		// Clear our reference to the readback buffers
		GPUReadback.OutputBuffer.Reset();
		GPUReadback.OutputVCBuffer.Reset();
	}
}

void UChunkObject::SetSharedResources(TArray<TObjectPtr<UStaticMeshComponent>>* InChunkSMCPool, TArray<TObjectPtr<UInstancedStaticMeshComponent>>* InFoliageISMCPool, TArray<TObjectPtr<UStaticMeshComponent>>* InWaterSMCPool, TArray<uint32>* InTriangles)
{
	ChunkSMCPool = InChunkSMCPool;
	FoliageISMCPool = InFoliageISMCPool;
	WaterSMCPool = InWaterSMCPool;
	Triangles = InTriangles;
}

void UChunkObject::InitializeChunk(int InChunkQuality, float InChunkSize, int32 InRecursionLevel, FVector InChunkLocation, FVector InChunkOriginLocation, FIntVector InPlanetSpaceRotation, float InChunkMaxHeight, uint8 InMaterialLayersNum, UStaticMesh* InCloseWaterMesh, UStaticMesh* InFarWaterMesh)
{
	ChunkQuality = InChunkQuality;
	ChunkSize = InChunkSize;
	RecursionLevel = InRecursionLevel;
	ChunkOriginLocation = InChunkOriginLocation;
	PlanetSpaceLocation = InChunkLocation;
	PlanetSpaceRotation = InPlanetSpaceRotation;
	ChunkMaxHeight = InChunkMaxHeight;
	MaterialLayersNum = InMaterialLayersNum;
	CloseWaterMesh = InCloseWaterMesh;
	FarWaterMesh = InFarWaterMesh;
}

void UChunkObject::ProcessChunkData(const TArray<float>& OutputVal, const TArray<uint8>& OutputVCVal)
{
	float LocalChunkMaxHeight = 0.0f;

	if (OutputVal.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Chunk Process: OutputVal Empty!"));
		GenerationComplete();
		return;
	}

	for (int y = 0; y < VerticesCount; y++)
	{
		for (int x = 0; x < VerticesCount; x++)
		{
			FVector3f Vertex = FVector3f(OutputVal[(x + y * VerticesCount) * 3], OutputVal[(x + y * VerticesCount) * 3 + 1], OutputVal[(x + y * VerticesCount) * 3 + 2]);
			Vertices.Add(Vertex);
			
			UVs.Add(FVector2f((float)x / (float)VerticesCount, (float)y / (float)VerticesCount));

			float Noise = ((FVector(Vertices[x + y * VerticesCount]) + ChunkOriginLocation).Size() - PlanetData->PlanetRadius) / PlanetData->NoiseHeight;

			float NoiseHeight = Noise * PlanetData->NoiseHeight;
			if (NoiseHeight > LocalChunkMaxHeight)
			{
				LocalChunkMaxHeight = NoiseHeight;
			}

			if (NoiseHeight < ChunkMinHeight)
			{
				ChunkMinHeight = NoiseHeight;
			}
			
			uint8 ShaderForestNoise = OutputVCVal[(x + y * VerticesCount) * 4 + 1];
			
			// 4th component (Alpha) is BiomeIndex
			uint8 BiomeIndex = OutputVCVal[(x + y * VerticesCount) * 4 + 3];

			if (PlanetData->BiomeData.Num() - 1 < BiomeIndex) {
				BiomeIndex = 0; // Safety clamp
			}
			
			VertexColors.Add(FColor(OutputVCVal[(x + y * VerticesCount) * 4], ShaderForestNoise, OutputVCVal[(x + y * VerticesCount) * 4 + 2], BiomeIndex));
			Slopes.Add(0); // Calculated later
			VertexHeight.Add(NoiseHeight);
			if (PlanetData->BiomeData[BiomeIndex].FoliageData != nullptr || PlanetData->BiomeData[BiomeIndex].ForestFoliageData != nullptr)
			{
				FoliageBiomeIndices.Add(BiomeIndex);
			}
			ForestStrength.Add(ShaderForestNoise);
			Biomes.Add(BiomeIndex);

		}
	}
	ChunkMaxHeight = LocalChunkMaxHeight;
	
	CompleteChunkGeneration();


}
void UChunkObject::CompleteChunkGeneration()
{
	if (bAbortAsync == true)
	{
		GenerationComplete();
		return;
	}

	for (int y = 0; y < VerticesCount; y++)
	{
		if (y == VerticesCount - 1)
		{
			for (int x = 0; x < VerticesCount - 1; x++)
			{
				FVector3f v1 = Vertices[x - VerticesCount + y * VerticesCount];
				FVector3f v2 = Vertices[x + 1 + y * VerticesCount];
				FVector3f v3 = Vertices[x + y * VerticesCount];
				FVector3f normal = FVector3f::CrossProduct(v3 - v2, v1 - v3).GetSafeNormal();

				Normals.Add(normal);
				Octahedrons.Add(FVoxelOctahedron(normal));

				float c1 = VertexHeight[x - VerticesCount + y * VerticesCount];
				float c2 = VertexHeight[x + 1 + y * VerticesCount];
				float c3 = VertexHeight[x + y * VerticesCount];

				float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
				Slopes[x + y * VerticesCount] = slope;

			}
			int x = VerticesCount - 1;
			FVector3f v1 = Vertices[x - VerticesCount + y * VerticesCount];
			FVector3f v2 = Vertices[x - 1 + y * VerticesCount];
			FVector3f v3 = Vertices[x + y * VerticesCount];
			FVector3f normal = FVector3f::CrossProduct(v2 - v3, v1 - v3).GetSafeNormal();

			Normals.Add(normal);
			Octahedrons.Add(FVoxelOctahedron(normal));


			float c1 = VertexHeight[x - VerticesCount + y * VerticesCount];
			float c2 = VertexHeight[x - 1 + y * VerticesCount];
			float c3 = VertexHeight[x + y * VerticesCount];

			float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
			Slopes[x + y * VerticesCount] = slope;
		}
		else
		{
			for (int x = 0; x < VerticesCount - 1; x++)
			{

				FVector3f v1 = Vertices[x + VerticesCount + y * VerticesCount];
				FVector3f v2 = Vertices[x + 1 + y * VerticesCount];
				FVector3f v3 = Vertices[x + y * VerticesCount];
				FVector3f normal = FVector3f::CrossProduct(v3 - v2, v3 - v1).GetSafeNormal();

				Normals.Add(normal);
				Octahedrons.Add(FVoxelOctahedron(normal));


				float c1 = VertexHeight[x + VerticesCount + y * VerticesCount];
				float c2 = VertexHeight[x + 1 + y * VerticesCount];
				float c3 = VertexHeight[x + y * VerticesCount];

				float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
				Slopes[x + y * VerticesCount] = slope;
			}
			int x = VerticesCount - 1;
			FVector3f v1 = Vertices[x + VerticesCount + y * VerticesCount];
			FVector3f v2 = Vertices[x - 1 + y * VerticesCount];
			FVector3f v3 = Vertices[x + y * VerticesCount];
			FVector3f normal = FVector3f::CrossProduct(v2 - v3, v3 - v1).GetSafeNormal();

			Normals.Add(normal);
			Octahedrons.Add(FVoxelOctahedron(normal));


			float c1 = VertexHeight[x + VerticesCount + y * VerticesCount];
			float c2 = VertexHeight[x - 1 + y * VerticesCount];
			float c3 = VertexHeight[x + y * VerticesCount];

			float slope = FMath::Clamp(FMath::Sqrt((float)((c1 - c2) / ChunkSize) * ((c1 - c2) / ChunkSize) + ((c1 - c3) / ChunkSize) * ((c1 - c3) / ChunkSize)) * 2500.0f, 0, 255);
			Slopes[x + y * VerticesCount] = slope;
		}
	}

	if (bAbortAsync == true)
	{
		GenerationComplete();
		return;
	}
	
	// Foliage Processing
	if (bGenerateFoliage && ForestStrength.Num() > 0 && Vertices.Num() > 0)
	{
		for (uint8 BiomeIdx : FoliageBiomeIndices)
		{
			if (PlanetData->BiomeData.IsValidIndex(BiomeIdx))
			{
				const FBiomeData& FoliageBiome = PlanetData->BiomeData[BiomeIdx];
				for (UFoliageData* FoliageData : { FoliageBiome.FoliageData, FoliageBiome.ForestFoliageData })
				{
					if (!FoliageData) continue;
					
					if (bAbortAsync == true)
					{
						GenerationComplete();
						return;
					}

					for (const FFoliageList& Foliage : FoliageData->FoliageList)
					{
						if (Foliage.SpawnDistance >= PlanetData->MaxRecursionLevel - RecursionLevel)
						{
							FFoliageRuntimeData RuntimeData;

							float Density = Foliage.FoliageDensity;


							if (Foliage.bScalableDensity == true)
							{
								Density = Density * FoliageDensityScale;
							}

							if (Foliage.LODs.Num() > 0)
							{
								int DistanceStep = PlanetData->MaxRecursionLevel - RecursionLevel;
								int BestStep = -1;
								float CurrentLODDensityScale = 1.0f;

								for (const FFoliageLOD& LOD : Foliage.LODs)
								{
									if (DistanceStep >= LOD.ActivationDistance)
									{
										if (LOD.ActivationDistance > BestStep)
										{
											BestStep = LOD.ActivationDistance;
											CurrentLODDensityScale = LOD.DensityScale;
										}
									}
								}
								Density = Density * CurrentLODDensityScale;
							}
							
							float FoliageSpacing = (10000 / Density);
							int LocalDensity = FMath::Max(ChunkSize / FoliageSpacing + 2.01, 1);
							long TotalInstances = (long)LocalDensity * (long)LocalDensity;

							// Protection against hanging the editor on low recursion levels from massive foliage count
							if (TotalInstances > 250000)
							{
#if WITH_EDITOR
								GEngine->AddOnScreenDebugMessage((uint64)this, 5.f, FColor::Yellow, FString::Printf(TEXT("Foliage generation skipped for chunk (Too many instances: %ld). Increase Recursion Level or Decrease Density."), TotalInstances));
#endif
								continue;
							}
							
							for (int y = 0; y < LocalDensity; y++)
							{
								for (int z = 0; z < LocalDensity; z++)
								{

									float xlocalpos = y * FoliageSpacing;
									float ylocalpos = z * FoliageSpacing;

									// Calculate the initial foliage position without quantization
									FVector PlanetSpaceFoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(xlocalpos, ylocalpos, 0.0f));

									// Quantize the actual foliage position to create a chunk-independent global grid
									PlanetSpaceFoliageLocation.X = QuantizeDouble(PlanetSpaceFoliageLocation.X, FoliageSpacing);
									PlanetSpaceFoliageLocation.Y = QuantizeDouble(PlanetSpaceFoliageLocation.Y, FoliageSpacing);
									PlanetSpaceFoliageLocation.Z = QuantizeDouble(PlanetSpaceFoliageLocation.Z, FoliageSpacing);
									
									FVector2f localpos = PlanetData->InversePlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, PlanetSpaceFoliageLocation);
									xlocalpos = localpos.X;
									ylocalpos = localpos.Y;
									
								// Generate a random stream based on the quantized position for consistent randomness
								FRandomStream ScaleStream(GetTypeHash(PlanetSpaceFoliageLocation));

								FVector2d NewLocalPos = HashFVectorToVector2D(PlanetSpaceFoliageLocation, -FoliageSpacing / 2, FoliageSpacing / 2, FoliageSpacing / 4.0f);
									
									// Base position for the cluster center (or single instance)
									float BaseXLocalPos = xlocalpos + NewLocalPos.X;
									float BaseYLocalPos = ylocalpos + NewLocalPos.Y;
									
								// Check if new position is within the current chunk bounds
								if (BaseXLocalPos > ChunkSize || BaseYLocalPos > ChunkSize || BaseXLocalPos < 0 || BaseYLocalPos < 0)
								{
									continue;
								}
									
								// Sample vertex data from the cluster center position for biome/forest/slope checks
								// This ensures cluster spawning decisions are consistent across chunks
								int centerVertexX = FMath::RoundToInt(FMath::Clamp(BaseXLocalPos / ChunkSize, 0, 1) * float(ChunkQuality));
								int centerVertexY = FMath::RoundToInt(FMath::Clamp(BaseYLocalPos / ChunkSize, 0, 1) * float(ChunkQuality));
								int centerVertexIndex = centerVertexX + centerVertexY * VerticesCount;
								centerVertexIndex = FMath::Clamp(centerVertexIndex, 0, Vertices.Num() - 1);

								// Get vertex random for density check
								float VertexRandom = abs(NewLocalPos.X / (FoliageSpacing / 2) * 255.0f);

								// Forest strength average from surrounding vertices
								float Forest = ForestStrength[centerVertexIndex];
								int32 Count = 1;

								if (centerVertexX + 1 < VerticesCount)
								{
									Forest += ForestStrength[centerVertexIndex + 1];
									Count++;
								}
								if (centerVertexY + 1 < VerticesCount)
								{
									Forest += ForestStrength[centerVertexIndex + VerticesCount];
									Count++;
								}
								if (centerVertexX + 1 < VerticesCount && centerVertexY + 1 < VerticesCount)
								{
									Forest += ForestStrength[centerVertexIndex + 1 + VerticesCount];
									Count++;
								}

								Forest /= Count;

								// Check forest density conditions
								if (Forest >= VertexRandom && FoliageData == FoliageBiome.FoliageData)
								{
									continue;
								}

								if (Forest < VertexRandom && FoliageData == FoliageBiome.ForestFoliageData)
								{
									continue;
								}

								// Check biome compatibility
								if (PlanetData->BiomeData[Biomes[centerVertexIndex]].FoliageData != FoliageData && PlanetData->BiomeData[Biomes[centerVertexIndex]].ForestFoliageData != FoliageData)
								{
									continue;
								}

								// Check slope constraint at cluster center
								if (Slopes[centerVertexIndex] > Foliage.MaxSlope || Slopes[centerVertexIndex] < Foliage.MinSlope)
								{
									continue;
								}

								int32 NumToSpawn = 1;
								if (Foliage.bEnableClustering)
								{
									NumToSpawn = ScaleStream.RandRange(Foliage.ClusterSizeMin, Foliage.ClusterSizeMax);
								}

								for (int32 InstanceIdx = 0; InstanceIdx < NumToSpawn; InstanceIdx++)
								{
									float CurrentXLocalPos = BaseXLocalPos;
									float CurrentYLocalPos = BaseYLocalPos;

									// If this is a cluster instance (and not the first one), apply random offset
									if (InstanceIdx > 0)
									{
										float Angle = ScaleStream.FRand() * 2.0f * PI;
										float Radius = ScaleStream.FRand() * Foliage.ClusterRadius;
										CurrentXLocalPos += FMath::Cos(Angle) * Radius;
										CurrentYLocalPos += FMath::Sin(Angle) * Radius;
									}

									// Sample height for THIS specific instance position
									int vertexX = FMath::RoundToInt(FMath::Clamp(CurrentXLocalPos / ChunkSize, 0, 1) * float(ChunkQuality));
									int vertexY = FMath::RoundToInt(FMath::Clamp(CurrentYLocalPos / ChunkSize, 0, 1) * float(ChunkQuality));
									int vertexIndex = vertexX + vertexY * VerticesCount;
									vertexIndex = FMath::Clamp(vertexIndex, 0, Vertices.Num() - 1);

									double Height = VertexHeight[vertexIndex];
									
									if (Foliage.bUseAbsoluteHeight == true)
									{
										Height = 0;
									}

									// Check height constraint for this instance
									if (Height > Foliage.MaxHeight || Height < Foliage.MinHeight)
									{
										continue;
									}

									// Recalculate PlanetSpaceLocation for this instance
									PlanetSpaceFoliageLocation = PlanetData->PlanetTransformLocation(PlanetSpaceLocation, PlanetSpaceRotation, FVector(CurrentXLocalPos, CurrentYLocalPos, 0.0f));

										//transform planetSpaceVertex to -1, 1 range
										float OriginalChunkSize = PlanetData->PlanetRadius * 2.0 / sqrt(2.0);
										FVector NormalizedFoliageLocation = PlanetSpaceFoliageLocation / (OriginalChunkSize / 2.0f);

										//apply deformation
										float deformation = 0.75;
										NormalizedFoliageLocation.X = tan(NormalizedFoliageLocation.X * PI * deformation / 4.0);
										NormalizedFoliageLocation.Y = tan(NormalizedFoliageLocation.Y * PI * deformation / 4.0);
										NormalizedFoliageLocation.Z = tan(NormalizedFoliageLocation.Z * PI * deformation / 4.0);

										NormalizedFoliageLocation.Normalize();
										
										// Restore PlanetSpaceFoliageLocation from normalized vector
										PlanetSpaceFoliageLocation = NormalizedFoliageLocation;

										// Use the height from the cluster center (already retrieved above)
										PlanetSpaceFoliageLocation *= (PlanetData->PlanetRadius + Height - Foliage.DepthOffset);
										PlanetSpaceFoliageLocation = PlanetSpaceFoliageLocation - (ChunkOriginLocation - PlanetSpaceLocation);

										FRotator Rot = FRotator(0, 0, 0);

										if (Foliage.bAlignToTerrain == false)
										{
											Rot = UKismetMathLibrary::FindLookAtRotation(FVector(0, 0, 0), PlanetSpaceFoliageLocation);
											Rot = FRotator(Rot.Pitch - 90, Rot.Yaw, Rot.Roll);
										}
										else if (Vertices.IsValidIndex(vertexIndex) && Normals.IsValidIndex(vertexIndex))
										{
											FVector3f v1 = Vertices[vertexIndex];
											FVector3f v2 = Vertices[vertexIndex] + Normals[vertexIndex];

											Rot = UKismetMathLibrary::FindLookAtRotation(FVector(v1), FVector(v2));
											Rot = FRotator(Rot.Pitch - 90, Rot.Yaw, Rot.Roll);
										}

										FTransform transform = FTransform(Rot, PlanetSpaceFoliageLocation - PlanetSpaceLocation, FVector(1, 1, 1));
										Rot = FRotator(0, ScaleStream.FRandRange(0, 360), 0);
										Rot = UKismetMathLibrary::TransformRotation(transform, Rot);

										transform = FTransform(Rot, PlanetSpaceFoliageLocation - PlanetSpaceLocation, FVector(
											ScaleStream.FRandRange(Foliage.MinScale, Foliage.MaxScale),
											ScaleStream.FRandRange(Foliage.MinScale, Foliage.MaxScale),
											ScaleStream.FRandRange(Foliage.MinScale, Foliage.MaxScale))
										);

										RuntimeData.LocalFoliageTransforms.Add(transform);
									}
								}
							}
							RuntimeData.Foliage = Foliage;
							FoliageRuntimeData.Add(RuntimeData);

						}
					}
				}
			}
		}
	}


	UploadChunk();

}



void UChunkObject::UploadChunk()
{

	//convert vertices to Positions
	TConstVoxelArrayView<FVector3f> Positions(Vertices.GetData(), Vertices.Num());

	TConstVoxelArrayView<FVector2f> TextureCoordinatesV(UVs.GetData(), UVs.Num());

	TVoxelArray<TConstVoxelArrayView<FVector2f>> TextureCoordinatesVV;
	TextureCoordinatesVV.Add(TextureCoordinatesV);
	
	TArray<int32> IntTriangles = TArray<int32>((*Triangles));

	TConstVoxelArrayView<int32> Indices(IntTriangles.GetData(), IntTriangles.Num());

	FPlanetNaniteBuilder NaniteBuilder;
	NaniteBuilder.PositionPrecision = -(FMath::Log2(ChunkSize / ChunkQuality / 32) - 1);
	NaniteBuilder.Mesh.Indices = Indices;
	NaniteBuilder.Mesh.Positions = Positions;
	NaniteBuilder.bCompressVertices = true;
	NaniteBuilder.Mesh.Normals = Octahedrons;
	NaniteBuilder.Mesh.Colors = VertexColors;
	NaniteBuilder.Mesh.TextureCoordinates = TextureCoordinatesVV;


	Chaos::FTriangleMeshImplicitObjectPtr ChaosMeshData;
	if (bCollisions)
	{
		TConstVoxelArrayView<uint16> FaceMaterials;
		ChaosMeshData = Chaos::FTriangleMeshImplicitObjectPtr (FVoxelChaosTriangleMeshCooker::Create(TArray<int32>(*Triangles), Positions, FaceMaterials));
	}
	
	if (bAbortAsync == true)
	{
		GenerationComplete();
		return;
	}

	// Generate the render data
	TUniquePtr<FStaticMeshRenderData> RenderData = NaniteBuilder.CreateRenderData(bAbortAsync, bRayTracing, bNaniteLandscape);
	
	if (bAbortAsync == true)
	{
		GenerationComplete();
		return;
	}
	
	// Use TWeakObjectPtr to safely handle potential GC during async execution
	TWeakObjectPtr<UChunkObject> WeakThis(this);
	bool bLocalRayTracing = bRayTracing;
	bool bLocalCollisions = bCollisions;
	bool bLocalNaniteLandscape = bNaniteLandscape;
	
	AsyncTask(ENamedThreads::GameThread, [WeakThis, ChaosMeshData, bLocalRayTracing, bLocalCollisions, bLocalNaniteLandscape, RenderData = MoveTemp(RenderData)]() mutable
	{
		UChunkObject* StrongThis = WeakThis.Get();
		if (!StrongThis)
		{
			// Object was garbage collected, clean up render data
			RenderData.Reset();
			return;
		}
		
		if (StrongThis->bAbortAsync == true)
		{
			StrongThis->GenerationComplete();
			return;
		}

		StrongThis->ChunkStaticMesh = NewObject<UStaticMesh>(StrongThis, NAME_None, RF_Transient);
		StrongThis->ChunkStaticMesh->bDoFastBuild = true;
		StrongThis->ChunkStaticMesh->bGenerateMeshDistanceField = false;
		StrongThis->ChunkStaticMesh->SetStaticMaterials({ FStaticMaterial() });
		StrongThis->ChunkStaticMesh->bSupportRayTracing = bLocalRayTracing;


		if (bLocalCollisions)
		{
			StrongThis->ChunkStaticMesh->CreateBodySetup();

			UBodySetup* BodySetup = StrongThis->ChunkStaticMesh->GetBodySetup();
			BodySetup->bGenerateMirroredCollision = false;
			BodySetup->bDoubleSidedGeometry = false;
			BodySetup->bSupportUVsAndFaceRemap = false;
			BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

			BodySetup->TriMeshGeometries = { ChaosMeshData };

			BodySetup->bHasCookedCollisionData = true;
			BodySetup->bCreatedPhysicsMeshes = true;
		}

		StrongThis->ChunkStaticMesh->SetRenderData(MoveTemp(RenderData));

		#if WITH_EDITOR
		StrongThis->ChunkStaticMesh->NaniteSettings.bEnabled = bLocalNaniteLandscape;
		#endif

		StrongThis->ChunkStaticMesh->CalculateExtendedBounds();
		StrongThis->ChunkStaticMesh->InitResources();


		StrongThis->GenerationComplete();

	});
	

}

void UChunkObject::AssignComponents()
{
	// Early validation: Check if ChunkStaticMesh is ready before doing any work
	// This can happen if InitResources() hasn't completed yet (it's async)
	if (!ChunkStaticMesh || !ChunkStaticMesh->GetRenderData() || !ChunkStaticMesh->GetRenderData()->IsInitialized())
	{
		// Render data not ready yet, remain in PENDING_ASSIGN to retry next frame
		return;
	}
	
	if ((*ChunkSMCPool).IsEmpty() == false)
	{
		//get avaliable ChunkSMC
		ChunkSMC = (*ChunkSMCPool).Pop();
		ChunkSMC->SetRelativeLocation(ChunkOriginLocation);
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
		ChunkSMC->SetRelativeLocation(ChunkOriginLocation);
		ChunkSMC->GetComponentVelocity() = FVector::ZeroVector;
		ChunkSMC->ResetSceneVelocity();
	}
	
	//--------------------------------------------------------------------------
	// Setup Dynamic Material Instance
	//--------------------------------------------------------------------------
	TObjectPtr<UMaterialInstanceDynamic> MaterialInst = ChunkSMC->CreateDynamicMaterialInstance(0, PlanetData->PlanetMaterial);
	
	// Global material parameters
	MaterialInst->SetTextureParameterValue("BiomeMap", BiomeMap);
	MaterialInst->SetTextureParameterValue("BiomeData", PlanetData->GPUBiomeData);
	MaterialInst->SetScalarParameterValue("recursionLevel", PlanetData->MaxRecursionLevel - RecursionLevel);
	MaterialInst->SetScalarParameterValue("PlanetRadius", PlanetData->PlanetRadius);
	MaterialInst->SetScalarParameterValue("NoiseHeight", PlanetData->NoiseHeight);
	MaterialInst->SetScalarParameterValue("ChunkSize", ChunkSize);
	MaterialInst->SetVectorParameterValue("ComponentLocation", ChunkOriginLocation);
	
	//--------------------------------------------------------------------------
	// Setup Material Layer Parameters
	// MaterialLayersNum = total layers in stack (Background + N overlay layers)
	// Blend parameters: indices 0 to (MaterialLayersNum-2), for layers 1+
	// Layer parameters: indices 0 to (MaterialLayersNum-1), for all layers
	//--------------------------------------------------------------------------
	for (int32 LayerIdx = 0; LayerIdx < MaterialLayersNum; LayerIdx++)
	{
		// Blend parameters (only for overlay layers, not background)
		if (LayerIdx > 0)
		{
			const int32 BlendIdx = LayerIdx - 1;
			
			FMaterialParameterInfo LayerIndexInfo;
			LayerIndexInfo.Name = "LayerIndex";
			LayerIndexInfo.Association = EMaterialParameterAssociation::BlendParameter;
			LayerIndexInfo.Index = BlendIdx;
			MaterialInst->SetScalarParameterValueByInfo(LayerIndexInfo, float(LayerIdx) + 0.01f);

			FMaterialParameterInfo BiomeCountInfo;
			BiomeCountInfo.Name = "BiomeCount";
			BiomeCountInfo.Association = EMaterialParameterAssociation::BlendParameter;
			BiomeCountInfo.Index = BlendIdx;
			MaterialInst->SetScalarParameterValueByInfo(BiomeCountInfo, float(MaterialLayersNum) + 0.01f);

			FMaterialParameterInfo BlendMapInfo;
			BlendMapInfo.Name = "BiomeMap";
			BlendMapInfo.Association = EMaterialParameterAssociation::BlendParameter;
			BlendMapInfo.Index = BlendIdx;
			MaterialInst->SetTextureParameterValueByInfo(BlendMapInfo, BiomeMap);
		}

		// Per-layer parameters (all layers including background)
		FMaterialParameterInfo RadiusInfo;
		RadiusInfo.Name = "PlanetRadius";
		RadiusInfo.Association = EMaterialParameterAssociation::LayerParameter;
		RadiusInfo.Index = LayerIdx;
		MaterialInst->SetScalarParameterValueByInfo(RadiusInfo, PlanetData->PlanetRadius);

		FMaterialParameterInfo HeightInfo;
		HeightInfo.Name = "NoiseHeight";
		HeightInfo.Association = EMaterialParameterAssociation::LayerParameter;
		HeightInfo.Index = LayerIdx;
		MaterialInst->SetScalarParameterValueByInfo(HeightInfo, PlanetData->NoiseHeight);

		FMaterialParameterInfo SizeInfo;
		SizeInfo.Name = "ChunkSize";
		SizeInfo.Association = EMaterialParameterAssociation::LayerParameter;
		SizeInfo.Index = LayerIdx;
		MaterialInst->SetScalarParameterValueByInfo(SizeInfo, ChunkSize);

		FMaterialParameterInfo LocationInfo;
		LocationInfo.Name = "ComponentLocation";
		LocationInfo.Association = EMaterialParameterAssociation::LayerParameter;
		LocationInfo.Index = LayerIdx;
		MaterialInst->SetVectorParameterValueByInfo(LocationInfo, ChunkOriginLocation);

		FMaterialParameterInfo RecursionInfo;
		RecursionInfo.Name = "recursionLevel";
		RecursionInfo.Association = EMaterialParameterAssociation::LayerParameter;
		RecursionInfo.Index = LayerIdx;
		MaterialInst->SetScalarParameterValueByInfo(RecursionInfo, PlanetData->MaxRecursionLevel - RecursionLevel);
	}
	MaterialInst = nullptr;
	
	//--------------------------------------------------------------------------
	// Upload Foliage Data
	//--------------------------------------------------------------------------
	if (bGenerateFoliage)
	{
		UploadFoliageData();
		for (FFoliageRuntimeData& Data : FoliageRuntimeData)
		{
			Data.LocalFoliageTransforms.Empty();
			Data.LocalFoliageTransforms.Shrink();
		}
	}
	
	if (ChunkMinHeight < 0 && PlanetData->bGenerateWater)
	{
		AddWaterChunk();
	}
	
	ChunkSMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChunkSMC->SetCollisionResponseToChannels(CollisionSetup);
	ChunkSMC->SetStaticMesh(ChunkStaticMesh);
	ChunkSMC->RegisterComponent();
	ChunkStatus = EChunkStatus::READY;
}



void UChunkObject::AddWaterChunk()
{
	FVector FaceDefaultForward = FVector(0, 0, 1);

	FVector InputDir = FVector(PlanetSpaceRotation);
	InputDir = InputDir.GetSafeNormal();

	// Compute the quaternion that rotates from the face's default forward to the input direction.
	FQuat FaceRotation = FQuat::FindBetweenNormals(FaceDefaultForward, InputDir);
	FTransform WaterTransform = FTransform(FaceRotation, ChunkOriginLocation, FVector(ChunkSize / 100000, ChunkSize / 100000, ChunkSize / 100000));

	FVector ChunkPositionOffset = PlanetSpaceLocation - ChunkOriginLocation;

	if (WaterSMCPool->IsEmpty() == false)
	{
		WaterChunk = WaterSMCPool->Pop();
		WaterChunk->SetVisibility(true);
	}
	else
	{
		// Spawn static mesh component for water chunk
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


	if (RecursionLevel != PlanetData->MaxRecursionLevel)
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
	
	TObjectPtr<UMaterialInstanceDynamic> WaterMaterialInst;
	if (RecursionLevel >= PlanetData->RecursionLevelForMaterialChange || PlanetData->FarWaterMaterial == nullptr)
	{
		WaterMaterialInst = WaterChunk->CreateDynamicMaterialInstance(0, PlanetData->WaterMaterial);
	}
	else
	{
		WaterMaterialInst = WaterChunk->CreateDynamicMaterialInstance(0, PlanetData->FarWaterMaterial);
	}

	WaterMaterialInst->SetTextureParameterValue("HeightMap", BiomeMap);
	WaterMaterialInst->SetScalarParameterValue("PlanetRadius", PlanetData->PlanetRadius);
	WaterMaterialInst->SetVectorParameterValue("PositionOffset", UKismetMathLibrary::InverseTransformLocation(WaterTransform, ChunkPositionOffset));
	WaterMaterialInst = nullptr;
}

void UChunkObject::FreeComponents()
{
	//Add components to pools (DISABLED TO PREVENT MOTION BLUR ISSUES)
	if (ChunkSMC != nullptr && ChunkSMC->IsValidLowLevel())
	{
		ChunkSMC->DestroyComponent();
	}
	ChunkSMC = nullptr;

	// Clear static mesh
	if (ChunkStaticMesh != nullptr && ChunkStaticMesh->IsValidLowLevel() && !ChunkStaticMesh->HasAnyFlags(RF_BeginDestroyed))
	{
		FStaticMeshRenderData* LocalRenderData = ChunkStaticMesh->GetRenderData();
		UStaticMesh* MeshToDestroy = ChunkStaticMesh;
		
		ChunkStaticMesh->bSupportRayTracing = false;
#if WITH_EDITOR
		ChunkStaticMesh->NaniteSettings.bEnabled = false;
#endif
		
		ChunkStaticMesh->ReleaseResources();

		// Prevent GC regarding this mesh until we are done with it on the render thread
		MeshToDestroy->AddToRoot();
		
		ENQUEUE_RENDER_COMMAND(ReleaseNaniteResources)(
		[LocalRenderData, MeshToDestroy](FRHICommandListImmediate& RHICmdList)
		{
			if (LocalRenderData)
			{
				LocalRenderData->ReleaseResources();
				if (LocalRenderData->NaniteResourcesPtr.IsValid())
				{
					LocalRenderData->NaniteResourcesPtr->ReleaseResources();
					LocalRenderData->NaniteResourcesPtr.Reset();
				}
			}

			// Defer destruction to game thread AFTER render resources are released
			AsyncTask(ENamedThreads::GameThread, [MeshToDestroy]()
			{
				if (MeshToDestroy && MeshToDestroy->IsValidLowLevel())
				{
					MeshToDestroy->RemoveFromRoot();
					MeshToDestroy->ConditionalBeginDestroy();
				}
			});
		});

		ChunkStaticMesh = nullptr;
	}

	// Clean up BiomeMap render target
	if (BiomeMap != nullptr && BiomeMap->IsValidLowLevel())
	{
		BiomeMap->ConditionalBeginDestroy();
	}
	BiomeMap = nullptr;


	if (WaterChunk != nullptr && WaterChunk->IsValidLowLevel())
	{
		WaterChunk->SetMaterial(0, nullptr);
		WaterChunk->SetVisibility(false);
		if (WaterSMCPool != nullptr)
		{
			(*WaterSMCPool).Add(WaterChunk);
		}
	}
	WaterChunk = nullptr;

	if (FoliageISMCPool != nullptr)
	{
		for (FFoliageRuntimeData& Data : FoliageRuntimeData)
		{
			if (Data.Ismc != nullptr && Data.Ismc->IsValidLowLevel())
			{
				Data.Ismc->DestroyComponent();
			}
			Data.Ismc = nullptr;
		}
	}
}


void UChunkObject::BeginSelfDestruct()
{
	ChunkStatus = EChunkStatus::REMOVING;
	bAbortAsync = true;
}

void UChunkObject::SelfDestruct()
{
	FreeComponents();
	bAbortAsync = true;
	
	WaterChunk = nullptr;

	// Empty and shrink arrays to release memory
	Vertices.Empty();
	Vertices.Shrink();
	Slopes.Empty();
	Slopes.Shrink();
	VertexHeight.Empty();
	VertexHeight.Shrink();
	FoliageBiomeIndices.Empty();
	FoliageBiomeIndices.Shrink();
	Normals.Empty();
	Normals.Shrink();
	Octahedrons.Empty();
	Octahedrons.Shrink();
	UVs.Empty();
	UVs.Shrink();
	VertexColors.Empty();
	VertexColors.Shrink();
	ForestStrength.Empty();
	ForestStrength.Shrink();
	Biomes.Empty();
	Biomes.Shrink();
	FoliageRuntimeData.Empty();
	FoliageRuntimeData.Shrink();
	
	// Clear our reference to the readback buffers
	GPUReadback.OutputBuffer.Reset();
	GPUReadback.OutputVCBuffer.Reset();

	ConditionalBeginDestroy();
}

// Math Helpers

uint32_t UChunkObject::fmix32(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6bU;
	h ^= h >> 13;
	h *= 0xc2b2ae35U;
	h ^= h >> 16;
	return h;
}

uint32_t UChunkObject::FloatAsUint(float f)
{
	uint32_t u;
	std::memcpy(&u, &f, sizeof(u));
	return u;
}

float UChunkObject::QuantizeFloat(float f, float Range)
{
	if (Range <= 0.0f)
	{
		return f;
	}
	return FMath::RoundToFloat(f / Range) * Range;
}

double UChunkObject::QuantizeDouble(double d, double Range)
{
	if (Range <= 0.0)
	{
		return d;
	}
	return FMath::RoundToDouble(d / Range) * Range;
}

float UChunkObject::Hash3DToUnit(const FVector& P, uint32_t Salt)
{
	const uint32_t ux = FloatAsUint(P.X);
	const uint32_t uy = FloatAsUint(P.Y);
	const uint32_t uz = FloatAsUint(P.Z);

	uint32_t h = (ux * 0x9e3779b1u) ^ (uy * 0x85ebca6bu) ^ (uz * 0xc2b2ae35u) ^ Salt;
	h = fmix32(h);

	return static_cast<float>(h) / 4294967295.0f;
}

FVector2D UChunkObject::HashFVectorToVector2D(const FVector& Input, float RangeMin, float RangeMax, float RangeTolerance)
{
	FVector QuantizedInput = Input;

	if (RangeTolerance > 0.0f)
	{
		QuantizedInput.X = QuantizeFloat(Input.X, RangeTolerance);
		QuantizedInput.Y = QuantizeFloat(Input.Y, RangeTolerance);
		QuantizedInput.Z = QuantizeFloat(Input.Z, RangeTolerance);
	}

	const float hx = Hash3DToUnit(QuantizedInput, 0x243F6A88u);
	const float hy = Hash3DToUnit(QuantizedInput, 0x85A308D3u);

	const float x = FMath::Lerp(RangeMin, RangeMax, hx);
	const float y = FMath::Lerp(RangeMin, RangeMax, hy);

	return FVector2D(x, y);
}








