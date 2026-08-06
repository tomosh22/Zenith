# Flux Grass & Vegetation Pipeline

## Overview

GPU-driven procedural grass. **Nothing about a blade is persisted.** Every frame
the feature regenerates every blade from scratch on the GPU: three compute passes
place and cull them into a fixed-capacity pool, then two indirect draws sweep a
cubic Bezier blade into the **G-buffer** and two more sweep the LO blade into CSM
cascades 0-1. There is no CPU instance array, no chunk grid and no upload of blade
data — the CPU's whole job is to choose which TILES to dispatch and to stage the
constants.

Regeneration is affordable because a blade is a pure function of its lattice node
(all per-blade randomness keys off `Zenith_TerrainNoise::HashCoords`), and it is
*required* because a blade that changed identity between frames would flicker
under TAA.

## Architecture

```
        GatherGrassFrame (main thread, hung on the Placement pass's .Prepare)
        selects tiles, stages the constants, uploads the frame buffers
                                |
                                v
+---------------------+  "Grass Reset"          (CS) zeroes the 16 indirect slots +
| Reset               |                              the pool cursor, seeds each
+---------------------+                              partition's firstInstance
                                |
                                v
+---------------------+  "Grass Placement"      (CS) one thread per lattice cell,
| Placement           |                              tile-major. Rolls the blade,
+---------------------+                              tests coverage/slope/type/fade,
                                |                    appends to the pool and to each
                                |                    view's visible-index partition
                                v
+---------------------+  "Grass Indirect Fixup" (CS) clamps the instance counts a
| IndirectFixup       |                              saturated frame overshot
+---------------------+
                                |
                                v
+---------------------+  "Grass GBuffer"        2 x DrawIndexedIndirect (HI slot 0,
| G-buffer draws      |                              LO slot 1) -> 4 core MRTs +
+---------------------+                              scene depth (5 MRTs under the
                                                     velocity latch)

    ... and, from inside "Shadow Cascade 0" / "Shadow Cascade 1" (which are
    ordered after IndirectFixup by their declared reads), 1 x
    DrawIndexedIndirect each of the LO partition in slot 2 / slot 3.
```

The blade draw is **indexed with no vertex buffer**: `SV_VertexID` delivers the
fetched index value (the logical vertex id 0-14), and `SV_StartInstanceLocation` —
seeded by the reset CS — carries the partition base, so the vertex stage recovers
its slice without a per-draw constant. Both draws are recorded unconditionally; an
empty frame draws `instanceCount 0`, which keeps the command stream identical
whether or not there is grass.

Blades are **opaque G-buffer geometry with the `GBUFFER_SHADING_SUBSURFACE` tag**,
not a forward blended overlay. They test *and write* depth, and render
`CULL_MODE_NONE` (a blade is a two-sided sliver; the fragment stage flips the
lighting normal on back faces rather than dropping them). The old forward pass's
self-translucency survives as a sun-driven back-scatter term written into the
**emissive** channel — back-lit plus through-scatter, weighted by the type's
base/tip translucency along the blade.

### Pass placement

The four passes are ordered by `DependsOn` edges plus the declared buffer traffic
(pool / visible list / cursor as UAV, indirect args as `READ_INDIRECT_ARG` at the
draw). `"Grass GBuffer"` declares `Writes` on all four core MRTs + `WRITE_DSV` on
scene depth, so the topological sort places the whole chain inside the G-buffer
block — after Terrain and UnifiedMesh, before Decals / HiZ / SSAO / SSR / SSGI /
DeferredShading.

The **velocity MRT changes the pass's attachment COUNT, never whether the pass
exists**: the pass set has to be invariant under the TAA toggle, and the
record-time pipeline pick reads the same frozen latch the setup does.

The dynamic (frame-indexed) buffers are deliberately **never declared** to the
graph — `GetBuffer()` returns a different physical buffer per frame in flight, so
a pointer captured at setup would bind the wrong frame's buffer forever after.

