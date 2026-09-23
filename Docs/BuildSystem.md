# The Zenith Build System

The single comprehensive reference for how Zenith is generated, built, tested,
packaged, and gated. Companion documents:

- **[GameProjects.md](GameProjects.md)** — the `.zproj` descriptor schema, codegen,
  manifest guard, solution inventory, templates, and the hub. Read that for
  "how do I add/describe a game"; read this for everything else.
- **`Tools/ZenithCli/CLAUDE.md`** — CLI module internals (for working ON the CLI).
- Root `CLAUDE.md` — the quick-start subset of this document.

Sharpmake (checked in at `../Sharpmake`) is the generator. **CMake is not used
and must not be introduced.**

## 1. The moving parts

```
Games/<Name>/<Name>.zproj          descriptors (the ONLY per-game build input)
        |
        v
Build/regen.ps1                    canonical regeneration (zenith regen forwards here)
  ├─ Build/zenith_buildsystem.psm1 scan/validate/codegen/name-rules/shared ops
  ├─ Build/Sharpmake_*.cs          hand-written projects + generated per-game shells
  └─ Build/fix_agde_vcxproj.ps1    AGDE post-fixup (clang c++ standard token)
        |
        v
Games/<Name>/<name>_win64.sln      per-game solutions (+ _agde.sln when android:true)
Build/zenith_engine_win64.sln      engine-only solution (zero games)
        |
        v
zenith build / test / run / package / clean      (zenith.bat -> Tools/zenith.ps1
                                                  -> Tools/ZenithCli/ZenithCli.psm1)
```

Central configuration data lives in **`Build/zenith_config.psd1`** (default and
hub configs, Slang/Vulkan SDK versions CI provisions, the artifact root) and is
read ONLY via accessors in `zenith_buildsystem.psm1` (`Get-ZenithBuildConfigData`,
`Get-ZenithDefaultConfig`, `ConvertTo-ZenithOutputDir`, `Get-ZenithGameExePath`).
Never hardcode a config name or exe path in a script — call the accessor.

## 2. The `zenith` CLI — command reference

`zenith.bat` (repo root) → `Tools/zenith.ps1` → `Tools/ZenithCli/ZenithCli.psm1`.
This is THE entry point for every build-system operation; there are no
alternative/forwarder scripts (no-legacy rule, §11).

```
zenith new <Name> [--template <T>] [--no-android] [--no-open]
zenith open <Name>
zenith list [--json]
zenith regen [--check]
zenith build <Name|engine> [--config <C>] [--headless] [--whole] [--timeout <min>]
zenith run <Name> [--config <C>] [--build] [-- <game args>]
zenith test <Name|all> [--filter X] [--tier N] [--config <C>] [--headless]
            [--per-process] [--fail-fast] [--build] [--results-dir D]
            [--tests A,B,C] [--batch-order reverse|rotate:<N>]
            [--exit-after-frames N] [--assertions-log F]
zenith clean [<Name>|engine|all] [--processes-only] [--dry-run]
zenith package <Name> [--config <C>] [--out <D>] [--force] [--no-shaders]
zenith hub [--rebuild]
zenith selftest
```

**Exit codes (all commands):** `0` ok · `1` usage · `2` validation · `3`
generation/drift · `4` build/test failure · `5` not found.

Command notes beyond the obvious:

- **`build`** resolves MSBuild via PATH then vswhere (Insiders/prerelease
  included); kills compiler processes older than 30 minutes on entry (hung-lock
  self-heal — a live concurrent build is never touched); `--timeout` arms a
  watchdog that taskkills the msbuild tree. `--whole` builds the target's sln
  with no `/t:` — every project the sln maps to `Build.0` for that config,
  including a game's own `extraSharpmakeProjects` (e.g. TilePuzzle's offline
  tools), which a scoped `/t:<Game>` never reaches (ZEN-111 / ZEN-D-044, §9).
  `build engine` builds `Build/zenith_engine_win64.sln` WHOLE in the requested
  config (this replaced a scoped `/t:Zenith`, which never linked the sln's own
  exe projects — see §9), plus the seven Sentinels via `/t:` against its
  `_False` sibling config (Sentinels exist only in `ToolsEnabled=False`; see
  §4) — `--whole` has no effect on `build engine`, which already builds its
  sln whole unconditionally.
- **`regen --check`** is a read-only staleness report: recomputes the codegen
  text from the descriptors and byte-compares against the on-disk generated
  `.cs`, then verifies every expected `.sln` exists. Exit 3 = run `zenith regen`.
  This is a LOCAL tool — CI always runs a full regen (a fresh checkout has no
  generated files, so a check-only step would always report drift).
- **`test`** — see §5.
- **`clean`** always sweeps hanging `cl`/`mspdbsrv`/`link`/`vctip`/`msbuild`
  first, then (with a target) deletes that target's `output/` + `obj/` trees.
- **`package`** — see §7.
- **`selftest`** runs `Build/Tests/run_buildsystem_tests.ps1` +
  `Tools/ZenithCli/Tests/run_cli_tests.ps1` (77 tests; the buildsystem suite
  includes the tracking-policy regression tests from §6).

## 3. Regeneration (regenerate-first)

`Build/regen.ps1` does, in order: worktree refusal → descriptor validation
(ALL errors collected) → codegen `Sharpmake_GameInstances.generated.cs` → one
Sharpmake run over the `Build/Sharpmake_*.cs` glob → AGDE vcxproj fixup →
obsolete-monolith deletion → **orphan prune** (`Remove-ZenithOrphanGameArtifacts`:
generated slns/vcxprojs of game dirs with no descriptor, and stale agde
artifacts of `android:false` games — never sources, never directories) →
**engine-level orphan prune** (`Remove-ZenithOrphanEngineArtifacts`: a
`Build/*.vcxproj` + its `.filters`/`.user` that the run just completed did NOT
generate — e.g. a deleted `Sharpmake_Zenith<Name>.cs` whose
`zenith<name>_{win64,agde}.vcxproj*` would otherwise linger under `Build/`.
Scope is `Build/*.vcxproj*` only — never `Games/**`, never the hand-written
`Sharpmake_*.cs` sources. "Known" is every `Name = "..."` project/solution-name
assignment parsed out of the hand-written `Build/Sharpmake_*.cs` files
(excluding the generated `Sharpmake_GameInstances.generated.cs`, whose
game project/solution classes live under `Games/<Name>/`, never `Build/`); a
`Build/*.vcxproj` whose basename doesn't match `<known-name>_<win64|agde>` was
not emitted this run and is deleted. This closed a real gap: when `ZenithBase`
was dissolved (Phase 4a), stale `Build/zenithbase_{win64,agde}.vcxproj[.filters/.user]`
and `Build/obj/ZenithBase/` lingered after a regen and had to be deleted by
hand — `Tools/check_project_membership.ps1` globs every `.vcxproj` on disk, so
a stale one is silently parsed as if it were still real) → sln inventory
print. Details of validation/codegen/manifest-guard:
[GameProjects.md](GameProjects.md).

**Regenerate-first policy: nothing Sharpmake emits is git-tracked** — every
`.sln`, `.vcxproj`, `.filters`, `.vcxproj.user`, and the generated `.cs`.
Consequences:

- After a fresh clone, or any checkout/pull touching a `.zproj` or
  `Sharpmake_*.cs`: run `zenith regen` before building. `zenith regen --check`
  tells you whether you need to.
- Branch switches never produce generated-file churn in `git status`.

**Worktree ban:** Sharpmake bakes ABSOLUTE paths (asset dirs, shader root,
include paths) resolved from its own location, so running it from a linked git
worktree can produce incorrect roots. `regen.ps1` refuses by default (exit 2).
For an explicitly authorized linked checkout, `Build/regen.ps1 -AllowLinkedWorktree`
checks the Git/script root and verifies every generated game root before succeeding.

**Registered-lane exception.** A [lane](../Tools/zagent/README.md#lanes)
is an explicitly authorized linked worktree by construction: `zagent lane
add <PROJECT>` provisions it via `Tools/lane_init.ps1`, which itself runs
`Build\regen.ps1 -AllowLinkedWorktree` as part of setup, and every later
regen inside that worktree (an auto-regen included, see below) passes the
same flag. The refusal still fires for an ordinary, *unregistered* linked
worktree — `zenith build` there fails its first build with a message
naming the remedy (`run 'Build\regen.ps1 -AllowLinkedWorktree' once, or
'zagent lane add'`), keyed on whether the worktree is a **registered**
lane, not merely a linked one.

**Regen staleness is tracked per checkout, and a stale tree auto-regens.**
`Build/artifacts/regen_sha` (gitignored) is written ONLY by a successful
`regen.ps1`, as its last step: line 1 is HEAD's commit, the remaining
lines are the current set of untracked *project-shaping* files (any
`.cpp`/`.c`/`.h`/`.inl`/`.slang` outside `Middleware/`, or a
`Build/Sharpmake_*.cs`/`Games/*/*.zproj`). `zenith build` and `zenith hub`
compare this against HEAD before every build
(`Get-ZenithBuildRegenStaleness` → `Test-ZenithRegenStale`,
`Build/zenith_buildsystem.psm1`) and call `Build/regen.ps1` automatically
when the tree is stale — missing/empty/unresolvable `regen_sha`; an
add/delete/rename of a project-shaping file, or ANY status change on a
Sharpmake/`.zproj` file, between the recorded commit and HEAD; or a NEW
untracked project-shaping file since the last regen. A plain edit (`M`)
to an existing `.cpp`/`.h`/etc. does **not** count — Sharpmake already
knows the file; only its contents changed. If a `devenv.exe` process has
this repo open at the moment an auto-regen fires, a best-effort check
prints a warning that it may need to reload the solution afterwards.
`zenith package` never auto-regens (see §7) — it only warns.

## 4. Configurations, outputs, and the library structure

Config axes (fragments): **RenderBackend** (`Vulkan_` = real renderer /
`Null_` = the GPU-less backend every headless run executes on / `D3D12_` = a
reserved no-op backend kept as the link-neutrality proof) × **vs2022** ×
**Optimization** (`Debug`/`Release`) × **Win64/Agde** × **ToolsEnabled**
(`True` = editor/tools, `False` = runtime-only). Example:
`Vulkan_vs2022_Debug_Win64_True`.

**agde also carries the `Vulkan_` prefix, plus an ABI fragment.** agde is
Vulkan-only, but Sharpmake emits the backend fragment anyway because it is
multi-valued across the target set as a whole, and it adds the Android ABI:

| agde config | MSBuild Platform |
|-------------|------------------|
| `Vulkan_arm64_v8a_vs2022_Debug_Agde_False` | `Android-arm64-v8a` |
| `Vulkan_arm64_v8a_vs2022_Release_Agde_False` | `Android-arm64-v8a` |
| `Vulkan_x86_64_vs2022_Debug_Agde_False` | `Android-x86_64` |
| `Vulkan_x86_64_vs2022_Release_Agde_False` | `Android-x86_64` |

The ABI axis is `ZenithAndroidAbi.All` in `Sharpmake_Common.cs` — one list every
agde project reads. It has **three** mirrors, because C#, Gradle and PowerShell
cannot share a literal, and widening the axis means editing all three:

| Mirror | Read by | Orientation |
|---|---|---|
| `ZenithAndroidAbi` (`Sharpmake_Common.cs`) | every agde project | `ConfigToken()` / `DirName()` |
| `Build/zenith_android_abis.gradle` | every game's `app/build.gradle` | dir name → token |
| `AndroidAbis` (`Build/zenith_config.psd1`) | all PowerShell tooling, via `Get-ZenithAndroidAbis` | token → dir name (the **transpose** of the Gradle map) |

A buildsystem selftest compares all three and fails on drift; read the psd1 one
through the accessor, which returns both spellings as named fields.
`arm64-v8a` is what physical devices run; **`x86_64` is what the Android
emulator runs**, because the QEMU2 emulator cannot host an arm64 guest on an
x86_64 host at all. A dev box with no ARM device can only exercise Android
through x86_64. Gradle merges every built ABI into one APK, so an APK contains
whichever ABIs you actually built.

Mind the two spellings: the config name uses `arm64_v8a` (underscores) while the
on-disk ABI directory is `arm64-v8a` (dash). They coincide for `x86_64`, which
is exactly what makes hardcoding either spelling silently wrong — see
`ZenithAndroidAbi.ConfigToken` vs `.DirName`.

> **Headless is a build config, not a flag.** There is no `--headless` engine
> flag. A `Null_*` config defines `ZENITH_NULL_RENDERER`, compiles `Zenith/RenderBackends/Null`
> in place of the Vulkan backend, and creates the window hidden. Crucially every
> render path still RUNS (pass callbacks, buffer uploads, the editor ImGui frame)
> against no-op backend calls, so a headless run exercises the same code as a
> windowed one instead of skipping it behind a runtime branch. `--headless` on
> `zenith build` / `zenith test` selects this config
> (`zenith_config.psd1: HeadlessConfigWin64`). In C++, query it with the
> constexpr `Zenith_IsNullRenderer()`.

**Case rule:** `/p:Configuration=` takes the PascalCase name; Sharpmake
LOWERCASES it to form the output directory leaf. The one place this fact lives
in code is `ConvertTo-ZenithOutputDir`; never `.ToLowerInvariant()` a config
name at a call site.

Layout:

| What | Where |
|------|-------|
| Game exe | `Games/<Name>/Build/output/win64/<lowercase config>/<name>.exe` |
| Game intermediates | `Games/<Name>/Build/obj/…` |
| Engine libs | `Build/output/win64/<lowercase config>/*.lib` |
| Engine/leaf intermediates | `Build/obj/<Lib>/…` (distinct per lib — PCH collision guard) — one subdir per project: `ZenithCore`, `ZenithMaths`, `ZenithDataStream`, `ZenithFileAccess`, `ZenithDebugVariables`, `ZenithProfiling`, `ZenithTaskSystem`, `ZenithInput`, `ZenithWindows`, `ZenithAndroid`, `ZenithUnitTests`, `ZenithECS`, `ZenithSaveData`, `ZenithTelemetry`, `ZenithPrefab`, `ZenithScripting`, `ZenithJolt`, `ZenithPhysics`, `ZenithAI`, `ZenithImGui`, `ZenithAssetRegistry`, `ZenithAssetHandling`, `ZenithFlux`, `ZenithRenderBackendVulkan`, `ZenithRenderBackendD3D12`, `ZenithRenderBackendNull`, `ZenithUI`, `ZenithEntityComponent`, `ZenithEditor`, `ZenithTools` |

**Library structure (Phase 4a — `ZenithBase` dissolved):** the old `ZenithBase`
was ONE bottom-leaf library spanning five directories (`Maths/`,
`Collections/`, `DataStream/`, `FileAccess/`, and a subtractive-regex slice of
`Core/`), held together only by a negative-lookahead regex over `Core`'s
top-level `.cpp` files — additive-glob-minus-subtractive-regex, global
membership that let a new file under an already-extracted directory silently
compile into two libraries. It is now FOUR single-directory leaf libraries,
each globbed additively by exactly one project:

- `ZenithCore` (`Zenith/Core` WHOLE — every top-level `.cpp` and every
  subdirectory: `Callstack/`, `Memory/`, `Multithreading/`, no exclusion regex
  — plus `Zenith/Collections`' headers, which have zero `.cpp` of their own
  and join purely for IDE-browsing ownership). ~15 TUs; compiles the shared
  `Zenith.pch` (`Zenith.cpp` is the Create TU).
- `ZenithMaths` (`Zenith/Maths`, 1 TU), `ZenithDataStream`
  (`Zenith/DataStream`, 2 TUs), `ZenithFileAccess` (top-level
  `Zenith/FileAccess` only — NOT `Zenith/Windows/FileAccess`, the platform
  impl, which stays in the aggregate — 1 TU). None of these three carries a
  PCH (see the rule below); each compiles `Zenith.h` textually.

**Library structure (Phase 4b — seven more leaves):** `ZenithDebugVariables`
(`Zenith/DebugVariables`, 1 TU), `ZenithProfiling` (`Zenith/Profiling`, 1 TU;
depends on `ZenithDebugVariables` — the profiler exposes debug-variable
toggles), `ZenithTaskSystem` (`Zenith/TaskSystem`, 1 TU; also depends on
`ZenithDebugVariables`), `ZenithInput` (`Zenith/Input`, 4 TUs; gets its own
PCH — see the ≥4-TU rule below), `ZenithWindows` (`Zenith/Windows`'s 9 TUs +
the `imgui_impl_glfw.cpp`/`imgui_impl_win32.cpp` ImGui backend pair it now
OWNS = 11 TUs; PCH; **win64 ONLY**; depends on `ZenithInput` + `ZenithProfiling`;
OWNS glfw — see below), `ZenithAndroid` (`Zenith/Android`'s 8 `.cpp` +
`NativeGlue/android_native_app_glue.c` = 9 TUs; PCH; **agde ONLY**; depends on
`ZenithFileAccess` + `ZenithProfiling` + `ZenithInput`), `ZenithUnitTests`
(`Zenith/UnitTests`, 1 TU — `Zenith_AssertCapture.cpp` — after
`Zenith_EditorTestFixture.{h,cpp}` moved to `Zenith/Editor/`, the only
directory that consumes it, leaving this a clean leaf with no
Editor/EntityComponent/Input header edge).

`ZenithWindows` and `ZenithAndroid` are the first two entries in
`Build/Sharpmake_EngineLibs.cs`'s table to restrict their own target set (no
`agde`/`win64` `AddTargets` call respectively) — every call site that adds a
project unconditionally (`conf.AddProject`/`conf.AddPublicDependency`) throws
if the target being configured is not one the callee itself declared, so both
are wired in with an explicit `Func<ZenithTarget,bool>` predicate
(`t => t.Platform == Platform.win64` / `.agde`) instead of a hand-written `if`
at each of the three consumer call sites.

