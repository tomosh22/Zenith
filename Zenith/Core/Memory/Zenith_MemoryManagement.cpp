#include "Zenith.h"
#include "Zenith_MemoryManagement.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
#include "Memory/Zenith_MemoryTracker.h"
#include "Memory/Zenith_MemoryBudgets.h"
#include "Callstack/Zenith_Callstack.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include <atomic>
#endif

#ifdef ZENITH_DEBUG_VARIABLES
DEBUGVAR std::string dbg_strDebugText;
#endif

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
// Thread-local category tracking
thread_local Zenith_MemoryCategory Zenith_MemoryManagement::tl_eCurrentCategory = MEMORY_CATEGORY_GENERAL;
thread_local Zenith_MemoryCategory Zenith_MemoryManagement::tl_aeCategoryStack[uMAX_CATEGORY_STACK_DEPTH] = {};
thread_local u_int Zenith_MemoryManagement::tl_uCategoryStackDepth = 0;

// Atomic initialization flag - ensures proper visibility across threads
// Uses acquire/release semantics for synchronization
static std::atomic<bool> g_bMemoryManagementInitialised{false};
#endif

void Zenith_MemoryManagement::Initialise()
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_Callstack::Initialise();
	Zenith_MemoryTracker::Initialise();
	Zenith_MemoryBudgets::Initialise();
	// Use release semantics to ensure all initialization is visible to other threads
	g_bMemoryManagementInitialised.store(true, std::memory_order_release);
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddText({ "Memory", "Summary" }, dbg_strDebugText);
#endif
}

void Zenith_MemoryManagement::Shutdown()
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	// Disable tracking first to prevent new allocations from being tracked during shutdown
	g_bMemoryManagementInitialised.store(false, std::memory_order_release);
	ReportLeaks();
	Zenith_MemoryTracker::Shutdown();
	Zenith_Callstack::Shutdown();
#endif
}

// Disable memory management macros for this file to allow direct malloc/free
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

