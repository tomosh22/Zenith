# STREAM design — formats and in-memory records

This file is part of the STREAM design family; the entry document is `Zenith2/Docs/Design/STREAM.md`. It specifies, byte for byte, the cooked streaming world index (`.zstream`), the streaming world authoring document, the rules a cell scene must satisfy, the cell delta encoding, the payloads of STREAM's four command kinds, the stream dump, and the in-memory runtime records the algorithms of `Zenith2/Docs/Design/STREAM/Runtime.md` operate on.

## Scope of this file

| Need | Heading |
|---|---|
| byte order, checks, what "sorted" means here | "Common rules" |
| the `.zstream` index: header use, sections, records, reader checks, limits | "Streaming world cooked file" |
| `*.streamworld.json`: schema, validation, the cook algorithm, the worst-case bound, an example | "Streaming world document" |
| what WORLD's partition cook must write into each cell scene | "Cell scene rules" |
| the bytes of `Zenith.Streaming.CellDelta` property 4 | "Cell delta encoding" |
| the payloads of STREAM's command kinds | "Module command payloads" |
| `zenith2.stream-dump` | "Stream dump format" |
| cell, source, admission, snapshot and HLOD records | "In-memory runtime records" |

## Common rules

- Every binary layout here is little-endian and is written and read field by field; no reader or writer copies a C++ object's memory (Z2-D-038). Every size, count, offset and index is checked against its input length or table size with overflow-checked arithmetic before use ([Z2-N-ARCH-951]); every field marked reserved is written as zero and a non-zero one is refused.
- `.zstream` uses ASSET's cooked container unchanged (`Zenith2/Docs/Design/ASSET/Formats.md` → "Header layout", "Section table", "Section placement and alignment", "Checks"); STREAM defines only the section kinds and their contents.
- A "check" is `Zenith_Hash64` (seed 0) truncated to its low 32 bits, as ASSET's (`Zenith2/Docs/Design/ASSET/Formats.md` → "Common rules"); D-SEL-HASH (open, Stage 1) changes it together with ASSET's.
- Persistent IDs are their 16-byte cooked binary form ([Z2-N-CONTRACT-067]); a null ID is 16 zero bytes. "Ascending persistent ID" compares the high word, then the low word, as unsigned 64-bit integers.
- Every table is written in a total order defined by its section, so the writer's output is byte-identical for equal inputs whatever the order of the descriptor's spans, and two cooks of one document on any machine produce the same `.zstream` ([Z2-N-CONTRACT-528] for the version rule).
- Floating-point fields are IEEE-754 binary32 or binary64 as stated; a non-finite value is refused by the reader and by the cook.

## Streaming world cooked file

<!-- elaboration -->
The partition model this file encodes (up to eight uniform runtime grids with roles, radii and grid-derived dependencies; a cell is a cooked SCENE scene) is a design choice of this family ("Design choices at a glance" in the entry document).

### Header use

The 64-byte ASSET header of a `.zstream`:

| Field | Value |
|---|---|
| magic | `ZSTW` (bytes `5A 53 54 57`) |
| format version | `uSTREAMING_WORLD_FORMAT_VERSION` (1) |
| asset GUID | the streaming world's persistent ID (the document's `worldId`) |
| kind | `ASSET_KIND_STREAMING_WORLD` (row ST6) |
| section count | 8 |

### Sections

Every section has index 0. Sections of more than 4,096 stored bytes are zstd level 3, smaller ones `NONE` (`Zenith2/Docs/Design/COMPRESS.md` → "Compressed section descriptor on disk").

| Kind | Name | Contents | Record bytes |
|---|---|---|---|
| 1 | `STREAM_SECTION_WORLD` | one world record | 64 |
| 2 | `STREAM_SECTION_GRIDS` | grid records in grid index order | 144 each |
| 3 | `STREAM_SECTION_CELLS` | cell records sorted by (grid, z, x) | 96 each |
| 4 | `STREAM_SECTION_DEPENDENCIES` | `uint32` cell indices, each cell's run ascending | 4 each |
| 5 | `STREAM_SECTION_PERSISTENT` | persistent entries sorted by (cell, persistent ID) | 24 each |
| 6 | `STREAM_SECTION_REFERENCES` | reference records sorted by (target cell, source cell, source entity, type ID, property ID, element) | 64 each |
| 7 | `STREAM_SECTION_OUTGOING` | `uint32` reference indices sorted by (source cell of the referenced record, reference index) | 4 each |
| 8 | `STREAM_SECTION_HLOD` | HLOD node records, per grid breadth-first from the top level (see "HLOD node records") | 96 each |

A section with no records (no references, no HLOD) is present with zero stored bytes, so the section count is always 8.

### World record

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | grid count G | 1 … 8 |
| 4 | 4 | cell count C | 1 … 65,536; equals the cell section's bytes ÷ 96 |
| 8 | 4 | dependency entry count | equals section 4 bytes ÷ 4; ≤ C × 32 |
| 12 | 4 | persistent entry count | ≤ 1,048,576; equals section 5 bytes ÷ 24 |
| 16 | 4 | reference count | ≤ 1,048,576; equals section 6 bytes ÷ 64 and section 7 bytes ÷ 4 |
| 20 | 4 | HLOD node count | ≤ 65,536; equals section 8 bytes ÷ 96 |
| 24 | 4 | maximum entities in one cell | ≤ 16,384; equals the maximum of the cell records' entity counts |
| 28 | 4 | flags | 0 |
| 32 | 16 | world ID | equals the header's asset GUID |
| 48 | 8 | estimated bytes | the sum of the cell records' estimated bytes |
| 56 | 4 | worst-case live instances | the cook's bound of "Worst-case active cells", ≤ 192 |
| 60 | 4 | reserved | 0 |

### Grid records

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 32 | name | UTF-8, 1 … 31 bytes of `[A-Za-z0-9_.-]`, zero-padded; unique within the world |
| 32 | 1 | role | `Zenith_StreamingGridRole` 1 … 5 |
| 33 | 1 | metric | `Zenith_StreamingDistanceMetric` 1 … 2 |
| 34 | 1 | HLOD level count | 0 … 3 |
| 35 | 1 | dependency grid mask | bit h set only for h < G, h ≠ this grid; the grid graph is acyclic |
| 36 | 4 | cell size | metres, 32 … 8,192, a multiple of 32 |
| 40 | 4 | first cell | the global index of the grid's first cell; grids' cell ranges are contiguous and in grid order |
| 44 | 4 | cell count | ≥ 1 |
| 48 | 8 | origin X | binary64 metres, absolute value ≤ 2^40 |
| 56 | 8 | origin Z | binary64 metres, absolute value ≤ 2^40 |
| 64 | 8 | base Y | binary64 metres, absolute value ≤ 2^40 |
| 72 | 8 | height | binary64 metres, 1 … 2^20 |
| 80 | 4 | activation radius | binary32 metres, > 0 |
| 84 | 4 | required radius | binary32, 0 ≤ value ≤ activation radius |
| 88 | 4 | unload hysteresis | binary32, 0 ≤ value ≤ activation radius |
| 92 | 4 | prefetch radius | binary32, ≥ activation radius + unload hysteresis |
| 96 | 4 | unload delay ticks | 0 … 3,600 |
| 100 | 4 | overlap margin fraction | binary32, 0 … 1 |
| 104 | 12 | HLOD maximum distance per level 1 … 3 | binary32 metres; for a level above the level count, 0; each ≥ the activation radius and non-decreasing by level |
| 116 | 12 | HLOD refine distance per level 1 … 3 | binary32 metres; level 1's is the activation radius; level k's (k ≥ 2) is level k − 1's maximum distance; 0 above the level count |
| 128 | 4 | worst-case active cells | B'(g) of "Worst-case active cells", including the cells its dependents pin |
| 132 | 4 | reserved | 0 |
| 136 | 8 | reserved | 0 |

