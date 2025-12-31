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
└── Maths/             # GLM wrapper (see Maths/CLAUDE.md)
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
Opening braces on new line for all blocks:
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
