# ADVGFX design: virtual texture import, cook and format

Section file of `Zenith2/Docs/Design/ADVGFX.md`. It specifies how a source image becomes a virtual texture: the import preset flag, the cook step `Cook_VirtualTexture`, the cooked container `.zvt` with every section, the reader's validation, the parser limits, the engine fallbacks, the cook report and the cook errors. The runtime that reads the file is `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md`. Byte order is little-endian, every field is written and read field by field (Z2-D-038), and every size, count and offset read from a file is checked with overflow-checked arithmetic before use ([Z2-N-ARCH-951]).

## Scope of this file

| Topic | Heading |
|---|---|
| choosing VT for a texture | "Import preset flag" |
| the cook step | "Cook pipeline", "Tile assembly", "Tile compression", "Determinism and derived-data keys" |
| the file | "The cooked virtual texture format" and its section headings |
| reading it | "Reader validation", "Parser limits" |
| missing VTs | "Engine fallbacks" |
| reporting | "Cook report entry", "Cook errors" |

## Import preset flag

<!-- assumption: pending CONTENT, GEOM -->
A texture becomes virtual through the import preset key `texture.virtual` (boolean, default false), which GEOM lists among the texture cook's preset keys (`Zenith2/Docs/Design/GEOM.md`, table of preset keys) and CONTENT's image importer reads: with the flag set, the importer declares the product with kind `ASSET_KIND_VIRTUAL_TEXTURE` instead of `ASSET_KIND_TEXTURE` and writes the same `geom.textureSource` intermediate (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`), so the GUID, the usage, the colour space and the address hint arrive exactly as for a streamed texture (row AV6). A material document names the product's GUID in its `textures` object like any texture; the material cook accepts a kind-18 GUID for a texture field of an `opaque` or `alphaTest` material whose `[TextureUsage]` matches (row AV3 (a)).
<!-- /assumption -->

Changing the flag re-imports the source and changes the product's kind; the sub-asset GUID is the importer's and stays the same (`Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`), so materials that name it need no edit.

## Cook pipeline

`Cook_VirtualTexture` is a ZenithCook step (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook steps"`) registered for kind 18, in `Zenith2/Tools/ZenithCook/Internal/VirtualTexture/Zenith_CookVirtualTexture.cpp`. It runs once per product per platform:

| Step | Action | Failure |
|---|---|---|
| 1 | read the item's `geom.textureSource` blob through `Zenith_CookContext::ReadIntermediate` and validate it with GEOM's `Zenith_ReadTextureSource` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"`) | GEOM's `GEOM_COOK_ERROR_TEXTURE_SOURCE`; a missing blob is the same error naming the product |
| 2 | check the shape rules of `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Virtual texture shape rules"` from the source header, before any mip or block buffer is allocated; the 16,384 extent cap equals GEOM's texture limit and CONTENT's `uCONTENT_IMAGE_MAX_EXTENT`, so every shape the importer admits is checked here and none larger reaches the step | `ADVGFX_COOK_ERROR_VT_USAGE`, `ADVGFX_COOK_ERROR_VT_EXTENT` |
| 3 | generate the full mip chain with GEOM's texture cook steps 3 to 5 (linear-light averaging for `COLOUR`, normal renormalisation, alpha coverage; `Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"`) | GEOM's errors |
| 4 | encode every mip whole with GEOM's step 6 (UASTC, transcoded to BC7 for Windows cooks and ASTC 4 × 4 for Android cooks, `threadCount = 1`) | GEOM's errors |
| 5 | for a clamp VT, encode the clamp rings of every page mip ("Tile assembly") with the same encoder settings | GEOM's errors |
| 6 | assemble every tile of every page mip from the encoded blocks ("Tile assembly") | — |
| 7 | compress every tile alone ("Tile compression") | COMPRESS's errors mapped to `ADVGFX_COOK_ERROR_TILE_COMPRESS` |
| 8 | split the chain: page mips 0 … M − 1 become tiles; mips M … end become the tail; the root mip's tiles go to the `ROOT` section, the others to `TILE_DATA` | — |
| 9 | write the `.zvt` (ASSET's container, "The cooked virtual texture format") and the cook-report entry | ASSET's writer errors |

<!-- assumption: pending GEOM -->
Steps 3 to 5 call GEOM's texture cook functions rather than duplicating them, as the boundary table of `Zenith2/Docs/Design/GEOM/Textures.md → "Virtual texturing boundary"` assigns ("GEOM's texture cook steps 1–6, reused by ADVGFX's tile cook"). ADVGFX assumes two functions in `Zenith2/Tools/ZenithCook/Internal/Textures/Zenith_TextureCook.h`: `Zenith_Result<Zenith_TextureMipChain, Zenith_CookError> Zenith_BuildTextureMipChain(const Zenith_TextureSourceView& xSource, const Zenith_TextureCookSettings& xSettings, Zenith_Arena& xArena)` (steps 3 to 5) and `Zenith_Result<Zenith_EncodedImage, Zenith_CookError> Zenith_EncodeTextureImage(const Zenith_TextureImageView& xImage, Flux_TextureUsage eUsage, Zenith_AssetPlatform ePlatform, const Zenith_TextureCookSettings& xSettings, Zenith_Arena& xArena)` (step 6 for one image: a mip or a ring), with `Zenith_EncodedImage` holding the block grid in row-major block order (row AV3 (d)).
<!-- /assumption -->

### Synthetic block-grid test path

`Zenith_VTCookTestAccess::CookFromBlockGrid` (`Zenith2/Tools/ZenithCook/Internal/VirtualTexture/Zenith_VTCookTestAccess.h`, a test-only friend of the cook step, reached from the cook tests and the ADVGFX fixture setup through ASSET's import test context) takes a shape and a deterministic generator of pre-encoded blocks (a page-ID pattern or a block gradient) and enters the pipeline at step 6, skipping steps 1 and 3 to 5: it costs tile assembly and compression only, so large shapes are cheap to cook in a test. Shape checks (step 2) still run. `Test_CookVirtualTexture_SyntheticBlockGridPath` requires its `.zvt` to pass every reader row and its tiles to equal a direct cut of the generated grid by the rules of "Tile assembly". The VT runtime tests' residency, streaming and page-table fixtures use this path; the real encoder runs only on the small fixtures of `Zenith2/Docs/Design/ADVGFX/Tests.md → "Fixtures"`.

## Tile assembly

A tile is cut from the encoded block grid of its mip, never encoded on its own, so the border blocks of a tile are byte-identical to the interior blocks of its neighbours and filtering across a page edge reads the same texels the neighbouring page shows.

| Element | Rule |
|---|---|
| block grid of mip m | `(W >> m) / 4 × (H >> m) / 4` blocks; for page mips at least 32 × 32 |
| tile (m, x, y) | 34 × 34 blocks: block columns `32x − 1 … 32x + 32`, rows `32y − 1 … 32y + 32` of mip m, in row-major order |
| a block outside the grid, wrap VT | taken from the grid modulo its extent in blocks (the texels across the VT edge) |
| a block outside the grid, clamp VT | taken from the clamp ring of mip m: the ring is the image `(W >> m) + 8` × `(H >> m) + 8` whose interior is mip m and whose 4-texel frame replicates the edge texels (corners replicate the corner texel), encoded whole in step 5, of which only the outermost block row and column are used |
| texel border | one block is 4 texels, which is `uFLUX_VT_BORDER_TEXELS` |
| tile bytes | 34 × 34 × 16 = 18,496 = `uFLUX_VT_PAGE_BYTES` |
| tile order | page mip ascending, then y ascending, then x ascending; tile index `i(m, x, y) = base(m) + y × pagesX(m) + x`, `base(m) = Σ_{k<m} pagesX(k) × pagesY(k)` |

<!-- informative -->
Encoding each mip once and cutting tiles from it costs one ring encode per page mip for clamp VTs and nothing for wrap VTs. Encoding each 136 × 136 window independently would make a border block depend on the window it was encoded in when the encoder's rate-distortion optimisation looks across blocks, which would put a visible seam at every page edge.
<!-- /informative -->

## Tile compression

| Element | Rule |
|---|---|
| unit | one tile, 18,496 raw bytes |
| codec and level | the asset's compression setting (`Zenith2/Docs/Design/COMPRESS.md → "Compression setting fragment"`), zstd by default |
| call | `Zenith_CompressSection(xCodec, tile bytes, iLevel, nullptr, xDestination, xScratch, xOptions)` (`Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"`); no dictionary |
| fallback | when the compressed form is not smaller, `Zenith_StoreSection` stores the tile with codec `NONE` |
| descriptor | the 24-byte `Zenith_CompressedSectionDesc` of the result goes into the tile's index entry, so every compressed byte range of the file has its own descriptor ([Z2-N-ARCH-951], COMPRESS's rule that each compressed range is described by one descriptor, `Zenith2/Docs/Design/COMPRESS.md → "How cooked formats embed the descriptor"`) |
| placement | tiles are concatenated in tile order inside their section, each at a 16-byte-aligned offset, gaps zero |

## Determinism and derived-data keys

- The step is single-threaded per product (the cook parallelises across products), the encoder runs with `threadCount = 1`, and tiles are compressed in tile order, so a clean and an incremental cook produce byte-identical `.zvt` files (`Test_CookVirtualTexture_Deterministic`).
- The derived-data key inputs are: the `geom.textureSource` blob bytes, the platform, the usage, the address hint, every encoder setting of the preset, the compression setting, `uADVGFX_VT_FORMAT_VERSION` and the versions of KTX-Software and of the compression library from `ThirdParty/manifest.json` (`Zenith2/Docs/Design/ASSET/DDC.md` for the key layout).

## The cooked virtual texture format

`.zvt` is ASSET's cooked container (`Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, "Section table") with magic `'ZVTX'` (bytes `5A 56 54 58`), kind 18 and format version `uADVGFX_VT_FORMAT_VERSION` = 1. Sections, in (kind, index) order:

| Section kind | Value | Index | Codec | Content |
|---|---|---|---|---|
| `ADVGFX_SECTION_VT_INFO` | 50 | 0 | `NONE` | the 64-byte info record |
| `ADVGFX_SECTION_VT_TILE_INDEX` | 51 | 0 | the asset's setting | one 40-byte entry per tile, in tile order |
| `ADVGFX_SECTION_VT_TAIL` | 52 | 0 | the asset's setting | the tail mips, mips in descending size order |
| `ADVGFX_SECTION_VT_ROOT` | 53 | 0 | `NONE` | the root mip's tiles, each compressed alone |
| `ADVGFX_SECTION_VT_TILE_DATA` | 54 | 0 | `NONE` | every other tile, each compressed alone |

The first four sections are read in one round at load; `TILE_DATA` is never read whole: the runtime range-reads single tiles from it, which is why it is stored `NONE` at the container level and each tile carries its own descriptor.

### Info section

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 1 | usage | `Flux_TextureUsage` 1 … 4 (`COLOUR`, `LINEAR`, `NORMAL`, `MASK`) |
| 1 | 1 | format | `Zenith_RHIFormat`: BC7 (sRGB for `COLOUR`) on Windows cooks, ASTC 4 × 4 (sRGB for `COLOUR`) on Android cooks |
| 2 | 1 | flags | bit 0 sRGB, bit 1 wrap addressing, bit 2 alpha-coverage scaled; bits 3–7 zero |
| 3 | 1 | w | log2 of the width, 8 … 14 |
| 4 | 1 | h | log2 of the height, 8 … 14, `|w − h| ≤ 3` |
| 5 | 1 | page mips M | `min(w, h) − 6` |
| 6 | 1 | tail mips T | `max(w, h) − M + 1` |
| 7 | 1 | border texels | 4 |
| 8 | 2 | tile texels | 128 |
| 10 | 2 | page texels | 136 |
| 12 | 4 | tile count | `Σ_{m<M} pagesX(m) × pagesY(m)` |
| 16 | 4 | root tile count | `2^|w − h|` |
| 20 | 4 | tile raw bytes | 18,496 |
| 24 | 16 | average colour | 4 × f32, linear, from the source's mip 0 (debug views, the editor's thumbnail) |
| 40 | 4 | mip bias | f32, the preset's `texture.virtualMipBias` (default 0, range [−1, 2]); copied into `Flux_VTRecord::m_fMipBias` |
| 44 | 20 | reserved | zero |

