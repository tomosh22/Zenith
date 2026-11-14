# Zenith2 Scripting — library layout and file map

Companion to `Zenith2/Docs/Scripting.md`: where every type and algorithm that document and its companions specify lives in `Zenith2/Engine/ZenithScript/`. Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Library layout and file map

<!-- elaboration -->
| Path (under `Zenith2/Engine/ZenithScript/`) | Contents |
|---|---|
| `Zenith_ScriptWorld.h` | `Zenith_ScriptWorld` (per-world runtime: VMs, module set, behaviour instances, merge chain) and its factory `Zenith_ScriptWorld::Create` |
| `Zenith_ScriptRuntime.h` | `Zenith_ScriptRuntime` (the process-wide owner of every script world and of the hot-reload queue; `ApplyPendingReloads`, `CommitPendingReloads`), `Zenith_ScriptRuntimeDesc`, `Zenith_ScriptDebuggerDesc`, the startup registration functions (`Scripting/PublicTypes.md` → "Public types") |
| `Zenith_ScriptError.h` | `Zenith_ScriptErrorCode`, `Zenith_ScriptError`, `Zenith_ScriptDiagnostic`, `Zenith_ScriptErrorCodeFromKind` (the error kinds come from `ZenithReflect/Zenith_ScriptBinding.h`; `Scripting/PublicTypes.md` → "Public types") |
| `Zenith_ScriptAsync.h` | `Zenith_ScriptPublication`, `Zenith_AsyncTableError`, `Zenith_AsyncResultTable` (the per-world pending-result set and completion-ID counter) |
| `Zenith_ScriptHost.h` | the upward interfaces `Zenith_ScriptWorldListener`, `Zenith_ScriptPublicationObserver`, `Zenith_ScriptPublicationSource`, `Zenith_AsyncPublicationHandler` |
| `Zenith_ScriptRecovery.h` | `Zenith_ScriptRecoveryPolicy`, `Zenith_ScriptRecoverySettings`, `Zenith_ScriptRecoveryResolve` |
| `Zenith_ScriptStateSnapshot.h` | `Zenith_ScriptSavedState`, `Zenith_ScriptStateSnapshot` (phase-11 save reads, save-load restores) |
| `Zenith_ScriptBehaviourComponent.h` | `Zenith_BehaviourComponent` (`"Zenith.Behaviour"`, registered at startup step 6) |
| `Zenith_ScriptTypes.h` | the types of `Scripting/PublicTypes.md` → "Public types" declared under this path |
| `Zenith_ScriptCompileOptions.h` | `Zenith_ScriptCompileOptions`, `Zenith_ScriptSandboxMutableGlobals`, `Zenith_ScriptProductionCompileOptions`, `Zenith_ScriptBindingSetHash` |
| `Zenith_ScriptTypesEmitter.h` | `Zenith_ScriptEmitDefinitions` (the `.d.luau` writer ZenithCook calls) |
| `Internal/Zenith_ScriptProtectedCall.{h,cpp}` | the protected entry (the one `lua_pcall`/`lua_resume`/`luau_load`/`catch` site) |
| `Internal/Zenith_ScriptVM.{h,cpp}` | `Zenith_ScriptVM` |
| `Internal/Zenith_ScriptAllocator.cpp` | `Zenith_ScriptAlloc` |
| `Internal/Zenith_ScriptInterrupt.cpp` | `Zenith_ScriptInterrupt`, safepoint counting, watchdog hook |
| `Internal/Zenith_ScriptWatchdog.{h,cpp}` | `Zenith_ScriptWatchdog` |
| `Internal/Zenith_ScriptSandbox.cpp` | environment construction, Zenith `pcall`/`xpcall`/`tostring`/`setmetatable`, removed-function stubs |
| `Internal/Zenith_ScriptModuleLoader.cpp` | the module loader and `require` |
| `Internal/Zenith_ScriptBindingDispatch.cpp` | the dispatcher: Luau stack ↔ `Zenith_ScriptCallFrame` |
| `Internal/Zenith_ScriptViews.cpp` | row proxies, component views, container objects, view validation |
| `Internal/Zenith_ScriptTransaction.{h,cpp}` | `Zenith_ScriptTransaction`, `Zenith_EntityTransaction`, `Zenith_ColumnSnapshot`, `Zenith_SideEffectBuffer`, `Zenith_CowShadow` |
| `Internal/Zenith_ScriptChunkRunner.cpp` | the row trampoline and execution-time failure handling |
| `Internal/Zenith_ScriptBehaviourRunner.cpp` | behaviour scheduling, coroutines, instance invocations |
| `Internal/Zenith_ScriptMerge.cpp` | merge points, capacity acceptance, the reduction tree, commit splice |
| `Internal/Zenith_ScriptRandom.{h,cpp}` | `Zenith_ScriptRandomKey`, `Zenith_ScriptRandomDraw`, the random API |
| `Internal/Zenith_ScriptState.cpp` | state schemas, proxies, checkpoint serialization, restore, migration |
| `Internal/Zenith_ScriptRecovery.cpp` | quarantine, recovery policy, hot reload |
| `Internal/Zenith_ScriptAsync.cpp` | tickets, completion IDs, tick-boundary publication, replay records |
| `Internal/Zenith_ScriptDiagnostics.{h,cpp}` | counters for the profiler and tests (no Luau types in the header) |
| `Internal/Zenith_ScriptTypesEmitter.cpp` | `.d.luau` generation |
| `Internal/Debug/Zenith_ScriptDap.{h,cpp}` | the DAP server |
| `Scripts/` | `@zenith/Core`, the hand-written part of the `@zenith` prelude (`Zenith.Behaviour`, `Zenith.ChunkSystem`, `Zenith.Field`, `Zenith.Wait*`); the generated `Zenith.luau` and `Types.luau` are written per game by the `luau-types` step |
| `Tests/` | doctest cases, `Corpus/{Stateless,Budget,Representative}/`, `Scenarios/`, `Golden/` |
| `Tests/Conformance/` | the `ZenithScript_Conformance` executable (links all ECS backends) |

