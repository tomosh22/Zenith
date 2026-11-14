# CORE design: Core and mathematics, and the memory governor

This is the design document of the CORE area (epic `Z2 CORE — Core & mathematics`), which owns the library `ZenithCore`. It is the entry document of a family: this file carries the purpose, scope, dependencies, layout, the public API index, the top-level contracts, failure handling, the test strategy, budgets, the stage plan, decisions and the requirement index; the split files under `Zenith2/Docs/Design/CORE/` carry the full specifications. A worker implementing a CORE ticket reads this file's sections the ticket cites and the split-file sections it names. Requirements are catalogued in `Zenith2/Docs/Requirements/CORE.md` as `Z2-R-CORE-NNN`.

<!-- informative -->
Conventions: `<!-- elaboration -->` marks detail designed here beyond the programme plan; `<!-- assumption: pending <AREA> -->` … `<!-- /assumption -->` marks an interface this document needs from a core document or another area that does not state it yet, reconciled through `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`. Design documents are not normatively scanned; every obligation here is traceable to a requirement in the catalogue (table "Requirement index").
<!-- /informative -->

## Purpose and scope

### What CORE owns

CORE owns the layer-0 library `ZenithCore`: everything every other library needs and nothing that needs another library.

| Capability | Specified in |
|---|---|
| ownership (`Zenith_Owned`), generational handles (`Zenith_SlotTable`, `Zenith_OperationHandle`), persistent IDs | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Owned"; `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable"; `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions" |
| the general heap, arenas, pools, sanitizer integration, the Debug leak table | `Zenith2/Docs/Contracts/Memory.md` (every heading); `Zenith2/Docs/Design/CORE/Memory.md` → "Heap backing", "Arena internals", "Pool internals" |
| allocation-failure behaviour: `Zenith_TryNew`, the OOM handler, `Zenith_FaultInjector` | `Zenith2/Docs/Contracts/Memory.md` → "OOM handler", "Fallible construction"; `Zenith2/Docs/Design/CORE/Memory.md` → "Emergency reserve and the default handler", "Fault-injection support" |
| containers and their justification records, including the single-producer single-consumer `Zenith_SpscRing` and `Zenith_TripleBuffer` (ruling Q80) | `Zenith2/Docs/Design/CORE/Containers.md` |
| strings, Unicode, interning, formatting, number text, JSON, and the JSON document tree `Zenith_DocumentTree` (the cursor every migration step receives) | `Zenith2/Docs/Design/CORE/Text.md` (the tree: "JSON document tree") |
| numerical conventions, units, coordinate systems, maths types, the deterministic transcendental functions (`Zenith_Sin` … `Zenith_Sqrt`, ruling X314-FMA3) and the CRT maths mode, hashing (`Zenith_Hash64` and the fixed SHA-256 `Zenith_Sha256`), random streams | `Zenith2/Docs/Design/CORE/Maths.md` |
| clocks, diagnostics (asserts, fatal errors, results, the error taxonomy, logging primitives), build information, configuration store | `Zenith2/Docs/Design/CORE/Diagnostics.md` |
| threading primitives (`Zenith_Multithreading`), the lock-order checker, callback registries, reentrancy guards, the no-allocation scope `Zenith_NoAllocationScope` | `Zenith2/Docs/Design/CORE/Threading.md` |
| the cross-system memory governor, including transition-peak and subsystem reservations | `Zenith2/Docs/Architecture.md` → "Memory governor"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` |

### Explicit exclusions

| Not CORE | Owner | Basis |
|---|---|---|
| the job system, work-stealing deques, the floating-point environment functions, the simulation clock | JOBS (`ZenithJobs`) | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Job system"; `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ([Z2-N-CONTRACT-360]) |
| the profiler (zones, counters, overlay, Tracy); CORE declares only the hook table | DEVT (`ZenithProfiling`) | `Zenith2/Docs/Contracts.md` → "No mutable global state" |
| log tooling: file sink, on-screen console, remote log streaming, crash dumps | DEVT and PLAT (D-SEL-CRASH (open, Stage 2)) | inventory item Z2-I-DEVT-22 |
| configuration sources: files through the virtual filesystem, the command line, user-layer persistence, the `Zenith_Config` facade | PLAT (`ZenithPlatform`) | `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" |
| half-float, octahedral and QTangent codecs | GEOM (`ZenithVertexFormat`) | `Zenith2/Docs/Rendering.md` → "Vertex data format" |
| type IDs, property IDs and the type registry | ECS (`ZenithReflect`) | `Zenith2/Docs/Contracts.md` → "Reflection" |
| entity, component, asset and device handles | ECS, ASSET, RHI | `Zenith2/Docs/Contracts.md` → "Handle kinds" |
| each subsystem's shedding under memory pressure | the subsystem's own area | `Zenith2/Docs/Architecture.md` → "Memory governor" (table "Shedding per subsystem") |
| text shaping, bidirectional text, localisation | UI, D-SEL-TEXT (open, Stage 1/2) | — |
| canonical key order of authoring documents and the migration framework that runs the steps (the step cursor `Zenith_DocumentTree` is CORE's, `Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree") | SCENE and ASSET | `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" |
| axis, handedness and unit conversion on import | CONTENT, D-SEL-FBX (open, Stage 4) | inventory item Z2-I-CONTENT-11 |
| STL smart pointers and exceptions | forbidden, not re-justified | Z2-D-040, Z2-D-045 |

### Inventory items covered

Every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" (section "CORE Core & mathematics"), Z2-I-CORE-01 to Z2-I-CORE-30, is covered by the catalogue (`Zenith2/Docs/Requirements/CORE.md` → "Coverage of inventory items"). CORE also owns the ZenithCore half of three cross-area items, naming the other area: the `Zenith_Multithreading` primitives under Z2-I-JOBS-01 (JOBS owns the scheduler), the blocking-wait and lock-contention hooks under Z2-I-JOBS-09 (JOBS owns contention diagnostics), and cancellation completion of `Zenith_OperationHandle` under Z2-I-JOBS-07 (JOBS owns job cancellation).

### Document family

| File | Its `##` headings |
|---|---|
| `Zenith2/Docs/Design/CORE.md` (this file) | Purpose and scope · Dependencies · Library and file layout · Public API · Data structures and formats · Algorithms and state machines · Threading lifetime and error contract · Diagnostics and profiling · Authoring and cooking · Failure handling · Test strategy · Budget rows · Stage plan · Decisions · Requirement index · Reconciliation items |
| `Zenith2/Docs/Design/CORE/Memory.md` | Scope of this file · Heap backing · Owned pointer placement · Arena internals · Pool internals · Zenith_Function · Zenith_SlotTable · Fault-injection support · Test main and the Android test entry · Test child processes · Factory fault points · Memory tests |
| `Zenith2/Docs/Design/CORE/Containers.md` | Scope of this file · Container set · Justification records · Zenith_Span · Zenith_Array · Zenith_FixedArray · Zenith_InlineArray · Zenith_HashMap and Zenith_HashSet · Zenith_RingBuffer · Zenith_BoundedQueue · Zenith_SpscRing · Zenith_TripleBuffer · Zenith_BitArray · Sorting and searching · Container tests |
| `Zenith2/Docs/Design/CORE/Text.md` | Scope of this file · Encoding policy · Zenith_StringView · Zenith_String · Unicode functions · String interning · Formatting · Number conversion · JSON writer · JSON reader · JSON document tree · Text tests |
| `Zenith2/Docs/Design/CORE/Maths.md` | Scope of this file · Numerical conventions · Deterministic transcendental functions · Units · Coordinate systems · Vector types · Matrix types · Zenith_Quaternion · Zenith_Colour · Bounds types · Zenith_Hash64 · Zenith_Sha256 · Zenith_RandomSource · Zenith_PersistentID functions · Maths tests |
| `Zenith2/Docs/Design/CORE/Diagnostics.md` | Scope of this file · Build information · Assertions and fatal errors · Results and the error taxonomy · Structured logging · Clocks · Configuration store · Diagnostics tests |
| `Zenith2/Docs/Design/CORE/Threading.md` | Scope of this file · Threads · Mutexes and lock ordering · Condition variables and semaphores · Debug thread state · Zenith_NoAllocationScope · Zenith_ReentrancyGuard · Zenith_CallbackRegistry implementation · Zenith_OperationHandle implementation · Threading tests |
| `Zenith2/Docs/Design/CORE/MemoryGovernor.md` | Scope of this file · Additions to the governor API · Consumer registration · Generated budget table · Tuning keys · Level evaluation · Reservation algorithm · Subsystem reservation algorithm · Accounting · OS memory queries · Counters · Governor tests |

## Dependencies

### Libraries

`ZenithCore` is layer 0 of `Zenith2/Docs/Architecture.md` → "Library dependency graph": it declares no Zenith2 dependency. It includes only the 13 allowed standard headers in runtime code (`Zenith2/Docs/Contracts.md` → "Containers and standard library policy"), plus, under `Internal/` alone:

| Include | Where | Basis |
|---|---|---|
| platform SDK headers (`windows.h`, `bcrypt.h`, `dbghelp.h`, `psapi.h`; `pthread.h`, `semaphore.h`, `unistd.h`, `sys/random.h`, `unwind.h`, `dlfcn.h`, `android/log.h`; `spawn.h`, `sys/wait.h`, `signal.h`, `poll.h`, `fcntl.h` for the test-process launcher on Linux) | `Internal/Windows/`, `Internal/Android/`, `Internal/Linux/` per platform | rule F18 |
| `<emmintrin.h>` (x86-64), `<arm_neon.h>` (arm64) | `Internal/Zenith_CompilerSupport.h` (`Zenith_HardwareSqrt`) | rule F11, JR-CORE-COMPILERSUPPORT |
| `<stdlib.h>` (`_set_FMA3_enable`, `_get_FMA3_enable`) | `Internal/Windows/Zenith_CrtMathMode_Windows.cpp` | ruling X314-FMA3 |
| `<sanitizer/asan_interface.h>`, `<sanitizer/hwasan_interface.h>` | `Internal/Memory/Zenith_SanitizerMemory.cpp` | [Z2-N-CONTRACT-432] |
| `<charconv>` | `Internal/Text/Zenith_NumberText.cpp` | JR-CORE-CHARCONV ([Z2-N-CONTRACT-273]) |
| xxHash | `Internal/Hash/Zenith_Hash.cpp` | D-SEL-HASH (open, Stage 1) |
| yyjson or simdjson (one) | `Internal/Json/Zenith_JsonBackend_<Name>.cpp` | D-SEL-JSON (open, Stage 1) |
| mimalloc (if ruled) | `Internal/Memory/Zenith_HeapBacking_Mimalloc.cpp` | D-SEL-HEAP (open, Stage 1) |

### Headings this area relies on

