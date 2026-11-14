# Zenith2 worker guide

Read this page before touching Zenith2 code. It is the short form of `Zenith2/Docs/Contracts.md`; every rule links to the Contracts.md heading that binds it, and that heading wins if the two ever differ. You hold Read, Edit, Write, Grep and Glob: you cannot build, run tests or touch the board, so the code you write has to pass gates you never see run.

## Where code goes

The repository, as `Zenith2/CLAUDE.md` → "Layout" lists it:

```
Zenith2/Engine/   core:    ZenithCore ZenithGraph ZenithJobs ZenithProfiling ZenithPlatform ZenithReflect ZenithDocument ZenithVertexFormat
                  data:    ZenithCompression(+_Zlib,_Zstd) ZenithECS(+_Custom,_Flecs,_EnTT) ZenithScene ZenithAssets ZenithPackage ZenithSave
                  render:  ZenithRHI(+_Vulkan,_D3D12) ZenithShadersGenerated ZenithFlux
                  script:  ZenithScript  ZenithScriptCompiler (tool side, never in a Player)   top: ZenithRuntime
                  modules: Zenith2D ZenithPhysics(+_Jolt,_PhysX) ZenithAnimation ZenithAudio ZenithAI ZenithUI
                           ZenithOnline(+_Offline) ZenithNet ZenithStreaming ZenithGameplay ZenithCinematics ZenithVFX ZenithEnvironment
Zenith2/Tools/    ZenithEditor ZenithCook ZenithShaderCompiler NewGame NewGameScaffold ZenithPathTracer DocumentHost ContentCore ZenithContent Package Gates (protected) Setup Backlog
Zenith2/          Shaders/ CMake/ Benchmarks/ Tests/<Suite>/ Player/ ThirdParty/ Games/<Name>/ Docs/ out/ (gitignored)
```

Inside one library:

```
Zenith2/Engine/<Lib>/
	CMakeLists.txt          one zenith_add_library(<Lib> DEPENDS <direct deps>) call
	Zenith_<Thing>.h        public headers: the whole API, one contract block per type
	Zenith_<Thing>.inl      optional public template and inline definitions, included at the end of its .h
	Internal/               private headers and every .cpp
	Internal/Windows/  Internal/Android/  Internal/Linux/    platform sources, selected by directory
	Tests/Zenith_<Type>_Tests.cpp       doctest tests, built into <Lib>_Tests
	Tests/Examples/         compiled examples cited by contract blocks
```

- Sources are globbed: a new file is built because of the directory it sits in. Never add a source list. Contracts.md → "API boundary".
- A file outside a globbed directory, platform code outside `Internal/<Platform>/`, or test code outside `Tests/` fails `SourceLayout`. Contracts.md → "API boundary".
- Only your own library and its tests may include your `Internal/` headers. Contracts.md → "API boundary".
- A new dependency between libraries is a new `DEPENDS` entry, and it must keep the graph a DAG. Contracts.md → "Layering DAG".
- Games contain no C++. Contracts.md → "Purpose and scope".

## Naming

Zenith Hungarian (Z2-D-034): `Zenith_`/`Flux_` type prefixes, scope prefix then type prefix on variables, PascalCase functions, `SCREAMING_SNAKE` enums with a category prefix. The full prefix table and the adopted code-style subset: Contracts.md → "Tests and code hygiene gates" (subsection "Naming and layout"). The root `CLAUDE.md` rules about `#include "Zenith.h"` and `#ifdef ZENITH_TOOLS` do not apply in Zenith2.

| # | Correct | Why |
|---|---|---|
| 1 | `class Zenith_Arena` | engine type: `Zenith_` + PascalCase |
| 2 | `struct Flux_RasterTargets` | renderer type: `Flux_` |
| 3 | `uint32 m_uSlotsPerPage;` | member, unsigned |
| 4 | `Zenith_Owned<Zenith_Streamer> m_xStreamer;` | member object |
| 5 | `Zenith_Heap* m_pxHeap;` | member pointer to a class |
| 6 | `const char* m_szName;` | member C string |
| 7 | `uint64 ulSourceBytes` | local or parameter `uint64` |
| 8 | `Zenith_PfnOOMHandler m_pfnOOMHandler;` | function pointer |
| 9 | `enum Zenith_QueueFullPolicy : uint8 { QUEUE_FULL_POLICY_REJECT, … };` | enum type + prefixed values |
| 10 | `constexpr uint64 ulHEAP_MAX_ALIGNMENT = 4096;` | constant |

