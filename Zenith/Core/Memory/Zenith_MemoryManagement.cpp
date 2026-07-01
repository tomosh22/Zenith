// =============================================================================
// Global allocator — tiered (FULL / LITE / OFF)
// -----------------------------------------------------------------------------
// The global operator new/delete overloads route ALL heap traffic through this
// file. The tier (ZENITH_MEMORY_TRACKING_LEVEL, see Zenith.h) decides the path:
//
//   FULL (Debug):   AllocateTracked/DeallocateTracked — guard bytes, a hashmap of
//                   live allocations (Zenith_MemoryTracker), 0xCD/0xDD fill,
//                   callstack capture, leak + double-free + guard checks.
//   LITE (Release): AllocateLite/DeallocateLite — a fixed header placed immediately
//                   before the user pointer (base recovery + size + category) plus
//                   lock-free per-category atomic counters. No hashmap/guards/stacks.
//   OFF  (Final):   straight malloc/free.
//
// Attribution is the thread-local category scope stack (ZENITH_MEMORY_SCOPE); there
// is NO `#define new` hammer. The scope is resolved INSIDE the allocator, AFTER the
// init-flag check, so the pre-init path never touches thread_local storage (TLS
// first-touch can itself allocate). See the load-bearing ordering comments below.
// =============================================================================

#include "Zenith.h"
#include "Zenith_MemoryManagement.h"

#if ZENITH_MEMORY_TRACKING_ANY
#include "Memory/Zenith_MemoryFrameSample.h"
#include "Memory/Zenith_MemoryAccounting.h"
#include "Memory/Zenith_MemoryBudgets.h"
#include <atomic>
#include <cstring>
static_assert(MEMORY_CATEGORY_COUNT <= ZENITH_MEM_CAT_MAX,
	"Zenith_MemoryFrameSample per-category arrays are too small for MEMORY_CATEGORY_COUNT — raise ZENITH_MEM_CAT_MAX");
#endif

#if ZENITH_MEMORY_TRACKING_FULL
#include "Memory/Zenith_MemoryTracker.h"
#include "Callstack/Zenith_Callstack.h"
#include "Collections/Zenith_Vector.h"
#include <cstring>
#include <algorithm>
#endif

// =============================================================================
// Always-available malloc path. Returns the BARE malloc pointer (no guard/cookie);
// custom containers use this and free via Deallocate(). Never mix with new/delete
// on the same pointer (see the Allocation-Consistency rule in the header).
// =============================================================================

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
	void* pResult = nullptr;
	posix_memalign(&pResult, ulAlignment, ullSize);
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

// =============================================================================
// Category scope stack + init flag (LITE + FULL)
// =============================================================================
#if ZENITH_MEMORY_TRACKING_ANY

thread_local Zenith_MemoryCategory Zenith_MemoryManagement::tl_eCurrentCategory = MEMORY_CATEGORY_GENERAL;
thread_local Zenith_MemoryCategory Zenith_MemoryManagement::tl_aeCategoryStack[uMAX_CATEGORY_STACK_DEPTH] = {};
thread_local u_int Zenith_MemoryManagement::tl_uCategoryStackDepth = 0;

// Initialisation flag — a PLAIN atomic (no TLS). Every tracked entry point checks
// this BEFORE any thread_local access, so an allocation during static init / TLS
// setup falls through to bare malloc instead of recursing through uninitialised TLS.
// This is the single most important ordering rule in the file — do not reorder it.
static std::atomic<bool> g_bMemoryManagementInitialised{ false };

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

#endif // ZENITH_MEMORY_TRACKING_ANY

// =============================================================================
// FULL path — tracked allocation with guard bytes + hashmap records
// =============================================================================
#if ZENITH_MEMORY_TRACKING_FULL

