#include "Zenith.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Profiling/Zenith_Profiling.h"
#include <algorithm>
#include <queue>

// RAII destructor - ensures heightfield spans are always freed
Zenith_NavMeshGenerator::GenerationContext::~GenerationContext()
{
	// Use FreeHeightfield to clean up (single implementation)
	Zenith_NavMeshGenerator::FreeHeightfield(*this);
}

Zenith_NavMesh* Zenith_NavMeshGenerator::GenerateFromScene(Zenith_SceneData& xScene, const NavMeshGenerationConfig& xConfig)
{
	Zenith_Log(LOG_CATEGORY_AI, "Starting NavMesh generation from scene...");

	// Collect geometry from static colliders
	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> axIndices;

	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_COLLECT_GEOMETRY);
		if (!CollectGeometryFromScene(xScene, axVertices, axIndices))
		{
			Zenith_Log(LOG_CATEGORY_AI, "Failed to collect geometry from scene");
			return nullptr;
		}
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
	// Top-level scope covers the entire generation pipeline. Sub-stage
	// scopes below let WriteTextReport pinpoint which Recast-style phase
	// dominates the total (collect / voxelize / filter / regions / contours
	// / polygon mesh / final navmesh assembly).
	Zenith_Profiling::Scope xGenerateScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE);

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
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_COMPUTE_BOUNDS);
		if (!ComputeBounds(axVertices, xContext))
		{
			return nullptr;  // RAII: xContext destructor cleans up
		}
	}

	// Voxelize
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_VOXELIZE);
		if (!VoxelizeTriangles(axVertices, axIndices, xContext))
		{
			return nullptr;  // RAII: xContext destructor cleans up
		}
	}

	// Filter walkable
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_FILTER_WALKABLE);
		if (!FilterWalkableSpans(xContext))
		{
			return nullptr;  // RAII: xContext destructor cleans up
		}
	}

	// Build compact heightfield
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_COMPACT_HF);
		if (!BuildCompactHeightfield(xContext))
		{
			return nullptr;  // RAII: xContext destructor cleans up
		}
	}

	// Build regions
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_REGIONS);
		if (!BuildRegions(xContext))
		{
			return nullptr;  // RAII: xContext destructor cleans up
		}
	}

	// Trace contours
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_TRACE_CONTOURS);
		if (!TraceContours(xContext))
		{
			return nullptr;  // RAII: xContext destructor cleans up
		}
	}

	// Build polygon mesh
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_POLY_MESH);
		if (!BuildPolygonMesh(xContext))
		{
			return nullptr;  // RAII: xContext destructor cleans up
		}
	}

	// Build final NavMesh
	Zenith_NavMesh* pxNavMesh = nullptr;
	{
		Zenith_Profiling::Scope xScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_NAVMESH);
		pxNavMesh = BuildNavMesh(xContext);
	}

	// RAII: xContext destructor cleans up heightfield when function returns

	if (pxNavMesh)
	{
		Zenith_Log(LOG_CATEGORY_AI, "NavMesh generation complete: %u vertices, %u polygons",
			pxNavMesh->GetVertexCount(), pxNavMesh->GetPolygonCount());
	}

	return pxNavMesh;
}

