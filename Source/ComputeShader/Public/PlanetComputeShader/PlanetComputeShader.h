#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"

//#include "ExampleComputeShader.generated.h"

struct COMPUTESHADER_API FPlanetComputeShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	
	FVector3f Input[4];
	UTextureRenderTarget2D* InputRenderTarget;
	UTexture2D* CurveAtlas;
	UTexture2D* BiomeDataTexture;
	float Output[110592];
	float OutputVC[110592];
	int PlanetType = 0;
	uint32 BiomeCount;
	int ChunkQuality;
};

// This is a public interface that we define so outside code can invoke our compute shader.
class COMPUTESHADER_API FPlanetComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FPlanetComputeShaderDispatchParams Params,
		TFunction<void(TArray<float> OutputVal, TArray<uint8> OutputVCVal)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FPlanetComputeShaderDispatchParams Params,
		TFunction<void(TArray<float> OutputVal, TArray<uint8> OutputVCVal)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FPlanetComputeShaderDispatchParams Params,
		TFunction<void(TArray<float> OutputVal, TArray<uint8> OutputVCVal)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}else{
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};
