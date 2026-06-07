#include "Zenith.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "ZenithECS/Zenith_Query.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cfloat>

// Global physics mesh configuration - CRITICAL: Explicitly initialize to ensure defaults are set
PhysicsMeshConfig g_xPhysicsMeshConfig = {
	PHYSICS_MESH_QUALITY_HIGH,  // m_eQuality: Use high quality (full mesh geometry)
	1.0f,                        // m_fSimplificationRatio: 1.0 = no simplification (exact match to render mesh)
	100,                         // m_uMinTriangles
	10000,                       // m_uMaxTriangles
	true,                        // m_bAutoGenerate: Automatically generate physics meshes on load
	Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)  // m_xDebugColor: Green
};


namespace
{
	// Renderer-neutral generated collision geometry. The generation helpers below
	// produce this; the asset side (Zenith_MeshGeometryAsset::CreateFromGeometryData)
	// turns it into the engine's Flux_MeshGeometry, so this TU names no renderer type.
	struct GeneratedMeshData
	{
		Zenith_Vector<Zenith_Maths::Vector3> m_xPositions;
		Zenith_Vector<Zenith_Maths::Vector3> m_xNormals;
		Zenith_Vector<uint32_t> m_xIndices;

		bool IsValid() const { return m_xPositions.GetSize() >= 3 && m_xIndices.GetSize() >= 3; }
	};

	struct DecimationCellKey
	{
		int32_t x, y, z;
		bool operator==(const DecimationCellKey& other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};

	struct DecimationCellKeyHash
	{
		size_t operator()(const DecimationCellKey& k) const
		{
			return std::hash<int32_t>()(k.x) ^
				(std::hash<int32_t>()(k.y) << 1) ^
				(std::hash<int32_t>()(k.z) << 2);
		}
	};

	using CellToVertexMap = std::unordered_map<DecimationCellKey, uint32_t, DecimationCellKeyHash>;
	using OldToNewIndexMap = std::unordered_map<uint32_t, uint32_t>;

	DecimationCellKey ComputeCellKey(const Zenith_Maths::Vector3& xPos, float fInvCellSize)
	{
		DecimationCellKey xKey;
		xKey.x = static_cast<int32_t>(std::floor(xPos.x * fInvCellSize));
		xKey.y = static_cast<int32_t>(std::floor(xPos.y * fInvCellSize));
		xKey.z = static_cast<int32_t>(std::floor(xPos.z * fInvCellSize));
		return xKey;
	}

	// Phase 1: Extreme vertices (min/max on each axis) must be preserved to keep
	// the bounding volume intact. Each gets a unique slot in the cell map, even
	// if their raw cell collides, unless they share the same position exactly.
	void PreserveExtremeVertices(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		const std::unordered_set<uint32_t>& xExtremeSet,
		float fInvCellSize,
		CellToVertexMap& xCellToVertex,
		OldToNewIndexMap& xOldToNewIndex,
		Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut)
	{
		for (uint32_t i = 0; i < xPositions.GetSize(); i++)
		{
			if (xExtremeSet.find(i) == xExtremeSet.end())
				continue;

			const Zenith_Maths::Vector3& xPos = xPositions.Get(i);
			const DecimationCellKey xKey = ComputeCellKey(xPos, fInvCellSize);

			DecimationCellKey xUniqueKey = xKey;
			bool bMerged = false;
			while (xCellToVertex.find(xUniqueKey) != xCellToVertex.end())
			{
				const uint32_t uExistingIdx = xCellToVertex[xUniqueKey];
				if (Zenith_Maths::Length(xPositionsOut.Get(uExistingIdx) - xPos) < 0.0001f)
				{
					xOldToNewIndex[i] = uExistingIdx;
					bMerged = true;
					break;
				}
				xUniqueKey.x += 1000000;
			}

			if (!bMerged)
			{
				const uint32_t uNewIdx = xPositionsOut.GetSize();
				xPositionsOut.PushBack(xPos);
				xCellToVertex[xKey] = uNewIdx;
				xOldToNewIndex[i] = uNewIdx;
			}
		}
	}

