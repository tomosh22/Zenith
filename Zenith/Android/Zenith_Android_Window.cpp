#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Zenith_Android_Window.h"
#include "Input/Zenith_Input.h"

#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

Zenith_Window* Zenith_Window::s_pxInstance = nullptr;
android_app* Zenith_Window::s_pxAndroidApp = nullptr;

// Single engine-singleton funnel for this TU. The window layer is a pure
// forwarder into the input device layer, so one accessor keeps g_xEngine's reach
// into the platform layer at exactly one line no matter how many funnels it grows.
static Zenith_Input& PlatformInput()
{
	return g_xEngine.Input();
}

Zenith_Window::Zenith_Window(const char* szTitle, uint32_t uWidth, uint32_t uHeight)
	: m_iWidth(uWidth)
	, m_iHeight(uHeight)
{
	// On Android, the native window is set later via SetNativeWindow
	// when the ANativeWindow becomes available
}

Zenith_Window::~Zenith_Window()
{
	// Android manages its own window lifecycle
}

void Zenith_Window::Initialise(const char* szTitle, uint32_t uWidth, uint32_t uHeight)
{
	s_pxInstance = new Zenith_Window(szTitle, uWidth, uHeight);
}

void Zenith_Window::SetAndroidApp(android_app* pxApp)
{
	s_pxAndroidApp = pxApp;
}

void Zenith_Window::SetNativeWindow(ANativeWindow* pxWindow)
{
	m_pxNativeWindow = pxWindow;
	if (pxWindow)
	{
		m_iWidth = ANativeWindow_getWidth(pxWindow);
		m_iHeight = ANativeWindow_getHeight(pxWindow);
	}
}

void Zenith_Window::BeginFrame()
{
	// Android event processing is handled in android_main (the ALooper pump runs
	// before Zenith_MainLoop is entered), so there is no pump to perform here.
}

void Zenith_Window::GetSize(int32_t& iWidth, int32_t& iHeight)
{
	if (m_pxNativeWindow)
	{
		iWidth = ANativeWindow_getWidth(m_pxNativeWindow);
		iHeight = ANativeWindow_getHeight(m_pxNativeWindow);
	}
	else
	{
		iWidth = m_iWidth;
		iHeight = m_iHeight;
	}
}

void Zenith_Window::GetMousePosition(Zenith_Maths::Vector2_64& xOut)
{
	// B3 (permanent): the primary pointer IS the mouse view on this platform.
	PlatformInput().GetProjectedPointerPosition(xOut);
}

float Zenith_Window::GetDisplayScale() const
{
	if (s_pxAndroidApp == nullptr || s_pxAndroidApp->config == nullptr)
	{
		return 1.0f;
	}

	const int32_t iDensity = AConfiguration_getDensity(s_pxAndroidApp->config);
	// ACONFIGURATION_DENSITY_DEFAULT (160 dpi) is scale 1.0. ANY/NONE/0 mean
	// "unknown", not "vanishingly small" -- treat them as 1.0.
	if (iDensity <= 0
		|| iDensity == ACONFIGURATION_DENSITY_ANY
		|| iDensity == ACONFIGURATION_DENSITY_NONE)
	{
		return 1.0f;
	}
	return static_cast<float>(iDensity) / 160.0f;
}

void Zenith_Window::OnTouchEvent(int32_t iAction, int32_t iPointerId, float fX, float fY)
{
	// Translate + ENQUEUE only. The FIFO drain (after the swapchain acquire)
	// stages these into the touch stream and maintains the B3 projection, which
	// is what makes an Android press edge visible to every existing consumer.
	Zenith_InputEventType eType = INPUT_EVENT_NONE;
	switch (iAction)
	{
	case AMOTION_EVENT_ACTION_DOWN:
	case AMOTION_EVENT_ACTION_POINTER_DOWN:
		eType = INPUT_EVENT_TOUCH_DOWN;
		break;
	case AMOTION_EVENT_ACTION_UP:
	case AMOTION_EVENT_ACTION_POINTER_UP:
		eType = INPUT_EVENT_TOUCH_UP;
		break;
	case AMOTION_EVENT_ACTION_MOVE:
		eType = INPUT_EVENT_TOUCH_MOVE;
		break;
	case AMOTION_EVENT_ACTION_CANCEL:
		eType = INPUT_EVENT_TOUCH_CANCEL;
		break;
	default:
		return;
	}

	PlatformInput().TouchEventCallback(eType, iPointerId, fX, fY);
}

void Zenith_Window::OnKeyEvent(int32_t iAction, int32_t iKeyCode)
{
	// Only the hardware/gesture BACK is meaningful to the engine today: it is
	// system NAVIGATION, not a key, so it rides its own event type and never
	// lands in the key domain. There is no Android-to-Zenith keycode table yet;
	// anything else is left to the platform.
	if (iKeyCode != AKEYCODE_BACK || iAction != AKEY_EVENT_ACTION_DOWN)
	{
		return;
	}
	PlatformInput().SystemBackCallback();
}

void Zenith_Window::OnLifecycleEvent(bool bArmed)
{
	if (bArmed)
	{
		PlatformInput().LifecycleArmCallback();
	}
	else
	{
		PlatformInput().LifecycleResetCallback();
	}
}

ANativeWindow* Zenith_Android_GetNativeWindow()
{
	if (Zenith_Window::GetInstance())
	{
		return Zenith_Window::GetInstance()->GetNativeWindow();
	}
	return nullptr;
}
