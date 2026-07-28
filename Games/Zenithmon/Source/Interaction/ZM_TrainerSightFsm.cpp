#include "Zenith.h"

#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"

#include <cmath>   // std::isfinite (the degenerate-dt guard)

// ============================================================================
// ZM_TrainerSightFsm (S7 item 3 SC6). See the header for the contract; this file
// is the ORDER. Nothing here touches the ECS, the scene, the physics world, the
// UI or the disk, and NOTHING HERE MAY Zenith_Assert ON ITS ARGUMENTS -- the boot
// units feed these functions NaN deltas, negative confirm windows and the
// ZM_TRAINER_NONE sentinel on purpose to pin the fail-closed answers.
// ============================================================================

u_int ZM_TrainerEngagementLatch::s_uEngagedMask = 0u;

void ZM_TrainerEngagementLatch::MarkEngaged(ZM_TRAINER_ID eTrainer)
{
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		return;
	}
	s_uEngagedMask |= (1u << (u_int)eTrainer);
}

bool ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_ID eTrainer)
{
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		return false;
	}
	return (s_uEngagedMask & (1u << (u_int)eTrainer)) != 0u;
}

void ZM_TrainerEngagementLatch::ResetRuntimeStateForTests()
{
	s_uEngagedMask = 0u;
}

u_int ZM_TrainerEngagementLatch::GetEngagedMaskForTests()
{
	return s_uEngagedMask;
}

const char* ZM_TrainerSightStateName(ZM_TRAINER_SIGHT_STATE eState)
{
	switch (eState)
	{
	case ZM_TRAINER_SIGHT_WATCHING:    return "WATCHING";
	case ZM_TRAINER_SIGHT_ENGAGED:     return "ENGAGED";
	case ZM_TRAINER_SIGHT_CHALLENGING: return "CHALLENGING";
	// A switch, not a table lookup, precisely so ZM_TRAINER_SIGHT_STATE_COUNT and
	// anything past it land here instead of reading off the end of an array.
	default:                           return "UNKNOWN";
	}
}

const char* ZM_TrainerSightActionName(ZM_TRAINER_SIGHT_ACTION eAction)
{
	switch (eAction)
	{
	case ZM_TRAINER_SIGHT_ACTION_NONE:            return "NONE";
	case ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER: return "RAISE_ENCOUNTER";
	case ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE:   return "RUN_CHALLENGE";
	default:                                      return "UNKNOWN";
	}
}

bool ZM_MayTrainerEngage(const ZM_TrainerData& xRow,
	bool bDefeatFlagSet,
	bool bSessionLatchSet)
{
	// The UNKNOWN row ZM_GetTrainerData hands back for a bad id carries
	// ZM_TRAINER_NONE, so this one comparison rejects it and every hand-built
	// garbage row together. Fail CLOSED: an unauthored trainer never battles.
	if (!ZM_IsRegisteredTrainer(xRow.m_eId))
	{
		return false;
	}
	// Spelled inline rather than via ZM_StoryFlags' registered-flag helper, which
	// is file-local to ZM_StoryFlags.cpp. ZM_STORY_FLAG_NONE aliases
	// ZM_STORY_FLAG_COUNT, so this rejects the sentinel and garbage together.
	if ((u_int)xRow.m_eDefeatFlag < (u_int)ZM_STORY_FLAG_COUNT)
	{
		return !bDefeatFlagSet;
	}
	return !bSessionLatchSet;
}

void ZM_TrainerSightFsm::Reset()
{
	m_eState = ZM_TRAINER_SIGHT_WATCHING;
	m_fConfirmElapsed = 0.0f;
	m_bRaiseConfirmed = false;
	m_uRaiseCount = 0u;
	m_fChallengeElapsed = 0.0f;
	m_bChallengeAccepted = false;
	m_uChallengeCount = 0u;
}

