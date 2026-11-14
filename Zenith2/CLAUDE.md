# Zenith2 — binding rules

`Zenith2/**` is governed by this file. It overrides the root `CLAUDE.md` for everything under `Zenith2/`. It adopts the root naming conventions and only the code-style subset listed under "Naming in Zenith Hungarian"; the root rules "All .cpp files must begin with `#include "Zenith.h"`" and "Tools-only code wrapped in `#ifdef ZENITH_TOOLS`" do **not** apply under `Zenith2/` (there is no `Zenith.h`, and rule F11 forbids such conditionals).

## What Zenith2 is

Zenith2 is a clean-room C++20 engine for general-purpose single-player production on Windows (Vulkan and D3D12, chosen at runtime) and Android arm64 (validated on the Galaxy S24 Ultra only). It is built with CMake presets and Ninja Multi-Config, renders through the Flux render graph, scripts gameplay in Luau, and runs three ECS backends. The old engine under `Zenith/` is reference only: read it for prior art, never copy its structure, never include or link it. Games live in `Zenith2/Games/<Name>/`, statically link the engine, and contain no C++.

## Read first

1. `Zenith2/Docs/WorkerGuide.md` — the one page every worker reads before touching code.
2. `Zenith2/Docs/Contracts.md` — the engineering contract; every rule below links to a heading there.
3. The ≤3 doc sections your ticket cites, by path and exact heading. A ticket that depends on a memory mechanism cites the `Zenith2/Docs/Contracts/Memory.md` heading directly; `Contracts.md → "Ownership and allocation"` covers those obligations for coverage, not for reading.

## Layout

```
Zenith2/
  Engine/   ZenithCore ZenithGraph ZenithJobs ZenithProfiling ZenithPlatform ZenithReflect ZenithDocument ZenithVertexFormat
            ZenithCompression (API) + ZenithCompression_Zlib / ZenithCompression_Zstd
            ZenithECS (API) + ZenithECS_Custom / ZenithECS_Flecs / ZenithECS_EnTT
            ZenithScene ZenithAssets ZenithPackage ZenithSave ZenithRHI (API) + ZenithRHI_Vulkan / ZenithRHI_D3D12
            ZenithShadersGenerated (generated shader headers) ZenithFlux ZenithScript
            ZenithScriptCompiler (tool side; never in a Player) ZenithRuntime (composition root; modules sit above it)
            modules: Zenith2D ZenithPhysics (API) + ZenithPhysics_Jolt / ZenithPhysics_PhysX ZenithAnimation ZenithAudio
            ZenithAI ZenithUI ZenithOnline (API) + ZenithOnline_Offline ZenithNet ZenithStreaming ZenithGameplay
            ZenithCinematics ZenithVFX ZenithEnvironment
  Shaders/  CMake/  Tools/{ZenithEditor,ZenithCook,ZenithShaderCompiler,NewGame,NewGameScaffold,ZenithPathTracer,DocumentHost,ContentCore,ZenithContent,Package,Gates (protected),Setup,Backlog}
  Benchmarks/<Harness>/ Benchmarks/Budgets/<AREA>/  Tests/<Suite>/ (integration suites)  Player/ (shared main + Android Gradle template)  ThirdParty/manifest.json
  Games/<Name>/{game.json, CMakeLists.txt (one zenith_add_game call), Scripts/, Scenes/, Schemas/, Assets/}
  Docs/{Architecture,Rendering,Scripting,Contracts,Budgets,Roadmap,DecisionLog,Coverage,WorkerGuide,QualityMatrix,ContentSpecs}.md
  Docs/Architecture/  Docs/Contracts/  Docs/Design/<CODE>.md   Docs/Requirements/{Inventory,Normative,<CODE>}.md   Docs/Research/   Docs/Board/ (Vault records only)
  out/      every build tree, gate report and coverage output (gitignored)
```

Per library: public headers (and their optional public `.inl` companions) in the root, everything else under `Internal/` (platform code in `Internal/Windows|Android|Linux/`), tests under `Tests/`. Contracts.md → "API boundary".

## Naming in Zenith Hungarian

