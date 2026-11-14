# DEVT design: the on-screen profiler

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It specifies the on-screen profiler of Z2-D-005: `Zenith_ProfilerOverlay.h` and `Zenith_ProfilerPanels.h`, the overlay's state machine, layout and views, the panel catalogue and its expression language, panel providers, input, drawing, freeze, the graph view, and the editor and S24 variants. The overlay is ZenithProfiling code; it reads the `Zenith_Profiler` of `Zenith2/Docs/Design/DEVT/Profiler.md` and draws through `Zenith_DebugDraw` of `Zenith2/Docs/Design/DEVT/DebugDraw.md`, so it needs no GPU code of its own and appears through the pipelined render world (Z2-D-050).

## Scope of this file

| Concern | Owner of the data | Specified here |
|---|---|---|
| what each area shows (its rows) | the area's own "On-screen profiler rows" heading | how a row is declared, evaluated and drawn |
| the frame timeline and node timings | GRAPH and JOBS (`Zenith2/Docs/Design/JOBS.md` → "On-screen profiler rows") | the timeline and contention views |
| GPU pass times | RPROD (`Zenith2/Docs/Design/RPROD/Diagnostics.md` → "On-screen profiler rows") | the GPU view |
| memory and configuration content | CORE (`Zenith2/Docs/Design/CORE.md` → "On-screen profiler rows") | the memory and configuration panels |
| the toggle action `Dev.ToggleProfiler` and the `Engine.Dev` context | PLAT (`Zenith2/Docs/Design/PLAT/Input.md` → "Engine actions and contexts") | how the overlay reacts to it, plus four actions DEVT asks PLAT to add (row DV13) |

## Overlay public headers

| Header | Declares | Stage |
|---|---|---|
| `Zenith_ProfilerOverlay.h` | `Zenith_ProfilerOverlayState`, `Zenith_ProfilerOverlayInput`, `Zenith_ProfilerOverlayDesc`, `Zenith_ProfilerOverlay` | Stage 2 |
| `Zenith_ProfilerPanels.h` | `Zenith_ProfilerRowKind`, `Zenith_ProfilerRowDesc`, `Zenith_ProfilerPanelDesc`, `Zenith_ProfilerExpression`, `Zenith_ProfilerPanelBuilder`, `Zenith_ProfilerPanelProvider`, `Zenith_GetBuiltInPanels` | Stage 2 |

## Zenith_ProfilerOverlay header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_ProfilerOverlay.h
#pragma once
#include "ZenithCore/Zenith_CallbackToken.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithProfiling/Zenith_ProfilerPanels.h"
#include "ZenithProfiling/Zenith_ProfilerTypes.h"

class Zenith_Profiler;
class Zenith_DebugDraw;
class Zenith_DevConsole;

/// @contract enum Zenith_ProfilerOverlayState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The names are the values of devtools.profiler.visible; never renumbered.
/// @endcontract
enum Zenith_ProfilerOverlayState : uint8
{
	PROFILER_OVERLAY_STATE_HIDDEN = 1,
	PROFILER_OVERLAY_STATE_COMPACT,   // the top bar alone
	PROFILER_OVERLAY_STATE_FULL,      // the top bar, the tab strip and the selected view
};

/// @contract value Zenith_ProfilerOverlayInput
/// Ownership: Plain value filled once per frame by ZenithRuntime's input adapter (Zenith_OverlayInputAdapter) or a test.
/// Thread affinity: IMMUTABLE; members non-const: the input adapter fills it field by field every frame.
/// Invalidation: None.
/// Units: Pixels of the target view's output, origin top-left; m_fScroll in wheel notches.
/// Errors: None; values outside the view are ignored.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerOverlayInput
{
	bool m_bToggle = false;          // Dev.ToggleProfiler pressed this frame
	bool m_bNextPanel = false;       // Dev.ProfilerNextPanel
	bool m_bPreviousPanel = false;   // Dev.ProfilerPreviousPanel
	bool m_bFreeze = false;          // Dev.ProfilerFreeze
	bool m_bCapture = false;         // Dev.ProfilerCapture
	bool m_bOlderFrame = false;      // Dev.ProfilerOlderFrame: the frame selector moves to an older held frame
	bool m_bNewerFrame = false;      // Dev.ProfilerNewerFrame
	bool m_bPointerValid = false;    // a mouse, touch or pen position exists for the overlay's view
	bool m_bPointerPressed = false;  // a press this frame that the overlay claimed (Overlay input)
	bool m_bPointerHeld = false;
	bool m_bTouch = false;           // the pointer is a touch (larger hit targets)
	float m_fPointerX = 0.0f;
	float m_fPointerY = 0.0f;
	float m_fScroll = 0.0f;
	uint32 m_uViewWidth = 0;        // the target view's output extent
	uint32 m_uViewHeight = 0;
	float m_fDisplayScale = 1.0f;    // the window's content scale (PLAT)
};