`"Grass Placement"` is **always enabled** and early-outs internally: the graph
skips the `Prepare` of a disabled pass, so gating the pass on "is there grass this
frame" would also gate the CPU work that decides it.

The **shadow cascades 0-1 are consumers of the same three persistent buffers**:
`Flux_ShadowsImpl::SetupRenderGraph` declares a `ReadBuffer` on the pool + the
visible-index list (`READ_BUFFER_SRV`) and on the indirect args
(`READ_INDIRECT_ARG`) for those two passes only, which is what orders them after
IndirectFixup and synthesises the `WRITE_UAV -> READ` barriers. Grass exposes those
three wrappers through `GetBladePoolBuffer` / `GetVisibleIndexBuffer` /
`GetIndirectArgsBuffer` for exactly that declaration and nothing else.

## Files

| File | Purpose |
|------|---------|
| `Flux_GrassImpl.h` | `Flux_GrassImpl` declaration + the GPU-mirror constant blocks (placement/draw constants, wind block, tile record) and the pool/partition capacities |
| `Flux_Grass.cpp` | Lifecycle, pipelines, GPU + map-texture resources, map build/quantize, CPU queries, stats, debug variables |
| `Flux_Grass_Frame.cpp` | `SetupRenderGraph`, the per-frame gather, and the four record bodies + the cascade caster |
| `Flux_GrassTypes.h` | **The pure half** — blade record, per-type block, lattice/tile/fade constants, tile selection, clump Voronoi, blade pose. No singleton, no device, no IO |
| `Flux_GrassTypeTable.h/.cpp` | The unpacked AUTHORING record + the one-way `ToGPU()` projection and its serialization |
| `Flux_Grass_Shaders.h` | The seven `Flux_ShaderDecl`s owned by the feature + `apxALL[]` |
| `Flux_Grass.Tests.inl` | Units for the pure definitions + the scene-state lifecycle, hosted at the bottom of `Flux_Grass.cpp` |

## Shaders

All in `Shaders/Vegetation/`.

| Shader | Stage | Purpose |
|--------|-------|---------|
| `Flux_GrassCommon.slang` | module | **Authoritative** blade record, index/vertex tables, partition bases + caps, the shared pose builder, the debug-colour set, and THE shared G-buffer write |
| `Flux_Grass_Reset.slang` | cs | Zeroes the indirect block + pool cursor, seeds per-partition `firstInstance` |
| `Flux_Grass_Placement.slang` | cs | Per-lattice-node placement, clump Voronoi, per-view frustum cull, partition append |
| `Flux_Grass_IndirectFixup.slang` | cs | Clamps instance counts against the partition caps |
| `Flux_Grass_Displacement.slang` | cs | Mover push field — **built, not dispatched** (see Seams) |
| `Flux_Grass_ToGBuffer.slang` | vs+fs | The 4-MRT blade draw |
| `Flux_Grass_ToGBufferVelocity.slang` | vs+fs | 5-MRT variant. Blades sway every frame, so its vertex stage rebuilds the pose against the PREVIOUS frame's wind rather than reprojecting a static position |
| `Flux_Grass_ToShadowmap.slang` | vs+fs | Depth-only caster, recorded inside CSM cascades 0-1 (see Shadow casting) |

## The pinned contracts

`Flux_GrassTypes.h` and `Flux_GrassCommon.slang` are **twin authorities**: the
Slang side is authoritative for the tables and the pose, the C++ side is a
transcription that `static_assert`s the sizes. Diff them against each other, never
re-derive either.

