# Shader Coding Style Guide

Conventions for GLSL shader source files in `Zenith/Flux/Shaders/`.

## File Naming

- `Flux_<Subsystem>_<Purpose>.<ext>` (e.g., `Flux_StaticMeshes_ToGBuffer.vert`)
- Extensions: `.vert`, `.frag`, `.comp`, `.tesc`, `.tese`, `.geom` for entry points; `.fxh` for headers

## Variable Naming (Hungarian Prefix)

| Prefix | Scope / Type | Example |
|--------|-------------|---------|
| `g_` | Global uniform / buffer | `g_xViewMat`, `g_xSunDir_Pad` |
| `a_` | Vertex attribute (`in`) | `a_xPosition`, `a_xUV` |
| `o_` | Varying output (`out`) | `o_xNormal`, `o_fBitangentSign` |
| `x` | vec / mat / struct | `xWorldPos`, `xNormalMatrix` |
| `f` | float | `fDepth`, `fRoughness` |
| `u` | uint | `uMipLevel`, `uInstanceID` |
| `i` | int | `iCascade` |
| `b` | bool | `bUseVAT` |
| `ax` | array of vec/struct | `axOffsets`, `axShadowMats` |

## Uniform Block Naming

- Frame-level constants: `FrameConstants` (set 0, binding 0) via `Common.fxh`
- Per-draw material data: `DrawConstants` (set 1, binding 0) via `DrawConstants.fxh`
- Subsystem-specific blocks: use descriptive names (`SSAOGenerateConstants`, `LightConstants`)

## Include Guards

All `.fxh` files must have `#ifndef / #define / #endif` guards:
```glsl
#ifndef MY_HEADER_FXH
#define MY_HEADER_FXH
// content
#endif // MY_HEADER_FXH
```

## Variant Pattern

Shader variants use the entry-point-file pattern:
```glsl
// Flux_Mesh_Static_ToGBuffer.vert
#version 450 core
#define MESH_STATIC
#include "../Meshes/Mesh_Vert.fxh"
```

The shared `.fxh` body uses `#ifdef MESH_STATIC / MESH_ANIMATED / MESH_INSTANCED` to switch code paths. The `#define SHADOWS` pattern works the same way for shadow-pass variants.

## Compute Shader Documentation

Every `.comp` file must include a header block:
```glsl
// ============================================================================
// <Purpose — one line>
// Workgroup: <NxMxK> (<thread count> threads — <rationale>)
// Shared memory: <size or "none">
//
// <2-3 sentence description of what the shader does>
// ============================================================================
```

## Descriptor Set Convention

See `BINDING_CONVENTION.md` for the current per-subsystem layout.

General pattern:
- **Set 0**: frame-level (camera, sun, screen dims, subsystem constants)
- **Set 1**: per-draw (material block, textures, instance buffers)
- **Set 2+**: reserved for subsystem-specific extensions

## Code Style

- Tabs for indentation (matching C++ convention)
- Opening braces on same line for functions and control flow
- Constants as `const float` / `const int` with SCREAMING_SNAKE or descriptive names
- Prefer helper functions in `.fxh` headers over inline duplication