### Cell records

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 16 | scene | the cell scene's persistent ID; not null; unique among cells |
| 16 | 16 | anchor entity | the persistent ID of the cell scene's anchor root ("Cell scene rules") |
| 32 | 1 | grid | < G; equals the grid whose cell range holds this record |
| 33 | 3 | reserved | 0 |
| 36 | 4 | x | `sint32`, absolute value ≤ 2^20 |
| 40 | 4 | z | `sint32`, absolute value ≤ 2^20 |
| 44 | 4 | entity count | 1 … 16,384 |
| 48 | 8 | estimated bytes | the cook's estimate of the cell's resident bytes |
| 56 | 4 | first dependency | index into section 4 |
| 60 | 4 | dependency count | 0 … 32 |
| 64 | 4 | first persistent | index into section 5 |
| 68 | 4 | persistent count | 0 … 1,024 |
| 72 | 4 | first incoming reference | index into section 6 |
| 76 | 4 | incoming reference count | 0 … 4,096 |
| 80 | 4 | first outgoing reference | index into section 7 |
| 84 | 4 | outgoing reference count | 0 … 4,096 |
| 88 | 4 | HLOD node | the cell's level-1 node, or 0xFFFFFFFF |
| 92 | 4 | reserved | 0 |

The ranges of consecutive cells are contiguous: cell i + 1's first dependency is cell i's first plus its count, and likewise for persistent entries, incoming references and outgoing references; cell 0's firsts are 0, and the last cell's ranges end exactly at their section's end. A cell's coordinate `(grid, x, z)` is unique.

### Dependency section

Cell i's run is `[first dependency, first dependency + dependency count)`, strictly ascending cell indices, none equal to i. A dependency means "cell i is activated only after each listed cell is ACTIVE" (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Desired-set algorithm"). The cook writes two kinds, merged and deduplicated: grid-derived dependencies (for every grid h in the cell's grid's dependency mask, every cell of grid h whose XZ rectangle intersects this cell's rectangle grown by grid h's overlap margin times its cell size) and the document's explicit `dependsOn` pairs. The cell dependency graph is acyclic (it is, by construction, when the grid graph is acyclic and explicit pairs only point to grids earlier in the topological order; the cook refuses any cycle with `COOK_ERROR_STREAM_DEPENDENCY_CYCLE`).

### Persistent section

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 16 | entity | the entity's persistent ID; unique across the world |
| 16 | 4 | cell | the cell whose range holds the entry |
| 20 | 1 | mode | `Zenith_StreamingPersistenceMode` 1 … 2 (an entity with mode 0 is not listed) |
| 21 | 3 | reserved | 0 |

### Reference sections

Reference record:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | source cell | < C |
| 4 | 4 | target cell | < C, ≠ source cell |
| 8 | 16 | source entity | an entity of the source cell scene |
| 24 | 16 | target entity | an entity of the target cell scene |
| 40 | 8 | type ID | a reflected component type (`Zenith_TypeID`) |
| 48 | 4 | property ID | an `ENTITY` property, or a `FIXED_ARRAY` of `ENTITY`, of that type |
| 52 | 4 | element | the fixed-array element index, or 0xFFFFFFFF for a scalar property |
| 56 | 8 | reserved | 0 |

Section 6 groups records by target cell (each cell's incoming range is contiguous). Section 7 lists, for each source cell in ascending order, the indices of the records whose source is that cell, ascending; a cell's outgoing range indexes section 7. Two records with equal (source entity, type, property, element) are refused: one reference slot names one target.

### HLOD node records

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 1 | level | 1 … the grid's HLOD level count |
| 1 | 1 | grid | < G, a grid with HLOD levels |
| 2 | 1 | mesh count | 1 … 4 |
| 3 | 1 | flags | bit 0 casts shadows; other bits 0 |
| 4 | 4 | x | `sint32`, node coordinate at its level |
| 8 | 4 | z | `sint32` |
| 12 | 4 | parent | node index of the level + 1 node covering it, or 0xFFFFFFFF at the grid's top level |
| 16 | 4 | first child | index of the first child, or 0xFFFFFFFF at level 1 |
| 20 | 4 | child count | 0 at level 1; 1 … 16 above |
| 24 | 8 | reserved | 0 |
| 32 | 64 | meshes | 4 GEOM mesh persistent IDs; slots beyond the mesh count are null |

A level-k node at node coordinate (x, z) covers the base cells `[x · 4^(k−1), (x + 1) · 4^(k−1)) × [z · 4^(k−1), (z + 1) · 4^(k−1))` of its grid (`uSTREAMING_HLOD_LEVEL_FACTOR`), with floor division for negative coordinates. Its anchor is the minimum corner of that region at the grid's base height. Proxy meshes are baked in anchor-relative coordinates. Order: for each grid in grid order, the top-level nodes in (z, x) order, then breadth-first, each node's children contiguous and in (z, x) order. A level-1 node exists exactly for the cells whose record names it, and names that cell's coordinate.

### Reader checks

`Zenith_StreamingWorldKindLoader::OnSectionsLoaded` applies ASSET's container checks first (through `Zenith_ParseCookedHeader`, which yields `ASSET_CORRUPTION_*` and is ASSET's to report), then these in order, and fails the load with the first failure (`STREAMING_ERROR_WORLD_CORRUPT` unless stated; `m_uDetail` is the check number, `m_ulAux` the record index):

