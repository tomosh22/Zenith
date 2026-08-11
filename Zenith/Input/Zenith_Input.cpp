#include "Zenith.h"

#include "Input/Zenith_Input.h"

#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
static inline bool IsSimulatorActive() { return Zenith_InputSimulator::IsEnabled(); }
#else
static inline bool IsSimulatorActive() { return false; }
#endif

// Monotonic seconds. The FIFO stamps every event so the action layer can time
// taps / holds without asking the frame context (which the platform callbacks,
// running mid-pump, have no business touching).
static double Zenith_GetInputTimestampSeconds()
{
	const auto xNow = std::chrono::high_resolution_clock::now().time_since_epoch();
	return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(xNow).count()) / 1.0e9;
}

static bool Zenith_IsTouchEventType(Zenith_InputEventType eType)
{
	return eType == INPUT_EVENT_TOUCH_DOWN
		|| eType == INPUT_EVENT_TOUCH_MOVE
		|| eType == INPUT_EVENT_TOUCH_UP
		|| eType == INPUT_EVENT_TOUCH_CANCEL;
}

static bool Zenith_IsPressEventType(Zenith_InputEventType eType)
{
	return eType == INPUT_EVENT_KEY_PRESS
		|| eType == INPUT_EVENT_MOUSE_PRESS
		|| eType == INPUT_EVENT_PAD_PRESS
		|| eType == INPUT_EVENT_TOUCH_DOWN;
}

// ============================================================================
// Zenith_InputEventQueue — the B2 compacting FIFO
// ============================================================================

void Zenith_InputEventQueue::RemoveAt(u_int32 uIndex)
{
	Zenith_Assert(uIndex < m_uCount, "Input FIFO: remove index out of range");
	for (u_int32 u = uIndex + 1; u < m_uCount; u++)
	{
		m_axEvents[u - 1] = m_axEvents[u];
	}
	m_uCount--;
}

bool Zenith_InputEventQueue::TryCoalesceMove(const Zenith_InputEvent& xEvent)
{
	// Walk BACK from the tail and stop at the first event for this pointer. Only
	// a MOVE may absorb the new sample: a DOWN/UP/CANCEL in between means this
	// sample belongs to a later gesture, and merging past it would reorder.
	for (u_int32 u = m_uCount; u > 0; u--)
	{
		Zenith_InputEvent& xExisting = m_axEvents[u - 1];
		if (!Zenith_IsTouchEventType(xExisting.m_eType) || xExisting.m_iCode != xEvent.m_iCode)
		{
			continue;
		}
		if (xExisting.m_eType != INPUT_EVENT_TOUCH_MOVE)
		{
			return false;
		}

		// ACCUMULATE the running max excursion BEFORE the position is lost.
		const float fDeltaX = xEvent.m_fX - xExisting.m_fAnchorX;
		const float fDeltaY = xEvent.m_fY - xExisting.m_fAnchorY;
		const float fDistance = std::sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);
		if (fDistance > xExisting.m_fExcursion)
		{
			xExisting.m_fExcursion = fDistance;
		}
		xExisting.m_fX = xEvent.m_fX;
		xExisting.m_fY = xEvent.m_fY;
		xExisting.m_fTimestamp = xEvent.m_fTimestamp;
		return true;
	}
	return false;
}

bool Zenith_InputEventQueue::EvictOldestMove()
{
	for (u_int32 u = 0; u < m_uCount; u++)
	{
		if (m_axEvents[u].m_eType == INPUT_EVENT_TOUCH_MOVE)
		{
			RemoveAt(u);
			return true;
		}
	}
	return false;
}

bool Zenith_InputEventQueue::EvictOldestPress()
{
	for (u_int32 u = 0; u < m_uCount; u++)
	{
		if (Zenith_IsPressEventType(m_axEvents[u].m_eType))
		{
			RemoveAt(u);
			return true;
		}
	}
	return false;
}

void Zenith_InputEventQueue::Push(const Zenith_InputEvent& xEvent)
{
	if (xEvent.m_eType == INPUT_EVENT_TOUCH_MOVE && TryCoalesceMove(xEvent))
	{
		return;
	}

	if (m_uCount == uCAPACITY)
	{
		// Losing a redundant MOVE costs nothing — the surviving entry already
		// carries the accumulated excursion. Losing a PRESS or dropping the
		// oldest entry outright desynchronises the held tables, so both raise
		// the reconcile request the drain acts on.
		if (!EvictOldestMove())
		{
			if (!EvictOldestPress())
			{
				RemoveAt(0);
			}
			m_bReconcileRequested = true;
		}
	}

	m_axEvents[m_uCount] = xEvent;
	m_axEvents[m_uCount].m_uSequence = m_uNextSequence++;
	if (xEvent.m_eType == INPUT_EVENT_TOUCH_MOVE || xEvent.m_eType == INPUT_EVENT_TOUCH_DOWN)
	{
		m_axEvents[m_uCount].m_fAnchorX = xEvent.m_fX;
		m_axEvents[m_uCount].m_fAnchorY = xEvent.m_fY;
		m_axEvents[m_uCount].m_fExcursion = 0.0f;
	}
	m_uCount++;
}

