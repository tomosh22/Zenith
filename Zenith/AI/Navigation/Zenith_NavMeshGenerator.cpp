#include "Zenith.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics_Fwd.h"
#include <algorithm>
#include <queue>

// RAII destructor - ensures heightfield spans are always freed
Zenith_NavMeshGenerator::GenerationContext::~GenerationContext()
{
	// Use FreeHeightfield to clean up (single implementation)
	Zenith_NavMeshGenerator::FreeHeightfield(*this);
}

Zenith_NavMesh* Zenith_NavMeshGenerator::GenerateFromScene(Zenith_Scene& xScene, const NavMeshGenerationConfig& xConfig)
{
	Zenith_Log(LOG_CATEGORY_AI, "Starting NavMesh generation from scene...");

	// Collect geometry from static colliders
	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> axIndices;

	if (!CollectGeometryFromScene(xScene, axVertices, axIndices))
	{
		Zenith_Log(LOG_CATEGORY_AI, "Failed to collect geometry from scene");
		return nullptr;
	}

	Zenith_Log(LOG_CATEGORY_AI, "Collected %u vertices, %u triangles",
		axVertices.GetSize(), axIndices.GetSize() / 3);

	return GenerateFromGeometry(axVertices, axIndices, xConfig);
}

Zenith_NavMesh* Zenith_NavMeshGenerator::GenerateFromGeometry(
	const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
	const Zenith_Vector<uint32_t>& axIndices,
	const NavMeshGenerationConfig& xConfig)
{
	if (axVertices.GetSize() == 0 || axIndices.GetSize() < 3)
	{
		Zenith_Log(LOG_CATEGORY_AI, "No geometry to generate NavMesh from");
		return nullptr;
	}

	// GenerationContext uses RAII - destructor automatically frees all allocated spans
	// This ensures cleanup happens even if an exception is thrown
	GenerationContext xContext;
	xContext.m_xConfig = xConfig;

	// Compute bounds
	if (!ComputeBounds(axVertices, xContext))
	{
		return nullptr;  // RAII: xContext destructor cleans up
	}

	// Voxelize
	if (!VoxelizeTriangles(axVertices, axIndices, xContext))
	{
		return nullptr;  // RAII: xContext destructor cleans up
	}

	// Filter walkable
	if (!FilterWalkableSpans(xContext))
	{
		return nullptr;  // RAII: xContext destructor cleans up
	}

	// Build compact heightfield
	if (!BuildCompactHeightfield(xContext))
	{
		return nullptr;  // RAII: xContext destructor cleans up
	}

	// Build regions
	if (!BuildRegions(xContext))
	{
		return nullptr;  // RAII: xContext destructor cleans up
	}

	// Trace contours
	if (!TraceContours(xContext))
	{
		return nullptr;  // RAII: xContext destructor cleans up
	}

	// Build polygon mesh
	if (!BuildPolygonMesh(xContext))
	{
		return nullptr;  // RAII: xContext destructor cleans up
	}

	// Build final NavMesh
	Zenith_NavMesh* pxNavMesh = BuildNavMesh(xContext);

	// RAII: xContext destructor cleans up heightfield when function returns

	if (pxNavMesh)
	{
		Zenith_Log(LOG_CATEGORY_AI, "NavMesh generation complete: %u vertices, %u polygons",
			pxNavMesh->GetVertexCount(), pxNavMesh->GetPolygonCount());
	}

	return pxNavMesh;
}

