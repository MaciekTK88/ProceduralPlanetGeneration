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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOD")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOD")
	int ActivationDistance = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOD")
	bool bEnableWPO = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LOD")
	float DensityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FFoliageList
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Mesh")
	TObjectPtr<UStaticMesh> FoliageMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Density")
	float FoliageDensity = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "Distance at which foliage instances are spawned"))
	int SpawnDistance = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Settings", meta = (ToolTip = "Maximum scale of the foliage instance"))
	float MaxScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Settings", meta = (ToolTip = "Minimum scale of the foliage instance"))
	float MinScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Culling", meta = (ToolTip = "Distance at which foliage instances are culled"))
	float CullingDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "If true, aligns foliage to the terrain normal"))
	bool bAlignToTerrain = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "Offset from the terrain surface"))
	float DepthOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Performance", meta = (ToolTip = "Enable World Position Offset"))
	bool bEnableWPO = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Performance", meta = (ToolTip = "Distance at which WPO is disabled"))
	float WPODisableDistance = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "Minimum height for foliage placement"))
	float MinHeight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "Maximum height for foliage placement"))
	float MaxHeight = 600000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "Minimum slope for foliage placement"))
	int32 MinSlope = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "Maximum slope for foliage placement"))
	int32 MaxSlope = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Placement", meta = (ToolTip = "If true, height is allways equal zero"))
	bool bUseAbsoluteHeight = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Density", meta = (ToolTip = "If true, density scales with global settings"))
	bool bScalableDensity = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Collision", meta = (ToolTip = "Enable collision for this foliage"))
	bool bEnableCollision = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Collision")
	uint8 CollisionDisableDistance = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Rendering")
	uint8 ShadowDisableDistance = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|LOD")
	TArray<FFoliageLOD> LODs;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Clustering", meta = (ToolTip = "WIP - Enable clustering of foliage instances"))
	bool bEnableClustering = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Clustering", meta = (EditCondition = "bEnableClustering"))
	int32 ClusterSizeMin = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Clustering", meta = (EditCondition = "bEnableClustering"))
	int32 ClusterSizeMax = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage|Clustering", meta = (EditCondition = "bEnableClustering"))
	float ClusterRadius = 100.0f;
};


UCLASS(BlueprintType, meta = (DisplayName = "Foliage Data Asset"))
class PPG_API UFoliageData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Foliage")
	TArray<FFoliageList> FoliageList;
};
