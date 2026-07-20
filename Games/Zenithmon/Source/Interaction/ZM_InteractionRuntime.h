#pragma once

#include "Maths/Zenith_Maths.h"                                   // Vector3 / Quat (the player's pose)
#include "ZenithECS/Zenith_Entity.h"                              // Zenith_EntityID (the latched target)
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"     // ZM_INTERACT_REJECT + the pure gate/picker

// ============================================================================
// ZM_InteractionRuntime (S6 item 3 SC4) -- the thin LIVE glue that finally calls
// SC1's world gate and SC2's candidate picker: snapshot the world, run the pure
// decision, raise the winner's screen through ZM_Interactable::Interact().
//
// It is NOT its own ECS component, deliberately. ZM_PlayerController (order 102)
// is already on the Player in every scene, so this is a by-value member of it and
// travels with it for free. A separate component would need one more
// AddStep_AddComponent per authored scene, and forgetting it in a single S9 town
// would kill interaction in that town with no test able to notice.
//
// It holds NO instance state. The latches below are process-global statics with
// instance-shaped accessors: ResetRuntimeStateForTests() is a STATIC called from
// the between-tests hook in Zenithmon.cpp (which can only reach ownerless globals
// -- it has no handle on any particular player's controller), so the latches must
// live where that hook can clear them. Being stateless also keeps the owning
// component trivially movable through the ECS pool's relocations.
// ============================================================================

// Fixed probe-array capacity. The picker is a by-value array walk, so the live
// site needs a bound; Dawnmere has FOUR NPCs, and 64 is generous enough that
// hitting it means a scene authored something pathological (which asserts + logs
// rather than silently truncating unnoticed).
inline constexpr u_int uZM_MAX_INTERACT_PROBES = 64u;

class ZM_InteractionRuntime
{
public:
	// Called ONCE PER FRAME from ZM_PlayerController::OnUpdate, between that
	// function's third and fourth early-outs -- see the comment at the call site.
	// The pose comes from the owner's Zenith_TransformComponent (interaction is pure
	// geometry: it needs the transform, never a physics body).
	//
	// Reads the interact EDGE, runs the shared decision, latches the outcome either
	// way, and on ZM_INTERACT_OK calls Interact() on the winning entity.
	void Tick(const Zenith_Maths::Vector3& xPlayerPosition,
		const Zenith_Maths::Quat& xPlayerRotation);

	// The TEST SEAM. Runs the SAME decision Tick runs -- with the input edge ASSUMED
	// pressed rather than read, so the answer describes what pressing E right now
	// WOULD do -- and neither consumes the edge nor raises anything nor touches a
	// latch. Windowed tests poll this so they can press E exactly when the interactor
	// is genuinely ready (event-driven, never frame-counted).
	//
	// It resolves the player's pose itself from the unique ZM_PlayerController in the
	// active scene; when no unique player resolves there is no origin to reason from,
	// which reports as ZM_INTERACT_REJECT_DEGENERATE_ORIGIN with an invalid target.
	ZM_INTERACT_REJECT EvaluateForTests(Zenith_EntityID& xTargetOut) const;

	// ---- Latches (process-global -- see the class comment) ----

	// The last decision Tick reached, reject or OK. Cleared to
	// ZM_INTERACT_REJECT_NO_INPUT_EDGE ("nothing has been attempted").
	ZM_INTERACT_REJECT GetLastResult() const;
	// The entity the last OK decision picked; INVALID_ENTITY_ID on every reject.
	Zenith_EntityID    GetLastTarget() const;
	// Monotonic count of screens ACTUALLY raised. Assert on this rather than on
	// GetLastResult() alone: an interactor stubbed to always say OK would satisfy the
	// result check while never raising anything.
	u_int              GetRaiseCount() const;
	// Has Tick run at all since the last reset? Distinguishes "cleared" from "ran and
	// legitimately decided NO_INPUT_EDGE", which is what makes the reset unit
	// non-vacuous (it populates, asserts populated, resets, asserts cleared).
	bool               HasLatchedResult() const;

	// Clear every latch. Called from the between-tests hook so a batched test cannot
	// inherit a previous (possibly failed) test's interaction outcome.
	static void ResetRuntimeStateForTests();

private:
	// The ONE decision path. Tick and EvaluateForTests call this and NOTHING else, so
	// the test seam can never drift from live behaviour -- if the two ever diverged,
	// every windowed test that polls the seam would be polling a lie.
	//
	// Order: the world gate first (is interacting legal AT ALL?), then the candidate
	// picker (WHICH interactable?). Both report through the same reject enum.
	// bHavePose is threaded in rather than checked by the CALLER so the world gate
	// always outranks a missing origin: EvaluateForTests has no owner transform and
	// resolves the pose from the active scene, which legitimately fails whenever no
	// unique player exists (FrontEnd, mid-warp before the spawn, the additive battle
	// scene). Checking it caller-side made the seam answer DEGENERATE_ORIGIN in all
	// three cases, hiding the honest NOT_OVERWORLD / WARP_IN_PROGRESS -- and
	// DEGENERATE_ORIGIN also means "facing straight up", so an SC5+ poller could not
	// tell a transient, expected block from a real geometry bug.
	ZM_INTERACT_REJECT Decide(bool bHavePose,
		const Zenith_Maths::Vector3& xPlayerPosition,
		const Zenith_Maths::Quat& xPlayerRotation,
		bool bInteractPressed,
		Zenith_EntityID& xTargetOut) const;

	// The unique active-scene player's pose / movement flag. Both fail OPEN (no pose
	// -> false; no unique controller -> "movement enabled"), because the REAL guard
	// against interacting while frozen is WHERE Tick is called from -- after
	// ZM_PlayerController::OnUpdate's frozen early-out. The movement bool fed to
	// ZM_ShouldInteract is belt-and-braces on top of that.
	static bool TryResolveActivePlayerPose(Zenith_Maths::Vector3& xPositionOut,
		Zenith_Maths::Quat& xRotationOut);
	static bool ResolveActivePlayerMovementEnabled();

	static ZM_INTERACT_REJECT s_eLastResult;
	static Zenith_EntityID    s_xLastTarget;
	static u_int              s_uRaiseCount;
	static bool               s_bHasLatchedResult;
};
