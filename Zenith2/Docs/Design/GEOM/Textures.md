# GEOM design: textures, sampler policy and texture streaming

Section file of `Zenith2/Docs/Design/GEOM.md`. It specifies texture usage classes, the texture cook (mips, BC7 and ASTC through KTX-Software), the cooked texture container `.ztex`, the sampler policy, the runtime texture resource, and texture streaming by reallocate-and-swap. The normative sources are `Zenith2/Docs/Rendering/GpuResources.md → "Texture streaming swap"` ([Z2-N-REND-554]), `Zenith2/Docs/Rendering.md → "Static sampler table"`, `Zenith2/Docs/Rendering.md → "Required format features"` and the shedding mechanisms of `Zenith2/Docs/Rendering/GpuResources.md → "Responses to memory pressure"`. Virtual texturing is ADVGFX's ("Virtual texturing boundary").

## Scope of this file

| Topic | Heading |
|---|---|
| what kinds of texture exist and how each is stored | "Texture usage classes" |
| the cook | "Texture cook pipeline" |
| the cook's mip and encode functions, which ADVGFX reuses | "Texture cook functions" |
| the cook's input blob, written by importers | "Texture source image intermediate" |
| the cooked file | "The cooked texture format" |
| which sampler a texture uses | "Sampler policy" |
| runtime types | "Flux_Texture header" |
| which mips are resident | "Texture residency model", "Required-mip computation" |
| the per-texture state machine | "Streaming state machine" |
| the swap, step by step | "Reallocate-and-swap realisation" |
| replacing a whole chain from CPU bytes | "Texture replace from memory" |
| sharing copy-queue bandwidth | "Streaming upload arbitration" |
| VT | "Virtual texturing boundary" |
| missing textures | "Fallback textures" |
| tests | "Texture tests" |

## Texture usage classes

