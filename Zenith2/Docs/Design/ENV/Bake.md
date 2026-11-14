# ENV design: documents, cook, formats and generation hooks

Section file of `Zenith2/Docs/Design/ENV.md`. It specifies the JSON schemas of ENV's authoring documents, the ENV cook steps (run by ZenithCook, ASSET's cook orchestration, `Zenith2/Docs/Design/ASSET.md → "Authoring and cooking"`), every ENV cooked format inside ASSET's cooked container, the collision and navigation generation hooks (PHYS and AI consume their output; ENV supplies the geometry), and the content ENV contributes to a streaming cell (STREAM owns cells; ENV defines what its part of a cell holds).

## Scope of this file

| Concern | Heading |
|---|---|
| JSON schemas, value types, the registered document types | "Authoring document schemas" |
| the cook's step graph and determinism | "Cook pipeline" |
| terrain, foliage and parameter cooks | "Terrain cook", "Foliage cook", "Parameter document cook" |
| binary formats | "Cooked formats" |
| the loaded objects of ENV's kinds | "Runtime asset objects" |
| collision | "Collision generation hook" |
| navigation | "Navigation generation hook" |
| edits after cooking | "Regeneration after edits" |
| streaming cells | "Streaming cell content" |

## Authoring document schemas

Every ENV authoring document is UTF-8 JSON read and written through CORE's JSON reader and writer (`Zenith2/Docs/Design/CORE/Text.md → "JSON reader"`, `"JSON writer"`) with the parser limits of [Z2-N-ARCH-953], carries `schemaVersion` (a `uint32`, 1 in the stage that introduces the type: Stage 3 for the terrain, tile, layer, foliage type, placement and atmosphere documents, Stage 4 for weather, water and cloud noise) and `id` (the document's `Zenith_PersistentID` in its 36-character form, [Z2-N-CONTRACT-067]), preserves unknown members with a warning, and is written through the journaled atomic write of SCENE's migration framework ([Z2-N-CONTRACT-540]), with which every type is registered tool-side ("Environment document types"). The nine types join the authoring-document list of [Z2-N-CONTRACT-552] in the diff that introduces each.

| Type | Extension | Heading |
|---|---|---|
| terrain | `*.terrain.json` | "Terrain document schema" |
| terrain tile | `*.terraintile.json` | "Terrain tile document schema" |
| terrain layer | `*.terrainlayer.json` | "Terrain layer document schema" |
| foliage type | `*.foliage.json` | `Zenith2/Docs/Design/ENV/Foliage.md → "Foliage type document"` |
| placement rule set | `*.placement.json` | `Zenith2/Docs/Design/ENV/Procedural.md → "Placement rule document"` |
| atmosphere | `*.atmosphere.json` | `Zenith2/Docs/Design/ENV/Atmosphere.md → "Atmosphere document"` |
| weather | `*.weather.json` | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather document"` |
| water | `*.water.json` | `Zenith2/Docs/Design/ENV/WaterWeather.md → "Water document"` |
| cloud noise | `*.cloudnoise.json` | "Cloud noise document schema" |

### Terrain document schema

```json
{
	"schemaVersion": 1,
	"id": "9c3e2f10-6a7b-4c8d-9e0f-1a2b3c4d5e6f",
	"name": "Valley",
	"origin": [ -1024.0, 0.0, -1024.0 ],
	"tiles": { "x": 16, "z": 16 },
	"heightLimits": [ -256.0, 1792.0 ],
	"lodDistanceRatio": 8.0,
	"placementSeed": "0x5ad1c0ffee0123ab",
	"layers": [
		{ "id": "c18e…", "document": "4a7d…-guid", "uvScale": null, "heightBlendDepth": 0.2, "physicalMaterial": "Grass", "navigationArea": "ground" }
	],
	"physicalMaterials": [ "Grass", "Rock", "Sand" ],
	"foliageTypes": [ "5b0f…-guid" ],
	"placementRuleSets": [ "7e2a…-guid" ],
	"exclusionSources": [ "../Scenes/Valley.scene.json" ],
	"tileFiles": [ { "x": 0, "z": 0, "file": "Valley/Tiles/x000_z000.terraintile.json" } ],
	"collision": { "enabled": true },
	"navigation": { "enabled": true, "maxWalkableSlopeDegrees": 45.0 }
}
```

| Member | Rule |
|---|---|
| `origin` | three finite floats; the minimum corner |
| `tiles` | `x`, `z` in 1 … 256 |
| `heightLimits` | min < max, width ≤ 8,192 m |
| `lodDistanceRatio` | 8 … 64, default 8: GEOM's CDLOD distance ratio r, which `Flux_TerrainRenderer::AddTerrain` refuses below 8 (`Zenith2/Docs/Design/GEOM/Meshes.md → "Node selection"`); `Load` refuses a value outside the range with `ENV_ERROR_DOCUMENT_SCHEMA` naming the field, and the cook writes it into GEOM's section 0x0101 |
| `placementSeed` | a `uint64` written as a 0x-prefixed 16-digit hexadecimal string (JSON numbers cannot hold 64 bits exactly) |
| `layers` | 1 … 16 entries; entry 0 is the base layer; ids unique; `uvScale` `null` uses the layer document's |
| `physicalMaterials` | names resolved against PHYS's physical-material table at cook (pending PHYS); `physicalMaterial` of a layer names one of them |
| `navigationArea` | `ground`, `road`, `offPath`, `unwalkable` ("Area flags") |
| `foliageTypes` | 0 … 64 foliage-type GUIDs; density maps and explicit instances index this list |
| `placementRuleSets` | 0 … 16 placement GUIDs |
| `exclusionSources` | 0 … 64 scene document paths relative to the terrain document; the importer reads each through `ReadInput` and takes the `Zenith.Environment.PlacementExclusion` components of its entities, by stable name, with their entities' world transforms (SCENE's scene document layout); their bytes enter the import key |
| `tileFiles` | one entry per present tile, coordinates in range and unique; `file` is relative to the terrain document's directory |

