#pragma once

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"

#include <chrono>
#include <atomic>

#if ZENITH_MEMORY_TRACKING_ANY
// Leaf POD only (no allocator/tracker header) — keeps the layer-0 profiler free of an
// up-edge onto the memory module. The sample is produced by the main loop.
#include "Memory/Zenith_MemoryFrameSample.h"
#endif

// Compile-time master switch. Default ON; a shipping/Retail config defines this to 0
// to strip the profiler. When 0: the hot path (ZENITH_PROFILE_SCOPE / ScopeZone +
// ZENITH_PROFILING_FUNCTION_WRAPPER) compiles to nothing, the per-task profiling calls
// drop out, and every subsystem method becomes a no-op stub (see the #else block in
// Zenith_Profiling.cpp). The subsystem object is still allocated (a few hundred bytes,
// inert) so engine/editor wiring needs no gating — true zero-cost for the per-zone /
// per-frame hot path, near-zero elsewhere.
#ifndef ZENITH_PROFILING_ENABLED
#define ZENITH_PROFILING_ENABLED 1
#endif

class Zenith_Multithreading;

// Boot markers gathered by the platform entry point BEFORE the engine exists, handed
// to Zenith_Core::Zenith_Init by pointer and imported into the boot capture just
// before it seals. Plain POD with static-literal names: the platform layer owns the
// storage as a local, so nothing is stored statically and nothing outlives the call.
// Callers that have no markers (Android, the automated-test driver) pass nullptr.
struct Zenith_BootMarkerBundle
{
	static constexpr u_int uMAX_BOOT_BUNDLE_MARKERS = 16;

	struct Marker
	{
		const char* m_szName = nullptr;   // static literal, never owned
		u_int64     m_uTicks = 0;
	};

	Marker  m_axMarkers[uMAX_BOOT_BUNDLE_MARKERS]{};
	u_int   m_uCount = 0;
	u_int64 m_uProcessStartTicks = 0;     // ~ platform entry point; the boot timeline origin

	void Add(const char* szName, const u_int64 uTicks)
	{
		if (m_uCount >= uMAX_BOOT_BUNDLE_MARKERS) return;
		m_axMarkers[m_uCount].m_szName = szName;
		m_axMarkers[m_uCount].m_uTicks = uTicks;
		++m_uCount;
	}
};

// Dense zone ids. Every recorded event stores a Zenith_ProfileZoneID, minted lazily by
// RegisterZone from a string name (content-deduped). The ZENITH_PROFILE_SCOPE("Name")
// and ZENITH_PROFILE_ZONE("Name") macros are the call-site entry points. Names resolve
// at DISPLAY time via GetZoneName.
using Zenith_ProfileZoneID = u_int;
static constexpr Zenith_ProfileZoneID ZENITH_PROFILE_ZONE_NULL     = 0xFFFFFFFFu; // distinct from id 0 (TOTAL_FRAME)
static constexpr Zenith_ProfileZoneID ZENITH_PROFILE_ZONE_OVERFLOW = 0xFFFFFFFEu; // table/arena full sentinel

struct Zenith_ProfileZoneDesc
{
	const char* m_szName = nullptr;                      // owned copy in the name arena
	u_int       m_uColorRGB = 0;                         // 0 => derive from id
	Zenith_ProfileZoneID m_uCategoryID = ZENITH_PROFILE_ZONE_NULL;
};

// Bridge so header-inline code (ScopeZone and ZENITH_PROFILING_FUNCTION_WRAPPER)
// can reach the per-thread profiling state without including Zenith_Engine.h
// here, which would cycle. The ENABLED definitions in Zenith_Profiling.cpp
// operate directly on the per-thread ring (tl_pxBuffer) — no g_xEngine reach,
// no lock — so the hot path stays engine-free.
namespace Zenith_Profiling_Detail
{
	// String-zone bridge (used by the ZENITH_PROFILE_SCOPE/_ZONE macros). RegisterZone
	// is a cold path (interns the name once); BeginProfileZone/EndProfileZone are the
	// hot path and operate directly on the per-thread ring.
	Zenith_ProfileZoneID RegisterZone(const char* szStaticName);
	void BeginProfileZone(Zenith_ProfileZoneID uZoneID, const char* szLabel);
	void EndProfileZone(Zenith_ProfileZoneID uZoneID);