	// Phase 2: Non-extreme vertices that land in the same cell collapse to a
	// single representative — the first occupant wins.
	void MergeNonExtremeVertices(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		const std::unordered_set<uint32_t>& xExtremeSet,
		float fInvCellSize,
		CellToVertexMap& xCellToVertex,
		OldToNewIndexMap& xOldToNewIndex,
		Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut)
	{
		for (uint32_t i = 0; i < xPositions.GetSize(); i++)
		{
			if (xExtremeSet.find(i) != xExtremeSet.end())
				continue;

			const Zenith_Maths::Vector3& xPos = xPositions.Get(i);
			const DecimationCellKey xKey = ComputeCellKey(xPos, fInvCellSize);

			const auto xIt = xCellToVertex.find(xKey);
			if (xIt == xCellToVertex.end())
			{
				const uint32_t uNewIdx = xPositionsOut.GetSize();
				xPositionsOut.PushBack(xPos);
				xCellToVertex[xKey] = uNewIdx;
				xOldToNewIndex[i] = uNewIdx;
			}
			else
			{
				xOldToNewIndex[i] = xIt->second;
			}
		}
	}

	// Phase 3: Rewrite indices against the new vertex slots and drop triangles
	// that collapsed to a line or point.
	void RemapAndFilterIndices(
		const Zenith_Vector<uint32_t>& xIndices,
		OldToNewIndexMap& xOldToNewIndex,
		Zenith_Vector<uint32_t>& xIndicesOut)
	{
		for (uint32_t t = 0; t < xIndices.GetSize(); t += 3)
		{
			const uint32_t i0 = xOldToNewIndex[xIndices.Get(t + 0)];
			const uint32_t i1 = xOldToNewIndex[xIndices.Get(t + 1)];
			const uint32_t i2 = xOldToNewIndex[xIndices.Get(t + 2)];

			if (i0 != i1 && i1 != i2 && i2 != i0)
			{
				xIndicesOut.PushBack(i0);
				xIndicesOut.PushBack(i1);
				xIndicesOut.PushBack(i2);
			}
		}
	}

	void ComputeAABB(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		Zenith_Maths::Vector3& xMinOut,
		Zenith_Maths::Vector3& xMaxOut)
	{
		xMinOut = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
		xMaxOut = Zenith_Maths::Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (uint32_t m = 0; m < xMeshViews.GetSize(); m++)
		{
			const Zenith_PhysicsMeshView& xView = xMeshViews.Get(m);
			if (!xView.m_pxPositions)
			{
				continue;
			}

			for (uint32_t v = 0; v < xView.m_uNumVerts; v++)
			{
				const Zenith_Maths::Vector3& xPos = xView.m_pxPositions[v];
				xMinOut.x = std::min(xMinOut.x, xPos.x);
				xMinOut.y = std::min(xMinOut.y, xPos.y);
				xMinOut.z = std::min(xMinOut.z, xPos.z);
				xMaxOut.x = std::max(xMaxOut.x, xPos.x);
				xMaxOut.y = std::max(xMaxOut.y, xPos.y);
				xMaxOut.z = std::max(xMaxOut.z, xPos.z);
			}
		}
	}

	void CollectAllPositions(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut)
	{
		xPositionsOut.Clear();

		for (uint32_t m = 0; m < xMeshViews.GetSize(); m++)
		{
			const Zenith_PhysicsMeshView& xView = xMeshViews.Get(m);
			if (!xView.m_pxPositions)
			{
				continue;
			}

			for (uint32_t v = 0; v < xView.m_uNumVerts; v++)
			{
				xPositionsOut.PushBack(xView.m_pxPositions[v]);
			}
		}
	}

	void DecimateVertices(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		const Zenith_Vector<uint32_t>& xIndices,
		Zenith_Vector<Zenith_Maths::Vector3>& xPositionsOut,
		Zenith_Vector<uint32_t>& xIndicesOut,
		float fCellSize)
	{
		xPositionsOut.Clear();
		xIndicesOut.Clear();

		if (xPositions.GetSize() == 0 || xIndices.GetSize() < 3 || fCellSize <= 0.0f)
		{
			return;
		}

		uint32_t uExtremeIndices[6];
		Zenith_PhysicsMeshGenerator::FindExtremeVertexIndices(xPositions, uExtremeIndices);

		std::unordered_set<uint32_t> xExtremeVertexSet;
		for (int i = 0; i < 6; i++)
		{
			xExtremeVertexSet.insert(uExtremeIndices[i]);
		}

		CellToVertexMap xCellToVertex;
		OldToNewIndexMap xOldToNewIndex;
		const float fInvCellSize = 1.0f / fCellSize;

		PreserveExtremeVertices(xPositions, xExtremeVertexSet, fInvCellSize, xCellToVertex, xOldToNewIndex, xPositionsOut);
		MergeNonExtremeVertices(xPositions, xExtremeVertexSet, fInvCellSize, xCellToVertex, xOldToNewIndex, xPositionsOut);
		RemapAndFilterIndices(xIndices, xOldToNewIndex, xIndicesOut);
	}

