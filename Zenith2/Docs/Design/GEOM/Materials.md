# GEOM design: materials, permutations, transparency, decals and surface models

Section file of `Zenith2/Docs/Design/GEOM.md`. It specifies the material document in full, the conventions a material module follows, the material cook and the cooked `.zmat` format, the runtime material resource, material-slot records, permutation enumeration and the per-game permutation limit, transparency, decals, the advanced surface models and the material fallbacks. The normative source is `Zenith2/Docs/Rendering.md → "Materials"` (the material model, the surface function contract, explicit gradients, the material document, classes and buckets, constants and the material-slot table, permutations and limits, previews and tests), `Zenith2/Docs/Rendering/ShaderSystem.md → "Permutations"` and Z2-D-079: materials are Slang surface functions plus a `.json` of parameters, with no node graph.

## Scope of this file

| Topic | Heading |
|---|---|
| the authoring document | "Material document schema" |
| what a material module declares | "Material module conventions" |
| the reflection the cook validates against | "Material reflection input" |
| the cook step | "Material cook" |
| the cook's input blob, authored or generated | "Material document intermediate" |
| the cooked file and its reader | "The cooked material format", "Material reader validation" |
| runtime types | "Flux_MaterialResource header" |
| slot records, constants blocks and buckets | "Material slots and constants" |
| a texture field bound to a virtual texture | "Virtual texture bindings" |
| which permutations exist | "Material permutation enumeration" |
| the limit and the report | "Permutation limit check and report" |
| transparency | "Transparency" |
| decals | "Decals" |
| clear coat, anisotropy, sheen, subsurface | "Advanced surface models" |
| what renders when something is missing | "Material fallbacks" |
| tests | "Material tests" |

## Material document schema

A material document (`<Name>.material.json`) is an authoring document ([Z2-N-REND-247]): it has a schema version and a migration chain, preserves unknown fields with a warning, and is written through the journaled atomic write (`Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`). Rendering gives the keys; the schema below is complete for schema version 1.

<!-- elaboration -->
| Key | JSON type | Required | Rule |
|---|---|---|---|
| `schemaVersion` | integer | yes | 1 |
| `id` | string, the persistent-ID form of [Z2-N-CONTRACT-067] | yes | the material's GUID |
| `module` | string, repo-relative path | yes | a `.slang` file under `Zenith2/Shaders/Materials/` or `Zenith2/Games/<Name>/Assets/Materials/`; a game material names a module of its own game or of the engine |
| `type` | string, identifier | yes | a type in `module` conforming to `Flux_MaterialType` (or `Flux_DecalMaterialType` when `domain` is `decal`) |
| `domain` | string | yes | `opaque`, `alphaTest`, `transparent`, `decal` |
| `twoSided` | boolean | no, default false | disables back-face culling for the material's PSOs ([Z2-N-REND-923]) |
| `alphaCutoff` | number in [0, 1] | `alphaTest` only; default 0.5 | opacity below it discards in depth-writing passes |
| `blend` | string | optional for every domain; default `alpha` for `transparent`, `none` for the others | `none`, `alpha`, `premultiplied`, `additive`; for `transparent`, `none` is `GEOM_COOK_ERROR_MATERIAL_DOMAIN_KEY`; for the other domains, any value other than `none` is `GEOM_COOK_ERROR_MATERIAL_DOMAIN_KEY` |
| `transparency` | object | `transparent` only | `{ "sortBias": number (metres added to the sort depth, default 0), "refraction": boolean (default false), "castShadows": "none" or "alphaTest" (default "none"), "shadowAlphaCutoff": number (default 0.5), "twoSidedOrder": "backThenFront" or "single" (default "backThenFront" when twoSided) }` |
| `decal` | object | `decal` only | `{ "channels": array of "albedo", "normal", "roughness", "metallic", "ao", "emissive" (non-empty), "angleFadeStartDegrees": number (default 60), "angleFadeEndDegrees": number (default 80), "sortPriority": integer in [−128, 127] (default 0) }` |
| `permutations` | object | no | values of the module's material-level `bPERM_*`/`uPERM_*` constants by name; an omitted constant takes its default (first) value |
| `parameters` | object | no | values of plain `Params` fields by name: a number for `float`, `int`, `uint` (integers must be exact), an array of 2–4 numbers for vectors, a 12- or 16-element array for matrices; colours are linear |
| `textures` | object | no | texture asset GUID strings by `DescriptorHandle` field name; a field absent here uses its `[Optional]` null substitute or, if not optional, is a cook error |
| `samplers` | object | no | sampler policy overrides by texture field name ("Material module conventions", row "sampler companion fields"); values from `Zenith2/Docs/Design/GEOM/Textures.md → "Sampler policy"` |
| `shadingModel` | string | no, default from the module | `defaultLit`, `unlit`, and from Stage 5 `subsurface`, `cloth`; plus the flags `clearCoat`, `anisotropy` inside `surfaceModelFlags` |
| `surfaceModelFlags` | array of strings | no | `clearCoat`, `anisotropy` (Stage 5); each must be supported by the module's type (reflection) |
| `preview` | object | no | `{ "mesh": "sphere" or "cube" or "plane" or a mesh GUID, "environment": an asset GUID }`, read by the editor's material preview alone (EDIT) |

- Unknown top-level keys, unknown keys inside `transparency`, `decal` and `preview`, and unknown names inside `parameters`, `textures`, `samplers` and `permutations` are different cases: unknown **top-level or object** keys are preserved with `MIGRATION_WARNING_UNKNOWN_FIELD` ([Z2-N-CONTRACT-543]); unknown **names** that should match the module's reflection are cook errors ([Z2-N-REND-248]), because they mean the document and its module disagree.
- A document is migrated by the material migration chain registered through `Zenith_RegisterMigrationStep` for the document type `Flux.Material` (`Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`); the registration function `Zenith_RegisterMaterialMigrations(Zenith_MigrationRegistry&)` lives in the tool library ZenithAuthoring (`Zenith2/Tools/Authoring/Internal/Geom/Zenith_MaterialDocumentTypes.cpp`, root header `Zenith2/Tools/Authoring/Zenith_AuthoringGeom.h`; ruling Q84 (a)), because both ZenithCook and the editor's `Zenith_RegisterEditorDocumentTypes` call it; version 1 has no step; its fixtures are `Zenith2/Engine/ZenithFlux/Tests/Fixtures/Migration/Material/v1.json` and `v1.expected.json` ([Z2-N-CONTRACT-548]).

## Material module conventions

A material module is Slang; its type implements `Flux_MaterialType` (`Zenith2/Docs/Rendering.md → "Surface function contract"`). GEOM adds these conventions, which the reflection exposes and the cook enforces.