// Thread-local recursion guards. FULL's tracker internals (hashmap rehash, callstack
// capture, error logging) themselves allocate; the guard makes those re-entrant
// allocations fall through to bare malloc so tracker bookkeeping is never itself
// tracked, and never recurses.
static thread_local bool tl_bInAllocation = false;
static thread_local bool tl_bInDeallocation = false;

void* Zenith_MemoryManagement::AllocateTracked(
	size_t ullSize,
	size_t ulAlignment,
	Zenith_MemoryCategory eCategory,
	const char* szFile,
	int32_t iLine)
{
	// (1) init flag FIRST — before any TLS access.
	if (!g_bMemoryManagementInitialised.load(std::memory_order_acquire))
	{
		// Uniform aligned alloc even for the untracked fallthrough, so the matching free
		// (DeallocateTracked's untracked/aligned path) always pairs _aligned_free with
		// _aligned_malloc on Windows — never free() on an _aligned_malloc block.
		return AllocateAligned(ullSize, (ulAlignment > 0) ? ulAlignment : alignof(std::max_align_t));
	}

	// (2) recursion guard (now safe to touch TLS).
	if (tl_bInAllocation)
	{
		// Uniform aligned alloc even for the untracked fallthrough, so the matching free
		// (DeallocateTracked's untracked/aligned path) always pairs _aligned_free with
		// _aligned_malloc on Windows — never free() on an _aligned_malloc block.
		return AllocateAligned(ullSize, (ulAlignment > 0) ? ulAlignment : alignof(std::max_align_t));
	}
	tl_bInAllocation = true;

	// (3) resolve the scope sentinel — provably after the init check.
	if (eCategory == MEMORY_CATEGORY_FROM_SCOPE)
	{
		eCategory = GetCurrentCategory();
	}

	// (4) Layout: [ pad | frontGuard(4) | user-data(ullSize) | backGuard(4) ].
	//     'user' must satisfy the requested alignment (or the default new alignment
	//     when ulAlignment == 0 — the old code returned real+4, violating it). The
	//     front region is rounded up to a multiple of the alignment AND is at least
	//     one guard wide, so the 4-byte front guard sits in the bytes immediately
	//     before 'user'. 'real' is stored in the record for alignment-agnostic free.
	//     ALL operator-new memory is allocated via AllocateAligned (ulAlign is at least
	//     the default new alignment) so every free pairs with _aligned_free uniformly.
	const size_t ulAlign = (ulAlignment > 0) ? ulAlignment : alignof(std::max_align_t);
	size_t ulFrontRoom = (uGUARD_SIZE > ulAlign) ? uGUARD_SIZE : ulAlign;
	ulFrontRoom = ((ulFrontRoom + ulAlign - 1) / ulAlign) * ulAlign;
	const size_t ulTotal = ulFrontRoom + ullSize + uGUARD_SIZE;

	void* pReal = AllocateAligned(ulTotal, ulAlign);
	if (pReal == nullptr)
	{
		tl_bInAllocation = false;
		return nullptr;
	}

	u_int8* pUser = static_cast<u_int8*>(pReal) + ulFrontRoom;
	*reinterpret_cast<u_int32*>(pUser - uGUARD_SIZE) = uMEMORY_GUARD_PATTERN; // front guard (just before user)
	*reinterpret_cast<u_int32*>(pUser + ullSize) = uMEMORY_GUARD_PATTERN;      // back guard
	memset(pUser, uMEMORY_FILL_NEW, ullSize);

	Zenith_MemoryTracker::TrackAllocation(pReal, pUser, ullSize, ulAlignment, eCategory, szFile, iLine);

	tl_bInAllocation = false;
	return pUser;
}