	void CreateBoxMesh(
		const Zenith_Maths::Vector3& xMin,
		const Zenith_Maths::Vector3& xMax,
		GeneratedMeshData& xOut)
	{
		xOut.m_xPositions.Clear();
		xOut.m_xNormals.Clear();
		xOut.m_xIndices.Clear();

		// 8 corners of the box (back-bottom-left to front-top-right)
		const Zenith_Maths::Vector3 axCorners[8] = {
			Zenith_Maths::Vector3(xMin.x, xMin.y, xMin.z), // 0: BBL
			Zenith_Maths::Vector3(xMax.x, xMin.y, xMin.z), // 1: BBR
			Zenith_Maths::Vector3(xMax.x, xMax.y, xMin.z), // 2: BTR
			Zenith_Maths::Vector3(xMin.x, xMax.y, xMin.z), // 3: BTL
			Zenith_Maths::Vector3(xMin.x, xMin.y, xMax.z), // 4: FBL
			Zenith_Maths::Vector3(xMax.x, xMin.y, xMax.z), // 5: FBR
			Zenith_Maths::Vector3(xMax.x, xMax.y, xMax.z), // 6: FTR
			Zenith_Maths::Vector3(xMin.x, xMax.y, xMax.z), // 7: FTL
		};

		const Zenith_Maths::Vector3 xCenter = (xMin + xMax) * 0.5f;
		for (int i = 0; i < 8; i++)
		{
			xOut.m_xPositions.PushBack(axCorners[i]);
			xOut.m_xNormals.PushBack(Zenith_Maths::Normalize(axCorners[i] - xCenter));
		}

		// 12 triangles (2 per face, 6 faces) — identical winding to the previous Flux build
		static const uint32_t auBoxIndices[36] = {
			0, 2, 1,  0, 3, 2,   // Back face (-Z)
			4, 5, 6,  4, 6, 7,   // Front face (+Z)
			0, 4, 7,  0, 7, 3,   // Left face (-X)
			1, 2, 6,  1, 6, 5,   // Right face (+X)
			0, 1, 5,  0, 5, 4,   // Bottom face (-Y)
			3, 7, 6,  3, 6, 2,   // Top face (+Y)
		};
		for (int i = 0; i < 36; i++)
		{
			xOut.m_xIndices.PushBack(auBoxIndices[i]);
		}
	}

	void CreateMeshFromData(
		const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
		const Zenith_Vector<uint32_t>& xIndices,
		GeneratedMeshData& xOut)
	{
		xOut.m_xPositions.Clear();
		xOut.m_xNormals.Clear();
		xOut.m_xIndices.Clear();

		const uint32_t uNumVerts = xPositions.GetSize();
		const uint32_t uNumIndices = xIndices.GetSize();
		if (uNumVerts == 0 || uNumIndices < 3)
		{
			return;
		}

		xOut.m_xPositions = xPositions;
		xOut.m_xIndices = xIndices;
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			xOut.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		}

		// Generate smooth vertex normals over the (now contiguous) buffers.
		Zenith_PhysicsMeshGenerator::ComputeVertexNormals(
			xOut.m_xNormals.GetDataPointer(), xOut.m_xPositions.GetDataPointer(),
			uNumVerts, xOut.m_xIndices.GetDataPointer(), uNumIndices);
	}

	void GenerateConvexHullMesh(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		GeneratedMeshData& xOut);

	void GenerateAABBMesh(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		GeneratedMeshData& xOut)
	{
		Zenith_Maths::Vector3 xMin, xMax;
		ComputeAABB(xMeshViews, xMin, xMax);

		// Validate AABB
		if (xMin.x > xMax.x || xMin.y > xMax.y || xMin.z > xMax.z)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Invalid AABB computed, using unit box");
			xMin = Zenith_Maths::Vector3(-0.5f, -0.5f, -0.5f);
			xMax = Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
		}

