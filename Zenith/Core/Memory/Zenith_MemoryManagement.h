#pragma once

#include <cstdint>
#include <cstdio>    // FILE (report sinks)
#include <new>       // std::align_val_t, std::nothrow_t

#if ZENITH_MEMORY_TRACKING_ANY
#include "Zenith_MemoryCategories.h"
struct Zenith_MemoryFrameSample;
#endif
#if ZENITH_MEMORY_TRACKING_FULL
struct Zenith_MemoryStats;
#endif

// =============================================================================
// Global operator new / delete
// -----------------------------------------------------------------------------
// Overloaded so ALL heap traffic routes through Zenith_MemoryManagement. The tier
// (ZENITH_MEMORY_TRACKING_LEVEL) decides what the routing does:
//   FULL -> AllocateTracked/DeallocateTracked  (guards, hashmap, callstacks)
//   LITE -> AllocateLite/DeallocateLite         (header cookie + atomic counters)
//   OFF  -> Allocate/Deallocate                 (bare malloc/free)
// There is NO `#define new` hammer; attribution comes from the thread-local
// category scope stack (ZENITH_MEMORY_SCOPE), resolved inside the allocator behind
// the init-flag check. See Zenith.h for the tier macros.
// =============================================================================

void* operator new(size_t ullSize);
void* operator new(size_t ullSize, const std::nothrow_t&) noexcept;
void* operator new(size_t ullSize, std::align_val_t);
void* operator new(size_t ullSize, std::align_val_t, const std::nothrow_t&) noexcept;

void* operator new[](size_t ullSize);
void* operator new[](size_t ullSize, const std::nothrow_t&) noexcept;
void* operator new[](size_t ullSize, std::align_val_t);
void* operator new[](size_t ullSize, std::align_val_t, const std::nothrow_t&) noexcept;

// The full C++20 delete surface (12 overloads). Every one routes to the same
// base-recovery deallocator. NOTE: the size_t passed to a sized delete is the
// OBJECT size, never the guard/cookie-inflated real size — recovery uses the FULL
// record (m_pRealAddress) or the LITE header offset, never this value.
void operator delete(void* p) noexcept;
void operator delete(void* p, const std::nothrow_t&) noexcept;
void operator delete(void* p, size_t) noexcept;
void operator delete(void* p, std::align_val_t) noexcept;
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept;
void operator delete(void* p, size_t, std::align_val_t) noexcept;

void operator delete[](void* p) noexcept;
void operator delete[](void* p, const std::nothrow_t&) noexcept;
void operator delete[](void* p, size_t) noexcept;
void operator delete[](void* p, std::align_val_t) noexcept;
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept;
void operator delete[](void* p, size_t, std::align_val_t) noexcept;

class Zenith_MemoryManagement
{
public:
	static void Initialise();
	// Read-only leak-report checkpoint. Call ONCE, as the very last step of engine
	// shutdown (after every tracked object is freed). It does NOT disable tracking
	// and does NOT tear down the tracker — base recovery must stay available for any
	// frees still happening during static destruction. See Zenith_MemoryManagement.cpp.
	static void Shutdown();
	static void BeginFrame();   // reset per-frame delta/alloc counters (frame start)
	static void EndFrame();     // budget check + accounting poll (frame end)

	// ----------------------------------------------------------------------------------
	// CRITICAL ALLOCATION CONSISTENCY RULE
	//
	// Allocate()/Reallocate()/Deallocate() use malloc/realloc/free under the hood and
	// return the BARE malloc pointer (no guard, no cookie). new/new[] go through the
	// overloaded operators above, which (at FULL/LITE) return a pointer whose real
	// base differs from the returned pointer (guard/cookie in front). Freeing a `new`
	// pointer via Deallocate() — or an Allocate() pointer via delete — is therefore
	// guaranteed heap corruption. Never mix the two strategies on the same pointer.
	//
	//   WRONG:  T* p = new T[count];                 // heap corruption
	//           p = (T*)Reallocate(p, newSize);
	//
	//   RIGHT:  T* p = (T*)Allocate(count * sizeof(T));
	//           p = (T*)Reallocate(p, newSize);
	//           Deallocate(p);
	//
	// See Core/CLAUDE.md "Allocation Consistency" for which engine systems (e.g.
	// Flux_MeshGeometry terrain streaming) rely on this convention.
	// ----------------------------------------------------------------------------------