**`ZenithWindows` owns GLFW.** GLFW is called directly from FOUR directories:
`Zenith/Windows/` (now including the two ImGui backend TUs it owns),
`Zenith/Input/`, the Vulkan render backend (still aggregate-owned), and
formerly `Zenith_Core.cpp` (moved to `Zenith/EngineComposition/` earlier,
leaving `ZenithCore` itself clean of GLFW calls). Only `ZenithWindows` calls
`AddGlfwLibs`. This links correctly at every game exe (the aggregate every
game links always carries `ZenithWindows` transitively), but leaves
`ZenithInput` and the Vulkan backend as libraries with undeclared win64 GLFW
symbols of their own — accepted, not a bug, this phase. A future `ZenithInput`
or Vulkan-backend Sentinel would need `ZenithWindows` added to its own link
set to prove either leaf standalone.

**The two ImGui platform-backend TUs**
(`Middleware/imgui-docking/backends/imgui_impl_glfw.cpp` /
`imgui_impl_win32.cpp`) are now members of `ZenithWindows` instead of being
globbed by the aggregate and per-configuration build-excluded on `agde`. Two of
the aggregate's four `agde`-only ImGui build-excludes became dead as a result
(glfw/win32 — deleted) while two stayed live (`imgui_impl_android`, which
compiles nowhere at all today, and `imgui_impl_vulkan`, still owned by the
aggregate/Vulkan-backend).

**Library structure (Phase 4c — five more, including the Jolt split):**
`ZenithSaveData` (`Zenith/SaveData`, 1 TU; depends on `ZenithDataStream` +
`ZenithFileAccess`), `ZenithTelemetry` (`Zenith/Telemetry`, 1 TU; depends on
`ZenithDataStream` + `ZenithMaths` + `ZenithECS` — a genuine extracted-library
edge, not include-path-only: it records entity IDs), `ZenithPrefab`
(`Zenith/Prefab`, 1 TU; depends on `ZenithDataStream` + `ZenithMaths` +
`ZenithECS` + `ZenithProfiling`; its `AssetHandling`/`EntityComponent`
`#include`s stay include-path-only — neither is its own library yet, both
still compile into the aggregate), `ZenithScripting` (`Zenith/Scripting`'s 5
TUs + its own PCH create TU = 6; PCH; depends on `ZenithDataStream` +
`ZenithMaths` + `ZenithECS`). None of the first three carries a PCH (below the
≥4-TU threshold); `ZenithScripting` clears it and CREATEs its own from
`Internal/Zenith_ScriptingPCH.cpp` (definition-free, same AGDE/clang rationale
as `ZenithInput`/`ZenithWindows`/`ZenithAndroid`).

**The Jolt split.** The vendored `Middleware/JoltPhysics-5.4.0` tree —
previously compiled AS PART OF `ZenithPhysicsLibProject` via
`AdditionalSourceRootPaths` — is now its own leaf library, `ZenithJolt`
(133 TUs in scope, the same nine `Build`/`Docs`/`UnitTests`/`Samples`/
`TestFramework`/`PerformanceTest`/`JoltViewer`/`HelloWorld`/`cmake`
vendored-tree membership excludes moved verbatim from `ZenithPhysics`).
`ZenithJolt` carries **no PCH at all** (not a TU-count judgement call — Jolt's
sources never `#include "Zenith.h"`, so there is no shared master header for
them to share) and **no Sharpmake dependency of its own**: the mandatory
`ZenithCore` edge every other library in this table carries is justified by
every PCH-consuming TU referencing the inline `Zenith_LogImpl` →
`Zenith_LogFileSinkWrite`, and that justification does not hold for a library
whose TUs never see the PCH. `ZenithPhysics` shrinks to its own 4 TUs (3
Physics `.cpp` + its PCH create TU — exactly at the ≥4-TU PCH threshold) and
gains a public dependency on `ZenithJolt`, so every `JPH::` symbol it calls
out-of-line still resolves transitively at any consuming link.
`SentinelPhysics` — the leaf-purity proof exe for `ZenithPhysics` — gained a
fourth `AddPublicDependency<ZenithJoltLibProject>` alongside its existing
three, and now proves a *stricter* claim than before the split: previously a
missing engine edge was the only way this link could fail; now a missing
`ZenithJolt` edge would fail it too, in exactly the same "declared leaf set,
nothing hidden" way.

**Library structure (Phase 4d — three more, including the Flux/AssetHandling
split and the imgui-core split):** `ZenithImGui` (`Middleware/imgui-docking`'s
5 top-level TUs — `imgui.cpp`/`imgui_demo.cpp`/`imgui_draw.cpp`/
`imgui_tables.cpp`/`imgui_widgets.cpp` — confirmed against what the aggregate
compiled before this split; win64 + agde). It is the SECOND library in this
table (after `ZenithJolt`) to carry no PCH and no Sharpmake dependency at
all — imgui's core TUs never `#include "Zenith.h"`, so there is no shared
master header and no honest `ZenithCore` edge to declare, exactly `ZenithJolt`'s
reasoning. None of `backends/` belongs to it: `imgui_impl_glfw.cpp` /
`imgui_impl_win32.cpp` stay `ZenithWindows`'s (Phase 4b), `imgui_impl_vulkan.cpp`
stays aggregate-owned pending the not-yet-extracted Vulkan-backend library, and
`imgui_impl_android.cpp` is compiled by nothing, as before.

`ZenithAssetHandling` (`Zenith/AssetHandling`, 21 TUs; PCH, Create TU
`Internal/Zenith_AssetHandlingPCH.cpp`; depends on `ZenithMaths` +
`ZenithDataStream` + `ZenithScripting`) and `ZenithFlux`
(`Zenith/Flux`, 84 TUs; PCH, Create TU `Internal/Zenith_FluxPCH.cpp`; depends
on `ZenithMaths` + `ZenithDataStream` + `ZenithFileAccess` + `ZenithProfiling`
+ `ZenithDebugVariables` + `ZenithTaskSystem` + `ZenithECS` +
`ZenithAssetHandling`) split a GENUINE header cycle between the two — 23
`Flux/**` files `#include` an `AssetHandling/**` header against 11 the other
way. The cycle is declared, not broken: `ZenithFlux` depends on
`ZenithAssetHandling` (the heavier direction); `ZenithAssetHandling` does
**not** depend back, so the two reverse includes resolve only via the plain
`/Zenith` include path every leaf carries, and any out-of-line `Flux_::`
symbol they call resolves at the final exe link (a static lib never resolves
an external symbol at its own build step). Moving files to break the cycle was
tried on paper and rejected: `Flux_AnimatorControllerDef.h` /
`Flux_GrassTypeTable.h` look like leaf data `AssetHandling` reaches into, but
each pulls in two-to-three more `Flux/**` headers, so relocating them alone
either drags the rest across or leaves the edge in place either way. The real
fix — splitting `AssetHandling` into a handle/registry half and a
GPU-resource half — is a design change for a future ticket.
**★ SUPERSEDED BY PHASE 4i, below** — that future ticket happened: the cycle
is broken now, `ZenithAssetHandling` is 9 TUs rather than 21, and the
`ZenithFlux` edge points at the new `ZenithAssetRegistry` instead. The
paragraph above is kept as the record of what Phase 4d measured and why it
declared rather than broke; read the Phase 4i entry for the current shape.
`ZenithFlux`
additionally owns `slang.lib`, conditionally on `win64 && RenderBackend.Vulkan`
only (the one Flux TU that calls the slang API,
`Flux/Slang/Flux_SlangCompiler.cpp`, is itself guarded by
`#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)`; the `Null`/`D3D12`
configs link no slang lib at all, matching `ConfigureCommonLibraryPaths`'s
historical behaviour). Both `ZenithAssetHandling` and `ZenithFlux` needed
`SHADER_SOURCE_ROOT` defined directly (not just reachable through the PCH) —
several TUs in each (`Zenith_TextureAsset.cpp` via `Flux_GraphicsImpl.h` →
the active backend's Pipeline header; `Flux.cpp`/`Flux_ShaderHotReload.cpp`/
`Flux_SlangCompiler.cpp` directly) reference it unguarded, the same defect
class `ZenithDebugVariables` (Phase 4b) already hit; `ZenithFlux` also needed
`ENGINE_ASSETS_DIR` (`Flux_Particles.cpp`).

**Library structure (Phase 4e — the render-backend split, this phase's
headline):** `Zenith/RenderBackends/` becomes THREE target-restricted leaf
libraries instead of one library that globs all three backends and
build-excludes two — that would reintroduce exactly the per-configuration
subtraction mechanism this refactor exists to remove. `ZenithRenderBackendVulkan`
(`Zenith/RenderBackends/Vulkan`'s 17 TUs + the owned `imgui_impl_vulkan.cpp`
ImGui backend TU + its own PCH create TU = 19; PCH; `RenderBackend.Vulkan`
ONLY, but BOTH win64 and agde, since agde is always Vulkan; depends on
`ZenithCore` + `ZenithFlux` + `ZenithDebugVariables` + `ZenithTaskSystem`),
`ZenithRenderBackendD3D12` (`Zenith/RenderBackends/D3D12`'s 3 TUs + its own
PCH create TU = 4, exactly at the ≥4-TU PCH threshold; `RenderBackend.D3D12`,
**win64 ONLY**; depends on `ZenithCore` + `ZenithFlux`), `ZenithRenderBackendNull`
(`Zenith/RenderBackends/Null`'s 3 TUs + its own PCH create TU = 4; `RenderBackend.Null`,
**win64 ONLY**; depends on `ZenithCore` + `ZenithFlux`).

