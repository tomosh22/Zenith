#include "Zenith.h"

#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputImpl.h"

#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

// Phase 5.5a: per-frame input state lives on Zenith_InputImpl held by
// Zenith_Engine. Every former file-static (g_xEngine.Input().m_xFrameKeyPresses /
// g_xEngine.Input().m_xLastMousePosition / g_xEngine.Input().m_xMouseDelta / g_xEngine.Input().m_fMouseWheelDelta / g_xEngine.Input().m_bFirstFrame
// / g_xEngine.Input().m_bSimWasEnabledLastFrame / g_xEngine.Input().m_xLastGamepadState / g_xEngine.Input().m_xCurrentGamepadState
// / g_xEngine.Input().m_bGamepadStateInitialized) is reached via g_xEngine.Input().m_xXxx.
#ifdef ZENITH_WINDOWS
static constexpr int MAX_GAMEPADS = Zenith_InputImpl::MAX_GAMEPADS;
#endif

void Zenith_Input::BeginFrame()
{
#ifdef ZENITH_INPUT_SIMULATOR
	if (Zenith_InputSimulator::IsEnabled())
	{
		Zenith_InputSimulator::ProcessAutoReleases();

		// Mouse delta must still be tracked when simulator is driving input —
		// otherwise GetMouseDelta() returns a stale value and code that drives
		// camera-look from the delta (e.g. RenderTest_FollowCamera) can't be
		// exercised via SimulateMousePosition.
		Zenith_Maths::Vector2_64 xCurrentMousePos;
		Zenith_InputSimulator::GetMousePositionSimulated(xCurrentMousePos);

		// Skip delta on the first simulator frame OR the frame we transitioned
		// from real input — the saved last-position is from a different domain
		// and would produce a single huge spurious delta.
		if (g_xEngine.Input().m_bFirstFrame || !g_xEngine.Input().m_bSimWasEnabledLastFrame)
		{
			g_xEngine.Input().m_xMouseDelta = { 0.0, 0.0 };
			g_xEngine.Input().m_bFirstFrame = false;
		}
		else
		{
			g_xEngine.Input().m_xMouseDelta.x = xCurrentMousePos.x - g_xEngine.Input().m_xLastMousePosition.x;
			g_xEngine.Input().m_xMouseDelta.y = xCurrentMousePos.y - g_xEngine.Input().m_xLastMousePosition.y;
		}
		g_xEngine.Input().m_xLastMousePosition = xCurrentMousePos;
		g_xEngine.Input().m_bSimWasEnabledLastFrame = true;
		return;
	}
	// Simulator just disabled this frame — skip one frame's delta to avoid the
	// (sim-position) -> (real-cursor-position) jump.
	const bool bJustLeftSimMode = g_xEngine.Input().m_bSimWasEnabledLastFrame;
	g_xEngine.Input().m_bSimWasEnabledLastFrame = false;
#endif

	g_xEngine.Input().m_xFrameKeyPresses.clear();
	// Reset wheel accumulator BEFORE poll — GLFW scroll callbacks accumulate
	// during this BeginFrame's poll cycle; game code reads after.
	g_xEngine.Input().m_fMouseWheelDelta = 0.0f;

	// Calculate mouse delta
	Zenith_Maths::Vector2_64 xCurrentMousePos;
	Zenith_Window::GetInstance()->GetMousePosition(xCurrentMousePos);

#ifdef ZENITH_INPUT_SIMULATOR
	if (g_xEngine.Input().m_bFirstFrame || bJustLeftSimMode)
#else
	if (g_xEngine.Input().m_bFirstFrame)
#endif
	{
		g_xEngine.Input().m_xMouseDelta = { 0.0, 0.0 };
		g_xEngine.Input().m_bFirstFrame = false;
	}
	else
	{
		g_xEngine.Input().m_xMouseDelta.x = xCurrentMousePos.x - g_xEngine.Input().m_xLastMousePosition.x;
		g_xEngine.Input().m_xMouseDelta.y = xCurrentMousePos.y - g_xEngine.Input().m_xLastMousePosition.y;
	}

	g_xEngine.Input().m_xLastMousePosition = xCurrentMousePos;

#ifdef ZENITH_WINDOWS
	// Update gamepad state for all connected gamepads
	for (int i = 0; i < MAX_GAMEPADS; i++)
	{
		// Copy current state to last state
		g_xEngine.Input().m_xLastGamepadState[i] = g_xEngine.Input().m_xCurrentGamepadState[i];

		// Get new current state
		int iJoystickID = GLFW_JOYSTICK_1 + i;
		if (glfwJoystickIsGamepad(iJoystickID))
		{
			glfwGetGamepadState(iJoystickID, &g_xEngine.Input().m_xCurrentGamepadState[i]);
			g_xEngine.Input().m_bGamepadStateInitialized[i] = true;
		}
		else
		{
			// Clear state if gamepad disconnected
			if (g_xEngine.Input().m_bGamepadStateInitialized[i])
			{
				memset(&g_xEngine.Input().m_xCurrentGamepadState[i], 0, sizeof(GLFWgamepadstate));
				g_xEngine.Input().m_bGamepadStateInitialized[i] = false;
			}
		}
	}
#endif
}