// ============================================================================
// Frame lifecycle
// ============================================================================

void Zenith_Input::BeginFrame()
{
	bool bJustLeftSimMode = false;
#ifdef ZENITH_INPUT_SIMULATOR
	if (IsSimulatorActive())
	{
		// The FIFO is DISCARDED while the simulator is active: a sim run replaces
		// the device sources wholesale, and letting real window events leak in
		// would make a test's input depend on what the desktop was doing.
		m_xPendingEvents.Clear();
		// Edges + the transition log live exactly one frame in BOTH branches —
		// step 7's injections append into the tables reset here.
		ResetPerFrameEdges();

		// Frame boundary for the injection queue, BEFORE ProcessAutoReleases
		// (which queues this frame's auto-release events).
		Zenith_InputSimulator::DiscardPendingInjections();
		Zenith_InputSimulator::ProcessAutoReleases();

		Zenith_Maths::Vector2_64 xCurrentMousePos;
		Zenith_InputSimulator::GetMousePositionSimulated(xCurrentMousePos);

		// Skip the delta when last-position came from a different input domain.
		if (m_bFirstFrame || !m_bSimWasEnabledLastFrame)
		{
			m_xMouseDelta = { 0.0, 0.0 };
			m_bFirstFrame = false;
		}
		else
		{
			m_xMouseDelta.x = xCurrentMousePos.x - m_xLastMousePosition.x;
			m_xMouseDelta.y = xCurrentMousePos.y - m_xLastMousePosition.y;
		}
		m_xLastMousePosition = xCurrentMousePos;
		m_xProjectedPointerPos = xCurrentMousePos;
		m_bSimWasEnabledLastFrame = true;
		return;
	}
	bJustLeftSimMode = m_bSimWasEnabledLastFrame;
	m_bSimWasEnabledLastFrame = false;
#endif

	DrainPendingPlatformEvents();

	Zenith_Maths::Vector2_64 xCurrentMousePos;
	ReadPlatformPointerPosition(xCurrentMousePos);

	UpdateMouseDeltaFromPosition(xCurrentMousePos, bJustLeftSimMode);

	// B3 (permanent), Windows direction: the mouse feeds pointer 0. On Android
	// the drain above already advanced this from the staged touch stream, so the
	// assignment is a no-op there.
	m_xProjectedPointerPos = xCurrentMousePos;
}

void Zenith_Input::ResetPerFrameEdges()
{
	memset(m_abFrameKeyPresses, 0, sizeof(m_abFrameKeyPresses));
	memset(m_abFrameKeyReleases, 0, sizeof(m_abFrameKeyReleases));
	memset(m_aabPadPresses, 0, sizeof(m_aabPadPresses));
	memset(m_aabPadReleases, 0, sizeof(m_aabPadReleases));
	m_fMouseWheelDelta = 0.0f;
	m_uTransitionCount = 0;
	m_uStagedTouchCount = 0;
}

void Zenith_Input::DrainPendingPlatformEvents()
{
	ResetPerFrameEdges();

	const u_int32 uCount = m_xPendingEvents.GetCount();
	for (u_int32 u = 0; u < uCount; u++)
	{
		ApplyEvent(m_xPendingEvents.Get(u));
	}
	const bool bReconcile = m_xPendingEvents.ConsumeReconcileRequest();
	m_xPendingEvents.Clear();

	// Latest pad axes are LEVEL state, not transitions: they ride the retained
	// snapshot rather than the FIFO, so a skipped frame cannot lose them. A
	// disarmed pad keeps the zeroes CancelAllInput left behind.
	if (m_bInputArmed)
	{
		for (int iPad = 0; iPad < MAX_GAMEPADS; iPad++)
		{
			if (!m_axPadPolled[iPad].m_bConnected)
			{
				continue;
			}
			for (int iAxis = 0; iAxis < Zenith_GamepadSnapshot::iAXIS_COUNT; iAxis++)
			{
				m_axPadLive[iPad].m_afAxes[iAxis] = m_axPadPolled[iPad].m_afAxes[iAxis];
			}
		}
	}

	if (!bReconcile)
	{
		return;
	}

	if (m_eReconcilePolicy == INPUT_RECONCILE_RESYNC_POLLABLE)
	{
		Zenith_Warning(LOG_CATEGORY_INPUT, "Input FIFO overflowed; resyncing held keys against the live device");
#ifdef ZENITH_WINDOWS
		ReconcileHeldKeysAgainstDevice(
			[](void*, Zenith_KeyCode iKey) { return Zenith_Window::GetInstance()->IsKeyDown(iKey); },
			nullptr);
#endif
	}
	else
	{
		// No live-state oracle on an event-fed device: the only safe answer is to
		// cancel everything rather than leave a phantom finger or a stuck key.
		Zenith_Warning(LOG_CATEGORY_INPUT, "Input FIFO overflowed; cancelling event-fed input");
		CancelAllInput();
	}
}