void Zenith_MemoryManagement::DeallocateTracked(void* p)
{
	if (p == nullptr)
	{
		return;
	}

	// (1) Init flag FIRST — before any TLS access (same ordering rule as AllocateTracked;
	//     TLS first-touch can allocate). Pre-init frees are of plain (untracked) pointers,
	//     so free directly. CRUCIALLY the flag is NEVER set back to false after Initialise()
	//     (see Shutdown()), so post-Shutdown static-destruction frees still take the tracked
	//     path below and recover their real base — base recovery does not depend on teardown.
	if (!g_bMemoryManagementInitialised.load(std::memory_order_acquire))
	{
		// Pre-init frees: FULL allocated uniformly via AllocateAligned, so free aligned.
		DeallocateAligned(p);
		return;
	}

	// (2) Recursion guard (tracker internals allocate). Now safe to touch TLS.
	if (tl_bInDeallocation)
	{
		DeallocateAligned(p);
		return;
	}
	tl_bInDeallocation = true;

	// (3) Copy the record BY VALUE under the tracker lock. Using the raw FindAllocation
	//     pointer here would be a use-after-free: it points into the hashmap's internal
	//     value array, which a CONCURRENT allocation's rehash (on another thread) can
	//     free out from under us. CopyAllocation takes the lock and copies the fields.
	Zenith_AllocationRecord xRecord;
	if (!Zenith_MemoryTracker::CopyAllocation(p, xRecord))
	{
		// Untracked (allocated before Initialise, or a foreign pointer). FULL allocates
		// uniformly via AllocateAligned, so free aligned.
		DeallocateAligned(p);
		tl_bInDeallocation = false;
		return;
	}

	if (!Zenith_MemoryTracker::CheckGuards(p))
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Guard corruption detected during deallocation at 0x%p", p);
	}

	memset(p, uMEMORY_FILL_DELETED, xRecord.m_ulSize);
	Zenith_MemoryTracker::TrackDeallocation(p);

	// FULL allocates ALL operator-new memory via AllocateAligned, so free via
	// DeallocateAligned regardless of the requested alignment (pairs _aligned_malloc
	// with _aligned_free on Windows; posix_memalign with free on POSIX).
	DeallocateAligned(xRecord.m_pRealAddress);

	tl_bInDeallocation = false;
}

Zenith_MemoryStats Zenith_MemoryManagement::GetStatsCopy()
{
	return Zenith_MemoryTracker::CopyStats();
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
	const Zenith_MemoryStats xStats = Zenith_MemoryTracker::CopyStats();

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
	Zenith_Vector<const Zenith_AllocationRecord*> axRecords;
	axRecords.Reserve(Zenith_MemoryTracker::GetAllocationCount());

	Zenith_MemoryTracker::ForEachAllocation(
		[](const Zenith_AllocationRecord& xRecord, void* pUserData)
		{
			auto* pRecords = static_cast<Zenith_Vector<const Zenith_AllocationRecord*>*>(pUserData);
			pRecords->PushBack(&xRecord);
		},
		&axRecords
	);

	std::sort(axRecords.begin(), axRecords.end(),
		[](const Zenith_AllocationRecord* a, const Zenith_AllocationRecord* b)
		{
			return a->m_ulSize > b->m_ulSize;
		}
	);

	Zenith_Log(LOG_CATEGORY_CORE, "=== Largest %u Allocations ===", uCount);
	for (u_int i = 0; i < uCount && i < axRecords.GetSize(); ++i)
	{
		const Zenith_AllocationRecord* pRecord = axRecords.Get(i);
		Zenith_Log(LOG_CATEGORY_CORE, "  #%u: %zu bytes at 0x%p [%s]",
			i + 1, pRecord->m_ulSize, pRecord->m_pAddress,
			GetMemoryCategoryName(pRecord->m_eCategory));

		if (pRecord->m_szFile != nullptr)
		{
			Zenith_Log(LOG_CATEGORY_CORE, "       %s:%d", pRecord->m_szFile, pRecord->m_iLine);
		}
	}
}

#endif // ZENITH_MEMORY_TRACKING_FULL

// =============================================================================
// LITE path — header-before-user cookie + lock-free atomic counters
// =============================================================================
#if ZENITH_MEMORY_TRACKING_ANY && !ZENITH_MEMORY_TRACKING_FULL

