#pragma once

#include "Input/Zenith_TouchInput.h"
#include "Maths/Zenith_Maths.h"

// Phase 5.5b: per-Engine touch-input state. The 15 file-statics in
// Zenith_TouchInput.cpp move here. Static facade keeps its method surface;
// method bodies now read/write g_xEngine.Touch().m_xXxx.
class Zenith_TouchInputImpl
{
public:
	Zenith_TouchInputImpl() = default;
	~Zenith_TouchInputImpl() = default;

	Zenith_TouchInputImpl(const Zenith_TouchInputImpl&) = delete;
	Zenith_TouchInputImpl& operator=(const Zenith_TouchInputImpl&) = delete;

	// Touch session state.
	bool                     m_bTouchActive          = false;
	Zenith_Maths::Vector2    m_xTouchStartPos        = { 0.f, 0.f };
	float                    m_fTouchStartTime       = 0.f;
	bool                     m_bWasTouchDownLastFrame = false;

	// Per-frame events (reset each Update).
	bool                     m_bTapThisFrame   = false;
	bool                     m_bSwipeThisFrame = false;
	Zenith_SwipeDirection    m_eSwipeDirection = ZENITH_SWIPE_NONE;
	Zenith_Maths::Vector2    m_xTapPosition    = { 0.f, 0.f };
	Zenith_Maths::Vector2    m_xSwipeStartPos  = { 0.f, 0.f };
	float                    m_fSwipeDistance  = 0.f;

	// Current position.
	Zenith_Maths::Vector2    m_xCurrentTouchPos = { 0.f, 0.f };
	bool                     m_bCurrentlyDown   = false;

	// Thresholds (configurable via the facade's setters).
	float                    m_fSwipeThreshold = 30.f;
	float                    m_fTapMaxMovement = 15.f;
	float                    m_fTapMaxDuration = 0.3f;
};
