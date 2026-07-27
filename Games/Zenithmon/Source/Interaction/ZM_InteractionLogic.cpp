#include "Zenith.h"

#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"

#include <cmath>   // std::sqrt / std::fabs

// ============================================================================
// ZM_InteractionLogic (S6 item 3 SC1 + SC2). See the header for the contract;
// this file is the ORDER. Nothing here touches the ECS, the scene, the UI or the
// disk.
// ============================================================================

ZM_INTERACT_REJECT ZM_ShouldInteract(bool bPressed, bool bMenuOpen, bool bOverworld,
	bool bWarpInProgress, bool bBattleTransitionActive,
	bool bPlayerMovementEnabled)
{
	// The sequence IS the specification: the first blocker that holds is the one
	// reported, so each early-out below must stay exactly where it is.
	if (!bPressed)                  { return ZM_INTERACT_REJECT_NO_INPUT_EDGE; }
	if (bMenuOpen)                  { return ZM_INTERACT_REJECT_MENU_OPEN; }
	if (!bOverworld)                { return ZM_INTERACT_REJECT_NOT_OVERWORLD; }
	if (bWarpInProgress)            { return ZM_INTERACT_REJECT_WARP_IN_PROGRESS; }
	if (bBattleTransitionActive)    { return ZM_INTERACT_REJECT_BATTLE_TRANSITION; }
	if (!bPlayerMovementEnabled)    { return ZM_INTERACT_REJECT_PLAYER_FROZEN; }
	return ZM_INTERACT_OK;
}

const char* ZM_InteractRejectName(ZM_INTERACT_REJECT eReject)
{
	switch (eReject)
	{
	case ZM_INTERACT_OK:                          return "OK";
	case ZM_INTERACT_REJECT_NO_INPUT_EDGE:        return "NO_INPUT_EDGE";
	case ZM_INTERACT_REJECT_MENU_OPEN:            return "MENU_OPEN";
	case ZM_INTERACT_REJECT_NOT_OVERWORLD:        return "NOT_OVERWORLD";
	case ZM_INTERACT_REJECT_WARP_IN_PROGRESS:     return "WARP_IN_PROGRESS";
	case ZM_INTERACT_REJECT_BATTLE_TRANSITION:    return "BATTLE_TRANSITION";
	case ZM_INTERACT_REJECT_PLAYER_FROZEN:        return "PLAYER_FROZEN";
	case ZM_INTERACT_REJECT_NO_CANDIDATE:         return "NO_CANDIDATE";
	case ZM_INTERACT_REJECT_OUT_OF_RANGE:         return "OUT_OF_RANGE";
	case ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND: return "OUT_OF_VERTICAL_BAND";
	case ZM_INTERACT_REJECT_NOT_FACING:           return "NOT_FACING";
	case ZM_INTERACT_REJECT_DEGENERATE_ORIGIN:    return "DEGENERATE_ORIGIN";
	// A switch, not a table lookup, precisely so COUNT and anything past it land
	// here instead of reading off the end of an array.
	default:                                      return "UNKNOWN";
	}
}

// ---- The shared XZ cone primitives (SC3) ------------------------------------
//
// These two were unnamed internals of this file until SC3. They now live at
// EXTERNAL linkage (declared in the header) because the trainer sight predicate
// must reuse the SAME cone rather than restate it. The unnamed-namespace copies
// were DELETED rather than left in place: with the header's declarations visible
// here, keeping them would make unqualified lookup ambiguous, not shadowed.

Zenith_Maths::Vector3 ZM_FlattenXZ(const Zenith_Maths::Vector3& xVector)
{
	const float fLengthSquared = (xVector.x * xVector.x) + (xVector.z * xVector.z);
	if (fLengthSquared <= fZM_INTERACT_DEGENERATE_LEN_SQ)
	{
		return Zenith_Maths::Vector3(0.0f);
	}
	const float fInverseLength = 1.0f / std::sqrt(fLengthSquared);
	return Zenith_Maths::Vector3(xVector.x * fInverseLength, 0.0f, xVector.z * fInverseLength);
}

