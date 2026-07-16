#pragma once
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_EncounterSlot, ZM_SCENE_ID
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_SPECIES_ID, ZM_SPECIES_NONE
class ZM_BattleRNG;

// Outcome of one tile-step encounter roll. m_eSpecies/m_uLevel are valid only when
// m_bEncounter is true (else ZM_SPECIES_NONE / 0).
struct ZM_EncounterRollResult
{
	bool			m_bEncounter	= false;
	ZM_SPECIES_ID	m_eSpecies		= ZM_SPECIES_NONE;
	u_int			m_uLevel		= 0u;
};

// Pure, deterministic, headless. All randomness is drawn from the caller-owned
// ZM_BattleRNG. No entity/scene/Flux state touched.
namespace ZM_EncounterZone
{
	// Weighted slot pick over the slot weight field. Precondition (Zenith_Assert):
	// uCount > 0 and the weight sum > 0. FROZEN draw order for rig stability.
	u_int SelectSlotIndex(const ZM_EncounterSlot* pxSlots, u_int uCount, ZM_BattleRNG& xRng);

	// FROZEN draw order: (1) rate gate rng.RandBelow(256) < uRatePer256; if it fails,
	// return {false,NONE,0} WITHOUT drawing again. (2) SelectSlotIndex. (3) inclusive
	// level in [minLevel,maxLevel] via the RNG. Empty table (uCount==0) or zero rate
	// => {false,NONE,0}.
	ZM_EncounterRollResult RollStep(const ZM_EncounterSlot* pxSlots, u_int uCount,
		u_int uRatePer256, ZM_BattleRNG& xRng);

	// Resolve eScene's ZM_WorldSpec row (its slot table + m_uEncounterRatePer256) then
	// RollStep. A non-ROUTE scene, a scene with no slots, or rate 0 => {false,NONE,0}.
	ZM_EncounterRollResult RollStepForScene(ZM_SCENE_ID eScene, ZM_BattleRNG& xRng);
}
