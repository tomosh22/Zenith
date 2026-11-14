# ADVGFX design: path-traced reference

Section file of `Zenith2/Docs/Design/ADVGFX.md`. It specifies the Stage 5 path-traced reference: the deterministic compute path tracer of the tool-side library `ZenithPathTracer`, its scenes, BVH, wavefront pipeline, materials, lights, integrators, random numbers, convergence and determinism; the error metrics and the comparison against the raster paths; the reference files, their fingerprint, the capture procedure and the landing protocol for a stale reference; and the execution substrate the tracer's graphs run on. A Stage 7 hardware traversal is not part of this design: it is flagged proposal ADVGFX-1 ("Compute and hardware traversal"). The specification is `Zenith2/Docs/Rendering.md → "Validation roles"` (physical accuracy, [Z2-N-REND-944] to [Z2-N-REND-948]) and Z2-D-037. The comparison scenes and their tolerances are LIGHT's (`Zenith2/Docs/Design/LIGHT/Tests.md → "Path-tracer comparison scenes"`, → "Approximation error scenes") and ENV's TerrainVT scene (`Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"`); this file owns the tracer, the runner, the metrics and the references.

## Scope of this file

| Topic | Heading |
|---|---|
| what the tracer is for | "Role and boundaries" |
| where it lives | "Library targets and programs" |
| its input | "Reference scenes", "Scene builder" |
| ray tracing | "BVH", "Traversal", "Wavefront pipeline", "Execution substrate" |
| shading | "Materials and gradients", "BSDF evaluation and sampling", "Lights and the environment", "Integrators" |
| numbers | "Random numbers", "Accumulation and convergence", "Determinism" |
| comparing | "Raster capture", "Comparison tonemap", "Error metrics" |
| references | "Reference files", "Reference manifest and fingerprint", "Reference capture procedure" |
| running | "Gate and acceptance runs", "Reports and artefacts", "Proposed per-scene caps" |
| the hardware-traversal proposal | "Compute and hardware traversal" |
| failures | "Path tracer failure handling" |

## Role and boundaries

| Boundary | Rule | Source |
|---|---|---|
| purpose | physical-accuracy validation of the raster paths: transport approximations (shadow maps, the split-sum IBL, LTC and representative-point area lights, screen-space techniques) against a converged Monte Carlo reference on controlled scenes | Z2-D-037, Z2-D-063 |
| shared code | the tracer evaluates materials with the same `EvaluateMaterial` and the same BRDF terms (`Zenith2/Shaders/Lighting/BRDF.slang`) as the raster paths, so it measures transport, not material code; the independent CPU reference BRDF covers the BRDF itself ([Z2-N-REND-949]) | Z2-D-063 |
| device | compute shaders only, on WARP and lavapipe ([Z2-N-REND-946]), and on any desktop device for development runs | Z2-D-037 |
| link closure | tool-side; never in a Player, game or runtime target, and outside the Shipping link closure ([Z2-N-REND-948], [Z2-N-CONTRACT-179]) | Z2-D-037 |
| labels | WARP and lavapipe runs are not `needs-gpu` (ruling X170); recording references is `needs-human` (ruling GOLDEN-FILES) | — |

## Library targets and programs

| Item | Path | Rule |
|---|---|---|
| library | `Zenith2/Tools/ZenithPathTracer/` | `zenith_add_library(ZenithPathTracer DEPENDS ZenithCore ZenithRHI ZenithVertexFormat ZenithCompression ZenithFlux ZenithShadersGenerated SUPPORTED win-debug win-release win-tools win-asan)`: every direct dependency and no other ([Z2-N-ARCH-167]); ZenithVertexFormat for the scene builder's mesh decode, ZenithCompression for the `.zref` planes. The tracer uses no job system (its BVH builder is single-threaded by design) and no asset-system type (the fingerprint's content hashes are ZenithCore's `Zenith_Hash128`). Architecture's row lists ZenithCore, ZenithRHI, ZenithFlux and ZenithShadersGenerated; cross-doc item X421 (c) adds ZenithVertexFormat and ZenithCompression (`Zenith2/Docs/Architecture.md → "Library dependency graph"`) |
| public headers | `Flux_PathTracer.h`, `Flux_PathTraceReference.h` | `Zenith2/Docs/Design/ADVGFX/PublicAPI.md`; they include public headers of ZenithCore, ZenithRHI, ZenithCompression and ZenithFlux, each header for every type it names, and no third-party header |
| internal files | `Internal/Flux_PTBVHBuilder.cpp/.h`, `Internal/Flux_PTSceneBuilder.cpp`, `Internal/Flux_PTScene.cpp`, `Internal/Flux_PTTracer.cpp`, `Internal/Flux_PTPasses.cpp/.h`, `Internal/Flux_PTAccumulator.cpp/.h`, `Internal/Flux_PTReference.cpp` | private to the library and its tests |
| scene-document helper | `Zenith2/Engine/ZenithFlux/Tests/Support/PathTrace/Flux_PTSceneDocument.h` | header-only (`inline` functions, [Z2-N-ARCH-255]): parses a reference scene document (instances, lights, camera and the `pathTrace` object) through ZenithDocument and feeds `Flux_PathTraceSceneBuilder`; included by the runner (`ZenithFlux_Tests`) and by `Zenith_Bench_PathTraceReference`, the two callers, each of which declares the ZenithDocument edge itself (`TEST_DEPENDS ZenithDocument` for `ZenithFlux_Tests`, `DEPENDS ZenithPathTracer ZenithDocument` on the benchmark) ("Scene-document helper") |
| test support | `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/Runner/Flux_PhysicalAccuracyRunner.h`, `Flux_PTMetrics.cpp`, `Flux_PTTonemap.cpp`, `Flux_PTRunner.cpp` (a directory named `Runner/`, so it is never read as the reserved header-only `Tests/Support/`) | compiled into `ZenithFlux_Tests` alone, the one target the `flip` header's manifest row links (`TEST_THIRD_PARTY flip`, `Zenith2/Docs/Design/SHIP/Manifest.md`); so no non-test library includes `flip` and the manifest row is unchanged |
| tests | `Zenith2/Tools/ZenithPathTracer/Tests/` (`ZenithPathTracer_Tests`: the tracer and reference-format tests); `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/` (`ZenithFlux_Tests` with `TEST_DEPENDS ZenithPathTracer`: the metric, runner and comparison tests) | `Zenith2/Docs/Design/ADVGFX/Tests.md` |
| programs | `Zenith2/Shaders/Tests/PathTracer/PTRayGen.slang`, `PTIntersect.slang`, `PTClassify.slang`, `PTMaterialResolve.slang`, `PTShade.slang`, `PTShadowIntersect.slang`, `PTAccumulate.slang`, `PTCommon.slang` | test-only programs, never packaged (`Zenith2/Docs/Rendering/ShaderSystem.md → "Shader source layout"`), compiled offline by `ZenithShaderCompiler` through `ZenithShadersGenerated`'s `SHADERS` option like every other program ([Z2-N-REND-101]) |
| material axis | `PTMaterialResolve` declares the material-type link-time axis of `Flux_ActiveMaterial` ([Z2-N-REND-243]), so its permutations are enumerated offline for every material variant of the cooked content like the visibility buffer's material resolve; a variant with no compiled permutation is `FLUX_PT_ERROR_MATERIAL_UNSUPPORTED` at scene build | — |
| PSOs | `Flux_PathTracer::Run` creates every compute PSO the scene needs before the first iteration, through the renderer's PSO table; no shader is compiled at run time ([Z2-N-REND-102]) | — |