void Zenith_Input::ApplyEvent(const Zenith_InputEvent& xEvent)
{
	// Lifecycle is never gated on the armed state — it is what sets it.
	if (xEvent.m_eType == INPUT_EVENT_LIFECYCLE_RESET)
	{
		ApplyLifecycleBarrier(xEvent);
		return;
	}
	if (xEvent.m_eType == INPUT_EVENT_LIFECYCLE_ARM)
	{
		m_bInputArmed = true;
		return;
	}

	if (!m_bInputArmed)
	{
		return;
	}

	switch (xEvent.m_eType)
	{
	case INPUT_EVENT_KEY_PRESS:
	case INPUT_EVENT_MOUSE_PRESS:
		ApplyKeyEvent(xEvent, true);
		break;
	case INPUT_EVENT_KEY_RELEASE:
	case INPUT_EVENT_MOUSE_RELEASE:
		ApplyKeyEvent(xEvent, false);
		break;
	case INPUT_EVENT_WHEEL:
		m_fMouseWheelDelta += xEvent.m_fY;
		break;
	case INPUT_EVENT_TOUCH_DOWN:
	case INPUT_EVENT_TOUCH_MOVE:
	case INPUT_EVENT_TOUCH_UP:
	case INPUT_EVENT_TOUCH_CANCEL:
		StageTouchEvent(xEvent);
		ProjectTouchOntoMouseView(xEvent);
		break;
	case INPUT_EVENT_PAD_PRESS:
	case INPUT_EVENT_PAD_RELEASE:
	case INPUT_EVENT_PAD_CONNECT:
	case INPUT_EVENT_PAD_DISCONNECT:
		ApplyPadEvent(xEvent);
		break;
	case INPUT_EVENT_SYSTEM_BACK:
		// Carried to the log for a future SYSTEM_BACK binding. Deliberately NOT a
		// key press: nothing in the key domain should see a navigation gesture.
		AppendTransition(xEvent);
		break;
	default:
		break;
	}
}

void Zenith_Input::ApplyLifecycleBarrier(const Zenith_InputEvent& xEvent)
{
	// A barrier voids everything staged before it, cancels the live input, and
	// disarms until a matching ARM. The barrier itself STAYS in the staged
	// stream: the pointer table has to know where the discontinuity was.
	m_uStagedTouchCount = 0;
	CancelAllInput();
	StageTouchEvent(xEvent);
	m_bInputArmed = false;
}

void Zenith_Input::ApplyKeyEvent(const Zenith_InputEvent& xEvent, bool bPressed)
{
	const int32_t iKey = xEvent.m_iCode;
	if (iKey < 0 || iKey >= MAX_KEY_CODES)
	{
		return;
	}

	m_abKeyHeld[iKey] = bPressed;
	if (bPressed)
	{
		m_abFrameKeyPresses[iKey] = true;
	}
	else
	{
		m_abFrameKeyReleases[iKey] = true;
	}
	AppendTransition(xEvent);
}

void Zenith_Input::ApplyPadEvent(const Zenith_InputEvent& xEvent)
{
	const int iPad = xEvent.m_iDevice;
	if (iPad < 0 || iPad >= MAX_GAMEPADS)
	{
		return;
	}

	if (xEvent.m_eType == INPUT_EVENT_PAD_CONNECT)
	{
		m_axPadLive[iPad].m_bConnected = true;
		AppendTransition(xEvent);
		return;
	}
	if (xEvent.m_eType == INPUT_EVENT_PAD_DISCONNECT)
	{
		ApplyPadDisconnect(iPad);
		AppendTransition(xEvent);
		return;
	}

	const int iButton = xEvent.m_iCode;
	if (iButton < 0 || iButton >= Zenith_GamepadSnapshot::iBUTTON_COUNT)
	{
		return;
	}

	const bool bPressed = (xEvent.m_eType == INPUT_EVENT_PAD_PRESS);
	m_axPadLive[iPad].m_abButtons[iButton] = bPressed;
	if (bPressed)
	{
		m_aabPadPresses[iPad][iButton] = true;
	}
	else
	{
		m_aabPadReleases[iPad][iButton] = true;
	}
	AppendTransition(xEvent);
}