### Tile index section

One entry per tile, in tile order:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 8 | file offset | the tile's first stored byte, from the file start; inside `ROOT` for root-mip tiles, inside `TILE_DATA` otherwise; 16-byte aligned |
| 8 | 4 | stored check | low 32 bits of `Zenith_Hash64` over the stored bytes |
| 12 | 4 | reserved | zero |
| 16 | 24 | descriptor | the tile's `Zenith_CompressedSectionDesc` (codec, level, flags, dictionary ID 0, stored bytes, decoded bytes = 18,496) |

### Tail section

The tail is the VT's mips M … M + T − 1 as one ordinary block-compressed texture with one layer: mips in descending size order, each `ceil(w_l / 4) × ceil(h_l / 4) × 16` bytes, with `w_l = max(1, (W >> M) >> l)`. It is uploaded into a `Flux_PersistentReadOnlyTexture` at activation (`Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`). At most 43,792 bytes (the 16,384 × 2,048 case: 512 × 64 down to 1 × 1).

### Root section

The root mip's tiles, `root tile count` of them, each stored as in `TILE_DATA`, in tile order. The loader decodes each with its index entry's descriptor after ASSET has delivered the section (`NONE` at the container level, so delivery is a copy).

### Tile data section

Every tile of page mips 0 … M − 2, in tile order, each compressed alone, 16-byte aligned. A 16,384 × 16,384 VT, the largest, holds 21,844 tiles here (404,026,624 raw bytes) and one root tile in `ROOT`, far inside ASSET's 16 GiB file limit.