| Contract | Value | Where |
|----------|-------|-------|
| Blade record | 64 B (16 x 32-bit slots) | `Flux_GrassBladeInstance` |
| Per-type block | 144 B (36 scalars) — **pack, never grow** | `Flux_GrassTypeParamsGPU` |
| Blade pool | 1 048 576 blades | `uFLUX_GRASS_BLADE_POOL_CAPACITY` |
| Visible index list | 1 572 864 uints, four fixed partitions | `uFLUX_GRASS_VISIBLE_INDEX_CAPACITY` |
| Partition bases / caps | `{0, 524288, 1048576, 1310720}` / `{524288, 524288, 262144, 262144}` | `kauGRASS_PARTITION_BASE/CAP` (Slang) |
| Indirect slots | 16 (4 live: camera HI/LO + cascade 0/1), 20-byte stride | `uFLUX_GRASS_INDIRECT_*` |
| Lattice step | HI 0.25 m, LO 0.50 m (stride 2) | `Flux_GrassConfig` |
| Tile size / cells | HI 16 m, LO 32 m, both 64 x 64 cells | `Flux_GrassConfig` |
| HI radius | 64 m | `fHI_RADIUS` |
| Tile cap | 256 per frame, nearest kept | `uMAX_TILES` |
| Max distance | default 250 m, clamped to [50, 400] | `fDEFAULT/MIN/MAX_MAX_DISTANCE` |
| Grass types | 16 | `uFLUX_GRASS_MAX_TYPES` |
| Blade mesh | 15 logical vertices, 48 indices (HI 0-32, LO 33-47) | `auBLADE_INDEX_TABLE` |

> **The partition bases and caps are the dangerous half.** The placement CS writes
> a survivor at `base[slot] + localIndex`, so a capacity that disagrees does not
> overflow the buffer — it writes one partition's blades into the NEXT partition's
> range, and the cascade draws the camera's blades.

Persistent VRAM is **constant after `Initialise`** — ~70 MB of buffers (the pool
alone is 64 MB) plus 34 MB of map textures (height 4096² R16, coverage and type
1024² R8). Nothing grows with scene content or with any toggle;
`GetBufferUsageMB()` reports the whole footprint.

## LOD

Two lattices, not four density tiers. A LO tile's nodes are exactly the
`(even, even)` subset of the HI lattice, so the HI→LO transition is a **fade**
rather than a reshuffle: each node's *lattice class* (bits 10-11 of the blade
record's `typeFlags`) selects a staggered fade band, class 0 never fades (it is
the set the LO tiles reproduce), and classes 1/2/3 shrink out on translated ramps
so no two ever pop together. A LO tile is dropped only when all four of the HI
tiles it covers are already being drawn.

## Public API (`g_xEngine.Grass()`)

**Lifecycle** — `Initialise` / `BuildPipelines` / `SetupRenderGraph` / `Shutdown`
are registry-driven. `Reset()` is the scene-lifecycle hook (idempotent, safe
before `Initialise`, double-fires at boot). `ClearSceneData()` drops maps, tiles,
movers and stats; the **type table, the wind state and every byte of VRAM
survive it** — VRAM is released only by `Shutdown`.

**Feed**
```cpp
// From a terrain texture directory: Height + GrassDensity are REQUIRED,
// GrassType is optional (absent => every texel type 0). A malformed required
// map is a hard failure that leaves the prior state completely untouched.
g_xEngine.Grass().BuildFromTerrainTextures(strTexDir, { .m_fDensityScale = 1.0f });

// The same build from raw CPU pointers (editor live maps + headless tests).
Flux_GrassImpl::MapSet xMaps{ ... };   // data is COPIED, quantized to the GPU formats
g_xEngine.Grass().BuildFromMaps(xMaps, xParams);
```

**Types** — `SetTypeTable(const Flux_GrassTypeTable&)` copies + validates and
re-uploads through the next gather; `GetTypeTable()` reads it back.

**Tuning** — `SetDensityScale` / `SetMaxDistance` / `SetWindDirection(yawRad)` /
`SetWindStrength` / `SetDebugMode`, each with a getter. The debug variables bind
these members **by reference**, so nothing is re-stamped per frame and a value
written from game code survives.