		Zenith_Log(LOG_CATEGORY_PHYSICS, " AABB bounds: (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)",
			xMin.x, xMin.y, xMin.z,
			xMax.x, xMax.y, xMax.z);

		CreateBoxMesh(xMin, xMax, xOut);
	}

	void GenerateSimplifiedMesh(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		const PhysicsMeshConfig& xConfig,
		GeneratedMeshData& xOut)
	{
		// Collect all positions and indices from all views
		Zenith_Vector<Zenith_Maths::Vector3> xAllPositions;
		Zenith_Vector<uint32_t> xAllIndices;

		uint32_t uVertexOffset = 0;
		for (uint32_t m = 0; m < xMeshViews.GetSize(); m++)
		{
			const Zenith_PhysicsMeshView& xView = xMeshViews.Get(m);
			if (!xView.m_pxPositions || !xView.m_puIndices)
			{
				Zenith_Log(LOG_CATEGORY_PHYSICS, " Skipping invalid mesh view %u", m);
				continue;
			}

			Zenith_Log(LOG_CATEGORY_PHYSICS, " Collecting mesh %u: %u verts, %u indices",
				m, xView.m_uNumVerts, xView.m_uNumIndices);

			for (uint32_t v = 0; v < xView.m_uNumVerts; v++)
			{
				xAllPositions.PushBack(xView.m_pxPositions[v]);
			}

			for (uint32_t i = 0; i < xView.m_uNumIndices; i++)
			{
				xAllIndices.PushBack(xView.m_puIndices[i] + uVertexOffset);
			}

			uVertexOffset += xView.m_uNumVerts;
		}

		Zenith_Log(LOG_CATEGORY_PHYSICS, " Total collected: %u vertices, %u indices from %u meshes",
			xAllPositions.GetSize(), xAllIndices.GetSize(), xMeshViews.GetSize());

		if (xAllPositions.GetSize() == 0 || xAllIndices.GetSize() < 3)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " No valid geometry for simplified mesh, using AABB fallback");
			GenerateAABBMesh(xMeshViews, xOut);
			return;
		}

		// Compute AABB for cell size calculation
		Zenith_Maths::Vector3 xMin, xMax;
		Zenith_PhysicsMeshGenerator::ComputeAABBFromPositions(xAllPositions, xMin, xMax);

		Zenith_Maths::Vector3 xExtent = xMax - xMin;
		float fMaxExtent = std::max(std::max(xExtent.x, xExtent.y), xExtent.z);

		// Calculate target vertex count based on simplification ratio
		uint32_t uSourceTriCount = xAllIndices.GetSize() / 3;
		uint32_t uTargetTriCount = static_cast<uint32_t>(uSourceTriCount * xConfig.m_fSimplificationRatio);
		uTargetTriCount = std::max(uTargetTriCount, xConfig.m_uMinTriangles);
		uTargetTriCount = std::min(uTargetTriCount, xConfig.m_uMaxTriangles);

		// Skip decimation if ratio is 1.0 (no simplification desired)
		if (xConfig.m_fSimplificationRatio >= 1.0f)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Skipping decimation (ratio=1.0): using full mesh with %u vertices, %u triangles",
				xAllPositions.GetSize(), uSourceTriCount);
			CreateMeshFromData(xAllPositions, xAllIndices, xOut);
			return;
		}

		// Iteratively decimate until we reach target triangle count
		Zenith_Vector<Zenith_Maths::Vector3> xCurrentPositions = xAllPositions;
		Zenith_Vector<uint32_t> xCurrentIndices = xAllIndices;

		float fCellSize = fMaxExtent * 0.01f; // Start with small cell size
		const float fCellSizeMultiplier = 1.5f;
		const int iMaxIterations = 10;

		for (int iter = 0; iter < iMaxIterations; iter++)
		{
			uint32_t uCurrentTriCount = xCurrentIndices.GetSize() / 3;
			if (uCurrentTriCount <= uTargetTriCount)
			{
				Zenith_Log(LOG_CATEGORY_PHYSICS, " Decimation complete: reached target tri count %u", uCurrentTriCount);
				break;
			}

			Zenith_Vector<Zenith_Maths::Vector3> xDecimatedPositions;
			Zenith_Vector<uint32_t> xDecimatedIndices;

			DecimateVertices(xCurrentPositions, xCurrentIndices, xDecimatedPositions, xDecimatedIndices, fCellSize);

			Zenith_Log(LOG_CATEGORY_PHYSICS, " Decimation iter %d (cell size %.4f): %u -> %u verts, %u -> %u tris",
				iter, fCellSize,
				xCurrentPositions.GetSize(), xDecimatedPositions.GetSize(),
				xCurrentIndices.GetSize() / 3, xDecimatedIndices.GetSize() / 3);

			if (xDecimatedIndices.GetSize() >= 3)
			{
				xCurrentPositions = xDecimatedPositions;
				xCurrentIndices = xDecimatedIndices;
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_PHYSICS, " Decimation produced invalid geometry, stopping");
				break;
			}

			fCellSize *= fCellSizeMultiplier;
		}

		// Ensure we have valid geometry
		if (xCurrentPositions.GetSize() < 3 || xCurrentIndices.GetSize() < 3)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Decimation produced invalid geometry, using convex hull fallback");
			GenerateConvexHullMesh(xMeshViews, xOut);
			return;
		}

		Zenith_Log(LOG_CATEGORY_PHYSICS, " Simplified mesh: %u -> %u vertices, %u -> %u triangles",
			xAllPositions.GetSize(), xCurrentPositions.GetSize(),
			xAllIndices.GetSize() / 3, xCurrentIndices.GetSize() / 3);

		CreateMeshFromData(xCurrentPositions, xCurrentIndices, xOut);
	}

	// Simple quickhull-style convex hull approximation (delegates to the decimator)
	void GenerateConvexHullMesh(
		const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
		GeneratedMeshData& xOut)
	{
		Zenith_Vector<Zenith_Maths::Vector3> xAllPositions;
		CollectAllPositions(xMeshViews, xAllPositions);

		if (xAllPositions.GetSize() < 4)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Not enough vertices for convex hull (%u), using AABB fallback",
				xAllPositions.GetSize());
			GenerateAABBMesh(xMeshViews, xOut);
			return;
		}

		// Find extreme points using shared helper
		uint32_t auExtremeIndices[6];
		Zenith_PhysicsMeshGenerator::FindExtremeVertexIndices(xAllPositions, auExtremeIndices);

		Zenith_Maths::Vector3 xMinPt[3], xMaxPt[3];
		for (int axis = 0; axis < 3; axis++)
		{
			xMinPt[axis] = xAllPositions.Get(auExtremeIndices[axis * 2]);
			xMaxPt[axis] = xAllPositions.Get(auExtremeIndices[axis * 2 + 1]);
		}

		// Collect unique extreme points
		Zenith_Vector<Zenith_Maths::Vector3> xHullPoints;
		auto AddUniquePoint = [&xHullPoints](const Zenith_Maths::Vector3& xPt) {
			for (uint32_t i = 0; i < xHullPoints.GetSize(); i++)
			{
				if (Zenith_Maths::Length(xHullPoints.Get(i) - xPt) < 0.001f)
					return;
			}
			xHullPoints.PushBack(xPt);
		};

		for (int axis = 0; axis < 3; axis++)
		{
			AddUniquePoint(xMinPt[axis]);
			AddUniquePoint(xMaxPt[axis]);
		}

		// If we have fewer than 4 unique points, add some intermediate points
		if (xHullPoints.GetSize() < 4)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Only %u unique extreme points, using AABB fallback",
				xHullPoints.GetSize());
			GenerateAABBMesh(xMeshViews, xOut);
			return;
		}

		// Use decimation on all vertices to create a better approximation that
		// follows the mesh shape.
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Using simplified mesh approach for better hull approximation");

		PhysicsMeshConfig xHullConfig;
		xHullConfig.m_eQuality = PHYSICS_MESH_QUALITY_HIGH;
		xHullConfig.m_fSimplificationRatio = 0.6f; // Moderate simplification for convex hulls
		xHullConfig.m_uMinTriangles = 24;
		xHullConfig.m_uMaxTriangles = 512; // Lower cap for convex hull

		GenerateSimplifiedMesh(xMeshViews, xHullConfig, xOut);
	}
}