The script firewall's files outside `Zenith2/Engine/ZenithScript/`:

| Path (under `Zenith2/Engine/`) | Contents |
|---|---|
| `ZenithReflect/Zenith_ScriptBinding.h` | the Luau-free binding ABI: values, call frames, thunks, descriptors, `Zenith_ScriptBindingTable`, `Zenith_ScriptInvocationContext`, the error, side-effect and capacity enums (`Scripting/Bindings.md` → "Binding registration and the thunk ABI") |
| `ZenithScriptCompiler/Zenith_ScriptCompile.h` | `Zenith_ScriptCompileModule`, `Zenith_ScriptCompileReport`, `Zenith_ScriptModuleSource`, `Zenith_ScriptFastcallSite`, `Zenith_ScriptListFastcallSites` |
| `ZenithScriptCompiler/Zenith_ScriptStateless.h` | `Zenith_ScriptCheckStateless`, `Zenith_ScriptStatelessVerdict`, `Zenith_ScriptAcceptedModuleSet` |
| `ZenithScriptCompiler/Internal/Zenith_ScriptCompile.cpp` | the one `luau_compile` site, with the mutable-globals list; `Zenith_ScriptListFastcallSites` (the one reader of `Luau/Bytecode.h`) |
| `ZenithScriptCompiler/Internal/Zenith_ScriptStateless.cpp` | the D-LUAU-STATELESS (open, Stage 1) mechanism |
| `ZenithScriptCompiler/Internal/Zenith_ScriptCompilerGuard.{h,cpp}` | `Zenith_ScriptCompilerGuarded`, the compiler's one `catch` site |
| `ZenithScriptCompiler/Tests/` | `Test_Script_Sandbox_NoFastcallForReplacedBuiltins`, the stateless corpus tests, `Test_Script_Bytecode_DevAndCookIdentical` |
| `ZenithRuntime/Internal/ScriptBindings/` | bindings of the libraries below ZenithReflect, registered by the composition root |

- [Z2-N-SCRIPT-881] `ZenithScript` depends on ZenithCore, ZenithReflect, ZenithECS, ZenithScene, ZenithAssets, ZenithJobs, ZenithPlatform and ZenithProfiling (asset handles cross the marshalling table; the DAP loopback socket and the file watcher use ZenithPlatform) as the library dependency graph declares (`Zenith2/Docs/Architecture.md` → "Library dependency graph"); no library that ZenithScript depends on, and no library that owns bindings, depends on ZenithScript, because the binding ABI is in ZenithReflect; `ZenithScriptCompiler` depends on ZenithScript (for the shared public types of `Scripting/PublicTypes.md` → "Public types"), `Luau.Compiler` and `Luau.Ast`, and ZenithScript never depends on ZenithScriptCompiler; the composition root passes the sealed `Zenith_ScriptBindingTable` to every `Zenith_ScriptWorld`.

