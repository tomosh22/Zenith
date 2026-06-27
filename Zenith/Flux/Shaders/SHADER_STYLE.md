# Shader Coding Style Guide

Conventions for Slang shader source files in `Zenith/Flux/Shaders/`. Slang is
the only supported source language post-migration; the legacy GLSL paths
(`.vert`/`.frag`/`.comp`/`.fxh`) and the `enableGLSL` Slang flag have been
removed.

## File Naming

- `Flux_<Subsystem>_<Purpose>.slang` (e.g., `Flux_StaticMesh_ToGBuffer.slang`).
- One `.slang` mega-file per subsystem with multiple named entry points where
  the variants are closely related (e.g. mesh static / animated / instanced
  share constants and helpers).
- Shared modules live under `Common/` (`Common.Frame`, `Common.PBR`,
  `Common.GBuffer`, `Common.Material`, …) and are imported via `import`.

## Entry Points

- Vertex entry: `vsMain`. Fragment entry: `psMain`. Compute entry: `csMain`.
- Multi-variant programs may suffix the variant (`vsMainStatic`, `vsMainAnimated`,
  …). The Slang shader registry (`Flux_ShaderRegistry`) records the entry-point
  names alongside the module path; runtime code looks programs up by
  `FluxShaderProgram` IDs generated into `Flux/Shaders/Generated/`.
- Slang's per-entry SPIR-V emitter renames each entry point to `main` in the
  emitted blob; the engine binds the Vulkan-visible name `"main"` regardless
  of the source-level name.

## Variable Naming (Hungarian Prefix)

| Prefix | Scope / Type | Example |
|--------|-------------|---------|
| `g_` | Global parameter (uniform / buffer / texture / sampler) | `g_xFrame`, `g_xAlbedoTex` |
| `a_` | Vertex stage input | `a_xPosition`, `a_xUV` |
| `o_` | Inter-stage varying output | `o_xNormalWS`, `o_fBitangentSign` |
| `x` | vec / mat / struct | `xWorldPos`, `xNormalMatrix` |
| `f` | float | `fDepth`, `fRoughness` |
| `u` | uint | `uMipLevel`, `uInstanceID` |
| `i` | int | `iCascade` |
| `b` | bool | `bUseVAT` |
| `ax` | array of vec/struct | `axOffsets`, `axShadowMats` |

## Constant Buffers and Parameter Blocks

- Frame-level data lives in `Common.Frame`'s `FrameConstants` cbuffer (set 0,
  binding 0). Per-draw / per-material data lives in `Common.DrawConstants` and
  `Common.Material`.
- Subsystem-specific blocks are `cbuffer` declarations named
  `<Subsystem>Constants` (e.g. `SSAOGenerateConstants`).
- Resource bindings come out as flat `set/binding` slots in reflection;
  `ParameterBlock<T>` from Slang is used where it cleanly groups related
  resources by update rate, but the engine binder still keys on the resource
  name.
- Matrix layout: `defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR`
  is set at session creation so HLSL-syntax matrices match the std140
  GLM-uploaded layout. Don't transpose at the call site.

## Include / Import

- Slang `import Common.Frame;` replaces the old `#include "Common.fxh"`. The
  `Flux_SlangCompiler` adds `Zenith/Flux/Shaders/` as a search root, so module
  names are relative to that directory with `.` for path separators.
- There is no `#ifndef` guard pattern — Slang modules are first-class units.

## Variant Pattern

Multiple named entry points in a single `.slang` file replace the GLSL
`#define VARIANT` pattern. Each variant is registered as its own
`FluxShaderProgram` ID with its own entry-point name; the registry resolves
which entries get emitted.

## Compute Shader Documentation

Every compute entry point should include a banner comment:
```slang
// ============================================================================
// <Purpose — one line>
// Workgroup: <NxMxK> (<thread count> threads — <rationale>)
// Shared memory: <size or "none">
//
// <2-3 sentence description of what the shader does>
// ============================================================================
[numthreads(N, M, K)]
void csMain(...) { ... }
```

Use `[numthreads(...)]` (HLSL syntax). For texture sampling inside compute,
prefer `SampleLevel(uv, mip)` over `Sample(uv)` — implicit derivatives in a
compute thread require `ComputeDerivativeGroupQuadsKHR` and Slang will fail
to lower without it.

## Descriptor Set Convention

The binding model is a frequency-based spine, declared once in `Common/Bindings.slang`
(text-`#include`d first by every shader) and **enforced in code**
(`Flux_ShaderCatalog::ValidateFrequencyTaxonomy`, `Flux_PersistentSetLayouts::ValidateCanonicalGroup`,
`Flux_ViewSetBinding`):

- **Set 0 — GLOBAL**: view-invariant per-frame data (`g_xGlobal` + `g_axMaterials`).
- **Set 1 — VIEW**: per-camera data + promoted view-frequency resources (`g_xView`, CSM,
  shadow matrices, clustered-lighting buffers, IBL — see `Flux/Flux_PersistentSetLayouts.h`).
- **Set 2 — BINDLESS**: the unbounded `g_axTextures[]` table.
- **Set 3 — PASS / Set 4 — DRAW**: each shader's own local `ParameterBlock`s.

Shaders carry NO `[[vk::binding]]` — every resource is a member of its frequency block.
(The old per-subsystem `BINDING_CONVENTION.md` is retired.)

## Vertex-Stage Buffer Reads

Storage-buffer reads from a vertex shader require Vulkan
`vertexPipelineStoresAndAtomics` if Slang emits them as RW. Use
`StructuredBuffer<T>` (read-only — Slang emits `NonWritable`) instead of
`RWStructuredBuffer<T>` whenever the vertex stage only reads the buffer.

## Code Style

- Tabs for indentation (matching C++ convention)
- Opening braces on the same line for functions and control flow
- Constants as `static const` with descriptive names
- Helper functions live in shared modules under `Common/`; prefer importing
  over inline duplication