void Zenith_Input::ApplyPadDisconnect(int iGamepad)
{
	// A yanked pad must not leave a button held forever, and its axes must rest
	// at the canonical zero — including the triggers, whose rest is 0 because
	// the normalisation happened at the device layer, not at the getter.
	for (int iButton = 0; iButton < Zenith_GamepadSnapshot::iBUTTON_COUNT; iButton++)
	{
		if (!m_axPadLive[iGamepad].m_abButtons[iButton])
		{
			continue;
		}
		m_axPadLive[iGamepad].m_abButtons[iButton] = false;
		m_aabPadReleases[iGamepad][iButton] = true;
		AppendTransition(INPUT_EVENT_PAD_RELEASE, iButton, iGamepad);
	}
	for (int iAxis = 0; iAxis < Zenith_GamepadSnapshot::iAXIS_COUNT; iAxis++)
	{
		m_axPadLive[iGamepad].m_afAxes[iAxis] = 0.0f;
	}
	m_axPadLive[iGamepad].m_bConnected = false;
}

void Zenith_Input::ProjectTouchOntoMouseView(const Zenith_InputEvent& xEvent)
{
	// B3 (PERMANENT): the FIRST touch drives the mouse view — position plus
	// ZENITH_MOUSE_BUTTON_LEFT held/press/release. Secondary pointers exist only
	// in the staged stream (and, from WP1, the pointer table). This is what keeps
	// every unmigrated IsMouseButtonHeld consumer working on a touch device.
	const int32_t iPointer = xEvent.m_iCode;

	switch (xEvent.m_eType)
	{
	case INPUT_EVENT_TOUCH_DOWN:
		if (m_iPrimaryPointerId >= 0)
		{
			return;
		}
		m_iPrimaryPointerId = iPointer;
		m_xProjectedPointerPos.x = static_cast<double>(xEvent.m_fX);
		m_xProjectedPointerPos.y = static_cast<double>(xEvent.m_fY);
		m_abKeyHeld[ZENITH_MOUSE_BUTTON_LEFT] = true;
		m_abFrameKeyPresses[ZENITH_MOUSE_BUTTON_LEFT] = true;
		AppendTransition(INPUT_EVENT_MOUSE_PRESS, ZENITH_MOUSE_BUTTON_LEFT, 0);
		break;

	case INPUT_EVENT_TOUCH_MOVE:
		if (iPointer != m_iPrimaryPointerId)
		{
			return;
		}
		m_xProjectedPointerPos.x = static_cast<double>(xEvent.m_fX);
		m_xProjectedPointerPos.y = static_cast<double>(xEvent.m_fY);
		break;

	case INPUT_EVENT_TOUCH_UP:
	case INPUT_EVENT_TOUCH_CANCEL:
		if (iPointer != m_iPrimaryPointerId)
		{
			return;
		}
		m_xProjectedPointerPos.x = static_cast<double>(xEvent.m_fX);
		m_xProjectedPointerPos.y = static_cast<double>(xEvent.m_fY);
		m_iPrimaryPointerId = -1;
		if (m_abKeyHeld[ZENITH_MOUSE_BUTTON_LEFT])
		{
			m_abKeyHeld[ZENITH_MOUSE_BUTTON_LEFT] = false;
			m_abFrameKeyReleases[ZENITH_MOUSE_BUTTON_LEFT] = true;
			AppendTransition(INPUT_EVENT_MOUSE_RELEASE, ZENITH_MOUSE_BUTTON_LEFT, 0);
		}
		break;

	default:
		break;
	}
}

void Zenith_Input::StageTouchEvent(const Zenith_InputEvent& xEvent)
{
	if (m_uStagedTouchCount >= uMAX_STAGED_TOUCH)
	{
		Zenith_Warning(LOG_CATEGORY_INPUT, "Staged touch stream full (%u); dropping event", uMAX_STAGED_TOUCH);
		return;
	}
	m_axStagedTouch[m_uStagedTouchCount++] = xEvent;
}

void Zenith_Input::AppendTransition(const Zenith_InputEvent& xEvent)
{
	if (m_uTransitionCount >= uMAX_TRANSITIONS)
	{
		Zenith_Warning(LOG_CATEGORY_INPUT, "Transition log full (%u); dropping transition", uMAX_TRANSITIONS);
		return;
	}
	m_axTransitionLog[m_uTransitionCount++] = xEvent;
}