### Public API of Zenith_ScriptWorld

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptWorld.h
#pragma once
#include "Zenith_ScriptError.h"
#include "Zenith_ScriptTypes.h"
#include "ZenithReflect/Zenith_ScriptBinding.h"
#include "ZenithCore/Zenith_ReentrancyGuard.h"

class Zenith_World;                  // ZenithECS
class Zenith_WorldSimulation;        // ZenithECS
class Zenith_SystemRegistry;         // ZenithECS
class Zenith_TypeRegistry;           // ZenithReflect
class Zenith_JobSystem;              // ZenithJobs
class Zenith_ScriptWatchdog;         // Scripting/PublicTypes.md → "Public types"
class Zenith_ScriptModuleManifest;
class Zenith_ScriptWorldListener;    // Zenith_ScriptHost.h
class Zenith_ScriptPublicationObserver;
class Zenith_ScriptPublicationSource;
class Zenith_Hash64Stream;           // ZenithCore
class Zenith_AsyncResultTable;       // Zenith_ScriptAsync.h
class Zenith_ScriptStateSnapshot;    // Zenith_ScriptStateSnapshot.h
struct Zenith_ScriptSavedState;
struct Zenith_ScriptBytecode;

enum Zenith_ScriptWorldState : uint8
{
	SCRIPT_WORLD_STATE_RUNNING,
	SCRIPT_WORLD_STATE_HALTED,           // a fatal event occurred; no further tick commits
};

struct Zenith_ScriptSafepointLimits                       // Zenith_ScriptWorld::Create: SCRIPT_ERROR_INVALID_DESC when any member is zero
{
	uint64 m_ulBehaviourPerInvocation = 0;                  // Z2-B-SCRIPT-001
	uint64 m_ulChunkSystemPerEntity = 0;                    // Z2-B-SCRIPT-002
	uint64 m_ulRecoveryPerStep = 0;                         // Z2-B-SCRIPT-018
	uint64 m_ulModuleLoad = 0;                              // Z2-B-SCRIPT-019
};

struct Zenith_ScriptCapacityLimits                       // the SCRIPT-held capacity classes; the ECS classes are Zenith_MergeCapacity's
{
	uint32 m_uStringInternEntries = 0;                      // Z2-B-SCRIPT-022
	uint32 m_uStringInternBytes = 0;
	uint32 m_uAudioTriggersPerTick = 0;                     // Z2-B-SCRIPT-023
	uint32 m_uVfxTriggersPerTick = 0;                       // Z2-B-SCRIPT-024
	uint32 m_uSaveRequestsPerTick = 0;                      // Z2-B-SCRIPT-025
	uint32 m_auAsyncInFlight[4] = {};                       // path (Z2-B-AI-003), streaming, physics query, online (Z2-B-SCRIPT-026)
};