Also: `bool bSuccess`, `float fDeltaSeconds`, `sint32 iLevel`, `sint64 iOffset`, `uint32 auIndices[4]`, `char cSeparator`, `char acBuffer[64]`, `e` for enum values (`eMode`). Integers are `uint8` … `uint64`, `sint8` … `sint64`, and `uint`/`sint` for the platform's default `unsigned int`/`int` (`Zenith_Types.h`), never `u_int*`, `intN_t` or `uintN_t` outside a third-party boundary in `Internal/`. Write no `noexcept` and no `[[nodiscard]]` (a `noexcept` that a language or standard-library mechanism actually requires is very rare and carries `// ZENITH-ALLOW: <reason>` on its own line; `ForbiddenCheck` rule F25 refuses the rest); `[[noreturn]]` only on a function that never returns; `final` only on a class that inherits from a base; a type whose contract says `Thread affinity: IMMUTABLE` makes its data members `const` where possible, or says in that line which operation needs them writable. Contracts.md → "Contract blocks per type", Contracts.md → "Tests and code hygiene gates". Interfaces have no `I` prefix (`Zenith_Physics`); factories are `Zenith_Create<Noun>_<Backend>`. Contracts.md → "API boundary".

## Ownership and allocation

Four idioms cover everything: own with `Zenith_Owned`, share with a handle, borrow with `T&`/`T*`, and put short-lived data in an arena. Contracts.md → "Ownership and allocation".

**Own a heap object.** `Zenith_New` never returns null (failure goes to the OOM handler); `Zenith_TryNew` returns a `Zenith_Result`. Constructors are infallible (and carry no exception specification). Contracts.md → "Ownership and allocation".

```cpp
Zenith_Owned<Zenith_JobQueue> xQueue = Zenith_New<Zenith_JobQueue>(uCapacity);
Zenith_Owned<Zenith_JobQueue> xOther = std::move(xQueue);   // xQueue is now null; there is no copy and no Release()
xOther.Reset();                                              // runs ~Zenith_JobQueue, frees to the general heap
```

**A replaceable subsystem: interface + factory.** The interface has a virtual destructor and no data; the implementation names the interface as its first base, so `Owned<Derived>` converts to `Owned<Base>` (checked in every build). Contracts.md → "API boundary", Contracts.md → "Ownership and allocation".

```cpp
// ZenithCompression_Zstd/Zenith_Compression_Zstd.h (public: the factory, nothing else)
Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> Zenith_CreateCompression_Zstd(
	const Zenith_CompressionDesc& xDesc);

// ZenithCompression_Zstd/Internal/Zenith_CompressionZstd.cpp
class Zenith_CompressionZstd final : public Zenith_Compression   // interface first
{
	// ... overrides, all ...
};

Zenith_Result<Zenith_Owned<Zenith_Compression>, Zenith_CompressionError> Zenith_CreateCompression_Zstd(
	const Zenith_CompressionDesc& xDesc)
{
	if (xDesc.m_iDefaultLevel < 1)
	{
		return Zenith_Err(COMPRESSION_ERROR_INVALID_DESC);
	}
	Zenith_Result<Zenith_Owned<Zenith_CompressionZstd>, Zenith_AllocError> xAlloc = Zenith_TryNew<Zenith_CompressionZstd>();
	if (xAlloc.IsError())
	{
		return Zenith_Err(COMPRESSION_ERROR_OUT_OF_MEMORY);
	}
	return Zenith_Owned<Zenith_Compression>(std::move(xAlloc).TakeValue());   // Owned<Derived> -> Owned<Base>
}
```

**Fallible construction.** An infallible constructor (private, or taking a private `PrivateTag` passkey so `Zenith_TryNew` can call it) plus a static `Create` that runs each fallible step; members are owners, so a failure half-way unwinds itself. Every step gets a failure-injection test. Contracts.md → "Ownership and allocation", Contracts.md → "Threading and concurrency contract".

