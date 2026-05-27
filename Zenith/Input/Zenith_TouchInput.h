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

// State + behaviour for the TouchInput subsystem. Held on g_xEngine
// and accessed via g_xEngine.Touch().
class Zenith_TouchInput
{
public:
	Zenith_TouchInput() = default;
	~Zenith_TouchInput() = default;

	Zenith_TouchInput(const Zenith_TouchInput&) = delete;
	Zenith_TouchInput& operator=(const Zenith_TouchInput&) = delete;

	void Update();

	bool WasTapThisFrame() const                       { return m_bTapThisFrame; }
	Zenith_Maths::Vector2 GetTapPosition() const       { return m_xTapPosition; }

	bool WasSwipeThisFrame() const                     { return m_bSwipeThisFrame; }
	Zenith_SwipeDirection GetSwipeDirection() const    { return m_eSwipeDirection; }
	Zenith_Maths::Vector2 GetSwipeStartPosition() const{ return m_xSwipeStartPos; }
	float GetSwipeDistance() const                     { return m_fSwipeDistance; }

	void SetSwipeThreshold(float fPixels)              { m_fSwipeThreshold = fPixels; }
	void SetTapMaxMovement(float fPixels)              { m_fTapMaxMovement = fPixels; }
	void SetTapMaxDuration(float fSeconds)             { m_fTapMaxDuration = fSeconds; }

	bool IsTouchDown() const                           { return m_bCurrentlyDown; }
	Zenith_Maths::Vector2 GetTouchPosition() const     { return m_xCurrentTouchPos; }

	bool                     m_bTouchActive          = false;
	Zenith_Maths::Vector2    m_xTouchStartPos        = { 0.f, 0.f };
	float                    m_fTouchStartTime       = 0.f;
	bool                     m_bWasTouchDownLastFrame = false;

	bool                     m_bTapThisFrame   = false;
	bool                     m_bSwipeThisFrame = false;
	Zenith_SwipeDirection    m_eSwipeDirection = ZENITH_SWIPE_NONE;
	Zenith_Maths::Vector2    m_xTapPosition    = { 0.f, 0.f };
	Zenith_Maths::Vector2    m_xSwipeStartPos  = { 0.f, 0.f };
	float                    m_fSwipeDistance  = 0.f;

	Zenith_Maths::Vector2    m_xCurrentTouchPos = { 0.f, 0.f };
	bool                     m_bCurrentlyDown   = false;

	float                    m_fSwipeThreshold = 30.f;
	float                    m_fTapMaxMovement = 15.f;
	float                    m_fTapMaxDuration = 0.3f;
};
