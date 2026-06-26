# Flux_Decals — Deferred Screen-Space Box Decals

## Overview

Industry-standard deferred decal renderer (FidelityFX / UE / Frostbite shape).
Each decal is an oriented unit cube in world space. The Apply pass rasterizes
the cube, samples the depth buffer per fragment, reconstructs world position,
transforms to decal-local cube space, discards if outside the unit box, and
writes blended values into all three G-buffer MRTs. The G-buffer is consumed
downstream by SSR, SSGI, SSAO, and DeferredShading as if the decal were
stamped onto the original geometry — reflections, ambient occlusion, and
lighting all interact with the decal correctly.

## Files

| File | Purpose |
|------|---------|
| [Flux_DecalsImpl.h](Flux_DecalsImpl.h) | Public API: `Initialise`, `Shutdown`, `BuildPipelines`, `SetupRenderGraph`, `SpawnDecal` |
| [Flux_Decals.cpp](Flux_Decals.cpp) | Implementation: CPU pool, GPU buffer upload, two-pass setup, basis math |
| [../Shaders/Decals/Flux_Decals_NormalsCopy.slang](../Shaders/Decals/Flux_Decals_NormalsCopy.slang) | Pre-Apply: copy live normalsAmbient → transient |
| [../Shaders/Decals/Flux_Decals_Apply.slang](../Shaders/Decals/Flux_Decals_Apply.slang) | Apply: instanced cube → blend into 3 G-buffer MRTs |

## Two-pass decomposition

The decal pass needs to **read pre-decal scene normals** (for the surface-
alignment test that prevents leakage onto perpendicular surfaces inside the
decal volume) AND **write all three live G-buffer MRTs**. Flux's render graph
does not expose Vulkan input attachments / subpass self-dependencies, so the
intended pattern is a two-pass decomposition with a transient copy:

1. **Decal Normals Copy** — fullscreen quad that samples the live
   normalsAmbient MRT and writes a `RGBA16F` transient.
2. **Decal Apply** — instanced cube draw. Reads depth + the cloned normals;
   writes diffuse / normalsAmbient / material under per-attachment alpha
   blend.

The transient memory aliases away after the second pass completes, so the cost
is one extra full-screen RGBA16F bandwidth, not a permanent allocation.

## Pass placement

Inserted between the **G-buffer writers** (Terrain, StaticMeshes, Primitives,
AnimatedMeshes, InstancedMeshes, Skybox — Grass does not qualify because it
writes the forward-lit HDR target, not G-buffer MRTs) and the
**G-buffer readers** (HiZ, SSR, SSGI, SSAO, DeferredShading). Source-side
registration order matters — the topo sort's `FindBestWriter` picks the
nearest writer in declaration order, so registering Decals after the geometry
block and before HiZ is required. Decals don't touch depth, so HiZ ordering
relative to Decals is unconstrained.

## Per-attachment blend state

Every Apply MRT uses:

| MRT | srcColor | dstColor | srcAlpha | dstAlpha | writeMask | Rationale |
|---|---|---|---|---|---|---|
| 0 (diffuse)        | SRCALPHA | ONEMINUSSRCALPHA | ZERO | ONE | R\|G\|B | Blend decal color into existing; preserve original diffuse alpha |
| 1 (normalsAmbient) | SRCALPHA | ONEMINUSSRCALPHA | ZERO | ONE | R\|G\|B | Blend decal normal into existing RGB; preserve AO (alpha) |
| 2 (material)       | SRCALPHA | ONEMINUSSRCALPHA | ZERO | ONE | R\|G\|B | Blend decal material; preserve unused alpha |

`srcAlpha=ZERO, dstAlpha=ONE` makes the alpha math a no-op (output = `dstA`).
Combined with the `R|G|B` write mask, even garbage in the shader's `.w`
output cannot reach the attachment.

The plumbing for separate alpha-blend factors and a per-attachment color
write mask was added to `Flux_BlendState` ([Flux_Types.h](../Flux_Types.h))
specifically for this work; the defaults reproduce the previously-hardcoded
Vulkan behaviour byte-identical, so existing call sites are unaffected.

## DecalInstance struct

```cpp
struct DecalInstance
{
    Zenith_Maths::Matrix4 m_xWorld;          // 64
    Zenith_Maths::Matrix4 m_xWorldInverse;   // 64 — precomputed; no inverse() in shader
    Zenith_Maths::Vector4 m_xAxisOpacity;    // 16 — xyz = unit-length projection axis, w = fade opacity
    Zenith_Maths::Vector4 m_xParams;         // 16 — x = normal-alignment threshold, y = mode (0 = procedural, 1 = brush indicator), zw reserved
    Zenith_Maths::Vector4 m_xColour;         // 16 — brush-indicator tint (rgb) + master alpha (w); unused by mode 0
};
static_assert(sizeof(DecalInstance) == 176, ...);
```

