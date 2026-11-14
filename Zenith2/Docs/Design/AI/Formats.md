# AI design: formats

Section file of `Zenith2/Docs/Design/AI.md`. It specifies every byte layout and document schema AI owns: the navigation file `.znav` (kind 15) and its sections, the navigation source intermediate ENV's importer writes, the tile bake intermediate, the navigation settings and navigation authoring documents, the command payloads, the script request and result payloads, the save section, and the parser limits.

## Common rules

- Binary formats are little-endian and written and read field by field; no reader or writer copies a C++ object's memory and no file holds a pointer, a slot number, an interned string ID or a `Zenith_NavPolyRef` (Z2-D-038).
- Floats are IEEE-754 binary32 stored by bit pattern; `double` origins binary64.
- Every reserved field is zero when written and refused when non-zero (`AI_ERROR_TILE_CORRUPT`, `AI_ERROR_VOLUME_CORRUPT`, `AI_ERROR_SAVE_CORRUPT` per format).
- Every count, offset and size is checked against the containing buffer with overflow-checked arithmetic before use ([Z2-N-ARCH-951]); a **check** is `Zenith_Hash64` truncated to its low 32 bits per ASSET's rule (`Zenith2/Docs/Design/ASSET/Formats.md` → "Common rules"). Every check field is 32 bits of value; where a field is 8 bytes wide (INFO's content check, the save section's tile and carve checks, `Zenith_NavTileInfo::m_ulContentHash`) the check is stored zero-extended and a reader refuses non-zero upper 4 bytes as a reserved-field violation.
- JSON documents are written by `Zenith_JsonWriter` and read under `Zenith_JsonLimits` with duplicate keys refused; persistent IDs and paths follow [Z2-N-CONTRACT-067].

## Navigation file

`.znav` files use ASSET's cooked container unchanged (`Zenith2/Docs/Design/ASSET/Formats.md` → "Cooked asset file"): magic `ZNAV` (`5A 4E 41 56`), format version `uAI_NAV_FORMAT_VERSION` (1), kind 15. The sections present depend on the role in the INFO section.

### Section kinds

| Kind | Name | Roles | Index | Stored | Resident as |
|---|---|---|---|---|---|
| 0x0F01 | INFO | every role | 0 | uncompressed | decoded |
| 0x0F02 | CLASSES | SETTINGS | 0 | uncompressed | decoded |
| 0x0F03 | AREAS | SETTINGS | 0 | uncompressed | decoded |
| 0x0F04 | PROFILES | SETTINGS | 0 | uncompressed | decoded |
| 0x0F10 | TILE | TILE | 0 | the cook's codec (zstd level 9 by default) | decoded at load |
| 0x0F11 | LAYERS | TILE when carve-capable | 0 | zstd level 9 | stored (compressed) bytes; decoded into the rebuild arena per carve |
| 0x0F20 | MANIFEST | MANIFEST | 0 | uncompressed | decoded |
| 0x0F30 | VOLUME | VOLUME | 0 | zstd level 9 | decoded at load |
| 0x0F40 | SOURCE_GEOMETRY | SOURCE_GEOMETRY | 0 | zstd level 9 | never loaded by a world |

A section kind a role does not list, or a missing required kind, is `AI_ERROR_TILE_CORRUPT` at load (`m_uDetail` = the kind).

### Info section

64 bytes:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 1 | role | `Zenith_NavFileRole` |
| 1 | 3 | reserved | 0 |
| 4 | 4 | flags | bit 0: the TILE carries a LAYERS section; other bits 0 |
| 8 | 16 | settings ID | the SETTINGS asset's persistent ID; zero in a SETTINGS file |
| 24 | 8 | settings hash | `Zenith_Hash64` over the SETTINGS file's CLASSES, AREAS and PROFILES section bytes in that order; a SETTINGS file stores its own |
| 32 | 8 | tile key | `Zenith_NavTileKey::Pack()` (TILE); the class index in bits 56–63 (VOLUME); 0 otherwise |
| 40 | 8 | content check | check over the role's principal section's decoded bytes, zero-extended ("Common rules") |
| 48 | 16 | reserved | 0 |

### Agent class object

Binary, CLASSES section: `uint32 count` (1 … 6, ground classes first) then `count` records of 128 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 32 | name, UTF-8, NUL-padded, `^[A-Za-z][A-Za-z0-9_]{0,30}$` |
| 32 | 8 | name hash (`Zenith_Hash64` of the name bytes) |
| 40 | 1 | medium (`Zenith_NavMedium`) |
| 41 | 1 | partition (`Zenith_NavPartition`; 0 for volume classes) |
| 42 | 1 | flags: bit 0 amphibious (volume classes) |
| 43 | 1 | reserved |
| 44 | 4 × 13 | agent radius, agent height, max climb, max slope (radians), cell size, cell height, tile size, edge max length, edge max error, detail sample distance, detail sample max error, region min area (as `uint32`), region merge area (`uint32`) |
| 96 | 32 | reserved |

JSON, in the settings document's `classes` array:

| Key | Type | Default | Range |
|---|---|---|---|
| `name` | string | — (required) | the binary rule |
| `medium` | `"ground"`, `"flying"`, `"underwater"` | `"ground"` | |
| `radius` | number | 0.35 | (0, 8] |
| `height` | number | 1.8 | (0, 16] |
| `maxClimb` | number | 0.35 | [0, height] (ground) |
| `maxSlopeDegrees` | number | 45 | [0, 89] (ground) |
| `cellSize` | number | 0.25 (ground), 0.5 (volume) | [0.05, 1] ground; [0.25, 4] volume |
| `cellHeight` | number | 0.1 | [0.02, 0.5] (ground) |
| `tileSize` | number | 32 | one of 16, 32, 64, 128 (ground) |
| `regionMinArea`, `regionMergeArea` | integer | 8, 20 | [0, 65,535] |
| `edgeMaxLength` | number | 12 | [0, 128] |
| `edgeMaxError` | number | 1.3 | [0.1, 3] |
| `detailSampleDistance` | number | 6 | 0 or [1, 32] |
| `detailSampleMaxError` | number | 1 | [0, 16] |
| `partition` | `"watershed"`, `"monotone"`, `"layers"` | `"watershed"` | ground |
| `amphibious` | boolean | false | volume `"underwater"` classes |

At most 4 ground and 2 volume classes; a fifth ground class or a third volume class is `AI_ERROR_DOCUMENT_SCHEMA`.

### Area table

Binary, AREAS section: 64 records of 48 bytes, indexed by area code:

| Offset | Size | Field |
|---|---|---|
| 0 | 32 | name (codes 0 … 4 the fixed names `ground`, `road`, `offPath`, `unwalkable`, `water`; unused codes all zero) |
| 32 | 2 | flags (`Zenith_NavPolyFlags`: WALK, SWIM, DOOR, JUMP) |
| 34 | 2 | reserved |
| 36 | 4 | default cost (≥ 1.0, or +∞) |
| 40 | 8 | reserved |

JSON `areas`: objects `{ "name": string, "code": 5 … 62, "flags": [ "walk" | "swim" | "door" | "jump" ], "cost": number ≥ 1 | "inf" }`; a duplicate name or code, a fixed name, or a code outside 5 … 62 is `AI_ERROR_DOCUMENT_SCHEMA`.

### Cost profiles

Binary, PROFILES section: `uint32 count` (1 … 8; profile 0 is always the area defaults, named `default`) then `count` records of 304 bytes: name (32), 64 × f32 costs (256), include flags `u16`, exclude flags `u16`, 12 reserved.

JSON `profiles`: `{ "name": string, "costs": { "<area name>": number | "inf" }, "include": [flag names], "exclude": [flag names] }`; unnamed areas take the default cost.

### Tile section

Header, 112 bytes:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 8 | tile key | `Pack()`; must equal the INFO section's |
| 8 | 24 | absolute origin | three `double`: the tile's minimum corner without border; must equal (tx × tileSize, minY, tz × tileSize) to 1e−6 m |
| 32 | 4 | cell size | equal to the class's |
| 36 | 4 | cell height | equal to the class's |
| 40 | 4 | min y | tile-local, the quantisation base |
| 44 | 4 | max y | |
| 48 | 4 | vertex count V | ≤ 16,384 |
| 52 | 4 | polygon count P | ≤ 4,096 |
| 56 | 4 | detail vertex count DV | ≤ 520,192 (`uAI_NAV_MAX_DETAIL_VERTS_PER_POLY` 127 × P at P ≤ 4,096) |
| 60 | 4 | detail triangle count DT | ≤ 1,044,480 (`uAI_NAV_MAX_DETAIL_TRIS_PER_POLY` 255 × P) |
| 64 | 4 | BV node count B | = 2 P − 1, or 0 when P = 0 |
| 68 | 4 | portal count | portal edges on the tile sides |
| 72 | 8 | area table hash | the settings' AREAS section check |
| 80 | 32 | reserved | 0 |

Then, each array 16-byte aligned (zero padding):

| Array | Element | Size |
|---|---|---|
| vertices | x, y, z quantised `uint16` (x, z in cells from the tile minimum, ≤ tileSize / cellSize; y in cell heights from min y) | 6 × V |
| polygons | 6 vertex indices `uint16` (unused 0xFFFF), 6 neighbours `uint16` (0 = wall; 1 … 4,096 = internal polygon index + 1; 0x8000 \| side for a portal on side 0 … 3), vertex count `uint8` (3 … 6), area `uint8` (0 … 62), flags `uint16`, detail vertex base `uint32`, detail triangle base `uint32`, detail vertex count `uint8`, detail triangle count `uint8`, reserved `uint16` | 40 × P |
| detail vertices | x, y, z quantised as vertices, the polygon's extra detail points | 6 × DV |
| detail triangles | three local indices `uint8` (an index i below the polygon's vertex count n names its vertex i; an index i ≥ n names its detail vertex i − n, so every index is < n + the polygon's detail vertex count ≤ 6 + 127 = 133), edge flags `uint8` | 4 × DT |
| BV nodes | min `uint16` × 3, max `uint16` × 3 (quantised like vertices), index `sint32` (≥ 0 polygon index of a leaf; < 0 the escape offset of an internal node) | 16 × B |

