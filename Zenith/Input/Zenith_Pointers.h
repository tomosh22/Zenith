#pragma once

#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Zenith_Pointers — the pointer table (B7).
//
// One slot per simultaneous pointer. A pointer is a finger on a touchscreen or,
// on a pointing-device platform, the mouse: B3 (PERMANENT) says the mouse feeds
// pointer 0 and the FIRST touch feeds the mouse view, so the two are the same
// thing at the primary slot and diverge only for secondary fingers.
//
// FRAME CONTRACT (Zenith_Core owns the call sites; see Zenith_Input.h for the
// device half):
//   4. ApplyPlatform()  — retire last frame's finished pointers, clear the
//      per-frame edges, then consume the drain's staged touch stream (barriers
//      included) plus the mouse projection.
//   7. ApplyInjection() — the automated-test Steps ran between 4 and 7, so
//      whatever they injected has been appended to the staged stream and the
//      simulator's key edges since. Consume the NEW tail; do NOT re-run the
//      frame reset.
//
// COORDINATES ARE RAW SURFACE PIXELS. Canvas / editor-viewport remapping is a
// UI-layer concern and lives there (Zenith_UIElement::TransformSurfacePosition),
// because only the UI knows which canvas a hit test belongs to.
//
// This is a PLAIN INSTANCE CLASS. The engine owns one (g_xEngine.Pointers());
// unit tests construct LOCAL instances and drive the window-free cores, so they
// never collide with the global.
// ============================================================================

// Claim owner token. Every claiming widget passes a value unique among LIVE
// claimers — the UI layer uses the widget's own address, which is unique for
// exactly as long as a claim can outlive it (the widget releases in its dtor).
typedef u_int64 Zenith_PointerOwner;
static constexpr Zenith_PointerOwner ulPOINTER_OWNER_NONE = 0;

// (slot, generation) handle. The generation is what makes a stale handle
// detectable: a slot recycles the frame AFTER its pointer ended, so a claimer
// holding last gesture's handle cannot release this gesture's claim.
struct Zenith_PointerHandle
{
	static constexpr u_int32 uINVALID_SLOT = 0xFFFFFFFFu;

	u_int32 m_uSlot       = uINVALID_SLOT;
	u_int32 m_uGeneration = 0;

	bool IsValid() const { return m_uSlot != uINVALID_SLOT; }
	bool operator==(const Zenith_PointerHandle& xOther) const
	{
		return m_uSlot == xOther.m_uSlot && m_uGeneration == xOther.m_uGeneration;
	}
	bool operator!=(const Zenith_PointerHandle& xOther) const { return !(*this == xOther); }
};

// One pointer slot. m_bActive means the slot HOLDS a pointer record, which
// stays true across the terminal UP/CANCEL frame so a claimer can still see the
// edge and release; IsDown() is the "finger is still on the glass" question.
struct Zenith_Pointer
{
	// Platform pointer id (Android pointer id, or iMOUSE_PLATFORM_ID for the
	// mouse projection). -1 when the slot is free.
	int32_t m_iPlatformId = -1;
	u_int32 m_uGeneration = 0;

	bool m_bActive             = false;
	bool m_bDownThisFrame      = false;
	bool m_bUpThisFrame        = false;
	bool m_bCancelledThisFrame = false;
	bool m_bTapThisFrame       = false;

	// True when the slot was created from the TOUCH stream rather than the mouse
	// projection. The two must never both drive the primary slot — see
	// Zenith_Pointers::ApplyMouseState.
	bool m_bFromTouch = false;
	// Terminal edge landed this frame; the slot frees at the NEXT frame reset so
	// claims survive the whole frame the gesture ended in.
	bool m_bPendingRetire = false;

	Zenith_Maths::Vector2 m_xPosition      = { 0.0f, 0.0f };
	Zenith_Maths::Vector2 m_xStartPosition = { 0.0f, 0.0f };
	// Running MAX distance from the start position across the gesture. Survives
	// coalesced MOVEs (the FIFO folds its own running max into every entry), so a
	// finger that wanders and comes back is not mistaken for a tap.
	float  m_fMaxExcursion = 0.0f;
	double m_fDownTime     = 0.0;

	Zenith_PointerOwner m_ulClaimOwner = ulPOINTER_OWNER_NONE;

	bool IsDown() const { return m_bActive && !m_bUpThisFrame && !m_bCancelledThisFrame; }
	bool IsClaimed() const { return m_ulClaimOwner != ulPOINTER_OWNER_NONE; }
};

class Zenith_Pointers
{
public:
	Zenith_Pointers() = default;
	~Zenith_Pointers() = default;

	Zenith_Pointers(const Zenith_Pointers&) = delete;
	Zenith_Pointers& operator=(const Zenith_Pointers&) = delete;

	static constexpr u_int32 uMAX_POINTERS            = 8;
	// A tap is short AND still. Both thresholds are B7 constants; the excursion
	// one is in LOGICAL pixels and scales by the display's logical-to-physical
	// ratio, because 15 physical pixels is a different gesture on every panel.
	static constexpr float   fTAP_MAX_DURATION_SECONDS = 0.3f;
	static constexpr float   fTAP_MAX_EXCURSION_PX     = 15.0f;
	// B3: the mouse projection owns this platform id.
	static constexpr int32_t iMOUSE_PLATFORM_ID        = 0;

	// ---- Frame lifecycle --------------------------------------------------
	// Step 4. Reset + consume everything the drain produced.
	void ApplyPlatform(Zenith_Input& xInput, float fDisplayScale);
	// Step 7. Consume only what the automated-test Steps added since step 4.
	void ApplyInjection(Zenith_Input& xInput);

