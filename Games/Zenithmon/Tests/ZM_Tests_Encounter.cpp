#include "Zenith.h"

// ============================================================================
// ZM_Tests_Encounter -- S5 headless unit gate for ZM_EncounterZone (category
// ZM_Encounter).
//
// ZM_EncounterZone is the pure, deterministic wild-encounter selector: a per-
// route rate gate, a weighted slot pick, and an inclusive level band, all drawn
// from a caller-owned seeded ZM_BattleRNG (no entity/scene/Flux state). These
// units lock the FROZEN draw order (rate gate -> weighted slot -> level band),
// the empty-table / zero-rate / rate-extremes edges, seed determinism, and the
// per-scene resolution (only a ROUTE row with slots + a non-zero rate rolls).
//   1. SelectSlotIndex_WeightedDeterminism   -- pure f(seed); same seed => same index.
//   2. SelectSlotIndex_ProportionalHistogram -- hit counts track the weights (not degenerate).
//   3. SelectSlotIndex_SingleSlot            -- a 1-slot table always returns index 0.
//   4. RollStep_RateGateExtremes             -- rate 0 never fires; rate 256 always fires + valid.
//   5. RollStep_EmptyTable                   -- uCount==0 (any rate) => {false, NONE, 0}.
//   6. RollStep_LevelBandInclusive           -- level in [min,max]; min==max pinned; endpoints reachable.
//   7. RollStep_Determinism                  -- same seed + inputs => byte-identical result.
//   8. RollStepForScene_NonRouteNoEncounter  -- town / interior / battle never encounter.
//   9. RollStepForScene_RouteYieldsRosterSpecies -- Route 1 hits stay in-roster + in-band.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/World/ZM_EncounterZone.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

namespace
{
	// A 3-species route table (mirrors the Route 1 shape) for the RollStep edges.
	// Overall level band across the table is [2, 5].
	const ZM_EncounterSlot s_axRouteTable[] = {
		{ ZM_SPECIES_PIPWIT,  2, 4, 40 },
		{ ZM_SPECIES_NIBBIN,  2, 4, 40 },
		{ ZM_SPECIES_SPARKIT, 3, 5, 20 },
	};

	// A deliberately-lopsided weight table (1:3:6) for the weighted-pick tests.
	const ZM_EncounterSlot s_axWeightTable[] = {
		{ ZM_SPECIES_PIPWIT,  2, 4, 1 },
		{ ZM_SPECIES_NIBBIN,  2, 4, 3 },
		{ ZM_SPECIES_SPARKIT, 3, 5, 6 },
	};

	template<typename T, u_int N>
	constexpr u_int ArrLen(const T (&)[N]) { return N; }

	// True iff some slot carries eSpecies AND uLevel lies in that slot's band.
	bool SlotAdmits(const ZM_EncounterSlot* pxSlots, u_int uCount, ZM_SPECIES_ID eSpecies, u_int uLevel)
	{
		for (u_int i = 0; i < uCount; ++i)
		{
			if (pxSlots[i].m_eSpecies == eSpecies
				&& uLevel >= pxSlots[i].m_uMinLevel
				&& uLevel <= pxSlots[i].m_uMaxLevel)
			{
				return true;
			}
		}
		return false;
	}
}

// ############################################################################
// 1. SelectSlotIndex -- pure, deterministic weighted pick
// ############################################################################

// A fixed-seed pick, and a fresh identically-seeded RNG, must land on the SAME
// slot index (SelectSlotIndex is a pure function of the RNG stream).
ZENITH_TEST(ZM_Encounter, SelectSlotIndex_WeightedDeterminism)
{
	const u_int uCount = ArrLen(s_axWeightTable);

	ZM_BattleRNG xA(0xC0FFEEull);
	const u_int uIdxA = ZM_EncounterZone::SelectSlotIndex(s_axWeightTable, uCount, xA);

	ZM_BattleRNG xB(0xC0FFEEull);
	const u_int uIdxB = ZM_EncounterZone::SelectSlotIndex(s_axWeightTable, uCount, xB);

	ZENITH_ASSERT_EQ(uIdxA, uIdxB, "SelectSlotIndex must be deterministic for a fixed seed");
	ZENITH_ASSERT_LT(uIdxA, uCount, "selected index out of range");
}

