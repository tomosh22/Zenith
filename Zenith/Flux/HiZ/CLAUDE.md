# Flux_HiZ - Hierarchical Z-Buffer System

## Overview

The Hi-Z (Hierarchical Z-Buffer) system generates a depth pyramid from a render view's depth buffer. This mip chain enables efficient depth-based operations like screen-space reflections (SSR) and screen-space GI (SSGI), and is a plausible future input to occlusion culling.

The chain is **per render view**, not per frame: `SetupRenderGraph` builds one
independent transient + per-mip pass chain for **every active full-pipeline
view** (see [Per-view setup and naming](#per-view-setup-and-naming)). Slot 0 —
the main camera — reduces the render-resolution scene depth; the material-preview
view builds its own smaller pyramid while it is up. Every accessor
(`GetHiZAttachment` / `GetHiZSRV` / `GetMipCount` / `GetMipSRV` / `GetMipUAV`)
takes a `uViewSlot`, defaulting to `kuFluxViewSlotMain`.

## Architecture

### Pass placement

A view's mip-0 pass declares `Reads(that view's depth)` and `Writes(that view's HiZ mip 0)`; each later mip pass reads mip N-1 and writes mip N. Because the render graph topo-sorts on declared dependencies, the chain automatically runs:

- **after** any pass that writes to that view's depth (terrain, opaque static meshes, animated meshes, instanced foliage, grass)
- **before** any pass that reads that view's HiZ chain (SSR raymarch, SSGI raymarch, future occlusion-culling)

Grass blades reach the depth buffer through the `"Grass GBuffer"` pass: they are generated on the GPU every frame and write depth like any other opaque geometry, so the chain built here already includes them. HiZ is **not** an input to grass — the placement compute shader culls whole tiles against CPU-supplied frusta and knows nothing about occlusion. Per-blade HiZ culling is a plausible future hook rather than current behaviour, and would need the HiZ chain declared as a read by the placement pass (transients are deliberately excluded from the graph's cross-frame cyclic seeding, so a previous-frame chain is not available for free).

There is no explicit ordering enum — the dependency declarations alone produce the correct placement.

### Per-view setup and naming

`SetupRenderGraph` does exactly two things: it stores the per-BUILD graph
back-ref (`m_pxGraph`, which every view's `GetHiZBuffer` resolves through, so it
is set once and outside the walk), then it drives
`Flux_RenderViewRegistry::ForEachActiveFullPipelineView`. The registry decides
membership by view **properties**, never by slot number:

- slot 0 (main) always qualifies — active + full-pipeline from construction;
- the material-preview slot joins while its owner has it active, so its
  transients exist exactly when its passes do (the graph's unused-transient
  validation requires that);
- shadow cascades are depth-only (`m_bFullPipeline == false`) and never get a
  chain.

The walk's callback is a **captureless lambda written inside
`SetupRenderGraph`**, not a file-static free function like `ExecuteHiZMip`:
`SetupViewPasses` is private, and a closure declared in a member body inherits
the class's access. `this`, the graph and the hoisted `Flux_GraphicsImpl&` all
travel through the registry's `void* pCtx` via a small local struct.

Each view's base dims come from `Flux_GraphicsImpl::GetViewSetupDims(uSlot)` —
the same single derivation `SetupTransients` sized that view's depth buffer with,
so a HiZ chain can never disagree with the depth it reduces. `UpdateMipCountFromSwapchain`
is the one thing that stays **main-only and outside the walk**: it seeds slot 0's
mip count before the first `SetupRenderGraph` and on a resolution change.

Pass names come from **one** base table (`s_aszHiZPassNames`, 12 literals
`"HiZ Mip 0"`…`"HiZ Mip 11"`) through `Flux_ViewPassName(base, uViewSlot)`
(`Flux/RenderViews/Flux_ViewPassNames.h`). There is no second `" (Preview)"`
table and no `uViewSlot == kuFluxViewSlotMain` ternary:

- slot 0 gets the base pointer back **by identity**, so the main view's pass
  names — which profiling labels, `FindPass` and `SetPassForceDisabled` key off
  — are byte-for-byte the historical ones;
- any other slot gets an interned, static-lifetime `"<base> (<suffix>)"`; the
  preview slot composes exactly `"HiZ Mip N (Preview)"`.

`Flux_HiZ.Tests.inl` pins both halves (the 12 base literals and the 12 composed
preview names) plus the `ComputeMipCount` formula.

### Files

| File | Purpose |
|------|---------|
| [Flux_HiZImpl.h](Flux_HiZImpl.h) | `Flux_HiZImpl` class declaration, constants |
| [Flux_HiZ.cpp](Flux_HiZ.cpp) | Implementation, pipeline setup, the pass-name base table |
| [Flux_HiZ.Tests.inl](Flux_HiZ.Tests.inl) | Unit tests (`HiZ` category), included at the bottom of `Flux_HiZ.cpp` |
| [Flux_HiZ_Shaders.h](Flux_HiZ_Shaders.h) | Shader `Flux_ShaderDecl`s + `apxALL[]` |
| [../Shaders/HiZ/Flux_HiZ_Generate.slang](../Shaders/HiZ/Flux_HiZ_Generate.slang) | Slang compute shader for mip generation |

## Implementation Details

### Render Target

- **Format:** `R32G32_SFLOAT` (two-channel 32-bit float: R = min depth, G = max depth)
- **Size:** one per view, at that view's `GetViewSetupDims` (mip 0 is 1:1 with the view's depth buffer), with a complete mip chain
- **Memory Flags:** `MEMORY_FLAGS__UNORDERED_ACCESS | MEMORY_FLAGS__SHADER_READ`

### Per-Mip Views

Each view's HiZ chain is a render-graph **transient** (created in
`SetupViewPasses` via `CreateTransient`, one handle per slot in
`m_axHiZBufferHandles`). The graph owns the image; per-mip SRV/UAV views are
resolved on demand from the view's transient attachment:

```cpp
static constexpr u_int uHIZ_MAX_MIPS = 12;  // full mip chain up to ~4K (max dimension <= 4095); larger clamps

// Chain length for a view of the given base dims: floor(log2(max(w,h))) + 1,
// clamped to uHIZ_MAX_MIPS. Both dims must be > 0 (floor(log2(0)) is UB).
static u_int ComputeMipCount(u_int uWidth, u_int uHeight);

Flux_ShaderResourceView&          GetMipSRV(u_int uMip, u_int uViewSlot = kuFluxViewSlotMain);  // Read previous mip — GetHiZBuffer(uViewSlot).SRV(uMip)
Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip, u_int uViewSlot = kuFluxViewSlotMain);  // Write current mip — GetHiZBuffer(uViewSlot).UAV(uMip)
```

`ExecuteHiZMip` is a non-capturing graph callback, so it learns which chain it is
building from `Flux_RenderGraph::GetCurrentRecordingPassViewSlot()` — the slot the
pass declared with `.View(uViewSlot)` — and indexes `m_auViewWidths` /
`m_auViewHeights` / `m_auMipCounts` with it. Nothing is passed through the pass's
user data but the mip index.

### Mip Generation Algorithm

The shader is a single Slang compute module (`Flux_HiZ_Generate.slang`), writing
both channels (`R` = min depth, `G` = max depth) every dispatch.

1. **Mip 0:** Sample the recording view's depth buffer 1:1. One HiZ texel maps to one depth
   texel, so min and max are set equal (`fMinDepth = fMaxDepth = fDepth`). Min/max
   divergence only begins at mip 1+.
2. **Mips 1-N:** Sample 2x2 from the previous mip; take min of the `.r` (min) lanes
   and max of the `.g` (max) lanes.

```slang
// For mip > 0: sample 2x2 from previous mip's .rg (min,max) channels
float2 xTexelSize = 1.0 / float2(pushConstants.u_uOutputWidth * 2,
                                 pushConstants.u_uOutputHeight * 2);
float2 xBaseUV = (float2(xPixelCoords) * 2.0 + 0.5) * xTexelSize;

float2 d00 = g_xInputTex.SampleLevel(xBaseUV, 0).rg;
float2 d10 = g_xInputTex.SampleLevel(xBaseUV + float2(xTexelSize.x, 0.0), 0).rg;
float2 d01 = g_xInputTex.SampleLevel(xBaseUV + float2(0.0, xTexelSize.y), 0).rg;
float2 d11 = g_xInputTex.SampleLevel(xBaseUV + xTexelSize, 0).rg;

float fMinDepth = min(min(d00.r, d10.r), min(d01.r, d11.r));
float fMaxDepth = max(max(d00.g, d10.g), max(d01.g, d11.g));
```

### Barrier Handling

`Flux_RenderGraph::SynthesizeBarriers` (called from `Compile`) walks the
execution order, tracks per-(attachment, mip, layer) access state for images
and per-buffer access state for buffers, and populates each pass's
`m_xPrologueBarriers` list. The Vulkan backend
(`Zenith_Vulkan.cpp::RecordCommandBuffersTask`) consumes that list — image
entries via `ImageTransition`, buffer entries via `BufferBarrier` — right
before each pass executes, outside any active render pass so
`vkCmdPipelineBarrier` is unrestricted.

For the HiZ chain, each per-mip pass uses the fluent builder:

```cpp
const Flux_PassHandle xPass = xGraph.AddPass(Flux_ViewPassName(s_aszHiZPassNames[uMip], uViewSlot), ExecuteHiZMip)
    .UserData(uMip)                                                                     // typed, no void* cast
    .View(uViewSlot)                                                                    // which view this pass records for
    .WritesTransient(m_axHiZBufferHandles[uViewSlot], RESOURCE_ACCESS_WRITE_UAV, uMip, 1); // current mip
if (uMip == 0)
    xGraph.Read(xPass, g_xEngine.FluxGraphics().GetDepthAttachment(uViewSlot), RESOURCE_ACCESS_READ_SRV);
else
    xGraph.ReadTransient(xPass, m_axHiZBufferHandles[uViewSlot], RESOURCE_ACCESS_READ_SRV, uMip - 1, 1); // previous mip
```

The write is chained on the builder; the read is a separate `Read`/`ReadTransient`
call against the returned `Flux_PassHandle` (mip 0 reads that view's depth attachment,
mip N>0 reads the single prior mip of that view's own chain). From those declarations
the graph emits:
- Mip pass `N`: `UNDEFINED → RESOURCE_ACCESS_WRITE_UAV` (GENERAL) on mip `N` before the dispatch
- Mip pass `N+1`: `RESOURCE_ACCESS_WRITE_UAV → RESOURCE_ACCESS_READ_SRV` (SHADER_READ_ONLY) on mip `N` before the dispatch
- The next graphics consumer for that slot — **SSR or SSGI, and only those two** —
  declaring `Reads(GetHiZAttachment(uViewSlot), READ_SRV, 0, GetMipCount(uViewSlot))`
  triggers `RESOURCE_ACCESS_WRITE_UAV → RESOURCE_ACCESS_READ_SRV` on the final mip
  before its render pass begins. **SSAO does not read the HiZ chain** (it samples
  scene depth directly); it is merely registered alongside SSR/SSGI in the feature
  order.

No inline `Flux_CommandImageTransition` calls live in `ExecuteHiZMip` — the
graph owns synchronisation end-to-end. Per-(mip, layer) state tracking
ensures consecutive mip passes don't over-synchronise.

## Public Interface

The subsystem is a non-static `Flux_HiZImpl` owned by `Zenith_Engine`; callers
reach it via `g_xEngine.HiZ()`. (The historical `Flux_HiZ` static facade was
removed in Phase 9.)

```cpp
class Flux_HiZImpl
{
public:
    void Initialise();
    void Shutdown();
    void BuildPipelines();

    // Builds one chain per ACTIVE FULL-PIPELINE view (see "Per-view setup and naming").
    void SetupRenderGraph(Flux_RenderGraph& xGraph);

    // Accessors for consumers (SSR, SSGI). Every one is indexed by view slot and
    // defaults to the main camera, so single-view callers stay unchanged.
    Flux_RenderAttachment&            GetHiZAttachment(u_int uViewSlot = kuFluxViewSlotMain);
    Flux_ShaderResourceView&          GetHiZSRV       (u_int uViewSlot = kuFluxViewSlotMain);  // Full mip chain
    u_int                             GetMipCount     (u_int uViewSlot = kuFluxViewSlotMain) const;
    Flux_ShaderResourceView&          GetMipSRV(u_int uMip, u_int uViewSlot = kuFluxViewSlotMain);  // Single mip
    Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip, u_int uViewSlot = kuFluxViewSlotMain);  // Compute write

    // Pure helper — chain length for a view of the given base dims (see Per-Mip Views).
    static u_int ComputeMipCount(u_int uWidth, u_int uHeight);

    bool IsEnabled() const;
};
```

A consumer must pass the slot it is building for. `SSR` and `SSGI` do exactly
that (`GetHiZAttachment(uViewSlot)` / `GetMipCount(uViewSlot)` /
`GetHiZSRV(uViewSlot)`), so the preview view reflects and gathers against its own
smaller pyramid — which has fewer mips than the main view's, hence the per-view
`GetMipCount` both features clamp their start mip against.

## Cross-Subsystem Dependencies

`Initialise()` takes no parameters. Cross-subsystem deps (the swapchain,
`Flux_GraphicsImpl`, `Flux_RendererImpl`) are engine-owned singletons reached
via `g_xEngine.X()` at point of use — this is the Flux-wide pattern (the
earlier injected-member-pointer DI seam was reverted in favour of direct
reaches). Two non-capturing fn-pointer trampolines cannot capture `this` and
so re-enter via `g_xEngine.HiZ()` to reach the singleton instance: the
resolution-change callback registered in `Initialise`, and the `ExecuteHiZMip`
graph callback (`void(*)(Flux_CommandBuffer*, void*)`).

The per-view setup callback is deliberately **not** one of them. It has a
`void* pCtx` to travel through, so `SetupRenderGraph` passes `this`, the graph
and a single hoisted `Flux_GraphicsImpl&` in a local struct instead of adding
another singleton reach.

## Enable Flag

Hi-Z generation is gated by `Zenith_GraphicsOptions::Get().m_bHiZEnabled` (see
`Core/Zenith_GraphicsOptions.h`), checked by `IsEnabled()` — it is **not** a
registered debug variable. The enable bit lives with the other render-feature
toggles in `Zenith_GraphicsOptions`, not under a `Flux/HiZ/...` debug-variable path.

## Usage

### In SSR (Ray Marching)

```glsl
// Sample Hi-Z at current mip level
float fHiZDepth = textureLod(g_xHiZTex, xSampleUV, float(uMipLevel)).r;

// Compare ray depth against Hi-Z
if (fRayDepth > fHiZDepth)
{
    // Ray is below surface at this mip level
    // Refine to finer mip or confirm hit at mip 0
}
```

### In Occlusion Culling

```glsl
// Sample conservative depth for object's screen bounds
float fMinHiZ = textureLod(g_xHiZTex, xObjectCenter, fMipForObjectSize).r;

// If object's nearest depth is behind Hi-Z, it's occluded
if (fObjectNearZ > fMinHiZ)
    discard;  // or skip rendering
```

## Performance Considerations

- Compute shader uses 8x16 (= 128-thread) workgroups
- Sequential mip dispatch (mip N depends on mip N-1)
- Total dispatches = mip count **per active full-pipeline view** (typically 10-12 for 1080p/4K; a 512² preview adds 10 more while it is up)
- GPU memory: ~1.33x each view's depth buffer (due to mip pyramid)
