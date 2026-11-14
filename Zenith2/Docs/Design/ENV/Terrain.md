# ENV design: terrain authoring and runtime

Section file of `Zenith2/Docs/Design/ENV.md`. It specifies the terrain authoring model (the terrain document, its tiles, layers, holes, foliage density and exclusion masks), every brush as a deterministic operation on the document, splat blending, edit deltas and the undo hooks EDIT calls, the live sculpting preview, the runtime side (`Zenith_TerrainRuntime`, which feeds GEOM's CDLOD renderer and answers height queries), and the layered terrain material. The geometry of terrain (CDLOD node selection, morphing, the height-tile textures, the min/max pyramid) is GEOM's (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain geometry"`); this file never restates it. Obligations are traced by `Z2-R-ENV-NNN` requirements of `Zenith2/Docs/Requirements/ENV.md` and `Zenith2/Docs/Requirements/ENV/Stage4Plus.md`.

## Scope of this file

| Concern | Heading |
|---|---|
| the tile grid, height representation, layers, holes, density and exclusion masks, explicit instances | "Terrain model" |
| the document and tile API | "Zenith_TerrainDocument header" |
| every brush, its falloff, its order and its parallel application | "Brush operations" |
| authored splat weights, the cooked top-four reduction, the blend in the material | "Splat layers and blending" |
| reversible edits and EDIT's undo | "Terrain edit deltas and undo" |
| sculpt and paint preview in the edit world | "Live preview" |
| the runtime terrain object and its world resource | "Zenith_TerrainRuntime header" |
| CPU height, normal and physical-material queries | "Height queries" |
| `Material_TerrainLayered` | "Terrain material module" |

## Terrain model

### Coordinates and tile grid

<!-- elaboration -->
A terrain is a rectangular grid of square tiles in the XZ plane (world +Y up, `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`). The tile size, the sample spacing and the sample count are fixed engine constants chosen to equal GEOM's height tile (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"`: 257 × 257 texels per 128 m at 0.5 m), so an authored tile maps to exactly one cooked height tile with no resampling.

| Constant | Value | Meaning |
|---|---|---|
| `fENV_TERRAIN_TILE_SIZE` | 128.0 m | edge of a tile |
| `fENV_TERRAIN_SAMPLE_SPACING` | 0.5 m | distance between height samples |
| `uENV_TERRAIN_TILE_SAMPLES` | 257 | height samples per tile edge (256 quads plus the shared border) |
| `uENV_TERRAIN_TILE_QUADS` | 256 | quads per tile edge; also the splat, hole and exclusion texel count per edge |
| `uENV_TERRAIN_DENSITY_TEXELS` | 128 | foliage density texels per tile edge (1 m per texel) |
| `uENV_TERRAIN_MAX_TILES_PER_AXIS` | 256 | a terrain covers at most 32,768 m per axis |
| `uENV_TERRAIN_MAX_LAYERS` | 16 | material layers per terrain |
| `uENV_TERRAIN_MAX_FOLIAGE_TYPES` | 64 | foliage types a terrain's density maps may name |
| `uENV_TERRAIN_MAX_EXPLICIT_INSTANCES_PER_TILE` | 16,384 | hand-placed foliage instances per tile |
| `uENV_TERRAIN_MAX_PLACEMENT_RULESETS` | 16 | placement rule sets a terrain references |

- Tile `(i, j)`, `0 ≤ i < tilesX`, `0 ≤ j < tilesZ`, covers `[originX + 128 i, originX + 128 (i + 1)] × [originZ + 128 j, originZ + 128 (j + 1)]`; the document's origin is its minimum XZ corner.
- Height sample `(s, t)`, `0 ≤ s, t ≤ 256`, of tile `(i, j)` lies at world `(originX + 128 i + 0.5 s, originZ + 128 j + 0.5 t)`; samples are stored row-major with `t` the row and `s` the column.
- Sample 256 of a row of tile `(i, j)` is the same world point as sample 0 of tile `(i + 1, j)`, and likewise along Z; these shared border samples are stored in both tiles and are always equal ("Height clamping and border consistency").
- Quad texel `(u, v)`, `0 ≤ u, v < 256`, of a splat, hole or exclusion map covers the quad whose lower corner is sample `(u, v)`; density texel `(a, b)`, `0 ≤ a, b < 128`, covers the 1 m square `[a, a + 1) × [b, b + 1)` in tile-local metres.
- A terrain's tiles need not fill its rectangle: a tile absent from the document is absent from the world (no geometry, no collision, no navigation), which is how irregular coastlines and whole-tile holes are authored. GEOM's terrain grid is dense, so the cook passes each absent cell to GEOM as a synthesised tile whose every quad is a hole (`Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"`), and GEOM draws nothing there.

### Height representation

| Stage | Type | Range | Rule |
|---|---|---|---|
| authoring (document and edits) | `float` metres, IEEE binary32 | the document's height limits `[m_fMinHeight, m_fMaxHeight]`, default `[−256, 1792]` m, at most 8,192 m wide | every brush result is clamped to the limits; non-finite values never enter a tile |
| cooked (GEOM's R16 height tile and ENV's tile CPU data) | `uint16` | the terrain's tight range `[hMin, hMax]` over every authored sample, widened to at least 0.01 m | `v = floor((h − hMin) / (hMax − hMin) × 65535 + 0.5)` evaluated in `double`, clamped to `[0, 65535]`; decoded `h = hMin + v / 65535 × (hMax − hMin)` in `float`, the formula of `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"` |

<!-- informative -->
Authoring heights are 32-bit floats so that repeated brush dabs never re-quantise: a smooth stroke of a thousand dabs on 16-bit storage would round a thousand times and terrace a gentle slope. The cost is 264,196 bytes per tile before compression, twice the 16-bit size. Zenith quantised its baked terrain per terrain with a per-axis box for the same precision reason (`Zenith/Flux/Terrain/CLAUDE.md`, prior art only).
<!-- /informative -->

### Layers

A terrain owns an ordered palette of at most 16 material layers. Each layer references a terrain layer document (`*.terrainlayer.json`, "Terrain material module") plus per-terrain overrides, and carries a stable `Zenith_PersistentID` layer ID. A tile stores a weight map only for layers painted on it (sparse), one `uint8` per quad texel. The layer at palette index 0 is the base layer: a texel with no painted weight is 255 of the base layer. The weights of one texel sum to 255 exactly ("Weight normalisation").

### Holes

A tile has an optional hole mask of 256 × 256 `uint8` texels, 0 (solid) or 255 (hole); every other value is invalid at load. A hole quad has no rendered surface (GEOM's `bPERM_TERRAIN_HOLES` variant discards it), no collision triangle and no navigation geometry.

### Foliage density and exclusion masks

- A density map is 128 × 128 `uint8` texels per (tile, foliage type), sparse: an absent map reads 0 for rules that use painted density and is not consulted by rules that do not ("Rule evaluation" in `Zenith2/Docs/Design/ENV/Procedural.md`).
- An exclusion mask is 256 × 256 `uint8` quad texels per (tile, rule set or `uENV_EXCLUSION_ALL`); a candidate of a rule of set s on a texel where the larger of set s's mask and the ALL mask is ≥ 128 is rejected before any other filter (`Zenith2/Docs/Design/ENV/Procedural.md → "Surface filters"`). It is how a designer erases procedural foliage from a path without editing rules. In memory a mask is keyed by the rule set's index in `GetPlacementRuleSets()`; in the tile document by the rule set's GUID (or `"all"`), so reordering the terrain's rule-set list never re-targets a mask; `SetPlacementRuleSets` re-keys the in-memory masks and drops (reversibly, in its edit) the masks of removed sets.

### Explicit foliage instances

A tile may carry hand-placed foliage instances (the scatter brush, "Brush kinds", and EDIT's single-placement tool): foliage type index into the terrain's foliage-type list, tile-local position in metres (the height is re-snapped to the surface at cook unless `m_bKeepHeight`), yaw in radians, uniform scale, and a stable instance ID unique within the tile, below 2^31 (`m_uNextExplicitID` of the tile, never reused; the cooked key is `Zenith_ExplicitInstanceKey(id)`, `Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity"`).

## Zenith_TerrainDocument header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_TerrainDocument.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"

class Zenith_Heap;
class Zenith_JobSystem;
class Zenith_Compression;
struct Zenith_TerrainStroke;
class Zenith_TerrainEdit;

/// @contract interface Zenith_TerrainTileSource
/// Ownership: Implemented by the caller (the editor's and the cook's document layer over the source tree, tests over
///   memory) and borrowed for the duration of Zenith_TerrainDocument::Load.
/// Thread affinity: OWNER_THREAD. Called on the thread that calls Load.
/// Invalidation: A span returned by ReadTile is valid until the next ReadTile call or the source's destruction.
/// Units: Bytes of one *.terraintile.json document.
/// Errors: ReadTile returns ENV_ERROR_TILE_MISSING when the tile file does not exist and ENV_ERROR_TILE_READ on an I/O
///   failure; neither asserts.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_TerrainTileSource
{
public:
	virtual ~Zenith_TerrainTileSource() = default;
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_EnvironmentError> ReadTile(Zenith_StringView strRelativePath) = 0;
};

/// @contract interface Zenith_TerrainDocumentSink
/// Ownership: Implemented by the editor's journaled document writer (EDIT over the migration framework) and by tests;
///   borrowed for one Save call.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Bytes of complete JSON documents.
/// Errors: WriteDocument returns ENV_ERROR_WRITE_FAILED when the sink cannot stage the bytes; the document is unchanged
///   and stays dirty.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_TerrainDocumentSink
{
public:
	virtual ~Zenith_TerrainDocumentSink() = default;
	// Every call of one Save belongs to one journaled batch ([Z2-N-CONTRACT-540]); the sink commits on EndBatch.
	virtual Zenith_Result<void, Zenith_EnvironmentError> BeginBatch() = 0;
	virtual Zenith_Result<void, Zenith_EnvironmentError> WriteDocument(Zenith_StringView strRelativePath,
		Zenith_ByteSpan xBytes) = 0;
	virtual Zenith_Result<void, Zenith_EnvironmentError> EndBatch() = 0;
};

/// @contract value Zenith_TerrainLayerDesc
/// Ownership: Plain value held by the document's layer palette.
/// Thread affinity: IMMUTABLE once read from a document. Members non-const: the document reader default-constructs it
///   and fills it field by field.
/// Invalidation: None.
/// Units: m_fUVScale in texture repeats per metre; m_fHeightBlendDepth in the layer height map's [0, 1] units.
/// Errors: Validated by AddLayer and Load (ENV_ERROR_DOCUMENT_SCHEMA with the layer index).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainLayerDesc
{
	Zenith_PersistentID m_xLayerID;            // stable; weight maps and edits key on it, never on the palette index
	Zenith_PersistentID m_xLayerDocument;      // the *.terrainlayer.json asset GUID
	float m_fUVScale = 0.25f;                  // used when m_bOverrideUVScale, else the layer document's scale
	bool m_bOverrideUVScale = false;
	float m_fHeightBlendDepth = 0.2f;          // "Height-based blend"
	uint16 m_uPhysicalMaterial = 0;           // index into the terrain's physical-material name table
	uint16 m_uNavigationArea = 0;             // navigation area code ("Area flags" in Bake.md)
};

/// @contract value Zenith_TerrainDocumentInfo
/// Ownership: Plain value; a copy of the document's header fields.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainDocumentInfo
{
	const Zenith_PersistentID m_xDocumentID{};
	const Zenith_Vector3 m_xOriginWS{};              // minimum corner; heights are absolute world Y
	const uint16 m_uTilesX = 0;
	const uint16 m_uTilesZ = 0;
	const float m_fMinHeight = -256.0f;
	const float m_fMaxHeight = 1792.0f;
	const float m_fLODDistanceRatio = 8.0f;          // at least 8: GEOM's crack-free argument (Zenith2/Docs/Design/GEOM/Meshes.md "Node selection"); cooked into GEOM's section 0x0101
	const uint64 m_ulPlacementSeed = 0;       // "Seeded streams" in Procedural.md
	const uint32 m_uLayerCount = 0;
	const uint32 m_uFoliageTypeCount = 0;
	const uint32 m_uRuleSetCount = 0;
	const uint32 m_uSchemaVersion = 0;        // the version the document was read at
};

/// @contract type Zenith_TerrainTile
/// Ownership: Owned by its Zenith_TerrainDocument (one per present tile); storage from the document's heap.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Reads are safe from any thread while no edit is being applied;
///   Zenith_TerrainDocument::ApplyStroke and ApplyEdit are the only writers.
/// Invalidation: Pointers and spans are invalid after RemoveTile of this tile, after an ApplyEdit that removes it, and
///   after the document's destruction; a sparse map's span is invalid after any edit that creates or drops that map.
/// Units: Heights in metres; weights, holes, densities and exclusions in uint8 counts (0 … 255).
/// Errors: Accessors return empty spans for absent sparse maps; an out-of-range coordinate is ZENITH_ASSERT.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_TerrainTile
{
public:
	Zenith_TerrainTileCoord GetCoord() const;
	Zenith_Span<const float> GetHeights() const;                         // 257 × 257, row-major (t, s)
	float GetHeight(uint32 uS, uint32 uT) const;
	Zenith_Span<const uint8> GetLayerWeights(const Zenith_PersistentID& xLayerID) const;   // 256 × 256 or empty
	uint32 GetPaintedLayerCount() const;
	Zenith_PersistentID GetPaintedLayerID(uint32 uIndex) const;         // ascending palette index
	Zenith_Span<const uint8> GetHoles() const;                           // 256 × 256 or empty
	Zenith_Span<const uint8> GetDensity(uint32 uFoliageType) const;    // 128 × 128 or empty
	Zenith_Span<const uint8> GetExclusion(uint32 uRuleSetOrAll) const; // 256 × 256 or empty
	Zenith_Span<const Zenith_ExplicitFoliageInstance> GetExplicitInstances() const;   // ascending ID
	uint64 GetRevision() const;                                          // bumped by every edit of this tile
};

/// @contract value Zenith_TerrainDocumentDesc
/// Ownership: Plain value passed to Create.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres.
/// Errors: Create validates it (ENV_ERROR_INVALID_DESC with the field index in m_uIndex).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainDocumentDesc
{
	Zenith_PersistentID m_xDocumentID;          // issued by the caller (Zenith_GeneratePersistentID)
	Zenith_Vector3 m_xOriginWS;
	uint16 m_uTilesX = 1;                      // 1 … 256
	uint16 m_uTilesZ = 1;                      // 1 … 256
	float m_fInitialHeight = 0.0f;              // every sample of every created tile
	float m_fMinHeight = -256.0f;
	float m_fMaxHeight = 1792.0f;               // m_fMaxHeight − m_fMinHeight in [0.01, 8192]
	Zenith_PersistentID m_xBaseLayerDocument;   // palette index 0
	uint64 m_ulPlacementSeed = 0;
	float m_fLODDistanceRatio = 8.0f;           // [8, 64]; below 8 is ENV_ERROR_INVALID_DESC
	bool m_bCreateAllTiles = true;              // false: an empty grid; tiles are added with AddTile
};

/// @contract type Zenith_TerrainDocument
/// Ownership: Created by Create or Load as a Zenith_Owned<Zenith_TerrainDocument>, owned by the editor's document table
///   or by the cook step for one cook; tiles, maps and edit bookkeeping come from the heap passed in.
/// Thread affinity: OWNER_THREAD. ApplyStroke may fan work out through the job system it is given and joins it before
///   returning; no other member is called concurrently with it.
/// Invalidation: A Zenith_TerrainTile pointer is invalidated by RemoveTile of that tile and by destruction; every applied
///   edit advances GetRevision.
/// Units: Metres, radians, uint8 weights.
/// Errors: Zenith_Result<…, Zenith_EnvironmentError>. Corrupt or out-of-limit JSON, a missing tile file, a border
///   mismatch or a newer schema version are recoverable errors naming the tile or field; a stroke with invalid parameters
///   returns ENV_ERROR_BRUSH_INVALID and changes nothing; ApplyEdit on a document whose revision does not match the edit
///   returns ENV_ERROR_EDIT_STALE and changes nothing.
/// Version: since Z2 Stage 3. The documents are authoring documents migrated per Zenith2/Docs/Contracts.md
///   "Compatibility policy per kind"; the in-memory API is source API.
/// @endcontract
class Zenith_TerrainDocument
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_TerrainDocument>, Zenith_EnvironmentError> Create(
		const Zenith_TerrainDocumentDesc& xDesc, Zenith_Heap& xHeap);
	// Reads the *.terrain.json bytes at the current schema version (a tool passes the bytes SCENE's migrator opened, which
	// ran the chain in memory: Bake.md "Environment document types"), then every tile the document lists through xTiles,
	// whose source opens each tile the same way. bRepairBorders: take the smaller-index tile's copy of a mismatched border
	// sample (editor) instead of failing (cook).
	static Zenith_Result<Zenith_Owned<Zenith_TerrainDocument>, Zenith_EnvironmentError> Load(
		Zenith_ByteSpan xTerrainJson, Zenith_TerrainTileSource& xTiles, const Zenith_CompressionCodecSet& xCodecs,
		bool bRepairBorders, Zenith_Heap& xHeap);

	Zenith_TerrainDocumentInfo GetInfo() const;
	uint64 GetRevision() const;                      // +1 per applied stroke or edit
	bool IsReadOnly() const;                           // a newer-version document ([Z2-N-CONTRACT-537])
	Zenith_Span<const Zenith_TerrainLayerDesc> GetLayers() const;       // palette order
	Zenith_Span<const Zenith_PersistentID> GetFoliageTypes() const;     // *.foliage.json GUIDs
	Zenith_Span<const Zenith_PersistentID> GetPlacementRuleSets() const; // *.placement.json GUIDs
	const Zenith_TerrainTile* FindTile(Zenith_TerrainTileCoord xCoord) const;   // null when absent
	uint32 GetTileCount() const;
	const Zenith_TerrainTile* GetTileByIndex(uint32 uIndex) const;    // ascending (z, x)

	// Structural edits. Each produces a reversible Zenith_TerrainEdit the caller hands to EDIT's undo stack.
	Zenith_Result<void, Zenith_EnvironmentError> AddTile(Zenith_TerrainTileCoord xCoord, float fInitialHeight,
		Zenith_TerrainEdit& xOutEdit);                               // borders taken from present neighbours
	Zenith_Result<void, Zenith_EnvironmentError> RemoveTile(Zenith_TerrainTileCoord xCoord, Zenith_TerrainEdit& xOutEdit);
	Zenith_Result<void, Zenith_EnvironmentError> AddLayer(const Zenith_TerrainLayerDesc& xLayer, Zenith_TerrainEdit& xOutEdit);
	Zenith_Result<void, Zenith_EnvironmentError> RemoveLayer(const Zenith_PersistentID& xLayerID, Zenith_TerrainEdit& xOutEdit);   // its weight goes to the other layers
	Zenith_Result<void, Zenith_EnvironmentError> MoveLayer(const Zenith_PersistentID& xLayerID, uint32 uNewIndex, Zenith_TerrainEdit& xOutEdit);   // index 0 stays the base layer
	Zenith_Result<void, Zenith_EnvironmentError> SetFoliageTypes(Zenith_Span<const Zenith_PersistentID> axTypes, Zenith_TerrainEdit& xOutEdit);
	Zenith_Result<void, Zenith_EnvironmentError> SetPlacementRuleSets(Zenith_Span<const Zenith_PersistentID> axRuleSets, Zenith_TerrainEdit& xOutEdit);
	// A finite fRatio in [8, 64] (GEOM's crack-free bound), else ENV_ERROR_INVALID_DESC with m_uIndex 0 and nothing changes; a LISTS
	// edit holding the ratio before and after; marks the terrain document (no tile) dirty for Save.
	Zenith_Result<void, Zenith_EnvironmentError> SetLODDistanceRatio(float fRatio, Zenith_TerrainEdit& xOutEdit);

	// Brushes: "Brush operations". pxJobs null runs single-threaded; the result is identical either way.
	Zenith_Result<void, Zenith_EnvironmentError> ApplyStroke(const Zenith_TerrainStroke& xStroke,
		Zenith_JobSystem* pxJobs, Zenith_TerrainEdit& xOutEdit);
	// Undo and redo: "Terrain edit deltas and undo".
	Zenith_Result<void, Zenith_EnvironmentError> ApplyEdit(const Zenith_TerrainEdit& xEdit,
		Zenith_TerrainEditDirection eDirection);

	// Preview and regeneration: the regions changed since the last call, in ascending (z, x) tile order.
	Zenith_Span<const Zenith_TerrainDirtyRegion> ConsumeDirtyRegions();

	// Saving: the terrain document and every dirty tile document through one journaled batch; clears the dirty flags on
	// success. Refused with ENV_ERROR_DOCUMENT_READ_ONLY on a newer-version document.
	Zenith_Result<void, Zenith_EnvironmentError> Save(Zenith_TerrainDocumentSink& xSink,
		const Zenith_Compression& xCodecForSave);

	// CPU sampling over the authored float heights (the editor's cursor, brush targets, placement preview): steps 1, 3 and
	// 4 of "Triangle interpolation" with h00 … h11 read as the authored floats (no quantisation, no decode); outside every
	// present tile ENV_ERROR_OUT_OF_BOUNDS.
	Zenith_Result<float, Zenith_EnvironmentError> SampleHeight(float fX, float fZ) const;
	Zenith_Result<Zenith_Vector3, Zenith_EnvironmentError> SampleNormal(float fX, float fZ) const;
};
```

`Zenith_TerrainTileCoord`, `Zenith_ExplicitFoliageInstance`, `Zenith_TerrainDirtyRegion` and `Zenith_TerrainEditDirection` are declared in `Zenith_EnvironmentTypes.h` (`Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentTypes header"`); the stroke and edit types in `Zenith_TerrainBrush.h` ("Stroke and dab model", "Terrain edit deltas and undo"). `Zenith_CompressionCodecSet` is COMPRESS's (`Zenith2/Docs/Design/COMPRESS.md → "What COMPRESS owns"`).

- `Load` validates in this order and stops at the first failure: the JSON parser limits ([Z2-N-ARCH-953], table "Parser limits": 64 MiB, depth 64); the schema version (newer: the document loads read-only and `ENV_WARNING_NEWER_SCHEMA` is logged, [Z2-N-CONTRACT-537]; older: `ENV_ERROR_DOCUMENT_MIGRATION`, because the tool-side migrator runs the chain before `Load` sees the bytes, "Migration and re-cook" in the entry); every field against "Terrain document schema" in `Zenith2/Docs/Design/ENV/Bake.md` (including `lodDistanceRatio` in [8, 64], `ENV_ERROR_DOCUMENT_SCHEMA` naming the field); every exclusion mask's rule set resolving to the terrain's list (`ENV_ERROR_DOCUMENT_SCHEMA`); each listed tile read through the source (`ENV_ERROR_TILE_MISSING`, `ENV_ERROR_TILE_CORRUPT`, and `ENV_ERROR_TILE_OUT_OF_RANGE` for a coordinate outside the grid or listed twice); every payload decoded and size-checked; holes only 0 or 255; weights summing to 255 per texel; heights finite and within the limits; borders equal, or repaired when `bRepairBorders`.
- Unknown JSON members of both document kinds are kept with their paths and re-emitted by `Save` at their anchors (rule C3 of `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"`), with one `MIGRATION_WARNING_UNKNOWN_FIELD` per document per path per load ([Z2-N-CONTRACT-543], [Z2-N-CONTRACT-544]).
- `Save` writes only dirty tiles plus the terrain document, each in SCENE's canonical form under its canonical schema (rules C1 to C12 with the terrain and tile schemas of `Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"`): it emits the known members through `Zenith_JsonWriter` (`Zenith2/Docs/Design/CORE/Text.md → "JSON writer"`) in the schema's member order, `tileFiles` sorted by (z, x), and every unknown member after its anchor with its raw number text, so its bytes equal what `Zenith_WriteCanonicalJson` writes for the same tree without ZenithEnvironment linking ZenithDocument (`Test_EnvironmentDocumentTypes_TerrainSaveIsCanonical`), and saving an unchanged document produces identical bytes and no tile write.

## Brush operations

### Stroke and dab model

A stroke is what one mouse drag, one tablet stroke or one scripted editor automation produces: an ordered list of dabs of one brush kind. EDIT's terrain tool builds strokes from input; ENV only applies them, so the same stroke applied twice to the same document produces the same bytes.

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_TerrainBrush.h
/// @contract enum Zenith_TerrainBrushKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (the height and foliage kinds); the paint, hole, density and exclusion kinds since Z2 Stage 4;
///   TERRAIN_BRUSH_ERODE_THERMAL since Z2 Stage 5. Values are stable.
/// @endcontract
enum Zenith_TerrainBrushKind : uint8
{
	TERRAIN_BRUSH_RAISE = 1,
	TERRAIN_BRUSH_LOWER,
	TERRAIN_BRUSH_FLATTEN,          // a rate toward a target height
	TERRAIN_BRUSH_SET_HEIGHT,       // an assignment of a target height
	TERRAIN_BRUSH_SMOOTH,
	TERRAIN_BRUSH_NOISE,
	TERRAIN_BRUSH_RAMP,             // dab = segment from m_xRampStart to the dab centre
	TERRAIN_BRUSH_ERODE_THERMAL,    // Stage 5
	TERRAIN_BRUSH_PAINT_LAYER,
	TERRAIN_BRUSH_ERASE_LAYER,
	TERRAIN_BRUSH_PAINT_HOLE,
	TERRAIN_BRUSH_ERASE_HOLE,
	TERRAIN_BRUSH_PAINT_DENSITY,
	TERRAIN_BRUSH_ERASE_DENSITY,
	TERRAIN_BRUSH_PAINT_EXCLUSION,
	TERRAIN_BRUSH_ERASE_EXCLUSION,
	TERRAIN_BRUSH_SCATTER_FOLIAGE,  // adds explicit instances
	TERRAIN_BRUSH_ERASE_FOLIAGE,    // removes explicit instances
};

/// @contract value Zenith_TerrainDab
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None.
/// Units: Metres; m_fStrength and m_fHardness are fractions in [0, 1].
/// Errors: Validated by ApplyStroke: non-finite values, a radius outside [0.5, 512] m or a fraction outside [0, 1] is
///   ENV_ERROR_BRUSH_INVALID with the dab index.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainDab
{
	float m_fCentreX = 0.0f;
	float m_fCentreZ = 0.0f;
	float m_fRadius = 8.0f;
	float m_fStrength = 0.5f;
	float m_fHardness = 0.5f;        // the inner fraction of the radius at full weight
};

/// @contract value Zenith_TerrainStroke
/// Ownership: Plain value holding a borrowed span; the dabs outlive the ApplyStroke call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None beyond the borrowed span.
/// Units: Metres; m_fAmount in metres per dab at full weight; m_fNoiseWavelength in metres; m_fTalusAngle in radians.
/// Errors: Validated by ApplyStroke (ENV_ERROR_BRUSH_INVALID; ENV_ERROR_LAYER_UNKNOWN; ENV_ERROR_FOLIAGE_TYPE_UNKNOWN).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainStroke
{
	Zenith_TerrainBrushKind m_eKind = TERRAIN_BRUSH_RAISE;
	Zenith_Span<const Zenith_TerrainDab> m_axDabs;       // 1 … 65,536 dabs, applied in order
	float m_fAmount = 0.25f;                              // RAISE, LOWER, NOISE
	float m_fTargetHeight = 0.0f;                         // FLATTEN, SET_HEIGHT
	Zenith_Vector3 m_xRampStart;                          // RAMP: start point (x, height, z)
	float m_fRampEndHeight = 0.0f;                        // RAMP: height at each dab centre
	float m_fNoiseWavelength = 16.0f;                     // NOISE
	uint64 m_ulNoiseSeed = 0;                            // NOISE, SCATTER_FOLIAGE; chosen by EDIT from its stroke counter
	Zenith_PersistentID m_xLayerID;                       // PAINT_LAYER, ERASE_LAYER
	uint32 m_uFoliageType = 0;                           // the density, scatter and erase-foliage kinds
	uint32 m_uRuleSetOrAll = uENV_EXCLUSION_ALL;         // PAINT_EXCLUSION, ERASE_EXCLUSION
	float m_fScatterDensity = 0.1f;                       // SCATTER_FOLIAGE: instances per m² at full weight
	uint32 m_uErosionIterations = 8;                     // ERODE_THERMAL: 1 … 64
	float m_fTalusAngle = 0.6f;                           // ERODE_THERMAL
};

/// @contract functions Zenith_TerrainBrushFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Normalised distance in [0, ∞); hardness in [0, 1].
/// Errors: None; inputs outside the domain clamp.
/// Version: since Z2 Stage 3.
/// @endcontract
float Zenith_TerrainBrushFalloff(float fNormalisedDistance, float fHardness);   // "Brush falloff"
```

### Brush falloff

For a sample or texel at world XZ `p` and a dab of centre `c`, radius `R` and hardness `H`, `d = |p − c| / R` is computed as `sqrtf(dx × dx + dz × dz) / R` in `float` with no contraction (`Zenith2/Docs/Architecture/BuildSystem.md → "Compile and link flags"`):

| Range | Weight `w` |
|---|---|
| `d ≥ 1` | 0 (the sample is outside the dab and is not written) |
| `d ≤ H` | 1 |
| `H < d < 1` | `x = (d − H) / (1 − H)`, `w = 1 − x × x × (3 − 2x)` |

`H = 1` gives a hard disc; `H = 0` gives a full smoothstep. `Zenith_TerrainBrushFalloff` is the one implementation; the editor's brush cursor calls it too, so the cursor draws the weights the brush writes.

### Brush application order

- Dabs apply strictly in stroke order; dab `k + 1` sees the result of dab `k`.
- Within one dab, every written sample reads the **pre-dab snapshot** of the dab's affected rectangle (plus the kernel margin of SMOOTH and ERODE_THERMAL), and the new values are written after every read; the result of a dab therefore does not depend on the order in which its samples are visited.
- A dab's affected rectangle is the samples (or texels) whose world position lies in `[c − R, c + R]` per axis, clipped to present tiles; a brush never creates a tile.
- A value is computed once per unique world sample (the canonical copy is in the tile with the smaller `(z, x)` index) and copied to the other copies of a shared border sample, so borders stay equal by construction.

### Brush kinds

`h` is the pre-dab height, `h′` the new height, `S` the dab strength, `w` the falloff weight, `A` = `m_fAmount`, `T` = `m_fTargetHeight`. Every height result is clamped to the document's limits. A `uint8` result is `clamp(floor(x + 0.5), 0, 255)` with `x` in `float`.

| Kind | Rule |
|---|---|
| RAISE | `h′ = h + S × w × A` |
| LOWER | `h′ = h − S × w × A` |
| FLATTEN | `h′ = h + (T − h) × (S × w)`: a rate; a dab with `S × w = 1` assigns, smaller values approach `T` geometrically (two dabs at 0.5 close 75% of the gap) |
| SET_HEIGHT | `h′ = h + (T − h) × w`: an assignment inside the hard region whatever the strength; `S` is ignored |
| SMOOTH | `g` = the 5 × 5 binomial blur of the snapshot (separable weights 1, 4, 6, 4, 1 over 16, the horizontal pass then the vertical pass in `float`; samples beyond present tiles clamp to the nearest present sample); `h′ = h + (g − h) × (S × w)` |
| NOISE | `n` = value noise at `p / m_fNoiseWavelength`: each corner `L` of the containing unit lattice cell takes `Zenith_RandomToUnitFloat(Zenith_Hash64U64(pack(L), m_ulNoiseSeed))`, where `pack(L)` = `(uint64)(uint32)Lx << 32 | (uint32)Lz`; the corners blend with the smoothstep of the fractional coordinates; `h′ = h + S × w × A × (2n − 1)` |
| RAMP | for the segment from `m_xRampStart` (x, z) to the dab centre: `q` = the projection parameter of `p` clamped to [0, 1], `r` = the distance from `p` to the segment; target `T(p) = lerp(m_xRampStart.y, m_fRampEndHeight, q)`; the weight uses `r / R` in place of `d`; `h′ = h + (T(p) − h) × (S × w)` |
| ERODE_THERMAL (Stage 5) | `m_uErosionIterations` Jacobi iterations over the rectangle, each from the previous iteration's buffer: for each sample and each of its 4 neighbours with `Δ = h − h_n > t` (`t = tan(m_fTalusAngle) × 0.5 m`), move `0.25 × (Δ − t) × S × w` from the sample to the neighbour; moves accumulate into a separate buffer applied at the end of the iteration |
| PAINT_LAYER | the painted layer's weight `x = W + S × w × (255 − W)`, then "Weight normalisation" |
| ERASE_LAYER | `x = W − S × w × W`, then "Weight normalisation" (the removed weight goes to the other layers in proportion, or to the base layer when the erased layer held all of it) |
| PAINT_HOLE, ERASE_HOLE | a texel with `S × w ≥ 0.5` becomes 255 (paint) or 0 (erase); others are unchanged |
| PAINT_DENSITY | `D′ = D + S × w × (255 − D)` on the density map of `m_uFoliageType` (created at 0 when absent) |
| ERASE_DENSITY | `D′ = D − S × w × D` |
| PAINT_EXCLUSION, ERASE_EXCLUSION | as PAINT_DENSITY and ERASE_DENSITY on the exclusion mask of `m_uRuleSetOrAll` |
| SCATTER_FOLIAGE | one `Zenith_Place` run per dab over an implicit rule and the scatter surface adapter, with the falloff weight as the density factor and the existing explicit instances as obstacles; the kept instances become explicit instances with fresh IDs ("Scatter brush") |
| ERASE_FOLIAGE | removes the explicit instances of `m_uFoliageType` (every type when it is `uENV_FOLIAGE_TYPE_ALL`) whose position has `w ≥ 0.5` |

A sparse map that an erase leaves all zero is dropped from the tile; the edit records the drop, so undo restores it.

### Scatter brush

Dab k of a SCATTER_FOLIAGE stroke (k counts from 0 in stroke order) runs exactly this placement, so a scatter stroke is a pure function of the document, the stroke and the dab order:

| Input of `Zenith_PlacementDesc` | Value |
|---|---|
| `m_ulSeed` | `Zenith_Hash64U64(k, m_ulNoiseSeed)` |
| `m_axRuleSets` | one implicit set: `m_xID` = the null `Zenith_PersistentID`, one rule |
| the rule | name `"Environment.Scatter"`, so `ruleStableID = Zenith_Hash64("Environment.Scatter", Zenith_Hash64U64(0, 0))`; foliage type = entry `m_uFoliageType` of the terrain's list; `densityPerSquareMetre = m_fScatterDensity × S` (at most 64); `minDistance = min(2 × footprint, 64)`, so two instances of the stroke keep two footprints apart (64 m for a footprint above 32 m); no slope, height, layer, water or noise filter; `useDensityMap = true`; `excludeEarlierRules = true` with `exclusionRadius = min(footprint, 64)`, so a kept instance keeps `footprint + footprint(obstacle)` from every obstacle; scale range the foliage type's; `alignToNormal = 0` (the cook leans explicit instances, "Foliage cook" in Bake.md) |
| `m_axFoliageTypes` | the terrain's list, from the foliage type documents EDIT has loaded |
| `m_axLayerPalette` | the terrain's layer IDs in palette order (the implicit rule names no layer) |
| `m_pxSurface` | a `Zenith_ScatterSurface` (`Internal/Terrain/Zenith_ScatterSurface.h`, internal) over the document and the dab: `Sample` returns false outside the dab's disc (falloff distance `d ≥ 1`) and outside present tiles, else the triangle height and normal of `SampleHeight` / `SampleNormal`, `m_bHole` from the hole mask, and `m_fExclusion` from the ALL mask alone (a scatter belongs to no rule set); `SampleDensity` returns the dab's falloff weight `w` at the point; `SampleLayerWeight` returns 1; `IsUnderWater` returns false |
| `m_axCells` | the placement cells (128 m, anchored at the terrain origin) that the dab's square `[c − R, c + R]` touches |
| `m_axObstacles` | every explicit instance of the present tiles in those cells and in their 8 neighbours, as the document holds them before this dab (so the instances earlier dabs of the stroke added are obstacles), each with its type's footprint radius |
| `m_uMaxInstancesPerCell` | `uENV_TERRAIN_MAX_EXPLICIT_INSTANCES_PER_TILE` (16,384) |

The kept instances, in the result's cell order and key order, become explicit instances of the tile containing each position: ID = that tile's `m_uNextExplicitID++`, tile-local position, yaw `2 × atan2(q.y, q.w)` of the instance's rotation (a yaw alone, because `alignToNormal` is 0), the drawn scale, `m_bKeepHeight = false`. A dab whose tile would exceed 16,384 explicit instances, or whose next ID would reach 2^31, fails the stroke with `ENV_ERROR_CAPACITY` and the dab index, and the stroke restores the document's pre-stroke bytes from its own before buffers, so nothing changes. `Test_TerrainBrush_ScatterFoliage_Deterministic` asserts identical instances and IDs for the same stroke, and `Test_TerrainBrush_ScatterFoliage_KeepsFootprints` asserts that every new instance lies at least `footprint(new) + footprint(existing)` from every pre-existing explicit instance and at least `min(2 × footprint, 64)` from every other new instance of the stroke (instances of earlier dabs are obstacles of the subsequent ones, so the same bound holds across dabs).

### Weight normalisation

After a paint or erase dab changes a layer's weight `W_L` to `x_L` on a texel, the other layers are rescaled so the texel sums to exactly 255:

1. `x_L` is rounded to an integer `X_L` in [0, 255] by the rounding rule above.
2. The remainder `R = 255 − X_L` is distributed over the other painted layers of the texel in proportion to their pre-dab weights: layer `i` gets `floor(R × W_i / ΣW_other)` in integer arithmetic; when `ΣW_other = 0`, the whole remainder goes to the base layer, or to the lowest-index other painted layer when `L` is the base layer.
3. The shortfall `R − Σ floor(…)` is added one unit at a time to the other layers in descending order of their remainders `R × W_i mod ΣW_other`, ties to the lower palette index.

After step 1 the algorithm is integer arithmetic, so its result is independent of platform and thread.

### Height clamping and border consistency

- Every brush clamps results to `[m_fMinHeight, m_fMaxHeight]`; a stroke whose unclamped result exceeds the limits increments `environment.terrain.brush_clamped` and EDIT shows a one-line warning; it is not an error.
- `Load` compares every shared border sample; a difference is `ENV_ERROR_TILE_BORDER_MISMATCH` (the smaller tile index in `m_uIndex`) unless `bRepairBorders` is set, in which case the smaller-index tile's value is copied, the tile is marked dirty and `environment.terrain.borders_repaired` counts the samples.

### Parallel application

When `pxJobs` is non-null and a dab's rectangle holds more than 4,096 samples, `ApplyStroke` splits the rectangle's rows into bands of 64 rows and runs one sub-job per band (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem header"`), each reading the snapshot and writing only its own rows; the stroke joins every band before the next dab. ERODE_THERMAL joins every band after **each iteration**, and the next iteration's bands read the buffer the joined iteration wrote, because a band's rows move material into their neighbours' rows. The snapshot (or the previous iteration's buffer) and the output rows are disjoint, so the result is bit-identical for 1, 2, 8 and all workers. SCATTER_FOLIAGE runs `Zenith_Place` with the same `pxJobs`, which is itself worker-invariant.

## Splat layers and blending

### Authoring weights

The authored representation is the sparse per-layer `uint8` weight map of "Layers", one texel per quad, summing to 255 per texel. The editor may show any number of layers; the palette caps a terrain at 16.

### Cooked top-four reduction

The cook reduces each texel to at most four (layer, weight) pairs, the form the material samples:

1. List the texel's non-zero weights as (palette index, weight) pairs.
2. Sort by weight descending, ties to the lower palette index.
3. Keep the first four; the others' total is redistributed over the kept four by steps 2 and 3 of "Weight normalisation" (proportional, then remainder order), so the kept weights sum to 255.
4. Write the index texel (four `uint8` palette indices; an unused slot repeats the first index with weight 0) and the weight texel (four `uint8` weights) into two 256 × 256 `R8G8B8A8_UNORM` source images per tile, cooked by GEOM's texture step with the classes `FLUX_TEXTURE_USAGE_SPLAT_INDEX` (raw, point-decimated chain, never filtered: a texel is read with `Load` and decoded `uint4(round(x × 255))`, which is exact because an UNORM8 lane decodes to k / 255) and `FLUX_TEXTURE_USAGE_SPLAT_WEIGHT` (raw, the integer-mean chain), both resident whole with the asset (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`, "Texture cook pipeline").

The reduction is lossless for texels with at most four layers; the cook report member `splatTexelsOverFour` counts the others so an artist can find them.

### Layer blend in the terrain material

For a shaded point, the material evaluates the splat of the 2 × 2 texel footprint around it, so layer boundaries are filtered like a bilinear texture without ever filtering indices:

1. From world XZ, the tile `(i, j)` and the continuous texel coordinate `q = (xz − tileMin) / 0.5 − 0.5`; the four texel centres around `q` and their bilinear weights `b_k` (a texel beyond the tile clamps to its edge texel; adjacent tiles meet continuously because their edge texels are computed from adjacent authored quads).
2. For each of the four texels, `Load` its index texel and decode the four palette indices as `uint4(round(x × 255))`, and for each of its four slots add `b_k × weight / 255` to an accumulator per palette index (16 `half` accumulators).
3. Select the `N` indices with the largest accumulated weight (N = the tier's layers per pixel, "Layers per pixel by quality tier"), ties to the lower index, and renormalise their weights to sum to 1.
4. Sample the `N` layers (albedo, normal, ORMH) with explicit gradients and apply "Height-based blend".

### Height-based blend

With normalised weights `w_i` and layer heights `η_i` (the H channel of ORMH, [0, 1]): `m_i = w_i + η_i`, `M = max_i m_i`, `δ_i` = the layer's `m_fHeightBlendDepth`; `w′_i = max(0, m_i − M + δ_i)`, renormalised to sum to 1; when every `w′_i` is 0 the weights stay `w_i`. `δ = 1` for every layer reproduces the linear blend.

### Layers per pixel by quality tier

| Tier class | `uPERM_LAYERS` | Rule |
|---|---|---|
| desktop tiers | 4 | the four strongest layers of the footprint |
| S24 tiers | 2 | the two strongest, renormalised |

The value comes from the configuration key `environment.terrain.layersPerPixel` (2 or 4, allowed in the quality-tier layer), which selects the permutation; it changes presentation alone. The debug view `r.debug.env.splat` draws the strongest layer's index as a flat colour.

## Terrain edit deltas and undo

### Zenith_TerrainEdit contents

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_TerrainBrush.h (continued)
/// @contract type Zenith_TerrainEdit
/// Ownership: Move-only; filled by the document's edit-producing members and owned by the caller (EDIT's undo stack holds
///   one per terrain command); its channel buffers come from the heap of the document that produced it.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Tied to the document that produced it: applying it to another document is ENV_ERROR_EDIT_STALE.
/// Units: Bytes (GetMemoryBytes).
/// Errors: None of its own; ApplyEdit validates it.
/// Version: since Z2 Stage 3. In-memory only; never serialized.
/// @endcontract
class Zenith_TerrainEdit
{
public:
	Zenith_TerrainEdit();
	Zenith_TerrainEdit(Zenith_TerrainEdit&& xOther);
	Zenith_TerrainEdit& operator=(Zenith_TerrainEdit&& xOther);
	Zenith_TerrainEdit(const Zenith_TerrainEdit&) = delete;
	Zenith_TerrainEdit& operator=(const Zenith_TerrainEdit&) = delete;
	~Zenith_TerrainEdit();
	bool IsEmpty() const;
	Zenith_PersistentID GetDocumentID() const;
	uint64 GetRevisionBefore() const;
	uint64 GetRevisionAfter() const;
	uint64 GetMemoryBytes() const;
	Zenith_TerrainEditKind GetKind() const;          // STROKE, TILE_ADD, TILE_REMOVE, LAYERS, LISTS
	Zenith_Span<const Zenith_TerrainDirtyRegion> GetRegions() const;
};
```

An edit holds, per touched (tile, channel), the affected rectangle and the **before** and **after** contents of that rectangle. Channels: heights (`float`), one per layer weight map, holes, one per density map, one per exclusion mask, the explicit-instance list (the touched instances before and after), and tile presence (add or remove, with the whole tile's content). A structural edit records the palette or list before and after; `SetLODDistanceRatio` records the ratio before and after as a `TERRAIN_EDIT_KIND_LISTS` edit (the kind covers the document's lists and its one editable scalar).

### Delta encoding

- While a stroke runs, the before rectangle of each (tile, channel) is copied the first time a dab touches it and grown to the union rectangle as subsequent dabs touch more, so memory is proportional to the area touched, not to the dab count.
- When the stroke ends, both buffers are compressed with COMPRESS's section form (`Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"`) at zstd level 1, and the uncompressed buffers are released.
- An edit whose compressed size exceeds the pending row "Terrain edit delta memory per undo step" is still applied and returned, and `environment.terrain.edit_over_budget` is incremented; EDIT's undo policy (pending EDIT) decides whether to keep it.

### Undo integration

- `ApplyEdit(xEdit, TERRAIN_EDIT_DIRECTION_REVERT)` requires `GetRevision() == xEdit.GetRevisionAfter()` and writes every before buffer; `TERRAIN_EDIT_DIRECTION_REAPPLY` requires `GetRevision() == xEdit.GetRevisionBefore()` and writes every after buffer. Either sets the document's revision to the other end, so an undo followed by a redo returns the document to the exact bytes it had.
- A document or revision mismatch is `ENV_ERROR_EDIT_STALE` and changes nothing: EDIT's transaction stack guarantees order, and the check turns a misuse (an edit applied out of stack order) into an error instead of a corrupt document.
- EDIT's terrain command owns the `Zenith_TerrainEdit` (pending EDIT, "Assumed types from pending design documents" in the entry); ENV declares no undo stack of its own and holds no pointer into the editor.
- Every applied edit marks its regions dirty for preview and for collision, navigation and placement regeneration (`Zenith2/Docs/Design/ENV/Bake.md → "Regeneration after edits"`).

## Live preview

In the edit world, sculpting and painting update the rendered terrain within one frame of the edit, without a full cook. The preview never swaps a single mip: CDLOD's crack-freeness rests on the point-decimated chain (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain data"`), so every prepared tile carries its whole chain, computed exactly as the cold cook computes it: the quantisation of "Height representation" (GEOM's output step 1 of `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain cook contract"`, the same formula), the hole conversion of `Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"` (absent neighbours counted as hole quads), and the chain rules of `Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"` step 3 (point decimation for heights, holes and splat indices; the integer mean `(s + c / 2) / c` for splat weights and the detail density). Each rule is a bit copy or integer arithmetic over the quantised samples, so the preview implements them in `Internal/Terrain/Zenith_TerrainPreview.cpp` instead of linking cook code, which the editor never links (ASSET's A11); `Test_TerrainPreview_ChainEqualsColdCook`, a ZenithCook test that links both, proves every previewed mip equal to the output of `Zenith_CookTerrainGeometry` and GEOM's texture step for the same document.

| Step | Thread and node | Action |
|---|---|---|
| 1 | EDIT, main thread, after `ApplyStroke` or `ApplyEdit` | `ConsumeDirtyRegions`; the list goes to `Zenith_TerrainPreview::QueueRegions` |
| 2 | EDIT, main thread, in the frame prologue of the edit world's frame, before the frame graph runs | `Zenith_TerrainPreview::Prepare`: for at most `environment.terrain.previewTilesPerFrame` (default 4) dirty tiles, in ascending (z, x) order, steps 2a to 2d |
| 2a | | quantise the tile's 257² authored heights with the running terrain's cooked range; values beyond it clamp and log `ENV_WARNING_PREVIEW_RANGE_EXCEEDED` once per tile (the next cook widens the range) |
| 2b | | when heights or holes are dirty: the tile's 257 × 257 hole samples by the conversion of "Geometry hand-off to GEOM"; the 9-mip `R16_UNORM` height chain and, when the cooked tile has a hole tile, the 9-mip `R8_UNORM` hole chain, both by point decimation; the tile's (min, max) over its closed rectangle. A hole painted in a tile whose cook produced no hole tile cannot be shown until the next cook (a texture cannot be created by a replacement): the preview logs `ENV_WARNING_PREVIEW_HOLE_TILE_MISSING` once per tile |
| 2c | | when a layer channel is dirty: the tile's splat index and weight images recomputed whole with "Cooked top-four reduction" and their chains by GEOM's rules for the two splat classes; when a detail type's density map is dirty: the packed detail-density image and its chain; a chain whose channel is clean is not produced and its texture is not replaced |
| 2d | | node bounds: the preview keeps every tile's (min, max) in a table; for each level whose node is smaller than a tile (levels 0 to 2 at a 16 m leaf) the min/max of each node overlapping the tile is recomputed from the quantised samples of its closed rectangle, and for each larger level from the table entries of the tiles it covers; the results are grouped into row runs of consecutive node indices per level |
| 2e | | the prepared tiles are published; they stay immutable until the next `Prepare` |
| 3 | EDIT's extractor of the edit world (phase 12) | `Zenith_TerrainPreview::WriteRecords` copies each published tile into one `Flux_RenderTerrainTileUpdate` (`Zenith2/Docs/Design/ENV.md → "Render-world records"`); it reads only the published buffers, and the prologue that could change them precedes the frame graph |
| 4 | `Flux.BuildGraph` | ENV's GPU side calls GEOM's `Flux_TextureManager::ReplaceChainFromMemory` (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture replace from memory"`) once per prepared chain (height, hole, splat index, splat weight, detail density) with the whole chain. A chain whose call returns `PEAK_REFUSED` (the governor refused the swap's transition peak) or any other error is retained on ENV's GPU side, keyed by (terrain key, tile index, chain kind); only refused chains are retained, never accepted ones. A later record of the same tile replaces only the chains it carries, so a pending height chain survives a splat-only update, and each frame re-applies the pending chains before that frame's records of the same tile; a pending height replacement keeps its node runs. The retained bytes are counted in `Flux.Environment.PreviewPendingBytes` and bounded by `uFLUX_ENV_PREVIEW_PENDING_MAX_BYTES` (64 MiB, `Internal/Environment/Flux_EnvironmentRenderer.h`): a refusal that would exceed the bound drops the oldest pending tile's chains whole, counts `Flux.Environment.PreviewPendingDropped` and logs once per terrain (that tile shows its edit at its next preview update or cook); removing the terrain clears its entries. So no accepted edit is lost and nothing is partially shown beyond the chains that were accepted. The node bounds are deferred to the height texture: ENV records the tile's node runs with the accepted height replacement and calls `Flux_TerrainRenderer::UpdateNodeBounds` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain public API"`) once per run only in the frame in which that replacement publishes (the height texture's `GetSRV()` changes, GEOM's step 5), never before, so culling never uses bounds of heights not yet drawn; a superseded replacement carries its runs over to the one that supersedes it. The tile's entries of the material tile table ("Terrain material module") are rewritten in the same publishing frame |
| 5 | the stroke's end | collision, navigation and foliage are regenerated for the dirty tiles (`Zenith2/Docs/Design/ENV/Bake.md → "Regeneration after edits"`), not per dab |

Every mip of a previewed tile equals the corresponding mip of a cold cook of the edited document at the same height range, because both apply the same formulas to the same quantised samples (`Test_TerrainPreview_ChainEqualsColdCook` on the CPU, `Test_EnvRender_TerrainPreviewSwap` by GPU readback of every mip).

The time from the end of a dab's `ApplyStroke` to the first presented frame showing it is measured against the pending editor row "Terrain sculpt preview latency" (producer: EDIT's W-Editor row tickets).

## Zenith_TerrainRuntime header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_TerrainRuntime.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"

class Zenith_Heap;
class Zenith_TerrainDocument;
class Flux_RenderWorldWriter;

/// @contract value Zenith_TerrainHeightSample
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: Metres; m_xNormal is unit length.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainHeightSample
{
	float m_fHeight = 0.0f;
	Zenith_Vector3 m_xNormal;                 // of the triangle containing the point
	uint16 m_uPhysicalMaterial = 0;          // of the quad containing the point
	uint16 m_uTerrainIndex = 0;              // the answering terrain's index in this resource's list, 0 … GetTerrainCount() − 1
	bool m_bHole = false;                     // the point is over a hole; m_fHeight is still the triangle's height
};

/// @contract type Zenith_TerrainWorldResource
/// Ownership: One per world, owned by the Environment module instance and registered as the world resource
///   "Zenith.Environment.TerrainWorld" with owning phase PHASE_INPUT; it holds the terrain and tile asset handles whose
///   loaded data it reads.
/// Thread affinity: PHASE(Input) for writes (the Environment.UpdateTerrainTiles system alone); reads from any system that
///   declares ReadResource on it, in any phase, and from the extractors; reads never lock.
/// Invalidation: Answers change only at phase 1 of a subsequent tick, when tiles activate or deactivate.
/// Units: Metres.
/// Errors: Queries return ENV_ERROR_OUT_OF_BOUNDS outside every terrain and ENV_ERROR_NOT_RESIDENT for a present tile whose
///   CPU data is not active; neither asserts.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_TerrainWorldResource
{
public:
	Zenith_Result<Zenith_TerrainHeightSample, Zenith_EnvironmentError> Sample(float fX, float fZ) const;
	Zenith_Result<float, Zenith_EnvironmentError> SampleHeight(float fX, float fZ) const;
	bool IsResident(float fX, float fZ) const;
	uint32 GetTerrainCount() const;
	uint32 GetResidentTileCount() const;
	// A ray against resident tiles (editor picking and gameplay line checks without physics): "Terrain raycast".
	Zenith_Result<Zenith_Vector3, Zenith_EnvironmentError> Raycast(const Zenith_Vector3& xOrigin,
		const Zenith_Vector3& xDirection, float fMaxDistance, uint32 uMaxSteps) const;
};

/// @contract value Zenith_TerrainPreviewDesc
/// Ownership: Plain value passed to Zenith_TerrainPreview::Create.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres.
/// Errors: Create validates it (ENV_ERROR_INVALID_DESC: a zero key, an inverted range, a tile budget outside 1 … 64).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TerrainPreviewDesc
{
	uint64 m_ulTerrainKey = 0;                 // the Flux_RenderTerrainDelta key of the running terrain in the edit world
	float m_fCookedHeightMin = 0.0f;            // the running terrain's cooked range ("Height representation")
	float m_fCookedHeightMax = 0.0f;
	uint32 m_uTilesPerFrame = 4;               // environment.terrain.previewTilesPerFrame
};

/// @contract type Zenith_TerrainPreview
/// Ownership: Created by EDIT for each edit world holding an open terrain document and owned by it as a Zenith_Owned;
///   an ordinary type of the module with no conditional compilation ([Z2-N-CONTRACT-441]); its prepared chains come from
///   the heap passed to Create (at most m_uTilesPerFrame × 1,051,664 bytes: a 176,824-byte height chain, an 88,412-byte
///   hole chain, two 349,524-byte splat chains and an 87,380-byte detail-density chain per tile).
/// Thread affinity: MAIN_THREAD for QueueRegions and Prepare; WriteRecords from the one EDIT extractor node of the edit world,
///   which reads only what the last Prepare published and never runs concurrently with Prepare (the prologue precedes
///   the frame graph).
/// Invalidation: Invalid after the terrain document it watches is destroyed.
/// Units: None.
/// Errors: QueueRegions returns ENV_ERROR_CAPACITY when its bounded queue (1,024 regions, QUEUE_FULL_POLICY_REJECT) is
///   full; EDIT then queues the whole tile set of the edit, which always fits, so no change is lost. WriteRecords returns the
///   writer's error when the render world refuses a record; the tile stays queued for the next frame.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_TerrainPreview
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_TerrainPreview>, Zenith_EnvironmentError> Create(
		const Zenith_TerrainDocument& xDocument, const Zenith_TerrainPreviewDesc& xDesc, Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_EnvironmentError> QueueRegions(Zenith_Span<const Zenith_TerrainDirtyRegion> axRegions);
	void Prepare();                                                              // "Live preview" step 2
	Zenith_Result<void, Zenith_EnvironmentError> WriteRecords(Flux_RenderWorldWriter& xWriter) const;   // step 3
	uint32 GetPendingTileCount() const;
};
```

`Zenith_TerrainRuntime` itself is internal (`Internal/Terrain/Zenith_TerrainRuntime.h`): the per-world object of the module instance that owns the resource above, tracks `Zenith_TerrainComponent` and `Zenith_TerrainTileComponent` entities, holds the terrain and tile asset handles, and writes the terrain records of the render world.

| Event | Action |
|---|---|
| a `Zenith_TerrainComponent` appears (change-filtered query in `Environment.UpdateTerrainTiles`, phase 1) | request its terrain asset (`ASSET_KIND_TERRAIN`, `Zenith2/Docs/Design/ASSET.md → "Asset kinds"`) at `ASSET_PRIORITY_HIGH`; the terrain is answerable once the asset is active at a tick boundary |
| the terrain asset becomes active | register the terrain in the resource (extent, cooked height range, tile grid); the next extraction writes a `Flux_RenderTerrainDelta` ADD |
| a `Zenith_TerrainTileComponent` appears (a streamed cell spawned it) | request its tile asset (`Environment.TerrainTile`); when active at a tick boundary, insert its CPU data into the resource |
| a tile component disappears (the cell unloaded) | remove the tile from the resource at that phase 1 and release the handle |
| the terrain component disappears | remove every tile of the terrain and release the terrain asset; the next extraction writes REMOVE |
| full-state extraction ([Z2-N-REND-015]) | every live terrain is written as ADD with its current state |

Asset activation reaches simulation only at tick boundaries in completion-ID order (Z2-D-053), so the tile set a phase-1 system sees is identical on replay.

## Height queries

### Triangle interpolation

The CPU height is what the collision surface and the rendered LOD-0 surface agree on: the triangle containing the point, with each quad split along its `(s, t)`–`(s + 1, t + 1)` diagonal (the split GEOM's grid mesh and PHYS's heightfield use, pending both, "Assumed types from pending design documents" in the entry).

1. Tile `(i, j) = (floor((x − originX) / 128), floor((z − originZ) / 128))`; local sample coordinates `u = (x − tileMinX) / 0.5`, `v = (z − tileMinZ) / 0.5`; quad `(s, t) = (min(floor(u), 255), min(floor(v), 255))`; fractions `fu = u − s`, `fv = v − t`.
2. Heights `h00, h10, h01, h11` of samples `(s, t)`, `(s + 1, t)`, `(s, t + 1)`, `(s + 1, t + 1)`, decoded from `uint16` by the formula of "Height representation".
3. If `fu ≥ fv`: `h = h00 + fu × (h10 − h00) + fv × (h11 − h10)`; else `h = h00 + fv × (h01 − h00) + fu × (h11 − h01)`.
4. The normal is the normalised cross product of the triangle's edges in world units (0.5 m per sample step), oriented +Y.

Every step is `float` with the operation order written above, so a query is bit-identical on every worker of one build ([Z2-N-ARCH-883]).

### Physical material queries

The quad's physical material is the cooked per-quad `uint8` index into the terrain's physical-material table: the layer with the largest authored weight on that quad's texel (ties to the lower palette index), mapped through its `m_uPhysicalMaterial`. Footstep audio (AUDIO) and surface effects (VFX) read it through `Sample`.

### Holes in queries

A point over a hole quad returns the triangle height with `m_bHole = true`; gameplay decides whether to fall through, and the collision surface has no triangle there (`Zenith2/Docs/Design/ENV/Bake.md → "Heightfield extraction"`).

### Terrain raycast

`Raycast(origin, direction, fMaxDistance, uMaxSteps)` walks the quads the ray's XZ projection crosses, in order, with a 2D DDA over the 0.5 m sample grid (Amanatides–Woo): the direction is normalised in `float` (a zero or non-finite direction, a negative or non-finite distance, or `uMaxSteps` of 0 is `ENV_ERROR_INVALID_DESC`); a vertical ray visits the one quad under the origin.

1. Clip the ray to the XZ rectangle of every terrain of the world and to `[0, fMaxDistance]`; with no overlap, `ENV_ERROR_OUT_OF_BOUNDS`.
2. For each visited quad, in DDA order: if its tile is present but not active, return `ENV_ERROR_NOT_RESIDENT` (the ray cannot see past data it does not have); if the quad is a hole, continue; else intersect the ray with the quad's two triangles of "Triangle interpolation" (the (s, t)–(s + 1, t + 1) split, heights decoded as step 2 states) with the Möller–Trumbore test in `float`, parameter t ≥ 0 only, and return the nearest hit of the quad (the lower triangle first on a tie).
3. Each visited quad counts one step; after `uMaxSteps` quads without a hit the result is `ENV_ERROR_CAPACITY` with the steps taken in `m_uIndex`, so a caller can distinguish "no hit within the budget" from "no terrain"; a ray that leaves the clipped range without a hit is `ENV_ERROR_OUT_OF_BOUNDS`.

A ray over an absent tile (outside the terrain's present tiles) passes through it. `Test_TerrainRuntime_Raycast` asserts hits within 1 mm of the analytic intersection on a sloped fixture, a pass through holes, `ENV_ERROR_NOT_RESIDENT` on an inactive tile before the hit, and `ENV_ERROR_CAPACITY` when `uMaxSteps` is smaller than the quads to the hit.

### Residency and tick-boundary activation

Only tiles whose tile asset is active are answered; a point in a present but inactive tile returns `ENV_ERROR_NOT_RESIDENT` and increments `environment.terrain.height_queries_not_resident`. Tile CPU data for a streamed cell becomes active at the tick boundary that publishes the asset load, so every query of that tick, on any worker, sees the same tile set.

## Terrain material module

The terrain surface is one engine material type, `Material_TerrainLayered`, in `Zenith2/Shaders/Materials/TerrainLayered.slang` (engine materials live under `Zenith2/Shaders/Materials/`, `Zenith2/Docs/Rendering.md → "Materials"`), drawn through GEOM's terrain adapter (`Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"`). Materials stay code (Z2-D-079); what artists author visually is the painted weights and the layer documents' values.

Each terrain gets its own material asset: the terrain importer's stage `Environment.TerrainMaterial` (`Zenith2/Docs/Design/ENV/Bake.md → "Step graph"`) writes a generated material document (module `Materials/TerrainLayered.slang`, type `Material_TerrainLayered`, domain `opaque`, the 48 layer texture GUIDs of the palette's layer documents, the per-layer values and `m_uTerrainRecord` = 0, a placeholder that no runtime draw reads, "Terrain record slots") and cooks it through GEOM's material cook as the terrain's sub-asset `material:terrain` (its `DeclareProduct` key, `Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`); `Flux_TerrainDesc::m_xMaterial` is its `Flux_MaterialID`. The layer textures are therefore ordinary material texture fields, which GEOM's material manager re-patches on every streaming swap (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`); the per-tile splat, height, hole and density textures, too many for material fields, are reached through the terrain's material tile table below.

```slang
// Zenith2/Shaders/Common/Environment.slang (every [FluxShared] struct of ENV lives here, [Z2-N-REND-136])
[FluxShared] struct Env_TerrainTileTextures           // 64 bytes, one per grid cell (row-major); an absent tile holds null handles
{
	DescriptorHandle<Texture2D<float>> m_xHeight;       // GEOM's height tile (FLUX_TEXTURE_USAGE_HEIGHT)
	DescriptorHandle<Texture2D<float>> m_xHoles;        // GEOM's hole tile, or null
	DescriptorHandle<Texture2D<float4>> m_xSplatIndex;  // FLUX_TEXTURE_USAGE_SPLAT_INDEX: Load, then uint4(round(x × 255))
	DescriptorHandle<Texture2D<float4>> m_xSplatWeight; // FLUX_TEXTURE_USAGE_SPLAT_WEIGHT
	DescriptorHandle<Texture2D<float4>> m_xDetailDensity;   // 128², channel k = detail type k's density, or null (GEOM's SPLAT_WEIGHT class, Textures.md "Texture source image intermediate")
	uint2 m_axReserved[3];
};

[FluxShared] struct Env_TerrainGpuRecord              // 48 bytes, one per active terrain of the frame
{
	float2 m_xOriginXZ;
	uint m_uTilesX;
	uint m_uTilesZ;
	float m_fHeightMin;                                 // the cooked range, for decoding height texels
	float m_fHeightMax;
	uint m_uDetailTypeCount;                            // 0 … 4
	uint m_uReserved;
	DescriptorHandle<StructuredBuffer<Env_TerrainTileTextures>> m_xTileTable;
	uint2 m_xPlacementSeed;                             // the terrain's uint64 placement seed, low word first (detail hash)
};

// Zenith2/Shaders/Materials/TerrainLayered.slang
struct Material_TerrainLayered : Flux_MaterialType
{
	struct Params                                        // 656 bytes, within uGEOM_MAX_MATERIAL_CONSTANTS_BYTES
	{
		// 48 texture fields, one set per palette index 0 … 15: m_xAlbedo<i> [TextureUsage("colour")], m_xNormal<i>
		// [TextureUsage("normal")], m_xORMH<i> [TextureUsage("mask")] (occlusion, roughness, metallic, height), each
		// [Optional], written out as fields m_xAlbedo0 … m_xORMH15 in the module.
		float4 m_axLayerParams[16];                     // x UV scale (repeats per metre), y height-blend depth, z porosity, w 0
		uint m_uTerrainRecord;                          // GEOM's constant (Meshes.md "Terrain data"): the record index R, written by AddTerrain
		uint m_uLayerCount;
		uint m_uLayerSampler;
		uint m_uReserved;
	};
	[Permutation(2, 4)] extern static const uint uPERM_LAYERS;
	static Flux_SurfaceData Evaluate(Flux_MaterialInput xIn, Params xParams);   // "Layer blend in the terrain material"
	static half EvaluateOpacity(Flux_MaterialInput xIn, Params xParams);         // 1; holes are GEOM's variant
};
```

- The layer UV is `xIn.m_xUV0 × m_fUVScale`, where the terrain adapter supplies `m_xUV0` = world XZ in metres with its gradients (pending GEOM), so the explicit gradients are the adapter's scaled by the same factor ([Z2-N-REND-245]); `Evaluate` samples only through `Flux_SampleMaterial`, in `half` for colour and weights and `float` for UVs ([Z2-N-REND-032]).
- The splat texels come from the terrain's `Env_TerrainGpuRecord` in the frame's `Env_FrameEnvironment` (`Zenith2/Docs/Design/ENV.md → "Render-world records"`): record `m_uTerrainRecord`, the same index GEOM's `Flux_TerrainRecord` has, tile table entry `tz × m_uTilesX + tx`. ENV's GPU side owns the records and tile tables as `Flux_PersistentReadOnly` buffers: it creates a terrain's table when the terrain asset activates, rewrites a tile's entry when the preview's replacement of one of its textures publishes or when a texture it references publishes a streaming swap (a per-frame comparison of each referenced resident texture's `GetSRV()`, at most 8 per active tile), and binds a null `m_xDetailDensity` while the density texture's `Flux_TextureResource::IsFallbackBound()` is true (loading or FAILED: the usage fallback's descriptor carries no ENV density, so the detail material reads density 0 there and draws no detail blade), and replaces the buffer by reallocate-and-swap, never in place ([Z2-N-REND-252]). How a mesh pass exposes `Env_FrameEnvironment` to materials is pending core-doc batch 4 (X146, row EN18), which gates the Stage 3 terrain material tickets.
- Weather wetness and snow reach the material through `Env_ApplySurfaceWeather` of `Zenith2/Shaders/Environment/Weather.slang` (`Zenith2/Docs/Design/ENV/WaterWeather.md → "Shared wetness function"`), called at the end of `Evaluate`.
- The terrain layer document's schema is `Zenith2/Docs/Design/ENV/Bake.md → "Terrain layer document schema"`.

### Terrain record slots

A terrain has two runtime indices; neither is ever cooked, because both depend on which terrains are live:

| Index | Assigned by | Range | Used by |
|---|---|---|---|
| the record index R | GEOM's `Flux_TerrainRenderer::AddTerrain` (the terrain's `Flux_TerrainRecord`) | GEOM's | the material's `m_uTerrainRecord`, which `AddTerrain` writes into `Flux_TerrainDesc::m_xMaterial` before any of its node instances draws (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`); `Env_FrameEnvironment::m_xTerrains` is indexed by R, so one constant names both GEOM's and ENV's record |
| the extractor slot S | `Environment.ExtractEnvironment` at the terrain's ADD record: the lowest of 0 … 255 no live terrain of the world holds; freed at its REMOVE and kept through a full-state extraction | 0 … 255 per world | detail foliage custom data bits 0–7 (the extractor cannot know R); the detail material reads `Env_FrameEnvironment::m_xTerrainSlots[S]` = R |

On a terrain ADD, `Flux_EnvironmentRenderer::ApplyRecords` calls `AddTerrain` and, right after it returns successfully, on the same build node and before any other record is applied, reads R with `Flux_TerrainRenderer::GetRecordIndex` (`Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`; R is below `uGEOM_MAX_TERRAINS`, and constant until `RemoveTerrain`), writes the terrain's `Env_TerrainGpuRecord` at index R of the record buffer and R at entry S of the slot table (both `Flux_PersistentReadOnly` buffers replaced by reallocate-and-swap, grown to the largest R + 1); on REMOVE it writes 0xFFFFFFFF at entry S, clears record R and calls `RemoveTerrain`. A world with 256 live terrains emits no further ADD (`environment.terrain.slots_full`). The material's `m_uTerrainRecord` is one constant per material, so the same terrain asset live in two worlds of one renderer at once would need two values: the second ADD whose material already backs a live terrain is refused by ENV's GPU side, counted in `Flux.Environment.TerrainMaterialShared` and logged once, and that world draws no terrain (GEOM's `AddTerrain` refuses it as well, step 4 of `Zenith2/Docs/Design/GEOM/Meshes.md → "Terrain record constant"`). `Test_EnvRender_TwoTerrainsRecordsEitherOrder` asserts that two terrains activated in either order, and one removed and re-added, each draw with their own heights and splat and that each detail instance reads its own terrain's density.

### VT-backed terrain materials

Plan §5.4 and [Z2-N-REND-944] require the physical-accuracy suite to include VT-backed terrain materials, and no ADVGFX design document exists yet, so ENV owns that reference scene. A VT-backed terrain material is `Material_TerrainLayered` whose layer texture fields bind virtual textures: GEOM's boundary lets a material texture field bind either a streamed or a virtual texture, chosen by the import preset's `virtual` flag (`Zenith2/Docs/Design/GEOM/Textures.md → "Virtual texturing boundary"`), so the material needs no VT permutation of its own, and the splat tile textures stay streamed.

<!-- assumption: pending ADVGFX -->
`Flux_SampleMaterial` resolves a virtual texture field through ADVGFX's page table and returns the same filtered value within ADVGFX's VT error tolerance, and the path tracer (`ZenithPathTracer`, ADVGFX's) evaluates `Material_TerrainLayered` from the same material constants over a BVH built from the terrain's LOD-0 triangles (`Zenith_ExtractTerrainCollision`'s tiles, "Triangle interpolation"). Recorded as cross-area row EN19.
<!-- /assumption -->

The scene `Zenith2/Engine/ZenithFlux/Tests/Environment/Scenes/TerrainVT/` is a 256 m fixture terrain (2 × 2 tiles, gentle relief under 20 m) with four layers whose textures are imported with `virtual: true`, one analytic sun with the atmosphere disabled, no fog, no clouds and every screen-space approximation off (Z2-D-037); `Test_EnvRender_TerrainVTReference` (Stage 5) renders it with the raster path and compares it with the converged path-traced reference within `Z2-B-ADVGFX-008` Physical-accuracy error tolerance per reference scene.