bool Zenith_NavMeshGenerator::CollectGeometryFromScene(Zenith_Scene& xScene,
	Zenith_Vector<Zenith_Maths::Vector3>& axVerticesOut,
	Zenith_Vector<uint32_t>& axIndicesOut)
{
	axVerticesOut.Clear();
	axIndicesOut.Clear();

	// Query all entities with ColliderComponent
	const Zenith_Vector<Zenith_EntityID>& axActiveEntities = xScene.GetActiveEntities();

	Zenith_Log(LOG_CATEGORY_AI, "CollectGeometryFromScene: Checking %u active entities", axActiveEntities.GetSize());
	uint32_t uEntitiesWithColliders = 0;
	uint32_t uEntitiesWithValidBodies = 0;

	for (uint32_t u = 0; u < axActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = axActiveEntities.Get(u);
		Zenith_Entity xEntity = xScene.TryGetEntity(xEntityID);

		if (!xEntity.IsValid())
		{
			continue;
		}

		// Check if entity has a ColliderComponent
		if (!xEntity.HasComponent<Zenith_ColliderComponent>())
		{
			continue;
		}

		Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
		++uEntitiesWithColliders;

		// Only include static bodies (floors, walls, etc.)
		// Dynamic bodies (players, enemies) shouldn't be part of the navmesh
		if (xCollider.GetRigidBodyType() != RIGIDBODY_TYPE_STATIC)
		{
			continue;
		}
		++uEntitiesWithValidBodies;

		if (!xEntity.HasComponent<Zenith_TransformComponent>())
		{
			continue;
		}

		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Matrix4 xWorldMatrix;
		xTransform.BuildModelMatrix(xWorldMatrix);
		Zenith_Maths::Vector3 xScale;
		xTransform.GetScale(xScale);

		// Get shape geometry based on collider type
		// For now, generate approximate geometry for basic shapes
		// A full implementation would query the actual mesh from Jolt

		// Generate a simple box/cube approximation based on scale
		// This is a simplified approach - a full implementation would
		// extract the actual collision mesh from the physics system

		uint32_t uBaseVertex = axVerticesOut.GetSize();
		Zenith_Maths::Vector3 xPos;
		xTransform.GetPosition(xPos);
		Zenith_Maths::Vector3 xHalfExtents = xScale * 0.5f;

		// Create box vertices (8 corners)
		// NOTE: The cube model is CENTERED at origin, so position is the center
		// and scale extends ±half in each direction
		Zenith_Maths::Vector3 axBoxVerts[8] = {
			// Bottom face (4 corners at center.y - halfExtent.y)
			xPos + Zenith_Maths::Vector3(-xHalfExtents.x, -xHalfExtents.y, -xHalfExtents.z),
			xPos + Zenith_Maths::Vector3( xHalfExtents.x, -xHalfExtents.y, -xHalfExtents.z),
			xPos + Zenith_Maths::Vector3( xHalfExtents.x, -xHalfExtents.y,  xHalfExtents.z),
			xPos + Zenith_Maths::Vector3(-xHalfExtents.x, -xHalfExtents.y,  xHalfExtents.z),
			// Top face (4 corners at center.y + halfExtent.y)
			xPos + Zenith_Maths::Vector3(-xHalfExtents.x,  xHalfExtents.y, -xHalfExtents.z),
			xPos + Zenith_Maths::Vector3( xHalfExtents.x,  xHalfExtents.y, -xHalfExtents.z),
			xPos + Zenith_Maths::Vector3( xHalfExtents.x,  xHalfExtents.y,  xHalfExtents.z),
			xPos + Zenith_Maths::Vector3(-xHalfExtents.x,  xHalfExtents.y,  xHalfExtents.z)
		};

		for (int i = 0; i < 8; ++i)
		{
			axVerticesOut.PushBack(axBoxVerts[i]);
		}

		// Only add TOP face - this creates walkable surfaces
		// Vertex indices: 0-3 = bottom face corners, 4-7 = top face corners
		// Top face (Y+) - CCW when viewed from above
		axIndicesOut.PushBack(uBaseVertex + 4);
		axIndicesOut.PushBack(uBaseVertex + 7);
		axIndicesOut.PushBack(uBaseVertex + 6);
		axIndicesOut.PushBack(uBaseVertex + 4);
		axIndicesOut.PushBack(uBaseVertex + 6);
		axIndicesOut.PushBack(uBaseVertex + 5);
	}

	Zenith_Log(LOG_CATEGORY_AI, "CollectGeometryFromScene: %u entities with colliders, %u with valid bodies, generated %u vertices",
		uEntitiesWithColliders, uEntitiesWithValidBodies, axVerticesOut.GetSize());

	// Debug: Log all collected geometry heights
	for (uint32_t u = 0; u < axVerticesOut.GetSize(); u += 8)
	{
		// Log each box's top face height (vertices 4-7 are top face)
		if (u + 7 < axVerticesOut.GetSize())
		{
			float fTopY = axVerticesOut.Get(u + 4).y;
			float fBottomY = axVerticesOut.Get(u).y;
			Zenith_Log(LOG_CATEGORY_AI, "  Box %u: bottom Y=%.2f, top Y=%.2f",
				u / 8, fBottomY, fTopY);
		}
	}

	return axVerticesOut.GetSize() > 0;
}

