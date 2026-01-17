// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "StaticMeshResources.h"
#include "Rendering/NaniteResources.h"

namespace Voxel::Nanite
{
	class FCluster;
	struct FEncodingSettings;
}

struct PPG_API FPlanetNaniteBuilder
{
public:
	// Triangle list
	struct FMesh
	{
		TConstVoxelArrayView<int32> Indices;
		TConstVoxelArrayView<FVector3f> Positions;
		TConstVoxelArrayView<FVoxelOctahedron> Normals;
		// Optional
		TConstVoxelArrayView<FColor> Colors;
		// Optional
		TVoxelArray<TConstVoxelArrayView<FVector2f>> TextureCoordinates;
	};
	FMesh Mesh;

	// Step is 2^(-PositionPrecision)
	int32 PositionPrecision = 4;
	static constexpr int32 NormalBits = 8;

	bool bCompressVertices = false;
	int32 ChunkIndex = -1;

	// OutClusteredIndices will be filled only when compressing vertices;
	// In other case, original indices array does represent clustered indices
	TUniquePtr<FStaticMeshRenderData> CreateRenderData(bool& AbortAsync, bool RayTracingProxy, bool NaniteEnabled);
	UStaticMesh* CreateStaticMesh();

public:
	static void ApplyRenderData(
		UStaticMesh& StaticMesh,
		TUniquePtr<FStaticMeshRenderData> RenderData);

	static UStaticMesh* CreateStaticMesh(TUniquePtr<FStaticMeshRenderData> RenderData);

private:
	using FCluster = Voxel::Nanite::FCluster;

	struct FBuildData
	{
		Nanite::FResources& Resources;
		const Voxel::Nanite::FEncodingSettings& EncodingSettings;
		TVoxelArray<TVoxelArray<TUniquePtr<FCluster>>>& Pages;
		TVoxelChunkedArray<uint8>& RootData;
		int32 NumClusters;
		const FVoxelBox& Bounds;
		const uint8 NumBitsPerIndex;
	};

	bool Build(FBuildData& BuildData);

	TVoxelArray<TUniquePtr<FCluster>> CreateClusters() const;

	Nanite::FResources Resources;
	FVoxelBox Bounds;

	TVoxelArray<TVoxelArray<TUniquePtr<FCluster>>> CreatePages(
		TVoxelArray<TUniquePtr<FCluster>>& Clusters,
		const Voxel::Nanite::FEncodingSettings& EncodingSettings) const;
};