| Doc and heading | What CORE takes from it |
|---|---|
| `Zenith2/Docs/Contracts.md` → "Ownership and allocation" and every heading of `Zenith2/Docs/Contracts/Memory.md` | the memory API and mechanisms CORE implements |
| `Zenith2/Docs/Contracts.md` → "Handle kinds" | `Zenith_PersistentID`, `Zenith_OperationHandle` declarations; the handle tests |
| `Zenith2/Docs/Contracts.md` → "No mutable global state" | the hook table, the five allowlisted variables of "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]) and their declaration comment block ([Z2-N-CONTRACT-578]) |
| `Zenith2/Docs/Contracts.md` → "Containers and standard library policy" | container fields, record template, oracle and property tests |
| `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" | primitives, lock table, callback, cancellation, queue and shutdown contracts |
| `Zenith2/Docs/Contracts.md` → "Errors and results" | `Zenith_Result`, asserts, fatal codes |
| `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" | test naming, suites, ForbiddenCheck rules, `Zenith_NarrowCast` |
| `Zenith2/Docs/Contracts.md` → "Contract blocks per type" | the block grammar every public type here carries |
| `Zenith2/Docs/Architecture.md` → "Memory governor" | the governor model and API |
| `Zenith2/Docs/Architecture.md` → "Security architecture" | the JSON parser limits |
| `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" | the rules hashed containers, IDs and maths obey |
| `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix", "Compile and link flags" | the matrix, the defines, no floating-point contraction |
| `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" | the six layers and provenance the store implements |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Simulation clock" | the consumer of the monotonic clock |
| `Zenith2/Docs/Scripting.md` → "Determinism rules and randomness" | the draw derivations CORE shares |
| `Zenith2/Docs/Budgets.md` → "Memory budgets", "Row index" | the rows the generated budget table reads |

### Upward-callback interfaces CORE declares

CORE is the lowest layer, so every notification to a higher library goes through an interface or function-pointer table that CORE declares and the higher library implements ([Z2-N-CONTRACT-175], [Z2-N-ARCH-078]):

| Interface | Implemented by | Passed down by |
|---|---|---|
| `Zenith_ProfilerHooks` (zones, counters, lock events, blocking waits, log records) | ZenithProfiling (profiler callbacks); `Zenith_LogRouter` (log callbacks, a CORE type) | `main` through `Zenith_InstallProfilerHooks` |
| `Zenith_MemoryConsumer` | every subsystem with memory to shed | the composition root and module start |
| `Zenith_LogSink` | CORE's console and ring sinks; DEVT's file sink and console | the composition root (startup step 7) |
| `Zenith_PfnOOMHandler` | `main` (the default handler unless a game composition overrides it) | `Zenith_HeapDesc` |
| `Zenith_FaultInjector` | tests | factory descs |
| `Zenith_CallbackRegistry<void(TArgs...)>` | any registrant | the registry owner |

### Names other documents assume

CORE adopts every name another document already uses (`Zenith2/Docs/Architecture.md` → "Purpose and scope", table "Assumed types", [Z2-N-ARCH-005]; cross-doc items X7, X26 and X36):

| Name | Header | Specified in |
|---|---|---|
| `Zenith_Span<T>`, `Zenith_ByteSpan`, `Zenith_MutableByteSpan` | `ZenithCore/Zenith_Span.h` | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Span" |
| `Zenith_StringView` | `ZenithCore/Zenith_StringView.h` | `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_StringView" |
| `Zenith_JsonWriter`, `Zenith_JsonWriteError` | `ZenithCore/Zenith_JsonWriter.h` | `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer" |
| `Zenith_Hash64` | `ZenithCore/Zenith_Hash.h` | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64" |
| `Zenith_Sha256Value`, `Zenith_Sha256Digest`, `Zenith_Sha256Stream`, `Zenith_FormatSha256Hex`, `Zenith_ParseSha256Hex` (the host SHA-256 of SCENE's derived IDs and journals and PLAT's patch mount, X152) | `ZenithCore/Zenith_Sha256.h` | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256" |
| `Zenith_StringID` | `ZenithCore/Zenith_StringID.h` | `Zenith2/Docs/Design/CORE/Text.md` → "String interning" |
| `Zenith_DocumentTree` (the cursor `Zenith_PfnMigrationStep` takes; ECS forward-declares it, SCENE and ASSET construct it; cross-area notes C13 and S5) | `ZenithCore/Zenith_DocumentTree.h` | `Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree" |
| `Zenith_Vector2`, `Zenith_Vector3`, `Zenith_Vector4`, `Zenith_Matrix4`, `Zenith_Quaternion`, `Zenith_Colour` | `ZenithCore/Zenith_Vector.h`, `Zenith_Matrix.h`, `Zenith_Quaternion.h`, `Zenith_Colour.h` | `Zenith2/Docs/Design/CORE/Maths.md` → "Vector types" |
| `Zenith_FixedArray<T, N>` | `ZenithCore/Zenith_FixedArray.h` | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_FixedArray" |
| `Zenith_BitArray` (a fixed-capacity bit set) | `ZenithCore/Zenith_BitArray.h` | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BitArray" |
| `Zenith_Function<TSignature>` | `ZenithCore/Zenith_Function.h` | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_Function" |
| `Zenith_FaultInjector` | `ZenithCore/Zenith_FaultInjector.h` | `Zenith2/Docs/Contracts/Memory.md` → "Fallible construction" |
| `Zenith_ProfilerHooks`, `Zenith_InstallProfilerHooks`, `Zenith_GetProfilerHooks`, `Zenith_IsZoneHookInstalled`, `Zenith_IsLockAcquireHookInstalled`, `Zenith_IsBlockingWaitHookInstalled`, the dispatch functions `Zenith_DispatchZoneBegin`, `Zenith_DispatchZoneEnd`, `Zenith_DispatchCounter`, `Zenith_DispatchLockAcquire`, `Zenith_DispatchLockRelease`, `Zenith_DispatchBlockingWait` through which every area invokes a hook, and `Zenith_GetHookDepth`, `Zenith_GetHookReentryCount` | `ZenithCore/Zenith_ProfilerHooks.h` | `Zenith2/Docs/Contracts.md` → "No mutable global state"; `Zenith2/Docs/Design/CORE/Threading.md` → "Lock profiling and hook dispatch" |
| `Zenith_Mutex` and `Zenith_MutexNoProfiling`, `Zenith_ScopedLock<TMutex>`, `Zenith_LockDesc`, the lock-order checker in `ZenithCore/Internal/Profiling/` | `ZenithCore/Zenith_Mutex.h` | `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering" |
| `Zenith_NarrowCast`, `Zenith_TryNarrowCast` | `ZenithCore/Zenith_NarrowCast.h` | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" |
| `Zenith_OperationHandle` | `ZenithCore/Zenith_OperationHandle.h` | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" |
| `Zenith_PersistentID`, `Zenith_GeneratePersistentID` | `ZenithCore/Zenith_PersistentID.h` | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions" |
| `Zenith_RandomSource` | `ZenithCore/Zenith_Random.h` | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| `Zenith_Sin`, `Zenith_Cos`, `Zenith_Atan2` (WORLD row WD29, by these names) and the rest of the deterministic set, `Zenith_ApplyCrtMathMode` ("CORE's deterministic functions" of PHYS rule D9 and of ANIM, CHAR, AI and TWOD) | `ZenithCore/Zenith_Transcendental.h`, included by `Zenith_Maths.h` | `Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions" |
| `Zenith_SpscRing<T>`, `Zenith_TripleBuffer<T>` (AUDIO row AU6, DEVT row DV23; AUDIO's `Zenith_MutableSpan<T>` is `Zenith_Span<T>`) | `ZenithCore/Zenith_SpscRing.h`, `Zenith_TripleBuffer.h` | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_SpscRing", "Zenith_TripleBuffer" |
| `Zenith_NoAllocationScope`, `Zenith_EnterNoAllocationScope`, `Zenith_LeaveNoAllocationScope` (AUDIO row AU6) | `ZenithCore/Zenith_NoAllocationScope.h` | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_NoAllocationScope" |
| `Zenith_RunChildTestCase`, `Zenith_TestProcess`, `Zenith_GetTestExecutablePath` (SCENE's and SAVE's child-process tests, cross-doc item X349) | `ZenithCore/Tests/Support/Zenith_TestProcess.h` (ZenithTestSupport) | `Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes" |
| `Zenith_IsShippingBuild` and the other build readers | `ZenithCore/Zenith_BuildInfo.h` | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information" |
| `Zenith_ReentrancyGuard`, `Zenith_CallbackRegistry`, `Zenith_CallbackToken` | `ZenithCore/Zenith_ReentrancyGuard.h`, `Zenith_CallbackRegistry.h` | `Zenith2/Docs/Design/CORE/Threading.md` |
| `Zenith_MemoryGovernor` and its types | `ZenithCore/Zenith_MemoryGovernor.h` | `Zenith2/Docs/Architecture.md` → "Memory governor" |
| `Zenith_Clock.h`, `Zenith_GetMonotonicTimeUs` | `ZenithCore/Zenith_Clock.h` | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" |
| `Zenith_ConfigValue`, `Zenith_ConfigKeyDesc`, `Zenith_ConfigProvenance`, `Zenith_ConfigLayer` | `ZenithCore/Zenith_ConfigStore.h` | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |

The three globals the engine allowlists as "installed in `main`" — the root heap pointer `g_pxRootHeap` and the profiler-hook table `g_xProfilerHooks` in ZenithCore, and ZenithCore's `thread_local` Debug thread state `g_xDebugThreadState` (ZenithProfiling's `thread_local` buffers and ZenithPlatform's crash handler anchor are the other two of the five entries of `Zenith2/Docs/Contracts.md` → "Writable-data allowlist entries", [Z2-N-CONTRACT-152]) — are all that ZenithCore holds; CORE adds no variable to the writable-data allowlist.

The Luau prelude's `Asset<TKind>` shape maps to `Zenith_AssetHandle<T>`, whose owner is ASSET (`ZenithAssets`, `Zenith2/Docs/Contracts.md` → "Handle kinds"); CORE provides no asset type and the mapping is recorded here because the prelude names it beside CORE's `Zenith_FixedArray` and `Zenith_BitArray` (X36).

### Areas CORE depends on for integration

| Area | Integration |
|---|---|
| PLAT | reports OS low-memory to the governor; owns configuration sources and `Zenith_Config`; installs the crash handler that CORE's termination reaches |
| DEVT | implements the profiler hooks; the file log sink; displays the counters CORE publishes |
| MOD | the Player's `main` and `SDL_main` call `Zenith_ApplyCrtMathMode` as their first statement (cross-area row C20); the composition root: installs the root heap, the log router and hooks in `main`; creates the string table, schema, store and governor at the startup steps named in "Threading lifetime and error contract"; makes the prologue calls of "Frame-graph placement"; owns the process's one `Zenith_StringTable` through `Zenith_Owned` created by CORE's `Create` (no second intern-table implementation) — every item is listed for MOD in the C16 row of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` |
| every subsystem area | implements `Zenith_MemoryConsumer` and its shedding row; an operation that must not allocate past its subsystem's budget takes a `Zenith_SubsystemReservation` (SCENE's large preparations, Stage 3) |
| SHIP | the `Zenith2/CMake/` helpers that host the budget-table generator; the manifest entries CORE's rulings add |
| MOD (test APKs) | links `ZenithTestSupport`'s `SDL_main` into each `<Test>_ApkLib` with `-Wl,--undefined=SDL_main` and reads the tagged output on logcat tag `Zenith2Test` (cross-area row MB23) |

## Library and file layout

### ZenithCore target

```cmake
# Zenith2/Engine/ZenithCore/CMakeLists.txt
zenith_add_library(ZenithCore
	THIRD_PARTY xxhash <yyjson|simdjson per D-SEL-JSON> [mimalloc per D-SEL-HEAP]
	GENERATES Internal/Generated/Zenith_MemoryBudgets.generated.h)
```

Because ZenithCore has a `Platform/` directory, `zenith_add_library()` also adds the target platform's `Platform/${CMAKE_SYSTEM_NAME}` directory (`Windows`, `Android` or `Linux`) to its public include directories ("Platform threading headers"); the call above names nothing for it.