	// Thread-exit unregister for header-inline producers (e.g. the task worker loop)
	// that should free their ring without reaching g_xEngine directly.
	void UnregisterThread();

	// Raw monotonic timestamp in CLOCK TICKS (not nanoseconds). The hot path stores
	// the raw u_int64 tick count; ticks are converted to ns only at display time via
	// GetTicksToNs(), captured once. Header-inline + cross-platform: high_resolution_clock
	// wraps QueryPerformanceCounter on Windows and the monotonic clock on Android.
	inline u_int64 GetTimestamp()
	{
		return static_cast<u_int64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	}
	inline double GetTicksToNs()
	{
		using Period = std::chrono::high_resolution_clock::period;
		// num/den is the tick duration in SECONDS; scale to ns. On MSVC the period is
		// std::nano, so this folds to 1.0 at compile time.
		return static_cast<double>(Period::num) * 1.0e9 / static_cast<double>(Period::den);
	}
}

// Held on g_xEngine, accessed via g_xEngine.Profiling().
//
// Storage model (rev. 3): each producing thread owns a lock-free SPSC ring
// (Zenith_ProfileThreadBuffer) published into m_apxThreadBuffers by stable thread
// id. The hot path (Begin/EndProfile) writes its own thread's ring with a single
// release-store and NO lock / NO hashmap / NO g_xEngine reach. The main thread is
// the sole consumer: it drains every ring at the frame boundary into a heap
// Snapshot and hands the snapshot off by POINTER SWAP (never container assignment,
// which would free+reallocate). See Zenith_Profiling.cpp for the full protocol.
class Zenith_Profiling
{
public:
	// Non-trivial: the ctor allocates the BootControlBlock so it exists before the
	// main thread's RegisterThread (which runs BEFORE Initialise). The dtor frees it
	// only if Shutdown did not already decide to leak it alongside surviving rings.
	Zenith_Profiling();
	~Zenith_Profiling();
	Zenith_Profiling(const Zenith_Profiling&) = delete;
	Zenith_Profiling& operator=(const Zenith_Profiling&) = delete;

	// A completed scope. Begin/End are raw monotonic CLOCK TICKS (converted to ns at
	// display via Zenith_Profiling_Detail::GetTicksToNs); m_uZoneID/m_szLabel identify
	// the zone (m_uZoneID resolves to a name via GetZoneName).
	struct Event
	{
		Event() = default;
		Event(const u_int64 uBeginTicks, const u_int64 uEndTicks, const Zenith_ProfileZoneID uZoneID, const u_int uDepth, const char* szLabel = nullptr)
			: m_uBeginTicks(uBeginTicks)
			, m_uEndTicks(uEndTicks)
			, m_uZoneID(uZoneID)
			, m_uDepth(uDepth)
			, m_szLabel(szLabel)
		{
		}
		u_int64 m_uBeginTicks = 0;
		u_int64 m_uEndTicks = 0;
		Zenith_ProfileZoneID m_uZoneID = ZENITH_PROFILE_ZONE_NULL;
		u_int m_uDepth = 0;
		const char* m_szLabel = nullptr;
	};

	static constexpr u_int uMAX_PROFILE_THREADS = 64;   // > main + 16 workers + FileWatcher + misc
	static constexpr u_int uRING_CAPACITY       = 8 * 1024;
	static constexpr u_int uMAX_PROFILE_DEPTH   = 64;
	static constexpr u_int uFRAME_HISTORY       = 256;

	// Declared in full below (it needs uMAX_ZONES). ThreadBuffer only holds a pointer.
	struct BootControlBlock;

	// One per producing thread. Producer = the owning thread (writes the ring with
	// one release-store per completed scope, NO lock). Consumer = the main thread
	// (drains via DrainPending). m_axStack is producer-private; the cursors are the
	// SPSC handshake.
	struct ThreadBuffer
	{
		struct InFlight
		{
			Zenith_ProfileZoneID m_uZoneID;
			const char* m_szLabel;
			u_int64 m_uStartTicks;
		};
		InFlight  m_axStack[uMAX_PROFILE_DEPTH];
		u_int     m_uDepth = 0;
		u_int     m_uSuppressedDepth = 0;            // release-safe nesting-overflow balance