bool Zenith_NavMeshGenerator::ComputeBounds(
	const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
	GenerationContext& xContext)
{
	if (axVertices.GetSize() == 0)
	{
		return false;
	}

	xContext.m_xBoundsMin = axVertices.Get(0);
	xContext.m_xBoundsMax = axVertices.Get(0);

	for (uint32_t u = 1; u < axVertices.GetSize(); ++u)
	{
		xContext.m_xBoundsMin.x = std::min(xContext.m_xBoundsMin.x, axVertices.Get(u).x);
		xContext.m_xBoundsMin.y = std::min(xContext.m_xBoundsMin.y, axVertices.Get(u).y);
		xContext.m_xBoundsMin.z = std::min(xContext.m_xBoundsMin.z, axVertices.Get(u).z);

		xContext.m_xBoundsMax.x = std::max(xContext.m_xBoundsMax.x, axVertices.Get(u).x);
		xContext.m_xBoundsMax.y = std::max(xContext.m_xBoundsMax.y, axVertices.Get(u).y);
		xContext.m_xBoundsMax.z = std::max(xContext.m_xBoundsMax.z, axVertices.Get(u).z);
	}

	// Add padding for agent radius
	float fPadding = xContext.m_xConfig.m_fAgentRadius;
	xContext.m_xBoundsMin -= Zenith_Maths::Vector3(fPadding);
	xContext.m_xBoundsMax += Zenith_Maths::Vector3(fPadding);

	// Calculate grid dimensions
	float fCellSize = xContext.m_xConfig.m_fCellSize;
	float fCellHeight = xContext.m_xConfig.m_fCellHeight;

	Zenith_Maths::Vector3 xSize = xContext.m_xBoundsMax - xContext.m_xBoundsMin;

	xContext.m_iWidth = static_cast<int32_t>(std::ceil(xSize.x / fCellSize));
	xContext.m_iHeight = static_cast<int32_t>(std::ceil(xSize.z / fCellSize));
	xContext.m_iDepth = static_cast<int32_t>(std::ceil(xSize.y / fCellHeight));

	// Clamp to reasonable limits
	const int32_t iMaxDim = 1024;
	xContext.m_iWidth = std::min(xContext.m_iWidth, iMaxDim);
	xContext.m_iHeight = std::min(xContext.m_iHeight, iMaxDim);
	xContext.m_iDepth = std::min(xContext.m_iDepth, iMaxDim);

	// Allocate heightfield columns
	uint32_t uColumnCount = static_cast<uint32_t>(xContext.m_iWidth * xContext.m_iHeight);
	xContext.m_axColumns.Clear();
	xContext.m_axColumns.Reserve(uColumnCount);
	for (uint32_t u = 0; u < uColumnCount; ++u)
	{
		HeightfieldColumn xCol;
		xCol.m_pxFirstSpan = nullptr;
		xContext.m_axColumns.PushBack(xCol);
	}

	Zenith_Log(LOG_CATEGORY_AI, "NavMesh grid: %d x %d x %d cells, bounds Y [%.2f, %.2f]",
		xContext.m_iWidth, xContext.m_iHeight, xContext.m_iDepth,
		xContext.m_xBoundsMin.y, xContext.m_xBoundsMax.y);

	return true;
}

bool Zenith_NavMeshGenerator::VoxelizeTriangles(
	const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
	const Zenith_Vector<uint32_t>& axIndices,
	GenerationContext& xContext)
{
	uint32_t uTriCount = axIndices.GetSize() / 3;

	for (uint32_t uTri = 0; uTri < uTriCount; ++uTri)
	{
		const Zenith_Maths::Vector3& xV0 = axVertices.Get(axIndices.Get(uTri * 3 + 0));
		const Zenith_Maths::Vector3& xV1 = axVertices.Get(axIndices.Get(uTri * 3 + 1));
		const Zenith_Maths::Vector3& xV2 = axVertices.Get(axIndices.Get(uTri * 3 + 2));

		RasterizeTriangle(xV0, xV1, xV2, xContext);
	}

	return true;
}

void Zenith_NavMeshGenerator::RasterizeTriangle(
	const Zenith_Maths::Vector3& xV0,
	const Zenith_Maths::Vector3& xV1,
	const Zenith_Maths::Vector3& xV2,
	GenerationContext& xContext)
{
	const float fCellSize = xContext.m_xConfig.m_fCellSize;
	const float fCellHeight = xContext.m_xConfig.m_fCellHeight;
	const float fInvCellSize = 1.0f / fCellSize;
	const float fInvCellHeight = 1.0f / fCellHeight;

	// Compute triangle normal for slope check
	Zenith_Maths::Vector3 xEdge1 = xV1 - xV0;
	Zenith_Maths::Vector3 xEdge2 = xV2 - xV0;
	Zenith_Maths::Vector3 xNormal = Zenith_Maths::Normalize(Zenith_Maths::Cross(xEdge1, xEdge2));

	// Only voxelize walkable slopes (we only include top faces now)
	if (!IsWalkableSlope(xNormal, xContext.m_xConfig.m_fMaxSlope))
	{
		return;
	}

	// Compute triangle bounding box in grid coords
	float fMinX = std::min({xV0.x, xV1.x, xV2.x});
	float fMaxX = std::max({xV0.x, xV1.x, xV2.x});
	float fMinZ = std::min({xV0.z, xV1.z, xV2.z});
	float fMaxZ = std::max({xV0.z, xV1.z, xV2.z});
	float fMinY = std::min({xV0.y, xV1.y, xV2.y});
	float fMaxY = std::max({xV0.y, xV1.y, xV2.y});

	int32_t iMinX = static_cast<int32_t>((fMinX - xContext.m_xBoundsMin.x) * fInvCellSize);
	int32_t iMaxX = static_cast<int32_t>((fMaxX - xContext.m_xBoundsMin.x) * fInvCellSize);
	int32_t iMinZ = static_cast<int32_t>((fMinZ - xContext.m_xBoundsMin.z) * fInvCellSize);
	int32_t iMaxZ = static_cast<int32_t>((fMaxZ - xContext.m_xBoundsMin.z) * fInvCellSize);

	// Clamp to grid bounds
	iMinX = std::max(0, std::min(iMinX, xContext.m_iWidth - 1));
	iMaxX = std::max(0, std::min(iMaxX, xContext.m_iWidth - 1));
	iMinZ = std::max(0, std::min(iMinZ, xContext.m_iHeight - 1));
	iMaxZ = std::max(0, std::min(iMaxZ, xContext.m_iHeight - 1));

	// Rasterize spans
	uint16_t uMinY = static_cast<uint16_t>((fMinY - xContext.m_xBoundsMin.y) * fInvCellHeight);
	uint16_t uMaxY = static_cast<uint16_t>((fMaxY - xContext.m_xBoundsMin.y) * fInvCellHeight);

	for (int32_t iZ = iMinZ; iZ <= iMaxZ; ++iZ)
	{
		for (int32_t iX = iMinX; iX <= iMaxX; ++iX)
		{
			int32_t iColIndex = GetColumnIndex(iX, iZ, xContext.m_iWidth);
			AddSpan(xContext.m_axColumns.Get(iColIndex), uMinY, uMaxY, 1);  // All voxelized surfaces are walkable
		}
	}
}

