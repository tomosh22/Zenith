#include "Zenith.h"
#include "Vulkan/Zenith_Vulkan_Platform.h"
#include "Windows/Zenith_Windows_Window.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Zenith_Vulkan_Platform
{
	std::vector<const char*> GetRequiredInstanceExtensions()
	{
		uint32_t uGLFWExtensionCount = 0;
		const char** pszGLFWExtensions = glfwGetRequiredInstanceExtensions(&uGLFWExtensionCount);
		std::vector<const char*> xExtensions(pszGLFWExtensions, pszGLFWExtensions + uGLFWExtensionCount);

#ifdef ZENITH_DEBUG
		xExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

		return xExtensions;
	}

	VkSurfaceKHR CreateSurface(VkInstance xInstance)
	{
		GLFWwindow* pxWindow = Zenith_Window::GetInstance()->GetNativeWindow();
		VkSurfaceKHR xSurface;
		glfwCreateWindowSurface(xInstance, pxWindow, nullptr, &xSurface);
		return xSurface;
	}
}
