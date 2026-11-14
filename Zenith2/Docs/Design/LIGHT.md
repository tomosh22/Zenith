# LIGHT design: lighting and image quality

Design document for the LIGHT area (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "LIGHT Lighting & image quality"). It elaborates `Zenith2/Docs/Rendering.md` and never contradicts it ([Z2-N-REND-001]): every pass's program, parameter struct, targets, formats, queue, capability requirements and budget row are Rendering's (`Zenith2/Docs/Rendering.md → "Per-pass specifications"`) and are cited, not restated. This family adds the algorithms, data layouts and cooked formats, the quality-tier variants, the QualityMatrix rows LIGHT supplies, the public API, and the tests and evidence. Every addition it needs to Rendering's tables is listed once, in "Rendering table additions pending X103", and waits for the core-doc batch that applies cross-doc items X103 and X148. The requirement catalogue is `Zenith2/Docs/Requirements/LIGHT.md`.

## Purpose and scope

### What LIGHT owns

| Capability | Where specified |
|---|---|
| the light model, photometric units, light keys, light record encodings beyond Rendering's three types, the render-world light delta and its validation | `Zenith2/Docs/Design/LIGHT/Lighting.md` |
| the frame light and decal list, froxel light clusters, the single-source `EvaluateLighting` and the BRDF, deferred and visibility-buffer lighting, area lights, the advanced-lobe integration | `Zenith2/Docs/Design/LIGHT/Lighting.md` |
| cascaded and local-light shadows, the shadow atlas, the one shadow-atlas allocator and shadow planner, the shadow-view record, filtering and bias, contact shadows | `Zenith2/Docs/Design/LIGHT/Shadows.md` |
| GTAO, sky irradiance, baked GI runtime evaluation and its cooked formats, SSGI | `Zenith2/Docs/Design/LIGHT/GI.md` |
| IBL, the environment cube array, reflection probes (capture, filtering, blending, residency), SSR | `Zenith2/Docs/Design/LIGHT/Reflections.md` |
| froxel fog, the atmosphere LUTs, the sky draw and aerial perspective, the star-field, cloud-layer, cloud-shadow and underwater hooks ENV feeds | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` |
| per-view temporal-history management, jitter, TAA, the temporal upscaler, the one upscaler enumeration, the dynamic-resolution controller | `Zenith2/Docs/Design/LIGHT/Temporal.md` |
| exposure, depth of field, motion blur, bloom | `Zenith2/Docs/Design/LIGHT/PostProcess.md` |
| tonemapping, grading LUTs, SDR, HDR10 and scRGB output transforms, colour-space conversion | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` |
| the meaning and values of LIGHT's quality keys per tier and LIGHT's QualityMatrix rows | `Zenith2/Docs/Design/LIGHT/QualityTiers.md` |
| the LIGHT test catalogue and the lighting validation scenes | `Zenith2/Docs/Design/LIGHT/Tests.md` |

### Explicit exclusions

| Not in LIGHT | Owner or decision |
|---|---|
| the render graph, views, view list, culling (including shadow-view culling and shadow HiZ), frame anatomy, PSOs, the quality-tier mechanism (`Flux_QualitySettings`'s shape, the tier list, the tier files and the keys' registration) | RPROD (`Zenith2/Docs/Rendering.md → "Render views"`, → "Hierarchical-Z occlusion culling"; `Zenith2/Docs/Design/RPROD/Renderer.md → "Quality settings"`) |
| GPU scene tables and their slots, the upload ring, readbacks, persistent resources, swapchain and HDR metadata plumbing | RHI (`Zenith2/Docs/Design/RHI.md → "Division of Flux code with RPROD"`) |
| material evaluation, surface models and their lobes' definitions, decal records and blend rules, textures | GEOM (`Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"`) |
| atmosphere, fog volume and time-of-day authoring, the sun and moon lights, the star cubemap, clouds and their passes, weather, water and the underwater composite, the sky GPU row | ENV (Z2-I-ENV-06 to Z2-I-ENV-09; `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"`) |
| the lighting bake (probe placement, the lightmap tracer, scheduling), world lighting authoring | WORLD (Z2-I-WORLD-06) |
| light and probe placement tools, gizmos, the grading editor | EDIT |
| the path tracer | ADVGFX (Z2-D-037); LIGHT supplies comparison scenes |
| hardware ray-traced shadows, reflections or GI | deferred to Stage 7 (Z2-D-043) |
| MSAA | excluded (Z2-D-035) |
| tile-local merging of lighting passes | not before D-TBDR (open, Stage 5) (Z2-D-036) |
| split-screen lighting | excluded (Z2-D-076) |
| stereo rendering of LIGHT passes | deferred to Stage 7 (Z2-D-072); feature signatures stay per view, so they are XR-ready |

### Inventory items covered

Z2-I-LIGHT-01 directional-light shadows (S3), Z2-I-LIGHT-02 local-light shadows (S3), Z2-I-LIGHT-03 probes (S4), Z2-I-LIGHT-04 baked GI (S5), Z2-I-LIGHT-05 dynamic GI (S5; its platform realisation is ruled by Q41 (a), `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"`), Z2-I-LIGHT-06 reflections (S3), Z2-I-LIGHT-07 volumetrics (S3), Z2-I-LIGHT-08 temporal-history management (S2), Z2-I-LIGHT-09 antialiasing (S3), Z2-I-LIGHT-10 upscaling (S3), Z2-I-LIGHT-11 dynamic resolution (S3), Z2-I-LIGHT-12 exposure (S3), Z2-I-LIGHT-13 colour management (S3), Z2-I-LIGHT-14 HDR (S5), Z2-I-LIGHT-15 motion blur (S5), Z2-I-LIGHT-16 depth of field (S5). The map is `Zenith2/Docs/Requirements/LIGHT.md → "Coverage of inventory items"`.

### Document family

| File | Headings |
|---|---|
| `Zenith2/Docs/Design/LIGHT.md` | the fifteen skeleton sections |
| `Zenith2/Docs/Design/LIGHT/Lighting.md` | Scope of this file · Light units · Light record encodings · Light extraction and the CPU light mirror · Light and decal list build · Light clustering · The EvaluateLighting contract · BRDF · Light attenuation and shape · Lighting channels · Advanced surface lobe integration · Deferred lighting pass · Lighting lookup tables · Flux_Lights header |
| `Zenith2/Docs/Design/LIGHT/Shadows.md` | Scope of this file · Shadow atlas · Shadowed-light budget · Shadow planning in view-list construction · Atlas layout · Atlas tile sizing · Atlas allocation · Cascaded shadow maps · Spot shadow views · Point shadow views · Area light shadow views · Shadow view record · Shadow depth rendering · Filtering · Bias · Shadow sampling in shaders · Contact shadows · Flux_ShadowAtlas header · Flux_Shadows header |
| `Zenith2/Docs/Design/LIGHT/GI.md` | Scope of this file · GTAO · GTAO denoise and upsample · Sky irradiance · Irradiance volume cooked format · Lightmap set cooked format · Bake output contract · Irradiance volume sampling · Lightmap evaluation · Lightmap binding · Lightmaps on the deferred paths · Baked GI residency and streaming · GI GPU records · SSGI · SSGI error budget · Indirect diffuse selection · Flux_GlobalIllumination header |
| `Zenith2/Docs/Design/LIGHT/Reflections.md` | Scope of this file · Environment cube array · IBL evaluation · Static environment · Sky capture · Reflection probe record · Reflection probe cooked format · Probe capture · Probe filtering · Probe blending · Probe residency and streaming · Probe fallback chain · SSR ray generation · SSR hierarchical trace · SSR hit validation · SSR denoise · SSR composite · Flux_Reflections header |
| `Zenith2/Docs/Design/LIGHT/Volumetrics.md` | Scope of this file · Atmosphere ownership · Media model · Froxel grid · Scattering injection · Temporal reprojection · Integration · Fog application · Transmittance LUT · Multiple-scattering LUT · Sky-view LUT · Sky draw and aerial perspective · Star field · Cloud layer composite · Sun transmittance · Cloud shadow on the sun · Underwater medium override · Local fog volumes · Flux_Atmosphere header |
| `Zenith2/Docs/Design/LIGHT/Temporal.md` | Scope of this file · Histories LIGHT owns · The first-write rule for histories · History invalidation · Dynamic resolution and history validity · Jitter sequence · TAA resolve · Temporal upscale · Upscaler selection · Dynamic-resolution controller · Thermal and pressure inputs · Dynamic-resolution telemetry · Flux_Temporal header · Flux_Upscaler header |
| `Zenith2/Docs/Design/LIGHT/PostProcess.md` | Scope of this file · Exposure units · Pre-exposure pipeline · Exposure buffer · Luminance histogram · Metering · Adaptation · Camera exposure · Depth of field · Motion blur · Bloom · Flux_PostProcess header |
| `Zenith2/Docs/Design/LIGHT/ColourManagement.md` | Scope of this file · Working colour space · SDR tonemap operator · HDR tonemap operator · Grading LUT · Grading LUT cook · Output transforms · Output mode selection · HDR calibration and metadata · Vignette and film grain · UI and HDR · Colour-space matrices · Tonemap parameters · Flux_ColourManagement header |
| `Zenith2/Docs/Design/LIGHT/QualityTiers.md` | Scope of this file · Tier list · Tier values and configuration · Shadow tiers · Volumetric tiers · Screen-space tiers · Reflection and GI tiers · Temporal and resolution tiers · Post-processing and output tiers · Fallbacks and triggers · QualityMatrix rows supplied by LIGHT |
| `Zenith2/Docs/Design/LIGHT/Tests.md` | Scope of this file · Test targets and gate steps · Light model unit tests · Shadow unit tests · Temporal unit tests · Exposure and post unit tests · Colour unit tests · GI and probe unit tests · Configuration and component unit tests · Format and fuzz tests · Cook and bake tests · Render tests · ENV hook render tests · Parity scenes · Analytical BRDF tests · Path-tracer comparison scenes · Approximation error scenes · Temporal sequence tests · Diagnostics tests · Failure and stress tests · Device runs |

### Conventions

Zenith Hungarian (Z2-D-034) and Rendering's conventions ([Z2-N-REND-012] to [Z2-N-REND-014]): sizes in bytes, distances in metres, angles in radians (degrees in authoring fields only), colours linear Rec.709, light quantities in the units of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"`. Design obligations here are plain prose; each is traceable to a `Z2-R-LIGHT-NNN` requirement. Elaborations beyond the specification are marked `<!-- elaboration -->`. Assumptions about documents still in review are fenced `<!-- assumption: pending <AREA>[, <X row>] -->` … `<!-- /assumption -->` and listed in "Assumed types from pending design documents", each with its row (L1 … L38) in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` and, where the assumption changes a core document, the cross-doc item (X80, X103 to X108, X125, X148) of `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md` that carries it; an orchestrator ruling that settles a seam between design documents is cited by the finding it answers. Under the core-doc freeze rule no LIGHT ticket edits a core document: a ticket whose work depends on a pending X row cites it and is blocked until the orchestrator marks it applied. Two identifier families are kept apart: `L<n>` names a cross-area row, `LR<n>` a design choice of this family ("Design choices this family makes").

## Dependencies

### Libraries LIGHT code depends on

LIGHT adds no library. Its runtime code lives in `ZenithFlux` (layer 5, dependencies owned by `Zenith2/Docs/Architecture.md → "Library dependency graph"`: ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI, ZenithVertexFormat, ZenithAssets, ZenithShadersGenerated), its shaders under `Zenith2/Shaders/Lighting/` and `Zenith2/Shaders/Passes/`, its cook steps in `ZenithCook`, the reflection registration of its component structs in `ZenithRuntime` (which already depends on ZenithReflect and ZenithFlux), and its comparison tests in `ZenithFlux_Tests` with `TEST_DEPENDS ZenithPathTracer`.

