#include "Zenith.h"
#include "Zenith_Android_Window.h"
#include "Input/Zenith_Input.h"

#include <android/native_window.h>
#include <android_native_app_glue.h>

Zenith_Window* Zenith_Window::s_pxInstance = nullptr;
android_app* Zenith_Window::s_pxAndroidApp = nullptr;

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

void Zenith_Window::Inititalise(const char* szTitle, uint32_t uWidth, uint32_t uHeight)
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
	// Android event processing is handled in android_main
	// This is called each frame to allow any per-frame window processing
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
	// Return touch position as mouse position
	xOut.x = static_cast<double>(m_fTouchX);
	xOut.y = static_cast<double>(m_fTouchY);
}

bool Zenith_Window::IsKeyDown(Zenith_KeyCode iKey)
{
	if (iKey <= ZENITH_MOUSE_BUTTON_LAST)
	{
		// Treat touch as left mouse button
		if (iKey == ZENITH_MOUSE_BUTTON_LEFT)
			return m_bTouchDown;
		return false;
	}
	return false;
}

void Zenith_Window::OnTouchEvent(int32_t iAction, float fX, float fY)
{
	m_fTouchX = fX;
	m_fTouchY = fY;

	// Map touch events to mouse button states
	// iAction values from android/input.h:
	// AMOTION_EVENT_ACTION_DOWN = 0
	// AMOTION_EVENT_ACTION_UP = 1
	// AMOTION_EVENT_ACTION_MOVE = 2
	switch (iAction)
	{
	case 0: // AMOTION_EVENT_ACTION_DOWN
		m_bTouchDown = true;
		Zenith_Input::MouseButtonPressedCallback(ZENITH_MOUSE_BUTTON_LEFT);
		break;
	case 1: // AMOTION_EVENT_ACTION_UP
		m_bTouchDown = false;
		break;
	case 2: // AMOTION_EVENT_ACTION_MOVE
		// Position already updated
		break;
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
