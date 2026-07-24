#include "Zenith.h"

// ============================================================================
// ZM_Tests_NavEval -- S7 item 3 SC1 boot units for the PURE, HEADLESS navmesh
// terrain-source evaluation spike (Games/Zenithmon/Source/Nav/ZM_NavEval).
//
// Everything here is headless and pure: it never constructs a live
// Zenith_TerrainComponent (asserts with no GPU -- engine gap
// Q-2026-07-21-001) and never reads/writes a disk asset (baked terrain is
// gitignored -> absent on CI). The spike harvests a synthetic FLAT coverage
// grid at Dawnmere's sampled ground height over the recipe's 1024 m export
// sub-rect and feeds the raw triangle soup to
// Zenith_NavMeshGenerator::GenerateFromGeometry. No persistence this SC (the
// .znavmesh write + routing are DEFERRED, Q-2026-07-24-002 Q-A).
//
// Each unit asserts on the RETURNED MESH CONTENTS (polygon / walkable-polygon
// counts read back from the generated navmesh), never merely "non-null", and
// names the one production change that would turn it red.
//
// TOTALITY: ZM_BuildCoverageGrid / ZM_EvaluateDawnmereNavGeneration are TOTAL
// -- they diagnose bad input with a non-fatal Zenith_Error and return a defined
// fail-closed verdict. These units deliberately feed a too-fine cell size into
// them; that must NOT fire a Zenith_Assert (which is fatal in every config and
// runs at boot before the scene loads, so one assert kills the whole gate).
//
// Numeric guards below are bracketed from BOTH sides with hand-written literals
// (never spelled against the production constant they pin): the polygon band is
// written as 3969 / 4489 rather than derived from the 1024 clamp, and the
// too-fine-rejection recommendation is bracketed 0.99 / 1.5 rather than against
// (domain + 2*pad)/1024.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Nav/ZM_NavEval.h"

namespace
{
	// A valid Dawnmere cell size: >= ~1.0 m, comfortably under the generator's
	// 1024-cell clamp. Chosen so the arithmetic is transparent -- 1024 / 16 = 64
	// coverage quads per side; the generator adds agent-radius padding so its
	// voxel grid is 65 cells per side; one walkable quad per column => ~65*65 =
	// 4225 polygons. See the band derivation in the first unit.
	constexpr float fVALID_CELL_SIZE = 16.0f;

	// Too fine for the 1024 m domain: 1024.8 / 0.3 ~= 3416 voxel cells, far past
	// the generator's iMaxDim clamp. The harvester must fail closed on this.
	constexpr float fTOO_FINE_CELL_SIZE = 0.3f;

	// A second valid cell size used as the "not rejecting everything" control.
	constexpr float fCONTROL_CELL_SIZE = 8.0f;
}

// ============================================================================
// Unit 1 -- a Dawnmere-shaped grid at a valid cell size yields a NON-EMPTY
// walkable navmesh, with the polygon count inside an expected band.
// ============================================================================

ZENITH_TEST(ZM_Nav, DawnmereFlatGridYieldsWalkableNavmeshInBand)
{
	const ZM_NavEvalResult xResult =
		ZM_EvaluateDawnmereNavGeneration(fVALID_CELL_SIZE, /*bUpwardNormals*/ true);

	ZENITH_ASSERT_TRUE(xResult.m_bAttempted,
		"a valid cell size (%.1f m) must not be rejected by the harvester",
		(double)fVALID_CELL_SIZE);
	ZENITH_ASSERT_TRUE(xResult.m_bSuccess,
		"the generator must return a non-null navmesh for a Dawnmere-scale ground surface");
	ZENITH_ASSERT_TRUE(xResult.m_bWalkable,
		"an upward-facing flat surface must be walkable (got %u walkable polygons)",
		xResult.m_uWalkablePolygonCount);

	// Pin the two quantities the band is derived from, so a future reader sees
	// exactly where 4225 comes from: 64 soup quads per side and a 65-cell
	// generator grid. Bracketed with hand literals, not the clamp constant.
	ZENITH_ASSERT_EQ(xResult.m_uQuadsPerSide, 64u,
		"1024 m / 16 m must give 64 coverage quads per side");
	ZENITH_ASSERT_GE(xResult.m_uGeneratorGridDim, 64u,
		"the generator voxel grid must be at least 64 cells per side");
	ZENITH_ASSERT_LE(xResult.m_uGeneratorGridDim, 66u,
		"the generator voxel grid must be about 65 cells per side (padding spill)");

	// The band: ~65*65 = 4225 polygons, bracketed 63*63 = 3969 .. 67*67 = 4489
	// to tolerate a couple of border columns either way. A generator that
	// stopped emitting one quad per walkable span (or a harvester that left gaps
	// / wound the far columns wrongly) would fall out of this band.
	ZENITH_ASSERT_GE(xResult.m_uPolygonCount, 3969u,
		"far too few polygons -- the flat grid did not cover the Dawnmere rect (got %u)",
		xResult.m_uPolygonCount);
	ZENITH_ASSERT_LE(xResult.m_uPolygonCount, 4489u,
		"far too many polygons for a 65x65 voxel grid (got %u)",
		xResult.m_uPolygonCount);

	// Every polygon in a generated navmesh is walkable by construction (only
	// walkable spans become quads); this pins that the readback classifier and
	// the mesh agree -- all normals point up.
	ZENITH_ASSERT_EQ(xResult.m_uWalkablePolygonCount, xResult.m_uPolygonCount,
		"every emitted polygon must be upward-facing (walkable %u of %u total)",
		xResult.m_uWalkablePolygonCount, xResult.m_uPolygonCount);

	// MUTATION: reds if Zenith_NavMeshGenerator::BuildPolygonMesh stops emitting
	// one quad per walkable span, or if ZM_BuildCoverageGrid's upward winding is
	// broken so the far columns get no walkable span -- the polygon count drops
	// below 3969.
}

