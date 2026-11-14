# Zenith2 engineering contracts

<!-- informative -->
`Zenith2/Docs/Contracts.md` is the engineering contract of Zenith2. Every ticket, design document and review cites it by exact `##` heading, for example `Zenith2/Docs/Contracts.md → "Ownership and allocation"`. Normative statements are tagged bullets `[Z2-N-CONTRACT-NNN]`; they are catalogued in `Zenith2/Docs/Requirements/Normative.md` (Z2-D-066) and checked by the coverage gate step. Rationale and prior art sit in informative blocks like this one. A `<!-- elaboration -->` marker before a heading or bullet flags detail designed in this document beyond the programme plan; the marker is a review aid and carries no meaning of its own. Code snippets outside "Contract blocks per type" omit their contract blocks for brevity; the implementation carries one per public type.
<!-- /informative -->

## Purpose and scope

### What this document binds

- [Z2-N-CONTRACT-001] This document binds every C and C++ source file under `Zenith2/Engine/`, `Zenith2/Tools/`, `Zenith2/Player/` and `Zenith2/Benchmarks/`, and every generated C++ file emitted into those libraries' build outputs.
- [Z2-N-CONTRACT-002] Third-party source pinned in `Zenith2/ThirdParty/manifest.json` is exempt from every rule in this document except confinement (see "Third-party confinement" under "API boundary"); Zenith2 code that wraps it is not exempt.
- [Z2-N-CONTRACT-003] A game under `Zenith2/Games/<Name>/` contains no C++ (Z2-D-002); the `SourceLayout` gate step rejects any compiled-language source under `Zenith2/Games/`.
- [Z2-N-CONTRACT-004] Where a design document under `Zenith2/Docs/Design/` departs from a contract in this document, the departure cites the `Z2-D-` decision that allows it at the point of departure; an uncited departure is a contradiction and is reported, never followed.
- [Z2-N-CONTRACT-005] A worker who finds a contradiction between a ticket, this document, a design document and the code stops, reports the contradiction in its result, and makes no change that picks a side (see "Review and change management").

### Target classes

<!-- elaboration -->
| Class | Members | Linked into a shipping Player | STL outside the allowed list |
|---|---|---|---|
| Runtime library | every `Zenith2/Engine/<Lib>` target, including API, backend and module libraries and the host-only libraries outside every Shipping closure ([Z2-N-CONTRACT-577]) | yes, when selected by `game.json` (never, for a host-only library) | no (see "Containers and standard library policy") |
| Runtime executable | `Zenith2/Player` (the shared main), and each game's executable built by `zenith_add_game` | is the shipping binary | no |
| Tooling target | `ZenithEditor`, `ZenithCook`, `ZenithShaderCompiler`, `Setup` and `NewGame` executables under `Zenith2/Tools/` | no | yes, in the tool's own sources |
| Test target | `<Lib>_Tests` per library, conformance executables, the gate fixtures' C++ | no | yes |
| Benchmark target | `Zenith2/Benchmarks/**` harnesses | no | yes |