`KIND ENGINE` (default), `SUPPORTED ZENITH_ALL` (every configure preset including `linux-tsan`, `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix"), `EXCEPTIONS OFF`. Until the D-SEL-HASH and D-SEL-JSON rulings land, the `THIRD_PARTY` list is empty and the hash and JSON-reader headers are API-only (see "Stage plan").

### Public root headers

| Header (`Zenith2/Engine/ZenithCore/`) | Declares | Specified in |
|---|---|---|
| `Zenith_Types.h` | `uint8`, `uint16`, `uint32`, `uint64`, `uint`, `sint8`, `sint16`, `sint32`, `sint64`, `sint` | `Zenith2/Docs/Design/CORE.md` → "Foundation group" |
| `Zenith_BuildInfo.h` | build readers, `Zenith_BuildPlatform`, `Zenith_BuildConfiguration`, `Zenith_SanitizerKind`, `Zenith_GetSanitizerKind` | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information" |
| `Zenith_Assert.h` | the four macros, `Zenith_FatalCode`, `Zenith_FailureKind`, `Zenith_SourceSite` | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors" |
| `Zenith_Diagnostics.h` | diagnostic output, backtrace, debugger, termination | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors" |
| `Zenith_Result.h` (+ `.inl`) | `Zenith_ErrorType`, `Zenith_GetErrorCode`, `Zenith_ErrorValue`, `Zenith_Err`, `Zenith_Result`, `ZENITH_TRY`, `ZENITH_TRY_ASSIGN`, exactly as `Zenith2/Docs/Contracts.md` → "Errors and results" declares them | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy" |
| `Zenith_ErrorCategory.h` | `Zenith_ErrorCategory` | same |
| `Zenith_NarrowCast.h` | `Zenith_NarrowCast`, `Zenith_TryNarrowCast`, `Zenith_NarrowError` | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" |
| `Zenith_Heap.h` | the heap API; `Zenith_HeapBackingState`, the heap's private backing member | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap"; `Zenith2/Docs/Design/CORE/Memory.md` → "Over-aligned side table" |
| `Zenith_Owned.h` (+ `.inl`) | `Zenith_Owned`, `Zenith_New`, `Zenith_TryNew`, `Zenith_Delete` | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Owned" |
| `Zenith_Arena.h` | arenas | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena" |
| `Zenith_Pool.h` | pools | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Pool" |
| `Zenith_FaultInjector.h` | `Zenith_FaultInjector` | `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support" |
| `Zenith_Function.h` | `Zenith_Function` | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_Function" |
| `Zenith_SlotTable.h` | `Zenith_SlotTable`, `Zenith_SlotID`, `Zenith_HandleError` | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable" |
| `Zenith_Span.h`, `Zenith_Array.h` (+ `.inl`), `Zenith_FixedArray.h`, `Zenith_InlineArray.h`, `Zenith_HashMap.h` (+ `.inl`), `Zenith_QueuePolicy.h`, `Zenith_RingBuffer.h` (+ `.inl`), `Zenith_BoundedQueue.h` (+ `.inl`), `Zenith_SpscRing.h`, `Zenith_TripleBuffer.h`, `Zenith_BitArray.h`, `Zenith_Sort.h` | the containers | `Zenith2/Docs/Design/CORE/Containers.md` |
| `Zenith_StringView.h`, `Zenith_String.h`, `Zenith_Unicode.h`, `Zenith_StringID.h`, `Zenith_Format.h` (+ `.inl`), `Zenith_NumberText.h`, `Zenith_JsonWriter.h`, `Zenith_JsonDocument.h`, `Zenith_DocumentTree.h` | text | `Zenith2/Docs/Design/CORE/Text.md` |
| `Zenith_Maths.h`, `Zenith_Transcendental.h`, `Zenith_Vector.h`, `Zenith_Matrix.h`, `Zenith_Quaternion.h`, `Zenith_Colour.h`, `Zenith_Bounds.h`, `Zenith_Hash.h`, `Zenith_Sha256.h`, `Zenith_Random.h`, `Zenith_PersistentID.h` | maths, deterministic transcendental functions and the CRT maths mode, hashing, SHA-256, randomness, identity | `Zenith2/Docs/Design/CORE/Maths.md` |
| `Zenith_Clock.h` | clocks | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" |
| `Zenith_Log.h`, `Zenith_LogRouter.h`, `Zenith_LogSinks.h` | logging | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Structured logging" |
| `Zenith_ConfigStore.h` | configuration store | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |
| `Zenith_ProfilerHooks.h` | the hook table | `Zenith2/Docs/Contracts.md` → "No mutable global state" |
| `Zenith_Multithreading.h`, `Zenith_LockDesc.h`, `Zenith_Mutex.h`, `Zenith_ConditionVariable.h`, `Zenith_Semaphore.h`, `Zenith_ReentrancyGuard.h`, `Zenith_CallbackRegistry.h`, `Zenith_OperationHandle.h`, `Zenith_NoAllocationScope.h` | threading | `Zenith2/Docs/Design/CORE/Threading.md` |
| `Zenith_MemoryGovernor.h` | the governor, `Zenith_OOMPressureFlag` | `Zenith2/Docs/Architecture.md` → "Memory governor"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` |

### Internal files

| Directory | Files |
|---|---|
| `Internal/Memory/` | `Zenith_Heap.cpp`, `Zenith_HeapBacking.h`, `Zenith_HeapBacking_Mimalloc.cpp` (after a mimalloc ruling), `Zenith_DefaultOOMHandler.cpp`, `Zenith_LeakTable.h/.cpp`, `Zenith_Arena.cpp`, `Zenith_Pool.cpp`, `Zenith_SanitizerMemory.h/.cpp` |
| `Internal/Profiling/` | `Zenith_ProfilerHooks.cpp` (`g_xProfilerHooks`), `Zenith_DebugThreadState.h/.cpp` (`g_xDebugThreadState`, the no-allocation scope functions and the hook-dispatch guard `Zenith_EnterHookDispatch`, `Zenith_LeaveHookDispatch`, `Zenith_GetHookDepth`, `Zenith_GetHookReentryCount`), `Zenith_LockOrderChecker.h/.cpp` (the checker and held-lock functions every platform threading class calls, `Zenith2/Docs/Design/CORE/Threading.md` → "Platform threading classes") |
| `Internal/Text/` | `Zenith_Unicode.cpp`, `Zenith_String.cpp`, `Zenith_StringTable.cpp`, `Zenith_Format.cpp`, `Zenith_NumberText.cpp` |
| `Internal/Json/` | `Zenith_JsonWriter.cpp`, `Zenith_JsonDocument.cpp`, `Zenith_DocumentTree.cpp`, `Zenith_JsonPositions.cpp`, `Zenith_JsonBackend.h`, `Zenith_JsonBackend_<Yyjson|Simdjson>.cpp` |
| `Internal/Hash/` | `Zenith_Hash.cpp`, `Zenith_Sha256.cpp` |
| `Internal/Maths/` | `Zenith_Matrix.cpp`, `Zenith_Quaternion.cpp`, `Zenith_Bounds.cpp`, `Zenith_Colour.cpp`, `Zenith_Random.cpp`, `Zenith_PersistentID.cpp`, `Zenith_Transcendental.cpp`, `Zenith_TranscendentalTables.h` (the committed constant and coefficient table) |
| `Internal/Zenith_CrtMathMode.h` | `Zenith_SetCrtFma3EnabledForTest`, for the CRT-independence test (`Zenith2/Docs/Design/CORE/Maths.md` → "The CRT maths mode") |
| `Internal/Zenith_ProcessLaunch.h` | the platform layer of the test-process launcher, included only by `Tests/Support/Zenith_TestProcess.cpp` (`Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes") |
| `Internal/Diagnostics/` | `Zenith_Fail.cpp`, `Zenith_Log.cpp`, `Zenith_LogRouter.cpp`, `Zenith_LogSinks.cpp`, `Zenith_BuildInfoChecks.cpp`, `Zenith_ErrorCategory.cpp` |
| `Internal/Config/` | `Zenith_ConfigSchema.cpp`, `Zenith_ConfigStore.cpp`, `Zenith_CoreConfigKeys.cpp`; `Internal/Config/Platform/{windows,android,linux}.config.json` (content fragments) |
| `Internal/Threading/` | `Zenith_Thread.cpp` (the platform-neutral parts of `Zenith_Thread`), `Zenith_CallbackRegistry.cpp`, `Zenith_OperationTable.cpp`, `Zenith_ReentrancyGuard.cpp`, `Zenith_BlockingWait.h` (`Zenith_CheckBlockingAllowed` and `Zenith_BlockingWaitScope<bProfiled>`, the blocking-wait rule every blocking primitive calls) |
| `Internal/Governor/` | `Zenith_MemoryGovernor.cpp`, `Zenith_MemoryBudgetDefaults.cpp` |
| `Internal/Generated/` | (build output only) `Zenith_MemoryBudgets.generated.h` under `${ZENITH_OUT}/generated/ZenithCore/` |
| `Internal/Zenith_CompilerSupport.h` | the one place a compiler macro may be tested (rule F11), for `[[msvc::no_unique_address]]`-style spellings `ZENITH_EXPORT_C_ENTRY` (the default-visibility attribute of the Android test entry), `ZENITH_STRICT_FP_BEGIN`/`ZENITH_STRICT_FP_END` and `Zenith_HardwareSqrt` (`Zenith2/Docs/Design/CORE/Maths.md` → "Shared evaluation rules"), with the decision ID of JR-CORE-COMPILERSUPPORT (appended by the ticket that first needs it) |

### Platform threading headers

`Zenith2/Engine/ZenithCore/Platform/Windows/Zenith_PlatformThreading.h`, `Platform/Android/Zenith_PlatformThreading.h` and `Platform/Linux/Zenith_PlatformThreading.h` declare the platform class templates (`Zenith_<Platform>Mutex_T<bool bProfiled>` and likewise for the condition variable and the semaphore) behind the six aliases `Zenith_Mutex`, `Zenith_MutexNoProfiling`, `Zenith_ConditionVariable`, `Zenith_ConditionVariableNoProfiling`, `Zenith_Semaphore` and `Zenith_SemaphoreNoProfiling`. Only the build platform's directory is on the include path ("ZenithCore target"). They are the one signposted exception of `Zenith2/Docs/Contracts.md` → "Library directory layout" [Z2-N-CONTRACT-583], and each carries its `// ZENITH-EXEMPTION` block (`Zenith2/Docs/Design/CORE/Threading.md` → "Platform threading classes").

### Platform files

| `Internal/Windows/` | `Internal/Android/` | `Internal/Linux/` |
|---|---|---|
| `Zenith_HeapBacking_Windows.cpp`, `Zenith_Thread_Windows.cpp`, `Zenith_Mutex_Windows.cpp`, `Zenith_ConditionVariable_Windows.cpp`, `Zenith_Semaphore_Windows.cpp`, `Zenith_Clock_Windows.cpp`, `Zenith_Diagnostics_Windows.cpp`, `Zenith_Terminate_Windows.cpp`, `Zenith_Entropy_Windows.cpp`, `Zenith_ProcessMemory_Windows.cpp`, `Zenith_LeakSymbols_Windows.cpp`, `Zenith_CrtMathMode_Windows.cpp`, `Zenith_ProcessLaunch_Windows.cpp` | the same set with `_Android` suffixes (no `LeakSymbols`: Android prints module offsets, [Z2-N-CONTRACT-258]) | the same set with `_Linux` suffixes |

### Tests files

`Zenith2/Engine/ZenithCore/Tests/` holds one `Zenith_<Type>_Tests.cpp` (and `_DefectTests.cpp` where the type has defect or death tests) per type named in the test tables of the split files, `Tests/Examples/` (one compiled example per contract-block `Example:` field: `Zenith_Owned_Example.cpp`, `Zenith_Array_Example.cpp`, `Zenith_HashMap_Example.cpp`, `Zenith_JsonWriter_Example.cpp`, `Zenith_ConfigStore_Example.cpp`, `Zenith_MemoryGovernor_Example.cpp`), `Tests/Fixtures/` (`Text/`, `Json/`, `Hash/`, `Random/`, `BudgetTable/`, `Transcendental/`), `Tests/Fuzz/` (`Utf8.cpp`, `JsonDocument.cpp`, `PersistentID.cpp`, `ConfigLayer.cpp`), and `Tests/Support/` (the `ZenithTestSupport` sources). `*_CompileFail.cpp` files are compiled only by their `WILL_FAIL` CTest entries, never linked into `ZenithCore_Tests`.

### ZenithTestSupport target

Built from `Tests/Support/**` ([Z2-N-CONTRACT-406]): `Zenith_TestMain.cpp` (the ordinary `main`), `Zenith_RunTestMain.cpp` and `Zenith_TestMain.h` (the shared body), `Zenith_TestEntry_SDLMain.cpp` and `Zenith_TestOutputLineBuffer.h` (the Android test entry of MOD's test APKs, `Zenith2/Docs/Design/CORE/Memory.md` → "Test main and the Android test entry"), `Zenith_TestProcess.h` and `Zenith_TestProcess.cpp` (the child-process launcher every library's tests use, `Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes"), `Zenith_CountingType.h`, `Zenith_PropertyGen.h`, `Zenith_TestFaultInjector.h`, `Zenith_FaultSweep.h`, `Zenith_FakeMemoryConsumer.h`, `Zenith_CapturingHooks.h` (a hook table that records zone begin and end, counter, lock acquire and release with the wait, blocking-wait with the wait, and log records for tests). It links ZenithCore and doctest and is linked by every `<Lib>_Tests` target.

### Benchmarks and integration suites

| Path | Purpose |
|---|---|
| `Zenith2/Benchmarks/CoreContainers/` | the container records' measurements (`Zenith_Bench_CoreContainers`) |
| `Zenith2/Benchmarks/HeapSelection/` | the D-SEL-HEAP harness (mixed sizes, cross-thread frees, spawn/destroy churn replay, W-Dense and W-Characters allocation traces) |
| `Zenith2/Benchmarks/HashSelection/` | the D-SEL-HASH harness (short keys, 4 KiB and 1 MiB inputs, collision count over 10^7 persisted-key candidates, cross-platform vector output) |
| `Zenith2/Benchmarks/JsonSelection/` | the D-SEL-JSON harness (parse and write throughput over the authoring corpus, conformance, fuzz smoke) |
| `Zenith2/Benchmarks/Budgets/CORE/` | `Budgets_CoreArenaPoolProbe` (replays a synthetic frame: 10,000 arena allocations of 16 to 4,096 bytes across three frame arenas with one reset per frame, and 50,000 pool create/destroy pairs across four slot sizes, for 600 frames; reports block overhead, peak reserved bytes and pool page fragmentation as the basis of Z2-B-CORE-004 and -005); `Zenith2/Benchmarks/Budgets/Check-Allocation.ps1` and its fixtures ([Z2-N-BUDGET-034]) |
| `Zenith2/Tests/Memory/` | the cross-system governor suite (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests") |

## Public API

<!-- informative -->
Every public declaration of ZenithCore is specified with its contract block in the split file named per header in "Library and file layout". This section states the rules that hold across the whole API and summarises each group, so a ticket can cite one heading here and one in a split file.
<!-- /informative -->

### Rules across the API

- Every fallible function returns `Zenith_Result<T, E>`, including the sticky-status reads of `Zenith_JsonWriter` (`GetStatus`, `Finish`).
- No ZenithCore declaration is `noexcept` or `[[nodiscard]]`, because no language or standard-library mechanism in ZenithCore needs either. `[[noreturn]]` marks exactly the functions that never return: `Zenith_Fail`, the two `Zenith_FailFormatted` overloads and `Zenith_TerminateProcess` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors").
- `final` appears only on a class that derives from a base: CORE's sink and test-support implementations of `Zenith_LogSink`, `Zenith_FaultInjector` and `std::streambuf`.
- A type whose contract block says `Thread affinity: IMMUTABLE` declares its data members `const` where it can. Where a required operation forbids it (assignment, default construction then fill), the Thread affinity line names the operation.
- Every public type carries one contract block of the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type"; containers carry the four container fields.
- ZenithCore has no replaceable subsystem and therefore no interface-plus-factory pair except `Zenith_FaultInjector`, `Zenith_MemoryConsumer` and `Zenith_LogSink`, which are implemented above it; objects with fallible construction use a static `Create` returning `Zenith_Result<Zenith_Owned<T>, E>` ([Z2-N-CONTRACT-222]): `Zenith_StringTable`, `Zenith_LogRouter`, `Zenith_ConfigSchema`, `Zenith_ConfigStore`, `Zenith_OperationTable`, `Zenith_BoundedQueue`, `Zenith_JsonDocument`, `Zenith_MemoryGovernor`.
- Every CORE error enumeration numbers from 1, has `Zenith_ToString` and `Zenith_GetErrorCategory`, and is listed in `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy".
- No public header includes a third-party or platform header, or an STL type. The one exemption is the per-platform threading header (`Zenith2/Docs/Contracts.md` → "Library directory layout" [Z2-N-CONTRACT-583]; "Platform threading headers").

### Foundation group

`Zenith_Types.h`, `Zenith_BuildInfo.h`, `Zenith_Assert.h`, `Zenith_Diagnostics.h`, `Zenith_Result.h`, `Zenith_ErrorCategory.h`, `Zenith_NarrowCast.h`: the headers every other header includes; they depend on nothing but the allowed standard headers and each other, in that order.

`Zenith_Types.h` is the engine's integer vocabulary. It is the one header that names the `<cstdint>` types, and engine code uses only its aliases (third-party boundaries under `Internal/` keep the third party's own types):

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Types.h
#pragma once
#include <cstddef>
#include <cstdint>

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using sint8 = std::int8_t;
using sint16 = std::int16_t;
using sint32 = std::int32_t;
using sint64 = std::int64_t;
using uint = unsigned int;      // the platform's default unsigned integer
using sint = int;               // the platform's default signed integer

static_assert(sizeof(uint8) == 1 && sizeof(uint16) == 2 && sizeof(uint32) == 4 && sizeof(uint64) == 8);
static_assert(sizeof(sint8) == 1 && sizeof(sint16) == 2 && sizeof(sint32) == 4 && sizeof(sint64) == 8);
static_assert(sizeof(uint) == 4 && sizeof(sint) == 4);   // 32-bit int on every supported platform and ABI
static_assert(static_cast<uint8>(-1) > 0 && static_cast<uint16>(-1) > 0 && static_cast<uint32>(-1) > 0
	&& static_cast<uint64>(-1) > 0 && static_cast<uint>(-1) > 0);           // unsigned
static_assert(static_cast<sint8>(-1) < 0 && static_cast<sint16>(-1) < 0 && static_cast<sint32>(-1) < 0
	&& static_cast<sint64>(-1) < 0 && static_cast<sint>(-1) < 0);           // signed
```

Each fixed-width alias is a type alias of the `<cstdint>` type, never a distinct type or a wrapper struct. A third-party header that declares its own integer names (Steamworks' `steamtypes.h` declares `uint8` … `uint64` and `int8` … `int64`) then coexists with these aliases in one translation unit, because both sides name the same underlying types and a repeated alias of the same type is legal; a distinct type would be a redefinition error there. `Test_Types_ApiSurface_CstdintAliases` asserts it with `static_assert(std::is_same_v<uint64, std::uint64_t>)` and the same for the other seven, plus `std::is_same_v<uint, unsigned int>` and `std::is_same_v<sint, int>`.

There is no other spelling: no `u_int*` alias, and no `intN_t` or `uintN_t` outside `Zenith_Types.h` and third-party calls under `Internal/`. The Hungarian type prefix of each alias is in the prefix table of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (subsection "Naming and layout").

### Memory group

`Zenith_Heap.h`, `Zenith_Owned.h`, `Zenith_Arena.h`, `Zenith_Pool.h`, `Zenith_FaultInjector.h`, `Zenith_Function.h`, `Zenith_SlotTable.h`. `Zenith_Owned<T>` is the one owning pointer; `Zenith_SlotTable<T>` is the generational store behind handle kinds whose layout is (32-bit slot, 32-bit generation).

### Container group

`Zenith_Span`, `Zenith_Array`, `Zenith_FixedArray` (exactly N elements, the layout of `T[N]`), `Zenith_InlineArray` (0 … N elements inline), `Zenith_HashMap`, `Zenith_HashSet`, `Zenith_RingBuffer`, `Zenith_BoundedQueue`, `Zenith_SpscRing` and `Zenith_TripleBuffer` (wait-free, over caller storage), `Zenith_BitArray` (a fixed-capacity bit set), `Zenith_Sort`: `Zenith2/Docs/Design/CORE/Containers.md` → "Container set".

### Text group

`Zenith_StringView`, `Zenith_String`, the Unicode functions, `Zenith_StringID` with `Zenith_StringTable`, `Zenith_FormatTo`, number text, `Zenith_JsonWriter`, `Zenith_JsonDocument`, `Zenith_DocumentTree`.

### Maths and identity group

`Zenith_Vector2/3/4`, `Zenith_Matrix3/4`, `Zenith_Quaternion`, `Zenith_Colour`, the bounds types, the deterministic transcendental functions `Zenith_Sin` … `Zenith_Sqrt` with `Zenith_ApplyCrtMathMode`, `Zenith_Hash64`, `Zenith_Hash128`, `Zenith_Hash64Stream`, `Zenith_Sha256Value`, `Zenith_Sha256Digest`, `Zenith_Sha256Stream`, `Zenith_RandomSource` and its derivations, `Zenith_PersistentID` functions.

### Threading group

`Zenith_Thread`, `Zenith_Mutex`, `Zenith_MutexNoProfiling`, `Zenith_ScopedLock`, `Zenith_ConditionVariable`, `Zenith_ConditionVariableNoProfiling`, `Zenith_Semaphore`, `Zenith_SemaphoreNoProfiling`, `Zenith_ReentrancyGuard`, `Zenith_CallbackRegistry`, `Zenith_CallbackToken`, `Zenith_OperationHandle`, `Zenith_OperationTable`, `Zenith_OperationTicket`, `Zenith_NoAllocationScope`.

### Diagnostics and configuration group

`Zenith_Log.h` (levels, categories, macros), `Zenith_LogRouter.h` (router, sink interface), `Zenith_LogSinks.h`, `Zenith_Clock.h`, `Zenith_ConfigStore.h`, `Zenith_ProfilerHooks.h`.

### Governor group

`Zenith_MemoryGovernor.h`: Architecture's declarations (including the subsystem reservation API of `Zenith2/Docs/Architecture.md` → "Subsystem reservations") plus the statistics types, fault point and reservation constructors of `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Additions to the governor API".

### Handle kinds used

| Handle | Kind (`Zenith2/Docs/Contracts.md` → "Handle kinds") | CORE's role |
|---|---|---|
| `Zenith_PersistentID` | persistent ID | declares it and its generation, text and binary forms |
| `Zenith_OperationHandle` | operation handle | declares it and implements the operation table |
| `Zenith_SlotID` | the generic generational ID wrapped by library-specific handle types | declares it |
| `Zenith_CallbackToken` | a registration token (generational, not one of the five kinds) | declares it |
| `Zenith_StringID` | a process-local value, not a handle (never stale, never serialized) | declares it |

## Data structures and formats

### In-memory layouts

| Type | Bytes | Alignment | Layout rule |
|---|---|---|---|
| `Zenith_Owned<T>` | 8 | 8 | `{T*}` ([Z2-N-CONTRACT-201]) |
| `Zenith_Span<T>` | 16 | 8 | `{T*, uint64}` |
| `Zenith_StringView` | 16 | 8 | `{const char*, uint64}` |
| `Zenith_DocumentTree` | 16 | 8 | `{Zenith_JsonDocument*, Zenith_JsonNode}` (4 bytes of tail padding) |
| `Zenith_Array<T>` | 32 | 8 | `{T*, count, capacity, Zenith_Arena*}` |
| `Zenith_String` | 32 | 8 | a `Zenith_Array<char>` |
| `Zenith_FixedArray<T, N>` | N × sizeof(T) | alignof(T) | `T[N]` |
| `Zenith_BitArray` | 24 | 8 | `{uint64*, bit count, Zenith_Arena*}` |
| `Zenith_Function<S>` | 16 | 8 | `{thunk, capture}` |
| `Zenith_SlotID` | 8 | 4 | `{slot, generation}` |
| `Zenith_StringID` | 4 | 4 | `{uint32}` |
| `Zenith_Vector2/3/4` | 8 / 12 / 16 | 4 | float members in x, y, z, w order |
| `Zenith_Quaternion`, `Zenith_Colour` | 16 | 4 | x, y, z, w / r, g, b, a |
| `Zenith_Matrix3`, `Zenith_Matrix4` | 36 / 64 | 4 | rows of vectors, row-major, column-vector convention |
| `Zenith_PersistentID` | 16 | 8 | `{high, low}` ([Z2-N-CONTRACT-065]) |
| `Zenith_Hash128` | 16 | 8 | `{low, high}` |
| `Zenith_Sha256Value` | 32 | 1 | the digest bytes, H0 first, each word big-endian |
| `Zenith_Sha256Stream` | 112 | 8 | `{uint32 state[8], uint64 total bytes, uint8 block[64], uint32 block bytes}` |
| `Zenith_RandomSource` | 24 | 8 | `{seed, stream, counter}` |
| `Zenith_SpscRing<T>` | 192 | 64 | `{T*, mask}`, then the producer line `{write, cached read, dropped}`, then the consumer line `{read, cached write, high water}` |
| `Zenith_TripleBuffer<T>` | 128 | 64 | `{T*, back, front}`, then the shared line `{middle, published, superseded}` |
| `Zenith_ReentrancyGuard` | 8 | 4 | `{atomic thread ID, depth}` |
| `Zenith_OperationHandle` | 16 | 8 | `{table*, slot, generation}` |

### File and text formats

| Format | Specified in |
|---|---|
| JSON writer output (pretty and compact), byte for byte | `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer" |
| JSON reader input rules and limits | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| configuration layer files and the configuration dump | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |
| the generated budget header | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| persistent-ID text (36 characters) and binary (16 bytes, little-endian words) | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions" |
| the random draw block (24 bytes little-endian) | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| the log record (the structured-log schema) and console line | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Structured logging" |
| the assertion report's first line | `Zenith2/Docs/Contracts.md` → "Errors and results" ([Z2-N-CONTRACT-470]) |
| SHA-256 digests (32 bytes, FIPS 180-4 byte order) and their hexadecimal text (64 lowercase characters) | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256" |
| fixtures: `utf8_cases.json`, `doubles.json`, `writer_golden_*.json`, the JSONTestSuite corpus, hash `vectors.json`, random `golden.json`, budget-table fixtures, the transcendental golden vectors `Transcendental/Zenith_TranscendentalGolden.inl` (hand-copied from `Zenith2/Docs/Design/CORE/Maths.md` → "Golden vectors") | the test tables of each split file |

CORE writes no cooked format and no save format; every format above is text or a generated header, and none depends on a C++ object's layout (Z2-D-038).

## Algorithms and state machines

| Algorithm or state machine | Complexity | Failure behaviour | Specified in |
|---|---|---|---|
| heap allocation with OOM retries | O(1) plus backing | retry, then `FATAL_CODE_OUT_OF_MEMORY` | `Zenith2/Docs/Design/CORE/Memory.md` → "Emergency reserve and the default handler" |
| Windows over-aligned side table | O(1) expected | growth failure fails the allocation | `Zenith2/Docs/Design/CORE/Memory.md` → "Over-aligned side table" |
| arena allocation, marker reset | O(1) amortised | cap → error or fatal | `Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena"; `Zenith2/Docs/Design/CORE/Memory.md` → "Arena internals" |
| pool page and quarantine | O(1); page lookup O(log pages) | cap → error | `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals" |
| slot table with FIFO reuse and retirement | O(1) | cap → error | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable" |
| array growth with the strong guarantee | amortised O(1) push | unchanged on failure | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array" |
| hash map: dense entries, linear probing, backward-shift removal | expected O(1) | unchanged on failure | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet" |
| bounded MPMC queue | O(1) lock-free | full policy | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue" |
| introsort and bottom-up merge sort | O(n log n) | scratch failure leaves input unchanged | `Zenith2/Docs/Design/CORE/Containers.md` → "Sorting and searching" |
| UTF-8 validation | O(n) | error code and offset | `Zenith2/Docs/Design/CORE/Text.md` → "Unicode functions" |
| string interning | expected O(length) | caps → error | `Zenith2/Docs/Design/CORE/Text.md` → "String interning" |
| JSON parse (backend, conversion, duplicates, positions) | O(n + Σ k log k) | error record with position | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| JSON Pointer resolution over a document subtree | O(pointer bytes + members scanned per object visited), no allocation | access error, document unchanged | `Zenith2/Docs/Design/CORE/Text.md` → "Pointer resolution steps" |
| configuration resolution | O(1) per get, O(layers) per apply | diagnostics, lower layer kept | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |
| failure procedure | — | terminates | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors" |
| log routing and drain | O(sinks) per record | ring drops oldest | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Structured logging" |
| callback dispatch and unregistration | O(entries) | deferred removal | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_CallbackRegistry implementation" |
| operation status machine | O(1) | — | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" |
| SHA-256 compression, padding and streaming | O(n), 64 rounds per 64-byte block | a length above 2^61 − 1 bytes is `ZENITH_CHECK` | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256" |
| counter-based random draws and derivations | O(1) per draw | — | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| deterministic transcendental evaluation (Cody–Waite reduction, Taylor kernels, two-word logarithm and power) | O(1), no allocation | out-of-domain → canonical NaN; subnormals read and returned as signed zeros | `Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions" |
| SPSC ring push and pop, triple-buffer publish and acquire | O(1) per element, wait-free | full ring: drop the newest and count it; superseded publish: counted | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_SpscRing", "Zenith_TripleBuffer" |
| test child-process launch, poll, kill | O(output) | error result; a child never outlives its object | `Zenith2/Docs/Design/CORE/Memory.md` → "Test child process steps" |
| memory-pressure level state machine | O(consumers) per frame | — | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation" |
| lock-free transition reservation | O(1) expected | refusal | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm" |
| lock-free subsystem reservation | O(1) expected | refusal (`MEMORY_ERROR_SUBSYSTEM_REFUSED`) | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Subsystem reservation algorithm" |
| budget-table generation | O(rows) at configure | configure error | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_Heap` | `ANY_THREAD` |
| `Zenith_Arena`, `Zenith_Pool`, `Zenith_SlotTable`, `Zenith_Array`, `Zenith_InlineArray`, `Zenith_HashMap`, `Zenith_HashSet`, `Zenith_RingBuffer`, `Zenith_BitArray`, `Zenith_String`, `Zenith_JsonWriter`, `Zenith_JsonDocument`, `Zenith_DocumentTree`, `Zenith_RandomSource`, `Zenith_Hash64Stream`, `Zenith_Sha256Stream` | `OWNER_THREAD` |
| `Zenith_Span`, `Zenith_StringView`, `Zenith_Function`, maths types, IDs | `IMMUTABLE` |
| the deterministic transcendental functions | `ANY_THREAD` (pure; usable on the audio device thread); `Zenith_ApplyCrtMathMode` `MAIN_THREAD` before any other thread |
| `Zenith_SpscRing`, `Zenith_TripleBuffer` | one producer and one consumer at a time, handed over by happens-before; `GetStats` and `GetCapacity` `ANY_THREAD` |
| `Zenith_NoAllocationScope` | `ANY_THREAD`, affecting the calling thread only |
| `Zenith_TestProcess` (test support) | `OWNER_THREAD` |
| `Zenith_BoundedQueue` (its semaphores are `Zenith_SemaphoreNoProfiling`), `Zenith_StringTable`, `Zenith_Mutex`, `Zenith_MutexNoProfiling`, `Zenith_ConditionVariable`, `Zenith_ConditionVariableNoProfiling`, `Zenith_Semaphore`, `Zenith_SemaphoreNoProfiling`, `Zenith_CallbackRegistry` (dispatch on one thread at a time), `Zenith_OperationTable`, `Zenith_LogRouter` (records), `Zenith_TransitionReservation` and `Zenith_SubsystemReservation` (one thread at a time per object) | `ANY_THREAD` |
| `Zenith_ConfigSchema` | `MAIN_THREAD` until `Freeze`, then `IMMUTABLE` |
| `Zenith_ConfigStore` | `MAIN_THREAD` |
| `Zenith_MemoryGovernor` | `MAIN_THREAD` except `ReserveTransitionPeak`, `ReserveSubsystemBytes`, `ReportOSPressure`, `GetReservedPeakBytes`, `GetReservedSubsystemBytes` (`ANY_THREAD`) |

### Frame-graph placement

ZenithCore declares no frame-graph node. Its per-frame work runs in named steps of the frame prologue on the main thread (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue", [Z2-N-ARCH-437]):

| Prologue step | CORE call made by the composition root |
|---|---|
| 2, after `Zenith_SimulationClock::Advance` | `Zenith_LogRouter::SetFrameIndex(k)` |
| 3 (queued user-layer writes) | `Zenith_ConfigStore::CommitStagedUserValues`, through PLAT's `Zenith_Config` |
| 4, before `Update` | `Zenith_MemoryGovernor::ReportProcessUsage`; publication of the `core.heap.*` and `core.strings.*` counters |
| 4 | `Zenith_MemoryGovernor::Update` |

None of them reads or writes ECS state, so they declare no access. The placement is `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue", steps 2 to 4.

### Construction and shutdown order

| Object | Created | Destroyed |
|---|---|---|
| `Zenith_OOMPressureFlag` | `main`, before the heap | after the heap |
| root `Zenith_Heap` | `main`, installed first ([Z2-N-CONTRACT-142]) | `main`, last (step 12 of the shutdown table) |
| `Zenith_LogRouter` | `main`, directly after the heap, with the build-time ring size `uLOG_RING_BYTES_DEFAULT` (allocations only, no thread); `FillHooks` before `Zenith_InstallProfilerHooks`; at startup step 7 the composition root applies the levels, adds the sinks, calls `ResizeRing` with `log.ringBytes` and then `StartDrainThread` (steps 1b and 7 of `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"; router detail in `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook") | `main`, after ZenithRuntime, before the hooks are uninstalled (joins `Zenith.LogDrain`) |
| `Zenith_StringTable` | ZenithRuntime, before the type registry (step 10 needs interned names) | ZenithRuntime, after every world and the type registry |
| `Zenith_ConfigSchema`, `Zenith_ConfigStore` | ZenithRuntime at step 6, keys registered by every library's registration function | after every subsystem that read them |
| `Zenith_MemoryGovernor` | ZenithRuntime at step 8, then `BindOOMPressureFlag` | after every consumer has unregistered |

### Lock-order entries

CORE owns the rows `uLOCK_LEVEL_CALLBACK_REGISTRY` (350), `uLOCK_LEVEL_HEAP_OOM` (900), `uLOCK_LEVEL_LOG_SINKS` (920) and `uLOCK_LEVEL_HEAP_LEAK_TABLE` (990) of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", and adds 380, 850, 930 and 980 (`Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering").

### Cancellation and shutdown

- CORE's only thread is the log router's drain thread, joined by the router; job-worker and render-submit threads are JOBS's.
- Operations complete (succeeded, failed or cancelled) before their table is destroyed; shutdown step 4 of the shutdown table guarantees it.
- A blocked `Zenith_BoundedQueue::PopWait` returns false after `WakeAllWaiters`, which owners call before joining their consumer threads.

### Error enumerations

CORE defines `Zenith_AllocError` (Contracts), `Zenith_NarrowError` (Contracts), `Zenith_ThreadError`, `Zenith_QueueError`, `Zenith_HandleError`, `Zenith_StringError`, `Zenith_UnicodeError`, `Zenith_ParseError`, `Zenith_JsonReadErrorCode` (in the record `Zenith_JsonReadError`), `Zenith_JsonAccessError`, `Zenith_JsonWriteError`, `Zenith_ConfigError`, `Zenith_RandomError`, `Zenith_PersistentIDError`, `Zenith_OperationError`, `Zenith_MemoryErrorCode` (in the record `Zenith_MemoryError`) and the base taxonomy `Zenith_ErrorCategory`; meanings and categories are `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy".

### Luau errors and asserts

CORE code never raises a Luau error: it is below ZenithScript. No CORE function throws (exceptions are disabled outside the Luau firewall, [Z2-N-CONTRACT-474]), so every CORE function is safe to call from a binding without a `noexcept` declaration; every CORE function that takes data a script can control (strings for interning, JSON, configuration values, numbers as text) returns `Zenith_Result` and never asserts on that data, so the binding converts the error into a controlled Luau error ([Z2-N-CONTRACT-473]). Asserts in CORE guard programmer errors alone: out-of-range indices, wrong-type configuration getters, stale IDs that were never issued, lock-order violations.

## Diagnostics and profiling

### Counters

| Counter | Source |
|---|---|
| `<queue name>.full`, `.dropped`, `.high_water` | every `Zenith_RingBuffer` and `Zenith_BoundedQueue` ([Z2-N-CONTRACT-345]) |
| `<arena name>.used_bytes`, `.reserved_bytes`, `.high_water_bytes` | arena owners, each frame |
| `core.heap.live_bytes`, `core.heap.live_allocations`, `core.heap.peak_bytes`, `core.heap.failed_allocations` | the composition root publishes `Zenith_GetRootHeap().GetStats()` each frame |
| `core.strings.count`, `core.strings.text_bytes` | the composition root publishes `Zenith_StringTable::GetStats()` each frame |
| `log.router.full`, `log.router.dropped`, `log.router.high_water` | the log router's ring ([Z2-N-CONTRACT-345]) |
| `memory.*` | the governor (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Counters") |

### Log categories

`core.memory`, `core.threading`, `core.config`, `core.json`, `core.governor`, `core.strings`, `core.diagnostics` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Structured logging").

### On-screen profiler rows

The memory panel (Z2-I-DEVT-06, DEVT) shows the heap counters, every arena's counters, the governor's level, process and device-local usage, each subsystem's resident bytes against its budget with the pending flag, the unattributed residual, the reserved transition peaks, each subsystem's outstanding subsystem reservations and the refusal counts. The configuration panel shows the store's dump ([Z2-N-ARCH-937]). DEVT owns both panels; CORE owns the data.

### Debug views and frame dumps

The configuration dump (`--config-dump`), the Debug leak report at shutdown ([Z2-N-CONTRACT-244]), the log ring (`Zenith_LogRingSink::CopyRecentLines`, attached to crash reports by DEVT), and the governor's statistics snapshot (`GetStats`) are CORE's debug outputs; CORE contributes no GPU frame-dump entry.

## Authoring and cooking

CORE has no editor surface and no importer. Its authoring-side responsibilities are:

- the JSON writer's byte format and the reader's round-trip guarantees, which every authoring document's canonical serialization (SCENE, ASSET) is built on;
- the configuration layer file format, which project settings (an authoring-document type, [Z2-N-CONTRACT-552]) use, with key renames migrated by the authoring-document migration framework (SCENE and ASSET, Stage 2);
- ZenithCore's configuration fragments `Internal/Config/Platform/<platform>.config.json`, which the cook merges into `content:/Config/Platform/<platform>.config.json` (the merge step is the cook's, ASSET);
- the generated budget table, produced at configure time from `Zenith2/Docs/Budgets.md`, not by the cook.

No CORE type is migrated at run time; CORE formats are text with explicit versions where they are persisted by other areas.

## Failure handling

| Failure | Defined behaviour | Requirement area |
|---|---|---|
| general-heap allocation failure (`TryAllocate`, `Zenith_TryNew`, container `Try` forms) | `ALLOC_ERROR_OUT_OF_MEMORY`, state unchanged | ownership, containers |
| general-heap allocation failure (non-`Try`) | OOM handler: release reserve, raise governor CRITICAL through the relay, retry up to `m_uMaxOOMRetries`, then `FATAL_CODE_OUT_OF_MEMORY` | allocation failure |
| failure inside the OOM handler | immediate `FATAL_CODE_OUT_OF_MEMORY` | allocation failure |
| arena or pool at its cap | `ALLOC_ERROR_CAPACITY_EXCEEDED` (`Try`), `FATAL_CODE_ARENA_EXHAUSTED` (arena non-`Try`) | ownership |
| a bounded queue full | its declared policy | containers |
| corrupt or malicious JSON, configuration, UTF-8, persistent-ID text, number text | `Zenith_Result` error with position; never an assert; parser limits enforced; fuzzed | text, configuration |
| configuration value rejected | diagnostic and warning; lower layer kept | configuration |
| required configuration key missing | startup exit code 2 naming the key | configuration, governor |
| bad memory budgets | `MEMORY_ERROR_BAD_BUDGETS`, startup exit code 2 | governor |
| transition peak exhausted | `MEMORY_ERROR_PEAK_REFUSED`; the caller defers or fails its request | governor |
| subsystem budget exhausted for a reservation (`ReserveSubsystemBytes`) | `MEMORY_ERROR_SUBSYSTEM_REFUSED` with the subsystem and the available bytes; nothing is reserved; the caller fails or defers as its own design states (SCENE: `SCENE_ERROR_MEMORY_REFUSED` before decoding) | governor |
| a subsystem reservation still held at governor destruction | logged in `core.governor` with its subsystem and bytes, then `ZENITH_CHECK` ([Z2-N-ARCH-826]) | governor |
| OS low-memory signal | CRITICAL (Android) or WARNING (Windows); consumers shed | governor |
| entropy unavailable | `RANDOM_ERROR_ENTROPY_UNAVAILABLE`; the caller (editor, cook) reports and refuses to mint IDs | randomness |
| thread creation failure | `THREAD_ERROR_CREATE_FAILED`; the factory that asked unwinds | threading |
| log router cannot be created (`Zenith_LogRouter::Create` at startup step 1b) | `main` writes `Zenith2: log router allocation failed` through `Zenith_WriteDiagnosticLine`, installs no hook, uninstalls the root heap and returns exit code 4 (`Zenith2/Docs/Design/MOD.md` → "Player headers") | diagnostics |
| governor cannot be allocated (`Zenith_MemoryGovernor::Create` returns `MEMORY_ERROR_OUT_OF_MEMORY` at step 8) | the composition root unwinds what it built and returns the step's exit code (`Zenith2/Docs/Design/MOD.md` → "Player headers") | governor |
| log drain thread cannot start (`Zenith_LogRouter::StartDrainThread` at startup step 7) | fatal: the composition root writes `ZENITH STARTUP: log drain thread did not start: <error>` through `Zenith_WriteDiagnosticLine` and raises `FATAL_CODE_LOGGING_UNAVAILABLE`; the fatal path outputs the buffered records and the fatal record synchronously to the registered sinks (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router") | diagnostics |
| log ring resize cannot allocate (`ResizeRing` at step 7) | `ALLOC_ERROR_OUT_OF_MEMORY`; the 1 MiB bootstrap ring and its records are kept, a warning is logged, and startup continues | diagnostics |
| lock-order violation | `FATAL_CODE_LOCK_ORDER` before the deadlock | threading |
| a transcendental argument outside its domain (`|x| > 2^20` for the trigonometric functions, `|x| > 1` for Asin and Acos, a negative argument for the logarithms and Sqrt, a negative base with a non-integer exponent) | the canonical quiet NaN; nothing asserts, so callers clamp or check `Zenith_IsFinite` | maths |
| an allocation, or a free, on a thread inside `Zenith_NoAllocationScope` | `ZENITH_ASSERT` naming the scope wherever `ZENITH_ASSERTS_ENABLED=1`; nothing in other configurations | threading, ownership |
| a full `Zenith_SpscRing` | `TryPush` returns false and `PushSpan` copies what fits; both count the rest as dropped (DROP_NEWEST); the owner publishes `.dropped` | containers |
| a test child process cannot launch, times out or floods its output | `Zenith_TestProcessError` to the test, which fails; a timed-out or flooding child is killed and reaped | tests |
| programmer error | `ZENITH_ASSERT`/`ZENITH_CHECK`: report, backtrace, fatal log record, crash-handler termination | diagnostics |
| recursive failure during a report | immediate termination with the recursive-failure line | diagnostics |
| log ring overflow | oldest records dropped and counted | diagnostics |
| leaks at shutdown | Debug report; test executables exit 3 | ownership |
| device loss | not handled by CORE (`FATAL_CODE_DEVICE_LOST` is raised by RHI per `Zenith2/Docs/Rendering.md` → "Recovery") | — |
| cancellation | operations release what they hold on their executor thread before reporting `CANCELLED` | threading |

## Test strategy

### Test files and gate steps

| Suite | Location | Gate step and preset |
|---|---|---|
| unit tests (default suite) | `Zenith2/Engine/ZenithCore/Tests/*_Tests.cpp` | `Test` over `win-debug` and `win-release`; the `android-arm64` build compiles them from Stage 2 |
| death tests (`z2-death-check`, `z2-death-assert`) | `*_DefectTests.cpp` | `Test`, each as its own CTest entry ([Z2-N-CONTRACT-413]) |
| sanitizer defect tests (`z2-sanitizer-defect`) | `Zenith_Arena_DefectTests.cpp`, `Zenith_Pool_DefectTests.cpp`, `Zenith_Heap_DefectTests.cpp` | `Sanitizers -Preset win-asan` (change-triggered by the `Zenith2Memory` category); HWASan rows on the S24 in each stage acceptance ([Z2-N-CONTRACT-253]) |
| stress (`z2-stress`) | the stress rows of each test table | `Test`; the concurrency rows also `Sanitizers -Preset linux-tsan` ([Z2-N-CONTRACT-357]) |
| compile-fail | `*_CompileFail.cpp` | `Test` (CTest `WILL_FAIL` compile entries) |
| fuzz | `Tests/Fuzz/*.cpp` (`Zenith_Fuzz_<Stem>`) | `win-asan` 60-second smoke in `Sanitizers`; full duration in stage acceptance ([Z2-N-ARCH-952]) |
| examples | `Tests/Examples/*.cpp` | `Test` ([Z2-N-CONTRACT-421]) |
| cross-system governor suite | `Zenith2/Tests/Memory/` | `Test` once the runtime exists (Stage 2), extended through Stage 4 |
| benchmarks and harness smokes | `Zenith2/Benchmarks/*` `--smoke` | `Test` |
| the budget-table generator fixtures | `Tests/Fixtures/BudgetTable/` via `cmake -P` | `Test` |

The named tests, with the case each asserts, are the test tables of the split files: `Zenith2/Docs/Design/CORE/Memory.md` → "Memory tests", `Zenith2/Docs/Design/CORE/Containers.md` → "Container tests", `Zenith2/Docs/Design/CORE/Text.md` → "Text tests", `Zenith2/Docs/Design/CORE/Maths.md` → "Maths tests", `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Diagnostics tests", `Zenith2/Docs/Design/CORE/Threading.md` → "Threading tests", `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests", plus the Contracts-mandated rows of `Zenith2/Docs/Contracts/Memory.md` → "Required intentional-defect tests" and the `Zenith_NarrowCast` tests of [Z2-N-CONTRACT-431].

### Rules every CORE test follows

- Test names follow `"ZenithCore/<Type>/<Behaviour>"` with ID `Test_<Type>_<Behaviour>` ([Z2-N-CONTRACT-411]); randomness prints its seed ([Z2-N-CONTRACT-409]).
- Every `Try` form has a failure test driven by heap fault injection; every factory has its fault sweep ([Z2-N-CONTRACT-341]); for a factory not named `Create`, `<Create>` in the test name `Test_<Type>_<Create>_FailsAt<Step>_Unwinds` is the factory's name (`Zenith2/Docs/Design/CORE/Memory.md` → "Factory fault points").
- No test pins a count ([Z2-N-CONTRACT-408]); golden fixtures pin bytes, which is a format contract, not a count.
- Handle tests run in every preset including `win-release` ([Z2-N-CONTRACT-096]).
- Every test of ZenithCore's allocators, containers and threading (the `Zenith2Memory` category's ZenithCore paths) carries the CTest label `memory`, so the category's `Test -Label memory` line selects it; the labelling mechanism is `zenith_add_library`'s (MOD and SHIP).

## Budget rows

### Rows owned

| Row | Name | Status | Used by |
|---|---|---|---|
| Z2-B-CORE-001 | S24 process RSS (3.0 GB) | adopted | `memory.budget.processBytes` on Android; pending-row fill value |
| Z2-B-CORE-002 | Windows process RAM (8 GB) | adopted | `memory.budget.processBytes` on Windows and Linux |
| Z2-B-CORE-003 | Windows VRAM (6 GB) | adopted | `memory.budget.deviceLocalBytes` |
| Z2-B-CORE-004 | General heap unattributed residual | pending (Stage 1 per-system budget ticket for CORE) | the residual of "Accounting"; `MEMORY_SUBSYSTEM_OTHER` |
| Z2-B-CORE-005 | Frame and graph arenas | pending (same) | arena owners' consumers; `MEMORY_SUBSYSTEM_OTHER` |
| Z2-B-CORE-006 | Streaming double-residency peak | pending (same) | `memory.peak.streamingDoubleResidencyBytes` |
| Z2-B-CORE-007 | World-transition peak | pending (same) | `memory.peak.worldTransitionBytes` |
| Z2-B-CORE-008 | Texture-swap peak | pending (same) | `memory.peak.textureSwapBytes` |
| Z2-B-CORE-009 | Memory pressure warning threshold | pending (same) | `memory.threshold.warningPercent` |
| Z2-B-CORE-010 | Memory pressure critical threshold | pending (same) | `memory.threshold.criticalPercent` |
| Z2-B-CORE-011 | Memory allocation reserve fraction | pending (same) | the allocation check of every memory bound ([Z2-N-BUDGET-026]); not read at run time |

### Rows measured against or consumed

The subsystem budgets summed by the generated budget table (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"): exactly the rows of `Zenith2/Docs/Architecture.md` → "Memory governor" (table "Budgets", [Z2-N-ARCH-823]), which are RHI, GEOM, RPROD, ADVGFX, VFX, STREAM, AUDIO, SCRIPT, EDIT, ASSET, UI, CINE, ECS, SCENE, PHYS, AI, ANIM, SAVE and DEVT rows, each produced by its area's Stage 1 per-system budget ticket.

### Pending row needs

<!-- elaboration -->
Two capacities CORE sizes are not yet budget rows; each is recorded here as pending and in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` for the Budgets owner, with the producer named:

| Row needed (descriptive name) | Unit | Member of bound | Producer |
|---|---|---|---|
| "String intern table capacity" (strings and bytes) | count, MB | `MEM-WIN-RAM`, `MEM-S24` (within `MEMORY_SUBSYSTEM_OTHER`) | Stage 1 per-system budget ticket for CORE |
| "Log router ring bytes" | MB | the same | Stage 1 per-system budget ticket for CORE |

Until they exist, `strings.maxStrings` and `strings.maxTextBytes` have no default and are `m_bRequired`, supplied by ZenithCore's platform configuration fragments that the same ticket writes. `log.ringBytes` defaults to `uLOG_RING_BYTES_DEFAULT` (1 MiB), the build-time size of the ring from startup step 1b to step 7, because the router is created before configuration exists; the "Log router ring bytes" row, once allocated, must be at least that constant, and the ticket sets `log.ringBytes` in the fragments when the row differs from it.

## Stage plan

### Stage 1 decision harness and budget tickets

| Ticket | Kind | Governs |
|---|---|---|
| ownership, allocator and container contracts: the justification records of `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records" and JR-CORE-SLOTTABLE, JR-CORE-FUNCTION, JR-CORE-STRING, JR-CORE-CHARCONV | decision (records appended with each landing ticket) | every container ticket (Z2-D-041) |
| D-SEL-HEAP harness (`Zenith2/Benchmarks/HeapSelection/`) and ruling | harness, decision (`needs-human`) | the non-sanitizer heap backing |
| D-SEL-HASH harness (`Zenith2/Benchmarks/HashSelection/`) and ruling | harness, decision | `Zenith_Hash64`, hashed containers, string table, random source, persistent IDs, every consumer's persisted keys |
| D-SEL-JSON harness (`Zenith2/Benchmarks/JsonSelection/`) and ruling | harness, decision | the JSON reader backend, `Zenith_ParseDouble`, configuration layer parsing |
| Stage 1 per-system budget harness for CORE (`Check-Allocation.ps1` and fixtures, `Budgets_CoreArenaPoolProbe`) and the Stage 1 per-system budget ticket for CORE | harness, budget | Z2-B-CORE-004 … -011, the two pending row needs, the governor tuning keys |

### Stage 1 pull-forward

Per `Zenith2/Docs/Roadmap.md` → "Pull-forward rule", every CORE ticket below depends on no unresolved Stage 1 decision and is scheduled in `Stage 1 — Architecture validation` with fix version `Z2 Stage 2`. Pull-forward is a sprint property the tickets record; the requirements these tickets cover keep stage S2, the stage whose acceptance they serve. API-only slices land first so consumers compile against them:

| Order | Ticket (kind) | Depends on no open decision because |
|---|---|---|
| 1 | foundation headers: `Zenith_Types.h`, `Zenith_BuildInfo.h`, `Zenith_Assert.h`, `Zenith_Diagnostics.h`, `Zenith_Result.h`, `Zenith_ErrorCategory.h`, `Zenith_NarrowCast.h` (implementation) | settled contracts |
| 2 | root heap on the system backing, `Zenith_Owned`, arenas, pools, sanitizer instrumentation and the defect tests, the leak table, fault injection (implementation) | the system backing is required in sanitizer builds whatever D-SEL-HEAP rules |
| 3 | `Zenith_ProfilerHooks.h`, threading primitives, the lock-order checker, the reentrancy guard, the callback registry (implementation) | settled contracts |
| 4 | `Zenith_Hash.h`, `Zenith_HashMap.h`, `Zenith_StringID.h`, `Zenith_Random.h`, `Zenith_PersistentID.h`, `Zenith_JsonDocument.h` (API-only: declarations and contract blocks, no implementation) | declarations do not depend on the rulings |
| 5 | `Zenith_Span`, `Zenith_Array`, `Zenith_FixedArray`, `Zenith_InlineArray`, `Zenith_RingBuffer`, `Zenith_BoundedQueue`, `Zenith_BitArray`, `Zenith_Sort`, `Zenith_Function`, `Zenith_SlotTable` (implementation, records appended) | no hash involved |
| 6 | `Zenith_StringView`, `Zenith_String`, Unicode functions, formatting, integer number text, `Zenith_JsonWriter` (implementation) | the writer is Zenith code on both D-SEL-JSON paths |
| 7 | maths types, bounds, colour, clocks (implementation) | settled conventions |
| 8 | the log router and CORE sinks (implementation) | settled contracts |
| 9 | `Zenith_OperationHandle` and table (implementation) | settled contracts |
| 10 | `Zenith_MemoryGovernor.h` (API-only) | declarations are Architecture's |
| 11 | `Zenith_Sha256.h` with its implementation and tests | SHA-256 is fixed by FIPS 180-4 and outside D-SEL-HASH (`Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH") |
| 12 | `Zenith_Transcendental.h` (API-only), then the trigonometric and inverse trigonometric functions, the exponential, logarithm, power and cube-root functions, and the CRT maths mode with the test main's call (implementation), before CORE's own maths implementations and ANIM's Stage 2 screw power | fixed algorithms and committed constants, ruling X314-FMA3; no decision |
| 13 | `Zenith_SpscRing.h`, `Zenith_TripleBuffer.h` and `Zenith_NoAllocationScope.h` (API-only), then the two channels with their provisional records and the no-allocation check (implementation), before DEVT's Stage 2 profiler rings and AUDIO's Stage 3 real-time chain | AUDIO's AU6 specification, ruling Q80; the records' measurement joins the container ruling |
| 14 | the test child-process launcher in `ZenithTestSupport` with ZenithCore's platform bodies (implementation), before SAVE's and SCENE's Stage 2 child-process tests | ruling X349 (a); no decision |

### Stage 2

| Ticket | Blocked by |
|---|---|
| `Zenith_Hash64` implementation, then `Zenith_HashMap`/`Zenith_HashSet`, `Zenith_StringTable`, `Zenith_RandomSource`, persistent-ID generation | the D-SEL-HASH ruling. The `Zenith_Hash64` implementation itself is scheduled in the Stage 1 sprint once the ruling lands, behind a revalidation of D-SEL-HASH alone, because GRAPH's pulled-forward structure hash and GRAPH's Stage 1 budget measurement stream through `Zenith_Hash64Stream`; the heap, JSON, budget and container-record rulings do not govern it |
| `Zenith_JsonDocument` with the ruled backend; `Zenith_ParseDouble`; the configuration schema and store | the D-SEL-JSON ruling |
| `Zenith_DocumentTree` and its pointer resolution | `Zenith_JsonDocument`'s accessors and mutators (no ruling of its own; the D-SEL-JSON ruling reaches it only through them) |
| the mimalloc backing (only when ruled) | the D-SEL-HEAP ruling |
| the budget-table generator and `Zenith_ReadMemoryBudgets`; the governor core (levels from budgets and OS signals, hysteresis, reservations, accounting, counters, forced-level unit tests) | the Stage 1 per-system budget ticket for CORE (tuning keys and thresholds); `Zenith_ConfigStore` |
| `Tests/Memory/` suite skeleton with the unit fake consumers and the first real consumers that land in Stage 2 | the runtime composition (MOD) |
| the integration scenarios of the transcendental functions, the real-time channels under a no-allocation scope and the test child-process launcher in `Zenith2/Tests/Core/` | the Stage 1 implementations of rows 12 to 14 of "Stage 1 pull-forward"; no ruling |

### Stage 3

| Ticket | Blocked by |
|---|---|
| the Android test entry: `Zenith_RunTestMain` split out of the test main, `SDL_main` in `ZenithTestSupport`, the tagged output route and `Zenith_WriteTaggedDiagnosticLine` (`Zenith2/Docs/Design/CORE/Memory.md` → "Test main and the Android test entry"), scheduled in the sprint of its first consumer, MOD's test APKs | the final test main (leak exit 3) and `Zenith_CompilerSupport.h` (Stage 1); no ruling and no budget row |
| the subsystem reservations of [Z2-N-ARCH-825] and [Z2-N-ARCH-826]: `ReserveSubsystemBytes`, `Zenith_SubsystemReservation`, their pressure accounting, counters and unit tests (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Subsystem reservation algorithm"), scheduled in the sprint of their first consumer, SCENE's preparation reservation | the governor's transition reservations and counters (Stage 2); no ruling and no budget row of its own |
| the booted-runtime case `Test_MemoryGovernor_SubsystemReservation_BootedRuntime` in `Zenith2/Tests/Memory/` | the subsystem reservations; the Stage 2 governor integration in a booted runtime |

### Stage 4

Shedding for every subsystem present (Z2-I-CORE-26) and the three transition peaks in real use (Z2-I-CORE-27 to -29) are each subsystem's tickets; CORE's Stage 4 tickets extend `Tests/Memory/` with the cross-system forced-level and transition-peak cases once streaming, audio, scripts, editor caches and assets have landed their shedding, and run the S24 memory-pressure device step (`needs-human`, `needs-gpu` with `gpu-reason:requires-graphics-test`) at the Stage 4 acceptance.

### Stages 3 5 6 and 7

Apart from the subsystem reservations and the Android test entry of "Stage 3", no CORE feature is scheduled in Stages 3, 5, 6 or 7. Each stage acceptance re-runs the HWASan defect rows on the S24 and the full-duration CORE fuzz targets; Stage 6 includes CORE's fuzz evidence in the security hardening evidence ([Z2-N-ARCH-960]).

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-040 | ownership, the three tiers, `Zenith_Owned`, generational handles, the leak table, sanitizer rules |
| Z2-D-041 | the container set and its justification records, the allowed standard headers |
| Z2-D-045 | `Zenith_Result`, asserts, no exceptions |
| Z2-D-046 | no mutable global state beyond the root heap and the profiler hooks (and the log router reached through them) |
| Z2-D-047 | one contract block per public type |
| Z2-D-011 | sanitizer presets and the system backing in them |
| Z2-D-019 | determinism: hashed-container order, string IDs, maths, randomness |
| Z2-D-038 | formats never depend on C++ layout |
| Z2-D-012 | the memory totals the governor enforces |
| Z2-D-084 | each selection split into a harness ticket and a ruling ticket |
| Z2-D-097 | rulings and justification records appended to `Zenith2/Docs/DecisionLog.md` in the ticket's own diff |

### Open decisions

| Decision | Stage | What it changes in CORE |
|---|---|---|
| D-SEL-HEAP | 1 | `uZENITH_HEAP_RULED_BACKING`; whether `Zenith_HeapBacking_Mimalloc.cpp` and the mimalloc manifest entry exist |
| D-SEL-HASH | 1 | the path of `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64" (XXH3-64/XXH3-128 or XXH64); the vector fixtures. It never changes `Zenith_Sha256` (`Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH") |
| D-SEL-JSON | 1 | the one `Zenith_JsonBackend_<Name>.cpp` and its manifest entry |
| D-SEL-CRASH | 2 | nothing in CORE's code: termination already reaches any installed handler |
| D-SEL-FBX | 4 | nothing in CORE: the engine convention import converts into is fixed here |

### Selection decisions CORE owns

| Selection | Candidates | Criteria | Acceptance rule |
|---|---|---|---|
| D-SEL-HEAP | the system allocator; mimalloc | throughput and fragmentation on the workload allocation traces, S24 RSS, sanitizer compatibility, maintenance | `Zenith2/Docs/DecisionLog.md` → "Open decisions" (entry D-SEL-HEAP): mimalloc only when it improves allocation throughput or p95 frame time in W-Dense and W-Characters beyond run-to-run variance (≥ 10 repetitions, 95% CI) without raising Z2-B-CORE-001 or Z2-B-CORE-004; otherwise the system allocator |
| D-SEL-HASH | XXH3-64 for in-memory keys and XXH3-128 for persisted keys; XXH64 throughout; a hand-rolled baseline | short-key and long-input throughput, persisted-key collision behaviour, bit-identical output on Windows and Android for the test vectors, pinned version | the D-SEL-HASH entry: the fastest candidate whose output is bit-identical across platforms for the vectors and whose version is pinned |
| D-SEL-JSON | yyjson; simdjson | parse throughput on the authoring corpus, conformance, fuzz clean under `win-asan`, error positions, unknown-field round-trip through the Zenith tree, Z2-B-EDIT-005 (scene open) | the D-SEL-JSON entry: the candidate meeting every criterion with the higher median parse throughput; because the writer is Zenith's on both paths, "writer support" is satisfied by `Zenith_JsonWriter` for either |

### Justification records

The custom infrastructure records CORE appends, with their content, are `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records".

## Requirement index

<!-- core-requirement-index -->

Every requirement of `Zenith2/Docs/Requirements/CORE.md` and the design headings that specify it:

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-CORE-001 | Owned pointer layout | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Owned pointer placement"; `Zenith2/Docs/Design/CORE.md` → "Handle kinds used" |
| Z2-R-CORE-002 | Owned creation and destruction | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Owned pointer placement"; `Zenith2/Docs/Design/CORE/Memory.md` → "Memory tests" |
| Z2-R-CORE-003 | Owned base conversion check | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Owned pointer placement" |
| Z2-R-CORE-004 | Slot table generational resolution | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable" |
| Z2-R-CORE-005 | Slot table retirement and reuse | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable" |
| Z2-R-CORE-006 | Slot table container tests | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable"; `Zenith2/Docs/Design/CORE/Memory.md` → "Memory tests" |
| Z2-R-CORE-007 | Operation handle handle tests | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" |
| Z2-R-CORE-008 | Heap backing selection | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Backing selection" |
| Z2-R-CORE-009 | System backing per platform | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Backing calls per platform"; `Zenith2/Docs/Design/CORE/Memory.md` → "Over-aligned side table" |
| Z2-R-CORE-010 | mimalloc backing path | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Backing selection"; `Zenith2/Docs/Design/CORE/Memory.md` → "Backing calls per platform" |
| Z2-R-CORE-011 | Heap statistics | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Statistics" |
| Z2-R-CORE-012 | Debug leak report | S2 | `Zenith2/Docs/Design/CORE.md` → "Library and file layout"; `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support" |
| Z2-R-CORE-013 | Bare allocation boundary | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Backing calls per platform"; `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-014 | Arena allocation and failure | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Arena internals"; `Zenith2/Docs/Design/CORE/Memory.md` → "Memory tests" |
| Z2-R-CORE-015 | Arena reset and markers | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Arena internals" |
| Z2-R-CORE-016 | Arena cap outcomes preset-independent | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Arena internals" |
| Z2-R-CORE-017 | Pool allocation | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals" |
| Z2-R-CORE-018 | Pool misuse | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals" |
| Z2-R-CORE-019 | Pool quarantine | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals" |
| Z2-R-CORE-020 | ASan defect detection | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Scope of this file"; `Zenith2/Docs/Design/CORE.md` → "Test strategy" |
| Z2-R-CORE-021 | HWASan defect detection on the S24 | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Scope of this file"; `Zenith2/Docs/Design/CORE.md` → "Test strategy" |
| Z2-R-CORE-022 | Zenith_Function | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_Function" |
| Z2-R-CORE-023 | OOM handler protocol | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Emergency reserve and the default handler" |
| Z2-R-CORE-024 | OOM inside the handler | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Emergency reserve and the default handler"; `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state" |
| Z2-R-CORE-025 | Default handler and pressure relay | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "OOM pressure relay"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation" |
| Z2-R-CORE-026 | Heap fault countdown | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection counter" |
| Z2-R-CORE-027 | Fault injector and sweep | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support" |
| Z2-R-CORE-028 | Container set and justification records | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Container set"; `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records" |
| Z2-R-CORE-029 | Container benchmarks | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records"; `Zenith2/Docs/Design/CORE.md` → "Library and file layout" |
| Z2-R-CORE-030 | Span | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Span" |
| Z2-R-CORE-031 | Array strong guarantee | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array" |
| Z2-R-CORE-032 | Array oracle and properties | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"; `Zenith2/Docs/Design/CORE/Containers.md` → "Container tests" |
| Z2-R-CORE-033 | FixedArray layout | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_FixedArray"; `Zenith2/Docs/Design/CORE.md` → "Dependencies" |
| Z2-R-CORE-034 | InlineArray never allocates | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_InlineArray" |
| Z2-R-CORE-035 | HashMap deterministic iteration | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet" |
| Z2-R-CORE-036 | HashMap correctness | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet" |
| Z2-R-CORE-037 | HashMap key order for simulation | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet" |
| Z2-R-CORE-038 | HashSet and heterogeneous lookup | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet" |
| Z2-R-CORE-039 | RingBuffer full policies | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer" |
| Z2-R-CORE-040 | Queue counters | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer"; `Zenith2/Docs/Design/CORE.md` → "Diagnostics and profiling" |
| Z2-R-CORE-041 | BoundedQueue MPMC correctness | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue" |
| Z2-R-CORE-042 | BoundedQueue blocking rules | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue" |
| Z2-R-CORE-043 | BitArray fixed capacity | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BitArray"; `Zenith2/Docs/Design/CORE.md` → "Dependencies" |
| Z2-R-CORE-044 | Deterministic sorting | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Sorting and searching" |
| Z2-R-CORE-045 | StringView contract | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_StringView" |
| Z2-R-CORE-046 | String UTF-8 invariant | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_String"; `Zenith2/Docs/Design/CORE/Text.md` → "Encoding policy" |
| Z2-R-CORE-047 | String oracle | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_String" |
| Z2-R-CORE-048 | Interned string identity | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "String interning" |
| Z2-R-CORE-049 | String table caps | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "String interning"; `Zenith2/Docs/Design/CORE.md` → "Budget rows" |
| Z2-R-CORE-050 | String IDs never order simulation | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "String interning" |
| Z2-R-CORE-051 | UTF-8 validation | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Unicode functions" |
| Z2-R-CORE-052 | UTF-16 conversion | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Unicode functions" |
| Z2-R-CORE-053 | Unicode policy | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Encoding policy" |
| Z2-R-CORE-054 | UTF-8 fuzzing | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Text tests" |
| Z2-R-CORE-055 | Checked formatting | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Formatting" |
| Z2-R-CORE-056 | Number text | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "Number conversion" |
| Z2-R-CORE-057 | Numerical conventions | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Numerical conventions" |
| Z2-R-CORE-058 | Degenerate inputs | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Numerical conventions"; `Zenith2/Docs/Design/CORE/Maths.md` → "Vector types" |
| Z2-R-CORE-059 | Narrowing checks | S2 | `Zenith2/Docs/Design/CORE.md` → "Library and file layout"; `Zenith2/Docs/Design/CORE.md` → "Test strategy" |
| Z2-R-CORE-060 | Units in contracts | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Units" |
| Z2-R-CORE-061 | Colour space | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Colour" |
| Z2-R-CORE-062 | Coordinate system | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems" |
| Z2-R-CORE-063 | Clip-space convention | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Matrix types"; `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems" |
| Z2-R-CORE-064 | Euler convention | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Quaternion" |
| Z2-R-CORE-065 | Vector layouts | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Vector types" |
| Z2-R-CORE-066 | Matrix layout | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Matrix types" |
| Z2-R-CORE-067 | Matrix operations | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Matrix types"; `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems" |
| Z2-R-CORE-068 | Quaternion operations | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Quaternion" |
| Z2-R-CORE-069 | Bounds tests | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Bounds types" |
| Z2-R-CORE-070 | Hash64 implementation | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64" |
| Z2-R-CORE-071 | Hash128 for persisted keys | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64" |
| Z2-R-CORE-072 | D-SEL-HASH selection | S1 | `Zenith2/Docs/Design/CORE.md` → "Decisions"; `Zenith2/Docs/Design/CORE.md` → "Library and file layout" |
| Z2-R-CORE-073 | Counter-based random draws | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| Z2-R-CORE-074 | Shared random derivations | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| Z2-R-CORE-075 | Unbiased ranges | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| Z2-R-CORE-076 | Entropy only outside simulation | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| Z2-R-CORE-077 | Persistent ID generation | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions" |
| Z2-R-CORE-078 | Persistent ID forms | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions" |
| Z2-R-CORE-079 | Monotonic clock | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" |
| Z2-R-CORE-080 | UTC clock formats | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" |
| Z2-R-CORE-081 | Clock placement | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"; `Zenith2/Docs/Design/CORE.md` → "Reconciliation items" |
| Z2-R-CORE-082 | Simulation reads no clock | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" |
| Z2-R-CORE-083 | Assertion macros | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors" |
| Z2-R-CORE-084 | Failure report | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Diagnostic output and backtraces" |
| Z2-R-CORE-085 | Recursive failure | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure" |
| Z2-R-CORE-086 | Result implementation | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy" |
| Z2-R-CORE-087 | Result propagation | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy" |
| Z2-R-CORE-088 | Error taxonomy | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Error taxonomy" |
| Z2-R-CORE-089 | Build information | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information" |
| Z2-R-CORE-090 | Log levels and compile-out | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Levels and categories"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log macros" |
| Z2-R-CORE-091 | Log without a router | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log macros" |
| Z2-R-CORE-092 | Log router fan-out | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router" |
| Z2-R-CORE-093 | Log sinks and drain | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router" |
| Z2-R-CORE-094 | Fatal log flush | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router" |
| Z2-R-CORE-095 | Log hook without a new global | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook" |
| Z2-R-CORE-096 | Log message truncation | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log macros" |
| Z2-R-CORE-097 | Threads joined by their owner | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Threads" |
| Z2-R-CORE-098 | POSIX parity | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Threads" |
| Z2-R-CORE-099 | Lock-order checker | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering" |
| Z2-R-CORE-100 | Lock rows added | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering" |
| Z2-R-CORE-101 | Mutex contention | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering" |
| Z2-R-CORE-102 | Blocking waits | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Condition variables and semaphores" |
| Z2-R-CORE-103 | Debug thread state | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state" |
| Z2-R-CORE-104 | Reentrancy guard | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_ReentrancyGuard" |
| Z2-R-CORE-105 | Callback registry lifetime | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_CallbackRegistry implementation" |
| Z2-R-CORE-106 | Callback registry bounds | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_CallbackRegistry implementation" |
| Z2-R-CORE-107 | Operation status and cancellation | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" |
| Z2-R-CORE-108 | Operation cancel stress | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" |
| Z2-R-CORE-109 | Configuration value types and schema | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |
| Z2-R-CORE-110 | Layer precedence and provenance | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" |
| Z2-R-CORE-111 | Rejected configuration values | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" |
| Z2-R-CORE-112 | Unknown configuration keys | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" |
| Z2-R-CORE-113 | Required configuration keys | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |
| Z2-R-CORE-114 | User values at the frame prologue | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" |
| Z2-R-CORE-115 | Configuration dump | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" |
| Z2-R-CORE-116 | Simulation-affecting keys | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |
| Z2-R-CORE-117 | Configuration file format | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration file format" |
| Z2-R-CORE-118 | JSON writer byte format | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"; `Zenith2/Docs/Design/CORE/Text.md` → "Byte format" |
| Z2-R-CORE-119 | JSON writer errors | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer" |
| Z2-R-CORE-120 | JSON reader limits | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-121 | JSON reader conformance | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-122 | JSON duplicate keys | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-123 | JSON positions | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-124 | JSON round trip | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-125 | JSON fuzzing | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-126 | D-SEL-JSON selection | S1 | `Zenith2/Docs/Design/CORE.md` → "Decisions"; `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-127 | D-SEL-HEAP selection | S1 | `Zenith2/Docs/Design/CORE.md` → "Decisions"; `Zenith2/Docs/Design/CORE/Memory.md` → "Backing selection" |
| Z2-R-CORE-128 | Governor creation checks | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Additions to the governor API"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests" |
| Z2-R-CORE-129 | Generated budget table | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| Z2-R-CORE-130 | Budget key defaults | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| Z2-R-CORE-131 | Rendering memory budget | S1 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| Z2-R-CORE-132 | Streaming memory budget | S1 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| Z2-R-CORE-133 | Audio memory budget | S1 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| Z2-R-CORE-134 | Script memory budget | S1 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| Z2-R-CORE-135 | Editor cache memory budget | S1 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" |
| Z2-R-CORE-136 | Governor tuning keys | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Tuning keys" |
| Z2-R-CORE-137 | CORE memory rows allocated | S1 | `Zenith2/Docs/Design/CORE.md` → "Budget rows"; `Zenith2/Docs/Design/CORE.md` → "Stage plan" |
| Z2-R-CORE-138 | Pressure from OS signals | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "OS memory queries" |
| Z2-R-CORE-139 | Pressure from budgets | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation" |
| Z2-R-CORE-140 | Pressure hysteresis | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Accounting" |
| Z2-R-CORE-141 | Consumer notification | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Consumer registration" |
| Z2-R-CORE-142 | Accounting and residual | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Accounting" |
| Z2-R-CORE-143 | Governor counters | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Counters" |
| Z2-R-CORE-144 | Shedding touches no simulation state | S4 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests" |
| Z2-R-CORE-145 | Every subsystem sheds | S4 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Counters" |
| Z2-R-CORE-146 | Streaming double-residency peak | S4 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm" |
| Z2-R-CORE-147 | World-transition peak | S4 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm" |
| Z2-R-CORE-148 | Texture-swap peak | S4 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm" |
| Z2-R-CORE-149 | Reservations never exceed peaks | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm" |
| Z2-R-CORE-150 | Reservations in pressure and shutdown | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm" |
| Z2-R-CORE-151 | Forced-level unit tests | S2 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests" |
| Z2-R-CORE-152 | Forced-level tests grow with subsystems | S4 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests" |
| Z2-R-CORE-153 | S24 memory-pressure run | S4 | `Zenith2/Docs/Design/CORE.md` → "Stage plan"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests" |
| Z2-R-CORE-154 | Factory fault points | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Factory fault points"; `Zenith2/Docs/Design/CORE/Threading.md` → "Threads" |
| Z2-R-CORE-155 | Log router holds records until its sinks exist | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router" |
| Z2-R-CORE-156 | Log ring sink | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router" |
| Z2-R-CORE-157 | Log console sink format | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router" |
| Z2-R-CORE-158 | JSON document mutation | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| Z2-R-CORE-159 | Heap backing state | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Over-aligned side table" |
| Z2-R-CORE-160 | Sanitizer kind query | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information" |
| Z2-R-CORE-161 | Log ring bootstrap size and resize | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Core configuration keys" |
| Z2-R-CORE-162 | Log drain thread start failure is fatal | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors"; `Zenith2/Docs/Design/CORE.md` → "Failure handling" |
| Z2-R-CORE-163 | Log router statistics and limits | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router" |
| Z2-R-CORE-164 | Configuration layer entries and clearing | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" |
| Z2-R-CORE-165 | Simulation-affecting list reader | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Simulation-affecting list" |
| Z2-R-CORE-166 | Configuration parse error position | S2 | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm" |
| Z2-R-CORE-167 | SHA-256 conforms to FIPS 180-4 outside D-SEL-HASH | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256"; `Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256 declarations"; `Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 algorithm steps"; `Zenith2/Docs/Design/CORE/Maths.md` → "Maths tests" |
| Z2-R-CORE-168 | JSON document tree and pointer resolution | S2 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree"; `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_DocumentTree declarations"; `Zenith2/Docs/Design/CORE/Text.md` → "Pointer resolution steps"; `Zenith2/Docs/Design/CORE/Text.md` → "Text tests" |
| Z2-R-CORE-169 | Subsystem reservations bounded by the subsystem budget | S3 | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Subsystem reservation algorithm"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Grant steps"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Interaction with transition reservations and budgets"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests" |
| Z2-R-CORE-171 | Deterministic transcendental functions | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions"; `Zenith2/Docs/Design/CORE/Maths.md` → "Transcendental declarations"; `Zenith2/Docs/Design/CORE/Maths.md` → "Shared evaluation rules"; `Zenith2/Docs/Design/CORE/Maths.md` → "Constants and coefficient tables"; `Zenith2/Docs/Design/CORE/Maths.md` → "Error bounds"; `Zenith2/Docs/Design/CORE/Maths.md` → "Golden vectors" |
| Z2-R-CORE-172 | CRT maths mode in every Windows main | S2 | `Zenith2/Docs/Design/CORE/Maths.md` → "The CRT maths mode"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test main steps" |
| Z2-R-CORE-173 | Single-producer single-consumer ring and triple buffer | S2 | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_SpscRing"; `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_TripleBuffer"; `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records"; `Zenith2/Docs/Design/CORE/Containers.md` → "Container tests" |
| Z2-R-CORE-174 | No-allocation scope checked by the heap | S2 | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_NoAllocationScope"; `Zenith2/Docs/Design/CORE/Threading.md` → "No-allocation check steps"; `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state" |
| Z2-R-CORE-175 | Test child-process launcher | S2 | `Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test child process platform matrix"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test child process steps" |
| Z2-R-CORE-176 | Profiled and unprofiled primitives | S2 | `Zenith2/Docs/Design/CORE/Threading.md → "Platform threading classes"`; `Zenith2/Docs/Design/CORE/Threading.md → "Mutexes and lock ordering"`; `Zenith2/Docs/Design/CORE/Threading.md → "Condition variables and semaphores"`; `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"` |
| Z2-R-CORE-177 | Lock acquire reports the wait | S2 | `Zenith2/Docs/Design/CORE/Threading.md → "Mutexes and lock ordering"`; `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"` |
| Z2-R-CORE-178 | Hook dispatch and recursion guard | S2 | `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"`; `Zenith2/Docs/Design/CORE/Threading.md → "Debug thread state"` |
| Z2-R-CORE-179 | Hook-reachable sources use unprofiled primitives | S2 | `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"` |
| Z2-R-CORE-170 | Android test entry for the shared test main | S3 | `Zenith2/Docs/Design/CORE/Memory.md` → "Test main and the Android test entry"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test main declarations"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test main steps"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Diagnostic output and backtraces" |

## Reconciliation items

<!-- informative -->
The interfaces this family agreed with other documents. A row whose owner column reads "landed" cites the heading that now carries it; the rest are still marked in place with `<!-- assumption: pending <AREA> -->` or are landed by the CORE ticket named, and are listed in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`:
<!-- /informative -->

| Item | Where marked | Owner to reconcile |
|---|---|---|
| `Zenith_ProfilerHooks` gains the log-record members (an extension of the Z2-D-046 exemption, ruled by the human on 2026-10-16 as ruling Q11 (a) together with the Debug thread state's OOM and failure flags, and recorded in `Zenith2/Docs/Contracts.md` → "Writable-data allowlist entries", [Z2-N-CONTRACT-152]); the router is created in a startup step 1b with the build-time ring size `uLOG_RING_BYTES_DEFAULT`; at step 7 the composition root adds the sinks, resizes the ring to `log.ringBytes` and only then starts the drain thread, whose start failure is fatal with the new `FATAL_CODE_LOGGING_UNAVAILABLE` | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook"; `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state" | landed: `Zenith2/Docs/Contracts.md` → "The two installed hooks" ([Z2-N-CONTRACT-157]) and "Fatal errors"; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" (steps 1b and 7, [Z2-N-ARCH-846]) |
| `Zenith_MarkThreadAsJobWorker` takes `bool bRunningJob` (a job body is running now), so a worker's idle park passes the blocking check | `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state" | landed: `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ([Z2-N-CONTRACT-315]) |
| `Zenith_OperationHandle` gains `GetFailureCode` and `GetSlotID` | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" | landed: `Zenith2/Docs/Contracts.md` → "Zenith_OperationHandle" ([Z2-N-CONTRACT-101]) |
| the configuration types are declared once, in `ZenithCore/Zenith_ConfigStore.h`; Architecture's "Assumed types" and `PlatformLayer.md`'s `Zenith_Config` facade are aligned (X49, landed); the store gains `Zenith_ConfigLayerEntry`, `GetLayerEntry`, `ClearLayer` and `ApplySimulationAffecting` for PLAT | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" | PLAT |
| the clock functions are declared once, in ZenithCore's `Zenith_Clock.h` (ZenithProfiling at layer 1 and ZenithJobs at layer 2 need them below ZenithPlatform) | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" | landed: `Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership" |
| every configuration define is set to 0 or 1 in every target | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information" | landed: `Zenith2/Docs/Architecture/BuildSystem.md` → "Compile and link flags" ([Z2-N-ARCH-247], [Z2-N-ARCH-248]) |
| the generated budget table's path is an include path under `${ZENITH_OUT}`; the generator parses Budgets' own Value grammar ([Z2-N-BUDGET-009]) and reads the subsystem-to-row mapping from Architecture's table "Budgets" ([Z2-N-ARCH-823], landed), keeping no copy | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table" | Budgets |
| the governor API additions (`m_uPendingSubsystemMask`, `MEMORY_ERROR_OUT_OF_MEMORY`, `GetDeviceLocalBytes`, `Create`'s `Zenith_FaultInjector*`, `ClearForcedLevel`, `BindOOMPressureFlag`, `GetStats`, `GetReservedPeakBytes`, `Zenith_ReadMemoryBudgets`, `Zenith_QueryProcessResidentBytes`) | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Additions to the governor API" | landed: `Zenith2/Docs/Architecture.md` → "Memory governor" |
| the subsystem reservation API (`MEMORY_ERROR_SUBSYSTEM_REFUSED`, `Zenith_MemoryError::m_eSubsystem`, `Zenith_SubsystemReservation`, `ReserveSubsystemBytes`, `GetReservedSubsystemBytes`) for SCENE's preparation reservation (X180, A5 row ScB16); CORE's additions are the reservations' private constructors, `Zenith_MemorySubsystemUsage::m_ulReservedBytes`, `Zenith_MemoryGovernorStats::m_ulSubsystemRefusals` and the counters `memory.subsystem.<name>_reserved_bytes` and `memory.subsystem_refusals` | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Subsystem reservation algorithm" | landed: `Zenith2/Docs/Architecture.md` → "Subsystem reservations" ([Z2-N-ARCH-825], [Z2-N-ARCH-826]); SCENE calls `ReserveSubsystemBytes(MEMORY_SUBSYSTEM_SIMULATION, …)` and maps `MEMORY_ERROR_SUBSYSTEM_REFUSED` to `SCENE_ERROR_MEMORY_REFUSED` |
| library configuration fragments merged by the cook | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Tuning keys" | ASSET, PLAT |
| the Android test entry of the shared test main: `Zenith_RunTestMain` with `Zenith_TestOutputRoute`, an exported `SDL_main` in `ZenithTestSupport`, logcat tag `Zenith2Test` through the new `Zenith_WriteTaggedDiagnosticLine` (A5 row MB23; MOD links it with `-Wl,--undefined=SDL_main`); no platform directory under `Tests/Support/` is needed, because the only Android call is in ZenithCore's own `Internal/Android/` | `Zenith2/Docs/Design/CORE/Memory.md` → "Test main and the Android test entry" | MOD |
| four lock-order rows (380, 850, 930, 980) | `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering" | open: Contracts, landed by the CORE tickets in their diffs |
| the log router's shutdown row | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Shutdown row" | landed: `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" (row 11) |
| the world-state hash hashes interned strings by text | `Zenith2/Docs/Design/CORE/Text.md` → "String interning" | SCRIPT, ECS |
| two pending budget rows | "Budget rows" | Budgets |
| the process's interned-string table is CORE's `Zenith_StringTable`, owned by the composition root | "Dependencies" (table "Areas CORE depends on for integration") | MOD |
| `c` (char) and `d` (double) naming prefixes | `Zenith2/Docs/Design/CORE/Maths.md` → "Numerical conventions" | landed: `Zenith2/Docs/Contracts.md` → "Naming and layout" |
| the host SHA-256 is CORE's `Zenith_Sha256.h` (X152): SCENE deletes `ZenithDocument/Zenith_Sha256.h` and its `Internal/Zenith_Sha256.cpp`, renames its type `Zenith_Sha256Digest` to `Zenith_Sha256Value`, calls `Zenith_Sha256Digest(xBytes)` where it called `Zenith_ComputeSha256(xBytes)` and a `Zenith_Sha256Stream` with two `Update` calls where it called the two-span overload, keeps `Zenith_FormatSha256Hex` and `Zenith_ParseSha256Hex` as named, and moves `Test_Sha256_NistVectors` to CORE's suite; PLAT's patch-mount verification digests each archive with `Zenith_Sha256Digest` or a `Zenith_Sha256Stream` | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256" | SCENE, PLAT |
| `Zenith_DocumentTree` is CORE's, in `ZenithCore/Zenith_DocumentTree.h` (C13, S5), with SCENE's listing unchanged; `FindPath` returns the new `JSON_ACCESS_ERROR_INVALID_POINTER` for a malformed pointer (SCENE's text names only `MISSING_MEMBER` and `INDEX_OUT_OF_RANGE`) and `JSON_ACCESS_ERROR_WRONG_KIND` for a token applied to a scalar; SCENE's `uDOCUMENT_MAX_POINTER_BYTES` equals `uJSON_POINTER_MAX_BYTES`; SCENE retires its `assumption: pending CORE` block over "Document tree" | `Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree" | SCENE, ECS, ASSET |
| the deterministic transcendental functions of ruling X314-FMA3 (WORLD row WD29 uses `Zenith_Sin`, `Zenith_Cos` and `Zenith_Atan2` by these names; PHYS rule D9, ANIM, CHAR, AI and TWOD call "CORE's deterministic functions", which are this set) and `Zenith_ApplyCrtMathMode` in every Windows `main` | `Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions" | landed: `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" ([Z2-N-ARCH-888], [Z2-N-ARCH-889]); MOD's Player entries (row C20) in `Zenith2/Docs/Design/MOD.md`; open: every area's sweep of CRT transcendental calls in simulation and tool-output code |
| `Zenith_SpscRing` and `Zenith_TripleBuffer` as AUDIO's AU6 block specifies them, `Zenith_MutableSpan<T>` read as `Zenith_Span<T>`; `Zenith_NoAllocationScope` with its Debug thread state field | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_SpscRing"; `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_NoAllocationScope" | AUDIO, DEVT (rows AU6, DV23); the field landed in `Zenith2/Docs/Contracts.md` → "The two installed hooks" ([Z2-N-CONTRACT-144], cross-doc item X361) |
| the test child-process launcher of ruling X349 (a): declared in `ZenithTestSupport`, platform bodies in ZenithCore's `Internal/<Platform>/` files, so rule F11 and the glob partition stay unchanged | `Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes" | SAVE, SCENE; landed: `Zenith2/Docs/Architecture/BuildSystem.md` → "Directory rules and globbing" ([Z2-N-ARCH-162]) and the F11 row of `Zenith2/Docs/Contracts.md` (cross-doc item X349) |
