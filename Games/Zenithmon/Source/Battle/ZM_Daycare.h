#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"   // ZM_BattleMonsterSpec
#include "Zenithmon/Source/Battle/ZM_Breeding.h"          // ZM_BreedingParams, ZM_GenerateEgg
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"          // ZM_BattleRNG

// ============================================================================
// ZM_Daycare -- S2 box 6 SC1. Deposit up to 2 monsters; they gain EXP/levels by
// walked steps (pure integer, reusing ZM_ExpAndLevel curves); an egg becomes
// available once a compatible deposited pair accrues enough steps. Pure state
// machine over ZM_DaycareState -- no globals, no UI, no overworld. RNG enters
// ONLY when collecting the egg (which calls ZM_GenerateEgg).
// ============================================================================

static const u_int uZM_DAYCARE_CAPACITY          = 2u;     // 2 deposit slots
static const u_int uZM_DAYCARE_EXP_PER_STEP       = 1u;     // exp gained per walked step
static const u_int uZM_DAYCARE_EGG_STEP_THRESHOLD = 256u;   // compatible-pair steps -> egg ready

struct ZM_DaycareSlot
{
	bool                 m_bOccupied = false;
	ZM_BattleMonsterSpec m_xMonster  = {};   // deposited seed; level/exp mutate on step
};

// Slot 0 is the "mother" (species source) by deposit-order convention (gender is
// not modelled -- spec section 6).
struct ZM_DaycareState
{
	ZM_DaycareSlot m_axSlots[uZM_DAYCARE_CAPACITY];
	u_int          m_uEggStepCounter = 0u;    // accrues only while a compatible pair is present
	bool           m_bEggAvailable   = false; // set once the counter reaches the threshold
};

// Deposit into the first free slot. Returns the slot index [0,2), or
// uZM_DAYCARE_CAPACITY if the daycare is full. Normalizes an UNSPECIFIED cumulative
// exp to the level floor so stepping is well-defined.
u_int ZM_DaycareDeposit(ZM_DaycareState& xState, const ZM_BattleMonsterSpec& xMon);

// Withdraw the monster in uSlot into xOut (the level/exp-updated spec), clear the
// slot, and RESET egg progress (m_uEggStepCounter = 0, m_bEggAvailable = false).
// Returns false if uSlot is out of range or empty (xOut untouched).
bool  ZM_DaycareWithdraw(ZM_DaycareState& xState, u_int uSlot, ZM_BattleMonsterSpec& xOut);

// Advance uSteps. Every occupied slot gains uSteps*uZM_DAYCARE_EXP_PER_STEP exp
// (clamped to the level-100 cap) and its level is recomputed. If BOTH slots hold a
// compatible pair, the egg counter advances by uSteps (saturating at the threshold,
// where m_bEggAvailable becomes true). No RNG.
void  ZM_DaycareStep(ZM_DaycareState& xState, u_int uSteps);

// True iff both slots are occupied AND their species are compatible.
bool  ZM_DaycarePairCompatible(const ZM_DaycareState& xState);

// Number of occupied slots [0,2].
u_int ZM_DaycareOccupancy(const ZM_DaycareState& xState);

// If an egg is available, generate it (mother = slot 0, father = slot 1) into
// xEggOut, RESET the counter/flag, and return true. Otherwise return false (xEggOut
// untouched). Consumes xRng exactly as ZM_GenerateEgg does.
bool  ZM_DaycareCollectEgg(ZM_DaycareState& xState, ZM_BattleRNG& xRng,
                           const ZM_BreedingParams& xParams, ZM_BattleMonsterSpec& xEggOut);
