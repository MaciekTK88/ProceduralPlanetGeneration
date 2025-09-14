// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "RHICommandList.h"
#include "RenderGraphUtils.h"
#include "Engine/DataAsset.h"
#include "FoliageData.h"
#include "PlanetData.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FBiomeDataS
{
    GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	float MinTemperature = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	float MaxTemperature = 1.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	uint8 TerrainCurveIndex = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FoliageSetup")
	UFoliageData* FoliageData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	uint8 GroundTextureIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	uint8 SlopeTextureIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForestSetup")
	bool GenerateForest = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForestSetup")
	uint8 ForestTextureIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForestSetup")
	UFoliageData* ForestFoliageData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForestSetup")
	float ForestHeight = 100000;
};

/*
class FMyNoiseComputeShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMyNoiseComputeShader);
	SHADER_USE_PARAMETER_STRUCT(FMyNoiseComputeShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, noiseScale)
		SHADER_PARAMETER(uint32, PositionsCount)
		SHADER_PARAMETER(FIntPoint, TextureResolution)  // Use FIntPoint instead of FVector2D.
		SHADER_PARAMETER_SRV(StructuredBuffer<float3>, Positions)
		SHADER_PARAMETER_UAV(RWTexture2D<float>, OutTexture)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FMyNoiseComputeShader, "/Project/Private/MyNoiseComputeShader.usf", "CSMain", SF_Compute);
*/


UCLASS(BlueprintType)
class PPG_API UPlanetData : public UDataAsset
{
	GENERATED_BODY()

public:

	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	TArray<FBiomeDataS> BiomeData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	int PlanetType = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	UTexture2D* CurveAtlas;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ChunkSetup")
	bool GenerateWater = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	float noiseHeight = 400000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ChunkSetup")
	float PlanetRadius = 2500000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ChunkSetup")
	int maxRecursionLevel = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ChunkSetup")
	int minRecursionLevel = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ChunkSetup")
	UMaterialInterface* PlanetMaterial;

	

	FVector PlanetTransformLocation(const FVector& TransformPos, const FIntVector TransformRotDeg, const FVector& LocalLocation)
	{
		// Round the rotation values to the nearest integer
		FIntVector Rotation = TransformRotDeg;
		//print rotation
		//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("Rotation: %d %d %d"), Rotation.X, Rotation.Y, Rotation.Z));

		if (Rotation.X == 0 && Rotation.Y == -1 && Rotation.Z == 0)
		{
			return FVector(LocalLocation.X, 0.f, LocalLocation.Y) + TransformPos;
		}

		if (Rotation.X == 0 && Rotation.Y == 1 && Rotation.Z == 0)
		{
			return FVector(LocalLocation.X, 0.f, -LocalLocation.Y) + TransformPos;
		}

		if (Rotation.X == -1 && Rotation.Y == 0 && Rotation.Z == 0)
		{
			return FVector(0.f, LocalLocation.Y, LocalLocation.X) + TransformPos;
		}

		if (Rotation.X == 0 && Rotation.Y == 0 && Rotation.Z == 1)
		{
			return FVector(LocalLocation.X, LocalLocation.Y, 0.f) + TransformPos;
		}

		if (Rotation.X == 0 && Rotation.Y == 0 && Rotation.Z == -1)
		{
			return FVector(-LocalLocation.X, LocalLocation.Y, 0.f) + TransformPos;
		}

		if (Rotation.X == 1 && Rotation.Y == 0 && Rotation.Z == 0)
		{
			return FVector(0.f, LocalLocation.Y, -LocalLocation.X) + TransformPos;
		}

		// Default case: Return the unchanged position if no match is found.
		return LocalLocation + TransformPos;
	}

	FVector2f InversePlanetTransformLocation(const FIntVector TransformRotDeg, const FVector& WorldLocation)
	{
		//FIntVector IntWorldPos = FVoxelUtilities::RoundToInt(WorldLocation / 10000);
		
		if (TransformRotDeg.X != 0 && TransformRotDeg.Y != 0)
		{
			return FVector2f(WorldLocation.X, WorldLocation.Y);
		}
		if (TransformRotDeg.X != 0 && TransformRotDeg.Z != 0)
		{
			return FVector2f(WorldLocation.X, WorldLocation.Z);
		}
		if (TransformRotDeg.Y != 0 && TransformRotDeg.Z != 0)
		{
			return FVector2f(WorldLocation.Y, WorldLocation.Z);
		}
		return FVector2f(0, 0);
		
    }


private:
	
		
	float hash(float n)
	{
		return FMath::Frac(FMath::Sin(n) * 753.5453123);
	}

	float mix(float a, float b, float t)
	{
		return a * (1.0 - t) + b * t;
	}


	float noise_iq(FVector x) 
	{
		//FVector p = floor(x);
		FVector p = FVector(FMath::FloorToInt(x.X), FMath::FloorToInt(x.Y), FMath::FloorToInt(x.Z));
		//FVector f = fract(x);
		FVector f = FVector(x.X - p.X, x.Y - p.Y, x.Z - p.Z);
		//f = f * f * (3.0 - 2.0 * f);
		f = f * f * (FVector(3.0, 3.0, 3.0) - FVector(2.0, 2.0, 2.0) * f);


		float n = p.X + p.Y * 157.0 + 113.0 * p.Z;
		return mix(mix(mix(hash(n + 0.0), hash(n + 1.0), f.X),
			mix(hash(n + 157.0), hash(n + 158.0), f.X), f.Y),
			mix(mix(hash(n + 113.0), hash(n + 114.0), f.X),
				mix(hash(n + 270.0), hash(n + 271.0), f.X), f.Y), f.Z);
	}

	//#define DECL_FBM_FUNC(_name, _octaves, _basis) 
	// float _name(_in(vec3) pos, _in(float) lacunarity, _in(float) init_gain, _in(float) gain) { 
	// vec3 p = pos; float H = init_gain; float t = 0.; 
	// for (int i = 0; i < _octaves; i++) { t += _basis * H; p *= lacunarity; H *= gain; }
	// return t; }
	
	
};
