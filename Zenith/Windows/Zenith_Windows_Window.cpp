#include "Zenith.h"

#include "Windows/Zenith_Windows_Window.h"

#include "Input/Zenith_Input.h"
#include <atomic>
#include <cstdlib>  // __argc / __argv (MSVC globals) for --headless parsing
#include <cstring>  // std::strcmp

Zenith_Window* Zenith_Window::s_pxInstance = nullptr;

// GLFW memory tracking
static std::atomic<u_int64> s_ulGLFWMemoryAllocated = 0;
static std::atomic<u_int64> s_ulGLFWAllocationCount = 0;

// Disable memory management macros for GLFW allocator (uses raw malloc/free)

// Custom GLFW allocator with tracking
static void* GLFWAllocWrapper(size_t sz, void* user)
{
	(void)user;
	if (sz == 0)
		return nullptr;

	// Allocate with header for size tracking
	size_t* pBlock = static_cast<size_t*>(std::malloc(sizeof(size_t) + sz));
	if (!pBlock)
		return nullptr;

	*pBlock = sz;
	s_ulGLFWMemoryAllocated += sz;
	s_ulGLFWAllocationCount++;

	return pBlock + 1;
}

static void* GLFWReallocWrapper(void* ptr, size_t sz, void* user)
{
	(void)user;

	if (!ptr)
		return GLFWAllocWrapper(sz, user);

	if (sz == 0)
	{
		// Free the block
		size_t* pBlock = static_cast<size_t*>(ptr) - 1;
		size_t ulOldSize = *pBlock;
		s_ulGLFWMemoryAllocated -= ulOldSize;
		s_ulGLFWAllocationCount--;
		std::free(pBlock);
		return nullptr;
	}

	// Reallocate
	size_t* pOldBlock = static_cast<size_t*>(ptr) - 1;
	size_t ulOldSize = *pOldBlock;

	size_t* pNewBlock = static_cast<size_t*>(std::realloc(pOldBlock, sizeof(size_t) + sz));
	if (!pNewBlock)
		return nullptr;

	// Update tracking
	s_ulGLFWMemoryAllocated -= ulOldSize;
	s_ulGLFWMemoryAllocated += sz;

	*pNewBlock = sz;
	return pNewBlock + 1;
}

static void GLFWFreeWrapper(void* ptr, void* user)
{
	(void)user;
	if (!ptr)
		return;

	size_t* pBlock = static_cast<size_t*>(ptr) - 1;
	size_t sz = *pBlock;

	s_ulGLFWMemoryAllocated -= sz;
	s_ulGLFWAllocationCount--;

	std::free(pBlock);
}

// Re-enable memory management macros

u_int64 Zenith_Window::GetGLFWMemoryAllocated()
{
	return s_ulGLFWMemoryAllocated.load();
}

u_int64 Zenith_Window::GetGLFWAllocationCount()
{
	return s_ulGLFWAllocationCount.load();
}

static void ErrorCallback(int32_t, const char*)
{
	__debugbreak();
}

static void KeyCallback(GLFWwindow*, int32_t iKey, int32_t, int32_t iAction, int32_t)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		Zenith_Input::KeyPressedCallback(iKey);
		break;
	}
}

static void MouseCallback(GLFWwindow*, int32_t iKey, int32_t iAction, int32_t)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		Zenith_Input::MouseButtonPressedCallback(static_cast<uint32_t>(iKey));
		break;
	}
}

// EXT-4: forward GLFW scroll events into Zenith_Input. Y offset is the
// vertical wheel ticks; positive = scroll up. X offset is horizontal scroll
// (touchpad / tilted wheel) — we ignore it for now but pass it through so a
// future API extension can read it without re-wiring GLFW.
static void ScrollCallback(GLFWwindow*, double fXOffset, double fYOffset)
{
	Zenith_Input::MouseWheelCallback(fXOffset, fYOffset);
}

Zenith_Window::Zenith_Window(const char* szTitle, uint32_t uWidth, uint32_t uHeight)
{
	// Hook GLFW allocator for memory tracking BEFORE glfwInit()
	GLFWallocator xAllocator = {};
	xAllocator.allocate = GLFWAllocWrapper;
	xAllocator.reallocate = GLFWReallocWrapper;
	xAllocator.deallocate = GLFWFreeWrapper;
	xAllocator.user = nullptr;
	glfwInitAllocator(&xAllocator);

	glfwInit();

#ifdef ZENITH_VULKAN
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

	// EXT-3b: headless mode — hide the window when --headless is passed.
	// We can't skip glfwCreateWindow entirely because Vulkan's surface still
	// needs a HWND, but GLFW_VISIBLE=false keeps the window off-screen so
	// CI / Claude Code automated test runs don't pop a black flash.
	bool bHeadless = false;
	for (int i = 1; i < __argc; ++i)
	{
		if (std::strcmp(__argv[i], "--headless") == 0) { bHeadless = true; break; }
	}
	if (bHeadless)
	{
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	}

	glfwSetErrorCallback(ErrorCallback);

	m_pxNativeWindow = glfwCreateWindow(static_cast<int32_t>(uWidth), static_cast<int32_t>(uHeight), szTitle, nullptr, nullptr);

	if (bHeadless)
	{
		// Belt-and-braces: re-hide in case the platform layer briefly flashed
		// the window (some Windows window-managers ignore GLFW_VISIBLE before
		// the first message pump).
		glfwHideWindow(m_pxNativeWindow);
	}

	glfwSetKeyCallback(m_pxNativeWindow, KeyCallback);
	glfwSetMouseButtonCallback(m_pxNativeWindow, MouseCallback);
	glfwSetScrollCallback(m_pxNativeWindow, ScrollCallback);

	Zenith_Log(LOG_CATEGORY_WINDOW, "Window created");
}

Zenith_Window::~Zenith_Window()
{
}

void Zenith_Window::BeginFrame()
{
	glfwPollEvents();
}

void Zenith_Window::GetMousePosition(Zenith_Maths::Vector2_64& xOut)
{
	glfwGetCursorPos(m_pxNativeWindow, &xOut.x, &xOut.y);
}

void Zenith_Window::ToggleCaptureCursor()
{
	const int iCurrent = glfwGetInputMode(m_pxNativeWindow, GLFW_CURSOR);
	glfwSetInputMode(m_pxNativeWindow, GLFW_CURSOR,
		iCurrent == GLFW_CURSOR_DISABLED ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

void Zenith_Window::EnableCaptureCursor()
{
	glfwSetInputMode(m_pxNativeWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Zenith_Window::DisableCaptureCursor()
{
	glfwSetInputMode(m_pxNativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

bool Zenith_Window::IsKeyDown(Zenith_KeyCode iKey)
{
	// Mouse buttons use GLFW_MOUSE_BUTTON_* codes (0-7)
	// Keyboard keys use GLFW_KEY_* codes (starting from 32)
	if (iKey <= ZENITH_MOUSE_BUTTON_LAST)
	{
		return glfwGetMouseButton(m_pxNativeWindow, iKey) == GLFW_PRESS;
	}
	else
	{
		return glfwGetKey(m_pxNativeWindow, iKey) == GLFW_PRESS;
	}
}