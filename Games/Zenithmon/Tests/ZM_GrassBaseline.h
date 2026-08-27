#pragma once

#include "Flux/Vegetation/Flux_GrassTypes.h"

// ============================================================================
// ZM_GrassBaseline -- the ONE place the battle round-trip suites state what
// "the overworld's grass came back" means, and why it is not an equality.
//
// PURE and header-only: four automated tests (BattleTransition, BattleHUD,
// BattleDirector, BattleMenu) each drive a walk-into-grass encounter, park the
// player, run a battle, and check the resumed overworld. All four used to spell
// `after != atPark` inline, which is four copies of a claim that turned out to
// be slightly wrong in the same way.
//
// ★ WHY THIS IS NOT AN EQUALITY. Flux_Grass regenerates around the CAMERA every
// frame and reports lattice cells DISPATCHED, so the count is a function of the
// camera pose, not of the world. The baseline is latched on the frame the
// encounter latches -- which is the frame the walk key is released, with the
// body still moving and the follow camera's arm still trailing it. The overworld
// then pauses with the camera exactly there. After the battle it resumes, the
// body comes to rest, and the arm CONVERGES before the second sample is taken.
// The camera has legitimately moved between the two readings, so a few tiles at
// the frustum edge enter or leave, and demanding bit-equality asserts that the
// camera does not settle.
//
// That comparison was already corrected ONCE, from entry-vs-resume to
// park-vs-resume, on exactly this reasoning: "grass is GPU-regenerated around
// the camera every frame, so its blade count is a function of where the camera
// is". Park-vs-resume is the same argument one step finer, and it only became
// visible when Dawnmere shrank from a 1024 m square to a 9x10 chunk grid: the
// coverage map is a fixed 1024 texels either way, so at 640 m it resolves the
// authored grass 6.4x finer and far more edge tiles carry blades to gain or
// lose. Measured on the shrunken map, the resume converges by exactly 2 tiles.
//
// What the clause is FOR is unchanged and is now asserted more tightly than
// before: the field must be REBUILT -- not empty, not degenerate -- and within a
// small, named number of tiles of what it was. An empty field used to pass this
// clause whenever the baseline was also empty; it cannot now.
namespace ZM_GrassBaseline
{
	// One tile's dispatch: uTILE_CELLS x uTILE_CELLS lattice cells, the same
	// shape for both LODs.
	constexpr u_int uCELLS_PER_TILE =
		Flux_GrassConfig::uTILE_CELLS * Flux_GrassConfig::uTILE_CELLS;

	// The convergence allowance. FOUR tiles against a measured 2 -- one doubling
	// of the observation, not a number chosen to make a run pass. Anything larger
	// would start to admit a genuinely differently-built field: the parked counts
	// these suites see are ~108 tiles, so four is under 4% of the field.
	constexpr u_int uMAX_CONVERGENCE_TILES = 4u;
	constexpr u_int uMAX_CONVERGENCE_CELLS =
		uCELLS_PER_TILE * uMAX_CONVERGENCE_TILES;

	// PURE. The resumed field must be non-empty AND within the convergence
	// allowance of the parked one.
	inline bool Restored(u_int uAtPark, u_int uAfter)
	{
		if (uAfter == 0u)
		{
			return false;
		}
		const u_int uDelta = uAfter > uAtPark ? uAfter - uAtPark : uAtPark - uAfter;
		return uDelta <= uMAX_CONVERGENCE_CELLS;
	}
}
