# Zenith Game Engine

## Overview

C++20 game engine with custom ECS, Vulkan-based renderer (Flux), and multi-threaded task system.

## Directory Structure

```
Zenith/
├── EntityComponent/   # Entity-Component System (see EntityComponent/AGENTS.md)
├── Flux/              # Vulkan renderer (see Flux/AGENTS.md)
│   ├── Fog/           # Volumetric fog (see Flux/Fog/AGENTS.md)
│   ├── Gizmos/        # Editor gizmos (see Flux/Gizmos/AGENTS.md)
│   ├── HDR/           # HDR pipeline (see Flux/HDR/AGENTS.md)
│   ├── HiZ/           # Hierarchical Z-buffer (see Flux/HiZ/AGENTS.md)
│   ├── IBL/           # Image-based lighting (see Flux/IBL/AGENTS.md)
│   ├── MeshAnimation/ # Skeletal animation (see Flux/MeshAnimation/AGENTS.md)
│   ├── SSR/           # Screen-space reflections (see Flux/SSR/AGENTS.md)
│   ├── SSGI/          # Screen-space GI (see Flux/SSGI/AGENTS.md)
│   ├── Terrain/       # Terrain rendering (see Flux/Terrain/AGENTS.md)
│   ├── Vegetation/    # Grass system (see Flux/Vegetation/AGENTS.md)
│   └── ...            # + DeferredShading, DynamicLights,
│                      #   InstancedMeshes, Particles, Primitives, Shadows,
│                      #   Skybox, SSAO, Text, UnifiedMesh, and more
├── AI/                # AI systems (see AI/AGENTS.md); decisions live in Scripting/ graphs
│   ├── Navigation/    # NavMesh pathfinding (see AI/Navigation/AGENTS.md)
│   ├── Perception/    # Sensory systems (see AI/Perception/AGENTS.md)
│   └── Squad/         # Squad tactics (see AI/Squad/AGENTS.md)
├── Editor/            # Editor tools (see Editor/AGENTS.md)
├── TaskSystem/        # Task parallelism (see TaskSystem/AGENTS.md)
├── Physics/           # Jolt Physics integration (see Physics/AGENTS.md)
├── Vulkan/            # Vulkan backend (see Vulkan/AGENTS.md)
├── Core/              # Core utilities (see Core/AGENTS.md)
├── Collections/       # Custom containers (see Collections/AGENTS.md)
├── Maths/             # GLM wrapper (see Maths/AGENTS.md)
├── AssetHandling/     # Asset import/export (see AssetHandling/AGENTS.md)
├── Windows/           # Windows platform layer (see Windows/AGENTS.md)
├── Android/           # Android platform layer (see Android/AGENTS.md)
├── DataStream/        # Binary serialization (see DataStream/AGENTS.md)
├── DebugVariables/    # Runtime debug variable tree (see DebugVariables/AGENTS.md)
├── Input/             # Input handling (see Input/AGENTS.md)
├── Profiling/         # CPU profiling system (see Profiling/AGENTS.md)
├── Prefab/            # Prefab system (see Prefab/AGENTS.md)
├── UI/                # UI framework (see UI/AGENTS.md)
└── FileAccess/        # File system abstraction (see FileAccess/AGENTS.md)
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

Games are **descriptor-driven** (`Games/<Name>/<Name>.zproj`); adding/removing a game
needs no Sharpmake C# edits. Regenerate all solutions:
```batch
Build\regen.ps1
```
(`zenith regen` forwards here.)

This generates **per-game** solutions plus one **engine-only** solution (NO all-games sln):
- `Games/<Name>/<name>_win64.sln` (+ `<name>_agde.sln` when `android:true`)
- `Build/zenith_engine_win64.sln` — engine libs + Sentinels + tools + ZenithHub, zero games

**Regenerate-first policy:** ALL Sharpmake outputs (`.sln`, `.vcxproj`, `.filters`,
`.user`, generated `.cs`) are gitignored — never commit them; run `Build\regen.ps1`
after clone/branch switch. Full reference: **`Docs/GameProjects.md`**.

**Artifact-root rule:** anything a runner/test/tool emits (test results, telemetry,
logs, packages) goes under `Build/artifacts/…` — never a new ad-hoc `Build/<name>_results`
dir, and never `git add` anything under an ignored path.

### Build Configurations

Each project supports these configurations:

| Configuration | Platform | Tools | Description |
|--------------|----------|-------|-------------|
| `vs2022_Debug_Win64_True` | Windows | Yes | Debug build with editor/tools |
| `vs2022_Debug_Win64_False` | Windows | No | Debug build, runtime only |
| `vs2022_Release_Win64_True` | Windows | Yes | Release build with editor/tools |
| `vs2022_Release_Win64_False` | Windows | No | Release build, runtime only |
| `arm64_v8a_vs2022_Debug_Agde_False` | Android | No | Android debug build |
| `arm64_v8a_vs2022_Release_Agde_False` | Android | No | Android release build |

### Projects

| Project | Description | Output |
|---------|-------------|--------|
| Zenith | Core engine library | Static lib (.lib) |
| FluxCompiler | Shader compiler utility (Windows only) | Executable (.exe) |
| ZenithTools | Asset tools (Windows only) | Executable (.exe) |
| Game projects | Sokoban, Combat, Marble, Exploration, Survival, TilePuzzle, AIShowcase, Runner, Test | Executable (.exe) / Shared lib (.so) |

### Building and Running

**The `zenith` CLI (recommended):** `zenith build|run|open|new|list|hub <Name>`.

**Direct msbuild — always `/t:<Game>`, never the whole sln** (aux tools are
pre-existing-red in `ToolsEnabled=True`):
```batch
REM win64 configs are prefixed by render backend: Vulkan_ (real) or D3D12_ (null
REM backend; backend-neutrality proof). Output dir is the lowercased config name.
msbuild Games\Sokoban\sokoban_win64.sln /t:Sokoban /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
Games\Sokoban\Build\output\win64\vulkan_vs2022_debug_win64_true\sokoban.exe
```
Engine work: `msbuild Build\zenith_engine_win64.sln /t:Zenith` (or `/t:Sentinel*` /
`/t:FluxCompiler` / `/t:ZenithHub`).

**Using Visual Studio:** `zenith open <Name>` (regens + opens
`Games/<Name>/<name>_win64.sln`); set the game as startup, pick a config, F5.

### Build Troubleshooting

#### Hanging Compiler Processes

MSBuild can sometimes leave hanging `cl.exe` (compiler) and `mspdbsrv.exe` (debug database server) processes that lock build files, preventing subsequent builds from succeeding.

**Symptoms:**
- Build fails with "cannot access file" or "file is being used by another process" errors
- Multiple retries to copy `.pdb` files fail
- Errors like: `error MSB3027: Could not copy "zenith_compiler.pdb"`

**Cause:**
Parallel builds (`-maxCpuCount`) or interrupted builds can leave compiler subprocesses running. These processes hold locks on `.pdb`, `.pch`, and `.obj` files.

**Solution - Clean Build Script:**

Create a helper script `CleanBuild.bat` to kill hanging processes before building:

```batch
@echo off
REM Kill any hanging compiler processes
taskkill /F /IM cl.exe /T 2>nul
taskkill /F /IM mspdbsrv.exe /T 2>nul
taskkill /F /IM link.exe /T 2>nul
taskkill /F /IM vctip.exe /T 2>nul

REM Wait for processes to terminate
timeout /t 1 /nobreak >nul

REM Now build
msbuild %*
```

**Usage:**
```batch
CleanBuild.bat Games\Sokoban\sokoban_win64.sln /t:Sokoban /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
```

**Alternative - Manual Cleanup:**

PowerShell:
```powershell
Get-Process cl,mspdbsrv,link,vctip -ErrorAction SilentlyContinue | Stop-Process -Force
```

Command Prompt:
```batch
taskkill /F /IM cl.exe /T
taskkill /F /IM mspdbsrv.exe /T
```

**Prevention:**
- Builds use parallel compilation (`-maxCpuCount`, all cores). If a parallel build leaves hanging processes, run `CleanBuild.bat` (above) before retrying — don't fall back to single-threaded
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
- `Sharpmake_Common.cs` - Base project class, platform configuration
- `Sharpmake_Zenith.cs` - Zenith engine project
- `Sharpmake_FluxCompiler.cs` - Shader compiler project
- `Sharpmake_Games.cs` - Game project template
- `Sharpmake_ZenithTools.cs` - Asset tools project
