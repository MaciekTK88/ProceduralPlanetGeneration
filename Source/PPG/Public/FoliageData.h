// Copyright (c) 2025 Maciej Tkaczewski. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FoliageData.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FFoliageLOD
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMesh* Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int ActivationStep = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool EnableWPO = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DensityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FFoliageListS
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMesh* FoliageMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float FoliageDensity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int FoliageDistance;

	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	//float FoliageStartDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float maxScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float minScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CullingDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool AlignToTarrain = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DepthOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool WPO = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float WPODisableDistance = 10000.0f;



	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MinHeight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MaxHeight = 600000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int MinSlope = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int MaxSlope = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool AbsoluteHeight = false;





	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool ScalableDensity = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool Collision = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CollisionEnableDistance = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	uint8 ShadowDisableDistance = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FFoliageLOD> LODs;

};


UCLASS()
class PPG_API UFoliageData : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FFoliageListS> FoliageList;
	
};