## Reference scenes

A reference scene is one of RPROD's parity scene documents (`Zenith2/Docs/Design/RPROD/Tests.md → "Parity scenes"`: instances, lights, camera path, cooked at build) that carries a `pathTrace` object. Two consumers read it: the runner builds both the raster view and the tracer scene from the same document, so the two renderers see identical content, and `Zenith_Bench_PathTraceReference` builds the tracer scene to record its references. Both read the tracer half through the scene-document helper ("Scene-document helper").

<!-- assumption: pending RPROD -->
The scene document gains the optional `pathTrace` object below; RPROD owns the parity scene schema and adds the key to it (row AV1 (g)).
<!-- /assumption -->

```json
"pathTrace": {
	"integrator": "radiance",
	"maxBounces": 1,
	"resolution": [256, 256],
	"minSamplesPerPixel": 256,
	"relativeStandardError": 0.01,
	"convergedPixelFraction": 0.995,
	"wallClockCapSeconds": 900,
	"ambientRadiusMetres": 1.0,
	"lightIndex": 0,
	"exposure": 1.0,
	"tolerance": { "flipMean": 0.02, "relativeLuminance": 0.02, "meanAbsolute": 0.0, "misclassified": 0.0 },
	"mask": { "kind": "all" },
	"rasterFeaturesOff": ["ssao", "ssr", "contactShadows", "ssgi", "fog"],
	"paths": ["forwardPlus", "forward", "deferred", "visibilityBuffer"]
}
```

| Key | Type | Rule |
|---|---|---|
| `integrator` | `radiance` · `ambientVisibility` · `lightVisibility` · `specularReflection` | `Flux_PathTraceIntegrator` |
| `maxBounces` | integer 1 … 32 | 1 compares direct lighting and the environment alone (the raster paths have no interreflection beyond their GI features, which reference scenes disable) |
| `resolution` | two integers 16 … 1,024 | at most `Z2-B-ADVGFX-004` for the scene |
| `minSamplesPerPixel` | integer, multiple of 64 | at least `Z2-B-ADVGFX-006` |
| `relativeStandardError`, `convergedPixelFraction` | numbers | the scene's values of `Z2-B-ADVGFX-007` |
| `wallClockCapSeconds` | one number for both backends | `Z2-B-ADVGFX-005` (one value per scene: the cap of the slower adapter, lavapipe) |
| `ambientRadiusMetres`, `lightIndex` | for the visibility integrators | the GTAO radius and the light of the compared technique |
| `exposure` | number | the scene's fixed exposure ([Z2-N-REND-942]) |
| `tolerance` | numbers | the scene's values of `Z2-B-ADVGFX-008`: FLIP mean and relative luminance error for RGB scenes, one metric for the visibility scenes (LIGHT's and ENV's proposals while pending; the row's two values per RGB scene are noted in cross-doc item X422) |
| `mask` | `{ "kind": "all" }`, `{ "kind": "rect", "rects": [[x, y, w, h], …] }`, `{ "kind": "penumbra", "light": n }` (pixels whose reference light visibility lies in (0.02, 0.98)), `{ "kind": "nonSky" }`, `{ "kind": "onScreenReflection" }` | the comparison mask |
| `rasterFeaturesOff` | strings | view features cleared in the raster run ([Z2-N-REND-944] "with every screen-space approximation disabled") |
| `paths` | shading paths | the raster paths compared; on the S24 Forward+ alone, never run there (the tracer is Windows-only) |

The scenes: LIGHT's `PT_AnalyticPoint`, `PT_AnalyticSpotDirectional`, `PT_SphereLight`, `PT_RectLight`, `PT_DiscLight`, `PT_IBLFurnace`, `PT_IBLEnvironment`, `PT_AreaLightShadow`, `AE_GTAO`, `AE_ContactShadows`, `AE_SSR`, `AE_SSGI`; ENV's `TerrainVT` (ruling AV-R6; LIGHT's `PT_TerrainVT` row is asked to name the same scene by row AV9 (a), pending AV9 (a)); and the tracer's own self-check scenes of "Gate and acceptance runs".

### Scene-document helper

`Flux_PTSceneDocument.h` (header-only, `Zenith2/Engine/ZenithFlux/Tests/Support/PathTrace/`) declares `inline` functions only:

| Function | Result |
|---|---|
| `Flux_ParsePTSceneDocument(Zenith_ByteSpan xDocument, Zenith_Arena& xArena)` | `Zenith_Result<Flux_PTSceneDocument, Flux_PathTraceError>`: the instances (mesh GUID, LOD, local-to-world matrix, section material GUIDs, custom data), the lights, the environment, the camera at the capture frame, and the `pathTrace` fields (settings, exposure, tolerance, mask, `rasterFeaturesOff`, `paths`); a document that fails ZenithDocument's parse or a key rule of this section returns `FLUX_PT_ERROR_SCENE_INVALID` naming the key |
| `Flux_BuildPTSceneFromDocument(const Flux_PTSceneDocument&, const Flux_PTDocumentAssets&, Flux_PathTraceSceneBuilder&)` | adds every instance, light, environment and the camera to the builder, with the caller's resolved meshes, materials and environment map (`Flux_PTDocumentAssets` is the caller's GUID → resource table; the caller owns asset loading) |
| `Flux_MakePTSettings(const Flux_PTSceneDocument&, Zenith_RHIBackend)` | the `Flux_PathTraceSettings` of the document for the backend |

The instance, light and camera keys are RPROD's parity scene keys; the helper reads the same keys RPROD's parity loader reads, and `Test_PTSceneDocument_MatchesParityLoader` (`ZenithFlux_Tests`) requires both loaders to return identical instance, light and camera lists for every reference scene.

## Scene builder

`Flux_PathTraceSceneBuilder` (`Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_PathTracer header"`):

| Input | Conversion |
|---|---|
| a mesh instance | each section of the requested LOD is decoded with ZenithVertexFormat's CPU codec (`Zenith2/Docs/Rendering/VertexData.md → "Default stream layout"`): positions dequantised with the primitive's scale and bias, the QTangent decoded to tangent, bitangent sign and normal, UV0 and UV1 dequantised; transformed to world space with the instance matrix (normals and tangents by the inverse transpose, renormalised); 16-bit indices widened. The codec's bit-exactness with the GPU fetch route is GEOM's (`Test_VertexFormat_FetchUnitParity`) |
| a skinned or procedural instance | refused with `FLUX_PT_ERROR_SCENE_INVALID`: reference scenes are static |
| terrain | ENV's `Zenith_ExtractTerrainCollision` LOD-0 tiles (`Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"`) through `AddTriangles`, with world-XZ UVs and the terrain material (rows EN19, AV8) |
| a light | `Flux_RenderLightDelta` in LIGHT's units and encodings (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"`) |
| the environment | an environment map asset's radiance cube mip 0, scaled by the intensity, or a uniform radiance (furnace scenes) |
| the camera | the parity camera's unjittered view and projection at capture frame f |