| # | Check |
|---|---|
| S1 | exactly 8 sections, kinds 1 … 8, each index 0 |
| S2 | the world section decodes to exactly 64 bytes; flags and reserved zero; the world ID equals the header GUID |
| S3 | G, C, the persistent, reference and HLOD counts within "Limits" (else `STREAMING_ERROR_WORLD_LIMIT`) |
| S4 | every section's decoded size equals its count times its record bytes |
| S5 | grid records: name charset, length, zero padding and uniqueness; role, metric and level count in range; reserved zero |
| S6 | grid numeric fields finite and within the grid record's rules; the radii ordering; HLOD distances per level |
| S7 | grid cell ranges contiguous, in grid order, covering `[0, C)` |
| S8 | the grid dependency masks name only other existing grids and form an acyclic graph (Kahn's algorithm over ≤ 8 nodes) |
| S9 | cell records: grid in range and matching its range; coordinates within ±2^20; entity count 1 … 16,384; reserved zero |
| S10 | cells sorted strictly by (grid, z, x) (which also proves coordinate uniqueness) |
| S11 | cell scene IDs non-null and unique (checked by sorting a scratch copy of the IDs) |
| S12 | the four range families contiguous per "Cell records"; every count within its per-cell limit |
| S13 | dependency runs strictly ascending, in range, no self-dependency |
| S14 | the cell dependency graph is acyclic (iterative Kahn's algorithm over C nodes with the dependents table built in step L3 of the loader) |
| S15 | persistent entries: cell equals the cell whose range holds them; mode 1 … 2; sorted by persistent ID within the cell; IDs unique across the world (checked on the sorted by-ID index built in step L4) |
| S16 | reference records: cells in range and different; grouped by target per the cell ranges; element is 0xFFFFFFFF or < 65,536; reserved zero; no duplicate (source entity, type, property, element) |
| S17 | section 7: each index < reference count; each appears exactly once; grouped by source cell per the cell ranges |
| S18 | HLOD nodes: level, grid, mesh count, flags in range; mesh slots beyond the count null, within it non-null; reserved zero |
| S19 | HLOD hierarchy: parent and child links consistent and in range; child coordinates inside their parent's region; level-1 nodes match their cell's coordinate and the cell names them; breadth-first order |
| S20 | the world record's derived fields (maximum entities, estimated bytes) equal what the tables give |

Loader steps around the checks: L1 allocate one block for every table plus the derived structures (the size is computed from the counts before allocation; a failure is `Fail(STREAMING_ERROR_OUT_OF_MEMORY)`); L2 decode the records into it; L3 build the dependents table by inverting dependencies (counting sort by dependency, so each dependents run is ascending); L4 build the by-ID persistent index (sorted indices); L5 build the cell hash map (`Zenith_HashMap<uint64, uint32>` keyed by `(grid << 56) ^ ((x & 0xFFFFFFF) << 28) ^ (z & 0xFFFFFFF)` packed exactly, since |x|, |z| ≤ 2^20 fit 28 bits), the scene-GUID map (`Zenith_HashMap<Zenith_PersistentID, uint32>` from each cell scene's persistent ID to its cell index, behind `FindCellByScene`; S11 makes it injective) and per-grid top-node spans; L6 run S1 to S20; L7 `SetResidentBytes` and `Activate`. Every step is bounded by the counts; the loader never asserts on file data ([Z2-N-CONTRACT-472]).

### Limits

| Quantity | Limit | Constant |
|---|---|---|
| grids per world | 8 | `uSTREAMING_MAX_GRIDS` |
| cells per world | 65,536 | `uSTREAMING_MAX_CELLS` |
| cell coordinate magnitude | 2^20 | `iSTREAMING_MAX_CELL_COORDINATE` |
| cell size | 32 … 8,192 m, multiple of 32 | `uSTREAMING_MIN_CELL_SIZE_METRES`, `uSTREAMING_MAX_CELL_SIZE_METRES`, `uSTREAMING_CELL_SIZE_STEP_METRES` |
| dependencies per cell | 32 | `uSTREAMING_MAX_DEPENDENCIES_PER_CELL` |
| entities per cell scene | 16,384 | `uSTREAMING_MAX_CELL_ENTITIES` |
| persistent entities per world / per cell | 1,048,576 / 1,024 | `uSTREAMING_MAX_PERSISTENT_ENTITIES`, `uSTREAMING_MAX_PERSISTENT_PER_CELL` |
| references per world / per cell (each direction) | 1,048,576 / 4,096 | `uSTREAMING_MAX_REFERENCES`, `uSTREAMING_MAX_REFERENCES_PER_CELL` |
| HLOD levels / nodes / meshes per node | 3 / 65,536 / 4 | `uSTREAMING_MAX_HLOD_LEVELS`, `uSTREAMING_MAX_HLOD_NODES`, `uSTREAMING_MAX_HLOD_MESHES_PER_NODE` |
| grid name | 31 bytes | `uSTREAMING_GRID_NAME_BYTES` − 1 |
| worst-case live instances | 192 | `uSTREAMING_MAX_LIVE_INSTANCES` |
| decoded index size at every limit | 6.3 MiB of cells and dependencies, 88 MiB of references and persistent entries, 6 MiB of HLOD nodes | — |

Each limit is tested at the limit and at limit + 1 ([Z2-N-ARCH-953]; `Zenith2/Docs/Design/STREAM/Tests.md` → "Cook and format tests").

These limits, with the cell delta's (65,535 bytes, the most a `container<u8>` holds, `Zenith_ContainerHeap`'s 65,535-element cap, 1,024 entity records, at most 32 component types per entity record, present and absent together: 1 ≤ M + A ≤ 32, check D5), are the `.zstream` and streaming cell delta rows of `Zenith2/Docs/Architecture.md` → "Parser limits" (core-doc item X228 (b)); this table is the parser limit the fuzz target and the at-limit tests assert.

## Streaming world document

### Location and type

`Games/<Name>/Scenes/<World>/<World>.streamworld.json`, document type `Zenith.Streaming.WorldDocument`, schema version 1, canonical JSON written by `Zenith_JsonWriter` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical rules"): object members in the order of the tables below, arrays in the order each table states, LF line endings and one trailing LF. Unknown members are preserved in position ([Z2-N-CONTRACT-543]). The document is registered with `Zenith_MigrationRegistry` by `Zenith_RegisterStreamingDocumentTypes` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_MigrationRegistry header").

### Root members

| Member | Type | Required | Rule |
|---|---|---|---|
| `$type` | string | yes | `"Zenith.Streaming.WorldDocument"` |
| `schemaVersion` | integer | yes | 1 |
| `worldId` | persistent ID | yes | the `.zstream` asset GUID; equals the document's own GUID in its meta sidecar |
| `grids` | array of grid objects | yes | 1 … 8, in grid index order |
| `cells` | array of cell objects | yes | 1 … 65,536, sorted by (grid name's grid index, z, x) |
| `hlod` | array of HLOD objects | no (default empty) | sorted by (grid index, level descending, z, x) |
| `references` | array of reference objects | no (default empty) | sorted by (target cell, source cell, source entity, component, property, element) |

### Grid object

| Member | Type | Default | Rule |
|---|---|---|---|
| `name` | string | — | the grid record's name rule |
| `role` | `"collision"`, `"navigation"`, `"gameplay"`, `"audio"`, `"large"` | `"gameplay"` | |
| `metric` | `"xz"`, `"xyz"` | `"xz"` | |
| `cellSize` | integer metres | 128 | 32 … 8,192, multiple of 32 |
| `origin` | `[x, z]` numbers | `[0, 0]` | |
| `baseY`, `height` | numbers | −1,024, 4,096 | |
| `activationRadius`, `requiredRadius`, `unloadHysteresis`, `prefetchRadius` | numbers | 384, 128, 64, 640 | the grid record's ordering |
| `unloadDelayTicks` | integer | 0 | 0 … 3,600 |
| `overlapMargin` | number | 0.25 | 0 … 1 |
| `dependsOn` | array of grid names | `[]` | names of other grids, declared anywhere in the array; the graph must be acyclic |
| `hlod` | object `{ "levels": 0…3, "maxDistance": [numbers], "refineDistance": [numbers] }` | `{ "levels": 0 }` | arrays of length `levels`; `refineDistance` may be omitted and is then derived as the grid record states |

### Cell object

| Member | Type | Rule |
|---|---|---|
| `grid` | grid name | an existing grid |
| `x`, `z` | integers | within ±2^20; unique per grid |
| `scene` | persistent ID | a scene asset (`ASSET_KIND_SCENE`) of the same game; unique among cells |
| `dependsOn` | array of `{ "grid", "x", "z" }` | optional extra dependencies beyond the grid-derived ones; each names an existing cell of a different grid |

The anchor entity, the entity count, the persistent entries and the estimated bytes are not authored: the cook step derives them from the cell scene's cooked output.

### HLOD object

| Member | Type | Rule |
|---|---|---|
| `grid` | grid name | a grid with `hlod.levels` ≥ the node's level |
| `level` | integer | 1 … 3 |
| `x`, `z` | integers | the node coordinate at its level |
| `meshes` | array of persistent IDs | 1 … 4 GEOM mesh assets (`ASSET_KIND_MESH`), baked relative to the node anchor |
| `castsShadows` | boolean, default true | |

### Reference object

| Member | Type | Rule |
|---|---|---|
| `sourceCell`, `targetCell` | `{ "grid", "x", "z" }` | existing, different cells |
| `sourceEntity`, `targetEntity` | persistent IDs | entities of the source and target cell scenes |
| `component` | stable name | a reflected component type of the source entity |
| `property` | integer | an `ENTITY` property or fixed array of `ENTITY` of that component |
| `element` | integer or `null` | the fixed-array element; `null` for a scalar property |

### Cook step algorithm

The `stream.world` step (`Zenith2/Docs/Design/STREAM.md` § "Authoring and cooking", table "Cook step"), in `Cook(Zenith_CookContext&)`:

1. `OpenSourceDocument` the document; migrate it in memory if its schema version is older (never written back by the cook, [Z2-N-CONTRACT-543]); validate its members (V1 to V10 of "Validation messages").
2. Resolve grid names to indices; topologically sort the grid graph (Kahn's algorithm with ties broken by grid index), refusing a cycle (V11).
3. For each cell, `ReadDependencyOutput` its scene's cooked output and decode it with SCENE's `Zenith_DecodeCookedScene` into a scratch arena; check the cell scene rules (V12 to V18 of "Cell scene rules", "Cell scene checks"); record the anchor ID, the entity count and the persistent entries; compute the estimated bytes as the scene's cooked bytes plus, for every hard dependency in its ASSET_REFS, the catalogue entry's decoded bytes, each asset counted once per cell.
4. Build the dependency lists: grid-derived plus explicit, deduplicated, sorted; refuse more than 32 for one cell (V19); run Kahn's algorithm over the cell graph and refuse a cycle (V11).
5. Validate HLOD objects (V20, V21): every level-1 node's cell exists; every node above level 1 has at least one child node; parents are derived from coordinates (floor division by 4); a node with no parent at a level below the grid's level count is refused.
6. Validate references (V22): both cells exist and differ; the source entity is in the source cell scene and its component has the property, of `ENTITY` kind or a fixed array of `ENTITY` with the element in range; the target entity is in the target cell scene; the cell scene's value of that property slot is null ("Cross-cell reference nulling").
7. Compute "Worst-case active cells", E(h) of the explicit dependencies included, and refuse a bound above 192 (V23).
8. Build a `Zenith_StreamingWorldDesc` from the result and call `Zenith_ValidateStreamingWorld` then `Zenith_WriteStreamingWorldFile`; every limit beyond the document checks is V24.
9. Declare the cell scenes and HLOD meshes as soft dependencies (packaged, not loaded with the index) and write the cook report member `streaming`.

Every message names the document path and a JSON pointer to the offending member ([Z2-N-CONTRACT-549] for fixtures); the step reports every message it finds before failing, in V order, then document order.

### Validation messages

| # | Condition | Cook error | Text |
|---|---|---|---|
| V1 | not JSON, wrong `$type`, schema version newer than 1 | `COOK_ERROR_STREAM_DOCUMENT` | `stream.world: '<path>' is not a streaming world document (<reason>)` |
| V2 | a required member missing or of the wrong JSON type | `COOK_ERROR_STREAM_DOCUMENT` | `stream.world: <pointer>: expected <type>` |
| V3 | grid count 0 or above 8; a grid name invalid or duplicated | `COOK_ERROR_STREAM_GRID` | `stream.world: <pointer>: grid name '<name>' <is invalid / is used twice>` |
| V4 | an unknown role or metric | `COOK_ERROR_STREAM_GRID` | `stream.world: <pointer>: unknown <role/metric> '<value>'` |
| V5 | a cell size outside 32 … 8,192 or not a multiple of 32 | `COOK_ERROR_STREAM_GRID` | `stream.world: grid '<name>': cell size <n> must be a multiple of 32 in [32, 8192]` |
| V6 | radii out of order, a non-finite number, height out of range | `COOK_ERROR_STREAM_RADII` | `stream.world: grid '<name>': <radius> <value> violates <rule>` |
| V7 | HLOD distances wrong length, decreasing, or below the activation radius | `COOK_ERROR_STREAM_RADII` | `stream.world: grid '<name>': hlod level <k> distance <value> violates <rule>` |
| V8 | a cell coordinate beyond ±2^20 | `COOK_ERROR_STREAM_CELL_RANGE` | `stream.world: cell (<grid>, <x>, <z>): coordinate beyond ±1048576` |
| V9 | two cells with one coordinate, or one scene in two cells | `COOK_ERROR_STREAM_CELL_DUPLICATE` | `stream.world: cell (<grid>, <x>, <z>) <duplicates a coordinate / reuses scene <id> of cell (…)>` |
| V10 | a cell's scene is not a scene asset of the game | `COOK_ERROR_STREAM_CELL_SCENE_KIND` | `stream.world: cell (<grid>, <x>, <z>): '<id>' is not a scene` |
| V11 | a grid or cell dependency cycle | `COOK_ERROR_STREAM_DEPENDENCY_CYCLE` | `stream.world: dependency cycle <a> -> <b> -> … -> <a>` |
| V12 | the cell scene has no root carrying `Zenith.Streaming.CellAnchor`, or more than one root | `COOK_ERROR_STREAM_CELL_ANCHOR` | `stream.world: cell (…): scene '<id>' must have exactly one root entity with Zenith.Streaming.CellAnchor` |
| V13 | the anchor's cell scene (property 1) is not the cell's own scene, or its world ID disagrees with the document | `COOK_ERROR_STREAM_CELL_ANCHOR` | `stream.world: cell (…): anchor names scene <id> of world <id>` |
| V14 | the anchor root's local transform is not the identity | `COOK_ERROR_STREAM_CELL_ANCHOR` | `stream.world: cell (…): anchor root transform must be identity` |
| V15 | an entity position outside the cell's rectangle grown by the overlap margin, or outside the vertical extent | `COOK_ERROR_STREAM_CELL_BOUNDS` | `stream.world: cell (…): entity <id> at (<x>, <y>, <z>) is outside the cell bounds by <d> m` |
| V16 | the cell scene has more entities than min(16,384, the game's configured `ecs.maxSpawnsPerTick`, the value of Z2-B-ECS-003 once allocated, read from the cook context's configuration) | `COOK_ERROR_STREAM_LIMIT` | `stream.world: cell (…): <n> entities exceed <limit>` |
| V17 | a `Zenith.Streaming.Persistent` component whose entity ID or cell scene (property 2) disagrees, or a persistent ID listed in two cells | `COOK_ERROR_STREAM_PERSISTENT_ID` | `stream.world: cell (…): entity <id>: <reason>` |
| V18 | more than 1,024 persistent entities in a cell | `COOK_ERROR_STREAM_LIMIT` | `stream.world: cell (…): <n> persistent entities exceed 1024` |
| V19 | more than 32 dependencies for a cell | `COOK_ERROR_STREAM_LIMIT` | `stream.world: cell (…): <n> dependencies exceed 32` |
| V20 | an HLOD node whose grid, level, coordinate or meshes are invalid | `COOK_ERROR_STREAM_HLOD` | `stream.world: <pointer>: <reason>` |
| V21 | an HLOD node with no parent below the top level, or no children above level 1 | `COOK_ERROR_STREAM_HLOD` | `stream.world: hlod (<grid>, <level>, <x>, <z>): <missing parent / has no children>` |
| V22 | a reference that fails step 6 | `COOK_ERROR_STREAM_REFERENCE` | `stream.world: <pointer>: <reason>` |
| V23 | the worst-case live-instance bound above 192 | `COOK_ERROR_STREAM_LIMIT` | `stream.world: worst-case active cells <n> exceed 192 (grid '<name>' contributes <m>)` |
| V24 | any other limit of "Limits" | `COOK_ERROR_STREAM_LIMIT` | `stream.world: <quantity> <n> exceeds <limit>` |

### Worst-case active cells

For grid g with cell size c_g and KEEP reach `r_g = activationRadius_g + unloadHysteresis_g`, the cells one source at radius scale 1 keeps active by its own band lie in the axis-aligned square of half-width r_g around the source, so their count is at most `B(g) = (2 · ceil(r_g / c_g) + 1)²`. A grid h that other grids depend on also holds the cells pinned by its dependents, and a dependent may itself be pinned by its own dependents, so the reach is defined transitively. Write `reach(g)` for the largest per-axis distance from the source to the near edge of a cell of grid g that the source keeps active or pinned; a cell of g whose near edge lies within reach(g) has its far edge within `reach(g) + c_g`, and the h-cells it pins intersect that cell grown by h's overlap margin `m_h · c_h`. Visiting the grids in dependency order (dependents before their dependencies, `GetGridInDependencyOrder`, so every dependent's reach is final when its dependency is visited):

```text
reach(h) = max(r_h, max over every grid g whose dependency mask names h of (reach(g) + c_g + m_h · c_h))
E(h)     = the number of distinct cells of grid h in the dependency closure of the explicit-dependency targets
B'(h)    = min(cellCount(h), (2 · ceil(reach(h) / c_h) + 1)² + E(h))
worst-case live instances = Σ over grids h of B'(h)
```

**Explicit dependencies.** An explicit `dependsOn` pair ("Cell object") may name a cell at any distance from its dependent, so the cells it pins, and the cells those pin in turn through grid-derived or further explicit dependencies, can lie outside every `reach(h)` square. E(h) counts them: the cook takes every cell named as the target of an explicit pair, walks the cell dependency graph from it (every dependency edge, grid-derived or explicit; breadth-first over the dependency runs, each cell visited once, O(C + edges)), and counts the visited cells per grid. A pinned cell is then either within its grid's reach square (reached from the source's own bands through grid-derived edges alone) or in that closure, so the bound holds whatever the explicit pairs name; with no explicit pair E(h) is 0 and B'(h) is the expression without it.

For a grid no other grid names, reach(h) = r_h and B'(h) = B(h) + E(h); for a grid named only by grids that nothing pins, the inner term is the direct `r_g + c_g + m_h · c_h` of a single level. In a chain gameplay → navigation → collision, the collision cells pinned through the navigation cells a far-reaching gameplay grid pins lie up to `r_gp + c_gp + m_nav · c_nav + c_nav + m_col · c_col` from the source per axis, and reach(collision) is exactly that when it exceeds the other terms. Both the own-band set and every pinned set surround the same source, so the grid's bound takes the largest reach, not a sum. The grid record stores B'(h) (E(h) included) and the world record the sum; the cook refuses a sum above 192 (V23). The bound is per source at radius scale 1; several sources share cells, and the runtime cap `stream.maxLiveInstances` holds whatever the source count (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Load ordering and admission"). At binding the runtime compares the sum with the configured `stream.maxLiveInstances` and logs and counts a world whose bound exceeds the cap (`Zenith2/Docs/Design/STREAM/Runtime.md` → "World binding"); such a world still streams, its REQUIRED loads preempting KEEP-band cells at the cap. The report member `streaming` lists each grid's B and B' so a designer sees which grid dominates.

### Example document

```json
{
	"$type": "Zenith.Streaming.WorldDocument",
	"schemaVersion": 1,
	"worldId": "3c2a7e10-5d4b-4b8e-9f21-0a6b3d7c9e01",
	"grids": [
		{
			"name": "collision",
			"role": "collision",
			"metric": "xz",
			"cellSize": 256,
			"origin": [-2048, -2048],
			"baseY": -512,
			"height": 2048,
			"activationRadius": 512,
			"requiredRadius": 256,
			"unloadHysteresis": 64,
			"prefetchRadius": 768,
			"unloadDelayTicks": 0,
			"overlapMargin": 0.25,
			"dependsOn": [],
			"hlod": { "levels": 0 }
		},
		{
			"name": "content",
			"role": "gameplay",
			"metric": "xz",
			"cellSize": 128,
			"origin": [-2048, -2048],
			"baseY": -512,
			"height": 2048,
			"activationRadius": 384,
			"requiredRadius": 128,
			"unloadHysteresis": 64,
			"prefetchRadius": 640,
			"unloadDelayTicks": 30,
			"overlapMargin": 0.25,
			"dependsOn": ["collision"],
			"hlod": { "levels": 2, "maxDistance": [2048, 8192] }
		}
	],
	"cells": [
		{ "grid": "collision", "x": 0, "z": 0, "scene": "9a1d4c2e-7b3f-4e5a-8c6d-1f2e3a4b5c60" },
		{ "grid": "content", "x": 0, "z": 0, "scene": "9a1d4c2e-7b3f-4e5a-8c6d-1f2e3a4b5c61" },
		{ "grid": "content", "x": 1, "z": 0, "scene": "9a1d4c2e-7b3f-4e5a-8c6d-1f2e3a4b5c62" }
	],
	"hlod": [
		{ "grid": "content", "level": 2, "x": 0, "z": 0, "meshes": ["5e6f7a8b-9c0d-4e1f-8a2b-3c4d5e6f7a80"], "castsShadows": true },
		{ "grid": "content", "level": 1, "x": 0, "z": 0, "meshes": ["5e6f7a8b-9c0d-4e1f-8a2b-3c4d5e6f7a81"], "castsShadows": true },
		{ "grid": "content", "level": 1, "x": 1, "z": 0, "meshes": ["5e6f7a8b-9c0d-4e1f-8a2b-3c4d5e6f7a82"], "castsShadows": true }
	],
	"references": [
		{
			"sourceCell": { "grid": "content", "x": 0, "z": 0 },
			"sourceEntity": "0b1c2d3e-4f50-4617-8293-a4b5c6d7e8f9",
			"targetCell": { "grid": "content", "x": 1, "z": 0 },
			"targetEntity": "1c2d3e4f-5061-4728-93a4-b5c6d7e8f90a",
			"component": "Game.DoorLink",
			"property": 1,
			"element": null
		}
	]
}
```

### Migration chain

Version 1 has an empty chain. Every subsequent schema version adds one `Zenith_MigrationStep` registered by `Zenith_RegisterStreamingDocumentTypes`, with a fixture pair (old document, expected migrated document, both JSON text) under `Zenith2/Tools/ZenithCook/Tests/Streaming/Migration/` and the document framework's preview, backup and journaled write when an editor migrates a file on disk (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Journaled write").

## Cell scene rules

### Anchor root

A cell scene has exactly one root entity. It carries `Zenith.Streaming.CellAnchor` with the cell scene's own persistent ID (property 1, the cell's content-stable identity) and the world ID, `Zenith.LocalTransform` at the identity, and every other entity of the cell scene is its descendant. The anchor stands at the cell's absolute anchor (the cell's minimum XZ corner at the grid's base Y); every entity's authored transform is relative to it. At activation the root receives the cell's translation relative to the current world origin (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Large coordinates and origin rebasing"), so a cell scene is position-independent, and a world of any extent within ±2^20 cells keeps every cooked `float` within one cell of its anchor.