void* Zenith_MemoryManagement::AllocateAligned(size_t ullSize, size_t ulAlignment)
{
#ifdef ZENITH_WINDOWS
	void* pResult = _aligned_malloc(ullSize, ulAlignment);
#else
	void* pResult = aligned_alloc(ulAlignment, ullSize);
#endif
	if (pResult == nullptr && ullSize > 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Aligned memory allocation failed: %zu bytes, alignment %zu", ullSize, ulAlignment);
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

void Zenith_MemoryManagement::DeallocateAligned(void* p)
{
#ifdef ZENITH_WINDOWS
	_aligned_free(p);
#else
	free(p);
#endif
}

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED

// Thread-local recursion guard - prevents infinite recursion when CRT debug heap calls operator new
// Each thread has its own guard, so thread A's allocation won't affect thread B
// IMPORTANT: We check g_bMemoryManagementInitialised BEFORE accessing this to avoid
// TLS initialization issues during static init (TLS init can itself allocate memory)
static thread_local bool tl_bInAllocation = false;

void* Zenith_MemoryManagement::AllocateTracked(
	size_t ullSize,
	size_t ulAlignment,
	Zenith_MemoryCategory eCategory,
	const char* szFile,
	int32_t iLine)
{
	// Check initialization flag FIRST (before accessing thread_local)
	// This ensures we don't trigger TLS initialization during static init
	// Use acquire semantics to see all initialization done by Initialise()
	if (!g_bMemoryManagementInitialised.load(std::memory_order_acquire))
	{
		if (ulAlignment > 0)
		{
			return AllocateAligned(ullSize, ulAlignment);
		}
		return Allocate(ullSize);
	}

	// Now safe to access thread_local - system is initialized
	// Check if we're already in an allocation (recursion from tracker internals)
	if (tl_bInAllocation)
	{
		if (ulAlignment > 0)
		{
			return AllocateAligned(ullSize, ulAlignment);
		}
		return Allocate(ullSize);
	}

	// Set thread-local recursion guard
	tl_bInAllocation = true;

	// Calculate total size with guards
	size_t ulTotalSize = uGUARD_SIZE + ullSize + uGUARD_SIZE;

	// Allocate memory
	void* pRealAddress;
	if (ulAlignment > 0)
	{
		// Ensure alignment is at least as large as guard size
		size_t ulActualAlignment = (ulAlignment > uGUARD_SIZE) ? ulAlignment : uGUARD_SIZE;
		pRealAddress = AllocateAligned(ulTotalSize, ulActualAlignment);
	}
	else
	{
		pRealAddress = Allocate(ulTotalSize);
	}

	if (pRealAddress == nullptr)
	{
		tl_bInAllocation = false;
		return nullptr;
	}

	// Calculate user address (after front guard)
	void* pUserAddress = static_cast<u_int8*>(pRealAddress) + uGUARD_SIZE;

	// Write front guard
	u_int32* pFrontGuard = static_cast<u_int32*>(pRealAddress);
	*pFrontGuard = uMEMORY_GUARD_PATTERN;

	// Write back guard
	u_int32* pBackGuard = reinterpret_cast<u_int32*>(static_cast<u_int8*>(pUserAddress) + ullSize);
	*pBackGuard = uMEMORY_GUARD_PATTERN;

	// Fill user memory with pattern
	memset(pUserAddress, uMEMORY_FILL_NEW, ullSize);

	// Track allocation
	Zenith_MemoryTracker::TrackAllocation(pRealAddress, pUserAddress, ullSize, ulAlignment, eCategory, szFile, iLine);

	tl_bInAllocation = false;
	return pUserAddress;
}

// Thread-local recursion guard for deallocation - prevents infinite recursion when
// tracker internals or error logging allocate memory during deallocation
static thread_local bool tl_bInDeallocation = false;

void Zenith_MemoryManagement::DeallocateTracked(void* p)
{
	if (p == nullptr)
	{
		return;
	}

	// Check initialization flag FIRST (before accessing thread_local)
	// Use acquire semantics to see all initialization done by Initialise()
	if (!g_bMemoryManagementInitialised.load(std::memory_order_acquire))
	{
		// Fall back to direct deallocation if tracking not initialized
		Deallocate(p);
		return;
	}

	// Now safe to access thread_local - check recursion guard
	if (tl_bInDeallocation)
	{
		// Already in deallocation on this thread - avoid recursion
		Deallocate(p);
		return;
	}

	tl_bInDeallocation = true;

	// Find allocation record
	const Zenith_AllocationRecord* pRecord = Zenith_MemoryTracker::FindAllocation(p);
	if (pRecord == nullptr)
	{
		// This is likely memory allocated before tracking was enabled (during static init)
		// or from external libraries. Just free it directly without logging - logging would
		// cause allocations which could cascade into more issues.
		//
		// Note: We can't reliably detect double-free for untracked allocations since we
		// don't know if this address was ever allocated through our system.
		Deallocate(p);
		tl_bInDeallocation = false;
		return;
	}

	// Check guards before deallocation
	if (!Zenith_MemoryTracker::CheckGuards(p))
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Guard corruption detected during deallocation at 0x%p", p);
	}

	// Get real address and size before we remove from tracking
	void* pRealAddress = pRecord->m_pRealAddress;
	size_t ulSize = pRecord->m_ulSize;
	size_t ulAlignment = pRecord->m_ulAlignment;

	// Fill freed memory with pattern (helps detect use-after-free)
	memset(p, uMEMORY_FILL_DELETED, ulSize);

	// Remove from tracking
	Zenith_MemoryTracker::TrackDeallocation(p);

	// Actually free the memory
	if (ulAlignment > 0)
	{
		DeallocateAligned(pRealAddress);
	}
	else
	{
		Deallocate(pRealAddress);
	}

	tl_bInDeallocation = false;
}

void Zenith_MemoryManagement::PushCategory(Zenith_MemoryCategory eCategory)
{
	if (tl_uCategoryStackDepth < uMAX_CATEGORY_STACK_DEPTH)
	{
		tl_aeCategoryStack[tl_uCategoryStackDepth] = tl_eCurrentCategory;
		tl_uCategoryStackDepth++;
		tl_eCurrentCategory = eCategory;
	}
	else
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Memory category stack overflow");
	}
}

void Zenith_MemoryManagement::PopCategory()
{
	if (tl_uCategoryStackDepth > 0)
	{
		tl_uCategoryStackDepth--;
		tl_eCurrentCategory = tl_aeCategoryStack[tl_uCategoryStackDepth];
	}
	else
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Memory category stack underflow");
	}
}

Zenith_MemoryCategory Zenith_MemoryManagement::GetCurrentCategory()
{
	return tl_eCurrentCategory;
}

const Zenith_MemoryStats& Zenith_MemoryManagement::GetStats()
{
	return Zenith_MemoryTracker::GetStats();
}

void Zenith_MemoryManagement::ResetFrameStats()
{
	Zenith_MemoryTracker::BeginFrame();
}

void Zenith_MemoryManagement::ReportLeaks()
{
	Zenith_MemoryTracker::ReportLeaks();
}

void Zenith_MemoryManagement::CheckAllGuards()
{
	Zenith_MemoryTracker::CheckAllGuards();
}

void Zenith_MemoryManagement::DumpAllocationsByCategory()
{
	const Zenith_MemoryStats& xStats = GetStats();

	Zenith_Log(LOG_CATEGORY_CORE, "=== Memory Allocations by Category ===");
	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
	{
		if (xStats.m_aulCategoryAllocationCount[i] > 0)
		{
			Zenith_Log(LOG_CATEGORY_CORE, "  %s: %llu bytes (%llu allocations, peak %llu)",
				GetMemoryCategoryName(static_cast<Zenith_MemoryCategory>(i)),
				xStats.m_aulCategoryAllocated[i],
				xStats.m_aulCategoryAllocationCount[i],
				xStats.m_aulCategoryPeakAllocated[i]);
		}
	}
	Zenith_Log(LOG_CATEGORY_CORE, "  Total: %llu bytes (%llu allocations)",
		xStats.m_ulTotalAllocated, xStats.m_ulTotalAllocationCount);
}

