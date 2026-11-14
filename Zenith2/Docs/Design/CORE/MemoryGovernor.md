# CORE design: the memory governor

This file is part of the CORE design document family (entry: `Zenith2/Docs/Design/CORE.md`). The governor's model, public types, budgets table, pressure-level sources, shedding table, transition-peak rules, subsystem-reservation rules and forced-level test names are `Zenith2/Docs/Architecture.md` → "Memory governor", which this file cites and never restates. It adds the remaining API surface, the generated budget table and its generator, the tuning keys, the level-evaluation state machine, the reservation algorithm, the subsystem reservation algorithm, the accounting and residual computation, the OS queries per platform, the counters, and the full test list.

## Scope of this file

| Topic | Fixed by `Zenith2/Docs/Architecture.md` → "Memory governor" | Added here |
|---|---|---|
| `Zenith_MemoryGovernor`, `Zenith_MemoryConsumer`, `Zenith_TransitionReservation`, `Zenith_SubsystemReservation`, `Zenith_MemoryBudgets`, `Zenith_MemoryThresholds`, the enumerations, `Zenith_MemoryError` | declarations ([Z2-N-ARCH-811] to [Z2-N-ARCH-826]) | contract blocks, the members below, enumerator numbering from 1 |
| budget sources | the table "Budgets" (configuration keys `memory.budget.*`, `memory.peak.*`, `memory.threshold.*`) and [Z2-N-BUDGET-144], [Z2-N-BUDGET-145] | the generated table, its generator and the key defaults |
| pressure levels | the table "Pressure levels" and the hysteresis rule | the state machine and its evaluation order |
| shedding | the table "Shedding per subsystem" (the single source, [Z2-N-ARCH-822]) | nothing: consumers implement it; this file names the counters that prove it ran |
| transition peaks | [Z2-N-ARCH-815], [Z2-N-ARCH-816] | the lock-free reservation algorithm |
| subsystem reservations | [Z2-N-ARCH-825], [Z2-N-ARCH-826] | the lock-free subsystem reservation algorithm, its interaction with the transition reservations and the pressure sources, its counters and tests |

## Additions to the governor API

`Zenith2/Docs/Architecture.md` → "Memory governor" declares the whole governor API, including `MEMORY_ERROR_OUT_OF_MEMORY`, `Zenith_MemoryBudgets::m_uPendingSubsystemMask`, `Zenith_MemoryConsumer::GetDeviceLocalBytes`, `Create`'s trailing `Zenith_FaultInjector*`, `ClearForcedLevel`, `BindOOMPressureFlag`, `GetStats`, `GetReservedPeakBytes`, the subsystem reservation API (`MEMORY_ERROR_SUBSYSTEM_REFUSED`, `Zenith_MemoryError::m_eSubsystem`, `Zenith_SubsystemReservation`, `ReserveSubsystemBytes`, `GetReservedSubsystemBytes`), `Zenith_ReadMemoryBudgets` and `Zenith_QueryProcessResidentBytes`; its table "Assumed types" names this document as the declarer of `Zenith_MemoryGovernorStats`. CORE adds only the statistics types, the governor's fault point and the reservations' private constructors:

```cpp
// Zenith2/Engine/ZenithCore/Zenith_MemoryGovernor.h — beside the declarations of Architecture.md "Memory governor"
struct Zenith_MemorySubsystemUsage
{
	uint64 m_ulResidentBytes = 0;       // Σ GetResidentBytes of the subsystem's consumers, this frame
	uint64 m_ulBudgetBytes = 0;        // the bound of "Subsystem reservation algorithm": the budget, or the process total when pending
	uint64 m_ulReservedBytes = 0;      // outstanding subsystem reservations, GetReservedSubsystemBytes at GetStats
	bool m_bPending = false;
	Zenith_MemoryPressureLevel m_eLevel = MEMORY_PRESSURE_NORMAL;   // this subsystem's own source level
};

struct Zenith_MemoryGovernorStats
{
	Zenith_MemoryPressureLevel m_eLevel;
	uint64 m_ulProcessBytes;
	uint64 m_ulDeviceLocalBytes;
	uint64 m_ulUnattributedBytes;
	Zenith_MemorySubsystemUsage m_axSubsystems[MEMORY_SUBSYSTEM_COUNT];
	uint64 m_aulReservedPeakBytes[TRANSITION_PEAK_COUNT];
	uint64 m_ulLevelChanges;
	uint64 m_ulPeakRefusals;
	uint64 m_ulSubsystemRefusals;      // MEMORY_ERROR_SUBSYSTEM_REFUSED returns, cumulative
	uint64 m_ulOOMEvents;
};

constexpr uint32 uFAULT_POINT_GOVERNOR_LOW_MEMORY_HANDLE = 0x434F0003;   // Windows: before CreateMemoryResourceNotification
```

Architecture declares the two reservation types with private members and no constructor that sets them; CORE adds to each, in its `private:` section, the constructor the governor uses and the friend declaration that lets it:

```cpp
// in class Zenith_TransitionReservation, private:
	Zenith_TransitionReservation(Zenith_MemoryGovernor* pxGovernor, Zenith_TransitionPeakKind eKind, uint64 ulBytes);
	friend class Zenith_MemoryGovernor;
// in class Zenith_SubsystemReservation, private:
	Zenith_SubsystemReservation(Zenith_MemoryGovernor* pxGovernor, Zenith_MemorySubsystem eSubsystem, uint64 ulBytes);
	friend class Zenith_MemoryGovernor;
```

No other code can construct a reservation that holds bytes; a reservation is obtained only from `ReserveTransitionPeak` or `ReserveSubsystemBytes`.

Contract blocks (written in the header in full):

