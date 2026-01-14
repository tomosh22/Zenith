#pragma once

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED

#include "Zenith_MemoryCategories.h"

#include <unordered_map>
#include <atomic>
#include <chrono>

static constexpr u_int uMEMORY_TRACKER_MAX_CALLSTACK_FRAMES = 16;
static constexpr u_int32 uMEMORY_GUARD_PATTERN = 0xDEADBEEF;
static constexpr u_int8 uMEMORY_FILL_NEW = 0xCD;
static constexpr u_int8 uMEMORY_FILL_DELETED = 0xDD;
static constexpr size_t uGUARD_SIZE = sizeof(u_int32);

struct Zenith_AllocationRecord
{
	void* m_pAddress;                                        // User-facing address (after front guard)
	void* m_pRealAddress;                                    // Actual malloc address (before guard)
	size_t m_ulSize;                                         // Requested size (not including guards)
	size_t m_ulAlignment;                                    // Alignment requested
	Zenith_MemoryCategory m_eCategory;                       // Allocation category
	u_int m_uThreadID;                                       // Thread that made allocation
	u_int64 m_ulAllocationID;                                // Unique sequential ID
	u_int64 m_ulFrameNumber;                                 // Frame when allocated
	const char* m_szFile;                                    // Source file (static string)
	int32_t m_iLine;                                         // Source line number
	void* m_apCallstack[uMEMORY_TRACKER_MAX_CALLSTACK_FRAMES]; // Raw frame addresses
	u_int m_uCallstackDepth;                                 // Number of valid frames
	std::chrono::steady_clock::time_point m_xTimestamp;      // When allocation occurred
};

struct Zenith_MemoryStats
{
	// Current state
	u_int64 m_ulTotalAllocated;
	u_int64 m_ulTotalAllocationCount;

	// Peak tracking
	u_int64 m_ulPeakAllocated;
	u_int64 m_ulPeakAllocationCount;

	// Lifetime counters
	u_int64 m_ulTotalBytesAllocatedLifetime;
	u_int64 m_ulTotalAllocationsLifetime;
	u_int64 m_ulTotalDeallocationsLifetime;

	// Per-category breakdown
	u_int64 m_aulCategoryAllocated[MEMORY_CATEGORY_COUNT];
	u_int64 m_aulCategoryAllocationCount[MEMORY_CATEGORY_COUNT];
	u_int64 m_aulCategoryPeakAllocated[MEMORY_CATEGORY_COUNT];

	// Frame delta tracking
	int64_t m_ilFrameDelta;
	u_int m_uFrameAllocations;
	u_int m_uFrameDeallocations;
};

class Zenith_MemoryTracker
{
public:
	static void Initialise();
	static void Shutdown();
	static void BeginFrame();
	static void EndFrame();

	// Allocation tracking
	static void TrackAllocation(
		void* pRealAddress,
		void* pUserAddress,
		size_t ulSize,
		size_t ulAlignment,
		Zenith_MemoryCategory eCategory,
		const char* szFile,
		int32_t iLine
	);

	static bool TrackDeallocation(void* pUserAddress);

	// Query functions
	static const Zenith_MemoryStats& GetStats();
	static const Zenith_AllocationRecord* FindAllocation(void* pAddress);
	static bool IsValidAllocation(void* pAddress);

	// Leak detection
	static void ReportLeaks();
	static u_int GetLeakCount();

	// Debug features
	static bool CheckGuards(void* pUserAddress);
	static void CheckAllGuards();
	static bool IsDoubleFree(void* pAddress);

	// Iteration for visualization
	using AllocationCallback = void(*)(const Zenith_AllocationRecord& xRecord, void* pUserData);
	static void ForEachAllocation(AllocationCallback pfnCallback, void* pUserData);
	static u_int GetAllocationCount();

private:
	static std::unordered_map<void*, Zenith_AllocationRecord> s_xAllocations;

	// Recently freed addresses for double-free detection
	static constexpr u_int uFREED_HISTORY_SIZE = 1024;
	static void* s_apFreedAddresses[uFREED_HISTORY_SIZE];
	static u_int s_uFreedIndex;

	static Zenith_MemoryStats s_xStats;
	static Zenith_Mutex_NoProfiling s_xMutex;
	static u_int64 s_ulFrameNumber;
	static std::atomic<u_int64> s_ulNextAllocationID;
	static bool s_bInitialised;
};

#endif // ZENITH_MEMORY_MANAGEMENT_ENABLED
