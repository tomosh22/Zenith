#include "Zenith.h"

#if ZENITH_MEMORY_TRACKING_FULL

#include "Memory/Zenith_MemoryTracker.h"
#include "Callstack/Zenith_Callstack.h"
#include "Core/Multithreading/Zenith_Multithreading.h"

#include <cstring>

void* Zenith_MemoryTracker::s_apFreedAddresses[uFREED_HISTORY_SIZE] = { nullptr };
u_int Zenith_MemoryTracker::s_uFreedIndex = 0;
Zenith_MemoryStats Zenith_MemoryTracker::s_xStats = {};
u_int64 Zenith_MemoryTracker::s_ulFrameNumber = 0;
std::atomic<u_int64> Zenith_MemoryTracker::s_ulNextAllocationID(0);
bool Zenith_MemoryTracker::s_bInitialised = false;

// Immortal mutex + map (see the header comment). Constructed once via placement-new
// into static storage on first use; only a trivially-destructible pointer has static
// storage duration, so the mutex/map objects themselves are NEVER destroyed and remain
// valid through the entire process teardown (post-Shutdown static-destruction frees
// still lock the mutex and recover their base from the map). ::new here is the standard
// non-allocating placement new (NOT our overloaded operator new) — no re-entrancy.
Zenith_Mutex_NoProfiling& Zenith_MemoryTracker::Mutex()
{
	alignas(Zenith_Mutex_NoProfiling) static unsigned char s_axStorage[sizeof(Zenith_Mutex_NoProfiling)];
	static Zenith_Mutex_NoProfiling* s_pxMutex = ::new (static_cast<void*>(s_axStorage)) Zenith_Mutex_NoProfiling();
	return *s_pxMutex;
}

Zenith_HashMap<void*, Zenith_AllocationRecord>& Zenith_MemoryTracker::Allocations()
{
	alignas(Zenith_HashMap<void*, Zenith_AllocationRecord>) static unsigned char s_axStorage[sizeof(Zenith_HashMap<void*, Zenith_AllocationRecord>)];
	static Zenith_HashMap<void*, Zenith_AllocationRecord>* s_pxMap = ::new (static_cast<void*>(s_axStorage)) Zenith_HashMap<void*, Zenith_AllocationRecord>();
	return *s_pxMap;
}

void Zenith_MemoryTracker::Initialise()
{
	if (s_bInitialised)
	{
		return;
	}

	// Reserve space for allocations to reduce rehashing
	Allocations().Reserve(10000);

	// Initialize stats
	memset(&s_xStats, 0, sizeof(s_xStats));

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_CORE, "Memory tracker initialised");
}

void Zenith_MemoryTracker::Shutdown()
{
	if (!s_bInitialised)
	{
		return;
	}

	// Report any leaks before shutdown
	ReportLeaks();

	Allocations().Clear();
	s_bInitialised = false;
}

void Zenith_MemoryTracker::BeginFrame()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	// Reset frame stats
	s_xStats.m_ilFrameDelta = 0;
	s_xStats.m_uFrameAllocations = 0;
	s_xStats.m_uFrameDeallocations = 0;
}

void Zenith_MemoryTracker::EndFrame()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());
	++s_ulFrameNumber;
}

