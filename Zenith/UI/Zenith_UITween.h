#pragma once

#include <cstdint>

namespace Zenith_UI {

enum class TweenEasing : uint32_t
{
	LINEAR,
	EASE_IN,
	EASE_OUT,
	EASE_IN_OUT
};

enum class TweenProperty : uint32_t
{
	ALPHA,
	POSITION_X,
	POSITION_Y,
	SIZE_X,
	SIZE_Y,
	COLOR_R,
	COLOR_G,
	COLOR_B,
	COLOR_A
};

struct Zenith_UITween
{
	TweenProperty m_eProperty;
	float m_fFrom;
	float m_fTo;
	float m_fDuration;
	float m_fDelay = 0.f;
	float m_fElapsed = 0.f;
	TweenEasing m_eEasing = TweenEasing::LINEAR;
	bool m_bActive = true;
};

inline float ApplyEasing(float fT, TweenEasing eEasing)
{
	switch (eEasing)
	{
	case TweenEasing::LINEAR:
		return fT;
	case TweenEasing::EASE_IN:
		return fT * fT;
	case TweenEasing::EASE_OUT:
		return 1.f - (1.f - fT) * (1.f - fT);
	case TweenEasing::EASE_IN_OUT:
		return fT * fT * (3.f - 2.f * fT); // smoothstep
	}
	return fT;
}

} // namespace Zenith_UI
