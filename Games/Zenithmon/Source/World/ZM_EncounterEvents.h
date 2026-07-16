#pragma once
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_SCENE_ID

// Fired by ZM_TallGrassSystem (SC3) on a wild-encounter roll and dispatched via
// Zenith_EventDispatcher. Item 2 defines + emits it; item 3 adds the real
// subscriber that triggers the additive battle. POD payload.
struct ZM_OnWildEncounter
{
	ZM_SPECIES_ID	m_eSpecies;
	u_int			m_uLevel;
	ZM_SCENE_ID		m_eSourceScene;
};
