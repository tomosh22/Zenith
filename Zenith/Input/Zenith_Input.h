#pragma once

#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Zenith_Input — the device layer.
//
// FRAME CONTRACT (the single source of truth for ordering; Zenith_Core owns the
// call sites):
//   1. BeginFrame_Platform: renderer begin + timers + the platform PUMP
//      (glfwPollEvents on Windows; the ALooper loop on Android) + the gamepad
//      POLL-DIFF. Platform callbacks ONLY enqueue into the event FIFO — they
//      change no state.
//   2. Acquire the swapchain. On a skip the frame RETURNS: the FIFO and the
//      retained pad snapshot persist, so nothing is lost across skipped frames.
//   3. Input().BeginFrame() — the DRAIN. Per-frame edges reset, then the FIFO
//      is consumed IN ARRIVAL ORDER into the held tables, the press/release
//      edges, the wheel, the staged touch stream and the ordered transition log.
//   ...
//   7. ApplySimulatorInjection() — this frame's injected events become
//      transitions, after the automated-test Steps have run.
//
// The drain sits AFTER the acquire gate deliberately: a frame the swapchain
// skips must not consume input that no game logic will ever see.
// ============================================================================

// Event kinds carried by the platform FIFO (B2). Mouse buttons occupy key slots
// 0-7, so MOUSE_* and KEY_* differ only in what they tell the consumer about the
// originating device — both land in the same held table.
enum Zenith_InputEventType : u_int8
{
	INPUT_EVENT_NONE = 0,
	INPUT_EVENT_KEY_PRESS,
	INPUT_EVENT_KEY_RELEASE,
	INPUT_EVENT_MOUSE_PRESS,
	INPUT_EVENT_MOUSE_RELEASE,
	INPUT_EVENT_WHEEL,
	INPUT_EVENT_TOUCH_DOWN,
	INPUT_EVENT_TOUCH_MOVE,
	INPUT_EVENT_TOUCH_UP,
	INPUT_EVENT_TOUCH_CANCEL,
	INPUT_EVENT_PAD_PRESS,
	INPUT_EVENT_PAD_RELEASE,
	INPUT_EVENT_PAD_CONNECT,
	INPUT_EVENT_PAD_DISCONNECT,
	INPUT_EVENT_LIFECYCLE_RESET,
	INPUT_EVENT_LIFECYCLE_ARM,
	INPUT_EVENT_SYSTEM_BACK,
	INPUT_EVENT_COUNT
};

// What the drain does when the FIFO had to drop state-bearing events. Windows
// devices are POLLABLE, so the held tables can be resynced against the live
// device (synthesizing the releases that were lost); Android devices are
// EVENT-FED with no such oracle, so the only safe answer is to cancel.
enum Zenith_InputReconcilePolicy : u_int8
{
	INPUT_RECONCILE_RESYNC_POLLABLE = 0,
	INPUT_RECONCILE_CANCEL_EVENT_FED
};

// One FIFO entry. Also reused verbatim for the staged touch stream and the
// ordered transition log — the fields mean the same thing in all three, and a
// near-duplicate struct per consumer would just be three places to forget.
struct Zenith_InputEvent
{
	double                m_fTimestamp = 0.0;
	u_int32               m_uSequence  = 0;
	// Key code / mouse button / pad button, or the POINTER ID for touch events.
	int32_t               m_iCode      = 0;
	// Gamepad index; 0 for every other device.
	int32_t               m_iDevice    = 0;
	// Touch position, or the wheel offsets (x = horizontal, y = vertical ticks).
	float                 m_fX         = 0.0f;
	float                 m_fY         = 0.0f;
	// Coalescing anchor: the position this MOVE chain started at. Only touch
	// MOVE entries use it (see Zenith_InputEventQueue::Push).
	float                 m_fAnchorX   = 0.0f;
	float                 m_fAnchorY   = 0.0f;
	// Running MAX displacement from the anchor across every MOVE coalesced into
	// this entry. Survives the position overwrite, so a finger that wanders and
	// comes back still reports that it moved.
	float                 m_fExcursion = 0.0f;
	Zenith_InputEventType m_eType      = INPUT_EVENT_NONE;
	// System navigation (Android BACK). Carried through to the transition log so
	// a future SYSTEM_BACK binding can consume it; excluded from the
	// last-used-device activity detection the action layer will do.
	bool                  m_bSystemNav = false;
};

