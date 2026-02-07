// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski
//
// Planet Compute Shader Interface
// Dispatches material-based compute shaders for procedural terrain generation.
// Evaluates the material graph per-vertex and outputs elevation, biome data, and vertex colors.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"
#include "RHIGPUReadback.h"

//------------------------------------------------------------------------------
// Dispatch Parameters
// Contains all data needed to execute the planet compute shader.
//------------------------------------------------------------------------------
struct COMPUTESHADER_API FPlanetComputeShaderDispatchParams
{
	// Thread group dimensions (X*Y = total vertices)
	int32 X = 1;
	int32 Y = 1;
	int32 Z = 1;

	// Chunk spatial data
	FVector3f ChunkLocation;        // Chunk center on unit sphere
	FIntVector ChunkRotation;       // Chunk face rotation
	FVector3f ChunkOriginLocation;    // World-space chunk origin
	float ChunkSize = 0.0f;         // Chunk edge length
	float PlanetRadius = 0.0f;     // Base planet radius
	float NoiseHeight = 0.0f;       // Maximum terrain displacement

	// GPU textures
	TObjectPtr<UTextureRenderTarget2D> BiomeMap;      // Output: indices + strengths
	TObjectPtr<UTexture2D> CurveAtlas;                // Input: terrain curves
	TObjectPtr<UTexture2D> BiomeDataTexture;          // Input: biome configuration
	
	// Material for evaluation
	TObjectPtr<UMaterialInterface> GenerationMaterial;
	const FMaterialRenderProxy* MaterialRenderProxy = nullptr;

	// Shader configuration
	uint32 BiomeCount = 0;          // Number of biomes
	int32 ChunkQuality = 0;         // Vertices per edge - 1
};

//------------------------------------------------------------------------------
// Readback Data
// Contains GPU buffer readbacks for vertex data retrieval.
//------------------------------------------------------------------------------
struct COMPUTESHADER_API FPlanetComputeShaderReadback
{
	TSharedPtr<FRHIGPUBufferReadback> OutputBuffer;    // Vertex positions (NumVertices * 3 floats)
	TSharedPtr<FRHIGPUBufferReadback> OutputVCBuffer;  // Vertex colors (NumVertices * 4 bytes RGBA)
	int32 NumVertices = 0;                             // Total vertex count
};

//------------------------------------------------------------------------------
// Compute Shader Interface
// Public API for dispatching planet terrain generation compute shaders.
//------------------------------------------------------------------------------
class COMPUTESHADER_API FPlanetComputeShaderInterface
{
public:
	/**
	 * Executes the compute shader on the render thread.
	 * @param RHICmdList - The RHI command list to execute on
	 * @param Params - Dispatch parameters
	 * @return Readback buffers for async data retrieval
	 */
	static FPlanetComputeShaderReadback DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FPlanetComputeShaderDispatchParams Params
	);

	/**
	 * Dispatches the compute shader from any thread.
	 * If called from render thread, executes immediately.
	 * Otherwise, enqueues a render command and calls back on game thread.
	 * @param Params - Dispatch parameters
	 * @param OnDispatchComplete - Callback with readback data
	 */
	static void Dispatch(
		FPlanetComputeShaderDispatchParams Params,
		TFunction<void(FPlanetComputeShaderReadback Readback)> OnDispatchComplete
	)
	{
		if (IsInRenderingThread())
		{
			FPlanetComputeShaderReadback Readback = DispatchRenderThread(
				GetImmediateCommandList_ForRenderCommand(), 
				Params
			);
			OnDispatchComplete(Readback);
		}
		else
		{
			ENQUEUE_RENDER_COMMAND(PlanetComputeShaderDispatch)(
				[Params, OnDispatchComplete](FRHICommandListImmediate& RHICmdList)
				{
					FPlanetComputeShaderReadback Readback = DispatchRenderThread(RHICmdList, Params);
					AsyncTask(ENamedThreads::GameThread, [Readback, OnDispatchComplete]()
					{
						OnDispatchComplete(Readback);
					});
				}
			);
		}
	}
};
