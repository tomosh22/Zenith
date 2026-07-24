#include "Zenith.h"

#include "Zenithmon/Source/Nav/ZM_NavEval.h"

#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <cmath>

// ============================================================================
// ZM_NavEval implementation. See the header for the SC1 rationale (headless
// evaluation spike; flat-fallback coverage grid; no terrain component, no disk,
// no persistence).
// ============================================================================

namespace
{
	// Ground height for the flat coverage grid. This is the TownCenter feet Y
	// captured in Zenithmon.cpp (xTownCenterFeet(512, 25.98577, 480)) -- the
	// exact sampled Dawnmere terrain surface at the spawn (Zenithmon.cpp:1830,
	// "TownCenter is the exact sampled terrain surface"). The recipe itself
	// carries only noise parameters + a flatten target, not a world ground
	// height, so this sampled value is the representative flat height.
	constexpr float fZM_DAWNMERE_GROUND_HEIGHT = 25.98577f;

	// Height of the degenerate vertical wall quads (bUpwardNormals=false). Any
	// positive value works; the point is a horizontal normal (normal.y == 0),
	// which the generator's slope test rejects.
	constexpr float fZM_NAV_VERTICAL_WALL_HEIGHT = 2.0f;

	// A generated polygon counts as walkable when its normal points sufficiently
	// up. The generator only ever emits upward quads (normal.y ~ +1) for
	// walkable spans, so this threshold is comfortably clear of both +1 (flat
	// floor) and 0 (vertical wall). It is a classification helper for reading
	// the mesh back, not the generator's own slope constant.
	constexpr float fZM_NAV_WALKABLE_NORMAL_Y_MIN = 0.5f;

	// Larger of the two rect side lengths -- the axis that most constrains the
	// generator's grid dimension.
	float LongerDomainSide(const ZM_NavEvalRect& xRect)
	{
		const float fDomainX = xRect.m_fMaxX - xRect.m_fMinX;
		const float fDomainZ = xRect.m_fMaxZ - xRect.m_fMinZ;
		return (fDomainX > fDomainZ) ? fDomainX : fDomainZ;
	}
}

ZM_NavEvalRect ZM_GetDawnmereNavRect()
{
	// Bounds come straight from Dawnmere's authoring recipe (a pure const table,
	// safe to read headless). These are the 1024 m export sub-rect of the
	// engine's fixed 4096 m terrain grid -- NOT the whole grid.
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();

	ZM_NavEvalRect xRect;
	xRect.m_fMinX = xRecipe.m_fWorldMinX;
	xRect.m_fMaxX = xRecipe.m_fWorldMaxX;
	xRect.m_fMinZ = xRecipe.m_fWorldMinZ;
	xRect.m_fMaxZ = xRecipe.m_fWorldMaxZ;
	xRect.m_fGroundHeight = fZM_DAWNMERE_GROUND_HEIGHT;
	return xRect;
}