const char* Zenith_PhysicsMeshGenerator::GetQualityName(PhysicsMeshQuality eQuality)
{
	switch (eQuality)
	{
	case PHYSICS_MESH_QUALITY_LOW:    return "LOW (AABB)";
	case PHYSICS_MESH_QUALITY_MEDIUM: return "MEDIUM (ConvexHull)";
	case PHYSICS_MESH_QUALITY_HIGH:   return "HIGH (SimplifiedMesh)";
	default:                          return "UNKNOWN";
	}
}

Zenith_MeshGeometryAsset* Zenith_PhysicsMeshGenerator::GeneratePhysicsMesh(
	const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
	PhysicsMeshQuality eQuality)
{
	PhysicsMeshConfig xConfig = g_xPhysicsMeshConfig;
	xConfig.m_eQuality = eQuality;
	return GeneratePhysicsMeshWithConfig(xMeshViews, xConfig);
}

Zenith_MeshGeometryAsset* Zenith_PhysicsMeshGenerator::GeneratePhysicsMeshWithConfig(
	const Zenith_Vector<Zenith_PhysicsMeshView>& xMeshViews,
	const PhysicsMeshConfig& xConfig)
{
	// Validate input
	if (xMeshViews.GetSize() == 0)
	{
		Zenith_Log(LOG_CATEGORY_PHYSICS, " No meshes provided for physics mesh generation");
		return nullptr;
	}

	// Count total triangles and vertices for logging
	uint32_t uTotalSourceVerts = 0;
	uint32_t uTotalSourceTris = 0;
	for (uint32_t i = 0; i < xMeshViews.GetSize(); i++)
	{
		const Zenith_PhysicsMeshView& xView = xMeshViews.Get(i);
		if (xView.m_pxPositions)
		{
			uTotalSourceVerts += xView.m_uNumVerts;
			uTotalSourceTris += xView.m_uNumIndices / 3;
		}
	}

	Zenith_Log(LOG_CATEGORY_PHYSICS, " Generating physics mesh from %u submeshes (%u verts, %u tris), quality=%s",
		xMeshViews.GetSize(),
		uTotalSourceVerts,
		uTotalSourceTris,
		GetQualityName(xConfig.m_eQuality));

	GeneratedMeshData xData;

	switch (xConfig.m_eQuality)
	{
	case PHYSICS_MESH_QUALITY_LOW:
		GenerateAABBMesh(xMeshViews, xData);
		break;

	case PHYSICS_MESH_QUALITY_MEDIUM:
		GenerateConvexHullMesh(xMeshViews, xData);
		break;

	case PHYSICS_MESH_QUALITY_HIGH:
		GenerateSimplifiedMesh(xMeshViews, xConfig, xData);
		break;

	default:
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Unknown quality level %d, falling back to AABB", xConfig.m_eQuality);
		GenerateAABBMesh(xMeshViews, xData);
		break;
	}

	if (!xData.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Failed to generate physics mesh, attempting AABB fallback");
		GenerateAABBMesh(xMeshViews, xData);
	}

	if (xData.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Generated physics mesh: %u verts, %u tris",
			xData.m_xPositions.GetSize(),
			xData.m_xIndices.GetSize() / 3);

		// The asset side owns the renderer-mesh (Flux_MeshGeometry) construction, so
		// this TU stays renderer-neutral.
		return Zenith_MeshGeometryAsset::CreateFromGeometryData(
			xData.m_xPositions, xData.m_xNormals, xData.m_xIndices);
	}

	return nullptr;
}

