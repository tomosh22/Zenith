#pragma once
/**
 * Exploration_AtmosphereController.h - Day/night cycle and weather system
 *
 * Demonstrates:
 * - Day/night cycle with animated sun position
 * - Sun color temperature changes
 * - Fog density and color tied to time of day
 * - Weather state machine
 * - Smooth transitions between states
 *
 * Engine APIs used:
 * - Flux_Graphics for sun direction/color uniforms
 */

#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_Graphics.h"

#include <cmath>
#include <random>
#include <algorithm>

namespace Exploration_AtmosphereController
{
	// Forward declaration
	inline void ApplyToEngine();

	// ========================================================================
	// Weather States
	// ========================================================================
	enum WeatherState
	{
		WEATHER_CLEAR = 0,
		WEATHER_CLOUDY,
		WEATHER_FOGGY,
		WEATHER_COUNT
	};

	// ========================================================================
	// Atmosphere State Structure
	// ========================================================================
	struct AtmosphereState
	{
		// Time of day (0.0 = midnight, 0.25 = 6AM, 0.5 = noon, 0.75 = 6PM)
		float m_fTimeOfDay = 0.25f;

		// Sun properties
		Zenith_Maths::Vector3 m_xSunDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		Zenith_Maths::Vector3 m_xSunColor = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);
		float m_fSunIntensity = 1.0f;

		// Fog properties
		float m_fFogDensity = 0.0001f;
		Zenith_Maths::Vector3 m_xFogColor = Zenith_Maths::Vector3(0.7f, 0.8f, 0.9f);

		// Ambient light
		float m_fAmbientIntensity = 0.15f;

