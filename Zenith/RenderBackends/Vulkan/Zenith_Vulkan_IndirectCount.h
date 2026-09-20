#pragma once

#include <cstdint>

// Pure negotiation seam for Vulkan's two equivalent indexed-indirect-count
// routes. Keeping this independent of vk::* types makes the advertised ->
// enabled -> usable state machine unit-testable without a device.
enum class Zenith_Vulkan_IndirectCountRoute : uint8_t
{
	NONE,
	CORE_1_2,
	KHR_EXTENSION,
};

struct Zenith_Vulkan_IndirectCountSelection
{
	Zenith_Vulkan_IndirectCountRoute m_eRoute = Zenith_Vulkan_IndirectCountRoute::NONE;
	bool m_bEnableCoreFeature = false;
	bool m_bEnableKHRExtension = false;
};

inline Zenith_Vulkan_IndirectCountSelection Zenith_Vulkan_SelectIndirectCountRoute(
	bool bApiAtLeast12,
	bool bCoreFeatureAdvertised,
	bool bKHRExtensionAdvertised)
{
	// Prefer the promoted core route when it is genuinely advertised. This
	// avoids enabling an unnecessary extension on devices exposing both.
	if (bApiAtLeast12 && bCoreFeatureAdvertised)
	{
		return { Zenith_Vulkan_IndirectCountRoute::CORE_1_2, true, false };
	}
	if (bKHRExtensionAdvertised)
	{
		return { Zenith_Vulkan_IndirectCountRoute::KHR_EXTENSION, false, true };
	}
	return {};
}

inline bool Zenith_Vulkan_IsIndirectCountRouteUsable(
	Zenith_Vulkan_IndirectCountRoute eRoute,
	bool bCoreProcResolved,
	bool bKHRProcResolved)
{
	switch (eRoute)
	{
	case Zenith_Vulkan_IndirectCountRoute::CORE_1_2:      return bCoreProcResolved;
	case Zenith_Vulkan_IndirectCountRoute::KHR_EXTENSION: return bKHRProcResolved;
	case Zenith_Vulkan_IndirectCountRoute::NONE:          return false;
	}
	return false;
}
