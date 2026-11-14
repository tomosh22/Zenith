# Zenith2 ENV design: environment

The ENV area specifies the environment content systems of Zenith2: the terrain authoring model and its brushes, foliage placement and wind, water, the atmosphere parameters and the time-of-day controller that drive LIGHT's sky and fog, clouds, weather, deterministic procedural placement, the collision and navigation generation hooks, and the environment content of streaming cells. This entry document carries the skeleton sections and the top-level contracts; seven section files carry the detail ("Document family"). The normative sources are `Zenith2/Docs/Architecture.md` (the module model, data-defined components, frame phases, determinism, the W-Outdoor workload) and `Zenith2/Docs/Rendering.md` (the sky and fog passes, the GPU scene, validation roles, the GPU performance rules); where this family disagrees with them they win and this family is corrected in the same diff ([Z2-N-REND-001], [Z2-N-ARCH-001]). Obligations are plain prose and bullets; each is traced by a `Z2-R-ENV-NNN` requirement of the ENV catalogue, `Zenith2/Docs/Requirements/ENV.md` (Stages 1 and 3) and `Zenith2/Docs/Requirements/ENV/Stage4Plus.md` (Stages 4 to 6).

## Purpose and scope

### What ENV owns

| Capability | Specified in |
|---|---|
| the optional module `ZenithEnvironment`: components, world resources, systems, extractors, bindings, the module entry points | "Library and file layout", "Public API" |
| the terrain document and tiles (heights, layers, holes, density and exclusion masks, explicit instances), every brush as an operation on it, weight normalisation, edit deltas and the undo hooks EDIT calls, the live preview | `Zenith2/Docs/Design/ENV/Terrain.md` |
| the terrain runtime: terrain and tile activation, CPU height, normal and physical-material queries, the layered terrain material | `Zenith2/Docs/Design/ENV/Terrain.md` |
| foliage types, placed instance streams into the GPU scene, density per quality tier and shedding, the detail-foliage generator (a source of GEOM's procedural geometry, Q38), wind and the shared wind function, the foliage vertex modifier | `Zenith2/Docs/Design/ENV/Foliage.md` |
| water bodies, Gerstner waves and the deterministic height query, water and underwater rendering, the advanced water tiers, the water error budget | `Zenith2/Docs/Design/ENV/WaterWeather.md` |
| the weather table, the weather state machine, weather effects on materials, particles, audio and gameplay | `Zenith2/Docs/Design/ENV/WaterWeather.md` |
| the atmosphere parameters document, the time-of-day clock and celestial state, the sun and moon lights, clouds, their 3D noise volumes (cook, format and asset kind) and their tiers | `Zenith2/Docs/Design/ENV/Atmosphere.md` |
| deterministic procedural placement from a seed | `Zenith2/Docs/Design/ENV/Procedural.md` |
| the nine authoring document schemas and their value types, ENV's terrain importer and cook steps, the ENV cooked formats, the collision and navigation generation hooks, the environment content of streaming cells | `Zenith2/Docs/Design/ENV/Bake.md` |
| ENV's GPU side inside ZenithFlux (the terrain tile tables and the frame environment constants, water nodes, the cloud and underwater passes, the detail-foliage source, the terrain kind loader), as GEOM's terrain renderer lives in Flux | "ZenithFlux ENV files" |
| the VT-backed terrain material reference scene of the physical-accuracy suite ([Z2-N-REND-944]), while no ADVGFX design exists | `Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"` |
| the test catalogue | `Zenith2/Docs/Design/ENV/Tests.md` |

### Explicit exclusions

| Not ENV | Owner | Decision or reason |
|---|---|---|
| terrain geometry: the cook function `Zenith_CookTerrainGeometry` (quantisation, the min/max pyramid, sections 0x0101–0x0104), CDLOD node selection, morphing, height-tile textures, `Flux_TerrainRenderer` | GEOM | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`; ENV builds the function's input and calls it (`Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"`) |
| procedural geometry: the pool, the `procedural:` stream-sets, `Flux_ProceduralFrame`, `Zenith_ProceduralGeometrySource`, `Flux_AddProceduralGeometry`, `Z2-B-GEOM-005` | GEOM | Q38 (orchestrator-ruled); ENV implements a source (`Zenith2/Docs/Design/ENV/Foliage.md → "Detail source"`) |
| the render environment's writer, `SetEnvironment`, the sun and moon slots | RPROD (the engine environment extractor), LIGHT (the slots' meaning) | X125 (orchestrator-ruled); ENV supplies the blocks it copies |
| the sky-view, transmittance and multiple-scattering LUTs, the sky draw, aerial perspective, the froxel fog passes, directional-light shading | LIGHT | `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"`; ENV supplies parameters |
| GPU-scene tables, instancing, culling, LOD, indirect draws, the frame's pass order | RPROD (GEOM for LOD and instancing) | Z2-D-058, Z2-D-060 |
| the terrain and foliage editor tools (brush UI, cursor, palette panels, the placement preview UI), undo transactions | EDIT | Z2-D-079: terrain gets visual authoring, delivered by EDIT's tools over ENV's documents and operations |
| world partitions, data and gameplay layers, concurrent-editing granularity, orchestration of reproducible procedural generation, the lighting, navigation and HLOD baking workflows | WORLD | Appendix A WORLD row; boundary in "Boundary with WORLD" |
| streaming cells, cell loading and cancellation, large-coordinate rebasing | STREAM | ENV defines only its cell content ("Streaming cell content" in Bake.md) |
| collision shapes and their cooking, physics bodies, buoyancy | PHYS | ENV supplies geometry and hooks ("Collision generation hook" in Bake.md) |
| navmesh generation and queries | AI (D-SEL-NAV (open, Stage 1/2)) | ENV supplies geometry and area flags ("Navigation generation hook") |
| particles for rain and snow, audio ambience, gameplay reactions to weather | VFX, AUDIO, GAME | ENV spawns effect prefabs and emits events ("Weather effects" in WaterWeather.md) |
| the asset container, kind table, cook orchestration and DDC | ASSET | ENV's formats and steps sit inside them |
| a material node graph | — | excluded by Z2-D-079; ENV's materials are Slang code |
| runtime terrain deformation (craters, digging) | — | not an inventory item; the terrain is static at runtime outside the editor |

### Boundary with WORLD

WORLD owns how environment content is organised and baked across a world; ENV owns the content systems themselves.

| Concern | ENV | WORLD |
|---|---|---|
| terrain | the document, tiles, brushes, cook | which partition a tile belongs to, per-partition locking for concurrent editing |
| procedural placement | the algorithm, rule documents, determinism | which rule sets apply where, re-generation triggers, baking results into partitions, whole-world generation from a seed |
| collision and navigation | the extraction hooks per tile | the navigation bake workflow and its UI (with AI) |
| streaming | the ENV content of a cell | the partition cook that writes cells (with STREAM) |

### Inventory items covered

| Item | Text | Stage | Specified in |
|---|---|---|---|
| Z2-I-ENV-01 | terrain sculpting | S3 (Q45; pending core-doc batch 4 (Q45)) | `Zenith2/Docs/Design/ENV/Terrain.md → "Brush operations"` |
| Z2-I-ENV-02 | terrain painting | S4 | `Zenith2/Docs/Design/ENV/Terrain.md → "Splat layers and blending"` |
| Z2-I-ENV-03 | foliage placement | S3 (Q45; pending core-doc batch 4 (Q45)) | `Zenith2/Docs/Design/ENV/Foliage.md → "Placed instance streams"` |
| Z2-I-ENV-04 | foliage wind | S4 | `Zenith2/Docs/Design/ENV/Foliage.md → "Wind model"` |
| Z2-I-ENV-05 | water | S4 | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Water model"` |
| Z2-I-ENV-06 | atmosphere | S3 (Q45; pending core-doc batch 4 (Q45)) | `Zenith2/Docs/Design/ENV/Atmosphere.md → "Atmosphere parameters"` |
| Z2-I-ENV-07 | clouds | S4 | `Zenith2/Docs/Design/ENV/Atmosphere.md → "Clouds"` |
| Z2-I-ENV-08 | weather | S4 | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather model"` |
| Z2-I-ENV-09 | day/night | S3 (Q45; pending core-doc batch 4 (Q45)) | `Zenith2/Docs/Design/ENV/Atmosphere.md → "Time of day"` |
| Z2-I-ENV-10 | procedural placement | S3 (Q45; pending core-doc batch 4 (Q45)) | `Zenith2/Docs/Design/ENV/Procedural.md → "Placement model"` |
| Z2-I-ENV-11 | collision generation | S4 | `Zenith2/Docs/Design/ENV/Bake.md → "Collision generation hook"` |
| Z2-I-ENV-12 | navigation generation | S4 | `Zenith2/Docs/Design/ENV/Bake.md → "Navigation generation hook"` |
| Z2-I-ENV-13 | environment streaming integration | S4 | `Zenith2/Docs/Design/ENV/Bake.md → "Streaming cell content"` |

The orchestrator ruled Q45 (a): the Stage 3 WorkloadOutdoor deliverable (`Zenith2/Docs/Roadmap.md → "Stage 3 deliverables"`, item 9) wins, so items 01, 03, 06, 09 and 10 move to Stage 3 by the inventory change rule, queued for core-doc batch 4; `Requirements/Inventory.md` still shows S4 until that batch lands. Every requirement serving them has `Stage: S3` and fix version `Z2 Stage 3` ("Stage table").

### Document family

| File | Its `##` headings |
|---|---|
| `Zenith2/Docs/Design/ENV.md` (this file) | the skeleton headings, then "Render-world records" and "Reconciliation items" |
| `Zenith2/Docs/Design/ENV/Terrain.md` | Scope of this file · Terrain model · Zenith_TerrainDocument header · Brush operations · Splat layers and blending · Terrain edit deltas and undo · Live preview · Zenith_TerrainRuntime header · Height queries · Terrain material module |
| `Zenith2/Docs/Design/ENV/Foliage.md` | Scope of this file · Foliage classes · Foliage type document · Zenith_FoliageSystem header · Placed instance streams · Density per quality tier · Instance cap and shedding · Detail foliage generator · Wind model · Wind shader function · Foliage vertex modifier |
| `Zenith2/Docs/Design/ENV/WaterWeather.md` | Scope of this file · Water model · Zenith_WaterSystem header · Wave synthesis · Water rendering · Underwater · Advanced water · Water error budget · Weather model · Zenith_WeatherSystem header · Weather state machine · Weather effects · Weather tiers |
| `Zenith2/Docs/Design/ENV/Atmosphere.md` | Scope of this file · Atmosphere parameters · Zenith_AtmosphereParameters header · Weather modulation of the atmosphere · Time of day · Zenith_TimeOfDay header · Solar and lunar position · Clouds · Render environment extraction |
| `Zenith2/Docs/Design/ENV/Procedural.md` | Scope of this file · Placement model · Placement rule document · Zenith_ProceduralPlacement header · Seeded streams · Candidate generation · Rule evaluation · Exclusion and priority · Cross-cell determinism · Determinism argument · Output ordering and capacity · Runtime and editor use · Boundary with WORLD procedural generation |
| `Zenith2/Docs/Design/ENV/Bake.md` | Scope of this file · Authoring document schemas · Cook pipeline · Terrain cook · Foliage cook · Parameter document cook · Cooked formats · Runtime asset objects · Collision generation hook · Navigation generation hook · Regeneration after edits · Streaming cell content |
| `Zenith2/Docs/Design/ENV/Tests.md` | Scope of this file · Test targets and locations · Unit tests · Determinism tests · Cook tests · Render and parity tests · Error-budget tests · Fuzz targets · Stress and sanitizer tests · Workload and device evidence · Gate steps per test group |
| `Zenith2/Docs/Design/ENV/RequirementIndex.md` | Scope of this file · Requirement to design map |

The requirement catalogue is two files with one numbering: `Zenith2/Docs/Requirements/ENV.md` (purpose, the S1 and S3 requirements, the coverage of inventory items, the exclusions) and `Zenith2/Docs/Requirements/ENV/Stage4Plus.md` (the S4, S5 and S6 requirements); "Requirement index" below points to `Zenith2/Docs/Design/ENV/RequirementIndex.md`, which maps every requirement to its design headings.

### Conventions

- Names are Zenith Hungarian (Z2-D-034); ENV's C++ types are `Zenith_*`, its Flux-side types `Flux_*`, its Slang types `Env_*` (shared structs) and `Material_*` (materials); configuration keys start with `environment.` (the module prefix of `Zenith2/Docs/Design/PLAT/Configuration.md → "Key grammar"`); counters start with `environment.`.
- Units are CORE's (`Zenith2/Docs/Design/CORE/Maths.md → "Units"`): metres, seconds of simulation time, radians internally (degrees only in authoring documents), lux; world +Y up.
- `<!-- elaboration -->` marks design beyond the core documents; `<!-- assumption: pending <AREA> -->` marks an interface ENV assumes from an area whose design is not yet signed off, each with a row in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows EN1 … EN25, "Reconciliation items"). A need that only a core document can grant is a cross-doc item (`X` row) of the orchestrator's queue; the assumption block and the reconciliation table name that X row beside the EN row, and the requirements that depend on it say so.
- Tests are named `Test_<Type>_<Behaviour>` and catalogued in `Zenith2/Docs/Design/ENV/Tests.md`.

## Dependencies

### Libraries ENV code depends on

| Library | Why | Declared in Architecture |
|---|---|---|
| ZenithRuntime | module interfaces, start and attach contexts | yes |
| ZenithFlux, ZenithShadersGenerated | the render-world writer, ENV's GPU side, generated parameter headers | yes |
| ZenithAssets | asset handles, kind loaders for ENV's kinds | yes |
| ZenithScene | transforms, prefab instantiation commands for effect prefabs, the `Zenith_FluidQuery` interface ENV implements for PHYS's buoyancy (Stage 4, row PH8) | yes |
| ZenithECS, ZenithReflect | components, world resources, systems, command kinds, events, the binding ABI | yes |
| ZenithJobs | sub-jobs for brushes and placement | yes |
| ZenithCore | containers, maths, random, hash, JSON, results | yes |
| ZenithPlatform | the module keeps the `Zenith_Config&` of `Zenith_ModuleStartContext::GetConfig()` beyond Start: it registers a callback on `Zenith_Config::GetChangeRegistry()` (`Zenith2/Docs/Design/PLAT/Configuration.md → "Zenith_Config API"`) to re-read its presentation keys at every quality-tier change and user commit, and unregisters it at shutdown, which is the case X124 requires for the edge | no: pending ARCH (X124) |
| ZenithCompression | the COMPRESS section form of tile payloads and edit deltas (`Zenith_CompressionCodecSet`, `Zenith2/Docs/Design/COMPRESS.md → "What COMPRESS owns"`) | no: pending ARCH (X124) |
| ZenithProfiling | profiler zones and on-screen rows | no: pending ARCH (X124) |

