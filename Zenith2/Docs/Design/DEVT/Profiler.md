# DEVT design: the ZenithProfiling capture engine

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It specifies the public headers `Zenith_ProfilerTypes.h`, `Zenith_Profiler.h`, `Zenith_ProfileZone.h` and `Zenith_ProfilerCapture.h`, the hook implementation behind CORE's `Zenith_ProfilerHooks`, the event, ring, name-table, counter, lane, lock-statistics, frame, GPU and capture structures with their byte sizes, Tracy forwarding, memory and the cost model.

## Scope of this file

| Topic | Fixed elsewhere | Specified here |
|---|---|---|
| the hook table, its installation and the thread-local allowlist | `Zenith2/Docs/Contracts.md` → "No mutable global state" | the six callback implementations, `FillHooks`, the thread slot |
| counter kinds and the `_delta` naming rule | `Zenith2/Docs/Contracts.md` → "No mutable global state" ([Z2-N-CONTRACT-155], [Z2-N-CONTRACT-156]) | how each kind is aggregated per frame |
| zones per node, sub-job and continuation | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Profiling" | lanes, frame assembly, the timeline data |
| GPU timestamps and their attribution | `Zenith2/Docs/Rendering/RenderGraph.md` → "GPU timestamps" | the GPU timing sink `Zenith_ProfilerGpuSink`, `SubmitGpuTimings` and the GPU lanes |
| the single-producer single-consumer ring | CORE's `Zenith_SpscRing<T>` (ruling Q80; its one specification is the AU6 block of `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas", pending CORE, row DV23) | how the profiler sizes, fills and drains one ring per thread |
| the frame loop and startup steps | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", "The frame loop" | what the Player and the runtime call, in which step |
| the profiled and unprofiled lock primitives and the hook dispatch functions | CORE: `Zenith2/Docs/Design/CORE/Threading.md` (the `Zenith_Mutex` / `Zenith_MutexNoProfiling` pairs), `Zenith2/Docs/Contracts.md` → "No mutable global state" (`Zenith_DispatchZoneBegin` and its siblings in `Zenith_ProfilerHooks.h`) | which primitives the profiler itself uses (the unprofiled variants alone), what its lock hooks record, the per-site lock statistics |

## Profiler public headers

| Header | Includes | Stage of the API-only slice |
|---|---|---|
| `Zenith_ProfilerTypes.h` | `ZenithCore/Zenith_Types.h`, `ZenithCore/Zenith_Result.h` | Stage 1 sprint, fix version `Z2 Stage 2` |
| `Zenith_Profiler.h` | `Zenith_ProfilerTypes.h`, `ZenithCore/Zenith_BuildInfo.h` (for `Zenith_GetPlayerProfilerDesc`), `ZenithCore/Zenith_ProfilerHooks.h`, `ZenithCore/Zenith_Owned.h`, `ZenithCore/Zenith_Span.h`, `ZenithCore/Zenith_StringView.h`, `ZenithCore/Zenith_Heap.h`, `ZenithCore/Zenith_MemoryGovernor.h` (for `Zenith_MemoryConsumer`) | as above |
| `Zenith_ProfileZone.h` | `ZenithCore/Zenith_ProfilerHooks.h`, `ZenithCore/Zenith_StringView.h` | as above |
| `Zenith_ProfilerCapture.h` | `Zenith_ProfilerTypes.h`, `ZenithCore/Zenith_Span.h`, `ZenithCore/Zenith_Result.h` | Stage 2 |

The API-only slice declares every type and member below with its contract block and gives every member body a `ZENITH_UNREACHABLE()` stub in `Internal/` only where the implementing ticket of the same stage fills it; consumers compile and link against the declarations from Stage 1 (`class Zenith_Profiler;` is enough for ZenithJobs). The slice includes CORE headers from four CORE API-only tickets, which block it: Z2T-CORE-001 (`Zenith_Types.h` to `Zenith_ProfilerHooks.h`: `Zenith_Result.h`, `Zenith_Span.h`, `Zenith_StringView.h`, `Zenith_Owned.h`, `Zenith_Heap.h`), Z2T-CORE-014 (`Zenith_ConfigStore.h`, which `Zenith_DevConsole.h` and `Zenith_DevToolsConfig.h` include), Z2T-CORE-015 (`Zenith_MemoryGovernor.h`, for `Zenith_MemoryConsumer`) and Z2T-CORE-007 (`Zenith_Vector.h`, `Zenith_Quaternion.h`, `Zenith_Matrix.h`, which `Zenith_DebugDraw.h` includes; `Zenith2/Docs/Design/DEVT.md` → "Stage 1").

## Zenith_ProfilerTypes header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_ProfilerTypes.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Types.h"

/// @contract value Zenith_ProfilerLaneID
/// Ownership: Plain value issued by Zenith_Profiler::RegisterThreadLane or by the automatic lane of a thread's first event.
/// Thread affinity: IMMUTABLE; members non-const: owners store it in a member assigned after registration (JOBS's lane
///   field).
/// Invalidation: Valid for the issuing profiler's lifetime; meaningless in another profiler.
/// Units: None.
/// Errors: The value 0 is the null lane; RegisterThreadLane never returns it on success.
/// Version: since Z2 Stage 2. Never serialized except inside a capture, where it is the capture's lane index.
/// @endcontract
struct Zenith_ProfilerLaneID
{
	uint16 m_uValue = 0;
	constexpr bool IsNull() const { return m_uValue == 0; }
	friend constexpr bool operator==(Zenith_ProfilerLaneID, Zenith_ProfilerLaneID) = default;
};

/// @contract value Zenith_ProfilerSiteID
/// Ownership: Plain value; an index into the profiler's site table.
/// Thread affinity: IMMUTABLE; members non-const: a member of Zenith_ProfilerZoneStat, which the profiler assigns in place.
/// Invalidation: Valid for the issuing profiler's lifetime.
/// Units: None.
/// Errors: 0 is "<names full>" or "<unknown>"; a real site is 1 … uPROFILER_MAX_SITES − 1.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerSiteID
{
	uint32 m_uValue = 0;
	friend constexpr bool operator==(Zenith_ProfilerSiteID, Zenith_ProfilerSiteID) = default;
};

/// @contract value Zenith_ProfilerCounterID
/// Ownership: Plain value; an index into the profiler's counter table.
/// Thread affinity: IMMUTABLE; members non-const: a member of Zenith_ProfilerCounterView, which the profiler's snapshot
///   assigns in place.
/// Invalidation: Valid for the issuing profiler's lifetime.
/// Units: None.
/// Errors: 0 is "<counters full>".
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerCounterID
{
	uint32 m_uValue = 0;
	friend constexpr bool operator==(Zenith_ProfilerCounterID, Zenith_ProfilerCounterID) = default;
};

/// @contract enum Zenith_ProfilerMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_ProfilerMode : uint8
{
	PROFILER_MODE_COUNTERS_ONLY = 1,   // CPU zones, values, locks and waits are dropped at the first branch; counters aggregate;
	                                   // GPU timings follow Zenith_ProfilerSettings::m_bGpuTimings, never the mode ("Runtime toggle and modes")
	PROFILER_MODE_CAPTURE,             // every event is recorded
};

/// @contract enum Zenith_ProfilerLaneKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Values are written into captures; never renumbered.
/// @endcontract
enum Zenith_ProfilerLaneKind : uint8
{
	PROFILER_LANE_KIND_CPU_THREAD = 1,
	PROFILER_LANE_KIND_GPU_QUEUE = 2,
};

/// @contract enum Zenith_ProfilerUnit
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: The enumerators name units.
/// Errors: None.
/// Version: since Z2 Stage 2. Values are written into captures and panel tables; never renumbered.
/// @endcontract
enum Zenith_ProfilerUnit : uint8
{
	PROFILER_UNIT_COUNT = 1, PROFILER_UNIT_BYTES, PROFILER_UNIT_NANOSECONDS, PROFILER_UNIT_MICROSECONDS,
	PROFILER_UNIT_MILLISECONDS, PROFILER_UNIT_PERCENT, PROFILER_UNIT_RATIO, PROFILER_UNIT_PER_SECOND,
};

/// @contract enum Zenith_ProfilerError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The enumerators of Zenith2/Docs/Design/DEVT.md "Error enumerations".
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_ProfilerError : uint8
{
	PROFILER_ERROR_INVALID_DESC = 1, PROFILER_ERROR_OUT_OF_MEMORY, PROFILER_ERROR_THREAD_CREATE, PROFILER_ERROR_LANES_FULL,
	PROFILER_ERROR_CAPTURE_ACTIVE, PROFILER_ERROR_NO_OUTPUT, PROFILER_ERROR_GPU_QUEUE_FULL, PROFILER_ERROR_CAPTURE_CORRUPT,
	PROFILER_ERROR_CAPTURE_VERSION, PROFILER_ERROR_EXPRESSION, PROFILER_ERROR_NOT_IN_SHIPPING,
};
constexpr const char* Zenith_ToString(Zenith_ProfilerError eError);
constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_ProfilerError eError);

