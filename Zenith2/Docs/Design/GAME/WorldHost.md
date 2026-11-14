# GAME design — world host

This file is part of the GAME design family; the entry document is `Zenith2/Docs/Design/GAME.md`. It specifies GAME's content in `ZenithRuntime` (inventory items Z2-I-GAME-01 game lifecycle, Z2-I-GAME-02 world lifecycle, Z2-I-GAME-31 world transition and Z2-I-GAME-32 shutdown): the world-transition record that `Zenith_Runtime::RequestWorldTransition` and SCENE's transition sink drive, the swap, carry-over, the retirement of a swapped-out world's asynchronous results, pause arbitration, the world state resource, the game-flow command kinds and their `Game` Luau namespace, and the lifecycle shutdown of each world with its teardown order. MOD owns `ZenithRuntime`, the startup sequence, the frame-loop driver, the module host and the process shutdown sequence (`Zenith2/Docs/Design/MOD.md` → "ZenithRuntime files", "Shutdown sequence", "Frame loop driver"); this file adds the GAME half of `Internal/Zenith_WorldHost.cpp` and the files listed in § "World host files and split with MOD", and changes no MOD-owned behaviour except where a cross-area row names it.

## Scope of this file

| Topic | Heading |
|---|---|
| which function lives in which file, and whose it is | "World host files and split with MOD" |
| the per-process and per-world records | "World host state" |
| the states of one world and of the run | "World lifecycle state machine", "Game flow state machine" |
| creating an isolated world | "World creation" |
| the transition | "World transition" |
| entities that survive a transition | "Carry-over" |
| results that complete after a swap | "Retired world asynchronous results" |
| pausing a world | "Pause arbitration" |
| what the simulation can read about its world | "World state resource" |
| the three runtime command kinds and the `Game` namespace | "Game command kinds", "Game script bindings" |
| shutdown | "Shutdown and teardown" |
| diagnostics | "World host counters and logs" |

## World host files and split with MOD

`Zenith2/Docs/Design/MOD.md` → "ZenithRuntime files" lists `Internal/Zenith_WorldHost.cpp` as GAME's content. MOD's step-16 ticket lands its MOD half (initial world creation, world destruction, the input-world mark, and the call of `Zenith_WorldHost_BeginTransition`); GAME's tickets land the rest, the Stage 2 declaration and definition of `Zenith_WorldHost_BeginTransition` first (below). Every world-host function is declared in `Zenith2/Engine/ZenithRuntime/Internal/Zenith_WorldHost.h`:

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Zenith_WorldHost.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"          // Zenith_ModuleError
#include "ZenithRuntime/Zenith_Runtime.h"         // Zenith_RuntimeError, Zenith_WorldTransitionDesc, Zenith_OperationHandle
#include "ZenithRuntime/Zenith_WorldState.h"      // Zenith_WorldOrigin

struct Zenith_RuntimeState;
class Zenith_PreparedSave;

/// @contract value Zenith_WorldCreateDesc
/// Ownership: Plain value passed by reference to Zenith_WorldHost_CreateWorld.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2 (the MOD half); m_bIsolated used from Stage 3.
/// @endcontract
struct Zenith_WorldCreateDesc
{
	uint64 m_ulWorldSeed = 0;
	Zenith_WorldOrigin m_eOrigin = WORLD_ORIGIN_STARTUP;
	bool m_bIsolated = false;            // true: no EmitInto until the swap ([Z2-N-ARCH-424])
	bool m_bInputWorld = false;          // true for the startup world alone; a swap moves the mark
};

// ---- MOD half (Zenith_WorldHost.cpp, MOD's step-16 ticket); every function MAIN_THREAD, no frame running.
// The startup world; a failure is the failing step's error, which MOD maps to the step-16 exit code.
Zenith_Result<void, Zenith_ModuleError> Zenith_WorldHost_CreateInitialWorld(Zenith_RuntimeState& xState);
// One world through the step-16 sequence (§ "World creation"); returns its tag. Errors: MODULE_ERROR_OUT_OF_MEMORY (no tag,
// world or engine state), MODULE_ERROR_REGISTRATION (a rejected registration, GAME's record included), or the
// attaching module's own Zenith_ModuleError; every step before the failing one is unwound.
Zenith_Result<uint16, Zenith_ModuleError> Zenith_WorldHost_CreateWorld(Zenith_RuntimeState& xState,
	const Zenith_WorldCreateDesc& xDesc);
// Walks axWORLD_TEARDOWN_ORDER (§ "Teardown order algorithm"); cannot fail.
void Zenith_WorldHost_DestroyWorld(Zenith_RuntimeState& xState, uint16 uTag);
uint16 Zenith_WorldHost_GetInputWorldTag(const Zenith_RuntimeState& xState);   // 0 when none
void Zenith_WorldHost_SetInputWorld(Zenith_RuntimeState& xState, uint16 uTag);               // uTag is live

// ---- GAME half.
// Zenith_Runtime::RequestWorldTransition for a known tag. MAIN_THREAD, no frame running. Errors (m_eCode):
// RUNTIME_ERROR_TRANSITION_IN_PROGRESS, RUNTIME_ERROR_TRANSITION_PEAK_REFUSED (§ "Transition request intake").
Zenith_Result<Zenith_OperationHandle, Zenith_RuntimeError> Zenith_WorldHost_BeginTransition(
	Zenith_RuntimeState& xState, uint16 uFromWorldTag, const Zenith_WorldTransitionDesc& xDesc);
// SAVE's runtime bridge, in the prologue, for a prepared Save.Load whose completion ID the bridge allocated. MAIN_THREAD.
// Errors: as BeginTransition; on any error the host has already completed ulCompletionID with the failure payload of its code
// (IN_PROGRESS: WORLD_TRANSITION_FAILURE_IN_PROGRESS; PEAK_REFUSED; a full operation table: OPERATION_TABLE) and destroyed
// xSave, so the bridge does nothing further. In a --replay run the codes come from the log (§ "Intake in a replay").
// xSave is non-null in a normal run (null there is a programmer error, ZENITH_ASSERT). In a --replay run it is null when the log
// holds no restored session after the requesting world's swap — the recorded load failed at or after the hand-off, or the
// recording ended first — and the intake then follows the recorded outcome alone, restoring nothing (§ "Failed save loads").
Zenith_Result<void, Zenith_RuntimeError> Zenith_WorldHost_BeginTransitionFromSave(Zenith_RuntimeState& xState,
	uint16 uFromWorldTag, Zenith_Owned<Zenith_PreparedSave> xSave, uint64 ulCompletionID);
// Prologue step 7 (§ "Prologue step 7 procedure"); failures are transition outcomes, never results. MAIN_THREAD.
void Zenith_WorldHost_ApplyPauseMasks(Zenith_RuntimeState& xState);
void Zenith_WorldHost_StepTransitions(Zenith_RuntimeState& xState);
// Shutdown sequence step 1 (§ "World lifecycle shutdown"); MAIN_THREAD; cannot fail.
void Zenith_WorldHost_ShutdownWorlds(Zenith_RuntimeState& xState);
// Prologue step 7 (bShutdown false) and after shutdown step 2 (true): § "Retired world asynchronous results". MAIN_THREAD.
void Zenith_WorldHost_ReleaseRetiredResults(Zenith_RuntimeState& xState, bool bShutdown);
// Command playback of Runtime.Quit and Runtime.ReportTestFailure, on any world's sync-point node (ANY_THREAD: atomics only).
void Zenith_WorldHost_RequestShutdownFromWorld(Zenith_RuntimeState& xState, uint16 uTag);
void Zenith_WorldHost_RecordTestFailure(Zenith_RuntimeState& xState, uint16 uTag);
// Pure: Zenith_Hash64 of the 12 little-endian bytes (ulOldSeed, uSessionOrdinal) (§ "World creation"). ANY_THREAD.
uint64 Zenith_DeriveWorldSeed(uint64 ulOldSeed, uint32 uSessionOrdinal);
```

Internal functions need no contract block (blocks are required of public types, `Zenith2/Docs/Contracts.md` → "Contract blocks per type", and `Internal/` is private to ZenithRuntime and its tests). MOD's call site of `Zenith_WorldHost_BeginTransition` (`Zenith_Runtime::RequestWorldTransition`) is unaffected by which file defines it: the declaration is the header's, the definition GAME's `Internal/Zenith_WorldTransition.cpp`.

**Declare first, define at Stage 2.** MOD's Stage 2 composition-root ticket Z2T-MOD-024 (startup steps 15–18) calls `Zenith_WorldHost_BeginTransition` from `Zenith_Runtime::RequestWorldTransition` for a known tag, so a definition must exist from Stage 2, or every Stage 2 executable that links `RequestWorldTransition` fails with an unresolved external. The Stage 2 ticket "GAME world host: lifecycle shutdown and teardown order" is therefore the Stage 2 producer of MOD's blocker row MB16 and lands before Z2T-MOD-024, which is blocked by it. Besides the teardown table that MOD's `Zenith_WorldHost_DestroyWorld` walks (GA5 (f)), it creates `Internal/Zenith_WorldTransition.cpp` with the Stage 2 definition of `Zenith_WorldHost_BeginTransition`: it allocates nothing, touches no record, logs `runtime.world: world transitions are not available before Stage 3` and returns `Zenith_RuntimeError{ RUNTIME_ERROR_TRANSITION_IN_PROGRESS }`. `Internal/Zenith_WorldHost.h` is created by whichever of the two tickets lands first, with its own half of the declarations above, and the other adds its half; the GAME ticket landing first creates it with the declarations of `Zenith_WorldHost_BeginTransition` and `Zenith_WorldHost_ShutdownWorlds`. The Stage 3 ticket "GAME world host: transition intake and record" replaces the body with § "Transition request intake" and adds the file's other functions; the declaration never changes. Cross-area row GA23 asks the orchestrator to link MB16 to the Stage 2 ticket and to word Z2T-MOD-024's header line to match.

The split, function by function:

| Function | Half | Called by |
|---|---|---|
| `Zenith_WorldHost_CreateInitialWorld` | MOD | startup step 16 |
| `Zenith_WorldHost_CreateWorld` | MOD (factored out of step 16, cross-area row GA5 (e)) | `CreateInitialWorld`; GAME's isolated-world creation (§ "World creation") |
| `Zenith_WorldHost_DestroyWorld` | MOD, walking GAME's teardown table (§ "Teardown order algorithm"; GA5 (f)) | startup unwind; GAME's swap and shutdown |
| `Zenith_WorldHost_GetInputWorldTag`, `Zenith_WorldHost_SetInputWorld` | MOD | PLAT's refresh; GAME's swap |
| `Zenith_WorldHost_BeginTransition` | GAME (`Internal/Zenith_WorldTransition.cpp`: the Stage 2 refusing definition, then the Stage 3 body) | `Zenith_Runtime::RequestWorldTransition` for a known tag (MOD's refusal of an unknown tag is unchanged) |
| `Zenith_WorldHost_BeginTransitionFromSave` | GAME (`Internal/Zenith_WorldTransition.cpp`) | SAVE's runtime bridge, for a `Save.Load` (§ "Transition sources") |
| `Zenith_WorldHost_StepTransitions` | GAME (`Internal/Zenith_WorldTransition.cpp`) | MOD's prologue step 7 driver (§ "Prologue step 7 procedure"); the GAME ticket that lands it moves the driver's own `Zenith_SceneLoader::StepActivations` call into it (its step 4), so the driver calls `ApplyPauseMasks` then `StepTransitions` and no longer calls `StepActivations` directly (`Zenith2/Docs/Design/MOD.md` → "Frame loop driver"; GA5 (g)) |
| `Zenith_WorldHost_ApplyPauseMasks` | GAME (`Internal/Zenith_WorldPause.cpp`) | the same driver, first (§ "Pause arbitration") |
| `Zenith_WorldHost_ShutdownWorlds` | GAME (`Internal/Zenith_WorldTeardown.cpp`) | MOD's shutdown sequence step 1, "shut down each world by its lifecycle", which the GAME ticket makes call it (§ "World lifecycle shutdown"; GA5 (j)) |
| `Zenith_WorldHost_ReleaseRetiredResults` | GAME (`Internal/Zenith_WorldTransition.cpp`) | prologue step 7 (`false`), and MOD's shutdown sequence right after step 2 destroys the worlds (`true`; GA5 (j)) (§ "Retired world asynchronous results") |
| `Zenith_WorldHost_RequestShutdownFromWorld`, `Zenith_WorldHost_RecordTestFailure` | GAME (`Internal/Zenith_GameFlowCommands.cpp`) | the playback of `Runtime.Quit` and `Runtime.ReportTestFailure` (§ "Game command kinds") |
| `Zenith_DeriveWorldSeed` | GAME (`Internal/Zenith_WorldTransition.cpp`) | § "World creation"; `Test_WorldTransition_ReplaySwapsAtLoggedTick` |

<!-- assumption: pending MOD -->
GAME assumes two MOD-half entry points beyond MOD's current text (cross-area row GA5 (d) and (i)): `void Zenith_Runtime::RecordTestAssertionFailure()` (`ANY_THREAD`: one atomic increment of the runtime's assertion count, read by the frame loop to return exit code 5 — the call MOD's row MB15 names for SAVE's replay verifier), reached from the world host's internal code through `Zenith_RuntimeState::m_pxRuntime`; and `Zenith_Result<void, Zenith_FrameGraphError> Zenith_FrameLoop_PrecompileSwap(Zenith_RuntimeState& xState, uint16 uOutgoingTag, uint16 uIncomingTag)` (`MAIN_THREAD`, no frame running): it builds the frame graph exactly as prologue step 8 would after the swap — every live world's `EmitInto` except the outgoing world's, plus the incoming world's — and calls `Zenith_FrameScheduleCache::Acquire` on it without handing the result to the executor (`Zenith2/Docs/Design/GRAPH/PublicAPI.md`), so a compile failure changes nothing cached or running and a success makes step 8's rebuild a cache hit. The trial build installs nothing, so the isolated world still contributes no node to the running schedule ([Z2-N-ARCH-424]).
<!-- /assumption -->

GAME's other files in `Zenith2/Engine/ZenithRuntime/`:

| Path | Contents |
|---|---|
| `Zenith_WorldControl.h` (public) | `Zenith_WorldPauseReason`, `Zenith_WorldControl` (§ "Pause arbitration") |
| `Zenith_WorldState.h` (public) | `Zenith_WorldOrigin`, `Zenith_WorldStateResource`, `Zenith_CarryOverComponent` (§ "World state resource", § "Carry-over") |
| `Internal/Zenith_WorldTransition.h`, `.cpp` | the transition record and its state machine, the swap, carry-over, retired results |
| `Internal/Zenith_WorldTeardown.h`, `.cpp` | `Zenith_WorldTeardownStep`, `axWORLD_TEARDOWN_ORDER`, the lifecycle shutdown |
| `Internal/Zenith_WorldPause.cpp` | the pause-mask application |
| `Internal/Zenith_GameFlowTypes.cpp` | `Zenith_RegisterGameFlowTypes`: the component `Zenith.Runtime.CarryOver` and the resource `Zenith.Runtime.WorldState`, called in startup step 6's engine-types part beside `Zenith_RegisterRenderComponentTypes` (GA5 (g)) |
| `Internal/Zenith_GameFlowCommands.cpp` | `Zenith_RegisterGameFlowCommandKinds`: `Runtime.Pause`, `Runtime.Quit`, `Runtime.ReportTestFailure`, called in startup step 15 beside `Zenith_RegisterSceneCommandKinds` (GA5 (g)) |
| `Internal/ScriptBindings/Zenith_GameScriptBindings.cpp` | `Zenith_RegisterGameScriptBindings`: the `Game` namespace, into the engine binding table before it is sealed (GA5 (g)) |
| `Internal/Zenith_WorldHostLog.h` | the log categories `runtime.world` and `runtime.game` |
| `Internal/Zenith_WorldHostTestAccess.cpp` | the definitions of GAME's `Zenith_RuntimeTestAccess` members (§ "Runtime test access members"); compiled in every configuration, used only by test targets |
| `Tests/Zenith_WorldTransition_Tests.cpp`, `Tests/Zenith_WorldHost_Tests.cpp`, `Tests/Zenith_LifecycleCommand_Tests.cpp`, `Tests/Zenith_GameFlow_Tests.cpp` | `Zenith2/Docs/Design/GAME/Tests.md` → "World host tests", "Lifecycle per-command tests" |
| `Tests/Support/Zenith_AuthorityCensusCheck.h` | header-only test support (the same rule): `Zenith_CensusCheckDesc`, `Zenith_CensusCheckReport` and `Zenith_CheckAuthorityCensus`, the comparison of `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format"; included by the three Lifecycle suites' census tests |
| `Tests/Support/Zenith_LifecycleProbe.h` | header-only test support (non-Core `Tests/Support/` holds headers only and is compiled by no target, [Z2-N-ARCH-255]; the X184 ruling): the probe module `Zenith_LifecycleProbeModule` — a test-local module listed in a test's hand-built composition (`Zenith2/Docs/Design/MOD.md` → "Generated test compositions") that registers the test component `Zenith.Test.LifecycleProbe` (one `uint32` property `Value`, authority `CommandsOnly`), one recording system per phase 1–10 and the test command kind `Test.Lifecycle.RequestQuit`, whose playback calls `Zenith_WorldHost_RequestShutdownFromWorld`; included by `ZenithRuntime_Tests` and by the Lifecycle integration suites |

