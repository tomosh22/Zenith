# Flux Primitives

## Overview

`Flux_PrimitivesImpl` draws asset-free procedural shapes (sphere / cube / line /
capsule / cylinder / triangle) straight into the G-buffer, with the colour and
model matrix supplied per draw through a push constant. There is no mesh asset,
no material asset, and no instancing — one draw call per submitted shape.

It owns **two independent channels**, and picking the wrong one is the classic
bug here:

| | Debug channel | Gameplay channel |
|---|---|---|
| Submit API | `AddSphere` / `AddCube` / `AddLine` / `AddCapsule` / `AddCylinder` / `AddTriangle` (+ the `AddCross`/`AddCircle`/`AddArrow`/`AddConeOutline`/`AddArc`/`AddPolygonOutline`/`AddGrid`/`AddAxes` composites, all of which decompose into `AddLine`) | `SubmitGameplayCylinderAndSphere` |
| CPU queues | `m_xSphereInstances`, `m_xCubeInstances`, `m_xLineInstances`, `m_xCapsuleInstances`, `m_xCylinderInstances`, `m_xTriangleInstances` | `m_xGameplaySphereInstances`, `m_xGameplayCylinderInstances` |
| `Graphics/Primitives/Enabled` (debug variable bound to `Zenith_GraphicsOptions::m_bPrimitivesEnabled`, default true) | **Gates it.** Unchecked -> not drained, not drawn. | **Ignored.** Drained and drawn either way. |
| Shading | matte default-lit: ambient 0.2, roughness 0.8, metallic 0, `GBUFFER_SHADING_DEFAULT_LIT` | `GBUFFER_SHADING_UNLIT` + HDR emissive (`colour * 0.5`); DeferredShading's unlit branch returns `diffuse + emissive` and skips all lighting |
| Intended use | editor/diagnostic overlays, physics and navmesh visualisation, anything a developer toggles | shipping player-facing cues that must survive a tools user unchecking the debug variable |

**A production gameplay cue must not ride the debug channel.** That is not a
style preference: `m_bPrimitivesEnabled` is bound to a live debug variable, so an
`Add*`-based cue disappears from the game the moment someone unchecks a checkbox
in the debug panel. Zenithmon's trainer "spotted" marker shipped that way once
and it is recorded as a defect (`Games/Zenithmon/Docs/Shortfalls.md` 1.8-3c).

## Files

| File | Purpose |
|------|---------|
| `Flux_PrimitivesImpl.h` | The `Flux_PrimitivesImpl` class + the six `Flux_Primitives*Instance` POD types |
| `Flux_Primitives.cpp` | Procedural mesh generation, both submit channels, the `ExecuteGBuffer` graph callback, the render helpers |
| `Flux_Primitives_Shaders.h` | The feature's `Flux_ShaderDecl` + `apxALL` (see Flux/CLAUDE.md "Each feature OWNS its shaders") |

Shader source: `Flux/Shaders/Primitives/Flux_Primitives.slang`; generated
reflection header `Flux/Shaders/Generated/Primitives.h`.

## Where it sits in the frame

`SetupRenderGraph` declares exactly one pass:

```
AddPass("Primitives GBuffer", ExecuteGBuffer)
    .Writes(MRT_INDEX_DIFFUSE / NORMALSAMBIENT / MATERIAL / EMISSIVE, WRITE_RTV)
    .Writes(GetDepthAttachment(), WRITE_DSV)
    // + .Writes(GetVelocityAttachment(), WRITE_RTV) while the TAA velocity latch is on
```

Because "Apply Lighting" `Read`s those same four core MRTs, the topological sort
can only place Primitives **before** deferred lighting. Primitives is a G-buffer
producer, not an overlay — its draws are depth-tested and depth-writing against
the rest of the opaque scene, and they go through the deferred pass like any
other geometry.

### TAA motion vectors: four pipelines, two independent axes

