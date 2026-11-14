# TWOD design: authoring, cooking and the 2D games

Section file of `Zenith2/Docs/Design/TWOD.md`. It specifies everything TWOD runs on the tool side: the sprite atlas importer and its packer, the five cook steps, the shared authoring functions in the `ZenithAuthoring` tool library, the scene content checks, the editor's 2D extension (sprite and atlas editor, tilemap painter, 2D animation editor, rig editor, physics authoring), the 2D project template content, the UI2D sample content and the `Game2D` flagship slice TWOD owns. Formats are `Zenith2/Docs/Design/TWOD/Formats.md`; the runtime algorithms are `Zenith2/Docs/Design/TWOD/Simulation.md` and `Zenith2/Docs/Design/TWOD/Rendering.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| where each tool-side file lives | "Tool-side layout" |
| the functions ZenithCook and ZenithEditor share | "ZenithAuthoring functions" |
| atlas import and packing | "Sprite atlas importer", "Atlas packing" |
| the cook steps | "Cook steps", "Cook step keys", "Cook diagnostics" |
| scene-level rules | "Scene content checks" |
| the editor | "Editor extension", "2D scene view mode", "Sprite and atlas editor", "Tilemap painter", "2D animation editor", "Rig editor", "2D physics authoring" |
| projects and games | "2D project template content", "UI2D sample content", "Game2D flagship slice" |
| the end-to-end flow | "2D authoring workflow" |

## Tool-side layout