The projection axis lives in `m_xAxisOpacity.xyz` so the shader doesn't have
to extract and normalize a column of `m_xWorld` (whose columns include the
per-axis scale). Stored explicitly at spawn time — same value that drove the
basis construction.

## CPU pool / GPU staging split

The CPU pool is a **64-slot ring buffer** indexed by `s_uNextSlot`. Calling
`SpawnDecal` overwrites the next slot in the ring; if it was occupied, the
oldest decal is recycled. The pool is initialised eagerly so test fixtures
that don't spin up Flux can still call `SpawnDecal` and observe the
bookkeeping.

The GPU staging buffer is **dense**: the per-frame Prepare callback walks
the CPU ring, packs active slots into `[0..uActiveDecalCount-1]`, and uploads
to the dynamic SSBO. The Apply shader's `SV_InstanceID` indexes the dense
copy, never the sparse ring.

## Idle-frame cost

Both passes are always-enabled. We tried gating via
`Flux_RenderGraph::SetEnabled` to skip work when `uActiveDecalCount == 0`,
but the render graph deliberately skips Prepare callbacks for disabled
passes (`Flux_RenderGraph_Execution.cpp:155`) — disabling the pass would
prevent Prepare from ticking lifetimes, which would mean the active count
could never lift off zero once it was at zero. The trade-off: when no
decals are active, the Execute callbacks early-out at the top without
recording any commands; only the (empty) render-pass setup runs. The
NormalsCopy and Apply render passes still get scheduled but record no
draws. If profiling later shows this idle cost is meaningful, the right
fix is to add a permanently-enabled "tick" pass that owns Prepare and
flips the visible passes' enable bits — but that complexity isn't
warranted at v1.

## Debug visualisation

Toggle `Render → Decals → Debug Spheres` (`dbg_bDecalDebugSpheres`) to
draw a yellow `Flux_Primitives` sphere at every active decal's hit point,
sized to the decal's XY half-extent. Off by default; spheres are
re-queued every frame from the Prepare lifetime tick because primitives
are cleared after rendering.

## Bullet-hole pattern (v1)

The Apply fragment shader produces a procedural circular bullet hole:
- Diffuse: dark scorch (RGB = 0.05, 0.04, 0.03), alpha falls off via
  `smoothstep` at the box's XY edge.
- Normal: small inward concavity along the projection axis (decal-local +Z),
  transformed to world space using the basis vectors stripped of scale.
- Material: roughness raised to 0.85, metallic = 0, emissive = 0.

The `pxTexture` parameter on `SpawnDecal` is reserved for v1's procedural
default and v2's texture-array support. Today the parameter is ignored.

## Editor brush-indicator slot (mode 1)

`SetEditorDecal(centre, diameter, verticalExtent, colour, texture)` arms ONE
persistent slot outside the 64-slot gameplay ring (instance capacity is
`uMAX_DECAL_INSTANCES = 65`). It projects straight down with a permissive
normal-alignment threshold (0.05) and renders in a second shader mode
(`m_xParams.y = 1`): samples the brush mask via the bindless table
(`g_axTextures[NonUniformResourceIndex(asuint(m_xParams.z))]` — the brush's
bindless slot, stored per-decal in `m_xParams.z`; `SetEditorDecal` marks the
texture bindless on arm), tints it with the per-call `m_xColour`, and writes
**diffuse only** (alpha 0
on the normals/material MRTs — an indicator must not stamp a dent or
roughness change). One-frame lifetime: the arm flag is consumed by the next
Prepare/pack, so the caller re-arms every frame while its cursor is valid and
a missed frame makes the indicator vanish instead of going stale. Used by
`Zenith_TerrainEditor` for the terrain brush cursor; the brush mask itself is
a generated artifact (`Zenith/Assets/Textures/Brushes/BrushIndicator.ztxtr`,
rebuilt at every editor boot by
`Zenith_TerrainEditor::RegenerateBrushTextures`).

## Future enhancements

- **Atlas / texture-array support**: the `pxTexture` parameter is the API
  forward-compatibility point. Slot it into a bindless texture array indexed
  by a per-decal `uint32` and the shader can pick its source albedo / normal
  maps per instance.
- **Stencil masking**: tag decal-receiving surfaces with a stencil bit at
  G-buffer time, then skip the alignment heuristic in favour of a hard
  stencil test in the Apply pass. Removes the leakage-onto-perpendicular-
  surface failure mode entirely.
- **Tile-based mobile path**: add native Vulkan input-attachment support to
  `Flux_RenderGraph` so the two-pass decomposition collapses to a single
  pass with a subpass self-dependency. Desktop is unaffected; mobile sees a
  bandwidth win.