static constexpr u_int32 uLITE_MAGIC = 0x5A4D4C31u; // 'ZML1'

// Fixed 24-byte header placed immediately before the returned user pointer, so it is
// always found as (user - sizeof(header)) regardless of the requested alignment. The
// offset recovers the real base; the full 64-bit size drives the category decrement.
struct Zenith_LiteHeader
{
	u_int32 m_uMagic;
	u_int8  m_eCategory;
	u_int8  m_uFlags;      // bit0 = allocated via AllocateAligned (free via _aligned_free)
	u_int16 m_uReserved0;
	u_int32 m_uOffset;     // user - real
	u_int32 m_uReserved1;
	u_int64 m_ulSize;      // request size
};
static_assert(sizeof(Zenith_LiteHeader) == 24, "Zenith_LiteHeader must be 24 bytes");

// Lock-free counters. Static storage → zero-initialised before dynamic init, so the
// pre-init allocation path may bump them safely.
static std::atomic<u_int64> s_aulLiteCategoryBytes[MEMORY_CATEGORY_COUNT];
static std::atomic<u_int64> s_aulLiteCategoryCount[MEMORY_CATEGORY_COUNT];
static std::atomic<u_int64> s_ulLiteTotalBytes{ 0 };
static std::atomic<u_int64> s_ulLiteTotalCount{ 0 };
static std::atomic<u_int64> s_ulLitePeakBytes{ 0 };

void* Zenith_MemoryManagement::AllocateLite(size_t ullSize, size_t ulAlignment, Zenith_MemoryCategory eCategory)
{
	// Resolve the scope sentinel WITHOUT touching TLS until the system is initialised.
	Zenith_MemoryCategory eResolved = eCategory;
	if (eResolved == MEMORY_CATEGORY_FROM_SCOPE)
	{
		eResolved = g_bMemoryManagementInitialised.load(std::memory_order_acquire)
			? GetCurrentCategory() : MEMORY_CATEGORY_GENERAL;
	}
	if (eResolved >= MEMORY_CATEGORY_COUNT)
	{
		eResolved = MEMORY_CATEGORY_GENERAL;
	}

	const size_t ulAlign = (ulAlignment > 0) ? ulAlignment : alignof(std::max_align_t);
	size_t ulRoom = (sizeof(Zenith_LiteHeader) > ulAlign) ? sizeof(Zenith_LiteHeader) : ulAlign;
	ulRoom = ((ulRoom + ulAlign - 1) / ulAlign) * ulAlign;   // multiple of ulAlign, >= header
	const size_t ulTotal = ulRoom + ullSize;

	void* pReal = (ulAlignment > 0) ? AllocateAligned(ulTotal, ulAlign) : Allocate(ulTotal);
	if (pReal == nullptr)
	{
		return nullptr;
	}

	u_int8* pUser = static_cast<u_int8*>(pReal) + ulRoom;
	Zenith_LiteHeader* pHeader = reinterpret_cast<Zenith_LiteHeader*>(pUser - sizeof(Zenith_LiteHeader));
	pHeader->m_uMagic = uLITE_MAGIC;
	pHeader->m_eCategory = static_cast<u_int8>(eResolved);
	pHeader->m_uFlags = (ulAlignment > 0) ? 1u : 0u;
	pHeader->m_uReserved0 = 0;
	pHeader->m_uOffset = static_cast<u_int32>(ulRoom);
	pHeader->m_uReserved1 = 0;
	pHeader->m_ulSize = ullSize;

	s_aulLiteCategoryBytes[eResolved].fetch_add(ullSize, std::memory_order_relaxed);
	s_aulLiteCategoryCount[eResolved].fetch_add(1, std::memory_order_relaxed);
	s_ulLiteTotalCount.fetch_add(1, std::memory_order_relaxed);
	const u_int64 ulNewTotal = s_ulLiteTotalBytes.fetch_add(ullSize, std::memory_order_relaxed) + ullSize;
	u_int64 ulPeak = s_ulLitePeakBytes.load(std::memory_order_relaxed);
	while (ulNewTotal > ulPeak && !s_ulLitePeakBytes.compare_exchange_weak(ulPeak, ulNewTotal, std::memory_order_relaxed))
	{
		// ulPeak is refreshed by compare_exchange_weak on failure.
	}

	return pUser;
}