// ############################################################################
// 2. SelectSlotIndex -- proportional weighting (locks it is real, not degenerate)
// ############################################################################

// Over 10000 draws advancing one RNG, a 1:3:6 weight table's hit counts must be
// strictly ordered and near their shares (expected 1000 / 3000 / 6000). Bands
// are deliberately generous -- this locks that weighting is honoured, not that a
// particular RNG lands on exact frequencies.
ZENITH_TEST(ZM_Encounter, SelectSlotIndex_ProportionalHistogram)
{
	const u_int uCount = ArrLen(s_axWeightTable);
	ZENITH_ASSERT_EQ(uCount, 3u, "histogram assumes a 3-slot weight table");

	u_int auHits[3] = { 0u, 0u, 0u };
	ZM_BattleRNG xRng(0x5EED1234ull);
	const u_int uDraws = 10000u;
	for (u_int i = 0; i < uDraws; ++i)
	{
		const u_int uIdx = ZM_EncounterZone::SelectSlotIndex(s_axWeightTable, uCount, xRng);
		ZENITH_ASSERT_LT(uIdx, uCount, "histogram draw out of range");
		++auHits[uIdx];
	}

	// Every draw is accounted for and every slot is reachable.
	ZENITH_ASSERT_EQ(auHits[0] + auHits[1] + auHits[2], uDraws, "draws unaccounted for");
	ZENITH_ASSERT_GT(auHits[0], 0u, "weight-1 slot never selected");
	ZENITH_ASSERT_GT(auHits[1], 0u, "weight-3 slot never selected");
	ZENITH_ASSERT_GT(auHits[2], 0u, "weight-6 slot never selected");

	// Strict ordering by weight is the strongest non-degeneracy lock.
	ZENITH_ASSERT_LT(auHits[0], auHits[1], "weight-3 slot must beat weight-1 slot");
	ZENITH_ASSERT_LT(auHits[1], auHits[2], "weight-6 slot must beat weight-3 slot");

	// Generous per-slot bands around the expected shares (1000 / 3000 / 6000).
	ZENITH_ASSERT_GT(auHits[0], 500u,  "weight-1 share implausibly low");
	ZENITH_ASSERT_LT(auHits[0], 1600u, "weight-1 share implausibly high");
	ZENITH_ASSERT_GT(auHits[1], 2200u, "weight-3 share implausibly low");
	ZENITH_ASSERT_LT(auHits[1], 3800u, "weight-3 share implausibly high");
	ZENITH_ASSERT_GT(auHits[2], 4800u, "weight-6 share implausibly low");
	ZENITH_ASSERT_LT(auHits[2], 7000u, "weight-6 share implausibly high");
}

// ############################################################################
// 3. SelectSlotIndex -- a single-slot table degenerates to index 0
// ############################################################################

ZENITH_TEST(ZM_Encounter, SelectSlotIndex_SingleSlot)
{
	const ZM_EncounterSlot axOne[] = { { ZM_SPECIES_PIPWIT, 3, 6, 5 } };
	for (u_int64 ulSeed = 1ull; ulSeed <= 64ull; ++ulSeed)
	{
		ZM_BattleRNG xRng(ulSeed);
		const u_int uIdx = ZM_EncounterZone::SelectSlotIndex(axOne, 1u, xRng);
		ZENITH_ASSERT_EQ(uIdx, 0u,
			"a 1-slot table must always select index 0 (seed %llu)", (unsigned long long)ulSeed);
	}
}

// ############################################################################
// 4. RollStep -- rate-gate extremes (0 never fires, 256 always fires)
// ############################################################################

