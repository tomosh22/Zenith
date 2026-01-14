#include "Zenith.h"

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED

#include "Memory/Zenith_MemoryTracker.h"
#include "Callstack/Zenith_Callstack.h"
#include "Multithreading/Zenith_Multithreading.h"

#include <cstring>

std::unordered_map<void*, Zenith_AllocationRecord> Zenith_MemoryTracker::s_xAllocations;
void* Zenith_MemoryTracker::s_apFreedAddresses[uFREED_HISTORY_SIZE] = { nullptr };
u_int Zenith_MemoryTracker::s_uFreedIndex = 0;
Zenith_MemoryStats Zenith_MemoryTracker::s_xStats = {};
Zenith_Mutex_NoProfiling Zenith_MemoryTracker::s_xMutex;
u_int64 Zenith_MemoryTracker::s_ulFrameNumber = 0;
std::atomic<u_int64> Zenith_MemoryTracker::s_ulNextAllocationID(0);
bool Zenith_MemoryTracker::s_bInitialised = false;

void Zenith_MemoryTracker::Initialise()
{
	if (s_bInitialised)
	{
		return;
	}

	// Reserve space for allocations to reduce rehashing
	s_xAllocations.reserve(10000);

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

	s_xAllocations.clear();
	s_bInitialised = false;
}

void Zenith_MemoryTracker::BeginFrame()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

	// Reset frame stats
	s_xStats.m_ilFrameDelta = 0;
	s_xStats.m_uFrameAllocations = 0;
	s_xStats.m_uFrameDeallocations = 0;
}

void Zenith_MemoryTracker::EndFrame()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);
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
		Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

		xRecord.m_ulFrameNumber = s_ulFrameNumber;

		s_xAllocations[pUserAddress] = xRecord;

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

	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

	auto it = s_xAllocations.find(pUserAddress);
	if (it == s_xAllocations.end())
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

	const Zenith_AllocationRecord& xRecord = it->second;
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
	s_xAllocations.erase(it);

	return true;
}

const Zenith_MemoryStats& Zenith_MemoryTracker::GetStats()
{
	return s_xStats;
}

const Zenith_AllocationRecord* Zenith_MemoryTracker::FindAllocation(void* pAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

	auto it = s_xAllocations.find(pAddress);
	if (it != s_xAllocations.end())
	{
		return &it->second;
	}
	return nullptr;
}

bool Zenith_MemoryTracker::IsValidAllocation(void* pAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);
	return s_xAllocations.find(pAddress) != s_xAllocations.end();
}

void Zenith_MemoryTracker::ReportLeaks()
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

	if (s_xAllocations.empty())
	{
		Zenith_Log(LOG_CATEGORY_CORE, "No memory leaks detected");
		return;
	}

	Zenith_Error(LOG_CATEGORY_CORE, "=== MEMORY LEAK REPORT ===");
	Zenith_Error(LOG_CATEGORY_CORE, "%zu allocations still active, %llu bytes total",
		s_xAllocations.size(), s_xStats.m_ulTotalAllocated);

	u_int uLeakCount = 0;
	constexpr u_int uMAX_LEAKS_TO_REPORT = 100;

	for (const auto& pair : s_xAllocations)
	{
		if (uLeakCount >= uMAX_LEAKS_TO_REPORT)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "... and %zu more leaks",
				s_xAllocations.size() - uMAX_LEAKS_TO_REPORT);
			break;
		}

		const Zenith_AllocationRecord& xRecord = pair.second;

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
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);
	return static_cast<u_int>(s_xAllocations.size());
}

bool Zenith_MemoryTracker::CheckGuards(void* pUserAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

	auto it = s_xAllocations.find(pUserAddress);
	if (it == s_xAllocations.end())
	{
		return false;
	}

	const Zenith_AllocationRecord& xRecord = it->second;

	// Check front guard
	u_int32* pFrontGuard = reinterpret_cast<u_int32*>(
		static_cast<u_int8*>(xRecord.m_pRealAddress));
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
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

	u_int uCorruptionCount = 0;

	for (const auto& pair : s_xAllocations)
	{
		const Zenith_AllocationRecord& xRecord = pair.second;

		// Check front guard
		u_int32* pFrontGuard = reinterpret_cast<u_int32*>(xRecord.m_pRealAddress);
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
		Zenith_Log(LOG_CATEGORY_CORE, "Guard check passed for %zu allocations", s_xAllocations.size());
	}
}

bool Zenith_MemoryTracker::IsDoubleFree(void* pAddress)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

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
	// 2. TrackAllocation tries to acquire s_xMutex -> DEADLOCK
	//
	// Solution: Copy allocation records while holding lock, then iterate unlocked.
	// This is safe because we're copying by value and the callback only reads.

	// Use a simple array to avoid std::vector allocations during copy
	// If there are more allocations than this, we'll just report what fits
	constexpr size_t uMAX_RECORDS = 10000;
	static thread_local Zenith_AllocationRecord s_axRecordsCopy[uMAX_RECORDS];
	size_t uRecordCount = 0;

	{
		Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

		for (const auto& pair : s_xAllocations)
		{
			if (uRecordCount >= uMAX_RECORDS)
			{
				break;
			}
			s_axRecordsCopy[uRecordCount++] = pair.second;
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
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);
	return static_cast<u_int>(s_xAllocations.size());
}

#endif // ZENITH_MEMORY_MANAGEMENT_ENABLED