<!-- elaboration -->
| Class (`Flux_TextureUsage`) | Content | Windows format | Android format | Colour space | Streamed |
|---|---|---|---|---|---|
| `FLUX_TEXTURE_USAGE_COLOUR` | albedo, emissive, decal colour | `ZENITH_RHI_FORMAT_BC7_UNORM_SRGB` | `ZENITH_RHI_FORMAT_ASTC_4X4_UNORM_SRGB` | sRGB, straight alpha | yes |
| `FLUX_TEXTURE_USAGE_LINEAR` | linear colour data (lookup ramps authored as materials) | `BC7_UNORM` | `ASTC_4X4_UNORM` | linear | yes |
| `FLUX_TEXTURE_USAGE_NORMAL` | tangent-space normal maps: X in R, Y in G, B and A unused; Z reconstructed as `sqrt(saturate(1 − x² − y²))` | `BC7_UNORM` | `ASTC_4X4_UNORM` | linear | yes |
| `FLUX_TEXTURE_USAGE_MASK` | packed masks (ORM: occlusion R, roughness G, metallic B; alpha free) | `BC7_UNORM` | `ASTC_4X4_UNORM` | linear | yes |
| `FLUX_TEXTURE_USAGE_HEIGHT` | terrain height tiles (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"`) | `R16_UNORM`, uncompressed | `R16_UNORM` | linear | yes |
| `FLUX_TEXTURE_USAGE_HOLE_MASK` | terrain hole masks | `R8_UNORM` | `R8_UNORM` | linear | yes |
| `FLUX_TEXTURE_USAGE_SPLAT_INDEX` | terrain splat indices: four `uint8` palette indices per texel, read with texel `Load` as `uint(round(x × 255))` per lane, which is exact because an UNORM8 texel decodes to k / 255 | `R8G8B8A8_UNORM`, uncompressed | `R8G8B8A8_UNORM` | linear, never filtered | no: the whole chain is the tail |
| `FLUX_TEXTURE_USAGE_SPLAT_WEIGHT` | terrain splat weights: four `uint8` weights per texel | `R8G8B8A8_UNORM`, uncompressed | `R8G8B8A8_UNORM` | linear | no: the whole chain is the tail |
| `FLUX_TEXTURE_USAGE_COLOUR_EXACT` | point-sampled sprite atlas pages (TWOD): colour that must reach the GPU bit for bit | `R8G8B8A8_UNORM_SRGB`, uncompressed, one mip | `R8G8B8A8_UNORM_SRGB` | sRGB, straight alpha | no: the one mip is the tail |

- The usage class is declared by the import preset (CONTENT) or, for an image another area's importer generates, by that importer in the header of the cook's input blob ("Texture source image intermediate"), and recorded in the `.ztex`; the texture cook never infers it from a file name.
- The two splat classes answer ENV's request EN2 (b) (`Zenith2/Docs/Design/ENV/Terrain.md → "Cooked top-four reduction"`). Both use `R8G8B8A8_UNORM`, a format every backend already supports for sampling (`Zenith2/Docs/Rendering.md → "Required format features"`), instead of an integer format no Rendering table lists: an index texel is decoded exactly with `Load` and a multiply by 255. Their extent is at most 512 per axis (`GEOM_COOK_ERROR_TEXTURE_LIMIT` above it), so a whole chain stays resident with the asset (at most 1,398,100 bytes; 349,524 bytes for ENV's 256 × 256 tiles), which is why they are not streamed: no mesh part binds them, so the required-mip walk never raises them. They are bound through ENV's terrain tile table, never through a material field, so no `[TextureUsage]` attribute names them and the material reader refuses them in a binding record.
- ENV's packed detail density (`Zenith2/Docs/Design/ENV/Bake.md → "Splat textures"`: per present tile painting a detail type, one 128 × 128 `R8G8B8A8_UNORM` image of the product `texture:tile/<x>_<z>/detailDensity`) is a `SPLAT_WEIGHT` texture, and GEOM needs no class for it: the class's four raw `uint8` channels, integer-mean chain (8 mips, 128 … 1), whole-chain residency and 512-texel limit fit it unchanged. What each channel means (channel k the density of the terrain's detail type k, 0 where the tile has no map) is ENV's; GEOM stores, decimates and binds the channels as raw values and never reads them. One rule follows from the reuse: the class's fallback (1, 0, 0, 0) is a weight vector, not a density, so a consumer whose channels are not weights binds no texture, and reads density 0, while `Flux_TextureResource::IsFallbackBound()` is true (loading, not yet tail-resident, or FAILED), rather than binding `GetSRV()`, which returns that fallback then ("Fallback textures").
- `COLOUR_EXACT` answers TWOD's request TD4 (A5 row TwB3; `Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"`). It is `COLOUR` with exactly one mip and no block compression: the cook stores the source texels unchanged, so a point atlas's texels are the source bytes on both platforms. Its extent is at most 4,096 per axis, TWOD's largest `pageSize` (`GEOM_COOK_ERROR_TEXTURE_LIMIT` above it). The whole image is the tail and stays resident with the asset, at most 64 MiB. TWOD binds the pages through its own page table with its point sampler, never through a material field, so no `[TextureUsage]` attribute names the class and the material reader refuses it in a binding record, as it refuses the splat classes.
- Dimensions: 2D and 2D array (layers ≤ 2,048); cube maps and 3D textures of lighting and colour grading are LIGHT's formats, not GEOM textures. ENV's 3D cloud-noise volumes (the rest of EN2 (b)) are therefore not a GEOM usage class: a `.ztex` holds 2D and 2D-array images alone, and a 3D volume is cooked by its owner (cross-area row GM30). Width and height ≤ 16,384, mips ≤ 15 ([Z2-N-ARCH-953] table "Parser limits").

<!-- assumption: pending core-doc item X295 (TWOD row TD5) -->
`COLOUR_EXACT` needs the sampled feature of `R8G8B8A8_UNORM_SRGB` on every backend. TWOD's row TD5 asks for that row in `Zenith2/Docs/Rendering.md → "Required format features"` (core half X295), and GEOM's class assumes it.
<!-- /assumption -->

## Texture cook pipeline

| Step | Action |
|---|---|
| 1 | read the item's `geom.textureSource` intermediate ("Texture source image intermediate"): 8- or 16-bit RGBA from CONTENT's image importer, 8-bit RGBA atlas pages from TWOD's importer, or the single-channel 16-bit heights, 8-bit hole masks and RGBA8 splat and density images ENV's importer generates, each with its declared usage, colour space and address hint |
| 2 | validate: finite, dimensions within the limits, non-zero; a non-multiple-of-4 dimension is allowed (blocks at the edge are padded by edge replication) |
| 3 | generate the mip chain. `COLOUR`, `LINEAR`, `NORMAL`, `MASK`: level n+1 has `max(1, floor(extent_n / 2))` per axis; each texel is the average of its 2 × 2 footprint in level n (3-wide at an odd edge, like the HiZ fold), computed in linear light for `COLOUR` (sRGB decoded with the IEC 61966-2-1 curve, filtered in double, re-encoded), in the stored space otherwise. `HEIGHT`, `HOLE_MASK`: point decimation, no averaging — the source extent must be `2^n + 1` per axis (the terrain cook writes 257, n = 8; otherwise `GEOM_COOK_ERROR_TEXTURE_LIMIT`), mip l has `((extent_0 − 1) >> l) + 1` texels per axis for l = 0 … n (the last mip has 2), and mip l texel (i, j) is mip 0 texel (i·2^l, j·2^l) copied bit for bit, so every mip keeps the tile border (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"`). `SPLAT_INDEX`: level n+1 has `max(1, extent_n >> 1)` texels per axis and mip l texel (i, j) is mip 0 texel (i·2^l, j·2^l) copied bit for bit, so every texel of every mip is an authored index quadruple. `SPLAT_WEIGHT`: level n+1 has `max(1, extent_n >> 1)` texels per axis and each channel of each texel is `(s + c / 2) / c` in integer arithmetic, s the sum of that channel over the texel's footprint of c texels in level n (c = 4 inside, 2 or 1 at an extent of 1). `COLOUR_EXACT`: no chain; level 0 alone |
| 4 | normal maps: each averaged XY is renormalised as a unit vector with its reconstructed Z before storing |
| 5 | alpha coverage: for a texture the preset marks `alphaCoverageCutoff` (materials of the `alphaTest` domain), each mip's alpha is scaled by the factor, found by bisection over [0, 4] in 16 steps, that makes the fraction of texels above the cutoff equal level 0's |
| 6 | encode `COLOUR`, `LINEAR`, `NORMAL` and `MASK` through KTX-Software: `ktxTexture2_Create`, `ktxTexture2_CompressBasisEx` with UASTC (`uastc = KTX_TRUE`, pack level from the preset's `quality`, RDO lambda from `uastcRdoLambda`, normal-map mode for `NORMAL`, `threadCount = 1` for determinism), then `ktxTexture2_TranscodeBasis` to `KTX_TTF_BC7_RGBA` for Windows cooks and `KTX_TTF_ASTC_4x4_RGBA` for Android cooks; the transcoded blocks are extracted per level and layer |
| 7 | `HEIGHT`, `HOLE_MASK`, `SPLAT_INDEX`, `SPLAT_WEIGHT` and `COLOUR_EXACT` are stored raw (no block compression); `COLOUR_EXACT` stores the source texels byte for byte |
| 8 | compress each file section through COMPRESS: `Zenith_CompressSection` with the codec and level of the asset's compression setting (zstd by default, `Zenith2/Docs/Design/COMPRESS.md → "Compression setting fragment"`), storing NONE when compression does not help |
| 9 | write the `.ztex` and the texture's cook-report entry (`textures`: GUID, usage, extent, mips, tail mip, bytes per platform, the encoder settings); publish `geom.textureAverageColour`, the info record's average colour as four little-endian `f32` (16 bytes), which the material step reads for HLOD proxy colours (`Zenith2/Docs/Design/GEOM/Meshes.md → "Merge recipes"`) |

- The cook step is deterministic: KTX-Software runs single-threaded per texture and the cook parallelises across textures, so a clean and an incremental cook give byte-identical `.ztex` (`Test_CookTexture_Deterministic`).
- The runtime never links KTX-Software: its manifest usage is tools (`Zenith2/Docs/Architecture.md → "Third-party dependencies"`), and the cooked container holds GPU-ready blocks.

<!-- informative -->
UASTC is used as the single intermediate because it transcodes to both targets: to ASTC 4×4 essentially losslessly, and to BC7 with a small loss. A separate direct BC7 encoder and ASTC encoder would give slightly higher BC7 quality at the cost of two encoders to pin and test; that trade-off is design ruling G6 of `Zenith2/Docs/Design/GEOM.md → "Design rulings owned by GEOM"`.
<!-- /informative -->

## Texture cook functions

<!-- elaboration: answers A3 row AV3 (d) -->
Steps 3 to 5 and step 6 of "Texture cook pipeline" are two functions, which the texture step calls and ADVGFX's virtual-texture cook step reuses (`Zenith2/Docs/Design/ADVGFX/VTCook.md → "Cook pipeline"`). There is one implementation, so a VT and a streamed texture of the same source get the same mips and blocks:

```cpp
// Zenith2/Tools/ZenithCook/Internal/Textures/Zenith_TextureCook.h
struct Zenith_TextureCookSettings
{
	Flux_TextureUsage m_eUsage = FLUX_TEXTURE_USAGE_COLOUR;   // the blob's declared usage, else the preset's texture.usage
	uint8 m_uQuality = 0;                                     // texture.quality: the UASTC pack level
	float m_fUastcRdoLambda = 0.0f;                            // texture.uastcRdoLambda
	float m_fAlphaCoverageCutoff = -1.0f;                      // texture.alphaCoverageCutoff; negative: no coverage scaling
};

struct Zenith_TextureImageView
{
	uint32 m_uWidth = 0;
	uint32 m_uHeight = 0;
	uint32 m_uLayers = 1;
	uint8 m_uTexelLayout = 0;                                 // the geom.textureSource layout codes 1 … 4
	bool m_bSrgb = false;
	Zenith_ByteSpan m_xTexels;                                 // layer-major, rows from the top, as the source blob
};

struct Zenith_TextureMipChain { Zenith_Span<const Zenith_TextureImageView> m_axMips; };   // mip 0 first; spans in the arena

struct Zenith_EncodedImage
{
	Zenith_RHIFormat m_eFormat;                                // BC7 or ASTC 4×4, UNORM or sRGB
	uint32 m_uBlocksX = 0;
	uint32 m_uBlocksY = 0;
	uint32 m_uLayers = 1;
	Zenith_ByteSpan m_xBlocks;                                 // 16 bytes per block, row-major block order, layers in order
};

Zenith_Result<Zenith_TextureCookSettings, Zenith_CookError> Zenith_ReadTextureCookSettings(
	const Zenith_TextureSourceView& xSource, const Zenith_ResolvedSettings& xPreset);
Zenith_Result<Zenith_TextureMipChain, Zenith_CookError> Zenith_BuildTextureMipChain(const Zenith_TextureSourceView& xSource,
	const Zenith_TextureCookSettings& xSettings, Zenith_Arena& xArena);
Zenith_Result<Zenith_EncodedImage, Zenith_CookError> Zenith_EncodeTextureImage(const Zenith_TextureImageView& xImage,
	Flux_TextureUsage eUsage, Zenith_AssetPlatform ePlatform, const Zenith_TextureCookSettings& xSettings, Zenith_Arena& xArena);
```

- `Zenith_ReadTextureCookSettings` resolves the usage rule of "Texture source image intermediate" and reads the four preset keys. It fails with `GEOM_COOK_ERROR_TEXTURE_SOURCE` exactly where the step does.
- `Zenith_BuildTextureMipChain` is steps 3 to 5 for the usage. Each level is in the source's texel layout, exactly the bytes the step's mips hold. Step 2's limits fail with `GEOM_COOK_ERROR_TEXTURE_LIMIT`.
- `Zenith_EncodeTextureImage` is step 6 for one image: a whole mip, or any image of the same layout, such as ADVGFX's clamp rings. KTX-Software runs with `threadCount = 1`, transcoding to BC7 for `ASSET_PLATFORM_WINDOWS` and to ASTC 4×4 for `ASSET_PLATFORM_ANDROID`, in the sRGB form for `COLOUR`. A usage that step 7 stores raw fails with `GEOM_COOK_ERROR_TEXTURE_ENCODE`, as does an encoder failure.
- The functions are pure and keep nothing. Every result lives in `xArena`.

`Test_CookTexture_FunctionsMatchStep` requires the `.ztex` mips and blocks of the 1,024² `COLOUR` gradient and the 512² `NORMAL` fixtures to equal the functions' output byte for byte, on both platforms. `Test_CookTexture_EncodeRejectsRawUsage` requires a `HEIGHT` image, a `COLOUR_EXACT` image and a zero-width image to fail with their errors and allocate nothing.

## Texture source image intermediate

The texture step reads exactly one intermediate blob per texture product, `geom.textureSource`, through `Zenith_CookContext::ReadIntermediate` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"`); a product without it fails with `GEOM_COOK_ERROR_TEXTURE_SOURCE`. GEOM owns the blob's format, as it owns the intermediate mesh its mesh step reads (`Zenith2/Docs/Design/ASSET/Import.md → "Intermediates"`), and every importer that declares an `ASSET_KIND_TEXTURE` product writes it with `Zenith_ImportContext::WriteIntermediate` (`Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`):

| Writer | Products | Usage and layout it writes |
|---|---|---|
| CONTENT's image importer | authored images | usage 0 (the preset's `texture.usage`), `R8G8B8A8` or `R16G16B16A16`, the preset's colour space |
| ENV's importer `env.terrain`, stage `Environment.TerrainGeometry` (`Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"`) | `texture:tile/<x>_<z>/height`, `texture:tile/<x>_<z>/holes` | `HEIGHT`, `R16`, 257 × 257 (`Zenith_TerrainGeometryCookOutput::m_auHeightSamples`); `HOLE_MASK`, `R8`, 257 × 257 (`m_auHoleSamples`); linear, clamp |
| the same importer, stage `Environment.TerrainSplat` (`Zenith2/Docs/Design/ENV/Bake.md → "Splat textures"`) | `texture:tile/<x>_<z>/splatIndex`, `…/splatWeight`, `…/detailDensity` | `SPLAT_INDEX` and `SPLAT_WEIGHT`, `R8G8B8A8`, 256 × 256; `SPLAT_WEIGHT`, `R8G8B8A8`, 128 × 128; linear, clamp |
| TWOD's importer `twod.spriteAtlas` (`Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"`; A3 row TD4) | one product per atlas page | `COLOUR_EXACT` for a point atlas or `COLOUR` for a linear one, `R8G8B8A8`, at most 4,096 × 4,096, sRGB, clamp |

Layout (32-byte header, then the texels; little-endian, written field by field, Z2-D-038):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `'ZTSI'` (0x4953545A) |
| 4 | 2 | blob version, `uGEOM_TEXTURE_SOURCE_VERSION` = 1 |
| 6 | 1 | usage: 0 for the preset's `texture.usage`, else a `Flux_TextureUsage` value 1 … 9 |
| 7 | 1 | texel layout: 1 `R8`, 2 `R16`, 3 `R8G8B8A8`, 4 `R16G16B16A16` (1, 2, 4 or 8 bytes per texel) |
| 8 | 4 | width (u32) |
| 12 | 4 | height (u32) |
| 16 | 4 | layers (u32, 1 … 2,048) |
| 20 | 1 | colour space: 0 linear, 1 sRGB |
| 21 | 1 | address hint: 0 wrap, 1 clamp, 0xFF the preset's `texture.addressHint` |
| 22 | 2 | reserved, zero |
| 24 | 8 | texel byte count (u64): width × height × layers × bytes per texel |
| 32 | — | the texels: layer-major, then rows from the top, texel (i, j) of layer k at `((k × height + j) × width + i) × bytes per texel`, each channel little-endian |

- A non-zero usage or address hint is the writing importer's declaration and wins; one that differs from a preset value set explicitly for the product is `GEOM_COOK_ERROR_TEXTURE_SOURCE`, as is usage 0 when the preset sets no usage.
- Allowed layouts: `COLOUR`, `LINEAR`, `NORMAL` and `MASK` take `R8G8B8A8` or `R16G16B16A16`; `HEIGHT` takes `R16`; `HOLE_MASK` takes `R8`, every byte 0 or 255; `SPLAT_INDEX`, `SPLAT_WEIGHT` and `COLOUR_EXACT` take `R8G8B8A8`. sRGB is allowed for `COLOUR` and `COLOUR_EXACT` alone, and `COLOUR_EXACT` requires it, with one layer.
- A wrong magic or version, a non-zero reserved field, a layout or colour space not allowed for the usage, a byte count that disagrees with the header or the blob's size, or a hole byte other than 0 and 255 is `GEOM_COOK_ERROR_TEXTURE_SOURCE`, naming the product GUID, the source path of the importer's source and the field; extents are step 2's (`GEOM_COOK_ERROR_TEXTURE_LIMIT`).
- GUID ownership: a product's GUID is the one `DeclareProduct` returned to the writing importer, ASSET's sub-asset identity of that importer's source and product key (`Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`). The importer owns the key and keeps it stable across reimports; GEOM's step never mints, renames or derives a GUID: it writes the `.ztex` under the item's GUID (`MakeWriterDesc`), so the GUIDs other cooked data store, such as the height and hole tables of `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`, name the cooked textures directly.
- The blob's bytes, header included, are the step's first derived-data key input (`Zenith2/Docs/Design/GEOM.md → "Cook steps and derived-data keys"`), so an unchanged image re-cooks from the DDC when its source re-imports.
- `Zenith2/Tools/ZenithCook/Internal/Textures/Zenith_TextureSource.h` declares `uGEOM_TEXTURE_SOURCE_VERSION`, `Zenith_Result<void, Zenith_CookError> Zenith_WriteTextureSource(const Zenith_TextureSourceDesc& xDesc, Zenith_ByteSpan xTexels, Zenith_Array<uint8>& auOut)` (the writer every importer calls) and `Zenith_Result<Zenith_TextureSourceView, Zenith_CookError> Zenith_ReadTextureSource(Zenith_ByteSpan xBlob)` (the step's validating reader); `Zenith_TextureSourceDesc` and `Zenith_TextureSourceView` hold the header's fields as members, and the view's texel span points into the blob. Both are tool-side plain values with contract blocks of kind `value` (thread affinity `IMMUTABLE`, version since Z2 Stage 2).

`Test_CookTexture_SourceIntermediate` asserts every rule above.

## The cooked texture format

`.ztex` is ASSET's cooked container (`Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`) with magic `'ZTEX'` (0x5845545A), kind `ASSET_KIND_TEXTURE` and format version 1. ASSET places sections in (kind, index) order, so the info record and the tail section come first and the initial load reads one contiguous range; each streamed mip is its own section, so the streamer reads exactly one range per mip.

<!-- elaboration -->
| Section kind | Value | Index | Content |
|---|---|---|---|
| `GEOM_SECTION_TEXTURE_INFO` | 30 | 0 | the info record |
| `GEOM_SECTION_TEXTURE_TAIL` | 31 | 0 | mips `tailFirstMip … mipCount − 1`, every layer, mips in descending size order, layers in order within a mip |
| `GEOM_SECTION_TEXTURE_MIP` | 32 | the mip level | one streamed mip, every layer; after the tail, in ascending mip level (ASSET's table order) |

Info record (48 bytes): usage u8; format u8 (`Zenith_RHIFormat`); dimension u8 (0 2D, 1 2D array); flags u8 (bit 0 sRGB, bit 1 alpha-coverage scaled, bit 2 streamable, bit 3 render target: "Render-target textures"; bits 4–7 zero); width u32; height u32; layers u32; mip count u8; tail first mip u8; address hint u8 (0 wrap, 1 clamp); reserved u8; average colour 4 × f32 (linear, used by debug views and the VT fallback); reserved u32 to 48 bytes.

| Rule | Value |
|---|---|
| tail first mip | the smallest mip index t such that mips t … mipCount − 1 together occupy at most `uGEOM_TEXTURE_TAIL_BYTES` = 64 KiB per layer (t is at most mipCount − 1, so the smallest mip is always in the tail); a texture with t = 0 is not streamable. `SPLAT_INDEX`, `SPLAT_WEIGHT` and `COLOUR_EXACT` always have t = 0 and flag bit 2 clear, whatever their size |
| mip extent | `HEIGHT` and `HOLE_MASK`: `((w − 1) >> l) + 1` × `((h − 1) >> l) + 1`, with w and h of the form `2^n + 1` and mip count n + 1; `SPLAT_INDEX` and `SPLAT_WEIGHT`: `max(1, w >> l)` × `max(1, h >> l)` with w, h ≤ 512, a full chain and t = 0; `COLOUR_EXACT`: one mip of w × h ≤ 4,096 × 4,096, one layer, format `R8G8B8A8_UNORM_SRGB`, t = 0; every other usage: `max(1, w >> l)` × `max(1, h >> l)`; the reader rejects any other extent or mip count for the usage (`FLUX_GEOMETRY_ERROR_CORRUPT_SECTION`) |
| mip byte size | `ceil(w / 4) × ceil(h / 4) × 16 × layers` for BC7 and ASTC 4×4; `w × h × bytesPerTexel × layers` for raw formats, w and h the mip's extent from the row above |
| validation | the header, section table, section checks and decoding are ASSET's (checks C1–C12 of `Zenith2/Docs/Design/ASSET/Formats.md → "Checks"`); `Flux_ReadTextureView` and `Flux_ReadTextureInfo` run the kind-specific rows as mesh files do ("Reader validation" of `Zenith2/Docs/Design/GEOM/Meshes.md`): magic, kind and format version of the view as a defence, the sections present (one info, one tail, one mip section per streamed level), usage 1 … 9, sizes equal to the computed size ([Z2-N-ARCH-953]), extent and mip limits, the tail rule, the format allowed for the usage and platform (`FLUX_GEOMETRY_ERROR_CORRUPT_SECTION`, `FLUX_GEOMETRY_ERROR_VERSION_MISMATCH`, `FLUX_GEOMETRY_ERROR_FORMAT_UNSUPPORTED`) |
| fuzz | `Zenith_Fuzz_TextureFile` (`ZenithFlux/Tests/Fuzz/TextureFile.cpp`), parsing arbitrary bytes with `Zenith_ParseCookedHeader` before the reader |

## Sampler policy

Samplers are the sixteen static entries of `Zenith2/Docs/Rendering.md → "Static sampler table"` ([Z2-N-REND-057]); no sampler is created at runtime. The policy decides which entry each material texture uses.

<!-- elaboration -->
| Usage | Address hint wrap | Address hint clamp |
|---|---|---|
| `COLOUR`, `LINEAR`, `NORMAL`, `MASK` in `opaque`, `alphaTest`, `transparent` materials | `uFLUX_SAMPLER_ANISO_WRAP` (5) | `uFLUX_SAMPLER_ANISO_CLAMP` (6) |
| the same usages in `decal` materials | `uFLUX_SAMPLER_ANISO_CLAMP` (6) | `uFLUX_SAMPLER_ANISO_CLAMP` (6) |
| `HEIGHT`, `HOLE_MASK` (terrain) | none: read with texel `Load` at an explicit mip, never through a sampler (`Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement"`) | none |
| `SPLAT_INDEX`, `SPLAT_WEIGHT` (terrain) | not a material texture: ENV's terrain material reads indices with `Load` and chooses the weights' static sampler itself (`Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module"`) | none |

| Override name in a material document's `samplers` | Index |
|---|---|
| `pointClamp` | 0 |
| `pointWrap` | 1 |
| `linearClamp` | 2 |
| `linearWrap` | 3 |
| `linearMirror` | 4 |
| `anisoWrap` | 5 |
| `anisoClamp` | 6 |
| `linearClampNoMip` | 9 |
| `pointClampBorderBlack` | 10 |
| `linearClampBorderWhite` | 11 |

- Resolution order: the document's override, else the usage and domain default with the texture's address hint; the result is written into the texture field's sampler companion (`Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"`).
- The shadow comparison sampler (7) and the reserved entries (8, 12–15) are not selectable by materials; naming one is `GEOM_COOK_ERROR_MATERIAL_SAMPLER`.
- Anisotropy is fixed per capability tier by the static table (16 desktop, 8 mobile); a streaming mip bias never changes the sampler: a texture whose top mips are not resident simply has fewer mips, so every sampler clamps to the resident chain.

## Flux_Texture header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_TextureResource.h
#pragma once
#include "ZenithRHI/Zenith_RHIShaderInterface.h"
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithFlux/Flux_GeometryErrors.h"

enum Flux_TextureUsage : uint8
{
	FLUX_TEXTURE_USAGE_COLOUR = 1, FLUX_TEXTURE_USAGE_LINEAR, FLUX_TEXTURE_USAGE_NORMAL, FLUX_TEXTURE_USAGE_MASK, FLUX_TEXTURE_USAGE_HEIGHT, FLUX_TEXTURE_USAGE_HOLE_MASK,
	FLUX_TEXTURE_USAGE_SPLAT_INDEX, FLUX_TEXTURE_USAGE_SPLAT_WEIGHT, FLUX_TEXTURE_USAGE_COLOUR_EXACT,
};

/// @contract value Flux_TextureID
/// Ownership: Plain value naming a loaded texture in Flux_TextureManager; the asset handle keeps it alive.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: Stale after retirement; resolution fails with FLUX_GEOMETRY_ERROR_STALE_ID.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Flux_TextureID
{
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
	constexpr bool IsNull() const { return m_uGeneration == 0; }
	friend constexpr bool operator==(Flux_TextureID, Flux_TextureID) = default;
};

/// @contract value Flux_TextureResidency
/// Ownership: Plain value copied out of the texture.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None; a snapshot.
/// Units: Mip levels; bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_TextureResidency
{
	uint8 m_uResidentFirstMip = 0;     // the finest resident mip
	uint8 m_uRequestedFirstMip = 0;    // the streamer's current target
	uint8 m_uTailFirstMip = 0;
	uint8 m_uMipCount = 0;
	uint64 m_ulResidentBytes = 0;
	bool m_bSwapInFlight = false;       // a streaming change or a whole-chain replacement is not yet published
	bool m_bReplaced = false;           // the chain came from ReplaceChainFromMemory; cleared by a reload or recovery
};

/// @contract type Flux_TextureResource
/// Ownership: Created by Flux_TextureManager when the asset system loads a .ztex; shared through
///   Zenith_AssetHandle<Flux_TextureResource>; it owns its current Flux_PersistentReadOnlyTexture and at most one
///   pending replacement; both retire through deferred deletion.
/// Thread affinity: ANY_THREAD for the const members.
/// Invalidation: GetSRV() changes when a streaming swap or a whole-chain replacement publishes; a descriptor obtained earlier stays valid until the
///   retirement of the frames that could reference it ([Z2-N-REND-488]).
/// Units: Texels; mips; bytes.
/// Errors: load failures through IsFailed(); nothing asserts on file data.
/// Version: since Z2 Stage 2.
/// @endcontract
class Flux_TextureResource
{
public:
	Flux_TextureID GetID() const;
	Flux_TextureUsage GetUsage() const;
	Zenith_RHIFormat GetFormat() const;
	uint32 GetWidth() const;
	uint32 GetHeight() const;
	uint32 GetLayerCount() const;
	bool IsStreamable() const;
	bool IsFailed() const;
	Flux_TextureResidency GetResidency() const;
	Zenith_RHIDescriptorHandle GetSRV() const;   // the published texture's; the usage fallback's while loading or failed
	bool IsFallbackBound() const;                 // GetSRV() is the usage fallback's: not tail-resident, or FAILED
	bool IsRenderTarget() const;                  // info flag bit 3 ("Render-target textures")
	Flux_TextureHandle GetRenderTargetHandle() const;   // the writable texture of a RESIDENT render target; null otherwise
};

/// @contract value Flux_TextureChainDesc
/// Ownership: Plain value; the mip spans are borrowed for the duration of ReplaceChainFromMemory, which copies them.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Texels; bytes.
/// Errors: Flux_TextureManager::ReplaceChainFromMemory validates it ("Texture replace from memory").
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_TextureChainDesc
{
	Flux_TextureUsage m_eUsage = FLUX_TEXTURE_USAGE_HEIGHT;   // must equal the texture's: HEIGHT, HOLE_MASK, SPLAT_INDEX or SPLAT_WEIGHT
	uint32 m_uWidth = 0;                                     // mip 0 extent, equal to the texture's
	uint32 m_uHeight = 0;
	Zenith_Span<const Zenith_ByteSpan> m_axMips;              // one span per mip, finest first; rows from the top, tightly packed
};

/// @contract type Flux_TextureManager
/// Ownership: Owned by Flux_Renderer; owns every Flux_TextureResource and the fallback textures.
/// Thread affinity: ANY_THREAD for Resolve and GetFallback; mutations, ReplaceChainFromMemory included, on the
///   Flux.BuildGraph node (RENDER_SUBMIT_THREAD).
/// Invalidation: A pointer from Resolve is valid until the end of the current render frame.
/// Units: Bytes.
/// Errors: Flux_GeometryError. ReplaceChainFromMemory: SHUTTING_DOWN, DEVICE_LOST, STALE_ID, MISSING_DEPENDENCY,
///   INVALID_DESC (m_uDetail 0 usage, 1 extent or layers, 2 mip count or a mip's size, 3 the chain rule), PEAK_REFUSED,
///   OUT_OF_MEMORY; each leaves the texture unchanged and allocates nothing ("Texture replace from memory").
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Flux_TextureManager
{
public:
	const Flux_TextureResource* Resolve(Flux_TextureID xID) const;
	Zenith_RHIDescriptorHandle GetFallbackSRV(Flux_TextureUsage eUsage) const;
	uint32 GetLoadedTextureCount() const;
	uint64 GetResidentBytes() const;
	// Replace a raw texture's whole mip chain from CPU bytes by reallocate-and-swap; published on a following frame.
	Zenith_Result<void, Flux_GeometryError> ReplaceChainFromMemory(Flux_TextureID xTexture,
		const Flux_TextureChainDesc& xChain);
};

// Zenith2/Engine/ZenithFlux/Flux_TextureStreaming.h
/// @contract enum Flux_UploadClass
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Flux_UploadClass : uint8
{
	FLUX_UPLOAD_CLASS_BLOCKING = 1,     // a load a scene or cell activation waits on
	FLUX_UPLOAD_CLASS_MATERIAL,
	FLUX_UPLOAD_CLASS_MESH,
	FLUX_UPLOAD_CLASS_TEXTURE_STREAM,
	FLUX_UPLOAD_CLASS_VT_TILE,
	FLUX_UPLOAD_CLASS_COUNT,
};

/// @contract value Flux_TextureStreamingSettings
/// Ownership: Plain value; filled from configuration keys (Zenith2/Docs/Design/GEOM.md "Diagnostics and profiling",
///   table "Configuration keys").
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes; mips; instances.
/// Errors: Flux_TextureStreamer::Create rejects zero budgets (FLUX_GEOMETRY_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_TextureStreamingSettings
{
	uint64 m_ulPoolTargetBytes = 0;          // Z2-B-GEOM-002 Texture residency pool
	uint64 m_ulUploadBytesPerFrame = 0;      // Z2-B-GEOM-003 Streaming upload bytes per frame
	uint32 m_uInstancesPerFrame = 65536;     // required-mip evaluation budget
	uint32 m_uMaxRequestsInFlight = 256;
	sint32 m_iTierMipBias = 0;               // the quality tier's bias
	float m_fGuardBandScale = 1.1f;           // frustum enlargement for the required-mip pass
	float m_fNearRadiusMetres = 5.0f;         // instances this close count whatever the frustum
};

/// @contract type Flux_TextureStreamer
/// Ownership: Owned by Flux_Renderer; created after Flux_TextureManager and destroyed before it.
/// Thread affinity: RENDER_SUBMIT_THREAD. Update runs on the Flux.BuildGraph node; its required-mip pass fans out as
///   sub-jobs of that node; I/O completions and decode results arrive through bounded queues.
/// Invalidation: None.
/// Units: Bytes; mips.
/// Errors: none returned from Update; per-texture failures move the texture to its fallback and count
///   Flux.Textures.StreamFailures.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Flux_TextureStreamer
{
public:
	void SetSettings(const Flux_TextureStreamingSettings& xSettings);
	void SetPressureMipBias(sint32 iBias);           // 0, +1 (warning), +2 (critical) — the shedding mechanisms
	void SetPoolTargetScale(float fScale);            // 1.0, or 0.75 at warning ([Z2-N-REND-506] action 1)
	void Update(const Flux_ViewList& xViews, Flux_RenderGraph& xGraph);   // once per frame, before Flux_AddStreamingUploads
	// A requirement from a system that binds textures outside material parts (terrain tiles); stated every frame before
	// Update, clamped to the texture's tail first mip, merged into the frame's minimum.
	void SetExternalRequirement(Flux_TextureID xTexture, uint32 uMip);
	uint64 GetResidentBytes() const;
	uint32 GetRequestsInFlight() const;
};

/// @contract type Flux_StreamingUploader
/// Ownership: Owned by Flux_Renderer; the one owner of the StreamingUpload passes
///   (Zenith2/Docs/Rendering/PerPass/Common.md "StreamingUpload").
/// Thread affinity: RENDER_SUBMIT_THREAD.
/// Invalidation: None.
/// Units: Bytes per frame.
/// Errors: none; a request that does not fit this frame waits in its class queue.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2 (meshes, materials); texture streaming and VT classes from Stage 4.
/// @endcontract
class Flux_StreamingUploader
{
public:
	uint64 GetBytesThisFrame() const;
	uint64 GetBytesQueued(Flux_UploadClass eClass) const;
	// One request into a class's FIFO, served by "Streaming upload arbitration": ADVGFX's VT tiles in VT_TILE and its
	// activation uploads in BLOCKING. Flux.BuildGraph node only. FLUX_UPLOAD_ERROR_QUEUE_FULL when the class queue is full;
	// FLUX_UPLOAD_ERROR_TOO_LARGE for a request above Z2-B-RHI-006 that is not splittable.
	Zenith_Result<Flux_UploadOperation, Flux_UploadError> Enqueue(Flux_UploadClass eClass,
		const Flux_UploadRequestDesc& xDesc);
	// The VT_TILE class limit in tiles per frame (Z2-B-ADVGFX-002, set by ADVGFX when it enables virtual texturing; 0 while
	// it is disabled) and the floor derived from it, ceil(uTilesPerFrame / 4). Flux.BuildGraph node only.
	void SetVirtualTileBudget(uint32 uTilesPerFrame);
	uint32 GetVirtualTilesThisFrame() const;   // VT_TILE requests handed to the ring this frame
};
```

The classes of `Flux_UploadClass` are scheduled as "Streaming upload arbitration" specifies.

## Texture residency model

<!-- elaboration -->
| Rule | Value |
|---|---|
| always resident | the tail (mips `tailFirstMip … mipCount − 1`), loaded with the asset by `Flux_TextureKindLoader` (`Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"`): `BeginLoad` runs `Flux_ReadTextureView`, copies the view's section entries of the streamed mips (offset, stored check, descriptor) into the texture for the streamer's range reads, and requests the info and tail sections in one round; `OnSectionsLoaded` runs `Flux_ReadTextureInfo`, pushes the texture onto the load queue and calls `Activate(pTexture)` before any drain runs (the activation order of `Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"`); the drain creates the texture and `RequestUpload`s the tail; when the upload completes the staging is released and the loader calls `ReportGpuResident`, and a texture creation or tail upload that fails is `ReportGpuFailed` with `FLUX_GEOMETRY_ERROR_OUT_OF_MEMORY` or `FLUX_GEOMETRY_ERROR_UPLOAD_FAILED` (residency FAILED: the texture binds its usage's fallback). Until then the texture binds its usage's fallback ("Fallback textures"), and a texture is drawable as soon as its tail is resident. `Cancel`, `Unload`, device-recovery loads and `DropCpuCopies` (which returns 0) follow the mesh rows of `Zenith2/Docs/Design/GEOM/Meshes.md → "Mesh residency state machine"`; a recovery load restarts streaming from the tail |
| streamed | mips `0 … tailFirstMip − 1`; the resident range is always a suffix `[r, mipCount − 1]` of the chain, never a hole |
| budget | the sum of streamed bytes over every texture (tails included) stays at most `pool target = Z2-B-GEOM-002 × scale`, scale 0.75 at warning ([Z2-N-REND-506] action 1); the mip bias rises by 1 at warning and 2 at critical (actions 5 and 8), and at critical textures outside the current visible set drop to their tails |
| priority | `priority = (r_resident − r_required) × 1,000,000 + screen coverage in pixels of the nearest user, capped at 999,999`; increases are granted in descending priority, decreases taken in ascending priority (a texture above its requirement first, then the least recently needed) |
| governor | the streamer is part of the rendering memory consumer (`Zenith2/Docs/Architecture.md → "Memory governor"`); `GetResidentBytes` is its streamed-texture share |

## Required-mip computation

Each frame, in `Flux_TextureStreamer::Update`, for a window of at most `m_uInstancesPerFrame` GPU-scene instances (round-robin over the instance table, so every instance is visited within `ceil(instances / window)` frames), and for each streaming view v (main, editor and texture views; at most 4 per frame, in view-list order):

```
for each instance i in the window, each section s of its current LOD, each streamable texture T bound by the section's material:
	c, r   = instance bounds sphere in world space
	if not (sphere intersects v's frustum scaled by m_fGuardBandScale) and distance(v.camera, c) > m_fNearRadiusMetres + r: continue
	d      = max(distance(v.camera, c) − r, 0.01)
	w      = 2 d / (g_v × H_v)                          // metres per screen pixel; g = P[1][1], H the render height
	ρ      = the part's UV density for T's UV channel × instance max scale (metres per UV unit)
	if ρ == 0: required = 0
	else:  tpp = max(T.width, T.height) × w / ρ          // texels per pixel along the texture's larger axis
	       required = tpp ≤ 1 ? 0 : floor(log2(tpp))
	required = clamp(required + m_iTierMipBias + pressureBias, 0, T.tailFirstMip)
	T.frameRequired = min(T.frameRequired, required)
```

- A texture's requirement is the minimum over the window's users; a texture not seen for 120 frames (`uGEOM_TEXTURE_UNSEEN_FRAMES`) requires its tail.
- Terrain height and hole tiles are bound by no mesh part, so the instance walk never visits them: `Flux_TerrainRenderer` states their requirement each frame through `SetExternalRequirement` ("Node selection" of `Zenith2/Docs/Design/GEOM/Meshes.md`), which enters the same per-texture minimum; an external requirement not restated for 120 frames lapses like an unseen texture.
- Materials record each texture's UV channel and the mesh parts their UV densities (`Zenith2/Docs/Design/GEOM/Meshes.md → "Bounds and UV density"`), so the computation needs no GPU feedback; GPU feedback is VT's.
- The pass is a `ParallelFor` over the window writing per-texture minima with a fixed-order reduction, so the result is independent of the worker count.

## Streaming state machine

<!-- elaboration -->
| State | Event | Next | Action |
|---|---|---|---|
| `STABLE(r)` | requirement t < r (increase) and the budget and in-flight limit allow | `RESERVING(r→t)` | — |
| `STABLE(r)` | requirement t > r (decrease), or budget eviction picks it | `RESERVING(r→t)` | — |
| `RESERVING` | `Zenith_MemoryGovernor::ReserveTransitionPeak(TRANSITION_PEAK_TEXTURE_SWAP, new bytes)` succeeds | `ALLOCATED` | create the new `Flux_PersistentReadOnlyTexture` with mips `[t, mipCount − 1]` (not resident, own descriptor) |
| `RESERVING` | the reservation is refused (`MEMORY_ERROR_PEAK_REFUSED`) | `STABLE(r)` | retry next frame; `Flux.Textures.PeakDeferred` ([Z2-N-ARCH-815]) |
| `ALLOCATED` | increase | `READING` | take a counted handle to the texture's asset (`Zenith_AssetRegistry::AcquireAny(guid, ASSET_KIND_TEXTURE)`, the GUID recorded at load); for each mip in `[t, r − 1]` submit `Zenith_AssetReader::ReadRange` of that mip section's stored bytes at its recorded offset (priority from the class), destination staging from the streamer's staging arena |
| `ALLOCATED` | decrease | `COPYING` | record a texture-to-texture copy of mips `[t, mipCount − 1]` from the current texture on the copy queue |
| `READING` | every read completes | `DECODING` | drop the handle; a background job per mip checks the stored bytes against the recorded entry (`Zenith_VerifySection`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Parsing and validation functions"`), then decompresses the section with its descriptor's codec (`Zenith_DecompressSection`) into upload staging; a failed check is the "section is corrupt" row below |
| `DECODING` | every decode completes | `UPLOADING` | `RequestUpload` one splittable request per new mip; record the copy of the retained mips `[r, mipCount − 1]` from the current texture |
| `UPLOADING`, `COPYING` | every upload and copy reaches `COMPLETE` on the copy timeline | `SWAPPING` | — |
| `SWAPPING` | the first frame whose start finds the copy timeline past the value | `STABLE(t)` | publish the new SRV; rewrite the material blocks and slot records ("Reallocate-and-swap realisation"); release the old texture and descriptor through deferred deletion; release the reservation after the old texture's retirement |
| any state before `SWAPPING` | the requirement changes so that the pending change is no longer wanted, the asset is released, or shutdown begins | `CANCELLING` | `RequestCancel` every read; cancel every queued upload; discard staged uploads when they complete; the read handle is dropped when the last read reports complete |
| `CANCELLING` | every operation reports complete | `STABLE(r)` | release the new texture through deferred deletion and the reservation; staging returns to its arena |
| any | a read or decode fails, or a section is corrupt | `STABLE(r)` | as cancelling; the texture keeps its current mips; `Flux.Textures.StreamFailures`, log once |
| any | device loss | `STABLE(tail)` | the recovery path reloads the tail; streaming resumes from the tail |
| any state before `SWAPPING` | `ReplaceChainFromMemory` passes validation | `CANCELLING`, then pinned | the pending change is cancelled as above, and the texture leaves streaming while `m_bReplaced` is true ("Texture replace from memory") |

- At most one change per texture is in flight; a new requirement that arrives while one is in flight is acted on after it completes or is cancelled.
- A texture in use is never written in place ([Z2-N-REND-554]): every change creates a new texture.
- Cancellation at any state leaves no texture, descriptor, staging, reservation or operation behind: `Test_TextureStreaming_Cancel1000Cycles` ([Z2-N-CONTRACT-339]).

Range reads go through the asset system, because ZenithFlux declares no dependency on ZenithPlatform: `Zenith_AssetReader::ReadRange(const Zenith_AnyAssetHandle& xAsset, uint64 ulOffset, uint64 ulBytes, Zenith_MutableByteSpan xDestination, Zenith_AssetPriority ePriority) → Zenith_Result<Zenith_OperationHandle, Zenith_AssetError>` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetReader header"`), which returns the raw stored bytes and holds a reference to the asset until the read completes. The streamer holds its own counted handle only while reads are in flight, never in `STABLE`, so a texture's lifetime stays its users'. GEOM uses `ASSET_PRIORITY_HIGH` for mips of textures on screen within 20 m and `ASSET_PRIORITY_NORMAL` otherwise.

## Reallocate-and-swap realisation

The six steps of `Zenith2/Docs/Rendering/GpuResources.md → "Texture streaming swap"`, realised:

| Rendering step | GEOM realisation |
|---|---|
| 1 new resource with its own descriptor | `ALLOCATED`: `Flux_PersistentResources::CreateTexture` with `m_uFirstResidentMip` = 0 of the new chain, after the transition-peak reservation |
| 2 fill on the copy queue from staging | `UPLOADING`: new mips from upload-ring staging (`FLUX_UPLOAD_CLASS_TEXTURE_STREAM`); retained mips by a texture-to-texture copy from the current texture, which is persistent read-only and therefore readable by the copy |
| 3 the copy batch signals the copy timeline | the uploader's copy passes |
| 4 first frame past the value: `PERSISTENT_READ` | `SWAPPING`: the graph's one-time acquire list, as Rendering specifies |
| 5 a new material-constants block with the new descriptor; scatter rewrites `m_uConstantsOffset` | `Flux_MaterialManager` rewrites every material in the texture's reverse index (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`) |
| 6 old texture, descriptor and constants blocks through deferred deletion | the last-use values of the last frame that could reference them |

<!-- assumption: pending RPROD -->
The texture-to-texture copy of retained mips is an addition to the resource list of `Zenith2/Docs/Rendering/PerPass/Common.md → "StreamingUpload"` (which lists buffer-to-texture and buffer-to-buffer copies); a persistent read-only texture as a copy source is a read, consistent with [Z2-N-REND-547]. RPROD adds the copy kind to that table in the diff that implements streaming.
<!-- /assumption -->

The required test "two tables referencing the same texture while a streaming swap is in flight" is `Test_GPUScene_SharedTextureDuringStreamingSwap` (`Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tests"`): two materials in two material-slot records and a decal record reference one texture, a swap is forced mid-frame, and both render identically to a no-swap reference before, during and after the swap with validation clean.

## Texture replace from memory

`Flux_TextureManager::ReplaceChainFromMemory` replaces a raw texture's whole mip chain from CPU bytes by reallocate-and-swap. Its user is ENV's terrain live preview (`Zenith2/Docs/Design/ENV/Terrain.md → "Live preview"`, step 4), which replaces a tile's height, hole, splat and density textures each frame a dab changes them. It never replaces one mip: the crack-free argument of `Zenith2/Docs/Design/GEOM/Meshes.md → "Morphing and displacement"` rests on every mip of a tile being the point decimation of the same mip 0, so the call takes a whole chain and checks it.

Validation, in this order; a failure returns its error, allocates nothing and leaves the texture unchanged:

| # | Rule | Error (`m_uDetail`) |
|---|---|---|
| 1 | shutdown has not begun, and the device-loss state is normal | `SHUTTING_DOWN`; `DEVICE_LOST` |
| 2 | `xTexture` resolves | `STALE_ID` |
| 3 | the texture is tail-resident and not FAILED (`IsFallbackBound()` false) | `MISSING_DEPENDENCY` |
| 4 | `m_eUsage` equals the texture's usage and is `HEIGHT`, `HOLE_MASK`, `SPLAT_INDEX` or `SPLAT_WEIGHT`: a block-compressed class would need the cook's encoder, which the runtime never links | `INVALID_DESC` (0) |
| 5 | `m_uWidth` and `m_uHeight` equal the texture's, which has one layer | `INVALID_DESC` (1) |
| 6 | `m_axMips` holds exactly the texture's mip count, and mip l exactly `w_l × h_l × bytes per texel` bytes, with the extents of "The cooked texture format": a 257 × 257 `HEIGHT` (2 bytes) or `HOLE_MASK` (1 byte) tile has the 9 mips 257, 129, 65, 33, 17, 9, 5, 3, 2; a 256 × 256 splat texture (4 bytes) the 9 mips 256 … 1; ENV's 128 × 128 density the 8 mips 128 … 1 | `INVALID_DESC` (2) |
| 7 | the class's chain rule of "Texture cook pipeline" step 3, texel by texel: for `HEIGHT`, `HOLE_MASK` and `SPLAT_INDEX`, mip l texel (i, j) is bit-identical to mip 0 texel (i·2^l, j·2^l); for `SPLAT_WEIGHT`, every channel of every texel of mip l + 1 is `(s + c / 2) / c` over its footprint in mip l; for `HOLE_MASK`, every mip-0 byte is 0 or 255 | `INVALID_DESC` (3); the log names the mip and the texel |

The check is linear in the chain's texels (88,412 for a 257 × 257 tile, 87,381 for a 256 × 256 splat texture); a chain is at most 1,398,100 bytes, the 512-texel splat limit.

| Step | Action | Failure |
|---|---|---|
| 1 | reserve the transition peak, `Zenith_MemoryGovernor::ReserveTransitionPeak(TRANSITION_PEAK_TEXTURE_SWAP, chain bytes)` | refused: `PEAK_REFUSED` (the bytes), nothing created; the caller retries on a following frame |
| 2 | copy the chain into a staging allocation of the manager's heap, so the caller's spans are borrowed only for the call | `OUT_OF_MEMORY` (the bytes); the reservation is released |
| 3 | create a new `Flux_PersistentReadOnlyTexture` with the full chain `[0, mipCount − 1]` and its own descriptor (Rendering's swap step 1) | `OUT_OF_MEMORY`; staging and reservation released |
| 4 | `RequestUpload` one splittable request per mip from the staging, class `FLUX_UPLOAD_CLASS_BLOCKING`, for the preview's one-frame latency; the caller bounds its calls per frame (ENV: `environment.terrain.previewTilesPerFrame`); `m_bSwapInFlight` is true | — |
| 5 | step (2) of `Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"`, in the first frame whose start finds every upload complete: publish the new SRV (`GetSRV()` changes), rewrite the material blocks of the texture's reverse index and `Flux_TerrainRenderer`'s tile-table entries, retire the old texture and descriptor through deferred deletion, release the staging, and release the reservation after the old texture retires (Rendering's steps 4–6, as "Reallocate-and-swap realisation") | an upload reports `FAILED`: the new texture is released through deferred deletion, the texture keeps its current chain, `Flux.Textures.ReplaceFailures` increments, logged once per texture |

- At most one replacement per texture is in flight. A second call before the first publishes supersedes it: the first's queued uploads are cancelled, a staged one is discarded when it completes, and its texture, staging and reservation are released (`Flux.Textures.ReplacesSuperseded`), so the latest bytes are the ones that publish.
- The call cancels a streaming change in flight for the texture (the `CANCELLING` row of "Streaming state machine").
- Replaced content is pinned: from the call on, `GetResidency().m_bReplaced` is true, `IsStreamable()` returns false, and the streamer never changes the texture (the required-mip walk and `SetExternalRequirement` skip it), because its cooked mips on disk no longer describe it; the full chain stays resident and counts in `Flux.Textures.ResidentBytes`. In Stage 3 every terrain tile is resident with its full chain and nothing streams, so the pin first acts in Stage 4.
- The pin ends only when the texture's GPU side is rebuilt from the cooked file, by a reload of the asset (the next cook's hot reload) or a device-recovery load; both reload the cooked chain and clear `m_bReplaced`. A caller that needs its bytes back after a device recovery re-applies them; `m_bReplaced` false on a texture it replaced tells it.
- Thread: `RENDER_SUBMIT_THREAD`, on the `Flux.BuildGraph` node, as every texture-manager mutation; the call never blocks and allocates only in steps 2 and 3. `Flux.Textures.Replaces` counts the published replacements.

`Test_Texture_ReplaceChainFromMemory` asserts the validation table, the steps and the rules.

## Streaming upload arbitration

Every copy-queue upload of GEOM data goes through `Flux_StreamingUploader`, whose passes are budgeted by `Z2-B-GEOM-003` Streaming upload bytes per frame.

| Order within a frame | Class | Share |
|---|---|---|
| 1 | `BLOCKING` (a scene or cell activation waits on it) | unlimited within the upload-ring hard cap `Z2-B-RHI-006` |
| 2 | `VT_TILE`, the floor | the first `ceil(T / 4)` queued tiles, T the class limit of `SetVirtualTileBudget` (`Z2-B-ADVGFX-002`; 16 desktop, 6 S24 at ADVGFX's proposed values), served even when `BLOCKING` has used the frame's bytes, within the ring's hard cap |
| 3 | `MATERIAL` | unlimited (constants blocks are small) |
| 4 | `MESH` | up to 40% of the per-frame bytes |
| 5 | `TEXTURE_STREAM` | the remainder, at least 40% |
| 6 | `VT_TILE`, the rest | further tiles in the bytes left, up to T tiles in the frame including the floor |

Within a class, FIFO; a class that uses less than its share leaves the remainder to the next class. The floor answers ADVGFX's row AV19: a teleport that fills the ring with mesh and texture streaming still uploads `ceil(T / 4)` tiles each frame, and the class limit is the row's value, so the two cannot disagree. A VT tile is one request (18,496 bytes, not splittable), so the class counts tiles as requests. With T = 0 the class receives nothing. Bytes over the per-frame row are deferred to the next frame (`BR-DEFER`), never dropped, and `Flux.Streaming.UploadDeferredBytes` counts them.

## Virtual texturing boundary

| Concern | Owner |
|---|---|
| a texture is either streamed by GEOM or virtual (VT); the import preset's `virtual` flag decides; a material texture field may bind either kind | CONTENT (flag), GEOM (streamed), ADVGFX (virtual) |
| VT page tables, physical cache, feedback, tile cook layout | ADVGFX (`Z2-B-ADVGFX-001`, `-002`, `-003`) |
| VT tile uploads | ADVGFX produces them and calls `Flux_StreamingUploader::Enqueue` in class `VT_TILE` (activation uploads in `BLOCKING`); the uploader schedules them with the floor and limit of "Streaming upload arbitration" (A3 rows AV3 (c), AV19) |
| residency budgets | streamed textures count in `Z2-B-GEOM-002`, VT in `Z2-B-ADVGFX-001`; neither evicts the other |
| mip generation and block encoding of VT source data | GEOM's `Zenith_ReadTextureSource`, `Zenith_ReadTextureCookSettings`, `Zenith_BuildTextureMipChain` and `Zenith_EncodeTextureImage` ("Texture cook functions"), called by ADVGFX's tile cook (A3 row AV3 (d)) |
| a material field bound to a VT | GEOM's material cook and material manager (`Zenith2/Docs/Design/GEOM/Materials.md → "Virtual texture bindings"`; A3 row AV3 (a), (b)) |

## Render-target textures

<!-- elaboration: answers cross-area rows RP28 and RPB18 (RPROD's Stage 5 gameplay texture cameras, `Zenith2/Docs/Design/RPROD/Extractors.md`) -->
A render-target texture is an ordinary `Flux.Texture` asset (no new asset kind) that the GPU writes and materials sample, for `FLUX_CAMERA_KIND_TEXTURE` cameras whose `m_xOutputTexture` names it.

| Element | Rule |
|---|---|
| source | a `geom-constant-texture` document with `"renderTarget": true` (Z2T-GEOM-127's importer at version 2): `channels` 4, `bits` 8 (`srgb` allowed) or 16, `width` and `height` 1 … 4,096, a `fill` that is the target's initial contents; the importer writes the `geom.textureSource` and a `geom.renderTarget` intermediate (empty bytes); `renderTarget` with any other shape is `IMPORT_ERROR_SOURCE_INVALID` |
| cook | the texture step, seeing `geom.renderTarget`, writes one mip, never compressed, in `R8G8B8A8_UNORM` (`_SRGB` when `srgb`) or `R16G16B16A16_SFLOAT`, tail first mip 0, flag bit 3 set and bit 2 clear; usage `COLOUR` (8-bit) or `LINEAR` (16-bit); the reader rejects bit 3 with any other format, usage, mip count or tail (`FLUX_GEOMETRY_ERROR_CORRUPT_SECTION`) |
| runtime | the loader creates it at the drain through `Flux_PersistentResources` with sampled and colour-target usage (not a `Flux_PersistentReadOnly` texture: the GPU writes it in place), uploads the tail as the initial contents and reports `ReportGpuResident`; `GetSRV()` samples it and `GetRenderTargetHandle()` returns the writable `Flux_TextureHandle` the view writing it imports into its frame's graph (RPROD's texture views); the streamer never touches it, it counts toward `Flux.Textures.ResidentBytes`, and a device recovery recreates it with its initial contents |
| fallback | until RESIDENT, or FAILED, it binds its usage's fallback like any texture, and `GetRenderTargetHandle()` is null, so RPROD's extractor skips the camera that frame |

`Test_Texture_RenderTargetLoadsWritable` and `Test_CookTexture_RenderTargetSingleMip` assert it ("Texture tests").

## Fallback textures

| Usage | Shipping fallback (1 × 1 unless stated) | Development fallback |
|---|---|---|
| `COLOUR` | (1, 1, 1, 1) white | an 8 × 8 magenta and black checker, 4 mips |
| `LINEAR` | (0.5, 0.5, 0.5, 1) | the checker |
| `NORMAL` | (0.5, 0.5, 1, 1): a flat normal | flat normal |
| `MASK` | (1, 0.5, 0, 1): occlusion 1, roughness 0.5, metallic 0 | the same |
| `HEIGHT` | 0 (the terrain's minimum height) | 0 |
| `HOLE_MASK` | 1 (no hole) | 1 |
| `SPLAT_INDEX` | (0, 0, 0, 0): palette index 0 in every slot | the same |
| `SPLAT_WEIGHT` | (1, 0, 0, 0): full weight on slot 0; a density consumer binds none ("Texture usage classes") | the same |
| `COLOUR_EXACT` | (1, 1, 1, 1) white | the checker |

- Fallbacks are engine content cooked from `Zenith2/Engine/ZenithFlux/Content/Textures/` (the engine asset rule of [Z2-N-ARCH-028]) and are always resident.
- A texture that fails to load, is missing from the package, or is not yet tail-resident binds the fallback of its usage; each case counts `Flux.Textures.FallbackBindings` and logs once per asset per run.

## Texture tests

<!-- elaboration -->
| Test | File | Asserts |
|---|---|---|
| `Test_CookTexture_MipChainLinearLight` | `Zenith2/Tools/ZenithCook/Tests/Textures/Zenith_CookTexture_Tests.cpp` | an sRGB checker's mip 1 equals the linear-light average, not the sRGB-space average |
| `Test_CookTexture_OddExtentFold` | same | a 5 × 3 texture's mips fold the edge texels |
| `Test_CookTexture_NormalRenormalised` | same | every mip texel of a normal map fixture decodes to unit length within one quantum |
| `Test_CookTexture_AlphaCoveragePreserved` | same | the fraction above the cutoff at mips 1–6 is within 1% of mip 0's |
| `Test_CookTexture_FormatsPerPlatform` | same | Windows cooks give BC7 (sRGB for colour), Android cooks ASTC 4×4 |
| `Test_CookTexture_Deterministic` | same | two cooks give byte-identical `.ztex` |
| `Test_CookTexture_TailRule` | same | the tail first mip follows the 64 KiB rule for 16 fixture extents; a 257 × 257 `HEIGHT` tile and a 257 × 257 `HOLE_MASK` tile have tail first mip 1; a 256 × 256 `SPLAT_INDEX` and `SPLAT_WEIGHT` texture have tail first mip 0 and flag bit 2 clear |
| `Test_CookTexture_HeightDecimation` | same | a 257 × 257 `HEIGHT` fixture has 9 mips of extents 257 … 2, each texel equal to mip 0 at (i·2^l, j·2^l) bit for bit; a 256 × 256 `HEIGHT` source fails with `GEOM_COOK_ERROR_TEXTURE_LIMIT`; the reader rejects a `HEIGHT` file whose mip 1 extent is 128 |
| `Test_CookTexture_SplatMips` | `Zenith2/Tools/ZenithCook/Tests/Textures/Zenith_CookTexture_Tests.cpp` | a 256 × 256 `SPLAT_INDEX` fixture has 9 mips of extents 256 … 1, each texel equal to mip 0 at (i·2^l, j·2^l) bit for bit; a `SPLAT_WEIGHT` fixture's mip 1 texel of footprint channel values (255, 0, 0, 1) is 64 = (256 + 2) / 4 and every mip follows the integer mean; both are `R8G8B8A8_UNORM` on both platforms; a 513-texel extent fails with `GEOM_COOK_ERROR_TEXTURE_LIMIT`; the reader rejects a splat file with t ≠ 0 |
| `Test_TextureStreaming_ExternalRequirement` | `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_TextureStreaming_Tests.cpp` | a height tile required at mip 0 by `SetExternalRequirement` streams mip 0 in; stopping the call lets it drop to its tail after 120 frames |
| `Test_Texture_ReaderRejectsCorruption` | `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_TextureReader_Tests.cpp` | one mutated fixture per validation rule fails with its error |
| `Test_Texture_LimitsAtAndPastLimit` | same | 16,384 and 16,385, 15 and 16 mips, 2,048 and 2,049 layers ([Z2-N-ARCH-953]) |
| `Test_SamplerPolicy_DefaultsPerUsage` | `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_CookSampler_Tests.cpp` | every row of the policy table resolves to its index |
| `Test_SamplerPolicy_OverrideResolved` | same | every override name resolves to its index |
| `Test_SamplerPolicy_ForbiddenEntriesRejected` | same | the comparison and reserved entries are cook errors |
| `Test_TextureStreaming_RequiredMipAnalytic` | `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_TextureStreaming_Tests.cpp` | a plane at known distances requires the analytic mip for a 2,048 texture at 1 m UV density |
| `Test_TextureStreaming_WorkerIndependent` | same | per-texture requirements are identical with 1, 2 and 8 workers |
| `Test_TextureStreaming_NeverWritesInPlace` | same | every residency change creates a new texture; the old one's contents are bit-identical until retired |
| `Test_TextureStreaming_RetainedMipsCopied` | same | an increase reads only the new mips from disk; a decrease reads nothing |
| `Test_TextureStreaming_BudgetRespected` | same | with a pool target below demand, resident bytes never exceed the target and the lowest-priority textures shrink first |
| `Test_TextureStreaming_PeakRefusedDefers` | same | a refused texture-swap reservation defers the change and allocates nothing |
| `Test_TextureStreaming_Cancel1000Cycles` | same | 1,000 changes cancelled at randomised states leave no texture, descriptor, staging, reservation or allocation behind |
| `Test_TextureStreaming_PressureShedding` | same | forced warning scales the target by 0.75 and biases by +1; forced critical biases by +2 and drops unseen textures to their tails; the counters prove each |
| `Test_TextureStreaming_UploadArbitration` | same | with every class saturated, per-frame bytes follow the class order and shares, and none exceed the row |
| `Test_GPUScene_SharedTextureDuringStreamingSwap` | `Zenith2/Engine/ZenithFlux/Tests/GPUScene/` | as described in "Reallocate-and-swap realisation" |
| `Test_Texture_FallbackPerUsage` | `Flux_TextureManager_Tests.cpp` | a failed texture of each usage binds its fallback, shipping and development forms; `IsFallbackBound()` is true while a texture is loading and when it is FAILED, and false from the frame its tail is resident |
| `Test_Texture_ReplaceChainFromMemory` | `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_TextureManager_Tests.cpp` | a 257 × 257 `HEIGHT` tile replaced with a point-decimated chain reads back all 9 mips byte for byte, `GetSRV()` changes on the publishing frame and not before, and the old texture's contents stay bit-identical until it retires; `HOLE_MASK`, `SPLAT_INDEX`, `SPLAT_WEIGHT` 256 × 256 and a 128 × 128 density texture round-trip likewise; one failing call per validation row (a shutdown and a device-lost state, a stale ID, a loading texture, a `COLOUR` texture, a 256-extent `HEIGHT` chain, 8 mips for a 257 tile, a mip one byte short, mip 3 texel (1, 1) off by one, a `SPLAT_WEIGHT` mip 1 texel one above the integer mean, a hole byte of 128) and a refused peak return their errors and create nothing; a second call before publication supersedes the first, only its bytes publish and the first's texture, staging and reservation are released; a terrain's tile-table entry names the new texture on the publishing frame; a replaced texture reports `m_bReplaced` and `IsStreamable()` false and is never changed by the streamer with `SetExternalRequirement` calls; a simulated device loss restores the cooked chain and clears `m_bReplaced`; 1,000 replacements cancelled by supersession at randomised states leave no texture, descriptor, staging or reservation behind |
| `Test_CookTexture_SourceIntermediate` | `Zenith2/Tools/ZenithCook/Tests/Textures/Zenith_CookTexture_Tests.cpp` | blobs written by `Zenith_WriteTextureSource` for ENV's five terrain products (257² `R16` heights, 257² `R8` holes, 256² `R8G8B8A8` splat index and weight, 128² `R8G8B8A8` density) cook to `.ztex` files of the declared usages under the declared products' GUIDs, and a CONTENT-style blob with usage 0 takes the preset's usage; one mutated blob per rule (magic, version, reserved byte, a `R8` `HEIGHT`, an sRGB `MASK`, a byte count one short, a hole byte of 7, usage 0 with no preset usage, a declared usage contradicting an explicit preset value) fails with `GEOM_COOK_ERROR_TEXTURE_SOURCE` naming the product and the field; a product with no `geom.textureSource` blob fails likewise; an unchanged blob re-cooks from the DDC |
| `Test_Texture_RenderTargetLoadsWritable` | `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_TextureManager_Tests.cpp` | a render-target seed loads RESIDENT with `IsRenderTarget()` true and a non-null `GetRenderTargetHandle()`; a compute pass writing (1, 0, 0, 1) through the handle in frame N is sampled as that colour through `GetSRV()` in frame N + 1 on WARP and lavapipe; an ordinary texture returns a null handle; a seed with flag bit 3 and a BC7 format is refused with `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| `Test_CookTexture_RenderTargetSingleMip` | `Zenith2/Tools/ZenithCook/Tests/Textures/Zenith_CookTexture_Tests.cpp` | a 256 × 128 `renderTarget` document cooks to one uncompressed mip, tail first mip 0, flags bit 3 set and bit 2 clear; a 16-bit one cooks `R16G16B16A16_SFLOAT` usage `LINEAR`; `renderTarget` with 3 channels fails the import with `IMPORT_ERROR_SOURCE_INVALID` |
| `Test_CookTexture_FunctionsMatchStep`, `Test_CookTexture_EncodeRejectsRawUsage` | `Zenith2/Tools/ZenithCook/Tests/Textures/Zenith_CookTexture_Tests.cpp` | "Texture cook functions" |
| `Test_CookTexture_ColourExactStoredRaw` | same | a 64 × 32 `COLOUR_EXACT` source with one texel of every byte value cooks to one `R8G8B8A8_UNORM_SRGB` mip on both platforms whose bytes equal the source texels, with t = 0 and flag bit 2 clear; a 4,097-texel extent fails with `GEOM_COOK_ERROR_TEXTURE_LIMIT`; a linear-space or two-layer `COLOUR_EXACT` blob fails with `GEOM_COOK_ERROR_TEXTURE_SOURCE`; the reader rejects a `COLOUR_EXACT` file with two mips or a BC7 format |
| `Test_CookTexture_PublishesAverageColour` | same | the texture step publishes `geom.textureAverageColour` equal to the info record's average colour bit for bit |
| `Test_StreamingUploader_EnqueueVirtualTiles`, `Test_StreamingUploader_VirtualTileFloor` | `Zenith2/Engine/ZenithFlux/Tests/Textures/Flux_StreamingUploader_Tests.cpp` | `Enqueue` and the `VT_TILE` floor and limit of "Streaming upload arbitration" |
| fuzz `Zenith_Fuzz_TextureFile` | `ZenithFlux/Tests/Fuzz/TextureFile.cpp` | arbitrary bytes never crash the reader |