bool Zenith_NavMeshGenerator::FilterWalkableSpans(GenerationContext& xContext)
{
	// Check clearance above each span - if there's an obstacle (span) above
	// within agent height, this span is not walkable

	const float fCellHeight = xContext.m_xConfig.m_fCellHeight;
	const int32_t iAgentHeightCells = static_cast<int32_t>(
		xContext.m_xConfig.m_fAgentHeight / fCellHeight) + 1;

	// Debug: Count and log span heights before filtering
	uint32_t uTotalSpansBefore = 0;
	float fMinSpanY = FLT_MAX;
	float fMaxSpanY = -FLT_MAX;
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			if (pxSpan->m_uAreaType > 0)
			{
				++uTotalSpansBefore;
				float fWorldY = xContext.m_xBoundsMin.y + pxSpan->m_uMaxY * fCellHeight;
				fMinSpanY = std::min(fMinSpanY, fWorldY);
				fMaxSpanY = std::max(fMaxSpanY, fWorldY);
			}
			pxSpan = pxSpan->m_pxNext;
		}
	}
	Zenith_Log(LOG_CATEGORY_AI, "FilterWalkableSpans: %u walkable spans before filtering, Y range [%.2f, %.2f]",
		uTotalSpansBefore, fMinSpanY, fMaxSpanY);

	uint32_t uFilteredCount = 0;

	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			// Only check walkable spans (those with m_uAreaType > 0)
			if (pxSpan->m_uAreaType > 0)
			{
				// Check if there's enough clearance above this span
				VoxelSpan* pxAbove = pxSpan->m_pxNext;
				while (pxAbove)
				{
					// Calculate gap between top of this span and bottom of span above
					int32_t iGap = static_cast<int32_t>(pxAbove->m_uMinY) -
								   static_cast<int32_t>(pxSpan->m_uMaxY);

					if (iGap < iAgentHeightCells)
					{
						// Not enough clearance - mark as unwalkable
						pxSpan->m_uAreaType = 0;
						++uFilteredCount;
						break;
					}

					// If gap is large enough, no need to check spans further above
					if (iGap > iAgentHeightCells * 2)
					{
						break;
					}

					pxAbove = pxAbove->m_pxNext;
				}
			}
			pxSpan = pxSpan->m_pxNext;
		}
	}

	// Debug: Count and log span heights after filtering
	uint32_t uTotalSpansAfter = 0;
	float fMinSpanYAfter = FLT_MAX;
	float fMaxSpanYAfter = -FLT_MAX;
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			if (pxSpan->m_uAreaType > 0)
			{
				++uTotalSpansAfter;
				float fWorldY = xContext.m_xBoundsMin.y + pxSpan->m_uMaxY * fCellHeight;
				fMinSpanYAfter = std::min(fMinSpanYAfter, fWorldY);
				fMaxSpanYAfter = std::max(fMaxSpanYAfter, fWorldY);
			}
			pxSpan = pxSpan->m_pxNext;
		}
	}
	Zenith_Log(LOG_CATEGORY_AI, "FilterWalkableSpans: Filtered %u spans, %u remaining, Y range [%.2f, %.2f]",
		uFilteredCount, uTotalSpansAfter, fMinSpanYAfter, fMaxSpanYAfter);
	return true;
}

bool Zenith_NavMeshGenerator::BuildCompactHeightfield(GenerationContext& xContext)
{
	// Count only WALKABLE spans (m_uAreaType > 0)
	uint32_t uTotalSpans = 0;
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			if (pxSpan->m_uAreaType > 0)
			{
				++uTotalSpans;
			}
			pxSpan = pxSpan->m_pxNext;
		}
	}

	if (uTotalSpans == 0)
	{
		Zenith_Log(LOG_CATEGORY_AI, "No walkable spans found");
		return false;
	}

	// Allocate compact spans for walkable spans only
	xContext.m_axCompactSpans.Clear();
	xContext.m_axCompactSpans.Reserve(uTotalSpans);

	xContext.m_axColumnSpanCounts.Clear();
	xContext.m_axColumnSpanCounts.Reserve(xContext.m_axColumns.GetSize());
	xContext.m_axColumnSpanStarts.Clear();
	xContext.m_axColumnSpanStarts.Reserve(xContext.m_axColumns.GetSize());
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		xContext.m_axColumnSpanCounts.PushBack(0);
		xContext.m_axColumnSpanStarts.PushBack(0);
	}

	// Build compact spans (only from walkable voxel spans)
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		xContext.m_axColumnSpanStarts.Get(u) = xContext.m_axCompactSpans.GetSize();
		uint32_t uColumnSpanCount = 0;

		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			// Only include walkable spans
			if (pxSpan->m_uAreaType > 0)
			{
				CompactSpan xCompact;
				xCompact.m_uY = pxSpan->m_uMaxY;  // Use top of span as height
				xCompact.m_uRegion = 0;
				xCompact.m_uNeighbors[0] = 0;
				xCompact.m_uNeighbors[1] = 0;
				xCompact.m_uNeighbors[2] = 0;
				xCompact.m_uNeighbors[3] = 0;
				xContext.m_axCompactSpans.PushBack(xCompact);
				++uColumnSpanCount;
			}
			pxSpan = pxSpan->m_pxNext;
		}

		xContext.m_axColumnSpanCounts.Get(u) = uColumnSpanCount;
	}

	Zenith_Log(LOG_CATEGORY_AI, "Built compact heightfield: %u spans", uTotalSpans);
	return true;
}

