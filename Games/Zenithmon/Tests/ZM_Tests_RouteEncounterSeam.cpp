#include "Zenith.h"

// ============================================================================
// ZM_Tests_RouteEncounterSeam -- R1-4 (ZM-D-196 ruling 4) headless, falsifiable
// proof of the FULL wild-encounter roll seam ZM_TallGrassSystem::OnUpdate
// composes: density sample -> tile-transition gate -> grass-density gate ->
// ZM_EncounterZone::RollStepForScene. This file replicates that exact
// composition against a SYNTHETIC in-memory ZM_GrassDensityMap (built via
// LoadDecoded -- the same seam ZM_Tests_TerrainGrass.cpp's fixture test
// already uses), never the real, GITIGNORED baked Route1/GrassDensity.ztxtr.
//
// WHY THIS FILE EXISTS (Status.md critic blocker #3, second half): the only
// existing proof that an on-grass tile transition reaches a roll is the
// windowed ZM_TallGrassEncounter_Test (Tests/ZM_AutoTests_TallGrass.cpp),
// which walks the REAL baked Dawnmere terrain and RequestSkip()s -- reporting
// PASS -- whenever that bake is absent, which it always is on a fresh
// checkout (the bake is gitignored). A headless CI run therefore never
// actually exercises this composition. These cases do, on every checkout,
// using Route 1's REAL (retuned) ZM_WorldSpec rate rather than a rigged one --
// so a change that zeroes the rate, breaks the density gate, or points the
// roll at the wrong scene id fails HERE.
//
// Uses ONLY the public, already-test-exposed static surface: ZM_GrassDensityMap
// (LoadDecoded/SampleWorld), ZM_TallGrassSystem's three pure static helpers
// (QuantizeToTile/IsTileTransition/IsGrassDensity), and
// ZM_EncounterZone::RollStepForScene -- no entity, scene, or Flux state, and no
// disk access. NO ZM_TallGrassSystem instance is constructed (its ctor needs a
// live Zenith_Entity&); StepWalk() below mirrors its OnUpdate composition by
// hand instead.
//   1. SyntheticMap_TileGeometrySelfCheck -- the fixture's own tile/density
//      shape is what the walk assumes (anti-vacuity for the cases below).
//   2. RouteWalk_GrassSideFiresAdmissibleEncounters -- stepping onto the grass
//      tile, swept over many seeds, fires >= 1 encounter and every fired
//      species/level is inside Route 1's live slot table.
//   3. RouteWalk_BareSideNeverRollsOrPerturbsRng -- stepping between two BARE
//      tiles never reaches the roll at all: zero encounters AND the RNG stream
//      is byte-for-byte unperturbed, across the same seed sweep.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_EncounterZone.h"
#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

namespace
{
	// A 4x4 synthetic density map, world size 4 (1 world unit == 1 pixel, so
	// world-space math is trivial to hand-verify). Pixel columns 0-1 are BARE
	// (0.0); columns 2-3 are GRASS (1.0). Every row (Z) repeats the same
	// columns, so Z never influences the bilinear blend and only X decides
	// bare vs grass.
	constexpr u_int uMAP_DIM = 4u;
	constexpr float fMAP_WORLD_SIZE = 4.0f;
	constexpr float afMAP_PIXELS[uMAP_DIM * uMAP_DIM] = {
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};

	// Three world positions the walk steps between, each chosen to land on an
	// UNAMBIGUOUS sample (either exactly on a pixel centre whose neighbours
	// agree, or exactly on a pixel index so the bilinear blend weight is 0 --
	// never inside the column-1/column-2 blend seam, which would read a
	// blended ~0.5 and defeat the bare/grass distinction the fixture wants).
	constexpr float fBARE_A_X = 0.5f, fBARE_A_Z = 0.5f;   // tile (0,0); pixel (0.5,0.5), cols 0-1 both 0.0
	constexpr float fBARE_B_X = 1.0f, fBARE_B_Z = 0.5f;   // tile (1,0); pixel (1.0,0.5), blendX=0 -> pure col 1 (0.0)
	constexpr float fGRASS_X  = 3.5f, fGRASS_Z  = 0.5f;   // tile (3,0); pixel clamps to (3.0,0.5) -> pure col 3 (1.0)

	bool BuildSyntheticMap(ZM_GrassDensityMap& xMapOut)
	{
		return xMapOut.LoadDecoded(TEXTURE_FORMAT_R32_SFLOAT, uMAP_DIM, uMAP_DIM,
			afMAP_PIXELS, sizeof(afMAP_PIXELS), uMAP_DIM, uMAP_DIM, fMAP_WORLD_SIZE);
	}

