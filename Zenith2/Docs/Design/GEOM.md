# Zenith2 GEOM design: geometry and materials

The GEOM area specifies how geometry and surface appearance get from source content to pixels: the vertex data format and its shared CPU/GPU codec, meshes and their pools, LOD, instancing, meshlets, skinning and morph targets, procedural geometry, materials and their permutations, transparency, decals, the geometry side of terrain, textures, texture streaming, the sampler policy and the advanced surface models. This entry document carries the skeleton sections and the top-level contracts; six section files carry the detail (see "Document family"). The normative sources are `Zenith2/Docs/Rendering.md → "Vertex data format"`, `Zenith2/Docs/Rendering.md → "Materials"`, `Zenith2/Docs/Rendering.md → "GPU scene tables and pools"`, `Zenith2/Docs/Rendering.md → "Shader system and offline compilation"` and their section files; where this family and Rendering disagree, Rendering wins and this family is corrected in the same diff ([Z2-N-REND-001]). Obligations are written as plain prose and bullets; each is traced by a `Z2-R-GEOM-NNN` requirement of `Zenith2/Docs/Requirements/GEOM.md`.

## Purpose and scope

### What GEOM owns

| Capability | Specified in |
|---|---|
| the leaf library `ZenithVertexFormat`: the exact arithmetic kernel, every scalar, QTangent, octahedral and half codec, position and UV quantisation, the layout-table types, stream-set codes, the reachable-set rule, the table-driven packer; the Slang declaration of every reachable stream-set (the generator of `StreamSets.slang`), and the null vertex stream | `Zenith2/Docs/Design/GEOM/VertexFormat.md` |
| the GPU half of the codec (`VertexKernel.slang`, `VertexFormats.slang`, `Half.slang`, `Octahedral.slang`, `MeshVertex.slang`) and its bit-for-bit agreement with the CPU half | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules"` |
| the mesh cook: validation, weld, tangents, precision checks and fallbacks, LOD generation, the meshoptimizer pipeline, splitting at 65,535 vertices, position-only indices, meshlets, the fallback report | `Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh cook pipeline"` |
| the cooked mesh, material and texture formats | "Data structures and formats" |
| `Flux_MeshResource`, `Flux_MaterialResource`, `Flux_TextureResource` and their managers inside ZenithFlux; the unified geometry pools | the section files |
| LOD selection, instancing (shared records, per-instance data), terrain geometry (CDLOD) | `Zenith2/Docs/Design/GEOM/Meshes.md` |
| skinned mesh data, the posed-extent check, the skinned-pose producer interface, the skinned-vertex pool, the skin-source pool's use, `Flux_SkinningBatch`, compute skinning, morph targets | `Zenith2/Docs/Design/GEOM/Skinning.md` |
| procedural geometry: the `procedural:` stream-sets, the procedural-geometry pool and its slot layout, the generator interface `Zenith_ProceduralGeometrySource`, `Flux_ProceduralGeometrySystem`, `Flux_ProceduralFrame`, `Flux_AddProceduralGeometry` and the draw path (GEOM's by the orchestrator's ruling on Q38) | `Zenith2/Docs/Design/GEOM/Procedural.md` |
| the material document schema, module conventions, the material cook, material-slot records, permutation enumeration and the permutation limit check, transparency, decals, advanced surface models, material fallbacks | `Zenith2/Docs/Design/GEOM/Materials.md` |
| texture usage classes, the texture cook (mips, BC7/ASTC through KTX-Software), the sampler policy, texture streaming by reallocate-and-swap, the streaming uploader, fallback textures | `Zenith2/Docs/Design/GEOM/Textures.md` |
| the D-VTX-TANGENTLESS (open, Stage 3) harness | `Zenith2/Docs/Rendering/VertexData.md → "D-VTX-TANGENTLESS harness"` (specified there in full); "Stage plan" here |

### Explicit exclusions

| Not GEOM | Owner | Decision or reason |
|---|---|---|
| importers (glTF through fastgltf, FBX per D-SEL-FBX (open, Stage 4)), import presets, the supported-DCC matrix, coordinate and unit conversion on import | CONTENT | Z2-D-079 keeps materials in code; import is content production |
| the asset database, dependency graph, DDC, cook orchestration, the cook report file itself, asset residency budgets, the asset loader interface | ASSET | GEOM supplies cook steps and loaders ("Authoring and cooking") |
| the render graph, GPU scene tables, culling, PSOs, the shader compiler, the permutation limit's budget row | RPROD (RHI for GPU lifetime) | Z2-D-055, Z2-D-058, Z2-D-025 |
| lighting mathematics inside `EvaluateLighting`, shadows, IBL | LIGHT | GEOM defines the surface-model lobes, LIGHT integrates them |
| terrain sculpting, painting, terrain materials, collision and navigation generation, foliage placement | ENV | Z2-D-079 (visual terrain authoring) |
| skeletons, poses, animation graphs | ANIM | GEOM deforms meshes from poses |
| virtual texturing, virtualized geometry, hardware ray tracing, hair | ADVGFX | Z2-D-043 (VT required Stage 4, virtualized geometry and RT deferred to Stage 7, hair excluded) |
| the material preview view and the material inspector | EDIT (`VIEW_KIND_PREVIEW`, [Z2-N-REND-254]) | GEOM exposes the resources and the reload path |
| a material node graph | — | excluded by Z2-D-079 |
| order-independent transparency | — | design ruling G13 ("Design rulings owned by GEOM") |
| procedural placement of meshes and foliage; reproducible procedural world generation; GPU particles | ENV (Z2-I-ENV-10); WORLD (Z2-I-WORLD-05); VFX | they may implement `Zenith_ProceduralGeometrySource`; GEOM owns only the regeneration pool, pass and draw path (`Zenith2/Docs/Design/GEOM/Procedural.md`) |

### Inventory items covered

| Item | Text | Stage | Specified in |
|---|---|---|---|
| Z2-I-GEOM-01 | Slang-declared streams | S2 | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"`, "Program stream-set specialisation", "Stream-set codes" |
| Z2-I-GEOM-02 | the shared CPU/GPU codec | S2 | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "The exact arithmetic kernel"` onwards |
| Z2-I-GEOM-03 | a position stream | S2 | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position and UV quantisation"` |
| Z2-I-GEOM-04 | QTangent normals | S2 | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation"` |
| Z2-I-GEOM-05 | octahedral normals | S2 | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Octahedral codec realisation"` |
| Z2-I-GEOM-06 | UNORM16 UVs with scale and bias | S2 | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position and UV quantisation"` |
| Z2-I-GEOM-07 | 16-bit indices with mesh splitting | S2 | `Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table"` |
| Z2-I-GEOM-08 | precision-checked fallbacks | S2 | `Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks"`, "Precision fallback report format" |
| Z2-I-GEOM-09 | the D-VTX-TANGENTLESS experiment | S3 | "Stage plan"; `Zenith2/Docs/Rendering/VertexData.md → "D-VTX-TANGENTLESS harness"` |
| Z2-I-GEOM-10 | LOD generation | S4 | `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation"` |
| Z2-I-GEOM-11 | LOD selection | S3 | `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"` |
| Z2-I-GEOM-12 | instancing | S3 | `Zenith2/Docs/Design/GEOM/Meshes.md → "Instancing"` |
| Z2-I-GEOM-13 | meshlets | S5 | `Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets"` |
| Z2-I-GEOM-14 | skinning | S3 | `Zenith2/Docs/Design/GEOM/Skinning.md` |
| Z2-I-GEOM-15 | morph targets | S3 | `Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets"` |
| Z2-I-GEOM-16 | material permutations | S3 | `Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration"`, "Permutation limit check and report" |
| Z2-I-GEOM-17 | transparency | S3 | `Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"` |
| Z2-I-GEOM-18 | decals | S3 | `Zenith2/Docs/Design/GEOM/Materials.md → "Decals"` |
| Z2-I-GEOM-19 | terrain geometry | S3 (the Inventory's S4 moves to S3 by ruling Q45: pending core-doc batch 4 (Q45)) | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry"` |
| Z2-I-GEOM-20 | texture streaming | S4 | `Zenith2/Docs/Design/GEOM/Textures.md` |
| Z2-I-GEOM-21 | sampler policy | S2 | `Zenith2/Docs/Design/GEOM/Textures.md → "Sampler policy"` |
| Z2-I-GEOM-22 | advanced surface models | S5 | `Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"` |

GEOM also delivers the procedural-geometry producer of the RHI item Z2-I-RHI-08 (GPU scene tables and pools): RHI implements the GPU scene and imports the pools, and GEOM owns the procedural-geometry pool's layout, allocator, generator interface and regeneration pass (requirements Z2-R-GEOM-160 to -167). GEOM also delivers the cook-side checks of the RPROD rule Z2-I-RPROD-32 (compressed vertex data, 16-bit indices and a position stream): the lints `Cook.VertexPrecisionFallbacks`, `Cook.NoIndex32` and `Cook.PositionStreamDedicated` of `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"` are GEOM cook steps, and RPROD owns the requirement.

### Document family

| File | Its `##` headings |
|---|---|
| `Zenith2/Docs/Design/GEOM.md` (this file) | the fifteen skeleton headings |
| `Zenith2/Docs/Design/GEOM/VertexFormat.md` | Scope of this file · The exact arithmetic kernel · Scalar quantisation codecs · QTangent codec realisation · Octahedral codec realisation · Half-float codec realisation · Position and UV quantisation · Zenith_VertexKernel header · Zenith_VertexFormat header · Stream-set codes (Reachable stream-set rule, Stream-set declarations in Slang, Declared stream-sets, Canonical stream-set layouts) · Program stream-set specialisation · Position decode invariance · Decode-route consistency · The GPU codec modules · Vertex format tests |
| `Zenith2/Docs/Design/GEOM/Meshes.md` | Scope of this file · Mesh terms · Mesh cook pipeline · Precision fallback report format · The cooked mesh format · Flux_MeshResource header · Geometry pools · Mesh residency state machine · LOD selection · Instancing · Terrain geometry (Terrain cook contract, Terrain record constant) · Mesh tests |
| `Zenith2/Docs/Design/GEOM/Skinning.md` | Scope of this file · Skinned mesh data · Posed-extent check · Flux_Skinning header (Flux_SkinningBatch) · Skinned vertex pool layout · Skinning palette computation · Compute skinning dispatch · Morph targets · Velocity and history reset · Skinning tests |
| `Zenith2/Docs/Design/GEOM/Materials.md` | Scope of this file · Material document schema · Material module conventions · Material reflection input · Material cook · Material document intermediate · The cooked material format (Material reader validation) · Flux_MaterialResource header · Material slots and constants · Material permutation enumeration · Permutation limit check and report · Transparency · Decals · Advanced surface models · Material fallbacks · Material tests |
| `Zenith2/Docs/Design/GEOM/Procedural.md` | Scope of this file · Procedural terms · Procedural stream-sets and pool layout · Flux_ProceduralGeometry header · Registration and sizing · Per-frame regeneration · Flux_AddProceduralGeometry contract · Drawing procedural instances · Procedural failure handling · Procedural tests |
| `Zenith2/Docs/Design/GEOM/Textures.md` | Scope of this file · Texture usage classes · Texture cook pipeline · Texture source image intermediate · The cooked texture format · Sampler policy · Flux_Texture header · Texture residency model · Required-mip computation · Streaming state machine · Reallocate-and-swap realisation · Texture replace from memory · Streaming upload arbitration · Virtual texturing boundary · Fallback textures · Texture tests |

### Conventions

- Names are Zenith Hungarian (Z2-D-034); renderer types `Flux_*`, library types `Zenith_*`, constants `uGEOM_*`/`fVF_*`, enums with a category prefix.
- Units follow `Zenith2/Docs/Design/CORE/Maths.md → "Units"` (metres, radians, bytes); the coordinate system is CORE's (`Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`: right-handed, +Y up, model front +Z, counter-clockwise front faces), and GEOM adds no convention of its own.
- A line `<!-- elaboration -->` marks a detail designed here beyond the plan; `<!-- assumption: pending <AREA> -->` blocks state an interface GEOM needs from a document that does not yet provide it, recorded in the cross-area notes.
- Open decisions are cited by name with their stage: D-VTX-TANGENTLESS (open, Stage 3), D-SEL-HASH (open, Stage 1), D-SLANG (open, Stage 1), D-SEL-FBX (open, Stage 4), D-TBDR (open, Stage 5); every bare mention of one of these names in this family means that open decision.
- Tests are named `Test_<Type>_<Behaviour>` (`Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`); each test table names its file.

## Dependencies

### Libraries GEOM code depends on

| GEOM code | Depends on | Why |
|---|---|---|
| `ZenithVertexFormat` | ZenithCore alone (`Zenith2/Docs/Architecture.md → "Library dependency graph"`) | types, `Zenith_Result`, spans, vectors, `Zenith_Hash64` |
| GEOM runtime code inside `ZenithFlux` | ZenithFlux's declared set: ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI, ZenithVertexFormat, ZenithAssets, ZenithShadersGenerated | pools and uploads (RHI), graph passes (Graph), sub-jobs (Jobs), the asset loader and range reads (Assets), generated params and shared structs |
| GEOM cook steps inside `ZenithCook` | ZenithCook's declared set (ZenithCore, ZenithReflect, ZenithCompression and both backends, ZenithAssets, ZenithScene, ZenithVertexFormat, ZenithJobs, ZenithPlatform, ZenithScriptCompiler) plus `ZenithShadersGenerated` (pending MOD, `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"`), plus its third party fastgltf, meshoptimizer, KTX-Software (and MikkTSpace, "Assumed types from pending design documents") | the codec, section compression, JSON; the generated stream-set layout table the packer reads |
| shader modules | `Zenith2/Shaders/Common/` (RPROD's tree, GEOM's modules) | — |

GEOM adds no library and no dependency edge to the Architecture table.

### Headings this area relies on

| Document heading | What GEOM takes from it |
|---|---|
| `Zenith2/Docs/Rendering/VertexData.md → "Vertex declaration in Slang"`, "Generated layout tables", "Default stream layout", "QTangent codec", "Octahedral codec", "Half-float codec", "Precision checks and fallbacks", "Index buffers and mesh splitting", "Unified stream pools", "Buffer usage unions", "Skinned vertex pool", "Codec tests" | the normative vertex rules |
| `Zenith2/Docs/Rendering.md → "Materials"` | the material model, document keys, classes and buckets, constants and slots, permutation count, preview, tests |
| `Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tables"`, "Persistent resources", "Pools", "Texture streaming swap", "Retirement", "Upload ring", "Responses to memory pressure" | records, residency, lifetime, uploads, shedding |
| `Zenith2/Docs/Rendering/ShaderSystem.md → "Permutations"`, "Shared GPU structs", "Generated parameter headers" | permutation rules, `[FluxShared]`, generated headers |
| `Zenith2/Docs/Rendering/Culling.md → "Indirect draw compaction"`, "Meshlet culling", "Transparent commands" | where LOD selection and transparent collection run |
| `Zenith2/Docs/Rendering/PerPass/Common.md → "ComputeSkinning"`, "MorphTargets", "StreamingUpload", "TransparentForward" | the passes GEOM feeds |
| `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`, "Vector types", "Bounds types", "Zenith_Hash64" | maths conventions and types |
| `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Span"`, "Zenith_Array" | containers |
| `Zenith2/Docs/Design/CORE/Diagnostics.md → "Results and the error taxonomy"`, "Structured logging" | `Zenith_Result` error records, logging |
| `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressedSection.h"`, `Zenith2/Docs/Design/COMPRESS.md → "How cooked formats embed the descriptor"` | codec IDs in cooked headers |
| `Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, "Section table", "Section placement and alignment", "Checks", "Cook report" | the common cooked header and section table every GEOM format uses field for field (cross-area row AS10), the checks C1–C12 the registry applies before a GEOM loader sees a file, and the report's `members` object |
| `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"`, "Zenith_AssetKindLoaderDesc", "Zenith_AssetLoadContext", "Zenith_AssetKindLoader", "Zenith_AssetReader header", "Zenith_CookedFile header", "Parsing and validation functions", "Zenith_CookedFileWriter", "Zenith_AnyAssetHandle" | the loader interface GEOM's three kind loaders implement, the validated `Zenith_CookedFileView`, range reads by handle, section verification, the file writer |
| `Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"`, "Delivery and activation", "Internal queues", "Tool and boot servicing", "Section decode", "Device-loss reload", "Memory pressure responses", "Headless loading" | activation before upload completion and `ReportGpuResident`, ZenithAssets' decoding of loaded sections, recovery loads, `DropCpuCopies` |
| `Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStep"`, "Zenith_CookContext", "Engine-provided steps" | the cook-step interface and the step IDs `geom.mesh`, `geom.material`, `geom.texture`, `geom.permutations` |
| `Zenith2/Docs/Design/ENV/Bake.md → "Section kind registry"`, "Geometry hand-off to GEOM", "Step graph", "Splat textures" | GEOM's `.zterrain` section-kind range 0x0100–0x01FF; the importer `env.terrain`, whose `Environment.TerrainGeometry` stage calls the terrain cook function and whose stages write the texture and material intermediates GEOM's steps cook; the ASSET-phase step `env.terrain` that copies GEOM's sections |
| `Zenith2/Docs/Design/ENV/Terrain.md → "Terrain record slots"`, "Live preview" | ENV's use of the terrain record index and of whole-chain texture replacement |
| `Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`, "Intermediates"; `Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"` | `WriteIntermediate`, the owner of an intermediate's format, and the product GUIDs of importer-written texture and material products |
| `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"` | background decode jobs of streamed mips |
| `Zenith2/Docs/Architecture.md → "Memory governor"` | the texture-swap transition peak and the rendering consumer |
| `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` | material document migration, cooked re-cook |
| `Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`, "Slot allocators", "Pool allocation" | the GPU scene's allocation, reference counting and upload of primitive and material-slot ranges (RP12), and the pool imports |
| `Zenith2/Docs/Design/RPROD/Culling.md → "Bucket registry"`, "Draw keys and command regions" | `Flux_BucketKey` and `Flux_BucketRegistry::Acquire/Release` (keyed by the raster stream-set), and the draw-key unit of compaction |
| `Zenith2/Docs/Design/RPROD/Passes.md → "Prologue feature functions"` | the signatures of `Flux_AddStreamingUploads`, `Flux_AddSkinning` and `Flux_AddProceduralGeometry`, which GEOM implements |
| `Zenith2/Docs/Rendering/GpuResources.md → "Upload ring"` | `Flux_UploadAllocation` in the skinning batch and the procedural frame |

### Upward-callback interfaces

GEOM declares no upward-callback interface. `ZenithVertexFormat` calls nothing; the Flux managers implement ZenithAssets' `Zenith_AssetKindLoader` for the kinds `Flux.Mesh`, `Flux.Material` and `Flux.Texture` ([Z2-N-ARCH-078]; "Asset kind loaders") and the rendering `Zenith_MemoryConsumer` of Flux reports GEOM's resident bytes; ENV, ANIM and the runtime reach GEOM from above through the render world and `Flux_TerrainRenderer`.

### Assumed types from pending design documents

The asset-side interfaces are cited, not assumed: cross-area row AS10 answered GEOM's GM10. The cooked container is ASSET's ("GEOM cooked file header"); the three GEOM kind loaders implement `Zenith_AssetKindLoader` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"`; "Asset kind loaders"); range reads are `Zenith_AssetReader::ReadRange` with a `Zenith_AnyAssetHandle` and a `Zenith_AssetPriority` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetReader header"`); cook steps implement `Zenith_CookStep` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStep"`; "Cook steps and derived-data keys"). The `Flux.Terrain` kind's loader is ENV's (`ZenithFlux/Internal/Environment/Flux_TerrainKindLoader.cpp`, cross-area row EN2 (f)); it reads GEOM's terrain sections through GEOM's section reader (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`).

<!-- assumption: pending ASSET -->
ASSET's design family is in revision. The ASSET headings this family cites are those on disk on 2026-09-26; a heading ASSET moves is re-cited here when it moves, and ASSET's two `pending GEOM` blocks (the loader members and the `ReadRange` signature, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"` and "Zenith_AssetReader header") are answered by this revision (cross-area row GM26).
<!-- /assumption -->