bool Zenith_NavMeshGenerator::BuildRegions(GenerationContext& xContext)
{
	// Simplified region building using flood fill
	// A full implementation would use watershed algorithm

	// Build a mapping from span index to column index for efficient lookup
	Zenith_Vector<uint32_t> axSpanToColumn;
	axSpanToColumn.Reserve(xContext.m_axCompactSpans.GetSize());
	for (uint32_t uCol = 0; uCol < xContext.m_axColumnSpanCounts.GetSize(); ++uCol)
	{
		uint32_t uSpanCount = xContext.m_axColumnSpanCounts.Get(uCol);
		for (uint32_t s = 0; s < uSpanCount; ++s)
		{
			axSpanToColumn.PushBack(uCol);
		}
	}

	uint16_t uNextRegion = 1;

	for (uint32_t u = 0; u < xContext.m_axCompactSpans.GetSize(); ++u)
	{
		if (xContext.m_axCompactSpans.Get(u).m_uRegion != 0)
		{
			continue;  // Already assigned
		}

		// Flood fill from this span
		std::queue<uint32_t> xQueue;
		xQueue.push(u);
		xContext.m_axCompactSpans.Get(u).m_uRegion = uNextRegion;

		while (!xQueue.empty())
		{
			uint32_t uCurrentSpan = xQueue.front();
			xQueue.pop();

			// Get column coordinates from span index
			uint32_t uCurrentCol = axSpanToColumn.Get(uCurrentSpan);
			int32_t iColX = uCurrentCol % xContext.m_iWidth;
			int32_t iColZ = uCurrentCol / xContext.m_iWidth;

			// Check 4 neighbors
			const int32_t aiDx[] = {1, 0, -1, 0};
			const int32_t aiDz[] = {0, 1, 0, -1};

			for (int32_t iDir = 0; iDir < 4; ++iDir)
			{
				int32_t iNx = iColX + aiDx[iDir];
				int32_t iNz = iColZ + aiDz[iDir];

				if (iNx < 0 || iNx >= xContext.m_iWidth ||
					iNz < 0 || iNz >= xContext.m_iHeight)
				{
					continue;
				}

				uint32_t uNeighborCol = GetColumnIndex(iNx, iNz, xContext.m_iWidth);
				uint32_t uNeighborSpanCount = xContext.m_axColumnSpanCounts.Get(uNeighborCol);
				if (uNeighborSpanCount == 0)
				{
					continue;  // No spans in neighbor column
				}

				uint32_t uNeighborSpanStart = xContext.m_axColumnSpanStarts.Get(uNeighborCol);
				CompactSpan& xCurrent = xContext.m_axCompactSpans.Get(uCurrentSpan);

				// Check all spans in the neighbor column
				for (uint32_t s = 0; s < uNeighborSpanCount; ++s)
				{
					uint32_t uNeighborSpanIdx = uNeighborSpanStart + s;
					CompactSpan& xNeighbor = xContext.m_axCompactSpans.Get(uNeighborSpanIdx);

					if (xNeighbor.m_uRegion != 0)
					{
						continue;  // Already assigned
					}

					// Check height difference for connectivity
					int32_t iHeightDiff = std::abs(static_cast<int32_t>(xNeighbor.m_uY) -
						static_cast<int32_t>(xCurrent.m_uY));

					int32_t iMaxStepCells = static_cast<int32_t>(
						xContext.m_xConfig.m_fMaxStepHeight / xContext.m_xConfig.m_fCellHeight);

					if (iHeightDiff <= iMaxStepCells)
					{
						xNeighbor.m_uRegion = uNextRegion;
						xQueue.push(uNeighborSpanIdx);
					}
				}
			}
		}

		++uNextRegion;
	}

	Zenith_Log(LOG_CATEGORY_AI, "Built %u regions", uNextRegion - 1);
	return uNextRegion > 1;
}