inline constexpr uint32 uPROFILER_MAX_THREADS = 48;              // thread states preallocated
inline constexpr uint32 uPROFILER_RING_RECORDS = 16384;          // per thread; 16 bytes each, 256 KiB
inline constexpr uint32 uPROFILER_MAX_SITES = 16384;
inline constexpr uint32 uPROFILER_MAX_COUNTERS = 4096;
inline constexpr uint32 uPROFILER_MAX_LANES = 64;                // CPU lanes 1 … 56, GPU lanes 57 … 63, the overflow CPU lane 64
inline constexpr uint32 uPROFILER_MAX_NAME_BYTES = 63;           // longer names are cut and end in '~'; stored with a NUL
// Shared by sites, counters and lanes, sized for the worst case: every entry of the three tables holding a 63-byte name and
// its terminating NUL fits exactly ((16,384 + 4,096 + 64) × 64 = 1,314,816 = 20,544 × (63 + 1)), so an insert never finds
// the arena exhausted ("Name tables").
inline constexpr uint32 uPROFILER_NAME_ARENA_BYTES = (uPROFILER_MAX_SITES + uPROFILER_MAX_COUNTERS + uPROFILER_MAX_LANES) * 64;
inline constexpr uint32 uPROFILER_MAX_ZONE_DEPTH = 64;
inline constexpr uint32 uPROFILER_HISTORY_FRAMES = 600;          // frame summaries (10 s at 60 Hz)
inline constexpr uint32 uPROFILER_DETAIL_FRAMES = 8;             // frames whose zones are kept
inline constexpr uint32 uPROFILER_DETAIL_ZONES_PER_FRAME = 32768;   // 1 MiB of 32-byte zones per detailed frame
inline constexpr uint32 uPROFILER_THREAD_STATE_BYTES = 8192;         // per thread state beside its ring ("Memory layout and accounting")
inline constexpr uint32 uPROFILER_LOCK_SITES_PER_THREAD = 64;        // entries of one per-thread lock-site table, of which at most 48 are filled ("Lock statistics")
inline constexpr uint32 uPROFILER_HELD_LOCKS_PER_THREAD = 16;        // the per-thread held-lock stack (CORE's uLOCK_ORDER_MAX_HELD)
inline constexpr uint32 uPROFILER_MAX_LOCK_SITES = 512;              // lock sites in the frame's merged lock statistics
inline constexpr uint32 uPROFILER_LOCK_STATE_BYTES = 8896;           // per thread state beside its ring and uPROFILER_THREAD_STATE_BYTES: two lock banks and the held-lock stack
inline constexpr uint32 uPROFILER_COUNTER_HISTORY_FRAMES = 128;
inline constexpr uint32 uPROFILER_GPU_QUEUE_FRAMES = 8;
inline constexpr uint32 uPROFILER_GPU_PASSES_PER_FRAME = 4096;
inline constexpr uint32 uPROFILER_CAPTURE_SEGMENT_BYTES = 16u << 20;
```

## Zenith_Profiler header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_Profiler.h
#pragma once
#include "ZenithCore/Zenith_BuildInfo.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_MemoryGovernor.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_ProfilerHooks.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithProfiling/Zenith_ProfilerTypes.h"

class Zenith_FaultInjector;

/// @contract interface Zenith_ProfilerOutput
/// Ownership: Implemented by ZenithRuntime (Zenith_DevToolsOutput, over Zenith_AsyncIO::WriteAtomic) and by tests
///   (Zenith_TestProfilerOutput); set on the profiler with SetOutput and cleared before the implementer is destroyed.
/// Thread affinity: ANY_THREAD. WriteFile is called on Zenith.ProfFlush and on the main thread; it never blocks.
/// Invalidation: xBytes is valid for the duration of the call; the implementer copies what it keeps.
/// Units: Bytes.
/// Errors: WriteFile returns false when the write could not be submitted (busy, refused path, I/O error); the caller
///   counts the refusal and never retries the same bytes.
/// Reentrancy: NONE. An implementation never calls a profiler member from WriteFile.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_ProfilerOutput
{
public:
	virtual ~Zenith_ProfilerOutput() = default;
	// strRelativeName: a path below user:/devtools/ such as "captures/<id>/part-0003.zprof" (Console.md rules for names).
	virtual bool WriteFile(Zenith_StringView strRelativeName, Zenith_ByteSpan xBytes) = 0;
};

/// @contract value Zenith_ProfilerDesc
/// Ownership: Plain value copied by Create.
/// Thread affinity: IMMUTABLE; members non-const: tests and Zenith_MakeStressProfilerDesc patch fields of the
///   Zenith_GetPlayerProfilerDesc value.
/// Invalidation: None.
/// Units: As the constants of Zenith_ProfilerTypes.h; uThreadStates counts thread states.
/// Errors: Create returns PROFILER_ERROR_INVALID_DESC for a field outside the range in its comment.
/// Version: since Z2 Stage 2. New fields are appended with defaults.
/// @endcontract
struct Zenith_ProfilerDesc
{
	Zenith_ProfilerMode m_eInitialMode = PROFILER_MODE_CAPTURE;   // COUNTERS_ONLY in Shipping (Zenith_GetPlayerProfilerDesc)
	uint32 m_uThreadStates = uPROFILER_MAX_THREADS;              // 4 … 128
	uint32 m_uRingRecords = uPROFILER_RING_RECORDS;              // power of two, 1024 … 65536
	uint32 m_uHistoryFrames = uPROFILER_HISTORY_FRAMES;          // 60 … 3600
	uint32 m_uDetailFrames = uPROFILER_DETAIL_FRAMES;            // 2 … 64
	bool m_bTracyAvailable = true;                               // false in Shipping and sanitizer builds (Zenith_GetPlayerProfilerDesc)
	bool m_bStartFlushThread = true;                              // false only in tests that flush synchronously
	Zenith_Heap* m_pxHeap = nullptr;                              // nullptr = the root heap
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;            // tests; the fault points of "Creation and shutdown"
};

/// @contract value Zenith_ProfilerSettings
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the profiler stores the clamped value and replaces it on every
///   ApplySettings.
/// Invalidation: None.
/// Units: m_fHitchThresholdMs milliseconds.
/// Errors: ApplySettings clamps out-of-range values and logs once.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerSettings
{
	bool m_bCaptureEnabled = true;          // devtools.profiler.enabled
	bool m_bCaptureLocks = false;           // devtools.profiler.captureLocks: lock and wait events and the per-site lock statistics; off by default, settable from the user layer, the console and the overlay's [locks] toggle
	bool m_bTracyEnabled = false;           // devtools.tracy.enabled (ignored when the desc made Tracy unavailable)
	bool m_bGpuTimings = true;              // devtools.profiler.gpuTimings: GPU submissions accepted in either mode
	float m_fHitchThresholdMs = 33.3f;      // devtools.profiler.hitchThresholdMs
};

/// @contract value Zenith_ProfilerGpuPass
/// Ownership: A view built by Flux_PassTimingPublisher for one call; strName views the slot-owned pass-name pool.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after SubmitGpuTimings returns; the profiler copies the name.
/// Units: Nanoseconds on the CPU monotonic clock when the submission is calibrated, else nanoseconds from the frame's
///   first GPU timestamp.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerGpuPass
{
	Zenith_StringView m_strName;   // the pass name with its view suffix, e.g. "GTAO[Main]"
	uint64 m_ulStartNs = 0;
	uint64 m_ulEndNs = 0;
};

/// @contract value Zenith_ProfilerGpuSubmission
/// Ownership: A view built by the publisher for one call.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after SubmitGpuTimings returns.
/// Units: As Zenith_ProfilerGpuPass.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerGpuSubmission
{
	uint64 m_ulFrameIndex = 0;          // the frame the timestamps belong to
	uint32 m_uQueue = 0;                // the RHI queue index (0 graphics, 1 async compute, 2 copy)
	bool m_bCalibrated = false;          // Zenith_RHIClockCalibration was valid
	Zenith_Span<const Zenith_ProfilerGpuPass> m_axPasses;   // at most uPROFILER_GPU_PASSES_PER_FRAME; more are cut
};

/// @contract interface Zenith_ProfilerGpuSink
/// Ownership: Implemented by Zenith_Profiler and by tests (a capturing sink); registered with ZenithFlux's frame lifetime by
///   Flux_Renderer::Create (Flux_FrameLifetime::SetGpuTimingSink, row DV5) and cleared before the implementer is destroyed.
/// Thread affinity: ANY_THREAD, one caller at a time: Flux_PassTimingPublisher::Publish calls it on the thread running RHI's
///   PrepareNextFrame step 4 (the render-submit thread or the main thread) and from a frames-in-flight change (main thread),
///   never concurrently.
/// Invalidation: xSubmission and every view in it are valid for the duration of the call; the implementer copies what it keeps.
/// Units: As Zenith_ProfilerGpuPass.
/// Errors: SubmitGpuTimings returns PROFILER_ERROR_GPU_QUEUE_FULL when it dropped the submission (counted); the publisher
///   ignores the result beyond that count.
/// Reentrancy: NONE. An implementation never calls back into Flux.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_ProfilerGpuSink
{
public:
	virtual ~Zenith_ProfilerGpuSink() = default;
	virtual Zenith_Result<void, Zenith_ProfilerError> SubmitGpuTimings(const Zenith_ProfilerGpuSubmission& xSubmission) = 0;
};

/// @contract value Zenith_ProfilerFrameInputs
/// Ownership: Plain value filled by the composition root at frame-loop step 3.
/// Thread affinity: IMMUTABLE; members non-const: the composition root fills it field by field at frame-loop step 3.
/// Invalidation: None.
/// Units: Nanoseconds of Zenith_GetMonotonicTimeNs; scales in 1/10000 (10000 = 100%).
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerFrameInputs
{
	uint64 m_ulFrameIndex = 0;
	uint32 m_uTickCount = 0;
	uint64 m_ulIterationStartNs = 0;    // the four stamps of GRAPH's Zenith_FrameTimeline
	uint64 m_ulSimulationEndNs = 0;
	uint64 m_ulRenderStartNs = 0;
	uint64 m_ulRenderEndNs = 0;
	uint16 m_uScaleX = 10000;           // the main view's dynamic-resolution scale
	uint16 m_uScaleY = 10000;
	bool m_bExcludedFromMeasurement = false;   // the budget run's initial-load window
};

/// @contract value Zenith_ProfilerFrameSummary
/// Ownership: Plain value stored in the history ring; copied out by GetFrameSummary.
/// Thread affinity: IMMUTABLE; members non-const: assembly step 8 builds it field by field and assigns it into the
///   history ring.
/// Invalidation: None.
/// Units: Microseconds unless the member says otherwise; scales in 1/10000.
/// Errors: None.
/// Version: since Z2 Stage 2. The 64-byte layout is written into captures (format version 1); bytes 58 to 63 carry the two lock fields.
/// @endcontract
struct Zenith_ProfilerFrameSummary
{
	uint64 m_ulFrameIndex = 0;          // offset 0
	uint64 m_ulStartNs = 0;             // 8: the previous EndFrame call
	uint32 m_uFrameUs = 0;              // 16: EndFrame(k-1) to EndFrame(k)
	uint32 m_uPresentIntervalUs = 0;    // 20: Flux.Pacing.IntervalUs when published, else 0
	uint32 m_uCpuSimUs = 0;             // 24: iteration start to simulation end
	uint32 m_uCpuRenderUs = 0;          // 28: render start to render end
	uint32 m_uGpuUs = 0;                // 32: the graphics queue's span of the GPU frame resolved this frame
	uint32 m_uGpuFrameLag = 0;          // 36: this frame index minus the GPU frame's index
	uint32 m_uTickCount = 0;            // 40
	uint32 m_uZoneCount = 0;            // 44
	uint32 m_uDroppedEvents = 0;        // 48
	uint16 m_uFlags = 0;                // 52: PROFILER_FRAME_FLAG_*
	uint16 m_uScaleX = 10000;           // 54
	uint16 m_uScaleY = 10000;           // 56
	uint16 m_uLockContended = 0;        // 58: contended lock acquisitions of the frame, all threads, saturating; 0 while lock capture is off
	uint32 m_uLockWaitUs = 0;           // 60: the wait of those acquisitions summed, microseconds, saturating; 0 while lock capture is off
};
static_assert(sizeof(Zenith_ProfilerFrameSummary) == 64);

inline constexpr uint16 uPROFILER_FRAME_FLAG_HITCH = 1u << 0;        // m_uFrameUs above the hitch threshold
inline constexpr uint16 uPROFILER_FRAME_FLAG_GPU_MISSING = 1u << 1;  // no GPU frame resolved this frame
inline constexpr uint16 uPROFILER_FRAME_FLAG_TRUNCATED = 1u << 2;    // events were dropped or zones repaired
inline constexpr uint16 uPROFILER_FRAME_FLAG_EXCLUDED = 1u << 3;     // m_bExcludedFromMeasurement
inline constexpr uint16 uPROFILER_FRAME_FLAG_CAPTURE_OFF = 1u << 4;  // capture was disabled for the frame

/// @contract value Zenith_ProfilerZone
/// Ownership: Plain value in a detailed-frame arena; read through Zenith_ProfilerFrameView.
/// Thread affinity: IMMUTABLE; members non-const: assembly emits it into the detailed-frame arena and sorts the arena
///   in place.
/// Invalidation: With its frame view.
/// Units: Nanoseconds of the CPU monotonic clock (GPU zones: as their submission).
/// Errors: None.
/// Version: since Z2 Stage 2. The 32-byte layout is written into captures.
/// @endcontract
struct Zenith_ProfilerZone
{
	uint64 m_ulStartNs = 0;    // 0
	uint64 m_ulEndNs = 0;      // 8
	uint64 m_ulValue = 0;      // 16: the first value attached, else 0
	uint32 m_uSite = 0;        // 24: Zenith_ProfilerSiteID value
	uint16 m_uLane = 0;        // 28: Zenith_ProfilerLaneID value
	uint8 m_uDepth = 0;        // 30: 0 for a lane's outermost zone
	uint8 m_uFlags = 0;        // 31: PROFILER_ZONE_FLAG_*
};
static_assert(sizeof(Zenith_ProfilerZone) == 32);

inline constexpr uint8 uPROFILER_ZONE_FLAG_HAS_VALUE = 1u << 0;
inline constexpr uint8 uPROFILER_ZONE_FLAG_TRUNCATED_START = 1u << 1;   // begun in an earlier frame (carried)
inline constexpr uint8 uPROFILER_ZONE_FLAG_TRUNCATED_END = 1u << 2;     // end lost; closed by repair
inline constexpr uint8 uPROFILER_ZONE_FLAG_GPU = 1u << 3;
inline constexpr uint8 uPROFILER_ZONE_FLAG_MULTIPLE_VALUES = 1u << 4;   // values beyond the first were attached

/// @contract value Zenith_ProfilerFrameView
/// Ownership: A view into the profiler's detailed-frame ring.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Invalid after the EndFrame that recycles the frame's slot (m_uDetailFrames frames afterwards), or at once when
///   the frame is not held by Freeze; callers copy what they keep beyond the current frame.
/// Units: As Zenith_ProfilerZone.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerFrameView
{
	Zenith_ProfilerFrameSummary m_xSummary;
	Zenith_Span<const Zenith_ProfilerZone> m_axZones;     // ordered by (lane, start, depth)
	Zenith_Span<const Zenith_ProfilerZone> m_axGpuZones;  // the GPU frame resolved this frame, ordered by (lane, start)
};

/// @contract value Zenith_ProfilerCounterView
/// Ownership: A view into the counter table.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: m_strName lives for the profiler; values are a snapshot of the last EndFrame.
/// Units: The counter's own; the profiler does not interpret them.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerCounterView
{
	Zenith_ProfilerCounterID m_xID;
	Zenith_StringView m_strName;
	Zenith_CounterKind m_eKind = COUNTER_KIND_CUMULATIVE;
	sint64 m_iFrameValue = 0;    // DELTA: the frame's sum; CUMULATIVE: change since the previous frame; GAUGE: latest
	sint64 m_iLatest = 0;        // CUMULATIVE: the running total; GAUGE: latest; DELTA: the sum since start
	sint64 m_iHistoryMin = 0;    // over the last uPROFILER_COUNTER_HISTORY_FRAMES frame values
	sint64 m_iHistoryMax = 0;
	double m_dHistoryMean = 0.0;
};

/// @contract value Zenith_ProfilerZoneStat
/// Ownership: Plain value; one entry per site in the profiler's frame statistics, read through GetZoneStats.
/// Thread affinity: IMMUTABLE; members non-const: the profiler accumulates every closure of the site into its entry in
///   place, resets the entries once per frame and sorts them by site ("Zone statistics").
/// Invalidation: None.
/// Units: Nanoseconds.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerZoneStat
{
	Zenith_ProfilerSiteID m_xSite;
	uint32 m_uCount = 0;        // every closure of the site in the frame: loops, call paths, self-nesting and threads included
	uint64 m_ulTotalNs = 0;     // inclusive time summed over the site's closures and threads; a closure begun inside an open
	                            // zone of the same site on the same thread adds nothing (no nanosecond is counted twice)
	uint64 m_ulMaxNs = 0;       // the longest single closure (a recursive site's outermost one)
};

/// @contract value Zenith_ProfilerLockStat
/// Ownership: Plain value; one entry per lock site in the profiler's frame lock statistics, read through GetLockStats.
/// Thread affinity: IMMUTABLE; members non-const: the end-of-frame merge accumulates every thread's lock-site table into
///   the entry in place, resets the entries once per frame and sorts them by site ("Lock statistics").
/// Invalidation: None.
/// Units: Nanoseconds.
/// Errors: None.
/// Version: since Z2 Stage 2. The 48-byte layout is written into captures (chunk LOCKS).
/// @endcontract
struct Zenith_ProfilerLockStat
{
	Zenith_ProfilerSiteID m_xSite;     // 0: the site "lock:<name>" (GetSiteName gives that text); one lock site per lock name
	uint16 m_uLevel = 0;               // 4: the level of the lock's Zenith_LockDesc
	uint16 m_uReserved = 0;            // 6
	uint32 m_uAcquisitions = 0;        // 8: every acquire of the site in the frame, contended or not, on every thread and mutex of the name
	uint32 m_uContended = 0;           // 12: the acquires that waited (a measured wait above 0)
	uint64 m_ulTotalWaitNs = 0;        // 16: the wait summed over the contended acquires
	uint64 m_ulMaxWaitNs = 0;          // 24: the longest single wait
	uint64 m_ulTotalHoldNs = 0;        // 32: the hold time summed over the releases of the frame
	uint64 m_ulMaxHoldNs = 0;          // 40: the longest single hold
};
static_assert(sizeof(Zenith_ProfilerLockStat) == 48);

/// @contract value Zenith_ProfilerLaneInfo
/// Ownership: A view into the lane table.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: m_strName lives for the profiler.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerLaneInfo
{
	Zenith_ProfilerLaneID m_xID;
	Zenith_ProfilerLaneKind m_eKind = PROFILER_LANE_KIND_CPU_THREAD;
	Zenith_StringView m_strName;
	uint64 m_ulOsThreadID = 0;       // 0 for GPU lanes and for a registered lane no thread bound yet
	uint32 m_uRegisteredIndex = 0;   // the uLaneIndex passed to RegisterThreadLane, 0xFFFFFFFF for automatic lanes
};

/// @contract type Zenith_Profiler
/// Ownership: Created by Create as a Zenith_Owned<Zenith_Profiler> on the Player's (or editor's, or test's) main
///   stack frame at startup step 2, before Zenith_InstallProfilerHooks; destroyed after Zenith_InstallProfilerHooks(nullptr)
///   (shutdown row 11). Every buffer is allocated by Create from m_pxHeap.
/// Thread affinity: ANY_THREAD_EXCEPT(Create, ApplySettings, SetMode, SetOutput, RegisterThreadLane, EndFrame, Freeze,
///   StartCapture, StopCapture, GetLanes, GetFrameSummary, GetFrame, GetCounters, FindCounter, GetZoneStats, GetLockStats,
///   GetSiteName, destruction). Those run on the main thread. BindCurrentThread runs on the thread it binds. SubmitGpuTimings runs on
///   any thread, one caller at a time (Flux's publisher through the Zenith_ProfilerGpuSink base). The hook functions
///   FillHooks installs run on every thread.
/// Invalidation: Views returned by Get* are valid until the next EndFrame unless the member says otherwise.
/// Units: Nanoseconds of Zenith_GetMonotonicTimeNs.
/// Errors: Create returns Zenith_Result<…, Zenith_ProfilerError>; RegisterThreadLane returns the null lane when the lane
///   table is full (counted in profiler.lanes_refused, logged once); the other members' errors are their declarations'.
///   A second profiler owning thread slots while the first is installed is legal; only the installed one receives events.
/// Reentrancy: NONE. Zenith_ProfilerOutput::WriteFile never calls back.
/// Locks: uLOCK_LEVEL_PROFILER_BUFFERS (940) around the capture-segment queue alone, held through a Zenith_MutexNoProfiling:
///   the profiler uses the unprofiled primitives only, so none of its locks can report to its own hooks ("Lock and wait events").
/// Version: since Z2 Stage 2. Source API.
/// Example: Zenith2/Engine/ZenithProfiling/Tests/Examples/Zenith_Profiler_Example.cpp
/// @endcontract
class Zenith_Profiler final : public Zenith_MemoryConsumer, public Zenith_ProfilerGpuSink
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Profiler>, Zenith_ProfilerError> Create(const Zenith_ProfilerDesc& xDesc);
	~Zenith_Profiler() override;

	void FillHooks(Zenith_ProfilerHooks& xHooks);   // writes the six profiler members and m_pUserData (in Shipping the lock pair and the blocking-wait member stay null); leaves the log members
	void ApplySettings(const Zenith_ProfilerSettings& xSettings);   // startup step 7 and on configuration change
	void SetMode(Zenith_ProfilerMode eMode);                       // takes effect for zones begun after the call
	Zenith_ProfilerMode GetMode() const;
	// Startup step 7 sets the runtime's output. nullptr detaches: asserts no capture is running, then waits until the flush
	// thread is idle (it is outside WriteFile), so the caller may destroy the output afterwards (MOD shutdown step 6, before
	// Zenith_AsyncIO is destroyed; "Creation and shutdown").
	void SetOutput(Zenith_ProfilerOutput* pxOutput);

	// JOBS's exact call (Zenith2/Docs/Design/JOBS.md "On-screen profiler rows"): idempotent per index; the null lane when the
	// 56 CPU lanes are taken. Index 0 binds the calling thread; every other lane binds the thread whose OS name equals szName
	// ("Thread lanes"), so JOBS needs no bind call.
	Zenith_ProfilerLaneID RegisterThreadLane(uint32 uLaneIndex, const char* szName);
	void BindCurrentThread(Zenith_ProfilerLaneID xLane);   // optional explicit bind for owners whose threads carry another name

	// Zenith_ProfilerGpuSink: accepted in either mode while m_bGpuTimings is set ("GPU timing ingestion").
	Zenith_Result<void, Zenith_ProfilerError> SubmitGpuTimings(const Zenith_ProfilerGpuSubmission& xSubmission) override;

	void EndFrame(const Zenith_ProfilerFrameInputs& xInputs);     // frame-loop step 3
	void Freeze(bool bFrozen);                                   // holds the detailed frames; history continues

	Zenith_Result<void, Zenith_ProfilerError> StartCapture(uint32 uFrames, bool bWithLocks);   // 1 … 36000
	void StopCapture();
	bool IsCapturing() const;

	Zenith_Span<const Zenith_ProfilerLaneInfo> GetLanes() const;
	uint32 GetHistoryCount() const;                                 // frames in the history ring
	Zenith_ProfilerFrameSummary GetFrameSummary(uint32 uFramesAgo) const;   // 0 = the last frame; ZENITH_ASSERT in range
	bool GetFrame(uint32 uFramesAgo, Zenith_ProfilerFrameView& xOut) const;  // false when not detailed
	Zenith_Span<const Zenith_ProfilerCounterView> GetCounters() const;      // byte order of name
	const Zenith_ProfilerCounterView* FindCounter(Zenith_StringView strName) const;
	Zenith_Span<const Zenith_ProfilerZoneStat> GetZoneStats() const;        // the last frame: one entry per site closed, ascending site
	Zenith_Span<const Zenith_ProfilerLockStat> GetLockStats() const;        // the last frame: one entry per lock site used, ascending site
	Zenith_StringView GetSiteName(Zenith_ProfilerSiteID xSite) const;
	Zenith_ProfilerSiteID FindSite(Zenith_StringView strName) const;        // 0 when absent

	// Zenith_MemoryConsumer: MEMORY_SUBSYSTEM_OTHER; every buffer Create allocated, not the Zenith_Profiler object itself
	// ("Memory layout and accounting").
	Zenith_MemorySubsystem GetSubsystem() const override;
	uint64 GetResidentBytes() const override;
	void OnPressureLevelChanged(Zenith_MemoryPressureLevel eOld, Zenith_MemoryPressureLevel eNew) override;   // sheds nothing
private:
	friend class Zenith_ProfilerTestAccess;   // Internal/Tracy/Zenith_TracyBridge.h: the tests' Tracy bridge seam ("Tracy forwarding")
	friend class Zenith_ProfilerSyncTestAccess;   // Internal/Capture/Zenith_ProfilerSync.h: the tests' view of the profiler's own primitives ("Lock and wait events")
	// … the thread states, tables, frame stores and queues of "Memory layout and accounting", declared by the landing ticket
	// (which adds the includes they need, among them ZenithCore/Zenith_Mutex.h and ZenithCore/Zenith_Semaphore.h)
};

/// @contract functions PlayerProfilerDesc
/// Ownership: No state; returns a plain value.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Zenith_ProfilerDesc.
/// Errors: None.
/// Version: since Z2 Stage 2. The Player (MOD), the editor and every test of the Shipping profiler configuration call it,
///   so the per-build profiler desc has one definition: Windows (and the Linux test host) 48 thread states, 16,384-event rings, 8 detailed frames;
///   Android 32, 8,192 and 4; m_eInitialMode PROFILER_MODE_COUNTERS_ONLY when eConfiguration is BUILD_CONFIGURATION_SHIPPING,
///   else PROFILER_MODE_CAPTURE; m_bTracyAvailable false when eConfiguration is BUILD_CONFIGURATION_SHIPPING or
///   bSanitizerBuild is true; the heap and fault injector null. Callers pass Zenith_GetBuildPlatform(),
///   Zenith_GetBuildConfiguration() and Zenith_IsSanitizerBuild(); a test may pass any values.
/// @endcontract
Zenith_ProfilerDesc Zenith_GetPlayerProfilerDesc(Zenith_BuildPlatform ePlatform, Zenith_BuildConfiguration eConfiguration,
	bool bSanitizerBuild);
```

## Zenith_ProfileZone header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_ProfileZone.h
#pragma once
#include "ZenithCore/Zenith_ProfilerHooks.h"
#include "ZenithCore/Zenith_StringView.h"

/// @contract type Zenith_ProfileScope
/// Ownership: A stack object; begins a zone in its constructor through CORE's Zenith_DispatchZoneBegin and ends it in its
///   destructor through Zenith_DispatchZoneEnd.
/// Thread affinity: OWNER_THREAD. Constructed and destroyed on one thread, in strict nesting.
/// Invalidation: None.
/// Units: None.
/// Errors: None; with no hook installed it does nothing. A dynamic name longer than 63 bytes is cut and ends in '~'.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_ProfileScope
{
public:
	explicit Zenith_ProfileScope(const Zenith_ZoneSite& xSite);                       // a constexpr site
	Zenith_ProfileScope(const Zenith_ZoneSite& xSite, Zenith_StringView strDynamicName);  // the name replaces the site's
	~Zenith_ProfileScope();
	Zenith_ProfileScope(const Zenith_ProfileScope&) = delete;
	Zenith_ProfileScope& operator=(const Zenith_ProfileScope&) = delete;
	void SetValue(uint64 ulValue);    // Zenith_ProfileZoneValue for this scope's zone
private:
	bool m_bActive = false;                     // a hook was installed at construction
};

/// @contract functions ProfileZone
/// Ownership: No state of its own: writes into the calling thread's slot of the installed ZenithProfiling profiler.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: The value's meaning is the caller's (an entity count, a GUID half, a section index).
/// Errors: None; a no-op when the thread has no open zone, when no ZenithProfiling profiler owns the thread's slot, or in
///   PROFILER_MODE_COUNTERS_ONLY.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
void Zenith_ProfileZoneValue(uint64 ulValue);   // attaches to the innermost open zone of the calling thread