<!-- elaboration -->
| Convention | Rule |
|---|---|
| `Params` | a plain struct under the scalar layout of [Z2-N-REND-064]; at most `uGEOM_MAX_MATERIAL_CONSTANTS_BYTES` = 1,024 bytes; resource fields are `DescriptorHandle<Texture2D<…>>` (persistent read-only textures only) and each may carry `[Optional]`; every other field is plain data |
| texture usage | each texture field carries `[TextureUsage("colour" | "linear" | "normal" | "mask")]` so the cook can check the bound texture's usage class and the mesh cook can see which meshes need tangents |
| UV channel | each texture field may carry `[UVChannel(1)]`; the default is 0; the reflection records it for the mesh cook and the texture streamer |
| sampler companion fields | for a texture field `m_x<Stem>`, a `uint m_u<Stem>Sampler` field, when the type declares one, is filled by the cook with the resolved static-sampler index, and the module passes it to `Flux_SampleMaterial`; a type without the companion uses its own constant, and a `samplers` override for such a field is a cook error |
| permutation constants | material-level `[Permutation(…)] extern static const` constants named `bPERM_*`/`uPERM_*`, as for programs; they are part of the material variant, never of the program key |
| vertex colour | reading `Flux_MaterialInput::m_xVertexColour` sets the reflection flag `readsVertexColour` |
| shading model | the type declares `static const uint uSHADING_MODEL` (one of `FLUX_SHADING_MODEL_*`) and `static const uint uSURFACE_MODEL_FLAGS`; the document's `shadingModel`/`surfaceModelFlags` must equal or be a supported subset |
| purity | `Evaluate` and `EvaluateOpacity` are pure and sample with explicit gradients ([Z2-N-REND-242], [Z2-N-REND-245]) |
| engine materials | `Zenith2/Shaders/Materials/Standard.slang` (`Material_Standard`: base colour, normal, ORM, emissive, factors; from Stage 4 its `Evaluate` ends with ENV's `Env_ApplySurfaceWeather(xSurface, <the frame's weather block>, xParams.m_fPorosity, 1.0h)` of `Zenith2/Shaders/Environment/Weather.slang`, with the plain parameter `half m_fPorosity` defaulting to 0.5, A3 row EN8, `Zenith2/Docs/Design/ENV/WaterWeather.md → "Shared wetness function"`; `Test_Material_StandardWeatherApplied`), `Unlit.slang` (`Material_Unlit`), `Fallback.slang` (the magenta PSO fallback, [Z2-N-REND-209]), `DecalStandard.slang` (`Decal_Standard`), `DevMissing.slang` with `DevMissing.material.json` (the development missing-material checker), `TerrainGrid.slang` (the terrain geometry adapter, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry"`), from Stage 4 `HLODProxy.slang` (`Material_HLODProxy`, the flatten-mode HLOD material, `Zenith2/Docs/Design/GEOM/Meshes.md → "Merge recipes"`; its document is `content:/Engine/Materials/HLODProxy.material.json`), and from Stage 5 `ClearCoat.slang`, `Cloth.slang`, `Skin.slang` |

<!-- assumption: pending RPROD -->
`[TextureUsage(…)]` and `[UVChannel(n)]` are two rows added to the closed attribute vocabulary of `Zenith2/Docs/Rendering/ShaderSystem.md → "Slang authoring vocabulary"` ([Z2-N-REND-115]), applying to resource fields of a material type's `Params`; the ticket that implements the material cook adds them there.
<!-- /assumption -->

## Material reflection input

<!-- assumption: pending RPROD -->
`ZenithShaderCompiler`, in cook mode, writes one `Reflection/Materials/<ModulePath>/<Type>.material.json` per material type found in the shader roots, schema version 1, and the cook validates documents against it. RPROD's `Zenith2/Docs/Rendering/ShaderSystem.md → "Reflection JSON"` gains this file in the diff that implements the material cook.
<!-- /assumption -->

```json
{
	"schemaVersion": 1,
	"module": "Zenith2/Shaders/Materials/Standard.slang",
	"type": "Material_Standard",
	"typeId": "0x9b1f40c2e7a55d03",
	"interface": "Flux_MaterialType",
	"shadingModel": "defaultLit",
	"surfaceModelFlags": [],
	"readsVertexColour": false,
	"params": {
		"size": 96,
		"fields": [
			{ "name": "m_xBaseColour", "offset": 0, "size": 8, "kind": "texture", "usage": "colour", "uvChannel": 0, "optional": true },
			{ "name": "m_uBaseColourSampler", "offset": 8, "size": 4, "kind": "data", "type": "uint", "samplerFor": "m_xBaseColour" },
			{ "name": "m_xBaseColourFactor", "offset": 16, "size": 16, "kind": "data", "type": "float4", "default": [1, 1, 1, 1] }
		]
	},
	"permutations": [ { "name": "bPERM_DETAIL_NORMAL", "type": "bool", "values": [false, true] } ]
}
```

`typeId` is `Zenith_Hash64` of `"<module path without extension>/<Type>"`; it keys `Zenith_RHIPermutationKey::m_ulMaterialVariantID` together with the permutation values ("Material cook").

## Material cook

The material cook step runs per material document per platform, in a ZenithCook worker process.

| Step | Action | Failure |
|---|---|---|
| 1 | read the document from the item's `geom.materialDocument` intermediate ("Material document intermediate") with `Zenith_JsonDocument` (CORE), migrate it in memory to schema version 1 if older (never writing the source) | `GEOM_COOK_ERROR_MATERIAL_SCHEMA` with line and column; a missing blob is `GEOM_COOK_ERROR_MATERIAL_SCHEMA` naming the product |
| 2 | load the type's reflection | `GEOM_COOK_ERROR_MATERIAL_UNKNOWN_TYPE` |
| 3 | validate every key of "Material document schema" and every name against the reflection: unknown parameter, texture, sampler or permutation name; type or arity mismatch; a permutation value outside the declared set; a missing non-optional texture; a domain key present for another domain | `GEOM_COOK_ERROR_MATERIAL_UNKNOWN_NAME`, `GEOM_COOK_ERROR_MATERIAL_TYPE_MISMATCH`, `GEOM_COOK_ERROR_PERMUTATION_VALUE`, `GEOM_COOK_ERROR_MATERIAL_MISSING_TEXTURE`, `GEOM_COOK_ERROR_MATERIAL_DOMAIN_KEY`, each naming the document, the key and the expected type ([Z2-N-REND-248]) |
| 4 | resolve each texture GUID through the asset database (ASSET) and check its usage class against the field's `[TextureUsage]`; a GUID of kind `ASSET_KIND_VIRTUAL_TEXTURE` follows "Virtual texture bindings" | `GEOM_COOK_ERROR_MATERIAL_TEXTURE_UNRESOLVED`, `GEOM_COOK_ERROR_MATERIAL_TEXTURE_USAGE` |
| 5 | resolve sampler indices ("Sampler policy" of `Zenith2/Docs/Design/GEOM/Textures.md`) into the companion fields | `GEOM_COOK_ERROR_MATERIAL_SAMPLER` |
| 6 | pack the constants block: the reflection's defaults, overwritten by `parameters`, in the reflected offsets, little-endian; texture handle fields left zero (patched at load); record the terrain record offset: the reflected offset of a `Params` field named `m_uTerrainRecord` of kind data and type `uint`, else 0xFFFFFFFF (a field of that name with another type is `GEOM_COOK_ERROR_MATERIAL_TYPE_MISMATCH`) | — |
| 7 | compute the variant ID: `Zenith_Hash64Stream` seed 0 over `typeId` (u64 LE), then each material-level permutation constant in declaration order as its value index (u32 LE) | — |
| 8 | write the `.zmat` and the material's cook-report entry | — |
| 9 | publish `geom.proxyColour`: four little-endian linear `f32`, the `float4` parameter `m_xBaseColourFactor` (1, 1, 1, 1 for a type without one) times, per channel, the `geom.textureAverageColour` of the first `COLOUR` texture binding (1, 1, 1, 1 when there is none or it is virtual), read with `ReadDependencyOutput`; the HLOD flatten mode reads it (`Zenith2/Docs/Design/GEOM/Meshes.md → "Merge recipes"`) | — |

