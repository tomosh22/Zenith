# TWOD requirement catalogue

## Purpose

This catalogue breaks the TWOD inventory items (`Zenith2/Docs/Requirements/Inventory.md`, items Z2-I-TWOD-01 to Z2-I-TWOD-10) into testable requirements `Z2-R-TWOD-NNN`. Each names the inventory items it covers, the stage whose acceptance it serves, its classification, its governing decisions and budget rows, the artefact that proves it and the design headings that specify it (`Zenith2/Docs/Design/TWOD.md` and its section files). Pending budget rows are named by their proposed IDs without the `Z2-B-` prefix until the row index holds them.

## Requirements

### Z2-R-TWOD-001 Module library target

- **Statement:** `Zenith2D` is a `KIND MODULE` static library with `MODULE_NAME 2D` in `Zenith2/Engine/Zenith2D/`, declares `SUPPORTED ZENITH_GPU` and exactly the `DEPENDS` list of "Libraries Zenith2D depends on", and contains no platform directory.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Libgraph`, `SourceLayout` and `CompileCoverage` on the ticket that lands the library.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Libraries Zenith2D depends on"; Zenith2/Docs/Design/TWOD.md → "Target summary"; Zenith2/Docs/Design/TWOD.md → "Zenith2D files"

### Z2-R-TWOD-002 Module entry points

- **Statement:** `Zenith_Module_2D.h` declares exactly `Zenith_Module_2D_RegisterTypes` and `Zenith_Module_2D_Start`; RegisterTypes registers the nine components, seven events, five payloads, three script value structs, the element struct `Zenith.TwoD.AnimatorParameter2D`, three world resources and the configuration keys, in that order, each type with its explicit script name.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ComponentsAndProperties`, `Test_TwoDRegistration_EventsAndPayloads` (`Zenith2D_Tests`, `Test`); `ApiContractCheck`.
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Module_2D header"; Zenith2/Docs/Design/TWOD.md → "Component table"; Zenith2/Docs/Design/TWOD.md → "World resources events and commands"

### Z2-R-TWOD-003 Start sequence unwinds

- **Statement:** Each module start step (1 to 10 and 2a) and each world attach step (0 to 8) has a failure-injection value, and a failure at any one leaves no allocation, registration, extension or node behind.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDModule_StartUnwindsOnFailure` (`Zenith2D_Tests`, `Test`, `Sanitizers -Preset win-asan`).
- **Design:** Zenith2/Docs/Design/TWOD.md → "Module start sequence"; Zenith2/Docs/Design/TWOD.md → "World attach and detach"

### Z2-R-TWOD-004 Public headers carry contracts

- **Statement:** Every public type of the eleven Zenith2D headers and of `Flux_Sprite2D.h` carries one contract block, every fallible public function returns `Zenith_Result`, and no STL, third-party or `Internal/` header reaches a public header.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` on every header-landing ticket.
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Scope of this file"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Header list"

### Z2-R-TWOD-005 API-only slices land first

- **Statement:** The Stage 2 API slice (`Zenith_Module_2D.h`, `Zenith_TwoDTypes.h`, `Zenith_Physics2D.h`, `Flux_Sprite2D.h` with no-pass feature functions) and the Stage 3 slice (`Zenith_Sprite2DAnimation.h`, `Zenith_Tilemap.h`) land with stub definitions before the implementations that consume them.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Build` and `ApiContractCheck` green on the slice tickets with no implementation file.
- **Design:** Zenith2/Docs/Design/TWOD.md → "API-only slices"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Header list"

### Z2-R-TWOD-006 Error codes are stable

- **Statement:** `Zenith_TwoDErrorCode` (16 codes), `Zenith_Physics2DErrorCode` (10) and `Flux_Sprite2DError` (3) keep their declared numeric values forever and `Zenith_ToString` names each.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDTypes_ErrorCodesStable` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD.md → "Error enumerations"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"

### Z2-R-TWOD-007 Configuration keys registered

- **Statement:** Every key of "Configuration keys" registers with its type, default, range and simulation or presentation class; simulation keys are read at world attach and never change during the world's life.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ConfigKeys` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD.md → "Configuration keys"

### Z2-R-TWOD-008 Sprite component

- **Statement:** `Zenith.TwoD.Sprite` registers its 13 properties with the declared IDs, defaults and ranges, `PHASE_LATE_SCRIPTS` authority, and round-trips through scene serialization.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ComponentsAndProperties` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Sprite2DComponents header"; Zenith2/Docs/Design/TWOD.md → "Component table"

### Z2-R-TWOD-009 Sort groups

- **Statement:** An entity with `Zenith.TwoD.SortGroup` sorts its descendants' sprites as one unit at the group's layer and order, with descendants ordered among themselves by their own keys.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSort_KeyFields` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Sprite2DComponents header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"

### Z2-R-TWOD-010 Sorting layers and y-sort

- **Statement:** Sorting layers are the names of `twod.sortingLayers` (at most 32); a sprite on a layer listed in `twod.ySortLayers` orders by its y key within its order, lower y in front; a layer index at or above the array's length draws in the last configured layer with `twod.render.bad_layer_delta`.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSort_KeyFields` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"; Zenith2/Docs/Design/TWOD.md → "Configuration keys"

### Z2-R-TWOD-011 Draw modes

- **Statement:** SIMPLE draws one quad of the frame's size, SLICED nine quads with unscaled corners stretched to `Size`, TILED repeats the frame to fill `Size` with at most `uTWOD_MAX_TILED_QUADS` quads, the excess counted `twod.extract.tiled_clipped_delta`.
- **Covers:** Z2-I-TWOD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DExtract_RecordsMatchComponents` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"

### Z2-R-TWOD-012 Extraction writes render-world records

- **Statement:** `TwoD.Extract` writes, per world and frame, one sprite list of 72-byte records carrying the interpolated world transform, frame rectangle, pivot, tint, flips, layer, order, blend, space and snap flag of every visible sprite.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-TWOD-001
- **Acceptance evidence:** `Test_Sprite2DExtract_RecordsMatchComponents`, `Test_Sprite2DRecord_Layout` (`Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Flux_Sprite2D header"

### Z2-R-TWOD-013 Extraction is all-or-nothing per slot

- **Statement:** A sprite list, chunk record or tileset record that does not fit the partition's slot is refused whole with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` and `twod.extract.slot_full_delta`; no partial list is drawn.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DExtract_SlotFullDropsWholeList` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"; Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour"

### Z2-R-TWOD-014 Sort keys

- **Statement:** Every sprite sorts by the 64-bit key (sorting layer, order, y key, sub-order) with the 64-bit simulation ID as the final tie-break, through a stable LSD radix sort, so the order is total.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-TWOD-001
- **Acceptance evidence:** `Test_Sprite2DSort_KeyFields` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"

### Z2-R-TWOD-015 Sort independent of layout

- **Statement:** The sorted order is identical for identical content regardless of chunk capacity, archetype layout or worker count.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSort_IndependentOfChunkLayout` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"; Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"

### Z2-R-TWOD-016 Batching breaks only on program and blend

- **Statement:** Consecutive sorted sprites share one instanced draw while their program and blend are equal; a texture change never splits a batch because the page is a bindless index in the instance.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** Z2-B-TWOD-001
- **Acceptance evidence:** `Test_Sprite2DBatch_TextureChangeKeepsBatch`, `Test_Sprite2DBatch_BlendChangeSplits` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Batching algorithm"

### Z2-R-TWOD-017 Frame data through the upload ring

- **Statement:** `PrepareFrame` declares the frame's instance, page-table and scatter bytes and writes nothing; `WriteFrame` writes them at the reserved offsets; a deferred frame writes no 2D bytes and draws no 2D content.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRenderer_WritesAfterReservation` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path"

### Z2-R-TWOD-018 Sprite passes in the render graph

- **Statement:** 2D content is drawn by the passes Sprite2DScatter, Sprite2DCanvas, Sprite2DComposite and Sprite2DWorld declared through the render graph by the two view extensions, with no barrier code and one push constant per draw.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-056, Z2-D-054
- **Budgets:** the pending row proposed as TWOD-002 (TWOD.md "Pending row needs"), the pending row proposed as TWOD-003 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Sprite2DRender_SortOrderVisible` (`Zenith_Integration_TwoD`, `Test`, WARP and lavapipe); `ShaderChecks`.
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DCanvas pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DWorld pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds"

### Z2-R-TWOD-019 Stream-sets

- **Statement:** The canvas, world and tile-chunk instance layouts are the SPRITE stream-set family 0x3001–0x3003 (40, 48, 36 bytes, stream 4, locations 8–13), matching the Slang declarations.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_SpriteStreamSet_LayoutsMatchTable` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite stream-sets"

### Z2-R-TWOD-020 Sprite cap

- **Statement:** The instances of all lists of a frame never exceed `twod.render.maxSpritesPerFrame`; lists over the cap are dropped whole from the highest world tag down with `flux.twod.sprites_dropped_delta`.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRenderer_WritesAfterReservation` cap case (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path"; Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour"

### Z2-R-TWOD-021 Blend modes

- **Statement:** Alpha, premultiplied, additive and multiply blends compose in linear light as their formulas state.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_BlendModes` (`Zenith_Integration_TwoD`, `Test`, WARP and lavapipe).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Colour and HDR output"

### Z2-R-TWOD-022 World-space sprites