ZM_NavEvalGrid ZM_BuildCoverageGrid(
	const ZM_NavEvalRect& xRect,
	float fCellSize,
	bool bUpwardNormals,
	Zenith_Vector<Zenith_Maths::Vector3>& axVerticesOut,
	Zenith_Vector<uint32_t>& auIndicesOut)
{
	ZM_NavEvalGrid xGrid;
	xGrid.m_bBuilt = false;
	xGrid.m_uQuadsPerSide = 0u;
	xGrid.m_uGeneratorGridDim = 0u;

	axVerticesOut.Clear();
	auIndicesOut.Clear();

	const float fDomainX = xRect.m_fMaxX - xRect.m_fMinX;
	const float fDomainZ = xRect.m_fMaxZ - xRect.m_fMinZ;

	// TOTAL: diagnose bad input with a non-fatal error and fail closed. This
	// function runs at boot from the unit suite; a Zenith_Assert here would take
	// the whole gate down on a deliberately-bad input.
	if (!std::isfinite(fCellSize) || fCellSize <= 0.0f ||
		!std::isfinite(fDomainX) || !std::isfinite(fDomainZ) ||
		fDomainX <= 0.0f || fDomainZ <= 0.0f)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"ZM_BuildCoverageGrid: rejecting bad cellSize/domain (cell=%f, domainX=%f, domainZ=%f)",
			(double)fCellSize, (double)fDomainX, (double)fDomainZ);
		return xGrid;
	}

	// Predict the generator's voxel grid dimension for the longer side and fail
	// closed if it would exceed the hard clamp -- otherwise the far half of the
	// domain collapses onto the border columns and the result is meaningless.
	const float fPaddedDomain = LongerDomainSide(xRect) + 2.0f * fZM_NAV_AGENT_RADIUS_PAD;
	const u_int uGeneratorGridDim = (u_int)std::ceil(fPaddedDomain / fCellSize);
	xGrid.m_uGeneratorGridDim = uGeneratorGridDim;

	if (uGeneratorGridDim > uZM_NAV_GENERATOR_MAX_GRID_DIM)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"ZM_BuildCoverageGrid: cellSize %f too fine -- generator grid %u exceeds clamp %u; failing closed",
			(double)fCellSize, uGeneratorGridDim, uZM_NAV_GENERATOR_MAX_GRID_DIM);
		return xGrid;
	}

	// Soup resolution derives from rect size / cell size (SC design). Clamp to
	// at least one quad per axis so a rect smaller than one cell still emits a
	// single quad rather than nothing.
	u_int uQuadsX = (u_int)(fDomainX / fCellSize);
	u_int uQuadsZ = (u_int)(fDomainZ / fCellSize);
	if (uQuadsX < 1u) { uQuadsX = 1u; }
	if (uQuadsZ < 1u) { uQuadsZ = 1u; }

	const float fGround = xRect.m_fGroundHeight;

	for (u_int uZ = 0u; uZ < uQuadsZ; ++uZ)
	{
		const float fZ0 = xRect.m_fMinZ + (float)uZ * fCellSize;
		const float fZ1 = xRect.m_fMinZ + (float)(uZ + 1u) * fCellSize;

		for (u_int uX = 0u; uX < uQuadsX; ++uX)
		{
			const float fX0 = xRect.m_fMinX + (float)uX * fCellSize;
			const float fX1 = xRect.m_fMinX + (float)(uX + 1u) * fCellSize;

			const uint32_t uBase = axVerticesOut.GetSize();

			if (bUpwardNormals)
			{
				// Flat quad in the XZ plane at ground height. Corners:
				//   V0 = (x0, g, z0)  V1 = (x1, g, z0)
				//   V2 = (x1, g, z1)  V3 = (x0, g, z1)
				// Triangles (V0,V3,V2) and (V0,V2,V1) each have
				// cross(edge1,edge2) = (0, +d^2, 0) -> normal.y > 0 (walkable).
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX0, fGround, fZ0)); // 0
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX1, fGround, fZ0)); // 1
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX1, fGround, fZ1)); // 2
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX0, fGround, fZ1)); // 3

				auIndicesOut.PushBack(uBase + 0u);
				auIndicesOut.PushBack(uBase + 3u);
				auIndicesOut.PushBack(uBase + 2u);

				auIndicesOut.PushBack(uBase + 0u);
				auIndicesOut.PushBack(uBase + 2u);
				auIndicesOut.PushBack(uBase + 1u);
			}
			else
			{
				// Vertical wall quad in the XY plane at z = z0. Corners:
				//   V0 = (x0, g,       z0)  V1 = (x1, g,       z0)
				//   V2 = (x1, g+h,     z0)  V3 = (x0, g+h,     z0)
				// Triangles have cross(edge1,edge2) = (0, 0, +d*h) -> normal.y
				// == 0, so the generator's slope test rejects every one and no
				// walkable span survives.
				const float fTop = fGround + fZM_NAV_VERTICAL_WALL_HEIGHT;
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX0, fGround, fZ0)); // 0
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX1, fGround, fZ0)); // 1
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX1, fTop,    fZ0)); // 2
				axVerticesOut.PushBack(Zenith_Maths::Vector3(fX0, fTop,    fZ0)); // 3

				auIndicesOut.PushBack(uBase + 0u);
				auIndicesOut.PushBack(uBase + 1u);
				auIndicesOut.PushBack(uBase + 2u);

				auIndicesOut.PushBack(uBase + 0u);
				auIndicesOut.PushBack(uBase + 2u);
				auIndicesOut.PushBack(uBase + 3u);
			}
		}
	}

	xGrid.m_bBuilt = true;
	xGrid.m_uQuadsPerSide = uQuadsX;   // Dawnmere is square, so uQuadsX == uQuadsZ
	return xGrid;
}

