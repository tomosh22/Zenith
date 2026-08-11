#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_Input.h"
#include "Zenith_Core.h"

bool Zenith_InputSimulator::s_bEnabled = false;
bool Zenith_InputSimulator::s_bFixedDtEnabled = false;
float Zenith_InputSimulator::s_fFixedDt = 0.f;
bool Zenith_InputSimulator::s_abKeyState[uMAX_SIMULATED_KEYS] = {};
bool Zenith_InputSimulator::s_abKeyPressedThisFrame[uMAX_SIMULATED_KEYS] = {};
bool Zenith_InputSimulator::s_abKeyReleasedThisFrame[uMAX_SIMULATED_KEYS] = {};
Zenith_Maths::Vector2_64 Zenith_InputSimulator::s_xMousePosition = { 0.0, 0.0 };
Zenith_KeyCode Zenith_InputSimulator::s_aeAutoReleaseKeys[uMAX_AUTO_RELEASE] = {};
u_int32 Zenith_InputSimulator::s_uAutoReleaseCount = 0;
float Zenith_InputSimulator::s_fMouseWheelDelta = 0.0f;
bool Zenith_InputSimulator::s_abPadConnected[iMAX_SIMULATED_GAMEPADS] = {};
bool Zenith_InputSimulator::s_aabPadButtons[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_BUTTON_COUNT] = {};
bool Zenith_InputSimulator::s_aabPadPressedThisFrame[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_BUTTON_COUNT] = {};
bool Zenith_InputSimulator::s_aabPadReleasedThisFrame[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_BUTTON_COUNT] = {};
float Zenith_InputSimulator::s_aafPadAxes[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_AXIS_COUNT] = {};
Zenith_InputEvent Zenith_InputSimulator::s_axPendingInjections[uMAX_PENDING_INJECTIONS] = {};
u_int32 Zenith_InputSimulator::s_uPendingInjectionCount = 0;

// ========== Lifecycle ==========

void Zenith_InputSimulator::Enable()
{
	s_bEnabled = true;
	ResetAllInputState();
}

void Zenith_InputSimulator::Disable()
{
	s_bEnabled = false;
	s_bFixedDtEnabled = false;
	ResetAllInputState();
}

bool Zenith_InputSimulator::IsEnabled()
{
	return s_bEnabled;
}

// ========== Frame Control ==========

void Zenith_InputSimulator::StepFrame()
{
	Zenith_Core::Zenith_MainLoop();
}

void Zenith_InputSimulator::StepFrames(u_int32 uCount)
{
	for (u_int32 u = 0; u < uCount; u++)
	{
		StepFrame();
	}
}

void Zenith_InputSimulator::StepFramesWithFixedDt(u_int32 uCount, float fDt)
{
	s_bFixedDtEnabled = true;
	s_fFixedDt = fDt;

	for (u_int32 u = 0; u < uCount; u++)
	{
		StepFrame();
	}

	s_bFixedDtEnabled = false;
}

void Zenith_InputSimulator::StepUntil(bool (*pfnCondition)(), u_int32 uMaxFrames)
{
	for (u_int32 u = 0; u < uMaxFrames; u++)
	{
		if (pfnCondition())
		{
			return;
		}
		StepFrame();
	}
	Zenith_Warning(LOG_CATEGORY_INPUT, "Zenith_InputSimulator::StepUntil reached max frames (%u) without condition met", uMaxFrames);
}

// ========== Pending injection ==========

void Zenith_InputSimulator::QueueInjection(const Zenith_InputEvent& xEvent)
{
	if (s_uPendingInjectionCount >= uMAX_PENDING_INJECTIONS)
	{
		Zenith_Warning(LOG_CATEGORY_INPUT, "Simulator injection queue full (%u); dropping event", uMAX_PENDING_INJECTIONS);
		return;
	}
	s_axPendingInjections[s_uPendingInjectionCount] = xEvent;
	s_axPendingInjections[s_uPendingInjectionCount].m_uSequence = s_uPendingInjectionCount;
	s_uPendingInjectionCount++;
}

void Zenith_InputSimulator::QueueTouchInjection(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY)
{
	Zenith_InputEvent xEvent;
	xEvent.m_eType = eType;
	xEvent.m_iCode = iPointerId;
	xEvent.m_fX = fX;
	xEvent.m_fY = fY;
	xEvent.m_fAnchorX = fX;
	xEvent.m_fAnchorY = fY;
	QueueInjection(xEvent);
}