void Zenith_MemoryManagement::DeallocateLite(void* p)
{
	if (p == nullptr)
	{
		return;
	}

	Zenith_LiteHeader* pHeader = reinterpret_cast<Zenith_LiteHeader*>(static_cast<u_int8*>(p) - sizeof(Zenith_LiteHeader));

	// Validate before trusting the header. A foreign / consistency-rule-violating
	// pointer won't match magic + plausible offset — fall through to a plain free.
	// The offset sanity bound must exceed any real allocation's user-offset (which
	// equals its alignment for over-aligned types). 1 GiB comfortably covers every
	// conceivable alignment while still rejecting garbage; the 32-bit magic is the
	// primary foreign-pointer discriminator. (A 1 MiB bound here wrongly rejected — and
	// then heap-corrupted — genuine allocations of types with alignment > 1 MiB.)
	const bool bValid = (pHeader->m_uMagic == uLITE_MAGIC)
		&& (pHeader->m_uOffset >= sizeof(Zenith_LiteHeader))
		&& (pHeader->m_uOffset <= (1u << 30));
	if (!bValid)
	{
		Deallocate(p);
		return;
	}

	const Zenith_MemoryCategory eCat = static_cast<Zenith_MemoryCategory>(pHeader->m_eCategory);
	if (eCat < MEMORY_CATEGORY_COUNT)
	{
		s_aulLiteCategoryBytes[eCat].fetch_sub(pHeader->m_ulSize, std::memory_order_relaxed);
		s_aulLiteCategoryCount[eCat].fetch_sub(1, std::memory_order_relaxed);
	}
	s_ulLiteTotalBytes.fetch_sub(pHeader->m_ulSize, std::memory_order_relaxed);
	s_ulLiteTotalCount.fetch_sub(1, std::memory_order_relaxed);

	void* pReal = static_cast<u_int8*>(p) - pHeader->m_uOffset;
	const bool bAligned = (pHeader->m_uFlags & 1u) != 0u;
	pHeader->m_uMagic = 0u; // poison so an accidental double-free won't re-decrement
	if (bAligned)
	{
		DeallocateAligned(pReal);
	}
	else
	{
		Deallocate(pReal);
	}
}

#endif // LITE

// =============================================================================
// SampleFrame — once-per-frame POD snapshot for profiler / HUD / report
// =============================================================================
#if ZENITH_MEMORY_TRACKING_ANY