```cpp
Zenith_Result<Zenith_Owned<Zenith_Streamer>, Zenith_StreamError> Zenith_Streamer::Create(
	const Zenith_StreamerDesc& xDesc, Zenith_FileSystem& xFiles)
{
	Zenith_Result<Zenith_Owned<Zenith_Streamer>, Zenith_AllocError> xAlloc = Zenith_TryNew<Zenith_Streamer>(PrivateTag{}, xFiles);
	if (xAlloc.IsError())
	{
		return Zenith_Err(STREAM_ERROR_OUT_OF_MEMORY);   // a different error enum is mapped explicitly
	}
	Zenith_Owned<Zenith_Streamer> xStreamer = std::move(xAlloc).TakeValue();
	ZENITH_TRY(xStreamer->CreateQueues(xDesc));       // Zenith_Result<void, Zenith_StreamError>: same enum, so ZENITH_TRY
	ZENITH_TRY(xStreamer->CreateWorkers(xDesc));      // on failure, xStreamer's destructor unwinds the queues
	return xStreamer;
}
```

**Arena data.** Frame and graph data use `Zenith_Construct`; a trivially destructible type needs no `Zenith_Destroy`, a non-trivial one does before `Reset()` (checked in every build). Graph captures are trivially destructible. Contracts.md → "Ownership and allocation".

```cpp
struct Flux_PassTiming          // trivially destructible
{
	uint32 m_uPassIndex = 0;
	float m_fGpuMilliseconds = 0.0f;
};

Flux_PassTiming* pxTimings = Zenith_ConstructArray<Flux_PassTiming>(xFrameArena, uPassCount);
Zenith_ScratchMap* pxMap = Zenith_Construct<Zenith_ScratchMap>(xFrameArena, xFrameArena);   // not trivially destructible
// ... use both ...
Zenith_Destroy(xFrameArena, pxMap);   // before xFrameArena.Reset()
```

**Shared references are handles, never stored pointers.** Entities, components, assets, GPU resources and async operations each have a handle type; resolving a stale one returns an error in every build. Never serialize a world or device handle: serialize the `Zenith_PersistentID`. Contracts.md → "Handle kinds".

```cpp
class Zenith_FollowCamera
{
public:
	// ...
private:
	Zenith_EntityHandle m_xTarget;                    // not Zenith_Entity*
	Zenith_AssetHandle<Zenith_CameraRig> m_xRig;      // reference-counted, shared across worlds
	Zenith_OperationHandle m_xPendingLoad;            // owned by us; destroying it cancels
};
```

**Narrowing and ignored results.** Contracts.md → "Tests and code hygiene gates", Contracts.md → "Errors and results".

```cpp
const uint32 uCount = Zenith_NarrowCast<uint32>(ulCount);                        // engine value: asserts on loss
Zenith_Result<uint16, Zenith_NarrowError> xPort = Zenith_TryNarrowCast<uint16>(iParsedPort);   // untrusted input
// In the class:  bool TryPop(Zenith_Job& xOut);   // fallible: never drop its result silently
static_cast<void>(xQueue.TryPop(xJob));   // shutdown drain: an empty queue is the expected end state
```

Errors: `Zenith_Result<T,E>` for anything a correct program can meet (I/O, allocation, corrupt data), a plain `{ T, E }` that is OK when the error code is 0, whose misuse (`Value()` of an error) asserts in debug builds only; `ZENITH_ASSERT` (debug) or `ZENITH_CHECK` (every build) for your own preconditions; never an assert on file, network, command-line or script input; no exceptions outside `ZenithScript/Internal` and `ZenithScriptCompiler/Internal`. Contracts.md → "Errors and results".

## Tests

- Tests live in `Zenith2/Engine/<Lib>/Tests/Zenith_<Type>_Tests.cpp`, build into `<Lib>_Tests`, and use doctest; CTest runs each `TEST_CASE` separately. Contracts.md → "Tests and code hygiene gates".
- Name each case `"<Library>/<Type>/<Behaviour>"` (`<Type>` without `Zenith_`/`Flux_`); its ID in tickets is `Test_<Type>_<Behaviour>`. Contracts.md → "Tests and code hygiene gates".
- Every failure scenario in your Definition of Done gets its own case. Tests land in the same ticket as the code. Pass/fail only: never pin a count. Contracts.md → "Tests and code hygiene gates".
- A test that expects termination goes in the `z2-death-check`, `z2-death-assert` or `z2-sanitizer-defect` suite. Contracts.md → "Tests and code hygiene gates".
- Tests may use the STL (never smart pointers). The test main installs a root heap and fails the run on any leak. Contracts.md → "Containers and standard library policy".