bool Zenith_NavMeshGenerator::CollectGeometryFromScene(Zenith_SceneData& xScene,
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

		// Opt-out: callers can mark a static collider as "not navmesh geometry"
		// when the obstacle is meant to be runtime-blockable (doors, gates,
		// lift barriers). Skipping these here lets the generator emit
		// walkable polygons across the doorway gap; the gameplay layer then
		// calls Zenith_NavMesh::SetBlockedAtPoint at runtime to mark those
		// polygons blocked when the door is closed. See
		// Zenith_ColliderComponent::SetIncludeInNavMesh for the contract.
		if (!xCollider.GetIncludeInNavMesh())
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

		// Emit all six faces of the box. The top face's normal points up
		// and the slope check (in RasterizeTriangle) marks it walkable;
		// the four sides and the bottom face have normals that fail the
		// slope check and are marked as OBSTRUCTION spans (area type 0).
		// Obstruction spans block the clearance check above any walkable
		// span beneath them, so a 1m-tall wall sitting on the floor carves
		// a hole in the floor's walkable surface directly under its
		// footprint -- which is what the navmesh needs to route AI around
		// walls instead of through them.
		//
		// Vertex layout (matches the axBoxVerts initialiser above):
		//   0-3 = bottom face corners, 4-7 = top face corners.
		//   0=(-x,-y,-z), 1=(+x,-y,-z), 2=(+x,-y,+z), 3=(-x,-y,+z),
		//   4=(-x,+y,-z), 5=(+x,+y,-z), 6=(+x,+y,+z), 7=(-x,+y,+z).
		// Each face wound CCW when viewed from OUTSIDE so the computed
		// triangle normal points outward (away from the box centre).
		auto EmitTri = [&](uint32_t a, uint32_t b, uint32_t c)
		{
			axIndicesOut.PushBack(uBaseVertex + a);
			axIndicesOut.PushBack(uBaseVertex + b);
			axIndicesOut.PushBack(uBaseVertex + c);
		};
		// Top face (normal +Y, walkable).
		EmitTri(4, 7, 6); EmitTri(4, 6, 5);
		// Bottom face (normal -Y, obstruction).
		EmitTri(0, 1, 2); EmitTri(0, 2, 3);
		// Front face Z- (normal -Z, obstruction).
		EmitTri(0, 4, 5); EmitTri(0, 5, 1);
		// Back face Z+ (normal +Z, obstruction).
		EmitTri(3, 2, 6); EmitTri(3, 6, 7);
		// Left face X- (normal -X, obstruction).
		EmitTri(0, 3, 7); EmitTri(0, 7, 4);
		// Right face X+ (normal +X, obstruction).
		EmitTri(1, 5, 6); EmitTri(1, 6, 2);
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
		ExpandBoundsToInclude(xContext.m_xBoundsMin, xContext.m_xBoundsMax, axVertices.Get(u));
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

// ============================================================================
// PHASE 2: VOXELIZATION
// Rasterise input triangles into the 3D heightfield grid. Each triangle
// produces voxel spans at every (x,z) cell its XZ projection covers, with the
// span's Y range derived from the triangle's Y at that cell. Walkable spans
// are tagged based on the triangle's normal (slope test in IsWalkableSlope).
// Output: xContext.m_axColumns populated with span lists (still unfiltered).
// ============================================================================
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

	// Triangles whose normals point sufficiently up are walkable surfaces
	// (area type 1) -- they emit voxel spans the agent can stand on.
	// Everything else (vertical walls, ceilings, undersides) emits an
	// OBSTRUCTION span (area type 0). Obstruction spans don't appear in
	// the polygon mesh, but they DO trip the clearance check above any
	// walkable span beneath them in the same column -- which is how a
	// wall sitting on a floor carves a hole in the floor's walkable area.
	const bool bWalkable = IsWalkableSlope(xNormal, xContext.m_xConfig.m_fMaxSlope);
	const uint8_t uAreaType = bWalkable ? static_cast<uint8_t>(1)
	                                    : static_cast<uint8_t>(0);

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
			AddSpan(xContext.m_axColumns.Get(iColIndex), uMinY, uMaxY, uAreaType);
		}
	}
}

// Iterate every walkable span across all columns and report count + Y range.
Zenith_NavMeshGenerator::WalkableSpanStats Zenith_NavMeshGenerator::GatherWalkableSpanStats(const GenerationContext& xContext)
{
	const float fCellHeight = xContext.m_xConfig.m_fCellHeight;
	WalkableSpanStats xStats;
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			if (pxSpan->m_uAreaType > 0)
			{
				++xStats.m_uCount;
				const float fWorldY = xContext.m_xBoundsMin.y + pxSpan->m_uMaxY * fCellHeight;
				xStats.m_fMinY = std::min(xStats.m_fMinY, fWorldY);
				xStats.m_fMaxY = std::max(xStats.m_fMaxY, fWorldY);
			}
			pxSpan = pxSpan->m_pxNext;
		}
	}
	return xStats;
}

