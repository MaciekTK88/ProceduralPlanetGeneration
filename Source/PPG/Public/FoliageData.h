// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FoliageData.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FFoliageListS
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMesh* FoliageMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float FoliageDensity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float FoliageDistance;

	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	//float FoliageStartDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int maxScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int minScale;

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
	bool FarFoliageWPO = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MinHeight = 5980000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MaxHeight = 6050000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool isSlopeFoliage = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool AbsoluteHeight = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMesh* LowPolyMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float LowPolyActivation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool FarFoliageDensityBoost = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool ScalableDensity = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool Collision = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CollisionEnableDistance = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	uint8 ShadowDisableDistance = 0;

};


UCLASS()
class PPG_API UFoliageData : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FFoliageListS> FoliageList;
	
};
