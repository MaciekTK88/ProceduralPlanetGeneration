// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Maciej Tkaczewski
//
// Planet Compute Shader Implementation
// Material-based compute shader that evaluates terrain generation per-vertex.

#include "ComputeShader/Public/PlanetComputeShader/PlanetComputeShader.h"

#include "GlobalShader.h"
#include "MaterialShader.h"
#include "MeshDrawShaderBindings.h"
#include "MeshPassProcessor.inl"
#include "MeshPassUtils.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHIGPUReadback.h"
#include "StaticMeshResources.h"

//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------
DECLARE_STATS_GROUP(TEXT("PlanetComputeShader"), STATGROUP_PlanetComputeShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("PlanetComputeShader Execute"), STAT_PlanetComputeShader_Execute, STATGROUP_PlanetComputeShader);

//------------------------------------------------------------------------------
// Shader Declaration
// Material-based compute shader for procedural terrain generation.
// Uses FMeshMaterialShader to access material graph evaluation.
//------------------------------------------------------------------------------
class COMPUTESHADER_API FPlanetComputeShader : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPlanetComputeShader, MeshMaterial);
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FPlanetComputeShader, FMeshMaterialShader)

	// Shader parameters bound from CPU
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Input/Output buffers
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float3>, Input)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, BiomeMap)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Output)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutputVC)
		
		// Input textures
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, CurveAtlas)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint>, BiomeDataTexture)
		
		// Chunk parameters
		SHADER_PARAMETER(FVector3f, chunkLocation)
		SHADER_PARAMETER(FIntVector, chunkRotation)
		SHADER_PARAMETER(FVector3f, chunkOriginLocation)
		SHADER_PARAMETER(float, chunkSize)
		SHADER_PARAMETER(float, planetRadius)
		SHADER_PARAMETER(float, noiseHeight)
		SHADER_PARAMETER(uint32, biomeCount)
		SHADER_PARAMETER(int32, chunkQuality)
		
		// View uniforms (required for material evaluation)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		// Compile only for materials marked "Used with Lidar Point Cloud"
		// This significantly reduces shader permutations while still providing material evaluation
		return Parameters.MaterialParameters.bIsUsedWithLidarPointCloud;
	}

	static void ModifyCompilationEnvironment(
		const FMaterialShaderPermutationParameters& Parameters, 
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Thread group size (16x16 = 256 threads)
		OutEnvironment.SetDefine(TEXT("THREADS_X"), 16);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), 16);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), 1);
		
		// Shader configuration
		OutEnvironment.SetDefine(TEXT("PLANET_COMPUTE_SHADER_COMPILE"), 1);
		OutEnvironment.SetDefine(TEXT("NUM_MATERIAL_TEXCOORDS"), 4);
		OutEnvironment.SetDefine(TEXT("NUM_TEX_COORD_INTERPOLATORS"), 4);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FPlanetComputeShader, TEXT("/ComputeShaderShaders/Planet.usf"), TEXT("PlanetComputeShader"), SF_Compute);