**Conditionality moved from a per-configuration source exclude to project
membership + solution wiring.** Each library declares ONLY the `RenderBackend`
fragment(s) it supports via its own `AddTargets`; `conf.AddProject`/
`conf.AddPublicDependency` THROW for a target the callee itself never
declared, so every one of the three is wired into
`Build/Sharpmake_EngineLibs.cs`'s table with a `Func<ZenithTarget,bool>`
predicate (`t => t.RenderBackend == RenderBackend.Vulkan` for the Vulkan
library — true for win64 AND agde; the other two additionally require
`t.Platform == Platform.win64`, since neither D3D12 nor Null is an agde
config at all). The whole render-backend `SourceFilesBuildExcludeRegex` block
that used to live in `ConfigurePlatformExcludes`
(`Build/Sharpmake_Common.cs` — three per-file `\Vulkan\`/`\D3D12\`/`\Null\`
path-segment excludes plus an `imgui_impl_vulkan` filename exclude on the two
inactive backends) is DELETED; a `Null_*` config simply has no Vulkan or
D3D12 *project* in its solution, so there is no source file to exclude — a
config with no such project at all still links (verified: `zenith build
Combat --headless`). `SourceFilesBuildExcludeRegex` occurrences across
`Build/Sharpmake_*.cs` dropped from 24 to 17 as a result.

**The platform arm of `ConfigurePlatformExcludes` (win64 excludes `\Android\`;
agde excludes `\Windows\`/`\Editor\`/`\Tools\`) was load-bearing for
`ZenithRenderBackendVulkan` specifically** at the time this section was
written: that library spanned both win64 and agde and its source tree
contained `Platform/Windows/Zenith_Windows_Vulkan.cpp` and
`Platform/Android/Zenith_Android_Vulkan.cpp` side by side, so a single
project needed the platform partition WITHIN itself. Whether to split
`Platform/Windows/` and `Platform/Android/` into their own target-restricted
sub-libraries, or keep this intra-project partition, was left to a future
phase — **Phase 4h.3 did the split; see §4's own Phase 4h entry below.**
`ConfigurePlatformExcludes` no longer exists at all.
**CORRECTION, found by Phase 4h's measurement:** the claim that this
mechanism "also still applies, unchanged, to every other multi-platform
project in the table (`ZenithFlux`, `ZenithImGui`, `ZenithAssetHandling`,
the aggregate itself)" was never re-verified after it was first written and
turned out to be **false** for all four — none of those four projects' own
generated `.vcxproj` ever contained a single member matching `\Android\`,
`_Android`, `\Windows\` or `_Windows`; the call was defensive dead weight in
every one of them. See the Phase 4h entry for the full measurement.

**THE Flux <-> RenderBackends cycle is declared `backend -> Flux`** — the
backend implements the Flux concepts (~41 unique header edges that direction
across the three backends combined); Flux's only reverse edges are the 3
concrete-type-alias includes in `Flux/Flux_BackendTypes.h`, which resolve
purely by include path (`RootPath + "/Zenith/RenderBackends"`, added by
`ZenithFlux`'s own `ConfigureAll`), not by a Sharpmake dependency edge — so
`ZenithFlux` does not depend back on any of the three backends, mirroring
exactly how Phase 4d declared the `ZenithFlux <-> ZenithAssetHandling` cycle
one-way. This relies on `ZENITH_VULKAN`/`ZENITH_D3D12`/`ZENITH_NULL_RENDERER`
coming from `ConfigureCommonSettings` (the explicit three-way `if`/`else
if`/`else throw` on `target.RenderBackend`) rather than from the dependency
edge — unchanged by this phase, still the sole definer.

**GLFW stays undeclared here too**, the same acceptance `ZenithInput` already
established (Phase 4b): all three backend libraries call `glfwXxx(...)`
and/or read `Zenith_Window::GetInstance()` (owned by `ZenithWindows`)
directly, but none of the three calls `AddGlfwLibs` or declares a
`ZenithWindows` dependency — the composition root every actual exe links
always carries `ZenithWindows` on win64 regardless. `ZenithRenderBackendVulkan`
DOES call `AddVulkanBackendLibs` (win64-only, matching the helper's own
guard — agde links its system Vulkan loader via `--sysroot`) but not
`AddSlangLibs` (`ZenithFlux` owns `slang.lib`); `ZenithRenderBackendD3D12`
calls `AddD3D12Libs`; `ZenithRenderBackendNull` calls **none** of the four
library helpers — the structural guarantee that a GPU-less backend cannot
acquire a graphics import library by accident.

**The duplicate-owned fix.** `ZenithHubProject` and
`TilePuzzleRegistryViewerProject` each used to glob
`Middleware/imgui-docking` (the core TUs) AND `imgui_impl_win32.cpp`
themselves, ON TOP OF `AddPublicDependency<ZenithProject>` — which already
linked `zenith.lib` carrying the SAME imgui core TUs (and, transitively,
`zenithwindows.lib` carrying the same `imgui_impl_win32.cpp`). Same-executable
duplication, hidden only by first-definition-wins at link:
`Tools/check_project_membership.ps1` reported exactly 40 DUPLICATE findings
(10 (source, exe) pairs × 4 configs) before this phase and 0 after. Both
projects now own only their DX11 backend TU (`imgui_impl_dx11.cpp`) and
declare an explicit `AddPublicDependency<ZenithImGuiLibProject>` for
legibility, even though `ZenithProject` already re-exports it.

**Library structure (Phase 4f — the two top-of-the-DAG leaves):** the
densest, highest-fan-in pair in the engine finally split out of the
aggregate. `ZenithUI` (`Zenith/UI`'s 14 TUs + its own PCH create TU = 15;
PCH; depends on `ZenithCore` + `ZenithMaths` + `ZenithDataStream` +
`ZenithAssetHandling` + `ZenithFlux` + `ZenithInput` + `ZenithProfiling`) and
`ZenithEntityComponent` (`Zenith/EntityComponent`'s 43 TUs across
`Components/` + the top-level ECS↔engine glue, + its own PCH create TU = 44;
PCH; depends on `ZenithCore` + `ZenithMaths` + `ZenithDataStream` +
`ZenithFileAccess` + `ZenithProfiling` + `ZenithDebugVariables` +
`ZenithTaskSystem` + `ZenithInput` + `ZenithAssetHandling` + `ZenithFlux` +
`ZenithECS` + `ZenithPhysics` + `ZenithAI` + `ZenithScripting` +
`ZenithPrefab` + `ZenithUI`). `ZenithEntityComponent` sits at the TOP of the
measured header DAG (layer 6); `ZenithUI` sits directly below it (layer 5).

**Two seams, handled two different ways.** `EntityComponent <-> Flux` is the
SAME shape as the `Flux <-> AssetHandling` cycle Phase 4d declared: a genuine
header cycle (`Tools/complexity_profiles.json`'s
`architecture.rules.ignore_module_cycles` already listed `["EntityComponent",
"Flux"]`), declared one-way as `EntityComponent -> Flux` (the heavier
direction — components own Flux types/handles by value); Flux does not
depend back. `EntityComponent <-> Editor` is different in kind: it is real
(`Zenith_ModelComponent_Editor.cpp` / `Zenith_TerrainComponent_Editor.cpp`
`#include Editor/Zenith_Editor*.h` directly — three pre-existing
`layer-up:` entries in `Tools/architecture_allowlist.txt` already recorded
it) but is **not** a declared Sharpmake edge in either direction, because
`Editor/` is not yet its own library (still aggregate-owned) and declaring
either direction would create a cycle the tool cannot represent anyway. It
resolves the same way every other undeclared cross-directory `#include` in
this refactor does: `ConfigureCommonIncludePaths` puts `/Zenith` on every
project's include path, so the header resolves at compile time with no
declared edge, and any out-of-line symbol resolves at the final game exe
link, where the aggregate (today's owner of `Editor/`) is always present.
**A third seam turned out not to be one at all**: the plan flagged
`AI -> EntityComponent` as possibly real, but `ZenithAILibProject`
(`Sharpmake_ZenithAI.cs`) carries no declared dependency on this project
today and never has — the only "EntityComponent" references under
`Zenith/AI` are three `.Tests.inl` files each `#include`ing
`EntityComponent/Components/Zenith_TransformComponent.h`, which already
compile off the same undeclared-include-path mechanism. Nothing to drop, no
file to move.

**SHADER_SOURCE_ROOT, again.** Both new libraries needed it —
`ZenithEntityComponent` directly (it `#include`s `Flux/Flux_GraphicsImpl.h`,
which reaches the active backend's Pipeline header), and `ZenithUI`
*indirectly*, through the shared `Zenith.h` PCH alone (no `.cpp` in
`Zenith/UI` references the token or includes `Flux_GraphicsImpl.h` at all,
yet the `Null_` build still failed with C2065 on every `Zenith_UI*.cpp` TU
until the define was added) — the first library in this refactor where the
PCH chain alone, not a direct `#include`, was enough to need it. Neither
needed `ENGINE_ASSETS_DIR`.

**Library structure (Phase 4i — the Flux/AssetHandling cycle is BROKEN, not
declared):** Phase 4d extracted `Zenith/AssetHandling` whole and declared its
genuine header cycle with `Flux` one-way, `ZenithFlux -> ZenithAssetHandling`,
because the lighter direction was not forward-declarable: the asset classes own
Flux types **by value**. That worked (a static lib resolves no external symbol
at its own build step) but left the cycle in place — the declared graph was
acyclic only because one direction was left undeclared. Phase 4i splits the
directory into two libraries so the lower half can sit below Flux honestly.

*The measurement first.* On `master` 2026-09-20: **74 `#include
"AssetHandling/..."` directives across 51 distinct `Flux/**` files** go Flux to
AssetHandling; **12 directives across 11 `AssetHandling` files** go the other
way, of which **only 4 are headers** — and headers are the only ones that make
a *header* cycle. (Phase 4d's "23 against 11" counted a build-relevant subset
one way and never narrowed the other to headers; its "11" also counted FILES
where the 74 counts DIRECTIVES. `Zenith_TextureAsset.cpp:7` and `:8` both
include `Flux_GraphicsImpl.h` — a pre-existing duplicate — which is where the
twelfth directive comes from. Both figures above include `.Tests.inl` on both
sides, so they are finally the same population.) The four:

| header | includes | owns by value |
|---|---|---|
| `Zenith_TextureAsset.h` | `Flux/Flux.h` (the umbrella) | `Flux_SurfaceInfo`, `Flux_VRAMHandle`, `Flux_ShaderResourceView` |
| `Zenith_MeshAsset.h` | `Flux/Flux_Buffers.h` | `Flux_VertexBuffer`, `Flux_IndexBuffer`, `Flux_BufferLayout` |
| `Zenith_AnimatorControllerAsset.h` | `Flux/MeshAnimation/Flux_AnimatorControllerDef.h` | `Flux_AnimatorControllerDef` |
| `Zenith_GrassTypeTableAsset.h` | `Flux/Vegetation/Flux_GrassTypeTable.h` | `Flux_GrassTypeTable` |

★ **`Zenith_TextureAsset.h` is the crux and Phase 4d's note named the wrong two
files.** It is the `AssetHandling` header `Flux` includes most often, so
`Flux -> Zenith_TextureAsset.h -> Flux/Flux.h` was a cycle straight through the
asset layer. Phase 4d discussed `Flux_AnimatorControllerDef.h` and
`Flux_GrassTypeTable.h` instead; that rider is **confirmed** (each pulls in two
or three more `Flux/**` headers, so relocating them alone drags the rest across
or leaves the edge in place) and beside the point.

*The split line.* A unit joins the GPU half if its `.h` **or** its `.cpp`
reaches `Flux/**` — a `.cpp` reaching Flux creates a link edge at library
granularity just as a header does. Five more `.cpp` qualify on their own
(`Zenith_AnimationAsset`, `Zenith_BoneMaskAsset`, `Zenith_FontAsset`,
`Zenith_MaterialAsset`, `Zenith_MeshGeometryAsset`), giving **nine** GPU units,
not four.

| library | Sharpmake file | TUs | depends on |
|---|---|---|---|
| `ZenithAssetRegistry` | `Sharpmake_ZenithAssetRegistry.cs` | 13 units (12 `.cpp` + header-only `Zenith_AssetTypeIds.h`) + `Internal/Zenith_AssetRegistryPCH.cpp`; PCH | `ZenithCore` + `ZenithMaths` + `ZenithDataStream` + `ZenithScripting`. **No `ZenithFlux` edge, ever — that is the deliverable.** |
| `ZenithAssetHandling` | `Sharpmake_ZenithAssetHandling.cs` | 9 `.cpp` + `Internal/Zenith_AssetHandlingPCH.cpp` = 10; PCH | `ZenithCore` + `ZenithAssetRegistry` + **`ZenithFlux`** + `ZenithMaths` + `ZenithDataStream` |

Declared graph: **`ZenithAssetRegistry` ← `ZenithFlux` ← `ZenithAssetHandling`**.
Acyclic. `ZenithFlux`'s `AddPublicDependency` moved from `ZenithAssetHandling`
to `ZenithAssetRegistry`; the GPU half now declares the `ZenithFlux` edge Phase
4d could not. **Never restore `ZenithFlux -> ZenithAssetHandling`** — Sharpmake
will not tolerate the cycle, and nothing needs it: Flux's `#include`s of the
nine GPU headers resolve off the plain `/Zenith` include path, and any
out-of-line `Zenith_*Asset::` symbol resolves at the final exe link, exactly the
one-way shape `EntityComponent -> Flux` and `backend -> Flux` already have.

*Measured and recorded: the lower half's header closure is CLOSED.* No header
`ZenithAssetRegistry` owns reaches Flux, directly or transitively through its
own headers. `Zenith_AssetRegistry.h`'s **forward declarations** of
`Zenith_TextureAsset` / `Zenith_MaterialAsset` / `Zenith_MeshAsset` /
`Zenith_SkeletonAsset` / `Zenith_ModelAsset` / `Zenith_Prefab` are what make
that possible and are load-bearing — turning one into an `#include` undoes the
split.

★ **What is NOT claimed.** Three registry-half TUs (`Zenith_AssetRegistry.cpp`,
`Zenith_AssetHandle.cpp`, `Zenith_SkinDeform.cpp`) `#include` a GPU-half header
and therefore *compile* Flux headers, and the shared `Zenith.h` PCH reaches Flux
for every TU in the engine regardless. "The registry half never compiles a Flux
header" was never available to claim and is not claimed. The deliverable is the
**declared module graph**, plus the header-closure property above.

*`SHADER_SOURCE_ROOT` — BOTH halves need it, and that was measured rather than
guessed.* The `C2065` that first forced this define onto the undivided
`ZenithAssetHandling` (Phase 4d) was reported against `Zenith_TextureAsset.cpp`
**and** `Zenith_SkinDeform.cpp` — and Phase 4i puts those two on **opposite**
sides of the split. Both projects therefore carry the identical win64/agde
branch; neither inherits it from the other. This is the fourth library to hit
the `ZenithDebugVariables` (Phase 4b) defect class, and the second after
`ZenithUI` where grepping the directory for the token predicts nothing.

*Membership, not relocation: NOTHING MOVED.* Not one file, not one `#include`
spelling — 251 files across `Zenith/`, `Games/`, `Tools/` and `Tests/`
`#include "AssetHandling/<something>.h"`, so relocating a header into a
subdirectory would break every one of them for no benefit. The two projects
share the `Zenith/AssetHandling` `SourceRootPath` and partition it with
constructor-time `SourceFilesExcludeRegex` — a **membership** exclude ("which
project owns this file"), the mechanism `Sharpmake_ZenithImGui.cs`,
`Sharpmake_ZenithJolt.cs` and `Sharpmake_FreeType.cs` already use. It is **not**
`conf.SourceFilesBuildExcludeRegex`, the per-configuration *build* exclude that
Phase 4h.3 drove to zero call sites codebase-wide and that
`Tools/check_project_membership.ps1` now fails on outright. The two exclude
lists are exact complements: each `.Tests.inl` follows its own `.cpp`, each
project carries its own definition-free PCH create TU under `Internal/`, and
each project gets a distinct `conf.IntermediatePath` (two projects creating
`Zenith.pch` in one obj dir is MSB8028).

★ **Adding a unit to `Zenith/AssetHandling` means adding it to exactly one of
those two exclude lists** — owned by neither is a fatal `UNOWNED` finding,
owned by both a fatal `DUPLICATE`. The patterns are prefix-sensitive and every
one ends in `\.` deliberately: `Zenith_Asset` is a prefix of
`Zenith_AssetHandle`, `Zenith_AssetRegistry`, `Zenith_AssetTypeIds` *and*
`Zenith_AssetHandlingPCH`, while `Zenith_MaterialAsset` and
`Zenith_MaterialParamTable` land on opposite sides of the split.

*Two things deliberately NOT done.* Splitting `Zenith_TextureAsset` or
`Zenith_MeshAsset` as **types** (separating their by-value Flux members from
their CPU data) is a type refactor, not a library split — both classes belong
wholesale in the GPU half. And `Tools/complexity_profiles.json`'s
`architecture.modules` still maps `Zenith/AssetHandling/**` to one `AssetHandling`
module at layer 2: that map is directory-glob-based and the files did not move,
so it cannot express this split without enumerating filenames. The C++
layer/cycle gates are unchanged by this phase.

Dependency order: `ZenithCore` ← {`ZenithMaths`, `ZenithDataStream`,
`ZenithFileAccess`} ← {`ZenithDebugVariables` ← `ZenithProfiling`,
`ZenithTaskSystem`} ← `ZenithInput` ← {`ZenithWindows` (win64), `ZenithAndroid`
(agde, also ← `ZenithFileAccess`)} ← `ZenithUnitTests` ← `ZenithECS` (L1) ←
{`ZenithSaveData`, `ZenithTelemetry`, `ZenithPrefab`, `ZenithScripting`} ←
(`ZenithJolt`, standalone) ← `ZenithPhysics` ← `ZenithAI` ←
(`ZenithImGui`, standalone) ← `ZenithAssetRegistry` ← `ZenithFlux` ←
{`ZenithRenderBackendVulkan` (win64+agde, also ← `ZenithDebugVariables` +
`ZenithTaskSystem`), `ZenithRenderBackendD3D12` (win64), `ZenithRenderBackendNull`
(win64)} ← `ZenithUI` (layer 5, also ← `ZenithAssetHandling` + `ZenithFlux` +
`ZenithInput` + `ZenithProfiling`) ← `ZenithEntityComponent` (layer 6, the top
of the DAG — also ← `ZenithECS` + `ZenithPhysics` + `ZenithAI` +
`ZenithScripting` + `ZenithPrefab` + `ZenithUI`) ← `Zenith` (aggregate:
Editor, EngineComposition, everything else — Flux + AssetHandling +
imgui-core + the three render backends + UI + EntityComponent all moved OUT
across Phases 4d/4e/4f).

`ZenithAssetHandling` (Phase 4i — the GPU half) **hangs off that chain rather
than sitting in it**: it depends on `ZenithFlux` + `ZenithAssetRegistry`, and
its own dependents are `ZenithUI`, `ZenithEntityComponent`, `ZenithEditor`,
`ZenithTools` and `SentinelFlux`. Writing it inline between `ZenithFlux` and
the render backends would read as "the three render backends depend on
`ZenithAssetHandling`", which is false — each backend depends only on
`ZenithCore` + `ZenithFlux` (see `Sharpmake_EngineLibs.cs`).
**`ZenithCore` is a MANDATORY, unconditional dependency of every other engine
library and every Sentinel**, even where the edge would be transitive
anyway — `Zenith_LogFileSinkWrite` (defined in `Zenith.cpp`, in `ZenithCore`)
is called from the *inline* `Zenith_LogImpl` in the shared `Zenith.h` header,
so every translation unit in every library emits a reference to it.

**The ≥4-TU precompiled-header rule:** a library gets its own binary
`Zenith.pch` only once it compiles at least 4 translation units. A win64
`Zenith.pch` is 119–192 MB and `Build/obj` runs into the gigabytes, so a 1-TU
library that CREATEs its own PCH pays a full `glm` header parse TWICE (once
building the PCH, once — for nothing, since there is only the one consuming
TU) instead of once. `ZenithMaths`/`ZenithDataStream`/`ZenithFileAccess`
(Phase 4a) and `ZenithDebugVariables`/`ZenithTaskSystem`/`ZenithUnitTests`
(Phase 4b — each 1 TU) fall below the threshold and leave
`conf.PrecompHeader`/`conf.PrecompSource` unset, compiling `Zenith.h` as a
plain textual include exactly like the Sentinels and the game executables
already do (verify `NotUsing` in the generated `.vcxproj`). `ZenithInput`
(4 TUs), `ZenithWindows` (11 TUs) and `ZenithAndroid` (9 TUs) clear the
threshold and each CREATE their own PCH from a definition-free create TU
under `Internal/` (`Zenith_InputPCH.cpp` / `Zenith_WindowsPCH.cpp` /
`Zenith_AndroidPCH.cpp` — a one-line `#include "Zenith.h"`, nothing else:
AGDE/clang precompiles the entire `PrecompSource` via `-xc++-header`, so a
definition-bearing create TU would bake its own symbols into the binary
`.pch` and every `/Yu` consumer would re-emit them). Each lib that DOES
compile a
PCH builds its own (a binary `.pch` cannot be shared across projects with
different defines). The lockstep rule: `ZENITH_TOOLS`,
`ZENITH_PROFILING_ENABLED`, `ZENITH_MEMORY_TRACKING_LEVEL` must be identical
across every leaf lib and every consumer (ODR) — including the three
PCH-less libs, which set `ZENITH_TOOLS` in a `True` config even though
nothing in their own source reads it.

**Sentinels — SEVEN, not one per library** (`Tests/Sentinel*/`): leaf-purity
proof exes that link EXACTLY a deliberately-narrow declared dependency set —
an accidental leaf→engine edge fails their link with an unresolved external.
A sentinel is only informative where the declared set is genuinely narrow;
for a library whose declared dependencies are "everything below me", linking
standalone is guaranteed by construction and proves nothing, which is why
this is seven sentinels at cluster boundaries rather than thirty-two (one per
leaf library):

| Sentinel | Proves | Links |
|---|---|---|
| `SentinelECS` | ZenithECS is a self-contained leaf | zenithecs.lib + zenithcore.lib |
| `SentinelPhysics` | ZenithPhysics is a self-contained leaf | zenithphysics.lib + zenithecs.lib + zenithcore.lib + zenithjolt.lib |
| `SentinelAI` | ZenithAI is a self-contained leaf | zenithai.lib + zenithphysics.lib + zenithecs.lib + zenithcore.lib |
| `SentinelCore` | the Core cluster (ZenithCore + ZenithMaths + ZenithDataStream + ZenithFileAccess) links with nothing above it — the floor every other sentinel already stands a slice of | those four only |
| `SentinelScripting` | ZenithScripting links against its declared set | zenithscripting.lib + zenithecs.lib + zenithdatastream.lib + zenithmaths.lib + zenithcore.lib |
| `SentinelFlux` | the Flux cluster (ZenithFlux + both AssetHandling halves + the ACTIVE render backend) links without reaching Editor/EntityComponent/UI | ZenithFlux's own declared set (which since Phase 4i names `ZenithAssetRegistry`, not `ZenithAssetHandling`) + an EXPLICIT `ZenithAssetHandling` edge + the target's active `ZenithRenderBackend{Vulkan,D3D12,Null}`. ★ That explicit edge was always there and is now load-bearing rather than redundant: `zenithflux.lib`'s objects reference out-of-line `Zenith_TextureAsset::`/`Zenith_MeshAsset::` symbols that only the GPU half defines, and Phase 4i stopped `ZenithFlux` supplying it transitively. |
| `SentinelUI` | ZenithUI's declared dependency edges resolve, and its own GPU-free Flux/Core surface links | zenithui.lib's declared set + a GPU-free `Zenith_RenderBus::RecordDrawCall` touch — **see the caveat below** |

They exist only in `ToolsEnabled=False` configs. The known, deliberate platform
seams of the leaf libs (file access, debug break, the non-profiling mutex, the
unit-test registrar, callstack capture, the profiler's string-zone markers) are
supplied by each sentinel's `sentinel_platform.cpp` — extend THAT file when a
leaf lib grows a new sanctioned seam; anything else unresolved is a real leak.
CI builds AND runs all seven (engine-gate, §8; the array appears twice in
`.github/workflows/engine-gate.yml` — build step and run step, the latter
lowercased for the executable path — a human edit, see the Phase 5.2 entry
below).

**`SentinelUI` proves a WEAKER claim than its six siblings, and the gap is a
real finding, not a workaround.** `Tests/SentinelUI/main.cpp`'s own header
comment has the full account: constructing ANY concrete `Zenith_UIElement`
(the SentinelECS-style "drive a real object" proof every other sentinel uses)
pulls `Zenith_UIElement.obj` — the base class every widget's constructor
needs — which ALSO defines `Zenith_UIElement::CreateElement(UIElementType)` (a
factory `new`-ing every sibling widget by enum, including `Zenith_UIOverlay`)
and `Zenith_UIElement::Render(Zenith_UICanvas&)`, which calls
`g_xEngine.Input().GetMousePosition(...)` directly. Because Sharpmake links at
`.obj` granularity, that one file's unrelated content comes along for free,
cascading through `Zenith_UICanvas.obj` into ~25 `Zenith_Engine::<Subsystem>()`
accessors, four `Zenith_Active*()` free functions and a dozen
`Zenith_Vulkan_*` backend methods — 113 unresolved externals, measured, none
of them declared anywhere in `Sharpmake_ZenithUI.cs`'s dependency list because
they route through `g_xEngine`, which no leaf project (not even `ZenithFlux`
itself) declares an edge to. This is architecturally correct — a widget's job
is to submit real draw calls to the live renderer — it just means `ZenithUI`,
unlike `ZenithECS`/`ZenithPhysics`/`ZenithAI`/`ZenithScripting`, is not
severable from `EngineComposition` at this granularity. Stubbing the ~110-symbol
surface was rejected as dishonest (it would be hand-faking the renderer, not
supplying a platform shim) in favour of scoping the sentinel to what IS true
without that reach.

**Library structure (Phase 4g — the two Tools-only leaves, and a headline
mechanism change):** `ZenithEditor` (`Zenith/Editor`'s 56 TUs across
`Animation/` + `Panels/` + `TerrainEditor/` + the top level, + its own PCH
create TU = 57; PCH; depends on `ZenithCore` + `ZenithMaths` +
`ZenithDataStream` + `ZenithFileAccess` + `ZenithProfiling` +
`ZenithDebugVariables` + `ZenithInput` + `ZenithAssetHandling` + `ZenithFlux` +
`ZenithECS` + `ZenithPhysics` + `ZenithScripting` + `ZenithPrefab` +
`ZenithWindows`) and `ZenithTools` (repo-root `Tools/`'s 17 TUs + its own PCH
create TU = 18; PCH; depends on `ZenithCore` + `ZenithMaths` +
`ZenithDataStream` + `ZenithFileAccess` + `ZenithTaskSystem` +
`ZenithAssetHandling` + `ZenithFlux` + `ZenithEntityComponent` +
`MsdfAtlasGenProject`). `ZenithTools` adopts a project that already existed —
it had generated `zenithtools_win64.vcxproj` since before this phase but was
added to no solution, which is exactly what made it the repo's one orphan
project (plus 7 orphan-duplicate findings, `check_project_membership.ps1`).
Both new libraries declare **win64 × ToolsEnabled.True ONLY** — no `agde`
target at all, and (unlike every earlier win64-only leaf) `ToolsEnabled`
itself carries a single value rather than `True | False`. That is a NEW
target-restriction shape: a predicate on both `target.Platform` AND
`target.ToolsEnabled` at once
(`t => t.Platform == Platform.win64 && t.ToolsEnabled == ToolsEnabled.True`
in `Build/Sharpmake_EngineLibs.cs`).

**Target restriction replaces the per-configuration Editor/Tools build
excludes.** Before this phase, `Zenith/Editor/` and the repo-root `Tools/`
were owned by the monolithic aggregate and kept out of configurations that
should not have them via FOUR separate `conf.SourceFilesBuildExcludeRegex`
entries: `ConfigurePlatformExcludes`'s `agde` arm (`\Editor\` and `\Tools\`),
`Sharpmake_Zenith.cs`'s own `agde`-only `\Editor\` exclude in `ConfigureAll`,
and its `ToolsEnabled.False`-branch `\Tools\` exclude. All four are DELETED,
not merely moved: a `_False` or `agde` configuration now simply has no
`ZenithEditorLibProject` / `ZenithToolsProject` in its solution at all — an
absent project, not an empty one, the same shape Phase 4e's three
render-backend leaves already established for `RenderBackend`. This is safe
for Editor specifically because all 56 of its `.cpp` files were independently
already wrapped start-to-end in `#ifdef ZENITH_TOOLS` (verified by script) —
the library does not add a new restriction, it makes an existing
content-level one structural. `Zenith/Editor`'s aggregate MEMBERSHIP exclude
(`SourceFilesExcludeRegex` in `Sharpmake_Zenith.cs`'s constructor) is new this
phase too — earlier phases never needed one because Editor's per-file
`#ifdef` was already doing the job in a `_False` config, and win64
`ToolsEnabled.True` was the one configuration where the aggregate and the new
library would otherwise have compiled the same 56 files into two different
`.lib`s.

**The repo-root `Tools/` tree gets a single owner for the first time.**
`ZenithToolsProject`'s own `AdditionalSourceRootPaths.Add(.../Zenith/Flux/MeshGeometry)`
is dropped — it depends on `ZenithFluxLibProject` (which owns that directory
whole) instead of compiling a second copy of `Flux_MeshGeometry.cpp` /
`Flux_MeshInstance.cpp` — and the aggregate's own
`AdditionalSourceRootPaths.Add(RootPath + "/Tools")` is dropped entirely, so
`ZenithToolsProject` is the sole owner of the tree. Adopting a project that
had never actually been built also surfaced real, previously-dormant defects
in its own `ConfigureAll`: no call to `ConfigureCommonSettings` /
`ConfigureCommonIncludePaths` at all (missing the render-backend define
`Zenith_BackendAliases.h` `#error`s without, C++20, warnings-as-errors,
memory-tracking level), no PCH, no distinct `IntermediatePath`, and — the
build-breaking one — no exclude for `Tools/Middleware/freetype` (221 `.cpp`)
or `Tools/Middleware/msdf-atlas-gen` (47 `.cpp`), which
`FreeTypeProject`/`MsdfgenProject`/`MsdfAtlasGenProject` already build as
separate projects linked as prebuilt `.lib`s — an ODR violation that was
silently dormant only because the project never compiled. All fixed by
rewriting the file onto the same `ConfigureCommonSettings`/
`ConfigureCommonIncludePaths` foundation every other leaf uses.

**The tools-only link surface split three ways, not moved wholesale.** The
aggregate's old `ToolsEnabled.True` block bundled `OPENDDLPARSER_BUILD`,
`ZENITH_ROOT`, the Tools/Middleware + assimp include paths, the assimp
library path + `.lib` file, `PrecompSourceExcludeFolders(Tools)`, and the
`MsdfAtlasGenProject` dependency. Each piece moved only to the project whose
own compiled TUs actually reference it: `ZenithToolsProject` keeps all of it
(its `Zenith_Tools_FontExport.cpp` genuinely calls `msdf-atlas-gen` directly —
the only Tools/Editor consumer of freetype/msdf-atlas-gen this phase).
`ZenithEditorLibProject` gets NONE of it — grep across every
`Zenith/Editor/**/*.{h,cpp}` finds zero references to assimp, freetype or
msdf-atlas-gen, so copying the block wholesale would have been exactly the
"do not just relocate the whole block" mistake the phase's own brief warned
against. **`ZENITH_ROOT` is the one exception that stayed on BOTH**: build
verification found `Zenith/EngineComposition/Zenith_UnitTests.Tests.inl`
(`#include`d from the aggregate's own `Zenith_Core.cpp`, unconditionally on
every `ToolsEnabled.True` win64 config) references it too, unguarded — a
grep across `Tools/*.cpp` alone (3 files) was not the whole picture, so the
aggregate keeps its own copy of the define alongside `ZenithTools`'s.

**SHADER_SOURCE_ROOT, a third time, and the same lesson twice more.**
`ZenithEditorLibProject` needs it directly (8 files `#include`
`Flux/Flux_GraphicsImpl.h`, which reaches the active backend's Pipeline
header) plus `ENGINE_ASSETS_DIR` (3 `TerrainEditor/*.cpp` files, unguarded).
`ZenithToolsProject` was assumed NOT to need it — no direct
`Flux_GraphicsImpl.h` / Pipeline-header `#include` anywhere under `Tools/` —
but a real `Null_..._True` build failed C2065 on 7 Tools TUs
(`…RockAssetExport.cpp`, `…MeshExport.cpp`, `…HumanModelExport.cpp`,
`…TerrainExport.cpp`, `…TextureExport.cpp`, `…GlbImport.cpp`,
`…TreeAssetExport.cpp`) that reach `RenderBackends/Null/Zenith_Null_Pipeline.h`
TRANSITIVELY through `Flux/MeshGeometry` headers. Exactly the same class of
defect Phase 4f already logged for `ZenithUI` (needed the define with ZERO
textual occurrences of the token anywhere in its own directory) — a second,
independent confirmation that a direct-`#include` grep is not sufficient and
only a real `Null_` build build-verifies it.

**One source fix, not a PCH exclusion.** `Zenith_Tools_MeshExport.cpp` had an
`#ifndef NOMINMAX` / `#define NOMINMAX` guard preceding its
`#include "Zenith.h"` — harmless under the aggregate's old
`PrecompSourceExcludeFolders.Add(Tools)` (which meant NO Tools file ever used
`/Yu`), but a C4005 macro-redefinition warning (→ C2220 under
`TreatWarningsAsErrors`) the moment the file compiles under `ZenithTools`'s
own PCH, since `NOMINMAX` is already defined for every configuration via
`ConfigureCommonSettings`. Removed the now-redundant guard rather than
reintroducing a PCH exclusion for the whole directory — every other
`Tools/*.cpp` already begins cleanly with `#include "Zenith.h"` and benefits
from the PCH `ZenithTools` earns at 17 TUs.

**`EntityComponent <-> Editor` stays undeclared, from the OTHER side now
too.** `ZenithEntityComponentLibProject` already promised (Phase 4f) not to
depend on a future Editor library; `ZenithEditorLibProject` now keeps the
other half of that promise — it does not declare
`AddPublicDependency<ZenithEntityComponentLibProject>` even though 17 of its
own files `#include EntityComponent/Components/*.h` directly. Both directions
would create a Sharpmake cycle the tool cannot represent; the include
resolves off the plain `/Zenith` include path at compile time (a static lib
never needs to resolve an external symbol at its own link step), and any
out-of-line symbol resolves at the final game exe link, where
`ZenithEntityComponent` is always present.

**Library structure (Phase 4h — the final tier: `ConfigurePlatformExcludes`
deleted, the library-split refactor's closing move).** The refactor's
invariant — every compiled source belongs to exactly one project per link
target, expressed as target declaration + solution membership, never as a
per-configuration subtraction — now holds with **zero** exceptions.

*4h.1 — three easy conversions.* The legacy `ZenithECS`/`Physics`/`AI`
per-configuration build-excludes in `Sharpmake_Zenith.cs`'s `ConfigureAll`
(left over from the original leaf extractions, back when only the `.cpp`
subtraction had been done and the constructor MEMBERSHIP-exclude conversion
every other leaf got was still pending) became constructor
`SourceFilesExcludeRegex` lines, anchored `.*\\Zenith\\<Dir>\\.*` (re-verified
safe: `\Zenith\Physics\` does not match `Middleware\JoltPhysics-5.4.0\`,
`\Zenith\AI\` matches nothing under `EntityComponent\`). `imgui_impl_android.cpp`
converted the same way — **contradicting this phase's own brief**, which
guessed the exclude might already be dead "now that ZenithImGui owns the
imgui tree and the aggregate no longer globs `Middleware/imgui-docking`": the
aggregate's `AdditionalSourceRootPaths(/Middleware/imgui-docking)` call was
never removed, and `imgui_impl_android.cpp` was never one of the per-file
membership excludes carved out of it — it compiled nowhere on any platform,
but for two DIFFERENT reasons (an agde-only `ConfigureAll` build-exclude, and
`ConfigurePlatformExcludes`' win64 `.*_Android.*` arm), not because it was
already ownerless.

*4h.2 — the measurement.* Every generated `.vcxproj`'s `ClCompile` membership
was checked against all four `ConfigurePlatformExcludes` patterns
(`\Android\`, `_Android`, `\Windows\`, `_Windows`). Result: **one genuinely
load-bearing call site in the entire codebase** —
`ZenithRenderBackendVulkan`, whose `RenderBackends/Vulkan` source root held
`Platform/Windows/` and `Platform/Android/` side by side while the project
itself spanned both win64 and agde. Every other one of the ~40 call sites
(every other engine leaf, every game via `Sharpmake_Games.cs`, the three
Sentinels) was dead: either the project's own directory held no
Windows/Android-named-or-pathed file at all, or — for `ZenithWindows`/
`ZenithAndroid` themselves — the project only ever configured the platform
whose own files matched, so the *opposite* exclusion arm (the only one that
could ever have found anything) never ran. This directly falsifies the prior
Phase 4e entry's claim that the mechanism stayed load-bearing for
`ZenithFlux`/`ZenithImGui`/`ZenithAssetHandling`/the aggregate — none of the
four ever had a matching member.

*4h.3 — the fix, and the deletion.* `Zenith/RenderBackends/Vulkan/Platform/{Windows,Android}`
each became a one-TU leaf library
(`ZenithRenderBackendVulkanPlatformWindows`, win64 × `RenderBackend.Vulkan`
only; `ZenithRenderBackendVulkanPlatformAndroid`, agde only — no PCH, one TU
apiece, both well under the ≥4-TU threshold), membership-excluded from
`ZenithRenderBackendVulkan`'s own constructor and wired into
`Build/Sharpmake_EngineLibs.cs` (taking the table to 32 entries). Both depend
on `ZenithCore` + `ZenithRenderBackendVulkan` (the `Zenith_Vulkan_Platform.h`
interface they implement); the Windows one also depends on `ZenithWindows`
for `Zenith_Window::GetInstance()`; the Android one leaves
`Zenith_Android_GetNativeWindow()` an accepted undeclared `extern` symbol,
resolved only at the final game `.so` link, the same precedent
`ZenithInput`'s glfw calls already established. With that split done, every
`ConfigurePlatformExcludes` call site in the codebase was dead, and every
call site was removed (32 leaves + the aggregate + the Vulkan backend + the
three Sentinels + `Sharpmake_Games.cs` = every caller). `ConfigurePlatformExcludes`
itself — the method, and the four `conf.SourceFilesBuildExcludeRegex.Add`
lines it contained — was then deleted from `Sharpmake_Common.cs` outright,
having no caller left. **`SourceFilesBuildExcludeRegex.Add(...)` call sites
across `Build/Sharpmake_*.cs` (excluding the vendored `FreeType`/`Msdfgen`/
`MsdfAtlasGen` projects, which never adopted the mechanism): zero.**

*4h.4 — the instrument flips to fatal.* `Tools/check_project_membership.ps1`
now defaults to **fatal** (duplicate-owned / orphan / orphan-duplicate /
genuine-unowned / un-allowlisted-build-exclude findings all exit 1); the old
always-exit-0 behaviour survives as an explicit `-ReportOnly` escape hatch.
The build-exclude grep gate is fatal too, narrowed from "any line mentioning
the identifier" (which would have permanently red-lit the very comments
documenting this section) to "an actual `.Add(...)` call site, not inside a
`//` comment" — with an allowlist for any deliberate, documented survivor.
**The allowlist is empty**: Phase 4h.3 left nothing to allow. The script
exits 0 on the current tree in its new default mode.

*4h.5 — the invariant, stated plainly.* Every compiled source belongs to
exactly one project per link target. Conditionality is expressed as target
declaration (a project's own `AddTargets`) plus solution membership (a
`Func<ZenithTarget,bool>` predicate in `Build/Sharpmake_EngineLibs.cs`), never
as a per-configuration source exclude. `Tools/check_project_membership.ps1`
enforces this by construction, fatally, by default — this is the definition
of done for the library-split refactor's structure.

**Phase 5 — the aggregate becomes the composition root, and four more
Sentinels.** By Phase 4h every OTHER engine subsystem was its own leaf
library; the aggregate `ZenithProject` was the one structurally-wrong thing
left, still globbing the whole `/Zenith` tree recursively (330 TUs) and
subtracting every extracted library back out with ~30 constructor
`SourceFilesExcludeRegex` lines — the exact greedy-glob-minus-subtraction
shape this whole refactor exists to remove, just at the top instead of a
leaf.

*5.1 — re-rooted.* `Build/Sharpmake_Zenith.cs` now sets `SourceRootPath =
RootPath + "/Zenith/EngineComposition"` — the aggregate's ENTIRE source tree
is the 7 `.cpp` + 5 `.Tests.inl` files that directory already held (moved
there in an earlier phase specifically so this phase could re-root onto it;
see `Zenith/EngineComposition/CLAUDE.md`). Every `SourceFilesExcludeRegex`
line that existed only to subtract an already-extracted leaf is deleted
outright — verified-dead, not merely unneeded, since the narrowed glob no
longer reaches any of those directories to begin with — and so are the
`AdditionalSourceRootPaths` entries for `Middleware/imgui-docking` and
repo-root `/Tools` (owned whole by `ZenithImGui` / `ZenithTools` now; the one
file this drops from the aggregate's membership,
`imgui_impl_android.cpp`, compiled nowhere before this change either and
sits outside `Zenith/**`, so it is invisible to
`Tools/check_project_membership.ps1`'s UNOWNED check both before and after).
Games, `FluxCompiler`, `ZenithHub` and both TilePuzzle projects needed ZERO
edits — they keep their one `AddPublicDependency<ZenithProject>` edge, and
`ZenithEngineLibs.AddAllAsDependenciesOfAggregate` still re-exports every
other leaf through it. `dbghelp.lib`/`psapi.lib` — carried unconditionally on
the aggregate for every win64 config since before this phase — moved to the
libraries that actually call them: `dbghelp.h` (`SymInitialize`/`SymFromAddr`/
`StackWalk64`) is used only in `Zenith/Windows/Callstack/Zenith_Windows_Callstack.cpp`
(now `ZenithWindows`'s own `conf.LibraryFiles.Add`), and `GetProcessMemoryInfo`
only in `Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp` (now
`ZenithEditor`'s own, gated `win64 x ToolsEnabled.True` — tighter than the
aggregate's old unconditional-for-every-win64-config placement, since nothing
in a `_False` build ever called it). **Correction to this phase's own brief,
found by grep before moving either line:** both were assumed to live in
`Zenith/Core/`; neither does — `Zenith/Core/Zenith_Win32.h` merely `#include`s
`psapi.h` for the `PROCESS_MEMORY_COUNTERS` type, which costs nothing at link
time until something calls an actual psapi-exported function.

*5.2 — four more Sentinels, seven total.* See the Sentinels table above for
what each new one proves. `SentinelFlux` is the one sentinel that
deliberately is NOT renderer-abstinent — it adds the target's active
`ZenithRenderBackend{Vulkan,D3D12,Null}` project (conditionally, since each
backend project declares only its own `RenderBackend` value) because the
whole point of "the Flux cluster" is the render-backend boundary; its
`main.cpp` stays GPU-free (`Zenith_RenderBus::RecordDrawCall`, the one
top-level Flux `.cpp` with zero `g_xEngine` references, confirmed by grep
against all 17 top-level `Flux/*.cpp`) because the sentinel proves LINKING,
not live rendering — creating an actual device is what the backend's own API
requires and what a headless link-proof exe has no business doing.
`SentinelUI` could not follow the same "construct one real object" pattern —
see the caveat in the Sentinels table above. All four new sentinels'
`sentinel_platform.cpp` started from `SentinelECS`'s existing L0 floor
(`Zenith_DebugBreak`, three `Zenith_FileAccess` entries,
`Zenith_Windows_Mutex_T<true>`/`<false>`, `Zenith_PlatformStdio::OpenFile`,
the four `Zenith_Callstack::Platform_*` hooks, the assert-capture atomics) and
needed **zero new stub symbols** beyond that floor — `SentinelCore`,
`SentinelFlux` and `SentinelScripting` all linked clean against it on the
first `zenith build engine` pass. That is itself a finding: it confirms the
Core-cluster platform floor is a genuine constant across every narrow leaf
this refactor has produced, not something each new sentinel reinvents.
`SentinelUI`'s finding was categorically different (a composition-root reach,
not a missing platform stub) and is documented in the Sentinels table above
rather than papered over with one.

`Tools/complexity_profiles.json`'s `architecture.modules` gained one entry
per new sentinel (`SentinelCore`/`SentinelFlux`/`SentinelScripting`/
`SentinelUI`, each `layer: 5`, each globbing only its own `Tests/Sentinel*/**`
— a single `Tests/**` module was tried first for the original three and
FAILED `max_module_fanout=8` at 9, because unioning independent harnesses
into one node merges their dependency footprints; one module per sentinel is
the shape that scales).

## 5. Testing

**Engine automated-test protocol** (any game exe): `--list-automated-tests`
(enumeration), `--all-automated-tests` (batch: one process runs every registered
test) or `--automated-test <Name>` (single), `--test-results-dir <D>` /
`--test-results <file>` (per-test JSON:
`passed`/`skipped`/`failures`/`frames`/`durationMs`), `--exit-after-frames N`,
`--fixed-dt`, `--skip-tool-exports`, `--skip-unit-tests`.

There is no `--headless` engine flag: run the game's **Null build** instead
(§4). Tests tagged `m_bRequiresGraphics=true` — the ones that READ PIXELS — are
skipped-as-passed there. **Discovery always uses the Null exe**, in every mode:
a Vulkan `--list-automated-tests` hangs in `vkEnumeratePhysicalDevices` on a
GPU-less runner, so `zenith test` errors with "build the Null config first" if
it is missing.

> **`--exit-after-frames` only applies while the test harness is stepping.** A
> boot with no `--automated-test` / `--all-automated-tests` never exits on its
> own; `Tools/run_unit_gate.ps1` relies on its watchdog kill, which fires AFTER
> the "Unit tests complete" line. A timeout there is expected, not a failure.

**`zenith test`** is the only test entry point, backed by
`Tools/ZenithCli/ZenithTestHarness.psm1` (`Invoke-ZenithGameTests`): pre-run
JSON wipe (stale results must never mask a regression), runtime-DLL self-heal,
discovery, dispatch, tally (a non-zero ENGINE exit fails the batch even if
every JSON says passed — crash-mid-suite guard), and a slowest-tests report.

- **Batch vs per-process:** batch by default; `--per-process`, `--fail-fast`,
  or a non-empty `--filter` forces one process per test (the engine's batch
  flag has no name filter, and batch ignores individual outcomes so fail-fast
  needs per-process).
- `--tier N` filters by the DP naming convention (`Test_T0*` / `Test_P<N>*`)
  BEFORE dispatch (does not force per-process).
- **Order control (cross-test-leak diagnosis).** `--tests A,B,C` runs exactly
  those tests, in that order, in ONE process (engine: `--automated-tests`) —
  the predecessor→victim probe for "does A contaminate B?". Duplicates are
  preserved on purpose (`A,A` probes self-contamination); note both
  occurrences write the same `<name>.json`, last-wins. Manual-only tests run
  when named explicitly. `--batch-order reverse|rotate:<N>` reorders the full
  batch without changing its set (`rotate:<N>` is normalised modulo the suite
  size, so `rotate:0` is the default order). Neither composes with the other,
  nor with `--filter`/`--tier`/`--per-process`/`--fail-fast`; conflicts are a
  usage error (CLI exit 1, engine exit 2). An unknown name in `--tests`
  reports EVERY unknown and runs nothing.
- **Between-test isolation.** Every test — batch member, batch-first, and
  single-test — runs against a world the ENGINE built. Before each test the
  harness normalises input + fixed-dt, calls
  `Zenith_SceneSystem::ResetWorldForNextTest()` (destroying **every** scene,
  including the persistent/DontDestroyOnLoad one), runs hygiene for the few
  things no scene owns (instrumentation logs, deferred-event queue, save
  sandbox, editor session state), reloads the boot scene, and settles. A test
  needs cleanup only for state no lifecycle owns — use `m_pfnTeardown`.
- **Infrastructure failures are exit code 3, not 1.** If the harness cannot
  build a clean world (boot scene will not load, post-reset settle times out)
  the remaining tests are *not run*: the engine writes
  `<results dir>/_infrastructure.json` (`phase` / `reason` / `beforeTest`) plus
  a `skipReason: "infrastructure"` record per unrun test, and the runner reports
  one infrastructure fault instead of a wall of MISSING lines. An
  infra-skipped test counts as neither passed nor failed.
- **Save sandbox.** Automated-test runs never touch real save data. The runner
  creates `Build/artifacts/savedata/<run-id>/`, writes an ownership marker, and
  passes `--test-save-root` + `--test-save-run-id`; `Zenith_SaveData` accepts a
  supplied root only if the marker's run-id matches, else falls back to
  `<exe dir>/TestSaveData/<game>/<run-id>/`. Between tests it deletes `*.zsave`
  only, non-recursively, only in the marker-bearing directory. The runner drops
  the sandbox on success and **keeps it on failure** for triage.
- Frame ceiling defaults to 8500 — a runaway backstop covering the slowest
  known suite; each test's own `m_iMaxFrames` governs its budget.
- Results default to `Build/artifacts/test_results/<game>/` (gitignored).
- Current suite baselines: **Combat 16**, **TilePuzzle 2**, **CityBuilder 47**,
  **DevilsPlayground 160**, **Zenithmon 61**, **RenderTest 16** (all observed
  2026-08-11 at the input program's closing gate, headless AND windowed; each
  game gained a sim-pad smoke test, ZM gained six touch tests. These are
  per-game AUTOMATED suites, which no gate pins the way `run_unit_gate.ps1`
  pins the engine unit count, so they drift silently. ZM windowed carries ONE
  known pre-existing failure: `ZM_InteriorTintPixels_Test`).

**Engine unit tests** run at every boot unless `--skip-unit-tests`. Baseline:
**1644 expected, 0 failed** (**+6 on 2026-08-21** for ZM-49's terrain
ground-height query, `Zenith_TerrainComponent::TryGetGroundHeightAt` and its two
static cores. Those six are BACKEND-NEUTRAL, so the same units moved **every**
game's row in one commit — Combat 1638 → **1644**, Null RenderTest 1729 →
**1735**, Zenithmon boot 3354 → **3360** — each number OBSERVED on a `Null_` run
rather than computed from the previous one. Before that, `1637` was observed
2026-08-12 on a `Null_` Combat
build, then the final backend-neutral indirect-count/multi-draw distinction test
added one shared registrar; that +1 was confirmed by the complete 1,729-test Null
RenderTest gate on 2026-08-13; 1600 → 1638 is +38 tests under the terrain
indirect-count compatibility plan — see `run_unit_gate.ps1` history); 1437 + 19 + 1
from compressed-vertex Phase 6 T6.a's instance-stream flips — 3 uint16x4 codec
units plus 4/5/4/3 packed-lane units for Text/Quads/Particles/Gizmos — then + 2 for
the scene-publish guard's `Editor.SceneSaveDeltaClassifiesPublish` /
`Editor.HeadlessSaveNeverRewritesSceneAsset`, then + 22 for
`Flux/Particles/Flux_ParticleGPU.Tests.inl` when the GPU-driven particle path was
wired, then + 11 for `Flux/SSAO/Flux_SSAO.Tests.inl` (6, the committed-handle
selection contract + blur-constant fill) and `Flux/Shadows/Flux_Shadows.Tests.inl`
(5, the sampling quality-flag pack and its float round trip), then + 6 for
`Flux/TAA/Flux_TAA_ResolveCPU.Tests.inl` when TAA's disocclusion test was found to
be rejecting history almost everywhere, then + 4 for the `TAASkyVelocity` cases in
`Flux/TAA/Flux_TAAJitter.Tests.inl` when the sky gained motion vectors, then
+ 24 for `Zenith/Input/Zenith_Input.Tests.inl` when the input program's WP0
landed the device foundations — event FIFO, drain-after-acquire, release edges,
gamepad completion, lifecycle barriers, simulator injection — then + 14 for the
input program's WP1: `Zenith_Pointers.Tests.inl` (the multi-touch pointer table
that replaced `Zenith_TouchInput`) and `Zenith_UIButton.Tests.inl` (standard-
control pointer capture), then + 33 for the input program's WP2:
`Zenith_InputActions.Tests.inl` — the action layer's bindings, profiles,
activity detection, mask-change synthesis, and claim suppression — plus canvas
focus nav on the engine-reserved UI actions, then + 13 for the input program's
WP3a: the on-screen `Zenith_UIVirtualStick`/`Zenith_UIVirtualButton` widgets —
deadzone/recentre/disarm/suppression semantics and their serialization
round trip, then + 4 for WP4a's B10 graph action nodes — OnActionPressed/
Released/Held + ReadActionAxis1D/2D and their builder factories, then + 10 for
WP6's `Zenith_UserSettings` persistence store and the SYSTEM_BACK device-layer
flag) — the
authority is the game's row in `Tools/unit_baselines.json`, not this
number; read it there if the two ever disagree.

> **A Slang probe does NOT move this number.** `Flux_MaterialTable.cpp` gates the
> whole `Flux_SlangProbes.Tests.inl` suite on `ZENITH_WINDOWS && ZENITH_VULKAN`,
> and both unit gates (`engine-gate` and `zm-tests`) run `Null_` builds — so no
> SlangProbes case is compiled into either. Probes are real coverage, but they
> are only exercised by a `Vulkan_` build; adding one and bumping the baseline
> turns the gate red. Asserted by that script (CI, which
`Tools/test_scaffold.ps1` also invokes for the scaffold gate), tolerating
exactly one known layout-sensitive skip
(`GraphComponent::RegistryWideNodeRoundTrip`). **When you add engine unit
tests, bump the game's row in `Tools/unit_baselines.json` in the same
change** — and note that a backend-neutral engine unit moves EVERY row in
that file, including Zenithmon's, which counts the engine suite plus its own. Combat carries the CI unit gate
(`engine-gate.yml`).

**PowerShell selftests** (`zenith selftest`): dependency-free assert-runners
covering name validation (shared vector file pinning PS↔C++ hub), the
descriptor validation matrix, codegen golden file, drift detection, process
sweep, DLL heal, orphan prune, the tracking-policy invariants, template
expansion, dispatcher exit codes, and the test-harness parsers/tally.

## 6. Artifacts and the tracking policy

- **`Build/artifacts/`** (gitignored) is the canonical root for EVERYTHING a
  runner/tool emits: `test_results/<game>/`, `telemetry/`, ad-hoc logs. Never
  invent a new ad-hoc `Build/<thing>_results` dir.
- **`dist/`** (gitignored) holds `zenith package` output.
- Tracked under `Build/`: ONLY hand-written files (`Sharpmake_*.cs`, scripts,
  `zenith_config.psd1`, `zenith_android_abis.gradle` — the shared Android ABI
  axis every game's `app/build.gradle` applies — `Templates/`, `Tests/`,
  `TestData/`) — ~39 files.
- Two regression tests in the buildsystem selftest lock this forever: no
  generated/transient pattern may appear in `git ls-files`, and no hand-written
  build file may be gitignored.

## 7. Packaging (relocatable builds)

`zenith package <Name>` stages exe + every runtime DLL (after the shared DLL
heal) + `Games/<Name>/Assets` + `Games/<Name>/Config` (if present) +
`Zenith/Assets` + `Zenith/Flux/Shaders` (skippable via `--no-shaders`) into
`dist/<Name>_<lowercase config>/`, plus a `run.bat` that launches with
`--assets-root "%~dp0"`.

**`zenith package` never regenerates or builds, unlike `zenith build`/
`zenith hub` — it only WARNS.** It has nothing to build (it stages an
already-built exe) and nothing to protect by acquiring the build slot, so
it checks regen staleness (§3) read-only against HEAD's current file list
and prints `WARNING: regen is stale (<reason>) -- the packaged exe may
predate HEAD's current file list.` rather than regenerating. A stale
warning here means the exe you are about to package was built from an
older file list than HEAD currently has — rebuild first if that matters.

**How relocation works:** the compile-time `GAME_ASSETS_DIR` /
`ENGINE_ASSETS_DIR` / `SHADER_SOURCE_ROOT` defines are absolute build-machine
paths. `Zenith_CommandLine::ResolveUnderAssetsRoot(baked, override, rel)`
(Core, L0 — deliberately NOT in AssetHandling so Flux can call it) substitutes
`<override>/<repo-relative layout>` when `--assets-root` is present and passes
the baked path through untouched otherwise. Consumers: the two asset dirs
(`Zenith_Engine::InitialiseAssets`), the Slang session search paths, Flux's
shader-root search path, and the shader hot-reload watcher.

**Residual limitation** (stated in each package's generated README): game code
that string-bakes `GAME_ASSETS_DIR` into compile-time literals (e.g. scene
build-index registration) bypasses runtime resolution and still points at the
build machine. Fix is a per-game sweep to runtime-resolved paths — never a
runtime alias/remap (§11).

**Verification recipe:** package Combat, copy `dist/...` elsewhere, `run.bat`
— windowed must render (shaders compile from the package tree); hide the
package's `Zenith/Flux/Shaders` and it must FAIL compile against only the
package path (proves no fallback to the baked path).

## 8. CI

All workflows run on `windows-latest`, regen with `Build/regen.ps1 -UseDotnet`
before building, and build with explicit `/t:` targets. Heavy workflows share
`.github/actions/zenith-setup` (composite action): setup-msbuild, vcpkg with
ONE shared cache key, Vulkan SDK cache+install, Slang release cache (105 MB,
previously downloaded every run), all downloads retried with backoff.
Concurrency groups cancel superseded PR runs (master pushes always complete).

| Workflow | Gates |
|----------|-------|
| `cb-tests` | CityBuilder Vulkan `_True` compile proof + **Null `_True` build (the exe every step runs)** + D3D12 `_False` link proof + 45-test headless suite via `zenith test` |
| `dp-tests` | Same shape for DevilsPlayground (158 tests) |
| `st-tests` | ScriptTest headless build + D3D12 `_False` link proof + **cold-bake stale-scene guard** (deletes the committed `.zscen` set — derived from `git ls-files`, never a hardcoded list — re-authors it from the boot recipes, asserts byte-identical return) + the 13-test headless suite. Drives `zenith` rather than msbuild so its gate lines stay character-identical to the zagent category |
| `engine-gate` | Sentinels (`Vulkan_Debug_Win64_False`) built AND executed + Combat unit gate (`Tools/run_unit_gate.ps1`; it passes no `-Baseline`, so the script's default is the pin — 1638 as of 2026-08-13 — known flake tolerated). Rollout: dispatch → burn-in → required |
| `release-build` | NIGHTLY (not PR-blocking): engine + DP in `Vulkan_vs2022_Release_Win64_True`, build-only — the only Release compile in CI |
| `shader-validation` | FluxCompiler (Release `_True`) catalog/parity/spine-lint + git-status drift gate on shader outputs |
| `scaffold-smoke` | Path-filtered end-to-end `zenith new` → build → boot (units baseline) → teardown leaves git status identical |
| `complexity` / `layering-gate` | `analyze_code_complexity.py` (engine-ci profile; pip cached via `Tools/requirements-ci.txt`). Ratchets: new architecture/lint findings must be FIXED, not allowlisted |
| `memory-gate` | Memory-budget baseline JSON ratchet (stdlib-only) |
| `doc-lint` | 6 consistency checks over `Games/DevilsPlayground/Docs/` |

Notes:

- CI compiles/links against the VENDORED `Middleware/VulkanSDK/<ver>`; the
  installed SDK only supplies `vulkan-1.dll` + the loader on PATH. The
  composite action has a `vulkan-provision: runtime` mode (~2 MB loader zip
  instead of the multi-GB SDK) — flip the default only after one green
  `workflow_dispatch` per heavy workflow verifies the LunarG zip layout.
- Android/AGDE is deliberately NOT in CI (AGDE VSIX absent from hosted
  runners); descriptor-level AGDE generation is exercised by every regen.
  Revisit with a self-hosted runner.
  **That gap has teeth.** Because nothing compiles the Android tree on a push,
  it bit-rots silently: an Android bring-up on 2026-08-03 found the agde build
  broken by MSVC-only `fopen_s`/`tmpfile_s` calls added the SAME day, plus a
  use-after-free in the Android thread bootstrap and several
  Windows-only-contract unit tests. All of it would have been a one-line CI
  failure. Until a runner exists, **build at least one agde config by hand
  before trusting the Android target**:
  ```
  msbuild Games\TilePuzzle\tilepuzzle_agde.sln /t:TilePuzzle ^
    /p:Configuration=Vulkan_x86_64_vs2022_Debug_Agde_False /p:Platform=Android-x86_64
  ```
  x86_64 is the cheap one to check because it also RUNS, on the local emulator.

## 9. Build hygiene and troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `MSB3027` "could not copy .pdb" / file-in-use | Hanging `cl.exe`/`mspdbsrv.exe` from an interrupted parallel build → `zenith clean` (or just retry `zenith build`, which self-heals stale compilers ≥30 min old). Don't fall back to single-threaded builds. |
| `.sln`/`.vcxproj` missing (fresh clone / branch switch) | Regenerate-first policy → `zenith regen` (`--check` to ask first). |
| MSBuild not found | Install VS2022 C++ workload or run from a Developer PowerShell; the CLI resolves via PATH then vswhere. |
| Exe dies at launch, `STATUS_DLL_NOT_FOUND` (0xC0000135) | Slang dependency tree / assimp missing next to the exe → `zenith test`/`zenith package` self-heal via `Repair-ZenithRuntimeDlls`; for a bare `zenith run`, build once via a game that has them or run `zenith test <Name> --filter <anything>` to trigger the heal. |
| Game hangs at shutdown (headless) | Known; harness/gates tally results from JSON and watchdog-kill. Don't wait on process exit for correctness. |
| ~~Whole-solution build red (TilePuzzle only)~~ — FIXED (ZEN-103, 2026-09-21) | `Games\TilePuzzle\tilepuzzle_win64.sln` used to be the one solution that failed whole: `TilePuzzleLevelGen` called removed/changed engine APIs and `TilePuzzleRegistryViewer` failed to link (missing `Project_*` composition-root symbols). Both were repaired against the current engine API (see "Whole-solution build status (ZEN-85, 2026-09-21)" below, and its ZEN-103 update) — the solution now builds whole with zero errors, same as every other measured solution. Both tools then crashed at launch (`0xC0000005`) — building is not running — until ZEN-107 gave a standalone tool a supported way to allocate the engine's runtime services (`Zenith_ToolRuntimeServices`); see the section below. |
| Regen refuses to run | You're in a linked git worktree that is not a registered [lane](../Tools/zagent/README.md#lanes) — run from the main checkout, or pass `-AllowLinkedWorktree` for an explicitly authorized one (§3). |
| Sentinel link error | A leaf lib grew an engine/platform reference. If it's a sanctioned L0 seam, add the shim to `Tests/Sentinel*/sentinel_platform.cpp`; otherwise fix the leak (that's the point of the proof). |
| `tick_gates.ps1` exits 11, "slot busy" | Another checkout — the main one, or another lane — holds the one machine-wide build slot (`Tools/build_lock.ps1`, named mutex `Local\ZenithBuildSlot`, capacity 1). Non-evidential: wait for it to clear, or check `<git-common-dir>/zenith-build-slot.json` for who holds it. |
| `tick_gates.ps1` exits 12, "tree unstable" | A gate wrote non-deterministic tracked output across two verify passes — `tick_gates.ps1` already retried once internally before reporting this. Fix the gate (find and freeze whatever it reads that isn't the authored value); re-running again is not a fix. |

**Exe sweep is path-scoped, not name-scoped.** `Tools/sweep_game_exes.ps1`
(the `cleanup` line every gate runs) dot-sources `Tools/tick_gates.ps1
-NoRun` to reuse its `Invoke-ExeSweep`/`Select-SweepTargets` — the same
killer every gate phase already uses — and kills only processes whose exe
path sits under *this* checkout's own tree. That matters once lanes are
in play: two lanes, or a lane and the main checkout, can each have their
own `zenithmon.exe` running at once, and a name-only kill (`Get-Process
zenithmon.exe | Stop-Process`) would take out both. Lane mode skips the
cleanup line entirely at its own step 9
(`Tools/zagent/protocol/TICK.md#lane-mode`) — a main-mode tick's step 9
runs after `finish` releases the repo lock, while a lane may still be
mid-build.

**Each [lane](../Tools/zagent/README.md#lanes) is its own full checkout's
worth of disk**, roughly 20-30 GB: `zagent lane add <PROJECT>` copies
every ignored runtime input the worktree needs (Slang/VulkanSDK/Sharpmake
binaries, every ignored file under each `Games/*/Assets` and
`Zenith/Assets`) on top of the worktree itself, then builds and
unit-gates every game. `zagent lane remove <PROJECT>` purges that lane's
own `Build/output`/`Build/obj`/`Build/artifacts` (root and per-game)
before removing the git worktree, so disk is reclaimed on an orderly
teardown; a lane abandoned by deleting its directory by hand does not
get that cleanup and also leaves a dangling `git worktree` entry.

`Tools/libgraph/` measures the boundaries between the engine libraries
listed in §4 — declared vs. used dependencies, each lib's public
header/type/link surface, leakage, and a swap contract — straight from the
generated `.vcxproj`/Sharpmake output and (optionally) a `dumpbin
/symbols` pass on a built Debug config. Run
`python Tools\libgraph\libgraph.py --open` (or add `--no-symbols` for a
header-only pass that needs no build first); the report lands at
`Build/artifacts/libgraph/index.html` (+ `libgraph.json`), both gitignored.
It is report-only — nothing here gates a build or a CI check; see
`Tools/libgraph/README.md` for how to read it.

### Whole-solution build status (ZEN-85, 2026-09-21)

The historical rule — "never build the whole solution, the aux tools are
pre-existing-red in `ToolsEnabled=True`" — is **stale**. Measured on this tree
2026-09-21 (`MSBuild.exe <sln> /p:Configuration=<cfg> /p:Platform=x64
-maxCpuCount`, `MSBUILDDISABLENODEREUSE=1`; "errors" = lines matching
`: error <CODE>`):

| Solution | Configuration | Exit | Compile/link errors |
|---|---|---|---|
| `Build/zenith_engine_win64.sln` | `Vulkan_vs2022_Debug_Win64_True` | 0 | 0 |
| `Build/zenith_engine_win64.sln` | `Vulkan_vs2022_Debug_Win64_False` | 0 | 0 |
| `Games/Combat/combat_win64.sln` | `Vulkan_vs2022_Debug_Win64_True` | 0 | 0 |
| `Games/TilePuzzle/tilepuzzle_win64.sln` | `Vulkan_vs2022_Debug_Win64_True` | 1 | 27 |

**Update (ZEN-103, 2026-09-21): TilePuzzle's whole-solution build is now
green too.** `TilePuzzleLevelGen` and `TilePuzzleRegistryViewer` were both
repaired against the current engine API (below) rather than retired —
`Games/TilePuzzle/tilepuzzle_win64.sln` in `Vulkan_vs2022_Debug_Win64_True`
now builds with the same "exit 0, zero errors" result as every other row in
the table above, closing the one exception this section used to document.

**Update (ZEN-111 / ZEN-D-044, fixB): this table's measurement is now WIRED
IN, not just recorded here.** Every row above was true and unenforced —
`zenith build engine` still ran only the scoped `/t:Zenith`, and
`zenith build TilePuzzle --headless` still ran only the scoped
`/t:TilePuzzle` — so neither `zenith_engine_win64.sln`'s own exe projects
(`FluxCompiler`, `ZenithHub`) nor `tilepuzzle_win64.sln`'s
`extraSharpmakeProjects` (`TilePuzzleLevelGen`, `TilePuzzleRegistryViewer`)
were ever gated: `fluxcompiler.exe` failed to link (LNK2005) in
`Vulkan_vs2022_Debug_Win64_True` while every Engine gate stayed green.
`zenith build engine` now runs `Build/zenith_engine_win64.sln` WHOLE (plus
the seven Sentinels via `/t:`, unchanged), `zenith build <Game> --whole` is
a new flag for the per-game case, and the Engine gate list
(`zagent.project.json`) adds
`zenith build TilePuzzle --config Vulkan_vs2022_Debug_Win64_True --whole`
after `build engine` — see §2's `build` command notes and §11 invariant 2.

### How a standalone tool obtains runtime services (ZEN-107, 2026-09-22)

**★ BUILDING WAS NOT RUNNING, and ZEN-103 delivered only the first.** Both tools
compiled, linked and produced an exe; both then **crashed with an access
violation (`0xC0000005`) on their first engine call**, measured on 2026-09-21
immediately after that fix:

| Tool | Invocation | Result (2026-09-21, BEFORE ZEN-107) |
|---|---|---|
| `tilepuzzlelevelgen.exe` | `--validate-registry --validate-max-states 2000` | exit `0xC0000005`, right after `Zenith_MemoryManagement::Initialise()` |
| `tilepuzzleregistryviewer.exe` | (no args) | exit `0xC0000005`, same point |

The cause was the same for both and was **not** API bit-rot: a standalone tool
had no way to allocate the engine's runtime services. `Zenith_Engine`'s
`m_pxThreading` / `m_pxTasks` / `m_pxProfiling` are private members `new`'d
only in `InitialiseRuntimeServices()`, which is **private** — the sole public
entry was `Initialise()`, the full boot (renderer, physics, ECS, editor) that a
CLI tool cannot run. So `g_xEngine.Threading()` in a tool dereferenced a null
pointer, and because `Threading`/`Tasks`/`Profiling` are declared with
`ZENITH_ENGINE_ACCESSOR_HOTPATH` — **no null assert**, deliberately, as a
hot-path choice — it faulted instead of reporting a clear message.

**The answer is a scoped guard, `Zenith_ToolRuntimeServices`**
(`Zenith/Core/Zenith_Engine.h`), over two new public entry points
(`Zenith_Engine::InitialiseToolRuntimeServices` /
`ShutdownToolRuntimeServices`). A tool's `main()` reads:

```cpp
Zenith_MemoryManagement::Initialise();        // the allocator stays the CALLER's
Zenith_ToolRuntimeServices xRuntimeServices;  // Threading + Profiling + Tasks
```

and the destructor tears the three down on every exit path — including the
early `return 1`s a CLI is full of. Both TilePuzzle tools now boot that way.

What makes this more than a visibility change: **`InitialiseRuntimeServices()`
is not purely runtime services.** Its first fifteen lines also call
`Project_SetGraphicsOptions()` (a game-project hook every tool stubs out) and
`Zenith_MemoryManagement::Initialise()` (which the tool has already called, on
the line above), and register memory-accounting poll callbacks that read
`Physics()` and `FluxMemory()` — subsystems a tool never allocates. Making it
public as-is would have dragged all of that into a CLI. Instead it was split
into two private halves, `AllocateThreadRegistry()` and
`InitialiseProfilingAndTaskWorkers()`, that the engine boot and tool mode both
call; the game-only work stays between them, in the engine path only. The
split is a pure extraction — `Zenith_Engine::Initialise()`'s phase order is
untouched and the statements run in the same sequence they always did.

Tool mode also allocates the **debug-variable tree**, which is not obvious and
is not optional: `Zenith_TaskSystem::Initialise()` registers its
`Multithreaded` toggle under `ZENITH_DEBUG_VARIABLES`, which `Zenith.h` defines
whenever `ZENITH_TOOLS` is — and a tool exe is a `ToolsEnabled=True` build. The
engine gets the tree from `AllocateEditorSubsystems()` two phases earlier; tool
mode runs no phases, so it allocates that one piece itself. It does **not**
allocate anything else, and `Zenith_MemoryManagement::Shutdown()` (the
whole-engine leak checkpoint) is deliberately not called on the tool path.

**The three accessors were left assert-free**, and that was a decision, not an
omission. `Zenith.h` defines `ZENITH_ASSERT` unconditionally — there is no
configuration where `Zenith_Assert` compiles away — so moving
`Threading`/`Tasks`/`Profiling` onto the asserting macro would add a branch to
every per-frame, per-worker-thread call in every shipping configuration, which
is exactly what the hot-path policy refuses. And there is no compile-time axis
that separates a tool exe from a game exe: both link the same `zenith.lib`,
built once per configuration. The diagnostic went where it costs the game loop
nothing instead — `Zenith_Engine::HasRuntimeServices()`, a public pointer
predicate, plus asserts inside the two tool-mode entry points.

Neither tool REGRESSED, and the two need separate arguments.
`TilePuzzleRegistryViewer` was already broken this way BEFORE ZEN-103: its boot
already used the modern `g_xEngine.Profiling()` / `g_xEngine.Threading()`
spelling and already dereferenced the same null pointers — it simply could not
be observed while the tool failed to link. (ZEN-103 did then touch those lines,
to correct a second, independent defect: the file had `Profiling().Initialise()`
BEFORE `RegisterThread(true)`, the reverse of the engine's order. `Initialise`
ends with `m_uMainThreadID = xThreading.GetMainThreadID()`, which is `~0u` until
`RegisterThread(true)` sets it, and it self-guards on `m_bInitialised` so a
later call cannot repair it — so the viewer would have recorded no main thread
at all. That one is latent today, masked by the crash, and would have surfaced
silently the moment runtime services became obtainable.)
`TilePuzzleLevelGen`'s crashing lines ARE lines ZEN-103 wrote, but it never
produced an exe to run beforehand, and its pre-diff
`g_xEngine.Profiling().Initialise()` would have null-dereferenced identically
had it compiled. The exception this section documents is a WHOLE-SOLUTION BUILD
exception, and that one is closed. The engine decision about how a standalone
tool obtains runtime services — the thing that was still missing when the
paragraphs above were written — is ZEN-107, recorded in the subsection above.
(The viewer's latent profiler-ordering bug is no longer latent-and-hand-fixed:
that ordering now lives inside
`Zenith_Engine::InitialiseProfilingAndTaskWorkers`, which both the engine boot
and every tool go through, so a tool cannot spell it wrong to begin with.)

**FluxCompiler and all three font-dependency projects build green.**
`FluxCompiler`, `FreeType`, `msdfgen` and `msdf-atlas-gen` all compiled with
zero errors in `Vulkan_vs2022_Debug_Win64_True`, in every solution they appear
in (msdfgen's C4100/C4458/C4505 are warnings only —
`TreatWarningsAsErrors.Disable` is set on that project). The "aux tools are
pre-existing-red" premise is false as of this measurement.

**The one genuinely red whole-solution build was TilePuzzle, for an unrelated
reason — fixed by ZEN-103 (2026-09-21).** `Games/TilePuzzle/tilepuzzle_win64.sln`
failed, and neither failing project was an aux tool — both are TilePuzzle's own
bespoke tooling:

- **`TilePuzzleLevelGen`** — 21 compile errors: it called engine APIs that no
  longer existed (`Zenith_TaskSystem::Inititalise` [sic], an undeclared
  `Zenith_TaskArray`, a changed `Zenith_Profiling::Initialise` signature, a
  no-longer-static `Zenith_Multithreading::RegisterThread`, an undeclared
  `ZENITH_PROFILE_INDEX__SCENE_UPDATE`). This was bit-rot against engine API
  drift, not a build-system defect. ZEN-103 repaired the boot sequence to
  match `Zenith_Engine::InitialiseRuntimeServices`'s current order
  (`g_xEngine.Threading().RegisterThread(true)` before
  `g_xEngine.Profiling().Initialise(g_xEngine.Threading())` before
  `g_xEngine.Tasks().Initialise()`) and migrated both `Zenith_TaskArray` call
  sites to `Zenith_DataParallelTask` + `g_xEngine.Tasks().SubmitDataParallelTask(...)`.
- **`TilePuzzleRegistryViewer`** — compiled, then failed to link: it links
  `zenith.lib`, which requires the composition-root symbols every game
  provides (`Project_SetGraphicsOptions`, `Project_RegisterGameComponents`,
  `Project_Shutdown`, `Project_InitializeResources`,
  `Project_RegisterEditorAutomationSteps`), and the viewer supplied only 2 of
  the 8. That surfaces as **6 link-error lines, but only 5 unresolved
  symbols**: `Project_LoadInitialScene` sits behind the non-tools `#else` and
  so is not referenced in a `_True` build, and `LNK1120` is the "N unresolved
  externals" SUMMARY line, not a sixth symbol. 21 + 6 is the 27 in the table
  above. ZEN-103 added the 5 missing stubs (mirroring `FluxCompiler.cpp`'s
  existing no-op pattern), plus `Project_LoadInitialScene` in both tools so
  they link in the `_False` sibling too. `TilePuzzleLevelGen` needed the same
  8-stub block once its compile errors were cleared (it previously defined
  only 2), confirming the link wall was latent behind the compile errors, not
  absent.

Neither project was retired: the registry they own is 668 committed
`.tlvl`/`.png` files under `TilePuzzleLevelGen/LevelRegistry/`, and retiring
the two tools would strand it with nothing able to regenerate or inspect it.
(That is an argument from what the tools OWN, not from what they currently do
— as the subsection above records, neither can presently run at all.)
`zenith build TilePuzzle` (`/t:TilePuzzle`) never built these two projects;
`zenith build TilePuzzle --whole` now does (fixB/ZEN-111, §2, §9's ZEN-111
update above) — the scoped form is unaffected either way, since the fix
only changes which whole-solution build passes.

**A `_True` solution build legitimately skips some projects, by target
declaration rather than by being broken.** From the `.sln` files: the engine
solution maps 43 projects to an `ActiveCfg` for `..._True` but only 34 to a
`Build.0` (9 skipped); Combat skips 2 of 36; TilePuzzle skips 2 of 38 (both of
TilePuzzle's own bespoke tools — `TilePuzzleLevelGen` and
`TilePuzzleRegistryViewer` — map a `Build.0` and are genuinely built; see the
ZEN-103 update above). Every skip in both counts is one of:

- `ZenithRenderBackendD3D12` / `ZenithRenderBackendNull` — each declares only
  its own `RenderBackend` fragment in `AddTargets`, so neither has a `Vulkan_*`
  configuration at all (the backend project-membership invariant, §4). Covered
  by the `D3D12_*` / `Null_*` configs instead.
- `SentinelAI`/`SentinelCore`/`SentinelECS`/`SentinelFlux`/`SentinelPhysics`/
  `SentinelScripting`/`SentinelUI` (engine solution only) — each declares
  `ToolsEnabled.False` only (the leaf-purity proof only makes sense outside
  tools), so none has a `_True` configuration. All seven build green in the
  `_False` sibling (`Vulkan_vs2022_Debug_Win64_False`, exit 0, second row of
  the table above).

None of these nine is broken; each is absent from one configuration by target
declaration and built by another. **Which other thing builds it differs, and
`zenith build engine` is not the answer for all nine:** it now builds
`Build/zenith_engine_win64.sln` WHOLE in the requested config (fixB/ZEN-111,
replacing the old scoped `/t:Zenith` step — see §2, §9) plus `/t:Sentinel*`
against the `_False` sibling, so it covers the seven Sentinels (stepping them
through the `_False` sibling of whatever config it was given) but builds
neither backend library — a whole `_True` build maps no `Build.0` for either,
same as the table above. The two backend libraries are covered by the
**Engine gate list** instead — `zenith build Combat --headless` builds the
`Null_` configuration, and `zenith build Combat --config
D3D12_vs2022_Debug_Win64_False` the `D3D12_` one.

## 10. Key defines (set by Sharpmake)

`ZENITH_TOOLS` (True configs), `ZENITH_WINDOWS`/`ZENITH_ANDROID`,
`ZENITH_DEBUG`, `ZENITH_VULKAN`/`ZENITH_D3D12` (exactly one),
`ZENITH_PROFILING_ENABLED`, `ZENITH_MEMORY_TRACKING_LEVEL` (2 Debug / 1
Release), `GAME_ASSETS_DIR`/`ENGINE_ASSETS_DIR`/`SHADER_SOURCE_ROOT` (absolute
baked paths — overridable at runtime by `--assets-root`, §7).

## 11. Invariants for contributors

1. **One entry point per operation; no legacy/compat code whatsoever.** No
   forwarder scripts, no alias remaps, no "deprecated" surfaces — migrate every
   caller and delete the old name in the same commit.
2. Prefer `/t:<target>` for a SCOPED build (faster, exactly what changed) —
   but this is no longer what every `zenith build` path always uses.
   `zenith build engine` builds `Build/zenith_engine_win64.sln` WHOLE (plus
   `/t:Sentinel*` against the `_False` sibling), and `zenith build <Game>
   --whole` does the same for a game's own sln — both deliberately, because a
   scoped `/t:Zenith`/`/t:<Game>` never built a sln's OWN exe projects or a
   game's `extraSharpmakeProjects` (ZEN-111/ZEN-D-044: `fluxcompiler.exe` and
   `tilepuzzlelevelgen.exe` failed to link while every Engine gate stayed
   green). A whole-solution build is supported and green for the engine
   solution and every game solution, including
   `Games\TilePuzzle\tilepuzzle_win64.sln` since ZEN-103 (see "Whole-solution
   build status (ZEN-85, 2026-09-21)" in §9). Default to `/t:<target>` for
   ad hoc scoped work; use `--whole` (or `build engine`'s built-in whole
   step) when the sln's OTHER projects — not just the named target — need to
   be verified, which is exactly what the Engine gate list now does.
3. Never run Sharpmake/regen from a linked worktree — except a registered
   [lane](../Tools/zagent/README.md#lanes) (§3).
4. Nothing generated is committed; nothing hand-written is ignored (enforced
   by selftest).
5. Runner/tool outputs go under `Build/artifacts/`; packages under `dist/`.
6. Config names/paths come from `zenith_config.psd1` accessors, not literals.
7. Engine unit-test baseline bumps edit ONE executable site, from an
   OBSERVED run (never arithmetic): the game's row in
   `Tools/unit_baselines.json`. A backend-neutral engine unit moves EVERY
   row there. The prose copies — this file's two mentions and
   `Games/Zenithmon/Docs/Status.md`'s top block — are narration read by no
   gate; update them for humans, but stale prose there reds nothing. (`Tools/test_scaffold.ps1`
   pins no number — it inherits the script default.)
8. Every new script function gets selftest coverage; every new engine
   type/method gets unit coverage.