		// Weather
		WeatherState m_eWeatherState = WEATHER_CLEAR;
		float m_fWeatherTransition = 1.0f;  // 0.0 = previous, 1.0 = current
	};

	// ========================================================================
	// Configuration
	// ========================================================================
	static float s_fDayCycleDuration = 600.0f;  // Seconds for full cycle
	static bool s_bDayCycleEnabled = true;
	static float s_fSunIntensity = 1.0f;
	static float s_fAmbientDay = 0.15f;
	static float s_fAmbientNight = 0.02f;

	// Sun colors at different times
	static Zenith_Maths::Vector3 s_xSunriseColor = Zenith_Maths::Vector3(1.0f, 0.6f, 0.3f);
	static Zenith_Maths::Vector3 s_xMiddayColor = Zenith_Maths::Vector3(1.0f, 0.98f, 0.95f);
	static Zenith_Maths::Vector3 s_xSunsetColor = Zenith_Maths::Vector3(1.0f, 0.5f, 0.2f);
	static Zenith_Maths::Vector3 s_xNightColor = Zenith_Maths::Vector3(0.1f, 0.1f, 0.2f);

	// Fog settings
	static float s_fFogDensityClear = 0.00015f;
	static float s_fFogDensityFoggy = 0.0015f;
	static float s_fFogTransitionSpeed = 0.5f;
	static Zenith_Maths::Vector3 s_xFogColorDay = Zenith_Maths::Vector3(0.7f, 0.8f, 0.9f);
	static Zenith_Maths::Vector3 s_xFogColorNight = Zenith_Maths::Vector3(0.02f, 0.02f, 0.05f);
	static Zenith_Maths::Vector3 s_xFogColorSunrise = Zenith_Maths::Vector3(0.9f, 0.7f, 0.5f);

	// Weather settings
	static float s_fWeatherChangeInterval = 120.0f;
	static float s_fWeatherTransitionDuration = 30.0f;
	static bool s_bRandomWeather = true;

	// ========================================================================
	// Internal State
	// ========================================================================
	static AtmosphereState s_xCurrentState;
	static AtmosphereState s_xTargetState;
	static float s_fWeatherTimer = 0.0f;
	static WeatherState s_ePreviousWeather = WEATHER_CLEAR;
	static float s_fTargetFogDensity = 0.00015f;
	static std::mt19937 s_xRng(12345);

	/**
	 * Configure atmosphere settings from Exploration_Config
	 */
	inline void Configure(
		float fDayCycleDuration,
		float fStartTimeOfDay,
		bool bDayCycleEnabled,
		float fSunIntensity,
		float fAmbientDay,
		float fAmbientNight,
		const float* afSunriseColor,
		const float* afMiddayColor,
		const float* afSunsetColor,
		const float* afNightColor,
		float fFogDensityClear,
		float fFogDensityFoggy,
		float fFogTransitionSpeed,
		const float* afFogColorDay,
		const float* afFogColorNight,
		const float* afFogColorSunrise,
		float fWeatherChangeInterval,
		float fWeatherTransitionDuration,
		bool bRandomWeather)
	{
		s_fDayCycleDuration = fDayCycleDuration;
		s_xCurrentState.m_fTimeOfDay = fStartTimeOfDay;
		s_bDayCycleEnabled = bDayCycleEnabled;
		s_fSunIntensity = fSunIntensity;
		s_fAmbientDay = fAmbientDay;
		s_fAmbientNight = fAmbientNight;

		s_xSunriseColor = Zenith_Maths::Vector3(afSunriseColor[0], afSunriseColor[1], afSunriseColor[2]);
		s_xMiddayColor = Zenith_Maths::Vector3(afMiddayColor[0], afMiddayColor[1], afMiddayColor[2]);
		s_xSunsetColor = Zenith_Maths::Vector3(afSunsetColor[0], afSunsetColor[1], afSunsetColor[2]);
		s_xNightColor = Zenith_Maths::Vector3(afNightColor[0], afNightColor[1], afNightColor[2]);

		s_fFogDensityClear = fFogDensityClear;
		s_fFogDensityFoggy = fFogDensityFoggy;
		s_fFogTransitionSpeed = fFogTransitionSpeed;

		s_xFogColorDay = Zenith_Maths::Vector3(afFogColorDay[0], afFogColorDay[1], afFogColorDay[2]);
		s_xFogColorNight = Zenith_Maths::Vector3(afFogColorNight[0], afFogColorNight[1], afFogColorNight[2]);
		s_xFogColorSunrise = Zenith_Maths::Vector3(afFogColorSunrise[0], afFogColorSunrise[1], afFogColorSunrise[2]);

		s_fWeatherChangeInterval = fWeatherChangeInterval;
		s_fWeatherTransitionDuration = fWeatherTransitionDuration;
		s_bRandomWeather = bRandomWeather;
	}

	/**
	 * Calculate sun direction based on time of day
	 * @param fTimeOfDay Time (0.0-1.0)
	 * @return Normalized sun direction vector
	 */
	inline Zenith_Maths::Vector3 CalculateSunDirection(float fTimeOfDay)
	{
		// Convert time to angle (0.0 = midnight = sun at nadir, 0.5 = noon = sun at zenith)
		float fAngle = (fTimeOfDay - 0.25f) * 2.0f * 3.14159265f;

		// Sun path: rises in east (negative X), arcs overhead (positive Y), sets in west (positive X)
		Zenith_Maths::Vector3 xSunDir;
		xSunDir.x = std::sin(fAngle);
		xSunDir.y = std::cos(fAngle);  // Y = height
		xSunDir.z = 0.3f;  // Slight tilt for more interesting shadows

		return glm::normalize(xSunDir);
	}

	/**
	 * Calculate sun color based on time of day
	 */
	inline Zenith_Maths::Vector3 CalculateSunColor(float fTimeOfDay)
	{
		// Determine which phase we're in
		// 0.0-0.2: Night
		// 0.2-0.35: Sunrise
		// 0.35-0.65: Day
		// 0.65-0.8: Sunset
		// 0.8-1.0: Night

		float fNight1End = 0.2f;
		float fSunriseEnd = 0.35f;
		float fDayEnd = 0.65f;
		float fSunsetEnd = 0.8f;

		Zenith_Maths::Vector3 xColor;

		if (fTimeOfDay < fNight1End)
		{
			// Night (first half)
			xColor = s_xNightColor;
		}
		else if (fTimeOfDay < fSunriseEnd)
		{
			// Sunrise transition
			float fT = (fTimeOfDay - fNight1End) / (fSunriseEnd - fNight1End);
			xColor = glm::mix(s_xNightColor, s_xSunriseColor, fT);
		}
		else if (fTimeOfDay < 0.5f)
		{
			// Morning to midday
			float fT = (fTimeOfDay - fSunriseEnd) / (0.5f - fSunriseEnd);
			xColor = glm::mix(s_xSunriseColor, s_xMiddayColor, fT);
		}
		else if (fTimeOfDay < fDayEnd)
		{
			// Midday to evening
			float fT = (fTimeOfDay - 0.5f) / (fDayEnd - 0.5f);
			xColor = glm::mix(s_xMiddayColor, s_xSunsetColor, fT);
		}
		else if (fTimeOfDay < fSunsetEnd)
		{
			// Sunset transition
			float fT = (fTimeOfDay - fDayEnd) / (fSunsetEnd - fDayEnd);
			xColor = glm::mix(s_xSunsetColor, s_xNightColor, fT);
		}
		else
		{
			// Night (second half)
			xColor = s_xNightColor;
		}

		return xColor;
	}

	/**
	 * Calculate sun intensity based on height in sky
	 */
	inline float CalculateSunIntensity(const Zenith_Maths::Vector3& xSunDir)
	{
		// Intensity based on sun height (Y component)
		float fHeight = std::max(0.0f, xSunDir.y);

		// Below horizon = no sun
		if (fHeight <= 0.0f)
			return 0.0f;

		// Ramp up intensity as sun rises
		float fIntensity = std::sqrt(fHeight) * s_fSunIntensity;

		return fIntensity;
	}

	/**
	 * Calculate ambient intensity based on time of day
	 */
	inline float CalculateAmbientIntensity(float fTimeOfDay)
	{
		// Use sine wave centered on noon (0.5)
		float fDayFactor = std::sin(fTimeOfDay * 3.14159265f);
		fDayFactor = std::max(0.0f, fDayFactor);

		return glm::mix(s_fAmbientNight, s_fAmbientDay, fDayFactor);
	}

	/**
	 * Calculate fog color based on time of day and sun position
	 */
	inline Zenith_Maths::Vector3 CalculateFogColor(float fTimeOfDay, const Zenith_Maths::Vector3& xSunColor)
	{
		// Near sunrise/sunset, fog takes on sun color
		bool bNearHorizon = (fTimeOfDay > 0.2f && fTimeOfDay < 0.35f) ||
		                    (fTimeOfDay > 0.65f && fTimeOfDay < 0.8f);

		Zenith_Maths::Vector3 xBaseFog;
		if (fTimeOfDay > 0.25f && fTimeOfDay < 0.75f)
		{
			// Daytime
			xBaseFog = s_xFogColorDay;
		}
		else
		{
			// Nighttime
			xBaseFog = s_xFogColorNight;
		}

		if (bNearHorizon)
		{
			// Blend with sunrise/sunset fog color
			float fHorizonFactor = 0.0f;
			if (fTimeOfDay > 0.2f && fTimeOfDay < 0.35f)
			{
				fHorizonFactor = 1.0f - std::abs((fTimeOfDay - 0.275f) / 0.075f);
			}
			else
			{
				fHorizonFactor = 1.0f - std::abs((fTimeOfDay - 0.725f) / 0.075f);
			}
			fHorizonFactor = std::max(0.0f, fHorizonFactor);
			xBaseFog = glm::mix(xBaseFog, s_xFogColorSunrise, fHorizonFactor);
		}

		return xBaseFog;
	}

	/**
	 * Get target fog density based on weather state
	 */
	inline float GetWeatherFogDensity(WeatherState eWeather)
	{
		switch (eWeather)
		{
		case WEATHER_CLEAR:
			return s_fFogDensityClear;
		case WEATHER_CLOUDY:
			return s_fFogDensityClear * 2.0f;
		case WEATHER_FOGGY:
			return s_fFogDensityFoggy;
		default:
			return s_fFogDensityClear;
		}
	}

	/**
	 * Update weather state machine
	 */
	inline void UpdateWeather(float fDt)
	{
		if (!s_bRandomWeather)
			return;

		s_fWeatherTimer += fDt;

		// Check if transitioning
		if (s_xCurrentState.m_fWeatherTransition < 1.0f)
		{
			s_xCurrentState.m_fWeatherTransition += fDt / s_fWeatherTransitionDuration;
			s_xCurrentState.m_fWeatherTransition = std::min(1.0f, s_xCurrentState.m_fWeatherTransition);

			// Interpolate fog density during transition
			float fPrevDensity = GetWeatherFogDensity(s_ePreviousWeather);
			float fNextDensity = GetWeatherFogDensity(s_xCurrentState.m_eWeatherState);
			s_fTargetFogDensity = glm::mix(fPrevDensity, fNextDensity, s_xCurrentState.m_fWeatherTransition);
		}

		// Check for weather change
		if (s_fWeatherTimer >= s_fWeatherChangeInterval)
		{
			s_fWeatherTimer = 0.0f;

			// Random new weather state
			s_ePreviousWeather = s_xCurrentState.m_eWeatherState;
			std::uniform_int_distribution<int> xDist(0, WEATHER_COUNT - 1);
			s_xCurrentState.m_eWeatherState = static_cast<WeatherState>(xDist(s_xRng));
			s_xCurrentState.m_fWeatherTransition = 0.0f;
		}
	}

	/**
	 * Update atmosphere state
	 * @param fDt Delta time in seconds
	 */
	inline void Update(float fDt)
	{
		// Update time of day
		if (s_bDayCycleEnabled && s_fDayCycleDuration > 0.0f)
		{
			s_xCurrentState.m_fTimeOfDay += fDt / s_fDayCycleDuration;
			if (s_xCurrentState.m_fTimeOfDay >= 1.0f)
			{
				s_xCurrentState.m_fTimeOfDay -= 1.0f;
			}
		}

		// Update weather
		UpdateWeather(fDt);

		// Calculate sun properties
		s_xCurrentState.m_xSunDirection = CalculateSunDirection(s_xCurrentState.m_fTimeOfDay);
		s_xCurrentState.m_xSunColor = CalculateSunColor(s_xCurrentState.m_fTimeOfDay);
		s_xCurrentState.m_fSunIntensity = CalculateSunIntensity(s_xCurrentState.m_xSunDirection);
		s_xCurrentState.m_fAmbientIntensity = CalculateAmbientIntensity(s_xCurrentState.m_fTimeOfDay);

		// Calculate fog properties
		s_xCurrentState.m_xFogColor = CalculateFogColor(
			s_xCurrentState.m_fTimeOfDay, s_xCurrentState.m_xSunColor);

		// Smoothly interpolate fog density
		s_xCurrentState.m_fFogDensity = glm::mix(
			s_xCurrentState.m_fFogDensity,
			s_fTargetFogDensity,
			fDt * s_fFogTransitionSpeed);

		// Apply to engine
		ApplyToEngine();
	}

	/**
	 * Apply current atmosphere state to engine graphics
	 */
	inline void ApplyToEngine()
	{
		// Update frame constants with sun direction and color
		// Note: This modifies the shared FrameConstants that get uploaded to GPU
		Flux_Graphics::FrameConstants& xConstants = Flux_Graphics::s_xFrameConstants;

		xConstants.m_xSunDir_Pad = Zenith_Maths::Vector4(
			s_xCurrentState.m_xSunDirection.x,
			s_xCurrentState.m_xSunDirection.y,
			s_xCurrentState.m_xSunDirection.z,
			0.0f);

		xConstants.m_xSunColour_Pad = Zenith_Maths::Vector4(
			s_xCurrentState.m_xSunColor.x * s_xCurrentState.m_fSunIntensity,
			s_xCurrentState.m_xSunColor.y * s_xCurrentState.m_fSunIntensity,
			s_xCurrentState.m_xSunColor.z * s_xCurrentState.m_fSunIntensity,
			s_xCurrentState.m_fAmbientIntensity);
	}

	/**
	 * Get current atmosphere state (for UI display)
	 */
	inline const AtmosphereState& GetState()
	{
		return s_xCurrentState;
	}

	/**
	 * Get time of day as human-readable string
	 */
	inline void GetTimeString(char* szBuffer, size_t uBufferSize)
	{
		float fHours24 = s_xCurrentState.m_fTimeOfDay * 24.0f;
		int iHours = static_cast<int>(fHours24);
		int iMinutes = static_cast<int>((fHours24 - static_cast<float>(iHours)) * 60.0f);
		snprintf(szBuffer, uBufferSize, "%02d:%02d", iHours, iMinutes);
	}

	/**
	 * Get weather state name
	 */
	inline const char* GetWeatherName(WeatherState eWeather)
	{
		switch (eWeather)
		{
		case WEATHER_CLEAR: return "Clear";
		case WEATHER_CLOUDY: return "Cloudy";
		case WEATHER_FOGGY: return "Foggy";
		default: return "Unknown";
		}
	}

	/**
	 * Force set time of day (for debugging)
	 */
	inline void SetTimeOfDay(float fTime)
	{
		s_xCurrentState.m_fTimeOfDay = std::clamp(fTime, 0.0f, 1.0f);
	}

	/**
	 * Force set weather state (for debugging)
	 */
	inline void SetWeather(WeatherState eWeather)
	{
		s_ePreviousWeather = s_xCurrentState.m_eWeatherState;
		s_xCurrentState.m_eWeatherState = eWeather;
		s_xCurrentState.m_fWeatherTransition = 0.0f;
	}

	/**
	 * Toggle day/night cycle
	 */
	inline void SetDayCycleEnabled(bool bEnabled)
	{
		s_bDayCycleEnabled = bEnabled;
	}

	/**
	 * Reset atmosphere to default state
	 */
	inline void Reset()
	{
		s_xCurrentState.m_fTimeOfDay = 0.25f;  // 6 AM
		s_xCurrentState.m_eWeatherState = WEATHER_CLEAR;
		s_xCurrentState.m_fWeatherTransition = 1.0f;
		s_fWeatherTimer = 0.0f;
		s_fTargetFogDensity = s_fFogDensityClear;
	}

} // namespace Exploration_AtmosphereController