A terrain's extent is always a whole number of 128 m tiles, so GEOM's extent rule (`COOK_ERROR_TERRAIN_EXTENT`, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"`) holds by construction.

### Terrain tile document schema

```json
{
	"schemaVersion": 1,
	"terrain": "9c3e2f10-6a7b-4c8d-9e0f-1a2b3c4d5e6f",
	"x": 0, "z": 0,
	"nextExplicitId": 12,
	"heights": { "format": "f32le", "width": 257, "height": 257, "data": "<base64>" },
	"layerWeights": [ { "layer": "c18e…", "format": "u8", "width": 256, "height": 256, "data": "<base64>" } ],
	"holes": null,
	"density": [ { "foliageType": 0, "format": "u8", "width": 128, "height": 128, "data": "<base64>" } ],
	"exclusion": [ { "ruleSet": "all", "format": "u8", "width": 256, "height": 256, "data": "<base64>" },
	               { "ruleSet": "7e2a…-guid", "format": "u8", "width": 256, "height": 256, "data": "<base64>" } ],
	"explicitInstances": [ { "id": 7, "foliageType": 0, "position": [ 12.5, 0.0, 88.25 ], "yaw": 1.2, "scale": 1.05, "keepHeight": false } ]
}
```

- Each `data` string is the base64 (RFC 4648, no line breaks) of one COMPRESS section: the 24-byte `Zenith_CompressedSectionDesc` followed by the section bytes (`Zenith2/Docs/Design/COMPRESS.md → "Compressed section descriptor on disk"`), little-endian row-major payload, zstd at the editor's authoring level (default 3) unless incompressible, in which case the section is stored raw (the descriptor says so); the reader selects the codec from the descriptor, never from configuration (Z2-D-004). The JSON wrapper keeps the tile an authoring document in JSON (Z2-D-038) and one file per tile keeps concurrent editing and merges local to a tile.
- `layerWeights` omits the base layer (its weight is the complement); `holes` is `null` or one u8 map; the base64 of an absent map is never written.
- `explicitInstances` are sorted by `id` (each below 2^31); positions are tile-local metres.
- `exclusion[].ruleSet` is `"all"` or the GUID of one of the terrain's `placementRuleSets`; a GUID the terrain does not list is `ENV_ERROR_DOCUMENT_SCHEMA`. At most one mask per value.

### Merging terrain documents

Concurrent editing and merges stay local to a tile because each tile is its own file, but a tile's payloads are compressed blobs no structural merge can combine. CONTENT's structure-aware merge driver (plan Appendix A, CONTENT row) therefore treats the documents as follows (pending CONTENT, row EN20):

| Document | Merge policy |
|---|---|
| `*.terraintile.json` | by file: a tile changed on both sides is a conflict resolved by taking one side's file whole (the editor offers either side and a preview of both); a tile changed on one side merges cleanly |
| `*.terrain.json` | structurally, keyed: `tileFiles` by (x, z), `layers` by layer `id`, `physicalMaterials` by name, `foliageTypes` and `placementRuleSets` by GUID; order changes of `layers`, `foliageTypes` or `placementRuleSets` on both sides are a conflict, because palette and list order are meaningful (the base layer, rule-set priority); scalar members (origin, tiles, limits, seed, `lodDistanceRatio`) conflict when both sides change them |
| the other ENV documents | CONTENT's ordinary JSON rules: arrays of objects with `name` or `id` merge by that key |

### Terrain layer document schema

```json
{
	"schemaVersion": 1,
	"id": "4a7d…",
	"albedo": "…guid", "normal": "…guid", "ormh": "…guid",
	"uvScale": 0.25,
	"heightBlendDepth": 0.2,
	"physicalMaterial": "Grass",
	"navigationArea": "ground",
	"porosity": 0.6
}
```

The textures are GEOM textures whose import presets declare usage COLOUR, NORMAL and MASK respectively (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`); the import fails with `ENV_ERROR_DOCUMENT_SCHEMA` ("Cook determinism") for a texture of another usage. `porosity` feeds `Env_ApplySurfaceWeather`.

### Cloud noise document schema

```json
{
	"schemaVersion": 1,
	"id": "b3e0…",
	"volume": "base",
	"size": 128,
	"seed": "0x436c6f7564734261"
}
```

| Member | Rule |
|---|---|
| `volume` | `base` or `detail`: the channel layout of `Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise volumes"` |
| `size` | 32, 64, 128 or 256 texels per axis |
| `seed` | a `uint64` as a 0x-prefixed 16-digit hexadecimal string |

### Document value types

The readers of `Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentDocuments header"` return these types, declared in that header before the readers; each member is the document member of the same name after the unit conversion the member's comment states, and strings and spans borrow the reader's arena.

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentDocuments.h (the value types)
/// @contract enum Zenith_FoliageClass
/// Ownership: Plain enumeration of the foliage type document's "class".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the reader refuses any other "class" string (ENV_ERROR_DOCUMENT_SCHEMA).
/// Version: since Z2 Stage 3 (FOLIAGE_CLASS_DETAIL used from Z2 Stage 4); values are stable.
/// @endcontract
enum Zenith_FoliageClass : uint8 { FOLIAGE_CLASS_PLACED = 1, FOLIAGE_CLASS_DETAIL };

/// @contract value Zenith_FoliageTypeDocument
/// Ownership: Plain value returned by Zenith_ReadFoliageTypeDocument; spans and strings borrow the reader's arena.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena.
/// Units: Metres; radians are not used (alignToNormal is a fraction); linear RGB colours.
/// Errors: None after the reader, which validates the table of Zenith2/Docs/Design/ENV/Foliage.md "Foliage type document".
/// Version: since Z2 Stage 3 (the detail block since Z2 Stage 4); the document's own version is m_uSchemaVersion.
/// @endcontract
struct Zenith_FoliageTypeDocument
{
	uint32 m_uSchemaVersion = 0;
	Zenith_PersistentID m_xID;
	Zenith_StringView m_strName;
	Zenith_FoliageClass m_eClass = FOLIAGE_CLASS_PLACED;
	Zenith_PersistentID m_xMesh;                              // placed; null for detail
	Zenith_Span<const Zenith_PersistentID> m_axMaterialOverrides;   // one per mesh section; null ID = keep the mesh's
	float m_fFootprintRadius = 0.5f;
	float m_fScaleMin = 1.0f, m_fScaleMax = 1.0f;
	float m_fAlignToNormal = 0.0f;
	bool m_bRandomYaw = true;
	bool m_bCastShadows = true;
	float m_fCullDistance = 1000.0f;
	bool m_bHasCollision = false;
	Zenith_FoliageCollisionShape m_eCollisionShape = FOLIAGE_COLLISION_CAPSULE;
	float m_fCollisionRadius = 0.0f, m_fCollisionHeight = 0.0f;
	Zenith_Vector3 m_xCollisionHalfExtents;
	Zenith_PersistentID m_xCollisionHull;                     // convex
	Zenith_StringView m_strPhysicalMaterial;
	bool m_bGameplayVisible = false;
	bool m_bRenderOnly = false;
	float m_fWindStiffness = 0.8f, m_fWindTrunkBend = 0.0f, m_fWindBranchFlutter = 0.0f, m_fWindLeafFlutter = 0.0f;
	float m_fWindPhaseVariation = 1.0f;
	bool m_bHasDetail = false;
	float m_fBladeHeightMin = 0.2f, m_fBladeHeightMax = 0.5f, m_fBladeWidth = 0.02f, m_fBendMax = 0.3f, m_fCardSize = 0.5f;
	Zenith_Vector3 m_xColourBase, m_xColourTip;
	float m_fColourVariation = 0.0f;
	float m_fDensityPerSquareMetre = 1.0f;
	Zenith_Span<const Zenith_PersistentID> m_axDetailLayerIDs;    // the detail block's layers, in document order
	Zenith_Span<const float> m_afDetailLayerMinWeights;           // same count
	Zenith_PersistentID m_xCoverageMask;                          // null: none
};

/// @contract value Zenith_AtmosphereDocument
/// Ownership: Plain value returned by Zenith_ReadAtmosphereDocument.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: None.
/// Units: SI (the reader converts kilometres, per-kilometre coefficients and degrees).
/// Errors: None after the reader, which applies Zenith_ValidateAtmosphere and the limits of Atmosphere.md "Validation limits".
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AtmosphereDocument
{
	uint32 m_uSchemaVersion = 0;
	Zenith_PersistentID m_xID;
	Zenith_AtmosphereParameters m_xParameters;
	Zenith_TimeOfDaySettings m_xTimeOfDay;
	float m_fCloudLayerAltitude = 1800.0f;
	float m_fCloudLayerThickness = 900.0f;
	float m_fCloudNoiseScale = 0.00012f;
	Zenith_PersistentID m_xCloudBaseNoise, m_xCloudDetailNoise;   // .zcloudnoise assets; null: no clouds / base only
	Zenith_PersistentID m_xMoonTexture, m_xStarCube;
};

/// @contract value Zenith_WeatherStateDocument
/// Ownership: Plain value inside a Zenith_WeatherDocument.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena.
/// Units: Seconds; the parameters' units (radians after conversion).
/// Errors: None after the reader.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WeatherStateDocument
{
	Zenith_StringView m_strName;
	uint64 m_ulNameHash = 0;                                 // Zenith_Hash64 of the name
	float m_fDwellMinSeconds = 1.0f, m_fDwellMaxSeconds = 1.0f;
	Zenith_WeatherParams m_xParams;
	Zenith_Span<const Zenith_PersistentID> m_axEffectPrefabs; // at most 8
};

/// @contract value Zenith_WeatherTransitionDocument
/// Ownership: Plain value inside a Zenith_WeatherDocument.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: None.
/// Units: Seconds (the loader converts to ticks with the world's tick rate).
/// Errors: None after the reader.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WeatherTransitionDocument
{
	uint32 m_uFromState = 0;                                 // indices into m_axStates
	uint32 m_uToState = 0;
	uint32 m_uWeight = 1;                                    // 1 … 1,000
	float m_fDurationSeconds = 1.0f;                          // 1 … 3,600
};

/// @contract value Zenith_WeatherDocument
/// Ownership: Plain value returned by Zenith_ReadWeatherDocument; spans borrow the reader's arena.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena.
/// Units: Seconds.
/// Errors: None after the reader, which applies the rules of WaterWeather.md "Weather document".
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WeatherDocument
{
	uint32 m_uSchemaVersion = 0;
	Zenith_PersistentID m_xID;
	uint32 m_uInitialState = 0;                              // index into m_axStates
	Zenith_Span<const Zenith_WeatherStateDocument> m_axStates;           // document order, 1 … 32
	Zenith_Span<const Zenith_WeatherTransitionDocument> m_axTransitions; // document order, 0 … 1,024
};

/// @contract value Zenith_WaterDocument
/// Ownership: Plain value returned by Zenith_ReadWaterDocument.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: None.
/// Units: Metres; per-metre coefficients; the wave directions normalised from the document's degrees.
/// Errors: None after the reader, which applies Zenith_ValidateWaveSet and WaterWeather.md "Water document".
/// Version: since Z2 Stage 4 (the FFT block since Z2 Stage 5).
/// @endcontract
struct Zenith_WaterDocument
{
	uint32 m_uSchemaVersion = 0;
	Zenith_PersistentID m_xID;
	Zenith_WaveSet m_xWaves;                                  // m_fAmplitudeScale 1
	bool m_bWindDriven = false;
	Zenith_Vector3 m_xAbsorption, m_xScattering;
	float m_fIndexOfRefraction = 1.333f;
	float m_fFoamShoreDepth = 0.6f, m_fFoamCrestThreshold = 0.55f;
	Zenith_PersistentID m_xFoamTexture, m_xNormalDetailTexture;
	float m_fNormalDetailScale = 0.08f, m_fNormalDetailSpeed = 0.4f;
	float m_fUnderwaterFogDensityScale = 1.0f;
	bool m_bHasFft = false;
	uint32 m_uFftResolution = 256;                           // 128 or 256
	float m_fFftWindSpeed = 0.0f, m_fFftFetch = 0.0f, m_fFftPatchSize = 0.0f;
};

/// @contract value Zenith_TerrainLayerDocument
/// Ownership: Plain value returned by Zenith_ReadTerrainLayerDocument.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena (the physical-material name).
/// Units: Repeats per metre; the layer height map's [0, 1] units.
/// Errors: None after the reader.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainLayerDocument
{
	uint32 m_uSchemaVersion = 0;
	Zenith_PersistentID m_xID;
	Zenith_PersistentID m_xAlbedo, m_xNormal, m_xORMH;        // COLOUR, NORMAL, MASK textures
	float m_fUVScale = 0.25f;
	float m_fHeightBlendDepth = 0.2f;
	Zenith_StringView m_strPhysicalMaterial;
	Zenith_NavigationArea m_eNavigationArea = NAVIGATION_AREA_GROUND;
	float m_fPorosity = 0.5f;
};

/// @contract value Zenith_CloudNoiseDocument
/// Ownership: Plain value returned by Zenith_ReadCloudNoiseDocument.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: None.
/// Units: Texels.
/// Errors: None after the reader.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CloudNoiseDocument
{
	uint32 m_uSchemaVersion = 0;
	Zenith_PersistentID m_xID;
	bool m_bDetail = false;                                   // "volume": "detail"
	uint32 m_uSize = 128;
	uint64 m_ulSeed = 0;
};
```

The weather document keeps seconds, because the tick rate is a property of the running process (`sim.tickHz`), which the weather kind loader applies when it builds the `Zenith_WeatherTable` of a `Zenith_WeatherAsset` and its `Zenith_WeatherTransitionDesc` values ("Runtime asset objects"); the foliage type's and the layer's physical-material names are resolved against PHYS's table by the cook.

### Environment document types

The nine types are registered with SCENE's framework (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`, the Q43 ruling) from tool-side code, following ASSET's `.meta` precedent (`Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"`): ZenithCook's composition root and ZenithEditor call `Zenith_RegisterEnvironmentDocumentTypes` before `Freeze`. <!-- elaboration --> The function, the type enumeration and the canonical schemas live in the static tool library `ZenithAuthoring` (ruling Q84 (a), A3 cross-area row ED40): root header `Zenith2/Tools/Authoring/Zenith_AuthoringEnvironment.h`, bodies under `Zenith2/Tools/Authoring/Internal/Environment/`, migration fixtures under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Environment/`, so the editor links them without ZenithCook's internals. The two callers register them beside the types of `Zenith2/Docs/Design/ASSET/Migration.md → "Registered document types"` (row EN25). ZenithEnvironment never names `ZenithDocument`: the module adds no edge and registers no migration in its entry points, and its readers and `Zenith_TerrainDocument::Load` read current-version bytes, which the tool obtained by opening the document through SCENE's `Zenith_DocumentMigrator` (which runs the chain in memory, keeps unknown members and journals every write). ZenithCook links `ZenithDocument` already, so the registration needs no new engine edge.

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringEnvironment.h (ENV's root header of ZenithAuthoring; the bodies in Internal/Environment/)
#pragma once
#include "ZenithDocument/Zenith_MigrationRegistry.h"
#include "ZenithDocument/Zenith_CanonicalJson.h"

/// @contract enum Zenith_EnvironmentDocumentType
/// Ownership: Plain enumeration; the row of the desc table below.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; values are stable.
/// @endcontract
enum Zenith_EnvironmentDocumentType : uint8
{
	ENV_DOCUMENT_TYPE_TERRAIN = 0, ENV_DOCUMENT_TYPE_TERRAIN_TILE, ENV_DOCUMENT_TYPE_TERRAIN_LAYER, ENV_DOCUMENT_TYPE_FOLIAGE_TYPE,
	ENV_DOCUMENT_TYPE_PLACEMENT, ENV_DOCUMENT_TYPE_ATMOSPHERE, ENV_DOCUMENT_TYPE_WEATHER, ENV_DOCUMENT_TYPE_WATER,
	ENV_DOCUMENT_TYPE_CLOUD_NOISE, ENV_DOCUMENT_TYPE_COUNT
};

/// @contract functions Zenith_EnvironmentDocumentTypeFunctions
/// Ownership: Registration writes into the caller's registry; the descs and canonical schemas are static data owned by
///   ZenithCook.
/// Thread affinity: MAIN_THREAD (tool start-up, before Zenith_MigrationRegistry::Freeze); the schema getter ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: The first failing RegisterDocumentType result, unchanged (DOCUMENT_ERROR_DUPLICATE_REGISTRATION,
///   DOCUMENT_ERROR_INVALID_DESC, DOCUMENT_ERROR_REGISTRY_FROZEN); a step registration failure is
///   DOCUMENT_ERROR_INVALID_DESC with the Zenith_MigrationError logged; the tool then stops its start-up.
/// Version: since Z2 Stage 3 (rows 0 to 5); rows 6 to 8 join in the Stage 4 diffs that introduce weather, water and cloud
///   noise. Tool-side; linked by ZenithCook and ZenithEditor only.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterEnvironmentDocumentTypes(Zenith_MigrationRegistry& xRegistry);
const Zenith_CanonicalSchema& Zenith_GetEnvironmentCanonicalSchema(Zenith_EnvironmentDocumentType eType);
inline constexpr uint32 uENV_DOCUMENT_SCHEMA_VERSION = 1;   // the current version of every ENV type
```

The function registers one `Zenith_DocumentTypeDesc` per row, in row order, then one `Zenith_RegisterMigrationStep` per version increment (none at version 1; each increment adds a step and a fixture pair, "Migration and re-cook" in the entry). Every desc has `m_szSchemaTag` null (ENV documents carry no `$schema` member; the suffix names the type), `m_szVersionKey` `"schemaVersion"`, `m_uCurrentVersion` `uENV_DOCUMENT_SCHEMA_VERSION`, `m_pxCanonical` the row's schema, `m_pfnVisitFieldSets` null (no ENV document embeds a separately versioned object) and `m_pUserData` null. The stable names end in `Document` because `Zenith.Environment.Terrain` and `Zenith.Environment.FoliageCell` are component stable names, and a document type's ID is `Zenith_MakeTypeID` of its stable name.

| Row | `m_szStableName` | `m_aszSuffixes` | `m_szIDKey` |
|---|---|---|---|
| 0 | `Zenith.Environment.TerrainDocument` | `.terrain.json` | `id` |
| 1 | `Zenith.Environment.TerrainTileDocument` | `.terraintile.json` | null: the document ID is `Zenith_MakePathDocumentID` of its path (a tile names its terrain in `terrain`) |
| 2 | `Zenith.Environment.TerrainLayerDocument` | `.terrainlayer.json` | `id` |
| 3 | `Zenith.Environment.FoliageTypeDocument` | `.foliage.json` | `id` |
| 4 | `Zenith.Environment.PlacementDocument` | `.placement.json` | `id` |
| 5 | `Zenith.Environment.AtmosphereDocument` | `.atmosphere.json` | `id` |
| 6 | `Zenith.Environment.WeatherDocument` | `.weather.json` | `id` |
| 7 | `Zenith.Environment.WaterDocument` | `.water.json` | `id` |
| 8 | `Zenith.Environment.CloudNoiseDocument` | `.cloudnoise.json` | `id` |

Each canonical schema (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_CanonicalJson header"`, rules of "Canonical rules") follows its type's JSON block; R marks a required member:

| Type | Root object, members in canonical order | Nested rules |
|---|---|---|
| terrain | `schemaVersion` R, `id` R, `name`, `origin`, `tiles`, `heightLimits`, `lodDistanceRatio`, `placementSeed`, `layers`, `physicalMaterials`, `foliageTypes`, `placementRuleSets`, `exclusionSources`, `tileFiles`, `collision`, `navigation` | `tiles` object (`x`, `z`); `layers` ordered array of objects (`id` R, `document` R, `uvScale`, `heightBlendDepth`, `physicalMaterial`, `navigationArea`); `tileFiles` ordered array of objects (`x` R, `z` R, `file` R), which `Save` keeps sorted by (z, x); `collision` object (`enabled`); `navigation` object (`enabled`, `maxWalkableSlopeDegrees`) |
| tile | `schemaVersion` R, `terrain` R, `x` R, `z` R, `nextExplicitId`, `heights`, `layerWeights`, `holes`, `density`, `exclusion`, `explicitInstances` | `heights` the payload object (`format`, `width`, `height`, `data`); `layerWeights` ordered array of objects (`layer`, `format`, `width`, `height`, `data`) in palette order; `density` the same with `foliageType` first, ascending; `exclusion` the same with `ruleSet` first, `"all"` first and then the terrain's rule-set order; `explicitInstances` ordered array of objects (`id`, `foliageType`, `position`, `yaw`, `scale`, `keepHeight`), ascending `id` |
| terrain layer | `schemaVersion` R, `id` R, `albedo`, `normal`, `ormh`, `uvScale`, `heightBlendDepth`, `physicalMaterial`, `navigationArea`, `porosity` | none |
| foliage type | `schemaVersion` R, `id` R, `name`, `class`, `mesh`, `materialOverrides`, `footprintRadius`, `scaleRange`, `alignToNormal`, `randomYaw`, `castShadows`, `cullDistance`, `collision`, `gameplayVisible`, `renderOnly`, `wind`, `detail` | `wind` object (`stiffness`, `trunkBend`, `branchFlutter`, `leafFlutter`, `phaseVariation`) |
| placement | `schemaVersion` R, `id` R, `rules` | `rules` ordered array of objects (`name`, `foliageType`, `densityPerSquareMetre`, `minDistance`, `slopeDegrees`, `heightRange`, `layers`, `useDensityMap`, `noise`, `avoidWater`, `excludeEarlierRules`, `exclusionRadius`, `scaleRange`, `alignToNormal`); a rule's `layers` ordered array of objects (`layer`, `minWeight`) |
| atmosphere | `schemaVersion` R, `id` R, `planet`, `rayleigh`, `mie`, `ozone`, `heightFog`, `sun`, `moon`, `stars`, `timeOfDay`, `clouds` | each an object with the members of `Zenith2/Docs/Design/ENV/Atmosphere.md → "Atmosphere document"` in that block's order |
| weather | `schemaVersion` R, `id` R, `initialState`, `states`, `transitions` | `states` ordered array of objects (`name`, `dwellSeconds`, `params`, `effects`); `params` object in the block's order (`cloudCoverage` … `lightningPerMinute`); `effects` object (`prefabs`); `transitions` ordered array of objects (`from`, `to`, `weight`, `durationSeconds`) |
| water | `schemaVersion` R, `id` R, `waves`, `windDriven`, `absorption`, `scattering`, `indexOfRefraction`, `foam`, `normalDetail`, `underwaterFogDensityScale`, `fft` | `waves` ordered array of objects (`wavelength`, `amplitude`, `directionDegrees`, `steepness`, `phase`); `foam` object (`shoreDepth`, `crestThreshold`, `texture`); `normalDetail` object (`texture`, `scale`, `speed`) |
| cloud noise | `schemaVersion` R, `id` R, `volume`, `size`, `seed` | none |

Every array of scalars (vectors, colours, ranges, GUID and path lists, `position`) is `CANONICAL_NODE_INLINE_ARRAY` (C7); a member the document allows to be `null` or an object (`holes`, a foliage type's `collision` and `detail`, `noise`, `fft`, a layer's `uvScale`) is `CANONICAL_NODE_VALUE`, which writes either form verbatim; every other scalar is a value under C9 (`placementSeed` and `seed` are strings), the base64 `data` payloads included. Arrays of objects are C8 ordered arrays, whose order is data; where the order is not authored, the sorts above define it, so two saves of one document are byte-identical.

## Cook pipeline

### Step graph

ASSET runs one ASSET-phase step per kind (`Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStepDesc"`) and cooks each product from the intermediates its source's importer wrote (`Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"`). ENV's terrain cook is therefore the importer `env.terrain` of `*.terrain.json`, whose stages run in the order below inside one import and write intermediates, plus ENV's steps for ENV's kinds; a terrain's textures, generated materials and collision shapes are ordinary products that their owners' steps cook (design ruling E15 in the entry).

| Stage or step | Runs in | Inputs | Outputs |
|---|---|---|---|
| `Environment.TerrainValidate` | importer `env.terrain` | the terrain document (`ReadSource`); every tile, layer, foliage type and placement document and every exclusion source (`ReadInput`) | the validated document ("Validation stage") |
| `Environment.TerrainProducts` | importer | the validated document | `DeclareProduct` for every product, in key order: the primary `Flux.Terrain`; per grid cell `texture:tile/<x>_<z>/height` and, for a cell whose hole span is non-empty ("Geometry hand-off to GEOM"), `texture:tile/<x>_<z>/holes`; per present tile `texture:tile/<x>_<z>/splatIndex`, `…/splatWeight`, `terrainTile:<x>_<z>`, `foliageCell:<x>_<z>` and, from Stage 4, `collision:<x>_<z>` and `navigation:<x>_<z>`; per present tile painting a detail type `texture:tile/<x>_<z>/detailDensity`; `material:terrain` and, from Stage 4 and only for a terrain whose foliage-type list holds at least one detail type, `material:detail` (`Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`); ENV's sections 0x0201–0x0206, which name those GUIDs, written as the terrain product's intermediate `envSections` |
| `Environment.TerrainGeometry` | importer | the validated document; the height and hole product GUIDs | "Geometry hand-off to GEOM": the intermediate `geomSections` of the terrain product and the source images of the height and hole products |
| `Environment.TerrainSplat` | importer, one pass per present tile | the document | the splat index and weight source images and the packed detail-density image ("Splat textures") |
| `Environment.TerrainMaterial` | importer | the document, the layer documents, the detail foliage types | the generated material documents, written as GEOM's `geom.materialDocument` blobs (`Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`), the intermediates of `material:terrain` (`Material_TerrainLayered`) and, when declared, `material:detail` (`Material_DetailFoliage`, which needs the vertex-modifier hook of X127 (1)) (`Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module"`) |
| `Environment.TerrainTiles` | importer, one pass per present tile | the document, the cooked range | the intermediate of `terrainTile:<x>_<z>`: the three sections of "Terrain tile file" |
| `Environment.Foliage` | importer, one pass per present tile | the document, foliage types, placement rule sets, the exclusion sources | the intermediate of `foliageCell:<x>_<z>`: the cell's sorted records ("Foliage cook") |
| `Environment.Collision`, `Environment.Navigation` (Stage 4) | importer, one pass per present tile | the document, the foliage cells, the navigation water bodies ("Water bodies for navigation") | the intermediates of `collision:<x>_<z>` (PHYS's heightfield input) and of `navigation:<x>_<z>`: `ai.navSource` (the navigation bake's input) and `environment.navWater` ("Navigation geometry extraction") |
| `env.terrain` | ASSET-phase step of `Flux.Terrain` | `geomSections` and the ENV section intermediate; from Stage 4 `envCellContent` | the `.zterrain` ("Terrain file"); from Stage 4 the published properties `environment.cellContent.<x>_<z>` ("Cell content records") |
| `env.terrainTile`, `env.foliageCell` | ASSET-phase steps of kinds 23 and 24 | the product's intermediate | the `.zenvtile`, the `.zfoliage` |
| `geom.texture`, `geom.material` | GEOM's steps | the source images; the generated documents | the `.ztex` and `.zmat` of each product |
| PHYS's and AI's steps | theirs | the collision and navigation intermediates | the `.zcol`; the navigation bake's input |
| `env.atmosphere`, `env.weather`, `env.water`, `env.cloudNoise` | importer and ASSET-phase step of kinds 25, 26, 27 and 34 | the document | the `.zatmos`, `.zweather`, `.zwater` (and each water document's `material:water` product through `geom.material`), `.zcloudnoise` ("Parameter document cook") |

- Every stage and step is a pure function of its declared inputs; the keys are those of `Zenith2/Docs/Design/ENV.md → "Cook steps and derived-data keys"`, so an unchanged product re-cooks from the DDC even when its terrain re-imports.
- The `Foliage` stage of a tile reads the surface of the tiles within the placement run's largest ring ("Cross-cell determinism" in Procedural.md), which the import holds whole.
- A stage failure fails the import with a cook error naming the terrain and, where it has one, the tile; nothing of the terrain is published (ASSET's atomic output publication).

### Cook determinism

Every ENV output is byte-identical for identical inputs, independent of the cook's worker count and step order: each step's output depends only on its inputs; placement is worker-invariant ("Determinism argument" in Procedural.md); sorting uses total orders; floating-point work runs in the fixed environment; and compression is deterministic per codec version (Z2-D-004). `Test_EnvironmentCook_ByteIdenticalAcrossWorkers` cooks the fixture terrain with 1 and 8 workers twice each and compares every output byte.

ENV adds no cook error code (ASSET's `Zenith_CookErrorCode` is closed to areas). An ENV importer stage that fails reports one ERROR diagnostic through `Zenith_ImportContext::Report` whose message begins with the `Zenith_EnvironmentErrorCode`'s name (`Zenith_ToString`) and names the document, tile or cell, and returns `IMPORT_ERROR_SOURCE_INVALID` (`IMPORT_ERROR_OUT_OF_MEMORY` for `ENV_ERROR_OUT_OF_MEMORY`), which the cook reports as `COOK_ERROR_IMPORT_FAILED` with that import error as detail (`Zenith2/Docs/Design/ASSET/Import.md`); an ENV cook step that fails returns `COOK_ERROR_STEP_FAILED` with the `Zenith_EnvironmentErrorCode` value in `m_uDetail` (`COOK_ERROR_OUT_OF_MEMORY` for memory) and reports the same diagnostic through `Zenith_CookContext::Report`. The cases: a mismatched tile border `ENV_ERROR_TILE_BORDER_MISMATCH`; a layer texture of the wrong usage and an inconsistent foliage-type combination `ENV_ERROR_DOCUMENT_SCHEMA`; a placement cell over capacity `ENV_ERROR_PLACEMENT_CAPACITY`; a misaligned STREAM cell size or origin `ENV_ERROR_INVALID_DESC`; a malformed intermediate `ENV_ERROR_COOKED_DATA`; every other document error its own code.

## Terrain cook

### Validation stage

The importer's `Environment.TerrainValidate` stage runs `Zenith_TerrainDocument::Load` with `bRepairBorders = false` (a mismatched border fails the import with `ENV_ERROR_TILE_BORDER_MISMATCH`, "Cook determinism"), plus: every layer document resolves; every foliage-type and placement GUID resolves; every physical-material name resolves against PHYS's table (pending PHYS); the tight height range is computed.

### Geometry hand-off to GEOM

GEOM's terrain geometry cook is a function, `Zenith_CookTerrainGeometry` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`), which the importer's `Environment.TerrainGeometry` stage calls once per terrain; `uGEOM_TERRAIN_COOK_VERSION` enters the importer's version (`Zenith2/Docs/Design/ENV.md → "Cook steps and derived-data keys"`). The stage builds `Zenith_TerrainGeometryCookInput` from the validated document:

| Member | Value |
|---|---|
| `m_xOriginWS` | the document's origin |
| `m_fExtentX`, `m_fExtentZ` | 128 m × the tile count per axis (GEOM's extent rule holds by construction) |
| `m_uTilesX`, `m_uTilesZ` | the document's grid |
| `m_fSampleSpacing` | 0.5 |
| `m_fHeightMin`, `m_fHeightMax` | the tight range over every sample of every present tile, the maximum raised so the range is at least 0.01 m |
| `m_fLODDistanceRatio` | the document's `lodDistanceRatio` |
| `m_axTiles` | exactly one `Zenith_TerrainGeometryCookTile` per grid cell, sorted by (z, x): the 257 × 257 float heights of a present tile or the synthesised heights of an absent cell (below); its hole span (below) or an empty span; `m_xHeightTexture` and `m_xHoleTexture` the GUIDs of the products `texture:tile/<x>_<z>/height` and `…/holes` that `Environment.TerrainProducts` declared, the latter null exactly when the hole span is empty |

**Absent cells.** GEOM's grid is dense (its validation rule 3), so a cell the terrain document does not list is passed as a tile whose every sample takes the value of the same global sample in a present tile containing it (every such copy is equal by `Zenith2/Docs/Design/ENV/Terrain.md → "Height clamping and border consistency"`), or `m_fHeightMin` where no present tile contains it, and whose every quad counts as a hole quad in the conversion below. Its hole samples are therefore all 0, the shared border samples of every border it has are 0 on both sides, GEOM's rule 5 holds, and GEOM's nodes draw nothing over it. The cost is one height tile and one hole tile per absent cell (132,098 and 66,049 bytes before compression).

**Hole conversion.** ENV's authored mask is per quad (255 = hole quad, 0 = solid); GEOM's is per sample (0 = hole, 255 = solid). Global sample (gs, gt) is 0 when any of the up to four global quads sharing it, `(gs − 1 … gs, gt − 1 … gt)` clipped to the grid, is an authored hole quad or lies in an absent cell; otherwise it is 255. Each tile's 257 × 257 span is read from this global mask, so a shared border sample has one value in both tiles. A tile whose span holds no 0 is passed with an empty span. Within `D_0` of the LOD reference (128 m at r = 8 and GEOM's 16 m leaf), where level-0 nodes draw, GEOM's nearest-sample discard removes every authored hole quad whole plus at most half a sample (0.25 m) around it, so there the rendered hole is never smaller than the collision hole, which uses the exact quads ("Holes and materials in collision"), and a player never falls through visible ground. Coarser levels read mip `min(l, 8)` of the point-decimated chain and show holes on their own grid (GEOM's rule, `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"`): beyond `D_0` a single-quad hole may vanish and a present tile's edge beside an absent cell may lose up to half a coarse texel. `Test_EnvironmentCook_HoleMaskConversion` asserts the single-quad, tile-edge and absent-neighbour cases.

**Outputs.** The stage writes the function's four sections as the terrain product's intermediate `geomSections`, which `env.terrain` copies into the `.zterrain` unchanged; and each tile's 257 × 257 quantised heights (`m_auHeightSamples`) and, for a tile with a hole span, its 257 × 257 hole bytes (`m_auHoleSamples`) as the source images of its `height` and `holes` products, which GEOM's texture step cooks with the classes `FLUX_TEXTURE_USAGE_HEIGHT` and `FLUX_TEXTURE_USAGE_HOLE_MASK` and their point-decimated chains (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"`; each written as GEOM's `geom.textureSource` blob with `Zenith_WriteTextureSource` (usage `HEIGHT`, layout `R16`, and `HOLE_MASK`, layout `R8`, both 257 × 257, linear, clamp), `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`). The quantisation is GEOM's formula, identical to "Height representation", so the `.zenvtile` heights equal GEOM's texels (`Test_EnvironmentCook_HeightsMatchGeomTiles`). A function error (`COOK_ERROR_TERRAIN_EXTENT`, `COOK_ERROR_TERRAIN_TILE`) fails the import naming the terrain and tile.

### Splat textures

Per present tile: "Cooked top-four reduction" (`Zenith2/Docs/Design/ENV/Terrain.md`) over the authored weights, written as two 256 × 256 `R8G8B8A8_UNORM` source images of the products `texture:tile/<x>_<z>/splatIndex` and `…/splatWeight`, which GEOM's texture step cooks with the classes `FLUX_TEXTURE_USAGE_SPLAT_INDEX` (point-decimated chain) and `FLUX_TEXTURE_USAGE_SPLAT_WEIGHT` (integer-mean chain), both resident whole (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`). A tile painting at least one of the terrain's detail types also gets the 128 × 128 `R8G8B8A8_UNORM` image of `texture:tile/<x>_<z>/detailDensity`, channel k the density map of detail type k (0 where the tile has no map), cooked with the class `FLUX_TEXTURE_USAGE_SPLAT_WEIGHT`, whose four raw `uint8` channels, integer-mean chain and whole-chain residency fit it (GEOM admits the 128 × 128 image, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`); the stage writes all three images as `geom.textureSource` blobs with `Zenith_WriteTextureSource` (usage `SPLAT_INDEX` or `SPLAT_WEIGHT`, layout `R8G8B8A8`, linear, clamp).

### Tile CPU data

Per tile, the `.zenvtile` ("Terrain tile file"): the heights quantised with the terrain's tight range (identical values to GEOM's R16 tile, from the same formula), the per-quad physical-material index, and the hole bits.

## Foliage cook

The importer's `Environment.Foliage` stage, per present tile:

1. Run one `Zenith_Place` over all the terrain's rule sets, in the terrain's list order (`m_axRuleSets`), for the tile's cell ("Runtime and editor use" in Procedural.md), with the terrain's layer IDs as `m_axLayerPalette`, the exclusion sources' shapes as `m_axExclusions` and the explicit instances of the tile and its eight neighbours as obstacles; each instance's key is `Zenith_PlacementInstanceKey`, unique within the cell across every rule set (`Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity"`).
2. Snap each explicit instance's height to the surface (unless `keepHeight`) and give it `Zenith_ExplicitInstanceKey(id)`.
3. For every instance, build the rotation (yaw about +Y, then leaned toward the surface normal by `alignToNormal`), quantise it with GEOM's kernel, and draw nothing new (the thinning rank comes from placement draw 6; explicit instances take the rank `Zenith_Hash64U64(id, seed) >> 48`).
4. Sort by (type index, instance key) and write the cell ("Foliage cell file").
5. Emit the collidable subset to "Collidable foliage".

## Parameter document cook

The atmosphere, weather, water and cloud-noise documents are read by their importers, validated (their tables' limits), converted to SI units and runtime layouts, and written field by field, little-endian, into `.zatmos`, `.zweather`, `.zwater` and `.zcloudnoise` ("Atmosphere file", "Weather file", "Water file", "Cloud noise file"); the weather cook resolves state names to their hashes and checks every transition target, and the water cook runs `Zenith_ValidateWaveSet`.

## Cooked formats

Every ENV cooked file uses ASSET's cooked container: the common header (magic, format version, engine cook version `uZENITH_ENGINE_COOK_VERSION`, section count, content hash, asset GUID, platform) and the section table of 40-byte entries with COMPRESS's 24-byte descriptor per section (`Zenith2/Docs/Design/ASSET.md → "Format index"`, the header layout GEOM also uses, `Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`). Section payloads are little-endian, written field by field, never by copying a C++ object (Z2-D-038).

ENV's kinds are rows of ASSET's closed kind table (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, answered in ASSET's row AS9): 4 `Flux.Terrain` `.zterrain` `ZTRN` (owner ENV, geometry sections GEOM); 23 `Environment.TerrainTile` `.zenvtile` `ZETL` (SIMULATION, FAIL, cacheable); 24 `Environment.FoliageCell` `.zfoliage` `ZFOL` (STREAMING, NULL_OBJECT, cacheable); 25 `Environment.Atmosphere` `.zatmos` `ZATM` (ASSETS, SUBSTITUTE with LIGHT's defaults, cacheable); 26 `Environment.Weather` `.zweather` `ZWTR` (ASSETS, FAIL, cacheable); 27 `Environment.Water` `.zwater` `ZWAT` (ASSETS, FAIL, cacheable); and 34 `Environment.CloudNoise` `.zcloudnoise` `ZCLN` (RENDERING, NULL_OBJECT, cacheable, GPU resident), which ENV adds by ASSET's growth rule (`Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise volumes"`, row EN22). The generated materials are ordinary `Flux.Material` products.

### Section kind registry

Section kinds are unique per file kind; GEOM's kinds occupy 0x0100–0x01FF of the terrain file, ENV's 0x0200–0x02FF. The four GEOM rows are laid out by `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"` and read through GEOM's `Flux_ReadTerrainGeometrySections`; ENV copies the function's sections unchanged, and where this summary disagrees GEOM's text wins.

| File | Kind | Owner | Content |
|---|---|---|---|
| `.zterrain` | 0x0101 | GEOM | terrain geometry header (56 bytes): the scalars of `Flux_TerrainDesc`, the LOD distance ratio among them |
| `.zterrain` | 0x0102 | GEOM | node min/max pyramid, f32 pairs, level-major |
| `.zterrain` | 0x0103 | GEOM | height-tile table: `u32` tile count, then one 16-byte texture GUID per grid cell, row-major; no entry is null (an absent cell names its synthesised tile) |
| `.zterrain` | 0x0104 | GEOM | hole-tile table, as 0x0103: per grid cell the GUID of its 257 × 257 hole `.ztex`, or 16 zero bytes for a tile with an empty hole span |
| `.zterrain` | 0x0201 | ENV | terrain info ("Terrain info section") |
| `.zterrain` | 0x0202 | ENV | layer table ("Layer table section") |
| `.zterrain` | 0x0203 | ENV | splat tile table: per grid cell the index and weight texture GUIDs (32 bytes; zero for an absent cell) |
| `.zterrain` | 0x0204 | ENV | tile asset table: per grid cell the `.zenvtile` GUID, zero for an absent cell |
| `.zterrain` | 0x0205 | ENV | detail foliage table (Stage 4): `u32` detail type count (0 … 4), the 32-byte records of `Flux_TerrainDetailType` ("Runtime asset objects"), then per grid cell the packed detail-density texture GUID (16 bytes, zero where the tile paints no detail type) |
| `.zterrain` | 0x0206 | ENV | materials: the GUIDs of `material:terrain` and `material:detail` (32 bytes; the second zero when `material:detail` is not declared) |
| `.zenvtile` | 0x0211 | ENV | heights: 257 × 257 `uint16` |
| `.zenvtile` | 0x0212 | ENV | physical material per quad: 256 × 256 `uint8` |
| `.zenvtile` | 0x0213 | ENV | hole bits: 256 × 256 bits, row-major, LSB first (8,192 bytes) |
| `.zfoliage` | 0x0301 | ENV | cell header ("Foliage cell file") |
| `.zfoliage` | 0x0302 | ENV | type table |
| `.zfoliage` | 0x0303 | ENV | instance records, 32 bytes each |
| `.zfoliage` | 0x0304 | ENV | query index of gameplay-visible and collidable instances (their record indices, ascending) |
| `.zatmos` | 0x0401 | ENV | atmosphere record |
| `.zweather` | 0x0501 | ENV | states ("Weather file") |
| `.zweather` | 0x0502 | ENV | transitions |
| `.zwater` | 0x0601 | ENV | water record ("Water file") |
| `.zwater` | 0x0602 | ENV | FFT block (Stage 5) |
| `.zcloudnoise` | 0x0701 | ENV | volume info ("Cloud noise file") |
| `.zcloudnoise` | 0x0702 | ENV | texels, the whole chain |

### Terrain file

`.zterrain` (`ZTRN`, format version 1): the GEOM sections plus the ENV sections above. The LOD distance ratio lives in GEOM's section 0x0101 (the scalars of `Flux_TerrainDesc`), not in ENV's info section; the kind loader reads it from there. Loaded by the terrain kind loader, which ASSET's table places in Flux; ENV's GPU side in ZenithFlux (`Engine/ZenithFlux/Internal/Environment/`) implements it and builds `Flux_TerrainDesc` from the GEOM sections through GEOM's `Flux_ReadTerrainGeometrySections` and ENV's material records from the ENV sections, and exposes the decoded ENV sections through the accessors of `Flux_TerrainResource` ("Runtime asset objects"), of which the terrain runtime reads `GetInfo` and `GetTileAssetGUIDs`. The terrain asset's cooked data never holds per-sample heights; those stream with the tiles.

### Terrain info section

| Offset | Type | Field |
|---|---|---|
| 0 | 3 × f32 | origin |
| 12 | u16, u16 | tiles X, Z |
| 16 | f32, f32 | tight height range min, max |
| 24 | u64 | placement seed |
| 32 | u32 | layer count |
| 36 | u32 | foliage type count |
| 40 | u32 | physical material count |
| 44 | u32 | flags: bit 0 collision enabled, bit 1 navigation enabled |
| 48 | u32 × count | the PHYS physical-material IDs, in the document's `physicalMaterials` order |

### Layer table section

The section is `layerCount` records of 40 bytes (the layer ID, 16 bytes; `uint16` physical-material index; `uint16` navigation area; `f32` UV scale; `f32` height-blend depth; `f32` porosity; 8 reserved bytes) followed by `layerCount × 3` texture GUIDs of 16 bytes each, in (albedo, normal, ORMH) order per layer.

### Terrain tile file

`.zenvtile` (`ZETL`, format version 1): the header's asset GUID is the tile's sub-asset GUID; sections 0x0211–0x0213; the reader checks the three sizes exactly (132,098, 65,536 and 8,192 bytes decoded).

### Foliage cell file

`.zfoliage` (`ZFOL`, format version 1):

Section 0x0301, the header (48 bytes):

| Offset | Type | Field |
|---|---|---|
| 0 | i32, i32 | cell X, Z (placement-cell coordinates) |
| 8 | 6 × f32 | bounds minimum then maximum (x, y, z) |
| 32 | u32 | type count (1 … 64) |
| 36 | u32 | instance count (0 … 1,048,576) |
| 40 | u32 | query-index count |
| 44 | u32 | reserved, 0 |

Section 0x0302, the type table: `type count` records of `48 + 16 × overrides` bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | 16 bytes | foliage type GUID |
| 16 | 16 bytes | mesh GUID |
| 32 | u32 | flags: bit 0 render-only, bit 1 collidable, bit 2 gameplay-visible, bit 3 casts shadows |
| 36 | f32 | cull distance, metres |
| 40 | u32 | material-override count (0 … 16) |
| 44 | u32 | reserved, 0 |
| 48 | 16 bytes × count | the override GUIDs (zero for `null`) |

Section 0x0303, the instances: `instance count` records of 32 bytes, the fields of `Zenith_FoliageInstance` (`Zenith2/Docs/Design/ENV/Foliage.md → "Instance record layout"`) in declaration order: offset 0 position (3 × f32), 12 rotation (4 × i16), 20 scale (f32), 24 type index (u16), 26 thinning rank (u16), 28 instance key (u32).

Section 0x0304, the query index: `query-index count` ascending `u32` record indices.

### Atmosphere file

`.zatmos` (`ZATM`, format version 1): one section 0x0401 of 240 bytes, SI units:

| Offset | Type | Field |
|---|---|---|
| 0 | f32, f32 | ground radius, atmosphere height |
| 8 | 3 × f32 | ground albedo |
| 20 | 3 × f32 | Rayleigh scattering per metre |
| 32 | f32 | Rayleigh scale height |
| 36 | 4 × f32 | Mie scattering, Mie absorption, Mie scale height, Mie anisotropy |
| 52 | 3 × f32 | ozone absorption |
| 64 | 2 × f32 | ozone centre, ozone half width |
| 72 | 5 × f32 | fog density, albedo, falloff, base height, anisotropy |
| 92 | 3 × f32 | fog emission |
| 104 | 2 × f32 | sun illuminance, sun angular radius |
| 112 | 3 × f32 | sun colour |
| 124 | 3 × f32 | moon illuminance, moon angular radius, star intensity |
| 136 | f32, u32, f32, u32 | latitude (radians), day of year, north offset (radians), day length (real seconds) |
| 152 | f64 | start hours |
| 160 | 3 × f32, u32 | cloud layer altitude, thickness, noise scale; reserved 0 |
| 176 | 4 × 16 bytes | cloud base noise, cloud detail noise, star cube and moon texture GUIDs |

### Weather file

`.zweather` (`ZWTR`, format version 1). Section 0x0501 begins with `u32` initial state index and `u32` state count (1 … 32), then one 208-byte record per state in document order:

| Offset | Type | Field |
|---|---|---|
| 0 | u64 | name hash (`Zenith_Hash64` of the name) |
| 8 | 2 × f32 | dwell minimum and maximum, seconds |
| 16 | 7 × f32 | cloud coverage, cloud density, wind speed, wind direction (radians), gust strength, gust frequency, turbulence |
| 44 | u32 | precipitation kind (`Zenith_PrecipitationKind`) |
| 48 | 6 × f32 | precipitation rate, wetness target, snow target, fog density scale, haze scale, lightning per minute |
| 72 | u32, u32 | effect prefab count (0 … 8), reserved 0 |
| 80 | 8 × 16 bytes | effect prefab GUIDs (zero beyond the count) |

Section 0x0502 begins with `u32` transition count (0 … 1,024), then 12-byte records sorted by (from index, document order): `u16` from, `u16` to, `u32` weight, `f32` duration seconds.

### Water file

`.zwater` (`ZWAT`, format version 1). Section 0x0601, 296 bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | u32, u32 | wave count (0 … 8); flags, bit 0 wind-driven |
| 8 | 8 × 6 × f32 | waves: wavelength, amplitude, direction x and z (normalised at cook), steepness, phase; unused waves zero |
| 200 | 3 × f32, 3 × f32 | absorption, scattering, per metre |
| 224 | 6 × f32 | index of refraction, foam shore depth, foam crest threshold, normal-detail scale, normal-detail speed, underwater fog scale |
| 248 | 3 × 16 bytes | foam texture, normal-detail texture and generated `material:water` sub-asset GUIDs |

Section 0x0602 (Stage 5, present only with an `fft` block), 32 bytes: `u32` spectrum (1 JONSWAP), `f32` wind speed, `f32` fetch, `u32` resolution (128 or 256), `f32` patch size, `f32` the fitted Gerstner set's height error the cook measured, 8 reserved bytes.

### Cloud noise file

`.zcloudnoise` (`ZCLN`, format version 1), the file of `Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise volumes"`. Section 0x0701, 16 bytes: `u32` extent N (32, 64, 128 or 256), `u32` mip count (log2 N + 1), `u32` the `Zenith_RHIFormat` value of `R8G8B8A8_UNORM`, `u32` flags (bit 0 the detail layout; the other bits zero). Section 0x0702: every mip from 0, each mip's (N >> l) slices along z, each slice's rows along y, 4 bytes per texel in R, G, B, A order; its decoded size is exactly `4 × Σ (N >> l)³` (9,586,980 bytes for N = 128, 149,796 for N = 32).

### Reader validation

Every ENV reader validates, before using any byte: the container through ASSET (magic, versions, section table bounds, descriptor decoding); each section's decoded size against its layout; every count against its maximum (layers 16, foliage types 64, instances 1,048,576 per cell, states 32, transitions 1,024, waves 8; a cloud-noise extent, mip count and texel size against its info record); every float finite and every value within its document's validation range; every index within its table. A violation fails the load with the asset error of ASSET's contract naming the section index (ENV's detail code in the error's detail field, "Error enumerations" in the entry), never an assert ([Z2-N-ARCH-951]); each reader has a fuzz target (`Zenith2/Docs/Design/ENV/Tests.md → "Fuzz targets"`).

## Runtime asset objects

Each ENV kind of ASSET's table (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, rows 4, 23 to 27 and 34) loads into the runtime object that row names, built by the kind loader (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"`) from the sections after "Reader validation"; the component and record structs name it through `Zenith_AssetHandle<T>`, and `Zenith_EnvironmentComponents.h` forward-declares it. Every object is immutable after load and readable from any thread while a handle is held; the loader decodes each section once, field by field (Z2-D-038), into the object's own arrays, and every accessor returns those arrays in place (no per-query copy).

| Kind | Runtime object | Header | Loader |
|---|---|---|---|
| 4 `Flux.Terrain` | `Flux_TerrainResource` | `Zenith2/Engine/ZenithFlux/Flux_Environment.h` | `Flux_TerrainKindLoader` (ENV's GPU side) |
| 23 `Environment.TerrainTile` | `Zenith_TerrainTileAsset` | `Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentAssets.h` | `Internal/Assets/` |
| 24 `Environment.FoliageCell` | `Zenith_FoliageCellAsset` | the same | the same |
| 25 `Environment.Atmosphere` | `Zenith_AtmosphereAsset` | the same | the same |
| 26 `Environment.Weather` | `Zenith_WeatherAsset` | the same | the same; it converts seconds to ticks with `sim.tickHz`, which the module reads at Start (every world of the process ticks at it) |
| 27 `Environment.Water` | `Zenith_WaterAsset` | the same | the same |
| 34 `Environment.CloudNoise` | `Flux_CloudNoiseResource` | `Zenith2/Engine/ZenithFlux/Flux_Environment.h` | `Flux_CloudNoiseKindLoader` |

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Environment.h (continued): the runtime objects of kinds 4 and 34
/// @contract value Flux_TerrainInfo
/// Ownership: Plain value inside a Flux_TerrainResource; the span points into the resource.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while a handle to the resource is held.
/// Units: World metres.
/// Errors: None; validated at load.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_TerrainInfo                              // section 0x0201
{
	Zenith_Vector3 m_xOriginWS;
	uint16 m_uTilesX = 0;
	uint16 m_uTilesZ = 0;
	float m_fHeightMin = 0.0f;                       // the tight cooked range: the tiles' quantisation range
	float m_fHeightMax = 0.0f;
	uint64 m_ulPlacementSeed = 0;
	uint32 m_uLayerCount = 0;
	uint32 m_uFoliageTypeCount = 0;
	uint32 m_uFlags = 0;                            // bit 0 collision enabled, bit 1 navigation enabled
	Zenith_Span<const uint32> m_auPhysicalMaterials;   // PHYS's IDs in the document's physicalMaterials order
};

/// @contract value Flux_TerrainLayerRecord
/// Ownership: Plain value; one per palette layer of a Flux_TerrainResource.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while a handle to the resource is held.
/// Units: Repeats per metre; metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_TerrainLayerRecord                       // one 40-byte record of section 0x0202 with its three texture GUIDs
{
	Zenith_PersistentID m_xLayerID;
	uint16 m_uPhysicalMaterialIndex = 0;            // into Flux_TerrainInfo::m_auPhysicalMaterials
	uint16 m_uNavigationArea = 0;                   // a Zenith_NavigationArea value
	float m_fUVScale = 0.25f;
	float m_fHeightBlendDepth = 0.2f;
	float m_fPorosity = 0.5f;
	Zenith_AssetGUID m_axTextures[3];                // albedo, normal, ORMH
};

/// @contract value Flux_TerrainSplatTile
/// Ownership: Plain value; one per grid cell of a Flux_TerrainResource.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while a handle to the resource is held.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_TerrainSplatTile                         // one 32-byte entry of section 0x0203
{
	Zenith_AssetGUID m_xIndex;                       // null for an absent cell
	Zenith_AssetGUID m_xWeight;
};

/// @contract value Flux_TerrainDetailType
/// Ownership: Plain value; one per detail type of a Flux_TerrainResource (at most 4).
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while a handle to the resource is held.
/// Units: Metres; per square metre.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_TerrainDetailType                        // one 32-byte record of section 0x0205, fields in this order
{
	uint32 m_uFoliageTypeIndex = 0;                 // into the terrain's foliageTypes list
	float m_fDensityPerSquareMetre = 1.0f;
	float m_fBladeHeightMin = 0.2f;
	float m_fBladeHeightMax = 0.5f;
	float m_fBladeWidth = 0.02f;
	float m_fBendMax = 0.3f;
	float m_fCardSize = 0.5f;
	uint32 m_uReserved = 0;
};

/// @contract type Flux_TerrainResource
/// Ownership: The runtime object of kind 4, created by Flux_TerrainKindLoader and owned by the asset registry through its
///   handles; owns the decoded ENV sections and the arrays the Flux_TerrainDesc spans name (tile texture IDs, node pyramid).
/// Thread affinity: IMMUTABLE after load; read by the terrain runtime (phase 1), the extractors and ENV's GPU side.
/// Invalidation: Every reference and span is valid while a handle is held.
/// Units: World metres.
/// Errors: None after load; the loader fails a corrupt file with ASSET's asset error naming the section.
/// Version: since Z2 Stage 3 (the detail members since Z2 Stage 4).
/// @endcontract
class Flux_TerrainResource
{
public:
	const Flux_TerrainDesc& GetDesc() const;                         // GEOM's sections; m_xMaterial = material:terrain
	const Flux_TerrainInfo& GetInfo() const;                         // 0x0201
	Zenith_Vector2 GetCookedRange() const;                           // (min, max) of 0x0201: the tiles' quantisation range
	Zenith_Span<const Flux_TerrainLayerRecord> GetLayers() const;    // 0x0202, palette order
	Zenith_Span<const Flux_TerrainSplatTile> GetSplatTiles() const;  // 0x0203, one per grid cell, row-major
	Zenith_Span<const Zenith_AssetGUID> GetTileAssetGUIDs() const;   // 0x0204, one per grid cell, row-major; null for an absent cell
	Zenith_Span<const Flux_TerrainDetailType> GetDetailTypes() const;      // 0x0205 records, 0 … 4
	Zenith_Span<const Zenith_AssetGUID> GetDetailDensityGUIDs() const;     // 0x0205, one per grid cell; null where none is painted
	Zenith_AssetGUID GetTerrainMaterialGUID() const;                 // 0x0206: material:terrain
	Zenith_AssetGUID GetDetailMaterialGUID() const;                  // 0x0206: material:detail, null when not declared
};

/// @contract type Flux_CloudNoiseResource
/// Ownership: The runtime object of kind 34, created by Flux_CloudNoiseKindLoader and owned by the asset registry through
///   its handles; owns one 3D R8G8B8A8_UNORM Flux_PersistentReadOnlyTexture with the file's whole chain.
/// Thread affinity: IMMUTABLE after load; GetSRV is read on RENDER_SUBMIT_THREAD by ENV's cloud passes.
/// Invalidation: Valid while a handle is held; the descriptor is null until the queued upload completes.
/// Units: Texels.
/// Errors: None after load (a headless world uses ASSET's validate-only loader and never creates one).
/// Version: since Z2 Stage 4.
/// @endcontract
class Flux_CloudNoiseResource
{
public:
	Zenith_RHIDescriptorHandle GetSRV() const;   // the 3D texture's shader-resource descriptor
	uint32 GetExtent() const;                   // N of section 0x0701
	uint32 GetMipCount() const;                 // log2 N + 1
	bool IsDetail() const;                       // 0x0701 flags bit 0
};
```

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentAssets.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithAssets/Zenith_AssetTypes.h"                // Zenith_AssetGUID
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"
#include "ZenithEnvironment/Zenith_FoliageSystem.h"         // Zenith_FoliageInstance
#include "ZenithEnvironment/Zenith_AtmosphereParameters.h"
#include "ZenithEnvironment/Zenith_TimeOfDay.h"
#include "ZenithEnvironment/Zenith_WaterSystem.h"           // Zenith_WaveSet
#include "ZenithEnvironment/Zenith_WeatherSystem.h"         // Zenith_WeatherTable

/// @contract type Zenith_TerrainTileAsset
/// Ownership: The runtime object of kind 23, created by ENV's tile kind loader, owned by the asset registry through its
///   handles; owns the three decoded sections.
/// Thread affinity: IMMUTABLE after load; read by the terrain runtime and its queries on any thread.
/// Invalidation: Spans are valid while a handle is held.
/// Units: Quantised height samples (decoded with the terrain's cooked range); indices; bits.
/// Errors: None after load; the reader checks the three sizes exactly ("Terrain tile file").
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_TerrainTileAsset
{
public:
	Zenith_Span<const uint16> GetHeights() const;        // 0x0211: 257 × 257, row-major (t, s)
	Zenith_Span<const uint8> GetQuadMaterials() const;   // 0x0212: 256 × 256 indices into the terrain's physical materials
	Zenith_Span<const uint8> GetHoleBits() const;        // 0x0213: 8,192 bytes, 256 × 256 bits, row-major, LSB first
};

/// @contract value Zenith_FoliageCellType
/// Ownership: Plain value; one per type-table entry of a Zenith_FoliageCellAsset; the span points into the asset.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while a handle to the asset is held.
/// Units: Metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FoliageCellType                       // one record of section 0x0302
{
	Zenith_AssetGUID m_xFoliageType;
	Zenith_AssetGUID m_xMesh;
	uint32 m_uFlags = 0;                           // bit 0 render-only, 1 collidable, 2 gameplay-visible, 3 casts shadows
	float m_fCullDistance = 1000.0f;
	Zenith_Span<const Zenith_AssetGUID> m_axMaterialOverrides;   // 0 … 16; a null GUID keeps the mesh's material
};

/// @contract type Zenith_FoliageCellAsset
/// Ownership: The runtime object of kind 24, created by ENV's foliage-cell kind loader, owned by the asset registry
///   through its handles; owns the decoded type table, instance records and query index.
/// Thread affinity: IMMUTABLE after load; read by Environment.UpdateFoliageCells, the foliage extractor and foliage queries.
/// Invalidation: Spans are valid while a handle is held.
/// Units: World metres (the authoring frame).
/// Errors: None after load.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_FoliageCellAsset
{
public:
	Zenith_EnvCellCoord GetCell() const;                            // 0x0301
	Zenith_Vector3 GetBoundsMin() const;                            // 0x0301
	Zenith_Vector3 GetBoundsMax() const;
	Zenith_Span<const Zenith_FoliageCellType> GetTypes() const;     // 0x0302, type-index order
	Zenith_Span<const Zenith_FoliageInstance> GetInstances() const; // 0x0303, sorted by (type index, instance key)
	Zenith_Span<const uint32> GetQueryIndex() const;               // 0x0304, ascending record indices
};

/// @contract value Zenith_AtmosphereCloudSettings
/// Ownership: Plain value inside a Zenith_AtmosphereAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Metres; per-metre noise scale.
/// Errors: None.
/// Version: since Z2 Stage 3 (consumed from Z2 Stage 4).
/// @endcontract
struct Zenith_AtmosphereCloudSettings               // 0x0401 offsets 160 … 207
{
	float m_fLayerAltitude = 1800.0f;
	float m_fLayerThickness = 900.0f;
	float m_fNoiseScale = 0.00012f;
	Zenith_AssetGUID m_xBaseNoise;                  // .zcloudnoise; null: no clouds
	Zenith_AssetGUID m_xDetailNoise;                // null: base noise only
};

/// @contract type Zenith_AtmosphereAsset
/// Ownership: The runtime object of kind 25, created by ENV's atmosphere kind loader, owned by the asset registry through
///   its handles; on a refused file the world uses LIGHT's defaults (ENV_WARNING_ATMOSPHERE_FALLBACK).
/// Thread affinity: IMMUTABLE after load; read by Environment.Advance.
/// Invalidation: References are valid while a handle is held.
/// Units: SI, as Zenith_AtmosphereParameters.
/// Errors: None after load; the loader applies Atmosphere.md "Validation limits".
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AtmosphereAsset
{
public:
	const Zenith_AtmosphereParameters& GetParameters() const;     // 0x0401 offsets 0 … 135
	const Zenith_TimeOfDaySettings& GetTimeOfDay() const;          // offsets 136 … 159
	const Zenith_AtmosphereCloudSettings& GetClouds() const;       // offsets 160 … 207
	Zenith_AssetGUID GetStarCube() const;                          // offset 208
	Zenith_AssetGUID GetMoonTexture() const;                       // offset 224
};

/// @contract type Zenith_WeatherAsset
/// Ownership: The runtime object of kind 26, created by ENV's weather kind loader, owned by the asset registry through its
///   handles; owns its Zenith_WeatherTable, built with the process's sim.tickHz.
/// Thread affinity: IMMUTABLE after load; read by Environment.Advance.
/// Invalidation: References and spans are valid while a handle is held.
/// Units: Ticks in the table; GUIDs.
/// Errors: None after load ("Reader validation").
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WeatherAsset
{
public:
	const Zenith_WeatherTable& GetTable() const;
	uint32 GetTickHz() const;                                     // the rate the table's ticks were converted with
	Zenith_Span<const Zenith_AssetGUID> GetEffectPrefabs(uint32 uState) const;   // 0 … 8, state order
};

/// @contract value Zenith_WaterOptics
/// Ownership: Plain value inside a Zenith_WaterAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Per-metre coefficients; metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WaterOptics                           // 0x0601 offsets 200 … 279
{
	Zenith_Vector3 m_xAbsorption;
	Zenith_Vector3 m_xScattering;
	float m_fIndexOfRefraction = 1.333f;
	float m_fFoamShoreDepth = 0.6f;
	float m_fFoamCrestThreshold = 0.55f;
	float m_fNormalDetailScale = 0.08f;
	float m_fNormalDetailSpeed = 0.4f;
	float m_fUnderwaterFogDensityScale = 1.0f;
	Zenith_AssetGUID m_xFoamTexture;
	Zenith_AssetGUID m_xNormalDetailTexture;
};

/// @contract type Zenith_WaterAsset
/// Ownership: The runtime object of kind 27, created by ENV's water kind loader, owned by the asset registry through its
///   handles.
/// Thread affinity: IMMUTABLE after load; read by Environment.UpdateWaterBodies, the water queries and the environment
///   extractor.
/// Invalidation: References are valid while a handle is held.
/// Units: Metres; radians.
/// Errors: None after load; the loader runs Zenith_ValidateWaveSet.
/// Version: since Z2 Stage 4 (the FFT block since Z2 Stage 5).
/// @endcontract
class Zenith_WaterAsset
{
public:
	const Zenith_WaveSet& GetWaves() const;                        // 0x0601; m_fAmplitudeScale 1
	bool IsWindDriven() const;                                     // flags bit 0
	const Zenith_WaterOptics& GetOptics() const;
	Zenith_AssetGUID GetMaterialGUID() const;                      // offset 280: material:water
	bool HasFft() const;                                           // section 0x0602 present (Stage 5)
};
```

`Test_EnvironmentAssets_AccessorsMatchSections`, `Test_TerrainKindLoader_BuildsDescAndRecords` and `Test_CloudNoiseKindLoader_ExtentAndChain` (`Zenith2/Docs/Design/ENV/Tests.md → "Module integration and diagnostics tests"`) assert every accessor against the fixture files' sections.

## Collision generation hook

### Heightfield extraction

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentGeneration.h
/// @contract value Zenith_TerrainCollisionTile
/// Ownership: Plain value whose spans borrow the arena passed to Zenith_ExtractTerrainCollision.
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: Invalid after the arena is reset.
/// Units: Metres; heights quantised as the cooked tile.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_TerrainCollisionTile
{
	Zenith_TerrainTileCoord m_xTile;
	Zenith_Vector3 m_xOriginWS;                          // the tile's minimum corner at height 0
	float m_fSampleSpacing = 0.5f;
	float m_fHeightMin = 0.0f;
	float m_fHeightMax = 0.0f;
	Zenith_Span<const uint16> m_auHeights;              // 257 × 257
	Zenith_Span<const uint8> m_auQuadMaterials;         // 256 × 256 physical-material indices
	Zenith_Span<const uint8> m_auHoleBits;              // 8,192 bytes
	uint8 m_uDiagonal = 0;                               // 0: every quad split (s,t)-(s+1,t+1) ("Triangle interpolation")
};

/// @contract enum Zenith_NavigationArea
/// Ownership: Plain enumeration of the area codes of "Area flags".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (terrain layer documents name the values); values are stable (the navigation bake reads them).
/// @endcontract
enum Zenith_NavigationArea : uint8
{
	NAVIGATION_AREA_GROUND = 0,
	NAVIGATION_AREA_ROAD,
	NAVIGATION_AREA_OFF_PATH,
	NAVIGATION_AREA_UNWALKABLE,
	NAVIGATION_AREA_WATER,
};

/// @contract value Zenith_NavigationGeometry
/// Ownership: Plain value whose spans borrow the arena passed to Zenith_ExtractNavigationGeometry.
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: Invalid after the arena is reset.
/// Units: World metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavigationGeometry
{
	Zenith_TerrainTileCoord m_xTile;
	Zenith_Span<const Zenith_Vector3> m_axVertices;     // deduplicated corner vertices, row-major order
	Zenith_Span<const uint32> m_auIndices;             // three per triangle, counter-clockwise seen from +Y
	Zenith_Span<const uint8> m_auAreas;                // one Zenith_NavigationArea per triangle
};

/// @contract value Zenith_WaterBodyShape
/// Ownership: Plain value; m_axPolygon borrows caller storage for the call.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: None beyond the borrowed span.
/// Units: World metres.
/// Errors: Validated by the extraction (a lake polygon that is not simple and counter-clockwise, has more than 256
///   vertices or bounds wider than 4,096 m on an axis, is ENV_ERROR_INVALID_DESC with the shape index).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WaterBodyShape
{
	uint64 m_ulBodyKey = 0;
	Zenith_WaterBodyKind m_eKind = WATER_BODY_KIND_LAKE;
	float m_fBaseHeight = 0.0f;
	float m_fMinX = 1.0f;                               // ocean bounds; min > max: unbounded
	float m_fMinZ = 1.0f;
	float m_fMaxX = 0.0f;
	float m_fMaxZ = 0.0f;
	Zenith_Span<const Zenith_Vector2> m_axPolygon;      // lake: 3 … 256 XZ vertices
};

/// @contract enum Zenith_FoliageCollisionShape
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (foliage type documents name the values); values are stable.
/// @endcontract
enum Zenith_FoliageCollisionShape : uint8 { FOLIAGE_COLLISION_CAPSULE = 1, FOLIAGE_COLLISION_BOX, FOLIAGE_COLLISION_CONVEX };

/// @contract value Zenith_FoliageCollisionDesc
/// Ownership: Plain value; one per collidable type of a cell's type table.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: None.
/// Units: Metres, in the instance's unscaled local frame.
/// Errors: None; the foliage type reader validated it.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_FoliageCollisionDesc
{
	uint16 m_uTypeIndex = 0;                           // the cell's type-table index
	Zenith_FoliageCollisionShape m_eShape = FOLIAGE_COLLISION_CAPSULE;
	float m_fRadius = 0.0f;                             // capsule
	float m_fHeight = 0.0f;                             // capsule, along local +Y from the pivot
	Zenith_Vector3 m_xHalfExtents;                      // box, centred at local (0, half height, 0)
	Zenith_PersistentID m_xConvexHull;                  // convex: the hull asset of the foliage type document
	uint32 m_uPhysicalMaterial = 0;                    // PHYS's physical-material ID
};

/// @contract value Zenith_FoliageCollisionInstance
/// Ownership: Plain value inside a Zenith_FoliageCollisionSet.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: None.
/// Units: World metres; unit quaternion; uniform scale.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_FoliageCollisionInstance
{
	uint32 m_uShapeIndex = 0;                          // into m_axShapes
	uint32 m_uRecordIndex = 0;                         // the instance's index in the cell's 0x0303 records
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	float m_fScale = 1.0f;
};

/// @contract value Zenith_FoliageCollisionSet
/// Ownership: Plain value whose spans borrow the arena passed to Zenith_ExtractFoliageCollision.
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: Invalid after the arena is reset.
/// Units: As its members.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_FoliageCollisionSet
{
	Zenith_EnvCellCoord m_xCell;
	Zenith_Span<const Zenith_FoliageCollisionDesc> m_axShapes;
	Zenith_Span<const Zenith_FoliageCollisionInstance> m_axInstances;   // record order
};

/// @contract functions Zenith_EnvironmentGeneration
/// Ownership: Pure functions; outputs from the caller's arena.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Metres.
/// Errors: ENV_ERROR_TILE_MISSING for an absent tile; ENV_ERROR_OUT_OF_MEMORY when the arena is exhausted.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<Zenith_TerrainCollisionTile, Zenith_EnvironmentError> Zenith_ExtractTerrainCollision(
	const Zenith_TerrainDocument& xDocument, Zenith_TerrainTileCoord xTile, float fHeightMin, float fHeightMax,
	Zenith_Arena& xArena);
Zenith_Result<Zenith_NavigationGeometry, Zenith_EnvironmentError> Zenith_ExtractNavigationGeometry(
	const Zenith_TerrainDocument& xDocument, Zenith_TerrainTileCoord xTile, Zenith_Span<const Zenith_WaterBodyShape> axWater,
	Zenith_Arena& xArena);
Zenith_Result<Zenith_FoliageCollisionSet, Zenith_EnvironmentError> Zenith_ExtractFoliageCollision(
	Zenith_EnvCellCoord xCell, Zenith_Span<const Zenith_FoliageInstance> axInstances,
	Zenith_Span<const Zenith_FoliageCollisionDesc> axTypes, Zenith_Arena& xArena);
// The cooked tile and cell files, written by the same code for the cook steps and for "Regeneration after edits".
Zenith_Result<Zenith_ByteSpan, Zenith_EnvironmentError> Zenith_WriteTerrainTileFile(const Zenith_TerrainDocument& xDocument,
	Zenith_TerrainTileCoord xTile, float fHeightMin, float fHeightMax, Zenith_Arena& xArena);
Zenith_Result<Zenith_ByteSpan, Zenith_EnvironmentError> Zenith_WriteFoliageCellFile(Zenith_EnvCellCoord xCell,
	Zenith_Span<const Zenith_FoliageInstance> axInstances, Zenith_Span<const Zenith_PersistentID> axFoliageTypes, Zenith_Arena& xArena);
```

`Zenith_EnvironmentGeneration.h` includes `Zenith_TerrainDocument.h`, `Zenith_FoliageSystem.h` and `Zenith_ProceduralPlacement.h`, so the header compiles alone.

The importer's `Environment.Collision` stage calls `Zenith_ExtractTerrainCollision` per present tile and writes the result as the intermediate of the tile's `collision:<x>_<z>` product, which PHYS's heightfield cook step cooks into one `Physics.CollisionShape` asset (`.zcol`) per tile; the tile's cell content then holds one static collider entity referencing it ("Cell content records").

<!-- assumption: pending PHYS -->
PHYS provides a heightfield cook input taking `Zenith_TerrainCollisionTile` (u16 samples with a range, per-quad material indices, hole bits, the fixed diagonal) and a static collider component that references a `Physics.CollisionShape` asset, and its heightfield shape splits quads along the same diagonal, so the collision surface equals "Triangle interpolation". Recorded as cross-area note EN4.
<!-- /assumption -->

### Collidable foliage

`Zenith_ExtractFoliageCollision` turns the collidable instances of a cooked cell (type flag bit 1) into a `Zenith_FoliageCollisionSet`: per type its shape (capsule, box or a convex hull GUID from the foliage type document) and the instances' transforms, in record order. PHYS cooks it into one static compound shape per cell (pending PHYS, cross-area note EN4); the cell content holds one static collider entity for it. Tree trunks therefore collide exactly where the rendered instance stands, from the same cooked record.

### Holes and materials in collision

A hole quad produces no triangle; the per-quad physical material selects PHYS's material for the two triangles of that quad; tiles absent from the terrain produce no collider at all.

## Navigation generation hook

### Navigation geometry extraction

`Zenith_ExtractNavigationGeometry` produces, per tile, a triangle list (the LOD-0 triangles of "Triangle interpolation", decimated for the bake by merging co-planar quads within 2 cm into larger quads, deterministic in row order) with a per-triangle area code, minus hole quads, with water bodies' footprints marked. The navigation bake (AI's navmesh generation under WORLD's baking workflow) consumes it; its implementation depends on D-SEL-NAV (open, Stage 1/2), and until the ruling the hook exists as the API-only slice with its tests on the extraction alone.

`Zenith_EnvironmentGeneration.h` declares, beside the extraction, `inline constexpr uint32 uENV_NAVIGATION_GEOMETRY_VERSION = 1;`. It is bumped by any change to `Zenith_ExtractNavigationGeometry`'s output for unchanged inputs (the decimation, the vertex order, the winding, an area rule), and a bump also bumps `uENV_TERRAIN_STAGES_VERSION` in the same diff, so every terrain re-imports ("Cook steps and derived-data keys" in the entry). WORLD's lighting fingerprint records the constant (its item 7, `Zenith2/Docs/Design/WORLD/Bakes.md → "Lighting fingerprint"`, row WD26), because the bake's terrain triangles are this output. `Test_NavigationGeometry_VersionCoversOutput` keeps one row `{ version, digest }` per value of the constant, the digest the `Zenith_Hash64` of the fixture tile (0, 0)'s extracted vertex, index and area bytes, and fails when the current value has no row or the bytes do not match its digest; the row is recorded from the observed run of the diff that sets the value.

The importer stage `Environment.Navigation` (Stage 4) runs per present tile in (z, x) order:

1. `Zenith_ExtractNavigationGeometry(document, tile, W, arena)`, W the terrain's navigation water bodies ("Water bodies for navigation").
2. AI's writer `Zenith_AIWriteNavSourceIntermediate(geometry, adTileOrigin, arena)` (`Zenith2/Tools/Authoring/Zenith_AuthoringAI.h`, `Zenith2/Docs/Design/AI/Formats.md → "Navigation source intermediate"`), `adTileOrigin` the tile's absolute minimum corner in binary64 (`origin.x + 128 × x`, `origin.y`, `origin.z + 128 × z`, the document's `origin` widened before the sum). Its bytes are the intermediate `ai.navSource` of the product `navigation:<x>_<z>` (kind 15 `ASSET_KIND_NAV_MESH`, script name `NavMesh`, non-primary), which `Environment.TerrainProducts` declares for every present tile. A tile has at most 66,049 vertices and 131,072 triangles, inside the writer's limits, so an AI error is a defect: it fails the import with one ERROR diagnostic beginning `ENV_ERROR_COOKED_DATA` and naming the tile and AI's code, and `IMPORT_ERROR_SOURCE_INVALID` ("Cook determinism").
3. `Zenith_WriteNavigationWaterBodies(W, tile, origin, arena)`, whose bytes are the same product's second intermediate `environment.navWater` ("Water bodies for navigation").

AI's step `ai.navMesh` cooks the product as a SOURCE_GEOMETRY item and publishes its `ai.navSource` bytes as the property of that name (`Zenith2/Docs/Design/AI.md → "Cook steps and derived-data keys"`) and its `environment.navWater` bytes as the property of that name (row EN27). AI's coarse occupancy pre-pass (the product declaration's) reads exactly what it needs from `ai.navSource`: the vertex bounds, tile-local, plus the tile origin at offset 16, in binary64 world metres; the vertices span the tile's walkable and unwalkable triangles alike, so those bounds contain every triangle the tile bake gathers. Every present tile gets the product and both intermediates, including a tile whose geometry has no triangle (every quad a hole: V = T = 0 and no bound), so the pre-pass finds a property for every present tile a navigation document's terrains cover; an absent tile has no product.

### Area flags

| Area code | From | Meaning for the bake |
|---|---|---|
| `ground` (0) | layer navigation area | walkable |
| `road` (1) | layer navigation area | walkable, preferred cost |
| `offPath` (2) | layer navigation area | walkable, higher cost |
| `unwalkable` (3) | layer navigation area, or slope above the document's `maxWalkableSlopeDegrees` | not walkable |
| `water` (4) | a water body covers the triangle's centroid | swimmable per AI's agent settings |

### Water bodies for navigation

The terrain's navigation water bodies W are the entities of its `exclusionSources` scenes (read through `ReadInput`, as the exclusions are, "Terrain document schema") that carry `Zenith.Environment.WaterBody`, by stable name, with their entities' world transforms (SCENE's scene document layout). Rivers are Stage 5 and are not navigation water at Stage 4: a RIVER body is skipped. Each other body becomes one `Zenith_WaterBodyShape`:

| Member | Value |
|---|---|
| `m_ulBodyKey` | the first 8 bytes, little-endian, of `Zenith_Hash128Of` over the scene's asset GUID (16 bytes) followed by the entity's persistent ID (16 bytes); two bodies with one key fail the import (`ENV_ERROR_INVALID_DESC` naming both scenes) |
| `m_eKind` | property 1 `Kind` (OCEAN or LAKE) |
| `m_fBaseHeight` | property 2 `BaseHeight` |
| `m_fMinX` … `m_fMaxZ` | an ocean: the entity's world XZ ∓ properties 3 and 4 (`HalfExtentX`, `HalfExtentZ`), summed in binary64 and narrowed last; both 0 leave the bounds unbounded (min > max). A lake: its polygon's bounds |
| `m_axPolygon` | a lake: the points of the `Zenith.Data` asset property 5 `Shape` names (`Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header"`: a `u32` count, then (x, z) `f32` pairs relative to the entity's position), read through `ReadInput` of the path its scene's `Data` entry gives, each point added to the entity's world XZ in binary64 and narrowed last. An ocean: empty |

W is sorted by `m_ulBodyKey`; `Zenith_ExtractNavigationGeometry` validates every shape (its contract's `ENV_ERROR_INVALID_DESC` with the shape index). The importer reads the shape bytes as import inputs, so editing a body, its shape or the scene re-imports the terrain, and the cook remains a pure function of its declared inputs.

The stage writes, per present tile, the bodies of W whose XZ bounds intersect the tile's closed 128 m square (an unbounded ocean intersects every tile), in W's order, as the intermediate `environment.navWater` of `navigation:<x>_<z>`, little-endian:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `ZENW` |
| 4 | 2 | version 1 |
| 6 | 2 | reserved, 0 |
| 8 | 4 | tile x, tile z (two `uint16`) |
| 12 | 4 | body count B (≤ 256, `uENV_NAVIGATION_WATER_MAX_BODIES`) |
| 16 | 40 B | per body: `m_ulBodyKey` (`uint64`); kind (`uint8`, 1 OCEAN or 2 LAKE); 3 reserved bytes; base height, min x, min z, max x, max z (five `f32`); polygon point count P (`uint32`, 0 for an ocean, 3 … 256 for a lake); 4 reserved bytes |
| 16 + 40 B | 8 ΣP | every lake's points in body order, (x, z) `f32` pairs in world metres |

The blob is at most 16 + 256 × 40 + 256 × 256 × 8 = 534,544 bytes. A body appears in every tile its bounds reach with identical bytes, so a consumer merging several tiles' lists keeps one body per key. AI's volume bake uses the list for its water prisms (`Zenith2/Docs/Design/AI/Volumes.md → "Volume bake"`).

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringEnvironment.h (continued; ENV's root header of ZenithAuthoring)
#include "ZenithEnvironment/Zenith_EnvironmentGeneration.h"   // Zenith_WaterBodyShape, Zenith_TerrainTileCoord

inline constexpr uint32 uENV_NAVIGATION_WATER_MAX_BODIES = 256;

/// @contract functions Zenith_EnvironmentNavigationWater
/// Ownership: Pure functions; the writer's bytes and the reader's shapes and polygon spans are allocated from xArena.
/// Thread affinity: ANY_THREAD (the importer stage on a cook worker, a navigation input source on a cook worker or the
///   editor's background operation); each touches only its arguments.
/// Invalidation: Every result is invalid after xArena is reset.
/// Units: World metres (binary32, as Zenith_WaterBodyShape); adTerrainOrigin in binary64 world metres.
/// Errors: Writer: ENV_ERROR_INVALID_DESC for a non-finite value, a kind other than OCEAN or LAKE, a lake polygon outside
///   3 … 256 points, axBodies not sorted by strictly ascending m_ulBodyKey (m_uIndex the body index), or more than
///   uENV_NAVIGATION_WATER_MAX_BODIES bodies intersecting the tile (m_uIndex the count). Reader: ENV_ERROR_COOKED_DATA for a
///   wrong magic or version, a size that is not exactly 16 + 40 B + 8 ΣP, B above the maximum, a kind other than 1 or 2, an
///   ocean with P ≠ 0, a lake with P outside 3 … 256, keys not strictly ascending, or a non-finite value (m_uIndex the
///   body ordinal, m_uDetail 0 for the header). Both: ENV_ERROR_OUT_OF_MEMORY when xArena is exhausted. Nothing partial is
///   returned.
/// Version: since Z2 Stage 4. Tool-side; ZenithAuthoring is outside every Shipping link closure.
/// @endcontract
Zenith_Result<Zenith_ByteSpan, Zenith_EnvironmentError> Zenith_WriteNavigationWaterBodies(
	Zenith_Span<const Zenith_WaterBodyShape> axBodies, Zenith_TerrainTileCoord xTile, const double adTerrainOrigin[3],
	Zenith_Arena& xArena);
// The bodies of one navigation:<x>_<z> product's published environment.navWater property, in the blob's order; an empty
// span for B = 0. Called behind Zenith_AIBakeInputSource::ListWaterBodies by AI's cook step and WORLD's import (row AI3).
Zenith_Result<Zenith_Span<const Zenith_WaterBodyShape>, Zenith_EnvironmentError> Zenith_ListWaterBodiesForNavigation(
	Zenith_ByteSpan xNavigationWater, Zenith_Arena& xArena);
```

The reader is the exact inverse of the writer: `Zenith_ListWaterBodiesForNavigation(Zenith_WriteNavigationWaterBodies(W, t, o))` returns, bit for bit, the bodies of W that intersect tile t, in order.

## Regeneration after edits

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_EnvironmentGeneration.h (continued)
/// @contract enum Zenith_TerrainRegenerateFlag
/// Ownership: Plain enumeration of the bits of Zenith_RegenerateTerrainDerivedData's uFlags and of
///   Zenith_TerrainRegeneratedTile::m_uProduced.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a bit outside the four is ENV_ERROR_INVALID_DESC at the call.
/// Version: since Z2 Stage 3 (tile data and foliage); collision and navigation since Z2 Stage 4; values are stable.
/// @endcontract
enum Zenith_TerrainRegenerateFlag : uint32
{
	TERRAIN_REGENERATE_TILE_DATA = 1u << 0,
	TERRAIN_REGENERATE_COLLISION = 1u << 1,
	TERRAIN_REGENERATE_NAVIGATION = 1u << 2,
	TERRAIN_REGENERATE_FOLIAGE = 1u << 3,
};

/// @contract value Zenith_TerrainRegenerateInputs
/// Ownership: Plain value of borrowed spans that outlive the call; filled by EDIT from the loaded documents.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None beyond the borrowed spans.
/// Units: Metres.
/// Errors: Validated by Zenith_RegenerateTerrainDerivedData (ENV_ERROR_INVALID_DESC with the member index).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainRegenerateInputs
{
	float m_fCookedHeightMin = 0.0f;                      // the terrain's last cooked range; the tiles quantise with it
	float m_fCookedHeightMax = 0.0f;
	Zenith_Span<const Zenith_PlacementRuleSet> m_axRuleSets;
	Zenith_Span<const Zenith_PlacementFoliageTypeInfo> m_axFoliageTypes;
	Zenith_Span<const Zenith_PlacementExclusion> m_axExclusions;
	Zenith_Span<const Zenith_FoliageCollisionDesc> m_axCollisionTypes;
	Zenith_Span<const Zenith_WaterBodyShape> m_axWater;
};

/// @contract value Zenith_TerrainRegeneratedTile
/// Ownership: Plain value whose spans borrow the arena of the call.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: Invalid after the arena is reset.
/// Units: As its members.
/// Errors: None.
/// Version: since Z2 Stage 3 (tile data and foliage); collision and navigation since Z2 Stage 4.
/// @endcontract
struct Zenith_TerrainRegeneratedTile
{
	Zenith_TerrainTileCoord m_xTile;
	uint32 m_uProduced = 0;                              // Zenith_TerrainRegenerateFlag bits filled in
	Zenith_ByteSpan m_xTileFile;                          // a complete .zenvtile
	Zenith_TerrainCollisionTile m_xCollision;
	Zenith_NavigationGeometry m_xNavigation;
	Zenith_ByteSpan m_xFoliageCellFile;                   // a complete .zfoliage
	Zenith_FoliageCollisionSet m_xFoliageCollision;
};

/// @contract functions Zenith_TerrainRegeneration
/// Ownership: Pure function of its inputs; outputs from the caller's arena.
/// Thread affinity: OWNER_THREAD (the editor's main thread); fans out through pxJobs and joins before returning.
/// Invalidation: None.
/// Units: Metres.
/// Errors: ENV_ERROR_INVALID_DESC; the first failing tile's error from the extraction and placement functions; nothing
///   partial is returned.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_Span<const Zenith_TerrainRegeneratedTile>, Zenith_EnvironmentError> Zenith_RegenerateTerrainDerivedData(
	const Zenith_TerrainDocument& xDocument, Zenith_Span<const Zenith_TerrainDirtyRegion> axRegions, uint32 uFlags,
	const Zenith_TerrainRegenerateInputs& xInputs, Zenith_JobSystem* pxJobs, Zenith_Arena& xArena);
```

When a stroke or edit ends in the editor, EDIT calls `Zenith_RegenerateTerrainDerivedData` with the stroke's regions. For the dirty tiles it writes the tile file, the collision tile and the navigation geometry with the same functions the cook steps call (`Zenith_WriteTerrainTileFile`, `Zenith_ExtractTerrainCollision`, `Zenith_ExtractNavigationGeometry`); when a height, density, exclusion or explicit-instance channel changed it re-places the dirty tiles' cells dilated by the placement run's largest ring `max(K(g) + ρ(g))` (at most 9 rings, `Zenith2/Docs/Design/ENV/Procedural.md → "Cross-cell determinism"`), because an exclusion chain can carry a change that far, and writes their cell files with `Zenith_WriteFoliageCellFile`. EDIT hands the files to ASSET's in-memory asset replacement and PHYS's and AI's cooks (pending ASSET, PHYS, AI; rows EN3, EN4), and the edit world reloads them at a tick boundary. A full cook produces the same bytes as this incremental path (`Test_EnvironmentCook_IncrementalEqualsClean`).

## Streaming cell content

### Environment cells and STREAM cells

An environment cell is one terrain tile (128 m). STREAM's cells that hold ENV's tiles and foliage cells, those of every `gameplay` grid of the world's streaming world document (`Zenith.Streaming.WorldDocument`, `Zenith2/Docs/Design/STREAM/Formats.md → "Streaming world document"`, its grid objects' `cellSize` and `origin`), must be whole multiples of 128 m aligned to the terrain origin, so each such STREAM cell contains a whole number of environment cells; the terrain importer's `Environment.TerrainValidate` stage reads the document as an import input and refuses (`COOK_ERROR_IMPORT_FAILED` with an `ENV_ERROR_INVALID_DESC` diagnostic naming the grid, "Cook determinism"; ENV adds no cook error code, so STREAM's `COOK_ERROR_TERRAIN_CELL_ALIGNMENT` of row ST11 is this diagnostic) a world whose `gameplay` grid violates this (cross-area note EN7). Grids of the other roles are not checked: colliders and navigation tiles are placed into them by position.

### Cell content records

The ENV content of a STREAM cell is ordinary scene content (entities with components), written into the cell's scene by WORLD's partition cook from ENV's per-environment-cell contribution. ENV declares no cell-content writer: the terrain cook publishes the contribution and WORLD places it by position (ruling WORLD-WDB9-WDB6). The step `env.terrain` publishes, for each present tile (x, z), the property `environment.cellContent.<x>_<z>` (`Zenith_CookContext::PublishProperty`; the importer, which knows each cell's placement and collision results, writes the lists into the terrain product's intermediate `envCellContent`, which the step publishes unchanged, one property per present tile; read by WORLD with `Zenith_ImportContext::ReadPublishedProperty` on the terrain's primary product for `ASSET_PLATFORM_WINDOWS`). `<x>` and `<z>` are the tile coordinates in decimal without leading zeros, as in every ENV product key, so environment cell (x, z) is the 128 m square whose minimum corner is `origin + 128 × (x, 0, z)`. An absent tile publishes nothing. The bytes are a JSON array, written with `Zenith_JsonWriter` in compact form, of exactly five elements, each a GUID string (the 36-character lowercase form) or `null`, in this fixed order:

| Element | Product it names | `null` when | Entity WORLD writes (grid role) | Components and property IDs |
|---|---|---|---|---|
| 0 tile | `terrainTile:<x>_<z>` (kind 23 `Environment.TerrainTile`) | never | terrain tile (`gameplay`) | `Zenith.Environment.TerrainTile` (`Zenith_TerrainTileComponent`, type version 1): 1 `TerrainDocument` (the terrain document's `id`, a `Zenith_PersistentID`, not the asset GUID), 2 `TileX` (x, `uint16`), 3 `TileZ` (z, `uint16`), 4 `Tile` (element 0's GUID) |
| 1 foliage cell | `foliageCell:<x>_<z>` (kind 24 `Environment.FoliageCell`) | the cell's placement produced no instance (the empty `.zfoliage` still cooks) | foliage cell (`gameplay`) | `Zenith.Environment.FoliageCell` (`Zenith_FoliageCellComponent`, type version 1): 1 `TerrainDocument` (as above), 2 `CellX` (x, `sint32`), 3 `CellZ` (z, `sint32`), 4 `Cell` (element 1's GUID) |
| 2 terrain collider | `collision:<x>_<z>` (kind 16, Stage 4) | the terrain declared no such product | terrain collider (`collision`) | PHYS's static collider component referencing the `.zcol` (PHYS's stable name and property IDs, row EN4) |
| 3 foliage collider | the cell's compound collision product (Stage 4, "Collidable foliage") | the cell has no collidable instance | foliage collider (`collision`) | as element 2 |
| 4 navigation | `navigation:<x>_<z>` (kind 15, Stage 4) | the terrain declared no such product | none ("Navigation coverage" below) | none |

The placement cell grid is aligned with the tile grid from the terrain's origin (`Zenith2/Docs/Design/ENV/Procedural.md`), so a cell's (CellX, CellZ) equal its tile's (x, z). The stable names and property IDs are stable data: renaming or renumbering one is a save migration step (`Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header"`) and changes what WORLD's contributor writes. ENV's runtime reads the tile and the cell from these properties, never from the entity's transform, so the position WORLD gives the two `gameplay` entities (the environment cell's minimum corner) changes nothing ENV computes; where a collider entity stands, and the collider component's own properties (its property 3 `Shape` among them), are PHYS's.

**Navigation coverage.** Element 4 names a SOURCE_GEOMETRY product: AI's runtime tile add procedure refuses that role (`ai.nav.wrong_role`, `Zenith2/Docs/Design/AI/Formats.md`), so no component may reference it and WORLD places no entity for it. It is a bake input only: AI's declaration pre-pass and tile bakes read its `ai.navSource` property, and volume bakes its `environment.navWater` property ("Navigation geometry extraction", "Water bodies for navigation"). The navmesh tiles that cover a NAVIGATION cell are only AI's derived `navTile:` products, which WORLD writes into the cell with `Zenith_AIWriteCellNavigationContent`. ENV contributes no runtime navigation entity, so ENV's navigation element and AI's derived tiles never both cover one navigation cell.

The terrain root entity (`Zenith_TerrainComponent`), the water bodies of large extent and the environment settings live in the always-resident layer (WORLD's layers), not in cells.

### Activation and cancellation

Cell activation is STREAM's (asset loads, entity spawn at a sync point, cancellation during I/O, decompression, GPU upload and world activation, [Z2-N-CONTRACT-338]). ENV reacts only to its components appearing and disappearing ("Zenith_TerrainRuntime header" in Terrain.md, "Activation and deactivation" in Foliage.md) and releases every handle it took when they disappear, so a cancelled or unloaded cell leaves no ENV reference behind; `Test_EnvironmentCells_LoadUnload1000Cycles_NoLeak` (in `Tests/Environment/`) runs 1,000 randomised load, cancel and unload cycles and asserts no live ENV asset handle, render instance or allocation remains.

### Large coordinates

ENV stores world positions as `float` (foliage records, water bodies, terrain origins). A binary32 value resolves 1 mm up to ±8,192 m (its spacing at 8,192 m is 0.98 mm) and 2 mm up to ±16,384 m (1.95 mm at 16,384 m): W-Outdoor's 4 km² lies wholly within the first range, and a terrain of the largest size (32,768 m per axis, centred on the world origin) reaches the second, where ENV accepts 2 mm for cooked foliage positions and body and terrain origins (terrain heights are quantised per terrain and unaffected; `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`). Worlds larger than that use STREAM's large-coordinate handling (Z2-I-STREAM-05, pending STREAM, row EN7). Cooked data keeps its authoring-frame positions and is never rewritten; STREAM's current rebase offset O (a `double` vector, pending STREAM) is applied at the two boundaries: every render record ENV writes subtracts O from its positions in `double` and narrows last (foliage translations, `Zenith2/Docs/Design/ENV/Foliage.md → "Instance record layout"`; terrain and water origins; detail cells), and every CPU query of ENV's world resources (terrain height, water, foliage, placement at runtime) takes rebased inputs and adds O before reading cooked data. A rebase re-extracts ENV's render records as a full-state extraction.