void Zenith_Input::AppendTransition(Zenith_InputEventType eType, int32_t iCode, int32_t iDevice)
{
	// Synthesized transition (cancel / disconnect / reconcile): it has no FIFO
	// sequence of its own, so the log INDEX is its only ordering.
	Zenith_InputEvent xEvent;
	xEvent.m_eType = eType;
	xEvent.m_iCode = iCode;
	xEvent.m_iDevice = iDevice;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	AppendTransition(xEvent);
}

void Zenith_Input::AppendInjectedEvent(const Zenith_InputEvent& xEvent)
{
	ApplyEvent(xEvent);
}

void Zenith_Input::ApplySimulatorInjection()
{
#ifdef ZENITH_INPUT_SIMULATOR
	if (IsSimulatorActive())
	{
		const u_int32 uCount = Zenith_InputSimulator::GetPendingInjectionCount();
		for (u_int32 u = 0; u < uCount; u++)
		{
			AppendInjectedEvent(Zenith_InputSimulator::GetPendingInjection(u));
		}
	}
	// Cleared even when the simulator is off: a test that queued input and then
	// disabled mid-frame must not leak it into the next run.
	Zenith_InputSimulator::DiscardPendingInjections();
#endif
}

void Zenith_Input::CancelAllInput()
{
	// Pointers first, so the staged stream records the cancel before the
	// projection's mouse-button release lands in the transition log.
	if (m_iPrimaryPointerId >= 0)
	{
		Zenith_InputEvent xCancel;
		xCancel.m_eType = INPUT_EVENT_TOUCH_CANCEL;
		xCancel.m_iCode = m_iPrimaryPointerId;
		xCancel.m_fX = static_cast<float>(m_xProjectedPointerPos.x);
		xCancel.m_fY = static_cast<float>(m_xProjectedPointerPos.y);
		xCancel.m_fTimestamp = Zenith_GetInputTimestampSeconds();
		StageTouchEvent(xCancel);
		m_iPrimaryPointerId = -1;
	}

	for (int iKey = 0; iKey < MAX_KEY_CODES; iKey++)
	{
		if (!m_abKeyHeld[iKey])
		{
			continue;
		}
		m_abKeyHeld[iKey] = false;
		m_abFrameKeyReleases[iKey] = true;
		AppendTransition(iKey <= ZENITH_MOUSE_BUTTON_LAST ? INPUT_EVENT_MOUSE_RELEASE : INPUT_EVENT_KEY_RELEASE,
			iKey, 0);
	}

	for (int iPad = 0; iPad < MAX_GAMEPADS; iPad++)
	{
		for (int iButton = 0; iButton < Zenith_GamepadSnapshot::iBUTTON_COUNT; iButton++)
		{
			if (!m_axPadLive[iPad].m_abButtons[iButton])
			{
				continue;
			}
			m_axPadLive[iPad].m_abButtons[iButton] = false;
			m_aabPadReleases[iPad][iButton] = true;
			AppendTransition(INPUT_EVENT_PAD_RELEASE, iButton, iPad);
		}
		for (int iAxis = 0; iAxis < Zenith_GamepadSnapshot::iAXIS_COUNT; iAxis++)
		{
			m_axPadLive[iPad].m_afAxes[iAxis] = 0.0f;
		}
	}

	// Claims (WP2) and virtual sources (WP3) do not exist yet; when they do,
	// they are released/neutralised HERE, not at their own call sites.
}

void Zenith_Input::ReconcileHeldKeysAgainstDevice(bool (*pfnIsKeyDownLive)(void*, Zenith_KeyCode), void* pxUser)
{
	if (pfnIsKeyDownLive == nullptr)
	{
		return;
	}

	for (int iKey = 0; iKey < MAX_KEY_CODES; iKey++)
	{
		const bool bLiveDown = pfnIsKeyDownLive(pxUser, iKey);
		if (bLiveDown == m_abKeyHeld[iKey])
		{
			continue;
		}

		m_abKeyHeld[iKey] = bLiveDown;
		if (bLiveDown)
		{
			m_abFrameKeyPresses[iKey] = true;
			AppendTransition(iKey <= ZENITH_MOUSE_BUTTON_LAST ? INPUT_EVENT_MOUSE_PRESS : INPUT_EVENT_KEY_PRESS,
				iKey, 0);
		}
		else
		{
			m_abFrameKeyReleases[iKey] = true;
			AppendTransition(iKey <= ZENITH_MOUSE_BUTTON_LAST ? INPUT_EVENT_MOUSE_RELEASE : INPUT_EVENT_KEY_RELEASE,
				iKey, 0);
		}
	}
}

