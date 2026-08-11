#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Windows/Zenith_Windows_Window.h"

#include "Input/Zenith_Input.h"
#include <atomic>

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

// Single engine-singleton funnel for this TU. The window layer is a pure
// forwarder into the input device layer, so one accessor keeps g_xEngine's reach
// into the platform layer at exactly one line no matter how many callbacks it
// grows.
static Zenith_Input& PlatformInput()
{
	return g_xEngine.Input();
}

// Platform callbacks ONLY enqueue (frame contract step 1); the drain that turns
// these into held state + edges runs later, after the swapchain acquire.
// GLFW_REPEAT is deliberately unhandled: auto-repeat is not a transition.
static void KeyCallback(GLFWwindow*, int32_t iKey, int32_t, int32_t iAction, int32_t)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		PlatformInput().KeyPressedCallback(iKey);
		break;
	case GLFW_RELEASE:
		PlatformInput().KeyReleasedCallback(iKey);
		break;
	}
}

static void MouseCallback(GLFWwindow*, int32_t iKey, int32_t iAction, int32_t)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		PlatformInput().MouseButtonPressedCallback(iKey);
		break;
	case GLFW_RELEASE:
		PlatformInput().MouseButtonReleasedCallback(iKey);
		break;
	}
}

// Focus loss is a LIFECYCLE_RESET barrier: it cancels every held key/button/pad
// and DISARMS input, so a key held while the user alt-tabs away cannot keep
// driving the game (and cannot come back "still held" on return). Focus gain
// re-arms. GLFW also synthesizes releases on focus loss; the barrier makes the
// outcome the same either way.
static void WindowFocusCallback(GLFWwindow*, int32_t iFocused)
{
	if (iFocused == GLFW_TRUE)
	{
		PlatformInput().LifecycleArmCallback();
	}
	else
	{
		PlatformInput().LifecycleResetCallback();
	}
}

// EXT-4: forward GLFW scroll events into Zenith_Input. Y offset is the
// vertical wheel ticks; positive = scroll up. X offset is horizontal scroll
// (touchpad / tilted wheel) — we ignore it for now but pass it through so a
// future API extension can read it without re-wiring GLFW.
static void ScrollCallback(GLFWwindow*, double fXOffset, double fYOffset)
{
	PlatformInput().MouseWheelCallback(fXOffset, fYOffset);
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

#if defined(ZENITH_VULKAN) || defined(ZENITH_NULL_RENDERER)
	// Neither backend wants GLFW to create a GL/GLES context: Vulkan owns the
	// surface itself, and the Null backend creates no graphics device at all.
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

#ifdef ZENITH_NULL_RENDERER
	// Headless is BUILD-TIME: a Null_* config IS the headless build, so the
	// window is created HIDDEN. We can't skip glfwCreateWindow entirely --
	// ImGui platform init (tools configs), input pumping, and window-size
	// queries all still run unchanged -- but GLFW_VISIBLE=false keeps it
	// off-screen so CI / automated test runs never pop a black flash.
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#endif

	glfwSetErrorCallback(ErrorCallback);

	m_pxNativeWindow = glfwCreateWindow(static_cast<int32_t>(uWidth), static_cast<int32_t>(uHeight), szTitle, nullptr, nullptr);

#ifdef ZENITH_NULL_RENDERER
	// Belt-and-braces: re-hide in case the platform layer briefly flashed the
	// window (some Windows window-managers ignore GLFW_VISIBLE before the first
	// message pump).
	glfwHideWindow(m_pxNativeWindow);
#endif

	glfwSetKeyCallback(m_pxNativeWindow, KeyCallback);
	glfwSetMouseButtonCallback(m_pxNativeWindow, MouseCallback);
	glfwSetScrollCallback(m_pxNativeWindow, ScrollCallback);
	glfwSetWindowFocusCallback(m_pxNativeWindow, WindowFocusCallback);

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

void Zenith_Window::SetCursorCaptured(bool bCaptured)
{
	// Single funnel for all cursor-mode changes. Only act when the mode ACTUALLY
	// changes: a redundant call (e.g. EnableCaptureCursor while already captured)
	// must not teleport the cursor, must not suppress that frame's delta, and must
	// not re-toggle raw motion.
	const int iTarget = bCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
	if (glfwGetInputMode(m_pxNativeWindow, GLFW_CURSOR) == iTarget)
	{
		return;
	}

	glfwSetInputMode(m_pxNativeWindow, GLFW_CURSOR, iTarget);

	// Raw (unaccelerated, unscaled) motion gives correct, linear look sensitivity
	// while captured; turn it off again on release so the desktop pointer behaves
	// normally. Only meaningful with GLFW_CURSOR_DISABLED, and only where supported.
	if (glfwRawMouseMotionSupported())
	{
		glfwSetInputMode(m_pxNativeWindow, GLFW_RAW_MOUSE_MOTION, bCaptured ? GLFW_TRUE : GLFW_FALSE);
	}

	// The mode switch re-centres / teleports the OS cursor; tell Input to drop the
	// resulting one-frame delta spike and resync its baseline next BeginFrame.
	PlatformInput().NotifyMouseDiscontinuity();
}

void Zenith_Window::ToggleCaptureCursor()
{
	SetCursorCaptured(glfwGetInputMode(m_pxNativeWindow, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);
}

void Zenith_Window::EnableCaptureCursor()
{
	SetCursorCaptured(true);
}

void Zenith_Window::DisableCaptureCursor()
{
	SetCursorCaptured(false);
}

bool Zenith_Window::IsCursorCaptured()
{
	return glfwGetInputMode(m_pxNativeWindow, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
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