bool ZM_IsFacingXZ(const Zenith_Maths::Vector3& xObserverPosition,
	const Zenith_Maths::Vector3& xFlatUnitForward,
	const Zenith_Maths::Vector3& xTargetPosition,
	float fMinFacingDot)
{
	// No facing at all -> nothing is faced. FAIL CLOSED. ZM_PickInteractTarget can
	// never reach this line (it returns DEGENERATE_ORIGIN before the walk), so the
	// guard is behaviour-neutral there and load-bearing for every other caller.
	const float fForwardLengthSquared =
		(xFlatUnitForward.x * xFlatUnitForward.x) + (xFlatUnitForward.z * xFlatUnitForward.z);
	if (fForwardLengthSquared <= fZM_INTERACT_DEGENERATE_LEN_SQ)
	{
		return false;
	}

	const float fDeltaX = xTargetPosition.x - xObserverPosition.x;
	const float fDeltaZ = xTargetPosition.z - xObserverPosition.z;
	const float fDistanceSquared = (fDeltaX * fDeltaX) + (fDeltaZ * fDeltaZ);

	// POLARITY IS `>`, NOT `<=`, and that is not a style choice: this is verbatim
	// the branch lifted out of the picker's loop, so a non-finite separation falls
	// into the coincident arm here exactly as it did there. That answer -- TRUE for
	// a NaN separation -- is pinned by Facing_NonFiniteSeparationIsTreatedAsCoincident,
	// so any rewrite of this branch has to keep that unit green. Reason about
	// respellings from what the suite actually says rather than from first
	// principles: the sibling `>=` / `!(<)` respelling below LOOKS like it must move
	// the non-finite answer, and was measured not to (see the note there).
	if (fDistanceSquared > fZM_INTERACT_DEGENERATE_LEN_SQ)
	{
		const float fDistance = std::sqrt(fDistanceSquared);
		const float fFacingDot =
			((xFlatUnitForward.x * fDeltaX) + (xFlatUnitForward.z * fDeltaZ)) / fDistance;
		// INCLUSIVE at the cone edge, and spelled `>=` because that is the direct
		// statement of the contract: "faced" is what must be PROVEN, so anything
		// the comparison cannot prove -- a non-finite dot included, whether it came
		// from the forward or from fMinFacingDot -- answers false and fails CLOSED.
		// That fail-closed answer is pinned by Facing_NonFiniteOperandFailsClosed,
		// at this surface and through ZM_PickInteractTarget.
		// The spelling is NOT a behaviour change out of the pre-SC3 fused block,
		// which wrote `if (fFacingDot < fMinFacingDot) { continue; }`. That was
		// checked, not assumed: this line was mutated to
		// `return !(fFacingDot < fMinFacingDot)`, the Null config rebuilt from
		// scratch and the whole boot unit gate run -- the suite stayed green,
		// identical counts, nothing redded, on a battery whose sibling mutations
		// elsewhere DID red. No input the suite exercises, non-finite included,
		// tells the two forms apart. Keep `>=` for clarity, and do not expect a
		// respelling here to be caught by a test.
		return fFacingDot >= fMinFacingDot;
	}
	// A COINCIDENT target has no direction to test, so it is defined to be faced
	// rather than divided by zero into a NaN.
	return true;
}

// ---- The candidate picker (SC2) ---------------------------------------------

namespace
{
	// How far the BEST near-miss got. Tracked as a single high-water mark during the
	// one walk (rather than by re-scanning the array once per test) so the reported
	// reason is always the furthest any probe reached, never the last one seen.
	// ORDER IS THE MEANING: each value must stay strictly weaker than the next.
	enum ZM_INTERACT_STAGE : u_int
	{
		ZM_INTERACT_STAGE_NONE = 0u,   // not one ENABLED probe was seen
		ZM_INTERACT_STAGE_ENABLED,     // an enabled probe existed...
		ZM_INTERACT_STAGE_IN_RANGE,    // ...and one passed the distance test...
		ZM_INTERACT_STAGE_IN_BAND,     // ...and one passed the vertical band...
		ZM_INTERACT_STAGE_FACED        // ...and one passed the cone (a survivor)
	};
}

