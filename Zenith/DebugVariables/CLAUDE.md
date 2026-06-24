# Debug Variables System

## Overview

Hierarchical debug variable tree for runtime inspection and modification of engine parameters. Tools-only (`#ifdef ZENITH_TOOLS`). Uses ImGui for rendering.

## Files

- `Zenith_DebugVariables.h` - Registration API and tree structure
- `Zenith_DebugVariables.cpp` - Node `ImGuiDisplay()` template implementations and ImGui widget bindings

## Architecture

Variables are organized in a tree via path segments (e.g., `{"Flux", "SSR", "Enable"}`). Leaf nodes store references/pointers to the original variables (zero-copy). The tree renders via ImGui with appropriate widgets per type.

The public `Zenith_DebugVariables` API class (held on `g_xEngine`) owns a `Zenith_DebugVariableTree m_xTree`; its inline `Add*` methods construct the matching leaf node and hand it to `m_xTree.AddLeafNode()`. `Zenith_DebugVariableTree` holds the `Node` hierarchy (path segments + child nodes + leaves) and the leaf-node type definitions.

## Registration API

All registration functions are instance methods on `Zenith_DebugVariables` (held on `g_xEngine`, accessed via `g_xEngine.DebugVariables()`):

| Function | Parameters | Widget |
|----------|-----------|--------|
| `AddBoolean` | `path, bool&` | Checkbox |
| `AddFloat` | `path, float&, min, max` | Slider |
| `AddUInt32` | `path, uint32_t&, min, max` | Slider |
| `AddVector2/3/4` | `path, Vector&, min, max` | Multi-slider |
| `AddUVector4` | `path, UVector4&, min, max` | 4-component integer input |
| `AddFloat_ReadOnly` | `path, float&` | Display only |
| `AddUInt32_ReadOnly` | `path, uint32_t&` | Display only |
| `AddUInt64_ReadOnly` | `path, uint64_t&` | Display only |
| `AddButton` | `path, void(*)()` | Button |
| `AddTexture` | `path, const Flux_ShaderResourceView&` | Texture preview |
| `AddTextureCallback` | `path, const Flux_ShaderResourceView*(*)()` | Texture preview via callback |
| `AddText` | `path, string&` | Text display |

Path is `std::vector<std::string>` of category segments.

## Node Types

All node types derive from `LeafNodeBase` (in `Zenith_DebugVariableTree`) and implement its pure virtual `ImGuiDisplay()`:

- `LeafNode<T>` - Editable variable with reference
- `LeafNodeWithRange<V, R>` - Editable with min/max bounds
- `LeafNodeReadOnly<T>` - Display-only variable
- `PfnLeafNode` - Callable button (function pointer)
- `TextNode` - Static text display
- `TextureCallbackLeafNode` - Texture preview resolved each frame via an SRV callback (used by `AddTextureCallback`)

## Usage Pattern

Engine subsystems register their debug variables during initialization:

```cpp
static bool s_bEnabled = true;
static float s_fIntensity = 1.0f;
g_xEngine.DebugVariables().AddBoolean({"Flux", "SSR", "Enable"}, s_bEnabled);
g_xEngine.DebugVariables().AddFloat({"Flux", "SSR", "Intensity"}, s_fIntensity, 0.f, 2.f);
```

Variables appear in the Debug Variables panel organized by path hierarchy.

## Constraints

- Maximum category name length: 64 characters
- Enum types supported via `AddUInt32<T>` template with `static_assert(std::is_enum<T>())`
- Variables must outlive the tree (references, not copies)