/// @contract value Zenith_ProfilerOverlayDesc
/// Ownership: Plain value copied by Create; the referenced objects outlive the overlay.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns PROFILER_ERROR_INVALID_DESC when m_pxProfiler or m_pxDebugDraw is null.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerOverlayDesc
{
	Zenith_Profiler* const m_pxProfiler = nullptr;
	Zenith_DebugDraw* const m_pxDebugDraw = nullptr;
	Zenith_DevConsole* const m_pxConsole = nullptr;                 // null in Shipping; enables the capture and GPU-capture buttons
	const Zenith_Span<const Zenith_ProfilerPanelDesc> m_axPanels{}; // the catalogue; normally Zenith_GetBuiltInPanels()
	const Zenith_ProfilerOverlayState m_eInitialState = PROFILER_OVERLAY_STATE_HIDDEN;
	const Zenith_StringView m_strInitialPanel = "frame";
	const uint32 m_uUiScale = 0;                                    // 0 = from m_fDisplayScale; 1 … 4 explicit
	const uint32 m_uWorldTag = 0;                                   // the debug-draw world tag for screen draws (0 = every view)
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract type Zenith_ProfilerOverlay
/// Ownership: Created by Create as a Zenith_Owned<Zenith_ProfilerOverlay>, owned by ZenithRuntime's Zenith_DevTools
///   (game) or by the editor's development-tools object (editor). Owns its compiled expressions, its layout arena and its
///   provider table; never owns the profiler, the recorder or the console.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: Pixels as Zenith_ProfilerOverlayInput.
/// Errors: Create returns Zenith_Result<…, Zenith_ProfilerError>: PROFILER_ERROR_INVALID_DESC, PROFILER_ERROR_OUT_OF_MEMORY,
///   PROFILER_ERROR_EXPRESSION (a catalogue expression fails to compile; the message names the panel, the row and the
///   column). AddProvider returns PROFILER_ERROR_INVALID_DESC for a duplicate or malformed panel ID.
/// Reentrancy: NONE. Providers are called from Update and may not call any overlay member.
/// Version: since Z2 Stage 2. Source API.
/// Example: Zenith2/Engine/ZenithProfiling/Tests/Examples/Zenith_Profiler_Example.cpp
/// @endcontract
class Zenith_ProfilerOverlay
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ProfilerOverlay>, Zenith_ProfilerError> Create(
		const Zenith_ProfilerOverlayDesc& xDesc);
	~Zenith_ProfilerOverlay();

	// Frame-loop step 3, after Zenith_Profiler::EndFrame: reacts to the input, then records this frame's screen draws.
	void Update(const Zenith_ProfilerOverlayInput& xInput);

	void SetState(Zenith_ProfilerOverlayState eState);
	Zenith_ProfilerOverlayState GetState() const;
	void CycleState();                                   // hidden → compact → full → hidden
	bool SelectPanel(Zenith_StringView strPanelID);   // false when no panel has that ID
	Zenith_StringView GetSelectedPanel() const;
	void SetFrozen(bool bFrozen);                        // forwards to Zenith_Profiler::Freeze
	bool IsFrozen() const;

	// A provider's panel joins the tab strip after the catalogue panels; the token removes it.
	Zenith_Result<Zenith_CallbackToken, Zenith_ProfilerError> AddProvider(Zenith_ProfilerPanelProvider& xProvider);

	// True when the last Update found the pointer over an overlay element; the input adapter claims presses with it.
	bool HitTest(float fPixelX, float fPixelY) const;
};
```

## Zenith_ProfilerPanels header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_ProfilerPanels.h
#pragma once
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithProfiling/Zenith_ProfilerTypes.h"

/// @contract enum Zenith_ProfilerRowKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_ProfilerRowKind : uint8
{
	PROFILER_ROW_KIND_VALUE = 1,     // label, value, history min/max/mean
	PROFILER_ROW_KIND_BAR,           // value against m_szLimit as a horizontal bar
	PROFILER_ROW_KIND_SPARKLINE,     // value with its last 128 frame values drawn as a line
	PROFILER_ROW_KIND_EACH,          // one VALUE row per counter matching m_szExpression's each() pattern
	PROFILER_ROW_KIND_HEADER,        // a section title; m_szExpression empty
};

/// @contract value Zenith_ProfilerRowDesc
/// Ownership: Plain value in a constexpr table; every pointer is a string literal.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_eUnit names the expression's unit; m_fScale multiplies the value before display.
/// Errors: An expression that fails to compile fails Zenith_ProfilerOverlay::Create with PROFILER_ERROR_EXPRESSION.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ProfilerRowDesc
{
	const char* const m_szLabel = "";      // at most 32 bytes, ASCII
	const char* const m_szExpression = ""; // the grammar of Overlay.md "Expression language"
	const Zenith_ProfilerRowKind m_eKind = PROFILER_ROW_KIND_VALUE;
	const Zenith_ProfilerUnit m_eUnit = PROFILER_UNIT_COUNT;
	const float m_fScale = 1.0f;
	const char* const m_szLimit = "";      // BAR: an expression giving the full-scale value; empty = 1.0
	const char* const m_szBudgetRow = "";  // a Z2-B-… ID shown beside the value, e.g. "Z2-B-JOBS-001"
	const float m_fBudgetValue = 0.0f;     // in m_eUnit after scaling; 0 = no comparison (a pending row)
	const char* const m_szExactness = "";  // "exact", "may spill" or "approximate", from the area's row table
};

/// @contract value Zenith_ProfilerPanelDesc
/// Ownership: Plain value in a constexpr table.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A duplicate m_szID fails Zenith_ProfilerOverlay::Create with PROFILER_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2. Panel IDs are stable; they are the values of devtools.profiler.panel.
/// @endcontract
struct Zenith_ProfilerPanelDesc
{
	const char* const m_szID = "";        // [a-z][a-z0-9_]{0,23}
	const char* const m_szTitle = "";     // at most 24 bytes
	const char* const m_szOwnerArea = ""; // the area code that owns the rows, e.g. "JOBS"
	const Zenith_Span<const Zenith_ProfilerRowDesc> m_axRows{};
};

/// @contract type Zenith_ProfilerExpression
/// Ownership: A compiled expression owned by the overlay (or a test); its byte code lives in the arena passed to Compile.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when its arena is reset.
/// Units: The value is a double in the unit of the expression's inputs (counters' own units, milliseconds for zone_ms).
/// Errors: Compile returns PROFILER_ERROR_EXPRESSION with the column of the first error and its message in xError;
///   Evaluate never fails: a missing counter evaluates to 0 and sets the row's "missing" mark; a division by zero yields 0
///   and sets the "undefined" mark.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ProfilerExpression
{
public:
	struct CompileError
	{
		uint32 m_uColumn = 0;
		const char* m_szMessage = "";   // a literal, e.g. "unknown function"
	};
	static Zenith_Result<Zenith_ProfilerExpression, Zenith_ProfilerError> Compile(Zenith_StringView strText,
		Zenith_Arena& xArena, CompileError& xError);
	struct Result
	{
		double m_dValue = 0.0;
		bool m_bMissing = false;
		bool m_bUndefined = false;
	};
	Result Evaluate(const Zenith_Profiler& xProfiler) const;
	// EACH rows: calls pfnRow once per matching counter in byte order of name, with the wildcard capture.
	void EvaluateEach(const Zenith_Profiler& xProfiler, void (*pfnRow)(Zenith_StringView strCapture, Result xValue,
		void* pUser), void* pUser) const;
};

/// @contract type Zenith_ProfilerPanelBuilder
/// Ownership: Owned by the overlay; passed by reference to a provider for one Build call.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Valid for the Build call alone; text is copied into the overlay's frame arena.
/// Units: As each member states.
/// Errors: Calls beyond 256 rows per panel are ignored and the panel shows "<rows cut>".
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ProfilerPanelBuilder
{
public:
	void Header(Zenith_StringView strTitle);
	void Text(Zenith_StringView strLabel, Zenith_StringView strValue);
	void Value(Zenith_StringView strLabel, double dValue, Zenith_ProfilerUnit eUnit);
	void Bar(Zenith_StringView strLabel, double dValue, double dLimit, Zenith_ProfilerUnit eUnit);
	void Indent(uint32 uLevel);   // 0 … 6; tree views
};

/// @contract interface Zenith_ProfilerPanelProvider
/// Ownership: Implemented by a library above ZenithProfiling (ZenithRuntime's providers of Overlay.md "Panel providers");
///   registered with Zenith_ProfilerOverlay::AddProvider; unregistered (token destroyed) before the implementer dies.
/// Thread affinity: MAIN_THREAD. Build is called from Zenith_ProfilerOverlay::Update when the provider's panel is visible.
/// Invalidation: The strings returned by GetPanelID and GetTitle live as long as the provider.
/// Units: None.
/// Errors: None; a provider with nothing to show writes one Text row saying so.
/// Reentrancy: NONE. Build may not call an overlay or profiler member other than the const getters of Zenith_Profiler.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_ProfilerPanelProvider
{
public:
	virtual ~Zenith_ProfilerPanelProvider() = default;
	virtual Zenith_StringView GetPanelID() const = 0;
	virtual Zenith_StringView GetTitle() const = 0;
	virtual void Build(Zenith_ProfilerPanelBuilder& xBuilder) = 0;
};

// The catalogue of Overlay.md "Built-in panel catalogue", in tab order.
Zenith_Span<const Zenith_ProfilerPanelDesc> Zenith_GetBuiltInPanels();
```