```cpp
// Zenith2/Engine/ZenithCore/Tests/Zenith_Arena_Tests.cpp
#include "Zenith_Arena.h"
#include <cstdint>
#include <doctest/doctest.h>

TEST_CASE("ZenithCore/Arena/Allocate_WithinBlock_ReturnsAlignedMemory")
{
	Zenith_Arena xArena(Zenith_ArenaDesc{ "test", 1024, 1024, 1 });
	void* pMemory = xArena.Allocate(24, 16);
	CHECK(pMemory != nullptr);
	CHECK(reinterpret_cast<std::uintptr_t>(pMemory) % 16 == 0);
}

TEST_CASE("ZenithCore/Arena/TryAllocate_PastCap_FailsAndLeavesArenaUnchanged")   // the failure scenario
{
	Zenith_Arena xArena(Zenith_ArenaDesc{ "test", 256, 256, 1 });
	REQUIRE(xArena.TryAllocate(200, 8).IsOk());
	const uint64 ulUsedBefore = xArena.GetUsedBytes();
	Zenith_Result<void*, Zenith_AllocError> xResult = xArena.TryAllocate(200, 8);
	REQUIRE(xResult.IsError());
	CHECK(xResult.Error() == ALLOC_ERROR_CAPACITY_EXCEEDED);
	CHECK(xArena.GetUsedBytes() == ulUsedBefore);
}
```

## Public headers Internal and forbidden constructs

- A public header (and its optional `.inl` companion, which sits beside it in the library root, never under `Internal/`): `#pragma once`; includes only its own library's public headers, its declared dependencies' public headers and the 13 allowed standard headers; compiles alone; no STL type; no third-party header; no `using namespace`; no `noexcept` and no `[[nodiscard]]`; one contract block per type. Contracts.md → "API boundary", Contracts.md → "Contract blocks per type".
- Allowed standard headers in runtime code: `<cstdint> <cstddef> <cstring> <cmath> <limits> <type_traits> <concepts> <utility> <new> <bit> <atomic> <initializer_list> <compare> <source_location>`. Contracts.md → "Containers and standard library policy".
- A contract block, immediately above each public type: Contracts.md → "Contract blocks per type".

```cpp
/// @contract type Zenith_JobQueue
/// Ownership: Owned by Zenith_JobSystem through Zenith_Owned; one per worker.
/// Thread affinity: ANY_THREAD_EXCEPT(Pop). Pop is called by the owning worker alone.
/// Invalidation: None.
/// Units: None.
/// Errors: TryPush returns Zenith_Result<void, Zenith_QueueError> (QUEUE_ERROR_FULL); a null job is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. Source API; deprecations per Contracts.md "Compatibility policy per kind".
/// @endcontract
```

`ForbiddenCheck` rejects these outside the named places (Contracts.md → "Tests and code hygiene gates"):

| Forbidden | Allowed only in |
|---|---|
| `new`, `delete`, `malloc`, `free`, `realloc`, `calloc`, aligned variants, `operator new/delete`, `VirtualAlloc`, `mmap` (`alloca` nowhere, not even there) | `ZenithCore/Internal/Memory/` |
| placement `new` | ZenithCore memory and container code |
| `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`, `make_unique`, `make_shared` | nowhere, not even tests |
| standard headers outside the allowed 13 | tests, tools, benchmarks; runtime `Internal/` with an allowlist entry and decision ID |
| STL types in runtime public headers | nowhere |
| `try`, `catch`, `throw`; Luau headers | `ZenithScript/Internal/`, `ZenithScriptCompiler/Internal/` |
| `lua_call`, `lua_callk`, `lua_pcall` | `ZenithScript/Internal/` |
| `dynamic_cast`, `typeid` | nowhere (RTTI is off) |
| `thread_local` | `ZenithCore/Internal/Profiling/`, `ZenithProfiling/Internal/`: the two variables of Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152] |
| mutable globals, mutable statics, function-local statics, singletons, service lookup | nowhere (only the five variables of Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152]: the root heap and profiler hooks installed in `main`, the two `thread_local` variables, and the crash handler anchor that `Zenith_CrashHandler::Create` claims at startup step 7) |
| `#if` on a platform or backend macro; any `#if` inside a class body | nowhere: use `Internal/<Platform>/` files |
| calling `Zenith_Delete` | `Zenith_Owned.h` |
| platform SDK headers (`windows.h`, `dbghelp.h`, `pthread.h`, `unwind.h`, `dlfcn.h`, `android/…`) | `Internal/<Platform>/` files of the matching platform; and `<pthread.h>`/`<semaphore.h>` in ZenithCore's `Platform/Android/` and `Platform/Linux/` `Zenith_PlatformThreading.h` alone, the signposted exemption of Contracts.md → "Library directory layout" [Z2-N-CONTRACT-583] |
| `#include` of a `.inl` | the `.h` of the same name |
| `rand`, `srand`, `time(`, `clock(` in runtime code | nowhere: use `Zenith_RandomSource` and the clock service |
| a CRT transcendental call (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `log`, `log2`, `log10`, `pow`, `cbrt`, their `std::` and `f`/`l` forms) in runtime code, `Tools/Authoring/**`, `Tools/ZenithCook/**` or any directory whose output is tracked or baked | nowhere (rule F21): call CORE's `Zenith_Sin` … `Zenith_Cbrt` from `ZenithCore/Zenith_Transcendental.h`; `std::sqrt` stays allowed (Architecture.md [Z2-N-ARCH-888]) |
| `#pragma warning(disable …)`, `#pragma clang diagnostic ignored` | third-party wrappers, with a decision ID |