ZM_NavEvalResult ZM_EvaluateDawnmereNavGeneration(float fCellSize, bool bUpwardNormals)
{
	ZM_NavEvalResult xResult;
	xResult.m_bAttempted = false;
	xResult.m_bSuccess = false;
	xResult.m_bWalkable = false;
	xResult.m_uPolygonCount = 0u;
	xResult.m_uVertexCount = 0u;
	xResult.m_uWalkablePolygonCount = 0u;
	xResult.m_uQuadsPerSide = 0u;
	xResult.m_uGeneratorGridDim = 0u;
	xResult.m_fMinSafeCellSize = 0.0f;

	const ZM_NavEvalRect xRect = ZM_GetDawnmereNavRect();

	// The smallest cell size that keeps the generator grid under its clamp, for
	// the longer domain side. Reported regardless of whether the build proceeds.
	xResult.m_fMinSafeCellSize =
		(LongerDomainSide(xRect) + 2.0f * fZM_NAV_AGENT_RADIUS_PAD) /
		(float)uZM_NAV_GENERATOR_MAX_GRID_DIM;

	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> auIndices;
	const ZM_NavEvalGrid xGrid =
		ZM_BuildCoverageGrid(xRect, fCellSize, bUpwardNormals, axVertices, auIndices);

	xResult.m_uQuadsPerSide = xGrid.m_uQuadsPerSide;
	xResult.m_uGeneratorGridDim = xGrid.m_uGeneratorGridDim;

	if (!xGrid.m_bBuilt)
	{
		// Harvester failed closed -- nothing to evaluate.
		return xResult;
	}
	xResult.m_bAttempted = true;

	// Leave the agent radius at its default (fZM_NAV_AGENT_RADIUS_PAD), only
	// override the cell size so the generator's grid matches our prediction.
	NavMeshGenerationConfig xConfig;
	xConfig.m_fCellSize = fCellSize;

	Zenith_NavMesh* pxNavMesh =
		Zenith_NavMeshGenerator::GenerateFromGeometry(axVertices, auIndices, xConfig);

	if (pxNavMesh == nullptr)
	{
		// The generator returns nullptr when no walkable span survives (e.g. the
		// all-vertical case) -- a valid, defined outcome, not a failure to run.
		return xResult;
	}

	xResult.m_bSuccess = true;
	xResult.m_uPolygonCount = pxNavMesh->GetPolygonCount();
	xResult.m_uVertexCount = pxNavMesh->GetVertexCount();

	u_int uWalkable = 0u;
	for (u_int u = 0u; u < xResult.m_uPolygonCount; ++u)
	{
		const Zenith_NavMeshPolygon& xPoly = pxNavMesh->GetPolygon(u);
		if (xPoly.m_xNormal.y > fZM_NAV_WALKABLE_NORMAL_Y_MIN)
		{
			++uWalkable;
		}
	}
	xResult.m_uWalkablePolygonCount = uWalkable;
	xResult.m_bWalkable = (uWalkable > 0u);

	delete pxNavMesh;
	return xResult;
}