void Zenith_PhysicsMeshGenerator::FindExtremeVertexIndices(
	const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	uint32_t auOutIndices[6])
{
	float afExtremeValues[6] = { FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX };
	for (int i = 0; i < 6; i++) auOutIndices[i] = 0;

	for (uint32_t i = 0; i < xPositions.GetSize(); i++)
	{
		const Zenith_Maths::Vector3& xPos = xPositions.Get(i);
		if (xPos.x < afExtremeValues[0]) { afExtremeValues[0] = xPos.x; auOutIndices[0] = i; }
		if (xPos.x > afExtremeValues[1]) { afExtremeValues[1] = xPos.x; auOutIndices[1] = i; }
		if (xPos.y < afExtremeValues[2]) { afExtremeValues[2] = xPos.y; auOutIndices[2] = i; }
		if (xPos.y > afExtremeValues[3]) { afExtremeValues[3] = xPos.y; auOutIndices[3] = i; }
		if (xPos.z < afExtremeValues[4]) { afExtremeValues[4] = xPos.z; auOutIndices[4] = i; }
		if (xPos.z > afExtremeValues[5]) { afExtremeValues[5] = xPos.z; auOutIndices[5] = i; }
	}
}

void Zenith_PhysicsMeshGenerator::ComputeAABBFromPositions(
	const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	Zenith_Maths::Vector3& xMinOut,
	Zenith_Maths::Vector3& xMaxOut)
{
	xMinOut = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	xMaxOut = Zenith_Maths::Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (uint32_t i = 0; i < xPositions.GetSize(); i++)
	{
		const Zenith_Maths::Vector3& xPos = xPositions.Get(i);
		xMinOut.x = std::min(xMinOut.x, xPos.x);
		xMinOut.y = std::min(xMinOut.y, xPos.y);
		xMinOut.z = std::min(xMinOut.z, xPos.z);
		xMaxOut.x = std::max(xMaxOut.x, xPos.x);
		xMaxOut.y = std::max(xMaxOut.y, xPos.y);
		xMaxOut.z = std::max(xMaxOut.z, xPos.z);
	}
}