- **Statement:** Sprites with `Space` WORLD draw in the TRANSPARENT extension point of MAIN, EDITOR and TEXTURE views, depth-tested and not depth-written, in sort order, unlit.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_WorldSpaceDepthTest` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DWorld pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds"

### Z2-R-TWOD-023 Headless adds no pass

- **Statement:** With no renderer the module registers no extension and calls no `Enable2D`; extraction still runs and the simulation is unchanged.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_HeadlessAddsNoPass` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Headless and device loss"; Zenith2/Docs/Design/TWOD.md → "Module start sequence"

### Z2-R-TWOD-024 Device loss recovery

- **Statement:** After a device loss the GPU chunk pool and tile table are rebuilt from the renderer's CPU mirrors, and the image equals the pre-loss image within ⌈wanted chunks / `twod.render.maxChunkUploadsPerFrame`⌉ frames (the first recovered frame when the wanted chunks fit one frame's uploads).
- **Covers:** Z2-I-TWOD-02, Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_DeviceLossRecovers` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Headless and device loss"

### Z2-R-TWOD-025 Missing page fallback

- **Statement:** A page texture that is not resident, failed or null draws the white usage fallback for that frame with `flux.twod.missing_page_delta`; more than 4,096 pages in a frame draw the excess as fallback.
- **Covers:** Z2-I-TWOD-02, Z2-I-TWOD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_PixelExactCanvas` fallback case (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour"

### Z2-R-TWOD-026 Atlas document schema

- **Statement:** `*.spriteatlas.json` follows its schema (filter, pixels per unit, page size, padding, extrude, trim, sources by relative image path with grid slicing, pivots, nine-slice borders) and is written in canonical form.
- **Covers:** Z2-I-TWOD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDDocuments_CanonicalRoundTrip`, `Test_TwoDValidation_EveryRule` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Sprite atlas document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Common rules"

### Z2-R-TWOD-027 Atlas importer

- **Statement:** The importer `twod.spriteAtlas` reads each source image through `ReadInput`, refuses JPEG, declares the atlas product and one texture product per page, and writes each page's `geom.textureSource` and the atlas's `twod.atlasSections` intermediate.
- **Covers:** Z2-I-TWOD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCook_AtlasImportAndCook` (`ZenithCook_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"

### Z2-R-TWOD-028 Deterministic packing

- **Statement:** `Zenith_PackSpriteAtlas` produces byte-identical pages and sections for identical frames regardless of document order, with no overlap of padded rectangles and no rotation.
- **Covers:** Z2-I-TWOD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SpriteAtlasPacker_Deterministic`, `Test_SpriteAtlasPacker_NoOverlapAndPadding` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing"

### Z2-R-TWOD-029 Trim and extrude

- **Statement:** Trimming keeps every frame's drawn position and pivot; extrusion repeats edge texels outward by `extrude` texels.
- **Covers:** Z2-I-TWOD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_SpriteAtlasPacker_TrimKeepsPivot`, `Test_SpriteAtlasPacker_ExtrudeReplicatesEdges` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing"

### Z2-R-TWOD-030 Point atlases are exact

- **Statement:** A `point` atlas cooks its pages with the usage `FLUX_TEXTURE_USAGE_COLOUR_EXACT`: one mip, no block compression, texels reaching the GPU as `R8G8B8A8_SRGB` bit for bit.
- **Covers:** Z2-I-TWOD-03, Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCook_AtlasImportAndCook` (`ZenithCook_Tests`, `Test`); `Test_Sprite2DRender_PixelExactCanvas`.
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"; Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"

### Z2-R-TWOD-031 Cooked atlas format

- **Statement:** `.zatlas` files use ASSET's container with magic `ZATL` and the sections INFO, FRAMES, NAME_INDEX, STRINGS and DEPENDENCIES as specified, little-endian, written field by field.
- **Covers:** Z2-I-TWOD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCompile_MatchesTestWriter` (`ZenithAuthoring_Tests`), `Test_TwoDFormats_ReaderChecks` (`Zenith2D_Tests`), `Test`.
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Cooked sprite atlas format"; Zenith2/Docs/Design/TWOD/Formats.md → "Common rules"

### Z2-R-TWOD-032 Atlas frame lookup

- **Statement:** `Zenith_SpriteAtlasAsset` finds a frame by `Zenith_StringID` in O(log n) and reports absence without asserting; a sprite naming an absent frame draws nothing with `twod.render.missing_sprite_delta`.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDLoaders_AtlasNameLookup` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_SpriteAtlas header"; Zenith2/Docs/Design/TWOD/Formats.md → "Common rules"

### Z2-R-TWOD-033 Atlas cook step and keys

- **Statement:** The `twod.spriteAtlas` step keys on the `twod.atlasSections` digest, writes `.zatlas`, and re-cooks exactly when an image or the document changes.
- **Covers:** Z2-I-TWOD-03, Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCook_Deterministic`, `Test_TwoDCook_ImageEditRecooksDependents` (`ZenithCook_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Cook steps"; Zenith2/Docs/Design/TWOD/Authoring.md → "Cook step keys"

### Z2-R-TWOD-034 Tileset document and format

- **Statement:** `*.tileset.json` declares tiles from one atlas with collision kinds, custom polygons and tile animations; it cooks to `.ztileset` (magic `ZTSE`) as specified.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCompile_MatchesTestWriter`, `Test_TwoDValidation_EveryRule` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Tileset document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked tileset format"

### Z2-R-TWOD-035 Tilemap document and format

- **Statement:** `*.tilemap.json` stores up to 16 layers of 32 × 32-cell chunks as base64 cells, sorted and one per line in canonical form; it cooks to `.ztilemap` (magic `ZTMP`) with a sorted chunk index.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDDocuments_CanonicalRoundTrip`, `Test_TwoDCompile_MatchesTestWriter` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Tilemap document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked tilemap format"

### Z2-R-TWOD-036 Tile cell encoding

- **Statement:** A tile cell is 32 bits: a 16-bit tile index (0 empty), flip X, flip Y and transpose; a cell with any of bits 19–31 set reads as empty, identically on the CPU and the GPU.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDFormats_TileCellSymmetries` (`Zenith2D_Tests`), `Test_Sprite2DRender_FlipsAndTileSymmetries` (`Zenith_Integration_TwoD`), `Test`.
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Tile cell encoding"

### Z2-R-TWOD-037 Tilemap component

- **Statement:** `Zenith.TwoD.Tilemap` registers its 10 properties; an unresolvable map draws and collides nothing with `twod.tilemap.unresolved_delta`.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ComponentsAndProperties` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Tilemap header"; Zenith2/Docs/Design/TWOD.md → "Component table"

### Z2-R-TWOD-038 Effective cell

- **Statement:** The effective cell of a map, layer and cell is the overlay's edited value, otherwise the authored cell, and empty outside the authored bounds.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TilemapRuntime_EffectiveCell` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime"

### Z2-R-TWOD-039 Tile edits in merge order

- **Statement:** `TwoD.SetTile` and `TwoD.FillTiles` apply at sync-point playback in merge order, clip `FillTiles` as specified, and never depend on whether the map's asset is resident.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TileEditPlayback_MergeOrder` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Tile edit playback"

### Z2-R-TWOD-040 Overlay capacity

- **Statement:** The overlay holds at most `twod.tilemap.maxEditedChunks` chunks per world; an edit needing a new chunk beyond it is dropped deterministically with `twod.tilemap.overlay_full_delta`.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TilemapRuntime_OverlayCapacity` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime"

### Z2-R-TWOD-041 Tile collision outlines

- **Statement:** Every dirty chunk of a colliding map generates static chain colliders by marching squares in map-local coordinates on one static body per (map, layer), merged across chunk borders with ghost vertices, each chain with a per-unit-edge cell table, equal to the reference outline.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TileCollision_OutlineMatchesReference` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap collision generation"; Zenith2/Docs/Design/TWOD/Formats.md → "Tile collision kinds"

### Z2-R-TWOD-042 No bumps across chunk seams

- **Statement:** A body sliding along tile collision across chunk seams never gains upward velocity at a seam.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TileCollision_NoBumpAcrossChunks` (`Zenith2D_Tests`, `Test`); the `ChainSeams` scenario.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap collision generation"

### Z2-R-TWOD-043 Tile chunk extraction

- **Statement:** Extraction sends each changed chunk once per revision, nearest the canvas camera first, and each tileset once, writing at most `twod.extract.maxChunkRecordsPerFrame` UPSERT records per frame (plus every REMOVE record of the frame); GPU uploads are paced separately by the renderer.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TilemapExtract_ChunkRecordShape` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"

### Z2-R-TWOD-044 GPU chunk pool and tile table

- **Statement:** The renderer keeps every sent tile chunk in a CPU mirror of `twod.render.tileChunkMirrorChunks` entries and makes chunks GPU-resident in a pool of `twod.render.tileChunkPoolChunks` 4 KiB slots by a per-frame wanted set (for a canvas, the visible chunks plus one chunk of margin; for every tilemap draw a non-pure-2D view draws, the world list or, in a view without a canvas, every draw, the chunks of the view frustum's footprint on the layer plane plus one, from the unjittered `VP = m_xProjection · m_xView`, converted to cells from the draw's absolute origin, and the map's authored bounds when an infinite far plane leaves the region unbounded; a world-space map on a pure-2D canvas follows the canvas rule) with least-recently-wanted eviction; uploads are paced at `twod.render.maxChunkUploadsPerFrame`; a wanted chunk not yet resident draws empty and is counted; tiles live in a 24-byte-entry table.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** the pending row proposed as TWOD-008 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Sprite2DRender_TileChunkScatterAndEdit`, `Test_Sprite2DRender_PoolPressureRecovers`, `Test_Sprite2DRender_WorldTilemapWantsFootprint` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DScatter pass"; Zenith2/Docs/Design/TWOD.md → "State machine index"

### Z2-R-TWOD-045 Tile edits visible next frame

- **Statement:** A tile edit applied at a tick's sync point is drawn in the next rendered frame, and only the edited chunk is uploaded.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_TileChunkScatterAndEdit` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"

### Z2-R-TWOD-046 Tilemap hot reload keeps edits

- **Statement:** A re-cooked tilemap reloaded in a running world keeps the world's overlay edits.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TilemapHotReload_KeepsEdits` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap hot reload"

### Z2-R-TWOD-047 Animation document and format

- **Statement:** `*.spriteanim.json` declares parameters, flipbook and skeletal clips, states, a default state and ordered transitions; it cooks to `.zsanim` (magic `ZSAN`) as specified.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCompile_MatchesTestWriter`, `Test_TwoDValidation_EveryRule` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Sprite animation document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked sprite animation format"

### Z2-R-TWOD-048 Flipbook frame selection

- **Statement:** A flipbook frame is selected from the state's tick count, speed and loop mode by the formula of "Flipbook evaluation" in binary64, for loop, once and ping-pong clips.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** the pending row proposed as TWOD-006 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Sprite2DFlipbook_FrameFormula` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation"

### Z2-R-TWOD-049 Paused and zero speed

- **Statement:** A paused animator writes nothing; speed 0 holds the current frame and emits no event.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DFlipbook_PausedWritesNothing`, `Test_Sprite2DFlipbook_SpeedZeroHolds` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation"

### Z2-R-TWOD-050 Animator transitions

- **Statement:** Transitions from ANY are evaluated before the state's own, in authored order; conditions and exit times fire as specified; at most one transition happens per tick.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_SpriteAnimator_Transitions`, `Test_SpriteAnimator_AnyBeforeOwn`, `Test_SpriteAnimator_OneTransitionPerTick` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"

### Z2-R-TWOD-051 Triggers

- **Statement:** A trigger stays set until a transition whose conditions use it fires, and is cleared by that transition alone.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_SpriteAnimator_TriggerConsumedOnlyByFiringTransition` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"

### Z2-R-TWOD-052 Animator commands

- **Statement:** `TwoD.AnimatorPlay` and `TwoD.AnimatorSetParameter` record the name's `Zenith_StringID` without reading the world, resolve it at playback against the entity's set, apply in merge order, and drop unknown names, kind mismatches and unresolved sets with `twod.anim.command_dropped_delta`.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SpriteAnimator_CommandsResolveAtPlayback`, `Test_TwoDScript_BufferedCommandsApplyNextSync` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"; Zenith2/Docs/Design/TWOD.md → "Luau errors and asserts"

### Z2-R-TWOD-053 Animation events

- **Statement:** Frame and key events fire once per crossing in order, including across loop wraps and multi-frame ticks, as `Zenith.TwoD.AnimationEvent`; a once clip's end emits `Zenith.TwoD.AnimationFinished`.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Animation2DEvents_OrderAndWrap` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Animation events"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDEvents header"

### Z2-R-TWOD-054 Events independent of workers

- **Statement:** The animation event stream is identical for 1, 2 and 8 workers.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Animation2DEvents_IndependentOfWorkers` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Animation events"; Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"

### Z2-R-TWOD-055 Animation hot reload

- **Statement:** A reloaded animation set keeps each animator's state by name and restarts an animator whose state was removed at the default state.
- **Covers:** Z2-I-TWOD-05, Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_SpriteAnimationHotReload_RemapsByName` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Animation hot reload"

### Z2-R-TWOD-056 Rig document and format

- **Statement:** `*.rig2d.json` declares bones with one root, slots and skins over one atlas; it cooks to `.zrig2d` (magic `ZR2D`) as specified.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCompile_MatchesTestWriter`, `Test_TwoDValidation_EveryRule` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Rig document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked rig format"

### Z2-R-TWOD-057 Cutout skeleton evaluation

- **Statement:** `TwoD.AnimateSkeletons` computes rig-space bone matrices from the bind pose and the skeletal clip's tracks, within 1 × 10^-5 m and 1 × 10^-6 rad of the double-precision reference, and publishes them in `Zenith.TwoD.Poses`.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** the pending row proposed as TWOD-006 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Skeleton2D_PoseMatchesReference` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Skeleton2D header"

### Z2-R-TWOD-058 Slots and draw order

- **Statement:** Each slot draws its current attachment from the active skin at its bone's matrix; draw-order tracks reorder slots and extraction emits them in the new order.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Skeleton2D_SlotDrawOrderTrack` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"

### Z2-R-TWOD-059 No root motion

- **Statement:** No 2D animation system writes an entity's transform.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSchedule_AccessCompiles` (declared access) (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Phase placement"

### Z2-R-TWOD-060 D-SEL-2DPHYS harness

- **Statement:** `Zenith_Bench_Physics2DSelection`, declared by `zenith_add_benchmark(Physics2DSelection DEPENDS Zenith2D ZenithSave ZenithProfiling ZenithCore THIRD_PARTY box2d jolt SUPPORTED ZENITH_WINDOWS ZENITH_ANDROID)`, implements candidate B on the `box2d` entry and candidate J directly on PHYS's pinned `jolt` entry behind the seam in its own tree (`Seam/`, `Internal/`), including public headers only; its `main` applies the CRT maths mode first, installs the root heap, creates the profiler, installs its hooks and registers lane 0, so the zone `Physics2DSelection.Tick` records; it runs the scenario suite on both and registers the CTest `Test_Physics2DSelection_HarnessRuns`, which asserts the zone's site exists.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DSelection_HarnessRuns` (the harness target, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Seam header placement"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"; Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"

### Z2-R-TWOD-061 D-SEL-2DPHYS ruling

- **Statement:** A `needs-human` Stage 2 ruling, blocked by the measurement run and by the Stage 1 per-system budget ticket for TWOD (which commits TWOD-004 and TWOD-005), applies the acceptance rule of "Selection decision D-SEL-2DPHYS" (the gates, the rows compared through the harness zone `Physics2DSelection.Tick` as an upper-bound check at the Stage 1 committed scales, the outcome table, ties by EXACT capability) to the report, appends the ruling to DecisionLog in its own diff, which touches only `Zenith2/Docs/`; the implementation ticket that follows it moves the seam into `Internal/Physics2D/` (and, under the Box2D outcome, the Box2D implementation into `Internal/Physics2D/Box2D/`) and deletes the loser's harness implementation; under the Jolt outcome neither carries J production code, which waits for PHYS's conditional post-signoff TD10 patch; when neither candidate passes the gates the ticket stays Blocked naming the failing scenarios and Stage 2 acceptance waits on it.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-097, D-SEL-2DPHYS (open, Stage 2)
- **Budgets:** the pending row proposed as TWOD-004 (TWOD.md "Pending row needs"), the pending row proposed as TWOD-005 (TWOD.md "Pending row needs")
- **Acceptance evidence:** the ruling entry in `Zenith2/Docs/DecisionLog.md` with the harness report.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"

### Z2-R-TWOD-062 Box2D pin

- **Statement:** When candidate B is measured or selected, `box2d` is pinned in `Zenith2/ThirdParty/manifest.json` (`cmake-subproject`, MIT, every option set explicitly; owner the harness with usage tools, `Zenith2D` with usage runtime after a Box2D ruling) and its headers appear only under the harness's `Internal/Box2D/` and Zenith2D's `Internal/Physics2D/Box2D/`.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck`; the manifest entry reviewed in the `needs-human` pin ticket.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Libraries Zenith2D depends on"; Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation"

### Z2-R-TWOD-063 Backend seam is internal

- **Statement:** No type of `Zenith_Physics2DBackend` appears in a public header; before the ruling the seam lives in the harness's tree and after it in Zenith2D's `Internal/Physics2D/`; the module's public API and its one-parameter `Zenith_Module_2D_Start` are identical under either outcome of D-SEL-2DPHYS.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `Libgraph` on the physics tickets.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam"; Zenith2/Docs/Design/TWOD/Simulation.md → "Seam header placement"

### Z2-R-TWOD-064 Physics components

- **Statement:** `Zenith.TwoD.RigidBody`, `Zenith.TwoD.Collider` and `Zenith.TwoD.Joint` register with their property IDs and authorities (runtime velocities and the broken flag `PHASE_PHYSICS`); a joint's AnchorB (property 5) is B-local and its world anchor is the separate property 20 `WorldAnchor`, read only when `BodyB` is null.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ComponentsAndProperties` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Physics2D header"; Zenith2/Docs/Design/TWOD.md → "Component table"

### Z2-R-TWOD-065 Ingest in simulation-ID order

- **Statement:** Ingest creates, rebuilds and removes bodies, shapes and joints in ascending simulation ID from a change list sorted independently of chunk layout.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** the pending row proposed as TWOD-004 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_TwoDDeterminism_Replay` (`Zenith2/Tests/Determinism/`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D ingest"; Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"

### Z2-R-TWOD-066 One physics body per entity

- **Statement:** An entity carrying both `Zenith.Physics.RigidBody` and `Zenith.TwoD.RigidBody` has no 2D body: none is created, and an existing one is removed when the PHYS body is added afterwards (`twod.physics.dual_body_delta`); the check resolves the PHYS type by stable name, so it needs no PHYS code; the scene check reports `TWOD_COOK_DIAG_TWO_PHYSICS_BODIES`.
- **Covers:** Z2-I-TWOD-06, Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DIngest_DualBodySkipped`, `Test_Physics2DIngest_LaterPhysicsBodyRemoves2DBody` (`Zenith2D_Tests`, with the `Zenith_TwoDTestForeignBody.h` stand-in), `Test_TwoDSceneChecks_EveryRule` (`ZenithAuthoring_Tests`), `Test`.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D ingest"; Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks"; Zenith2/Docs/Design/TWOD/Tests.md → "Test support"

### Z2-R-TWOD-067 Single-threaded deterministic step

- **Statement:** The backend step runs on the step node's thread with the configured substeps; worker count never reaches the solver.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** the pending row proposed as TWOD-004 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_TwoDDeterminism_Replay` (`Test`); the `Determinism` scenario.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D step"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam"

### Z2-R-TWOD-068 External writes adopted

- **Statement:** A script write of a body's velocity or of a dynamic body's local transform, detected by bit comparison of all seven position and rotation floats and three velocity floats with the written values, is applied to the backend before the step and counted.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DExternal_SevenFloatCompare`, `Test_Physics2DSync_ParentedBody` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D external writes"

### Z2-R-TWOD-069 Physics commands

- **Statement:** `TwoD.Body2D` commands apply in list order before the step (impulses once, forces constant over the tick); a full list drops with `twod.physics.command_dropped_delta`, a command without a body with `twod.physics.command_no_body_delta`.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DCommands_PendingOrder` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D pending operations"

### Z2-R-TWOD-070 Write-back and sync

- **Statement:** Write-back stores runtime velocities and sleep state; phase-6 sync writes `Zenith.LocalTransform` position and rotation of awake dynamic bodies only, keeping z and scale, through `WRITER_SOURCE_PHYSICS_2D`.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** the pending row proposed as TWOD-004 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Physics2DSync_WritesOnlyAwakeBodies`, `Test_Physics2DSync_KeepsZAndScale`, `Test_Physics2DSync_ParentedBody` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D write-back and sync"

### Z2-R-TWOD-071 Contact and trigger events

- **Statement:** Contact began/ended and trigger entered/exited events are emitted once per pair transition, sorted by record key, identically for any worker count.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DEvents_SortedByKey`, `Test_Physics2DEvents_IndependentOfWorkers` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Contact and trigger events"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDEvents header"

### Z2-R-TWOD-072 Joints

- **Statement:** Distance, revolute, prismatic, weld, wheel and motor joints hold their constraints within the scenario tolerances; a joint over its break force breaks once and emits `Zenith.TwoD.JointBroken`.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DJoint_BreakEmitsEvent` (`Zenith2D_Tests`); the six joint scenarios (`Zenith_Integration_TwoD`), `Test`.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D step"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Physics2D header"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"

### Z2-R-TWOD-073 One-way platforms

- **Statement:** A body passes up through a one-way collider and lands on it from above; disabling the collider for one tick (the drop-through pattern) passes it down.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DOneWay_PassFromBelowLandFromAbove`, `Test_Physics2DOneWay_DropThroughCommand` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "One-way platforms"

### Z2-R-TWOD-074 Continuous collision

- **Statement:** No dynamic body tunnels static geometry and a bullet body does not tunnel dynamic bodies: a 0.05 m-radius circle at 300 m/s and 60 Hz passes neither a 0.02 m static wall nor, as a bullet, a 0.02 m dynamic box, on either candidate.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DCCD_BulletThroughThinWall` (`Zenith2D_Tests`, `Test`); the `ContinuousCollision` scenario.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Continuous collision"

### Z2-R-TWOD-075 Sleeping

- **Statement:** A body below the sleep tolerances for `twod.physics.sleepSeconds` sleeps and wakes on contact, command or external write.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DSleep_RestAndWake` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Sleeping"

### Z2-R-TWOD-076 Queries

- **Statement:** Ray, circle and box queries return analytic hits within 1 × 10^-4 m; each backend query is a bounded collector keeping the 64 smallest hits by (fraction, body ID, collider ID, cell key) whatever its traversal, ties break by simulation ID, and concurrent readers get identical results.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DQuery_TieBreakDeterministic`, `Test_Physics2DQuery_ConcurrentReads` (`Zenith2D_Tests`, `Test`); `Test_Physics2DQuery_ConcurrentReadsStress` (`Sanitizers -Preset win-asan`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D queries"

### Z2-R-TWOD-077 Physics capacities

- **Statement:** Body, shape, joint, contact-pair and pending-operation capacities are reserved at attach; reaching one refuses deterministically with its counter and never allocates during a tick.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** the pending row proposed as TWOD-005 (TWOD.md "Pending row needs")
- **Acceptance evidence:** the capacity cases of `Test_Physics2DCommands_PendingOrder` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D model"; Zenith2/Docs/Design/TWOD.md → "Failure table"

### Z2-R-TWOD-078 Scenario suite as a gate

- **Statement:** After the ruling the sixteen `Test_Physics2DScenario_*` cases of the scenario suite (every pass/fail scenario except the S24 half of `Determinism`; the cost scenarios `CostGame2D` and `CostUI2D` have no `Test` case) run against the winner in the `Test` step.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DScenario_Stacking` and the fifteen other cases of "Integration suite" (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"; Zenith2/Docs/Design/TWOD/Tests.md → "Integration suite"

### Z2-R-TWOD-079 Camera component

- **Statement:** `Zenith.TwoD.Camera` registers its 11 properties on the main camera entity selected by RPROD and never writes `Zenith.Render.Camera`.
- **Covers:** Z2-I-TWOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ComponentsAndProperties` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"; Zenith2/Docs/Design/TWOD.md → "Component table"

### Z2-R-TWOD-080 Canvas geometry rules

- **Statement:** `Flux_ComputeCanvasGeometry` computes the integer scale, canvas extent and letterbox for INTEGER_LETTERBOX, INTEGER_EXPAND and NATIVE by rules 1–6, falling back to downscaling when the output is smaller than the reference.
- **Covers:** Z2-I-TWOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DCanvasGeometry_Rules` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Canvas geometry"

### Z2-R-TWOD-081 Camera mapping functions

- **Statement:** `Zenith_Camera2D_WorldToReference` and `Zenith_Camera2D_ReferenceToWorld` invert each other within 1 × 10^-9 m and are deterministic binary32 functions simulation may call.
- **Covers:** Z2-I-TWOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Camera2D_ReferenceWorldRoundTrip` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"

### Z2-R-TWOD-082 Snap modes

- **Statement:** NONE draws at exact positions, TEXEL rounds the camera to whole reference pixels, SUBPIXEL renders with a one-pixel margin and applies the camera's fraction as a composite offset.
- **Covers:** Z2-I-TWOD-07, Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSnap_TexelCameraRounds`, `Test_Sprite2DSnap_SubpixelOffsetsAndMargin` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"

### Z2-R-TWOD-083 Camera-relative snapping

- **Statement:** Record origins are stored relative to the binary64 canvas camera, so snapping gives bit-identical results for a scene and camera offset by ±10,000 m as at the origin when every coordinate is a multiple of 2⁻¹⁰ m (exact in binary32 at both magnitudes), and otherwise results within the binary32 precision of the position.
- **Covers:** Z2-I-TWOD-07, Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSnap_FarFromOrigin` (`ZenithFlux_Tests`, `Test`); `Test_Sprite2DExtract_OriginsCameraRelative` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"

### Z2-R-TWOD-084 Camera bounds

- **Statement:** With `BoundsEnabled`, extraction clamps the rendered canvas camera position so the reference rectangle stays inside `BoundsMin`–`BoundsMax` before snapping, and never writes the camera entity or `Zenith.Render.Camera`.
- **Covers:** Z2-I-TWOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DCanvasGeometry_Rules` bounds case (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"

### Z2-R-TWOD-085 Pure-2D views

- **Statement:** A view is pure-2D when its effective feature mask has none of the bits of `uTWOD_PURE_2D_FEATURE_CLEAR_MASK`; such a view runs no 3D pass, still composites UI, and draws its world-space sprites on its canvas.
- **Covers:** Z2-I-TWOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_UIOverCanvas`, `Test_Sprite2DRender_WorldSpriteInPure2DView` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"; Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds"

### Z2-R-TWOD-086 Several canvases

- **Statement:** Up to `twod.render.maxCanvasesPerFrame` canvas views draw per frame; the excess draw their canvas content as world content with `flux.twod.canvas_limit_delta`.
- **Covers:** Z2-I-TWOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRenderer_WritesAfterReservation` canvas-limit case (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path"; Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour"

### Z2-R-TWOD-087 Pixel-exact canvas

- **Statement:** Canvas content drawn at integer scales 1, 3 and 6 into `R8G8B8A8_SRGB` equals the CPU mapping of "Snapping" byte for byte, with no tolerance and no committed golden image.
- **Covers:** Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** the pending row proposed as TWOD-002 (TWOD.md "Pending row needs"), the pending row proposed as TWOD-003 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Sprite2DRender_PixelExactCanvas` (`Zenith_Integration_TwoD`, `Test`, WARP and lavapipe).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"; Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"

### Z2-R-TWOD-088 Flips and symmetries exact

- **Statement:** Sprites and tiles under the eight flip and transpose symmetries equal the reference mapping byte for byte.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_FlipsAndTileSymmetries` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"; Zenith2/Docs/Design/TWOD/Formats.md → "Tile cell encoding"

### Z2-R-TWOD-089 Integer composite

- **Statement:** `Sprite2DComposite` scales the canvas by k with point sampling, fills the letterbox with the letterbox colour and writes every output pixel.
- **Covers:** Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** the pending row proposed as TWOD-003 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Sprite2DRender_CompositeLetterbox` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DComposite pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Canvas geometry"

### Z2-R-TWOD-090 Sprite corner snapping

- **Statement:** A `PixelSnap` sprite's corner lands on whole reference pixels; rotated or mixed-PPU canvas sprites are counted (`twod.render.rotated_canvas_sprites_delta`, `twod.render.mixel_sprites_delta`).
- **Covers:** Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSnap_SpriteCornerRounds` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"; Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"

### Z2-R-TWOD-091 UI composites over the canvas

- **Statement:** The canvas passes run at `FLUX_EXTENSION_POINT_BEFORE_UI`, so UI drawn by `UIComposite` covers the canvas.
- **Covers:** Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_UIOverCanvas` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds"

### Z2-R-TWOD-092 HDR output

- **Statement:** In HDR10 output the canvas composite encodes white to PQ of `r.hdr.uiNits` through `Flux_EncodeOutput`, and in scRGB to `r.hdr.uiNits` / 80.
- **Covers:** Z2-I-TWOD-08
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DRender_HDR10Encoding` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Colour and HDR output"

### Z2-R-TWOD-093 Editor game view matches the Player

- **Statement:** The editor's game view of a play world at an integer scale is byte-identical to a Player render of the same world.
- **Covers:** Z2-I-TWOD-08, Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDEditor_GameViewMatchesPlayer` (`ZenithEditor_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "2D scene view mode"

### Z2-R-TWOD-094 Document types registered

- **Statement:** `Zenith_RegisterTwoDDocumentTypes` registers the six document types with their schema tags, suffixes, canonical schemas and version 1, and every subsequent version adds one migration step with its fixtures in the same diff.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDMigration_Fixtures` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Document types and migration"; Zenith2/Docs/Design/TWOD/Authoring.md → "ZenithAuthoring functions"

### Z2-R-TWOD-095 Unknown members preserved

- **Statement:** Loading and saving any TWOD document is byte-identical and preserves unknown members with a warning.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDDocuments_CanonicalRoundTrip` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Common rules"

### Z2-R-TWOD-096 Validation rules

- **Statement:** Each of the 15 validation rules is reported with its severity and message by the cook and the editor; an error fails the document's cook item.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDValidation_EveryRule` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Validation rules"; Zenith2/Docs/Design/TWOD/Authoring.md → "Cook diagnostics"

### Z2-R-TWOD-097 Document cook steps

- **Statement:** The `twod.spriteAnimation`, `twod.tileset`, `twod.tilemap` and `twod.rig` steps open their documents through `OpenSourceDocument`, key on the documents' digests and the referenced assets' hashes, and produce byte-identical output in clean and incremental cooks.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCook_Deterministic`, `Test_TwoDCook_ImageEditRecooksDependents` (`ZenithCook_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Cook steps"; Zenith2/Docs/Design/TWOD/Authoring.md → "Cook step keys"

### Z2-R-TWOD-098 Cook validators

- **Statement:** Each TWOD kind registers a cook validator that runs the runtime reader over the written file.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCook_ValidatorRunsReader` (`ZenithCook_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Cook steps"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked reader checks"

### Z2-R-TWOD-099 Shared authoring functions

- **Statement:** The packer, compilers, validation, auto-tiling and scene checks live in `ZenithAuthoring` (`Zenith2/Tools/Authoring/Internal/TwoD/`, root header `Zenith_AuthoringTwoD.h`) and are pure functions shared by ZenithCook and ZenithEditor.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCompile_MatchesTestWriter` (`ZenithAuthoring_Tests`, `Test`); `Libgraph`.
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "ZenithAuthoring functions"; Zenith2/Docs/Design/TWOD/Authoring.md → "Tool-side layout"

### Z2-R-TWOD-100 Scene content checks

- **Statement:** `Zenith_CheckTwoDSceneContent` reports each of the six scene rules with its severity, in the editor on save and in the cook once SCENE provides the registration.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSceneChecks_EveryRule` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks"

### Z2-R-TWOD-101 Editor extension

- **Statement:** `Zenith_TwoDEditorExtension` registers its panels, document handlers, inspectors, gizmos, asset kind handlers, commands, preferences and automation steps with no duplicate, under EDIT's extension contract.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDEditor_ExtensionRegisters` (`ZenithEditor_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Editor extension"

### Z2-R-TWOD-102 Atlas editor

- **Statement:** The atlas editor edits sources, grids, pivots and borders as document commands and shows a live pack identical to the cooked result.
- **Covers:** Z2-I-TWOD-03, Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDEditor_WorkflowRehearsal` (`ZenithEditor_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite and atlas editor"

### Z2-R-TWOD-103 Tilemap painter

- **Statement:** Brush, rectangle, bucket, eraser, picker and terrain tools paint as one undoable command per stroke; the terrain brush re-resolves the 3 × 3 neighbourhood through the auto-tile rules.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDEditor_PaintStrokeUndo`, `Test_TwoDEditor_TerrainBrushResolves` (`ZenithEditor_Tests`), `Test_TilesetAutoTile_BlobReduction` (`ZenithAuthoring_Tests`), `Test`.
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Tilemap painter"

### Z2-R-TWOD-104 Play-mode painting uses commands

- **Statement:** Painting in a play world records `TwoD.SetTile` commands and leaves the document unchanged.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDEditor_PlayModePaintUsesCommands` (`ZenithEditor_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Tilemap painter"

### Z2-R-TWOD-105 Animation and rig editors

- **Statement:** The animation editor edits states, transitions, frames and keys with a preview running the module's own evaluation; the rig editor edits bones, slots and skins as document commands.
- **Covers:** Z2-I-TWOD-05, Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDEditor_WorkflowRehearsal` (`ZenithEditor_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "2D animation editor"; Zenith2/Docs/Design/TWOD/Authoring.md → "Rig editor"

### Z2-R-TWOD-106 2D authoring workflow

- **Statement:** The six-step 2D authoring workflow runs green as an automation rehearsal and is walked by a human at Stage 4 acceptance.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDEditor_WorkflowRehearsal` (`ZenithEditor_Tests`, `Test`); the Stage 4 walkthrough record.
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "2D authoring workflow"

### Z2-R-TWOD-107 2D template content

- **Statement:** A game created from the 2D template with the 2D module selected has a camera with `Zenith.TwoD.Camera` at 320 × 180, 16 PPU, INTEGER_LETTERBOX and TEXEL snapping, the pure-2D feature mask and the default sorting layers.
- **Covers:** Z2-I-TWOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** MOD's `Test_NewGame_CreatedGameBoots` 2D case extended by row TD15 (`Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "2D project template content"

### Z2-R-TWOD-108 UI2D sample 2D layer

- **Statement:** WorkloadUI2D carries TWOD's 2D layer: a 480 × 270 canvas, four parallax tilemap layers, animated sprites and skeletons and dynamic 2D bodies at the counts UI's Stage 1 workload scale ticket for W-UI2D commits (TWOD proposes 500 bodies), with UI over the canvas.
- **Covers:** Z2-I-TWOD-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** Z2-B-TWOD-001, Z2-B-CONTENT-009
- **Acceptance evidence:** the W-UI2D budget runs (`Zenith_Bench_TwoD`); the WorkloadUI2D boot test.
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "UI2D sample content"

### Z2-R-TWOD-109 Generated sample content

- **Statement:** The UI2D sample's and Game2D's images are generated at cook time from seeds; no binary art is committed.
- **Covers:** Z2-I-TWOD-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** Z2-B-CONTENT-004, Z2-B-CONTENT-009
- **Acceptance evidence:** `Test_TwoDIntegration_SampleContentIsGenerated` (`Zenith_Integration_TwoD`, `Test`); `Test_TwoDImageGenerator_Goldens`, `Test_TwoDCook_GeneratedAtlasSource` (`ZenithAuthoring_Tests`, `ZenithCook_Tests`, `Test`); the repository content check of the source-asset quota rows.
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "UI2D sample content"; Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice"

### Z2-R-TWOD-110 Game2D definition

- **Statement:** `Zenith2/Games/Game2D` selects the modules `2D`, `Audio`, `Gameplay`, `Online` and `UI` (and, under the Jolt outcome of D-SEL-2DPHYS, `Physics` with backend Jolt and `physics.planarWorld` true) on the Custom ECS backend and contains no C++.
- **Covers:** Z2-I-TWOD-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `SourceLayout` on the game; `Test_TwoDIntegration_Game2DBoots` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice"

### Z2-R-TWOD-111 Game2D flagship acceptance ticket

- **Statement:** The Stage 1 flagship acceptance ticket for Game2D sets W-Game2D's content scale (including the 2D physics scale D-SEL-2DPHYS measures, with TWOD's proposal as an input) in `Zenith2/Docs/Research/Workloads/W-Game2D.md`, the playthrough script and Z2-B-SHIP-022.
- **Covers:** Z2-I-TWOD-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-012
- **Budgets:** Z2-B-SHIP-022
- **Acceptance evidence:** the committed values in `Zenith2/Docs/Budgets.md` and `Zenith2/Docs/Research/Workloads/W-Game2D.md`.
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice"; Zenith2/Docs/Design/TWOD.md → "Rows owned"

### Z2-R-TWOD-112 Game2D first playable

- **Statement:** Game2D's first playable level runs its playthrough on Windows and the S24 with pixel-exact canvas output at the device's integer scale.
- **Covers:** Z2-I-TWOD-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-TWOD-001, the pending row proposed as TWOD-002 (TWOD.md "Pending row needs"), the pending row proposed as TWOD-004 (TWOD.md "Pending row needs")
- **Acceptance evidence:** the device run record (`needs-human`) of `Zenith2/Docs/Design/TWOD/Tests.md` "Device runs".
- **Design:** Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice"; Zenith2/Docs/Design/TWOD/Tests.md → "Device runs"

### Z2-R-TWOD-113 Game2D release acceptance

- **Statement:** Game2D meets every row it measures on W-Game2D at Stage 6, as an equally binding flagship slice.
- **Covers:** Z2-I-TWOD-10
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-TWOD-001, Z2-B-SHIP-022, Z2-B-PACK-004
- **Acceptance evidence:** the Stage 6 flagship acceptance records.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Stage table"; Zenith2/Docs/Design/TWOD.md → "Rows owned"

### Z2-R-TWOD-114 Per-system budget ticket

- **Statement:** The Stage 1 per-system budget ticket for TWOD commits Z2-B-TWOD-001 and the pending rows TWOD-002 to TWOD-008 with their values.
- **Covers:** Z2-I-TWOD-02, Z2-I-TWOD-06, Z2-I-TWOD-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-TWOD-001
- **Acceptance evidence:** the committed rows in `Zenith2/Docs/Budgets.md`.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Pending row needs"; Zenith2/Docs/Design/TWOD.md → "Rows owned"

### Z2-R-TWOD-115 2D CPU within budget

- **Statement:** Sprite extraction, sorting, batching and frame writing meet Z2-B-TWOD-001 on W-UI2D and W-Game2D on Windows and the S24.
- **Covers:** Z2-I-TWOD-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-TWOD-001
- **Acceptance evidence:** `Test_TwoDBench_SmokeRuns` (`Test`); the values recorded on the reference desktop and the S24 by the `needs-human` TWOD budget-run ticket.
- **Design:** Zenith2/Docs/Design/TWOD/Tests.md → "Benchmarks"; Zenith2/Docs/Design/TWOD.md → "Rows owned"

### Z2-R-TWOD-116 Determinism replay

- **Statement:** The 600-tick 2D determinism scene replays with bit-identical per-tick world-state hashes for 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-05, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDDeterminism_Replay` (`Zenith_Integration_TwoDDeterminism`, `Zenith2/Tests/Determinism/TwoD/`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"

### Z2-R-TWOD-117 Presentation never reaches simulation

- **Statement:** No extraction, snapping, canvas or render setting writes simulation state.
- **Covers:** Z2-I-TWOD-07, Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSchedule_AccessCompiles` (declared access) (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"

### Z2-R-TWOD-118 Save participant

- **Statement:** `Zenith_TwoDSaveParticipant` saves, as of phase 11, the tile overlay, the collision dirty set, the map records and chain index, the body, shape and joint records with seven written local floats per body, the pair and one-way disabled sets, the pending operation list, the pose buffer, the ground body's seam ID and recorded position `g` (in the 80-byte section header) and the backend state; an EXACT round trip continues bit-identically, including a save taken while phase-10 commands are pending and a null-`BodyB` joint created after a restore of a shifted world; a COHERENT restore ignores the ground body fields.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSave_ExactRoundTrip`, `Test_TwoDSave_ExactRoundTripWithPendingCommands` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation"; Zenith2/Docs/Design/TWOD/Formats.md → "TWOD save section format"

### Z2-R-TWOD-119 Coherent downgrade

- **Statement:** A backend whose `SupportsExactState` is false makes the participant's `GetCaptureFidelity` return COHERENT, so the capture is COHERENT; its restore rebuilds bodies from components at the recorded poses and velocities without duplicate contact events.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSave_CoherentDowngrade` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation"

### Z2-R-TWOD-120 Corrupt save refused

- **Statement:** A corrupted TWOD save section, or an EXACT backend blob that fails its check, is refused with `SAVE_ERROR_CORRUPT` before any state changes; there is no restore-time downgrade.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSave_CorruptSectionRefused` (`Zenith2D_Tests`, `Test`); `Zenith_Fuzz_TwoDSaveSection`.
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "TWOD save section format"; Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation"

### Z2-R-TWOD-121 Script bindings

- **Statement:** Every function of "Luau surface" is bound in the `TwoD` table with its access class; buffered functions take effect at the next sync point.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-04, Z2-I-TWOD-05, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDScript_FunctionsBound`, `Test_TwoDScript_BufferedCommandsApplyNextSync` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD.md → "Luau surface"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDScriptApi header"

### Z2-R-TWOD-122 Misuse codes

- **Statement:** Each misuse of "Luau errors and asserts" (ZS2401, ZS2405 to ZS2409) raises its code of TWOD's block with its exact text and never asserts; a non-integral or out-of-range layer mask is SCRIPT's marshalling error (ZS1502, ZS1503), never a TWOD code.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-04, Z2-I-TWOD-05, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDScript_MisuseCodes` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD.md → "Luau errors and asserts"

### Z2-R-TWOD-123 Memory consumer

- **Statement:** Zenith2D reports its resident bytes (the backend's process-wide share once plus each world's share, record tables, poses and overlays) as one `MEMORY_SUBSYSTEM_SIMULATION` consumer and sheds nothing at WARNING or CRITICAL; the renderer's 2D memory is reported through `Flux_Sprite2DStats`.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** the pending row proposed as TWOD-005 (TWOD.md "Pending row needs"), the pending row proposed as TWOD-007 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_TwoDMemoryConsumer_ReportsAndShedsNothing` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Memory consumer"

### Z2-R-TWOD-124 Cooked readers never assert

- **Statement:** Every TWOD reader checks counts, offsets and indices before use, fails with a result, and survives arbitrary bytes.
- **Covers:** Z2-I-TWOD-03, Z2-I-TWOD-04, Z2-I-TWOD-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDFormats_ReaderChecks`, `Test_TwoDLoaders_FailureIsResult` (`Zenith2D_Tests`, `Test`); `Zenith_Fuzz_TwoDAsset`.
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Cooked reader checks"

### Z2-R-TWOD-125 Parser limits

- **Statement:** Every row of "Parser limits" is accepted at the limit and refused at limit + 1.
- **Covers:** Z2-I-TWOD-03, Z2-I-TWOD-04, Z2-I-TWOD-05, Z2-I-TWOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDFormats_LimitsAtAndAbove` (`Zenith2D_Tests`, `Test`); `Zenith_Fuzz_TwoDDocument`.
- **Design:** Zenith2/Docs/Design/TWOD/Formats.md → "Parser limits"

### Z2-R-TWOD-126 Schedule access

- **Statement:** Every TWOD node compiles into the world schedule in its phase with its declared access and no conflict, beside a second phase-6 transform writer of another writer source, in a world without the PHYS module (the dual-body type a stand-in); under the Jolt outcome only, the planar-world write also compiles beside PHYS's phase-5 nodes.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSchedule_AccessCompiles` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Phase placement"; Zenith2/Docs/Design/TWOD.md → "Frame-graph placement"

### Z2-R-TWOD-127 No locks

- **Statement:** Zenith2D and TWOD's ZenithFlux files take no lock and create no thread; Box2D runs with one worker and no task callbacks.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` and the lock-order checker (no TWOD row); review of the backend configuration.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Lock-order entries"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam"

### Z2-R-TWOD-128 Diagnostics

- **Statement:** The counters, profiler zones, log categories, debug views and frame-dump section of "Diagnostics and profiling" exist with the names stated, and the debug views exist only in development builds.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-02, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ConfigKeys` (debug keys) (`Zenith2D_Tests`, `Test`); review.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Counters"; Zenith2/Docs/Design/TWOD.md → "Profiler zones"; Zenith2/Docs/Design/TWOD.md → "Debug views"

### Z2-R-TWOD-129 Command kind IDs resolved at attach

- **Statement:** The module resolves the five command-kind IDs into `Zenith_TwoDCommandKinds` at its first `AttachWorld`; every recorder takes that struct after the buffer, and a zero ID records nothing and counts `twod.command.unresolved_kind_delta`.
- **Covers:** Z2-I-TWOD-04, Z2-I-TWOD-05, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDCommands_KindsResolvedAtAttach` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"; Zenith2/Docs/Design/TWOD.md → "World attach and detach"

### Z2-R-TWOD-130 Animator command payloads

- **Statement:** `Zenith_AnimatorPlayPayload` and `Zenith_AnimatorParameterPayload` are 24-byte payloads whose every byte is a declared field (no implicit padding reaches the command buffer), carrying the entity, the name's `Zenith_StringID` and the restart flag or value and kind byte, recorded by `Zenith_RecordAnimatorPlay` and `Zenith_RecordAnimatorSetParameter`; like every TWOD payload they are in-process command bytes, never serialized.
- **Covers:** Z2-I-TWOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_EventsAndPayloads`, `Test_SpriteAnimator_CommandsResolveAtPlayback` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Sprite2DAnimation header"; Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"

### Z2-R-TWOD-131 Script query value structs

- **Statement:** Every `TwoD` binding parameter and result has a row of the Value marshalling table: casts return the value struct `Zenith.TwoD.RayHit`, `OverlapBox` returns `Zenith.TwoD.OverlapResult`, and no reference out-parameter or span crosses a binding.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDScript_FunctionsBound` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDScriptApi header"; Zenith2/Docs/Design/TWOD.md → "Luau surface"

### Z2-R-TWOD-132 Tilemap parallax and opacity

- **Statement:** A tilemap layer of parallax (px, py) is drawn at the camera-relative origin plus ((1 − px)·cx, (1 − py)·cy) before snapping, its opacity multiplies the tint's alpha, parallax is ignored in world views, and a parallax layer generates no collision.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TilemapDraw_ParallaxFormula` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"; Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime"

### Z2-R-TWOD-133 Premultiplied canvas blending

- **Statement:** The canvas holds premultiplied linear colour; ALPHA and PREMULTIPLIED blend `ONE, ONE_MINUS_SRC_ALPHA` on colour and alpha, ADDITIVE `ONE, ONE` on colour and `ZERO, ONE` on alpha, MULTIPLY `DST_COLOR, ZERO` on colour with `lerp(1, c, a)` and `ZERO, ONE` on alpha, and the over composite blends premultiplied.
- **Covers:** Z2-I-TWOD-02, Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** the pending row proposed as TWOD-002 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Sprite2DRender_CanvasOverScene`, `Test_Sprite2DRender_BlendModes` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DCanvas pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DComposite pass"

### Z2-R-TWOD-134 Signed composite indexing

- **Statement:** The composite computes canvas texels with signed `floor_div` so the SUBPIXEL top rows read margin row 0, applies the margin under the rule-5 downscale with SUBPIXEL offsets ignored, and never indexes outside the canvas.
- **Covers:** Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** the pending row proposed as TWOD-003 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Sprite2DComposite_SubpixelMarginRows` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DComposite pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"

### Z2-R-TWOD-135 Integer-domain snapping

- **Statement:** Camera snapping computes the remainder `cx·s − n` in binary64 and never forms `n / s`, so corners at ±0.5 boundaries round identically for every PPU, and the CPU reference uses the same operations in the same order.
- **Covers:** Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSnap_SpriteCornerRounds` at PPU 16, 24 and 100 (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"

### Z2-R-TWOD-136 Camera-invariant y sort

- **Statement:** The y key quantises both the sort point and the camera y to 1/256 m before their difference, so two records' relative order in a y-sorted layer never changes as the camera moves.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DSort_KeyFields` (`ZenithFlux_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"

### Z2-R-TWOD-137 Physics origin shift

- **Statement:** At a `LIFECYCLE_EVENT_ORIGIN_SHIFTED` the module shifts every backend body, map static body and ground body by the shift, applies SCENE's formula to root bodies' written positions, map positions, the ground body's recorded position `g` and pending world points, and counts the shift, so no external write or static move is detected; a null-`BodyB` joint anchors at `WorldAnchor − g` (Joint property 20) at every creation, so a creation before a shift and a re-creation after one agree while `WorldAnchor` is a current-frame point (SCENE shifts it once TD24 lands); a body-to-body joint's B-local AnchorB is never shifted.
- **Covers:** Z2-I-TWOD-06, Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DOriginShift_ContinuesSeamlessly` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D origin shift"

### Z2-R-TWOD-138 D-SEL-2DPHYS measurement run

- **Statement:** A `needs-human` Stage 2 ticket, blocked by the harness and the two Stage 1 scale tickets, runs every pass/fail scenario on both candidates, the S24 half of `Determinism` and the cost scenarios `CostGame2D` and `CostUI2D` under "D-SEL-2DPHYS cost procedure" at the committed Game2D and W-UI2D scales on the reference desktop and the S24, and commits the report under `Zenith2/Docs/Research/Physics2DSelection/`.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S2
- **Classification:** research
- **Decisions:** Z2-D-084
- **Budgets:** the pending row proposed as TWOD-004 (TWOD.md "Pending row needs"), the pending row proposed as TWOD-005 (TWOD.md "Pending row needs")
- **Acceptance evidence:** the committed report under `Zenith2/Docs/Research/Physics2DSelection/` (the `needs-human` run ticket).
- **Design:** Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"

### Z2-R-TWOD-139 Trigger reach

- **Statement:** A sensor reports only dynamic and kinematic non-sensor colliders, evaluated against end-of-tick poses, never static colliders, tile chains or other sensors, on either candidate.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DEvents_TriggerReach` (`Zenith2D_Tests`, `Test`); the `Sensors` scenario.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Contact and trigger events"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam"

### Z2-R-TWOD-140 Joint break force

- **Statement:** A joint breaks when its last substep's linear constraint force, the impulse over the substep duration in newtons, exceeds `BreakForce`, on either candidate.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Physics2DJoint_BreakEmitsEvent` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D step"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam"

### Z2-R-TWOD-141 Axis-aligned tilemaps follow their entity

- **Statement:** Tilemaps draw and collide at their entity's world position only; a moved map moves its static bodies without regenerating chains; a rotated or scaled map entity is counted and warned by a scene check.
- **Covers:** Z2-I-TWOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_TileCollision_MovedMapFollows` (`Zenith2D_Tests`, `Test`); `Test_TwoDSceneChecks_EveryRule` (`ZenithAuthoring_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap collision generation"; Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime"; Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks"

### Z2-R-TWOD-142 Box2D process state

- **Statement:** Under the Box2D outcome the allocator hooks are installed with the same root-heap functions at every module start and never uninstalled, Box2D worlds are created and destroyed only on the main thread at attach and detach, and a world beyond the world table fails attach with `PHYSICS2D_ERROR_WORLD_CAPACITY`.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-082, D-SEL-2DPHYS (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDModule_StartUnwindsOnFailure` (the world-capacity case) (`Zenith2D_Tests`, `Test`); `ForbiddenCheck`.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation"

### Z2-R-TWOD-143 Canvas backend parity

- **Statement:** The 2D parity scene renders on Vulkan and D3D12 to canvases equal byte for byte on the pixel-exact subset (opaque, untinted, point-sampled texels) and within TWOD's declared 1/255-per-channel tolerance on blended and tinted pixels; the scene directory holds text only.
- **Covers:** Z2-I-TWOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-078
- **Budgets:** none
- **Acceptance evidence:** `Test_Sprite2DParity_CanvasScene` (`BackendParity`).
- **Design:** Zenith2/Docs/Design/TWOD/Tests.md → "Gate steps and labels"; Zenith2/Docs/Design/TWOD/Tests.md → "Render tests"

### Z2-R-TWOD-144 Physics 2D writer source

- **Statement:** The 2D sync writes through `WRITER_SOURCE_PHYSICS_2D` (value 6, schema name `Physics2D`, default precedence after physics and before animation) under either outcome of D-SEL-2DPHYS.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDSchedule_AccessCompiles` (`Zenith2D_Tests`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D write-back and sync"

### Z2-R-TWOD-145 Jolt-2D through the planar world

- **Statement:** Conditional on the J outcome (ruling TD10-CONDITIONAL): if D-SEL-2DPHYS selects Jolt-2D, and only after PHYS's post-signoff patch lands row TD10, Zenith2D steps its 2D bodies in the Physics module's planar world resource `Zenith.Physics.PlanarWorld` (Jolt backend only), written only by `TwoD.Physics2DStep`, shifted and counted once, and never constructs Jolt or a `Zenith_Physics` instance; under the Box2D outcome this requirement is void.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-084, D-SEL-2DPHYS (open, Stage 2)
- **Budgets:** the pending row proposed as TWOD-004 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_TwoDSchedule_AccessCompiles` (the planar-world access case) and the `Test_Physics2DScenario_*` cases on the winner (`Test`); `Libgraph`.
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation"

### Z2-R-TWOD-146 D-SEL-2DPHYS cost procedure

- **Statement:** The harness measures each candidate's cost as the zone `Physics2DSelection.Tick` around every seam call one `TwoD.Physics2DStep` would make per tick (p50 and p95 over ticks 300 to 2,099) and the peak `GetResidentBytes` + `GetProcessResidentBytes`, in the seeded scenes `CostGame2D` and `CostUI2D` built from the committed counts, and the ruling compares them with TWOD-004 and TWOD-005 as an upper-bound check.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S2
- **Classification:** research
- **Decisions:** Z2-D-084, D-SEL-2DPHYS (open, Stage 2)
- **Budgets:** the pending row proposed as TWOD-004 (TWOD.md "Pending row needs"), the pending row proposed as TWOD-005 (TWOD.md "Pending row needs")
- **Acceptance evidence:** `Test_Physics2DSelection_HarnessRuns` (the cost rows at the smoke counts, the harness target, `Test`); the committed report of the `needs-human` measurement run.
- **Design:** Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"; Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"; Zenith2/Docs/Design/TWOD.md → "Pending row needs"

### Z2-R-TWOD-147 Box2D path needs no PHYS deliverable

- **Statement:** Under the Box2D outcome Zenith2D, `Zenith2D_Tests` and the integration suites depend on no PHYS library, module or post-signoff patch (the harness only on the Stage 1 `jolt` pin), and the J-path tickets stay draft until the ruling.
- **Covers:** Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-084, D-SEL-2DPHYS (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` on the Box2D ruling's diff and on every S3 physics ticket; `Test_TwoDSchedule_AccessCompiles`, `Test_Physics2DIngest_DualBodySkipped` (`Zenith2D_Tests`, `Test`) built without ZenithPhysics.
- **Design:** Zenith2/Docs/Design/TWOD.md → "Areas TWOD depends on"; Zenith2/Docs/Design/TWOD.md → "Libraries Zenith2D depends on"; Zenith2/Docs/Design/TWOD.md → "Stage table"

### Z2-R-TWOD-148 Deterministic trigonometry

- **Statement:** Every trigonometric evaluation in a TWOD simulation node or playback uses CORE's deterministic binary32 `Zenith_Sin`, `Zenith_Cos` and `Zenith_Atan2` (X314; names as `Zenith2/Docs/Design/CORE/Maths.md` "Transcendental declarations" declares them, pending CORE's X314 follow-up, in an assumption block), never a `<cmath>` transcendental.
- **Covers:** Z2-I-TWOD-05, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Zenith2D.NoCrtTranscendental` and its six self-tests (`Test`, label `z2-cmake`), the interim scan [Z2-N-CONTRACT-579] requires until `ForbiddenCheck` rule F21 lands; `Test_TwoDDeterminism_Replay` (`Zenith_Integration_TwoDDeterminism`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D model"; Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation"

### Z2-R-TWOD-149 Explicit script names

- **Statement:** Every type `Zenith_Module_2D_RegisterTypes` registers (nine components, seven events, five payloads, four structs, three world resources) sets an explicit script name, Bindings rule 1 applied to its C++ type name (for example `Camera2D`, `RigidBody2D` and `Contact2DBeganEvent`), unique programme-wide (ruling X353-SCRIPTNAMES, cross-doc item X353), so a registry holding TWOD's types beside every engine type freezes without `REFLECT_ERROR_BAD_NAME`.
- **Covers:** Z2-I-TWOD-01, Z2-I-TWOD-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_TwoDRegistration_ComponentsAndProperties`, `Test_TwoDRegistration_EventsAndPayloads` (`Zenith2D_Tests`, `Test`); `Test_TwoDIntegration_RegistryFreezesWithEngineTypes` (`Zenith_Integration_TwoD`, `Test`).
- **Design:** Zenith2/Docs/Design/TWOD.md → "Component table"; Zenith2/Docs/Design/TWOD.md → "World resources events and commands"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Module_2D header"

## Coverage of inventory items

| Item | Title | Requirements |
|---|---|---|
| Z2-I-TWOD-01 | sprites | Z2-R-TWOD-001, Z2-R-TWOD-002, Z2-R-TWOD-003, Z2-R-TWOD-004, Z2-R-TWOD-005, Z2-R-TWOD-006, Z2-R-TWOD-007, Z2-R-TWOD-008, Z2-R-TWOD-009, Z2-R-TWOD-010, Z2-R-TWOD-011, Z2-R-TWOD-012, Z2-R-TWOD-022, Z2-R-TWOD-032, Z2-R-TWOD-121, Z2-R-TWOD-122, Z2-R-TWOD-126, Z2-R-TWOD-128, Z2-R-TWOD-136, Z2-R-TWOD-149 |
| Z2-I-TWOD-02 | sprite batching through the render graph | Z2-R-TWOD-010, Z2-R-TWOD-012, Z2-R-TWOD-013, Z2-R-TWOD-014, Z2-R-TWOD-015, Z2-R-TWOD-016, Z2-R-TWOD-017, Z2-R-TWOD-018, Z2-R-TWOD-019, Z2-R-TWOD-020, Z2-R-TWOD-021, Z2-R-TWOD-022, Z2-R-TWOD-023, Z2-R-TWOD-024, Z2-R-TWOD-025, Z2-R-TWOD-114, Z2-R-TWOD-115, Z2-R-TWOD-128, Z2-R-TWOD-133, Z2-R-TWOD-136 |
| Z2-I-TWOD-03 | atlases | Z2-R-TWOD-025, Z2-R-TWOD-026, Z2-R-TWOD-027, Z2-R-TWOD-028, Z2-R-TWOD-029, Z2-R-TWOD-030, Z2-R-TWOD-031, Z2-R-TWOD-032, Z2-R-TWOD-033, Z2-R-TWOD-102, Z2-R-TWOD-124, Z2-R-TWOD-125 |
| Z2-I-TWOD-04 | tilemaps | Z2-R-TWOD-024, Z2-R-TWOD-034, Z2-R-TWOD-035, Z2-R-TWOD-036, Z2-R-TWOD-037, Z2-R-TWOD-038, Z2-R-TWOD-039, Z2-R-TWOD-040, Z2-R-TWOD-041, Z2-R-TWOD-042, Z2-R-TWOD-043, Z2-R-TWOD-044, Z2-R-TWOD-045, Z2-R-TWOD-046, Z2-R-TWOD-088, Z2-R-TWOD-103, Z2-R-TWOD-104, Z2-R-TWOD-116, Z2-R-TWOD-118, Z2-R-TWOD-120, Z2-R-TWOD-121, Z2-R-TWOD-122, Z2-R-TWOD-123, Z2-R-TWOD-124, Z2-R-TWOD-125, Z2-R-TWOD-129, Z2-R-TWOD-132, Z2-R-TWOD-137, Z2-R-TWOD-141 |
| Z2-I-TWOD-05 | 2D animation | Z2-R-TWOD-047, Z2-R-TWOD-048, Z2-R-TWOD-049, Z2-R-TWOD-050, Z2-R-TWOD-051, Z2-R-TWOD-052, Z2-R-TWOD-053, Z2-R-TWOD-054, Z2-R-TWOD-055, Z2-R-TWOD-056, Z2-R-TWOD-057, Z2-R-TWOD-058, Z2-R-TWOD-059, Z2-R-TWOD-105, Z2-R-TWOD-116, Z2-R-TWOD-121, Z2-R-TWOD-122, Z2-R-TWOD-124, Z2-R-TWOD-125, Z2-R-TWOD-129, Z2-R-TWOD-130, Z2-R-TWOD-148 |
| Z2-I-TWOD-06 | 2D physics (selection ticket D-SEL-2DPHYS) | Z2-R-TWOD-005, Z2-R-TWOD-006, Z2-R-TWOD-041, Z2-R-TWOD-042, Z2-R-TWOD-060, Z2-R-TWOD-061, Z2-R-TWOD-062, Z2-R-TWOD-063, Z2-R-TWOD-064, Z2-R-TWOD-065, Z2-R-TWOD-066, Z2-R-TWOD-067, Z2-R-TWOD-068, Z2-R-TWOD-069, Z2-R-TWOD-070, Z2-R-TWOD-071, Z2-R-TWOD-072, Z2-R-TWOD-073, Z2-R-TWOD-074, Z2-R-TWOD-075, Z2-R-TWOD-076, Z2-R-TWOD-077, Z2-R-TWOD-078, Z2-R-TWOD-114, Z2-R-TWOD-116, Z2-R-TWOD-118, Z2-R-TWOD-119, Z2-R-TWOD-120, Z2-R-TWOD-121, Z2-R-TWOD-122, Z2-R-TWOD-123, Z2-R-TWOD-126, Z2-R-TWOD-127, Z2-R-TWOD-128, Z2-R-TWOD-129, Z2-R-TWOD-131, Z2-R-TWOD-137, Z2-R-TWOD-138, Z2-R-TWOD-139, Z2-R-TWOD-140, Z2-R-TWOD-142, Z2-R-TWOD-144, Z2-R-TWOD-145, Z2-R-TWOD-146, Z2-R-TWOD-147, Z2-R-TWOD-148, Z2-R-TWOD-149 |
| Z2-I-TWOD-07 | pixel-perfect cameras | Z2-R-TWOD-079, Z2-R-TWOD-080, Z2-R-TWOD-081, Z2-R-TWOD-082, Z2-R-TWOD-083, Z2-R-TWOD-084, Z2-R-TWOD-085, Z2-R-TWOD-086, Z2-R-TWOD-117 |
| Z2-I-TWOD-08 | pixel-perfect rendering | Z2-R-TWOD-030, Z2-R-TWOD-082, Z2-R-TWOD-083, Z2-R-TWOD-087, Z2-R-TWOD-088, Z2-R-TWOD-089, Z2-R-TWOD-090, Z2-R-TWOD-091, Z2-R-TWOD-092, Z2-R-TWOD-093, Z2-R-TWOD-114, Z2-R-TWOD-117, Z2-R-TWOD-133, Z2-R-TWOD-134, Z2-R-TWOD-135, Z2-R-TWOD-143 |
| Z2-I-TWOD-09 | 2D authoring in the editor | Z2-R-TWOD-033, Z2-R-TWOD-046, Z2-R-TWOD-055, Z2-R-TWOD-066, Z2-R-TWOD-093, Z2-R-TWOD-094, Z2-R-TWOD-095, Z2-R-TWOD-096, Z2-R-TWOD-097, Z2-R-TWOD-098, Z2-R-TWOD-099, Z2-R-TWOD-100, Z2-R-TWOD-101, Z2-R-TWOD-102, Z2-R-TWOD-103, Z2-R-TWOD-104, Z2-R-TWOD-105, Z2-R-TWOD-106, Z2-R-TWOD-107, Z2-R-TWOD-125 |
| Z2-I-TWOD-10 | the UI2D sample | Z2-R-TWOD-108, Z2-R-TWOD-109, Z2-R-TWOD-110, Z2-R-TWOD-111, Z2-R-TWOD-112, Z2-R-TWOD-113 |

## Excluded and deferred

| Capability | Classification | Rationale |
|---|---|---|
| 2D dynamic lighting (normal-mapped sprites, 2D shadows) | excluded | outside the scope list of Z2-D-078; canvas content is unlit |
| deformable skinned 2D meshes | excluded | outside the scope list of Z2-D-078; cutout skeletons serve Z2-I-TWOD-05 |
| stereo and XR presentation of 2D content | deferred to S7 | Z2-D-072; no TWOD requirement until that stage's XR work cites it |
| networked 2D play | deferred to S7 | Z2-D-073 |
| split-screen 2D play | excluded | Z2-D-076 |
| modding of 2D content | excluded | Z2-D-075 |
