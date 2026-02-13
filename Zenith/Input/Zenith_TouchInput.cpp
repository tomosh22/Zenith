#include "Zenith.h"

#include "Input/Zenith_TouchInput.h"
#include "Input/Zenith_Input.h"
#include "Core/Zenith_Core.h"

// ============================================================================
// State
// ============================================================================
static bool s_bTouchActive = false;
static Zenith_Maths::Vector2 s_xTouchStartPos = { 0.f, 0.f };
static float s_fTouchStartTime = 0.f;
static bool s_bWasTouchDownLastFrame = false;

// Per-frame events (reset each Update)
static bool s_bTapThisFrame = false;
static bool s_bSwipeThisFrame = false;
static Zenith_SwipeDirection s_eSwipeDirection = ZENITH_SWIPE_NONE;
static Zenith_Maths::Vector2 s_xTapPosition = { 0.f, 0.f };
static Zenith_Maths::Vector2 s_xSwipeStartPos = { 0.f, 0.f };
static float s_fSwipeDistance = 0.f;

// Current position
static Zenith_Maths::Vector2 s_xCurrentTouchPos = { 0.f, 0.f };
static bool s_bCurrentlyDown = false;

// Thresholds
static float s_fSwipeThreshold = 30.f;
static float s_fTapMaxMovement = 15.f;
static float s_fTapMaxDuration = 0.3f;

// ============================================================================
// Update
// ============================================================================
void Zenith_TouchInput::Update()
{
	// Reset per-frame events
	s_bTapThisFrame = false;
	s_bSwipeThisFrame = false;
	s_eSwipeDirection = ZENITH_SWIPE_NONE;
	s_fSwipeDistance = 0.f;

	// Read current mouse/touch state
	bool bDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	Zenith_Maths::Vector2_64 xPos64;
	Zenith_Input::GetMousePosition(xPos64);
	s_xCurrentTouchPos = Zenith_Maths::Vector2(static_cast<float>(xPos64.x), static_cast<float>(xPos64.y));
	s_bCurrentlyDown = bDown;

	float fCurrentTime = Zenith_Core::GetTimePassed();

	if (bDown && !s_bWasTouchDownLastFrame)
	{
		// Touch just started
		s_bTouchActive = true;
		s_xTouchStartPos = s_xCurrentTouchPos;
		s_fTouchStartTime = fCurrentTime;
	}
	else if (!bDown && s_bWasTouchDownLastFrame && s_bTouchActive)
	{
		// Touch just ended - classify gesture
		float fDeltaX = s_xCurrentTouchPos.x - s_xTouchStartPos.x;
		float fDeltaY = s_xCurrentTouchPos.y - s_xTouchStartPos.y;
		float fDistance = std::sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);
		float fDuration = fCurrentTime - s_fTouchStartTime;

		if (fDistance < s_fTapMaxMovement && fDuration < s_fTapMaxDuration)
		{
			// Tap detected
			s_bTapThisFrame = true;
			s_xTapPosition = s_xTouchStartPos;
		}
		else if (fDistance >= s_fSwipeThreshold)
		{
			// Swipe detected - determine dominant direction
			s_bSwipeThisFrame = true;
			s_xSwipeStartPos = s_xTouchStartPos;
			s_fSwipeDistance = fDistance;

			float fAbsDeltaX = std::abs(fDeltaX);
			float fAbsDeltaY = std::abs(fDeltaY);

			if (fAbsDeltaX > fAbsDeltaY)
			{
				s_eSwipeDirection = (fDeltaX > 0.f) ? ZENITH_SWIPE_RIGHT : ZENITH_SWIPE_LEFT;
			}
			else
			{
				s_eSwipeDirection = (fDeltaY > 0.f) ? ZENITH_SWIPE_DOWN : ZENITH_SWIPE_UP;
			}
		}

		s_bTouchActive = false;
	}

	s_bWasTouchDownLastFrame = bDown;
}

// ============================================================================
// Tap
// ============================================================================
bool Zenith_TouchInput::WasTapThisFrame()
{
	return s_bTapThisFrame;
}

Zenith_Maths::Vector2 Zenith_TouchInput::GetTapPosition()
{
	return s_xTapPosition;
}

// ============================================================================
// Swipe
// ============================================================================
bool Zenith_TouchInput::WasSwipeThisFrame()
{
	return s_bSwipeThisFrame;
}

Zenith_SwipeDirection Zenith_TouchInput::GetSwipeDirection()
{
	return s_eSwipeDirection;
}

Zenith_Maths::Vector2 Zenith_TouchInput::GetSwipeStartPosition()
{
	return s_xSwipeStartPos;
}

float Zenith_TouchInput::GetSwipeDistance()
{
	return s_fSwipeDistance;
}

// ============================================================================
// Configuration
// ============================================================================
void Zenith_TouchInput::SetSwipeThreshold(float fPixels)
{
	s_fSwipeThreshold = fPixels;
}

void Zenith_TouchInput::SetTapMaxMovement(float fPixels)
{
	s_fTapMaxMovement = fPixels;
}

void Zenith_TouchInput::SetTapMaxDuration(float fSeconds)
{
	s_fTapMaxDuration = fSeconds;
}

// ============================================================================
// State Queries
// ============================================================================
bool Zenith_TouchInput::IsTouchDown()
{
	return s_bCurrentlyDown;
}

Zenith_Maths::Vector2 Zenith_TouchInput::GetTouchPosition()
{
	return s_xCurrentTouchPos;
}