// The B2 FIFO. NOT Zenith_CircularQueue: this structure has to evict from the
// MIDDLE (coalesce a redundant touch MOVE, drop the oldest PRESS under
// pressure), which a circular queue cannot do. Capacity is small and the drain
// empties it every frame, so the O(n) compaction is a handful of moves.
class Zenith_InputEventQueue
{
public:
	static constexpr u_int32 uCAPACITY = 256;

	// Appends, coalescing per-pointer touch MOVEs. Under pressure: evict the
	// oldest MOVE (lossless), else the oldest PRESS, else drop the oldest entry
	// outright — the last two corrupt the held tables, so both raise the
	// reconcile request the drain acts on.
	void Push(const Zenith_InputEvent& xEvent);

	u_int32 GetCount() const { return m_uCount; }
	bool    IsEmpty() const { return m_uCount == 0; }
	const Zenith_InputEvent& Get(u_int32 uIndex) const { return m_axEvents[uIndex]; }

	void Clear() { m_uCount = 0; }

	bool IsReconcileRequested() const { return m_bReconcileRequested; }
	bool ConsumeReconcileRequest()
	{
		const bool bRequested = m_bReconcileRequested;
		m_bReconcileRequested = false;
		return bRequested;
	}

private:
	// Returns true when xEvent was merged into an existing pending MOVE for the
	// same pointer. Only merges when no DOWN/UP/CANCEL for that pointer sits
	// between it and the tail — otherwise the merge would reorder a gesture.
	bool TryCoalesceMove(const Zenith_InputEvent& xEvent);
	bool EvictOldestMove();
	bool EvictOldestPress();
	void RemoveAt(u_int32 uIndex);

	Zenith_InputEvent m_axEvents[uCAPACITY];
	u_int32           m_uCount              = 0;
	u_int32           m_uNextSequence       = 0;
	bool              m_bReconcileRequested = false;
};

// A gamepad's physical state in CANONICAL domains: sticks [-1,1] with no
// deadzone applied (that happens at the getter), triggers [0,1] with REST = 0.
// The trigger normalisation happens ONCE, here at the device layer — the old
// per-getter (v+1)*0.5 meant a DISCONNECTED pad's zeroed axis read as 0.5.
struct Zenith_GamepadSnapshot
{
	static constexpr int iBUTTON_COUNT = ZENITH_GAMEPAD_BUTTON_LAST + 1;
	static constexpr int iAXIS_COUNT   = ZENITH_GAMEPAD_AXIS_LAST + 1;

	bool  m_bConnected                = false;
	bool  m_abButtons[iBUTTON_COUNT]  = {};
	float m_afAxes[iAXIS_COUNT]       = {};
};

// State + behaviour for the Input subsystem. Held on g_xEngine and
// accessed via g_xEngine.Input().
class Zenith_Input
{
public:
	Zenith_Input() = default;
	~Zenith_Input() = default;

	Zenith_Input(const Zenith_Input&) = delete;
	Zenith_Input& operator=(const Zenith_Input&) = delete;

	// Covers GLFW keycodes (max 348) and mouse buttons (0-7).
	static constexpr int      MAX_KEY_CODES        = 512;
	static constexpr int      MAX_GAMEPADS         = 4;
	static constexpr float    GAMEPAD_DEADZONE     = 0.15f;
	static constexpr u_int32  uMAX_TRANSITIONS     = 64;
	static constexpr u_int32  uMAX_STAGED_TOUCH    = 64;

	// ---- Frame lifecycle -------------------------------------------------
	// Step 3 of the frame contract: the DRAIN. Resets the per-frame edges then
	// consumes the FIFO. Early-returns through the simulator branch when the
	// simulator is enabled (the FIFO is DISCARDED there — sim runs replace the
	// device sources wholesale).
	void BeginFrame();

