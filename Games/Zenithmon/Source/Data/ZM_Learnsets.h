#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"

// ============================================================================
// ZM_Learnsets -- per-species level-up learnsets (S1 data core). Completes the
// ZM_SpeciesData Roadmap box (roster + base stats + learnsets); see DecisionLog
// ZM-D-023.
//
// Learnsets are systematically DERIVED, not hand-authored -- the same
// placeholder strategy as base stats (ZM-D-021), and for the same reason: real
// per-species movepools are an S11 balance concern (headless AI-vs-AI), and
// hand-authoring ~150 movepools of arbitrary placeholder quality in one commit
// buys nothing over a deterministic, type-appropriate, referentially-valid
// derivation that unblocks S2. The derivation (see the .cpp):
//   * draws only from the species' own type(s) + universal NORMAL moves,
//   * teaches a damaging same-type move at level 1 and the strongest moves last,
//   * is damaging-dominant (at most a few status moves),
//   * scales its size with evolution stage,
//   * is fully deterministic (pure function of the species + move tables).
// It is trivially superseded by a stored per-species table later -- the accessor
// signature is the stable seam.
// ============================================================================

// Upper bound on a single species' level-up list (sizes the return struct).
static const u_int uZM_MAX_LEARNSET_SIZE = 16u;

// One level-up entry: the move is learned on reaching m_uLevel.
struct ZM_LevelUpMove
{
	u_int		m_uLevel;
	ZM_MOVE_ID	m_eMove;
};

// A species' full level-up learnset, ordered by non-decreasing level.
struct ZM_Learnset
{
	u_int			m_uCount;
	ZM_LevelUpMove	m_axMoves[uZM_MAX_LEARNSET_SIZE];
};

// Derived level-up learnset for a species (bounds-asserted via ZM_GetSpeciesData).
// Deterministic; every referenced move is a real ZM_MOVE_ID.
ZM_Learnset ZM_GetSpeciesLearnset(ZM_SPECIES_ID eId);
