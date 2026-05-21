#include "Zenith.h"

#include "Input/Zenith_TouchInputImpl.h"
#include "Input/Zenith_TouchInputImpl.h"
#include "Input/Zenith_Input.h"

// Phase 5.5b: touch-gesture state lives on Zenith_TouchInputImpl held by
// Zenith_Engine. All former file-statics are now reached via
// g_xEngine.Touch().m_xXxx.

// ============================================================================
// Update
// ============================================================================
void Zenith_TouchInputImpl::Update()
{
	// Reset per-frame events
	g_xEngine.Touch().m_bTapThisFrame = false;
	g_xEngine.Touch().m_bSwipeThisFrame = false;
	g_xEngine.Touch().m_eSwipeDirection = ZENITH_SWIPE_NONE;
	g_xEngine.Touch().m_fSwipeDistance = 0.f;

	// Read current mouse/touch state
	bool bDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	Zenith_Maths::Vector2_64 xPos64;
	Zenith_Input::GetMousePosition(xPos64);
	g_xEngine.Touch().m_xCurrentTouchPos = Zenith_Maths::Vector2(static_cast<float>(xPos64.x), static_cast<float>(xPos64.y));
	g_xEngine.Touch().m_bCurrentlyDown = bDown;

	float fCurrentTime = g_xEngine.Frame().GetTimePassed();

	if (bDown && !g_xEngine.Touch().m_bWasTouchDownLastFrame)
	{
		// Touch just started
		g_xEngine.Touch().m_bTouchActive = true;
		g_xEngine.Touch().m_xTouchStartPos = g_xEngine.Touch().m_xCurrentTouchPos;
		g_xEngine.Touch().m_fTouchStartTime = fCurrentTime;
	}
	else if (!bDown && g_xEngine.Touch().m_bWasTouchDownLastFrame && g_xEngine.Touch().m_bTouchActive)
	{
		// Touch just ended - classify gesture
		float fDeltaX = g_xEngine.Touch().m_xCurrentTouchPos.x - g_xEngine.Touch().m_xTouchStartPos.x;
		float fDeltaY = g_xEngine.Touch().m_xCurrentTouchPos.y - g_xEngine.Touch().m_xTouchStartPos.y;
		float fDistance = std::sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);
		float fDuration = fCurrentTime - g_xEngine.Touch().m_fTouchStartTime;

		if (fDistance < g_xEngine.Touch().m_fTapMaxMovement && fDuration < g_xEngine.Touch().m_fTapMaxDuration)
		{
			// Tap detected
			g_xEngine.Touch().m_bTapThisFrame = true;
			g_xEngine.Touch().m_xTapPosition = g_xEngine.Touch().m_xTouchStartPos;
		}
		else if (fDistance >= g_xEngine.Touch().m_fSwipeThreshold)
		{
			// Swipe detected - determine dominant direction
			g_xEngine.Touch().m_bSwipeThisFrame = true;
			g_xEngine.Touch().m_xSwipeStartPos = g_xEngine.Touch().m_xTouchStartPos;
			g_xEngine.Touch().m_fSwipeDistance = fDistance;

			float fAbsDeltaX = std::abs(fDeltaX);
			float fAbsDeltaY = std::abs(fDeltaY);

			if (fAbsDeltaX > fAbsDeltaY)
			{
				g_xEngine.Touch().m_eSwipeDirection = (fDeltaX > 0.f) ? ZENITH_SWIPE_RIGHT : ZENITH_SWIPE_LEFT;
			}
			else
			{
				g_xEngine.Touch().m_eSwipeDirection = (fDeltaY > 0.f) ? ZENITH_SWIPE_DOWN : ZENITH_SWIPE_UP;
			}
		}

		g_xEngine.Touch().m_bTouchActive = false;
	}

	g_xEngine.Touch().m_bWasTouchDownLastFrame = bDown;
}

// ============================================================================
// Tap
// ============================================================================


// ============================================================================
// Swipe
// ============================================================================




// ============================================================================
// Configuration
// ============================================================================



// ============================================================================
// State Queries
// ============================================================================

