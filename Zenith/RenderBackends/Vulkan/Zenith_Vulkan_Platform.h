#pragma once

#include <vector>

// Forward declare Vulkan types to avoid including vulkan.hpp in header
typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;

// Platform-agnostic Vulkan functions
// Declaration only - implementations in platform-specific .cpp files:
// Windows: Zenith/Windows/Vulkan/Zenith_Windows_Vulkan.cpp
// Android: Zenith/Android/Vulkan/Zenith_Android_Vulkan.cpp

namespace Zenith_Vulkan_Platform
{
	// Returns the required Vulkan instance extensions for the current platform
	// Windows: Uses glfwGetRequiredInstanceExtensions()
	// Android: Returns VK_KHR_SURFACE_EXTENSION_NAME and VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
	std::vector<const char*> GetRequiredInstanceExtensions();

	// Creates a Vulkan surface for the current window
	// Windows: Uses glfwCreateWindowSurface()
	// Android: Uses vkCreateAndroidSurfaceKHR()
	VkSurfaceKHR CreateSurface(VkInstance xInstance);
}
