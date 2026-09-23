# Zenith Game Engine

## Overview

C++20 game engine with custom ECS, Vulkan-based renderer (Flux), and multi-threaded task system.

> **New to the codebase?** Read [Docs/Onboarding/NewcomerMap.md](Docs/Onboarding/NewcomerMap.md) first — it covers startup flow, recommended reading order, and the sharp edges that trip up first-timers.

## Directory Structure

```
Zenith/
├── ZenithECS/         # ECS leaf lib: entity/scene/query/event/component-meta machinery (see ZenithECS/CLAUDE.md)
├── EntityComponent/   # Concrete components + ECS<->engine glue (see EntityComponent/CLAUDE.md)
├── Scripting/         # Behaviour Graph runtime: visual scripting interpreter (see Scripting/CLAUDE.md)
├── Flux/              # Vulkan renderer (see Flux/CLAUDE.md)
│   ├── Decals/        # Deferred decals (see Flux/Decals/CLAUDE.md)
│   ├── Fog/           # Volumetric fog (see Flux/Fog/CLAUDE.md)
│   ├── Gizmos/        # Editor gizmos (see Flux/Gizmos/CLAUDE.md)
│   ├── HDR/           # HDR pipeline (see Flux/HDR/CLAUDE.md)
│   ├── HiZ/           # Hierarchical Z-buffer (see Flux/HiZ/CLAUDE.md)
│   ├── IBL/           # Image-based lighting (see Flux/IBL/CLAUDE.md)
│   ├── MeshAnimation/ # Skeletal animation (see Flux/MeshAnimation/CLAUDE.md)
│   ├── RenderGraph/   # Render graph compile/execute (see Flux/RenderGraph/CLAUDE.md)
│   ├── Shadows/       # Cascaded shadow maps (see Flux/Shadows/CLAUDE.md)
│   ├── SSR/           # Screen-space reflections (see Flux/SSR/CLAUDE.md)
│   ├── SSGI/          # Screen-space GI (see Flux/SSGI/CLAUDE.md)
│   ├── Terrain/       # Terrain rendering (see Flux/Terrain/CLAUDE.md)
│   ├── Vegetation/    # Grass system (see Flux/Vegetation/CLAUDE.md)
│   └── ...            # + Backend, DeferredShading, DynamicLights,
│                      #   InstancedMeshes, MaterialPreview, MeshGeometry, Particles,
│                      #   Present, Primitives, Quads, SDFs, SceneGraph, Shaders, Skybox,
│                      #   Slang, SSAO, UnifiedMesh, Text, Translucency, and more
├── AI/                # AI systems (see AI/CLAUDE.md); decisions live in Scripting/ graphs
│   ├── Navigation/    # NavMesh pathfinding (see AI/Navigation/CLAUDE.md)
│   ├── Perception/    # Sensory systems (see AI/Perception/CLAUDE.md)
│   └── Squad/         # Squad tactics (see AI/Squad/CLAUDE.md)
├── Editor/            # Editor tools (see Editor/CLAUDE.md)
├── TaskSystem/        # Task parallelism — lib ZenithTaskSystem (see TaskSystem/CLAUDE.md)
├── Physics/           # Jolt Physics integration (see Physics/CLAUDE.md)
├── RenderBackends/    # The three render backends, grouped (see RenderBackends/CLAUDE.md)
│   ├── Vulkan/        # Vulkan backend — lib ZenithRenderBackendVulkan, RenderBackend.Vulkan (win64+agde) (see RenderBackends/Vulkan/CLAUDE.md)
│   ├── D3D12/         # No-op null render backend (Flux neutrality proof) — lib ZenithRenderBackendD3D12, win64 ONLY (see RenderBackends/D3D12/CLAUDE.md)
│   └── Null/          # THE headless/CI backend — lib ZenithRenderBackendNull, win64 ONLY (see RenderBackends/Null/CLAUDE.md)
├── Core/              # Core utilities (see Core/CLAUDE.md)
├── EngineComposition/ # The composition root: TUs that wire subsystems together + own g_xEngine (see EngineComposition/CLAUDE.md)
├── Collections/       # Custom containers (see Collections/CLAUDE.md)
├── Maths/             # GLM wrapper (see Maths/CLAUDE.md)
├── AssetHandling/     # Asset import/export (see AssetHandling/CLAUDE.md)
├── Windows/           # Windows platform layer — lib ZenithWindows, win64 ONLY (see Windows/CLAUDE.md)
├── Android/           # Android platform layer — lib ZenithAndroid, agde ONLY (see Android/CLAUDE.md)
├── DataStream/        # Binary serialization (see DataStream/CLAUDE.md)
├── SaveData/          # Save/load persistence (see SaveData/CLAUDE.md)
├── DebugVariables/    # Runtime debug variable tree — lib ZenithDebugVariables (see DebugVariables/CLAUDE.md)
├── Input/             # Input handling — lib ZenithInput (see Input/CLAUDE.md)
├── Profiling/         # CPU profiling system — lib ZenithProfiling (see Profiling/CLAUDE.md)
├── Telemetry/         # Runtime telemetry capture
├── UnitTests/         # Unit-test harness — lib ZenithUnitTests (see UnitTests/CLAUDE.md)
├── Prefab/            # Prefab system (see Prefab/CLAUDE.md)
├── UI/                # UI framework (see UI/CLAUDE.md)
├── Tools/             # The asset bake: importers + procedural exporters (see Tools/CLAUDE.md)
└── FileAccess/        # File system abstraction (see FileAccess/CLAUDE.md)
```

## Dependencies

- Vulkan SDK 1.3
- Jolt Physics
- GLFW
- GLM
- ImGui (docking branch)

## Naming Conventions

### Class/Struct Names
- `PascalCase` with namespace prefix
- Core engine classes: `Zenith_ClassName` (e.g., `Zenith_Entity`, `Zenith_Scene`)
- Renderer classes: `Flux_ClassName` (e.g., `Flux_Texture`, `Flux_RenderGraph`)

### Variable Scope Prefixes
| Prefix | Meaning | Example |
|--------|---------|---------|
| `m_` | Member variable | `m_uEntityID` |
| `s_` | Static member | `s_xCurrentScene` |
| `g_` | Global variable | `g_pxAnimUpdateTask` |
| `ls_` | Local static | `ls_bOnce` |

### Type Prefixes (after scope prefix)
| Prefix | Type | Example |
|--------|------|---------|
| `x` | Struct/class instance | `m_xComponents`, `xEntity` |
| `p` | Pointer | `m_pData`, `pData` |
| `px` | Pointer to class/struct | `m_pxParentScene`, `pxAnim` |
| `a` | Array | `m_axColourAttachments`, `auIndices` |
| `u` | Unsigned int | `m_uWidth`, `uIndex` |
| `b` | Bool | `m_bInitialised`, `bSuccess` |
| `f` | Float | `m_fScale`, `fDeltaTime` |
| `str` | String | `m_strName`, `strPath` |
| `e` | Enum value | `m_eFormat`, `eType` |
| `pfn` | Function pointer | `m_pfnFunc` |
| `ul` | Unsigned long (u_int64) | `m_ulSize` |
| `i` | Signed int | `iIndex` |

### Functions
- `PascalCase` (e.g., `GetPosition`, `BuildModelMatrix`, `WriteToDataStream`)

### Enums
- `SCREAMING_SNAKE_CASE` with category prefix
- Values: `CATEGORY_NAME_VALUE` (e.g., `TEXTURE_FORMAT_RGBA8_UNORM`, `RESOURCE_ACCESS_READ_SRV`)

### Constants
- `SCREAMING_SNAKE_CASE` with type prefix (e.g., `uFLUX_MAX_TARGETS`, `fCHAR_ASPECT_RATIO`)

## Code Style

### Braces
Opening braces on new line for class, function, and control flow blocks. Short inline one-liners in headers may use same-line braces:
```cpp
class Zenith_Entity
{
public:
    void DoSomething()
    {
        if (condition)
        {
            // code
        }
    }

    // Short inline getters may use same-line braces
    bool IsValid() const { return m_bValid; }
};
```

