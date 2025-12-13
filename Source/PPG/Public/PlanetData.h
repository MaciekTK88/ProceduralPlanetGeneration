// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once

#include "CoreMinimal.h"

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	uint8 MaterialLayerIndex = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FoliageSetup")
	UFoliageData* FoliageData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForestSetup")
	bool GenerateForest = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForestSetup")
	UFoliageData* ForestFoliageData;


};




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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	float NoiseHeight = 400000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	float PlanetRadius = 2500000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	int MaxRecursionLevel = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	int MinRecursionLevel = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChunkSetup")
	UMaterialInterface* PlanetMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water")
	bool GenerateWater = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water")
	UMaterialInterface* CloseWaterMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water")
	UMaterialInterface* FarWaterMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water")
	int RecursionLevelForMaterialChange = 3;

	

	FVector PlanetTransformLocation(const FVector& TransformPos, const FIntVector TransformRotDeg, const FVector& LocalLocation)
	{
		// Round the rotation values to the nearest integer
		FIntVector Rotation = TransformRotDeg;

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

	FVector2f InversePlanetTransformLocation(
		const FVector& TransformPos,
		const FIntVector TransformRotDeg,
		const FVector& PlanetSpaceLocation)
	{
		FVector Pos = PlanetSpaceLocation - TransformPos;
		
		if (TransformRotDeg.X == 0 && TransformRotDeg.Y == -1 && TransformRotDeg.Z == 0)
		{
			return FVector2f(Pos.X, Pos.Z);
		}
		
		if (TransformRotDeg.X == 0 && TransformRotDeg.Y == 1 && TransformRotDeg.Z == 0)
		{
			return FVector2f(Pos.X, -Pos.Z);
		}
		
		if (TransformRotDeg.X == -1 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == 0)
		{
			return FVector2f(Pos.Z, Pos.Y);
		}
		
		if (TransformRotDeg.X == 0 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == 1)
		{
			return FVector2f(Pos.X, Pos.Y);
		}
		
		if (TransformRotDeg.X == 0 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == -1)
		{
			return FVector2f(-Pos.X, Pos.Y);
		}
		
		if (TransformRotDeg.X == 1 && TransformRotDeg.Y == 0 && TransformRotDeg.Z == 0)
		{
			return FVector2f(-Pos.Z, Pos.Y);
		}

		return FVector2f(0, 0);
	}



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
