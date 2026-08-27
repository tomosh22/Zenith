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
// written as 1365 / 1677 rather than derived from the 1024 clamp, and the
// too-fine-rejection recommendation is bracketed 0.5 / 0.8 rather than against
// (domain + 2*pad)/1024.
//
// ★ EVERY NUMBER IN THIS FILE MOVED WHEN DAWNMERE SHRANK, AND NONE OF THEM IS A
// DENSITY CONSTANT. The rect used to be a 1024 m square carved out of a fixed
// 4096 m terrain; the terrain carries its own 9x10-chunk grid now, so the domain
// is 576 x 640 m. Cell sizes, the clamp and the agent pad are all UNCHANGED --
// what changed is the domain they are applied to, so these bands were
// re-observed from a run rather than rescaled by hand.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Nav/ZM_NavEval.h"

namespace
{
	// A valid Dawnmere cell size, comfortably under the generator's 1024-cell
	// clamp. The arithmetic is transparent: 576 / 16 = 36 coverage quads in X and
	// 640 / 16 = 40 in Z; the generator adds 0.4 m of agent-radius padding per
	// side, so its voxel grid is ceil(576.8/16) = 37 by ceil(640.8/16) = 41 and
	// one walkable quad per column gives 37*41 = 1517 polygons. See the band
	// derivation in the first unit.
	constexpr float fVALID_CELL_SIZE = 16.0f;

	// Too fine for the domain: 640.8 / 0.3 = 2136 voxel cells, still far past the
	// generator's 1024-cell iMaxDim clamp. The harvester must fail closed on this.
	//
	// ★ IT IS FURTHER PAST THE CLAMP THAN IT LOOKS, AND SHRINKING THE MAP MOVED IT
	// TOWARD THE CLAMP, NOT AWAY. A smaller domain needs fewer cells at any given
	// size, so the smallest SAFE cell also got finer (1.0008 m -> 0.6258 m). 0.3 m
	// is still rejected with a wide margin, which is why it is still the case this
	// unit uses; if the domain ever shrinks below ~307 m on its long axis, 0.3 m
	// would become legal and this unit would need a finer one.
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

	// Pin the quantities the band is derived from, so a future reader sees exactly
	// where 1517 comes from: 36 x 40 soup quads and a 41-cell generator grid on
	// the longer axis. Bracketed with hand literals, not the clamp constant.
	//
	// ★ TWO NUMBERS, NOT ONE. Dawnmere is 576 x 640 m -- it is not square any
	// more -- so a single "quads per side" would report X and say nothing about Z.
	// Asserting both is what would catch a harvester that used one axis's count
	// for the other and covered a square region of a rectangular rect.
	ZENITH_ASSERT_EQ(xResult.m_uQuadsX, 36u,
		"576 m / 16 m must give 36 coverage quads in X");
	ZENITH_ASSERT_EQ(xResult.m_uQuadsZ, 40u,
		"640 m / 16 m must give 40 coverage quads in Z");
	ZENITH_ASSERT_GE(xResult.m_uGeneratorGridDim, 40u,
		"the generator voxel grid must be at least 40 cells on the longer side");
	ZENITH_ASSERT_LE(xResult.m_uGeneratorGridDim, 42u,
		"the generator voxel grid must be about 41 cells on the longer side (padding spill)");

	// The band: 37*41 = 1517 polygons, bracketed 35*39 = 1365 .. 39*43 = 1677 to
	// tolerate a couple of border columns either way. A generator that stopped
	// emitting one quad per walkable span (or a harvester that left gaps / wound
	// the far columns wrongly) would fall out of this band.
	ZENITH_ASSERT_GE(xResult.m_uPolygonCount, 1365u,
		"far too few polygons -- the flat grid did not cover the Dawnmere rect (got %u)",
		xResult.m_uPolygonCount);
	ZENITH_ASSERT_LE(xResult.m_uPolygonCount, 1677u,
		"far too many polygons for a 37x41 voxel grid (got %u)",
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
	// below 1365.
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
		"a 0.3 m cell over a 640 m domain must be rejected before generation");
	ZENITH_ASSERT_FALSE(xTooFine.m_bWalkable,
		"a rejected harvest must not report a walkable navmesh");
	ZENITH_ASSERT_EQ(xTooFine.m_uWalkablePolygonCount, 0u,
		"a rejected harvest must have zero walkable polygons");
	ZENITH_ASSERT_EQ(xTooFine.m_uPolygonCount, 0u,
		"a rejected harvest must have zero polygons");

	// Why it is rejected: the generator voxel grid it WOULD need is 2136 cells
	// (640.8 / 0.3), still more than twice the clamp. Bracketed 2000 / 2300 with
	// hand literals, independent of the clamp constant itself.
	ZENITH_ASSERT_GT(xTooFine.m_uGeneratorGridDim, 2000u,
		"0.3 m must demand far more than 2000 voxel cells (got %u)",
		xTooFine.m_uGeneratorGridDim);
	ZENITH_ASSERT_LT(xTooFine.m_uGeneratorGridDim, 2300u,
		"0.3 m should demand about 2136 voxel cells (got %u)",
		xTooFine.m_uGeneratorGridDim);