## Overlay state machine

| State | Draws | Cost when entered |
|---|---|---|
| `HIDDEN` | nothing; `Update` returns after reading the input | the profiler still assembles frames (the history stays complete for when the overlay opens) |
| `COMPACT` | the top bar (§ "Top bar") | top-bar expressions only |
| `FULL` | the top bar, the tab strip, the selected view | the selected view's work only; panels not shown are not evaluated |

Transitions, applied at the start of `Update` in this order:

1. `m_bToggle` → `CycleState()` (hidden → compact → full → hidden).
2. `m_bNextPanel` / `m_bPreviousPanel` in `FULL` → the next or previous tab (wrapping); in `COMPACT` → enter `FULL` on the current tab.
3. `m_bFreeze` → toggle freeze (§ "Freeze and frame selection"); in `HIDDEN` it is ignored.
4. `m_bOlderFrame` / `m_bNewerFrame` while frozen → the frame selector steps one held frame; unfrozen or `HIDDEN` they are ignored.
5. `m_bCapture` → console line `profiler.capture 300` (§ "GPU view" for the GPU capture button); without a console the press is ignored.
6. Pointer actions on the tab strip and buttons (§ "Overlay input").

`devtools.profiler.visible` sets the state at startup and whenever the key changes; a state reached by the toggle is not written back to the key (the key is the initial state, the toggle is the session's choice). The console commands `profiler.visible <hidden|compact|full>` and `profiler.panel <id>` call `SetState` and `SelectPanel`. In Shipping the overlay exists, its state is `HIDDEN` by default, and `devtools.profiler.visible` in the user layer or on the command line shows it (Z2-D-005: the runtime toggle needs no build flag); the toggle action exists only where `Engine.Dev` exists, so Shipping has no key binding for it.

## Overlay layout

- Units: one cell is the debug font's 8 × 8 glyph times the UI scale `s` (`devtools.profiler.uiScale`, or `max(1, round(displayScale × viewHeight / 1080))` when 0, clamped to 1 … 4), so on a 1440p view at scale 1.0 `s = 1` gives 8-pixel cells and on the S24 at 3088 × 1440 landscape `s = 2`.
- Regions: the top bar is one row of cells across the full width at the top, with a one-cell margin; in `FULL` the tab strip is the next row, and the content region fills the rest down to 60% of the view height (the lower 40% shows the game); a panel taller than its region scrolls with `m_fScroll` or a drag on its scrollbar column.
- Colours, written with `Zenith_DebugDrawRgba(r, g, b, a)` (`Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Zenith_DebugDraw header"): one translucent background `(0x10, 0x10, 0x10, 0xC0)` behind each region; text white `(0xFF, 0xFF, 0xFF, 0xFF)`, dim text `(0xA0, 0xA0, 0xA0, 0xFF)`, over-budget values red `(0xFF, 0x40, 0x40, 0xFF)`, within 90–100% of budget amber `(0xFF, 0xC0, 0x40, 0xFF)`, hitch markers red.
- Safe area: on Android the regions are inset by the display cut-out and system-bar insets PLAT reports for the window (the adapter passes them as a reduced view rectangle).
- Columns of a panel: label (32 cells), value (12 cells, right aligned), unit (4), history min / max / mean (12 each, `FULL` alone on views at least 160 cells wide), budget (16): narrower views drop the history columns, then the budget column.
- Numbers: `Zenith_Format` with three significant digits for values below 1,000 and grouped integers above; bytes in KiB, MiB or GiB by magnitude; percentages with one decimal.

## Top bar

One line, left to right, each item a fixed expression:

| Item | Expression | Shown as |
|---|---|---|
| frame rate | `1000000 / frame_us()` | `60.0 fps` |
| frame time | `frame_us() / 1000` | `16.6 ms`, red above the hitch threshold |
| CPU sim, CPU render | `frame_field(cpuSimUs) / 1000`, `frame_field(cpuRenderUs) / 1000` | `sim 4.1 render 3.2` |
| GPU | `frame_field(gpuUs) / 1000` | `gpu 9.8`; `gpu —` when the frame has `GPU_MISSING` |
| dynamic resolution | `frame_field(scaleX) / 100` | `res 87%` when below 100% |
| hitches | the count of `HITCH` frames in the history ring | `hitch 3/600` |
| memory level | `latest(memory.level)` | `mem OK`, `mem WARN` (amber), `mem CRIT` (red) |
| state marks | the profiler's mode and the overlay's freeze | `FROZEN`, `CAPTURE`, `COUNTERS ONLY` |

`frame_us()` and `frame_field()` are the expression functions over the last summary (§ "Expression language"); `memory.level` is CORE's governor gauge.

## Timeline view

Panel ID `frame`, the default. It draws the last assembled frame (or the selected frozen frame) as a horizontal timeline:

- The time axis spans the frame's first zone start to its last zone end (CPU lanes) with a tick every millisecond and a label every 5 ms; `m_fScroll` zooms around the pointer (×1.25 per notch, 1 µs per pixel at the most) and a drag pans.
- One lane per CPU lane, in the profiler's lane order (Profiler.md "Thread lanes"), labelled with the lane name; each lane is `depth × 1` cell rows high, at most 8 rows (deeper zones are drawn at row 8 with a `+` mark).
- Each zone is a rectangle from start to end, coloured from its site colour, or from a stable 24-bit hash of its name when the site colour is 0; a zone narrower than one pixel is merged with its neighbours into a grey run; zones with `TRUNCATED_START` or `TRUNCATED_END` have a hatched end (two diagonal lines).
- Labels: a zone at least as wide as its name in cells shows the name, cut with `~`; the hovered zone shows a tooltip with name, duration, value (when `HAS_VALUE`), file and line.
- The frame graph's nodes are the zones GRAPH emits per node ([Z2-N-ARCH-432] via `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Profiling"); the timeline marks the four `Zenith_FrameTimeline` stamps as vertical lines labelled `iter`, `sim`, `render`, `end`.
- Lock and wait zones (Profiler.md "Lock and wait events"): `lock:<name>` is a hold, `lockwait:<name>` the wait before it, and `wait:<name>` a blocking wait of the measured length, named by what was waited on: a condition variable shows under its mutex's name (`wait:<mutex name>`), a semaphore under its optional name (default `wait:Zenith_Semaphore`), the job park as `wait:jobs.park`. A wait zone that began in an earlier frame is drawn with the hatched start of a truncated zone.
- Below the lanes, the contention strip shows the JOBS rows of § "Built-in panel catalogue" panel `jobs` in one line (steals, idle %, parks).
- The lock table. A `[locks]` toggle at the right of the strip's header is a two-state button that also switches lock capture: pressing it runs the console line `set devtools.profiler.captureLocks true` and replaces the strip with the lock table, pressing it again runs `set devtools.profiler.captureLocks false` and restores the strip (the value is session-only, and the button starts on when the key is already true, from the user layer or the command line; without a console the button is drawn disabled, and in Shipping, which installs no lock hooks, it is absent). The table is: one row per entry of the profiler's `GetLockStats()` (Profiler.md "Lock statistics"), sorted by total wait descending and then by total hold, each row showing the lock name without its `lock:` prefix, its level, the acquisitions, the contended acquisitions with their percentage of the acquisitions, the total and the maximum wait and the total and the maximum hold, in milliseconds with two decimals. At most 12 rows fit and `m_fScroll` moves the window over the rest. The table's header line shows the frame summary's `m_uLockContended` and `m_uLockWaitUs`. While `devtools.profiler.captureLocks` is off (the key was cleared elsewhere after the button was pressed) the table shows the text `lock capture off (devtools.profiler.captureLocks)` instead of rows, and a frame with no lock activity shows `no lock activity`. The banks behind the table are the authoritative record of acquisitions, waits and holds; a row's tooltip says that the timeline's `lock:` and `lockwait:` zones come from ring events and can be short when the ring dropped events. Like the zone statistics, the lock statistics are the last assembled frame's and are not held by a freeze, so a frozen timeline shows the table with the header `live`.

## GPU view

Panel ID `gpu`. The GPU frame resolved this frame (the summary's `m_uGpuFrameLag` frames old) on one lane per queue (`GPU Graphics`, `GPU Compute`, `GPU Copy`), drawn as the timeline view draws CPU lanes, with pass names carrying their view suffix (`GTAO[Main]`). An uncalibrated frame is aligned to the render start and labelled `uncalibrated`. Beside it, a table lists every pass site with its frame total from the zone statistics (every occurrence of the pass name in the GPU frame summed, with the count when it is above 1), sorted by total, and the sum per view.

The GPU capture button `[capture GPU]` at the right of the view's header runs the console line `r.capture 1` (RHI's command, `Zenith2/Docs/Design/RHI/Recovery.md` → "Captures"); it is drawn only when a console is present and the RHI reports a capture tool attached (the console command's reply names the outcome, which the button shows for 3 s). `Dev.ProfilerCapture` runs the profiler capture instead (§ "Overlay state machine").

## Frame history view

Panel ID `history`. A bar graph of the last 600 frame times (one bar per frame, newest at the right), with:

- horizontal lines at 16.67 ms and 33.3 ms and at the hitch threshold when it differs;
- each bar split into CPU sim (blue), CPU render (green) and the remainder (grey) of the frame, the GPU time as a white tick on the bar;
- `HITCH` frames in red with a marker triangle above them; `EXCLUDED` frames (a budget run's initial-load window) drawn dim; frames where capture was off drawn as outlines;
- the pointer selects a frame: its summary is shown in a tooltip (with `contended` and `lock wait` lines when the frame's lock fields are non-zero), and when the frame is one of the detailed frames a click opens it in the timeline view (§ "Freeze and frame selection");
- statistics over the visible history: mean, p50, p95, p99 and maximum frame time, hitch count, computed with the nearest-rank rule of `Zenith2/Docs/Design/DEVT/Evidence.md` → "Statistics".

## Panels

Every tab after `frame`, `gpu` and `history` is a panel: a list of rows from the catalogue (§ "Built-in panel catalogue") or from a provider (§ "Panel providers").

- A catalogue row is evaluated once per frame while its panel is visible; its history (for min, max, mean and sparklines) is that of the counters it reads, so the history columns are present from the first frame the panel opens.
- The counter panel (`counters`) lists every counter of the registry in byte order of name with kind, frame value, latest, history min, max and mean, filtered by the text typed after `profiler.filter <text>` in the console (a substring match, case-sensitive); it is the panel that shows any counter no catalogue row names (Z2-I-DEVT-07).
- A row whose expression reads a counter that no code has published shows `—` in dim text; a row with a budget value shows the comparison colour of § "Overlay layout".

## Expression language

Grammar (ASCII; whitespace between tokens is ignored):

```
expression := sum
sum        := product { ("+" | "-") product }
product    := unary { ("*" | "/") unary }
unary      := "-" unary | primary
primary    := number | counter | call | "(" expression ")"
number     := digit { digit } [ "." digit { digit } ]
counter    := name                                   ; the counter's frame value
name       := letter { letter | digit | "_" | "." }  ; at most 63 bytes
call       := function "(" [ argument { "," argument } ] ")"
argument   := expression | pattern | field
pattern    := "\"" { any byte but "\"" } "\""        ; a counter or site name with at most one "*"
field      := letter { letter }
```

| Function | Value |
|---|---|
| `frame(c)` | the counter's frame value (the same as writing `c`) |
| `latest(c)` | the counter's latest value |
| `hmin(c)`, `hmax(c)`, `hmean(c)` | over the counter's history |
| `sum("p")` | the sum of the frame values of every counter matching the pattern |
| `each("p")` | in an `EACH` row: one row per matching counter, the label's `{}` replaced by the `*` capture |
| `count("p")` | the number of published counters matching the pattern |
| `zone_ms("s")`, `zone_max_ms("s")`, `zone_count("s")` | the site's frame statistics (Profiler.md "Zone statistics"): every closure of the site in the frame accumulated, on every thread, reset once per frame; `zone_ms` is the inclusive total in ms and `zone_count` the closure count, each summed over the sites a pattern matches; `zone_max_ms` is the longest single closure among them (a maximum, never a sum); a site with no closure in the frame reads 0 |
| `frame_us()` | the last summary's frame time in µs |
| `frame_field(f)` | a summary field: `cpuSimUs`, `cpuRenderUs`, `gpuUs`, `presentIntervalUs`, `tickCount`, `zoneCount`, `droppedEvents`, `scaleX`, `scaleY` |
| `wall_ns()` | the frame's wall time in ns (`frame_us() × 1000`), for per-thread percentages |
| `max(a, b)`, `min(a, b)`, `abs(a)` | arithmetic |

Compilation produces a byte code of at most 64 instructions (`PUSH_CONST`, `PUSH_COUNTER id`, `PUSH_FUNC kind id`, `ADD`, `SUB`, `MUL`, `DIV`, `NEG`, `MIN`, `MAX`, `ABS`) with a stack depth of at most 16; counter names are resolved to IDs lazily (an unpublished name is retried every 60 frames, so a counter first published mid-run appears). Patterns are matched against the registry every 60 frames and the matching ID list is cached in the expression's arena (at most 256 IDs). Compile errors name the column: `unknown function`, `expected ')'`, `name longer than 63 bytes`, `two wildcards in a pattern`, `expression too long`, `stack too deep`. The catalogue is compiled at `Zenith_ProfilerOverlay::Create`, so a malformed row fails `Test_ProfilerPanels_CatalogueCompiles` at the latest.

## Built-in panel catalogue

`Internal/Panels/Zenith_PanelCatalogue.cpp` holds the tab order; each area's rows live in `Internal/Panels/Zenith_Panels<Area>.cpp` as one `inline constexpr Zenith_ProfilerRowDesc axPANEL_ROWS_<AREA>[]` table. DEVT owns the files' mechanism and the area owns its rows' content. Who writes the rows:

- The Stage 2 tabs (Memory, Jobs, Render, RHI, Platform, ECS, Scene, Assets, Compression, Script, Environment, and DEVT's own) are written by DEVT's Stage 2 panel-catalogue tickets (`Zenith2/Docs/Design/DEVT.md` → "Stage 2", item 6), one file per area, transcribing the rows of that area's "On-screen profiler rows" heading as it stands when the ticket is drafted; an area's ticket that publishes the counters is not asked to edit DEVT's files, and a row whose counter is not yet published shows `—` until it is (§ "Panels").
- From Stage 3, an area ticket that adds a row to its own "On-screen profiler rows" heading adds it to its `Zenith_Panels<Area>.cpp` table in the same diff, and an area whose features land in Stages 3 to 5 adds its tab and file in the ticket that lands its first counters. Row DV31 records this rule for every area whose rows the catalogue carries (JOBS, CORE, RPROD, RHI, PLAT, ECS, SCENE, ASSET, COMPRESS, SCRIPT, ENV) and for the areas of Stages 3 to 5.

| Tab | ID | Owner area | Rows from |
|---|---|---|---|
| Frame | `frame` | DEVT | § "Timeline view" |
| GPU | `gpu` | DEVT, RPROD data | § "GPU view" |
| History | `history` | DEVT | § "Frame history view" |
| Memory | `memory` | CORE, RPROD data | `Zenith2/Docs/Design/CORE.md` → "On-screen profiler rows"; RPROD's transient-memory row |
| Jobs | `jobs` | JOBS | `Zenith2/Docs/Design/JOBS.md` → "On-screen profiler rows" |
| Graph | `graph` | GRAPH (provider) | § "Graph view" |
| Render | `render` | RPROD | `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "On-screen profiler rows" |
| RHI | `rhi` | RHI | `Zenith2/Docs/Design/RHI.md` → "On-screen profiler rows" |
| Platform | `platform` | PLAT | `Zenith2/Docs/Design/PLAT.md` → "On-screen profiler rows" |
| ECS | `ecs` | ECS | `Zenith2/Docs/Design/ECS.md` → "On-screen profiler rows" |
| Scene | `scene` | SCENE | `Zenith2/Docs/Design/SCENE.md` → "On-screen profiler rows" |
| Assets | `assets` | ASSET | `Zenith2/Docs/Design/ASSET.md` → "On-screen profiler rows" |
| Compression | `compress` | COMPRESS | `Zenith2/Docs/Design/COMPRESS.md` → "On-screen profiler and frame dumps" |
| Script | `script` | SCRIPT | `Zenith2/Docs/Design/SCRIPT.md` → "On-screen profiler rows summary" |
| Environment | `env` | ENV | `Zenith2/Docs/Design/ENV.md` → "On-screen profiler rows" |
| Counters | `counters` | DEVT | § "Panels" |
| Startup | `startup` | MOD (provider) | § "Panel providers" |
| Config | `config` | CORE (provider) | § "Panel providers" |
| DevTools | `devtools` | DEVT | `Zenith2/Docs/Design/DEVT.md` → "Counters" |

Areas whose features land in Stages 3 to 5 (STREAM, ANIM, AUDIO, UI, VFX, SAVE, GAME and the physics and AI modules) add their tab and file in the ticket that lands their first counters; the tab order places them before `counters`.

Examples of the encoding, from JOBS's table:

```cpp
// Zenith2/Engine/ZenithProfiling/Internal/Panels/Zenith_PanelsJobs.cpp
inline constexpr Zenith_ProfilerRowDesc axPANEL_ROWS_JOBS[] = {
	{ "Steals per frame", "jobs.steals", PROFILER_ROW_KIND_SPARKLINE, PROFILER_UNIT_COUNT, 1.0f, "", "", 0.0f, "exact" },
	{ "Steal failure ratio", "jobs.steal_failures / (jobs.steals + jobs.steal_failures)", PROFILER_ROW_KIND_VALUE,
		PROFILER_UNIT_RATIO, 1.0f, "", "", 0.0f, "exact" },
	{ "Idle % {}", "each(\"jobs.idle_ns.*\") / wall_ns() * 100", PROFILER_ROW_KIND_EACH, PROFILER_UNIT_PERCENT, 1.0f, "",
		"", 0.0f, "may spill" },
	{ "Scheduler overhead", "jobs.overhead_ns / 1000000", PROFILER_ROW_KIND_BAR, PROFILER_UNIT_MILLISECONDS, 1.0f, "",
		"Z2-B-JOBS-001", 0.0f, "approximate" },
	{ "Sim ticks", "sim.ticks", PROFILER_ROW_KIND_VALUE, PROFILER_UNIT_COUNT, 1.0f, "", "Z2-B-JOBS-002", 0.0f, "exact" },
};
```

`m_fBudgetValue` stays 0 while the row's budget is `pending` in `Zenith2/Docs/Budgets.md` → "Row index"; the area's budget-allocation ticket sets it with the allocated value. The memory panel (plan §2: "heap, pools, GPU heaps, aliasing savings") has five sections, in this order:

| Section | Rows (catalogue expressions unless a provider is named) | Data owner |
|---|---|---|
| Governor and heap | `latest(memory.level)`; `memory.process_bytes`; `memory.device_local_bytes`; `memory.unattributed_bytes`; `core.heap.live_bytes`, `core.heap.live_allocations`, `core.heap.peak_bytes`, `core.heap.failed_allocations`; `each("memory.subsystem.*_budget_bytes")` beside each subsystem's resident bytes; the `memory.subsystem.*_reserved_bytes` rows and `memory.subsystem_refusals`, `memory.peak_refusals` | CORE (`Zenith2/Docs/Design/CORE.md` → "On-screen profiler rows") |
| Arenas | `each("*.used_bytes")` with the matching `*.reserved_bytes` as the bar limit and `*.high_water_bytes` beside it | each arena's owner (CORE's arena counter rule) |
| Pools | `each("*.live_slots")` with the matching `*.capacity_slots` as the bar limit, for every pool whose owner publishes the two gauges from `Zenith_RawPool::GetLiveSlots` and `GetCapacitySlots` (the pool-row convention, row DV29 (d)); then RHI's GPU pools, `RHI.Memory.Category.PoolsBytes` and `RHI.Memory.Category.TablesBytes` | the pool's owner; RHI |
| GPU heaps | `RHI.Memory.DeviceLocalUsageBytes` against `RHI.Memory.DeviceLocalBudgetBytes`; `each("RHI.Memory.Heap*.UsageBytes")` with the matching `.BudgetBytes` as the bar limit; the eight `RHI.Memory.Category.*Bytes` rows | RHI (`Zenith2/Docs/Design/RHI.md` → "Counters") |
| Transient memory and aliasing | the provider `memtransient` below | RPROD |

Its transient-memory section is the provider `memtransient` of § "Panel providers", which reads RPROD's `Flux_TransientMemoryStats` snapshot (dedicated bytes, aliased bytes, savings in bytes and percent, the 300-frame aliased peak, the group count, the aliasing mode) and is the "transient memory with and without aliasing" of Z2-D-057. The Luau panel (`script`) shows SCRIPT's per-VM script time and GC rows in Stage 3, when SCRIPT's gameplay VMs land.

Exactness: a row over counters JOBS marks "may spill" shows its exactness in the dim column, and its tests assert interval sums, never per-frame identities (`Test_Profiler_ContentionRowsFromCounters`, `Zenith2/Docs/Design/DEVT/Tests.md` → "Integration suites").

## Panel providers

| Provider (ZenithRuntime `Internal/DevTools/Zenith_RuntimePanelProviders.cpp`) | Panel ID | Content |
|---|---|---|
| `Zenith_StartupPanelProvider` | `startup` | `Zenith_Runtime::GetStartupReport()`: each startup step's name, duration and outcome, the total, the slowest three |
| `Zenith_ConfigPanelProvider` | `config` | the configuration store's dump: every key, its value, its winning layer, flagged when not the default; filtered by `profiler.filter` |
| `Zenith_GraphPanelProvider` | `graph` | § "Graph view" |
| `Zenith_PlatformPanelProvider` | `platformev` | the frame's platform events, the window state, the I/O queue depths (the same data as the frame dump's platform section) |
| `Zenith_TransientMemoryPanelProvider` | `memtransient` (drawn as the last section of the `memory` tab) | `Flux_Renderer::GetTransientMemoryStats()`, which forwards `Flux_GraphCaches::GetTransientMemoryStats()` (row DV5) |

Any library above ZenithProfiling may implement a provider; ZenithRuntime registers its own at startup step 13 (after the frame graph is built), a module registers its own through `Zenith_ModuleStartContext::GetProfilerOverlay()` (row DV12; VFX's particle-system statistics and STREAM's per-world state are such panels), and each token is destroyed at shutdown before the providers' owners. A provider is called only while its panel is the visible tab, so its cost is zero otherwise.

## Overlay input

`Zenith_OverlayInputAdapter` (ZenithRuntime) fills `Zenith_ProfilerOverlayInput` at frame-loop step 3 from PLAT's presentation frame (`Zenith_InputSystem::GetFrame()`, read after `Platform.Pump`):

| Field | Source |
|---|---|
| `m_bToggle` | `Dev.ToggleProfiler` (0x00F00100) `m_bPressed` |
| `m_bNextPanel`, `m_bPreviousPanel`, `m_bFreeze`, `m_bCapture`, `m_bOlderFrame`, `m_bNewerFrame` | `Dev.ProfilerNextPanel` (0x00F00104, `F4`), `Dev.ProfilerPreviousPanel` (0x00F00105, `Shift+F4`), `Dev.ProfilerFreeze` (0x00F00106, `F5`), `Dev.ProfilerCapture` (0x00F00107, `F6`), `Dev.ProfilerOlderFrame` (0x00F00108, `F7`), `Dev.ProfilerNewerFrame` (0x00F00109, `F8`), all `BUTTON` in `Engine.Dev`, `developmentOnly` (row DV13 (a)); no two share a key or a modifier combination |
| pointer fields | the presentation frame's pointer position for the overlay's window and view; touch flagged from the device kind |
| `m_fScroll` | `UI.Scroll` y while the pointer is over the overlay |

Claims: the adapter is a `Zenith_InputClaimant` registered after UI and before the editor's claimant; its `ClaimPointers` claims every pointer down for which `Zenith_ProfilerOverlay::HitTest` is true (using the previous frame's layout, which is what the user saw), so a click on the overlay never reaches the game or the UI below; `ClaimActions` claims `UI.Scroll` while the pointer is over a scrollable region. In `HIDDEN` nothing is claimed. Tests (in `ZenithRuntime_Tests`, Stage 3, with PLAT's input system driven by injected transitions): `Test_OverlayInputAdapter_ClaimsPointersOverOverlay` (a press inside a drawn region is claimed and absent from the game's frame; a press outside reaches the game), `Test_OverlayInputAdapter_ClaimsScrollOverScrollable` (`UI.Scroll` is claimed over a scrollable panel and not elsewhere), `Test_OverlayInputAdapter_NothingClaimedWhenHidden`.

Before PLAT's input system exists (it is `since Z2 Stage 3`), the Stage 2 overlay is driven by the configuration key and the console commands of § "Overlay state machine", and the adapter is the Stage 3 ticket that lands with PLAT's presentation frame.

## Overlay drawing

The overlay records screen-space primitives into `Zenith_DebugDraw` on the main thread during `Update`: `ScreenRect` for backgrounds, bars and zones, `ScreenLine` for axes, sparklines and markers, `ScreenText` for text (DebugDraw.md "Zenith_DebugDraw header"), all with duration one frame, the overlay's world tag and the `SCREEN` flag. The main thread's records go into the recorder's main ring; the drain node of the next frame-graph iteration moves them into the staging store and the extractors into the render world (DebugDraw.md "Debug-draw extractor"), so the overlay of frame `k`'s data appears with frame `k + 1`'s image; the one-frame lag is inherent to the pipelined hand-off and is visible only as the overlay trailing a sudden change by one frame.

Budget: in `FULL` the overlay records at most 6,000 rectangles, 4,000 lines and 1,500 text records carrying at most 12,000 glyphs per frame, 11,500 records in all (the timeline's zone merging enforces it; beyond it the timeline draws merged runs only and shows `<detail cut>`). Those records go into the recorder's main ring, which is sized to hold them plus the on-screen console's (`Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Recorder rings and drain": 16,384 records and 64 KiB of text); before drawing each view the overlay compares the view's worst-case record count with `Zenith_DebugDraw::GetFreeRecords()` and draws the merged form when it does not fit, so a `FULL` frame is never truncated by a ring drop. After expansion the records stay within the recorder's per-frame caps (`Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Overlay upload budget"). The overlay's own memory is three arenas allocated by `Create`: a 256 KiB layout arena (panel layouts, the tab strip, row caches), a 128 KiB frame arena reset every `Update` (formatted text, provider rows) and a 64 KiB expression arena (compiled catalogue expressions and pattern ID lists), 458,752 bytes in all, part of Z2-B-DEVT-001. The CPU cost is the zone `Profiler.Overlay`, inside Z2-B-DEVT-002; the GPU cost is the `DebugOverlay` pass, Z2-B-DEVT-003.

## Freeze and frame selection

- `SetFrozen(true)` calls `Zenith_Profiler::Freeze(true)`: the eight detailed frames are held (Profiler.md "Frame history and detailed frames") and the timeline shows the newest held frame; the history view, counters and top bar stay live.
- While frozen, the frame selector (a row of eight cells at the top of the timeline, `Dev.ProfilerOlderFrame` and `Dev.ProfilerNewerFrame` stepping one held frame at a time and stopping at the oldest and newest) picks one of the held frames; a click on a history bar of a held frame selects it. The selector actions have their own bindings, so they never collide with the panel actions (`Shift+F4` is `Dev.ProfilerPreviousPanel`).
- Unfrozen, clicking a history bar of one of the last eight frames shows that frame until the next frame arrives; older bars show their summary only.
- `profiler.freeze` and `profiler.unfreeze` console commands do the same; freezing is also how a capture's on-screen check is made before saving one (`profiler.capture` captures from the next frame regardless of freeze).

## Graph view

`Zenith_GraphPanelProvider` (panel `graph`) shows the CPU frame graph of the last iteration from `Zenith_FrameExecutor::GetLastTimings()` and `GetLastTimeline()` (`Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameExecutor.h"): one row per node in schedule order, indented by subgraph, with the node's stage, its time, its critical-path mark and its world tag, and the render graph's pass list from the last Flux frame dump when one has been written in this session (`r.graph.dump 1`). The provider reads a dump file with its private member `ReadDumpFile` (a development-only translation unit, forbidden in Shipping, `Zenith2/Docs/Design/DEVT/Console.md` → "Shipping absence of development services"), which uses CORE's JSON reader and GRAPH's version check `Zenith_GraphDumpVersionIsReadable`; a dump whose `schemaVersion` is above the reader's is refused with the text of `Zenith_GraphDumpVersionMessage`, shown in the panel as one dim row, and the rest of the panel stays live.

## Editor overlay

The editor creates its own `Zenith_ProfilerOverlay` over the editor process's profiler, with `m_uWorldTag` set to the editor viewport's view so its draws appear in `VIEW_KIND_EDITOR` views alone, and the play-in-editor world's draws keep their own tag. The editor's toolbar has a "Profiler" toggle that calls `CycleState`, the panel tabs are the same catalogue, and the editor adds one provider of its own (`editor`: the editor frame's panel times), owned by EDIT. The editor does not use the `Engine.Dev` actions while its own UI has keyboard focus; it maps `F3` through its own shortcut table to the same calls. The editor overlay lands in Stage 3 with the minimal editor.

## S24 overlay

On Android the overlay is the same code with three differences: the UI scale is 2 by default (§ "Overlay layout"); the toggle is `/gamepad/back` with `/gamepad/leftStickPress` (PLAT's binding), or a three-finger tap (`/gesture/threeFingerTap` bound to `Dev.ToggleProfiler` in the engine bindings file); PLAT's gesture device has no three- or four-finger tap today (`Zenith2/Docs/Design/PLAT/Input.md` lists `tap` … `position`), so row DV13 (a) asks PLAT for two recognisers, `GESTURE_CONTROL_THREE_FINGER_TAP` 9 (`threeFingerTap`) and `GESTURE_CONTROL_FOUR_FINGER_TAP` 10 (`fourFingerTap`), discrete taps of three and four simultaneous touches within the tap slop and duration of the existing tap, pulsing a button like `twoFingerTap`; until they exist the gamepad binding is the S24 toggle, and the tab strip is touch-sized (two cells high, hit targets of at least 48 dp); and the top bar in `COMPACT` adds PLAT's thermal counters (`platform.thermal_status`, `platform.thermal_headroom`). Its cost is measured on the S24 as part of Z2-B-DEVT-002 and Z2-B-DEVT-003 in the Stage 3 device run (`Zenith2/Docs/Design/DEVT/Tests.md` → "Device and acceptance runs").
