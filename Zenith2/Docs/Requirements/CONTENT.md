# CONTENT requirement catalogue

## Purpose

This catalogue breaks the CONTENT inventory items (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "CONTENT Content production") into testable requirements `Z2-R-CONTENT-NNN`. Each requirement names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of `Zenith2/Docs/Design/CONTENT.md` and its split files under `Zenith2/Docs/Design/CONTENT/` that specify it; the design document's "Requirement index" is the reverse map. CONTENT owns every requirement here. Where a requirement is proved partly by another area's work (ASSET's worker pool, SCENE's canonical form, ANIM's helpers, EDIT's channel implementation), that area is named in the evidence and in the design's "Dependencies", and the assumption is a row `CT<n>` of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`. Test names resolve to `Zenith2/Docs/Design/CONTENT/Tests.md`, which gives each test's file and asserted case. A requirement's stage is the stage whose acceptance it serves: the importers' core serves Stage 3 (its workloads import real content, and its acceptance requires reimport and corrupt-content evidence), and the inventory items that core serves, Z2-I-CONTENT-01, -02, -04, -06, -07, -15 and -22, are moved to S3 by the inventory change of cross-doc row X334 (`Zenith2/Docs/Design/CONTENT.md → "Stage 3"`), so no requirement sits at an earlier stage than an item it covers except Z2-R-CONTENT-133, the Stage 1 budget-row ruling, whose coverage basis is its budget rows.

## Requirements

### Z2-R-CONTENT-001 ZenithContentCore library target

- **Statement:** `ZenithContentCore` is a static tool library at `Zenith2/Tools/ContentCore/` declared `zenith_add_tool(ZenithContentCore DEPENDS ZenithCore ZenithDocument THIRD_PARTY fastgltf simdjson meshoptimizer stb-image dr-wav dr-flac dr-mp3 stb-vorbis SUPPORTED ZENITH_WINDOWS)` (lower-case kebab manifest names), with the five public headers and the internal files the design lists, and appears in no Player or Shipping link closure.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-042, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (declared edges, Shipping closure) and `SourceLayout` on the creating ticket; `CompileCoverage` in `win` and `win-asan`.
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Target summary"; Zenith2/Docs/Design/CONTENT.md → "ZenithContentCore internal files"; Zenith2/Docs/Design/CONTENT.md → "ZENITH_SUPPORTED matrix"

### Z2-R-CONTENT-002 CONTENT files inside ZenithCook

- **Statement:** CONTENT's importers live in `Zenith2/Tools/ZenithCook/Internal/Importers/` and its cook step in `Internal/Content/`, register only through `Zenith_RegisterContentImporters` (cook services item 4, after ASSET's passthrough importers and before every module's) and `Zenith_RegisterContentCookSteps` (item 6, after SCENE), and ZenithCook declares `ZenithContentCore` and `ZenithAuthoring` in its `DEPENDS`.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentImporters_RegistrationDescs` (the `Test` step on `win-debug`); `Libgraph`.
- **Design:** Zenith2/Docs/Design/CONTENT.md → "CONTENT's ZenithCook files"; Zenith2/Docs/Design/CONTENT/Importers.md → "Importer registration"

### Z2-R-CONTENT-003 CONTENT files inside ZenithAuthoring

- **Statement:** CONTENT's tool-side authoring code lives in `Zenith2/Tools/Authoring/Internal/CONTENT/` behind the one public root header `Zenith2/Tools/Authoring/Zenith_AuthoringContent.h` (the per-owner layout of `Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`), which includes no STL, third-party or platform header, and CONTENT's move appends exactly the edges its files include to ZenithAuthoring's `DEPENDS` (ZenithDocument, ZenithScene, ZenithAssets, ZenithContentCore; rule 4 of `Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, row ED50 (b)).
- **Covers:** Z2-I-CONTENT-14, Z2-I-CONTENT-16, Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-042, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` and `Libgraph` on the creating ticket.
- **Design:** Zenith2/Docs/Design/CONTENT.md → "CONTENT's ZenithAuthoring files"; Zenith2/Docs/Design/CONTENT.md → "Zenith_AuthoringContent header"

### Z2-R-CONTENT-004 ZenithContent tool target

- **Statement:** `ZenithContent` is a tool executable at `Zenith2/Tools/ZenithContent/` (`SUPPORTED ZENITH_TOOLS_HOST`) whose `main` dispatches exactly the subcommands `merge`, `batch`, `storage-check`, `ddc`, `distribute` and `helper` with the exit codes each section states.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-23, Z2-I-CONTENT-27, Z2-I-CONTENT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatchCommand_ExitCodes`, `Test_ContentMergeCommand_ExitCodesAndAtomicWrite` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT.md → "ZenithContent files"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch command line"; Zenith2/Docs/Design/CONTENT/Merge.md → "Driver command line"

### Z2-R-CONTENT-005 Tool-side API rules

- **Statement:** Every CONTENT type carries one contract block with Ownership, Thread affinity, Invalidation, Units, Errors and Version (plus Reentrancy for interfaces), every fallible CONTENT function returns `Zenith_Result`, and no byte read from a source, document, journal, cache entry or git object reaches an assert.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` over the public headers; review of the `Internal/` contract blocks; the fuzz targets of `Z2-R-CONTENT-056`.
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Rules across the CONTENT API"

### Z2-R-CONTENT-006 Content error record

- **Statement:** `Zenith_ContentError` is an 8-byte trivially copyable record of a `Zenith_ContentErrorCode` (numbered from 1, appended never renumbered, with a `Zenith_ToString` overload), a 16-bit detail and a 32-bit index, returned with the meaning of each row of the error table.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `static_assert(sizeof(Zenith_ContentError) == 8)`; `Test_ContentValidation_PresetRules` and the batch tests assert the codes they produce (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Errors"

### Z2-R-CONTENT-007 Import detail codes

- **Statement:** Every import failure and warning a CONTENT importer reports carries a `Zenith_ContentImportDetail` (failures below 0x8000, warnings from 0x8000), named in a report line `<source path>: <detail name>: <message>` with the element index where one applies.
- **Covers:** Z2-I-CONTENT-02, Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_MalformedSourcesFailCleanly`, `Test_GltfImporter_ReportMember` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Import detail codes"

### Z2-R-CONTENT-008 Supported-DCC matrix is normative and machine-checked

- **Statement:** The DCC routes and the feature support matrix of the design are the supported-DCC matrix, and `dcc_matrix.json` records each route's DCC and exporter versions, fixtures and per-feature levels, which equal the design table's cells.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_DccMatrix_TableAgreesWithDesign` (the `Test` step on `win-debug`); `Test_DccMatrix_EverySupportedRouteRecorded` (suite `z2-acceptance-content`, the Stage 4 acceptance).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "DCC routes"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Feature support matrix"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Per-DCC fixtures"

### Z2-R-CONTENT-009 glTF core support

- **Statement:** The `gltf` importer imports every glTF 2.0 core element of the core support table (both containers, every buffer form, every accessor type including sparse and normalized, every scene and node form, the three triangle modes, the attribute sets, morph targets, skins, animations, materials, textures, samplers and cameras) with the behaviour that table states.
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfSource_CoreElementsParse`, `Test_GltfAccessors_SparseNormalizedQuantized` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "glTF core support"; Zenith2/Docs/Design/CONTENT/Importers.md → "ZenithContentCore API"

### Z2-R-CONTENT-010 glTF extension support table

- **Statement:** Every extension row marked supported in the extension table imports with its stated behaviour by its stated stage, and every ignored row produces exactly one warning per (source, extension).
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfSource_ExtensionGating` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "glTF extension support"

### Z2-R-CONTENT-011 Unsupported glTF extensions

- **Statement:** A source whose `extensionsRequired` lists an extension the table does not support fails with `IMPORT_ERROR_SOURCE_INVALID`, detail `CONTENT_IMPORT_DETAIL_REQUIRED_EXTENSION`, naming the extension; an unsupported extension only used is ignored with a fallback where the table states one.
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_RequiredUnsupportedExtensionFails`, `Test_GltfSource_ExtensionGating` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "glTF extension support"; Zenith2/Docs/Design/CONTENT/Importers.md → "glTF importer"

### Z2-R-CONTENT-012 Meshopt and quantized geometry

- **Statement:** `EXT_meshopt_compression` views (with the octahedral, quaternion and exponential filters) and `KHR_mesh_quantization` accessors decode to the values the encoder was given, without reading the fallback buffer, from Stage 3.
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfAccessors_MeshoptDecode`, `Test_GltfAccessors_SparseNormalizedQuantized` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "glTF extension support"; Zenith2/Docs/Design/CONTENT/Importers.md → "ZenithContentCore API"

### Z2-R-CONTENT-013 Texture transforms

- **Statement:** `KHR_texture_transform` is baked into the UVs of each primitive whose material's textures on one UV set share a transform, and when they disagree the base colour texture's transform is baked with `CONTENT_WARNING_TEXTURE_TRANSFORM_CONFLICT`.
- **Covers:** Z2-I-CONTENT-02, Z2-I-CONTENT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_TextureTransformBaked` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Textures referenced by materials"

### Z2-R-CONTENT-014 Basis Universal textures

- **Statement:** A `KHR_texture_basisu` image is transcoded through KTX-Software to RGBA8 and written as a texture source with `CONTENT_WARNING_BASISU_REENCODED`.
- **Covers:** Z2-I-CONTENT-02, Z2-I-CONTENT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_BasisuTranscoded` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Textures referenced by materials"

### Z2-R-CONTENT-015 FBX through the selected conversion path

- **Statement:** When the D-SEL-FBX ruling adopts a converter, the `fbx` importer (`Zenith_FbxImporter`) runs it as a child process in a per-import temporary directory under its timeout, imports its glTF output through the glTF import algorithm with the source's settings, deletes the directory, and fails `CONTENT_IMPORT_DETAIL_FBX_CONVERTER` with the converter's standard error on a non-zero exit or missing output.
- **Covers:** Z2-I-CONTENT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-084, D-SEL-FBX (open, Stage 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_FbxImporter_ConverterPathImportsAsGltf` (the `Test` step on `win-debug`) in the adopted form; blocked by the D-SEL-FBX ruling ticket.
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "FBX route"; Zenith2/Docs/Design/CONTENT/Importers.md → "FBX importer"

### Z2-R-CONTENT-016 FBX refused when no path is selected