## Material document intermediate

The material step reads its document from one intermediate blob per material product, `geom.materialDocument`: the UTF-8 bytes of a material document of "Material document schema", read through `Zenith_CookContext::ReadIntermediate` (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"`). Every importer that declares an `ASSET_KIND_MATERIAL` product writes it with `Zenith_ImportContext::WriteIntermediate` (`Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`):

| Writer | Document | Rules |
|---|---|---|
| ASSET's `zenith-doc` importer, for an authored `<Name>.material.json` | the source file's bytes, unchanged | the authoring document: migrated in memory by step 1, migrated on disk only by the document chain ("Material document schema") |
| an importer that generates a material, such as ENV's `env.terrain` (stage `Environment.TerrainMaterial`: `material:terrain` with `Material_TerrainLayered`, `material:detail` with `Material_DetailFoliage`) and its water documents' `material:water` (`Zenith2/Docs/Design/ENV/Bake.md → "Step graph"`) | a document the importer builds | `schemaVersion` the current version 1; `id` the GUID `DeclareProduct` returned for the product; `textures` by GUID string, each a product of the same import or another asset, resolved by step 4 like any texture reference; plain-data `parameters` by name, with `m_uTerrainRecord` 0 for a terrain material (the placeholder `AddTerrain` overwrites, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`); no unknown keys |

- A generated document is not an authoring document: it exists only as the intermediate, keyed by the importer's version, so it has no file, no journaled write and no migration of its own; an importer whose document format changes bumps its version and re-imports.
- GUID ownership is the rule of `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`: the importer owns the product key and the GUID `DeclareProduct` returned (ASSET's sub-asset identity), the document's `id` must equal the item's GUID (`GEOM_COOK_ERROR_MATERIAL_SCHEMA` otherwise), and GEOM's step writes the `.zmat` under it and mints none.
- The blob's bytes are the step's first derived-data key input (`Zenith2/Docs/Design/GEOM.md → "Cook steps and derived-data keys"`, row "material").

`Test_Material_CookFromGeneratedDocument` asserts these rules.

## The cooked material format

`.zmat` is ASSET's cooked container (`Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`) with magic `'ZMAT'` (0x54414D5A), kind `ASSET_KIND_MATERIAL` and format version 1.

<!-- elaboration -->
| Section kind | Value | Content |
|---|---|---|
| `GEOM_SECTION_MATERIAL_INFO` | 20 | the info record |
| `GEOM_SECTION_MATERIAL_CONSTANTS` | 21 | the packed constants block (size from the info record) |
| `GEOM_SECTION_MATERIAL_TEXTURES` | 22 | texture binding records |

Info record (64 bytes): type ID u64; variant ID u64; program ID of the module (u64, the `Zenith_RHIShaderProgramID` value); domain u8 (0 opaque, 1 alphaTest, 2 transparent, 3 decal); blend u8 (0 none, 1 alpha, 2 premultiplied, 3 additive); flags u16 (bit 0 two-sided, bit 1 refraction, bit 2 transparent casts alpha-tested shadows, bit 3 back-then-front order, bit 4 reads vertex colour); alpha cutoff f32; shadow alpha cutoff f32; sort bias f32; shading model u8; surface model flags u8; decal channel mask u8; decal sort priority i8; decal angle fade start cosine f32; decal angle fade end cosine f32; constants size u32; texture binding count u16; reserved u16; terrain record offset u32 (step 6 of "Material cook": the constants offset of `m_uTerrainRecord`, or 0xFFFFFFFF when the type declares none; `Flux_TerrainRenderer::AddTerrain` reads it, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`), to 64 bytes.

Texture binding record (24 bytes): constants offset of the handle field u32; texture GUID (16 bytes); usage class u8; UV channel u8; flags u8 (bit 0: the GUID is a virtual texture, "Virtual texture bindings"; bits 1–7 zero); reserved u8.

### Material reader validation

`Flux_ReadMaterial(const Zenith_CookedFileView&, Zenith_Span<const Zenith_AssetLoadedSection>)` (in `ZenithFlux/Internal/Materials/Flux_MaterialReader.cpp`) treats the file as untrusted ([Z2-N-ARCH-951]) and returns an error, never asserting on its bytes ([Z2-N-CONTRACT-472]). The header, the section table, the section checks and decoding are ASSET's (checks C1–C12 of `Zenith2/Docs/Design/ASSET/Formats.md → "Checks"`, applied before the material loader's `BeginLoad`); the reader runs the kind-specific rows:

<!-- elaboration -->
| Check | Error |
|---|---|
| the view's magic is `'ZMAT'` and its kind `ASSET_KIND_MATERIAL`: a defence for views not produced by the registry (the fuzz target, tests), which ASSET's C1 and C7 already refuse | `FLUX_GEOMETRY_ERROR_BAD_HEADER` |
| the view's format version is 1: the same defence, behind ASSET's C5 | `FLUX_GEOMETRY_ERROR_VERSION_MISMATCH` |
| the sections present are exactly one each of kinds 20, 21 and 22, each at index 0 (22 may be absent when the binding count is 0) | `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| the info section is 64 bytes; domain ≤ 3; blend ≤ 3; `transparent` has blend ≠ none and every other domain blend none; flags bits 5–15 zero; shading model ≤ 3; surface model flags hold only bits 2 and 3; alpha cutoffs in [0, 1]; decal fields zero unless the domain is `decal` | `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| the terrain record offset is 0xFFFFFFFF, or a multiple of 4 with offset + 4 ≤ the constants size that overlaps no texture binding's 8 bytes | `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| the constants size is a multiple of 4, at most `uGEOM_MAX_MATERIAL_CONSTANTS_BYTES`, and equal to the decoded constants section's size | `FLUX_GEOMETRY_ERROR_CONSTANTS_TOO_LARGE` above the limit, otherwise `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| the binding count × 24 equals the decoded texture section's size; each binding's constants offset is a multiple of 8 with offset + 8 ≤ constants size, offsets are distinct, usage class is 1 … 6 (`FLUX_TEXTURE_USAGE_*`), UV channel ≤ 1, flags bits 1–7 and the reserved byte zero, flags bit 0 only with domain 0 or 1 | `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| the type ID and variant ID name a variant of the loaded shader container | not a reader error: the missing-PSO fallback of "Material fallbacks" applies |

