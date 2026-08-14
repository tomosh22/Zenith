#include "Core/Zenith_TestFramework.h"
#include "Vulkan/Zenith_Vulkan_DeviceSelection.h"

#ifdef ZENITH_TESTING

ZENITH_TEST(VulkanDeviceSelection, RequiresVulkan11AndClampsAllocatorVersion)
{
	ZENITH_ASSERT_FALSE(Zenith_Vulkan_IsDeviceAPIVersionSupported(VK_API_VERSION_1_0),
		"SPIR-V 1.3 shaders make Vulkan 1.0 unsupported");
	ZENITH_ASSERT_TRUE(Zenith_Vulkan_IsDeviceAPIVersionSupported(VK_API_VERSION_1_1),
		"Vulkan 1.1 is the minimum supported physical-device API");
	ZENITH_ASSERT_EQ(Zenith_Vulkan_VMACompileVersionToAPIVersion(1002000u),
		static_cast<uint32_t>(VK_API_VERSION_1_2),
		"VMA's decimal compile-time version must convert to Vulkan API packing");
	ZENITH_ASSERT_EQ(Zenith_Vulkan_ResolveAllocatorAPIVersion(
		VK_API_VERSION_1_1, VK_API_VERSION_1_3),
		static_cast<uint32_t>(VK_API_VERSION_1_1),
		"VMA must use the selected older device API version");
	ZENITH_ASSERT_EQ(Zenith_Vulkan_ResolveAllocatorAPIVersion(
		VK_API_VERSION_1_2, VK_API_VERSION_1_3),
		static_cast<uint32_t>(VK_API_VERSION_1_2),
		"VMA must preserve a selected Vulkan 1.2 device API version");
	ZENITH_ASSERT_EQ(Zenith_Vulkan_ResolveAllocatorAPIVersion(
		VK_MAKE_API_VERSION(0, 1, 4, 0), VK_API_VERSION_1_3),
		static_cast<uint32_t>(VK_API_VERSION_1_3),
		"VMA must not exceed the version requested in VkApplicationInfo");
	ZENITH_ASSERT_EQ(Zenith_Vulkan_ResolveAllocatorAPIVersion(
		VK_API_VERSION_1_3, VK_API_VERSION_1_2),
		static_cast<uint32_t>(VK_API_VERSION_1_2),
		"a Vulkan 1.3 emulator must stay within a VMA build compiled for Vulkan 1.2");
}

ZENITH_TEST(VulkanDeviceSelection, RejectsSplitGraphicsAndComputeOnlyFamilies)
{
	const Zenith_Vulkan_QueueFamilySupport axFamilies[] = {
		{ true, false, true },
		{ false, true, false },
	};
	const auto xSelection = Zenith_Vulkan_SelectQueueFamilies(axFamilies, 2u);
	ZENITH_ASSERT_FALSE(xSelection.IsComplete(),
		"render-graph workers cannot dispatch compute through a graphics-only family");
}

ZENITH_TEST(VulkanDeviceSelection, SelectsComputeCapableGraphicsAndSeparatePresent)
{
	const Zenith_Vulkan_QueueFamilySupport axFamilies[] = {
		{ true, true, false },
		{ false, false, true },
		{ false, true, false },
	};
	const auto xSelection = Zenith_Vulkan_SelectQueueFamilies(axFamilies, 3u);
	ZENITH_ASSERT_TRUE(xSelection.IsComplete(), "all backend queue semantics are available");
	ZENITH_ASSERT_EQ(xSelection.m_uGraphics, 0u, "graphics uses the graphics+compute family");
	ZENITH_ASSERT_EQ(xSelection.m_uPresent, 1u, "presentation may use a separate family");
	ZENITH_ASSERT_EQ(xSelection.m_uCompute, 2u, "explicit compute prefers its dedicated family");
	ZENITH_ASSERT_EQ(xSelection.m_uCopy, 0u, "copy reuses graphics' implicit transfer support");
}

ZENITH_TEST(VulkanDeviceSelection, PrefersCombinedGraphicsComputePresentFamily)
{
	const Zenith_Vulkan_QueueFamilySupport axFamilies[] = {
		{ true, true, false },
		{ true, true, true },
	};
	const auto xSelection = Zenith_Vulkan_SelectQueueFamilies(axFamilies, 2u);
	ZENITH_ASSERT_TRUE(xSelection.IsComplete(), "combined family is a complete selection");
	ZENITH_ASSERT_EQ(xSelection.m_uGraphics, 1u, "combined presentation family is preferred");
	ZENITH_ASSERT_EQ(xSelection.m_uPresent, 1u, "presentation reuses the combined family");
	ZENITH_ASSERT_EQ(xSelection.m_uCompute, 1u, "compute reuses graphics without a dedicated family");
}

ZENITH_TEST(VulkanDeviceSelection, SeparatePresentationFamilyGetsNoCommandPool)
{
	// Topology A — the separate-presentation case this machine's GPU cannot
	// reproduce. Family 1 presents but supports neither graphics nor compute,
	// so it can own no command buffers: vkQueuePresentKHR takes none, and
	// creating a pool against a family with no command capability is invalid.
	const Zenith_Vulkan_QueueFamilySupport axSplit[] = {
		{ true,  true,  false },   // 0: graphics + compute, cannot present
		{ false, false, true  },   // 1: presentation only
		{ false, true,  false },   // 2: dedicated compute
	};
	const auto xSplit = Zenith_Vulkan_SelectQueueFamilies(axSplit, 3u);
	ZENITH_ASSERT_EQ(xSplit.m_uPresent, 1u, "presentation resolved to its own family");
	const auto xSplitPlan = Zenith_Vulkan_PlanCommandPools(
		xSplit.m_uGraphics, xSplit.m_uCompute, xSplit.m_uCopy, xSplit.m_uPresent);
	ZENITH_ASSERT_TRUE(xSplitPlan.m_bGraphics, "the graphics family records commands");
	ZENITH_ASSERT_TRUE(xSplitPlan.m_bCompute, "the dedicated compute family records commands");
	ZENITH_ASSERT_TRUE(xSplitPlan.m_bCopy, "copy reuses graphics, which records commands");
	ZENITH_ASSERT_FALSE(xSplitPlan.m_bPresent,
		"a separate presentation-only family must NOT be given a command pool");

	// Topology B — the common case. Presentation shares the graphics family,
	// so that slot keeps the same command-capable family's pool.
	const Zenith_Vulkan_QueueFamilySupport axShared[] = {
		{ true, true, true },
	};
	const auto xShared = Zenith_Vulkan_SelectQueueFamilies(axShared, 1u);
	ZENITH_ASSERT_EQ(xShared.m_uPresent, xShared.m_uGraphics,
		"presentation shares the combined family");
	const auto xSharedPlan = Zenith_Vulkan_PlanCommandPools(
		xShared.m_uGraphics, xShared.m_uCompute, xShared.m_uCopy, xShared.m_uPresent);
	ZENITH_ASSERT_TRUE(xSharedPlan.m_bPresent,
		"presentation sharing the graphics family keeps that family's pool");

	// An unresolved slot never gets a pool — CreateCommandPools must not turn
	// UINT32_MAX into a queue-family index.
	const auto xUnresolved = Zenith_Vulkan_PlanCommandPools(
		0u, 0u, 0u, uZENITH_VULKAN_INVALID_QUEUE_FAMILY);
	ZENITH_ASSERT_FALSE(xUnresolved.m_bPresent,
		"an unresolved presentation family must not be turned into a pool");
}

#endif