	// Step 1: the gamepad poll-diff, run inside the platform pump so a pad tap
	// that lands on a frame the swapchain later skips still reaches the FIFO.
	void PollGamepads();

	// The poll-diff core, window-free. PollGamepads builds the snapshot from
	// GLFW and hands it here; unit tests hand it a synthetic one.
	void SubmitGamepadSnapshot(int iGamepad, const Zenith_GamepadSnapshot& xPhysical);

	// The drain proper. Public so units can drive a frame's worth of platform
	// events without a window, and so WP2's action layer has an explicit seam.
	void DrainPendingPlatformEvents();

	// Frame contract step 7, called right after the automated-test Steps have run:
	// pulls everything the Steps injected into the simulator this frame and
	// applies it here, so the action layer sees simulated and real input through
	// exactly one path. No-op when the simulator is disabled or absent.
	void ApplySimulatorInjection();

	// Releases every held key / mouse button / pad button (emitting the release
	// edges + transitions), cancels the live pointer, and zeroes the pad axes.
	// Raised by a LIFECYCLE_RESET barrier and available to shutdown paths.
	void CancelAllInput();

	// Between automated tests: transition log, staged stream, edges, held
	// tables, armed state, pad state, pending FIFO. Registrations (there are
	// none yet) are untouched.
	void ResetTransientForTest();

	// ---- Platform ingress (callbacks ONLY enqueue) ------------------------
	void KeyPressedCallback(Zenith_KeyCode iKey);
	void KeyReleasedCallback(Zenith_KeyCode iKey);
	void MouseButtonPressedCallback(Zenith_KeyCode iKey);
	void MouseButtonReleasedCallback(Zenith_KeyCode iKey);
	void MouseWheelCallback(double fXOffset, double fYOffset);
	void TouchEventCallback(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY);
	void SystemBackCallback();
	// Focus loss / pause / window teardown, and their re-arm counterparts.
	void LifecycleResetCallback();
	void LifecycleArmCallback();
	void EnqueuePlatformEvent(const Zenith_InputEvent& xEvent) { m_xPendingEvents.Push(xEvent); }

	// Appends a simulator-injected event: transition log + held tables + the
	// staged touch stream, exactly as if it had come off the FIFO. Driven by
	// ApplySimulatorInjection at step 7.
	void AppendInjectedEvent(const Zenith_InputEvent& xEvent);

	// Pure (window-free) core of BeginFrame's mouse-delta update: given the current
	// cursor position, computes m_xMouseDelta + advances m_xLastMousePosition, applying
	// the first-frame / left-sim / discontinuity one-shot skip. BeginFrame supplies the
	// live GLFW position; unit tests supply a synthetic one to verify the one-shot
	// discontinuity semantics without a window.
	void UpdateMouseDeltaFromPosition(const Zenith_Maths::Vector2_64& xCurrentMousePos, bool bJustLeftSimMode);

	// Signal that the OS cursor position jumped discontinuously this frame (e.g. the
	// window just switched GLFW_CURSOR between NORMAL and DISABLED, which re-centres /
	// teleports the cursor). The NEXT BeginFrame zeroes that frame's mouse delta and
	// resyncs the last-position baseline — mirroring the input-domain (simulator) skip
	// — so the switch doesn't produce a one-frame look spike. One-shot.
	void NotifyMouseDiscontinuity() { m_bMouseDiscontinuity = true; }

	// ---- Queries ----------------------------------------------------------
	void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	void GetMouseDelta(Zenith_Maths::Vector2_64& xOut);
	bool IsKeyDown(Zenith_KeyCode iKey);
	bool IsMouseButtonHeld(Zenith_KeyCode iMouseButton) { return IsKeyDown(iMouseButton); }
	bool WasKeyPressedThisFrame(Zenith_KeyCode iKey);
	bool WasKeyReleasedThisFrame(Zenith_KeyCode iKey);
	float GetMouseWheelDelta() const;  // sim-aware; defined in .cpp

