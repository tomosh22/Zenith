#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Profiling/Zenith_Profiling.h"

#include <android_native_app_glue.h>
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
		break;

	case APP_CMD_LOST_FOCUS:
		LOGI("APP_CMD_LOST_FOCUS");
		s_bAppActive = false;
		break;

	case APP_CMD_PAUSE:
		LOGI("APP_CMD_PAUSE");
		s_bAppActive = false;
		break;

	case APP_CMD_RESUME:
		LOGI("APP_CMD_RESUME");
		s_bAppActive = true;
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
	if (AInputEvent_getType(pxEvent) == AINPUT_EVENT_TYPE_MOTION)
	{
		int32_t iAction = AMotionEvent_getAction(pxEvent) & AMOTION_EVENT_ACTION_MASK;
		float fX = AMotionEvent_getX(pxEvent, 0);
		float fY = AMotionEvent_getY(pxEvent, 0);

		if (iAction == 0 || iAction == 1)
		{
			LOGI("Touch %s at (%.1f, %.1f)", iAction == 0 ? "DOWN" : "UP", fX, fY);
		}

		if (Zenith_Window::GetInstance())
		{
			Zenith_Window::GetInstance()->OnTouchEvent(iAction, fX, fY);
		}

		return 1; // Event handled
	}

	return 0; // Event not handled
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
			g_xEngine.Profiling().BeginFrame();
			Zenith_Core::Zenith_MainLoop();
			g_xEngine.Profiling().EndFrame();
		}
	}

	LOGI("android_main exiting");

	// Clean shutdown
	if (s_bEngineInitialised)
	{
		Zenith_Core::Zenith_Shutdown();
	}
}
