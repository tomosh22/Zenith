#include "Zenith.h"
#include "Vulkan/Zenith_Vulkan_Platform.h"

#include <vulkan/vulkan.h>
#include <android/native_window.h>

// Forward declare - will be implemented in Android window implementation
extern ANativeWindow* Zenith_Android_GetNativeWindow();

namespace Zenith_Vulkan_Platform
{
	std::vector<const char*> GetRequiredInstanceExtensions()
	{
		std::vector<const char*> xExtensions;
		xExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		xExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);

#ifdef ZENITH_DEBUG
		xExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

		return xExtensions;
	}

	VkSurfaceKHR CreateSurface(VkInstance xInstance)
	{
		ANativeWindow* pxWindow = Zenith_Android_GetNativeWindow();

		VkAndroidSurfaceCreateInfoKHR xCreateInfo = {};
		xCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		xCreateInfo.window = pxWindow;

		VkSurfaceKHR xSurface;
		vkCreateAndroidSurfaceKHR(xInstance, &xCreateInfo, nullptr, &xSurface);
		return xSurface;
	}
}