	// Core allocation functions (always available — the canonical malloc path).
	static void* Allocate(size_t ullSize);
	static void* AllocateAligned(size_t ullSize, size_t ulAlignment);
	static void* Reallocate(void* p, size_t ullSize);
	static void Deallocate(void* p);
	static void DeallocateAligned(void* p);

#if ZENITH_MEMORY_TRACKING_ANY
	// Category scope stack — the attribution mechanism, shared by LITE and FULL.
	// Pushed/popped at subsystem boundaries via ZENITH_MEMORY_SCOPE.
	static void PushCategory(Zenith_MemoryCategory eCategory);
	static void PopCategory();
	static Zenith_MemoryCategory GetCurrentCategory();

	// Once-per-frame POD snapshot for the profiler / HUD / report. Pure counter read;
	// takes a consistent copy under the lock at FULL and composes from the atomics at
	// LITE. Never allocates; safe to call on the main thread at the frame boundary.
	static Zenith_MemoryFrameSample SampleFrame();

	// Memory reports for --memory-dump and the profiling report's Memory section.
	// WriteReport is human-readable text; WriteReportCSV is the machine-readable feed
	// for the CI budget gate — schema: kind,name,bytes,count,peak_bytes,budget_bytes.
	static void WriteReport(FILE* pxFile);
	static void WriteReportCSV(FILE* pxFile);
#endif

#if ZENITH_MEMORY_TRACKING_FULL
	// Race-free copy of the live stats (taken under the tracker mutex). Consumers
	// (editor panel, reports) must read this snapshot, never a live reference.
	static Zenith_MemoryStats GetStatsCopy();
	static void DumpAllocationsByCategory();
	// Tracked allocation (guard bytes, hashmap record, callstack). eCategory may be
	// the MEMORY_CATEGORY_FROM_SCOPE sentinel — resolved to GetCurrentCategory()
	// INSIDE the function, after the init-flag + recursion-guard checks.
	static void* AllocateTracked(
		size_t ullSize,
		size_t ulAlignment,
		Zenith_MemoryCategory eCategory,
		const char* szFile,
		int32_t iLine
	);
	static void DeallocateTracked(void* p);

	// Debug features
	static void ReportLeaks();
	static void CheckAllGuards();
	static void DumpLargestAllocations(u_int uCount);
#endif

#if ZENITH_MEMORY_TRACKING_ANY && !ZENITH_MEMORY_TRACKING_FULL
	// LITE allocator: header-before-user cookie + lock-free atomic counters. eCategory
	// may be the MEMORY_CATEGORY_FROM_SCOPE sentinel — resolved AFTER the init-flag
	// check (pre-init falls back to GENERAL without touching TLS).
	static void* AllocateLite(size_t ullSize, size_t ulAlignment, Zenith_MemoryCategory eCategory);
	static void DeallocateLite(void* p);
#endif

#if ZENITH_MEMORY_TRACKING_ANY
private:
	// Thread-local category stack for contextual allocations (LITE + FULL).
	static constexpr u_int uMAX_CATEGORY_STACK_DEPTH = 16;
	static thread_local Zenith_MemoryCategory tl_eCurrentCategory;
	static thread_local Zenith_MemoryCategory tl_aeCategoryStack[uMAX_CATEGORY_STACK_DEPTH];
	static thread_local u_int tl_uCategoryStackDepth;
#endif
};

#if ZENITH_MEMORY_TRACKING_ANY
// Scoped category helper — the attribution primitive. Compiles in at LITE and FULL;
// a no-op at tier OFF.
class Zenith_ScopedMemoryCategory
{
public:
	explicit Zenith_ScopedMemoryCategory(Zenith_MemoryCategory eCategory)
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

#define ZENITH_MEMORY_SCOPE_PASTE2(a, b) a##b
#define ZENITH_MEMORY_SCOPE_PASTE(a, b)  ZENITH_MEMORY_SCOPE_PASTE2(a, b)
#define ZENITH_MEMORY_SCOPE(category) \
	Zenith_ScopedMemoryCategory ZENITH_MEMORY_SCOPE_PASTE(_zenithMemScope_, __LINE__)(category)
#else
#define ZENITH_MEMORY_SCOPE(category)
#endif