void Zenith_PhysicsMeshGenerator::ComputeVertexNormals(
	Zenith_Maths::Vector3* pxNormals,
	const Zenith_Maths::Vector3* pxPositions,
	uint32_t uNumVerts,
	const uint32_t* puIndices,
	uint32_t uNumIndices)
{
	for (uint32_t i = 0; i < uNumVerts; i++)
		pxNormals[i] = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);

	for (uint32_t t = 0; t < uNumIndices; t += 3)
	{
		uint32_t i0 = puIndices[t + 0];
		uint32_t i1 = puIndices[t + 1];
		uint32_t i2 = puIndices[t + 2];

		Zenith_Maths::Vector3 xEdge1 = pxPositions[i1] - pxPositions[i0];
		Zenith_Maths::Vector3 xEdge2 = pxPositions[i2] - pxPositions[i0];
		Zenith_Maths::Vector3 xFaceNormal = Zenith_Maths::Cross(xEdge1, xEdge2);

		pxNormals[i0] += xFaceNormal;
		pxNormals[i1] += xFaceNormal;
		pxNormals[i2] += xFaceNormal;
	}

	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		float fLen = Zenith_Maths::Length(pxNormals[i]);
		if (fLen > 0.0001f)
			pxNormals[i] = pxNormals[i] / fLen;
		else
			pxNormals[i] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	}
}

void Zenith_PhysicsMeshGenerator::QueuePhysicsDebugDraws()
{
	Zenith_Vector<Zenith_ModelComponent*> xModels;
	xModels.Clear();
	g_xEngine.Scenes().QueryAllScenes<Zenith_ModelComponent>().ForEach([&xModels](Zenith_EntityID, Zenith_ModelComponent& xComp) { xModels.PushBack(&xComp); });

	for (uint32_t i = 0; i < xModels.GetSize(); i++)
	{
		Zenith_ModelComponent* pxModel = xModels.Get(i);
		if (!pxModel || !pxModel->GetDebugDrawPhysicsMesh())
		{
			continue;
		}

		pxModel->QueueDebugDrawPhysicsMesh(g_xPhysicsMeshConfig.m_xDebugColor);
	}
	// Audit §3.18 fix: iterate all loaded scenes, not just the active one.
	// Physics debug visualisation should surface every loaded scene's model
	// components — e.g. props in additively-loaded scenes, characters in the
	// persistent scene. Ref: Unity's GameObject.scene contract —
	// https://docs.unity3d.com/ScriptReference/GameObject-scene.html

	Zenith_Vector<Zenith_ColliderComponent*> xColliders;
	xColliders.Clear();
	g_xEngine.Scenes().QueryAllScenes<Zenith_ColliderComponent>().ForEach([&xColliders](Zenith_EntityID, Zenith_ColliderComponent& xComp) { xColliders.PushBack(&xComp); });

	for (uint32_t i = 0; i < xColliders.GetSize(); i++)
	{
		Zenith_ColliderComponent* pxCollider = xColliders.Get(i);
		if (!pxCollider || !pxCollider->GetDebugDrawPhysicsMesh())
		{
			continue;
		}

		pxCollider->QueueDebugDraw(g_xPhysicsMeshConfig.m_xDebugColor);
	}
}