	bool IsGamepadConnected(int iGamepad = 0);
	bool IsGamepadButtonDown(int iButton, int iGamepad = 0);
	bool WasGamepadButtonPressedThisFrame(int iButton, int iGamepad = 0);
	bool WasGamepadButtonReleasedThisFrame(int iButton, int iGamepad = 0);
	float GetGamepadAxis(int iAxis, int iGamepad = 0);
	void GetGamepadLeftStick(float& fX, float& fY, int iGamepad = 0);
	void GetGamepadRightStick(float& fX, float& fY, int iGamepad = 0);
	float GetGamepadLeftTrigger(int iGamepad = 0);
	float GetGamepadRightTrigger(int iGamepad = 0);

	// Radial stick deadzone: a stick is dead when its 2D MAGNITUDE is inside the
	// threshold, not when either axis is — a per-axis test carves a square hole
	// and leaks diagonal drift. Pure, so it is unit-testable on its own.
	static float ApplyRadialDeadzone(float fValue, float fPartnerValue);
	// GLFW reports triggers over [-1,1] with REST = -1. Canonicalise ONCE.
	static float NormaliseTriggerAxis(float fRawAxis);

	// Ordered transition log for this frame (consumed by the action layer).
	u_int32 GetTransitionCount() const { return m_uTransitionCount; }
	const Zenith_InputEvent& GetTransition(u_int32 uIndex) const { return m_axTransitionLog[uIndex]; }

	// Staged touch stream for this frame, LIFECYCLE_RESET barriers included.
	// The pointer table (WP1) consumes it; WP0 only feeds the B3 projection.
	u_int32 GetStagedTouchCount() const { return m_uStagedTouchCount; }
	const Zenith_InputEvent& GetStagedTouch(u_int32 uIndex) const { return m_axStagedTouch[uIndex]; }

	// B3 (PERMANENT, never deleted): the primary pointer and the mouse view are
	// the same thing. On Windows the mouse feeds pointer 0; on Android the FIRST
	// touch feeds the mouse view — position plus ZENITH_MOUSE_BUTTON_LEFT
	// held/press/release — maintained at drain. This is what keeps every
	// unmigrated IsMouseButtonHeld consumer and every MOUSE_BUTTON binding
	// working on a touch device at every phase of the program.
	void GetProjectedPointerPosition(Zenith_Maths::Vector2_64& xOut) const { xOut = m_xProjectedPointerPos; }
	int32_t GetPrimaryPointerId() const { return m_iPrimaryPointerId; }

	bool IsInputArmed() const { return m_bInputArmed; }

	// Does the running game bind the platform's system-BACK gesture to anything?
	//
	// The answer is a REGISTRATION fact, so it lives here at the device layer
	// rather than being walked out of the action layer's public surface. The
	// platform entry point that has to decide whether to CONSUME Android's Back
	// gesture sits BELOW Input in the layer DAG and cannot ask the action layer at
	// all; it can reach the device layer through the window funnel it already uses
	// for every other input path. Zenith_InputActions raises the flag from
	// RegisterBinding — Actions -> Input is the legal DOWN edge, and the reference
	// was injected at its Initialise.
	//
	// Sticky, and deliberately NOT cleared by ResetTransientForTest: a binding is
	// registered once at boot and is not per-frame state.
	void NotifySystemBackBindingRegistered() { m_bSystemBackBindingRegistered = true; }
	bool HasSystemBackBinding() const { return m_bSystemBackBindingRegistered; }

	// Resync the held key table against a live-device probe, synthesizing the
	// releases the FIFO lost. The probe is a function pointer (no std::function
	// in this codebase): Windows passes a GLFW thunk, units pass a synthetic one.
	void ReconcileHeldKeysAgainstDevice(bool (*pfnIsKeyDownLive)(void*, Zenith_KeyCode), void* pxUser);

	const Zenith_InputEventQueue& GetPendingEvents() const { return m_xPendingEvents; }
	Zenith_InputEventQueue& GetPendingEvents() { return m_xPendingEvents; }