// ============================================================================
// Unit 2 -- a too-fine cell size is provably wrong: the harvester fails closed
// (rather than letting the far domain collapse onto the clamp border).
// ============================================================================

ZENITH_TEST(ZM_Nav, TooFineCellSizeIsRejectedFailClosed)
{
	const ZM_NavEvalResult xTooFine =
		ZM_EvaluateDawnmereNavGeneration(fTOO_FINE_CELL_SIZE, /*bUpwardNormals*/ true);

	// Fail-closed: the harvester refused to build, so nothing was generated.
	ZENITH_ASSERT_FALSE(xTooFine.m_bAttempted,
		"a 0.3 m cell over a 1024 m domain must be rejected before generation");
	ZENITH_ASSERT_FALSE(xTooFine.m_bWalkable,
		"a rejected harvest must not report a walkable navmesh");
	ZENITH_ASSERT_EQ(xTooFine.m_uWalkablePolygonCount, 0u,
		"a rejected harvest must have zero walkable polygons");
	ZENITH_ASSERT_EQ(xTooFine.m_uPolygonCount, 0u,
		"a rejected harvest must have zero polygons");

	// Why it is rejected: the generator voxel grid it WOULD need is ~3416 cells
	// (1024.8 / 0.3), far past the clamp. Bracketed 3000 / 3500 with hand
	// literals, independent of the clamp constant itself.
	ZENITH_ASSERT_GT(xTooFine.m_uGeneratorGridDim, 3000u,
		"0.3 m must demand far more than 3000 voxel cells (got %u)",
		xTooFine.m_uGeneratorGridDim);
	ZENITH_ASSERT_LT(xTooFine.m_uGeneratorGridDim, 3500u,
		"0.3 m should demand about 3416 voxel cells (got %u)",
		xTooFine.m_uGeneratorGridDim);

	// The recommended floor: the smallest cell size that stays under the clamp
	// for this domain is ~1.0008 m. Bracketed 0.99 / 1.5 with hand literals --
	// NOT spelled against (domain + 2*pad)/1024.
	ZENITH_ASSERT_GT(xTooFine.m_fMinSafeCellSize, 0.99f,
		"the min safe cell size for a 1024 m domain must be about 1 m (got %.4f)",
		(double)xTooFine.m_fMinSafeCellSize);
	ZENITH_ASSERT_LT(xTooFine.m_fMinSafeCellSize, 1.5f,
		"the min safe cell size must be just above 1 m, not larger (got %.4f)",
		(double)xTooFine.m_fMinSafeCellSize);

	// Control: a valid cell size at the SAME domain IS attempted and walkable,
	// so the assertions above cannot be satisfied by a harvester that rejects
	// everything.
	const ZM_NavEvalResult xControl =
		ZM_EvaluateDawnmereNavGeneration(fCONTROL_CELL_SIZE, /*bUpwardNormals*/ true);
	ZENITH_ASSERT_TRUE(xControl.m_bAttempted,
		"an 8 m cell over the same domain must be attempted");
	ZENITH_ASSERT_TRUE(xControl.m_bWalkable,
		"an 8 m cell must produce a walkable navmesh");
	ZENITH_ASSERT_GT(xControl.m_uPolygonCount, 0u,
		"the control must produce polygons");

	// MUTATION: reds if the fail-closed guard in ZM_BuildCoverageGrid
	// (uGeneratorGridDim > uZM_NAV_GENERATOR_MAX_GRID_DIM => don't build) is
	// removed -- the 0.3 m case would then be attempted (m_bAttempted becomes
	// true), tripping the ASSERT_FALSE above.
}

// ============================================================================
// Unit 3 -- a degenerate all-VERTICAL grid yields ZERO walkable polygons. This
// proves walkability is actually evaluated (via triangle normals), not assumed.
// ============================================================================

