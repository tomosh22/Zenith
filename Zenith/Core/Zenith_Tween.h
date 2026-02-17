#pragma once
#include "Maths/Zenith_Maths.h"

//=============================================================================
// Zenith_EasingType
// Easing curve types wrapping GLM gtx/easing functions
//=============================================================================
enum Zenith_EasingType : uint8_t
{
	EASING_LINEAR = 0,
	EASING_QUAD_IN, EASING_QUAD_OUT, EASING_QUAD_IN_OUT,
	EASING_CUBIC_IN, EASING_CUBIC_OUT, EASING_CUBIC_IN_OUT,
	EASING_ELASTIC_IN, EASING_ELASTIC_OUT, EASING_ELASTIC_IN_OUT,
	EASING_BOUNCE_IN, EASING_BOUNCE_OUT, EASING_BOUNCE_IN_OUT,
	EASING_BACK_IN, EASING_BACK_OUT, EASING_BACK_IN_OUT,
	EASING_SINE_IN, EASING_SINE_OUT, EASING_SINE_IN_OUT,
	EASING_COUNT
};

// Apply easing function to a 0-1 value, returns 0-1 eased value
float Zenith_ApplyEasing(Zenith_EasingType eType, float fT);

// Get display name for an easing type
const char* Zenith_GetEasingTypeName(Zenith_EasingType eType);

//=============================================================================
// Zenith_TweenProperty
// Which transform property is being tweened
//=============================================================================
enum Zenith_TweenProperty : uint8_t
{
	TWEEN_PROPERTY_POSITION = 0,
	TWEEN_PROPERTY_ROTATION,
	TWEEN_PROPERTY_SCALE,
};

//=============================================================================
// Zenith_TweenCallback
// Completion callback (function pointer + void* userdata, no std::function)
//=============================================================================
using Zenith_TweenCallback = void(*)(void* pUserData);

//=============================================================================
// Zenith_TweenInstance
// A single active tween
//=============================================================================
struct Zenith_TweenInstance
{
	Zenith_TweenProperty m_eProperty = TWEEN_PROPERTY_SCALE;
	Zenith_EasingType m_eEasing = EASING_LINEAR;

	Zenith_Maths::Vector3 m_xFrom = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xTo = Zenith_Maths::Vector3(0.0f);

	// Quaternion endpoints for rotation tweens (avoids gimbal lock from Euler interpolation)
	Zenith_Maths::Quat m_xFromQuat = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Quat m_xToQuat = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

	float m_fDuration = 1.0f;
	float m_fElapsed = 0.0f;
	float m_fDelay = 0.0f;
	bool m_bLoop = false;
	bool m_bPingPong = false;
	bool m_bReversing = false;

	Zenith_TweenCallback m_pfnOnComplete = nullptr;
	void* m_pCallbackUserData = nullptr;

	float GetNormalizedTime() const;
};
