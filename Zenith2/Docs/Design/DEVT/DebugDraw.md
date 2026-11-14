# DEVT design: debug drawing

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It specifies the debug-draw recorder `Zenith_DebugDraw` (ZenithProfiling) and its public record type, its rings, drain and overflow policy, the render-world record `Flux_RenderDebugDraw` (ZenithFlux), the extractor that moves records into the render world, the `DebugOverlay` pass with its two programs, the debug font, and the recorder slot alternative of ruling Q79. Every debug primitive of every area (SCENE's hierarchy view, ANIM's skeletons, AUDIO's emitters, GAME's camera rigs, the on-screen profiler and console) is drawn through this path, so it appears through the pipelined render world (Z2-D-050) with no GPU code outside ZenithFlux.

## Scope of this file

| Concern | Owner | Specified here |
|---|---|---|
| the render-world section member `m_axDebugDraws` and the writer member `AddDebugDraw` | REND (`Zenith2/Docs/Rendering.md` → "The render world hand-off") | the element type and its copying rule (row DV8, X246 (a)) |
| the `DebugOverlay` per-pass table | REND (`Zenith2/Docs/Rendering/PerPass/Common.md` → "DebugOverlay") | the pass body, programs, records, caps (two data fields requested as X246 (b)) |
| the declaration of `Flux_AddDebugOverlay` and the view epilogue that calls it | RPROD (`Zenith2/Docs/Design/RPROD/Passes.md` → "Flux_Features header") | the definition and the call contract |
| the maths and colour types | CORE (`Zenith2/Docs/Design/CORE/Maths.md` → "Vector types", "Matrix types", "Zenith_Quaternion", "Zenith_Colour") | none: cited |
| what each area draws | the area | none |

## Zenith_DebugDraw header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_DebugDraw.h
#pragma once
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithProfiling/Zenith_ProfilerTypes.h"

/// @contract enum Zenith_DebugDrawFlags
/// Ownership: Plain bit flags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Bit values are copied into GPU records; never renumbered.
/// @endcontract
enum Zenith_DebugDrawFlags : uint8
{
	DEBUG_DRAW_FLAG_NONE = 0,
	DEBUG_DRAW_FLAG_DEPTH_TESTED = 1u << 0,   // world primitives dimmed behind scene depth (lines, boxes, spheres, text 3D)
	DEBUG_DRAW_FLAG_SCREEN = 1u << 1,         // pixel coordinates of the target view, origin top-left (set by the Screen* calls)
	DEBUG_DRAW_FLAG_THICK = 1u << 2,          // two-pixel lines instead of one
};

/// @contract functions DebugDrawColour
/// Ownership: Stateless constexpr helper.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: 8-bit sRGB-encoded display values, alpha straight; the packing of CORE's Zenith_ColourToSrgb8Packed
///   (R in bits 0–7, G 8–15, B 16–23, A 24–31).
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
constexpr uint32 Zenith_DebugDrawRgba(uint8 uR, uint8 uG, uint8 uB, uint8 uA)
{
	return static_cast<uint32>(uR) | (static_cast<uint32>(uG) << 8) | (static_cast<uint32>(uB) << 16) | (static_cast<uint32>(uA) << 24);
}

/// @contract enum Zenith_DebugDrawRecordKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_DebugDrawRecordKind : uint8
{
	DEBUG_DRAW_RECORD_LINE = 1,      // m_afA = from, m_afB = to
	DEBUG_DRAW_RECORD_RECT,          // screen: m_afA = x, y, m_afB = width, height
	DEBUG_DRAW_RECORD_TEXT,          // m_afA = position (3D, or screen x, y); the text is m_uTextBytes bytes at m_uTextOffset
};

/// @contract value Zenith_DebugDrawRecord
/// Ownership: Plain value; the recorder copies records into its rings and staging store; the extractor reads them through
///   Zenith_DebugDrawBatchView.
/// Thread affinity: IMMUTABLE; members non-const: written in place into ring, staging and persistent slots, which the
///   drain compacts by assignment.
/// Invalidation: None (a copy); a batch view's records are valid until the next DrainInto.
/// Units: World metres, or pixels when m_uFlags has DEBUG_DRAW_FLAG_SCREEN; m_uDurationMs milliseconds of platform time.
/// Errors: None.
/// Version: since Z2 Stage 2. The 48-byte layout is fixed by the static_assert; a change edits the extractor in the same diff.
/// @endcontract
struct Zenith_DebugDrawRecord        // 48 bytes
{
	float m_afA[3];                  // 0
	float m_afB[3];                  // 12
	uint32 m_uColour;               // 24: Zenith_DebugDrawRgba packing
	uint32 m_uWorldTag;             // 28
	uint32 m_uDurationMs;           // 32: the requested duration, 0 = this frame (the drain turns it into a deadline)
	uint32 m_uTextOffset;           // 36: TEXT: offset into the batch view's m_acText (the staging store's text arena)
	uint16 m_uTextBytes;            // 40: TEXT: at most 255
	uint8 m_uKind;                  // 42: Zenith_DebugDrawRecordKind
	uint8 m_uFlags;                 // 43: Zenith_DebugDrawFlags
	uint8 m_uScale;                 // 44: text scale 1 … 4
	uint8 m_auReserved[3];          // 45
};
static_assert(sizeof(Zenith_DebugDrawRecord) == 48);