void Zenith_Input::KeyPressedCallback(Zenith_KeyCode iKey)
{
	g_xEngine.Input().m_xFrameKeyPresses.insert(iKey);
}

void Zenith_Input::MouseButtonPressedCallback(Zenith_KeyCode iKey)
{
	g_xEngine.Input().m_xFrameKeyPresses.insert(iKey);
}

void Zenith_Input::MouseWheelCallback(double /*fXOffset*/, double fYOffset)
{
	// Accumulate within the frame — multiple scroll callbacks may fire
	// between two BeginFrame calls.
	g_xEngine.Input().m_fMouseWheelDelta += static_cast<float>(fYOffset);
}

float Zenith_Input::GetMouseWheelDelta()
{
#ifdef ZENITH_INPUT_SIMULATOR
	if (Zenith_InputSimulator::IsEnabled())
	{
		// Simulator path lives entirely in the simulator class — that lets
		// SimulateMouseWheel inject a value that survives a full frame
		// without the GLFW callback path interfering.
		return Zenith_InputSimulator::GetMouseWheelDeltaSimulated();
	}
#endif
	return g_xEngine.Input().m_fMouseWheelDelta;
}

void Zenith_Input::GetMousePosition(Zenith_Maths::Vector2_64& xOut)
{
#ifdef ZENITH_INPUT_SIMULATOR
	if (Zenith_InputSimulator::IsEnabled())
	{
		Zenith_InputSimulator::GetMousePositionSimulated(xOut);
		return;
	}
#endif
	Zenith_Window::GetInstance()->GetMousePosition(xOut);
}

void Zenith_Input::GetMouseDelta(Zenith_Maths::Vector2_64& xOut)
{
	xOut = g_xEngine.Input().m_xMouseDelta;
}

bool Zenith_Input::IsKeyDown(Zenith_KeyCode iKey)
{
#ifdef ZENITH_INPUT_SIMULATOR
	if (Zenith_InputSimulator::IsEnabled())
	{
		return Zenith_InputSimulator::IsKeyDownSimulated(iKey);
	}
#endif
	return Zenith_Window::GetInstance()->IsKeyDown(iKey);
}

bool Zenith_Input::WasKeyPressedThisFrame(Zenith_KeyCode iKey)
{
#ifdef ZENITH_INPUT_SIMULATOR
	if (Zenith_InputSimulator::IsEnabled())
	{
		return Zenith_InputSimulator::WasKeyPressedThisFrameSimulated(iKey);
	}
#endif
	return g_xEngine.Input().m_xFrameKeyPresses.find(iKey) != g_xEngine.Input().m_xFrameKeyPresses.end();
}