While `FluxGraphics().IsVelocityMRTActive()` is on (TAA's velocity latch), the pass
gains a 5th colour attachment and the record binds the 5-attachment
`Primitives_ToGBufferVelocity` pipelines instead — identical shading plus
`MRT_INDEX_VELOCITY`. There are FOUR pipeline objects because two independent
booleans decide pipeline state:

| | solid | wireframe |
|---|---|---|
| **latch off** (4 MRTs) | `m_xPrimitivesPipeline` | `m_xPrimitivesWireframePipeline` |
| **latch on** (5 MRTs) | `m_xPrimitivesVelocityPipeline` | `m_xPrimitivesWireframeVelocityPipeline` |

**The two axes are resolved in two different places, deliberately.** The latch picks
a `Flux_PrimitivesPipelineSet` once at the top of `ExecuteGBuffer`; the per-cube
`m_bWireframe` flag picks within that pair, exactly as it always did. Neither
selection can swallow the other — which is the trap
`Flux/Terrain/Flux_TerrainPipelineSelect.h` documents at length: a nested
`bVelocity ? velocity : (bWireframe ? ...)` there left the wireframe branch
unreachable in every default run, because TAA ships ON.

> **★ MOVING PRIMITIVES GET CAMERA-ONLY MOTION, AND THAT IS THE DESIGN.**
> `prevWorld == curWorld`, the same reprojection terrain uses. A primitive is
> submitted **by value** every frame (a centre + extents, or a start/end pair) with
> no identity the renderer can key on, so there is no `Flux_PrevTransformCache`
> entry to reproject through — nothing here knows that this frame's sphere is last
> frame's sphere moved. Camera-only is strictly better than the (0,0) primitives
> used to leave behind, and exactly right for the static world annotations that are
> the overwhelming majority of primitive draws. Fixing it means giving the submit
> API a caller-supplied identity, i.e. changing every `Add*` / `SubmitGameplay*`
> call site — do that deliberately, not as a side effect of a TAA change.

## The drain, and the two halves of the early-return bug

`ExecuteGBuffer` is a non-capturing `void(*)(Flux_CommandBuffer*, void*)`
trampoline, so it recovers the singleton via `g_xEngine.Primitives()`. Under one
`m_xInstanceMutex` lock it copies the queues it is going to draw into locals,
clears them, and releases the lock before recording — so recording never holds
the submit mutex.

**The early return is `!bHaveDebugPrimitives && !bHaveGameplayPrimitives`, and it
is the whole point.** The function used to bail on `!m_bPrimitivesEnabled` before
touching anything, which cost two bugs at once:

1. a gameplay cue on this subsystem was silently lost when the debug variable was
   unchecked; and
2. the debug queues were never drained while it was unchecked, so producers kept
   appending into queues nothing consumed.

Now the gameplay queues are drained unconditionally and the debug queues are
drained only when the option is on. **Half (2) is still open for the DEBUG
queues specifically** — with the option off they still accumulate, and
`s_uMaxTriangles = 8192` bounds only the triangle *buffer*, not the queue. That
is pre-existing behaviour, unchanged by the gameplay channel, and it is the
reason `Clear()` exists as a public escape hatch.

## Why `SubmitGameplayCylinderAndSphere` returns a count

It returns `1` only when **both** queues were observed to grow, and `0`
otherwise. The append and both size samples happen under `m_xInstanceMutex`,
which the render-thread drain also takes, so a drain cannot split the payload
between the two measurements or erase it before them.

This exists because of a specific test-integrity failure. A caller that does

```cpp
xPrimitives.AddLine(...); xPrimitives.AddSphere(...); ++m_uSubmitCount;   // WRONG
```

has a counter that stays truthful with the two submit calls **deleted** — every
downstream assertion then watches a proxy for the renderer instead of the
renderer. Callers must `+=` this return value and never increment beside the
call. See `Games/Zenithmon/Components/ZM_Interactable.cpp`
(`SubmitTrainerSpottedIndicator`), which is the one caller today.

## Gotchas

- **A `Zenith_Vector` size delta is the contract, not a bool.** The queues only
  ever grow between a submit's two samples, so a concurrent producer can inflate
  a delta but can never hide a missing primitive of yours.
- **★ `AddLine` IS A FLAT QUAD, AND IT IS CENTRED ON `start`, NOT SPANNING
  `start`->`end`.** Two separate traps, both live and neither fixed here:
  1. *Wrong extent.* `GenerateUnitLine` spans local y in [-1, 1], and
     `RenderLinePrimitives` builds `Translate(m_xStart) * Rot * Scale(t, len*0.5, t)`
     — so the quad runs from `start - dir*len/2` to `start + dir*len/2`. It sticks
     out half a length behind the start and stops half a length short of the end.
     `RenderCylinderPrimitives` gets this right (it translates to the midpoint), so
     `AddLine(A,B)` and `AddCylinder(A,B,...)` do **not** cover the same segment.
     Every `Add*` composite built on `AddLine` (cross, circle, arrow, grid, axes,
     polygon outline) inherits it, which is why those read as "roughly right".
  2. *Flat and camera-independent.* The quad lies in its local XY plane with a +Z
     normal, and `ComputeYAxisAlignment` only aligns local +Y to the direction —
     the roll about that axis is unconstrained and never faces the camera. A
     vertical line is a +Z-facing quad: a bar from the front, ~1 px edge-on from
     the side.

  Together these are exactly why Zenithmon's exclamation mark rendered as "a gold
  sphere with a diagonal stroke": the stem it asked for at `top+0.55 .. top+1.20`
  was actually drawn at `top+0.225 .. top+0.875`, straight through the dot at
  `top+0.25`. **For anything whose shape or extent matters, use a cylinder.**
- **`fEmissiveIntensity` is what selects the shading model**, not a separate
  flag: the fragment shader picks `GBUFFER_SHADING_UNLIT` iff the push-constant
  scalar is `> 0`. The debug channel passes `0.0f` and therefore keeps the exact
  matte path it always had — that is the behaviour-preservation guarantee for
  every other game's debug draws.
- **Debug primitives are LIT.** Boxes and spheres submitted through `Add*` need
  outward/up-facing normals to read correctly (see
  `reference_screen_capture_and_primitive_winding`).
- **A submit is not a pixel.** A non-zero submit count proves the CPU payload
  reached Flux's queues. It does not prove anything was visible: the shape may
  be sub-pixel at range, behind geometry (these draws are depth-tested), or off
  screen. Visual claims need a framebuffer capture.
- **★ DO NOT PREDICT WHAT AN EMISSIVE PRIMITIVE LOOKS LIKE ON SCREEN. MEASURE IT.**
  A gameplay draw submitting linear `(1.0, 0.82, 0.08)` at intensity 0.5 goes
  through the unlit branch as `diffuse + emissive` = `(1.5, 1.23, 0.12)`, which
  reads like "saturated yellow, almost no blue". It is not: after tonemap, bloom
  and TAA it lands at **`RGB(208, 182, 97)`** — blue/red ≈ **0.47**, not ≈ 0.08.
  On 2026-07-29 two hand-rolled screen scans looked for "low blue" and reported
  **zero marker pixels across 539 captured frames** of a marker that was drawing
  correctly the whole time. The false negative was nearly written up as a
  rendering defect. Sample the actual bytes and build the predicate from them.
- **A wall-clock screen-grab loop cannot sample a short beat.** `Tools\capture_viewport.ps1`
  asked for 40 ms and delivered **206 ms** at 2560×1440 (PNG encode dominates the
  loop); dropping to 1280×800 only got it to 81 ms. For anything shorter than
  about a second, use the frame-exact engine path — `Flux_Screenshot::RequestDump`,
  consumed once per `EndFrame` by `Zenith_Vulkan_Swapchain` — from inside the
  frame you care about. It is a no-op on the Null backend, so gate the assertion
  on `Zenith_IsNullRenderer()`.
