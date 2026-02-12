# Debug Variables System

## Overview

Hierarchical debug variable tree for runtime inspection and modification of engine parameters. Tools-only (`#ifdef ZENITH_TOOLS`). Uses ImGui for rendering.

## Files

- `Zenith_DebugVariables.h` - Registration API and tree structure

## Architecture

Variables are organized in a tree via path segments (e.g., `{"Flux", "SSR", "Enable"}`). Leaf nodes store references/pointers to the original variables (zero-copy). The tree renders via ImGui with appropriate widgets per type.

## Registration API

All registration functions are static on `Zenith_DebugVariables`:

| Function | Parameters | Widget |
|----------|-----------|--------|
| `AddBoolean` | `path, bool&` | Checkbox |
| `AddFloat` | `path, float&, min, max` | Slider |
| `AddUInt32` | `path, uint32_t&, min, max` | Slider |
| `AddVector2/3/4` | `path, Vector&, min, max` | Multi-slider |
| `AddFloat_ReadOnly` | `path, float&` | Display only |
| `AddUInt32_ReadOnly` | `path, uint32_t&` | Display only |
| `AddUInt64_ReadOnly` | `path, uint64_t&` | Display only |
| `AddButton` | `path, void(*)()` | Button |
| `AddTexture` | `path, Flux_SRV&` | Texture preview |
| `AddText` | `path, string&` | Text display |

Path is `std::vector<std::string>` of category segments.

## Node Types

- `LeafNode<T>` - Editable variable with reference
- `LeafNodeWithRange<V, R>` - Editable with min/max bounds
- `LeafNodeReadOnly<T>` - Display-only variable
- `PfnLeafNode` - Callable button (function pointer)
- `TextNode` - Static text display

## Usage Pattern

Engine subsystems register their debug variables during initialization:

```cpp
static bool s_bEnabled = true;
static float s_fIntensity = 1.0f;
Zenith_DebugVariables::AddBoolean({"Flux", "SSR", "Enable"}, s_bEnabled);
Zenith_DebugVariables::AddFloat({"Flux", "SSR", "Intensity"}, s_fIntensity, 0.f, 2.f);
```

Variables appear in the Debug Variables panel organized by path hierarchy.

## Constraints

- Maximum category name length: 64 characters
- Enum types supported via `AddUInt32<T>` template with `static_assert(sizeof(T) == sizeof(uint32_t))`
- Variables must outlive the tree (references, not copies)