`Build` builds the BVH, sorts materials into variants (by `Zenith_RHIPermutationKey::m_ulMaterialVariantID`, ascending), builds the environment alias table and validates every input; an invalid input names the instance or light index. Material slots are acquired at the first `Run` ("Execution substrate"): each distinct `Flux_MaterialResource` of the scene gets one material-slot record from the renderer's GPU scene, every triangle carries that slot in `Flux_PTTriangle::m_uMaterialSlot`, and a `slot → variant index` table (one `uint32` per acquired slot) is uploaded with the scene so `PTClassify` bins hits by variant.

## BVH

### Builder

| Element | Rule |
|---|---|
| kind | one binary BVH over world-space triangles (reference scenes are static, at most 16,777,216 triangles) |
| split | binned SAH, 32 bins along the axis of largest centroid extent; cost `C_trav = 1`, `C_isect = 1`; a node becomes a leaf when it holds at most 4 triangles or no split beats the leaf cost |
| determinism | triangle references are sorted by (bin, original index) within each partition, ties broken by triangle index; the builder is single-threaded per subtree and subtrees are built in a fixed order, so the BVH bytes depend only on the input |
| depth | at most 64; a deeper tree is `FLUX_PT_ERROR_BVH_LIMIT` |
| memory | nodes `2 × triangles` at most (32 bytes each), triangles 48 bytes each, attributes 64 bytes per vertex |

### Node and triangle layout

```slang
// Zenith2/Shaders/Tests/PathTracer/PTCommon.slang ([FluxShared] in the tracer's generated header)
[FluxShared] struct Flux_PTNode            // 32 bytes
{
	float3 m_xMin; uint m_uLeftOrFirst;     // inner: index of the left child (right = left + 1); leaf: first triangle
	float3 m_xMax; uint m_uCount;           // 0: inner node; 1 … 4: leaf triangle count
};
[FluxShared] struct Flux_PTTriangle        // 48 bytes
{
	float3 m_xP0; uint m_uMaterialSlot;     // the GPU scene material-slot record EvaluateMaterial reads; its variant
	                                        // index comes from the scene's slot → variant table
	float3 m_xP1; uint m_uVertexBase;       // first of the triangle's three attribute vertices
	float3 m_xP2; uint m_uCustomData;
};
[FluxShared] struct Flux_PTVertex          // 64 bytes
{
	float3 m_xNormal; float m_fBitangentSign;
	float3 m_xTangent; float m_fPad;
	float2 m_xUV0; float2 m_xUV1;
	float4 m_xColour;
};
```

## Traversal

