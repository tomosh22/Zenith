#pragma once

#include <cstdint>

enum class DP_ItemTag : uint32_t
{
	None = 0,
	Iron,
	Key,
	SkeletonKey,
	// MVP-2.3: Wood + Spike for forge recipes. Wood is a tool-class
	// reagent (Child still can't carry); Spike is its forged output
	// (a Devout-only Aelfric counter -- post-MVP feature scope; for
	// MVP this tag exists so the forge can produce it as a recipe
	// output, and other systems treat it as a generic tool).
	Wood,
	Spike,
	Objective1,
	Objective2,
	Objective3,
	Objective4,
	Objective5,

	COUNT
};

inline const char* DP_ItemTagToString(DP_ItemTag eTag)
{
	switch (eTag)
	{
	case DP_ItemTag::None:        return "None";
	case DP_ItemTag::Iron:        return "Iron";
	case DP_ItemTag::Key:         return "Key";
	case DP_ItemTag::SkeletonKey: return "SkeletonKey";
	case DP_ItemTag::Wood:        return "Wood";
	case DP_ItemTag::Spike:       return "Spike";
	case DP_ItemTag::Objective1:  return "Objective1";
	case DP_ItemTag::Objective2:  return "Objective2";
	case DP_ItemTag::Objective3:  return "Objective3";
	case DP_ItemTag::Objective4:  return "Objective4";
	case DP_ItemTag::Objective5:  return "Objective5";
	default:                       return "Unknown";
	}
}

inline bool DP_IsObjectiveTag(DP_ItemTag eTag)
{
	return eTag >= DP_ItemTag::Objective1 && eTag <= DP_ItemTag::Objective5;
}

// MVP-2.1.4: classify an item as a "tool" -- the crafting / utility
// items used to open doors, forge new items, etc. The Child archetype
// can't pick these up (the GDD framing: small hands, can't carry).
// Objectives + the SkeletonKey-as-objective edge case are NOT tools
// (the SkeletonKey is technically a master key but the GDD treats it
// as an objective-class pickup -- it's exempt from the Child filter
// so a Child can still carry it).
//
// MVP-2.3 forge additions: Wood (forge input) and Spike (forge
// output) are also tools.
inline bool DP_IsToolTag(DP_ItemTag eTag)
{
	return eTag == DP_ItemTag::Iron
	    || eTag == DP_ItemTag::Key
	    || eTag == DP_ItemTag::Wood
	    || eTag == DP_ItemTag::Spike;
}

inline uint32_t DP_ObjectiveTagToBit(DP_ItemTag eTag)
{
	if (!DP_IsObjectiveTag(eTag)) return 0;
	return 1u << (static_cast<uint32_t>(eTag) - static_cast<uint32_t>(DP_ItemTag::Objective1));
}

inline constexpr uint32_t DP_ALL_OBJECTIVES_MASK = 0b11111u;