- [Z2-N-CONTRACT-006] Runtime-library rules apply to a runtime library's public headers even when the including file belongs to a tooling, test or benchmark target, because the header is compiled into the runtime.
- [Z2-N-CONTRACT-577] A host-only engine library, one under `Zenith2/Engine/` that is outside every Shipping closure ([Z2-N-CONTRACT-179]) such as `ZenithScriptCompiler`, is a runtime library for [Z2-N-CONTRACT-271] and ForbiddenCheck rule F4 in its public and `Internal/` sources alike: it reaches host files, directories and processes only through an interface its tooling or test caller implements and passes in (ZenithScriptCompiler's `Zenith_ScriptCookHost`, `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptCook header"), and starts threads only through `Zenith_Thread` (see "Zenith_Multithreading" under "Threading and concurrency contract").

### Glossary

| Term | Meaning in this document |
|---|---|
| Public header | a `.h` file directly in a library's root directory `Zenith2/Engine/<Lib>/`, its companion public `.inl` in the same directory, or a generated header a target lists as a public API output |
| Private header | any header under `<Lib>/Internal/` |
| API library | a library that declares an interface and no factory, e.g. `ZenithPhysics` |
| Backend library | a library that implements an API library's interface and exports its factory, e.g. `ZenithPhysics_Jolt` |
| Module | an optional static library a game selects in `game.json` (Z2-D-024) |
| Composition root | the Player main and the `ZenithRuntime` code that constructs every subsystem and passes dependencies (Z2-D-081) |
| Programmer error | a violated precondition inside engine code; handled by an assert |
| Recoverable error | a failure a correct program can meet (I/O, allocation, corrupt data, script misuse); handled by `Zenith_Result` or a controlled Luau error |
| Sync point | the boundary between two frame phases where commands are applied (Z2-D-080) |
| Fix version | a board milestone `Z2 Stage 0` … `Z2 Stage 7` |
| Debug build | a configuration defining `ZENITH_DEBUG`: the `win-debug` preset, the Debug configuration of `win-tools`, and the Android Debug build type |
| Sanitizer build | a configuration defining `ZENITH_SANITIZER_ADDRESS` (`win-asan`), `ZENITH_SANITIZER_HWADDRESS` (the S24 HWASan build type) or `ZENITH_SANITIZER_THREAD` (`linux-tsan`) |

### Governing decisions

| ID | Subject in this document |
|---|---|
| Z2-D-009 | C++20 |
| Z2-D-017 | the Luau exception firewall |
| Z2-D-034 | Zenith Hungarian naming |
| Z2-D-038 | serialization never depends on C++ layout |
| Z2-D-040 | ownership: `Zenith_Owned<T>`, generational handles, no `Zenith_Ptr`, no STL smart pointers |
| Z2-D-041 | containers and the allowed standard headers |
| Z2-D-042 | the hard gates |
| Z2-D-045 | no exceptions outside the Luau firewall; `Zenith_Result`; asserts |
| Z2-D-046 | no mutable global state except the root heap and profiler hooks |
| Z2-D-047 | contract blocks per type |
| Z2-D-048 | compatibility policy per kind |
| Z2-D-049 | diff-derived API review |
| Z2-D-085 | living documents edited in the same diff |

### Related documents

| Document | Relationship |
|---|---|
| `Zenith2/Docs/Architecture.md` | library graph, frame phases, ECS model, determinism mechanisms; this document states the contracts those structures obey |
| `Zenith2/Docs/Rendering.md` | GPU lifetime and synchronisation contracts; device handles defined here are consumed there |
| `Zenith2/Docs/Scripting.md` | the Luau side of the exception firewall, transactions and script-facing errors |
| `Zenith2/Docs/Budgets.md` | the numeric rows that capacities, caps and regression tolerances reference |
| `Zenith2/Docs/DecisionLog.md` | every `Z2-D-` ID cited here |
| `Zenith2/Docs/WorkerGuide.md` | the short worker-facing summary of this document, with examples |
| `Zenith2/CLAUDE.md` | the binding rules file, which points here |
| `Zenith2/Docs/Contracts/Memory.md` | the memory mechanisms of "Ownership and allocation", split out for length; its headings are listed in "Mechanism sections in Contracts/Memory.md" |

## API boundary

<!-- elaboration -->
### Library directory layout

```
Zenith2/Engine/<Lib>/
	CMakeLists.txt                one zenith_add_library(<Lib> DEPENDS ...) call
	Zenith_<Thing>.h              public headers: the library's whole API
	Zenith_<Thing>.inl            optional public template and inline definitions for Zenith_<Thing>.h
	Internal/                     private headers and every .cpp of the library
	Internal/Windows/             Windows-only sources (filtered by zenith_add_library)
	Internal/Android/             Android-only sources
	Internal/Linux/               linux-tsan host sources
	Platform/<Platform>/          ZenithCore alone: the per-platform threading header ([Z2-N-CONTRACT-583])
	Tests/                        doctest sources, built into <Lib>_Tests
	Tests/Examples/               compiled examples of the public API
```

- [Z2-N-CONTRACT-021] A library's root directory holds its public headers (`Zenith_<Thing>.h`), their optional companion public inline files (`Zenith_<Thing>.inl`) and its `CMakeLists.txt`, and nothing else; every `.cpp` and `.c` of the library lives under `Internal/` or `Tests/`.
- [Z2-N-CONTRACT-046] Template and inline definitions of a public header live in the header itself or in a public `Zenith_<Thing>.inl` beside it, included at the end of `Zenith_<Thing>.h` and by no other file (`ForbiddenCheck` rule F19); a public `.inl` declares no namespace-scope entity its `.h` does not declare, obeys every public-header rule, and is never placed under `Internal/`, because an `Internal/` file reachable from a public header is a `Libgraph` leakage finding.
- [Z2-N-CONTRACT-022] Platform-specific sources live under `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/`, and `zenith_add_library()` selects them per platform; the `SourceLayout` gate step fails a platform source anywhere else.
- [Z2-N-CONTRACT-023] Test sources live under `Tests/` and feed the separate `<Lib>_Tests` target; the `SourceLayout` and `TestSeparationCheck` gate steps fail test code anywhere else.
- [Z2-N-CONTRACT-024] A header under `<Lib>/Internal/` is included by sources of `<Lib>` and of `<Lib>_Tests` and by no other target; the `Libgraph` gate step fails an `Internal/` encapsulation break.
- [Z2-N-CONTRACT-051] One exception to the `Internal/` rule exists: a library declared `KIND BACKEND` with `BACKEND_OF <ApiLib>` may include `<ApiLib>/Internal/Zenith_*Base.h` headers (the backend-neutral half the backends share, e.g. `ZenithRHI/Internal/Zenith_RHIDeviceBase.h`) and the headers those files include from the same `<ApiLib>/Internal/` directory. `Libgraph` allows an include edge from a source of target B to a file F under `A/Internal/` exactly when `declared_deps.json` records B's kind as `BACKEND` and its `BACKEND_OF` as A, and F matches `Zenith_*Base.h` or is reachable from such a file through includes that stay inside `A/Internal/`; every other `Internal/` edge still fails.
<!-- elaboration -->
- [Z2-N-CONTRACT-583] One signposted exception to [Z2-N-CONTRACT-022], [Z2-N-CONTRACT-026], [Z2-N-CONTRACT-442] and `ForbiddenCheck` rule F18 exists, by the human's ruling of 2026-10-23 (Z2-D-141), and it follows the old Zenith pattern of one class per platform: ZenithCore's per-platform threading header `Zenith2/Engine/ZenithCore/Platform/<Platform>/Zenith_PlatformThreading.h`, one file each for `Windows`, `Android` and `Linux`, declares that platform's synchronisation class templates (`Zenith_WindowsMutex_T<bool bProfiled>`, `Zenith_AndroidMutex_T<bool bProfiled>`, `Zenith_LinuxMutex_T<bool bProfiled>`, and likewise `…ConditionVariable_T` and `…Semaphore_T`; Z2-D-142), each holding its native object as a typed member: on Android and Linux `pthread_mutex_t m_xMutex`, `pthread_cond_t m_xCondition`, and `sem_t m_xSemaphore` with `uint m_uMaxValue`; on Windows the pointer-sized `SRWLOCK`, `CONDITION_VARIABLE` and semaphore `HANDLE` as `void* m_pSRWLock`, `void* m_pConditionVariable` and `void* m_pHandle`, so `<windows.h>` reaches no public header. `zenith_add_library()` adds exactly `${CMAKE_CURRENT_SOURCE_DIR}/Platform/${CMAKE_SYSTEM_NAME}`, the target platform's directory, to ZenithCore's public include directories (the signposted carve-out of `Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-161], [Z2-N-ARCH-162] and [Z2-N-ARCH-164]); each platform header ends with the six aliases of its templates (`using Zenith_Mutex = Zenith_<Platform>Mutex_T<true>;` and `using Zenith_MutexNoProfiling = Zenith_<Platform>Mutex_T<false>;`, and the same pair for `Zenith_ConditionVariable` with `Zenith_ConditionVariableNoProfiling` and for `Zenith_Semaphore` with `Zenith_SemaphoreNoProfiling`), and the public `Zenith_Mutex.h`, `Zenith_ConditionVariable.h` and `Zenith_Semaphore.h` only `#include "Zenith_PlatformThreading.h"` (the one bare include of an engine header, because a platform-neutral header cannot name the platform class without an `#if`), so the selection is platform code in platform files chosen by the build, never a preprocessor conditional (rule F11); `Zenith_LockDesc` has its own public header `ZenithCore/Zenith_LockDesc.h`, which `Zenith_Mutex.h` includes first and the platform header includes for the mutex's desc; the out-of-line definitions stay under `ZenithCore/Internal/<Platform>/`. The Android and Linux files may include `<pthread.h>` and `<semaphore.h>` and no other platform header; the Windows file includes none. No synchronisation type holds an opaque native byte array (`m_auNative` does not exist). These classes are the one case where a type's layout differs between platforms ([Z2-N-CONTRACT-441]); on one platform each template has one layout in every configuration and in both of its instantiations, which differ in behaviour alone ([Z2-N-CONTRACT-589]).
- [Z2-N-CONTRACT-584] The exception of [Z2-N-CONTRACT-583] is signposted in documentation and in code, as every exemption is. In documentation: this bullet and [Z2-N-CONTRACT-583] are its one statement, every document sentence that relies on it links `Zenith2/Docs/Contracts.md → "Library directory layout"` [Z2-N-CONTRACT-583], and `Zenith2/Tools/Gates/platform_headers.json` carries one `publicPlatformHeaders` element per POSIX platform file ([Z2-N-CONTRACT-432]), `{ "path": "Zenith2/Engine/ZenithCore/Platform/Android/Zenith_PlatformThreading.h", "platform": "Android", "headers": [ "pthread.h", "semaphore.h" ], "comment": "ZENITH-EXEMPTION: <text>" }` and its Linux twin, whose `comment` states why the include exists and names Z2-D-141 and the signpost `Zenith2/Docs/Contracts.md → "Library directory layout" [Z2-N-CONTRACT-583]`, in the manner of [Z2-N-CONTRACT-153]; the Windows file has no element, so `<windows.h>` there stays an F18 finding. In code: each of the three platform files carries the four-line comment block of [Z2-N-CONTRACT-578] with the file as its subject, the Android and Linux files directly above their platform includes, as below, and the Windows file directly after `#pragma once`. The `SourceLayout` step accepts these three files outside `Internal/<Platform>/` and no other platform source (rules SL3 and SL4 of `Zenith2/Docs/Design/PROC/BaseSteps.md` → "SourceLayout"), and `ForbiddenCheck` admits the two POSIX includes through the elements (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck allowlist files"). The template form of Z2-D-142 changes none of the three blocks, because each names its file and the exemption, not a class.

```cpp
// Zenith2/Engine/ZenithCore/Platform/Android/Zenith_PlatformThreading.h
#pragma once
#include "ZenithCore/Zenith_LockDesc.h"
#include "ZenithCore/Zenith_Types.h"

// ZENITH-EXEMPTION Platform/Android/Zenith_PlatformThreading.h: Z2-D-141
// Allowlist: Zenith2/Tools/Gates/platform_headers.json, publicPlatformHeaders element "Zenith2/Engine/ZenithCore/Platform/Android/Zenith_PlatformThreading.h"
// Contract: Zenith2/Docs/Contracts.md → "Library directory layout" [Z2-N-CONTRACT-583]
// Why: the per-platform threading classes hold their native pthread and semaphore objects as typed members, the old Zenith pattern.
#include <pthread.h>
#include <semaphore.h>

class Zenith_AndroidMutex { /* … pthread_mutex_t m_xMutex; Zenith_LockDesc m_xDesc; */ };
class Zenith_AndroidConditionVariable { /* … pthread_cond_t m_xCondition; */ };
class Zenith_AndroidSemaphore { /* … sem_t m_xSemaphore; uint m_uMaxValue; */ };

using Zenith_Mutex = Zenith_AndroidMutex;                           // the aliases end the platform header
using Zenith_ConditionVariable = Zenith_AndroidConditionVariable;
using Zenith_Semaphore = Zenith_AndroidSemaphore;
```

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Mutex.h (platform-neutral: no #if, no alias of its own)
#pragma once
#include "ZenithCore/Zenith_LockDesc.h"
#include "Zenith_PlatformThreading.h"   // resolved in Platform/<Platform>/ ([Z2-N-CONTRACT-583])
```

### Public headers

- [Z2-N-CONTRACT-025] The public API of every library is kept as small as its consumers need; a growth of the inbound surface against `Zenith2/Tools/Gates/libgraph_baseline.json` fails the `Libgraph` gate step unless the diff carries a matching API review record (see "Review and change management").
- [Z2-N-CONTRACT-026] A public header includes only public headers of its own library, public headers of libraries its target declares as direct dependencies, and the allowed standard headers listed in "Containers and standard library policy".
- [Z2-N-CONTRACT-027] A public header compiles on its own in a generated translation unit (the `HeaderSelfContained` gate step).
- [Z2-N-CONTRACT-028] A public header uses `#pragma once` and contains no `using namespace` directive.
- [Z2-N-CONTRACT-029] A public header forward-declares a type it names only by pointer or reference instead of including that type's header.
<!-- elaboration -->
- [Z2-N-CONTRACT-030] Withdrawn (Z2-D-141): the rule that every function of a runtime library's public header is `noexcept`; a Luau binding calls an engine function with no exception specification, because the engine is compiled with exceptions off and re-enters script only through the protected entry ([Z2-N-CONTRACT-475]); [Z2-N-CONTRACT-582] is the rule.
- [Z2-N-CONTRACT-047] Withdrawn (Z2-D-141): the extent of the withdrawn `noexcept` rule (constructors, destructors, operators, functions, templates, pure virtual functions, function-pointer aliases); [Z2-N-CONTRACT-582] is the rule.
- [Z2-N-CONTRACT-048] Withdrawn (Z2-D-141): the explicit `noexcept` on `= default` declarations; a `= default` declaration carries no exception specification ([Z2-N-CONTRACT-582]).
- [Z2-N-CONTRACT-049] Withdrawn (Z2-D-141): the ban on conditional `noexcept(<expression>)` as a refinement of the withdrawn rule; [Z2-N-CONTRACT-582] covers both forms.
- [Z2-N-CONTRACT-582] `noexcept`, in either form, is not written in Zenith2 code (public headers, `Internal/`, tests, tools and benchmarks alike, function-pointer aliases included), except where a language or standard-library mechanism actually requires it, which is very rare; each such use carries, on its own line, the comment `// ZENITH-ALLOW: <reason>` naming that mechanism, the signpost `ForbiddenCheck` rule F25 accepts, and review confirms the reason (Z2-D-141). Exceptions stay disabled outside the Luau firewall ([Z2-N-CONTRACT-394]), so no exception specification is needed for safety.
- [Z2-N-CONTRACT-031] Every public type or interface carries one contract block (see "Contract blocks per type").

### Interfaces and factories

- [Z2-N-CONTRACT-032] A replaceable subsystem is an abstract interface class plus one factory per implementation, and each factory returns `Zenith_Result<Zenith_Owned<I>, E>` where `I` is the interface and `E` the library's error enumeration.
- [Z2-N-CONTRACT-033] An interface class declares a public `virtual ~I() = default;` destructor and no data members.
- [Z2-N-CONTRACT-034] A class implementing an interface names that interface as its first base class, so that the `Zenith_Owned<Derived>` to `Zenith_Owned<Base>` same-address check holds (see "Ownership and allocation").
- [Z2-N-CONTRACT-035] Small value types and algorithms stay concrete classes and free functions; a virtual layer exists for a subsystem that has, or is planned by a decision to have, a second implementation, and for no other reason.
- [Z2-N-CONTRACT-036] A factory takes its configuration as a `const <Name>Desc&` and every dependency as a reference or pointer parameter; it performs no service lookup (see "No mutable global state").
<!-- elaboration -->
- [Z2-N-CONTRACT-037] Factory names follow `Zenith_Create<Interface noun>_<Backend>`, e.g. `Zenith_CreatePhysics_Jolt`, `Zenith_CreatePhysics_PhysX`, `Zenith_CreateWorld_Flecs`; an interface class name carries no `I` prefix.

<!-- informative -->
The declarations below illustrate the shape. The authoritative physics API is specified in `Zenith2/Docs/Design/PHYS.md`.
<!-- /informative -->

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_Physics.h  (API library: interface, no factory)
class Zenith_Physics
{
public:
	virtual ~Zenith_Physics() = default;
	// ... pure virtual API ...
};

// Zenith2/Engine/ZenithPhysics_Jolt/Zenith_Physics_Jolt.h  (backend library: its factory)
Zenith_Result<Zenith_Owned<Zenith_Physics>, Zenith_PhysicsError> Zenith_CreatePhysics_Jolt(
	const Zenith_PhysicsDesc& xDesc, Zenith_JobSystem& xJobs);
```

### Backend libraries and selection

- [Z2-N-CONTRACT-038] Every backend (ECS, RHI, physics, compression) is its own library target, and the API library target exports no factory (Z2-D-001).
- [Z2-N-CONTRACT-039] A backend library's public headers declare its factory and nothing a consumer could depend on beyond the API library's types.
- [Z2-N-CONTRACT-040] A game links the ECS and physics backends it selects in `game.json` at link time, links both compression backends and chooses the codec per asset, and on Windows links both RHI backends and chooses one at runtime.
- [Z2-N-CONTRACT-041] The conformance executables of each backend family link every backend of that family and run one shared suite against each.
- [Z2-N-CONTRACT-042] A backend library's public root headers are included by the composition root and by test, benchmark, fuzz and tool executables, and by nothing else: runtime targets and every other library include only the API library, and the `Libgraph` swap-contract report proves each family's consumers depend on the API library alone.
- [Z2-N-CONTRACT-050] Test, benchmark, fuzz and tool executables may include a backend's public root headers directly, to link every backend for conformance runs or to select one explicitly. `Libgraph` decides this with a target-kind predicate over the CMake File API: a target is exempt when it is an `EXECUTABLE` whose sources all lie under a `Tests/` directory, under `Zenith2/Benchmarks/**`, or under `Zenith2/Tools/**`; every other target, including every library and the Player and game executables, is checked unchanged, and the exemption never covers an include from a public or `Internal/` header of any library.

### Third-party confinement

- [Z2-N-CONTRACT-043] Third-party headers (EnTT, flecs, Jolt, PhysX, zlib, zstd, Vulkan, D3D12, SDL3, Luau, VMA, D3D12MA, ImGui, Tracy and every other manifest entry) are included from files under some library's `Internal/` and from nowhere else; the `Libgraph` gate step fails a leak into a public header, direct or transitive (Z2-D-082).
<!-- elaboration -->
- [Z2-N-CONTRACT-044] Third-party code is compiled with its own warning settings and included as system includes; Zenith2 warning flags apply to Zenith2 files (see "Tests and code hygiene gates").
- [Z2-N-CONTRACT-045] No runtime target links Slang or DXC; the `Libgraph` gate step's link-surface stage fails any Slang or DXC symbol in a runtime target (Z2-D-025).

## Handle kinds

<!-- informative -->
Zenith used raw pointers and a global service locator for shared references. Zenith2 separates five lifetimes, each with its own handle type, so that a reference states who owns the referenced object and when the reference goes stale. Owning a heap object is a sixth relationship, expressed by `Zenith_Owned<T>` (see "Ownership and allocation"), and a borrowed reference for the duration of a call is a plain `T&` or `T*`.
<!-- /informative -->

### Handle kind summary

<!-- elaboration -->
| Kind | Type | Declared in | Size | Owner scope | Serialized | Stale detection |
|---|---|---|---|---|---|---|
| Persistent ID | `Zenith_PersistentID` | ZenithCore `Zenith_PersistentID.h` | 16 bytes | none: an identity, never a reference | yes, always | not applicable: resolution by lookup returns not-found |
| World handle (entity) | `Zenith_EntityHandle` | ZenithECS `Zenith_EntityHandle.h` | 8 bytes | the issuing world | never | generation and world tag compare, every configuration; after a world-tag reissue, per-world initial generations keep a stale handle from aliasing (see "Zenith_EntityHandle") |
| World handle (component) | `Zenith_ComponentHandle` | ZenithECS `Zenith_ComponentHandle.h` | 16 bytes | the issuing world | never | entity generation, world tag and presence check, every configuration |
| Shared asset handle | `Zenith_AssetHandle<T>` | ZenithAssets `Zenith_AssetHandle.h` | 16 bytes | the asset registry, reference-counted, shared across worlds | as the asset's `Zenith_PersistentID` | generation compare, every configuration |
| Device handle | `Zenith_DeviceHandle<TTag>` and its `Flux_*Handle` aliases | ZenithRHI `Zenith_DeviceHandle.h` | 8 bytes | the RHI device (one per process) | never | generation and kind compare, every configuration |
| Operation handle | `Zenith_OperationHandle` | ZenithCore `Zenith_OperationHandle.h` | 16 bytes | the requester (move-only) | never | generation compare, every configuration |

- [Z2-N-CONTRACT-061] Each handle kind is a distinct C++ type; no handle converts implicitly to an integer, to another handle kind, or to a pointer.
- [Z2-N-CONTRACT-062] Resolving a stale, null or foreign handle is a recoverable error returned as `Zenith_Result` in every configuration and never undefined behaviour; generational handles are the use-after-free protection that remains in shipping builds (Z2-D-040).
- [Z2-N-CONTRACT-063] A handle's bit layout is an implementation detail of its declaring library; code outside that library reads a handle only through its public accessors.
- [Z2-N-CONTRACT-064] A handle kind marked "never" in the Serialized column is not written into any save, cooked asset, authoring document, replay or network message; the serializer writes the referenced object's `Zenith_PersistentID` or network ID instead (Z2-D-038).

### Zenith_PersistentID

<!-- elaboration -->
```cpp
struct Zenith_PersistentID
{
	uint64 m_ulHigh = 0;
	uint64 m_ulLow = 0;

	constexpr bool IsValid() const { return (m_ulHigh | m_ulLow) != 0; }
	friend constexpr bool operator==(const Zenith_PersistentID&, const Zenith_PersistentID&) = default;
	friend constexpr std::strong_ordering operator<=>(const Zenith_PersistentID&, const Zenith_PersistentID&) = default;
};
static_assert(sizeof(Zenith_PersistentID) == 16);
```

- [Z2-N-CONTRACT-065] A `Zenith_PersistentID` is a 128-bit random identity (RFC 9562 version 4 layout) created by `Zenith_GeneratePersistentID(Zenith_RandomSource&)`; all-zero is the null ID and is never issued.
- [Z2-N-CONTRACT-066] GUIDs of assets and documents and the stable authoring IDs of objects inside authoring documents (entities, prefab instances, override targets) are all `Zenith_PersistentID` values; an ID once issued is never reused for a different object.
- [Z2-N-CONTRACT-067] The JSON form of a `Zenith_PersistentID` is 36 lowercase characters `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` (the high word first); the cooked binary form is 16 bytes, the high word then the low word, each little-endian.
- [Z2-N-CONTRACT-068] Network IDs and the per-entity stable simulation ID used for script randomness are separate identities, distinct from persistent IDs and from world handles (see `Zenith2/Docs/Architecture.md → "Multiplayer foundations"` and `Zenith2/Docs/Scripting.md → "Determinism rules and randomness"`).

### Zenith_EntityHandle

<!-- elaboration -->
| Bits | Field | Range | Rule |
|---|---|---|---|
| 0–23 | slot index | 0 … 16,777,215 per world | a world holds at most 2^24 live entities |
| 24–51 | generation | 1 … 2^28 − 1 | starts at the world's initial generation; incremented when the slot's destroy is played back; 0 never appears in a live handle |
| 52–63 | world tag | 1 … 4095 | assigned by the world registry at world creation; 0 is the null world |

- [Z2-N-CONTRACT-069] The all-zero `Zenith_EntityHandle` is the null handle; a live handle has a non-zero generation and a non-zero world tag.
- [Z2-N-CONTRACT-070] A slot whose generation reaches 2^28 − 1 is retired and never reissued in that world, so a stale handle never aliases an entity created afterwards.
- [Z2-N-CONTRACT-071] The world registry `Zenith_WorldRegistry`, declared in ZenithECS (the API library, below every backend), allocates world tags round-robin over 1 … 4095, skipping tags of live worlds, so a destroyed world's tag is reissued as late as possible; the registry is an object the composition root owns and passes in, never global state.
- [Z2-N-CONTRACT-098] Creating a world while all 4,095 tags belong to live worlds fails with the recoverable error `ECS_ERROR_WORLD_TAGS_EXHAUSTED` of `Zenith_ECSError`, returned as `Zenith_Result` by `Zenith_WorldRegistry` before any ECS backend's world factory is called, never an assert.
- [Z2-N-CONTRACT-099] Reissuing a tag leaves a residual aliasing window: a stale handle (tag T, slot S, generation G) of a destroyed world would resolve in a subsequently created world holding tag T if that world's slot S were at generation G. It is closed by the initial generation `Zenith_WorldRegistry` issues with each tag, in two cases: a tag issued before, whose destroyed world reported its highest generation h > 0 through `ReleaseTag`, starts at h + 1, wrapping past 2^28 − 1 to 1 (`Zenith2/Docs/Architecture/ECSModel.md` [Z2-N-ARCH-725]); a tag never issued starts at the offset `1 + ((uWorldCreationCounter × 65,537) mod 2^27)`, computed from the registry's own count of worlds created so far. Aliasing then needs the generation counter of one tag to wrap; the remaining risk is accepted and documented in the handle's contract block.
- [Z2-N-CONTRACT-100] The ECS-layer registry computes the initial generation and hands the tag and the generation down in the `Zenith_WorldDesc` passed to `Zenith_CreateWorld_<Backend>`: `Zenith_WorldDesc::m_uWorldTag` (1 … 4095) and `Zenith_WorldDesc::m_xEntityTable`, a `Zenith_EntityTableDesc` whose `m_uInitialGeneration` carries it (never 0); backend world factories read both from the desc and never read registry state. A test passes 2^28 − 2 directly so that `Test_EntityHandle_GenerationExhaustion_RetiresSlot` reaches retirement in one destroy, with no test switch in shipping code.
- [Z2-N-CONTRACT-072] An entity handle becomes stale when the entity's destroy command is played back at the end of its tick; within that tick the handle still resolves (Z2-D-080).
- [Z2-N-CONTRACT-073] Resolving an entity handle in a world other than the issuing world fails with the wrong-world error in every configuration.

### Zenith_ComponentHandle

<!-- elaboration -->
```cpp
class Zenith_ComponentHandle
{
public:
	Zenith_EntityHandle GetEntity() const;
	Zenith_TypeID GetTypeID() const;
private:
	Zenith_EntityHandle m_xEntity;   // 8 bytes
	Zenith_TypeID m_xTypeID;         // 8 bytes: the reflection type ID (see "Reflection")
};
```

- [Z2-N-CONTRACT-074] A `Zenith_ComponentHandle` resolves when its entity handle resolves and the entity has a component of its type; it becomes stale when either condition fails.
- [Z2-N-CONTRACT-075] A component handle does not distinguish a component that was removed and re-added to the same live entity; a consumer that needs that distinction reads the component's change-tracking version (see `Zenith2/Docs/Architecture.md → "ECS model"`).
- [Z2-N-CONTRACT-076] A chunk-system view is not a handle; its validity is an invocation-ID and structural-epoch compare defined in `Zenith2/Docs/Scripting.md → "Views epochs and misuse errors"`.

### Views and structural epochs

- [Z2-N-CONTRACT-077] Structural changes (spawn, destroy, add and remove component) go through command buffers and are played back at sync points, never applied immediately from a system or script.
- [Z2-N-CONTRACT-078] Each chunk has a structural epoch that is bumped only when structural commands are played back at a sync point; change-tracking versions are per column, separate from the structural epoch, and advance at a commit's merge point.
- [Z2-N-CONTRACT-079] A view is scoped to its invocation ID and becomes invalid when that invocation ends; a commit never invalidates another invocation's view.
- [Z2-N-CONTRACT-080] A retained or stale view raises a controlled Luau error in every build, through an invocation-ID and structural-epoch compare (see `Zenith2/Docs/Scripting.md → "Views epochs and misuse errors"`).
- [Z2-N-CONTRACT-081] Worlds are isolated: each owns its ECS storage, physics world and script VMs, and no handle, view or pointer from one world is valid in another (see `Zenith2/Docs/Architecture.md → "Worlds and isolation"`).

### Zenith_AssetHandle

<!-- elaboration -->
```cpp
template<typename TAsset>
class Zenith_AssetHandle
{
public:
	constexpr Zenith_AssetHandle() = default;                    // null
	Zenith_AssetHandle(const Zenith_AssetHandle& xOther);        // atomic reference increment
	Zenith_AssetHandle& operator=(const Zenith_AssetHandle& xOther);
	Zenith_AssetHandle(Zenith_AssetHandle&& xOther);             // no count change
	Zenith_AssetHandle& operator=(Zenith_AssetHandle&& xOther);
	~Zenith_AssetHandle();                                       // atomic reference decrement
	bool IsNull() const;
	Zenith_PersistentID GetPersistentID() const;
private:
	Zenith_AssetRegistry* m_pxRegistry = nullptr;
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
};
```

- [Z2-N-CONTRACT-082] A non-null `Zenith_AssetHandle<T>` holds one reference counted by the asset registry; copying adds a reference, destroying or reassigning drops one, and moving transfers it.
- [Z2-N-CONTRACT-083] An asset whose reference count reaches zero is retired by the registry through deferred deletion; for GPU-backed assets retirement waits until every queue has completed the frames that referenced it (see `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`).
- [Z2-N-CONTRACT-084] Asset handles are shared across worlds; a world's destruction drops its own references and never forces an asset's destruction.
- [Z2-N-CONTRACT-085] The asset registry outlives every asset handle; the shutdown order in "Shutdown ownership order" guarantees it, and the registry reports every slot still referenced when it is destroyed.
- [Z2-N-CONTRACT-086] An asset handle is serialized as the asset's `Zenith_PersistentID` and resolved on load; an unresolvable ID produces a null handle and a diagnostic, never an assert.
- [Z2-N-CONTRACT-087] Reflected component fields of asset-handle type copy and destroy through the reflected lifecycle hooks, so ECS storage never copies an asset handle's bytes without adjusting its reference count.

### Zenith_DeviceHandle

<!-- elaboration -->
| Bits | Field | Rule |
|---|---|---|
| 0–31 | slot index | index into the device's resource table of that kind |
| 32–55 | generation | incremented at retirement; 0 never appears in a live handle |
| 56–63 | kind | `DEVICE_HANDLE_KIND_*` value, checked on every resolution |

```cpp
template<typename TTag>
class Zenith_DeviceHandle
{
public:
	constexpr bool IsNull() const { return m_ulBits == 0; }
private:
	uint64 m_ulBits = 0;
};
// The tag types are ZenithRHI's (Zenith_RHI prefix): ZenithRHI declares them and cannot depend on ZenithFlux.
// Each tag carries its DEVICE_HANDLE_KIND_* value as eKIND.
struct Zenith_RHITextureTag;
struct Zenith_RHIBufferTag;
struct Zenith_RHISamplerTag;
struct Zenith_RHIPipelineTag;
using Flux_TextureHandle  = Zenith_DeviceHandle<Zenith_RHITextureTag>;
using Flux_BufferHandle   = Zenith_DeviceHandle<Zenith_RHIBufferTag>;
using Flux_SamplerHandle  = Zenith_DeviceHandle<Zenith_RHISamplerTag>;
using Flux_PipelineHandle = Zenith_DeviceHandle<Zenith_RHIPipelineTag>;
```

- [Z2-N-CONTRACT-102] The tag types of `Zenith_DeviceHandle` are named `Zenith_RHI<Kind>Tag` (`Zenith_RHITextureTag`, `Zenith_RHIBufferTag`, `Zenith_RHISamplerTag`, `Zenith_RHIPipelineTag`, `Zenith_RHIHeapTag`, `Zenith_RHIQueryPoolTag`) and declared in ZenithRHI's `Zenith_DeviceHandle.h`, because RHI classes are `Zenith_RHI*` classes; the `Flux_*Handle` aliases above keep their names, and the complete tag and alias list is `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_DeviceHandle header".

- [Z2-N-CONTRACT-088] A device handle names an RHI resource owned by the device; the code that created it destroys it through the device, and the device retires the resource after every queue's last use has completed (see `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`).
- [Z2-N-CONTRACT-089] A device handle is invalid in any other device instance and after the device's destruction; the device reports every live handle of every kind when it is destroyed. Zenith2 creates one RHI device per process, which is why the handle carries no device tag.
- [Z2-N-CONTRACT-090] A bindless descriptor index is not a device handle; descriptor slots are reused after the same retirement as the resource (see `Zenith2/Docs/Rendering.md → "Binding model"`).

### Zenith_OperationHandle

<!-- elaboration -->
```cpp
enum Zenith_OperationStatus : uint8
{
	OPERATION_STATUS_PENDING,     // issued, not started
	OPERATION_STATUS_RUNNING,
	OPERATION_STATUS_SUCCEEDED,   // results owned by the requester until taken or released
	OPERATION_STATUS_FAILED,      // error code available; no results
	OPERATION_STATUS_CANCELLED,   // everything the operation held has been released
};

class Zenith_OperationHandle
{
public:
	constexpr Zenith_OperationHandle() = default;  // null
	Zenith_OperationHandle(const Zenith_OperationHandle&) = delete;
	Zenith_OperationHandle& operator=(const Zenith_OperationHandle&) = delete;
	Zenith_OperationHandle(Zenith_OperationHandle&& xOther);              // xOther becomes null
	Zenith_OperationHandle& operator=(Zenith_OperationHandle&& xOther);   // cancels and detaches the current operation first
	~Zenith_OperationHandle();                    // RequestCancel() then detach if not complete
	bool IsNull() const;
	Zenith_OperationStatus GetStatus() const;
	float GetProgress() const;       // 0.0 … 1.0, monotonic
	bool IsComplete() const;         // SUCCEEDED, FAILED or CANCELLED
	void RequestCancel();                         // idempotent, never blocks
	uint64 GetIssueID() const;
	uint16 GetFailureCode() const;   // the issuing subsystem's error enumerator value; 0 unless FAILED
	Zenith_SlotID GetSlotID() const;  // slot and generation, for diagnostics and handle tests
private:
	Zenith_OperationTable* m_pxTable = nullptr;
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
};
```

- [Z2-N-CONTRACT-091] An operation handle is owned by its requester and is move-only; destroying an incomplete handle requests cancellation and detaches, and the operation then releases its own resources (see "Cancellation completion").
- [Z2-N-CONTRACT-092] Results of a succeeded operation are taken through the issuing subsystem's typed accessor (e.g. a readback's data span) and are owned by the requester from completion until taken or released; a readback's result lifetime is documented in the issuing type's contract block.
- [Z2-N-CONTRACT-093] An operation's result becomes visible to simulation state only at a tick boundary, in completion-ID order, and the publication tick is written to the replay log (Z2-D-053).
- [Z2-N-CONTRACT-094] Progress is monotonic and reaches 1.0 no earlier than the status becoming `OPERATION_STATUS_SUCCEEDED`.
- [Z2-N-CONTRACT-101] `GetFailureCode()` returns the issuing subsystem's error enumerator value when the status is `OPERATION_STATUS_FAILED` and 0 otherwise; `GetSlotID()` returns the `Zenith_SlotID` (slot and generation, ZenithCore) of the operation's table entry, whose slot is always below the table's capacity, so an issuing subsystem may index per-operation state by it (see `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation").

### Frame-scoped graph handles

- [Z2-N-CONTRACT-095] Render-graph virtual handles (`Flux_RGTexture`, `Flux_RGBuffer` and their views) and CPU-frame-graph node handles are frame-scoped values allocated in the graph's arena; they are not one of the five handle kinds and are invalid after the graph that issued them is reset (see `Zenith2/Docs/Rendering.md → "Render graph"`).

### Handle tests

<!-- elaboration -->
- [Z2-N-CONTRACT-096] Each handle type's library carries the tests `Test_<Handle>_NullDoesNotResolve`, `Test_<Handle>_StaleAfterDestroy_DoesNotResolve`, `Test_<Handle>_ForeignOwner_DoesNotResolve` and, for generational handles, `Test_<Handle>_GenerationExhaustion_RetiresSlot`, each run in every configuration including `win-release`.
- [Z2-N-CONTRACT-097] `Zenith_AssetHandle` carries `Test_AssetHandle_CopyMoveDestroy_BalancesReferences`, which asserts the registry's reference count after every copy, move, assignment and destruction and asserts retirement at zero.

## Contract blocks per type

<!-- informative -->
Contracts are written per type or interface, not per declaration (Z2-D-047). A block states the six properties that a caller needs before using a type. Individual declarations are documented only where they depart from the type's block. Implementation comments stay minimal but explain non-obvious algorithms, invariants and performance decisions; history never appears in source.
<!-- /informative -->

### Which declarations carry a block

- [Z2-N-CONTRACT-111] Every `class`, `struct`, `union` and `enum` defined at namespace scope in a public header carries its own contract block immediately before its declaration; a `template<…>` line may sit between the block and the declaration.
- [Z2-N-CONTRACT-112] A namespace-scope function, type alias, concept or variable template in a public header is covered by the nearest preceding contract block in the same header, which is either the block of a type or a `functions` block.
- [Z2-N-CONTRACT-113] A nested type is covered by its enclosing type's block and needs no block of its own.
- [Z2-N-CONTRACT-114] A generator that emits a public header (for example the shader compiler's `<Name>.generated.h`) emits a contract block for every type it defines.

### Block grammar

<!-- elaboration -->
```
block        := open-line field-line+ close-line
open-line    := "/// @contract " kind " " name EOL
kind         := "interface" | "type" | "value" | "container" | "enum" | "functions"
name         := the C++ identifier of the following type, or a group name for "functions"
field-line   := "/// " field-name ": " text EOL continuation*
continuation := "///   " text EOL                      (three spaces after the slashes)
close-line   := "/// @endcontract" EOL
field-name   := "Ownership" | "Thread affinity" | "Invalidation" | "Units" | "Errors"
			  | "Reentrancy" | "Version" | "Alignment" | "Overflow" | "Allocation failure"
			  | "Complexity" | "Locks" | "Example"
```

| Field | Required for kinds | Content |
|---|---|---|
| `Ownership` | all | who creates, owns and destroys instances; which allocation tier backs them |
| `Thread affinity` | all | a keyword from "Thread affinity vocabulary", then a sentence |
| `Invalidation` | all | what invalidates instances, pointers, spans, views or handles obtained from the type; `None.` when nothing does |
| `Units` | all | units of every quantity (SI plus the conventions of `Zenith2/Docs/Design/CORE.md`); `None.` when no quantity appears |
| `Errors` | all | the `Zenith_Result` error enumeration, which failures are recoverable, which preconditions are asserts; `None.` when nothing can fail |
| `Reentrancy` | `interface`, and any type that invokes a callback | a keyword from "Reentrancy notation", then a sentence |
| `Version` | all | `since Z2 Stage <N>` then the compatibility statement per "Compatibility policy per kind" |
| `Alignment` | `container` | element and storage alignment guarantees |
| `Overflow` | `container` | what happens at maximum size or on size arithmetic overflow |
| `Allocation failure` | `container` | behaviour of every allocating operation on failure, per "Container allocation failure" |
| `Complexity` | `container` | time complexity of every operation, amortised where noted |
| `Locks` | any type holding a `Zenith_Mutex` | the lock levels it acquires, from "Lock ordering table" |
| `Example` | optional | the repo-relative path of a compiled example under `Tests/Examples/`, or under an API library's `Tests/Conformance/Examples/` for a backend-parameterised example ([Z2-N-CONTRACT-421]) |

- [Z2-N-CONTRACT-115] A contract block uses exactly the grammar above: the required fields of its kind appear once each, in the order of the field table, and every field has non-empty text.
- [Z2-N-CONTRACT-116] The literal text `None.` is accepted for `Invalidation` and `Units`, and for `Errors` of a type or function that cannot fail (it returns no `Zenith_Result` and asserts nothing about its caller's input), and for no other field.
- [Z2-N-CONTRACT-117] A `Version` field starts with `since Z2 Stage <N>` and continues with `;`-separated segments (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Version field clauses"): a deprecated type continues `; deprecated Z2 Stage <D>, use <Replacement>, removed Z2 Stage <D+1>`, an experimental block `; experimental until Z2 Stage <G> (consumers <targets>)` and an internal block `; internal`, with D the engine MAJOR when the deprecating diff lands; free segments stay valid; and a member, function or enumerator is deprecated through `/// @deviation Version: deprecated Z2 Stage <D>, use <R>, removed Z2 Stage <D+1>`.
- [Z2-N-CONTRACT-118] An `interface` block's type declares at least one pure virtual function and a virtual destructor; a `value` block's type is trivially copyable; a `container` block carries the four container fields.
- [Z2-N-CONTRACT-585] A `struct` or `class` whose block's `Thread affinity` field begins with `IMMUTABLE` declares its data members `const` wherever that is possible, so the values fixed at construction cannot be written afterwards (Z2-D-141). A member stays non-const only when `const` would break an operation the type needs (copy or move assignment by an API or a container, as by `Zenith_Result` or a sorted or ring-buffered array; default construction followed by filling; a builder or decoder that writes fields after construction), and then the `Thread affinity` field names that operation in one clause, for example `IMMUTABLE. … Members non-const: held as the error of a Zenith_Result, whose move assignment assigns it.` Every `const` data member carries a default member initializer (`const uint32 m_uMajor = 0;`), so the type stays default-constructible, as `Zenith_Result` ([Z2-N-CONTRACT-587]) and a fixed array of the type require, and stays aggregate-initialisable; such a type is not copy- or move-assignable, which is acceptable when no API assigns it (`Zenith_Result`'s move assignment is then never instantiated for it). A trivially copyable descriptor that callers aggregate-initialise and pass by `const&` takes `const` members; one that callers default-construct and then fill does not. Review confirms each judgement.

### Declaration-level deviations

- [Z2-N-CONTRACT-119] A declaration that follows its type's contract carries no documentation comment of its own; the type's block covers it.
- [Z2-N-CONTRACT-120] A member or function that departs from its type's block carries a single `/// @deviation <Field name>: <text>` line immediately before its declaration, naming the field it overrides; it carries no other documentation comment unless an algorithm needs explaining.

### Example interface contract block

<!-- informative -->
Declaration names in the two examples illustrate the block format. The authoritative compression API is specified in `Zenith2/Docs/Design/COMPRESS.md`, and the authoritative entity handle in `Zenith2/Docs/Design/ECS.md`.
<!-- /informative -->

```cpp
/// @contract interface Zenith_Compression
/// Ownership: Created by Zenith_CreateCompression_Zlib or Zenith_CreateCompression_Zstd as a
///   Zenith_Owned<Zenith_Compression>; the composition root owns one instance per codec for the process lifetime.
/// Thread affinity: ANY_THREAD. Instances hold no mutable state; per-call scratch comes from the caller's arena.
/// Invalidation: None.
/// Units: Sizes in bytes. Levels are codec-defined integers, clamped to [GetMinLevel(), GetMaxLevel()].
/// Errors: Zenith_Result<uint64, Zenith_CompressionError>. Corrupt or truncated input returns
///   COMPRESSION_ERROR_CORRUPT_INPUT and never asserts; a short destination returns COMPRESSION_ERROR_DESTINATION_TOO_SMALL.
///   A null span with a non-zero size is a programmer error (ZENITH_ASSERT).
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 2. Output bytes are identical for the same codec, level and pinned backend version;
///   cooked headers record the codec ID, so a codec change is a re-cook, never a runtime migration.
/// Example: Zenith2/Engine/ZenithCompression/Tests/Examples/Zenith_Compression_Example.cpp
/// @endcontract
class Zenith_Compression
{
public:
	virtual ~Zenith_Compression() = default;
	virtual Zenith_CodecID GetCodecID() const = 0;
	virtual sint32 GetMinLevel() const = 0;
	virtual sint32 GetMaxLevel() const = 0;
	virtual uint64 GetCompressBound(uint64 ulSourceBytes) const = 0;
	virtual Zenith_Result<uint64, Zenith_CompressionError> Compress(
		Zenith_ByteSpan xSource, Zenith_MutableByteSpan xDestination, sint32 iLevel, Zenith_Arena& xScratch) const = 0;
	virtual Zenith_Result<uint64, Zenith_CompressionError> Decompress(
		Zenith_ByteSpan xSource, Zenith_MutableByteSpan xDestination, Zenith_Arena& xScratch) const = 0;
	/// @deviation Thread affinity: OWNER_THREAD. The returned stream is used by one thread at a time.
	virtual Zenith_Result<Zenith_Owned<Zenith_CompressionStream>, Zenith_CompressionError> CreateStream(
		Zenith_CompressionMode eMode, sint32 iLevel) const = 0;
};
```

### Example value type contract block

```cpp
/// @contract value Zenith_EntityHandle
/// Ownership: Plain value. Copying never extends the entity's lifetime; the issuing world owns the entity.
/// Thread affinity: IMMUTABLE. Resolving it follows the issuing world's access rules for the current phase. Members
///   non-const: a handle is copy-assigned wherever it is stored (component fields, containers, Zenith_Result).
/// Invalidation: Stale once the entity's destroy command is played back at the end of the tick; resolution
///   then fails in every configuration (generation compare). Never valid in another world (world-tag compare).
/// Units: None.
/// Errors: Resolution returns Zenith_Result<…, Zenith_ECSError>: ECS_ERROR_NULL_HANDLE, ECS_ERROR_STALE_HANDLE,
///   ECS_ERROR_WRONG_WORLD. No resolution path asserts.
/// Version: since Z2 Stage 2. Never serialized; the bit layout may change in any build.
/// @endcontract
class Zenith_EntityHandle
{
public:
	constexpr Zenith_EntityHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	friend constexpr bool operator==(Zenith_EntityHandle, Zenith_EntityHandle) = default;
private:
	friend class Zenith_EntityTable;
	explicit constexpr Zenith_EntityHandle(uint64 ulBits) : m_ulBits(ulBits) {}
	uint64 m_ulBits = 0;
};
```

### ApiContractCheck behaviour

<!-- elaboration -->
The `ApiContractCheck` gate step (`pwsh -NoProfile -File Zenith2\Tools\Gates\ApiContractCheck.ps1`) runs these steps:

1. **Inputs.** Enumerate public headers: every `.h` directly under `Zenith2/Engine/<Lib>/` plus every generated header a target lists as a public API output in `Zenith2/out/<checkout-hash>/declared_deps.json`; each header's companion `.inl` is read with it.
2. **Front end.** For each header, obtain its translation unit through `New-Zenith2HeaderTU` of the shared gate module (see "Shared gate environment"), which writes `hsc/<Lib>/<Header>.hsc.cpp` when it is absent, so the step does not depend on `HeaderSelfContained` having run. Compile it with the pinned LLVM clang, using the front-end command in the block after this list, under the MSVC environment that `Get-Zenith2MsvcEnvironment` returns: each `INCLUDE` entry of that environment is passed as `-isystem`, the File API `includes[]` of the header's library as `-I` (or `-isystem` where the File API marks them system) and its `defines[]` as `-D`. A header that clang fails to parse is finding `ACC-FRONTEND`, so a front-end failure fails the step; a missing MSVC environment, or a standard header that is not found, is reported as `ACC-FRONTEND: MSVC environment: <header or reason> not found`, so an environment fault is distinguishable from a contract defect.
3. **Declarations.** From the JSON AST keep the nodes whose expansion location is the header or its `.inl`; the `file` key of clang's JSON locations is sticky (it is present just when it differs from the previous location in document order), so the reader carries the last seen `file` forward, reading `range.begin.expansionLoc` before `loc`, and treats `includedFrom` as the marker of a transition into or out of an included file. Kept nodes: at namespace scope `CXXRecordDecl`, `RecordDecl` and `EnumDecl` with `completeDefinition`, `ClassTemplateDecl`, `FunctionDecl`, `FunctionTemplateDecl`, `TypeAliasDecl`, `TypedefDecl`, `ConceptDecl` and `VarTemplateDecl`; inside public types every `CXXMethodDecl`, `CXXConstructorDecl`, `CXXDestructorDecl` and `CXXConversionDecl` that is not `isImplicit`.
4. **Block attachment.** A separate line tokeniser reads the header's `///` lines into blocks per "Block grammar"; each type declaration from step 3 is matched to the block that ends on the line directly above the declaration's first line (the `ClassTemplateDecl` range starts at its `template` line); every other namespace-scope declaration needs a block earlier in the same file at the same namespace depth, where the `.h` and its `.inl` count as one file. A node that is a redeclaration or an out-of-line definition of an entity declared earlier in the `.h` (a `previousDecl` is present, or it is a `CXXMethodDecl` whose `parentDeclContextId` names a covered type) is covered by that entity's block and needs none, which covers the out-of-line member and template definitions of a public `.inl` and inline definitions placed after a class in its `.h`.
5. **Block validation.** Fields, order, non-empty text, the `Thread affinity` and `Reentrancy` keywords, the `Version` prefix, the block `name` against the declaration's name, and the kind checks: an `interface` type has a pure virtual method and a virtual destructor, a `value` type is trivially copyable (`definitionData.isTriviallyCopyable`), a `container` block has the four container fields.
6. **Withdrawn: the exception-specification check.** No step 6 runs (Z2-D-141): `noexcept` is not written in Zenith2 code ([Z2-N-CONTRACT-582]), which review confirms; the step keeps its number so that steps 7 to 10 keep theirs.
7. **Deviations and examples.** Every `@deviation` line names a known field and sits directly above a member or function declaration; every `Example:` path exists under a `Tests/Examples/` directory, or under an API library's `Tests/Conformance/Examples/` directory for a backend-parameterised example, and is compiled into a test target (`<Lib>_Tests` or `<ApiLib>_Conformance` respectively; CMake File API).
8. **`.inl` check.** A namespace-scope declaration located in a `.inl` whose name is not declared in its `.h` is finding `ACC-INL-DECLARATION`.
9. **Output.** Write `Zenith2/out/<checkout-hash>/gates/ApiContractCheck.json` as `{ "file", "line", "rule", "message" }` records, print each as `ApiContractCheck: <file>:<line>: <rule>: <message>`, and exit 1 when any finding exists, 0 otherwise. Rule IDs: `ACC-FRONTEND`, `ACC-BLOCK-MISSING`, `ACC-BLOCK-NAME`, `ACC-FIELD-MISSING`, `ACC-FIELD-ORDER`, `ACC-FIELD-EMPTY`, `ACC-KEYWORD`, `ACC-VERSION`, `ACC-KIND`, `ACC-DEVIATION`, `ACC-EXAMPLE`, `ACC-INL-DECLARATION`.
10. **Self-test.** `Zenith2/Tools/Gates/Tests/ApiContractCheck/` holds one failing fixture header per rule ID and two passing fixtures: a plain header, and a template class whose `.h` has an inline definition after the class and whose `.inl` holds out-of-line member and function-template definitions; the gate's own test asserts each failing fixture is detected with its rule ID and each passing fixture yields no finding.

```
clang++ -std=c++20 -fsyntax-only --target=x86_64-pc-windows-msvc -fms-compatibility-version=<19.minor.build> -Xclang -ast-dump=json <-isystem INCLUDE entries> <-I and -D from the File API> hsc/<Lib>/<Header>.hsc.cpp
```

- [Z2-N-CONTRACT-121] `ApiContractCheck` runs the steps listed above (1 to 5 and 7 to 10; step 6 is withdrawn), in order, over every public header of every library in the checkout, not over changed files alone.
- [Z2-N-CONTRACT-122] `ApiContractCheck` proves that each block exists and is well formed; the accuracy of a block's content is confirmed by review of the diff that adds or changes the type.

### Implementation comments

- [Z2-N-CONTRACT-123] Implementation comments explain non-obvious algorithms, invariants and performance decisions and nothing else; restating the code, recording history, naming tickets or dating changes in source is rejected in review.
- [Z2-N-CONTRACT-124] Comment volume is reported, not gated, by the `ReviewSignals` gate step; the reviewer judges it.

## No mutable global state

### The two installed hooks

- [Z2-N-CONTRACT-141] Runtime code (every source under `Zenith2/Engine/**` and `Zenith2/Player/**` outside `Tests/`) holds no mutable state at namespace scope, in static data members or in function-local statics, with three exceptions: the root heap pointer and the profiler hooks, installed by `main`, and the crash handler anchor, claimed by `Zenith_CrashHandler::Create` at startup step 7 (Z2-D-046, as extended on 2026-10-09 by ruling Q88 (a)), plus the two `thread_local` variables of [Z2-N-CONTRACT-144]; every variable these exceptions admit is an entry of "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]), and nothing else is; tooling, test and benchmark sources follow the same rule, confirmed by review.
<!-- elaboration -->
- [Z2-N-CONTRACT-142] The root heap is installed (into the allowlisted `g_pxRootHeap` of "Writable-data allowlist entries", [Z2-N-CONTRACT-152]) by `Zenith_InstallRootHeap(&xHeap)` as the first statement of every `main` (Player, tools, tests, benchmarks) after `Zenith_ApplyCrtMathMode()`, which touches no Zenith state (Zenith2/Docs/Architecture.md → "Determinism guarantee and mechanisms", [Z2-N-ARCH-889]), and uninstalled by `Zenith_InstallRootHeap(nullptr)` after the last engine object is destroyed.
<!-- elaboration -->
- [Z2-N-CONTRACT-143] The profiler hooks are declared in ZenithCore's public header `Zenith_ProfilerHooks.h` (below), installed by `Zenith_InstallProfilerHooks(&xHooks)` directly after the root heap and the log router's creation (startup step 1b of `Zenith2/Docs/Architecture.md → "Composition root and Player"`) and uninstalled by `Zenith_InstallProfilerHooks(nullptr)` directly before it; the installed table is copied into a single variable in `ZenithCore/Internal/Profiling/` (the allowlisted `g_xProfilerHooks` of "Writable-data allowlist entries", [Z2-N-CONTRACT-152]), so ZenithCore code such as `Zenith_Mutex` reaches it with no upward dependency, and ZenithProfiling supplies the callback implementations. The six profiling hooks are invoked only through the dispatch functions declared in the same header ([Z2-N-CONTRACT-590]); the lock-acquire hook receives the wait the acquiring thread measured, `ulWaitTicks`, in ticks of `Zenith_GetMonotonicTimeNs()` (one tick is one nanosecond), which is 0 for an uncontended acquire (one that did not block) and for a successful `TryLock`, and at least 1 for an acquire that blocked. `pLock` of the lock hooks is an identity token that no hook dereferences, because the lock's owner may be destroyed as soon as the lock is released. `Zenith_InstallProfilerHooks` refuses, with `ZENITH_CHECK` in every build, a table in which exactly one of `m_pfnOnZoneBegin` and `m_pfnOnZoneEnd` is non-null, or exactly one of `m_pfnOnLockAcquire` and `m_pfnOnLockRelease` is non-null, so a dispatched begin always has a dispatchable end and a caller never stores hook pointers to pair them (Z2-D-142).
- [Z2-N-CONTRACT-155] Every counter published through `m_pfnOnCounter` (that is, through `Zenith_DispatchCounter`, [Z2-N-CONTRACT-590]) states its semantics in the `eKind` parameter: `COUNTER_KIND_CUMULATIVE` for a running total, `COUNTER_KIND_DELTA` for a change since the previous call, `COUNTER_KIND_GAUGE` for an instantaneous value (a live count, a resident size, a current level) of which the hook keeps the latest; the counters of "Bounded queues" are cumulative.
- [Z2-N-CONTRACT-156] Until the DEVT hook implementation consumes `eKind`, every delta counter's name also ends in `_delta` (for example `jobs.steals_delta`), so a consumer that ignores the parameter still tells the kinds apart; a cumulative or gauge counter's name never ends in `_delta`. The rule binds every counter name, including names an Architecture or design bullet fixes: such a bullet uses the names this rule produces (for example the ECS counters of `Zenith2/Docs/Architecture/ECSModel.md` [Z2-N-ARCH-713], whose per-tick counts are deltas named `…_delta` and whose entity, chunk, archetype and snapshot-size counts are gauges).
<!-- informative -->
The three log-record members extend the Z2-D-046 exemption: the hook table carries them, the log router that `main` owns is reached through them, and no writable symbol is added, so they add no entry to the writable-data allowlist (its fifth entry, the crash handler anchor, comes from ruling Q88 (a)). The human ruled this extension, together with the Debug thread state, on 2026-10-16 (Q11 (a)); Z2-D-046 records it as a dated extension (Zenith2/Docs/DecisionLog.md → "Z2-D-046 No mutable global state except the root allocator and profiler hooks installed in main; no runtime service lookup"), and every variable that relies on the exemption is listed in "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]).
<!-- /informative -->

- [Z2-N-CONTRACT-157] `Zenith_ProfilerHooks` carries the log-record hook after `m_pUserData`: `m_pfnOnLogRecord`, `m_pfnIsLogLevelEnabled` and `m_pLogUserData`, written by `Zenith_LogRouter::FillHooks` and read by `Zenith_EmitLog`; the record and filter semantics are specified in `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook".
- [Z2-N-CONTRACT-158] `main` creates the log router (startup step 1b) after installing the root heap and before installing the hook table, because the table holds the router's pointer; at shutdown `main` destroys the router after ZenithRuntime and before uninstalling the hooks (shutdown row 11).
- [Z2-N-CONTRACT-590] Every invocation of the six profiling hooks `m_pfnOnZoneBegin`, `m_pfnOnZoneEnd`, `m_pfnOnCounter`, `m_pfnOnLockAcquire`, `m_pfnOnLockRelease` and `m_pfnOnBlockingWait` anywhere in the engine goes through the inline dispatch functions of `Zenith_ProfilerHooks.h` (below): `Zenith_DispatchZoneBegin`, `Zenith_DispatchZoneEnd`, `Zenith_DispatchCounter`, `Zenith_DispatchLockAcquire`, `Zenith_DispatchLockRelease` and `Zenith_DispatchBlockingWait(szPrimitive, ulWaitTicks)`, which a primitive calls after its wait returns. No code outside that header calls a member of the hook table; `Zenith_GetProfilerHooks()` stays public for tests and for the profiler's own install code. A dispatch does nothing when its hook is null. Otherwise it calls `Zenith_EnterHookDispatch()`, which raises the calling thread's `m_uHookDepth` (a member of the Debug thread state, present in every build), calls the hook, and calls `Zenith_LeaveHookDispatch()`. When the depth is already non-zero the dispatch does not call the hook: `Zenith_EnterHookDispatch()` increments the thread's `m_uHookReentryCount` and, wherever `ZENITH_ASSERTS_ENABLED=1`, fails `ZENITH_ASSERT(false, "profiler hook re-entered: a hook used a profiled primitive")`. So a hook that uses a profiled primitive recurses at most one level in any build, and a debug build reports it; in every build the first refusal on each thread also writes one line through `Zenith_WriteDiagnosticLine`, because the count itself is visible only to that thread. A table in which only one of a pair (zone begin and end, lock acquire and release) is set cannot be installed, so a dispatch needs no pairing logic and a caller keeps no hook pointer between a begin and its end; `Zenith_IsZoneHookInstalled()`, `Zenith_IsLockAcquireHookInstalled()` and `Zenith_IsBlockingWaitHookInstalled()` are the three sanctioned cost gates for work done only to feed a zone, a lock report or a wait duration. The log-record members `m_pfnOnLogRecord` and `m_pfnIsLogLevelEnabled` are not dispatched through this guard: the failure procedure emits its fatal log record from inside whatever hook asserted, and a refused dispatch would drop exactly that record; the log router's own locks are unprofiled instead ([Z2-N-CONTRACT-591]). Z2-D-142.
- [Z2-N-CONTRACT-591] A profiled primitive cannot recurse through the profiler, for three independent reasons (Z2-D-142). Types: ZenithProfiling's code and everything ZenithCore calls from inside a hook dispatch (the clock, `Zenith_SpscRing`, the Debug thread state, the failure procedure, the log router with its sink fan-out and ring locks and its drain thread's semaphore) use only `Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling` and `Zenith_SemaphoreNoProfiling`; the lock at level 940 is an ordinary row of the lock ordering table and is not exempted from recording at run time. Gate: `ForbiddenCheck` rule F26 refuses a profiled `Zenith_Mutex`, `Zenith_ConditionVariable` or `Zenith_Semaphore` under `Zenith2/Engine/ZenithProfiling/**`, under `Zenith2/Engine/ZenithCore/Internal/Profiling/**` and in the files of the `profilerReachable` list of `forbidden_allowlist.json`, and refuses a direct use of the hook table outside `Zenith_ProfilerHooks.h` ([Z2-N-CONTRACT-416], [Z2-N-CONTRACT-417]). Run time: the guard of [Z2-N-CONTRACT-590], in every build. The ZenithCore sources a dispatch reaches are listed in `Zenith2/Docs/Design/CORE/Threading.md` → "Lock profiling and hook dispatch".

```cpp
// Zenith2/Engine/ZenithCore/Zenith_ProfilerHooks.h
struct Zenith_LockDesc;                         // declared in Zenith_LockDesc.h
struct Zenith_LogRecord;                        // declared in ZenithCore's logging header
struct Zenith_LogCategory;
enum Zenith_LogLevel : uint8;

struct Zenith_ZoneSite
{
	const char* m_szName = nullptr;
	const char* m_szFile = nullptr;
	uint32 m_uLine = 0;
	uint32 m_uColour = 0;
};

using Zenith_PfnOnZoneBegin = void (*)(const Zenith_ZoneSite& xSite, void* pUserData);
using Zenith_PfnOnZoneEnd = void (*)(void* pUserData);
enum Zenith_CounterKind : uint8
{
	COUNTER_KIND_CUMULATIVE = 1,   // iValue is the running total since start (full events, high-water marks)
	COUNTER_KIND_DELTA,            // iValue is the change since the previous call for this name
	COUNTER_KIND_GAUGE,            // iValue is an instantaneous value; the hook keeps the latest per name
};

using Zenith_PfnOnCounter = void (*)(const char* szName, sint64 iValue, Zenith_CounterKind eKind, void* pUserData);
using Zenith_PfnOnLockAcquire = void (*)(const Zenith_LockDesc& xDesc, const void* pLock, uint64 ulWaitTicks, void* pUserData);
using Zenith_PfnOnLockRelease = void (*)(const Zenith_LockDesc& xDesc, const void* pLock, void* pUserData);   // pLock is an identity token: no hook dereferences it, because the lock's owner may be destroyed once the lock is released
using Zenith_PfnOnBlockingWait = void (*)(const char* szPrimitive, uint64 ulWaitTicks, void* pUserData);   // called after the wait returns; ulWaitTicks is the time it blocked, in nanoseconds of Zenith_GetMonotonicTimeNs()
using Zenith_PfnOnLogRecord = void (*)(const Zenith_LogRecord& xRecord, void* pLogUserData);
using Zenith_PfnIsLogLevelEnabled = bool (*)(const Zenith_LogCategory& xCategory, Zenith_LogLevel eLevel, void* pLogUserData);

struct Zenith_ProfilerHooks
{
	Zenith_PfnOnZoneBegin m_pfnOnZoneBegin = nullptr;
	Zenith_PfnOnZoneEnd m_pfnOnZoneEnd = nullptr;
	Zenith_PfnOnCounter m_pfnOnCounter = nullptr;
	Zenith_PfnOnLockAcquire m_pfnOnLockAcquire = nullptr;
	Zenith_PfnOnLockRelease m_pfnOnLockRelease = nullptr;
	Zenith_PfnOnBlockingWait m_pfnOnBlockingWait = nullptr;
	void* m_pUserData = nullptr;
	Zenith_PfnOnLogRecord m_pfnOnLogRecord = nullptr;               // the log router's record entry
	Zenith_PfnIsLogLevelEnabled m_pfnIsLogLevelEnabled = nullptr;   // the log router's level filter
	void* m_pLogUserData = nullptr;                                 // the Zenith_LogRouter
};

void Zenith_InstallProfilerHooks(const Zenith_ProfilerHooks* pxHooks);   // copies the table; nullptr uninstalls; ZENITH_CHECK (every build) that zone begin and end, and lock acquire and release, are each both set or both null
const Zenith_ProfilerHooks& Zenith_GetProfilerHooks();     // an all-null table when none is installed

// The dispatch functions: the one way any code invokes the six profiling hooks ([Z2-N-CONTRACT-590]).
bool Zenith_EnterHookDispatch();         // Internal/Profiling/Zenith_DebugThreadState.cpp: raises m_uHookDepth and returns true; when the depth is already non-zero, counts the re-entry (and asserts) and returns false
void Zenith_LeaveHookDispatch();         // lowers m_uHookDepth
uint32 Zenith_GetHookDepth();            // the calling thread's m_uHookDepth: 0 outside a hook, 1 inside one
uint32 Zenith_GetHookReentryCount();     // the calling thread's m_uHookReentryCount: dispatches it has refused

inline bool Zenith_IsLockAcquireHookInstalled()
{
	return Zenith_GetProfilerHooks().m_pfnOnLockAcquire != nullptr;   // one of the three sanctioned null tests (with Zenith_IsZoneHookInstalled and Zenith_IsBlockingWaitHookInstalled): a profiled Lock reads the clock on contention only when this is true
}

inline bool Zenith_IsZoneHookInstalled()
{
	return Zenith_GetProfilerHooks().m_pfnOnZoneBegin != nullptr;   // the cost gate for work done only to build a zone, such as formatting its name; the install check pairs begin with end
}

inline bool Zenith_IsBlockingWaitHookInstalled()
{
	return Zenith_GetProfilerHooks().m_pfnOnBlockingWait != nullptr;   // a blocking primitive reads the clock around its wait only when this is true
}

inline void Zenith_DispatchZoneBegin(const Zenith_ZoneSite& xSite)
{
	const Zenith_ProfilerHooks& xHooks = Zenith_GetProfilerHooks();
	if (xHooks.m_pfnOnZoneBegin != nullptr && Zenith_EnterHookDispatch())
	{
		xHooks.m_pfnOnZoneBegin(xSite, xHooks.m_pUserData);
		Zenith_LeaveHookDispatch();
	}
}

inline void Zenith_DispatchZoneEnd()
{
	const Zenith_ProfilerHooks& xHooks = Zenith_GetProfilerHooks();
	if (xHooks.m_pfnOnZoneEnd != nullptr && Zenith_EnterHookDispatch())
	{
		xHooks.m_pfnOnZoneEnd(xHooks.m_pUserData);
		Zenith_LeaveHookDispatch();
	}
}

inline void Zenith_DispatchCounter(const char* szName, sint64 iValue, Zenith_CounterKind eKind)
{
	const Zenith_ProfilerHooks& xHooks = Zenith_GetProfilerHooks();
	if (xHooks.m_pfnOnCounter != nullptr && Zenith_EnterHookDispatch())
	{
		xHooks.m_pfnOnCounter(szName, iValue, eKind, xHooks.m_pUserData);
		Zenith_LeaveHookDispatch();
	}
}

inline void Zenith_DispatchLockAcquire(const Zenith_LockDesc& xDesc, const void* pLock, uint64 ulWaitTicks)
{
	const Zenith_ProfilerHooks& xHooks = Zenith_GetProfilerHooks();
	if (xHooks.m_pfnOnLockAcquire != nullptr && Zenith_EnterHookDispatch())
	{
		xHooks.m_pfnOnLockAcquire(xDesc, pLock, ulWaitTicks, xHooks.m_pUserData);
		Zenith_LeaveHookDispatch();
	}
}

inline void Zenith_DispatchLockRelease(const Zenith_LockDesc& xDesc, const void* pLock)
{
	const Zenith_ProfilerHooks& xHooks = Zenith_GetProfilerHooks();
	if (xHooks.m_pfnOnLockRelease != nullptr && Zenith_EnterHookDispatch())
	{
		xHooks.m_pfnOnLockRelease(xDesc, pLock, xHooks.m_pUserData);
		Zenith_LeaveHookDispatch();
	}
}

inline void Zenith_DispatchBlockingWait(const char* szPrimitive, uint64 ulWaitTicks)
{
	const Zenith_ProfilerHooks& xHooks = Zenith_GetProfilerHooks();
	if (xHooks.m_pfnOnBlockingWait != nullptr && Zenith_EnterHookDispatch())
	{
		xHooks.m_pfnOnBlockingWait(szPrimitive, ulWaitTicks, xHooks.m_pUserData);
		Zenith_LeaveHookDispatch();
	}
}
```
<!-- elaboration -->
- [Z2-N-CONTRACT-144] `thread_local` appears in Zenith2 code in two places alone, each an allowlisted variable of "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]): `Zenith2/Engine/ZenithCore/Internal/Profiling/`, where the Debug thread state `g_xDebugThreadState` is the per-thread state the plan §3.8 lock-order checker needs ("Debug lock-order checker": its held-lock stack), with the flags `m_bJobWorker`, `m_bInOOMHandler` and `m_bInFailure`, the no-allocation depth `m_uNoAllocationDepth` (Z2-D-046 as extended on 2026-10-16 by ruling Q11 (a)) the hook depth `m_uHookDepth` with the re-entry count `m_uHookReentryCount` of [Z2-N-CONTRACT-590], and the log router's nested-record flag `m_bInLogRouter` with the count `m_uLogNestedDropCount` (Z2-D-142; four members of the same variable, so no entry or extension is added); and `Zenith2/Engine/ZenithProfiling/Internal/`, where `g_xProfilerThreadBuffers` holds the per-thread buffers of the hook implementations and of the debug-draw recorder, which caches its ring pointer, owner and generation in the same thread slot and adds no further symbol (Z2-D-046; the recorder's use as extended on 2026-10-16 by ruling Q79 (a)).
- [Z2-N-CONTRACT-145] Immutable data (`constexpr` variables, `constinit const` tables, string literals) is not mutable state and is allowed everywhere.

### Dependency injection

- [Z2-N-CONTRACT-146] Every dependency of an engine object is passed to its constructor or factory as a reference, a pointer or a `Zenith_Owned`, and stored as a member for the object's lifetime.
- [Z2-N-CONTRACT-147] The composition root (`Zenith2/Player/` and `ZenithRuntime`) constructs every subsystem in dependency order and destroys them in reverse; no subsystem constructs a peer subsystem it does not own.
- [Z2-N-CONTRACT-148] An optional module is registered by the composition root when the game selected it (its reflection types, Luau bindings and systems); a module never registers itself through static initialisation (Z2-D-024).

### Service lookup

- [Z2-N-CONTRACT-149] Runtime service lookup is forbidden: no registry, locator or singleton returns a service object when queried by name, string, type or ID.
- [Z2-N-CONTRACT-150] Querying the reflection registry for type metadata by `Zenith_TypeID` is data lookup, not service lookup, and is allowed.

### Writable-data symbol scan

<!-- elaboration -->
- [Z2-N-CONTRACT-151] The `ForbiddenCheck` gate step's writable-data symbol scan (rule F16) runs the steps listed below over the `win-debug` build and fails on any in-scope writable data symbol not listed in `Zenith2/Tools/Gates/writable_data_allowlist.json`.

<!-- elaboration -->
| Step | Action |
|---|---|
| 1. Scope | From the CMake File API codemodel of the `win-debug` build, take every compile-group source under `Zenith2/Engine/**` or `Zenith2/Player/**` that is not under a `Tests/` directory and not under a third-party source root listed in `Zenith2/ThirdParty/manifest.json`, and map it to its object file through the `output` field of the build's `compile_commands.json` (the `win-debug` preset sets `CMAKE_EXPORT_COMPILE_COMMANDS=ON`); when the objects are absent the step first builds `win-debug` itself |
| 2. Dump | Run the pinned LLVM `llvm-readobj --section-headers --symbols <object>` on each object and parse every symbol record's `Name`, `Section`, `StorageClass` and `Value` |
| 3. Classify | A symbol is writable data when its section name is `.data`, `.bss` or `.tls`, or starts with `.data$`, `.bss$` or `.tls$`, its storage class is `External` or `Static`, and it is not a section symbol; an `External` undefined symbol with a non-zero `Value` (a common symbol) is writable data too |
| 4. Compiler-generated symbols | Thread-safe static guards (`?$TSS…`), dynamic-initialiser records (any symbol in a `.CRT$XC*` section) and TLS index symbols (`_tls_…`) are attributed to their owner, found by demangling with the pinned `llvm-undname`; an owner whose own symbol sits in a read-only section (`.rdata` or `.rdata$…`) is a constant, so its record yields an informational `WDS-GENERATED-CONSTANT` and does not fail the step (in practice a `.CRT$XC*` record for a `.rdata` owner is the only such case); any other owner is judged as in step 5 |
| 5. Judge | Each in-scope writable symbol (or owner) is matched against the allowlist by demangled name and source path; a match passes when its entry is complete ([Z2-N-CONTRACT-153]) and its `source` file holds the entry's `// ZENITH-EXEMPTION <symbol>: <decision ID>` line ([Z2-N-CONTRACT-578]); anything else is a violation |
| 6. Report | Write `Zenith2/out/<checkout-hash>/gates/ForbiddenCheck_WritableData.json` as `{ "source", "object", "symbol", "demangled", "section", "verdict" }` records, print one line per violation in the form `ForbiddenCheck: F16: <source>: writable data symbol <demangled> in <section> is not in writable_data_allowlist.json` (or, for an allowlisted symbol whose entry is incomplete or whose declaration comment block is missing, `ForbiddenCheck: F16: <source>: allowlisted symbol <demangled> lacks its complete entry or its ZENITH-EXEMPTION declaration comment`), and exit 1 when any violation exists |

- [Z2-N-CONTRACT-153] `writable_data_allowlist.json` is one JSON object `{ "comment": "<text>", "entries": [ … ] }`. JSON carries no comments, so its `comment` members are the file's comments: the top-level one states that every entry is an exemption of Z2-D-046 and names `Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152]`. Each entry has the form `{ "symbol": "<demangled name>", "source": "<repo-relative path>", "decision": "Z2-D-NNN", "extensions": [ "<YYYY-MM-DD> <ruling>", … ], "comment": "<text>" }`: `extensions` lists every dated extension of the decision that the variable relies on (empty when the decision's own text covers it), and `comment` states in one sentence why the variable exists, then names the decision ID, each extension and the signpost `Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152]`. An entry without a decision ID, without an `extensions` array, or whose `comment` omits the decision ID, an extension or the signpost fails the step.
- [Z2-N-CONTRACT-154] Namespace-scope and static constants are `constexpr` or `constinit const`, so no Zenith2 runtime object needs dynamic initialisation; a dynamic-initialiser record for a non-constant owner is itself a violation.

### Writable-data allowlist entries

<!-- elaboration -->
- [Z2-N-CONTRACT-152] The allowlist's entries are the five variables of the table below, each citing Z2-D-046 with the dated extensions the table names (Zenith2/Docs/DecisionLog.md → "Z2-D-046 No mutable global state except the root allocator and profiler hooks installed in main; no runtime service lookup"); this table is the one list of the variables that rely on the exemption, and every document sentence that relies on the exemption links it by this heading and ID. The anchor is written only by `Zenith_CrashHandler::Create` (CAS null → the handler), by its destructor (CAS the handler → null, retried every 1 ms while a claim holds it), by the crash path's one claim (CAS the handler → a reserved capturing value, after which the process terminates) and, when the claimed handler is not armed, by the same trampoline's unclaim (capturing value → the handler), and is read only by `Create` and the OS-callback trampolines; a trampoline owns the handler through its own CAS before it dereferences it, because a flag inside the object could be read after a concurrent destructor freed it (`Zenith2/Docs/Design/DEVT/Crash.md` → "Crash handler anchor").

<!-- elaboration -->
| Variable | Source directory | Decision and dated extensions | Why it exists |
|---|---|---|---|
| `g_pxRootHeap` | `Zenith2/Engine/ZenithCore/Internal/Memory/` | Z2-D-046 (its own text: the root allocator) | the root heap `main` installs ([Z2-N-CONTRACT-142]) |
| `g_xProfilerHooks` | `Zenith2/Engine/ZenithCore/Internal/Profiling/` | Z2-D-046 (its own text: the profiler hooks); its log-record members, extended 2026-10-16 by ruling Q11 (a), add no symbol | the installed hook table ([Z2-N-CONTRACT-143], [Z2-N-CONTRACT-157]) |
| `g_xDebugThreadState` (`thread_local`) | `Zenith2/Engine/ZenithCore/Internal/Profiling/` | Z2-D-046, extended 2026-10-16 by ruling Q11 (a) | the per-thread state the plan §3.8 lock-order checker needs, with the job-worker, OOM-handler and failure flags, the no-allocation depth, the hook depth and re-entry count that stop a profiler hook from recursing, and the log router's nested-record flag and count ([Z2-N-CONTRACT-144], [Z2-N-CONTRACT-322], [Z2-N-CONTRACT-590]; `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state") |
| `g_xProfilerThreadBuffers` (`thread_local`) | `Zenith2/Engine/ZenithProfiling/Internal/` | Z2-D-046 (the hook implementations' buffers); the debug-draw recorder's per-thread ring, extended 2026-10-16 by ruling Q79 (a) | the per-thread buffers of the hook implementations and of the debug-draw recorder ([Z2-N-CONTRACT-144]; `Zenith2/Docs/Design/DEVT/Profiler.md` → "Thread slots and the thread-local variable") |
| `g_pxCrashHandlerAnchor` (`std::atomic<Zenith_CrashHandler*>`) | `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/` | Z2-D-046, extended 2026-10-09 by ruling Q88 (a) (orchestrator ruling, human may override) | the only state OS crash callbacks without user data can reach (`Zenith2/Docs/Design/DEVT/Crash.md` → "Crash handler anchor") |

- [Z2-N-CONTRACT-578] The definition of every allowlisted variable is preceded directly, in its entry's `source` file, by a declaration comment block of four `//` lines, and its `extern` declaration in an `Internal/` header, where one exists, carries the same block: (1) `// ZENITH-EXEMPTION <symbol>: <decision ID>`, continued on the same line by `, extended <YYYY-MM-DD> by ruling <ruling>` for each of the entry's extensions; (2) `// Allowlist: Zenith2/Tools/Gates/writable_data_allowlist.json, entry "<symbol>"`; (3) `// Contract: Zenith2/Docs/Contracts.md → "Writable-data allowlist entries" [Z2-N-CONTRACT-152]`; (4) `// Why: <the reason the entry's comment states>`. Step 5 of the scan checks line (1) in the `source` file and review checks the rest; `Zenith2/Docs/WorkerGuide.md` → "Public headers Internal and forbidden constructs" shows the block.

## Layering DAG

### Declared dependencies

- [Z2-N-CONTRACT-171] Each library declares its direct dependencies in its `zenith_add_library(<Lib> DEPENDS …)` call, and `zenith_add_library()` writes them to `Zenith2/out/<checkout-hash>/declared_deps.json`, because the CMake File API reports transitive static-library dependencies.
- [Z2-N-CONTRACT-172] The declared graph is a DAG consistent with `Zenith2/Docs/Architecture.md → "Library dependency graph"`; Flux never depends on Scene, and Runtime extracts a render world for Flux.

### What libgraph checks

The `Libgraph` gate step runs libgraph with its CMake File API front end over the `win-debug` build (built with `/Gy-`, Z2-D-028) and fails on each row below.

| Finding | Evidence libgraph uses |
|---|---|
| used-but-undeclared edge | `#include` resolution and link-symbol use against `declared_deps.json` |
| cycle | the declared graph and the real include and symbol graph |
| `Internal/` encapsulation break | an include of `<Lib>/Internal/**` from outside `<Lib>` and `<Lib>_Tests` |
| leakage | a third-party or `Internal/` header reachable from a public header |
| shader compiler in a runtime target | a Slang or DXC symbol in a runtime target's link surface |
| inbound-surface growth | the surface delta against `Zenith2/Tools/Gates/libgraph_baseline.json`, unless covered by an API review record |
| third-party header outside its owner's `Internal/` | an include of a manifest entry's header from any directory other than the `Internal/` of a library the manifest names as its owner (the `owner` field may list several sibling backends of one API library for one header-only entry) |
| Shipping link closure | a library outside the Shipping link closure (`ZenithScriptCompiler`, `ZenithPathTracer`, tooling libraries) in a shipping Player's link graph |
| development services in Shipping | a development-service symbol (remote tools, the Luau debugger server, the on-screen profiler's remote endpoint) in the Shipping Player's link surface |

- [Z2-N-CONTRACT-173] Generated code is attributed to the library whose build generates it, and its includes and symbols count as that library's edges.
- [Z2-N-CONTRACT-174] The layering rules in the table above are proved by the `Libgraph` gate step and are hard failures (Z2-D-007, Z2-D-042).

### Callback direction

- [Z2-N-CONTRACT-175] A callback registration points downward: a lower library never stores a function pointer, interface pointer or callback into a higher library's code except through an interface that the lower library itself declares.
- [Z2-N-CONTRACT-176] An upward notification goes through that lower-declared interface, implemented by the higher library and passed down by the composition root; review confirms the direction of every new registration.

### Inbound surface baseline and swap contracts

- [Z2-N-CONTRACT-177] Libgraph writes a swap-contract report per backend family (ECS, RHI, physics, compression, online services) listing the symbols consumers use; the report is the evidence that each backend is swappable.
- [Z2-N-CONTRACT-179] The Shipping link closure that the libgraph rows check is the one `Zenith2/Docs/Architecture.md → "Build system and presets"` defines; `ZenithScriptCompiler` and `ZenithPathTracer` are outside it in every configuration.
- [Z2-N-CONTRACT-178] Updating `Zenith2/Tools/Gates/libgraph_baseline.json` is protected work done by a PROC `needs-human` ticket or by the API review flow, never as an incidental edit in an ordinary ticket.

## Ownership and allocation

<!-- informative -->
Ownership is scoped back to the smallest model that removes Zenith's failures (Z2-D-040): one owning pointer type with no per-allocation header, generational handles for every shared reference, and plain references for borrowing. Sanitizers, not a debug-only smart pointer, catch use-after-free of heap objects; handles catch it in every configuration for shared objects. `Zenith_Ptr` does not exist. STL smart pointers are a standing human ruling and are not re-justified.
<!-- /informative -->

### Allocation tiers

| Tier | Type | Lifetime | Backing | Thread affinity | Typical use |
|---|---|---|---|---|---|
| General heap | `Zenith_Heap` | per allocation, freed by pointer | the allocator chosen by D-SEL-HEAP (open, Stage 1): system `malloc` or mimalloc; sanitizer builds use the system allocator | `ANY_THREAD` | every `Zenith_Owned<T>`, container storage, arena blocks, pool pages |
| Arena | `Zenith_Arena` | until `Reset()` or `ResetToMarker()` | blocks from the general heap | `OWNER_THREAD` | frame lifetime, graph lifetime (render graph, CPU frame graph), per-invocation script scratch |
| Pool | `Zenith_RawPool`, `Zenith_Pool<T>` | per slot | pages from the general heap | `OWNER_THREAD` | fixed-size objects on hot paths |

- [Z2-N-CONTRACT-191] Every allocation in Zenith2 code comes from one of the three tiers; a new allocator type is custom infrastructure and needs a justification record (see "Custom infrastructure justification record").
- [Z2-N-CONTRACT-192] Sanitizer builds back the general heap with the system allocator, so that ASan and HWASan instrument it natively.

### Core memory headers

<!-- elaboration -->
| Public header (ZenithCore) | Declares |
|---|---|
| `Zenith_Types.h` | aliases of the exact `<cstdint>` and built-in types: `uint8 = std::uint8_t`, `uint16 = std::uint16_t`, `uint32 = std::uint32_t`, `uint64 = std::uint64_t`, `sint8 = std::int8_t`, `sint16 = std::int16_t`, `sint32 = std::int32_t`, `sint64 = std::int64_t`, `uint = unsigned int`, `sint = int`; signed code uses `sint*` ([Z2-N-CONTRACT-588]) |
| `Zenith_Assert.h` | `ZENITH_ASSERT`, `ZENITH_CHECK`, `ZENITH_FATAL`, `ZENITH_UNREACHABLE` (see "Errors and results") |
| `Zenith_Result.h` | `Zenith_Result<T, E>`, `Zenith_Err`, `ZENITH_TRY`, `ZENITH_TRY_ASSIGN` |
| `Zenith_NarrowCast.h` | `Zenith_NarrowCast`, `Zenith_TryNarrowCast` |
| `Zenith_Heap.h` | `Zenith_Heap`, `Zenith_HeapDesc`, `Zenith_HeapStats`, `Zenith_AllocError`, the OOM handler types, `Zenith_InstallRootHeap`, `Zenith_GetRootHeap` |
| `Zenith_Owned.h` | `Zenith_Owned<T>`, `Zenith_New<T>`, `Zenith_TryNew<T>`, `Zenith_Delete` |
| `Zenith_Arena.h` | `Zenith_Arena`, `Zenith_ArenaDesc`, `Zenith_ArenaMarker`, `Zenith_Construct`, `Zenith_TryConstruct`, `Zenith_ConstructArray`, `Zenith_Destroy` |
| `Zenith_Pool.h` | `Zenith_RawPool`, `Zenith_Pool<T>`, `Zenith_PoolDesc` |
| `Zenith_Span.h` | `Zenith_Span<T>`: a non-owning, trivially copyable `{T* m_pxData; uint64 m_ulCount;}` view with bounds-checked `operator[]` (`ZENITH_ASSERT`), `GetData()`, `GetCount()` and `Subspan()`; it never allocates, and its lifetime is the viewed storage's (see `Zenith2/Docs/Contracts/Memory.md → "Non-owning references"`) |

- [Z2-N-CONTRACT-193] The declarations in `Zenith2/Docs/Contracts/Memory.md → "Zenith_Heap"`, `Zenith2/Docs/Contracts/Memory.md → "Zenith_Owned"`, `Zenith2/Docs/Contracts/Memory.md → "Zenith_Arena"` and `Zenith2/Docs/Contracts/Memory.md → "Zenith_Pool"` are the public API of ZenithCore's memory headers; a change to any of them is an API change reviewed per "Review and change management".
- [Z2-N-CONTRACT-254] `Zenith_Owned.h` declares the `Zenith_New` and `Zenith_TryNew` function templates before `class Zenith_Owned`, so the friend declarations inside the class match those templates.

<!-- informative -->
The headline rules, stated in `Zenith2/Docs/Contracts/Memory.md`: `Zenith_Owned<T>` is exactly `{T*}` and move-only [Z2-N-CONTRACT-201]; it is created by `Zenith_New` or `Zenith_TryNew` alone [Z2-N-CONTRACT-202]; `TryAllocate` and `Zenith_TryNew` return a result while `Zenith_New` reaches the OOM handler [Z2-N-CONTRACT-197], [Z2-N-CONTRACT-214]; borrowing is `T&`/`T*` and there is no `Zenith_Ptr` [Z2-N-CONTRACT-219]; bare allocation lives in `ZenithCore/Internal/Memory/` [Z2-N-CONTRACT-238]; graph captures are trivially destructible [Z2-N-CONTRACT-232].
<!-- /informative -->

### Mechanism sections in Contracts/Memory.md

<!-- informative -->
The mechanism specifications of this section live in `Zenith2/Docs/Contracts/Memory.md`, which belongs to this document's normative family (`Z2-N-CONTRACT`) and follows the same grammar. Each of its headings below is citable as `Zenith2/Docs/Contracts/Memory.md → "<heading>"`.
<!-- /informative -->

- [Z2-N-CONTRACT-260] The obligations in `Zenith2/Docs/Contracts/Memory.md` are part of this section's contract, so a cite of `Zenith2/Docs/Contracts.md → "Ownership and allocation"` covers them.

| Heading in `Zenith2/Docs/Contracts/Memory.md` | Content |
|---|---|
| Zenith_Heap | the general heap, its error and OOM types, root-heap installation |
| Zenith_Owned | the owning pointer and its layout |
| Zenith_New Zenith_TryNew and Zenith_Delete | creation and destruction steps |
| Owned conversion to a base | the same-address rule |
| OOM handler | allocation-failure handling |
| Non-owning references | borrowing rules |
| Fallible construction | factories, partial unwinding, fault injection |
| Zenith_Arena | arenas, `Zenith_Construct`/`Zenith_Destroy`, graph captures |
| Zenith_Pool | fixed-size pools and the sanitizer quarantine |
| Bare allocation boundary | where raw allocation may appear |
| Debug leak side table | leak tracking, capture and symbolisation |
| Sanitizer instrumentation | ASan and HWASan integration |
| ASan poisoning rules | rules A1 to A7 |
| HWASan tagging rules | rules H0 to H12 |
| Required intentional-defect tests | the defect and allocation tests |

## Containers and standard library policy

<!-- informative -->
Zenith wrote its own containers without recording why, and a container's invalidation and failure behaviour lived in its author's head. Zenith2 keeps custom infrastructure where a measured record justifies it, lets an established alternative win where it does not, and requires every container to state its behaviour in its contract block (Z2-D-041).
<!-- /informative -->

### Allowed standard headers

The standard utilities that stay acceptable in runtime code, verbatim:

| Group | Headers |
|---|---|
| C compatibility | `<cstdint>`, `<cstddef>`, `<cstring>`, `<cmath>`, `<limits>` |
| Language support | `<type_traits>`, `<concepts>`, `<utility>`, `<new>`, `<bit>`, `<atomic>` |
| Language support (continued) | `<initializer_list>`, `<compare>`, `<source_location>` |

- [Z2-N-CONTRACT-271] Runtime code (public and `Internal/` sources of every runtime library, and the Player) includes no standard header outside the thirteen listed in the table "Allowed standard headers"; `ForbiddenCheck` fails any other standard include in runtime code.
- [Z2-N-CONTRACT-272] No STL type (containers, strings, string views, spans, optional, variant, function, smart pointers, streams, threads, mutexes) appears in a runtime public header.
- [Z2-N-CONTRACT-273] A further standard header becomes acceptable in runtime `Internal/` code only when a container justification record selects the standard facility as the winner; the record's `Z2-D-` ID is then listed beside the header in `Zenith2/Tools/Gates/forbidden_allowlist.json`, and the header still never reaches a public header.
- [Z2-N-CONTRACT-274] `<memory>` smart pointers (`std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`, `std::make_unique`, `std::make_shared`, `std::auto_ptr`) are forbidden in all Zenith2 code, including tests, tooling and benchmarks; this is a standing human ruling and needs no justification record (Z2-D-040).

### STL in tests and tooling

- [Z2-N-CONTRACT-275] Test targets, tooling targets and benchmark targets may use any standard header in their own sources, except the smart pointers above.
- [Z2-N-CONTRACT-276] A tooling target's own source that uses the STL never passes an STL type across a runtime library's API; conversion to Zenith2 types happens in the tool.
- [Z2-N-CONTRACT-277] Test and tooling targets compile with exceptions disabled like every other non-firewall target; the STL is used in its no-exceptions configuration (`_HAS_EXCEPTIONS=0` on MSVC), and every test target is compiled with `DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`, which `zenith_add_library()` defines.

### Custom infrastructure justification record

- [Z2-N-CONTRACT-278] Each custom container, allocator, heap wrapper or threading primitive in Zenith2 has a decision record appended to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN` ID in the diff that adds it, before or together with its first line of code (Z2-D-097; see `Zenith2/Docs/DecisionLog.md → "How this log is written"`). When the record's measured evidence needs a run the adding diff cannot make (a harness on the reference hardware, which a `needs-human` ruling ticket runs under Z2-D-084), the adding diff appends the record provisionally: every field of the template filled, `Status: Provisional`, `Measured evidence: pending — <harness> run recorded by the ruling <ticket>` and `Decision: adopt provisionally`.
- [Z2-N-CONTRACT-292] The `needs-human` ruling ticket that runs the harness completes a provisional record in place, in its own diff: it writes the measured evidence, states whether the acceptance criteria are met, and rewrites the status to `Settled` when they are; when they are not, it keeps the entry as the record of the failed criterion, states `Decision: reject in favour of <alternative>`, and files the maintenance ticket that replaces the primitive. In both outcomes the ruling rewrites the status to `Settled`, because the ruling is in force and its evidence is no longer pending; a rejected record's Decision line names the alternative that replaces it. A record never carries `Status: Settled` while its measured evidence is pending.
- [Z2-N-CONTRACT-279] The record uses the template below; an established alternative (the STL in `Internal/`, a pinned third-party library) wins when the record cannot show a measured benefit that exceeds the maintenance cost.
- [Z2-N-CONTRACT-280] STL smart pointers being forbidden and exceptions being forbidden outside the Luau firewall are recorded human rulings and are never re-justified.

<!-- elaboration -->
```markdown
### Z2-D-NNN Custom <kind>: <Zenith_TypeName>

- Status: Provisional | Settled
- Kind: container | allocator | heap wrapper | threading primitive
- Standard alternative: <the std facility or pinned library it replaces>
- Improvement claimed: <what it does better: API constraint, allocation tier, layout, determinism, compile time>
- Measured evidence: <harness>, study <name> v<version>, run <RunId>, <platform>, <metric> median <value> [<low>, <high>]
    against <standard alternative> <value> [<low>, <high>], <verdict>; report <report path>   (one line per platform;
    Zenith2/Docs/Design/RESEARCH/Formats.md → "Justification record evidence line")
    | pending — <harness> run recorded by the ruling <ticket>   (Status: Provisional only)
- Maintenance cost: <lines of code estimate, tests, sanitizer and platform surface, owner area epic>
- Acceptance criteria: <the measured thresholds the implementation meets, re-run on each dependency upgrade>
- Contract: <path of the public header whose contract block carries the container fields>
- Decision: adopt | adopt provisionally (Status: Provisional only) | reject in favour of <alternative>
- Research / Analysis / Revalidation: the three research fields of Zenith2/Docs/DecisionLog.md → "Entry template",
    carried by a completed record
```

### Container contract fields

- [Z2-N-CONTRACT-281] Every container's contract block has kind `container` and documents invalidation (in `Invalidation`), alignment (`Alignment`), overflow (`Overflow`), allocation failure (`Allocation failure`), complexity (`Complexity`) and thread safety (`Thread affinity`), per "Block grammar".
- [Z2-N-CONTRACT-282] `Invalidation` names, per operation, which iterators, element pointers, element references and spans it invalidates.
- [Z2-N-CONTRACT-283] `Overflow` states the maximum element count and that size arithmetic is overflow-checked, returning `ALLOC_ERROR_SIZE_OVERFLOW` from `Try` operations and failing `ZENITH_CHECK` from the others.
- [Z2-N-CONTRACT-284] `Complexity` gives every operation's time complexity, marking amortised bounds, and names the hash function for hashed containers, chosen by D-SEL-HASH (open, Stage 1).

### Container allocation failure

<!-- elaboration -->
- [Z2-N-CONTRACT-285] Every allocating container operation exists in two forms: `TryX(…)` returning `Zenith_Result<…, Zenith_AllocError>` and leaving the container unchanged on failure (the strong guarantee), and `X(…)` routing failure through the root heap's OOM handler.
- [Z2-N-CONTRACT-286] A container allocates from the tier its contract names (general heap or a caller-supplied `Zenith_Arena&`) and from no other.
- [Z2-N-CONTRACT-287] Simulation-facing containers whose capacity is shared across parallel invocations are checked at the deterministic merge point, never reserved first-come (see `Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"`).

### Oracle and property tests

- [Z2-N-CONTRACT-288] Every container has an oracle test, `Test_<Container>_MatchesOracle`, that applies seeded random operation sequences to the container and to its STL equivalent and asserts identical observable state after every operation: at least 10,000 operations per seed and 16 seeds, the failing seed and operation index printed on failure.
- [Z2-N-CONTRACT-289] Every container has a property test, `Test_<Container>_Properties`, asserting its invariants (size bookkeeping, element alignment, iteration order where the contract defines one, no element constructed or destroyed twice) over generated inputs, with the generator's seed printed on failure.
<!-- elaboration -->
- [Z2-N-CONTRACT-290] Every container has `Test_<Container>_TryFailure_LeavesUnchanged`, which drives each `Try` operation into failure through heap fault injection and asserts the container's observable state is unchanged, and `Test_<Container>_SizeOverflow_IsRejected`.
- [Z2-N-CONTRACT-291] Element types in container tests include a counting type that records constructions, moves and destructions, so the tests prove no leak and no double destruction; the counting type and the property-test generator live in `Zenith2/Engine/ZenithCore/Tests/Support/`.

## Threading and concurrency contract

<!-- informative -->
The job graph never blocks a job on another job (Z2-D-006); waiting is an edge or a continuation. This section states the contracts every primitive, callback registry, operation and queue obeys, and the global lock order. The job graph's scheduling contract itself lives in `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`.
<!-- /informative -->

### Zenith_Multithreading

<!-- elaboration -->
| Public header (ZenithCore) | Declares | Windows implementation | Android and Linux implementation |
|---|---|---|---|
| `Zenith_Multithreading.h` | `Zenith_Thread`, `Zenith_ThreadDesc`, `Zenith_GetCurrentThreadID`, `Zenith_YieldThread`, `Zenith_SetThreadName`, `Zenith_MarkThreadAsJobWorker(bool bRunningJob)`, `Zenith_IsJobWorkerThread()` (reads the flag: a job body is running on the calling thread now) | `CreateThread`, `SetThreadDescription`, `SwitchToThread` | `pthread_create`, `pthread_setname_np`, `sched_yield` |
| `Zenith_LockDesc.h` | `Zenith_LockDesc` (included first by `Zenith_Mutex.h`) | — | — |
| `Zenith_Mutex.h` | `Zenith_Mutex` and `Zenith_MutexNoProfiling` (the aliases `using Zenith_Mutex = Zenith_<Platform>Mutex_T<true>;` and `using Zenith_MutexNoProfiling = Zenith_<Platform>Mutex_T<false>;` at the end of the platform header it includes), `Zenith_ScopedLock` (a template on the mutex type) | `Zenith_WindowsMutex_T<bool bProfiled>`: `SRWLOCK` (exclusive) held as `void* m_pSRWLock` | `Zenith_AndroidMutex_T<bool bProfiled>`, `Zenith_LinuxMutex_T<bool bProfiled>`: `pthread_mutex_t m_xMutex` |
| `Zenith_ConditionVariable.h` | `Zenith_ConditionVariable` and `Zenith_ConditionVariableNoProfiling` (aliased at the end of the platform header) | `Zenith_WindowsConditionVariable_T<bool bProfiled>`: `CONDITION_VARIABLE` held as `void* m_pConditionVariable` | `Zenith_AndroidConditionVariable_T<bool bProfiled>`, `Zenith_LinuxConditionVariable_T<bool bProfiled>`: `pthread_cond_t m_xCondition` |
| `Zenith_Semaphore.h` | `Zenith_Semaphore` and `Zenith_SemaphoreNoProfiling` (aliased at the end of the platform header) | `Zenith_WindowsSemaphore_T<bool bProfiled>`: the `CreateSemaphoreW` `HANDLE` held as `void* m_pHandle` | `Zenith_AndroidSemaphore_T<bool bProfiled>`, `Zenith_LinuxSemaphore_T<bool bProfiled>`: `sem_t m_xSemaphore`, `uint m_uMaxValue` |

- [Z2-N-CONTRACT-311] Threads, mutexes, condition variables and semaphores come from `Zenith_Multithreading` in ZenithCore, implemented per platform under `Internal/Windows/`, `Internal/Android/` and `Internal/Linux/`; the mutex, condition variable and semaphore are one class template per platform, instantiated as a profiled and an unprofiled variant ([Z2-N-CONTRACT-589]), declared in ZenithCore's per-platform threading header and aliased by the public header, as the signposted exception of [Z2-N-CONTRACT-583] states; `<thread>`, `<mutex>`, `<condition_variable>` and `<semaphore>` are not used in runtime code.
- [Z2-N-CONTRACT-589] Every synchronisation primitive exists in two variants that are the two instantiations of one class template per platform on `bool bProfiled`: the profiled `Zenith_Mutex`, `Zenith_ConditionVariable` and `Zenith_Semaphore`, and the unprofiled `Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling` and `Zenith_SemaphoreNoProfiling` (Z2-D-142). The variants are identical except that the unprofiled one never reads the hook table (no `Zenith_GetProfilerHooks`, no `Zenith_Dispatch…` call and no clock read appears in its instantiation) and the profiled one reports to the installed hooks. A profiled `Lock` tries the native lock first and reports the acquisition with a wait of 0, meaning it did not block; on contention it reads `Zenith_GetMonotonicTimeNs()` before and after blocking, only when an acquire hook is installed, and reports the measured wait, at least 1; `TryLock` reports a success with a wait of 0 and calls no hook on failure; `Unlock` reports the release, from a copy of the desc taken before the native unlock. A profiled condition-variable wait reports a release before the native wait and an acquire (wait 0) after it, and the blocking-wait hook reports the time asleep under the paired mutex's `m_szName` (`wait:<mutex name>`); a `Zenith_Semaphore` reports under its optional static-storage name, default `Zenith_Semaphore`. In a Shipping build the profiled lock operations skip the hook lookup altogether, under `if constexpr (bProfiled && !Zenith_IsShippingBuild())`. Lock-level registration, the lock-order checks and the held-lock stack apply to both variants. A condition variable waits only on a mutex of its own variant, and a `static_assert` refuses the mixed pairing. `Zenith_ScopedLock` is a class template on the mutex type. `Zenith_Thread::Join` and `Zenith_SleepUs` call the blocking-wait hook. The earlier rule of `Zenith2/Docs/Design/CORE/Threading.md` that there is no unprofiled variant is withdrawn.
- [Z2-N-CONTRACT-312] `std::atomic` (from `<atomic>`) is the atomic primitive; every atomic operation states its memory order explicitly, and `memory_order_seq_cst` by default is rejected in review unless a comment names the ordering requirement.
- [Z2-N-CONTRACT-313] A `Zenith_Thread` is joined by exactly one owner before its destructor runs (`ZENITH_CHECK` in the destructor); detached threads do not exist.
- [Z2-N-CONTRACT-314] Thread creation returns `Zenith_Result<Zenith_Thread, Zenith_ThreadError>`; failure to create a thread is recoverable at the factory that asked for it.
- [Z2-N-CONTRACT-315] A job body calls no blocking primitive: no condition-variable or semaphore wait, no thread join, no cross-thread callback unregistration, and no acquisition of a lock whose row in the lock ordering table is not short-hold. ZenithJobs calls `Zenith_MarkThreadAsJobWorker(true)` on entry to every job body and `Zenith_MarkThreadAsJobWorker(false)` on its exit, so the flag means "a job body is running on this thread now" and a worker's own idle park between bodies is not a violation; wherever `ZENITH_ASSERTS_ENABLED=1`, each blocking primitive checks the flag with `ZENITH_CHECK` and fails while it is set, and every build calls the installed blocking-wait hook for profiling after the wait returns, with the time it blocked, through `Zenith_DispatchBlockingWait`, from the profiled variants and from `Zenith_Thread::Join` and `Zenith_SleepUs`. The one exception is the development-build debugger stop of [Z2-N-CONTRACT-365].
- [Z2-N-CONTRACT-316] A short-hold lock guards a critical section that performs no I/O, no allocation from a lock-taking allocator path other than the root heap, no callback and no wait, so acquiring one from a job is not a blocking wait; the one lock that is not short-hold and that a job body may acquire is `uLOCK_LEVEL_SCRIPT_DAP` (710), under the exemption of [Z2-N-CONTRACT-365] alone.
<!-- elaboration -->
- [Z2-N-CONTRACT-365] A development-build debugger stop is exempt from [Z2-N-CONTRACT-315]: in a configuration other than `Shipping` and `ShippingTest`, ZenithScript's debugger code may block a thread inside a job body (a Luau debug callback holding a stopped VM until the debugger releases it, or a script node body parking at its world's debugger gate before it starts an invocation), and acquire `uLOCK_LEVEL_SCRIPT_DAP` for it, only inside the bracket that reads `const bool bWasJobBody = Zenith_IsJobWorkerThread();`, calls `Zenith_MarkThreadAsJobWorker(false)`, waits on `_NoProfiling` primitives, reports the wait once after it returns through `Zenith_DispatchBlockingWait("debugger-stop", ulWaitTicks)`, and restores the flag with `Zenith_MarkThreadAsJobWorker(bWasJobBody)` after it is released; `Test_JobSystem_NoBlockingWaitInJobs` excludes waits reported as `"debugger-stop"`, and no other code in Zenith2 uses the bracket (`Zenith2/Docs/Design/SCRIPT/Debugger.md` → "Stop protocol"; `Zenith2/Docs/Architecture/CPUFrameGraph.md` [Z2-N-ARCH-394] carries the same exemption).

### Thread affinity vocabulary

<!-- elaboration -->
| Keyword | Meaning |
|---|---|
| `ANY_THREAD` | every member may be called concurrently from any thread |
| `ANY_THREAD_EXCEPT(<Member>, …)` | as `ANY_THREAD`, except the listed members, which follow the sentence after the keyword |
| `OWNER_THREAD` | one thread at a time; ownership moves only at a graph edge, a sync point or a hand-off the block documents |
| `MAIN_THREAD` | the thread running the SDL platform pump and the world's main Luau VM |
| `RENDER_SUBMIT_THREAD` | the render-submit thread |
| `EXTERNALLY_SYNCHRONISED` | the caller serialises every access; the type has no internal locking |
| `PHASE(<Phase>)` | callable only from frame-graph nodes of the named phase (see `Zenith2/Docs/Architecture.md → "Frame phases and authority"`) |
| `IMMUTABLE` | a value that never changes after construction; safe everywhere; its data members are `const` where possible ([Z2-N-CONTRACT-585]) |

- [Z2-N-CONTRACT-317] A contract block's `Thread affinity` field begins with exactly one keyword from the table "Thread affinity vocabulary"; `ApiContractCheck` validates the keyword.

### Lock ordering table

- [Z2-N-CONTRACT-318] Every `Zenith_Mutex` and `Zenith_MutexNoProfiling` is constructed with a `Zenith_LockDesc{ m_szName, m_uLevel, m_bShortHold, m_bSameLevelByAddress }` whose level is a row of the table below, declared as a `constexpr uint16 uLOCK_LEVEL_<NAME>` in the owning library's `Internal/` header. `m_szName` points to a string with static storage duration, which review confirms because no gate can ([Z2-N-CONTRACT-589] relies on it: a release hook reads the name after the lock's owner may have been destroyed). A lock that ZenithProfiling owns, or that a profiler hook dispatch can reach (levels 920, 930 and 940 among them), is a `Zenith_MutexNoProfiling` ([Z2-N-CONTRACT-591]).
- [Z2-N-CONTRACT-319] A thread holding a lock at level L acquires only locks at levels strictly greater than L; two locks of the same level are held together only when the row sets `m_bSameLevelByAddress`, and then in ascending address order.
- [Z2-N-CONTRACT-320] Every lock has its row in this table, with the fields below, no later than the diff that introduces its `uLOCK_LEVEL_` constant: the owning area's design names the row, and the core-doc batch that applies that design lands it in this table, ahead of the introducing ticket, because an area ticket never amends this document; a ticket whose lock has no row here stays `draft` until the batch lands. A lock with no row is a `ZENITH_CHECK` failure at construction wherever `ZENITH_ASSERTS_ENABLED=1`.
- [Z2-N-CONTRACT-321] Levels at or above 900 are leaf levels: a thread holding one acquires no further Zenith2 lock except a higher leaf level.

<!-- elaboration -->
| Level | Constant | Owner | Guards | Short-hold | Same level by address | Acquired on |
|---|---|---|---|---|---|---|
| 100 | `uLOCK_LEVEL_RUNTIME_LIFECYCLE` | ZenithRuntime | world creation and destruction, shutdown state | no | no | main thread |
| 200 | `uLOCK_LEVEL_ASSET_REGISTRY` | ZenithAssets | slot allocation and the free list (the slot array never grows; it is reserved once at `Create`), the persistent-ID index, the zero-reference retire decision, the catalogue snapshot pointer, kind-loader registration, the missing-reference log set; never held across an I/O call, a kind-loader call, a job submission or a governor call | yes | no | any thread |
| 250 | `uLOCK_LEVEL_STREAMING_REQUESTS` | ZenithAssets | streaming request queue and priorities | yes | no | any thread |
| 300 | `uLOCK_LEVEL_JOBS_SCHEDULE` | ZenithJobs | compiled-schedule swap on recompile | no | no | main thread |
| 350 | `uLOCK_LEVEL_CALLBACK_REGISTRY` | ZenithCore | a `Zenith_CallbackRegistry` entry list | yes | no | any thread |
| 380 | `uLOCK_LEVEL_OPERATION_TABLE` | ZenithCore | an operation table's slot allocation and free list | yes | no | any thread |
| 400 | `uLOCK_LEVEL_RHI_QUEUE_SUBMIT` | ZenithRHI | submission to and presentation on one queue | yes | yes | render-submit thread, the main thread during recovery |
| 410 | `uLOCK_LEVEL_RHI_RESOURCE_TABLE` | ZenithRHI | a resource table's page growth and free list | yes | no | any thread |
| 450 | `uLOCK_LEVEL_RHI_RETIREMENT` | ZenithRHI | the retirement ring, descriptor free and pending lists | yes | no | any thread; the main thread only at render-idle points |
| 460 | `uLOCK_LEVEL_RHI_DESCRIPTOR_WRITE` | ZenithRHI | Vulkan descriptor-set updates | yes | no | any thread |
| 470 | `uLOCK_LEVEL_RHI_PIPELINE_LIBRARY` | ZenithRHI | D3D12 `StorePipeline` | yes | no | pipeline creation jobs |
| 480 | `uLOCK_LEVEL_FLUX_PERSISTENT` | ZenithFlux | persistent-resource creation bookkeeping (the acquire list, the upload handles); never held across a device call | yes | no | asset loader jobs, render-submit thread, the build node (taking the acquire list) |
| 500 | `uLOCK_LEVEL_PHYSICS_HANDLE_TABLES` | ZenithPhysics | the shape and material handle tables' slot allocation and free lists (the service's mutex), and the primitive cache's entries, pins and index (the cache's own mutex) | yes | no | any thread (loader jobs, query nodes) |
| 510 | `uLOCK_LEVEL_PHYSICS_DISPATCH` | ZenithPhysics_PhysX | the PhysX dispatcher's task queue (push and pop only; a task never runs under it) | yes | no | PhysX drain leaves, the step node |
| 600 | `uLOCK_LEVEL_FILESYSTEM_MOUNTS` | ZenithPlatform | the virtual filesystem mount table | yes | no | any thread |
| 610 | `uLOCK_LEVEL_IO_QUEUES` | ZenithPlatform | the five asynchronous I/O priority queues and the per-target in-flight set | yes | no | I/O threads, any requester |
| 620 | `uLOCK_LEVEL_PLATFORM_CROSS_THREAD_QUEUES` | ZenithPlatform | the account-signal, lifecycle hand-over, thermal-status and low-disk queues | yes | no | any thread; drained on the main thread |
| 630 | `uLOCK_LEVEL_DEV_FILE_WATCH_QUEUE` | ZenithPlatform | the development file watcher's change queue | yes | no | file-watcher thread, main thread |
| 640 | `uLOCK_LEVEL_ACCESSIBILITY` | ZenithPlatform | the published accessibility snapshot and its revision, the accessibility action queue, the OS state, preference and speech cells, the Windows speech command queue | yes | no | main thread, UI Automation provider threads, the Android UI thread through the JNI natives, Zenith.Speech |
| 650 | `uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES` | ZenithPackage | the Play delivery's update slot table (the state, result and confirmation cells, the closed flag); a holder allocates nothing, calls nothing and takes no other lock, and the main thread never holds it while calling Java | yes | no | Play's callback threads; the main thread in `Poll` and `Close` |
| 670 | `uLOCK_LEVEL_ONLINE_PLAY_EVENTS` | ZenithOnline_GooglePlay | the Play bridge's event queue and request table; a holder copies and pushes or pops, takes no other lock and allocates nothing | yes | no | the Android UI thread (the JNI natives), the main thread |
| 700 | `uLOCK_LEVEL_SCRIPT_WATCHDOG` | ZenithScript | the script watchdog's VM table and its world-tag to world-index map; a debugger stop never takes it | yes | no | the watchdog thread, VM creation and destruction |
| 710 | `uLOCK_LEVEL_SCRIPT_DAP` | ZenithScript (development builds) | the DAP session, the stop mailbox, each world's debugger gate and stopped count, each VM's counted flag; a holder allocates nothing, frees nothing, takes no other lock and calls no other subsystem | no: the one lock that is not short-hold and that a job body may acquire, only inside the debugger-stop bracket of [Z2-N-CONTRACT-365] | no | the DAP thread; frame threads and the main thread inside the debugger-stop bracket |
| 800 | `uLOCK_LEVEL_UI_FONT_REGISTRY` | ZenithUI | the font registry's face table and stacks (a reader-writer lock); never held while another lock is taken or across a call out of ZenithUI | yes | no | any thread |
| 810 | `uLOCK_LEVEL_UI_GLYPH_CACHE` | ZenithUI | glyph placements, page records and the dynamic-glyph request merge (a reader-writer lock); page creation calls `Flux_PersistentResources` (480) with no UI lock held | yes | no | any thread |
| 820 | `uLOCK_LEVEL_UI_LOCALIZATION` | ZenithUI | the active and pending localization table chains (a reader-writer lock); interning (850) happens before it is taken | yes | no | any thread |
| 850 | `uLOCK_LEVEL_STRING_TABLE` | ZenithCore | the intern map and page publication of `Zenith_StringTable` | yes | no | any thread |
| 900 | `uLOCK_LEVEL_HEAP_OOM` | ZenithCore | OOM handler serialisation | no | no | any thread |
| 920 | `uLOCK_LEVEL_LOG_SINKS` | ZenithCore | log sink list and sink writes (a `Zenith_MutexNoProfiling`) | yes | no | any thread |
| 930 | `uLOCK_LEVEL_LOG_RING_SINK` | ZenithCore | a `Zenith_LogRingSink`'s ring (a `Zenith_MutexNoProfiling`) | yes | no | any thread (under `uLOCK_LEVEL_LOG_SINKS`, from `Write`) |
| 940 | `uLOCK_LEVEL_PROFILER_BUFFERS` | ZenithProfiling | profiler buffer flush (a `Zenith_MutexNoProfiling`) | yes | no | any thread |
| 950 | `uLOCK_LEVEL_CONTENT_DDC_WATCHDOG` | ZenithAuthoring (CONTENT, `Tools/Authoring/Internal/CONTENT/SharedDDC/`) | the file-share DDC store's 256-slot table of in-flight shared-store file calls (thread handle, deadline, cancelled flag); a leaf: the holder calls `CancelSynchronousIo` or reads and writes one slot and nothing else, and never logs, allocates or performs file I/O while holding it | yes | no | any cook thread calling the store's `Get` or `Put`; the store's watchdog thread |
| 980 | `uLOCK_LEVEL_HEAP_ALIGNED_TABLE` | ZenithCore | the Windows system backing's over-aligned side table | yes | no | any thread |
| 990 | `uLOCK_LEVEL_HEAP_LEAK_TABLE` | ZenithCore | the Debug leak side table | yes | no | any thread |

<!-- informative -->
Ordering facts the rows rely on: interning allocates from the root heap under 850, which may enter the OOM handler at 900; a sink's `Write` at 920 takes its ring lock at 930; the heap takes 980 inside the backing call and releases it before it records a leak-table entry at 990, so the two are never held together (`Zenith2/Docs/Design/CORE/Threading.md` → "Lock-order rows added by CORE"). No thread holding 710 calls the platform listener (600 … 630), the watchdog (700) or a heap, and the watchdog's poll never takes 710 (`Zenith2/Docs/Design/SCRIPT.md` → "Lock-order entries"). The rows 400 to 480 are `Zenith2/Docs/Design/RHI.md` → "Lock-order rows added by RHI"; 450 is also acquired on the main thread at render-idle points, and the imported-state table holds no mutex.
<!-- /informative -->

### Debug lock-order checker

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithCore/Zenith_LockDesc.h (included first by Zenith_Mutex.h and by each platform threading header)
struct Zenith_LockDesc
{
	const char* m_szName = nullptr;   // a string with static storage duration (a literal): the profiler keys its per-thread table by this pointer and a hook reads it after the owner may be gone
	uint16 m_uLevel = 0;
	bool m_bShortHold = false;
	bool m_bSameLevelByAddress = false;
};

// The lock-event hook types live in Zenith_ProfilerHooks.h (see "No mutable global state").
```

- [Z2-N-CONTRACT-322] Wherever `ZENITH_ASSERTS_ENABLED=1`, `Lock` of either mutex variant first calls ZenithCore's lock-order checker (`ZenithCore/Internal/Profiling/Zenith_LockOrderChecker.cpp`), which takes the mutex's `Zenith_LockDesc` and address and keeps a per-thread stack of held `(level, address, name)` entries in the Debug thread state (the allowlisted `g_xDebugThreadState` of "Writable-data allowlist entries", [Z2-N-CONTRACT-152]), a fixed `thread_local` array of `uLOCK_ORDER_MAX_HELD = 16` entries whose overflow is a `ZENITH_CHECK` failure; the lock is then acquired, and a profiled `Zenith_Mutex` reports the acquisition through `Zenith_DispatchLockAcquire` ([Z2-N-CONTRACT-590]). `Unlock` pops the entry and, on a profiled mutex, copies the desc into a local before the native unlock and reports the release through `Zenith_DispatchLockRelease` with a reference to that copy (the address is identity only, because the mutex's owner may be destroyed once the lock is released). The checker needs no installed hook and calls none, so it runs in every Debug test executable.
- [Z2-N-CONTRACT-323] On an acquisition that violates the table's ordering the checker terminates through `ZENITH_FATAL(FATAL_CODE_LOCK_ORDER, …)` naming both locks, both levels and the held stack, before the acquisition can deadlock.
- [Z2-N-CONTRACT-324] The checker is tested by `Test_LockOrder_Inversion_IsReported` and `Test_LockOrder_SameLevelDescendingAddress_IsReported` (death tests) and `Test_LockOrder_AscendingLevels_Passes`, in ZenithCore's tests.

### Callback lifetime

<!-- elaboration -->
```cpp
class Zenith_CallbackRegistryBase;   // the type-erased entry table a token addresses; each registry derives from it
template<typename TSignature> class Zenith_CallbackRegistry;   // TSignature: void(TArgs...)

template<typename... TArgs>
class Zenith_CallbackRegistry<void(TArgs...)>
{
public:
	using PfnCallback = void (*)(void* pUserData, TArgs... xArgs);
	Zenith_Result<Zenith_CallbackToken, Zenith_AllocError> Register(PfnCallback pfnCallback, void* pUserData);
	void Unregister(Zenith_CallbackToken& xToken);   // waits for an in-flight call, except self-unregistration
	void Dispatch(TArgs... xArgs);                    // one dispatching thread at a time
	void DeferDestroy(void* pObject, void (*pfnDestroy)(void* pObject));   // runs after Dispatch returns
};

class Zenith_CallbackToken
{
public:
	constexpr Zenith_CallbackToken() = default;                      // null
	Zenith_CallbackToken(const Zenith_CallbackToken&) = delete;
	Zenith_CallbackToken& operator=(const Zenith_CallbackToken&) = delete;
	Zenith_CallbackToken(Zenith_CallbackToken&& xOther);             // xOther becomes null
	Zenith_CallbackToken& operator=(Zenith_CallbackToken&& xOther);  // unregisters the current entry first
	~Zenith_CallbackToken();                                          // unregisters
	bool IsNull() const;
private:
	Zenith_CallbackRegistryBase* m_pxRegistry = nullptr;
	uint32 m_uEntry = 0;
	uint32 m_uGeneration = 0;
};
```

- [Z2-N-CONTRACT-325] A callback is unregistered before its owner is destroyed; `Zenith_CallbackToken`'s destructor unregisters, so a token held as a member of the owner satisfies this rule.
- [Z2-N-CONTRACT-326] Unregistration from a thread other than the dispatching thread marks the entry removed, so it receives no further calls, and then waits until an in-flight invocation of that entry has returned. The unregistering thread releases the registry's `uLOCK_LEVEL_CALLBACK_REGISTRY` lock first, then spins on the entry's in-flight flag with `Zenith_YieldThread` until it clears, because that lock is short-hold and is never held across a wait.
- [Z2-N-CONTRACT-327] A callback that unregisters itself from inside its own invocation marks itself for removal after it returns and does not wait, because waiting would deadlock.
- [Z2-N-CONTRACT-328] Unregistering a different entry from inside a dispatch on the dispatching thread marks that entry removed and does not wait, because it is not in flight.
- [Z2-N-CONTRACT-329] Callbacks are function pointers with a user-data pointer; `std::function` and capturing lambdas stored beyond a call do not exist in runtime code.
- [Z2-N-CONTRACT-330] Callback registrations point downward in the layering DAG (see "Callback direction").

### Deferred destruction during dispatch

- [Z2-N-CONTRACT-331] Destruction requested while a dispatch loop runs is deferred until that loop exits: entries marked removed are skipped for the rest of the loop and freed after it, and an object whose destruction a callback requests through the registry's `DeferDestroy` is destroyed after the loop exits.
- [Z2-N-CONTRACT-332] The dispatcher reads no entry field and no user data of an entry after that entry's callback has returned, so an owner may free its own state inside its callback after self-unregistering.
- [Z2-N-CONTRACT-333] `Test_CallbackRegistry_SelfUnregister_DoesNotWait`, `Test_CallbackRegistry_UnregisterOtherDuringDispatch_SkipsIt`, `Test_CallbackRegistry_CrossThreadUnregister_WaitsForInFlight` and `Test_CallbackRegistry_DestroyDuringDispatch_IsDeferred` are required ZenithCore tests, the cross-thread test also running under `linux-tsan`.

### Cancellation completion

- [Z2-N-CONTRACT-334] `Zenith_OperationHandle::RequestCancel()` is idempotent and never blocks; it sets a flag the operation's executor observes at its cancellation points.
- [Z2-N-CONTRACT-335] A cancelled operation releases everything it holds (partial buffers, device resources, entity reservations, file handles) on its own executor thread, before it reports `OPERATION_STATUS_CANCELLED`.
- [Z2-N-CONTRACT-336] Once an operation reports complete, its requester owns only the handle and, for `OPERATION_STATUS_SUCCEEDED`, the results; nothing else of the operation remains allocated.
- [Z2-N-CONTRACT-337] An operation that completes successfully before it observes a cancellation reports `OPERATION_STATUS_SUCCEEDED`, and the requester takes or releases its results as for any success.
- [Z2-N-CONTRACT-338] Each operation kind documents its cancellation points in its contract block; streaming, for example, names I/O, decompression, GPU upload and world activation (see `Zenith2/Docs/Architecture.md → "Lifecycle commands"` and the STREAM design).
- [Z2-N-CONTRACT-339] Each operation kind has a stress test performing at least 1,000 issue-then-cancel cycles at randomised stages, asserting no entities, references or allocations remain afterwards.

### Partial initialisation failure

- [Z2-N-CONTRACT-340] Each factory unwinds exactly what it has already constructed when a subsequent step fails, following `Zenith2/Docs/Contracts/Memory.md → "Fallible construction"`.
- [Z2-N-CONTRACT-341] Every construction step of every factory has a failure-injection test that fails that step alone and asserts the factory's error, zero leaks from the test heap, no thread left running and no registration left behind; the test names are `Test_<Type>_Create_FailsAt<Step>_Unwinds`.

### Bounded queues

<!-- elaboration -->
```cpp
enum Zenith_QueueFullPolicy : uint8
{
	QUEUE_FULL_POLICY_REJECT,              // TryPush returns QUEUE_ERROR_FULL; the producer handles it
	QUEUE_FULL_POLICY_GROW_THEN_REJECT,    // doubles up to m_uHardCapacity, then REJECT
	QUEUE_FULL_POLICY_BLOCK_PRODUCER,      // the producer waits for space
	QUEUE_FULL_POLICY_GROW_THEN_BLOCK,     // doubles up to m_uHardCapacity, then BLOCK_PRODUCER
	QUEUE_FULL_POLICY_DROP_OLDEST,         // overwrite the oldest element (telemetry and log rings)
	QUEUE_FULL_POLICY_DROP_NEWEST,         // discard the pushed element (sampling streams)
	QUEUE_FULL_POLICY_FATAL,               // a full queue is a defect: ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)
};

struct Zenith_BoundedQueueDesc
{
	const char* m_szName = nullptr;
	uint32 m_uInitialCapacity = 0;
	uint32 m_uHardCapacity = 0;           // equals m_uInitialCapacity for the non-GROW policies
	Zenith_QueueFullPolicy m_eFullPolicy = QUEUE_FULL_POLICY_REJECT;
};
```

- [Z2-N-CONTRACT-342] Every queue, ring and channel in Zenith2 is bounded, with its capacity taken from a `Zenith2/Docs/Budgets.md` row or derived in its contract block from bounded inputs; an unbounded queue does not exist.
- [Z2-N-CONTRACT-343] Every queue declares one `QUEUE_FULL_POLICY_*` value, and the policy appears in the owning type's contract block.
- [Z2-N-CONTRACT-364] A per-worker job deque uses `QUEUE_FULL_POLICY_REJECT`, and the job system runs a rejected job inline on the pushing worker, so a full deque never blocks and never drops work (see `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`).
- [Z2-N-CONTRACT-344] `QUEUE_FULL_POLICY_BLOCK_PRODUCER` and `QUEUE_FULL_POLICY_GROW_THEN_BLOCK` are used only by queues whose producers are dedicated threads (I/O, decode, upload staging) or the render-submit thread; a job-worker producer never blocks.
- [Z2-N-CONTRACT-345] Every queue counts its full events, drops and high-water mark as counters named `<queue name>.full`, `<queue name>.dropped` and `<queue name>.high_water`, shown by the on-screen profiler. `.full` counts a push the queue refuses back to its caller (a `REJECT` push, or a `GROW_THEN_REJECT` push at the hard capacity); `.dropped` counts an element the queue loses; `.high_water` is the largest element count reached. A `DROP_OLDEST` or `DROP_NEWEST` overflow is a drop and never a full event: it increments `.dropped` alone (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer", policy table). A refused push whose element the queue's owner then discards, because it is queued nowhere else, counts in both (`Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow"), and a `FATAL` queue counts neither, because its full event ends the process.
- [Z2-N-CONTRACT-346] A queue whose capacity is shared by parallel simulation invocations accepts or rejects at the deterministic merge point in merge order, so a rejection is an entity-level rollback, identical on every run (Z2-D-019).
- [Z2-N-CONTRACT-347] The upload ring uses `QUEUE_FULL_POLICY_GROW_THEN_BLOCK` up to the upload-ring hard cap row, and a single request larger than the cap is split or refused with an error (see `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`).

### Shutdown ownership order

<!-- elaboration -->
| Step | Owner | Action | Threads joined |
|---|---|---|---|
| 1 | Player main | stops issuing frames | none |
| 2 | ZenithRuntime | drains the in-flight pipelined frame: simulation N+1 finishes, rendering N submits | none |
| 3 | ZenithRuntime | shuts down each world per its lifecycle: drain the tick, flush commands, tear down in reverse phase order | none |
| 4 | ZenithRuntime | cancels every outstanding operation and runs jobs until every operation reports complete; the asset registry's cancellation entry is `Zenith_AssetRegistry::BeginShutdown` (refuses new requests with `ASSET_ERROR_SHUTTING_DOWN` and requests cancellation of every load and read), after which the job system's drain runs the asset service until every operation has completed; ZenithRuntime begins the save system's shutdown (`Zenith_RuntimeSave_BeginShutdown`, `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Shutdown sequence"): new requests are refused, queued saves are cancelled except the exit autosave, and the running write is drained for at most `save.shutdownDrainMs`, then cancelled with rollback; an exit autosave requested by the device-loss clean exit (`Zenith_RuntimeSave_RequestAutosaveForExit`, Architecture/CompositionRoot.md → "The frame loop") is waited for at most 6 s from its request, then cancelled with rollback and waited for at most 1 s more; the package service's shutdown (`Zenith_RuntimePackage_BeginShutdown`, `Zenith2/Docs/Design/PACK/Algorithms.md` → "Shutdown drain"): its `BeginShutdown` cancels every verification and unmounts every delivered pack and Windows DLC (`RemoveSource`), every persisted intent kept; then, since no prologue runs, `PumpShutdown` with 1 ms sleeps until every verification stopped and every unmounted source was released, waiting for the releases at most `package.shutdownDrainMs` (1,000 ms; no file is deleted at shutdown); then the delivery state file is written with `WriteAtomic` and waited for, only when it is dirty and never on RHI's device-loss clean exit; then PACK's entitlement forwarder drops its provider, before step 6 destroys any module; the replay writer is closed | `Zenith.ReplayWrite`, by the replay writer's `Close` |
| 5 | ZenithRuntime | waits for GPU idle on every queue, retires every deferred deletion | none |
| 6 | ZenithRuntime | destroys worlds, then the CPU frame executor, then the schedule cache (after the executor, which may hold pointers to cached schedules), then module subsystems in reverse registration order (audio, a module, among them), then the package service (`Zenith_RuntimePackage_Destroy`: the service, whose Play delivery's destructor stops the Java bridge's native callbacks before closing its update slot table, then the forwarding entitlement query; no thread joined), then the script runtime, the script watchdog and the binding table, then the Flux renderer (its retirement listener cleared first; swapchains, GPU scene, upload ring, pipeline-cache final write), a render-idle point after the modules and before the job system | module-owned threads, by their owning module: the Audio module joins `Zenith.AudioCtl`, `Zenith.AudioDec` and `Zenith.AudioNul`, and stops the backend's device thread through the backend (it is not joined, since the backend created it; `Zenith2/Docs/Design/AUDIO.md` → "Threads AUDIO owns"); the Cinematics module, in video-enabled configurations, joins `Zenith.VideoDec` after `Flux_Renderer::DisableVideo` has stopped calls into its frame source and its players are closed, then destroys the decoder, before its instance is freed (`Zenith2/Docs/Design/CINE.md` → "Threads CINE owns"); the DAP thread (development builds), by the script runtime; `Zenith.ScriptWatchdog`, by the script watchdog; the executor and the renderer start none |
| 7 | ZenithRuntime | destroys the job system, which joins its workers in reverse dependency order | job workers, render-submit thread |
| 7b | ZenithRuntime | destroys the save system (`Zenith_RuntimeSave_Destroy`), so every SAVE I/O has completed before `Zenith_AsyncIO` is destroyed in step 8 | `Zenith.SaveWriter`, by the save system's destructor |
| 8 | ZenithRuntime | flushes the user layers while `Zenith_AsyncIO` is still alive (`Zenith_Config::FlushUserLayer`, then `Zenith_InputSystem::FlushUserBindings`, each waiting for its pending atomic write), then detaches every DEVT output with `Zenith_DevTools::DetachOutputs` (the log file sink, the telemetry, the profiler's output and the development-file writer, waiting for every write they submitted; `Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown"), then destroys the platform services, that is the owners of the platform's I/O threads (`Zenith_AsyncIO`) | I/O threads |
| 9 | ZenithRuntime | destroys the RHI device (reports live device handles); with a device-interop provider (Stage 7), calls its `OnDeviceDestroying` before destroying the device and `OnDeviceDestroyed` after it, then destroys the provider | none |
| 10 | ZenithRuntime | destroys the asset registry (reports referenced slots), then the compression codecs | none |
| 10b | ZenithRuntime | destroys the type and world registries, the memory governor, the log sinks and the crash reporter, then `Zenith_Config`, then `Zenith_VirtualFileSystem`, then PACK's instance guard is released and its install file system destroyed (`Zenith_RuntimePackage_ReleaseInstanceGuard`; no thread), so no handle on the install tree is open when the guard lets another instance install, then `Zenith_Platform`, which is destroyed after the RHI device of step 9 because the swapchain surface belongs to a platform window | `Zenith.Speech` (Windows, the accessibility OS backend; joined by `Zenith_AccessibilityService`'s destructor, first in the platform's destructor), `Zenith.Watchdog` (the `--timeout-seconds` watchdog, joined unfired by the platform's destructor), SDL3's internal threads (`SDL_Quit`) |
| 11 | Player main | destroys ZenithRuntime, then the log router (which drains its ring and joins its drain thread), then uninstalls the profiler hooks | profiler flush thread, log drain thread (`Zenith.LogDrain`) |
| 12 | Player main | destroys the root heap after uninstalling it (Debug leak report) | none |

- [Z2-N-CONTRACT-348] Shutdown follows the table "Shutdown ownership order"; each thread is joined by exactly the owner named in its row, and a subsystem that starts a thread adds or extends a row in the same diff.
- [Z2-N-CONTRACT-349] A subsystem's destructor joins the threads it started and runs no engine code on other subsystems that the order has already destroyed.

### Reentrancy notation

<!-- elaboration -->
| Keyword | Meaning |
|---|---|
| `NOT_APPLICABLE` | the type invokes no callback |
| `NONE` | no member of the instance may be called from inside a callback it is invoking, on the invoking thread; a Debug reentrancy guard reports a violation |
| `ALLOWED(<Member>, …)` | the listed members may be called from inside its callbacks; every other member follows `NONE` |
| `FULL` | every member may be called from inside its callbacks |

- [Z2-N-CONTRACT-350] Every interface and every type that invokes a callback states its rule in its contract block's `Reentrancy` field, beginning with one keyword from the table "Reentrancy notation"; `ApiContractCheck` validates the keyword.
- [Z2-N-CONTRACT-351] A type declaring `NONE` or `ALLOWED(…)` holds a `Zenith_ReentrancyGuard` member (present in every configuration, checked wherever `ZENITH_ASSERTS_ENABLED=1`) set for the duration of each callback invocation.
- [Z2-N-CONTRACT-359] `Zenith_ReentrancyGuard` (ZenithCore `Zenith_ReentrancyGuard.h`) is an 8-byte value holding the invoking thread's ID and a depth count while a callback runs, with `void Enter()`, `void Exit()` and `void CheckNotInCallback() const` (a `ZENITH_ASSERT` that the calling thread is not inside a callback of the same instance); its layout is identical in every configuration.
- [Z2-N-CONTRACT-352] Review confirms that the declared reentrancy rule matches the implementation; the rule's tests are named `Test_<Type>_Reentrancy_<Case>`.

### Determinism obligations on concurrent code

- [Z2-N-CONTRACT-353] No simulation result depends on worker count, chunk layout, scheduling or completion order, including ticks where scripts fail or exhaust budgets, for the same backend and build (Z2-D-019); the mechanisms are specified in `Zenith2/Docs/Architecture.md → "Determinism guarantee and mechanisms"`.
- [Z2-N-CONTRACT-354] Command buffers from parallel invocations are merged in (system order, logical invocation ID, sequence) order, spawn IDs are assigned at playback in that order, and events are delivered in that order, never in completion order.
- [Z2-N-CONTRACT-355] Parallel reductions use a fixed-order reduction tree whose shape is independent of the worker count.
- [Z2-N-CONTRACT-356] A new concurrent simulation path carries the replay test run with 1, 2, 8 and all workers, with forced chunk compaction and a different chunk capacity, on each backend, asserting bit-identical results per backend.

### Floating-point environment

<!-- elaboration -->
| Target | Register | Fixed state |
|---|---|---|
| MSVC x64 (Windows) | MXCSR through `_mm_getcsr`/`_mm_setcsr` | round to nearest even; flush-to-zero (bit 15) and denormals-are-zero (bit 6) set; every exception masked; no x87 code is generated |
| clang x86-64 (`linux-tsan`, `android-x86_64`) | MXCSR through `_mm_getcsr`/`_mm_setcsr` | as MSVC x64 |
| clang arm64 (`android-arm64`) | FPCR through `mrs`/`msr fpcr` inline assembly in `Internal/Android/` | round to nearest (RMode 00); flush-to-zero (FZ, bit 24) set; default-NaN (DN, bit 25) set; every exception trap disabled |

- [Z2-N-CONTRACT-360] Every thread that runs simulation or job-graph work has the floating-point environment of the table "Floating-point environment". `ZenithJobs` owns it: its public header `Zenith_FloatEnvironment.h` declares `void Zenith_ApplyFloatEnvironment()` and `Zenith_FloatEnvironmentState Zenith_ReadFloatEnvironment()` (a value holding the MXCSR or FPCR bits), implemented under `ZenithJobs/Internal/Windows/`, `Internal/Linux/` and `Internal/Android/`; the job system calls `Zenith_ApplyFloatEnvironment()` at the start of every worker thread and of the render-submit thread, and the composition root calls it on the main thread before the first frame.
- [Z2-N-CONTRACT-361] After applying it, each thread compares `Zenith_ReadFloatEnvironment()` with the fixed state through `ZENITH_ASSERT` (wherever `ZENITH_ASSERTS_ENABLED=1`), and the job system repeats that assert at the start of every frame-graph node, so a third-party call that changes the environment is caught at the next node.
- [Z2-N-CONTRACT-362] No Zenith2 code changes the floating-point environment after start-up; a third-party call known to change it is wrapped in `Internal/` code that restores the fixed state before returning. This is what makes the determinism guarantee independent of which worker ran a computation (see `Zenith2/Docs/Scripting.md → "Determinism rules and randomness"`).
- [Z2-N-CONTRACT-363] ZenithJobs carries `Test_FloatEnvironment_WorkerThreads_MatchMain`, which starts the job system with 8 workers and asserts each reads back the fixed state, and `Test_FloatEnvironment_Denormal_FlushesToZero`, which asserts a denormal product is zero on every worker.

### Concurrency tests

- [Z2-N-CONTRACT-357] Every concurrency primitive, callback registry, queue and job-graph path has a stress test run under `linux-tsan` through the `Sanitizers` gate step of the `Zenith2Memory` category, with thread counts 2, 8 and the host's hardware concurrency.
- [Z2-N-CONTRACT-358] TSan and UBSan reports fail the run; suppressions follow `Zenith2/Docs/Contracts/Memory.md → "Sanitizer instrumentation"`.

## Reflection

- [Z2-N-CONTRACT-371] Zenith2 has one reflection system, ZenithReflect, with explicit registration, stable type and property IDs, and schema versions; no second property or reflection mechanism exists in any library.
- [Z2-N-CONTRACT-372] Registration is explicit: the composition root calls each library's and each selected module's registration function, and data-defined component schemas from `Games/<Name>/Schemas/*.schema.json` are registered at game load (Z2-D-020); nothing registers through static initialisation.
<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ReflectIDs.h
struct Zenith_TypeID
{
	uint64 m_ulHash = 0;
	friend constexpr bool operator==(Zenith_TypeID, Zenith_TypeID) = default;
	friend constexpr std::strong_ordering operator<=>(Zenith_TypeID, Zenith_TypeID) = default;
};

struct Zenith_PropertyID
{
	uint32 m_uID = 0;
	friend constexpr bool operator==(Zenith_PropertyID, Zenith_PropertyID) = default;
	friend constexpr std::strong_ordering operator<=>(Zenith_PropertyID, Zenith_PropertyID) = default;
};
```

- [Z2-N-CONTRACT-373] A `Zenith_TypeID` is the 64-bit hash of the type's registered stable name, never of its C++ type name, with the hash chosen by D-SEL-HASH (open, Stage 1); a registration whose ID collides with an existing type is a load error naming both types.
- [Z2-N-CONTRACT-380] The registered stable name is a dedicated identifier, distinct from any display name: the `"stableName"` member of a `*.schema.json` or the registration string passed by C++ registration; changing it changes the type ID and is therefore an authoring-document migration step.
- [Z2-N-CONTRACT-374] A `Zenith_PropertyID` is a 32-bit ID declared explicitly per property at registration; an ID once used for a property of a type is never reused for a different property of that type, including after the property is removed; both ID types are structs with no implicit conversion to an integer.
- [Z2-N-CONTRACT-375] Each type carries a `uint32` schema version, incremented by any change to its property set or a property's type, and every increment ships a migration step (see "Compatibility policy per kind").
- [Z2-N-CONTRACT-376] Runtime-registered types are indistinguishable from C++-registered types to every consumer: serialization, the inspector, migration, `.d.luau` generation, chunk-system column access and phase-authority declarations.
- [Z2-N-CONTRACT-377] Serialization reads and writes through reflected properties, never through `memcpy` of a C++ object's layout (Z2-D-038).
- [Z2-N-CONTRACT-378] The type registry is immutable outside the hot-reload window: `BeginHotReload()` and `EndHotReload()`, called by the composition root in the frame prologue, bracket the only period in which types register or change, admitting behaviour-state types (`TYPE_KIND_BEHAVIOUR_STATE`) during play and schema types in the editor between play sessions; lookups by `Zenith_TypeID` are `ANY_THREAD` outside the window, and a registration outside it is a `ZENITH_CHECK` failure (see `Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"`).
- [Z2-N-CONTRACT-379] The full reflection model is specified in `Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"`; this section states the contract every library obeys.

## Tests and code hygiene gates

### Naming and layout

| Prefix | Meaning | Example |
|---|---|---|
| `m_` | member | `m_uEntityID` |
| `s_` | static data member; unused in runtime code, where static data members are constants named like constants (see "No mutable global state") | — |
| `g_` | namespace-scope variable; used by the five allowlisted variables of "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]) alone, each with its declaration comment block ([Z2-N-CONTRACT-578]) | `g_xProfilerHooks` |
| `ls_` | function-local static; unused in runtime code (see "No mutable global state") | — |
| `x` | object of class or struct type | `xEntity`, `m_xComponents` |
| `p` | pointer | `pMemory` |
| `px` | pointer to class or struct | `pxHeap` |
| `a` | array | `auIndices` |
| `u` | unsigned integer of 32 bits or fewer: `uint`, `uint8`, `uint16`, `uint32` | `uIndex` |
| `ul` | `uint64` | `ulSize` |
| `i` | signed integer of any width: `sint`, `sint8`, `sint16`, `sint32`, `sint64` | `iLevel`, `iOffset` |
| `b` | bool | `bSuccess` |
| `f` | float | `fDeltaSeconds` |
| `d` | double | `dMin`, `dPI` |
| `c` | `char` | `cValue`; a `char` array takes `ac` (`acLiteral`, `acBuffer`, `Zenith_String::m_acBytes`) |
| `str` | string object | `strPath` |
| `sz` | pointer to a NUL-terminated C string (`const char*`); a `char` array is `ac`, never `sz` | `m_szName` |
| `e` | enum value | `eMode` |
| `pfn` | function pointer | `m_pfnOOMHandler` |

- [Z2-N-CONTRACT-422] Zenith2 code uses Zenith Hungarian (Z2-D-034): types `Zenith_PascalCase`, renderer types `Flux_PascalCase`, functions `PascalCase`, a scope prefix then a type prefix on every variable per the table "Naming and layout", enumerators `SCREAMING_SNAKE_CASE` with a category prefix (`QUEUE_FULL_POLICY_REJECT`), and constants `SCREAMING_SNAKE_CASE` with a type prefix (`uLOCK_LEVEL_HEAP_OOM`, `ulHEAP_MAX_ALIGNMENT`).
- [Z2-N-CONTRACT-423] Interface classes carry no `I` prefix, factories are named `Zenith_Create<Interface noun>_<Backend>`, error enumerations `Zenith_<Subsystem>Error` with `<SUBSYSTEM>_ERROR_<NAME>` enumerators, and tests follow "Test naming".
- [Z2-N-CONTRACT-424] Layout follows the root `Zenith/CLAUDE.md` code style in this subset: opening braces on their own line (short inline accessors in headers may stay on one line), tabs for indentation, `public:` then `protected:` then `private:`, `#pragma once`, and no `using namespace` in headers; `Zenith2/.clang-format` is the binding form of the formatting part.
- [Z2-N-CONTRACT-425] Two root `Zenith/CLAUDE.md` code-style rules do not apply under `Zenith2/`: there is no `Zenith.h` precompiled header that every `.cpp` includes first (see "HeaderSelfContained"), and tools-only code is never wrapped in `#ifdef ZENITH_TOOLS` (tools-only code lives in tooling targets under `Zenith2/Tools/`, and rule F11 forbids such conditionals).
- [Z2-N-CONTRACT-426] Naming is confirmed by review; formatting is proved by the `Format` gate step.
- [Z2-N-CONTRACT-588] Zenith2 code names integers only through `Zenith_Types.h` (`uint8`, `uint16`, `uint32`, `uint64`, `sint8`, `sint16`, `sint32`, `sint64`, and `uint` and `sint` for the platform's default `unsigned int` and `int`), never `u_int*`, `intN_t` or `uintN_t`, except at a third-party API boundary inside `Internal/`; and `final` appears only on a class that inherits from a base, never on a class or struct with no base (Z2-D-141). `ForbiddenCheck` rule F25 refuses `u_int*`, `intN_t` and `uintN_t` outside `Zenith_Types.h` and the `f25Boundary` files of [Z2-N-CONTRACT-417], and `final` on a class with no base clause.

### Compiler warning flags

<!-- elaboration -->
| Compiler | Presets | Flags applied to every Zenith2 target |
|---|---|---|
| MSVC `cl` | `win-debug`, `win-release`, `win-tools`, `win-asan` | `/W4 /WX /permissive- /Zc:__cplusplus /Zc:preprocessor /Zc:inline /utf-8`, plus these off-by-default warnings raised to level 4: `/w44242 /w44254 /w44263 /w44265 /w44287 /w44296 /w44311 /w44545 /w44546 /w44547 /w44549 /w44555 /w44619 /w44640 /w44826 /w44905 /w44906 /w44928`, and `/we4289` |
| clang (NDK r27c) | `android-arm64`, `android-x86_64`, the S24 HWASan build type | `-Wall -Wextra -Werror -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough`, plus the curated conversion subset below |
| clang (pinned LLVM) | `linux-tsan` | the NDK clang flags above |
| curated conversion subset (clang) | all clang presets | `-Wshorten-64-to-32 -Wimplicit-int-conversion -Wimplicit-float-conversion -Wfloat-conversion -Wsign-conversion -Wsign-compare` |

- [Z2-N-CONTRACT-391] Warnings are errors in every Zenith2 target and every preset, with the flag sets of the table "Compiler warning flags" defined once in `Zenith2/CMake/ZenithWarnings.cmake` and applied by `zenith_add_library()`, `zenith_add_game()` and every tool and test target.
- [Z2-N-CONTRACT-392] A warning is silenced in Zenith2 source by fixing the code; `#pragma warning(disable …)` and `#pragma clang diagnostic ignored` appear only in `Internal/` wrappers around third-party headers, each with a comment naming the `Z2-D-` decision that allows it.
- [Z2-N-CONTRACT-393] The pinned clang-tidy configuration `Zenith2/.clang-tidy` runs in each stage acceptance, and its findings block the acceptance.

### Language feature flags

<!-- elaboration -->
| Setting | Every Zenith2 target | Luau and the firewall compile groups `ZenithScript_EH` and `ZenithScriptCompiler_EH` |
|---|---|---|
| C++ standard | C++20 (`/std:c++20`, `-std=c++20`) (Z2-D-009) | C++20 |
| Exceptions (MSVC) | `/EHs-c-` and `_HAS_EXCEPTIONS=0` | `/EHs`, never `/EHsc` |
| Exceptions (clang) | `-fno-exceptions` | `-fexceptions` |
| RTTI | `/GR-`, `-fno-rtti` | `/GR-`, `-fno-rtti` |

<!-- informative -->
`/EHsc` lets MSVC assume that `extern "C"` functions never throw, so it could elide the cleanups around a `lua_error` call; `/EHs` keeps them, which is why the firewall targets use `/EHs`.
<!-- /informative -->

- [Z2-N-CONTRACT-394] Exceptions are enabled per target in Luau and in the firewall compile groups `ZenithScript_EH` (`ZenithScript/Internal`) and `ZenithScriptCompiler_EH` (`ZenithScriptCompiler/Internal`, a tool-side library no Player links) alone, with `/EHs` on MSVC and `-fexceptions` on clang; every other target is compiled with exceptions off, and the `ForbiddenCheck` gate step verifies the flags from the CMake File API compile fragments (Z2-D-017).
- [Z2-N-CONTRACT-395] RTTI is disabled in every Zenith2 target; `dynamic_cast` and `typeid` do not appear in Zenith2 code.

### clang-format

<!-- elaboration -->
| `.clang-format` key (at `Zenith2/.clang-format`) | Value |
|---|---|
| `Language` | `Cpp` |
| `Standard` | `c++20` |
| `UseTab` | `ForIndentation` |
| `IndentWidth` / `TabWidth` | `4` / `4` |
| `BreakBeforeBraces` | `Allman` |
| `AllowShortFunctionsOnASingleLine` | `InlineOnly` |
| `AllowShortIfStatementsOnASingleLine` | `Never` |
| `ColumnLimit` | `140` |
| `PointerAlignment` | `Left` |
| `NamespaceIndentation` | `None` |
| `SortIncludes` | `CaseSensitive` |
| `IncludeBlocks` | `Regroup` |
| `IncludeIsMainRegex`, `IncludeCategories` | as in the block below |

```yaml
IncludeIsMainRegex: '(_Tests|_DefectTests|_Example)?$'
IncludeCategories:
  - Regex: '^"(Internal/)?(Zenith|Flux)[A-Za-z0-9_]*(\.generated)?\.h"$'    # Zenith2 headers
    Priority: 1
  - Regex: '^<(cstdint|cstddef|cstring|cmath|limits|type_traits|concepts|utility|new|bit|atomic|initializer_list|compare|source_location)>$'    # allowed standard headers
    Priority: 2
  - Regex: '^<.*>$'    # third-party headers in Internal/, and test-only standard headers
    Priority: 3
  - Regex: '.*'
    Priority: 4
```

- [Z2-N-CONTRACT-396] Every C and C++ file under `Zenith2/` outside `Zenith2/ThirdParty/` is formatted with the pinned `clang-format` from `Zenith2/ThirdParty/manifest.json` and `Zenith2/.clang-format`; the `Format` gate step runs `clang-format --dry-run --Werror` over tracked and untracked files and fails on any difference.
- [Z2-N-CONTRACT-427] `Zenith2/.clang-format` carries exactly the keys and values of the table "clang-format" and its include block; the own header of a file sorts first through `IncludeIsMainRegex`, which makes rule "a `.cpp` includes its own header first" hold under formatting.
- [Z2-N-CONTRACT-397] `// clang-format off` regions appear only around tables of data whose alignment carries meaning, and each region is closed in the same file.

### Checked narrowing

<!-- elaboration -->
```cpp
template<typename TTo, typename TFrom>
	requires std::is_arithmetic_v<TTo> && std::is_arithmetic_v<TFrom>
constexpr TTo Zenith_NarrowCast(TFrom xValue);
	// ZENITH_ASSERT, in this order, before any cast to TTo: not NaN and within
	// [numeric_limits<TTo>::lowest(), numeric_limits<TTo>::max()] compared in the wider common type;
	// sign preserved; exact round trip (precision). The cast to TTo happens last.

template<typename TTo, typename TFrom>
	requires std::is_arithmetic_v<TTo> && std::is_arithmetic_v<TFrom>
constexpr Zenith_Result<TTo, Zenith_NarrowError> Zenith_TryNarrowCast(TFrom xValue);
	// NARROW_ERROR_OUT_OF_RANGE, NARROW_ERROR_SIGN_CHANGE, NARROW_ERROR_PRECISION_LOSS
```

- [Z2-N-CONTRACT-398] A conversion that can lose value or sign goes through `Zenith_NarrowCast` (engine-internal values, checked by `ZENITH_ASSERT`) or `Zenith_TryNarrowCast` (values derived from untrusted input, checked in every configuration), never through a raw `static_cast` or an implicit conversion that could truncate.
- [Z2-N-CONTRACT-399] Implicit narrowing is caught by the warning flags (`C4242`, `C4244`, `C4267`, and the curated clang conversion subset); an explicit `static_cast` between arithmetic types that narrows is rejected in review.
- [Z2-N-CONTRACT-430] `Zenith_NarrowCast` and `Zenith_TryNarrowCast` check before they cast: first NaN and the range of `TTo` (compared in the wider common type, so no out-of-range floating-to-integer cast ever executes), giving `NARROW_ERROR_OUT_OF_RANGE`; then the sign, giving `NARROW_ERROR_SIGN_CHANGE`; then the exact round trip, giving `NARROW_ERROR_PRECISION_LOSS`; the cast to `TTo` runs only after every check passes, so UBSan's `float-cast-overflow` never fires inside them.
- [Z2-N-CONTRACT-431] ZenithCore carries `Test_NarrowCast_InRange_Passes`, `Test_NarrowCast_OutOfRange_Asserts` (suite `z2-death-assert`), `Test_TryNarrowCast_OutOfRange_ReturnsError`, `Test_TryNarrowCast_FloatToIntOverflow_ReturnsError` (a `double` of `1e20` and NaN into `sint32`, run under `linux-tsan` with UBSan), `Test_TryNarrowCast_SignChange_ReturnsError` and `Test_TryNarrowCast_PrecisionLoss_ReturnsError`.

### nodiscard

- [Z2-N-CONTRACT-400] Withdrawn (Z2-D-141): `Zenith_Result` declared `[[nodiscard]]` as a class; [Z2-N-CONTRACT-586] is the rule.
- [Z2-N-CONTRACT-401] Withdrawn (Z2-D-141): `[[nodiscard]]` on every fallible API that does not return `Zenith_Result` and on pure accessors; [Z2-N-CONTRACT-586] is the rule.
- [Z2-N-CONTRACT-586] `[[nodiscard]]` is not written anywhere in Zenith2 code, `Zenith_Result` included (Z2-D-141), except on a line carrying the `// ZENITH-ALLOW: <reason>` signpost of [Z2-N-CONTRACT-582] where a mechanism requires it; `ForbiddenCheck` rule F25 refuses every other occurrence. `[[noreturn]]` is written only on a function that genuinely never returns (the fatal and unreachable handlers behind `ZENITH_FATAL`, `ZENITH_CHECK` and `ZENITH_UNREACHABLE`, the process-exit path), where warnings-as-errors needs it to accept the code after a call; on any other function it is rejected in review.
- [Z2-N-CONTRACT-402] An intentionally discarded result of a fallible call (a `Zenith_Result`, a `bool TryX`, a pointer that can be null, a count that can be short) is written `static_cast<void>(Expr)` with a comment naming why the failure is irrelevant, and no fallible result is dropped silently; review confirms both, since no attribute makes an ignored result a compile error.

### HeaderSelfContained

<!-- elaboration -->
The `HeaderSelfContained` gate step runs these steps:

1. For every public header `Zenith2/Engine/<Lib>/<Header>.h`, call `New-Zenith2HeaderTU` of the shared gate module, which writes `Zenith2/out/<checkout-hash>/hsc/<Lib>/<Header>.hsc.cpp` containing `#include "<Header>.h"` twice and nothing else.
2. Compile each generated file with the flags of `<Lib>` in `win-debug`, with the include path limited to the public include directories of `<Lib>` and of its declared direct dependencies (excluding every `Internal/` directory), and with warnings as errors.
3. Repeat step 2 with the `android-arm64` clang flags once the Android gate lines are active.
4. Fail on any compile error; the objects are not linked.

- [Z2-N-CONTRACT-403] Every public header passes the `HeaderSelfContained` steps listed above.
- [Z2-N-CONTRACT-404] A `.cpp` file includes its own public header (for an implementation of a public type) or its own private header first, before any other include.

<!-- elaboration -->
### Shared gate environment

| Function of `Zenith2/Tools/Gates/Common/Zenith2Gates.psm1` | Contract |
|---|---|
| `Get-Zenith2MsvcEnvironment` | Locates the pinned MSVC with `vswhere.exe` (`%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`, filtered to the toolset version in `Zenith2/ThirdParty/manifest.json`), runs `cmd /c "<installationPath>\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 -vcvars_ver=<pinned toolset> -winsdk=<pinned Windows SDK> && set"`, and captures `INCLUDE`, `LIB`, `LIBPATH`, `PATH`, `WindowsSdkDir`, `WindowsSDKVersion` and `VCToolsVersion` into `Zenith2/out/<checkout-hash>/env/msvc_x64.json`, keyed by the manifest's hash (which covers the pinned toolset and the pinned Windows SDK version); a subsequent call reads the file while the hash matches and every cached `INCLUDE`, `LIB` and `LIBPATH` directory still exists, and regenerates it otherwise; it also returns the `cl.exe` version `19.<minor>.<build>`, formed from `VCToolsVersion` by replacing its leading `14` with `19`, which `ApiContractCheck` passes as `-fms-compatibility-version`; an absent toolset returns a failure naming what was not found |
| `New-Zenith2HeaderTU` | Writes `Zenith2/out/<checkout-hash>/hsc/<Lib>/<Header>.hsc.cpp` (the header included twice, nothing else) when absent or older than the header, and returns its path |
| `Get-Zenith2CompileFlags` | Returns the File API `includes[]` (with their system flag), `defines[]` and `compileCommandFragments` of a target's compile group for a preset, configuring the preset first when its build tree is absent |
| `New-Zenith2HscOverlay -Library <Lib>` | Builds `${ZENITH_OUT}/hsc/overlay/<Lib>/`, holding copies of the root headers and root `.inl` files of the library and of its declared dependencies |

- [Z2-N-CONTRACT-428] Gate steps that compile or parse C++ outside CMake (`HeaderSelfContained`, `ApiContractCheck`) obtain their translation units, compile flags and MSVC environment through the functions of the table "Shared gate environment" alone, so each step is self-sufficient and order-independent.
- [Z2-N-CONTRACT-429] `Zenith2/ThirdParty/manifest.json` pins the Windows SDK version alongside the MSVC toolset, so the SDK the gate environment resolves is reproducible and a change to it is a manifest change.
- [Z2-N-CONTRACT-434] The shared gate environment sets `VK_DRIVER_FILES` to the pinned lavapipe ICD manifest of `Zenith2/ThirdParty/manifest.json` in the environment of every test process a gate step starts, so a Vulkan GPU test selected with `rhi.adapter=software` runs on the pinned software driver; the Setup script installs the pinned `VK_LAYER_KHRONOS_validation` on every development machine and CI host, where the Vulkan loader finds it (`Zenith2/Docs/Design/RHI/Vulkan.md`).

### Test layout

<!-- elaboration -->
```
Zenith2/Engine/<Lib>/Tests/
	Zenith_<Type>_Tests.cpp              one file per public type or closely related type group
	Zenith_<Type>_DefectTests.cpp        intentional-defect and death tests, when the type has any
	Examples/Zenith_<Header>_Example.cpp compiled examples cited by contract blocks
	Conformance/                          API libraries only: the shared backend conformance suite
	Support/                              ZenithCore only: test main, counting types, property generator
```

| Target | Built from | Links |
|---|---|---|
| `<Lib>_Tests` | `Zenith2/Engine/<Lib>/Tests/**` except `Conformance/` and `Support/` | `<Lib>`, its declared dependencies, `ZenithTestSupport`, doctest |
| `<ApiLib>_Conformance` | `Zenith2/Engine/<ApiLib>/Tests/Conformance/**` | the API library, every backend of its family, `ZenithTestSupport`, doctest |
| `ZenithTestSupport` | `Zenith2/Engine/ZenithCore/Tests/Support/**` | ZenithCore, doctest |

- [Z2-N-CONTRACT-405] Tests use doctest and are run by CTest, and each library's tests build into the `<Lib>_Tests` executable laid out as in "Test layout" (Z2-D-042).
- [Z2-N-CONTRACT-406] Every test executable's `main` comes from `ZenithTestSupport`, which defines `DOCTEST_CONFIG_IMPLEMENT`, installs a root heap exactly as the Player does, installs no profiler hooks (the lock-order checker lives in ZenithCore and runs regardless), runs doctest, excludes the `z2-death-*` and `z2-sanitizer-defect` suites unless a single test case is named, reports leaks, and exits 3 on any leak.
- [Z2-N-CONTRACT-407] Each doctest `TEST_CASE` is registered as its own CTest test with the same name, by `zenith_add_library()` through doctest's CTest discovery; conformance cases are registered once per backend, named `<doctest name>[<Backend>]`.
- [Z2-N-CONTRACT-408] Tests are judged by pass or fail alone: no test count, pass count or baseline number is pinned anywhere in Zenith2, and adding or removing a test changes no gate configuration.
- [Z2-N-CONTRACT-409] Each test is independent of every other test's order and outcome, prints the seed of any randomness it uses, and has a CTest timeout of 60 seconds unless its `TEST_CASE` carries the `z2-stress` suite, which allows 600 seconds.
- [Z2-N-CONTRACT-410] Tests land in the same ticket as the code they test, never in a separate "add tests" ticket.

### Test naming

<!-- elaboration -->
| Element | Rule | Example |
|---|---|---|
| doctest name | `"<Library>/<Type>/<Behaviour>"`; `<Library>` is the CMake target, `<Type>` the type name without its `Zenith_` or `Flux_` prefix | `"ZenithCore/Arena/OverflowPastBlock_IsCaught"` |
| `<Behaviour>` | PascalCase words, `_`-separated into action, condition and outcome | `ResetToMarker_WithLiveObject_Checks` |
| test ID (used in docs and tickets) | `Test_<Type>_<Behaviour>` | `Test_Arena_OverflowPastBlock_IsCaught` |
| file | `Tests/Zenith_<Type>_Tests.cpp` (`Flux_` for renderer types) | `Tests/Zenith_Arena_Tests.cpp` |

- [Z2-N-CONTRACT-411] Test names follow the table "Test naming"; a test ID and its doctest name map one to one, so a ticket names either and the other is derived.
- [Z2-N-CONTRACT-412] Every failure scenario a ticket's Definition of Done lists has its own test case, named for the failure.

### Death and sanitizer-defect tests

<!-- elaboration -->
| doctest suite | Registered in | CTest pass condition |
|---|---|---|
| `z2-death-check` | every preset | output matches `ZENITH (CHECK\|FATAL\|UNREACHABLE) FAILED` |
| `z2-death-assert` | presets with `ZENITH_ASSERT` enabled | output matches `ZENITH ASSERT FAILED` |
| `z2-sanitizer-defect` | `win-asan`, `linux-tsan`, the S24 HWASan build type | output matches `ERROR: (AddressSanitizer\|HWAddressSanitizer): `, `WARNING: ThreadSanitizer: ` or `runtime error: ` (UBSan) |
| `z2-stress` | every preset | exit code 0 within 600 seconds |

- [Z2-N-CONTRACT-413] A test that expects the process to terminate is placed in a `z2-death-*` or `z2-sanitizer-defect` doctest suite and registered as its own CTest test with the pass condition of the table "Death and sanitizer-defect tests"; it runs in a child process and never inside the default run.

### Test separation

The `TestSeparationCheck` gate step fails on each of the following:

1. a source under any `Tests/` directory appearing in the compile group of a target that is not a test target (CMake File API);
2. a non-test target linking doctest, nanobench, `ZenithTestSupport`, a `<Lib>_Tests` or a `<ApiLib>_Conformance` target;
3. a doctest include or a `TEST_CASE`, `SUBCASE` or `TEST_SUITE` token in a file outside `Tests/` and `Zenith2/Benchmarks/`;
4. a doctest symbol in the `win-release` Player binary's link map;
5. a compile definition that test targets set, non-test targets lack, and non-test sources read (a test switch compiled into shipping code);
6. an `#include` of a public test-access header (a library-root header whose file name ends `TestAccess.h`, for example `ZenithECS/Zenith_SystemTestAccess.h` and `ZenithRuntime/Zenith_RuntimeTestAccess.h`), or a reference to a type whose name ends `TestAccess` wherever it is declared (`Zenith_WorldTestAccess`, in `ZenithECS/Zenith_World.h`), in a source outside `Tests/`, `Zenith2/Benchmarks/` and the `Internal/` sources of the library that declares that header;
7. an `#include` of a public tool-access header (a library-root header whose file name ends `ToolAccess.h`, for example `ZenithRuntime/Zenith_RuntimeToolAccess.h`), or a reference to a type whose name ends `ToolAccess`, in a source outside the sources of `KIND TOOL` targets, `Tests/`, `Zenith2/Benchmarks/` and the `Internal/` sources of the library that declares that header.

- [Z2-N-CONTRACT-414] Tests are never in shipping targets; `TestSeparationCheck` proves the seven conditions listed above, the sixth being what keeps a test-access type, compiled into every configuration, reachable from test and research code alone, and the seventh what keeps a tool-access type reachable from tool, test and research code alone.
- [Z2-N-CONTRACT-415] Shipping code has no test-mode switch: behaviour a test needs to observe is reached through the public API, a fault injector or a test heap passed in, never through a preprocessor flag.

### ForbiddenCheck rules

<!-- elaboration -->
| Rule | Token or construct | Scope where it is rejected | Allowed in |
|---|---|---|---|
| F1 | `new`/`delete` expressions, `malloc`, `calloc`, `realloc`, `free`, `_aligned_malloc`, `_aligned_free`, `aligned_alloc`, `posix_memalign`, `operator new`, `operator delete`, `VirtualAlloc`, `mmap`, `alloca`, `_alloca` | all Zenith2 code | `ZenithCore/Internal/Memory/` (`alloca` nowhere) |
| F2 | placement `new (` | all Zenith2 code | ZenithCore memory and container headers and their `Internal/` sources |
| F3 | `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`, `std::make_unique`, `std::make_shared`, `std::auto_ptr` | all Zenith2 code including tests and tools | nowhere |
| F4 | an angle-bracket `#include` whose name is in the committed C++20 standard-header list `Zenith2/Tools/Gates/std_headers.json` and outside the allowed list (platform SDK headers are F18's) | runtime code | runtime `Internal/` with an allowlist entry and decision ID; tests, tools and benchmarks |
| F5 | any STL type name (`std::vector`, `std::string`, `std::string_view`, `std::span`, `std::optional`, `std::variant`, `std::function`, `std::map`, `std::unordered_map`, `std::array`, `std::thread`, `std::mutex`, and the rest of the standard library outside the allowed headers) | runtime public headers | nowhere |
| F6 | `try`, `catch`, `throw` | all Zenith2 code | `ZenithScript/Internal/`, `ZenithScriptCompiler/Internal/` |
| F7 | `lua_call`, `lua_callk`, `lua_pcall`, `luaL_` entry functions; `#include` of Luau headers | all Zenith2 code | the calls: `ZenithScript/Internal/`; the includes: `ZenithScript/Internal/`, `ZenithScriptCompiler/Internal/` |
| F8 | `dynamic_cast`, `typeid` | all Zenith2 code | nowhere |
| F9 | `thread_local` | all Zenith2 code | `ZenithCore/Internal/Profiling/`, `ZenithProfiling/Internal/`: the two `thread_local` variables of "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]) |
| F10 | `using namespace` | headers | nowhere |
| F11 | a preprocessor conditional on a platform or backend macro (`ZENITH_PLATFORM_*`, `ZENITH_RHI_*`, `_WIN32`, `__ANDROID__`, `__linux__`, `_MSC_VER`, `__clang__`) | all Zenith2 code, tests included (a test that needs a platform API, such as a child test process or its own executable's path, calls ZenithTestSupport's `Zenith_RunChildTestCase`, `Zenith_TestProcess` and `Zenith_GetTestExecutablePath`, ruling X349 (a)) | `ZenithCore/Internal/Zenith_CompilerSupport.h` with a decision ID |
| F12 | any preprocessor conditional between the opening and closing brace of a `class`, `struct` or `union` | all Zenith2 code | nowhere (see "No layout-changing conditionals") |
| F13 | a direct call of `Zenith_Delete(` | all Zenith2 code | `Zenith_Owned.h` |
| F14 | `rand`, `srand`, `std::rand`, `time(`, `clock(` | runtime code | nowhere (randomness through `Zenith_RandomSource`, time through the clock service) |
| F15 | `#pragma warning(disable`, `#pragma clang diagnostic ignored` | all Zenith2 code | third-party wrapper headers under `Internal/` with a decision ID |
| F16 | a writable data symbol | runtime objects compiled from `Zenith2/Engine/**` and `Zenith2/Player/**` outside `Tests/` and third-party roots (see "Writable-data symbol scan") | `writable_data_allowlist.json` entries, each with its declaration comment block ("Writable-data allowlist entries", [Z2-N-CONTRACT-152], [Z2-N-CONTRACT-578]) |
| F17 | exceptions or RTTI enabled in a target's compile fragments (RTTI is off everywhere) | every compile group except Luau, `ZenithScript_EH` and `ZenithScriptCompiler_EH` for exceptions | nowhere |
| F18 | an angle-bracket include of a platform SDK header listed in `Zenith2/Tools/Gates/platform_headers.json` (`windows.h`, `dbghelp.h`, `pthread.h`, `unwind.h`, `dlfcn.h`, `android/…` and the rest) | all Zenith2 code | files under `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/` matching the header's platform; the file of a `publicPlatformHeaders` element, for the headers it lists (the per-platform threading header, [Z2-N-CONTRACT-583]) |
| F19 | an `#include` of a `.inl` | all Zenith2 code | the `.h` of the same stem in the same directory |
| F20 | an `#include` of `ZenithRHI/Zenith_RHICommandList.h` or of `ZenithRHI/Zenith_RHIBarrier.h` (Z2-D-056) | all Zenith2 code | command list: `ZenithFlux/Internal/**`, the RHI backend libraries, ZenithRHI's own sources and tests; barriers: `ZenithFlux/Internal/Graph/**`, the RHI backend libraries, ZenithRHI's own sources and tests |
| F21 | a call of a C runtime transcendental function of `<cmath>` or `<math.h>` (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `log`, `log2`, `log10`, `pow`, `cbrt`, their `std::` forms and their `f` and `l` suffixed forms); `std::sqrt` is not matched ([Z2-N-ARCH-888]) | runtime code (`Zenith2/Engine/**`, `Zenith2/Player/**`), `Zenith2/Tools/Authoring/**`, `Zenith2/Tools/ZenithCook/**`, and every further tool directory whose output is tracked or baked that an area's design names under [Z2-N-ARCH-888] (for example CHAR's `Zenith2/Tools/ZenithEditor/Internal/Character/**`) | test files that compare against the host library, each through a `forbidden_allowlist.json` entry (CORE's `Test_Transcendental_WithinBoundOfHostLibrary_*`) |
| F22 | a call of `LoadLibrary*` or `dlopen` | all Zenith2 code | `ZenithPlatform/Internal/**` and `ZenithRHI_*/Internal/**` ([Z2-N-ARCH-274]) |
| F23 | an `#include` of a third-party header | all Zenith2 code | its owner's `Internal/`, and the test, benchmark or tool targets that declare it ([Z2-N-ARCH-165]) |
| F24 | a call or address-of of a raw file, process, socket or dynamic-loading API listed in `Zenith2/Tools/Gates/confined_apis.json` | every Zenith2 source | the directories its allow records name (the dynamic-loading records match [Z2-N-ARCH-274]; `Zenith2/Docs/Design/SEC/PathTraversal.md` → "Confined OS APIs") |
| F25 | four token groups, each finding naming its group: (a) the identifiers `noexcept` and `nodiscard`, so `noexcept`, `noexcept(…)`, `[[nodiscard]]`, `[[nodiscard("…")]]` and `[[nodiscard, maybe_unused]]` all match; (b) every identifier beginning `u_int` (`u_int8` … `u_int64`); (c) the identifiers `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint32_t` and `uint64_t`, `std::`-qualified or not; (d) `final` as the class-virt-specifier of a `class`, `struct` or `union` definition with no base clause (the class head followed by `final` and then the body's `{` rather than `:`) | runtime public and `Internal/` sources and tooling sources (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table") | (a) a line whose own `//` comment reads `// ZENITH-ALLOW: ` followed by a non-blank reason ([Z2-N-CONTRACT-582], [Z2-N-CONTRACT-586]); a comment on another line, or one with no reason, exempts nothing. (b) nowhere. (c) `Zenith2/Engine/ZenithCore/Zenith_Types.h`, which defines `uint8` … `uint64`, `sint8` … `sint64`, `uint` and `sint` from them, and the third-party boundary files of the `f25Boundary` list of `forbidden_allowlist.json` ([Z2-N-CONTRACT-417]), each under its third-party owner's `Internal/`. (d) nowhere; a `final` class with a base clause passes. No `entries` element exempts F25 (Z2-D-141) |
| F26 | code view, two clauses, each finding naming its clause: **(i)** a profiled synchronisation primitive: the whole-token identifiers `Zenith_Mutex`, `Zenith_ConditionVariable` and `Zenith_Semaphore`, and a platform class template of those primitives instantiated with `true`, that is `Zenith_<Platform>Mutex_T`, `Zenith_<Platform>ConditionVariable_T` or `Zenith_<Platform>Semaphore_T` (`<Platform>` one of `Windows`, `Android`, `Linux`) followed by `<`, `true` and `>` (blanks allowed between); the aliases `Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling` and `Zenith_SemaphoreNoProfiling` and the `<false>` instantiations are other whole identifiers and never match; **(ii)** a direct use of the hook table: either the identifier `Zenith_GetProfilerHooks`, `(`, `)` and then `.` or `->` and an identifier beginning `m_pfn` other than the log-record members `m_pfnOnLogRecord` and `m_pfnIsLogLevelEnabled` (blanks and line breaks allowed between), or one of the identifiers `m_pfnOnZoneBegin`, `m_pfnOnZoneEnd`, `m_pfnOnCounter`, `m_pfnOnLockAcquire`, `m_pfnOnLockRelease` and `m_pfnOnBlockingWait` followed by optional blanks, an optional `)` and `(`, so a call through a `Zenith_ProfilerHooks` value matches while an assignment of the member, its declaration and a call of a `Zenith_Dispatch…` function do not; the log-record members `m_pfnOnLogRecord` and `m_pfnIsLogLevelEnabled` are outside both forms (named in neither), because the log-record hooks stay outside the dispatch functions of `Zenith_ProfilerHooks.h`: the failure procedure emits its fatal record from inside whatever hook asserted, and the log router's locks and drain semaphore are `_NoProfiling` variants instead | (i) `runtime-public` and `runtime-internal` files under `Zenith2/Engine/ZenithProfiling/**` or `Zenith2/Engine/ZenithCore/Internal/Profiling/**`, and every file named by the `profilerReachable` list of `forbidden_allowlist.json`; (ii) every class | (i) `test` files (a test's subject may be a profiled primitive); (ii) `Zenith2/Engine/ZenithCore/Zenith_ProfilerHooks.h` and `test` files. No `entries` element and no signpost exempts F26; a file in both the directories and the list is reported once. Clause (ii) is lexical: a hook member copied into a variable and then called, or a ZenithCore type that embeds a profiled mutex and is used from profiler code, is not seen by the scan; layer (c), the run-time hook-depth guard of [Z2-N-CONTRACT-590], and review cover those cases. |

- [Z2-N-CONTRACT-416] The `ForbiddenCheck` gate step enforces rules F1 to F26 of the table "ForbiddenCheck rules" over tracked and untracked Zenith2 files, with comments and string literals stripped and tokens matched as whole identifiers (so `Free(` never matches `free`, nor `GetTime(` match `time(`), and fails on any occurrence outside the allowed scope.
- [Z2-N-CONTRACT-433] Rule F20 makes Z2-D-056 mechanical: no code outside the render graph's internals and the RHI backends can record a command or a barrier, because it cannot include the headers that declare them.
- [Z2-N-CONTRACT-579] Rule F21 makes [Z2-N-ARCH-888] mechanical over every directory whose code affects simulation or produces tracked or baked output; implementing it in `ForbiddenCheck` is a PROC `needs-human` change (a protected gate path), and until that change lands an area enforces the rule over its own directories with a non-protected CTest `cmake -P` source scan registered in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, the precedent of CORE's `ZenithCore.TranscendentalNoCrtCall` (`Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions").
- [Z2-N-CONTRACT-432] `Zenith2/Tools/Gates/platform_headers.json` entries have the form `{ "header": "windows.h", "platform": "Windows" }` with `platform` one of `Windows`, `Android`, `Linux`, and its `publicPlatformHeaders` list holds the elements `{ "path", "platform", "headers", "comment" }` of the one public-header exception, [Z2-N-CONTRACT-583]: a `path` that is `Zenith2/Engine/ZenithCore/Platform/<platform>/Zenith_PlatformThreading.h` with `<platform>` equal to the element's `platform`, `headers` naming only `pthread.h` and `semaphore.h`, and a `comment` beginning `ZENITH-EXEMPTION: ` (one element each for Android and Linux; Windows has none); compiler sanitizer headers (`<sanitizer/*.h>`) are neither standard nor platform headers and are allowed in `ZenithCore/Internal/Memory/`, a scope extension expressed as allowlist entries with `rule` `SANITIZER-HEADER`.
- [Z2-N-CONTRACT-417] Exemptions live in `Zenith2/Tools/Gates/forbidden_allowlist.json`, a wrapper `{ "schema", "schemaVersion", "comment", "f21Scope", "f25Boundary", "profilerReachable", "entries" }` whose entries are `{ "id", "rule", "path", "decision" }` with optional `token` and `comment`; `decision` is an existing `Z2-D-NNN` ID and nothing else, and an entry without one fails the step. `f25Boundary` is a scope list, not an exemption entry: its elements `{ "path", "thirdParty", "comment" }` name the files under a third-party owner's `Internal/` where a third-party API signature names a fixed-width `<cstdint>` type that Zenith code passes through unchanged, the one place rule F25 (c) admits `int8_t` … `uint64_t` besides `Zenith_Types.h`. `profilerReachable` is likewise a scope list, not an exemption: its elements `{ "path", "comment" }` name the sources outside `Zenith2/Engine/ZenithProfiling/` and `Zenith2/Engine/ZenithCore/Internal/Profiling/` that a profiler hook dispatch can reach, and rule F26 (i) refuses a profiled primitive in each of them ([Z2-N-CONTRACT-591]).

### Complexity gate

- [Z2-N-CONTRACT-418] The `Complexity` gate step runs `analyze_code_complexity.py --profile zenith2 --profile-file Zenith2/Tools/Gates/complexity_profile.json`, and every threshold in the profile (`max-cc`, `max-cognitive`, nesting, `max-loc`, `max-forbidden-tokens`, `max-file-loc`) is a hard failure (Z2-D-007).
- [Z2-N-CONTRACT-419] The profile file is the single source of the threshold values; they are calibrated from measured distributions, and a recalibration is a MAINT `needs-human` ruling ticket that commits the measurement and its decision entry, followed by a PROC `needs-human` ticket, blocked by it, that edits the protected profile (`Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration").
- [Z2-N-CONTRACT-420] An exception to a threshold is an entry in the profile naming the function or file and a `Z2-D-` ID; reviewers reject artificial fragmentation (splitting a coherent function only to pass a limit) and prefer a justified exception.

### Examples compiled as tests

- [Z2-N-CONTRACT-421] A public usage example exists as a compiled source under `Tests/Examples/`, built into the library's test target and run as a test, or, when the example must run against every backend of an API library's family, under that API library's `Tests/Conformance/Examples/`, built into the family conformance executable `<ApiLib>_Conformance` (never into `<Lib>_Tests`) and run once per backend; a contract block cites it in its `Example` field, and no uncompiled code example appears in a public header.

## No layout-changing conditionals

- [Z2-N-CONTRACT-441] No preprocessor conditional changes the layout of any type: its members, base classes, virtual functions, `sizeof`, `alignof` or member offsets are identical in every configuration, preset and platform.
- [Z2-N-CONTRACT-442] Platform code lives in `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/` files selected by `zenith_add_library()`, and backend code lives in separate backend targets; a platform or backend difference is never an `#ifdef` inside a shared file (rule F11).
- [Z2-N-CONTRACT-443] Debug-only data lives outside the objects it describes, in side tables (the leak side table is the model); a member that exists in some configurations and not others does not exist.
- [Z2-N-CONTRACT-444] A configuration difference that is behavioural (asserts, sanitizer calls, lock-order hooks, debug validation) is expressed as a function whose body differs by configuration inside `Internal/` or as a macro in `Zenith_Assert.h`; its call sites and every type's layout stay identical.
- [Z2-N-CONTRACT-445] `ForbiddenCheck` rules F11 and F12 enforce the textual forms of this contract, and review confirms that a behavioural difference does not change an observable data format.

## Errors and results

### Zenith_Result

<!-- elaboration -->
```cpp
template<typename E>
concept Zenith_ErrorType =
	(std::is_enum_v<E> && std::is_trivially_copyable_v<E>)
	|| (std::is_class_v<E> && std::is_trivially_copyable_v<E> && sizeof(E) <= 32
		&& requires(const E& xError) { requires std::is_enum_v<decltype(xError.m_eCode)>; });

template<typename E> requires Zenith_ErrorType<E>
constexpr auto Zenith_GetErrorCode(const E& xError);   // the enum itself, or xError.m_eCode

template<typename E>
struct Zenith_ErrorValue
{
	E m_eError;
};

template<typename E>
constexpr Zenith_ErrorValue<E> Zenith_Err(E eError) { return { eError }; }

template<typename T, typename E>
class Zenith_Result
{
	static_assert(Zenith_ErrorType<E>);   // an error enumeration, or a code-plus-context struct (see below)
	static_assert(std::is_default_constructible_v<T>);   // a failure result holds a default-constructed T
	static_assert(std::is_default_constructible_v<E>);   // a success result holds a value-initialised E
public:
	Zenith_Result(T&& xValue);                     // success: m_eError is value-initialised, so its code is 0
	Zenith_Result(const T& xValue) requires std::is_copy_constructible_v<T>;
	Zenith_Result(Zenith_ErrorValue<E> xError);    // failure; ZENITH_ASSERT: Zenith_GetErrorCode(error) != 0; m_xValue default-constructed
	Zenith_Result(Zenith_Result&& xOther) = default;
	Zenith_Result& operator=(Zenith_Result&& xOther) = default;
	~Zenith_Result() = default;

	bool IsOk() const;               // Zenith_GetErrorCode(m_eError) == 0
	bool IsError() const;            // !IsOk()
	T& Value() &;                    // ZENITH_ASSERT: IsOk()
	const T& Value() const&;         // ZENITH_ASSERT: IsOk()
	T TakeValue() &&;                // ZENITH_ASSERT: IsOk(); moves the value out
	E Error() const;                 // ZENITH_ASSERT: IsError()

private:
	T m_xValue;
	E m_eError;
};

template<typename E>
class Zenith_Result<void, E>
{
	static_assert(Zenith_ErrorType<E>);
	static_assert(std::is_default_constructible_v<E>);
public:
	Zenith_Result();                                // success: m_eError is value-initialised, so its code is 0
	Zenith_Result(Zenith_ErrorValue<E> xError);     // failure; ZENITH_ASSERT: Zenith_GetErrorCode(error) != 0
	bool IsOk() const;                // Zenith_GetErrorCode(m_eError) == 0
	bool IsError() const;             // !IsOk()
	E Error() const;                  // ZENITH_ASSERT: IsError()

private:
	E m_eError;
};

// ZENITH_TRY(Expr): evaluates Expr once; on error returns Zenith_Err(result.Error()) from the enclosing function,
//   whose return type is a Zenith_Result with the same E.
// ZENITH_TRY_ASSIGN(Declaration, Expr): as ZENITH_TRY, then Declaration = std::move(result).TakeValue().
```

- [Z2-N-CONTRACT-461] Recoverable errors are returned as `Zenith_Result<T, E>` (Z2-D-045); `Zenith_Result` carries no heap allocation, no string and no exception, it is not `[[nodiscard]]` and not `final` ([Z2-N-CONTRACT-586]), and neither error form below changes the no-exceptions rule.
- [Z2-N-CONTRACT-587] `Zenith_Result<T, E>` stores exactly `{ T m_xValue; E m_eError; }`, and `Zenith_Result<void, E>` exactly `{ E m_eError; }`, in every build: no union, no success flag and no debug-only member, so the layout is identical in every configuration ([Z2-N-CONTRACT-441]) and a non-debug build carries no state beyond the value and the error. `T` and `E` are default-constructible, each checked by a `static_assert(std::is_default_constructible_v<…>)` (a failure result holds a default-constructed `T`, a success result a value-initialised `E`), so every type used as a result value or error has a default constructor or default member initializers on every member, `const` members of `IMMUTABLE` types included ([Z2-N-CONTRACT-585]); a success result holds a value-initialised `E`, whose code is 0, and `IsOk()` is `Zenith_GetErrorCode(m_eError) == 0`, which [Z2-N-CONTRACT-465] makes exact because no error code is 0 (Z2-D-141).
- [Z2-N-CONTRACT-480] `E` takes one of two forms, enforced by the `Zenith_ErrorType` concept through `static_assert(Zenith_ErrorType<E>)` inside `Zenith_Result`: an error enumeration (an `enum class`, or the prefixed `enum Zenith_<Subsystem>Error : uint8` form of "Error enumerations"), trivially copyable; or a trivially copyable struct of at most 32 bytes whose public `m_eCode` member is such an enumeration, carrying fixed-size context beside the code (a step index, an ID, a count), never a pointer to heap data or a string.
- [Z2-N-CONTRACT-481] Generic code reads the code of either form through `Zenith_GetErrorCode(const E&)`, which returns the enumeration itself or `m_eCode`; a failure result's code is never 0, and `ZENITH_TRY` propagates an error unchanged within one `E`, whichever form it has.
- [Z2-N-CONTRACT-462] Reading `Value()` or `TakeValue()` of an error result, or `Error()` of a success result, is a `ZENITH_ASSERT` failure, checked in Debug and sanitizer builds alone (table "Assertions"); a Release or Shipping build performs no check, so such a read returns the default-constructed `T` or the value-initialised `E`, and the misuse is a programmer error the Debug test runs catch (Z2-D-141).
- [Z2-N-CONTRACT-463] An error crossing a library boundary into a caller with a different error enumeration is mapped explicitly at that boundary; `ZENITH_TRY` propagates within one enumeration.
- [Z2-N-CONTRACT-464] Diagnostic context (paths, values, counts) is logged at the point of failure through the structured log; the error value names the category of failure.

### Error enumerations

<!-- elaboration -->
- [Z2-N-CONTRACT-465] Each library declares its error enumerations as `enum Zenith_<Subsystem>Error : uint8` (or `uint16`), with enumerators `<SUBSYSTEM>_ERROR_<NAME>` numbered from 1; value 0 is never an enumerator, because a `Zenith_Result` whose error code is 0 is a success ([Z2-N-CONTRACT-587]).
- [Z2-N-CONTRACT-482] An error type takes the name `Zenith_<Subsystem>Error` in both forms of "Zenith_Result": in the enumeration form the enumeration itself is `Zenith_<Subsystem>Error`; in the struct form the struct is `Zenith_<Subsystem>Error` and its code enumeration, held in the member `m_eCode`, is `Zenith_<Subsystem>ErrorCode`, numbered from 1 with the same `<SUBSYSTEM>_ERROR_` enumerator prefix.
- [Z2-N-CONTRACT-466] Every error enumeration has a `constexpr const char* Zenith_ToString(Zenith_<Subsystem>Error eError)` overload in the same header, used by logs and tests, and its contract block describes when each enumerator is returned.
- [Z2-N-CONTRACT-467] An enumerator once shipped in a fix version keeps its meaning; a new failure gets a new enumerator.

### Assertions

<!-- elaboration -->
| Macro | Enabled in | On failure | Use |
|---|---|---|---|
| `ZENITH_ASSERT(Condition, Format, …)` | Debug builds and sanitizer builds (`Debug`, `Sanitize`, `Hwasan`), in every target; off in Release, Shipping and ShippingTest, host tools included | report, debugger break when attached, terminate | programmer errors on hot paths |
| `ZENITH_CHECK(Condition, Format, …)` | every configuration including `win-release` and shipping Android | report, terminate | programmer errors whose continuation would corrupt memory or state |
| `ZENITH_FATAL(Code, Format, …)` | every configuration | report with a `Zenith_FatalCode`, terminate | unrecoverable runtime conditions (out of memory, arena exhaustion, lock-order violation, the script wall-clock watchdog) |
| `ZENITH_UNREACHABLE()` | every configuration | report, terminate | the impossible branch of a switch over a closed enumeration |

- [Z2-N-CONTRACT-479] Each assertion macro is enabled exactly as the table "Assertions" states, and `ZENITH_ASSERT` is controlled per configuration, never per target: every target of a configuration (engine libraries, the Player, game executables and the host tools `ZenithCook`, `ZenithShaderCompiler`, `ZenithHub` and every `zenith_add_tool` target alike) compiles with `ZENITH_ASSERTS_ENABLED=1` in Debug builds and sanitizer builds and `ZENITH_ASSERTS_ENABLED=0` in Release and Shipping, so every link sees one value and an asserting inline or template member of an engine header has one definition per link (a per-target value would give it two in a Release tool, an ODR violation); `ZENITH_CHECK`, `ZENITH_FATAL` and `ZENITH_UNREACHABLE` are enabled in every target and configuration, and host-tool code that needs an invariant checked in the Release tools the `win-tools` preset builds uses `ZENITH_CHECK`. The `win-tools` preset therefore needs no configuration of its own (`Zenith2/Docs/Architecture/BuildSystem.md` → "Configurations").
- [Z2-N-CONTRACT-468] Assertion macros are hygienic: each is a single statement, evaluates its condition exactly once when enabled, and when disabled expands to `static_cast<void>(sizeof(!(Condition)))`, which type-checks the condition without evaluating it.
- [Z2-N-CONTRACT-469] An assertion condition has no side effects; review rejects one that does, and the stage-acceptance clang-tidy run includes `bugprone-assert-side-effect` configured for `ZENITH_ASSERT`.
- [Z2-N-CONTRACT-470] The first line of every assertion report is `ZENITH <KIND> FAILED: <condition text> (<file>:<line>) <message>`, with `<KIND>` one of `ASSERT`, `CHECK`, `FATAL`, `UNREACHABLE`, followed by the callstack and a crash dump through the crash handler selected by D-SEL-CRASH (open, Stage 2).
- [Z2-N-CONTRACT-471] Asserts guard programmer errors in engine code and nothing else.

### Untrusted input

- [Z2-N-CONTRACT-472] Data from outside the process image (files, cooked assets, authoring documents, saves, replays, network messages, command-line arguments, environment variables, clipboard, platform callbacks) never reaches an assert: every parser validates bounds and returns `Zenith_Result`, with the parser limits of `Zenith2/Docs/Architecture.md → "Security architecture"`.
- [Z2-N-CONTRACT-473] Script misuse never relies on asserts; it always raises a controlled Luau error (see `Zenith2/Docs/Scripting.md → "Views epochs and misuse errors"`).

### Exceptions and the Luau firewall

- [Z2-N-CONTRACT-474] No C++ exception is thrown, caught or propagated in Zenith2 code outside Luau, `ZenithScript/Internal` and `ZenithScriptCompiler/Internal` (Z2-D-045, Z2-D-017).
- [Z2-N-CONTRACT-475] Every entry into Luau, from anywhere, is a protected call inside `ZenithScript/Internal`; an engine callback that re-enters script goes through that same protected entry, so no Luau error unwinds through an exceptions-disabled engine frame.
- [Z2-N-CONTRACT-476] Restated (Z2-D-141): an engine function a Luau binding calls carries no exception specification ([Z2-N-CONTRACT-582]); it cannot throw, because every engine target outside the firewall compile groups is compiled with exceptions off ([Z2-N-CONTRACT-394]), and it re-enters script only through the protected entry of [Z2-N-CONTRACT-475], so no Luau error unwinds through it.
- [Z2-N-CONTRACT-477] Whether the pinned Luau's API has C or C++ linkage is verified by a Stage 1 ticket, and the firewall design in `Zenith2/Docs/Scripting.md → "Luau integration and the exception firewall"` records the result.

### Fatal errors

<!-- elaboration -->
| `Zenith_FatalCode` | Raised by |
|---|---|
| `FATAL_CODE_OUT_OF_MEMORY` | the root heap after the OOM handler gives up |
| `FATAL_CODE_ARENA_EXHAUSTED` | `Zenith_Arena::Allocate` at its cap |
| `FATAL_CODE_QUEUE_FULL` | a `QUEUE_FULL_POLICY_FATAL` queue |
| `FATAL_CODE_LOCK_ORDER` | the debug lock-order checker |
| `FATAL_CODE_ASSERTION` | `ZENITH_ASSERT`, `ZENITH_CHECK`, `ZENITH_UNREACHABLE` |
| `FATAL_CODE_SCRIPT_WATCHDOG` | the script wall-clock watchdog (a fatal, non-replayable diagnostic event, Z2-D-052) |
| `FATAL_CODE_DEVICE_LOST` | a device-loss (or frame-failure) exit whose autosave and shutdown have not finished within 10 s; the exit itself is clean, with `EXIT_CODE_DEVICE_LOST` (6), and this fatal is its fallback alone (see `Zenith2/Docs/Rendering.md → "Recovery"`) |
| `FATAL_CODE_SCRIPT_HANG` | the script watchdog, when an invocation is still running after twice the watchdog limit (stuck inside a C++ binding, where no safepoint occurs), after the hang report through the crash handler (`Zenith2/Docs/Scripting.md → "Budgets safepoints and the watchdog"`) |
| `FATAL_CODE_LOGGING_UNAVAILABLE` | the log router's drain thread could not start at startup step 7 |

- [Z2-N-CONTRACT-478] A fatal error terminates through `ZENITH_FATAL` with one `Zenith_FatalCode` from the table "Fatal errors"; a new fatal condition adds a code to that table no later than the diff that raises it, landed through the core-doc batch that applies the raising area's design as [Z2-N-CONTRACT-320] states for lock rows.
- [Z2-N-CONTRACT-484] A device loss that cannot be recovered is a clean exit, never a fatal: the device-loss handler requests an autosave and shuts down with `EXIT_CODE_DEVICE_LOST` (6, "the graphics device was lost and could not be recovered; autosave requested", which also covers 30 consecutive frames that could not be rendered), and raises `FATAL_CODE_DEVICE_LOST` only when that exit has not finished within 10 s; `EXIT_CODE_DEVICE` (4) keeps the meaning "a required device could not be created", so CI tells a runtime loss from a missing driver (`Zenith2/Docs/Architecture.md → "Composition root and Player"`, table "Exit codes" of its CompositionRoot companion; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine").
- [Z2-N-CONTRACT-483] When a fatal error is raised while a log-record hook is installed, the log router outputs every buffered record and then the fatal record to every registered sink synchronously, in sequence order, before the process terminates, so no record accepted before the fatal error is lost (see `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router").

## Review and change management

### Diff-derived API review

<!-- informative -->
API review is enforced from the diff, not from a label (Z2-D-049). The mechanism is delivered by the Stage 0 PROC-API ticket, which is `needs-human` because it edits protected files. The `api-change` label remains a board annotation and triggers nothing.
<!-- /informative -->

| # | Element | Delivered by PROC-API |
|---|---|---|
| 1 | a generic `reviewPaths` category key; `zagent gates`, run at measure with the changed set, writes `review.requiredByDiff` into `gates.json` | yes |
| 2 | a `TICK.md` step 6.3 edit: the reviewer is dispatched when `review.required` from the payload, or `review.requiredByDiff`, is true; followed by `Build-HarnessArtifacts.ps1 -Harness <id>` regeneration for claude, opencode and codex, and a re-derived tier-mention allowlist | yes |
| 3 | a revision-bound review-completion signal: `zagent review record <KEY> --verdict <pass\|fixes-applied> --reviewer <tier>`, written into the ticket's history with a hash over the current blobs of every changed path in the `reviewPaths` scope | yes |
| 4 | enforcement: `zagent finish --status Done` refuses when `requiredByDiff` is true and no review record's hash still matches the current blobs | yes |
| 5 | a test: a post-review edit to a reviewed header is refused at `finish` | yes |
| 6 | Vault tests for each element above | yes |

- [Z2-N-CONTRACT-491] A diff that changes any path in the Z2 categories' `reviewPaths` requires an independent review before the ticket can finish Done, whatever the ticket's risk or labels.
- [Z2-N-CONTRACT-492] The Z2 categories list as `reviewPaths` every public root header and public inline file (`Zenith2/Engine/*/*.h`, `Zenith2/Engine/*/*.inl`), every generated public API output (generated public headers and the generated `.d.luau` files), `Zenith2/Tools/Gates/libgraph_baseline.json`, and `Zenith2/Tools/Maintenance/*.json` (the engine changelog, the API surface listing and the ownership, rehearsal and sample-games files).

### Review record hash

<!-- elaboration -->
- [Z2-N-CONTRACT-493] The review record hash is SHA-256 over the UTF-8 text formed by sorting the changed paths inside the `reviewPaths` scope by byte order and joining, with `\n`, one line per path of the form `<repo-relative path> <git blob SHA-1 of the working-tree content>`, where a deleted path uses the blob value `deleted`.
- [Z2-N-CONTRACT-494] Any change to a reviewed path after the review record was written changes the hash and invalidates the record; a new review is then needed.

### Finish refusal

- [Z2-N-CONTRACT-495] `zagent finish --status Done` for a Z2 ticket whose `review.requiredByDiff` is true is refused unless a `zagent review record` entry exists whose hash matches the current blobs; the refusal names the paths whose blobs differ.

### Reviewer conventions

- [Z2-N-CONTRACT-496] The Z2 categories carry a `reviewerConventions` value, inlined into the reviewer prompt by the generic per-category field a definite Stage 0 PROC ticket adds, instructing the reviewer to challenge behaviour beyond the author's tests and to check each item of the ticket's "Review focus" list in `## Validation`.
- [Z2-N-CONTRACT-497] Every ticket's `## Validation` section carries a "Review focus" list of behaviours and missing cases a reviewer probes beyond the author's tests.

### Independent review for risky changes

- [Z2-N-CONTRACT-498] A ticket routed `risk HIGH` (a defect would be silent — memory, synchronisation, determinism — or the change is a destructive migration, a security boundary, a release or packaging path, or a save-format change) is reviewed independently of its author.
- [Z2-N-CONTRACT-499] A Definition of Done asserts observable checks and avoids phrasing that forces review without need ("provably", "byte-identical", "no new allocations"), unless review is wanted.
- [Z2-N-CONTRACT-511] A ticket's Goal and Definition of Done contain no `##` or `###` heading, and no heading named `Gates` appears anywhere in a ticket body.
- [Z2-N-CONTRACT-512] Governing rulings are cited by `Z2-D-NNN` inside the Goal or Definition of Done text, because the rulings matcher reads those two sections; the `Decisions:` line repeats them.
- [Z2-N-CONTRACT-513] Outside `needs-human` tickets, the Goal and Definition of Done avoid board references (`the blocker`, `the blocking`, `linked` or `related` ticket, `the parent` ticket) and human-only phrases (human, manual or visual sign-off or approval; "no agent may", "no agent can"; "a human must", "a person must"; "human-executed", "human-authored", "human-only"; "needs a human"; "only a human can"; "must be done by a human", "must be run by a human", "must be authored by a human"), which raise prompt warnings.

### Refactor owed

- [Z2-N-CONTRACT-500] A change that leaves a known shortcut behind (a temporary limitation, a duplicated mechanism, a missing failure path) records it in its result, and a refactor ticket is filed in the same stage, owned by the area epic of the shortcut.
- [Z2-N-CONTRACT-501] A refactor ticket names the shortcut's exact location and the contract it restores; a shortcut without a filed ticket is rejected in review.

### Living documents

- [Z2-N-CONTRACT-502] A ticket that changes a contract, an API or a design edits the governing section of `Zenith2/Docs/Design/<CODE>.md`, this document or the core document concerned in the same diff, and its Definition of Done says so (Z2-D-085).
- [Z2-N-CONTRACT-503] A ruling is recorded by appending its entry to `Zenith2/Docs/DecisionLog.md` in the ticket's own diff with an explicit, previously unused `Z2-D-NNN` ID (Z2-D-097), following `Zenith2/Docs/DecisionLog.md → "How this log is written"`; `zagent decide --ID Z2-D-NNN` is at most an optional knowledge-row mirror and never the record.
<!-- elaboration -->
- [Z2-N-CONTRACT-504] A section a ticket edits keeps its heading text, because tickets cite headings by exact text; a rename is a separate ticket that updates every citing ticket through the filer's reconcile mode.
- [Z2-N-CONTRACT-505] Every normative statement added to a scanned document is a tagged bullet with a fresh `[Z2-N-<DOC>-NNN]` ID, never reusing a retired ID.

### Subsystem ownership

- [Z2-N-CONTRACT-506] Every subsystem has a named owner, which is its area epic `Z2 <CODE> — <Area name>`; the owner is responsible for the subsystem's maintenance tickets, refactors owed and dependency upgrades.
- [Z2-N-CONTRACT-507] Anything spanning areas is owned by one area, which names the others in its tickets' `## Dependencies` section.

### Benchmark regressions

- [Z2-N-CONTRACT-508] A benchmark or workload regression beyond its budget row's tolerance blocks the stage acceptance until it is fixed or re-budgeted by a decision (Z2-D-012).
- [Z2-N-CONTRACT-509] A mid-sprint integration check that measures a regression files it as a ticket in the current sprint, owned by the regressing area.

### Challenges to settled choices

- [Z2-N-CONTRACT-510] A settled `Z2-D-` decision is challenged only as a flagged proposal with a concern, evidence, a proposal and a trade-off; nothing changes until the human approves it (Z2-D-022), and work proceeds under the settled decision meanwhile.

## Compatibility policy per kind

### Compatibility table

| Kind | Policy | Mechanism | Evidence |
|---|---|---|---|
| Source API (C++) | deprecated for one fix version, with a replacement, then removed | `[[deprecated("use <Replacement>; removed in Z2 Stage <N+1>")]]` and the contract block's `Version` field | every in-repo caller migrated in the deprecating diff; removal ticket filed in the next fix version |
| Luau-facing API (bindings and generated `.d.luau`) | deprecated for one fix version, with a replacement, then removed | a load-time deprecation diagnostic per script and call site | see `Zenith2/Docs/Scripting.md → "Shipping bytecode compatibility"` |
| Modules | statically linked, so no binary compatibility surface exists | static libraries selected in `game.json` (Z2-D-024) | none needed |
| Saves | forward migration from every shipped save version of the current major release | a per-version migration chain in SAVE | a migration fixture per shipped version |
| Cooked assets | re-cooked on any engine cook version change (`uZENITH_ENGINE_COOK_VERSION`); the runtime refuses mismatched cooked versions; patches compatible within one release line | cooked headers record format version, engine cook version, codec ID | a load test of a mismatched cooked file |
| Authoring documents | migrated, never merely re-cooked | the authoring-document migration framework below | a migration fixture per version, per type |

- [Z2-N-CONTRACT-521] Each kind of persistent or consumed interface follows its row of the table "Compatibility table" (Z2-D-048).

### Source API deprecation

- [Z2-N-CONTRACT-522] A deprecated C++ declaration carries `[[deprecated("use <Replacement>; removed in Z2 Stage <N+1>")]]`, and its contract block's `Version` field records the deprecation.
- [Z2-N-CONTRACT-523] Because every C++ consumer is in this repository and deprecation warnings are errors, the diff that deprecates a declaration migrates every in-repo caller; the deprecated declaration then remains for one fix version and is removed by a ticket filed in that fix version.
- [Z2-N-CONTRACT-580] Every contract block of a public header has one stability class (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Stability classes"). Its effective `since` stage is `max(since, 2)`, because the engine version first exists in Stage 2. A block without a class clause is stable: shipped when its effective `since` is below the engine MAJOR (its stage accepted), provisional otherwise, and [Z2-N-CONTRACT-523] binds both. A block whose `Version` field carries `; experimental until Z2 Stage <G> (consumers <targets>)` or `; internal`, or whose header falls under the internal name rules N1 to N4 of that section, changes in any diff that migrates every in-repo caller, with no deprecation.
- [Z2-N-CONTRACT-581] Only a declaration a diff introduces may start experimental or internal: giving either clause to a stable block present in an earlier commit is an incompatible change of a stable declaration under [Z2-N-CONTRACT-523]; an experimental or internal block may become stable in any diff.

### Save migration

- [Z2-N-CONTRACT-524] A save carries a `(major, minor)` version; the runtime loads every save version shipped within the current major release by applying forward migration steps in sequence, and refuses a save of another major version with a recoverable error.
- [Z2-N-CONTRACT-525] Each shipped save version has a committed migration fixture and a test that loads it, migrates it to the current version and asserts the migrated state.
- [Z2-N-CONTRACT-526] Save writes are atomic and journaled like authoring documents (see "Journaled atomic write"); save-specific rules live in `Zenith2/Docs/Design/SAVE.md`.

### Cooked asset versioning

<!-- elaboration -->
- [Z2-N-CONTRACT-527] Every cooked file begins with a header recording a four-byte format magic, the format version, the engine cook version, the compression codec ID of each compressed section, and the content hash of the source inputs.
- [Z2-N-CONTRACT-528] The runtime refuses a cooked file whose format version or engine cook version differs from its own with a recoverable asset error, never an assert; a patch applies only within one release line.
- [Z2-N-CONTRACT-529] For cooked data the engine version is the engine cook version `uZENITH_ENGINE_COOK_VERSION` that every cooked header records (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Constants and fault points"): any change of it triggers a re-cook, and cooked data is never migrated at runtime; MAINT's engine semantic version (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Engine version") moves without forcing a re-cook, and the re-cook rule for Luau bytecode is [Z2-N-SCRIPT-505]'s, with its own key.

### Authoring-document migration

<!-- informative -->
Authoring documents are the data a human made: scenes, prefabs, data-defined component schemas, animation, UI and cinematic documents, material definitions, project settings and importer `.meta` settings. They are migrated in place, with the human's work preserved, and never merely re-cooked.
<!-- /informative -->

- [Z2-N-CONTRACT-552] The authoring-document types are scenes, prefabs, data-defined component schemas (`*.schema.json`), animation documents (`Zenith.AnimationGraphDocument`, `Zenith.AnimationMaskDocument`, `Zenith.AnimationRigDocument`, `Zenith.AnimationClipDocument`, `Zenith.AnimationRetargetDocument`), UI documents, font-family documents (`Zenith.UI.FontFamilyDocument`), localization-project documents (`Zenith.UI.LocalizationProjectDocument`), physics documents (`Zenith.PhysicsMaterialDocument`, `Zenith.RagdollDocument`, `Zenith.CollisionShapeDocument`), 2D documents (`Zenith.TwoD.SpriteAtlasDocument`, `Zenith.TwoD.SpriteAnimationDocument`, `Zenith.TwoD.TilesetDocument`, `Zenith.TwoD.TilemapDocument`, `Zenith.TwoD.RigDocument`), navigation documents (`Zenith.NavigationSettingsDocument`, `Zenith.NavigationDocument`), character documents (`Zenith.CharacterAssemblyDocument`, `Zenith.CharacterLipSyncDocument`, `Zenith.CharacterPoseBookDocument`, `Zenith.CharacterControlSetDocument`, `Zenith.CharacterProjectDocument`, registered by `Zenith_RegisterCharacterDocumentTypes`), world documents (`Zenith.World.WorldDocument`, `Zenith.World.GeneratorDocument`, `Zenith.World.LightBakeDocument`, `Zenith.World.LightPageDocument`), import presets (`Zenith.Content.ImportPresetDocument`, `*.importpreset.json`), packaging recipes (`Zenith.Package.RecipeDocument`, `package.recipe.json`), online catalogue documents (`Zenith.Online.CatalogueDocument`, schema tag `zenith2.online`, version 1, an empty migration chain, unknown fields preserved; registered by `Zenith_RegisterOnlineDocumentTypes` in `Zenith2/Tools/Authoring/Zenith_AuthoringOnline.h`; fixture `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Online/CatalogueDocument/v1.json` with `v1.expected.json` beside it), cinematic documents, material definitions, project settings and importer `.meta` settings; a new authoring-document type joins this list in the diff that introduces it. The per-product owner members of a `.meta` sub-asset entry (`exclude` and `materialRemap` of CONTENT, `annotation` and `rig` of ANIM, the ASSET-written `fingerprint`) are part of the `Zenith.AssetMeta` schema and change only through `.meta` version steps, while the top-level `.meta` member `provenance` carries its own `schema` and migrates through a member chain inside the `.meta` type's visitor.
- [Z2-N-CONTRACT-530] Each authoring-document type carries a per-type `uint32` schema version and a migration chain with exactly one step per version increment.
- [Z2-N-CONTRACT-531] Before anything is written, an upgrade preview presents a dry-run diff of every document the migration would change.
- [Z2-N-CONTRACT-532] An automatic backup of every document to be changed is written before the migration writes any document.
- [Z2-N-CONTRACT-533] Every migration write is a journaled atomic write, so an interrupted migration resumes or rolls back cleanly (see "Journaled atomic write").
- [Z2-N-CONTRACT-534] Authored overrides are preserved, keyed by stable authoring IDs (`Zenith_PersistentID` plus property path), never by array index, name or position (see "Override preservation").
- [Z2-N-CONTRACT-535] Unknown fields are preserved on round-trip with a warning and never dropped, so newer tools' data survives (see "Unknown-field preservation").
- [Z2-N-CONTRACT-536] Each authoring-document type has a migration fixture per version (see "Migration fixtures").
<!-- elaboration -->
- [Z2-N-CONTRACT-537] A document whose schema version is newer than the running tool's is opened read-only with a diagnostic and is never written by that tool.

### Migration step interface

<!-- elaboration -->
```cpp
struct Zenith_MigrationContext
{
	const Zenith_PersistentID m_xDocumentID = {};
	const Zenith_TypeID m_xDocumentType = {};
	const uint32 m_uFromVersion = 0;               // the step migrates m_uFromVersion to m_uFromVersion + 1
	Zenith_MigrationDiagnostics* const m_pxDiagnostics = nullptr;
};

using Zenith_PfnMigrationStep = Zenith_Result<void, Zenith_MigrationError> (*)(
	Zenith_DocumentTree& xDocument, const Zenith_MigrationContext& xContext);

// Registered once per (document type, from-version) by the owning library's registration function:
Zenith_Result<void, Zenith_MigrationError> Zenith_RegisterMigrationStep(
	Zenith_MigrationRegistry& xRegistry, Zenith_TypeID xDocumentType, uint32 uFromVersion,
	Zenith_PfnMigrationStep pfnStep);
```

- [Z2-N-CONTRACT-538] A migration step is a pure function over the in-memory document tree: it performs no I/O, reads no global or engine state, and is deterministic, so the preview and the write produce identical output.
- [Z2-N-CONTRACT-539] A chain with a missing step for any version between a document's version and the current version is a registration error reported at tool start, naming the type and the missing version.

### Journaled atomic write

<!-- elaboration -->
| Step | Action | Journal state after the step |
|---|---|---|
| 1 | write `<batch>.z2journal` at the storage root (`Games/<Name>/` for a game's authoring documents, the save directory for a save write), so one batch may span directories and start-up recovery scans one location, listing for each document its path, the SHA-256 of its current bytes, the SHA-256 of its new bytes and its backup path; flush it to disk | `PREPARED` |
| 2 | copy every document to its backup path; flush | `BACKED_UP` |
| 3 | write each new document to `<path>.z2tmp` in the same directory; flush each file | `WRITTEN` |
| 4 | rename each `<path>.z2tmp` over `<path>` atomically (`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` and `MOVEFILE_WRITE_THROUGH` on Windows, `rename` then a directory flush elsewhere), recording each completed entry | `COMMITTING` |
| 5 | delete the journal once every entry is renamed | none |

| Journal state found at start-up | Recovery |
|---|---|
| `PREPARED` or `BACKED_UP` | roll back: delete every `.z2tmp`; the originals are untouched |
| `WRITTEN` | roll forward when every `.z2tmp` matches its recorded new hash; otherwise roll back |
| `COMMITTING` | roll forward the entries not yet renamed, verifying each `.z2tmp` hash; an entry whose target already holds the new hash counts as renamed |

- [Z2-N-CONTRACT-540] Every authoring-document and save write follows the steps of the table "Journaled atomic write", and start-up recovery follows the recovery table; re-running recovery converges without duplicating or losing any entry.
- [Z2-N-CONTRACT-541] Authoring-document backups are written under `Games/<Name>/.migration/backups/<UTC timestamp>/` with the document's game-relative path, and that directory is excluded from source control by the nested `Zenith2/.gitignore`.
- [Z2-N-CONTRACT-553] A save write's step 2 backup is one `<save file>.bak` beside the save, replaced after each successful write, per the slot policy of `Zenith2/Docs/Design/SAVE.md`; saves never write under `Games/<Name>/`.
- [Z2-N-CONTRACT-542] The journal format and the recovery paths are tested by `Test_JournaledWrite_InterruptedAtEachStep_Recovers`, which interrupts after each step, and after each individual rename, and asserts every document is either fully old or fully new.

### Unknown-field preservation

- [Z2-N-CONTRACT-543] The document reader keeps every JSON member it does not recognise, with its path and raw value, and the writer re-emits it at the same position in canonical order.
- [Z2-N-CONTRACT-544] Each unknown field produces one warning `MIGRATION_WARNING_UNKNOWN_FIELD` per document per field path per load, naming the document, the path and the schema version that wrote it.
- [Z2-N-CONTRACT-545] A migration step never deletes an unknown field; removing a field the schema knows is an explicit step operation recorded in the step's name.

### Override preservation

- [Z2-N-CONTRACT-546] An override (a prefab-instance override, a variant override, an importer setting override) is keyed by the target object's `Zenith_PersistentID` and the property's `Zenith_PropertyID` path.
- [Z2-N-CONTRACT-547] A migration step that renames, moves or restructures a property rewrites every override keyed to it in the same step; an override whose target no longer exists is kept, flagged in the preview, and reported, never silently dropped.

### Migration fixtures

- [Z2-N-CONTRACT-548] For every authoring-document type and every schema version from its first shipped version, a fixture document lives under the owning library's `Tests/Fixtures/Migration/<Type>/v<N>.json`, and a test migrates each fixture to the current version and compares the result against `v<N>.expected.json`.
- [Z2-N-CONTRACT-549] A schema version bump adds the new version's fixture and expected output in the same diff; a bump without them fails the owning library's tests.

### Delivery stages of the migration framework

- [Z2-N-CONTRACT-550] The authoring-document migration framework (versioning, chain, preview, backup, journaled write, override and unknown-field preservation) is delivered in Stage 2 by SCENE and ASSET, and complete coverage of every authoring-document type, with the editor's migration UI, in Stage 4 by EDIT.
- [Z2-N-CONTRACT-551] The per-kind compatibility policy is demonstrated with migration fixtures in Stage 6 shipping qualification.

## Enforceability matrix

<!-- informative -->
Automation proves what can be proved; review confirms the rest. Each row names the gate step or test that proves a rule and what the reviewer confirms. Gate lines are `pwsh -NoProfile -File Zenith2\Tools\Gates\<Step>.ps1 …` and are activated by adding them to a Z2 category's `gates` list in `zagent.project.json` (Z2-D-032).
<!-- /informative -->

### Gate steps

| Step | What it runs | Proves |
|---|---|---|
| `Configure` | CMake configure for every preset that applies on the host | presets and globbing configure cleanly |
| `Build -Preset <p>` | a build of a preset of `Build.ps1`'s preset table (`win-debug`, `win-release`, `win-shipping-test` as measure lines; `android-arm64` from Stage 2) | compilation with warnings as errors |
| `Test` | CTest over the built tests | tests, defect tests and examples pass |
| `SourceLayout` | the directory convention over tracked and untracked sources | every source sits in a globbed directory; platform code in `Internal/<Platform>/`; tests in `Tests/`; a declared-but-unavailable library (a `declared_deps.json` record carrying `unavailable`) is reported with its absent entry, not failed |
| `CompileCoverage` | every `.cpp`/`.c` produces an object in each configuration of its target's `ZENITH_SUPPORTED` matrix built this phase | nothing is silently filtered out; a declared-but-unavailable library is reported by name, never filtered out silently |
| `Libgraph` | libgraph over the CMake File API and `declared_deps.json` | the layering DAG, encapsulation, leakage, the link surface, the swap contracts, the surface baseline |
| `Complexity` | `analyze_code_complexity.py` with the Zenith2 profile | complexity, nesting, function and file length, forbidden-token thresholds |
| `ForbiddenCheck` | rules F1 to F26 | forbidden constructs, writable data, exception and RTTI flags, dynamic loading, third-party include confinement, confined OS APIs, the banned exception specifications, attributes, integer spellings and `final` placement, profiled primitives in profiler-reachable code and direct hook-table calls |
| `Format` | pinned `clang-format --dry-run --Werror` | formatting |
| `HeaderSelfContained` | each public header compiled alone | self-contained public headers |
| `ApiContractCheck` | the contract-block parser | a well-formed contract block per public type |
| `TestSeparationCheck` | the seven test-separation conditions | tests are never in shipping targets |
| `ApkCheck` | `llvm-readelf -l` alignment and `zipalign -c -P 16 -v 4`; for every selected module with `Engine/<Lib>/Internal/Android/Java/**`, those classes are present in the APK's dex | 16 KB page compliance; module Java classes packaged |
| `Coverage` | `Coverage.ps1` | backlog coverage, ticket rules, normative grammar |
| `ReviewSignals` | comment ratio and other review signals | report only; fails nothing |
| `ShaderChecks` | shader lints | the GPU performance rules' shader lints |
| `BackendParity` | cross-RHI image and data comparison | Vulkan and D3D12 agree |
| `PathParity` | cross-shading-path comparison | the four shading paths agree within the capability matrix |
| `Sanitizers -Preset <p>` | `win-asan` and `linux-tsan` suites | memory and concurrency defects |
| `SecurityConformance` | the SEC parser inventory, path matrix and update-key checks SC01 to SC16 (`Zenith2/Docs/Design/SEC/ConformanceCheck.md`) | every parser's limits named and tested, every parser fuzzed, the path matrix complete, the update-key record valid |

### Categories and step activation

| Category | Paths | Adds | Activated |
|---|---|---|---|
| `Zenith2` | `Zenith2/**` | the base steps: `Configure`, `Build`, `Test`, `SourceLayout`, `CompileCoverage`, `Libgraph`, `Complexity`, `ForbiddenCheck`, `Format`, `HeaderSelfContained`, `ApiContractCheck`, `TestSeparationCheck`, `Coverage`, `ReviewSignals`; `Build -Preset android-arm64`, `ApkCheck` and `SecurityConformance` from Stage 2 | each step by the PROC ticket that lands it; the base list in Stage 0 (see `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`) |
| `Zenith2Render` | ZenithFlux, ZenithRHI*, `Shaders`, `Tools/ZenithShaderCompiler`, the Cook mesh, texture and material importers | `ShaderChecks`; `BackendParity` from Stage 3; `PathParity` from Stage 5 | by PROC tickets in those stages |
| `Zenith2ECS` | ZenithECS* | the conformance suite built and run against all three backend targets | with the ECS backends in Stage 2 |
| `Zenith2Memory` | the union of the two lists of `Zenith2/Tools/Gates/sanitizer_paths.json` (ZenithCore allocators, containers and threading, the job graph, ZenithScript's VM glue, `Zenith2/Engine/ZenithProfiling/**`, `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/**`, `Zenith2/Tests/DevTools/**`, `Zenith2/Tests/TsanSmoke/**` among them) | `Sanitizers -Preset win-asan` and `linux-tsan`, change-triggered | by the PROC ticket that brings the sanitizer presets |
| `Zenith2Acceptance` | none; selected explicitly by stage acceptance tickets | the full acceptance suites (`-Stage N`) with the full sanitizer set, exempt from the 30-minute cap, excluded from CI's union, run by the human through `Run-Committed.ps1 -Acceptance -Stage N` | per stage |

- [Z2-N-CONTRACT-571] The hard gates are `Libgraph` (cycles, encapsulation, undeclared edges, leakage, the Slang and DXC link surface), `Complexity` (the Zenith2 profile), `ForbiddenCheck` (unsafe allocation and forbidden constructs), `TestSeparationCheck` (shipping and test separation) and `ApiContractCheck` (type-level API contracts); a failure of any of them fails the phase and is never downgraded to a report (Z2-D-042).
- [Z2-N-CONTRACT-572] The gate list is decided by the diff: a ticket runs the union of the gates of every Z2 category whose `paths` its diff touches, and CI runs the union of every Z2 category's active gates except `Zenith2Acceptance` (Z2-D-031, Z2-D-032).
- [Z2-N-CONTRACT-573] Every gate step is self-sufficient and order-independent, writes only under `Zenith2/out/<checkout-hash>/`, and sweeps its own executables scoped to the checkout path.
- [Z2-N-CONTRACT-574] A warm phase run is capped at 30 minutes, with a larger cold cap for a phase that starts from an empty or invalidated build tree, reported in the gate output (Z2-D-028).

### Rule to mechanism matrix

| Rule | Contract section | Proved by automation | Confirmed by review |
|---|---|---|---|
| Private headers, public API boundary, backend swappability | "API boundary" | `Libgraph`: encapsulation, leakage, swap contract, inbound-surface baseline | the justification for any growth in surface |
| Public headers self-contained | "Public headers" | `HeaderSelfContained` | — |
| No `noexcept` or `[[nodiscard]]` without the signpost, the `Zenith_Types.h` integer names, `final` only on a class with a base | "Public headers", "Tests and code hygiene gates" | `ForbiddenCheck` F25 | the reason of each `// ZENITH-ALLOW:` signpost; `[[noreturn]]` placement |
| DAG layering | "Layering DAG" | `Libgraph`: links, includes, symbols, generated code | upward callbacks go through a declared interface |
| No mutable global state | "No mutable global state" | `ForbiddenCheck` F9, F16 (writable-data symbol scan) | no service lookup in disguise |
| No profiled primitive in profiler code, no hook call outside the dispatch functions, no unbounded hook re-entry | "Threading and concurrency contract", "No mutable global state" | `ForbiddenCheck` F26; `Test_HookDispatch_*` and `Test_Mutex_*` tests of ZenithCore | the profiled-versus-unprofiled choice of each new lock outside the scanned scope |
| Function and file complexity, nesting, forbidden tokens | "Tests and code hygiene gates" | `Complexity` (Zenith2 profile), thresholds calibrated from measured distributions, exceptions as profile entries with a decision ID | the exception's rationale; artificial fragmentation rejected |
| Warnings, formatting, narrowing, self-contained headers | "Tests and code hygiene gates" | compiler warnings-as-errors in `Build`, `Format`, `HeaderSelfContained` | explicit narrowing `static_cast`; every ignored result of a fallible call ([Z2-N-CONTRACT-402]) |
| No bare allocation, no smart pointers, no STL in public headers, exceptions only in the firewall, no `lua_call` outside it | "Ownership and allocation", "Containers and standard library policy", "Errors and results" | `ForbiddenCheck` F1–F8, F17 | — |
| No shader compiler in the runtime | "API boundary" | `Libgraph` link surface: no Slang or DXC symbols in any runtime target | — |
| Ownership contracts | "Ownership and allocation" | sanitizer defect tests, the Debug leak side table, generational-handle tests | lifetime reasoning in new owners |
| Container contracts | "Containers and standard library policy" | oracle, property, failure and overflow tests in `Test`; `ApiContractCheck` container fields | the justification record's measurements |
| Handles | "Handle kinds" | the handle tests in every configuration | serialization of handles through persistent IDs |
| Script statelessness | `Zenith2/Docs/Scripting.md` | the D-LUAU-STATELESS (open, Stage 1) load-time rejection | — |
| Transactions and determinism | `Zenith2/Docs/Architecture.md`, `Zenith2/Docs/Scripting.md` | the conformance and fault-matrix tests | new side-effect kinds are routed through the private buffer |
| Contracts per type | "Contract blocks per type" | `ApiContractCheck` (a block exists and is well formed) | the block is accurate |
| Tests never in shipping targets | "Tests and code hygiene gates" | `TestSeparationCheck` | — |
| No layout-changing conditionals | "No layout-changing conditionals" | `ForbiddenCheck` F11, F12 | behavioural differences change no data format |
| Reflection | "Reflection" | registration tests; type-ID collision detection at load | ID stability across renames |
| API review | "Review and change management" | the diff-derived review requirement and `finish` refusal (PROC-API ticket) | the review itself |
| Compatibility | "Compatibility policy per kind" | migration fixtures, journaled-write interruption tests, cooked-version refusal tests | preservation of overrides in new migration steps |
| Normative coverage | this document's tagged bullets | `Coverage` (a consistency check) | the catalogue captures obligations phrased without keywords |
| Evidence freshness | `Zenith2/Docs/Roadmap.md` | acceptance-mode evidence records with input fingerprints | device and visual evidence (`needs-human`) |
| Concurrency contract | "Threading and concurrency contract" | the lock-order checker, `linux-tsan` stress in `Sanitizers` | reentrancy and shutdown ordering |
| Comment ratio | "Contract blocks per type" | `ReviewSignals` (report only) | the whole judgement |
| Parser limits named and tested, every parser fuzzed | `Zenith2/Docs/Architecture.md` → "Security architecture" | `SecurityConformance` over the SEC parser inventory | a new parser's inventory entry |
| Path matrix complete, confined OS APIs | `Zenith2/Docs/Architecture.md` → "Security architecture" | `SecurityConformance` (the path matrix), `ForbiddenCheck` F24 | — |
| Update-key record valid | `Zenith2/Docs/Architecture.md` → "Security architecture" | `SecurityConformance` (the update-key checks) | — |

- [Z2-N-CONTRACT-575] Each rule in the table "Rule to mechanism matrix" is proved by the gate step or test named in its automation column, and the reviewer of a diff touching the rule confirms the item in its review column.

### Review-only obligations

- [Z2-N-CONTRACT-576] Obligations with no automated proof in the matrix (explicit narrowing casts, ignored results of fallible calls, `[[noreturn]]` placement, `const` members of `IMMUTABLE` types, lifetime reasoning of stored pointers, reentrancy and shutdown ordering, callback direction, contract-block accuracy, comment content, artificial fragmentation) are each items of the reviewer conventions and of the "Review focus" list of any ticket that touches them.