| Dependency | Used for |
|---|---|
| ZenithCore | maths (`Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`, → "Units", → "Coordinate systems", → "Zenith_Colour", → "Zenith_Hash64"), containers (`Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Array"`, → "Zenith_BoundedQueue"), configuration (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Configuration store"`), clocks (→ "Clocks") |
| ZenithRHI | formats and capabilities (`Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`) |
| ZenithAssets | asset loaders for probes, environments, LUTs and baked GI (assumption pending ASSET, row L11); `Zenith_AssetHandle<T>` in the component structs |
| ZenithCompression | through ZenithAssets' section decoding (`Zenith2/Docs/Design/COMPRESS.md → "How cooked formats embed the descriptor"`) |
| ZenithReflect (from ZenithRuntime only) | `Zenith_TypeRegistry::BeginComponent` for the lighting component structs (`Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"`) |

### Headings this design relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Rendering.md → "Render views"`, `→ "Hierarchical-Z occlusion culling"`, `→ "Shading paths and capability matrix"`, `→ "Validation roles"`, `→ "Per-pass specifications"`, `→ "Frame anatomy common epilogue"` | the specification LIGHT elaborates |
| `Zenith2/Docs/Rendering/PerPass/Common.md → "Shared lighting passes"`, `→ "Shared late passes"`, `→ "Epilogue passes"`, `→ "Shadow passes"`, `→ "Prologue passes"` | every LIGHT pass table |
| `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md → "Forward+ passes"`, `Zenith2/Docs/Rendering/PerPass/Forward.md → "Forward passes"`, `Zenith2/Docs/Rendering/PerPass/Deferred.md → "Deferred passes"` | the opaque passes that call `EvaluateLighting` |
| `Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tables"`, `→ "Readbacks"`, `→ "Persistent resources"`, `→ "GPU memory rules"`, `→ "Swapchain recreation"` | records, readbacks, resources, shedding |
| `Zenith2/Docs/Rendering/FrameAnatomy.md → "Async compute placement"` | async eligibility |
| `Zenith2/Docs/Budgets.md → "Rendering rows"`, `→ "Frame-rate definitions and measurement procedure"`, `→ "Production-quality acceptance"` | budgets, the dynamic-resolution floor, QualityMatrix |
| `Zenith2/Docs/Architecture.md → "Memory governor"`, `→ "Reflection and runtime type registration"`, `Zenith2/Docs/Architecture/LifecycleCommands.md → "Teleport"`, `Zenith2/Docs/Architecture/DataDefinedComponents.md → "The schema file"` | shedding, component registration, teleport, the schema alternative of X108 |
| `Zenith2/Docs/Design/PLAT/Android.md → "Applying thermal actions"`, `Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier changes"`, `→ "Registration API"`, `→ "Key grammar"` | thermal and tier inputs, key registration |
| `Zenith2/Docs/Design/RPROD/Renderer.md → "Quality settings"`, `→ "Quality tier selection"`, `→ "Render world consumption"`, `→ "Render frame sequence"`, `Zenith2/Docs/Design/RPROD/Views.md → "Shadowed-light selection"`, `→ "View list construction"`, `Zenith2/Docs/Design/RPROD/Extractors.md → "Camera extractor"` | the quality struct LIGHT's aggregate lives in, the frame's consumption order and build-node steps, the shadow seam, the view identity ENV's per-view records are keyed by |
| `Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`, `Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"`, `Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application"` | cooked formats, lightmap pages, decals |
| `Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`, `→ "Readback pool"`, `→ "Persistent resource creation and the acquire list"`, `→ "Memory pressure responder"` | light slots and staging, readbacks, uploads, the pressure state |
| `Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`, `→ "Frame deferral"`, `→ "Frame failures"`, `→ "Upload request state machine"` | when LIGHT's per-frame data is declared and written, and what a frame that submits nothing leaves behind ("Build-node upload data", "Deferred and failed frames"; row L35) |
| `Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"` | the `Common/` modules that declare LIGHT's `[FluxShared]` structs |
| `Zenith2/Docs/Design/ENV/Atmosphere.md → "Night sky"`, `→ "Sun and moon illuminance"`, `→ "Cloud layer pass"`, `→ "Cloud shadows"`, `→ "Blocks ENV writes"`, `Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater medium"`, `→ "Submersion test"`, `Zenith2/Docs/Design/ENV.md → "Render-world records"` | the ENV hooks LIGHT provides; the `Flux_RenderEnvironmentView` records LIGHT consumes |

### Upward-callback interfaces

LIGHT declares no upward interface: the runtime pushes thermal caps into `Flux_DynamicResolutionController::SetThermalCap` (a downward call from ZenithRuntime, which depends on ZenithFlux, so the callback direction of `Zenith2/Docs/Contracts.md → "Layering DAG"` holds), asset completions arrive through the loader interface ZenithAssets declares, memory pressure arrives through RHI's `Flux_MemoryConsumer` state, which RPROD's pressure hook turns into `SetPressureFloor` calls, and ZenithRuntime registers the lighting component structs by calling downward into their declarations.

### Assumed types from pending design documents

<!-- assumption: pending RPROD, RHI, ASSET, ECS, GAME, ENV, WORLD, UI, MOD, PLAT, BUDGET -->
| Row | Area | Cross-doc item | Assumption | Where used |
|---|---|---|---|---|
| L1 | RPROD | — | LIGHT passes live in `Zenith2/Engine/ZenithFlux/Internal/Passes/<Pass>/` and LIGHT subsystems in `Internal/Lighting/<Subsystem>/` | "Library and file layout" |
| L2 | RPROD | X103 (b) | `Flux_QualitySettings::m_xLight` (`Flux_LightQualitySettings`), `Flux_RenderEnvironment::m_xLighting` (`Flux_RenderLightingEnvironment`), `Flux_ViewRequest::m_xCameraLighting` (`Flux_CameraLightingSettings`), `Flux_FrameShared::m_pxLighting` and `m_xShadowViewRecords` | "Flux_LightSettings header" |
| L3 | RPROD | X103 (c) | the five added `Flux_LightingInput` members, `Flux_LightingOutput`, `EvaluateLightingEx`, the probe members of `Flux_LightSource` | `Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"` |
| L4 | RPROD | X103 (d) | lighting channels in bits 24–31 of the instance record's `m_uLayerMask` | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Lighting channels"` |
| L5 | RPROD | X103 (e) | shadow-atlas and shadow-view fields `[Optional]` in the consumer tables | `Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow atlas"` |
| L6 | RPROD | X103 (e) | `Flux_ShadowViewRecord`'s 96-byte layout in the shared-struct documentation; the depth-bias entry `FLUX_DEPTH_BIAS_SHADOW` = (0, 2.0, 0) (RPROD row RP19) | `Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow view record"` |
| L7 | RPROD | X103 (a) | `m_axFogScattering[2]` in `Flux_ViewPersistentState` | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Temporal reprojection"` |
| L8 | ENV | — (superseded by L31) | ENV's components fill the atmosphere block of `Flux_RenderLightingEnvironment`; LIGHT's defaults otherwise | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"` |
| L9 | RPROD | X103 (f) | the `multiScatter` step, `m_xMultiScatterLUT`, `Flux_AddSkyLUTs` as the shared producer, persistent transmittance and MS LUTs | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Multiple-scattering LUT"` |
| L10 | RPROD | X103 (i) | Stage 4 retyping of `m_xIBLSpecular` and `m_xIBLIrradiance` to graph SRVs, `m_xProbeRecords`, the `Common/Probes.slang` module row | `Zenith2/Docs/Design/LIGHT/Reflections.md → "Environment cube array"` |
| L11 | ASSET | — | asset kinds `Flux.Environment`, `Flux.Probe`, `Flux.IrradianceVolume`, `Flux.LightmapSet`, `Flux.GradingLUT`, `Flux.LightingLUT` with the loader interface GEOM also assumes | "Construction and shutdown" |
| L12 | RPROD | X103 (h) | the second forward colour target `m_xSpecularWeight`, attached only with `FLUX_VIEW_FEATURE_SSR` (`bPERM_SSR_WEIGHT`), and `Flux_ViewTargets::m_xSpecularWeight` | `Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR composite"` |
| L13 | WORLD, ASSET | — | `Cook_Error`, `Cook_OutputSink`, the cook step interface and `Cook_LightmapBinding` | `Zenith2/Docs/Design/LIGHT/GI.md → "Bake output contract"` |
| L14 | RPROD, GEOM | X106 (ruling requested), LIGHT-1 | the fifth GPU-scene table (instance lighting) and the `FLUX_SURFACE_MODEL_FLAG_BAKED_DIFFUSE` bit; the refused-ruling binding of row L32 | `Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap binding"` |
| L15 | RPROD | X105 | the SSGI pass in the frame anatomy and `m_axSSGIHistory[2]` | `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI"` |
| L16 | RPROD | X103 (g) | previous-history fields `[Optional]` | `Zenith2/Docs/Design/LIGHT/Temporal.md → "The first-write rule for histories"` |
| L17 | GAME, RPROD | — | the camera extractor sets `m_bCameraCut` for a teleported camera | `Zenith2/Docs/Design/LIGHT/Temporal.md → "History invalidation"` |
| L18 | RPROD | X104 | [Z2-N-REND-599]'s render-extent clause narrowed to a history's own extent | `Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic resolution and history validity"` |
| L19 | DecisionLog, Architecture | X107 | D-SEL-UPSCALER (open, Stage 5) registered | `Zenith2/Docs/Design/LIGHT/Temporal.md → "Upscaler selection"` |
| L20 | RPROD | X103 (j) | the plain-data fields of `Flux_TonemapParams` and the exposure passes | `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Tonemap parameters"` |
| L21 | RHI | — | `Flux_HDRMetadata` applied by the swapchain manager | `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "HDR calibration and metadata"` |
| L22 | UI | — | UI programs encode through `Flux_EncodeOutput` | `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "UI and HDR"` |
| L23 | RPROD | — | the tier IDs of `Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Tier list"` | every tier table |
| L24 | ECS, MOD | X108 (ruled (b) by the orchestrator, Q42) | withdrawn: engine-content schemas are not used; the lighting components are C++-registered structs ("Light component schemas") | "Light component schemas" |
| L25 | BUDGET | — | seven pending LIGHT rows and the per-scene parity tolerance proposals | "Pending row needs" |
| L26 | PLAT | — | answers P16: `SetThermalCap` is the thermal entry | `Zenith2/Docs/Design/LIGHT/Temporal.md → "Thermal and pressure inputs"` |
| L27 | GEOM | X80 | answers GM9: the four lobes, GB4 decode, decal order, the sheen LUT | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Advanced surface lobe integration"` |
| L28 | RPROD | X103 (b) | answers RP19: `m_xLight` holds every LIGHT-meaning quality field (the dynamic-resolution range included) under `render.quality.*` keys, `clusterGrid`, `shadowAtlasSize` and `froxelGrid` withdrawn (the grid is the three INT keys `froxelGridX`, `froxelGridY`, `froxelGridZ`), one `Flux_UpscalerMode`, `.config.json` tier files, the controller as the only writer of the scale; the pressure floor is RPROD's path (`Zenith2/Docs/Design/RPROD/Renderer.md → "Quality tier selection"` step 4 calls `SetPressureFloor` on every view for which `Flux_LightingSystem::HasDynamicResolution` is true). For RPROD revision 2: `Renderer.md` "Quality settings" drops "output, light clusters" from `m_xLight` (output settings are `Flux_OutputSettings`, the cluster grid is a constant) and its "Stages 2 to 4" froxel value for `QUALITY_TIER_WIN_DEFAULT` becomes 128 × 72 × 48 | "Flux_LightSettings header" |
| L29 | RPROD | — | answers RP7 and RP18: `Flux_BuildViewList` calls `Flux_LightingSystem::PlanShadows` through the registry's `GetLightingSystem()`; one packer (the buddy quadtree), one counter (`Flux.Shadows.Dropped`), one tie-break (`Flux_RenderLightKey`); `Flux_AtlasTile`, `Flux_ShadowViewKind`, `Flux_ShadowViewPlan` and `Flux_RenderLightKey` are declared in the leaf header `Flux_ShadowAtlas.h` (ZenithCore and `Flux_LightingErrors.h` only), so `Flux_View.h` and `Flux_ViewContext.h` include it for `Flux_ViewOutputs::m_xAtlasTile` and `Flux_ViewList::m_axShadowPlans` without a cycle. For RPROD revision 2: `Views.md` "Flux_AtlasTile include" names `Flux_ShadowAtlas.h` (not `Flux_LightSettings.h` or the "Flux_Shadows header"), and its step 4 says the tiles reach the light records through `Flux_GPUScene::StageLightRecord` from `PlanShadows`, not through `ApplyDeltas` input | `Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow planning in view-list construction"` |
| L30 | RHI | — | `Flux_GPUScene` calls `Flux_ValidateLightDelta` and `Flux_EncodeLightRecord`, and adds `FindLightSlot`, `StageLightRecord` and `ReserveLightSlots` (slots 0 and 1 for the sun and moon, Stage 3, Z2-D-131, the Q45 ruling core-doc batch 4 applied); a full-state resync (discard and re-allocate every slot) keeps the reserved slots reserved and never hands one to a re-added delta | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Division of the delta work with the GPU scene"` |
| L31 | ENV | X125 | answers EN5 and EN6 under X125 (RPROD row RP17): the atmosphere and world-lighting blocks filled by the one environment extractor, the star-field, cloud-layer and cloud-shadow hooks carried in ENV's `Flux_EnvironmentBlocks` under LIGHT's types; the underwater override is not in that resource (row L36); any `m_uSkyRevision` change (atmosphere, fog or stars) rebuilds the LUTs; the sun and moon slots as directional entries 0 and 1, whose role is the slot position (no flag names a sun or a moon; bits 5–7 stay reserved), written by ENV with the slot flag contract of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"` (sun: casts-shadow, contact shadows and volumetric; moon: volumetric, and casts-shadow above 1 lux), which ENV's writer must set (row L39); `Flux_MakeNamedLightKey` | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"` |
| L32 | GEOM, RHI | X106 (if refused) | the per-material-slot lightmap binding: the extended material-assignment key and the 16-byte constants extension | `Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap binding"` |
| L33 | MOD | X108 | `ZenithRuntime` files `Internal/Zenith_LightingTypes.h/.cpp` and the call of `Zenith_RegisterLightingComponentTypes` in startup step 6; answers M18 for the directional light's stable name | "Light component schemas" |
| L34 | BUDGET | — | the `QUALITY_TIER_S24_HIGH` value of `Z2-B-LIGHT-007` includes the specular-weight target's store | "Pending row needs" |
| L35 | RHI, RPROD | X129 | answers R18: every LIGHT build-node producer of per-frame upload data declares its bytes at setup (`Flux_UploadRing::DeclareFrameData`) and writes them through `GetFrameData` at order 5 of RHI's build-node order; each consumer field is the frame chunk of `Flux_UploadRing::ImportFrameChunk` plus a plain-data element base of its ticket (row L42); RPROD's `Flux.BuildGraph` step 11 calls `Flux_LightingSystem::WriteFrameData()` (no ring parameter: it uses the ring `Create` borrowed) after `ExtendReservation` returned `FITS` or `EXTENDED`, and `Flux_LightingSystem::EndFrame(Flux_FrameLifetime::GetFrameOutcome() == FLUX_FRAME_OUTCOME_SUBMITTED)` as the node's last LIGHT call on every path | "Build-node upload data", "Deferred and failed frames" |
| L36 | ENV, RPROD | the orchestrator's ruling on LIGHT round-2 finding 4, as refined (RPROD row RP25 (b), (d)) | the underwater medium is ENV's per-view extraction record `Flux_RenderEnvironmentView` (`Zenith2/Docs/Design/ENV.md → "Render-world records"`), written by ENV's extractor per view request and keyed by the `Flux_ViewID` the camera extractor's request carries (`Zenith2/Docs/Design/RPROD/Extractors.md → "Camera extractor"`); RPROD's camera extractor cannot fill it, because ZenithRuntime has no edge to ZenithEnvironment. LIGHT consumes the records on the render side into `Flux_LightingFrame::m_axViewMediumOverrides` (at most 16 `Flux_ViewMediumOverride` per frame across worlds; ENV's revision-2 record is `{ m_xMedium, m_ulWaterBodyKey, m_bWaterline }`, written only for a submerged view and capped at 16 per world, and LIGHT copies `m_xMedium` whole); neither ENV's blocks resource nor `Flux_AtmosphereBlock` carries an override, and the ENV and RPROD sites that still name `Flux_AtmosphereBlock::m_axViewMediumOverrides` are listed in rows L39 and L40. Also: `Flux_BuildViewList` copies `Flux_ViewRequest::m_xCameraLighting` into `Flux_View::m_xCameraLighting` | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Underwater medium override"` |
| L37 | PLAT, MOD, BUDGET | — | superseded by row L41 (the orchestrator's ruling (b) on LIGHT's round-3 finding 11): no workload layer is added; a budget run's graphics settings reach the store through PLAT's existing command-line layer, and the budget-run mode's side of that is an owner gap routed by the orchestrator | "Configuration keys" |
| L38 | RPROD, RHI, ENV | — | the revision-2 amendments of L28 to L31 in the notes file (the text of those four rows above is already amended): the froxel-grid keys, RPROD's pressure path and its drift items, the plan and key in the leaf header, the slots (now Stage 3, Z2-D-131, the Q45 ruling core-doc batch 4 applied) kept across a full-state resync, the slot flags (as amended by row L39: no role flags, and the slot flag contract ENV writes), the any-revision LUT rebuild | the rows L28 to L31 |
| L39 | ENV | — | the post-round amendments of L31 and L36 for ENV: (a) the slot flag contract: ENV's revision-2 "every other bit zero" (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Sun and moon illuminance"`, `Test_Atmosphere_SunMoonSlots`) must admit `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` and `FLUX_LIGHT_FLAG_VOLUMETRIC` on the sun and `FLUX_LIGHT_FLAG_VOLUMETRIC` on the moon, beside the casts-shadow bits ENV already writes, or the sun loses its Stage 3 contact shadows and both bodies their fog; bits 5–7 stay zero; (b) the underwater destination is `Flux_LightingFrame::m_axViewMediumOverrides`: `Zenith2/Docs/Design/ENV.md` (the reconciliation row "the underwater medium" and the comment of `uFLUX_ENV_MAX_SUBMERGED_VIEWS`), `Zenith2/Docs/Design/ENV/Atmosphere.md → "Blocks ENV writes"` and `Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"` still name `Flux_AtmosphereBlock::m_axViewMediumOverrides`, a member LIGHT removed; ENV's cap stays 16 per world and LIGHT's is 16 per frame; (c) the slots, `Flux_ValidateEnvironmentLight` and Z2-R-LIGHT-147 are Stage 3 (ENV round-3 finding 18 under Q45, Z2-D-131) | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`, `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Underwater medium override"` |
| L40 | RPROD | — | the post-round items for RPROD: (a) `Zenith2/Docs/Design/RPROD/Views.md → "View list construction"` step 4 forms `m_xJitteredProjection` from `xRegistry.GetLightingSystem().GetJitterOffsetPixels(xID, output / render)` for a jittered view with `FLUX_VIEW_FEATURE_TEMPORAL_AA` (the projection unchanged otherwise); (b) `Flux.BuildGraph` step 11 calls `WriteFrameData()` without the ring; (c) `Zenith2/Docs/Rendering.md → "Render environment"` (row "underwater medium") and `Zenith2/Docs/Design/RPROD/Extractors.md → "Environment extractor"` step 4 still name `Flux_AtmosphereBlock::m_axViewMediumOverrides`: the destination is `Flux_LightingFrame::m_axViewMediumOverrides`, and the extractor has no span to leave empty; (d) answers RP35: property 4 `IntensityUnit` is the unit of every point, spot, rect and disc light (0 the type's stored unit, 1 lumens), and the rect light registers no property 19, so the rect row of `Zenith2/Docs/Design/RPROD/Extractors.md → "Light conversions"` reads property 4 | `Zenith2/Docs/Design/LIGHT/Temporal.md → "Jitter sequence"`, "Light component schemas" |
| L41 | PLAT, MOD, BUDGET | — | the orchestrator's ruling (b) on LIGHT's round-3 finding 11, superseding L37: the Player's budget-run mode (`--budget-run`, [Z2-N-BUDGET-105]; the options of `Zenith2/Docs/Design/MOD.md → "Zenith_CommandLine.h"`) configures a workload's graphics settings ([Z2-N-BUDGET-122]) through PLAT's existing command-line layer (`Zenith2/Docs/Design/PLAT/Configuration.md → "Command-line layer"`): the workload runner passes every `render.quality.*` value of the workload definition as a `--set`, so the command-line layer, above the user layer, fixes every LIGHT quality value. LIGHT's part is `Test_LightConfig_CommandLineLayerDeterminesQuality`; that the runner passes every key and that the budget-run mode admits no other `--set` of a `render.quality.*` key is an owner gap (candidate owner: PLAT's Player budget-run mode with BUDGET's [Z2-N-BUDGET-122]), routed by the orchestrator | "Configuration keys" |
| L42 | RHI | X129 | the consumer binding of LIGHT's frame data: `Flux_LightingSystem::AddSharedPasses` imports the frame chunk once per frame through `Flux_UploadRing::ImportFrameChunk(xGraph)` into `Flux_LightingFrame::m_xFrameChunk`, every LIGHT consumer field of a frame-data range binds that buffer, and a plain-data `uint32` element base ("Build-node upload data") locates the range; LIGHT assumes that a second `ImportFrameChunk` call in one frame returns the same `Flux_RGBuffer`, so LIGHT's, RHI's and RPROD's imports name one graph buffer (Rendering's once-per-frame chunk import, X129) | "Build-node upload data" |
<!-- /assumption -->

### Rendering table additions pending X103

<!-- assumption: pending RPROD, X103, X148 -->
Every field, target, permutation, shared struct and pass table LIGHT's algorithms read or write beyond Rendering's per-pass and struct declarations, in one table: the payload of cross-doc items X103 and X148 (X148 makes this whole table the payload, including every row marked "beyond X103", and supersedes X103's letter text where the two differ; letters are X103's items). Rendering applies it in the next core-doc batch under the freeze rule ([Z2-N-REND-800] binds the pass tables, [Z2-N-REND-136] the shared-struct module table); until then each LIGHT ticket that needs a row cites X103, X148 and this table and is blocked by the batch. Plain data is `—` in the access and stage columns; stages are `C` compute, `V` vertex, `P` pixel. "Every lighting pass" is ForwardPlusOpaque, ForwardOpaquePerObject, DeferredLighting and TransparentForward: the passes that call `EvaluateLightingEx`.

| Target struct or pass | Field or element | C++ type | Access | Stages | Attributes | Item | Stage |
|---|---|---|---|---|---|---|---|
| `Flux_ViewPersistentState` | `m_axFogScattering[2]` | `Flux_TextureHandle` (RGBA16F 3D, froxel grid) | — | — | ping-pong | (a) | S3 |
| `Flux_ViewPersistentState` | `m_axSSGIHistory[2]` | `Flux_TextureHandle` (RGBA16F, half render extent) | — | — | ping-pong | (a) | S5 |
| `Flux_ViewContext` | `m_xPrevVelocity`, `m_xPrevFogScattering`, `m_xPrevSSGI` | `Flux_RGTexture` | — | — | imported previous halves, unset when invalid | beyond X103 (a) | S3, S5 |
| `Flux_View` | `m_xCameraLighting` | `Flux_CameraLightingSettings` | — | — | copied from the view's request by `Flux_BuildViewList`, so feature functions read it through `Flux_ViewContext::m_pxView` (row L36) | beyond X103 (b) | S3 |
| `Flux_ViewRequest` | `m_xCameraLighting` | `Flux_CameraLightingSettings` | — | — | — | (b) | S3 |
| `Flux_FrameShared` | `m_pxLighting` | `const Flux_LightingFrame*` | — | — | — | (b) | S3 |
| `Flux_FrameShared` | `m_xShadowViewRecords` | `Flux_RGBuffer` | — | — | the frame chunk `Flux_LightingFrame::m_xFrameChunk` (`ImportFrameChunk`, X129); the records are declared at setup, written at order 5 and located by `m_uShadowViewBase` ("Build-node upload data") | (b) | S3 |
| `Flux_FrameShared` | `m_xMultiScatterLUT` | `Flux_RGTexture` (32 × 32 RGBA16F, persistent-imported) | — | — | — | (b), (f) | S3 |
| `Flux_FrameShared` | `m_xCloudLayer`, `m_xCloudTransmittance` | `Flux_RGTexture` (192 × 108 R11G11B10F, R8_UNORM) | — | — | set by ENV's `Env_AddCloudLayer`, else unset | beyond X103 (ENV hook) | S4 |
| `Flux_FrameShared` | `m_xCloudShadowMap` | `Flux_RGTexture` (512² R8_UNORM) | — | — | set by ENV's `Env_AddCloudShadowMap`, else unset | beyond X103 (ENV hook) | S5 |
| `Flux_FrameShared` | `m_xIBLCubeArray`, `m_xIBLIrradiance` | `Flux_RGTexture`, `Flux_RGBuffer` (persistent-imported) | — | — | marked output on frames that write them | (i) | S4 |
| `Flux_FrameShared` | `m_xProbeRecords` | `Flux_RGBuffer` | — | — | upload ring, rebuilt every frame from the resident probes (`Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe record"`) | (i) | S4 |
| `Flux_ViewTargets` | `m_xSpecularWeight` | `Flux_RGTexture` (R11G11B10F, render extent) | — | — | unset without `FLUX_VIEW_FEATURE_SSR` | (b), (h) | S3 |
| `Flux_QualitySettings` | `m_xLight` | `Flux_LightQualitySettings` | — | — | replaces the LIGHT-meaning flat fields (row L28) | (b) | S3 |
| `Flux_RenderEnvironment` | `m_xLighting` | `Flux_RenderLightingEnvironment` | — | — | the two blocks of row L31 | (b) | S3 |
| `Flux_LightingInput` (Slang) | `m_xGeometricNormalWS`, `m_xIndirectIrradiance`, `m_uLightingChannels`, `m_uFrameIndex`, `m_fPreExposure` | `float3`, `half3`, `uint`, `uint`, `float` | — | — | — | (c) | S3 |
| Rendering.md "Single-source lighting" | `Flux_LightingOutput`, `EvaluateLightingEx`, `Flux_DirectionalLight`, `Flux_LightingAtmosphere`; `Flux_LightSource::GetDirectionalCount`, `GetDirectional`, `GetAtmosphere`, `GetProbeCount`, `GetProbe` | Slang declarations of `Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"` | — | — | — | (c), X148 (3) | S3 (probes S4) |
| `Flux_InstanceRecord` | `m_uLayerMask` bit assignment | bits 0–23 render layers, 24–31 lighting channels | — | — | — | (d) | S3 |
| Rendering/ShaderSystem.md "Shared GPU structs", module `Common/GPUScene.slang` | `Flux_ShadowViewRecord` (96 B); `Flux_InstanceLightingRecord` (16 B, only if X106 is accepted) | layouts of `Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow view record"` and `Zenith2/Docs/Design/LIGHT/GI.md → "Binding if X106 is accepted"` | — | — | `[FluxShared]` | (e), X106 | S3, S5 |
| Rendering/ShaderSystem.md "Shared GPU structs", new module `Common/Probes.slang` | `Flux_ReflectionProbeRecord` (64 B) | layout of `Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe record"` | — | — | `[FluxShared]` | (i) | S4 |
| Rendering/ShaderSystem.md "Shared GPU structs", new module `Common/Media.slang` | `Flux_AtmosphereGPU` (64 B), `Flux_FogMediumGPU` (48 B), `Flux_MediumOverrideGPU` (48 B), `Flux_LocalFogVolumeGPU` (112 B) | layouts of `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "GPU parameter records"` | — | — | `[FluxShared]` | X148 (3) | S3 (volumes S5) |
| Rendering/ShaderSystem.md "Shared GPU structs", new module `Common/GI.slang` | `Flux_IrradianceVolumeRecord` (64 B), `Flux_LightmapPageRecord` (16 B) | layouts of `Zenith2/Docs/Design/LIGHT/GI.md → "GI GPU records"` | — | — | `[FluxShared]` | beyond X103 | S5 |
| RPROD/PSO.md "Fixed state tables" | depth-bias entry `FLUX_DEPTH_BIAS_SHADOW` | constant 0, slope 2.0, clamp 0 | — | — | — | (e), X148 (2) | S3 |
| every lighting pass, `VolumetricFog` | `m_xLightList` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (the frame light list; step 3 of "Evaluation order" reads words 1–3) | read | P or C | — | X148 (3) | S3 |
| every lighting pass, `VolumetricFog` | `m_xTransmittanceLUT` | `Zenith_RHISRV<Zenith_RHITex2D>` (`Flux_FrameShared::m_xTransmittanceLUT`) | read | P or C | — | X148 (3) | S3 |
| every lighting pass, `VolumetricFog` | data `Flux_AtmosphereGPU m_xAtmosphere` (planet and atmosphere geometry, Rayleigh, Mie and ozone media, enabled flag) | `[FluxShared]` struct by value | — | — | — | X148 (3) | S3 |
| every lighting pass, `VolumetricFog` | data `Zenith_RHIFloat4 m_xCloudShadowMapping` (origin x, origin z, 1 / extent or 0 without a map, outside transmittance), `uint32 m_uFrameIndex` | as named | — | — | — | beyond X103 | S3 (the map S5) |
| `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `TransparentForward`, `VolumetricFog` | `m_xShadowAtlas`, `m_xShadowViews` | as ForwardPlusOpaque's | read | P or C | `[Optional]` | (e) | S3 |
| every lighting pass, `VolumetricFog` | `m_xCloudShadowMap` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | P or C | `[Optional]` | beyond X103 (ENV hook) | S5 |
| `ForwardPlusOpaque`, `ForwardOpaquePerObject` | second colour target `m_xSpecularWeight` and permutation `bPERM_SSR_WEIGHT` {false, true} | R11G11B10F, `CLEAR` 0, `STORE`; not declared with `false` | write | P | — | (h) | S3 |
| every lighting pass | `m_xIBLSpecular`, `m_xIBLIrradiance` retyped; `m_xProbeRecords` | `Zenith_RHISRV<Zenith_RHITexCubeArray>`, `Zenith_RHISRV<Zenith_RHIByteBuffer>`, `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ReflectionProbeRecord>>` | read | P or C | `m_xProbeRecords` `[Optional]` | (i) | S4 |
| every lighting pass | `m_xGIVolumes`; `m_xSSGI`; data `uint32 m_uGIVolumeCount` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_IrradianceVolumeRecord>>` (upload ring); `Zenith_RHISRV<Zenith_RHITex2D>` (SSGI irradiance, half render extent) | read | P or C | `[Optional]` both | beyond X103 | S5 |
| `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `TransparentForward`, Deferred.md `GBuffer`, VisibilityBuffer.md `MaterialResolve` | `m_xLightmapPages`; `m_xInstanceLighting` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LightmapPageRecord>>`; `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_InstanceLightingRecord>>` (only if X106 is accepted) | read | P or C | `[Optional]` both | beyond X103, X106 | S5 |
| `DeferredLighting` | `m_xShadowViews` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ShadowViewRecord>>` | read | C | `[Optional]` | beyond X103 (e) | S5 |
| `DeferredLighting` | `m_axGBuffer[5]` (GB4 advanced surfaces) | `Zenith_RHISRV<Zenith_RHITex2D>` | read | C | `[Optional]` (GB4) | X80 | S5 |
| `DeferredLighting` | data `Zenith_RHIFloat4x4 m_xInvViewProjection`, `Zenith_RHIFloat3 m_xCameraPositionWS`, `Zenith_RHIUInt3 m_xClusterGrid`, `float m_fExposure` | as named | — | — | — | beyond X103 | S5 |
| `TransparentForward` | `m_xIBLSpecular`, `m_xIBLIrradiance`, `m_xBRDFLUT` | as in ForwardPlusOpaque | read | P | — | beyond X103 (i) | S3 |
| `VolumetricFog` | `m_xIBLIrradiance`, `m_xLocalFogVolumes` | `Zenith_RHISRV<Zenith_RHIByteBuffer>`, `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_LocalFogVolumeGPU>>` (upload ring) | read | C | `[Optional]` for the volumes | beyond X103 | S3 (volumes S5) |
| every `Flux_ClusterIndex` caller: `ForwardPlusOpaque`, `TransparentForward` with `bPERM_CLUSTERED`, `DeferredLighting`, `SSR`, `VolumetricFog`, VFX's lit-particle passes | data `Zenith_RHIUInt3 m_xClusterGrid` (where the pass's table lacks it: `VolumetricFog`), `Zenith_RHIUInt2 m_xClusterTileSize`, `Zenith_RHIFloat2 m_xClusterDepthRange` (n, f), the inputs of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"` | as named | — | — | — | beyond X103 | S3 (deferred S5) |
| every consumer of a LIGHT frame-data range | data `uint32 m_uShadowViewBase` (with `m_xShadowViews`), `m_uViewFrustumBase` (`LightDecalListBuild`, with `m_xViewFrusta`), `m_uProbeRecordBase` (with `m_xProbeRecords`), `m_uLocalFogVolumeBase` (`VolumetricFog`, with `m_xLocalFogVolumes`), `m_uGIVolumeBase` (with `m_xGIVolumes`): the element base of "Build-node upload data"; the SRV field itself binds `Flux_LightingFrame::m_xFrameChunk` | `uint32` | — | — | — | X129, beyond X103 | S3 (probes S4, volumes S5) |
| `VolumetricFog` | data `Zenith_RHIUInt3 m_xGrid`, `Zenith_RHIUInt2 m_xRenderExtent` (the render extent the clusters were built at, for the froxel's projected pixel), `float m_fNear`, `m_fFar`, `m_fHistoryWeight`, `m_fPreExposure`, `uint32 m_uLocalVolumeCount`, `Zenith_RHIFloat4x4 m_xInvViewProjection`, `m_xPrevViewProjection`, `Flux_FogMediumGPU m_xMedium`, `Flux_MediumOverrideGPU m_xOverride`, `Zenith_RHIFloat3 m_xMediumExtinctionRatio` | as named | — | — | — | beyond X103 | S3 (override S4) |
| `LightDecalListBuild` | data `uint32 m_uAtmosphereEnabled` (the sun-role rule of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`) | `uint32` | — | — | — | X148 (3) | S3 |
| `LightDecalListBuild` | `m_xProbeRecords` in, `m_xProbeList` out (third group); data `uint32 m_uProbeCount` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_ReflectionProbeRecord>>`, `Zenith_RHIUAV<Zenith_RHIByteBuffer>` `[FullCoverage]` | read / write | C | `[Optional]` | beyond X103 (i) | S4 |
| `LightClustering` | data `uint32 m_uMaxLights`, `m_uMaxDecals`, `m_uMaxProbes`; `m_xProbeList` | `uint32`; `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | C | `m_xProbeList` `[Optional]` | beyond X103 | S3 (probes S4) |
| `SkyAtmosphere` (`Flux_SkyViewLUTParams`) | `uPERM_STEP` value `multiScatter`; `m_xMultiScatterLUT` | `Zenith_RHIUAV<Zenith_RHITex2D>` (step multiScatter), `Zenith_RHISRV<Zenith_RHITex2D>` (step skyView) | write / read | C | `[FullCoverage]` on write | (f) | S3 |
| `SkyAtmosphere` (`Flux_SkyViewLUTParams`) | data `Zenith_RHIFloat4 m_xScaleHeights` (H_R, H_M, ozone centre, ozone half width), `Zenith_RHIFloat4 m_xGroundAlbedoMieG`, `Zenith_RHIFloat3 m_xMoonDirectionWS`, `m_xMoonIlluminance`, `float m_fStartDistance` (F), `float m_fPreExposure` | as named | — | — | — | beyond X103 (f) | S3 |
| `SkyAtmosphere` (`Flux_SkyDrawParams`) | data `Zenith_RHIFloat3 m_xSunDirectionWS`, `m_xSunIlluminance`, `float m_fSunAngularRadius`, `uint32 m_uDrawSunDisc`, `uint32 m_uAtmosphereEnabled`, `Zenith_RHIFloat3 m_xFallbackSkyRadiance` | as named | — | — | — | (f) | S3 |
| `SkyAtmosphere` (`Flux_SkyDrawParams`) | `m_xStarField` (persistent read-only cube, invalid handle when none), data `Zenith_RHIFloat4 m_xStarRotation`, `float m_fStarIntensity` | `Zenith_RHIDescriptorHandle`, as named | read | P | — | beyond X103 (ENV hook) | S4 |
| `SkyAtmosphere` (`Flux_SkyDrawParams`) | `m_xCloudLayer`, `m_xCloudTransmittance`; data `float m_fCloudPreExposureRatio` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | P | `[Optional]` | beyond X103 (ENV hook) | S4 |
| `SSR` | `m_xIBLSpecular`, `m_xProbeRecords`, `m_xClusterLists`, `m_xBRDFLUT` | as in ForwardPlusOpaque | read | C | `m_xProbeRecords` `[Optional]` | (h) | S3 |
| `SSR` | `m_xSpecularWeight` (Forward+ and Forward), `m_xSurfaceParams[2]` (GB0 and GB2 on Deferred and the visibility buffer) | `Zenith_RHISRV<Zenith_RHITex2D>` | read | C | `[Optional]` | (h) | S3 (deferred S5) |
| `SSR` | data `float m_fPreExposure`, `m_fPrevPreExposure`, `m_fMaxRoughness`, `m_fMaxDistance`, `uint32 m_uMaxIterations`, `m_uMaxMip`, `m_uFrameIndex`, `Zenith_RHIUInt3 m_xClusterGrid`, `Zenith_RHIFloat4x4 m_xInvViewProjection`, `m_xPrevViewProjection` | as named | — | — | — | beyond X103 | S3 |
| `TemporalAA` | `m_xPrevVelocity`; `m_xPrevHistory` | `Zenith_RHISRV<Zenith_RHITex2D>` | read | C | `[Optional]` both | (g) | S3 |
| `TemporalAA` | data `float m_fCurrentWeight`, `m_fGamma`, `m_fPreExposureRatio` | `float` | — | — | — | (g) | S3 |
| `AutoExposure` | data `float m_fPreExposure`, `Flux_MeteringMode m_eMetering` (as `uint32`), `m_fHistogramMinLog2`, `m_fHistogramMaxLog2`, `m_fLowPercent`, `m_fHighPercent`, `m_fSpeedUp`, `m_fSpeedDown`, `m_fDeltaSeconds`, `m_fCompensation`, `m_fMinEV100`, `m_fMaxEV100`, `m_fConfiguredEV100`, `uint32 m_uReadStride`, `m_uHistoryValid` | as named | — | — | — | (j) | S3 |
| `TonemapAndGrade` | `m_xGradingLUTB`; the plain data of `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Tonemap parameters"` | `Zenith_RHIDescriptorHandle`; as named | read | P | `[Optional]` LUT B | (j) | S3 (HDR fields S5) |
| `Bloom` | Kind text "a 6-level (desktop) or 5-level (mobile) downsample chain" becomes "a `render.quality.bloomLevels`-level (4 to 6) downsample chain"; data `float m_fKnee`, `m_fPreExposure` | `float` | — | — | — | X148 (3), beyond X103 | S3 |
| `GTAO` | data `float m_fThickness`, `uint32 m_uStepsPerSide`, `m_uFrameIndex` | as named | — | — | — | beyond X103 | S3 |
| `ContactShadows` | data `float m_fThickness`, `uint32 m_uFrameIndex`, `Zenith_RHIFloat4x4 m_xInvProjection` | as named | — | — | — | beyond X103 | S3 |
| `DepthOfField` | data `float m_fFocalLengthMm`, `m_fAperture`, `m_fSensorWidthMm`, `uint32 m_uOutputWidth`, `uint32 m_uPhysical` | as named | — | — | — | beyond X103 | S5 |
| new pass `ProbeFilter` | the pass table of `Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe filter pass table"` (program, fields, targets, queue G, compute) | — | — | C | — | beyond X103 (i) | S4 |
| new pass `SkyCapture` | the pass table of `Zenith2/Docs/Design/LIGHT/Reflections.md → "Sky capture pass table"` (programs `SkyCapture.slang` and `SHProject.slang`, queue G, compute) | — | — | C | — | beyond X103 (i) | S4 |
| new pass `SSGI` | the pass table of `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI pass table"` (programs `SSGITrace.slang` and `SSGIDenoise.slang`, queue G, compute) | — | — | C | — | X105 | S5 |
<!-- /assumption -->

### Areas that consume LIGHT

| Area | Consumes |
|---|---|
| RPROD | the feature functions in its frame functions, `Flux_LightingSystem::PlanShadows`, `GetDynamicResolution` and `GetJitterOffsetPixels` (view-list step 4, row L40) in `Flux_BuildViewList`, `HasDynamicResolution` and `SetPressureFloor` in its pressure hook, `WriteFrameData()` and `EndFrame` in `Flux.BuildGraph` (row L35), `Flux_LightQualitySettings` inside `Flux_QualitySettings`, `Flux_UpscalerMode`, `Flux_AtlasTile` and `Flux_ShadowViewPlan` from the leaf header, `Flux_RenderLightingEnvironment` and the sun and moon slot rule in the render environment, `Flux_CameraLightingSettings` in the view request and the view (row L36), `Flux_LightingSystem` in `Flux_Renderer` |
| RHI | `Flux_ValidateLightDelta` and `Flux_EncodeLightRecord` in `Flux_GPUScene::ApplyDeltas` |
| GEOM | `EvaluateLighting` for the advanced lobes; the lighting LUTs |
| ENV | the atmosphere block of `Flux_RenderLightingEnvironment`, `Flux_ViewMediumOverride` as the payload of its `Flux_RenderEnvironmentView` records, the sun and moon slot positions and their slot flag contract (row L39; no role flag exists), `Flux_MakeNamedLightKey`, the atmosphere LUTs for clouds and water, `Flux_ComputeAtmosphereTransmittance` |
| WORLD | the bake output contract; probe capture for baked probes |
| EDIT | the debug views, the grading LUT edit path, previews through `VIEW_KIND_PREVIEW`, the lighting component structs in the inspector |
| MOD, SCENE | the stable names of the lighting component types for the scaffold's template scenes |
| PLAT | `Flux_DynamicResolutionController::SetThermalCap` through the runtime |
| VFX, UI, TWOD | `Flux_ApplyFroxelFog`, `EvaluateLighting` for lit particles, `Flux_EncodeOutput` |
| ADVGFX | the path-tracer comparison scenes |
| CINE | camera lighting settings per shot (exposure, DoF, LUT blends) |

## Library and file layout

### Target summary

| Target | LIGHT content | `ZENITH_SUPPORTED` |
|---|---|---|
| `ZenithFlux` (engine library) | public headers, passes, subsystems, the component struct declarations | `ZENITH_GPU` (every preset except `linux-tsan`), as ZenithFlux declares |
| `ZenithShadersGenerated` (generated) | `<Program>.generated.h` for every LIGHT program | follows ZenithFlux |
| `ZenithCook` (tool) | LIGHT cook steps | host tool presets (`win-debug`, `win-release`, `win-tools`, `win-asan`) |
| `ZenithRuntime` | the reflection registration of the lighting component structs (`Internal/Zenith_LightingTypes.cpp`, row L33) | as ZenithRuntime |
| `ZenithFlux_Tests` | LIGHT tests | `ZENITH_GPU` host presets; the path-tracer tests only on `ZenithPathTracer`'s presets |

### ZenithFlux LIGHT files

| Path under `Zenith2/Engine/ZenithFlux/` | Content |
|---|---|
| `Flux_LightingErrors.h` | "Flux_LightingErrors header" |
| `Flux_LightSettings.h` | "Flux_LightSettings header" |
| `Flux_LightComponents.h` | "Flux_LightComponents header" |
| `Flux_Lighting.h` | `Flux_LightingSystemDesc`, `Flux_LightingFrame`, `Flux_LightingSystem` ("Flux_Lighting header") |
| `Flux_Lights.h` | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Flux_Lights header"` |
| `Flux_ShadowAtlas.h` | `Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_ShadowAtlas header"` |
| `Flux_Shadows.h` | `Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_Shadows header"` |
| `Flux_GlobalIllumination.h` | `Zenith2/Docs/Design/LIGHT/GI.md → "Flux_GlobalIllumination header"` |
| `Flux_Reflections.h` | `Zenith2/Docs/Design/LIGHT/Reflections.md → "Flux_Reflections header"` |
| `Flux_Atmosphere.h` | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Flux_Atmosphere header"` |
| `Flux_Upscaler.h` | `Zenith2/Docs/Design/LIGHT/Temporal.md → "Flux_Upscaler header"` |
| `Flux_Temporal.h` | `Zenith2/Docs/Design/LIGHT/Temporal.md → "Flux_Temporal header"` |
| `Flux_PostProcess.h` | `Zenith2/Docs/Design/LIGHT/PostProcess.md → "Flux_PostProcess header"` |
| `Flux_ColourManagement.h` | `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Flux_ColourManagement header"` |
| `Internal/Passes/LightList/`, `LightClustering/`, `Shadows/`, `ContactShadows/`, `GTAO/`, `Fog/`, `Sky/`, `SSR/`, `SSGI/`, `Probes/`, `DeferredLighting/`, `TAA/`, `Exposure/`, `DoF/`, `MotionBlur/`, `Bloom/`, `Tonemap/` | one directory per pass group: the feature function's `.cpp`, its parameter filling, and its CPU reference where a test needs one |
| `Internal/Lighting/LightingSystem.cpp`, `LightingUploads.cpp/.h`, `LightMirror.cpp/.h`, `LightKeys.cpp`, `ShadowAtlasAllocator.cpp`, `ShadowPlanning.cpp`, `CascadeSetup.cpp`, `Probes/ReflectionProbeManager.cpp`, `Probes/ProbeFormat.cpp/.h`, `BakedGI/BakedGIManager.cpp`, `BakedGI/GIFormats.cpp/.h`, `DynamicResolution/DynamicResolutionController.cpp`, `Exposure/ExposureState.cpp`, `Colour/ColourReference.cpp`, `Colour/LUTFormat.cpp/.h`, `Environment/EnvironmentFormat.cpp/.h`, `LightingConfigKeys.cpp`, `LightingCounters.cpp/.h`, `LightingDebugViews.cpp` | subsystems |
| `Internal/Content/Lighting/` | `DFG.lutsrc.json`, `SheenAlbedo.lutsrc.json`, `LTC.lutsrc.json`, `DefaultEnvironment.env.json`, `Identity.cube` (engine content sources, cooked like any content) |
| `Tests/Lighting/`, `Tests/Shadows/`, `Tests/Temporal/`, `Tests/Post/`, `Tests/Colour/`, `Tests/GI/`, `Tests/Probes/`, `Tests/Config/`, `Tests/ReferenceBRDF/`, `Tests/PhysicalAccuracy/`, `Tests/Parity/Scenes/Lighting*.scene.json` (RPROD's one-file-per-scene layout, entries in `Tests/Parity/Tolerances.json`), `Tests/Fuzz/ProbeFile.cpp`, `IrradianceVolumeFile.cpp`, `LightmapSetFile.cpp`, `EnvironmentFile.cpp`, `GradingLUTFile.cpp` | tests (`Zenith2/Docs/Design/LIGHT/Tests.md`) |

### ZenithRuntime LIGHT files

<!-- assumption: pending MOD, X108 -->
| Path under `Zenith2/Engine/ZenithRuntime/` | Content |
|---|---|
| `Internal/Zenith_LightingTypes.h`, `Internal/Zenith_LightingTypes.cpp` | `Zenith_Result<void, Zenith_ReflectError> Zenith_RegisterLightingComponentTypes(Zenith_TypeRegistry& xTypes)`: registers every struct of "Flux_LightComponents header" with the stable names and property IDs of "Light component schemas" |
| `Tests/Lighting/LightingTypesTests.cpp` | `Test_LightComponents_Registered`, `Test_LightComponents_RoundTripByPropertyID` |

MOD's "ZenithRuntime files" table gains these rows and the startup sequence's engine-types part of step 6 calls the function (row L33); the core-doc note that engine component types are C++-registered is X108's.
<!-- /assumption -->

### Shader modules and passes

| Path | Content |
|---|---|
| `Zenith2/Shaders/Lighting/EvaluateLighting.slang`, `BRDF.slang`, `Clusters.slang`, `Shadows.slang`, `IBL.slang`, `Fog.slang`, `Atmosphere.slang`, `Decals.slang`, `IndirectDiffuse.slang`, `AreaLights.slang` | the lighting modules (`Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"`); `Atmosphere.slang` holds `Flux_SampleSunTransmittance` and `Flux_SampleCloudShadow` |
| `Zenith2/Shaders/Common/ColourSpace.slang`, `Probes.slang` | the output encoders; `Flux_ReflectionProbeRecord` |
| `Zenith2/Shaders/Passes/Lights/LightList.slang`, `Clustering.slang` | Rendering's paths |
| `Zenith2/Shaders/Passes/Shadows/ShadowDepth.slang`, `ContactShadows.slang` | Rendering's paths |
| `Zenith2/Shaders/Passes/GTAO/GTAO.slang`, `GTAODenoise.slang`; `Fog/Froxels.slang`; `Sky/SkyViewLUT.slang`, `SkyDraw.slang`, `SkyCapture.slang`; `SSR/SSRTrace.slang`, `SSRDenoise.slang`, `SSRComposite.slang`; `SSGI/SSGITrace.slang`, `SSGIDenoise.slang`; `Probes/ProbeFilter.slang`, `SHProject.slang`; `DeferredLighting/DeferredLighting.slang`; `TAA/TAA.slang`; `Exposure/Histogram.slang`, `Adapt.slang`; `DoF/CoC.slang`, `Gather.slang`, `Composite.slang`; `MotionBlur/TileMax.slang`, `Gather.slang`; `Bloom/Downsample.slang`, `Upsample.slang`; `Tonemap/Tonemap.slang`; `Debug/LightingDebug.slang` | the pass programs |
| `Zenith2/Shaders/Tests/Lighting/BRDFHarness.slang`, `LightingHarness.slang` | test programs |

### ZenithCook LIGHT files

| Path under `Zenith2/Tools/ZenithCook/` | Content |
|---|---|
| `Cook_LightingBake.h` | the bake output contract (`Zenith2/Docs/Design/LIGHT/GI.md → "Bake output contract"`) |
| `Internal/Lighting/Cook_LightingLUTs.cpp`, `Cook_ReferenceBRDF.cpp/.h`, `Cook_LTCFit.cpp` | "Cook steps" |
| `Internal/Lighting/Cook_Environment.cpp`, `Cook_RGBEReader.cpp/.h`, `Cook_ProbeFilter.cpp`, `Cook_SH.cpp` | environments and probes |
| `Internal/Lighting/Cook_GradingLUT.cpp`, `Cook_CubeReader.cpp/.h` | grading LUTs |
| `Internal/Lighting/Cook_IrradianceVolumeWriter.cpp`, `Cook_LightmapWriter.cpp`, `Cook_ProbeWriter.cpp` | the bake writers |
| `Tests/Lighting/`, `Tests/Fuzz/CubeReader.cpp`, `Tests/Fuzz/RGBEReader.cpp` | cook tests |

The CPU reference BRDF used by the cook (`Cook_ReferenceBRDF`) and the one used by the analytical tests (`ZenithFlux/Tests/ReferenceBRDF/`) are the same test-side source compiled into both, and neither includes `Zenith2/Shaders/Lighting/` ([Z2-N-REND-949]).

### Engine content

`Zenith2/Engine/ZenithFlux/Internal/Content/Lighting/` (above). The cooked files are addressed as `content:/Engine/Lighting/<file>`, ASSET's engine content-root mapping of `Zenith2/Engine/<Lib>/Internal/Content/<p>` to `content:/Engine/<p>` (`Zenith2/Docs/Design/ASSET.md`), because a library root holds only its public headers and `CMakeLists.txt` ([Z2-N-CONTRACT-021]). LIGHT's per-tier values live in the tier fragments RPROD owns, `Zenith2/Engine/ZenithFlux/Internal/Config/Quality/<TierID>.config.json` (cooked into `content:/Engine/Config/Quality/<TierID>.config.json`, `Zenith2/Docs/Design/RPROD/Renderer.md → "Quality settings"`): LIGHT specifies the keys and values (`Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Tier values and configuration"`), RPROD the files.

### Test files

The catalogue is `Zenith2/Docs/Design/LIGHT/Tests.md`; every test file sits under a `Tests/` directory of its target, so `TestSeparationCheck` and `SourceLayout` hold.

### ZENITH_SUPPORTED matrix

LIGHT declares no target of its own; its files follow their host target's matrix (table "Target summary"). No LIGHT file sits under `Internal/Windows|Android|Linux/`: every LIGHT source is platform-neutral, and the platform differences (tier values, output modes) are data.

## Public API

### Header map

| Header | Types and functions |
|---|---|
| `Flux_LightingErrors.h` | `Flux_LightingError`, `Zenith_ToString(Flux_LightingError)` |
| `Flux_LightSettings.h` | every settings struct, `Flux_MeteringMode`, `Flux_LightQualitySettings`, `Flux_OutputModeRequest`, the environment blocks, `Flux_ViewMediumOverride` and `Flux_RenderLightingEnvironment`, `Flux_ExposureMode`, `Flux_CameraLightingSettings`, `Flux_OutputSettings`, `Flux_LightingDebugView`, `Flux_RegisterLightConfigKeys`, `Flux_ReadLightQualitySettings`, `Flux_ReadLightOutputSettings` |
| `Flux_LightComponents.h` | the lighting component structs and their property-ID constants |
| `Flux_Lighting.h` | `Flux_LightingSystemDesc`, `Flux_ViewGpuTime`, `Flux_LightingFrame`, `Flux_LightingSystem` |
| `Flux_Lights.h` | `Flux_LightType`, `Flux_LightFlag`, `Flux_LightDeltaOp`, `Flux_RenderLightDelta`, `Flux_CPULight`, the key functions, validation of deltas and environment slots, conversions, the cluster layout, the list, clustering and deferred-lighting feature functions |
| `Flux_ShadowAtlas.h` | `Flux_RenderLightKey`, `Flux_AtlasTile`, `Flux_ShadowViewKind`, `Flux_ShadowViewPlan`, `Flux_ShadowAtlasLayout`, `Flux_ShadowAtlasAllocator` (a leaf header) |
| `Flux_Shadows.h` | `Flux_CascadeSetup`, cascades, tile sizing, record encoding, the shadow priority, the shadow feature functions |
| `Flux_GlobalIllumination.h` | `Flux_SH9`, `Flux_SkyIrradianceSH`, SH functions, `Flux_AddSSAO`, `Flux_AddSSGI`, `Flux_BakedGIManager` |
| `Flux_Reflections.h` | `Flux_RenderProbeKey`, `Flux_MakeProbeKey`, `Flux_ProbeCaptureMode`, probe deltas, `Flux_ReflectionProbeManager`, `Flux_AddSSR`, capture and filter functions |
| `Flux_Atmosphere.h` | fog and sky feature functions, atmosphere reference functions |
| `Flux_Upscaler.h` | `Flux_UpscalerMode`, the one upscaler enumeration |
| `Flux_Temporal.h` | `Flux_JitterSequence`, `Flux_DynamicResolutionController`, TAA and upscale feature functions |
| `Flux_PostProcess.h` | `Flux_ExposureState`, exposure maths, exposure, DoF, motion blur and bloom feature functions |
| `Flux_ColourManagement.h` | `Flux_OutputMode`, `Flux_TonemapInputs`, `Flux_LottesCurve`, `Flux_HDRMetadata`, colour reference functions, `Flux_AddTonemap` |

### Rules across the LIGHT API

- Every public type carries one contract block ([Z2-N-CONTRACT-111]); fallible functions return `Zenith_Result<…, Flux_LightingError>` (or Rendering's error types where Rendering's API is called).
- Feature functions keep Rendering's signatures exactly (`Zenith2/Docs/Rendering/PerPass/Common.md` and siblings); LIGHT's additional feature functions (`Flux_AddCascadedShadows`, `Flux_AddLocalShadows`, `Flux_AddSkyLUTs`, `Flux_AddTemporalUpscale`, `Flux_AddSSGI`, `Flux_AddProbeCaptureFace`, `Flux_AddProbeFilter`, `Flux_AddSkyCapture`) follow the same form: a free function over `Flux_RenderGraph&` and graph handles that adds no pass when its feature is disabled ([Z2-N-REND-281], [Z2-N-REND-282]).
- A feature is enabled by the view's `Flux_ViewFeature` bit, which the tier's `render.quality.features` mask (RPROD's key) and the view kind set ([Z2-N-REND-601]); LIGHT's settings carry no second on/off switch for a feature that has a bit. Settings reach feature functions through `Flux_ViewContext::m_pxQuality->m_xLight` (row L28), except `Flux_AddSSAO`, which takes its `Flux_SSAOSettings` argument as Rendering specifies; the frame function passes `xView.m_pxQuality->m_xLight.m_xSSAO`.
- No LIGHT public header includes a Vulkan, D3D12, third-party or `Internal/` header; no STL type appears in a public header (`Zenith2/Docs/Contracts.md → "Containers and standard library policy"`).
- Every public header includes what it names: a header that names `uint8` … `uint64` includes `ZenithCore/Zenith_Types.h` (`Zenith2/Docs/Design/CORE.md`), so each compiles alone (`HeaderSelfContained`); the two leaf headers `Flux_LightingErrors.h` and `Flux_Upscaler.h` include nothing else.
- Every public type has its own contract block in the header listings of this family; where a listing says a type carries "the block of" another, the worker writes that block, with the type's name, above the type.
- CPU reference functions (conversions, splits, exposure, colour, atmosphere) are public so the tests and the cook call the same code the runtime does, and they allocate nothing.

### Flux_LightingErrors header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_LightingErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"   // uint8

/// @contract enum Flux_LightingError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: INVALID_LIGHT, INVALID_PROBE, INVALID_ENVIRONMENT: a render-world record failed validation and was refused
///   (the previous state or the defaults stay). INVALID_SETTINGS: a quality or configuration value violates a rule
///   (the previous values of that struct stay). ATLAS_FULL: a shadow request found no atlas space at the minimum tile
///   size. CAPACITY: a fixed-capacity structure (probe slots, lightmap pages, volumes) is full. CORRUPT_DATA,
///   VERSION_MISMATCH: a cooked lighting file failed validation or has another format or engine cook version.
///   OUT_OF_MEMORY: an allocation failed. QUEUE_FULL: flux.lighting.loadQueue refused a completion; the asset
///   system retries. DEVICE_LOST: the operation ran across a device loss and was abandoned.
/// Version: since Z2 Stage 3. Enumerators keep their meaning once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Flux_LightingError : uint8
{
	FLUX_LIGHTING_ERROR_INVALID_LIGHT = 1,
	FLUX_LIGHTING_ERROR_INVALID_PROBE,
	FLUX_LIGHTING_ERROR_INVALID_ENVIRONMENT,
	FLUX_LIGHTING_ERROR_INVALID_SETTINGS,
	FLUX_LIGHTING_ERROR_ATLAS_FULL,
	FLUX_LIGHTING_ERROR_CAPACITY,
	FLUX_LIGHTING_ERROR_CORRUPT_DATA,
	FLUX_LIGHTING_ERROR_VERSION_MISMATCH,
	FLUX_LIGHTING_ERROR_OUT_OF_MEMORY,
	FLUX_LIGHTING_ERROR_QUEUE_FULL,
	FLUX_LIGHTING_ERROR_DEVICE_LOST,
};

constexpr const char* Zenith_ToString(Flux_LightingError eError);   // "INVALID_LIGHT", …
```

### Flux_LightSettings header

Settings are plain values. Quality settings (`Flux_LightQualitySettings`) come from `render.quality.*` configuration keys and change with the tier; they live as the member `m_xLight` of RPROD's `Flux_QualitySettings` (`Zenith2/Docs/Design/RPROD/Renderer.md → "Quality settings"`), whose shape, tier mechanism and key registration are RPROD's. LIGHT owns the type of the aggregate, the meaning of its fields and their values per tier. Authored settings (`Flux_RenderLightingEnvironment`, `Flux_CameraLightingSettings`) come from components through the render world; output settings (`Flux_OutputSettings`) are user settings outside the tier. Each struct below is declared in `Zenith2/Engine/ZenithFlux/Flux_LightSettings.h` with the contract block shown for the first and the same fields for every other struct of the header, each block naming its own type (`Flux_ContactShadowSettings`, `Flux_SSAOSettings`, `Flux_FogSettings`, `Flux_SSRSettings`, `Flux_ProbeSettings`, `Flux_GISettings`, `Flux_TemporalSettings`, `Flux_DynamicResolutionSettings`, `Flux_ExposureSettings`, `Flux_BloomSettings`, `Flux_DoFSettings`, `Flux_MotionBlurSettings`, `Flux_LightQualitySettings`, `Flux_OutputSettings`, `Flux_AtmosphereParams`, `Flux_FogMediumParams`, `Flux_LocalFogVolume`, `Flux_StarField`, `Flux_CloudShadowMapping`, `Flux_ViewMediumOverride`, `Flux_AtmosphereBlock`, `Flux_WorldLightingBlock`, `Flux_RenderLightingEnvironment`, `Flux_CameraLightingSettings`): Ownership "plain value"; Thread affinity `IMMUTABLE`; Invalidation `None.`; Units as each member's comment; Errors "none; `Flux_ReadLightQualitySettings` and extraction validate ranges"; Version "since Z2 Stage 3" (Stage 4 or 5 where a comment says so). The four enumerations carry the enum block shown for `Flux_MeteringMode`.

<!-- assumption: pending RPROD, X103 -->
The seam is RPROD's row RP19 and LIGHT's row L28 (the struct change in Rendering's text is item (b) of X103). RPROD's `Flux_QualitySettings` keeps its own members (the feature mask `render.quality.features`, the two view caps, `m_uPerObjectLightCap`, `m_fLODScale`, `m_iTextureMipBias`, `m_uAnisotropy`) and carries `Flux_LightQualitySettings m_xLight`, whose type, members, keys, registration and validation are LIGHT's. Every quality value whose rendering meaning is LIGHT's lives in `m_xLight` and nowhere else: the shadow cascades, atlas, shadowed-light cap, shadow HiZ and filter quality, the froxel grid, SSAO, SSR and TAA quality and resolution, the upscaler, the bloom levels, the contact-shadow steps and the dynamic-resolution range. Keys keep RPROD's flat names where RPROD had named them (`render.quality.shadowCascades`, `…shadowHiZ`, `…shadowQuality`, `…ssaoQuality`, `…ssaoHalfResolution`, `…ssrQuality`, `…ssrHalfResolution`, `…taaQuality`, `…upscaler`, `…bloomLevels`, `…contactShadowSteps`, `…maxShadowedLights`, `…dynamicResolutionMin`, `…dynamicResolutionMax`); `render.quality.shadowAtlasSize` becomes `render.quality.shadowAtlasWidth` and `…shadowAtlasHeight` (the tiers use non-square atlases); `render.quality.froxelGrid` becomes the three `CONFIG_VALUE_TYPE_INT` keys `render.quality.froxelGridX`, `…froxelGridY` and `…froxelGridZ`, because the configuration store's types are BOOL, INT, FLOAT, STRING and ENUM (`Zenith2/Docs/Design/PLAT/Configuration.md → "Value encoding per type"`) and a three-component value has none; `render.quality.clusterGrid` is not a key (Rendering's `LightClustering` table fixes the grid per capability tier, `Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"`); `Flux_UpscalerMode` is the single upscaler enumeration (`Zenith2/Docs/Design/LIGHT/Temporal.md → "Flux_Upscaler header"`). The additional LIGHT keys below take the same flat `render.quality.<camelCaseName>` form (`Zenith2/Docs/Design/PLAT/Configuration.md → "Key grammar"`). RPROD's `Flux_QualityOverrides::m_iShadowQualityDelta` and `m_bFroxelGridDown` modify `m_xLight.m_xShadows.m_uFilterQuality` and `m_xLight.m_xFog.m_auGrid` on the per-view copy, as RPROD specifies.
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithFlux/Flux_LightSettings.h
#pragma once
#include "ZenithCore/Zenith_BuildInfo.h"   // Zenith_BuildPlatform (Flux_RegisterLightConfigKeys)
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithCore/Zenith_ConfigStore.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightingErrors.h"
#include "ZenithFlux/Flux_Upscaler.h"

/// @contract value Flux_ShadowSettings
/// Ownership: Plain value; a member of Flux_LightQualitySettings.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the quality-tier reader default-constructs it and fills its fields from configuration keys.
/// Invalidation: None.
/// Units: Texels, metres, counts, as each member's comment states.
/// Errors: None; Flux_ReadLightQualitySettings validates the ranges and the atlas rules of Shadows.md "Atlas layout".
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ShadowSettings
{
	uint32 m_uAtlasWidth = 4096;              // render.quality.shadowAtlasWidth, texels, power of two, 1,024 … 8,192
	uint32 m_uAtlasHeight = 4096;             // render.quality.shadowAtlasHeight
	uint32 m_uCascadeCount = 4;               // render.quality.shadowCascades, 1 … 4
	uint32 m_uCascadeTileTexels = 1024;       // render.quality.shadowCascadeTileTexels, 256 … 4,096, power of two
	float m_fShadowDistance = 250.0f;          // render.quality.shadowDistanceMetres, 10 … 2,000
	float m_fSplitLambda = 0.8f;               // render.quality.shadowSplitLambda, [0, 1]
	float m_fCascadeBlendFraction = 0.1f;      // render.quality.shadowCascadeBlend, [0, 0.5]
	float m_fCasterExtensionMetres = 500.0f;   // render.quality.shadowCasterExtensionMetres, 0 … 5,000
	uint32 m_uMaxShadowedLocalLights = 16;    // render.quality.maxShadowedLights (local lights only), 0 … 64
	uint32 m_uLocalTileMinTexels = 128;       // render.quality.shadowLocalTileMinTexels, 32 … 1,024, power of two
	uint32 m_uLocalTileMaxTexels = 1024;      // render.quality.shadowLocalTileMaxTexels, ≥ min, ≤ 4,096, power of two
	float m_fLocalResolutionScale = 1.0f;      // render.quality.shadowLocalResolutionScale, 0.25 … 4
	uint32 m_uFilterQuality = 1;              // render.quality.shadowQuality, uPERM_SHADOW_QUALITY 0 … 2
	bool m_bShadowHiZ = true;                  // render.quality.shadowHiZ
};

struct Flux_ContactShadowSettings              // the pass runs when FLUX_VIEW_FEATURE_CONTACT_SHADOWS is set
{
	uint32 m_uSteps = 8;                      // render.quality.contactShadowSteps, 8 or 16
	float m_fMaxDistanceMetres = 0.3f;         // render.quality.contactShadowMaxDistanceMetres, 0.05 … 2
	float m_fThicknessMetres = 0.05f;          // render.quality.contactShadowThicknessMetres, 0.005 … 0.5
};

struct Flux_SSAOSettings                       // the pass runs when FLUX_VIEW_FEATURE_SSAO is set
{
	uint32 m_uQuality = 1;                    // render.quality.ssaoQuality, uPERM_QUALITY 0 … 2
	bool m_bHalfResolution = false;            // render.quality.ssaoHalfResolution
	float m_fRadiusMetres = 1.0f;              // render.quality.ssaoRadiusMetres, 0.1 … 10
	float m_fThicknessMetres = 0.3f;           // render.quality.ssaoThicknessMetres, 0.01 … 5
};

struct Flux_FogSettings                        // the pass runs when FLUX_VIEW_FEATURE_VOLUMETRIC_FOG is set
{
	uint32 m_auGrid[3] = { 128, 72, 48 };     // render.quality.froxelGridX, …froxelGridY, …froxelGridZ: three INT keys, each 16 … 256
	float m_fNearMetres = 0.5f;                // render.quality.fogNearMetres, 0.1 … 10
	float m_fFarMetres = 2000.0f;              // render.quality.fogFarMetres (also the sky-view LUT start), 100 … 10,000
	bool m_bTemporal = true;                   // render.quality.fogTemporal
	float m_fHistoryWeight = 0.9f;             // render.quality.fogHistoryWeight, [0, 0.98]
	uint32 m_uShadowQuality = 1;              // render.quality.fogShadowQuality, 0 … 2
	uint32 m_uMaxLocalVolumes = 16;           // render.quality.fogLocalVolumes, 0 … 64 (Stage 5)
};

struct Flux_SSRSettings                        // the pass runs when FLUX_VIEW_FEATURE_SSR is set
{
	uint32 m_uQuality = 0;                    // render.quality.ssrQuality, 0 … 2
	bool m_bHalfResolution = false;            // render.quality.ssrHalfResolution (forced on mobile)
	float m_fMaxRoughness = 0.6f;              // render.quality.ssrMaxRoughness, [0, 1]
	float m_fMaxDistanceMetres = 200.0f;       // render.quality.ssrMaxDistanceMetres, 1 … 2,000
	uint32 m_uMaxIterations = 32;             // render.quality.ssrMaxIterations, 8 … 128
	uint32 m_uMaxMip = 7;                     // render.quality.ssrMaxMip, 0 … 12
};

struct Flux_ProbeSettings                      // Stage 4
{
	uint32 m_uMaxResident = 32;               // render.quality.probesMaxResident, ≤ uFLUX_MAX_RESIDENT_PROBES_<TIER>
	bool m_bRealtimeCapture = true;            // render.quality.probesRealtimeCapture
	uint32 m_uFilterSamples = 32;             // render.quality.probesFilterSamples, 16 or 32
	float m_fCaptureNearMetres = 0.1f;         // render.quality.probesCaptureNearMetres, 0.01 … 10
};

struct Flux_GISettings                         // Stage 5
{
	bool m_bVolumes = true;                    // render.quality.giVolumes
	bool m_bLightmaps = true;                  // render.quality.giLightmaps
	bool m_bSSGI = false;                      // render.quality.ssgiEnabled (Q41 (a), GI.md "SSGI platform scope")
};

struct Flux_TemporalSettings                   // the pass runs when FLUX_VIEW_FEATURE_TEMPORAL_AA is set
{
	Flux_UpscalerMode m_eUpscaler = FLUX_UPSCALER_TAAU;   // render.quality.upscaler: taa, taau (external refused until D-SEL-UPSCALER)
	uint32 m_uQuality = 1;                    // render.quality.taaQuality, uPERM_QUALITY 0 … 2
	float m_fCurrentWeight = 0.1f;             // render.quality.taaCurrentWeight, [0.02, 1]
};

struct Flux_DynamicResolutionSettings          // the only home of the dynamic-resolution range (RPROD row RP19)
{
	bool m_bEnabled = true;                    // render.quality.dynamicResolutionEnabled
	float m_fTargetMilliseconds = 15.0f;       // render.quality.dynamicResolutionTargetMs, 4 … 100
	float m_fMinScale = 0.75f;                 // render.quality.dynamicResolutionMin, [the floor, 1]
	float m_fMaxScale = 1.0f;                  // render.quality.dynamicResolutionMax, [m_fMinScale, 1]
	float m_fFloor = 0.75f;                    // generated from Z2-B-SHIP-011; not a key; m_fMinScale is never below it
};

/// @contract enum Flux_MeteringMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a configuration name outside the enumeration is refused by the store (ENUM key), and 0 is never valid.
/// Version: since Z2 Stage 3. Enumerators keep their values once shipped ([Z2-N-CONTRACT-467]); Flux_OutputModeRequest,
///   Flux_ExposureMode and Flux_LightingDebugView carry this block with their own names.
/// @endcontract
enum Flux_MeteringMode : uint8 { FLUX_METERING_AVERAGE = 1, FLUX_METERING_CENTRE_WEIGHTED, FLUX_METERING_SPOT };

struct Flux_ExposureSettings                   // the passes run when FLUX_VIEW_FEATURE_AUTO_EXPOSURE is set
{
	Flux_MeteringMode m_eMetering = FLUX_METERING_CENTRE_WEIGHTED;   // render.quality.exposureMetering: average, centre, spot
	float m_fHistogramMinLog2 = -12.0f;        // render.quality.exposureHistogramMinLog2, [−20, 0]
	float m_fHistogramMaxLog2 = 18.0f;         // render.quality.exposureHistogramMaxLog2, [4, 24], > min + 8
	float m_fLowPercent = 50.0f;               // render.quality.exposureLowPercent, [0, 99]
	float m_fHighPercent = 95.0f;              // render.quality.exposureHighPercent, (low, 100]
	float m_fSpeedUp = 3.0f;                   // render.quality.exposureSpeedUp, per second, (0, 20]
	float m_fSpeedDown = 1.0f;                 // render.quality.exposureSpeedDown, (0, 20]
	float m_fFixedDeltaSeconds = 0.0f;         // r.exposure.fixedDeltaSeconds (engine default and command line only); > 0 overrides the clock
};

struct Flux_BloomSettings                      // the pass runs when FLUX_VIEW_FEATURE_BLOOM is set
{
	uint32 m_uLevels = 6;                     // render.quality.bloomLevels, 4 … 6
	float m_fIntensity = 0.04f;                // render.quality.bloomIntensity, [0, 1]
	float m_fRadius = 0.85f;                   // render.quality.bloomRadius, [0.1, 2]
	float m_fThreshold = 0.0f;                 // render.quality.bloomThreshold, luminance after exposure, [0, 16]
	float m_fKnee = 0.5f;                      // render.quality.bloomKnee, [0, 1]
};

struct Flux_DoFSettings { uint32 m_uQuality = 0; };           // render.quality.dofQuality, 0 or 1 (Stage 5)
struct Flux_MotionBlurSettings { uint32 m_uSamples = 8; };    // render.quality.motionBlurSamples, 8 or 16 (Stage 5)

/// The member m_xLight of RPROD's Flux_QualitySettings (row L28): the tier's LIGHT values with the view's overrides applied.
struct Flux_LightQualitySettings
{
	Flux_ShadowSettings m_xShadows;
	Flux_ContactShadowSettings m_xContactShadows;
	Flux_SSAOSettings m_xSSAO;
	Flux_FogSettings m_xFog;
	Flux_SSRSettings m_xSSR;
	Flux_ProbeSettings m_xProbes;
	Flux_GISettings m_xGI;
	Flux_TemporalSettings m_xTemporal;
	Flux_DynamicResolutionSettings m_xDynamicResolution;
	Flux_ExposureSettings m_xExposure;
	Flux_BloomSettings m_xBloom;
	Flux_DoFSettings m_xDoF;
	Flux_MotionBlurSettings m_xMotionBlur;
};

enum Flux_OutputModeRequest : uint8 { FLUX_OUTPUT_MODE_REQUEST_AUTO = 1, FLUX_OUTPUT_MODE_REQUEST_SDR, FLUX_OUTPUT_MODE_REQUEST_HDR10, FLUX_OUTPUT_MODE_REQUEST_SCRGB };

struct Flux_OutputSettings                     // user settings; not a quality-tier value, held by Flux_LightingSystem
{
	Flux_OutputModeRequest m_eRequest = FLUX_OUTPUT_MODE_REQUEST_AUTO;   // r.output.mode: auto, sdr, hdr10, scrgb
	float m_fPeakNits = 1000.0f;               // r.hdr.peakNits, 400 … 10,000
	float m_fPaperWhiteNits = 203.0f;          // r.hdr.paperWhiteNits, 80 … 500
	float m_fUINits = 203.0f;                  // r.hdr.uiNits, 80 … 500
};

struct Flux_AtmosphereParams                   // ENV's atmosphere block; LIGHT's defaults when the block is absent
{
	bool m_bEnabled = true;
	float m_fGroundHeightMetres = 0.0f;
	float m_fPlanetRadiusMetres = 6360000.0f;
	float m_fAtmosphereHeightMetres = 100000.0f;
	Zenith_Vector3 m_xRayleighScattering = { 5.802e-6f, 13.558e-6f, 33.1e-6f };   // per metre
	float m_fRayleighScaleHeightMetres = 8000.0f;
	float m_fMieScattering = 3.996e-6f;
	float m_fMieAbsorption = 4.40e-6f;
	float m_fMieScaleHeightMetres = 1200.0f;
	float m_fMieAnisotropy = 0.8f;
	Zenith_Vector3 m_xOzoneAbsorption = { 0.650e-6f, 1.881e-6f, 0.085e-6f };
	Zenith_Colour m_xGroundAlbedo = { 0.3f, 0.3f, 0.3f, 1.0f };
	Zenith_Colour m_xFallbackSkyRadiance = { 0.0f, 0.0f, 0.0f, 1.0f };            // nits, when disabled
};

struct Flux_FogMediumParams
{
	bool m_bEnabled = true;
	float m_fDensity = 0.002f;                 // per metre at the base height
	float m_fHeightFalloff = 0.05f;            // per metre
	float m_fBaseHeightMetres = 0.0f;
	Zenith_Colour m_xAlbedo = { 0.9f, 0.9f, 0.9f, 1.0f };
	float m_fAnisotropy = 0.3f;                // Henyey–Greenstein g
	Zenith_Colour m_xEmission = { 0.0f, 0.0f, 0.0f, 1.0f };   // nits
};

struct Flux_LocalFogVolume                     // Stage 5
{
	Zenith_Matrix4 m_xWorldFromLocal;
	uint32 m_uShape = 0;                      // 0 box, 1 ellipsoid
	float m_fDensity = 0.01f;
	float m_fHeightFalloff = 0.0f;
	float m_fEdgeFade = 0.1f;
	Zenith_Colour m_xAlbedo;
	float m_fAnisotropy = 0.0f;
	Zenith_Colour m_xEmission;
};

struct Flux_StarField                          // Stage 4; Volumetrics.md "Star field"
{
	Zenith_PersistentID m_xCubeTexture;        // null: no stars
	Zenith_Quaternion m_xRotation;             // celestial rotation R
	float m_fIntensityNits = 0.0f;
};

struct Flux_CloudShadowMapping                 // Stage 5; Volumetrics.md "Cloud shadow on the sun"
{
	bool m_bValid = false;
	float m_fOriginX = 0.0f;                   // metres, world X of the square's centre
	float m_fOriginZ = 0.0f;
	float m_fExtentMetres = 4096.0f;
	float m_fOutsideTransmittance = 1.0f;
};

struct Flux_ViewMediumOverride                 // Stage 4; LIGHT's per-view evaluation input, built from ENV's Flux_RenderEnvironmentView (row L36), Volumetrics.md "Underwater medium override"
{
	uint64 m_ulViewID = 0;                    // Flux_ViewID::m_ulValue of the submerged view (a uint64: Flux_View.h includes this header)
	Zenith_Vector3 m_xScattering;              // per metre, per channel
	Zenith_Vector3 m_xAbsorption;              // per metre, per channel
	float m_fSurfaceHeightMetres = 0.0f;
};

struct Flux_AtmosphereBlock                    // copied by the engine environment extractor from ENV's Flux_EnvironmentBlocks (X125, row L31)
{
	bool m_bValid = false;                     // false: LIGHT's defaults, height fog off, no stars, no cloud shadow
	uint32 m_uSkyRevision = 0;                // incremented by ENV whenever the atmosphere, fog or star values change; any change rebuilds the LUTs and recaptures the sky
	Flux_AtmosphereParams m_xAtmosphere;
	Flux_FogMediumParams m_xFog;
	Zenith_Span<const Flux_LocalFogVolume> m_axLocalFogVolumes;          // render-world storage, at most 64
	Flux_StarField m_xStars;
	Flux_CloudShadowMapping m_xCloudShadow;
};

struct Flux_WorldLightingBlock                 // filled by the engine environment extractor from Zenith.Lighting.WorldLighting by reflection
{
	bool m_bValid = false;                     // false: the engine default environment, no grading, the defaults below
	Zenith_PersistentID m_xStaticEnvironment;  // .zenv asset; null = the engine default
	Zenith_PersistentID m_xGradingLUT;         // .zlut asset; null = none
	float m_fDefaultEV100 = 12.0f;             // the world's configured exposure
	float m_fExposureCompensation = 0.0f;      // EV
	float m_fMinEV100 = -4.0f;
	float m_fMaxEV100 = 16.0f;
	float m_fSkyIntensity = 1.0f;
};

struct Flux_RenderLightingEnvironment          // Flux_RenderEnvironment::m_xLighting (row L2, item (b) of X103; X125: one writer, the engine environment extractor)
{
	Flux_AtmosphereBlock m_xAtmosphere;
	Flux_WorldLightingBlock m_xWorldLighting;
};

enum Flux_ExposureMode : uint8 { FLUX_EXPOSURE_MODE_AUTO = 1, FLUX_EXPOSURE_MODE_MANUAL_EV, FLUX_EXPOSURE_MODE_PHYSICAL };

struct Flux_CameraLightingSettings             // a member of Flux_ViewRequest (row L2)
{
	Flux_ExposureMode m_eExposureMode = FLUX_EXPOSURE_MODE_AUTO;
	float m_fManualEV100 = 12.0f;
	float m_fAperture = 8.0f;                  // f-stops
	float m_fShutterSeconds = 1.0f / 125.0f;
	float m_fISO = 100.0f;
	float m_fExposureCompensation = 0.0f;
	bool m_bDepthOfField = false;
	float m_fFocalLengthMm = 0.0f;             // 0 = the artistic DoF model
	float m_fSensorWidthMm = 36.0f;
	float m_fFocusDistanceMetres = 10.0f;
	float m_fFocusRangeMetres = 5.0f;          // artistic model
	float m_fMaxCoCPixels = 32.0f;             // at 1440p output height, scaled
	bool m_bMotionBlur = false;
	float m_fShutterFraction = 0.5f;
	float m_fMaxBlurPixels = 32.0f;
	Zenith_PersistentID m_xGradingLUT;         // overrides the world's; null = the world's
	Zenith_PersistentID m_xGradingLUTB;        // blend target
	float m_fLUTBlend = 0.0f;
	float m_fVignetteIntensity = 0.0f;
	float m_fGrainIntensity = 0.0f;
};

enum Flux_LightingDebugView : uint8
{
	FLUX_LIGHTING_DEBUG_NONE = 0, FLUX_LIGHTING_DEBUG_CASCADES, FLUX_LIGHTING_DEBUG_SHADOW_ATLAS, FLUX_LIGHTING_DEBUG_CLUSTER_LIGHTS,
	FLUX_LIGHTING_DEBUG_CLUSTER_DECALS, FLUX_LIGHTING_DEBUG_AO, FLUX_LIGHTING_DEBUG_CONTACT_SHADOWS, FLUX_LIGHTING_DEBUG_SSR_HITS,
	FLUX_LIGHTING_DEBUG_SSR_CONFIDENCE, FLUX_LIGHTING_DEBUG_FOG_SLICES, FLUX_LIGHTING_DEBUG_PROBES, FLUX_LIGHTING_DEBUG_GI_SOURCE,
	FLUX_LIGHTING_DEBUG_EXPOSURE, FLUX_LIGHTING_DEBUG_HISTORY_REJECTION, FLUX_LIGHTING_DEBUG_VELOCITY, FLUX_LIGHTING_DEBUG_DIFFUSE_ONLY,
	FLUX_LIGHTING_DEBUG_SPECULAR_ONLY, FLUX_LIGHTING_DEBUG_LUMINANCE_HEATMAP, FLUX_LIGHTING_DEBUG_COUNT
};

/// @contract functions Flux_LightConfiguration
/// Ownership: Registration writes into the caller's schema; reading fills the caller's value.
/// Thread affinity: MAIN_THREAD (the configuration store's affinity).
/// Invalidation: None.
/// Units: As the struct members.
/// Errors: Registration returns the schema's Zenith_ConfigError; reading returns FLUX_LIGHTING_ERROR_INVALID_SETTINGS
///   for a rule violation (that struct keeps its previous values) and never asserts on configuration input.
/// Version: since Z2 Stage 3. Key names are stable; a rename is a configuration-file migration.
/// @endcontract
Zenith_Result<void, Zenith_ConfigError> Flux_RegisterLightConfigKeys(Zenith_ConfigSchema& xSchema, Zenith_BuildPlatform ePlatform);
Zenith_Result<void, Flux_LightingError> Flux_ReadLightQualitySettings(const Zenith_ConfigStore& xStore, Flux_LightQualitySettings& xInOut);
Zenith_Result<void, Flux_LightingError> Flux_ReadLightOutputSettings(const Zenith_ConfigStore& xStore, Flux_OutputSettings& xInOut);
```

| Rule | Specification |
|---|---|
| registration | `Flux_RegisterLightConfigKeys` is the key table of this header; it is called once, from RPROD's `Zenith_RegisterFluxConfigKeys` (`Zenith2/Docs/Design/RPROD/Renderer.md → "Create sequence"`, step 1), in the composition root's registration order (`Zenith2/Docs/Design/PLAT/Configuration.md → "Registration API"`), so ZenithFlux has one registration entry |
| defaults | the member defaults above are the `QUALITY_TIER_WIN_DEFAULT` values; `ePlatform` = Android registers the `QUALITY_TIER_S24_DEFAULT` values of `Zenith2/Docs/Design/LIGHT/QualityTiers.md` as the engine defaults instead |
| layers | every `render.quality.*` key allows every layer (engine default, platform, game, quality tier, user, command line) and is not restart-required; `r.output.mode` and `r.hdr.*` allow every layer except the quality tier; `r.exposure.fixedDeltaSeconds` and `r.debug.lighting` allow the engine default and the command line only (development builds for the latter) |
| simulation | no LIGHT key is simulation-affecting ([Z2-N-ARCH-919]) |
| reading | RPROD's step 2 of `Zenith2/Docs/Design/RPROD/Renderer.md → "Quality tier selection"` calls `Flux_ReadLightQualitySettings` into the new `Flux_QualitySettings::m_xLight`; a range or cross-field rule violation (for example the atlas rules of Shadows.md "Atlas layout", `m_fHighPercent ≤ m_fLowPercent`, `FLUX_UPSCALER_EXTERNAL`) keeps the previous value of that sub-struct, logs `flux.quality` once with the key, and returns `FLUX_LIGHTING_ERROR_INVALID_SETTINGS`, which RPROD logs and ignores |
| output | `Flux_LightingSystem::ApplyOutputSettings` reads `Flux_OutputSettings` on each configuration revision change |

### Flux_LightComponents header

<!-- assumption: pending MOD, X108 -->
Engine-shipped lighting components are C++-registered structs (Q42, ruled (b) by the orchestrator on 2026-09-26; the core-doc mechanism note is X108, which core-doc batch 4 applied; the human may override, in which case the same fields and property IDs become engine-content schemas and this header becomes their generated view). The structs are declared here, in ZenithFlux, with no reflection dependency; ZenithRuntime registers them ("ZenithRuntime LIGHT files", row L33). The field and ID tables are "Light component schemas".
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithFlux/Flux_LightComponents.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightSettings.h"

struct Flux_EnvironmentAsset; struct Flux_ProbeAsset; struct Flux_IrradianceVolumeAsset; struct Flux_LightmapSetAsset; struct Flux_GradingLUTAsset;

/// @contract value Flux_PointLightComponent
/// Ownership: Plain component value stored by the ECS backend through the reflected type Zenith.Lighting.PointLight.
/// Thread affinity: As every component: written by the phases its authority allows, read by the light extractor in
///   phase 12 (PHASE_RENDER_EXTRACTION).
/// Invalidation: None.
/// Units: Authoring units (degrees, Kelvin, lumens or candela as m_uIntensityUnit states); extraction converts.
/// Errors: None; the extractor validates and refuses through Flux_ValidateLightDelta.
/// Version: since Z2 Stage 3; the type version is 1. A layout change never changes a serialized form ([Z2-N-ARCH-644]).
/// @endcontract
struct Flux_PointLightComponent
{
	Zenith_Colour m_xColour = { 1.0f, 1.0f, 1.0f, 1.0f };   // 1  Colour
	float m_fTemperatureKelvin = 0.0f;                      // 2  TemperatureKelvin, 0 = use m_xColour, else 1,667 … 25,000
	float m_fIntensity = 800.0f;                            // 3  Intensity
	uint8 m_uIntensityUnit = 1;                            // 4  IntensityUnit: 0 the type's stored unit (candela for point and spot, nits for rect and disc), 1 lumens
	float m_fRangeMetres = 10.0f;                           // 5  RangeMetres
	float m_fSourceRadiusMetres = 0.0f;                     // 6  SourceRadiusMetres
	bool m_bCastsShadows = true;                            // 7  CastsShadows
	bool m_bVolumetric = true;                              // 8  Volumetric
	float m_fVolumetricScale = 1.0f;                        // 9  VolumetricScale, 0 … 2
	uint8 m_uLightingChannels = 1;                         // 10 LightingChannels
	float m_fShadowPriorityBias = 1.0f;                     // 11 ShadowPriorityBias
	uint16 m_uShadowMaxTileTexels = 0;                     // 12 ShadowMaxTileTexels
	float m_fShadowNormalOffsetTexels = 1.5f;               // 13 ShadowNormalOffsetTexels
	float m_fShadowDepthBiasMetres = 0.0f;                  // 14 ShadowDepthBiasMetres
	bool m_bSpecularEnabled = true;                         // 15 SpecularEnabled
};

struct Flux_SpotLightComponent                              // members 1–15 as Flux_PointLightComponent, then:
{
	/* 1–15 */
	float m_fInnerAngleDegrees = 20.0f;                     // 16 InnerAngleDegrees
	float m_fOuterAngleDegrees = 30.0f;                     // 17 OuterAngleDegrees
};

struct Flux_DirectionalLightComponent                       // members 1, 2, 7–15 as Flux_PointLightComponent (no range, radius, intensity unit), then:
{
	/* 1, 2, 7–15 */
	float m_fIlluminanceLux = 100000.0f;                    // 16 IlluminanceLux
	float m_fAngularRadiusDegrees = 0.2665f;                // 17 AngularRadiusDegrees
	bool m_bContactShadows = true;                          // 18 ContactShadows
};

struct Flux_RectLightComponent                              // Stage 5; members 1–15, then:
{
	/* 1–15 */
	float m_fWidthMetres = 1.0f;                            // 16 WidthMetres
	float m_fHeightMetres = 1.0f;                           // 17 HeightMetres
	bool m_bTwoSided = false;                               // 18 TwoSided (the unit is property 4; no property 19)
};

struct Flux_DiscLightComponent                              // Stage 5; members 1–15, then:
{
	/* 1–15 */
	float m_fRadiusMetres = 0.5f;                           // 16 RadiusMetres
	bool m_bTwoSided = false;                               // 17 TwoSided
	float m_fBarnInnerAngleDegrees = 90.0f;                 // 18 InnerAngleDegrees (90 = no barn cone)
	float m_fBarnOuterAngleDegrees = 90.0f;                 // 19 OuterAngleDegrees
};

struct Flux_ReflectionProbeComponent                        // Stage 4
{
	uint8 m_uCaptureMode = 1;                              // 1  CaptureMode: 1 baked, 2 realtime
	Zenith_AssetHandle<Flux_ProbeAsset> m_xBakedAsset;      // 2  BakedAsset
	Zenith_Vector3 m_xInfluenceHalfExtent = { 5, 5, 5 };    // 3  InfluenceHalfExtent
	Zenith_Vector3 m_xInfluenceOffset = { 0, 0, 0 };        // 4  InfluenceOffset
	float m_fFadeDistanceMetres = 1.0f;                     // 5  FadeDistanceMetres
	Zenith_Vector3 m_xParallaxHalfExtent = { 5, 5, 5 };     // 6  ParallaxHalfExtent (0 disables)
	uint8 m_uPriority = 0;                                 // 7  Priority
	bool m_bInterior = false;                               // 8  Interior
	bool m_bExteriorOnly = false;                           // 9  ExteriorOnly
	float m_fCaptureEV100 = 12.0f;                          // 10 CaptureEV100
	float m_fCaptureFarMetres = 500.0f;                     // 11 CaptureFarMetres
	uint32 m_uUpdateIntervalFrames = 0;                    // 12 UpdateIntervalFrames
	float m_fIntensity = 1.0f;                              // 13 Intensity
	uint32 m_uCaptureRequest = 0;                          // 14 CaptureRequest: incremented to recapture
};

struct Flux_IrradianceVolumeComponent                       // Stage 5
{
	Zenith_AssetHandle<Flux_IrradianceVolumeAsset> m_xAsset; // 1 Asset
	uint8 m_uPriority = 0;                                 // 2 Priority
	float m_fBlendDistanceMetres = 1.0f;                    // 3 BlendDistanceMetres
};

struct Flux_LightmapSetComponent                            // Stage 5; on the scene root
{
	Zenith_AssetHandle<Flux_LightmapSetAsset> m_xAsset;     // 1 Asset
};

struct Flux_WorldLightingComponent                          // one per world, on the scene root
{
	Zenith_AssetHandle<Flux_EnvironmentAsset> m_xStaticEnvironment;   // 1 StaticEnvironment
	Zenith_AssetHandle<Flux_GradingLUTAsset> m_xGradingLUT;           // 2 GradingLUT
	float m_fDefaultEV100 = 12.0f;                          // 3 DefaultEV100
	float m_fExposureCompensation = 0.0f;                   // 4 ExposureCompensation
	float m_fMinEV100 = -4.0f;                              // 5 MinEV100
	float m_fMaxEV100 = 16.0f;                              // 6 MaxEV100
	float m_fSkyIntensity = 1.0f;                           // 7 SkyIntensity
};

struct Flux_CameraLightingComponent                         // beside the camera component; IDs 1–20 in the member order of Flux_CameraLightingSettings
{
	Flux_CameraLightingSettings m_xSettings;                // registered member by member, angles and lens values in authoring units
};
```

Each of the other nine structs (`Flux_SpotLightComponent`, `Flux_DirectionalLightComponent`, `Flux_RectLightComponent`, `Flux_DiscLightComponent`, `Flux_ReflectionProbeComponent`, `Flux_IrradianceVolumeComponent`, `Flux_LightmapSetComponent`, `Flux_WorldLightingComponent`, `Flux_CameraLightingComponent`) carries the contract block shown for `Flux_PointLightComponent`, with its own name, its stable name of "Light component schemas", the extractor that reads it (light, probe, GI volume, lightmap binding, the engine environment extractor, the camera extractor) and its stage (Stage 3; Stage 4 for the reflection probe; Stage 5 for rect, disc, irradiance volume and lightmap set). The asset types are the loaded types of LIGHT's managers (asset kinds of row L11); each component type registers its lifecycle hooks because `Zenith_AssetHandle` carries a reference ([Z2-N-ARCH-645]).

### Flux_Lighting header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Lighting.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithFlux/Flux_ColourManagement.h"
#include "ZenithFlux/Flux_GlobalIllumination.h"
#include "ZenithFlux/Flux_Lights.h"
#include "ZenithFlux/Flux_PostProcess.h"
#include "ZenithFlux/Flux_Reflections.h"
#include "ZenithFlux/Flux_ShadowAtlas.h"
#include "ZenithFlux/Flux_Shadows.h"
#include "ZenithFlux/Flux_Temporal.h"
#include "ZenithFlux/Flux_ViewContext.h"   // Flux_ViewID, Flux_ViewContext, Flux_ViewList, Flux_FrameShared (RPROD's leaf headers; none includes Flux_Lighting.h)

class Zenith_Arena;               // CORE's; named by reference only ([Z2-N-CONTRACT-029])
class Flux_GPUScene;              // RHI's (Zenith2/Docs/Design/RHI/Memory.md); borrowed by reference in Create and ApplyRenderWorld
struct Flux_RenderWorld;          // RPROD's sealed render world; named by const reference only
class Flux_UploadRing;            // RHI's type (ZenithFlux/Internal/Upload); borrowed by reference, so no Internal header is included
class Flux_LightingUploadList;    // Internal/Lighting/LightingUploads.h: the frame's declared uploads ("Build-node upload data")
class Flux_ViewExtension;         // RPROD's (ZenithFlux/Flux_ViewExtension.h); LIGHT's debug-view extension is returned by reference ("Debug views")

/// @contract value Flux_LightingSystemDesc
/// Ownership: Plain value passed to Flux_LightingSystem::Create by Flux_Renderer.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Counts; the settings' own units.
/// Errors: None; Create validates m_uMaxViews in [1, uFLUX_MAX_PERSISTENT_VIEWS] (ZENITH_ASSERT) and the settings through
///   Flux_ReadLightQualitySettings's rules (FLUX_LIGHTING_ERROR_INVALID_SETTINGS). m_strContentRoot ends in '/'; Create reads
///   <m_strContentRoot><file> whole; Flux_Renderer keeps the default; a root that is not a content:/ path names a host
///   directory (the z2-light-cooked test cases, "Fixtures") and is refused with FLUX_LIGHTING_ERROR_INVALID_SETTINGS when
///   Zenith_IsShippingBuild() is true.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_LightingSystemDesc
{
	Flux_CapabilityTier m_eCapabilityTier;     // the ZenithFlux alias of RHI's Zenith_RHICapabilityTier (X73)
	Flux_LightQualitySettings m_xInitialQuality;
	Flux_OutputSettings m_xInitialOutput;
	uint32 m_uMaxViews = 16;                  // dynamic-resolution controllers and exposure states
	Zenith_StringView m_strContentRoot = "content:/Engine/Lighting/";  // where Create steps 2–5 load DFG.zllt, SheenAlbedo.zllt, LTC*.zllt and DefaultEnvironment.zenv
};

/// @contract value Flux_ViewGpuTime
/// Ownership: Plain value; RPROD fills a frame-arena span of them from its pass timing publisher.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Milliseconds of GPU time of one completed frame of the view's graph.
/// Errors: None; a non-finite or negative time is ignored by the controller and counted in Flux.DynRes.InvalidSamples.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ViewGpuTime
{
	Flux_ViewID m_xView;
	uint64 m_ulFrameIndex;                    // the completed frame the sample measures; a controller consumes each index once
	float m_fMilliseconds;
};

/// @contract value Flux_LightingFrame
/// Ownership: Constructed in the frame arena by Flux_LightingSystem::BeginFrame; the shadow spans are filled by
///   PlanShadows during view-list construction.
/// Thread affinity: IMMUTABLE after the view list is built.
///   Members non-const: the view-list build fills it field by field before it becomes immutable.
/// Invalidation: Invalid after the frame arena resets.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_LightingFrame
{
	Zenith_Span<const Flux_ShadowViewPlan> m_axShadowViews;     // in view-list order; RPROD's Flux_ViewList::m_axShadowPlans is this span
	Zenith_Span<const Flux_ShadowViewRecord> m_axShadowRecords; // declared at setup and written at order 5 as Flux_FrameShared::m_xShadowViewRecords
	uint32 m_uShadowsDropped = 0;                              // this frame's Flux.Shadows.Dropped
	Flux_SkyIrradianceSH m_xSkySH;                              // current sky irradiance SH (CPU copy of the cooked or captured value)
	uint32 m_uDirectionalCount = 0;
	Zenith_Span<const Flux_ViewMediumOverride> m_axViewMediumOverrides;   // Stage 4; at most uFLUX_MAX_VIEW_MEDIUM_OVERRIDES, from ENV's records
	uint32 m_uProbeCount = 0;                                  // Stage 4
	uint32 m_uGIVolumeCount = 0;                               // Stage 5
	Flux_LightingUploadList* m_pxUploads = nullptr;             // setup-time declarations; the list is mutable during setup, written by WriteFrameData
	Flux_RGBuffer m_xFrameChunk;                                // ImportFrameChunk of the borrowed ring, once per frame in AddSharedPasses; every LIGHT frame-data field binds it (row L42)
};

/// @contract type Flux_LightingSystem
/// Ownership: Created by Flux_Renderer through Create as a Zenith_Owned<Flux_LightingSystem>, after the persistent
///   resources and the GPU scene and before the first frame; destroyed with the renderer before the RHI device.
/// Thread affinity: ANY_THREAD_EXCEPT(ApplyRenderWorld, ApplyQualitySettings, ApplyOutputSettings, UpdateDynamicResolution,
///   BeginFrame, PlanShadows, AddSharedPasses, WriteFrameData, EndFrame, HasDynamicResolution, GetDynamicResolution,
///   GetJitterOffsetPixels, GetExposureState, OnDeviceLost). Those run on the Flux.BuildGraph node (OnDeviceLost at the render-idle point of
///   device-loss recovery); GetDynamicResolution(...).SetThermalCap may be called from the main thread through the
///   controller the runtime was handed at view creation.
/// Invalidation: Spans in Flux_LightingFrame are valid for the frame.
/// Units: As its members.
/// Errors: Create returns FLUX_LIGHTING_ERROR_OUT_OF_MEMORY or the resource error of a failed LUT or default-environment
///   load (mapped to FLUX_LIGHTING_ERROR_CORRUPT_DATA); per-record problems are counted, never returned.
/// Reentrancy: NOT_APPLICABLE. The type invokes no callbacks.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_LightingSystem
{
public:
	static Zenith_Result<Zenith_Owned<Flux_LightingSystem>, Flux_LightingError> Create(const Flux_LightingSystemDesc& xDesc,
		Flux_PersistentResources& xResources, Flux_GPUScene& xScene, Flux_UploadRing& xRing);   // xRing: Flux_Renderer's, which outlives the system
	void ApplyRenderWorld(const Flux_RenderWorld& xWorld);           // after Flux_GPUScene::ApplyDeltas: mirror, sun and moon slots, probe and GI deltas, environment
	void ApplyQualitySettings(const Flux_LightQualitySettings& xQuality);   // on a configuration revision change
	void ApplyOutputSettings(const Flux_OutputSettings& xOutput);
	void UpdateDynamicResolution(Zenith_Span<const Flux_ViewGpuTime> axGpuTimes);   // before the view list; the pressure floor is RPROD's SetPressureFloor call
	Flux_LightingFrame& BeginFrame(Zenith_Arena& xFrameArena);
	Zenith_Result<Zenith_Span<const Flux_ShadowViewPlan>, Flux_LightingError> PlanShadows(const Flux_ViewList& xNonShadowViews,
		Zenith_Arena& xFrameArena);                                  // called by Flux_BuildViewList (Shadows.md "Shadow planning in view-list construction")
	void AddSharedPasses(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);   // light list, shadows, sky LUTs, probes
	void WriteFrameData();                                           // order 5: every ticket LIGHT declared, through GetFrameData of the ring Create borrowed ("Build-node upload data")
	bool HasDynamicResolution(Flux_ViewID xView) const;
	Zenith_Vector2 GetJitterOffsetPixels(Flux_ViewID xView, float fOutputOverRender) const;   // view-list step 4; the committed index (Temporal.md "Jitter sequence")
	Flux_DynamicResolutionController& GetDynamicResolution(Flux_ViewID xView);   // ZENITH_ASSERT(HasDynamicResolution(xView))
	Flux_ExposureState& GetExposureState(Flux_ViewID xView);
	void OnDeviceLost();                                             // invalidates every LIGHT-held GPU state
	Flux_ViewExtension& GetDebugViewExtension();       // the LightingDebug extension at FLUX_EXTENSION_POINT_OVERLAY; Flux_Renderer registers it after create step 7b ("Debug views")
	void SetDebugView(Flux_LightingDebugView eView);                 // r.debug.lighting, forwarded by Flux_Renderer; a value >= FLUX_LIGHTING_DEBUG_COUNT is ignored
	void EndFrame(bool bFrameSubmitted);                            // bFrameSubmitted: GetFrameOutcome() == FLUX_FRAME_OUTCOME_SUBMITTED; commits or discards the frame's LIGHT state ("Deferred and failed frames"), drains the load queue
};
```

### Handle kinds used

| Kind (`Zenith2/Docs/Contracts.md → "Handle kinds"`) | LIGHT use |
|---|---|
| persistent IDs | environment, probe, volume, lightmap-set and LUT asset references in render-world records; entity IDs hashed into light keys |
| shared asset handles | `Zenith_AssetHandle<T>` in the component structs; the loaded `.zenv`, `.zprb`, `.zgiv`, `.zlmp`, `.zlut` assets held by `Flux_LightingSystem`'s managers |
| device handles | `Flux_PersistentReadOnlyTexture` and `Buffer` for LUTs and cooked data |
| operation handles | `Flux_ReadbackHandle` for exposure and counters; asset loads |
| frame-scoped graph handles | every `Flux_RGTexture` and `Flux_RGBuffer` a feature returns |

## Data structures and formats

### Format index

| Format | Magic | Specified in | Producer | Consumer |
|---|---|---|---|---|
| static environment `.zenv` | `'ZENV'` | `Zenith2/Docs/Design/LIGHT/Reflections.md → "Static environment"` | `Cook_Environment` | `Flux_ReflectionProbeManager` (slice 0) |
| reflection probe `.zprb` | `'ZPRB'` | `Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe cooked format"` | WORLD's bake through `Cook_WriteReflectionProbe` | `Flux_ReflectionProbeManager` |
| irradiance volume `.zgiv` | `'ZGIV'` | `Zenith2/Docs/Design/LIGHT/GI.md → "Irradiance volume cooked format"` | WORLD's bake | `Flux_BakedGIManager` |
| lightmap set `.zlmp` (+ GEOM `.ztex` pages) | `'ZLMP'` | `Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap set cooked format"` | WORLD's bake | `Flux_BakedGIManager` |
| grading LUT `.zlut` | `'ZLUT'` | `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Grading LUT cook"` | `Cook_GradingLUT` | tonemap |
| lighting LUTs (`DFG`, `SheenAlbedo`, `LTCMatrix`, `LTCMagnitude`) | `'ZLLT'` | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Lighting lookup tables"` | `Cook_LightingLUTs` | every lighting pass |
| lighting component types | reflected C++ structs (no file of their own; serialized by property ID in scene documents) | "Light component schemas" | authors, through EDIT's inspector | the engine light and environment extractors |

### LIGHT cooked section kinds

Every LIGHT cooked file uses the GEOM cooked file header (`Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`) with its own magic and format version 1; section kinds 100 … 199 are LIGHT's:

| Kinds | File |
|---|---|
| 100–101 | `.zllt`: 100 `LIGHT_SECTION_LUT_TABLE_INFO` (32 bytes: width u32, height u32, format u32, table ID u32, 16 reserved zero bytes), 101 `LIGHT_SECTION_LUT_TABLE_TEXELS` |
| 110–112 | `.zenv` |
| 120–121 | `.zprb` (120 is the 64-byte record of `Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe info record"`) |
| 130–134 | `.zgiv` |
| 140–142 | `.zlmp` |
| 150–151 | `.zlut` |

### Byte order and serialization rules

- Every integer and float is little-endian and written field by field (Z2-D-038); GPU images (cube faces, 3D textures, LUT texels) are written as their upload image.
- Every compressed section is one COMPRESS section with its 24-byte descriptor (`Zenith2/Docs/Design/COMPRESS.md → "Compressed section descriptor on disk"`); readers select the codec from the data (Z2-D-004).
- Cooked lighting data re-cooks on any engine cook version or format version change and is never migrated at runtime ([Z2-N-CONTRACT-529]); readers refuse any other version with `FLUX_LIGHTING_ERROR_VERSION_MISMATCH`.
- Reserved bytes are written as zero and a reader refuses a non-zero reserved byte with `FLUX_LIGHTING_ERROR_CORRUPT_DATA`.

### In-memory sizes that matter

| Datum | Size |
|---|---|
| light record, shadow-view record, probe record, render-light delta, atlas tile | 64, 96, 64, 112, 8 bytes |
| cluster header; cluster lists per view | 8 bytes; 1,188,864 bytes desktop, 81,920 mobile |
| exposure buffer; histogram | 16 bytes; 1,024 bytes |
| sky irradiance SH | 144 bytes |
| environment cube array | 34.1 MB desktop, 2.2 MB mobile |
| shadow atlas (transient) | 16 – 128 MiB by tier |
| froxel volume | 7.03 MiB per volume at 160 × 90 × 64; a view holds 3 (21.1 MiB) |
| TAA history | 8 bytes per output pixel per half; a view holds two halves, 16 bytes per output pixel (59.0 MB at 2560 × 1440) |
| instance lighting table (Stage 5, if X106 is accepted) | 16 bytes per instance slot (4 MiB at 262,144 desktop instances) |

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Failure behaviour | Specified in |
|---|---|---|---|
| light delta validation and mirror update | O(deltas) | invalid deltas refused and counted | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light extraction and the CPU light mirror"` |
| light and decal list | O(lights + decals) GPU | directional overflow dropped and counted | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"` |
| clustering | O(tiles × lights / 64) groups | overflow truncated and counted | `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light clustering"` |
| shadowed-light selection and atlas allocation | O(n log n + requests × log(area)) CPU | unshadowed light, counted | `Zenith2/Docs/Design/LIGHT/Shadows.md → "Atlas allocation"` |
| cascade setup | O(cascades) | none | `Zenith2/Docs/Design/LIGHT/Shadows.md → "Cascaded shadow maps"` |
| GTAO | O(pixels × slices × steps) | none | `Zenith2/Docs/Design/LIGHT/GI.md → "GTAO"` |
| froxel injection and integration | O(froxels) | NaN sanitised | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Integration"` |
| sky LUTs | fixed; transmittance and MS only on a sky revision change | defaults on invalid parameters | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sky-view LUT"` |
| SSR trace | O(pixels × iterations) | miss → probes | `Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR hierarchical trace"` |
| probe capture and filter | 6 face views + O(texels × samples) | fallback chain | `Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe filtering"` |
| TAA and upscale | O(output pixels) | history rejected, NaN sanitised | `Zenith2/Docs/Design/LIGHT/Temporal.md → "TAA resolve"` |
| dynamic-resolution control | O(1) per view | invalid samples ignored | `Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution controller"` |
| exposure metering and adaptation | O(pixels / 4) + O(256) | reset on NaN | `Zenith2/Docs/Design/LIGHT/PostProcess.md → "Adaptation"` |
| tonemap and output | O(output pixels) | SDR fallback | `Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output transforms"` |

### Frame-level flow of LIGHT work

| Where (`Flux.BuildGraph`, `Zenith2/Docs/Design/RPROD/Renderer.md → "Render frame sequence"`) | Step |
|---|---|
| step 4, render-world consumption, after `Flux_GPUScene::ApplyDeltas` | `ApplyRenderWorld` (the light mirror, the sun and moon slots, probe and GI deltas, the environment, the per-view medium overrides from ENV's `Flux_RenderEnvironmentView` records), `ApplyQualitySettings` and `ApplyOutputSettings` when the configuration revision changed, `BeginFrame`, `UpdateDynamicResolution` (GPU times; RPROD's pressure hook sets the floor through `SetPressureFloor`), exposure-state readback consumption |
| step 5, inside `Flux_BuildViewList` (RPROD) | `GetDynamicResolution(view).GetScale()` per view (the view extent), `GetJitterOffsetPixels(view, output / render)` per jittered view (`m_xJitteredProjection`, row L40), `PlanShadows` (selection, cascades, atlas allocation, records, the light-record staging), probe residency selection |
| step 8, common prologue | `LightDecalListBuild`, `Flux_AddCascadedShadows`, `Flux_AddLocalShadows`, sky LUTs and probe captures and filters as shared producers; every per-frame upload these declare is a `DeclareFrameData` ticket ("Build-node upload data") |
| step 9, per view | clustering, fog, GTAO, contact shadows, (SSGI), the path's lighting via `EvaluateLighting`, sky, SSR, then the epilogue: TAA or upscale, exposure, DoF, motion blur, bloom, tonemap; the frame's exposure and counter readbacks are declared here |
| step 11, after RHI's `ExtendReservation` returned `FITS` or `EXTENDED` (row L35) | `WriteFrameData()`: every LIGHT ticket written through `GetFrameData`, in declaration order |
| last LIGHT call of `Flux.BuildGraph`, on every path | `EndFrame(Flux_FrameLifetime::GetFrameOutcome() == FLUX_FRAME_OUTCOME_SUBMITTED)`: commit the frame's LIGHT state on a submitted frame, discard it on a `DEFERRED` or `FAILED` one ("Deferred and failed frames"), drain `flux.lighting.loadQueue` |

### History validity state machine

Per view and history (`Flux_ViewPersistentState::m_bHistoryValid` is Rendering's; LIGHT keeps per-history flags for the histories of `Zenith2/Docs/Design/LIGHT/Temporal.md → "Histories LIGHT owns"` in `Flux_LightingSystem`):

| State × event | frame submitted | frame not submitted (`DEFERRED` or `FAILED`) | invalidation event of "History invalidation" | extent step (dynamic resolution) | view absent 60 frames |
|---|---|---|---|---|---|
| ABSENT | → FIRST (create and clear resources) | stays ABSENT (a resource created this frame is kept; its clear is declared again) | stays ABSENT | stays | stays |
| FIRST | → VALID (history written this frame) | stays FIRST (nothing was written; the ping-pong index does not swap) | → FIRST (clear again) | → VALID | → ABSENT (released) |
| VALID | stays VALID | stays VALID (the previous half still holds the last submitted frame's history) | → FIRST | stays VALID (output-extent histories) or → RESAMPLE (render-extent histories) | → ABSENT |
| RESAMPLE | → VALID after the UV-normalised resample | stays RESAMPLE | → FIRST | stays RESAMPLE | → ABSENT |

In FIRST no pass reads the previous half ("The first-write rule for histories"). The extent-step column is the narrowed rule of X104 (row L18). The transition a frame's graph was built for is computed on the build node as a pending state and applied by `EndFrame` only when the frame's outcome is `FLUX_FRAME_OUTCOME_SUBMITTED`, the RHI rule that a frame submitting nothing writes no history and swaps no ping-pong (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`); "view absent 60 frames" counts submitted frames.

### Probe residency state machine

| State × event | selected | not selected 120 submitted frames | load completes | load fails | capture and filter complete in a submitted frame | frame not submitted | device lost |
|---|---|---|---|---|---|---|---|
| UNLOADED | → LOADING (baked) or → CAPTURING (realtime) | stays | — | — | — | stays | stays |
| LOADING | stays | → UNLOADED (cancel) | → RESIDENT (upload into a slice; the slice is assigned when the upload's `StreamingUpload` copy is in a submitted frame) | → FAILED | — | stays (RHI carries the staged upload to the next submitted frame) | → UNLOADED |
| CAPTURING | stays | → UNLOADED | — | — | → RESIDENT | stays, with the same count of captured faces: the frame's face is captured again | → UNLOADED |
| RESIDENT | stays; a capture request → CAPTURING keeping the old slice until done | → UNLOADED (slice freed) | — | — | — | stays | → UNLOADED |
| FAILED | stays (logged once) | → UNLOADED | — | — | — | stays | → UNLOADED |

### Build-node upload data

<!-- assumption: pending RHI, RPROD -->
RHI's build-node order (`Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`) forbids any write into a frame's ring memory before that frame's `ExtendReservation`. Every LIGHT producer of per-frame upload data therefore declares its bytes during graph setup through `Flux_FrameShared::m_pxLighting->m_pxUploads->Declare(xSourceBytes, szOwner)` (`Flux_LightingUploadList`, `Zenith2/Engine/ZenithFlux/Internal/Lighting/LightingUploads.h`), which calls `Flux_UploadRing::DeclareFrameData(ulBytes, 16, szOwner)` on the ring `Create` borrowed (the owner string is the pass name), keeps the `Flux_FrameDataRange` ticket with the frame-arena source span, and returns the ticket; nothing is written at setup; `Flux_LightingSystem::WriteFrameData()`, which RPROD's `Flux.BuildGraph` step 11 calls after `ExtendReservation` returned `FITS` or `EXTENDED` (row L35, answering RHI row R18), copies each span into `GetFrameData(ticket)` in declaration order. A frame whose extension defers or fails never reaches `WriteFrameData`, so LIGHT writes no ring byte in it.

The consumer binding (row L42, pending RHI and X129: RHI's `Zenith2/Docs/Design/RHI/Memory.md → "Upload ring internals"` reads ring data through `ImportFrameChunk`, and X129 carries the core-doc wording). A pass reads a frame-data range as the frame chunk plus an element base, never as a view of the range, because the range's offset exists only after `ExtendReservation` while the fields are declared at setup:

1. Setup: `AddSharedPasses` calls `Flux_UploadRing::ImportFrameChunk(xGraph)` once per frame on the ring `Create` borrowed and stores the result in `Flux_LightingFrame::m_xFrameChunk`; every LIGHT consumer field of a range (the table below) is declared as a read of that buffer, with its C++ type unchanged (`Zenith_RHISRV<Zenith_RHIStructBuffer<T>>`: the graph's structured view of the chunk with stride `sizeof(T)`, a transient view descriptor written at order 5 like every transient view, [Z2-N-REND-052]).
2. Setup: `Declare` reserves `n × sizeof(T) + sizeof(T) − 16` bytes at alignment 16, so that the range holds n records starting at a multiple of `sizeof(T)` wherever the ring places it.
3. Order 5: `WriteFrameData()` reads the ticket's byte offset o (`Flux_UploadAllocation::m_ulOffset` of `GetFrameData`), sets the element base `b = ceil(o / sizeof(T))`, and copies the n records to byte `b × sizeof(T)` of the chunk (at most `sizeof(T) − 16` bytes past o, inside the range).
4. The record job fills b into the consumer's plain-data field (`m_uShadowViewBase`, `m_uViewFrustumBase`, `m_uProbeRecordBase`, `m_uLocalFogVolumeBase`, `m_uGIVolumeBase`; X148 table) when it `Allocate`s the parameter struct after the reservation is final (RHI's parameter-struct rule), and the shader reads record i at element `b + i`.
<!-- /assumption -->

| Producer | Records (bytes each; declared bytes add one record less 16) | Declared by (setup) | Consumer field, element base |
|---|---|---|---|
| the frame's shadow-view records (`PlanShadows`) | shadow views (96) | the first of `Flux_AddCascadedShadows` and `Flux_AddLocalShadows` that runs | `Flux_FrameShared::m_xShadowViewRecords`, every `m_xShadowViews`; `m_uShadowViewBase` |
| the view-frustum array of the light and decal list | non-shadow views (`sizeof(Flux_FrustumPlanes)`, 96) | `Flux_AddLightDecalList` | `LightDecalListBuild`'s `m_xViewFrusta`; `m_uViewFrustumBase` |
| the frame probe records, sorted by the blending key (Stage 4) | resident probes (64) | `Flux_ReflectionProbeManager::AddPasses` | `Flux_FrameShared::m_xProbeRecords`; `m_uProbeRecordBase` |
| the per-view local fog volume list (Stage 5) | the view's volumes (112) | `Flux_AddVolumetricFog` | `VolumetricFog`'s `m_xLocalFogVolumes`; `m_uLocalFogVolumeBase` |
| the per-view irradiance-volume records (Stage 5) | at most 8 (64) | the path's opaque lighting pass setup | every lighting pass's `m_xGIVolumes`; `m_uGIVolumeBase` |

What is not LIGHT frame data, and how it reaches the GPU:

| Data | Path |
|---|---|
| LIGHT's plain-data parameter fields (exposure, grids, matrices, the atmosphere and fog records) | captured by value in the frame arena at setup and copied by the record job into the parameter struct it `Allocate`s after the reservation is final (RHI's rule for parameter structs) |
| light records restaged with this frame's shadow index (`StageLightRecord`), instance lighting records | RHI's GPU-scene pending store, uploaded by RHI's scatter at order 5 and kept pending by a frame that submits nothing |
| lightmap page records, baked probe cubes, irradiance-volume textures, LUT and environment images | upload requests (`Zenith2/Docs/Design/RHI/Memory.md → "Upload request state machine"`), copied by the `StreamingUpload` pass of the frame that services them and carried by RHI to the next submitted frame |

`Test_Lighting_FrameDataDeclaredBeforeExtension` asserts the order and the byte counts.

### Deferred and failed frames

A frame that submits nothing (`FLUX_FRAME_OUTCOME_DEFERRED` or `FLUX_FRAME_OUTCOME_FAILED`, `Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`, `→ "Frame failures"`) records no pass, writes no history and fails its readbacks with `FLUX_READBACK_ERROR_FRAME_DEFERRED` or `_FRAME_FAILED`. LIGHT state that describes what the GPU has done must therefore advance only on a submitted frame. The build node computes each piece as a pending value in the frame arena; `EndFrame(bFrameSubmitted)` applies it when the frame submitted and discards it otherwise:

| LIGHT frame state | On a submitted frame | On a frame that submits nothing |
|---|---|---|
| atmosphere LUT bookkeeping (`m_uLUTRevision`, `m_bLUTValid`; "Transmittance LUT" of Volumetrics.md) | a frame that declared the transmittance and multiple-scattering steps records the revision it built for and counts `Flux.Sky.LUTRebuilds` | unchanged: the rebuild stays pending and the next built frame declares it again, so a deferred rebuild frame never leaves a stale LUT behind |
| history validity per view and history ("History validity state machine") | the pending transition applies | no transition; the ping-pong index does not swap |
| the jitter frame counter per view, held by `Flux_LightingSystem` in the view's per-view slot and read through `GetJitterOffsetPixels` (Temporal.md "Jitter sequence") | +1 | unchanged, so the sequence repeats the index instead of skipping one |
| the sky-capture face round robin and the captured-sky key (Reflections.md "Sky capture") | the face advances; after the sixth, the filter frame follows | unchanged: the same face is captured in the next built frame |
| probe capture progress (faces captured, filter done) and the residency frame counters ("Probe residency state machine") | applied | unchanged |
| per-light shadow tile-size hysteresis and `m_uLastShadowIndex` (`Flux_CPULight`) | copied from the frame's pending copies into the mirror | discarded: the mirror keeps the last submitted values, so the next frame restages every light whose shadow index differs from the last one uploaded; the records staged in the frame stay in RHI's pending store and are overwritten by the next frame's (last write wins) |
| the mirror's `m_bChangedThisFrame` flags | cleared | kept, so the next built frame restages those lights |
| `Flux_ExposureState` | the frame's readback is consumed when READY, 2 to 3 frames after the frame that declared it | the readback fails with `FLUX_READBACK_ERROR_FRAME_DEFERRED` (or `_FAILED`); `Consume` keeps the previous exposure, counts no `Flux.Exposure.Reset`, and the next built frame declares a new readback |
| the GPU counter readback | consumed when READY | fails the same way; the counters keep their previous values for that frame |
| dynamic-resolution controllers | independent of the outcome: each consumes a GPU-time sample once, by its frame index (Temporal.md "Dynamic-resolution controller") | the same |
| `flux.lighting.loadQueue` | drained | drained: completions are CPU state; a slice or page is assigned only when its upload reaches a submitted frame |

`Flux.Lighting.FramesNotSubmitted` counts the frames `EndFrame` saw with `bFrameSubmitted` false. `Test_Lighting_DeferredFrameKeepsState` asserts every row.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| settings structs, the environment blocks, `Flux_ViewMediumOverride`, `Flux_RenderLightDelta`, `Flux_RenderLightKey`, `Flux_RenderProbeKey`, `Flux_RenderProbeDelta`, `Flux_AtlasTile`, `Flux_ShadowViewPlan`, `Flux_CascadeSetup`, `Flux_JitterSequence`, `Flux_SH9`, `Flux_LottesCurve`, `Flux_HDRMetadata`, `Flux_ViewGpuTime`, `Flux_LightingSystemDesc`, every LIGHT enumeration | `IMMUTABLE` |
| `Flux_CPULight` | owned by `Flux_LightingSystem`'s mirror on the `Flux.BuildGraph` node; read-only to `Flux_ComputeShadowPriority` and the tests |
| pure functions (keys, validation, encoding, conversions, splits, priority, exposure maths, colour and atmosphere references, SH) | `ANY_THREAD` |
| `Flux_LightingSystem`, `Flux_ShadowAtlasAllocator`, `Flux_ReflectionProbeManager`, `Flux_BakedGIManager`, `Flux_ExposureState` | `OWNER_THREAD`, the `Flux.BuildGraph` node |
| `Flux_DynamicResolutionController` | `ANY_THREAD_EXCEPT(Update, GetScale, SetPressureFloor, Reset)`: those on the `Flux.BuildGraph` node (`SetPressureFloor` from RPROD's pressure hook), `SetThermalCap` from the main thread |
| the lighting component structs | as every component (their authority phases), read by the extractors in phase 12 |
| feature functions | the `Flux.BuildGraph` node (graph setup is single-threaded, [Z2-N-REND-286]) |
| configuration functions | `MAIN_THREAD` |
| cook steps | one worker process per invocation |

### Frame-graph placement

LIGHT's CPU work runs on the render subgraph's `Flux.BuildGraph` node and its sub-jobs (`Zenith2/Docs/Architecture/CPUFrameGraph.md`, `FRAME_SUBGRAPH_RENDER`), reads only the sealed `Flux_RenderWorld` and Flux-owned state, and never runs in a simulation phase, so it cannot affect determinism ([Z2-N-ARCH-812]); the thermal cap is the one cross-thread input, through an atomic. GPU work runs in the passes named by the frame anatomy.

### Async-compute eligibility

Exactly the LIGHT passes Rendering marks A are declared with `FLUX_PASS_FLAG_ASYNC_COMPUTE`: `LightClustering`, `VolumetricFog`, `GTAO`, `ContactShadows` ([Z2-N-REND-791]). Every other LIGHT pass (the light list, shadows, sky and sky LUTs, SSR, SSGI, probe capture and filter, deferred lighting, TAA, exposure, DoF, motion blur, bloom, tonemap) is G; making one async-eligible is the Stage 5 RPROD frame-anatomy ticket's decision, not LIGHT's. Results are identical on either queue (`Test_AsyncCompute_LightingIdentical`).

### Lock-order entries

LIGHT adds no `Zenith_Mutex`. Asset-load completions for environments, probes, volumes, lightmap sets and LUTs reach `Flux.BuildGraph` through one bounded queue:

| Queue | Capacity | Full policy |
|---|---|---|
| `flux.lighting.loadQueue` (`Zenith_BoundedQueue`, `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_BoundedQueue"`) | 1,024 | `QUEUE_FULL_POLICY_REJECT`: the loader returns `FLUX_LIGHTING_ERROR_QUEUE_FULL` and the asset system retries |

### Construction and shutdown

`Flux_Renderer` creates `Flux_LightingSystem` after the persistent resources and the GPU scene (RPROD's create step 7b). `Create` runs these steps in order; each has a failure-injection point and the test `Test_LightingSystem_Create_FailsAt<Step>_Unwinds` ([Z2-N-CONTRACT-341]) asserting that exactly the steps before it are undone, in reverse order, and nothing leaks:

| Step | `<Step>` | Action |
|---|---|---|
| 1 | `Allocate` | `Zenith_TryNew` of the system |
| 2 | `DFGLUT` | synchronous load of `<m_strContentRoot>DFG.zllt` (engine content `content:/Engine/Lighting/` by default; the cooked test fixtures in the `z2-light-cooked` cases, "Fixtures") into a persistent read-only texture |
| 3 | `SheenLUT` | `<m_strContentRoot>SheenAlbedo.zllt` (Stage 5) |
| 4 | `LTCLUT` | `<m_strContentRoot>LTCMatrix.zllt` and `LTCMagnitude.zllt` (Stage 5) |
| 5 | `DefaultEnvironment` | synchronous load of `<m_strContentRoot>DefaultEnvironment.zenv` into slice 0 |
| 6 | `LightSlots` | `Flux_GPUScene::ReserveLightSlots(2)` for the sun and moon (Stage 3, Z2-D-131, the Q45 ruling core-doc batch 4 applied; assumption pending RHI, row L30) |
| 7 | `AtlasAllocator` | the shadow-atlas allocator's quadtree storage for the largest tier layout |
| 8 | `ProbeManager` | `Flux_ReflectionProbeManager::Create` (Stage 4) |
| 9 | `BakedGIManager` | `Flux_BakedGIManager::Create` (Stage 5) |
| 10 | `LoadQueue` | `flux.lighting.loadQueue` |
| 11 | `ControllerPool` | `m_uMaxViews` dynamic-resolution controllers |
| 12 | `ExposurePool` | `m_uMaxViews` exposure states |
| 13 | `CounterReadback` | the 256-byte counter readback ring |

| Phase | Action |
|---|---|
| shutdown step 4 | every LIGHT asset load and pending readback is cancelled and completes |
| shutdown step 5 | every LIGHT persistent resource retires through deferred deletion; the two reserved light slots are released |
| destruction | inside `Flux_Renderer`'s destruction, before the RHI device, in reverse construction order |

### Cancellation

| Operation | Cancellation points | Who frees partial results |
|---|---|---|
| probe, volume, lightmap and environment loads | header read, section reads, decompression, queued upload | the owning manager, before the load reports cancelled; a slice or page is assigned only after the upload |
| probe capture | between faces | the manager keeps the old slice contents; the partial capture cube is a transient |
| exposure readback | pending | `Flux_ExposureState` cancels and releases it |
| cook steps | between pipeline steps | the worker exits, outputs discarded |

Each load path has the 1,000-cycle stress test `Test_Probes_LoadCancelStress`.

### Error codes

`Flux_LightingError` ("Flux_LightingErrors header"); cook errors `COOK_ERROR_LIGHTING_LUT`, `COOK_ERROR_ENVIRONMENT_FORMAT`, `COOK_ERROR_LUT_FORMAT`, `COOK_ERROR_LIGHTING_BAKE` join ZenithCook's cook error enumeration (row L13).

### Luau errors and asserts

LIGHT raises no Luau error and has no script binding of its own: scripts change lighting only through component fields, which extraction validates. Asserts guard programmer errors (span sizes, cascade counts); render-world values, configuration and cooked bytes are validated and reported, never asserted ([Z2-N-CONTRACT-472]).

## Diagnostics and profiling

### Counters

Emitted through the profiler hooks once per render frame (`Flux.*` names; GPU-side counters arrive through a 256-byte counter readback with 2 to 3 frames of latency):

| Counter | Meaning |
|---|---|
| `Flux.Lights.Count`, `.Visible`, `.Directional`, `.Dropped`, `.Invalid`, `.UpdateUnknown`, `.RemoveUnknown`, `.FlagIgnored`, `.DirectionalDropped`, `.ClusterOverflow` | lights |
| `Flux.Decals.ClusterOverflow`, `Flux.Probes.ClusterOverflow` | cluster capacities |
| `Flux.Shadows.Views`, `.CascadeViews`, `.LocalViews`, `.Dropped`, `.AtlasTexelsUsedPercent` | shadows (`.Dropped` is the one atlas-exhaustion counter, row L29) |
| `Flux.Probes.Resident`, `.Missing`, `.Captures`, `.Filters`, `.LoadFailures` | probes |
| `Flux.GI.Volumes`, `.LightmapPages`, `.LightmapSlotOverflow`, `.SSGIConfidencePercent` | GI |
| `Flux.Temporal.HistoryNaN`, `.HistoryRejectedPercent` | TAA |
| `Flux.DynRes.ScalePercent`, `.FramesAtFloor`, `.ThermalCapPercent`, `.InvalidSamples` | dynamic resolution |
| `Flux.Exposure.EV100`, `.TargetEV100`, `.Reset` | exposure |
| `Flux.Colour.OutputMode`, `.OutputFallback` | output |
| `Flux.Sky.LUTRebuilds`, `Flux.Sky.Captures` | atmosphere LUT rebuilds (sky revision changes) and sky captures, counted in submitted frames |
| `Flux.Lighting.FramesNotSubmitted` | frames whose outcome was `DEFERRED` or `FAILED` ("Deferred and failed frames") |
| `Flux.Fog.OverridesDropped` | submerged views beyond the 16 medium overrides of a frame |
| `flux.lighting.loadQueue.full`, `.dropped`, `.high_water` | the queue ([Z2-N-CONTRACT-345]) |

`Test_LightingCounters_Emitted` asserts every row.

### Log categories

`flux.lighting`, `flux.shadows`, `flux.gi`, `flux.probes`, `flux.post`, `flux.colour`, `flux.quality` (LIGHT's settings validation) (runtime); `cook.lighting` (cook). Invalid records and load failures log once per key per run at `LOG_LEVEL_WARNING` with the key or asset GUID ([Z2-N-CONTRACT-464]).

### Profiler zones

`Flux.Lighting.ApplyRenderWorld`, `Flux.Lighting.PlanShadows`, `Flux.Lighting.Probes`, `Flux.Lighting.DynRes`, `Flux.Lighting.Exposure`; GPU passes get their timestamps from the graph (`Zenith2/Docs/Rendering/RenderGraph.md → "GPU timestamps"`, Z2-I-RPROD-19).

### Debug views

`r.debug.lighting <n>` selects a `Flux_LightingDebugView`, drawn by LIGHT's `LightingDebug` pass at `FLUX_EXTENSION_POINT_OVERLAY` (a LIGHT `Flux_ViewExtension`, `Flux_LightingDebugExtension` in `Internal/Lighting/LightingDebugExtension.h`, for main and editor views; `Zenith2/Docs/Design/RPROD/Views.md → "View extensions"`) from LIGHT's debug program `Zenith2/Shaders/Passes/Debug/LightingDebug.slang`; DEVT's overlay draws the legend. `Flux_Renderer` registers `Flux_LightingSystem::GetDebugViewExtension()` right after create step 7b, unregisters it before the lighting system is destroyed, and forwards the key's value through `SetDebugView` at `Create` and on every `ApplyConfigChanges` that changes it:

| n | View |
|---|---|
| 1 | cascades: one colour per cascade, blend bands striped |
| 2 | the shadow atlas with tile outlines and light IDs |
| 3, 4 | cluster light and decal counts as a heat map (black 0 … red at capacity, white overflow) |
| 5, 6 | GTAO and contact-shadow masks |
| 7, 8 | SSR hit (green) or miss (red), SSR confidence |
| 9 | froxel slices at a chosen screen row |
| 10 | probe influence boxes and the dominant probe per pixel |
| 11 | the indirect diffuse source per pixel (SSGI, lightmap, volume, sky) |
| 12 | the exposure histogram with the metered range and EV100 |
| 13 | TAA history rejection mask |
| 14 | velocity |
| 15, 16 | diffuse only, specular only |
| 17 | luminance heat map in EV |

### Frame-dump entries

LIGHT adds a `lighting` object to the frame dump (`Zenith2/Docs/Rendering/RenderGraph.md → "Frame dump"`): the frame light counts, the sun and moon slot states, every shadow view (light key, kind, tile, matrices), the cascade splits, the probe slots, the exposure values per view, the dynamic-resolution scale per view and the sky revision.

### Configuration keys

Every LIGHT key is listed in the `Flux_LightSettings.h` comments ("Flux_LightSettings header") with its default and range, registered by `Flux_RegisterLightConfigKeys` from RPROD's `Zenith_RegisterFluxConfigKeys`; the per-tier values are `Zenith2/Docs/Design/LIGHT/QualityTiers.md`. The `render.quality.*` keys are quality-tier keys (row L28); the non-tier keys are `r.output.mode`, `r.hdr.peakNits`, `r.hdr.paperWhiteNits`, `r.hdr.uiNits` (user settings), `r.exposure.fixedDeltaSeconds` (tests, parity and budget runs) and `r.debug.lighting` (development builds). The dynamic-resolution floor's default is generated from `Z2-B-SHIP-011` by the budget-table generator (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` pattern, GEOM row GM13) and is not a key. The froxel grid is three `CONFIG_VALUE_TYPE_INT` keys, `render.quality.froxelGridX`, `…froxelGridY` and `…froxelGridZ`, each 16 to 256; `Flux_ReadLightQualitySettings` validates the three together and keeps the previous grid when any is out of range.

<!-- assumption: pending PLAT, BUDGET -->
A budget run's graphics settings come from its workload definition, never from the user's settings file or a hand-typed command line ([Z2-N-BUDGET-122]). No workload configuration layer exists or is added (the orchestrator's ruling (b), row L41): the Player's budget-run mode (`--budget-run`, [Z2-N-BUDGET-105]; options of `Zenith2/Docs/Design/MOD.md → "Zenith_CommandLine.h"`) receives the workload's values as `--set` arguments from the workload runner, which reach the store at PLAT's command-line layer (`Zenith2/Docs/Design/PLAT/Configuration.md → "Command-line layer"`), above the user layer. LIGHT's part, which it owns and tests: every LIGHT quality value is a `render.quality.*` key read only through `Flux_ReadLightQualitySettings`, so a command-line value of every such key fully determines `m_xLight` whatever the user layer holds (`Test_LightConfig_CommandLineLayerDeterminesQuality`). That the runner passes every `render.quality.*` key and that no other `--set` of one follows is the budget-run mode's, an owner gap row L41 names for the orchestrator.
<!-- /assumption --> The cluster grid and capacities are compile-time constants, not keys (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"`).

## Authoring and cooking

### What LIGHT authors

LIGHT has no editor tool of its own: lights, probes, volumes, world lighting and camera lighting are components authored in EDIT's inspector and placed with EDIT's gizmos; the bake is WORLD's. LIGHT owns the component types, the cook steps and the formats.

### Light component schemas

<!-- assumption: pending MOD, X108 -->
The lighting components are C++-registered engine component types (Q42, ruled (b) by the orchestrator on 2026-09-26; X108 carried the one core-doc sentence that engine component types are C++-registered, which core-doc batch 4 applied; the heading keeps its name because other designs cite it). The structs are "Flux_LightComponents header"; `Zenith_RegisterLightingComponentTypes` in ZenithRuntime (row L33) registers each with `Zenith_TypeRegistry::BeginComponent<T>(szStableName, 1)`, `DefaultAuthority(PHASE_LATE_SCRIPTS)`, one `Property(uPROP_…, szName, &T::m_…)` per row below with `Flags(PROPERTY_FLAG_SERIALIZED)` and the `Range` and `Default` of the struct's comments, the lifecycle hooks of the asset-handle members, then `End()` (`Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"`). The inspector, serialization, migration, `.d.luau` generation and render extraction treat these types exactly as schema types ([Z2-N-ARCH-643]), so a scene document stores the same property IDs either way. If the human overrides Q42 in favour of engine-content schemas, the same stable names, fields and property IDs become `*.schema.json` files and this table is their specification.
<!-- /assumption -->

| Stable name | Struct | Property IDs and names | Extracted into |
|---|---|---|---|
| `Zenith.Lighting.PointLight` | `Flux_PointLightComponent` | 1 Colour, 2 TemperatureKelvin, 3 Intensity, 4 IntensityUnit, 5 RangeMetres, 6 SourceRadiusMetres, 7 CastsShadows, 8 Volumetric, 9 VolumetricScale, 10 LightingChannels, 11 ShadowPriorityBias, 12 ShadowMaxTileTexels, 13 ShadowNormalOffsetTexels, 14 ShadowDepthBiasMetres, 15 SpecularEnabled | `Flux_RenderLightDelta` (type 1) |
| `Zenith.Lighting.SpotLight` | `Flux_SpotLightComponent` | 1–15 as the point light; 16 InnerAngleDegrees, 17 OuterAngleDegrees | type 2 |
| `Zenith.Lighting.DirectionalLight` | `Flux_DirectionalLightComponent` | 1, 2, 7–15 as the point light; 16 IlluminanceLux, 17 AngularRadiusDegrees, 18 ContactShadows | type 0 (a scene directional light; ENV's sun and moon are the environment slots) |
| `Zenith.Lighting.RectLight` (Stage 5) | `Flux_RectLightComponent` | 1–15 (4 IntensityUnit: 0 nits, 1 lumens); 16 WidthMetres, 17 HeightMetres, 18 TwoSided; no property 19 is registered | type 3 |
| `Zenith.Lighting.DiscLight` (Stage 5) | `Flux_DiscLightComponent` | 1–15 (4 IntensityUnit: 0 nits, 1 lumens); 16 RadiusMetres, 17 TwoSided, 18 InnerAngleDegrees, 19 OuterAngleDegrees | type 4 |
| `Zenith.Lighting.ReflectionProbe` (Stage 4) | `Flux_ReflectionProbeComponent` | 1 CaptureMode, 2 BakedAsset, 3 InfluenceHalfExtent, 4 InfluenceOffset, 5 FadeDistanceMetres, 6 ParallaxHalfExtent, 7 Priority, 8 Interior, 9 ExteriorOnly, 10 CaptureEV100, 11 CaptureFarMetres, 12 UpdateIntervalFrames, 13 Intensity, 14 CaptureRequest | `Flux_RenderProbeDelta` |
| `Zenith.Lighting.IrradianceVolume` (Stage 5) | `Flux_IrradianceVolumeComponent` | 1 Asset, 2 Priority, 3 BlendDistanceMetres | the GI volume delta |
| `Zenith.Lighting.LightmapSet` (Stage 5) | `Flux_LightmapSetComponent` | 1 Asset | the lightmap binding delta |
| `Zenith.Lighting.WorldLighting` | `Flux_WorldLightingComponent` | 1 StaticEnvironment, 2 GradingLUT, 3 DefaultEV100, 4 ExposureCompensation, 5 MinEV100, 6 MaxEV100, 7 SkyIntensity | `Flux_WorldLightingBlock` (by the engine environment extractor) |
| `Zenith.Lighting.CameraLighting` | `Flux_CameraLightingComponent` | 1 ExposureMode, 2 ManualEV100, 3 Aperture, 4 ShutterSeconds, 5 ISO, 6 ExposureCompensation, 7 DepthOfField, 8 FocalLengthMm, 9 SensorWidthMm, 10 FocusDistanceMetres, 11 FocusRangeMetres, 12 MaxCoCPixels, 13 MotionBlur, 14 ShutterFraction, 15 MaxBlurPixels, 16 GradingLUT, 17 GradingLUTB, 18 LUTBlend, 19 VignetteIntensity, 20 GrainIntensity | `Flux_ViewRequest::m_xCameraLighting` (row L2) |

- Property 4 `IntensityUnit` has one meaning on every light that registers it: 0 the type's stored unit of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"` (candela for point and spot lights, nits for rect and disc lights), 1 lumens; no light registers a second unit property (row L40 (d)).
- The property-ID constants are `uPROP_POINT_LIGHT_COLOUR` = 1 … in `Flux_LightComponents.h`, one per row entry; an ID is never reused, including after a field's removal ([Z2-N-ARCH-641] and the property-ID row of Architecture's identity table).
- The atmosphere and height-fog parameters are not in `WorldLighting`: they are ENV's components (X125); a world without ENV uses LIGHT's defaults ("Atmosphere ownership" of Volumetrics.md).
- Extraction converts degrees to radians, lumens to the stored unit and Kelvin to a tint (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"`), computes the key with `Flux_MakeEntityLightKey`, and sets a light's direction to its entity's world −Z.
- From Stage 3 the scaffold's 3D template scene names the directional light by `Zenith.Lighting.DirectionalLight` on its `Sun` entity (answers MOD row M18 through row L33); at Stage 2 the template scenes stay transform-only, because the type exists only once LIGHT's Stage 3 component registration lands (orchestrator ruling on X108, M18).
- A type change follows the migration rules of `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` (a version step with a `Migration` of the type builder).

### Cook steps

| Step | Inputs | Output | Derived-data key |
|---|---|---|---|
| `Cook_LightingLUTs` | a `.lutsrc.json` (`{ "table": "DFG" or "SheenAlbedo" or "LTC", "size": n, "samples": s }`) | `.zllt` | the source, the reference-BRDF source hash, the cook version |
| `Cook_Environment` | an `.env.json` (`{ "source": "<file.hdr>" }` or `{ "sky": { "sunDirection": [x, y, z], … } }`) | `.zenv` | the source and the image bytes, the platform (face size) |
| `Cook_GradingLUT` | a `.cube` and its import preset | `.zlut` | the source bytes and preset |
| `Cook_WriteReflectionProbe`, `Cook_WriteIrradianceVolume`, `Cook_WriteLightmapSet` | WORLD's bake output | `.zprb`, `.zgiv`, `.zlmp` + `.ztex` pages | the bake's inputs (WORLD) |

Every step is a pure function of its inputs (the cook step interface, row L13), deterministic byte for byte (`Test_Cook_LightingDeterministic`), and reports into the cook report member `lighting` (sizes, LUT energy check results, clamped `.cube` values).

### Editor surfaces

EDIT supplies the inspector for the component types above, gizmos for light shapes, probe boxes and volume grids, the grading editor (which uses the per-view LUT texture path), and the probe "capture now" action (incrementing `CaptureRequest`); LIGHT supplies the debug views and the `VIEW_KIND_PREVIEW` lighting of the material and asset previews.

### Migration and re-cook

Lighting components are authoring data migrated per `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` (Z2-D-048); every cooked LIGHT format re-cooks on a version change; the `.cube` and `.hdr` sources are never modified.

## Failure handling

### Failure table

| Failure | Behaviour |
|---|---|
| missing history (first frame, cut, output-extent change, device loss) | the no-history path of every temporal consumer; no read of the previous half |
| invalid light, probe or environment record | refused at validation (`Flux_ValidateLightDelta` in both the GPU scene and the mirror), previous state or defaults kept, logged once, counted |
| an invalid sun or moon slot | the slot is inactive for the frame, logged once, counted |
| invalid configuration value | `FLUX_LIGHTING_ERROR_INVALID_SETTINGS`, that struct's previous values kept, logged |
| shadow atlas full | the lowest-priority requests unshadowed for the frame, counted in `Flux.Shadows.Dropped` |
| cluster overflow | truncation in ascending order, counted |
| probe missing, loading, failed, evicted | the fallback chain of `Zenith2/Docs/Design/LIGHT/Reflections.md → "Probe fallback chain"` |
| baked GI missing | the next source of `Zenith2/Docs/Design/LIGHT/GI.md → "Indirect diffuse selection"` |
| corrupt or wrong-version cooked file | `FLUX_LIGHTING_ERROR_CORRUPT_DATA` or `_VERSION_MISMATCH`, the asset treated as missing, logged with its GUID |
| missing or corrupt grading LUT | grading disabled for the view, warning |
| missing or corrupt static environment | the engine default environment |
| missing lighting LUT or default environment at startup | `Flux_LightingSystem::Create` fails; the renderer fails to start with the content error (engine content is part of the build) |
| no ENV atmosphere resource, a star cube not resident, the cloud targets or map unset | LIGHT's atmosphere defaults with height fog off; no stars; no cloud composite; cloud shadow factor 1 |
| NaN or infinity in a history, exposure or fog | sanitised and counted ("The first-write rule for histories", "Adaptation") |
| unsupported HDR output | SDR, warning, counter |
| allocation failure | `FLUX_LIGHTING_ERROR_OUT_OF_MEMORY` from creation paths; a probe or GI load that cannot allocate is FAILED |
| memory pressure | exactly Architecture's Rendering actions that touch LIGHT (9 SSR, DoF and motion blur off; 10 the fog grid drops one step; 11 the floor scale; 12 capture and preview histories dropped) with Rendering's mechanisms ([Z2-N-REND-506]); LIGHT sheds nothing else |
| a frame deferred or failed at the upload cap | LIGHT writes no ring byte and advances no frame state ("Build-node upload data", "Deferred and failed frames"); a pending LUT rebuild or capture face repeats in the next built frame |
| device loss | `OnDeviceLost` invalidates every history and probe slot; baked data reloads through the loaders, realtime probes re-capture, the default content reloads, the atmosphere LUTs rebuild; `Test_Lighting_DeviceLossRecovery` |
| budget breach | a GPU row breach is BR-ACCEPT; a run at the dynamic-resolution floor that misses its target fails ([Z2-N-BUDGET-088]) |
| thermal throttling | the cap and tier offset of `Zenith2/Docs/Design/LIGHT/Temporal.md → "Thermal and pressure inputs"`, presentation only |

## Test strategy

### Test catalogue index

The catalogue is `Zenith2/Docs/Design/LIGHT/Tests.md`, with headings "Light model unit tests", "Shadow unit tests", "Temporal unit tests", "Exposure and post unit tests", "Colour unit tests", "GI and probe unit tests", "Configuration and component unit tests", "Format and fuzz tests", "Cook and bake tests", "Render tests", "ENV hook render tests", "Parity scenes", "Analytical BRDF tests", "Path-tracer comparison scenes", "Approximation error scenes", "Temporal sequence tests", "Diagnostics tests", "Failure and stress tests" and "Device runs". Every test name a requirement's evidence cites is in one of those tables with the case it asserts, except the tests of other areas the catalogue names by owner (Rendering's `Test_Paths_*`, `Test_View_*`, `Test_Memory_*`, `Test_Swapchain_*`; PLAT's `Test_Thermal_*`; ENV's `Test_Clouds_*`, `Test_Atmosphere_*`).

### Gate steps per test group

| Group | Gate step |
|---|---|
| unit, render, format, temporal, failure tests | `Test`, on WARP and lavapipe, sync validation clean |
| shader lints | `ShaderChecks` |
| backend parity scenes | `BackendParity` (from Stage 3) |
| path parity scenes | `PathParity` (from Stage 5) |
| fuzz smoke | `Sanitizers -Preset win-asan` |
| path-tracer convergence, device runs, budget runs | the Stage 5 (and Stage 3 for the S24 lighting) acceptance suites, run by a human through `Run-Committed.ps1 -Acceptance` (Z2-D-083) |

### Fixtures

No LIGHT fixture is a committed binary, and no LIGHT render test compares against a committed reference image (Q55): every fixture is built in code by the test that uses it, is committed text, or is cooked at test time by the one LIGHT fixture setup.

| Fixture set | Generation |
|---|---|
| the 4,096-light list | `Flux_MakeLightListFixture` (fixed seed, `Zenith_RandomSource`) in `Zenith2/Engine/ZenithFlux/Tests/Lighting/` |
| the analytic shadow scenes (plane on plane, sloped plane, two adjacent tiles) | `Flux_BuildShadowTestScene` in `Zenith2/Engine/ZenithFlux/Tests/Shadows/ShadowSamplingTests.cpp` |
| the fog, counter and device-loss test views | `Flux_BuildFogTestView`, `Flux_BuildCounterTestWorld` and `Flux_BuildDeviceLossTestView`, in-code builders in their tests |
| the 16²-per-face SH cube, the 8²-per-face star cube and the 192 × 108 cloud-layer pair | synthesised in the test from fixed SH coefficients or constants |
| the corrupt-file sets of every LIGHT format | mutations each test applies to a copy of a valid file its writer produced in memory |
| the `.cube` and RGBE grammar cases | string literals and in-memory byte arrays (`Cook_TestMakeRGBE`) in the cook tests |
| tier values | the committed tier fragments `Zenith2/Engine/ZenithFlux/Internal/Config/Quality/<TierID>.config.json` |
| parity, path-tracer and approximation-error scenes | committed text: `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/Lighting*.scene.json` with `Tests/Parity/Tolerances.json`, `Tests/PathTracer/Scenes/PT_*.scene.json` and `AE_*.scene.json`, and the text bake description `Tests/Parity/Scenes/LightingBakedGI.bake.json` |
| cooked LIGHT content (fixture `LightCookedFixtures`): the DFG table, the default environment, the identity grading LUT, a 17³ `.cube` LUT of per-channel gamma 0.8, and from Stage 5 the sheen-albedo and LTC tables and the `LightingBakedGI` irradiance volume, lightmap set and pages | generated at test time, never committed: the case `Fixture_LightCooked_Generate` of `ZenithCook_Tests` (doctest suite `z2-fixture-setup`, file `Zenith2/Tools/ZenithCook/Tests/Lighting/Zenith_LightFixtures_Generate.cpp`) empties and rewrites the directory the environment variable `ZENITH2_LIGHT_COOKED_FIXTURES` names, `${ZENITH_OUT}/test-fixtures/LIGHT/`, and fails naming the variable when it is unset. The case and both registry entries below are created with the DFG table's cook step (`DFG.lutsrc.json` → `DFG.zllt`, Stage 3); each later cook step extends the same case with its own file: `Cook_Environment` (`DefaultEnvironment.env.json` → `DefaultEnvironment.zenv`), `Cook_GradingLUT` (`Identity.gradinglut.json` → `Identity.zlut`), the Stage 3 post-chain integration (the `.cube` it writes as text → `Gamma08.zlut`), the Stage 5 lighting tables (`SheenAlbedo.lutsrc.json` and `LTC.lutsrc.json` → `SheenAlbedo.zllt`, `LTCMatrix.zllt`, `LTCMagnitude.zllt`) and the Stage 5 baked GI (`LightingBakedGI.bake.json` through `Cook_WriteIrradianceVolume` and `Cook_WriteLightmapSet`). From the DFG table's step on, every `ZenithFlux_Tests` case that calls `Flux_LightingSystem::Create`, or reads a cooked LIGHT file, is in `TEST_SUITE("z2-light-cooked")` and passes `Flux_LightingSystemDesc::m_strContentRoot` = the value of `ZENITH2_LIGHT_COOKED_FIXTURES` followed by `/`, so `Create` steps 2–5 load the generated files. The test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` registers it as the CTest entry `LIGHT_CookedFixtures` with `FIXTURES_SETUP LightCookedFixtures`, and the doctest suite `z2-light-cooked` of `ZenithFlux_Tests` as the entry `ZenithFlux_Tests[z2-light-cooked]` with `FIXTURES_REQUIRED LightCookedFixtures`, both with `ENVIRONMENT ZENITH2_LIGHT_COOKED_FIXTURES=${ZENITH_OUT}/test-fixtures/LIGHT` ([Z2-N-ARCH-257]). `ZenithCook_Tests` is built only where the host tools are (the `win` preset), so both entries are registered in `win` alone, the rule cross-document item X191 proposes; in `win-asan` and `linux-tsan` neither is registered, no LIGHT fuzz smoke requires the set (each LIGHT fuzz target seeds itself from in-code seeds), and no case outside `z2-light-cooked` reads it. A consuming case that mutates a cooked file copies it into a subdirectory of its own first |

## Budget rows

### Rows owned

| Row | Name | Measured by |
|---|---|---|
| Z2-B-LIGHT-001 | Light and decal list build GPU | `LightDecalListBuild` |
| Z2-B-LIGHT-002 | Shadow views total GPU | every shadow view's passes |
| Z2-B-LIGHT-003 | Light clustering GPU | `LightClustering` |
| Z2-B-LIGHT-004 | Volumetric fog GPU | `VolumetricFog` (and local fog volumes) |
| Z2-B-LIGHT-005 | SSAO GTAO and denoise GPU | `GTAO` |
| Z2-B-LIGHT-006 | Screen-space shadows GPU | `ContactShadows` |
| Z2-B-LIGHT-007 | SSR trace denoise and composite GPU | `SSR`, plus the specular-weight target's store on the forward paths (row L34) |
| Z2-B-LIGHT-008 | Deferred lighting GPU | `DeferredLighting` on Deferred |
| Z2-B-LIGHT-009 | Visibility buffer lighting GPU | `DeferredLighting` on the visibility buffer |
| Z2-B-LIGHT-010 | TAA or temporal upscaler GPU | `TemporalAA` |
| Z2-B-LIGHT-011 | Auto-exposure GPU | `AutoExposure` |
| Z2-B-LIGHT-012 | Depth of field GPU | `DepthOfField` |
| Z2-B-LIGHT-013 | Motion blur GPU | `MotionBlur` |
| Z2-B-LIGHT-014 | Bloom GPU | `Bloom` |
| Z2-B-LIGHT-015 | Tonemap and colour grading GPU | `TonemapAndGrade` |
| Z2-B-LIGHT-016 | SSGI GPU | `SSGI` (Windows only) |
| Z2-B-LIGHT-017 | Probe capture and filter GPU | the probe capture views, `ProbeFilter` and `SkyCapture` of a frame |
| Z2-B-LIGHT-018 | Reflection probe and environment memory | the environment cube array, probe records and capture transients (MP-SUBMEM) |
| Z2-B-LIGHT-019 | Baked GI resident memory | resident irradiance volumes and lightmap pages (MP-SUBMEM) |
| Z2-B-LIGHT-020 | LIGHT CPU on Flux.BuildGraph | LIGHT's build-node work, measured inside the RHI graph-build span (MP-CPUZONE) |
| Z2-B-LIGHT-021 | Shadowed local lights per frame per tier | `render.quality.maxShadowedLights` per tier; excess lights counted in `Flux.Shadows.Dropped` (MP-COUNTER, BR-DROP) |
| Z2-B-LIGHT-022 | SSGI error budget | the four criteria of `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI error budget"` (MP-APPROX, BR-TEST) |

All are pending until the Stage 1 per-system budget ticket for LIGHT allocates them ([Z2-N-REND-796]); every LIGHT ticket citing one is blocked by that ticket. Rows 016 to 022 are the row needs core-document batch 4 added to `Zenith2/Docs/Budgets.md`.

### Rows measured against

| Row | Relevance |
|---|---|
| Z2-B-ENV-001 Sky and atmosphere GPU | LIGHT's sky passes, measured together with ENV's cloud layer |
| Z2-B-SHIP-011 Dynamic-resolution floor | the controller's floor (adopted: ≥ 75 per axis) |
| Z2-B-SHIP-012 Hitch threshold, Z2-B-SHIP-015, Z2-B-SHIP-017 | probe captures and LUT recomputation must not hitch |
| Z2-B-RPROD-001 | the GPU-scene tables (and the instance lighting table if X106 is accepted) |
| Z2-B-RPROD-022, Z2-B-RPROD-023 | the view caps that bound shadow views and captures |
| Z2-B-RPROD-027 Input-to-photon latency | the exposure and dynamic-resolution latency add nothing to it |
| Z2-B-RPROD-031 Per-scene parity tolerances | LIGHT's parity scene proposals |
| Z2-B-ADVGFX-004 … Z2-B-ADVGFX-008 | the path-tracer comparison scenes |
| Z2-B-RHI-001, Z2-B-RHI-010 | histories and the atlas, fog and post transients |
| Z2-B-CORE-001, Z2-B-CORE-003 | the S24 RSS and Windows VRAM totals |

### Pending row needs

The seven row needs this family recorded (cross-area rows L25 and L34) are `Zenith2/Docs/Budgets.md` rows since core-document batch 4, pending with producer "Stage 1 per-system budget ticket for LIGHT":

| Row | Unit | Workloads |
|---|---|---|
| Z2-B-LIGHT-016 SSGI GPU | ms | W-Cinematic, W-Indoor (Windows) |
| Z2-B-LIGHT-017 Probe capture and filter GPU | ms per frame | WS-3D |
| Z2-B-LIGHT-018 Reflection probe and environment memory | MB | WS-3D |
| Z2-B-LIGHT-019 Baked GI resident memory | MB | W-OpenWorld, W-Indoor |
| Z2-B-LIGHT-020 LIGHT CPU on Flux.BuildGraph | ms | WS-3D |
| Z2-B-LIGHT-021 Shadowed local lights per frame per tier | lights | W-Indoor |
| Z2-B-LIGHT-022 SSGI error budget | the four criteria of `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI error budget"` | the approximation error scenes |

The `QUALITY_TIER_S24_HIGH` value of `Z2-B-LIGHT-007` (the one S24 tier with SSR) includes the forward opaque pass's store of the specular-weight target (4 bytes per render pixel per frame; `Zenith2/Docs/Design/LIGHT/Reflections.md → "SSR composite"`); tiers without SSR carry no such cost (row L34).

## Stage plan

### Stage table

| Stage | LIGHT delivers | Gated by |
|---|---|---|
| S1 | the LIGHT rows of `QualityMatrix.md` (`Zenith2/Docs/Design/LIGHT/QualityTiers.md → "QualityMatrix rows supplied by LIGHT"`); the Stage 1 per-system budget ticket for LIGHT allocating Z2-B-LIGHT-001 … 022 | the Stage 1 QualityMatrix and budget tickets |
| S2 | temporal-history management (Z2-I-LIGHT-08): the history rules, validity state machine, first-write rule and the API-only slice of every LIGHT header (declarations, contract blocks, feature functions that add no pass) | RPROD's view identity (Z2-I-RPROD-08), D-SLANG (open, Stage 1) for resource fields; the X103 core-doc batch for the additions the slice declares |
| S3 | the Forward+ lighting set of the playable slice on both APIs and the S24: light model, units and keys, light list, clustering, `EvaluateLighting` and the BRDF, CSM and local shadows with the atlas and `PlanShadows`, contact shadows, GTAO, froxel fog, sky and atmosphere LUTs, the static IBL, SSR, TAA and TAAU, dynamic resolution, auto exposure, bloom, SDR tonemap and grading, the lighting component types, the sun and moon slots that ENV fills for the W-Outdoor workload (moved from S4 by the Q45 ruling, Z2-D-131); backend parity scenes; the S3 tiers (WIN_DEFAULT, S24_DEFAULT) | Stage 2 graph and views; the pending LIGHT rows; X103, X104, X108 applied; for the slots, Q45 in core-doc batch 4, X125 and RHI row L30 |
| S4 | reflection probes (capture, filter, blending, residency and streaming integration), sky capture, the ENV hooks (star field, cloud-layer composite, underwater override), the baked GI cooked formats and the bake output contract for WORLD's bake workflow | ASSET loaders, STREAM cells, ENV's atmosphere and cloud layer (X125) |
| S5 | baked GI runtime evaluation (volumes, lightmaps), SSGI with its error budget, deferred and visibility-buffer lighting with path parity, area lights, advanced lobes (GEOM GM9, X80), DoF, motion blur, HDR10 and scRGB output, local fog volumes, the cloud shadow on the sun, the full per-platform tier set and fallbacks, the thermal coupling, the path-tracer comparison and approximation-error suites, D-SEL-UPSCALER | D-TBDR (open, Stage 5) revalidation of the S5 LIGHT tickets; the path tracer (ADVGFX); X105 (SSGI), X106 (lightmap binding, ruling requested), Q41 (SSGI scope, ruled (a) on 2026-10-16), X107 (D-SEL-UPSCALER registered) |
| S6 | sustained thermal runs of the tiers on the S24 (evidence only) | SHIP |
| S7 | nothing: RT lighting is ADVGFX's deferred scope; SSGI stays in Stage 5 under ruling Q41 (a) (`Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"`) | — |

Stage 5 LIGHT tickets that depend on no unresolved Stage 5 decision (the formats already landed, area lights, the advanced lobes) may be pulled into the Stage 4 sprint with fix version `Z2 Stage 5` (`Zenith2/Docs/Roadmap.md → "Pull-forward rule"`).

### API-only slices and ordering

| Order | Slice |
|---|---|
| 1 | S2: the shading-module stub slice first (`Zenith2/Shaders/Lighting/EvaluateLighting.slang`, `Shadows.slang` and `IBL.slang` with every declaration and the constructor signatures of `Zenith2/Docs/Design/LIGHT/Lighting.md → "The two light sources"`, stub bodies returning emissive × pre-exposure, shadow 1 and environment 0, plus the `[FluxShared]` modules `Common/Media.slang` and `Common/Probes.slang`), so RPROD's Forward+ programs compile against final declarations (orchestrator ruling Q65 (a) as refined by Q66); then `Flux_LightingErrors.h`, `Flux_Upscaler.h`, `Flux_ShadowAtlas.h`, `Flux_LightSettings.h`, `Flux_LightComponents.h`, `Flux_Lighting.h` and every feature header, declarations only, feature functions adding no pass; `Flux_RegisterLightConfigKeys` |
| 2 | S2: history rules in `Flux_LightingSystem` and their tests |
| 3 | S3: light model (units, keys, deltas, validation, records; the brightest scene directional light takes the sun role in a world without environment slots) → the sun and moon slots (reserved light-table slots 0 and 1, `Flux_ValidateEnvironmentLight`, the slot flag contract, `Create` step 6; moved from S4 by the Q45 ruling, Z2-D-131) → light list → clustering → `EvaluateLighting` and BRDF with the DFG LUT → shadows (allocator, `PlanShadows`, cascades, local, contact) → GTAO → fog → sky → IBL static → SSR → TAA → exposure → bloom → tonemap → dynamic resolution → component registration → tiers → parity scenes |
| 4 | S4: probe formats and manager → capture → filter → blending → streaming; the ENV hooks; baked GI formats and writers |
| 5 | S5: baked GI evaluation (the binding of the X106 ruling) → SSGI (WIN_HIGH and WIN_EPIC, ruling Q41 (a)) → deferred lighting → area lights → lobes → DoF, motion blur → HDR output → local fog volumes → cloud shadow → tiers and fallbacks → path-tracer suites |

## Decisions

### Settled decisions

Z2-D-012 (Budgets v1: the dynamic-resolution floor), Z2-D-013 (GPU baseline, `shaderFloat16`), Z2-D-014 (GPU performance rules: half precision, formats), Z2-D-015 (Slang source of truth), Z2-D-019 (determinism: lighting is presentation only), Z2-D-020 (data-defined components: game types only; LIGHT's engine types are C++-registered under Q42/X108), Z2-D-022 (a settled choice is challenged only as a flagged proposal: LIGHT-1), Z2-D-025 (offline shaders), Z2-D-035 (Forward+ default, no MSAA, single-source lighting), Z2-D-036 (TBDR), Z2-D-037 (path tracer), Z2-D-038 (data formats), Z2-D-043 (RT deferred), Z2-D-048 (compatibility), Z2-D-050 (frame pipelining), Z2-D-054 (bindless), Z2-D-055 (render graph), Z2-D-056 (barriers), Z2-D-057 (transient aliasing), Z2-D-058 (GPU scene tables; the fifth table is flagged, X106), Z2-D-059 (render views and per-view histories), Z2-D-060 (HiZ min channel for SSR), Z2-D-061 (depth modes; shadow views standard Z), Z2-D-063 (validation roles), Z2-D-064 (research methodology), Z2-D-072 (XR-ready views), Z2-D-076 (no split-screen), Z2-D-079 (materials in code), Z2-D-084 (measurement split), Z2-D-085 (living docs), Z2-D-097 (rulings in the ticket diff).

### Open decisions

D-SLANG (open, Stage 1): every LIGHT pass's resource fields; D-SEL-HASH (open, Stage 1): light and probe keys and view IDs; D-TBDR (open, Stage 5): its revalidation blocks the Stage 5 LIGHT tickets; D-VTX-TANGENTLESS (open, Stage 3): the tangent frame the lighting modules receive.

Q41 is ruled (a) by the human on 2026-10-16 (SSGI on WIN_HIGH and WIN_EPIC; `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"`). Pending human rulings that change LIGHT's scope or shape: X106 with flagged proposal LIGHT-1 (the fifth GPU-scene table; `Zenith2/Docs/Design/LIGHT/GI.md → "Lightmap binding"` states both bindings). Q42 is ruled (b) by the orchestrator and may be overridden by the human ("Light component schemas" states the alternative).

### Selection decisions LIGHT owns

D-SEL-UPSCALER (open, Stage 5): the third-party temporal upscaler, with candidates, criteria and acceptance rule in `Zenith2/Docs/Design/LIGHT/Temporal.md → "Upscaler selection"`; a harness ticket and a `needs-human` ruling ticket (Z2-D-084); its registration in `Zenith2/Docs/DecisionLog.md` and `Zenith2/Docs/Architecture.md → "Named selection decisions"` is cross-doc item X107 (applied by core-doc batch 4) and row L19.

### Design choices this family makes

These are elaborations within LIGHT's authority (each marked `<!-- elaboration -->` where it is specified), not rulings: none is a `Z2-D-` entry, none changes a settled decision, and a ticket cites one by this heading and its `LR` number. The `LR` numbering is separate from the cross-area rows `L1` … `L38`.

| Choice | Content |
|---|---|
| LR1 | every shadow view renders every frame into a transient atlas; no cached, static or virtual shadow maps |
| LR2 | one shadow atlas per frame shared by cascades and local lights |
| LR3 | the coupled, energy-conserving diffuse term of `Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"` instead of Lambert or Burley, so the furnace and reciprocity tests hold by construction; it reduces exactly to Lambert at F0 = 0 ("Closed-form reductions" of Lighting.md) |
| LR4 | lighting channels in instance bits 24–31 (row L4) |
| LR5 | no IES photometric profiles in v1; lights are photometric through their units and cones |
| LR6 | area-light shadows cover a 120° cone; beyond it the light is unshadowed |
| LR7 | circular bokeh only |
| LR8 | content is Rec.709 gamut; HDR extends luminance, not gamut |
| LR9 | UI blends in the output encoding on HDR outputs |
| LR10 | SSR keeps no history of its own; temporal stability comes from TAA |
| LR11 | pre-exposure uses the exposure read back from the GPU, and tonemap applies the current GPU exposure |
| LR12 | the froxel volume's far distance is a tier constant and the sky-view LUT starts there, so aerial perspective is counted once |
| LR13 | a dynamic-resolution step invalidates no output-extent history (row L18, X104) |
| LR14 | the cluster grid and capacities are compile-time constants per capability tier, not configuration keys |
| LR15 | a submerged view's per-channel water transmittance is reconstructed from the luminance transmittance by the exponent ratio, exact for the homogeneous water medium |

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/LIGHT.md` and the design headings that specify it (the same list as its `Design:` line); a heading of this file is cited by its text alone.

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-LIGHT-001 | LIGHT code lives in ZenithFlux and the lighting shader modules | "ZenithFlux LIGHT files"; "Shader modules and passes"; "Target summary" |
| Z2-R-LIGHT-002 | LIGHT public headers carry contracts and are self-contained | "Header map"; "Rules across the LIGHT API"; "Flux_LightingErrors header"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Flux_Upscaler header" |
| Z2-R-LIGHT-003 | API-only slice lands first | "API-only slices and ordering"; "Header map" |
| Z2-R-LIGHT-004 | Feature functions keep Rendering signatures | "Rules across the LIGHT API"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Flux_Lights header"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Flux_Shadows header" |
| Z2-R-LIGHT-005 | Settings reach features through the view context | "Flux_LightSettings header"; "Rules across the LIGHT API" |
| Z2-R-LIGHT-006 | LIGHT quality keys registered under render.quality | "Flux_LightSettings header"; "Configuration keys"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Tier values and configuration" |
| Z2-R-LIGHT-007 | Histories live in per-view persistent state | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Histories LIGHT owns" |
| Z2-R-LIGHT-008 | First-write rule for histories | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "The first-write rule for histories"; "History validity state machine"; "Deferred and failed frames" |
| Z2-R-LIGHT-009 | History invalidation events | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "History invalidation"; "History validity state machine" |
| Z2-R-LIGHT-010 | Dynamic-resolution steps keep output-extent histories | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Dynamic resolution and history validity" |
| Z2-R-LIGHT-011 | History ping-pong writes are marked outputs | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Histories LIGHT owns" |
| Z2-R-LIGHT-012 | Teleport of the camera is a camera cut | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "History invalidation" |
| Z2-R-LIGHT-013 | Teleported instances produce zero velocity and no smear | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "History invalidation"; `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Motion blur" |
| Z2-R-LIGHT-014 | Halton jitter sequence | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Jitter sequence"; "Flux_Lighting header" |
| Z2-R-LIGHT-015 | Variance clipping in weighted YCoCg | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "TAA resolve" |
| Z2-R-LIGHT-016 | Velocity dilation and bicubic reprojection | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "TAA resolve" |
| Z2-R-LIGHT-017 | Disocclusion rejects history | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "TAA resolve" |
| Z2-R-LIGHT-018 | Anti-flicker weighting and NaN sanitisation | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "TAA resolve"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "The first-write rule for histories" |
| Z2-R-LIGHT-019 | Temporal upscale to output extent | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Temporal upscale" |
| Z2-R-LIGHT-020 | Upscaler is a quality-tier setting | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Upscaler selection"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Flux_Upscaler header"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Temporal and resolution tiers" |
| Z2-R-LIGHT-021 | Temporal convergence and stability sequences | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Temporal sequence tests"; `Zenith2/Docs/Design/LIGHT/Tests.md` → "Parity scenes" |
| Z2-R-LIGHT-022 | Dynamic resolution follows GPU frame time | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Dynamic-resolution controller" |
| Z2-R-LIGHT-023 | Scale bounded by the 75% floor and the cap | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Dynamic-resolution controller"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Thermal and pressure inputs" |
| Z2-R-LIGHT-024 | Step limits, hysteresis and quantisation | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Dynamic-resolution controller" |
| Z2-R-LIGHT-025 | Thermal cap and tier offset drive resolution | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Thermal and pressure inputs" |
| Z2-R-LIGHT-026 | Critical memory pressure forces the floor | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Thermal and pressure inputs" |
| Z2-R-LIGHT-027 | Dynamic-resolution telemetry | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Dynamic-resolution telemetry" |
| Z2-R-LIGHT-028 | Workload runs fix dynamic-resolution settings | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Dynamic-resolution controller"; "Configuration keys" |
| Z2-R-LIGHT-029 | Luminance histogram of absolute luminance | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Luminance histogram" |
| Z2-R-LIGHT-030 | Metering and percentile average | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Metering" |
| Z2-R-LIGHT-031 | Exponential adaptation with reproducible time step | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Adaptation" |
| Z2-R-LIGHT-032 | Exposure buffer and consistent pre-exposure | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Exposure buffer"; `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Pre-exposure pipeline" |
| Z2-R-LIGHT-033 | Manual and physical camera exposure | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Camera exposure"; `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Exposure units" |
| Z2-R-LIGHT-034 | Capture and preview views use fixed exposure | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Camera exposure" |
| Z2-R-LIGHT-035 | Scene-referred Rec.709 working space | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Working colour space" |
| Z2-R-LIGHT-036 | ACES-style SDR operator | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "SDR tonemap operator" |
| Z2-R-LIGHT-037 | 3D grading LUT application | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Grading LUT" |
| Z2-R-LIGHT-038 | Grading LUT cook from .cube | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Grading LUT cook" |
| Z2-R-LIGHT-039 | SDR output through the sRGB swapchain | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Output transforms" |
| Z2-R-LIGHT-040 | HDR10 output | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "HDR tonemap operator"; `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Output transforms" |
| Z2-R-LIGHT-041 | scRGB output | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Output transforms" |
| Z2-R-LIGHT-042 | Output mode selection and SDR fallback | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Output mode selection" |
| Z2-R-LIGHT-043 | HDR calibration and metadata | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "HDR calibration and metadata" |
| Z2-R-LIGHT-044 | Optional vignette and deterministic film grain | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Vignette and film grain" |
| Z2-R-LIGHT-045 | Colour-space conversions match references | `Zenith2/Docs/Design/LIGHT/ColourManagement.md` → "Colour-space matrices" |
| Z2-R-LIGHT-046 | Bloom chain | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Bloom" |
| Z2-R-LIGHT-047 | Bloom soft threshold | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Bloom" |
| Z2-R-LIGHT-048 | Circle of confusion | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Depth of field" |
| Z2-R-LIGHT-049 | Depth-of-field gather and composite | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Depth of field" |
| Z2-R-LIGHT-050 | Depth of field per camera | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Depth of field"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Fallbacks and triggers" |
| Z2-R-LIGHT-051 | Motion blur tile and neighbour maxima | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Motion blur" |
| Z2-R-LIGHT-052 | Motion blur reconstruction gather | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Motion blur" |
| Z2-R-LIGHT-053 | Motion blur never smears cuts or teleports | `Zenith2/Docs/Design/LIGHT/PostProcess.md` → "Motion blur" |
| Z2-R-LIGHT-054 | Photometric light units | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light units" |
| Z2-R-LIGHT-055 | Light record encodings per type | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light record encodings" |
| Z2-R-LIGHT-056 | Light deltas maintain the CPU mirror and the light table | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light extraction and the CPU light mirror"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Division of the delta work with the GPU scene" |
| Z2-R-LIGHT-057 | Deterministic light and decal list | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light and decal list build" |
| Z2-R-LIGHT-058 | Froxel light clusters | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light clustering"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Cluster grid"; "Rendering table additions pending X103" |
| Z2-R-LIGHT-059 | Single-source EvaluateLighting | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "The EvaluateLighting contract" |
| Z2-R-LIGHT-060 | GGX BRDF with the precision split | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "BRDF" |
| Z2-R-LIGHT-061 | Energy-conserving coupled BRDF | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "BRDF" |
| Z2-R-LIGHT-062 | Punctual attenuation and cones | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light attenuation and shape" |
| Z2-R-LIGHT-063 | Lighting channels | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Lighting channels" |
| Z2-R-LIGHT-064 | Rect and disc area lights | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light attenuation and shape"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Lighting lookup tables" |
| Z2-R-LIGHT-065 | Advanced surface lobes in EvaluateLighting | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Advanced surface lobe integration" |
| Z2-R-LIGHT-066 | Deferred and visibility-buffer lighting | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Deferred lighting pass" |
| Z2-R-LIGHT-067 | Cooked lighting LUTs | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Lighting lookup tables"; "Cook steps" |
| Z2-R-LIGHT-068 | Independent CPU reference BRDF tests | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Analytical BRDF tests"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Closed-form reductions" |
| Z2-R-LIGHT-069 | Shadow atlas | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadow atlas"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Atlas layout" |
| Z2-R-LIGHT-070 | Deterministic atlas allocation | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Atlas allocation"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Atlas tile sizing" |
| Z2-R-LIGHT-071 | Practical cascade splits | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Cascaded shadow maps" |
| Z2-R-LIGHT-072 | Stable cascades | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Cascaded shadow maps" |
| Z2-R-LIGHT-073 | Cascade depth range and pancaking | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Cascaded shadow maps"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadow depth rendering" |
| Z2-R-LIGHT-074 | Cascade selection, blend and fade | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Cascaded shadow maps"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadow sampling in shaders" |
| Z2-R-LIGHT-075 | Spot shadow views | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Spot shadow views" |
| Z2-R-LIGHT-076 | Point shadow views | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Point shadow views" |
| Z2-R-LIGHT-077 | Shadow view record layout | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadow view record" |
| Z2-R-LIGHT-078 | Shadow filtering levels | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Filtering" |
| Z2-R-LIGHT-079 | Shadow bias | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Bias"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadow depth rendering" |
| Z2-R-LIGHT-080 | Shadowed-light budget | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadowed-light budget" |
| Z2-R-LIGHT-081 | Screen-space contact shadows | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Contact shadows"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Evaluation order"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Sun and moon slots" |
| Z2-R-LIGHT-082 | Area light shadows | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Area light shadow views" |
| Z2-R-LIGHT-083 | GTAO | `Zenith2/Docs/Design/LIGHT/GI.md` → "GTAO" |
| Z2-R-LIGHT-084 | GTAO denoise and upsample | `Zenith2/Docs/Design/LIGHT/GI.md` → "GTAO denoise and upsample" |
| Z2-R-LIGHT-085 | AO application | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "BRDF"; `Zenith2/Docs/Design/LIGHT/GI.md` → "GTAO denoise and upsample" |
| Z2-R-LIGHT-086 | Froxel grid and slice distribution | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Froxel grid" |
| Z2-R-LIGHT-087 | Scattering injection | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Scattering injection"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Media model" |
| Z2-R-LIGHT-088 | Energy-conserving front-to-back integration | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Integration" |
| Z2-R-LIGHT-089 | Temporal froxel reprojection | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Temporal reprojection" |
| Z2-R-LIGHT-090 | Fog applied in-shader | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Fog application" |
| Z2-R-LIGHT-091 | Transmittance LUT | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Transmittance LUT" |
| Z2-R-LIGHT-092 | Multiple-scattering LUT | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Multiple-scattering LUT" |
| Z2-R-LIGHT-093 | Sky-view LUT | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Sky-view LUT" |
| Z2-R-LIGHT-094 | Sky draw with aerial perspective counted once | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Sky draw and aerial perspective"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Sun transmittance" |
| Z2-R-LIGHT-095 | Local fog volumes | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Local fog volumes" |
| Z2-R-LIGHT-096 | SSR ray generation | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "SSR ray generation" |
| Z2-R-LIGHT-097 | Hierarchical trace on the HiZ min channel | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "SSR hierarchical trace" |
| Z2-R-LIGHT-098 | SSR hit validation and confidence | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "SSR hit validation" |
| Z2-R-LIGHT-099 | SSR denoise | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "SSR denoise" |
| Z2-R-LIGHT-100 | SSR composite replaces environment specular | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "SSR composite" |
| Z2-R-LIGHT-101 | IBL split-sum evaluation | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "IBL evaluation"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Environment cube array" |
| Z2-R-LIGHT-102 | Static environment | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Static environment" |
| Z2-R-LIGHT-103 | Reflection probe cooked format | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Reflection probe cooked format"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe info record" |
| Z2-R-LIGHT-104 | Probe capture through capture views | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe capture" |
| Z2-R-LIGHT-105 | GGX probe prefilter | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe filtering" |
| Z2-R-LIGHT-106 | Probe blending and parallax | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe blending"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Reflection probe record" |
| Z2-R-LIGHT-107 | Probe residency and streaming integration | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe residency and streaming"; "Probe residency state machine" |
| Z2-R-LIGHT-108 | Probe fallback chain | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe fallback chain" |
| Z2-R-LIGHT-109 | Dynamic sky capture | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Sky capture" |
| Z2-R-LIGHT-110 | Irradiance volume cooked format | `Zenith2/Docs/Design/LIGHT/GI.md` → "Irradiance volume cooked format" |
| Z2-R-LIGHT-111 | Lightmap set cooked format | `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmap set cooked format" |
| Z2-R-LIGHT-112 | Bake output contract for WORLD | `Zenith2/Docs/Design/LIGHT/GI.md` → "Bake output contract" |
| Z2-R-LIGHT-113 | Irradiance volume sampling | `Zenith2/Docs/Design/LIGHT/GI.md` → "Irradiance volume sampling" |
| Z2-R-LIGHT-114 | Lightmap evaluation | `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmap evaluation" |
| Z2-R-LIGHT-115 | Lightmap binding and deferred paths | `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmap binding"; `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmaps on the deferred paths" |
| Z2-R-LIGHT-116 | Baked GI residency and streaming | `Zenith2/Docs/Design/LIGHT/GI.md` → "Baked GI residency and streaming" |
| Z2-R-LIGHT-117 | SSGI | `Zenith2/Docs/Design/LIGHT/GI.md` → "SSGI"; `Zenith2/Docs/Design/LIGHT/GI.md` → "SSGI platform scope" |
| Z2-R-LIGHT-118 | SSGI documented error budget | `Zenith2/Docs/Design/LIGHT/GI.md` → "SSGI error budget"; `Zenith2/Docs/Design/LIGHT/Tests.md` → "Approximation error scenes" |
| Z2-R-LIGHT-119 | Indirect diffuse source order | `Zenith2/Docs/Design/LIGHT/GI.md` → "Indirect diffuse selection" |
| Z2-R-LIGHT-120 | Sky irradiance SH | `Zenith2/Docs/Design/LIGHT/GI.md` → "Sky irradiance" |
| Z2-R-LIGHT-121 | LIGHT QualityMatrix rows | `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "QualityMatrix rows supplied by LIGHT" |
| Z2-R-LIGHT-122 | Per-platform LIGHT tier values | `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Shadow tiers"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Volumetric tiers"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Screen-space tiers"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Reflection and GI tiers"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Temporal and resolution tiers"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Post-processing and output tiers" |
| Z2-R-LIGHT-123 | Fallbacks and their triggers | `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Fallbacks and triggers" |
| Z2-R-LIGHT-124 | Async compute only for the A passes | "Async-compute eligibility" |
| Z2-R-LIGHT-125 | LIGHT CPU work on Flux.BuildGraph without locks | "Frame-graph placement"; "Lock-order entries"; "Thread affinity per type" |
| Z2-R-LIGHT-126 | LIGHT counters | "Counters" |
| Z2-R-LIGHT-127 | LIGHT debug views | "Debug views" |
| Z2-R-LIGHT-128 | Lighting frame-dump section | "Frame-dump entries" |
| Z2-R-LIGHT-129 | Corrupt or wrong-version lighting data | "Failure table"; `Zenith2/Docs/Design/LIGHT/Tests.md` → "Format and fuzz tests" |
| Z2-R-LIGHT-130 | Device-loss recovery of lighting state | "Failure table"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "History invalidation" |
| Z2-R-LIGHT-131 | Memory-pressure shedding limited to the Architecture actions | "Failure table"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md` → "Fallbacks and triggers" |
| Z2-R-LIGHT-132 | Lighting component types registered from C++ | "Light component schemas"; "Flux_LightComponents header"; "ZenithRuntime LIGHT files" |
| Z2-R-LIGHT-133 | LIGHT cook steps | "Cook steps" |
| Z2-R-LIGHT-134 | Backend parity of the lighting scenes | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Parity scenes" |
| Z2-R-LIGHT-135 | Path parity of the lighting scenes | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Parity scenes" |
| Z2-R-LIGHT-136 | Path-tracer comparison for analytic lights | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Path-tracer comparison scenes" |
| Z2-R-LIGHT-137 | IBL furnace against the path tracer | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Path-tracer comparison scenes" |
| Z2-R-LIGHT-138 | Area-light shadows against the path tracer | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Path-tracer comparison scenes" |
| Z2-R-LIGHT-139 | Temporal sequence tests | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Temporal sequence tests" |
| Z2-R-LIGHT-140 | History invalidation on teleport test | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Temporal sequence tests"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "History invalidation" |
| Z2-R-LIGHT-141 | LIGHT budget rows allocated | "Rows owned"; "Pending row needs" |
| Z2-R-LIGHT-142 | D-TBDR revalidation of Stage 5 LIGHT work | "Stage table"; "Open decisions" |
| Z2-R-LIGHT-143 | D-SEL-UPSCALER ruling | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Upscaler selection"; "Selection decisions LIGHT owns" |
| Z2-R-LIGHT-144 | Screen-space approximations within their budgets | `Zenith2/Docs/Design/LIGHT/Tests.md` → "Approximation error scenes" |
| Z2-R-LIGHT-145 | Light keys from two disjoint functions | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light keys" |
| Z2-R-LIGHT-146 | One validation verdict for the GPU scene and the mirror | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Division of the delta work with the GPU scene"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "The delta record"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Validation" |
| Z2-R-LIGHT-147 | Sun and moon slots are directional lights 0 and 1 | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Sun and moon slots"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light and decal list build"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Light record encodings"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Validation" |
| Z2-R-LIGHT-148 | One shadow-planning entry | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadow planning in view-list construction"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Shadowed-light budget" |
| Z2-R-LIGHT-149 | One shadow-atlas tile and plan type in a leaf header | `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Flux_ShadowAtlas header" |
| Z2-R-LIGHT-150 | Cluster layout fixed per capability tier | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Cluster grid" |
| Z2-R-LIGHT-151 | One upscaler enumeration | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Flux_Upscaler header"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Upscaler selection" |
| Z2-R-LIGHT-152 | LIGHT quality values live only in m_xLight | "Flux_LightSettings header"; "Configuration keys" |
| Z2-R-LIGHT-153 | The controller is the only writer of the dynamic-resolution scale | `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Dynamic-resolution controller"; `Zenith2/Docs/Design/LIGHT/Temporal.md` → "Thermal and pressure inputs" |
| Z2-R-LIGHT-154 | Star field behind the atmosphere | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Star field"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Sky draw and aerial perspective" |
| Z2-R-LIGHT-155 | Cloud layer composite | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Cloud layer composite" |
| Z2-R-LIGHT-156 | Cloud shadow on the sun only | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Cloud shadow on the sun"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "The EvaluateLighting contract" |
| Z2-R-LIGHT-157 | Underwater medium override | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Underwater medium override"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Integration"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Fog application" |
| Z2-R-LIGHT-158 | Atmosphere LUTs rebuild on a sky revision change only | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Transmittance LUT"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Multiple-scattering LUT"; "Deferred and failed frames" |
| Z2-R-LIGHT-159 | Lighting environment blocks and their defaults | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Atmosphere ownership"; "Flux_LightSettings header" |
| Z2-R-LIGHT-160 | Specular-weight target only with SSR | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "SSR composite" |
| Z2-R-LIGHT-161 | Lightmap binding follows the X106 ruling | `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmap binding" |
| Z2-R-LIGHT-162 | Dynamic GI on the tiers without SSGI | `Zenith2/Docs/Design/LIGHT/GI.md` → "SSGI platform scope"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Sky capture" |
| Z2-R-LIGHT-163 | LIGHT tickets never edit core documents | "Rendering table additions pending X103"; "Assumed types from pending design documents" |
| Z2-R-LIGHT-164 | LIGHT per-frame upload data follows the build-node order | "Build-node upload data"; `Zenith2/Docs/Design/LIGHT/Shadows.md` → "Flux_Shadows header"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Reflection probe record" |
| Z2-R-LIGHT-165 | A frame that submits nothing advances no LIGHT frame state | "Deferred and failed frames"; "History validity state machine"; "Probe residency state machine" |
| Z2-R-LIGHT-166 | LIGHT shared GPU structs live in Common modules | `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "GPU parameter records"; `Zenith2/Docs/Design/LIGHT/GI.md` → "GI GPU records"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Reflection probe record"; `Zenith2/Docs/Design/LIGHT/GI.md` → "Binding if X106 is accepted" |
| Z2-R-LIGHT-167 | EvaluateLightingEx reaches the directional and atmosphere data through its light source | `Zenith2/Docs/Design/LIGHT/Lighting.md` → "The EvaluateLighting contract"; `Zenith2/Docs/Design/LIGHT/Lighting.md` → "Evaluation order"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Sun transmittance"; `Zenith2/Docs/Design/LIGHT/Volumetrics.md` → "Scattering injection"; "Rendering table additions pending X103" |
| Z2-R-LIGHT-168 | Probe keys derived from the entity identity | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Flux_Reflections header" |
| Z2-R-LIGHT-169 | Probe filter and sky capture passes follow their pass tables | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Probe filter pass table"; `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Sky capture pass table" |
| Z2-R-LIGHT-170 | SSGI follows its pass table | `Zenith2/Docs/Design/LIGHT/GI.md` → "SSGI pass table"; `Zenith2/Docs/Design/LIGHT/GI.md` → "SSGI" |
