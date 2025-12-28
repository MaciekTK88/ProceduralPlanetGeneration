// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"
#include "RHIGPUReadback.h"

//#include "ExampleComputeShader.generated.h"

struct COMPUTESHADER_API FPlanetComputeShaderDispatchParams
{
	int32 X = 1;
	int32 Y = 1;
	int32 Z = 1;

	FVector3f ChunkLocation;
	FIntVector ChunkRotation;
	FVector3f ChunkOriginLocation;
	float ChunkSize = 0.0f;
	float PlanetRadius = 0.0f;
	float NoiseHeight = 0.0f;

	TObjectPtr<UTextureRenderTarget2D> BiomeMap;
	TObjectPtr<UTexture2D> CurveAtlas;
	TObjectPtr<UTexture2D> BiomeDataTexture;

	int32 PlanetType = 0;
	uint32 BiomeCount = 0;
	int32 ChunkQuality = 0;
};

struct FPlanetComputeShaderReadback
{
	TSharedPtr<FRHIGPUBufferReadback> OutputBuffer;
	TSharedPtr<FRHIGPUBufferReadback> OutputVCBuffer;
	int32 NumDataPoints = 0;
};

// This is a public interface that we define so outside code can invoke our compute shader.
class COMPUTESHADER_API FPlanetComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static FPlanetComputeShaderReadback DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FPlanetComputeShaderDispatchParams Params
	);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FPlanetComputeShaderDispatchParams Params,
		TFunction<void(FPlanetComputeShaderReadback Readback)> OnDispatchComplete
	)
	{
		if (IsInRenderingThread()) {
			FPlanetComputeShaderReadback Readback = DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params);
			OnDispatchComplete(Readback);
		}else{
			ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
			[Params, OnDispatchComplete](FRHICommandListImmediate& RHICmdList)
			{
				FPlanetComputeShaderReadback Readback = DispatchRenderThread(RHICmdList, Params);
				AsyncTask(ENamedThreads::GameThread, [Readback, OnDispatchComplete]() {
					OnDispatchComplete(Readback);
				});
			});
		}
	}
};
