#pragma once

#include <cstdint>

// ============================================================================
// CB_ESimSpeed — the city sim clock speed (pause / slow / normal / fast / ultra)
// and CB_SpeedMultiplier, the real-time multiplier each level applies. The manager
// holds the current speed; building growth is gated on != PAUSED and the traffic
// dt is scaled by the multiplier so cars keep pace with the clock.
// ============================================================================

enum CB_ESimSpeed : uint8_t
{
	CB_SIM_PAUSED = 0,
	CB_SIM_SLOW   = 1,
	CB_SIM_NORMAL = 2,
	CB_SIM_FAST   = 3,
	CB_SIM_ULTRA  = 4,
	CB_SIM_SPEED_COUNT
};

inline float CB_SpeedMultiplier(CB_ESimSpeed eSpeed)
{
	switch (eSpeed)
	{
	case CB_SIM_PAUSED: return 0.0f;
	case CB_SIM_SLOW:   return 0.5f;
	case CB_SIM_NORMAL: return 1.0f;
	case CB_SIM_FAST:   return 2.0f;
	case CB_SIM_ULTRA:  return 4.0f;
	default:            return 1.0f;
	}
}
