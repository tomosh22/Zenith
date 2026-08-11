#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Profiling/Zenith_Profiling.h"

#include <android_native_app_glue.h>
#include <android/keycodes.h>            // AKEYCODE_BACK
#include <android/log.h>
#include <unistd.h>	// chdir -- see android_main

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Zenith", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "Zenith", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "Zenith", __VA_ARGS__))

static bool s_bEngineInitialised = false;
static bool s_bWindowReady = false;
static bool s_bAppActive = true;
static bool s_bDestroyRequested = false;

static void InitialiseEngine()
{
	if (s_bEngineInitialised)
	{
		return;
	}

	LOGI("Initialising Zenith Engine...");

	// Window is already initialised via SetNativeWindow
	Zenith_Core::Zenith_Init();

	s_bEngineInitialised = true;
	LOGI("Zenith Engine initialised successfully");
}

// Activity lifecycle -> input lifecycle. A RESET raises the barrier that cancels
// every held key/button/pointer and DISARMS input until the matching ARM: an app
// that is paused, backgrounded or has lost its window must not come back with a
// finger still "down" from before. Guarded on initialisation because the glue
// delivers commands before Zenith_Init has allocated the engine subsystems, and
// g_xEngine accessors are undefined until then.
static void NotifyInputLifecycle(bool bArmed)
{
	if (!s_bEngineInitialised || Zenith_Window::GetInstance() == nullptr)
	{
		return;
	}
	Zenith_Window::GetInstance()->OnLifecycleEvent(bArmed);
}

static void OnAppCmd(android_app* pxApp, int32_t iCmd)
{
	switch (iCmd)
	{
	case APP_CMD_INIT_WINDOW:
		LOGI("APP_CMD_INIT_WINDOW");
		if (pxApp->window != nullptr)
		{
			int32_t iNativeWidth = ANativeWindow_getWidth(pxApp->window);
			int32_t iNativeHeight = ANativeWindow_getHeight(pxApp->window);
			LOGI("Native window size: %dx%d", iNativeWidth, iNativeHeight);

			Zenith_Window::GetInstance()->SetNativeWindow(pxApp->window);
			s_bWindowReady = true;

			if (!s_bEngineInitialised)
			{
				InitialiseEngine();
			}
			else
			{
				// Window was recreated - need to recreate Vulkan surface
				// This happens when app is resumed from background
				LOGI("Window recreated - recreating Vulkan surface");
				// Flux will handle this via swapchain recreation on next frame
			}
		}
		break;

	case APP_CMD_TERM_WINDOW:
		LOGI("APP_CMD_TERM_WINDOW");
		s_bWindowReady = false;
		NotifyInputLifecycle(false);
		if (Zenith_Window::GetInstance())
		{
			Zenith_Window::GetInstance()->SetNativeWindow(nullptr);
		}
		break;

	case APP_CMD_WINDOW_RESIZED:
		LOGI("APP_CMD_WINDOW_RESIZED");
		// Flux will handle swapchain recreation automatically
		break;

	case APP_CMD_GAINED_FOCUS:
		LOGI("APP_CMD_GAINED_FOCUS");
		s_bAppActive = true;
		NotifyInputLifecycle(true);
		break;

	case APP_CMD_LOST_FOCUS:
		LOGI("APP_CMD_LOST_FOCUS");
		s_bAppActive = false;
		NotifyInputLifecycle(false);
		break;

	case APP_CMD_PAUSE:
		LOGI("APP_CMD_PAUSE");
		s_bAppActive = false;
		NotifyInputLifecycle(false);
		break;

	case APP_CMD_RESUME:
		LOGI("APP_CMD_RESUME");
		s_bAppActive = true;
		NotifyInputLifecycle(true);
		break;

	case APP_CMD_DESTROY:
		LOGI("APP_CMD_DESTROY");
		s_bDestroyRequested = true;
		break;

	case APP_CMD_LOW_MEMORY:
		LOGW("APP_CMD_LOW_MEMORY");
		// Could trigger asset cleanup here
		break;

	default:
		break;
	}
}

