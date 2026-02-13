#pragma once

#include "Maths/Zenith_Maths.h"

enum Zenith_SwipeDirection : uint8_t
{
	ZENITH_SWIPE_NONE = 0,
	ZENITH_SWIPE_UP,
	ZENITH_SWIPE_DOWN,
	ZENITH_SWIPE_LEFT,
	ZENITH_SWIPE_RIGHT
};

// Touch gesture detection system
// Works through mouse emulation - on Android, touch events are automatically
// translated to mouse button presses and position updates.
// On desktop, mouse click+drag is detected as swipe, click as tap.
class Zenith_TouchInput
{
public:
	// Call once per frame after Zenith_Input::BeginFrame()
	static void Update();

	// Tap: short press with minimal movement
	static bool WasTapThisFrame();
	static Zenith_Maths::Vector2 GetTapPosition();

	// Swipe: drag exceeding distance threshold in a dominant direction
	static bool WasSwipeThisFrame();
	static Zenith_SwipeDirection GetSwipeDirection();
	static Zenith_Maths::Vector2 GetSwipeStartPosition();
	static float GetSwipeDistance();

	// Configuration
	static void SetSwipeThreshold(float fPixels);
	static void SetTapMaxMovement(float fPixels);
	static void SetTapMaxDuration(float fSeconds);

	// Current touch/mouse state
	static bool IsTouchDown();
	static Zenith_Maths::Vector2 GetTouchPosition();
};