| Element | Rule |
|---|---|
| intersection | the watertight ray-triangle test of Woop, Benthin and Wald 2013 (shear and permute; no edge or vertex gap or double hit) |
| order | the child whose box entry distance along the ray is smaller is visited first; ties visit the left child first |
| stack | 64 entries in thread-local storage; the depth limit makes overflow impossible |
| closest hit | `PTIntersect`: the nearest t in `(1e-4 × scene scale, t_max)`; hit record (triangle, t, barycentric u, v) |
| any hit | `PTShadowIntersect`: returns at the first hit with t in `(ε, t_light)`; alpha-tested materials are evaluated for opacity in `PTClassify`'s alpha pre-pass, so shadow rays treat an alpha-tested triangle as transparent where `EvaluateOpacity` is below the cutoff (rows "Materials and gradients") |
| self-intersection | origins offset along the geometric normal by `1e-4 × max(1, |p|)` (Wächter and Binder 2019's scale-aware offset) |

## Wavefront pipeline

### Path state

```slang
[FluxShared] struct Flux_PTPath            // 64 bytes per path, one path per pixel per iteration
{
	float3 m_xOrigin; uint m_uPixel;        // pixel index y × width + x
	float3 m_xDirection; uint m_uBounceFlags;   // bits 0–7 bounce, bit 8 alive, bit 9 specular-only, bit 10 shadow pending
	float3 m_xThroughput; float m_fPrevBSDFPdf;  // for MIS when a path hits an emitter
	float m_fConeWidth; float m_fConeSpread; uint m_uSampleIndex; uint m_uRngDimension;
};
[FluxShared] struct Flux_PTHit { uint m_uTriangle; float m_fT; float m_fU; float m_fV; };   // 16 bytes; m_uTriangle 0xFFFFFFFF: miss
```

`Flux_SurfaceData` of a path's hit is written by `PTMaterialResolve` into a 64-byte packed record; the shadow-ray queue holds 48-byte records (origin, direction, t_max, the pixel, the contribution).

### Kernels in order

One iteration traces one sample per pixel. Every kernel is a compute pass of the `Flux_RenderGraph` the tracer builds for each iteration batch inside `Flux_Renderer::RunToolGraph` ("Execution substrate"), so barriers are synthesised as for every Flux pass ([Z2-N-REND-380]); indirect passes use `AddComputePassIndirect`.

| # | Kernel | Work | Dispatch |
|---|---|---|---|
| 1 | `PTRayGen` | the primary ray of pixel (x, y), sample s: pinhole camera, the sub-pixel position from dimensions 0 and 1 of the sample's random sequence (box filter), the primary ray differentials | one thread per pixel |
| 2 | `PTIntersect` | closest hit of every alive path | one thread per alive path (indirect, the alive count) |
| 3 | `PTClassify` | for misses: add the environment radiance with its MIS weight and kill the path; for hits: add emission (MIS-weighted against the previous BSDF sample); count hits per material variant and write the compacted per-variant path lists and indirect arguments (prefix sum over variants) | one thread per alive path |
| 4 | `PTMaterialResolve` × variants | per material variant present: interpolate attributes from the barycentrics, build `Flux_MaterialInput` (gradients of "Materials and gradients"), run `EvaluateMaterial(xIn, m_uMaterialSlot)` and write the packed `Flux_SurfaceData` | one indirect dispatch per variant with hits, in ascending variant order |
| 5 | `PTShade` | next-event estimation: choose one light ("Lights and the environment"), sample a point on it, write a shadow-ray record with the MIS-weighted contribution; sample the BSDF for the continuation; Russian roulette from bounce `m_uRussianRouletteBounce` with survival `min(0.95, max(throughput))`; update throughput and the path | one thread per alive path |
| 6 | `PTShadowIntersect` | any-hit for each shadow record; on no occluder add the contribution to the pixel's sample radiance | one thread per shadow record |
| 7 | loop 2 … 6 for bounces 1 … `m_uMaxBounces` | — | — |
| 8 | `PTAccumulate` | add the sample's radiance (RGB) and luminance square to the batch accumulators (`RGBA32F` buffer of Σ RGB and Σ Y; `R32F` buffer of Σ Y²) | one thread per pixel |

The per-pixel sample radiance of an iteration is a `float3` buffer each kernel adds to only for its own pixel (one path per pixel per iteration), so no atomics are needed and the sum for a pixel is formed in kernel order. Compaction in `PTClassify` uses atomics, so the order in which paths are processed inside a variant varies with the GPU, but every path's arithmetic depends only on its own state, so the results do not ("Determinism").

### Batches

`m_uBatchSamples` (64) iterations form a batch. After a batch, the accumulators are read back (`Flux_RenderGraph::AddReadback`, `width × height × 20` bytes; 1,310,720 for 256 × 256), added into CPU `double` accumulators in pixel order and cleared; the convergence check of "Accumulation and convergence" runs on the CPU over the doubles. Cancellation and the wall-clock cap are evaluated at batch boundaries.

## Execution substrate

RPROD and RHI define graph construction, compile, record, submit and retirement inside the render subgraph's nodes on the render-submit thread (`Zenith2/Docs/Design/RPROD/Renderer.md → "Render frame sequence"`); a tool's graph has no frame of its own there. RPROD's answer to row AV17 is `Zenith2/Docs/Design/RPROD/Renderer.md → "Tool graph entry"`: `Flux_Renderer::RunToolGraph`, `SettleToolFrames`, `AcquireToolMaterialSlot`, `ReleaseToolMaterialSlot` and `Flux_ToolGraphError`. ADVGFX cites it and does not re-specify it; the tracer uses it as follows.

| Element | How the tracer uses RPROD's entry |
|---|---|
| "in progress" | the tracer calls the entry only between render frames or in a test host with no frame loop, where no render frame is in progress (RPROD's "Tool frames in flight"); a call from inside a frame returns `FLUX_TOOL_GRAPH_ERROR_FRAME_IN_PROGRESS` |
| frame slot and index | each batch is one tool frame, which takes the next slot of the frame ring under RPROD's slot rule (it waits on that slot's last timeline values), consumes one frame index and presents nothing |
| outstanding work | `RunToolGraph` returns after its phase B has waited for the tool frame's own timeline values and its phase C has retired the slot and resolved the readbacks, so no part of the batch is outstanding when it returns; earlier render frames are ordered before it by the slot rule and the graph's cross-frame ordering |
| thread | `Flux_PathTracer` is `MAIN_THREAD`; RPROD runs phases A and C as root jobs continuing on `JOB_AFFINITY_RENDER_SUBMIT`, whose `Zenith_JobContext` serves VT's `BeginFrame` and the other `RENDER_SUBMIT_THREAD` members (RPROD's "Tool frame thread and job context"); the callback receives the graph and `Flux_FrameShared` only |
| callback | declares the batch's kernels and its accumulator readbacks; it returns an error only for a tracer defect, which the entry reports as `CALLBACK_FAILED` |
| errors | any `Flux_ToolGraphError` from `RunToolGraph` ends the run with `FLUX_PT_ERROR_TOOL_GRAPH`, logging the value, except `DEVICE_LOST`, which is `FLUX_PT_ERROR_DEVICE_LOST` |
| VT settlement | RPROD pumps it: with VT in `full` mode, the tracer calls `SettleToolFrames(uFLUX_MAX_TOOL_SETTLE_FRAMES)` (600) before its first batch and maps `FLUX_TOOL_GRAPH_ERROR_VT_NOT_SETTLED` to `FLUX_PT_ERROR_VT_NOT_SETTLED`; a tracer-only test with no raster frame settles its VTs this way |
| material slots | at its first `Run`, outside every frame, the tracer calls `AcquireToolMaterialSlot` once per distinct material (`SLOTS_EXHAUSTED` and `MATERIAL_NOT_RESIDENT` become `FLUX_PT_ERROR_SCENE_INVALID`, naming the material), so `EvaluateMaterial(xIn, uMaterialSlot)` reads the constants a raster draw reads; it releases them in its destructor. After a device loss RPROD has released every tool slot: the tracer is unusable (`Run` returns `FLUX_PT_ERROR_DEVICE_LOST`), its destructor's releases are stale and ignored, and it is destroyed |
| blocking | until RPROD's "Tool graph entry" is implemented, every Stage 5 tracer ticket that runs a graph is blocked by it |

`Test_PathTracer_BatchRunsOutsideFrameLoop`: with no render frame running, a `Run` of `PTSelf_WhiteFurnace` completes; each batch is one `RunToolGraph` call whose readbacks are `READY` when it returns; a call made from inside a render frame's build node returns `FRAME_IN_PROGRESS`; and the upload ring, the readback pool, the deferred-deletion queue and the GPU scene's material-slot table return to their idle state after the tracer is destroyed. `Test_PathTracer_VirtualTextureFullMode` covers the settlement pump with no raster frame at all.

## Materials and gradients

| Element | Rule |
|---|---|
| evaluation | `EvaluateMaterial` with the hit's material variant (link-time specialised in `PTMaterialResolve`), the same code every raster path runs ([Z2-N-REND-241]) |
| `Flux_MaterialInput` | position, view direction (toward the ray origin), interpolated normal, tangent, bitangent with sign, UV0, UV1, vertex colour, `m_bFrontFace` from the geometric normal against the ray, the custom data, `m_fTimeSeconds` 0 |
| primary gradients | ray differentials (Igehy 1999) of the primary ray, transferred to the hit plane and to UV through the triangle's barycentric derivatives: the same analytic construction the visibility buffer's material resolve uses ([Z2-N-REND-246]), so a primary hit samples the mip a raster pixel samples |
| secondary gradients | ray cones (Akenine-Möller et al. 2021): width grows by the spread angle per unit distance, the spread widens at each hit by the surface's curvature estimate; the UV gradient is the cone width projected onto the triangle's UV parameterisation, isotropic |
| virtual textures | the tracer's renderer runs the VT system in `full` residency mode (`Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Residency modes"`), and `PTMaterialResolve` fills `Flux_MaterialInput::m_xVirtual` with feedback off, so a VT-backed material samples through `Flux_SampleVirtual` exactly as the raster run of the same scene does |
| alpha-tested materials | the continuation and shadow rays treat a hit as absent when `EvaluateOpacity` is below the material's cutoff; `PTClassify` runs a per-variant opacity pre-pass for alpha-tested variants before compaction |
| emissive surfaces | `m_xEmissive` is added on hit with the BSDF-sample MIS weight 1 (emissive surfaces are not sampled by next-event estimation; reference scenes light with analytic lights) |
| unsupported in reference scenes | transparency, refraction, decals and subsurface transmission: the scene builder refuses a `transparent` or `decal` material with `FLUX_PT_ERROR_SCENE_INVALID` |

## BSDF evaluation and sampling