// Two-level concatenation: the outer macro's arguments are macro-expanded before the inner one pastes them, so __LINE__
// becomes the line number (a single-level a##__LINE__ would paste the literal token __LINE__).
#define ZENITH_PROFILE_CONCAT_INNER(A, B) A##B
#define ZENITH_PROFILE_CONCAT(A, B) ZENITH_PROFILE_CONCAT_INNER(A, B)
#define ZENITH_PROFILE_SITE(Identifier, NameLiteral) \
	static constexpr Zenith_ZoneSite Identifier{ NameLiteral, __FILE__, static_cast<uint32>(__LINE__), 0u }
#define ZENITH_PROFILE_SCOPE(NameLiteral) \
	ZENITH_PROFILE_SITE(ZENITH_PROFILE_CONCAT(xZenithProfileSite_, __LINE__), NameLiteral); \
	Zenith_ProfileScope ZENITH_PROFILE_CONCAT(xZenithProfileScope_, __LINE__)(ZENITH_PROFILE_CONCAT(xZenithProfileSite_, __LINE__))
#define ZENITH_PROFILE_SCOPE_NAMED(NameLiteral, DynamicNameView) \
	ZENITH_PROFILE_SITE(ZENITH_PROFILE_CONCAT(xZenithProfileSite_, __LINE__), NameLiteral); \
	Zenith_ProfileScope ZENITH_PROFILE_CONCAT(xZenithProfileScope_, __LINE__)(ZENITH_PROFILE_CONCAT(xZenithProfileSite_, __LINE__), \
		DynamicNameView)
```

- The identifiers are `xZenithProfileSite_<line>` and `xZenithProfileScope_<line>`, so two scopes on different lines of one block are distinct objects and a nested scope never shadows its parent's names (C4456 and `-Wshadow` stay silent under warnings-as-errors). Two `ZENITH_PROFILE_SCOPE` uses on one source line remain a redefinition error, which is the intended diagnostic. `Test_ProfileZone_MacroTwoScopesInOneBlock` (two scopes in one block and a third nested in an inner block, compiled in `ZenithProfiling_Tests` under the build's warnings-as-errors flags, asserting three zones with the right names and depths) is the compile test.
- One macro use is one bracket, and a bracket is reusable without limit: the same `ZENITH_PROFILE_SCOPE` or `ZENITH_PROFILE_SCOPE_NAMED` line may be entered any number of times per frame, in a loop, from several call paths, nested inside itself (recursion) and on several threads at once. Each entry is its own `Zenith_ProfileScope` object and its own zone, but nothing keys a statistic on the object or the zone: the profiler accumulates every closure into the frame total of the zone's site, the name the zone was begun with (§ "Hook implementation" step 5), and resets that total once per frame at `EndFrame` (§ "Zone statistics", Z2-R-DEVT-143).
- The `static constexpr` site is immutable data ([Z2-N-CONTRACT-145]) and produces no writable symbol, so the macros pass the writable-data scan.
- The dynamic-name constructor copies at most 63 bytes of the name into a 64-byte local buffer, builds a local `Zenith_ZoneSite` with `m_szName` pointing at it, and calls `Zenith_DispatchZoneBegin`; the hook copies the text at the call (§ "Hook implementation"), so the buffer may die with the scope. No scope member names a `m_pfn` member of the hook table.
- Two values attached to one zone keep the first in `m_ulValue` and set `uPROFILER_ZONE_FLAG_MULTIPLE_VALUES`; a capture keeps every value (§ "Capture format", chunk `ZONE_VALUES`). ASSET attaches the GUID's high word, then its low word, then the section index; the capture keeps all three, the overlay shows the first.
- `Zenith_ProfileZoneValue` reads `g_xProfilerThreadBuffers` (§ "Thread slots and the thread-local variable") directly, the one public path that does, which [Z2-N-CONTRACT-144] admits because the variable is ZenithProfiling's own.

## Hook implementation

`FillHooks` writes:

| Member | Function (`Internal/Hooks/Zenith_ProfilerHooksImpl.cpp`) |
|---|---|
| `m_pfnOnZoneBegin` | `Zenith_ProfilerOnZoneBegin(const Zenith_ZoneSite&, void*)` |
| `m_pfnOnZoneEnd` | `Zenith_ProfilerOnZoneEnd(void*)` |
| `m_pfnOnCounter` | `Zenith_ProfilerOnCounter(const char*, sint64, Zenith_CounterKind, void*)` |
| `m_pfnOnLockAcquire` | `Zenith_ProfilerOnLockAcquire(const Zenith_LockDesc&, const void*, uint64 ulWaitTicks, void*)` |
| `m_pfnOnLockRelease` | `Zenith_ProfilerOnLockRelease(const Zenith_LockDesc&, const void*, void*)` |
| `m_pfnOnBlockingWait` | `Zenith_ProfilerOnBlockingWait(const char*, uint64 ulWaitTicks, void*)` |
| `m_pUserData` | the `Zenith_Profiler*` |

It never writes `m_pfnOnLogRecord`, `m_pfnIsLogLevelEnabled` or `m_pLogUserData`; the Player calls `Zenith_LogRouter::FillHooks` on the same table afterwards ([Z2-N-CONTRACT-157]; `Zenith2/Docs/Design/MOD.md` → "Player headers").

How the hooks are reached. Every caller in the engine, the profiler's own code included, invokes a hook through CORE's inline dispatch functions in `Zenith_ProfilerHooks.h` (`Zenith_DispatchZoneBegin`, `Zenith_DispatchZoneEnd`, `Zenith_DispatchCounter`, `Zenith_DispatchLockAcquire`, `Zenith_DispatchLockRelease`, `Zenith_DispatchBlockingWait`); a dispatch function counts the thread's hook depth, refuses to call a hook that is already running on the thread and reports the refusal ([Z2-N-CONTRACT-590]; `Zenith2/Docs/Contracts.md` → "No mutable global state"). The log-record hooks are outside the dispatch functions. The profiler's own code that emits zones, `Zenith_ProfileScope`, `EndFrame`'s `Profiler.Assemble` and the flush thread's `Profiler.FlushSegment`, calls `Zenith_DispatchZoneBegin` and `Zenith_DispatchZoneEnd` and never a `m_pfn` member. `Zenith_GetProfilerHooks()` is read by the install code (`FillHooks`, the destructor's installed-hooks assert) and by tests alone. The hook-depth guard is CORE's last line against recursion; the profiler's own guarantee is its types ("Lock and wait events").

Shipping. `FillHooks` writes the lock pair (`m_pfnOnLockAcquire`, `m_pfnOnLockRelease`) and `m_pfnOnBlockingWait` only when `!Zenith_IsShippingBuild()` (an `if constexpr`), so in a Shipping build those three members stay null, a Shipping lock or wait pays a try-lock and a null test and no dispatch, and the pair rule of [Z2-N-CONTRACT-143] (both set or both null) holds. Lock capture does not exist there: `StartCapture` is refused, `captureLocks` has no effect and the overlay draws no lock toggle.

Each thread state keeps one zone-depth counter `m_uDepth` (0 … `uPROFILER_MAX_ZONE_DEPTH`) shared by both modes, and two 64-bit masks indexed by depth level: `m_ulCapturedMask` (bit d set when the zone at level d was begun in `PROFILER_MODE_CAPTURE`, so its begin is in the ring) and `m_ulTracyMask` (bit d set when a Tracy context was pushed for it). The masks make every end match its own begin whatever happened between the two: a mode switch or a Tracy start between a begin and its end never unbalances the ring, the depth or Tracy's stack.

Zone begin, in order:

1. Load the thread slot `g_xProfilerThreadBuffers`. If `m_bInProfiler` is set, return (a hook fired from inside the profiler).
2. If `m_pxOwner != pUserData` or `m_uGeneration` differs from the owner's generation, claim a thread state (§ "Thread slots and the thread-local variable"); on failure (every state claimed) increment the profiler's `m_ulUnclaimedDrops` (relaxed) and return.
3. If `m_uDepth == uPROFILER_MAX_ZONE_DEPTH`, count `m_ulDepthOverflows` and increment `m_uDepthOverflowPending` (the matching ends are swallowed by step 2 of the end) and return. Let `d = m_uDepth`; increment `m_uDepth`; clear bits d of both masks.
4. Load the profiler's mode (relaxed). In `PROFILER_MODE_COUNTERS_ONLY`, return (bit d of `m_ulCapturedMask` stays clear).
5. Hash the name: `Zenith_Hash64` over at most 63 bytes of `xSite.m_szName` (stop at NUL). Look the hash up in the thread's 256-entry direct-mapped cache (`m_axSiteCache[hash & 255]`, entries `{ uint64 hash; uint32 site; }`); on a miss, `Zenith_ProfilerNameTable::FindOrInsert(hash, name, length, file, line, colour)` (§ "Name tables") and fill the entry only when the returned site is not 0: site 0 (`<names full>`, or a writer's spin timeout) is used for this event and never cached, so the next call retries the table.
6. Read the clock: `Zenith_GetMonotonicTimeNs()`.
7. Append `{ time, site, 0, ZONE_BEGIN, d + 1 }` to the thread's ring (§ "Per-thread event rings"); when the push succeeds set bit d of `m_ulCapturedMask`; a refused push leaves it clear and is counted (the zone then has no begin and its end is not appended, so the ring stays balanced).
8. If Tracy is active (`m_bTracyActive`, acquire; a Shipping build compiles this step out, `if constexpr (!Zenith_IsShippingBuild())`), call the bridge table's `m_pfnZoneBegin(&m_axTracySourceLocations[site])` with the site's persistent source location (§ "Tracy forwarding"), push the returned 8-byte Tracy context on the slot's Tracy stack and set bit d of `m_ulTracyMask`. Site 0 has a source location too (`<names full>`).

Zone end, in order: (1) step 1 of the begin; (2) if the thread has no claimed state of this profiler, or `m_uDepth == 0` (an end with no begin since the state was claimed), count `m_ulOrphanEnds` and return; if `m_uDepthOverflowPending > 0`, decrement it and return; (3) decrement `m_uDepth` to `d`; (4) when bit d of `m_ulCapturedMask` is set, read the clock and append `{ time, 0, 0, ZONE_END, d }` whatever the current mode (a zone begun in capture mode always records its end, so a mode switch never truncates it); a refused push stores the thread state's `std::atomic<bool> m_bEndDropped` with `release` for frame assembly's repair; (5) when bit d of `m_ulTracyMask` is set, pop the Tracy context and call `Zenith_TracyBridgeZoneEnd(context)` (a zone begun before Tracy started has bit d clear and never pops Tracy's stack).

Zone value (`Zenith_ProfileZoneValue`, `Zenith_ProfileScope::SetValue`): step 1; when `m_uDepth > 0` and bit `m_uDepth − 1` of `m_ulCapturedMask` is set, append `{ value, 0, index, ZONE_VALUE, m_uDepth }`, where `index` is the level's value count (`m_auValueCount[m_uDepth − 1]`, a 64-entry `uint8` array of the thread state reset by the begin, saturating at 255) before it is incremented; with Tracy's bit set for that level, `Zenith_TracyBridgeZoneValue`.

Counter: step 1; then the counter path of § "Counter registry" — it needs no thread state (counters are aggregated in the shared table), so it works for threads without a claimed state and in both modes.

Lock acquire and release, blocking wait: § "Lock and wait events"; the per-site lock statistics those hooks keep: § "Lock statistics".

Every hook takes no mutex, condition variable or semaphore of any kind (profiled or not), makes no allocation from any Zenith heap, makes no system call other than the clock read (and the one thread-name query at claim time, § "Thread lanes": on Windows `GetThreadDescription` allocates the OS string with `LocalAlloc` and the claim frees it with `LocalFree` at once, the only OS-heap allocation on a hook path, once per thread), and touches only the slot's own state, the shared tables through atomics, and the thread's own ring. Hooks may therefore fire under any held lock (including `uLOCK_LEVEL_HEAP_LEAK_TABLE` 990), inside the OOM handler and inside the log router's sink lock. These guarantees are stated for Tracy inactive, which is every budget run, every sanitizer build and every Shipping build: while Tracy is active, Tracy's own C API calls inside steps 8 and end (5) enqueue into Tracy's per-thread queue, which Tracy allocates from its own allocator, outside the Zenith heap and outside every Zenith lock, and the profiler adds no allocation of its own to them (§ "Tracy forwarding").

Tests of the masks (`Zenith2/Docs/Design/DEVT/Tests.md` → "ZenithProfiling unit tests"): `Test_Profiler_ModeSwitchMidZone_EndsBalanced` (a zone begun in capture mode and ended after a switch to counters-only is assembled whole with no repair flag; a zone begun in counters-only mode and ended after a switch to capture appends nothing and counts no orphan end), `Test_Profiler_TracyStartedMidZone_StackBalanced` (with a recording bridge table installed through `Zenith_ProfilerTestAccess::SetTracyBridge`, § "Tracy forwarding": a zone open when Tracy starts ends without popping Tracy's stack, and a zone begun after the start pushes and pops exactly once; in `ShippingTest` the table records no call at all, because every bridge call is compiled out), `Test_Profiler_SiteZeroNeverCached` (the test holds the name's table entry in the "being written" state through `Zenith_ProfilerNameTableTestAccess::HoldWriting` from `Internal/Capture/Zenith_ProfilerNameTable.h`, so the begin's lookup times out and records site 0; after `ReleaseWriting` the next begin from the same call site on the same thread records the real site, proving the 0 was not cached).

## Thread slots and the thread-local variable

```cpp
// Zenith2/Engine/ZenithProfiling/Internal/Hooks/Zenith_ProfilerThreadSlot.h
struct Zenith_ProfilerThreadSlot
{
	Zenith_ProfilerThreadState* m_pxState = nullptr;        // the claimed state (depth, masks, caches, open-zone stack), or null
	const void* m_pxOwner = nullptr;                        // the Zenith_Profiler that owns m_pxState
	uint32 m_uGeneration = 0;
	bool m_bInProfiler = false;
	// The three members below exist only under the primary path of ruling Q79 (the block after this listing).
	Zenith_DebugDrawThreadState* m_pxDebugDraw = nullptr;   // the debug-draw ring of DebugDraw.md "Recorder rings and drain"
	const void* m_pxDebugDrawOwner = nullptr;               // the Zenith_DebugDraw that owns m_pxDebugDraw
	uint32 m_uDebugDrawGeneration = 0;
};
// Internal/Hooks/Zenith_ProfilerThreadSlot.cpp: the one writable symbol of ZenithProfiling (writable_data_allowlist.json, Z2-D-046)
constinit thread_local Zenith_ProfilerThreadSlot g_xProfilerThreadBuffers;
```

<!-- assumption: pending Q79/Q11 -->
**Primary path (ruling Q79 (a)).** The debug-draw recorder caches its per-thread ring in the same slot (the last three members), so a drawing call is one thread-local load and one owner compare, as a hook is. [Z2-N-CONTRACT-144] admits `g_xProfilerThreadBuffers` as the place "where the hook implementations keep per-thread buffers"; the recorder is not a hook, so this path widens the Z2-D-046 exemption by one use (no new writable symbol) and needs [144] to read "the hook implementations and the debug-draw recorder keep per-thread buffers", requested as X247 (a) and to be recorded as a dated extension of the exemption, as the logging extension folded into Q11 is. **Alternative (Q79 (b)), specified in full in `Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Recorder slot alternative":** the slot keeps only its first four members, and `Zenith_DebugDraw` finds a thread's ring through its own lock-free table keyed by the OS thread ID, with no thread-local storage. The recorder's public API, its rings, the drain and every test are identical on both paths; only `Internal/DebugDraw/Zenith_DebugDrawSlots.cpp` differs, and the ticket that lands the recorder implements whichever path the ruling chose.
<!-- /assumption -->

