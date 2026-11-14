# RPROD design: feature functions, path frames and pass ownership

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It specifies the public feature-function surface of every pass of every shading path, the internal prologue feature functions, the four path frame functions that compose them, the ownership and file of each pass implementation, the shared feature input types, the async-compute flags and the present pass.

## Scope of this file

The specification of every pass (program, parameter fields, targets and formats, queue, capability, budget row, permutations and feature-function signature) is the per-pass section files listed in `Zenith2/Docs/Rendering.md → "Per-pass specifications"`, and the order of every frame is `Zenith2/Docs/Rendering/FrameAnatomy.md`. This file does not restate a pass table; it fixes which header declares each feature function, which area implements it, where its code lives, and how the path functions call them ([Z2-N-REND-723]).

## Flux_Features header

`Zenith2/Engine/ZenithFlux/Flux_Features.h` is RPROD's public header declaring every per-view feature function, so the editor and modules compose passes through one surface (Z2-D-055: features are free functions over handles). Every signature is exactly the one of the pass's per-pass table; the header adds nothing but contract coverage (one `functions` block, [Z2-N-CONTRACT-112]) and the input types of "Shared feature input types".

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Features.h
#pragma once
#include "ZenithFlux/Flux_RenderGraph.h"
#include "ZenithFlux/Flux_ViewContext.h"

/// @contract functions Flux_Features
/// Ownership: Free functions over graph handles; each declares its passes into the caller's graph and owns nothing.
/// Thread affinity: OWNER_THREAD (the Flux.BuildGraph node that owns the graph).
/// Invalidation: Returned handles are frame-scoped ([Z2-N-CONTRACT-095]).
/// Units: As Flux_RenderGraph.
/// Errors: None returned; misuse is a graph diagnostic at compile. A function whose view feature bit is clear, or that
///   the capability matrix excludes for the view's path, adds no pass and returns invalid handles ([Z2-N-REND-281]).
/// Version: since Z2 Stage 2 (culling, HiZ, prepass, opaque: Stage 3; Forward, Deferred, visibility buffer: Stage 5).
/// @endcontract