Zenith_MemoryFrameSample Zenith_MemoryManagement::SampleFrame()
{
	Zenith_MemoryFrameSample xSample;
	xSample.m_uCategoryCount = MEMORY_CATEGORY_COUNT;

#if ZENITH_MEMORY_TRACKING_FULL
	const Zenith_MemoryStats xStats = Zenith_MemoryTracker::CopyStats();
	xSample.m_ulTotalBytes = xStats.m_ulTotalAllocated;
	xSample.m_ulPeakBytes = xStats.m_ulPeakAllocated;
	xSample.m_ulTotalAllocations = xStats.m_ulTotalAllocationCount;
	xSample.m_ilFrameDeltaBytes = xStats.m_ilFrameDelta;
	xSample.m_uFrameAllocations = xStats.m_uFrameAllocations;
	xSample.m_uFrameDeallocations = xStats.m_uFrameDeallocations;
	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
	{
		xSample.m_aulCategoryBytes[i] = xStats.m_aulCategoryAllocated[i];
		xSample.m_aulCategoryPeak[i] = xStats.m_aulCategoryPeakAllocated[i];
		xSample.m_aulCategoryCount[i] = xStats.m_aulCategoryAllocationCount[i];
	}
#else // LITE
	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT; ++i)
	{
		xSample.m_aulCategoryBytes[i] = s_aulLiteCategoryBytes[i].load(std::memory_order_relaxed);
		xSample.m_aulCategoryCount[i] = s_aulLiteCategoryCount[i].load(std::memory_order_relaxed);
		// LITE keeps no per-category peak; leave m_aulCategoryPeak[i] at 0.
	}
	xSample.m_ulTotalBytes = s_ulLiteTotalBytes.load(std::memory_order_relaxed);
	xSample.m_ulPeakBytes = s_ulLitePeakBytes.load(std::memory_order_relaxed);
	xSample.m_ulTotalAllocations = s_ulLiteTotalCount.load(std::memory_order_relaxed);
	// LITE keeps no per-frame delta/alloc/free counters; leave those at 0.
#endif

	return xSample;
}

void Zenith_MemoryManagement::WriteReport(FILE* pxFile)
{
	if (pxFile == nullptr)
	{
		return;
	}
	const Zenith_MemoryFrameSample xS = SampleFrame();

	fprintf(pxFile, "=== Memory ===\n");
	fprintf(pxFile, "Tracked: %.2f MB   Peak: %.2f MB   Live allocations: %llu\n",
		static_cast<double>(xS.m_ulTotalBytes) / (1024.0 * 1024.0),
		static_cast<double>(xS.m_ulPeakBytes) / (1024.0 * 1024.0), xS.m_ulTotalAllocations);
#if ZENITH_MEMORY_TRACKING_FULL
	fprintf(pxFile, "Frame delta: %+.2f KB   allocs: %u  frees: %u\n",
		static_cast<double>(xS.m_ilFrameDeltaBytes) / 1024.0, xS.m_uFrameAllocations, xS.m_uFrameDeallocations);
#endif

	fprintf(pxFile, "%-14s %16s %12s %16s\n", "Category", "Bytes", "Count", "Peak");
	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT && i < xS.m_uCategoryCount; ++i)
	{
		if (xS.m_aulCategoryBytes[i] == 0 && xS.m_aulCategoryCount[i] == 0)
		{
			continue;
		}
		fprintf(pxFile, "%-14s %16llu %12llu %16llu\n",
			GetMemoryCategoryName(static_cast<Zenith_MemoryCategory>(i)),
			xS.m_aulCategoryBytes[i], xS.m_aulCategoryCount[i], xS.m_aulCategoryPeak[i]);
	}

	fprintf(pxFile, "--- Sources ---\n");
	for (u_int i = 0; i < Zenith_MemoryAccounting::GetSourceCount(); ++i)
	{
		const Zenith_MemorySource& xSrc = Zenith_MemoryAccounting::GetSource(i);
		fprintf(pxFile, "%-14s %16llu %12llu  %s\n",
			xSrc.m_szName, xSrc.m_ulBytes, xSrc.m_ulAllocCount, xSrc.m_bIsVRAM ? "[VRAM]" : "");
	}
	fprintf(pxFile, "Process RAM: %.2f MB   VRAM: %.2f MB\n",
		static_cast<double>(Zenith_MemoryAccounting::GetTotalProcessRAM()) / (1024.0 * 1024.0),
		static_cast<double>(Zenith_MemoryAccounting::GetTotalVRAM()) / (1024.0 * 1024.0));
}