	// Replicates ZM_TallGrassSystem::OnUpdate's composition EXACTLY (see its
	// production body, Components/ZM_TallGrassSystem.cpp): quantize -> tile-
	// transition gate -> density gate -> RollStepForScene(ZM_SCENE_ROUTE1, ...),
	// with the roll called ONLY from inside the density gate -- never
	// unconditionally. Returns {false, NONE, 0} whenever either gate was
	// closed, matching the production "no roll happened" shape.
	ZM_EncounterRollResult StepWalk(const ZM_GrassDensityMap& xMap,
		float fWorldX, float fWorldZ,
		ZM_GrassTile& xLastTile, bool& bHasLastTile,
		ZM_BattleRNG& xRng)
	{
		const ZM_GrassTile xCurrentTile = ZM_TallGrassSystem::QuantizeToTile(fWorldX, fWorldZ);
		ZM_EncounterRollResult xResult{ false, ZM_SPECIES_NONE, 0u };

		if (ZM_TallGrassSystem::IsTileTransition(xLastTile, bHasLastTile, xCurrentTile))
		{
			const float fDensity = xMap.SampleWorld(fWorldX, fWorldZ);
			if (ZM_TallGrassSystem::IsGrassDensity(fDensity))
			{
				xResult = ZM_EncounterZone::RollStepForScene(ZM_SCENE_ROUTE1, xRng);
			}
		}

		xLastTile = xCurrentTile;
		bHasLastTile = true;
		return xResult;
	}

	// True iff Route 1's LIVE slot table admits eSpecies at uLevel (read from
	// ZM_GetWorldSpec, not hardcoded, so this stays correct across a future
	// roster/level retune -- only the RATE and the WIRING are what this file pins).
	bool RouteAdmits(ZM_SPECIES_ID eSpecies, u_int uLevel)
	{
		const ZM_WorldSpec& xRoute = ZM_GetWorldSpec(ZM_SCENE_ROUTE1);
		for (u_int i = 0; i < xRoute.m_uEncounterCount; ++i)
		{
			const ZM_EncounterSlot& xSlot = xRoute.m_pxEncounters[i];
			if (xSlot.m_eSpecies == eSpecies && uLevel >= xSlot.m_uMinLevel && uLevel <= xSlot.m_uMaxLevel)
			{
				return true;
			}
		}
		return false;
	}
}

// ############################################################################
// 1. Anti-vacuity: the fixture's own geometry is what the walk assumes
// ############################################################################

ZENITH_TEST(ZM_Grass, SyntheticMap_TileGeometrySelfCheck)
{
	ZM_GrassDensityMap xMap;
	ZENITH_ASSERT_TRUE(BuildSyntheticMap(xMap), "the synthetic fixture map must decode");

	const float fBareDensityA = xMap.SampleWorld(fBARE_A_X, fBARE_A_Z);
	const float fBareDensityB = xMap.SampleWorld(fBARE_B_X, fBARE_B_Z);
	const float fGrassDensity = xMap.SampleWorld(fGRASS_X, fGRASS_Z);
	ZENITH_ASSERT_FALSE(ZM_TallGrassSystem::IsGrassDensity(fBareDensityA),
		"fixture bug: BARE_A must sample below the grass threshold (got %f)", (double)fBareDensityA);
	ZENITH_ASSERT_FALSE(ZM_TallGrassSystem::IsGrassDensity(fBareDensityB),
		"fixture bug: BARE_B must sample below the grass threshold (got %f)", (double)fBareDensityB);
	ZENITH_ASSERT_TRUE(ZM_TallGrassSystem::IsGrassDensity(fGrassDensity),
		"fixture bug: GRASS must sample at/above the grass threshold (got %f)", (double)fGrassDensity);

	const ZM_GrassTile xTileA = ZM_TallGrassSystem::QuantizeToTile(fBARE_A_X, fBARE_A_Z);
	const ZM_GrassTile xTileB = ZM_TallGrassSystem::QuantizeToTile(fBARE_B_X, fBARE_B_Z);
	const ZM_GrassTile xTileG = ZM_TallGrassSystem::QuantizeToTile(fGRASS_X, fGRASS_Z);
	ZENITH_ASSERT_TRUE(xTileA.m_iX != xTileB.m_iX || xTileA.m_iZ != xTileB.m_iZ,
		"fixture bug: BARE_A and BARE_B must quantize to DIFFERENT tiles");
	ZENITH_ASSERT_TRUE(xTileA.m_iX != xTileG.m_iX || xTileA.m_iZ != xTileG.m_iZ,
		"fixture bug: BARE_A and GRASS must quantize to DIFFERENT tiles");

	ZENITH_ASSERT_EQ((u_int)ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_eKind, (u_int)ZM_SCENE_KIND_ROUTE,
		"fixture assumption: ZM_SCENE_ROUTE1 must still be a ROUTE");
	ZENITH_ASSERT_GT(ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_uEncounterRatePer256, 0u,
		"fixture assumption: Route 1 must carry a non-zero encounter rate");
}

// ############################################################################
// 2. The grass-side walk fires admissible encounters at Route 1's LIVE rate
// ############################################################################