		Event                m_axEvents[uRING_CAPACITY];
		std::atomic<u_int64> m_uWriteCursor{ 0 };    // producer publishes (release)
		std::atomic<u_int64> m_uReadCursor{ 0 };     // consumer publishes (release)
		u_int64              m_uWriteLocal = 0;       // producer-private copy of the write cursor
		std::atomic<u_int64> m_uDroppedEvents{ 0 };   // producer counts, consumer reports
		std::atomic<u_int64> m_uUnmatchedEnds{ 0 };   // producer counts, consumer reports
		u_int                m_uThreadID = ~0u;
		bool                 m_bDropWarned = false;    // consumer-owned (main thread only)
		bool                 m_bUnmatchedWarned = false;

		// Everything the ring-full routing path dereferences lives behind this ONE
		// pointer (mutex, state, buffer table, profiler back-pointer), and the block
		// outlives every ring it is stamped on — see BootControlBlock. Set once at
		// RegisterThread, never cleared, so a producer racing Shutdown reads DEAD
		// rather than a dangling profiler.
		BootControlBlock*    m_pxControl = nullptr;
	};

	// Per-thread event lists for one displayed frame, keyed by thread id. Reused
	// across frames (entries persist; inner vectors are Clear()ed — capacity kept —
	// so steady state never reallocates). Handed off by pointer swap, never copied.
	struct Snapshot
	{
		Zenith_HashMap<u_int, Zenith_Vector<Event>> m_xThreadEvents;
		u_int64 m_uBeginTicks = 0;
		u_int64 m_uEndTicks = 0;
	};

	void Initialise(Zenith_Multithreading& xThreading);
	void Shutdown();

	void RegisterThread();
	void UnregisterThread();

	void BeginFrame();
	void EndFrame();

	// String-zone API. RegisterZone interns a name (content-deduped) into a fixed,
	// reader-safe descriptor table and returns its dense id; Begin/EndProfileZone are
	// the hot path. GetZoneName resolves an id back to its name for display.
	Zenith_ProfileZoneID RegisterZone(const char* szStaticName);
	void BeginProfileZone(const Zenith_ProfileZoneID uZoneID, const char* szLabel = nullptr);
	void EndProfileZone(const Zenith_ProfileZoneID uZoneID);
	const char* GetZoneName(const Zenith_ProfileZoneID uZoneID) const;

	// Zone id of the innermost in-flight scope on the calling thread (tests/debug).
	Zenith_ProfileZoneID GetCurrentZoneID();

	// The most recently published (previous) frame, for tools/tests. Only completed
	// frames are exposed — the live frame is still accumulating.
	const Snapshot& GetDisplaySnapshot() const { return *m_pxDisplay; }

	void ClearEvents();
	void WriteTextReport(FILE* pFile);

	// ---- Boot capture API (see the Boot capture section further down) --------
	// Bracket the boot-time unit-test run: the profiler's own self-tests need an
	// un-drained ring (ProfilingOverflow) and events that survive to their own
	// EndFrame (TestProfiling), neither of which is true while ring-full routing
	// is draining into the capture.
	void SuspendBootCapture();
	void ResumeBootCapture();

	// Seal the capture at the end of Zenith_Init: stamp the cutoff, take the final
	// drain, import the platform marker bundle, then sort the raw events. Idempotent;
	// always logs the machine-readable BootSummary line (even when capture is off).
	void EndBootCapture(const Zenith_BootMarkerBundle* pxBundle = nullptr);

	// Depth-0 boot phase markers (paired by name: "<X>Begin" / "<X>End") and one-shot
	// milestones ("FirstFrameCompleted", ...). Both no-op once the capture is gone.
	void AddBootMarker(const char* szName);
	void AddBootMarker(const char* szName, const u_int64 uTicks);
	void RecordBootMilestone(const char* szName);

	// True once EndBootCapture has sealed a live capture (false when disabled).
	bool IsBootCaptureSealed() const;
	void WriteBootReport(FILE* pFile);

	// Per-zone table for the last PUBLISHED frame, under a caller-supplied title.
	// The boot artifact uses it for frame 1, which is part of the wait to first
	// present: editor-automation steps run inside the frame, before submission.
	void WriteDisplayFrameZoneTable(FILE* pFile, const char* szTitle) const;

	// Per-thread ring lookup (tests/diagnostics). The table itself lives on the
	// control block so the routing path reaches it through one pointer.
	ThreadBuffer* GetThreadBuffer(const u_int uThreadID) const;

