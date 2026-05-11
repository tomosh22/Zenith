#pragma once

#include <cstdint>

enum class DP_ItemTag : uint32_t
{
	None = 0,
	Iron,
	Key,
	SkeletonKey,
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

inline uint32_t DP_ObjectiveTagToBit(DP_ItemTag eTag)
{
	if (!DP_IsObjectiveTag(eTag)) return 0;
	return 1u << (static_cast<uint32_t>(eTag) - static_cast<uint32_t>(DP_ItemTag::Objective1));
}

inline constexpr uint32_t DP_ALL_OBJECTIVES_MASK = 0b11111u;
