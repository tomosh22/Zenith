#include "Core/Zenith_TestFramework.h"
#include "Flux/Backend/Flux_IndirectDraw.h"
#include "Vulkan/Zenith_Vulkan_IndirectCount.h"

#ifdef ZENITH_TESTING

ZENITH_TEST(VulkanIndirectCount, Pre12KHRSelectsExtensionRoute)
{
	const auto x = Zenith_Vulkan_SelectIndirectCountRoute(false, false, true);
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(x.m_eRoute),
		static_cast<uint32_t>(Zenith_Vulkan_IndirectCountRoute::KHR_EXTENSION),
		"a pre-1.2 device can use the advertised KHR route");
	ZENITH_ASSERT_TRUE(x.m_bEnableKHRExtension, "the selected KHR route enables its extension");
	ZENITH_ASSERT_FALSE(x.m_bEnableCoreFeature, "the pre-1.2 route cannot enable the core feature");
}

ZENITH_TEST(VulkanIndirectCount, Vulkan12CoreFeatureSelectsCoreRoute)
{
	const auto x = Zenith_Vulkan_SelectIndirectCountRoute(true, true, false);
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(x.m_eRoute),
		static_cast<uint32_t>(Zenith_Vulkan_IndirectCountRoute::CORE_1_2),
		"Vulkan 1.2 plus drawIndirectCount selects the promoted core route");
	ZENITH_ASSERT_TRUE(x.m_bEnableCoreFeature, "the core route enables drawIndirectCount");
	ZENITH_ASSERT_FALSE(x.m_bEnableKHRExtension, "the core route needs no KHR extension");
}

ZENITH_TEST(VulkanIndirectCount, BothAdvertisedPreferCoreRoute)
{
	const auto x = Zenith_Vulkan_SelectIndirectCountRoute(true, true, true);
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(x.m_eRoute),
		static_cast<uint32_t>(Zenith_Vulkan_IndirectCountRoute::CORE_1_2),
		"when both aliases are advertised the promoted core route is canonical");
	ZENITH_ASSERT_FALSE(x.m_bEnableKHRExtension, "the unused extension must not be enabled");
}

ZENITH_TEST(VulkanIndirectCount, NeitherAdvertisedHasNoRoute)
{
	const auto x = Zenith_Vulkan_SelectIndirectCountRoute(true, false, false);
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(x.m_eRoute),
		static_cast<uint32_t>(Zenith_Vulkan_IndirectCountRoute::NONE),
		"without an advertised core feature or KHR extension no native route exists");
	ZENITH_ASSERT_FALSE(Zenith_Vulkan_IsIndirectCountRouteUsable(x.m_eRoute, true, true),
		"a resolved-looking proc cannot promote an unadvertised route");
}

ZENITH_TEST(VulkanIndirectCount, AdvertisedRouteWithNullProcIsNotUsable)
{
	const auto xCore = Zenith_Vulkan_SelectIndirectCountRoute(true, true, false);
	ZENITH_ASSERT_FALSE(Zenith_Vulkan_IsIndirectCountRouteUsable(xCore.m_eRoute, false, true),
		"a null selected core proc makes the core route unusable");
	const auto xKHR = Zenith_Vulkan_SelectIndirectCountRoute(false, false, true);
	ZENITH_ASSERT_FALSE(Zenith_Vulkan_IsIndirectCountRouteUsable(xKHR.m_eRoute, true, false),
		"a null selected KHR proc makes the extension route unusable");
}

ZENITH_TEST(VulkanIndirectCount, CountRouteIsIndependentOfFixedMultiDraw)
{
	const auto xRoute = Zenith_Vulkan_SelectIndirectCountRoute(true, true, false);
	const Flux_IndirectDrawCapabilities xCaps{
		Zenith_Vulkan_IsIndirectCountRouteUsable(xRoute.m_eRoute, true, false),
		false, // multiDrawIndirect gates fixed commands only
		true,
		true,
		4096u,
	};
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(Flux_SelectIndirectExecutionMode(xCaps, 4096u,
		Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::AUTO)),
		static_cast<uint32_t>(Flux_IndirectExecutionMode::NATIVE_COUNT),
		"a usable Vulkan count route must remain native without fixed multiDrawIndirect");
	ZENITH_ASSERT_EQ(Flux_ResolveFixedDrawPerCallLimit(xCaps), 1u,
		"the same Vulkan device must clamp fixed indirect calls to one record");
}

#endif
