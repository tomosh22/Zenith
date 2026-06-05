#pragma once

#include "Maths/Zenith_Maths.h"
#include <cmath>

// ============================================================================
// CB_DayNight — pure day/night clock + sun-direction math. Time-of-day is 0..1
// (0.0/1.0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset). The sun
// direction is the *light* direction (points from the sun toward the ground:
// straight down at noon, shallow at dawn/dusk). Engine application (sky/sun
// light) is the visual-pass remainder; this math is unit-tested.
// ============================================================================
struct CB_DayNight
{
	float m_fTimeOfDay      = 0.5f;    // 0..1
	float m_fDayLengthSecs  = 120.0f;  // real seconds per in-game day

	void Advance(float fDt)
	{
		if (m_fDayLengthSecs <= 0.0f)
		{
			return;
		}
		m_fTimeOfDay += fDt / m_fDayLengthSecs;
		m_fTimeOfDay -= std::floor(m_fTimeOfDay);  // wrap into [0,1)
	}

	bool IsDay() const { return m_fTimeOfDay > 0.25f && m_fTimeOfDay < 0.75f; }

	// Sun elevation 0..1 (0 at/below horizon at night, 1 at noon).
	float GetSunElevation() const
	{
		if (m_fTimeOfDay <= 0.25f || m_fTimeOfDay >= 0.75f)
		{
			return 0.0f;
		}
		return std::sin(((m_fTimeOfDay - 0.25f) / 0.5f) * glm::pi<float>());
	}

	// Light direction (from sun toward ground). Straight down at noon, shallow
	// and swinging east->west across the day.
	Zenith_Maths::Vector3 GetSunDirection() const
	{
		const float fElev = GetSunElevation();
		const float fAzimuth = m_fTimeOfDay * 2.0f * glm::pi<float>();
		const float fHoriz = std::sqrt(1.0f - fElev * fElev);  // horizontal magnitude
		Zenith_Maths::Vector3 xDir(std::cos(fAzimuth) * fHoriz, -fElev, std::sin(fAzimuth) * fHoriz);
		const float fLen = std::sqrt(xDir.x * xDir.x + xDir.y * xDir.y + xDir.z * xDir.z);
		return (fLen > 0.0001f) ? (xDir / fLen) : Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	}
};