| Type | Kind | Thread affinity | Key content |
|---|---|---|---|
| `Zenith_MemoryGovernor` | type | `MAIN_THREAD` except `ReserveTransitionPeak`, `ReserveSubsystemBytes`, `ReportOSPressure`, `GetReservedPeakBytes` and `GetReservedSubsystemBytes` (`ANY_THREAD`), per Architecture | Ownership: created by `Create` at startup step 8 and owned by ZenithRuntime; destroyed after every consumer has unregistered (`ZENITH_CHECK`). Reentrancy: `NONE` — a consumer's `OnPressureLevelChanged` calls no governor member. Errors: `Create` returns `MEMORY_ERROR_BAD_BUDGETS` or `MEMORY_ERROR_OUT_OF_MEMORY`; `ReserveTransitionPeak` returns `MEMORY_ERROR_PEAK_REFUSED`; `ReserveSubsystemBytes` returns `MEMORY_ERROR_SUBSYSTEM_REFUSED`. Units: bytes and percentages. |
| `Zenith_MemoryConsumer` | interface | `GetResidentBytes` `ANY_THREAD` (relaxed reads); `OnPressureLevelChanged` on the main thread in the frame prologue | Ownership: implemented by subsystems, registered by reference by the composition root or a module start, unregistered before destruction. Reentrancy: `NONE`. |
| `Zenith_TransitionReservation` | type | `ANY_THREAD` | Ownership: move-only; releases its bytes in its destructor; must not outlive the governor (the governor reports live reservations at shutdown, [Z2-N-ARCH-816]). |
| `Zenith_SubsystemReservation` | type | `ANY_THREAD` (one thread at a time per object; release may happen on a thread other than the reserving one) | Ownership: move-only; held by the operation that reserved (for SCENE, beside the preparation's arena); releases its bytes in its destructor or earlier through `Shrink`; must not outlive the governor (the governor reports live reservations at shutdown, [Z2-N-ARCH-826]). Invalidation: a moved-from reservation holds 0 bytes and no governor. Errors: none of its members fails; `Shrink` above `GetBytes()` is `ZENITH_ASSERT`. Units: bytes. Reentrancy: `NOT_APPLICABLE`. |
| `Zenith_OOMPressureFlag` | type | `ANY_THREAD` | `Zenith2/Docs/Design/CORE/Memory.md` → "OOM pressure relay" |

## Consumer registration

- `RegisterConsumer` appends to a `Zenith_InlineArray<Zenith_MemoryConsumer*, 64>` (64 consumers at most; the 65th is `ZENITH_CHECK`, because the consumer set is fixed by the composition's module list, which bounds it); a consumer registered twice is `ZENITH_CHECK`.
- `UnregisterConsumer` removes it preserving order; calling either from inside `OnPressureLevelChanged` is `ZENITH_ASSERT` through the governor's `Zenith_ReentrancyGuard`.
- A consumer registered while the level is not `MEMORY_PRESSURE_NORMAL` receives `OnPressureLevelChanged(MEMORY_PRESSURE_NORMAL, current)` at the next `Update`, so it sheds like every other consumer.

## Generated budget table

<!-- elaboration -->
The governor's budgets and thresholds come from `Zenith2/Docs/Budgets.md` through a table generated at configure time ([Z2-N-BUDGET-144], [Z2-N-BUDGET-145]), never from numbers written in engine code ([Z2-N-ARCH-820]).

| Aspect | Rule |
|---|---|
| generator | `Zenith2/CMake/ZenithBudgetTable.cmake`, a CMake-language script with the function `zenith_generate_budget_table(<BudgetsMd> <ArchitectureMd> <OutputHeader>)`, called by the root `Zenith2/CMakeLists.txt` before the engine libraries are added; `Zenith2/Docs/Budgets.md` and `Zenith2/Docs/Architecture.md` are added to `CMAKE_CONFIGURE_DEPENDS`, so an edit of either re-runs configure (the Ninja automatic re-run, Z2-D-001) |
| output | `${ZENITH_OUT}/generated/ZenithCore/Internal/Generated/Zenith_MemoryBudgets.generated.h`, listed in ZenithCore's `zenith_add_library(… GENERATES Internal/Generated/Zenith_MemoryBudgets.generated.h)` and included as `"Internal/Generated/Zenith_MemoryBudgets.generated.h"` by `Internal/Governor/Zenith_MemoryBudgetDefaults.cpp` alone; the path of [Z2-N-BUDGET-145] is this include path, and the file lives under `out/` ([Z2-N-ARCH-025]) |
| input | the table under `## Row index` of `Budgets.md` (columns `ID`, `Name`, `Status`, `Area`, `Value`, `Unit`, `Producer`), and the table "Budgets" under `### Budgets` of `Architecture.md` → "Memory governor" (columns `Budget`, `Budgets row(s)`, `Status and producer`) |
| selected rows | every row the summed lists of Architecture's table "Budgets" name (below); each must be of unit `GB`, `MB`, `KB`, `bytes` or `% of platform total` |
| value grammar | the Value column rule of `Zenith2/Docs/Budgets.md` → "Purpose versioning and change rule" ([Z2-N-BUDGET-009]), parsed as written there: `pending`; a number (one value for every platform the row covers); `Win: <n>; S24: <n>` (one value per platform); or, for a memory row resident on both sides of the Windows CPU/GPU split, `Win RAM: <n>; Win VRAM: <n>; S24: <n>`; the generator adds no grammar of its own |
| unit conversion | GB × 10^9, MB × 10^6, KB × 10^3 (`Zenith2/Docs/Budgets.md` → "Memory budgets", [Z2-N-BUDGET-141]); percentages kept as integers 1 … 100 |
| platform columns | Android host: the `S24:` value (or the single number); Windows: the `Win RAM:`/`Win VRAM:` values of a dual-resident row, and otherwise the `Win:` value (or the single number) placed on the host side when the row is a member of the bound `MEM-WIN-RAM` and on the device side when it is a member of `MEM-WIN-VRAM`; the Linux headless test host takes the Windows host values and device 0 |
| Allocation sums | the generator reads, from the table "Allocation sums" under `## Purpose versioning and change rule`, the Member rows cells of `MEM-WIN-RAM` and `MEM-WIN-VRAM` alone, and tokenises each cell in this order: (1) extract every group `max(<IDs separated by ", ">)`; its IDs are the transition-peak rows (members of the bound, not summed into any subsystem); (2) split the remainder at `; ` into segments, each either unlabelled or labelled `dual-resident:`; (3) split each segment at `, ` into IDs, stripping a `×<n>` suffix from an ID. An unlabelled ID is a member of that bound; an ID of a `dual-resident:` segment is dual-resident (its host and device values come from its own `Win RAM:`/`Win VRAM:` cells); an empty segment or any token that is not an ID after these steps stops configure |
| pending rows | host value = the platform's process total (Z2-B-CORE-002 on Windows and Linux, Z2-B-CORE-001 on Android), device value = Z2-B-CORE-003 on Windows, `m_bPending = true`; a pending subsystem therefore never sheds by its own budget and is reported |
| failure | a selected row missing, a value not matching the grammar, or a unit outside the list stops configure with `zenith: Budgets.md Row index: <ID>: <reason>` |

Generated content:

```cpp
// Generated by Zenith2/CMake/ZenithBudgetTable.cmake from Zenith2/Docs/Budgets.md "Row index". Do not edit.
#pragma once
#include "Zenith_Types.h"

enum Zenith_BudgetRowUnit : uint8 { BUDGET_ROW_UNIT_BYTES = 1, BUDGET_ROW_UNIT_PERCENT };

struct Zenith_GeneratedBudgetRow
{
	const char* m_szRowID;             // "Z2-B-CORE-004"
	Zenith_BudgetRowUnit m_eUnit;
	bool m_bPending;
	uint64 m_aulHostValue[3];         // index Zenith_BuildPlatform − 1: bytes, or percent
	uint64 m_aulDeviceValue[3];       // device-local bytes (Windows VRAM side); 0 elsewhere
};

inline constexpr Zenith_GeneratedBudgetRow axGENERATED_BUDGET_ROWS[] = {
	// one initialiser per selected row, in ascending row-ID byte order
};
inline constexpr const char* szGENERATED_BUDGETS_VERSION = "v1";   // the Budgets.md version line
```

The rows summed for each budget are exactly those of `Zenith2/Docs/Architecture.md` → "Memory governor" (table "Budgets", [Z2-N-ARCH-823]); this document keeps no copy of that mapping. The generator reads it from Architecture's table: for each row whose `Budget` cell is listed below, the summed rows are the `Z2-B-…` IDs of the `Budgets row(s)` cell that precede its first `; ` (the text after it names rows that are not summed, such as Z2-B-RHI-003 and Z2-B-SCRIPT-010/015). The generator adds only the key each budget fills:

| `Budget` cell of Architecture's table | Key | Value |
|---|---|---|
| `MEMORY_SUBSYSTEM_<NAME>` in backticks (eight rows) | `memory.budget.<name>Bytes` (`renderingBytes`, `streamingBytes`, `audioBytes`, `scriptsBytes`, `editorCachesBytes`, `assetsBytes`, `simulationBytes`, `otherBytes`) | the sum of the rows' host and device values for the build platform |
| `process total, S24` | `memory.budget.processBytes` (Android) | the row's value |
| `process total, Windows` | `memory.budget.processBytes` from the first ID, `memory.budget.deviceLocalBytes` from the second (Windows, and the Linux headless test host) | each row's value; `deviceLocalBytes` is 0 on Android |
| `transition peaks` | `memory.peak.streamingDoubleResidencyBytes`, `memory.peak.worldTransitionBytes`, `memory.peak.textureSwapBytes`, from the three IDs in order | each row's value |
| `pressure thresholds …` | `memory.threshold.warningPercent`, `memory.threshold.criticalPercent`, from the two IDs in order | each row's percentage |

A `Budget` cell of that list missing from Architecture's table, a summed ID absent from `## Row index`, or an ID count different from the one stated above stops configure with `zenith: Architecture.md Memory governor Budgets: <cell>: <reason>`. The generated header carries the result as a second array beside the rows:

```cpp
enum Zenith_GeneratedBudgetSide : uint8 { GENERATED_BUDGET_SIDE_BOTH = 1, GENERATED_BUDGET_SIDE_HOST, GENERATED_BUDGET_SIDE_DEVICE };

struct Zenith_GeneratedBudgetKey
{
	const char* m_szKey;                 // "memory.budget.renderingBytes"
	uint16 m_auRowIndices[16];          // indices into axGENERATED_BUDGET_ROWS of the summed rows
	uint8 m_uRowCount;
	Zenith_GeneratedBudgetSide m_eSide;  // which values of the rows are summed
};

inline constexpr Zenith_GeneratedBudgetKey axGENERATED_BUDGET_KEYS[] = {
	// one initialiser per key of the list above, in its order
};
```

`Zenith_RegisterCoreConfigKeys` registers every `memory.*` key of `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Core configuration keys" with the defaults summed from `axGENERATED_BUDGET_KEYS`; `Zenith_ReadMemoryBudgets` fills `Zenith_MemoryBudgets` from the resolved values and sets `m_uPendingSubsystemMask` from the table's pending flags.

## Tuning keys

The four keys `memory.subsystemCriticalPercent`, `memory.hysteresisPercent`, `memory.hysteresisMs` and `memory.androidLowMemoryHoldMs` are not budget rows; their values are set by the Stage 1 per-system budget ticket for CORE ([Z2-N-ARCH-820]) in ZenithCore's platform configuration fragments:

<!-- assumption: pending PLAT -->
| File | Contents |
|---|---|
| `Zenith2/Engine/ZenithCore/Internal/Config/Platform/windows.config.json` | the four keys for Windows |
| `Zenith2/Engine/ZenithCore/Internal/Config/Platform/android.config.json` | the four keys for the S24 |
| `Zenith2/Engine/ZenithCore/Internal/Config/Platform/linux.config.json` | the four keys for the headless test host |

The cook merges every library's `Internal/Config/Platform/<platform>.config.json` fragment into `content:/Config/Platform/<platform>.config.json` (layer 2 of `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering"), refusing a key defined by two fragments.
<!-- /assumption -->

The keys are registered with `m_bRequired`, so a platform layer without them fails `ValidateRequired`, and `Zenith_MemoryGovernor::Create` is never reached (startup exit code 2 naming the key, [Z2-N-ARCH-819]).

## Level evaluation

`Update()` runs on the main thread as step 4 of `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue", in this order:

1. **Measure.** Read each consumer's `GetResidentBytes()` into its subsystem's sum; take the process and device-local bytes last reported by `ReportProcessUsage` (the composition root reports `Zenith_QueryProcessResidentBytes()` and the RHI's device-local usage inside prologue step 4 of `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue", immediately before `Update`); compute the unattributed residual (see "Accounting"); store each subsystem's sum into `m_aulMeasuredSubsystemBytes[e]` (relaxed), the snapshot the grant rule of "Subsystem reservation algorithm" reads until the next `Update`.
2. **Source levels.** Compute a level per source, each with its own hysteresis state:

| Source | Raw level | Clears one step down when |
|---|---|---|
| process | WARNING at `process ≥ processBytes × warning% / 100`, where `process` is the reported usage plus the outstanding transition reservations of "Reservation algorithm" plus the outstanding subsystem reservations of "Subsystem reservation algorithm" (every subsystem, pending ones included); CRITICAL at `≥ … × critical% / 100` | usage stays below `(threshold% − hysteresis%)` of the total for `hysteresisMs` |
| device-local (Windows) | the same thresholds against `deviceLocalBytes` | the same rule |
| each non-pending subsystem | WARNING at `resident ≥ budget`, where `resident` is the measured sum plus the subsystem's outstanding subsystem reservations ([Z2-N-ARCH-826]); CRITICAL at `≥ budget × subsystemCritical% / 100` | usage stays below `budget × (100 − hysteresis%) / 100` (WARNING), or below the critical bound less the same margin (CRITICAL), for `hysteresisMs` |
| Windows low-memory notification | WARNING while `QueryMemoryResourceNotification` on the `LowMemoryResourceNotification` handle reports true | not signalled for `hysteresisMs` |
| OS report (`ReportOSPressure`, from PLAT) | the reported level | `androidLowMemoryHoldMs` after the last report |
| OOM relay | CRITICAL when `Zenith_OOMPressureFlag::Consume()` is non-zero | `hysteresisMs` after the last raise |
| `ForceLevel` | the forced level, until `ClearForcedLevel` | removed at `ClearForcedLevel`, with no dwell |

3. **Combine.** The new level is the maximum of the source levels. The combined level has no dwell of its own: it falls in the `Update` in which the last source above the new level clears. The unattributed residual of "Accounting" is a reported diagnostic, not a pressure source.
4. **Apply.** When it differs from the current level: store it, increment `memory.level_changes`, and call `OnPressureLevelChanged(old, new)` on every consumer in registration order ([Z2-N-ARCH-811]); a consumer applies its shedding within one frame and reverts one frame after the level falls ([Z2-N-ARCH-821]).
5. **Publish.** Emit the counters of "Counters".

State machine of the combined level (evaluated once per `Update`, after every source's own state has been updated):

| State | Maximum of the source levels is higher | Maximum of the source levels is lower | Otherwise |
|---|---|---|---|
| NORMAL | go to it at once; notify | — | stay |
| WARNING | go to CRITICAL at once; notify | go to NORMAL; notify | stay |
| CRITICAL | — | go to the new maximum; notify once | stay |

- Rising is immediate. A measured source falls one step per satisfied dwell, so when measured sources alone hold the level, CRITICAL reaches NORMAL through WARNING with two notifications.
- Clearing the forced level removes that source; the fall then follows the remaining sources' own states, so a forced WARNING over otherwise-NORMAL sources falls to NORMAL in the next `Update` with one notification, and a forced CRITICAL over otherwise-NORMAL sources falls to NORMAL with one `(CRITICAL, NORMAL)` notification.
- Timestamps come from `Zenith_GetMonotonicTimeUs()`; this is presentation-side state and never reaches simulation ([Z2-N-ARCH-812]).
- `ReportOSPressure` stores the level and the time into atomics (`ANY_THREAD`); `Update` reads them.

## Reservation algorithm

`ReserveTransitionPeak(eKind, ulBytes)` (`ANY_THREAD`), over `std::atomic<uint64> m_aulReserved[TRANSITION_PEAK_COUNT]`:

1. Load `r = m_aulReserved[eKind]` (relaxed).
2. If `ulBytes > peak − r` (the subtraction form; `peak` from the budgets), increment `m_ulPeakRefusals` and return `Zenith_Err(Zenith_MemoryError{ MEMORY_ERROR_PEAK_REFUSED, eKind, MEMORY_SUBSYSTEM_COUNT, ulBytes, peak − r })` (`m_eSubsystem` is `MEMORY_SUBSYSTEM_COUNT`, not applicable, in a peak refusal).
3. `compare_exchange_weak(r, r + ulBytes, acq_rel)`; on failure go to step 2 with the new `r`.
4. Return a `Zenith_TransitionReservation{ this, eKind, ulBytes }`.

- `Shrink(n)` (`n ≤ GetBytes()`, `ZENITH_ASSERT`) subtracts the difference with `fetch_sub`; the destructor subtracts the rest; a moved-from reservation holds 0 bytes.
- The process source of "Level evaluation" adds `Σ m_aulReserved` (and the subsystem reservations of "Subsystem reservation algorithm") to the measured process usage, so a transition in progress counts toward pressure before its allocations land ([Z2-N-BUDGET-146]); the double count while the new copy is allocated is conservative by design.
- The governor's destructor logs every non-zero transition reservation and every non-zero subsystem reservation ("Subsystem reservation algorithm"), each with `ZENITH_LOG_ERROR(xLOG_CATEGORY_CORE_GOVERNOR, …)` naming the kind or subsystem and the bytes, and only then `ZENITH_CHECK`s that all are zero ([Z2-N-ARCH-816], [Z2-N-ARCH-826]), so one failed run reports every live reservation.
- Consumers on refusal: streaming and texture swaps defer to a following frame; a world transition fails its request ([Z2-N-ARCH-815]; `Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition").

## Subsystem reservation algorithm

`ReserveSubsystemBytes(eSubsystem, ulBytes)` grants a reservation against one subsystem's budget for a single operation that is not a transition and must not allocate past that budget ([Z2-N-ARCH-825]); the first caller is SCENE's preparation of a scene whose decoded size exceeds `scene.prepareReserveThresholdBytes` (`Zenith2/Docs/Design/SCENE/Loading.md` → "Preparation operation"), reserving under `MEMORY_SUBSYSTEM_SIMULATION`.

### Subsystem reservation state

| Private member of `Zenith_MemoryGovernor` | Written by | Read by |
|---|---|---|
| `std::atomic<uint64> m_aulReservedSubsystemBytes[MEMORY_SUBSYSTEM_COUNT]` | `ReserveSubsystemBytes` (`compare_exchange_weak`, acq_rel); `Zenith_SubsystemReservation::Shrink`, its destructor and its move assignment (`fetch_sub`, release) | `ReserveSubsystemBytes` and `GetReservedSubsystemBytes` (relaxed); `Update` step 2, `GetStats` and the destructor (acquire) |
| `std::atomic<uint64> m_aulMeasuredSubsystemBytes[MEMORY_SUBSYSTEM_COUNT]` | `Update` step 1 (relaxed store of the subsystem's sum of `GetResidentBytes`; 0 before the first `Update`) | `ReserveSubsystemBytes` (relaxed) |
| `std::atomic<uint64> m_ulSubsystemRefusals` | `ReserveSubsystemBytes` on refusal (`fetch_add`, relaxed) | `GetStats`; the counter `memory.subsystem_refusals` |
| `uint64 m_aulSubsystemBoundBytes[MEMORY_SUBSYSTEM_COUNT]` | `Create`, once: `m_aulSubsystemBytes[e]` of the budgets, or `m_ulProcessBytes` when bit `e` of `m_uPendingSubsystemMask` is set ([Z2-N-ARCH-824]) | `ReserveSubsystemBytes`; `GetStats` (`m_ulBudgetBytes`); immutable after `Create`, so it is not atomic |

### Grant steps

`ReserveSubsystemBytes` is `ANY_THREAD`, lock-free, allocates nothing and never blocks:

1. `ZENITH_ASSERT(eSubsystem < MEMORY_SUBSYSTEM_COUNT)` (a programmer error, never data). Let `b = m_aulSubsystemBoundBytes[eSubsystem]` and `m = m_aulMeasuredSubsystemBytes[eSubsystem].load(relaxed)`.
2. Load `r = m_aulReservedSubsystemBytes[eSubsystem]` (relaxed).
3. Compute the available bytes without overflow: `a = 0` when `m ≥ b` or `r ≥ b − m`, otherwise `a = b − m − r`.
4. If `ulBytes > a`, increment `m_ulSubsystemRefusals` and return `Zenith_Err(Zenith_MemoryError{ MEMORY_ERROR_SUBSYSTEM_REFUSED, TRANSITION_PEAK_COUNT, eSubsystem, ulBytes, a })` (`m_ePeak` is `TRANSITION_PEAK_COUNT`, not applicable, in a subsystem refusal). Nothing else changes.
5. `compare_exchange_weak(r, r + ulBytes, acq_rel)`; on failure go to step 3 with the new `r` (and the same `m`).
6. Return `Zenith_SubsystemReservation(this, eSubsystem, ulBytes)` (the private constructor of "Additions to the governor API") as the success value.

- A request of 0 bytes is granted as a reservation of 0 bytes (steps 3 to 6 with `ulBytes = 0`), so a caller need not special-case it.
- The grant rule is exactly [Z2-N-ARCH-825], `m + r + ulBytes ≤ b`, written in the subtraction form so no sum can wrap. `m` is the snapshot of the last `Update`, never a fresh consumer query, because the governor calls consumers' `GetResidentBytes` only from `Update` on the main thread; a grant between two `Update`s therefore sees the previous frame's resident bytes plus every reservation granted since.
- Thread safety: concurrent reservations of one subsystem never sum above `b − m` (each grant is one compare-exchange against the others); an `Update` that stores a larger `m` between two grants makes the second see less room, the conservative direction.

### Release and ownership

- `Zenith_SubsystemReservation` holds `m_pxGovernor`, `m_eSubsystem` and `m_ulBytes` (Architecture's members). `Shrink(n)` asserts `n ≤ GetBytes()` (`ZENITH_ASSERT`), subtracts `GetBytes() − n` from `m_aulReservedSubsystemBytes[m_eSubsystem]` with `fetch_sub(release)` and stores `n`; the destructor subtracts the remaining bytes when `m_pxGovernor` is non-null.
- The move constructor takes the source's three members and leaves the source with `m_pxGovernor = nullptr`, `m_eSubsystem = MEMORY_SUBSYSTEM_COUNT` and 0 bytes; the move assignment first releases the target's own bytes exactly as the destructor does, then moves, and a self-assignment changes nothing. A moved-from reservation releases nothing.
- The owner releases the reservation once the reserved memory has been freed, or shrinks it once the operation knows its final size: SCENE keeps it beside the preparation's arena and destroys it with the arena ([Z2-N-ARCH-826]). Nothing in the governor releases a reservation on its owner's behalf.
- At governor destruction every non-zero entry of `m_aulReservedSubsystemBytes` is logged and checked with the transition reservations ("Reservation algorithm"); `Test_MemoryGovernor_LiveSubsystemReservationAtShutdown_Checks` proves it.

### Interaction with transition reservations and budgets

| Quantity | Subsystem reservations | Transition reservations |
|---|---|---|
| bound checked at grant | the subsystem's bound `b` (its budget of Architecture's table "Budgets", or the process total when pending) | the transition-peak row of its kind |
| bytes counted against that bound | the subsystem's measured bytes plus its outstanding subsystem reservations | the kind's outstanding transition reservations alone |
| pressure sources the outstanding bytes add to ("Level evaluation") | the subsystem's own source (non-pending subsystems only, [Z2-N-ARCH-824]) and the process source | the process source |
| device-local source (Windows) | not added: a subsystem reservation is host-side | not added |

- The two kinds never see each other at grant time: a full world-transition reservation leaves every subsystem's available bytes unchanged, and a subsystem at its bound still grants a transition reservation within its peak row. This is the separation [Z2-N-ARCH-825] states: a world transition keeps its transition-peak reservation ([Z2-N-ARCH-815], [Z2-N-ARCH-559]) and takes no subsystem reservation for the same bytes, so no byte is counted twice at grant.
- Counting the outstanding bytes in the subsystem source and the process source while the reserved memory is also reported by a consumer is the double count [Z2-N-ARCH-826] accepts as conservative; it lasts until the owner shrinks or destroys the reservation.
- A pending subsystem's reservations are bounded by the process total and raise no subsystem source, but they still count toward the process source, so a large pending-subsystem reservation raises pressure through the process thresholds.
- Refusal behaviour belongs to the caller ([Z2-N-ARCH-825]): SCENE fails the preparation with `SCENE_ERROR_MEMORY_REFUSED` before `DECODING` and allocates nothing. The governor never retries, never sheds because of a refusal and never raises a level because of one.
- A test forces a refusal deterministically without a fault point: it gives `Create` a subsystem budget below the request (bit clear in `m_uPendingSubsystemMask`), or registers a fake consumer of that subsystem whose resident bytes fill the budget and runs one `Update`.

## Accounting

- A subsystem's usage is the sum of its consumers' `GetResidentBytes()`, host and device-local together; each consumer counts the memory it holds exactly once (for example Flux counts its GPU heaps and CPU render data; ZenithScript counts every VM heap, snapshot and checkpoint).
- The unattributed residual is `max(0, heap.GetStats().m_ulLiveBytes − Σ resident bytes of every consumer's host-side memory)`, where each consumer's host-side share is its resident bytes less its device-local bytes (a consumer that holds device memory reports it through the optional query below); it is compared against Z2-B-CORE-004 ([Z2-N-BUDGET-148]) and published as `memory.unattributed_bytes`. It raises no pressure level: an over-budget residual is logged once per rise with `ZENITH_LOG_WARNING(xLOG_CATEGORY_CORE_GOVERNOR, …)` and is caught by the budget measurement of that row, while the process source already counts the same bytes through the measured process usage.
- Frame and graph arenas are attributed to `MEMORY_SUBSYSTEM_OTHER` by their owners (the job system and Flux) through a consumer of that subsystem reporting `GetReservedBytes()` of their arenas, measured against Z2-B-CORE-005.

The consumer's host-side share uses `Zenith_MemoryConsumer::GetDeviceLocalBytes()` (non-pure, default 0, `ANY_THREAD`), declared in `Zenith2/Docs/Architecture.md` → "Memory governor".

## OS memory queries

| Query | Windows (`Internal/Windows/Zenith_ProcessMemory_Windows.cpp`) | Android (`Internal/Android/…`) | Linux (`Internal/Linux/…`) |
|---|---|---|---|
| `Zenith_QueryProcessResidentBytes` | `GetProcessMemoryInfo` → `PROCESS_MEMORY_COUNTERS_EX::WorkingSetSize` | `/proc/self/statm` second field × page size (`sysconf(_SC_PAGESIZE)`), read with one `read` into a 128-byte buffer | as Android |
| low-memory signal | `CreateMemoryResourceNotification(LowMemoryResourceNotification)` at `Create`, polled with `QueryMemoryResourceNotification` in `Update` | none in CORE: PLAT calls `ReportOSPressure(MEMORY_PRESSURE_CRITICAL)` on `SDL_EVENT_LOW_MEMORY` | none |

The S24 budget statistic of `Zenith2/Docs/Budgets.md` → "Memory budgets" (`VmRSS` plus the Graphics term) is the measurement procedure's, not the governor's; the governor's process source uses resident pages alone, and the rendering consumer's device-local bytes cover GPU memory on the S24 through the RHI's accounting.

## Counters

Published each `Update` through `Zenith_DispatchCounter`, which feeds the on-screen profiler's memory panel ([Z2-N-ARCH-818]):

| Counter | Value |
|---|---|
| `memory.level` | 0 NORMAL, 1 WARNING, 2 CRITICAL |
| `memory.process_bytes`, `memory.device_local_bytes` | the last reported usage |
| `memory.subsystem.<rendering, streaming, audio, scripts, editor_caches, assets, simulation, other>_bytes` | resident bytes |
| `memory.subsystem.<name>_budget_bytes` | the budget |
| `memory.unattributed_bytes` | the residual |
| `memory.peak.<streaming_double_residency, world_transition, texture_swap>_bytes` | reserved bytes |
| `memory.subsystem.<name>_reserved_bytes` | the subsystem's outstanding subsystem reservations (`GetReservedSubsystemBytes`), for each of the eight names |
| `memory.level_changes`, `memory.peak_refusals`, `memory.subsystem_refusals`, `memory.oom_events` | cumulative counts |
| `memory.pending_rows` | the number of pending subsystem budgets |
| `memory.simulation_over_budget` | 1 while the simulation subsystem is at CRITICAL, with a warning logged once per rise (the Simulation row of the shedding table) |

Each consumer's design document lists the counters that prove its shedding ran ([Z2-N-ARCH-814]); the forced-level tests read them.

## Governor tests

Unit tests in `Zenith2/Engine/ZenithCore/Tests/Zenith_MemoryGovernor_Tests.cpp` use fake consumers (`Tests/Support/Zenith_FakeMemoryConsumer.h`, a consumer whose resident bytes the test sets and which records every notification). Cross-system tests in `Zenith2/Tests/Memory/` (integration suite owned by CORE, linking the runtime and modules) use the real consumers and their shedding counters.

| Test ID | Location | Asserts |
|---|---|---|
| `Test_MemoryGovernor_BadBudgetsRejected` | unit | [Z2-N-ARCH-819]: a zero budget, a zero threshold, warning ≥ critical, and non-pending subsystem budgets exceeding the process total each fail with `MEMORY_ERROR_BAD_BUDGETS`; pending subsystems are excluded from the sum |
| `Test_MemoryGovernor_ProcessThresholds_RaiseLevel` | unit | usage at the warning and critical percentages raises each level in the same `Update` |
| `Test_MemoryGovernor_SubsystemOverBudget_RaisesWarning` | unit | one subsystem at its budget gives WARNING; at the critical factor, CRITICAL; a pending subsystem never raises |
| `Test_MemoryGovernor_HysteresisReturnsToNormal` | unit | falling needs both the margin and the dwell; CRITICAL falls through WARNING with two notifications; with two subsystem sources at WARNING whose usage drops below the margin at different times, the combined level falls to NORMAL in the `Update` in which the second one's dwell completes, not one dwell after it |
| `Test_MemoryGovernor_OSSignalMapsToLevel` | unit | `ReportOSPressure(CRITICAL)` holds CRITICAL for the Android hold and then falls |
| `Test_MemoryGovernor_OOMRelay_ForcesCritical` | unit | a raised `Zenith_OOMPressureFlag` gives CRITICAL at the next `Update` |
| `Test_MemoryGovernor_ForcedLevel_AndClear` | unit | `ForceLevel(WARNING)` over NORMAL sources raises in the next `Update`; after `ClearForcedLevel` the next `Update` falls to NORMAL with one notification; `ForceLevel(CRITICAL)` then `ClearForcedLevel` gives one `(CRITICAL, NORMAL)` notification; with a subsystem source at WARNING, clearing a forced CRITICAL falls to WARNING |
| `Test_MemoryGovernor_NotifiesInRegistrationOrder` | unit | three consumers receive every change in registration order |
| `Test_MemoryGovernor_LateConsumer_ReceivesCurrentLevel` | unit | a consumer registered at CRITICAL receives `(NORMAL, CRITICAL)` |
| `Test_MemoryGovernor_RegisterInsideCallback_Asserts` | unit, `z2-death-assert` | Reentrancy `NONE` |
| `Test_MemoryGovernor_TransitionPeakRefusedAtCap` | unit | reservations up to the peak succeed; the next is refused with the available bytes; `Shrink` and destruction release |
| `Test_MemoryGovernor_ConcurrentReservations_NeverExceedPeak` | unit, `z2-stress`, `linux-tsan` | 8 threads reserving and releasing random sizes never observe the sum above the peak |
| `Test_MemoryGovernor_LiveReservationAtShutdown_Checks` | unit, `z2-death-check` | [Z2-N-ARCH-816] |
| `Test_MemoryGovernor_SubsystemReservationRefusedAtBudget` | unit | [Z2-N-ARCH-826]: reservations up to a non-pending subsystem's budget succeed; the next is refused with `MEMORY_ERROR_SUBSYSTEM_REFUSED`, `m_eSubsystem`, the requested bytes and the available bytes, increments `memory.subsystem_refusals` and changes no reserved byte; `Shrink` and destruction each release exactly, after which the same request succeeds |
| `Test_MemoryGovernor_SubsystemReservation_CountsMeasuredBytes` | unit | a fake consumer of the subsystem reporting resident bytes `R`, after one `Update`, leaves exactly `budget − R` available; a consumer above the budget leaves 0 available, without wrapping; a 0-byte request is granted |
| `Test_MemoryGovernor_SubsystemReservation_PendingUsesProcessTotal` | unit | a subsystem in `m_uPendingSubsystemMask` grants up to the process total, never up to its own budget, and `GetStats().m_axSubsystems[e].m_ulBudgetBytes` reports that bound |
| `Test_MemoryGovernor_SubsystemReservation_RaisesPressureSources` | unit | an outstanding reservation that brings a non-pending subsystem to its budget gives WARNING at the next `Update` through the subsystem source; a pending subsystem's reservation crossing the process warning threshold gives WARNING through the process source; the device-local source is unchanged; destroying the reservation lets the level fall by the hysteresis rule |
| `Test_MemoryGovernor_SubsystemReservation_IndependentOfTransitionPeaks` | unit | a world-transition reservation at its full peak leaves every subsystem's available bytes unchanged, and a subsystem reserved to its bound still grants a transition reservation within its peak |
| `Test_MemoryGovernor_SubsystemReservation_MoveTransfersBytes` | unit | a move constructor and a move assignment transfer the bytes; the assigned-over reservation's bytes are released first; a moved-from reservation releases nothing; `GetReservedSubsystemBytes` matches at each step |
| `Test_MemoryGovernor_ConcurrentSubsystemReservations_NeverExceedBudget` | unit, `z2-stress`, `linux-tsan` | 8 threads reserving, shrinking and releasing random sizes on one subsystem never observe `GetReservedSubsystemBytes` above the budget, and end at 0 |
| `Test_MemoryGovernor_SubsystemReservation_ShrinkAboveBytes_Asserts` | unit, `z2-death-assert` | `Shrink(n)` with `n > GetBytes()` |
| `Test_MemoryGovernor_LiveSubsystemReservationAtShutdown_Checks` | unit, `z2-death-check` | [Z2-N-ARCH-826]: a subsystem reservation live at governor destruction is logged with its subsystem and bytes in `core.governor`, then fails the `ZENITH_CHECK` |
| `Test_MemoryGovernor_UnattributedResidual_Computed` | unit | residual = heap live bytes − host-side consumer bytes, clamped at 0; a residual above the Z2-B-CORE-004 budget logs one warning and leaves the level NORMAL |
| `Test_MemoryGovernor_Create_FailsAtObject_Unwinds` | unit | `Create` with the heap countdown failing the object returns `MEMORY_ERROR_OUT_OF_MEMORY`, with zero leaks and no consumer, handle or counter left behind |
| `Test_MemoryGovernor_Create_LowMemoryHandleUnavailable_Warns` | unit, Windows | with `uFAULT_POINT_GOVERNOR_LOW_MEMORY_HANDLE` failing, `Create` succeeds, logs one warning in `core.governor`, and the low-memory source never raises (the other sources still do) |
| `Test_MemoryGovernor_CountersPublished` | unit | every counter of "Counters" is emitted each `Update` to a capturing hook |
| `Test_BudgetTable_Generator_Fixtures` | unit (CTest over `cmake -P`) | fixtures under `Tests/Fixtures/BudgetTable/`: a valid table, pending rows, per-platform values in the colon form `Win: …; S24: …`, a dual-resident row in the form `Win RAM: …; Win VRAM: …; S24: …`, an Allocation-sums cell with `dual-resident:`, `max(…)` and `×2` tokens, asserting explicitly that the three IDs inside `max(Z2-B-CORE-006, Z2-B-CORE-007, Z2-B-CORE-008)` come out as transition-peak members and no token `max(Z2-B-CORE-006` or `Z2-B-CORE-008)` is produced, an Architecture "Budgets" fixture whose rendering cell carries a `; … not summed` clause (its IDs are excluded), and one malformed file per failure kind of either input (each `WILL_FAIL` with its message) |
| `Test_BudgetTable_DefaultsMatchRowIndex` | unit | the defaults registered for each `memory.*` key equal the sums, over the generated rows, of the rows Architecture's table "Budgets" lists for it, recomputed in the test from `axGENERATED_BUDGET_KEYS` and `axGENERATED_BUDGET_ROWS` |
| `Test_MemoryGovernor_ForcedWarningShedsEachConsumer` | `Tests/Memory/` | [Z2-N-ARCH-817]: each subsystem's WARNING counters move |
| `Test_MemoryGovernor_ForcedCriticalShedsEachConsumer` | `Tests/Memory/` | [Z2-N-ARCH-817]: each subsystem's CRITICAL counters move, and revert one frame after `ClearForcedLevel` |
| `Test_MemoryGovernor_ShedsNoSimulationState` | `Tests/Memory/` | [Z2-N-ARCH-817]: per-tick world-state hashes identical over 600 ticks with and without forced CRITICAL |
| `Test_MemoryGovernor_TransitionPeaks_EachKind` | `Tests/Memory/` | a streaming double residency, a world transition and a texture swap each reserve before allocating, and a refused world transition fails its request while a refused streaming load defers |
| `Test_MemoryGovernor_SubsystemReservation_BootedRuntime` | `Tests/Memory/` | in a booted runtime, a `MEMORY_SUBSYSTEM_SIMULATION` reservation of exactly the bytes `GetStats` reports available succeeds and one more byte is refused with `MEMORY_ERROR_SUBSYSTEM_REFUSED`; the next frame publishes `memory.subsystem.simulation_reserved_bytes` equal to the reservation and `memory.subsystem_refusals` at least 1; after release the counter returns to 0 and the runtime shuts down with no live reservation logged |

The forced-level cross-system tests grow as each subsystem lands its shedding (`Z2-I-CORE-30`); each subsystem's first shedding ticket adds its rows to the two forced-level tests in the same diff. The S24 memory-pressure run is a `needs-human` acceptance device step ([Z2-N-ARCH-817]).