**CPU queries** — `SampleGrassCoverage` (bilinear), `SampleGrassType`
(nearest-texel, never interpolated), `SampleGrassHeight` (bilinear, metres). They
read the *same quantized bytes* the GPU textures hold, so the query surface and
the placement CS cannot drift. **All three return 0 when unbuilt** — a caller with
no data has to decide what that means.

**Stats** — `IsBuilt` / `HasCoverageMap` / `GetCoverageMapSize` /
`GetCoverageWorldSize` / `GetScheduledInstanceCount` / `GetVisibleTileCount` /
`GetTileCount` / `GetSubmittedDrawCount` / `GetBufferUsageMB`.

**Readback** — `ReadbackVisibleBladeCount()` is an **explicit slow path**: it
drains staged writes, idles the device, downloads the 320-byte indirect block and
sums the 16 instance counts. Never call it from a frame path. **Headless it is 0
by construction** (the GPU-less download zero-fills), so it is *windowed-only
truth* and must never be asserted on in a `Null_` test.

> **`GetScheduledInstanceCount()` determinism is a CONTRACT, not an implementation
> detail.** It counts lattice cells over the tiles scheduled *this frame* — the
> dispatched work, not a blade count. For a fixed camera and fixed maps it is
> exactly reproducible, because the tile scheduler is a pure function with a total
> order over its output (nearest-first, tie-broken on LOD then coordinates).
> Zenithmon suites assert an EXACT restore of this number across battle
> transitions, so anything that makes tile selection frame-order- or
> float-accumulation-dependent breaks them.

## Authoring flow

1. **Paint the maps** in the terrain editor: the `GrassDensity` tool paints
   coverage `[0,1]`, the `GrassType` tool stamps the per-texel type index
   (0 = default, 255 = no grass). Both are 1024² maps over the terrain footprint.
2. **Bake** them out beside the terrain's other textures as `GrassDensity.ztxtr`
   (R32_SFLOAT) and `GrassType.ztxtr` (R8_UNORM, POINT-sampled). `Height.ztxtr`
   (R32_SFLOAT, **normalized** — scaled to metres on load) is the third input.
   Height and GrassDensity are required; GrassType is optional.
3. **Feed** the directory to `BuildFromTerrainTextures`. The world footprint is
   taken from `Flux_TerrainConfig::TERRAIN_SIZE`, not from the files. Coverage and
   type are quantized to R8 and height to R16 unorm over `[bias, bias + scale]`
   metres, so the fixed-point range tracks the terrain actually loaded.
4. **Type parameters** come from `game:Vegetation/GrassTypes.zdata`
   (`Zenith_GrassTypeTableAsset`) if the game ships one, loaded once at
   `Initialise` before the first gather. **No game ships one today: absence is the
   normal path**, and the four seeded built-ins (Meadow / Tall / Dry / Flowers)
   stand in. A present-but-unreadable file warns and keeps the built-ins.

The authored record (`Flux_GrassTypeParams`) is plain and unpacked, one field per
parameter; `ToGPU()` is the **one-way** projection onto the packed 144-byte block.
Nothing reads back the other way — the packing is a GPU detail and must never
reach the file.

## Debug variables (`Flux/Grass/...`, tools builds)

| Path | Type | Range | Effect |
|------|------|-------|--------|
| `DebugMode` | uint | 0-7 | Fragment debug view (below) |
| `DensityScale` | float | 0-4 | Multiplies the coverage map into the placement probability |
| `MaxDistance` | float | 50-400 | Furthest tile ring considered |
| `WindStrength` | float | 0-10 | Wind gain |
| `WindYawDeg` | float | -180-180 | **The** wind heading; the direction vector is derived from it each frame, so the slider and `SetWindDirection` can never disagree |
| `FreezeCulling` | bool | | Holds the tile schedule so you can fly out and inspect it |
| `ShowTileGrid` | bool | | **Storage only** — the outlines belong on the gameplay-safe primitives channel, which is not wired yet |
| `DisableShadowCasting` | bool | | Third input to `IsShadowCastingEnabled()`. Live A/B: drops the cascade partitions out of the active-slot mask, so it removes the placement work as well as the two cascade draws |
| `ForceLoBlades` | bool | | **Storage only** |
| `DebugOrbitDisplacer` | bool | | **Storage only** — consumed when the displacement pass lands |