void Zenith_MemoryTracker::TrackAllocation(
	void* pRealAddress,
	void* pUserAddress,
	size_t ulSize,
	size_t ulAlignment,
	Zenith_MemoryCategory eCategory,
	const char* szFile,
	int32_t iLine)
{
	if (!s_bInitialised || pUserAddress == nullptr)
	{
		return;
	}

	Zenith_AllocationRecord xRecord;
	xRecord.m_pAddress = pUserAddress;
	xRecord.m_pRealAddress = pRealAddress;
	xRecord.m_ulSize = ulSize;
	xRecord.m_ulAlignment = ulAlignment;
	xRecord.m_eCategory = eCategory;
	xRecord.m_szFile = szFile;
	xRecord.m_iLine = iLine;
	xRecord.m_ulAllocationID = s_ulNextAllocationID.fetch_add(1);
	xRecord.m_xTimestamp = std::chrono::steady_clock::now();

	// Get thread ID (may not be registered yet during early init)
	xRecord.m_uThreadID = 0;

	// Capture callstack
	xRecord.m_uCallstackDepth = Zenith_Callstack::Capture(
		xRecord.m_apCallstack,
		uMEMORY_TRACKER_MAX_CALLSTACK_FRAMES,
		3  // Skip Capture, TrackAllocation, and AllocateTracked
	);

	{
		Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

		xRecord.m_ulFrameNumber = s_ulFrameNumber;

		Allocations()[pUserAddress] = xRecord;

		// Update stats
		s_xStats.m_ulTotalAllocated += ulSize;
		s_xStats.m_ulTotalAllocationCount++;
		s_xStats.m_ulTotalBytesAllocatedLifetime += ulSize;
		s_xStats.m_ulTotalAllocationsLifetime++;
		s_xStats.m_ilFrameDelta += static_cast<int64_t>(ulSize);
		s_xStats.m_uFrameAllocations++;

		// Update peak
		if (s_xStats.m_ulTotalAllocated > s_xStats.m_ulPeakAllocated)
		{
			s_xStats.m_ulPeakAllocated = s_xStats.m_ulTotalAllocated;
		}
		if (s_xStats.m_ulTotalAllocationCount > s_xStats.m_ulPeakAllocationCount)
		{
			s_xStats.m_ulPeakAllocationCount = s_xStats.m_ulTotalAllocationCount;
		}

		// Update category stats
		if (eCategory < MEMORY_CATEGORY_COUNT)
		{
			s_xStats.m_aulCategoryAllocated[eCategory] += ulSize;
			s_xStats.m_aulCategoryAllocationCount[eCategory]++;

			if (s_xStats.m_aulCategoryAllocated[eCategory] > s_xStats.m_aulCategoryPeakAllocated[eCategory])
			{
				s_xStats.m_aulCategoryPeakAllocated[eCategory] = s_xStats.m_aulCategoryAllocated[eCategory];
			}
		}
	}
}

bool Zenith_MemoryTracker::TrackDeallocation(void* pUserAddress)
{
	if (!s_bInitialised || pUserAddress == nullptr)
	{
		return false;
	}

	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	const Zenith_AllocationRecord* pxRecord = Allocations().TryGet(pUserAddress);
	if (pxRecord == nullptr)
	{
		// Check if this was recently freed (double-free detection)
		for (u_int i = 0; i < uFREED_HISTORY_SIZE; ++i)
		{
			if (s_apFreedAddresses[i] == pUserAddress)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "Double-free detected at address 0x%p", pUserAddress);
				return false;
			}
		}

		Zenith_Error(LOG_CATEGORY_CORE, "Deallocation of unknown address 0x%p", pUserAddress);
		return false;
	}

	const Zenith_AllocationRecord& xRecord = *pxRecord;
	size_t ulSize = xRecord.m_ulSize;
	Zenith_MemoryCategory eCategory = xRecord.m_eCategory;

	// Update stats
	s_xStats.m_ulTotalAllocated -= ulSize;
	s_xStats.m_ulTotalAllocationCount--;
	s_xStats.m_ulTotalDeallocationsLifetime++;
	s_xStats.m_ilFrameDelta -= static_cast<int64_t>(ulSize);
	s_xStats.m_uFrameDeallocations++;

	// Update category stats
	if (eCategory < MEMORY_CATEGORY_COUNT)
	{
		s_xStats.m_aulCategoryAllocated[eCategory] -= ulSize;
		s_xStats.m_aulCategoryAllocationCount[eCategory]--;
	}

	// Add to freed history for double-free detection
	s_apFreedAddresses[s_uFreedIndex] = pUserAddress;
	s_uFreedIndex = (s_uFreedIndex + 1) % uFREED_HISTORY_SIZE;

	// Remove from map
	Allocations().Remove(pUserAddress);

	return true;
}

const Zenith_MemoryStats& Zenith_MemoryTracker::GetStats()
{
	return s_xStats;
}