- **Statement:** When the D-SEL-FBX ruling is glTF-only, the `fbx` importer still claims `.fbx` and fails every source with `CONTENT_IMPORT_DETAIL_FBX_UNSUPPORTED` and the message naming the glTF routes, so an FBX file is never silently ignored.
- **Covers:** Z2-I-CONTENT-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-FBX (open, Stage 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_FbxImporter_GltfOnlyRefusesWithMessage` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "FBX importer"

### Z2-R-CONTENT-017 D-SEL-FBX fidelity harness

- **Statement:** `Zenith_Bench_FbxFidelity` runs each D-SEL-FBX candidate as a child process over the FBX reference exports, compares each output with the same asset's glTF export under every fidelity threshold and requires byte-identical repeated conversions, writing `Zenith2/Docs/Research/D-SEL-FBX/report.md`; the ruling ticket runs it and records the outcome.
- **Covers:** Z2-I-CONTENT-03, Z2-I-CONTENT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-097, D-SEL-FBX (open, Stage 4)
- **Budgets:** none
- **Acceptance evidence:** The harness's `--smoke` CTest run (the `Test` step on `win-debug`); the ruling ticket's report under `Zenith2/Docs/Research/D-SEL-FBX/` and its DecisionLog entry.
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "FBX route"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Fidelity thresholds"

### Z2-R-CONTENT-018 Skin import through ANIM's helpers

- **Statement:** The `gltf` importer passes converted nodes, skins and morph names to ANIM's `Zenith_BuildSkeletons` and writes each skinned mesh with its mesh-joint remap from the skeleton result, the skeleton product's GUID and its converted inverse bind matrices.
- **Covers:** Z2-I-CONTENT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_SkinsThroughAnimHelpers` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Skins"; Zenith2/Docs/Design/CONTENT/Importers.md → "Meshes"

### Z2-R-CONTENT-019 Skin weight validation and normalisation

- **Statement:** Up to eight influences per vertex are imported; negative weights fail `CONTENT_IMPORT_DETAIL_SKIN_WEIGHT`, out-of-range joints fail `CONTENT_IMPORT_DETAIL_SKIN_JOINT`, weights are normalised to sum 1 and sorted by descending weight, and an all-zero vertex binds to the skin's first joint with a warning.
- **Covers:** Z2-I-CONTENT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_SkinsThroughAnimHelpers` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Skins"

### Z2-R-CONTENT-020 Morph target import

- **Statement:** Morph targets import with converted position, normal and tangent deltas (zeros for absent attributes), names from `extras.targetNames` or `Target<n>`, at most 256 per mesh, and their names reach ANIM's skeleton curves.
- **Covers:** Z2-I-CONTENT-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_MorphTargets` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Morph targets"

### Z2-R-CONTENT-021 Animation import through ANIM's helpers

- **Statement:** Every glTF animation reaches ANIM's `Zenith_WriteClipIntermediates` with converted channel values for joints; a channel on a node that is no skeleton joint is dropped with `CONTENT_WARNING_NON_JOINT_CHANNEL`.
- **Covers:** Z2-I-CONTENT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_AnimationsThroughAnimHelpers` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Animations"

### Z2-R-CONTENT-022 Clip settings declared as source-wide defaults

- **Statement:** The `gltf` desc declares ANIM's clip import settings as source-wide defaults only, so a preset or the `.meta` settings set them for every clip of a source, and one clip's own values come from ANIM's clip annotation document attached by the clip's sub-asset entry, never from a second CONTENT mechanism.
- **Covers:** Z2-I-CONTENT-06, Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_AnimationsThroughAnimHelpers` (the `Test` step on `win-debug`); the clip-document case of `Test_ContentPipeline_ReimportPreservesOverrides` in the integration suite.
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Animations"; Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "What an authored override is"

### Z2-R-CONTENT-023 Generated standard material documents

- **Statement:** Each glTF material becomes a generated `geom.materialDocument` (canonical, schema version 1, `id` the product GUID) for `Material_Standard` with the factor and texture mapping of the materials table, and a primitive without a material uses the generated glTF default material.
- **Covers:** Z2-I-CONTENT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_GeneratedMaterialDocuments` (the `Test` step on `win-debug`); GEOM's `Test_Material_CookFromGeneratedDocument`.
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Materials"

### Z2-R-CONTENT-024 Occlusion roughness metallic packing

- **Statement:** A material's ORM binding is absent, the shared occlusion-metallic-roughness image used directly, or a packed 8-bit product `texture:<material>/orm` with occlusion in R (255 when absent) and roughness and metallic in G and B, exactly as the packing table states.
- **Covers:** Z2-I-CONTENT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_OrmPacking` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Textures referenced by materials"

### Z2-R-CONTENT-025 Alpha mode and double-sided mapping

- **Statement:** `OPAQUE`, `MASK` (with its cutoff) and `BLEND` map to the `opaque`, `alphaTest` and `transparent` domains, and `doubleSided` to `twoSided`.
- **Covers:** Z2-I-CONTENT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_GeneratedMaterialDocuments` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Materials"

### Z2-R-CONTENT-026 Unlit and emissive-strength extensions

- **Statement:** `KHR_materials_unlit` generates a `Material_Unlit` document and `KHR_materials_emissive_strength` multiplies the emissive factor.
- **Covers:** Z2-I-CONTENT-07, Z2-I-CONTENT-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_GeneratedMaterialDocuments` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Materials"

### Z2-R-CONTENT-027 Advanced material extensions

- **Statement:** From Stage 5, `KHR_materials_clearcoat` and `KHR_materials_sheen` map to GEOM's clear-coat and cloth material types; until then they are ignored with a warning.
- **Covers:** Z2-I-CONTENT-07
- **Stage:** S5
- **Classification:** deferred
- **Decisions:** Z2-D-079, Z2-D-043
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_GeneratedMaterialDocuments` (Stage 5 cases) (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Materials"

### Z2-R-CONTENT-028 Embedded and external images

- **Statement:** An external image URI binds the image asset's GUID through `ResolveReferencePath` (outside the roots fails); an embedded image becomes one texture product per usage role with the role's usage, colour space and the sampler's address hint in GEOM's texture source header.
- **Covers:** Z2-I-CONTENT-07, Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_EmbeddedAndExternalImages` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Textures referenced by materials"

### Z2-R-CONTENT-029 Camera import

- **Statement:** Each glTF camera becomes an inactive (Active false) `Zenith.Render.Camera` component of the generated prefab with the projection, field of view in degrees, unit-scaled planes and orthographic height of the cameras table, on a view-correction child exactly when Q ≠ I.
- **Covers:** Z2-I-CONTENT-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_CamerasAndLights` (the `Test` step on `win-debug`); the probe camera rows of `Test_DccMatrix_EveryRowHasPassingFixture`.
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Cameras"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Basis change algorithm"

### Z2-R-CONTENT-030 Punctual light import

- **Statement:** Each `KHR_lights_punctual` light becomes the directional, point or spot component of the lights table with candela or lux intensities, unit-scaled ranges (or the 0.01 lux range when absent) and cone angles in degrees.
- **Covers:** Z2-I-CONTENT-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_CamerasAndLights` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Lights"

### Z2-R-CONTENT-031 Generated model prefab

- **Statement:** Each glTF scene becomes a prefab product `prefab:<scene>` whose `scene.prefabDocument` intermediate is a canonical prefab document with one entity per node carrying the local transform, the mesh instance (the mirrored variant for a negative-determinant node), cameras and lights, and the default scene's prefab is the primary product.
- **Covers:** Z2-I-CONTENT-15, Z2-I-CONTENT-08, Z2-I-CONTENT-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_GeneratedPrefabDocument` (the `Test` step on `win-debug`); `Test_ContentPipeline_CookAndLoadFixtureGame` in the integration suite.
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Generated model prefab"

### Z2-R-CONTENT-032 Stable generated entity IDs

- **Statement:** A generated prefab entity's ID is `Zenith_DeriveAuthoringID(prefab GUID, local ID of its node key)`, where a node with `extras.zenithId` keys as `id:<zenithId>` and any other node by its path of escaped segments (the four reserved bytes `%`, `/`, `#`, `@` percent-encoded) from its nearest `zenithId` ancestor or the scene root, derived entities key as `/`, `<node key>/@view` and `<node key>/@instance<n>`, which no node can produce, so node keys and entity IDs are unique by construction; the ID survives exactly the edits that keep the key (for a `zenithId` node any rename, reparent or reorder of it or its ancestors), equal-named siblings are warned, and no node rename is recognised.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_NodeKeysFromZenithId`, `Test_GltfImporter_NodeKeysEscapeReservedCharacters`, `Test_GltfImporter_PrefabIdsStableAcrossReorder`, `Test_RenameRecognition_ZenithIdIsStable` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Node keys"; Zenith2/Docs/Design/CONTENT/Importers.md → "Generated model prefab"; Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Reimport guarantees"

### Z2-R-CONTENT-033 Physics collision products

- **Statement:** A mesh source whose `.meta` `physics.collision` is not `None` declares `collision:shape` (kind `CollisionShape`) and writes PHYS's collision source with the selected nodes' mesh GUIDs, converted world matrices and the physical material's GUID; a material path is refused.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_CollisionProduct` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Physics collision products"

### Z2-R-CONTENT-034 Primitive modes and default material

- **Statement:** Triangle strips and fans are converted to counter-clockwise triangle lists, non-triangle primitives are skipped with a warning, and slots are the distinct materials in first-appearance order with `material:@default` for primitives without one, at most 255.
- **Covers:** Z2-I-CONTENT-02, Z2-I-CONTENT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfSource_StripsAndFansWindCounterClockwise`, `Test_GltfImporter_MeshSourceContent` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Meshes"

### Z2-R-CONTENT-035 Mesh intermediate content

- **Statement:** Each mesh product writes GEOM's mesh source with the members of the mesh source table (model-space attributes after conversion, the all-or-nothing tangent rule, the texCoord-1 swap, influences, slots, morphs, authored LODs, skin data), and `mesh.combine = perSource` bakes the default scene's static instances into one mesh.
- **Covers:** Z2-I-CONTENT-02, Z2-I-CONTENT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_MeshSourceContent`, `Test_GltfImporter_CombinedMeshBakesInstances` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Meshes"

### Z2-R-CONTENT-036 Conversion settings

- **Statement:** The `gltf` and `fbx` importers declare `transform.sourceUp`, `transform.sourceFront`, `transform.mirror`, `transform.unitScale` and `normal.flipGreen` with the glTF convention as defaults, and refuse non-perpendicular axes and a scale outside 0.0001 … 10,000.
- **Covers:** Z2-I-CONTENT-10, Z2-I-CONTENT-11, Z2-I-CONTENT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentConversion_RefusesBadAxes`, `Test_ContentImporters_RegistrationDescs` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Conversion settings"

### Z2-R-CONTENT-037 Axis conversion

- **Statement:** Axis conversion applies the basis change B = s·M·R to vertices, directions, tangents, morph deltas, node transforms (by conjugation; TRS scales permuted with their signs kept), inverse binds and animation keys (quaternions as (w, det Q · Q · v)), so the converted world equals B applied to the source world for every axis pair and a mirrored node stays mirrored.
- **Covers:** Z2-I-CONTENT-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentConversion_EveryAxisPairRoundTrips`, `Test_ContentConversion_NegativeScaleSurvivesConversion`, `Test_GltfImporter_ProbeExpectationsUnderConversion` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Basis change algorithm"

### Z2-R-CONTENT-038 Handedness conversion

- **Statement:** A `transform.mirror` conversion reverses every triangle and negates every tangent sign, and camera and light directions follow through the proper view correction, so a left-handed source imports with outward winding and its views intact.
- **Covers:** Z2-I-CONTENT-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ProbeExpectationsUnderConversion` (mirrored case) (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Basis change algorithm"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Winding and the negative-determinant rule"

### Z2-R-CONTENT-039 Unit conversion

- **Statement:** The unit scale multiplies positions, translations, inverse-bind translations, morph position deltas, camera planes and orthographic heights and light ranges, and nothing else.
- **Covers:** Z2-I-CONTENT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ProbeExpectationsUnderConversion` (centimetre case) (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Unit conversion"

### Z2-R-CONTENT-040 Counter-clockwise winding preserved

- **Statement:** Imported triangles keep glTF's counter-clockwise winding and are reversed only for a mirror conversion or a mirrored variant ([Z2-N-REND-885]), proven by every probe cube triangle winding outward.
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ProbeCubeWindsOutward` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Winding and the negative-determinant rule"

### Z2-R-CONTENT-041 Mirrored instances

- **Statement:** A node whose converted world matrix has a negative determinant (from a negative TRS scale or a negative-determinant `matrix`, decomposed by ContentCore into a negative x scale and a proper rotation) references a mirrored variant `mesh:<key>@mirrored` with reversed triangles and negated tangent signs, declared only when such a node exists, at identity and under every conversion; a mirrored skin joint is warned.
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_MirroredNodeUsesReversedVariant`, `Test_GltfSource_MatrixNodeNegativeDeterminantDecomposes` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Winding and the negative-determinant rule"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Matrix nodes and the negative-determinant decomposition"

### Z2-R-CONTENT-042 Normals and tangents validated

- **Statement:** Missing normals become flat normals, bad normals are repaired, a primitive with any invalid source tangent loses its whole tangent set for GEOM to generate, valid tangents are orthogonalised, non-finite UVs fail the import, and no non-finite attribute reaches an intermediate.
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfSource_FlatNormalsWhenAbsent`, `Test_GltfSource_BadTangentsDiscardedPerPrimitive`, `Test_GltfImporter_NoNonFiniteAttributeReachesIntermediate` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Normals and tangents on import"

### Z2-R-CONTENT-043 Normal map green convention

- **Statement:** `normal.flipGreen` inverts the green channel of normal-usage images (embedded in a glTF or imported by the image importer), the route for DirectX-convention normal maps.
- **Covers:** Z2-I-CONTENT-13, Z2-I-CONTENT-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ImageImporter_UsagesAndLayouts` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Conversion settings"; Zenith2/Docs/Design/CONTENT/Importers.md → "Image importer"

### Z2-R-CONTENT-044 Bounds and facing report

- **Statement:** Every `gltf` and `fbx` import reports each mesh product's and the default scene's bounds in metres after conversion, the scene height and the yaw of the front marker's offset from the scene's root entity in its `contentImports` report element.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ReportMember` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Bounds and facing report"; Zenith2/Docs/Design/CONTENT/Importers.md → "Import report member"

### Z2-R-CONTENT-045 Preset size and facing expectations

- **Statement:** The `validate.*` settings check the scene height range, the maximum extent and the front-marker yaw, warning with the measured value and the correcting setting, or failing `CONTENT_IMPORT_DETAIL_EXPECTATION_FAILED` under `validate.strict`.
- **Covers:** Z2-I-CONTENT-13, Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ExpectationsWarnAndStrictFail` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Bounds and facing report"; Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset document schema"

### Z2-R-CONTENT-046 Convention probe asset

- **Statement:** The convention probe of the design, built by the committed DCC scripts, meets every expected value of the probe table within its tolerance after import through any route with correct settings, the bitangent of every cube face included.
- **Covers:** Z2-I-CONTENT-13, Z2-I-CONTENT-10, Z2-I-CONTENT-11, Z2-I-CONTENT-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ProbeExpectationsDefaultSettings` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Convention probe asset"

### Z2-R-CONTENT-047 Per-DCC convention fixtures

- **Statement:** Every route of the DCC matrix has a recorded probe export and feature fixtures with its `RECORDING.md`, committed by a needs-human recording ticket, and each passes the probe expectations and its feature checks.
- **Covers:** Z2-I-CONTENT-13, Z2-I-CONTENT-01, Z2-I-CONTENT-04, Z2-I-CONTENT-05, Z2-I-CONTENT-06, Z2-I-CONTENT-07, Z2-I-CONTENT-08, Z2-I-CONTENT-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_DccMatrix_EveryRowHasPassingFixture` (the `Test` step on `win-debug`), blocked by the recording tickets of `Zenith2/Docs/Design/CONTENT/Tests.md` "Needs-human recordings".
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Per-DCC fixtures"; Zenith2/Docs/Design/CONTENT/Tests.md → "Needs-human recordings"

### Z2-R-CONTENT-048 Image importer

- **Statement:** The `image` importer decodes PNG (1 … 16 bits), JPEG (baseline, progressive), TGA and BMP per the image table into the texture source header's layout, usage, colour space and address hint, downscaling to `image.maxExtent` in linear space.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_MediaDecoders_ImageFormats`, `Test_ImageImporter_UsagesAndLayouts` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Image importer"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Source format tables"

### Z2-R-CONTENT-049 Image usage is declared never inferred

- **Statement:** An image whose resolved `texture.usage` is `unset` fails the import with `CONTENT_IMPORT_DETAIL_USAGE_UNSET` naming the file and the remedy; no usage is inferred from a file name or the channels at import, and no shipped preset matches every image, so an unmatched image reaches this failure.
- **Covers:** Z2-I-CONTENT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ImageImporter_UnsetUsageFailsLoudly`, `Test_ShippedPresets_NoCatchAllImagePreset` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Image importer"

### Z2-R-CONTENT-050 Audio importer decoding

- **Statement:** The `audio` importer decodes WAV (8-bit unsigned PCM offset by 128), AIFF, FLAC, Ogg Vorbis and MP3 per the audio table to float samples through the libraries' float readers with every allocation on `Zenith_Heap` (stb_vorbis's working block growing by doubling between the bounds of `Zenith_AudioDecodeOptions`, each open recorded in the optional `Zenith_AudioDecodeStats` out-parameter, and a failure at the maximum reported by the cap-failure rule: through `Zenith_CheckAudioCounts` with the doubled block at the default maximum, or as the same `LIMIT_EXCEEDED` row directly at a lowered maximum), refuses a magic that disagrees with the extension, and writes AUDIO's `decoded-audio` blob.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_MediaDecoders_AudioFormats`, `Test_MediaDecoders_VorbisWorkBlockGrows`, `Test_MediaDecoders_DecodedAudioBlobLayout`, `Test_AudioImporter_DecodedBlobAndSettings`, `Test_AudioImporter_MagicMismatchFails` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Audio importer"; Zenith2/Docs/Design/CONTENT/Importers.md → "Zenith_MediaDecoders header"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Source format tables"

### Z2-R-CONTENT-051 Audio loop points and markers

- **Statement:** Source loops (WAV `smpl` converted to an exclusive end, AIFF `INST`, Vorbis comments) and up to 64 markers are imported, explicit loop settings override them and are validated, and further markers are dropped with a warning.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_MediaDecoders_AudioLoopsAndMarkers`, `Test_AudioImporter_DecodedBlobAndSettings` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Audio importer"

### Z2-R-CONTENT-052 Audio settings keys declared

- **Statement:** The `audio` desc declares exactly AUDIO's eleven sound import settings with their types, defaults and ranges.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentImporters_RegistrationDescs` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Audio importer"

### Z2-R-CONTENT-053 Source format tables

- **Statement:** The scene, image and audio source tables state every extension the pipeline claims and every refused variant, and a refused variant fails with `CONTENT_ERROR_UNSUPPORTED` and its detail.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_MediaDecoders_ImageFormats`, `Test_MediaDecoders_AudioFormats` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Source format tables"

### Z2-R-CONTENT-054 Importer determinism

- **Statement:** Every CONTENT import is a pure function of its source bytes, read inputs, resolved settings, overrides, previous product list and importer version, writing byte-identical intermediates in process and in a worker and on every machine of one build: no result that reaches an intermediate calls a CRT transcendental, only CORE's deterministic transcendental functions (X314; ruling X314-FMA3).
- **Covers:** Z2-I-CONTENT-02, Z2-I-CONTENT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_SameInputsSameIntermediates`, `Test_ImageImporter_UsagesAndLayouts` (the colour downscale against hand-computed values) (the `Test` step on `win-debug`); review of each single-file library unit's routed transcendental list.
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "glTF importer"

### Z2-R-CONTENT-055 Parser limits

- **Statement:** Every limit of the parser limits table is one of the 39 named constants of `Zenith_ContentTypes.h`, enumerated by `Zenith_GetContentLimits`, checked by the pure functions `Zenith_CheckGltfCounts`, `Zenith_CheckImageCounts` and `Zenith_CheckAudioCounts` before any allocation it sizes, failing `LIMIT_EXCEEDED` with the row's index, and tested at the limit and at limit + 1 (limit − 1 for the minimum) through those functions ([Z2-N-ARCH-953]), with a file-level case for every limit a file of at most 70 MiB reaches first.
- **Covers:** Z2-I-CONTENT-02, Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfLimits_EachLimitAtAndAbove` (with the header-claim cases for vertices per primitive and keys per channel), `Test_GltfLimits_CountsFromSources`, `Test_MediaDecoders_ImageLimits`, `Test_MediaDecoders_AudioLimits` (with the STREAMINFO decoded-bytes case), `Test_MediaDecoders_VorbisWorkBlockGrows`, `Test_ContentLimits_TableAgreesWithConstants` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Parser limits"; Zenith2/Docs/Design/CONTENT/Importers.md → "Limit reachability"; Zenith2/Docs/Design/CONTENT/Importers.md → "Zenith_GltfSource header"

### Z2-R-CONTENT-056 Parser fuzz targets

- **Statement:** The six CONTENT fuzz targets exist under `Zenith2/Tools/ContentCore/Tests/Fuzz/`, seeded in code, and run as 60-second smokes in `win-asan` without a finding ([Z2-N-ARCH-952]).
- **Covers:** Z2-I-CONTENT-22, Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** The fuzz smoke entries of `Sanitizers -Preset win-asan`.
- **Design:** Zenith2/Docs/Design/CONTENT/Tests.md → "Fuzz targets"

### Z2-R-CONTENT-057 Third-party parsers run out of process

- **Statement:** Every CONTENT importer's desc sets `m_bOutOfProcess` true, so every import runs in a cook worker process except under `--in-process`.
- **Covers:** Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentImporters_RegistrationDescs` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Crash isolation"

### Z2-R-CONTENT-058 Fault-source cook key

- **Statement:** The cook key `cook.content.faultSource` (`crash:<path>` or `hang:<path>`) makes every CONTENT importer of that source terminate abnormally, or loop on `Zenith_SleepUs` in a worker process, at its first step, in workers through the forwarded command line, and is not a key input.
- **Covers:** Z2-I-CONTENT-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentCrash_WorkerCrashContained`, `Test_ContentCrash_InProcessCrashKillsCook` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Crash isolation"

### Z2-R-CONTENT-059 Crash containment tests

- **Statement:** A crashing CONTENT import fails only its item with `IMPORT_ERROR_WORKER_CRASHED`, every independent item publishes, and no partial intermediate or temporary remains.
- **Covers:** Z2-I-CONTENT-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentCrash_WorkerCrashContained`, `Test_ContentCrash_NoPartialIntermediate` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Crash isolation"; Zenith2/Docs/Design/CONTENT/Tests.md → "Importer tests"

### Z2-R-CONTENT-060 Hang containment tests

- **Statement:** A hanging CONTENT import fails only its item with `IMPORT_ERROR_WORKER_TIMEOUT` after `cook.workerTimeoutSeconds`, and the cook completes.
- **Covers:** Z2-I-CONTENT-22
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentCrash_WorkerHangTimesOut` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Crash isolation"

### Z2-R-CONTENT-061 Import report member

- **Statement:** Each CONTENT importer adds one element to the cook report's `contentImports` member (sorted by source GUID) with its products, bounds, counts, renames, excluded keys and warnings, and no time, host or user.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ReportMember` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Import report member"

### Z2-R-CONTENT-062 Preset document type

- **Statement:** Import presets are authoring documents `*.importpreset.json` of type `Zenith.Content.ImportPresetDocument` (schema `zenith2.import-preset` version 1) with the keys of the preset schema, registered by `Zenith_RegisterContentDocumentTypes` with their canonical schema, and joining the authoring-document type list.
- **Covers:** Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportPresetDocument_CanonicalRoundTrip` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset document schema"; Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset document type registration"

### Z2-R-CONTENT-063 Preset validation

- **Statement:** Preset documents are validated by rules P1 … P12 in the editor, in settings resolution and in tests, an error failing with `CONTENT_ERROR_PRESET_INVALID` or `IMPORT_ERROR_SETTING_INVALID` naming the key path.
- **Covers:** Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentValidation_PresetRules`, `Test_ImportPreset_ValidationRules` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset validation"

### Z2-R-CONTENT-064 Preset migration fixtures

- **Statement:** The preset type has a fixture pair per schema version under ZenithAuthoring's fixture root, `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/CONTENT/ImportPresetDocument/` (rule 2 of `Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`, row ED48), and migrates its `settings` through the importer's settings chain inside the framework's preview, backup and journaled write.
- **Covers:** Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportPresetDocument_MigrationFixtures` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset document type registration"

### Z2-R-CONTENT-065 Preset matching at meta creation

- **Statement:** When a `.meta` is created, `Zenith_ImportPresetSet::Select` chooses the preset by importer, extension, glob, priority and logical-path tie-break, its GUID and labels are written into the new `.meta`, the choice and its glob are listed for review, and the choice is never re-evaluated afterwards.
- **Covers:** Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportPresetSet_SelectRules` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset matching"

### Z2-R-CONTENT-066 Shipped engine presets

- **Statement:** The engine ships the presets of the shipped-presets table in `Zenith2/Engine/ZenithAssets/Internal/Content/ImportPresets/`, each with a committed `.meta`, each passing validation against its importer, with no image preset matching every path and each file-name suffix carrying one usage.
- **Covers:** Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ShippedPresets_ValidateAgainstImporters`, `Test_ShippedPresets_NoCatchAllImagePreset`, `Test_ContentImporters_DeclaredKeysAreGeomKeys` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Shipped presets"

### Z2-R-CONTENT-067 Preset change reimports its users

- **Statement:** Changing a preset document reimports exactly the sources whose `.meta` names it.
- **Covers:** Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportPreset_EditReimportsUsers` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset matching"

### Z2-R-CONTENT-068 Stable product keys

- **Statement:** Product keys follow the product key table and name rules (`zenithId` first and unique per kind, control-character removal, unnamed-object fallback, a cut at 200 bytes, or 100 per name in two-name keys, with a SHA-256 suffix, `#n` duplicates), depend on no open decision, and an unchanged source declares unchanged keys.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ProductKeysAndPrimary` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Product keys"

### Z2-R-CONTENT-069 Rename recognition

- **Statement:** A single disappeared and a single new key of one kind prefix (for `texture:` keys, of one role suffix `@<u>`) are a rename that moves the GUID only when their product fingerprints are equal, reported `CONTENT_WARNING_RENAME_ASSUMED` with both keys; unequal or missing fingerprints orphan the old key with `CONTENT_WARNING_RENAME_NO_EVIDENCE`, any other change set orphans the old keys with `CONTENT_WARNING_RENAME_AMBIGUOUS`, and a GUID is never re-bound to an unrelated object.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ProductFingerprints_EachKindSpecified`, `Test_RenameRecognition_OneToOneRenames`, `Test_RenameRecognition_DeletedAndAddedNeverRebound`, `Test_RenameRecognition_AmbiguousOrphansAndWarns` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Rename recognition"; Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Product fingerprints"; Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Recognition steps"

### Z2-R-CONTENT-070 Product overrides schema

- **Statement:** CONTENT's per-product overrides are the members `exclude` and `materialRemap` of the product's `.meta` sub-asset entry, beside ANIM's `annotation` and `rig` and ASSET's `fingerprint`, validated by rules O1 … O7 before use; the owner members are import-key inputs in GUID order and the fingerprint is none.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentValidation_OverrideRules` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Product overrides schema"

### Z2-R-CONTENT-071 Material remap override

- **Statement:** A mesh product's `materialRemap` replaces the slot material GUIDs it names in the mesh intermediate, and the generated material stays declared so the remap is reversible.
- **Covers:** Z2-I-CONTENT-15, Z2-I-CONTENT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_ProductOverrides_ExcludeAndRemap` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Applying product overrides"

### Z2-R-CONTENT-072 Exclude override

- **Statement:** An entry carrying `exclude: true`, live or orphaned, keeps its product undeclared on every subsequent import, is never a rename candidate and never reported dangling; an excluded mesh leaves its node entity without a mesh-instance component, and an excluded material falls back to the default material.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ProductOverrides_ExcludeAndRemap`, `Test_ProductOverrides_ExcludeStableAcrossThreeReimports` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Applying product overrides"

### Z2-R-CONTENT-073 No per-product settings layer

- **Statement:** No product carries its own importer settings and settings resolution gains no CONTENT layer: per-clip and per-rig values are ANIM's documents attached to sub-asset entries, per-product overrides are the members `exclude` and `materialRemap`, and a change to any owner member reimports exactly its source.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentImporters_RegistrationDescs` (no product-overridable key), `Test_ProductOverrides_ChangeReimportsSource` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Product overrides schema"; Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Applying product overrides"

### Z2-R-CONTENT-074 Dangling overrides kept

- **Statement:** A material remap on an orphaned entry or naming a material that is no longer live stays in the `.meta`, is not applied and is reported `CONTENT_WARNING_OVERRIDE_DANGLING`; reimport never deletes a member.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ProductOverrides_DanglingKeptAndReported` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Applying product overrides"

### Z2-R-CONTENT-075 Scene overrides survive reimport

- **Statement:** After reimports that change vertices, rename a mesh, reorder nodes, rename a `zenithId` node, and rename one of its ancestors while reparenting it, every material remap, exclusion, attached clip annotation document, scene instance override of the model prefab and variant addition still applies.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentPipeline_ReimportPreservesOverrides` in the integration suite (Stage 4 acceptance "reimport", the override layer).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Override-preserving reimport"

### Z2-R-CONTENT-076 Product members across meta migration

- **Statement:** Importer settings steps never touch the sub-asset entry members, a change to CONTENT's members is a `Zenith.AssetMeta` version step, and `provenance` migrates through its own member chain; every `exclude`, `materialRemap`, `annotation`, `rig` and `fingerprint` member survives a `.meta` migration byte for byte.
- **Covers:** Z2-I-CONTENT-15, Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ProductOverrides_SurviveMetaMigration` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Overrides across settings migration"

### Z2-R-CONTENT-077 Batch specification

- **Statement:** A batch specification (`zenith2.content-batch-spec` version 1) of one of the seven kinds is validated by rules B1 … B10, including the refusal of engine-content targets, and planning refuses by rule B12 any plan that would exceed the journal's own limits (1,048,576 expanded targets, 16,384 chunks, a 256 MiB `plan.json`, a 256 MiB prepared-file bound per chunk), so no admissible plan writes a journal its reader refuses.
- **Covers:** Z2-I-CONTENT-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatchSpec_Rules` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch specification"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Operation kinds"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Planning"

### Z2-R-CONTENT-078 Batch planning

- **Statement:** `Plan` validates what the specification alone decides, takes the operation lock and returns the operation in PLANNING; the planning units then expand targets, resolve filters with at most 32 requests in flight, index the references one document per unit, pack targets that share referencing documents into common chunks, compute the move maps and `planSha256`, and write only `plan.json` and `state.json` at PLANNED.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-19, Z2-I-CONTENT-20, Z2-I-CONTENT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_PlanWritesJournalOnly`, `Test_ContentBatch_PlanningIsStepped`, `Test_ContentBatch_ChunkPackingGroupsSharedDocuments` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Planning"

### Z2-R-CONTENT-079 Chunked execution

- **Statement:** Each chunk runs the steps of chunked execution in order (writer lock, the free-space check and one unit per prepared document writing its before-copy and its staged new bytes under the chunk's own `before/<k>/` and `after/<k>/`, the immutable prepared file, creation of missing destination directories, moves, `SaveDocuments` sub-batches in document order at each document's `writePath` from the staged bytes with expected digests, lock release, `.meta` before-states, asynchronous cook requests), writing the chunk file after each unit; a conflict at the move step undoes the chunk's moves and leaves the tree as before the chunk, and a conflict at the save step keeps the completed moves and written sub-batches and pauses, so the chunk is completed by a resume after the stale document is restored or reverted by `Abandon`; a document the chunk itself moves is read at its `readPath` and written at its `writePath`, and no unit holds more than one document's bytes and one save sub-batch.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_InterruptedAtEachStepResumes`, `Test_ContentBatch_MoveKeepsGuidsAndRepairsPaths`, `Test_ContentBatch_MovedDocumentIsAlsoRepaired`, `Test_ContentBatch_SaveConflictAfterPartialWrite` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"

### Z2-R-CONTENT-080 Operation journal

- **Statement:** The operation journal is the directory `<game root>/.migration/content-ops/<opId>/` holding the immutable `plan.json` (`zenith2.content-batch-plan` version 1), `state.json` (`zenith2.content-batch-state` version 1), per started chunk an immutable `chunk-<k>.prepared.json` (`zenith2.content-batch-chunk-prepared` version 1: moves, documents with `readPath`, `writePath`, staged paths and listed edits, warnings, cook groups, created directories) and a mutable `chunk-<k>.json` (`zenith2.content-batch-chunk` version 1: state, prepared digest and progress counts), and one immutable `meta/<k>/<n>.json` (`zenith2.content-batch-meta` version 1) per `.meta` before-state, each written by write-then-replace, so a unit rewrites a constant number of bytes plus the chunk's per-GUID cook failures and the operation's journal is O(W + targets + moves) plus a constant per unit; the files are read and written by the declared codec, whose writers refuse what its readers refuse, and refused as corrupt on any schema, digest or operation mismatch.
- **Covers:** Z2-I-CONTENT-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatchJournal_RoundTripAndCorruption` (the `Test` step on `win-debug`); `Zenith_Fuzz_BatchJournal` smoke.
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Operation journal"

### Z2-R-CONTENT-081 Resume after interruption

- **Statement:** Resume recovers framework journals first, classifies each move, each document (at its `writePath` and `readPath`), each staged copy (against its recorded digest) and each `.meta` request (by meta element classification against the server's record) of the first incomplete chunk, re-issues only what was not applied, and continues from its first incomplete step, converging on the uninterrupted result.
- **Covers:** Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_InterruptedAtEachStepResumes`, `Test_ContentBatch_RemapResumeAfterServerWrite` (the `Test` step on `win-debug`); `Test_ContentPipeline_BatchKilledAndResumed` in the integration suite.
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Resume algorithm"

### Z2-R-CONTENT-082 Conflict detection on resume

- **Statement:** A file whose digest matches neither the recorded old nor new value, or a move whose source and destination both or neither exist, stops the operation with `CONTENT_ERROR_BATCH_CONFLICT` naming every path, writing nothing; a stale document found by a save sub-batch pauses the chunk with its completed moves and written sub-batches kept and the stale document named with its current digest.
- **Covers:** Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_ConflictOnExternalEdit`, `Test_ContentBatch_SaveConflictAfterPartialWrite` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Resume algorithm"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"

### Z2-R-CONTENT-083 Abandon and rollback

- **Statement:** `Abandon` reverts only the incomplete chunk; `Rollback` reverts every chunk in reverse order from a stated starting chunk, each from its own before-copies (checked against their recorded digests before anything is written) in three ordered passes (restore the documents holding their new digest, undo the moves recreating missing source directories and removing empty created ones, verify every document holds its old digest at its final path), with the deleted files and the recorded `.meta` before-states (restoring tombstoned sub-asset GUIDs), classifying every document and every `.meta` element three ways (before, target, other) so that a subsequent edit is a conflict that is listed and kept, never overwritten, and re-issues the inverse cook requests, so every batch kind is reversible; `Discard` deletes the operation directory of a terminal operation.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_AbandonRevertsIncompleteChunk`, `Test_ContentBatch_SaveConflictAfterPartialWrite`, `Test_ContentBatch_RollbackRevertsAll`, `Test_ContentBatch_DamagedBeforeCopyRefused`, `Test_ContentBatch_RollbackRefusesLaterMetaEdit`, `Test_ContentBatch_DeleteRecoverableAndTombstoned`, `Test_ContentBatch_SettingsBatchRollsBack`, `Test_ContentBatch_RemapRollbackRestoresTombstonedGuid` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Abandon and rollback"

### Z2-R-CONTENT-084 Batch report

- **Statement:** Every pause and the end write `report.json` (`zenith2.content-batch-report` version 1) with the counts, per-target outcomes, per-document digests and edits (64 listed per document within 16 MiB per chunk), cook results, warnings (within 4 MiB per chunk), errors and removed directories, every member taken from the journal files, so the report after any interruptions equals an uninterrupted run's.
- **Covers:** Z2-I-CONTENT-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_ReportContent`, `Test_ContentBatch_InterruptedAtEachStepResumes` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch report"

### Z2-R-CONTENT-085 One batch per storage root

- **Statement:** The operation lock admits one batch operation per game root; a second is refused with `CONTENT_ERROR_BATCH_BUSY` naming the holder.
- **Covers:** Z2-I-CONTENT-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_SecondOperationRefusedBusy` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"

### Z2-R-CONTENT-086 Batch cancellation

- **Statement:** A false progress-callback return pauses the operation at the next chunk boundary with `CONTENT_ERROR_BATCH_CANCELLED`, keeping every completed chunk and finishing the current one.
- **Covers:** Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_CancelAtChunkBoundary` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"

### Z2-R-CONTENT-087 Batch state machine

- **Statement:** The operation follows every cell of the state table, the PLANNING row and the `Step` column included, any other request failing `CONTENT_ERROR_BATCH_STATE` without change.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_StateMachineEveryCell` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch state machine"

### Z2-R-CONTENT-088 Batch command line

- **Statement:** `ZenithContent batch` provides `plan`, `run`, `resume`, `status`, `abandon`, `rollback` and `discard` with the exit codes 0 … 7 of the command-line table.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17, Z2-I-CONTENT-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatchCommand_ExitCodes` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch command line"

### Z2-R-CONTENT-089 Cook channel interface

- **Statement:** A batch reaches the cook server only through the asynchronous `Zenith_ContentCookChannel` (`Begin*` for record, list, import, setSettings and dependents, a non-blocking `Poll`, `Cancel`), implemented in `ZenithEditorCore`, with at most 32 requests outstanding per operation, so `ZenithAuthoring` links no cook code and no batch call blocks on the server.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-19, Z2-I-CONTENT-20, Z2-I-CONTENT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (no ZenithCook symbol in ZenithAuthoring); `Test_ContentBatch_CookChannelFailureResumes` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Cook channel"

### Z2-R-CONTENT-090 Batch low disk and locks

- **Statement:** A free-space check before a chunk's PREPARED step (through the desc's seam, else the game storage) or the framework's low-disk refusal pauses with `CONTENT_ERROR_LOW_DISK`, and a writer lock held past `content.batch.lockWaitMs` pauses with `CONTENT_ERROR_SOURCE_LOCKED`, the chunk untouched in both cases.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_LowDiskPauses`, `Test_ContentBatch_SourceLockedPauses` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"

### Z2-R-CONTENT-091 Batch stress

- **Statement:** A 10,000-target move over 2,000 referencing documents of two test document types (`Zenith.Test.StressTarget`, `Zenith.Test.StressReferrer`), whose plan has the 10 chunks, 2,010 document repairs and 20,000 moves derived from its fixture formula, resumed after each of 270 interruptions fired by a counting injector at positions reachable by the fault-point consultation order (100 `JOURNAL_WRITTEN`, 100 `MOVE`, every `PREPARED`, `SAVE` and `DONE` hit, the 40 odd `COOK` hits), equals an uninterrupted run byte for byte in about 93 s, within the 600-second stress timeout.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** Z2-B-EDIT-004
- **Acceptance evidence:** `Test_ContentBatch_TenThousandTargetsWithRandomFaults` (`z2-stress`) (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Stress case"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Fault-point consultation order"; Zenith2/Docs/Design/CONTENT/Tests.md → "Batch tests"

### Z2-R-CONTENT-092 Move keeps identity

- **Statement:** A move or rename moves each `.meta` before its source with `MoveFileExW` without replace, creating any missing destination directory first and removing at COMPLETED each source directory its moves emptied, keeping every GUID, and its `reimport` request makes the cook record the move and re-cook only the catalogue.
- **Covers:** Z2-I-CONTENT-19, Z2-I-CONTENT-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_MoveKeepsGuidsAndRepairsPaths` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Move and rename"

### Z2-R-CONTENT-093 Move repairs path references

- **Statement:** A move rewrites SCENE's prefab-instance hints and every whole string value naming the old game-relative or logical path in every other document of the game, keeping sub-asset suffixes.
- **Covers:** Z2-I-CONTENT-19, Z2-I-CONTENT-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_MoveKeepsGuidsAndRepairsPaths`, `Test_GenericRepairer_KeepsSubAssetSuffix` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Move and rename"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Generic reference repairer"

### Z2-R-CONTENT-094 Rename of a folder

- **Statement:** A directory move becomes one target per file beneath it in path order, pairing sources with their `.meta`, and a `.meta` without its source moves alone and is reported.
- **Covers:** Z2-I-CONTENT-19, Z2-I-CONTENT-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_MoveKeepsGuidsAndRepairsPaths` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Move and rename"

### Z2-R-CONTENT-095 Delete with replacement

- **Statement:** A delete with a replacement of the same kind rewrites every reference to the deleted GUID (SCENE's planner and the generic repairer) before removing the files; a kind mismatch is refused.
- **Covers:** Z2-I-CONTENT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_DeleteWithReplacementRewritesReferences` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Delete"

### Z2-R-CONTENT-096 Delete without replacement

- **Statement:** A delete without replacement nulls references (listing every nulled pointer) and, with `unpackInstances`, unpacks prefab instances of a deleted prefab.
- **Covers:** Z2-I-CONTENT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_DeleteWithoutReplacementNullsAndUnpacks` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Delete"

### Z2-R-CONTENT-097 Deleted files recoverable and tombstoned

- **Statement:** Deleted sources and `.meta` files move into the operation's `deleted/` directory, the `import` request tombstones their GUIDs, and `Rollback` restores them byte for byte.
- **Covers:** Z2-I-CONTENT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_DeleteRecoverableAndTombstoned` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Delete"

### Z2-R-CONTENT-098 Orphaned sub-asset repair

- **Statement:** A sub-asset remap moves an orphaned GUID, with its entry's owner members, back onto its intended key and rewrites references to the discarded GUID; an orphan repair rewrites or nulls references and removes the orphaned entry with its GUID tombstoned; both journal the touched entries, so a rollback restores them until the operation is discarded.
- **Covers:** Z2-I-CONTENT-19, Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_RemapAndOrphanRepair` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Sub-asset remap and orphan repair"

### Z2-R-CONTENT-099 Generic reference repairer

- **Statement:** The generic repairer rewrites only whole JSON string values equal to the subject (known and unknown members alike) and writes the document canonically, so only edited lines change.
- **Covers:** Z2-I-CONTENT-19, Z2-I-CONTENT-20, Z2-I-CONTENT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_GenericRepairer_RewritesWholeStringValuesOnly`, `Test_GenericRepairer_ReachesUnknownMembers` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Generic reference repairer"

### Z2-R-CONTENT-100 Scene repair through SCENE's planner

- **Statement:** Scene and prefab documents are repaired only through `Zenith_PlanReferenceRepair` with the request mapping of the repairer table, and `.meta` documents only through the cook channel.
- **Covers:** Z2-I-CONTENT-19, Z2-I-CONTENT-20, Z2-I-CONTENT-21
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneRepairAdapter_MapsEachKind` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Generic reference repairer"

### Z2-R-CONTENT-101 Reimport and settings batches

- **Statement:** Reimport, setPreset and setSettings batches resolve their filter by one `list` and one `record` per listed GUID, journal each target's `.meta` before-state, send each `setSettings` with the `.meta` digest from `record`, and fail only the target whose `.meta` changed.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_ReimportAndSettingsBatches` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Reimport and settings batches"

### Z2-R-CONTENT-102 File-share store

- **Statement:** `Zenith_FileShareDerivedDataStore` implements ASSET's store interface over a share with the layout of the store layout section, created only when its marker is present with ASSET's key schema version.
- **Covers:** Z2-I-CONTENT-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDC_CreateRequiresMarker`, `Test_SharedDDC_HitCopiedToLocal` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "File-share store"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Store layout"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Store operations"

### Z2-R-CONTENT-103 Store put is create-only

- **Statement:** A `Put` writes a host- and process-named temporary, flushes and renames it without replace, treats an existing entry as success, and never replaces or partially writes an entry.
- **Covers:** Z2-I-CONTENT-23, Z2-I-CONTENT-25
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDC_PutCreateOnlyGetComplete`, `Test_ContentPipeline_SharedDDCConcurrentPuts` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Store operations"

### Z2-R-CONTENT-104 Store get is never partial

- **Statement:** A `Get` returns a complete entry, a miss, or UNAVAILABLE after any short read, error or cancellation, and `Remove` is a no-op.
- **Covers:** Z2-I-CONTENT-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDC_PutCreateOnlyGetComplete`, `Test_SharedDDC_RemoveIsNoOp` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Store operations"

### Z2-R-CONTENT-105 Store timeout watchdog

- **Statement:** The store's watchdog cancels any file call exceeding `cook.ddc.sharedTimeoutMs`, issuing each cancel within the timeout plus 50 ms under the leaf watchdog mutex `uLOCK_LEVEL_CONTENT_DDC_WATCHDOG` (950) that unregistering also takes, so no cancel reaches a subsequent call; an operation is bounded by its watched call count times that per-call bound, and a stalled call makes it return UNAVAILABLE.
- **Covers:** Z2-I-CONTENT-24
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDC_TimeoutCancelsAndFallsBack`, `Test_SharedDDC_WatchdogCancelNeverHitsNextCall` (the `Test` step on `win-debug`); the needs-human Stage 4 step "share unplugged mid-cook".
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Timeout watchdog"

### Z2-R-CONTENT-106 Unreachable share falls back

- **Statement:** An unreachable, timing-out or uninitialised share makes every item cook locally, with a cooked tree identical to a cook with the shared mode off, and the share unchanged.
- **Covers:** Z2-I-CONTENT-24
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDC_UnreachableFallsBackLocal` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Unreachable share"

### Z2-R-CONTENT-107 Corrupt shared entry falls back

- **Statement:** A corrupt shared entry is a reported miss, the item cooks locally to the clean bytes, and no client deletes or overwrites the entry.
- **Covers:** Z2-I-CONTENT-25
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDC_CorruptEntryLeftAndReported` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Corrupt entries"

### Z2-R-CONTENT-108 Stale shared entry falls back

- **Statement:** A shared entry with matching key and differing producer fields is a reported miss, the item cooks locally, and the entry is left in place.
- **Covers:** Z2-I-CONTENT-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDC_StaleEntryIsMiss` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Stale entries"

### Z2-R-CONTENT-109 Store administration

- **Statement:** `ZenithContent ddc init`, `stats`, `verify` (quarantining corrupt entries, listing stale ones) and `prune` (oldest-first by creation time to a byte bound, never younger than the minimum age) administer the share outside any cook through the declared `Zenith_FileShareStoreAdmin`.
- **Covers:** Z2-I-CONTENT-23, Z2-I-CONTENT-25, Z2-I-CONTENT-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_SharedDDCAdmin_VerifyQuarantinesCorrupt`, `Test_SharedDDCAdmin_PruneNeverExceedsAndKeepsYoung`, `Test_SharedDDCAdmin_InitRefusesNonEmpty` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Store administration"

### Z2-R-CONTENT-110 Store deployment

- **Statement:** The shared store is deployed per the deployment table: only distribution helpers, running ZenithCook built from a clean checkout at the cooked commit under the writer service account, write it; developer machines and CI read it (ACL read-only); a scheduled operator task populates it nightly (and optionally per `master` commit) through a distributed cook, a CI-run population being a PROC hand-off; default `off`; `cook.ddc.sharedRoot` never in a tracked file.
- **Covers:** Z2-I-CONTENT-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** Review of the deployment ticket against the table; `ZenithContent ddc init --help` output checked by `Test_SharedDDCAdmin_InitRefusesNonEmpty` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Deployment"

### Z2-R-CONTENT-111 Shared DDC failure-mode acceptance

- **Statement:** The Stage 4 acceptance run shows unreachable, timing-out, corrupt and stale shared stores each giving a cooked tree byte-identical to a clean local cook with the share unchanged.
- **Covers:** Z2-I-CONTENT-23, Z2-I-CONTENT-24, Z2-I-CONTENT-25, Z2-I-CONTENT-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065, Z2-D-083
- **Budgets:** Z2-B-ASSET-010
- **Acceptance evidence:** `Test_ContentPipeline_SharedDDCFailureModes` in the integration suite and the Stage 4 acceptance.
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Unreachable share"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Corrupt entries"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Stale entries"; Zenith2/Docs/Design/CONTENT/Tests.md → "Integration suite"

### Z2-R-CONTENT-112 Merge driver command

- **Statement:** `ZenithContent merge` migrates the three versions git passes to the registry's version, merges them, writes the result over ours atomically and exits 0 when clean, 1 with conflicts (an input over 64 MiB or not UTF-8 as one whole-file block, so ours is never left alone) and 2 on a bad command line, with `--explain` listing every decision.
- **Covers:** Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMergeCommand_ExitCodesAndAtomicWrite`, `Test_ContentMerge_DriverMigratesInputs` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Driver command line"; Zenith2/Docs/Design/CONTENT/Merge.md → "Canonical inputs and outputs"

### Z2-R-CONTENT-113 Structural three-way merge

- **Statement:** The structural merge applies the one-sided shortcut and the member-wise presence table to every object and map, treats scalars and atomic arrays as values, records every conflict and continues.
- **Covers:** Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge3_TableCases`, `Test_ContentMerge_DisjointEntityAddsEqualSequentialEdits` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Structural merge algorithm"

### Z2-R-CONTENT-114 Keyed and set array merges

- **Statement:** Arrays of identified objects merge by their key member with the insertion and reorder rules, and sorted scalar sets merge by the set formula without conflict.
- **Covers:** Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge3_KeyedArrays`, `Test_ContentMerge3_SetArrays` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Keyed array merge"; Zenith2/Docs/Design/CONTENT/Merge.md → "Set merge"

### Z2-R-CONTENT-115 Merge consumes canonical serialization

- **Statement:** Inputs are migrated to one version and canonicalized before comparison and the result is written with the type's canonical schema, so formatting is never a conflict and disjoint edits merge byte-identical to sequential edits; the canonical form itself is SCENE's Stage 2 delivery.
- **Covers:** Z2-I-CONTENT-28, Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge_DisjointEntityAddsEqualSequentialEdits` (the `Test` step on `win-debug`); SCENE's `Test_SceneCanonicalSchema_ClassifiesEveryGoldenMember`.
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Canonical inputs and outputs"

### Z2-R-CONTENT-116 Conflict marker format

- **Statement:** Each conflict is written as a diff3-style block with marker size L at column 0 and each side canonically serialized at its position, making the document unparsable so the editor opens it conflicted.
- **Covers:** Z2-I-CONTENT-29
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge3_MarkerFormat` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Conflict markers"

### Z2-R-CONTENT-117 Nested-prefab override conflicts

- **Statement:** A prefab swap on one side with edited overrides on the other makes the whole instance one conflict, and two different values for one override member conflict, each as an explicit marker block.
- **Covers:** Z2-I-CONTENT-29
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge_SceneRules`, `Test_ContentMerge_OverrideKindsMatchSceneTable` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Scene and prefab rules"

### Z2-R-CONTENT-118 Array-ordering conflicts

- **Statement:** Two sides reordering the common elements of a keyed array differently, and any two-sided change of an atomic array, fall back to one explicit marker block for the whole array.
- **Covers:** Z2-I-CONTENT-29
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge3_KeyedArrays` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Keyed array merge"; Zenith2/Docs/Design/CONTENT/Merge.md → "Conflict markers"

### Z2-R-CONTENT-119 Scene semantic checks

- **Statement:** A parent deleted on one side while the other keeps children under it becomes a conflict at the parent, and equal sibling sort keys are accepted without conflict.
- **Covers:** Z2-I-CONTENT-27, Z2-I-CONTENT-29
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge_SceneRules` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Scene and prefab rules"

### Z2-R-CONTENT-120 Schema merge rules

- **Statement:** Component schemas merge fields and struct fields by `id`, structs by `name`, migrations by `fromVersion` and retired IDs as a set, with retired-ID, duplicate-name and version checks turning violations into conflicts.
- **Covers:** Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge_SchemaRules` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Schema rules"

### Z2-R-CONTENT-121 Environment and world document merge rules

- **Statement:** Terrain tile documents merge by file and terrain documents structurally with their lists keyed as ENV states, an order change on both sides being a conflict; WORLD's world and generator documents merge structurally by WORLD's schemas and its two bake document types by file, and no policy names a derived navigation document, which is never a file.
- **Covers:** Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge_EnvironmentRules`, `Test_ContentMerge_PoliciesFromRegistry` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Environment document rules"; Zenith2/Docs/Design/CONTENT/Merge.md → "Merge policies"

### Z2-R-CONTENT-122 Text fallback merge

- **Statement:** A path without a policy, an unparsable or other-version input, or an input over the JSON node limits merges by diff3 over lines, computed by the linear-space form of Myers' algorithm in O(N + M) memory, with conflict blocks for two-sided hunks, bounded by an edit distance of 10,000 lines and a 30-second time limit, beyond which the file is one block.
- **Covers:** Z2-I-CONTENT-27, Z2-I-CONTENT-29
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentTextMerge_Diff3Cases`, `Test_ContentMerge_UnparsableFallsBackToText` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Text fallback"

### Z2-R-CONTENT-123 Git integration

- **Statement:** `Zenith2/.gitattributes` routes every policy suffix to `merge=zenith-document` in its marked block, `ZenithContent merge --install` sets the clone's driver configuration, and a git merge of two branches runs the driver end to end.
- **Covers:** Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge_GitAttributesListEveryPolicySuffix`, `Test_ContentMergeCommand_InstallWritesLocalConfig` (the `Test` step on `win-debug`); `Test_ContentPipeline_GitMergeDriverEndToEnd` in the integration suite.
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Git integration"

### Z2-R-CONTENT-124 Merge determinism

- **Statement:** A merge is a pure function of its inputs, policy and marker size, except that a text merge stopped by the time bound is one whole-file block reported with its reason and elapsed time; it is symmetric up to section order and idempotent, and an input over 64 MiB or not UTF-8 is one whole-file block with both sides.
- **Covers:** Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentMerge_DeterministicAndSymmetric`, `Test_ContentMerge3_OversizeAndInvalidUtf8WholeFileBlock` (the `Test` step on `win-debug`); `Zenith_Fuzz_MergeInput` smoke.
- **Design:** Zenith2/Docs/Design/CONTENT/Merge.md → "Merge determinism and limits"

### Z2-R-CONTENT-125 Distributed partitioned cook

- **Statement:** A distributed cook writes a job with n partitions of the game's source GUIDs, helpers claim partitions by create-only claim files and run `--only` cooks of their partition into the shared store, and the coordinator waits on partition states derived by `Zenith_DerivePartitionState` then optionally runs the ordinary cook.
- **Covers:** Z2-I-CONTENT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentDistribute_JobFilesAndClaims`, `Test_ContentDistribute_PartitionFilesRoundRobin` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Distributed cooking"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Partition protocol"

### Z2-R-CONTENT-126 Helper checkout and tool verification

- **Statement:** A helper cooks a partition only when its checkout's HEAD equals the job's commit, its `Zenith2/` tree is clean and it runs a ZenithCook it built from that checkout (its stamp recording the commit and executable digest), otherwise writing `treeMismatch` or `toolMismatch` and cooking nothing; the coordinator counts a done file of another commit's tool as not `ok`.
- **Covers:** Z2-I-CONTENT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentDistribute_TreeMismatchRefused`, `Test_ContentDistribute_ToolMismatchRefused` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Partition protocol"

### Z2-R-CONTENT-127 Lost helpers never block

- **Statement:** A partition whose latest claim or heartbeat is 60 s or more older than the reader's clock is stale, the coordinator stops waiting for it, and its outputs are produced by the coordinator's own cook.
- **Covers:** Z2-I-CONTENT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentDistribute_PartitionStateClock`, `Test_ContentDistribute_LostHelperDoesNotBlock` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Partition protocol"

### Z2-R-CONTENT-128 Distributed output equivalence

- **Statement:** A distributed cook with a helper killed mid-run yields a cooked tree byte-identical to a clean local cook, with shared hits for the completed partitions and no host name in any output.
- **Covers:** Z2-I-CONTENT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-065, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentPipeline_DistributedCookEqualsLocal` in the integration suite.
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Distributed cooking"

### Z2-R-CONTENT-129 Origin metadata schema

- **Statement:** The `.meta` member `provenance` follows the origin schema version 1 (origin, tool, author, source file, licence, licence text, attribution, URL, notes) validated by rules V1 … V7.
- **Covers:** Z2-I-CONTENT-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentValidation_ProvenanceRules` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Content provenance"

### Z2-R-CONTENT-130 Licence table

- **Statement:** Licence identifiers are rows of the licence table, which states per identifier whether attribution and licence text are required; non-commercial and no-derivatives licences fail `CONTENT_ERROR_LICENCE_FORBIDDEN`, unknown identifiers fail `CONTENT_ERROR_PROVENANCE_INVALID`, and whether an identifier ships is SHIP's `allowedContent` list, never this table.
- **Covers:** Z2-I-CONTENT-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentValidation_ProvenanceRules`, `Test_ContentValidation_LicenceShippableFromPolicy` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Licence table"

### Z2-R-CONTENT-131 Content licences step

- **Statement:** The GLOBAL_POST step `content.licences`, registered by `Zenith_RegisterContentCookSteps`, validates every plan origin, reports the warning `CONTENT_COOK_WARNING_LICENCE_NOT_SHIPPABLE` through `Zenith_CookContext::Report` for a valid licence outside `licence_policy.json`'s `allowedContent` (whose hash is a key input), and writes `provenance-licences.json`, sorted by GUID, listing every cooked asset whose licence is not the project's, with its licence text or SPDX URL and attribution.
- **Covers:** Z2-I-CONTENT-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentLicencesStep_WritesSortedThirdPartyList`, `Test_ContentLicencesStep_ForbiddenLicenceFailsCook`, `Test_ContentLicencesStep_ProjectContentOmitted`, `Test_ContentLicencesStep_NotShippableWarns`, `Test_ContentLicencesStep_KeyFollowsOrigins` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Content licences step"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Licence table"

### Z2-R-CONTENT-132 Storage policy check

- **Statement:** `ZenithContent storage-check` reports a binary at or above the LFS threshold outside LFS and any file whose attributes say LFS but whose blob is not a pointer (`STORAGE_NOT_LFS`), a missing LFS attribute and a game over its quota (`STORAGE_QUOTA_EXCEEDED`), over every tracked path under `Zenith2/` with `--all`, reading LFS pointers as git blobs so it needs no git-lfs, exits 0, 1, 2 or 3 as specified, and runs over the repository in the `Test` step. (Coverage basis: budget rows Z2-B-CONTENT-001 … -009, [Z2-N-BUDGET-176], [Z2-N-BUDGET-177]; `Covers` names the provenance item, the nearest inventory item, because where content sources live is part of their provenance.)
- **Covers:** Z2-I-CONTENT-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CONTENT-001, Z2-B-CONTENT-002, Z2-B-CONTENT-003, Z2-B-CONTENT-004, Z2-B-CONTENT-005, Z2-B-CONTENT-006, Z2-B-CONTENT-007, Z2-B-CONTENT-008, Z2-B-CONTENT-009
- **Acceptance evidence:** `Test_ContentStorage_DetectsUntrackedBinary`, `Test_ContentStorage_DetectsLfsAttributeWithoutPointer`, `Test_ContentStorage_QuotaExceeded`, `Test_ContentStorage_PendingRowsSkipped`, `Test_ContentStorage_ReadsPointersWithoutGitLfs` (the `Test` step on `win-debug`); `Test_ContentStorage_RepositoryWithinPolicy` in the integration suite.
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Storage policy check"

### Z2-R-CONTENT-133 Storage budget rows

- **Statement:** The Stage 1 per-system budget ticket for CONTENT writes the values of Z2-B-CONTENT-001 … -009 and the pending CONTENT rows into `Budgets.md`, `storage.config.json` and the DecisionLog in one diff. (Coverage basis: budget rows Z2-B-CONTENT-001 … -009, [Z2-N-BUDGET-176], [Z2-N-BUDGET-177]; `Covers` names the provenance item, the nearest inventory item, because where content sources live is part of their provenance.)
- **Covers:** Z2-I-CONTENT-31
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084, Z2-D-097
- **Budgets:** Z2-B-CONTENT-001, Z2-B-CONTENT-002, Z2-B-CONTENT-003, Z2-B-CONTENT-004, Z2-B-CONTENT-005, Z2-B-CONTENT-006, Z2-B-CONTENT-007, Z2-B-CONTENT-008, Z2-B-CONTENT-009
- **Acceptance evidence:** The budget ticket's diff; the coverage gate step in acceptance mode `-Stage 1` finds the rows adopted.
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Rows CONTENT owns"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Storage policy check"

### Z2-R-CONTENT-134 LFS attributes

- **Statement:** The Stage 4 storage-policy ticket's check reports every entry of the storage configuration's binary suffix list that lacks its `filter=lfs diff=lfs merge=lfs -text` rule in `Zenith2/.gitattributes` (the block Z2-R-CONTENT-151's Stage 3 ticket wrote) as `STORAGE_ATTRIBUTES_MISSING`, and a tracked file whose attributes say LFS but whose blob is not a pointer as `STORAGE_NOT_LFS`; the storage-policy ticket owns the hand-off by which a tracked binary found outside LFS is converted by a needs-human `git lfs migrate import --no-rewrite` ticket. (Coverage basis: budget rows Z2-B-CONTENT-001 … -009, [Z2-N-BUDGET-176], [Z2-N-BUDGET-177]; `Covers` names the provenance item, the nearest inventory item, because where content sources live is part of their provenance.)
- **Covers:** Z2-I-CONTENT-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CONTENT-001
- **Acceptance evidence:** `Test_ContentStorage_DetectsMissingAttributes`, `Test_ContentStorage_DetectsLfsAttributeWithoutPointer` (the `Test` step on `win-debug`); `Test_ContentStorage_RepositoryWithinPolicy`.
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Storage policy check"

### Z2-R-CONTENT-135 Log categories

- **Statement:** CONTENT logs through the six categories `content.import`, `content.batch`, `content.merge`, `content.ddc`, `content.distribute` and `content.storage` with their compiled minimums, each repeated warning once per source per run.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** Review; `Test_GltfImporter_ReportMember` checks the warning counts (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Log categories"

### Z2-R-CONTENT-136 Editor models

- **Statement:** CONTENT supplies the preset picker list, the override validator, batch progress and reports, the merge explanation and the import report member to EDIT's panels, and writes no ImGui code.
- **Covers:** Z2-I-CONTENT-14, Z2-I-CONTENT-15, Z2-I-CONTENT-18, Z2-I-CONTENT-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** EDIT's panel tests that consume the models (row CT18); `ForbiddenCheck` (no ImGui include under CONTENT's directories).
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Editor surfaces CONTENT supplies"

### Z2-R-CONTENT-137 Design rulings recorded

- **Statement:** The rulings CT-R1 … CT-R8 are appended to `Zenith2/Docs/DecisionLog.md` with consecutive free `Z2-D-NNN` IDs in one human-run diff before any implementation relying on them.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-097, Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** The DecisionLog diff of the ruling ticket; the coverage gate step finds each cited ID.
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Design rulings CONTENT records"

### Z2-R-CONTENT-138 Full-duration fuzz evidence

- **Statement:** Every CONTENT fuzz target runs for its full duration without a finding in the Stage 6 security evidence.
- **Covers:** Z2-I-CONTENT-22
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** The Stage 6 acceptance's security evidence ([Z2-N-ARCH-960]).
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Acceptance evidence"; Zenith2/Docs/Design/CONTENT/Tests.md → "Fuzz targets"

### Z2-R-CONTENT-139 Stage 3 corrupt-source evidence

- **Statement:** Corrupt and over-limit sources in the fixture game each fail with their detail while every independent item publishes, with no crash, as Stage 3 missing-or-corrupt content evidence.
- **Covers:** Z2-I-CONTENT-22, Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentPipeline_CorruptSourcesFailCleanly` in the integration suite and the Stage 3 acceptance.
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Acceptance evidence"; Zenith2/Docs/Design/CONTENT/Tests.md → "Integration suite"

### Z2-R-CONTENT-140 Glb container and buffers

- **Statement:** The glTF parser validates the GLB header, chunk order and lengths, decodes `data:` URIs within their limit, and reads external buffers only through the import context with `..`, absolute and remote URIs refused.
- **Covers:** Z2-I-CONTENT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfSource_GlbChunkRules`, `Test_GltfSource_ExternalBuffersThroughCallback` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "ZenithContentCore API"

### Z2-R-CONTENT-141 Tangent frames under conversion

- **Statement:** Every probe cube vertex's reconstructed bitangent w · (n × t) equals its face's row of the hand-written bitangent table after import at identity, under an axis conversion and under a mirror conversion, so the w × det Q rule is proven against an independent oracle.
- **Covers:** Z2-I-CONTENT-10, Z2-I-CONTENT-11, Z2-I-CONTENT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ProbeTangentFrameUnderConversion` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Convention probe asset"; Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Basis change algorithm"

### Z2-R-CONTENT-142 Product fingerprints

- **Statement:** Every declared product carries a fingerprint, the first 8 bytes of SHA-256 over its kind's tag and identity description, recorded with `SetProductFingerprint` and returned by `GetPreviousProducts`, never an input of any key.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ProductFingerprints_EachKindSpecified`, `Test_ProductOverrides_ChangeReimportsSource` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Product fingerprints"

### Z2-R-CONTENT-143 Batch time-budgeted stepping

- **Statement:** `Zenith_ContentBatchOperation::Step` runs units of planning and of chunked execution only while the elapsed time plus each unit's estimate fits its budget, runs a unit of a not yet measured kind only first in a call, never splits a unit, never blocks on the cook channel, and makes progress on every call without a request in flight.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_StepNeverExceedsBudget` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch API"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"

### Z2-R-CONTENT-144 Recoverable meta changes

- **Statement:** Before its first `.meta` request a target's `record` digest and changed members are journaled, `setSettings` carries that digest, the answer is journaled, and resume, abandon and rollback classify each request from the server's record by one three-way rule (before, target, other), so no `.meta` change of a batch is lost, applied twice, or reverted over a subsequent edit.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17, Z2-I-CONTENT-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_RemapResumeAfterServerWrite`, `Test_ContentBatch_SettingsBatchRollsBack`, `Test_ContentBatch_RollbackRefusesLaterMetaEdit`, `Test_ContentBatch_InterruptedAtEachStepResumes` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Operation journal"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Resume algorithm"

### Z2-R-CONTENT-145 Every supported route recorded

- **Statement:** Every DCC route whose matrix status is supported has a `dcc_matrix.json` row with a convention probe fixture and a fixture proving every feature cell that is not `none`, checked by an acceptance-only case that fails while any supported route is unrecorded.
- **Covers:** Z2-I-CONTENT-01, Z2-I-CONTENT-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_DccMatrix_EverySupportedRouteRecorded` (suite `z2-acceptance-content`, run by the Stage 4 acceptance).
- **Design:** Zenith2/Docs/Design/CONTENT/DccMatrix.md → "Per-DCC fixtures"

### Z2-R-CONTENT-146 Watchdog lock row

- **Statement:** The file-share store's watchdog mutex is constructed with the lock row `uLOCK_LEVEL_CONTENT_DDC_WATCHDOG` (level 950, leaf, short hold) of the lock ordering table, and its holder acquires no other lock, logs, allocates or performs file I/O.
- **Covers:** Z2-I-CONTENT-23, Z2-I-CONTENT-24
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** The lock-order checker under `Sanitizers -Preset win-asan`; `Test_SharedDDC_WatchdogCancelNeverHitsNextCall` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT.md → "Lock-order entries"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Timeout watchdog"

### Z2-R-CONTENT-147 Declared tool-side APIs and clock seam

- **Statement:** Every type and function the CONTENT design names is declared with its contract block (the store's class, statistics and administration, the journal codec and its three views, the licence lookup, the limit table and check functions, the audio decode options, the chunk packing function, the partition-state function, the storage check, the conversion's private layout and the cook-step registration), and every time-dependent rule reads `Zenith_ContentClock`, so tests drive time with a scripted clock.
- **Covers:** Z2-I-CONTENT-23, Z2-I-CONTENT-30, Z2-I-CONTENT-31
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` on each creating ticket; `Test_ContentDistribute_PartitionStateClock`, `Test_SharedDDCAdmin_PruneNeverExceedsAndKeepsYoung` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Store administration"; Zenith2/Docs/Design/CONTENT/SharedCache.md → "Partition protocol"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch API"

### Z2-R-CONTENT-148 Escaped product key names

- **Statement:** Every name in a product key has the four reserved bytes `%`, `/`, `#` and `@` percent-encoded before it is cut and de-duplicated, and the glTF default material and the combined mesh key as `material:@default` and `mesh:<stem>@combined`, so a literal name never equals a generated suffix (`#<n>`, `@mirrored`, `@combined`, `@default`, `@<u>`, `/orm`) and product keys are unique by construction within a source.
- **Covers:** Z2-I-CONTENT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_GltfImporter_ProductKeysAndPrimary` (its escaped-name cases; the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/Importers.md → "Product keys"

### Z2-R-CONTENT-149 Batch owner thread and stepped planning

- **Statement:** A batch operation's owner thread is whichever thread called `Plan` or `Open`, never a job worker; `Plan` returns in PLANNING without reading a document or sending a request, planning runs in non-blocking units of `Step` or `Run`, `Run` polls the channel with a 1 ms sleep and never busy-waits, and the editor runs `Plan` and `Run` on its `EditorContentBatch` thread.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_PlanningIsStepped`, `Test_ContentBatch_PlanWritesJournalOnly`, `Test_ContentBatch_StateMachineEveryCell` (the `Test` step on `win-debug`); EDIT's `Test_EditorAssetOps_LargeBatchUsesBatchOperation` (row CT30).
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Batch API"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Planning"; Zenith2/Docs/Design/CONTENT.md → "Thread affinity per type"

### Z2-R-CONTENT-150 Bounded batch work

- **Statement:** Planning packs targets that share referencing documents into common chunks (up to 16 times the chunk size per component part), so a document is repaired once per chunk holding one of its targets; a plan whose document repairs exceed 1,000,000, or a target referenced by more than 65,536 documents, is refused with the chunk size that would fit (rule B11); and a unit rewrites a constant number of journal bytes plus its chunk's per-GUID cook failures, everything that grows with the chunk being written once.
- **Covers:** Z2-I-CONTENT-16, Z2-I-CONTENT-19, Z2-I-CONTENT-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ContentBatch_ChunkPackingGroupsSharedDocuments`, `Test_ContentBatch_TenThousandTargetsWithRandomFaults` (`z2-stress`) (the `Test` step on `win-debug`); the pending budget row "Batch reference repair time per 1,000 documents".
- **Design:** Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Planning"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Operation journal"; Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Chunked execution"

### Z2-R-CONTENT-151 LFS attributes before recorded binaries

- **Statement:** `Zenith2/.gitattributes` carries `*<ext> filter=lfs diff=lfs merge=lfs -text` for every v1 `binaryExtensions` suffix, in the `# BEGIN zenith-content lfs` block written by its own Stage 3 ticket, before any recorded binary is committed: that ticket `blocks` every needs-human recording ticket, and a recorded-fixture test that reads a git-lfs pointer fails at once with a message naming the path and the setup step.
- **Covers:** Z2-I-CONTENT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the Stage 3 ticket's diff, whose block equals the 32-line golden derived from the v1 list, and its `blocks` links to the recording tickets; `Test_MediaDecoders_RecordedFixturePointerFailsClearly` (the `Test` step on `win-debug`).
- **Design:** Zenith2/Docs/Design/CONTENT/SharedCache.md → "Storage policy check"; Zenith2/Docs/Design/CONTENT.md → "Stage 3"; Zenith2/Docs/Design/CONTENT/Tests.md → "Needs-human recordings"

## Coverage of inventory items

| Item | Text | Classification | Requirements |
|---|---|---|---|
| Z2-I-CONTENT-01 | a supported-DCC feature matrix | required | Z2-R-CONTENT-001, Z2-R-CONTENT-002, Z2-R-CONTENT-005, Z2-R-CONTENT-006, Z2-R-CONTENT-008, Z2-R-CONTENT-017, Z2-R-CONTENT-033, Z2-R-CONTENT-044, Z2-R-CONTENT-047, Z2-R-CONTENT-048, Z2-R-CONTENT-050, Z2-R-CONTENT-051, Z2-R-CONTENT-052, Z2-R-CONTENT-053, Z2-R-CONTENT-061, Z2-R-CONTENT-135, Z2-R-CONTENT-137, Z2-R-CONTENT-145, Z2-R-CONTENT-151 |
| Z2-I-CONTENT-02 | DCC matrix: glTF 2.0 extensions | required | Z2-R-CONTENT-001, Z2-R-CONTENT-007, Z2-R-CONTENT-009, Z2-R-CONTENT-010, Z2-R-CONTENT-011, Z2-R-CONTENT-012, Z2-R-CONTENT-013, Z2-R-CONTENT-014, Z2-R-CONTENT-026, Z2-R-CONTENT-028, Z2-R-CONTENT-034, Z2-R-CONTENT-035, Z2-R-CONTENT-040, Z2-R-CONTENT-041, Z2-R-CONTENT-042, Z2-R-CONTENT-054, Z2-R-CONTENT-055, Z2-R-CONTENT-056, Z2-R-CONTENT-139, Z2-R-CONTENT-140 |
| Z2-I-CONTENT-03 | DCC matrix: FBX via a conversion path if selected | required | Z2-R-CONTENT-015, Z2-R-CONTENT-016, Z2-R-CONTENT-017 |
| Z2-I-CONTENT-04 | skins | required | Z2-R-CONTENT-018, Z2-R-CONTENT-019, Z2-R-CONTENT-035, Z2-R-CONTENT-047 |
| Z2-I-CONTENT-05 | morphs | required | Z2-R-CONTENT-020, Z2-R-CONTENT-047 |
| Z2-I-CONTENT-06 | animation | required | Z2-R-CONTENT-021, Z2-R-CONTENT-022, Z2-R-CONTENT-047 |
| Z2-I-CONTENT-07 | materials | required | Z2-R-CONTENT-013, Z2-R-CONTENT-014, Z2-R-CONTENT-023, Z2-R-CONTENT-024, Z2-R-CONTENT-025, Z2-R-CONTENT-026, Z2-R-CONTENT-027, Z2-R-CONTENT-028, Z2-R-CONTENT-034, Z2-R-CONTENT-047, Z2-R-CONTENT-048, Z2-R-CONTENT-049, Z2-R-CONTENT-071 |
| Z2-I-CONTENT-08 | cameras | required | Z2-R-CONTENT-029, Z2-R-CONTENT-031, Z2-R-CONTENT-047 |
| Z2-I-CONTENT-09 | lights | required | Z2-R-CONTENT-030, Z2-R-CONTENT-031, Z2-R-CONTENT-047 |
| Z2-I-CONTENT-10 | axis conversion on import | required | Z2-R-CONTENT-036, Z2-R-CONTENT-037, Z2-R-CONTENT-043, Z2-R-CONTENT-046, Z2-R-CONTENT-141 |
| Z2-I-CONTENT-11 | handedness conversion on import | required | Z2-R-CONTENT-036, Z2-R-CONTENT-038, Z2-R-CONTENT-046, Z2-R-CONTENT-141 |
| Z2-I-CONTENT-12 | unit conversion on import | required | Z2-R-CONTENT-036, Z2-R-CONTENT-039, Z2-R-CONTENT-046 |
| Z2-I-CONTENT-13 | conventions tested per DCC | required | Z2-R-CONTENT-043, Z2-R-CONTENT-045, Z2-R-CONTENT-046, Z2-R-CONTENT-047, Z2-R-CONTENT-141, Z2-R-CONTENT-145 |
| Z2-I-CONTENT-14 | import presets | required | Z2-R-CONTENT-003, Z2-R-CONTENT-045, Z2-R-CONTENT-062, Z2-R-CONTENT-063, Z2-R-CONTENT-064, Z2-R-CONTENT-065, Z2-R-CONTENT-066, Z2-R-CONTENT-067, Z2-R-CONTENT-076, Z2-R-CONTENT-101, Z2-R-CONTENT-136, Z2-R-CONTENT-144 |
| Z2-I-CONTENT-15 | reimport that preserves authored overrides | required | Z2-R-CONTENT-022, Z2-R-CONTENT-031, Z2-R-CONTENT-032, Z2-R-CONTENT-054, Z2-R-CONTENT-068, Z2-R-CONTENT-069, Z2-R-CONTENT-070, Z2-R-CONTENT-071, Z2-R-CONTENT-072, Z2-R-CONTENT-073, Z2-R-CONTENT-074, Z2-R-CONTENT-075, Z2-R-CONTENT-076, Z2-R-CONTENT-098, Z2-R-CONTENT-136, Z2-R-CONTENT-142, Z2-R-CONTENT-148 |
| Z2-I-CONTENT-16 | batch operations: journaled | required | Z2-R-CONTENT-003, Z2-R-CONTENT-004, Z2-R-CONTENT-077, Z2-R-CONTENT-078, Z2-R-CONTENT-079, Z2-R-CONTENT-080, Z2-R-CONTENT-083, Z2-R-CONTENT-085, Z2-R-CONTENT-087, Z2-R-CONTENT-088, Z2-R-CONTENT-089, Z2-R-CONTENT-090, Z2-R-CONTENT-091, Z2-R-CONTENT-101, Z2-R-CONTENT-143, Z2-R-CONTENT-144, Z2-R-CONTENT-149, Z2-R-CONTENT-150 |
| Z2-I-CONTENT-17 | batch operations: resumable | required | Z2-R-CONTENT-079, Z2-R-CONTENT-081, Z2-R-CONTENT-082, Z2-R-CONTENT-083, Z2-R-CONTENT-086, Z2-R-CONTENT-087, Z2-R-CONTENT-088, Z2-R-CONTENT-090, Z2-R-CONTENT-091, Z2-R-CONTENT-143, Z2-R-CONTENT-144, Z2-R-CONTENT-149 |
| Z2-I-CONTENT-18 | batch operations: with a report | required | Z2-R-CONTENT-084, Z2-R-CONTENT-088, Z2-R-CONTENT-136 |
| Z2-I-CONTENT-19 | rename with reference repair | required | Z2-R-CONTENT-078, Z2-R-CONTENT-089, Z2-R-CONTENT-092, Z2-R-CONTENT-093, Z2-R-CONTENT-094, Z2-R-CONTENT-098, Z2-R-CONTENT-099, Z2-R-CONTENT-100, Z2-R-CONTENT-150 |
| Z2-I-CONTENT-20 | move with reference repair | required | Z2-R-CONTENT-078, Z2-R-CONTENT-089, Z2-R-CONTENT-092, Z2-R-CONTENT-093, Z2-R-CONTENT-094, Z2-R-CONTENT-099, Z2-R-CONTENT-100, Z2-R-CONTENT-150 |
| Z2-I-CONTENT-21 | delete with reference repair | required | Z2-R-CONTENT-078, Z2-R-CONTENT-089, Z2-R-CONTENT-095, Z2-R-CONTENT-096, Z2-R-CONTENT-097, Z2-R-CONTENT-099, Z2-R-CONTENT-100 |
| Z2-I-CONTENT-22 | importer crash isolation | required | Z2-R-CONTENT-002, Z2-R-CONTENT-005, Z2-R-CONTENT-006, Z2-R-CONTENT-007, Z2-R-CONTENT-055, Z2-R-CONTENT-056, Z2-R-CONTENT-057, Z2-R-CONTENT-058, Z2-R-CONTENT-059, Z2-R-CONTENT-060, Z2-R-CONTENT-135, Z2-R-CONTENT-137, Z2-R-CONTENT-138, Z2-R-CONTENT-139 |
| Z2-I-CONTENT-23 | shared DDC operation | required | Z2-R-CONTENT-004, Z2-R-CONTENT-102, Z2-R-CONTENT-103, Z2-R-CONTENT-104, Z2-R-CONTENT-109, Z2-R-CONTENT-110, Z2-R-CONTENT-111, Z2-R-CONTENT-146, Z2-R-CONTENT-147 |
| Z2-I-CONTENT-24 | shared DDC: unreachable | required | Z2-R-CONTENT-105, Z2-R-CONTENT-106, Z2-R-CONTENT-111, Z2-R-CONTENT-146 |
| Z2-I-CONTENT-25 | shared DDC: corrupt | required | Z2-R-CONTENT-103, Z2-R-CONTENT-107, Z2-R-CONTENT-109, Z2-R-CONTENT-111 |
| Z2-I-CONTENT-26 | shared DDC: stale | required | Z2-R-CONTENT-108, Z2-R-CONTENT-109, Z2-R-CONTENT-111 |
| Z2-I-CONTENT-27 | semantic merges: structure-aware 3-way merge driver | required | Z2-R-CONTENT-003, Z2-R-CONTENT-004, Z2-R-CONTENT-112, Z2-R-CONTENT-113, Z2-R-CONTENT-114, Z2-R-CONTENT-115, Z2-R-CONTENT-119, Z2-R-CONTENT-120, Z2-R-CONTENT-121, Z2-R-CONTENT-122, Z2-R-CONTENT-123, Z2-R-CONTENT-124, Z2-R-CONTENT-136 |
| Z2-I-CONTENT-28 | semantic merges depend on canonical serialization | required | Z2-R-CONTENT-115 (the canonical serialization itself is SCENE's Stage 2 delivery: Z2-R-SCENE-002, -015 … -022, -139, -141, -143 … -145; CONTENT's requirement is its use) |
| Z2-I-CONTENT-29 | semantic merges: textual-conflict marker fallback | required | Z2-R-CONTENT-116, Z2-R-CONTENT-117, Z2-R-CONTENT-118, Z2-R-CONTENT-119, Z2-R-CONTENT-122 |
| Z2-I-CONTENT-30 | distributed cooking | required | Z2-R-CONTENT-004, Z2-R-CONTENT-125, Z2-R-CONTENT-126, Z2-R-CONTENT-127, Z2-R-CONTENT-128, Z2-R-CONTENT-147 |
| Z2-I-CONTENT-31 | content provenance | required | Z2-R-CONTENT-129, Z2-R-CONTENT-130, Z2-R-CONTENT-131, Z2-R-CONTENT-132, Z2-R-CONTENT-133, Z2-R-CONTENT-134, Z2-R-CONTENT-147 |

## Excluded and deferred

| Item or part | Classification | Rationale | Decision |
|---|---|---|---|
| `KHR_materials_clearcoat` and `KHR_materials_sheen` mapping (part of Z2-I-CONTENT-07) | deferred to Stage 5 (Z2-R-CONTENT-027) | the target material types are GEOM's Stage 5 surface models | Z2-D-043, Z2-D-079 |
| FBX import (Z2-I-CONTENT-03) | required, with two specified outcomes | whether a conversion path exists is D-SEL-FBX; Z2-R-CONTENT-015 and -016 specify both outcomes, and every consumer is blocked by the ruling ticket | D-SEL-FBX (open, Stage 4), Z2-D-084 |
| the glTF extensions marked ignored or refused in the extension table | excluded from import | no engine material or decoder carries them; the table records each with its behaviour | Z2-D-079, Z2-D-082 |
| any Luau surface for content production | excluded | CONTENT is tool-side only | Z2-D-016 |
| third-party mod content | excluded | modding is excluded | Z2-D-075 |