	// The recommended floor: the smallest cell size that stays under the clamp
	// for this domain is ~0.6258 m. Bracketed 0.5 / 0.8 with hand literals --
	// NOT spelled against (domain + 2*pad)/1024.
	ZENITH_ASSERT_GT(xTooFine.m_fMinSafeCellSize, 0.5f,
		"the min safe cell size for a 640 m domain must be about 0.63 m (got %.4f)",
		(double)xTooFine.m_fMinSafeCellSize);
	ZENITH_ASSERT_LT(xTooFine.m_fMinSafeCellSize, 0.8f,
		"the min safe cell size must be just above 0.6 m, not larger (got %.4f)",
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
// Unit 4 -- the harvested rect is Dawnmere's OWN authored terrain, 576 x 640 m,
// and not the engine's default 4096 m domain. It used to be a 1024 m sub-rect
// carved out of that fixed grid; the recipe carries its own grid now, so the
// rect follows the terrain instead of being kept in step with it by hand.
// ============================================================================

ZENITH_TEST(ZM_Nav, DawnmereRectIsTheRecipesOwnAuthoredTerrain)
{
	const ZM_NavEvalRect xRect = ZM_GetDawnmereNavRect();

	const float fDomainX = xRect.m_fMaxX - xRect.m_fMinX;
	const float fDomainZ = xRect.m_fMaxZ - xRect.m_fMinZ;

	// 9 x 10 chunks of 64 m = 576 x 640 m, bracketed with hand literals either
	// side. ★ THE TWO AXES ARE ASSERTED SEPARATELY AND WITH DIFFERENT BANDS: a
	// single shared band would pass for a harvester that used X's extent for both
	// and covered a 576 m square of a 640 m deep rect, losing the northern
	// route corridor entirely.
	ZENITH_ASSERT_GT(fDomainX, 560.0f,
		"Dawnmere X extent must be 576 m (got %.1f)", (double)fDomainX);
	ZENITH_ASSERT_LT(fDomainX, 592.0f,
		"Dawnmere X extent must be 576 m (got %.1f)", (double)fDomainX);
	ZENITH_ASSERT_GT(fDomainZ, 624.0f,
		"Dawnmere Z extent must be 640 m (got %.1f)", (double)fDomainZ);
	ZENITH_ASSERT_LT(fDomainZ, 656.0f,
		"Dawnmere Z extent must be 640 m (got %.1f)", (double)fDomainZ);

	// Explicitly NOT the engine's DEFAULT 4096 m domain, and not the 1024 m
	// sub-rect this used to be either: Dawnmere is sized to its own content.
	// 1024 is a hand literal, distinct from any grid constant.
	ZENITH_ASSERT_LT(fDomainX, 1024.0f,
		"the harvest must be Dawnmere's own terrain, not a default-sized one");
	ZENITH_ASSERT_LT(fDomainZ, 1024.0f,
		"the harvest must be Dawnmere's own terrain, not a default-sized one");

	// The terrain starts at the world origin, so the rect must too -- a harvest
	// offset into the middle of the world would cover nothing that exists.
	ZENITH_ASSERT_EQ_FLOAT(xRect.m_fMinX, 0.0f, 0.0001f,
		"Dawnmere's grid starts at world x=0");
	ZENITH_ASSERT_EQ_FLOAT(xRect.m_fMinZ, 0.0f, 0.0001f,
		"Dawnmere's grid starts at world z=0");

	// TownCenter (280, 160) must sit inside the rect -- otherwise the flat grid
	// is not covering the playable area the player spawns in.
	ZENITH_ASSERT_LT(xRect.m_fMinX, 280.0f, "TownCenter X (280) must be inside the rect");
	ZENITH_ASSERT_GT(xRect.m_fMaxX, 280.0f, "TownCenter X (280) must be inside the rect");
	ZENITH_ASSERT_LT(xRect.m_fMinZ, 160.0f, "TownCenter Z (160) must be inside the rect");
	ZENITH_ASSERT_GT(xRect.m_fMaxZ, 160.0f, "TownCenter Z (160) must be inside the rect");

	// The flat ground height is the sampled TownCenter surface (~25.99 m),
	// bracketed 20 / 30 with hand literals.
	ZENITH_ASSERT_GT(xRect.m_fGroundHeight, 20.0f,
		"the flat ground height must be near the sampled Dawnmere surface (got %.3f)",
		(double)xRect.m_fGroundHeight);
	ZENITH_ASSERT_LT(xRect.m_fGroundHeight, 30.0f,
		"the flat ground height must be near the sampled Dawnmere surface (got %.3f)",
		(double)xRect.m_fGroundHeight);

	// MUTATION: reds if ZM_GetDawnmereNavRect is repointed at a default-dimensioned
	// terrain instead of the recipe's own grid, if the two axes are transposed or
	// collapsed onto one, or if the recipe bounds drift such that TownCenter
	// (280, 160) falls outside the harvested rect.
}
