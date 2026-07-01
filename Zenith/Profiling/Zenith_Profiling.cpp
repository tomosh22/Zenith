#include "Zenith.h"

#include "Profiling/Zenith_Profiling.h"

#include "Core/Zenith_EditorWindowNames.h"

#include "Core/Zenith_Engine.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include <cstring>

#if ZENITH_MEMORY_TRACKING_ANY
// Category names for the Memory tab/HUD (same layer-0 module; a trivial enum + names,
// no code dependency). The per-frame sample itself is the leaf POD in the header.
#include "Memory/Zenith_MemoryCategories.h"
#include "Memory/Zenith_MemoryAccounting.h"
#include "Memory/Zenith_MemoryBudgets.h"
#endif

#if ZENITH_PROFILING_ENABLED

static constexpr float fPROFILING_MAX_EVENT_TIME_SECONDS = 0.5f;

// Raw tick deltas -> wall-clock. GetTicksToNs() is captured once (compile-time-folded
// to 1.0 on MSVC, where the clock period is nanoseconds).
static inline double TickDeltaToNs(const u_int64 uBegin, const u_int64 uEnd)
{
	return static_cast<double>(uEnd - uBegin) * Zenith_Profiling_Detail::GetTicksToNs();
}
static inline double TickDeltaToMs(const u_int64 uBegin, const u_int64 uEnd)
{
	return TickDeltaToNs(uBegin, uEnd) / 1.0e6;
}

// The producing thread's own ring. Set at RegisterThread, cleared at
// UnregisterThread. The hot path (Begin/EndProfile) reaches the ring ONLY through
// this thread_local — no g_xEngine reach, no lock, no hashmap.
thread_local static Zenith_Profiling::ThreadBuffer* tl_pxBuffer = nullptr;

// Pause is a CONSUMER-side concept (rev. 3): producers never read it. When paused,
// EndFrame still drains the rings (so they don't back-pressure) but discards the
// drained events and leaves the display snapshot frozen.
DEBUGVAR bool dbg_bPauseRequested = false;

// Auto-pause + pin the first frame that exceeds the spike threshold (ms), so a stall
// is captured for inspection instead of scrolling past. Toggled from the editor panel.
// Debug-variables only: the feature mutates dbg_bPauseRequested, which DEBUGVAR makes
// const (read-only) in non-debug-variable builds.
#ifdef ZENITH_DEBUG_VARIABLES
DEBUGVAR bool  dbg_bPauseOnSpike      = false;
DEBUGVAR float dbg_fSpikeThresholdMs  = 33.3f;

#if ZENITH_MEMORY_TRACKING_ANY
// Toggles the always-on memory HUD overlay (drawn from the editor ImGui frame).
DEBUGVAR bool  dbg_bShowMemoryHUD     = false;
#endif
#endif

// --- Hot path (producer, lock-free) ---------------------------------------
// Both the member Begin/EndProfile and the Zenith_Profiling_Detail:: bridge route
// here. Operates purely on tl_pxBuffer.

static void DoBeginProfileZone(const Zenith_ProfileZoneID uZoneID, const char* szLabel)
{
	Zenith_Profiling::ThreadBuffer* pxBuf = tl_pxBuffer;
	if (pxBuf == nullptr) return;

	if (pxBuf->m_uDepth >= Zenith_Profiling::uMAX_PROFILE_DEPTH)
	{
		// Release-safe nesting-overflow: skip the begin, remember we owe a skipped
		// end. Never overruns m_axStack.
		++pxBuf->m_uSuppressedDepth;
		return;
	}

	Zenith_Profiling::ThreadBuffer::InFlight& xFrame = pxBuf->m_axStack[pxBuf->m_uDepth++];
	xFrame.m_uZoneID = uZoneID;
	xFrame.m_szLabel = szLabel;
	xFrame.m_uStartTicks = Zenith_Profiling_Detail::GetTimestamp();
}