Code that relies on a Z2-D-046 exemption (it defines, or declares `extern`, one of the five variables) carries the declaration comment block of Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-578] directly above the declaration, naming the decision, every dated extension the allowlist entry lists, the entry and the contract heading. `ForbiddenCheck` F16 fails the build when the first line is missing. For example:

```cpp
// Zenith2/Engine/ZenithCore/Internal/Profiling/Zenith_DebugThreadState.cpp
// ZENITH-EXEMPTION g_xDebugThreadState: Z2-D-046, extended 2026-10-16 by ruling Q11 (a)
// Allowlist: Zenith2/Tools/Gates/writable_data_allowlist.json, entry "g_xDebugThreadState"
// Contract: Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152]
// Why: the per-thread state the lock-order checker needs (held-lock stack), with the job-worker, OOM and failure flags, the no-allocation depth, the hook depth and re-entry count that stop a profiler hook from recursing, and the log router's nested-record flag and count.
constinit thread_local Zenith_DebugThreadState g_xDebugThreadState;
```

A new exemption is a dated extension of Z2-D-046 that a ruling records, never something an implementation ticket adds: if your ticket seems to need a sixth variable, stop and report it (Contracts.md → "No mutable global state").

Profiling in one breath: a hook is never called directly, only through the `Zenith_Dispatch…` functions of `Zenith_ProfilerHooks.h`; profiler code, and any code a hook can reach, names only `Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling` and `Zenith_SemaphoreNoProfiling` (`ForbiddenCheck` F26); `Zenith_Mutex` is the profiled variant everything else uses. Contracts.md → "Threading and concurrency contract" [Z2-N-CONTRACT-589]-[Z2-N-CONTRACT-591].

Concurrency in one breath: locks come from the lock-ordering table and are taken in ascending level; jobs never block (edges and continuations instead); every queue is bounded with a `QUEUE_FULL_POLICY_*`; a callback is unregistered before its owner dies; every thread is joined by its owner. Contracts.md → "Threading and concurrency contract".

Complexity limits are hard failures. Split a function when it stops being coherent, not to dodge a threshold; a justified exception is a profile entry with a decision ID. Contracts.md → "Tests and code hygiene gates".

## GPU performance rules on every platform

Full rules and lints: `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"`; gate mapping: Contracts.md → "Enforceability matrix".

- You never write a barrier, layout transition or aliasing barrier; pass contexts expose none. Declare resources once, through the generated parameter struct and `Flux_RasterTargets`.
- The runtime never compiles a shader; Slang is compiled offline and C++ parameter structs are generated.
- Keep early-Z: opaque pipelines never `discard`/`clip` or write `SV_Depth`; alpha-tested materials use `[AlphaTest]` variants drawn after opaque; subsequent colour passes use an equal depth test.
- Depth is D24S8 by default; D32F_S8 only for a view whose precision check fails or where the device lacks D24S8.
- No 32-bit-per-channel render target except the justified list (visibility ID R32_UINT, D32F depth views, GPU-scene buffers). HDR colour R11G11B10F, TAA history RGBA16F, normals RG16_UNORM octahedral, velocity RG16F, G-buffer ≤16 bits per channel, HiZ RG16F.
- Bind only the attachments you declare; depth read-only when not written; an output written only conditionally has colour write mask 0 in PSOs that skip it.
- Use native `half` (not `min16float`) for colour, normals, BRDF terms and post; keep positions, depth, UV derivatives and accumulations `float`.
- Opaque draws cull back faces; two-sided needs the material's flag.
- Vertex data: compressed Slang-declared streams, a dedicated position stream, QTangent (or the octahedral tangentless frame if D-VTX-TANGENTLESS (open, Stage 3) selects it), UNORM16 UVs with scale and bias, 16-bit indices (`Zenith2/Docs/Rendering.md → "Vertex data format"`).
- The graph culls empty passes, folds clears and merges compatible passes: do not hand-merge.

