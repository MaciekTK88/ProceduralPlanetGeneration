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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage Settings", meta = (ToolTip = "Maximum scale of the foliage instance"))
	float MaxScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage Settings", meta = (ToolTip = "Minimum scale of the foliage instance"))
	float MinScale;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Culling", meta = (ToolTip = "Distance at which foliage instances are culled"))
	float CullingDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ToolTip = "If true, aligns foliage to the terrain normal"))
	bool bAlignToTerrain = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ToolTip = "Offset from the terrain surface"))
	float DepthOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Performance", meta = (ToolTip = "Enable World Position Offset"))
	bool bEnableWPO = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Performance", meta = (ToolTip = "Distance at which WPO is disabled"))
	float WPODisableDistance = 10000.0f;



	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ToolTip = "Minimum height for foliage placement"))
	float MinHeight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ToolTip = "Maximum height for foliage placement"))
	float MaxHeight = 600000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ToolTip = "Minimum slope for foliage placement"))
	int MinSlope = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ToolTip = "Maximum slope for foliage placement"))
	int MaxSlope = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ToolTip = "Use absolute height instead of relative to planet surface"))
	bool bUseAbsoluteHeight = false;





	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Density", meta = (ToolTip = "If true, density scales with global settings"))
	bool bScalableDensity = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision", meta = (ToolTip = "Enable collision for this foliage"))
	bool bEnableCollision = false;

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
