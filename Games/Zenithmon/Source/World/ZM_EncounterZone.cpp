#include "Zenith.h"
#include "Zenithmon/Source/World/ZM_EncounterZone.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_WorldSpec, ZM_GetWorldSpec, ZM_SCENE_KIND_ROUTE
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_SPECIES_ID, ZM_SPECIES_NONE
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"       // ZM_BattleRNG

// ============================================================================
// ZM_EncounterZone -- pure, headless, deterministic wild-encounter selection
// (S5 item 2). Every random decision draws from the caller-owned ZM_BattleRNG so
// a rigged stream fully determines the outcome; no entity/scene/Flux state is
// touched. RollStepForScene resolves the ZM_WorldSpec row (slot table + per-route
// rate) and defers to RollStep. The FROZEN draw order (rate gate, then slot pick,
// then inclusive level band) keeps rigged-RNG unit tests stable.
// ============================================================================

namespace ZM_EncounterZone
{
	// Per-tile-transition rate is expressed out of 256 (see ZM_WorldSpec).
	static const u_int uZM_ENCOUNTER_RATE_DENOM = 256u;

	u_int SelectSlotIndex(const ZM_EncounterSlot* pxSlots, u_int uCount, ZM_BattleRNG& xRng)
	{
		Zenith_Assert(pxSlots != nullptr, "ZM_EncounterZone::SelectSlotIndex: null slot table");
		Zenith_Assert(uCount > 0u, "ZM_EncounterZone::SelectSlotIndex: empty slot table");

		u_int uTotal = 0u;
		for (u_int i = 0u; i < uCount; ++i)
		{
			uTotal += pxSlots[i].m_uWeight;
		}
		Zenith_Assert(uTotal > 0u, "ZM_EncounterZone::SelectSlotIndex: total weight must be > 0");

		const u_int uRoll = xRng.RandBelow(uTotal);
		u_int uAcc = 0u;
		for (u_int i = 0u; i < uCount; ++i)
		{
			uAcc += pxSlots[i].m_uWeight;
			if (uRoll < uAcc)
			{
				return i;
			}
		}

		// Unreachable when uTotal == sum of weights (uRoll < uTotal), but return the
		// last valid index as a safety fallback rather than an out-of-range value.
		return uCount - 1u;
	}

	ZM_EncounterRollResult RollStep(const ZM_EncounterSlot* pxSlots, u_int uCount,
		u_int uRatePer256, ZM_BattleRNG& xRng)
	{
		// Empty table or zero rate => no encounter, and (crucially) no RNG draw, so a
		// rigged stream is not perturbed by inert steps.
		if (uCount == 0u || uRatePer256 == 0u)
		{
			return ZM_EncounterRollResult{ false, ZM_SPECIES_NONE, 0u };
		}

		// (1) Rate gate FIRST. On failure, return WITHOUT drawing again.
		if (xRng.RandBelow(uZM_ENCOUNTER_RATE_DENOM) >= uRatePer256)
		{
			return ZM_EncounterRollResult{ false, ZM_SPECIES_NONE, 0u };
		}

		// (2) Weighted slot pick.
		const u_int uIndex = SelectSlotIndex(pxSlots, uCount, xRng);
		const ZM_EncounterSlot& xSlot = pxSlots[uIndex];

		// (3) Inclusive level in [min, max]. ZM_BattleRNG::RandRange(a, b) is [a, b]
		// (inclusive both ends -- verified in ZM_BattleRNG.h), so it maps directly.
		const u_int uLevel = xRng.RandRange(xSlot.m_uMinLevel, xSlot.m_uMaxLevel);

		return ZM_EncounterRollResult{ true, xSlot.m_eSpecies, uLevel };
	}

	ZM_EncounterRollResult RollStepForScene(ZM_SCENE_ID eScene, ZM_BattleRNG& xRng)
	{
		const ZM_WorldSpec& xRow = ZM_GetWorldSpec(eScene);

		// Only ROUTE-kind scenes with a populated slot table and a non-zero rate can
		// produce an encounter; everything else short-circuits with NO RNG draw.
		if (xRow.m_eKind != ZM_SCENE_KIND_ROUTE ||
			xRow.m_uEncounterCount == 0u ||
			xRow.m_uEncounterRatePer256 == 0u)
		{
			return ZM_EncounterRollResult{ false, ZM_SPECIES_NONE, 0u };
		}

		return RollStep(xRow.m_pxEncounters, xRow.m_uEncounterCount, xRow.m_uEncounterRatePer256, xRng);
	}
}