## Reader validation

`Flux_ReadVirtualTextureView` (header and section table) and `Flux_ReadVirtualTextureSections` (the four loaded sections) in `Zenith2/Engine/ZenithFlux/Internal/VirtualTexture/Flux_VTReader.cpp` run after ASSET's checks C1 … C12 (`Zenith2/Docs/Design/ASSET/Formats.md → "Checks"`) and apply these rows in order, returning the first failure as a `Flux_VirtualTextureError`:

| # | Check | Error |
|---|---|---|
| V1 | the five sections are present exactly once each, and no other section kind | `FLUX_VT_ERROR_CORRUPT` |
| V2 | `INFO` decoded bytes equal 64 | `FLUX_VT_ERROR_CORRUPT` |
| V3 | every info field in its rule's range; reserved bytes zero | `FLUX_VT_ERROR_CORRUPT` |
| V4 | the format equals the platform's format for the usage (the runtime refuses a Windows cook on Android and the reverse) | `FLUX_VT_ERROR_FORMAT_UNSUPPORTED` |
| V5 | M, T, tile count and root tile count equal the values derived from w and h | `FLUX_VT_ERROR_CORRUPT` |
| V6 | `TILE_INDEX` decoded bytes equal `40 × tile count` | `FLUX_VT_ERROR_CORRUPT` |
| V7 | `TAIL` decoded bytes equal the computed tail size | `FLUX_VT_ERROR_CORRUPT` |
| V8 | every index entry: reserved zero; descriptor decodes and validates (`Zenith_DecodeCompressedSectionDesc`); decoded bytes 18,496; offset 16-byte aligned; `[offset, offset + stored)` inside `ROOT` for the root mip and inside `TILE_DATA` otherwise | `FLUX_VT_ERROR_CORRUPT` |
| V9 | entries are in increasing offset order within each section and do not overlap | `FLUX_VT_ERROR_CORRUPT` |
| V10 | a root tile's stored check matches and it decodes to 18,496 bytes | `FLUX_VT_ERROR_CORRUPT_TILE` |

A non-root tile is verified when it is read (`Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile I/O and decode"`); a failure there marks that one page corrupt and never fails the asset. `Test_VirtualTexture_ReaderRejectsCorruption` mutates one fixture per row and requires its error; the fuzz target `Zenith_Fuzz_VirtualTextureFile` (`Zenith2/Engine/ZenithFlux/Tests/Fuzz/VirtualTextureFile.cpp`) parses arbitrary bytes with `Zenith_ParseCookedHeader` and then the reader, under `win-asan`, and never crashes.

