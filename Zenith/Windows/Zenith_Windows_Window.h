#pragma once

#ifdef ZENITH_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include "GLFW/glfw3.h"
#ifdef ZENITH_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "Input/Zenith_KeyCodes.h"

class Zenith_Window
{
public:
	Zenith_Window(const char* szTitle, uint32_t uWidth, uint32_t uHeight);
	virtual ~Zenith_Window();

	static void Inititalise(const char* szTitle, uint32_t uWidth, uint32_t uHeight) { s_pxInstance = new Zenith_Window(szTitle, uWidth, uHeight); }

	GLFWwindow* GetNativeWindow() const { return m_pxNativeWindow; }

	static Zenith_Window* GetInstance() { return s_pxInstance; }

	void BeginFrame();

	void ToggleCaptureCursor();
	void EnableCaptureCursor();
	void DisableCaptureCursor();
	bool IsCursorCaptured();

	void GetSize(int32_t& iWidth, int32_t& iHeight) { glfwGetWindowSize(m_pxNativeWindow, &iWidth, &iHeight); }

	void SetEventCallback(void(*pfnEventCallback)()) {
		m_pfnEventCallback = pfnEventCallback;
	}
	void SetVSync(bool bEnabled) { m_bVSync = bEnabled; };
	bool GetVSyncEnabled() const { return m_bVSync; };

	void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	bool IsKeyDown(Zenith_KeyCode iKey);

private:
	static Zenith_Window* s_pxInstance;

	GLFWwindow* m_pxNativeWindow = nullptr;
	bool m_bVSync;
	void(*m_pfnEventCallback)() = nullptr;

	void Shutdown();
};