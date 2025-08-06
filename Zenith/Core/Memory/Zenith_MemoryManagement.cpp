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
	return malloc(ullSize);
}

void* Zenith_MemoryManagement::Reallocate(void* p, size_t ullSize)
{
	return realloc(p, ullSize);
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

void* operator new(size_t ullSize, std::align_val_t)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void* operator new(size_t ullSize, std::align_val_t, const std::nothrow_t&)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
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

void* operator new[](size_t ullSize, std::align_val_t)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
}

void* operator new[](size_t ullSize, std::align_val_t, const std::nothrow_t&)
{
	return Zenith_MemoryManagement::Allocate(ullSize);
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