- Zenith Hungarian is retained (Z2-D-034). Types `Zenith_PascalCase`; renderer types `Flux_PascalCase`; functions `PascalCase`; interfaces carry no `I` prefix.
- Scope prefix `m_` member; `g_` global, used by the five allowlisted variables alone (Contracts.md → "Writable-data allowlist entries", [Z2-N-CONTRACT-152]), each preceded by its `// ZENITH-EXEMPTION` declaration comment block ([Z2-N-CONTRACT-578]); `s_` and `ls_` are unused, because runtime code has no mutable statics.
- Type prefix after scope: `x` object, `p` pointer, `px` pointer to class, `a` array, `u` unsigned (`uint`, `uint8`…`uint32`), `ul` `uint64`, `i` signed (`sint`, `sint8`…`sint64`), `c` char (`ac` a char array), `b` bool, `f` float, `str` string, `sz` C string, `e` enum, `pfn` function pointer.
- Integer types are `uint8`, `uint16`, `uint32`, `uint64`, `sint8`, `sint16`, `sint32`, `sint64`, and `uint`/`sint` for the platform's default `unsigned int`/`int` (`Zenith_Types.h`); never `u_int*`, `intN_t` or `uintN_t` outside a third-party boundary in `Internal/`.
- Enums `SCREAMING_SNAKE` with category prefix (`QUEUE_FULL_POLICY_REJECT`); constants `uFOO`, `fFOO`, `ulFOO`.
- Braces on their own line, tabs, `public:` → `protected:` → `private:`, `#pragma once`, no `using namespace` in headers.
- No `noexcept`, no `[[nodiscard]]`, unless a language or standard-library mechanism actually requires it (very rare; say why in a comment); `[[noreturn]]` only on a function that never returns. `final` only on a class that inherits from a base. A type whose contract says `Thread affinity: IMMUTABLE` makes its data members `const` where possible.
- The full prefix table and the adopted style subset are in Contracts.md → "Tests and code hygiene gates" (subsection "Naming and layout"); naming is checked in review, layout by the `Format` gate step.

## Non-negotiables

Each item is enforced by the gate step named, or by review. Breaking one fails the ticket.

1. **Public/Internal boundary.** Root headers are the API; `Internal/` is private to its library and its tests; third-party headers only under `Internal/`. (`Libgraph`) Contracts.md → "API boundary".
2. **Layering is a DAG.** Declare every direct dependency in `zenith_add_library(… DEPENDS …)`; callbacks point downward only. (`Libgraph`) Contracts.md → "Layering DAG".
3. **No mutable global state.** Only the five allowlisted variables (the root heap and profiler hooks installed in `main`, the two thread-local debug and profiler buffers, and the crash handler anchor — Contracts.md → "Writable-data allowlist entries"), each carrying its [Z2-N-CONTRACT-578] comment block; dependencies go in constructors; no service lookup. (`ForbiddenCheck` writable-data scan) Contracts.md → "No mutable global state".
4. **Ownership idioms.** `Zenith_Owned<T>` from `Zenith_New`/`Zenith_TryNew`; arenas through `Zenith_Construct`/`Zenith_Destroy`; pools through `Create`/`Destroy`; shared references are generational handles; borrowing is `T&`/`T*`; no bare `new`/`delete`/`malloc`; no STL smart pointers anywhere; no `Zenith_Ptr`. (`ForbiddenCheck`, sanitizer defect tests) Contracts.md → "Ownership and allocation", Contracts.md → "Handle kinds".
5. **No STL in public headers.** Runtime code includes only the 13 allowed standard headers; tests and tools may use the STL. (`ForbiddenCheck`) Contracts.md → "Containers and standard library policy".
6. **No exceptions outside the Luau firewall.** `try`/`catch`/`throw` and Luau headers only in `ZenithScript/Internal` and `ZenithScriptCompiler/Internal` (compile groups `ZenithScript_EH`, `ZenithScriptCompiler_EH`), `lua_call` only in `ZenithScript/Internal`; recoverable errors are `Zenith_Result<T,E>`; programmer errors are `ZENITH_ASSERT`/`ZENITH_CHECK`; untrusted input and script misuse never assert. (`ForbiddenCheck`) Contracts.md → "Errors and results".
7. **One contract block per public type** (`/// @contract … /// @endcontract`, fields Ownership, Thread affinity, Invalidation, Units, Errors, Version, plus Reentrancy for interfaces and callback-invoking types). (`ApiContractCheck`) Contracts.md → "Contract blocks per type".
8. **Every public header self-contained.** (`HeaderSelfContained`) Contracts.md → "API boundary".
9. **Warnings are errors; clang-format clean; narrowing through `Zenith_NarrowCast`.** (`Build`, `Format`) Contracts.md → "Tests and code hygiene gates".
10. **Tests in `Tests/`, never in shipping targets.** doctest + CTest, `<Lib>_Tests`, pass/fail only, no pinned counts, tests in the same ticket as the code. (`TestSeparationCheck`, `SourceLayout`) Contracts.md → "Tests and code hygiene gates".
11. **No layout-changing `#ifdef`.** Platform code in `Internal/<Platform>/` files or backend targets; debug data in side tables. (`ForbiddenCheck` F11/F12) Contracts.md → "No layout-changing conditionals".
12. **Concurrency.** Locks from the global lock-ordering table; bounded queues with a declared full policy; jobs never block; unregister callbacks before destroying their owner. (lock-order checker, `Sanitizers`) Contracts.md → "Threading and concurrency contract".
13. **One reflection system**, explicit registration, IDs never reused. Contracts.md → "Reflection".
14. **Migrate, never drop.** Authoring documents migrate with preview, backup, journaled write and unknown-field preservation; saves migrate forward; cooked data re-cooks. Contracts.md → "Compatibility policy per kind".
15. **Complexity limits are hard.** Split for coherence, not to pass a limit; an exception is a profile entry with a decision ID. (`Complexity`) Contracts.md → "Tests and code hygiene gates".
16. **Tagged normative bullets.** In Architecture, Rendering, Scripting, Contracts and Budgets every obligation is `- [Z2-N-<DOC>-NNN] …`; rationale goes in `<!-- informative -->` blocks. (`Coverage`) Contracts.md → "Review and change management".
17. **Edit the design doc in the same diff** as any contract, API or design change. A ruling is appended to `Zenith2/Docs/DecisionLog.md` in the ticket's own diff with an explicit `Z2-D-NNN` (Z2-D-097; `Zenith2/Docs/DecisionLog.md → "How this log is written"`); `zagent decide` does not write that file. Contracts.md → "Review and change management".
18. **Contradictions stop the work.** If the ticket, the docs and the code disagree, report it in your result and change nothing that picks a side. Contracts.md → "Purpose and scope".

