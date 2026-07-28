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
	case ZM_TRAINER_SIGHT_WATCHING: return "WATCHING";
	case ZM_TRAINER_SIGHT_ENGAGED:  return "ENGAGED";
	// A switch, not a table lookup, precisely so ZM_TRAINER_SIGHT_STATE_COUNT and
	// anything past it land here instead of reading off the end of an array.
	default:                        return "UNKNOWN";
	}
}

const char* ZM_TrainerSightActionName(ZM_TRAINER_SIGHT_ACTION eAction)
{
	switch (eAction)
	{
	case ZM_TRAINER_SIGHT_ACTION_NONE:            return "NONE";
	case ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER: return "RAISE_ENCOUNTER";
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
		m_eState = ZM_TRAINER_SIGHT_ENGAGED;
		m_fConfirmElapsed = 0.0f;
		m_bRaiseConfirmed = false;
		++m_uRaiseCount;
		return ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER;

	case ZM_TRAINER_SIGHT_ENGAGED:
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