	// ---- GPU per-pass timing channel ---------------------------------------
	// Populated by the render backend's deferred timestamp readback: one entry per
	// Flux_RenderGraph pass that ran, in execution order, with its GPU milliseconds.
	// Capture is main-thread only (BeginGPUCapture -> AddGPUPass* -> EndGPUCapture),
	// so the list is consumed (report / viz) on the same thread with no locking. A
	// frame with no readback leaves the previous capture intact (no flicker to empty).
	struct GPUPass
	{
		const char* m_szName = nullptr;     // static-lifetime pass DebugName()
		double      m_fMilliseconds = 0.0;
		u_int       m_uExecIndex = 0;       // Flux_RenderGraph execution-order index
	};
	void BeginGPUCapture();
	void AddGPUPass(const char* szName, double fMilliseconds, u_int uExecIndex);
	void EndGPUCapture();
	const Zenith_Vector<GPUPass>& GetGPUPasses() const { return m_xGPUPasses; }
	double GetGPUTotalMs() const { return m_fGPUTotalMs; }

#if ZENITH_MEMORY_TRACKING_ANY
	// ---- Memory channel ----------------------------------------------------
	// The main loop calls PushMemorySample once per frame with a POD snapshot from
	// Zenith_MemoryManagement::SampleFrame() (a pure counter read). We append it to a
	// rolling history (mirroring the frame-time ring) and keep the frozen latest sample.
	// Skipped while paused so the Memory tab freezes with the CPU/GPU timeline.
	void PushMemorySample(const Zenith_MemoryFrameSample& xSample);
	const Zenith_MemoryFrameSample& GetMemorySample() const { return m_xMemSample; }
#endif

	#ifdef ZENITH_TOOLS
	struct TimelineViewState
	{
		int m_iMinDepthToRender = 0;
		int m_iMaxDepthToRender = 10;
		int m_iMaxDepthToRenderSeparately = 3;
		float m_fTimelineZoom = 1.0f;
		float m_fTimelineScroll = 0.0f;
		float m_fVerticalScale = 1.0f;
	};

	void RenderToImGui();
	void RenderTimelineView(TimelineViewState& xState);
	void RenderThreadBreakdown(float fFrameDurationMs, u_int& uThreadID);
	#if ZENITH_MEMORY_TRACKING_ANY
	// Small always-on corner overlay (total MB + frame delta + worst category),
	// toggled by dbg_bShowMemoryHUD. Drawn from the editor's ImGui frame.
	void RenderMemoryHUD();
	#endif
	#endif

	// RAII begin/end for a dense zone id (used by the ZENITH_PROFILE_SCOPE macro).
	// Routes through the _Detail:: bridge because g_xEngine is not reachable from this
	// header. Optional runtime label for dynamic scopes.
	class ScopeZone
	{
	public:
		ScopeZone() = delete;
		ScopeZone(Zenith_ProfileZoneID uZoneID, const char* szLabel = nullptr)
			: m_uZoneID(uZoneID)
		{
#if ZENITH_PROFILING_ENABLED
			Zenith_Profiling_Detail::BeginProfileZone(uZoneID, szLabel);
#else
			(void)szLabel;
#endif
		}
		~ScopeZone()
		{
#if ZENITH_PROFILING_ENABLED
			Zenith_Profiling_Detail::EndProfileZone(m_uZoneID);
#endif
		}
	private:
		Zenith_ProfileZoneID m_uZoneID;
	};

	// Injected at Initialise(). Used only on cold paths (the hot path reaches the
	// per-thread ring via the tl_pxBuffer thread_local in the .cpp).
	Zenith_Multithreading* m_pxThreading = nullptr;

	// Four heap snapshots, swapped/copied by pointer — see Zenith_Profiling.cpp §B.
	Snapshot* m_pxAccumulator = nullptr;   // the frame currently being built
	Snapshot* m_pxDisplay     = nullptr;   // the previous (published) frame
	Snapshot* m_pxWorst       = nullptr;   // worst frame captured
	Snapshot* m_pxPinned      = nullptr;   // user-pinned frame (Phase 5)

	// Rolling history of the last uFRAME_HISTORY wall-clock frame durations (ms).
	float  m_afFrameHistoryMs[uFRAME_HISTORY]{};
	u_int  m_uFrameHistoryHead = 0;
	u_int  m_uFrameHistoryCount = 0;
	float  m_fWorstFrameMs = 0.0f;