// ========== Key Simulation ==========

void Zenith_InputSimulator::SimulateKeyDown(Zenith_KeyCode eKey)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		s_abKeyState[eKey] = true;
		s_abKeyPressedThisFrame[eKey] = true;

		Zenith_InputEvent xEvent;
		xEvent.m_eType = (eKey <= ZENITH_MOUSE_BUTTON_LAST) ? INPUT_EVENT_MOUSE_PRESS : INPUT_EVENT_KEY_PRESS;
		xEvent.m_iCode = eKey;
		QueueInjection(xEvent);
	}
}

void Zenith_InputSimulator::SimulateKeyUp(Zenith_KeyCode eKey)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		s_abKeyState[eKey] = false;
		s_abKeyReleasedThisFrame[eKey] = true;

		Zenith_InputEvent xEvent;
		xEvent.m_eType = (eKey <= ZENITH_MOUSE_BUTTON_LAST) ? INPUT_EVENT_MOUSE_RELEASE : INPUT_EVENT_KEY_RELEASE;
		xEvent.m_iCode = eKey;
		QueueInjection(xEvent);
	}
}

void Zenith_InputSimulator::SimulateKeyPress(Zenith_KeyCode eKey)
{
	SimulateKeyDown(eKey);

	Zenith_Assert(s_uAutoReleaseCount < uMAX_AUTO_RELEASE, "Auto-release queue full");
	if (s_uAutoReleaseCount < uMAX_AUTO_RELEASE)
	{
		s_aeAutoReleaseKeys[s_uAutoReleaseCount] = eKey;
		s_uAutoReleaseCount++;
	}
}

void Zenith_InputSimulator::SimulateKeySequence(const Zenith_KeyCode* aeKeys, u_int32 uCount, u_int32 uFramesBetween)
{
	for (u_int32 u = 0; u < uCount; u++)
	{
		SimulateKeyPress(aeKeys[u]);
		StepFrames(1 + uFramesBetween);
	}
}

// ========== Mouse Simulation ==========

void Zenith_InputSimulator::SimulateMousePosition(double fScreenX, double fScreenY)
{
	s_xMousePosition.x = fScreenX;
	s_xMousePosition.y = fScreenY;
}

void Zenith_InputSimulator::SimulateMouseButtonDown(Zenith_KeyCode eButton)
{
	SimulateKeyDown(eButton);
}

void Zenith_InputSimulator::SimulateMouseButtonUp(Zenith_KeyCode eButton)
{
	SimulateKeyUp(eButton);
}

void Zenith_InputSimulator::SimulateMouseClick(double fScreenX, double fScreenY)
{
	SimulateMousePosition(fScreenX, fScreenY);
	SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	StepFrame();
}

void Zenith_InputSimulator::SimulateMouseDrag(double fStartX, double fStartY, double fEndX, double fEndY, u_int32 uDurationFrames)
{
	Zenith_Assert(uDurationFrames > 0, "Drag duration must be > 0");
	if (uDurationFrames == 0)
	{
		uDurationFrames = 1;
	}

	SimulateMousePosition(fStartX, fStartY);
	SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
	StepFrame();

	for (u_int32 u = 1; u <= uDurationFrames; u++)
	{
		double fT = static_cast<double>(u) / static_cast<double>(uDurationFrames);
		double fX = fStartX + (fEndX - fStartX) * fT;
		double fY = fStartY + (fEndY - fStartY) * fT;
		SimulateMousePosition(fX, fY);
		StepFrame();
	}

	SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);
	StepFrame();
}

// ========== Touch Simulation ==========

void Zenith_InputSimulator::SimulateTap(double fScreenX, double fScreenY)
{
	SimulateMouseClick(fScreenX, fScreenY);
}

void Zenith_InputSimulator::SimulateSwipe(double fStartX, double fStartY, double fEndX, double fEndY, u_int32 uDurationFrames)
{
	SimulateMouseDrag(fStartX, fStartY, fEndX, fEndY, uDurationFrames);
}

void Zenith_InputSimulator::SimulateTouchDown(int32_t iPointerId, float fX, float fY)
{
	QueueTouchInjection(INPUT_EVENT_TOUCH_DOWN, iPointerId, fX, fY);
}

void Zenith_InputSimulator::SimulateTouchMove(int32_t iPointerId, float fX, float fY)
{
	QueueTouchInjection(INPUT_EVENT_TOUCH_MOVE, iPointerId, fX, fY);
}