None of these files includes a module header: `ZenithRuntime` sits below every module ([Z2-N-ARCH-072]), so the gameplay framework of `ZenithGameplay` never appears here, and nothing here knows a quest, an item or a dialogue: game rules live in Luau and in data (Z2-D-077).

### Runtime test access members

GAME's tests reach GAME's own records through static members of MOD's public test-access type, which GAME's tickets add themselves (orchestrator ruling GAME-OWN-TEST-ACCESS, following SAVE's members of the same header; MOD is not asked for them). Each is declared in `Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h` and defined in GAME's `Internal/Zenith_WorldHostTestAccess.cpp`, reaches the runtime's private `Zenith_RuntimeState m_xState` through the `friend class Zenith_RuntimeTestAccess;` of `Zenith_Runtime`, and is `MAIN_THREAD`, callable with no frame running or from inside `m_pfnOnTeardownStep`:

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h — GAME's members of Zenith_RuntimeTestAccess
// (Zenith_RuntimeState, Zenith_ModuleInstance, Zenith_AsyncResultTable, Zenith_WorldStateResource, Flux_Renderer forward-declared)
static Zenith_RuntimeState& GetRuntimeStateForTest(Zenith_Runtime& xRuntime);         // complete only where Internal/Zenith_RuntimeState.h is included
static uint8 GetWorldLifecycleForTest(const Zenith_Runtime& xRuntime, uint16 uTag); // the record's Zenith_WorldLifecycleState; 0 for a tag with no record
static void SetWorldLifecycleForTest(Zenith_Runtime& xRuntime, uint16 uTag, uint8 uLifecycleState); // ZENITH_ASSERT: a record exists and the value is 1 … 7
static Zenith_ModuleInstance* FindModuleInstance(Zenith_Runtime& xRuntime, const char* szModuleName); // null: no such module, or not started
static Zenith_WorldStateResource* GetWorldStateForTest(Zenith_Runtime& xRuntime, uint16 uTag); // the record's m_xState; null for a tag with no record
static Zenith_Result<uint16, Zenith_ModuleError> CreateLiveWorld(Zenith_Runtime& xRuntime, const Zenith_WorldCreateDesc& xDesc);
static const Zenith_AsyncResultTable* GetAsyncResultsForTest(const Zenith_Runtime& xRuntime, uint16 uTag); // live, or detached into m_xDetached; null once disposed
static Flux_Renderer* GetRendererForTest(Zenith_Runtime& xRuntime);   // the runtime state's m_pxRenderer; null when headless
```

| Member | Behaviour | Added by (sprint) |
|---|---|---|
| `GetRuntimeStateForTest`, `GetWorldLifecycleForTest`, `SetWorldLifecycleForTest` | the runtime's state; the record's `m_eLifecycle` read and written. A Stage 2 test reads the flow state as `GetRuntimeStateForTest(xRuntime).m_xGameHost.m_eFlow`; MOD's `GetGameFlowState` (§ "Game flow state machine") arrives in the Stage 3 sprint | "GAME world host: the lifecycle shutdown of each world and the quit request seam" (Stage 2) |
| `FindModuleInstance` | the index i of `xRuntime.GetComposition().m_axModules` whose `m_szName` equals `szModuleName` (byte comparison), then the started instance of entry i of the runtime state's `Zenith_Owned<Zenith_ModuleHost>` slot (the one MOD's startup step 15 fills; i is the canonical composition index the host uses) through `Zenith_ModuleInstance* Zenith_ModuleHost::GetStartedInstance(uint16 uIndex) const` (null unless the entry is `MODULE_STATE_STARTED`): one const, test-only read accessor on MOD's internal type that the same GAME diff adds to `Internal/Zenith_ModuleHost.h`/`.cpp`, as orchestrator ruling Q95 allows (it extends GAME-OWN-TEST-ACCESS; MOD lists it in `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Call sites GAME's tickets add"); MOD's tool access (`Zenith_RuntimeToolAccess::FindModuleInstance`) reuses it, and no other MOD code calls it | "ZenithRuntime test support: the header-only lifecycle probe module" (Stage 2) |
| `GetWorldStateForTest`, `CreateLiveWorld` | the record's world state resource; a second world through `Zenith_WorldHost_CreateWorld`, frozen with `Zenith_WorldHost_FreezeIsolatedWorld` (§ "World creation" step 9) and set `LIVE` with the schedule marked dirty, so the next prologue step 8 adds its nodes (a freeze failure destroys the world and returns `MODULE_ERROR_REGISTRATION`) | "GAME world host: the transition sink, isolated world creation and the game-flow types" (Stage 3) |
| `GetAsyncResultsForTest` | the world's live table, or the table held in the record's `m_xDetached` between teardown steps `SCRIPTS` and `ASYNC_RESULTS`; null after step `ASYNC_RESULTS` | "GAME world host: retired world asynchronous results" (Stage 3) |
| `GetRendererForTest` | the runtime state's `m_pxRenderer` (null in a headless run), so a GAME suite outside ZenithFlux reaches RPROD's public view read-back | "GAME Stage 7 integration: XR camera output through a stereo view" (Stage 7) |

## World host state

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Zenith_WorldTransition.h (GAME's part of Zenith_RuntimeState)
inline constexpr uint32 uWORLD_MAX_CARRY_OVER_ROOTS = 256;     // SCENE's script-request limit (Scene.RequestTransition)
inline constexpr uint32 uWORLD_MAX_RETIRED_RESULT_TABLES = 4;
inline constexpr uint32 uWORLD_TRANSITION_OPERATIONS = 4;      // the operation table's capacity (handles a requester still holds)

struct Zenith_RetiredResults                                    // 40 bytes
{
	Zenith_Owned<Zenith_AsyncResultTable> m_pxTable;            // the detached world's table (§ "Retired world asynchronous results")
	Zenith_Owned<Zenith_ScenePublicationSink> m_pxSceneSink;    // MOD's Zenith_RuntimeScenePublicationSink of that world
	Zenith_Owned<Zenith_ModulePublicationSink> m_pxModuleSink;  // MOD's single module publication sink of that world (Q76 (a))
	uint64 m_ulInFlightAtDetach = 0;                           // GetInFlightCount() at the detach, for the log line
	uint16 m_uWorldTag = 0;                                    // the detached world's tag (the tag is free again; logs only)
	uint8 m_auReserved[6] = {};
};
static_assert(sizeof(Zenith_RetiredResults) == 40);             // three one-pointer Zenith_Owned + 8 + 2 + 6

struct Zenith_WorldHostGameState
{
	Zenith_Owned<Zenith_OperationTable> m_pxTransitionOperations;   // "world transitions", uWORLD_TRANSITION_OPERATIONS
	Zenith_WorldTransitionRecord m_xTransition;                     // § "Transition record state machine"; one per process
	Zenith_RetiredResults m_axRetired[uWORLD_MAX_RETIRED_RESULT_TABLES];
	uint32 m_uRetiredCount = 0;
	Zenith_Array<Zenith_RetiredResults> m_xRetiredOverflow;         // root heap, empty until a fifth table is held
	Zenith_GameFlowState m_eFlow = GAME_FLOW_STATE_BOOTING;
	uint32 m_uSessionOrdinal = 0;                                  // the WORLD_BEGIN ordinal of the current primary world
	std::atomic<uint32> m_uShutdownRequestedByWorld{ 0 };          // 1 + the requesting tag, or 0
	uint16 m_eRecordedFailure = 0;                                 // replay: the recorded failure code the old world's handler met
	uint64 m_ulRecordedFailureTick = 0;                            //   for the in-flight record, and its tick (§ "Publication payloads", row 4)
};

// One per live or isolated world, beside MOD's attach journals in the world's host record.
struct Zenith_WorldHostGameRecord
{
	Zenith_WorldControl m_xControl;                                 // § "Pause arbitration"
	Zenith_WorldStateResource m_xState;                             // § "World state resource"; the registered resource object
	Zenith_WorldTransitionSinkSlot m_xSinkSlot;                     // § "Transition request intake"
	Zenith_WorldHostPublicationHandler m_xPublicationHandler;       // § "Publication payloads"; knows its world's tag
	Zenith_WorldLifecycleState m_eLifecycle = WORLD_LIFECYCLE_STATE_CREATED;
	uint64 m_ulLastExecutedTick = 0;                               // read out of tick from the world simulation
	uint64 m_ulDivergedTick = 0;                                   // replay: a divergence this world's handler recorded (row 6)
	uint8 m_eDivergence = 0;                                       // which condition of "Replay of a transition" step 5; 0 for none
	Zenith_RetiredResults m_xDetached;                              // teardown: the table and sinks step SCRIPTS detached, held
	                                                                //   until step ASYNC_RESULTS (§ "Teardown order algorithm")
};
```

`Zenith_RuntimeState` (MOD's private runtime state, `Internal/Zenith_RuntimeState.h`) holds one `Zenith_WorldHostGameState` and one `Zenith_WorldHostGameRecord` per world host record (GA5 (e)). `std::atomic` is the atomic primitive ([Z2-N-CONTRACT-312]) and `<atomic>` is one of the allowed standard headers (`Zenith2/Docs/Contracts.md` → "Containers and standard library policy"); `Zenith_Array` is CORE's (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"). Every member is touched on the main thread outside frame work, except the shutdown atomic (written by command playback on sync-point nodes), the two divergence fields of each world record (written only by that world's own publication handler), the two recorded-failure fields and the record's published tick (written only by the handler of the transition's old world, `m_uFromTag`) — all written on the writing world's tick-boundary node and ordered before the next prologue by the frame structure, so no two tick-boundary nodes write one field and the sink slot (§ "Transition request intake"). A test failure is not counted here: it goes to `Zenith_Runtime::RecordTestAssertionFailure()` (§ "Game command kinds").

## World lifecycle state machine

Per world (`Zenith_WorldLifecycleState`, `Internal/Zenith_WorldTransition.h`):

| State | Entered | Leaves to | On |
|---|---|---|---|
| `WORLD_LIFECYCLE_STATE_CREATED = 1` | `Zenith_WorldHost_CreateWorld` returned | `LIVE` (startup world, after step 18); `ACTIVATING` (isolated world) | step 18 compiled its nodes; `RegisterIsolatedActivation` succeeded |
| `WORLD_LIFECYCLE_STATE_ACTIVATING = 2` | an isolated world is being filled in prologue slices | `LOADED`; `DESTROYING` | activation completed; activation failed or the transition was cancelled |
| `WORLD_LIFECYCLE_STATE_LOADED = 3` | activation complete, awaiting the swap | `LIVE`; `DESTROYING` | the swap; a failed swap or shutdown |
| `WORLD_LIFECYCLE_STATE_LIVE = 4` | its nodes are in the frame schedule | `SWAPPED_OUT`; `SHUTTING_DOWN` | the swap that replaces it; the lifecycle shutdown |
| `WORLD_LIFECYCLE_STATE_SWAPPED_OUT = 5` | the swap captured its carry-over | `DESTROYING` | the same swap, at once |
| `WORLD_LIFECYCLE_STATE_SHUTTING_DOWN = 6` | MOD's shutdown step 1 drained its last tick | `DESTROYING` | § "World lifecycle shutdown" |
| `WORLD_LIFECYCLE_STATE_DESTROYING = 7` | teardown started | (record freed) | `Zenith_WorldHost_DestroyWorld` returned |

A `LIVE` world is paused or running by its pause mask (§ "Pause arbitration"); pause is not a lifecycle state, because a paused world keeps its nodes, its tick index and every subsystem's state ([Z2-N-ARCH-764]). Any transition not in the table is `ZENITH_UNREACHABLE`.

## Game flow state machine

Per process (`Zenith_GameFlowState`):

| State | Entered | Leaves to |
|---|---|---|
| `GAME_FLOW_STATE_BOOTING = 1` | `Zenith_Runtime::Create` | `RUNNING` when step 18 succeeds; a failed step unwinds and the record is destroyed |
| `GAME_FLOW_STATE_RUNNING = 2` | step 18 | `TRANSITIONING` when a transition record leaves `NONE`; `SHUTTING_DOWN` |
| `GAME_FLOW_STATE_TRANSITIONING = 3` | a transition is in flight | `RUNNING` when the record reaches `DONE`, `FAILED` or `CANCELLED`; `SHUTTING_DOWN` |
| `GAME_FLOW_STATE_SHUTTING_DOWN = 4` | the frame loop stopped after a shutdown request | none: the runtime is destroyed |

The flow state is diagnostic: it is the gauge `runtime.game.flow` and `Zenith_RuntimeTestAccess::GetGameFlowState` (GA5 (h)), and it is deliberately not in the world state resource, because when a transition starts and how long it takes depend on real-time activation slices, and no tick may read a value that differs between a run and its replay (Z2-D-019).

## World creation

An isolated world (a transition target) is created by the MOD half's `Zenith_WorldHost_CreateWorld`, which performs the step-16 sequence (`Zenith2/Docs/Design/MOD.md` → "Step construction notes" row 16) for one world from a `Zenith_WorldCreateDesc` (§ "World host files and split with MOD"):

| Order | Step | Owner |
|---|---|---|
| 1 | acquire a tag from `Zenith_WorldRegistry` | MOD |
| 2 | create the ECS world with the composition's `m_pfnCreateWorld` and `m_ulWorldSeed` | MOD |
| 3 | create `Zenith_SystemRegistry` and `Zenith_WorldSimulation` | MOD |
| 4 | create the per-world engine state: the scene world state with `m_pxTransitions` = the world host's sink (§ "Transition request intake"), the script world, the input bridge resources | MOD (SCENE, SCRIPT, PLAT bodies) |
| 5 | GAME's per-world record: construct `Zenith_WorldHostGameRecord`; add the resource `Zenith.Runtime.WorldState` (owning phase `PHASE_INPUT`) pointing at `m_xState`; on the script world's `Zenith_AsyncResultTable`: `DeclareSource(ASYNC_SOURCE_WORLD_LOAD, 1, 8)` (value 6 of `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry", which SCRIPT owns) and `SetHandler(ASYNC_SOURCE_WORLD_LOAD, &record.m_xPublicationHandler)`, the record's own handler constructed with the new world's tag (§ "Publication payloads") | GAME |
| 6 | attach every started module in canonical order (`Zenith2/Docs/Design/MOD.md` → "Module lifecycle state machine") | MOD |
| 7 | the runtime's per-world engine extractors | MOD (RPROD bodies) |
| 8 | when `m_bInputWorld`: `Zenith_WorldHost_SetInputWorld(tag)` | MOD |
| 9 | when `m_bIsolated`, after `CreateWorld` returned: `Zenith_SystemRegistry::Freeze()` on the new world's registry (`Zenith2/Docs/Design/ECS/SystemAPI.md`; the startup world freezes at step 18 instead), so every schedule error of the new world — an authority clash, a cross-entity read, a merge-point limit under the world's `SetLimits` — is found while the old world is untouched | GAME |

Step 5 runs before the modules attach so that a module's `AttachWorld` may declare reads of `Zenith.Runtime.WorldState`. A failure in any step unwinds the steps before it (Z2-D-045): a failed step 1–8 is `CreateWorld`'s `Zenith_ModuleError` (step 5's resource registration rejected, which only an engine defect causes, is `MODULE_ERROR_REGISTRATION`), mapped by the transition to `WORLD_TRANSITION_FAILURE_WORLD_CREATE`; a failed step 9 returns a `Zenith_ScheduleError`, whose diagnostics the host logs one line each (`runtime.world: transition target world <tag>: schedule error <code>: <diagnostic>`), destroys the world through `Zenith_WorldHost_DestroyWorld` and maps to `WORLD_TRANSITION_FAILURE_SCHEDULE`.

The isolated world's seed is `Zenith_DeriveWorldSeed(ulOldSeed, uNextSessionOrdinal)` = `Zenith_Hash64` of the 12 little-endian bytes (old seed, ordinal) (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64"), so a run and its replay create the same seed; a replay compares it with the logged `WORLD_BEGIN` seed (§ "Replay of a transition"). A world restored from a save takes the saved seed instead (`Zenith_PreparedSave::GetWorldSeed(0)`).

## World transition

### Transition sources

| Source | Entry | Target | Carry-over |
|---|---|---|---|
| `WORLD_TRANSITION_SOURCE_SCENE = 1` | `Zenith_Runtime::RequestWorldTransition` (C++, between frames); `Zenith_RecordRequestSceneTransition` playback through the sink (scripts: `Scene.RequestTransition`) | a cooked scene | the desc's list, then every `Zenith.Runtime.CarryOver` entity (§ "Carry-over") |
| `WORLD_TRANSITION_SOURCE_SAVE = 2` (Stage 4) | `Zenith_WorldHost_BeginTransitionFromSave` from SAVE's runtime bridge after a `Save.Load` prepared the save | the saved world | none: a load replaces the world whole |

<!-- assumption: pending SAVE -->
SAVE's row SV7 (revised, answering GAME's row GA11) fixes the save source's seams, and GAME uses them as stated in `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host": every SAVE image holds exactly one world (the resume and exit-autosave images capture the primary world only), so GAME restores world index 0 and its refusal of any other `Zenith_PreparedSave::GetWorldCount()` with `WORLD_TRANSITION_FAILURE_SAVE_WORLD_COUNT` (logged with the count) is a corrupt-input guard that a SAVE-written file never trips; a script `Save.Load` carries two completion IDs of the requesting world, both allocated at intake in merge order on the committing merge node — SCRIPT's `ASYNC_SOURCE_SAVE` ID (which always publishes `Zenith.SaveCompleted`) and, right after it, the `ASYNC_SOURCE_WORLD_LOAD` ID; SAVE's bridge calls `Zenith_WorldHost_BeginTransitionFromSave(state, tag, save, worldLoadID)` in prologue step 7 when the load was read, verified and migrated, after which the host owns that ID (and completes it with a failure payload on any subsequent refusal), and on a load failure the bridge itself completes it with `WORLD_TRANSITION_FAILURE_PREPARE` (§ "Failed save loads": the host never holds that ID and its handler ignores the publication); the new world is bound to the save system by SAVE's own `Zenith_RuntimeSave_AttachWorld` during the module attach of § "World creation" step 6; and `Zenith.Runtime.WorldState`'s `ContinuityTickBase` is saved and restored as a serialized resource property (§ "World state resource"). The save's other accessors (`GetWorldSeed`, `GetWorldTick`, `GetBaseScene`, `BeginWorldRestore`, `Zenith_WorldRestoreJob::Step()` within `save.restoreBudgetUs`) are SAVE's public API. Cross-area row GA11 records what remains open: (g), the replay form's null-save call (§ "Failed save loads"); (d') `FindPublication` and (f) the wording of SAVE's replay sentence are answered. GAME adopts SAVE's row SV25, the restore timeout of a save-source `ACTIVATING` record (§ "Transition record state machine").
<!-- /assumption -->

### Transition request intake

**Direct call.** `Zenith_WorldHost_BeginTransition(xState, uFrom, xDesc)` on the main thread with no frame running (`Zenith_Runtime`'s `MAIN_THREAD`, reentrancy `NONE`):

1. A null `xDesc.m_xScene`, or more than `uWORLD_MAX_CARRY_OVER_ROOTS` carry-over IDs, is a C++ caller's programmer error and `ZENITH_ASSERT`s; scripts never reach this function (they reach the sink), so no untrusted input can trigger it ([Z2-N-CONTRACT-472]).
2. Allocate the old world's completion ID: `GetAsyncResults().AllocateCompletionID(ASYNC_SOURCE_WORLD_LOAD)` (main thread, no frame running); `ASYNC_TABLE_ERROR_FULL` (the world's single world-load ID is already held: a sink request of the same world is mid-intake, a transition of the world is in flight, or a `Save.Load` of the world holds it) → `RUNTIME_ERROR_TRANSITION_IN_PROGRESS`, with no ID to complete. In a `--replay` run the recorded outcome of the new ID is read now (`FindPublication`, § "Intake in a replay").
3. The record is not idle (any state but `NONE`, `DONE`, `FAILED`, `CANCELLED`), or a sink slot of any world holds a request → complete the ID with the `WORLD_TRANSITION_FAILURE_IN_PROGRESS` failure payload (§ "Publication payloads"), log `runtime.world: world <tag>: transition to <scene> refused: WORLD_TRANSITION_FAILURE_IN_PROGRESS`, count `world.transition.refused`, and return `RUNTIME_ERROR_TRANSITION_IN_PROGRESS`; the record is untouched. The allocation comes first so that this refusal, like every intake refusal that holds an ID, is a logged publication a replay follows.
4. Reserve the peak (§ "Transition record state machine", `RESERVING`); a refusal completes the completion ID with the failure payload (§ "Publication payloads") and returns `RUNTIME_ERROR_TRANSITION_PEAK_REFUSED` ([Z2-N-ARCH-559]). In a `--replay` run steps 3 to 5 do not decide from live state: § "Replay of a transition", "Intake in a replay".
5. `Issue()` on the operation table (`OPERATION_ERROR_TABLE_FULL`, a requester holding four stale handles → release the reservation, complete the ID with `WORLD_TRANSITION_FAILURE_OPERATION_TABLE` and return `RUNTIME_ERROR_TRANSITION_IN_PROGRESS`); copy the desc into the record (the scene handle gains a reference, the IDs are copied into the record's fixed array); `BeginPrepare` (§ "Transition record state machine", `PREPARING`); return the handle.

**Sink.** The world host implements `Zenith_SceneTransitionSink` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneWorld header"). `RequestTransition(uFrom, xScene, axCarryOver)` is called on world `uFrom`'s sync-point node, during frame work, so it cannot reserve, prepare or allocate; it writes the world's slot:

```cpp
struct Zenith_WorldTransitionSinkSlot           // 4,128 bytes, one per world record
{
	Zenith_PersistentID m_xScene;               // the scene's asset persistent ID
	Zenith_PersistentID m_axCarryOver[uWORLD_MAX_CARRY_OVER_ROOTS];
	uint64 m_ulCompletionID = 0;               // allocated here, on the merge chain
	uint32 m_uCarryOverCount = 0;
	uint32 m_bPending = 0;                     // written by the sink, read and cleared by prologue step 7
};
static_assert(sizeof(Zenith_WorldTransitionSinkSlot) == 4128);   // 16 + 256 × 16 + 8 + 4 + 4
```

1. `m_bPending` already set (a second request of the same world before the next prologue) → counter `world.transition.request_dropped`, log `runtime.world: world <tag>: transition request to <scene> dropped; one is already pending`, return.
2. `AllocateCompletionID(ASYNC_SOURCE_WORLD_LOAD)` on the merge chain, which the table's affinity allows (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptAsync header") — deterministic because sync-point playback runs in merge order ([Z2-N-ARCH-481]). `ASYNC_TABLE_ERROR_FULL` (a transition of this world is already in flight) → counter `world.transition.request_dropped`, log, return.
3. Copy the scene ID and up to 256 carry-over IDs (SCENE has already truncated longer lists); set `m_bPending = 1`.

The slot needs no lock: one world's sync points run one after another ([Z2-N-ARCH-479]), and the prologue reads it with no frame running. Prologue step 7 takes each pending slot in ascending world-tag order (§ "Prologue step 7 procedure", step 2): it acquires the scene by persistent ID (`Zenith_AssetRegistry::Acquire<Zenith_Scene>`), and then runs steps 3, 4 and 5 of the direct call with the slot's completion ID; a refusal at intake completes that ID with the failure payload of its code — the scene acquisition failed: `WORLD_TRANSITION_FAILURE_SCENE_MISSING`; step 3 (the record is busy with a transition of another world, because a transition of this world would have made the sink's own allocation fail): `WORLD_TRANSITION_FAILURE_IN_PROGRESS`; step 4: `PEAK_REFUSED`; step 5: `OPERATION_TABLE` — logs `runtime.world: world <tag>: transition to <scene> refused: <failure>` and counts `world.transition.refused`. A `SCENE_MISSING` or `IN_PROGRESS` refusal leaves the record untouched (it may belong to the other transition), so its completion ID never becomes the record's; a `PEAK_REFUSED` or `OPERATION_TABLE` refusal ends the record `FAILED`, holding that completion ID (§ "Transition record state machine"). In a replay the sink's intake follows the log exactly as the direct call does (§ "Replay of a transition"). Its operation handle is held by the record itself (there is no external requester), so no handle is lost.

### Transition record state machine

```cpp
enum Zenith_WorldTransitionState : uint8
{
	WORLD_TRANSITION_STATE_NONE = 0,
	WORLD_TRANSITION_STATE_RESERVING = 1,           // transient, inside intake
	WORLD_TRANSITION_STATE_PREPARING = 2,           // SCENE's preparation operation running (scene source)
	WORLD_TRANSITION_STATE_ACTIVATING = 3,          // isolated world created; activation (or save restore) in prologue slices
	WORLD_TRANSITION_STATE_AWAITING_PUBLICATION = 4,// activation complete; the completion parked for the old world
	WORLD_TRANSITION_STATE_READY_TO_SWAP = 5,       // published (and logged) at an old-world tick boundary
	WORLD_TRANSITION_STATE_SWAPPING = 6,            // transient, inside the swap procedure
	WORLD_TRANSITION_STATE_DONE = 7,
	WORLD_TRANSITION_STATE_FAILED = 8,
	WORLD_TRANSITION_STATE_CANCELLED = 9,
};

enum Zenith_WorldTransitionFailure : uint16      // the operation's failure code (Zenith_OperationHandle::GetFailureCode)
{
	WORLD_TRANSITION_FAILURE_PEAK_REFUSED = 1,
	WORLD_TRANSITION_FAILURE_OPERATION_TABLE = 2,
	WORLD_TRANSITION_FAILURE_SCENE_MISSING = 3,     // Acquire<Zenith_Scene> of a sink request failed
	WORLD_TRANSITION_FAILURE_PREPARE = 4,           // BeginPrepare refused, or the preparation operation failed
	WORLD_TRANSITION_FAILURE_WORLD_CREATE = 5,
	WORLD_TRANSITION_FAILURE_ACTIVATION = 6,
	WORLD_TRANSITION_FAILURE_RESTORE = 7,
	WORLD_TRANSITION_FAILURE_SAVE_WORLD_COUNT = 8,  // a prepared save whose world count is not 1
	WORLD_TRANSITION_FAILURE_CARRY_OVER = 9,        // Zenith_CaptureEntities or Zenith_ActivateSceneData failed
	WORLD_TRANSITION_FAILURE_REPLAY_DIVERGED = 10,  // a replay's log disagrees with the transition (§ "Replay of a transition")
	WORLD_TRANSITION_FAILURE_CANCELLED = 11,        // the publication payload of a cancelled transition; the operation reports CANCELLED
	WORLD_TRANSITION_FAILURE_SCHEDULE = 12,         // the new world's Freeze or the trial frame-graph compile of the swap failed
	WORLD_TRANSITION_FAILURE_IN_PROGRESS = 13,      // an ID-holding request (a sink slot, a Save.Load hand-off) taken while the record
	                                                // is busy with another world's transition (§ "Transition request intake")
};

struct Zenith_WorldTransitionRecord
{
	Zenith_WorldTransitionState m_eState = WORLD_TRANSITION_STATE_NONE;
	Zenith_WorldTransitionSource m_eSource = WORLD_TRANSITION_SOURCE_SCENE;
	uint16 m_uFromTag = 0;
	uint16 m_uNewTag = 0;                          // 0 until the isolated world exists
	uint64 m_ulIssueID = 0;                        // the operation's issue ID: the WORLD_LOADED payload
	uint64 m_ulCompletionID = 0;                   // in the old world's async table; kept after the record ends, until the next intake
	uint64 m_ulPublishedTick = 0;                  // the old-world tick whose boundary published it
	uint64 m_ulRequestFrame = 0;                   // frame index at intake, for world.transition.frames
	uint64 m_ulActivationUs = 0;                   // Σ of the activation slices
	uint64 m_ulActivatingSinceUs = 0;              // save source: monotonic µs at entering ACTIVATING (restore timeout, SV25)
	Zenith_OperationHandle m_xHandle;               // held here for sink requests; moved to the caller for direct ones
	Zenith_OperationTicket m_xTicket;
	Zenith_TransitionReservation m_xPeak;
	Zenith_AssetHandle<Zenith_Scene> m_xScene;
	Zenith_OperationHandle m_xPrepare;              // SCENE's preparation operation
	Zenith_OperationHandle m_xActivation;           // RegisterIsolatedActivation's completion
	Zenith_Owned<Zenith_PreparedSave> m_pxSave;     // save source
	Zenith_Owned<Zenith_WorldRestoreJob> m_pxRestore;
	Zenith_PersistentID m_axCarryOver[uWORLD_MAX_CARRY_OVER_ROOTS];
	uint32 m_uCarryOverCount = 0;
	uint16 m_eFailure = 0;                         // the Zenith_WorldTransitionFailure of a FAILED or CANCELLED record; 0 otherwise
	uint8 m_bCompleted = 0;                        // 1 once m_ulCompletionID has been completed (live) or its outcome applied (replay)
	uint8 m_bReserved = 0;                         // 1 while m_xPeak holds a reservation
	uint8 m_eRecordedOutcome = 0;                  // replay only, set at intake from FindPublication: 0 no publication,
	                                                // 1 WORLD_LOADED, 2 a failure (§ "Intake in a replay")
	uint16 m_eRecordedCode = 0;                    // replay only: the recorded failure code when m_eRecordedOutcome is 2
	uint8 m_bLiveEnded = 0;                        // replay only: 1 once a live failure ahead of the recorded one released
	                                                // everything; the record stays busy until its recorded ending (§ "Intake in a replay")
};
```

| State | Event (checked in prologue step 7 unless noted) | Next | Action |
|---|---|---|---|
| `NONE`, `DONE`, `FAILED`, `CANCELLED` | intake (direct or sink) | `RESERVING` | `m_ulRequestFrame` = the frame index; counter `world.transitions.requested` |
| `RESERVING` | `ReserveTransitionPeak(TRANSITION_PEAK_WORLD_TRANSITION, B)` succeeded (B below) | `PREPARING` (scene) or `ACTIVATING` (save) | issue the operation; scene: `BeginPrepare({ m_xScene, ID scope null, UNKNOWN_TYPE_POLICY_FAIL, "transition" })`; save: check `GetWorldCount() == 1` (else `FAILED`, `SAVE_WORLD_COUNT`), create the isolated world with `GetWorldSeed(0)` and origin `WORLD_ORIGIN_SAVE_LOAD` (§ "World creation"; SAVE's runtime part binds the world inside its own `AttachWorld`, `Zenith_RuntimeSave_AttachWorld`, during step 6 of the creation, SV7 (b)), then `BeginWorldRestore(0, Zenith_SaveSystem::MakeRestoreDesc(new tag, bReplay))` (its budget `save.restoreBudgetUs`; `bUnderPublicationSource` is true exactly in a `--replay` run, because every recorded result of the restored world then comes from the log, `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host"); `m_ulActivatingSinceUs` = the platform's monotonic time |
| `RESERVING` | refused | `FAILED` | failure `PEAK_REFUSED`; `RUNTIME_ERROR_TRANSITION_PEAK_REFUSED` to a direct caller |
| `PREPARING` | `m_xPrepare` `SUCCEEDED` | `ACTIVATING` | `TakePrepared`; create the isolated world (§ "World creation", `m_bIsolated = true`, origin `WORLD_ORIGIN_TRANSITION`); `RegisterIsolatedActivation(prepared, world, sceneState, m_xActivation)`; lifecycle `ACTIVATING` |
| `PREPARING` | `m_xPrepare` `FAILED` | `FAILED` | failure `PREPARE` (the SCENE error is logged with the scene's name) |
| `PREPARING` (scene) or `RESERVING` (save) | world creation failed (§ "World creation" steps 1–8) or the new world's `Freeze` failed (step 9) | `FAILED` | failure `WORLD_CREATE` or `SCHEDULE`; nothing was created that outlives the failure (creation unwinds, a frozen-failed world is destroyed) |
| `ACTIVATING` | `m_xActivation` `SUCCEEDED` (scene), or the restore job reports done (save) | `AWAITING_PUBLICATION` | lifecycle `LOADED`; outside a replay `Complete(m_ulCompletionID, PUBLICATION_KIND_WORLD_LOADED, issue ID)` into the old world's table |
| `ACTIVATING` | activation or restore failed | `FAILED` | destroy the isolated world; failure `ACTIVATION` or `RESTORE` |
| `ACTIVATING` (save source, outside a replay) | prologue step 4 finds the restore job not done `runtime.startupLoadTimeoutMs` after `m_ulActivatingSinceUs` (SAVE's row SV25: a participant whose `IsRestoreReady` never returns true would otherwise hold the transition, its world-load ID and the world's single world-load slot until a cancel) | `FAILED` | failure `RESTORE` through § "Failure application procedure", after logging `runtime.world: world <tag>: save restore not ready after <ms> ms (participant <key>)` with `Zenith_WorldRestoreJob::GetFirstUnreadyParticipantKey()` (`Zenith2/Docs/Design/SAVE/PublicAPI.md`); in a `--replay` run the timeout decides nothing: the recorded outcome does (§ "Intake in a replay"; a recorded `RESTORE` ends the intake at once) |
| `AWAITING_PUBLICATION` | (tick boundary of old-world tick t, the host's publication handler) the `WORLD_LOADED` publication with this issue ID | `READY_TO_SWAP` | `m_ulPublishedTick = t` |
| `READY_TO_SWAP` | prologue step 7 of the frame after the one that ran tick `m_ulPublishedTick` | `SWAPPING` → `DONE` | § "Swap procedure" |
| `SWAPPING` | the trial compile of § "Swap procedure" step 2 failed | `FAILED` | destroy the new world; the old world stays `LIVE`; failure `SCHEDULE` |
| `SWAPPING` | carry-over capture or activation failed | `FAILED` | destroy the new world; the old world stays `LIVE`; failure `CARRY_OVER` |
| `RESERVING`, `PREPARING`, `ACTIVATING` | the ticket reports a cancel request | `CANCELLED` | § "Transition failure and cancellation"; the completion ID is completed with the failure payload `WORLD_TRANSITION_FAILURE_CANCELLED` |
| any but `NONE`, `DONE`, `FAILED`, `CANCELLED` | the lifecycle shutdown | `CANCELLED` | the same release, before any world is torn down |

The reserved bytes B are `max(world.transitionReserveFloorBytes, the simulation subsystem's resident bytes at the last Update)` — `Zenith_MemoryGovernor::GetStats().m_axSubsystems[MEMORY_SUBSYSTEM_SIMULATION].m_ulResidentBytes` — rounded up to a multiple of 1 MiB: the new world is assumed to be at most as large as the worlds resident now, and the floor (u64 configuration key, 0 … 2⁴⁰, default 33,554,432) keeps a transition from an almost empty world honest. A refusal is the governor's: B above `memory.peak.worldTransitionBytes` minus the outstanding reservation (Z2-B-CORE-007; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm"). The reservation is released once both worlds are no longer resident together (§ "Swap procedure" step 10, after the old world was destroyed at step 7) or when the transition ends in `FAILED` or `CANCELLED`.

Every row above whose next state is `FAILED` or `CANCELLED`, except the swap failures (whose path is § "Swap procedure" step 13), ends through § "Failure application procedure" with that row's code; a refusal inside intake runs it with the record still in `RESERVING`, so it releases only what intake took.

From `AWAITING_PUBLICATION` on, a cancel request is ignored and counted (`world.transition.cancel_ignored`): the completion is parked and will be published and logged, and a replay swaps after the logged tick whatever any handle asked ([Z2-N-ARCH-566]).

### Publication payloads

| Outcome | Publication kind | Payload (8 bytes, little-endian) |
|---|---|---|
| activation complete | `PUBLICATION_KIND_WORLD_LOADED` | `uint64` the operation's issue ID ([Z2-N-ARCH-788]'s payload) |
| failure after the completion ID was allocated | `PUBLICATION_KIND_GENERIC_RECORDED` | `uint16` the `Zenith_WorldTransitionFailure`, then 6 zero bytes |

Every completion ID the host allocates or receives is completed exactly once — live by the host's own `Complete`, in a replay by the recorded publication ([Z2-N-ARCH-783]) — so no slot of the old world's async table leaks and a replay sees the same publications. SAVE's runtime bridge completes the other `ASYNC_SOURCE_WORLD_LOAD` IDs that reach these tables: the `Save.Load` IDs whose load failed before the hand-off ("failure: the bridge completes it with GAME's 8-byte failure payload carrying `WORLD_TRANSITION_FAILURE_PREPARE`", `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host"), which the host never held.

**The publication handler.** Each world record owns one `Zenith_WorldHostPublicationHandler : Zenith_AsyncPublicationHandler` (`Internal/Zenith_WorldTransition.h`, members: the world's tag W and a pointer to the runtime's `Zenith_WorldHostGameState`), registered on that world's table at § "World creation" step 5. `OnPublished(xPublication, xWorld)` runs on W's tick-boundary node for each `ASYNC_SOURCE_WORLD_LOAD` publication, in completion-ID order (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptHost header"), with t the tick whose boundary publishes it. Its match key is the pair (W, `xPublication.m_ulCompletionID`) against the record R's (`m_uFromTag`, `m_ulCompletionID`) — completion IDs are per world, so the tag is part of the key, and the failure payload carries no issue ID. A payload is *loaded* when the kind is `PUBLICATION_KIND_WORLD_LOADED` with 8 bytes, and a *failure f* when the kind is `PUBLICATION_KIND_GENERIC_RECORDED` with 8 bytes whose first two hold a code f in 1–13 other than 10 (`REPLAY_DIVERGED`) and whose other six are zero; anything else is *malformed*. The rows are tested in order; the first that matches applies:

| # | Publication | R | Normal run | `--replay` run |
|---|---|---|---|---|
| 1 | loaded, key matches, payload = R's `m_ulIssueID` | `AWAITING_PUBLICATION` | R → `READY_TO_SWAP`, `m_ulPublishedTick = t` | the same |
| 2 | loaded, key matches | any other state, or payload ≠ issue ID | `ZENITH_ASSERT` (an engine defect: the host completes `WORLD_LOADED` only from `AWAITING_PUBLICATION`, with the issue ID); nothing changes | divergence `loaded publication for a record in state <s>` (recorded for step 7) |
| 3 | failure f, key matches | `FAILED` or `CANCELLED` | the echo of the host's own `Complete`: nothing | the echo of an outcome this run already applied with the recorded code (an intake-time refusal or a save-source ending of § "Intake in a replay"): nothing |
| 4 | failure f, key matches | `PREPARING`, `ACTIVATING` or `AWAITING_PUBLICATION` | `ZENITH_ASSERT` (the host completes a failure only after R has ended); nothing changes | the recorded run ended here: store `m_eRecordedFailure = f`, `m_ulRecordedFailureTick = t`; prologue step 7 applies it (§ "Prologue step 7 procedure" step 6) |
| 5 | failure f, key does not match | any | nothing: SAVE's failure for a `Save.Load` the host never received (`CompleteLinkedLoadRefused` or the bridge), or the echo of a refusal of the direct call, the sink or the hand-off (`SCENE_MISSING`, `IN_PROGRESS`) whose ID never became the record's | nothing; never a divergence (a replay of a failed `Save.Load` exits 0); also the recorded payload of a record that a subsequent intake already ended (rule 2 of § "Intake in a replay") |
| 6 | anything else: loaded with a key that does not match; a failure whose key matches R in `NONE`, `READY_TO_SWAP`, `SWAPPING` or `DONE` (a published success cannot also fail); malformed | any | `ZENITH_ASSERT`; nothing changes | divergence `unexpected world-load publication <c>` (recorded for step 7) |

"Recorded for step 7" writes W's record fields `m_eDivergence` (the row's condition) and `m_ulDivergedTick = t` when `m_eDivergence` is still 0, so the first divergence of a world wins. The handler writes no simulation state and does no main-thread work (destroying a world, releasing a reservation and requesting shutdown are main-thread work): it stores integers into main-thread records, which the frame structure orders before the next prologue reads them. Outside a replay rows 2, 4 and 6 cannot occur, so the asserts guard the host's own code only, never untrusted input.

### Prologue step 7 procedure

`Zenith_WorldHost_StepTransitions(xState)` and its neighbours run in step 7 of the frame prologue (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue"), in this order, inside the zone `Runtime.WorldHost.Step7`:

1. `Zenith_WorldHost_ApplyPauseMasks` (§ "Pause arbitration"); the frame loop driver calls it, then `Zenith_WorldHost_StepTransitions`, which runs steps 2–8.
2. Intake: for each world in ascending tag order whose sink slot is pending, the sink intake of § "Transition request intake"; clear the slot.
3. Cancellation and preparation: a cancel request moves the record to `CANCELLED`; a `PREPARING` record whose operation completed moves on (creating the isolated world — SCENE's row 3, "Scene transitions" table of `Zenith2/Docs/Design/SCENE/Loading.md`).
4. `Zenith_SceneLoader::StepActivations(scene.activationBudgetUs)` (the call MOD's driver made itself before the GAME ticket moved it here, SCENE's row 4; it also starts pending additive preparations; it runs every prologue whether or not a transition is in flight); for a save source, `m_pxRestore->Step()` (bounded by the desc's `save.restoreBudgetUs`), then, outside a replay, the restore timeout of § "Transition record state machine" (`runtime.startupLoadTimeoutMs`, SAVE's row SV25); add the slice's measured time to `m_ulActivationUs`.
5. Activation completion: an `ACTIVATING` record whose activation finished moves to `AWAITING_PUBLICATION` (and parks its completion).
6. In a replay, in this order: (a) a divergence any world's publication handler recorded (`m_eDivergence != 0`, rows 2 and 6 of § "Publication payloads"), or one found by the checks of § "Replay of a transition" step 5, runs that step's divergence actions, and steps 6 (b), 6 (c) and 7 are skipped; (b) a recorded failure the handler stored for the in-flight record (row 4) ends it through § "Failure application procedure" with that code (`CANCELLED`: as a cancellation), with no `Complete` — only the procedure's steps 6–8 when `m_bLiveEnded` is set (rule 3 of § "Intake in a replay") — and clears `m_eRecordedFailure`; (c) the activation hold of § "Replay of a transition" step 3.
7. Swap: a `READY_TO_SWAP` record whose published tick is at or before the last executed tick of the old world runs § "Swap procedure".
8. `Zenith_WorldHost_ReleaseRetiredResults(xState, false)`.

In a replay steps 3–5 skip a record whose `m_bLiveEnded` is set: its work is already released and only its recorded ending remains. Steps 3–7 are SCENE's assumed order for step 7 (create, activate, swap; `Zenith2/Docs/Design/SCENE/Loading.md` → "Scene transitions"), so a record never activates and swaps in the same prologue: its completion must first be published at an old-world tick boundary, which happens inside the frame.

### Swap procedure

On the main thread, no frame running, old world O (tag `m_uFromTag`, last executed tick L), new world N (frozen at its creation, § "World creation" step 9):

1. State `SWAPPING`.
2. **Trial compile.** `Zenith_FrameLoop_PrecompileSwap(xState, O, N)` (§ "World host files and split with MOD"): the frame graph the swap will run is compiled and cached before anything of O is touched. Failure → step 13 with `WORLD_TRANSITION_FAILURE_SCHEDULE`; the cache's diagnostics are logged one line each (`runtime.world: swap of world <O> for world <N>: frame graph error <code>: <diagnostic>`).
3. **Carry-over capture** (scene source only; a save source carries nothing, so steps 3 and 4 are skipped for `WORLD_TRANSITION_SOURCE_SAVE`): build the root list (§ "Carry-over") and call `Zenith_CaptureEntities(O.GetInspector(), types, strings, roots, swapArena, data)` ([Z2-N-ARCH-567]: after O's final end-of-tick playback, which ran inside the frame that just completed). Failure → step 13.
4. **Carry-over activation** into N: `Zenith_ActivateSceneData(data, N, N's scene world state, assets)`; a root whose persistent ID already exists in N is skipped by SCENE and logged (`SCENE_ERROR_DUPLICATE_ID` per root); any other error → step 13.
5. **World state.** Write N's world state resource (§ "World state resource"): origin, session ordinal + 1, first tick 1, the continuity tick base B_N = B_O + L, the carried root count, the previous scene ID (O's scene). For a world restored from a save: origin `WORLD_ORIGIN_SAVE_LOAD`, session ordinal + 1, first tick = `GetWorldTick(0)` + 1, carried roots 0, previous scene O's scene, and the continuity tick base kept as SAVE restored it into the resource (cross-area row GA11 (e)): the host does not recompute it, because the saved base already counts every tick of the saved world's history.
6. Detach O's asynchronous results and retire them with MOD's scene and module publication sinks of O (§ "Retired world asynchronous results").
7. Lifecycle O → `SWAPPED_OUT` → `DESTROYING`; `Zenith_WorldHost_DestroyWorld(O)` (§ "Teardown order algorithm"; listeners receive `LIFECYCLE_EVENT_DESTROYED` per entity only in debug diagnostics mode, `Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition" step 3). The modules' `DetachWorld` calls cancel their own operations for O (MOD: a module owns its world's work); what they and their background jobs complete afterwards reaches O's retired table through the retired sinks.
8. If O held the input-world mark: `Zenith_WorldHost_SetInputWorld(N)` (it cannot fail: N is live).
9. Lifecycle N → `LIVE`; mark the schedule dirty, so step 8 of the same prologue calls `EmitInto` for N and rebuilds the frame graph ([Z2-N-ARCH-424]; `Zenith2/Docs/Design/MOD.md` → "Frame loop driver", the structural trigger). The rebuilt graph is the one step 2 compiled, so `Acquire` returns the cached schedule (one compile per swap). Should that `Acquire` still fail — only an allocation failure of the cache can cause it — MOD's driver records an unrecoverable runtime error and the run ends with exit code 3 (`Zenith2/Docs/Design/MOD.md` → "Frame loop driver"); O no longer exists to fall back to, and the failure is logged `runtime.world: schedule of swapped world <N> failed to install after a successful trial compile`.
10. Release the transition reservation (both worlds are no longer resident together).
11. When recording: `Zenith_ReplayLogWriter::SwapWorld(O, L, N, N's scene content hash)`, then `BeginWorld(N, seed, scene ID, hash)` (the order Architecture's log needs: `WORLD_SWAP`, then the new world's `WORLD_BEGIN`; GA11); for a world restored from a save, the scene is the save's base scene (`GetBaseScene(0)`) and `Zenith_RuntimeReplay_LogRestoredWorld` follows at once, writing the restored image as the new world's first checkpoint (`Zenith2/Docs/Design/SAVE/Formats.md` → "Restored-world convention"; `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host"; SAVE's row SV7 (c)). A writer error stops recording and never fails the swap (SAVE's rule).
12. `m_xTicket.CompleteSucceeded()`; state `DONE`; `m_uSessionOrdinal += 1`; counters `world.transitions.succeeded`, `world.transition.frames` (frames since intake), `world.transition.activation_us`; log `runtime.world: swapped world <O> (last tick <L>) for world <N> (<scene>, <k> roots carried) in <frames> frames`.
13. **Failure path** (from 2, 3 or 4): destroy N, release the reservation, `CompleteFailed(code)` with `SCHEDULE` (from 2) or `CARRY_OVER` (from 3, 4), state `FAILED`, counter `world.transitions.failed`; O stays `LIVE` and keeps ticking, and nothing of O was changed. The failure is logged; the completion ID was already published (as `WORLD_LOADED`), so there is no second publication; O's scripts observe that the world did not change.

The host calls the optional test hook `m_pfnOnSwapStep` of `Zenith_RuntimeTestHooks` before each of steps 2–12 (GA5 (h)), declared beside MOD's startup hooks in `Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h` with MOD's convention that `false` injects a failure:

```cpp
using Zenith_PfnSwapStepHook = bool (*)(uint16 uFromTag, uint16 uToTag, uint8 uStep, void* pUserData);
using Zenith_PfnTeardownStepHook = void (*)(uint16 uWorldTag, uint8 uStep, void* pUserData);
// Zenith_RuntimeTestHooks gains: Zenith_PfnSwapStepHook m_pfnOnSwapStep = nullptr;
//                                Zenith_PfnTeardownStepHook m_pfnOnTeardownStep = nullptr;   (both receive m_pUserData)
```

For uStep 2, 3 or 4 a `false` return fails that step without running it, exactly as its own failure would: step 2 → step 13 with `SCHEDULE` and the log line `runtime.world: swap of world <O> for world <N>: step <s> failed by the test hook`; steps 3 and 4 → step 13 with `CARRY_OVER` and the same line. For uStep 5–12 the return value is ignored (those steps cannot fail). This is how the scenario tests read the old world's table before step 6 and how `Test_WorldTransition_SwapFailureLeavesOldWorld` and `Test_WorldTransition_ScheduleFailureLeavesOldWorld` inject faults at steps 2–4; the fault-injecting heap of `Zenith_RunFaultSweep` covers the allocation failures of the same steps in `Test_WorldTransition_FaultSweep`.

Steps 2–4 can fail and change nothing of O; steps 5–12 cannot fail except the allocation case of step 9, so a swap is all or nothing: every schedule error the new world can have is found either at its `Freeze` (before it is published) or by step 2 (before O is touched). N's first tick boundary runs in the frame that follows, with tick index 1 (a restored world: its restored tick + 1) ([Z2-N-ARCH-557]; LifecycleCommands step 5).

### Replay of a transition

The replay reader's lookups are SAVE's (`Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header"; GA11 (d) and (d'), both adopted): `Zenith_ReplayLogReader::FindNextPublication(uint16 uWorldTag, uint8 uSourceKind, uint64 ulAfterTick, uint64& ulTickOut, uint64& ulCompletionIDOut) const` — the first publication of that world and source kind at a tick after `ulAfterTick` (false when the log holds none; the reader holds no cursor, so the caller passes the tick it reached) — and `FindPublication(uint16 uWorldTag, uint8 uSourceKind, uint64 ulCompletionID, Zenith_ReplayPublicationPeek& xOut) const`, the recorded publication of one completion ID with its tick, `Zenith_PublicationKind` and first 8 payload bytes (the 32-byte `Zenith_ReplayPublicationPeek`), which § "Intake in a replay" reads because an intake decision needs the outcome's kind and failure code, not only its tick; plus `GetSessionAfterSwap`, `GetRestoredCheckpoint` and the next `WORLD_SWAP` and `WORLD_BEGIN` records' fields (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host"). GAME uses them exactly so.

In a `--replay` run:

1. Requests are reproduced, not read from the log: a script's `Scene.RequestTransition` replays bit for bit, so the sink receives it at the same sync point and allocates the same completion ID; a direct C++ request is re-issued by whoever issued it (a test harness issues it after the same tick) and allocates the same ID, because the allocation sequence of the old world's table is itself replayed.
2. Intake decisions are the recording's, never re-decided from live state (§ "Intake in a replay").
3. Every prologue step 7, for a record in `PREPARING` or `ACTIVATING` whose recorded outcome (§ "Intake in a replay") is `WORLD_LOADED`: when `FindNextPublication(old world, ASYNC_SOURCE_WORLD_LOAD, the old world's last executed tick, tick, completion ID)` names the record's completion ID at a tick in the range this frame will run (from the clock's tick count T of step 2), the host completes preparation and activation without a budget before the frame starts (scene source: it waits on the preparation with `Zenith_JobSystem::RunRoot` and calls `StepActivations(UINT64_MAX)`; save source: it calls `m_pxRestore->Step()` until the restore job reports done), so the recorded publication never reaches an unloaded or unrestored world. This is the hold of `Zenith2/Docs/Design/SCENE/Loading.md` → "Scene transitions" (a replay holds the publication tick until activation is complete).
4. The host never calls `Complete` for `ASYNC_SOURCE_WORLD_LOAD` (value 6 of `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"), neither for a load nor for a refusal or a failure: the replay source supplies every recorded publication at its recorded tick ([Z2-N-ARCH-783]), so each completion ID is still completed exactly once.
5. **Divergence.** Any of: a divergence a publication handler recorded (rows 2 and 6 of § "Publication payloads": a `WORLD_LOADED` publication for a record that is not awaiting it or that names another issue ID, a `WORLD_LOADED` publication of an ID the host does not hold, a failure publication for a record that already succeeded, a malformed payload); an intake recorded as taking the record that meets a record in flight whose own recorded outcome is `WORLD_LOADED` or no publication, or a save-source intake with a null save whose recorded outcome is `WORLD_LOADED` (§ "Intake in a replay"); a `WORLD_SWAP` whose old tick is not the old world's last executed tick at the swap; a swap that fails in the replay where the log holds its `WORLD_SWAP`, or a frame that ends at the tick a `WORLD_SWAP` of the old world names without that swap being `READY_TO_SWAP` in the next prologue; a `WORLD_BEGIN` seed different from `Zenith_DeriveWorldSeed`'s; or a recorded `WORLD_LOADED` whose replayed preparation or activation fails. A failure publication for an ID the host never held is not a divergence (row 5: SAVE's bridge failures and the untouched-record refusals of the direct call, the sink and the hand-off), and a recorded failure is not one either: the replay ends the record there (step 6 (b) of § "Prologue step 7 procedure"), or at once at intake for a save source (§ "Intake in a replay"). The host then, in this order: when a record is in flight, ends it through § "Failure application procedure" with `WORLD_TRANSITION_FAILURE_REPLAY_DIVERGED` (destroying an isolated world and releasing any reservation, exactly as a failure in that state does); logs `runtime.world: replay diverged at world <tag> tick <t>: <what>` at error severity, `<what>` naming the condition; counts `world.transition.replay_diverged`; calls `Zenith_Runtime::RecordTestAssertionFailure()` (MOD's single exit-5 path, the same call SAVE's replay verifier reaches, MB15; § "World host files and split with MOD"); and calls `Zenith_Runtime::RequestShutdown()`, so the frame loop stops when the current iteration returns and the run exits with code 5 ([Z2-N-ARCH-784] uses the same exit code for a refused replay). No tick of a diverged replay runs after the frame in which the divergence was found, so nothing downstream of it is compared.

The clock caps the frame's ticks so that the tick named by the next `WORLD_SWAP` is the frame's last (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue" step 2), so the swap procedure of the next prologue finds L equal to the recorded tick.

### Intake in a replay

A transition's outcome is recorded: success is the `WORLD_LOADED` publication of its completion ID, and every failure published after the ID was allocated is the `GENERIC_RECORDED` failure publication of that ID (§ "Publication payloads"), both written to the log at the old world's tick boundary like every asynchronous publication. A replay reads that outcome back with `FindPublication(old world, ASYNC_SOURCE_WORLD_LOAD, completion ID, …)` at intake — after the completion ID is known (the direct call's step 2, the sink slot's, or the ID a `Save.Load` hand-off passes) and before anything is decided — stores it in the record (`m_eRecordedOutcome`, `m_eRecordedCode`), and follows it. The codes split into intake-time codes, which the recording decided before preparation began, and running codes, which the recording reached while the transition ran. Three rules apply to every row of both tables below:

1. **Every intake decision follows the log, whatever the live record state.** The record's live state is not reproduced frame for frame: a `Save.Load` hand-off happens live in the prologue after SAVE's writer thread finishes, but in a replay only after the recorded `ASYNC_SOURCE_SAVE` publication; and a recorded failure ends a record at its recorded tick, not at the prologue where the recording's live run ended it. So no row compares the live record state with the recording: a recorded refusal refuses, and a recorded taking takes.
2. **A recorded taking that meets a record in flight.** An intake recorded as taking the record (every outcome except `SCENE_MISSING` and `IN_PROGRESS`) can meet, in the replay, a record still in flight that the recording had already ended (rule 1). When that record's own recorded outcome is a failure, the intake first ends it through § "Failure application procedure" with its stored `m_eRecordedCode` and no `Complete`, and then proceeds; that record's recorded payload then arrives, at its recorded tick, as row 5 of § "Publication payloads" (the key is no longer the record's) and changes nothing. When its recorded outcome is `WORLD_LOADED` or no publication, the recording could not have taken a second transition, and the meeting is a divergence (§ "Replay of a transition" step 5).
3. **A live failure ahead of the recorded one.** When the live run fails a record whose recorded outcome is a failure at a subsequent tick or no publication (a preparation that fails live sooner, or a replayed cancel request of the requester), steps 1–5 of § "Failure application procedure" run at once (preparation, activation or restore, isolated world, inputs, reservation), `m_bLiveEnded` becomes 1, and the record keeps its state, which prologue steps 3–5 skip, until its recorded ending: prologue step 6 (b) then runs the procedure's steps 6–8 with the recorded code (nothing is left to release), or an intake of rule 2 does, or, with no publication, the shutdown at the log's end cancels it. When the live code differs from the recorded one, the ending also logs at warning `runtime.world: replay: transition of world <W> ended live with <a>, recorded <f>` — not a divergence, because the old world's scripts observe the recorded payload in both runs and both runs released everything. So in a replay a record leaves the busy states at the tick the recording's publication names, never earlier.

| Recorded outcome | Replay intake | Live state that would have decided otherwise |
|---|---|---|
| failure `PEAK_REFUSED` or `OPERATION_TABLE` | after rule 2: no reservation, no `Issue`, no preparation, no `Complete`; the record goes from its idle state (`NONE`, `DONE`, `FAILED` or `CANCELLED`) to `FAILED` with the recorded code through § "Failure application procedure" (which then has nothing to release but the scene handle and, for a save source, the prepared save); a direct caller receives the `Zenith_RuntimeError` the recording returned (`RUNTIME_ERROR_TRANSITION_PEAK_REFUSED` or `RUNTIME_ERROR_TRANSITION_IN_PROGRESS`) | ignored: the governor is not asked and the operation table is not consulted |
| failure `SCENE_MISSING` or `IN_PROGRESS` (the direct call's step-3 refusal, the sink's and the hand-off's refusals) | the scene is not acquired (`SCENE_MISSING`), nothing is reserved or issued, the record is untouched whatever its live state, the slot's or hand-off's prepared save (if any) is destroyed, no `Complete`; a direct caller receives `RUNTIME_ERROR_TRANSITION_IN_PROGRESS` | the asset registry and the record's live state are not consulted (rule 1) |
| `WORLD_LOADED` (accepted) | after rule 2: reserve the peak; when the governor refuses where the recording succeeded, proceed unreserved, log `runtime.world: replay: transition peak refused live but accepted in the recording; proceeding unreserved` and count `world.transition.replay_unreserved`; `Issue` the operation (a full table here is a divergence: handles are released by the replayed requester's own code, so their count is reproduced, and a difference means the requester diverged); the activation hold of § "Replay of a transition" step 3 applies | the live reservation never changes the outcome |
| a running failure of a scene source: `PREPARE`, `WORLD_CREATE`, `SCHEDULE` (the new world's `Freeze`), `ACTIVATION` or `CANCELLED` | intake proceeds exactly as for `WORLD_LOADED` (after rule 2, the same unreserved fallback and the same `Issue`), without the activation hold; the record runs live with its normal budgets until the handler meets the recorded failure at its recorded tick (row 4 of § "Publication payloads"), and prologue step 7 then ends it through § "Failure application procedure" with that code (step 6 (b) there), with the isolated world destroyed and the reservation released; a live failure ahead of it follows rule 3 | the live outcome never reaches the old world's scripts: they observe the recorded payload at the recorded tick in both cases |
| failure `REPLAY_DIVERGED`, `CARRY_OVER`, 0 or above 13 | divergence: a recording never publishes `REPLAY_DIVERGED` (only a replay sets it, and a replay completes no ID) or `CARRY_OVER` (a swap failure follows the `WORLD_LOADED` publication of its ID and publishes nothing, § "Swap procedure" step 13) | — |
| no publication for the ID in the log (scene source) | after rule 2, proceed as for `WORLD_LOADED` but without the activation hold: the recording ended with this transition still in flight (its shutdown cancelled it after the last logged tick boundary, so the cancellation was never published), the clock stops the replay at the log's last tick ([Z2-N-ARCH-790]), and the shutdown at the log's end cancels the record again; a `WORLD_SWAP` of the old world that the replay reaches without a publication is a divergence (step 5) | — |

**Save sources.** In a replay, `Zenith_WorldHost_BeginTransitionFromSave` receives either the prepared save SAVE's replay form built from the log's restored session or, when the log holds no restored session after the requesting world's swap, a null save (§ "Failed save loads"). Its intake reads `FindPublication(W, ASYNC_SOURCE_WORLD_LOAD, ulCompletionID, …)` before it examines the save, and a recorded failure of any code ends the intake at once — no world, no restore job, no reservation, no `Complete` — because the old world's scripts observe only the recorded payload at its recorded tick (row 3's echo), so ending the record early changes nothing they see:

| Recorded outcome of the world-load ID | Save-source intake in a replay |
|---|---|
| failure `IN_PROGRESS` | the record untouched whatever its live state; the save (if any) destroyed |
| failure `PEAK_REFUSED`, `OPERATION_TABLE` or `SAVE_WORLD_COUNT`, a running code `WORLD_CREATE`, `SCHEDULE`, `RESTORE` or `CANCELLED`, or `PREPARE` (which only SAVE's bridge publishes, before a hand-off) | after rule 2, the record goes from its idle state through `RESERVING` to `FAILED` with the recorded code (`CANCELLED`: as a cancellation) through § "Failure application procedure", the save (if any) destroyed at its step 4; the prepared save's world count is not re-checked |
| `WORLD_LOADED` with the log's prepared save | as the `WORLD_LOADED` row above: the restore runs from the log's image, under the hold of § "Replay of a transition" step 3 |
| `WORLD_LOADED` with a null save | divergence: the recording swapped, so the log must hold the restored session |
| no publication for the ID in the log | the recording ended with the load in flight: after rule 2, the record goes from its idle state through `RESERVING` to `CANCELLED` at once through the procedure, without `Complete`; the replay stops at the log's last tick ([Z2-N-ARCH-790]) before anything could observe the difference |
| failure `REPLAY_DIVERGED`, `CARRY_OVER`, 0 or above 13 | divergence, as for a scene source |

Two intake decisions are not recorded and need no lookahead, because they are decided from the old world's asynchronous result table, whose allocation sequence and slot releases the replay reproduces: the direct call's `ASYNC_TABLE_ERROR_FULL` at step 2 (before any ID exists) and the sink's drop. The governor's answer is the only intake input that depends on the machine and not on the log, which is why the accepted row alone overrides it; every other recorded code is followed because it is the outcome the old world's scripts observed.

### Transition failure and cancellation

| Ending | Released | Reported |
|---|---|---|
| refused at intake | the reservation (if taken), the scene handle | direct: the `Zenith_RuntimeError`; both: the failure publication, `world.transition.refused`, a log line |
| `PREPARING` failed or cancelled | the preparation operation (cancelled: SCENE releases its partial state on its executor, [Z2-N-CONTRACT-335]), the scene handle, the reservation | `CompleteFailed(code)` or `CompleteCancelled()`; the failure publication |
| `ACTIVATING` failed or cancelled | the isolated world through `Zenith_WorldHost_DestroyWorld` (bulk destruction releases every entity's handles), the prepared scene or the restore job and prepared save, the reservation | as above (`Zenith2/Docs/Design/SCENE/Loading.md` → "Cancellation", the isolated-world row) |
| swap failed | § "Swap procedure" step 13 | `CompleteFailed(SCHEDULE)` or `CompleteFailed(CARRY_OVER)` |

A cancelled or failed transition leaves the old world exactly as it was: nothing of it is touched before the swap's step 6.

### Failure application procedure

`void Zenith_WorldHost_FailTransition(Zenith_RuntimeState& xState, Zenith_WorldTransitionFailure eFailure, bool bCancelled)` (declared in `Internal/Zenith_WorldTransition.h`, defined in `Internal/Zenith_WorldTransition.cpp`; `MAIN_THREAD`, no frame running; cannot fail) ends the record R in its current state s with code f — or as a cancellation, f = `WORLD_TRANSITION_FAILURE_CANCELLED`. It is the one path for every failure of the state table before the swap: a refusal inside intake (s = `RESERVING`), a failed preparation, creation, freeze, activation or restore, a cancel request, the lifecycle shutdown, a replay's recorded failure (prologue step 6 (b), or at intake under rule 2 or the save-source rows of § "Intake in a replay") and a replay divergence (step 5 of § "Replay of a transition", f = `REPLAY_DIVERGED`). In this order:

1. **Preparation** (s = `PREPARING`): `m_xPrepare.RequestCancel()`, then reset `m_xPrepare` (the handle detaches; SCENE releases its partial state on its executor, [Z2-N-CONTRACT-335] and `Zenith2/Docs/Design/SCENE/Loading.md` → "Cancellation").
2. **Activation or restore** (s = `ACTIVATING` or `AWAITING_PUBLICATION`): scene source: `m_xActivation.RequestCancel()` and reset it (SCENE's isolated-world row of "Cancellation" drops the pending activation and the prepared scene it owns); save source: `m_pxRestore.Reset()` (the restore job references the world, so it goes first).
3. **Isolated world** (`m_uNewTag != 0`): lifecycle → `DESTROYING`; `Zenith_WorldHost_DestroyWorld(m_uNewTag)` (§ "Teardown order algorithm"; steps 1 and 2 are no-ops for a world that never went live; the module detach of step `MODULES` undoes SAVE's `Zenith_RuntimeSave_AttachWorld` binding of a save-source world; a table that still has results in flight — a save-source world's re-issued restored requests — is retired at step `ASYNC_RESULTS` with its sinks, never destroyed under a running producer, § "Retired world asynchronous results"); `m_uNewTag = 0`.
4. **Inputs**: `m_pxSave.Reset()` (save source); reset `m_xScene` (scene source).
5. **Reservation** (`m_bReserved`): `Zenith_TransitionReservation` released; `m_bReserved = 0`.
6. **Operation**: `m_xTicket.CompleteCancelled()` when cancelled, else `m_xTicket.CompleteFailed(f)`; a direct requester's handle reports `OPERATION_STATUS_CANCELLED` or `OPERATION_STATUS_FAILED` with `GetFailureCode() == f` from here on.
7. **State**: R → `FAILED` (or `CANCELLED`), `m_eFailure = f`; flow → `RUNNING`; counter `world.transitions.failed` (or `world.transitions.cancelled`; an intake-time refusal counts `world.transition.refused` instead).
8. **Log**: `runtime.world: transition of world <O> to <scene> failed in <state>: <failure>` (or `… cancelled in <state>`), `<scene>` the scene's name or `save slot <n>`; a replay appends ` (recorded at tick <t>)` when the failure came from the log.
9. **Publication**: outside a replay, when `m_bCompleted == 0`: `Complete(m_ulCompletionID, PUBLICATION_KIND_GENERIC_RECORDED, the failure payload of f)` into O's table, then `m_bCompleted = 1`. In a replay nothing is completed: the log's own publication (or, at the log's end, none) is the outcome.

The order stops every piece of work that references the isolated world before the world is destroyed, destroys the world before releasing the reservation that covers its memory, completes the operation only when everything is released (so a requester that sees `FAILED` can issue the next transition at once), and publishes last. What is observable afterwards: the old world O unchanged (its tick hashes equal a run without the request, `Test_WorldTransition_CancelBeforePublication`), the operation's status and code, the counter and the log line, and in O's scripts the failure payload at O's next tick boundary (live) or at the recorded tick (replay). A swap failure is not this procedure: its path is § "Swap procedure" step 13, because its ID was already published as `WORLD_LOADED`.

### Failed save loads

A script `Save.Load` that fails ends in one of four places; in none of them is the requesting world W touched. W's scripts observe `Zenith.SaveCompleted` (`Operation` 4) on the `ASYNC_SOURCE_SAVE` ID and, whenever a world-load ID was allocated (every row but the first), that ID's failure payload, both at W's tick boundaries in completion-ID order (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Script loads and the world host"):

| Where it fails | Who completes the world-load ID | GAME's state | What W's scripts see | `--replay` run |
|---|---|---|---|---|
| SAVE's intake cannot allocate the linked world-load ID (a transition or another `Save.Load` of W holds W's single world-load ID) | nobody: no world-load ID exists (`SAVE_ERROR_TRANSITION_BUSY`) | nothing: GAME is never called and holds nothing | `Zenith.SaveCompleted` with `SAVE_ERROR_TRANSITION_BUSY` only | the same: SAVE's recorded publication is all there is; GAME is not called |
| before the hand-off: SAVE's intake refuses the load (`SAVE_ERROR_SHUTTING_DOWN`, `SAVE_ERROR_QUEUE_FULL`), `BeginShutdown` cancels it while queued, or the load (read, verify, migrate) fails | SAVE, with `WORLD_TRANSITION_FAILURE_PREPARE`: `CompleteLinkedLoadRefused` on the merge node at intake or on the main thread at `BeginShutdown`, or the bridge in the prologue after a failed load | nothing: the record is untouched (the ID never reaches the host) and the handler ignores the publication (row 5 of § "Publication payloads") | `Zenith.SaveCompleted` with the error, then the `PREPARE` failure payload | for a recorded `ASYNC_SOURCE_SAVE` failure the bridge's replay form calls nothing ("In replay mode the writer reads nothing and completes nothing", same heading); the recorded `PREPARE` payload arrives at its recorded tick and meets row 5: nothing, no divergence |
| the hand-off: `Zenith_WorldHost_BeginTransitionFromSave` refuses (`IN_PROGRESS`, `PEAK_REFUSED`, `OPERATION_TABLE`) | the host, before returning, with that code; the prepared save is destroyed | `IN_PROGRESS`: the record untouched; the other two: § "Failure application procedure" from `RESERVING` (the save is destroyed at its step 4) | `Zenith.SaveCompleted` with Result 0, then the failure payload | the recorded `ASYNC_SOURCE_SAVE` publication carries Result 0 and the log holds no session after W's swap, so SAVE's replay form calls `Zenith_WorldHost_BeginTransitionFromSave(state, W, null, linkedID)` (GA11 (g)); the save-source intake of § "Intake in a replay" ends at once with the recorded code: the record untouched (`IN_PROGRESS`) or `FAILED` from `RESERVING`, no restore job, no reservation, no `Complete`; the recorded payload arrives at its recorded tick as row 3's echo (row 5 for `IN_PROGRESS`) |
| after the hand-off: `SAVE_WORLD_COUNT` (in intake), `WORLD_CREATE` or `SCHEDULE` (creating or freezing the isolated world), `RESTORE` (the restore job fails, SAVE's "Restore job state machine": "the caller destroys the world", or is not done within `runtime.startupLoadTimeoutMs`, SAVE's row SV25), or a cancel | the host, at step 9 of § "Failure application procedure" | § "Failure application procedure": the restore job, then the isolated world (with its SAVE binding; a table with re-issued requests in flight retired), then the prepared save, then the reservation, the operation and the record | `Zenith.SaveCompleted` with Result 0, then the failure payload | the same null-save call; the recorded running code ends the record `FAILED` (`CANCELLED`: as a cancellation) at intake from `RESERVING`, with no isolated world created, no restore job, no reservation and no `Complete`; the recorded payload arrives at its recorded tick as row 3's echo |

No `WORLD_SWAP`, `WORLD_BEGIN` or restored-world checkpoint is written for a failed load, so SAVE's replay form has no restored session to prepare a save from for a load that failed at or after the hand-off: that is why the intake accepts a null save in a replay and follows the recorded outcome alone. Every row therefore replays with no divergence, no restore job and exit code 0 (`Test_WorldTransition_ReplayIntakeFollowsLog` cases (c), (g) and (h)). SAVE's sentence, reworded as GA11 (f) asked, agrees: "when the load failed before the hand-off, GAME's handler ignores the bridge-completed ID (the host never held it); when it failed after the hand-off, GAME's handler ends its record from the recorded failure payload" — for the third and fourth rows the save-source intake ends the record with the recorded code and the recorded payload is its echo, and for the first two GAME holds no record and the handler does nothing (same heading).

<!-- assumption: pending SAVE -->
For the third and fourth rows GAME assumes the replay form of SAVE's bridge, meeting a recorded Result 0 of a script LOAD with no session after the requesting world's swap (`GetSessionAfterSwap` returns null), calls `Zenith_WorldHost_BeginTransitionFromSave(state, W, null, linkedID)` with a null `Zenith_Owned<Zenith_PreparedSave>` (cross-area row GA11 (g)); SAVE's current text calls it only with the prepared save of the restored session. Should SAVE adopt the equivalent alternative — calling nothing in that case — the linked ID never becomes the record's, its recorded payload meets row 5 of § "Publication payloads", and the outcome W's scripts observe is identical (the recorded payload at its recorded tick, no world created, exit code 0); only the record's diagnostic state differs (untouched instead of `FAILED`). GAME implements the null-save path, and cases (g) and (h) drive it by calling `Zenith_WorldHost_BeginTransitionFromSave` with a null save exactly as the assumed replay form does.
<!-- /assumption -->

## Carry-over

The root list of a swap, at most `uWORLD_MAX_CARRY_OVER_ROOTS` (256) persistent IDs:

1. The request's IDs in request order.
2. Then every entity of O carrying `Zenith.Runtime.CarryOver` with `Enabled != 0`, in ascending stable simulation ID, through `Zenith_WorldInspector`; one without a persistent ID cannot be addressed across worlds ([Z2-N-ARCH-762]) and is skipped, counted `world.carryover.no_persistent_id` and logged once per entity.
3. Remove duplicates (the first occurrence stays) and every ID whose entity has an ancestor in the list (its subtree is captured with that ancestor, `Zenith2/Docs/Design/SCENE/Loading.md` → "Carry-over").
4. IDs beyond 256 are dropped and counted (`world.carryover.dropped`); an ID that names no live entity is skipped and counted by SCENE (`scene.carryover.missing`).
5. **Across a save profile change, nothing.** When the old world's SAVE binding profile (`Zenith_SaveSystem::GetBindingProfile(O)`) differs from SAVE's active profile at the swap, the host collects no roots at all — neither the requested IDs nor the `Zenith.Runtime.CarryOver` entities — counts `world.carryover.profile_changed`, logs `runtime.world: world <O>: save profile changed; nothing carried over`, and the swap otherwise proceeds, so the new world, bound under the new profile, holds nothing of the previous account's world (ONLINE's isolation rule I11, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Cross-account isolation"; cross-area row ON23 (a)).

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_WorldState.h
/// @contract value Zenith_CarryOverComponent
/// Ownership: Stored as component "Zenith.Runtime.CarryOver".
/// Thread affinity: IMMUTABLE as a value; members non-const: an ECS component the store relocates and systems write
///   property by property; property 1 has authority CommandsOnly.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_CarryOverComponent
{
	uint8 m_bEnabled = 1;          // 1 Enabled
	uint8 m_auReserved[3] = {};
	static const Zenith_TypeID xTYPE_ID;   // "Zenith.Runtime.CarryOver"
};
```

Values survive bit for bit: capture serializes every serialized field and activation deserializes it into new handles ([Z2-N-ARCH-567]). Gameplay state that counts time keeps its meaning because `ZenithGameplay` stores absolute times as gameplay ticks, the world's tick index plus the world state's continuity tick base, which the swap sets to B_O + L (§ "Swap procedure" step 5): a timer due at gameplay tick g in O is due at g in N, which is the same number of ticks after the swap (`Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay time"). Nothing is rebased, so no field is ever missed by a rebase list.

## Retired world asynchronous results

[Z2-N-ARCH-558] discards results addressed to a swapped-out world when they complete. The producers (streaming cell preparations and additive loads through SCENE, path requests, physics queries) hold a completion ID and reach O's `Zenith_AsyncResultTable` through MOD's runtime sinks, and a module's `DetachWorld` cancels but cannot always wait for a background job that is running, so the table — and every sink a producer may still call — must outlive O. Every asynchronous result of a world reaches it through that table ([Z2-N-ARCH-792]), so the table is the one place where every discarded result of a swapped-out world is counted.

<!-- assumption: pending SCRIPT -->
GAME assumes these `Zenith_ScriptWorld` and `Zenith_AsyncResultTable` members (cross-area row GA10 (a)):

```cpp
// ZenithScript/Zenith_ScriptWorld.h
Zenith_Owned<Zenith_AsyncResultTable> DetachAsyncResults();   // MAIN_THREAD, out of tick; the world keeps no table
// ZenithScript/Zenith_ScriptAsync.h
void Retire();                                    // MAIN_THREAD: discards and counts every completed, unpublished record;
                                                           // from now on Complete (ANY_THREAD, as before) releases the slot, discards, counts
uint32 GetInFlightCount() const;   // ANY_THREAD: allocated and not yet completed
uint64 GetDiscardedCount() const;  // ANY_THREAD: results discarded by Retire and since Retire
```
<!-- /assumption -->

**The producer obligation.** Release rests on one rule, core text since cross-doc item X255 ([Z2-N-SCRIPT-931] in `Zenith2/Docs/Scripting.md` → "Tick-boundary publication", and the `Zenith_AsyncResultTable` contract of `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript asynchronous results and host interfaces"): every completion ID allocated from a world's `Zenith_AsyncResultTable` is completed exactly once by the producer that allocated it or took ownership of it — including work that the producer's `DetachWorld` cancels, which it completes with its own failure or cancellation payload (`Complete` is `ANY_THREAD`, so a background job completes on its own thread when it notices the cancel) — and a producer never counts a discarded result itself: the table, live or retired, discards and counts it, and the world host alone publishes `world.async_discarded` ([Z2-N-ARCH-558]). The world host keeps the rule for `ASYNC_SOURCE_WORLD_LOAD` (§ "Publication payloads"); SCENE's additive loads and STREAM's cell preparations complete into the table (cross-area row GA2 (d)); PHYS keeps it for its deferred queries, having adopted cross-doc item X265: `Zenith2/Docs/Design/PHYS/Simulation.md` → "Deferred query state machine" completes every `INBOX` entry in `DetachWorld` through the module publication sink, before the world state is destroyed, with a cancelled zero-hit result, counts nothing under `world.async_discarded`, and proves it with `Test_PhysicsIntegration_SwapCompletesInboxQueries` (cross-area row GA19).

**Sinks bound at creation and retired with the table.** A producer's completion reaches the table only through a runtime sink, so the table's lifetime rule needs the sinks' (cross-doc item X274; cross-area rows GA5 (k) and GA2 (d)). Items (a) and (b) are MOD's design since X274 (3) (`Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime", the paragraph "Sink binding and lifetime"), and the table's retirement is core text since X274 (1) and (2) ([Z2-N-ARCH-558], [Z2-N-ARCH-560], [Z2-N-SCRIPT-931]): (a) every runtime sink over a world's table — MOD's `Zenith_RuntimeScenePublicationSink` (`Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime"), the single module publication sink `Zenith_ModulePublicationSink` of the Q76 (a) ruling that `Zenith_WorldAttachContext::GetPublicationSink()` returns, and SAVE's sink — binds the `Zenith_AsyncResultTable*` once, at the world's creation (§ "World creation" step 4, before any module attaches), and never re-resolves `Zenith_ScriptWorld::GetAsyncResults()`; because moving a `Zenith_Owned` into a retired entry does not move its pointee, a sink bound at creation keeps reaching the same table after the detach; (b) MOD's world host record holds the scene publication sink and the module publication sink as `Zenith_Owned` and destroys neither itself: the world host takes both into the world's `Zenith_RetiredResults` entry — at swap step 6 for a swapped-out world, at teardown step `ASYNC_RESULTS` for any other — and destroys them with the table at its release, so a background job that parks after its world was destroyed (a SCENE or STREAM preparation continuation through the scene sink, an AI path job or PHYS's `DetachWorld` park through the module sink) reaches the retired table, never freed memory; SAVE's sink is not taken, because SAVE unbinds the world in its own `DetachWorld` and no SAVE job completes into a destroyed world's table.

<!-- assumption: pending SCENE (X274 (4)) -->
GAME assumes (c): SCENE's "Publication service" states that a `Park` after the world's destruction reaches the retired table through that sink (`Zenith2/Docs/Design/SCENE/Loading.md` → "Publication service").
<!-- /assumption -->

Swap step 6 calls `DetachAsyncResults()` on O's script world, then `Retire()` on the table, takes O's scene and module publication sinks from MOD's world host record, and appends `{ table, scene sink, module sink, the in-flight count at the detach, O's tag }` to `m_axRetired` while fewer than `uWORLD_MAX_RETIRED_RESULT_TABLES` (4) entries are held; a fifth and every subsequent held table goes to `m_xRetiredOverflow` (`PushBack`, root heap; an allocation failure there is the root heap's out-of-memory path, as for every root-heap allocation, `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"), with the warning `runtime.world: world <tag>: retired result table kept in overflow; <n> retired tables still hold in-flight results` and counter `world.async_retired_overflow` + 1. The overflow never stops the run and never releases a table early: releasing a table a producer still references would be a use-after-free, so a table stays held for as long as its in-flight count is above 0. Under the producer obligation the overflow means four earlier swaps each still had a background job running when the fifth swap came, which a correct producer reaches only with jobs that outlive four transitions; without the obligation (a producer that never completes an ID) the tables are held until shutdown, are reported there, and cost only their own memory.

**Tables of worlds that are not swapped out.** The teardown of § "Teardown order algorithm" detaches every other world's table at step `SCRIPTS` (`DetachAsyncResults()` into the world record's `m_xDetached`) instead of destroying it, so the modules' `DetachWorld` at step `MODULES` — PHYS's `INBOX` parks among them — still complete into a live table; step `ASYNC_RESULTS`, after the modules, the scene world state and the input resources, then disposes of it:

| World being destroyed | Step `ASYNC_RESULTS` |
|---|---|
| a swapped-out world (§ "Swap procedure" step 7) | nothing: swap step 6 already retired its table and sinks |
| a live world destroyed by MOD's shutdown step 2 (its lifecycle was `SHUTTING_DOWN`; MOD's step 1 ran every job to completion) | destroy the table and both sinks at once; no result is counted under `world.async_discarded` (shutdown discards nothing it must report); a `GetInFlightCount()` above 0 logs `runtime.world: world <tag>: result table destroyed at shutdown with <n> results never completed` at error severity and counts `world.async_never_completed` |
| any other world: an isolated world destroyed by a failed or cancelled transition (including the cancel of the lifecycle shutdown's step 2, before MOD drains the jobs), a startup unwind | `Retire()`, take the two sinks, and append the entry to the retired list exactly as swap step 6 does; when `GetInFlightCount()` is 0 the entry is released at once instead, as `Zenith_WorldHost_ReleaseRetiredResults` releases one (its discarded count added to `world.async_discarded`, the release line logged when the count is above 0) |

So a save-source world restored with re-issued requests in flight (PHYS's `GetReissuedSourceKinds`, SAVE's "Restore pipeline") whose restore or swap then fails is retired like a swapped-out world while those jobs run, and the same overflow and release rules apply to it.

`Zenith_WorldHost_ReleaseRetiredResults(xState, bShutdown)` walks `m_axRetired` and then `m_xRetiredOverflow` in insertion order and releases each entry whose table's `GetInFlightCount()` is 0 (an overflow entry is removed with `RemoveAt`, keeping the order; a freed inline slot is refilled from the front of the overflow list, so inline entries stay the oldest four); with `bShutdown` (after MOD's shutdown step 2, when MOD's step 1 has cancelled every operation and run jobs until each completed) it releases every entry. A release destroys the table, then the scene sink, then the module sink. At each release it adds the table's `GetDiscardedCount()` to the counter `world.async_discarded` and logs `runtime.world: world <tag>: <n> asynchronous results discarded after the swap`. A table released at shutdown whose `GetInFlightCount()` is still above 0 holds IDs no producer ever completed — a breach of the producer obligation, since every job has finished by then — and is additionally logged at error severity `runtime.world: world <tag>: retired result table released at shutdown with <n> results never completed` and counted `world.async_never_completed` (CUMULATIVE); no job can still reference it, so releasing it is safe.

**One name, one publisher.** `world.async_discarded` (CUMULATIVE) is published by the world host alone, at each retired table's release, and its increment for a retired world is that world's final count: every result completed but unpublished at the detach, plus every result completing after it — STREAM's cell preparations and SCENE's additive loads included, because SCENE completes them into the world's table (`ASYNC_SOURCE_SCENE_LOAD`, value 8 of `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"), where the retired table discards and counts them. ECS does not publish the counter at world destruction and SCENE does not count its discarded preparations under it (cross-area rows GA1 (d) and GA2 (d) withdraw both); there is no per-tag counter name, and the log line names the tag. A world shut down by MOD's shutdown step 2 never retires its table (shutdown discards nothing it must report), so swaps and destroyed isolated worlds are what move the counter.

## Pause arbitration

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_WorldControl.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithRuntime/Zenith_Module.h"          // Zenith_ModuleError, for the friend declaration
#include <atomic>

struct Zenith_RuntimeState;       // ZenithRuntime internal; named only by the two friend declarations below
struct Zenith_WorldCreateDesc;

/// @contract enum Zenith_WorldPauseReason
/// Ownership: Plain enumeration of bits.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; bits 4–31 are reserved, ignored when set and counted (world.pause.reserved_bits).
/// Version: since Z2 Stage 3; bit positions never move.
/// @endcontract
enum Zenith_WorldPauseReason : uint32
{
	WORLD_PAUSE_REASON_GAMEPLAY = 1u << 0,   // a script's timed pause (hit-stop, a scripted freeze); cleared by its deadline
	WORLD_PAUSE_REASON_MENU = 1u << 1,       // a pause menu; cleared by the UI that opened it
	WORLD_PAUSE_REASON_EDITOR = 1u << 2,     // the editor's pause and step controls
	WORLD_PAUSE_REASON_DEBUGGER = 1u << 3,   // a debugger break
};

/// @contract type Zenith_WorldControl
/// Ownership: One per world, owned by the world host's per-world record; borrowed by modules through
///   Zenith_WorldAttachContext::GetWorldControl() for the world's lifetime (cross-area row GA5 (a)).
/// Thread affinity: ANY_THREAD for every member: the requested mask, the pending gameplay duration and the applied mask are
///   atomic words (fetch_or, fetch_and and the duration's compare-exchange maximum with memory_order_acq_rel; loads acquire;
///   the applied mask is stored release by prologue step 7 alone). The gameplay deadline is a private main-thread field
///   that only prologue step 7 reads or writes.
/// Invalidation: Invalid after the world's DetachWorld; a module drops its pointer there.
/// Units: Milliseconds for a requested gameplay pause; the deadline is microseconds on the platform's monotonic clock,
///   computed on the main thread in prologue step 7, never by simulation code ([Z2-N-ARCH-885]). A step count is a number of
///   fixed ticks (RequestStep: MAIN_THREAD, between frames).
/// Errors: None; RequestStep with 0 or above 600 ticks does nothing (logged, counted world.pause.step_refused).
/// Reentrancy: NOT_APPLICABLE. It invokes no callback.
/// Version: since Z2 Stage 3; RequestStep since Z2 Stage 3 (the single-step ticket).
/// @endcontract
class Zenith_WorldControl
{
public:
	Zenith_WorldControl() = default;                // default-constructed in place inside the world host record; the tag is set by Zenith_WorldHost_CreateGameRecord
	void AddPauseReasons(uint32 uReasons);          // fetch_or
	void RemovePauseReasons(uint32 uReasons);       // fetch_and(~)
	void PauseForGameplay(uint32 uMilliseconds);   // requests GAMEPLAY for 1 … 60,000 ms from the next prologue (the larger of two pending wins)
	uint32 GetRequestedReasons() const;
	uint32 GetAppliedReasons() const;  // as of the last prologue step 7
	uint16 GetWorldTag() const;
	void RequestStep(uint32 uTicks);               // MAIN_THREAD: run uTicks (1 … 600) fixed ticks, then pause again (§ "Single step")
	uint32 GetRemainingStepTicks() const;   // MAIN_THREAD
private:
	friend void Zenith_WorldHost_ApplyPauseMasks(Zenith_RuntimeState& xState);          // the deadline and the step count
	friend Zenith_Result<void, Zenith_ModuleError> Zenith_WorldHost_CreateGameRecord(Zenith_RuntimeState& xState, uint16 uTag,
		const Zenith_WorldCreateDesc& xDesc);                                         // sets m_uWorldTag once
	std::atomic<uint32> m_uRequested{ 0 };
	std::atomic<uint32> m_uPendingGameplayMs{ 0 };            // a requested duration not yet turned into a deadline
	std::atomic<uint32> m_uApplied{ 0 };                      // stored by prologue step 7 only
	uint64 m_ulGameplayDeadlineUs = 0;                        // main thread, prologue step 7 only
	uint32 m_uRequestedStepTicks = 0;                         // main thread: written by RequestStep, consumed by prologue step 7
	uint32 m_uRemainingStepTicks = 0;                         // main thread: the step in progress
	uint16 m_uWorldTag = 0;
};
```

`Zenith_WorldHost_ApplyPauseMasks`, first in prologue step 7, for each `LIVE` world, with now = the platform's monotonic time in microseconds read once on the main thread: exchange `m_uPendingGameplayMs` with 0, and when it held d > 0 set the deadline to max(the current deadline, now + d × 1,000); if GAMEPLAY is requested and now is past the deadline, clear GAMEPLAY and the deadline; let r = the requested mask without reserved bits; if r ≠ 0 and the applied mask was 0, `Zenith_WorldSimulation::SetPaused(true)`; if r = 0 and the applied mask was not, `SetPaused(false)`; store r as applied; publish the gauge `world.pause.reasons.<tag>`. The single-step handling of § "Single step" runs inside the same pass, after the mask is computed and before it is stored. ECS applies `SetPaused` at the world's next tick boundary (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldSimulation header"), and a paused world's tick-boundary node runs none of its steps, so its tick index does not advance and no node body of its tick runs.

Pause cannot affect determinism: a paused tick does not exist, so the sequence of ticks, their inputs and their publications is the same whatever the pause pattern, and a replay needs no pause record ([Z2-N-ARCH-783]), with one exception: a `MENU` clear. UI's `resumeWorld` publishes a recorded `UI_EVENT_KIND_WORLD_RESUMED` record from a publication slot it reserves, and clears `MENU` only when the record was accepted, so every live clear has a log record; under `--replay` UI ignores the live action and clears `MENU` in prologue step 1 when the sink's recorded publications for the world's next tick hold that record, before `Zenith_WorldHost_ApplyPauseMasks` runs in step 7, so a replayed pause menu resumes at the recorded tick (`Zenith2/Docs/Design/UI/Interaction.md` → "Pause menus"; cross-area row UI22). GAME changes nothing for it: the host only applies the mask it finds. A paused old world defers its own swap, because its load completion publishes only at a tick boundary that runs (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition").

| Reason | Set by | Cleared by |
|---|---|---|
| GAMEPLAY | `Game.RequestPause(ctx, "Gameplay", seconds)` through the `Runtime.Pause` command (§ "Game command kinds") | its deadline, at the first prologue past it |
| MENU | a script: `Game.RequestPause(ctx, "Menu")` (the pause menu's open action), through the `Runtime.Pause` command | the UI module alone: its `resumeWorld` action calls `Zenith_WorldControl::RemovePauseReasons(WORLD_PAUSE_REASON_MENU)` from UI's C++ (a paused world runs no script, so no script can clear it); UI never sets a pause reason itself (`Zenith2/Docs/Design/UI/Interaction.md`; cross-area row GA13 (a)) |
| EDITOR | the editor's play controls | the editor |
| DEBUGGER | the script debugger's break (SCRIPT's DAP adapter) and DEVT's frame stepping | the same |

### Single step

`Zenith_WorldControl::RequestStep(n)` (cross-area rows ED10, EdB4; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Pause and single step") asks for exactly n fixed ticks of a paused world, then a pause again. It is `MAIN_THREAD` between frames (the editor's and DEVT's controls run there); n outside 1 … 600 does nothing, logs `runtime.world: world <tag>: step of <n> ticks refused` and counts `world.pause.step_refused`. A request replaces any step not yet started. Prologue step 7, inside `Zenith_WorldHost_ApplyPauseMasks`, for each `LIVE` world:

1. When `m_uRequestedStepTicks` > 0: if the requested mask r without reserved bits holds any reason other than `EDITOR` and `DEBUGGER`, or no reason at all, the request is refused (same log line and counter) and cleared; otherwise `m_uRemainingStepTicks` = the request, the request is cleared, and the host calls `Zenith_WorldSimulation::RunTicksThenPause(n)` (ECS; it un-pauses the world for exactly n ticks, decrements at each tick boundary and applies `SetPaused(true)` at the boundary after the n-th).
2. While `m_uRemainingStepTicks` > 0: a `DEBUGGER` reason added since the step started cancels it — the remaining count is cleared, ECS's run is cancelled with `RunTicksThenPause(0)`, and the world stays paused under both reasons; otherwise the frame runs `min(remaining, max(1, T))` ticks of the world, T being the frame's unpaused tick count (the clock's T capped by `sim.maxCatchUpTicks`), and the remaining count drops by that number. The accumulator is neither consumed nor filled by a step, and restarts from 0 at the next Resume, so no catch-up burst follows.
3. The applied mask stays the requested mask throughout (`EDITOR` and/or `DEBUGGER`): a step does not remove a reason, so `GetAppliedReasons` keeps reporting the pause the editor shows.

A step's ticks are ordinary ticks with the world's recorded inputs, so a step changes no determinism (Z2-D-019) and a replay needs no step record: a replay of a recording made in the editor runs the same ticks unpaused.

## World state resource

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_WorldState.h
/// @contract enum Zenith_WorldOrigin
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; values never move.
/// @endcontract
enum Zenith_WorldOrigin : uint8
{
	WORLD_ORIGIN_STARTUP = 1,       // startup step 16
	WORLD_ORIGIN_TRANSITION = 2,    // a scene transition
	WORLD_ORIGIN_SAVE_LOAD = 3,     // restored from a save
	WORLD_ORIGIN_EDITOR_PLAY = 4,   // EDIT's play world
};

/// @contract value Zenith_WorldStateResource
/// Ownership: Registered as the world resource "Zenith.Runtime.WorldState" (owning phase PHASE_INPUT); the object is the
///   world host's per-world record member; readers borrow it through Zenith_SystemContext::GetResource.
/// Thread affinity: Written by the world host on the main thread out of tick only (world creation and the swap), so every
///   tick reads constant values; read from any node that declares ReadResource of it.
/// Invalidation: None while the world lives.
/// Units: Ticks for the tick fields.
/// Errors: None.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_WorldStateResource                 // 64 bytes
{
	uint64 m_ulFirstTick = 1;                   // 1 FirstTick: the tick index of the world's first tick
	uint64 m_ulWorldSeed = 0;                   // 2 WorldSeed
	uint64 m_ulContinuityTickBase = 0;          // 3 ContinuityTickBase: gameplay tick = tick index + this (serialized)
	Zenith_PersistentID m_xScene;                // 4 Scene: the scene the world was created from (16 bytes)
	Zenith_PersistentID m_xPreviousScene;        // 5 PreviousScene: the scene of the world it replaced; null for none
	uint32 m_uSessionOrdinal = 0;               // 6 SessionOrdinal: the WORLD_BEGIN ordinal
	uint16 m_uCarriedRoots = 0;                 // 7 CarriedRoots: roots carried into this world
	uint8 m_eOrigin = WORLD_ORIGIN_STARTUP;     // 8 Origin
	uint8 m_uReserved = 0;
	static const Zenith_TypeID xTYPE_ID;         // "Zenith.Runtime.WorldState"
};
static_assert(sizeof(Zenith_WorldStateResource) == 64);
```

Serialization (SAVE's rows SV7 (e) and SV21): `Zenith_RegisterGameFlowTypes` registers property 3 `ContinuityTickBase` with `PROPERTY_FLAG_SERIALIZED` and properties 1, 2 and 4–8 with `PROPERTY_FLAG_NONE` (transient: the world host sets them at creation and at the swap). SAVE therefore saves the resource in the per-world RESOURCES section and restores the base out of tick before the participants and the first tick (`Zenith2/Docs/Design/SAVE/Formats.md` → "Resources section"); for a save-source swap the host writes the transient fields and leaves the restored base untouched (§ "Swap procedure" step 5). Every other field is rebuilt by the host, so saving it would only duplicate what the swap writes.

Every field is a function of the request history and the log, never of real time, so a run and its replay read the same values in every tick (Z2-D-019). The startup world gets origin `STARTUP`, first tick 1, base 0, ordinal 0 and the startup scene's ID at step 16 (before step 17 loads the scene, so the scene ID is the configured `content.startupScene`'s asset ID); EDIT sets origin `EDITOR_PLAY` for a play world through the same creation desc. The resource is the only path by which simulation learns about its own world lifecycle: `Gameplay.WorldEvents` emits `Zenith.Gameplay.WorldStarted` from it (`Zenith2/Docs/Design/GAME/Framework.md` → "The world events system"), and `Game.GetWorldState(ctx)` returns it to scripts.

## Game command kinds

Registered by `Zenith_RegisterGameFlowCommandKinds(Zenith_CommandKindTable&, Zenith_GameFlowServices&)` at startup step 15, in the runtime's own name space (`Runtime.`), each with its reflected payload type registered at step 6:

| Kind | Payload (bytes) | Allowed phases | Playback (at the sync point, in merge order) |
|---|---|---|---|
| `Runtime.Pause` | `{ uint8 op; uint8 reason; uint16 0; uint32 milliseconds; }` (8) — op 1 add, 2 remove; reason 1 GAMEPLAY, 2 MENU | 1–10 | GAMEPLAY add: `PauseForGameplay(ms)` (ms 1 … 60,000, else rejected), which only stores the duration — the deadline is computed by the next prologue step 7 on the main thread; MENU add or remove: `AddPauseReasons`/`RemovePauseReasons(MENU)`; GAMEPLAY remove: `RemovePauseReasons(GAMEPLAY)`; any other op or reason is rejected (`world.commands.rejected`) |
| `Runtime.Quit` | `{ uint32 0; }` (4) | 1–10 | `Zenith_WorldHost_RequestShutdownFromWorld(tag)`: compare-exchange of 1 + tag into `m_uShutdownRequestedByWorld`; MOD's frame loop treats a non-zero value as `RequestShutdown` after the current frame (GA5 (d)); log `runtime.game: world <tag> requested quit at tick <t>` |
| `Runtime.ReportTestFailure` | `{ uint16 length; char text[198]; }` (200) | 1–10 | `Zenith_WorldHost_RecordTestFailure(tag)`: `Zenith_Runtime::RecordTestAssertionFailure()` (MOD's single exit-5 path, MB15; GAME keeps no failure count of its own) and the diagnostic counter `runtime.game.test_failures` + 1; log `runtime.game: TEST FAILED (world <tag>, tick <t>): <text>` at error severity; MOD returns exit code 5 at the end of the run because an automated test assertion was recorded (`Zenith2/Docs/Design/MOD.md` → "Frame loop driver"; GA5 (d)) |

The playbacks touch only atomics and the log, so they are legal on any world's sync-point node concurrently. A quit takes effect after the frame in which its playback ran, so every tick that frame started drains ([Z2-N-ARCH-560]; `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Lifecycle command contract matrix").

## Game script bindings

`Zenith_RegisterGameScriptBindings` registers the namespace `Game` (Luau `Zenith.Game.*`) into the engine binding table before it is sealed (`Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types"); every function is available in every phase in which commands may be recorded (1–10) and to behaviours and chunk systems alike:

| Function | Records | Misuse (a script error, never an assert) |
|---|---|---|
| `Game.RequestPause(ctx, reason: "Gameplay" \| "Menu", seconds: number?)` | `Runtime.Pause` add; "Gameplay" requires seconds in (0, 60], converted with `ceil(seconds × 1000)` milliseconds | an unknown reason; "Gameplay" without seconds or out of range; seconds given with "Menu" |
| `Game.CancelPause(ctx, reason: "Gameplay" \| "Menu")` | `Runtime.Pause` remove (useful only from a world that is still ticking, for instance a pause requested and cancelled in the same tick) | an unknown reason |
| `Game.Quit(ctx)` | `Runtime.Quit` | none |
| `Game.ReportTestFailure(ctx, message: string)` | `Runtime.ReportTestFailure`, the message truncated to 198 bytes at a UTF-8 boundary | a non-string message |
| `Game.GetWorldState(ctx) -> { firstTick, worldSeed, continuityTickBase, scene, previousScene, sessionOrdinal, carriedRoots, origin }` | nothing: `SCRIPT_FUNCTION_EFFECT_READ_WORLD` of `Zenith.Runtime.WorldState`, so the calling module lists the resource in `Uses` ([Z2-N-SCRIPT-562]) | the resource missing from `Uses` |

Each misuse raises its catalogued runtime error through `Zenith_ScriptFailThunk` (kind `SCRIPT_ERROR_KIND_RUNTIME`) and records nothing. The codes continue GAME's block after the `Gameplay` namespace's ZS4101–ZS4112 (`Zenith2/Docs/Design/GAME/Scripting.md` → "Misuse errors") and are kept in the table below, GAME's own design, as [Z2-N-SCRIPT-926] requires of a block owner ("lists its codes in that row, and keeps their rows in its own design"); the GAME row of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" reads `ZS4101–ZS4117` (widened by core-doc patch e; cross-area row GA10 (c); cross-doc item X206 created the range table and assigned GAME the block ZS4100–ZS4199):

| Code | Raised by | Message |
|---|---|---|
| ZS4113 | `Game.RequestPause`, `Game.CancelPause` | `Game.<Fn>: unknown pause reason '<r>' (expected "Gameplay" or "Menu")` |
| ZS4114 | `Game.RequestPause` | `Game.RequestPause: "Gameplay" requires seconds in (0, 60], got <v>` (also for missing seconds, shown as `nil`) |
| ZS4115 | `Game.RequestPause` | `Game.RequestPause: seconds are not accepted with "Menu"` |
| ZS4116 | `Game.ReportTestFailure` | `Game.ReportTestFailure: message must be a string, got <type>` |
| ZS4117 | `Game.GetWorldState` | `Game.GetWorldState: the calling module does not list Zenith.Runtime.WorldState in Uses` |

`origin` is one of the strings `"Startup"`, `"Transition"`, `"SaveLoad"`, `"EditorPlay"`; `scene` and `previousScene` are `Zenith.PersistentID` values (nil for a null ID). The generated `.d.luau` entries come from the binding table like every engine binding. A gametest's assertion is a behaviour that calls `Game.ReportTestFailure`, which is what makes a `*.gametest.json` able to fail with exit code 5 rather than only time out (`Zenith2/Docs/Design/MOD.md` → "The gametest format").

## Shutdown and teardown

### Shutdown request

A shutdown is requested by `Zenith_Runtime::RequestShutdown` (C++), a platform quit event, `--exit-after-ticks` (MOD) or `Runtime.Quit` (§ "Game command kinds"). All four take effect the same way: when the current `RunFrame` returns, the frame loop calls no further `RunFrame` (`Zenith2/Docs/Design/MOD.md` → "Frame loop driver", "Shutdown sequence" step 1): every tick that frame started has drained through every phase and every sync point's commands have been played back by their own sync point.

### World lifecycle shutdown

`Zenith_WorldHost_ShutdownWorlds(xState)`, MOD's shutdown step 1 ("shut down each world by its lifecycle"), on the main thread:

1. Flow → `SHUTTING_DOWN`.
2. Cancel an in-flight transition (§ "Transition failure and cancellation"), destroying its isolated world before any live world.
3. For each `LIVE` world in descending tag order: lifecycle `SHUTTING_DOWN`. The commands recorded after the world's final sync point (follow-ups recorded by lifecycle listeners during the final end-of-tick playback) are counted and logged by ECS ([Z2-N-ARCH-561]): `world.commands_after_shutdown` and the `ecs.world` warning line written once when the world simulation is destroyed; the host adds no query and no line of its own, so none is dropped silently and none is reported twice.
4. MOD's step 1 then cancels every outstanding operation and runs jobs until each completes (Contracts step 4); MOD's step 2 destroys the worlds in descending tag order through `Zenith_WorldHost_DestroyWorld`, which walks the teardown order below; then `Zenith_WorldHost_ReleaseRetiredResults(xState, true)`.

### Teardown order algorithm

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Zenith_WorldTeardown.h
enum Zenith_WorldTeardownStep : uint8
{
	WORLD_TEARDOWN_STEP_EXTRACTION = 1,          // the runtime's engine extractors of the world (phase 12)
	WORLD_TEARDOWN_STEP_SNAPSHOT = 2,            // SAVE's per-world snapshot state and replay participant (phase 11)
	WORLD_TEARDOWN_STEP_SCRIPTS = 3,             // the script world: VMs and transactions destroyed; the async table detached, not
	                                             //   destroyed (phases 10 and 2; § "Retired world asynchronous results")
	WORLD_TEARDOWN_STEP_EVENTS = 4,              // undelivered events and pending playback buffers discarded (phase 9)
	WORLD_TEARDOWN_STEP_MODULES = 5,             // DetachWorld in reverse canonical order (phases 7 … 1 of module systems)
	WORLD_TEARDOWN_STEP_SCENE = 6,               // the scene world state: hierarchy, lifecycle dispatcher, additive set (phase 8)
	WORLD_TEARDOWN_STEP_INPUT = 7,               // the input bridge's per-world resources (phase 1)
	WORLD_TEARDOWN_STEP_SIMULATION = 8,          // Zenith_WorldSimulation, the system registry, the ECS world, the tag
	WORLD_TEARDOWN_STEP_ASYNC_RESULTS = 9,       // the detached async table and MOD's two sinks: destroyed, or retired while
	                                             //   results are in flight (§ "Retired world asynchronous results")
};

// Nine steps; ASYNC_RESULTS runs after INPUT and before SIMULATION (the value 9 is its identity, not its position).
inline constexpr Zenith_WorldTeardownStep axWORLD_TEARDOWN_ORDER[] = {
	WORLD_TEARDOWN_STEP_EXTRACTION, WORLD_TEARDOWN_STEP_SNAPSHOT, WORLD_TEARDOWN_STEP_SCRIPTS, WORLD_TEARDOWN_STEP_EVENTS,
	WORLD_TEARDOWN_STEP_MODULES, WORLD_TEARDOWN_STEP_SCENE, WORLD_TEARDOWN_STEP_INPUT, WORLD_TEARDOWN_STEP_ASYNC_RESULTS,
	WORLD_TEARDOWN_STEP_SIMULATION };
```

The order is [Z2-N-ARCH-560]'s reverse phase order as cross-doc items X223 and X274 (1) reworded it (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Shutdown"): extraction state, snapshot state, scripts, events, the modules' per-world state in reverse canonical order, transforms and the scene world state, input, then the world's asynchronous result table, realised over the objects that exist, with four readings. The bullet places the scene world state after the modules, as the signed-off precedent `Zenith2/Docs/Design/MOD.md` → "Shutdown sequence" step 2 does (modules detach, then each world's `Zenith_SceneWorldState` is destroyed with its world, before its `Zenith_WorldSimulation`); the D-PHASES ruling confirms it as candidate R12 (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Refinement candidates").

- Module systems span phases 1–7 (AI and gameplay 7, post-physics 6, physics 5, movement arbitration 4, animation 3, streaming and environment 1), and MOD detaches modules in reverse canonical order (`Zenith2/Docs/Design/MOD.md` → "Module lifecycle state machine"), which is dependents first; for the repository's selections that is also descending phase (Gameplay and AI before Physics before Animation), and MOD's order is authoritative where the two would differ, because a dependent module must detach before what it depends on.
- The scene world state (transforms, phase 8) is torn down after the modules, not before them: it owns the lifecycle dispatcher that modules' listeners are registered with, and each module's attach-journal undo removes its listener from that dispatcher (`Zenith2/Docs/Design/MOD.md` → "Registration journal and undo"); destroying the dispatcher first would leave the undo a dangling reference. Reconciliation item G2 records the reading.
- Scripts (phases 2 and 10) go once, at the position of their second phase, because one script world holds both.
- The scripts entry keeps the world's asynchronous result table: step `SCRIPTS` detaches it and step `ASYNC_RESULTS` destroys or retires it after the modules, the scene world state and the input resources ([Z2-N-ARCH-560], [Z2-N-ARCH-558]; candidate R12), because producers complete into it from their own `DetachWorld` (PHYS's `INBOX` queries, `Zenith2/Docs/Design/PHYS/Simulation.md` → "Deferred query state machine") and from background jobs that outlive it, and the sinks they call are destroyed with it.

The alternative of splitting `WORLD_TEARDOWN_STEP_SCENE` — the propagation state at the transforms' position and the dispatcher and additive set after the modules — was considered and not chosen: SCENE's world state is one object with one `Destroy` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneWorld header"), whose hierarchy resource the modules' attach-journal undo does not touch but whose dispatcher it does, and splitting it would need a second SCENE teardown entry point for no observable difference, since no module reads the hierarchy during its `DetachWorld`.

`Zenith_WorldHost_DestroyWorld` calls the optional test hook `m_pfnOnTeardownStep(tag, step, pUserData)` before each step (through `Zenith_RuntimeTestHooks`, GA5 (h)), which is how `Test_Lifecycle_Shutdown_ReversePhaseTeardown` observes the nine steps in order. The same order is used for a swapped-out world (§ "Swap procedure" step 7), an isolated world that never went live (steps 1 and 2 are then no-ops) and a startup unwind.

## World host counters and logs

| Counter | Kind | Published |
|---|---|---|
| `world.transitions.requested`, `.succeeded`, `.failed`, `.cancelled` | CUMULATIVE | at each event |
| `world.transition.refused`, `world.transition.request_dropped`, `world.transition.cancel_ignored` | CUMULATIVE | at each event |
| `world.transition.replay_diverged`, `world.transition.replay_unreserved` | CUMULATIVE | at each replay divergence; at each replay intake that proceeds unreserved (§ "Intake in a replay") |
| `world.transition.frames` | GAUGE | at each swap: frames from intake to swap |
| `world.transition.activation_us` | GAUGE | at each swap: Σ activation slice time |
| `world.carryover.roots`, `world.carryover.dropped`, `world.carryover.no_persistent_id` | GAUGE (roots), CUMULATIVE | at each swap |
| `world.async_discarded` | CUMULATIVE | at each retired table's release, increased by that retired world's final discarded count (a swapped-out world, or an isolated world destroyed by a failed or cancelled transition); the world host is its only publisher (§ "Retired world asynchronous results") |
| `world.async_retired_overflow`, `world.async_never_completed` | CUMULATIVE | when a fifth or subsequent retired table is held in the overflow list; when a table released at shutdown, or a live world's table destroyed at teardown step `ASYNC_RESULTS` during shutdown, still had uncompleted IDs (§ "Retired world asynchronous results") |
| `world.pause.reasons.<tag>` | GAUGE | every prologue step 7 |
| `world.pause.reserved_bits` | CUMULATIVE | when a reserved bit is ignored |
| `world.commands.rejected` | CUMULATIVE | a rejected `Runtime.*` payload |
| `runtime.game.flow` | GAUGE | at each flow change (`Zenith_GameFlowState` value) |
| `runtime.game.test_failures` | CUMULATIVE | at each `Runtime.ReportTestFailure` playback (diagnostic only: the exit code comes from `Zenith_Runtime::RecordTestAssertionFailure()`) |

| Zone | Around |
|---|---|
| `Runtime.WorldHost.Step7` | the whole of § "Prologue step 7 procedure" |
| `Runtime.WorldHost.CreateIsolated`, `.Activate`, `.Swap`, `.Capture`, `.Teardown` | the corresponding parts |

Log categories, declared in `Internal/Zenith_WorldHostLog.h` with the CORE category macro: `runtime.world` (transitions, swaps, carry-over, teardown, replay divergence) and `runtime.game` (quit requests, test failures, pause requests rejected). Every line quoted in this file is the exact message text.