void Zenith_MemoryManagement::DumpLargestAllocations(u_int uCount)
{
	std::vector<const Zenith_AllocationRecord*> axRecords;
	axRecords.reserve(Zenith_MemoryTracker::GetAllocationCount());

	Zenith_MemoryTracker::ForEachAllocation(
		[](const Zenith_AllocationRecord& xRecord, void* pUserData)
		{
			auto* pRecords = static_cast<std::vector<const Zenith_AllocationRecord*>*>(pUserData);
			pRecords->push_back(&xRecord);
		},
		&axRecords
	);

	// Sort by size descending
	std::sort(axRecords.begin(), axRecords.end(),
		[](const Zenith_AllocationRecord* a, const Zenith_AllocationRecord* b)
		{
			return a->m_ulSize > b->m_ulSize;
		}
	);

	Zenith_Log(LOG_CATEGORY_CORE, "=== Largest %u Allocations ===", uCount);
	for (u_int i = 0; i < uCount && i < axRecords.size(); ++i)
	{
		const Zenith_AllocationRecord* pRecord = axRecords[i];
		Zenith_Log(LOG_CATEGORY_CORE, "  #%u: %zu bytes at 0x%p [%s]",
			i + 1, pRecord->m_ulSize, pRecord->m_pAddress,
			GetMemoryCategoryName(pRecord->m_eCategory));

		if (pRecord->m_szFile != nullptr)
		{
			Zenith_Log(LOG_CATEGORY_CORE, "       %s:%d", pRecord->m_szFile, pRecord->m_iLine);
		}
	}
}

#endif // ZENITH_MEMORY_MANAGEMENT_ENABLED

void Zenith_MemoryManagement::EndFrame()
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryTracker::EndFrame();
	Zenith_MemoryBudgets::CheckAllBudgets();

#ifdef ZENITH_DEBUG_VARIABLES
	// Update summary text
	const Zenith_MemoryStats& xStats = GetStats();

	char acBuffer[512];
	snprintf(acBuffer, sizeof(acBuffer),
		"Allocated: %.2f MB (%llu allocs)\n"
		"Peak: %.2f MB\n"
		"Frame delta: %+.2f KB",
		static_cast<double>(xStats.m_ulTotalAllocated) / (1024.0 * 1024.0),
		xStats.m_ulTotalAllocationCount,
		static_cast<double>(xStats.m_ulPeakAllocated) / (1024.0 * 1024.0),
		static_cast<double>(xStats.m_ilFrameDelta) / 1024.0);

	dbg_strDebugText = acBuffer;
#endif
#endif
}

// ============================================================================
// Global operator new/delete implementations
// ============================================================================

void* operator new(size_t ullSize)
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new(size_t ullSize, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new(size_t ullSize, std::align_val_t eAlign)
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

void* operator new(size_t ullSize, std::align_val_t eAlign, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

void* operator new(size_t ullSize, const int32_t iLine, const char* szFile)
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, Zenith_MemoryManagement::GetCurrentCategory(), szFile, iLine);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new[](size_t ullSize)
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new[](size_t ullSize, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new[](size_t ullSize, std::align_val_t eAlign)
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

void* operator new[](size_t ullSize, std::align_val_t eAlign, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_GENERAL, nullptr, 0);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

void* operator new[](size_t ullSize, const int32_t iLine, const char* szFile)
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, Zenith_MemoryManagement::GetCurrentCategory(), szFile, iLine);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
void* operator new(size_t ullSize, Zenith_MemoryCategory eCategory, const int32_t iLine, const char* szFile)
{
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, eCategory, szFile, iLine);
}

void* operator new[](size_t ullSize, Zenith_MemoryCategory eCategory, const int32_t iLine, const char* szFile)
{
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, eCategory, szFile, iLine);
}
#endif

void operator delete(void* p) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::Deallocate(p);
#endif
}

void operator delete(void* p, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::Deallocate(p);
#endif
}

void operator delete(void* p, size_t) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::Deallocate(p);
#endif
}

void operator delete[](void* p) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::Deallocate(p);
#endif
}

void operator delete[](void* p, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::Deallocate(p);
#endif
}

void operator delete[](void* p, size_t) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::Deallocate(p);
#endif
}

// Aligned delete operators
void operator delete(void* p, std::align_val_t) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::DeallocateAligned(p);
#endif
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::DeallocateAligned(p);
#endif
}

void operator delete[](void* p, std::align_val_t) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::DeallocateAligned(p);
#endif
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_MemoryManagement::DeallocateTracked(p);
#else
	Zenith_MemoryManagement::DeallocateAligned(p);
#endif
}