void Zenith_MemoryManagement::WriteReportCSV(FILE* pxFile)
{
	if (pxFile == nullptr)
	{
		return;
	}
	const Zenith_MemoryFrameSample xS = SampleFrame();

	fprintf(pxFile, "kind,name,bytes,count,peak_bytes,budget_bytes\n");
	for (u_int i = 0; i < MEMORY_CATEGORY_COUNT && i < xS.m_uCategoryCount; ++i)
	{
		const u_int64 ulBudget = Zenith_MemoryBudgets::GetBudget(static_cast<Zenith_MemoryCategory>(i));
		// LITE keeps no per-category peak (leaves it 0); emit the current bytes as the peak
		// proxy so the CSV self-describes and the CI live gate has a meaningful per-category
		// value regardless of tier. At FULL, peak >= bytes so this is just the real peak.
		const u_int64 ulPeakOut = (xS.m_aulCategoryPeak[i] > xS.m_aulCategoryBytes[i])
			? xS.m_aulCategoryPeak[i] : xS.m_aulCategoryBytes[i];
		fprintf(pxFile, "category,%s,%llu,%llu,%llu,%llu\n",
			GetMemoryCategoryName(static_cast<Zenith_MemoryCategory>(i)),
			xS.m_aulCategoryBytes[i], xS.m_aulCategoryCount[i], ulPeakOut, ulBudget);
	}
	for (u_int i = 0; i < Zenith_MemoryAccounting::GetSourceCount(); ++i)
	{
		const Zenith_MemorySource& xSrc = Zenith_MemoryAccounting::GetSource(i);
		// Accounting keeps no historical peak; the current value is the dump-time peak proxy.
		fprintf(pxFile, "source,%s,%llu,%llu,%llu,%llu\n",
			xSrc.m_szName, xSrc.m_ulBytes, xSrc.m_ulAllocCount, xSrc.m_ulBytes, xSrc.m_ulBudgetBytes);
	}
	fprintf(pxFile, "total,All,%llu,%llu,%llu,0\n", xS.m_ulTotalBytes, xS.m_ulTotalAllocations, xS.m_ulPeakBytes);
}

#endif // ZENITH_MEMORY_TRACKING_ANY

// =============================================================================
// Lifecycle
// =============================================================================

void Zenith_MemoryManagement::Initialise()
{
#if ZENITH_MEMORY_TRACKING_FULL
	Zenith_Callstack::Initialise();
	Zenith_MemoryTracker::Initialise();
#endif
#if ZENITH_MEMORY_TRACKING_ANY
	Zenith_MemoryBudgets::Initialise();   // seeds default tripwires; works at LITE too
	// Publish LAST, with release, so all init above is visible before any thread
	// starts taking the tracked path (and so Initialise()'s own allocations are
	// untracked — the flag is still false while Reserve() etc. run).
	g_bMemoryManagementInitialised.store(true, std::memory_order_release);
#endif
}

void Zenith_MemoryManagement::Shutdown()
{
	// Read-only leak checkpoint. Deliberately does NOT flip the init flag and does NOT
	// tear down the tracker: base recovery (FULL map / LITE header) must stay available
	// for frees still happening during static destruction after this point (e.g. a
	// static std::string). The tracker's own bookkeeping is reclaimed by the OS at exit.
	// Call this as the LAST step of Zenith_Engine::Shutdown(), after every engine-owned
	// tracked object is freed.
#if ZENITH_MEMORY_TRACKING_FULL
	ReportLeaks();
#endif
}

void Zenith_MemoryManagement::BeginFrame()
{
#if ZENITH_MEMORY_TRACKING_FULL
	Zenith_MemoryTracker::BeginFrame();   // reset per-frame delta / alloc / dealloc counters
#endif
}

void Zenith_MemoryManagement::EndFrame()
{
#if ZENITH_MEMORY_TRACKING_FULL
	Zenith_MemoryTracker::EndFrame();
#endif
#if ZENITH_MEMORY_TRACKING_ANY
	Zenith_MemoryBudgets::CheckAllBudgets();   // enforcement works at both tiers via SampleFrame()
	// Refresh the unified-accounting sources (engine CPU + VRAM + Jolt + ...) once per
	// frame. Each poll is a captureless free fn registered by the engine; PollAll just
	// invokes them, so this stays dependency-free at the leaf.
	Zenith_MemoryAccounting::PollAll();
#endif
}