The GPU scene's slot allocation and the bucket registry are cited, not assumed. RP12 answered GM5: the slot allocators are RHI's (`Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`, "Slot allocators"), and the bucket key and registry are RPROD's (`Zenith2/Docs/Design/RPROD/Culling.md → "Bucket registry"`). The call surface through which the GPU scene reports material-slot ranges is RHI's `Flux_GPUSceneRangeListener` (`Zenith2/Docs/Design/RHI/Memory.md → "Material-slot range listener"`, cross-area rows GM23 and RPB23), implemented by RPROD's `Flux_MaterialRangeForwarder`, which calls GEOM's `Flux_MaterialSlotCache` (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`).

<!-- assumption: pending RPROD -->
| Type or rule | Semantics GEOM assumes |
|---|---|
| `Flux_RenderInstanceKey`, `Flux_RenderInstanceDelta` | the render-world instance key and delta; the delta carries a `Flux_MeshID` (null for a procedural instance, with `FLUX_INSTANCE_FLAG_PROCEDURAL`), material overrides and custom data |
| render-world procedural records | `Flux_RenderWorldSection::m_axProceduralInstances` and `Flux_RenderWorldWriter::AddProceduralInstance` (`Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"`, row GM22) |
| shader compiler additions | `[StreamSet(<code>)]` and the `--stream-sets` aggregate command writing `Flux_StreamSets.generated.h`, `--stream-set-list`, per-stream-set vertex tables copied from the declarations with the null stream, material reflection files, the `[TextureUsage]` and `[UVChannel]` attributes, `Flux_MeshletRecord`, `Flux_LODView`, `Flux_TerrainRecord`, `Flux_ProceduralRecord`, `Flux_ProceduralDispatchData` as `[FluxShared]` structs (rows GM3, GM4, GM24) |
| the null vertex stream binding | raster draw recording binds `Flux_NullVertexStream` at vertex binding 7 whenever the PSO's specialised table has a null stream (row GM25) |
<!-- /assumption -->

<!-- assumption: pending SHIP -->
MikkTSpace joins the manifest (owner ZenithCook, tools usage).
<!-- /assumption -->

### Areas that consume GEOM

| Area | Consumes |
|---|---|
| RPROD | stream-set codes, primitive and material-slot records, LOD selection inside culling, meshlets, the permutation count, the three prologue feature functions |
| RHI | the record images of meshes, materials and procedural instances for the GPU scene; the procedural pool buffer for its frame import |
| VFX, ENV, gameplay modules | `Zenith_ProceduralGeometrySource`, to put generated geometry on screen |
| LIGHT | `Flux_SurfaceData` and the surface-model lobes; decal lists |
| CONTENT | the import-preset keys GEOM reads; the `geom.textureSource` blob its image importer writes; the cook report |
| ENV | terrain geometry, the terrain record index (`GetRecordIndex`), whole-chain texture replacement of edited tiles, the splat classes (the detail density included), the `geom.textureSource` and `geom.materialDocument` blobs its importer writes |
| ANIM | the skinned-pose producer interface |
| ADVGFX | the texture cook functions `Zenith_BuildTextureMipChain` and `Zenith_EncodeTextureImage`, `Flux_StreamingUploader::Enqueue` with the `VT_TILE` floor and limit, the material manager's VT handle writes (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook functions"`, "Streaming upload arbitration"; `Zenith2/Docs/Design/GEOM/Materials.md → "Virtual texture bindings"`) |
| EDIT | material reload, `ReplaceParameters`, debug views, the cook report |
| TWOD, DEVT, UI | stream-set families SPRITE, DEBUG, UI and layout validation (the SPRITE codes in `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Owner-family stream-sets"`); for TWOD also the usage class `COLOUR_EXACT` and its `geom.textureSource` writer row |
| WORLD | the published `geom.lod0Summary` and `geom.meshSections`, `Zenith_ReadCookedMeshGeometry` and `Zenith_ComputeMeshGeometryDigest`, the merge recipe writer and the recipe mode of `geom.mesh` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Published mesh properties"`, "Merge recipes", "Cooked mesh geometry in tools") |
| CHAR | the mesh import setting `skin.rigidAttach` (`Zenith2/Docs/Design/GEOM/Skinning.md → "Rigid attachment setting"`) |

## Library and file layout

### Target summary

| Target | Kind | Directory | Public headers | `ZENITH_SUPPORTED` |
|---|---|---|---|---|
| `ZenithVertexFormat` | engine library (`zenith_add_library(ZenithVertexFormat DEPENDS ZenithCore)`) | `Zenith2/Engine/ZenithVertexFormat/` | `Zenith_VertexKernel.h`, `Zenith_VertexFormat.h`, `Zenith_VertexStreamSet.h` | `ZENITH_ALL` |
| `ZenithVertexFormat_Tests` | test executable (the helper creates it) | `Zenith2/Engine/ZenithVertexFormat/Tests/` | — | host presets |
| GEOM code in `ZenithFlux` | part of the engine library | `Zenith2/Engine/ZenithFlux/` | the headers of "ZenithFlux GEOM files" | `ZENITH_GPU` |
| GEOM cook steps in `ZenithCook` | part of the host tool | `Zenith2/Tools/ZenithCook/` | none (tool internals) | `ZENITH_TOOLS_HOST` |

### ZenithVertexFormat files

| Path | Content |
|---|---|
| `Zenith2/Engine/ZenithVertexFormat/CMakeLists.txt` | `zenith_add_library(ZenithVertexFormat DEPENDS ZenithCore)` |
| `Zenith2/Engine/ZenithVertexFormat/Zenith_VertexKernel.h` | "Zenith_VertexKernel header" of `Zenith2/Docs/Design/GEOM/VertexFormat.md` |
| `Zenith2/Engine/ZenithVertexFormat/Zenith_VertexFormat.h` | "Zenith_VertexFormat header" |
| `Zenith2/Engine/ZenithVertexFormat/Zenith_VertexStreamSet.h` | "Stream-set codes" |
| `Zenith2/Engine/ZenithVertexFormat/Internal/Zenith_VertexKernel.cpp` | the kernel |
| `Zenith2/Engine/ZenithVertexFormat/Internal/Zenith_VertexCodec.cpp` | scalar, QTangent, octahedral, half, weight codecs |
| `Zenith2/Engine/ZenithVertexFormat/Internal/Zenith_VertexQuantise.cpp` | boxes and position and UV quantisation |
| `Zenith2/Engine/ZenithVertexFormat/Internal/Zenith_VertexLayout.cpp` | validation, hashing, format names |
| `Zenith2/Engine/ZenithVertexFormat/Internal/Zenith_VertexStreamSet.cpp` | codes, names, the table lookup and the declaration check (no layout data) |
| `Zenith2/Engine/ZenithVertexFormat/Internal/Zenith_VertexPacker.cpp` | `Zenith_PackVertexStreams` |
| `Zenith2/Engine/ZenithVertexFormat/Tests/Zenith_VertexKernel_Tests.cpp`, `Zenith_VertexFormat_Tests.cpp`, `Zenith_VertexLayout_Tests.cpp`, `Zenith_VertexStreamSet_Tests.cpp`, `Zenith_VertexPacker_Tests.cpp` | CPU tests ("Vertex format tests") |
| `Zenith2/Engine/ZenithVertexFormat/Tests/Fixtures/QTangentVectors.json` | the committed reference frames |
| `Zenith2/Engine/ZenithVertexFormat/Tests/Fuzz/StreamSetName.cpp` | fuzz target |
| `Zenith2/Engine/ZenithVertexFormat/Tests/Examples/Zenith_VertexFormat_Example.cpp` | the example the contract blocks cite |

The library has no `Internal/Windows|Android|Linux` files: it contains no platform code.

### ZenithFlux GEOM files

| Path (under `Zenith2/Engine/ZenithFlux/`) | Content |
|---|---|
| `Flux_GeometryErrors.h` | "Flux_GeometryErrors header" |
| `Flux_MeshResource.h` | `Flux_MeshID`, `Flux_MeshResidency`, `Flux_MeshPartInfo`, `Flux_MeshResource`, `Flux_MeshManager` |
| `Flux_LOD.h` | `Flux_LODView` (the generated shared struct re-exported), `Flux_SelectLODCPU`, `Flux_ComputeProjectedSize` |
| `Flux_Skinning.h` | `Flux_JointMatrix`, `Flux_SkinnedPoseFlag`, `Flux_RenderSkinnedPose`, `Flux_SkinningSystem`, `Flux_ValidateSkinnedPose` |
| `Flux_ProceduralGeometry.h` | `Flux_ProceduralUVFormat`, `Flux_ProceduralGeneratorDesc`, `Flux_ProceduralSizing`, `Zenith_ProceduralGeometrySource`, `Flux_ProceduralInstanceFlag`, `Flux_RenderProceduralInstance`, `Flux_ProceduralGeometrySystem` |
| `Flux_MaterialResource.h` | `Flux_MaterialID`, `Flux_MaterialDomain`, `Flux_MaterialBlend`, `Flux_MaterialResource`, `Flux_MaterialManager` |
| `Flux_Decal.h` | `Flux_RenderDecal` |
| `Flux_TextureResource.h` | `Flux_TextureUsage`, `Flux_TextureID`, `Flux_TextureResidency`, `Flux_TextureResource`, `Flux_TextureChainDesc`, `Flux_TextureManager` |
| `Flux_TextureStreaming.h` | `Flux_UploadClass`, `Flux_TextureStreamingSettings`, `Flux_TextureStreamer`, `Flux_StreamingUploader` |
| `Flux_Terrain.h` | `uGEOM_MAX_TERRAINS`, `uGEOM_TERRAIN_RECORD_INVALID`, `Flux_TerrainID`, `Flux_TerrainDesc`, `Flux_TerrainRenderer` |
| `Internal/Geometry/` | `Flux_MeshReader.cpp/.h`, `Flux_MeshLoader.cpp/.h` (`Flux_MeshKindLoader`), `Flux_MeshManager.cpp`, `Flux_GeometryPools.cpp/.h`, `Flux_TLSFAllocator.cpp/.h`, `Flux_LOD.cpp`, `Flux_MaterialSlotCache.cpp/.h`, `Flux_GeometryPoolGrowPass.cpp` |
| `Internal/Skinning/` | `Flux_SkinningSystem.cpp`, `Flux_SkinningPalette.cpp`, `Flux_SkinnedPool.cpp/.h`, `Flux_MorphTargets.cpp`, `Flux_SkinningBatch.h` |
| `Internal/Procedural/` | `Flux_ProceduralInternal.h`, `Flux_ProceduralGeometrySystem.cpp`, `Flux_ProceduralPool.cpp/.h`, `Flux_ProceduralPass.cpp` (`Flux_AddProceduralGeometry`), `Flux_RibbonSource.cpp/.h` |
| `Internal/Geometry/Flux_NullVertexStream.cpp/.h` | the 32-byte zero vertex buffer of the null stream |
| `Internal/Materials/` | `Flux_MaterialReader.cpp/.h`, `Flux_MaterialLoader.cpp/.h` (`Flux_MaterialKindLoader`), `Flux_MaterialManager.cpp`, `Flux_MaterialConstantsPool.cpp/.h`, `Flux_Decals.cpp` |
| `Internal/Textures/` | `Flux_TextureReader.cpp/.h`, `Flux_TextureLoader.cpp/.h` (`Flux_TextureKindLoader`), `Flux_TextureManager.cpp`, `Flux_TextureStreamer.cpp`, `Flux_RequiredMip.cpp`, `Flux_StreamingUploader.cpp`, `Flux_FallbackTextures.cpp` |
| `Internal/Terrain/` | `Flux_TerrainRenderer.cpp`, `Flux_TerrainSelection.cpp/.h`, `Flux_TerrainSectionReader.cpp/.h` (`Flux_ReadTerrainGeometrySections`, called by ENV's terrain kind loader) |
| `Content/Textures/`, `Content/Meshes/` | engine content sources: fallback textures, the terrain grid mesh, preview meshes (cooked like any content, [Z2-N-ARCH-028]) |
| `Tests/VertexFormat/`, `Tests/Geometry/`, `Tests/Skinning/`, `Tests/Materials/`, `Tests/Textures/`, `Tests/Procedural/`, `Tests/GPUScene/` (the GEOM rows), `Tests/Fuzz/MeshFile.cpp`, `Tests/Fuzz/TextureFile.cpp`, `Tests/Fuzz/MaterialFile.cpp`, `Tests/Fixtures/` | tests and fixtures |

<!-- assumption: pending MOD -->
`Content/` is a non-source directory of ZenithFlux; the library helper's glob ignores non-source directories ([Z2-N-ARCH-161]), and the cook finds engine content through each library's declared content roots, which MOD's build design lists.
<!-- /assumption -->

### ZenithCook GEOM files

ZenithCook is ASSET's tool; GEOM's cook steps live in their own subdirectories of its `Internal/`. Each registered GEOM step (`geom.mesh`, `geom.material`, `geom.texture`, `geom.permutations`) implements `Zenith_CookStep` ("Cook steps and derived-data keys"), and each writes its asset files through ASSET's `Zenith_CookedFileWriter`.

| Path (under `Zenith2/Tools/ZenithCook/`) | Content |
|---|---|
| `Internal/Geometry/Zenith_CookMesh.cpp/.h` | the mesh step driver: validation, weld, stream-set choice (`Zenith_CookChooseStreamSet`, `Zenith_CookResolveStreamSet`), precision checks, packing, writing the `.zmesh` through `Zenith_CookedFileWriter` |
| `Internal/Geometry/Zenith_CookMeshTangents.cpp` | MikkTSpace wrapper |
| `Internal/Geometry/Zenith_CookMeshLOD.cpp` | LOD generation and thresholds |
| `Internal/Geometry/Zenith_CookMeshOptimise.cpp` | the meshoptimizer wrapper and the split |
| `Internal/Geometry/Zenith_CookMeshlets.cpp` | meshlets |
| `Internal/Geometry/Zenith_CookMeshReport.cpp` | the report members |
| `Internal/Geometry/Zenith_CookMeshProperties.cpp` | the published `geom.lod0Summary`, `geom.meshSections` and `geom.sourceMesh` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Published mesh properties"`) |
| `Internal/Geometry/Zenith_CookMeshRecipe.cpp/.h` | the recipe mode of `geom.mesh` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Merge recipes"`) |
| `Internal/Skinning/Zenith_CookSkin.cpp` | skin data, posed extent, morph deltas |
| `Internal/Skinning/Zenith_CookRigidAttach.cpp/.h` | the `skin.rigidAttach` setting (`Zenith2/Docs/Design/GEOM/Skinning.md → "Rigid attachment setting"`) |
| `Internal/Materials/Zenith_CookMaterial.cpp/.h` | the material step |
| `Internal/Materials/Zenith_CookSampler.cpp` | sampler policy resolution |
| `Internal/Materials/Zenith_CookPermutations.cpp` | enumeration, the limit check, `material_variants.json`, `stream_sets.json` |
| `Internal/Textures/Zenith_TextureSource.cpp/.h` | the `geom.textureSource` blob's writer and validating reader (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`), called by every importer that declares a texture product and by the texture step |
| `Internal/Textures/Zenith_CookTexture.cpp/.h` | mips, alpha coverage, KTX-Software encode and transcode, writing the `.ztex` through `Zenith_CookedFileWriter` |
| `Internal/Textures/Zenith_TextureCook.h`, `.cpp` | `Zenith_ReadTextureCookSettings`, `Zenith_BuildTextureMipChain` and `Zenith_EncodeTextureImage`, which the texture step and ADVGFX's VT cook step call (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook functions"`) |
| `Internal/Terrain/Zenith_CookTerrainGeometry.cpp/.h` | `Zenith_CookTerrainGeometry` and its input and output types (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`): quantised height tiles, the node min/max pyramid, the four `.zterrain` sections; called by the `Environment.TerrainGeometry` stage of ENV's importer `env.terrain`. The grid mesh is engine content cooked by `geom.mesh` |
| `Internal/Geometry/Zenith_GeomCookSteps.cpp/.h` | `Zenith_RegisterGeomCookSteps(Zenith_CookStepRegistry&, Zenith_CookServices&)`, GEOM's step registration function (`Zenith2/Docs/Design/ASSET/Cook.md → "Step registration functions"`), called fifth by `Zenith_RegisterAllCookSteps`; registers `geom.mesh`, `geom.texture`, `geom.material` and `geom.permutations` ("Cook steps and derived-data keys") |
| `Internal/Geometry/Zenith_GeomImporters.cpp/.h` | `Zenith_RegisterGeomImporters(Zenith_ImporterRegistry&)`, of the `Zenith_PfnRegisterImporters` type, at GEOM's slot of `Zenith_RegisterAllImporters`; registers `geom-constant-texture` and `geom-grid-mesh` |
| `Internal/Geometry/Zenith_ImportGridMesh.cpp/.h`, `Internal/Textures/Zenith_ImportConstantTexture.cpp/.h` | the two declared-constant importers ("Fixtures", engine content sources) |
| `Internal/Geometry/Zenith_GeomCookCodes.cpp/.h` | `Zenith_GeomCookCode`, `Zenith_ToString`, `Zenith_GeomCookReport` ("Cook diagnostics") |
| `Tests/Geometry/` (including `Zenith_CookStreamSet_Tests.cpp`), `Tests/Skinning/`, `Tests/Materials/`, `Tests/Textures/`, `Tests/Terrain/`, `Tests/Fixtures/Geometry/` | cook tests and fixtures |

The material document type's registration, `Zenith_RegisterMaterialMigrations`, is the one GEOM tool-side function the editor calls (its `Zenith_RegisterEditorDocumentTypes` registers every owner's document types, `Zenith2/Docs/Design/EDIT/Foundation.md`), so under ruling Q84 (a) it moves from `Internal/Materials/Zenith_MaterialDocument.cpp` of ZenithCook to the tool library ZenithAuthoring: `Zenith2/Tools/Authoring/Internal/Geom/Zenith_MaterialDocumentTypes.cpp` with the root header `Zenith2/Tools/Authoring/Zenith_AuthoringGeom.h` (`Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`). The material reader, writer and step stay in ZenithCook: the editor validates a material document through the cook server's `validate` request (cross-area row GB24).

The same header also declares the tool-side mesh functions that WORLD's authoring code and editor call, with bodies in `Zenith2/Tools/Authoring/Internal/Geom/` and tests in `Zenith2/Tools/Authoring/Tests/Geom/`: `Zenith_ReadCookedMeshGeometry`, `Zenith_ComputeMeshGeometryDigest` and `Zenith_ComputeMeshLod0Sphere` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Cooked mesh geometry in tools"`), and the merge recipe types with `Zenith_WriteMeshMergeRecipe` and `Zenith_ReadMeshMergeRecipe` ("Merge recipes"). The mesh step in ZenithCook calls them too, so the editor and the cook share one implementation. The ticket that adds the readers appends ZenithFlux, ZenithAssets and ZenithCompression to ZenithAuthoring's `DEPENDS`, which the readers' includes need. This follows EDIT's dependency growth rule, and EDIT's owner table records the edge (A3 row GM32).

### Shader modules and engine content

| Path | Content | Specified in |
|---|---|---|
| `Zenith2/Shaders/Common/VertexKernel.slang`, `VertexFormats.slang`, `Half.slang`, `Octahedral.slang`, `MeshVertex.slang` | the GPU codec | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules"` |
| `Zenith2/Shaders/Common/StreamSetFields.slang`, `StreamSets.slang` | the segment macros and the generator: the one declaration of every reachable MESH, TERRAIN and PROCEDURAL stream-set | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"` |
| `Zenith2/Shaders/Common/Procedural.slang` | `Flux_ProceduralRecord`, `Flux_ProceduralDispatchData`, `Flux_ProceduralFindRecord`, `Flux_ProceduralWriteVertex` | `Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"` |
| `Zenith2/Shaders/Passes/Procedural/Ribbon.slang` | the engine's ribbon generator | `Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"` |
| `Zenith2/Shaders/Common/LOD.slang` | `Flux_SelectLOD`, `Flux_LODView` | `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"` |
| `Zenith2/Shaders/Common/Meshlets.slang` | `Flux_MeshletRecord` | `Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets"` |
| `Zenith2/Shaders/Common/Terrain.slang` | `Flux_TerrainRecord`, `TerrainVertex` | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry"` |
| `Zenith2/Shaders/Common/Skinning.slang` | `Flux_SkinDispatchRecord` | `Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"` |
| `Zenith2/Shaders/Common/Material.slang`, `Sampling.slang` | the material interface and explicit-gradient sampling (Rendering's contract), the decal material interface | `Zenith2/Docs/Design/GEOM/Materials.md` |
| `Zenith2/Shaders/Passes/Skinning/Skinning.slang`, `Morph.slang` | the two passes | `Zenith2/Docs/Design/GEOM/Skinning.md` |
| `Zenith2/Shaders/Materials/*.slang`, `*.material.json` | engine materials | `Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"` |
| `Zenith2/Shaders/StreamSets.list` | the engine blob stream-set list (names only; each must be declared) | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"` |

### ZENITH_SUPPORTED matrix

`ZenithVertexFormat` is CPU-only and declares `ZENITH_ALL`, so it builds on every preset; its tests run on the `ZENITH_HOST` presets `win`, `win-asan` and `linux-tsan` (`Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"`, [Z2-N-ARCH-180], [Z2-N-ARCH-216]), and on-device runs of the GPU half are `needs-human` device steps ("Device and acceptance runs"). GEOM code in ZenithFlux follows ZenithFlux's `ZENITH_GPU`; GEOM cook steps follow ZenithCook's `ZENITH_TOOLS_HOST`.

## Public API

### Header map

| Header | Library | Specified in |
|---|---|---|
| `Zenith_VertexKernel.h` | ZenithVertexFormat | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexKernel header"` |
| `Zenith_VertexFormat.h` | ZenithVertexFormat | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"` |
| `Zenith_VertexStreamSet.h` | ZenithVertexFormat | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set codes"` |
| `Flux_GeometryErrors.h` | ZenithFlux | "Flux_GeometryErrors header" below |
| `Flux_MeshResource.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"` |
| `Flux_LOD.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"` |
| `Flux_Terrain.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry"` |
| `Flux_Skinning.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_Skinning header"` |
| `Flux_ProceduralGeometry.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"` |
| `Flux_MaterialResource.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Materials.md → "Flux_MaterialResource header"` |
| `Flux_Decal.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Materials.md → "Decals"` |
| `Flux_TextureResource.h`, `Flux_TextureStreaming.h` | ZenithFlux | `Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header"` |

`Flux_LOD.h` declares:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_LOD.h
#pragma once
#include "Flux/Shaders/Flux_SharedTypes.generated.h"     // Flux_LODView ([FluxShared] in Common/LOD.slang)
#include "ZenithCore/Zenith_Bounds.h"

/// @contract functions Flux_LODSelection
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Metres; the projected size is a fraction of the view height.
/// Errors: None; a camera inside the sphere gives +infinity (LOD 0).
/// Version: since Z2 Stage 3. Bit-identical to Flux_SelectLOD in Zenith2/Shaders/Common/LOD.slang.
/// @endcontract
float Flux_ComputeProjectedSize(const Zenith_Sphere& xWorldSphere, const Flux_LODView& xView);
uint32 Flux_SelectLODCPU(float fProjectedSize, Zenith_Span<const float> afThresholds, const Flux_LODView& xView);
```

### Rules across the GEOM API

- Every fallible public function returns `Zenith_Result<…, E>`, and every public type has one contract block ([Z2-N-CONTRACT-031]); `ApiContractCheck` and `HeaderSelfContained` cover every header of the map.
- No STL type and no third-party header appears in a GEOM public header; meshoptimizer, KTX-Software and MikkTSpace are included only from `ZenithCook/Internal/` files ([Z2-N-CONTRACT-043]).
- Runtime identities (`Flux_MeshID`, `Flux_MaterialID`, `Flux_TextureID`, `Flux_TerrainID`) are 8-byte slot-and-generation values that never serialize; persistent references are asset GUIDs ([Z2-N-CONTRACT-064]).
- Every file parser returns an error and never asserts on file bytes ([Z2-N-CONTRACT-472]).

### Flux_GeometryErrors header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_GeometryErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Flux_GeometryErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The meanings are the table "Geometry error codes" of Zenith2/Docs/Design/GEOM.md.
/// Version: since Z2 Stage 2. Enumerators keep their meaning once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Flux_GeometryErrorCode : uint8
{
	FLUX_GEOMETRY_ERROR_BAD_HEADER = 1,
	FLUX_GEOMETRY_ERROR_VERSION_MISMATCH,
	FLUX_GEOMETRY_ERROR_CORRUPT_SECTION,
	FLUX_GEOMETRY_ERROR_LAYOUT_MISMATCH,
	FLUX_GEOMETRY_ERROR_FORMAT_UNSUPPORTED,
	FLUX_GEOMETRY_ERROR_LIMIT,
	FLUX_GEOMETRY_ERROR_INVALID_DESC,
	FLUX_GEOMETRY_ERROR_POOL_FULL,
	FLUX_GEOMETRY_ERROR_TABLE_FULL,
	FLUX_GEOMETRY_ERROR_QUEUE_FULL,
	FLUX_GEOMETRY_ERROR_OUT_OF_MEMORY,
	FLUX_GEOMETRY_ERROR_UPLOAD_FAILED,
	FLUX_GEOMETRY_ERROR_DEVICE_LOST,
	FLUX_GEOMETRY_ERROR_MISSING_DEPENDENCY,
	FLUX_GEOMETRY_ERROR_PEAK_REFUSED,
	FLUX_GEOMETRY_ERROR_CANCELLED,
	FLUX_GEOMETRY_ERROR_STALE_ID,
	FLUX_GEOMETRY_ERROR_INVALID_POSE,
	FLUX_GEOMETRY_ERROR_CONSTANTS_TOO_LARGE,
	FLUX_GEOMETRY_ERROR_SHUTTING_DOWN,
};
constexpr const char* Zenith_ToString(Flux_GeometryErrorCode eCode);

/// @contract value Flux_GeometryError
/// Ownership: Plain value, the error record of [Z2-N-CONTRACT-480].
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: m_uDetail is a section index, a count or an offset, by code (table "Geometry error codes").
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_GeometryError
{
	Flux_GeometryErrorCode m_eCode;
	uint8 m_uSectionKind = 0;       // the GEOM section kind involved, 0 when none
	uint16 m_uReserved = 0;
	uint32 m_uDetail = 0;
};
static_assert(sizeof(Flux_GeometryError) == 8);

/// @contract enum Flux_VertexError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: FLUX_VERTEX_ERROR_LAYOUT_MISMATCH: a mesh's recorded layout hash differs from the program's table for its
///   stream-set at bind ([Z2-N-REND-872]); the draw is skipped and Flux.Geometry.LayoutMismatches counts it.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Flux_VertexError : uint8
{
	FLUX_VERTEX_ERROR_LAYOUT_MISMATCH = 1,
};
```

### Handle kinds used

| Kind (`Zenith2/Docs/Contracts.md → "Handle kinds"`) | GEOM use |
|---|---|
| shared asset handle | `Zenith_AssetHandle<Flux_MeshResource>`, `<Flux_MaterialResource>`, `<Flux_TextureResource>`: held by components and materials; a texture referenced by a material is held by the material |
| device handle | the pool buffers, textures and constants buffers through `Flux_PersistentReadOnlyBuffer`/`Texture` |
| operation handle | range reads, upload requests (`Flux_UploadOperation`), background decode jobs |
| persistent ID | asset GUIDs in cooked files and material documents |
| frame-scoped graph handles | the pool-growth, skinning, morph and streaming-upload passes |

## Data structures and formats

### Format index

| Format | Kind | Specified in |
|---|---|---|
| `.zmesh` | cooked binary, versioned | `Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"` |
| `.zmat` | cooked binary | `Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"` |
| `.ztex` | cooked binary | `Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"` |
| `.zterrain` geometry sections 0x0101–0x0104 (geometry header, node min/max pyramid, height-tile table, hole-tile table) | cooked binary inside ENV's terrain asset (`ZTRN`), in GEOM's section-kind range 0x0100–0x01FF | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"` |
| `*.material.json` | authoring document | `Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"` |
| cook report members `vertexFallbacks`, `meshes`, `streamSets`, `materials`, `permutations`, `textures` | JSON under the `members` object of ASSET's report (`Zenith2/Docs/Design/ASSET/Formats.md → "Cook report"`) | `Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"`, `Zenith2/Docs/Design/GEOM/Materials.md → "Permutation limit check and report"` |
| `material_variants.json`, `stream_sets.json` | cook intermediates, shader-compiler inputs | `Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration"` |
| stream-set code | 16-bit field | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set code fields"` |
| GPU records `Flux_MeshletRecord`, `Flux_SkinDispatchRecord`, `Flux_DecalRecord`, `Flux_LODView`, `Flux_TerrainRecord`, `Flux_ProceduralRecord`, `Flux_ProceduralDispatchData` | `[FluxShared]` scalar-layout structs | the section files |
| raster, skin-source, skinned, procedural, meshlet, morph and constants pools; the null vertex stream | GPU byte layouts | the section files |
| `Flux_StreamSets.generated.h` | generated table of the 255 reachable stream-sets | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"` |

### GEOM cooked file header

Every GEOM cooked file (`.zmesh`, `.zmat`, `.ztex`) and GEOM's sections of ENV's `.zterrain` use ASSET's cooked container field for field: the 64-byte header of `Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, the 40-byte entries of "Section table" (each embedding COMPRESS's 24-byte descriptor), the placement of "Section placement and alignment", and the checks C1–C12 of "Checks" ([Z2-N-CONTRACT-527], [Z2-N-CONTRACT-528]). Cross-area row AS10 answered GEOM's GM10 in these terms, and GEOM specifies no header or table bytes of its own. The cook writes every GEOM file through `Zenith_CookedFileWriter` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_CookedFileWriter"`) with the descriptor from `Zenith_CookContext::MakeWriterDesc` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"`); the runtime receives the parsed, validated `Zenith_CookedFileView` in `Zenith_AssetLoadInfo::m_pxFile`, and the section bytes decoded by ZenithAssets ("Asset kind loaders"). The container's own tests are ASSET's (`Test_CookedFile_WriterRoundTrip`, `Test_CookedFile_EachCheckRejects`, `Test_CookedFile_VersionMismatchRefused`); GEOM's are the kind-specific reader tests and `Test_GeomFiles_UseAssetContainer`.

What GEOM defines inside the container:

<!-- elaboration -->
| Element | GEOM's definition |
|---|---|
| magics and kinds | `.zmesh` `'ZMSH'` (`ASSET_KIND_MESH`), `.zmat` `'ZMAT'` (`ASSET_KIND_MATERIAL`), `.ztex` `'ZTEX'` (`ASSET_KIND_TEXTURE`), as ASSET's kind table reserves them (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`); `.zterrain` (`'ZTRN'`, `ASSET_KIND_TERRAIN`) is ENV's file |
| format versions | 1 for each of the three, the `m_uFormatVersion` of GEOM's loader descriptors; bumped by any change to a record of the format |
| section kinds | `GEOM_SECTION_*`: 1–12 in `.zmesh` (`Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"`), 20–22 in `.zmat` (`Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"`), 30–32 in `.ztex` (`Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"`), 0x0101–0x0104 in `.zterrain` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`); none in ASSET's reserved range 0xFF00 … 0xFFFF |
| section order | ASSET sorts the table by (kind, index) and places the sections in table order at 16-byte-aligned offsets (`uASSET_SECTION_ALIGNMENT`), so a GEOM file's byte order is its section-kind order: a `.ztex`'s info (30) and tail (31) precede every streamed mip (32), so the initial load reads one contiguous range, and GPU-image sections stored uncompressed can be read into upload staging directly |
| codec | each section's codec is the one its descriptor records, never configuration (Z2-D-004): ZenithAssets decodes the sections a loader requests (`Zenith2/Docs/Design/ASSET/Loading.md → "Section decode"`), and the texture streamer decodes its own range reads after `Zenith_VerifySection` |

### Byte order and serialization rules

- Every integer and float is little-endian, and every record is written and read field by field; no reader or writer copies a C++ object's memory as a record (Z2-D-038).
- Arrays whose byte image is a GPU format by design (vertex streams, indices, meshlet arrays, texture blocks) are written as that image and uploaded unchanged, which `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Span"` admits for GPU uploads.
- Cooked GEOM data is re-cooked on any engine or format version change and never migrated at runtime ([Z2-N-CONTRACT-529]); material documents migrate (`Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"`).

### In-memory sizes that matter

| Datum | Size |
|---|---|
| default mesh vertex (streams 0 + 1) | 20 bytes (8 + 12); normal-only variant 16; FLOAT32 position fallback 24 |
| skinned pool vertex | 16 bytes (FLOAT16 layout) or 24 (FLOAT32) per half |
| primitive, material-slot, instance records | 128, 32, 128 bytes (Rendering's) |
| meshlet record, meshlet vertex, meshlet triangle | 32, 2, 4 bytes |
| morph delta | 24 bytes |
| decal record | 96 bytes |
| skin dispatch record | 32 bytes |
| mesh part record | 84 bytes (`Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"`) |
| procedural record, procedural dispatch data | 64, 16 bytes (`Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"`) |
| procedural pool vertex | 20 bytes (8 in region 0, 12 in region 1) |
| null vertex stream | 32 bytes (`uVERTEX_NULL_STREAM_BYTES`), one buffer per renderer |
| reachable stream-sets | 255 (`uVERTEX_REACHABLE_STREAM_SET_COUNT`) |
| `Flux_LODView` | 32 bytes (`Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"`) |
| `Flux_TerrainRecord` | 80 bytes (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"`) |
| sections per mesh | at most 255 (`uGEOM_MAX_SECTIONS`, the 8-bit section field of the instance record) |
| material constants block | at most 1,024 bytes, 16-byte granules |

## Algorithms and state machines

### Algorithm index

| Algorithm or state machine | Complexity | Failure behaviour | Specified in |
|---|---|---|---|
| the kernel operations and Newton sequences | O(1) | preconditions assert | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "The exact arithmetic kernel"` |
| QTangent encode and decode | O(1) | degenerate tangent → fallback axis; degenerate normal → error | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation"` |
| weight quantisation | O(1) | — | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Scalar quantisation codecs"` |
| reachable-set rule and the declaration generator | O(1) per code; the generator expands 255 structs | a missing or unreachable declaration → shader compile error | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"` |
| stream-set specialisation, the null stream, enumeration | O(codes × programs) | undrawable code → cook error | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"` |
| mesh cook pipeline, precision checks | O(V + T log T) | cook errors and warnings | `Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh cook pipeline"` |
| splitting at 65,535 | O(T) | — | `Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table"` |
| LOD generation and thresholds | meshoptimizer's | stop rule | `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation"` |
| meshlet generation | meshoptimizer's | — | `Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets"` |
| TLSF pool allocation, growth | O(1); growth O(pool) once | pool full → `ReportGpuFailed` (residency FAILED) | `Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"` |
| the three asset kind loaders | O(sections) per load | `Fail` with the reader's code; cancellation; recovery loads | "Asset kind loaders" |
| mesh residency state machine | — | cancellation, upload failure, device loss | `Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"` |
| LOD selection | O(L) per instance | — | `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"` |
| terrain cook function | O(samples) | input rule → cook error | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"` |
| terrain node selection and morphing | O(nodes visited) | extent not a whole number of tiles → cook error; edge clamp | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry"` |
| procedural admission, instance states, regeneration | O(records log records) per frame | rejected record, pool full, device loss | `Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing"`, "Per-frame regeneration" |
| posed-extent check | O(J + V) | fallback | `Zenith2/Docs/Design/GEOM/Skinning.md → "Posed-extent check"` |
| palette, skinning dispatch, morphs | O(joints), O(V) | invalid pose skipped | `Zenith2/Docs/Design/GEOM/Skinning.md` |
| material cook, permutation enumeration, limit check | O(materials + triples) | cook errors | `Zenith2/Docs/Design/GEOM/Materials.md` |
| decal ordering and blending | O(D log D) | invalid decal dropped | `Zenith2/Docs/Design/GEOM/Materials.md → "Decals"` |
| texture mip generation and encoding | O(texels) | cook errors | `Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"` |
| required-mip computation | O(window × sections) | — | `Zenith2/Docs/Design/GEOM/Textures.md → "Required-mip computation"` |
| whole-chain texture replace from memory | O(chain texels) | validation error, peak refusal, supersession, upload failure, device loss | `Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"` |
| terrain record index and constant | O(1) per `AddTerrain` | the step failures of the table; device loss | `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"` |
| streaming state machine, reallocate-and-swap | — | cancel, failure, peak refusal, device loss | `Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine"` |
| upload arbitration | O(requests) | defer, never drop | `Zenith2/Docs/Design/GEOM/Textures.md → "Streaming upload arbitration"` |

### Frame-level flow of GEOM work

On the `Flux.BuildGraph` node of RENDER(k−1), in this order: (1) drain the mesh, material and texture load queues and finalise loads; (2) apply material parameter replacements, texture swaps and whole-chain texture replacements whose copies completed, which also publishes a terrain's record-constant block and moves the terrain from `PENDING_CONSTANTS` to `ACTIVE` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`); (3) apply the render world's procedural records and build the `Flux_ProceduralFrame` (`Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"`); (4) apply render-world deltas that reference meshes, materials and procedural instances (the GPU scene of `Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`, which obtains GEOM's record images); (5) terrain node selection; (6) texture streaming `Update`; (7) the skinning palette, dispatch records and `Flux_SkinningBatch`; (8) declare the passes `GeometryPoolGrow`, `StreamingUpload` (through `Flux_StreamingUploader`), `MorphTargets`, `ComputeSkinning` and `ProceduralGeometry` at their places in the common prologue (`Zenith2/Docs/Rendering/FrameAnatomy.md → "Common prologue"`).

### Asset kind loaders

GEOM implements `Zenith_AssetKindLoader` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader"`) once per GEOM kind. `Flux_Renderer` registers the three with `Zenith_AssetRegistry::RegisterKindLoader` at construction, before registration closes; in a headless registry ASSET's validate-only loader serves the three kinds instead (`Zenith2/Docs/Design/ASSET/Loading.md → "Headless loading"`).

<!-- elaboration -->
| Loader class (file) | Kind | `Zenith_AssetKindLoaderDesc` | First section round |
|---|---|---|---|
| `Flux_MeshKindLoader` (`ZenithFlux/Internal/Geometry/Flux_MeshLoader.h`) | `ASSET_KIND_MESH` (`Flux.Mesh`) | `m_szName` `"Flux.MeshLoader"`, `m_uFormatVersion` 1, `m_uMaxConcurrentLoads` 2,048 | every section of the file |
| `Flux_MaterialKindLoader` (`ZenithFlux/Internal/Materials/Flux_MaterialLoader.h`) | `ASSET_KIND_MATERIAL` (`Flux.Material`) | `"Flux.MaterialLoader"`, 1, 1,024 | sections 20, 21 and 22 |
| `Flux_TextureKindLoader` (`ZenithFlux/Internal/Textures/Flux_TextureLoader.h`) | `ASSET_KIND_TEXTURE` (`Flux.Texture`) | `"Flux.TextureLoader"`, 1, 1,024 | the info (30) and tail (31) sections; streamed mips are range reads (`Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine"`) |

The concurrency caps bound the loads ZenithAssets holds in flight before activation; they do not bound `flux.geometry.loadQueue`, which holds activated objects until the next drain on `Flux.BuildGraph`, so its capacity is derived from the registry's slot count instead ("Lock-order entries").

| Member | GEOM behaviour (identical for the three kinds unless stated) |
|---|---|
| `BeginLoad(xContext)` | a device-recovery load (`GetInfo().m_bDeviceRecovery`) re-creates the GPU side of `m_pExistingObject`; any load runs the kind reader's view rows on `GetInfo().m_pxFile` (a failure is `Fail(code)`) and requests its first round with `RequestSections`, each section with a destination of its decoded size in a staging allocation of the manager's heap, so ZenithAssets decodes in place; returns within 1 ms |
| `OnSectionsLoaded(xContext, axSections)` | runs the kind reader's section rows over the decoded bytes (a failure is `Fail(code)`), then pushes the object onto `flux.geometry.loadQueue` (an infallible push, "Lock-order entries") and calls `Activate(pObject)` in the same call; it never waits for a drain |
| terminal `Activate(pObject)` | from `OnSectionsLoaded`, after the push and before any drain runs, never on `Flux.BuildGraph`: the object is complete on the CPU side and its uploads are requested, the activation order of `Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"` step 3 and "GPU residency sub-state" (cross-area row AS29). The asset becomes ACTIVE with GPU residency PENDING, or with the early outcome a report recorded first; a recovery load activates with `m_pExistingObject`. So a GEOM asset reaches ACTIVE during the boot, before the first frame renders |
| the drain on `Flux.BuildGraph` | pops each queued object, allocates its ranges, block or texture and `RequestUpload`s its uploads; a request refused with `FLUX_UPLOAD_ERROR_QUEUE_FULL` is retried at the next drain; an object marked unloaded allocates nothing, releases its staging and calls `AcknowledgeUnload()` |
| `Zenith_AssetRegistry::ReportGpuResident(GetInfo().m_xResidency)` | when every upload of the activation reports `COMPLETE`, after the staging is released; the object is then `RESIDENT` (a texture: its tail is resident). A report that reaches the registry before the service has applied the `Activate` is kept as the attempt's early outcome |
| `Zenith_AssetRegistry::ReportGpuFailed(GetInfo().m_xResidency, uCode)` | after activation only, the attempt's one report when its GPU work fails: a pool or constants-pool allocation at the drain that finds its pool at the budget clamp (`FLUX_GEOMETRY_ERROR_POOL_FULL`), a texture creation that fails (`FLUX_GEOMETRY_ERROR_OUT_OF_MEMORY`), or an upload that reports `FAILED` (`FLUX_GEOMETRY_ERROR_UPLOAD_FAILED`); the staging and everything allocated so far are released first; the asset stays ACTIVE with residency FAILED, the kind's fallback applies, and only a reload or a device recovery clears it |
| terminal `Fail(uLoaderCode)` | before activation only: a reader row or the staging allocation failed; the loader code is the `Flux_GeometryErrorCode` value; the staging is released first |
| `Cancel(xContext)` | before activation only, so before the push: release the staging, then `AcknowledgeCancel()` |
| `Unload(xContext)` | after activation (last reference, keep-alive eviction, reload retirement, shutdown): an object still in the load queue is marked unloaded and acknowledged by the drain that pops it, so it is never destroyed under the queue; otherwise cancel queued uploads, free ranges, blocks and records with the current frame's last-use values, then `AcknowledgeUnload()` |
| `DropCpuCopies()` | returns 0: GEOM keeps no CPU copy of GPU-resident data once an object is `RESIDENT` or `FAILED` (the staging is released before `ReportGpuResident` and before `ReportGpuFailed`), so a CRITICAL memory-pressure response finds nothing to drop (`Zenith2/Docs/Design/ASSET/Loading.md → "Memory pressure responses"`) |

The per-kind rows are `Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"`, `Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"` (row "load") and `Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"` (row "always resident"). `Test_GeomLoaders_Contract` asserts this table.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| every `ZenithVertexFormat` function and value | `ANY_THREAD` / `IMMUTABLE` |
| `Flux_MeshResource`, `Flux_MaterialResource`, `Flux_TextureResource` (const members) | `ANY_THREAD` |
| `Flux_MeshManager`, `Flux_MaterialManager`, `Flux_TextureManager` | `ANY_THREAD` for resolution; mutation on `Flux.BuildGraph` |
| `Flux_TextureStreamer`, `Flux_StreamingUploader`, `Flux_SkinningSystem`, `Flux_TerrainRenderer`, `Flux_ProceduralGeometrySystem` | `RENDER_SUBMIT_THREAD` (the render subgraph's nodes) |
| `Zenith_ProceduralGeometrySource` implementations | called on the `Flux.BuildGraph` node alone, with render-world bytes |
| render-world value types (`Flux_RenderSkinnedPose`, `Flux_RenderDecal`, `Flux_RenderProceduralInstance`) | `IMMUTABLE` after `Render.Seal` |
| cook steps | one worker process per step invocation; single-threaded inside the step unless the step fans out with its own fixed-order reduction |

### Frame-graph placement

| Work | Where | Declared access |
|---|---|---|
| load finalisation, streaming decisions, palettes, terrain selection | `Flux.BuildGraph` (render subgraph) and its sub-jobs | the render world (read), the Flux managers (write, single node) |
| section decode of loads | ZenithAssets' decode queue (`Zenith2/Docs/Design/ASSET/Loading.md → "Section decode"`), into the loaders' staging | the loaders' staging |
| section check and decode of streamed mips | background jobs (`JOB_PRIORITY_BACKGROUND_*`) submitted by the streamer | the streamer's staging |
| GPU work | the prologue passes above, on the copy queue (uploads) and async compute (skinning, morphs, procedural regeneration) | their graph declarations |
| cooking | ZenithCook worker processes | — |

GEOM work never runs in a simulation phase and reads no simulation state; it cannot affect determinism ([Z2-N-ARCH-812]).

### Lock-order entries

GEOM adds no `Zenith_Mutex`: every mutable GEOM structure is mutated on the single `Flux.BuildGraph` node, and producers on other threads reach it through bounded lock-free queues (`Zenith2/Docs/Design/CORE/Containers.md → "Zenith_BoundedQueue"`).

| Queue | Capacity | Full policy |
|---|---|---|
| `flux.geometry.loadQueue` (mesh, material and texture objects pushed by `OnSectionsLoaded`, popped by the drain on `Flux.BuildGraph`) | `uGEOM_LOAD_QUEUE_CAPACITY`: the smallest power of two at least 2 × `asset.registry.maxAssets` (`Zenith_AssetRegistryDesc::m_uMaxAssets`, read when `Flux_Renderer` creates the queue); 64-byte cells, so 128 bytes per registry slot | `QUEUE_FULL_POLICY_FATAL`, never reached. The push precedes `Activate` in `OnSectionsLoaded` and has no failure edge (`Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"`: every fallible step of the hand-off comes before `Activate`), so REJECT, which needs one, is not available. The bound: under the activation order of `Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"` a load record returns to ASSET's pool when the service applies its `Activate`, while its object stays queued until a drain, and no drain runs before startup step 19 (`Zenith2/Docs/Design/ASSET/Loading.md → "Tool and boot servicing"`), so `m_uMaxConcurrentLoads` bounds nothing here. Instead (1) an entry is an object, and an object is in the queue at most once (its in-queue flag: a device-recovery load of a queued object updates the queued request and adds no entry); (2) every GEOM object is the current object of one registry slot or, during a reload swap, that slot's retiring object, at most two per slot (the bound `asset.ack.queue` rests on, `Zenith2/Docs/Design/ASSET/Loading.md → "Internal queues"`); (3) a queued object is never destroyed and its slot never freed before the drain pops it (an `Unload` of a queued object is acknowledged by the drain). So at most 2 × `m_uMaxAssets` entries exist, and a push that finds the queue full is a defect that terminates with `FATAL_CODE_QUEUE_FULL`. The counters `.full`, `.dropped` and `.high_water` are published as for every queue; `.full` and `.dropped` stay 0 |
| `flux.textures.decodeQueue` (streamed-mip decode completions) | 1,024 | `QUEUE_FULL_POLICY_REJECT`: the streamer submits at most as many decodes as free slots, so the queue cannot overflow |
| `flux.materials.replaceQueue` (`ReplaceParameters`) | 256 | `QUEUE_FULL_POLICY_REJECT`: `FLUX_GEOMETRY_ERROR_QUEUE_FULL` |

### Construction and shutdown

| Step | Action |
|---|---|
| construction | `Flux_Renderer` calls `Flux_GeomSystems::Create`, which builds, in this order: the uploader; the texture manager, `RegisterKindLoader(ASSET_KIND_TEXTURE, …)`, then `Flux_TextureManager::AcquireFallbacks(Zenith_AssetRegistry&)`; the material manager, `RegisterKindLoader(ASSET_KIND_MATERIAL, …)`, then `Flux_MaterialManager::AcquireDefaultMaterials(Zenith_AssetRegistry&)`; the mesh manager and pools (with the null vertex stream), then `RegisterKindLoader(ASSET_KIND_MESH, …)`; then the procedural geometry system (registering the ribbon source), the skinning system, the streamer and the terrain renderer. A kind's pinned fallbacks are acquired after its registration because `GetPinnedAsset` is valid only from the kind's registration (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"`), and every registration precedes `CloseRegistration`. Each step is a fault point (`uFAULT_POINT_GEOM_CREATE_BASE` + step) of a factory whose steps have failure-injection tests (`Test_GeomManagers_Create_FailsAt<Step>_Unwinds`, [Z2-N-CONTRACT-341]); an unwind destroys what it built in reverse and calls `UnregisterKindLoader` for each kind it registered |
| shutdown step 4 | every GEOM operation (reads, decodes, uploads) is cancelled and completes |
| shutdown step 5 | every deferred deletion retires |
| destruction | in reverse order of construction, inside `Flux_Renderer`'s destruction before the RHI device (step 9); a resource still referenced is reported with its asset GUID |

### Cancellation

| Operation | Cancellation points | Who frees partial results |
|---|---|---|
| mesh load | before activation, `Cancel` (the header read, section read and decode points are ZenithAssets', `Zenith2/Docs/Design/ASSET/Loading.md → "Cancellation points by stage"`); after activation, `Unload` of a mesh whose uploads are queued or staged | the mesh loader, through deferred deletion, before `AcknowledgeCancel` or `AcknowledgeUnload` |
| texture residency change | range reads, decodes, queued uploads, GPU copies | the streamer, before returning to `STABLE` |
| material load and parameter replacement | `Cancel` before activation, `Unload` after; queued upload | the material loader and manager |
| procedural admission and topology change | queued index upload | the procedural geometry system, before the instance leaves `PENDING` |
| cook step | between pipeline steps (the cook orchestrator's signal) | the worker process exits and its outputs are discarded |

Each has a 1,000-cycle stress test ("Test strategy").

### Error enumerations

GEOM defines `Zenith_VertexFormatError` (ZenithVertexFormat), `Flux_GeometryErrorCode` with the record `Flux_GeometryError`, `Flux_VertexError`, and the cook diagnostic codes below; every enumeration is numbered from 1 and has `Zenith_ToString` ([Z2-N-CONTRACT-465], [Z2-N-CONTRACT-466]).

### Vertex format errors

| Enumerator | Returned when |
|---|---|
| `VERTEX_FORMAT_ERROR_UNKNOWN_FORMAT` | a format string or value outside the vocabulary |
| `VERTEX_FORMAT_ERROR_UNKNOWN_SEMANTIC` | a semantic outside the vocabulary |
| `VERTEX_FORMAT_ERROR_BAD_LAYOUT` | a layout breaks a rule of "Layout validation and hashing" |
| `VERTEX_FORMAT_ERROR_BAD_STREAM_SET` | a reserved or inconsistent stream-set code |
| `VERTEX_FORMAT_ERROR_NON_FINITE_INPUT` | NaN or infinity in an encoder input |
| `VERTEX_FORMAT_ERROR_DEGENERATE_NORMAL` | a zero normal given to a QTangent or octahedral encoder |
| `VERTEX_FORMAT_ERROR_VALUE_RANGE` | an integer lane that does not fit its format |
| `VERTEX_FORMAT_ERROR_DESTINATION_TOO_SMALL` | a destination span shorter than count × stride |
| `VERTEX_FORMAT_ERROR_COUNT_OVERFLOW` | count × stride overflows |
| `VERTEX_FORMAT_ERROR_BAD_NAME` | a stream-set name that does not parse |
| `VERTEX_FORMAT_ERROR_UNDECLARED_STREAM_SET` | a generated layout table holds no entry for the code |

### Geometry error codes

| Enumerator | Meaning | `m_uDetail` |
|---|---|---|
| `BAD_HEADER` | a cooked-file view whose magic or kind is not the reader's: a defence for views the registry did not produce (tests, fuzz targets), whose files ASSET's checks C1 and C7 refuse first | 0 |
| `VERSION_MISMATCH` | a view's format version is not the reader's: the same defence, behind ASSET's check C5 | the file's version |
| `CORRUPT_SECTION` | a descriptor, range or count fails validation | the section index |
| `LAYOUT_MISMATCH` | a stream-set's layout hash differs | the stream-set code |
| `FORMAT_UNSUPPORTED` | a texture format not allowed for the usage or platform | the format |
| `LIMIT` | a parser or engine limit exceeded | the value |
| `INVALID_DESC` | a runtime descriptor (terrain, streaming settings) invalid | the field index |
| `POOL_FULL` | a geometry, skinned, constants or meshlet pool at its budget clamp | the pool |
| `TABLE_FULL` | a GPU-scene table at its maximum | the table |
| `QUEUE_FULL` | a GEOM bounded queue full | — |
| `OUT_OF_MEMORY` | a CPU allocation failed | bytes |
| `UPLOAD_FAILED` | an upload operation failed | — |
| `DEVICE_LOST` | device loss during a GEOM operation | — |
| `MISSING_DEPENDENCY` | a referenced asset (texture of a material, skeleton of a mesh) unresolvable | — |
| `PEAK_REFUSED` | the governor refused the texture-swap peak | bytes |
| `CANCELLED` | the operation was cancelled | — |
| `STALE_ID` | a runtime ID resolved after retirement | — |
| `INVALID_POSE` | a skinned pose fails validation | the joint count |
| `CONSTANTS_TOO_LARGE` | parameter bytes larger than the type's constants | the size |
| `SHUTTING_DOWN` | a request after shutdown began | — |

### Cook diagnostics

| Code | Kind | Raised by |
|---|---|---|
| `GEOM_COOK_ERROR_MESH_LIMIT`, `GEOM_COOK_ERROR_MESH_NON_FINITE`, `GEOM_COOK_ERROR_MESH_INDEX` | error | mesh input validation |
| `GEOM_COOK_WARNING_MESH_DEGENERATE_REMOVED`, `GEOM_COOK_WARNING_MESH_NORMAL_REPAIRED`, `GEOM_COOK_WARNING_MESH_WINDING_SUSPECT` | warning | mesh input validation |
| `GEOM_COOK_WARNING_UV_PRECISION` | warning | UV precision check ([Z2-N-REND-879]'s table) |
| `GEOM_COOK_ERROR_SKIN_BIND` | error | skin data |
| `COOK_ERROR_SKIN_REST_POSE` | error | skin data: a mesh joint's inverse bind disagrees with the skeleton's `restModelInverse` beyond the rest-pose tolerance (`Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data"`; A3 row AN4 (2)) |
| `GEOM_COOK_ERROR_STREAM_SET_UNDECLARED` | error | stream-set lookup: a table that lacks the chosen code (a defence; every code the choice can produce is declared, and a failed precision check never raises it) |
| `GEOM_COOK_ERROR_STREAM_SET_UNDRAWABLE` | error | permutation step |
| `GEOM_COOK_ERROR_MATERIAL_SCHEMA`, `_UNKNOWN_TYPE`, `_UNKNOWN_NAME`, `_TYPE_MISMATCH`, `_MISSING_TEXTURE`, `_DOMAIN_KEY`, `_TEXTURE_UNRESOLVED`, `_TEXTURE_USAGE`, `_SAMPLER` (each `GEOM_COOK_ERROR_MATERIAL_…`) | error | material cook |
| `GEOM_COOK_ERROR_PERMUTATION_VALUE` | error | material cook |
| `GEOM_COOK_ERROR_TEXTURE_LIMIT`, `GEOM_COOK_ERROR_TEXTURE_NON_FINITE`, `GEOM_COOK_ERROR_TEXTURE_ENCODE` | error | texture cook |
| `GEOM_COOK_ERROR_TEXTURE_SOURCE` | error | texture cook: a missing or malformed `geom.textureSource` blob (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`) |
| `GEOM_COOK_ERROR_TERRAIN_TILE` | error | terrain geometry cook |
| `GEOM_COOK_ERROR_TERRAIN_EXTENT` | error | terrain geometry cook: an extent that is not a whole number of tiles (256 × `terrain.sampleSpacing`), naming the two nearest multiples |
| `GEOM_COOK_ERROR_MESH_RECIPE` | error | the recipe mode of the mesh step: a malformed `geom.meshMergeRecipe`, a source without `geom.sourceMesh`, a missing source LOD, a singular instance matrix (`Zenith2/Docs/Design/GEOM/Meshes.md → "Merge recipes"`) |
| `GEOM_COOK_ERROR_SKIN_RIGID_ATTACH` | error | the `skin.rigidAttach` setting: a skinned source, a malformed member, an unknown joint (`Zenith2/Docs/Design/GEOM/Skinning.md → "Rigid attachment setting"`) |

`Zenith_GeomCookCode` (`Zenith2/Tools/ZenithCook/Internal/Geometry/Zenith_GeomCookCodes.h`) numbers the errors above from 1 in table order, then `GEOM_COOK_ERROR_MESH_INTERMEDIATE` and `GEOM_COOK_ERROR_NOT_IMPLEMENTED`, and the warnings from 1000; `Zenith_ToString` returns each enumerator's own name. ASSET's `Zenith_CookErrorCode` is closed (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook errors"`; ruling ENV-F7, cross-area row EnB26), so GEOM's codes travel inside it and never repeat an ASSET name:

- a step's failure is `COOK_ERROR_STEP_FAILED` whose `m_uDetail` is the `GEOM_COOK_ERROR_<X>` value, with one ERROR diagnostic starting with that name; a GEOM ticket or test that says "fails with `GEOM_COOK_ERROR_<X>`" means exactly this;
- an importer of GEOM's returns `IMPORT_ERROR_SOURCE_INVALID`, so the driver reports `COOK_ERROR_IMPORT_FAILED`, with one ERROR diagnostic naming the key;
- the permutation step's missing permutation and limit are ASSET's own enumerators `COOK_ERROR_PERMUTATION_MISSING` and `COOK_ERROR_PERMUTATION_LIMIT` ([Z2-N-REND-125], [Z2-N-REND-126]), returned directly as `m_eCode` with `m_uDetail` 0 and the program and key in the diagnostic;
- the terrain geometry function returns its own `Zenith_TerrainGeometryCookError` (`m_eCode` `GEOM_COOK_ERROR_TERRAIN_EXTENT` or `_TILE`, `m_uDetail` the axis, tile or rule), which ENV's importer stage carries as `IMPORT_ERROR_SOURCE_INVALID` with one diagnostic (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`).

`Zenith_GeomCookReport` writes each diagnostic as `<CODE>: <asset GUID> <path>: <element>: <detail>`, where `<path>` is the asset record's source path when it has one and its logical path otherwise (a cook test context's record has no source path); the cook report records it; errors fail the cook of that asset and of the game package.

### Luau errors and asserts

GEOM raises no Luau error and exposes no script binding: scripts reach geometry and materials only through components the runtime extracts. Asserts guard programmer errors alone (kernel preconditions, out-of-range LOD indices); file bytes, material documents and render-world values are validated and reported, never asserted.

## Diagnostics and profiling

### Counters

| Counter | Meaning |
|---|---|
| `Flux.Geometry.MeshesResident`, `Flux.Geometry.MeshFailures`, `Flux.Geometry.LayoutMismatches` | mesh state |
| `Flux.Geometry.PoolBytesUsed`, `Flux.Geometry.PoolBytesCapacity`, `Flux.Geometry.PoolGrowths`, `Flux.Geometry.PoolFull` | geometry pools against `Z2-B-GEOM-001` |
| `Flux.Skinning.Instances`, `Flux.Skinning.Vertices`, `Flux.Skinning.InvalidPoses`, `Flux.Skinning.LODSwitches`, `Flux.Skinning.MorphWeightsClamped`, `Flux.Skinning.PoolBytesUsed` | skinning |
| `Flux.Materials.Loaded`, `Flux.Materials.ConstantsBytes`, `Flux.Materials.Swaps`, `Flux.Materials.FallbackSections`, `Flux.Materials.VirtualHandleRewrites` | materials |
| `Flux.Textures.Resident`, `Flux.Textures.ResidentBytes`, `Flux.Textures.PoolTargetBytes`, `Flux.Textures.RequestsInFlight`, `Flux.Textures.SwapsCompleted`, `Flux.Textures.SwapsCancelled`, `Flux.Textures.StreamFailures`, `Flux.Textures.PeakDeferred`, `Flux.Textures.FallbackBindings`, `Flux.Textures.MipBias` | streaming and residency against `Z2-B-GEOM-002` |
| `Flux.Streaming.UploadBytes`, `Flux.Streaming.UploadDeferredBytes` (per upload class) | uploads against `Z2-B-GEOM-003` |
| `Flux.Decals.Count`, `Flux.Decals.Rejected` | decals |
| `Flux.Textures.Replaces`, `Flux.Textures.ReplacesSuperseded`, `Flux.Textures.ReplaceFailures` | whole-chain replacements published, superseded before publication, and failed (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`) |
| `Flux.Terrain.PendingConstants` | terrains whose record-constant block is not yet published |
| `Flux.Terrain.Nodes`, `Flux.Terrain.ResidencyClamped` | terrain nodes selected; level-0 nodes emitted at level 1 for a missing mip 0 |
| `Flux.Procedural.Instances`, `Flux.Procedural.RegeneratedVertices`, `Flux.Procedural.PoolBytesUsed`, `Flux.Procedural.PoolFull`, `Flux.Procedural.Rejected`, `Flux.Procedural.UploadFailures`, `Flux.Procedural.Coalesced`, `Flux.Procedural.PendingUnregenerated`, `Flux.Procedural.UVPrecisionWarnings` | procedural geometry (`Zenith2/Docs/Design/GEOM/Procedural.md`) |
| `flux.geometry.loadQueue.full`, `.dropped`, `.high_water` and the same for the other two queues | queue counters ([Z2-N-CONTRACT-345]) |

All counters are emitted through `Zenith_DispatchCounter` once per render frame and shown by the on-screen profiler's rendering group (DEVT); the memory counters feed the memory panel.

### Log categories

`flux.geometry`, `flux.skinning`, `flux.materials`, `flux.textures`, `flux.terrain`, `flux.procedural` (runtime); `cook.geometry`, `cook.materials`, `cook.textures` (cook). Fallbacks and failures log once per asset per run at `LOG_LEVEL_WARNING` or `LOG_LEVEL_ERROR` with the asset GUID ([Z2-N-CONTRACT-464]).

### Profiler zones

`Flux.Geometry.FinaliseLoads`, `Flux.Procedural.Apply`, `Flux.Textures.Update`, `Flux.Textures.RequiredMip` (per sub-job), `Flux.Skinning.Palette`, `Flux.Terrain.Select`, and `bg/GeomDecode` for background decodes; the GPU passes get timestamps from the graph.

### Debug views

| Console value | View |
|---|---|
| `r.debug.geometry 1` | LOD index per instance, colour-coded |
| `r.debug.geometry 2` | raster stream-set per instance (precision fallbacks highlighted in red) |
| `r.debug.geometry 3` | meshlets, colour per meshlet (visibility-buffer path) |
| `r.debug.geometry 4` | parts, colour per part (split meshes) |
| `r.debug.textureMips 1` | resident mip versus required mip per texel (green equal, red under-resident, blue over-resident) |
| `r.debug.forceLOD <n>` | force LOD n on every instance |
| `r.debug.terrainNodes 1` | terrain node boundaries and levels |

### Frame-dump entries

GEOM adds a `geometry` object to the render graph's frame dump (`Zenith2/Docs/Rendering/RenderGraph.md → "Frame dump"`): pool occupancy per pool, the streaming requests in flight (texture GUID, state, target mip), the skinning dispatch count and vertex count, the terrain node count, and the procedural instance count with this frame's regenerated vertex count.

### Cook report members

`vertexFallbacks`, `meshes`, `streamSets`, `materials`, `permutations` and `textures` (see "Format index"), each contributed through `Zenith_CookContext::AddReportMember` and placed by ASSET under the report's `members` object, sorted by name (`Zenith2/Docs/Design/ASSET/Formats.md → "Cook report"`); each member's own array is sorted deterministically.

### Configuration keys

| Key | Default | Meaning |
|---|---|---|
| `geom.poolBudgetBytes` | generated from `Z2-B-GEOM-001` | the geometry pools' clamp |
| `geom.textureResidencyBytes` | generated from `Z2-B-GEOM-002` | the streamer's pool target |
| `geom.uploadBytesPerFrame` | generated from `Z2-B-GEOM-003` | the uploader's per-frame bytes |
| `geom.streaming.enabled` | true | false keeps every texture at its full chain (tools and tests) |
| `geom.streaming.instancesPerFrame` | 65,536 | required-mip window |
| `geom.streaming.maxRequestsInFlight` | 256 | changes in flight |
| `r.lodBias`, `r.lodScale` | 0, 1.0 (tier-set) | LOD selection |
| `cook.permutationLimitPerGame` | generated from `Z2-B-RPROD-024` | the limit check |
| `geom.terrain.enabled` | true | development builds only, absent from Shipping: false makes `Flux_TerrainRenderer` emit no node and add no terrain instance, for the A/B measurement of `Z2-B-GEOM-007` (procedure: `Zenith2/Docs/Budgets.md` → "Lighting environment and terrain pass rows", [Z2-N-BUDGET-276]; cross-document item X253, applied by core-doc patch f) |

The budget-derived defaults come from the budget-table generator, like the governor's keys (`Zenith2/Docs/Architecture.md → "Memory governor"`).

## Authoring and cooking

### What GEOM authors

Materials are authored in code (a Slang module) plus a `.material.json` document (Z2-D-079); GEOM owns the document schema and its migration chain. GEOM has no visual editor of its own: meshes and textures are authored in DCC tools and imported by CONTENT, terrain is authored in ENV's editor, and the material preview and inspector are EDIT's. Procedural generators are authored in code by the module that owns them, as a `Zenith_ProceduralGeometrySource` and a compute program under `Zenith2/Shaders/Passes/Procedural/`; they have no cooked data.

### Import preset keys GEOM reads

| Key (CONTENT's preset) | Read by |
|---|---|
| `precision.positionRelative`, `precision.positionAbsoluteMetres`, `precision.uvTexels`, `precision.uvReferenceResolution`, `precision.weightMetres`, `precision.posedMetres` | mesh and skin cook |
| `lod.mode`, `lod.ratios`, `lod.maxError`, `lod.pixelError`, `lod.normalWeight`, `lod.uvWeight`, `lod.positionOnlyThreshold` | mesh cook |
| `lightmapped` | stream-set choice |
| `skin.rigidAttach` (object, absent by default) | mesh cook: a rigid prop skinned to one joint (`Zenith2/Docs/Design/GEOM/Skinning.md → "Rigid attachment setting"`; A3 row CH6 (a)) |
| `cull.customDistance` (bool, default false) | mesh cook: sets mesh-info flag bit 6, so every primitive record of the mesh carries `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (`Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"`; cross-area rows RP21, RP36, RPB18) |
| `texture.usage`, `texture.addressHint`, `texture.quality`, `texture.uastcRdoLambda`, `texture.alphaCoverageCutoff`, `texture.virtual` | texture cook |
| `terrain.sampleSpacing`, `terrain.lodDistanceRatio` | terrain geometry cook, as `m_fSampleSpacing` and `m_fLODDistanceRatio` of `Zenith_TerrainGeometryCookInput`, which the `Environment.TerrainGeometry` stage of ENV's importer `env.terrain` fills from its terrain document |

### Editor surfaces

| Surface | Owner | What GEOM provides |
|---|---|---|
| material preview (`VIEW_KIND_PREVIEW`, Forward) | EDIT | the material resource, its reload through the shader watch journal ([Z2-N-REND-254]), `preview` document keys |
| live parameter tuning | EDIT | `Flux_MaterialResource::ReplaceParameters` |
| asset inspectors | EDIT | `Flux_MeshResource` part and LOD info, texture residency, the cook report members |
| debug views | DEVT, EDIT | "Debug views" |
| terrain sculpting and painting | ENV | `Flux_TerrainRenderer::UpdateNodeBounds` and `Flux_TextureManager::ReplaceChainFromMemory` for the edited tiles' height, hole, splat and density textures |

### Cook steps and derived-data keys

Each GEOM step is a `Zenith_CookStep` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStep"`): `GetDesc` returns its `Zenith_CookStepDesc`, `AddKeyInputs` adds the inputs below beyond the driver's own, in the order listed, and `Cook(Zenith_CookContext&)` reads its intermediates through the context, writes the asset file through `MakeWriterDesc` and `WriteAssetFile`, and contributes its report member through `AddReportMember`.

| Step ID (`m_szID`) | `m_ePhase`, `m_eKind` | `m_uVersion` |
|---|---|---|
| `geom.mesh` | `COOK_PHASE_ASSET`, `ASSET_KIND_MESH` | 1 |
| `geom.material` | `COOK_PHASE_ASSET`, `ASSET_KIND_MATERIAL` | 1 |
| `geom.texture` | `COOK_PHASE_ASSET`, `ASSET_KIND_TEXTURE`; its `m_axEngineFallbacks` rows are role 0, the kind's SUBSTITUTE fallback (the `COLOUR` shipping fallback), and one further role per other usage class and per development form of `Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"` | 1 |
| `geom.permutations` | `COOK_PHASE_GLOBAL_POST`, `ASSET_KIND_NONE`; runs before RPROD's `rprod.shaders` | 1 |

The four steps are static instances registered by `Zenith_Result<void, Zenith_CookError> Zenith_RegisterGeomCookSteps(Zenith_CookStepRegistry& xRegistry, Zenith_CookServices& xServices)` (`Zenith2/Tools/ZenithCook/Internal/Geometry/Zenith_GeomCookSteps.h`), the one signature of `Zenith2/Docs/Design/ASSET/Cook.md → "Step registration functions"`; `Zenith_RegisterAllCookSteps` calls it fifth, after SCRIPT's and before RPROD's, so a driver cook runs every GEOM step. GEOM's two importers are registered by `Zenith_RegisterGeomImporters(Zenith_ImporterRegistry&)` at GEOM's slot of `Zenith_RegisterAllImporters` ("ZenithCook GEOM files"). `Test_Cook_GeomStepsRegistered` asserts the four steps' IDs, phases, kinds and versions.

The terrain geometry cook is not a registered step: the `Environment.TerrainGeometry` stage of ENV's importer `env.terrain` calls `Zenith_CookTerrainGeometry` and adds `uGEOM_TERRAIN_COOK_VERSION` to the importer's version, and ENV's ASSET-phase step `env.terrain` copies the four sections into the `.zterrain` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`).

| Step | Inputs hashed into the derived-data key |
|---|---|
| mesh | the intermediate mesh bytes, or in recipe mode the `geom.meshMergeRecipe` bytes with each source's `geom.sourceMesh` and, in flatten mode, each material's `geom.proxyColour` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Merge recipes"`); the preset, the canonical bytes of a `skin.rigidAttach` setting with the skeleton properties it reads; the platform, the texture resolutions of the assigned materials, `uZENITH_ENGINE_COOK_VERSION`, the pinned meshoptimizer and MikkTSpace versions |
| material | the `geom.materialDocument` blob's bytes (`Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`), the type's reflection file hash, each referenced texture's usage and address hint, the platform |
| texture | the `geom.textureSource` blob's bytes, header included (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`), the preset, the platform, the pinned KTX-Software version |
| permutations | every material's variant record and every mesh's raster stream-set, the platform, the limit |

### Multi-process cooking

Cook steps are pure functions of their declared inputs and run in ZenithCook worker processes (out-of-process importers are CONTENT's; crash isolation is the orchestrator's, ASSET). A step writes its outputs only through `Zenith_CookContext::WriteAssetFile` and `WriteFile`, which publish atomically (`Zenith2/Docs/Design/ASSET/Cook.md → "Atomic output publication"`), so a crashed worker leaves no partial file. The permutation step runs after every mesh and material step of the game and platform has finished.

### Migration and re-cook

Material documents migrate through the material chain (preview, backup, journaled write, unknown fields kept, fixtures per version, `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`); cooked files re-cook on any version change. A change to the kernel, a codec, a stream-set layout or a format record is a re-cook of every affected asset, recorded by bumping `uZENITH_ENGINE_COOK_VERSION` or the format version in the same diff.

## Failure handling

### Failure table

| Failure | Behaviour | Evidence |
|---|---|---|
| a precision check fails | the listed fallback, whose code is always declared; the records of the record rule, never silent, never a cook error | `Test_Cook_NoFallbackIsSilent`, `Test_Cook_ChoiceReachesOnlyDeclaredCodes` |
| a terrain extent that is not a whole number of tiles | `GEOM_COOK_ERROR_TERRAIN_EXTENT` at cook; `INVALID_DESC` from `AddTerrain` | `Test_CookTerrain_ExtentRejected` |
| a procedural record or generator that fails validation, or a full procedural pool | the change is dropped, or the instance is not drawn; counted | `Test_Procedural_AdmissionLimits`, `Test_Procedural_PoolFullNotDrawn` |
| a mesh file missing or corrupt | the load reports `FAILED`; instances of it draw nothing; `Flux.Geometry.MeshFailures`; logged once | `Test_Mesh_ReaderRejectsCorruption` |
| a material missing, corrupt or not resident | the default material (shipping) or the development checker | `Test_Material_FallbackForMissing` |
| a texture missing, corrupt or not tail-resident | the usage's fallback texture; a density consumer binds none while `IsFallbackBound()` | `Test_Texture_FallbackPerUsage` |
| a whole-chain replacement that fails validation, finds the peak refused or cannot allocate | the error is returned, nothing is created and the texture keeps its chain; an upload that fails after the call keeps the old chain and is counted | `Test_Texture_ReplaceChainFromMemory` |
| an `AddTerrain` whose material is missing, lacks `m_uTerrainRecord` or already backs a live terrain, or that finds 1,024 live terrains or the constants pool full | the error is returned and no terrain, record or index exists; no node of a terrain draws before its record constant is published | `Test_Terrain_RecordConstantBeforeFirstDraw` |
| a cooked version mismatch | refused by the registry with `ASSET_ERROR_VERSION_MISMATCH` (check C5) before a GEOM loader runs; never interpreted | `Test_Mesh_VersionMismatchRefused`, `Test_Material_VersionMismatchRefused` |
| a pool at its budget clamp | the new object's GPU residency fails (`ReportGpuFailed` with `POOL_FULL`; the asset stays ACTIVE and its kind's fallback applies, cross-area row AS29); nothing resident is evicted | `Test_Mesh_PoolFullFailsLoad` |
| a GPU-scene table full | the instance is not rendered (RPROD's rule, [Z2-N-REND-545]) | RPROD's test |
| the texture-swap peak refused | the change is deferred to a following frame | `Test_TextureStreaming_PeakRefusedDefers` |
| memory pressure warning or critical | the texture shedding mechanisms 1, 5 and 8 of [Z2-N-REND-506] | `Test_TextureStreaming_PressureShedding` |
| allocation failure of CPU memory in a manager | the operation fails with `OUT_OF_MEMORY`; construction steps unwind | the factory failure-injection tests |
| an invalid skinned pose | skinning skipped for that instance and frame; counted | `Test_Skinning_InvalidPoseSkipped` |
| a permutation missing from the compiled set | cook error; at runtime the missing-PSO fallback, never a compile | `Test_Permutations_MissingIsCookError` |
| the permutation count over the limit | cook error | `Test_Permutations_LimitIsCookError` |
| device loss | every resident mesh and material and every texture tail reloads through the recovery path; streaming restarts from tails | `Test_Recovery_DeviceLossRecover` (RHI) with the GEOM assertions of `Test_Mesh_DeviceLossReloads` |
| cancellation at any stage | partial resources released before completion reports | the 1,000-cycle tests |
| device loss with procedural instances | every instance returns to `PENDING`, re-uploads its indices and regenerates from the full-state records | `Test_Procedural_DeviceLossRegenerates` |
| a device whose arithmetic is not correctly rounded | `Test_VertexKernel_DeviceArithmeticConformance` fails on that device, recorded as a device finding | device run records |

## Test strategy

### Test targets and gate steps

| Target | Contents | Gate step |
|---|---|---|
| `ZenithVertexFormat_Tests` | CPU codec, kernel, layouts, stream-sets, packer | `Test` (every host preset), `Sanitizers` (`win-asan`, `linux-tsan`) |
| `ZenithFlux_Tests` | GPU codec cases, geometry, skinning, materials, textures, GPU-scene GEOM rows, LOD, terrain | `Test` on WARP and lavapipe, with validation clean ([Z2-N-REND-382]); `BackendParity` from Stage 3 |
| `ZenithCook_Tests` | cook steps | `Test` (`win` presets) |
| `ZenithShaderCompiler_Tests` | the stream-set and material reflection outputs GEOM needs | `Test` |
| fuzz executables | `Zenith_Fuzz_StreamSetName`, `Zenith_Fuzz_MeshFile`, `Zenith_Fuzz_TextureFile`, `Zenith_Fuzz_MaterialFile` | `win-asan` smoke; full duration in stage acceptance ([Z2-N-ARCH-952]) |
| benchmarks | `Zenith2/Benchmarks/Budgets/GEOM/` (the Stage 1 harness), `Zenith2/Benchmarks/VertexTangentless/` | CTest smoke in `Test` |

### Test catalogue index

| Catalogue | Heading |
|---|---|
| kernel, codecs, stream-sets | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Vertex format tests"` |
| mesh cook, pools, residency, LOD, instancing, terrain | `Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh tests"` |
| skinning and morphs | `Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning tests"` |
| materials, permutations, transparency, decals, surface models | `Zenith2/Docs/Design/GEOM/Materials.md → "Material tests"` |
| textures, sampler policy, streaming | `Zenith2/Docs/Design/GEOM/Textures.md → "Texture tests"` |
| procedural geometry | `Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural tests"` |

Additional tests in this file's scope, each in the named library's `Tests/` directory:

| Test | Location | Asserts |
|---|---|---|
| `Test_VertexFormat_Example_Compiles` | `ZenithVertexFormat/Tests/Examples/` | the example the contract blocks cite compiles and round-trips a QTangent |
| `Test_Cook_TangentsGenerated` | `ZenithCook/Tests/Geometry/Zenith_CookMesh_Tests.cpp` | a normal-mapped fixture without tangents gets MikkTSpace tangents; a skinned fixture without normal maps gets basis tangents with sign +1 |
| `Test_Cook_StreamSetChoice` | same | each row of "Stream-set choice" selects its field value on a fixture; normal-mapped meshes use QTangent |
| `Test_Cook_AuthoredLODChain` | `ZenithCook/Tests/Geometry/Zenith_CookLOD_Tests.cpp` | an `MSFT_lod` fixture yields its authored levels and thresholds |
| `Test_GeomFiles_UseAssetContainer` | `ZenithCook/Tests/Geometry/Zenith_GeomFile_Tests.cpp` | a cooked fixture `.zmesh`, `.zmat` and `.ztex` each parse with `Zenith_ParseCookedHeader` against the expectation of its kind (magic, kind, format version 1, the fixture's GUID and content hash), hold exactly the section kinds and indices their format lists and no other, and a terrain fixture's four GEOM sections are kinds 0x0101–0x0104 at index 0 |
| `Test_GeomLoaders_Contract` | `Zenith2/Engine/ZenithFlux/Tests/Geometry/Flux_GeomLoaders_Tests.cpp` | for each of the three loaders, through ASSET's header-only `Zenith_AssetKindLoaderHarness` (Z2T-ASSET-097) driving a real `Zenith_AssetRegistry` over seed files built in code (no double of a concrete engine class); `Fail` is observed as `LoadAndWait`'s error and `Observe` reading FAILED with `ASSET_ERROR_LOADER_FAILED`, the residency reports as `Observe`'s residency: the descriptor's name, format version 1 and concurrency cap; the first round's sections; `OnSectionsLoaded` pushes the object and calls `Activate` in the same call, with zero `Flux.BuildGraph` frames run (`Activate` before any drain and before any `RequestUpload` is observed, since the harness records only the forwarded calls, as: right after `LoadAndWait` returns with zero `Flux.BuildGraph` frames run, `Observe` of the test's handle reads ACTIVE with residency PENDING, the SECTIONS_LOADED event is recorded, the GEOM load queue holds the object and `GetBytesQueued` is 0 for every class); the first drain allocates and queues the uploads, and `ReportGpuResident` follows with the load's ticket once all complete; a hit of GEOM's fault point `uFAULT_POINT_GEOM_DRAIN_ALLOCATE` at the drain calls `ReportGpuFailed` with `FLUX_GEOMETRY_ERROR_POOL_FULL`, and a hit of `uFAULT_POINT_GEOM_UPLOAD_RESULT` `ReportGpuFailed` with `FLUX_GEOMETRY_ERROR_UPLOAD_FAILED`, never `Fail`, with nothing left allocated; a corrupt fixture calls `Fail` with its `Flux_GeometryErrorCode` before any push; `Cancel` before activation ends in `AcknowledgeCancel` with no range, block or staging left; a device-recovery load activates with the existing object's pointer, and one issued while that object is still queued leaves one queue entry; an `Unload` of a queued object is acknowledged by the drain that pops it; the load queue: a harness registry with `m_uMaxAssets` = 64 gives `uGEOM_LOAD_QUEUE_CAPACITY` 128, and 64 activated loads plus 64 reload swaps with no drain fill 128 entries, the queue's `.full` counter staying 0; `DropCpuCopies` returns 0 with 10 resident and 10 failed objects of each kind |
| `Test_CookTerrain_TilesAndPyramid` | `ZenithCook/Tests/Terrain/Zenith_CookTerrain_Tests.cpp` | `Zenith_CookTerrainGeometry` given the 1,024 m fixture as a `Zenith_TerrainGeometryCookInput` (8 × 8 tiles, 0.5 m, r = 8, two tiles with hole masks) yields 64 tiles of 257 × 257 quantised samples equal to the formula of "Terrain cook contract"; cooked through the texture step, their 9 mips have extents 257, 129, 65, 33, 17, 9, 5, 3, 2 and mip l's texel (i, j) equals mip 0's texel (i·2^l, j·2^l) bit for bit; a shared tile border is identical in both tiles; section 0x0101 holds the listed fields (level count 7, node count 4^6 + … + 1 over the 64 × 64 level-0 grid, flags bit 0 set), the pyramid bounds every sample of each node's closed rectangle, and sections 0x0103 and 0x0104 hold the input GUIDs row-major with zero entries exactly for the 62 tiles without holes |
| `Test_CookTerrain_InputRejected` | same | one failing input per row of the validation table of "Terrain cook contract" returns its error and writes no section: tile counts 0 and 257, a spacing of 0, a range narrower than 0.01 m, r = 7.9, 63 tiles for an 8 × 8 grid, a duplicated cell, 66,048 heights, a NaN height, a height above the range, a hole byte of 128, a hole GUID without a mask, and two neighbours whose shared border differs by one bit |
| `Test_ShaderCompiler_StreamSetTables` | `Zenith2/Tools/ZenithShaderCompiler/Tests/` | for a fixture program, every specialised table equals the subset of the drawable code's entry in the generated `axFLUX_STREAMSET_LAYOUTS` (the Slang reflection of `StreamSets.slang`) holding the attributes the program declares, with the entry's offsets, strides, locations and layout hash, followed by the null-stream attributes; named cases: `pos-snorm16x4|surface-oct16` for a QTangent-declaring MESH program (the surface attribute keeps semantic `QTANGENT` and location 1 with format SNORM16X2 at stream 1, offset 0; TEXCOORD0 at stream 7, offset 0, stride 0), `terrain:pos-snorm16x4` for a terrain program (POSITION alone, no null stream), a procedural code for a MESH program (streams 0 and 1 as declared), and `pos-pool16x4|surface-qt16-uv16` for a MESH program's `bPERM_SKINNED = true` permutation (POSITION SNORM16X4 at stream 7 offset 0 and stride 0, QTANGENT and TEXCOORD0 at stream 1 as declared, no stream 0 in the table); a code that is not drawable has no table, including the cooked skinned code `pos-snorm16x4|surface-qt16-uv16|skin-j8w8` for every MESH program, and a MESH program's table count is at most 110 |
| `Test_Cook_DiagnosticsNameAssetAndKey` | `ZenithCook/Tests/Geometry/Zenith_CookDiagnostics_Tests.cpp` | one failing fixture per GEOM step code of "Cook diagnostics" (mesh, skin, material and texture codes; the terrain codes are returned, not reported, and are asserted by `Test_CookTerrain_InputRejected`), each run in a `Zenith_MakeCookTestContext` context (Z2T-ASSET-096), asserts that `GetDiagnostics()` holds one record at the code's level whose message is `<CODE>: <asset GUID> <logical path>: <element>: <detail>`, and that an error is `COOK_ERROR_STEP_FAILED` with the GEOM code in `m_uDetail`; the cook-report record is asserted through the driver by `Test_GeomCook_CleanEqualsIncrementalAllKinds` |
| `Test_Cook_GeomStepsRegistered` | `ZenithCook/Tests/Geometry/Zenith_CookMesh_Tests.cpp` | after `Zenith_RegisterGeomCookSteps` and `Freeze`, the registry holds `geom.mesh`, `geom.texture` and `geom.material` (`COOK_PHASE_ASSET`, their kinds, version 1) and `geom.permutations` (`COOK_PHASE_GLOBAL_POST`, `ASSET_KIND_NONE`, version 1); a second registration before `Freeze` returns `COOK_ERROR_INVALID_STEP` |
| `Test_GeomFrame_StepOrder` | `Zenith2/Engine/ZenithFlux/Tests/Geometry/Flux_GeomFrame_Tests.cpp` | GEOM's own observer `Flux_GeomFrameSpy` over the GEOM work of `Flux.BuildGraph` observes the eight steps of "Frame-level flow of GEOM work" in order on each of 10 frames, with the procedural records applied before the GPU scene's deltas |
| `Test_Cook_PackerReadsGeneratedTable` | `ZenithCook/Tests/Geometry/Zenith_CookMesh_Tests.cpp` | the mesh step packs from the generated entry of its code: a fixture mesh's streams equal a pack driven by `axFLUX_STREAMSET_LAYOUTS`, its recorded hash equals the entry's, and a code with no entry fails with `GEOM_COOK_ERROR_STREAM_SET_UNDECLARED` |
| `Test_GeomDebugViews_Render` | `ZenithFlux/Tests/Geometry/` | each debug view renders a fixture scene without validation messages |
| `Test_GeomFrameDump_Section` | `ZenithFlux/Tests/Geometry/` | the frame dump's `geometry` object holds the listed members |
| `Test_GeomErrors_ToStringEveryEnumerator` | `ZenithFlux/Tests/Geometry/Flux_GeomErrors_Tests.cpp` | every enumerator of `Flux_GeometryErrorCode` and `Flux_VertexError` has a distinct string (the `Zenith_VertexFormatError` strings are `Test_VertexFormatErrors_ToStringEveryEnumerator`, `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Vertex format tests"`) |

Runtime tests also in this file's scope: `Test_Mesh_DeviceLossReloads` (after a simulated device loss every resident mesh redraws bit-identically), `Test_GeomManagers_Create_FailsAt<Step>_Unwinds` (one per construction step of each manager), and `Test_GeomCounters_Emitted` (a recording hook table sees every counter of "Counters" with its expected value in a scripted scene).

### Fixtures

No fixture a test needs is a generated binary in the tree: binary fixtures are generated at build or test time from committed code or text, and no GEOM render test compares against a committed reference image. Render scenarios compare within the run: same-device reload and replacement equivalence (a frame after an unload and reload, or a replacement, against a fresh instance that loaded the final state from its start), cross-device tolerance (lavapipe against WARP), a path against the Forward path at the `PathParity` tolerance, full residency against a streamed settle, and analytic expectations (CPU reference skinning, analytic ribbons, exact unlit pixels).

Every preset that runs a GEOM test produces that test's inputs itself. `ZenithCook_Tests` exists only where ZenithCook builds (`ZENITH_TOOLS_HOST`, the `win` preset), so a fixture set ZenithCook generates is registered only there, and every consumer that runs in `win-asan` or `linux-tsan` builds its inputs in code ([Z2-N-ARCH-257]). No test double of a concrete engine class exists: cook steps run in contexts from ASSET's `Zenith_CookTestContexts.h` (Z2T-ASSET-096), loaders run against a real registry through ASSET's `Zenith_AssetKindLoaderHarness.h` (Z2T-ASSET-097), and GEOM's own failures are injected through GEOM's fault points.

| Fixture set | Path and generation | Presets |
|---|---|---|
| cook-step contexts | every cook-step and importer test gets its `Zenith_CookContext` from `Zenith_MakeCookTestContext`, or its `Zenith_ImportContext` from `Zenith_MakeImportTestContext` (settings from `Zenith_MakeTestImportSettings`), of ASSET's `Zenith2/Tools/ZenithCook/Internal/Cook/Zenith_CookTestContexts.h` (Z2T-ASSET-096): in-memory settings, intermediates, dependency outputs and source bytes, and a capturing sink for published files, properties, references, report members and diagnostics; a whole step runs through `RunAddKeyInputs` and `RunCook`. No test constructs a context or runs the cook driver | `win` |
| mesh sources: 200,000-vertex split mesh, 200 m building, precision-failing meshes per attribute, two-material split mesh, inside-out mesh, eight-influence and 300-joint skins, 5 m creature, morph mesh | built in code with fixed seeds by `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookMeshFixtures.cpp`; a source reaches `ReadIntermediate("mesh")` as the bytes of `Zenith_WriteCookMeshSource`; no source file is committed | `win` |
| seed files | `Zenith2/Engine/ZenithFlux/Tests/Support/Flux_GeomSeedFiles.h`, header-only ([Z2-N-ARCH-255]): `Flux_BuildSeedMeshSections`, `Flux_BuildSeedTextureSections`, `Flux_BuildSeedMaterialSections` and `Flux_BuildSeedCookedFile`, the decoded sections of minimal valid `.zmesh`, `.ztex` and `.zmat` files written field by field from "The cooked mesh format", "The cooked texture format" and "The cooked material format", wrapped by ASSET's `Zenith_CookedFileWriter`, and `Flux_BuildSeedSkinnedMeshSections(xCookedCode, uJointCount, uMorphTargets, axOut)`, a unit quad in any cooked skinned code with its skin section and morph targets (Z2T-GEOM-064). Reader tests mutate copies in memory; loader tests mount them in a real `Zenith_AssetRegistry` through ASSET's header-only `Zenith2/Engine/ZenithAssets/Tests/Support/Zenith_AssetKindLoaderHarness.h` (Z2T-ASSET-097), one `Zenith_HarnessAsset` per seed file, whose `Initialise` writes the catalogue from the `Zenith_HarnessAsset` list the test builds | every preset that builds `ZenithFlux_Tests` (`win`, `win-asan`, `linux-tsan`) |
| fuzz seeds | none on disk: `Zenith_Fuzz_MeshFile`, `_TextureFile` and `_MaterialFile` parse the raw input, then the seed file of `Flux_GeomSeedFiles.h` with the input applied as (offset, byte) overwrites; their smokes require no fixture | `win-asan` |
| GEOM fault points | `Zenith2/Engine/ZenithFlux/Internal/Geometry/Flux_GeomFaultPoints.h` (prefix `0x4745`, [Z2-N-CONTRACT-262]): `uFAULT_POINT_GEOM_DRAIN_ALLOCATE`, `uFAULT_POINT_GEOM_UPLOAD_RESULT` and `uFAULT_POINT_GEOM_CREATE_BASE` + step, consulted through the `Zenith_FaultInjector` given to the uploader and to `Flux_GeomSystemsDesc` | every preset |
| cooked mesh, texture, material and terrain fixtures (`quad.zmesh`, `split200k.zmesh`, `split200k.triangles`, `twoMaterial.zmesh`, `float32.zmesh`, `colour1024.ztex`, `normal512.ztex`, `height257.ztex`, `splat256.ztex`, `mask512.ztex`, `standard.zmat`, `transparent.zmat`, `decal.zmat`, `unlit.zmat`, the 1,024 m terrain's four sections and its tile textures, `terrainGrid.zmesh`, and the skinned `human.zmesh`, `joints300.zmesh` and `creature5m.zmesh`) | generated at test time, never committed: the case `Fixture_GeomCooked_Generate` of `ZenithCook_Tests` (doctest suite `z2-fixture-setup`, file `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_GeomFixtures_Generate.cpp`) runs the GEOM cook steps in `Zenith_MakeCookTestContext` contexts (Z2T-ASSET-096) over the code-built sources (the skinned sources with the skeleton's published properties as dependency entries, the terrain grid imported first through `geom-grid-mesh` in a `Zenith_MakeImportTestContext` context) and writes the captured files, with `fixtures.json` (name, GUID, kind and hard dependencies of each file), into the directory the environment variable `ZENITH2_GEOM_COOKED_FIXTURES` names, `${ZENITH_OUT}/test-fixtures/GEOM/`. The test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` registers it as the CTest entry `GEOM_CookedFixtures` with `FIXTURES_SETUP GeomCookedFixtures`, and the doctest suite `z2-geom-cooked` of `ZenithFlux_Tests` (and of `Zenith_Integration_GeomScene`) as entries with `FIXTURES_REQUIRED GeomCookedFixtures`, all with that variable ([Z2-N-ARCH-257]) and inside `if(TARGET ZenithCook_Tests)`, so they exist in `win` alone. A consumer loads the files through `Zenith_AssetKindLoaderHarness`, whose `Initialise` writes the catalogue from the `Zenith_HarnessAsset` list the test builds from `fixtures.json` (one entry per row: the file's bytes, `content:/Test/<name>`, the GUID, kind and hard dependencies); a test needing a mutated variant copies a fixture into its own subdirectory and mutates the copy | `win` |
| text fixture games | `Zenith2/Tests/GeomCook/Fixtures/` and `Zenith2/Tests/GeomScene/Content/`: `geom-grid-mesh` and `geom-constant-texture` documents and `.material.json` documents with their `.meta` files, cooked by the suites with the `ZenithCook` executable; no CONTENT importer and no binary is involved | `win` |
| material documents and migration fixtures | `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/Material/`, `Zenith2/Tools/ZenithCook/Tests/Fixtures/Materials/` (JSON text) | `win` |
| QTangent reference vectors | `Zenith2/Engine/ZenithVertexFormat/Tests/Fixtures/QTangentVectors.json`, recorded once by a person from the record case (a `needs-human` step) | every preset |
| engine content sources: the fallback textures and the terrain grid mesh | text documents under `Zenith2/Engine/ZenithFlux/Internal/Content/`: `Textures/Fallback<Usage>.consttex.json` and `Fallback<Usage>Dev.consttex.json` (extent, channels, bit depth and a fill or checker of declared constants) and `Meshes/TerrainGrid.gridmesh.json` (`quadsX` and `quadsZ` 32, `fixedStreamSet` `terrain:pos-snorm16x4`), imported by GEOM's two declared-constant importers `geom-constant-texture` and `geom-grid-mesh` ("ZenithCook GEOM files"); no image or glTF binary is committed | cooked content |
| terrain cook fixtures | `Zenith2/Tools/ZenithCook/Tests/Fixtures/Terrain/`: a 1,024 m height field (8 × 8 tiles), a 384 m height field (3 × 3 tiles, for the edge rule) and a 1,000 m source (for `GEOM_COOK_ERROR_TERRAIN_EXTENT`), each generated by the test from its declared formula; the 32,768 m terrain of the level-7 to level-9 crack cases is generated by the test too, and nothing is committed | `win` |
| procedural fixtures | the ribbon source and a fixture generator per test of `Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural tests"`, in `Zenith2/Engine/ZenithFlux/Tests/Procedural/` | every preset |

### Device and acceptance runs

The GPU cases of `Test_VertexFormat_CpuGpuBitMatch`, `Test_VertexKernel_DeviceArithmeticConformance` and `Test_VertexFormat_FetchUnitParity` run on the RTX 3070 and the S24 as `needs-human` device steps in the Stage 2 and Stage 3 acceptance; the texture streaming and skinning workload measurements run in W-Outdoor, W-Characters and the flagship slices per `Zenith2/Docs/Budgets.md → "Measured in every workload run"`.

## Budget rows

### Rows owned

| Row | Name | Status | Producer | Measured by |
|---|---|---|---|---|
| Z2-B-GEOM-001 | Unified vertex and index pools | pending | the Stage 1 per-system budget ticket for GEOM | `Flux.Geometry.PoolBytesUsed` (raster and skin-source pools) plus the skinned, procedural (`Flux.Procedural.PoolBytesUsed`), meshlet and morph pools, WS-3D |
| Z2-B-GEOM-002 | Texture residency pool | pending | same | `Flux.Textures.ResidentBytes`, WS-FRAME |
| Z2-B-GEOM-003 | Streaming upload bytes per frame | pending | same | `Flux.Streaming.UploadBytes`, WS-STREAM, breach `BR-DEFER` |
| Z2-B-GEOM-004 | Compute skinning and morph targets GPU | pending | same | the `ComputeSkinning` and `MorphTargets` timestamps, W-Characters, W-Cinematic, W-OpenWorld |
| Z2-B-GEOM-005 | Procedural geometry regeneration GPU | pending | same | the `ProceduralGeometry` timestamps (GEOM's pass, `Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_AddProceduralGeometry contract"`), W-Outdoor, W-OpenWorld |
| Z2-B-GEOM-006 | Deferred decals GPU | pending | same | the `DeferredDecals` timestamps, WS-3D |

The Stage 1 per-system budget ticket for GEOM is blocked by the Stage 1 per-system budget harness for GEOM (`Zenith2/Benchmarks/Budgets/GEOM/`, measuring the reference engine's vertex pools, texture residency and compute skinning, as `Zenith2/Docs/Budgets.md` lists), and every GEOM ticket that enforces one of these rows is blocked by the budget ticket.

### Rows measured against

| Row | Why |
|---|---|
| `Z2-B-RPROD-024` Shader permutation limit per game | the cook's limit check |
| `Z2-B-RPROD-001` GPU scene tables | primitive and material-slot records GEOM allocates |
| `Z2-B-RHI-006` Upload ring hard cap | every GEOM upload |
| `Z2-B-RHI-004` Render graph build and compile CPU | GEOM's work on `Flux.BuildGraph` |
| `Z2-B-CORE-008` Texture-swap peak | the transition reservation |
| `Z2-B-ADVGFX-001`, `Z2-B-ADVGFX-002` | VT, which shares the uploader |
| `Z2-B-RPROD-006`, `-009`, `-010`, `-014`, `-016`, `-018` | passes whose cost the vertex format and materials drive |
| `Z2-B-EDIT-003` Shader blob reload, `Z2-B-EDIT-004` Asset reimport | material reload and mesh and texture reimport times |
| `Z2-B-SHIP-013` Cold boot Windows, `Z2-B-SHIP-014` Cold boot S24 | boot-time mesh and texture loads |
| `Z2-B-PACK-002`, `-003`, `-004` | texture and mesh cooked sizes |

### Pending row needs

These rows do not yet exist in `Zenith2/Docs/Budgets.md`; each is recorded in the cross-area notes for the Budgets writer, with the Stage 1 per-system budget ticket for GEOM as producer:

| Needed row | Unit | Procedure and workloads |
|---|---|---|
| Terrain geometry GPU | ms | GPU pass time of terrain nodes in the prepass, opaque and shadow passes, W-Outdoor, W-OpenWorld |
| Material constants memory | MB | `Flux.Materials.ConstantsBytes`, WS-FRAME; a member of `MEMORY_SUBSYSTEM_RENDERING` |
| Texture streaming request-to-resident latency | frames | from a requirement change to the swap, p95, WS-STREAM |

## Stage plan

### Stage table

| Stage | GEOM delivers | Gated by |
|---|---|---|
| S1 | the budget harness `Zenith2/Benchmarks/Budgets/GEOM/` and the budget ticket's allocation of GEOM-001 … -006; pull-forward (fix version S2): the `ZenithVertexFormat` API-only slice and the codec implementation (kernel, scalar, QTangent, octahedral, half, quantisation, weights, stream-set codes and names) | the budget ticket is `needs-human`; the codec depends on no open decision (QTangent is the default until D-VTX-TANGENTLESS, open, Stage 3) |
| S2 | `ZenithVertexFormat` complete, including the layout hash (after D-SEL-HASH, open, Stage 1) and the packer; the GPU codec modules and the CPU/GPU bit-match tests once the shader system and RHIs exist; the mesh cook (validation, weld, tangents, stream-set choice, precision checks and report, optimisation order, splitting, position-only indices, bounds), `.zmesh`, the geometry pools and the mesh manager (basic assets); the texture cook (mips, encoding), `.ztex`, the texture manager without streaming, fallback textures; the sampler policy; the material document schema and migration chain, the material cook and `.zmat`, the material manager and slot records (opaque and alpha-tested); the `geom.textureSource` and `geom.materialDocument` intermediates the texture and material steps read, and the material info record's terrain record offset | D-SLANG (open, Stage 1) for the handle fields of material constants; D-SEL-HASH (open, Stage 1) for the layout hash and variant IDs |
| S3 | terrain geometry (CDLOD, the terrain cook with the extent rule, the terrain renderer, Forward+), with every height and hole tile resident whole until streaming lands in S4 (ruling Q45: the Roadmap's Stage 3 W-Outdoor deliverable needs terrain; the Inventory's S4 placement changes in core-doc batch 4, pending core-doc batch 4 (Q45)), with the terrain record index and constant (`GetRecordIndex`, "Terrain record constant") and whole-chain texture replace from memory for ENV's live preview (open question Q52, ruled (a): the post-signoff patch of cross-area row EN21); skinning (skin data, posed extent, producer interface, pool, palette, dispatch, `Flux_SkinningBatch`), morph targets; procedural geometry (the pool, the generator interface, admission, regeneration, the pass, the ribbon generator, the draw path) with the GPU scene pools of Z2-I-RHI-08; material permutations (enumeration, the limit check with RPROD-024); transparency; decals (clustered, Forward+); LOD selection (authored chains); instancing; the D-VTX-TANGENTLESS harness and ruling | the RPROD budget ticket for RPROD-024; the D-VTX-TANGENTLESS ruling for any adoption |
| S4 | texture streaming (residency model, required mips, state machine, swap, arbitration, including the terrain tiles' external requirements and the residency clamp) and the VT boundary with ADVGFX; LOD generation | the GEOM budget ticket for GEOM-002 and -003 |
| S5 | meshlets and the visibility-buffer consumers (meshlet records and pools, terrain in the visibility buffer); deferred decals and per-object decals (with the Deferred and Forward paths); the advanced surface models, and GB4 once ruling X80 is applied to the Rendering docs | D-TBDR (open, Stage 5) does not gate GEOM; the Deferred and visibility-buffer paths (RPROD) |
| S6 | shipping qualification of GEOM rows on the S24 and the reference desktop; package sizes | — |
| S7 | nothing: virtualized geometry is ADVGFX's (Z2-D-043) | — |

### Ticket slices and ordering

1. API-only slice: the three `ZenithVertexFormat` headers with contract blocks and stub definitions returning `VERTEX_FORMAT_ERROR_UNKNOWN_FORMAT`, so the shader compiler and cook tickets can build against them.
2. Kernel, then scalar codecs, then QTangent and octahedral, then half, each with its tests.
3. Stream-set codes and layouts; the packer.
4. The `Flux_GeometryErrors.h`, `Flux_MeshResource.h`, `Flux_MaterialResource.h` and `Flux_TextureResource.h` API-only slices.
5. The mesh reader over ASSET's `Zenith_CookedFileView` and `Flux_MeshKindLoader` (after ASSET's container and loader interface land); then mesh cook steps in pipeline order, writing through `Zenith_CookedFileWriter`; then the mesh manager and pools.
6. Texture cook (with the `geom.textureSource` writer and reader) and manager; material cook (reading `geom.materialDocument` and recording the terrain record offset) and manager.
7. Stage 3 slices: terrain (the cook function, the record and renderer, the record index and constant, node selection, morphing, holes; gated by ENV's terrain asset); whole-chain texture replace from memory, before ENV's live-preview ticket; then skinning data → pool → palette → dispatch → morphs; procedural geometry (the API-only header, then the pool and admission, then regeneration and the pass, then the draw path); permutations; transparency; decals; LOD selection; instancing.
8. Stage 4 streaming slices: residency model, required mips, state machine without GPU copy, GPU copy of retained mips, arbitration, pressure shedding, the terrain tiles' external requirements; LOD generation.
9. Stage 5: meshlets; deferred and per-object decals; each surface model separately.

## Decisions

### Settled decisions

| ID | What it settles for GEOM |
|---|---|
| Z2-D-062 | the vertex data format: Slang-declared streams, a dedicated position stream, QTangent SNORM16x4, UNORM16 UVs with scale and bias, 16-bit indices with splitting, precision-checked fallbacks, `ZenithVertexFormat` |
| Z2-D-014 | the universal GPU performance rules the vertex format and materials follow |
| Z2-D-015 | Slang is the source of truth; the vertex tables and material reflection are generated |
| Z2-D-079 | materials are Slang surface functions plus a `.json`; no node graph |
| Z2-D-058 | GPU scene tables and pools; texture streaming by reallocate-and-swap |
| Z2-D-054 | the static sampler table and bindless textures |
| Z2-D-025 | permutations enumerated and compiled offline |
| Z2-D-038 | JSON for authoring, versioned binary for cooked data, no layout dependence |
| Z2-D-004 | codec IDs in cooked headers |
| Z2-D-043 | VT required in Stage 4 (ADVGFX), virtualized geometry deferred to Stage 7 |
| Z2-D-035 | every shading path shares `EvaluateMaterial` |
| Z2-D-048 | material documents migrate; cooked data re-cooks |
| Z2-D-012 | GEOM budget rows follow Budgets v1 |
| Z2-D-084 | the budget and D-VTX-TANGENTLESS measurements split into harness and ruling tickets |
| Z2-D-097 | a GEOM ruling is appended to `Zenith2/Docs/DecisionLog.md` in its ticket's diff |

### Open decisions

| Decision | Relationship |
|---|---|
| D-VTX-TANGENTLESS (open, Stage 3) | the harness and ruling; QTangent is the default until it rules; adoption would make SURF `OCT` with a `bPERM_TANGENTLESS` variant a quality-tier or material-class choice |
| D-SLANG (open, Stage 1) | the lowering of the `DescriptorHandle` fields in material `Params`; GEOM code depends only on the 8-byte `Zenith_RHIDescriptorHandle` ([Z2-N-REND-970]) |
| D-SEL-HASH (open, Stage 1) | layout hashes, material type and variant IDs, content hashes |
| D-SEL-FBX (open, Stage 4) | an FBX importer feeds the same mesh cook; no GEOM change |
| D-TBDR (open, Stage 5) | not used by GEOM |

### Design rulings owned by GEOM

These elaborate settled decisions and need no new decision ID; a ticket that changes one edits this section in its diff (Z2-D-085).

<!-- informative -->
| Ruling | Alternatives considered | Why this one |
|---|---|---|
| G1 the exact arithmetic kernel with fixed Newton sequences | correctly rounded square root and division by exact-residual correction; tolerance-based CPU/GPU tests | Newton sequences from integer seeds use only correctly rounded add, sub and mul, so bit identity follows from IEEE conformance alone; correct rounding needs exact residuals that FTZ can break; tolerance tests would not meet "bit for bit" |
| G2 stable 16-bit stream-set codes | dense runtime indices | a code names the same layout in every build, so recorded PSO sets, pipeline caches and cooked meshes agree |
| G3 fallbacks keep the decode formula with identity scale and bias | separate shader code per fallback | a fallback changes the PSO's vertex input and nothing else |
| G4 decode-route consistency | asserting fixed-function parity | no API guarantees fixed-function conversion bits; route consistency guarantees the equalities that matter |
| G5 CDLOD terrain with vertex-stage displacement | cooked chunk meshes with skirts; clipmaps; procedural-pool generation | one shared grid mesh, crack-free morphing, GPU-scene instances that culling already handles, and height tiles that stream as textures |
| G6 UASTC as the single intermediate for BC7 and ASTC | direct BC7 and ASTC encoders | one encoder to pin and test; ASTC is near-lossless from UASTC |
| G7 CPU required-mip computation from UV density | GPU sampler feedback | no readback, deterministic, independent of VT's feedback |
| G8 GPU copy of retained mips on a swap | re-reading every mip from disk | halves I/O and upload for increases; decreases need no I/O |
| G9 skinning palettes on the CPU | a GPU palette pass | 6,000 products per frame at W-Characters, and the inverse binds never reach the GPU |
| G10 stateless LOD selection from pixel-error thresholds | hysteresis bands | the GPU culling pass keeps no per-instance history; TAA absorbs a one-frame pop |
| G11 linear blend skinning with four weights | dual-quaternion skinning | matches content expectations and the four-weight stream format |
| G12 material blocks rewritten per texture swap | a texture indirection table | an extra table would be a fifth GPU-scene table outside Rendering's fixed set |
| G13 per-instance sorted transparency with back-then-front pairs | order-independent transparency | OIT needs per-pixel storage beyond the 16-bit target rules; sorted instances meet the slices' needs |
| G14 a fifth G-buffer target for extended surface models, proposed to Rendering and accepted as ruling X80; applied only when Rendering applies it | packing parameters into existing channels; forward-only support | existing channels are full; every path must shade the same materials; until X80 is applied the extended models are Forward+ and Forward only (`Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"`) |
| G15 the declared stream-sets are the reachable product, expanded by one preprocessor generator | a closed hand-written list; declaring codes on demand | a hand-written list turned mandated fallbacks into cook errors; the product costs one small table entry per code, and blobs are compiled only for used codes |
| G16 a declared-but-absent attribute reads a stride-0 zero stream | link-time conditional vertex fields; making such codes undrawable | one vertex struct per family stays valid on both backends, and every declared code is drawable |
| G17 raster pools keyed by the raster code; a compute-only skin-source pool keyed by the cooked code | pools keyed by the cooked code; a bucket key carrying the cooked code | RPROD's bucket key is the raster code, and one bucket must bind one pool set |
| G18 procedural geometry in the fixed-function default stream layout, regenerated into one two-region pool | vertex pulling from the pool | procedural draws reuse every raster program, bucket and depth-equal rule of meshes |
<!-- /informative -->

### Classification decisions GEOM owns

| Capability | Classification | Stage | Rationale |
|---|---|---|---|
| clear coat | required | S5 | car paint, varnished wood, lacquer in CinematicSlice |
| anisotropy | required | S5 | brushed metal and hair-like highlights on cards (hair itself is excluded, Z2-D-078) |
| sheen | required | S5 | cloth on characters |
| subsurface approximation | required | S5 | skin and foliage in CinematicSlice; no screen-space diffusion |
| order-independent transparency | not provided (design ruling G13) | — | per-instance sorting |
| dual-quaternion skinning | not provided (design ruling G11) | — | — |
| mesh LOD streaming | not provided: meshes load whole; world-scale LOD is STREAM's HLOD | — | — |

The ticket that implements each Stage 5 surface model records its classification in `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D-NNN` in its diff (Z2-D-097).

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/GEOM.md` and the design headings that specify it.

| Requirement | Title | Stage | Specified in |
|---|---|---|---|
| Z2-R-GEOM-001 | Leaf library with a Core-only dependency | S2 | Zenith2/Docs/Design/GEOM.md → "Target summary"; Zenith2/Docs/Design/GEOM.md → "ZenithVertexFormat files"; Zenith2/Docs/Design/GEOM.md → "Libraries GEOM code depends on" |
| Z2-R-GEOM-002 | Public headers carry contracts and are self-contained | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexKernel header"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set codes"; Zenith2/Docs/Design/GEOM.md → "Rules across the GEOM API" |
| Z2-R-GEOM-003 | API-only slice lands first | S2 | Zenith2/Docs/Design/GEOM.md → "Ticket slices and ordering" |
| Z2-R-GEOM-004 | Codec pulled forward into Stage 1 | S2 | Zenith2/Docs/Design/GEOM.md → "Stage table" |
| Z2-R-GEOM-005 | Closed format vocabulary | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header" |
| Z2-R-GEOM-006 | Closed semantic vocabulary and fixed locations | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Canonical stream-set layouts" |
| Z2-R-GEOM-007 | Layout-table types match the generated headers | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header" |
| Z2-R-GEOM-008 | Layout validation | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Layout validation and hashing" |
| Z2-R-GEOM-009 | Canonical layout hash | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Layout validation and hashing" |
| Z2-R-GEOM-010 | Kernel operation set | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel operations"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel constants" |
| Z2-R-GEOM-011 | Newton reciprocal and square root | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel operations"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel preconditions and device conformance" |
| Z2-R-GEOM-012 | Round half away from zero without the add trick | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel operations" |
| Z2-R-GEOM-013 | No contraction in either half | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel preconditions and device conformance"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules" |
| Z2-R-GEOM-014 | Device arithmetic conformance recorded | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Kernel preconditions and device conformance"; Zenith2/Docs/Design/GEOM.md → "Device and acceptance runs" |
| Z2-R-GEOM-015 | Scalar lane codecs | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Scalar quantisation codecs" |
| Z2-R-GEOM-016 | QTangent encode with the bias rule | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation" |
| Z2-R-GEOM-017 | QTangent degenerate tangent | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation" |
| Z2-R-GEOM-018 | QTangent decode never flips the normal | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "QTangent codec realisation" |
| Z2-R-GEOM-019 | Octahedral codec | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Octahedral codec realisation" |
| Z2-R-GEOM-020 | Integer-only half codec | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Half-float codec realisation" |
| Z2-R-GEOM-021 | Position quantisation to the tight box | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position and UV quantisation" |
| Z2-R-GEOM-022 | UV remapping with scale and bias | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position and UV quantisation" |
| Z2-R-GEOM-023 | Weights sum exactly | S3 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Weight quantisation" |
| Z2-R-GEOM-024 | CPU and GPU halves match bit for bit | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Vertex format tests"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules" |
| Z2-R-GEOM-025 | GPU codec module contents | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "The GPU codec modules" |
| Z2-R-GEOM-026 | Stream-set code layout | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set code fields" |
| Z2-R-GEOM-027 | Canonical stream-set names | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Canonical stream-set names" |
| Z2-R-GEOM-028 | Stream-sets declared in Slang alone | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Declared stream-sets"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Canonical stream-set layouts" |
| Z2-R-GEOM-029 | Fallbacks map to field changes | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Fallback derivation rule" |
| Z2-R-GEOM-030 | Program stream-set specialisation | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation" |
| Z2-R-GEOM-031 | Stream-sets compiled from the engine list and content | S3 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"; Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration" |
| Z2-R-GEOM-032 | Position decode invariance | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Position decode invariance" |
| Z2-R-GEOM-033 | Decode-route consistency | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Decode-route consistency" |
| Z2-R-GEOM-034 | Packer driven by the generated table | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"; Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh cook pipeline" |
| Z2-R-GEOM-035 | Layout mismatch refused at bind | S2 | Zenith2/Docs/Design/GEOM.md → "Flux_GeometryErrors header"; Zenith2/Docs/Design/GEOM/Meshes.md → "Reader validation" |
| Z2-R-GEOM-036 | Mesh input validation | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Input validation" |
| Z2-R-GEOM-037 | Weld before quantisation | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Weld" |
| Z2-R-GEOM-038 | Tangent generation | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Tangent generation" |
| Z2-R-GEOM-039 | Stream-set choice | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Stream-set choice" |
| Z2-R-GEOM-040 | Shared quantisation across LODs and parts | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks" |
| Z2-R-GEOM-041 | Position precision check | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks" |
| Z2-R-GEOM-042 | UV precision check | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks" |
| Z2-R-GEOM-043 | Joint and weight precision checks | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Quantisation box and precision checks" |
| Z2-R-GEOM-044 | Fallbacks reported, never silent | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"; Zenith2/Docs/Design/GEOM.md → "Cook report members" |
| Z2-R-GEOM-045 | meshoptimizer order | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Optimisation order" |
| Z2-R-GEOM-046 | Splitting at 65,535 vertices | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table" |
| Z2-R-GEOM-047 | Split without cracks | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table" |
| Z2-R-GEOM-048 | Sections stable across LODs | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh terms"; Zenith2/Docs/Design/GEOM/Meshes.md → "Splitting and part table" |
| Z2-R-GEOM-049 | Position-only index buffer | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Position-only indices" |
| Z2-R-GEOM-050 | Part bounds and UV density | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Bounds and UV density" |
| Z2-R-GEOM-051 | Mesh cook determinism | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh cook pipeline" |
| Z2-R-GEOM-052 | GEOM cooked file header | S2 | Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"; Zenith2/Docs/Design/GEOM.md → "Byte order and serialization rules" |
| Z2-R-GEOM-053 | Mesh file records | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format" |
| Z2-R-GEOM-054 | Mesh reader validation | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Reader validation" |
| Z2-R-GEOM-055 | Mesh resource API | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header" |
| Z2-R-GEOM-056 | Raster and skin-source stream-set pools | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"; Zenith2/Docs/Design/GEOM/Meshes.md → "Pool allocator" |
| Z2-R-GEOM-057 | Buffer usage unions | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools" |
| Z2-R-GEOM-058 | Pool growth within the budget | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Pool growth" |
| Z2-R-GEOM-059 | Pools are persistent read-only | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"; Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine" |
| Z2-R-GEOM-060 | Mesh residency state machine | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM.md → "Asset kind loaders" |
| Z2-R-GEOM-061 | Mesh load cancellation | S2 | Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM.md → "Cancellation" |
| Z2-R-GEOM-062 | Mesh device-loss reload | S6 | Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"; Zenith2/Docs/Design/GEOM.md → "Failure table" |
| Z2-R-GEOM-063 | Authored LOD chains | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation" |
| Z2-R-GEOM-064 | LOD generation | S4 | Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation" |
| Z2-R-GEOM-065 | LOD thresholds from pixel error | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "LOD generation" |
| Z2-R-GEOM-066 | LOD selection | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"; Zenith2/Docs/Design/GEOM.md → "Header map" |
| Z2-R-GEOM-067 | Shadow LOD from the reference view | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection" |
| Z2-R-GEOM-068 | Shared records for instancing | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Instancing" |
| Z2-R-GEOM-069 | Per-instance data | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Instancing" |
| Z2-R-GEOM-070 | Meshlet generation | S5 | Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets" |
| Z2-R-GEOM-071 | Meshlet record and pools | S5 | Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets"; Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools" |
| Z2-R-GEOM-072 | Terrain node selection | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Node selection" |
| Z2-R-GEOM-073 | Terrain morphing without cracks | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement" |
| Z2-R-GEOM-074 | Terrain data and cook | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract" |
| Z2-R-GEOM-075 | Terrain renderer API | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant" |
| Z2-R-GEOM-076 | Terrain in the deferred and visibility-buffer paths | S5 | Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement" |
| Z2-R-GEOM-077 | Skin section of the mesh | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data" |
| Z2-R-GEOM-078 | Posed-extent check | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Posed-extent check" |
| Z2-R-GEOM-079 | Skinned-pose producer interface | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_Skinning header" |
| Z2-R-GEOM-080 | Skinning palette on the CPU | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning palette computation" |
| Z2-R-GEOM-081 | Skinned vertex pool | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned vertex pool layout" |
| Z2-R-GEOM-082 | Compute skinning dispatch | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"; Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_SkinningBatch" |
| Z2-R-GEOM-083 | Skinned instances drawn the frame they are skinned | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning tests" |
| Z2-R-GEOM-084 | Skinned velocity and history reset | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Velocity and history reset" |
| Z2-R-GEOM-085 | Skinned LOD handling | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned vertex pool layout" |
| Z2-R-GEOM-086 | Morph target format | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets" |
| Z2-R-GEOM-087 | Morph target application | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets" |
| Z2-R-GEOM-088 | Material document schema | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema" |
| Z2-R-GEOM-089 | Material migration chain | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"; Zenith2/Docs/Design/GEOM.md → "Migration and re-cook" |
| Z2-R-GEOM-090 | Material module conventions | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"; Zenith2/Docs/Design/GEOM/Materials.md → "Material reflection input" |
| Z2-R-GEOM-091 | Material cook validation | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material cook" |
| Z2-R-GEOM-092 | Material variant ID | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material cook" |
| Z2-R-GEOM-093 | Material file format | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"; Zenith2/Docs/Design/GEOM/Materials.md → "Material reader validation" |
| Z2-R-GEOM-094 | Material resource API | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Flux_MaterialResource header" |
| Z2-R-GEOM-095 | Material constants and slots | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants" |
| Z2-R-GEOM-096 | Parameter replacement by swap | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants" |
| Z2-R-GEOM-097 | Permutation enumeration | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration" |
| Z2-R-GEOM-098 | Missing permutation is a cook error | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Material permutation enumeration"; Zenith2/Docs/Design/GEOM/Materials.md → "Material fallbacks" |
| Z2-R-GEOM-099 | Permutation count report | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Permutation limit check and report" |
| Z2-R-GEOM-100 | Permutation limit enforced | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Permutation limit check and report" |
| Z2-R-GEOM-101 | Transparent blend modes | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Transparency" |
| Z2-R-GEOM-102 | Transparent ordering and two-sided pairs | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Transparency" |
| Z2-R-GEOM-103 | Transparent refraction and shadows | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Transparency" |
| Z2-R-GEOM-104 | Decal records and render decals | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Decal records" |
| Z2-R-GEOM-105 | Decal order and blending | S3 | Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application" |
| Z2-R-GEOM-106 | Deferred and per-object decals | S5 | Zenith2/Docs/Design/GEOM/Materials.md → "Decal order and application" |
| Z2-R-GEOM-107 | Clear coat surface model | S5 | Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"; Zenith2/Docs/Design/GEOM.md → "Classification decisions GEOM owns" |
| Z2-R-GEOM-108 | Anisotropy surface model | S5 | Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models" |
| Z2-R-GEOM-109 | Sheen surface model | S5 | Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models" |
| Z2-R-GEOM-110 | Subsurface approximation surface model | S5 | Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models" |
| Z2-R-GEOM-111 | Extended surface data in the G-buffer | S5 | Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models" |
| Z2-R-GEOM-112 | Material fallbacks | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material fallbacks"; Zenith2/Docs/Design/GEOM.md → "Failure table" |
| Z2-R-GEOM-113 | Material preview and reload support | S3 | Zenith2/Docs/Design/GEOM.md → "Editor surfaces" |
| Z2-R-GEOM-114 | Texture usage classes | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes" |
| Z2-R-GEOM-115 | Mip generation | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline" |
| Z2-R-GEOM-116 | Block encoding through KTX-Software | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline" |
| Z2-R-GEOM-117 | Texture file format | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format" |
| Z2-R-GEOM-118 | Texture reader validation | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format" |
| Z2-R-GEOM-119 | Sampler policy | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "Sampler policy" |
| Z2-R-GEOM-120 | Texture resource API | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header" |
| Z2-R-GEOM-121 | Tail residency | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model" |
| Z2-R-GEOM-122 | Required-mip computation | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Required-mip computation" |
| Z2-R-GEOM-123 | Residency budget | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model" |
| Z2-R-GEOM-124 | Streaming state machine | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine" |
| Z2-R-GEOM-125 | Reallocate-and-swap | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Reallocate-and-swap realisation" |
| Z2-R-GEOM-126 | Retained mips copied on the GPU | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Reallocate-and-swap realisation" |
| Z2-R-GEOM-127 | Two tables sharing a texture during a swap | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Reallocate-and-swap realisation" |
| Z2-R-GEOM-128 | Streaming cancellation | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine"; Zenith2/Docs/Design/GEOM.md → "Cancellation" |
| Z2-R-GEOM-129 | Texture memory-pressure shedding | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"; Zenith2/Docs/Design/GEOM.md → "Failure table" |
| Z2-R-GEOM-130 | Streaming upload arbitration | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Streaming upload arbitration" |
| Z2-R-GEOM-131 | Virtual texturing boundary | S4 | Zenith2/Docs/Design/GEOM/Textures.md → "Virtual texturing boundary" |
| Z2-R-GEOM-132 | Fallback textures | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures" |
| Z2-R-GEOM-133 | D-VTX-TANGENTLESS harness | S3 | Zenith2/Docs/Design/GEOM.md → "Open decisions"; Zenith2/Docs/Design/GEOM.md → "Stage table" |
| Z2-R-GEOM-134 | D-VTX-TANGENTLESS measurement and ruling | S3 | Zenith2/Docs/Design/GEOM.md → "Open decisions" |
| Z2-R-GEOM-135 | QTangent default until the ruling | S3 | Zenith2/Docs/Design/GEOM.md → "Open decisions"; Zenith2/Docs/Design/GEOM/Meshes.md → "Stream-set choice" |
| Z2-R-GEOM-136 | GEOM counters | S2 | Zenith2/Docs/Design/GEOM.md → "Counters" |
| Z2-R-GEOM-137 | GEOM debug views | S3 | Zenith2/Docs/Design/GEOM.md → "Debug views" |
| Z2-R-GEOM-138 | GEOM frame-dump section | S3 | Zenith2/Docs/Design/GEOM.md → "Frame-dump entries" |
| Z2-R-GEOM-139 | Cook diagnostics | S2 | Zenith2/Docs/Design/GEOM.md → "Cook diagnostics" |
| Z2-R-GEOM-140 | Error enumerations | S2 | Zenith2/Docs/Design/GEOM.md → "Error enumerations"; Zenith2/Docs/Design/GEOM.md → "Geometry error codes"; Zenith2/Docs/Design/GEOM.md → "Vertex format errors"; Zenith2/Docs/Design/GEOM.md → "Flux_GeometryErrors header" |
| Z2-R-GEOM-141 | No GEOM lock and bounded queues | S2 | Zenith2/Docs/Design/GEOM.md → "Lock-order entries"; Zenith2/Docs/Design/GEOM.md → "Frame-graph placement"; Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work" |
| Z2-R-GEOM-142 | Construction unwinds | S2 | Zenith2/Docs/Design/GEOM.md → "Construction and shutdown" |
| Z2-R-GEOM-143 | GEOM touches no simulation state | S2 | Zenith2/Docs/Design/GEOM.md → "Frame-graph placement" |
| Z2-R-GEOM-144 | GEOM budget rows allocated | S1 | Zenith2/Docs/Design/GEOM.md → "Rows owned" |
| Z2-R-GEOM-145 | GEOM budget harness | S1 | Zenith2/Docs/Design/GEOM.md → "Rows owned"; Zenith2/Docs/Design/GEOM.md → "Test targets and gate steps" |
| Z2-R-GEOM-146 | Pending GEOM row needs recorded | S1 | Zenith2/Docs/Design/GEOM.md → "Pending row needs" |
| Z2-R-GEOM-147 | Workload measurements | S3 | Zenith2/Docs/Design/GEOM.md → "Rows measured against"; Zenith2/Docs/Design/GEOM.md → "Device and acceptance runs" |
| Z2-R-GEOM-148 | Cook steps are pure and multi-process safe | S2 | Zenith2/Docs/Design/GEOM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/GEOM.md → "Multi-process cooking" |
| Z2-R-GEOM-149 | Cooked data re-cooks on version change | S2 | Zenith2/Docs/Design/GEOM.md → "Migration and re-cook" |
| Z2-R-GEOM-150 | Engine content cooked, not copied | S2 | Zenith2/Docs/Design/GEOM.md → "ZenithFlux GEOM files"; Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures" |
| Z2-R-GEOM-151 | Every reachable stream-set declared | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang" |
| Z2-R-GEOM-152 | Every mandated fallback of every family declared | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Fallback derivation rule"; Zenith2/Docs/Design/GEOM/Meshes.md → "Stream-set choice" |
| Z2-R-GEOM-153 | Null vertex stream for an absent attribute | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Layout validation and hashing"; Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang" |
| Z2-R-GEOM-154 | Surface substitution keeps the program semantic | S2 | Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation" |
| Z2-R-GEOM-155 | Skin-source pool keyed by the cooked code | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"; Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch" |
| Z2-R-GEOM-156 | Tied fallback record rule | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"; Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"; Zenith2/Docs/Design/GEOM/Skinning.md → "Posed-extent check" |
| Z2-R-GEOM-157 | Terrain extent is a whole number of tiles | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"; Zenith2/Docs/Design/GEOM.md → "Cook diagnostics" |
| Z2-R-GEOM-158 | Terrain edge handling | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Node selection"; Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement" |
| Z2-R-GEOM-159 | Material reader validation | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material reader validation" |
| Z2-R-GEOM-160 | Procedural geometry interface | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"; Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing" |
| Z2-R-GEOM-161 | Procedural stream-sets and pool layout | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural stream-sets and pool layout" |
| Z2-R-GEOM-162 | Generator registration | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing" |
| Z2-R-GEOM-163 | Procedural instance admission | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing" |
| Z2-R-GEOM-164 | Procedural regeneration frame | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"; Zenith2/Docs/Design/GEOM/Procedural.md → "Instance states" |
| Z2-R-GEOM-165 | Flux_AddProceduralGeometry contract | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_AddProceduralGeometry contract" |
| Z2-R-GEOM-166 | Drawing procedural instances | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Drawing procedural instances" |
| Z2-R-GEOM-167 | Procedural instance lifecycle and recovery | S3 | Zenith2/Docs/Design/GEOM/Procedural.md → "Registration and sizing"; Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural failure handling" |
| Z2-R-GEOM-168 | Asset kind loaders | S2 | Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"; Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"; Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"; Zenith2/Docs/Design/GEOM/Textures.md → "Texture residency model"; Zenith2/Docs/Design/GEOM.md → "Lock-order entries" |
| Z2-R-GEOM-169 | Terrain geometry sections | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"; Zenith2/Docs/Design/GEOM.md → "Format index" |
| Z2-R-GEOM-170 | Terrain splat usage classes | S3 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"; Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"; Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format"; Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures" |
| Z2-R-GEOM-171 | Terrain record constant written before the first draw | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"; Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"; Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"; Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work" |
| Z2-R-GEOM-172 | Whole-chain texture replace from memory | S3 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"; Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header"; Zenith2/Docs/Design/GEOM/Textures.md → "Streaming state machine" |
| Z2-R-GEOM-173 | Texture source image intermediate | S2 | Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"; Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"; Zenith2/Docs/Design/GEOM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/GEOM.md → "Cook diagnostics" |
| Z2-R-GEOM-174 | Material document intermediate | S2 | Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"; Zenith2/Docs/Design/GEOM/Materials.md → "Material cook"; Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"; Zenith2/Docs/Design/GEOM/Materials.md → "Material reader validation" |
| Z2-R-GEOM-175 | Per-mesh custom cull distance flag | S4 | Zenith2/Docs/Design/GEOM/Meshes.md → "The cooked mesh format"; Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"; Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads" |
| Z2-R-GEOM-176 | Render-target textures | S5 | Zenith2/Docs/Design/GEOM/Textures.md → "Render-target textures"; Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header"; Zenith2/Docs/Design/GEOM/Textures.md → "The cooked texture format" |
| Z2-R-GEOM-177 | Standard material surface weather | S4 | Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions" |
| Z2-R-GEOM-178 | Skinned mesh binds in the skeleton rest pose | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data" |
| Z2-R-GEOM-179 | Skin shell distance published per mesh | S3 | Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data" |
| Z2-R-GEOM-180 | Skeleton ID and morph target names on the mesh resource | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"; Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets" |
| Z2-R-GEOM-181 | Terrain adapter UVs and grid diagonal | S3 | Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"; Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry" |