struct Zenith_ScriptWorldDesc
{
	Zenith_World* m_pxWorld = nullptr;                       // borrowed; the ECS world; outlives the script world
	Zenith_WorldSimulation* m_pxSimulation = nullptr;        // borrowed: the simulation of m_pxWorld (tick index, system registry, tick-boundary handlers)
	const Zenith_TypeRegistry* m_pxTypes = nullptr;          // frozen
	uint16 m_uWorldTag = 0;                                 // the world's tag (Zenith_WorldTagIssue::m_uTag)
	Zenith_JobSystem* m_pxJobs = nullptr;                    // borrowed
	Zenith_ScriptWatchdog* m_pxWatchdog = nullptr;           // borrowed; owned by the composition root
	const Zenith_ScriptBindingTable* m_pxBindings = nullptr; // sealed before any world is created
	const Zenith_ScriptModuleManifest* m_pxModules = nullptr;
	uint64 m_ulWorldSeed = 0;
	uint32 m_uFrameThreadCount = 0;                         // Zenith_JobSystem::GetFrameThreadCount(); 1 … uSCRIPT_MAX_WORKER_VM_SLOTS
	uint64 m_ulMainVMCapBytes = 0;                          // from the main VM memory cap row
	uint64 m_ulWorkerVMCapBytes = 0;                        // from the worker VM memory cap row
	uint64 m_ulWorkerVMTotalBytes = 0;                      // Z2-B-SCRIPT-011
	Zenith_ScriptGCTuning m_xGCTuning;                       // Z2-B-SCRIPT-017
	uint32 m_uGCStepKBPerTick = 0;                          // Z2-B-SCRIPT-007
	uint32 m_uSnapshotCapBytesPerEntity = 0;                // Z2-B-SCRIPT-012
	uint32 m_uCheckpointSlotBytes = 0;                      // Z2-B-SCRIPT-015
	Zenith_ScriptSafepointLimits m_xSafepoints;
	Zenith_ScriptCapacityLimits m_xCapacities;
	Zenith_ScriptNativePolicy m_eNativePolicy = SCRIPT_NATIVE_POLICY_TOOLS_ONLY;
	Zenith_ScriptRecoveryPolicy m_eRecoveryPolicy = SCRIPT_RECOVERY_POLICY_MANUAL;   // the three recovery members come from one Zenith_ScriptRecoverySettings
	uint32 m_uRecoveryDelayTicks = 60;
	uint32 m_uMaxRecoveriesPerModule = 3;
	Zenith_ScriptWorldListener* m_pxListener = nullptr;      // borrowed; halts, disables, recoveries and reloads
	bool m_bRunSimulationScripts = true;                     // false: no script system registered and no behaviour instance attached,
	                                                         // modules still load and the console evaluates; false for EDIT and PREVIEW worlds
	Zenith_ScriptPublicationObserver* m_pxPublicationObserver = nullptr;   // borrowed; the replay recorder, or null
	Zenith_ScriptPublicationSource* m_pxPublicationSource = nullptr;       // borrowed; under --replay, else null
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;
};

class Zenith_ConfigSchema;           // ZenithCore, Zenith_ConfigStore.h
class Zenith_ConfigStore;            // ZenithCore, Zenith_ConfigStore.h

/// @contract functions Zenith_ScriptReadWorldLimits
/// Ownership: Writes only the row-valued members of xDesc (m_ulMainVMCapBytes, m_ulWorkerVMCapBytes, m_ulWorkerVMTotalBytes,
///   m_xGCTuning, m_uGCStepKBPerTick, m_uSnapshotCapBytesPerEntity, m_uCheckpointSlotBytes, m_xSafepoints, m_xCapacities):
///   SCRIPT's allocated row values, overridden by the registered script.* configuration keys; pointers, tag, seed and thread
///   count stay the caller's.
/// Thread affinity: MAIN_THREAD. Startup step 16, before Zenith_ScriptRuntime::CreateWorld.
/// Invalidation: None.
/// Units: As the members' rows.
/// Errors: SCRIPT_ERROR_INVALID_DESC when a script.* key it reads is missing from xSchema; xDesc is then unchanged.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptReadWorldLimits(const Zenith_ConfigSchema& xSchema,
	const Zenith_ConfigStore& xStore, Zenith_ScriptWorldDesc& xDesc);   // the allocated rows; script.gc.stepKBPerTick sets the GC step

inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_MAIN_VM = 0x53430001u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_SLOT_TABLE = 0x53430002u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_TXN_ARENAS = 0x53430003u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_ASYNC_TABLE = 0x53430004u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_HANDLER = 0x53430005u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_WATCHDOG_REGISTER = 0x53430006u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_DIAGNOSTICS = 0x53430007u;