// True if any span above pxSpan is close enough to block an agent — the
// signal that pxSpan itself must be demoted to unwalkable.
bool Zenith_NavMeshGenerator::HasInsufficientClearance(const VoxelSpan* pxSpan, int32_t iAgentHeightCells)
{
	// INVARIANT: spans in a column's linked list are Y-sorted ascending by m_uMinY.
	// Callers in the voxelisation pipeline (rasterisation + MergeOverlappingSpans)
	// insert in ascending Y order and never re-order afterwards. The
	// `iGap > iAgentHeightCells * 2` early-return below relies on this: once a
	// gap exceeds 2× agent height, subsequent spans can only be further above,
	// so nothing closer than iAgentHeightCells can block clearance. Any future
	// code that builds or mutates a span column must preserve Y-ascending order,
	// or this early-return must be removed — without the invariant, a later span
	// with smaller Y would produce a negative gap and spuriously report a
	// collision.
	const VoxelSpan* pxAbove = pxSpan->m_pxNext;
	while (pxAbove)
	{
		const int32_t iGap = static_cast<int32_t>(pxAbove->m_uMinY)
						   - static_cast<int32_t>(pxSpan->m_uMaxY);
		if (iGap < iAgentHeightCells) return true;
		// Gap already beyond 2× agent height — nothing further above can matter.
		if (iGap > iAgentHeightCells * 2) return false;
		pxAbove = pxAbove->m_pxNext;
	}
	return false;
}

// ============================================================================
// PHASE 3: WALKABILITY FILTER
// Remove walkable tags from spans that don't satisfy the agent's geometric
// constraints: insufficient vertical clearance above the span (HasInsufficientClearance)
// or step heights that exceed m_fMaxStepHeight to neighbouring spans.
// Output: xContext columns where only truly walkable spans keep the walkable flag.
// ============================================================================
bool Zenith_NavMeshGenerator::FilterWalkableSpans(GenerationContext& xContext)
{
	// Check clearance above each span — if an obstacle exists within agent
	// height, this span is not walkable.
	const int32_t iAgentHeightCells = static_cast<int32_t>(
		xContext.m_xConfig.m_fAgentHeight / xContext.m_xConfig.m_fCellHeight) + 1;

	const WalkableSpanStats xBefore = GatherWalkableSpanStats(xContext);
	Zenith_Log(LOG_CATEGORY_AI, "FilterWalkableSpans: %u walkable spans before filtering, Y range [%.2f, %.2f]",
		xBefore.m_uCount, xBefore.m_fMinY, xBefore.m_fMaxY);

	uint32_t uFilteredCount = 0;
	for (uint32_t u = 0; u < xContext.m_axColumns.GetSize(); ++u)
	{
		VoxelSpan* pxSpan = xContext.m_axColumns.Get(u).m_pxFirstSpan;
		while (pxSpan)
		{
			if (pxSpan->m_uAreaType > 0 && HasInsufficientClearance(pxSpan, iAgentHeightCells))
			{
				pxSpan->m_uAreaType = 0;
				++uFilteredCount;
			}
			pxSpan = pxSpan->m_pxNext;
		}
	}

	const WalkableSpanStats xAfter = GatherWalkableSpanStats(xContext);
	Zenith_Log(LOG_CATEGORY_AI, "FilterWalkableSpans: Filtered %u spans, %u remaining, Y range [%.2f, %.2f]",
		uFilteredCount, xAfter.m_uCount, xAfter.m_fMinY, xAfter.m_fMaxY);
	return true;
}

// ============================================================================
// PHASE 4: COMPACT HEIGHTFIELD
// Pack the surviving walkable spans into a flat, cache-friendly array indexed
// by (x, z). The linked-list span representation from voxelization is convenient
// to build but slow to flood-fill; the compact form is what BuildRegions and
// TraceContours operate on.
// Output: xContext.m_axCompactSpans + per-cell (x,z) index ranges.
// ============================================================================
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

