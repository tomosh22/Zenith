#pragma once

#include "Collections/Zenith_Vector.h"

// Forward declarations only — this header includes NO EntityComponent header.
// The store is keyed by Zenith_EntityID (taken by value in the public API); a
// by-value parameter of an INCOMPLETE type is legal in a *declaration*, so the
// header needs only the forward declaration here. The .cpp pulls the full
// EntityComponent/Zenith_Entity.h definition to read the id's slot index.
// (The layering gate scans #include edges, not forward decls — a forward
// declaration introduces no cross-layer coupling.)
struct Zenith_EntityID;

// The store lives under Flux/, so it owns the full Flux_AnimationController
// type freely (it includes Flux_AnimationController.h in its .cpp). This is the
// Flux side of the Wave-19 ownership relocation — the TWIN of WS18's
// Flux_TerrainStreamingState relocation: the heavy Flux animation controller
// that used to be a by-value member of Zenith_AnimatorComponent now lives here,
// keyed by the stable EntityID slot, and the component is a thin forwarding
// handle.
class Flux_AnimationController;

//=============================================================================
// Flux_AnimationControllerStore
//
// HEAP-STABLE owning store of one Flux_AnimationController per entity. Held by
// Zenith_Engine (m_pxAnimationControllers), reached via
// g_xEngine.AnimationControllers().
//
// Storage model (heap-stable, by design):
//   * m_xControllers — a dense Zenith_Vector<Flux_AnimationController*>; each
//     controller is individually heap-allocated with `new` / freed with
//     `delete`. The POINTER is stable: growing/compacting the vector never
//     moves the pointed-to controller object. This is the load-bearing
//     guarantee — a cached Flux_AnimationController* on the component (and the
//     games' cached Flux_AnimationLayer* / Flux_AnimationStateMachine* into the
//     controller's sub-objects) NEVER dangles across a component-pool
//     relocation or a cross-scene MoveEntityToScene.
//   * m_xSlotToController — index-by-entity-SLOT array (Zenith_Vector<u_int>),
//     grown lazily to cover the largest slot index seen, entries default to
//     uINVALID. m_xSlotToController.Get(slot) is the index into m_xControllers,
//     giving O(1) lookup keyed by EntityID slot with NO hash.
//
// Lifetime: created in Zenith_Engine::Initialise (alongside the Flux mesh
// subsystems) and deleted in Zenith_Engine::Shutdown. Controllers hold no GPU
// resources (the unified compute-skinning path reads their CPU skinning matrices),
// so there is no device-lifetime ordering constraint.
//=============================================================================
class Flux_AnimationControllerStore
{
public:
	Flux_AnimationControllerStore() = default;
	~Flux_AnimationControllerStore();

	Flux_AnimationControllerStore(const Flux_AnimationControllerStore&) = delete;
	Flux_AnimationControllerStore& operator=(const Flux_AnimationControllerStore&) = delete;
	Flux_AnimationControllerStore(Flux_AnimationControllerStore&&) = delete;
	Flux_AnimationControllerStore& operator=(Flux_AnimationControllerStore&&) = delete;

	// Return the controller for xID, allocating one on first request. The
	// returned reference is stable for the controller's lifetime (until
	// Destroy(xID)) regardless of subsequent store mutations.
	Flux_AnimationController& GetOrCreate(Zenith_EntityID xID);

	// Pointer-or-null lookup. No allocation. Returns nullptr if no controller
	// exists for xID.
	Flux_AnimationController* TryGet(Zenith_EntityID xID);
	const Flux_AnimationController* TryGet(Zenith_EntityID xID) const;

	// Asserts the controller is present, then returns it. Hot-path forwarders
	// on the component go through this.
	Flux_AnimationController& Get(Zenith_EntityID xID);
	const Flux_AnimationController& Get(Zenith_EntityID xID) const;

	// IDEMPOTENT: deletes the controller for xID and frees its slot mapping.
	// A no-op when no controller exists (so the component's dtor AND OnDestroy
	// can both call it, and a moved-from component can call it, without a
	// double-free). Returns true iff a controller was actually destroyed.
	bool Destroy(Zenith_EntityID xID);

	// Live controller count (test / diagnostic helper).
	u_int GetCount() const { return m_xControllers.GetSize(); }

private:
	// Sentinel for "no controller for this slot".
	static constexpr u_int uINVALID = 0xFFFFFFFFu;

	// Resolve a slot index to an index into m_xControllers, or uINVALID.
	u_int SlotToControllerIndex(u_int uSlot) const;

	// Grow m_xSlotToController so uSlot is a valid index, padding with uINVALID.
	void EnsureSlotCapacity(u_int uSlot);

	// Free + swap-and-pop the controller at dense index uControllerIndex (owning
	// entity slot uSlot) and repair the relocated element's slot mapping. Shared
	// by Destroy() and the GetOrCreate() stale-slot recovery. Generation
	// validation is the CALLER's responsibility.
	void DestroyControllerAt(u_int uControllerIndex, u_int uSlot);

	// Dense array of heap-allocated controllers (pointers are stable).
	Zenith_Vector<Flux_AnimationController*> m_xControllers;

	// Parallel dense array: for each live controller in m_xControllers, the
	// entity SLOT index it belongs to. Used to repair m_xSlotToController after
	// a swap-and-pop removal (the moved element's slot entry must be repointed
	// at its new index).
	Zenith_Vector<u_int> m_xControllerSlots;

	// Parallel dense array: the entity GENERATION each live controller belongs
	// to. The slot index alone is NOT a stable identity — slots are recycled, and
	// Zenith_EntityID carries a generation precisely for stale-handle detection.
	// Every entry point validates index AND generation so a stale id for a reused
	// slot can never resolve to, or destroy, the new occupant's controller.
	Zenith_Vector<u_int> m_xControllerGenerations;

	// Index-by-entity-SLOT: slot -> index into m_xControllers (or uINVALID).
	Zenith_Vector<u_int> m_xSlotToController;
};
