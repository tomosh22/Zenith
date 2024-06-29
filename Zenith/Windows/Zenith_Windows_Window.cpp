#include "Zenith.h"

#include "Windows/Zenith_Windows_Window.h"

#include "Input/Zenith_Input.h"

Zenith_Windows_Window* Zenith_Windows_Window::s_pxInstance = nullptr;

static void ErrorCallback(int32_t iError, const char* szDesc)
{
	__debugbreak();
}

static void KeyCallback(GLFWwindow* pxWindow, int32_t iKey, int32_t iScancode, int32_t iAction, int32_t iMods)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		Zenith_Input::KeyPressedCallback(static_cast<uint32_t>(iKey));
		break;
	}
}

static void MouseCallback(GLFWwindow* pxWindow, int32_t iKey, int32_t iAction, int32_t iMods)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		Zenith_Input::MouseButtonPressedCallback(static_cast<uint32_t>(iKey));
		break;
	}
}

Zenith_Windows_Window::Zenith_Windows_Window(const char* szTitle, uint32_t uWidth, uint32_t uHeight)
{
	int glfwinit = glfwInit();

#ifdef ZENITH_VULKAN
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

	glfwSetErrorCallback(ErrorCallback);

	m_pxNativeWindow = glfwCreateWindow(static_cast<int32_t>(uWidth), static_cast<int32_t>(uHeight), szTitle, nullptr, nullptr);

	glfwSetKeyCallback(m_pxNativeWindow, KeyCallback);
	glfwSetMouseButtonCallback(m_pxNativeWindow, MouseCallback);

	Zenith_Log("Window created");
}

Zenith_Windows_Window::~Zenith_Windows_Window()
{

}