/// @contract type Zenith_ScriptWorld
/// Ownership: Created by Zenith_ScriptRuntime::CreateWorld (through Zenith_ScriptWorld::Create) as a Zenith_Owned<Zenith_ScriptWorld> held by the Zenith_ScriptRuntime;
///   owns its VMs, behaviour instances, transaction arenas and module generations; destroyed after the tick is drained.
/// Thread affinity: MAIN_THREAD. Every member runs on the main thread, except that the worker VMs it owns are used by
///   their owning workers inside the script nodes it registers.
/// Invalidation: Destroying it releases every VM, retained callback and instance; views and tickets it issued are invalid.
/// Units: Ticks are fixed-step simulation ticks; memory caps in bytes.
/// Errors: Fallible members return Zenith_Result<…, Zenith_ScriptError>; script errors never assert; a fatal script event
///   sets SCRIPT_WORLD_STATE_HALTED and is reported to the game's fatal handler.
/// Reentrancy: ALLOWED(GetState, IsModuleDisabled). Script callbacks re-enter through Zenith_ScriptInvocationContext::InvokeCallback,
///   at most uSCRIPT_MAX_PROTECTED_DEPTH deep; every other member of this type is non-reentrant.
/// Version: since Z2 Stage 3. Cooked bytecode and checkpoints carry their own versions (Zenith2/Docs/Scripting.md → "Shipping bytecode compatibility").
/// @endcontract
class Zenith_ScriptWorld
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ScriptWorld>, Zenith_ScriptError> Create(const Zenith_ScriptWorldDesc& xDesc);
	~Zenith_ScriptWorld();

	Zenith_Result<void, Zenith_ScriptError> LoadModules();          // world load: validates, checks statelessness, compares state-schema shapes
		// succeeds when the world is usable: a module that fails to load is disabled for the session, recorded in the
		// diagnostic store and reported through OnModuleDisabled; an error means the manifest is unusable
		// (SCRIPT_ERROR_BYTECODE_MISMATCH) or no behaviour or chunk-system module of a non-empty manifest loaded (SCRIPT_ERROR_LOAD)
	Zenith_Result<void, Zenith_ScriptError> RegisterScriptNodes(Zenith_SystemRegistry& xSystems);
		// startup step 17: Zenith_SystemRegistry::AddSystem per chunk system (SYSTEM_KIND_SCRIPT_CHUNK) and per behaviour module
		// and phase (SYSTEM_KIND_SCRIPT_BEHAVIOURS), each with its Zenith_PfnSystemMerge; ECS builds the nodes and merge chain
	void OnTickBoundary(uint64 ulTick);                                           // the step-3 body of the tick-boundary adapter Create registers; public for tests
	uint32 GetDisabledModuleCount() const;
	Zenith_AsyncResultTable& GetAsyncResults();
	// MAIN_THREAD, out of tick: hands the table to the world host (retirement at a swap or teardown); the world keeps no table
	// afterwards and GetAsyncResults asserts
	Zenith_Owned<Zenith_AsyncResultTable> DetachAsyncResults();
	// The committed checkpoints of Zenith2/Docs/Scripting.md → "Determinism test matrix" into the tick hash, in ascending
	// (source ID, simulation ID)
	void AppendCommittedStateHash(Zenith_Hash64Stream& xHash) const;
	const Zenith_ScriptStateSnapshot& GetStateSnapshot() const;       // phase-11 readers
	Zenith_Result<void, Zenith_ScriptError> RestoreSavedStates(Zenith_Span<const Zenith_ScriptSavedState> axStates);   // out of tick
	bool GetDiagnostic(uint32 uSequence, Zenith_ScriptDiagnostic& xOut) const;   // false when overwritten or unknown
	uint32 GetLatestDiagnosticSequence() const;
	uint16 GetWorldTag() const;
	Zenith_Result<uint32, Zenith_ScriptError> ConsoleEvaluate(Zenith_StringView strSource,
		Zenith_MutableByteSpan xResultText);          // development builds, out of tick (TOOLS context); Shipping: SCRIPT_ERROR_INVALID_DESC
	/// @deviation Thread affinity: ANY_THREAD. Reads an atomic flag.
	bool IsHalted() const;


	Zenith_Result<void, Zenith_ScriptError> RequestHotReload(Zenith_ScriptModuleID xModule, const Zenith_ScriptBytecode& xBytecode,
		Zenith_Span<const Zenith_ScriptNameRefRecord> axNameRefs = {});  // the module's load-time name references, copied with the bytecode ([Z2-N-SCRIPT-927]); queued; applied by Zenith_ScriptRuntime::ApplyPendingReloads in the next frame prologue (the loop steps before RunFrame that precede the frame's first tick boundary)
	Zenith_Result<void, Zenith_ScriptError> RequestModuleReload(Zenith_ScriptModuleID xModule);  // last-good restore of a disabled module

	Zenith_ScriptWorldState GetState() const { return m_eState; }
	bool IsModuleDisabled(Zenith_ScriptModuleID xModule) const;

private:
	Zenith_ScriptWorldState m_eState = SCRIPT_WORLD_STATE_RUNNING;
	Zenith_ReentrancyGuard m_xReentrancyGuard;           // Zenith2/Docs/Contracts.md → "Threading and concurrency contract"
};
```

- [Z2-N-SCRIPT-882] Every fallible public `ZenithScript` function returns a `Zenith_Result`, and no public function carries `noexcept` or `[[nodiscard]]` (`Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-582], [Z2-N-CONTRACT-586]); the public headers include no Luau header and name no Luau type.
