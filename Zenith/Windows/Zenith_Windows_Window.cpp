#include "Zenith.h"

#include "Windows/Zenith_Windows_Window.h"

#include "Input/Zenith_Input.h"
#include <atomic>

Zenith_Window* Zenith_Window::s_pxInstance = nullptr;

// GLFW memory tracking
static std::atomic<u_int64> s_ulGLFWMemoryAllocated = 0;
static std::atomic<u_int64> s_ulGLFWAllocationCount = 0;

// Disable memory management macros for GLFW allocator (uses raw malloc/free)
#include "Memory/Zenith_MemoryManagement_Disabled.h"

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
#include "Memory/Zenith_MemoryManagement_Enabled.h"

u_int64 Zenith_Window::GetGLFWMemoryAllocated()
{
	return s_ulGLFWMemoryAllocated.load();
}

u_int64 Zenith_Window::GetGLFWAllocationCount()
{
	return s_ulGLFWAllocationCount.load();
}

static void ErrorCallback(int32_t iError, const char* szDesc)
{
	__debugbreak();
}

static void KeyCallback(GLFWwindow* pxWindow, int32_t iKey, int32_t iScancode, int32_t iAction, int32_t iMods)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		Zenith_Input::KeyPressedCallback(iKey);
		break;
	}
}

static void MouseCallback(GLFWwindow* pxWindow, int32_t iKey, int32_t iAction, int32_t iMods)
{
	switch (iAction)
	{
	case GLFW_PRESS:
		Zenith_Input::MouseButtonPressedCallback(static_cast<uint32_t>(iKey));
		break;
	}
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

	int glfwinit = glfwInit();

#ifdef ZENITH_VULKAN
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

	glfwSetErrorCallback(ErrorCallback);

	m_pxNativeWindow = glfwCreateWindow(static_cast<int32_t>(uWidth), static_cast<int32_t>(uHeight), szTitle, nullptr, nullptr);

	glfwSetKeyCallback(m_pxNativeWindow, KeyCallback);
	glfwSetMouseButtonCallback(m_pxNativeWindow, MouseCallback);

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