static void DoEndProfileZone(const Zenith_ProfileZoneID uZoneID)
{
	Zenith_Profiling::ThreadBuffer* pxBuf = tl_pxBuffer;
	if (pxBuf == nullptr) return;

	const u_int64 uEnd = Zenith_Profiling_Detail::GetTimestamp();

	if (pxBuf->m_uSuppressedDepth > 0)
	{
		--pxBuf->m_uSuppressedDepth;   // balances a suppressed begin
		return;
	}
	if (pxBuf->m_uDepth == 0)
	{
		// Unmatched/mismatched end: counted (consumer reports it), no underflow.
		pxBuf->m_uUnmatchedEnds.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	const u_int uDepth = --pxBuf->m_uDepth;
	const Zenith_Profiling::ThreadBuffer::InFlight& xFrame = pxBuf->m_axStack[uDepth];
	// The depth model is authoritative; uZoneID is informational. A mismatched id is
	// tolerated in release (the top in-flight scope is popped regardless).
	(void)uZoneID;

	const u_int64 uWrite = pxBuf->m_uWriteLocal;
	const u_int64 uRead = pxBuf->m_uReadCursor.load(std::memory_order_acquire);  // back-pressure
	if (uWrite - uRead >= Zenith_Profiling::uRING_CAPACITY)
	{
		pxBuf->m_uDroppedEvents.fetch_add(1, std::memory_order_relaxed);          // DROP
		return;
	}

	pxBuf->m_axEvents[uWrite % Zenith_Profiling::uRING_CAPACITY] =
		Zenith_Profiling::Event(xFrame.m_uStartTicks, uEnd, xFrame.m_uZoneID, uDepth, xFrame.m_szLabel);
	pxBuf->m_uWriteLocal = uWrite + 1;
	pxBuf->m_uWriteCursor.store(uWrite + 1, std::memory_order_release);          // PUBLISH
}

// --- Consumer (main thread): drain + snapshot helpers ---------------------
// All run on the main thread under m_xRegistrationMutex (NOT the hot path), so a
// concurrent Register/UnregisterThread can never free a ring mid-drain.

static void ResetSnapshotLengths(Zenith_Profiling::Snapshot& xSnap)
{
	// Clear inner vector lengths but keep their capacity (no realloc), and keep the
	// map entries — so steady state never reallocates. Never call HashMap::Clear,
	// which would destroy the inner vectors and free their capacity.
	for (Zenith_HashMap<u_int, Zenith_Vector<Zenith_Profiling::Event>>::Iterator xIt(xSnap.m_xThreadEvents); !xIt.Done(); xIt.Next())
	{
		xIt.GetValueMutable().Clear();
	}
}

// Drains every ring's completed events. pxDest == nullptr discards (advances read
// cursors only); otherwise appends into pxDest keyed by thread id.
static void DrainAllRings(Zenith_Profiling& xSelf, Zenith_Profiling::Snapshot* pxDest)
{
	Zenith_ScopedMutexLock_T xLock(xSelf.m_xRegistrationMutex);

	for (u_int uThreadID = 0; uThreadID < Zenith_Profiling::uMAX_PROFILE_THREADS; ++uThreadID)
	{
		Zenith_Profiling::ThreadBuffer* pxBuf = xSelf.m_apxThreadBuffers[uThreadID].load(std::memory_order_acquire);
		if (pxBuf == nullptr) continue;

		const u_int64 uWrite = pxBuf->m_uWriteCursor.load(std::memory_order_acquire);
		const u_int64 uRead = pxBuf->m_uReadCursor.load(std::memory_order_relaxed);  // consumer owns it

		if (pxDest != nullptr && uWrite != uRead)
		{
			Zenith_Vector<Zenith_Profiling::Event>* pxEvents = pxDest->m_xThreadEvents.TryGet(uThreadID);
			if (pxEvents == nullptr)
			{
				pxEvents = &pxDest->m_xThreadEvents.Emplace(uThreadID);
			}
			for (u_int64 u = uRead; u < uWrite; ++u)
			{
				const Zenith_Profiling::Event& xEvent = pxBuf->m_axEvents[u % Zenith_Profiling::uRING_CAPACITY];
				pxEvents->PushBack(xEvent);

				// Long-event warning lives on the consumer (never the hot path).
				const double fDurationSeconds = TickDeltaToNs(xEvent.m_uBeginTicks, xEvent.m_uEndTicks) / 1.0e9;
				if (fDurationSeconds > fPROFILING_MAX_EVENT_TIME_SECONDS)
				{
					const char* szName = xEvent.m_szLabel ? xEvent.m_szLabel : xSelf.GetZoneName(xEvent.m_uZoneID);
					Zenith_Warning(LOG_CATEGORY_CORE, "Profiling: Event '%s' took %.3fms (threshold: %.3fms) on thread %u",
						szName, fDurationSeconds * 1000.0f, fPROFILING_MAX_EVENT_TIME_SECONDS * 1000.0f, uThreadID);
				}
			}
		}

		pxBuf->m_uReadCursor.store(uWrite, std::memory_order_release);  // free slots

		// Warn-once diagnostics (consumer-owned flags).
		if (!pxBuf->m_bDropWarned && pxBuf->m_uDroppedEvents.load(std::memory_order_relaxed) > 0)
		{
			pxBuf->m_bDropWarned = true;
			Zenith_Warning(LOG_CATEGORY_CORE, "Profiling: thread %u dropped events (ring capacity %u exceeded in a frame)", uThreadID, Zenith_Profiling::uRING_CAPACITY);
		}
		if (!pxBuf->m_bUnmatchedWarned && pxBuf->m_uUnmatchedEnds.load(std::memory_order_relaxed) > 0)
		{
			pxBuf->m_bUnmatchedWarned = true;
			Zenith_Warning(LOG_CATEGORY_CORE, "Profiling: thread %u had unmatched EndProfile call(s)", uThreadID);
		}
	}
}

// Capacity-preserving copy: clear dst lengths (keep capacity), then append src.
static void CopyInto(Zenith_Profiling::Snapshot& xDst, const Zenith_Profiling::Snapshot& xSrc)
{
	ResetSnapshotLengths(xDst);
	for (Zenith_HashMap<u_int, Zenith_Vector<Zenith_Profiling::Event>>::Iterator xIt(xSrc.m_xThreadEvents); !xIt.Done(); xIt.Next())
	{
		const u_int uThreadID = xIt.GetKey();
		const Zenith_Vector<Zenith_Profiling::Event>& xSrcEvents = xIt.GetValue();

		Zenith_Vector<Zenith_Profiling::Event>* pxDstEvents = xDst.m_xThreadEvents.TryGet(uThreadID);
		if (pxDstEvents == nullptr)
		{
			pxDstEvents = &xDst.m_xThreadEvents.Emplace(uThreadID);
		}
		for (u_int u = 0; u < xSrcEvents.GetSize(); ++u)
		{
			pxDstEvents->PushBack(xSrcEvents.Get(u));
		}
	}
	xDst.m_uBeginTicks = xSrc.m_uBeginTicks;
	xDst.m_uEndTicks = xSrc.m_uEndTicks;
}

// --- Lifecycle ------------------------------------------------------------

void Zenith_Profiling::Initialise(Zenith_Multithreading& xThreading)
{
	m_pxThreading = &xThreading;

	// Idempotent w.r.t. already-registered threads: the main thread registers BEFORE
	// Initialise runs, so we must NOT clear the table or its tl_pxBuffer. Only the
	// snapshots are (lazily) allocated here.
	if (m_bInitialised) return;

	m_pxAccumulator = new Snapshot();
	m_pxDisplay = new Snapshot();
	m_pxWorst = new Snapshot();
	m_pxPinned = new Snapshot();

	// Resolve the frame-root zone once; every other zone registers lazily at first use
	// (via the ZENITH_PROFILE_SCOPE/_ZONE macros' static-locals).
	m_uTotalFrameZone = RegisterZone("Total Frame");
	m_uMainThreadID = xThreading.GetMainThreadID();

	m_bInitialised = true;
}

void Zenith_Profiling::Shutdown()
{
	Zenith_ScopedMutexLock_T xLock(m_xRegistrationMutex);

	const u_int uMainID = m_pxThreading ? m_pxThreading->GetMainThreadID() : ~0u;

	// Clear the main thread's TLS before freeing its ring (this runs on the main
	// thread, after the task workers have joined + unregistered).
	tl_pxBuffer = nullptr;

	for (u_int u = 0; u < uMAX_PROFILE_THREADS; ++u)
	{
		ThreadBuffer* pxBuf = m_apxThreadBuffers[u].load(std::memory_order_relaxed);
		if (pxBuf == nullptr) continue;

		if (u == uMainID)
		{
			m_apxThreadBuffers[u].store(nullptr, std::memory_order_release);
			delete pxBuf;
		}
		else
		{
			// A producer that never unregistered (e.g. the FileWatcher, whose
			// shutdown sleeps rather than joins) may still be live and writing its
			// ring. Leave it allocated rather than risk a use-after-free; harmless
			// at process teardown.
			Zenith_Warning(LOG_CATEGORY_CORE, "Profiling: thread %u still registered at shutdown; leaving its ring allocated to avoid a use-after-free", u);
		}
	}

	delete m_pxAccumulator; m_pxAccumulator = nullptr;
	delete m_pxDisplay;     m_pxDisplay = nullptr;
	delete m_pxWorst;       m_pxWorst = nullptr;
	delete m_pxPinned;      m_pxPinned = nullptr;
	m_bInitialised = false;
}

void Zenith_Profiling::RegisterThread()
{
	// Runs during engine bootstrap BEFORE Initialise() stores m_pxThreading (the
	// main thread registers first), so the thread-id query goes through
	// g_xEngine.Threading().
	const u_int uThreadID = g_xEngine.Threading().GetCurrentThreadID();
	if (uThreadID >= uMAX_PROFILE_THREADS)
	{
		Zenith_Warning(LOG_CATEGORY_CORE, "Profiling: thread id %u >= uMAX_PROFILE_THREADS (%u); this thread will not be profiled", uThreadID, uMAX_PROFILE_THREADS);
		tl_pxBuffer = nullptr;
		return;
	}

	Zenith_ScopedMutexLock_T xLock(m_xRegistrationMutex);
	ThreadBuffer* pxBuf = m_apxThreadBuffers[uThreadID].load(std::memory_order_relaxed);
	if (pxBuf == nullptr)
	{
		pxBuf = new ThreadBuffer();
		pxBuf->m_uThreadID = uThreadID;
		m_apxThreadBuffers[uThreadID].store(pxBuf, std::memory_order_release);
	}
	tl_pxBuffer = pxBuf;
}

void Zenith_Profiling::UnregisterThread()
{
	// Producer-thread exit: free this thread's ring and clear its TLS. Runs under the
	// registration mutex, which the consumer's drain also takes, so the drain can
	// never touch a freed ring.
	const u_int uThreadID = g_xEngine.Threading().GetCurrentThreadID();
	if (uThreadID >= uMAX_PROFILE_THREADS)
	{
		tl_pxBuffer = nullptr;
		return;
	}

	Zenith_ScopedMutexLock_T xLock(m_xRegistrationMutex);
	ThreadBuffer* pxBuf = m_apxThreadBuffers[uThreadID].load(std::memory_order_relaxed);
	m_apxThreadBuffers[uThreadID].store(nullptr, std::memory_order_release);
	tl_pxBuffer = nullptr;
	delete pxBuf;
}

// --- Frame boundaries -----------------------------------------------------

void Zenith_Profiling::BeginFrame()
{
	// Reset the accumulator BEFORE accumulating this frame, then open TOTAL_FRAME.
	ResetSnapshotLengths(*m_pxAccumulator);
	m_pxAccumulator->m_uBeginTicks = Zenith_Profiling_Detail::GetTimestamp();
	BeginProfileZone(m_uTotalFrameZone);
}

void Zenith_Profiling::EndFrame()
{
	// Close TOTAL_FRAME so its event lands in a ring, then drain.
	EndProfileZone(m_uTotalFrameZone);
	m_pxAccumulator->m_uEndTicks = Zenith_Profiling_Detail::GetTimestamp();

	if (dbg_bPauseRequested)
	{
		// Paused: drain-and-discard (no back-pressure) and freeze the display.
		DrainAllRings(*this, nullptr);
		return;
	}

	DrainAllRings(*this, m_pxAccumulator);

	// Publish via O(1) pointer swap. The displaced display storage becomes next
	// frame's accumulator and is reused (no container assignment, no realloc).
	Snapshot* pxTmp = m_pxDisplay;
	m_pxDisplay = m_pxAccumulator;
	m_pxAccumulator = pxTmp;

	// Frame-duration history (wall clock, independent of event sums).
	const float fFrameMs = static_cast<float>(TickDeltaToMs(m_pxDisplay->m_uBeginTicks, m_pxDisplay->m_uEndTicks));
	m_afFrameHistoryMs[m_uFrameHistoryHead] = fFrameMs;
	m_uFrameHistoryHead = (m_uFrameHistoryHead + 1) % uFRAME_HISTORY;
	if (m_uFrameHistoryCount < uFRAME_HISTORY) m_uFrameHistoryCount++;

	if (fFrameMs > m_fWorstFrameMs)
	{
		m_fWorstFrameMs = fFrameMs;
		CopyInto(*m_pxWorst, *m_pxDisplay);
	}

	// Pause-on-spike: capture (pin) and freeze the first frame over the threshold so a
	// stall is preserved for inspection instead of scrolling away. Debug-variables only
	// (writes the otherwise-const dbg_bPauseRequested).
#ifdef ZENITH_DEBUG_VARIABLES
	if (dbg_bPauseOnSpike && fFrameMs > dbg_fSpikeThresholdMs)
	{
		CopyInto(*m_pxPinned, *m_pxDisplay);
		dbg_bPauseRequested = true;
	}
#endif
}

#if ZENITH_MEMORY_TRACKING_ANY
void Zenith_Profiling::PushMemorySample(const Zenith_MemoryFrameSample& xSample)
{
	// Skip while paused so the Memory tab freezes with the CPU/GPU timeline (the last
	// sample + history persist). Main-thread only — no locking, just a POD copy + ring push.
	if (dbg_bPauseRequested)
	{
		return;
	}

	m_xMemSample = xSample;

	const float fTotal = static_cast<float>(xSample.m_ulTotalBytes);
	m_afMemHistoryBytes[m_uMemHistoryHead] = fTotal;
	const u_int uCats = (xSample.m_uCategoryCount < ZENITH_MEM_CAT_MAX) ? xSample.m_uCategoryCount : ZENITH_MEM_CAT_MAX;
	for (u_int c = 0; c < ZENITH_MEM_CAT_MAX; ++c)
	{
		m_aafMemHistoryCat[m_uMemHistoryHead][c] = (c < uCats) ? static_cast<float>(xSample.m_aulCategoryBytes[c]) : 0.0f;
	}
	m_uMemHistoryHead = (m_uMemHistoryHead + 1) % uFRAME_HISTORY;
	if (m_uMemHistoryCount < uFRAME_HISTORY)
	{
		m_uMemHistoryCount++;
	}
	if (fTotal > m_fMemPeakBytes)
	{
		m_fMemPeakBytes = fTotal;
	}
}
#endif

// --- Hot-path member + bridge entry points --------------------------------

void Zenith_Profiling::BeginProfileZone(const Zenith_ProfileZoneID uZoneID, const char* szLabel)
{
	DoBeginProfileZone(uZoneID, szLabel);
}

void Zenith_Profiling::EndProfileZone(const Zenith_ProfileZoneID uZoneID)
{
	DoEndProfileZone(uZoneID);
}

Zenith_ProfileZoneID Zenith_Profiling::RegisterZone(const char* szStaticName)
{
	if (szStaticName == nullptr) return ZENITH_PROFILE_ZONE_NULL;

	Zenith_ScopedMutexLock_T xLock(m_xRegistrationMutex);

	// Content dedup: the same name from any number of call sites maps to ONE id.
	const u_int uCount = m_uZoneCount.load(std::memory_order_relaxed);
	for (u_int u = 0; u < uCount; ++u)
	{
		if (m_axZoneDescs[u].m_szName != nullptr && strcmp(m_axZoneDescs[u].m_szName, szStaticName) == 0)
		{
			return u;
		}
	}

	if (uCount >= uMAX_ZONES)
	{
		return ZENITH_PROFILE_ZONE_OVERFLOW;
	}

	// Copy the name into the owned arena so any caller string (literal, temporary,
	// std::string::c_str) is lifetime-safe for the process.
	const size_t uLen = strlen(szStaticName) + 1;
	if (m_uArenaUsed + uLen > uARENA_BYTES)
	{
		return ZENITH_PROFILE_ZONE_OVERFLOW;
	}
	char* pszCopy = &m_acNameArena[m_uArenaUsed];
	memcpy(pszCopy, szStaticName, uLen);
	m_uArenaUsed += static_cast<u_int>(uLen);

	m_axZoneDescs[uCount].m_szName = pszCopy;
	m_axZoneDescs[uCount].m_uColorRGB = 0;
	m_axZoneDescs[uCount].m_uCategoryID = ZENITH_PROFILE_ZONE_NULL;
	m_uZoneCount.store(uCount + 1, std::memory_order_release);   // publish (UI reads acquire)
	return uCount;
}

const char* Zenith_Profiling::GetZoneName(const Zenith_ProfileZoneID uZoneID) const
{
	if (uZoneID == ZENITH_PROFILE_ZONE_NULL) return "(null)";
	if (uZoneID == ZENITH_PROFILE_ZONE_OVERFLOW) return "(zone overflow)";
	const u_int uCount = m_uZoneCount.load(std::memory_order_acquire);
	if (uZoneID < uCount && m_axZoneDescs[uZoneID].m_szName != nullptr)
	{
		return m_axZoneDescs[uZoneID].m_szName;
	}
	return "(unknown)";
}

Zenith_ProfileZoneID Zenith_Profiling::GetCurrentZoneID()
{
	ThreadBuffer* pxBuf = tl_pxBuffer;
	Zenith_Assert(pxBuf != nullptr && pxBuf->m_uDepth > 0, "Trying to get profiling zone but nothing is being profiled");
	return pxBuf->m_axStack[pxBuf->m_uDepth - 1].m_uZoneID;
}

void Zenith_Profiling::ClearEvents()
{
	// Live drain-and-discard + clear the accumulator: a following live report sees
	// only new work (the NavMesh ClearEvents -> work -> WriteTextReport pattern).
	DrainAllRings(*this, nullptr);
	ResetSnapshotLengths(*m_pxAccumulator);
}

// --- GPU per-pass timing channel (main thread only) ---------------------------
// The render backend opens a capture each time it reads back a frame's timestamps,
// pushes one entry per Flux_RenderGraph pass (in execution order), and commits the
// total. Clear() keeps capacity, so steady state never reallocates.
void Zenith_Profiling::BeginGPUCapture()
{
	m_xGPUPasses.Clear();
	m_fGPUBuildingTotalMs = 0.0;
}

void Zenith_Profiling::AddGPUPass(const char* szName, double fMilliseconds, u_int uExecIndex)
{
	GPUPass xPass;
	xPass.m_szName = szName ? szName : "(unnamed)";
	xPass.m_fMilliseconds = fMilliseconds;
	xPass.m_uExecIndex = uExecIndex;
	m_xGPUPasses.PushBack(xPass);
	m_fGPUBuildingTotalMs += fMilliseconds;
}

void Zenith_Profiling::EndGPUCapture()
{
	m_fGPUTotalMs = m_fGPUBuildingTotalMs;

	// Push total GPU ms into the rolling history ring (mirrors EndFrame's CPU
	// frame-time history). Plotted in the GPU viz tab.
	const float fMs = static_cast<float>(m_fGPUTotalMs);
	m_afGPUHistoryMs[m_uGPUHistoryHead] = fMs;
	m_uGPUHistoryHead = (m_uGPUHistoryHead + 1) % uFRAME_HISTORY;
	if (m_uGPUHistoryCount < uFRAME_HISTORY) ++m_uGPUHistoryCount;
	m_fWorstGPUMs = std::max(m_fWorstGPUMs, fMs);
}

namespace
{
	// One stats bucket per registered zone (keyed by dense zone id).
	struct IndexStats
	{
		double fTotalMs = 0.0;
		double fMinMs = 1e30;
		double fMaxMs = 0.0;
		uint32_t uCallCount = 0;
	};

	// Events that carry a runtime label (currently only the "Flux Record Pass" zone,
	// labelled with the pass DebugName) are aggregated by label so the report can show
	// which pass is the CPU-record hotspot — the main per-zone table aggregates the
	// label away.
	struct LabelStat { const char* m_szLabel; double m_fTotalMs; u_int m_uCount; };

	// Drains the accumulated per-thread events into the per-zone and per-label stats.
	void AggregateZoneAndLabelStats(Zenith_Profiling::Snapshot& xAccumulator, u_int uZoneCount,
		Zenith_Vector<IndexStats>& xStats, Zenith_Vector<LabelStat>& xLabelStats,
		u_int& uTotalEvents, u_int& uThreadCount)
	{
		for (Zenith_HashMap<u_int, Zenith_Vector<Zenith_Profiling::Event>>::Iterator xIt(xAccumulator.m_xThreadEvents); !xIt.Done(); xIt.Next())
		{
			const Zenith_Vector<Zenith_Profiling::Event>& xEvents = xIt.GetValue();
			const u_int uEventCount = xEvents.GetSize();
			if (uEventCount > 0)
				uThreadCount++;
			uTotalEvents += uEventCount;

			for (u_int u = 0; u < uEventCount; ++u)
			{
				const Zenith_Profiling::Event& xEvent = xEvents.Get(u);
				if (xEvent.m_uZoneID >= uZoneCount) continue;   // sentinel / overflow
				const double fDurationMs = TickDeltaToMs(xEvent.m_uBeginTicks, xEvent.m_uEndTicks);

				// Aggregate labelled events by label (pass DebugName pointers are shared
				// static literals, so a pointer compare hits first; strcmp is the fallback).
				if (xEvent.m_szLabel != nullptr)
				{
					bool bFoundLabel = false;
					for (u_int k = 0; k < xLabelStats.GetSize(); ++k)
					{
						LabelStat& xLS = xLabelStats.Get(k);
						if (xLS.m_szLabel == xEvent.m_szLabel || strcmp(xLS.m_szLabel, xEvent.m_szLabel) == 0)
						{
							xLS.m_fTotalMs += fDurationMs;
							xLS.m_uCount++;
							bFoundLabel = true;
							break;
						}
					}
					if (!bFoundLabel)
					{
						xLabelStats.PushBack(LabelStat{ xEvent.m_szLabel, fDurationMs, 1 });
					}
				}

				IndexStats& xStat = xStats.Get(xEvent.m_uZoneID);
				xStat.uCallCount++;
				xStat.fTotalMs += fDurationMs;
				if (fDurationMs < xStat.fMinMs)
					xStat.fMinMs = fDurationMs;
				if (fDurationMs > xStat.fMaxMs)
					xStat.fMaxMs = fDurationMs;
			}
		}
	}

	// Collects zones that fired, then sorts them by total time descending.
	Zenith_Vector<Zenith_ProfileZoneID> SortZonesByTotalTime(const Zenith_Vector<IndexStats>& xStats, u_int uZoneCount)
	{
		Zenith_Vector<Zenith_ProfileZoneID> xSorted;
		for (u_int i = 0; i < uZoneCount; ++i)
		{
			if (xStats.Get(i).uCallCount > 0)
				xSorted.PushBack(i);
		}

		for (u_int i = 0; i < xSorted.GetSize(); ++i)
		{
			for (u_int j = i + 1; j < xSorted.GetSize(); ++j)
			{
				if (xStats.Get(xSorted.Get(j)).fTotalMs > xStats.Get(xSorted.Get(i)).fTotalMs)
				{
					const Zenith_ProfileZoneID uTmp = xSorted.Get(i);
					xSorted.Get(i) = xSorted.Get(j);
					xSorted.Get(j) = uTmp;
				}
			}
		}
		return xSorted;
	}

	// Frame-time headline + the sorted per-zone CPU table.
	void WriteHeadlineAndZoneTable(FILE* pFile, const Zenith_Profiling& xSelf, double fDisplayFrameMs,
		u_int uThreadCount, u_int uTotalEvents,
		const Zenith_Vector<Zenith_ProfileZoneID>& xSorted, const Zenith_Vector<IndexStats>& xStats)
	{
		fprintf(pFile, "\n=== Profiling Report ===\n");
		fprintf(pFile, "Frame: %.3f ms (%.1f FPS, last complete) | Threads: %u | Events: %u\n\n",
			fDisplayFrameMs, fDisplayFrameMs > 0.0 ? 1000.0 / fDisplayFrameMs : 0.0, uThreadCount, uTotalEvents);
		fprintf(pFile, "%-40s %12s %10s %12s %12s %12s\n",
			"Profile Zone", "Total (ms)", "Calls", "Avg (ms)", "Min (ms)", "Max (ms)");
		fprintf(pFile, "---------------------------------------- ------------ ---------- ------------ ------------ ------------\n");

		for (u_int i = 0; i < xSorted.GetSize(); ++i)
		{
			const Zenith_ProfileZoneID uZoneID = xSorted.Get(i);
			const IndexStats& xStat = xStats.Get(uZoneID);
			const double fAvgMs = xStat.fTotalMs / xStat.uCallCount;
			fprintf(pFile, "%-40s %12.1f %10u %12.3f %12.3f %12.3f\n",
				xSelf.GetZoneName(uZoneID),
				xStat.fTotalMs,
				xStat.uCallCount,
				fAvgMs,
				xStat.fMinMs,
				xStat.fMaxMs);
		}
	}

	// GPU per-pass timings (populated by the backend's deferred timestamp readback).
	// Printed in execution order so each line maps to exactly one render-graph pass
	// bracket. Empty on the null backend or when GPU timestamps are unsupported.
	void WriteGPUPassesSection(FILE* pFile, const Zenith_Vector<Zenith_Profiling::GPUPass>& xGPUPasses, double fGPUTotalMs)
	{
		if (xGPUPasses.GetSize() == 0)
			return;

		fprintf(pFile, "\n=== GPU Passes (one bracket per Flux_RenderGraph pass, execution order) ===\n");
		fprintf(pFile, "Total GPU: %.3f ms across %u passes\n\n", fGPUTotalMs, xGPUPasses.GetSize());
		fprintf(pFile, "%4s  %-40s %12s\n", "exec", "GPU Pass", "GPU (ms)");
		fprintf(pFile, "----  ---------------------------------------- ------------\n");
		for (u_int i = 0; i < xGPUPasses.GetSize(); ++i)
		{
			const Zenith_Profiling::GPUPass& xPass = xGPUPasses.Get(i);
			fprintf(pFile, "%4u  %-40s %12.3f\n", xPass.m_uExecIndex, xPass.m_szName ? xPass.m_szName : "(unnamed)", xPass.m_fMilliseconds);
		}
	}

	// Per-pass CPU RECORD cost (by label = pass DebugName), sorted by CPU cost, with
	// the matched GPU cost alongside — closes the gap where the "Flux Record Pass"
	// zone aggregated away WHICH pass is expensive to record on the CPU. Pass names
	// join 1:1 because both the record label and the GPU pass name come from
	// pxPass->DebugName() (the same static literal).
	void WritePerPassCPUVsGPUSection(FILE* pFile, const Zenith_Vector<LabelStat>& xLabelStats,
		const Zenith_Vector<Zenith_Profiling::GPUPass>& xGPUPasses)
	{
		if (xLabelStats.GetSize() == 0)
			return;

		Zenith_Vector<u_int> xLabelOrder;
		for (u_int i = 0; i < xLabelStats.GetSize(); ++i) xLabelOrder.PushBack(i);
		for (u_int i = 0; i < xLabelOrder.GetSize(); ++i)
			for (u_int j = i + 1; j < xLabelOrder.GetSize(); ++j)
				if (xLabelStats.Get(xLabelOrder.Get(j)).m_fTotalMs > xLabelStats.Get(xLabelOrder.Get(i)).m_fTotalMs)
				{
					const u_int uTmp = xLabelOrder.Get(i); xLabelOrder.Get(i) = xLabelOrder.Get(j); xLabelOrder.Get(j) = uTmp;
				}

		fprintf(pFile, "\n=== Per-pass CPU record cost (sorted) + matched GPU cost ===\n");
		fprintf(pFile, "%-40s %14s %12s\n", "Pass", "CPU rec (ms)", "GPU (ms)");
		fprintf(pFile, "---------------------------------------- -------------- ------------\n");
		for (u_int r = 0; r < xLabelOrder.GetSize(); ++r)
		{
			const LabelStat& xLS = xLabelStats.Get(xLabelOrder.Get(r));
			double fGpuMs = -1.0;
			for (u_int g = 0; g < xGPUPasses.GetSize(); ++g)
			{
				const Zenith_Profiling::GPUPass& xGP = xGPUPasses.Get(g);
				if (xGP.m_szName && (xGP.m_szName == xLS.m_szLabel || strcmp(xGP.m_szName, xLS.m_szLabel) == 0))
				{
					fGpuMs = xGP.m_fMilliseconds;
					break;
				}
			}
			if (fGpuMs >= 0.0)
				fprintf(pFile, "%-40s %14.3f %12.3f\n", xLS.m_szLabel, xLS.m_fTotalMs, fGpuMs);
			else
				fprintf(pFile, "%-40s %14.3f %12s\n", xLS.m_szLabel, xLS.m_fTotalMs, "-");
		}
	}
}

void Zenith_Profiling::WriteTextReport(FILE* pFile)
{
	// Live capture: drain completed events into the accumulator, then aggregate.
	DrainAllRings(*this, m_pxAccumulator);

	const u_int uZoneCount = m_uZoneCount.load(std::memory_order_acquire);
	Zenith_Vector<IndexStats> xStats;
	xStats.Reserve(uZoneCount);
	for (u_int u = 0; u < uZoneCount; ++u) xStats.PushBack(IndexStats{});

	Zenith_Vector<LabelStat> xLabelStats;
	u_int uTotalEvents = 0;
	u_int uThreadCount = 0;
	AggregateZoneAndLabelStats(*m_pxAccumulator, uZoneCount, xStats, xLabelStats, uTotalEvents, uThreadCount);

	const Zenith_Vector<Zenith_ProfileZoneID> xSorted = SortZonesByTotalTime(xStats, uZoneCount);

	// Frame time comes from the DISPLAY snapshot (the previous COMPLETE frame): the
	// live report drains the in-flight frame whose TOTAL_FRAME zone is still open, so
	// the per-zone table below is the in-flight frame while this headline ms is the
	// last completed frame's wall-clock.
	const double fDisplayFrameMs = TickDeltaToMs(m_pxDisplay->m_uBeginTicks, m_pxDisplay->m_uEndTicks);
	WriteHeadlineAndZoneTable(pFile, *this, fDisplayFrameMs, uThreadCount, uTotalEvents, xSorted, xStats);
	WriteGPUPassesSection(pFile, m_xGPUPasses, m_fGPUTotalMs);
	WritePerPassCPUVsGPUSection(pFile, xLabelStats, m_xGPUPasses);

#if ZENITH_MEMORY_TRACKING_ANY
	// Combined CPU+GPU+memory snapshot: a single --profiling-dump now also covers memory.
	fprintf(pFile, "\n");
	Zenith_MemoryManagement::WriteReport(pFile);
#endif

	fprintf(pFile, "\n");
}

// --- Bridge forwarders (hot path; operate on the thread_local, no g_xEngine) --
void Zenith_Profiling_Detail::BeginProfileZone(Zenith_ProfileZoneID uZoneID, const char* szLabel)
{
	DoBeginProfileZone(uZoneID, szLabel);
}

void Zenith_Profiling_Detail::EndProfileZone(Zenith_ProfileZoneID uZoneID)
{
	DoEndProfileZone(uZoneID);
}

Zenith_ProfileZoneID Zenith_Profiling_Detail::RegisterZone(const char* szStaticName)
{
	// Cold path (once per unique zone via the macro's static-local); reaching the
	// subsystem here is fine -- the per-event path stays engine-free.
	return g_xEngine.Profiling().RegisterZone(szStaticName);
}

void Zenith_Profiling_Detail::UnregisterThread()
{
	g_xEngine.Profiling().UnregisterThread();
}

#ifdef ZENITH_TOOLS
static Zenith_Maths::Vector3 HSV2RGB(const Zenith_Maths::Vector3 xHSV)
{
	const float c = xHSV[2] * xHSV[1];
	const float x = c * (1.0f - glm::abs(fmodf(xHSV[0] / 60.f, 2.f) - 1.f));
	const float m = xHSV[2] - c;

	Zenith_Maths::Vector3 xRet;
	if (xHSV[0] < 60.f)
	{
		xRet = {c,x,0.f};
	}
	else if (xHSV[0] < 120.f)
	{
		xRet = {x,c,0.f};
	}
	else if (xHSV[0] < 180)
	{
		xRet = {0.f,c,x};
	}
	else if (xHSV[0] < 240)
	{
		xRet = { 0.f,x,c };
	}
	else if (xHSV[0] < 300)
	{
		xRet = { x,0.f,c };
	}
	else
	{
		xRet = {c,0.f,x};
	}
	return xRet + Zenith_Maths::Vector3(m,m,m);
}

static Zenith_Maths::Vector3 IntToColour(u_int u)
{
	const float fHue = fmodf((u * 0.61803398875f) * 360.f, 360.f);
	return HSV2RGB({fHue,0.8f,0.5f});
}

// Deterministic per-zone colour, computed on the fly (works for any dense zone id,
// including string zones beyond the old fixed range).
static ImU32 ZoneColour(const Zenith_ProfileZoneID uZoneID)
{
	const Zenith_Maths::Vector3 xColour = IntToColour(uZoneID) * 255.f;
	return IM_COL32((int)xColour.r, (int)xColour.g, (int)xColour.b, 255);
}

// --- GPU pass viz helpers ---------------------------------------------------
// FNV-1a 32-bit string hash — stable across frames/runs for a given pass name, so
// a pass keeps its colour even when an upstream pass toggles on/off (shifting
// execution indices).
static u_int HashName(const char* szName)
{
	u_int uHash = 2166136261u;
	if (szName)
	{
		for (const char* p = szName; *p; ++p)
		{
			uHash ^= static_cast<u_int>(static_cast<unsigned char>(*p));
			uHash *= 16777619u;
		}
	}
	return uHash;
}

// Stable per-name colour, routed through IntToColour so GPU passes share the same
// palette family as CPU zones.
static ImU32 NameColour(const char* szName)
{
	const Zenith_Maths::Vector3 xColour = IntToColour(HashName(szName)) * 255.f;
	return IM_COL32((int)xColour.r, (int)xColour.g, (int)xColour.b, 255);
}

// Derive a coarse category from a pass name: the leading run up to the first
// space / underscore / digit. "HiZ Mip 3" -> "HiZ", "Shadow Cascade 2" -> "Shadow",
// "HDR_BloomDownsample Mip3" -> "HDR", "SSR DenoiseH" -> "SSR". Returns into a
// caller buffer to avoid std::string churn.
static void DeriveCategory(const char* szName, char* acOut, u_int uOutSize)
{
	if (!szName || uOutSize == 0) { if (uOutSize) acOut[0] = '\0'; return; }
	u_int o = 0;
	for (const char* p = szName; *p && o < uOutSize - 1; ++p)
	{
		const char c = *p;
		if (c == ' ' || c == '_' || (c >= '0' && c <= '9')) break;
		acOut[o++] = c;
	}
	acOut[o] = '\0';
	if (o == 0)   // name began with a delimiter/digit — fall back to the whole name
	{
		u_int u = 0;
		for (const char* p = szName; *p && u < uOutSize - 1; ++p) acOut[u++] = *p;
		acOut[u] = '\0';
	}
}

// Friendly lane label: the main thread reads "Main", others "Thread N".
static void FormatThreadLaneName(char* pszOut, size_t uSize, u_int uThreadID, u_int uMainThreadID)
{
	if (uThreadID == uMainThreadID)
		snprintf(pszOut, uSize, "Main");
	else
		snprintf(pszOut, uSize, "Thread %u", uThreadID);
}

// Aggregated per-zone statistics for the displayed frame, summed across all threads,
// with a substring filter and total-time-descending sort.
static void RenderStatistics(Zenith_Profiling& xSelf)
{
	static char ls_acFilter[64] = "";
	ImGui::SetNextItemWidth(200.0f);
	ImGui::InputText("Filter", ls_acFilter, sizeof(ls_acFilter));

	const Zenith_Profiling::Snapshot& xDisp = xSelf.GetDisplaySnapshot();
	const u_int uZoneCount = xSelf.m_uZoneCount.load(std::memory_order_acquire);

	struct Stat { double fTotal = 0.0; double fMin = 1e30; double fMax = 0.0; u_int uCalls = 0; };
	Zenith_Vector<Stat> xStats;
	xStats.Reserve(uZoneCount);
	for (u_int u = 0; u < uZoneCount; ++u) xStats.PushBack(Stat{});

	for (Zenith_HashMap<u_int, Zenith_Vector<Zenith_Profiling::Event>>::Iterator xIt(xDisp.m_xThreadEvents); !xIt.Done(); xIt.Next())
	{
		const Zenith_Vector<Zenith_Profiling::Event>& xEvents = xIt.GetValue();
		for (u_int u = 0; u < xEvents.GetSize(); ++u)
		{
			const Zenith_Profiling::Event& xEvent = xEvents.Get(u);
			if (xEvent.m_uZoneID >= uZoneCount) continue;
			const double fMs = TickDeltaToMs(xEvent.m_uBeginTicks, xEvent.m_uEndTicks);
			Stat& xStat = xStats.Get(xEvent.m_uZoneID);
			xStat.uCalls++;
			xStat.fTotal += fMs;
			if (fMs < xStat.fMin) xStat.fMin = fMs;
			if (fMs > xStat.fMax) xStat.fMax = fMs;
		}
	}

	Zenith_Vector<Zenith_ProfileZoneID> xSorted;
	for (u_int i = 0; i < uZoneCount; ++i)
	{
		if (xStats.Get(i).uCalls == 0) continue;
		if (ls_acFilter[0] != '\0' && strstr(xSelf.GetZoneName(i), ls_acFilter) == nullptr) continue;
		xSorted.PushBack(i);
	}
	for (u_int i = 0; i < xSorted.GetSize(); ++i)
		for (u_int j = i + 1; j < xSorted.GetSize(); ++j)
			if (xStats.Get(xSorted.Get(j)).fTotal > xStats.Get(xSorted.Get(i)).fTotal)
			{
				const Zenith_ProfileZoneID uTmp = xSorted.Get(i);
				xSorted.Get(i) = xSorted.Get(j);
				xSorted.Get(j) = uTmp;
			}

	if (!ImGui::BeginTable("ProfileStats", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		return;
	ImGui::TableSetupColumn("Zone", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Total ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
	ImGui::TableSetupColumn("Avg ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
	ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
	ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 70.0f);
	ImGui::TableHeadersRow();
	for (u_int i = 0; i < xSorted.GetSize(); ++i)
	{
		const Zenith_ProfileZoneID uZoneID = xSorted.Get(i);
		const Stat& xStat = xStats.Get(uZoneID);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		const ImVec2 xPos = ImGui::GetCursorScreenPos();
		pxDraw->AddRectFilled(ImVec2(xPos.x + 2, xPos.y + 2), ImVec2(xPos.x + 14, xPos.y + 14), ZoneColour(uZoneID), 2.0f);
		ImGui::Dummy(ImVec2(16, 14)); ImGui::SameLine();
		ImGui::TextUnformatted(xSelf.GetZoneName(uZoneID));
		ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", xStat.fTotal);
		ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", xStat.fTotal / xStat.uCalls);
		ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", xStat.fMax);
		ImGui::TableSetColumnIndex(4); ImGui::Text("%u", xStat.uCalls);
	}
	ImGui::EndTable();
}

// ===========================================================================
// GPU per-pass visualization (the "GPU" tab). Entries arrive in Flux_RenderGraph
// execution order (the backend readback sorts by topological index), so a serial
// GPU queue lets us lay passes end-to-end on a cumulative-ms axis — an RGP/PIX-style
// GPU timeline — without needing absolute timestamps in the viz layer.
// ===========================================================================

// Persisted GPU-view UI state (held as a static local in RenderToImGui, mirroring
// the CPU timeline's TimelineViewState).
struct GPUViewState
{
	float m_fZoom = 1.0f;            // 1.0 = whole GPU frame fits the strip width
	float m_fScrollPx = 0.0f;        // horizontal pan, pixels
	bool  m_bSortByCost = false;     // table: false = execution order, true = ms desc
	bool  m_bGroupByCategory = false;// collapse passes into name-prefix categories
};

// One drawable bar in the strip (a pass, or a collapsed category).
struct GPUStripSegment
{
	const char* m_szName = nullptr;
	double      m_fMs = 0.0;
	ImU32       m_uColour = 0;
	u_int       m_uCount = 1;        // >1 for a collapsed category
	int         m_iExecIndex = -1;   // execution-order index for a single pass, else -1
};

// A summed category row (passes sharing a name prefix), in first-appearance order.
struct GPUCatRow
{
	char   m_acName[48];
	double m_fMs;
	u_int  m_uCount;
	ImU32  m_uColour;
};

// Collapse the execution-ordered pass list into per-category rows, preserving the
// order each category first appears (so the grouped strip stays roughly execution-ordered).
static void BuildGPUCategories(const Zenith_Vector<Zenith_Profiling::GPUPass>& xPasses, Zenith_Vector<GPUCatRow>& xOut)
{
	for (u_int i = 0; i < xPasses.GetSize(); ++i)
	{
		char acCat[48];
		DeriveCategory(xPasses.Get(i).m_szName, acCat, sizeof(acCat));
		bool bFound = false;
		for (u_int c = 0; c < xOut.GetSize(); ++c)
		{
			if (strcmp(xOut.Get(c).m_acName, acCat) == 0)
			{
				xOut.Get(c).m_fMs += xPasses.Get(i).m_fMilliseconds;
				xOut.Get(c).m_uCount++;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			GPUCatRow xRow{};
			u_int u = 0;
			for (; acCat[u] && u < sizeof(xRow.m_acName) - 1; ++u) xRow.m_acName[u] = acCat[u];
			xRow.m_acName[u] = '\0';
			xRow.m_fMs = xPasses.Get(i).m_fMilliseconds;
			xRow.m_uCount = 1;
			xRow.m_uColour = NameColour(acCat);
			xOut.PushBack(xRow);
		}
	}
}

// The headline GPU timeline strip: time-proportional bars laid end-to-end in
// execution order, zoom/pan/hover. Reuses the CPU flame-graph drawing idiom verbatim
// (full-size Dummy canvas + GetWindowDrawList + AddRectFilled/AddText + manual
// mouse-in-rect hit-test + BeginTooltip).
static void DrawGPUStrip(const Zenith_Vector<GPUStripSegment>& xSegs, double fTotalMs, GPUViewState& xView)
{
	const float fAvailW = ImGui::GetContentRegionAvail().x;
	constexpr float fStripH = 46.0f;
	constexpr float fMinBarW = 3.0f;
	const double fTotal = std::max(fTotalMs, 1e-4);

	ImGui::BeginChild("GPUStrip", ImVec2(0, fStripH + 12.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::Dummy(ImVec2(fAvailW, fStripH));
	ImDrawList* const pxDraw = ImGui::GetWindowDrawList();
	const ImVec2 xOrigin = ImGui::GetItemRectMin();
	const bool bHovered = ImGui::IsItemHovered();
	const ImVec2 xMouse = ImGui::GetMousePos();

	if (bHovered)
	{
		if (ImGui::GetIO().MouseWheel != 0.0f)
		{
			const float fOldZoom = xView.m_fZoom;
			xView.m_fZoom = std::clamp(xView.m_fZoom * (1.0f + ImGui::GetIO().MouseWheel * 0.1f), 1.0f, 200.0f);
			const float fMouseX = xMouse.x - xOrigin.x;
			const float fRatio = xView.m_fZoom / fOldZoom;
			xView.m_fScrollPx = std::max(0.0f, (xView.m_fScrollPx + fMouseX) * fRatio - fMouseX);
		}
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
			xView.m_fScrollPx = std::max(0.0f, xView.m_fScrollPx - ImGui::GetIO().MouseDelta.x);
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { xView.m_fZoom = 1.0f; xView.m_fScrollPx = 0.0f; }
	}

	const float fPxPerMs = (fAvailW * xView.m_fZoom) / static_cast<float>(fTotal);
	int iHover = -1;
	double fCum = 0.0;
	for (u_int i = 0; i < xSegs.GetSize(); ++i)
	{
		const GPUStripSegment& xS = xSegs.Get(i);
		const float fStartPx = static_cast<float>(fCum) * fPxPerMs - xView.m_fScrollPx;
		const float fTrueW = static_cast<float>(xS.m_fMs) * fPxPerMs;
		const float fDrawW = std::max(fTrueW, fMinBarW);
		fCum += xS.m_fMs;
		if (fStartPx + fDrawW < 0.0f || fStartPx > fAvailW) continue;   // off-screen cull

		const ImVec2 xMin(xOrigin.x + fStartPx, xOrigin.y);
		const ImVec2 xMax(xOrigin.x + fStartPx + fDrawW, xOrigin.y + fStripH);
		const bool bH = bHovered && xMouse.x >= xMin.x && xMouse.x <= xMax.x && xMouse.y >= xMin.y && xMouse.y <= xMax.y;
		if (bH) iHover = static_cast<int>(i);

		pxDraw->AddRectFilled(xMin, xMax, bH ? IM_COL32(255, 255, 255, 255) : xS.m_uColour, 3.0f);
		pxDraw->AddRect(xMin, xMax, IM_COL32(0, 0, 0, 90), 3.0f);   // separator so same-hue neighbours read distinctly

		const char* szName = xS.m_szName ? xS.m_szName : "(unnamed)";
		const float fTextW = ImGui::CalcTextSize(szName).x;
		if (fTextW + 6.0f <= fDrawW)
		{
			const ImU32 uTxt = bH ? IM_COL32(0, 0, 0, 255) : IM_COL32_WHITE;
			pxDraw->AddText(ImVec2(xMin.x + 3.0f, xMin.y + 5.0f), uTxt, szName);
			char acMs[32];
			snprintf(acMs, sizeof(acMs), "%.3f ms", xS.m_fMs);
			if (ImGui::CalcTextSize(acMs).x + 6.0f <= fDrawW)
				pxDraw->AddText(ImVec2(xMin.x + 3.0f, xMin.y + 24.0f), uTxt, acMs);
		}
	}

	if (iHover >= 0)
	{
		const GPUStripSegment& xS = xSegs.Get(static_cast<u_int>(iHover));
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(xS.m_szName ? xS.m_szName : "(unnamed)");
		ImGui::Separator();
		if (xS.m_iExecIndex >= 0) ImGui::Text("Exec index: %d", xS.m_iExecIndex);
		if (xS.m_uCount > 1)      ImGui::Text("Passes: %u", xS.m_uCount);
		if (xS.m_fMs >= 1.0)      ImGui::Text("GPU: %.3f ms", xS.m_fMs);
		else                      ImGui::Text("GPU: %.1f us", xS.m_fMs * 1000.0);
		ImGui::Text("Frame %%: %.2f%%", (xS.m_fMs / fTotal) * 100.0);
		ImGui::EndTooltip();
	}

	ImGui::EndChild();
	ImGui::TextDisabled("scroll = zoom, middle-drag = pan, double-click = reset");
}

// The per-pass (or per-category) detail table. Default order is execution order;
// "sort by cost" reorders a LOCAL index array only, so the strip + the source list
// stay in execution order.
static void RenderGPUPassTable(Zenith_Profiling& xSelf, GPUViewState& xView, const Zenith_Vector<GPUCatRow>& xCats)
{
	const double fTotal = std::max(xSelf.m_fGPUTotalMs, 1e-4);

	if (xView.m_bGroupByCategory)
	{
		double fMaxMs = 0.0001;
		for (u_int c = 0; c < xCats.GetSize(); ++c) fMaxMs = std::max(fMaxMs, xCats.Get(c).m_fMs);
		if (ImGui::BeginTable("GPUCats", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("% frame", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();
			for (u_int c = 0; c < xCats.GetSize(); ++c)
			{
				const GPUCatRow& xR = xCats.Get(c);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImDrawList* const pxDraw = ImGui::GetWindowDrawList();
				const ImVec2 xPos = ImGui::GetCursorScreenPos();
				pxDraw->AddRectFilled(ImVec2(xPos.x + 2, xPos.y + 2), ImVec2(xPos.x + 14, xPos.y + 14), xR.m_uColour, 2.0f);
				ImGui::Dummy(ImVec2(16, 14)); ImGui::SameLine();
				ImGui::Text("%s (x%u)", xR.m_acName, xR.m_uCount);
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", xR.m_fMs);
				ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f%%", (xR.m_fMs / fTotal) * 100.0);
				ImGui::TableSetColumnIndex(3); ImGui::ProgressBar(static_cast<float>(xR.m_fMs / fMaxMs), ImVec2(-1.0f, 0.0f), "");
			}
			ImGui::EndTable();
		}
		return;
	}

	const Zenith_Vector<Zenith_Profiling::GPUPass>& xPasses = xSelf.m_xGPUPasses;
	Zenith_Vector<u_int> xOrder;
	for (u_int i = 0; i < xPasses.GetSize(); ++i) xOrder.PushBack(i);
	if (xView.m_bSortByCost)   // descending by ms; selection sort (mirrors WriteTextReport), N~65
	{
		for (u_int i = 0; i < xOrder.GetSize(); ++i)
			for (u_int j = i + 1; j < xOrder.GetSize(); ++j)
				if (xPasses.Get(xOrder.Get(j)).m_fMilliseconds > xPasses.Get(xOrder.Get(i)).m_fMilliseconds)
				{
					const u_int uTmp = xOrder.Get(i); xOrder.Get(i) = xOrder.Get(j); xOrder.Get(j) = uTmp;
				}
	}

	double fMaxMs = 0.0001;
	for (u_int i = 0; i < xPasses.GetSize(); ++i) fMaxMs = std::max(fMaxMs, xPasses.Get(i).m_fMilliseconds);

	if (ImGui::BeginTable("GPUPasses", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("% frame", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		for (u_int r = 0; r < xOrder.GetSize(); ++r)
		{
			const Zenith_Profiling::GPUPass& xP = xPasses.Get(xOrder.Get(r));
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImDrawList* const pxDraw = ImGui::GetWindowDrawList();
			const ImVec2 xPos = ImGui::GetCursorScreenPos();
			pxDraw->AddRectFilled(ImVec2(xPos.x + 2, xPos.y + 2), ImVec2(xPos.x + 14, xPos.y + 14), NameColour(xP.m_szName), 2.0f);
			ImGui::Dummy(ImVec2(16, 14)); ImGui::SameLine();
			ImGui::TextUnformatted(xP.m_szName ? xP.m_szName : "(unnamed)");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", xP.m_fMilliseconds);
			ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f%%", (xP.m_fMilliseconds / fTotal) * 100.0);
			ImGui::TableSetColumnIndex(3); ImGui::ProgressBar(static_cast<float>(xP.m_fMilliseconds / fMaxMs), ImVec2(-1.0f, 0.0f), "");
		}
		ImGui::EndTable();
	}
}

// GPU tab orchestrator: frame-time history graph + toolbar + execution-order timeline
// strip + detail table.
static void RenderGPUView(Zenith_Profiling& xSelf, GPUViewState& xView)
{
	const Zenith_Vector<Zenith_Profiling::GPUPass>& xPasses = xSelf.m_xGPUPasses;
	ImGui::Text("GPU Frame: %.3f ms across %u passes  (worst %.3f ms)",
		xSelf.m_fGPUTotalMs, xPasses.GetSize(), xSelf.m_fWorstGPUMs);

	if (xPasses.GetSize() == 0)
	{
		ImGui::TextDisabled("No GPU pass timings yet (null backend or timestamps unsupported).");
		return;
	}

	// GPU frame-time history (mirror of the CPU frame-time PlotLines).
	float fMaxHistMs = 1.0f;
	for (u_int i = 0; i < Zenith_Profiling::uFRAME_HISTORY; ++i) fMaxHistMs = std::max(fMaxHistMs, xSelf.m_afGPUHistoryMs[i]);
	ImGui::PlotLines("##GPUFrameTimes", xSelf.m_afGPUHistoryMs, static_cast<int>(Zenith_Profiling::uFRAME_HISTORY),
		static_cast<int>(xSelf.m_uGPUHistoryHead), "GPU ms", 0.0f, fMaxHistMs * 1.1f, ImVec2(-1.0f, 60.0f));

	ImGui::Checkbox("Group by category", &xView.m_bGroupByCategory);
	ImGui::SameLine(); ImGui::Checkbox("Sort by cost", &xView.m_bSortByCost);
	ImGui::SameLine(); if (ImGui::Button("Reset GPU Worst")) xSelf.m_fWorstGPUMs = 0.0f;
	ImGui::Separator();

	// Build strip segments (per-pass, or collapsed per-category) and draw the strip.
	Zenith_Vector<GPUStripSegment> xSegs;
	Zenith_Vector<GPUCatRow> xCats;
	if (xView.m_bGroupByCategory)
	{
		BuildGPUCategories(xPasses, xCats);
		for (u_int c = 0; c < xCats.GetSize(); ++c)
		{
			GPUStripSegment xSeg;
			xSeg.m_szName = xCats.Get(c).m_acName;     // stable: xCats is fully built and not grown hereafter
			xSeg.m_fMs = xCats.Get(c).m_fMs;
			xSeg.m_uColour = xCats.Get(c).m_uColour;
			xSeg.m_uCount = xCats.Get(c).m_uCount;
			xSeg.m_iExecIndex = -1;
			xSegs.PushBack(xSeg);
		}
	}
	else
	{
		for (u_int i = 0; i < xPasses.GetSize(); ++i)
		{
			GPUStripSegment xSeg;
			xSeg.m_szName = xPasses.Get(i).m_szName;
			xSeg.m_fMs = xPasses.Get(i).m_fMilliseconds;
			xSeg.m_uColour = NameColour(xPasses.Get(i).m_szName);
			xSeg.m_uCount = 1;
			xSeg.m_iExecIndex = static_cast<int>(i);
			xSegs.PushBack(xSeg);
		}
	}
	DrawGPUStrip(xSegs, xSelf.m_fGPUTotalMs, xView);

	ImGui::Separator();
	RenderGPUPassTable(xSelf, xView, xCats);
}

#if ZENITH_MEMORY_TRACKING_ANY
// Human-readable byte formatter for the Memory tab/HUD.
static const char* MemFormatBytes(u_int64 ulBytes, char* acBuf, size_t uLen)
{
	if (ulBytes >= (1ull << 30))      snprintf(acBuf, uLen, "%.2f GB", static_cast<double>(ulBytes) / (1024.0 * 1024.0 * 1024.0));
	else if (ulBytes >= (1ull << 20)) snprintf(acBuf, uLen, "%.2f MB", static_cast<double>(ulBytes) / (1024.0 * 1024.0));
	else if (ulBytes >= 1024)         snprintf(acBuf, uLen, "%.2f KB", static_cast<double>(ulBytes) / 1024.0);
	else                              snprintf(acBuf, uLen, "%llu B", ulBytes);
	return acBuf;
}

// Memory tab: total-bytes history graph + per-category live breakdown, read from the
// per-frame POD sample the main loop pushes. LITE leaves per-frame delta counters at 0.
static void RenderMemoryView(Zenith_Profiling& xSelf)
{
	const Zenith_MemoryFrameSample& xS = xSelf.m_xMemSample;
	char acA[64], acB[64];

	ImGui::Text("Tracked: %s   Peak: %s   Live allocations: %llu",
		MemFormatBytes(xS.m_ulTotalBytes, acA, sizeof(acA)),
		MemFormatBytes(xS.m_ulPeakBytes, acB, sizeof(acB)),
		xS.m_ulTotalAllocations);
	if (xS.m_ilFrameDeltaBytes != 0 || xS.m_uFrameAllocations != 0 || xS.m_uFrameDeallocations != 0)
	{
		ImGui::Text("Frame delta: %+.2f KB   allocs: %u  frees: %u",
			static_cast<double>(xS.m_ilFrameDeltaBytes) / 1024.0, xS.m_uFrameAllocations, xS.m_uFrameDeallocations);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Peak")) xSelf.m_fMemPeakBytes = 0.0f;

	// Total-bytes history (peak MB annotated), mirroring the GPU-history plot.
	{
		float fMaxBytes = 1.0f;
		for (u_int i = 0; i < Zenith_Profiling::uFRAME_HISTORY; ++i) fMaxBytes = std::max(fMaxBytes, xSelf.m_afMemHistoryBytes[i]);
		char acOverlay[48];
		snprintf(acOverlay, sizeof(acOverlay), "%.1f MB", fMaxBytes / (1024.0f * 1024.0f));
		ImGui::PlotLines("##MemTotal", xSelf.m_afMemHistoryBytes, static_cast<int>(Zenith_Profiling::uFRAME_HISTORY),
			static_cast<int>(xSelf.m_uMemHistoryHead), acOverlay, 0.0f, fMaxBytes * 1.1f, ImVec2(-1.0f, 60.0f));
	}

	ImGui::Separator();

	// Per-category table: bytes, live count, and a share bar vs the current total.
	if (ImGui::BeginTable("MemCategoryTable", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Allocated", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		const u_int uCats = (xS.m_uCategoryCount < ZENITH_MEM_CAT_MAX) ? xS.m_uCategoryCount : ZENITH_MEM_CAT_MAX;
		const double fTotal = (xS.m_ulTotalBytes > 0) ? static_cast<double>(xS.m_ulTotalBytes) : 1.0;
		for (u_int i = 0; i < uCats && i < MEMORY_CATEGORY_COUNT; ++i)
		{
			if (xS.m_aulCategoryBytes[i] == 0 && xS.m_aulCategoryCount[i] == 0)
			{
				continue;
			}
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", GetMemoryCategoryName(static_cast<Zenith_MemoryCategory>(i)));
			ImGui::TableNextColumn(); ImGui::Text("%s", MemFormatBytes(xS.m_aulCategoryBytes[i], acA, sizeof(acA)));
			ImGui::TableNextColumn(); ImGui::Text("%llu", xS.m_aulCategoryCount[i]);
			ImGui::TableNextColumn();
			ImGui::ProgressBar(static_cast<float>(static_cast<double>(xS.m_aulCategoryBytes[i]) / fTotal), ImVec2(-1.0f, 0.0f));
		}
		ImGui::EndTable();
	}

	// Unified sources — engine CPU + Jolt + VRAM + ... (VRAM shown separately, never
	// summed into process RAM). Populated once per frame by Zenith_MemoryAccounting.
	ImGui::Separator();
	ImGui::TextDisabled("Unified sources");
	if (ImGui::BeginTable("MemSourcesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Allocations", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		for (u_int i = 0; i < Zenith_MemoryAccounting::GetSourceCount(); ++i)
		{
			const Zenith_MemorySource& xSrc = Zenith_MemoryAccounting::GetSource(i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s%s", xSrc.m_szName, xSrc.m_bIsVRAM ? " [VRAM]" : "");
			ImGui::TableNextColumn(); ImGui::Text("%s", MemFormatBytes(xSrc.m_ulBytes, acA, sizeof(acA)));
			ImGui::TableNextColumn(); ImGui::Text("%llu", xSrc.m_ulAllocCount);
		}
		ImGui::EndTable();
	}
	ImGui::Text("Process RAM: %s   |   VRAM: %s",
		MemFormatBytes(Zenith_MemoryAccounting::GetTotalProcessRAM(), acA, sizeof(acA)),
		MemFormatBytes(Zenith_MemoryAccounting::GetTotalVRAM(), acB, sizeof(acB)));
}

void Zenith_Profiling::RenderMemoryHUD()
{
	if (!dbg_bShowMemoryHUD)
	{
		return;
	}

	const Zenith_MemoryFrameSample& xS = m_xMemSample;
	ImGuiViewport* pxVP = ImGui::GetMainViewport();
	const ImVec2 xPos(pxVP->WorkPos.x + pxVP->WorkSize.x - 10.0f, pxVP->WorkPos.y + 10.0f);
	ImGui::SetNextWindowPos(xPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.55f);
	const ImGuiWindowFlags uFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing;
	if (ImGui::Begin("##MemoryHUD", nullptr, uFlags))
	{
		char acA[64];
		if (xS.m_ilFrameDeltaBytes != 0)
		{
			ImGui::Text("MEM %s  (%+.1f KB)", MemFormatBytes(xS.m_ulTotalBytes, acA, sizeof(acA)),
				static_cast<double>(xS.m_ilFrameDeltaBytes) / 1024.0);
		}
		else
		{
			ImGui::Text("MEM %s", MemFormatBytes(xS.m_ulTotalBytes, acA, sizeof(acA)));
		}

		// Worst category vs its budget — red when over, orange when near.
		const Zenith_MemoryWorstOffender xWorst = Zenith_MemoryBudgets::GetWorstOffender();
		if (xWorst.m_fUsagePercent > 0.0f)
		{
			const ImVec4 xCol = (xWorst.m_fUsagePercent > 100.0f)
				? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
			ImGui::TextColored(xCol, "%s %.0f%% of budget",
				GetMemoryCategoryName(xWorst.m_eCategory), xWorst.m_fUsagePercent);
		}
	}
	ImGui::End();
}
#endif // ZENITH_MEMORY_TRACKING_ANY

void Zenith_Profiling::RenderToImGui()
{
	ImGui::Begin(szEDITOR_WINDOW_PROFILING);

	static TimelineViewState ls_xTimelineState;
	static bool ls_bShowStats = true;
	static u_int ls_uSelectedThreadID = 0;
	static GPUViewState ls_xGPUView;

	// Frame statistics (the previous, published frame).
	const float fFrameDurationMs = static_cast<float>(TickDeltaToMs(m_pxDisplay->m_uBeginTicks, m_pxDisplay->m_uEndTicks));
	const float fFPS = (fFrameDurationMs > 0.0f) ? (1000.0f / fFrameDurationMs) : 0.0f;

	if (ls_bShowStats)
	{
		ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", fFrameDurationMs, fFPS);
		ImGui::SameLine();
		ImGui::Text(" | Threads: %u | Worst: %.2f ms", m_pxDisplay->m_xThreadEvents.GetSize(), m_fWorstFrameMs);
	}

	// Scrolling frame-time history graph. The ring is plotted oldest->newest via the
	// values_offset parameter so a stall shows up as a spike on the right.
	{
		float fMaxMs = 1.0f;
		for (u_int i = 0; i < uFRAME_HISTORY; ++i) fMaxMs = std::max(fMaxMs, m_afFrameHistoryMs[i]);
		ImGui::PlotLines("##FrameTimes", m_afFrameHistoryMs, static_cast<int>(uFRAME_HISTORY), static_cast<int>(m_uFrameHistoryHead),
			"frame ms", 0.0f, fMaxMs * 1.1f, ImVec2(-1.0f, 60.0f));
	}

	// Global controls available in all tabs.
	ImGui::Checkbox("Paused", &dbg_bPauseRequested);
#ifdef ZENITH_DEBUG_VARIABLES
	ImGui::SameLine();
	ImGui::Checkbox("Pause on spike", &dbg_bPauseOnSpike);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat("Spike ms", &dbg_fSpikeThresholdMs, 5.0f, 100.0f, "%.1f");
#endif
	ImGui::SameLine();
	if (ImGui::Button("Reset Worst")) m_fWorstFrameMs = 0.0f;
	ImGui::Separator();

	if (ImGui::BeginTabBar("ProfilingTabs"))
	{
		if (ImGui::BeginTabItem("Timeline"))
		{
			RenderTimelineView(ls_xTimelineState);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Statistics"))
		{
			RenderStatistics(*this);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Thread Breakdown"))
		{
			RenderThreadBreakdown(fFrameDurationMs, ls_uSelectedThreadID);
			ImGui::EndTabItem();
		}

		// GPU tab: per-Flux_RenderGraph-pass GPU times from the backend's deferred
		// timestamp readback. One row per pass bracket, in execution order, with a
		// share bar scaled against the slowest pass. Empty on the null backend.
		// GPU tab: per-Flux_RenderGraph-pass GPU times in execution order — a
		// time-proportional timeline strip (zoom/pan/hover) + frame-time history +
		// detail table with optional sort-by-cost / category grouping.
		if (ImGui::BeginTabItem("GPU"))
		{
			RenderGPUView(*this, ls_xGPUView);
			ImGui::EndTabItem();
		}

#if ZENITH_MEMORY_TRACKING_ANY
		// Memory tab: per-frame tracked-bytes history + per-category live breakdown,
		// from Zenith_MemoryManagement::SampleFrame() pushed each frame by the main loop.
		if (ImGui::BeginTabItem("Memory"))
		{
			RenderMemoryView(*this);
			ImGui::EndTabItem();
		}
#endif

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// Per-frame canvas, layout, and cache state for the timeline event renderer.
struct TimelineRenderContext
{
	ImDrawList* pxDrawList;
	ImVec2 xCanvasPos;
	ImVec2 xCanvasMax;
	ImVec2 xMousePos;
	bool bIsHovered;
	float fCanvasWidth;
	float fCanvasTimeScale;
	float fTimelineScroll;
	float fThreadHeight;
	float fRowHeight;
	float fRowSpacing;
	int iMinDepthToRender;
	int iMaxDepthToRender;
	int iMaxDepthToRenderSeparately;
};

struct TimelineHoveredEvent
{
	const Zenith_Profiling::Event* pEvent = nullptr;
	const char* pszName = nullptr;   // resolved zone name (no g_xEngine reach in the tooltip)
	float fDurationNs = 0.0f;
};

// Draws every visible event in the previous frame and reports which one the
// mouse hovers, so the caller can render the tooltip outside the loop.
static TimelineHoveredEvent RenderTimelineEvents(const TimelineRenderContext& xCtx)
{
	auto& xSelf = g_xEngine.Profiling();
	const Zenith_Profiling::Snapshot& xDisplay = xSelf.GetDisplaySnapshot();
	TimelineHoveredEvent xHovered;
	for (Zenith_HashMap<u_int, Zenith_Vector<Zenith_Profiling::Event>>::Iterator xIt(xDisplay.m_xThreadEvents); !xIt.Done(); xIt.Next())
	{
		const u_int uThreadID = xIt.GetKey();
		const Zenith_Vector<Zenith_Profiling::Event>& xEvents = xIt.GetValue();
		const float fThreadBaseY = xCtx.xCanvasPos.y + uThreadID * xCtx.fThreadHeight;

		char acLabel[64];
		FormatThreadLaneName(acLabel, sizeof(acLabel), uThreadID, xSelf.m_uMainThreadID);
		xCtx.pxDrawList->AddText(ImVec2(xCtx.xCanvasPos.x, fThreadBaseY), IM_COL32_WHITE, acLabel);

		const u_int uEventCount = xEvents.GetSize();
		for (u_int u = 0; u < uEventCount; ++u)
		{
			const Zenith_Profiling::Event& xEvent = xEvents.Get(uEventCount - u - 1);

			if (xEvent.m_uDepth < static_cast<u_int>(xCtx.iMinDepthToRender) || xEvent.m_uDepth > static_cast<u_int>(xCtx.iMaxDepthToRender))
				continue;

			const u_int uRowIndex = (xEvent.m_uDepth <= static_cast<u_int>(xCtx.iMaxDepthToRenderSeparately))
				? (xEvent.m_uDepth - static_cast<u_int>(xCtx.iMinDepthToRender))
				: (static_cast<u_int>(xCtx.iMaxDepthToRenderSeparately) - static_cast<u_int>(xCtx.iMinDepthToRender));

			const float fEventStartNs = static_cast<float>(TickDeltaToNs(xDisplay.m_uBeginTicks, xEvent.m_uBeginTicks));
			const float fEventEndNs = static_cast<float>(TickDeltaToNs(xDisplay.m_uBeginTicks, xEvent.m_uEndTicks));
			const float fEventDurationNs = fEventEndNs - fEventStartNs;

			const float fStartPx = (fEventStartNs * xCtx.fCanvasTimeScale) - xCtx.fTimelineScroll;
			const float fEndPx = (fEventEndNs * xCtx.fCanvasTimeScale) - xCtx.fTimelineScroll;

			if (fEndPx < 0.0f || fStartPx > xCtx.fCanvasWidth)
				continue;

			const float fRowY = fThreadBaseY + uRowIndex * (xCtx.fRowHeight + xCtx.fRowSpacing);
			const ImVec2 xRectMin = ImVec2(xCtx.xCanvasPos.x + fStartPx, fRowY);
			const ImVec2 xRectMax = ImVec2(xCtx.xCanvasPos.x + fEndPx, fRowY + xCtx.fRowHeight);

			const ImVec2 xClampedMin = ImVec2(std::max(xRectMin.x, xCtx.xCanvasPos.x), xRectMin.y);
			const ImVec2 xClampedMax = ImVec2(std::min(xRectMax.x, xCtx.xCanvasMax.x), xRectMax.y);

			const bool bIsEventHovered = xCtx.bIsHovered &&
				xCtx.xMousePos.x >= xClampedMin.x && xCtx.xMousePos.x <= xClampedMax.x &&
				xCtx.xMousePos.y >= xClampedMin.y && xCtx.xMousePos.y <= xClampedMax.y;

			const ImU32 uColor = bIsEventHovered
				? IM_COL32(255, 255, 255, 255)
				: ZoneColour(xEvent.m_uZoneID);

			xCtx.pxDrawList->AddRectFilled(xClampedMin, xClampedMax, uColor, 3.0f);

			const char* szDisplayName = xEvent.m_szLabel ? xEvent.m_szLabel : xSelf.GetZoneName(xEvent.m_uZoneID);
			const float fDisplayTextWidth = ImGui::CalcTextSize(szDisplayName).x;
			const float fRectWidth = xRectMax.x - xRectMin.x;
			if (fDisplayTextWidth <= fRectWidth)
			{
				const ImVec2 xTextPos = ImVec2(std::max(xRectMin.x, xCtx.xCanvasPos.x), xRectMin.y);
				const ImU32 uTextColor = bIsEventHovered ? IM_COL32(0, 0, 0, 255) : IM_COL32_WHITE;
				xCtx.pxDrawList->AddText(xTextPos, uTextColor, szDisplayName);
			}

			if (bIsEventHovered)
			{
				xHovered.pEvent = &xEvent;
				xHovered.pszName = szDisplayName;
				xHovered.fDurationNs = fEventDurationNs;
			}
		}
	}
	return xHovered;
}

static void RenderTimelineHoverTooltip(const TimelineHoveredEvent& xHovered, float fFrameDurationNs)
{
	if (xHovered.pEvent == nullptr) return;

	ImGui::BeginTooltip();
	const char* szHoveredName = xHovered.pszName ? xHovered.pszName : "(unknown)";
	ImGui::Text("%s", szHoveredName);
	ImGui::Separator();

	const float fDurationUs = xHovered.fDurationNs / 1000.0f;
	const float fDurationMs = fDurationUs / 1000.0f;

	if (fDurationMs >= 1.0f)
	{
		ImGui::Text("Duration: %.3f ms", fDurationMs);
	}
	else
	{
		ImGui::Text("Duration: %.3f us", fDurationUs);
	}

	ImGui::Text("Depth: %u", xHovered.pEvent->m_uDepth);

	const float fPercentOfFrame = (xHovered.fDurationNs / fFrameDurationNs) * 100.0f;
	ImGui::Text("Frame %%: %.2f%%", fPercentOfFrame);

	ImGui::EndTooltip();
}

void Zenith_Profiling::RenderTimelineView(TimelineViewState& xState)
{
	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderInt("Min Depth to Render", &xState.m_iMinDepthToRender, 0, 10);
		ImGui::SliderInt("Max Depth to Render", &xState.m_iMaxDepthToRender, 0, 20);
		ImGui::SliderInt("Max Depth to Render Separately", &xState.m_iMaxDepthToRenderSeparately, 0, 20);
		ImGui::SliderFloat("Vertical Scale", &xState.m_fVerticalScale, 0.5f, 4.0f, "%.1fx");
	}

	xState.m_iMaxDepthToRender = std::max(xState.m_iMaxDepthToRender, xState.m_iMinDepthToRender);
	xState.m_iMaxDepthToRenderSeparately = std::clamp(xState.m_iMaxDepthToRenderSeparately, xState.m_iMinDepthToRender, xState.m_iMaxDepthToRender);

	constexpr float fBASE_ROW_HEIGHT = 20.0f;
	constexpr float fBASE_ROW_SPACING = 5.0f;
	constexpr float fTHREAD_SPACING = 30.0f;

	const float fRowHeight = fBASE_ROW_HEIGHT * xState.m_fVerticalScale;
	const float fRowSpacing = fBASE_ROW_SPACING * xState.m_fVerticalScale;

	const u_int uSeparateRowCount = xState.m_iMaxDepthToRenderSeparately - xState.m_iMinDepthToRender + 1;
	const u_int uRowsPerThread = uSeparateRowCount;
	const float fThreadHeight = uRowsPerThread * (fRowHeight + fRowSpacing) + fTHREAD_SPACING;

	const float fCanvasWidth = ImGui::GetContentRegionAvail().x;
	const float fTotalHeight = static_cast<float>(m_pxDisplay->m_xThreadEvents.GetSize()) * fThreadHeight;

	ImGui::BeginChild("Timeline", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::Dummy(ImVec2(fCanvasWidth, fTotalHeight));
	ImDrawList* const pxDrawList = ImGui::GetWindowDrawList();
	const ImVec2 xCanvasPos = ImGui::GetItemRectMin();
	const ImVec2 xCanvasMax = ImGui::GetItemRectMax();
	const bool bIsHovered = ImGui::IsItemHovered();

	if (bIsHovered)
	{
		if (ImGui::GetIO().MouseWheel != 0.0f)
		{
			const float fOldZoom = xState.m_fTimelineZoom;
			xState.m_fTimelineZoom *= (1.0f + ImGui::GetIO().MouseWheel * 0.1f);
			xState.m_fTimelineZoom = std::clamp(xState.m_fTimelineZoom, 0.1f, 100.0f);

			const float fMouseX = ImGui::GetMousePos().x - xCanvasPos.x;
			const float fZoomRatio = xState.m_fTimelineZoom / fOldZoom;
			xState.m_fTimelineScroll = (xState.m_fTimelineScroll + fMouseX) * fZoomRatio - fMouseX;
			xState.m_fTimelineScroll = std::max(0.0f, xState.m_fTimelineScroll);
		}

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			xState.m_fTimelineScroll -= ImGui::GetIO().MouseDelta.x;
			xState.m_fTimelineScroll = std::max(0.0f, xState.m_fTimelineScroll);
		}
	}

	// Zone colours + label widths are computed on the fly (ZoneColour / GetZoneName) so
	// dynamically-registered string zones render correctly without a fixed-size cache.
	const float fFrameDuration = static_cast<float>(TickDeltaToNs(m_pxDisplay->m_uBeginTicks, m_pxDisplay->m_uEndTicks));
	const float fCanvasTimeScale = (fCanvasWidth * xState.m_fTimelineZoom) / fFrameDuration;

	TimelineRenderContext xCtx;
	xCtx.pxDrawList = pxDrawList;
	xCtx.xCanvasPos = xCanvasPos;
	xCtx.xCanvasMax = xCanvasMax;
	xCtx.xMousePos = ImGui::GetMousePos();
	xCtx.bIsHovered = bIsHovered;
	xCtx.fCanvasWidth = fCanvasWidth;
	xCtx.fCanvasTimeScale = fCanvasTimeScale;
	xCtx.fTimelineScroll = xState.m_fTimelineScroll;
	xCtx.fThreadHeight = fThreadHeight;
	xCtx.fRowHeight = fRowHeight;
	xCtx.fRowSpacing = fRowSpacing;
	xCtx.iMinDepthToRender = xState.m_iMinDepthToRender;
	xCtx.iMaxDepthToRender = xState.m_iMaxDepthToRender;
	xCtx.iMaxDepthToRenderSeparately = xState.m_iMaxDepthToRenderSeparately;

	const TimelineHoveredEvent xHovered = RenderTimelineEvents(xCtx);
	RenderTimelineHoverTooltip(xHovered, fFrameDuration);

	ImGui::EndChild();
}

struct ProfileNode
{
	Zenith_ProfileZoneID m_uZoneID;
	float fTotalTimeMs;
	float fSelfTimeMs;
	u_int uCallCount;
	u_int uDepth;
	const Zenith_Profiling::Event* pEvent; // Original event for time comparisons.
	Zenith_Vector<ProfileNode> xChildren;

	ProfileNode() : m_uZoneID(ZENITH_PROFILE_ZONE_NULL), fTotalTimeMs(0.0f), fSelfTimeMs(0.0f), uCallCount(0), uDepth(0), pEvent(nullptr) {}
};

static Zenith_Vector<const Zenith_Profiling::Event*> SortEventsByStart(const Zenith_Vector<Zenith_Profiling::Event>& xThreadEvents)
{
	Zenith_Vector<const Zenith_Profiling::Event*> xSorted;
	const u_int uEventCount = xThreadEvents.GetSize();
	for (u_int u = 0; u < uEventCount; ++u)
	{
		xSorted.PushBack(&xThreadEvents.Get(u));
	}
	std::sort(xSorted.GetDataPointer(), xSorted.GetDataPointer() + xSorted.GetSize(),
		[](const Zenith_Profiling::Event* a, const Zenith_Profiling::Event* b) { return a->m_uBeginTicks < b->m_uBeginTicks; });
	return xSorted;
}

// Self-time starts equal to total time; it is reduced as children are added.
static ProfileNode MakeNodeFromEvent(const Zenith_Profiling::Event* pEvent)
{
	const float fDurationNs = static_cast<float>(TickDeltaToNs(pEvent->m_uBeginTicks, pEvent->m_uEndTicks));
	const float fDurationMs = fDurationNs / 1000000.0f;

	ProfileNode xNode;
	xNode.m_uZoneID = pEvent->m_uZoneID;
	xNode.fTotalTimeMs = fDurationMs;
	xNode.fSelfTimeMs = fDurationMs;
	xNode.uCallCount = 1;
	xNode.uDepth = pEvent->m_uDepth;
	xNode.pEvent = pEvent;
	return xNode;
}

// Scopes that ended before the new event begins must not collect further children.
static void PopExpiredScopes(Zenith_Vector<ProfileNode*>& xDepthStack, const Zenith_Profiling::Event* pNewEvent)
{
	while (xDepthStack.GetSize() > 0)
	{
		const ProfileNode* pStackTop = xDepthStack.Get(xDepthStack.GetSize() - 1);
		if (pStackTop->pEvent->m_uEndTicks <= pNewEvent->m_uBeginTicks)
		{
			xDepthStack.Remove(xDepthStack.GetSize() - 1);
		}
		else
		{
			break;
		}
	}
}

static void TrimStackToDepth(Zenith_Vector<ProfileNode*>& xDepthStack, u_int uDepth)
{
	while (xDepthStack.GetSize() > uDepth)
	{
		xDepthStack.Remove(xDepthStack.GetSize() - 1);
	}
}

// Assembles a thread's events (in start-time order) into a parent/child tree
// keyed on the per-event depth field.
static Zenith_Vector<ProfileNode> BuildProfileHierarchy(const Zenith_Vector<Zenith_Profiling::Event>& xThreadEvents)
{
	const Zenith_Vector<const Zenith_Profiling::Event*> xSortedEvents = SortEventsByStart(xThreadEvents);

	Zenith_Vector<ProfileNode> xRootNodes;
	Zenith_Vector<ProfileNode*> xDepthStack; // xDepthStack[i] = current parent node at depth i
	xDepthStack.Reserve(16);

	for (u_int u = 0; u < xSortedEvents.GetSize(); ++u)
	{
		const Zenith_Profiling::Event* pEvent = xSortedEvents.Get(u);
		ProfileNode xNode = MakeNodeFromEvent(pEvent);

		PopExpiredScopes(xDepthStack, pEvent);
		TrimStackToDepth(xDepthStack, pEvent->m_uDepth);

		if (pEvent->m_uDepth == 0)
		{
			xRootNodes.PushBack(xNode);
			TrimStackToDepth(xDepthStack, 0);
			xDepthStack.PushBack(&xRootNodes.Get(xRootNodes.GetSize() - 1));
		}
		else if (xDepthStack.GetSize() >= pEvent->m_uDepth)
		{
			ProfileNode* pParent = xDepthStack.Get(pEvent->m_uDepth - 1);
			pParent->xChildren.PushBack(xNode);
			pParent->fSelfTimeMs -= xNode.fTotalTimeMs;
			TrimStackToDepth(xDepthStack, pEvent->m_uDepth);
			xDepthStack.PushBack(&pParent->xChildren.Get(pParent->xChildren.GetSize() - 1));
		}
	}
	return xRootNodes;
}

static void RenderThreadSelector(u_int& uThreadID)
{
	ImGui::Text("Select Thread:");

	auto& xSelf = g_xEngine.Profiling();
	const Zenith_Profiling::Snapshot& xDisplay = xSelf.GetDisplaySnapshot();
	Zenith_Vector<u_int> xAvailableThreads;
	for (Zenith_HashMap<u_int, Zenith_Vector<Zenith_Profiling::Event>>::Iterator xIt(xDisplay.m_xThreadEvents); !xIt.Done(); xIt.Next())
	{
		xAvailableThreads.PushBack(xIt.GetKey());
	}
	std::sort(xAvailableThreads.GetDataPointer(), xAvailableThreads.GetDataPointer() + xAvailableThreads.GetSize());

	char acCurrentThreadLabel[64];
	snprintf(acCurrentThreadLabel, sizeof(acCurrentThreadLabel), "Thread %u", uThreadID);

	if (!ImGui::BeginCombo("Thread", acCurrentThreadLabel))
		return;

	for (u_int u = 0; u < xAvailableThreads.GetSize(); ++u)
	{
		const u_int uID = xAvailableThreads.Get(u);
		char acThreadLabel[64];
		snprintf(acThreadLabel, sizeof(acThreadLabel), "Thread %u", uID);

		const bool bIsSelected = (uThreadID == uID);
		if (ImGui::Selectable(acThreadLabel, bIsSelected))
		{
			uThreadID = uID;
		}
		if (bIsSelected)
		{
			ImGui::SetItemDefaultFocus();
		}
	}
	ImGui::EndCombo();
}

struct ProfileRowContext
{
	float fFrameDurationMs;
	u_int uThreadID;
	const Zenith_Profiling* pxSelf;   // for GetZoneName (no g_xEngine reach in the row renderer)
	u_int uNodeIDCounter;
};

// Renders one table row per node, recursing into children when open.
// uNodeIDCounter gives every node a unique ImGui ID.
static void RenderProfileNodeRow(const ProfileNode& xNode, u_int uIndentLevel, ProfileRowContext& xCtx)
{
	const u_int uCurrentNodeID = xCtx.uNodeIDCounter++;

	ImGui::TableNextRow();

	// Color swatch
	ImGui::TableSetColumnIndex(0);
	ImDrawList* pxDrawList = ImGui::GetWindowDrawList();
	const ImVec2 xCursorPos = ImGui::GetCursorScreenPos();
	const float fSwatchSize = 16.0f;
	const float fIndent = uIndentLevel * 20.0f;
	pxDrawList->AddRectFilled(
		ImVec2(xCursorPos.x + 2 + fIndent, xCursorPos.y + 2),
		ImVec2(xCursorPos.x + fSwatchSize + fIndent, xCursorPos.y + fSwatchSize),
		ZoneColour(xNode.m_uZoneID),
		2.0f
	);
	ImGui::Dummy(ImVec2(fSwatchSize + fIndent, fSwatchSize));

	// Profile name (indented tree node)
	ImGui::TableSetColumnIndex(1);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + uIndentLevel * 20.0f);

	const bool bHasChildren = xNode.xChildren.GetSize() > 0;
	char acNodeID[128];
	snprintf(acNodeID, sizeof(acNodeID), "###node_%u_%u", xCtx.uThreadID, uCurrentNodeID);

	const char* szNodeName = xCtx.pxSelf->GetZoneName(xNode.m_uZoneID);
	bool bNodeOpen = false;
	if (bHasChildren)
	{
		bNodeOpen = ImGui::TreeNodeEx(acNodeID, ImGuiTreeNodeFlags_SpanFullWidth, "%s", szNodeName);
	}
	else
	{
		const ImGuiTreeNodeFlags eFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
		ImGui::TreeNodeEx(acNodeID, eFlags, "%s", szNodeName);
	}

	// Total time
	ImGui::TableSetColumnIndex(2);
	if (xNode.fTotalTimeMs >= 1.0f)
		ImGui::Text("%.3f ms", xNode.fTotalTimeMs);
	else
		ImGui::Text("%.3f us", xNode.fTotalTimeMs * 1000.0f);

	// Self time
	ImGui::TableSetColumnIndex(3);
	if (xNode.fSelfTimeMs >= 1.0f)
		ImGui::Text("%.3f ms", xNode.fSelfTimeMs);
	else if (xNode.fSelfTimeMs >= 0.0f)
		ImGui::Text("%.3f us", xNode.fSelfTimeMs * 1000.0f);
	else
		ImGui::Text("0.000 us");

	// Percentage
	ImGui::TableSetColumnIndex(4);
	const float fPercentOfFrame = (xCtx.fFrameDurationMs > 0.0f) ? (xNode.fTotalTimeMs / xCtx.fFrameDurationMs) * 100.0f : 0.0f;
	ImGui::Text("%.2f%%", fPercentOfFrame);

	// Call count
	ImGui::TableSetColumnIndex(5);
	ImGui::Text("%u", xNode.uCallCount);

	if (bNodeOpen && bHasChildren)
	{
		for (u_int i = 0; i < xNode.xChildren.GetSize(); ++i)
		{
			RenderProfileNodeRow(xNode.xChildren.Get(i), uIndentLevel + 1, xCtx);
		}
		ImGui::TreePop();
	}
}

void Zenith_Profiling::RenderThreadBreakdown(float fFrameDurationMs, u_int& uThreadID)
{
	RenderThreadSelector(uThreadID);
	ImGui::Separator();

	const Zenith_Vector<Event>* pxThreadEvents = m_pxDisplay->m_xThreadEvents.TryGet(uThreadID);
	if (pxThreadEvents == nullptr)
	{
		ImGui::Text("Thread %u not found in profiling data", uThreadID);
		return;
	}

	const Zenith_Vector<Event>& xThreadEvents = *pxThreadEvents;
	if (xThreadEvents.GetSize() == 0)
	{
		ImGui::Text("No events recorded for Thread %u", uThreadID);
		return;
	}

	Zenith_Vector<ProfileNode> xRootNodes = BuildProfileHierarchy(xThreadEvents);

	ImGui::Text("Thread %u - Hierarchical Breakdown", uThreadID);
	ImGui::Separator();

	if (!ImGui::BeginTable("ProfileBreakdown", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		return;

	ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 20.0f);
	ImGui::TableSetupColumn("Profile Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Total Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
	ImGui::TableSetupColumn("Self Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
	ImGui::TableSetupColumn("% of Frame", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Call Count", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableHeadersRow();

	ProfileRowContext xCtx{ fFrameDurationMs, uThreadID, this, 0u };
	for (u_int i = 0; i < xRootNodes.GetSize(); ++i)
	{
		RenderProfileNodeRow(xRootNodes.Get(i), 0, xCtx);
	}

	ImGui::EndTable();
}
#endif // ZENITH_TOOLS

#else // !ZENITH_PROFILING_ENABLED

// Profiling compiled out. The subsystem object is still allocated by the engine but
// inert; the hot path is already gone at the call sites (ZENITH_PROFILE_SCOPE / ScopeZone
// and the function-wrapper macro expand to nothing). These stubs only catch the few direct
// g_xEngine.Profiling().X() callers (frame loop, mutex, tools), each now a single empty
// call. No rings, no snapshots, no threads registered -> nothing to tear down.
void Zenith_Profiling::Initialise(Zenith_Multithreading&) {}
void Zenith_Profiling::Shutdown() {}
void Zenith_Profiling::RegisterThread() {}
void Zenith_Profiling::UnregisterThread() {}
void Zenith_Profiling::BeginFrame() {}
void Zenith_Profiling::EndFrame() {}
Zenith_ProfileZoneID Zenith_Profiling::RegisterZone(const char*) { return ZENITH_PROFILE_ZONE_NULL; }
void Zenith_Profiling::BeginProfileZone(const Zenith_ProfileZoneID, const char*) {}
void Zenith_Profiling::EndProfileZone(const Zenith_ProfileZoneID) {}
const char* Zenith_Profiling::GetZoneName(const Zenith_ProfileZoneID) const { return ""; }
Zenith_ProfileZoneID Zenith_Profiling::GetCurrentZoneID() { return ZENITH_PROFILE_ZONE_NULL; }
void Zenith_Profiling::ClearEvents() {}
void Zenith_Profiling::WriteTextReport(FILE*) {}
void Zenith_Profiling::BeginGPUCapture() {}
void Zenith_Profiling::AddGPUPass(const char*, double, u_int) {}
void Zenith_Profiling::EndGPUCapture() {}
#if ZENITH_MEMORY_TRACKING_ANY
void Zenith_Profiling::PushMemorySample(const Zenith_MemoryFrameSample&) {}
#endif

void Zenith_Profiling_Detail::BeginProfileZone(Zenith_ProfileZoneID, const char*) {}
void Zenith_Profiling_Detail::EndProfileZone(Zenith_ProfileZoneID) {}
Zenith_ProfileZoneID Zenith_Profiling_Detail::RegisterZone(const char*) { return ZENITH_PROFILE_ZONE_NULL; }
void Zenith_Profiling_Detail::UnregisterThread() {}

#ifdef ZENITH_TOOLS
void Zenith_Profiling::RenderToImGui() {}
void Zenith_Profiling::RenderTimelineView(TimelineViewState&) {}
void Zenith_Profiling::RenderThreadBreakdown(float, u_int&) {}
#if ZENITH_MEMORY_TRACKING_ANY
void Zenith_Profiling::RenderMemoryHUD() {}
#endif
#endif

#endif // ZENITH_PROFILING_ENABLED
