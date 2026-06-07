#include "Zenith.h"

#include "Input/Zenith_TouchInput.h"
#include "Input/Zenith_TouchInput.h"
#include "Input/Zenith_Input.h"

// Phase 5.5b: touch-gesture state lives on Zenith_TouchInput held by
// Zenith_Engine. All former file-statics are now reached via
// g_xEngine.Touch().m_xXxx.

// ============================================================================
// Update
// ============================================================================
void Zenith_TouchInput::Update()
{
	// Reset per-frame events
	m_bTapThisFrame = false;
	m_bSwipeThisFrame = false;
	m_eSwipeDirection = ZENITH_SWIPE_NONE;
	m_fSwipeDistance = 0.f;

	// Read current mouse/touch state
	bool bDown = g_xEngine.Input().IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	Zenith_Maths::Vector2_64 xPos64;
	g_xEngine.Input().GetMousePosition(xPos64);
	m_xCurrentTouchPos = Zenith_Maths::Vector2(static_cast<float>(xPos64.x), static_cast<float>(xPos64.y));
	m_bCurrentlyDown = bDown;

	float fCurrentTime = g_xEngine.Frame().GetTimePassed();

	if (bDown && !m_bWasTouchDownLastFrame)
	{
		// Touch just started
		m_bTouchActive = true;
		m_xTouchStartPos = m_xCurrentTouchPos;
		m_fTouchStartTime = fCurrentTime;
	}
	else if (!bDown && m_bWasTouchDownLastFrame && m_bTouchActive)
	{
		// Touch just ended - classify gesture
		float fDeltaX = m_xCurrentTouchPos.x - m_xTouchStartPos.x;
		float fDeltaY = m_xCurrentTouchPos.y - m_xTouchStartPos.y;
		float fDistance = std::sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);
		float fDuration = fCurrentTime - m_fTouchStartTime;

		if (fDistance < m_fTapMaxMovement && fDuration < m_fTapMaxDuration)
		{
			// Tap detected
			m_bTapThisFrame = true;
			m_xTapPosition = m_xTouchStartPos;
		}
		else if (fDistance >= m_fSwipeThreshold)
		{
			// Swipe detected - determine dominant direction
			m_bSwipeThisFrame = true;
			m_xSwipeStartPos = m_xTouchStartPos;
			m_fSwipeDistance = fDistance;

			float fAbsDeltaX = std::abs(fDeltaX);
			float fAbsDeltaY = std::abs(fDeltaY);

			if (fAbsDeltaX > fAbsDeltaY)
			{
				m_eSwipeDirection = (fDeltaX > 0.f) ? ZENITH_SWIPE_RIGHT : ZENITH_SWIPE_LEFT;
			}
			else
			{
				m_eSwipeDirection = (fDeltaY > 0.f) ? ZENITH_SWIPE_DOWN : ZENITH_SWIPE_UP;
			}
		}

		m_bTouchActive = false;
	}

	m_bWasTouchDownLastFrame = bDown;
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