ZENITH_TEST(ZM_Nav, AllVerticalGridHasZeroWalkablePolygons)
{
	const ZM_NavEvalResult xVertical =
		ZM_EvaluateDawnmereNavGeneration(fVALID_CELL_SIZE, /*bUpwardNormals*/ false);

	// The cell size is valid, so the grid WAS built -- this is not a fail-closed
	// case. The verticality alone must strip all walkability.
	ZENITH_ASSERT_TRUE(xVertical.m_bAttempted,
		"a valid cell size must still build the grid, even for vertical quads");
	ZENITH_ASSERT_FALSE(xVertical.m_bWalkable,
		"an all-vertical surface must not be walkable");
	ZENITH_ASSERT_EQ(xVertical.m_uWalkablePolygonCount, 0u,
		"an all-vertical surface must have zero walkable polygons (got %u)",
		xVertical.m_uWalkablePolygonCount);
	ZENITH_ASSERT_EQ(xVertical.m_uPolygonCount, 0u,
		"the generator emits no polygons when no walkable span survives (got %u)",
		xVertical.m_uPolygonCount);

	// Control: the SAME grid dimensions, wound upward instead, ARE walkable --
	// so it is specifically the verticality that kills walkability, not a broken
	// grid or an all-rejecting generator.
	const ZM_NavEvalResult xUpward =
		ZM_EvaluateDawnmereNavGeneration(fVALID_CELL_SIZE, /*bUpwardNormals*/ true);
	ZENITH_ASSERT_TRUE(xUpward.m_bWalkable,
		"the same grid wound upward must be walkable");
	ZENITH_ASSERT_GT(xUpward.m_uWalkablePolygonCount, 0u,
		"the upward control must have walkable polygons");

	// MUTATION: reds if ZM_BuildCoverageGrid ignores bUpwardNormals and always
	// emits upward quads (the vertical case would then be walkable), or if the
	// generator's slope test were disabled so vertical faces counted as floor.
}

// ============================================================================
// Unit 4 -- the harvested rect is Dawnmere's 1024 m export sub-rect, NOT the
// engine's full 4096 m terrain grid. Pins the "bound to the sub-rect" contract.
// ============================================================================

ZENITH_TEST(ZM_Nav, DawnmereRectIsThe1024ExportSubRect)
{
	const ZM_NavEvalRect xRect = ZM_GetDawnmereNavRect();

	const float fDomainX = xRect.m_fMaxX - xRect.m_fMinX;
	const float fDomainZ = xRect.m_fMaxZ - xRect.m_fMinZ;

	// About 1024 m per side -- bracketed 1000 / 1100 with hand literals.
	ZENITH_ASSERT_GT(fDomainX, 1000.0f,
		"Dawnmere X extent must be about 1024 m (got %.1f)", (double)fDomainX);
	ZENITH_ASSERT_LT(fDomainX, 1100.0f,
		"Dawnmere X extent must be about 1024 m (got %.1f)", (double)fDomainX);
	ZENITH_ASSERT_GT(fDomainZ, 1000.0f,
		"Dawnmere Z extent must be about 1024 m (got %.1f)", (double)fDomainZ);
	ZENITH_ASSERT_LT(fDomainZ, 1100.0f,
		"Dawnmere Z extent must be about 1024 m (got %.1f)", (double)fDomainZ);

	// Explicitly NOT the full 4096 m terrain grid: the sub-rect must be well
	// under half of it. 2048 is a hand literal, distinct from any grid constant.
	ZENITH_ASSERT_LT(fDomainX, 2048.0f,
		"the harvest must be the 1024 m sub-rect, not the 4096 m terrain grid");
	ZENITH_ASSERT_LT(fDomainZ, 2048.0f,
		"the harvest must be the 1024 m sub-rect, not the 4096 m terrain grid");

	// TownCenter (512, 480) must sit inside the rect -- otherwise the flat grid
	// is not covering the playable area the player spawns in.
	ZENITH_ASSERT_LT(xRect.m_fMinX, 512.0f, "TownCenter X (512) must be inside the rect");
	ZENITH_ASSERT_GT(xRect.m_fMaxX, 512.0f, "TownCenter X (512) must be inside the rect");
	ZENITH_ASSERT_LT(xRect.m_fMinZ, 480.0f, "TownCenter Z (480) must be inside the rect");
	ZENITH_ASSERT_GT(xRect.m_fMaxZ, 480.0f, "TownCenter Z (480) must be inside the rect");

	// The flat ground height is the sampled TownCenter surface (~25.99 m),
	// bracketed 20 / 30 with hand literals.
	ZENITH_ASSERT_GT(xRect.m_fGroundHeight, 20.0f,
		"the flat ground height must be near the sampled Dawnmere surface (got %.3f)",
		(double)xRect.m_fGroundHeight);
	ZENITH_ASSERT_LT(xRect.m_fGroundHeight, 30.0f,
		"the flat ground height must be near the sampled Dawnmere surface (got %.3f)",
		(double)xRect.m_fGroundHeight);

	// MUTATION: reds if ZM_GetDawnmereNavRect is repointed at the full 4096 m
	// terrain grid instead of the recipe's 1024 m export sub-rect, or the recipe
	// bounds drift such that TownCenter (512, 480) falls outside the harvested
	// rect.
}
