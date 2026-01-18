#pragma once

#ifdef ZENITH_TOOLS

#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include <unordered_set>

class Zenith_MockInput
{
public:
	// Enable/disable mock mode
	static void EnableMocking(bool bEnable);
	static bool IsMockingEnabled();

	// Simulate a key press (adds to "pressed this frame" set)
	static void SimulateKeyPress(Zenith_KeyCode eKey);

	// Simulate mouse button press
	static void SimulateMousePress(Zenith_KeyCode eMouseButton);

	// Set mouse position for mocking
	static void SetMousePosition(const Zenith_Maths::Vector2_64& xPos);

	// Set keys to be held down (for IsKeyDown checks)
	static void SetKeyHeld(Zenith_KeyCode eKey, bool bHeld);
	static void SetKeysHeld(const std::unordered_set<Zenith_KeyCode>& xKeys);
	static void ClearHeldKeys();

	// Query mock state (called by Zenith_Input when mocking is enabled)
	static bool IsKeyHeldMocked(Zenith_KeyCode eKey);
	static bool WasKeyPressedThisFrameMocked(Zenith_KeyCode eKey);
	static void GetMousePositionMocked(Zenith_Maths::Vector2_64& xOut);

	// Frame lifecycle for tests
	static void BeginTestFrame();
	static void EndTestFrame();

	// Reset all mock state
	static void Reset();

private:
	static bool s_bMockingEnabled;
	static std::unordered_set<Zenith_KeyCode> s_xMockedHeldKeys;
	static std::unordered_set<Zenith_KeyCode> s_xMockedFrameKeyPresses;
	static Zenith_Maths::Vector2_64 s_xMockedMousePosition;
};

#endif // ZENITH_TOOLS