void Zenith_Input::ResetTransientForTest()
{
	m_xPendingEvents.Clear();
	(void)m_xPendingEvents.ConsumeReconcileRequest();
	ResetPerFrameEdges();
	memset(m_abKeyHeld, 0, sizeof(m_abKeyHeld));
	for (int iPad = 0; iPad < MAX_GAMEPADS; iPad++)
	{
		m_axPadLive[iPad] = Zenith_GamepadSnapshot();
		m_axPadPolled[iPad] = Zenith_GamepadSnapshot();
	}
	m_iPrimaryPointerId = -1;
	m_xProjectedPointerPos = { 0.0, 0.0 };
	m_bInputArmed = true;
	m_bMouseDiscontinuity = false;
	m_bFirstFrame = true;
}

void Zenith_Input::UpdateMouseDeltaFromPosition(const Zenith_Maths::Vector2_64& xCurrentMousePos, bool bJustLeftSimMode)
{
	// Skip the delta when last-position came from a different input domain, or when a
	// cursor-mode change (capture/release) just teleported the OS cursor — a stale
	// last-position would otherwise produce a one-frame spike. m_bMouseDiscontinuity is
	// a one-shot: cleared here so the FOLLOWING frame resumes computing real deltas from
	// the resynced baseline. Window-free (current pos is supplied) so it is unit-testable.
	if (m_bFirstFrame || bJustLeftSimMode || m_bMouseDiscontinuity)
	{
		m_xMouseDelta = { 0.0, 0.0 };
		m_bFirstFrame = false;
	}
	else
	{
		m_xMouseDelta.x = xCurrentMousePos.x - m_xLastMousePosition.x;
		m_xMouseDelta.y = xCurrentMousePos.y - m_xLastMousePosition.y;
	}

	m_bMouseDiscontinuity = false;
	m_xLastMousePosition = xCurrentMousePos;
}

void Zenith_Input::ReadPlatformPointerPosition(Zenith_Maths::Vector2_64& xOut) const
{
#ifdef ZENITH_ANDROID
	// B3: the first touch IS the mouse view here. The drain has already advanced
	// the projection from the staged stream, so there is no window to ask.
	xOut = m_xProjectedPointerPos;
#else
	Zenith_Window::GetInstance()->GetMousePosition(xOut);
#endif
}

// ============================================================================
// Platform ingress — enqueue only, no state changes
// ============================================================================