| Element | Rule |
|---|---|
| evaluation | `Flux_EvaluateBRDF` of LIGHT's `BRDF.slang` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"`): the full `f = f_ss + f_ms + f_d` with the DFG table, so the tracer and the raster paths share the reflectance model and the comparison isolates transport |
| lobe choice | specular with probability `P_s = clamp(lum(E_tot(μv)), 0.1, 0.9)`, diffuse otherwise (dimension 2 of the bounce's sequence) |
| specular sampling | GGX visible-normal sampling (Heitz 2018) with α = max(roughness², 0.002), dimensions 3 and 4 |
| diffuse sampling | cosine-weighted hemisphere, dimensions 3 and 4 |
| pdf | the mixture `P_s × pdf_GGX + (1 − P_s) × pdf_cos`, used for every MIS weight |
| MIS | the power heuristic with β = 2 between the light sample and the BSDF sample, for every light that can be hit by a BSDF sample (area lights, the sun disc, the environment); delta lights (point, spot, a zero-radius directional) take the light sample alone |
| advanced lobes | clear coat, anisotropy, sheen and subsurface reflection through LIGHT's lobe evaluation (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Advanced surface lobe integration"`), sampled by the base mixture; their scenes are Stage 5 with LIGHT's lobes |

<!-- assumption: pending LIGHT -->
The tracer calls `Flux_EvaluateBRDF(Flux_BRDFTerms xTerms, float3 xN, float3 xV, float3 xL)` and `Flux_SampleDFG` from compute; LIGHT's evaluation-order table makes `EvaluateLightingEx` pure and stage-independent, and ADVGFX assumes the two `BRDF.slang` functions have that property and that signature (row AV9 (b)).
<!-- /assumption -->

## Lights and the environment

| Light (LIGHT's model) | Sampling in `PTShade` |
|---|---|
| point (radius 0) | delta: the direction to the position; radiance contribution `I × W(d) × cosθ / d²` with LIGHT's windowed falloff `W(d) = saturate(1 − (d / range)⁴)²` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light attenuation and shape"`), which is part of the light model and so part of the reference |
| spot (radius 0) | the point rule × LIGHT's `S(cos θ)` cone |
| sphere (radius r > 0) | uniform sampling of the cone of directions subtended by the sphere (solid-angle sampling); emitted radiance `L = I / (π r²)` × `W(d)` at the centre distance; MIS with the BSDF sample |
| rect (Stage 5) | uniform area sampling of the rectangle, pdf converted to solid angle; radiance from the light's luminance; `W(d)` at the centre distance; the barn cone |
| disc (Stage 5) | uniform area sampling of the true disc (not LIGHT's 8-gon, whose error is what the scene measures) |
| directional | the sun's angular radius θ_s: uniform cone sampling; radiance `E / (π sin² θ_s)`; θ_s = 0 is a delta light |
| environment | an alias table over the cube's mip-0 texels weighted by luminance × texel solid angle (Walker's method, built on the CPU in `Build`); the uniform environment samples the cosine hemisphere |
| light choice | one light per path vertex, with probability proportional to its unoccluded power estimate at the scene's centre (fixed at `Build`), plus the environment with probability proportional to its power; dimension 5 |

## Integrators

| Integrator | Output per pixel | Specification |
|---|---|---|
| `RADIANCE` | RGB radiance, not pre-exposed | the pipeline above, `m_uMaxBounces` bounces |
| `AMBIENT_VISIBILITY` | scalar in [0, 1] | from the primary hit, one cosine-weighted ray per sample against the scene within `m_fAmbientRadiusMetres`; 1 when unoccluded; the reference for `AE_GTAO` (white sky, no direct light) |
| `LIGHT_VISIBILITY` | scalar in [0, 1] | from the primary hit, one shadow ray toward a sample of light `m_uLightIndex` per sample; the reference for `AE_ContactShadows` and the `penumbra` mask |
| `SPECULAR_REFLECTION` | RGB | from the primary hit, the specular lobe alone is sampled at bounce 0, then the radiance integrator continues with `m_uMaxBounces − 1`; the reference for `AE_SSR` |

## Random numbers

| Element | Rule |
|---|---|
| generator | counter-based: `pcg4d(uint4(x, y, s, d) ^ seed)` (Jarzynski and Olano 2020), no state stored between kernels |
| dimensions | per bounce b: d = 16 b + k, with k = 0, 1 pixel position (b = 0), 2 lobe choice, 3 and 4 BSDF direction, 5 light choice, 6 and 7 light position, 8 Russian roulette, 9 and 10 ambient or visibility rays; k < 16 always |
| seed | `m_ulSeed`, folded to 128 bits by splitting its two halves into the x and y lanes' xor masks |
| consequence | sample s of pixel (x, y) is the same sequence on every run, whatever the batch size or the order kernels process paths in |

## Accumulation and convergence

| Element | Rule |
|---|---|
| per-pixel statistics | n samples; CPU `double` sums Σ R, Σ G, Σ B, Σ Y, Σ Y² (Y = Rec.709 luminance, or the scalar) |
| mean | Σ / n |
| standard error | `SE = sqrt(max(Σ Y² − (Σ Y)² / n, 0) / (n (n − 1)))` |
| converged pixel | `SE ≤ max(m_fRelativeStandardError × mean_Y, m_fAbsoluteStandardErrorFraction × Y_99)`, Y_99 the 99th percentile of the pixel means (nearest rank) |
| converged run | n ≥ `m_uMinSamplesPerPixel` and the converged-pixel fraction ≥ `m_fConvergedPixelFraction` ([Z2-N-REND-945]: proposed 256 spp and 1% of the mean for 99.5% of pixels) |
| stop | at the first batch boundary after which the run is converged (`CONVERGED`), or when the elapsed wall-clock time exceeds `m_uWallClockCapMs` or n reaches `m_uMaxSamplesPerPixel` (`NOT_CONVERGED`, [Z2-N-REND-946]) |
| result | the means and standard errors as `float` in `Flux_PathTraceResult` |

## Determinism

- For one backend, device, driver and pinned software-adapter version, a `Run` over the same scene and settings returns bit-identical means and standard errors: the BVH is built deterministically, every random number is a function of (pixel, sample, dimension), the per-pixel radiance of an iteration is summed in kernel order, and batch sums are accumulated on the CPU in pixel order (`Test_PathTracer_RunIsDeterministic`).
- Results are not promised identical across backends or devices; that difference is why references are recorded per backend ("Reference files") and why the trace-against-reference check is statistical.
- A wall-clock stop depends on timing, so a `NOT_CONVERGED` run's sample count varies; a `CONVERGED` run stops at a batch boundary decided by the statistics alone.

## Raster capture

The runner renders the same scene through the raster path under test with RPROD's parity runner (`Zenith2/Docs/Design/RPROD/Tests.md → "Parity suites"`): the camera static, the view features of `rasterFeaturesOff` cleared, the parity jitter sequence run for 64 frames so TAA has converged, the VT system in `full` mode and settled before frame 0, and the HDR scene colour captured after the TAA resolve and before exposure and tonemapping, divided by the frame's pre-exposure so it is in the tracer's units.

<!-- assumption: pending RPROD -->
The parity runner's capture point "HDR scene colour after TAA, before exposure" (a readback of `Flux_ViewTargets::m_xSceneColour` after `TemporalAA`, in its own `R11G11B10_UFLOAT` format, decoded to `float` on the CPU) is new; RPROD adds it to the runner's surface-data capture (row AV1 (g)).
<!-- /assumption -->

| Capture precision | Bound |
|---|---|
| R and G (6-bit mantissa) | rounding error at most 2⁻⁷ = 0.78% of the value, mean 0.39% |
| B (5-bit mantissa) | at most 2⁻⁶ = 1.56%, mean 0.78% |
| Rec.709 luminance | at most 0.84%, mean 0.42% |

`Test_PhysicalAccuracy_RasterCaptureUnits` bounds a single pixel at 2% (the B channel's worst case plus TAA's resolve error). The relative luminance error is a mean over the mask, so the capture contributes at most its mean, 0.42%, to it: `PT_IBLFurnace`'s proposed 1% therefore leaves 0.58% for the transport error it measures, and every other scene's proposal is at least 2%.

## Comparison tonemap

Both images are mapped by the same operator before FLIP: per channel `T(c) = srgbEncode(1 − exp(−e × c))`, e = the scene's `exposure`, IEC 61966-2-1 encoding. The operator is smooth, monotonic and never clips, so FLIP sees highlight differences, and it belongs to the comparison alone (`Internal/Flux_PTTonemap.cpp`), not to LIGHT's output transforms.

## Error metrics

| Metric | Definition | Used by |
|---|---|---|
| FLIP mean | NVIDIA LDR-FLIP (Andersson et al. 2020) of the two tonemapped images at 67 pixels per degree, the pinned `flip` header RPROD's parity tooling already uses, averaged over the mask | every RGB scene (`Z2-B-ADVGFX-008`) |
| relative luminance error | `Σ_mask |Y − Y_ref| / Σ_mask Y_ref` over pixels with `Y_ref ≥ floor × max Y_ref`, in linear HDR | every RGB scene: the second value of the scene's `Z2-B-ADVGFX-008` row (LIGHT's second tolerance column) |
| mean absolute error | `Σ_mask |x − x_ref| / count` | `AE_GTAO` |
| misclassified fraction | fraction of mask pixels with `(x > 0.5) ≠ (x_ref > 0.5)` | `AE_ContactShadows` |
| z above 4 fraction | `z = |Y_trace − Y_ref| / sqrt(SE_trace² + SE_ref²)` per pixel; the fraction of mask pixels with z > 4 | the trace-against-reference check of a smoke or acceptance trace; at most 0.5% |

The metrics are test support of `ZenithFlux_Tests` ("Library targets and programs"). `Test_PTMetrics_FlipMatchesReferenceImplementation` (FLIP of two fixture image pairs equals the pinned header's published values within 1e-4), `Test_PTMetrics_RelativeLuminanceAnalytic` (constant images with a known ratio), `Test_PTMetrics_MaskRespected`, `Test_PTMetrics_ZScoreSyntheticNoise` (synthetic Gaussian noise of known variance gives the expected tail fraction within 0.1% over 2²⁰ pixels) and `Test_PTMetrics_DownsampledReferenceSE` ("Gate and acceptance runs": a reference of independent per-pixel noise of known SE, 2 × 2 downsampled, gives SE_down within 1% of the propagated value and the expected z tail).

## Reference files

`.zref`, a versioned binary written and read field by field, little-endian (Z2-D-038):

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `'ZPTR'` (bytes `5A 50 54 52`) |
| 4 | 2 | format version | `uFLUX_PT_REFERENCE_FORMAT_VERSION` = 1 |
| 6 | 2 | flags | bit 0 converged; bits 1–15 zero |
| 8 | 2 | width | 16 … 1,024 |
| 10 | 2 | height | 16 … 1,024 |
| 12 | 1 | channels | 1 or 3 |
| 13 | 1 | integrator | `Flux_PathTraceIntegrator` |
| 14 | 1 | backend | `Zenith_RHIBackend` |
| 15 | 1 | reserved | zero |
| 16 | 4 | samples per pixel | |
| 20 | 4 | wall-clock milliseconds | |
| 24 | 4 | tracer version | `uFLUX_PT_VERSION` |
| 28 | 4 | converged pixel fraction | f32 |
| 32 | 32 | fingerprint | SHA-256 ("Reference manifest and fingerprint") |
| 64 | 32 | device | UTF-8, NUL-padded |
| 96 | 8 | driver version | |
| 104 | 24 | mean plane descriptor | `Zenith_CompressedSectionDesc` of the mean plane |
| 128 | 24 | error plane descriptor | `Zenith_CompressedSectionDesc` of the standard-error plane |
| 152 | 8 | reserved | zero |
| 160 | — | the mean plane, then the error plane, each 16-byte aligned | mean: `float16` per channel per pixel, row-major from the top; error: `float16` per pixel; each compressed with zstd (`Zenith_CompressSection`) or stored |

`Flux_ReadPathTraceReference` validates magic, version, flags, reserved fields, extents, channels against the integrator, the descriptors and the decoded sizes (`width × height × channels × 2` and `width × height × 2`), and returns `FLUX_PT_ERROR_REFERENCE_CORRUPT` for any failure (`Test_PTReference_RoundTrip`, `Test_PTReference_ReaderRejectsCorruption`, fuzz target `Zenith_Fuzz_PathTraceReference`). Parser limits: extents 16 … 1,024, decoded planes at most 6,291,456 bytes (row AV7).

### Reference size budget

| Reference | Decoded planes | Count per backend |
|---|---|---|
| 256 × 256 RGB (`PT_*`, `AE_SSR`, `AE_SSGI`) | 393,216 mean + 131,072 error = 524,288 bytes | 10 |
| 320 × 180 RGB (`TerrainVT`) | 345,600 + 115,200 = 460,800 bytes | 1 |
| 256 × 256 one channel (`AE_GTAO`, `AE_ContactShadows`) | 131,072 + 131,072 = 262,144 bytes | 2 |
| one backend | 6,227,968 bytes | 13 files |
| a full recording (both backends, 26 files with their 160-byte headers) | 12,460,096 bytes before compression | 26 files |

The budget of one full recording is 16 MiB (16,777,216 bytes) stored; the recording harness refuses to write a set whose stored total exceeds it (`Test_PTReference_RecordingSetSizeRefused`), and `References.json` records each file's stored `bytes`. Converged Monte Carlo planes are noise-like `float16`, which zstd compresses little, so the stored size is close to the decoded size, and each re-record adds a full copy to history. Every `.zref` is therefore LFS-tracked through one `.gitattributes` line for `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/References/*.zref`, which the first recording ticket adds; that satisfies [Z2-N-BUDGET-177] whatever value `Z2-B-CONTENT-001` (LFS tracking threshold) takes, and `References.json` stays plain text.

## Reference manifest and fingerprint

`Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/References/References.json` lists every committed reference: `{ "scene": path, "backend": "d3d12"|"vulkan", "file": "<Scene>.<backend>.zref", "bytes": stored size, "fingerprint": hex, "samples": n, "wallClockMs": t, "converged": true, "device": "WARP 1.0.x"|"llvmpipe …", "recorded": "YYYY-MM-DD" }`, written by the recording harness, sorted by (scene, backend). One field is not the harness's: `"pendingRecording": "<ticket key>"`, which a worker adds by hand to an entry its change stales and the recording ticket removes ("Stale reference landing protocol").

The fingerprint is `Zenith_Sha256Digest` (`Zenith2/Docs/Design/CORE.md` for the digest API) over this byte stream:

| # | Bytes |
|---|---|
| 1 | the ASCII tag `ZPTR-FP1` |
| 2 | the scene document's committed bytes (u64 length, then the bytes) |
| 3 | every referenced asset's 16-byte catalogue content hash (`Zenith_Hash128`, low word then high word), sorted by GUID (u32 count, then the hashes) |
| 4 | every program blob hash the run uses (the tracer's eight programs and every resolve variant), ascending (u32 count, then the u64 hashes) |
| 5 | every member of `Flux_PathTraceSettings` field by field |
| 6 | the backend byte and the pinned software adapter's version string from `ThirdParty/manifest.json` (u32 length, then the bytes) |
| 7 | `uFLUX_PT_VERSION` |

A change to the scene, to any cooked input, to any shader the trace runs (materials and the BRDF included), to the settings, to the adapter pin or to the tracer's arithmetic therefore changes the fingerprint (`Test_PTReference_FingerprintCoversInputs`: each of the seven inputs changed alone changes it).

## Reference capture procedure

A committed golden is required here, where the binary-artefacts ruling otherwise prefers in-run comparison (test-time generation for fixtures, a `needs-human` recording only where a committed golden is required): gate mode cannot trace the scenes to convergence inside the gate's 30-minute phase cap (the per-scene caps of "Proposed per-scene caps" sum to 11,250 seconds per backend, 22,500 seconds for both), so the gate compares against a recorded converged result and acceptance mode traces in-run. References are outputs no reviewer can derive by hand, so they are recorded under ruling GOLDEN-FILES and never by a worker (ruling WORKER-EVIDENCE):

| Step | Who | Action |
|---|---|---|
| 1 | an ordinary harness ticket | lands `Zenith_Bench_PathTraceReference` under `Zenith2/Benchmarks/PathTraceReference/` (`zenith_add_benchmark`), which traces every reference scene to convergence with `Flux_PathTracer` on WARP and lavapipe and writes `<Scene>.<backend>.zref` and `References.json` into `${ZENITH_OUT}/pt-references/`; its CTest smoke records one 32 × 32 self-check scene at 64 spp and reads it back, inside the normal `Test` step (Z2-D-084) |
| 2 | a `needs-human` recording ticket, blocked by the harness ticket and by every scene's owning ticket | runs the harness on the gate host (WARP and lavapipe; no GPU, so no `needs-gpu`), checks every output reports converged, and commits the files under `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/References/`, LFS-tracked ("Reference size budget"), removing every `pendingRecording` marker the recording answers |
| 3 | every gate run | `Test_PhysicalAccuracy_ReferencesFresh` recomputes each reference's fingerprint from the current tree; a mismatch fails, naming the scene and the first differing input, unless the entry carries `pendingRecording` ("Stale reference landing protocol"); a missing file fails |
| 4 | a change that stales a reference | follows "Stale reference landing protocol": it marks the entries it stales and lands; the re-record is the `needs-human` recording ticket in the shape of step 2 (ruling DIGEST-REFRESH), never a worker's refresh from test output |

### Stale reference landing protocol

Input 4 of the fingerprint covers every program the trace runs, `EvaluateMaterial`, every material module and LIGHT's `BRDF.slang` included, so an ordinary LIGHT, GEOM, ENV, RPROD or ADVGFX ticket that edits one of those shaders stales references it cannot re-record. The protocol lets such a ticket land without weakening acceptance:

| Step | Rule |
|---|---|
| (a) the marker | the staling ticket adds `"pendingRecording": "<key>"` to every `References.json` entry its change stales, where `<key>` is the board key of a `needs-human` recording ticket filed before it; the field is plain text a worker may edit, and the binary files are untouched |
| (b) the gate | `Test_PhysicalAccuracy_ReferencesFresh` passes a mismatching entry only when it carries a well-formed `pendingRecording` key, recording a skip reason in the shape of [Z2-N-REND-034] ("reference <scene>.<backend> stale; pending recording <key>"); the gate-mode comparison of that scene and backend is skipped with the same reason. A mismatch without the field fails, naming the scene and the first differing input; a marker on an entry whose fingerprint matches also fails, so a marker cannot outlive its recording |
| (c) the ticket | the staling ticket's Definition of Done includes the markers and a `blocks` link from it to the pre-filed recording ticket, so the recording runs on the tree that contains the change; no test can see whether that ticket is open, which is why (d) exists |
| (d) acceptance | Stage 5 acceptance and every `FLUX_PA_MODE_ACCEPTANCE` run fail while any entry carries `pendingRecording` |

`Test_PhysicalAccuracy_PendingRecordingProtocol` drives the four cases on a fixture manifest: a stale entry with a marker skips with the reason, a stale entry without one fails naming the input, a fresh entry with a marker fails, and acceptance mode fails while a marker remains.

## Gate and acceptance runs

| Run | Where | What |
|---|---|---|
| tracer unit tests | `ZenithPathTracer_Tests`, `Test` step, WARP and lavapipe | BVH, traversal, sampling and accumulation cases of `Zenith2/Docs/Design/ADVGFX/Tests.md → "Path tracer tests"` |
| self-check scenes | the same | `PTSelf_WhiteFurnace` (a sphere of albedo 1, F0 0.04, in a uniform environment of radiance 1: every pixel converges to 1 within 3 SE), `PTSelf_LambertPlane` (an infinite Lambert plane under a point light: irradiance `I cos θ / d²` closed form per pixel), `PTSelf_SphereLightFormFactor` (a sphere light over a Lambert plane against the analytic form factor), `PTSelf_RayDifferentials` (a textured plane's primary-hit mip equals the visibility-buffer resolve's analytic mip) |
| gate comparison | `ZenithFlux_Tests` (`Tests/PhysicalAccuracy/`), `Test` step | `FLUX_PA_MODE_GATE`: each LIGHT and ENV comparison test compares its raster capture against the committed reference with the scene's tolerances, and traces the scene at 32 spp at most 128 × 128 against the 2 × 2 box-downsampled reference with the z check, inside the 30-minute phase cap; a reference marked `pendingRecording` skips the comparison with its reason |
| acceptance | the Stage 5 acceptance suite (`Run-Committed.ps1 -Acceptance -Stage 5`, Z2-D-083) | `FLUX_PA_MODE_ACCEPTANCE`: every scene traced to convergence in-run on WARP and on lavapipe under its caps, the raster paths compared against that in-run result ([Z2-N-REND-945], [Z2-N-REND-946]), and the in-run result checked against the committed reference with the z check |

The z check of a gate trace against a downsampled reference uses the downsampled reference's own standard error: for a 2 × 2 box of independent pixels, `mean_down = (Σ mean_i) / 4` and `SE_down = sqrt(Σ SE_i²) / 4`, so `z = |Y_trace − mean_down| / sqrt(SE_trace² + SE_down²)` (`Test_PTMetrics_DownsampledReferenceSE`).

## Reports and artefacts

Every runner call writes `${ZENITH_OUT}/PhysicalAccuracy/<scene>/<path>.<backend>/report.json` (scene, mode, path, backend, device, spp, wall-clock, outcome, converged fraction, every metric with its tolerance, pass or fail), and on a failure also the raster HDR image and the reference as `float16` RGB raw planes with a JSON header, both tonemapped PNGs, the FLIP map, the luminance-ratio map and the z map, and the raster run's graph dump, as parity failures retain theirs ([Z2-N-REND-943]; `Test_PhysicalAccuracy_FailureRetainsArtefacts`).

## Proposed per-scene caps

The proposed values the Stage 1 per-system budget ticket for ADVGFX adopts or replaces; tolerances are LIGHT's and ENV's proposals:

<!-- elaboration -->
| Scene | `Z2-B-ADVGFX-004` resolution | `-005` cap (both adapters) | `-006` min spp | `-007` (relative SE, pixel fraction) | `-008` (FLIP, RLE) |
|---|---|---|---|---|---|
| `PT_AnalyticPoint`, `PT_AnalyticSpotDirectional` | 256 × 256 | 450 s | 256 | 1%, 99.5% | 0.02, 2% |
| `PT_SphereLight`, `PT_RectLight`, `PT_DiscLight` | 256 × 256 | 900 s | 256 | 1%, 99.5% | 0.04, 5% |
| `PT_IBLFurnace` | 256 × 256 | 450 s | 256 | 0.5%, 99.5% | 0.01, 1% |
| `PT_IBLEnvironment` | 256 × 256 | 1,350 s | 512 | 1%, 99.5% | 0.04, 5% |
| `PT_AreaLightShadow` | 256 × 256 | 1,350 s | 1,024 | 1%, 99% | 0.06, 8% (penumbra mask) |
| `TerrainVT` | 320 × 180 | 1,350 s | 256 | 1%, 99.5% | 0.03, 3% |
| `AE_GTAO` | 256 × 256 | 450 s | 256 | 1%, 99.5% | mean absolute 0.08 |
| `AE_ContactShadows` | 256 × 256 | 450 s | 256 | 1%, 99.5% | misclassified 10% |
| `AE_SSR` | 256 × 256 | 900 s | 512 | 1%, 99.5% | FLIP 0.08 (on-screen reflection mask) |
| `AE_SSGI` | 256 × 256 | 1,350 s | 512 | 1%, 99.5% | LIGHT's SSGI error budget |

The rows are "one value per reference scene": `-005` takes the slower adapter's cap (lavapipe's), so one number serves both; `-007` carries two values per scene (relative standard error and converged-pixel fraction) and `-008` two per RGB scene (FLIP mean and relative luminance error), which cross-doc item X422 notes on the rows. The caps sum to 11,250 seconds per backend.

## Compute and hardware traversal

| Stage | Traversal | Rule |
|---|---|---|
| S5 and after | `FLUX_PT_TRAVERSAL_COMPUTE_BVH` alone | the software BVH and compute traversal of this file, on every device; the only traversal of Z2-D-037 and the only value of `Flux_PathTraceTraversal` |
| proposal | a ray-query traversal (flagged proposal ADVGFX-1) | not part of this design. If the proposal is approved, its own Stage 7 ticket adds the enumerator, the error code for a device without ray queries and the capability query to `Flux_PathTracer.h`, ray-query permutations of `PTIntersect` and `PTShadowIntersect` over the TLAS of a `Flux_RayTracingScene` built from the same world-space triangles, and `Test_PathTracer_HardwareMatchesComputeTraversal` (z above 4 fraction ≤ 0.5% against the compute run on every self-check and reference scene); acceptance always uses compute |

<!-- informative -->
The hardware option is an accelerator for development runs on RT-capable desktops, never a second reference: Z2-D-037 rejected hardware ray tracing as the reference because WARP and lavapipe lack it in the pinned versions and because RT is deferred to Stage 7 (Z2-D-043). Adding it at all is flagged proposal ADVGFX-1 (`C:/dev/Zenith2-drafts/findings/FLAGGED_PROPOSALS.md`); until the human approves it, the Stage 7 requirement that delivers it is deferred and conditional, and the compute traversal is the whole design.
<!-- /informative -->

## Path tracer failure handling

| Case | Behaviour |
|---|---|
| invalid settings or scene input | `FLUX_PT_ERROR_INVALID_SETTINGS` or `FLUX_PT_ERROR_SCENE_INVALID`, naming the member or the input index; nothing is allocated on the device |
| BVH limits | `FLUX_PT_ERROR_BVH_LIMIT`, with the triangle count and depth |
| allocation failure | `FLUX_PT_ERROR_OUT_OF_MEMORY`; everything allocated by the call is released first (failure-injection points `0x41560010` … `0x41560014` for path buffers at `Create`, and scene upload, accumulators, PSO creation and the readback ranges at the first `Run`, each with a `Test_PathTracer_Create_FailsAt<Step>_Unwinds` or `Test_PathTracer_Run_FailsAt<Step>_Unwinds` case) |
| device loss during a run | `FLUX_PT_ERROR_DEVICE_LOST` at the next batch boundary; the tracer is unusable and must be destroyed |
| wall-clock cap | `NOT_CONVERGED`, a successful `Run`; the runner fails the comparison and writes the report |
| cancellation | `Cancel()` sets a flag read at the next batch boundary; the batch's `RunToolGraph` call has already returned with its readbacks `READY`, so `Run` releases its readback ranges and returns `CANCELLED` |
| a missing or stale reference in gate mode | the comparison test fails with `FLUX_PT_ERROR_REFERENCE_STALE` or a missing-file message naming the recording ticket shape, unless the entry carries `pendingRecording` (a skip with its reason) |
| `RunToolGraph` refused or failed | `FLUX_PT_ERROR_TOOL_GRAPH` (or `FLUX_PT_ERROR_DEVICE_LOST` for `DEVICE_LOST`), logged with the `Flux_ToolGraphError` value; nothing of the batch is accumulated |
| `SettleToolFrames` returns `VT_NOT_SETTLED` | `FLUX_PT_ERROR_VT_NOT_SETTLED`; nothing is traced |