Checks at load (T1 … T9), each `AI_ERROR_TILE_CORRUPT` with `m_uDetail` = the check number:

| # | Check |
|---|---|
| T1 | the decoded size equals the header's arrays' sum with alignment |
| T2 | counts within the limits above |
| T3 | every vertex index < V (or 0xFFFF after the vertex count) |
| T4 | every internal neighbour < P + 1 and the neighbour names this polygon back on the shared edge; every portal side ∈ 0 … 3 and the edge lies on that tile side |
| T5 | every area ≤ 62; flags contain only defined bits |
| T6 | every polygon convex (consecutive edge cross products ≥ −1e−3 in quantised units) and of non-zero area |
| T7 | every polygon's detail vertex count ≤ 127 and detail triangle count ≤ 255 (and ≥ 1 when the polygon has a detail mesh); detail bases plus counts within DV and DT; every detail index < the polygon's vertex count + its detail vertex count |
| T8 | the BV tree's leaf set is every polygon exactly once and every escape offset in range |
| T9 | the INFO section's content check equals the check over the decoded section |

### Layers section

Decoded layout: header 32 bytes —

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | W (`uint16`, ≤ 2,048) |
| 2 | 2 | H (`uint16`, ≤ 2,048) |
| 4 | 2 | border cells (`uint16`) |
| 6 | 2 | reserved, 0 |
| 8 | 4 | span count S (`uint32`, ≤ 16,777,216) |
| 12 | 4 | area volume count A (`uint32`, ≤ 256) |
| 16 | 16 | reserved, 0 |