A failed read leaves the material `FAILED`, and its sections draw with the default material ("Material fallbacks"). The fuzz target `Zenith_Fuzz_MaterialFile` (`ZenithFlux/Tests/Fuzz/MaterialFile.cpp`) parses arbitrary bytes with `Zenith_ParseCookedHeader`, decodes the sections and drives the reader with a seed corpus of the material fixtures.

## Flux_MaterialResource header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_MaterialResource.h
#pragma once
#include "ZenithRHI/Zenith_RHIShaderInterface.h"
#include "ZenithFlux/Flux_GeometryErrors.h"

constexpr uint32 uGEOM_MAX_MATERIAL_CONSTANTS_BYTES = 1024;

/// @contract value Flux_MaterialID
/// Ownership: Plain value naming a loaded material in Flux_MaterialManager; the asset handle that loaded it keeps it alive.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: Stale after the material's retirement; resolution fails with FLUX_GEOMETRY_ERROR_STALE_ID.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Flux_MaterialID
{
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
	constexpr bool IsNull() const { return m_uGeneration == 0; }
	friend constexpr bool operator==(Flux_MaterialID, Flux_MaterialID) = default;
};

enum Flux_MaterialDomain : uint8 { FLUX_MATERIAL_DOMAIN_OPAQUE, FLUX_MATERIAL_DOMAIN_ALPHA_TEST, FLUX_MATERIAL_DOMAIN_TRANSPARENT, FLUX_MATERIAL_DOMAIN_DECAL };
enum Flux_MaterialBlend : uint8 { FLUX_MATERIAL_BLEND_NONE, FLUX_MATERIAL_BLEND_ALPHA, FLUX_MATERIAL_BLEND_PREMULTIPLIED, FLUX_MATERIAL_BLEND_ADDITIVE };

/// @contract type Flux_MaterialResource
/// Ownership: Created by Flux_MaterialManager when the asset system loads a .zmat; shared through
///   Zenith_AssetHandle<Flux_MaterialResource>; its constants block retires through deferred deletion.
/// Thread affinity: ANY_THREAD for the const members; ReplaceParameters is RENDER_SUBMIT_THREAD-deferred (it queues a
///   request applied on the Flux.BuildGraph node).
/// Invalidation: None while a handle is held; the constants block location changes with every swap and is never
///   exposed.
/// Units: None.
/// Errors: Flux_GeometryError: FLUX_GEOMETRY_ERROR_CONSTANTS_TOO_LARGE, FLUX_GEOMETRY_ERROR_QUEUE_FULL.
/// Version: since Z2 Stage 2. Cooked data re-cooks on any change; the document migrates.
/// @endcontract
class Flux_MaterialResource
{
public:
	Flux_MaterialID GetID() const;
	Flux_MaterialDomain GetDomain() const;
	Flux_MaterialBlend GetBlend() const;
	bool IsTwoSided() const;
	uint64 GetVariantID() const;
	Zenith_RHIShaderProgramID GetModuleProgram() const;
	bool IsUsingFallbackTextures() const;       // some bound texture is not yet resident or failed
	// Editor and live tuning: replace the plain-data parameters (texture handle fields are ignored in xBytes); applied by
	// reallocate-and-swap of the constants block, never by an in-place write ([Z2-N-REND-252]).
	Zenith_Result<void, Flux_GeometryError> ReplaceParameters(Zenith_ByteSpan xBytes);
};

/// @contract type Flux_MaterialManager
/// Ownership: Owned by Flux_Renderer; owns every material, the material-constants pool, Flux_MaterialSlotCache and the
///   texture-to-material reverse index used by texture swaps.
/// Thread affinity: ANY_THREAD for Resolve; mutations on the Flux.BuildGraph node.
/// Invalidation: A pointer from Resolve is valid until the end of the current render frame.
/// Units: Bytes.
/// Errors: Flux_GeometryError.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Flux_MaterialManager
{
public:
	const Flux_MaterialResource* Resolve(Flux_MaterialID xID) const;
	Flux_MaterialID GetDefaultMaterial() const;   // "Material fallbacks"
	uint32 GetLoadedMaterialCount() const;
	uint64 GetConstantsBytesUsed() const;
};
```

## Material slots and constants

<!-- elaboration -->
| Element | Rule |
|---|---|
| constants pool | a `Flux_PersistentReadOnlyBuffer` of 16-byte granules (initial 4 MiB desktop, 1 MiB mobile; growth by resize-and-copy within the pending row "Material constants memory"); a TLSF allocator; one block per material, plus one per live parameter replacement |
| load | `Flux_MaterialKindLoader` (`Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"`): `BeginLoad` requests sections 20, 21 and 22 in one round into the load's staging; `OnSectionsLoaded` runs `Flux_ReadMaterial` (a failed row is `Fail(code)`), pushes the material onto the load queue and calls `Activate(pMaterial)` before any drain runs (the activation order of `Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"`). The referenced textures are the load's hard dependencies (`GetInfo().m_axDependencies`, each ACTIVE or FAILED with a fallback). On `Flux.BuildGraph` the drain allocates a block (a constants pool at its clamp is `ReportGpuFailed` with `FLUX_GEOMETRY_ERROR_POOL_FULL`: residency FAILED, and the sections draw with the default material of "Material fallbacks"), patches each texture binding's handle field with the texture's current `GetSRV()` (a tail-resident texture already has one; a texture not yet tail-resident or failed binds its usage's fallback) and `RequestUpload`s the block; when it completes, the staging is released, the material is `RESIDENT` ([Z2-N-REND-250]) and the loader calls `ReportGpuResident`. `Cancel` before activation (so before the push) releases the staging and ends in `AcknowledgeCancel`; `Unload` frees the block and every slot-record reference through deferred deletion and ends in `AcknowledgeUnload`; a device-recovery load re-creates the block for `m_pExistingObject`; `DropCpuCopies` returns 0 |
| material-slot record | one per (mesh section, material). The range of S records of one (mesh, material assignment) is allocated, reference counted and uploaded by the GPU scene (`Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`, "Slot allocators"). `Flux_MaterialSlotCache` ("Instancing" of `Zenith2/Docs/Design/GEOM/Meshes.md`) supplies each record's image with the fields of `Zenith2/Docs/Rendering/GpuResources.md → "Material-slot record (32 bytes)"`: variant index, flags from the info record, constants buffer and offset, alpha cutoff, and the three bucket IDs |
| bucket IDs | acquired by `Flux_MaterialSlotCache` when the GPU scene creates the range, and released when it frees the range, through `Flux_BucketRegistry::Acquire(const Flux_BucketKey&)` and `Release(uint32)`. The key, its derivation per kind (depth, colour, shadow; transparent domains a colour bucket alone), the 65,535 cap and `uFLUX_RG_INVALID` are `Zenith2/Docs/Design/RPROD/Culling.md → "Bucket registry"`'s; the stream-set of the key's `Zenith_RHIPermutationKey` is the mesh's raster code (`Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`) and its material variant is this material's variant ID. A slot whose `Acquire` returns `uFLUX_RG_INVALID` is not drawn in that kind, as RPROD states |
| texture swap | when a texture swaps ("Reallocate-and-swap realisation" of `Zenith2/Docs/Design/GEOM/Textures.md`), every material in the reverse index gets a new block with the new descriptor, and the scatter upload rewrites the `m_uConstantsOffset` of each slot record referencing it; the old block retires through deferred deletion |
| parameter replacement | the same path as a texture swap, with the caller's plain-data bytes; for a material bound to a live terrain, the four bytes at the terrain record offset are overwritten with the terrain's record index before packing |
| terrain record binding | `Flux_TerrainRenderer::AddTerrain` binds a material to a terrain's record index R (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`): the manager keeps the pair in a side table indexed by the material's slot and creates a new block holding R at the terrain record offset, on the parameter-replacement path; while the pair exists every block it creates for the material (a replacement, a texture swap's rewrite, a device-recovery load) holds R there; `RemoveTerrain` removes the pair |