ZenithCook links ZenithEnvironment for the ENV cook steps (ASSET's assumption that ZenithCook declares ZenithRuntime and every module, `Zenith2/Docs/Design/ASSET.md → "Assumed interfaces pending other areas"`; the edge is pending ARCH, X124); ZenithEditor links every module already (the tools table of `Zenith2/Docs/Architecture.md → "Library dependency graph"`). Until X124 lands, `Libgraph` fails the three new edges, so the scaffold ticket is blocked by it.

### Headings this area relies on

| Heading | Used for |
|---|---|
| `Zenith2/Docs/Architecture.md → "Engine modules and game projects"` | the module list row `Environment`, entry points, headless behaviour |
| `Zenith2/Docs/Architecture.md → "Library dependency graph"` | ZenithEnvironment's layer and declared edges |
| `Zenith2/Docs/Architecture.md → "Frame phases and authority"`, `Zenith2/Docs/Architecture/FramePhases.md → "Per-field authority"` | phase-1 ownership of ENV's world resources, command application |
| `Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"` | `Zenith_ModuleStartContext`, `Zenith_WorldAttachContext`, `Zenith_CommandKindDesc` |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"` | extractors, `Zenith_ExtractorDesc` |
| `Zenith2/Docs/Architecture.md → "Determinism guarantee and mechanisms"` | Z2-D-019 obligations of every ENV simulation path |
| `Zenith2/Docs/Architecture.md → "Memory governor"` | the Streaming shedding row for foliage and tile data |
| `Zenith2/Docs/Architecture.md → "Security architecture"` | parser limits, fuzz targets |
| `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` | authoring-document migration, cooked versioning |
| `Zenith2/Docs/Contracts.md → "Errors and results"`, `"Contract blocks per type"`, `"API boundary"` | every public declaration |
| `Zenith2/Docs/Rendering.md → "Purpose and scope"` | the render-world hand-off, full-state extraction |
| `Zenith2/Docs/Rendering.md → "Materials"`, `"Render views"`, `"Validation roles"`, `"GPU performance rules and lints"` | ENV's materials, views, error budgets, compliance |
| `Zenith2/Docs/Rendering/PerPass/Common.md → "SkyAtmosphere"`, `"VolumetricFog"` | the passes ENV feeds |
| `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`, `"Terrain data"`, `"Node selection"`, `"Terrain public API"`, `"Terrain record constant"`, `"Instancing"`, `"LOD selection"` | the terrain cook function and its dense tile grid, the record's `m_uTerrainRecord` constant and `GetRecordIndex`, CDLOD, the LOD distance ratio, instancing, LOD |
| `Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"`, `"Procedural stream-sets"`, `"Registration and sizing"` | the detail-foliage source |
| `Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`, `"Texture cook pipeline"`, `"Texture source image intermediate"`, `"Texture replace from memory"`, `"Virtual texturing boundary"` | the height, hole-mask, splat-index and splat-weight classes and their chain rules; the `geom.textureSource` blobs ENV's importer writes; the preview's whole-chain replace; VT-backed layers |
| `Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`, `"Material cook"`, `"Material document intermediate"` | the generated terrain, detail and water materials and their `geom.materialDocument` blobs |
| `Zenith2/Docs/Design/RPROD/Renderer.md → "Render environment"` | the one writer of the render environment and the sun and moon slots (X125) |
| `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"`, `"Media model"`, `"Sun transmittance"`, `"Star field"`, `"Cloud layer composite"`, `"Cloud shadow on the sun"`, `"Underwater medium override"`, `"Flux_Atmosphere header"` | the parameters ENV supplies and the hooks LIGHT draws them with |
| `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light extraction and the CPU light mirror"`, `"The delta record"`, `"Light and decal list build"`, `"Light record encodings"`, `"Sun and moon slots"`, `"Lighting channels"` | every member of the sun and moon slots, the slot flag contract, the reserved slots, the directional order and overflow |
| `Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`, `"Flux_Lighting header"` | `Flux_ViewMediumOverride`, the payload of ENV's per-view record; `Flux_LightingFrame::m_axViewMediumOverrides`, where LIGHT copies it (16 per frame across worlds) |
| `Zenith2/Docs/Rendering.md → "The render world hand-off"` | `Flux_RenderWorldSection` and `Flux_RenderWorldWriter`, which X156 extends with ENV's four record types |
| `Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`, `Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`, `Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStepDesc"` | ENV's importers declare the products of a terrain and write their intermediates; one ASSET-phase step per kind |
| `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"` | the threading and lifetime of ENV's kind loaders |
| `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`, `"Zenith_CanonicalJson header"`, `"Canonical rules"`; `Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"`, `"Registered document types"` | the tool-side registration of ENV's nine document types, their canonical schemas, and editing them as JSON trees, which keeps their unknown members |
| `Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"` | the 3D cloud-noise textures |
| `Zenith2/Docs/Design/CORE/Maths.md → "Zenith_RandomSource"`, `"Zenith_Hash64"`, `"Coordinate systems"` | seeded streams, keys, coordinates |
| `Zenith2/Docs/Design/COMPRESS.md → "Compressed section descriptor on disk"` | tile payloads, cooked sections |
| `Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, `"Upward interfaces ASSET declares"` | ENV's kinds and loaders |
| `Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier changes"`, `Zenith2/Docs/Design/PLAT/Android.md → "Applying thermal actions"` | tier-driven density and cloud tiers |
| `Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"`, `"Zenith_SystemContext header"` | system registration and access |
| `Zenith2/Docs/Scripting.md → "Modules, bindings and generated types"` | ENV's Luau bindings |
| `Zenith2/Docs/Budgets.md → "W-Outdoor"`, `"Per-view Forward+ pass rows"` | the workload and `Z2-B-ENV-001` |

### Upward-callback interfaces

ENV declares the interfaces its own callers implement, so every call points downward ([Z2-N-CONTRACT-175]):

| Interface | Declared in | Implemented by | Purpose |
|---|---|---|---|
| `Zenith_TerrainTileSource` | `Zenith_TerrainDocument.h` | EDIT, the cook, tests | supply tile document bytes |
| `Zenith_TerrainDocumentSink` | `Zenith_TerrainDocument.h` | EDIT (journaled writes), tests | receive saved document bytes |
| `Zenith_PlacementSurface` | `Zenith_ProceduralPlacement.h` | the terrain document adapter, the runtime terrain adapter, tests | surface queries for placement |

ENV implements `Zenith_AssetKindLoader` (ASSET) for its kinds, `Zenith_ModuleInstance` (ZenithRuntime) and `Zenith_MemoryConsumer` (CORE); it stores no pointer into a higher library.

### Assumed types from pending design documents

<!-- assumption: pending RPROD, REND (X125, X127, X146, X156) -->
| Need | ENV's assumption | Note |
|---|---|---|
| a material vertex-modifier hook | `Flux_MaterialType` gains optional `ModifyPositionWS` evaluated in every mesh vertex program, current and previous time, with UV0 and the frame's `Env_FrameEnvironment` in its input ("Material_Foliage" in Foliage.md); RPROD confirms its programs apply it through GEOM's mesh vertex function (RP20) | EN1, EN18; X127 (1), X146 |
| the frame environment constants | every mesh pass exposes one frame-scoped `Env_FrameEnvironment` ("Render-world records") to vertex modifiers and material `Evaluate` functions, and the modifier input carries UV0; the Stage 3 terrain material reads its splat tiles through it, so the Stage 3 terrain material tickets are gated by core-doc batch 4 (X146) | EN18; X146 (extends X127 (1)) |
| the render environment | one `SetEnvironment` per world per frame by the engine environment extractor, which reads ENV's `PHASE_INPUT` `Flux_EnvironmentBlocks` world resource after the phase boundary (`Zenith2/Docs/Design/RPROD/Renderer.md → "Render environment"`); ENV's block setters are withdrawn | X125 (ruled), RP17, RP25, EN17 |
| ENV's render-world records | four `Flux_RenderWorldSection` spans and four typed `Flux_RenderWorldWriter` members, stated exactly in "Record writer"; detail foliage uses GEOM's `AddProceduralInstance` (X141) | EN5 (reopened), EN24; X156 |
| instance delta and key | `Flux_RenderInstanceDelta` with a mesh ID, overrides and 32-bit custom data; `Flux_RenderInstanceKey` 64-bit | GM5 (GEOM's), EN5 |
| per-instance cull distance | adopted by RPROD (`Zenith2/Docs/Design/RPROD/Culling.md → "Per-instance cull distance"`, RP21), pending the primitive-record flag in Rendering | EN10; X127 (2) |
| frame slots of ENV passes | `Env_AddCloudLayer` before `SkyAtmosphere`; `Env_AddUnderwater` after `Particles`; `Env_AddVolumetricClouds`, `Env_AddCloudShadowMap` and `Env_AddOceanFFT` (Stage 5) in the prologue after `Flux_AddProceduralGeometry`; confirmed by RPROD (RP20) | EN11; X127 (3) |
| capability matrix rows | terrain, placed foliage, detail foliage, water, underwater, cloud layer, volumetric clouds, with their parity comparisons | EN11; X127 (3) |
| ENV's GPU side only with the module | `Flux_Renderer` creates `Flux_EnvironmentRenderer` when ENV's module start calls `Flux_Renderer::EnableEnvironment()` through `Zenith_ModuleStartContext::GetRenderer()` ("ZenithFlux ENV files"); answered: `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"` declares the member and the owned renderer member, ENV's implementation defines the body in RPROD's `Internal/Renderer/Flux_Renderer.cpp`, and `Flux.BuildGraph` step 7 sets `Flux_FrameShared::m_pxEnvironment` from that member ([Z2-N-REND-995]) | EN17 |
<!-- /assumption -->

GEOM's post-signoff patch of open question Q52 (ruled (a), cross-area row GM31) has landed and answers row EN21, so these four needs are cited, not assumed:

| Need | GEOM's answer |
|---|---|
| the terrain record constant | `AddTerrain` binds R into `Flux_TerrainDesc::m_xMaterial`'s `m_uTerrainRecord` before any node draws, and `uint32 Flux_TerrainRenderer::GetRecordIndex(Flux_TerrainID) const` returns it (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"`) |
| texture replace from memory | `Flux_TextureManager::ReplaceChainFromMemory`, whole chains of the `HEIGHT`, `HOLE_MASK`, `SPLAT_INDEX` and `SPLAT_WEIGHT` classes (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`) |
| the packed detail density | a 128 × 128 `R8G8B8A8` `SPLAT_WEIGHT` image (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`) |
| importer-written source images and documents | the blobs `geom.textureSource` (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`) and `geom.materialDocument` (`Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`) |

<!-- assumption: pending GEOM -->
| Need | ENV's assumption | Note |
|---|---|---|
| the terrain adapter's UVs | `TerrainGrid.slang` supplies `m_xUV0` = world XZ in metres with gradients | EN2 |
| the grid diagonal | the terrain grid mesh splits each quad along (s, t)–(s + 1, t + 1) | EN2, EN4 |
| weather in engine materials | `Material_Standard` calls `Env_ApplySurfaceWeather` | EN8 |
| the detail source | `Flux_DetailFoliageSource` registers with `Flux_ProceduralGeometrySystem` through `Flux_Renderer::GetProceduralGeometry()` (GM22, GM29); the pool sizing inputs of `Zenith2/Docs/Design/ENV/Foliage.md → "Detail pool sizing"` reach GEOM's Stage 1 budget ticket | EN16 |
<!-- /assumption -->

<!-- assumption: pending LIGHT (X125) -->
| Need | ENV's assumption | Note |
|---|---|---|
| cloud composite, stars, cloud shadows | adopted by LIGHT (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Cloud layer composite"`, "Star field", "Cloud shadow on the sun"); the engine extractor copies their parameters from ENV's render-blocks resource into LIGHT's `Flux_AtmosphereBlock` (L31) | EN6, EN17 |
| the underwater medium | LIGHT fills a submerged view's `Flux_ViewMediumOverride` (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Underwater medium override"`) on the render side from ENV's `Flux_RenderEnvironmentView` record, whose payload is that type, at most 16 per world (ENV's cap), and LIGHT copies the payloads into `Flux_LightingFrame::m_axViewMediumOverrides` (`Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"`, 16 per frame across worlds); no environment block carries an override, since the per-view medium cannot be a phase-1 block | EN17, EN23; LIGHT row L39 |
| the sun and moon slots | LIGHT treats `m_xSun` and `m_xMoon` as directional lights 0 and 1 in its reserved light-table slots; the role is the slot and no flag carries it; ENV writes the slot flag contract's bits (sun: casts-shadow, contact, volumetric; moon: volumetric, plus casts-shadow above 1 lux) and every other member of the table in Atmosphere.md "Sun and moon illuminance" (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`, "The delta record") | EN6, EN23; LIGHT row L39 |
<!-- /assumption -->

<!-- assumption: pending ASSET, PHYS, STREAM, WORLD, EDIT, AUDIO, VFX, GAME, CONTENT, ADVGFX -->
| Need | Owner | ENV's assumption | Note |
|---|---|---|---|
| the in-memory asset replacement of regenerated files and cook error codes (the kinds 23 … 27 landed, AS9); kind 34 `Environment.CloudNoise`; the terrain importer's products and intermediates | ASSET | "Regeneration after edits", "Step graph" in Bake.md; "Cloud noise volumes" in Atmosphere.md | EN3, EN22 |
| heightfield cook input, static collider component, instanced static compound shape, physical-material table, a matching diagonal | PHYS | "Collision generation hook" in Bake.md | EN4 |
| STREAM cells are whole multiples of 128 m aligned to the terrain origin; cell content is scene content | STREAM, WORLD | "Streaming cell content" in Bake.md | EN7 |
| EDIT's terrain command owns a `Zenith_TerrainEdit`; EDIT's tools call the brush API; the sculpt-preview latency row | EDIT | "Undo integration" in Terrain.md | EN12 |
| effect prefabs carry VFX emitters and AUDIO emitters; GAME and scripts consume weather events | VFX, AUDIO, GAME | "Weather effects" in WaterWeather.md | EN13 |
| the merge driver treats tile documents by file | CONTENT | "Merging terrain documents" in Bake.md | EN20 |
| VT sampling of terrain layers and the path tracer's terrain material | ADVGFX | "VT-backed terrain materials" in Terrain.md | EN19 |
| EDIT's extractor writes the preview records; the anchor on the editor camera | EDIT | "Live preview" in Terrain.md | EN12 |
<!-- /assumption -->

### Areas that consume ENV

| Area | Uses |
|---|---|
| EDIT | the terrain document, brushes, edits, preview, placement preview, every ENV document's read and write |
| WORLD | placement, the cell contribution, regeneration after edits |
| PHYS | collision tiles, foliage collision sets, water samples for buoyancy |
| AI | navigation geometry and area flags (the `ai.navSource` intermediate written through AI's writer), the navigation water list `Zenith_ListWaterBodiesForNavigation` decodes |
| GAME, SCRIPT | height, water, wind and weather queries, commands, events, bindings |
| LIGHT | the atmosphere, fog and sun and moon parameters |
| VFX, AUDIO | wind samples, physical materials, weather effect prefabs |
| RPROD | W-Outdoor, whose primary rows include terrain and foliage; the engine environment extractor, which copies ENV's blocks |
| GEOM | the detail-foliage source's instances in its procedural pool; the terrain's generated materials |
| ADVGFX | the VT-backed terrain reference scene of its physical-accuracy suite |

## Library and file layout

### Target summary

| Target | Kind | `CMakeLists.txt` | `ZENITH_SUPPORTED` |
|---|---|---|---|
| `ZenithEnvironment` | `KIND MODULE`, `MODULE_NAME Environment` | `zenith_add_library(ZenithEnvironment KIND MODULE MODULE_NAME Environment DEPENDS ZenithRuntime ZenithFlux ZenithShadersGenerated ZenithAssets ZenithScene ZenithECS ZenithReflect ZenithJobs ZenithPlatform ZenithCompression ZenithProfiling ZenithCore)`; the nine edges up to `ZenithJobs` and `ZenithCore` are `Zenith2/Docs/Architecture.md → "Library dependency graph"`'s row, and `ZenithPlatform`, `ZenithCompression` and `ZenithProfiling` are pending ARCH (X124): the scaffold ticket lands this line only after X124 | `ZENITH_GPU` (every module is, [Z2-N-ARCH-173]) |
| ENV's files in `ZenithFlux` | part of ZenithFlux | ZenithFlux's own call | ZenithFlux's (`ZENITH_GPU`) |
| ENV's cook steps in `ZenithCook` | part of the tool | ZenithCook's own call, which gains `ZenithEnvironment` in `DEPENDS` (pending ARCH, X124) and, when absent, `ZenithAuthoring` (EDIT rule 5, the first owner move that lands) | `ZENITH_TOOLS_HOST` |
| ENV's tool-side files in `ZenithAuthoring` | part of EDIT's static tool library (ruling Q84 (a), row ED40) | EDIT's one `zenith_add_tool(ZenithAuthoring …)` call, to whose `DEPENDS` ENV's move appends `ZenithDocument` and `ZenithEnvironment` when absent (`Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, rule 4) | `ZENITH_TOOLS_HOST` |
| `Zenith_Integration_Environment` | integration suite | `zenith_add_integration_tests(Environment DEPENDS ZenithRuntime ZenithEnvironment ZenithPhysics ZenithStreaming)` | narrowed to the GPU presets by its dependencies ([Z2-N-ARCH-172]) |

### ZenithEnvironment files

```
Zenith2/Engine/ZenithEnvironment/
	CMakeLists.txt
	Zenith_Module_Environment.h        module entry points
	Zenith_EnvironmentErrors.h         Zenith_EnvironmentError, codes, warnings
	Zenith_EnvironmentTypes.h          coordinates, dirty regions, instance and placement value types, enums, constants
	Zenith_EnvironmentComponents.h     the component and world-resource type IDs and C++ layouts
	Zenith_EnvironmentState.h          Zenith_EnvironmentState, commands, events
	Zenith_TerrainDocument.h           Terrain.md
	Zenith_TerrainBrush.h              Terrain.md
	Zenith_TerrainRuntime.h            Terrain.md
	Zenith_FoliageSystem.h             Foliage.md
	Zenith_Wind.h                      wind state and the CPU mirror
	Zenith_WaterSystem.h               WaterWeather.md
	Zenith_WeatherSystem.h             WaterWeather.md
	Zenith_AtmosphereParameters.h      Atmosphere.md
	Zenith_TimeOfDay.h                 Atmosphere.md
	Zenith_ProceduralPlacement.h       Procedural.md
	Zenith_EnvironmentGeneration.h     Bake.md (collision and navigation hooks, regeneration)
	Zenith_EnvironmentDocuments.h      the document value types and the readers of the foliage, atmosphere, weather, water, terrain layer and cloud-noise documents
	Zenith_EnvironmentAssets.h         the runtime objects of kinds 23 to 27 (Bake.md "Runtime asset objects")
	Internal/Module/                   Zenith_EnvironmentModule.cpp (entry points, start, attach), Zenith_EnvironmentConfig.cpp (keys), Zenith_EnvironmentMemory.cpp (consumer)
	Internal/Systems/                  Zenith_EnvironmentAdvance.cpp, Zenith_EnvironmentRenderBlocks.cpp (the blocks resource), Zenith_UpdateTerrainTiles.cpp, Zenith_UpdateFoliageCells.cpp, Zenith_UpdateWaterBodies.cpp
	Internal/Extract/                  Zenith_ExtractEnvironment.cpp (terrain, water-body and view records), Zenith_ExtractFoliage.cpp (placed instances and detail cells), Zenith_EnvironmentRecordSink.h (the extractors' record sink, "Record writer")
	Internal/Terrain/                  Zenith_TerrainDocument.cpp, Zenith_TerrainTileCodec.cpp, Zenith_TerrainBrush.cpp, Zenith_TerrainWeights.cpp, Zenith_TerrainEdit.cpp, Zenith_ScatterSurface.cpp/.h, Zenith_TerrainPreview.cpp, Zenith_TerrainRuntime.cpp/.h, Zenith_TerrainQueries.cpp, Zenith_TerrainRaycast.cpp
	Internal/Foliage/                  Zenith_FoliageSystem.cpp/.h, Zenith_FoliageExtraction.cpp, Zenith_Wind.cpp
	Internal/Water/                    Zenith_Gerstner.cpp, Zenith_WaterWorld.cpp
	Internal/Weather/                  Zenith_WeatherMachine.cpp, Zenith_WeatherSystem.cpp/.h, Zenith_WeatherEffects.cpp
	Internal/Atmosphere/               Zenith_Atmosphere.cpp, Zenith_TimeOfDay.cpp, Zenith_Celestial.cpp
	Internal/Procedural/               Zenith_Placement.cpp, Zenith_PlacementStages.cpp, Zenith_PlacementRules.cpp
	Internal/Generation/               Zenith_CollisionExtract.cpp, Zenith_NavigationExtract.cpp, Zenith_TileFileWriter.cpp, Zenith_FoliageCellWriter.cpp, Zenith_Regenerate.cpp
	Internal/Assets/                   the kind loaders of Environment.TerrainTile, .FoliageCell, .Atmosphere, .Weather, .Water
	Internal/Documents/                the JSON readers (the document-type registration and migration steps are tool-side, in ZenithAuthoring)
	Internal/Bindings/                 Zenith_EnvironmentBindings.cpp (the Luau binding table)
	Tests/                             Tests.md
	Tests/Fixtures/, Tests/Fuzz/, Tests/Examples/
```

No ENV source is platform-specific; there is no `Internal/Windows|Android|Linux/` directory.

### ZenithFlux ENV files

```
Zenith2/Engine/ZenithFlux/
	Flux_Environment.h                 Flux_EnvironmentBlocks, the render-world records of "Render-world records", the five Env_Add* frame-slot entry points, Flux_TerrainResource, Flux_CloudNoiseResource
	Internal/Environment/              Flux_EnvironmentRenderer.h/.cpp (applies ENV records in Flux.BuildGraph), Flux_EnvironmentEntryPoints.cpp (the Env_Add* definitions), Flux_TerrainKindLoader.cpp, Flux_CloudNoiseKindLoader.cpp, Flux_WaterPasses.cpp (ocean FFT, caustics),
	                                   Flux_EnvironmentFrame.cpp (Env_FrameEnvironment, terrain GPU records and tile tables),
	                                   Flux_WaterRenderer.cpp (water node selection and instances), Flux_CloudPasses.cpp,
	                                   Flux_UnderwaterPass.cpp, Flux_DetailFoliageSource.cpp/.h (GEOM's generator interface)
	Tests/Environment/                 the render, error-budget and parity tests of Tests.md
	Tests/Environment/Scenes/          the water, environment parity and TerrainVT reference scenes
```

ENV's Flux-side code reads only the render world and Flux's own objects; it includes no ZenithEnvironment header ([Z2-N-ARCH-076]).

<!-- elaboration -->
A game whose `game.json` does not select `Environment` pays for ENV's GPU side in code size alone. `Flux_Renderer` creates `Flux_EnvironmentRenderer` (and so registers the detail source, the cloud and underwater programs and ENV's frame constants) only when ENV's module start calls `Flux_Renderer::EnableEnvironment()` (declared by RPROD, `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`; its body is ENV's, defined in RPROD's `Internal/Renderer/Flux_Renderer.cpp` as UI's `EnableUI` is; row EN17); ENV's engine materials and pass programs are engine content of the ZenithEnvironment library (`Zenith2/Engine/ZenithEnvironment/Content/`, [Z2-N-ARCH-028]), which a game's cook includes only with the module, so a game without it packages no ENV shader blob and precaches no ENV pipeline. The cook report's `environment.permutations` member counts ENV's variants when the module is selected.

### ZenithCook ENV files

```
Zenith2/Tools/ZenithCook/Internal/Environment/
	Zenith_TerrainImporter.cpp         the importer env.terrain and its stages TerrainValidate, TerrainProducts, TerrainGeometry (calls GEOM's Zenith_CookTerrainGeometry), TerrainSplat, TerrainMaterial, TerrainTiles
	Zenith_CookTerrain.cpp             the steps env.terrain (.zterrain) and env.terrainTile (.zenvtile)
	Zenith_CookFoliage.cpp             the importer stage Foliage and the step env.foliageCell (.zfoliage)
	Zenith_CookEnvironmentGeneration.cpp   the importer stages Collision and Navigation
	Zenith_CookEnvironmentParameters.cpp   the importers and steps of the atmosphere, weather, water and cloud-noise documents (and each water document's generated material)
	Zenith_CloudNoiseGenerator.h/.cpp      the cloud-noise generator (Atmosphere.md "Cloud noise generator")
Zenith2/Tools/ZenithCook/Tests/Environment/          cook tests
Zenith2/Tools/ZenithCook/Tests/Fixtures/Environment/ fixtures
```

The cook-only code stays in ZenithCook: the importer stages and steps, and the cloud-noise generator, which only the step `env.cloudNoise` calls. Everything tool-side that ZenithEditor calls as well lives in ZenithAuthoring (next heading).

### ZenithAuthoring ENV files

ENV's subdirectory of EDIT's tool library `ZenithAuthoring` (ruling Q84 (a), row ED40), laid out by `Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"` (owner directory `Environment`, root header `Zenith_AuthoringEnvironment.h`, included as `#include "Authoring/Zenith_AuthoringEnvironment.h"`):

```
Zenith2/Tools/Authoring/
	Zenith_AuthoringEnvironment.h      ENV's one root header: Zenith_EnvironmentDocumentType, Zenith_RegisterEnvironmentDocumentTypes,
	                                   Zenith_GetEnvironmentCanonicalSchema, uENV_DOCUMENT_SCHEMA_VERSION (Bake.md "Environment document types");
	                                   Zenith_WriteNavigationWaterBodies, Zenith_ListWaterBodiesForNavigation (Bake.md "Water bodies for navigation")
	Internal/Environment/              Zenith_EnvironmentDocumentTypes.cpp, Zenith_EnvironmentNavigationWater.cpp
	Tests/Environment/                 Zenith_EnvironmentDocumentTypes_Tests.cpp, Zenith_EnvironmentNavigationWater_Tests.cpp (ZenithAuthoring_Tests)
	Tests/Fixtures/Migration/Environment/<Type>/   migration fixtures (v<N>.json, v<N>.expected.json)
```

No file here names a ZenithCook or ZenithEditor type (EDIT rule 3): ZenithCook's composition root, its importer stage `Environment.Navigation` and AI's and WORLD's navigation input sources call these functions with plain inputs. ENV's move appends `ZenithDocument` and `ZenithEnvironment` to the library's `DEPENDS` (Z2T-ENV-033); ZenithEnvironment itself declares no `ZenithDocument` edge.

### Shader modules and engine content

| Path | Content |
|---|---|
| `Zenith2/Shaders/Common/Environment.slang` | every `[FluxShared]` struct of ENV, because `[FluxShared]` is admitted only under `Zenith2/Shaders/Common/` ([Z2-N-REND-136]; the ticket adds the module's row to `Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`): `Env_WindParams`, `Env_FoliageWindParams`, `Env_LocalWindSource`, `Env_WeatherParams`, `Env_FrameEnvironment`, `Env_TerrainGpuRecord`, `Env_TerrainTileTextures`, `Env_WaterWaveRecord`, `Env_WaterBodyRecord`; their C++ declarations are generated into `Flux_SharedTypes.generated.h` |
| `Zenith2/Shaders/Environment/EnvHash.slang` | the GPU port of the pinned hash and value noise |
| `Zenith2/Shaders/Environment/Wind.slang` | `Env_WindSample`, `Env_FoliageWindOffset` |
| `Zenith2/Shaders/Environment/Weather.slang` | `Env_ApplySurfaceWeather` |
| `Zenith2/Shaders/Environment/Water.slang` | Gerstner evaluation over `Env_WaterBodyRecord`, the optical model |
| `Zenith2/Shaders/Environment/Passes/` | `CloudLayer.slang`, `VolumetricClouds.slang`, `CloudShadowMap.slang`, `Underwater.slang`, `OceanFFT.slang` |
| `Zenith2/Shaders/Passes/Procedural/DetailFoliage.slang` | the detail-foliage generator program of `Flux_DetailFoliageSource` (GEOM's generator directory) |
| `Zenith2/Shaders/Materials/` | `TerrainLayered.slang`, `Foliage.slang`, `DetailFoliage.slang`, `Water.slang` (engine material types; the per-terrain and per-water material assets are generated by ENV's cook) |
| `Zenith2/Engine/ZenithEnvironment/Content/` | `Meshes/WaterGrid.gridmesh.json` and `Meshes/WaterSkirtQuad.gridmesh.json` (GEOM's `zenith2.grid-mesh` text documents, `Zenith2/Docs/Design/ENV/WaterWeather.md → "Water GPU side"`), the cloud-noise documents `Clouds/BaseNoise.cloudnoise.json` and `Clouds/DetailNoise.cloudnoise.json` (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise volumes"`), the caustic texture, the default atmosphere and weather documents (cooked as engine content, [Z2-N-ARCH-028]) |

### ZENITH_SUPPORTED matrix

`ZenithEnvironment` declares `ZENITH_GPU` (`win`, `win-asan`, `android-arm64`, `android-x86_64`); it is never in `linux-tsan`. Its tests therefore run on Windows and Android builds; the headless test path is a Null-device world in the `Test` step.

## Public API

### Header map

| Header | Declares | Specified in |
|---|---|---|
| `Zenith_Module_Environment.h` | `Zenith_Module_Environment_RegisterTypes`, `Zenith_Module_Environment_Start` | "Zenith_Module_Environment header" |
| `Zenith_EnvironmentErrors.h` | `Zenith_EnvironmentErrorCode`, `Zenith_EnvironmentError`, `Zenith_EnvironmentWarning` | "Zenith_EnvironmentErrors header" |
| `Zenith_EnvironmentTypes.h` | coordinates, regions, constants, value types | "Zenith_EnvironmentTypes header" |
| `Zenith_EnvironmentComponents.h` | the nine component structs | "Zenith_EnvironmentComponents header" |
| `Zenith_EnvironmentState.h` | the environment state resource, commands, events | "Zenith_EnvironmentState header" |
| `Zenith_Wind.h` | `Zenith_WindState`, `Zenith_EvaluateWindCPU` | "Zenith_Wind header" |
| `Zenith_TerrainDocument.h`, `Zenith_TerrainBrush.h`, `Zenith_TerrainRuntime.h` | terrain | `Zenith2/Docs/Design/ENV/Terrain.md` |
| `Zenith_FoliageSystem.h` | foliage | `Zenith2/Docs/Design/ENV/Foliage.md → "Zenith_FoliageSystem header"` |
| `Zenith_WaterSystem.h`, `Zenith_WeatherSystem.h` | water, weather | `Zenith2/Docs/Design/ENV/WaterWeather.md` |
| `Zenith_AtmosphereParameters.h`, `Zenith_TimeOfDay.h` | atmosphere, clock | `Zenith2/Docs/Design/ENV/Atmosphere.md` |
| `Zenith_ProceduralPlacement.h` | placement | `Zenith2/Docs/Design/ENV/Procedural.md → "Zenith_ProceduralPlacement header"` |
| `Zenith_EnvironmentGeneration.h` | collision and navigation hooks | `Zenith2/Docs/Design/ENV/Bake.md → "Collision generation hook"` |
| `Zenith_EnvironmentDocuments.h` | the document value types and readers | "Zenith_EnvironmentDocuments header"; `Zenith2/Docs/Design/ENV/Bake.md → "Document value types"` |
| `Zenith_EnvironmentAssets.h` | `Zenith_TerrainTileAsset`, `Zenith_FoliageCellAsset`, `Zenith_AtmosphereAsset`, `Zenith_WeatherAsset`, `Zenith_WaterAsset` and their value types | `Zenith2/Docs/Design/ENV/Bake.md → "Runtime asset objects"` |
| `ZenithFlux/Flux_Environment.h` | `Flux_EnvironmentBlocks`, the render-world records, the `Env_Add*` frame-slot entry points, `Flux_TerrainResource`, `Flux_CloudNoiseResource` (`Flux_EnvironmentRenderer` is internal, `Internal/Environment/Flux_EnvironmentRenderer.h`) | "Render-world records"; `Zenith2/Docs/Design/ENV/Bake.md → "Runtime asset objects"` |

### Rules across the ENV API

- Every fallible public function returns `Zenith_Result<…, Zenith_EnvironmentError>`; every public type carries one contract block ([Z2-N-CONTRACT-111]).
- No public header includes a third-party header or an STL type ([Z2-N-CONTRACT-043], [Z2-N-CONTRACT-272]).
- ENV's simulation-visible state (the environment state, the terrain, foliage and water resources) is written only in phase 1 by ENV's own systems; every other writer uses a command.
- Content never reaches an assert: documents, cooked files and script arguments fail with results or Luau errors ([Z2-N-CONTRACT-472], [Z2-N-CONTRACT-473]).
- No ENV type holds mutable global state; each world's ENV objects belong to the module instance ([Z2-N-CONTRACT-141]).

### Zenith_Module_Environment header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_Module_Environment.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module_Environment
/// Ownership: RegisterTypes registers ENV's reflected types, command kinds' payload types, events and configuration keys;
///   Start creates the module instance the runtime owns.
/// Thread affinity: MAIN_THREAD, at startup steps 6 and 15.
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION when a registration is rejected (logged by the context); MODULE_ERROR_OUT_OF_MEMORY
///   when the instance cannot be allocated.
/// Version: since Z2 Stage 3. Module entry points per [Z2-N-ARCH-168].
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Environment_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Environment_Start(
	Zenith_ModuleStartContext& xContext);
```

| Entry point step | Action | Failure |
|---|---|---|
| RegisterTypes 1 | the components of "Zenith_EnvironmentComponents header", the three resource types, `Zenith_EnvironmentState`, the render-blocks resource type (no serialized property), the command payload structs, the two event types | `MODULE_ERROR_REGISTRATION` |
| RegisterTypes 2 | the `environment.*` configuration keys of "Configuration keys" through `GetConfigSchema` | `MODULE_ERROR_REGISTRATION` |
| Start 1 | allocate the module instance | `MODULE_ERROR_OUT_OF_MEMORY` |
| Start 2 | register the five kind loaders with the asset registry | `MODULE_ERROR_REGISTRATION` |
| Start 3 | register the command kinds (`Environment.SetTimeOfDay`, `Environment.SetTimeScale`, `Environment.RequestWeather`, `Environment.ForceWeather`, `Environment.ReleaseWeather`), allowed in phases 2 to 10 | `MODULE_ERROR_REGISTRATION` |
| Start 4 | register the Luau binding table ("Luau errors and asserts"), the memory consumer and the configuration change callback on `Zenith_Config::GetChangeRegistry()`; when not headless, call `Flux_Renderer::EnableEnvironment()` (RPROD's declaration, ENV's body, row EN17) | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 1 | create the world's `Zenith_EnvironmentState`, `Zenith_TerrainWorldResource`, `Zenith_FoliageWorldResource`, `Zenith_WaterWorldResource`, the render-blocks resource (`Flux_EnvironmentBlocks`, "Zenith.Environment.RenderBlocks") and, from Stage 4, the `Zenith_EnvironmentFluidQuery` over the water resource (SCENE's interface `Zenith_FluidQuery`, "Zenith.FluidQuery", `Zenith2/Docs/Design/ENV/WaterWeather.md → "Fluid query provider"`), add them as world resources owned by `PHASE_INPUT`, and request the two water meshes | `MODULE_ERROR_OUT_OF_MEMORY` |
| AttachWorld 2 | add the four phase-1 systems ("Frame-graph placement") | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 3 | when not headless, add the two extractors | `MODULE_ERROR_REGISTRATION` |

The module registers no document type: the nine are registered tool-side (`Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"`). Each step has a failure-injection test ([Z2-N-CONTRACT-341]): `Test_EnvironmentModule_Create_FailsAtRegisterTypes1_Unwinds`, `…RegisterTypes2…`, `…Start1…`, `…Start2…`, `…Start3…`, `…Start4…`, `…AttachWorld1…`, `…AttachWorld2…` and `…AttachWorld3_Unwinds` (nine tests, `Zenith2/Docs/Design/ENV/Tests.md → "Stress and sanitizer tests"`); the runtime undoes the registrations in reverse ([Z2-N-ARCH-851]), and a failure after Start 4 also unregisters the configuration callback.

### Zenith_EnvironmentErrors header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentErrors.h
/// @contract enum Zenith_EnvironmentErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Values are stable once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_EnvironmentErrorCode : uint8
{
	ENV_ERROR_INVALID_DESC = 1,          // a descriptor field is invalid; m_uIndex names it
	ENV_ERROR_OUT_OF_MEMORY,
	ENV_ERROR_DOCUMENT_PARSE,            // malformed JSON or a parser limit; m_uIndex the byte offset / 1024
	ENV_ERROR_DOCUMENT_SCHEMA,           // a member has a wrong type or out-of-range value; m_uIndex the field
	ENV_ERROR_DOCUMENT_MIGRATION,        // a migration step failed or the chain is incomplete
	ENV_ERROR_DOCUMENT_READ_ONLY,        // a newer-version document cannot be written
	ENV_ERROR_TILE_MISSING,
	ENV_ERROR_TILE_READ,
	ENV_ERROR_TILE_CORRUPT,
	ENV_ERROR_TILE_OUT_OF_RANGE,
	ENV_ERROR_TILE_BORDER_MISMATCH,
	ENV_ERROR_LAYER_LIMIT,
	ENV_ERROR_LAYER_UNKNOWN,
	ENV_ERROR_FOLIAGE_TYPE_UNKNOWN,
	ENV_ERROR_FOLIAGE_NOT_QUERYABLE,
	ENV_ERROR_BRUSH_INVALID,
	ENV_ERROR_EDIT_STALE,
	ENV_ERROR_WRITE_FAILED,
	ENV_ERROR_PLACEMENT_RULE_INVALID,
	ENV_ERROR_PLACEMENT_CAPACITY,
	ENV_ERROR_WAVESET_INVALID,
	ENV_ERROR_ATMOSPHERE_INVALID,
	ENV_ERROR_WEATHER_TABLE_INVALID,
	ENV_ERROR_WEATHER_STATE_UNKNOWN,
	ENV_ERROR_NOT_RESIDENT,
	ENV_ERROR_OUT_OF_BOUNDS,
	ENV_ERROR_CAPACITY,                  // a bounded output or queue is full
	ENV_ERROR_COOKED_DATA,               // a cooked ENV section failed validation; m_uIndex the section index
};

/// @contract value Zenith_EnvironmentError
/// Ownership: Plain value, 12 bytes ([Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE. Members non-const: held as the error of a Zenith_Result, whose move assignment assigns
///   it.
/// Invalidation: None.
/// Units: m_uIndex per code (a field, tile, rule, dab, wave or section index); m_uDetail a second index or a
///   lower-level code (a COMPRESS or ASSET error value).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EnvironmentError
{
	Zenith_EnvironmentErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uDetail = 0;
	uint32 m_uIndex = 0;
	uint32 m_uIndex2 = 0;
};

constexpr const char* Zenith_ToString(Zenith_EnvironmentErrorCode eCode);

/// @contract enum Zenith_EnvironmentWarning
/// Ownership: Plain enumeration of the warnings ENV logs (never returned as errors).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EnvironmentWarning : uint8
{
	ENV_WARNING_NEWER_SCHEMA = 1,
	ENV_WARNING_PREVIEW_RANGE_EXCEEDED,
	ENV_WARNING_BORDER_REPAIRED,
	ENV_WARNING_ATMOSPHERE_FALLBACK,
	ENV_WARNING_FOLIAGE_TABLE_FULL,
	ENV_WARNING_WEATHER_REQUEST_REFUSED,
	ENV_WARNING_SCENE_SUN_IGNORED,           // the settings drive the sun and moon while the world holds a directional light component
	ENV_WARNING_ANCHOR_MISSING,              // no Zenith_EnvironmentAnchorComponent: the world origin is the presentation focus
	ENV_WARNING_TERRAIN_MISSING,             // a terrain component's asset failed: no terrain drawn, queries out of bounds
	ENV_WARNING_PREVIEW_HOLE_TILE_MISSING,   // a hole painted in a tile whose cook produced no hole tile shows after the next cook
};

constexpr const char* Zenith_ToString(Zenith_EnvironmentWarning eWarning);   // the enumerator's name
```

A warning is logged as `environment: <Zenith_ToString(eWarning)> <GUID or world tag>` in the category of "Log categories"; that name is the only name a diagnostic of this table carries anywhere in the family.

### Zenith_EnvironmentTypes header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentTypes.h
inline constexpr float fENV_TERRAIN_TILE_SIZE = 128.0f;
inline constexpr float fENV_TERRAIN_SAMPLE_SPACING = 0.5f;
inline constexpr uint32 uENV_TERRAIN_TILE_SAMPLES = 257;
inline constexpr uint32 uENV_TERRAIN_TILE_QUADS = 256;
inline constexpr uint32 uENV_TERRAIN_DENSITY_TEXELS = 128;
inline constexpr uint32 uENV_TERRAIN_MAX_TILES_PER_AXIS = 256;
inline constexpr uint32 uENV_TERRAIN_MAX_LAYERS = 16;
inline constexpr uint32 uENV_TERRAIN_MAX_FOLIAGE_TYPES = 64;
inline constexpr uint32 uENV_TERRAIN_MAX_EXPLICIT_INSTANCES_PER_TILE = 16384;
inline constexpr uint32 uENV_TERRAIN_MAX_PLACEMENT_RULESETS = 16;
inline constexpr uint32 uENV_EXCLUSION_ALL = 0xFFFFFFFFu;
inline constexpr uint32 uENV_FOLIAGE_TYPE_ALL = 0xFFFFFFFFu;
inline constexpr float fENV_PLACEMENT_CELL_SIZE = 128.0f;
inline constexpr uint32 uENV_MAX_GERSTNER_WAVES = 8;
inline constexpr uint32 uENV_MAX_WEATHER_STATES = 32;
inline constexpr uint32 uENV_WEATHER_STATE_NONE = 0xFFFFFFFFu;
inline constexpr uint32 uENV_MAX_ENVIRONMENT_COMMANDS_PER_TICK = 64;
inline constexpr uint32 uENV_TERRAIN_MAX_DETAIL_TYPES = 4;
inline constexpr uint32 uENV_DETAIL_MAX_LATTICE = 64;
inline constexpr uint32 uENV_MAX_RENDER_WATER_BODIES = 256;
inline constexpr uint32 uENV_MAX_ACTIVE_TERRAINS = 256;          // per world: the extractor slots of Terrain.md "Terrain record slots"
inline constexpr float fENV_DETAIL_WIND_PAD = 0.5f;               // metres: the detail material's wind-offset clamp and box pad

/// @contract value Zenith_TerrainTileCoord
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a key is copy-assigned wherever it is stored (records, containers,
///   sort keys).
/// Invalidation: None.
/// Units: Tile indices.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainTileCoord
{
	uint16 m_uX = 0;
	uint16 m_uZ = 0;
	friend constexpr bool operator==(Zenith_TerrainTileCoord, Zenith_TerrainTileCoord) = default;
};

/// @contract value Zenith_EnvCellCoord
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a key is copy-assigned wherever it is stored (records, containers,
///   sort keys).
/// Invalidation: None.
/// Units: Placement-cell indices (128 m) from the grid origin; signed for runtime grids.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EnvCellCoord
{
	sint32 m_iX = 0;
	sint32 m_iZ = 0;
	friend constexpr bool operator==(Zenith_EnvCellCoord, Zenith_EnvCellCoord) = default;
};

/// @contract value Zenith_TerrainDirtyRegion
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: an element of dirty-region arrays that the editing path appends to
///   and merges in place.
/// Invalidation: None.
/// Units: Sample or texel indices within the tile, inclusive minimum, exclusive maximum.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainDirtyRegion
{
	Zenith_TerrainTileCoord m_xTile;
	uint16 m_uMinS = 0, m_uMinT = 0, m_uMaxS = 0, m_uMaxT = 0;
	uint32 m_uChannelMask = 0;          // TERRAIN_CHANNEL_* bits: heights, layers, holes, density, exclusion, instances, presence
};

/// @contract value Zenith_ExplicitFoliageInstance
/// Ownership: Plain value stored in a tile.
/// Thread affinity: IMMUTABLE. Members non-const: an element of a tile's instance list, which the document reader and
///   the editor write in place.
/// Invalidation: None.
/// Units: Tile-local metres; radians.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ExplicitFoliageInstance
{
	uint32 m_uID = 0;                   // below 2^31
	uint32 m_uFoliageType = 0;
	float m_fX = 0.0f, m_fY = 0.0f, m_fZ = 0.0f;
	float m_fYaw = 0.0f;
	float m_fScale = 1.0f;
	bool m_bKeepHeight = false;
};

enum Zenith_TerrainEditDirection : uint8 { TERRAIN_EDIT_DIRECTION_REVERT = 1, TERRAIN_EDIT_DIRECTION_REAPPLY };
enum Zenith_TerrainEditKind : uint8 { TERRAIN_EDIT_KIND_STROKE = 1, TERRAIN_EDIT_KIND_TILE_ADD, TERRAIN_EDIT_KIND_TILE_REMOVE, TERRAIN_EDIT_KIND_LAYERS, TERRAIN_EDIT_KIND_LISTS };
enum Zenith_WaterBodyKind : uint8 { WATER_BODY_KIND_OCEAN = 1, WATER_BODY_KIND_LAKE, WATER_BODY_KIND_RIVER };
enum Zenith_PrecipitationKind : uint8 { PRECIPITATION_KIND_NONE = 0, PRECIPITATION_KIND_RAIN, PRECIPITATION_KIND_SNOW };
enum Zenith_CloudTier : uint8 { CLOUD_TIER_LAYER_LOW = 1, CLOUD_TIER_LAYER_HIGH, CLOUD_TIER_VOLUMETRIC };
enum Zenith_TimeOfDayMarker : uint8 { TIME_OF_DAY_MARKER_HOUR = 1, TIME_OF_DAY_MARKER_SUNRISE, TIME_OF_DAY_MARKER_SUNSET, TIME_OF_DAY_MARKER_DAWN, TIME_OF_DAY_MARKER_DUSK };

/// @contract enum Zenith_EnvironmentCommandKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (the time commands); the weather commands since Z2 Stage 4. Values are stable (recorded in
///   replays through their command kinds).
/// @endcontract
enum Zenith_EnvironmentCommandKind : uint8
{
	ENVIRONMENT_COMMAND_SET_TIME_OF_DAY = 1,
	ENVIRONMENT_COMMAND_SET_TIME_SCALE,
	ENVIRONMENT_COMMAND_REQUEST_WEATHER,
	ENVIRONMENT_COMMAND_FORCE_WEATHER,
	ENVIRONMENT_COMMAND_RELEASE_WEATHER,
};

/// @contract value Zenith_EnvironmentCommand
/// Ownership: Plain value; the payload of the five Environment command kinds.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Hours; a dimensionless scale; seconds.
/// Errors: Validated at playback: an out-of-range value is dropped with environment.commands_invalid and a log line.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EnvironmentCommand
{
	Zenith_EnvironmentCommandKind m_eKind = ENVIRONMENT_COMMAND_SET_TIME_OF_DAY;
	bool m_bHold = false;
	uint16 m_uReserved = 0;
	sint32 m_iDayDelta = 0;
	double m_dValue = 0.0;               // hours or scale
	uint64 m_ulStateNameHash = 0;       // weather commands
	float m_fTransitionSeconds = 0.0f;
};

/// @contract value Zenith_WeatherChangedEvent
/// Ownership: Plain value; the payload of the event "Zenith.Environment.WeatherChanged".
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: None.
/// Units: Ticks; the rain and snow intensities in the precipitation-rate units of Zenith_WeatherParams; metres per second;
///   radians (0 = +Z, positive toward +X). The four values are the TARGET state's parameters (the "to" state), at the
///   transition start and at the arrival alike, so a bridge (AUDIO's) reads them without reading ENV's world state.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WeatherChangedEvent
{
	uint64 m_ulFromNameHash = 0;       // 1 FromNameHash
	uint64 m_ulToNameHash = 0;         // 2 ToNameHash
	uint64 m_ulTransitionTicks = 0;    // 3 TransitionTicks
	bool m_bArrived = false;            // 4 Arrived
	float m_fRainIntensity = 0.0f;      // 5 RainIntensity: the target's m_fPrecipitationRate when its kind is PRECIPITATION_KIND_RAIN, else 0
	float m_fSnowIntensity = 0.0f;      // 6 SnowIntensity: the target's m_fPrecipitationRate when its kind is PRECIPITATION_KIND_SNOW, else 0
	float m_fWindSpeed = 0.0f;          // 7 WindSpeed: the target's m_fWindSpeed
	float m_fWindDirection = 0.0f;      // 8 WindDirection: the target's m_fWindDirection
};

/// @contract value Zenith_TimeOfDayEvent
/// Ownership: Plain value; the payload of the event "Zenith.Environment.TimeOfDay".
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: None.
/// Units: Hours.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TimeOfDayEvent
{
	Zenith_TimeOfDayMarker m_eMarker = TIME_OF_DAY_MARKER_HOUR;   // 1 Marker
	uint8 m_uHour = 0;                                          // 2 Hour
	double m_dHours = 0.0;                                       // 3 Hours
	uint64 m_ulDay = 0;                                         // 4 Day
};
```

Each enumeration above carries its own contract block in the header, abbreviated here for the five one-line enumerations. The two event payloads are reflected like the components: each member comment's number is its property ID and the word after it its stable name (`Zenith_WeatherChangedEvent` 1 `FromNameHash` … 8 `WindDirection`; `Zenith_TimeOfDayEvent` 1 `Marker` … 4 `Day`), the names AUDIO's event bridges match (`Zenith2/Docs/Design/AUDIO/Authoring.md`), so renaming or renumbering one is a breaking change to every bridge. The commands and events live in this leaf header, which includes only ZenithCore headers, so `Zenith_WeatherSystem.h` and `Zenith_EnvironmentState.h` include it without a cycle. The other value types appear with the API that takes them, each declared in full with its contract block:

| Types | Header | Declared in |
|---|---|---|
| `Zenith_PlacementFilter`, `Zenith_PlacementRule`, `Zenith_PlacementRuleSet`, `Zenith_PlacementFoliageTypeInfo`, `Zenith_PlacementSurfaceSample`, `Zenith_PlacementExclusionShape`, `Zenith_PlacementExclusion`, `Zenith_PlacementObstacle`, `Zenith_PlacementStats` | `Zenith_ProceduralPlacement.h` | `Zenith2/Docs/Design/ENV/Procedural.md → "Placement value types"` |
| `Zenith_NavigationArea`, `Zenith_NavigationGeometry`, `Zenith_WaterBodyShape`, `Zenith_FoliageCollisionShape`, `Zenith_FoliageCollisionDesc`, `Zenith_FoliageCollisionInstance`, `Zenith_FoliageCollisionSet`, `Zenith_TerrainRegenerateInputs`, `Zenith_TerrainRegeneratedTile` | `Zenith_EnvironmentGeneration.h` | `Zenith2/Docs/Design/ENV/Bake.md → "Heightfield extraction"`, "Regeneration after edits" |
| `Zenith_WeatherTransitionDesc` | `Zenith_WeatherSystem.h` | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Zenith_WeatherSystem header"` |

### Zenith_EnvironmentComponents header

Every component is a C++ struct registered in `Zenith_Module_Environment_RegisterTypes` with `NETWORK_AUTHORITY_SERVER` and `PROPERTY_FLAG_SERIALIZED` on every property; asset-handle properties carry the asset lifecycle hooks ([Z2-N-ARCH-645]); games author them in scenes, and data-defined schemas may reference them by type like any registered type (Z2-D-020). The comment of each member is its property ID and stable property name, which are stable data (a rename or a re-numbering is a save migration step); every property's authority is `PHASE_COMMANDS_ONLY` except where a comment names a phase ([Z2-N-ARCH-482]).

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentComponents.h
#pragma once
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"

class Flux_TerrainResource; class Zenith_TerrainTileAsset; class Zenith_FoliageCellAsset; class Zenith_AtmosphereAsset;
class Zenith_WeatherAsset; class Zenith_WaterAsset; class Zenith_DataAsset;   // Bake.md "Runtime asset objects"; ASSET's Zenith.Data

/// @contract value Zenith_TerrainComponent
/// Ownership: Plain component value stored by the ECS backend as "Zenith.Environment.Terrain"; one per terrain, in the
///   always-resident layer; its handle is balanced by the asset lifecycle hooks.
/// Thread affinity: Written by command playback only; read by Environment.UpdateTerrainTiles (phase 1) and the extractors.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a failed asset is ENV_WARNING_TERRAIN_MISSING ("Failure table").
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_TerrainComponent
{
	Zenith_AssetHandle<Flux_TerrainResource> m_xTerrain;    // 1 Terrain (property 2 is never assigned: GEOM's nodes always cast shadows)
};

/// @contract value Zenith_TerrainTileComponent
/// Ownership: Plain component value "Zenith.Environment.TerrainTile"; one per present tile, in its cell's content.
/// Thread affinity: As Zenith_TerrainComponent.
/// Invalidation: None.
/// Units: Tile indices.
/// Errors: None; a tile whose terrain is absent is ignored and counted in environment.terrain.height_queries_not_resident.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_TerrainTileComponent
{
	Zenith_PersistentID m_xTerrainDocument;                  // 1 TerrainDocument: the terrain document's id
	uint16 m_uTileX = 0;                                    // 2 TileX
	uint16 m_uTileZ = 0;                                    // 3 TileZ
	Zenith_AssetHandle<Zenith_TerrainTileAsset> m_xTile;     // 4 Tile
};

/// @contract value Zenith_FoliageCellComponent
/// Ownership: Plain component value "Zenith.Environment.FoliageCell"; one per environment cell with foliage.
/// Thread affinity: Written by command playback only; read by Environment.UpdateFoliageCells and the foliage extractor.
/// Invalidation: None.
/// Units: Placement-cell indices.
/// Errors: None; a failed cell stays empty (environment.foliage.cells_failed).
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_FoliageCellComponent
{
	Zenith_PersistentID m_xTerrainDocument;                  // 1 TerrainDocument
	sint32 m_iCellX = 0;                                    // 2 CellX
	sint32 m_iCellZ = 0;                                    // 3 CellZ
	Zenith_AssetHandle<Zenith_FoliageCellAsset> m_xCell;     // 4 Cell
};

/// @contract value Zenith_WaterBodyComponent
/// Ownership: Plain component value "Zenith.Environment.WaterBody"; the body's position is its entity's world position.
/// Thread affinity: Written by command playback only; read by Environment.UpdateWaterBodies.
/// Invalidation: None.
/// Units: World metres.
/// Errors: An invalid shape (a lake polygon wider than 4,096 m on an axis included) is ENV_ERROR_INVALID_DESC at the body's
///   activation; the body is not added and is logged once.
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_WaterBodyComponent
{
	uint8 m_uKind = WATER_BODY_KIND_LAKE;                   // 1 Kind: a Zenith_WaterBodyKind value
	float m_fBaseHeight = 0.0f;                              // 2 BaseHeight, world Y
	float m_fHalfExtentX = 0.0f;                             // 3 HalfExtentX: an ocean's bounding rectangle about the entity; 0 = unbounded
	float m_fHalfExtentZ = 0.0f;                             // 4 HalfExtentZ
	Zenith_AssetHandle<Zenith_DataAsset> m_xShape;           // 5 Shape: a lake's polygon or a river's spline (below)
	Zenith_AssetHandle<Zenith_WaterAsset> m_xWater;          // 6 Water
	bool m_bUnderwaterEnabled = true;                        // 7 UnderwaterEnabled
};

/// @contract value Zenith_EnvironmentSettingsComponent
/// Ownership: Plain component value "Zenith.Environment.Settings"; at most one per world (a second is ignored and logged).
/// Thread affinity: Written by command playback only; read by Environment.Advance.
/// Invalidation: None.
/// Units: None.
/// Errors: None; its absence leaves LIGHT's defaults and no weather.
/// Version: since Z2 Stage 3 (the weather members since Z2 Stage 4); type version 1.
/// @endcontract
struct Zenith_EnvironmentSettingsComponent
{
	Zenith_AssetHandle<Zenith_AtmosphereAsset> m_xAtmosphere;   // 1 Atmosphere
	Zenith_AssetHandle<Zenith_WeatherAsset> m_xWeather;         // 2 Weather
	bool m_bDriveSunAndMoon = true;                              // 3 DriveSunAndMoon
	bool m_bWeatherEnabled = true;                               // 4 WeatherEnabled
};

/// @contract value Zenith_EnvironmentAnchorComponent
/// Ownership: A tag component "Zenith.Environment.Anchor" with no property; at most one per world (the lowest entity ID wins).
/// Thread affinity: Read by Environment.Advance and the extractors, with its entity's world transform.
/// Invalidation: None.
/// Units: None.
/// Errors: None; its absence is ENV_WARNING_ANCHOR_MISSING.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_EnvironmentAnchorComponent
{
};

/// @contract value Zenith_PlacementExclusionComponent
/// Ownership: Plain component value "Zenith.Environment.PlacementExclusion"; the shape's centre and yaw are its entity's.
/// Thread affinity: Written by command playback only; read by the terrain importer's Foliage stage and the editor preview.
/// Invalidation: None.
/// Units: Metres.
/// Errors: Validated by Zenith_Place (ENV_ERROR_INVALID_DESC with the exclusion index).
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_PlacementExclusionComponent
{
	uint8 m_uShape = 2;                                     // 1 Shape: a Zenith_PlacementExclusionShape value (2 = disc)
	float m_fHalfExtentX = 0.0f;                             // 2 HalfExtentX (box)
	float m_fHalfExtentZ = 0.0f;                             // 3 HalfExtentZ (box)
	float m_fRadius = 0.0f;                                  // 4 Radius (disc)
	float m_fWidth = 0.0f;                                   // 5 Width (polyline)
	Zenith_AssetHandle<Zenith_DataAsset> m_xPolyline;        // 6 Polyline (below)
};

/// @contract value Zenith_WindSourceComponent
/// Ownership: Plain component value "Zenith.Environment.WindSource".
/// Thread affinity: Written in phase 2 (every property's authority is PHASE_EARLY_SCRIPTS) and by commands; read by
///   Environment.Advance of the next tick.
/// Invalidation: None.
/// Units: Metres; m/s; seconds.
/// Errors: None; out-of-range values clamp to the documented ranges.
/// Version: since Z2 Stage 5; type version 1.
/// @endcontract
struct Zenith_WindSourceComponent
{
	uint8 m_uKind = 1;                                      // 1 Kind: 1 radial, 2 directional along the entity's +Z
	float m_fRadius = 10.0f;                                 // 2 Radius, 0.1 … 1,000
	float m_fStrength = 10.0f;                               // 3 Strength, m/s at the centre, 0 … 100
	float m_fFalloff = 1.0f;                                 // 4 Falloff exponent, 0.1 … 8
	float m_fDurationSeconds = 0.0f;                         // 5 DurationSeconds, 0 = until removed
};

/// @contract value Zenith_LocalFogVolumeComponent
/// Ownership: Plain component value "Zenith.Environment.LocalFogVolume"; its fields are LIGHT's local fog volume
///   (Zenith2/Docs/Design/LIGHT/Volumetrics.md "Local fog volumes"), its transform the entity's.
/// Thread affinity: Written by command playback only; read by Environment.Advance.
/// Invalidation: None.
/// Units: Metres; per-metre densities.
/// Errors: None.
/// Version: since Z2 Stage 5; type version 1.
/// @endcontract
struct Zenith_LocalFogVolumeComponent
{
	uint8 m_uShape = 1;                                     // 1 Shape: 1 box, 2 ellipsoid
	Zenith_Vector3 m_xHalfExtents = { 5.0f, 5.0f, 5.0f };    // 2 HalfExtents
	float m_fDensity = 0.02f;                                // 3 Density
	float m_fHeightFalloff = 0.0f;                           // 4 HeightFalloff
	float m_fAlbedo = 0.9f;                                  // 5 Albedo
	float m_fAnisotropy = 0.0f;                              // 6 Anisotropy
	Zenith_Colour m_xEmission;                               // 7 Emission
	float m_fEdgeFade = 0.1f;                                // 8 EdgeFade, a fraction of the extent
};
```

A `Shape` or `Polyline` asset is a `Zenith.Data` asset (ASSET's generic kind) whose bytes are a little-endian `u32` point count followed by the points relative to the entity's position: a lake's (x, z) `f32` pairs (3 … 256, a simple counter-clockwise polygon), a river's (x, z, width, height) `f32` quadruples (Stage 5, 2 … 1,024 spline control points), an exclusion polyline's (x, z) pairs (2 … 1,024). The anchor is the presentation focus: effect prefabs attach to it, and detail foliage, foliage activation order, local fog volumes and the cloud-shadow square centre on its world position; games put it on the main camera rig and EDIT on the active editor camera.

### Zenith_EnvironmentState header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentState.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"
#include "ZenithEnvironment/Zenith_TimeOfDay.h"
#include "ZenithEnvironment/Zenith_WeatherSystem.h"
#include "ZenithEnvironment/Zenith_Wind.h"
#include "ZenithEnvironment/Zenith_AtmosphereParameters.h"

/// @contract type Zenith_EnvironmentState
/// Ownership: One per world, owned by the Environment module instance and registered as the world resource
///   "Zenith.Environment.State" (TYPE_KIND_RESOURCE) with owning phase PHASE_INPUT; its reflected properties are saved.
/// Thread affinity: PHASE(Input) for writes (Environment.Advance alone; command playback appends to the pending array at
///   sync points); reads from any system declaring ReadResource, and from the extractors.
/// Invalidation: Values change only at phase 1.
/// Units: As its members.
/// Errors: None; commands are validated at playback.
/// Version: since Z2 Stage 3 (clock and atmosphere); weather and wind members since Z2 Stage 4. A property change is a
///   save migration step (Zenith2/Docs/Contracts.md "Compatibility policy per kind").
/// @endcontract
class Zenith_EnvironmentState
{
public:
	const Zenith_TimeOfDay& GetTimeOfDay() const;
	const Zenith_CelestialState& GetCelestial() const;     // at the tick's clock
	const Zenith_WeatherRuntimeState& GetWeather() const;
	const Zenith_WindState& GetWind() const;
	const Zenith_AtmosphereParameters& GetAtmosphere() const; // after weather modulation
	Zenith_Vector3 GetSunGroundIlluminance(float fAltitudeMetres) const;   // lux, per channel
	uint64 GetTick() const;
};
```

### Zenith_Wind header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_Wind.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithEnvironment/Zenith_WeatherSystem.h"     // Zenith_WeatherParams

/// @contract value Zenith_WindState
/// Ownership: Plain value, part of Zenith_EnvironmentState; saved.
/// Thread affinity: PHASE(Input) for writes; IMMUTABLE to readers within a tick.
/// Invalidation: None.
/// Units: m/s, radians, Hz, fractions.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WindState
{
	float m_fDirectionX = 0.0f;          // unit XZ
	float m_fDirectionZ = 1.0f;
	float m_fSpeed = 0.0f;
	float m_fGustStrength = 0.0f;
	float m_fGustFrequency = 0.1f;
	float m_fTurbulence = 0.0f;
	float m_fWanderAngle = 0.0f;
	float m_fWanderRate = 0.02f;         // radians per second
};

/// @contract functions Zenith_WindFunctions
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: m/s; seconds of simulation time.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Vector3 Zenith_EvaluateWindCPU(const Zenith_WindState& xWind, const Zenith_Vector3& xPositionWS, double dTimeSeconds);
void Zenith_StepWind(const Zenith_WeatherParams& xWeather, uint64 ulWorldSeed, uint64 ulTick, float fTickSeconds, Zenith_WindState& xWind);
```

### Zenith_EnvironmentDocuments header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentDocuments.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"
#include "ZenithEnvironment/Zenith_AtmosphereParameters.h"   // Zenith_AtmosphereParameters
#include "ZenithEnvironment/Zenith_TimeOfDay.h"              // Zenith_TimeOfDaySettings
#include "ZenithEnvironment/Zenith_WaterSystem.h"            // Zenith_WaveSet
#include "ZenithEnvironment/Zenith_WeatherSystem.h"          // Zenith_WeatherParams
#include "ZenithEnvironment/Zenith_EnvironmentGeneration.h"  // Zenith_NavigationArea, Zenith_FoliageCollisionShape

class Zenith_Arena;

// The document value types of Zenith2/Docs/Design/ENV/Bake.md "Document value types" are declared here, first.

/// @contract functions Zenith_EnvironmentDocuments
/// Ownership: Readers return values whose spans and strings are allocated from the caller's arena.
/// Thread affinity: ANY_THREAD (each call uses only its arguments).
/// Invalidation: Returned spans and strings are valid until the arena is reset.
/// Units: SI after conversion (degrees become radians, kilometres metres, per-kilometre coefficients per metre).
/// Errors: ENV_ERROR_DOCUMENT_PARSE, ENV_ERROR_DOCUMENT_SCHEMA with the field index, ENV_ERROR_DOCUMENT_MIGRATION for a
///   version older than the current one (tools pass bytes SCENE's migrator opened, which ran the chain); a newer
///   schema version reads with ENV_WARNING_NEWER_SCHEMA; each unknown member is skipped with one
///   MIGRATION_WARNING_UNKNOWN_FIELD and stays in the source JSON, which is never rewritten by a reader.
/// Version: since Z2 Stage 3 (foliage type, atmosphere, terrain layer); weather, water and cloud noise since Z2 Stage 4.
///   Each document type carries a uint32 schema version and a tool-side migration chain ([Z2-N-CONTRACT-530]).
/// @endcontract
Zenith_Result<Zenith_FoliageTypeDocument, Zenith_EnvironmentError> Zenith_ReadFoliageTypeDocument(Zenith_ByteSpan xJson, Zenith_Arena& xArena);
Zenith_Result<Zenith_AtmosphereDocument, Zenith_EnvironmentError> Zenith_ReadAtmosphereDocument(Zenith_ByteSpan xJson, Zenith_Arena& xArena);
Zenith_Result<Zenith_WeatherDocument, Zenith_EnvironmentError> Zenith_ReadWeatherDocument(Zenith_ByteSpan xJson, Zenith_Arena& xArena);
Zenith_Result<Zenith_WaterDocument, Zenith_EnvironmentError> Zenith_ReadWaterDocument(Zenith_ByteSpan xJson, Zenith_Arena& xArena);
Zenith_Result<Zenith_TerrainLayerDocument, Zenith_EnvironmentError> Zenith_ReadTerrainLayerDocument(Zenith_ByteSpan xJson, Zenith_Arena& xArena);
Zenith_Result<Zenith_CloudNoiseDocument, Zenith_EnvironmentError> Zenith_ReadCloudNoiseDocument(Zenith_ByteSpan xJson, Zenith_Arena& xArena);
```

The placement reader is `Zenith_ReadPlacementRuleSet` (Procedural.md) and the terrain document's reader is `Zenith_TerrainDocument::Load`. The nine types are registered with SCENE's `Zenith_MigrationRegistry` (the Q43 ruling) by tool-side code, `Zenith_RegisterEnvironmentDocumentTypes` with its descs and canonical schemas (`Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"`), so ZenithEnvironment has no `ZenithDocument` edge. ENV declares no writer for these six document types: EDIT's inspectors edit a document as a JSON tree through SCENE's document framework (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_CanonicalJson header"`), which keeps every unknown member at its anchor and writes through the journaled write, and validate the edited tree with the reader before the write is committed; the typed values are what the cook and the inspectors' validation read. The terrain document is the exception: it is written by `Zenith_TerrainDocument::Save`.

### Handle kinds used

| Handle | Kind (`Zenith2/Docs/Contracts.md → "Handle kinds"`) | ENV's use |
|---|---|---|
| `Zenith_AssetHandle<T>` | shared asset handle | terrain, tile, foliage cell, atmosphere, weather, water, cloud-noise, data and mesh assets; component properties serialize as GUIDs |
| `Zenith_PersistentID` | persistent ID | documents, layers, foliage types, terrain identity in tile components |
| `Zenith_EntityHandle` | world handle | the environment anchor entity and the spawned effect prefabs (never serialized) |
| `Flux_RenderInstanceKey`, `Flux_RenderLightKey` | render-world keys (not a handle kind) | foliage, detail instances, water nodes, sun and moon |
| `Flux_MaterialID`, `Flux_MeshID`, `Flux_TextureID` | Flux runtime identities (GEOM's, never serialized) | the generated materials, the water meshes, the tile textures on the render side |
| `Zenith_OperationHandle` | operation handle | none: ENV issues no asynchronous operation of its own; asset loads are ASSET's |

## Data structures and formats

### Format index

| Format | Kind | Specified in |
|---|---|---|
| `*.terrain.json`, `*.terraintile.json`, `*.terrainlayer.json` | authoring documents | `Zenith2/Docs/Design/ENV/Bake.md → "Authoring document schemas"` |
| `*.foliage.json` | authoring document | `Zenith2/Docs/Design/ENV/Foliage.md → "Foliage type document"` |
| `*.placement.json` | authoring document | `Zenith2/Docs/Design/ENV/Procedural.md → "Placement rule document"` |
| `*.atmosphere.json` | authoring document | `Zenith2/Docs/Design/ENV/Atmosphere.md → "Atmosphere document"` |
| `*.weather.json`, `*.water.json` | authoring documents | `Zenith2/Docs/Design/ENV/WaterWeather.md` |
| `*.cloudnoise.json` | authoring document (engine content) | `Zenith2/Docs/Design/ENV/Bake.md → "Cloud noise document schema"` |
| `.zterrain`, `.zenvtile`, `.zfoliage`, `.zatmos`, `.zweather`, `.zwater`, `.zcloudnoise` | cooked, versioned | `Zenith2/Docs/Design/ENV/Bake.md → "Cooked formats"` |
| the render-world records | in-memory, per frame | "Render-world records" |
| the environment state's saved properties | save data through reflection | "Zenith_EnvironmentState header" |

### ENV cooked file header

ENV adds no header of its own: every ENV cooked file uses ASSET's cooked container header and 40-byte section entries with COMPRESS's descriptor (the layout GEOM's files use, `Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`), with the magics `ZTRN`, `ZETL`, `ZFOL`, `ZATM`, `ZWTR`, `ZWAT`, `ZCLN`, format version 1 each, and the section kinds of `Zenith2/Docs/Design/ENV/Bake.md → "Section kind registry"`. A reader refuses a mismatched format or engine cook version with a recoverable asset error ([Z2-N-CONTRACT-528]).

### Byte order and serialization rules

- Every multi-byte value in an ENV cooked section is little-endian and written field by field; no section is a `memcpy` of a C++ type (Z2-D-038).
- Floats are IEEE binary32 unless a layout says `f64`; GUIDs are 16 bytes, high word then low word ([Z2-N-CONTRACT-067]).
- Section order is the registry's (kind ascending), 16-byte aligned.
- Authoring payloads inside JSON are base64 of a COMPRESS section; a `uint64` in JSON is a hexadecimal string.

### In-memory sizes that matter

| Datum | Size | Note |
|---|---|---|
| authored tile heights | 264,196 bytes | 257² floats |
| a layer weight, hole or exclusion map | 65,536 bytes | 256² u8 |
| a density map | 16,384 bytes | 128² u8 |
| cooked tile CPU data | 132,098 + 65,536 + 8,192 bytes | per resident tile |
| a foliage instance | 32 bytes | `Zenith_FoliageInstance` |
| W-Outdoor's foliage CPU data | 3.2 MB for 100k instances | plus the type tables |
| `Zenith_EnvironmentState` | under 512 bytes | per world |
| `Flux_EnvironmentBlocks` | under 12 KiB (the 64 local fog volumes dominate) | per world, copied once per frame |
| a terrain's tile table (`Env_TerrainTileTextures`) | 64 × tiles bytes | per active terrain, GPU |
| the cloud-noise volumes | 9,586,980 bytes (128³ RGBA8 with its chain) and 149,796 bytes (32³) | GPU, while an atmosphere with clouds is active |
| detail foliage in GEOM's pool | at most `detailMaxVertices` × 20 bytes plus indices | `Zenith2/Docs/Design/ENV/Foliage.md → "Detail pool sizing"` |

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Heading |
|---|---|---|
| brush application | O(dabs × samples per dab) | `Zenith2/Docs/Design/ENV/Terrain.md → "Brush operations"` |
| weight normalisation | O(layers per texel) | `Zenith2/Docs/Design/ENV/Terrain.md → "Brush operations"` |
| splat top-four reduction | O(texels × layers log layers) | `Zenith2/Docs/Design/ENV/Terrain.md → "Splat layers and blending"` |
| height query | O(1) | `Zenith2/Docs/Design/ENV/Terrain.md → "Height queries"` |
| foliage activation | O(instances) per cell, rate-limited | `Zenith2/Docs/Design/ENV/Foliage.md → "Placed instance streams"` |
| tier thinning | O(1) per instance | `Zenith2/Docs/Design/ENV/Foliage.md → "Density per quality tier"` |
| wind | O(1) per tick; O(1) per sample | `Zenith2/Docs/Design/ENV/Foliage.md → "Wind model"` |
| Gerstner evaluation and height query | O(waves × 5) | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Wave synthesis"` |
| weather state machine | O(transitions of a state) per event | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather state machine"` |
| time-of-day advance | O(1) | `Zenith2/Docs/Design/ENV/Atmosphere.md → "Time of day"` |
| solar position | O(1) | `Zenith2/Docs/Design/ENV/Atmosphere.md → "Solar and lunar position"` |
| procedural placement | O(rules × (cells + halo) × candidates × neighbours) | `Zenith2/Docs/Design/ENV/Procedural.md` |
| collision and navigation extraction | O(samples) per tile | `Zenith2/Docs/Design/ENV/Bake.md` |

### State machines

| Machine | Heading |
|---|---|
| weather (states × events → next state and action) | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather state machine"` |
| terrain and tile activation | `Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header"` |
| foliage cell activation | `Zenith2/Docs/Design/ENV/Foliage.md → "Placed instance streams"` |
| edit revisions (revert and reapply) | `Zenith2/Docs/Design/ENV/Terrain.md → "Terrain edit deltas and undo"` |

### Tick-level flow of ENV work

In each world's phase 1, in this order (declared with `Before`):

1. `Environment.Advance`: consume the pending commands in merge order; advance the clock; compute the celestial state; step the weather machine; step the wind; converge wetness and snow; record effect-prefab spawn and destroy commands; emit weather and time-of-day events; then build the render-blocks resource (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Extraction steps"`).
2. `Environment.UpdateWaterBodies`: refresh the water resource from changed water components and the new wind (amplitude scales).
3. `Environment.UpdateTerrainTiles`: request assets of new terrain and tile components; activate tiles whose assets became active at this tick boundary; deactivate removed ones.
4. `Environment.UpdateFoliageCells`: the same for foliage cell components.

In phase 12: `Environment.ExtractEnvironment`, then `Environment.ExtractFoliage` (registration order); RPROD's `Render.ExtractEnvironment` reads the render-blocks resource in the same phase; in an edit world, EDIT's extractor writes the preview records.

### Frame-level flow of ENV render work

On `Flux.BuildGraph` of RENDER(k−1), after GEOM's load finalisation and before the GPU scene applies the frame's deltas: `Flux_EnvironmentRenderer` applies terrain records (`Flux_TerrainRenderer::AddTerrain` with the record and slot tables of `Zenith2/Docs/Design/ENV/Terrain.md → "Terrain record slots"`, `RemoveTerrain`, `UpdateNodeBounds`, the preview's texture replacements), rewrites changed entries of the terrain tile tables, applies water-body records, selects water nodes and updates their GPU-scene instances, and writes the frame's `Env_FrameEnvironment` (wind time, water phases, terrain records, the water-body table) into the upload ring; GEOM's procedural system applies the detail instances' records and dispatches `Flux_DetailFoliageSource`'s program in `Flux_AddProceduralGeometry`; ENV's passes join the frame at their slots (RP20, X127 (3)).

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_TerrainDocument`, `Zenith_TerrainEdit` | `OWNER_THREAD` (the editor's main thread or a cook worker); sub-jobs inside `ApplyStroke` |
| `Zenith_TerrainPreview` | `MAIN_THREAD` for `QueueRegions` and `Prepare`; `WriteRecords` from EDIT's extractor node of the edit world |
| the four world resources | `PHASE(Input)` for writes, reads from any phase through declared access |
| `Zenith_PlacementSurface` implementations | `ANY_THREAD` pure reads |
| `Zenith_Place` | any thread; sub-jobs joined before return |
| the kind loaders | ASSET's loader threading (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"`) |
| `Flux_EnvironmentRenderer`, `Flux_DetailFoliageSource` | `RENDER_SUBMIT_THREAD` (the render subgraph's nodes; the source's members run on `Flux.BuildGraph` alone, GEOM's contract) |
| the render-blocks resource | `PHASE(Input)` for writes (`Environment.Advance`); read by `Render.ExtractEnvironment` in phase 12 |
| the module instance | `MAIN_THREAD` |

### Frame-graph placement

| Node | Kind | Phase | Declared access |
|---|---|---|---|
| `Environment.Advance` | system | 1 | writes `Zenith.Environment.State` and `Zenith.Environment.RenderBlocks`; reads the settings and anchor components and the anchor's transform; emits the two events; records structural commands (effect prefabs) |
| `Environment.UpdateWaterBodies` | system | 1 | reads `Zenith_WaterBodyComponent`, `Zenith.Environment.State`; writes `Zenith.Environment.WaterWorld` |
| `Environment.UpdateTerrainTiles` | system | 1 | reads the terrain and tile components; writes `Zenith.Environment.TerrainWorld` |
| `Environment.UpdateFoliageCells` | system | 1 | reads the foliage cell components; writes `Zenith.Environment.FoliageWorld` |
| `Environment.ExtractEnvironment` | extractor | 12 | reads the state, the terrain and water resources, the settings component, the anchor's transform and the view requests' camera transforms; writes its render-world writer partition and its own bookkeeping (emitted keys, terrain slots), never a world resource |
| `Environment.ExtractFoliage` | extractor | 12 | reads the foliage and terrain resources, the anchor's transform; its partition (placed instances, detail instances and GEOM's procedural records) |

The four phase-1 systems write disjoint resources and run single-threaded each; ENV adds no sub-job inside simulation nodes.

<!-- assumption: pending ARCH, ECS (X126, X84) -->
Phase 1's typical nodes are "action mapping, context stack, replay-log read or write" (`Zenith2/Docs/Architecture/FramePhases.md → "The twelve phases"`), and the admitted owning-phase range of module world resources is pending the ECS revision (X84). ENV assumes that module systems advancing world-clock-like state run in phase 1 and that its five world resources may declare `PHASE_INPUT` (X126); if the ruling names another phase, the four systems and the five resources move to it together, and the one-tick command latency of "Commands" in Atmosphere.md is kept by recording commands in the phases after it.
<!-- /assumption -->

### Lock-order entries

ENV adds no lock: its world objects are written by one node each, its bookkeeping by one extractor, and its editor objects are owner-thread.

### Construction and shutdown

Construction is the entry-point table of "Zenith_Module_Environment header"; each step unwinds what it built. Shutdown: `DetachWorld` releases every asset handle, destroys the world resources and removes the effect prefabs' records; the module instance is destroyed at shutdown step 6 of `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`; ENV starts no thread.

### Cancellation

ENV owns no asynchronous operation. Cancelling a cell load (STREAM) or an asset load (ASSET) reaches ENV only as a component or asset that never becomes active; ENV holds its handle until the component disappears and then releases it, so a cancel leaves nothing of ENV's behind ("Activation and cancellation" in Bake.md). An editor stroke is synchronous and cannot be cancelled mid-dab; EDIT undoes a finished stroke instead.

### Error enumerations

`Zenith_EnvironmentErrorCode` ("Zenith_EnvironmentErrors header"). ENV maps it to `Zenith_ModuleError` at the module boundary and places its code in the detail field of ASSET's `Zenith_AssetError` inside kind loaders ([Z2-N-CONTRACT-463]). Cook-time failures use ASSET's existing codes and add none: an importer stage fails with `COOK_ERROR_IMPORT_FAILED` (an ERROR diagnostic naming the `Zenith_EnvironmentErrorCode`), a step with `COOK_ERROR_STEP_FAILED` (the code in `m_uDetail`), as `Zenith2/Docs/Design/ENV/Bake.md → "Cook determinism"` lists.

### Luau errors and asserts

The binding table `Environment` exposes: `Environment.GetTimeOfDay()` → hours, day; `Environment.SetTimeOfDay(hours[, dayDelta])`; `Environment.SetTimeScale(scale)`; `Environment.GetWeather()` → current name, target name, blend; `Environment.RequestWeather(name, seconds)`; `Environment.ForceWeather(name, seconds, hold)`; `Environment.ReleaseWeather()`; `Environment.GetWind(position)` → vector; `Environment.SampleTerrainHeight(x, z)` → height or `nil`; `Environment.SampleWater(x, z)` → table or `nil`. Setters record commands (visible at the next tick). Misuse raises a controlled Luau error naming the argument, never an assert ([Z2-N-CONTRACT-473]): a non-finite number, hours outside [0, 24), a scale outside [0, 10,000], a transition outside [0, 3,600] s, an unknown weather name, a call from a chunk system of the wrong phase for a setter (commands are allowed in phases 2 to 10). Asserts guard only ENV's own preconditions (a span of the wrong size passed by engine code, an out-of-range tile coordinate from engine code).

## Diagnostics and profiling

### Counters

All `COUNTER_KIND_CUMULATIVE` unless the name ends in `_delta` ([Z2-N-CONTRACT-156]):

| Counter | Meaning |
|---|---|
| `environment.terrain.tiles_resident` | active tile CPU data |
| `environment.terrain.height_queries_delta`, `environment.terrain.height_queries_not_resident` | queries this frame, and those refused |
| `environment.terrain.brush_clamped`, `environment.terrain.borders_repaired`, `environment.terrain.edit_over_budget` | editor diagnostics |
| `environment.foliage.cells_active`, `environment.foliage.cells_failed` | foliage cells |
| `environment.foliage.instances_resident`, `environment.foliage.instances_shed`, `environment.foliage.activation_backlog` | placed instances in the GPU scene, shed by the cap, waiting for activation |
| `environment.foliage.detail_cells`, `environment.foliage.detail_vertices`, `environment.foliage.detail_cells_capped` | detail instances resident, their vertices, and cells refused by `detailMaxVertices` |
| `environment.water.bodies`, `environment.water.nodes`, `environment.water.submerged_views_dropped` | bodies, selected water nodes, and submerged views beyond ENV's per-world cap of 16 that got no record |
| `environment.terrain.slots_full` | terrain ADDs not emitted because 256 terrains of the world were live |
| `Flux.Environment.PreviewPublishedFrame` | <!-- elaboration --> `COUNTER_KIND_GAUGE`, set by ENV's GPU side in `Flux.BuildGraph` ("Live preview" step 4 of `Zenith2/Docs/Design/ENV/Terrain.md`): the frame index of the latest frame in which an accepted preview height chain replacement became visible, which is the frame whose `ReplaceChainFromMemory` call returned success and changed the height texture's `GetSRV()`. 0 until the first replacement; a refused or retained chain does not move it; it never decreases. The editor compares it with the frame index of a dab's `ApplyStroke` to find the dab's end event (A3 cross-area row ED20). One editor session previews one terrain at a time, so the gauge is process-wide |
| `environment.weather.state`, `environment.weather.transitions`, `environment.weather.requests_refused_held` | weather |
| `environment.tod.hours_x1000` | clock in thousandths of an hour |
| `environment.commands_invalid` | dropped commands |
| `environment.placement.candidates`, `.kept`, `.rejected_<filter>` (one per `Zenith_PlacementFilter`: surface, exclusion, height, slope, water, layer, acceptance, spacing, cross_rule) | per `Zenith_Place` call (editor and cook) |

### Log categories

`environment` (runtime), `environment.terrain`, `environment.foliage`, `environment.weather` (runtime), `cook.environment` (cook). Warnings of `Zenith_EnvironmentWarning` log once per asset or document (per world for the anchor and scene-sun warnings) per run at `LOG_LEVEL_WARNING` with the GUID ([Z2-N-CONTRACT-464]), named by `Zenith_ToString` of the value; the render side's `Flux.Environment.*` counters are the other diagnostics names ENV uses.

### Profiler zones

`Environment.Advance`, `Environment.UpdateTerrainTiles`, `Environment.UpdateFoliageCells`, `Environment.UpdateWaterBodies`, `Environment.ExtractEnvironment`, `Environment.ExtractFoliage` (node zones, [Z2-N-BUDGET-223]); `Env.Brush.Dab`, `Env.Placement.Stage` (sub-job zones); on the render side `Flux.Environment.Apply`, `Flux.Environment.WaterNodes`; the GPU passes get timestamps from the graph.

### On-screen profiler rows

An "Environment" panel: time of day and day, weather state and blend, wind speed and direction, tiles resident, foliage instances resident and shed and the activation backlog, detail cells, water bodies and nodes, the cloud tier; each labelled with the budget rows it feeds (`Z2-B-ENV-001` and the pending ENV rows).

### Debug views

| Variable | Shows |
|---|---|
| `r.debug.env.splat 1` / `2` | layer weights as colours / the strongest layer's index |
| `r.debug.env.foliageDensity` | the painted density of the selected type |
| `r.debug.env.wind` | wind vectors on a grid around the camera |
| `r.debug.env.waterNodes` | water node boundaries and levels |
| `r.debug.env.cloudCoverage` | the coverage field |
| `r.debug.env.tiles` | tile residency (active, requested, absent) |
| `r.debug.env.placement` | editor: candidates coloured by first failing filter |

### Frame-dump entries

ENV adds an `environment` object to the render graph's frame dump (`Zenith2/Docs/Rendering/RenderGraph.md → "Frame dump"`): terrains and resident tiles, foliage instance counts per type, detail instances and vertices, water nodes per body, the cloud tier and the frame's `m_uSkyRevision`.

### Cook report members

`environment`: per terrain the tight height range, tile count, `splatTexelsOverFour`, `borderMismatches` (0 in a successful cook); per foliage cell the instance count per type, `placedPerRule` (achieved density per global stage) and `shedByCapacity`; per parameter document its validation result; `permutations` (the variant count of ENV's materials and passes for the game).

### Configuration keys

| Key | Type | Default | Allowed layers | Restart | Simulation-affecting |
|---|---|---|---|---|---|
| `environment.foliage.densityScale` | float 0 … 1 | 1.0 (S24 platform layer 0.6) | all | no | no |
| `environment.foliage.detailDensityScale` | float 0 … 1 | 1.0 (S24 0.5) | all | no | no |
| `environment.foliage.detailRadius` | float 8 … 256 | 96 (S24 48) | all | no | no |
| `environment.foliage.cullDistanceScale` | float 0.1 … 2 | 1.0 (S24 0.6) | all | no | no |
| `environment.foliage.maxActivationsPerFrame` | int 256 … 65,536 | 8,192 | all | no | no |
| `environment.foliage.instanceCap` | int | from the pending row "Placed foliage instances resident cap" | engine default, platform, game | yes | no |
| `environment.foliage.detailCellsPerFrame` | int 1 … 64 | 8 (S24 4) | all | no | no |
| `environment.foliage.detailNearRadius` | float 0 … 256 | 32 (S24 24; S24 low tier 0) | all | no | no |
| `environment.foliage.detailMaxVertices` | int 0 … 16,777,216 | 1,048,576 (S24 262,144) | all | no | no |
| `environment.terrain.layersPerPixel` | enum 2, 4 | 4 (S24 2) | all | no | no |
| `environment.terrain.previewTilesPerFrame` | int 1 … 64 | 4 | all | no | no |
| `environment.clouds.tier` | enum `layerLow`, `layerHigh`, `volumetric` | `layerHigh` (S24 `layerLow`) | all | no | no |
| `environment.clouds.enabled` | bool | true | all | no | no |
| `environment.water.planarReflections` | bool | false (Stage 5 desktop high: true) | all | no | no |

No ENV key is simulation-affecting: every simulation input of ENV comes from documents and the world seed, so a tier change or thermal adaptation cannot change a simulation result (Z2-D-019; the schema rule of `Zenith2/Docs/Design/PLAT/Configuration.md → "Simulation-affecting keys"`). The module reads every key at Start and again in its configuration change callback (main thread, frame prologue, `Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier changes"`), which republishes `Zenith_FoliageSettings`, the terrain layer count and the cloud tier as immutable values for the frame; nodes and extractors read only the published copy, never the store.

## Authoring and cooking

### What ENV authors

ENV owns the documents and the operations; EDIT owns the tools that drive them (Z2-D-079: terrain is authored visually):

| Authoring surface (EDIT's) | ENV API it calls |
|---|---|
| terrain sculpt tools (raise, lower, flatten, set, smooth, noise, ramp, erode) | `Zenith_TerrainDocument::ApplyStroke` with the brush kinds, `Zenith_TerrainBrushFalloff` for the cursor |
| terrain paint tools (layers, holes, density, exclusion) | `ApplyStroke` with the paint kinds; `AddLayer`, `RemoveLayer`, `MoveLayer` |
| foliage scatter and erase, single placement | `ApplyStroke` (scatter, erase; `Zenith2/Docs/Design/ENV/Terrain.md → "Scatter brush"`); a single placement is a one-dab scatter stroke |
| undo and redo | `ApplyEdit` with the `Zenith_TerrainEdit` EDIT stored |
| live preview | `ConsumeDirtyRegions`, `Zenith_TerrainPreview::QueueRegions`, `Prepare` in the prologue, `WriteRecords` from EDIT's extractor |
| placement preview | `Zenith_Place` over the terrain document |
| regenerate collision and navigation | `Zenith_RegenerateTerrainDerivedData` |
| terrain creation wizard | `Zenith_TerrainDocument::Create` |
| inspectors of foliage, placement, atmosphere, weather, water and cloud-noise documents | SCENE's JSON-tree edits of the document, validated by `Zenith_ReadFoliageTypeDocument` and the other readers ("Zenith_EnvironmentDocuments header") |
| time-of-day and weather scrubbing in the edit world | the five command kinds recorded by the editor into the edit world |

### Editor surfaces

EDIT's terrain panel, foliage panel and environment inspector (pending EDIT, EN12) present these operations; the edit world runs the module like a play world, so what the author sees is the runtime's rendering of the document.

### Cook steps and derived-data keys

ENV's cook follows ASSET's model (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStepDesc"`): its importers read the documents and write intermediates for every product, and one ASSET-phase step per kind writes each cooked file (`Zenith2/Docs/Design/ENV/Bake.md → "Step graph"`). The keys are ASSET's (`Zenith2/Docs/Design/ASSET/DDC.md → "Key inputs"`); ENV decides what enters them:

| Importer or step | Inputs hashed into the key |
|---|---|
| the terrain importer `env.terrain` (the stages `Environment.TerrainValidate`, `.TerrainProducts`, `.TerrainGeometry`, `.TerrainSplat`, `.TerrainMaterial`, `.TerrainTiles`, `.Foliage`, `.Collision`, `.Navigation`) | the terrain document's bytes; every document it reads through `ReadInput` (every tile, layer, foliage type and placement rule set document, and the exclusion sources); the resolved settings; the importer ID and `uENV_TERRAIN_IMPORTER_VERSION` = `uGEOM_TERRAIN_COOK_VERSION × 1,000,000 + uENV_PLACEMENT_ALGORITHM_VERSION × 1,000 + uENV_TERRAIN_STAGES_VERSION`, so a change of GEOM's function, of placement or of any ENV stage re-imports every terrain (a bump of `uENV_NAVIGATION_GEOMETRY_VERSION`, `Zenith2/Docs/Design/ENV/Bake.md → "Navigation geometry extraction"`, bumps `uENV_TERRAIN_STAGES_VERSION` in the same diff); the pinned hash library as a declared tool version |
| `env.terrain`, `env.terrainTile`, `env.foliageCell` | the product's intermediates, the step's `m_uVersion`, the platform, `uZENITH_ENGINE_COOK_VERSION` and the ENV format versions |
| the parameter importers and steps `env.atmosphere`, `env.weather`, `env.water`, `env.cloudNoise` | the document's bytes, the step's `m_uVersion`, the platform, `uZENITH_ENGINE_COOK_VERSION`; for water also the `Material_Water` module version; for cloud noise `uENV_CLOUD_NOISE_ALGORITHM_VERSION` |
| the terrain's textures, generated materials, collision shapes and navigation inputs | their owners' step keys (GEOM's, PHYS's, AI's) over the intermediates ENV's importer wrote |

An edit of one tile re-imports its terrain, but the intermediates of every product the edit does not reach are byte-identical, so their step keys are unchanged and ASSET reuses their outputs from the DDC; only the edited tile's products and the foliage cells within the placement run's largest ring (`Zenith2/Docs/Design/ENV/Procedural.md → "Cross-cell determinism"`) re-cook.

### Migration and re-cook

- Each of the nine document types carries a schema version (1 in the stage that introduces it: Stage 3 for the terrain, tile, layer, foliage type, placement and atmosphere documents, Stage 4 for weather, water and cloud noise) and a migration chain with one step per version, registered tool-side by `Zenith_RegisterEnvironmentDocumentTypes` into SCENE's registry with the type's canonical schema (`Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"`); a fixture per version per type under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Environment/<Type>/`, run by `ZenithAuthoring_Tests` ([Z2-N-CONTRACT-530], [Z2-N-CONTRACT-548]). EDIT's migration UI previews, backs up and journals ([Z2-N-CONTRACT-550]).
- Cooked ENV files are re-cooked on any engine or format version change; the runtime refuses a mismatch ([Z2-N-CONTRACT-529]).
- The environment state's saved properties migrate with saves (SAVE's chain); a change to a saved ENV property adds a save migration step.

## Failure handling

### Failure table

| Failure | Where | Behaviour |
|---|---|---|
| a missing tile document | `Load` | `ENV_ERROR_TILE_MISSING`; the editor offers to remove the tile entry; the cook fails |
| a corrupt tile or document | `Load`, readers | a recoverable error naming the document and field; the editor keeps the last good copy open read-only |
| a newer-version document | `Load`, readers | read-only with a warning ([Z2-N-CONTRACT-537]) |
| a border mismatch | `Load` | error (cook) or repair with a warning (editor) |
| a missing or failed terrain asset at runtime | terrain runtime | no terrain drawn, height queries `ENV_ERROR_OUT_OF_BOUNDS`, `ENV_WARNING_TERRAIN_MISSING` once per terrain (ASSET's fallback policy FAIL for the kind) |
| a failed cloud-noise asset | ENV's GPU side | no cloud pass (fallback NULL_OBJECT); the sky draws without clouds |
| more than 16 submerged views in a frame | environment extractor | the 16 nearest the anchor get their override; the others draw without the underwater medium that frame (`environment.water.submerged_views_dropped`) |
| more than 256 live terrains in a world, or a terrain asset live twice in one renderer | extractor, ENV's GPU side | the terrain is not drawn (`environment.terrain.slots_full`, `Flux.Environment.TerrainMaterialShared`) (`Zenith2/Docs/Design/ENV/Terrain.md → "Terrain record slots"`) |
| a missing tile asset | terrain runtime | that tile answers `ENV_ERROR_NOT_RESIDENT`; rendering still shows GEOM's height tile |
| a failed foliage cell asset | foliage system | the cell stays empty; counted; others unaffected |
| an invalid atmosphere or weather cooked file | loaders | LIGHT's defaults and no weather, with `ENV_WARNING_ATMOSPHERE_FALLBACK` |
| an invalid script or editor command | command playback | dropped, counted, logged; a Luau error at the call site where detectable |
| the GPU-scene instance table full | foliage extractor | render-only foliage shed first, one warning per session ("Instance cap and shedding" in Foliage.md) |
| a frame-time or instance budget breach | foliage | density shedding through the instance cap and the quality tier (render-only instances only); simulation-visible instances never shed |
| memory pressure | memory consumer (`MEMORY_SUBSYSTEM_STREAMING`) | STREAM's evictions release ENV's cell data; ENV sheds nothing that simulation reads |
| allocation failure | constructors, loaders, `Zenith_Place` | `ENV_ERROR_OUT_OF_MEMORY` (or the module error); construction unwinds; `Zenith_Place` returns nothing partial |
| device loss | ENV's GPU side | rebuilt through the full-state extraction after recovery ([Z2-N-REND-015]); ENV's simulation state is untouched |
| cancellation of a cell load | STREAM | ENV releases what it held when the component disappears; nothing leaks (`Test_EnvironmentCells_LoadUnload1000Cycles_NoLeak`) |
| placement over capacity | `Zenith_Place` | `ENV_ERROR_PLACEMENT_CAPACITY` with a truncated canonical result; the cook fails |
| detail foliage over its vertex cap or GEOM's pool at its clamp | foliage extractor, GEOM | the farthest cells are not emitted (`environment.foliage.detail_cells_capped`); an instance GEOM refuses is not drawn (`Flux.Procedural.PoolFull`); nothing resident is evicted |
| more than 256 live water bodies in a frame | ENV's GPU side | the bodies beyond the table are not drawn (`Flux.Environment.WaterBodiesDropped`); queries are unaffected |
| no environment anchor | extractors, `Environment.Advance` | the world origin is the focus, `ENV_WARNING_ANCHOR_MISSING` once per world |
| a render-world record refused (`FLUX_RENDER_WORLD_ERROR_SLOT_FULL`) | ENV's extractors | the record is not emitted; its key stays pending in the bookkeeping and is emitted in the next frame ("Record writer") |

## Test strategy

### Test targets and gate steps

`Zenith2/Docs/Design/ENV/Tests.md → "Test targets and locations"` and `"Gate steps per test group"`.

### Test catalogue index

| Subsystem | Heading in `Zenith2/Docs/Design/ENV/Tests.md` |
|---|---|
| terrain document, brushes, edits | "Unit tests" (subsections "Terrain document tests", "Brush tests", "Splat tests") |
| foliage and wind | "Unit tests" ("Foliage tests", "Wind tests") |
| water and weather | "Unit tests" ("Water tests", "Weather tests") |
| atmosphere and time of day | "Unit tests" ("Atmosphere and time-of-day tests") |
| placement | "Unit tests" ("Procedural placement tests") |
| runtime, cells, module | "Unit tests" ("Cell and runtime tests") |
| determinism under 1, 2, 8 and all workers | "Determinism tests" |
| cook determinism and readers | "Cook tests" |
| GPU rendering and parity | "Render and parity tests" |
| documented approximations | "Error-budget tests" |
| parsers | "Fuzz targets" |
| stress and failure injection | "Stress and sanitizer tests" |
| W-Outdoor and device runs | "Workload and device evidence" |

### Fixtures

The 512 m fixture terrain (4 × 4 tiles, four layers, holes, two foliage types, two placement rule sets that share a rule name), the fixture weather table (Clear, Overcast, Rain, Storm, Snow with every transition kind), the fixture atmosphere and water documents, migration fixtures per document type, the render scenes and the TerrainVT reference scene, under the paths of Tests.md. Every product the importers declare for a fixture source has a fixed GUID in the builder's product table (`Zenith_GetTerrain512FixtureProducts`), which the setup writes as the `.meta` `subAssets`, so the build cook finds a GUID for each and hand-written scenes can name tiles and foliage cells. Only hand-written text is committed: the terrain and tile documents are built at test time, in memory or by the `ENV_TerrainFixtures` CTest setup that also cooks the set, and no render test compares against a committed golden (ruling Q55, Tests.md "Test targets and locations").

### Device and acceptance runs

W-Outdoor on the reference desktop and the S24 at Stage 3 (terrain, placed foliage, placement, atmosphere and time of day) and Stage 4 (with water, weather, clouds and detail foliage), the S24 sustained run at Stage 6, and the OpenWorldSlice day/night sign-off: "Workload and device evidence" in Tests.md.

## Budget rows

### Rows owned

| Row | Name | Status | ENV's part |
|---|---|---|---|
| `Z2-B-ENV-001` | Sky and atmosphere GPU | pending (Stage 1 per-system budget ticket for ENV) | LIGHT's sky LUT and sky draw passes plus ENV's cloud layer pass (and the Stage 5 volumetric clouds, cloud shadows), per path on Windows and the S24, measured with MP-GPUPASS in W-Outdoor, W-OpenWorld and W-Cinematic |

### Rows measured against

| Row | Why |
|---|---|
| `Z2-B-LIGHT-004` Volumetric fog GPU | ENV's fog parameters and the underwater medium drive its cost |
| `Z2-B-GEOM-005` Procedural geometry regeneration GPU | the detail-foliage generator runs inside GEOM's `ProceduralGeometry` pass |
| `Z2-B-GEOM-001` Unified vertex and index pools | detail foliage's slots in GEOM's procedural pool (the sizing inputs of `Zenith2/Docs/Design/ENV/Foliage.md → "Detail pool sizing"`, row EN16) |
| `Z2-B-ADVGFX-008` Physical-accuracy error tolerance per reference scene | the TerrainVT reference scene |
| `Z2-B-RPROD-006` Depth velocity normal prepass GPU, `Z2-B-RPROD-009` Forward opaque GPU, `Z2-B-LIGHT-002` Shadow views total GPU | terrain and foliage draw inside them |
| `Z2-B-RPROD-010` Transparent forward GPU | water surfaces |
| `Z2-B-RPROD-003` Render extraction CPU | ENV's two extractors |
| `Z2-B-RPROD-001` GPU scene tables | foliage and water instances |
| `Z2-B-GEOM-002` Texture residency pool, `Z2-B-GEOM-003` Streaming upload bytes per frame | height, splat, layer and density textures |
| `Z2-B-STREAM-001` Streaming CPU-side resident and in-flight cell data | ENV's tile and foliage cell data, from Stage 4: W-Outdoor at Stage 3 runs without the Streaming module (ruling Q73), so the STREAM rows are first confirmed by the Stage 4 W-Outdoor run (Z2-R-ENV-151) |
| GEOM's pending row "Terrain geometry GPU" (cross-area note GM12) | terrain draws |
| the frame-time, hitch and sustained rows of W-Outdoor (`Zenith2/Docs/Budgets.md → "W-Outdoor"`) | the workload ENV's content defines |

### Pending row needs

Each is recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (row EN14) for the Budgets writer, with producer "Stage 1 per-system budget ticket for ENV" unless stated:

| Proposed row | Unit | Procedure | Workloads | Bound or exclusion |
|---|---|---|---|---|
| Environment update CPU | ms | MP-CPUZONE (the four phase-1 ENV nodes) | WS-FRAME outdoor members: W-Outdoor, W-OpenWorld | CPU-SIM |
| Environment extraction CPU | ms | MP-CPUZONE (the two extractors) | W-Outdoor, W-OpenWorld | inside `Z2-B-RPROD-003`'s span; reported, excluded from bounds |
| Water surface and underwater GPU | ms | MP-GPUPASS | W-Outdoor, W-OpenWorld | GPU per view |
| Detail foliage generation GPU | ms | MP-GPUPASS | W-Outdoor, W-OpenWorld | inside `Z2-B-GEOM-005`; reported |
| Environment resident CPU data | MB | MP-SUBMEM (`MEMORY_SUBSYSTEM_STREAMING` share) | W-Outdoor, W-OpenWorld | within `Z2-B-STREAM-001`, from Stage 4 (ruling Q73) |
| Placed foliage instances resident cap | count | MP-COUNTER | W-Outdoor (100k is its scale), W-OpenWorld | not a memory bound |
| Foliage activation catch-up frames | frames | MP-STREAM | WS-STREAM | not a bound |
| Terrain edit delta memory per undo step | MB | editor measurement | W-Editor | producer: EDIT's W-Editor row tickets |
| Terrain sculpt preview latency | ms | editor measurement | W-Editor | producer: EDIT's W-Editor row tickets |
| Cloud layer approximation error; Volumetric cloud approximation error (Stage 5); Underwater composite error; Ocean FFT to query-set height error (Stage 5); Environment render test tolerances | FLIP mean, metres | the error-budget tests | — | not budgets of time or memory: tolerance rows |

## Stage plan

### Stage table

| Stage | ENV deliverables | Gated by |
|---|---|---|
| S1 | the Stage 1 per-system budget ticket for ENV (`Z2-B-ENV-001` and the pending rows above, reference-engine data, no harness half per `Zenith2/Docs/Budgets.md → "Per-system producer shape"`) | the CORE, JOBS and RPROD rulings ([Z2-N-BUDGET-023]) |
| S3 (fix version Z2 Stage 3; Q45, pending core-doc batch 4 (Q45)) | Z2-I-ENV-01, -03, -06, -09, -10 and what W-Outdoor needs of the others: the module skeleton and API-only headers; the terrain document, tiles, height brushes, the scatter brush, edits, undo and the live preview; the terrain cook, runtime and height queries; the terrain material with its generated per-terrain material; placed foliage, tier thinning and the cap; procedural placement over every rule set; the atmosphere document, time of day, the sun and moon slots and the render-blocks resource; W-Outdoor authored and measured (the Stage 3 WorkloadOutdoor ticket is `blocks`-linked from these) | GEOM's terrain geometry at Stage 3 (Q45, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"`); the GEOM dependency of the terrain runtime, material, preview and cook tickets (Z2-R-ENV-035, -043, -140, -172) is satisfied by GEOM's landed text (Q52, row GM31: `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`, `Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`); they are `blocks`-linked only behind the GEOM tickets that implement those headings, like every other GEOM-built API; X124 (the dependency edges); X125 (the render environment, ruled); X146 (the frame environment constants through which the Stage 3 terrain material reads its splat tiles); X156 (ENV's render-world records); D-SEL-HASH (open, Stage 1) for persisted keys |
| S4 | Z2-I-ENV-02, -04, -05, -07, -08, -11, -12, -13: painting (layers, holes, density, exclusion) and splat blending, foliage wind and the vertex modifier, detail foliage, water (Gerstner, lakes and ocean, underwater), the cloud layer on every tier (the S24 fallback), weather with effects and events, collision generation, navigation generation (after D-SEL-NAV), streaming cell content, every document's migration chain, W-Outdoor with the Stage 4 features, the editor iteration rows | D-SEL-NAV (open, Stage 1/2) for the navigation hook's consumer; EDIT's tools; STREAM's cells; X127 (the vertex-modifier hook, frame slots); X146; the packed detail density and its preview rely on GEOM's landed text (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`), satisfied; the asset kind 34 (ASSET's growth rule, row EN22) for the cloud-noise volumes |
| S5 | volumetric clouds and cloud shadows (desktop high), the FFT ocean tier, rivers, caustics, planar water reflections, weather tiers (puddles, snow accumulation, precipitation occlusion, lightning flashes), thermal erosion, local wind sources and local fog volumes, terrain and foliage in the Deferred and visibility-buffer paths with PathParity, the TerrainVT reference scene of the physical-accuracy suite | D-TBDR (open, Stage 5) does not gate ENV; RPROD's Stage 5 paths |
| S6 | W-Outdoor sustained on the S24 and the flagship evidence; fuzz targets at full duration | shipping qualification |
| S7 | none: ENV has no deferred inventory item | — |

### API-only slices

1. `Zenith_Module_Environment.h`, `Zenith_EnvironmentErrors.h`, `Zenith_EnvironmentTypes.h`, `Zenith_EnvironmentComponents.h`, `Zenith_EnvironmentState.h` and `ZenithFlux/Flux_Environment.h` (so RPROD's engine extractor compiles against `Flux_EnvironmentBlocks`) with contract blocks and stub definitions, so EDIT, WORLD, PHYS, AI and RPROD tickets compile against them.
2. `Zenith_TerrainDocument.h` and `Zenith_TerrainBrush.h` declarations before the document implementation.
3. `Zenith_ProceduralPlacement.h` declarations and the rule reader before the algorithm (the hash port waits for D-SEL-HASH).
4. `Zenith_EnvironmentGeneration.h` before PHYS's and AI's consumers; the navigation half stays API-only until D-SEL-NAV is ruled.

### Ticket slices and ordering

1. Module skeleton and API-only headers (slice 1), after X124.
2. Terrain document and tile codec; height brushes and the scatter brush; edits and undo (Stage 3).
3. Terrain importer and steps (validation, products, GEOM hand-off, splat, generated material, tiles, `env.terrain`, `env.terrainTile`) and the terrain kind loader in Flux (Stage 3).
4. Terrain runtime, height queries, the layered material, the live preview (Stage 3).
5. Placement; foliage cook; foliage system and extractor; tier thinning and the cap (Stage 3).
6. Atmosphere document, time of day, celestial state, the render-blocks resource and the sun and moon slots (Stage 3).
7. The Stage 3 WorkloadOutdoor evidence.
8. Paint brushes and weight normalisation (Stage 4).
9. Wind, the vertex modifier (after X127), foliage wind (Stage 4).
10. Water: waves, queries, the GPU side, underwater (Stage 4).
11. Weather: table, machine, effects, events (Stage 4).
12. Clouds (layer), detail foliage (Stage 4).
13. Collision and navigation hooks, regeneration after edits, cell content, the 1,000-cycle stress (Stage 4).
14. Bindings; diagnostics; the Stage 4 W-Outdoor evidence.
15. Stage 5 slices in the order of the stage table.

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-079 | terrain gets visual authoring (EDIT's tools over ENV's documents); ENV's materials are code with previews |
| Z2-D-019 | every ENV simulation path (clock, weather, wind, water, heights, placement) is worker- and layout-independent |
| Z2-D-024 | ZenithEnvironment is an optional static module |
| Z2-D-020 | ENV's components are registered types usable by data-defined schemas |
| Z2-D-038 | JSON authoring documents, versioned binary cooked data |
| Z2-D-048 | authoring-document migration and cooked re-cook |
| Z2-D-014 | GPU performance rules for ENV's passes and materials |
| Z2-D-063 | water and clouds validated against their own error budgets |
| Z2-D-043 | virtual texturing is required in Stage 4 (ADVGFX); a terrain layer texture may be virtual through GEOM's VT boundary with no ENV permutation, and ENV owns the VT-backed terrain reference scene while no ADVGFX design exists |
| Z2-D-037 | the path-traced reference the TerrainVT scene is compared with |
| Z2-D-050, Z2-D-053 | extraction and tick-boundary publication of asset loads |
| Z2-D-058, Z2-D-059, Z2-D-060 | foliage and water through GPU-scene instances, views, HiZ culling |
| Z2-D-080 | phase-1 ownership of ENV's resources; commands |
| Z2-D-004 | codec chosen from the data for ENV payloads |
| Z2-D-012, Z2-D-021 | W-Outdoor and the flagship slices ENV is measured in |
| Z2-D-035 | Forward+ on the S24 (ENV's S24 tiers target it) |
| Z2-D-045, Z2-D-047, Z2-D-040, Z2-D-046 | errors, contract blocks, ownership, no globals |

### Open decisions

| Decision | Effect on ENV |
|---|---|
| D-SEL-NAV (open, Stage 1/2) | the navigation hook's consumer; the extraction stays API-only with extraction tests until ruled |
| D-SEL-HASH (open, Stage 1) | render keys, placement streams and the GPU hash port use the selected hash |
| D-SEL-JSON (open, Stage 1) | the JSON reader and writer behind every ENV document |
| D-PHASES refinement (open, Stage 1) | confirms phase-1 ownership of ENV's resources (X126 with X84) |
| D-TBDR (open, Stage 5) | none for ENV's passes (no tile-local reads) |
| D-VTX-TANGENTLESS (open, Stage 3) | foliage and water meshes use whatever frame GEOM rules |

### Design rulings owned by ENV

| Ruling | Choice | Alternatives rejected | Reason |
|---|---|---|---|
| E1 terrain consumes GEOM's CDLOD | ENV authors and cooks heights; GEOM renders | ENV-owned terrain rendering | one geometry path, culled like every instance |
| E2 float authoring heights | `float` in documents, `uint16` cooked | 16-bit authoring | no re-quantisation per dab |
| E3 placed foliage is not entities | 32-byte cooked records into GPU-scene instances | one entity per instance | 100k entities for data no system writes |
| E4 placement at cook and in the editor, GPU for detail foliage | cooked instance lists; lattice-hash detail cells | runtime CPU scatter every load | reproducible bytes, no load-time cost |
| E5 phase-1 environment state | clock, weather and wind advance in phase 1; commands apply next tick | a dedicated phase | the phase list is fixed (Z2-D-080) |
| E6 weather effects through prefabs and events | ENV spawns prefabs and emits events | ENV depending on VFX and AUDIO | no module-to-module edges ([Z2-N-ARCH-850]) |
| E7 Gerstner in Stage 4, FFT in Stage 5 | a CPU-queryable wave set for gameplay | FFT only | deterministic buoyancy on every tier |
| E8 cloud layer in Stage 4, volumetric in Stage 5 | an 8-step layer as the S24 fallback | volumetric everywhere | the S24 GPU budget |
| E9 JSON-wrapped compressed tile payloads | one JSON file per tile with base64 COMPRESS sections | binary sidecars | Z2-D-038 keeps authoring in JSON; per-tile files keep merges local |
| E10 ENV's GPU side in ZenithFlux | ENV-owned files in Flux | passes in the module | GEOM's and VFX's precedent; Flux never sees the module |
| E11 environment blocks in a phase-1 world resource | `Environment.Advance` computes `Flux_EnvironmentBlocks`; RPROD's extractor copies them | block setters on the writer (withdrawn by X125); a phase-12 ENV node (extractors are read-only) | one writer of the render environment; blocks are a function of simulation state |
| E12 detail placement at draw time | GEOM's generator writes hash-stable geometry once; the detail material's vertex modifier reads density, splat and height | reading the terrain textures in the generator | GEOM's generator reads only its frame parameters; texture descriptors change on every streaming swap; density edits and tier scales apply without regeneration |
| E13 generated materials | the terrain, detail and water materials are sub-assets ENV's cook generates | per-terrain buffers in material parameters | GEOM's materials hold texture fields only and re-patch them on swaps |
| E14 one placement run over every rule set | stages ordered (set, rule); keys carry the set index | one run per set | cross-set exclusion is defined and keys cannot collide |
| E15 the terrain cook is ENV's importer | the importer validates, calls GEOM's function and writes every product's intermediate; one step per kind writes the files | one ASSET step per ENV stage | ASSET admits one ASSET-phase step per kind and cooks textures and materials from intermediates |
| E16 runtime terrain indices are never cooked | GEOM's record index R and the extractor's slot S are assigned at the terrain ADD | a cooked terrain index | the index of a terrain depends on which terrains are live |
| E17 ENV cooks its cloud-noise volumes | kind 34, `.zcloudnoise`, ENV's generator | a GEOM 3D usage class | GEOM's `.ztex` holds 2D images alone (GM30) |
| E18 ENV's document types register tool-side | `Zenith_RegisterEnvironmentDocumentTypes` in ZenithAuthoring, called by ZenithCook and ZenithEditor | a module entry step with a `ZenithDocument` edge | the orchestrator's ruling; the registry is a tool-process object (ASSET's `.meta` precedent) |
| E19 the ocean skirt keeps its T-junctions | one 8,192 m edge per skirt side | a subdivided skirt ring | the boundary is at least 2,048 m away, flat and exact; a raster check bounds the cracks |

### Classification decisions ENV owns

None of an inventory item: every ENV item is required (five at Stage 3 by Q45, eight at Stage 4); Stage 5 work serves the same items at higher tiers. One ownership decision is recorded here: the VT-backed terrain reference scene of [Z2-N-REND-944] is ENV's until an ADVGFX design document exists (candidates ENV or ADVGFX; criterion: which area's design specifies the terrain material; acceptance: `Test_EnvRender_TerrainVTReference` passes in the Stage 5 physical-accuracy suite), recorded as cross-area row EN19.

## Requirement index

The map from every requirement `Z2-R-ENV-NNN` to the design headings that specify it is the section file `Zenith2/Docs/Design/ENV/RequirementIndex.md` (heading "Requirement to design map"), moved out of this entry in revision 2 so the entry stays under the 2,000-line cap; its rows keep their order and wording. Requirements of Stages 1 and 3 are in `Zenith2/Docs/Requirements/ENV.md`, those of Stages 4 to 6 in `Zenith2/Docs/Requirements/ENV/Stage4Plus.md`.

## Render-world records

ENV's render-side declarations live in ZenithFlux's public header `Flux_Environment.h` (Flux types, because Flux reads them and never includes a module header, [Z2-N-ARCH-076]): the environment blocks the engine environment extractor copies (X125), and ENV's own render-world records, each a plain value immutable after `Render.Seal`, appended by ENV's extractors through the typed writer members of "Record writer" (pending REND, X156). The `[FluxShared]` structs (`Env_…`) are generated from `Zenith2/Shaders/Common/Environment.slang` into `Flux_SharedTypes.generated.h`.

<!-- assumption: pending RPROD, LIGHT, REND (X125, RP25, L31) -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_Environment.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithFlux/Flux_Atmosphere.h"                  // LIGHT's Flux_AtmosphereParams, Flux_FogMediumParams
#include "ZenithFlux/Flux_LightSettings.h"               // LIGHT's Flux_LocalFogVolume, Flux_StarField, Flux_CloudShadowMapping, Flux_ViewMediumOverride
#include "ZenithFlux/Flux_Lights.h"                      // LIGHT's Flux_RenderLightDelta
#include "ZenithShadersGenerated/Flux/Shaders/Flux_SharedTypes.generated.h"   // Env_WindParams, Env_LocalWindSource, Env_WeatherParams

#include "ZenithFlux/Flux_Terrain.h"                    // GEOM's Flux_TerrainDesc
#include "ZenithCore/Zenith_Vector.h"                   // the runtime objects of Bake.md "Runtime asset objects"
#include "ZenithAssets/Zenith_AssetTypes.h"              // Zenith_AssetGUID

class Flux_TerrainResource; class Flux_CloudNoiseResource;   // declared below (Bake.md "Runtime asset objects")
class Flux_MaterialResource; class Flux_MeshResource;       // GEOM's

inline constexpr uint32 uFLUX_ENV_MAX_LOCAL_FOG_VOLUMES = 64;
inline constexpr uint32 uFLUX_ENV_MAX_LOCAL_WIND_SOURCES = 16;
inline constexpr uint32 uFLUX_ENV_MAX_SUBMERGED_VIEWS = 16;       // ENV's per-world cap; LIGHT keeps 16 per frame (Flux_LightingFrame)

/// @contract value Flux_EnvironmentClouds
/// Ownership: Plain value inside Flux_EnvironmentBlocks.
/// Thread affinity: IMMUTABLE once written. Members non-const: the extractor writes it field by field into the frame's
///   shared data.
/// Invalidation: None.
/// Units: Fractions; metres; per-metre noise scale.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_EnvironmentClouds
{
	float m_fCoverage = 0.0f;                      // 0: ENV adds no cloud pass
	float m_fDensity = 0.0f;
	float m_fLayerAltitude = 1800.0f;
	float m_fLayerThickness = 900.0f;
	float m_fNoiseOffsetX = 0.0f;                  // accumulated by Environment.Advance, wrapped to the noise tile period
	float m_fNoiseOffsetZ = 0.0f;
	float m_fNoiseScale = 0.00012f;
	Zenith_AssetHandle<Flux_CloudNoiseResource> m_xBaseNoise;     // 128³ volume; null until active: no cloud pass
	Zenith_AssetHandle<Flux_CloudNoiseResource> m_xDetailNoise;   // 32³ volume; null: base noise only
};

/// @contract value Flux_EnvironmentBlocks
/// Ownership: The object of the world resource "Zenith.Environment.RenderBlocks" (TYPE_KIND_RESOURCE, no serialized
///   property), created by the Environment module at AttachWorld 1 and owned by the module instance; copied once per frame
///   by the engine environment extractor Render.ExtractEnvironment into Flux_RenderEnvironment (LIGHT's atmosphere
///   block, the sun and moon slots, m_xBlocks).
/// Thread affinity: PHASE(Input): written by Environment.Advance alone, after the tick's state step; read in phase 12 by
///   Render.ExtractEnvironment through its declared read, ordered by the phase boundary ([Z2-N-ARCH-472]); no phase-12
///   node writes it ([Z2-N-ARCH-476]).
/// Invalidation: Values change only in phase 1; a frame with no tick keeps the previous tick's values.
/// Units: SI; the members' own units.
/// Errors: None; m_bValid false means "use LIGHT's world-lighting values and these defaults".
/// Version: since Z2 Stage 3 (atmosphere, fog, slots, stars, wind, weather defaults); clouds, wind and weather content since
///   Z2 Stage 4; cloud shadow and local fog volumes since Z2 Stage 5. Never serialized.
/// @endcontract
struct Flux_EnvironmentBlocks
{
	bool m_bValid = false;                         // a settings component exists and its atmosphere asset is active
	bool m_bHasSun = false;
	bool m_bHasMoon = false;
	uint32 m_uTickHz = 60;                        // the world's sim.tickHz, for the frame-time fields ENV's GPU side derives
	uint32 m_uSkyRevision = 0;                    // raised on any bitwise change of atmosphere, fog or stars; 1 at attach
	Flux_AtmosphereParams m_xAtmosphere;           // LIGHT's type
	Flux_FogMediumParams m_xFog;                   // LIGHT's type
	Flux_RenderLightDelta m_xSun;                  // the sun slot (role by position): flags CASTS_SHADOW | CONTACT_SHADOWS | VOLUMETRIC, channels 0x01,
	                                               // volumetric scale 128, every member per Atmosphere.md "Sun and moon illuminance"
	Flux_RenderLightDelta m_xMoon;                 // the moon slot: flags VOLUMETRIC, plus CASTS_SHADOW above 1 lux; channels 0x01, scale 128
	Flux_StarField m_xStars;                       // LIGHT's type
	Flux_CloudShadowMapping m_xCloudShadow;        // LIGHT's type; Stage 5
	Flux_LocalFogVolume m_axLocalFogVolumes[uFLUX_ENV_MAX_LOCAL_FOG_VOLUMES];   // LIGHT's type; Stage 5, nearest the anchor
	uint32 m_uLocalFogVolumeCount = 0;
	Flux_EnvironmentClouds m_xClouds;
	Env_WindParams m_xWind;                        // time fields zero; ENV's GPU side fills them per frame
	Env_LocalWindSource m_axLocalWindSources[uFLUX_ENV_MAX_LOCAL_WIND_SOURCES];   // Stage 5, nearest the anchor, rebased
	uint32 m_uLocalWindSourceCount = 0;
	Env_WeatherParams m_xWeather;
};
```
<!-- /assumption -->

The read contract `Render.ExtractEnvironment` relies on: the resource is present exactly when the world runs the Environment module; every member is written in every phase 1 (so no member is stale by more than one tick); `m_bValid` false leaves every other member at the defaults above; the struct holds no pointer (its two members of type `Zenith_AssetHandle` are reference-counted handles, which the render world may hold), so a copy is a complete snapshot ([Z2-N-ARCH-416]). The per-view underwater medium is not a member ("Blocks ENV writes" in Atmosphere.md): it is the `Flux_RenderEnvironmentView` record below.

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Environment.h (continued): ENV's render-world records
/// @contract enum Flux_EnvironmentRecordOp
/// Ownership: Plain enumeration: the operation of a terrain or water-body record.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value is counted in Flux.Environment.RecordInvalid and the record ignored.
/// Version: since Z2 Stage 3; values are stable within a build (render-world records are never serialized).
/// @endcontract
enum Flux_EnvironmentRecordOp : uint8 { FLUX_ENV_RECORD_OP_ADD = 1, FLUX_ENV_RECORD_OP_UPDATE, FLUX_ENV_RECORD_OP_REMOVE };

/// @contract value Flux_RenderTerrainDelta
/// Ownership: Plain value in the render world's slot arena.
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: Valid for the render world that carries it.
/// Units: Metres.
/// Errors: Applied by Flux_EnvironmentRenderer; an unknown key on UPDATE or REMOVE is counted and ignored.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_RenderTerrainDelta
{
	uint64 m_ulTerrainKey = 0;                    // Zenith_Hash64U64 of the terrain component's persistent ID and the world tag
	Flux_EnvironmentRecordOp m_eOp = FLUX_ENV_RECORD_OP_ADD;
	uint8 m_uTerrainSlot = 0;                     // the extractor slot S, 0 … 255 ("Terrain record slots" in Terrain.md)
	Zenith_AssetHandle<Flux_TerrainResource> m_xTerrain;   // the loaded .zterrain (the kind loader's runtime object)
};

/// @contract value Flux_RenderNodeBoundsRun
/// Ownership: Plain value inside a Flux_RenderTerrainTileUpdate.
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: With its record.
/// Units: Node indices of GEOM's level-major pyramid.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_RenderNodeBoundsRun
{
	uint32 m_uFirstNode = 0;
	uint32 m_uCount = 0;                          // its min/max pairs are the next 2 × m_uCount floats of m_afNodeMinMax
};

/// @contract value Flux_RenderTerrainTileUpdate
/// Ownership: Plain value; the spans borrow the render world's slot arena.
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: Valid for the render world that carries it.
/// Units: Texel bytes; metres for the min/max values.
/// Errors: A chain whose size differs from its layout is refused by Flux_EnvironmentRenderer and counted in
///   Flux.Environment.PreviewRejected; nothing of that record is applied.
/// Version: since Z2 Stage 3. Written by EDIT's extractor in edit worlds ("Live preview" in Terrain.md).
/// @endcontract
struct Flux_RenderTerrainTileUpdate
{
	uint64 m_ulTerrainKey = 0;
	uint32 m_uTileIndex = 0;                      // row-major grid index
	Zenith_Span<const uint8> m_auHeightChain;     // 9 mips of R16 texels, mip 0 first, row-major: 176,824 bytes, or empty
	Zenith_Span<const uint8> m_auHoleChain;       // 9 mips of R8 texels (257² … 2²): 88,412 bytes, or empty
	Zenith_Span<const uint8> m_auSplatIndexChain; // 9 mips of RGBA8 texels (256² … 1²): 349,524 bytes, or empty
	Zenith_Span<const uint8> m_auSplatWeightChain;
	Zenith_Span<const uint8> m_auDetailDensityChain;   // 8 mips of RGBA8 texels (128² … 1²): 87,380 bytes, or empty
	Zenith_Span<const Flux_RenderNodeBoundsRun> m_axNodeRuns;
	Zenith_Span<const float> m_afNodeMinMax;       // passed to Flux_TerrainRenderer::UpdateNodeBounds run by run
};

/// @contract value Flux_EnvironmentWave
/// Ownership: Plain value inside a Flux_RenderWaterBodyDelta.
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: With its record.
/// Units: Metres; radians.
/// Errors: None; validated by ENV's cook and reader.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_EnvironmentWave
{
	float m_fWavelength = 10.0f;
	float m_fAmplitude = 0.0f;
	float m_fDirectionX = 1.0f;
	float m_fDirectionZ = 0.0f;
	float m_fSteepness = 0.0f;
	float m_fPhase = 0.0f;
};

/// @contract value Flux_RenderWaterBodyDelta
/// Ownership: Plain value; the polygon span borrows the slot arena; the asset handles are copies the render side keeps
///   while the body lives.
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: Valid for the render world that carries it.
/// Units: Metres; radians.
/// Errors: As Flux_RenderTerrainDelta.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_RenderWaterBodyDelta
{
	uint64 m_ulBodyKey = 0;
	Flux_EnvironmentRecordOp m_eOp = FLUX_ENV_RECORD_OP_ADD;   // UPDATE when the wind-driven amplitude scale changed
	uint8 m_uKind = 0;                            // Zenith_WaterBodyKind value
	float m_fBaseHeight = 0.0f;
	Zenith_Span<const float> m_afPolygonXZ;        // lake: x, z pairs; ocean: an optional rectangle (4 pairs) or empty
	Flux_EnvironmentWave m_axWaves[8];
	uint32 m_uWaveCount = 0;
	float m_fAmplitudeScale = 1.0f;                // the tick's wind-driven scale
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterial;    // the water document's generated material sub-asset
	Zenith_AssetHandle<Flux_MeshResource> m_xGridMesh;        // WaterGrid
	Zenith_AssetHandle<Flux_MeshResource> m_xSkirtMesh;       // WaterSkirt (ocean only)
};

/// @contract value Flux_RenderEnvironmentView
/// Ownership: Plain value, one per submerged main, editor or texture view request of the world, at most
///   uFLUX_ENV_MAX_SUBMERGED_VIEWS per section ("Submersion test" in WaterWeather.md); a view above water gets none.
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: Valid for the render world that carries it.
/// Units: As Flux_ViewMediumOverride (per-metre coefficients, metres).
/// Errors: None; a submerged view beyond the cap gets no record (environment.water.submerged_views_dropped).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_RenderEnvironmentView
{
	Flux_ViewMediumOverride m_xMedium;             // LIGHT's type, the payload LIGHT copies: m_ulViewID the Flux_MakeViewID(kind,
	                                               // owner, sub-index).m_ulValue of the request's identity, the body's scattering and
	                                               // absorption × underwaterFogDensityScale, the surface height at the camera
	uint64 m_ulWaterBodyKey = 0;
	bool m_bWaterline = false;                     // the camera within 0.25 m of the surface: the composite splits per pixel
};
```

```slang
// Zenith2/Shaders/Common/Environment.slang (continued)
[FluxShared] struct Env_WaterWaveRecord               // 32 bytes
{
	float2 m_xDirection;
	float m_fWaveNumber;
	float m_fAngularFrequency;
	float m_fAmplitude;                                 // × the body's amplitude scale
	float m_fSteepness;
	float m_fPhaseOffset;                               // (φ − ω t) mod 2π for this frame's time, computed in double on the CPU
	float m_fPrevPhaseOffset;                           // the previous frame's, for velocity
};

[FluxShared] struct Env_WaterBodyRecord               // 272 bytes, one per live body of the frame
{
	float m_fBaseHeight;
	float m_fAmplitudeScale;
	uint m_uWaveCount;
	uint m_uKind;
	Env_WaterWaveRecord m_axWaves[8];
};

[FluxShared] struct Env_LocalWindSource               // 32 bytes, Stage 5, from Zenith_WindSourceComponent
{
	float3 m_xPositionWS;                               // rebased
	float m_fRadius;
	float2 m_xDirectionXZ;                              // directional: unit; radial: zero (outward from the position)
	float m_fStrength;                                  // m/s at the centre, × the remaining-duration fade
	float m_fFalloff;                                   // the contribution is m_fStrength × saturate(1 − d / m_fRadius)^m_fFalloff
};

[FluxShared] struct Env_FrameEnvironment              // 128 bytes, one per world per frame, in the upload ring
{
	Env_WindParams m_xWind;                             // with this frame's and the previous frame's wind time
	Env_WeatherParams m_xWeather;
	DescriptorHandle<StructuredBuffer<Env_TerrainGpuRecord>> m_xTerrains;    // indexed by GEOM's terrain record index R
	DescriptorHandle<StructuredBuffer<Env_WaterBodyRecord>> m_xWaterBodies;
	uint m_uTerrainCount;
	uint m_uWaterBodyCount;
	float m_fDetailDensityScale;
	uint m_uPad0;
	DescriptorHandle<StructuredBuffer<uint>> m_xTerrainSlots;               // 256 entries: extractor slot S → R; 0xFFFFFFFF free
	DescriptorHandle<StructuredBuffer<Env_LocalWindSource>> m_xLocalWindSources;   // Stage 5; m_xWind.m_uLocalSourceCount entries
};
```

<!-- assumption: pending RPROD, REND (X127, X146) -->
Every mesh pass exposes the frame's `Env_FrameEnvironment` to vertex modifiers and material `Evaluate` functions (row EN18, core-doc item X146, an extension of X127 (1)); a world without the module gets a zeroed block (no terrains, no water, zero wind).
<!-- /assumption -->

ENV's frame slots (`Zenith2/Docs/Rendering/FrameAnatomy.md` rows 5b, 6c and 11b) are ZenithFlux functions declared in `Flux_Environment.h`, so RPROD's path functions call them without a module edge (ruling Q62 (a)). Each reads ENV's GPU side through `Flux_FrameShared::m_pxEnvironment` (set by `Flux.BuildGraph` step 7 to the renderer `EnableEnvironment` created, null otherwise, [Z2-N-REND-995]) and forwards to the renderer member of the same name; it adds no pass when that pointer is null or the feature is absent from the view or the world:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Environment.h (continued); Flux_RenderGraph.h and Flux_ViewContext.h are RPROD's
void Env_AddCloudLayer(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);        // 6c, Stage 4
void Env_AddUnderwater(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);   // 11b, Stage 4
void Env_AddVolumetricClouds(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared); // 5b, Stage 5
void Env_AddCloudShadowMap(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);   // 5b, Stage 5
void Env_AddOceanFFT(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);         // 5b, Stage 5
```

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Environment/Flux_EnvironmentRenderer.h
/// @contract type Flux_EnvironmentRenderer
/// Ownership: Owned by Flux_Renderer, created by Flux_Renderer::EnableEnvironment (RPROD's declaration, ENV's body) after
///   Flux_TerrainRenderer and Flux_ProceduralGeometrySystem, destroyed before them; owns Flux_DetailFoliageSource, the terrain
///   GPU records and tile tables, the water renderer and ENV's pass state.
/// Thread affinity: RENDER_SUBMIT_THREAD for every member.
/// Invalidation: None.
/// Units: None.
/// Errors: Record application never fails the frame: refused records are counted (Flux.Environment.*) and logged once.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_EnvironmentRenderer
{
public:
	void ApplyRecords(const Flux_RenderWorldSection& xSection, Flux_TerrainRenderer& xTerrain);   // Flux.BuildGraph
	void WriteFrameEnvironment(const Flux_RenderWorld& xWorld, Flux_UploadRing& xRing);
	// the bodies behind the five Env_Add* entry points below, one member each with the entry point's parameters
	void AddCloudLayer(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);
	void AddUnderwater(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
	void AddVolumetricClouds(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);
	void AddCloudShadowMap(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);
	void AddOceanFFT(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);
	uint32 GetWaterNodeCount() const;
};
```

`Flux_FogMediumParams`, `Flux_AtmosphereParams`, `Flux_LocalFogVolume`, `Flux_StarField`, `Flux_CloudShadowMapping` and `Flux_RenderLightDelta` are LIGHT's (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`, `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Flux_Atmosphere header"`); `Flux_TerrainResource` and `Flux_CloudNoiseResource`, the runtime objects of the terrain and cloud-noise kind loaders, are declared in `Flux_Environment.h` after the records, with their accessors (`Zenith2/Docs/Design/ENV/Bake.md → "Runtime asset objects"`); `Flux_MaterialResource` and `Flux_MeshResource` are GEOM's; `Flux_ViewMediumOverride` is LIGHT's (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`).

### Record writer

The four record types travel in four spans of the section, each appended through its own typed writer member; this is core-doc item X156, modelled on GEOM's procedural records (X141).

<!-- assumption: pending REND, RPROD (X156) -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_RenderWorld.h: the additions of X156 to Zenith2/Docs/Rendering.md "The render world hand-off"
struct Flux_RenderWorldSection
{
	// … Rendering's members and GEOM's m_axProceduralInstances (X141), then:
	Zenith_Span<const Flux_RenderTerrainDelta> m_axTerrainDeltas;
	Zenith_Span<const Flux_RenderTerrainTileUpdate> m_axTerrainTileUpdates;
	Zenith_Span<const Flux_RenderWaterBodyDelta> m_axWaterBodyDeltas;
	Zenith_Span<const Flux_RenderEnvironmentView> m_axEnvironmentViews;
};

class Flux_RenderWorldWriter
{
public:
	// … Rendering's members and AddProceduralInstance (X141), then:
	Zenith_Result<void, Flux_RenderWorldError> AddTerrainDelta(const Flux_RenderTerrainDelta& xDelta);
	Zenith_Result<void, Flux_RenderWorldError> AddTerrainTileUpdate(const Flux_RenderTerrainTileUpdate& xUpdate);
	Zenith_Result<void, Flux_RenderWorldError> AddWaterBodyDelta(const Flux_RenderWaterBodyDelta& xDelta);
	Zenith_Result<void, Flux_RenderWorldError> AddEnvironmentView(const Flux_RenderEnvironmentView& xView);
};
```

| Rule | Specification |
|---|---|
| copying | each member copies the record and the elements of every span it holds (the tile chains, node runs and min/max values of a tile update, a water body's polygon) into the partition's slot arena and re-points the copy's spans at them, so no record borrows extractor or editor memory ([Z2-N-ARCH-416]) |
| refusal | a record, or one of its spans, that does not fit the slot arena is refused whole with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` and counted in `Flux.RenderWorld.Dropped` ([Z2-N-REND-008]); ENV's extractors keep a refused key pending in their bookkeeping and emit it in the next frame, and a refused view record costs that view its override for the frame |
| sink | ENV's two extractors append through the ENV-internal interface `Zenith_EnvironmentRecordSink` (`Internal/Extract/Zenith_EnvironmentRecordSink.h`: one pure virtual per record type with the writer member's signature, plus `AddRenderInstanceDelta` and `AddProceduralInstance`), implemented over the frame's `Flux_RenderWorldWriter` in production and by a recording sink that can refuse the Nth record in tests, because the writer is a concrete class with non-virtual members |
| writers | `Environment.ExtractEnvironment` writes terrain deltas, water-body deltas and view records; EDIT's preview extractor writes tile updates ("Live preview" in Terrain.md); `Environment.ExtractFoliage` writes none of the four (it writes instance deltas and GEOM's procedural records) |
| application | `Flux_EnvironmentRenderer::ApplyRecords` applies a section's spans on `Flux.BuildGraph` in the order terrain deltas, tile updates, water-body deltas, view records, each in record order |
| no module | a section of a world without the Environment module has the four spans empty |
<!-- /assumption -->

## Reconciliation items

The cross-area needs this family assumes, each a row of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`; a need only a core document can grant names its cross-doc item:

| Row | To | Need | Cross-doc item |
|---|---|---|---|
| EN1 | RPROD, GEOM | the material vertex-modifier hook (current and previous time) | X127 (1) |
| EN2 | GEOM | (a) and (b) answered by GEOM's terrain cook contract and splat classes (GM28, GM30); (c) texture replace from memory answered by the Q52 patch (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`); open: the adapter's world-metre UVs, the grid diagonal | — |
| EN3 | ASSET | kinds 23 … 27 (landed, AS9); cook error codes; in-memory asset replacement for regeneration | — |
| EN4 | PHYS | the heightfield cook input and diagonal, the static collider component, the instanced static compound, the physical-material table | — |
| EN5 | RPROD, REND | reopened in revision 2: X125 withdrew the block setters only; ENV's four record types travel through the typed writer members of "Record writer" | X156 |
| EN6 | LIGHT | the cloud composite, cloud shadows, star field, underwater override and the sun and moon slots (answered by L31) | X125 |
| EN7 | STREAM, WORLD, RPROD | cell alignment and cell content; the Stage 3 WorkloadOutdoor dependence (now Stage 3 by Q45) | Q45, batch 4 |
| EN8 | GEOM | `Material_Standard` calls `Env_ApplySurfaceWeather` | — |
| EN9 | ARCH | ZenithEnvironment's declared dependencies, ZenithCook's edge, the ZenithFlux ownership row | X124 |
| EN10 | RPROD | per-instance cull distance (adopted, RP21) | X127 (2) |
| EN11 | RPROD | frame slots and capability-matrix rows (confirmed, RP20) | X127 (3) |
| EN12 | EDIT | the terrain command owning a `Zenith_TerrainEdit`, the tools, the preview extractor, the anchor on the editor camera, the editor rows | — |
| EN13 | VFX, AUDIO, GAME | effect prefabs and weather events | — |
| EN14 | BUDGET | the pending rows of "Pending row needs" | X127 (4) |
| EN15 | GEOM | withdrawn in revision 2: the 257 × 257 hole hand-off is GEOM's contract (GM28), and the preview computes the chains itself ("Live preview" in Terrain.md), so no shared kernel is needed | — |
| EN16 | GEOM | the detail source's registration, blocks and pool sizing inputs | — |
| EN17 | RPROD, LIGHT | the `Flux_EnvironmentBlocks` resource written in phase 1 (answers RP25), `EnableEnvironment` (answered: RPROD's Renderer.md declares it and `Flux.BuildGraph` step 7 sets `Flux_FrameShared::m_pxEnvironment`; ENV defines the body), per-view overrides as ENV records | X125; X209 (applied, [Z2-N-REND-995]) |
| EN18 | RPROD, REND | `Env_FrameEnvironment` in every mesh pass; UV0 and the frame environment in the vertex-modifier input | X146 (extends X127 (1)) |
| EN19 | ADVGFX | the VT-backed terrain reference scene and VT sampling of layer textures | — |
| EN20 | CONTENT | the merge policy of terrain documents | — |
| EN21 | GEOM | `AddTerrain` writes `m_uTerrainRecord` and `GetRecordIndex` returns it; texture replace from memory; the packed detail density in the splat-weight class; importer-written texture images and material documents | Q52 (ruled (a)): answered by GEOM's landed post-signoff patch, row GM31 (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`, `Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`), which gates Z2-R-ENV-035, -043, -140 and -172 |
| EN22 | ASSET | kind 34 `Environment.CloudNoise`; the terrain importer's products, keys and intermediates | — |
| EN23 | LIGHT, RPROD | the per-view record's payload is `Flux_ViewMediumOverride` (at most 16 per world), copied into `Flux_LightingFrame`; the sun and moon carry no role flag and the slot flag contract's bits (LIGHT row L39) | X125 |
| EN25 | ASSET | `Zenith2/Docs/Design/ASSET/Migration.md → "Registered document types"` lists ENV's nine types, registered by `Zenith_RegisterEnvironmentDocumentTypes` (`Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"`) in ZenithCook and ZenithEditor | — |
| EN24 | REND, RPROD | the four spans and typed writer members of "Record writer" | X156 |
| EN27 | AI | the step `ai.navMesh` publishes a SOURCE_GEOMETRY item's `environment.navWater` intermediate, unchanged, as the property of the same name, beside `ai.navSource` (`Zenith2/Docs/Design/ENV/Bake.md → "Water bodies for navigation"`); AI's `Zenith_AIBakeInputSource::ListWaterBodies` implementations read it and call `Zenith_ListWaterBodiesForNavigation` (answers AI3 (b)) | — |