void Zenith_InputSimulator::SimulateTouchUp(int32_t iPointerId, float fX, float fY)
{
	QueueTouchInjection(INPUT_EVENT_TOUCH_UP, iPointerId, fX, fY);
}

void Zenith_InputSimulator::SimulateTouchCancel(int32_t iPointerId, float fX, float fY)
{
	QueueTouchInjection(INPUT_EVENT_TOUCH_CANCEL, iPointerId, fX, fY);
}

// NOTE: SimulateClickOnUIElement was relocated UP to the UI layer
// (Zenith_UI::Zenith_UICanvas::SimulateClickOnUIElement) so this L1 Input TU no
// longer needs to include UI/Zenith_UICanvas.h. The UI-side helper resolves the
// named element to its screen center and calls back DOWN into SimulateMouseClick.

// ========== Gamepad Simulation ==========

namespace
{
	float Zenith_ClampSimulatedAxis(int iAxis, float fValue)
	{
		// Canonical domains, enforced at injection so a test cannot smuggle an
		// out-of-range value past the device layer's guarantees.
		const float fMin = (iAxis > ZENITH_GAMEPAD_AXIS_RIGHT_Y) ? 0.0f : -1.0f;
		if (fValue < fMin) return fMin;
		if (fValue > 1.0f) return 1.0f;
		return fValue;
	}

	bool Zenith_IsSimulatedPadValid(int iGamepad)
	{
		return iGamepad >= 0 && iGamepad < Zenith_Input::MAX_GAMEPADS;
	}

	bool Zenith_IsSimulatedPadButtonValid(int iButton)
	{
		return iButton >= 0 && iButton < Zenith_GamepadSnapshot::iBUTTON_COUNT;
	}
}

void Zenith_InputSimulator::SimulateGamepadConnected(bool bConnected, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad))
	{
		return;
	}

	if (!bConnected && s_abPadConnected[iGamepad])
	{
		// A yanked pad synthesizes the releases it will never send itself, and
		// rests its axes at the canonical zero.
		for (int iButton = 0; iButton < iSIM_PAD_BUTTON_COUNT; iButton++)
		{
			if (!s_aabPadButtons[iGamepad][iButton])
			{
				continue;
			}
			s_aabPadButtons[iGamepad][iButton] = false;
			s_aabPadReleasedThisFrame[iGamepad][iButton] = true;
		}
		for (int iAxis = 0; iAxis < iSIM_PAD_AXIS_COUNT; iAxis++)
		{
			s_aafPadAxes[iGamepad][iAxis] = 0.0f;
		}
	}

	s_abPadConnected[iGamepad] = bConnected;

	Zenith_InputEvent xEvent;
	xEvent.m_eType = bConnected ? INPUT_EVENT_PAD_CONNECT : INPUT_EVENT_PAD_DISCONNECT;
	xEvent.m_iDevice = iGamepad;
	QueueInjection(xEvent);
}

void Zenith_InputSimulator::SimulateGamepadButtonDown(int iButton, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad) || !Zenith_IsSimulatedPadButtonValid(iButton))
	{
		return;
	}
	s_abPadConnected[iGamepad] = true;
	s_aabPadButtons[iGamepad][iButton] = true;
	s_aabPadPressedThisFrame[iGamepad][iButton] = true;

	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_PAD_PRESS;
	xEvent.m_iCode = iButton;
	xEvent.m_iDevice = iGamepad;
	QueueInjection(xEvent);
}

void Zenith_InputSimulator::SimulateGamepadButtonUp(int iButton, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad) || !Zenith_IsSimulatedPadButtonValid(iButton))
	{
		return;
	}
	s_aabPadButtons[iGamepad][iButton] = false;
	s_aabPadReleasedThisFrame[iGamepad][iButton] = true;

	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_PAD_RELEASE;
	xEvent.m_iCode = iButton;
	xEvent.m_iDevice = iGamepad;
	QueueInjection(xEvent);
}

void Zenith_InputSimulator::SimulateGamepadAxis(int iAxis, float fValue, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad))
	{
		return;
	}
	if (iAxis < 0 || iAxis >= iSIM_PAD_AXIS_COUNT)
	{
		return;
	}
	s_abPadConnected[iGamepad] = true;
	s_aafPadAxes[iGamepad][iAxis] = Zenith_ClampSimulatedAxis(iAxis, fValue);
}