// Rate 0: the gate RandBelow(256) < 0 can never pass -> {false, NONE, 0}.
// Rate 256: RandBelow(256) in [0,255] is always < 256 -> always fires with a
// species/level drawn from the table.
ZENITH_TEST(ZM_Encounter, RollStep_RateGateExtremes)
{
	const u_int uCount = ArrLen(s_axRouteTable);

	for (u_int64 ulSeed = 1ull; ulSeed <= 64ull; ++ulSeed)
	{
		ZM_BattleRNG xZero(ulSeed);
		const ZM_EncounterRollResult xR0 = ZM_EncounterZone::RollStep(s_axRouteTable, uCount, 0u, xZero);
		ZENITH_ASSERT_FALSE(xR0.m_bEncounter, "rate 0 must never fire (seed %llu)", (unsigned long long)ulSeed);
		ZENITH_ASSERT_EQ((u_int)xR0.m_eSpecies, (u_int)ZM_SPECIES_NONE, "rate-0 miss must carry NONE");
		ZENITH_ASSERT_EQ(xR0.m_uLevel, 0u, "rate-0 miss must carry level 0");

		ZM_BattleRNG xFull(ulSeed);
		const ZM_EncounterRollResult xR256 = ZM_EncounterZone::RollStep(s_axRouteTable, uCount, 256u, xFull);
		ZENITH_ASSERT_TRUE(xR256.m_bEncounter, "rate 256 must always fire (seed %llu)", (unsigned long long)ulSeed);
		ZENITH_ASSERT_NE((u_int)xR256.m_eSpecies, (u_int)ZM_SPECIES_NONE, "a fired encounter must name a species");
		ZENITH_ASSERT_TRUE(SlotAdmits(s_axRouteTable, uCount, xR256.m_eSpecies, xR256.m_uLevel),
			"fired species/level must come from the table (seed %llu)", (unsigned long long)ulSeed);
	}
}

// ############################################################################
// 5. RollStep -- an empty table short-circuits for any rate
// ############################################################################

ZENITH_TEST(ZM_Encounter, RollStep_EmptyTable)
{
	// uCount == 0 short-circuits regardless of the rate (even a guaranteed 256).
	const u_int auRates[] = { 0u, 1u, 128u, 256u };
	for (u_int r = 0; r < ArrLen(auRates); ++r)
	{
		for (u_int64 ulSeed = 1ull; ulSeed <= 16ull; ++ulSeed)
		{
			ZM_BattleRNG xRng(ulSeed);
			const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStep(s_axRouteTable, 0u, auRates[r], xRng);
			ZENITH_ASSERT_FALSE(xR.m_bEncounter, "empty table must never fire (rate %u)", auRates[r]);
			ZENITH_ASSERT_EQ((u_int)xR.m_eSpecies, (u_int)ZM_SPECIES_NONE, "empty-table miss must carry NONE");
			ZENITH_ASSERT_EQ(xR.m_uLevel, 0u, "empty-table miss must carry level 0");
		}
	}
}

// ############################################################################
// 6. RollStep -- the level band is inclusive on both ends
// ############################################################################

// A min==max slot pins the level exactly; a [3,8] band keeps every roll inside
// and reaches both endpoints across enough seeds. Rate 256 guarantees a fire.
ZENITH_TEST(ZM_Encounter, RollStep_LevelBandInclusive)
{
	const ZM_EncounterSlot axPinned[] = { { ZM_SPECIES_PIPWIT, 7, 7, 5 } };
	for (u_int64 ulSeed = 1ull; ulSeed <= 64ull; ++ulSeed)
	{
		ZM_BattleRNG xRng(ulSeed);
		const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStep(axPinned, 1u, 256u, xRng);
		ZENITH_ASSERT_TRUE(xR.m_bEncounter, "pinned slot must fire at rate 256");
		ZENITH_ASSERT_EQ(xR.m_uLevel, 7u,
			"min==max slot must always yield that level (seed %llu)", (unsigned long long)ulSeed);
	}

	const ZM_EncounterSlot axBand[] = { { ZM_SPECIES_NIBBIN, 3, 8, 5 } };
	bool bSawMin = false;
	bool bSawMax = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 512ull; ++ulSeed)
	{
		ZM_BattleRNG xRng(ulSeed);
		const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStep(axBand, 1u, 256u, xRng);
		ZENITH_ASSERT_TRUE(xR.m_bEncounter, "band slot must fire at rate 256");
		ZENITH_ASSERT_GE(xR.m_uLevel, 3u, "level below band min (seed %llu)", (unsigned long long)ulSeed);
		ZENITH_ASSERT_LE(xR.m_uLevel, 8u, "level above band max (seed %llu)", (unsigned long long)ulSeed);
		if (xR.m_uLevel == 3u) { bSawMin = true; }
		if (xR.m_uLevel == 8u) { bSawMax = true; }
	}
	ZENITH_ASSERT_TRUE(bSawMin, "band min (3) never reached across 512 seeds");
	ZENITH_ASSERT_TRUE(bSawMax, "band max (8) never reached across 512 seeds");
}

