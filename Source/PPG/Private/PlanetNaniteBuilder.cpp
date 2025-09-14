// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "PlanetNaniteBuilder.h"

#include "VoxelChaosTriangleMeshCooker.h"
#include "VoxelCore/Private/VoxelNanite.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "StaticMeshResources.h"
#include "MeshCardRepresentation.h"
#include "MeshCardBuild.h"
#include "Rendering/NaniteResources.h"

FVoxelBox FPlanetNaniteBuilder::CalculateBounds()
{
	return FVoxelBox::FromPositions(Mesh.Positions);
}

TUniquePtr<FStaticMeshRenderData> FPlanetNaniteBuilder::CreateRenderData(TVoxelArray<int32>& OutVertexOffsets, bool bRaytracing)
{
	//VOXEL_FUNCTION_COUNTER();
	check(Mesh.Positions.Num() == Mesh.Normals.Num());
	check(Mesh.Positions.Num() % 3 == 0);

	if (!ensure(Mesh.Positions.Num() > 0))
	{
		//return {};
	}

	using namespace Voxel::Nanite;

	const FVoxelBox Bounds = FVoxelBox::FromPositions(Mesh.Positions);

	Nanite::FResources Resources;

	TVoxelArray<FCluster> AllClusters;
	for (int32 TriangleIndex = 0; TriangleIndex < TrianglesM->Num() / 3; TriangleIndex++)
	{
		if (AllClusters.Num() == 0 ||
			AllClusters.Last().NumTriangles() == NANITE_MAX_CLUSTER_TRIANGLES ||
			AllClusters.Last().Positions.Num() + 3 > NANITE_MAX_CLUSTER_VERTICES)
		{
			FCluster& Cluster = AllClusters.Emplace_GetRef();
			Cluster.TextureCoordinates.SetNum(Mesh.TextureCoordinates.Num());

			for (TVoxelArray<FVector2f>& TextureCoordinate : Cluster.TextureCoordinates)
			{
				TextureCoordinate.Reserve(128);
			}
		}

		FCluster& Cluster = AllClusters.Last();

		const int32 IndexA = (*TrianglesM)[3 * TriangleIndex];//3 * TriangleIndex + 0;
		const int32 IndexB = (*TrianglesM)[3 * TriangleIndex + 1];//3 * TriangleIndex + 1;
		const int32 IndexC = (*TrianglesM)[3 * TriangleIndex + 2];//3 * TriangleIndex + 2;

		Cluster.Positions.Add(Mesh.Positions[IndexA]);
		Cluster.Positions.Add(Mesh.Positions[IndexB]);
		Cluster.Positions.Add(Mesh.Positions[IndexC]);

		Cluster.Normals.Add(Mesh.Normals[IndexA]);
		Cluster.Normals.Add(Mesh.Normals[IndexB]);
		Cluster.Normals.Add(Mesh.Normals[IndexC]);

		if (Mesh.Colors.Num() > 0)
		{
			Cluster.Colors.Add(Mesh.Colors[IndexA]);
			Cluster.Colors.Add(Mesh.Colors[IndexB]);
			Cluster.Colors.Add(Mesh.Colors[IndexC]);
		}

		for (int32 UVIndex = 0; UVIndex < Cluster.TextureCoordinates.Num(); UVIndex++)
		{
			Cluster.TextureCoordinates[UVIndex].Add(Mesh.TextureCoordinates[UVIndex][IndexA]);
			Cluster.TextureCoordinates[UVIndex].Add(Mesh.TextureCoordinates[UVIndex][IndexB]);
			Cluster.TextureCoordinates[UVIndex].Add(Mesh.TextureCoordinates[UVIndex][IndexC]);
		}
	}


	const int32 TreeDepth = FMath::CeilToInt(FMath::LogX(4.f, AllClusters.Num()));
	ensure(TreeDepth < NANITE_MAX_CLUSTER_HIERARCHY_DEPTH);

	const auto MakeHierarchyNode = [&]
	{
		Nanite::FPackedHierarchyNode HierarchyNode;

		for (int32 Index = 0; Index < 4; Index++)
		{
			HierarchyNode.LODBounds[Index] = FVector4f(
				Bounds.GetCenter().X,
				Bounds.GetCenter().Y,
				Bounds.GetCenter().Z,
				Bounds.Size().Length());

			HierarchyNode.Misc0[Index].MinLODError_MaxParentLODError = FFloat16(-1).Encoded | (FFloat16(1e10f).Encoded << 16);
			HierarchyNode.Misc0[Index].BoxBoundsCenter = FVector3f(Bounds.GetCenter());
			HierarchyNode.Misc1[Index].BoxBoundsExtent = FVector3f(Bounds.GetExtent());
			HierarchyNode.Misc1[Index].ChildStartReference = 0xFFFFFFFF;
			HierarchyNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize = 0;
		}

		return HierarchyNode;
	};

	TVoxelArray<int32> LeafNodes;
	for (int32 Depth = 0; Depth <= TreeDepth; Depth++)
	{
		if (Depth == 0)
		{
			Resources.HierarchyNodes.Add(MakeHierarchyNode());
			LeafNodes.Add(0);
			continue;
		}

		Resources.HierarchyNodes.Reserve(Resources.HierarchyNodes.Num() + 4 * LeafNodes.Num());

		TVoxelArray<int32> NewLeafNodes;
		for (const int32 ParentIndex : LeafNodes)
		{
			Nanite::FPackedHierarchyNode& ParentNode = Resources.HierarchyNodes[ParentIndex];
			for (int32 Index = 0; Index < 4; Index++)
			{
				const int32 ChildIndex = Resources.HierarchyNodes.Add(MakeHierarchyNode());

				ensure(ParentNode.Misc1[Index].ChildStartReference == 0xFFFFFFFF);
				ensure(ParentNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize == 0);

				ParentNode.Misc1[Index].ChildStartReference = ChildIndex;
				ParentNode.Misc2[Index].ResourcePageIndex_NumPages_GroupPartSize = 0xFFFFFFFF;

				NewLeafNodes.Add(ChildIndex);
			}
		}

		LeafNodes = MoveTemp(NewLeafNodes);
	}
	check(AllClusters.Num() <= LeafNodes.Num());

	for (int32 ClusterIndex = 0; ClusterIndex < AllClusters.Num(); ClusterIndex++)
	{
		Nanite::FPackedHierarchyNode& HierarchyNode = Resources.HierarchyNodes[LeafNodes[ClusterIndex]];

		ensure(HierarchyNode.Misc1[0].ChildStartReference == 0xFFFFFFFF);
		ensure(HierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize == 0);

		HierarchyNode.Misc1[0].ChildStartReference = Resources.HierarchyNodes.Num() + ClusterIndex;
		HierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize = 0xFFFFFFFF;
	}

	FEncodingSettings EncodingSettings;
	EncodingSettings.PositionPrecision = PositionPrecision;
	checkStatic(FEncodingSettings::NormalBits == NormalBits);

	TVoxelArray<TVoxelArray<FCluster>> Pages;
	{
		int32 ClusterIndex = 0;
		while (ClusterIndex < AllClusters.Num())
		{
			TVoxelArray<FCluster>& Clusters = Pages.Emplace_GetRef();
			int32 GpuSize = 0;

			while (
				ClusterIndex < AllClusters.Num() &&
				Clusters.Num() < NANITE_ROOT_PAGE_MAX_CLUSTERS)
			{
				FCluster& Cluster = AllClusters[ClusterIndex];

				const int32 ClusterGpuSize = Cluster.GetEncodingInfo(EncodingSettings).GpuSizes.GetTotal();
				if (GpuSize + ClusterGpuSize > NANITE_ROOT_PAGE_GPU_SIZE)
				{
					break;
				}

				Clusters.Add(MoveTemp(Cluster));
				GpuSize += ClusterGpuSize;
				ClusterIndex++;
			}

			ensure(GpuSize <= NANITE_ROOT_PAGE_GPU_SIZE);
		}
		check(ClusterIndex == AllClusters.Num());
	}

	TVoxelChunkedArray<uint8> RootData;

	int32 VertexOffset = 0;
	int32 ClusterIndexOffset = 0;
	for (int32 PageIndex = 0; PageIndex < Pages.Num(); PageIndex++)
	{
		TVoxelArray<FCluster>& Clusters = Pages[PageIndex];
		ON_SCOPE_EXIT
		{
			ClusterIndexOffset += Clusters.Num();
		};

		Nanite::FPageStreamingState PageStreamingState{};
		PageStreamingState.BulkOffset = RootData.Num();

#if VOXEL_ENGINE_VERSION < 506
		Nanite::FFixupChunk FixupChunk;
		FixupChunk.Header.Magic = NANITE_FIXUP_MAGIC;
		FixupChunk.Header.NumClusters = Clusters.Num();
		FixupChunk.Header.UE_506_SWITCH(NumHierachyFixups, NumHierarchyFixups) = Clusters.Num();
		FixupChunk.Header.NumClusterFixups = Clusters.Num();
#else
		Nanite::FFixupChunkBuffer FixupChunkBuffer;
		Nanite::FFixupChunk& FixupChunk = FixupChunkBuffer.Add_GetRef(
			Clusters.Num(),
			Clusters.Num(),
			Clusters.Num());
#endif

#if VOXEL_ENGINE_VERSION < 506
		const TVoxelArrayView<uint8> HierarchyFixupsData = MakeVoxelArrayView(FixupChunk.Data).LeftOf(sizeof(Nanite::FHierarchyFixup) * Clusters.Num());
		const TVoxelArrayView<uint8> ClusterFixupsData = MakeVoxelArrayView(FixupChunk.Data).RightOf(sizeof(Nanite::FHierarchyFixup) * Clusters.Num());
		const TVoxelArrayView<Nanite::FHierarchyFixup> HierarchyFixups = HierarchyFixupsData.ReinterpretAs<Nanite::FHierarchyFixup>();
		const TVoxelArrayView<Nanite::FClusterFixup> ClusterFixups = ClusterFixupsData.ReinterpretAs<Nanite::FClusterFixup>().LeftOf(Clusters.Num());
#endif

		for (int32 Index = 0; Index < Clusters.Num(); Index++)
		{
			UE_506_SWITCH(HierarchyFixups[Index], FixupChunk.GetHierarchyFixup(Index)) = Nanite::FHierarchyFixup(
				PageIndex,
				Resources.HierarchyNodes.Num() + ClusterIndexOffset + Index,
				0,
				Index,
				0,
				0);

			UE_506_SWITCH(ClusterFixups[Index], FixupChunk.GetClusterFixup(Index)) = Nanite::FClusterFixup(
				PageIndex,
				Index,
				0,
				0);
		}

#if VOXEL_ENGINE_VERSION < 506
		RootData.Append(MakeByteVoxelArrayView(FixupChunk).LeftOf(FixupChunk.GetSize()));
#else
		RootData.Append(TConstVoxelArrayView<uint8>(
			reinterpret_cast<uint8*>(&FixupChunk),
			FixupChunk.GetSize()));
#endif

		const int32 PageStartIndex = RootData.Num();

		OutVertexOffsets.Add(VertexOffset);

		CreatePageData(
			Clusters,
			EncodingSettings,
			RootData,
			VertexOffset);

		PageStreamingState.BulkSize = RootData.Num() - PageStreamingState.BulkOffset;
		PageStreamingState.PageSize = RootData.Num() - PageStartIndex;
		PageStreamingState.MaxHierarchyDepth = NANITE_MAX_CLUSTER_HIERARCHY_DEPTH;
		Resources.PageStreamingStates.Add(PageStreamingState);
	}

	for (int32 ClusterIndex = 0; ClusterIndex < AllClusters.Num(); ClusterIndex++)
	{
		Nanite::FPackedHierarchyNode PackedHierarchyNode;
		FMemory::Memzero(PackedHierarchyNode);

		PackedHierarchyNode.Misc0[0].BoxBoundsCenter = FVector3f(Bounds.GetCenter());
		PackedHierarchyNode.Misc0[0].MinLODError_MaxParentLODError = FFloat16(-1).Encoded | (FFloat16(1e10f).Encoded << 16);

		PackedHierarchyNode.Misc1[0].BoxBoundsExtent = FVector3f(Bounds.GetExtent());
		PackedHierarchyNode.Misc1[0].ChildStartReference = 0xFFFFFFFFu;

		const int32 PageIndexStart = 0;
		const int32 PageIndexNum = 0;
		const int32 GroupPartSize = 1;
		PackedHierarchyNode.Misc2[0].ResourcePageIndex_NumPages_GroupPartSize =
			(PageIndexStart << (NANITE_MAX_CLUSTERS_PER_GROUP_BITS + NANITE_MAX_GROUP_PARTS_BITS)) |
			(PageIndexNum << NANITE_MAX_CLUSTERS_PER_GROUP_BITS) |
			GroupPartSize;

		Resources.HierarchyNodes.Add(PackedHierarchyNode);
	}

	Resources.RootData = RootData.Array();
	Resources.PositionPrecision = -1;
	Resources.NormalPrecision = -1;
	Resources.NumInputTriangles = 0;
	Resources.NumInputVertices = Mesh.Positions.Num();
#if VOXEL_ENGINE_VERSION < 506
	Resources.NumInputMeshes = 1;
	Resources.NumInputTexCoords = Mesh.TextureCoordinates.Num();
#endif
	Resources.NumClusters = AllClusters.Num();
	Resources.NumRootPages = Pages.Num();
	Resources.HierarchyRootOffsets.Add(0);


	TUniquePtr<FStaticMeshRenderData> RenderData = MakeUnique<FStaticMeshRenderData>();
	RenderData->Bounds = Bounds.ToFBox();
	RenderData->NumInlinedLODs = 1;
	RenderData->NaniteResourcesPtr = MakePimpl<Nanite::FResources>(MoveTemp(Resources));

	FStaticMeshLODResources* LODResource = new FStaticMeshLODResources();
	LODResource->bBuffersInlined = true;
	LODResource->Sections.Emplace();

	if (bRaytracing)
	{
		// Ensure UStaticMesh::HasValidRenderData returns true
		// Use MAX_flt to try to not have the vertex picked by vertex snapping
		//const TVoxelArray<FVector3f> DummyPositions = { FVector3f(MAX_flt) };
		const int32 NumVertices = Mesh.Positions.Num();
		LODResource->VertexBuffers.PositionVertexBuffer.Init(*Mesh.Positions3f);
		LODResource->VertexBuffers.StaticMeshVertexBuffer.Init(NumVertices, Mesh.TextureCoordinates.Num());
		LODResource->VertexBuffers.ColorVertexBuffer.Init(Mesh.Colors.Num());

		for (int32 i = 0; i < NumVertices; i++)
		{
			FVector3f Normal = (*Mesh.Normals3f)[i].GetSafeNormal();
			FVector3f TangentX, TangentY;
			Normal.FindBestAxisVectors(TangentX, TangentY);

			LODResource->VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, TangentX, TangentY, Normal);
			LODResource->VertexBuffers.ColorVertexBuffer.VertexColor(i) = Mesh.Colors[i];

			for (int32 UVIndex = 0; UVIndex < Mesh.TextureCoordinates.Num(); UVIndex++)
			{
				LODResource->VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, UVIndex, Mesh.TextureCoordinates[UVIndex][i]);
			}
		}
	
		// Ensure FStaticMeshRenderData::GetFirstValidLODIdx doesn't return -1
		LODResource->IndexBuffer.SetIndices(*TrianglesM, EIndexBufferStride::Force32Bit);
		LODResource->Sections[0].FirstIndex = 0;
		LODResource->Sections[0].NumTriangles = TrianglesM->Num() / 3;

		LODResource->Sections[0].FirstIndex = 0;
		LODResource->Sections[0].NumTriangles = TrianglesM->Num() / 3;
		LODResource->Sections[0].MinVertexIndex = 0;
		LODResource->Sections[0].MaxVertexIndex = NumVertices - 1;
		LODResource->Sections[0].MaterialIndex = 0;
		//LODResource->Sections[0].bEnableCollision = false;
		//LODResource->Sections[0].bVisibleInRayTracing = true;

		LODResource->BuffersSize = NumVertices * sizeof(FVector3f);
		
		LODResource->IndexBuffer.TrySetAllowCPUAccess(false);

		RenderData->LODResources.Add(LODResource);

		RenderData->LODVertexFactories.Add(FStaticMeshVertexFactories(GMaxRHIFeatureLevel));

		
		//StaticMesh.AddSourceModel();
		//UStaticMesh* StaticMeshP = &StaticMesh;
		//BuildRayTracingFallback(StaticMeshP, *RenderData);
		RenderData->InitializeRayTracingRepresentationFromRenderingLODs();

		TPimplPtr<FCardRepresentationData> MeshCards;
		FBox Box = RenderData->Bounds.GetBox();

		if (MeshCards.IsValid() == false)
		{
			MeshCards = MakePimpl<FCardRepresentationData>();
		}

		*MeshCards = FCardRepresentationData();
		FMeshCardsBuildData& CardData = MeshCards->MeshCardsBuildData;

		CardData.Bounds = Box;
		// Mark as two-sided so a high sampling bias is used and hits are accepted even if they don't match well
		CardData.bMostlyTwoSided = true;

		MeshCardRepresentation::SetCardsFromBounds(CardData);
		RenderData->LODResources[0].CardRepresentationData = new FCardRepresentationData();
		RenderData->LODResources[0].CardRepresentationData->MeshCardsBuildData = CardData;
		
		
	}
	else
	{
		// Ensure UStaticMesh::HasValidRenderData returns true
		// Use MAX_flt to try to not have the vertex picked by vertex snapping
		const TVoxelArray<FVector3f> DummyPositions = { FVector3f(MAX_flt) };

		LODResource->VertexBuffers.StaticMeshVertexBuffer.Init(DummyPositions.Num(), 1);
		LODResource->VertexBuffers.PositionVertexBuffer.Init(DummyPositions);
		LODResource->VertexBuffers.ColorVertexBuffer.Init(DummyPositions.Num());

		// Ensure FStaticMeshRenderData::GetFirstValidLODIdx doesn't return -1
		LODResource->BuffersSize = 1;
		LODResource->IndexBuffer.TrySetAllowCPUAccess(false);

		RenderData->LODResources.Add(LODResource);

		RenderData->LODVertexFactories.Add(FStaticMeshVertexFactories(GMaxRHIFeatureLevel));

	}
	return RenderData;
}


