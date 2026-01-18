#include "Zenith.h"

#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"

#ifdef ZENITH_TOOLS
#include "UnitTests/Zenith_MockInput.h"
#endif

static std::unordered_set<Zenith_KeyCode> s_xFrameKeyPresses;
static Zenith_Maths::Vector2_64 s_xLastMousePosition = { 0.0, 0.0 };
static Zenith_Maths::Vector2_64 s_xMouseDelta = { 0.0, 0.0 };
static bool s_bFirstFrame = true;

// Gamepad state tracking for "button pressed this frame" detection
static constexpr int MAX_GAMEPADS = 4;
static GLFWgamepadstate s_xLastGamepadState[MAX_GAMEPADS] = {};
static GLFWgamepadstate s_xCurrentGamepadState[MAX_GAMEPADS] = {};
static bool s_bGamepadStateInitialized[MAX_GAMEPADS] = { false };

void Zenith_Input::BeginFrame()
{
	s_xFrameKeyPresses.clear();

	// Calculate mouse delta
	Zenith_Maths::Vector2_64 xCurrentMousePos;
	Zenith_Window::GetInstance()->GetMousePosition(xCurrentMousePos);

	if (s_bFirstFrame)
	{
		s_xMouseDelta = { 0.0, 0.0 };
		s_bFirstFrame = false;
	}
	else
	{
		s_xMouseDelta.x = xCurrentMousePos.x - s_xLastMousePosition.x;
		s_xMouseDelta.y = xCurrentMousePos.y - s_xLastMousePosition.y;
	}

	s_xLastMousePosition = xCurrentMousePos;

	// Update gamepad state for all connected gamepads
	for (int i = 0; i < MAX_GAMEPADS; i++)
	{
		// Copy current state to last state
		s_xLastGamepadState[i] = s_xCurrentGamepadState[i];

		// Get new current state
		int iJoystickID = GLFW_JOYSTICK_1 + i;
		if (glfwJoystickIsGamepad(iJoystickID))
		{
			glfwGetGamepadState(iJoystickID, &s_xCurrentGamepadState[i]);
			s_bGamepadStateInitialized[i] = true;
		}
		else
		{
			// Clear state if gamepad disconnected
			if (s_bGamepadStateInitialized[i])
			{
				memset(&s_xCurrentGamepadState[i], 0, sizeof(GLFWgamepadstate));
				s_bGamepadStateInitialized[i] = false;
			}
		}
	}
}

void Zenith_Input::KeyPressedCallback(Zenith_KeyCode iKey)
{
	s_xFrameKeyPresses.insert(iKey);
}

void Zenith_Input::MouseButtonPressedCallback(Zenith_KeyCode iKey)
{
	s_xFrameKeyPresses.insert(iKey);
}

void Zenith_Input::GetMousePosition(Zenith_Maths::Vector2_64& xOut)
{
#ifdef ZENITH_TOOLS
	if (Zenith_MockInput::IsMockingEnabled())
	{
		Zenith_MockInput::GetMousePositionMocked(xOut);
		return;
	}
#endif
	Zenith_Window::GetInstance()->GetMousePosition(xOut);
}

void Zenith_Input::GetMouseDelta(Zenith_Maths::Vector2_64& xOut)
{
	xOut = s_xMouseDelta;
}

bool Zenith_Input::IsKeyDown(Zenith_KeyCode iKey)
{
#ifdef ZENITH_TOOLS
	if (Zenith_MockInput::IsMockingEnabled())
	{
		return Zenith_MockInput::IsKeyHeldMocked(iKey);
	}
#endif
	return Zenith_Window::GetInstance()->IsKeyDown(iKey);
}

bool Zenith_Input::WasKeyPressedThisFrame(Zenith_KeyCode iKey)
{
#ifdef ZENITH_TOOLS
	if (Zenith_MockInput::IsMockingEnabled())
	{
		return Zenith_MockInput::WasKeyPressedThisFrameMocked(iKey);
	}
#endif
	return s_xFrameKeyPresses.find(iKey) != s_xFrameKeyPresses.end();
}

// ========== Gamepad Functions ==========

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
	if (!s_bGamepadStateInitialized[iGamepad]) return false;

	return s_xCurrentGamepadState[iGamepad].buttons[iButton] == GLFW_PRESS;
}

bool Zenith_Input::WasGamepadButtonPressedThisFrame(int iButton, int iGamepad)
{
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return false;
	if (iButton < 0 || iButton > GLFW_GAMEPAD_BUTTON_LAST) return false;
	if (!s_bGamepadStateInitialized[iGamepad]) return false;

	// Button is pressed this frame if it's down now but wasn't down last frame
	bool bDownNow = s_xCurrentGamepadState[iGamepad].buttons[iButton] == GLFW_PRESS;
	bool bDownBefore = s_xLastGamepadState[iGamepad].buttons[iButton] == GLFW_PRESS;
	return bDownNow && !bDownBefore;
}

float Zenith_Input::GetGamepadAxis(int iAxis, int iGamepad)
{
	if (iGamepad < 0 || iGamepad >= MAX_GAMEPADS) return 0.0f;
	if (iAxis < 0 || iAxis > GLFW_GAMEPAD_AXIS_LAST) return 0.0f;
	if (!s_bGamepadStateInitialized[iGamepad]) return 0.0f;

	float fValue = s_xCurrentGamepadState[iGamepad].axes[iAxis];

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