| Path | Target | Holds | First stage |
|---|---|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringTwoD.h` | `ZenithAuthoring` (the one tool library of `Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`) | the root header of "ZenithAuthoring functions" | S3 |
| `Zenith2/Tools/Authoring/Internal/TwoD/` | `ZenithAuthoring` | `Zenith_TwoDDocumentTypes.cpp` (registration and canonical schemas), `Zenith_AtlasPacker.cpp`, `Zenith_TwoDCompile.cpp` (document → cooked sections for the five kinds), `Zenith_TwoDValidate.cpp` (the rules of `Zenith2/Docs/Design/TWOD/Formats.md → "Validation rules"`), `Zenith_AutoTile.cpp`, `Zenith_TwoDSceneChecks.cpp` | S3 (atlas, validation), S4 (the rest) |
| `Zenith2/Tools/Authoring/Tests/TwoD/` | `ZenithAuthoring_Tests` | `Zenith_SpriteAtlasPacker_Tests.cpp`, `Zenith_TwoDCompile_Tests.cpp`, `Zenith_TwoDDocuments_Tests.cpp`, `Zenith_TilesetAutoTile_Tests.cpp`, `Migration/<Type>/v1.json` and `v1.expected.json` | S3, S4 |
| `Zenith2/Tools/ZenithCook/Internal/TwoD/` | `ZenithCook` | `Zenith_TwoDCookSteps.h` (`Zenith_RegisterTwoDCookSteps`), `Zenith_TwoDImporters.h` (`Zenith_RegisterTwoDImporters`), the importer and the five step classes, each calling the authoring functions | S3 (atlas), S4 (the rest) |
| `Zenith2/Tools/ZenithCook/Tests/Zenith_TwoDCook_Tests.cpp` | `ZenithCook_Tests` | the cook tests of `Zenith2/Docs/Design/TWOD/Tests.md → "Cook and authoring tests"` | S3 |
| `Zenith2/Tools/ZenithEditor/Internal/2D/` | `ZenithEditor` | the 2D extension (`Zenith_TwoDEditorExtension.cpp`) and its panels, inspectors, gizmo providers, document handlers | S4 |
| `Zenith2/Tools/ZenithEditor/Tests/2D/` | `ZenithEditor_Tests` | the editor tests of `Zenith2/Docs/Design/TWOD/Tests.md → "Editor tests"` | S4 |

ZenithAuthoring's `DEPENDS` gains `Zenith2D` in the first TWOD diff that includes a Zenith2D header there (the dependency growth rule of `Zenith2/Docs/Design/EDIT.md → "Dependency growth rule"`; row TD8); ZenithCook already depends on every module and on ZenithAuthoring. Image decoding uses CONTENT's decoders (`Zenith2/Docs/Design/CONTENT/Importers.md → "Zenith_MediaDecoders header"`), reached only from ZenithCook's importer, so ZenithAuthoring does not depend on `ZenithContentCore` for TWOD.

## ZenithAuthoring functions

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringTwoD.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithCore/Zenith_DocumentTree.h"

/// @contract enum Zenith_TwoDAuthoringErrorCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side; values never reused.
/// @endcontract
enum Zenith_TwoDAuthoringErrorCode : uint16
{
	TWOD_AUTHORING_ERROR_INVALID_DOCUMENT = 1,   // a Formats.md "Validation rules" error; m_uDetail the rule number
	TWOD_AUTHORING_ERROR_PACK_FAILED,            // a frame does not fit, or more than 64 pages
	TWOD_AUTHORING_ERROR_LIMIT,                  // a Formats.md "Parser limits" count exceeded; m_uDetail the limit's row
	TWOD_AUTHORING_ERROR_MISSING_INPUT,          // a referenced asset's compile view was not supplied
	TWOD_AUTHORING_ERROR_OUT_OF_MEMORY,
};

/// @contract value Zenith_TwoDAuthoringError
/// Ownership: Plain value; m_szMessage points at static text or into the caller's scratch arena.
/// Thread affinity: IMMUTABLE once returned.
/// Invalidation: m_szMessage with the scratch arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TwoDAuthoringError
{
	const Zenith_TwoDAuthoringErrorCode m_eCode = TWOD_AUTHORING_ERROR_INVALID_DOCUMENT;
	const uint32 m_uDetail = 0;
	const char* const m_szMessage = nullptr;                     // the Formats.md message text with its placeholders filled
};

/// @contract value Zenith_AtlasPackFrame
/// Ownership: Plain value; one source frame of Zenith_AtlasPackInput; m_strName and m_xTexels are borrowed for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: Texels; m_uRowStride in bytes.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_AtlasPackFrame
{
	Zenith_StringView m_strName;                 // the frame name, after grid expansion
	uint32 m_uWidth, m_uHeight;                 // the untrimmed source rectangle
	uint32 m_uRowStride;                        // bytes per source row
	Zenith_ByteSpan m_xTexels;                   // RGBA8, straight alpha, rows from the top, the rectangle's first texel first
	float m_afPivot[2];                          // texels from the untrimmed top-left
	uint16 m_auBorder[4];
};

/// @contract value Zenith_AtlasPackInput
/// Ownership: Plain value; the frame span and every frame's texel span are borrowed for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Texels; m_fPixelsPerUnit in texels per metre.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_AtlasPackInput
{
	Zenith_Span<const Zenith_AtlasPackFrame> m_axFrames;   // in document order (sources, then grid cells)
	uint32 m_uPageSize;
	uint32 m_uPadding;
	uint32 m_uExtrude;
	bool m_bTrim;
	bool m_bPointFilter;
	float m_fPixelsPerUnit;
};

/// @contract value Zenith_AtlasPackResult
/// Ownership: Arrays in the caller's arena.
/// Thread affinity: IMMUTABLE once returned; members non-const: its producer default-constructs it and fills it member
///   by member.
/// Invalidation: With the arena.
/// Units: Texels.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_AtlasPackResult
{
	Zenith_Span<const Zenith_ByteSpan> m_axPages;          // RGBA8 page images, m_uPageExtent² each except the last (see "Atlas packing" step 6)
	Zenith_Span<const uint32> m_auPageWidth, m_auPageHeight;
	Zenith_ByteSpan m_xAtlasSections;                      // the INFO, FRAMES, NAME_INDEX and STRINGS sections of Formats.md "Cooked sprite atlas format"
};

/// @contract enum Zenith_TwoDDiagnosticLevel
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side; the values equal Zenith_CookDiagnosticLevel's WARNING and ERROR.
/// @endcontract
enum Zenith_TwoDDiagnosticLevel : uint8
{
	TWOD_DIAGNOSTIC_LEVEL_WARNING = 2,
	TWOD_DIAGNOSTIC_LEVEL_ERROR = 3,
};

/// @contract value Zenith_TwoDDiagnostic
/// Ownership: Plain value; the strings point into the caller's scratch arena.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: With the scratch arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TwoDDiagnostic
{
	Zenith_TwoDDiagnosticLevel m_eLevel;         // ERROR or WARNING (Formats.md "Validation rules" severity)
	uint16 m_uRule;                             // the rule number, or the scene check number + 100
	Zenith_StringView m_strPointer;              // the JSON pointer of the member ("/sources/3/pivot")
	Zenith_StringView m_strMessage;
};

/// @contract value Zenith_TwoDAtlasCompileView
/// Ownership: Plain value; the compile view of one referenced atlas, read from a cooked .zatlas or an open atlas document's pack
///   result; every span is borrowed for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: Texels for frame sizes; m_fPixelsPerUnit in texels per metre.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TwoDAtlasCompileView
{
	Zenith_AssetGUID m_xAtlas;
	Zenith_Span<const Zenith_StringView> m_astrFrameNames;      // in frame-index order
	Zenith_Span<const uint16> m_auSourceWidth, m_auSourceHeight;
	float m_fPixelsPerUnit;
	bool m_bPointFilter;
};

/// @contract value Zenith_TwoDRigCompileView
/// Ownership: Plain value; the compile view of one referenced rig (bone, slot and attachment names); every span is borrowed for
///   the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TwoDRigCompileView
{
	Zenith_AssetGUID m_xRig;
	Zenith_Span<const Zenith_StringView> m_astrBones, m_astrSlots, m_astrAttachments;
};

/// @contract value Zenith_TwoDCompileInputs
/// Ownership: Plain value; the compile views of every asset a document references; every span is borrowed for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a reference absent from the views is TWOD_AUTHORING_ERROR_MISSING_INPUT at the function that needs it.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TwoDCompileInputs
{
	Zenith_Span<const Zenith_TwoDAtlasCompileView> m_axAtlases;  // sorted by GUID
	Zenith_Span<const Zenith_TwoDRigCompileView> m_axRigs;       // sorted by GUID
	Zenith_AssetGUID m_xTileset;                                  // a tilemap's tileset; null otherwise
	uint32 m_uTilesetTileCount;
};

/// @contract functions Zenith_AuthoringTwoD
/// Ownership: Results are written into the caller's arena or array; nothing is retained between calls.
/// Thread affinity: ANY_THREAD; every function is a pure function of its arguments (Z2-D-019), so the cook and the editor's
///   preview produce bit-identical outputs for identical inputs.
/// Invalidation: None.
/// Units: Texels, metres, milliseconds as each argument states.
/// Errors: Zenith_TwoDAuthoringError; never an assert on document or image content ([Z2-N-ARCH-951]).
/// Version: since Z2 Stage 3 (registration, validation, packing, atlas compile); Stage 4 (the other compiles, auto-tiling,
///   scene checks). Tool-side; ZenithAuthoring is outside every Shipping link closure.
/// @endcontract
Zenith_Result<void, Zenith_TwoDAuthoringError> Zenith_RegisterTwoDDocumentTypes(Zenith_MigrationRegistry& xRegistry);
Zenith_Result<void, Zenith_TwoDAuthoringError> Zenith_ValidateTwoDDocument(const Zenith_DocumentTree& xDocument,
	const Zenith_TwoDCompileInputs& xInputs, Zenith_Arena& xScratch, Zenith_Array<Zenith_TwoDDiagnostic>& axOut);
Zenith_Result<Zenith_AtlasPackResult, Zenith_TwoDAuthoringError> Zenith_PackSpriteAtlas(const Zenith_AtlasPackInput& xInput,
	Zenith_Arena& xArena);
Zenith_Result<Zenith_ByteSpan, Zenith_TwoDAuthoringError> Zenith_CompileSpriteAnimation(const Zenith_DocumentTree& xDocument,
	const Zenith_TwoDCompileInputs& xInputs, Zenith_Arena& xArena);
Zenith_Result<Zenith_ByteSpan, Zenith_TwoDAuthoringError> Zenith_CompileTileset(const Zenith_DocumentTree& xDocument,
	const Zenith_TwoDCompileInputs& xInputs, Zenith_Arena& xArena);
Zenith_Result<Zenith_ByteSpan, Zenith_TwoDAuthoringError> Zenith_CompileTilemap(const Zenith_DocumentTree& xDocument,
	const Zenith_TwoDCompileInputs& xInputs, Zenith_Arena& xArena);
Zenith_Result<Zenith_ByteSpan, Zenith_TwoDAuthoringError> Zenith_CompileRig2D(const Zenith_DocumentTree& xDocument,
	const Zenith_TwoDCompileInputs& xInputs, Zenith_Arena& xArena);
// The seeded image generator ("Seeded image generator"): pixels of one migrated generator document, RGBA8, rows from the top.
struct Zenith_TwoDGeneratedImage { uint32 m_uWidth; uint32 m_uHeight; Zenith_ByteSpan m_xTexels; };
Zenith_Result<Zenith_TwoDGeneratedImage, Zenith_TwoDAuthoringError> Zenith_GenerateTwoDImage(const Zenith_DocumentTree& xDocument,
	Zenith_Arena& xArena);
// Auto-tiling: the tile index of terrain uTerrain for the 8-neighbour mask (Formats.md "Tileset document schema"); 0 when no rule matches.
uint32 Zenith_AutoTileIndex(const Zenith_DocumentTree& xTileset, uint32 uTerrain, uint8 uNeighbourMask);
// The scene content checks of "Scene content checks" over one resolved scene; appends one diagnostic per violation.
Zenith_Result<void, Zenith_TwoDAuthoringError> Zenith_CheckTwoDSceneContent(const Zenith_DocumentTree& xResolvedScene,
	Zenith_Arena& xScratch, Zenith_Array<Zenith_TwoDDiagnostic>& axOut);
```