void Zenith_Input::KeyPressedCallback(Zenith_KeyCode iKey)
{
	if (iKey < 0 || iKey >= MAX_KEY_CODES)
	{
		return;
	}
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_KEY_PRESS;
	xEvent.m_iCode = iKey;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::KeyReleasedCallback(Zenith_KeyCode iKey)
{
	if (iKey < 0 || iKey >= MAX_KEY_CODES)
	{
		return;
	}
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_KEY_RELEASE;
	xEvent.m_iCode = iKey;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::MouseButtonPressedCallback(Zenith_KeyCode iKey)
{
	if (iKey < 0 || iKey >= MAX_KEY_CODES)
	{
		return;
	}
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_MOUSE_PRESS;
	xEvent.m_iCode = iKey;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::MouseButtonReleasedCallback(Zenith_KeyCode iKey)
{
	if (iKey < 0 || iKey >= MAX_KEY_CODES)
	{
		return;
	}
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_MOUSE_RELEASE;
	xEvent.m_iCode = iKey;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::MouseWheelCallback(double fXOffset, double fYOffset)
{
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_WHEEL;
	xEvent.m_fX = static_cast<float>(fXOffset);
	xEvent.m_fY = static_cast<float>(fYOffset);
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::TouchEventCallback(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY)
{
	Zenith_Assert(Zenith_IsTouchEventType(eType), "TouchEventCallback given a non-touch event type");
	Zenith_InputEvent xEvent;
	xEvent.m_eType = eType;
	xEvent.m_iCode = iPointerId;
	xEvent.m_fX = fX;
	xEvent.m_fY = fY;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::SystemBackCallback()
{
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_SYSTEM_BACK;
	xEvent.m_bSystemNav = true;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::LifecycleResetCallback()
{
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_LIFECYCLE_RESET;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

void Zenith_Input::LifecycleArmCallback()
{
	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_LIFECYCLE_ARM;
	xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
	m_xPendingEvents.Push(xEvent);
}

// ============================================================================
// Queries
// ============================================================================

float Zenith_Input::GetMouseWheelDelta() const
{
	// Read through to the simulator each query — SimulateMouseWheel() typically
	// runs AFTER BeginFrame in the same tick, so a BeginFrame-time copy would
	// be a frame stale.
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::GetMouseWheelDeltaSimulated();
	}
	return m_fMouseWheelDelta;
}

void Zenith_Input::GetMousePosition(Zenith_Maths::Vector2_64& xOut)
{
	if (IsSimulatorActive())
	{
		Zenith_InputSimulator::GetMousePositionSimulated(xOut);
		return;
	}
	ReadPlatformPointerPosition(xOut);
}

void Zenith_Input::GetMouseDelta(Zenith_Maths::Vector2_64& xOut)
{
	xOut = m_xMouseDelta;
}

bool Zenith_Input::IsKeyDown(Zenith_KeyCode iKey)
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::IsKeyDownSimulated(iKey);
	}
	return iKey >= 0 && iKey < MAX_KEY_CODES && m_abKeyHeld[iKey];
}

bool Zenith_Input::WasKeyPressedThisFrame(Zenith_KeyCode iKey)
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::WasKeyPressedThisFrameSimulated(iKey);
	}
	return iKey >= 0 && iKey < MAX_KEY_CODES && m_abFrameKeyPresses[iKey];
}

bool Zenith_Input::WasKeyReleasedThisFrame(Zenith_KeyCode iKey)
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::WasKeyReleasedThisFrameSimulated(iKey);
	}
	return iKey >= 0 && iKey < MAX_KEY_CODES && m_abFrameKeyReleases[iKey];
}

// ============================================================================
// Gamepad
// ============================================================================

float Zenith_Input::NormaliseTriggerAxis(float fRawAxis)
{
	// GLFW reports a trigger over [-1,1] with REST = -1. Canonicalise ONCE, here
	// at the device layer: doing it in the getter meant a disconnected pad's
	// zeroed axis mapped to 0.5 — a half-pulled trigger that was never touched.
	const float fNormalised = (fRawAxis + 1.0f) * 0.5f;
	return fNormalised < 0.0f ? 0.0f : (fNormalised > 1.0f ? 1.0f : fNormalised);
}

float Zenith_Input::ApplyRadialDeadzone(float fValue, float fPartnerValue)
{
	const float fMagnitude = std::sqrt(fValue * fValue + fPartnerValue * fPartnerValue);
	return fMagnitude < GAMEPAD_DEADZONE ? 0.0f : fValue;
}

void Zenith_Input::SubmitGamepadSnapshot(int iGamepad, const Zenith_GamepadSnapshot& xPhysical)
{
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS)
	{
		return;
	}

	Zenith_GamepadSnapshot& xBaseline = m_axPadPolled[iGamepad];

	if (xPhysical.m_bConnected && !xBaseline.m_bConnected)
	{
		// Adopt the arriving state as the baseline WITHOUT emitting transitions:
		// a pad plugged in with a button already held has not just been pressed.
		xBaseline = xPhysical;
		Zenith_InputEvent xEvent;
		xEvent.m_eType = INPUT_EVENT_PAD_CONNECT;
		xEvent.m_iDevice = iGamepad;
		xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
		m_xPendingEvents.Push(xEvent);
		return;
	}

	if (!xPhysical.m_bConnected)
	{
		if (!xBaseline.m_bConnected)
		{
			return;
		}
		// The DISCONNECT alone rides the FIFO; the drain synthesizes the releases
		// so a disconnect on a skipped frame still produces them when drained.
		xBaseline = Zenith_GamepadSnapshot();
		Zenith_InputEvent xEvent;
		xEvent.m_eType = INPUT_EVENT_PAD_DISCONNECT;
		xEvent.m_iDevice = iGamepad;
		xEvent.m_fTimestamp = Zenith_GetInputTimestampSeconds();
		m_xPendingEvents.Push(xEvent);
		return;
	}

	const double fTimestamp = Zenith_GetInputTimestampSeconds();
	for (int iButton = 0; iButton < Zenith_GamepadSnapshot::iBUTTON_COUNT; iButton++)
	{
		if (xPhysical.m_abButtons[iButton] == xBaseline.m_abButtons[iButton])
		{
			continue;
		}
		Zenith_InputEvent xEvent;
		xEvent.m_eType = xPhysical.m_abButtons[iButton] ? INPUT_EVENT_PAD_PRESS : INPUT_EVENT_PAD_RELEASE;
		xEvent.m_iCode = iButton;
		xEvent.m_iDevice = iGamepad;
		xEvent.m_fTimestamp = fTimestamp;
		m_xPendingEvents.Push(xEvent);
	}

	xBaseline = xPhysical;
}

#ifdef ZENITH_WINDOWS