// Culling and HiZ (RPROD)
Flux_CullOutput Flux_AddInstanceCullPhase1(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
Flux_CullOutput Flux_AddInstanceCullPhase2(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xPhase1, Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddHiZBuild(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xDepth);
// Forward+ and Forward (RPROD)
void Flux_AddForwardPlusPrepass(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets);
void Flux_AddForwardPlusOpaque(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets);
Flux_RGBuffer Flux_AddPerObjectLightLists(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull);
void Flux_AddForwardOpaquePerObject(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_RGBuffer xPerObjectLights, Flux_ViewTargets& xTargets);
// Deferred (RPROD, decals GEOM)
void Flux_AddDeferredDepthPrepass(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets);
void Flux_AddGBuffer(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_CullOutput& xCull, Flux_ViewTargets& xTargets);
void Flux_AddDeferredDecals(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
// Visibility buffer (RPROD)
Flux_MeshletCullOutput Flux_AddMeshletCullPhase1(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
Flux_MeshletCullOutput Flux_AddMeshletCullPhase2(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_MeshletCullOutput& xPhase1, Flux_ViewTargets& xTargets);
void Flux_AddVisibilityRaster(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_MeshletCullOutput& xCull, Flux_ViewTargets& xTargets);
Flux_MaterialBins Flux_AddMaterialClassify(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
void Flux_AddMaterialResolve(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_MaterialBins& xBins, Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddVelocityReconstruct(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
// Shared lighting and late passes (LIGHT, ENV, VFX, RPROD transparent)
Flux_RGBuffer Flux_AddLightClustering(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddVolumetricFog(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddSSAO(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_SSAOSettings& xSettings);
Flux_RGTexture Flux_AddContactShadows(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddDeferredLighting(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
void Flux_AddSkyAtmosphere(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddSSR(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
void Flux_AddTransparentForward(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
void Flux_AddParticleDraw(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
// Common epilogue (LIGHT, UI, DEVT, RPROD present)
Flux_RGTexture Flux_AddTemporalAA(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
Flux_RGBuffer Flux_AddAutoExposure(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
Flux_RGTexture Flux_AddDepthOfField(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGTexture xDepth);
Flux_RGTexture Flux_AddMotionBlur(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGTexture xVelocity, Flux_RGTexture xDepth);
Flux_RGTexture Flux_AddBloom(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xColour, Flux_RGBuffer xExposure);
Flux_RGTexture Flux_AddTonemap(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_TonemapInputs& xInputs, Flux_RGTexture& xOutput);
void Flux_AddUIComposite(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput);
void Flux_AddDebugOverlay(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput);
void Flux_AddPresent(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture xOutput);
// Path frame functions (RPROD)
void Flux_BuildForwardPlusView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
void Flux_BuildForwardView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
void Flux_BuildDeferredView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
void Flux_BuildVisibilityBufferView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
```

`Flux_BuildView` (`Zenith2/Engine/ZenithFlux/Flux_ViewContext.h`) dispatches on `m_eShadingPath` to the four path functions.

## Prologue feature functions

The common prologue's feature functions take renderer-internal systems, so they are declared in `Zenith2/Engine/ZenithFlux/Internal/Passes/Flux_PrologueFeatures.h` and called by `Flux_Renderer::BuildGraph` alone ([Z2-N-REND-720], [Z2-N-REND-724]); their signatures are the per-pass tables' exactly:

| Function | Passes | Implementing area |
|---|---|---|
| `void Flux_AddGPUSceneUpdate(Flux_RenderGraph&, Flux_GPUScene&, Flux_FrameShared&)` | `GPUSceneGrow`, `GPUSceneScatter` | RHI (the GPU scene, `Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`); the RPROD `BucketScatter` of `Zenith2/Docs/Design/RPROD/Culling.md → "Draw keys and command regions"` is declared right after it |
| `void Flux_AddStreamingUploads(Flux_RenderGraph&, Flux_StreamingUploader&)` | `StreamingUpload` | GEOM (`Zenith2/Docs/Design/GEOM/Textures.md → "Streaming upload arbitration"`) |
| `void Flux_AddSkinning(Flux_RenderGraph&, const Flux_SkinningBatch&, Flux_FrameShared&)` | `MorphTargets`, `ComputeSkinning` | GEOM (`Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"`) |
| `void Flux_AddParticleSimulation(Flux_RenderGraph&, const Flux_ParticleFrame&, Flux_FrameShared&)` | `ParticleEmit`, `ParticleSimulate`, `ParticleCompactAndSortKeys`, `ParticleSort` | VFX (pending `Zenith2/Docs/Design/VFX.md`) |
| `void Flux_AddProceduralGeometry(Flux_RenderGraph&, const Flux_ProceduralFrame&, Flux_FrameShared&)` | `ProceduralGeometry` | GEOM |
| `void Flux_AddLightDecalList(Flux_RenderGraph&, const Flux_ViewList&, Flux_FrameShared&)` | `LightDecalListBuild` | LIGHT (pending `Zenith2/Docs/Design/LIGHT.md`) |
| `void Flux_AddCascadedShadows(Flux_RenderGraph&, const Flux_ViewList&, Flux_FrameShared&)`, `void Flux_AddLocalShadows(Flux_RenderGraph&, const Flux_ViewList&, Flux_FrameShared&)`, each calling `void Flux_AddShadowView(Flux_RenderGraph&, const Flux_ViewContext&, Flux_FrameShared&)` per shadow view | the atlas clear, `ShadowInstanceCull`, `ShadowDepth`, `ShadowHiZBuild` | LIGHT (`Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_Shadows header"`: the functions, the atlas, the shadow depth program and bias); RPROD (the culling and HiZ half through `Flux_AddShadowViewCulling`, `Zenith2/Docs/Design/RPROD/Culling.md → "Shadow view culling realisation"`) |

## Path frame functions

Each path function calls its feature functions in the order of its table in `Zenith2/Docs/Rendering/FrameAnatomy.md` ([Z2-N-REND-723], [Z2-N-REND-737], [Z2-N-REND-752], [Z2-N-REND-762], [Z2-N-REND-773]), guarding each by the view's feature mask and the capability matrix (`Flux_GetMatrixCell`), and calling the view extensions at their points (`Zenith2/Docs/Design/RPROD/Views.md → "View extensions"`). Forward+ is:

```cpp
void Flux_BuildForwardPlusView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xT)
{
	const Flux_CullOutput xCull1 = Flux_AddInstanceCullPhase1(xGraph, xView, xT);
	Flux_AddForwardPlusPrepass(xGraph, xView, xCull1, xT);                 // phase 1: CLEAR, STORE
	xT.m_xHiZ = Flux_AddHiZBuild(xGraph, xView, xT.m_xDepth);
	const Flux_CullOutput xCull2 = Flux_AddInstanceCullPhase2(xGraph, xView, xCull1, xT);
	Flux_AddForwardPlusPrepass(xGraph, xView, xCull2, xT);                 // phase 2: LOAD, STORE
	xT.m_xHiZ = Flux_AddHiZBuild(xGraph, xView, xT.m_xDepth);             // final
	xT.m_xClusters = Flux_AddLightClustering(xGraph, xView, xT);
	xT.m_xFogVolume = Flux_AddVolumetricFog(xGraph, xView, xT);            // before any shading ([Z2-N-REND-739])
	xT.m_xAO = Flux_AddSSAO(xGraph, xView, xView.m_pxQuality->m_xLight.m_xSSAO);   // LIGHT's Flux_SSAOSettings
	xT.m_xContactShadows = Flux_AddContactShadows(xGraph, xView, xT);
	const Flux_CullOutput xCullBoth = Flux_CombineCullOutputs(xCull1, xCull2);   // both phases' draws ([Z2-N-REND-740])
	Flux_AddForwardPlusOpaque(xGraph, xView, xCullBoth, xT);
	Flux_AddSkyAtmosphere(xGraph, xView, xT);
	Flux_CallViewExtensions(xGraph, xView, xT, FLUX_EXTENSION_POINT_AFTER_OPAQUE);
	Flux_AddSSR(xGraph, xView, xT);
	Flux_AddTransparentForward(xGraph, xView, xT);                         // OpaqueColourCopy, TransparentCull, TransparentForward
	Flux_CallViewExtensions(xGraph, xView, xT, FLUX_EXTENSION_POINT_TRANSPARENT);
	Flux_AddParticleDraw(xGraph, xView, xT);
	Flux_BuildCommonEpilogue(xGraph, xView, xT);                           // "Common epilogue" below
}
```

`Flux_CombineCullOutputs` (RPROD, `Flux_Features.h`) returns a `Flux_CullOutput` whose `m_uPhaseCount` is 2 and whose two phase entries are the inputs', so a raster consumer issues each bucket's indirect call once per phase. Forward, Deferred and the visibility buffer follow their tables the same way: Forward inserts `Flux_AddPerObjectLightLists` and calls `Flux_AddForwardOpaquePerObject` instead of clustering and Forward+ opaque; Deferred calls `Flux_AddDeferredDepthPrepass` in both phases, then `Flux_AddGBuffer`, `Flux_AddDeferredDecals`, GTAO and contact shadows, clustering and fog, `Flux_AddDeferredLighting`; the visibility buffer calls the meshlet culling, the visibility raster in both phases, classification, clustering, resolve, velocity, GTAO and fog, and deferred lighting. `Flux_BuildCommonEpilogue` calls, in order, the view extensions at `BEFORE_UPSCALE`, TAA, auto-exposure, depth of field and motion blur when their bits are set, bloom, tonemap, the video composite (row 6b) and the extensions at `BEFORE_UI` (row 6c) of "Common epilogue rows 6b and 6c", UI composite, the extensions at `OVERLAY`, debug overlay, the HiZ debug overlay when `r.debug.hiz` selects the view (development builds, `Zenith2/Docs/Design/RPROD/Culling.md → "HiZDebugOverlay"`) and present ([Z2-N-REND-785], [Z2-N-REND-787]).

### Common epilogue rows 6b and 6c

<!-- elaboration -->
<!-- assumption: pending core-doc (X292 (b), X313 (e), X454 (d)) -->
RPROD answers rows CN8 and TD2 with one epilogue order between `TonemapAndGrade` (row 6) and `UIComposite` (row 7):

| # | Pass | Feature function | Owner | Rule |
|---|---|---|---|---|
| 6b | `VideoComposite` | `Flux_AddVideoComposite(xGraph, xView, xTargets.m_xOutput)` (CINE's, `ZenithFlux/Flux_Video.h`) | CINE | called for every view; CINE's function records nothing unless the view is the main view (the editor's game view) and `Flux_FrameShared::m_pxVideo` has an active player or an overlay record |
| 6c | the extensions at `FLUX_EXTENSION_POINT_BEFORE_UI` | `Flux_CallViewExtensions(…, FLUX_EXTENSION_POINT_BEFORE_UI)` | RPROD calls; TWOD's canvas passes | output extent; where tonemapping would run when the `TONEMAP` bit is clear |

So 2D canvases draw over cinematic video and under the UI (X313 (e)). The common prologue gains row 2b `VideoConvert` after `StreamingUpload` (row 2): CINE's video-system ticket records it at `Flux.BuildGraph` step 8 through its own internal function (`Zenith2/Docs/Design/CINE/Video.md → "GPU side"`) when `m_pxVideo` is non-null, and declares it outside `Internal/Passes/Flux_PrologueFeatures.h`. Both video passes publish their GPU time through `Flux_PassTimingPublisher` under the zone names `VideoConvert` and `VideoComposite` with the view suffix, which is what the pending row "Video conversion and composite GPU" measures by MP-GPUPASS (cross-doc item X455, row CnB1).
<!-- /assumption -->

A shadow view runs no path function: `Flux_AddShadowView` is its whole frame ([Z2-N-REND-725]). A capture view runs the Forward (desktop) or Forward+ (mobile) function with its reduced feature mask ([Z2-N-REND-691]); a preview view runs Forward with the preview mask.

### Environment and lighting passes in the path functions

ENV's frame slots (row EN11) and LIGHT's Stage 5 SSGI pass (row L15) are Rendering's FrameAnatomy rows since X127 (3) and X105 (reconciliation row RP20):

| Pass | Where the path functions call it | Paths |
|---|---|---|
| `Env_AddCloudLayer` | once per frame as a shared producer before the first view's `Flux_AddSkyAtmosphere`; its output reaches the sky draw through `Flux_FrameShared` | every path |
| `Env_AddUnderwater` | after `Flux_AddParticleDraw`, before `Flux_BuildCommonEpilogue`, for a view ENV marks submerged | every path |
| `Env_AddVolumetricClouds`, `Env_AddCloudShadowMap`, `Env_AddOceanFFT` (Stage 5) | in the common prologue after `Flux_AddProceduralGeometry`, async-flagged where the frame table marks A | every path |
| ENV's detail generator | through `Flux_AddProceduralGeometry` (GEOM's), no slot of its own | every path |
| `Flux_AddSSGI` (Stage 5, LIGHT) | after `Flux_AddContactShadows` | Forward+, Forward, Deferred and visibility buffer on desktop tiers |

ENV's functions are ZenithFlux functions ENV declares in `Flux_Environment.h` (row EN9), so the path functions call them without a module edge; each adds no pass when its feature is absent from the view or the world. The capability-matrix rows ENV asks for (terrain, placed foliage, detail foliage, water, underwater, cloud layer, volumetric clouds) join `CapabilityMatrix.json` with Rendering's table in the same diff ([Z2-N-REND-695]).

### LIGHT additions to RPROD's pass tables

LIGHT's rows L5, L10 and L12 are in the per-pass tables of passes RPROD implements since X103 (applied); RPROD's pass tickets implement them:

| LIGHT row | RPROD pass | Change |
|---|---|---|
| L5 (X103 e) | `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `TransparentForward` | `m_xShadowAtlas` and `m_xShadowViews` become `[Optional]`: a frame with no shadowed light has no atlas and sets neither field |
| L12 (X103 h) | `ForwardPlusOpaque`, `ForwardOpaquePerObject` | a second colour target `m_xSpecularWeight` (R11G11B10F, render extent, `CLEAR` 0, `STORE`) attached only when the view's mask carries `FLUX_VIEW_FEATURE_SSR`, selected by the permutation `bPERM_SSR_WEIGHT`; with the bit clear the target is not attached and the write mask is 0 ([Z2-N-REND-919], [Z2-N-REND-921]); `Flux_ViewTargets::m_xSpecularWeight` carries it to SSR |
| L10 (X103 i), Stage 4 | `ForwardPlusOpaque`, `ForwardOpaquePerObject` | `m_xIBLSpecular` becomes `Zenith_RHISRV<Zenith_RHITexCubeArray>`, `m_xIBLIrradiance` a graph buffer SRV, and `m_xProbeRecords` is added |

## Virtual texture bindings in material passes

<!-- elaboration -->
<!-- assumption: pending ADVGFX, core-doc (X420 (b) to (d), (j)) -->
RPROD's material-evaluating passes bind ADVGFX's VT pools (rows AV1 (b), (e), (f); `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material input binding"`, "Feedback writers", "Feedback fields of material passes"), Stage 4 on Forward+ and Stage 5 on the other paths:

| Rule | Specification |
|---|---|
| pool fields | every RPROD pass with an alpha-test permutation or material variants of the `opaque` or `alphaTest` domains declares the three `[Optional]` fields `m_xVTPhysicalSRGB`, `m_xVTPhysicalLinear` and `m_xVTPageTable` on `Flux_FrameShared::m_xVTPhysical` and `m_xVTPageTable`, fills `Flux_MaterialInput::m_xVirtual` from them and sets bit 1 of `m_uFeedbackFlags`: `ForwardPlusPrepass` and `DeferredDepthPrepass` (alpha-test buckets), `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer`, `VisibilityRaster` (alpha-test variants) and `MaterialResolve`; `ShadowDepth`'s alpha-tested bucket is LIGHT's program and binds them in LIGHT's ticket |
| feedback fields | `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer` and `MaterialResolve` add `m_xVTFeedback`, `m_uVTFeedbackStride` (0 when the view writes no feedback) and `m_uVTFeedbackJitter`, call `Flux_VTWriteFeedback` in their epilogue, and the three pixel passes carry `[earlydepthstencil]` |
| feedback buffer | `Flux_ViewTargets` gains `Flux_RGBuffer m_xVTFeedback`; each path function calls `Flux_AddVTFeedbackClear` before the view's first feedback writer and `Flux_AddVTFeedbackReadback` after its last, before `TemporalAA`; ADVGFX's functions add no pass for a view that writes no feedback ("Views that write feedback") |
| VT disabled | with `GetVirtualTextureSystem()` null or VT disabled the optional fields stay unset and bit 1 clear, and the passes sample the tail texture of any VT handle ([Z2-N-REND-068]) |
| not RPROD's | `EvaluateMaterial`'s hand-off to `Sampling.slang` and `Flux_SampleMaterial`'s VT branch (rows AV1 (c), (d)) live in GEOM's `Common/Material.slang` and `Common/Sampling.slang`; the transparent and decal passes bind no pool |
| measured against | the pending row "VT branch cost in VT-free material passes (S24)" (MP-GPUPASS A/B of `ForwardPlusOpaque`, cross-doc item X422) |
| Stage 7 | `FLUX_INSTANCE_FLAG_VIRTUAL_GEOMETRY` (bit 6 of "Instance flags" in `Zenith2/Docs/Design/RPROD/Extractors.md`) marks an instance ADVGFX's virtualized-geometry passes draw; the visibility-buffer buckets (`MeshletCullPhase1`, `MeshletCullPhase2`, `VisibilityRaster`) skip it, and every other pass treats it as an ordinary instance (row AV1 (h)) |
<!-- /assumption -->

## Stereo eye copies and the desktop mirror

<!-- elaboration -->
<!-- assumption: pending RHI (A5 row RPB27) -->
Stage 7 adds three RPROD feature functions, declared in `Internal/Passes/Flux_StereoPasses.h` and called by `Flux.BuildGraph` for an expanded stereo frame (row SP1 (c)):

| Pass | Function | Reads | Writes |
|---|---|---|---|
| `XrEyeCopy` ×2 | `Flux_AddStereoEyeCopies(xGraph, xPoses, axEyeOutputs)` | each eye view's output, an `R8G8B8A8_SRGB` renderer-owned texture at the eye's extent | the eye's imported runtime image (`Flux_XrViewPoses::m_axImages`) |
| `XrDepthCopy` ×2 (when `m_axDepthImages` are non-null) | same function | each eye view's depth target | the eye's imported runtime depth image; reversed Z is reported in `Flux_StereoSubmitInfo::m_bReversedZ` |
| `XrMirrorCopy` | `Flux_AddStereoMirror(xGraph, xLeftEyeOutput, xSwapchain)` | the left eye's output | the window swapchain, scaled to fit with letterbox bars cleared to black (`xr.mirror = leftEye`); with `none`, a clear |

The eye views render into renderer-owned outputs, never into the runtime's images, so an image is written only by its copy. The four eye copies carry `FLUX_PASS_FLAG_CONDITIONAL_SUBMIT`, which needs RHI's compile step 7 to place them in a graphics partition of their own after every other partition; `Flux.Submit` submits that partition only when `BeforeSubmit` returned true ("Stereo frame source" in `Zenith2/Docs/Design/RPROD/Renderer.md`), and otherwise discards its recorded commands, leaving each runtime image in its acquire state. The mirror never renders the scene again.
<!-- /assumption -->

## Pass ownership and files

<!-- elaboration -->
RPROD owns the composition of every frame (Z2-I-RPROD-21 to -24): the feature-function surface, the path functions and the order. Each pass's implementation belongs to the area that owns its budget row (`Zenith2/Docs/Budgets.md → "Rendering rows"`) and lives in `Zenith2/Engine/ZenithFlux/Internal/Passes/<Pass>/Flux_<Pass>.cpp`, with its program under `Zenith2/Shaders/Passes/<Feature>/`:

| Pass group | Directory (`Internal/Passes/`) | Implementing area | Row |
|---|---|---|---|
| `InstanceCullPhase1/2`, `HiZBuild`, `TransparentCull`, `ShadowInstanceCull`, `ShadowHiZBuild` | `Culling/`, `HiZ/` (bodies in `Internal/Culling/`) | RPROD | Z2-B-RPROD-005, -007, -008, -010 |
| `ForwardPlusPrepass`, `ForwardPlusOpaque` | `ForwardPlus/` | RPROD (the material and lighting calls are GEOM's and LIGHT's single-source modules) | Z2-B-RPROD-006, -009 |
| `PerObjectLightLists`, `ForwardOpaquePerObject` | `Forward/` | RPROD | Z2-B-RPROD-011, -012 |
| `DeferredDepthPrepass`, `GBuffer` | `Deferred/` | RPROD | Z2-B-RPROD-006, -014 |
| `DeferredDecals` | `Deferred/` | GEOM | Z2-B-GEOM-006 |
| `MeshletCullPhase1/2`, `VisibilityRaster`, `MaterialClassify`, `MaterialResolve`, `VelocityReconstruct` | `VisibilityBuffer/` | RPROD | Z2-B-RPROD-015 to -019 |
| `OpaqueColourCopy`, `TransparentForward` | `Transparent/` | RPROD | Z2-B-RPROD-010 |
| `Present` | `Present/` | RPROD | Z2-B-RPROD-020, -027 |
| `LightDecalListBuild`, `LightClustering`, `VolumetricFog`, `GTAO`, `ContactShadows`, `DeferredLighting`, `SSR`, `ShadowDepth`, `TemporalAA`, `AutoExposure`, `DepthOfField`, `MotionBlur`, `Bloom`, `TonemapAndGrade` | `Lights/`, `Fog/`, `GTAO/`, `Shadows/`, `DeferredLighting/`, `SSR/`, `TAA/`, `Exposure/`, `DoF/`, `MotionBlur/`, `Bloom/`, `Tonemap/`; LIGHT's subsystems (the lighting system, the shadow atlas allocator, probes, baked GI, the dynamic-resolution controllers) under `Internal/Lighting/<Subsystem>/` (LIGHT row L1, confirmed) | LIGHT | Z2-B-LIGHT-001 to -015 |
| `SkyAtmosphere` | `Sky/` | ENV | Z2-B-ENV-001 |
| `ComputeSkinning`, `MorphTargets`, `ProceduralGeometry`, `StreamingUpload` | `Skinning/`, `Procedural/`, `Streaming/` | GEOM | Z2-B-GEOM-003 to -005 |
| particle passes | `Particles/` | VFX | Z2-B-VFX-003, -004 |
| `UIComposite` | `UI/` | UI | Z2-B-UI-003 |
| `DebugOverlay` | `Debug/` | DEVT (RPROD's `HiZDebugOverlay` beside it) | Z2-B-DEVT-003 |
| `GPUSceneGrow`, `GPUSceneScatter` | `GPUScene/` (bodies in `Internal/GPUScene/`) | RHI | Z2-B-RPROD-004 |

## Pass implementation rules

- A pass implementation matches its per-pass table field for field and edits that table in the same diff when it changes ([Z2-N-REND-795], [Z2-N-REND-800]).
- A pass adds no timing code: its GPU time is the graph's timestamps (MP-GPUPASS, [Z2-N-BUDGET-251]).
- A feature function reads the view through `Flux_ViewContext` alone and shares state with another view only through `Flux_FrameShared` or a handle the graph sees ([Z2-N-REND-607]).
- Every depth-writing pass draws the opaque buckets, then the alpha-tested buckets (`Flux_BucketRegistry::GetBucketsInOrder`); every subsequent colour pass binds depth read-only with the `EQUAL` compare from `Flux_DepthModeOps::GetColourCompare` and no discard ([Z2-N-REND-722], [Z2-N-REND-917]).
- Every raster pass declares its shader set with `Programs()` ([Z2-N-REND-920]); an indirect multi-PSO pass declares every program its buckets can bind (the bucket registry's program list for its kind).
- A pass declares at most the attachments it writes and binds depth read-only when it does not write it ([Z2-N-REND-919]).
- Depth targets are created with `Flux_CreateViewDepth(xGraph, xView)` (`Flux_Features.h`), which sets the format and `m_eDepthReason` from the view's depth mode, so the D32 check has its input (`m_eDepthReason`, `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Flux_RenderGraph header"`).
- Every mesh vertex program of RPROD's passes (both prepass phases, the opaque passes, and in Stage 5 the G-buffer and visibility raster) obtains its world position through GEOM's `Zenith2/Shaders/Common/MeshVertex.slang`, so a material's optional vertex modifier (ENV row EN1, cross-doc item X127 (1): `bHAS_VERTEX_MODIFIER`, `ModifyPositionWS`) is applied in every one of them, for the current and, where velocity is written, the previous frame's time and transform; RPROD's programs add no modifier call of their own. The hook itself is in Rendering's "Materials" since X127 (1), in this form (reconciliation row RP20).

## Shared feature input types

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_Features.h (types)
struct Flux_CullPhaseOutput
{
	Flux_RGBuffer m_xDrawArgs;             // Flux_IndirectDrawIndexed × commands of the kind
	Flux_RGBuffer m_xDrawCounts;           // u32 per bucket
	Flux_RGBuffer m_xDrawRecords;          // Flux_DrawRecord × commands
	Flux_RGBuffer m_xVisibleInstances;     // u32 × candidate count
};
struct Flux_CullOutput
{
	Flux_CullPhaseOutput m_axPhase[2];
	uint32 m_uPhaseCount;                 // 1, or 2 after Flux_CombineCullOutputs
	Flux_BucketKind m_eKind;
	Flux_RGBuffer m_xDrawnBits;            // phase 1's drawn bits (phase 2 candidates)
	Flux_RGBuffer m_xCounters;             // Flux_CullCounters of the view
};
struct Flux_MeshletCullOutput { Flux_RGBuffer m_xVisibleMeshlets; Flux_RGBuffer m_xMeshletCount; Flux_RGBuffer m_xRasterArgs; Flux_RGBuffer m_xDrawnBits; uint32 m_uPhase; };
struct Flux_MaterialBins { Flux_RGBuffer m_xBinCounts; Flux_RGBuffer m_xBinTiles; Flux_RGBuffer m_xDispatchArgs; uint32 m_uVariantCount; };
struct Flux_TonemapInputs { Flux_RGTexture m_xColour; Flux_RGTexture m_xBloom; Flux_RGBuffer m_xExposure; };

Flux_CullOutput Flux_CombineCullOutputs(const Flux_CullOutput& xPhase1, const Flux_CullOutput& xPhase2);
Flux_RGTexture Flux_CreateViewDepth(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView);
void Flux_CallViewExtensions(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets, Flux_ExtensionPoint ePoint);
void Flux_BuildCommonEpilogue(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
```

Each type carries a `value` contract block (frame-scoped handles, `OWNER_THREAD`); `Flux_SkinningBatch` is GEOM's, `Flux_ParticleFrame` VFX's, `Flux_ProceduralFrame` GEOM's (`Zenith2/Docs/Design/GEOM/Procedural.md`), forward-declared where the prologue header needs them. `Flux_SSAOSettings`, the argument of Rendering's `Flux_AddSSAO`, is LIGHT's type (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`), passed from `m_pxQuality->m_xLight.m_xSSAO`; RPROD declares no second SSAO settings type, and the depth and normal inputs the pass reads come from `Flux_ViewTargets` ([Z2-N-REND-803]).

## Async-compute flags

Exactly the passes marked A in the frame tables carry `FLUX_PASS_FLAG_ASYNC_COMPUTE` ([Z2-N-REND-791]): `ComputeSkinning`, `MorphTargets`, the four particle simulation passes, `ProceduralGeometry`, `LightClustering`, `VolumetricFog`, `GTAO` and its denoise, `ContactShadows`. Every other pass runs on graphics ([Z2-N-REND-792]); `StreamingUpload` carries `FLUX_PASS_FLAG_COPY_QUEUE`. `Test_Paths_AsyncFlagsMatchFrameTables` checks every path's declared passes against this list, and the parity suite runs each scene with `m_bAsyncCompute` on and off ([Z2-N-REND-791]).

## Present pass realisation

`Flux_AddPresent` ([Z2-N-REND-790]):

| View output | Declaration |
|---|---|
| `FLUX_VIEW_OUTPUT_SWAPCHAIN` (the main view, the editor's shell view; an editor game view renders into a tool texture, `Zenith2/Docs/Design/RPROD/Extractors.md` → "Game view target") | a side-effect pass `Present` that transitions the imported swapchain image to `PRESENT` (the RHI records nothing else; the present call is `Flux.Present`'s) |
| a texture, cubemap face or other output | no pass: `MarkOutput` on the output texture, so the chain that writes it is kept ([Z2-N-REND-299]) |

The swapchain image is imported as a placeholder at graph construction and bound to the acquired image at `Flux.Submit` (RHI's stand-in rule when the acquire fails, `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Submission"` step 1). Pacing is `Zenith2/Docs/Design/RPROD/Renderer.md → "Frame pacer"`.