	// Cancels every live pointer (raising CANCEL edges) and drops every claim.
	// Raised by a LIFECYCLE_RESET barrier and available to teardown paths.
	void CancelAll();

	// Between automated tests: slots, generations, claims, edges, armed state.
	// Generations reset wholesale — a registration is not a pointer concept, so
	// nothing outlives a test that could be fooled by a recycled generation.
	void ResetTransientForTest();

	// ---- Window-free cores (the engine calls these through ApplyPlatform /
	// ApplyInjection; unit tests drive them directly) -----------------------
	// Retire finished slots, clear per-frame edges, latch the display scale.
	void BeginFrame(float fDisplayScale);
	// Apply ONE staged event (TOUCH_DOWN/MOVE/UP/CANCEL or a LIFECYCLE_RESET
	// barrier). Anything else is ignored.
	void ApplyEvent(const Zenith_InputEvent& xEvent);
	// B3 mouse projection. Level-plus-edge driven and idempotent within a frame,
	// because it runs at BOTH step 4 and step 7 and the same edge is visible to
	// both. Suppressed entirely while the touch stream owns a pointer.
	void ApplyMouseState(bool bPressedEdge, bool bReleasedEdge,
		const Zenith_Maths::Vector2& xPosition, double fTimestamp);
	// Consume staged entries [m_uStagedConsumed, GetStagedTouchCount()). Public
	// so units can drive the two-phase (step 4 / step 7) consume without a
	// window, the same seam Zenith_Input::DrainPendingPlatformEvents offers.
	void ConsumeStagedTail(Zenith_Input& xInput);

	// ---- Queries ----------------------------------------------------------
	const Zenith_Pointer& GetPointer(u_int32 uSlot) const { return m_axPointers[uSlot]; }
	Zenith_PointerHandle  GetHandle(u_int32 uSlot) const;
	// Null when the handle is stale (slot recycled) or was never valid.
	const Zenith_Pointer* Resolve(const Zenith_PointerHandle& xHandle) const;
	bool                  IsLive(const Zenith_PointerHandle& xHandle) const { return Resolve(xHandle) != nullptr; }
	Zenith_PointerHandle  FindByPlatformId(int32_t iPlatformId) const;
	// Lowest live slot — the one B3 keeps in step with the mouse view.
	Zenith_PointerHandle  GetPrimaryPointer() const;
	u_int32               GetActivePointerCount() const;

	// A tap is only reported for an UNCLAIMED pointer, so a widget that captured
	// the press has already consumed it.
	bool WasTapThisFrame() const;
	bool GetTapPosition(Zenith_Maths::Vector2& xOut) const;

	bool IsArmed() const { return m_bArmed; }
	float GetDisplayScale() const { return m_fDisplayScale; }

	// ---- Claims -----------------------------------------------------------
	// Fails on a stale handle, a dead pointer, or one already claimed by someone
	// else. Re-claiming by the same owner succeeds (idempotent).
	bool ClaimPointer(const Zenith_PointerHandle& xHandle, Zenith_PointerOwner ulOwner);
	// Stale-generation releases are REJECTED: last gesture's handle must not free
	// this gesture's claim. So is a release by a non-owner.
	void ReleaseClaim(const Zenith_PointerHandle& xHandle, Zenith_PointerOwner ulOwner);
	bool IsClaimed(const Zenith_PointerHandle& xHandle) const;
	Zenith_PointerOwner GetClaimOwner(const Zenith_PointerHandle& xHandle) const;
	// Widget / canvas destruction and teardown. Cheap enough to call blindly.
	void ReleaseAllClaimsForOwner(Zenith_PointerOwner ulOwner);

private:
	// Mouse half of both apply phases (reads the sim-aware edge queries).
	void ApplyMouseFromInput(Zenith_Input& xInput);

	u_int32 FindSlotByPlatformId(int32_t iPlatformId) const;
	u_int32 AllocateSlot(int32_t iPlatformId, bool bFromTouch, float fX, float fY, double fTimestamp);
	void    UpdateSlotPosition(Zenith_Pointer& xPointer, float fX, float fY, float fEventExcursion);
	void    EndPointer(Zenith_Pointer& xPointer, bool bCancelled);
	bool    IsTouchOwningPointer() const;

	Zenith_Pointer m_axPointers[uMAX_POINTERS] = {};

	// How many of this frame's staged touch entries have already been applied.
	// Step 7 resumes from here rather than replaying step 4's work.
	u_int32 m_uStagedConsumed = 0;

	// Mouse-projection bookkeeping. The per-frame "consumed" latches exist
	// because the SAME press/release edge is visible at step 4 and step 7 — an
	// edge must produce exactly one transition per frame, not one per phase.
	bool m_bMouseDown                = false;
	bool m_bMousePressConsumed       = false;
	bool m_bMouseReleaseConsumed     = false;
	// Any touch event at all this frame suppresses the mouse projection: on a
	// touch device the mouse view IS the projected first finger, and letting it
	// open a second pointer would double every tap.
	bool m_bSawTouchEventThisFrame   = false;

	// Disarmed by a LIFECYCLE_RESET barrier; re-armed from the device layer's
	// armed state at the end of each apply phase.
	// The generation counter is PER SLOT (m_axPointers[u].m_uGeneration) and is
	// bumped when the slot frees, so every reuse of a slot is a new identity.
	bool  m_bArmed        = true;
	float m_fDisplayScale = 1.0f;
};
