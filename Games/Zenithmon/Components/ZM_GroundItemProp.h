#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/World/ZM_GroundItem.h"   // ZM_GROUND_ITEM_ID + the pure pickup path

class Zenith_DataStream;

// ============================================================================
// ZM_GroundItemProp (S8, ZM-27 follow-up (a) of ZM-D-201) -- the ECS component
// that turns "the player pressed E at a prop" into a call to the pure pickup
// path in Source/World/ZM_GroundItem.h.
//
// ★ THIS COMPONENT OWNS NO RULE. Every accept/reject decision belongs to
// ZM_CanPickUpGroundItem, every mutation to ZM_TryPickUpGroundItem, and every
// byte of persistence to save module 12. What lives here is the ECS-shaped
// wrapper those pure functions deliberately do not have: an authored id, a reach
// bonus, and the raise. A second copy of "is there room" or "was it taken" in
// this file could drift from the functions that actually decide, and the drift
// would surface as a prop that burns itself and delivers nothing.
//
// ★ WHY THIS IS NOT A ZM_Interactable ROLE. ZM_Interactable dispatches on
// ZM_NpcData's ZM_NPC_ROLE, so every interactable it serves must first be an NPC
// with a roster row, a display name and dialogue lines. A ground item is a prop,
// not a person: giving it an NPC id would put three non-people into a roster
// whose ids are save-stable and whose rows are asserted to be content-complete
// (Tests/ZM_Tests_NpcData.cpp). The two components sit side by side and
// ZM_InteractionRuntime gathers probes from BOTH.
//
// ★ A COLLECTED PROP GOES INERT, IT DOES NOT DISAPPEAR. IsInteractable() drops
// to false the moment the save records the prop as taken, so the picker stops
// offering it and pressing E at it does nothing -- the same shape as an NPC whose
// component was never configured. Removing the entity would need a scene mutation
// on a load-bearing committed asset; going inert needs nothing and survives the
// save/load round trip for free, because the ANSWER is read from the save rather
// than latched here.
// ============================================================================

class ZM_GroundItemProp
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;

	// Reach BONUS added to fZM_INTERACT_MAX_DISTANCE by the picker, in exactly the
	// units and with exactly the clamp ZM_Interactable::fDEFAULT_RADIUS uses. A prop
	// lying on the ground is addressed from standing height, so the default is zero
	// and an author who wants more must say so.
	static constexpr float fDEFAULT_RADIUS = 0.0f;
	// The same upper bound ZM_Interactable enforces, and for the same reason: one
	// mis-authored huge radius would let a single prop swallow every interact press
	// on the route.
	static constexpr float fMAX_RADIUS = 8.0f;

	ZM_GroundItemProp() = delete;
	explicit ZM_GroundItemProp(Zenith_Entity& xParentEntity);

	// Component pools relocate their elements (move-construct + destruct the
	// source), so moves must exist; copies are deleted. Every member is a POD or a
	// movable handle, so defaulted moves are correct.
	ZM_GroundItemProp(const ZM_GroundItemProp&) = delete;
	ZM_GroundItemProp& operator=(const ZM_GroundItemProp&) = delete;
	ZM_GroundItemProp(ZM_GroundItemProp&&) noexcept = default;
	ZM_GroundItemProp& operator=(ZM_GroundItemProp&&) noexcept = default;

	void OnStart();

	ZM_GROUND_ITEM_ID GetGroundItemId() const { return m_eGroundItemId; }
	// Rejects an out-of-range id by storing ZM_GROUND_ITEM_NONE (which makes the
	// component non-interactable) rather than keeping a stale row -- the
	// ZM_Interactable::SetNpcId contract, one for one. Returns whether it took.
	bool SetGroundItemId(ZM_GROUND_ITEM_ID eId);

	float GetRadius() const { return m_fRadius; }
	// Clamps into [0, fMAX_RADIUS]; a non-finite value resets to fDEFAULT_RADIUS.
	// Returns whether the requested value was taken verbatim.
	bool SetRadius(float fRadius);

	// ---- the live candidacy answer, which feeds ZM_InteractProbe::m_bEnabled ----

	// Has THIS SAVE already taken this prop? Reads the live ZM_GameState and
	// nothing local, so it is correct across a save/load round trip with no state
	// of its own to restore. **NO REACHABLE GAME STATE MEANS NOT COLLECTED** --
	// the identical "NOTHING HAS HAPPENED YET" ruling ZM_Interactable::Interact()
	// makes for story gates, and the answer a fresh save would give.
	bool IsCollected() const;

	// A registered prop that this save has not taken. The conjunction is deliberate:
	// an entity carrying an unconfigured component must not absorb the interact
	// press and leave the player standing in front of a mute object.
	bool IsInteractable() const
	{
		return m_eGroundItemId < ZM_GROUND_ITEM_COUNT && !IsCollected();
	}

	// Take the prop. Delegates the whole rule to ZM_TryPickUpGroundItem and raises a
	// dialogue line naming what was found.
	//
	// ★ RETURNS WHETHER A SCREEN WENT UP, not whether the pickup succeeded -- the
	// same contract ZM_Interactable::Interact() answers on, because its ONE caller
	// (ZM_InteractionRuntime) increments a raise count with it. The two can differ:
	// a successful pickup whose dialogue was refused by a full queue returns false
	// while the item IS in the bag. GetLastPickupResult() below is what a test
	// asserting on the pickup itself must read.
	bool Interact();

	// ---- test/tools observation, runtime-only and never serialized ----

	// MONOTONIC count of Interact() calls that reached the pickup path -- i.e. that
	// found a live game state. A test asserting "the prop was taken exactly once"
	// needs this as well as the bag, because a second press on a collected prop is
	// a legitimate no-op that leaves the bag unchanged either way.
	u_int GetPickupAttemptCount() const { return m_uPickupAttemptCount; }
	// What the last attempt actually answered. ZM_GROUND_ITEM_PICKUP_COUNT until one
	// has run -- deliberately a value the pure enum never returns, so "never tried"
	// cannot be confused with any real outcome.
	ZM_GROUND_ITEM_PICKUP GetLastPickupResult() const { return m_eLastPickupResult; }

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	Zenith_Entity m_xParentEntity;

	// AUTHORED and SERIALIZED. The sentinel is the default so an entity that gains
	// the component in the editor and is saved before being configured comes back as
	// what it is -- an unconfigured prop -- rather than silently as prop 0.
	ZM_GROUND_ITEM_ID m_eGroundItemId = ZM_GROUND_ITEM_NONE;
	float             m_fRadius       = fDEFAULT_RADIUS;

	// RUNTIME-ONLY. Neither is serialized and uSERIALIZATION_VERSION does not cover
	// them: they describe what this INSTANCE did since the scene loaded, which a
	// stream read does not undo and a saved scene must not carry.
	u_int                 m_uPickupAttemptCount = 0u;
	ZM_GROUND_ITEM_PICKUP m_eLastPickupResult   = ZM_GROUND_ITEM_PICKUP_COUNT;
};