// ============================================================================
// PHASE 5: REGION FLOOD-FILL
// Group connected walkable spans into regions by 4-neighbour flood-fill on the
// compact heightfield (FloodFillRegion does the actual recursion). Each region
// gets a unique 16-bit ID stored on every span in m_uRegion. A "region" here is
// a connected component of walkable cells — in a fuller Recast pipeline this
// would also incorporate distance-field watershed, but a simple flood fill is
// sufficient for current needs.
// Output: xContext.m_axCompactSpans with m_uRegion populated per span.
// ============================================================================
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

	int32_t iMaxStepCells = static_cast<int32_t>(
		xContext.m_xConfig.m_fMaxStepHeight / xContext.m_xConfig.m_fCellHeight);

	uint16_t uNextRegion = 1;

	for (uint32_t u = 0; u < xContext.m_axCompactSpans.GetSize(); ++u)
	{
		if (xContext.m_axCompactSpans.Get(u).m_uRegion != 0)
		{
			continue;  // Already assigned
		}

		FloodFillRegion(
			xContext.m_axCompactSpans,
			xContext.m_axColumnSpanCounts,
			xContext.m_axColumnSpanStarts,
			axSpanToColumn,
			xContext.m_iWidth,
			xContext.m_iHeight,
			iMaxStepCells,
			u,
			uNextRegion);

		++uNextRegion;
	}

	Zenith_Log(LOG_CATEGORY_AI, "Built %u regions", uNextRegion - 1);
	return uNextRegion > 1;
}

bool Zenith_NavMeshGenerator::IsRegionBoundary(const GenerationContext& xContext, int32_t iX, int32_t iZ, uint16_t uRegion)
{
	static const int32_t aiDx[] = {1, 0, -1, 0};
	static const int32_t aiDz[] = {0, 1, 0, -1};
	for (int32_t iDir = 0; iDir < 4; ++iDir)
	{
		const int32_t iNx = iX + aiDx[iDir];
		const int32_t iNz = iZ + aiDz[iDir];
		if (iNx < 0 || iNx >= xContext.m_iWidth || iNz < 0 || iNz >= xContext.m_iHeight)
			return true;
		if (!ColumnHasSpanInRegion(xContext.m_axCompactSpans, xContext.m_axColumnSpanCounts, xContext.m_axColumnSpanStarts, iNx, iNz, xContext.m_iWidth, xContext.m_iHeight, uRegion))
			return true;
	}
	return false;
}

