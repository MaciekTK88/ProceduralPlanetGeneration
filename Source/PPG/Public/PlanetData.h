// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "FoliageData.h"
#include "Curves/CurveLinearColor.h"
#include "PlanetData.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FBiomeData
{
    GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Biome|Environment")
	TArray<int32> BiomeMaskIndices;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Biome|Terrain")
	TObjectPtr<UCurveLinearColor> TerrainCurve;

	// Hidden - computed from TerrainCurve at runtime
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Biome|Terrain")
	uint8 TerrainCurveIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Biome|Terrain")
	uint8 MaterialLayerIndex = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Biome|Foliage", meta = (ToolTip = "Foliage data asset"))
	TObjectPtr<UFoliageData> FoliageData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Biome|Forest")
	bool bGenerateForest = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Biome|Forest")
	TObjectPtr<UFoliageData> ForestFoliageData;
};

/**
 * Data asset containing setup for a procedural planet.
 */
UCLASS(BlueprintType)
class PPG_API UPlanetData : public UDataAsset
{
	GENERATED_BODY()

public:

	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Biomes")
	TArray<FBiomeData> BiomeData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|General")
	int32 PlanetType = 0;



	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Dimensions")
	float NoiseHeight = 400000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Dimensions")
	float PlanetRadius = 2500000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|LOD")
	int32 MaxRecursionLevel = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|LOD")
	int32 MinRecursionLevel = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Rendering")
	TObjectPtr<UMaterialInterface> PlanetMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Water")
	bool bGenerateWater = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Water")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Water")
	TObjectPtr<UMaterialInterface> FarWaterMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Planet|Water")
	int32 RecursionLevelForMaterialChange = 3;
	
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Planet|Internal")
	TObjectPtr<UTexture2D> GPUBiomeData;

	UPROPERTY(EditAnywhere, Category = "Planet|Internal")
	int32 CurveAtlasWidth = 256;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Planet|Internal")
	TObjectPtr<UTexture2D> CurveAtlas;

	/** Transforms a local location (e.g. on a cube face) to planet space location. */
	FVector PlanetTransformLocation(const FVector& TransformPos, const FIntVector& TransformRotDeg, const FVector& LocalLocation) const;

	/** Transforms a planet space location back to a 2D local location. */
	FVector2f InversePlanetTransformLocation(const FVector& TransformPos, const FIntVector& TransformRotDeg, const FVector& PlanetSpaceLocation) const;


private:
	
		
	/*
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
	*/
};
