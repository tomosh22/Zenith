#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

// Zenith shaders target SPIR-V 1.3, whose first core Vulkan route is 1.1.
// Keep the minimum and the instance-requested API version in one seam so
// physical-device suitability and VMA initialisation cannot drift apart.
constexpr uint32_t uZENITH_VULKAN_MIN_DEVICE_API_VERSION = VK_API_VERSION_1_1;
constexpr uint32_t uZENITH_VULKAN_REQUESTED_API_VERSION = VK_API_VERSION_1_3;
constexpr uint32_t uZENITH_VULKAN_INVALID_QUEUE_FAMILY = UINT32_MAX;

inline bool Zenith_Vulkan_IsDeviceAPIVersionSupported(uint32_t uDeviceAPIVersion)
{
	return uDeviceAPIVersion >= uZENITH_VULKAN_MIN_DEVICE_API_VERSION;
}

// VMA_VULKAN_VERSION uses decimal major/minor/patch packing (for example,
// 1002000 means Vulkan 1.2), unlike Vulkan's bit-packed API version.
constexpr uint32_t Zenith_Vulkan_VMACompileVersionToAPIVersion(uint32_t uVMACompileVersion)
{
	return VK_MAKE_API_VERSION(0,
		uVMACompileVersion / 1000000u,
		(uVMACompileVersion / 1000u) % 1000u,
		uVMACompileVersion % 1000u);
}

inline uint32_t Zenith_Vulkan_ResolveAllocatorAPIVersion(
	uint32_t uDeviceAPIVersion,
	uint32_t uVMACompileAPIVersion)
{
	// VMA requires this value to be no higher than both VkApplicationInfo's
	// apiVersion and the selected physical device's apiVersion. It also asserts
	// when asked to use an API newer than its VMA_VULKAN_VERSION build ceiling.
	const uint32_t uRuntimeCeiling = uDeviceAPIVersion < uZENITH_VULKAN_REQUESTED_API_VERSION
		? uDeviceAPIVersion
		: uZENITH_VULKAN_REQUESTED_API_VERSION;
	return uRuntimeCeiling < uVMACompileAPIVersion
		? uRuntimeCeiling
		: uVMACompileAPIVersion;
}

struct Zenith_Vulkan_QueueFamilySupport
{
	bool m_bGraphics = false;
	bool m_bCompute = false;
	bool m_bPresent = false;
};

struct Zenith_Vulkan_QueueFamilySelection
{
	uint32_t m_uGraphics = uZENITH_VULKAN_INVALID_QUEUE_FAMILY;
	uint32_t m_uCompute = uZENITH_VULKAN_INVALID_QUEUE_FAMILY;
	uint32_t m_uCopy = uZENITH_VULKAN_INVALID_QUEUE_FAMILY;
	uint32_t m_uPresent = uZENITH_VULKAN_INVALID_QUEUE_FAMILY;

	bool IsComplete() const
	{
		return m_uGraphics != uZENITH_VULKAN_INVALID_QUEUE_FAMILY &&
			m_uCompute != uZENITH_VULKAN_INVALID_QUEUE_FAMILY &&
			m_uCopy != uZENITH_VULKAN_INVALID_QUEUE_FAMILY &&
			m_uPresent != uZENITH_VULKAN_INVALID_QUEUE_FAMILY;
	}
};