// =============================================================================
// Global operator new / delete — tier dispatch
// =============================================================================

void* operator new(size_t ullSize)
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new(size_t ullSize, const std::nothrow_t&) noexcept
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new(size_t ullSize, std::align_val_t eAlign)
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

void* operator new(size_t ullSize, std::align_val_t eAlign, const std::nothrow_t&) noexcept
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

void* operator new[](size_t ullSize)
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new[](size_t ullSize, const std::nothrow_t&) noexcept
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, 0, MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::Allocate(ullSize);
#endif
}

void* operator new[](size_t ullSize, std::align_val_t eAlign)
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

void* operator new[](size_t ullSize, std::align_val_t eAlign, const std::nothrow_t&) noexcept
{
#if ZENITH_MEMORY_TRACKING_FULL
	return Zenith_MemoryManagement::AllocateTracked(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE, nullptr, 0);
#elif ZENITH_MEMORY_TRACKING_ANY
	return Zenith_MemoryManagement::AllocateLite(ullSize, static_cast<size_t>(eAlign), MEMORY_CATEGORY_FROM_SCOPE);
#else
	return Zenith_MemoryManagement::AllocateAligned(ullSize, static_cast<size_t>(eAlign));
#endif
}

// --- delete: FULL/LITE always route to the recovery dealloc (which finds the real
//     base from the record/header); OFF routes plain vs aligned per overload. ---

#if ZENITH_MEMORY_TRACKING_FULL
	#define ZENITH_MEM_FREE(p)         Zenith_MemoryManagement::DeallocateTracked(p)
	#define ZENITH_MEM_FREE_ALIGNED(p) Zenith_MemoryManagement::DeallocateTracked(p)
#elif ZENITH_MEMORY_TRACKING_ANY
	#define ZENITH_MEM_FREE(p)         Zenith_MemoryManagement::DeallocateLite(p)
	#define ZENITH_MEM_FREE_ALIGNED(p) Zenith_MemoryManagement::DeallocateLite(p)
#else
	#define ZENITH_MEM_FREE(p)         Zenith_MemoryManagement::Deallocate(p)
	#define ZENITH_MEM_FREE_ALIGNED(p) Zenith_MemoryManagement::DeallocateAligned(p)
#endif

void operator delete(void* p) noexcept                                        { ZENITH_MEM_FREE(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept                 { ZENITH_MEM_FREE(p); }
void operator delete(void* p, size_t) noexcept                                { ZENITH_MEM_FREE(p); }
void operator delete(void* p, std::align_val_t) noexcept                      { ZENITH_MEM_FREE_ALIGNED(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { ZENITH_MEM_FREE_ALIGNED(p); }
void operator delete(void* p, size_t, std::align_val_t) noexcept              { ZENITH_MEM_FREE_ALIGNED(p); }

void operator delete[](void* p) noexcept                                      { ZENITH_MEM_FREE(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept               { ZENITH_MEM_FREE(p); }
void operator delete[](void* p, size_t) noexcept                              { ZENITH_MEM_FREE(p); }
void operator delete[](void* p, std::align_val_t) noexcept                    { ZENITH_MEM_FREE_ALIGNED(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { ZENITH_MEM_FREE_ALIGNED(p); }
void operator delete[](void* p, size_t, std::align_val_t) noexcept            { ZENITH_MEM_FREE_ALIGNED(p); }

#undef ZENITH_MEM_FREE
#undef ZENITH_MEM_FREE_ALIGNED

// Unit tests live in an always-linked TU (this one — the global allocator) so MSVC
// cannot dead-strip their ZENITH_TEST static-init registration.
#ifdef ZENITH_TESTING
#include "Memory/Zenith_MemoryManagement.Tests.inl"
#endif