	// GPU per-pass timings for the last read-back frame (see the GPU channel API).
	// m_xGPUPasses is published by EndGPUCapture; m_fGPUBuildingTotalMs accumulates
	// during a capture and is committed to m_fGPUTotalMs at EndGPUCapture.
	Zenith_Vector<GPUPass> m_xGPUPasses;
	double m_fGPUTotalMs = 0.0;
	double m_fGPUBuildingTotalMs = 0.0;

	// Rolling history of total GPU ms per read-back frame (mirrors the CPU frame
	// history); pushed in EndGPUCapture, plotted in the GPU viz tab.
	float  m_afGPUHistoryMs[uFRAME_HISTORY]{};
	u_int  m_uGPUHistoryHead = 0;
	u_int  m_uGPUHistoryCount = 0;
	float  m_fWorstGPUMs = 0.0f;

#if ZENITH_MEMORY_TRACKING_ANY
	// Memory history (mirrors the frame-time ring): total tracked bytes/frame + per
	// category, plus the frozen latest sample and a running peak. Main-thread only.
	float  m_afMemHistoryBytes[uFRAME_HISTORY]{};
	float  m_aafMemHistoryCat[uFRAME_HISTORY][ZENITH_MEM_CAT_MAX]{};
	u_int  m_uMemHistoryHead = 0;
	u_int  m_uMemHistoryCount = 0;
	float  m_fMemPeakBytes = 0.0f;
	Zenith_MemoryFrameSample m_xMemSample{};
#endif

	// Zone descriptor table: fixed capacity (never reallocates), published count read
	// with acquire so the UI can iterate lock-free while a worker appends a new zone.
	// Names are copied into m_acNameArena (owned), so any caller string is lifetime-safe.
	static constexpr u_int uMAX_ZONES   = 4096;
	static constexpr u_int uARENA_BYTES = 256 * 1024;
	Zenith_ProfileZoneDesc m_axZoneDescs[uMAX_ZONES];
	std::atomic<u_int>     m_uZoneCount{ 0 };
	char                   m_acNameArena[uARENA_BYTES]{};
	u_int                  m_uArenaUsed = 0;   // guarded by m_xRegistrationMutex (cold)

	// The frame-root zone ("Total Frame"), resolved once at Initialise so BeginFrame
	// needs no per-frame registration.
	Zenith_ProfileZoneID m_uTotalFrameZone = ZENITH_PROFILE_ZONE_NULL;

	// Main-thread id (captured at Initialise) so the timeline can label its lane "Main".
	u_int m_uMainThreadID = ~0u;

	// ======================= Boot capture ===================================
	// The frame profiler cannot see boot: nothing drains the 8K per-thread rings
	// until the first EndFrame, so a ring saturates and every newer event is
	// dropped. Boot capture gives the ring-full case a second destination — a
	// permanent, bounded snapshot of everything recorded before Zenith_Init
	// returns — and seals it at a cutoff tick so ordinary frame-era back-pressure
	// goes back to plain drops.

	enum BootCaptureState : u_int
	{
		BOOT_CAPTURE_DISABLED = 0,   // --skip-boot-capture: never allocated; plain drop
		BOOT_CAPTURE_ACTIVE,         // recording; a full ring drains into the capture
		BOOT_CAPTURE_SUSPENDED,      // RunAllTests window: no consumer, plain drop
		BOOT_CAPTURE_SEALED,         // boot over; only pre-cutoff stragglers still land
		BOOT_CAPTURE_DEAD,           // Shutdown ran; the profiler back-pointer is null
	};

	// Per-zone rollup. Counts EVERY boot event, including those past the raw cap,
	// so the aggregate table is complete even when the raw timeline is truncated.
	struct BootZoneAgg
	{
		u_int64 m_uCount     = 0;
		u_int64 m_uTotalTicks = 0;
		u_int64 m_uMinTicks  = ~0ull;
		u_int64 m_uMaxTicks  = 0;
	};

	// A raw boot event plus the thread that produced it. NOTE the label policy: a
	// permanent snapshot cannot hold arbitrary transient `const char*` labels, so
	// Ingest drops them and the capture keeps only stable interned zone identities.
	// (No label-bearing zone — they are render passes — runs at boot.)
	struct BootRawEvent
	{
		Event m_xEvent;
		u_int m_uThreadID = ~0u;
	};

	// Static-literal name + tick. Used for both phase markers ("<X>Begin"/"<X>End",
	// paired by name in the report) and one-shot milestones.
	struct BootMarker
	{
		const char* m_szName = nullptr;   // static literal, never owned
		u_int64     m_uTicks = 0;
	};