Enable / Wind / Displacement / Shadows are `Zenith_GraphicsOptions` flags
(`m_bGrassEnabled`, `m_bGrassWindEnabled`, `m_bGrassDisplacementEnabled`,
`m_bGrassShadowsEnabled`) surfaced under `Graphics/Grass/...`, not feature debug
variables. The last two are **set once at boot** from
`Project_SetGraphicsOptions`; flipping them mid-run is not a supported path.

`m_bGrassEnabled` is read **once per frame into a latch** that the four record
callbacks and the caster all read. The gather is what decides there is nothing to
place, so a toggle landing between the two would leave a draw reading indirect
args whose reset it had already skipped.

### Debug modes

`kGRASS_DEBUG_*` in `Flux_GrassCommon.slang` is the authority. The value rides the
draw CB verbatim; `Flux_GrassDebugColour` tests 1-5 explicitly and falls through to
the world-normal view, so **6 and everything above it (the slider's 7 included)
read as normals**.

| Value | View |
|-------|------|
| 0 | None — normal shading |
| 1 | Type index (hashed colour per type) |
| 2 | Clump (R = clump hash, G = normalized distance to the clump centre) |
| 3 | Height t (greyscale along the blade) |
| 4 | LOD mesh (orange = LO strip, green = HI) |
| 5 | Lattice class (white / red / green / blue for 0-3) |
| 6+ | World normal |

A non-zero mode short-circuits the whole surface: the debug colour goes out as
albedo through the simple `MakeGBuffer` overload (ambient 1, roughness 1, metallic
0, no emissive, DEFAULT_LIT), so the value is still lit by the deferred pass but
carries none of the blade's real material.

## Wind

Wind is a **global** three-`float4` block (`Flux_GrassWindBlockGPU`, 48 B): heading
+ strength + time, then frequency / scroll / gust / seed, then the detail
frequency / speed / tip amplitude. It reaches both the placement CS (which bakes a
per-blade `windStrength` into the record) and the vertex stage (which deflects the
Bezier's P2 and P3, weighting the deflection by height² so the base stays still
while the tip travels).

The **previous** frame's block is captured before each advance and is the only
previous-frame state the velocity vertex stage reads. On the first frame it equals
the current block, which reports zero motion rather than a jump.

## Headless (`Null_` builds)

Every CPU consequence of a build still happens — maps quantized, tile schedule
selected, stats updated, type table loaded and validated — because the CPU maps
*are* the query surface. Only two things are skipped explicitly:
`CreateMapTextures` / `UploadMapTextures` (34 MB of zero staging against a no-op
backend) and, by construction, `ReadbackVisibleBladeCount`, which returns 0. The
four passes are still declared and their callbacks still run against the no-op
recorder, so a headless run exercises the same code a windowed one does.

## Shadow casting (live, cascades 0-1)

Grass casts into the first two CSM cascades. `Flux_ShadowsImpl::ExecuteShadowCascade`
calls `RenderToShadowMap(cmdBuf, cascade)`, which draws that cascade's LO partition
from indirect slot `2 + cascade`; cascades 2-3 own no partition and the impl
early-outs above index 1.

Blades are **generated once and culled per view**. The gather freezes the camera's
and both cascades' `m_xViewProjMatNoJitter` (from the render-view registry — the
same payload the unified cull extracts *its* planes from), extracts six inward
planes per view into the placement constants, and the placement CS appends each
survivor into every partition that wants it. There is no second placement pass and
no per-light pose: the caster binds the **same `GrassDrawConstants`** the lit draws
do, so wind, LOD convergence and class fade all key off the MAIN camera and a
swaying blade casts the shadow it is standing in.