//------------------------------------------------------------------------------
// Dispatch Implementation
//------------------------------------------------------------------------------
FPlanetComputeShaderReadback FPlanetComputeShaderInterface::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList, 
	FPlanetComputeShaderDispatchParams Params)
{
	FPlanetComputeShaderReadback Readback;

	FRDGBuilder GraphBuilder(RHICmdList);
	{
		SCOPE_CYCLE_COUNTER(STAT_PlanetComputeShader_Execute);
		DECLARE_GPU_STAT(PlanetComputeShader);
		RDG_EVENT_SCOPE(GraphBuilder, "PlanetComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, PlanetComputeShader);

		//----------------------------------------------------------------------
		// Validate Material
		//----------------------------------------------------------------------
		const FMaterialRenderProxy* MaterialProxy = Params.MaterialRenderProxy;
		if (!MaterialProxy)
		{
			UE_LOG(LogTemp, Error, TEXT("PlanetComputeShader: No material proxy provided"));
			return Readback;
		}
		
		const FMaterial* Material = MaterialProxy->GetMaterialNoFallback(GMaxRHIFeatureLevel);
		if (!Material || !Material->GetRenderingThreadShaderMap())
		{
			UE_LOG(LogTemp, Error, TEXT("PlanetComputeShader: Material not ready or shader not compiled. Ensure 'Used with Lidar Point Cloud' is enabled on the Generation Material."));
			return Readback;
		}

		//----------------------------------------------------------------------
		// Get Shader
		//----------------------------------------------------------------------
		// FLocalVertexFactory used as dummy since we don't have a real mesh
		TShaderRef<FPlanetComputeShader> ComputeShader = Material->GetShader<FPlanetComputeShader>(
			&FLocalVertexFactory::StaticType, 
			false
		);

		if (!ComputeShader.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("PlanetComputeShader: Shader not found. Ensure 'Used with Lidar Point Cloud' is enabled on the Generation Material and recompile shaders."));
			return Readback;
		}

		//----------------------------------------------------------------------
		// Setup Parameters
		//----------------------------------------------------------------------
		FPlanetComputeShader::FParameters* PassParams = GraphBuilder.AllocParameters<FPlanetComputeShader::FParameters>();
		
		// Register textures
		FRDGTextureRef BiomeMapRDG = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(Params.BiomeMap->GetResource()->TextureRHI, TEXT("BiomeMap")));
		PassParams->BiomeMap = GraphBuilder.CreateUAV(BiomeMapRDG);

		FRDGTextureRef CurveAtlasRDG = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(Params.CurveAtlas->GetResource()->TextureRHI, TEXT("CurveAtlas")));
		PassParams->CurveAtlas = GraphBuilder.CreateSRV(CurveAtlasRDG);

		FRDGTextureRef BiomeDataRDG = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(Params.BiomeDataTexture->GetResource()->TextureRHI, TEXT("BiomeDataTexture")));
		PassParams->BiomeDataTexture = GraphBuilder.CreateSRV(BiomeDataRDG);

		// Chunk parameters
		PassParams->chunkLocation = Params.ChunkLocation;
		PassParams->chunkRotation = Params.ChunkRotation;
		PassParams->chunkOriginLocation = Params.ChunkOriginLocation;
		PassParams->chunkSize = Params.ChunkSize;
		PassParams->planetRadius = Params.PlanetRadius;
		PassParams->noiseHeight = Params.NoiseHeight;
		PassParams->biomeCount = Params.BiomeCount;
		PassParams->chunkQuality = Params.ChunkQuality;
		
		// Create minimal view uniform buffer (required for material evaluation)
		FViewUniformShaderParameters ViewParams;
		ViewParams.GameTime = 0.0f;
		ViewParams.RealTime = 0.0f;
		PassParams->View = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(
			ViewParams, 
			UniformBuffer_SingleFrame
		);

		//----------------------------------------------------------------------
		// Create Output Buffers
		//----------------------------------------------------------------------
		const int32 NumVertices = Params.X * Params.Y;
		Readback.NumVertices = NumVertices;

		// Position buffer: 3 floats (x,y,z) per vertex
		FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(float), NumVertices * 3),
			TEXT("PlanetOutputBuffer")
		);
		PassParams->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_FLOAT));

		// Vertex color buffer: 4 bytes (RGBA) per vertex
		FRDGBufferRef OutputVCBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(uint8), NumVertices * 4),
			TEXT("PlanetOutputVCBuffer")
		);
		PassParams->OutputVC = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputVCBuffer, PF_R8_UINT));
		
		//----------------------------------------------------------------------
		// Dispatch Compute Shader
		//----------------------------------------------------------------------
		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(
			FIntPoint(Params.X, Params.Y), 
			FIntPoint(16, 16)
		);
		
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("PlanetComputeShader"),
			PassParams,
			ERDGPassFlags::AsyncCompute,
			[PassParams, ComputeShader, MaterialProxy, Material, GroupCount](FRHIComputeCommandList& RHICmdList)
			{
				MaterialProxy->UpdateUniformExpressionCacheIfNeeded(GMaxRHIFeatureLevel);

				// Setup shader bindings
				FMeshMaterialShaderElementData ShaderElementData;
				FMeshProcessorShaders PassShaders;
				PassShaders.ComputeShader = ComputeShader;

				FMeshDrawShaderBindings ShaderBindings;
				ShaderBindings.Initialize(PassShaders);

				int32 DataOffset = 0;
				FMeshDrawSingleShaderBindings SingleBindings = ShaderBindings.GetSingleShaderBindings(SF_Compute, DataOffset);
				ComputeShader->GetShaderBindings(
					nullptr,              // Scene
					GMaxRHIFeatureLevel,  // FeatureLevel
					nullptr,              // PrimitiveSceneProxy
					*MaterialProxy,       // MaterialRenderProxy
					*Material,            // Material
					ShaderElementData,    // ElementData
					SingleBindings        // ShaderBindings
				);

				ShaderBindings.Finalize(&PassShaders);
				UE::MeshPassUtils::Dispatch(RHICmdList, ComputeShader, ShaderBindings, *PassParams, GroupCount);
			}
		);

		//----------------------------------------------------------------------
		// Setup Readback
		//----------------------------------------------------------------------
		Readback.OutputBuffer = MakeShared<FRHIGPUBufferReadback>(TEXT("PlanetOutputReadback"));
		AddEnqueueCopyPass(GraphBuilder, Readback.OutputBuffer.Get(), OutputBuffer, 0u);
		
		Readback.OutputVCBuffer = MakeShared<FRHIGPUBufferReadback>(TEXT("PlanetOutputVCReadback"));
		AddEnqueueCopyPass(GraphBuilder, Readback.OutputVCBuffer.Get(), OutputVCBuffer, 0u);
	}
	
	GraphBuilder.Execute();
	return Readback;
}