bool Zenith_NavMeshGenerator::TraceContours(GenerationContext& xContext)
{
	// Simplified contour tracing - for each region, create a boundary polygon
	// A full implementation would use marching squares or similar

	// Find unique regions
	uint16_t uMaxRegion = 0;
	for (uint32_t u = 0; u < xContext.m_axCompactSpans.GetSize(); ++u)
	{
		uMaxRegion = std::max(uMaxRegion, xContext.m_axCompactSpans.Get(u).m_uRegion);
	}

	if (uMaxRegion == 0)
	{
		return false;
	}

	xContext.m_axContours.Clear();
	xContext.m_axContours.Reserve(uMaxRegion + 1);
	for (uint32_t u = 0; u <= uMaxRegion; ++u)
	{
		Zenith_Vector<ContourVertex> xContour;
		xContext.m_axContours.PushBack(std::move(xContour));
	}

	// Helper lambda to check if a column has a span in a given region
	auto ColumnHasSpanInRegion = [&](int32_t iX, int32_t iZ, uint16_t uRegion) -> bool
	{
		if (iX < 0 || iX >= xContext.m_iWidth || iZ < 0 || iZ >= xContext.m_iHeight)
		{
			return false;
		}

		uint32_t uCol = GetColumnIndex(iX, iZ, xContext.m_iWidth);
		uint32_t uSpanCount = xContext.m_axColumnSpanCounts.Get(uCol);
		if (uSpanCount == 0)
		{
			return false;
		}

		uint32_t uSpanStart = xContext.m_axColumnSpanStarts.Get(uCol);
		for (uint32_t s = 0; s < uSpanCount; ++s)
		{
			if (xContext.m_axCompactSpans.Get(uSpanStart + s).m_uRegion == uRegion)
			{
				return true;
			}
		}
		return false;
	};

	// For each region, find boundary cells
	for (uint16_t uRegion = 1; uRegion <= uMaxRegion; ++uRegion)
	{
		Zenith_Vector<ContourVertex>& axContour = xContext.m_axContours.Get(uRegion);

		// Find all spans in this region and their boundaries
		for (int32_t iZ = 0; iZ < xContext.m_iHeight; ++iZ)
		{
			for (int32_t iX = 0; iX < xContext.m_iWidth; ++iX)
			{
				uint32_t uCol = GetColumnIndex(iX, iZ, xContext.m_iWidth);
				uint32_t uSpanCount = xContext.m_axColumnSpanCounts.Get(uCol);
				if (uSpanCount == 0)
				{
					continue;
				}

				uint32_t uSpanStart = xContext.m_axColumnSpanStarts.Get(uCol);

				// Check each span in this column
				for (uint32_t s = 0; s < uSpanCount; ++s)
				{
					CompactSpan& xSpan = xContext.m_axCompactSpans.Get(uSpanStart + s);
					if (xSpan.m_uRegion != uRegion)
					{
						continue;
					}

					// Check if this is a boundary cell (has neighbor in different region)
					bool bBoundary = false;
					const int32_t aiDx[] = {1, 0, -1, 0};
					const int32_t aiDz[] = {0, 1, 0, -1};

					for (int32_t iDir = 0; iDir < 4 && !bBoundary; ++iDir)
					{
						int32_t iNx = iX + aiDx[iDir];
						int32_t iNz = iZ + aiDz[iDir];

						if (iNx < 0 || iNx >= xContext.m_iWidth ||
							iNz < 0 || iNz >= xContext.m_iHeight)
						{
							bBoundary = true;
						}
						else if (!ColumnHasSpanInRegion(iNx, iNz, uRegion))
						{
							bBoundary = true;
						}
					}

					if (bBoundary)
					{
						ContourVertex xVert;
						xVert.m_iX = iX;
						xVert.m_iY = xSpan.m_uY;
						xVert.m_iZ = iZ;
						xVert.m_uRegion = uRegion;
						axContour.PushBack(xVert);
					}
				}
			}
		}
	}

	return true;
}

