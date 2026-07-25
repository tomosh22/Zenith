#pragma once

#include "Maths/Zenith_Maths.h"

#include <cstdint>

class Zenith_NavMesh;

// ============================================================================
// Zenith_NavMeshStats -- a read-only summary of a navmesh, computed on demand.
//
// Exists so the editor's NavMesh panel FORMATS numbers rather than deriving
// them: every figure the panel shows comes from this one unit-tested helper, so
// the panel can never drift from what the mesh actually contains. Also useful
// from a test or a bake report.
//
// Stateless leaf utility -- no cache, no globals, nothing owned.
// ============================================================================

struct Zenith_NavMeshStats
{
	u_int m_uVertexCount = 0;
	u_int m_uPolygonCount = 0;

	Zenith_Maths::Vector3 m_xBoundsMin = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xBoundsMax = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xExtents = Zenith_Maths::Vector3(0.0f);

	// Sum of every polygon's cached area, and the per-polygon spread.
	float m_fTotalArea = 0.0f;
	float m_fMinPolygonArea = 0.0f;
	float m_fMaxPolygonArea = 0.0f;
	float m_fAveragePolygonArea = 0.0f;

	// Portal links are counted as UNORDERED PAIRS: an edge whose neighbour index
	// is in range contributes once, from the lower-indexed polygon only, so a
	// mutual link is one portal rather than two.
	u_int m_uPortalLinkCount = 0;

	// Polygons with no in-range neighbour on any edge -- islands A* can enter
	// only by starting on them.
	u_int m_uIsolatedPolygonCount = 0;

	// Polygons carrying FLAG_BLOCKED (dynamic obstacles; skipped by FindPath).
	u_int m_uBlockedPolygonCount = 0;

	// Approximate heap footprint of the mesh's vertex/polygon storage. Indicative
	// (it ignores the spatial grid and container slack), so treat it as an order
	// of magnitude, not an allocator figure.
	uint64_t m_ulApproxMemoryBytes = 0;

	/**
	 * Compute the stats for xNavMesh. TOTAL: an empty mesh yields all-zero
	 * counts, zero areas and zero bounds rather than asserting.
	 */
	static Zenith_NavMeshStats Compute(const Zenith_NavMesh& xNavMesh);
};