// ========== Gamepad Functions ==========

#ifdef ZENITH_WINDOWS

bool Zenith_Input::IsGamepadConnected(int iGamepad)
{
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	int iJoystickID = GLFW_JOYSTICK_1 + iGamepad;
	return glfwJoystickIsGamepad(iJoystickID) == GLFW_TRUE;
}

bool Zenith_Input::IsGamepadButtonDown(int iButton, int iGamepad)
{
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	if (iButton < 0 || iButton > GLFW_GAMEPAD_BUTTON_LAST) return false;
	if (!g_xEngine.Input().m_bGamepadStateInitialized[iGamepad]) return false;

	return g_xEngine.Input().m_xCurrentGamepadState[iGamepad].buttons[iButton] == GLFW_PRESS;
}

bool Zenith_Input::WasGamepadButtonPressedThisFrame(int iButton, int iGamepad)
{
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	if (iButton < 0 || iButton > GLFW_GAMEPAD_BUTTON_LAST) return false;
	if (!g_xEngine.Input().m_bGamepadStateInitialized[iGamepad]) return false;

	// Button is pressed this frame if it's down now but wasn't down last frame
	bool bDownNow = g_xEngine.Input().m_xCurrentGamepadState[iGamepad].buttons[iButton] == GLFW_PRESS;
	bool bDownBefore = g_xEngine.Input().m_xLastGamepadState[iGamepad].buttons[iButton] == GLFW_PRESS;
	return bDownNow && !bDownBefore;
}

float Zenith_Input::GetGamepadAxis(int iAxis, int iGamepad)
{
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return 0.0f;
	if (iAxis < 0 || iAxis > GLFW_GAMEPAD_AXIS_LAST) return 0.0f;
	if (!g_xEngine.Input().m_bGamepadStateInitialized[iGamepad]) return 0.0f;

	float fValue = g_xEngine.Input().m_xCurrentGamepadState[iGamepad].axes[iAxis];

	// Apply deadzone for stick axes (not triggers)
	if (iAxis <= GLFW_GAMEPAD_AXIS_RIGHT_Y)
	{
		if (std::abs(fValue) < GAMEPAD_DEADZONE)
			return 0.0f;
	}

	return fValue;
}

void Zenith_Input::GetGamepadLeftStick(float& fX, float& fY, int iGamepad)
{
	fX = GetGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X, iGamepad);
	fY = GetGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y, iGamepad);
}

void Zenith_Input::GetGamepadRightStick(float& fX, float& fY, int iGamepad)
{
	fX = GetGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X, iGamepad);
	fY = GetGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y, iGamepad);
}

float Zenith_Input::GetGamepadLeftTrigger(int iGamepad)
{
	// Triggers return -1 to 1, normalize to 0 to 1
	float fValue = GetGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, iGamepad);
	return (fValue + 1.0f) * 0.5f;
}

float Zenith_Input::GetGamepadRightTrigger(int iGamepad)
{
	// Triggers return -1 to 1, normalize to 0 to 1
	float fValue = GetGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, iGamepad);
	return (fValue + 1.0f) * 0.5f;
}

#else

bool Zenith_Input::IsGamepadConnected(int) { return false; }
bool Zenith_Input::IsGamepadButtonDown(int, int) { return false; }
bool Zenith_Input::WasGamepadButtonPressedThisFrame(int, int) { return false; }
float Zenith_Input::GetGamepadAxis(int, int) { return 0.0f; }
void Zenith_Input::GetGamepadLeftStick(float& fX, float& fY, int) { fX = 0.0f; fY = 0.0f; }
void Zenith_Input::GetGamepadRightStick(float& fX, float& fY, int) { fX = 0.0f; fY = 0.0f; }
float Zenith_Input::GetGamepadLeftTrigger(int) { return 0.0f; }
float Zenith_Input::GetGamepadRightTrigger(int) { return 0.0f; }

#endif