bool Zenith_NavMeshGenerator::BuildPolygonMesh(GenerationContext& xContext)
{
	const float fCellSize = xContext.m_xConfig.m_fCellSize;
	const float fCellHeight = xContext.m_xConfig.m_fCellHeight;

	xContext.m_axOutputVertices.Clear();
	xContext.m_axOutputPolygons.Clear();

	// Build a map for vertex deduplication
	std::unordered_map<uint64_t, uint32_t> xVertexMap;

	auto GetOrCreateVertex = [&](int32_t iX, int32_t iZ, float fY) -> uint32_t
	{
		// Create a unique key from grid coordinates
		uint64_t uKey = (static_cast<uint64_t>(iX + 10000) << 32) |
						(static_cast<uint64_t>(iZ + 10000) << 16) |
						(static_cast<uint64_t>(static_cast<int32_t>(fY * 100.0f) + 10000));

		auto it = xVertexMap.find(uKey);
		if (it != xVertexMap.end())
		{
			return it->second;
		}

		// Create new vertex
		Zenith_Maths::Vector3 xWorldPos;
		xWorldPos.x = xContext.m_xBoundsMin.x + iX * fCellSize;
		xWorldPos.y = fY;
		xWorldPos.z = xContext.m_xBoundsMin.z + iZ * fCellSize;

		uint32_t uIndex = xContext.m_axOutputVertices.GetSize();
		xContext.m_axOutputVertices.PushBack(xWorldPos);
		xVertexMap[uKey] = uIndex;
		return uIndex;
	};

	// Debug: count polygons at different height levels
	uint32_t uFloorPolygons = 0;
	uint32_t uMidPolygons = 0;
	uint32_t uHighPolygons = 0;

	// Iterate through all walkable spans and create quads
	// Create polygons for ALL walkable spans, not just the topmost
	for (int32_t iZ = 0; iZ < xContext.m_iHeight; ++iZ)
	{
		for (int32_t iX = 0; iX < xContext.m_iWidth; ++iX)
		{
			uint32_t uCol = GetColumnIndex(iX, iZ, xContext.m_iWidth);
			if (uCol >= xContext.m_axColumnSpanCounts.GetSize())
			{
				continue;
			}

			// Get spans for this column using the proper indexing
			uint32_t uSpanCount = xContext.m_axColumnSpanCounts.Get(uCol);
			if (uSpanCount == 0)
			{
				continue;  // No spans in this column
			}

			uint32_t uSpanStart = xContext.m_axColumnSpanStarts.Get(uCol);

			// Create a polygon for EVERY walkable span in this column
			// This allows multiple walkable levels (e.g., floor + bridge/platform)
			for (uint32_t s = 0; s < uSpanCount; ++s)
			{
				CompactSpan& xSpan = xContext.m_axCompactSpans.Get(uSpanStart + s);
				if (xSpan.m_uRegion == 0)
				{
					continue;  // No region assigned = not walkable
				}

				// Calculate world Y from span height
				float fWorldY = xContext.m_xBoundsMin.y + xSpan.m_uY * fCellHeight;

				// Debug: categorize by height
				if (fWorldY < 0.5f)
					++uFloorPolygons;
				else if (fWorldY < 2.5f)
					++uMidPolygons;
				else
					++uHighPolygons;

				// Create a quad for this cell (4 vertices, CCW winding when viewed from above)
				// Corners: (iX, iZ), (iX+1, iZ), (iX+1, iZ+1), (iX, iZ+1)
				uint32_t uV0 = GetOrCreateVertex(iX, iZ, fWorldY);
				uint32_t uV1 = GetOrCreateVertex(iX + 1, iZ, fWorldY);
				uint32_t uV2 = GetOrCreateVertex(iX + 1, iZ + 1, fWorldY);
				uint32_t uV3 = GetOrCreateVertex(iX, iZ + 1, fWorldY);

				// Create quad polygon (CCW when viewed from above: 0, 3, 2, 1)
				// V0=bottom-left, V1=bottom-right, V2=top-right, V3=top-left
				// CCW order for upward normal: V0 -> V3 -> V2 -> V1
				Zenith_Vector<uint32_t> axQuadIndices;
				axQuadIndices.PushBack(uV0);
				axQuadIndices.PushBack(uV3);
				axQuadIndices.PushBack(uV2);
				axQuadIndices.PushBack(uV1);

				xContext.m_axOutputPolygons.PushBack(std::move(axQuadIndices));
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_AI, "Built polygon mesh: %u vertices, %u polygons (floor: %u, mid: %u, high: %u)",
		xContext.m_axOutputVertices.GetSize(), xContext.m_axOutputPolygons.GetSize(),
		uFloorPolygons, uMidPolygons, uHighPolygons);

	return xContext.m_axOutputPolygons.GetSize() > 0;
}

Zenith_NavMesh* Zenith_NavMeshGenerator::BuildNavMesh(GenerationContext& xContext)
{
	if (xContext.m_axOutputPolygons.GetSize() == 0)
	{
		return nullptr;
	}

	Zenith_NavMesh* pxNavMesh = new Zenith_NavMesh();

	// Add vertices
	for (uint32_t u = 0; u < xContext.m_axOutputVertices.GetSize(); ++u)
	{
		pxNavMesh->AddVertex(xContext.m_axOutputVertices.Get(u));
	}

	// Add polygons
	for (uint32_t u = 0; u < xContext.m_axOutputPolygons.GetSize(); ++u)
	{
		pxNavMesh->AddPolygon(xContext.m_axOutputPolygons.Get(u));
	}

	// Compute spatial data
	pxNavMesh->ComputeSpatialData();

	// Build adjacency (find shared edges)
	for (uint32_t uPoly1 = 0; uPoly1 < pxNavMesh->GetPolygonCount(); ++uPoly1)
	{
		const Zenith_NavMeshPolygon& xPoly1 = pxNavMesh->GetPolygon(uPoly1);

		for (uint32_t uPoly2 = uPoly1 + 1; uPoly2 < pxNavMesh->GetPolygonCount(); ++uPoly2)
		{
			const Zenith_NavMeshPolygon& xPoly2 = pxNavMesh->GetPolygon(uPoly2);

			// Check each edge of poly1 against each edge of poly2
			for (uint32_t uEdge1 = 0; uEdge1 < xPoly1.m_axVertexIndices.GetSize(); ++uEdge1)
			{
				uint32_t uV1a = xPoly1.m_axVertexIndices.Get(uEdge1);
				uint32_t uV1b = xPoly1.m_axVertexIndices.Get((uEdge1 + 1) % xPoly1.m_axVertexIndices.GetSize());

				for (uint32_t uEdge2 = 0; uEdge2 < xPoly2.m_axVertexIndices.GetSize(); ++uEdge2)
				{
					uint32_t uV2a = xPoly2.m_axVertexIndices.Get(uEdge2);
					uint32_t uV2b = xPoly2.m_axVertexIndices.Get((uEdge2 + 1) % xPoly2.m_axVertexIndices.GetSize());

					// Check if edges share vertices (in opposite order for adjacency)
					if ((uV1a == uV2b && uV1b == uV2a) || (uV1a == uV2a && uV1b == uV2b))
					{
						pxNavMesh->SetNeighbor(uPoly1, uEdge1, uPoly2);
						pxNavMesh->SetNeighbor(uPoly2, uEdge2, uPoly1);
					}
				}
			}
		}
	}

	// Build spatial grid for queries
	pxNavMesh->BuildSpatialGrid();

	return pxNavMesh;
}

int32_t Zenith_NavMeshGenerator::GetColumnIndex(int32_t iX, int32_t iZ, int32_t iWidth)
{
	return iZ * iWidth + iX;
}

bool Zenith_NavMeshGenerator::IsWalkableSlope(const Zenith_Maths::Vector3& xNormal, float fMaxSlopeDeg)
{
	// Normal.y gives cos(angle with up vector)
	float fMaxSlopeRad = fMaxSlopeDeg * (3.14159265f / 180.0f);
	float fMinCos = std::cos(fMaxSlopeRad);
	return xNormal.y >= fMinCos;
}

void Zenith_NavMeshGenerator::AddSpan(HeightfieldColumn& xColumn, uint16_t uMinY, uint16_t uMaxY, uint8_t uAreaType)
{
	VoxelSpan* pxNewSpan = new VoxelSpan();
	pxNewSpan->m_uMinY = uMinY;
	pxNewSpan->m_uMaxY = uMaxY;
	pxNewSpan->m_uRegion = 0;
	pxNewSpan->m_uAreaType = uAreaType;
	pxNewSpan->m_pxNext = nullptr;

	if (xColumn.m_pxFirstSpan == nullptr)
	{
		xColumn.m_pxFirstSpan = pxNewSpan;
		return;
	}

	// Insert in sorted order by minY
	VoxelSpan** ppxInsertPos = &xColumn.m_pxFirstSpan;
	while (*ppxInsertPos != nullptr && (*ppxInsertPos)->m_uMinY < uMinY)
	{
		ppxInsertPos = &((*ppxInsertPos)->m_pxNext);
	}

	pxNewSpan->m_pxNext = *ppxInsertPos;
	*ppxInsertPos = pxNewSpan;

	// Merge overlapping spans ONLY if they have the same area type
	// This keeps walkable and non-walkable spans separate for proper clearance checking
	VoxelSpan* pxCurrent = xColumn.m_pxFirstSpan;
	while (pxCurrent && pxCurrent->m_pxNext)
	{
		// Only merge if overlapping AND same area type
		if (pxCurrent->m_uMaxY >= pxCurrent->m_pxNext->m_uMinY &&
			pxCurrent->m_uAreaType == pxCurrent->m_pxNext->m_uAreaType)
		{
			// Merge spans with same area type
			pxCurrent->m_uMaxY = std::max(pxCurrent->m_uMaxY, pxCurrent->m_pxNext->m_uMaxY);
			VoxelSpan* pxToDelete = pxCurrent->m_pxNext;
			pxCurrent->m_pxNext = pxToDelete->m_pxNext;
			delete pxToDelete;
		}
		else if (pxCurrent->m_uMaxY >= pxCurrent->m_pxNext->m_uMinY &&
				 pxCurrent->m_uAreaType != pxCurrent->m_pxNext->m_uAreaType)
		{
			// Overlapping but different area types - keep both but adjust boundaries
			// The non-walkable span takes precedence (blocks walkability)
			if (pxCurrent->m_uAreaType > 0 && pxCurrent->m_pxNext->m_uAreaType == 0)
			{
				// Current is walkable, next is blocker - truncate current
				pxCurrent->m_uMaxY = pxCurrent->m_pxNext->m_uMinY;
				if (pxCurrent->m_uMaxY <= pxCurrent->m_uMinY)
				{
					// Current span is now empty, remove it
					VoxelSpan* pxToDelete = pxCurrent;
					if (pxCurrent == xColumn.m_pxFirstSpan)
					{
						xColumn.m_pxFirstSpan = pxCurrent->m_pxNext;
						pxCurrent = xColumn.m_pxFirstSpan;
					}
					else
					{
						// Find previous span and update its next pointer
						// This is complex, so just mark as non-walkable instead
						pxCurrent->m_uAreaType = 0;
						pxCurrent = pxCurrent->m_pxNext;
					}
					continue;
				}
			}
			pxCurrent = pxCurrent->m_pxNext;
		}
		else
		{
			pxCurrent = pxCurrent->m_pxNext;
		}
	}
}

void Zenith_NavMeshGenerator::FreeHeightfield(GenerationContext& xContext)
{
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			VoxelSpan* pxNext = pxSpan->m_pxNext;
			delete pxSpan;
			pxSpan = pxNext;
		}
		xContext.m_axColumns.Get(u).m_pxFirstSpan = nullptr;
	}
}