static int32_t OnInputEvent(android_app* pxApp, AInputEvent* pxEvent)
{
	(void)pxApp;

	// The glue can deliver input before Zenith_Init has built the engine; every
	// path below reaches g_xEngine through the window funnels.
	if (!s_bEngineInitialised || Zenith_Window::GetInstance() == nullptr)
	{
		return 0;
	}

	const int32_t iType = AInputEvent_getType(pxEvent);

	if (iType == AINPUT_EVENT_TYPE_KEY)
	{
		const int32_t iKeyCode = AKeyEvent_getKeyCode(pxEvent);
		Zenith_Window::GetInstance()->OnKeyEvent(AKeyEvent_getAction(pxEvent), iKeyCode);
		// BACK is CONSUMED iff the running game actually bound it (WP3b). The old
		// unconditional `return 0` was right while nothing consumed SYSTEM_BACK:
		// swallowing it then would have left the activity with no way to finish.
		// Now a game that binds it (Zenithmon's CANCEL) wants Back to close its
		// menu, and a game that does NOT bind it must keep the platform's own
		// behaviour -- so the answer is a QUESTION, never a constant.
		//
		// It is asked of the DEVICE layer through the same window funnel every
		// other input path here uses: the action layer stamps the flag when a
		// SYSTEM_BACK row is registered, so this file names no Input type and adds
		// no up-edge out of the platform layer. Unregistered (i.e. before the game
		// registers its bindings) reads false, which is the safe direction: an
		// un-bound Back backgrounds the app exactly as it always did.
		return (iKeyCode == AKEYCODE_BACK
			&& Zenith_Window::GetInstance()->HasSystemBackBinding()) ? 1 : 0;
	}

	if (iType != AINPUT_EVENT_TYPE_MOTION)
	{
		return 0;
	}

	// Real touch only: a mouse / trackball / joystick motion event would
	// otherwise masquerade as a finger.
	const int32_t iSource = AInputEvent_getSource(pxEvent);
	if ((iSource & AINPUT_SOURCE_TOUCHSCREEN) != AINPUT_SOURCE_TOUCHSCREEN)
	{
		return 0;
	}

	const int32_t iRawAction = AMotionEvent_getAction(pxEvent);
	const int32_t iAction = iRawAction & AMOTION_EVENT_ACTION_MASK;

	// FULL multi-touch (WP1). Android reports one MotionEvent covering every
	// pointer currently on the glass; which one an event is ABOUT lives in the
	// action word's index field, and the coordinates of all the others live in
	// the same event under their own indices. Getting this wrong is what kept
	// the platform single-touch: reading index 0 for a POINTER_DOWN attributes
	// the second finger's coordinates to the first.
	switch (iAction)
	{
	case AMOTION_EVENT_ACTION_DOWN:
	case AMOTION_EVENT_ACTION_UP:
	case AMOTION_EVENT_ACTION_POINTER_DOWN:
	case AMOTION_EVENT_ACTION_POINTER_UP:
	{
		// DOWN/UP name exactly ONE pointer. For the primary pair the index is
		// always 0; for the secondary pair it is carried in the action word.
		const int32_t iPointerIndex = (iAction == AMOTION_EVENT_ACTION_POINTER_DOWN
			|| iAction == AMOTION_EVENT_ACTION_POINTER_UP)
			? ((iRawAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT)
			: 0;

		const int32_t iToolType = AMotionEvent_getToolType(pxEvent, static_cast<size_t>(iPointerIndex));
		if (iToolType != AMOTION_EVENT_TOOL_TYPE_FINGER && iToolType != AMOTION_EVENT_TOOL_TYPE_STYLUS)
		{
			return 0;
		}

		const int32_t iPointerId = AMotionEvent_getPointerId(pxEvent, static_cast<size_t>(iPointerIndex));
		const float fX = AMotionEvent_getX(pxEvent, static_cast<size_t>(iPointerIndex));
		const float fY = AMotionEvent_getY(pxEvent, static_cast<size_t>(iPointerIndex));

		LOGI("Touch %s id=%d at (%.1f, %.1f)",
			(iAction == AMOTION_EVENT_ACTION_DOWN || iAction == AMOTION_EVENT_ACTION_POINTER_DOWN) ? "DOWN" : "UP",
			iPointerId, fX, fY);

		Zenith_Window::GetInstance()->OnTouchEvent(iAction, iPointerId, fX, fY);
		return 1;
	}

	case AMOTION_EVENT_ACTION_MOVE:
	{
		// A MOVE is about EVERY pointer at once — there is no per-pointer move
		// event on this platform. Emit one per tracked pointer; the FIFO coalesces
		// the redundant ones per pointer id, so a fast finger costs one entry.
		const size_t uPointerCount = AMotionEvent_getPointerCount(pxEvent);
		bool bHandled = false;
		for (size_t u = 0; u < uPointerCount; u++)
		{
			const int32_t iToolType = AMotionEvent_getToolType(pxEvent, u);
			if (iToolType != AMOTION_EVENT_TOOL_TYPE_FINGER && iToolType != AMOTION_EVENT_TOOL_TYPE_STYLUS)
			{
				continue;
			}
			Zenith_Window::GetInstance()->OnTouchEvent(iAction,
				AMotionEvent_getPointerId(pxEvent, u),
				AMotionEvent_getX(pxEvent, u),
				AMotionEvent_getY(pxEvent, u));
			bHandled = true;
		}
		return bHandled ? 1 : 0;
	}

	case AMOTION_EVENT_ACTION_CANCEL:
	{
		// The gesture was taken over by the system (a system gesture, a parent
		// view). EVERY pointer dies, and none of them produce an UP — a cancel
		// that only reached the primary finger is exactly how a phantom
		// secondary pointer gets stuck down forever.
		const size_t uPointerCount = AMotionEvent_getPointerCount(pxEvent);
		for (size_t u = 0; u < uPointerCount; u++)
		{
			Zenith_Window::GetInstance()->OnTouchEvent(iAction,
				AMotionEvent_getPointerId(pxEvent, u),
				AMotionEvent_getX(pxEvent, u),
				AMotionEvent_getY(pxEvent, u));
		}
		LOGI("Touch CANCEL for %d pointer(s)", static_cast<int>(uPointerCount));
		return 1;
	}

	default:
		return 0;
	}
}

void android_main(android_app* pxApp)
{
	LOGI("android_main started");

	// Initialize file access with AAssetManager for reading APK assets
	Zenith_FileAccess::InitialisePlatform(pxApp->activity->assetManager);

	// Make internalDataPath the working directory so RELATIVE paths work too.
	//
	// An Android process starts with cwd "/", which is read-only. Every engine
	// path that is relative rather than routed through Zenith_FileAccess --
	// the boot-profile/memory dumps, and above all the unit-test batch that
	// Zenith_Init runs at every boot (`create_directories("TestData")` and
	// friends) -- therefore resolved against an unwritable root. That is not a
	// soft failure: std::filesystem throws, exceptions are disabled on this
	// platform, and the process terminates during Zenith_Init.
	const char* szInternalDataPath = pxApp->activity->internalDataPath;
	if (szInternalDataPath == nullptr || szInternalDataPath[0] == '\0'
		|| chdir(szInternalDataPath) != 0)
	{
		LOGE("Cannot enter internalDataPath '%s'; aborting before engine initialisation",
			szInternalDataPath != nullptr ? szInternalDataPath : "(null)");
		ANativeActivity_finish(pxApp->activity);

		// Do NOT just return. The framework's UI thread parks in
		// android_app_set_activity_state waiting for THIS thread to consume the
		// lifecycle command off the cmd pipe (only android_app_pre_exec_cmd
		// assigns activityState, and it is reachable only from process_cmd
		// here). Returning without draining leaves the UI thread blocked and the
		// activity ANRs instead of finishing. Pump until the framework's destroy
		// arrives, then fall through to the normal exit -- nothing was
		// initialised, so there is nothing to shut down.
		while (!pxApp->destroyRequested)
		{
			int iEvents = 0;
			android_poll_source* pxSource = nullptr;
			const int iPollResult = ALooper_pollOnce(-1, nullptr, &iEvents, (void**)&pxSource);
			if (iPollResult == ALOOPER_POLL_ERROR)
			{
				// A broken looper never delivers the destroy, and re-polling it
				// would spin at 100% CPU forever -- recreating the very ANR this
				// drain exists to prevent. Give up and let the process go.
				LOGE("Looper error while draining after chdir failure; abandoning drain");
				break;
			}
			if (pxSource != nullptr)
			{
				pxSource->process(pxApp, pxSource);
			}
		}

		LOGI("android_main exiting (internalDataPath unusable)");
		return;
	}

	// Set writable directory for file writes (saves, unit test output, etc.)
	Zenith_FileAccess::SetWritableDirectory(szInternalDataPath);

	// Store app state for window class
	Zenith_Window::SetAndroidApp(pxApp);

	// Initialise window with placeholder dimensions
	// The actual screen size is set in APP_CMD_INIT_WINDOW via SetNativeWindow
	// Graphics options are populated inside Zenith_Init() for all platforms
	Zenith_Window::Initialise("Zenith", 0, 0);

	// Set up callbacks
	pxApp->onAppCmd = OnAppCmd;
	pxApp->onInputEvent = OnInputEvent;

	// Main loop
	while (!s_bDestroyRequested)
	{
		// Process pending events
		int iEvents;
		android_poll_source* pxSource;

		// Use blocking poll when paused, non-blocking when active
		int iTimeout = s_bAppActive ? 0 : -1;

		while (ALooper_pollOnce(iTimeout, nullptr, &iEvents, (void**)&pxSource) >= 0)
		{
			if (pxSource != nullptr)
			{
				pxSource->process(pxApp, pxSource);
			}

			if (pxApp->destroyRequested)
			{
				s_bDestroyRequested = true;
				break;
			}

			// Only use blocking timeout on first iteration when paused
			iTimeout = 0;
		}

		// Only run game loop when active and window is ready
		if (s_bAppActive && s_bWindowReady && s_bEngineInitialised && !s_bDestroyRequested)
		{
			// One reference, two calls. This file's g_xEngine budget is shrink-only
			// (Tools/engine_singleton_allowlist.txt), and this is now the ONLY reach
			// left in the translation unit -- the Back-consume question moved down to
			// the device layer and travels the window funnel like every other input
			// path here, so it costs this file nothing.
			Zenith_Profiling& xProfiling = g_xEngine.Profiling();
			xProfiling.BeginFrame();
			Zenith_Core::Zenith_MainLoop();
			xProfiling.EndFrame();
		}
	}

	LOGI("android_main exiting");

	// Clean shutdown
	if (s_bEngineInitialised)
	{
		Zenith_Core::Zenith_Shutdown();
	}
}