- `Zenith_TwoDCompileInputs` holds the compile views of the referenced assets: the cook step builds them from `ReadDependencyOutput` of the referenced cooked files, the editor from the open documents (an atlas through `Zenith_PackSpriteAtlas`). A reference missing from the inputs is `TWOD_AUTHORING_ERROR_MISSING_INPUT`.
- `Zenith_CookDiagnosticLevel` is ZenithCook's (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook diagnostics"`); ZenithAuthoring cannot include a ZenithCook header (ZenithCook depends on ZenithAuthoring, and the layering is a DAG), so the TWOD header declares `Zenith_TwoDDiagnosticLevel` with the same values, and the cook step converts by value.
- Every document argument is CORE's `Zenith_DocumentTree` (`Zenith2/Docs/Design/CORE/Text.md → "JSON document tree"`) of a document migrated to its current version: the cook step's comes from `OpenSourceDocument`, the editor's from its open document; a scene check takes SCENE's resolved scene tree (prefabs expanded).

## Sprite atlas importer

The importer `twod.spriteAtlas` (`Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImporterDesc"`) claims the suffix `.spriteatlas.json`; `Zenith_RegisterTwoDImporters(Zenith_ImporterRegistry&)` registers it at cook-services item 4 after CONTENT's importers (`Zenith2/Docs/Design/ASSET/Import.md → "Importer registry"`). Its desc: version 1, settings version 1 with no settings (the document carries them), `m_bOutOfProcess` true (it decodes untrusted images), tool versions `stb_image`. `Import`:

1. Parse the document from `ReadSource()` through the migration registry (a document at an older schema version is migrated in memory, never written back); a validation error fails the import with `IMPORT_ERROR_SOURCE_INVALID` and the rule's message.
2. For each source in document order: `ReadInput(image)`; `Zenith_DetectImageSourceFormat` and `Zenith_DecodeImageSource`; a JPEG is refused (validation rule 4); a 16-bit image is converted to 8 bits by `(v × 255 + 32,767) / 65,535` with warning rule 13 for a point atlas. Grid sources are cut into cells; a cell outside the image is validation rule 5's error.
3. `Zenith_PackSpriteAtlas` ("Atlas packing").
4. `DeclareProduct("atlas", ASSET_KIND_SPRITE_ATLAS, true)`, which returns the GUID kept in the source's `.meta` (the importer cannot choose it); when that GUID differs from the document's `id` the import fails with `IMPORT_ERROR_SOURCE_INVALID` and the message `<path>: atlas id <id> differs from the .meta product GUID <guid>`, so every reference to the atlas by its document `id` names the product; and, per page i, `DeclareProduct("page/<i>", ASSET_KIND_TEXTURE, false)`; pages that disappear on reimport are dropped by ASSET's product reconciliation (`Zenith2/Docs/Design/ASSET/Import.md → "Reimport"`).
5. Per page: `WriteIntermediate(page, "geom.textureSource", …)` through GEOM's `Zenith_WriteTextureSource` (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`) with usage `FLUX_TEXTURE_USAGE_COLOUR_EXACT` (point atlases, row TD4) or `FLUX_TEXTURE_USAGE_COLOUR` (linear), layout `R8G8B8A8`, colour space sRGB, address hint clamp. TWOD joins GEOM's writer table (row TD4).
6. For the atlas product: `WriteIntermediate(atlas, "twod.atlasSections", m_xAtlasSections)` followed by the page GUIDs in page order (the cooked file's DEPENDENCIES section).

`COLOUR_EXACT` (requested from GEOM, row TD4) is `COLOUR` without block compression and with exactly one mip: the texels reach the GPU as `R8G8B8A8_SRGB` bit for bit, which the pixel exactness rules need (`Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"`). Linear atlases are block-compressed like every colour texture (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"`). At mip level L a texel averages a 2^L × 2^L block of mip 0, so an extrude border of e texels keeps a frame's edge texels free of its neighbours' colour exactly for the mips with 2^L ≤ e (mip 0 alone at the default e = 1, mips 0 and 1 at e = 2); deeper mips bleed by up to `2^L − e` texels and are accepted as linear atlases' documented filtering cost. Block compression encodes 4 × 4 texel blocks, so the packer aligns every padded rectangle of a linear atlas to 4-texel boundaries ("Atlas packing" step 3) and no block straddles two frames; point atlases are never block-compressed.

## Seeded image generator

<!-- elaboration: orchestrator ruling of 2026-10-17: TWOD owns the cook-time image generator its sample and flagship content needs, so no binary art is committed -->
A generator document (`*.generator.json`, schema `Formats.md → "Image generator document schema"`) describes one RGBA8 image as a seed and an ordered list of integer drawing operations. Two consumers read it, through one pure function so their pixels are identical:

- The atlas importer `twod.spriteAtlas` accepts a source object whose `generator` member names a generator document (a path relative to the atlas document's directory) instead of `image`: step 2 reads it with `ReadInput` (an import-input edge, so an edit of the generator re-imports the atlas), migrates and validates it, and calls `Zenith_GenerateTwoDImage` in place of the decode; grid cutting, packing and every later step are unchanged.
- The importer `twod.imageGenerator`, registered by `Zenith_RegisterTwoDImporters` beside `twod.spriteAtlas` (suffix `.generator.json`, version 1, settings version 1 with no settings, in process: it reads no untrusted binary), imports a generator document on its own as one texture product (`DeclareProduct("image", ASSET_KIND_TEXTURE, true)`, the product GUID equal to the document's `id` as the atlas importer requires of atlases) whose `geom.textureSource` intermediate carries the generated pixels with usage `FLUX_TEXTURE_USAGE_COLOUR_EXACT` when the document's `filter` is `point` and `FLUX_TEXTURE_USAGE_COLOUR` when `linear`, layout `R8G8B8A8`, colour space sRGB, address hint clamp, for backgrounds and UI art that is not packed into an atlas.

`Zenith_GenerateTwoDImage` (ZenithAuthoring, "ZenithAuthoring functions") allocates `width × height × 4` bytes in the caller's arena, clears them to transparent black and applies the operations in document order, each painting with straight-alpha source-over in integer arithmetic (a source alpha of 255 replaces the pixel): `out_a = a + (dst_a × (255 − a) + 127) / 255`, and per colour channel `out_c = 0` when `out_a = 0`, else `(src_c × a × 255 + dst_c × dst_a × (255 − a) + (out_a × 255) / 2) / (out_a × 255)`, all in unsigned 32-bit integers. Every random value is `SplitMix64(seed + k)` (the function of `Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"`), `k` the operation's own stream index defined per operation, so an image depends only on its document, never on the platform, the thread count or the order documents are imported in (Z2-D-019). No floating point is used.

| Operation | Members | Pixels |
|---|---|---|
| `fill` | `colour` | every pixel |
| `rect` | `x`, `y`, `w`, `h`, `colour` | the rectangle clipped to the image |
| `circle` | `cx`, `cy`, `r`, `colour` | pixels whose centre `(x + ½, y + ½)` satisfies `(2x + 1 − 2cx)² + (2y + 1 − 2cy)² ≤ 4r²` |
| `ramp` | `axis` (`x` or `y`), `from`, `to` | channel `c = from + (to − from) × i / (n − 1)` rounded toward zero, `i` the pixel index along the axis, `n` the image extent along it (`n` = 1 paints `from`) |
| `noise` | `cell` (1 … 256), `amplitude` (0 … 255), `colour` | a lattice of `⌈w / cell⌉ + 1` × `⌈h / cell⌉ + 1` values `v(i, j) = SplitMix64(seed + j × 65,536 + i) >> 56`; each pixel bilinearly interpolates the four surrounding lattice values in integers (weights in 1/`cell` steps, result `/ cell²` rounded toward zero) and paints `colour` with alpha `v × amplitude / 255` |
| `sheet` | `cellWidth`, `cellHeight`, `columns`, `rows`, `outline` | cell `(c, r)` filled with the opaque colour whose R, G, B are bytes 0, 1, 2 of `SplitMix64(seed + 1,000,000 + r × columns + c)`, then a one-texel `outline` border inside the cell |
| `blobTiles` | `tileWidth`, `tileHeight`, `fill`, `edge`, `edgeWidth` (1 … 8) | the 47 blob classes of `Formats.md → "Tileset document schema"` in a 8 × 6 grid of tiles in ascending reduced-mask order (the 48th tile empty): each tile filled with `fill`, then for every side whose edge bit is clear an `edgeWidth` band of `edge`, and for every corner whose corner bit is clear while both its sides are set an `edgeWidth × edgeWidth` notch of `edge` |

A colour is `"#RRGGBBAA"` (8 hexadecimal digits); the bytes of `SplitMix64` are numbered from the least significant. The test support's `Zenith_TwoDTestImageSheet(seed, cellW, cellH, cols, rows)` is defined as the `sheet` operation with `outline` `#000000FF`, and `Zenith_TwoDTestImageRamp(w, h)` as `ramp` along x from `#000000FF` to `#FFFFFFFF` (R = G = B = ⌊255 × x / (w − 1)⌋, so at w = 256 every 8-bit value appears in each channel), so tests may compare the two byte for byte.

## Atlas packing

`Zenith_PackSpriteAtlas`, deterministic MaxRects with the best-short-side-fit rule:

1. **Trim.** With `trim`, each frame's rectangle shrinks to the bounding box of texels with alpha > 0 (an all-transparent frame keeps a 1 × 1 rectangle at its top-left); the trim offset and the source size are kept so the pivot and the drawn quad do not move.
2. **Order.** Frames sort by (max(w, h) descending, w × h descending, name bytes ascending); the name is unique (validation rule 2), so the order is total.
3. **Place.** Each frame needs `(w + 2 × padding) × (h + 2 × padding)`, rounded up in each axis to a multiple of 4 for a linear atlas (and placed at a free-rectangle corner whose x and y are multiples of 4, the free rectangles' coordinates staying multiples of 4 because every split is); it goes into the first page (in creation order) whose free-rectangle list has a fit, choosing the free rectangle with the smallest `min(free.w − need.w, free.h − need.h)`, ties by the smaller long-side remainder, then the smaller (y, x); no rotation (a rotated frame would break pixel-exact flips). A frame no page fits opens a new page; a frame larger than a page is validation rule 5; page 65 is rule 6 (`TWOD_AUTHORING_ERROR_PACK_FAILED`).
4. **Split and prune.** The used rectangle splits every intersecting free rectangle into up to four maximal ones; free rectangles contained in another are removed, comparing in list order, so the list is identical on every run.
5. **Blit and extrude.** Texels are copied into the page at `(x + padding, y + padding)`; with `extrude` e, the frame's edge rows and columns are repeated e texels outward (corners repeat the corner texel); the rest of the page is (0, 0, 0, 0).
6. **Page extent.** Every page is `pageSize²` except the last, which shrinks to the smallest power of two in each axis that contains its used rectangles (minimum 64).
7. **Output.** Frame records in name-index order of the cooked format; the atlas sections are written field by field.

Complexity O(n · f) for n frames and f free rectangles (f stays below 4 n). `Test_SpriteAtlasPacker_Deterministic` packs a generated set of 1,000 frames (seeded sizes 1 … 128) twice and in shuffled document order and compares the bytes; `Test_SpriteAtlasPacker_NoOverlapAndPadding` asserts no two padded rectangles overlap and every frame is inside its page; `Test_SpriteAtlasPacker_ExtrudeReplicatesEdges` and `Test_SpriteAtlasPacker_TrimKeepsPivot` assert steps 5 and 1.

## Cook steps

`Zenith_RegisterTwoDCookSteps(Zenith_CookStepRegistry&, Zenith_CookServices&)` (`Zenith2/Docs/Design/ASSET/Cook.md → "Step registration functions"`) registers five `COOK_PHASE_ASSET` steps and one validator per kind (`RegisterValidator`, which runs the runtime reader of `Zenith2/Docs/Design/TWOD/Formats.md → "Cooked reader checks"` over each written file):

| Step ID | Kind | Reads | Writes | Out of process | Platforms |
|---|---|---|---|---|---|
| `twod.spriteAtlas` | `ASSET_KIND_SPRITE_ATLAS` | the `twod.atlasSections` intermediate | `.zatlas` | yes | all |
| `twod.spriteAnimation` | `ASSET_KIND_SPRITE_ANIMATION` | the `*.spriteanim.json` document (`OpenSourceDocument`), the cooked atlases and rigs it names (`ReadDependencyOutput`) | `.zsanim` | no (opens source documents) | all |
| `twod.tileset` | `ASSET_KIND_TILESET` | the `*.tileset.json` document; the cooked atlas | `.ztileset` | no | all |
| `twod.tilemap` | `ASSET_KIND_TILEMAP` | the `*.tilemap.json` document; the cooked tileset | `.ztilemap` | no | all |
| `twod.rig` | `ASSET_KIND_RIG_2D` | the `*.rig2d.json` document; the cooked atlas | `.zrig2d` | no | all |

- The four document kinds are imported by ASSET's `zenith-doc` passthrough importer, which declares one product of the document's kind with the document's `id` as GUID (row TD3 adds the four suffixes to its list); the steps read the document through `OpenSourceDocument` (`Zenith2/Docs/Design/ASSET/Cook.md → "Source documents in cook steps"`).
- Each step calls the matching ZenithAuthoring compile function, wraps its sections in the ASSET container through the context's cooked-file writer (compression per section, the DEPENDENCIES section from the references) and reports every diagnostic through `Zenith_CookContext::Report`.
- No step declares an engine fallback: the five kinds' default fallback policy is FAIL (row TD3); a missing atlas page falls back through the texture kind's own SUBSTITUTE policy and draws the white usage fallback (`Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour"`).
- Every step is deterministic: identical keys produce byte-identical output in a clean and an incremental cook (`Test_TwoDCook_Deterministic`).

## Cook step keys

Beyond the driver's key inputs, each step's `AddKeyInputs` adds, in this order:

| Step | Rows |
|---|---|
| `twod.spriteAtlas` | `twod.atlasSections` bytes' SHA-256; the step version |
| `twod.spriteAnimation`, `twod.tileset`, `twod.tilemap`, `twod.rig` | `twod.document` (the GUID) and `twod.document.sha256` (the digest of `OpenSourceDocument`); then, per referenced asset in first-use order, its GUID and the content hash of its cooked file |

Editing an image re-imports the atlas (an import-input edge from `ReadInput`), which changes `twod.atlasSections` and the page intermediates, so the atlas and its pages re-cook and every animation, tileset and rig naming the atlas re-cooks through its dependency hash; renaming a frame that an animation uses is then that animation's validation error rule 3, reported by its own item.

## Cook diagnostics

| Code | Level | Raised by | Meaning |
|---|---|---|---|
| `TWOD_COOK_DIAG_VALIDATION` (1) | error or warning, as the rule | every step and the importer | a rule of `Zenith2/Docs/Design/TWOD/Formats.md → "Validation rules"`; the message is the rule's |
| `TWOD_COOK_DIAG_PACK` (2) | error | importer | a frame does not fit or more than 64 pages |
| `TWOD_COOK_DIAG_LIMIT` (3) | error | every step | a parser limit exceeded |
| `TWOD_COOK_DIAG_MIXED_PPU` (4) | warning | `twod.spriteAnimation`, `twod.rig` | rule 14 |
| `TWOD_COOK_DIAG_TWO_PHYSICS_BODIES` (5) | error | the scene check | "Scene content checks" rule 1 |
| `TWOD_COOK_DIAG_SCENE_RULE` (6) | warning | the scene check | "Scene content checks" rules 2 to 6 |

Each diagnostic is reported through `Zenith_CookContext::Report(level, message)` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"`) with the message `TWOD<code> <JSON pointer>: <text>`, so the cook report and the editor show the code; after reporting every diagnostic of the item, a step that recorded an error returns `COOK_ERROR_STEP_FAILED` naming the asset, which fails the item (exit code 1), and warnings never change the exit code.

## Scene content checks

`Zenith_CheckTwoDSceneContent` runs over every resolved scene and prefab; the editor runs it on save and shows each result on its entity, and the scene cook step runs it per item once SCENE provides a component-check registration (row TD12); until then the editor check and the runtime rules below are the enforcement:

| # | Rule | Severity | Runtime behaviour if bypassed |
|---|---|---|---|
| 1 | an entity carries both `Zenith.Physics.RigidBody` and `Zenith.TwoD.RigidBody` | error (`TWOD_COOK_DIAG_TWO_PHYSICS_BODIES`) | the 2D body is not created (`Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D ingest"` step 3) |
| 2 | a `Zenith.TwoD.Collider` with no `Zenith.TwoD.RigidBody` on its entity or an ancestor | warning | the collider is a static body's shape on a static body created at the entity (the PHYS rule for colliders without bodies) |
| 3 | a `Zenith.TwoD.Joint` whose `BodyA`, or whose non-null `BodyB`, names an entity without a 2D body | warning | the joint is not created until both bodies exist |
| 4 | a `Zenith.TwoD.SpriteAnimator` without a `Zenith.TwoD.Sprite` or `Zenith.TwoD.Skeleton` | warning | the animator evaluates and writes nothing |
| 5 | two entities carry an enabled `Zenith.TwoD.Camera` on active `MAIN` cameras (`Zenith.Render.Camera`) with equal highest `Priority` | warning | RPROD's main-camera selection (`Flux_SelectMainCamera`) picks the lower simulation ID; the other's canvas is unused |
| 6 | a `Zenith.TwoD.Tilemap` entity whose world rotation is not the identity or whose world scale is not 1, or a map layer with collision whose parallax is not (1, 1) | warning | the map draws and collides axis-aligned at its world position (`twod.tilemap.transformed_delta`); a parallax layer generates no collision |

`Test_TwoDSceneChecks_EveryRule` builds one scene per rule in memory and asserts the diagnostic.

## Editor extension

`Zenith_TwoDEditorExtension` (`GetName` = `"2D"`) implements `Zenith_EditorExtension` (`Zenith2/Docs/Design/EDIT/Panels.md → "Extension interface"`) in `Zenith2/Tools/ZenithEditor/Internal/2D/`, one row of `Zenith2/Docs/Design/EDIT/Panels.md → "Extension table"`, ticketed by TWOD under the rules of `Zenith2/Docs/Design/EDIT/Panels.md → "Area-owned panels"`. `Register` makes:

| Registration | What |
|---|---|
| panel types | `TwoD.AtlasEditor` (multi-instance, one per `*.spriteatlas.json`), `TwoD.TilemapPainter` (multi, one per tilemap entity being painted), `TwoD.AnimationEditor` (multi, per `*.spriteanim.json`), `TwoD.RigEditor` (multi, per `*.rig2d.json`), `TwoD.TilePalette` (single) |
| document handlers | the five TWOD document types: open in their panel; validator `Zenith_ValidateTwoDDocument`; projection: a saved atlas, tileset, tilemap, animation or rig re-cooks through the cook server and hot-reloads in the play and edit worlds (`Zenith2/Docs/Design/TWOD/Simulation.md → "Animation hot reload"`, "Tilemap hot reload") |
| component inspectors | `Zenith.TwoD.Sprite` (atlas picker with frame thumbnails, sorting-layer names from `twod.sortingLayers`), `Zenith.TwoD.Camera` (reference resolution presets 320 × 180, 384 × 216, 480 × 270, 640 × 360, the computed integer scale for the game view's current size), `Zenith.TwoD.Collider` (shape editing handles), `Zenith.TwoD.Tilemap` ("Paint" button opening the painter) |
| gizmo providers | sprite bounds and pivot; collider outlines (box, circle, capsule, polygon, chain) with one-way arrows; joint anchors and limits; the camera's reference rectangle and safe area |
| bounds providers | sprite quad bounds, tilemap authored bounds (frame selected) |
| asset kind handlers | the five kinds: open the document, create a new document from the asset browser's "Create" menu, thumbnail (the atlas's first frame, the tileset's first tile, the animation's default state's first frame) |
| commands | `TwoD.NewAtlasFromImages` (selected images → a new atlas document), `TwoD.SliceGrid`, `TwoD.FrameSelection2D`, `TwoD.TogglePixelGrid` |
| preferences | `editor.twod.pixelGrid` (bool), `editor.twod.gridColour` |
| automation steps | `twod.paintTiles`, `twod.setTile` (the walkthrough's tile painting, through the painter's commands) |

Every edit goes through the document command model, so undo, redo and multi-user merge rules apply unchanged; no panel writes a world directly.

## 2D scene view mode

EDIT provides the orthographic 2D scene view (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Tools hosted for other areas"`, the TWOD row): the Front view with 2D navigation, pan with middle drag and zoom at the cursor. TWOD adds: the pixel grid (one line per reference pixel of the primary 2D camera when the zoom shows at least 4 screen pixels per reference pixel), snapping of moved sprites to that grid when `PixelSnap` is set, and the 2D camera's reference rectangle as an overlay. The game view of a play world draws through the same canvas path as the Player (`MAIN|EDITOR` kind bits of the canvas extension), so what the editor shows at an integer scale is pixel-identical to the Player at the same output size (`Test_TwoDEditor_GameViewMatchesPlayer`).

## Sprite and atlas editor

The `TwoD.AtlasEditor` panel edits one `*.spriteatlas.json` through EDIT's JSON document editor widget (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "JSON document editor"`) plus a canvas:

- the source list (add by dropping images from the asset browser; grid slicing with a live grid overlay; per-frame name, pivot and nine-slice border handles, pivots snapping to half texels);
- a live packed preview: the panel calls `Zenith_PackSpriteAtlas` in process on decoded previews of the sources (thumbnails of the asset browser's image cache at full size) and shows pages, page count and utilisation; the cooked result comes from the cook server after save, and the two agree because both call the same function on the same texels;
- validation results inline (rule number, message) on the member they concern.

## Tilemap painter

The `TwoD.TilemapPainter` paints a `*.tilemap.json` with the brush-stroke model of the terrain tools (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Sculpt and paint strokes"`, "Terrain undo"):

| Tool | Behaviour |
|---|---|
| brush | paints the selected tile (with flip and transpose toggles: keys X, Y, R rotate 90° by transpose + flip) under a 1 × 1 … 16 × 16 footprint |
| rectangle fill | fills a dragged rectangle |
| bucket fill | flood-fills the 4-connected region of equal cells, capped at 65,536 cells per click |
| eraser | writes 0 |
| picker | selects the cell under the cursor as the brush |
| terrain brush | paints a terrain: each painted cell and its 8 neighbours are re-resolved through `Zenith_AutoTileIndex` with the masks of the painted terrain |

- A stroke (press to release) is one document command holding the changed chunks' before and after cells, so undo restores exactly; the chunk's base64 is re-encoded on commit only.
- Layers are chosen in the panel; non-selected layers dim to 40 % opacity.
- The `TwoD.TilePalette` panel shows the tileset's tiles at integer zoom with their collision kinds overlaid.
- In play mode the painter edits the play world through `TwoD.SetTile` and `TwoD.FillTiles` commands (the same path a script uses), never the document, so play edits are discarded on stop like every play-world change.

## 2D animation editor

The `TwoD.AnimationEditor` edits one `*.spriteanim.json`: the state list and transition graph on EDIT's graph canvas widget (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Graph canvas"`), a flipbook clip's frames on the timeline widget (`Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Timeline"`) with per-frame duration and event markers, a skeletal clip's tracks with keys on the same timeline and the curve editor for linear keys, and a preview pane that runs the module's own evaluation (`Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation"`, "Cutout skeleton evaluation") in a preview world at the frame the timeline cursor shows. Parameters can be set live in the preview; a transition's condition evaluation is highlighted when it fires.

## Rig editor

The `TwoD.RigEditor` edits one `*.rig2d.json`: bones drawn as oriented triangles in the 2D view mode, created by dragging from a parent, moved and rotated with the transform gizmo; slots listed with draw order (drag to reorder); skins as tabs with an attachment picker per slot showing atlas frames. Bind-pose edits are document commands; the preview uses the module's evaluation.

## 2D physics authoring

Colliders and joints are components edited in the inspector with the gizmos of "Editor extension"; `twod.physics.layerNames` and `twod.physics.layerCollision` are edited in the project settings panel as a symmetric 32 × 32 check matrix; the polygon editor refuses a non-convex polygon at the vertex that breaks convexity; a chain collider is edited as a polyline with an open or loop toggle. In play mode the physics debug draw (`TwoD.PhysicsDebugDraw`) shows shapes, contacts and sleep state.

## 2D project template content

MOD owns the templates and their scaffold (`Zenith2/Docs/Design/MOD/ProjectCreation.md → "Templates"`); its 2D template selects no module, so its scene can name no TWOD component. TWOD defines the content the 2D template gains when its game selects the `2D` module (`ZenithNewGame create <Name> --template 2D --modules 2D`, or the wizard's 2D template with the 2D module checked), requested from MOD as row TD15:

- the `Camera` entity gains `Zenith.TwoD.Camera` with `ReferenceWidth` 320, `ReferenceHeight` 180, `PixelsPerUnit` 16, scale mode `INTEGER_LETTERBOX`, snap `TEXEL`, beside RPROD's `Zenith.Render.Camera` (which the main-camera selection needs), whose `FeatureMaskClear` (property 9) is set to `uTWOD_PURE_2D_FEATURE_CLEAR_MASK` so the view runs no 3D pass;
- `game.json`'s configuration layer gains `twod.sortingLayers` `["Background", "Default", "Foreground"]` and `twod.ySortLayers` `[1]`;
- no image, atlas or tilemap is written: the template stays text-only, and the 2D walkthrough ("2D authoring workflow") creates its content.

## UI2D sample content

The UI2D sample of Z2-D-078 is the workload game `Zenith2/Games/WorkloadUI2D` (W-UI2D), owned by UI (`Zenith2/Docs/Budgets.md → "W-UI2D"`; the UI side is UI's design). TWOD supplies its 2D layer, requested from UI and the W-UI2D workload scale ticket as row TD16:

| Content | Purpose |
|---|---|
| a canvas camera at 480 × 270, `INTEGER_LETTERBOX` | the pixel-perfect path under UI on every output size |
| a scrolling parallax background of 4 tilemap layers (parallax 0.25, 0.5, 0.75, 1.0) | tilemap chunk extraction and the chunk pool under camera motion |
| animated sprites at the count the W-UI2D scale ticket sets, half flipbooks and half on one cutout rig | sort, batch and animation CPU under UI load |
| dynamic 2D bodies at the count UI's Stage 1 workload scale ticket for W-UI2D commits; TWOD proposes 500 as its input (row TD16) | the physics rows' W-UI2D column and D-SEL-2DPHYS's W-UI2D scale |
| HUD and menus over the canvas | UI composited after `FLUX_EXTENSION_POINT_BEFORE_UI`, the ordering row TD2 creates |

Its images are generated at cook time from generator documents in the game's `Assets/` ("Seeded image generator"), so no binary image is committed, and its scripted path is the workload's.

## Game2D flagship slice

`Zenith2/Games/Game2D` is TWOD's flagship (`Zenith2/Docs/Architecture.md` lists it with TWOD as owner; W-Game2D is `Zenith2/Docs/Budgets.md → "W-Game2D"`). TWOD owns its definition and the "Stage 1 flagship acceptance ticket for Game2D" (producer of Z2-B-SHIP-022, which also sets W-Game2D's content scale):

| Property | Value |
|---|---|
| `game.json` modules | `2D`, `Audio`, `Gameplay`, `Online`, `UI` under the Box2D outcome of D-SEL-2DPHYS; under the Jolt outcome only, once PHYS's post-signoff patch has landed row TD10 (ruling TD10-CONDITIONAL), also `Physics` with backend `Jolt` and the configuration key `physics.planarWorld` true (the `2D` module then depends on "Physics (Jolt)"; row TD21); that `game.json` change is a J-path ticket, draft until the ruling |
| ECS backend | Custom |
| Genre | a side-scrolling platformer with 6 levels, a hub, an inventory and a dialogue system, so every TWOD feature is exercised in shipping content |
| Scale for D-SEL-2DPHYS | set by the Stage 1 flagship acceptance ticket for Game2D, which commits it with the slice's content scale; TWOD's proposed input to that ticket is 2,000 dynamic bodies, 20,000 static tile unit edges and 200 joints at the scene peak (row TD17). The harness reads the committed numbers, never these |
| Scale for rendering | the content scale ticket sets sprite and tile counts; the 2D rows are measured on its scripted playthrough |
| Flows | accessibility (remapping, text scale, colour-blind palette through the sprite tint table) and localization, owned by UI and GAME and exercised in this game |
| Content | text documents only: every image is a generator document turned into pixels at cook time ("Seeded image generator"); no committed binary art |

The Game2D content scale (the slice's level, sprite, tile, chunk and 2D physics counts) is committed by the Stage 1 flagship acceptance ticket for Game2D in `Zenith2/Docs/Research/Workloads/W-Game2D.md`, the shape [Z2-N-BUDGET-195] gives the workload scale files (row TD17; `Zenith2/Docs/ContentSpecs.md` keeps its fixed section list of [Z2-N-BUDGET-410] and gains nothing), and its playthrough script is the W-Game2D scripted path.

## 2D authoring workflow

The walkthrough a human runs at Stage 4 acceptance (and the editor automation rehearses in `Test_TwoDEditor_WorkflowRehearsal`):

1. Create a 2D game with the 2D module (`ZenithNewGame create Sample2D --template 2D --modules 2D`) and open it.
2. Import two generated PNGs (a 4 × 2 character sheet, a 16-tile terrain sheet), create an atlas from them, slice the grid, set pivots.
3. Create a tileset from the terrain frames with collision kinds and one terrain rule set; paint a 64 × 32 level with the terrain brush.
4. Create an animation document with Idle and Run states and a Speed parameter.
5. Add a player entity with Sprite, SpriteAnimator, RigidBody (dynamic) and a capsule Collider; add a behaviour that sets Speed and applies jumps through `TwoD.ApplyImpulse`.
6. Play in editor; paint tiles while playing; stop; save; cook; run the Player and see the identical pixel-perfect output at 1080p (scale 6).

Each step's timing is a row of the editor walkthrough measurements (EDIT's), and its scripted rehearsal is green in the `Test` step.