/// @contract value Zenith_DebugDrawDesc
/// Ownership: Plain value copied by Create.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Records and bytes.
/// Errors: Create returns PROFILER_ERROR_INVALID_DESC for a value outside its range (record counts must be powers of two).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DebugDrawDesc
{
	const uint32 m_uMainRingRecords = 16384;   // 4,096 … 65,536: the ring of the thread that calls Create (the overlay, the console)
	const uint32 m_uMainRingTextBytes = 65536; // 16 KiB … 1 MiB
	const uint32 m_uThreadRings = 47;          // 3 … 127 rings for the other recording threads (Android: 31)
	const uint32 m_uRingRecords = 1024;        // 256 … 16,384 records per other ring
	const uint32 m_uRingTextBytes = 8192;      // 4 KiB … 256 KiB per other ring
	const uint32 m_uStagingRecords = 32768;    // 4,096 … 65,536 per staging buffer (two buffers; Android: 16,384)
	const uint32 m_uStagingTextBytes = 131072; // per staging buffer
	const uint32 m_uPersistentRecords = 8192;  // records with a duration above one frame, kept by the drain
	const uint32 m_uPersistentTextBytes = 65536;
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract value Zenith_DebugDrawStats
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Records.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DebugDrawStats
{
	const uint32 m_uLines = 0;             // line records in the last drain
	const uint32 m_uRects = 0;             // rectangle records in the last drain
	const uint32 m_uTextRecords = 0;       // text records in the last drain
	const uint64 m_ulDropped = 0;          // records dropped since creation (every cause of "Debug-draw failure handling")
	const uint64 m_ulMainRingDropped = 0;  // of which: the main ring was full
	const uint32 m_uMainRingHighWater = 0; // largest main-ring occupancy, records
	const uint32 m_uRingHighWater = 0;     // largest occupancy of any other ring, records
	const uint32 m_uStagingHighWater = 0;  // largest staging fill, records
};

/// @contract value Zenith_DebugDrawBatchView
/// Ownership: A view into the drain's staging store.
/// Thread affinity: ANY_THREAD for reading while the extractors of one iteration run; written only by DrainInto.
/// Invalidation: Invalid at the second DrainInto after the one that produced it (the store is double-buffered).
/// Units: Records of Zenith_DebugDrawRecord.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DebugDrawBatchView
{
	uint32 m_uWorldTag = 0;                          // 0 = draws for every view (the overlay, the console)
	Zenith_Span<const Zenith_DebugDrawRecord> m_axRecords;
	Zenith_Span<const char> m_acText;                 // the text bytes the records' text offsets index
};

/// @contract type Zenith_DebugDraw
/// Ownership: Created by Create as a Zenith_Owned<Zenith_DebugDraw>, owned by ZenithRuntime's Zenith_DevTools (or the
///   editor's development-tools object, or a test). Owns its rings, the persistent table and the staging store; its state
///   is private members of this class (rings, slot table, staging buffers, statistics), declared in the header's private
///   section by the landing ticket, with no separate implementation class.
/// Thread affinity: ANY_THREAD for every drawing call (each thread writes its own ring); MAIN_THREAD for Create, SetEnabled
///   and destruction; DrainInto on one thread at a time (the drain node); GetBatches while no DrainInto runs (the extractors
///   run after the drain node); GetStats and GetResidentBytes ANY_THREAD (relaxed reads).
/// Invalidation: None.
/// Units: World primitives in metres in world space; screen primitives in pixels; durations in seconds of platform time.
/// Errors: Drawing calls never fail; a record that does not fit is dropped and counted ("Debug-draw failure handling").
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks.
/// Locks: none; per-thread single-producer rings (CORE's Zenith_SpscRing).
/// Version: since Z2 Stage 2. Source API. Present in Shipping (the overlay draws through it); every drawing call returns
///   at its first branch while no consumer has enabled it (SetEnabled).
/// Example: Zenith2/Engine/ZenithProfiling/Tests/Examples/Zenith_DebugDraw_Example.cpp
/// @endcontract
class Zenith_DebugDraw
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DebugDraw>, Zenith_ProfilerError> Create(const Zenith_DebugDrawDesc& xDesc);
	~Zenith_DebugDraw();

	void SetEnabled(bool bEnabled);                 // main thread; false drops every call at its first branch
	bool IsEnabled() const;

	// World primitives. uWorldTag selects the world section; fDuration 0 = this frame only; uColour in Zenith_DebugDrawRgba packing.
	void Line(uint32 uWorldTag, const Zenith_Vector3& xFrom, const Zenith_Vector3& xTo, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Point(uint32 uWorldTag, const Zenith_Vector3& xPosition, float fSize, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);   // a three-axis cross of fSize metres
	void Arrow(uint32 uWorldTag, const Zenith_Vector3& xFrom, const Zenith_Vector3& xTo, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Box(uint32 uWorldTag, const Zenith_Vector3& xCentre, const Zenith_Vector3& xHalfExtents, const Zenith_Quaternion& xRotation,
		uint32 uColour, Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Aabb(uint32 uWorldTag, const Zenith_Vector3& xMin, const Zenith_Vector3& xMax, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Sphere(uint32 uWorldTag, const Zenith_Vector3& xCentre, float fRadius, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Capsule(uint32 uWorldTag, const Zenith_Vector3& xA, const Zenith_Vector3& xB, float fRadius, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Axes(uint32 uWorldTag, const Zenith_Matrix4& xTransform, float fLength,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Frustum(uint32 uWorldTag, const Zenith_Matrix4& xInverseViewProjection, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);
	void Text3D(uint32 uWorldTag, const Zenith_Vector3& xPosition, Zenith_StringView strText, uint32 uColour,
		Zenith_DebugDrawFlags eFlags = DEBUG_DRAW_FLAG_NONE, float fDuration = 0.0f);   // screen-aligned, 1 cell high

	// Screen primitives for the view(s) of uWorldTag (0 = every main and editor view).
	void ScreenLine(uint32 uWorldTag, float fX0, float fY0, float fX1, float fY1, uint32 uColour);
	void ScreenRect(uint32 uWorldTag, float fX, float fY, float fWidth, float fHeight, uint32 uColour);
	void ScreenText(uint32 uWorldTag, float fX, float fY, Zenith_StringView strText, uint32 uColour, uint32 uScale = 1);

	// Free records in the calling thread's ring (the overlay checks it before a view and draws "<detail cut>" instead of
	// overflowing; "Recorder rings and drain").
	uint32 GetFreeRecords() const;

	// The drain node: moves every ring's records into the staging store, expires persistent records, sorts by world tag.
	void DrainInto(uint64 ulFrameIndex, float fFrameSeconds);
	Zenith_Span<const Zenith_DebugDrawBatchView> GetBatches() const;   // ascending world tag
	Zenith_DebugDrawStats GetStats() const;
	uint64 GetResidentBytes() const;   // every buffer Create allocated
};
```

Colours are `uint32` in the packing of CORE's `Zenith_ColourToSrgb8Packed` (R in bits 0–7, then G, B, A; 8-bit sRGB-encoded display values with straight alpha), built with `Zenith_DebugDrawRgba` or converted from a linear `Zenith_Colour` with `Zenith_ColourToSrgb8Packed` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Colour"). The recorder keeps the packed form rather than taking `Zenith_Colour` because every record and every GPU element carries the colour in 4 bytes and the `DebugOverlay` pass draws after tone mapping in display values, so a 16-byte linear float colour would be converted once per call anyway. Consumers receive the recorder as `Zenith_DebugDraw*`: SCENE through its create desc, modules through `Zenith_ModuleStartContext::GetDebugDraw()` (row DV12, X245 (c)), the overlay and the console through `Zenith_DevTools`. Script-facing debug drawing is SCRIPT's binding over the same recorder, if SCRIPT's design adds one. The extractor (ZenithRuntime) reads records through this public header alone; no other library includes a ZenithProfiling `Internal/` header.

## Recorder records

`Zenith_DebugDrawRecord` and `Zenith_DebugDrawRecordKind` are public (§ "Zenith_DebugDraw header"), so the extractor in ZenithRuntime reads every field without an `Internal/` include ([Z2-N-CONTRACT-026], [Z2-N-CONTRACT-043]). The shapes (box, sphere, capsule, arrow, axes, frustum, AABB) are expanded into line records on the recording thread (§ "Shape expansion"), so the rings, the staging store and the render-world record hold three kinds alone. In a ring, a TEXT record's bytes are the next `m_uTextBytes` bytes of the thread's text ring; the drain copies them into the staging store's text arena and writes `m_uTextOffset` there.

## Shape expansion

| Call | Line records |
|---|---|
| `Line` | 1 |
| `Point` | 3 (a cross along the world axes, `fSize` long) |
| `Arrow` | 1 shaft + 4 head lines (a cone of half-angle 20° and length `min(0.25 m, 0.2 × length)` around the shaft) |
| `Aabb`, `Box` | 12 edges (box corners from the centre, half extents and rotation) |
| `Sphere` | 3 great circles of 24 segments each (72), in the world axes' planes |
| `Capsule` | 2 rings of 24 segments at the ends, 4 side lines, 4 half-circle arcs of 12 segments (100) |
| `Axes` | 3 (red X, green Y, blue Z from the transform's origin) |
| `Frustum` | 12 edges of the 8 corners `xInverseViewProjection × (±1, ±1, z, 1)` for `z` in {0, 1}, divided by w; for an infinite far plane, 4 near edges and 4 side edges of 1,000 m (the bullets below) |
| `Text3D`, `ScreenText` | 1 text record (glyph expansion happens in the extractor, § "Debug-draw extractor") |
| `ScreenLine`, `ScreenRect` | 1 |

- `Frustum` is independent of the view's depth mode (Z2-D-061: standard Z with D24S8 by default, reversed Z only where the precision check requires it; orthographic shadow views always standard Z). The clip-space depths 0 and 1 are the near and far planes in standard Z and the far and near planes in reversed Z, and the inverse matrix maps each to the plane it came from, so the 8 corners and their 12 edges are the same set of world-space segments in both modes; the recorder never needs to know which plane is near. `Test_DebugDraw_FrustumCornersBothDepthModes` asserts it.
- Finiteness and the infinite far plane: each corner is the homogeneous product `h = xInverseViewProjection × (sx, sy, z, 1)`. A corner is finite when `h.w > 1e-6 × max(|h.x|, |h.y|, |h.z|, 1)` and the divided `h.xyz / h.w` is finite. With CORE's `Zenith_MakePerspectiveReversedZInfinite` (`Zenith2/Docs/Design/CORE/Maths.md` → "Matrix types"), clip depth 0 is the far plane at infinity, so the four corners at that depth have `h.w = 0`. When the four corners of one depth are finite and the four of the other are not, the call draws the finite depth's 4 edges and, for each of the 4 corner rays, a side edge from the finite corner `N` to `N + 1000 × normalize(M − N)`, where `M` is the corner at clip depth 0.5 (finite for every perspective projection, standard or reversed, finite or infinite), so the side edges point away from the camera and end 1,000 m out; the 4 far edges are not drawn (12 − 4 = 8 records). When fewer than four corners of either depth are finite, the call is dropped and counted as non-finite. The matrix argument is checked element by element with `Zenith_IsFinite(float)` over its 16 elements before expansion, because CORE declares the predicate for vectors and quaternions, not for `Zenith_Matrix4`. `Test_DebugDraw_FrustumCornersBothDepthModes` adds the infinite reversed-Z case: 8 records, every endpoint finite, the side edges 1,000 m long and pointing away from the eye.
- Expansion writes the records straight into the thread's ring after checking `GetWritableCount()` against the whole shape; a shape whose records do not all fit is dropped whole (no partial sphere) and counted.
- A non-finite coordinate (`Zenith_IsFinite` of each `Zenith_Vector3` and `Zenith_Quaternion`, and of each of a matrix argument's 16 elements) drops the call before expansion, counted.

## Recorder rings and drain

- Rings: one main ring, claimed by the thread that calls `Create` (the main thread: the overlay and the console record there at frame-loop step 3), of `m_uMainRingRecords` records and `m_uMainRingTextBytes` text bytes, and `m_uThreadRings` other rings of `m_uRingRecords` records and `m_uRingTextBytes` text bytes, each a pair of CORE `Zenith_SpscRing` instances (`Zenith_SpscRing<Zenith_DebugDrawRecord>` and `Zenith_SpscRing<uint8>` for the text; ruling Q80, the same primitive the profiler uses, `Zenith2/Docs/Design/DEVT/Profiler.md` → "Per-thread event rings") over storage `Create` allocates. Producer: the recording thread; consumer: the drain node, which may run on a different worker each iteration, the role handed over by the frame graph's ordering exactly as the ring's contract allows. A recording thread finds its ring through its recorder slot (§ "Durations and world tags" for tags; the slot by ruling Q79: the profiler's thread slot on the primary path, `Zenith2/Docs/Design/DEVT/Profiler.md` → "Thread slots and the thread-local variable", or § "Recorder slot alternative"), claimed at its first call by an atomic scan as the profiler claims thread states; more recording threads than rings drop their records and count them.
- Sizing: the main ring holds the whole overlay. The overlay in `FULL` records at most 6,000 rectangles, 4,000 lines and 1,500 text records carrying at most 12,000 glyphs (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Overlay drawing"), the on-screen console at most 64 rectangles and 64 text records, a total of at most 11,628 records and 16,000 text bytes, inside 16,384 records and 64 KiB; the remaining 4,756 records are the main thread's own world draws made while it runs jobs in the same iteration. Other rings hold 1,024 records and 8 KiB of text per thread per iteration.
- Overflow policy: every ring is `QUEUE_FULL_POLICY_DROP_NEWEST` (a record that does not fit, with its text, is refused whole and counted); the counters are `devtools.debugdraw.dropped_delta` (every drop), `devtools.debugdraw.main_ring.full`, `.dropped`, `.high_water` and `devtools.debugdraw.rings.full`, `.dropped`, `.high_water` (summed over the other rings, [Z2-N-CONTRACT-345]); the first drop of each kind logs `devtools.debugdraw: <ring> full (<records> records); draws dropped` once per run. The overlay never relies on the drop: before drawing a view it compares its worst-case record count for that view with `GetFreeRecords()` and, when it does not fit, draws merged runs and `<detail cut>` instead.
- Text: a text call copies at most 255 bytes into the thread's text ring after checking that the record ring and the text ring both have room; text that does not fit drops the record.
- Drain (`DrainInto`, the node `DevTools.DrainDebugDraws`): for each ring, `PopSpan` its records into the write-side staging buffer (at most `m_uStagingRecords`; beyond it the rest of the rings' records are popped and dropped, counted, and the warning `devtools.debugdraw: staging full` is logged once per run), popping each TEXT record's bytes from the text ring into the staging text arena and re-pointing `m_uTextOffset`; records with a duration (`m_uDurationMs != 0`) are also copied into the persistent table (8,192 records and a 2 × 32 KiB double text arena); then the persistent table's records whose expiry is past are removed and the live ones are appended to the staging buffer, in one compaction pass over the table: the persistent text is a 2 × 32 KiB double arena, and the pass copies each live record's text from the current half into the other half (re-pointing its `m_uTextOffset`) before the halves swap, so expired records leave no text holes, the arena reports full only when the live text really exceeds 32 KiB (a newly persisted record that does not fit is dropped and counted), and the pass costs at most O(8,192 records + 32 KiB of text); finally the buffer is sorted by world tag (a counting sort over the tags seen, stable within a tag). The drain allocates nothing.
- The staging store is double-buffered: `GetBatches` serves the buffer the last drain wrote while the next drain writes the other, so an extractor of iteration `k` never reads a buffer iteration `k + 1`'s drain is writing (the drain node of `k + 1` is ordered after the extractors of `k` by the frame structure, and the double buffer makes the ordering unnecessary for safety).
- Memory (Z2-B-DEVT-001's debug-draw part, `Zenith2/Docs/Design/DEVT/Profiler.md` → "Memory layout and accounting"): main ring 16,384 × 48 + 65,536 = 851,968 bytes; other rings 47 × (1,024 × 48 + 8,192) = 2,695,168 (Android 31 rings: 1,777,664); staging 2 × (32,768 × 48 + 131,072) = 3,407,872 (Android 16,384 records: 1,835,008); persistent 8,192 × 48 + 65,536 (the 2 × 32 KiB text arena) + 8,192 × 8 (deadlines) = 524,288; total 7,479,296 bytes on Windows and 4,988,928 on Android, all allocated by `Create` (path (b) of § "Recorder slot alternative" adds its slot table).
- Counters (published at `Zenith_DevTools::EndFrame`): `devtools.debugdraw.lines`, `devtools.debugdraw.quads`, `devtools.debugdraw.dropped_delta`, the two ring triples above and `devtools.debugdraw.staging_high_water` (`Zenith2/Docs/Design/DEVT.md` → "Counters").
- `Test_DebugDraw_FullOverlayFrameDrainsWhole` records the overlay's `FULL` budget plus the console's on the main thread, drains once and asserts every record and every text byte arrives with nothing dropped; `Test_DebugDraw_MainRingOverflowCounted` records 20,000 lines on the main thread and asserts the newest 3,616 are dropped and counted in the main-ring triple, with one warning.

## Durations and world tags

- Duration: the recording thread stores `m_uDurationMs = (fDuration > 0.0f && fDuration <= 3.402823466e38f) ? (uint32)min(round((double)fDuration × 1000.0), 4294967295.0) : 0` (the comparison is false for a NaN and for both infinities, so it needs no `<cmath>` call, and the clamp in `double` precedes the conversion), so a NaN, an infinite or a negative duration means this frame only and no float-to-integer conversion is ever undefined (`Test_DebugDraw_NonFiniteDropped` adds a NaN, +infinity and −1 duration), and never reads the drain clock (a field another thread owns). The drain clock is `uint64 m_ulDrainClockNs`, owned by the drain alone, advanced by each `DrainInto` by `round(fFrameSeconds × 10^9)` (platform time, so a paused simulation still expires debug draws). When the drain copies a record with `m_uDurationMs > 0` into the persistent table it stores the deadline `m_ulDrainClockNs + m_uDurationMs × 10^6` in the table's parallel `uint64` deadline array (8,192 × 8 bytes), and a persistent record expires at the first drain whose clock has passed its deadline; the duration therefore starts at the drain that first sees the record, less than one frame after the call. `fDuration ≤ 0` means this frame only. An integer nanosecond clock keeps 1 ns resolution for 584 years, so expiry is exact at any uptime (a `float` second sum would stop advancing near 2^19 s). `Test_DebugDraw_DurationExpiresAfterLongUptime` sets the drain clock to 10^6 s through `Zenith_DebugDrawTestAccess::SetDrainClockNs` and asserts a 1-s record survives drains totalling 0.9 s and is gone after 1.1 s.
- World tags: a world primitive carries the world's stable tag (`Flux_RenderWorldSection::m_uWorldTag`), so draws from the play-in-editor world and the edited world land in their own sections and appear only in that world's views; tag 0 is reserved for draws that appear in every main and editor view (the overlay, the console, a game's own HUD-space debug text).
- View filtering: the `DebugOverlay` pass of a view draws the records of its world's section plus the tag-0 records the primary world's section carries; screen records with a nonzero tag are drawn in the views of that world alone; capture, preview and texture views draw no debug records (their epilogue omits the pass, which RPROD's epilogue decides by view kind).
- The editor's own debug draws (gizmo helpers drawn as debug primitives) use the edited world's tag.

## Flux_DebugDraw header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_DebugDraw.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Types.h"

/// @contract value Flux_DebugLineSegment
/// Ownership: Plain value; the GPU layout of one line (Zenith2/Shaders/Common/DebugDrawRecords.slang declares the same).
/// Thread affinity: IMMUTABLE; members non-const: written element by element into the extractor's staging and upload
///   buffers.
/// Invalidation: None.
/// Units: World metres, or pixels when the SCREEN flag is set; colour in Zenith_DebugDrawRgba packing.
/// Errors: None.
/// Version: since Z2 Stage 2. The layout is shared with the shader module; a change edits both in one diff.
/// @endcontract
struct Flux_DebugLineSegment     // 32 bytes
{
	float m_afFrom[3];           // 0
	uint32 m_uColour;           // 12
	float m_afTo[3];             // 16
	uint32 m_uFlags;            // 28: bit 0 depth tested, bit 1 screen, bit 2 thick
};
static_assert(sizeof(Flux_DebugLineSegment) == 32);

/// @contract value Flux_DebugQuad
/// Ownership: Plain value; the GPU layout of one screen rectangle or one glyph.
/// Thread affinity: IMMUTABLE; members non-const: written element by element into the extractor's staging and upload
///   buffers.
/// Invalidation: None.
/// Units: Pixels of the view's output extent for m_afRect; atlas texels for m_uGlyph.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_DebugQuad            // 32 bytes
{
	float m_afRect[4];           // 0: x, y, width, height in pixels (a deferred 3D glyph: see the paragraph below)
	uint32 m_uColour;           // 16
	uint32 m_uGlyph;            // 20: 0xFFFFFFFF = solid rectangle; else the glyph index 0 … 94
	float m_afAnchor[2];         // 24: unused for screen quads; for 3D text, see the paragraph below
};
static_assert(sizeof(Flux_DebugQuad) == 32);

/// @contract enum Flux_RenderDebugDrawKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Flux_RenderDebugDrawKind : uint8
{
	FLUX_RENDER_DEBUG_DRAW_LINES = 1,
	FLUX_RENDER_DEBUG_DRAW_QUADS,
};

/// @contract value Flux_RenderDebugDraw
/// Ownership: A render-world record appended through Flux_RenderWorldWriter::AddDebugDraw, which copies the record and
///   its span into the partition's slot arena and re-points the copy's span there (the rule of Zenith2/Docs/Rendering.md
///   "Render-world record types", row DV8, X246 (a)).
/// Thread affinity: IMMUTABLE once appended; members non-const: AddDebugDraw re-points the copy's spans into the slot
///   arena after copying it.
/// Invalidation: With its render-world slot.
/// Units: As the element types.
/// Errors: AddDebugDraw refuses a record whose span does not fit with FLUX_RENDER_WORLD_ERROR_SLOT_FULL; the extractor
///   counts it as dropped.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_RenderDebugDraw
{
	Flux_RenderDebugDrawKind m_eKind = FLUX_RENDER_DEBUG_DRAW_LINES;
	bool m_bAllViews = false;                              // tag-0 records, carried in the primary world's section
	Zenith_Span<const Flux_DebugLineSegment> m_axLines;    // LINES: at most 2,048 per record
	Zenith_Span<const Flux_DebugQuad> m_axQuads;           // QUADS: at most 2,048 per record
};
```

The extractor writes 3D text as screen quads in a deferred form: a `Flux_DebugQuad` with `m_afAnchor` holding the anchor's world-space x and y and `m_afRect[0]` its z, `m_afRect[1]` the glyph's column offset in cells, and bit 31 of `m_uGlyph` set; the `DebugText` program projects the anchor with the view's matrix and places the glyph in pixels, so one record serves every view of the world. The field comments of the struct above state the screen-quad meaning; the 3D-text meaning is this paragraph's, and `DebugDrawRecords.slang` decodes both. Both programs unpack `m_uColour` with R in bits 0–7 (`Zenith_DebugDrawRgba`), so the recorder's colours reach the target unchanged.

## Debug-draw extractor

Two node kinds, registered by ZenithRuntime (`Internal/DevTools/Zenith_DebugDrawExtractor.cpp`) once per frame-schedule compile, beside the engine stage nodes (`Zenith2/Docs/Design/MOD.md` → "Frame-graph nodes registered by ZenithRuntime"):

| Node | Subgraph, stage and group | Affinity | Order | Work |
|---|---|---|---|---|
| `DevTools.DrainDebugDraws` (addressed as `DevTools.DrainDebugDraws@W0`) | `FRAME_SUBGRAPH_EPILOGUE`, stage 12, group 0 (an engine-wide stage node) | any worker | none of its own; the extractors order after it | `Zenith_DebugDraw::DrainInto(frame, platform frame seconds)` |
| `DevTools.ExtractDebugDraws@W<tag>` | `FRAME_SUBGRAPH_EPILOGUE`, stage 12, the world's group | any worker | `After("DevTools.DrainDebugDraws@W0", true)` and `Before("Render.Seal@W0", true)`, both explicit | reads the batch of its world tag (and, for the primary world, tag 0), expands text into glyph quads, splits the records into `Flux_RenderDebugDraw` records of at most 2,048 elements and appends them with `AddDebugDraw` |

- Ordering: group-0 stage nodes order against world nodes "through explicit orderings alone" (`Zenith2/Docs/Architecture/CPUFrameGraph.md` group rule 7), so the extractor's two orderings are explicit and it declares no shared resource with the drain node; its one declared access is its world's render-world writer partition, the access every extractor of the world declares. `Render.Seal` is the one engine-wide stage-13 node, addressed as `Render.Seal@W0` exactly as `Zenith_WorldSimulation::EmitInto` addresses it for the world's own extractors ([Z2-N-ARCH-439]). It never touches a component, so it adds no ECS access.
- Data flow: simulation and the world's systems record before stage 12, so their draws reach the same iteration's render world; the overlay and the console record at frame-loop step 3 (between iterations), so their draws reach the next iteration's render world (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Overlay drawing").
- Text expansion: each character byte 0x20 … 0x7E becomes one `Flux_DebugQuad` (others become `?`); `\n` starts a new line one cell lower; screen text at scale `s` uses `8s` pixel cells.
- Caps: per frame at most 65,536 line segments and 32,768 quads across all records of all worlds (§ "Overlay upload budget"); beyond them records are dropped in world-tag order (higher tags first), counted, and the warning `devtools.debugdraw: frame cap reached (<lines> lines, <quads> quads)` is logged once per run.
- Headless runs: startup step 13 does not run (no renderer, no render world), so neither node is registered and the composition root leaves the recorder disabled (`SetEnabled(false)`): every drawing call returns at its first branch and nothing accumulates. Tests that exercise the recorder headless call `DrainInto` and `GetBatches` themselves.

## DebugOverlay pass

`Flux_AddDebugOverlay` has one declaration, RPROD's in `Zenith2/Engine/ZenithFlux/Flux_Features.h` (`Zenith2/Docs/Design/RPROD/Passes.md` → "Flux_Features header"): `void Flux_AddDebugOverlay(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput);`. DEVT defines it in `Zenith2/Engine/ZenithFlux/Internal/Debug/Flux_DebugOverlay.cpp` and declares no second prototype. DEVT's API-only slice for `Flux_DebugDraw.h` (Stage 2, `Zenith2/Docs/Design/DEVT.md` → "API-only slices") ships a no-pass definition in that file (it returns without adding a pass), as A5 clarification (d) item 5 requires of every `Flux_Features.h` owner, so RPROD's epilogue links before the pass lands.

- Called by RPROD's common epilogue for every main and editor view, after `FLUX_EXTENSION_POINT_OVERLAY`'s passes (where LIGHT's lighting debug views run, row DV7) and before `HiZDebugOverlay`; it adds no pass when the view's sections carry no debug record.
- Inputs: the render world's sections for the view's world (and the primary section's `m_bAllViews` records); the pass counts lines and quads, and when either is nonzero allocates one upload of `lines × 32 + quads × 32` bytes from the frame's upload ring (`Zenith2/Docs/Design/RHI.md` names the ring), copies the records into it in section order, and binds it as `m_xVertices`.
- Pass parameters (the per-pass table's fields, `Zenith2/Docs/Rendering/PerPass/Common.md` → "DebugOverlay"): `m_xVertices` (a byte-buffer SRV over the upload: lines first, then quads, with the quad base offset in the data), `m_xDepth` (the view's resolved depth at output extent, bound only when any line is depth tested), `m_xFontAtlas` (the persistent debug font atlas), `m_xViewProjection` (the view's unjittered view-projection), `m_xInvOutputExtent`; and two data fields DEVT asks REND to add to the table (row DV8, X246 (b)): `uint32 m_uQuadBase` (the first quad's byte offset in `m_xVertices`) and `uint32 m_uReversedZ` (the view's depth mode as data: 1 when the view uses reversed Z, 0 for standard Z, copied from the view's depth mode of `Zenith2/Docs/Rendering.md` → "Render views" under Z2-D-061, which makes standard Z with D24S8 the default and reversed Z the exception the precision check selects; orthographic shadow views are always standard Z). The depth comparison is never inferred.
- Draws: (1) lines, permutation `bPERM_DEPTH_TESTED = false`, `6 × lineCount` vertices, no vertex buffer (vertex pulling from `m_xVertices`); (2) lines with `bPERM_DEPTH_TESTED = true` when any exist (the pass partitions depth-tested lines after the untested ones at copy time); (3) quads, `6 × quadCount` vertices with the `DebugText` program. Blend: premultiplied-alpha over (`ONE, ONE_MINUS_SRC_ALPHA`), no depth attachment (depth testing is done in the pixel shader against `m_xDepth`, so the pass needs no depth target and is legal on every tier).
- Target: the view output, `LOAD`/`STORE`, after tone mapping, so debug colours are display values.
- `Test_DebugOverlayRender_DepthTestedDimmedBothModes` renders a depth-tested line behind a quad in a standard-Z view and in a reversed-Z view and asserts 25% alpha in both.

## DebugLines program

`Zenith2/Shaders/Passes/Debug/DebugLines.slang` → `Flux_DebugLinesParams`:

- Vertex: `segment = vertexID / 6`, `corner = vertexID % 6` over two triangles; loads the 32-byte segment from `m_xVertices`; for a world segment, projects both ends with `m_xViewProjection`, clips the segment against the near plane in clip space (an end behind the near plane is moved along the segment to it), converts to pixels with the output extent and expands the segment into a quad of width 1 pixel (2 with `THICK`) perpendicular to its screen direction, plus half a pixel of anti-aliasing fringe; for a screen segment, the same expansion directly in pixels.
- Pixel: alpha from the distance to the line's centre (a 1-pixel smoothstep fringe); in the depth-tested permutation, loads `m_xDepth` at the pixel and multiplies alpha by 0.25 when the fragment is behind the scene (`m_uReversedZ` ? `fragDepth < sceneDepth − ε` : `fragDepth > sceneDepth + ε`, `ε = 1e-5`), so hidden lines are dimmed rather than removed (no `discard`, which keeps the program legal for every early-depth rule and every tier).

## DebugText program

`Zenith2/Shaders/Passes/Debug/DebugText.slang` → `Flux_DebugTextParams`:

- Vertex: `quad = vertexID / 6`; loads the 32-byte quad at `m_uQuadBase + quad × 32`; a deferred 3D glyph (bit 31 of `m_uGlyph`) projects its anchor and is culled (degenerate triangle) when behind the camera or outside the view; the rectangle is emitted in pixels and converted to clip space with `m_xInvOutputExtent`.
- Pixel: a solid rectangle (`m_uGlyph == 0xFFFFFFFF`) outputs its colour; a glyph samples the R8 atlas with a point sampler at its texel and outputs `colour × coverage`, premultiplied; glyph text gets a one-pixel black drop shadow by a second sample offset by (−1, −1) texel, which keeps text legible over any scene.

## Debug font

- Glyphs: the 95 printable ASCII characters 0x20 … 0x7E, each 8 × 8 pixels at one bit per pixel, held as `inline constexpr uint64 aulDEBUG_FONT_GLYPHS[95]` in `Zenith2/Engine/ZenithFlux/Internal/Debug/Flux_DebugFontGlyphs.h` (row-major, bit 63 the top-left pixel). The glyph table is an original design drawn for Zenith2 (a plain monospaced face), committed as C++ data, so there is no font file, licence entry or cook step.
- Atlas: `Flux_DebugFont.cpp` expands the table at renderer creation into a 128 × 48 `R8_UNORM` texture (16 columns × 6 rows of 8 × 8 cells, glyph `g` at column `g % 16`, row `g / 16`, 0xFF for set bits), uploads it once as a persistent read-only texture and registers its descriptor; the 6 KiB texture is part of RPROD's persistent resources (row DV6 asks RPROD for the creation step and its fault value).
- `Test_DebugFont_AtlasMatchesGlyphTable` decodes the atlas bytes back into bits and compares them with the table.

## Overlay upload budget

| Item | Cap | Bytes |
|---|---|---|
| line segments per frame | 65,536 | 2 MiB |
| quads (rectangles and glyphs) per frame | 32,768 | 1 MiB |
| total upload per frame | | at most 3 MiB, inside the upload ring's hard cap Z2-B-RHI-006 |

The overlay in `FULL` uses at most 4,000 lines and 18,000 quads (6,000 rectangles and 12,000 glyphs; `Zenith2/Docs/Design/DEVT/Overlay.md` → "Overlay drawing"), leaving the rest for area debug views. The GPU cost of the pass at the caps is what Z2-B-DEVT-003 bounds: vertex pulling at 6 vertices per element and a pixel cost of one or two texture reads per covered pixel; the dominant term is the overlay's translucent backgrounds, which cover at most 60% of the view, measured by `Test_DebugOverlay_GpuCostWithinRow` in the Stage 3 device run (`Zenith2/Docs/Design/DEVT/Tests.md` → "Device and acceptance runs").

## Debug-draw failure handling

| Failure | Behaviour |
|---|---|
| a thread's ring (or its text ring) is full | the record (or the whole shape) is dropped, `devtools.debugdraw.dropped_delta` and the ring's triple count it, one warning per run per ring kind |
| the main ring would overflow during the overlay's update | the overlay checks `GetFreeRecords()` first and draws merged runs with `<detail cut>`; a drop that still happens is counted as above |
| more recording threads than rings | that thread's draws are dropped and counted, one warning naming the thread ID |
| the staging buffer is full at the drain | the remaining popped records are dropped, counted, one warning per run |
| the persistent table is full | the duration record is drawn this frame and not kept, counted |
| text longer than 255 bytes | cut, the last byte replaced by `~` |
| a frame cap reached | records dropped in world-tag order, counted, one warning per run |
| `AddDebugDraw` refuses a record (slot full) | the record is dropped and counted; the remaining records of the batch are still offered |
| the upload ring cannot supply the pass's upload | the pass draws nothing that frame, `devtools.debugdraw.upload_refused_delta` counts it |
| a NaN or infinite coordinate | the call is dropped before expansion (`Zenith_IsFinite`), counted |
| the recorder is disabled (`SetEnabled(false)`, the default in Shipping until the overlay is shown, and always in headless runs) | every call returns at its first branch |

## Recorder slot alternative

This is the full specification of ruling Q79's option (b), which replaces the primary path (the recorder caching its ring in the profiler's thread slot, `Zenith2/Docs/Design/DEVT/Profiler.md` → "Thread slots and the thread-local variable") if the human rules against widening [Z2-N-CONTRACT-144]. It uses no thread-local storage and adds no writable symbol; everything outside `Internal/DebugDraw/Zenith_DebugDrawSlots.cpp` is unchanged, and so is every test.

```cpp
// Zenith2/Engine/ZenithProfiling/Internal/DebugDraw/Zenith_DebugDrawSlots.h (option (b) only)
struct Zenith_DebugDrawSlotEntry      // 16 bytes, 8-byte aligned
{
	std::atomic<uint64> m_ulThreadID;   // 0 = empty; Zenith_GetCurrentThreadID() of the owning thread (never 0 on any platform)
	std::atomic<uint32> m_uRing;        // ring index + 1; 0 while the claimant is between its two stores
	uint32 m_uReserved;
};
```

- Storage: a table of `2 × (m_uThreadRings + 1)` entries rounded up to a power of two (128 on Windows, 64 on Android), allocated by `Create`, plus `std::atomic<uint32> m_uNextRing` (the main ring is index 0, claimed by `Create` for the calling thread).
- Lookup on every drawing call (after the enabled check): `t = Zenith_GetCurrentThreadID()` (`Zenith2/Docs/Design/CORE/Threading.md` → "Threads"); probe linearly from `Zenith_Hash64` of `t` masked to the table; an entry whose `m_ulThreadID.load(acquire) == t` gives the ring once `m_uRing.load(acquire) != 0`; an empty entry ends the probe (the thread has no ring yet).
- Claim (a miss): probe from the hash for an empty entry and `compare_exchange_strong(0 → t, acq_rel)` on `m_ulThreadID` (no empty entry in the whole table: the call is dropped and counted, and nothing is modified, so the next call probes again without any increment); then `r = m_uNextRing.fetch_add(1, relaxed)`; when `r > m_uThreadRings` the thread is refused a ring: `m_uRing.store(0xFFFFFFFF, release)`, the sentinel "no ring", after which every lookup of that thread finds the entry and drops the call at once, counted ("Debug-draw failure handling"); otherwise `m_uRing.store(r + 1, release)`. `m_uNextRing` is therefore incremented at most once per table entry (at most 128 times on Windows), so it can never wrap and no ring index is ever handed to two threads. A lookup that finds `m_uRing == 0` (the claimant between its two stores) drops the call for that call only. A thread that exits leaves its entry; a subsequent thread reusing the OS ID finds the entry and reuses the ring, which is safe because the ring's single producer is again one thread at a time (the earlier thread is gone).
- `Test_DebugDraw_RefusedThreadNeverSharesRing` (on the path built; path-neutral assertions): with 3 other rings, 6 threads draw 1,000 lines each; at most 3 of them own a ring, no ring receives records from two threads, every refused thread's lines are counted dropped, and the ring-claim count reported by `Zenith_DebugDrawSlotsTestAccess::GetRingClaims()` (on path (b) `m_uNextRing`) is at most the number of drawing threads.
- Cost: one OS thread-ID read (a TEB or TLS read inside the OS, not a writable symbol of Zenith2) and one to three probes per call, a few nanoseconds more than the primary path; `Zenith_Bench_ProfilerOverhead` case `DebugDraw_Line_Cached` measures the call on whichever path is built.
- The thread slot (`Zenith2/Docs/Design/DEVT/Profiler.md` → "Thread slots and the thread-local variable") then has its first four members only, and [Z2-N-CONTRACT-144] keeps its current wording.
