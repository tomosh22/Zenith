#pragma once

#include <cstdint>

// ============================================================================
// CB_Policy — the city ordinances a player can enact, either city-wide or per
// district (CB_Districts). Each policy hooks the simulation (CB_BuildingPlacement
// applies its effect scaled by the fraction of buildings it covers) and costs a
// little upkeep. Cities: Skylines-style policy toggles.
// ============================================================================

enum CB_EPolicy : uint32_t
{
	CB_POLICY_RECYCLING         = 0,   // households sort waste → less garbage generated
	CB_POLICY_FREE_TRANSIT      = 1,   // fare-free buses → higher ridership (relieves traffic)
	CB_POLICY_POLLUTION_CONTROL = 2,   // emission limits on industry → less pollution
	CB_POLICY_PARKS_MANDATE     = 3,   // green-space requirement → happier residents
	CB_POLICY_COUNT
};

inline uint32_t CB_PolicyBit(CB_EPolicy ePolicy) { return 1u << static_cast<uint32_t>(ePolicy); }

inline const char* CB_PolicyName(CB_EPolicy ePolicy)
{
	switch (ePolicy)
	{
	case CB_POLICY_RECYCLING:         return "Recycling";
	case CB_POLICY_FREE_TRANSIT:      return "Free Transit";
	case CB_POLICY_POLLUTION_CONTROL: return "Pollution Control";
	case CB_POLICY_PARKS_MANDATE:     return "Parks Mandate";
	default:                          return "?";
	}
}