void Zenith_InputSimulator::SimulateGamepadStick(int iAxisX, float fX, float fY, int iGamepad)
{
	Zenith_Assert(iAxisX == ZENITH_GAMEPAD_AXIS_LEFT_X || iAxisX == ZENITH_GAMEPAD_AXIS_RIGHT_X,
		"SimulateGamepadStick expects the X axis of a stick pair");
	SimulateGamepadAxis(iAxisX, fX, iGamepad);
	SimulateGamepadAxis(iAxisX + 1, fY, iGamepad);
}

// ========== Key State Manipulation ==========

void Zenith_InputSimulator::SetKeyHeld(Zenith_KeyCode eKey, bool bHeld)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		s_abKeyState[eKey] = bHeld;
	}
}

void Zenith_InputSimulator::ClearHeldKeys()
{
	memset(s_abKeyState, 0, sizeof(s_abKeyState));
}

// ========== State Reset ==========

void Zenith_InputSimulator::ResetAllInputState()
{
	memset(s_abKeyState, 0, sizeof(s_abKeyState));
	memset(s_abKeyPressedThisFrame, 0, sizeof(s_abKeyPressedThisFrame));
	memset(s_abKeyReleasedThisFrame, 0, sizeof(s_abKeyReleasedThisFrame));
	s_xMousePosition = { 0.0, 0.0 };
	s_uAutoReleaseCount = 0;
	s_fMouseWheelDelta = 0.0f;
	memset(s_abPadConnected, 0, sizeof(s_abPadConnected));
	memset(s_aabPadButtons, 0, sizeof(s_aabPadButtons));
	memset(s_aabPadPressedThisFrame, 0, sizeof(s_aabPadPressedThisFrame));
	memset(s_aabPadReleasedThisFrame, 0, sizeof(s_aabPadReleasedThisFrame));
	memset(s_aafPadAxes, 0, sizeof(s_aafPadAxes));
	s_uPendingInjectionCount = 0;
}

// ========== Test Frame Lifecycle ==========

void Zenith_InputSimulator::BeginTestFrame()
{
	memset(s_abKeyPressedThisFrame, 0, sizeof(s_abKeyPressedThisFrame));
	memset(s_abKeyReleasedThisFrame, 0, sizeof(s_abKeyReleasedThisFrame));
	memset(s_aabPadPressedThisFrame, 0, sizeof(s_aabPadPressedThisFrame));
	memset(s_aabPadReleasedThisFrame, 0, sizeof(s_aabPadReleasedThisFrame));
}

void Zenith_InputSimulator::EndTestFrame()
{
	memset(s_abKeyPressedThisFrame, 0, sizeof(s_abKeyPressedThisFrame));
	memset(s_abKeyReleasedThisFrame, 0, sizeof(s_abKeyReleasedThisFrame));
	memset(s_aabPadPressedThisFrame, 0, sizeof(s_aabPadPressedThisFrame));
	memset(s_aabPadReleasedThisFrame, 0, sizeof(s_aabPadReleasedThisFrame));
	// EXT-4: clear wheel delta at frame end so SimulateMouseWheel asserts for
	// exactly one full frame, mirroring SimulateKeyPress semantics.
	s_fMouseWheelDelta = 0.0f;
}

void Zenith_InputSimulator::SimulateMouseWheel(float fDelta)
{
	s_fMouseWheelDelta = fDelta;

	Zenith_InputEvent xEvent;
	xEvent.m_eType = INPUT_EVENT_WHEEL;
	xEvent.m_fY = fDelta;
	QueueInjection(xEvent);
}

float Zenith_InputSimulator::GetMouseWheelDeltaSimulated()
{
	return s_fMouseWheelDelta;
}

// ========== Fixed Dt Override ==========

bool Zenith_InputSimulator::HasFixedDtOverride()
{
	return s_bEnabled && s_bFixedDtEnabled;
}

float Zenith_InputSimulator::GetFixedDt()
{
	return s_fFixedDt;
}

// ========== Query Simulated State ==========

bool Zenith_InputSimulator::IsKeyDownSimulated(Zenith_KeyCode eKey)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		return s_abKeyState[eKey];
	}
	return false;
}

bool Zenith_InputSimulator::WasKeyPressedThisFrameSimulated(Zenith_KeyCode eKey)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		return s_abKeyPressedThisFrame[eKey];
	}
	return false;
}

