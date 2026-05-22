#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Input/Zenith_InputSimulator.h"
#include "UI/Zenith_UICanvas.h"
#include "Zenith_Core.h"

bool Zenith_InputSimulator::s_bEnabled = false;
bool Zenith_InputSimulator::s_bFixedDtEnabled = false;
float Zenith_InputSimulator::s_fFixedDt = 0.f;
bool Zenith_InputSimulator::s_abKeyState[uMAX_SIMULATED_KEYS] = {};
bool Zenith_InputSimulator::s_abKeyPressedThisFrame[uMAX_SIMULATED_KEYS] = {};
Zenith_Maths::Vector2_64 Zenith_InputSimulator::s_xMousePosition = { 0.0, 0.0 };
Zenith_KeyCode Zenith_InputSimulator::s_aeAutoReleaseKeys[uMAX_AUTO_RELEASE] = {};
u_int32 Zenith_InputSimulator::s_uAutoReleaseCount = 0;
float Zenith_InputSimulator::s_fMouseWheelDelta = 0.0f;

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

// ========== Key Simulation ==========

void Zenith_InputSimulator::SimulateKeyDown(Zenith_KeyCode eKey)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		s_abKeyState[eKey] = true;
		s_abKeyPressedThisFrame[eKey] = true;
	}
}

void Zenith_InputSimulator::SimulateKeyUp(Zenith_KeyCode eKey)
{
	if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
	{
		s_abKeyState[eKey] = false;
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

void Zenith_InputSimulator::SimulateClickOnUIElement(const char* szElementName)
{
	Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
	Zenith_Assert(pxCanvas, "No primary canvas for SimulateClickOnUIElement");
	Zenith_UI::Zenith_UIElement* pxElement = pxCanvas->FindElement(szElementName);
	Zenith_Assert(pxElement, "UI element not found: %s", szElementName);

	Zenith_Maths::Vector4 xBounds = pxElement->GetScreenBounds();
	double fCenterX = static_cast<double>((xBounds.x + xBounds.z) * 0.5f);
	double fCenterY = static_cast<double>((xBounds.y + xBounds.w) * 0.5f);

	SimulateMouseClick(fCenterX, fCenterY);
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
	s_xMousePosition = { 0.0, 0.0 };
	s_uAutoReleaseCount = 0;
	s_fMouseWheelDelta = 0.0f;
}

// ========== Test Frame Lifecycle ==========

void Zenith_InputSimulator::BeginTestFrame()
{
	memset(s_abKeyPressedThisFrame, 0, sizeof(s_abKeyPressedThisFrame));
}

void Zenith_InputSimulator::EndTestFrame()
{
	memset(s_abKeyPressedThisFrame, 0, sizeof(s_abKeyPressedThisFrame));
	// EXT-4: clear wheel delta at frame end so SimulateMouseWheel asserts for
	// exactly one full frame, mirroring SimulateKeyPress semantics.
	s_fMouseWheelDelta = 0.0f;
}

void Zenith_InputSimulator::SimulateMouseWheel(float fDelta)
{
	s_fMouseWheelDelta = fDelta;
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

void Zenith_InputSimulator::GetMousePositionSimulated(Zenith_Maths::Vector2_64& xOut)
{
	xOut = s_xMousePosition;
}

// ========== Frame Lifecycle ==========

void Zenith_InputSimulator::ProcessAutoReleases()
{
	// Clear per-frame pressed flags from previous frame
	memset(s_abKeyPressedThisFrame, 0, sizeof(s_abKeyPressedThisFrame));

	// Release keys that were queued for auto-release
	for (u_int32 u = 0; u < s_uAutoReleaseCount; u++)
	{
		Zenith_KeyCode eKey = s_aeAutoReleaseKeys[u];
		if (eKey >= 0 && eKey < static_cast<Zenith_KeyCode>(uMAX_SIMULATED_KEYS))
		{
			s_abKeyState[eKey] = false;
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