— then W × H cells of 6 bytes (`uint32` first span index, `uint16` span count), then S spans of 8 bytes (`uint16` floor y in cell heights, `uint8` clearance in cell heights saturated at 255, `uint8` area, `uint32` connections: 4 × 6 bits, 63 = none, the neighbour column's span offset), then A `Zenith_NavAreaVolume` records of 48 bytes (centre, half extents, rotation as 4 floats, area, priority, ordinal, padding). The decoded section is the stage-4 compact heightfield before stage 5; checks L1 (sizes), L2 (connections in range), L3 (area ≤ 62 or 63), failure `AI_ERROR_TILE_CORRUPT` with detail 100 + check number.

### Manifest section

`uint32` count (≤ 1,048,576) then 32-byte entries sorted by (role, class, tile key): asset GUID (16), role (1), class index (1), reserved (6), tile key or volume region index (8). Each entry's asset is a hard dependency of the manifest file (`DeclareReference` with `ASSET_DEPENDENCY_FLAG_HARD`), so loading the manifest loads every tile and volume it names.

### Volume section

Header 96 bytes —

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | class index `uint8` |
| 1 | 1 | levels `uint8` (1 … 10) |
| 2 | 2 | reserved, 0 |
| 4 | 4 | leaf edge `float` |
| 8 | 24 | region minimum, three `double` (absolute metres) |
| 32 | 4 | region edge `float` |
| 36 | 40 | node counts per level, 10 × `uint32` (unused levels 0) |
| 76 | 4 | total nodes `uint32` (≤ `uAI_VOLUME_MAX_NODES`) |
| 80 | 16 | reserved, 0 |

— then nodes of 48 bytes in level order, Morton order within a level: solid mask `uint64`, water mask `uint64` (level 0 only; a coarser node stores 0 and its medium tag), medium tag `uint8` (0 mixed, 1 air, 2 water, 3 solid), level `uint8`, reserved `uint16`, first child index `uint32` (0xFFFFFFFF none), six face links `uint32` (level 4 bits, index 28 bits; 0xFFFFFFFF none). Checks V1 (counts ≤ `uAI_VOLUME_MAX_NODES`, sizes), V2 (child and link indices in range), V3 (a level-0 node's water mask has no bit set that is set in its solid mask); failure `AI_ERROR_VOLUME_CORRUPT`.

### Source geometry section

The navigation source intermediate's bytes (below) stored compressed. The cook step publishes the same bytes as the property `ai.navSource`, which tile bakes read with `ReadDependencyOutput`. A world never loads a SOURCE_GEOMETRY file: no runtime component can name one (the tile add procedure refuses the role, counted `ai.nav.wrong_role`).

<!-- assumption: pending ASSET, PACK -->
SOURCE_GEOMETRY files are cook inputs of other cook items; AI asks ASSET (cross-area row AI2) for a catalogue flag or packaging rule that keeps them out of packaged mounts, since nothing at run time reads them.
<!-- /assumption -->

## Navigation source intermediate

`ZNSG`, written by ENV's importer stage `Environment.Navigation` for each `navigation:<x>_<z>` product through AI's tool-side writer `Zenith_AIWriteNavSourceIntermediate(const Zenith_NavigationGeometry& xGeometry, const double adTileOrigin[3], Zenith_Arena& xArena) → Zenith_ByteSpan` (`Zenith2/Tools/Authoring/Zenith_AuthoringAI.h`), the PHYS precedent of ENV calling the consumer's writer (`Zenith2/Docs/Design/ENV/Bake.md` → "Navigation geometry extraction"). Blob name `ai.navSource`.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `ZNSG` |
| 4 | 2 | version 1 |
| 6 | 2 | reserved |
| 8 | 4 | ENV tile x (`sint32`) |
| 12 | 4 | ENV tile z (`sint32`) |
| 16 | 24 | absolute origin of the ENV tile (three `double`) |
| 40 | 4 | vertex count V (≤ 263,169 = 513²) |
| 44 | 4 | triangle count T (≤ 524,288) |
| 48 | 4 | check over the bytes from offset 64 |
| 52 | 12 | reserved |
| 64 | 12 V | vertices, tile-local `float` x, y, z |
| … | 12 T | indices `uint32` × 3 |
| … | T | area codes (`Zenith_NavigationArea`, 0 … 4) |

A reader refuses a wrong magic, version, size, an index ≥ V or an area > 4 with `AI_ERROR_SOURCE_GEOMETRY_CORRUPT`.

## Tile bake intermediate

Blob `ai.navTileJob`, written by AI's importer `ai.navigation` for each `navTile:<class>/<tileSize>/<tx>_<tz>_<layer>` and `navVolume:<class>/<index>/<xmm>_<ymm>_<zmm>` product; header 112 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `ZNTJ` |
| 4 | 2 | version 1 |
| 6 | 1 | product role (2 TILE, 4 VOLUME) |
| 7 | 1 | flags: bit 0 keep layers (the tile intersects a `carveRegions` box) |
| 8 | 8 | tile key `Pack()` (TILE) or class index and region index (VOLUME) |
| 16 | 16 | settings GUID |
| 32 | 48 | bounds: absolute minimum and maximum (six `double`) including the border |
| 80 | 4 | scene count S (≤ 4,096) |
| 84 | 4 | source geometry count G (≤ 4,096) |
| 88 | 4 | water volume count W (≤ 256) |
| 92 | 4 | reserved, 0 |
| 96 | 16 | manifest GUID: the navigation document's MANIFEST product, whose `ai.navMappings` this job reads |
| 112 | 48 S | scene entries sorted by GUID (a GUID twice is `AI_ERROR_DOCUMENT_SCHEMA` at import): GUID (16); form `uint8` (1 source document, read with `OpenSourceDocument`; 2 cooked scene product, read as its cooked output, Navigation.md "Bake inputs"); reserved (7); offset, three `double` (absolute metres added to every entity position of a form-2 scene; zero for form 1) |
| … | 16 G | `navigation:<x>_<z>` product GUIDs overlapping the bounds, sorted |
| … | 32 W | water boxes: minimum and maximum `float` × 6, surface height `float`, 4 reserved |

The navigation document's mapping tables (`areaByMaterial`, `blockerLayers`, `ignoredLayers`) are written once into the MANIFEST product's intermediate `ai.navMappings`; the MANIFEST step publishes the same bytes as its property `ai.navMappings`, and each tile or volume step reads them with `ReadDependencyOutput(manifest GUID at offset 96, "ai.navMappings")`, which records the cook-input edge and puts the mappings' key hash in the tile's key. Offset 16 is always the settings GUID.

## Navigation settings document

`Games/<Name>/Assets/Navigation/<Name>.navsettings.json`, document type `Zenith.NavigationSettingsDocument`, schema version 1, imported by the importer `ai.navSettings` (intermediate blob `ai.navSettings`: the validated document's CLASSES, AREAS and PROFILES section bytes) and cooked by the kind-15 step `ai.navMesh` into a SETTINGS file. One per game is expected; two settings documents are allowed (for example a test map), and a world accepts only one ("Agent classes" in `Zenith2/Docs/Design/AI/Navigation.md`).

```json
{
	"schemaVersion": 1,
	"classes": [
		{ "name": "Humanoid", "radius": 0.35, "height": 1.8, "maxClimb": 0.35, "maxSlopeDegrees": 45 },
		{ "name": "Large", "radius": 0.9, "height": 2.6, "maxClimb": 0.5, "maxSlopeDegrees": 35, "cellSize": 0.3 },
		{ "name": "Bird", "medium": "flying", "radius": 0.3, "cellSize": 0.5 }
	],
	"areas": [ { "name": "Door", "code": 5, "flags": [ "walk", "door" ], "cost": 1.0 } ],
	"profiles": [ { "name": "Swimmer", "costs": { "water": 1.0 }, "include": [ "walk", "swim" ] } ]
}
```

Unknown members are preserved on round-trip with a warning (Z2-D-048); schema errors are `AI_ERROR_DOCUMENT_SCHEMA` cook diagnostics naming the member path.

## Navigation document schema

`*.navigation.json`, document type `Zenith.NavigationDocument`, schema version 1, imported by `ai.navigation`:

| Key | Type | Required | Meaning |
|---|---|---|---|
| `schemaVersion` | 1 | yes | |
| `settings` | path | yes | the settings document (a hard reference) |
| `classes` | array of class names | no (default: every ground and volume class) | the classes this document bakes |
| `bounds` | `{ "min": [x, y, z], "max": [x, y, z] }` absolute metres | yes | the baked region; tiles whose cell intersects it are declared |
| `scenes` | array; each element a scene path (a source document, positions absolute) or a **product entry** `{ "scene": <GUID>, "offset": [x, y, z] }` (a cooked scene product, positions relative to `offset`, absolute metres in `double`; WORLD's derived documents, row WD23) | yes (may be empty) | the scenes whose static colliders, area volumes and links are inputs (Navigation.md "Bake inputs") |
| `terrains` | array of terrain document paths | no | terrains whose `navigation:<x>_<z>` products are inputs |
| `blockerLayers`, `ignoredLayers` | arrays of physics layer names | no | "Bake inputs" in Navigation.md |
| `areaByMaterial` | object: physics material path → area name | no | the area of collider triangles by material |
| `carveRegions` | array of `{ "min", "max" }` | no | tiles intersecting a region keep layers (carve-capable) |
| `volumes` | array of `{ "class", "min", "max", "index"? }` | no | volume regions (Volumes.md), edges powers of two multiples of 4 × leaf edge; `index` (0 … 255, unique within the document) is the region's index in its product key, defaulting to the element's array position (WORLD writes the index of the element in its world document, row WD23) |
| `waterVolumes` | array of `{ "min", "max", "surfaceHeight" }` | no | explicit water prisms for volume bakes |

The product declaration is one tool-side function, `Zenith_AIDeclareNavigationProducts` (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header"), which AI's importer `ai.navigation` calls with the manifest key `navigation` (its primary product) and WORLD's world importer calls in process with `navigation/<x>_<z>` (non-primary) for each derived document (row WD23). It declares, in key order: the MANIFEST product under the given manifest key; per baked class and per tile whose coarse occupancy pre-pass (a 4 m column test of the inputs' AABBs) is non-empty, `navTile:<class>/<tileSize>/<tx>_<tz>_<layer>` (the class's tile size in whole metres, 16, 32, 64 or 128; tile coordinates as signed decimal integers; layer 0, or 0 … k for LAYERS partitions, k from the pre-pass's height clusters); per volume region `navVolume:<class>/<index>/<xmm>_<ymm>_<zmm>` (the region's `index` member, or its position in the document's `volumes` array when absent, then its absolute minimum corner as signed decimal integers of millimetres, `floor(v × 1000)` per axis). Both forms carry what the cell contribution needs to place the product in a cell (a tile's minimum corner is `(tx × tileSize, tz × tileSize)`, a volume's is its encoded corner), so `Zenith_AIMakeNavigationManifestView` reads only product keys (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header"); a key that does not parse is `AI_ERROR_DOCUMENT_SCHEMA` there. Examples: `navTile:Humanoid/32/12_-4_0`, `navVolume:Bird/0/-128000_0_256000`. Renaming a product key on reimport uses `RenameProduct` so GUIDs survive ([Z2-N-CONTRACT-543] for identities).

## Command payloads

Every payload is a registered `TYPE_KIND_STRUCT` (`Zenith_CommandKindDesc::m_xPayloadType`), allowed in phases 1 … 10 (`m_uAllowedPhaseMask` = 0x7FE):

| Kind | Payload type | Bytes | Layout |
|---|---|---|---|
| `AI.Agent` | `Zenith.AI.AgentCommandPayload` | 48 | 0 op `uint8` (1 MOVE_TO, 2 STOP, 3 PAUSE, 4 RESUME, 5 SET_SPEED_SCALE, 6 SET_MEDIUM); 1 medium `uint8`; 2 reserved `uint16`; 4 reserved `uint32`; 8 target (12); 20 reserved `uint32`; 24 target entity (8); 32 accept radius `float`; 36 speed scale `float`; 40 reserved (8). The agent is the command's target entity (the record's own entity field), so the payload names no agent; every field is at its natural alignment |
| `AI.Noise` | `Zenith.AI.NoisePayload` | 32 | position (12), loudness `float`, instigator (8), team `uint8` (0xFF: from the instigator), reserved 7 |
| `AI.Stimulus` | `Zenith.AI.StimulusPayload` | 24 | perceiver (8), instigator (8), sense `uint8` (DAMAGE, TOUCH), reserved 7 |
| `AI.Blackboard` | `Zenith.AI.BlackboardPayload` | 192 | 0 op `uint8` (1 SET, 2 CLEAR, 3 CLEAR_ALL), reserved 7; 8 owner (8); 16 key length `uint8`; 17 key bytes (63); 80 value kind `uint8` (1 BOOL, 2 NUMBER, 3 VECTOR, 4 ENTITY, 5 NAME, 6 POSITION), reserved 7; 88 number `double`; 96 vector (12, kinds 3 and 6); 108 reserved 4; 112 entity (8); 120 name length `uint8`; 121 name bytes (63); 184 expiry tick `uint64` (0 = never) |
| `AI.OffMeshLink` | `Zenith.AI.LinkPayload` | 16 | op `uint8` (1 COMPLETE on the agent, 2 ENABLE, 3 DISABLE on the link), reserved 7; entity (8) |
| `AI.Perception` | `Zenith.AI.PerceptionPayload` | 24 | op `uint8` (1 FORGET, 2 FORCE_AWARE), reserved 7; perceiver (8); target (8) |

<!-- elaboration -->
The blackboard payload carries key and name values as text, not interned IDs, because interned IDs are per session while the payload type is a serialized, registered struct (a command buffer saved with its entity transaction, a recorded scenario); playback interns them through the world's string table, which consumes Z2-B-SCRIPT-022 and can refuse ("Blackboard commands" of `Zenith2/Docs/Design/AI/Agents.md`).

## Script request payload

64 bytes, the `SIDE_EFFECT_KIND_ASYNC_REQUEST` record body of `AI.RequestPath`:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | source kind: `ASYNC_SOURCE_PATHFINDING` (the multiplexer routes by it) |
| 1 | 1 | request kind: 1 navmesh, 2 volume |
| 2 | 1 | class index (0xFF unresolved) |
| 3 | 1 | cost profile index |
| 4 | 1 | max corners (1 … 32) |
| 5 | 1 | medium (`Zenith_NavMedium`) |
| 6 | 2 | reserved |
| 8 | 12 | from |
| 20 | 12 | to |
| 32 | 12 | extents |
| 44 | 4 | reserved |
| 48 | 8 | requester simulation ID |
| 56 | 8 | reserved |

## Script result payloads

`uAI_PATH_RESULT_BYTES` = 416 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | status (`Zenith_NavPathStatus`) |
| 1 | 1 | corner count C (0 … 32) |
| 2 | 1 | limit (`Zenith_NavPathLimit`, Navigation.md "Partial results") |
| 3 | 1 | reserved |
| 4 | 4 | length (metres) |
| 8 | 4 | cost |
| 12 | 4 | expansions |
| 16 | 12 | final point |
| 28 | 4 | off-mesh mask: bit i set when corner i starts a link |
| 32 | 12 × 32 | corners (positions), the first C meaningful, the rest zero |

The payload is a pure function of the request and the navmesh content (RECOMPUTED), so the replay log carries no payload for it ([Z2-N-SCRIPT-336]).

## Save section layout

Participant `Zenith.AI.Navigation`, section version 1; written field by field through `Zenith_SaveSectionWriter`, integers little-endian, floats by bit pattern, entity references in SAVE's 16-byte form (`WriteEntityReference`), simulation IDs as `uint64` (SAVE restores every entity's simulation ID). A "tile index" is an index into list 1, a "volume index" into list 2; 0xFFFFFFFF is none. Every position is world-local in the frame of the `Zenith.WorldOrigin` SAVE restores with the world, except the carved records' obstacle shapes (list 6) and the link registrations' snapped ends (list 8), which are tile-local. A **node reference entry** (16 bytes) is the save form of a `Zenith_NavPolyRef` in either form: 0 kind `uint8` (1 polygon, 2 link node, 3 dead link node; 0 none, the rest zero; kind 3 is written for every link-node reference that does not resolve at capture, in a corridor entry or a corner reference, with every other field zero, and is restored as `Zenith_NavPolyRef::MakeLinkNode(0, 0)`, Navigation.md "Search graph"); 1 class `uint8` (link nodes; 0 otherwise); 2 reserved `uint16`; 4 tile index `uint32` (0xFFFFFFFF for a link node); 8 `uint64`: the polygon index (polygon) or the link entity simulation ID (link node). Thirteen lists follow the header, in this order (`Zenith2/Docs/Design/AI/Agents.md` → "Save and restore algorithm"):

**Header**, 112 bytes: 0 magic `ZAIS` (4); 4 version `uint16` 1; 6 reserved `uint16`; 8 settings ID (16); 24 settings hash (8); 32 the counts of lists 1 … 13 (13 × `uint32`); 84 applied rebase count `uint32` (`m_uAppliedRebaseCount`); 88 applied origin, three `double` (`m_adAppliedOrigin`, Navigation.md "Origin shift", rebase bookkeeping).

| # | List | Order | Record |
|---|---|---|---|
| 1 | Tiles | tile key | 48 bytes: 0 key `Pack()` (8); 8 asset GUID (16); 24 supplier simulation ID (8); 32 content check (8, zero-extended); 40 version `uint32`; 44 flags `uint32` (bit 0 carve-capable, bit 1 carved, bits 8–15 the supplier slot 0 … 15; other bits 0) |
| 2 | Volumes | (class, asset GUID) | 64 bytes: 0 asset GUID (16); 16 supplier simulation ID (8); 24 class `uint8`, supplier slot `uint8`, reserved (6); 32 region minimum, three `double` (24); 56 content check (8) |
| 3 | Navigation load records | completion ID | 40 bytes: 0 completion ID (8); 8 asset GUID (16); 24 requester simulation ID (8); 32 kind `uint8` (1 MANIFEST, 2 TILE, 3 VOLUME); 33 state `uint8` (0 ISSUED, 1 PARKED_OK, 2 PARKED_FAILED, 3 ABANDONED); 34 reserved (6) |
| 4 | Supplier records | simulation ID | 16-byte head: supplier simulation ID (8), component kind `uint8` (1 manifest, 2 tiles, 3 volume), slot count `uint8` (1 for kinds 1 and 3, 0 … 16 for kind 2), reserved (6); then slot count × 24 bytes in slot order: asset GUID (16, zero for a null handle), state `uint8` (0 NEED, 1 IN_FLIGHT, 2 JOINED, 3 FAILED; 0 for a zero GUID), reserved (7) (Navigation.md "Supplier records") |
| 5 | Carve queue | queue order | 16 bytes: tile key (8), first-queued tick (8) |
| 6 | Carved tiles | tile key | 24-byte head: tile key (8), version `uint32`, obstacle count O `uint16`, reserved `uint16`, rebuilt-section check (8, zero-extended); then O obstacle-set entries of 40 bytes in simulation-ID order, each the exact `Zenith_NavObstacleShape` the rebuild was given: simulation ID (8), shape `uint8`, area `uint8`, reserved (2), tile-local centre (12), half extents (12), yaw `float` (4) |
| 7 | Obstacle applied records | simulation ID | 56 bytes: entity reference (16); applied position (12); applied yaw (4); half extents (12); move threshold (4); shape, mode, area, enabled (4 × `uint8`); reserved (4) |
| 8 | Link registrations | (link simulation ID, class) | 72 bytes: link entity reference (16); class `uint8`, flags `uint8` (bit 0 bidirectional), reserved (2); start tile index `uint32`, start polygon `uint32`, start tile-local position (12); end tile index, end polygon, end tile-local position (4 + 4 + 12); registered entity position (12) |
| 9 | Agents | simulation ID | a 416-byte fixed part (below), then the corridor (corridor count × 16: one node reference entry per entry, polygons and link nodes) and the volume corners (count × 12) |
| 10 | Path entries | inbox, then queue, then active set, each in its order | a 104-byte head (below), then for a begun ACTIVE search its search state (below) |
| 11 | Perception ledger | perceiver simulation ID | 20-byte head: perceiver reference (16), entry count E `uint8`, pending-forget count F `uint8`, reserved (2); then E × 12 bytes (target simulation ID (8), emitted level `uint8`, reserved (3)) and F × 20 bytes (target reference (16), reason `uint8`, reserved (3)) |
| 12 | Blackboards | (owner simulation ID, key text bytes) | only records unexpired at the capture tick t (expiry 0 or > t): owner reference (16); key length `uint8` and text; value kind `uint8` (1 … 6); number `double` (8); vector (12, kinds 3 and 6); entity reference (16); name length `uint8` and text; expiry tick (8) |
| 13 | Noises | buffer order | consume tick (8), kind `uint8`, position (12), loudness `float`, instigator reference (16), perceiver reference (16), team `uint8`, reserved (2) |

**Agent fixed part** (416 bytes; the record fields of `Zenith2/Docs/Design/AI/Internals.md` → "Agent runtime record", every proposed field excluded because each is resolved by its merge function before phase 11): 0 entity reference (16); 16 class, medium, plan state, link state (4 × `uint8`); 20 paused-from status, flags, strikes, pending completion count (4 × `uint8`); 24 handled serial `uint32`; 28 emitted completed serial `uint32`; 32 current polygon (tile index, polygon: 2 × `uint32`); 40 random counter (8); 48 target (12); 60 corner count `uint32`; 64 target polygon (2 × `uint32`); 72 suspended-since tick (8); 80 four corners of 32 bytes (position 12, flags `uint8`, reserved 3, the corner's reference as a node reference entry 16: a link node for an OFFMESH_START corner); 208 link entity reference (16); 224 link start (12); 236 link end (12); 248 elapsed ticks `uint32`; 252 duration ticks `uint32`; 256 link kind `uint8`, reserved (3); 260 stuck start (12); 272 stuck intended displacement `float`; 276 stuck start tick (8); 284 next optimise tick (8); 292 next replan tick (8); 300 last desired velocity (12); 312 last yaw `float`; 316 last input flags `uint32`; 320 last written mask `uint32`; 324 last applied velocity (12); 336 last root-motion yaw `float`; 340 eight pending completions of 8 bytes (serial `uint32`, result `uint8`, reserved 3); 404 corridor count `uint16`; 406 volume corner count `uint16`; 408 continuation distance `float` (Agents.md "Path corridor", continuation); 412 continuing flag `uint8`; 413 plan reason `uint8` (0 NEW_MOVE, 1 CONTINUATION, 2 RETARGET, 3 REPLAN; Agents.md "Agent state machine"), reserved (2).

**Path entry head** (104 bytes): 0 kind `uint8` (1 agent plan, 2 script request, 3 agent volume plan, 4 script volume request); 1 state `uint8` (0 INBOX, 1 QUEUED, 2 ACTIVE); 2 flags `uint8` (bit 0 restart-pending); 3 reserved; 4 owner (16: the agent's entity reference, or the completion ID in bytes 0 … 7 and zero); 20 descriptor (64, the form of "Script request payload", which agent plans use too with the agent's simulation ID as requester); 84 enqueue tick (8); 92 expansions `uint32`; 96 ticks active `uint32`; 100 search present `uint8` (1 for a begun ACTIVE entry), reserved (3).

**Search state**: a 72-byte head — 0 search kind `uint8` (1 navmesh, 2 volume), state, status, flags (bit 0 end polygon found, bit 1 pool exhaustion noted) (`uint8` each); 4 start: tile or volume index `uint32`, polygon or node index `uint32`, level `uint8`, voxel `uint8` (0xFF whole node), reserved (2), position (12); 28 end, the same 24-byte form; 52 best node `uint32`; 56 node count N `uint32`; 60 open count H `uint32`; 64 stamp count S `uint32`; 68 reserved (4) — then N node records of 40 bytes in node-array order, H open-heap entries (`uint32` node indices) in heap-array order, and S stamps of 8 bytes: for a navmesh search a tile stamp (tile index `uint32`, version `uint32`), for a volume search a volume stamp (volume index `uint32`, reserved `uint32`, Volumes.md "Volume search" step 6). A node record: 0 kind `uint8` (1 polygon, 2 link node, 3 volume unit, 4 dead link node: a link node whose reference does not resolve at capture, every field but flags, position, g and parent zero, restored as `MakeLinkNode(0, 0)`; the record kind 3 was already the volume unit's, so the dead form is 4 here and 3 in the node reference entry); 1 flags `uint8` (bit 0 open, bit 1 closed); 2 class `uint8` (link nodes), level `uint8` (volume units; 0 otherwise); 4 tile or volume index `uint32` (0xFFFFFFFF for a link node); 8 `uint64`: the polygon index (polygon), the link entity simulation ID (link node), or node index in bits 0–31 and voxel in bits 32–39 (volume unit); 16 position (12); 28 g `float`; 32 parent index `uint32` (0xFFFFFFFF for the start node); 36 reserved (4).

A reader refuses, with `SAVE_ERROR_CORRUPT`: a wrong magic or version; a node reference entry of kind 3 or a node record of kind 4 with a non-zero class, index or identity field; a plan reason above 3; a count beyond its limit (tiles 65,536; volumes 256; load records 16,384; supplier records 65,536 with slots ≤ 16; carve queue and carved tiles 65,536; obstacles per carved tile and applied records 4,096; link registrations 65,536; agents 4,096 with corridor ≤ 256, volume corners ≤ 64 and pending completions ≤ 8; path entries 20,480 with active ≤ `ai.path.maxActiveRequests`, nodes per search ≤ `uAI_PATH_MAX_SEARCH_NODES`, open ≤ nodes and stamps ≤ min(nodes, the tile count of list 1) for a navmesh search or min(nodes, the volume count of list 2) for a volume search; ledger perceivers 65,536 with entries and pending forgets ≤ 16; blackboards 65,536; noises 512); a tile, volume or node index out of range; a parent index that does not name an earlier node; a heap entry naming a closed node; a non-zero reserved field; trailing bytes. At the defaults (64 active requests of 2,048 nodes) the search states are at most 64 × 2,048 × (40 + 4 + 8) bytes = 6.5 MiB (node record, heap entry, and at most one stamp per node). Version 1 has no migration step; each subsequent version adds one forward step and a fixture under `Zenith2/Engine/ZenithAI/Tests/Fixtures/Save/` generated at test time from the previous version's writer ([Z2-N-ARCH-257]).

## Parser limits

The rows AI asks the core-doc batch to add to `Zenith2/Docs/Architecture.md` → "Security architecture" table "Parser limits" (cross-area row AI8); each is a constant in the reader, tested at the limit and at limit + 1 (`Test_NavFormats_ParserLimits`, [Z2-N-ARCH-953]).

| Parser | Limits |
|---|---|
| navigation files (`.znav`) | the ASSET container limits; INFO 64 bytes; classes ≤ 6; areas = 64; profiles ≤ 8; tile vertices ≤ 16,384, polygons ≤ 4,096, detail vertices ≤ 127 and detail triangles ≤ 255 per polygon; layers W, H ≤ 2,048, spans ≤ 16,777,216, area volumes ≤ 256; manifest entries ≤ 1,048,576; volume nodes ≤ 4,194,304, levels ≤ 10 |
| navigation source intermediate (`ZNSG`) | vertices ≤ 263,169, triangles ≤ 524,288 |
| tile bake intermediate (`ZNTJ`) | scenes ≤ 4,096, sources ≤ 4,096, water volumes ≤ 256 |
| navigation and settings documents | the JSON limits of the table; `scenes` and `terrains` ≤ 4,096 entries; `volumes`, `carveRegions`, `waterVolumes` ≤ 256 |
| AI save section | the counts and index rules of "Save section layout" (search states at most `ai.path.maxActiveRequests` × `ai.path.maxSearchNodes` × 52 bytes plus heads) |
| script path requests and results | 64 and 416 bytes exactly |
