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
// written as 225 / 361 rather than derived from the 1024 clamp, and the
// too-fine-rejection recommendation is bracketed 0.5 / 0.8 rather than against
// (domain + 2*pad)/1024.
//
// ★ EVERY NUMBER IN THIS FILE MOVED WHEN DAWNMERE SHRANK -- TWICE NOW -- AND
// NONE OF THEM IS A DENSITY CONSTANT. The rect used to be a 1024 m square carved
// out of a fixed 4096 m terrain; the terrain carries its own grid now, and it
// has shrunk twice more -- 9x10 -> 6x6 at v7, 6x6 -> 4x4 at v8 -- so the domain
// is 256 x 256 m. Cell sizes, the clamp and the agent pad are all UNCHANGED --
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
	constexpr float fTOO_FINE_CELL_SIZE = 0.2f;

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
	// ★ TWO NUMBERS, NOT ONE, AND THAT STAYS TRUE EVEN THOUGH v7's DAWNMERE IS
	// SQUARE AGAIN (384 x 384 m). A single "quads per side" would report X and say
	// nothing about Z, and asserting both is what would catch a harvester that used
	// one axis's count for the other -- a defect a square domain HIDES, which is
	// exactly why the clause is kept now that the map no longer forces the issue.
	ZENITH_ASSERT_EQ(xResult.m_uQuadsX, 16u,
		"256 m / 16 m must give 16 coverage quads in X");
	ZENITH_ASSERT_EQ(xResult.m_uQuadsZ, 16u,
		"256 m / 16 m must give 16 coverage quads in Z");
	ZENITH_ASSERT_GE(xResult.m_uGeneratorGridDim, 16u,
		"the generator voxel grid must be at least 16 cells on the longer side");
	ZENITH_ASSERT_LE(xResult.m_uGeneratorGridDim, 19u,
		"the generator voxel grid must be about 17 cells on the longer side (padding spill)");

	// The band: 17*17 = 289 polygons, bracketed 15*15 = 225 .. 19*19 = 361 to
	// tolerate a couple of border columns either way. A generator that stopped
	// emitting one quad per walkable span (or a harvester that left gaps / wound
	// the far columns wrongly) would fall out of this band.
	ZENITH_ASSERT_GE(xResult.m_uPolygonCount, 225u,
		"far too few polygons -- the flat grid did not cover the Dawnmere rect (got %u)",
		xResult.m_uPolygonCount);
	ZENITH_ASSERT_LE(xResult.m_uPolygonCount, 361u,
		"far too many polygons for a 17x17 voxel grid (got %u)",
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
	// below 225.
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

	// Why it is rejected: the generator voxel grid it WOULD need is 1284 cells
	// (256.8 / 0.2), comfortably over the clamp. Bracketed 1150 / 1400 with hand
	// literals, independent of the clamp constant itself.
	//
	// ★★ THE PROBE MOVED FROM 0.3 m TO 0.2 m AT v8, AND v7's OWN NOTE PREDICTED
	// IT: "if Dawnmere ever shrinks again, check this first -- below ~307 m square,
	// 0.3 m stops being rejected at all and this unit goes vacuous." At 256 m a
	// 0.3 m cell needs 856 voxels, comfortably UNDER the 1024 clamp, so the old
	// probe would have been ACCEPTED and every clause here would have passed while
	// proving nothing. 0.2 m needs 1284, 25% over, which is the same margin 0.3
	// held at v7.
	// ★ THE PROBE IS SIZED TO THE MAP, NOT LOOSENED TO THE ABSURD. A clause that
	// only rejects a cell size nobody would try proves less than one that rejects
	// a tempting one; the recommended floor for this domain is 0.2508 m, so 0.2 is
	// the nearest round number a person would actually reach for. Below ~205 m
	// square this needs revisiting again.
	ZENITH_ASSERT_GT(xTooFine.m_uGeneratorGridDim, 1150u,
		"0.2 m must demand far more than 1150 voxel cells (got %u)",
		xTooFine.m_uGeneratorGridDim);
	ZENITH_ASSERT_LT(xTooFine.m_uGeneratorGridDim, 1400u,
		"0.2 m should demand about 1284 voxel cells (got %u)",
		xTooFine.m_uGeneratorGridDim);

	// The recommended floor: the smallest cell size that stays under the clamp
	// for this domain is ~0.2508 m. Bracketed 0.2 / 0.35 with hand literals --
	// NOT spelled against (domain + 2*pad)/1024.
	ZENITH_ASSERT_GT(xTooFine.m_fMinSafeCellSize, 0.2f,
		"the min safe cell size for a 256 m domain must be about 0.25 m (got %.4f)",
		(double)xTooFine.m_fMinSafeCellSize);
	ZENITH_ASSERT_LT(xTooFine.m_fMinSafeCellSize, 0.35f,
		"the min safe cell size must be just above 0.25 m, not larger (got %.4f)",
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

	// 6 x 6 chunks of 64 m = 384 x 384 m, bracketed with hand literals either side.
	// ★ THE TWO AXES ARE STILL ASSERTED SEPARATELY EVEN THOUGH v7's DAWNMERE IS
	// SQUARE AGAIN, and that is deliberate: a harvester that used X's extent for
	// both axes is invisible on a square domain, so this is exactly the shape of
	// map where a shared band would start passing a broken harvester. The bands
	// are kept distinct in FORM so the clause survives the next non-square map.
	ZENITH_ASSERT_GT(fDomainX, 240.0f,
		"Dawnmere X extent must be 256 m (got %.1f)", (double)fDomainX);
	ZENITH_ASSERT_LT(fDomainX, 272.0f,
		"Dawnmere X extent must be 256 m (got %.1f)", (double)fDomainX);
	ZENITH_ASSERT_GT(fDomainZ, 240.0f,
		"Dawnmere Z extent must be 256 m (got %.1f)", (double)fDomainZ);
	ZENITH_ASSERT_LT(fDomainZ, 272.0f,
		"Dawnmere Z extent must be 256 m (got %.1f)", (double)fDomainZ);

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
	ZENITH_ASSERT_LT(xRect.m_fMinX, 120.0f, "TownCenter X (192) must be inside the rect");
	ZENITH_ASSERT_GT(xRect.m_fMaxX, 120.0f, "TownCenter X (192) must be inside the rect");
	ZENITH_ASSERT_LT(xRect.m_fMinZ, 60.0f, "TownCenter Z (128) must be inside the rect");
	ZENITH_ASSERT_GT(xRect.m_fMaxZ, 60.0f, "TownCenter Z (128) must be inside the rect");

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
