#pragma once

#include <cstdint>

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
#include "Zenith_MemoryCategories.h"
#endif

// Forward declare stats struct
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
struct Zenith_MemoryStats;
#endif

// Standard operator new/delete declarations
void* operator new(size_t ullSize);
void* operator new(size_t ullSize, const std::nothrow_t&) noexcept;
void* operator new(size_t ullSize, std::align_val_t);
void* operator new(size_t ullSize, std::align_val_t, const std::nothrow_t&) noexcept;
void* operator new(size_t ullSize, const int32_t iLine, const char* szFile);

void* operator new[](size_t ullSize);
void* operator new[](size_t ullSize, const std::nothrow_t&) noexcept;
void* operator new[](size_t ullSize, std::align_val_t);
void* operator new[](size_t ullSize, std::align_val_t, const std::nothrow_t&) noexcept;
void* operator new[](size_t ullSize, const int32_t iLine, const char* szFile);

void operator delete(void* p) noexcept;
void operator delete(void* p, const std::nothrow_t&) noexcept;
void operator delete(void* p, size_t) noexcept;
void operator delete(void* p, std::align_val_t) noexcept;
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept;

void operator delete[](void* p) noexcept;
void operator delete[](void* p, const std::nothrow_t&) noexcept;
void operator delete[](void* p, size_t) noexcept;
void operator delete[](void* p, std::align_val_t) noexcept;
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept;

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
// Category-aware operator new (used when memory tracking is enabled)
void* operator new(size_t ullSize, Zenith_MemoryCategory eCategory, const int32_t iLine, const char* szFile);
void* operator new[](size_t ullSize, Zenith_MemoryCategory eCategory, const int32_t iLine, const char* szFile);
#endif

class Zenith_MemoryManagement
{
public:
	static void Initialise();
	static void Shutdown();
	static void EndFrame();

	// Core allocation functions (always available)
	static void* Allocate(size_t ullSize);
	static void* AllocateAligned(size_t ullSize, size_t ulAlignment);
	static void* Reallocate(void* p, size_t ullSize);
	static void Deallocate(void* p);
	static void DeallocateAligned(void* p);

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	// Tracked allocation functions
	static void* AllocateTracked(
		size_t ullSize,
		size_t ulAlignment,
		Zenith_MemoryCategory eCategory,
		const char* szFile,
		int32_t iLine
	);
	static void DeallocateTracked(void* p);

	// Category-based helpers
	static void PushCategory(Zenith_MemoryCategory eCategory);
	static void PopCategory();
	static Zenith_MemoryCategory GetCurrentCategory();

	// Statistics access
	static const Zenith_MemoryStats& GetStats();
	static void ResetFrameStats();

	// Debug features
	static void ReportLeaks();
	static void CheckAllGuards();
	static void DumpAllocationsByCategory();
	static void DumpLargestAllocations(u_int uCount);

private:
	// Thread-local category stack for contextual allocations
	static constexpr u_int uMAX_CATEGORY_STACK_DEPTH = 16;
	static thread_local Zenith_MemoryCategory tl_eCurrentCategory;
	static thread_local Zenith_MemoryCategory tl_aeCategoryStack[uMAX_CATEGORY_STACK_DEPTH];
	static thread_local u_int tl_uCategoryStackDepth;
#endif
};

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
// Scoped category helper
class Zenith_ScopedMemoryCategory
{
public:
	Zenith_ScopedMemoryCategory(Zenith_MemoryCategory eCategory)
	{
		Zenith_MemoryManagement::PushCategory(eCategory);
	}
	~Zenith_ScopedMemoryCategory()
	{
		Zenith_MemoryManagement::PopCategory();
	}

	Zenith_ScopedMemoryCategory(const Zenith_ScopedMemoryCategory&) = delete;
	Zenith_ScopedMemoryCategory& operator=(const Zenith_ScopedMemoryCategory&) = delete;
};

#define ZENITH_MEMORY_SCOPE(category) Zenith_ScopedMemoryCategory _zenithMemScope##__LINE__(category)
#else
#define ZENITH_MEMORY_SCOPE(category)
#endif