	// Engine-free plain struct: constructible standalone in a unit test, with an
	// injectable raw-event cap so truncation is deterministic to exercise.
	struct BootCapture
	{
		static constexpr u_int uDEFAULT_MAX_RAW_EVENTS = 512 * 1024;
		static constexpr u_int uMAX_LATE_EVENTS        = 4 * 1024;
		static constexpr u_int uMAX_MARKERS            = 64;
		static constexpr u_int uMAX_MILESTONES         = 8;

		void Configure(const u_int uMaxRawEvents, const bool bRetainRaw, const u_int64 uOriginTicks);

		// Aggregates only (sentinel-guarded). Both ingest paths funnel through it.
		void IngestAggregate(const Event& xEvent);
		// Pre-seal ingest: aggregates always, raw only while retained and under cap.
		void Ingest(const Event& xEvent, const u_int uThreadID);
		// Post-seal boot-era straggler: aggregates + the bounded pending-late list.
		void IngestLate(const Event& xEvent, const u_int uThreadID);
		// Main-thread publish step: swap pending into the immutable published list.
		void PublishLate();

		void AddMarker(const char* szName, const u_int64 uTicks);
		void SetMilestone(const char* szName, const u_int64 uTicks);   // first-wins
		const BootMarker* FindMilestone(const char* szName) const;

		// Drop accounting is reported as deltas over NON-SUSPENDED intervals only.
		void FoldDropDelta(const u_int uThreadID, const u_int64 uCurrentDrops);
		void RebaselineDrops(const u_int uThreadID, const u_int64 uCurrentDrops);
		u_int64 GetTotalDrops() const;

		void Seal(const u_int64 uCutoffTicks);

		u_int   m_uMaxRawEvents = uDEFAULT_MAX_RAW_EVENTS;
		bool    m_bRetainRaw    = false;
		bool    m_bSealed       = false;
		u_int64 m_uOriginTicks  = 0;
		u_int64 m_uCutoffTicks  = 0;
		double  m_fSealOverheadMs = 0.0;

		BootZoneAgg m_axZoneAggs[uMAX_ZONES];
		u_int64 m_uTotalEvents         = 0;
		u_int64 m_uTruncatedRawEvents  = 0;
		u_int64 m_uUnattributedEvents  = 0;   // sentinel / overflow zone ids
		u_int64 m_uLateEvents          = 0;   // post-seal boot-era arrivals (counted always)
		u_int64 m_uLateDropped         = 0;   // late arrivals past uMAX_LATE_EVENTS

		Zenith_Vector<BootRawEvent> m_xRawEvents;       // sorted by begin at Seal
		Zenith_Vector<BootRawEvent> m_xLatePending;     // written under the control mutex
		Zenith_Vector<BootRawEvent> m_xLatePublished;   // immutable once published

		BootMarker m_axMarkers[uMAX_MARKERS];
		u_int      m_uMarkerCount = 0;
		BootMarker m_axMilestones[uMAX_MILESTONES];
		u_int      m_uMilestoneCount = 0;

		u_int64 m_aulDropBaseline[uMAX_PROFILE_THREADS]{};
		u_int64 m_aulDropAccum[uMAX_PROFILE_THREADS]{};
	};

	// Heap-allocated in the Zenith_Profiling CONSTRUCTOR (so it exists before the
	// main thread's RegisterThread, which runs before Initialise) and freed by
	// Shutdown only when every ring was freed with it — otherwise deliberately
	// leaked alongside the surviving rings, extending the documented ring-leak
	// policy. That is what makes the routing path lifetime-safe: every pointer it
	// dereferences (mutex, state, table, profiler) lives HERE, and Shutdown sets
	// DEAD + nulls m_pxProfiling under m_xMutex before the profiler is deleted, so
	// a producer that loses the race sees DEAD and falls back to a plain drop.
	struct BootControlBlock
	{
		// Registration (threads + zones), every drain, and boot routing.
		Zenith_Mutex_NoProfiling m_xMutex;

		// Thread-buffer table: fixed capacity, published with release at
		// RegisterThread, read with acquire at drain. Never reallocates, so the
		// consumer can iterate it concurrently with a late registration.
		std::atomic<ThreadBuffer*> m_apxThreadBuffers[uMAX_PROFILE_THREADS]{};