### Entity bounds and the overlap margin

Every entity's world-space position (its `Zenith.WorldTransform` position, computed by the cook from the hierarchy relative to the anchor) lies inside the cell's XZ rectangle grown by `overlapMargin × cellSize` on each side, and inside `[baseY, baseY + height)` vertically. Content that is larger (a long bridge, a mountain) belongs in a grid with larger cells or the `large` role. The overlap margin is also the reach of grid-derived dependencies, so a dependent cell's content near its edge is covered by the collision or navigation cells it depends on.

### Persistent markers

An entity whose state must survive unload carries `Zenith.Streaming.Persistent` with its mode, its cell scene's persistent ID and a copy of its own persistent ID. Only entities with authored persistent IDs can be persistent (a runtime-spawned entity has none, [Z2-N-CONTRACT-066]). A persistent entity's descendants are destroyed and restored with it but are not persistent themselves unless they carry the component.

### Cross-cell reference nulling

An `ENTITY` property of a cell-scene entity that names an entity of another cell is written as null in the cell scene and recorded in the streaming world document's `references`, because SCENE resolves references within one scene's ID space only (`Zenith2/Docs/Design/SCENE/Format.md` → "Resolved ID space"). STREAM writes the live handle when both cells are active (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Cross-cell references"). References into the persistent layer (the world's main scene, always loaded) are ordinary persistent-ID references and need no record: SCENE's activation resolves them against live entities by persistent ID.

### Content of each grid role

| Role | Content the partition cook writes into the role's cells | Consumer |
|---|---|---|
| `collision` | static colliders, terrain colliders, trigger volumes, kinematic platforms; the region that lets bodies simulate | PHYS (row ST8) |
| `navigation` | navigation mesh tiles and their links (components of AI, pending D-SEL-NAV) | AI (row ST9) |
| `gameplay` | everything else: meshes, lights, probes, irradiance volumes, the cell's lightmap set, decals, gameplay entities, ENV's terrain tiles and foliage cells | every module |
| `audio` | ambience zones, emitters, reverb and occlusion volumes, and the bank references they hold | AUDIO (row ST10) |
| `large` | landmark meshes and distant gameplay with a long activation radius | every module |

LIGHT's content (row ST12, answering LB20): `Flux_ReflectionProbeComponent`, `Flux_IrradianceVolumeComponent` and the cell's lightmap set are components of entities in `gameplay` cells; their baked assets are hard dependencies of the cell scene (cooked ASSET_REFS), so ASSET makes them ACTIVE before SCENE's preparation reads the scene, they enter the world in the cell's activation sync point and they leave it at the unload sync point, when their last handles drop. The lightmap set of a cell lives on the anchor root (one set per cell, keyed by the cell scene), so a cell's lightmaps load and unload exactly with its content. ENV's content follows `Zenith2/Docs/Design/ENV/Bake.md` → "Cell content records"; ENV's alignment rule (cells a multiple of 128 m aligned to the terrain origin) is ENV's cook check, not STREAM's.

### Cell scene checks

The cook step's V12 to V18 above are the cell scene checks. WORLD's partition cook writes cell scenes that pass them; a hand-written cell scene (tests, small games) must pass them too. SCENE's own scene checks apply first (the scene is cooked by SCENE's step before `stream.world` reads its output).

