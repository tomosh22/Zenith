#include "Zenith.h"
#include "Zenith_MemoryManagement.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

#ifdef ZENITH_DEBUG_VARIABLES
DEBUGVAR std::string dbg_strDebugText;
#endif

void Zenith_MemoryManagement::Initialise()
{
	#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddText({ "Memory", "Summary" }, dbg_strDebugText);
	#endif
}

#include "Memory/Zenith_MemoryManagement_Disabled.h"

void* Zenith_MemoryManagement::Allocate(size_t ullSize)
{
	void* pResult = malloc(ullSize);
	if (pResult == nullptr && ullSize > 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Memory allocation failed: %zu bytes", ullSize);
	}
	return pResult;
}

void* Zenith_MemoryManagement::Reallocate(void* p, size_t ullSize)
{
	void* pResult = realloc(p, ullSize);
	if (pResult == nullptr && ullSize > 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Memory reallocation failed: %zu bytes", ullSize);
		// Note: original 'p' is still valid on realloc failure
	}
	return pResult;
}

void Zenith_MemoryManagement::Deallocate(void* p)
{
	free(p);
}

void Zenith_MemoryManagement::EndFrame()
{
}

void* operator new(size_t ullSize)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void* operator new(size_t ullSize, const std::nothrow_t&)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void* operator new(size_t ullSize, std::align_val_t eAlign)
{
#ifdef ZENITH_WINDOWS
	void* pResult = _aligned_malloc(ullSize, static_cast<size_t>(eAlign));
#else
	void* pResult = aligned_alloc(static_cast<size_t>(eAlign), ullSize);
#endif
	if (pResult == nullptr && ullSize > 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Aligned memory allocation failed: %zu bytes, alignment %zu", ullSize, static_cast<size_t>(eAlign));
	}
	return pResult;
}

void* operator new(size_t ullSize, std::align_val_t eAlign, const std::nothrow_t&)
{
#ifdef ZENITH_WINDOWS
	return _aligned_malloc(ullSize, static_cast<size_t>(eAlign));
#else
	return aligned_alloc(static_cast<size_t>(eAlign), ullSize);
#endif
}

void* operator new(size_t ullSize, const int32_t iLine, const char* szFile)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void* operator new[](size_t ullSize)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void* operator new[](size_t ullSize, const std::nothrow_t&)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void* operator new[](size_t ullSize, std::align_val_t eAlign)
{
#ifdef ZENITH_WINDOWS
	void* pResult = _aligned_malloc(ullSize, static_cast<size_t>(eAlign));
#else
	void* pResult = aligned_alloc(static_cast<size_t>(eAlign), ullSize);
#endif
	if (pResult == nullptr && ullSize > 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Aligned array allocation failed: %zu bytes, alignment %zu", ullSize, static_cast<size_t>(eAlign));
	}
	return pResult;
}

void* operator new[](size_t ullSize, std::align_val_t eAlign, const std::nothrow_t&)
{
#ifdef ZENITH_WINDOWS
	return _aligned_malloc(ullSize, static_cast<size_t>(eAlign));
#else
	return aligned_alloc(static_cast<size_t>(eAlign), ullSize);
#endif
}

void* operator new[](size_t ullSize, const int32_t iLine, const char* szFile)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void operator delete(void* p)
{
	Zenith_MemoryManagement::Deallocate(p);
}

void operator delete(void* p, const std::nothrow_t&)
{
	Zenith_MemoryManagement::Deallocate(p);
}

void operator delete[](void* p)
{
	Zenith_MemoryManagement::Deallocate(p);
}

void operator delete[](void* p, const std::nothrow_t&)
{
	Zenith_MemoryManagement::Deallocate(p);
}

// Aligned delete operators - must use _aligned_free on Windows
void operator delete(void* p, std::align_val_t) noexcept
{
#ifdef ZENITH_WINDOWS
	_aligned_free(p);
#else
	free(p);
#endif
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_WINDOWS
	_aligned_free(p);
#else
	free(p);
#endif
}

void operator delete[](void* p, std::align_val_t) noexcept
{
#ifdef ZENITH_WINDOWS
	_aligned_free(p);
#else
	free(p);
#endif
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_WINDOWS
	_aligned_free(p);
#else
	free(p);
#endif
}
