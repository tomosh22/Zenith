#pragma once

#include <android/native_window.h>
#include "Input/Zenith_KeyCodes.h"

// Forward declare android_app
struct android_app;

class Zenith_Window
{
public:
	Zenith_Window(const char* szTitle, uint32_t uWidth, uint32_t uHeight);
	virtual ~Zenith_Window();

	static void Inititalise(const char* szTitle, uint32_t uWidth, uint32_t uHeight);
	static void SetAndroidApp(android_app* pxApp);

	ANativeWindow* GetNativeWindow() const { return m_pxNativeWindow; }

	static Zenith_Window* GetInstance() { return s_pxInstance; }

	void BeginFrame();

	void ToggleCaptureCursor() {}
	void EnableCaptureCursor() {}
	void DisableCaptureCursor() {}
	bool IsCursorCaptured() { return false; }

	void GetSize(int32_t& iWidth, int32_t& iHeight);

	void SetEventCallback(void(*pfnEventCallback)()) {
		m_pfnEventCallback = pfnEventCallback;
	}
	void SetVSync(bool bEnabled) { m_bVSync = bEnabled; }
	bool GetVSyncEnabled() const { return m_bVSync; }

	void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	bool IsKeyDown(Zenith_KeyCode iKey);

	// Android-specific
	void SetNativeWindow(ANativeWindow* pxWindow);
	bool IsWindowReady() const { return m_pxNativeWindow != nullptr; }

	// Touch to mouse translation
	void OnTouchEvent(int32_t iAction, float fX, float fY);

private:
	static Zenith_Window* s_pxInstance;
	static android_app* s_pxAndroidApp;

	ANativeWindow* m_pxNativeWindow = nullptr;
	bool m_bVSync = true;
	void(*m_pfnEventCallback)() = nullptr;

	// Touch state for mouse emulation
	float m_fTouchX = 0.0f;
	float m_fTouchY = 0.0f;
	bool m_bTouchDown = false;

	int32_t m_iWidth = 0;
	int32_t m_iHeight = 0;
};

// Function to get native window for Vulkan surface creation
ANativeWindow* Zenith_Android_GetNativeWindow();