// ############################################################################
// 7. RollStep -- same seed + inputs => byte-identical result
// ############################################################################

ZENITH_TEST(ZM_Encounter, RollStep_Determinism)
{
	const u_int uCount = ArrLen(s_axRouteTable);
	for (u_int64 ulSeed = 1ull; ulSeed <= 128ull; ++ulSeed)
	{
		ZM_BattleRNG xA(ulSeed);
		ZM_BattleRNG xB(ulSeed);
		const ZM_EncounterRollResult xRA = ZM_EncounterZone::RollStep(s_axRouteTable, uCount, 200u, xA);
		const ZM_EncounterRollResult xRB = ZM_EncounterZone::RollStep(s_axRouteTable, uCount, 200u, xB);

		ZENITH_ASSERT_EQ(xRA.m_bEncounter, xRB.m_bEncounter,
			"encounter flag not deterministic (seed %llu)", (unsigned long long)ulSeed);
		ZENITH_ASSERT_EQ((u_int)xRA.m_eSpecies, (u_int)xRB.m_eSpecies,
			"species not deterministic (seed %llu)", (unsigned long long)ulSeed);
		ZENITH_ASSERT_EQ(xRA.m_uLevel, xRB.m_uLevel,
			"level not deterministic (seed %llu)", (unsigned long long)ulSeed);
	}
}

// ############################################################################
// 8. RollStepForScene -- non-route scenes never encounter
// ############################################################################

// A town (Dawnmere), an interior (Player's Home), and the additive Battle scene
// carry no slots + a 0 rate, so they resolve to the clean miss for every seed.
ZENITH_TEST(ZM_Encounter, RollStepForScene_NonRouteNoEncounter)
{
	const ZM_SCENE_ID aeNonRoute[] = { ZM_SCENE_DAWNMERE, ZM_SCENE_PLAYERHOME, ZM_SCENE_BATTLE };
	for (u_int s = 0; s < ArrLen(aeNonRoute); ++s)
	{
		for (u_int64 ulSeed = 1ull; ulSeed <= 64ull; ++ulSeed)
		{
			ZM_BattleRNG xRng(ulSeed);
			const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStepForScene(aeNonRoute[s], xRng);
			ZENITH_ASSERT_FALSE(xR.m_bEncounter,
				"non-route scene %u must never fire (seed %llu)", (u_int)aeNonRoute[s], (unsigned long long)ulSeed);
			ZENITH_ASSERT_EQ((u_int)xR.m_eSpecies, (u_int)ZM_SPECIES_NONE, "non-route miss must carry NONE");
			ZENITH_ASSERT_EQ(xR.m_uLevel, 0u, "non-route miss must carry level 0");
		}
	}
}

// ############################################################################
// 9. RollStepForScene -- Route 1 hits stay in-roster + in-band
// ############################################################################