void Zenith_Input::PollGamepads()
{
	for (int iPad = 0; iPad < MAX_GAMEPADS; iPad++)
	{
		Zenith_GamepadSnapshot xPhysical;
		const int iJoystickID = GLFW_JOYSTICK_1 + iPad;

		GLFWgamepadstate xGlfwState = {};
		if (glfwJoystickIsGamepad(iJoystickID) && glfwGetGamepadState(iJoystickID, &xGlfwState))
		{
			xPhysical.m_bConnected = true;
			for (int iButton = 0; iButton < Zenith_GamepadSnapshot::iBUTTON_COUNT; iButton++)
			{
				xPhysical.m_abButtons[iButton] = (xGlfwState.buttons[iButton] == GLFW_PRESS);
			}
			for (int iAxis = 0; iAxis < Zenith_GamepadSnapshot::iAXIS_COUNT; iAxis++)
			{
				xPhysical.m_afAxes[iAxis] = (iAxis >= ZENITH_GAMEPAD_AXIS_LEFT_TRIGGER)
					? NormaliseTriggerAxis(xGlfwState.axes[iAxis])
					: xGlfwState.axes[iAxis];
			}
		}

		SubmitGamepadSnapshot(iPad, xPhysical);
	}
}

#else

void Zenith_Input::PollGamepads()
{
	// No pollable pad on this platform. Event-fed pads (if any ever arrive) go
	// through the FIFO like every other event-fed device.
}

#endif

float Zenith_Input::ReadRawGamepadAxis(int iAxis, int iGamepad) const
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::GetGamepadAxisSimulated(iAxis, iGamepad);
	}
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return 0.0f;
	if (iAxis < 0 || iAxis >= Zenith_GamepadSnapshot::iAXIS_COUNT) return 0.0f;
	return m_axPadLive[iGamepad].m_afAxes[iAxis];
}

bool Zenith_Input::IsGamepadConnected(int iGamepad)
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::IsGamepadConnectedSimulated(iGamepad);
	}
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	return m_axPadLive[iGamepad].m_bConnected;
}

bool Zenith_Input::IsGamepadButtonDown(int iButton, int iGamepad)
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::IsGamepadButtonDownSimulated(iButton, iGamepad);
	}
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	if (iButton < 0 || iButton >= Zenith_GamepadSnapshot::iBUTTON_COUNT) return false;
	return m_axPadLive[iGamepad].m_abButtons[iButton];
}

bool Zenith_Input::WasGamepadButtonPressedThisFrame(int iButton, int iGamepad)
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::WasGamepadButtonPressedThisFrameSimulated(iButton, iGamepad);
	}
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	if (iButton < 0 || iButton >= Zenith_GamepadSnapshot::iBUTTON_COUNT) return false;
	return m_aabPadPresses[iGamepad][iButton];
}

bool Zenith_Input::WasGamepadButtonReleasedThisFrame(int iButton, int iGamepad)
{
	if (IsSimulatorActive())
	{
		return Zenith_InputSimulator::WasGamepadButtonReleasedThisFrameSimulated(iButton, iGamepad);
	}
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	if (iButton < 0 || iButton >= Zenith_GamepadSnapshot::iBUTTON_COUNT) return false;
	return m_aabPadReleases[iGamepad][iButton];
}

float Zenith_Input::GetGamepadAxis(int iAxis, int iGamepad)
{
	if (iAxis < 0 || iAxis >= Zenith_GamepadSnapshot::iAXIS_COUNT) return 0.0f;

	const float fValue = ReadRawGamepadAxis(iAxis, iGamepad);
	if (iAxis > ZENITH_GAMEPAD_AXIS_RIGHT_Y)
	{
		// Triggers: canonical [0,1], REST 0, no deadzone.
		return fValue;
	}

	// Stick: the deadzone is RADIAL, so it needs the partner axis of the pair.
	const int iPartner = (iAxis % 2 == 0) ? iAxis + 1 : iAxis - 1;
	return ApplyRadialDeadzone(fValue, ReadRawGamepadAxis(iPartner, iGamepad));
}

void Zenith_Input::GetGamepadLeftStick(float& fX, float& fY, int iGamepad)
{
	fX = GetGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_X, iGamepad);
	fY = GetGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_Y, iGamepad);
}

void Zenith_Input::GetGamepadRightStick(float& fX, float& fY, int iGamepad)
{
	fX = GetGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_X, iGamepad);
	fY = GetGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_Y, iGamepad);
}

float Zenith_Input::GetGamepadLeftTrigger(int iGamepad)
{
	return GetGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_TRIGGER, iGamepad);
}

float Zenith_Input::GetGamepadRightTrigger(int iGamepad)
{
	return GetGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, iGamepad);
}

#include "Input/Zenith_Input.Tests.inl"