### Indentation
- Tabs (not spaces)

### Class Layout
1. `public:` members first
2. `protected:` members
3. `private:` members last

### Headers
- Use `#pragma once` for include guards
- Avoid `using namespace` in headers

### Conditionals
- Tools-only code wrapped in `#ifdef ZENITH_TOOLS`

### Precompiled Header
- All .cpp files must begin with `#include "Zenith.h"`

## Build System

The project uses [Sharpmake](https://github.com/ubisoft/Sharpmake) to generate Visual Studio solution files.

> This section is the quick-start subset. The full reference — CLI commands +
> exit codes, configurations/outputs/PCH structure, the test harness and
> baselines, packaging (`--assets-root`), the CI matrix, hygiene and
> troubleshooting, contributor invariants — is
> **[Docs/BuildSystem.md](Docs/BuildSystem.md)**. The descriptor system
> (`.zproj` schema, codegen, templates, hub) is
> **[Docs/GameProjects.md](Docs/GameProjects.md)**.

### Generating Solution Files

Games are **descriptor-driven**: each game has a `Games/<Name>/<Name>.zproj` (JSON —
`schemaVersion`, `name`, `android`, optional `extraDefines`/`extraSharpmakeProjects`).
Adding or removing a game touches only its descriptor — **no Sharpmake C# edits**.

Regenerate every solution from the repo root (or `Build/`):
```batch
Build\regen.ps1
```
(`zenith regen` forwards here.) regen.ps1 validates all descriptors, codegens
`Build/Sharpmake_GameInstances.generated.cs`, runs Sharpmake once, and fixes up AGDE.

This generates **per-game solutions** plus one **engine-only** solution — there is
NO all-games solution:
- `Games/<Name>/<name>_win64.sln` — one per game (+ `<name>_agde.sln` when `android:true`)
- `Build/zenith_engine_win64.sln` — engine libs + Sentinels + tools + ZenithHub, **zero games**

**Regenerate-first policy:** EVERYTHING Sharpmake emits is gitignored — all `.sln`,
`.vcxproj`, `.vcxproj.filters`, `.vcxproj.user`, and the generated `.cs`. After a fresh
clone, or a checkout/pull that touches any `.zproj` or `Sharpmake_*.cs`, run
`Build\regen.ps1` (or `zenith regen`) before building — generated projects on disk are
untracked and can go stale silently (`zenith regen --check` reports staleness). Test
results, telemetry, and build logs are also never committed; runners write under
`Build/artifacts/`. See **`Docs/GameProjects.md`** for the schema, validation rules,
sln inventory, CI mapping, and troubleshooting.

**The library structure's invariant (Phase 4h, complete):** every compiled
source belongs to exactly one project per link target. Conditionality is
expressed as target declaration (a project's own `AddTargets`) plus solution
membership (a `Func<ZenithTarget,bool>` predicate in
`Build/Sharpmake_EngineLibs.cs`) — never as a per-configuration source
exclude (`conf.SourceFilesBuildExcludeRegex`). `ConfigurePlatformExcludes`,
the last mechanism of that kind, is gone; `Tools/check_project_membership.ps1`
enforces the invariant fatally by default (duplicate-owned / orphan /
unowned / stray build-exclude findings all exit 1 — pass `-ReportOnly` for
the old always-succeed behaviour). See **`Docs/BuildSystem.md`** §4 for the
full library list and the Phase 4a-4h history.

### Build Configurations

Each project supports these configurations. Every win64 config is prefixed by the
render backend: `Vulkan_` (the real renderer), `Null_` (the GPU-less backend that
**every headless run executes on**), or `D3D12_` (a reserved no-op backend kept as
the link-neutrality proof). The table shows the `Vulkan_` rows in full; the other
two prefixes offer the same four combinations. **agde carries the `Vulkan_` prefix
too, plus an ABI fragment** — `Vulkan_arm64_v8a_…` (devices) and `Vulkan_x86_64_…`
(the emulator, the only ABI a machine with no ARM device can run).

> **HEADLESS IS A BUILD CONFIG, NOT A FLAG.** There is no `--headless`. A `Null_*`
> config defines `ZENITH_NULL_RENDERER`, compiles `Zenith/RenderBackends/Null` instead of Vulkan,
> and creates its window hidden. Every render path still RUNS — pass callbacks,
> uploads, the editor ImGui frame — against no-op backend calls, so a headless run
> exercises the same code a windowed one does. `zenith build|test <G> --headless`
> selects this config. Compile-time checks use `Zenith_IsNullRenderer()` (or
> `#ifdef ZENITH_NULL_RENDERER` where one side cannot compile).

**Backend selection is project membership per target, not a per-configuration
source exclude.** Each of `Zenith/RenderBackends/{Vulkan,D3D12,Null}` is its
own static library (`ZenithRenderBackendVulkan`/`…D3D12`/`…Null`) that
declares only the `RenderBackend` fragment(s) it supports; a `Null_*`
configuration simply has no Vulkan or D3D12 *project* in its solution — there
is no source file to build-exclude, because there is no project membership to
begin with. See `Zenith/RenderBackends/CLAUDE.md`.

| Configuration | Platform | Tools | Description |
|--------------|----------|-------|-------------|
| `Vulkan_vs2022_Debug_Win64_True` | Windows | Yes | Debug build with editor/tools |
| `Vulkan_vs2022_Debug_Win64_False` | Windows | No | Debug build, runtime only |
| `Vulkan_vs2022_Release_Win64_True` | Windows | Yes | Release build with editor/tools |
| `Vulkan_vs2022_Release_Win64_False` | Windows | No | Release build, runtime only |
| `Null_vs2022_Debug_Win64_True` | Windows | Yes | **The headless/CI build** — GPU-less, hidden window |
| `D3D12_vs2022_Debug_Win64_False` | Windows | No | Reserved-backend link/neutrality proof (+ _True / Release variants) |
| `Vulkan_arm64_v8a_vs2022_Debug_Agde_False` | Android (`Android-arm64-v8a`) | No | Android debug, physical devices |
| `Vulkan_arm64_v8a_vs2022_Release_Agde_False` | Android (`Android-arm64-v8a`) | No | Android release, physical devices |
| `Vulkan_x86_64_vs2022_Debug_Agde_False` | Android (`Android-x86_64`) | No | **The emulator build** (+ Release variant) |

### Projects

| Project | Description | Output |
|---------|-------------|--------|
| Zenith | Core engine library | Static lib (.lib) |
| FluxCompiler | Shader compiler utility (Windows only) | Executable (.exe) |
| ZenithTools | Asset tools (Windows only) | Executable (.exe) |
| Game projects | Combat, TilePuzzle, RenderTest, CityBuilder (SimCity/C:S-style — see Games/CityBuilder/CLAUDE.md), DevilsPlayground, Zenithmon (monster-collecting RPG — see Games/Zenithmon/CLAUDE.md), ScriptTest (zero-gameplay-C++ Behaviour Graph proving ground — see Games/ScriptTest/CLAUDE.md) | Executable (.exe) / Shared lib (.so) |

### Building and Running

**The `zenith` CLI (recommended):**
```batch
zenith new <Name>          REM scaffold a new game (regen + open its sln)
zenith build Combat       REM msbuild the game's per-game sln (/t:<Game>)
zenith run Combat         REM launch the newest built exe
zenith test Combat        REM run the game's automated tests (or: zenith test all)
zenith open Combat        REM regen + open the game's sln in Visual Studio
zenith list                REM list games + built configs
zenith clean Combat       REM kill hanging build processes + wipe output/obj
zenith package Combat     REM stage a relocatable build into dist/ (run.bat --assets-root)
zenith regen --check       REM report whether on-disk generated files are stale
zenith hub                 REM launch the Unity-Hub-style GUI launcher
```

**Direct msbuild (per-game solution):**
```batch
msbuild Games\Combat\combat_win64.sln /t:Combat /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
Games\Combat\Build\output\win64\vulkan_vs2022_debug_win64_true\combat.exe
```
Prefer `/t:<Game>` over a whole-solution build — it's faster and scopes exactly
what changed, and it's what `zenith build` does. A whole-solution build is not a
workaround for red aux tools any more: FluxCompiler and the font libs build
green in `ToolsEnabled=True`, and `Build\zenith_engine_win64.sln` and
`Games\Combat\combat_win64.sln` both build whole with zero errors in
`Vulkan_vs2022_Debug_Win64_True` (ZEN-85, measured 2026-09-21). Since ZEN-103
(2026-09-21) `Games\TilePuzzle\tilepuzzle_win64.sln` builds whole too, so there
is no remaining exception — its two bespoke tools (`TilePuzzleLevelGen`,
`TilePuzzleRegistryViewer`) were repaired against the current engine API. They
BUILD but do NOT yet RUN: both crash at launch (`0xC0000005`) because a
standalone tool has no public way to allocate the engine's runtime services.
See `Docs/BuildSystem.md` §9 for both measurements.

> **Config-name prefix (RenderBackend fragment):** every win64 config is prefixed
> with the render backend — `Vulkan_vs2022_Debug_Win64_True` (the real renderer),
> `Null_vs2022_Debug_Win64_True` (the GPU-less headless/CI backend; see
> `Zenith/RenderBackends/Null/CLAUDE.md`), or `D3D12_vs2022_Debug_Win64_False` (the reserved
> backend kept as a link-neutrality proof; see `Zenith/RenderBackends/D3D12/CLAUDE.md`). The
> output dir is the lowercased config name. AGDE configs carry the Vulkan prefix
> plus an ABI token (for example `Vulkan_arm64_v8a_vs2022_Debug_Agde_False`).

**Using Visual Studio:** `zenith open <Name>` regenerates then opens
`Games/<Name>/<name>_win64.sln`; set the game as startup project, pick a config
(e.g. `Vulkan_vs2022_Debug_Win64_True|x64`), F5.

**Engine-only work:** `msbuild Build\zenith_engine_win64.sln /t:Zenith` (or
`/t:FluxCompiler` / `/t:Sentinel*` / `/t:ZenithHub`); `zenith build engine` does this.
`zenith build engine` uses `/t:` for speed and because it needs to span both
`ToolsEnabled` configs in one call (Sentinels exist only in `_False`); a whole
`zenith_engine_win64.sln` build is supported and green in
`Vulkan_vs2022_Debug_Win64_True` — see `Docs/BuildSystem.md` §9.

### Build Troubleshooting

#### Hanging Compiler Processes

MSBuild can sometimes leave hanging `cl.exe` (compiler) and `mspdbsrv.exe` (debug database server) processes that lock build files, preventing subsequent builds from succeeding.

**Symptoms:**
- Build fails with "cannot access file" or "file is being used by another process" errors
- Multiple retries to copy `.pdb` files fail
- Errors like: `error MSB3027: Could not copy "zenith_compiler.pdb"`

**Cause:**
Parallel builds (`-maxCpuCount`) or interrupted builds can leave compiler subprocesses running. These processes hold locks on `.pdb`, `.pch`, and `.obj` files.

**Solution:**

```batch
zenith clean                  REM kill hanging cl/mspdbsrv/link/vctip/msbuild
zenith clean Combat          REM ...and also wipe that game's output/ + obj/
zenith clean engine           REM ...engine + hub intermediates
zenith clean --processes-only REM just the process sweep
```

`zenith build` also self-heals on entry: it kills compiler processes older than
30 minutes (genuinely hung ones only — a live concurrent build is never touched)
before invoking MSBuild, and accepts `--timeout <min>` as a watchdog that kills
the msbuild tree if a build wedges. (Implementation:
`Stop-ZenithBuildProcesses` in `Build/zenith_buildsystem.psm1`.)

**Prevention:**
- Builds use parallel compilation (`-maxCpuCount`, all cores). If a parallel build leaves hanging processes, run `zenith clean` before retrying — don't fall back to single-threaded
- Always let builds complete or use Ctrl+C to properly terminate
- Close Visual Studio when building from command line to avoid file conflicts

### Key Defines

Set automatically by the build system:

| Define | Description |
|--------|-------------|
| `ZENITH_TOOLS` | Enables editor/tools code (True configurations) |
| `ZENITH_WINDOWS` | Windows platform build |
| `ZENITH_ANDROID` | Android platform build |
| `ZENITH_DEBUG` | Debug build |
| `ZENITH_VULKAN` | Vulkan renderer enabled |
| `GAME_ASSETS_DIR` | Absolute path to game assets |
| `ENGINE_ASSETS_DIR` | Absolute path to engine assets |
| `SHADER_SOURCE_ROOT` | Path to shader source files |

### Sharpmake Files

Located in `Build/`:
- `Sharpmake_Common.cs` - Base project class, platform configuration (no `[Sharpmake.Main]`)
- `Sharpmake_Zenith.cs` - Zenith engine project
- `Sharpmake_FluxCompiler.cs` - Shader compiler project
- `Sharpmake_Games.cs` - Abstract `GameProject` / `GameSolution` bases (concrete per-game classes are generated)
- `Sharpmake_Solutions.cs` - `[Sharpmake.Main]` + SHA256 manifest guard + engine-only `ZenithEngineSolution`
- `Sharpmake_GameInstances.generated.cs` - **generated** per-game project/solution shells + manifest (gitignored)
- `Sharpmake_ZenithHub.cs` - Unity-Hub-style launcher project (engine sln only)
- `zenith_buildsystem.psm1` - descriptor scan / validate / codegen / name-validation + shared ops (config accessors, process sweep, DLL heal, drift check, orphan prune) — single source of truth
- `zenith_config.psd1` - central config DATA (default/hub configs, CI-provisioned versions, artifact root); read via the psm1 accessors only
- `regen.ps1` - canonical regenerator (worktree-guard -> validate -> codegen -> one Sharpmake run -> AGDE fixup -> orphan prune)
- `fix_agde_vcxproj.ps1` - AGDE post-Sharpmake fixup (clang c++ standard token)
- `Sharpmake_EngineLibs.cs` - single source-of-truth table of engine lib projects (`ZenithEngineLibs.All`), consumed by both solutions + the aggregate's dependency block
- `Sharpmake_ZenithCore.cs` - Core module project (`Zenith/Core` + `Zenith/Collections` headers; compiles the shared `Zenith.pch`; mandatory dependency of every other engine lib/Sentinel)
- `Sharpmake_ZenithMaths.cs` / `Sharpmake_ZenithDataStream.cs` / `Sharpmake_ZenithFileAccess.cs` - Maths/DataStream/FileAccess module projects (no PCH — below the ≥4-TU threshold)
- `Sharpmake_ZenithDebugVariables.cs` / `Sharpmake_ZenithTaskSystem.cs` / `Sharpmake_ZenithUnitTests.cs` - DebugVariables/TaskSystem/UnitTests module projects (no PCH — below the ≥4-TU threshold)
- `Sharpmake_ZenithInput.cs` - Input module project (4 TUs; own PCH, Create TU `Zenith/Input/Internal/Zenith_InputPCH.cpp`)
- `Sharpmake_ZenithWindows.cs` - Windows platform module project (**win64 ONLY**; owns GLFW + the `imgui_impl_glfw`/`imgui_impl_win32` ImGui backend TUs; own PCH, Create TU `Zenith/Windows/Internal/Zenith_WindowsPCH.cpp`)
- `Sharpmake_ZenithAndroid.cs` - Android platform module project (**agde ONLY**; own PCH, Create TU `Zenith/Android/Internal/Zenith_AndroidPCH.cpp`)
- `Sharpmake_ZenithAI.cs` - AI module project
- `Sharpmake_ZenithECS.cs` - ECS module project
- `Sharpmake_ZenithPhysics.cs` - Physics module project (3 TUs + PCH create TU; depends on `ZenithJolt`, no longer owns the vendored Jolt sources — Phase 4c)
- `Sharpmake_ZenithJolt.cs` - vendored Jolt Physics module project (`Middleware/JoltPhysics-5.4.0`, 133 TUs; **no PCH ever** — Jolt never includes `Zenith.h`; no Sharpmake dependency of its own)
- `Sharpmake_ZenithSaveData.cs` / `Sharpmake_ZenithTelemetry.cs` / `Sharpmake_ZenithPrefab.cs` - SaveData/Telemetry/Prefab module projects (1 TU each, no PCH — below the ≥4-TU threshold)
- `Sharpmake_ZenithScripting.cs` - Scripting (Behaviour Graph) module project (5 TUs; own PCH, Create TU `Zenith/Scripting/Internal/Zenith_ScriptingPCH.cpp`)
- `Sharpmake_ZenithImGui.cs` - vendored ImGui CORE module project (`Middleware/imgui-docking`'s 5 top-level TUs only — none of `backends/`; **no PCH ever**, same reasoning as `Sharpmake_ZenithJolt.cs`; no Sharpmake dependency of its own; targets win64 + agde, unlike Jolt)
- `Sharpmake_ZenithAssetHandling.cs` - AssetHandling module project (21 TUs; own PCH, Create TU `Zenith/AssetHandling/Internal/Zenith_AssetHandlingPCH.cpp`; declares no dependency on `ZenithFlux` despite 11 files `#include`ing `Flux/**` headers — see its own header comment and `AssetHandling/CLAUDE.md`)
- `Sharpmake_ZenithFlux.cs` - Flux renderer module project (84 TUs; own PCH, Create TU `Zenith/Flux/Internal/Zenith_FluxPCH.cpp`; declares `Flux -> AssetHandling`, the heavier direction of a genuine header cycle between the two — see `Flux/CLAUDE.md`; owns `slang.lib` conditionally on `win64 && RenderBackend.Vulkan`)
- `Sharpmake_ZenithRenderBackendVulkan.cs` / `Sharpmake_ZenithRenderBackendD3D12.cs` / `Sharpmake_ZenithRenderBackendNull.cs` - one target-restricted library per render backend (`Zenith/RenderBackends/{Vulkan,D3D12,Null}`); each declares `-> ZenithFlux`, never the reverse
- `Sharpmake_ZenithUI.cs` - UI module project (14 TUs; own PCH, Create TU `Zenith/UI/Internal/Zenith_UIPCH.cpp`; layer 5 — declares `-> ZenithFlux`/`ZenithAssetHandling`/`ZenithInput`/`ZenithProfiling`; needed `SHADER_SOURCE_ROOT` through the shared PCH chain alone, with no direct `#include` of the header that requires it — see `UI/CLAUDE.md`)
- `Sharpmake_ZenithEntityComponent.cs` - EntityComponent module project (43 TUs; own PCH, Create TU `Zenith/EntityComponent/Internal/Zenith_EntityComponentPCH.cpp`; layer 6, the top of the measured header DAG — declares `-> ZenithFlux` (the heavy direction of the declared `EntityComponent <-> Flux` cycle), `ZenithECS`, `ZenithPhysics`, `ZenithAI`, `ZenithScripting`, `ZenithPrefab`, `ZenithUI`; the real `EntityComponent <-> Editor` edge is deliberately UNDECLARED in EITHER direction — see `EntityComponent/CLAUDE.md`)
- `Sharpmake_ZenithEditor.cs` - Editor module project (56 TUs; own PCH, Create TU `Zenith/Editor/Internal/Zenith_EditorPCH.cpp`; **win64 x ToolsEnabled.True ONLY** — no `agde` target, and `ToolsEnabled` itself carries a single value, not `True | False`; declares `-> ZenithWindows` + everything `ZenithEntityComponent` depends on except `ZenithEntityComponent` itself, which stays deliberately UNDECLARED the other way too — see `Editor/CLAUDE.md`)
- `Sharpmake_ZenithTools.cs` - repo-root `Tools/` asset-bake module project (17 TUs; own PCH, Create TU `Tools/Internal/Zenith_ToolsPCH.cpp`; **win64 x ToolsEnabled.True ONLY**, same target restriction as `ZenithEditor`; adopted an orphan — this project generated a `.vcxproj` since before Phase 4g but was added to no solution; declares `-> ZenithFlux` (MeshGeometry) + `ZenithEntityComponent` + `MsdfAtlasGenProject` — see `Tools/CLAUDE.md`)
- `Sharpmake_SentinelAI.cs` / `Sharpmake_SentinelECS.cs` / `Sharpmake_SentinelPhysics.cs` - Sentinel test modules
- `Sharpmake_FreeType.cs` / `Sharpmake_Msdfgen.cs` / `Sharpmake_MsdfAtlasGen.cs` - Font/text dependency projects
- `Sharpmake_TilePuzzleLevelGen.cs` / `Sharpmake_TilePuzzleRegistryViewer.cs` - TilePuzzle tooling projects

## External agent board (projects `ZM`, `ZEN`, `DP`)

Some work in this repo is queued and executed by an autonomous Claude
Code loop driven from a Jira-style board. **The board is a web
application that may run on an entirely different machine**, and nothing
here knows where its source lives. This repo remains the source of truth
for everything — the board is a queue and an audit log, not a spec. You
can develop here and ignore it entirely, with three exceptions.

**Everything the loop enforces lives HERE, committed:**

| File | Carries |
|---|---|
| `zagent.project.json` | Gate command lines, pinned unit baselines, per-category conventions, branching mode, living-doc directories. **Sent to the board with every request** |
| `Tools/zagent/protocol/TICK.md` | The `/tick` protocol — nine steps, seven invariants. **Harness-neutral**: it names no harness, tool or model, only the five control verbs (`DISPATCH`/`AWAIT`/`ABORT`/`SCHEDULE`/`STOP`), the two roles (`WORKER`/`REVIEWER`) and the three tiers (`haiku`/`sonnet`/`opus`). Resolved into concrete mechanism by three per-harness entry points: `.claude/commands/tick.md` (Claude Code, `/tick`), `.opencode/commands/tick.md` (OpenCode, `/tick`), `.codex/skills/tick/SKILL.md` (Codex, invoked `$tick`) — all three GENERATED, never hand-edited; see `Tools/zagent/CLAUDE.md` |
| `Tools/zagent/` | There is exactly one backend: `zagent.ps1` is a thin PowerShell dispatch shim that execs `node Tools/Vault/cli/client.mjs --vault <repoRoot>` with argv/env passed straight through, and exits — nothing else in this directory runs in a default checkout, and there is no config file to pick a backend because there is nothing left to pick between. It carries: `zagent.ps1` (the dispatch shim above), `ZagentClient.psm1` (pure helpers used by `build_prompt.ps1`), `build_prompt.ps1` (assembles a tick's worker prompt deterministically — invoked directly with `pwsh -File`; see `Tools/zagent/protocol/TICK.md` step 5), `zagent.cmd` shim, and two assert scripts — `Test-ZagentClient.ps1`, `Test-BuildPrompt.ps1` — each prints its own count; do not pin one here, this line has been stale twice. `Tools/Vault/cli/tick-checks.mjs` is the sole implementation of the tick-protocol checks (excursions, commit completeness, `blocked-scope`, gate-verdict freshness), wired into `Tools/Vault/cli/write.mjs` and `client.mjs` — there is no second copy anywhere. It also carries `protocol/`: the canonical role charters (`protocol/roles/{worker,reviewer}.md`) and their per-harness generated copies (`protocol/roles/generated/*.resolved.md`), the three harness profiles as data (`protocol/harnesses.json`), the shared entry-point template (`protocol/entrypoint.md.tmpl`), and `protocol/tier-mentions.allowlist`; the generator that turns all of that into each harness's own entry point and role files (`Build-HarnessArtifacts.ps1`, `-Harness <id>`, `-Check` for staleness); and the gate that checks the three harnesses stay in parity (`Test-HarnessParity.ps1`). **No Node, no `node_modules` in this directory itself** — Vault is Node and lives in `Tools/Vault/` |
| `.zagent/` | Run scratch (`last.json`, `run/<KEY>/`). **Gitignored**, so the dirty-tree precondition still holds. A `claim`/`next` **through `zagent`** archives an existing `run/<KEY>/` to `run/<KEY>.prev/` (replacing an older `.prev`) before writing the fresh scratch, and reports `archivedPriorRun` on the result. The `npm run vault` entry point does not archive |

The board keeps only POLICY — the agent account, the complexity→model
routing, guardrail floors — and holds no path into this filesystem.
`zagent.project.json` declares which board projects this checkout serves;
that inversion is the only direction that survives the two being on
separate machines.

**One board project per game area.** `ZM` is Zenithmon, `ZEN` the engine,
`DP` DevilsPlayground — each declared by its CATEGORY in
`zagent.project.json`, with `ZEN` as the checkout default. Each area wants
its own epics, sprints, releases and burndown, which is what a project is
for; a component could carry none of them.

**Three more joined, bootstrapped from their TDDs (2026-09): `UV`
Undervault, `HE` Hearth, `FD` Foundry.** Each has docs but no `.zproj`
yet, so each project carries ONE milestone epic + ONE PLANNED sprint per
documented milestone and nothing else. The bootstrap transition: the
game's first ticket is THE SCAFOLD (`<Name>.zproj`, directory skeleton, a
CLAUDE.md carrying the TDD's conventions), its category gates
(`Build\regen.ps1` + `zenith build <Name>` + `zenith test <Name>`)
FAIL until that diff lands and pass exactly when it is real — so the
scaffold needs no protected-path edit, ever — and nothing starts a game
sprint or files implementation tickets until a human starts the first
one. Two HUMAN edits follow a landed scaffold: a `build <Name>` line
joins the Engine gate list (a `.zproj`-carrying game missing from it
fails `Test-ZagentClient.ps1`), and the game's baseline row joins
`Tools/unit_baselines.json` when its first units land. The full
transition is `packages/agent/OPERATIONS.md` §5 in the board repo.

**I5 is unaffected, and that is deliberate.** Every project this file
declares resolves to the SAME checkout, so the advisory lock and the
in-flight check still mean **one ticket at a time for `C:\dev\Zenith`**,
not one per board — except with [lanes](Tools/zagent/protocol/TICK.md#lane-mode)
enabled, where the scope narrows to "main, or this lane": each registered
lane may legitimately hold one in-flight ticket of its own alongside the
main checkout's. `zagent next` with no `--project` walks every declared
board in declaration order and reports "nothing to claim" only when every
one is empty.

**Blockers are mechanical now.** A `BLOCKS` link between tickets is the
one thing on the board that changes what the loop DOES: the claim query
refuses any ticket whose predecessor has not reached a DONE-category
status, and a targeted claim on one comes back exit 4 naming the ticket
to finish first. Zenithmon's R1-x chain used to be a `deps` column in a
Markdown table inside `Status.md`, which nothing could enforce.

Which is exactly why **a `blocks` link REQUIRES `--reason`** and the
other verbs do not. Nothing can verify the judgement — "does ZM-55
really need ZM-49 first" is not computable — so the mechanism is to make
the reasoning exist and put it where the block bites: in the refusal a
claim comes back with, in `zagent blocked`, and in `zagent links`. ZM-55
sat behind ZM-49 on reasoning that did not hold, and the refusal said
only "blocked by ZM-49", so the person who could have falsified it never
saw what it was. If the reasoning does not hold, `zagent unlink` is the
fix — not an edit to the blocked ticket.

The file is **sent every time rather than registered once**. A stored
copy goes stale, and a stale gate list is exactly how a green run
ratchets the wrong pinned baseline. The board keeps a mirror purely so
its web UI and file-time `create` validation can see categories.

★ **CORRECTION, not an update — the sentence this replaces was false as
implemented.** `protectedPaths` from both sides is **not** a union: `zagent
guard` calls `checkProtectedPaths(index.config.policy.resolved, changed)`
(`Tools/Vault/cli/local.mjs`), and `guardrailsOf`
(`Tools/Vault/cli/compat.mjs`) resolves the list as `project.protectedPaths
?? boardGuardrails ?? clientFile.protectedPaths` — a **fallback chain**,
not a union. `boardGuardrails` (`vault.config.json`'s
`policy.board.guardrails`, mirrored into `policy.resolved.guardrails`) is
always present, so `zagent.project.json`'s own `protectedPaths` **never
wins** and enforces nothing on its own; it has only ever survived by
staying a strict subset of the board's list. So this repo cannot add a
protection of its own that the board doesn't already have, and dropping
one of the board's protections from `zagent.project.json` alone would do
nothing — the fallback would still read `boardGuardrails`. `vault.config.json`
— the file that actually holds the floor — was **not itself protected**
until this round (`agent.config*.json` in the old list is a glob that does
not match `vault.config.json`'s literal name); it is now. Both files are
still protected in the ordinary sense: the loop may file a Suggestions row
about its own gates, never commit a change to either.

**A pinned unit baseline has exactly ONE home: `Tools/unit_baselines.json`.**
Gate lines and workflows name the GAME — `-Game Zenithmon` — and
`run_unit_gate.ps1` resolves the number, deriving the game from the `-Exe`
path when `-Game` is omitted. An explicit `-Baseline N` still wins, so any
caller that passed a number keeps working.

| | |
|---|---|
| The number | `Tools/unit_baselines.json` — **the only place it is written** |
| Who reads it | `zm-tests.yml`, `engine-gate.yml`, `test_scaffold.ps1`, `zagent.project.json`, all by game name |
| Human narration | `Games/*/Docs/Status.md` — read by no gate; stale there reds nothing |

It used to be duplicated across `Status.md`, `.github/workflows/*.yml`,
`run_unit_gate.ps1`'s default and `zagent.project.json`. Two of those are
`protectedPaths`, which made **bumping a pin work the agent loop could
never do** — so every test-adding ticket was unreachable, in a repo whose
conventions ask for a test with all new code. The gate asserts
`ran == Baseline` EXACTLY, so a stale pin still reds a required check with
**zero failing tests**; the difference is that fixing it is now one line in
one unprotected file.

A backend-neutral engine unit still moves **every** game's row, because
they all boot the same engine suite — bump them together, and note which
count you actually measured versus inferred.

**Branching policy is enforced mechanically per area.** Zenithmon, Engine
and DevilsPlayground tickets all carry `branching: "direct"` — the loop
commits straight to `master` and hard-refuses `git switch -c`,
`git worktree` and `gh pr` (ZM-D-031). DevilsPlayground no longer
overrides this to `"branch"`. Nothing is ever pushed. The one exception
is [lane mode](Tools/zagent/protocol/TICK.md#lane-mode): every lane
ticket gets its own `agent/<KEY>` branch regardless of `branching`, cut
in the lane's own worktree, never the main checkout.

**Leave `master` clean.** The loop's precondition check treats a dirty
tree as fatal and refuses the ticket rather than stashing around it — it
has to, since `direct` mode commits in place. Uncommitted work here
silently blocks the queue. In lane mode this still holds for the
**main** checkout specifically — a lane's own precondition checks its own
worktree, but a human's uncommitted edits in main still block a lane's
`integrate` (`MAIN_DIRTY`); see "Parallel lanes" below.

Gate command lines are copied VERBATIM from this repo's own docs and CI
(`.github/workflows/{zm-tests,engine-gate,dp-tests}.yml`,
`Tools/run_unit_gate.ps1`), never paraphrased. If you change how a game
is built or tested, `zagent.project.json` is a downstream consumer in
the same commit.

**WHICH gate lines run is decided by the DIFF, not by the filing
category.** Each category declares the directories it owns (`paths`), and
`zagent gates <KEY>` unions in the gates of every category the change
actually reached — so a ticket filed `Zenithmon` that edits `Zenith/**`
builds and tests Combat and checks the engine pin as well. It only ever
ADDS; the category's own list stays a prefix. `zagent guard <KEY>`
re-derives that selection and refuses a stale one, which is what keeps it
from being something a tick has to remember.

Four consecutive tickets edited engine code under a one-game gate list
before this existed. ZM-50's own Goal said *"the fix is engine-side"*
while `category: Zenithmon` meant the gates never built Combat — the body
and the category disagreed and only the category had any mechanical
effect. Adding `Zenith/**` to a category's `paths` costs those gates on
every ticket that touches it, which is the trade; `Tools/**` is
deliberately NOT listed, because a one-line pin bump in
`Tools/unit_baselines.json` does not need Combat rebuilt.

**AN ENGINE CHANGE COMPILES EVERY GAME**, and that is a separate rule the
union cannot supply. `Games/Combat` — once the only game the Engine gates
built — references `Zenith_TerrainComponent` in ZERO source files, while
Zenithmon, CityBuilder and RenderTest reference it in 12, 4 and 4. ZEN-5
made members of that header private and every Engine gate went green;
three games could have been left un-compilable and nothing would have
said so. The union cannot catch it because the diff is inside Engine's
OWN `paths` — a public header's blast radius is everything that INCLUDES
it, which no directory mapping expresses. So the Engine gate list names
every game, and `Test-ZagentClient.ps1` asserts that a game with a
`.zproj` appears in it: adding a game and forgetting the list FAILS
instead of quietly narrowing engine coverage.

### Running the loop

**Three harnesses run `/tick` today**, each against its own generated entry
point — never hand-edit any of them; regenerate with
`Tools/zagent/Build-HarnessArtifacts.ps1` (see `Tools/zagent/CLAUDE.md`):

| Harness | Invocation | Entry point | Needs |
|---|---|---|---|
| Claude Code | `/tick` (a slash command) | `.claude/commands/tick.md` | just `claude` started in this repo — no `--add-dir`, no second checkout |
| OpenCode | `/tick` (a repo command) | `.opencode/commands/tick.md` | the OpenCode desktop app pointed at this checkout |
| Codex | `$tick` (a `$`-prefixed skill mention typed in the composer) | `.codex/skills/tick/SKILL.md` | the Codex desktop app pointed at this checkout — **Codex has no repo-level slash commands**, so a skill is its entry point, not a command |

A bare `claude` started in this repo is the whole setup for the first row:

```
/tick                # one tick against the head of whichever board has work
/tick ZM             # ...constrained to Zenithmon's board
/tick ZM-22          # run that ticket wherever it sits, once
/loop /tick          # unattended, until the queues empty
```

**Unattended queue-draining (`/loop /tick`, or any external scheduler firing
`/tick` repeatedly) is Claude-Code-only.** OpenCode and Codex are GUI desktop
apps with no external driver and no in-app scheduling surface reachable from
this checkout, so both carry `loopMode: "manual"` in
`Tools/zagent/protocol/harnesses.json` — a human re-invokes `/tick` /
`$tick` for each firing there. Between firings on any harness, pacing is
`.zagent/next.json`: `{disposition: "again"|"wait"|"stop", delaySeconds,
reason}`, written by the tick that just ran and read by whatever fires the
next one — a human, on the two manual harnesses.

**No `--add-dir`, no second checkout.** The scratch is local, and the client
is local. **`zagent` always talks to the local Vault** — this same
checkout — so it needs no network credential of any kind, and there is no
separate board machine to reach. `Tools/zagent/README.md` has the setup
and `zagent doctor` names whatever piece is missing.

### Parallel lanes (optional)

A **lane** is a second, persistent, *registered* git worktree bound to one
board project (`ZM`, `ZEN`, …), so that project's queue can run alongside
the main checkout's without two ticks fighting over the same tree. Off by
default: turning it on is a **protected-path hand-edit** to
`zagent.project.json`'s `lanes.enabled`, plus a commit — never something a
tick does to itself. Full protocol:
`Tools/zagent/protocol/TICK.md#lane-mode`; full how-to (enabling, adding a
lane, per-harness setup, troubleshooting): `Tools/zagent/README.md#lanes`.

Hazards, once lanes are on:

- **Never run `/tick` from an unregistered linked worktree** — every
  mutating command refuses with exit 1 (`zagent lane which --json` reports
  the mode).
- **The main checkout is the integration tree.** A human's own work
  belongs in a worktree of their own; `integrate` refuses to land over an
  overlapping dirty path in the main checkout (`MAIN_DIRTY`).
- **Landings rewrite tracked files in the main checkout**, including
  generated ones (shader headers, a lane's re-pinned `.zscen`) — the main
  checkout can go regen-stale under you while lanes run.
- **Builds serialise on one machine-wide slot** (`Tools/build_lock.ps1`) —
  a lane's build can wait behind the main checkout's, or another lane's.
- **~20-30 GB of disk per lane** (ignored runtime inputs plus every game's
  assets, copied in whole).
- **Remove every lane before disabling** — `zagent lane add`/`remove`
  refuse while `lanes.enabled` is `false`.
- **Claude Code needs interactive trust per lane directory** — until
  `claude` has run there once and the workspace-trust dialog was
  accepted, every `permissions.allow` entry is ignored, so gate commands
  prompt or fail unattended.

### The `zagent` CLI

`zagent` is the board's command line — the **only** thing that writes to
it from outside the web app. It lives HERE, in `Tools/zagent/`, as a
PowerShell script:

```
zagent <command> [--json]
```

That works from `C:\dev\Zenith` and from any subdirectory of it
(`Games\Zenithmon`), in PowerShell, cmd or Git Bash — **but not because it
walks up looking for `zagent.project.json`.** `zagent.ps1` resolves this
repo's root from its own script location (`$PSScriptRoot/../..`), the
same way on every invocation, never by searching upward from the current
directory the way git walks up for `.git`. A run from `Games\Zenithmon`
reads the same gates as one from the repo root simply because the repo
root never moved. What the current directory DOES decide, once
[lanes](#parallel-lanes-optional) are enabled, is which MODE the call
runs in — the main checkout, a registered lane, or an unregistered
worktree that every mutating command refuses; see
`Tools/zagent/protocol/TICK.md#lane-mode`.

Its scratch is `.zagent/last.json` and `.zagent/run/<KEY>/` at the repo
root, gitignored so `git status --porcelain` stays empty while a ticket
is in flight — the loop's precondition depends on that.

If `zagent` is not on PATH, `Tools/zagent/README.md` has the one-time
setup; you can always call `pwsh -NoProfile -File Tools\zagent\zagent.ps1`
directly.

**There is no protocol handshake left to describe.** The single local
backend has nothing to negotiate a version with, and `zagent contract`
— which used to print a remote board's command contract as data — was
removed along with it; `Tools/Vault/spec/command-matrix.md` is the
offline specification of record now.

The client has its own tests — a plain assert script, matching
`Tools/Test_T0Harness_*`, because the only PowerShell this repo requires
is the `pwsh` every gate already needs:

```
pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1
```

They cover the parts that bite: `,@(…)` returns (a PowerShell function
UNROLLS a returned collection, so a one-element `cleanup` array would go
on the wire as a bare string), separator folding (a mismatch makes an
uploaded docs tree match nothing, which mirrors zero pages and reports
success), and reading optional fields through `PSObject.Properties[…]`
(`Set-StrictMode` turns a missing property into a throw, so a project
file that legally omits `baseBranch` would make `doctor` crash instead of
report).

★ **A parse-check plus green unit tests is NOT proof the client runs.**
Splitting the helpers into a module once left `Get-BoardUrl` unexported:
both files parsed, all 47 assertions passed, and every command failed.
Smoke the real binary after touching either file.

Commands you would actually use from here:

| Command | What it does |
|---|---|
| `queue` | what is claimable in To Do — the loop's queue (the repo declares its own project) |
| `show ZEN-6` | one ticket: routing, category, gates, contract errors |
| `create --project ZEN --title "…" --file body.md --category Zenithmon --complexity … --risk …` | file a ticket |
| `doctor` | pre-flight — scanner/config diagnostics, board roots and ticket count, all local now: exit 0, `ok: true`, empty `diagnostics` |
| `owns ZEN-6` | is the loop still holding it |
| `gates ZEN-6` | the gate list this DIFF needs — the ticket's own plus every category whose `paths` it touched |
| `guard ZEN-6` | protected paths, **and** that the recorded gate selection still describes the diff |
| `docs ls [<path>]` | local Markdown files under the vault root, one repo-relative path per line |
| `docs read <path>` | that exact file's body as Markdown, on stdout |
| `docs search "<text>"` | titles + bodies, with a snippet |
| `rows decisions\|questions\|shortfalls\|changelog\|suggestions` | read the knowledge databases |
| `suggestions [--all]` / `rows <db> resolve <ID> <STATUS> --reason "…"` | triage one row. `--reason` is REQUIRED, like `blocks` and for the same cause — nothing can recompute whether the friction is gone. Only questions, shortfalls and suggestions have a `Status`; decisions and changelog are append-only |
| `docs status` | local Markdown document count plus scanner diagnostics |
| `link <KEY> blocks\|relates\|duplicates\|causes <KEY> --reason "…"` | one directed link; `blocks` is the one the claim query reads, and the only LINK that REQUIRES `--reason` (a row resolve requires one too) |
| `blocked --project ZM` | everything waiting, and on what |
| `links <KEY>` / `epic <KEY>` / `parent <KEY> <PARENT>` | the dependency graph and the hierarchy |
| `sprint list\|create\|start\|complete\|add\|remove\|gate` | one ACTIVE sprint per project (a DB constraint). `gate` designates the sprint's milestone-acceptance ticket — an ordinary member of the sprint, never an EPIC. Completing a milestone sprint is REFUSED while that gate is unmet, and `--force` does not reach the check; `--force` with unfinished work and NO designated gate is also a refusal, so the old silent carry-over is honest now or not at all |
| `version list\|create\|release\|set` | milestones — what a BUILD contains |
| `check ZEN-6` | would a claim take it? **exit 4 if not** — writes nothing, claims nothing |
| `update <KEY> --points --severity --repro --environment --due` | field writes, with a history trail |
| `update <KEY> --body\|--goal\|--dod <file>` | edit the spec, keeping any appended work log. `--goal`/`--dod` REPLACE that section in the existing body and refuse ("Body has no `## Goal` section") when the ticket has no such section yet — e.g. a pre-check-era ticket filed with no structure at all (ZEN-10). Use `--body` with the full replacement text (both sections) to give a structureless ticket its first `## Goal`/`## Definition of Done` |
| `update <KEY> --label a,b --unlabel c` | apply a marker (e.g. `needs-human`) AFTER filing, which is when you learn one is needed |
| `update <KEY> --assignee none` | UNASSIGN — the only way to undo a claim. A claim writes the assignee, `move` only changes status, and the queue skips anything assigned, so a release without this parks the card invisibly |
| `estimate` / `flag` / `resolve` / `history` | the one-gesture shorthands |
| `report epic\|velocity --project ZM` | per-epic progress; velocity over COMPLETED sprints only |
| `board status --project ZM` | does `Roadmap.md` still describe the board? **exit 1 on drift** |

**Reading a doc from a Zenith session no longer needs a browser.** `docs
ls` prints every local Markdown file's own repo-relative path (e.g.
`Games/Zenithmon/Docs/AgentBriefing.md`); `docs read <path>` and `docs
search "<text>"` take that exact path back. There is no remote mirror,
no page ids, and none of the old Notion-only flags — local docs reads
refuse every flag except `--json`.

**`Games/*/Docs` is the only copy, and always was authoritative — there
is nothing left to mirror it into.** `docs write` and `docs sync` are
`removed-transport` (`Tools/Vault/spec/command-matrix.md`): both refuse
unconditionally, `"<command> is unavailable in the read-only local
vault; remote sync and writes are not implemented."` Edit the Markdown
file directly — `Tools/doc_lint.ps1` still lints it off disk, `zagent
decide` still appends to `Games/Zenithmon/Docs/DecisionLog.md`, and
`Status.md` is still the authority for the pinned ZM baseline.

None of this reaches a **loop worker**, which still has no shell and no
network and still reads `Docs/AgentBriefing.md` off disk with the Read
tool. This is for your session and for the tick.

**Filing a ticket from Zenith work.** The description IS the spec, and
the loop refuses anything it cannot route. Three sections:

```markdown
## Goal
<what outcome — quote the living docs rather than paraphrasing>

## Definition of Done
- [ ] <observable outcome>
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run

## Gates
<omit this — the category supplies the right gate list>
```

**Never name a pin SITE in a Definition of Done.** That line used to read
"bumped in Status.md AND zm-tests.yml AND zagent.project.json", which was
true until the baseline got one home and is now a worker prompt pointing
at two `protectedPaths`. Ten open tickets still carry it. A DoD is
inlined into the worker prompt verbatim (I3), so a stale one competes
directly with the tick's own instructions — and naming sites in a DoD is
the same duplication the manifest refactor removed from the build, just
re-introduced one ticket at a time. Say the OUTCOME; the plumbing is
allowed to move.

Always pass `--category`: `Zenithmon`, `Engine`, `DevilsPlayground`,
`ScriptTest`, `Undervault`, `Hearth` or `Foundry`. It selects the gate
list, the conventions inlined into the worker's prompt, the branching
mode, **and now which board the ticket is filed on**. `ScriptTest`
declares no `project` of its own, so it files on the checkout default
(`ZEN`) and inherits `direct` branching — a game area is not
automatically a board. `Undervault`/`Hearth`/`Foundry` file onto `UV` /
`HE` / `FD`, which carry NO sprint started and NO delivery tickets: the
only ticket those categories accept today is a game's first scaffold,
and the milestone a sprint serves begins when a human runs
`zagent sprint start`.

`create` also takes `--type EPIC|STORY|TASK|BUG|SUBTASK`, `--parent`,
`--points N`, `--severity S1_CRITICAL…`, `--repro ALWAYS…`,
`--environment "Null_vs2022_Debug_Win64_True"`, `--due`, `--sprint`,
`--fix-version`, `--blocks a,b` and `--blocked-by a,b`. Sizing is TWO
fields and they are not interchangeable: `--complexity`/`--risk` pick the
MODEL, `--points` sizes the SPRINT. A `ZEN` ticket without one comes back
`contractValid: false` rather than being guessed at, because guessing
between areas means ratcheting the wrong pin.

`create` refuses at FILE time — you no longer find out hours later when
the loop reaches the card — whenever the ticket would be **immediately
claimable**, which is every unassigned, un-`needs-human` ticket. It used to
refuse only a ticket filed straight into *Ready for Agent* and merely
warn about one filed into *To Do*, on the reasoning that a stub in To Do
was safe because the loop could not reach it there. **To Do IS the queue
now**, so there is no lane where a malformed ticket sits harmlessly and
no drafting lane to file one into: keep an unfinished body in a file
until it is done, or file it with `--label needs-human` / `--assignee`. It
also refuses a body carrying its own `## Gates`, because a ticket's gate
list REPLACES the category's rather than adding to it: pasting the
Zenithmon list into a ticket is how a pinned baseline goes stale, and
`echo ok` in a body would merge on `echo ok`.

**It also refuses a DoD that names a protected path.** `contractValid`
used to answer only "can this be ROUTED" — category, complexity, risk —
and said nothing about whether the work is inside what an agent bound by
`protectedPaths` can reach. Two consecutive tickets were valid and
impossible: one required a `.github/workflows/` file, the other a pin
bump in files the guard refuses. Each cost a claim, a slot in the
one-ticket-per-repo lock and a worker dispatch before anyone noticed.
The scan reads the **Definition of Done only** — the Goal may quote a
protected path as background without being refused — and reports the
path AND the pattern, so the fix is obvious: split the protected half
out as a human task.

Size it deliberately: `--complexity SIMPLE|MODERATE|COMPLEX`
picks the model, `--risk LOW|MEDIUM|HIGH` escalates it one tier. Risk
never blocks a merge — it buys more thinking.

**Risk no longer decides who gets REVIEWED, and that is the point.**
Sizing is answered when a ticket is filed, before anyone has seen a
diff; whether the work needs a reader is a property of what the ticket
CLAIMS. A Definition of Done that asserts an equivalence
(*"provably unchanged"*, *"byte-identical"*), an absence (*"no new
allocations"*, *"must not"*) or something about the tests themselves
(*"covered by a test"*) is asking for something no gate can settle, and
`review.required` in the payload is now the OR of `reviewOn` and that
scan. ZEN-2 was filed LOW, honestly, and its DoD said the recorder was
provably unchanged; the review found the recorder had zero coverage
while the test file said it was covered.

**Work the loop cannot finish gets `--label needs-human`.** No machine
can produce the deliverable — a visual sign-off, a ruling the ticket does
not contain. The units that would notice are compiled constants that stay
green, so the failure mode is a clean gate run with the deliverable
missing. The label is filtered out of the claim query and fails the
contract on a targeted claim, so the loop cannot take it either way. Add
`--assignee <email>` as well when you want it on someone's card; the
label is what protects it.

This used to be `--assignee` alone, which worked only because the queue
skips assigned tickets — making the SAFE state the one nobody typed.
Now the marker is explicit, greppable, and enforced.

**Work that needs a GPU gets `--label needs-gpu`, and that gates
NOTHING.** A GPU is assumed available, so the ticket is claimed like any
other; the label tells the tick to build `Vulkan_*_True` and run windowed
rather than the `--headless` `Null_` config every gate line defaults to.
The pin still comes from a `Null_` run — a Vulkan exe reports higher for
the same tree — so the shape is Vulkan to author, Null to verify and pin.

**These two were ONE label, and splitting them was worth doing.**
`windowed` meant "the loop cannot finish this" and collected unrelated
reasons under one word. The guard that made scene authoring LOOK
impossible was a `if constexpr (Zenith_IsNullRenderer())` refusal in
`Zenith_Editor::SaveActiveScene`, **compiled out of a Vulkan build
entirely** — so it protected against a Null boot serializing an
incomplete world over a tracked asset, never against an absent human.

**★ ZEN-6 REMOVED THAT GUARD (2026-08-26), and the reason it existed is
gone with it.** Entity creation on the authoring path is backend-neutral
now — only the GPU allocation is skipped, which the Null memory manager
was already doing — so a headless boot authors a complete world and
publishes it. A `Null_*_True` boot re-authors every committed scene both
RenderTest and Zenithmon publish and reports `[ScenePublish] IDENTICAL`.
**So "it re-authors a committed scene" is no longer a reason for
`needs-gpu` at all.** The label still means what it says — the
deliverable needs a graphics driver — but scene authoring is not that. Of 12 labelled tickets, 8 wanted only a
graphics driver and 4 wanted a person — and two of those four had nothing
to do with rendering (one's deliverable was a board row, the other's a
`.github/**` workflow). Six of the ten tickets on Zenithmon's S8 critical
path sat behind the label; exactly one needed a human.
ZEN-6 tracks removing the underlying limit; the upstream migration
`0028_split_windowed_marker` did the rename.

**And a marker can be applied after filing** — `zagent update <KEY>
--label needs-human` — which is the point, because you learn a ticket
needs a person by READING it, not by predicting it at create time. ZM-56
is *"`ZM_InteriorTintPixels_Test` passes windowed, or is retired with a
recorded reason"*: a `requiresGraphics` test whose only
headless-verifiable branch is deleting the coverage. Nothing stopped the
loop claiming it, because labels could only be set at create time.

**Filing a ticket IS handing it over.** There is no hand-off column and
no drag to perform: a card in *To Do* is claimable the moment it lands,
and the loop carries it to Done with nobody in the middle. That is the
whole point — a lane a person had to drag a card into put a human in
front of every ticket, so it was retired (upstream migration
`0026_retire_ready_for_agent`) and its cards moved into *To Do*.

**Three markers, and only three, put a person back in the loop**, and
each of them is something the TICKET says out loud:

| Marker | What it does |
| --- | --- |
| `--label needs-human` | never claimed, by the queue or by a targeted claim — no machine can produce the deliverable |
| `--label human-gate` | the loop does the whole job, then stops at *In Review* instead of Done (I7) |
| `--label deferred` | skipped by the QUEUE, still taken by a targeted claim — "not yet", not "not ever" |
| `--assignee <email>` | an assigned ticket is invisible to the queue |

`--label needs-gpu` is deliberately NOT in that table. It changes how the
tick builds a ticket, never whether it may take one.

Everything else runs unattended, To Do straight through to Done — with
two exceptions that are not markers, because nobody applies them. An
**EPIC** is never claimable, by the queue or by name: it is a rollup
container, and its "Every child issue Done" inlines under I3 as a
specification no worker can act on. A ticket in a **PLANNED sprint**
while another is ACTIVE is skipped by the QUEUE and still claimable by
name — exactly the `deferred` shape, needing no label because the sprint
IS the deferral. `zagent check` shows both: exit 4 for the epic, an
`OUT OF STAGE` line for the sprint.

*Blocked* is not a fifth marker: it is a FAILURE terminal the loop puts
work in when it could not finish, not a step anything routine passes
through.

**`deferred` exists because To Do became the queue**, and it is the one
new thing this change needed. Several tickets defer THEMSELVES in prose
no field captures — ZM-48 reads *"Zenithmon does NOT need this now"*,
ZM-47 is *"DEFERRED post-Zenithmon"* — and the tick used to handle one by
releasing it from *Ready for Agent* back to *To Do*, which took it out of
the queue. The same gesture now puts it straight back at the head, so the
loop would claim and release it forever. `Blocked` would be the wrong
home: nothing is wrong with these tickets, and three of them would trip
`maxConsecutiveBlocked` and stop the queue over work that is merely
early. A targeted claim still takes a `deferred` ticket, because the
deferral is a judgement about timing and `/tick ZM-48` is how you
overrule it.

**A body that says it needs a person is not a marker.** Removing the
hand-off lane made that difference load-bearing for the first time: five
open tickets said so in their own words and carried nothing mechanical —
ZM-9 and ZM-18 (*"no agent may sign it"*, *"a human visual sign-off"*),
ZM-60, and the two self-deferred engine gaps — and every one of them sat
in *To Do*, which the loop now claims from. They are labelled now. Any
NEW ticket in that shape needs its marker applied at file time, because
nothing scans prose for it and the queue will not hesitate.

To run a ticket immediately rather than waiting for the queue, use
`/tick` from a session started here — see **Running the loop** above.

**Exit codes are the interface** — branch on them, not on the text:
`0` ok · `3` nothing to claim · `4` contract invalid · `5` ownership
lost / repo busy · `6` circuit breaker open · `1` error. Note that `5` is
what you get when this repo already has a ticket in flight: one at a
time, per repo, by design. There is no longer a distinct "could not
reach the board" code — there is no network hop left to fail that way,
and a local read/write problem is just `1`.

**`check` ranks these differently from `claim`/`next`, on purpose.** For
`claim` and `next` — which mutate — `5` still wins outright whenever a
ticket is in flight: refusing to steal it is the point. `check` is
read-only and answers "would a claim take this?", so it ranks the
DURABLE verdict over the transient lock: a contract failure, an EPIC or
a `needs-human` ticket answers `4` even while the repo is busy, and `5`
from `check` means nothing durable is wrong — the repo is simply busy
right now. The lock clears by itself; a durable refusal does not, and
reporting the one that clears hides the one that matters.

**Setting a machine up** (once): put `Tools\zagent` on `PATH`. That is the
whole setup — no token, no URL, nothing to mint, because the board is this
checkout and there is exactly one backend. `Tools/zagent/README.md` has
the commands; `zagent doctor` names whatever piece is missing.

`packages/agent/CLAUDE.md` in the other repo is the full reference for
the dispatcher, `apps/jira/CLAUDE.md` for the HTTP surface.
