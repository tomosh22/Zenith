#pragma once

#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include <unordered_set>

#ifdef ZENITH_WINDOWS
#include "GLFW/glfw3.h"
#endif

// Phase 5.5a: per-Engine input state. The 9 file-statics on the
// Zenith_Input facade move here. The static facade itself stays as is;
// method bodies read/write through g_xEngine.Input().m_xXxx.
//
// Zenith_InputSimulator state stays static by design (test/automation
// helper, documented carve-out in the refactor plan).
class Zenith_InputImpl
{
public:
	Zenith_InputImpl() = default;
	~Zenith_InputImpl() = default;

	Zenith_InputImpl(const Zenith_InputImpl&) = delete;
	Zenith_InputImpl& operator=(const Zenith_InputImpl&) = delete;

	// Keys pressed THIS frame (cleared every BeginFrame). std::unordered_set
	// chosen by the original implementation; preserved as-is.
	std::unordered_set<Zenith_KeyCode> m_xFrameKeyPresses;

	// Mouse state.
	Zenith_Maths::Vector2_64 m_xLastMousePosition = { 0.0, 0.0 };
	Zenith_Maths::Vector2_64 m_xMouseDelta        = { 0.0, 0.0 };
	float                    m_fMouseWheelDelta   = 0.0f;
	bool                     m_bFirstFrame        = true;

#ifdef ZENITH_INPUT_SIMULATOR
	// Simulator/real-input transition guard. Without it, switching domains
	// mid-run produces a single spurious huge mouse delta on the changeover
	// frame because the saved last-position is from the wrong domain.
	bool                     m_bSimWasEnabledLastFrame = false;
#endif

#ifdef ZENITH_WINDOWS
	// Gamepad state tracking for "button pressed this frame" detection.
	// MAX_GAMEPADS matches the original file-static constant.
	static constexpr int     MAX_GAMEPADS = 4;
	GLFWgamepadstate         m_xLastGamepadState[MAX_GAMEPADS]    = {};
	GLFWgamepadstate         m_xCurrentGamepadState[MAX_GAMEPADS] = {};
	bool                     m_bGamepadStateInitialized[MAX_GAMEPADS] = { false };
#endif
};