// Sweep many seeds against Route 1. Its tuned rate makes encounters intermittent,
// so accumulate hits over enough steps and validate each one against the route's
// OWN slot table (read live from the world spec, so this stays correct if the
// roster/levels are retuned). A miss must be the clean sentinel.
ZENITH_TEST(ZM_Encounter, RollStepForScene_RouteYieldsRosterSpecies)
{
	const ZM_WorldSpec& xRoute = ZM_GetWorldSpec(ZM_SCENE_ROUTE1);
	ZENITH_ASSERT_EQ((u_int)xRoute.m_eKind, (u_int)ZM_SCENE_KIND_ROUTE, "Route 1 must be a ROUTE");
	ZENITH_ASSERT_GT(xRoute.m_uEncounterCount, 0u, "Route 1 must carry encounter slots");

	u_int uHits = 0u;
	for (u_int64 ulSeed = 1ull; ulSeed <= 6000ull; ++ulSeed)
	{
		ZM_BattleRNG xRng(ulSeed);
		const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStepForScene(ZM_SCENE_ROUTE1, xRng);
		if (!xR.m_bEncounter)
		{
			ZENITH_ASSERT_EQ((u_int)xR.m_eSpecies, (u_int)ZM_SPECIES_NONE, "Route 1 miss must carry NONE");
			ZENITH_ASSERT_EQ(xR.m_uLevel, 0u, "Route 1 miss must carry level 0");
			continue;
		}
		++uHits;
		ZENITH_ASSERT_TRUE(
			SlotAdmits(xRoute.m_pxEncounters, xRoute.m_uEncounterCount, xR.m_eSpecies, xR.m_uLevel),
			"Route 1 hit species %u level %u is outside the route's slot table (seed %llu)",
			(u_int)xR.m_eSpecies, xR.m_uLevel, (unsigned long long)ulSeed);
	}
	ZENITH_ASSERT_GT(uHits, 0u, "Route 1 fired no encounters across 6000 seeds -- rate wiring suspect");
}

// ############################################################################
// 10. RollStep -- inert / miss steps must not perturb the caller's RNG stream
// ############################################################################

// The rig-stability contract (S5 item 2 design): an INERT step (empty table or
// rate 0) must draw the RNG ZERO times, and a rate-gate MISS must draw EXACTLY
// ONCE (the single rate roll). Verified by comparing the raw Next() position of
// the stepped RNG against an identically-seeded control advanced by the expected
// number of draws. A future refactor that drew a slot pick on an inert/miss step
// would leave every result-only test above passing while silently perturbing a
// rigged stream -- this is the one test that catches that.
ZENITH_TEST(ZM_Encounter, RollStep_InertAndMissDoNotPerturbRng)
{
	const u_int uCount = ArrLen(s_axRouteTable);

	// (a) Empty table: zero draws, even at rate 256.
	{
		ZM_BattleRNG xStepped(0xABCDEFull);
		ZM_BattleRNG xControl(0xABCDEFull);
		const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStep(s_axRouteTable, 0u, 256u, xStepped);
		ZENITH_ASSERT_FALSE(xR.m_bEncounter, "empty table must not fire");
		ZENITH_ASSERT_EQ(xStepped.Next(), xControl.Next(), "an empty-table step must not advance the RNG");
	}

	// (b) Zero rate: zero draws, even with a populated table.
	{
		ZM_BattleRNG xStepped(0x13579ull);
		ZM_BattleRNG xControl(0x13579ull);
		const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStep(s_axRouteTable, uCount, 0u, xStepped);
		ZENITH_ASSERT_FALSE(xR.m_bEncounter, "zero rate must not fire");
		ZENITH_ASSERT_EQ(xStepped.Next(), xControl.Next(), "a zero-rate step must not advance the RNG");
	}

	// (c) Rate-gate miss: EXACTLY one draw. Rate 1/256 misses for almost every
	// seed (RandBelow(256) never rejects, so a miss is exactly one Next()); find a
	// miss and confirm the stepped RNG advanced by exactly one draw vs the control.
	{
		bool bCheckedMiss = false;
		for (u_int64 ulSeed = 1ull; ulSeed <= 64ull && !bCheckedMiss; ++ulSeed)
		{
			ZM_BattleRNG xStepped(ulSeed);
			ZM_BattleRNG xControl(ulSeed);
			const ZM_EncounterRollResult xR = ZM_EncounterZone::RollStep(s_axRouteTable, uCount, 1u, xStepped);
			if (xR.m_bEncounter)
			{
				continue;   // rare hit at rate 1/256; try another seed
			}
			xControl.Next();   // the single rate roll a miss consumes
			ZENITH_ASSERT_EQ(xStepped.Next(), xControl.Next(),
				"a rate-gate miss must draw the RNG exactly once (seed %llu)", (unsigned long long)ulSeed);
			bCheckedMiss = true;
		}
		ZENITH_ASSERT_TRUE(bCheckedMiss, "no rate-1 miss found across 64 seeds (rate wiring suspect)");
	}
}