The GPU scene reports every material-slot range it creates and frees through RHI's `Flux_GPUSceneRangeListener` (`Zenith2/Docs/Design/RHI/Memory.md → "Material-slot range listener"`, Z2T-RHI-126, cross-area row RPB23), called on the build node from `Flux_GPUScene::ApplyDeltas`; RPROD's `Flux_MaterialRangeForwarder` implements it and is the one call site of GEOM's `Flux_MaterialSlotCache` (`ZenithFlux/Internal/Geometry/Flux_MaterialSlotCache.h`): `void OnRangeCreated(Flux_MeshID, Zenith_Span<const Flux_MaterialID>, uint32 uFirstSlot, Zenith_Span<Flux_MaterialSlotRecord> axOut)` fills the images and acquires the buckets, and `void OnRangeFreed(uint32 uFirstSlot)` releases them. `OnRangeFreed` arrives when the range's reference count reaches zero during `ApplyDeltas` (its slots retire later), and a range created while no listener was set is never reported. GEOM supplies the primitive record images through `Flux_MeshWritePrimitiveRecords` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"`). Cross-area row GM23 is answered.

## Virtual texture bindings

<!-- elaboration: answers A3 row AV3 (a) and (b) -->
A texture field of a material may name a virtual texture (VT), ADVGFX's `ASSET_KIND_VIRTUAL_TEXTURE` (kind 18; `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material handle encoding"`). The field keeps its `DescriptorHandle` type, so no permutation distinguishes the two kinds.