/*
UStaticMesh* FPlanetNaniteBuilder::CreateStaticMesh()
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelArray<int32> VertexOffsets;
	return CreateStaticMesh(CreateRenderData(VertexOffsets));
}
*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FPlanetNaniteBuilder::ApplyRenderData(UStaticMesh& StaticMesh, TUniquePtr<FStaticMeshRenderData> RenderData, bool bRaytracing, bool bCollision, Chaos::FTriangleMeshImplicitObjectPtr ChaosMeshData)
{
	//VOXEL_FUNCTION_COUNTER();
	
	StaticMesh.ReleaseResources();
	// Not supported, among other issues FSceneProxy::FSceneProxy crashes because GetNumVertices is always 0
	StaticMesh.bSupportRayTracing = bRaytracing;
	//StaticMesh.RayTracingProxySettings.bEnabled = bRaytracing;

	
	if (bCollision)
	{
		//UStaticMesh* StaticMeshP = &StaticMesh;
		//BodySetup->BodySetupGuid = FGuid::NewGuid();
		UBodySetup* BodySetup = StaticMesh.GetBodySetup();
		BodySetup->bGenerateMirroredCollision = false;
		BodySetup->bDoubleSidedGeometry = false;
		BodySetup->bSupportUVsAndFaceRemap = false;
		BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

		
		ChaosMeshData->SetDoCollide(false);
		BodySetup->TriMeshGeometries = { ChaosMeshData };

		
		BodySetup->BodySetupGuid           = FGuid::NewGuid();
		BodySetup->bHasCookedCollisionData = true;
		BodySetup->bCreatedPhysicsMeshes = true;
		//StaticMesh.SetBodySetup(BodySetup);
	}

	//StaticMesh.LODForCollision = 0;
	StaticMesh.bDoFastBuild = true;
	
	StaticMesh.SetStaticMaterials({ FStaticMaterial() });
	StaticMesh.SetRenderData(MoveTemp(RenderData));
	StaticMesh.CalculateExtendedBounds();
	
#if WITH_EDITOR
	StaticMesh.NaniteSettings.bEnabled = true;
#endif

	//VOXEL_SCOPE_COUNTER("UStaticMesh::InitResources");
	StaticMesh.InitResources();
}

UStaticMesh* FPlanetNaniteBuilder::CreateStaticMesh(TUniquePtr<FStaticMeshRenderData> RenderData, bool bRaytracing, bool bCollision, FPlanetNaniteBuilder& MeshData)
{
	//VOXEL_FUNCTION_COUNTER();
	//check(IsInGameThread());

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>();
	//ApplyRenderData(*StaticMesh, MoveTemp(RenderData), bRaytracing, bCollision, MeshData);
	return StaticMesh;
}