## Citing and updating docs

- Cite as path plus exact heading, for example `Zenith2/Docs/Contracts.md → "Handle kinds"` (or, once the area design docs exist, `Zenith2/Docs/Design/ECS.md → "Chunk queries"`). `Read` cannot follow `#anchors`. Contracts.md → "Review and change management".
- A ticket that depends on a memory mechanism (heap, arena, pool, sanitizer rules) cites the `Zenith2/Docs/Contracts/Memory.md` heading directly; `Contracts.md → "Ownership and allocation"` alone covers those rules for coverage, not for reading.
- Your ticket names at most three sections to read. They are starting points: read callers and neighbouring code freely.
- If your change alters a contract, an API or a design, edit that section of `Zenith2/Docs/Design/<CODE>.md` (or the core doc) in the same diff; keep its heading text unchanged. Contracts.md → "Review and change management".
- In Architecture, Rendering, Scripting, Contracts and Budgets, every obligation you add is a tagged bullet `- [Z2-N-<DOC>-NNN] …` with a fresh ID; rationale goes inside `<!-- informative -->` … `<!-- /informative -->`. Contracts.md → "Review and change management".
- When your ticket records a ruling, append its entry to `Zenith2/Docs/DecisionLog.md` in your own diff with the explicit `Z2-D-NNN` the ticket names, following `Zenith2/Docs/DecisionLog.md → "How this log is written"` (Z2-D-097); `zagent decide` does not write that file. Contracts.md → "Review and change management".
- You never edit `Zenith2/Docs/Board/**`, `Zenith2/Tools/Gates/**` or `Zenith2/CLAUDE.md`. Contracts.md → "Review and change management".

## When the ticket, docs and code disagree

Stop. Do not pick a side, do not improvise a workaround, do not "fix" the doc to match your code. Report the contradiction in your result: the two (or three) sources, quoted with path and heading or file and line, and what you would need decided. The orchestrator blocks the ticket for revalidation. Contracts.md → "Purpose and scope".

If you leave a known shortcut behind, say so in your result so a refactor ticket is filed in the same stage. Contracts.md → "Review and change management".

## What each ticket section means to you

| Section | What you do with it |
|---|---|
| `## Goal` | the scope and the explicit exclusions; do nothing outside it |
| `## Implementation Contract` | the exact files (repo-relative paths), the exact public declarations and signatures, the tests with their names and cases, and ≤3 doc sections to read |
| `## Dependencies` | what must already exist, and every other subsystem your change touches or relies on; read their public headers |
| `## Compatibility` | what your change does to saves, cooked data and public API, and which migration it adds |
| `## Definition of Done` | a `- [ ]` checklist of observable outcomes, success and failure; every item must hold when you finish |
| `## Validation` | the checks and fixtures that prove it, the diagnostic evidence (logs, counters, captures), and the "Review focus" list a reviewer will probe beyond your tests |
| `## Budgets` | the budget rows your code must stay within (`Zenith2/Docs/Budgets.md`) |
| `## Ownership` | the owning area epic; you may be asked to leave the subsystem in a state its owner can maintain |
| `Requirements:` / `Decisions:` | the `Z2-R-…` requirements you satisfy and the `Z2-D-…` rulings you obey; the governing `Z2-D-…` IDs are also cited inside the Goal or DoD text, which is where the rulings pasted into your prompt are matched from |
| `Draft-ID:` / `Spec-Maturity:` / `Cites:` | bookkeeping for the filer; `Cites:` lists the exact sections the spec was written against |
| `kind:` (front matter) | what sort of ticket it is: `implementation`, `api`, `decision`, `harness`, `budget`, `revalidation`, `integration`, `acceptance`, `proc` or `maintenance`; an `api` ticket lands interfaces only, a `harness` ticket builds measurement code under `Zenith2/Benchmarks/` |

There is no `## Gates` section: the gates are chosen from the paths your diff touches. Contracts.md → "Enforceability matrix".