| Element | Rule |
|---|---|
| cook, step 4 | a kind-18 GUID is accepted for a field of an `opaque` or `alphaTest` material when the VT's usage, read as for a streamed texture from its `geom.textureSource` header and import settings, matches the field's `[TextureUsage]`. Its binding record sets flags bit 0. In a `transparent` or `decal` material it fails with `GEOM_COOK_ERROR_MATERIAL_TEXTURE_USAGE`, naming the field and the domain, because those passes bind no VT pool |
| dependency | the VT is one of the material load's hard dependencies, like any bound texture (`GetInfo().m_axDependencies`). ADVGFX's `Unload` of a VT therefore runs only after every material that binds it has unloaded |
| handle written | for a binding with flags bit 0, the handle field holds `Flux_VirtualTextureResource::GetMaterialHandle()` (ADVGFX's `Flux_VirtualTexture.h`) while the VT's GPU residency is RESIDENT. Otherwise (loading, FAILED, or a replaced object not yet resident) it holds the SRV handle of the usage's fallback ("Fallback textures" of `Zenith2/Docs/Design/GEOM/Textures.md`), whose `m_uReserved` is 0 |
| rewrite | the manager keeps the list of its materials that have a virtual binding. On every `Flux.BuildGraph` frame, at GEOM frame step 2, it computes for each listed material the handle the row above gives and compares it with the handle in the material's current block. A difference writes a new block by the parameter-replacement path ("Material slots and constants"): a new block, the slot records rewritten by the scatter upload, and the old block retired through deferred deletion. This covers the VT's activation, its failure, a development reload and a device-recovery load. GEOM's step 2 precedes ADVGFX's VT drain at build-node step 3c, so a replaced VT object's quarantined slot is never the one a new block names |
| CPU readers | GEOM's CPU walks over material constants read a handle's word 0 alone. For a VT that word is the tail texture's real SRV index |
| counter | `Flux.Materials.VirtualHandleRewrites` counts the blocks the rewrite row wrote |

`Test_MaterialCook_VirtualTextureDomains` requires an `opaque` and an `alphaTest` material naming a kind-18 `COLOUR` product to cook with flags bit 0 set, and requires a `transparent` and a `decal` material naming it, and an `opaque` material naming a kind-18 `NORMAL` product in a `COLOUR` field, to fail with `GEOM_COOK_ERROR_MATERIAL_TEXTURE_USAGE`. `Test_Material_VirtualHandleRewrites` requires the block of a material that binds a VT fixture to hold the fallback handle while the VT loads, then the VT handle (tag `0xA` in bits 28–31 of word 1) from the frame it is resident, then the fallback again after an injected GPU failure. Each change writes one new block and retires the old one, and the old block's bytes stay unchanged until retirement.

## Material permutation enumeration

Every permutation is enumerated and compiled offline ([Z2-N-REND-103], [Z2-N-REND-124]); a material program's permutations are the product of its program-level keys, the stream-sets it draws and the game's material variants for the domains it draws ([Z2-N-REND-253]).

<!-- elaboration -->
| Step | Who | Output |
|---|---|---|
| 1 | the material cook, per material | its variant ID, domain, type ID and permutation values in the cook report's `materials` array |
| 2 | the cook's permutation step, per game and platform, after every material and mesh is cooked | `material_variants.json`: the distinct (type ID, variant ID, domain, module, permutation values) set, sorted by variant ID — the shader compiler's `--material-set` input; `stream_sets.json` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Precision fallback report format"`) |
| 3 | the permutation step | the used (program, material variant, stream-set) triples: for every mesh section drawn with a material, the variant, the mesh's raster stream-set and each pass program its domain draws in (the domain table of `Zenith2/Docs/Rendering.md → "Material classes and buckets"`) for each shading path the platform enables |
| 4 | the shader compiler in cook mode | the compiled set: per program, `compiledKeys × {stream-sets it can draw ∩ used}` × {variants of the domains it draws} |
| 5 | the permutation step | every used triple must be in the compiled set, else `COOK_ERROR_PERMUTATION_MISSING` naming the asset, the program and the key ([Z2-N-REND-125]) |

```json
{ "schemaVersion": 1, "game": "OpenWorldSlice", "platform": "windows",
  "variants": [ { "variantId": "0x51c0…", "typeId": "0x9b1f…", "module": "Zenith2/Shaders/Materials/Standard.slang",
                  "type": "Material_Standard", "domain": "opaque", "permutation": { "bPERM_DETAIL_NORMAL": false } } ] }
```

## Permutation limit check and report

| Count | Definition |
|---|---|
| per program | the compiled permutations of step 4 (keys × stream-sets × variants, every entry point counted once) |
| per game | the sum over every program in the game's package (engine and material programs) and every platform tier |
| limit | `Z2-B-RPROD-024` Shader permutation limit per game (pending) |

- The cook report's `permutations` member lists, per program, the key count, stream-set count, variant count and product, and the per-game total and limit, sorted by program path.
- The shader compiler exits 5 and the cook fails with `COOK_ERROR_PERMUTATION_LIMIT` when the per-game total exceeds the limit ([Z2-N-REND-126]); the error lists the ten largest programs.
- The limit is read from the configuration key `cook.permutationLimitPerGame`, whose engine default is generated from the Budgets row by the budget-table generator; while the row is pending the generated default is 0, the check reports without failing and the report records `"limitStatus": "pending"`.

<!-- assumption: pending CORE -->
The budget-table generator of `Zenith2/Docs/Design/CORE/MemoryGovernor.md → "Generated budget table"` also emits the tool-side key default for `Z2-B-RPROD-024` (a non-memory row); the ticket implementing the limit check adds that row to the generator's input set.
<!-- /assumption -->

## Transparency

<!-- elaboration -->
| Aspect | Rule |
|---|---|
| passes | `transparent` materials draw in `TransparentForward` alone (`Zenith2/Docs/Rendering/PerPass/Common.md → "TransparentForward"`), after the opaque world, reading depth and writing no depth |
| blend states | `alpha`: colour `src × a + dst × (1 − a)`, alpha `a + dst_a × (1 − a)`; `premultiplied`: colour `src + dst × (1 − a)`; `additive`: colour `src × a + dst`, alpha unchanged; each is one entry of the fixed blend-state table ([Z2-N-REND-197]) |
| order | one command per transparent instance-section, sorted back to front by the view-space depth of the bounds centre plus `sortBias`, ties by instance then section ([Z2-N-REND-661]); there is no order-independent transparency and no per-triangle sort |
| two-sided | a two-sided transparent material with `twoSidedOrder` `backThenFront` emits two consecutive entries per section, back faces (cull front) then front faces (cull back), with the same sort key, so a closed object's interior renders behind its exterior; `single` emits one cull-none entry |
| refraction | `refraction` sets `bPERM_REFRACTION`, which reads the opaque colour copy through `Flux_MaterialInput`'s screen position and the material's `m_fIOR` and `m_fThickness` plain parameters (Standard transparent reads them) |
| shadows | `castShadows` `alphaTest` adds the material to the shadow depth alpha-tested buckets with `shadowAlphaCutoff`; `none` casts no shadow |
| fog | applied per pixel in-shader ([Z2-N-REND-736]) |
| LOD | chosen on the CPU at collection with `Flux_SelectLODCPU` (`Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"`) |

<!-- assumption: pending RPROD -->
The `backThenFront` pair of entries is a GEOM addition to the collection of `Zenith2/Docs/Rendering/Culling.md → "Transparent commands"`; RPROD adds the rule (and the cull-mode run break it causes) to that table in the diff that implements two-sided transparency.
<!-- /assumption -->

## Decals

Decals are `decal`-domain materials projected through an oriented box. They are applied to `Flux_SurfaceData` in Forward+ and the visibility buffer (clustered), in Deferred to the G-buffer, and in Forward per object (`Zenith2/Docs/Rendering.md → "Material classes and buckets"`).

<!-- elaboration -->
### Decal records

`Flux_DecalRecord` is the `[FluxShared]` struct of `Common/GPUScene.slang` (Rendering names it; GEOM gives its layout), 96 bytes, written per frame into the upload ring:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 48 | `m_xWorldToDecal` | float3x4 row-major: world to the decal's unit box [−0.5, 0.5]³ |
| 48 | 12 | `m_xBoundsCentre` | world-space centre |
| 60 | 4 | `m_fBoundsRadius` | world-space bounding radius |
| 64 | 4 | `m_uMaterialSlot` | the decal material's material-slot record (constants) |
| 68 | 4 | `m_uChannelMask` | bit 0 albedo, 1 normal, 2 roughness, 3 metallic, 4 AO, 5 emissive |
| 72 | 4 | `m_fOpacity` | [0, 1], the instance's opacity × its distance fade |
| 76 | 4 | `m_fAngleFadeCosStart` | cosine of `angleFadeStartDegrees` |
| 80 | 4 | `m_fAngleFadeCosEnd` | cosine of `angleFadeEndDegrees` |
| 84 | 4 | `m_uLayerMask` | receives only on instances whose `m_uLayerMask` intersects it |
| 88 | 4 | `m_iSortPriority` | sint32 |
| 92 | 4 | `m_uFlags` | bit 0 affects skinned receivers; bits 1–31 zero |

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Decal.h
/// @contract value Flux_RenderDecal
/// Ownership: Plain value written into the render world by an extractor.
/// Thread affinity: IMMUTABLE after Render.Seal.
///   Members non-const: the extractor fills each record field by field in the render world before Render.Seal.
/// Invalidation: None.
/// Units: Metres; opacity dimensionless.
/// Errors: Flux validates it: a null or stale material, a non-decal material, or a singular transform drops the decal
///   for the frame and counts Flux.Decals.Rejected.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_RenderDecal
{
	uint64 m_ulStableKey = 0;        // the decal's persistent-ID hash: the final tie-break of decal order
	Zenith_Matrix4 m_xDecalToWorld;   // the unit box [−0.5, 0.5]³ to world
	Flux_MaterialID m_xMaterial;
	float m_fOpacity = 1.0f;
	uint32 m_uLayerMask = 0xFFFFFFFFu;
	uint32 m_uFlags = 0;
};
```

<!-- assumption: pending RPROD -->
`Flux_RenderDecal` is the element type of `Flux_RenderWorldSection::m_axDecals`; GEOM declares it here.
<!-- /assumption -->

### Decal order and application

| Step | Rule |
|---|---|
| order | Flux sorts the frame's decals on the CPU by (`m_iSortPriority` ascending, `m_ulStableKey` ascending) before writing the records; record index is the application order |
| lists | the light and decal list build and the clustering keep decal indices in ascending record order within each list (LIGHT's passes) |
| decal material | a `Flux_DecalMaterialType` with `static Flux_DecalOutput EvaluateDecal(Flux_DecalInput xIn, Params xParams)`: input is the decal-space position, its UV `(x + 0.5, 0.5 − z)` projected along the box's −Y axis, explicit gradients from the receiver's, and the receiver's normal; output is albedo, opacity, a tangent-space normal, roughness, metallic, AO, emissive |
| fade | `fade = opacity × saturate((dot(receiverNormal, −boxY) − cosEnd) / (cosStart − cosEnd))`, and 0 outside the box |
| blend into `Flux_SurfaceData` | per channel in the mask: `albedo = lerp(albedo, dAlbedo, dOpacity × fade)`; normal by reoriented normal-map blending of the decal's tangent-space normal onto the receiver normal, then `lerp` by the same weight and renormalise; scalar channels by `lerp` |
| Forward | at most 4 decals per object, chosen by projected overlap by `Zenith2/Docs/Rendering/PerPass/Forward.md → "PerObjectLightLists"`, applied in record order |
| Deferred | the `DeferredDecals` pass blends the same channels into GB0–GB2 with write masks from the channel mask ([Z2-N-REND-843]), permutation `uPERM_CHANNELS` from the mask; it is declared by GEOM's `Flux_Features.h` function `Flux_AddDeferredDecals` (`Zenith2/Docs/Design/RPROD/Passes.md → "Flux_Features header"`), defined in `ZenithFlux/Internal/Materials/Flux_DeferredDecals.cpp`: a no-pass body from Z2T-GEOM-132, so RPROD's Deferred path links (A5 row RPB24), until Z2T-GEOM-107 declares the pass |

## Advanced surface models

Classification of the advanced surface models of Z2-I-GEOM-22. Each is required in Stage 5 (Roadmap Stage 5 deliverable 4, advanced lighting and materials), on every shading path. GEOM owns the surface data, the material-side parameters and the BRDF lobe definitions below; LIGHT integrates them into `EvaluateLighting` (`Zenith2/Docs/Rendering.md → "Single-source lighting"`).

<!-- elaboration -->
| Model | Classification | Parameters added to `Flux_SurfaceData` | Lobe |
|---|---|---|---|
| clear coat | required, Stage 5 | `m_fClearCoat` (half, [0, 1]), `m_fClearCoatRoughness` (half, perceptual) | an isotropic GGX specular lobe with fixed F0 = 0.04 over the base; the base lobes are attenuated by `1 − F_c(v·h) × clearCoat`; the coat uses the geometric normal (no coat normal map in v1) |
| anisotropy | required, Stage 5 | `m_fAnisotropy` (half, [−1, 1]), `m_xAnisotropyDirection` (half2, tangent space) | anisotropic GGX with `α_t = max(α(1 + a), 0.001)`, `α_b = max(α(1 − a), 0.001)`, α = roughness², and the height-correlated anisotropic Smith visibility |
| sheen | required, Stage 5 (shading model `cloth`) | `m_xSheenColour` (half3, linear), `m_fSheenRoughness` (half) | the "Charlie" distribution of Estevez and Kulla with the Neubelt–Pettineo visibility; the base diffuse and specular are scaled by `1 − max(sheenColour) × E_sheen(n·v)` from a 32 × 32 R16F albedo LUT cooked with the engine |
| subsurface approximation | required, Stage 5 (shading model `subsurface`) | `m_xSubsurfaceColour` (half3), `m_fThickness` (half, [0, 1]) | wrapped diffuse `saturate((n·l + w) / (1 + w)²)` with `w = 0.5`, tinted by the subsurface colour, plus a thickness-attenuated back transmission `(1 − thickness) × saturate(−n·l) × subsurfaceColour`; no screen-space diffusion pass |

| Aspect | Rule |
|---|---|
| encoding | `m_uShadingModel` keeps bits 0–1 as the base model (`FLUX_SHADING_MODEL_DEFAULT_LIT` 0, `_UNLIT` 1, `_SUBSURFACE` 2, `_CLOTH` 3) and adds bit 2 `FLUX_SURFACE_MODEL_FLAG_CLEAR_COAT` and bit 3 `FLUX_SURFACE_MODEL_FLAG_ANISOTROPY`; the 8-bit value is what GB2 alpha stores ([Z2-N-REND-842]) |
| Forward+ and Forward | the model is a link-time property of the material type, so the colour program's lighting takes the extended branch only in variants whose type declares it; no new program permutation |
| Deferred and visibility buffer | pending the Rendering change of the assumption below (ruling X80, accepted, not yet applied). Until it lands the extended models are evaluated on Forward+ and Forward alone: a material type declaring an extended model cooks for those two paths, and a game whose platform profile selects Deferred or the visibility buffer and has such a material resident fails the cook with `GEOM_COOK_ERROR_MATERIAL_SCHEMA`, naming the material, the model and the path |
| energy | every model passes white-furnace energy conservation (directional albedo ≤ 1 + 1e-3) and reciprocity in the CPU reference BRDF of `Zenith2/Docs/Rendering.md → "Independent analytical material tests"` |

<!-- assumption: pending REND -->
Ruling X80 (accepted, recorded in the cross-area queue, not yet applied to the core docs) adds an advanced-surface G-buffer target to the desktop-only Deferred and visibility-buffer paths, at most 16 bits per channel and present only in an "advanced surfaces" pass permutation, so it costs nothing when no material with an extended model is resident. [Z2-N-REND-841] ("all five colour targets") and [Z2-N-REND-842] (the exact G-buffer format list) do not yet admit it, and `Zenith2/Docs/Rendering/PerPass/VisibilityBuffer.md → "MaterialResolve"` declares four surface-data targets. Once Rendering applies X80, GEOM's design for those paths is: the extended parameters go into that target as GB4 `R8G8B8A8_UNORM` (clear coat and coat roughness, or anisotropy and direction angle, or sheen colour, or subsurface colour and thickness, by model), written only by the advanced-surfaces permutation of the G-buffer and material-resolve passes, and read by deferred lighting per pixel on the model bits; the fallback row above is then removed. The Stage 5 surface-model tickets extend `Flux_SurfaceData` in `Zenith2/Shaders/Common/Material.slang` and edit `Zenith2/Docs/Rendering.md → "Materials"` in the same diffs (Z2-D-085).
<!-- /assumption -->

## Material fallbacks

| Situation | Behaviour | Counter |
|---|---|---|
| a mesh section's material is null, stale, failed to load or not yet `RESIDENT` | the section draws with the default material: `Zenith2/Shaders/Materials/Standard.material.json` with the engine's neutral fallback textures in shipping builds, and `Materials/DevMissing.material.json` (a magenta and black checker, `Material_Unlit`) in development builds | `Flux.Materials.FallbackSections` |
| a bound texture is not resident or failed | the material's block references the texture's tail mips, or the fallback texture of its usage class (`Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"`) | `Flux.Textures.FallbackBindings` |
| a material variant absent from the loaded shader container | the missing-PSO fallback of [Z2-N-REND-192], never a compile | `Flux.PSO.MissBuckets` |
| a decal with a missing material | the decal is dropped for the frame | `Flux.Decals.Rejected` |

Each fallback logs once per (asset, run) under `flux.materials`, with the asset GUID.

## Material tests

Rendering's material tests ([Z2-N-REND-255]) are implemented here with the files named; GEOM adds the rest.

<!-- elaboration -->
| Test | File | Asserts |
|---|---|---|
| `Test_Material_DocumentRoundTrip` | `Zenith2/Engine/ZenithFlux/Tests/Materials/Flux_MaterialDocument_Tests.cpp` | read then write of every fixture document is byte-identical in canonical order |
| `Test_Material_UnknownFieldPreserved` | same | an unknown top-level key survives a round trip and warns once |
| `Test_Material_MigrationFixtures` | same | every `Tests/Fixtures/Migration/Material/v<N>.json` migrates to its expected output |
| `Test_Material_CookFromGeneratedDocument` | `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_CookMaterial_Tests.cpp` | a `Material_TerrainLayered` document written as the `geom.materialDocument` blob of a declared product cooks to a `.zmat` under the product's GUID whose info record's terrain record offset is the reflected offset of `m_uTerrainRecord` and whose constants hold 0 there; `Material_Standard` records 0xFFFFFFFF; a document whose `id` differs from the item's GUID, a product with no blob, and an `int m_uTerrainRecord` field fail with their errors; the reader rejects a file whose offset is 2, or overlaps a texture binding, with `CORRUPT_SECTION` |
| `Test_Material_CookRejectsTypeMismatch` | `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_CookMaterial_Tests.cpp` | a string where a float is declared fails naming the key and the expected type |
| `Test_Material_CookRejectsUnknownPermutationValue` | same | a value outside the declared set fails |
| `Test_Material_CookRejectsUnknownName` | same | unknown parameter, texture, sampler and permutation names each fail |
| `Test_Material_CookRejectsMissingTexture` | same | a non-optional texture field left unbound fails |
| `Test_Material_CookTextureUsageChecked` | same | a colour texture bound to a normal field fails |
| `Test_Material_VariantIDStable` | same | the variant ID of a fixture equals its committed value, and changes when a permutation value changes and not when a parameter changes |
| `Test_Material_ConstantsPackedAtReflectedOffsets` | same | each parameter lands at its reflected offset, little-endian, defaults elsewhere |
| `Test_Material_SlotRebindNoPSOChange` | `Flux_MaterialManager_Tests.cpp` | rebinding a section to another material of the same variant changes one slot record and creates no PSO |
| `Test_Material_ParameterSwapNeverInPlace` | same | a parameter replacement writes a new block; frames in flight read the old block until retirement |
| `Test_Material_TextureSwapRewritesBlocks` | same | a texture swap rewrites every referencing material's block and slot records |
| `Test_Material_FallbackForMissing` | same | a missing material draws the development checker, and in shipping configuration the default |
| `Test_Material_ReaderRejectsCorruption` | `Zenith2/Engine/ZenithFlux/Tests/Materials/Flux_MaterialReader_Tests.cpp` | one mutated fixture per row of "Material reader validation" (bad magic; a missing constants section; a duplicated info section; domain 4; a transparent material with blend none; a flags bit 5; a constants size of 1,028; a constants size disagreeing with its section; a binding offset of 1,020 in a 1,024-byte block; two bindings at one offset; usage class 7) returns that row's error, allocates nothing and leaves the material `FAILED` |
| `Test_Permutations_EnumerationMatchesUse` | `Zenith_CookPermutations_Tests.cpp` | the fixture game's used triples are all compiled, and nothing unused is compiled for material programs |
| `Test_Permutations_MissingIsCookError` | same | removing a variant from the compiled set fails with `COOK_ERROR_PERMUTATION_MISSING` |
| `Test_Permutations_LimitIsCookError` | same | a limit of 1 fails with `COOK_ERROR_PERMUTATION_LIMIT` listing the largest programs; a pending limit reports and passes |
| `Test_Permutations_ReportDeterministic` | same | two cooks write byte-identical `permutations` members |
| `Test_Transparent_BlendStates` | `Flux_Transparency_Tests.cpp` | each blend mode composites a fixture over a known background to the expected colour within 1/255 |
| `Test_Transparent_TwoSidedBackThenFront` | same | a two-sided sphere draws its back faces before its front faces |
| `Test_Transparent_ShadowMode` | same | `castShadows` `alphaTest` produces shadow-atlas depth; `none` produces none |
| `Test_Decal_OrderDeterministic` | `Flux_Decal_Tests.cpp` | overlapping decals of equal priority apply in stable-key order on every path that supports decals |
| `Test_Decal_ChannelMask` | same | a normal-only decal leaves albedo bit-identical |
| `Test_Decal_AngleFade` | same | a receiver beyond the fade end receives nothing |
| `Test_Decal_DeferredDecalsStubAddsNoPass` | same | Z2T-GEOM-132's no-pass `Flux_AddDeferredDecals` leaves a deviceless graph with no pass and no resource and the view targets unchanged; deleted by Z2T-GEOM-107, which declares the pass |
| `Test_Material_StandardWeatherApplied` | `Flux_Material_Tests.cpp` | through a compute harness of `Material_Standard`, wetness 1 and porosity 0.5 darken albedo and lower roughness by `Env_ApplySurfaceWeather`'s formula within 2^−10, wetness 0 leaves the surface bit-identical, and the default `m_fPorosity` is 0.5 |
| `Test_Material_DocumentTypesRegistered` | `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_MaterialDocument_Tests.cpp` (through `Authoring/Zenith_AuthoringGeom.h`) | `Zenith_RegisterMaterialMigrations` from ZenithAuthoring registers `Flux.Material` at version 1 on a fresh registry; a second registration is refused with the registry's error |
| `Test_BRDF_ClearCoatWhiteFurnace`, `Test_BRDF_AnisotropyWhiteFurnace`, `Test_BRDF_SheenWhiteFurnace`, `Test_BRDF_SubsurfaceWhiteFurnace` | `Zenith2/Engine/ZenithFlux/Tests/ReferenceBRDF/` | directional albedo ≤ 1 + 1e-3 over a roughness and parameter sweep, and the shader matches the CPU reference |
| `Test_Material_ExtendedModelDeferredRejected` | `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_CookMaterial_Tests.cpp` | while ruling X80 is unapplied, a clear-coat fixture material cooks for Forward+ and Forward and fails with `GEOM_COOK_ERROR_MATERIAL_SCHEMA` for a Deferred profile, naming the material, the model and the path |
| `Test_Material_ExtendedGBufferRoundTrip` | `Flux_Material_Tests.cpp` | once ruling X80 is applied: each extended model's parameters survive GB4 encode and decode within one UNORM8 quantum, and the advanced-surfaces permutation is the only one that writes GB4 |
| `Test_ShaderChecks_MaterialImplicitDerivative_Fails` | `Zenith2/Tools/ZenithShaderCompiler/Tests/` | the lint fixture of [Z2-N-REND-255] |
| `Test_MaterialCook_VirtualTextureDomains` | `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_CookMaterial_Tests.cpp` | "Virtual texture bindings" |
| `Test_Material_VirtualHandleRewrites` | `Zenith2/Engine/ZenithFlux/Tests/Materials/Flux_MaterialManager_Tests.cpp` | "Virtual texture bindings" |
| `Test_MaterialCook_PublishesProxyColour` | `Zenith2/Tools/ZenithCook/Tests/Materials/Zenith_CookMaterial_Tests.cpp` | step 9 of "Material cook": `Standard.material.json` with base colour factor (0.5, 1, 1, 1) and a `COLOUR` texture of average (0.2, 0.4, 0.6, 1) publishes (0.1, 0.4, 0.6, 1); a type without `m_xBaseColourFactor` and without a colour texture publishes (1, 1, 1, 1) |