- Claim: the profiler holds `m_axThreadStates[m_uThreadStates]`, each with `std::atomic<uint8> m_uClaimed`: 0 free, 1 being claimed, 2 published. The claiming thread scans from index `hash(threadID) % N`, does `compare_exchange_strong(0 → 1, acq_rel)` on the first free state, writes `m_ulOsThreadID` and the OS thread name (at most 63 bytes, read once, § "Thread lanes" for the platform's limit) as plain fields, then publishes with `m_uClaimed.store(2, seq_cst)`, then binds a registered lane of the same name when one is published (§ "Thread lanes", binding at claim; otherwise the lane stays 0 and `EndFrame` assigns it), and stores the slot fields. The claim never writes the lane table. Every other reader of a state (frame assembly, `RegisterThreadLane`'s binding scan, `GetLanes`) acts only on `m_uClaimed.load(acquire) == 2` (`seq_cst` in the binding scan) and reads the plain fields after that load, so the fields are never read while being written. A thread never releases its state: thread exit leaves it claimed with its ring drained at the next `EndFrame`, and a thread ID reused by the OS after exit claims a new state (bounded by `m_uThreadStates`; the 49th distinct thread's events are dropped and counted; the claim runs inside a hook, so it may run under any held lock and never logs: it stores the thread's ID in the profiler's `m_ulExhaustedThread` (`compare_exchange_strong(0 → id, relaxed)`; a thread ID of 0 is not a valid claimant, so the claim stores `0xFFFFFFFFFFFFFFFF` for it), and `EndFrame` logs, only while the main-thread latch `m_bExhaustionWarned` is clear (and then sets it), the one warning `devtools.profiler: thread states exhausted (48); events of thread <id> dropped` on the main thread).
- The generation (`m_uGeneration`) is the low 32 bits of the profiler object's address XOR the low 32 bits of its creation time in ns, computed once by `Create`; it needs no shared state, and a slot claimed by a destroyed profiler is not trusted by a new profiler created at the same address.
- Every field of a thread state that more than one thread touches, with its type and orders (the rest are the owning thread's alone, or the main thread's alone):

| Field | Type | Written by | Read by | Orders |
|---|---|---|---|---|
| `m_uClaimed` | `std::atomic<uint8>` | the claiming thread (CAS 0 → 1, then 2) | assembly, the binding scan, `GetLanes` | CAS `acq_rel`; publication `store(2, seq_cst)`; readers `load(acquire)`, the binding scan `load(seq_cst)` |
| `m_ulOsThreadID`, `m_acOsName[64]`, `m_uOsNameLength` | plain | the claiming thread, before the publication | assembly, the binding scan, `GetLanes` | published by `m_uClaimed`'s store of 2 |
| `m_uLane` | `std::atomic<uint16>` (0 = unassigned) | the claiming thread (CAS 0 → a registered lane, once, at claim), `RegisterThreadLane` (main thread; CAS from the value read, 0 or automatic, → the registered lane), `EndFrame` (main thread; CAS 0 → a registered, automatic or overflow lane before draining), `BindCurrentThread` (owner; `store`) | assembly | every CAS `acq_rel` with failure `acquire`; `BindCurrentThread` `store(release)`; assembly `load(acquire)` and a reload after a failed CAS |
| the event ring | CORE `Zenith_SpscRing<Zenith_ProfilerEvent>` | the owner (`TryPush`) | assembly (`PopSpan`) | the ring's own contract (row DV23) |
| `m_bEndDropped` | `std::atomic<bool>` | the owner, when an end push is refused | assembly, after draining the ring | `store(true, release)`; `exchange(false, acq_rel)` |
| `m_uDepth`, `m_ulCapturedMask`, `m_ulTracyMask`, `m_auValueCount`, the caches, the Tracy stack, `m_bTracyNamed` | plain | the owner alone | the owner alone | none |
| the open-zone stack, the carry list | plain | assembly alone (main thread) | assembly alone | none |
| the thread's `m_xLock.m_uActive` (`std::atomic<uint32>`, 0 or 1) | atomic | the main thread (`store(seq_cst)`, once per `EndFrame` per state) | the owner's lock hooks | `load(seq_cst)`: the bank handover of § "Lock statistics" |
| each lock bank's `m_uWriters` | `std::atomic<uint32>` | the owner (`fetch_add(seq_cst)` on entry, `fetch_sub(release)` on exit) | the main thread (`load(seq_cst)`) | the writer-count handshake of § "Lock statistics" |
| a lock bank's counters and entries | plain | the owner while inside a writer count of the active bank; the main thread once it has granted itself the inactive bank | the same side | the handshake above |
| the held-lock stack `m_xLock.m_axHeld` and its count | plain | the owner alone | the owner alone | none |

- The profiler-level switches that hooks on every thread read, and that the main thread writes (`ApplySettings`, `SetMode`), are atomic members of `Zenith_Profiler` (`Internal/Hooks/Zenith_ProfilerHooksImpl.h` reads them through the profiler pointer the hooks receive):

| Member | Type | Written by | Read by | Orders |
|---|---|---|---|---|
| `m_uMode` | `std::atomic<uint8>` holding a `Zenith_ProfilerMode` | `ApplySettings`, `SetMode` (main thread) | the zone begin (§ "Hook implementation" step 4), `GetMode` | `store(relaxed)`; `load(relaxed)`: the mode guards no data, a hook that reads the old value for one zone is the documented "applies to zones begun after it", and every end follows its own begin's mask |
| `m_bCaptureLocks` | `std::atomic<bool>` | `ApplySettings`, `StartCapture` with locks (main thread) | the lock and wait hooks (§ "Lock and wait events", § "Lock statistics") | `store(release)`; `load(relaxed)` (it guards no data) |
| `m_bGpuTimings` | `std::atomic<bool>` | `ApplySettings` (main thread) | `SubmitGpuTimings` (the render-submit or main thread) | `store(release)`; `load(acquire)` |
| `m_bTracyActive` | `std::atomic<bool>` | `ApplySettings`, once, after `m_pfnStartup` returned (main thread) | the zone begin step 8, the claim's thread naming, `EndFrame` | `store(true, release)`; `load(acquire)`, so a hook that sees it true sees Tracy started |
| `m_ulExhaustedThread` | `std::atomic<uint64>` | a claiming thread that found every state claimed (`compare_exchange_strong(0 → id, relaxed)`) | `EndFrame` (`exchange(0, relaxed)`, then the one warning per run, guarded by the main-thread latch `m_bExhaustionWarned`) | `relaxed`: it carries a thread ID for a log line and guards no data |

  No other profiler member is written on the main thread and read by a hook; the thread states' shared fields are the table above.
- `m_bInProfiler` is set by the profiler around its own code that can reach a hook (`EndFrame` between its steps (b) and (d), and the flush thread around each `Zenith_ProfilerOutput::WriteFile` call), so a hook that fires from inside that code is ignored. It is defence in depth and not the guarantee: the profiler's own mutexes, condition variables and semaphores are `_NoProfiling` types that never report to a hook, and CORE's dispatch functions refuse a re-entered hook in every build ("Lock and wait events"). No hook looks at a lock's level.

## Event records

```cpp
// Zenith2/Engine/ZenithProfiling/Internal/Capture/Zenith_ProfilerEvent.h
enum Zenith_ProfilerEventKind : uint8
{
	PROFILER_EVENT_ZONE_BEGIN = 1,
	PROFILER_EVENT_ZONE_END = 2,
	PROFILER_EVENT_ZONE_VALUE = 3,    // m_ulTime carries the value
	PROFILER_EVENT_LOCK_ACQUIRED = 4,
	PROFILER_EVENT_LOCK_RELEASED = 5,
	PROFILER_EVENT_BLOCKING_WAIT = 6,
	PROFILER_EVENT_LANE_BIND = 7,     // m_uSite carries the lane ID
	PROFILER_EVENT_LOCK_WAIT_BEGIN = 8,   // a contended acquire: m_ulTime is the start of the wait, m_uSite the "lockwait:<name>" site; the LOCK_ACQUIRED that follows it ends the wait
	PROFILER_EVENT_WAIT_BEGIN = 9,        // a blocking wait with a measured wait: m_ulTime is the start, m_uSite the "wait:<primitive>" site; the BLOCKING_WAIT that follows it ends the wait
};

struct Zenith_ProfilerEvent           // 16 bytes, 8-byte aligned
{
	uint64 m_ulTime;                 // 0: ns, or the value for ZONE_VALUE
	uint32 m_uSite;                  // 8: site (BEGIN), "lock:<name>" site (LOCK_ACQUIRED, LOCK_RELEASED), "lockwait:<name>" site (LOCK_WAIT_BEGIN), "wait:<primitive>" site (BLOCKING_WAIT, WAIT_BEGIN), lane (LANE_BIND), 0 otherwise
	uint16 m_uAux;                   // 12: lock level (LOCK_*), value index (ZONE_VALUE), 0 otherwise
	uint8 m_uKind;                   // 14: Zenith_ProfilerEventKind
	uint8 m_uDepth;                  // 15: the thread's zone depth after the event (BEGIN: level + 1; END: level; VALUE: the open depth)
};
static_assert(sizeof(Zenith_ProfilerEvent) == 16);
```

## Per-thread event rings

Each thread state owns one `Zenith_SpscRing<Zenith_ProfilerEvent>` (CORE's primitive, ruling Q80: DEVT adds no ring of its own and no justification record for one; the specification is the AU6 block of `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas", which CORE lands as `ZenithCore/Zenith_SpscRing.h`, row DV23):

- Storage: `Create` allocates one block of `m_uThreadStates × m_uRingRecords` events from `m_pxHeap` and calls `Init` on each state's ring with its slice (a `Zenith_Span<Zenith_ProfilerEvent>` of `m_uRingRecords` elements, a power of two); nothing is allocated afterwards.
- Producer: the thread that owns the state, through `TryPush` alone, from inside the hooks; the ring calls no hook, logs nothing, makes no system call and never blocks, which is what a hook context needs ([Z2-N-CONTRACT-144]'s per-thread buffers).
- Consumer: the main thread in `EndFrame`, through `PopSpan` into the assembler's 4,096-event buffer, repeated until it returns fewer events than the buffer holds (the ring copies at most two contiguous runs per call).
- Full policy: `QUEUE_FULL_POLICY_DROP_NEWEST`; the ring counts each refused push itself, and `EndFrame` publishes the triple `profiler.rings.full` (always 0 under DROP_NEWEST, published for uniformity), `profiler.rings.dropped` and `profiler.rings.high_water`, summed over every ring from each ring's `GetStats` ([Z2-N-CONTRACT-345]).
- Thread safety under `linux-tsan`: the ring's stress test is CORE's (`Test_SpscRing_StressRoleHandOver` at 2, 8 and the hardware concurrency, [Z2-N-CONTRACT-357]); DEVT's own stress test of the whole hook path is `Test_Profiler_HookStress` (`Zenith2/Docs/Design/DEVT/Tests.md` → "ZenithProfiling unit tests").

## Name tables

`Zenith_ProfilerNameTable` (`Internal/Capture/Zenith_ProfilerNameTable.h`; one justification record) maps a 64-bit name hash to an ID. One instance holds sites (capacity 16,384), one counters (4,096), one lanes (64); all share the name arena of `uPROFILER_NAME_ARENA_BYTES` (1,314,816 bytes), which holds a 63-byte name and its terminating NUL for every entry of the three tables, so in the profiler an insert never finds the arena exhausted; the exhaustion rule below exists for a table built over a smaller arena (the tests) and as a defensive bound.

| Field of an entry (32 bytes) | Meaning |
|---|---|
| `std::atomic<uint64> m_ulHash` | 0 = empty; a hash of 0 is stored as 1 |
| `uint32 m_uNameOffset` | offset into the name arena |
| `uint16 m_uNameLength` | bytes, at most 63, excluding the NUL stored after them in the arena (Tracy reads site and plot names as C strings, "Tracy forwarding") |
| `uint16 m_uLine` | the site's line (0 for counters), cut to 16 bits |
| `const char* m_szFile` | the site's file literal (stable: sites' file strings are literals), null for counters and dynamic names |
| `uint32 m_uColour` | the site's colour |
| `std::atomic<uint32> m_uState` | 0 empty, 1 being written, 2 published, 3 refused (the arena could not hold the name: a terminal state, read as site 0 without spinning) |

`FindOrInsert(hash, name, length, …)`: probe linearly from `hash & (capacity − 1)`. For each entry: `state == 2` and hash equal → compare the stored bytes with the name (`memcmp`, at most 63 bytes); equal returns the index (a hash collision with different text continues probing). `state == 3` and hash equal → return 0 at once, with no spin and no byte comparison (the entry holds no text; a different name colliding on the full 64-bit hash would also have found the arena exhausted). `m_ulHash == 0` → `compare_exchange_strong(0 → hash, acq_rel)`; the winner reserves its bytes and the terminator with `o = m_uArenaUsed.fetch_add(length + 1, relaxed)` (`std::atomic<uint32>`); when `o + length + 1` exceeds the arena it stores `m_uState = 3` (release), counts `profiler.sites_full_delta` or `profiler.counters_full_delta` and returns 0, and every subsequent lookup of that name on every thread returns 0 at once; otherwise it copies the bytes to `o`, stores a NUL at `o + length`, fills the fields and stores `m_uState = 2` (release). A loser of the exchange re-reads the entry and, while `m_uState == 1`, spins with `Zenith_CpuRelax` (the writer is between two stores, no wait on another job's work) up to 1,024 iterations, after which it returns 0 for this call only (callers never cache a 0, § "Hook implementation" step 5 and § "Counter registry" step 1); state 1 never persists, because the winner leaves it for 2 or 3 within the same call. A full table (probe of `capacity` entries) returns 0 and counts `profiler.sites_full_delta` or `profiler.counters_full_delta`. Readers on the main thread read published entries only.

The table is DEVT's one custom container (record `Custom container: Zenith_ProfilerNameTable`, `Zenith2/Docs/Design/DEVT.md` → "Justification records DEVT adds"). Its contract block, in `Internal/Capture/Zenith_ProfilerNameTable.h`, carries the container fields of [Z2-N-CONTRACT-281]:

| Field | Value |
|---|---|
| Ownership | storage of `capacity` 32-byte entries and the shared name arena, both borrowed from the profiler's `Create` block; the table allocates nothing |
| Thread affinity | `FindOrInsert` ANY_THREAD (lock-free, any hook context); `GetName`, `GetEntryCount` and iteration MAIN_THREAD (published entries only) |
| Invalidation | none: entries are insert-only and never move; a name view stays valid for the profiler's lifetime |
| Alignment | entries 8-byte aligned (the `std::atomic<uint64>` hash); the arena byte-aligned |
| Overflow | at most `capacity` entries (a power of two in 64 … 65,536, `ZENITH_CHECK` at `Init`); `Init` computes `capacity × 32` with overflow-checked arithmetic and refuses an overflowing or over-storage size with `ZENITH_CHECK`; a full table returns ID 0 and a name the arena cannot hold leaves its entry in the terminal state 3, so it returns ID 0 on every subsequent call without spinning |
| Allocation failure | not applicable (no allocation after `Init`) |
| Complexity | `FindOrInsert` expected O(1), worst case O(capacity) probes; the hash is `Zenith_Hash64`, chosen by D-SEL-HASH (open, Stage 1) |
| Locks | none; no profiler hook, no log, no system call |

There is no `Try` operation (the table never allocates, so [Z2-N-CONTRACT-285]'s two forms and `Test_ProfilerNameTable_TryFailure_LeavesUnchanged` have nothing to exercise), and entries are trivially copyable bytes that are never constructed or destroyed, so [Z2-N-CONTRACT-291]'s counting element type does not apply; the tests that do apply are `Test_ProfilerNameTable_MatchesOracle`, `Test_ProfilerNameTable_Properties`, the three size-overflow death cases `Test_ProfilerNameTable_SizeOverflow_NotPowerOfTwo`, `…_AboveMaximum` and `…_ExceedsStorage` (one refusal each, because a death case dies once per process) and the stress test `Test_ProfilerNameTable_StressConcurrentInsert` (`Zenith2/Docs/Design/DEVT/Tests.md` → "ZenithProfiling unit tests").

## Counter registry

`Zenith_ProfilerOnCounter(szName, iValue, eKind, pUser)`:

1. Hash the name (the full name up to 63 bytes); per-thread 64-entry counter cache (a thread without a claimed state skips the cache), then `FindOrInsert` in the counter table (the kind is stored at insert; a subsequent call with another kind for the same name counts `profiler.counter_kind_conflicts` and is applied with the stored kind); an ID of 0 is applied to counter 0 `<counters full>` and never cached.
2. Apply by kind, on the counter's 64-byte atomic cell:

| Kind | Action |
|---|---|
| `COUNTER_KIND_CUMULATIVE` | `m_iLatest.store(iValue, relaxed)` (the running total the producer publishes) |
| `COUNTER_KIND_DELTA` | `m_iAccum.fetch_add(iValue, relaxed)` |
| `COUNTER_KIND_GAUGE` | `m_iLatest.store(iValue, relaxed)` |

Snapshot at `EndFrame`, for each published counter: CUMULATIVE — frame value = latest − previous snapshot, previous = latest; DELTA — frame value = `m_iAccum.exchange(0, acq_rel)`, latest += frame value; GAUGE — frame value = latest. The frame value is pushed into the counter's 128-entry history ring (`sint64`), from which min, max and mean are recomputed incrementally (min and max by rescanning the 128 entries when the removed value was the extreme, which is O(128) at most per changed counter). Names keep their `_delta` suffix as published; the overlay shows them verbatim. The profiler publishes its own counters (`Zenith2/Docs/Design/DEVT.md` → "Counters") through the same path from `EndFrame`.

## Thread lanes

| Lane IDs | Kind | Source |
|---|---|---|
| 1 … 56 | CPU threads | `RegisterThreadLane` (JOBS: `Z2 Main` index 0, `Z2 Worker NN` 1 … N, `Z2 RenderSubmit` N + 1; any other owner may register) or automatic (created by `EndFrame`) |
| 57 … 63 | GPU queues | created by `EndFrame` on the main thread when it first drains a submission of a queue (never on the submitting thread): `GPU Graphics`, `GPU Compute`, `GPU Copy` |
| 64 | the overflow CPU lane `<other threads>` | created by `Create`; every thread state left without a lane once the 56 CPU lanes are taken |

`RegisterThreadLane` is exactly JOBS's call (`Zenith_ProfilerLaneID RegisterThreadLane(uint32 uLaneIndex, const char* szName)`, `Zenith2/Docs/Design/JOBS.md` → "On-screen profiler rows"), and JOBS's flow — `Zenith_JobSystem::Create` step 9 registers one lane per slot after its threads exist, stores the returned value, logs a null result once and otherwise ignores it — attributes every JOBS thread to its lane with no further call, by these rules. The lane table has one writer, the main thread (`Create`, `RegisterThreadLane` and `EndFrame`); a recording thread never writes it.

- `RegisterThreadLane(uLaneIndex, szName)`: main thread. A second registration with the same `uLaneIndex` returns the existing lane (idempotent across job-system re-creation in tests). It copies the name (at most 63 bytes). Past 56 CPU lanes (registered and automatic together) it returns the null lane, counts `profiler.lanes_refused` and logs `devtools.profiler: lane table full (56); lane '<name>' not registered` once per run; the thread then stays on whatever lane `EndFrame` gives it (the overflow lane once no CPU lane is free).
- Lane state of a thread: the `std::atomic<uint16> m_uLane` of the thread state is 0 (unassigned) from the claim until a binding sets it; every binding below is a compare-exchange from the value its writer read, so no binding overwrites one it did not see.
- Binding at claim: each claimed thread state records its OS thread name at claim. After `m_uClaimed.store(2, seq_cst)` the claiming thread reads `m_uLaneCount.load(seq_cst)` and the entries below it; when a registered lane `R` of its name exists it binds with `m_uLane.compare_exchange_strong(0 → R, acq_rel)` (a failure means the main thread bound it first: to `R` by the same rule, or to an automatic lane that the registration's scan replaces, below). It reads only entry fields that never change after the count's store (`m_uNameId`, `m_uKind`, `m_uRegisteredIndex`), and the name bytes through the lane name table's published entry, read as `FindOrInsert`'s comparison reads one (`m_uState.load(acquire) == 2`). Otherwise `m_uLane` stays 0.
- Binding at registration: `RegisterThreadLane` (main thread) writes the new entry (its name inserted into the lane name table first), then `m_uLaneCount.store(n + 1, seq_cst)`, and only then scans the thread states (`m_uClaimed.load(seq_cst) == 2`) and binds every state whose recorded name equals `szName` and whose lane, read as `v`, is 0 or automatic, with `m_uLane.compare_exchange_strong(v → R, acq_rel)`; a replaced automatic lane is marked merged (never shown again, its ID never reused).
- Binding at assembly: `EndFrame` (main thread, § "Frame assembly" step 2), before it drains a published state, loads its `m_uLane` (`acquire`). When it is 0, assembly looks up a registered lane of the state's recorded name and binds with `compare_exchange_strong(0 → R, acq_rel)`; when there is none it creates an automatic CPU lane `A` (named from the recorded OS thread name, or `Thread <id>` when it has none), writes its entry, stores `m_uLaneCount` (`seq_cst`) and binds with `compare_exchange_strong(0 → A, acq_rel)`; when no CPU lane is free it binds the overflow lane 64 instead, counting `profiler.lanes_refused` once per state and logging `devtools.profiler: lane table full (56); threads without a lane share '<other threads>'` once per run. A failed exchange means the claim bound it first, and assembly reloads `m_uLane`. So every lane-table entry is written on the main thread before the count's `seq_cst` store that publishes it.
- Why every registered thread reaches its lane: registration and claim are each a store followed by a load, both `seq_cst` (the registration stores the count and then loads `m_uClaimed`; the claim stores `m_uClaimed` and then loads the count), so at least one side sees the other: either the registration's scan binds the state, or the claim binds the lane. When the claim lost its exchange to an automatic lane that an earlier `EndFrame` created (assembly ran before the registration), the registration's scan sees that published state and replaces the automatic lane. A state that neither side bound is bound by name at the next assembly, before any of its events is drained, because assembly runs on the registering thread and sees every earlier registration. When several bind, they bind the same `R`, which is harmless.
- Names are compared byte for byte over at most `Zenith_ProfilerPlatformThreadNameLimit()` bytes, which the platform file supplies: 63 on Windows (`GetThreadDescription` has no shorter limit; the profiler keeps 63), 15 on Android and Linux (`pthread_setname_np` keeps 15 bytes plus the NUL), so a registered name longer than 15 bytes still binds there by its first 15 bytes. Two registered names that agree on those bytes are ambiguous on that platform: the first registered binds, and `devtools.profiler: lane '<name>' ambiguous in the first <n> bytes` is logged once. JOBS's names (`Z2 Worker NN`, 12 bytes; `Z2 RenderSubmit`, 15) fit. JOBS's threads carry exactly the names it registers (`Z2 Worker NN`, `Z2 RenderSubmit`, applied by CORE's thread start before the entry runs; `Zenith2/Docs/Design/JOBS/Algorithms.md` steps 7 and 8), so they bind without a call.
- The lane table: `m_axLanes[64]` of `{ uint32 m_uNameId; uint32 m_uRegisteredIndex; uint8 m_uKind; uint8 m_uMerged; uint16 m_uReserved; uint64 m_ulOsThreadID; uint64 m_ulReserved; }` (24 bytes) plus `std::atomic<uint16> m_uLaneCount`. `m_uNameId`, `m_uRegisteredIndex` and `m_uKind` are written before the count's store and never change; `m_uMerged` and `m_ulOsThreadID` (the thread last bound, for `GetLanes`) are written and read by the main thread alone. Every field a claiming thread reads is therefore published by the count's `seq_cst` store and needs no ordering of its own.
- Index 0 binds the caller: JOBS's slot 0 is the thread that calls `Create` (the main thread), whose OS name is the process's, so a registration with `uLaneIndex == 0` binds the calling thread's state (claiming it if needed) rather than matching by name.
- Attribution: frame assembly reads `m_uLane` (`acquire`) when it drains a state, so events drained after a rebinding are attributed to the new lane.
- `BindCurrentThread(xLane)`: optional, on the thread itself, for an owner whose threads carry another name: stores the state's lane (`store(release)`; a registration scan or an assembly that read the older value then fails its exchange and leaves it) and appends a `LANE_BIND` event so assembly switches lanes at that point of the ring. JOBS does not call it.
- System calls and allocation at claim: the claim reads the OS thread name once (`GetThreadDescription`, `pthread_getname_np`; the platform files of `Zenith2/Docs/Design/DEVT.md` → "ZenithProfiling platform files"), the one place a hook makes a system call beyond the clock, once per thread. On Windows `GetThreadDescription` returns a string the OS allocated with `LocalAlloc`, which the claim copies (at most 63 bytes) and releases with `LocalFree` at once: the only allocation on any hook path, from the OS process heap, never from a Zenith heap, once per thread (§ "Hook implementation").
- Lane order in the timeline: registered lanes in `uLaneIndex` order, then automatic lanes in creation order, then the overflow lane, then GPU lanes.

JOBS keeps the returned value in a `Zenith_ProfilerLaneID` field (row DV10 asks JOBS to retype `m_pxProfilerLane`, an "opaque lane handle or null" pointer in `Zenith2/Docs/Design/JOBS/Algorithms.md`, to that value type, null meaning the registration was refused). Tests: `Test_Profiler_Lanes_RegisterIdempotent`, `Test_Profiler_Lanes_BindByThreadName`, `Test_Profiler_Lanes_IndexZeroBindsCaller`, `Test_Profiler_Lanes_FullReturnsNullAndLogsOnce`, and the integration test `Test_Profiler_JobsLanesRegisteredAtCreate`.

## Frame assembly

`EndFrame(xInputs)` (main thread, frame-loop step 3) in this order: (a) open the zone `Profiler.Assemble` through `Zenith_DispatchZoneBegin` like any other zone (its begin event enters the main thread's ring); (b) read the clock into `ulAssembleStart` and set the main thread's `m_bInProfiler`; (c) run steps 1 to 10 below; (d) clear `m_bInProfiler`, publish `profiler.assemble_us` (GAUGE, `(now − ulAssembleStart) / 1000`); (e) close the zone through `Zenith_DispatchZoneEnd`. Because the zone's begin was drained by this assembly and its end is appended after it, the zone is emitted by the next frame's assembly with its true start and end (the carry rule of step 4), as is every main-thread zone that encloses `EndFrame` (`DevTools.EndFrame`); its statistics therefore land in the following frame's `zoneSum`, and the gauge is the cost row's per-frame measurement ("Profiler cost model"). While the flag is set, a hook that fires on the main thread is ignored (defence in depth, § "Thread slots and the thread-local variable").

1. Take the detailed-frame slot `k % m_uDetailFrames` unless it is held by `Freeze` (then assemble into a scratch slot that is discarded after the summary and statistics are computed); reset its arena. Reset the zone statistics: this is their one reset per frame, made before the frame's first event is drained (§ "Zone statistics"). Reset the frame's lock statistics in the same place, their one reset per frame (§ "Lock statistics").
2. For each claimed thread state, `PopSpan` its ring in chunks of 4,096 events into the assembler buffer and process events in ring order with the thread's open-zone stack (kept in the thread state across frames, depth ≤ 64), attributing them to the state's lane (`m_uLane`, acquire):
   - `ZONE_BEGIN`: push `{ start, site, value 0, flags 0 }`;
   - `ZONE_VALUE`: set the top's value (first) or the multiple-values flag; append to the capture value list when capturing;
   - `ZONE_END`: pop and emit a `Zenith_ProfilerZone` with the thread's lane and the popped depth, and add the closure to its site's statistic (§ "Zone statistics"), as every zone that steps 2 to 4 emit, close or repair is added;
   - `LOCK_WAIT_BEGIN`, `LOCK_ACQUIRED`, `LOCK_RELEASED`, `WAIT_BEGIN`, `BLOCKING_WAIT`: § "Lock and wait events";
   - `LANE_BIND`: change the thread's lane for subsequent zones.

   Before draining a state whose `m_uLane` is 0, assembly binds it (a registered lane of its recorded name, else a new automatic lane, else the overflow lane 64; § "Thread lanes", binding at assembly), so every drained event has a lane and every lane-table write happens here, on the main thread.
3. Repair: a ring that dropped events this frame (its dropped count changed) marks the frame `TRUNCATED`. A dropped begin never leaves an end behind (the hook appends an end only when its begin was pushed, § "Hook implementation"), so the only loss that unbalances the stack is a refused end push, which the hook records in the state's `m_bEndDropped`; after draining the thread's ring, assembly takes the flag with `exchange(false, acq_rel)`, and when it was set, every zone left open on that thread is closed at the frame end with `TRUNCATED_END` and popped (a flag set by a push refused after the drain is taken by the next frame's assembly, which then closes the zones open at that point). Zones left open on any other thread are carried (step 4).
4. Carry: zones still open (legitimately: a background slice, the I/O thread's read) are not emitted; at the next frame they are emitted when they end, with `TRUNCATED_START` when their start precedes that frame's start. A carried zone older than 600 frames is closed with `TRUNCATED_END` (a thread that died with open zones).
5. Sort the frame's zones by (lane, start, depth) — each thread's zones are already ordered by end time; the assembler merges per lane by start with an insertion pass over at most 64 open levels.
6. Merge the GPU frame (§ "GPU timing ingestion").
7. Zone statistics: sort the entries accumulated by steps 2 to 6 by site and publish them (§ "Zone statistics"); lock statistics: hand every claimed thread state's lock bank over, merge the banks into the frame's lock statistics, sort them by site and publish them (§ "Lock statistics"); counter snapshots (§ "Counter registry").
8. Summary: `m_uFrameUs = now − previous EndFrame`; `m_uCpuSimUs = (simulationEnd − iterationStart) / 1000`; `m_uCpuRenderUs = (renderEnd − renderStart) / 1000` (0 when no render ran); `m_uGpuUs` and `m_uGpuFrameLag` from step 6 in either mode; `m_uPresentIntervalUs` from the `Flux.Pacing.IntervalUs` gauge when present; `m_uLockContended` and `m_uLockWaitUs` from step 7's lock statistics (the sums of `m_uContended` and `m_ulTotalWaitNs / 1000` over every entry, each saturating at its field's maximum); flags (hitch when `m_uFrameUs > threshold × 1000`; `EXCLUDED`; `CAPTURE_OFF` in counters-only mode); push into the history ring.
9. Capture: when capturing, append the frame to the current segment (§ "Capture writer and the flush thread").
10. Tracy: when active, the bridge table's `m_pfnFrameMark()` and one `m_pfnPlot(name, frame value)` per counter whose frame value changed (Tracy keeps the name pointer, which the name arena makes stable and NUL-terminated, § "Name tables").

A frame holds at most `uPROFILER_DETAIL_ZONES_PER_FRAME` zones; beyond it zones still contribute to statistics but are not stored, and the frame is `TRUNCATED`.

## Frame history and detailed frames

- History: a ring of `m_uHistoryFrames` × 64-byte `Zenith_ProfilerFrameSummary` (600 × 64 = 37.5 KiB); `GetFrameSummary(n)` is O(1).
- Detailed frames: `m_uDetailFrames` slots, each an arena of `32768 × 32` bytes of zones (1 MiB) plus `4096 × 32` bytes of GPU zones (128 KiB), 1,179,648 bytes per slot: 8 slots are 9,437,184 bytes on Windows; the Player passes 4 on Android (4,718,592 bytes). In counters-only mode a slot holds the frame's GPU zones alone.
- `Freeze(true)` holds every current slot: assembly continues into the scratch slot, so the history, counters and statistics stay live while the timeline shows the frozen frames; `Freeze(false)` releases them.

## Zone statistics

Per frame, for every site that closed at least one zone: count, inclusive total and maximum duration (`Zenith_ProfilerZoneStat`). **A statistic is keyed on the site, never on a zone or a `Zenith_ProfileScope` object, and it accumulates: every closure of a site in a frame, however many times the bracket was entered (in a loop, from several call paths, nested in itself, on several threads), is added to that site's one frame total, and the totals are reset exactly once per frame, at the start of `EndFrame`'s assembly (Z2-R-DEVT-143).**

- The key: the site ID, which is the name the zone was begun with (§ "Hook implementation" step 5): the literal of a `ZENITH_PROFILE_SCOPE`, the dynamic name of a `ZENITH_PROFILE_SCOPE_NAMED` (each distinct dynamic name is a site of its own), `lock:<name>`, `lockwait:<name>` and `wait:<primitive>` for lock hold, lock wait and wait zones (§ "Lock and wait events"; the totals of the `lock:` and `lockwait:` sites come from ring events and can be short when the ring dropped events, where the lock statistics of § "Lock statistics" are authoritative), and a GPU pass name with its view suffix (a site of its own, flagged GPU in the site table). Two source locations that use the same literal share one site and one total; the name table keeps the first one's file and line.
- A closure is a zone the frame's assembly emits: a CPU zone popped at `ZONE_END` (§ "Frame assembly" step 2), a zone closed by repair (step 3) or by the 600-frame carry limit (step 4), a lock hold zone, a lock wait zone, a `wait:` zone (a closure of its measured wait, 0 ns when the wait was reported as 0), and every GPU zone of the GPU frame merged at step 6. A closure belongs to the frame whose `EndFrame` emits it: a carried zone is added once, in the frame it ends, with its whole duration; a zone beyond `uPROFILER_DETAIL_ZONES_PER_FRAME` is added although it is not stored; a zone the hook never recorded (counters-only mode, a refused begin push, a depth overflow) is not a closure.
- `m_uCount` counts every closure, self-nested ones included.
- `m_ulTotalNs` is the inclusive total: a closure's duration includes the zones of other sites nested inside it, and the durations are summed over the site's closures on every thread, so two threads running the site at once add both and a total may exceed the frame time. Recursion is collapsed: a CPU closure that began while another zone of the same site was open on the same thread is self-nested and adds nothing to the total, because its time is already inside the enclosing closure. The total is therefore the time the site was on each thread's zone stack, summed over threads, and no nanosecond is counted twice. Assembly decides self-nesting when it emits the closure, by scanning the thread's open-zone stack below it (at most 63 levels) for the same site; a lock zone is self-nested when a lock zone of the same name is open on its thread. Exclusive (self) time is not a frame statistic; the timeline view shows each zone with its children. GPU zones are flat (depth 0 on their queue's lane), so every GPU closure adds its duration, and the same pass on two queues adds both.
- `m_ulMaxNs` is the longest single closure (for a recursive site, its outermost one).
- Storage: `m_axZoneStats`, a compact array of `uPROFILER_MAX_SITES` `Zenith_ProfilerZoneStat` entries filled in first-touch order, and `m_auZoneStatSlot`, a site-indexed array of `uPROFILER_MAX_SITES` `uint32` slots (0 = the site has no entry this frame, else its entry's index + 1): the 16,384 × 24 and 16,384 × 4 bytes of § "Memory layout and accounting". Adding a closure reads the site's slot, appends `{ site, 0, 0, 0 }` when it is 0, then increments the count, adds the duration to the total unless self-nested, and raises the maximum.
- Reset: at `EndFrame`'s step 1 only, before the frame's first event is drained, the slot of every entry of the previous frame is cleared and the entry count set to 0, which costs O(sites touched). Nothing else resets or overwrites an entry: the second thread state drained, the second 4,096-event chunk of one ring, the repair, the carry and the GPU merge all add into the same entries, and `GetZoneStats` reads without resetting. An `EndFrame` in which no zone closes publishes no entry, and a site that closed zones in one frame and none in the next has no entry in the next (a reader sees 0, never the previous frame's values).
- Publication: step 7 sorts the entries by site (O(t log t) in the t sites touched, never a pass over the table), and `GetZoneStats` returns them until the next `EndFrame`. A reader finds one site by binary search over the span; a site without an entry reads count 0, total 0 and maximum 0. `Freeze` does not hold the statistics: they are computed whether the frame is assembled into its slot or into the scratch slot.
- In counters-only mode no CPU zone is recorded, so the statistics hold only the GPU sites (while `m_bGpuTimings` is set): a CPU site reads 0, and `gpuPassSum` still reads every pass, in either mode.
- Readers: the expression functions of `Zenith2/Docs/Design/DEVT/Overlay.md` → "Expression language" (`zone_ms(name)` the total in milliseconds and `zone_count(name)` the count, each summed over the sites a pattern matches; `zone_max_ms(name)` the largest maximum among them), the overlay's GPU view, the budget-run `zoneSum` source (the summed totals of the named sites) and `gpuPassSum` source (the summed totals of the matched GPU sites) of `Zenith2/Docs/Design/DEVT/Evidence.md` → "Row source bindings", each read once per frame.
- Tests (`Zenith2/Docs/Design/DEVT/Tests.md` → "ZenithProfiling unit tests"): `Test_Profiler_ZoneStats_CountTotalMax`, `Test_Profiler_ZoneStats_LoopAndCallPathsAccumulate`, `Test_Profiler_ZoneStats_TwoThreadsAccumulate`, `Test_Profiler_ZoneStats_RecursiveSiteNotDoubleCounted`, `Test_Profiler_ZoneStats_NextFrameStartsAtZero`, `Test_Profiler_ZoneStats_ResetOncePerFrame` and, with GPU ingestion, `Test_Profiler_ZoneStats_GpuPassesAccumulate`.

## GPU timing ingestion

The hook table has no GPU member ([Z2-N-REND-312] says the timestamps are "published to the on-screen profiler and Tracy"), so GPU timings reach the profiler through the sink interface `Zenith_ProfilerGpuSink` that `Zenith_Profiler` implements:

- Path: the composition root passes the profiler to `Flux_RendererDesc::m_pxGpuTimingSink` as a `Zenith_ProfilerGpuSink*` (null in tests and tools that run no profiler); `Flux_Renderer::Create` registers it with `Flux_FrameLifetime::SetGpuTimingSink(Zenith_ProfilerGpuSink*)` and clears it in its destructor; RHI's `PrepareNextFrame` step 4 and the frames-in-flight change pass the frame lifetime's registered sink as the last argument of the stateless `Flux_PassTimingPublisher::Publish(const Flux_FrameTimestampRecord& xRecord, Zenith_Span<const uint64> xTicks, uint32 uQueue, Zenith_ProfilerGpuSink* pxSink)`, which converts ticks with the queue's period, applies `Zenith_RHIClockCalibration` when valid, builds the `Zenith_ProfilerGpuSubmission` on its stack and calls `pxSink->SubmitGpuTimings` (nothing when `pxSink` is null). The publisher keeps no state and never names `Zenith_Profiler`. Rows DV5 (RPROD: the desc member, the fourth parameter, the revalidation of Z2T-RPROD-029 with `Test_PassTimingPublisher_ZonesNamedWithSuffix` rewritten to a capturing `Zenith_ProfilerGpuSink`) and DV24 (RHI: `SetGpuTimingSink` and the caller passing it, the revalidation of Z2T-RHI-075 with `Test_FrameLifetime_TimestampsPublishedAtSlotReuse` reading a capturing sink instead of a capturing hook table) record the two halves.
- `SubmitGpuTimings(xSubmission)` is accepted in either mode while `m_bGpuTimings` (`devtools.profiler.gpuTimings`) is set, and returns success having done nothing when it is clear: GPU timing ingestion costs one copy per queue per frame, not a per-hook cost, so it does not follow the zone mode. It copies the submission into one of `uPROFILER_GPU_QUEUE_FRAMES` (8) fixed slots, each a 32-byte header `{ uint64 m_ulFrameIndex; uint32 m_uQueue; uint32 m_uPassCount; uint8 m_uCalibrated; uint8 m_auReserved[15]; }` and 4,096 passes of 24 bytes `{ uint64 m_ulSiteHash; uint64 m_ulStartNs; uint64 m_ulEndNs; }`. Slot ownership moves through two CORE `Zenith_SpscRing<uint8>` rings of slot indices over 8-element storage (ruling Q80, row DV23): the free ring, whose producer is `EndFrame` (it returns each drained slot) and whose consumer is `SubmitGpuTimings`; and the ready ring, whose producer is `SubmitGpuTimings` and whose consumer is `EndFrame`. `Create` pushes 0 … 7 into the free ring. `SubmitGpuTimings` pops a free index (none: return `PROFILER_ERROR_GPU_QUEUE_FULL` and count `profiler.gpu_frames_dropped_delta`), fills that slot with plain stores, and pushes the index into the ready ring; the ring's push is the `release` that publishes the slot and `EndFrame`'s pop is the `acquire` that observes it, so the slot's plain bytes are owned by exactly one side at a time. The sink contract's "one caller at a time" (the render-submit thread or the main thread, never concurrently) is what lets `SubmitGpuTimings` be both rings' single role, handed over between threads exactly as the ring's contract allows. Names are hashed and inserted into the site table at submission (the name views are valid only during the call); a site of 0 keeps the pass under `<names full>`.
- `EndFrame`, in either mode, pops every ready index, keeps the submissions whose frame index is the highest seen (pushing the other indices straight back to the free ring), builds GPU zones (lane = the queue's GPU lane, depth 0, `GPU` flag) into the frame's GPU zone list, adds every one of them to the frame's zone statistics under its pass site (§ "Zone statistics": a pass name that occurs several times in the GPU frame, on one queue or on several, accumulates count, total and maximum into one entry; the discarded older submissions add nothing), which the `gpuPassSum` row source and the overlay's GPU view read, sets `m_uGpuUs = max(end) − min(start)` of the graphics queue and `m_uGpuFrameLag`, and then returns those slots' indices to the free ring; an uncalibrated submission is placed so that its first pass starts at that frame's render start, marked by the summary's `GPU_MISSING` bit cleared and a capture flag `uncalibrated`. So a budget run, which disables the profiler's zones (`devtools.profiler.enabled=false`, counters-only), still measures every MP-GPUPASS row from the real timestamps ([Z2-N-BUDGET-107] disables the on-screen profiler, whose overlay stays hidden; the GPU sums are data the row's procedure reads). `Test_Profiler_CountersOnly_GpuTimingsStillIngested` asserts it.
- Tracy: when active, each calibrated GPU zone is forwarded through the bridge table's `m_pfnGpuZone(queue, &m_axTracySourceLocations[site], startNs, endNs)`, which `Zenith_TracyBridge.cpp` realises with Tracy's serial GPU API over the persistent source location (`___tracy_emit_gpu_zone_begin_serial`, `___tracy_emit_gpu_time_serial` for the start and the end, `___tracy_emit_gpu_zone_end_serial`; never an `_alloc_` variant) on the Tracy GPU context of the queue, created by `m_pfnGpuContext` (`___tracy_emit_gpu_new_context` with the period 1 ns and the calibration flag) the first time `EndFrame` forwards a zone of that queue; uncalibrated submissions are not forwarded.

## Lock and wait events

What reaches the hooks. CORE's profiled primitives (`Zenith_Mutex`, `Zenith_ConditionVariable`, `Zenith_Semaphore`) report to the installed hook table; their unprofiled twins (`Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling`, `Zenith_SemaphoreNoProfiling`) never read it (`Zenith2/Docs/Design/CORE/Threading.md`). A profiled `Lock` tries the native lock first. When that succeeds it calls the acquire hook, through `Zenith_DispatchLockAcquire`, with `ulWaitTicks` 0. When the lock is held it reads the clock, blocks, reads the clock again and calls the acquire hook with the measured wait, at least 1 tick (0 means the lock was acquired without blocking). `TryLock` reports a success with wait 0 and calls no hook on failure; `Unlock` calls the release hook with a reference to a local copy of the mutex's desc, because the thread it woke may destroy the object that holds the mutex, so the mutex address passed with it is identity only and no hook dereferences it; a condition-variable wait dispatches the release before the native wait and the acquire, with wait 0, after it returns, so the hold ends at the wait and a new hold begins after it; every blocking call (condition-variable and semaphore waits, `Zenith_Thread::Join`, `Zenith_SleepUs`) calls the blocking-wait hook after it returns, with the primitive's name and the measured wait. A condition variable reports its mutex's `m_szName`, so a wait shows as `wait:<mutex name>`, one site per mutex name; a semaphore reports its optional static-storage name (default `Zenith_Semaphore`); the job park is `jobs.park`. `ulWaitTicks` is the difference of two `Zenith_GetMonotonicTimeNs()` reads, so one tick is one nanosecond and the profiler uses it unconverted ([Z2-N-CONTRACT-143], [Z2-N-CONTRACT-589]).

- Gate. With `captureLocks` on and mode `CAPTURE` the three hooks record; otherwise they return before reading the clock and the frame's lock statistics are empty. No hook looks at a lock's level.
- Acquire (`Zenith_ProfilerOnLockAcquire`): steps 1 and 2 of the zone begin (the in-profiler return, the claim) and the gate; read the clock; update the thread's lock statistics (§ "Lock statistics"); append `LOCK_WAIT_BEGIN` `{ time − wait, "lockwait:<name>" site, level }` when the wait is above 0, then `LOCK_ACQUIRED` `{ time, "lock:<name>" site, level }`, each carrying the thread's zone depth in `m_uDepth`. A refused push is counted like any other.
- Release (`Zenith_ProfilerOnLockRelease`): the same entry steps; read the clock; update the hold statistics; append `LOCK_RELEASED` `{ time, "lock:<name>" site, level }` (a release that matches no held lock appends nothing, § "Lock statistics").
- Blocking wait (`Zenith_ProfilerOnBlockingWait(szPrimitive, ulWaitTicks)`, dispatched after the wait returns): the same entry steps; read the clock; append `WAIT_BEGIN` `{ time − wait, "wait:<primitive>" site }` when the wait is above 0, then `BLOCKING_WAIT` `{ time, "wait:<primitive>" site }`, each built and cached like a `lock:` site.
- Assembly keeps a lock stack of `uPROFILER_HELD_LOCKS_PER_THREAD` (16) entries per thread. `LOCK_ACQUIRED` pushes `{ start, site, depth }` (beyond 16 entries the event is ignored and the frame is `TRUNCATED`); `LOCK_RELEASED` pops the topmost entry of its site and emits a zone named `lock:<name>` (flags 0, depth = the thread's zone depth at acquire), so lock hold times appear on the lane; a release with no entry of its site emits nothing and pops nothing.
- A `LOCK_WAIT_BEGIN` is pending on its thread until that thread's next event. A `LOCK_ACQUIRED` closes it as a zone named `lockwait:<name>` from the begin's time to the acquire's time, at the acquire's depth, so a contended acquire shows its wait beside its hold. A `WAIT_BEGIN` is pending the same way, and only the `BLOCKING_WAIT` that follows it closes it. A `lockwait:` or `wait:` zone whose start precedes the start of the frame it is emitted in (a long park or an idle condition-variable wait) is emitted in the frame of its end with `TRUNCATED_START`, and its full duration is added to the zone statistics of that frame, as a hold is; a frame's `zoneSum` of a `wait:` site can therefore exceed the frame time. Any other event, or a second begin, discards the pending wait and marks the frame `TRUNCATED`.
- A `BLOCKING_WAIT` closes its pending `WAIT_BEGIN` as a zone `wait:<primitive>` of the measured wait; one reported with wait 0 is a zero-length marker (the `"debugger-stop"` primitive of [Z2-N-CONTRACT-365] shows as `wait:debugger-stop`). `lock:`, `lockwait:` and `wait:` zones are closures of their sites and accumulate like any zone (§ "Zone statistics"), so the per-primitive wait statistics, count, total and maximum wait, are the zone statistics of the `wait:<primitive>` sites and need no table of their own.

The profiler can never recurse through its own locks, by construction, in three independent layers. DEVT owns the first and the profiler's part of the other two.

- Types. Every mutex, condition variable and semaphore in ZenithProfiling is an unprofiled type: the capture-segment queue's mutex (level 940, `Zenith_MutexNoProfiling m_xSegmentQueueMutex`), the flush thread's wake-up semaphore (`Zenith_SemaphoreNoProfiling m_xFlushSemaphore`) and any further primitive the profiler adds, such as the idle wait of `SetOutput(nullptr)` (a `Zenith_ConditionVariableNoProfiling` over a `Zenith_MutexNoProfiling`). An unprofiled primitive never reads the hook table and calls no hook, so no lock the profiler takes can reach a hook, whatever its level; the run-time rule "a lock whose level is 940 is never recorded" is withdrawn because the type makes it unnecessary. The lock-order checker still applies to the unprofiled variants, so level 940 keeps its row. `Internal/Capture/Zenith_ProfilerSync.h` declares `Zenith_ProfilerSyncTestAccess`, whose accessors return the profiler's primitives by reference with their declared types, so that `Test_Profiler_OwnPrimitivesAreNoProfiling` can `static_assert` them.
- Gate. PROC's `ForbiddenCheck` rule F26 fails when a non-test source under `Zenith2/Engine/ZenithProfiling/**` (or under `Zenith2/Engine/ZenithCore/Internal/Profiling/**`, or in its `profilerReachable` list) names `Zenith_Mutex`, `Zenith_ConditionVariable` or `Zenith_Semaphore` as a whole token (`Zenith_MutexNoProfiling` passes), and when anything but `Zenith_ProfilerHooks.h` and test files calls a `m_pfnOn…` member of the hook table. Test files are exempt, because a test's subject may be a profiled primitive: the contended-lock and hammer cases lock a real profiled `Zenith_Mutex` and live in `Zenith2/Tests/DevTools/`.
- Run time. Every hook call, the profiler's own included, goes through CORE's dispatch functions, which count a per-thread hook depth and refuse a re-entered hook in every build, asserting in asserts-enabled builds (§ "Hook implementation"). `m_bInProfiler` remains as defence in depth and is not the guarantee (§ "Thread slots and the thread-local variable").

Log sinks. A sink's `Write` runs on the logging thread, which can be inside a hook (a hook that logs, the failure procedure). DEVT's `Zenith_LogFileSink::Write` does nothing (the sink works on the drain thread) and lies under ZenithProfiling in any case; `Zenith_CrashLogMirror::Write` (`Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashLogMirror.cpp` and `.h`) is atomic words alone and takes no lock. Those two mirror files are on PROC's `profilerReachable` list, so they use no profiled primitive.

The tests are `Test_Profiler_OwnPrimitivesAreNoProfiling`, `Test_Profiler_OwnLockNotRecorded` and `Test_Profiler_OwnZonesRecorded`; the lock event cases are `Test_Profiler_LockEvents_OnlyWhenEnabled` and `Test_Profiler_BlockingWait_MarkerZone` (`Zenith2/Docs/Design/DEVT/Tests.md` → "ZenithProfiling unit tests").

## Lock statistics

Per frame, for every lock site used in the frame: the acquisitions, the contended acquisitions, the total and maximum wait and the total and maximum hold (`Zenith_ProfilerLockStat`). A lock site is a lock name: every mutex whose `Zenith_LockDesc::m_szName` has the same text is one site, so a per-object mutex accumulates across its instances, in the per-thread table too: its key is the name pointer and the level, never a desc or mutex address, so 200 instances of one name take one entry (two literals of equal text at different addresses are two entries, which merge by site at `EndFrame`). **A statistic is keyed on the lock site, never on a mutex object, and it accumulates: every use of the site in the frame, however many times, on however many threads and mutex instances, is added to that site's one frame total, and the totals are reset exactly once per frame, at the start of `EndFrame`'s assembly (Z2-R-DEVT-144),** as the zone statistics are (§ "Zone statistics").

- What is counted. An acquisition is one call of the acquire hook, a `TryLock` success included. It is contended when its `ulWaitTicks` is above 0, and its wait is that value in nanoseconds. A hold is the time between the clock read of the acquire hook and the clock read of the release hook for the same thread and mutex address, matched through the held-lock stack below. The acquire hook runs inside the critical section, so a measured hold includes the cost of the acquire hook's own work (tens of nanoseconds with capture on). A condition-variable wait ends the hold at the release it dispatches, and the acquire after the wait starts a new one, so an idle wait is never counted as hold; the wait itself is the `wait:` zone. It is added to the bank the release updates, so a hold that spans an `EndFrame` lands in the frame of its release, while its acquisition was counted in the earlier frame. Counts are `uint32` and saturate; waits and holds are `uint64` nanoseconds.
- When. The same gate as the lock events: `captureLocks` on and mode `CAPTURE`. Otherwise no hook records, the frame has no entry, and the summary's lock fields are 0.
- Where. Each thread state holds a `Zenith_ProfilerThreadLockState` (`uPROFILER_LOCK_STATE_BYTES`, 8,896 bytes), allocated by `Create` with the state; a hook writes only its own thread's lock state, never a shared table.

```cpp
// Zenith2/Engine/ZenithProfiling/Internal/Hooks/Zenith_ProfilerLockBank.h
struct Zenith_ProfilerLockEntry                 // 64 bytes: one lock site of one thread in one bank
{
	const char* m_szName = nullptr;               // 0: the key with m_uLevel: the desc's name pointer (a string literal), never the desc's or the mutex's address
	uint32 m_uHoldSite = 0;                       // 8: the site "lock:<name>"
	uint16 m_uLevel = 0;                          // 12: the other half of the key
	uint16 m_uReserved = 0;                       // 14
	uint32 m_uAcquisitions = 0;                   // 16
	uint32 m_uContended = 0;                      // 20
	uint64 m_ulTotalWaitNs = 0;                   // 24
	uint64 m_ulMaxWaitNs = 0;                     // 32
	uint64 m_ulTotalHoldNs = 0;                   // 40
	uint64 m_ulMaxHoldNs = 0;                     // 48
	uint64 m_ulReserved = 0;                      // 56
};
static_assert(sizeof(Zenith_ProfilerLockEntry) == 64);

struct alignas(64) Zenith_ProfilerLockBank      // 64 + 64 × 64 = 4,160 bytes: a fixed, open-addressed table
{
	std::atomic<uint32> m_uWriters{ 0 };          // owner updates in flight on this bank
	uint32 m_uUsed = 0;                           // entries filled, at most 48 of the 64
	uint32 m_uDropped = 0;                        // updates refused in this bank: a full table, a held-lock stack overflow, a release with no acquire
	uint32 m_uReserved = 0;
	uint8 m_auPad[48] = {};
	Zenith_ProfilerLockEntry m_axEntries[uPROFILER_LOCK_SITES_PER_THREAD];
};
static_assert(sizeof(Zenith_ProfilerLockBank) == 4160);

struct Zenith_ProfilerHeldLock                  // 32 bytes: one lock the thread holds now
{
	const void* m_pLock = nullptr;                // 0: the mutex address the hook received: identity only, never dereferenced
	const char* m_szName = nullptr;               // 8: the name pointer of the acquire's desc (the key, with m_uLevel)
	uint64 m_ulAcquiredNs = 0;                    // 16: the clock read of the acquire hook
	uint32 m_uHoldSite = 0;                       // 24
	uint16 m_uLevel = 0;                          // 28
	uint16 m_uReserved = 0;                       // 30
};
static_assert(sizeof(Zenith_ProfilerHeldLock) == 32);

struct alignas(64) Zenith_ProfilerThreadLockState   // 64 + 2 × 4,160 + 16 × 32 = 8,896 bytes
{
	std::atomic<uint32> m_uActive{ 0 };           // the bank the owner's hooks write
	uint32 m_uHeldCount = 0;                      // the owner alone: entries of m_axHeld in use
	sint32 m_iPendingBank = -1;                   // the main thread alone: the bank handed over and not yet merged, or -1
	uint32 m_auReserved[13] = {};
	Zenith_ProfilerLockBank m_axBanks[2];
	Zenith_ProfilerHeldLock m_axHeld[uPROFILER_HELD_LOCKS_PER_THREAD];
};
static_assert(sizeof(Zenith_ProfilerThreadLockState) == uPROFILER_LOCK_STATE_BYTES);
```

- Acquire update, after the entry steps and the clock read of "Lock and wait events". (1) Enter the active bank: `b = m_uActive.load(seq_cst)`, `m_axBanks[b].m_uWriters.fetch_add(1, seq_cst)`, then `m_uActive.load(seq_cst)` again; when it is no longer `b`, `fetch_sub(1, release)` and repeat on the new bank (the main thread flipped between the two loads, which happens at most once per `EndFrame`). (2) Find the entry for the key `(xDesc.m_szName, xDesc.m_uLevel)` by linear probing from the top six bits of `(name pointer + level) × 0x9E3779B97F4A7C15`; when the key is absent and `m_uUsed` is below 48, insert it: copy the name pointer and level, build `lock:` plus the name in a 64-byte local buffer (the text cut to 63 bytes, ending in `~` when cut), hash it with `Zenith_Hash64`, and resolve the site through the thread's site cache and `FindOrInsert` exactly as the zone begin does (§ "Hook implementation" step 5; a site 0 stays in the entry until its bank is cleared and is retried by the next frame's insert). With 48 entries used and the key absent, `m_uDropped` is incremented, no entry is written, and the hook resolves the site the same way for its events. (3) Add the acquisition, and for a wait above 0 the contended count, the total wait and the maximum wait. (4) Push `{ pLock, xDesc.m_szName, now, site, level }` onto `m_axHeld`; with 16 held locks already, the lock is not tracked and `m_uDropped` is incremented. (5) Leave the bank: `fetch_sub(1, release)`.
- Release update. Scan `m_axHeld` from the top for `pLock`. When it is absent (the acquire predates the capture, was not tracked, or its bank was dropped), enter the bank, increment `m_uDropped`, leave, and append no event. Otherwise remove the entry (later entries shift down), enter the bank, find or insert the entry by the held entry's key and site (the release's desc is a copy of a stack object and is not read) (its `m_uAcquisitions` may be 0: the acquire was counted in an earlier bank), add `now − m_ulAcquiredNs` to the total hold and raise the maximum, leave the bank, and append `LOCK_RELEASED`.
- Handover. A hook and the main thread never touch a bank at the same time, by a writer-count handshake with no lock. For each claimed thread state `EndFrame` step 7 (main thread) does, in order: (A) when `m_iPendingBank` is 0 or 1 (a bank handed over earlier and not yet merged) and that bank's `m_uWriters.load(seq_cst)` is 0, merge and clear it and set `m_iPendingBank` to −1; (B) when `m_iPendingBank` is −1, read `a = m_uActive.load(relaxed)`, `m_uActive.store(1 − a, seq_cst)`, set `m_iPendingBank = a`, and when bank `a`'s `m_uWriters.load(seq_cst)` is 0 merge and clear it and set `m_iPendingBank` to −1. A bank that still has a writer stays pending, `profiler.lock_banks_deferred_delta` counts it, and step (A) of the next frame merges it; the thread meanwhile writes the other bank, so no update is lost and nobody waits. Why it is race-free: a hook increments a bank's writer count and then re-reads `m_uActive`, both `seq_cst`. If the re-read still shows the bank active, the increment precedes the flip in the single order of `seq_cst` operations, so the main thread's later load of `m_uWriters` sees the increment or the `release` decrement that ended it, and defers or finds the owner's plain writes complete. If the re-read shows the flip, the hook backs out and never touches the bank. A bank merged at a writer count of 0 has no writer and gains none until the next flip, which only happens after it is cleared. The hooks never wait: a hook that loses the race against a flip repeats its entry once, on the other bank.
- Merge (main thread, `EndFrame` step 7, after every ring is drained). For each used entry of the handed-over bank, find or append the entry for `m_uHoldSite` in the frame's `m_axLockStats` (a compact array of `uPROFILER_MAX_LOCK_SITES` (512) `Zenith_ProfilerLockStat`, filled in first-touch order, with `m_auLockStatSlot`, an open-addressed table of 1,024 `uint32` slots keyed by site and holding the entry index + 1) and add its acquisitions, contended count, waits and holds (sums saturating for the counts, maxima raised); the first merged entry's level is kept. The bank's `m_uDropped` and the acquisitions of an entry that did not fit the frame table (a 513th site) are added to the frame's dropped count. The bank is cleared afterwards when its `m_uUsed` or `m_uDropped` is non-zero; the clear writes `m_uUsed`, `m_uDropped` and the entries and never `m_uWriters`, which an owner that is backing out may be incrementing and decrementing at that moment.
- Reset. At `EndFrame` step 1 only, the slot table is cleared and the entry count set to 0, so a site that was used in one frame and not in the next has no entry in the next (a reader sees count 0, never the previous frame's values). Nothing else resets or overwrites an entry.
- Publication. Step 7 sorts the entries by site and `GetLockStats` returns them until the next `EndFrame`; a reader finds one site by binary search. `Freeze` does not hold them, as it does not hold the zone statistics. The frame summary's `m_uLockContended` and `m_uLockWaitUs` are their sums (step 8); a capture writes them as the `LOCKS` chunk ("Capture format"); the overlay's lock table draws them (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Timeline view").
- Authority. The banks are the authoritative source of acquisitions, waits and holds. The `lock:` and `lockwait:` zone statistics come from ring events and can be short when the ring dropped events (`profiler.rings.dropped`); the overlay's lock-table tooltip and the `zoneSum` documentation say so.
- Counters, published by `EndFrame` through its own counter path: `profiler.lock_sites` (GAUGE, the entries published), `profiler.lock_stats_dropped_delta` (DELTA, updates refused by a full table, a full held-lock stack, a release with no acquire or a full frame table) and `profiler.lock_banks_deferred_delta` (DELTA, banks whose merge waited a frame).
- Cost. The pair of hooks costs the two clock reads, two ring pushes, two writer-count round trips and two table probes, and the acquire half lengthens the hold it measures: the `Lock_AcquireRelease_Capture` and `Mutex_LockUnlock_Uncontended_*` rows of "Profiler cost model", pending measurement.
- Tests (`Zenith2/Docs/Design/DEVT/Tests.md` → "ZenithProfiling unit tests" and "Integration suites"): `Test_Profiler_LockStats_AccumulateOverUses`, `Test_Profiler_LockStats_ResetAtEndFrame`, `Test_Profiler_LockStats_ContendedWaitAndZone`, `Test_Profiler_LockStats_HoldAcrossFrames`, `Test_Profiler_LockStats_GatedByCaptureLocks`, `Test_Profiler_LockStats_TableOverflowCounted`, `Test_Profiler_LockStats_FrameSummaryFields`, `Test_Profiler_LockStats_ManyInstancesOneEntry`, `Test_Profiler_LockStats_CondVarWaitEndsHold`, `Test_Profiler_BlockingWait_DurationZone`, `Test_Profiler_LockStats_BankHandoverNoLoss`, `Test_Profiler_Capture_LocksChunkRoundTrip`, `Test_Profiler_ContendedLock_ProducesLockWaitZone`, `Test_Profiler_CondVarWait_HoldExcludesWait`, `Test_Profiler_LockStats_HammerOneLock` and `Test_Profiler_NoProfilingLock_NotRecorded`.

## Runtime toggle and modes

| Build | Initial mode | Tracy | Overlay | Capture |
|---|---|---|---|---|
| Debug, Release, Sanitize, Hwasan | `CAPTURE` (`devtools.profiler.enabled` true) | available (not in sanitizer builds); started only with `devtools.tracy.enabled` | hidden by default; toggled at run time | on request |
| Shipping | `COUNTERS_ONLY` (`devtools.profiler.enabled` false; GPU timings follow `devtools.profiler.enabled`, the bullet below) | absent: no Tracy symbol is referenced (`if constexpr (!Zenith_IsShippingBuild())` around every bridge call), so the linker drops the Tracy objects ([Z2-N-ARCH-124]) | hidden; `devtools.profiler.enabled=true` with `devtools.profiler.visible=compact` in the user or command-line layer turns it on (no build flag, Z2-D-005) | `StartCapture` refused with `PROFILER_ERROR_NO_OUTPUT` (Shipping sets no output) |

- `SetMode` switches between the modes at any time from the main thread; the change applies to zones begun after it, and every zone ends as it began (a zone begun in capture mode records its end in counters-only mode; a zone begun in counters-only mode records nothing in capture mode; § "Hook implementation").
- The mode follows `devtools.profiler.enabled` at startup step 7 and on every change of the key (the runtime's configuration callback calls `ApplySettings` with `Zenith_ResolveProfilerSettings(xStore, Zenith_GetBuildConfiguration())`, `Zenith2/Docs/Design/DEVT.md` → "Configuration keys"); outside Shipping GPU timings follow `devtools.profiler.gpuTimings` independently (§ "GPU timing ingestion").
- The on-screen profiler toggle in Shipping is `devtools.profiler.enabled`: it is the key [Z2-N-REND-313] means by "while the on-screen profiler is enabled", and RPROD's `r.graph.timestamps` follows it (the `m_bTimestamps` row of `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphSettings header", row DV33). So in Shipping `Zenith_ResolveProfilerSettings` gives `m_bGpuTimings` the resolved value of `devtools.profiler.enabled` (the `devtools.profiler.gpuTimings` key admits the engine default alone there, [Z2-N-REND-313] leaving no other source), and every change of `devtools.profiler.enabled` re-applies both the mode and the GPU timings: a user who shows the overlay in Shipping (`devtools.profiler.enabled=true`, `devtools.profiler.visible=compact` in the user or command-line layer) gets timestamps inserted by RPROD and ingested here in the same frame, and the GPU view and the top bar's GPU time are filled. Nothing writes a configuration key: the linkage is a resolution rule, data-driven by the configuration value, so `Test_DevToolsConfig_ShippingGpuTimingsFollowEnabled` asserts it in every configuration (it passes `BUILD_CONFIGURATION_SHIPPING` and `BUILD_CONFIGURATION_RELEASE` with each value of the two keys) and `Test_DevToolsConfig_ShippingMasksFromBuildInfo` asserts it in `ShippingTest`.
- In counters-only mode the cost of a zone hook is the slot load, the owner compare, the depth update and the mode load plus CORE's hook-depth enter and leave around the call (the `Zone_BeginEnd_CountersOnly` row of "Profiler cost model", pending measurement), which is what Shipping pays for instrumented code.

| Mode | CPU zones, values | lock and wait events, lock statistics | counters | GPU timings |
|---|---|---|---|---|
| `PROFILER_MODE_CAPTURE` | recorded | recorded while `captureLocks` | aggregated | ingested while `gpuTimings` |
| `PROFILER_MODE_COUNTERS_ONLY` | dropped at the first branch | dropped | aggregated | ingested while `gpuTimings` |
- The "counter-only sink" of startup step 2 ([Z2-N-ARCH-124]) is this profiler in `COUNTERS_ONLY` mode with no Tracy.

## Tracy forwarding

- Manifest options (the ZenithProfiling `options.common` of the `tracy` entry, SHIP's manifest ticket): `TRACY_ENABLE=ON`, `TRACY_ON_DEMAND=ON`, `TRACY_MANUAL_LIFETIME=ON`, `TRACY_DELAYED_INIT=ON`, `TRACY_ONLY_LOCALHOST=ON`, `TRACY_NO_BROADCAST=ON`, `TRACY_NO_CRASH_HANDLER=ON` (DEVT's crash handler owns signals and the unhandled-exception filter), `TRACY_NO_SAMPLING=ON`, `TRACY_NO_SYSTEM_TRACING=ON`, `TRACY_NO_FRAME_IMAGE=ON`, `TRACY_NO_CODE_TRANSFER=ON`, `TRACY_NO_EXIT=OFF`; `linux` options identical (row DV2).
- The bridge table (the test seam). `Internal/Tracy/Zenith_TracyBridge.h` declares the table the profiler calls Tracy through, so no profiler file names a Tracy symbol and a test can substitute recording functions without a second definition of any symbol:

```cpp
// Zenith2/Engine/ZenithProfiling/Internal/Tracy/Zenith_TracyBridge.h
struct Zenith_TracySourceLocation          // 32 bytes; Zenith_TracyBridge.cpp static_asserts the layout equals
{                                          // ___tracy_source_location_data's and passes its address unchanged
	const char* m_szName;                  // the site's NUL-terminated name in the name arena (stable for the profiler's life)
	const char* m_szFunction;              // "" (Tracy requires a non-null pointer)
	const char* m_szFile;                  // the site's file literal, or "" for dynamic and GPU names
	uint32 m_uLine;
	uint32 m_uColour;
};
struct Zenith_TracyBridgeTable             // every member non-null in a table the profiler uses
{
	void (*m_pfnStartup)();                                            // ___tracy_startup_profiler
	void (*m_pfnShutdown)();                                           // ___tracy_shutdown_profiler
	uint64 (*m_pfnZoneBegin)(const Zenith_TracySourceLocation*);      // ___tracy_emit_zone_begin(srcloc, 1), packed context
	void (*m_pfnZoneEnd)(uint64 ulContext);                           // ___tracy_emit_zone_end
	void (*m_pfnZoneValue)(uint64 ulContext, uint64 ulValue);        // ___tracy_emit_zone_value
	void (*m_pfnPlot)(const char* szName, double dValue);              // ___tracy_emit_plot
	void (*m_pfnFrameMark)();                                          // ___tracy_emit_frame_mark(nullptr)
	void (*m_pfnThreadName)(const char* szName);                       // ___tracy_set_thread_name, on the calling thread
	void (*m_pfnGpuZone)(uint32 uQueue, const Zenith_TracySourceLocation*, uint64 ulStartNs, uint64 ulEndNs);
	void (*m_pfnGpuContext)(uint32 uQueue);                           // ___tracy_emit_gpu_new_context, once per queue
};
// Zenith_TracyBridge.cpp, the only file that includes TracyC.h; referenced only inside if constexpr (!Zenith_IsShippingBuild()).
const Zenith_TracyBridgeTable& Zenith_GetTracyBridgeTable();
// Tests only: installs xTable before ApplySettings starts Tracy (friend of Zenith_Profiler). Defined in
// Internal/Tracy/Zenith_ProfilerTestAccess.cpp, which includes no Tracy header, so a ShippingTest test that calls it never
// pulls Zenith_TracyBridge.cpp (and its ___tracy_* references) into a link where Tracy is not linked.
class Zenith_ProfilerTestAccess
{
public:
	static void SetTracyBridge(Zenith_Profiler& xProfiler, const Zenith_TracyBridgeTable& xTable);
};
```

  `Create` copies `Zenith_GetTracyBridgeTable()` into the profiler's member table inside `if constexpr (!Zenith_IsShippingBuild())`, so a Shipping or `ShippingTest` build references no bridge function and the linker drops `Zenith_TracyBridge.cpp` with Tracy (which `$<CONFIG:Shipping,ShippingTest>` does not link, `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "ShippingTest configuration"). Every call through the table is also inside `if constexpr (!Zenith_IsShippingBuild())`. A test's recording table therefore links in every configuration, and in `ShippingTest` it records no call at all. No test references a bridge function or a Tracy symbol outside `if constexpr (!Zenith_IsShippingBuild())`.
- Source locations: Tracy keeps the source-location pointer of every zone until the zone has been sent, so each site has one persistent `Zenith_TracySourceLocation` in `m_axTracySourceLocations[uPROFILER_MAX_SITES]` (16,384 × 32 = 524,288 bytes, allocated by `Create` when `m_bTracyAvailable` is true, never otherwise), filled by the winner of `FindOrInsert` before it publishes the entry (§ "Name tables": the arena name, the file literal, the line, the colour), and site 0's (`<names full>`) by `Create`. The begin hook passes `&m_axTracySourceLocations[site]` to `m_pfnZoneBegin`, the non-allocating `___tracy_emit_zone_begin`, so the profiler allocates nothing per zone; the allocating `___tracy_alloc_srcloc_name` path is never used.
- Start: `ApplySettings` with `m_bTracyEnabled` and the desc's `m_bTracyAvailable` calls `m_pfnStartup` once, calls `m_pfnGpuContext` lazily per queue at the first calibrated GPU zone, and stores `m_bTracyActive = true` (release). Tracy is never stopped mid-run; `~Zenith_Profiler` calls `m_pfnShutdown` after the flush thread is joined.
- Thread names: a thread claimed while Tracy is active names itself at claim (`m_pfnThreadName` with the registered lane's name when the claim bound one, else its recorded OS name, from the claim path, with `m_bInProfiler` set, so no profiler hook re-enters); a thread claimed before Tracy started names itself at its first zone begin after the start, once (`m_bTracyNamed` in the thread state). Tracy copies the name with its own allocator, which, like its per-thread event queue, is Tracy's memory, outside the Zenith heap and outside Z2-B-DEVT-001, and exists only while Tracy runs: never in a budget run (the runner passes `devtools.tracy.enabled=false`), in a sanitizer build or in Shipping.
- Zones: begin `m_pfnZoneBegin(&srcloc)`; values `m_pfnZoneValue`; ends `m_pfnZoneEnd`; counters `m_pfnPlot`; frames `m_pfnFrameMark`; log records are not forwarded.
- Security: Tracy's listener binds 127.0.0.1 alone (`TRACY_ONLY_LOCALHOST`), starts only when `devtools.tracy.enabled` is set on the command line, is absent from Shipping, and is reached on the S24 through `adb forward tcp:8086 tcp:8086`; its protocol carries no session token, which is the exception to [Z2-N-ARCH-959] recorded as row DV9.
- `___tracy_startup_profiler` and `___tracy_shutdown_profiler` are entries of `shipping_forbidden_symbols.json` (row DV15).

## Capture format

A capture is one or more segment files `user:/devtools/captures/<captureID>/part-<NNNN>.zprof` (NNNN the decimal sequence from 0000), each self-contained: a reader can parse any segment alone, and `ZenithDevTool capture merge` concatenates them.

Segment header (96 bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `5A 50 52 46` ("ZPRF") |
| 4 | 2 | format version, 1 |
| 6 | 2 | header bytes, 96 |
| 8 | 4 | segment sequence |
| 12 | 4 | flags: bit 0 last segment, bit 1 events were dropped, bit 2 uncalibrated GPU data present, bit 3 lock events and lock statistics present |
| 16 | 16 | capture ID (128 random bits, identical across the capture's segments, from `Zenith_RandomSource` seeded by the OS entropy call of CORE) |
| 32 | 8 | composition hash |
| 40 | 8 | build identity |
| 48 | 8 | monotonic base ns (the capture's first event) |
| 56 | 8 | UTC microseconds at capture start |
| 64 | 8 | first frame index in the segment |
| 72 | 8 | last frame index in the segment |
| 80 | 1 | platform (`Zenith_BuildPlatform`) |
| 81 | 1 | RHI backend: 0 none, 1 Vulkan, 2 D3D12 |
| 82 | 2 | reserved, 0 |
| 84 | 4 | chunk count |
| 88 | 8 | chunk bytes (the file size minus 96) |

Chunks follow, each `{ uint32 kind; uint32 bytes; payload; zero padding to 8 bytes }` (bytes excludes the padding):

| Kind | Name | Payload |
|---|---|---|
| 1 | `STRINGS` | `uint32 count`, then per string `{ uint32 id; uint8 table (1 site, 2 counter, 3 lane); uint8 reserved; uint16 length; bytes }` — every name the segment's other chunks reference |
| 2 | `LANES` | `uint32 count`, then `{ uint16 lane; uint8 kind; uint8 reserved; uint32 nameId; uint64 osThreadId }` (16 bytes each) |
| 3 | `FRAME` | the 64-byte summary, `uint32 zoneCount`, `uint32 gpuZoneCount`, then the zones and the GPU zones (32 bytes each) |
| 4 | `COUNTERS` | `uint64 frameIndex`, `uint32 count`, `uint32 reserved`, then `{ uint32 nameId; uint8 kind; uint8 reserved[3]; sint64 frameValue; sint64 latest }` (24 bytes each), for counters whose frame value or latest changed |
| 5 | `LOG` | `uint32 count`, then `{ uint64 monotonicUs; uint8 level; uint8 reserved; uint16 length; uint32 categoryNameId; bytes }` — log lines of the captured frames, from the runtime's ring sink |
| 6 | `MARKER` | `uint64 ns`, `uint32 nameId`, `uint32 reserved` — `devtools.marker <text>` console markers |
| 7 | `MEMORY` | `uint64 frameIndex`, then `Zenith_MemoryGovernorStats` field by field (level u8, pad 7, process, device-local, unattributed u64, 8 subsystems × {resident, budget, reserved} u64, 3 peaks u64, 4 event counts u64) — once per 60 frames and at every level change |
| 8 | `ZONE_VALUES` | `uint64 frameIndex`, `uint32 count`, `uint32 reserved`, then `{ uint32 zoneIndex; uint32 valueIndex; uint64 value }` for every value beyond the first |
| 9 | `LOCKS` | `uint64 frameIndex`, `uint32 count`, `uint32 dropped`, then `{ uint32 nameId; uint16 level; uint16 reserved; uint32 acquisitions; uint32 contended; uint64 totalWaitNs; uint64 maxWaitNs; uint64 totalHoldNs; uint64 maxHoldNs }` (48 bytes each, the layout of `Zenith_ProfilerLockStat`; `nameId` is the string id of the site `lock:<name>`, table 1) — the frame's lock statistics, once per captured frame that has an entry or dropped updates; `dropped` is the frame's `profiler.lock_stats_dropped_delta` |
| 255 | `END` | empty; the last chunk of a complete segment |

`Zenith_ProfilerCapture.h` declares `uPROFILER_CAPTURE_MAGIC`, `uPROFILER_CAPTURE_VERSION = 1`, the chunk kind enumeration `Zenith_ProfilerChunkKind` and `Zenith_ProfilerCaptureReader` (`Open(Zenith_ByteSpan)` validating header, lengths and every count against the bytes with overflow-checked arithmetic, returning `PROFILER_ERROR_CAPTURE_CORRUPT` or `PROFILER_ERROR_CAPTURE_VERSION`; `NextChunk(Zenith_ProfilerChunk& xOut)` returning kind and payload span; typed decoders per chunk). A segment without `END` is a truncated segment (a crash mid-write cannot produce one, because each segment is written atomically, but a corrupt file can): the reader returns its chunks up to the first malformed one and reports `truncated`. Unknown chunk kinds are skipped. A format change that alters an existing chunk's layout bumps the format version; a new chunk kind does not. `LOCKS` (kind 9) is a new chunk kind, and the frame summary's two lock fields (`m_uLockContended` at byte 58, `m_uLockWaitUs` at byte 60) occupy bytes that were reserved, so no existing chunk changes layout and the version stays 1: a reader that does not know chunk 9 skips it by length and reads the summary's two fields as reserved. The reader's typed decoder for the chunk is `DecodeLocks`. Memory snapshots (chunk 7) are the memory-profiling record of Z2-I-DEVT-16.

## Capture writer and the flush thread

- `StartCapture(uFrames, bWithLocks)` (main thread; also from the console `profiler.capture <frames>` and the remote `CAPTURE_START`): refuses a running capture and a null output; enables lock events and lock statistics when asked; draws the capture ID; from the next `EndFrame`, each frame's chunks are appended to the segment buffer (a 16 MiB block from the profiler's reserved capture arena, two blocks alternating).
- When a block reaches 16 MiB, or the capture's last frame is reached, `EndFrame` appends `STRINGS` (every name referenced since the segment began, from a 16,384-bit referenced-name bitmap per table) at the end, then `END`, finalises the header and pushes `{ block, size, sequence }` onto the segment queue (capacity 2, `QUEUE_FULL_POLICY_REJECT`, guarded by the `Zenith_MutexNoProfiling` at lock level 940): when the queue is full (the flush thread still writing two segments), the capture stops with `profiler.capture: stopped: writer behind` and the segment is dropped.
- `Zenith.ProfFlush` (a `Zenith_Thread`, created by `Create` unless `m_bStartFlushThread` is false, priority below normal, stack 256 KiB) waits on its `Zenith_SemaphoreNoProfiling`, pops a segment, calls `Zenith_ProfilerOutput::WriteFile("captures/<id>/part-<NNNN>.zprof", bytes)`, returns the block, counts `profiler.capture.segments_delta` and bytes; a refused write stops the capture. It emits the zone `Profiler.FlushSegment` around each segment through `Zenith_DispatchZoneBegin` and `Zenith_DispatchZoneEnd`, and sets `m_bInProfiler` around the `WriteFile` call alone (defence in depth, § "Thread slots and the thread-local variable"); none of its primitives is profiled (§ "Lock and wait events"). The JSON Lines file sink does not use this thread: it writes from the log router's drain thread (`Zenith2/Docs/Design/DEVT/Console.md` → "Log file sink").
- `StopCapture` closes the current segment at the next `EndFrame` as the last segment.
- Reading: the overlay never reads captures; `ZenithDevTool` and the remote client do.

## Memory layout and accounting

Every figure is in bytes; the totals are converted to MB as `Zenith2/Docs/Budgets.md` defines it (10^6 bytes, [Z2-N-BUDGET-141]); KiB and MiB in descriptions are 1,024 and 1,048,576 bytes.

| Buffer (profiler) | Windows (desc: 48 thread states, 16,384-event rings, 8 detailed frames) | Android (32, 8,192, 4) |
|---|---|---|
| thread states: states × (ring × 16 bytes + `uPROFILER_THREAD_STATE_BYTES` 8,192: the 256-entry site cache 4,096, the 64-entry counter cache 1,024, the open-zone stack 64 × 24 = 1,536, the Tracy stack 512, masks, value counts and fields + `uPROFILER_LOCK_STATE_BYTES` 8,896: two lock banks of 4,160, the held-lock stack 16 × 32 = 512 and 64 bytes of fields) | 48 × (262,144 + 8,192 + 8,896) = 13,403,136 | 32 × (131,072 + 8,192 + 8,896) = 4,741,120 |
| name tables: (16,384 + 4,096 + 64) × 32 bytes of entries + the `uPROFILER_NAME_ARENA_BYTES` arena, 20,544 × 64 = 1,314,816 | 1,972,224 | 1,972,224 |
| Tracy source locations 16,384 × 32 bytes (allocated when `m_bTracyAvailable`, which `Zenith_GetPlayerProfilerDesc` sets in every non-Shipping, non-sanitizer build, budget runs included; 0 in Shipping) | 524,288 | 524,288 |
| counter cells 4,096 × 64 bytes and histories 4,096 × 128 × 8 bytes | 4,456,448 | 4,456,448 |
| frame history 600 × 64 bytes and zone statistics 16,384 × 24 bytes | 431,616 | 431,616 |
| lock statistics: `m_axLockStats` 512 × 48 bytes and the slot table `m_auLockStatSlot` 1,024 × 4 bytes | 28,672 | 28,672 |
| detailed frames: slots × (32,768 × 32 + 4,096 × 32) | 8 × 1,179,648 = 9,437,184 | 4 × 1,179,648 = 4,718,592 |
| GPU slots 8 × (32-byte header + 4,096 × 24 bytes) and the storage of the two 8-element `Zenith_SpscRing<uint8>` index rings (2 × 8) | 786,704 | 786,704 |
| assembler: the 4,096-event pop buffer (65,536) and the zone statistics' site-to-entry slots 16,384 × 4 (65,536) | 131,072 | 131,072 |
| lane table 64 × 24 bytes ("Thread lanes") | 1,536 | 1,536 |
| capture-segment queue: a `Zenith_RingBuffer` of 2 entries of 24 bytes `{ block, size, sequence }` in the `Create` block | 48 | 48 |
| **profiler resident (`Zenith_Profiler::GetResidentBytes`)** | **31,172,928** | **17,792,320** |
| capture arena: two 16 MiB blocks | 33,554,432 while a capture runs, else 0 | likewise |

The profiler's figure is one part of Z2-B-DEVT-001. The row covers every buffer DEVT keeps resident in a running Player, each from the part of this family that sizes it:

| Part of Z2-B-DEVT-001 | Windows | Android | Sized in |
|---|---|---|---|
| profiler | 31,172,928 | 17,792,320 | the table above |
| debug-draw recorder (main ring, other rings, staging, persistent table with its 8,192 × 8-byte deadlines; ruling Q79 path (b) adds its slot table, 128 × 16 = 2,048 bytes on Windows and 64 × 16 = 1,024 on Android, inside the headroom) | 7,479,296 | 4,988,928 | `Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Recorder rings and drain" |
| console (the 64-entry line queue, a CORE `Zenith_BoundedQueue` of 1,088-byte cells: 69,632 from the root heap; 256 × 512 history; 1,024 × 64 registry slots; the 1,024 × 2 sorted index; 64 KiB name arena; 64 KiB output; 1 KiB line) | 400,384 | 400,384 | `Zenith2/Docs/Design/DEVT/Console.md` → "Command registry" |
| overlay (256 KiB layout arena, 128 KiB frame arena, 64 KiB expression arena) | 458,752 | 458,752 | `Zenith2/Docs/Design/DEVT/Overlay.md` → "Overlay drawing" |
| log file sink (two 256 KiB chunks) | 524,288 | 524,288 | `Zenith2/Docs/Design/DEVT/Console.md` → "Log file sink" |
| run telemetry (the 256-event queue, a CORE `Zenith_BoundedQueue` of 192-byte cells: 49,152 from the root heap; 512 KiB frame CSV buffer; 16 KiB run values) | 589,824 | 589,824 | `Zenith2/Docs/Design/DEVT/Evidence.md` → "Run telemetry" |
| crash handler (annotations 64 × 168, breadcrumbs 64 × 112, the log mirror's 65,536 bytes of words and 8-byte cursor, the 64 KiB report buffer `uCRASH_REPORT_BUFFER_BYTES`, kept at 64 KiB because the Android tail truncates with `"truncated": true` rather than growing (`Zenith2/Docs/Design/DEVT/Crash.md` → "Signal-safe report tail"), registration table 64 × 64 (the stack bounds included), the 272-byte fatal slot: 153,368; Windows: the 64 KiB shared section; Android: 32 × 64 KiB alternate signal stacks, a registration beyond the 32nd getting none) | 218,904 | 2,250,520 | `Zenith2/Docs/Design/DEVT/Crash.md` → "Annotations and breadcrumbs" |
| **total** | **40,844,376 bytes = 40.84 MB** | **27,005,016 bytes = 27.01 MB** | |
| **proposed value of Z2-B-DEVT-001** | **44 MB** (7.7% headroom) | **29 MB** (7.4% headroom) | `Zenith2/Docs/Design/DEVT.md` → "Rows DEVT owns" |

Buffers allocated only on demand are excluded from the row, because none of them exists during a budget run and each is accounted while it lives: the capture arena (`StartCapture`; a budget run never captures, the runner disables the profiler's zones), development-file writes (allocated per write and freed at completion; no console command runs in a budget run), the remote server's per-connection rings (`devtools.remote.enabled` is false in budget runs) and the budget-run record (the measurement apparatus itself, a separate consumer whose size the run telemetry reports as `budgetRun.recordBytes`; `Zenith2/Docs/Design/DEVT/Evidence.md` → "Zenith_BudgetRunRecord header"). The row's procedure note in "Rows DEVT owns" states the exclusion.

Every profiler buffer except the capture arena is allocated in `Create`; the capture arena is allocated by `StartCapture` from the heap (`Zenith_TryNew`; failure is `PROFILER_ERROR_OUT_OF_MEMORY` and no capture) and freed when the capture ends. `GetResidentBytes` returns the sum of live buffers; it does not count the `Zenith_Profiler` object itself (the first allocation `Create` makes, `sizeof(Zenith_Profiler)` bytes of private members whose size no table row carries), and every other DEVT consumer's `GetResidentBytes` follows the same rule for its own object.

Accounting rule. Each `GetResidentBytes` is the exact sum of its object's live buffers, computed from the same constants the tables use, and a `Zenith_BoundedQueue` counts at the size of its one root-heap allocation: CORE's cells hold an 8-byte sequence and the element, aligned to `max(alignof(T), 64)` (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue"), so a cell is `roundup(8 + sizeof(T), 64)` bytes (1,088 for the console's `Zenith_DevConsoleQueuedLine` of 1,028 bytes, 192 for the 128-byte `Zenith_TelemetryEvent`). The owner reads that size from the queue's `GetAllocatedBytes()` <!-- assumption: pending CORE --> a `uint64 GetAllocatedBytes() const` member of `Zenith_BoundedQueue<T>` returning the bytes of its one allocation (row DV34) <!-- /assumption -->, never from its own copy of CORE's layout. `Test_Profiler_MemoryConsumer_ResidentBytesMatchesAllocations` asserts, for the Windows and the Android desc of `Zenith_GetPlayerProfilerDesc`, that `GetResidentBytes` equals the sum the test computes from the desc and the constants of `Zenith_ProfilerTypes.h` component by component (the rows of the first table above) and equals the bytes the test's recording heap recorded for the profiler minus the one allocation of `sizeof(Zenith_Profiler)` bytes (the object, which the rule above excludes), with no literal total in the test; `Test_DevTools_MemoryGaugeSumsConsumers` asserts the same for `Zenith_DevToolsMemoryConsumer`, its queues included through `GetAllocatedBytes()`.

Attribution (the MP-SUBMEM rule for a component inside a shared subsystem): DEVT's buffers live in `MEMORY_SUBSYSTEM_OTHER`, which other areas share, and `Zenith_MemoryConsumer` carries no name, so the governor's subsystem total cannot isolate them. DEVT therefore registers exactly two consumers, both `MEMORY_SUBSYSTEM_OTHER`: the profiler itself (registered by the composition root at startup step 8) and `Zenith_DevToolsMemoryConsumer` (ZenithRuntime, `Internal/DevTools/Zenith_DevTools.cpp`), whose `GetResidentBytes` sums the recorder's, the console's, the overlay's, the file sink's, the telemetry's and the crash handler's `GetResidentBytes`. `Zenith_DevTools::EndFrame` publishes the GAUGE `devtools.memory_bytes`, the sum of both consumers' resident bytes, every frame; Z2-B-DEVT-001's row source is that gauge (`Zenith2/Docs/Design/DEVT/Evidence.md` → "Row source bindings", kind `memorySubsystem` with `component`), and its statistic is the maximum over the run. Both consumers' `OnPressureLevelChanged` shed nothing (the subsystem's row of `Zenith2/Docs/Architecture.md` → "Memory governor" is "none / none"); the case row in `axMEMORY_SHEDDING_CASES` asserts that the gauge is unchanged at WARNING and CRITICAL (row DV20). `Test_Profiler_MemoryConsumer_ResidentBytesMatchesAllocations` and `Test_DevTools_MemoryGaugeSumsConsumers` assert the two sums.

## Creation and shutdown

Player (`Zenith2/Docs/Design/MOD.md` → "Player headers", row DV12), in order:

1. step 1: the root heap;
2. step 1b: `Zenith_LogRouter::Create`;
3. step 2: `Zenith_Profiler::Create(Zenith_GetPlayerProfilerDesc(Zenith_GetBuildPlatform(), Zenith_GetBuildConfiguration(), Zenith_IsSanitizerBuild()))` (§ "Zenith_Profiler header": `COUNTERS_ONLY` and no Tracy in Shipping, Tracy unavailable in sanitizer builds, 48 or 32 thread states, 16,384- or 8,192-event rings, 8 or 4 detailed frames); on failure write `Zenith2: profiler creation failed: <error>`, destroy the router, uninstall the heap, return 4; then `xProfiler.FillHooks(xHooks)`, `xRouter.FillHooks(xHooks)`, `Zenith_InstallProfilerHooks(&xHooks)`; the Player registers no lane itself, because `Zenith_JobSystem::Create` registers `Z2 Main` as lane index 0, which binds the main thread (§ "Thread lanes");
4. `Zenith_RuntimeMain(…, xProfiler, …)` (row DV3; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" carries the signature after X245 (a));
5. after the runtime returns: destroy the router, `Zenith_InstallProfilerHooks(nullptr)`, destroy the profiler (joins `Zenith.ProfFlush`), uninstall and destroy the heap.

Inside the runtime (row DV12): startup step 7 calls `SetOutput(&xDevToolsOutput)` and `ApplySettings`; startup step 8 registers the profiler as a memory consumer; shutdown step 4 (`Zenith_DevTools::BeginShutdown`) stops a running capture and writes its last segment synchronously; MOD's shutdown step 6, before `Zenith_AsyncIO` is destroyed (Contracts step 8), calls `Zenith_DevTools::DetachOutputs`, which calls `SetOutput(nullptr)` (the detach waits until `Zenith.ProfFlush` is idle, so no `WriteFile` can run after the output is gone) together with the file sink's and telemetry's detach (`Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown"); the profiler itself outlives the runtime.

`Create` steps with their fault points (`uFAULT_POINT_PROFILER_*`, values `0x44560001` … `0x44560006`, high half 'DV'): (1) validate the desc; (2) allocate the thread states and rings; (3) the name tables and arena; (4) the counter cells and histories; (5) the frame stores and GPU queue; (6) start `Zenith.ProfFlush`. The segment queue's mutex and the flush thread's semaphore are the unprofiled `Zenith_MutexNoProfiling` and `Zenith_SemaphoreNoProfiling` ("Lock and wait events"). Each failure unwinds what earlier steps built (members are owners) and returns its error; each step has `Test_Profiler_Create_FailsAt<Step>_Unwinds` ([Z2-N-CONTRACT-341]).

`~Zenith_Profiler`: asserts the hooks no longer point at it (`Zenith_GetProfilerHooks().m_pUserData != this`, ZENITH_ASSERT); stops a capture without writing; wakes and joins the flush thread; shuts Tracy down; frees every buffer; clears the main thread's slot when it names this profiler.

## Profiler cost model

The figures for the hook rows below were set before CORE's dispatch functions existed: every hook call now also pays the hook-depth enter and leave around it, and a lock or unlock pays two dispatches even when nothing is captured. They are design estimates and stay `pending measurement` until `Zenith_Bench_ProfilerOverhead` runs on the reference desktop and the S24 and sets them; the estimate is kept in brackets. A measured hold includes the cost of the acquire hook, which runs inside the critical section.

| Operation | Target on the reference desktop | Target on the S24 | Measured by |
|---|---|---|---|
| zone begin + end, capture mode, cached site | pending measurement (estimate ≤ 45 ns) | pending measurement (estimate ≤ 90 ns) | `Zenith_Bench_ProfilerOverhead`, case `Zone_BeginEnd_Capture` |
| zone begin + end, counters-only mode | pending measurement (estimate ≤ 4 ns) | pending measurement (estimate ≤ 8 ns) | the same, `Zone_BeginEnd_CountersOnly` |
| counter, cached name | pending measurement (estimate ≤ 25 ns) | pending measurement (estimate ≤ 50 ns) | the same, `Counter_Cached` |
| lock acquire hook + release hook, capture mode with `captureLocks`, uncontended, cached site | pending measurement (estimate ≤ 140 ns) | pending measurement (estimate ≤ 280 ns) | the same, `Lock_AcquireRelease_Capture` (`Zenith_DispatchLockAcquire` and `Zenith_DispatchLockRelease` on a static `Zenith_LockDesc`) |
| `Zenith_Mutex` lock + unlock, uncontended, no hook table installed | pending measurement | pending measurement | the same, `Mutex_LockUnlock_Uncontended_NoTable` (a try-lock, a call to read the table and a branch per side) |
| the same with the profiler installed and `captureLocks` off | pending measurement | pending measurement | the same, `Mutex_LockUnlock_Uncontended_GateOff` (two dispatches that return at the gate) |
| the same with capture on | pending measurement | pending measurement | the same, `Mutex_LockUnlock_Uncontended_Capture` (the two hooks, the acquire half inside the critical section) |
| `EndFrame` assembly at 10,000 zones, 8 threads, 500 counters | ≤ 0.25 ms | ≤ 0.60 ms | the gauge `profiler.assemble_us` (§ "Frame assembly"), and the harness case `EndFrame_10000Zones` |
| overlay build, full state | ≤ 0.10 ms | ≤ 0.25 ms | the zone `Profiler.Overlay` |

The harness `Zenith2/Benchmarks/ProfilerOverhead/` (`zenith_add_benchmark(ProfilerOverhead DEPENDS ZenithProfiling ZenithCore)`, nanobench) runs the cases above with 1, 2, 8 and all threads hammering one profiler, plus the cases that measure the acceptance criteria of the name table's justification record (`Zenith2/Docs/Design/DEVT.md` → "Justification records DEVT adds"), each against its standard alternative under the same conditions:

| Case | Measures | Alternative case |
|---|---|---|
| `NameTable_Lookup_Resident` | median ns of `FindOrInsert` for a name already published, 4,096 resident names, 1, 2, 8 and all threads | `HashMapMutex_Lookup_Resident`: CORE's `Zenith_HashMap<uint64, uint32>` under a `Zenith_Mutex`, the same names and threads |
| `NameTable_Insert_Distinct` | median ns per insert of 16,384 distinct names from 8 threads | `HashMapMutex_Insert_Distinct` |
| `NameTable_LookupUnderHeldLock` | lookups from 8 threads each holding a `Zenith_Mutex` of level 990 (`uLOCK_LEVEL_HEAP_LEAK_TABLE`) in a Debug-checked build, reporting the lock-order checker's violation count | `HashMapMutex_LookupUnderHeldLock`: the same with the mutex-guarded map at level 940, which reports the violations a mutex inside a hook would cause |

Its CTest `--smoke` runs one iteration of each case. The Z2-B-DEVT-002 row measures the sum of `DevTools.EndFrame`, `DevTools.DrainDebugDraws` and every `DevTools.ExtractDebugDraws@W<tag>`; the per-hook cost is reported beside it from the enabled-versus-disabled run pair ([Z2-N-BUDGET-107]).