// Pure queue-selection seam. Render-graph workers record graphics and compute
// commands into graphics-family command buffers, so the selected graphics
// family must support BOTH capabilities. Presentation may use another family.
inline Zenith_Vulkan_QueueFamilySelection Zenith_Vulkan_SelectQueueFamilies(
	const Zenith_Vulkan_QueueFamilySupport* paxFamilies,
	uint32_t uFamilyCount)
{
	Zenith_Vulkan_QueueFamilySelection xSelection;
	if (paxFamilies == nullptr || uFamilyCount == 0u)
	{
		return xSelection;
	}

	// Prefer a graphics+compute family that can also present.
	for (uint32_t u = 0; u < uFamilyCount; ++u)
	{
		if (paxFamilies[u].m_bGraphics && paxFamilies[u].m_bCompute &&
			paxFamilies[u].m_bPresent)
		{
			xSelection.m_uGraphics = u;
			xSelection.m_uPresent = u;
			break;
		}
	}
	if (xSelection.m_uGraphics == uZENITH_VULKAN_INVALID_QUEUE_FAMILY)
	{
		for (uint32_t u = 0; u < uFamilyCount; ++u)
		{
			if (paxFamilies[u].m_bGraphics && paxFamilies[u].m_bCompute)
			{
				xSelection.m_uGraphics = u;
				break;
			}
		}
	}
	if (xSelection.m_uPresent == uZENITH_VULKAN_INVALID_QUEUE_FAMILY)
	{
		for (uint32_t u = 0; u < uFamilyCount; ++u)
		{
			if (paxFamilies[u].m_bPresent)
			{
				xSelection.m_uPresent = u;
				break;
			}
		}
	}

	// Preserve the backend's dedicated-compute preference for explicit compute
	// command buffers. The render graph remains safe because graphics itself was
	// required to support compute above. Reuse graphics when no dedicated family
	// exists.
	for (uint32_t u = 0; u < uFamilyCount; ++u)
	{
		if (paxFamilies[u].m_bCompute && !paxFamilies[u].m_bGraphics)
		{
			xSelection.m_uCompute = u;
			break;
		}
	}
	if (xSelection.m_uCompute == uZENITH_VULKAN_INVALID_QUEUE_FAMILY)
	{
		xSelection.m_uCompute = xSelection.m_uGraphics;
	}

	// Graphics queues implicitly support transfer operations.
	xSelection.m_uCopy = xSelection.m_uGraphics;
	return xSelection;
}

// Which of the four resolved queue slots should own a vkCommandPool.
struct Zenith_Vulkan_CommandPoolPlan
{
	bool m_bGraphics = false;
	bool m_bCompute = false;
	bool m_bCopy = false;
	bool m_bPresent = false;
};

// Pure command-pool seam, factored out of CreateCommandPools so the
// presentation decision is coverable without the hardware topology that
// triggers it (a machine whose present and graphics families coincide can
// never execute the skip branch at runtime).
//
// A command pool is only meaningful on a queue family that can OWN command
// buffers. Graphics, compute and copy always resolve to command-capable
// families — Zenith_Vulkan_SelectQueueFamilies requires graphics+compute on
// the graphics slot, points copy at graphics, and only ever picks a real
// compute family for compute. Presentation is different: it is selected purely
// on vkGetPhysicalDeviceSurfaceSupportKHR, vkQueuePresentKHR takes no command
// buffer, and a WSI-only family may expose none of graphics/compute/transfer —
// so a separate presentation family gets no pool. When presentation shares the
// graphics family it keeps that family's pool, so the common single-family
// topology stays fully populated.
inline Zenith_Vulkan_CommandPoolPlan Zenith_Vulkan_PlanCommandPools(
	uint32_t uGraphicsFamily,
	uint32_t uComputeFamily,
	uint32_t uCopyFamily,
	uint32_t uPresentFamily)
{
	Zenith_Vulkan_CommandPoolPlan xPlan;
	xPlan.m_bGraphics = uGraphicsFamily != uZENITH_VULKAN_INVALID_QUEUE_FAMILY;
	xPlan.m_bCompute  = uComputeFamily  != uZENITH_VULKAN_INVALID_QUEUE_FAMILY;
	xPlan.m_bCopy     = uCopyFamily     != uZENITH_VULKAN_INVALID_QUEUE_FAMILY;
	xPlan.m_bPresent  = uPresentFamily  != uZENITH_VULKAN_INVALID_QUEUE_FAMILY &&
	                    uPresentFamily  == uGraphicsFamily;
	return xPlan;
}
