#pragma once

#include <cassert>

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

	static void Initialise(const char* szTitle, uint32_t uWidth, uint32_t uHeight) { s_pxInstance = new Zenith_Window(szTitle, uWidth, uHeight); }

	GLFWwindow* GetNativeWindow() const { return m_pxNativeWindow; }

	static Zenith_Window* GetInstance()
	{
		assert(s_pxInstance != nullptr && "Zenith_Window::GetInstance() called before Initialise()");
		return s_pxInstance;
	}

	void BeginFrame();

	bool ShouldClose() const { return glfwWindowShouldClose(m_pxNativeWindow); }
	void RequestClose() { glfwSetWindowShouldClose(m_pxNativeWindow, GLFW_TRUE); }

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
	// The live-device probe the input layer's overflow RESYNC reconciles its
	// event-fed held table against. Not the normal read path — game code asks
	// g_xEngine.Input(), whose table survives lifecycle barriers and cancels.
	bool IsKeyDown(Zenith_KeyCode iKey);

	// Logical-to-physical pixel ratio, for touch-target sizing. Desktop is
	// always 1.0; Android derives it from the screen density bucket.
	float GetDisplayScale() const { return 1.0f; }

	// GLFW memory tracking (tracked separately from normal allocations)
	static u_int64 GetGLFWMemoryAllocated();
	static u_int64 GetGLFWAllocationCount();

private:
	// Single funnel for cursor-mode changes (Enable/Disable/Toggle route here): only
	// acts on an actual mode change, then toggles raw motion + signals the input
	// discontinuity. See the .cpp for the rationale.
	void SetCursorCaptured(bool bCaptured);

	static Zenith_Window* s_pxInstance;

	GLFWwindow* m_pxNativeWindow = nullptr;
	bool m_bVSync;
	void(*m_pfnEventCallback)() = nullptr;

	void Shutdown();
};
