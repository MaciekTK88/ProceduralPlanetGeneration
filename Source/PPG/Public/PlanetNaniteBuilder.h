// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "VoxelMinimal.h"
#include "StaticMeshResources.h"

struct PPG_API FPlanetNaniteBuilder
{
public:
	// Triangle list
	struct FMesh
	{
		TConstVoxelArrayView<FVector3f> Positions;
		TConstVoxelArrayView<FVoxelOctahedron> Normals;
		TArray<FVector3f>* Normals3f;
		TArray<FVector3f>* Positions3f;
		// Optional
		TConstVoxelArrayView<FColor> Colors;
		// Optional
		TVoxelArray<TConstVoxelArrayView<FVector2f>> TextureCoordinates;
	};
	FMesh Mesh;

	struct FMeshOutput
	{
		TPimplPtr<Nanite::FResources> NaniteResources;
		FBoxSphereBounds Bounds;
		FStaticMeshLODResources* LODResource;
	};
	FMeshOutput Output;

	// Step is 2^(-PositionPrecision)
	int32 PositionPrecision = 4;
	static constexpr int32 NormalBits = 8;

	TUniquePtr<FStaticMeshRenderData> CreateRenderData(TVoxelArray<int32>& OutVertexOffsets, bool bRaytracing);
	FVoxelBox CalculateBounds();
	
	//UStaticMesh* CreateStaticMesh();

public:
	TArray<uint32>* TrianglesM;

	static void ApplyRenderData(
		UStaticMesh& StaticMesh,
		TUniquePtr<FStaticMeshRenderData> RenderData,
		bool bRaytracing,
		bool bCollision,
		Chaos::FTriangleMeshImplicitObjectPtr ChaosMeshData);

	static UStaticMesh* CreateStaticMesh(TUniquePtr<FStaticMeshRenderData> RenderData, bool bRaytracing, bool bCollision, FPlanetNaniteBuilder& MeshData);
};