		std::atomic<u_int> m_uState{ BOOT_CAPTURE_DISABLED };   // BootCaptureState

		// Boot/frame divide. m_uCutoffTicks is written under m_xMutex at seal but read
		// by producers OUTSIDE it (the ring-full fast pre-check needs it to tell a
		// post-seal boot straggler from ordinary frame-era back-pressure), hence
		// atomic. m_uFirstFrameOriginTicks is the IMMUTABLE first-production-frame
		// boundary: post-seal frame drains route only events beginning before it into
		// the capture, so an ordinary cross-frame scope at frame N is never
		// boot-attributed.
		std::atomic<u_int64> m_uCutoffTicks{ 0 };
		std::atomic<u_int64> m_uFirstFrameOriginTicks{ 0 };
		u_int64              m_uInitTicks = 0;   // fallback origin when no bundle arrives

		Zenith_Profiling* m_pxProfiling = nullptr;   // nulled under m_xMutex at Shutdown
	};

	BootControlBlock* GetControlBlock() const { return m_pxControl; }
	BootCapture*      GetBootCapture() const { return m_pxBootCapture; }

	BootControlBlock* m_pxControl = nullptr;      // allocated in the ctor, see above
	BootCapture*      m_pxBootCapture = nullptr;  // owned; null while DISABLED/DEAD

	bool m_bInitialised = false;
};

// Uses the bridge forwarders so callers do not need Zenith_Engine.h. When profiling
// is compiled out the wrapper still invokes the wrapped call (args evaluated, fn run)
// but adds no begin/end — zero overhead.
#if ZENITH_PROFILING_ENABLED
#define ZENITH_PROFILING_FUNCTION_WRAPPER(x, uZone, ...) \
	Zenith_Profiling_Detail::BeginProfileZone(uZone, nullptr); \
	x(__VA_ARGS__); \
	Zenith_Profiling_Detail::EndProfileZone(uZone);
#else
#define ZENITH_PROFILING_FUNCTION_WRAPPER(x, uZone, ...) x(__VA_ARGS__)
#endif

// String-literal profiling, usable ANYWHERE without editing a central list. The
// static-local registers the zone exactly once (C++11 thread-safe static init); the
// __LINE__ token-paste makes the locals unique per use site. The hot path then stores
// only the cached dense id.
#define ZENITH_PP_CAT_(a, b) a##b
#define ZENITH_PP_CAT(a, b)  ZENITH_PP_CAT_(a, b)

#if ZENITH_PROFILING_ENABLED
	// Scoped zone: ZENITH_PROFILE_SCOPE("My Zone");
	#define ZENITH_PROFILE_SCOPE(szName) \
		static const Zenith_ProfileZoneID ZENITH_PP_CAT(znProfZone_, __LINE__) = \
			Zenith_Profiling_Detail::RegisterZone(szName); \
		Zenith_Profiling::ScopeZone ZENITH_PP_CAT(znProfScope_, __LINE__)( \
			ZENITH_PP_CAT(znProfZone_, __LINE__))

	// Scoped zone with a runtime label under a static category. COLD: the category id
	// is cached (static-local) but the label is passed through to the timeline as-is;
	// for per-frame hot labels (e.g. render passes) cache the id on the object instead.
	#define ZENITH_PROFILE_SCOPE_DYNAMIC(szCategory, szLabel) \
		static const Zenith_ProfileZoneID ZENITH_PP_CAT(znProfZone_, __LINE__) = \
			Zenith_Profiling_Detail::RegisterZone(szCategory); \
		Zenith_Profiling::ScopeZone ZENITH_PP_CAT(znProfScope_, __LINE__)( \
			ZENITH_PP_CAT(znProfZone_, __LINE__), (szLabel))

	// Value form: returns a cached dense id, for APIs that take an id by value (tasks).
	#define ZENITH_PROFILE_ZONE(szName) \
		([]() -> Zenith_ProfileZoneID { \
			static const Zenith_ProfileZoneID s_uZoneId = Zenith_Profiling_Detail::RegisterZone(szName); \
			return s_uZoneId; }())
#else
	#define ZENITH_PROFILE_SCOPE(szName)                 ((void)0)
	#define ZENITH_PROFILE_SCOPE_DYNAMIC(szCat, szLabel) ((void)0)
	#define ZENITH_PROFILE_ZONE(szName)                  (ZENITH_PROFILE_ZONE_NULL)
#endif