Two consequences worth stating out loud:

- **Tile scheduling is a UNION over camera ∪ cascades**, not an intersection — a
  tile behind the camera still has to fill the cascades it casts into.
  `GetScheduledInstanceCount()` therefore rises when casting is on. It stays
  deterministic (the cascade matrices are camera-derived), which is what keeps the
  Zenithmon exact-restore suites green.
- **Disabling casting stops GENERATION, not just the draw.**
  `m_uCascadeFrustaCount` goes to zero, the cascade slots leave the active-slot
  mask, and the placement CS never appends a shadow blade. Both the
  `m_bGrassShadowsEnabled` option and the `Flux/Grass/DisableShadowCasting` debug
  variable land there, via `IsShadowCastingEnabled()`.

A slot is never activated without a REAL cascade frustum behind it: culling a
cascade partition against a duplicated *camera* frustum would fill it with the
wrong blades — worse than an empty cascade, and harder to spot.

**`Grass` is registered BEFORE `Shadows`** in `RegisterDefaultFeatures` for the
same reason `UnifiedMesh` is: cascades 0-1 declare `ReadBuffer`s on the blade pool,
the visible-index buffer and the indirect args, and a reader only links to an
EARLIER-declared writer. Pinned by `FluxGrassImpl::GrassIsDeclaredBeforeShadows`.

### Every grass draw must call `UseBindlessTextures(2)` — the depth-only one too

Set 2 (BINDLESS) is in the layout of **every** spine pipeline, used or not.
`Common/Bindings.slang` declares the block, Slang does not dead-strip a declared
`ParameterBlock` (that is exactly what keeps the PASS block at space 3), so
`Flux_Grass_ToShadowmap`'s reflection lists `g_axTextures` even though its `fsMain`
is literally `void fsMain() {}` — its `.refl` is byte-identical to
`Flux_Grass_ToGBuffer`'s. `BindPersistentSpineSets` auto-binds only GLOBAL (0) and
VIEW (1); **BINDLESS stays on the explicit `UseBindlessTextures` path**, so Vulkan
raises *"uses set 2 but that set is not bound"* on the first draw of a pipeline
nobody bound it for.

Do not try to fix this in the shader. There is no way to drop set 2 from a spine
module's layout short of not including the spine, which would move the PASS block to
space 0.

The trap is that a missing bind is usually *invisible*: a Vulkan set binding survives
a pipeline switch when the layouts are prefix-compatible, so a grass draw inherits
whatever Terrain (`Flux_Terrain.cpp`) or UnifiedMesh bound earlier in the same worker
command buffer. `RecordGBuffer` shipped without the call for exactly that reason. The
cascade path is where it finally bit: `Flux_UnifiedMeshImpl::RenderToShadowMap`
early-outs on zero buckets **before** its own `UseBindlessTextures(2)`, so on a
grass-only scene the grass caster is the first user in that command buffer and there
is nothing to inherit.

## Seams that are not live yet

- **Displacement.** `SubmitMover` accepts up to 64 immediate-mode movers per frame
  and the displacement pipeline is built (so the VRAM footprint the TAA toggle
  stress test pins stays constant), but it is never dispatched and the push scale
  is 0. The scale is the one slot `m_bGrassDisplacementEnabled` is applied to, so
  the option is honoured by the VALUE and never by a branch in the CS.

## Accepted look changes

Grass is now a G-buffer writer at blade depth, which means it **receives SSAO,
SSGI, SSR, decals and volumetric fog** like any other opaque geometry — it did
not before, when it was a forward pass over the already-lit HDR scene.

Grass is also noticeably **brighter than in any capture taken before the
physically-grounded lighting work**. The old forward term was roughly 7x
underlit relative to the deferred pipeline; going through `DeferredShading`
removed that discrepancy. The brightness jump is EXPECTED and correct — do not
re-tune the type colours to match an old screenshot.