ZM_TRAINER_SIGHT_ACTION ZM_TrainerSightFsm::Step(const ZM_TrainerSightInputs& xInputs,
	const ZM_TrainerSightFsmTuning& xTuning)
{
	// SIGHT is the CONJUNCTION: inside the pure cone AND with an unblocked line.
	// This single expression is where occlusion enters the decision.
	const bool bSees = xInputs.m_bTargetInSight && xInputs.m_bSightLineClear;

	switch (m_eState)
	{
	case ZM_TRAINER_SIGHT_WATCHING:
		if (!bSees)
		{
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (!xInputs.m_bMayEngage)
		{
			// Already defeated (flagged row) or already battled this session
			// (flagless row). Stay WATCHING: the gate is re-read every tick, so a
			// new game or a test reset re-arms without any extra transition.
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (xInputs.m_bChannelBusy)
		{
			// DEFER, do not consume. Raising into a busy channel is silently
			// dropped, and the trainer would then sit ENGAGED having achieved
			// nothing.
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (xInputs.m_bChallengeAvailable)
		{
			// S7 item 3 SC7: this trainer has lines, so the BEAT runs first. The
			// engagement latch is deliberately NOT burnt here -- see the CHALLENGING
			// abandon arm below.
			m_eState = ZM_TRAINER_SIGHT_CHALLENGING;
			m_fChallengeElapsed = 0.0f;
			m_bChallengeAccepted = false;
			++m_uChallengeCount;
			return ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE;
		}
		// A silent trainer is SC6 verbatim: straight to the encounter, no window.
		m_eState = ZM_TRAINER_SIGHT_ENGAGED;
		m_fConfirmElapsed = 0.0f;
		m_bRaiseConfirmed = false;
		++m_uRaiseCount;
		return ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER;

	case ZM_TRAINER_SIGHT_CHALLENGING:
		if (xInputs.m_bChannelBusy)
		{
			// The bark TOOK: pushing a ZM_UI_MenuStack dialogue is precisely what
			// makes IsMenuOpen() -- and therefore this input -- true. Latched, because
			// the channel goes idle again when the box closes and we must read THAT
			// as "the conversation ended", not as "nothing ever happened".
			m_bChallengeAccepted = true;
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (m_bChallengeAccepted)
		{
			// The box has closed. ★ ORDER 112 (ZM_UI_MenuStack) < 113
			// (ZM_Interactable) IS LOAD-BEARING: MenuStack has ALREADY popped, closed
			// and run UnfreezePlayer earlier in THIS frame, so the encounter we raise
			// on the next line lands after the menu freeze was released and before
			// anything else can claim the channel. There is exactly one freeze owner
			// at every instant, and SC7 introduced neither of them.
			m_eState = ZM_TRAINER_SIGHT_ENGAGED;
			m_fConfirmElapsed = 0.0f;
			m_bRaiseConfirmed = false;
			++m_uRaiseCount;
			return ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER;
		}
		if (!bSees)
		{
			// Reachable ONLY when the bark never landed -- a bark that landed FROZE
			// the player, so he cannot walk out of the cone mid-conversation. Abandon
			// WITHOUT raising and WITHOUT burning the engagement latch, so a flagless
			// trainer does not lose his one session shot to a bark nobody heard.
			// (A WANDERING trainer could still turn away mid-bark; SC8's Vesper is
			// stationary, and the outcome there is a re-bark, not a lost battle.)
			m_eState = ZM_TRAINER_SIGHT_WATCHING;
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		// A non-finite or non-positive dt contributes NOTHING (the SC6 rule), so the
		// accumulator can never go NaN.
		if (std::isfinite(xInputs.m_fDeltaSeconds) && xInputs.m_fDeltaSeconds > 0.0f)
		{
			m_fChallengeElapsed += xInputs.m_fDeltaSeconds;
		}
		// ★ FAIL OPEN, and note the polarity is INVERTED relative to the ENGAGED arm
		// below: a degenerate window here does not disable the transition, it takes
		// it IMMEDIATELY. Silence in the raise window costs a spurious battle;
		// silence HERE would cost the battle altogether.
		if (!(std::isfinite(xTuning.m_fChallengeConfirmSeconds)
				&& xTuning.m_fChallengeConfirmSeconds > 0.0f)
			|| m_fChallengeElapsed >= xTuning.m_fChallengeConfirmSeconds)
		{
			m_eState = ZM_TRAINER_SIGHT_ENGAGED;
			m_fConfirmElapsed = 0.0f;
			m_bRaiseConfirmed = false;
			++m_uRaiseCount;
			return ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER;
		}
		return ZM_TRAINER_SIGHT_ACTION_NONE;

	case ZM_TRAINER_SIGHT_ENGAGED:
		// ---- UNCHANGED FROM SC6, byte for byte. ----
		if (xInputs.m_bChannelBusy)
		{
			// The screen went busy after our raise: it TOOK. Latched, because the
			// channel goes idle again at the end of the round trip and we must not
			// then read that idleness as "the raise was dropped".
			m_bRaiseConfirmed = true;
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (!bSees)
		{
			// The target left the cone (or stepped behind cover): re-arm. This is
			// the rising-edge discipline ZM_WarpTrigger's overlap latch and
			// ZM_TallGrassSystem's tile transition already use -- one raise per
			// continuous spotting, never one per frame.
			m_eState = ZM_TRAINER_SIGHT_WATCHING;
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (!m_bRaiseConfirmed)
		{
			// A non-finite or non-positive dt contributes NOTHING, so the
			// accumulator can never go NaN and the re-arm can never fire on a
			// garbage frame.
			if (std::isfinite(xInputs.m_fDeltaSeconds) && xInputs.m_fDeltaSeconds > 0.0f)
			{
				m_fConfirmElapsed += xInputs.m_fDeltaSeconds;
			}
			// A degenerate window disables the re-arm entirely (fail closed).
			if (std::isfinite(xTuning.m_fRaiseConfirmSeconds)
				&& xTuning.m_fRaiseConfirmSeconds > 0.0f
				&& m_fConfirmElapsed >= xTuning.m_fRaiseConfirmSeconds)
			{
				m_eState = ZM_TRAINER_SIGHT_WATCHING;
			}
		}
		return ZM_TRAINER_SIGHT_ACTION_NONE;

	default:
		// Unreachable for a value this class can hold; fail closed rather than
		// assert (this runs inside the boot unit suite).
		m_eState = ZM_TRAINER_SIGHT_WATCHING;
		return ZM_TRAINER_SIGHT_ACTION_NONE;
	}
}