// For one region: scan every (x,z) span; push a ContourVertex for every span
// in this region that borders a different region (or the heightfield edge).
// "Boundary" check uses 4-connectivity (N/E/S/W) via IsRegionBoundary.
void Zenith_NavMeshGenerator::CollectRegionContour(GenerationContext& xContext, uint16_t uRegion,
	Zenith_Vector<ContourVertex>& axContour)
{
	for (int32_t iZ = 0; iZ < xContext.m_iHeight; ++iZ)
	{
		for (int32_t iX = 0; iX < xContext.m_iWidth; ++iX)
		{
			const uint32_t uCol = GetColumnIndex(iX, iZ, xContext.m_iWidth);
			const uint32_t uSpanCount = xContext.m_axColumnSpanCounts.Get(uCol);
			if (uSpanCount == 0) continue;

			const uint32_t uSpanStart = xContext.m_axColumnSpanStarts.Get(uCol);
			if (!IsRegionBoundary(xContext, iX, iZ, uRegion)) continue;

			for (uint32_t s = 0; s < uSpanCount; ++s)
			{
				CompactSpan& xSpan = xContext.m_axCompactSpans.Get(uSpanStart + s);
				if (xSpan.m_uRegion != uRegion) continue;

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

// ============================================================================
// PHASE 6: CONTOUR TRACING
// For each region, walk its boundary (cells with at least one neighbour in a
// different region) and collect the boundary vertices into a polygon. This
// extracts a simple polygon outline per region. A full implementation would
// use marching squares for cleaner contours; this one collects boundary cells
// directly.
// Output: per-region contour polygon ready for triangulation in Phase 7.
// ============================================================================
bool Zenith_NavMeshGenerator::TraceContours(GenerationContext& xContext)
{
	uint16_t uMaxRegion = 0;
	for (uint32_t u = 0; u < xContext.m_axCompactSpans.GetSize(); ++u)
	{
		uMaxRegion = std::max(uMaxRegion, xContext.m_axCompactSpans.Get(u).m_uRegion);
	}
	if (uMaxRegion == 0) return false;

	xContext.m_axContours.Clear();
	xContext.m_axContours.Reserve(uMaxRegion + 1);
	for (uint32_t u = 0; u <= uMaxRegion; ++u)
	{
		Zenith_Vector<ContourVertex> xContour;
		xContext.m_axContours.PushBack(std::move(xContour));
	}

	for (uint16_t uRegion = 1; uRegion <= uMaxRegion; ++uRegion)
	{
		CollectRegionContour(xContext, uRegion, xContext.m_axContours.Get(uRegion));
	}

	return true;
}

// ============================================================================
// PHASE 7: POLYGON MESH ASSEMBLY
// Triangulate the per-region contours from Phase 6 into convex polygons in
// world space. This is the final output consumed by Zenith_NavMesh / pathfinding.
//
// **CRITICAL: WINDING ORDER**
// All polygons MUST use counter-clockwise (CCW) winding when viewed from above
// (Y-up). CCW + Y-up produces upward-facing normals (Y > 0). Clockwise winding
// inverts the normal (Y < 0), which:
//   - flips debug visualisation (polygons render below the floor)
//   - breaks IsPointOnNavMesh / FindNearestPolygon spatial queries
//   - breaks pathfinding edge tests
// See Navigation/CLAUDE.md "Polygon Winding Order (Critical)" for the exact
// vertex ordering expected.
//
// All walkable spans are processed (not just topmost) — a column may have
// floor + raised obstacle-top spans, both of which become valid polygons.
// ============================================================================
uint32_t Zenith_NavMeshGenerator::GetOrCreateVertex(GenerationContext& xContext,
	Zenith_HashMap<uint64_t, uint32_t>& xVertexMap,
	int32_t iX, int32_t iZ, float fY, uint16_t uRegion)
{
	// Quantise (iX, iZ, region) into a unique 64-bit key. The +10000 bias
	// makes the cast safe for negative grid coordinates.
	//
	// Why REGION not Y: real-scene floor geometry is often authored from
	// many separate collider entities at slightly varying Y (e.g. DP
	// GameLevel has floor tiles at 1.00, 1.04, 0.98, etc.). Adjacent
	// walkable cells inside one flood-fill region thus end up at
	// micro-different Y values; if we key vertices by Y the same XZ corner
	// shared by two cells in the same region maps to two different
	// vertices, the adjacency hash (which uses vertex indices) doesn't
	// link them, and one logical room fragments into a dozen disconnected
	// polygon islands.
	//
	// Keying by region ID instead solves it: cells in the same region
	// share vertices by construction (flood fill guarantees walkable
	// connectivity within a region), while cells in DIFFERENT regions
	// stay separate (a ground floor and a balcony stack are different
	// regions). The vertex's stored Y is whichever cell created it first;
	// within a region the Y delta across cells is bounded by step-height
	// (0.3m by default) so the slight visual tilt is invisible to
	// pathfinding.
	uint64_t uKey = (static_cast<uint64_t>(iX + 10000) << 32) |
					(static_cast<uint64_t>(iZ + 10000) << 16) |
					(static_cast<uint64_t>(uRegion));

	if (uint32_t* puExisting = xVertexMap.TryGet(uKey))
	{
		return *puExisting;
	}

	const float fCellSize = xContext.m_xConfig.m_fCellSize;
	Zenith_Maths::Vector3 xWorldPos;
	xWorldPos.x = xContext.m_xBoundsMin.x + iX * fCellSize;
	xWorldPos.y = fY;
	xWorldPos.z = xContext.m_xBoundsMin.z + iZ * fCellSize;

	uint32_t uIndex = xContext.m_axOutputVertices.GetSize();
	xContext.m_axOutputVertices.PushBack(xWorldPos);
	xVertexMap.Insert(uKey, uIndex);
	return uIndex;
}

Zenith_NavMeshGenerator::HeightCategoryCounts Zenith_NavMeshGenerator::EmitQuadsFromSpans(
	GenerationContext& xContext, Zenith_HashMap<uint64_t, uint32_t>& xVertexMap)
{
	const float fCellHeight = xContext.m_xConfig.m_fCellHeight;
	HeightCategoryCounts xCounts;

	for (int32_t iZ = 0; iZ < xContext.m_iHeight; ++iZ)
	{
		for (int32_t iX = 0; iX < xContext.m_iWidth; ++iX)
		{
			uint32_t uCol = GetColumnIndex(iX, iZ, xContext.m_iWidth);
			if (uCol >= xContext.m_axColumnSpanCounts.GetSize())
			{
				continue;
			}

			uint32_t uSpanCount = xContext.m_axColumnSpanCounts.Get(uCol);
			if (uSpanCount == 0)
			{
				continue;  // No spans in this column
			}

			uint32_t uSpanStart = xContext.m_axColumnSpanStarts.Get(uCol);

			// Emit a quad for EVERY walkable span in this column so multi-level
			// terrain (floor + bridge/platform) keeps both levels.
			for (uint32_t s = 0; s < uSpanCount; ++s)
			{
				CompactSpan& xSpan = xContext.m_axCompactSpans.Get(uSpanStart + s);
				if (xSpan.m_uRegion == 0)
				{
					continue;  // No region assigned = not walkable
				}

				float fWorldY = xContext.m_xBoundsMin.y + xSpan.m_uY * fCellHeight;

				if (fWorldY < 0.5f)       ++xCounts.m_uFloor;
				else if (fWorldY < 2.5f)  ++xCounts.m_uMid;
				else                      ++xCounts.m_uHigh;

				// Quad corners (V0=bottom-left, V1=bottom-right, V2=top-right,
				// V3=top-left). CCW winding (V0→V3→V2→V1) yields an upward
				// normal — see Navigation/CLAUDE.md "Polygon Winding Order".
				const uint16_t uRegion = static_cast<uint16_t>(xSpan.m_uRegion);
				uint32_t uV0 = GetOrCreateVertex(xContext, xVertexMap, iX,     iZ,     fWorldY, uRegion);
				uint32_t uV1 = GetOrCreateVertex(xContext, xVertexMap, iX + 1, iZ,     fWorldY, uRegion);
				uint32_t uV2 = GetOrCreateVertex(xContext, xVertexMap, iX + 1, iZ + 1, fWorldY, uRegion);
				uint32_t uV3 = GetOrCreateVertex(xContext, xVertexMap, iX,     iZ + 1, fWorldY, uRegion);

				Zenith_Vector<uint32_t> axQuadIndices;
				axQuadIndices.PushBack(uV0);
				axQuadIndices.PushBack(uV3);
				axQuadIndices.PushBack(uV2);
				axQuadIndices.PushBack(uV1);

				xContext.m_axOutputPolygons.PushBack(std::move(axQuadIndices));
			}
		}
	}

	return xCounts;
}

void Zenith_NavMeshGenerator::LogHeightCategoryBreakdown(const HeightCategoryCounts& xCounts,
	uint32_t uVertexCount, uint32_t uPolygonCount)
{
	Zenith_Log(LOG_CATEGORY_AI, "Built polygon mesh: %u vertices, %u polygons (floor: %u, mid: %u, high: %u)",
		uVertexCount, uPolygonCount, xCounts.m_uFloor, xCounts.m_uMid, xCounts.m_uHigh);
}

bool Zenith_NavMeshGenerator::BuildPolygonMesh(GenerationContext& xContext)
{
	xContext.m_axOutputVertices.Clear();
	xContext.m_axOutputPolygons.Clear();

	Zenith_HashMap<uint64_t, uint32_t> xVertexMap;
	const HeightCategoryCounts xCounts = EmitQuadsFromSpans(xContext, xVertexMap);
	LogHeightCategoryBreakdown(xCounts,
		xContext.m_axOutputVertices.GetSize(),
		xContext.m_axOutputPolygons.GetSize());

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

	// Build adjacency by finding shared edges.
	//
	// Previously this was an O(N^2 * E^2) double loop -- for a navmesh with
	// 131,983 polygons (DP GameLevel scale) that's ~1.4e11 operations and
	// the generator took >5 minutes on a single scene load. Replaced with an
	// O(N * E) hash-map pass: each edge is normalised to (min(v1,v2),
	// max(v1,v2)) and looked up; the first polygon to claim an edge inserts
	// itself, the second finds it and the pair are stitched as neighbours.
	// Each edge is shared by at most two polygons in a valid manifold mesh
	// (which Recast-style polygon meshes are by construction), so the hash
	// map yields exact-pair matches in linear time.
	//
	// Verified DP GameLevel: 131,983 polygons, adjacency went from
	// > 5 minutes (timeout) to milliseconds.
	{
		struct EdgeOwner
		{
			uint32_t m_uPoly = UINT32_MAX;
			uint32_t m_uEdge = UINT32_MAX;
		};
		const auto MakeKey = [](uint32_t uA, uint32_t uB) -> uint64_t
		{
			const uint32_t uLo = uA < uB ? uA : uB;
			const uint32_t uHi = uA < uB ? uB : uA;
			return (static_cast<uint64_t>(uHi) << 32) | static_cast<uint64_t>(uLo);
		};

		Zenith_HashMap<uint64_t, EdgeOwner> xEdgeOwners;
		const uint32_t uPolyCount = pxNavMesh->GetPolygonCount();
		for (uint32_t uPoly = 0; uPoly < uPolyCount; ++uPoly)
		{
			const Zenith_NavMeshPolygon& xPoly = pxNavMesh->GetPolygon(uPoly);
			const uint32_t uEdgeCount = xPoly.m_axVertexIndices.GetSize();
			for (uint32_t uEdge = 0; uEdge < uEdgeCount; ++uEdge)
			{
				const uint32_t uVa = xPoly.m_axVertexIndices.Get(uEdge);
				const uint32_t uVb = xPoly.m_axVertexIndices.Get((uEdge + 1) % uEdgeCount);
				const uint64_t uKey = MakeKey(uVa, uVb);

				if (EdgeOwner* pxExisting = xEdgeOwners.TryGet(uKey))
				{
					// Found a polygon that already owns this edge -- stitch
					// both directions. A truly degenerate mesh could in
					// principle map three polygons to the same edge; we'd
					// only stitch the first two found here. That's
					// acceptable: production-mesh generation produces
					// manifold geometry, and stitching extras would require
					// edge->multi-polygon storage.
					pxNavMesh->SetNeighbor(uPoly, uEdge, pxExisting->m_uPoly);
					pxNavMesh->SetNeighbor(pxExisting->m_uPoly, pxExisting->m_uEdge, uPoly);
				}
				else
				{
					EdgeOwner xOwner;
					xOwner.m_uPoly = uPoly;
					xOwner.m_uEdge = uEdge;
					xEdgeOwners.Insert(uKey, xOwner);
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

void Zenith_NavMeshGenerator::MergeOverlappingSpans(HeightfieldColumn& xColumn)
{
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
			continue;
		}

		if (pxCurrent->m_uMaxY >= pxCurrent->m_pxNext->m_uMinY &&
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
		}

		pxCurrent = pxCurrent->m_pxNext;
	}
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

	MergeOverlappingSpans(xColumn);
}

void Zenith_NavMeshGenerator::FloodFillRegion(
	Zenith_Vector<CompactSpan>& axCompactSpans,
	const Zenith_Vector<uint32_t>& axColumnSpanCounts,
	const Zenith_Vector<uint32_t>& axColumnSpanStarts,
	const Zenith_Vector<uint32_t>& axSpanToColumn,
	int32_t iWidth,
	int32_t iHeight,
	int32_t iMaxStepCells,
	uint32_t uStartSpanIdx,
	uint16_t uRegionID)
{
	std::queue<uint32_t> xQueue;
	xQueue.push(uStartSpanIdx);
	axCompactSpans.Get(uStartSpanIdx).m_uRegion = uRegionID;

	while (!xQueue.empty())
	{
		uint32_t uCurrentSpan = xQueue.front();
		xQueue.pop();

		// Get column coordinates from span index
		uint32_t uCurrentCol = axSpanToColumn.Get(uCurrentSpan);
		int32_t iColX = uCurrentCol % iWidth;
		int32_t iColZ = uCurrentCol / iWidth;

		// Check 4 neighbors
		const int32_t aiDx[] = {1, 0, -1, 0};
		const int32_t aiDz[] = {0, 1, 0, -1};

		for (int32_t iDir = 0; iDir < 4; ++iDir)
		{
			int32_t iNx = iColX + aiDx[iDir];
			int32_t iNz = iColZ + aiDz[iDir];

			if (iNx < 0 || iNx >= iWidth ||
				iNz < 0 || iNz >= iHeight)
			{
				continue;
			}

			uint32_t uNeighborCol = GetColumnIndex(iNx, iNz, iWidth);
			uint32_t uNeighborSpanCount = axColumnSpanCounts.Get(uNeighborCol);
			if (uNeighborSpanCount == 0)
			{
				continue;  // No spans in neighbor column
			}

			uint32_t uNeighborSpanStart = axColumnSpanStarts.Get(uNeighborCol);
			CompactSpan& xCurrent = axCompactSpans.Get(uCurrentSpan);

			// Check all spans in the neighbor column
			for (uint32_t s = 0; s < uNeighborSpanCount; ++s)
			{
				uint32_t uNeighborSpanIdx = uNeighborSpanStart + s;
				CompactSpan& xNeighbor = axCompactSpans.Get(uNeighborSpanIdx);

				if (xNeighbor.m_uRegion != 0)
				{
					continue;  // Already assigned
				}

				// Check height difference for connectivity
				int32_t iHeightDiff = std::abs(static_cast<int32_t>(xNeighbor.m_uY) -
					static_cast<int32_t>(xCurrent.m_uY));

				if (iHeightDiff <= iMaxStepCells)
				{
					xNeighbor.m_uRegion = uRegionID;
					xQueue.push(uNeighborSpanIdx);
				}
			}
		}
	}
}

bool Zenith_NavMeshGenerator::ColumnHasSpanInRegion(
	const Zenith_Vector<CompactSpan>& axCompactSpans,
	const Zenith_Vector<uint32_t>& axColumnSpanCounts,
	const Zenith_Vector<uint32_t>& axColumnSpanStarts,
	int32_t iX,
	int32_t iZ,
	int32_t iWidth,
	int32_t iHeight,
	uint16_t uRegion)
{
	if (iX < 0 || iX >= iWidth || iZ < 0 || iZ >= iHeight)
	{
		return false;
	}

	uint32_t uCol = GetColumnIndex(iX, iZ, iWidth);
	uint32_t uSpanCount = axColumnSpanCounts.Get(uCol);
	if (uSpanCount == 0)
	{
		return false;
	}

	uint32_t uSpanStart = axColumnSpanStarts.Get(uCol);
	for (uint32_t s = 0; s < uSpanCount; ++s)
	{
		if (axCompactSpans.Get(uSpanStart + s).m_uRegion == uRegion)
		{
			return true;
		}
	}
	return false;
}

void Zenith_NavMeshGenerator::ExpandBoundsToInclude(
	Zenith_Maths::Vector3& xMin,
	Zenith_Maths::Vector3& xMax,
	const Zenith_Maths::Vector3& xPoint)
{
	xMin.x = std::min(xMin.x, xPoint.x);
	xMin.y = std::min(xMin.y, xPoint.y);
	xMin.z = std::min(xMin.z, xPoint.z);

	xMax.x = std::max(xMax.x, xPoint.x);
	xMax.y = std::max(xMax.y, xPoint.y);
	xMax.z = std::max(xMax.z, xPoint.z);
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

#ifdef ZENITH_TESTING
#include "AI/Navigation/Zenith_NavMeshGenerator.Tests.inl"
#endif