## Citing docs

- Cite a doc as its repo-relative path plus the exact heading text: `Zenith2/Docs/Contracts.md → "Ownership and allocation"`. `Read` cannot resolve `#anchors`; the heading text is the anchor.
- Cite a normative statement by its ID, e.g. `[Z2-N-CONTRACT-091]`, and a decision by `Z2-D-NNN`. Open decisions are cited by name with "(open, Stage N)", e.g. D-SEL-HEAP (open, Stage 1).
- Never rename a heading a ticket may cite; a rename is its own ticket.

## Board

- The board is the local Vault at `Zenith2/Docs/Board`, project **Z2**. Files there are Vault records only; never hand-edit them.
- Categories (all `project: Z2`, identical settings except `paths`, `gates`, `phaseTimeoutMinutes`): `Zenith2` (every ordinary ticket), `Zenith2Render`, `Zenith2ECS`, `Zenith2Memory` (these three add gates when a diff touches their paths), `Zenith2Acceptance` (stage acceptance tickets, run by a human).
- Branching is `direct` on the Z2 lane; one Z2 ticket runs at a time.
- Ticket bodies carry ten sections; `Zenith2/Docs/WorkerGuide.md` explains what each means to you.

## Gate steps

Every line is `pwsh -NoProfile -File Zenith2\Tools\Gates\<Step>.ps1 …`; steps write only under `Zenith2/out/<checkout-hash>/`.
`Configure` · `Build -Preset win-debug|win-release|android-arm64` · `Test` · `SourceLayout` · `CompileCoverage` · `Libgraph` · `Complexity` · `ForbiddenCheck` · `Format` · `HeaderSelfContained` · `ApiContractCheck` · `TestSeparationCheck` · `ApkCheck` · `Coverage` · `ReviewSignals` (report only) · `ShaderChecks` · `BackendParity` · `PathParity` · `Sanitizers -Preset win-asan|linux-tsan`.
Which run is decided by the diff's paths; the full mapping is Contracts.md → "Enforceability matrix". You cannot run them: write code that passes them.

## Protected paths

`Zenith2/Tools/Gates/**` and this file are protected. Editing them is PROC `needs-human` work; an ordinary ticket may cite a gate step by name but never edits it.