	// ---- State (public, as the rest of this class always has been) --------
	bool m_abFrameKeyPresses[MAX_KEY_CODES] = {};
	bool m_abFrameKeyReleases[MAX_KEY_CODES] = {};
	// Event-fed logical held state. NOT a live device poll: a poll cannot be
	// cancelled by a lifecycle barrier and cannot be corrected after an overflow.
	bool m_abKeyHeld[MAX_KEY_CODES] = {};

	Zenith_Maths::Vector2_64 m_xLastMousePosition = { 0.0, 0.0 };
	Zenith_Maths::Vector2_64 m_xMouseDelta        = { 0.0, 0.0 };
	float                    m_fMouseWheelDelta   = 0.0f;
	bool                     m_bFirstFrame        = true;
	// One-shot: set by NotifyMouseDiscontinuity (cursor capture/release), consumed by
	// the next BeginFrame to skip that frame's delta + resync the baseline.
	bool                     m_bMouseDiscontinuity = false;

	// Disarmed by a LIFECYCLE_RESET barrier; ordinary events are DISCARDED until
	// a LIFECYCLE_ARM (focus/resume) re-arms.
	bool                     m_bInputArmed = true;

#ifdef ZENITH_ANDROID
	Zenith_InputReconcilePolicy m_eReconcilePolicy = INPUT_RECONCILE_CANCEL_EVENT_FED;
#else
	Zenith_InputReconcilePolicy m_eReconcilePolicy = INPUT_RECONCILE_RESYNC_POLLABLE;
#endif

	// Logical pad state consumers see, and the physical baseline the poll diffs
	// against. They differ while input is disarmed: the physical pad keeps
	// reporting a held button, the logical one stays released until a fresh
	// transition arrives after the re-arm.
	Zenith_GamepadSnapshot m_axPadLive[MAX_GAMEPADS]   = {};
	Zenith_GamepadSnapshot m_axPadPolled[MAX_GAMEPADS] = {};
	bool m_aabPadPresses[MAX_GAMEPADS][Zenith_GamepadSnapshot::iBUTTON_COUNT]  = {};
	bool m_aabPadReleases[MAX_GAMEPADS][Zenith_GamepadSnapshot::iBUTTON_COUNT] = {};

#ifdef ZENITH_INPUT_SIMULATOR
	bool                     m_bSimWasEnabledLastFrame = false;
#endif

private:
	void ResetPerFrameEdges();
	void AppendTransition(const Zenith_InputEvent& xEvent);
	void AppendTransition(Zenith_InputEventType eType, int32_t iCode, int32_t iDevice);
	void StageTouchEvent(const Zenith_InputEvent& xEvent);
	// Applies one drained/injected event to the held tables, edges, wheel,
	// staged stream and projection, honouring the armed gate. Shared by the
	// drain and the injection path.
	void ApplyEvent(const Zenith_InputEvent& xEvent);
	void ApplyLifecycleBarrier(const Zenith_InputEvent& xEvent);
	void ApplyKeyEvent(const Zenith_InputEvent& xEvent, bool bPressed);
	void ApplyPadEvent(const Zenith_InputEvent& xEvent);
	void ApplyPadDisconnect(int iGamepad);
	// B3: maintain the primary-pointer <-> mouse projection from a touch event.
	void ProjectTouchOntoMouseView(const Zenith_InputEvent& xEvent);
	void ReadPlatformPointerPosition(Zenith_Maths::Vector2_64& xOut) const;
	float ReadRawGamepadAxis(int iAxis, int iGamepad) const;

	Zenith_InputEventQueue m_xPendingEvents;

	Zenith_InputEvent m_axTransitionLog[uMAX_TRANSITIONS] = {};
	u_int32           m_uTransitionCount = 0;

	Zenith_InputEvent m_axStagedTouch[uMAX_STAGED_TOUCH] = {};
	u_int32           m_uStagedTouchCount = 0;

	Zenith_Maths::Vector2_64 m_xProjectedPointerPos = { 0.0, 0.0 };
	int32_t                  m_iPrimaryPointerId    = -1;

	// Raised by Zenith_InputActions::RegisterBinding the first time a SYSTEM_BACK
	// row is registered; sticky for the life of the process.
	bool                     m_bSystemBackBindingRegistered = false;
};