Zenith_MemoryStats Zenith_MemoryTracker::CopyStats()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());
	return s_xStats; // copy under the lock — no torn reads while workers allocate
}

const Zenith_AllocationRecord* Zenith_MemoryTracker::FindAllocation(void* pAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	return Allocations().TryGet(pAddress);
}

bool Zenith_MemoryTracker::CopyAllocation(void* pAddress, Zenith_AllocationRecord& xOut)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	const Zenith_AllocationRecord* pRecord = Allocations().TryGet(pAddress);
	if (pRecord == nullptr)
	{
		return false;
	}
	xOut = *pRecord; // copy the whole record BY VALUE while the lock is held — the
	                 // returned pointer would dangle on a concurrent insert's rehash.
	return true;
}

bool Zenith_MemoryTracker::IsValidAllocation(void* pAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());
	return Allocations().Contains(pAddress);
}

void Zenith_MemoryTracker::ReportLeaks()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	if (Allocations().GetSize() == 0)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "No memory leaks detected");
		return;
	}

	Zenith_Error(LOG_CATEGORY_CORE, "=== MEMORY LEAK REPORT ===");
	Zenith_Error(LOG_CATEGORY_CORE, "%u allocations still active, %llu bytes total",
		Allocations().GetSize(), s_xStats.m_ulTotalAllocated);

	u_int uLeakCount = 0;
	constexpr u_int uMAX_LEAKS_TO_REPORT = 100;

	for (Zenith_HashMap<void*, Zenith_AllocationRecord>::Iterator xIt(Allocations()); !xIt.Done(); xIt.Next())
	{
		if (uLeakCount >= uMAX_LEAKS_TO_REPORT)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "... and %u more leaks",
				Allocations().GetSize() - uMAX_LEAKS_TO_REPORT);
			break;
		}

		const Zenith_AllocationRecord& xRecord = xIt.GetValue();

		Zenith_Error(LOG_CATEGORY_CORE, "Leak #%u: %zu bytes at 0x%p [%s]",
			uLeakCount + 1,
			xRecord.m_ulSize,
			xRecord.m_pAddress,
			GetMemoryCategoryName(xRecord.m_eCategory));

		if (xRecord.m_szFile != nullptr)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "  Allocated at: %s:%d", xRecord.m_szFile, xRecord.m_iLine);
		}

		// Format and log callstack
		if (xRecord.m_uCallstackDepth > 0)
		{
			char acCallstack[4096];
			Zenith_Callstack::FormatCallstack(
				const_cast<void**>(xRecord.m_apCallstack),
				xRecord.m_uCallstackDepth,
				acCallstack,
				sizeof(acCallstack)
			);
			Zenith_Error(LOG_CATEGORY_CORE, "  Callstack:\n%s", acCallstack);
		}

		++uLeakCount;
	}

	Zenith_Error(LOG_CATEGORY_CORE, "=== END LEAK REPORT ===");
}

u_int Zenith_MemoryTracker::GetLeakCount()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());
	return Allocations().GetSize();
}

bool Zenith_MemoryTracker::CheckGuards(void* pUserAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	const Zenith_AllocationRecord* pxRecord = Allocations().TryGet(pUserAddress);
	if (pxRecord == nullptr)
	{
		return false;
	}

	const Zenith_AllocationRecord& xRecord = *pxRecord;

	// Check front guard — it lives in the 4 bytes IMMEDIATELY BEFORE the user pointer
	// (not at the real base), because the front region is padded up to the requested
	// alignment. See AllocateTracked's layout in Zenith_MemoryManagement.cpp.
	u_int32* pFrontGuard = reinterpret_cast<u_int32*>(
		static_cast<u_int8*>(xRecord.m_pAddress) - uGUARD_SIZE);
	if (*pFrontGuard != uMEMORY_GUARD_PATTERN)
	{
		Zenith_Error(LOG_CATEGORY_CORE,
			"Front guard corruption at 0x%p (expected 0x%08X, got 0x%08X)",
			pUserAddress, uMEMORY_GUARD_PATTERN, *pFrontGuard);
		return false;
	}

	// Check back guard
	u_int32* pBackGuard = reinterpret_cast<u_int32*>(
		static_cast<u_int8*>(pUserAddress) + xRecord.m_ulSize);
	if (*pBackGuard != uMEMORY_GUARD_PATTERN)
	{
		Zenith_Error(LOG_CATEGORY_CORE,
			"Back guard corruption at 0x%p (expected 0x%08X, got 0x%08X)",
			pUserAddress, uMEMORY_GUARD_PATTERN, *pBackGuard);
		return false;
	}

	return true;
}

