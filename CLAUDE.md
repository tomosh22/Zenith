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
├── TaskSystem/        # Task parallelism (see TaskSystem/CLAUDE.md)
├── Physics/           # Jolt Physics integration (see Physics/CLAUDE.md)
├── Vulkan/            # Vulkan backend (see Vulkan/CLAUDE.md)
├── D3D12/             # No-op null render backend (Flux neutrality proof) (see D3D12/CLAUDE.md)
├── Core/              # Core utilities (see Core/CLAUDE.md)
├── Collections/       # Custom containers (see Collections/CLAUDE.md)
├── Maths/             # GLM wrapper (see Maths/CLAUDE.md)
├── AssetHandling/     # Asset import/export (see AssetHandling/CLAUDE.md)
├── Windows/           # Windows platform layer (see Windows/CLAUDE.md)
├── Android/           # Android platform layer (see Android/CLAUDE.md)
├── DataStream/        # Binary serialization (see DataStream/CLAUDE.md)
├── SaveData/          # Save/load persistence (see SaveData/CLAUDE.md)
├── DebugVariables/    # Runtime debug variable tree (see DebugVariables/CLAUDE.md)
├── Input/             # Input handling (see Input/CLAUDE.md)
├── Profiling/         # CPU profiling system (see Profiling/CLAUDE.md)
├── Telemetry/         # Runtime telemetry capture
├── UnitTests/         # Unit-test harness
├── Prefab/            # Prefab system (see Prefab/CLAUDE.md)
├── UI/                # UI framework (see UI/CLAUDE.md)
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

### Generating Solution Files

Games are **descriptor-driven**: each game has a `Games/<Name>/<Name>.zproj` (JSON —
`schemaVersion`, `name`, `android`, optional `extraDefines`/`extraSharpmakeProjects`).
Adding or removing a game touches only its descriptor — **no Sharpmake C# edits**.

Regenerate every solution from the repo root (or `Build/`):
```batch
Build\regen.ps1
```
(`zenith regen` and the legacy `Build\Sharpmake_Build.bat` / `run_sharpmake.ps1`
forward here.) regen.ps1 validates all descriptors, codegens
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

### Build Configurations

Each project supports these configurations. Every win64 config is prefixed by the
render backend (`Vulkan_` = the real renderer; `D3D12_` = the no-op null backend
that proves Flux is backend-neutral). The table shows the `Vulkan_` rows; the
`D3D12_` rows are identical with the prefix swapped. agde is Vulkan-only (`Vulkan_` prefix on all configs).

| Configuration | Platform | Tools | Description |
|--------------|----------|-------|-------------|
| `Vulkan_vs2022_Debug_Win64_True` | Windows | Yes | Debug build with editor/tools |
| `Vulkan_vs2022_Debug_Win64_False` | Windows | No | Debug build, runtime only |
| `Vulkan_vs2022_Release_Win64_True` | Windows | Yes | Release build with editor/tools |
| `Vulkan_vs2022_Release_Win64_False` | Windows | No | Release build, runtime only |
| `D3D12_vs2022_Debug_Win64_False` | Windows | No | Null-backend link/neutrality proof (+ _True / Release variants) |
| `Vulkan_arm64_v8a_vs2022_Debug_Agde_False` | Android | No | Android debug build |
| `Vulkan_arm64_v8a_vs2022_Release_Agde_False` | Android | No | Android release build |

### Projects

| Project | Description | Output |
|---------|-------------|--------|
| Zenith | Core engine library | Static lib (.lib) |
| FluxCompiler | Shader compiler utility (Windows only) | Executable (.exe) |
| ZenithTools | Asset tools (Windows only) | Executable (.exe) |
| Game projects | Sokoban, Combat, Marble, Exploration, Survival, TilePuzzle, AIShowcase, Runner, Test, RenderTest, CityBuilder (SimCity/C:S-style — see Games/CityBuilder/CLAUDE.md), DevilsPlayground | Executable (.exe) / Shared lib (.so) |

### Building and Running

**The `zenith` CLI (recommended):**
```batch
zenith new <Name>          REM scaffold a new game (regen + open its sln)
zenith build Sokoban       REM msbuild the game's per-game sln (/t:<Game>)
zenith run Sokoban         REM launch the newest built exe
zenith test Sokoban        REM run the game's automated tests (or: zenith test all)
zenith open Sokoban        REM regen + open the game's sln in Visual Studio
zenith list                REM list games + built configs
zenith clean Sokoban       REM kill hanging build processes + wipe output/obj
zenith package Sokoban     REM stage a relocatable build into dist/ (run.bat --assets-root)
zenith regen --check       REM report whether on-disk generated files are stale
zenith hub                 REM launch the Unity-Hub-style GUI launcher
```

**Direct msbuild (per-game solution):**
```batch
msbuild Games\Sokoban\sokoban_win64.sln /t:Sokoban /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
Games\Sokoban\Build\output\win64\vulkan_vs2022_debug_win64_true\sokoban.exe
```
Always build with `/t:<Game>`, never the whole solution: the aux tools (FluxCompiler /
font libs) present in the sln are pre-existing-red in `ToolsEnabled=True`.

> **Config-name prefix (RenderBackend fragment):** every win64 config is prefixed
> with the render backend — `Vulkan_vs2022_Debug_Win64_True` (the real renderer)
> or `D3D12_vs2022_Debug_Win64_False` (a no-op null backend that proves the Flux
> surface is backend-neutral; see `Zenith/D3D12/CLAUDE.md`). The output dir is the
> lowercased config name. agde is Vulkan-only (`Vulkan_` prefix on all configs).

**Using Visual Studio:** `zenith open <Name>` regenerates then opens
`Games/<Name>/<name>_win64.sln`; set the game as startup project, pick a config
(e.g. `Vulkan_vs2022_Debug_Win64_True|x64`), F5.

**Engine-only work:** `msbuild Build\zenith_engine_win64.sln /t:Zenith` (or
`/t:FluxCompiler` / `/t:Sentinel*` / `/t:ZenithHub`); `zenith build engine` does this.
Never build the whole engine sln (pre-existing-red aux tools).

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
zenith clean Sokoban          REM ...and also wipe that game's output/ + obj/
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
- `zenith_buildsystem.psm1` - descriptor scan / validate / codegen / name-validation module (single source of truth)
- `regen.ps1` - canonical regenerator (worktree-guard -> validate -> codegen -> one Sharpmake run -> AGDE fixup)
- `Sharpmake_ZenithTools.cs` - Asset tools project
- `Sharpmake_ZenithAI.cs` - AI module project
- `Sharpmake_ZenithECS.cs` - ECS module project
- `Sharpmake_ZenithPhysics.cs` - Physics module project
- `Sharpmake_SentinelAI.cs` / `Sharpmake_SentinelECS.cs` / `Sharpmake_SentinelPhysics.cs` - Sentinel test modules
- `Sharpmake_FreeType.cs` / `Sharpmake_Msdfgen.cs` / `Sharpmake_MsdfAtlasGen.cs` - Font/text dependency projects
- `Sharpmake_TilePuzzleLevelGen.cs` / `Sharpmake_TilePuzzleRegistryViewer.cs` - TilePuzzle tooling projects