ZM_INTERACT_REJECT ZM_PickInteractTarget(const ZM_InteractProbe* paxProbes, u_int uCount,
	const ZM_InteractOrigin& xOrigin,
	const ZM_InteractTuning& xTuning,
	u_int& uBestIndexOut)
{
	// uCount doubles as the "no winner" sentinel: it is an unreachable index, so a
	// caller that ignores the return value cannot address probe 0 by accident. It is
	// written FIRST so every early-out below inherits it.
	uBestIndexOut = uCount;

	// Checked before the walk: with no facing direction the cone test is meaningless,
	// so no probe could be judged even if the array were full of perfect candidates.
	const Zenith_Maths::Vector3 xForward = ZM_FlattenXZ(xOrigin.m_xForward);
	if (((xForward.x * xForward.x) + (xForward.z * xForward.z)) <= fZM_INTERACT_DEGENERATE_LEN_SQ)
	{
		return ZM_INTERACT_REJECT_DEGENERATE_ORIGIN;
	}

	if (paxProbes == nullptr)
	{
		return ZM_INTERACT_REJECT_NO_CANDIDATE;
	}

	ZM_INTERACT_STAGE eBestStage = ZM_INTERACT_STAGE_NONE;
	float fBestDistanceSquared = 0.0f;

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_InteractProbe& xProbe = paxProbes[u];
		if (!xProbe.m_bEnabled)
		{
			// A disabled probe raises no stage either: it must not make a scene full
			// of parked NPCs report OUT_OF_RANGE when the honest answer is NO_CANDIDATE.
			continue;
		}
		if (eBestStage < ZM_INTERACT_STAGE_ENABLED)
		{
			eBestStage = ZM_INTERACT_STAGE_ENABLED;
		}

		// 2. XZ distance, Y deliberately dropped -- a sunk or floating NPC must stay
		//    reachable, and the height difference is policed by the band test below.
		const float fDeltaX = xProbe.m_xPosition.x - xOrigin.m_xPosition.x;
		const float fDeltaZ = xProbe.m_xPosition.z - xOrigin.m_xPosition.z;
		const float fDistanceSquared = (fDeltaX * fDeltaX) + (fDeltaZ * fDeltaZ);

		const float fReach = xTuning.m_fMaxDistance + xProbe.m_fRadius;
		if (fReach < 0.0f)
		{
			// A negative reach never squares its way back into "everything is in range".
			continue;
		}
		if (fDistanceSquared > (fReach * fReach))
		{
			continue;
		}
		if (eBestStage < ZM_INTERACT_STAGE_IN_RANGE)
		{
			eBestStage = ZM_INTERACT_STAGE_IN_RANGE;
		}

		// 3. Vertical band, inclusive: this is what stops talking through a floor.
		const float fDeltaY = xProbe.m_xPosition.y - xOrigin.m_xPosition.y;
		if (std::fabs(fDeltaY) > xTuning.m_fMaxVertical)
		{
			continue;
		}
		if (eBestStage < ZM_INTERACT_STAGE_IN_BAND)
		{
			eBestStage = ZM_INTERACT_STAGE_IN_BAND;
		}

		// 4. Facing cone, inclusive -- the ONE angular test, now shared with the
		//    trainer sight predicate. Same operands, same order, same result: the
		//    helper recomputes fDeltaX / fDeltaZ / fDistanceSquared from the same
		//    two positions with the same expression, so no value differs by a bit.
		//    A COINCIDENT probe still has no direction to test and still counts as
		//    faced -- that carve-out moved INTO the helper unchanged.
		if (!ZM_IsFacingXZ(xOrigin.m_xPosition, xForward, xProbe.m_xPosition, xTuning.m_fMinFacingDot))
		{
			continue;
		}

		// A survivor. STRICTLY closer to displace the incumbent, which is what makes a
		// tie break to the lowest index and the result independent of array order.
		if ((eBestStage < ZM_INTERACT_STAGE_FACED) || (fDistanceSquared < fBestDistanceSquared))
		{
			uBestIndexOut = u;
			fBestDistanceSquared = fDistanceSquared;
		}
		eBestStage = ZM_INTERACT_STAGE_FACED;
	}

	if (uBestIndexOut != uCount)
	{
		return ZM_INTERACT_OK;
	}

	// Most-specific-LAST: report how far the best near-miss got.
	switch (eBestStage)
	{
	case ZM_INTERACT_STAGE_IN_RANGE:  return ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND;
	case ZM_INTERACT_STAGE_IN_BAND:   return ZM_INTERACT_REJECT_NOT_FACING;
	case ZM_INTERACT_STAGE_ENABLED:   return ZM_INTERACT_REJECT_OUT_OF_RANGE;
	// ZM_INTERACT_STAGE_FACED cannot reach here: a faced survivor always sets a
	// winner index, so the OK return above would have fired.
	default:                          return ZM_INTERACT_REJECT_NO_CANDIDATE;
	}
}

Zenith_Maths::Vector3 ZM_ForwardFromRotation(const Zenith_Maths::Quat& xRotation)
{
	// Rotate the +Z basis vector. NEVER glm::eulerAngles(xRotation).y -- see the
	// header: that decomposition collapses past 90 degrees off +Z.
	return ZM_FlattenXZ(xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
}
