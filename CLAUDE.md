# Zenith Game Engine

## Overview

C++20 game engine with custom ECS, Vulkan-based renderer (Flux), and multi-threaded task system.

## Directory Structure

```
Zenith/
├── EntityComponent/   # Entity-Component System (see EntityComponent/CLAUDE.md)
├── Flux/              # Vulkan renderer (see Flux/CLAUDE.md)
│   ├── Gizmos/        # Editor gizmos (see Flux/Gizmos/CLAUDE.md)
│   └── Terrain/       # Terrain rendering (see Flux/Terrain/CLAUDE.md)
├── Editor/            # Editor tools (see Editor/CLAUDE.md)
├── TaskSystem/        # Task parallelism (see TaskSystem/CLAUDE.md)
├── Physics/           # Jolt Physics integration (see Physics/CLAUDE.md)
├── Vulkan/            # Vulkan backend
├── Core/              # Core utilities (see Core/CLAUDE.md)
├── Collections/       # Custom containers (see Collections/CLAUDE.md)
├── Maths/             # GLM wrapper (see Maths/CLAUDE.md)
├── AssetHandling/     # Asset import/export (see AssetHandling/CLAUDE.md)
├── Windows/           # Windows platform layer
├── Android/           # Android platform layer
├── DataStream/        # Binary serialization
├── Input/             # Input handling
├── Prefab/            # Prefab system
├── UI/                # UI components
└── FileAccess/        # File system abstraction
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
- Renderer classes: `Flux_ClassName` (e.g., `Flux_Texture`, `Flux_CommandList`)

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
| `ax` | Array | `m_axColourAttachments` |
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
- Values: `CATEGORY_NAME_VALUE` (e.g., `TEXTURE_FORMAT_RGBA8_UNORM`, `RENDER_ORDER_SKYBOX`)

### Constants
- `SCREAMING_SNAKE_CASE` (e.g., `FLUX_MAX_TARGETS`, `MAX_FRAMES_IN_FLIGHT`)

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

Run from the `Build/` directory:
```batch
Sharpmake_Build.bat
```

This generates:
- `zenith_win64.sln` - Windows x64 solution
- `zenith_agde.sln` - Android (AGDE) solution

### Build Configurations

Each project supports these configurations:

| Configuration | Platform | Tools | Description |
|--------------|----------|-------|-------------|
| `vs2022_Debug_Win64_True` | Windows | Yes | Debug build with editor/tools |
| `vs2022_Debug_Win64_False` | Windows | No | Debug build, runtime only |
| `vs2022_Release_Win64_True` | Windows | Yes | Release build with editor/tools |
| `vs2022_Release_Win64_False` | Windows | No | Release build, runtime only |
| `vs2022_Debug_agde_False` | Android | No | Android debug build |
| `vs2022_Release_agde_False` | Android | No | Android release build |

### Projects

| Project | Description | Output |
|---------|-------------|--------|
| Zenith | Core engine library | Static lib (.lib) |
| FluxCompiler | Shader compiler utility (Windows only) | Executable (.exe) |
| Sokoban | Game project | Executable (.exe) / Shared lib (.so) |

### Building and Running

**Windows (with editor):**
```batch
cd Build
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Sokoban\Build\output\win64\vs2022_debug_win64_true
sokoban.exe
```

**Using Visual Studio:**
1. Open `Build\zenith_win64.sln`
2. Set Sokoban as startup project
3. Select configuration (e.g., `vs2022_Debug_Win64_True|x64`)
4. Build and run (F5)

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
CleanBuild.bat zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
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
- Use single-threaded builds if parallel builds cause issues: `-maxCpuCount:1`
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
