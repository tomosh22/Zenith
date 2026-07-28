#pragma once
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID
#include "Zenithmon/Source/Data/ZM_TrainerData.h"    // ZM_TRAINER_ID (S7 item 3 SC5)
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

// Fired when a registered trainer forces a battle (S7 item 3 SC5), dispatched
// through the same Zenith_EventDispatcher as ZM_OnWildEncounter but as a DISTINCT
// type (design default Q-G): the wild payload, its validator and its subscriber
// stay byte-identical, and the two entry channels can never be confused for one
// another. SC5 ships the CHANNEL and the subscriber; SC6 adds the overworld sight
// FSM that raises it. POD payload -- the team, the prize, the defeat flag and the
// AI tier all live in the compiled ZM_TrainerData row, so the id is the whole
// payload.
struct ZM_OnTrainerEncounter
{
	ZM_TRAINER_ID	m_eTrainer;
	ZM_SCENE_ID		m_eSourceScene;
};