## Cell delta encoding

<!-- elaboration -->
The delta encoding (DESTROYED and STATE records of field sets, keyed by persistent ID) is a design choice of this family ("Design choices at a glance" in the entry document).

The bytes of `Zenith.Streaming.CellDelta` property 4 for one cell: what changed, at its last unload, in the cell's persistent entities. Format version 1 (`uSTREAMING_DELTA_FORMAT_VERSION`), recorded in property 3 and in the header.

### Delta header

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 2 | format version | 1 |
| 2 | 2 | flags | 0 |
| 4 | 4 | entity record count N | 0 … 1,024 |
| 8 | 4 | total bytes | equals the container's element count; ≤ 65,535 |
| 12 | 4 | check | check over bytes `[16, total bytes)` |

### Entity records

N records follow, sorted by ascending persistent ID, each:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 16 | entity | a persistent entry of this cell |
| 16 | 1 | kind | 1 DESTROYED (a tombstone; nothing follows), 2 STATE |
| 17 | 1 | reserved | 0 |
| 18 | 2 | present count M | 0 for DESTROYED; 0 … 32 for STATE |
| 20 | 4 | record bytes | the record's total size, from offset 0, a multiple of 4 |
| 24 | 2 | absent count A | 0 for DESTROYED; 0 … 32 for STATE, with 1 ≤ M + A ≤ 32 |
| 26 | 2 | reserved | 0 |
| 28 | 8 × A | absent types | the type IDs (`Zenith_TypeID`, uint64) of configured types the entity did not carry at capture, strictly ascending |
| 28 + 8A | … | present types | M binary field-set records (`Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record") in strictly ascending type ID, each padded with zeros to a multiple of 4 |

A STATE record states, for every type of `stream.persistence.componentTypes` at capture, either its presence (a field-set record of the type's eligible properties with the type's schema version at capture, `Zenith2/Docs/Design/STREAM/Runtime.md` → "Eligible property kinds") or its absence (its type ID in the absent list); a type ID appears in at most one of the two lists. At reload the presence is applied by SCENE's amendment and the values by `Streaming.ApplyCellState` (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Component presence"). A DESTROYED record supersedes everything: the entity is skipped at preparation and never restored.

### Decoder checks

`Zenith_DecodeStreamingCellDelta` (Internal) applies, in order, and returns `STREAMING_ERROR_DELTA_CORRUPT` with the check number as detail:

| # | Check |
|---|---|
| D1 | at least 16 bytes; version is a version the build decodes (1); flags 0 |
| D2 | total bytes equals the input length and ≤ 65,535; the check matches |
| D3 | N ≤ 1,024 |
| D4 | each record: at least 28 bytes remain; record bytes ≥ 28, a multiple of 4, and within the remaining input |
| D5 | kind 1 or 2; reserved bytes 0; M = A = 0 for kind 1; M ≤ 32, A ≤ 32 and 1 ≤ M + A ≤ 32 for kind 2 |
| D6 | records strictly ascending by persistent ID |
| D7 | the absent list fits the record and is strictly ascending; each present record decodes with `Zenith_DecodeFieldSet` within its record's bytes, type IDs strictly ascending, none also in the absent list; padding bytes zero |
| D8 | the records exactly fill the input |
| D9 | every entity is a persistent entry of the delta's cell, the cell its holder's `CellScene` names in the bound world; otherwise the record is set aside, not an error (content changed since the save): it is never applied to this cell, the update's step 14 re-homes it to the entity's current cell once that cell is UNLOADED, FAILED or FAILED_PERMANENT, a capture carries it unchanged until then and keeps the holder's re-home bit set (`CellStatus` property 6 bit 6), and only a record whose entity is a persistent entry of no cell of the world is dropped and counted in `stream.persistence.records_dropped` (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Cell identity across re-cooks") |

### Versioning

A delta written by an older format version is decoded by that version's decoder (each version's decoder stays in the build, [Z2-N-CONTRACT-543] for saves) and rewritten in the current version at the cell's next capture. A component record whose schema version is older than its type's is migrated through `Zenith_MigrateFieldSet` before it is applied (`Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration"); an unknown type ID is skipped and counted (`MIGRATION_WARNING_UNKNOWN_FIELD` is the type's per-field rule; a whole unknown type is `stream.persistence.unknown_types`).

## Module command payloads

Each payload is a registered `TYPE_KIND_STRUCT`; the playback reads exactly the bytes below and ignores (and counts in `stream.commands.malformed`) a payload of any other size or with an out-of-range field.

| Kind | Payload type | Bytes | Layout |
|---|---|---|---|
| `Streaming.ApplyCellState` | `Zenith.Streaming.ApplyCellStatePayload` | 8 | `uint32` cell index; `uint32` SCENE instance ID |
| `Streaming.BindReferences` | `Zenith.Streaming.CellPayload` | 8 | `uint32` cell index; `uint32` SCENE instance ID |
| `Streaming.UnbindReferences` | `Zenith.Streaming.CellPayload` | 8 | `uint32` cell index; `uint32` SCENE instance ID (of the instance being unloaded) |
| `Streaming.SetCellOverride` | `Zenith.Streaming.OverridePayload` | 16 | `uint8` grid; `uint8` `Zenith_StreamingCellOverride`; `uint16` reserved 0; `sint32` x; `sint32` z; `uint32` reserved 0 |

The instance ID in the first three lets a playback detect that the cell's instance changed between recording and playback (it cannot within one sync point, so a mismatch is a defect counted under `stream.commands.malformed`).

## Stream dump format

`zenith2.stream-dump` version 1, written by `Zenith_StreamingStateView::WriteDump` through `Zenith_JsonWriter` in `JSON_WRITE_STYLE_PRETTY` (one tab per depth, LF endings, one trailing LF from `Finish`; `Zenith2/Docs/Design/CORE/Text.md` → "Byte format"), members in this order. Spelling: every state, band, binding and override name is lower-case: cell states and bands are `Zenith_ToString` of their enumerators (`unloaded` … `failed_permanent`; `required`, `activation`, `keep`, `prefetch`, `none`), overrides are `none`, `force_active` or `force_unloaded`. Numbers are integers or `Zenith_JsonWriter::Double`'s shortest round-trip text (so 0.0 is `0`). `position`, `displayed` and `resident` are inline arrays; `sources` and `cells` are multiline arrays of objects:

| Member | Type | Contents |
|---|---|---|
| `format` | string | `"zenith2.stream-dump"` |
| `version` | integer | 1 |
| `worldTag` | integer | the world's tag |
| `tick` | integer | the world's last executed tick |
| `binding` | object | `{ "state": <one of "none", "bound", "refused", "failed", "unbinding">, "world": <persistent ID or null>, "error": <code text or null> }` |
| `origin` | object | `{ "x", "y", "z": numbers (shortest round-trip doubles), "rebaseCount": integer }` |
| `stats` | object | in this order: `"cellsPerState"` (an object with all ten state names as keys, in enumerator order `unloaded` … `failed_permanent`, each an integer), `"inFlight"`, `"liveInstances"`, `"sources"`, `"hlodDisplayed"`, `"residentBytes"`, `"rebaseCount"` (integers) and `"bound"` (boolean): the fields of `Zenith_StreamingStats` in declaration order |
| `sources` | array | per source in ascending simulation ID: `{ "simulationId", "position": [x, y, z], "primary", "ready", "missingRequired", "teleportSequence" }` |
| `cells` | array | per cell not UNLOADED, in cell index order: `{ "index", "grid", "x", "z", "state", "band", "instance", "retries", "sinceTick", "distance", "deltaBytes", "override" }` (`grid` the grid index, `x` and `z` the cell coordinate, `instance` the SCENE instance ID, `distance` the ordering distance in metres) |
| `hlod` | object | `{ "displayed": [node indices ascending], "resident": [node indices ascending], "gaps": integer }` |
| `snapshots` | object | `{ "slots": integer, "held": integer, "retiring": integer }` |

`Test_StreamingState_DumpShape` compares the bound fixture's dump with its golden after setting five environment-dependent members to 0: `worldTag`, `stats.residentBytes`, each source's `simulationId`, and each cell's `instance` and `sinceTick`. It asserts each of the five separately against what is derivable. The dump is diagnostic: no code reads it back, and its members may grow in a subsequent version (a reader that knows version 1 ignores unknown members).

## In-memory runtime records

Sizes are pinned by `static_assert` and by `Test_StreamingTypes_RecordLayouts`.

### Cell runtime record

`Zenith_StreamingCellRuntime`, 56 bytes, one per cell of the bound world (3.5 MiB at 65,536 cells). The fields marked "persisted" equal the cell's `Zenith.Streaming.CellStatus` property named beside them at the end of every step (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Persisted cell status"); every other field is per-tick, in-flight or diagnostic:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | state (`Zenith_StreamingCellState`; persisted, property 1) |
| 1 | 1 | band of this tick (`Zenith_StreamingBand`, after overrides and dependency closure) |
| 2 | 1 | own band (from sources alone) |
| 3 | 1 | override (`Zenith_StreamingCellOverride`, read each tick from the world reference's slots) |
| 4 | 1 | retries used (persisted, property 3) |
| 5 | 1 | flags: bit 0 wanted by a dependent (pinned), bit 1 unload in progress (persisted, property 6 bit 4: set only at T15, the unload of an ACTIVE cell; cleared by that unload's `Streaming.UnbindReferences` playback, by T17, and as backstops by T16 and at admission, T8), bit 2 load action recorded this tick, bit 3 commit refused, bit 4 has a holder, bit 5 required by a primary source, bit 6 admitted this tick, bit 7 references bound (persisted, property 6 bit 2) |
| 6 | 2 | dependents wanting it this tick (saturating) |
| 8 | 4 | SCENE instance ID (0 when none; persisted, property 2) |
| 12 | 4 | quantised effective distance `min(floor(d_eff × 16), 0xFFFFFFFF)` |
| 16 | 8 | tick of the last state change (diagnostic) |
| 24 | 8 | hook tag (`Zenith_MakeStreamingHookTag`; 0 when no load is in flight) |
| 32 | 8 | tick at which a FAILED cell may retry (persisted, property 7) |
| 40 | 8 | unload due tick (0 none; persisted, property 8) |
| 48 | 1 | consecutive capacity failures (saturating at 255; persisted, property 4) |
| 49 | 1 | consecutive transient failures (saturating at 255; persisted, property 5) |
| 50 | 1 | flags 2: bit 0 release pending (T6 and the rows that release a FAILED instance; persisted, property 6 bit 0), bit 1 preempted ("Loads (step 9)"; persisted, property 6 bit 1), bit 2 externally unloaded this step, bit 3 state applied (set by the `Streaming.ApplyCellState` playback; persisted, property 6 bit 3), bit 4 forced unloaded (an own or inherited FORCE_UNLOADED), bit 5 unload of a QUEUED instance ("Caps"; persisted, property 6 bit 5), bit 6 holder spawn pending, bit 7 delta merge pending (persisted, property 11) |
| 51 | 1 | flags 3: bit 0 has pending tombstones (some entry's `m_auPersistentFlags` bit 1 is set); bit 1 re-home pending (cache of the holder's `CellStatus` property 6 bit 6, "Cell identity across re-cooks"); bit 2 entered UNLOADED this tick; bit 3 holder prune pending (a prune recorded last tick, resolved by holder discovery step 2); bit 4 the cell's holder is written by an action of this tick; other bits 0 |
| 52 | 4 | reserved |

### Source runtime record

`Zenith_StreamingSourceRuntime`, 96 bytes, 16 per world:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | simulation ID |
| 8 | 8 | entity handle bits |
| 16 | 24 | absolute position (three binary64) |
| 40 | 24 | previous tick's absolute position |
| 64 | 8 | forward direction XZ (two binary32, unit length or zero) |
| 72 | 4 | radius scale (clamped) |
| 76 | 4 | grid mask |
| 80 | 1 | priority |
| 81 | 1 | flags: bit 0 primary, bit 1 drives HLOD, bit 2 ready, bit 3 teleported this tick, bit 4 seen this tick |
| 82 | 2 | reserved |
| 84 | 4 | missing required cells |
| 88 | 4 | teleport sequence |
| 92 | 4 | critical boost ticks remaining (presentation, read by the service) |

### Admission entry

`Zenith_StreamingAdmissionEntry`, 24 bytes, at most 32 per tick, sorted by instance ID so the hook binary-searches it:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | SCENE instance ID |
| 4 | 4 | cell index |
| 8 | 12 | root translation (three binary32 metres relative to the current origin) |
| 20 | 4 | entity count |

### Snapshot slot

`Zenith_StreamingSnapshotSlot`, 64 bytes plus its entity table (`Zenith_StreamingEntityTable`, 8,320 bytes: 1,024 tombstone bits and 1,024 add and remove masks, reserved at pool creation), `uSTREAMING_SNAPSHOT_SLOTS` (112) per attached world, aligned to 64 bytes so the atomics of two slots never share a cache line:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | holder mask (`std::atomic<uint8>`: bit 0 simulation, bit 1 preparation) |
| 1 | 1 | gate open (`std::atomic<bool>`) |
| 2 | 1 | entity table filled (`std::atomic<bool>`) |
| 3 | 1 | reserved |
| 4 | 4 | cell index |
| 8 | 8 | hook tag (`Zenith_MakeStreamingHookTag`) |
| 16 | 8 | frame the cell's PSO group was requested (0 when none) |
| 24 | 8 | entity table pointer (`Zenith_StreamingEntityTable`, 8,320 bytes, reserved at pool creation) |
| 32 | 8 | world asset pointer captured at `Acquire` (the bound `Zenith_StreamingWorldAsset`, for the persistent-entry search) |
| 40 | 24 | reserved (padding to the cache line) |

### HLOD runtime record

`Zenith_StreamingHLODRuntime`, 32 bytes, one per HLOD node of the bound world, owned by the presentation state:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | state (ABSENT 0, REQUESTED 1, READY 2, DISPLAYED 3) |
| 1 | 1 | resident mesh mask (bit per mesh slot) |
| 2 | 1 | flags: bit 0 in residency range, bit 1 rebase update pending, bit 2 emitted this frame |
| 3 | 1 | reserved |
| 4 | 4 | first handle slot in the presentation handle pool (4 consecutive), 0xFFFFFFFF when none |
| 8 | 8 | frame of the last request |
| 16 | 4 | distance from the HLOD-driving source this frame (binary32 metres) |
| 20 | 4 | rebase count its emitted transforms were computed with |
| 24 | 8 | hash of the emitted transforms and flags |

### Other per-world records

| Record | Bytes | Count | Purpose |
|---|---|---|---|
| `Zenith_StreamingCellDeltaMirror` | 32 | one per cell | the holder entity's handle (8), the anchor root's handle while the cell is ACTIVE (8, the T28 check), the decoded entity record count (4), the encoded bytes (4), the DESTROYED record count (4), reserved (4); the decoded form is rebuilt from the component when needed |
| per-persistent-entry flags | 1 | one per persistent entry of the bound world | bit 0 committed tombstone, bit 1 pending tombstone ("Tombstones" in Runtime.md; caches of `CellStatus` properties 9 and 10) |
| `Zenith_StreamingRestoredLoad` | 16 | inbox, ≤ `uSTREAMING_SNAPSHOT_SLOTS` (112) | a load SAVE's restore re-requested: SCENE instance ID (4), slot index or 0xFFFFFFFF when the pool refused (4), hook tag (8) |
| `Zenith_StreamingTickCounters` | 608 | one per world | 76 `uint64` fields, no padding: one per CUMULATIVE counter of `Zenith2/Docs/Design/STREAM.md` → "Counters" (58, grouped rows expanded, in table order), then one per single-name GAUGE (18, in table order, `stream.cells.activation_entities` included). The ten `stream.cells.state.<state>` gauges come from the cell table and the two DELTA counters from profiler zones, so neither is stored. The service differences the cumulative fields and publishes the gauges per frame |
| presentation handle pool | 16 per handle | 4 × HLOD nodes in residency range + boosters + prefetches, bounded by `Zenith_Pool` capacity 8,192 | the service's `Zenith_AnyAssetHandle` and `Zenith_OperationHandle` pairs |
