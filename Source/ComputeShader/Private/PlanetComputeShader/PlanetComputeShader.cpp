// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#include "ComputeShader/Public/PlanetComputeShader/PlanetComputeShader.h"
#include "PixelShaderUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MeshDrawShaderBindings.h"
#include "RHIGPUReadback.h"
#include "MeshPassUtils.h"
#include "RenderGraphUtils.h"      // For FComputeShaderUtils::Dispatch
#include "RHIStaticStates.h"
#include "RHICommandList.h"
#include "DynamicRHI.h"
#include "RenderTargetPool.h"
#include "MaterialShader.h"

DECLARE_STATS_GROUP(TEXT("PlanetComputeShader"), STATGROUP_PlanetComputeShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("PlanetComputeShader Execute"), STAT_PlanetComputeShader_Execute, STATGROUP_PlanetComputeShader);

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class COMPUTESHADER_API FPlanetComputeShader: public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FPlanetComputeShader);
	SHADER_USE_PARAMETER_STRUCT(FPlanetComputeShader, FGlobalShader);
	
	
	class FPlanetComputeShader_Perm_TEST : SHADER_PERMUTATION_INT("PlanetType", 4);
	using FPermutationDomain = TShaderPermutationDomain<
		FPlanetComputeShader_Perm_TEST
	>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		/*
		* Here's where you define one or more of the input parameters for your shader.
		* Some examples:
		*/
		// SHADER_PARAMETER(uint32, MyUint32) // On the shader side: uint32 MyUint32;
		// SHADER_PARAMETER(FVector3f, MyVector) // On the shader side: float3 MyVector;

		// SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture) // On the shader side: Texture2D<float4> MyTexture; (float4 should be whatever you expect each pixel in the texture to be, in this case float4(R,G,B,A) for 4 channels)
		// SHADER_PARAMETER_SAMPLER(SamplerState, MyTextureSampler) // On the shader side: SamplerState MySampler; // CPP side: TStaticSamplerState<ESamplerFilter::SF_Bilinear>::GetRHI();

		//SHADER_PARAMETER_ARRAY(float, MyFloatArray, [3]) // On the shader side: float MyFloatArray[3];

		// SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, MyTextureUAV) // On the shader side: RWTexture2D<float4> MyTextureUAV;
		// SHADER_PARAMETER_UAV(RWStructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWStructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_UAV(RWBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWBuffer<FMyCustomStruct> MyCustomStructs;

		// SHADER_PARAMETER_SRV(StructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: StructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Buffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: Buffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Texture2D<FVector4f>, MyReadOnlyTexture) // On the shader side: Texture2D<float4> MyReadOnlyTexture;

		// SHADER_PARAMETER_STRUCT_REF(FMyCustomStruct, MyCustomStruct)

		//SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, Agent)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float3>, Input)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, BiomeMap)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, CurveAtlas)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, BiomeDataTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Output)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutputVC)
		SHADER_PARAMETER(uint32, biomeCount)
		SHADER_PARAMETER(int32, chunkQuality)
		SHADER_PARAMETER(FVector3f, chunkLocation)
		SHADER_PARAMETER(FIntVector, chunkRotation)
		SHADER_PARAMETER(FVector3f, chunkOriginLocation)
		SHADER_PARAMETER(float, chunkSize)
		SHADER_PARAMETER(float, planetRadius)
		SHADER_PARAMETER(float, noiseHeight)
		

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		OutEnvironment.SetDefine(TEXT("THREADS_X"), 16);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), 16);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), 1);
		OutEnvironment.SetDefine(TEXT("PlanetType"), PermutationVector.Get<FPlanetComputeShader::FPlanetComputeShader_Perm_TEST>());
		
	}
private:
};

// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FPlanetComputeShader, "/ComputeShaderShaders/Planet.usf", "PlanetComputeShader", SF_Compute);

FPlanetComputeShaderReadback FPlanetComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FPlanetComputeShaderDispatchParams Params) {
	typename FPlanetComputeShader::FPermutationDomain PermutationVector;
	PermutationVector.Set<FPlanetComputeShader::FPlanetComputeShader_Perm_TEST>(Params.PlanetType);

	TShaderMapRef<FPlanetComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

	FPlanetComputeShaderReadback Readback;

	if (ComputeShader.IsValid())
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			SCOPE_CYCLE_COUNTER(STAT_PlanetComputeShader_Execute);
			DECLARE_GPU_STAT(PlanetComputeShader);
			RDG_EVENT_SCOPE(GraphBuilder, "PlanetComputeShader");
			RDG_GPU_STAT_SCOPE(GraphBuilder, PlanetComputeShader);

			FPlanetComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FPlanetComputeShader::FParameters>();
			
			FRDGTextureRef RenderTargetRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Params.BiomeMap->GetResource()->TextureRHI, TEXT("BiomeMap")));
			PassParameters->BiomeMap = GraphBuilder.CreateUAV(RenderTargetRDG);

			FRDGTextureRef CurveAtlas = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Params.CurveAtlas->GetResource()->TextureRHI, TEXT("CurveAtlas")));
			PassParameters->CurveAtlas = GraphBuilder.CreateSRV(CurveAtlas);
			//PassParameters->LQRenderTarget = GraphBuilder.CreateUAV(LQRenderTargetRDG);

			FRDGTextureRef BiomeDataTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Params.BiomeDataTexture->GetResource()->TextureRHI, TEXT("BiomeDataTexture")));
			PassParameters->BiomeDataTexture = GraphBuilder.CreateSRV(BiomeDataTexture);

			PassParameters->biomeCount = Params.BiomeCount;
			PassParameters->chunkQuality = Params.ChunkQuality;
			PassParameters->chunkLocation = Params.ChunkLocation;
			PassParameters->chunkRotation = Params.ChunkRotation;
			PassParameters->chunkOriginLocation = Params.ChunkOriginLocation;
			PassParameters->chunkSize = Params.ChunkSize;
			PassParameters->planetRadius = Params.PlanetRadius;
			PassParameters->noiseHeight = Params.NoiseHeight;
			
			int32 NumVertices = Params.X * Params.Y;
			int32 NumDataPoints = NumVertices * 3;
			Readback.NumDataPoints = NumDataPoints;

			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(float), NumDataPoints),
				TEXT("OutputBuffer"));

			PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_FLOAT));

			FRDGBufferRef OutputBufferVC = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(uint8), NumDataPoints),
			TEXT("OutputBufferVC"));

			PassParameters->OutputVC = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBufferVC, PF_R8_UINT));
			
			// Group count optimization
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(Params.X, Params.Y), FIntPoint(16, 16));
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecutePlanetComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
			{
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
			});

			
			Readback.OutputBuffer = MakeShared<FRHIGPUBufferReadback>(TEXT("ExecutePlanetComputeShaderOutput"));
			AddEnqueueCopyPass(GraphBuilder, Readback.OutputBuffer.Get(), OutputBuffer, 0u);
			
			Readback.OutputVCBuffer = MakeShared<FRHIGPUBufferReadback>(TEXT("ExecutePlanetComputeShaderOutputVC"));
			AddEnqueueCopyPass(GraphBuilder, Readback.OutputVCBuffer.Get(), OutputBufferVC, 0u);
			
		}
		GraphBuilder.Execute();
	}
	else
	{
		#if WITH_EDITOR
			GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader has a problem.")));
		#endif
	}

	return Readback;
}