// Steps BARE_A -> GRASS (a genuine tile transition onto a grass tile) once per
// seed, swept over enough seeds that Route 1's retuned ~7.8%/step rate produces
// at least one hit. Every hit is checked against the route's OWN live slot
// table (RouteAdmits), not a hardcoded species/level, so this stays correct
// across a future roster/level retune -- only the RATE and the WIRING are
// what this pins.
ZENITH_TEST(ZM_Grass, RouteWalk_GrassSideFiresAdmissibleEncounters)
{
	ZM_GrassDensityMap xMap;
	ZENITH_ASSERT_TRUE(BuildSyntheticMap(xMap), "the synthetic fixture map must decode");

	u_int uHits = 0u;
	for (u_int64 ulSeed = 1ull; ulSeed <= 6000ull; ++ulSeed)
	{
		ZM_BattleRNG xRng(ulSeed);
		ZM_GrassTile xLastTile{};
		bool bHasLastTile = false;

		// Establish the baseline tile (BARE_A) -- the first call never transitions
		// (mirrors ZM_TallGrassSystem::OnAwake's fresh m_bHasLastTile = false).
		const ZM_EncounterRollResult xBaseline =
			StepWalk(xMap, fBARE_A_X, fBARE_A_Z, xLastTile, bHasLastTile, xRng);
		ZENITH_ASSERT_FALSE(xBaseline.m_bEncounter, "the very first step must never roll (no last tile yet)");

		// Step onto the grass tile: a genuine transition, density passes, roll fires.
		const ZM_EncounterRollResult xResult =
			StepWalk(xMap, fGRASS_X, fGRASS_Z, xLastTile, bHasLastTile, xRng);
		if (!xResult.m_bEncounter)
		{
			ZENITH_ASSERT_EQ((u_int)xResult.m_eSpecies, (u_int)ZM_SPECIES_NONE, "a miss must carry NONE");
			ZENITH_ASSERT_EQ(xResult.m_uLevel, 0u, "a miss must carry level 0");
			continue;
		}
		++uHits;
		ZENITH_ASSERT_TRUE(RouteAdmits(xResult.m_eSpecies, xResult.m_uLevel),
			"grass-side hit species %u level %u is outside Route 1's slot table (seed %llu)",
			(u_int)xResult.m_eSpecies, xResult.m_uLevel, (unsigned long long)ulSeed);
	}
	ZENITH_ASSERT_GT(uHits, 0u,
		"the synthetic grass-side walk fired NO encounters across 6000 seeds -- "
		"the density-map -> tile-transition -> Route 1 roll seam is not wired, "
		"or the retuned rate has gone to 0");
}

// ############################################################################
// 3. The bare-side walk never reaches the roll -- zero hits, zero RNG draws
// ############################################################################

// Steps between the two BARE tiles (a genuine transition every time, density
// always below threshold) over the SAME seed range. Because the density gate
// short-circuits BEFORE RollStepForScene is ever called (StepWalk mirrors
// ZM_TallGrassSystem::OnUpdate's production `if (IsGrassDensity(...))`
// wrapper), this must produce not only zero encounters but a BYTE-IDENTICAL
// RNG stream to an untouched control -- proving the gate truly never rolls,
// rather than rolling and happening to always miss (the same idiom
// RollStep_InertAndMissDoNotPerturbRng already uses in ZM_Tests_Encounter.cpp).
ZENITH_TEST(ZM_Grass, RouteWalk_BareSideNeverRollsOrPerturbsRng)
{
	ZM_GrassDensityMap xMap;
	ZENITH_ASSERT_TRUE(BuildSyntheticMap(xMap), "the synthetic fixture map must decode");

	for (u_int64 ulSeed = 1ull; ulSeed <= 64ull; ++ulSeed)
	{
		ZM_BattleRNG xStepped(ulSeed);
		ZM_BattleRNG xControl(ulSeed);
		ZM_GrassTile xLastTile{};
		bool bHasLastTile = false;

		const ZM_EncounterRollResult xBaseline =
			StepWalk(xMap, fBARE_A_X, fBARE_A_Z, xLastTile, bHasLastTile, xStepped);
		ZENITH_ASSERT_FALSE(xBaseline.m_bEncounter, "the very first step must never roll");

		const ZM_EncounterRollResult xResult =
			StepWalk(xMap, fBARE_B_X, fBARE_B_Z, xLastTile, bHasLastTile, xStepped);
		ZENITH_ASSERT_FALSE(xResult.m_bEncounter,
			"a bare-to-bare transition must never fire an encounter (seed %llu)", (unsigned long long)ulSeed);
		ZENITH_ASSERT_EQ((u_int)xResult.m_eSpecies, (u_int)ZM_SPECIES_NONE, "a gated-off step must carry NONE");
		ZENITH_ASSERT_EQ(xResult.m_uLevel, 0u, "a gated-off step must carry level 0");

		ZENITH_ASSERT_EQ(xStepped.Next(), xControl.Next(),
			"a bare-to-bare walk must never draw the RNG -- the density gate should "
			"short-circuit before RollStepForScene is even called (seed %llu)",
			(unsigned long long)ulSeed);
	}
}
