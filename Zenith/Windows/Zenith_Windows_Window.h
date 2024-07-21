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

	uint32_t GetWidth() const { return m_uWidth; }
	uint32_t GetHeight() const { return m_uHeight; }

	void SetWidth(uint32_t uWidth)  { m_uWidth = uWidth; }
	void SetHeight(uint32_t uHeight)  { m_uHeight = uHeight; }

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
	uint32_t m_uWidth, m_uHeight;
	bool m_bVSync;
	void(*m_pfnEventCallback)() = nullptr;



	void Shutdown();
};