void Zenith_MemoryTracker::CheckAllGuards()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	u_int uCorruptionCount = 0;

	for (Zenith_HashMap<void*, Zenith_AllocationRecord>::Iterator xIt(Allocations()); !xIt.Done(); xIt.Next())
	{
		const Zenith_AllocationRecord& xRecord = xIt.GetValue();

		// Check front guard — in the 4 bytes immediately before the user pointer.
		u_int32* pFrontGuard = reinterpret_cast<u_int32*>(
			static_cast<u_int8*>(xRecord.m_pAddress) - uGUARD_SIZE);
		if (*pFrontGuard != uMEMORY_GUARD_PATTERN)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"Front guard corruption at 0x%p (size %zu, category %s)",
				xRecord.m_pAddress, xRecord.m_ulSize, GetMemoryCategoryName(xRecord.m_eCategory));
			++uCorruptionCount;
		}

		// Check back guard
		u_int32* pBackGuard = reinterpret_cast<u_int32*>(
			static_cast<u_int8*>(xRecord.m_pAddress) + xRecord.m_ulSize);
		if (*pBackGuard != uMEMORY_GUARD_PATTERN)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"Back guard corruption at 0x%p (size %zu, category %s)",
				xRecord.m_pAddress, xRecord.m_ulSize, GetMemoryCategoryName(xRecord.m_eCategory));
			++uCorruptionCount;
		}
	}

	if (uCorruptionCount > 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Guard check found %u corruptions", uCorruptionCount);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Guard check passed for %u allocations", Allocations().GetSize());
	}
}

bool Zenith_MemoryTracker::IsDoubleFree(void* pAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

	for (u_int i = 0; i < uFREED_HISTORY_SIZE; ++i)
	{
		if (s_apFreedAddresses[i] == pAddress)
		{
			return true;
		}
	}
	return false;
}

void Zenith_MemoryTracker::ForEachAllocation(AllocationCallback pfnCallback, void* pUserData)
{
	// CRITICAL: We must NOT hold the mutex while calling the callback!
	// The callback may allocate memory (e.g., vector::push_back), which would:
	// 1. Call operator new -> AllocateTracked -> TrackAllocation
	// 2. TrackAllocation tries to acquire Mutex() -> DEADLOCK
	//
	// Solution: Copy allocation records while holding lock, then iterate unlocked.
	// This is safe because we're copying by value and the callback only reads.

	// Use a simple array to avoid std::vector allocations during copy
	// If there are more allocations than this, we'll just report what fits
	constexpr size_t uMAX_RECORDS = 10000;
	static thread_local Zenith_AllocationRecord s_axRecordsCopy[uMAX_RECORDS];
	size_t uRecordCount = 0;

	{
		Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());

		for (Zenith_HashMap<void*, Zenith_AllocationRecord>::Iterator xIt(Allocations()); !xIt.Done(); xIt.Next())
		{
			if (uRecordCount >= uMAX_RECORDS)
			{
				break;
			}
			s_axRecordsCopy[uRecordCount++] = xIt.GetValue();
		}
	}

	// Now safe to call callback - mutex is released
	for (size_t i = 0; i < uRecordCount; ++i)
	{
		pfnCallback(s_axRecordsCopy[i], pUserData);
	}
}

u_int Zenith_MemoryTracker::GetAllocationCount()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Mutex());
	return Allocations().GetSize();
}

#endif // ZENITH_MEMORY_TRACKING_FULL