bool Zenith_InputSimulator::WasKeyReleasedThisFrameSimulated(Zenith_KeyCode eKey)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		return s_abKeyReleasedThisFrame[eKey];
	}
	return false;
}

void Zenith_InputSimulator::GetMousePositionSimulated(Zenith_Maths::Vector2_64& xOut)
{
	xOut = s_xMousePosition;
}

bool Zenith_InputSimulator::IsGamepadConnectedSimulated(int iGamepad)
{
	return Zenith_IsSimulatedPadValid(iGamepad) && s_abPadConnected[iGamepad];
}

bool Zenith_InputSimulator::IsGamepadButtonDownSimulated(int iButton, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad) || !Zenith_IsSimulatedPadButtonValid(iButton)) return false;
	return s_aabPadButtons[iGamepad][iButton];
}

bool Zenith_InputSimulator::WasGamepadButtonPressedThisFrameSimulated(int iButton, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad) || !Zenith_IsSimulatedPadButtonValid(iButton)) return false;
	return s_aabPadPressedThisFrame[iGamepad][iButton];
}

bool Zenith_InputSimulator::WasGamepadButtonReleasedThisFrameSimulated(int iButton, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad) || !Zenith_IsSimulatedPadButtonValid(iButton)) return false;
	return s_aabPadReleasedThisFrame[iGamepad][iButton];
}

float Zenith_InputSimulator::GetGamepadAxisSimulated(int iAxis, int iGamepad)
{
	if (!Zenith_IsSimulatedPadValid(iGamepad)) return 0.0f;
	if (iAxis < 0 || iAxis >= iSIM_PAD_AXIS_COUNT) return 0.0f;
	return s_aafPadAxes[iGamepad][iAxis];
}

// ========== Frame Lifecycle ==========

void Zenith_InputSimulator::ProcessAutoReleases()
{
	// Clear per-frame edge flags from previous frame
	memset(s_abKeyPressedThisFrame, 0, sizeof(s_abKeyPressedThisFrame));
	memset(s_abKeyReleasedThisFrame, 0, sizeof(s_abKeyReleasedThisFrame));
	memset(s_aabPadPressedThisFrame, 0, sizeof(s_aabPadPressedThisFrame));
	memset(s_aabPadReleasedThisFrame, 0, sizeof(s_aabPadReleasedThisFrame));

	// Release keys that were queued for auto-release. The release EDGE lands on
	// this frame, so a SimulateKeyPress produces a press on frame N and a
	// release on frame N+1 -- the same two-frame shape a real tap has.
	for (u_int32 u = 0; u < s_uAutoReleaseCount; u++)
	{
		Zenith_KeyCode eKey = s_aeAutoReleaseKeys[u];
		if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
		{
			s_abKeyState[eKey] = false;
			s_abKeyReleasedThisFrame[eKey] = true;

			Zenith_InputEvent xEvent;
			xEvent.m_eType = (eKey <= ZENITH_MOUSE_BUTTON_LAST) ? INPUT_EVENT_MOUSE_RELEASE : INPUT_EVENT_KEY_RELEASE;
			xEvent.m_iCode = eKey;
			QueueInjection(xEvent);
		}
	}
	s_uAutoReleaseCount = 0;

	// NOTE: wheel-delta is NOT cleared here. Doing so would defeat the
	// pattern where SimulateMouseWheel(f) is called in the test's Setup or
	// in Step, and the consumer (orbit camera etc.) reads via
	// g_xEngine.Input().GetMouseWheelDelta the same frame — BeginFrame runs
	// before either, so clearing here would zero the value before the
	// consumer ever sees it. The wheel is instead cleared from
	// EndOfFrameTickComplete() below, invoked by Zenith_Core::Zenith_MainLoop
	// AFTER the scene/script Update phase has consumed the wheel.
}

void Zenith_InputSimulator::EndOfFrameTickComplete()
{
	// Clear simulator-supplied wheel delta. Called once per main-loop
	// iteration AFTER the game systems update phase has had a chance to
	// read it via g_xEngine.Input().GetMouseWheelDelta. The same call also
	// covers the Setup-before-tick-0 case: Setup's SimulateMouseWheel
	// survives the first BeginFrame (since ProcessAutoReleases no longer
	// clears), the test's Step + game systems read it within tick 0,
	// and this clear at the end of tick 0 zeros it before tick 1 starts.
	s_fMouseWheelDelta = 0.0f;
}

#endif // ZENITH_INPUT_SIMULATOR