## Parser limits

The `.zvt` limits, constants of `Flux_VTReader.h`, each tested at the limit and at limit + 1 ([Z2-N-ARCH-953]); the table joins `Zenith2/Docs/Architecture.md → "Security architecture"` (table "Parser limits") through cross-doc item X421 (b) and SEC's inventory through row AV7:

| Quantity | Limit | Constant |
|---|---|---|
| extent per axis | 256 … 16,384, powers of two | `uFLUX_VT_MIN_EXTENT_LOG2`, `uFLUX_VT_MAX_EXTENT_LOG2` |
| aspect | 8 : 1 | `uFLUX_VT_MAX_ASPECT_LOG2` |
| page mips | 8 | `uFLUX_VT_MAX_PAGE_MIPS` |
| tail mips | 10 | `uFLUX_VT_MAX_TAIL_MIPS` |
| tiles | 21,845 | `uFLUX_VT_MAX_TILES` |
| root tiles | 8 | `uFLUX_VT_MAX_ROOT_TILES` |
| tile stored bytes | COMPRESS's bound for 18,496 input bytes | `Zenith_GetCompressBound` |
| tail bytes | 43,792 | `uFLUX_VT_MAX_TAIL_BYTES` |
| tile index bytes | 873,800 | `40 × uFLUX_VT_MAX_TILES` |

`Test_VirtualTexture_LimitsAtAndPastLimit`.

## Engine fallbacks

Kind 18 is SUBSTITUTE in ASSET's kind table, so `Cook_VirtualTexture` declares engine fallbacks in `Zenith_CookStepDesc::m_axEngineFallbacks` (`Zenith2/Docs/Design/ASSET/Cook.md → "Engine fallbacks"`), cooked from `Zenith2/Engine/ZenithFlux/Content/VirtualTextures/`:

| Role | Logical path | Content (256 × 256, wrap) |
|---|---|---|
| 0 | `engine:/VirtualTextures/FallbackColour.png` | GEOM's `COLOUR` shipping fallback, white |
| 1 (usage `COLOUR` + 1, GEOM's per-usage convention) | the same file | white |
| 2 | `engine:/VirtualTextures/FallbackLinear.png` | (0.5, 0.5, 0.5, 1) |
| 3 | `engine:/VirtualTextures/FallbackNormal.png` | (0.5, 0.5, 1, 1), a flat normal |
| 4 | `engine:/VirtualTextures/FallbackMask.png` | (1, 0.5, 0, 1) |

Each fallback is itself a VT of the smallest shape (256 × 256: M = 2, four mip-0 tiles and one root tile), so a substituted VT streams like any other. The values are GEOM's shipping fallbacks (`Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"`).

## Cook report entry

The step appends one object to the cook report's `virtualTextures` array (`Zenith2/Docs/Design/ASSET/Formats.md → "Cook report"`): `guid`, `usage`, `width`, `height`, `pageMips`, `tailMips`, `tiles`, `rootTiles`, `rawBytes`, `storedBytes`, `compressionRatio`, `addressMode`, `platform`, `encoderSettings`. `Test_CookVirtualTexture_ReportEntry`.

## Cook errors

```cpp
// Zenith2/Tools/ZenithCook/Internal/VirtualTexture/Zenith_CookVirtualTexture.h
enum Zenith_VirtualTextureCookError : uint16
{
	ADVGFX_COOK_ERROR_VT_USAGE = 1,          // a usage other than COLOUR, LINEAR, NORMAL, MASK
	ADVGFX_COOK_ERROR_VT_EXTENT,             // not a power of two in 256 … 16,384, aspect above 8 : 1, or more than one layer
	ADVGFX_COOK_ERROR_TILE_COMPRESS,         // COMPRESS failed for a tile (its code in the detail)
	ADVGFX_COOK_ERROR_MIP_BIAS,              // texture.virtualMipBias outside [−1, 2]
};
```

Each maps onto ASSET's `COOK_ERROR_STEP_FAILED` with the ADVGFX code in the detail field, as ENV's step errors do, so no ASSET enumerator is added; the cook log names the product GUID, the source path, the rule and the offending value.
