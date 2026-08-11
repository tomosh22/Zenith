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

	static void Initialise(const char* szTitle, uint32_t uWidth, uint32_t uHeight);
	static void SetAndroidApp(android_app* pxApp);
	static android_app* GetAndroidApp() { return s_pxAndroidApp; }

	ANativeWindow* GetNativeWindow() const { return m_pxNativeWindow; }

	static Zenith_Window* GetInstance() { return s_pxInstance; }

	void BeginFrame();

	void ToggleCaptureCursor() {}
	void EnableCaptureCursor() {}
	void DisableCaptureCursor() {}
	bool IsCursorCaptured() { return false; }

	// On Windows (GLFW) this sets a should-close flag to exit the desktop poll
	// loop -- the shared automated-test driver (Zenith_AutomatedTest.cpp) calls it
	// after a run. Android's lifecycle is driven by the NativeActivity / glue main
	// loop, not by this call, so it is a no-op here; it exists only to satisfy the
	// cross-platform Zenith_Window API the driver compiles against on both platforms.
	void RequestClose() {}

	void GetSize(int32_t& iWidth, int32_t& iHeight);

	void SetEventCallback(void(*pfnEventCallback)()) {
		m_pfnEventCallback = pfnEventCallback;
	}
	void SetVSync(bool bEnabled) { m_bVSync = bEnabled; }
	bool GetVSyncEnabled() const { return m_bVSync; }

	// B3 (permanent): the FIRST touch feeds the mouse view. The position is owned
	// by Zenith_Input (maintained at drain from the staged touch stream), not by
	// this class -- the platform funnels below only ENQUEUE.
	void GetMousePosition(Zenith_Maths::Vector2_64& xOut);

	// Logical-to-physical pixel ratio, for touch-target sizing. Derived from the
	// activity's density bucket (160 dpi == 1.0); 1.0 when unknown.
	float GetDisplayScale() const;

	// Does the running game bind the platform's Back gesture to anything? Owned by
	// the DEVICE layer (Zenith_Input), which the action layer stamps when a
	// SYSTEM_BACK binding is registered; this is the same pure forward as the
	// funnels below. android_main asks THIS rather than the action layer directly,
	// because the platform entry point sits below Input in the layer DAG and this
	// class is the one seam it is allowed to reach through. Callers guard on engine
	// initialisation exactly as they do for the funnels.
	bool HasSystemBackBinding() const;

	// Android-specific
	void SetNativeWindow(ANativeWindow* pxWindow);
	bool IsWindowReady() const { return m_pxNativeWindow != nullptr; }

	// ---- Platform funnels: translate + ENQUEUE, never mutate state ----------
	// iAction is the raw AMOTION_EVENT_ACTION_* / AKEY_EVENT_ACTION_* value; the
	// mapping to the engine's event types lives in the .cpp so this header stays
	// free of engine input headers (it is reached through the PCH).
	void OnTouchEvent(int32_t iAction, int32_t iPointerId, float fX, float fY);
	// AKEYCODE_BACK becomes a dedicated SYSTEM_BACK event; everything else is
	// ignored for now (there is no Android keycode -> Zenith keycode table yet).
	void OnKeyEvent(int32_t iAction, int32_t iKeyCode);
	// Activity lifecycle: false raises a LIFECYCLE_RESET barrier (cancel + disarm),
	// true re-arms.
	void OnLifecycleEvent(bool bArmed);

private:
	static Zenith_Window* s_pxInstance;
	static android_app* s_pxAndroidApp;

	ANativeWindow* m_pxNativeWindow = nullptr;
	bool m_bVSync = true;
	void(*m_pfnEventCallback)() = nullptr;

	int32_t m_iWidth = 0;
	int32_t m_iHeight = 0;
};

// Function to get native window for Vulkan surface creation
ANativeWindow* Zenith_Android_GetNativeWindow();
