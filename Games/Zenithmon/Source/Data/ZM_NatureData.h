#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_STAT

// ============================================================================
// ZM_NatureData -- the 25 natures (S1 data core). Each nature raises one stat by
// 10% and lowers another by 10%; the five where the raised and lowered stat are
// the same are NEUTRAL (no net change). Natures never touch HP -- the raised and
// lowered stat are always in {ATTACK, DEFENSE, SPATTACK, SPDEFENSE, SPEED}. The
// 25 rows are exactly the 5x5 grid of (raised, lowered) pairs over those stats.
// Spec: Docs/GameDesignDocument.md section 7 (natures); DecisionLog ZM-D-025.
//
// Real compiled table (ZM-D-009), not derived: only 25 rows, and the pairing is
// exact. Names are original (Scope.md: zero Nintendo IP). ZM_NATURE order is
// save-stable once content ships -- APPEND before ZM_NATURE_COUNT, never reorder.
// The x11/10 and x9/10 multipliers are applied (integer-exact) by ZM_StatCalc.
// ============================================================================

enum ZM_NATURE : u_int
{
	// raised ATTACK
	ZM_NATURE_FERAL,        // neutral (ATK/ATK)
	ZM_NATURE_RECKLESS,     // +ATK -DEF
	ZM_NATURE_BRUTISH,      // +ATK -SPA
	ZM_NATURE_ROWDY,        // +ATK -SPD
	ZM_NATURE_HULKING,      // +ATK -SPE
	// raised DEFENSE
	ZM_NATURE_GUARDED,      // +DEF -ATK
	ZM_NATURE_STOLID,       // neutral (DEF/DEF)
	ZM_NATURE_IRONBOUND,    // +DEF -SPA
	ZM_NATURE_SHELTERED,    // +DEF -SPD
	ZM_NATURE_PONDEROUS,    // +DEF -SPE
	// raised SPATTACK
	ZM_NATURE_ARCANE,       // +SPA -ATK
	ZM_NATURE_MYSTIC,       // +SPA -DEF
	ZM_NATURE_PENSIVE,      // neutral (SPA/SPA)
	ZM_NATURE_ZEALOUS,      // +SPA -SPD
	ZM_NATURE_BROODING,     // +SPA -SPE
	// raised SPDEFENSE
	ZM_NATURE_PLACID,       // +SPD -ATK
	ZM_NATURE_SERENE,       // +SPD -DEF
	ZM_NATURE_DEVOUT,       // +SPD -SPA
	ZM_NATURE_TRANQUIL,     // neutral (SPD/SPD)
	ZM_NATURE_PATIENT,      // +SPD -SPE
	// raised SPEED
	ZM_NATURE_NIMBLE,       // +SPE -ATK
	ZM_NATURE_FLEET,        // +SPE -DEF
	ZM_NATURE_DARTING,      // +SPE -SPA
	ZM_NATURE_SKITTISH,     // +SPE -SPD
	ZM_NATURE_RESTLESS,     // neutral (SPE/SPE)

	ZM_NATURE_COUNT
};

// One nature row. For a neutral nature m_eRaised == m_eLowered (both a non-HP
// stat), which the percent helper reads as "no change".
struct ZM_NatureData
{
	ZM_NATURE	m_eId;
	const char*	m_szName;
	ZM_STAT		m_eRaised;    // ATTACK..SPEED (never HP)
	ZM_STAT		m_eLowered;   // ATTACK..SPEED (never HP); == m_eRaised when neutral
};

// Table accessors (bounds-asserted). GetNatureData indexes by ZM_NATURE.
const ZM_NatureData&	ZM_GetNatureData(ZM_NATURE eId);
u_int					ZM_GetNatureCount();                 // == ZM_NATURE_COUNT
const char*				ZM_GetNatureName(ZM_NATURE eId);     // "NONE" out of range

// The nature multiplier for a stat, as an integer percent: 110 if the nature
// raises that stat, 90 if it lowers it, 100 otherwise (incl. every neutral
// nature and always for HP). ZM_StatCalc applies it as (stat * percent) / 100.
u_int					ZM_GetNatureStatPercent(ZM_NATURE eNature, ZM_STAT eStat);
