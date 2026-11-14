# TWOD design: formats

Section file of `Zenith2/Docs/Design/TWOD.md`. It specifies the five TWOD authoring documents (their JSON schemas, validation rules and migration), the five cooked TWOD formats, the tile cell encoding shared by CPU and GPU, the tile collision kinds, the TWOD save section and the parser limits. Every authoring document is an authoring-document type of `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` handled by SCENE's framework (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`); every cooked file uses ASSET's container (`Zenith2/Docs/Design/ASSET/Formats.md → "Cooked asset file"`).

## Scope of this file

| Topic | Heading |
|---|---|
| rules every TWOD format obeys | "Common rules" |
| the 32-bit tile cell | "Tile cell encoding" |
| tile collision | "Tile collision kinds" |
| authoring JSON | "Sprite atlas document schema", "Sprite animation document schema", "Tileset document schema", "Tilemap document schema", "Rig document schema" |
| validation | "Validation rules" |
| cooked files | "Cooked sprite atlas format", "Cooked sprite animation format", "Cooked tileset format", "Cooked tilemap format", "Cooked rig format" |
| checks shared by the readers | "Cooked reader checks" |
| saves | "TWOD save section format" |
| document registration and migration | "Document types and migration" |
| limits | "Parser limits" |

## Common rules

- Authoring documents are UTF-8 JSON with `"$schema"` (the type's schema tag), `"schemaVersion"` (u32, 1 for every TWOD type at its first shipped version) and `"id"` (the document's `Zenith_PersistentID`, 36-character form of [Z2-N-CONTRACT-067]); asset references are persistent IDs in the same form; colours are four linear floats `[r, g, b, a]`; texel coordinates are integers from the image's top-left, +Y down; world quantities are metres and radians.
- Every document is written in canonical form (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical serialization"`): object members in the order of the type's canonical schema, map keys in byte order, one array element per line for the arrays the canonical schema marks `lineArray` (atlas sources, animation frames, tileset tiles, tilemap chunks, rig bones and slots), so a text merge of two edits of different elements never conflicts.
- Unknown members are preserved with a warning and never dropped ([Z2-N-CONTRACT-535]).
- Cooked files are little-endian, written field by field (Z2-D-038), with ASSET's 64-byte header and 40-byte section entries; TWOD section kinds are `0x0200` … `0x02FF`; names are stored once in a `STRINGS` section and found through a `NAME_INDEX` section of 16-byte entries `{ uint64 hash; uint32 element; uint32 stringOffset }` sorted by (hash, string bytes), with hash = `Zenith_Hash64` of the UTF-8 name, seed 0; the loader interns every name into the process `Zenith_StringTable` and keeps the interned `Zenith_StringID`s beside the elements, so a name lookup by `Zenith_StringID` is a binary search over the hashes followed by an ID compare.
- Every cooked section is compressed through COMPRESS's section codec when compression helps (`Zenith2/Docs/Design/COMPRESS.md → "Compressed section descriptor on disk"`), and the descriptor records the codec (Z2-D-004).
- Asset references cooked into a TWOD file are listed in a `DEPENDENCIES` section of 16-byte GUIDs and declared as hard dependencies in the catalogue, so the asset system loads them first.

## Tile cell encoding

| Bits | Field |
|---|---|
| 0–15 | tile index: 0 empty, 1 … 65,535 the tileset's tiles |
| 16 | flip X |
| 17 | flip Y |
| 18 | transpose (swap x and y before the flips; with the flips gives the eight square symmetries, so 90° rotations are transpose + flip) |
| 19–31 | zero; a cell with any of these set reads as empty (`twod.tilemap.bad_tile_delta`) |

The same 32 bits are the authored cell, the overlay cell, the `Flux_RenderTilemapChunk` cell and the chunk-pool cell the GPU reads (`Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"`). In-tile coordinates `(u, v)` in [0, 1)² map through transpose, then flip X (`u := 1 − u`), then flip Y.

A cell's identity outside a chunk, where physics contacts, query hits, one-way state and saves name it, is the 64-bit cell key `Zenith_MakeTileCellKey(layer, x, y) = (1 << 63) | (layer << 44) | ((x + 2^20) << 22) | (y + 2^20)` (`Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"`): y in bits 0–21, x in bits 22–43, the layer (0 … 15) in bits 44–47, bits 48–62 zero, bit 63 set so a key is never 0. A tilemap document bounds chunk coordinates to magnitude 2^15 ("Tilemap document schema"), so every cell that can carry collision has x and y in [−2^20, 2^20 + 32) and its key is exact; a cell outside the authored bounds reads empty and never carries collision, so it never needs a key.

## Tile collision kinds

| Value | Name in JSON | Meaning |
|---|---|---|
| 0 | `none` | no collision |
| 1 | `full` | a solid cell, merged into outline chains |
| 2 | `oneWayUp` | the top edge as a one-way chain of normal +Y in tile space (rotated by the cell's symmetry bits) |
| 3 | `custom` | the tile's polygon (3 … 8 vertices, convex, counter-clockwise, in [0, 1]² tile units) as one polygon shape |

## Sprite atlas document schema

`*.spriteatlas.json`, schema tag `zenith2.twod.spriteatlas`:

| Member | Type | Presence | Rule |
|---|---|---|---|
| `filter` | `"point"` or `"linear"` | required | `point` cooks `FLUX_TEXTURE_USAGE_COLOUR_EXACT` pages with one mip; `linear` cooks `FLUX_TEXTURE_USAGE_COLOUR` pages with a full mip chain |
| `pixelsPerUnit` | number | required | 1 … 1,024 |
| `pageSize` | integer | default 2,048 | a power of two, 64 … 4,096: the maximum page extent |
| `padding` | integer | default 2 | 0 … 16 texels between packed frames |
| `extrude` | integer | default 1 | 0 … `padding`: edge texels duplicated outward (stops linear-filter bleeding) |
| `trim` | boolean | default true | transparent borders (alpha 0) are cut off; the trim offset and source size keep the pivot |
| `sources` | array of source objects | required, 1 … 4,096 | each names an image and how to cut it into frames |
| `frames` | object, frame name → override object | optional | per-frame pivot and border overrides |

A source object: exactly one of `image` (the image file's path relative to the document's directory, a PNG, TGA or BMP the importer reads through `Zenith_ImportContext::ReadInput`, so an edit of the image re-imports the atlas; JPEG is refused, because its lossy decode is not pixel-exact) or `generator` (the path of a `*.generator.json` document, read the same way and turned into pixels by `Zenith2/Docs/Design/TWOD/Authoring.md → "Seeded image generator"`), then exactly one of `name` (the whole image is one frame) or `grid` (`{ "cellWidth", "cellHeight", "count", "names" }`: cells taken left to right, top to bottom; `names` a pattern with `{i}` replaced by the cell index from 0), plus `pivot` (`[x, y]` texels from the frame's top-left, each a multiple of 0.5; or `"center"`, `"bottomCenter"`, `"topLeft"`; default `"center"`) and `border` (`[left, top, right, bottom]` nine-slice texels, default 0s).

```json
{
	"$schema": "zenith2.twod.spriteatlas",
	"schemaVersion": 1,
	"id": "2f0c6e1a-7b3d-4e5f-9a10-3c4d5e6f7a81",
	"filter": "point",
	"pixelsPerUnit": 16,
	"pageSize": 1024,
	"padding": 2,
	"extrude": 1,
	"trim": true,
	"sources": [
		{ "image": "Images/hero_run.png", "grid": { "cellWidth": 16, "cellHeight": 24, "count": 8, "names": "hero_run_{i}" }, "pivot": "bottomCenter" },
		{ "image": "Images/panel.png", "name": "panel", "pivot": "center", "border": [4, 4, 4, 4] }
	],
	"frames": { "hero_run_0": { "pivot": [8, 24] } }
}
```

## Sprite animation document schema

`*.spriteanim.json`, schema tag `zenith2.twod.spriteanim`:

| Member | Type | Rule |
|---|---|---|
| `parameters` | array of `{ "name", "kind": "float" \| "int" \| "bool" \| "trigger", "default" }` | 0 … 16; names unique; at most 16 triggers |
| `clips` | object, clip name → clip | 1 … 256 |
| `states` | array of `{ "name", "clip", "speed" (default 1, 0 … 16) }` | 1 … 64; names unique |
| `defaultState` | state name | required |
| `transitions` | array of `{ "from": state name or "*", "to", "conditions": [ { "parameter", "op": ">" \| "<" \| "==" \| "!=" \| "true" \| "false" \| "triggered", "value" } ], "exitTime": number or null, "canTransitionToSelf": bool }` | 0 … 256; at most 4 conditions each; `value` absent for `true`, `false`, `triggered` |

A flipbook clip: `{ "kind": "flipbook", "atlas": id, "loop": "loop" | "once" | "pingPong", "frames": [ { "sprite": name, "ms": 1 … 60,000, "events": [ { "name", "value" } ] } ] }`, 1 … 1,024 frames, at most 8 events per frame. A skeletal clip: `{ "kind": "skeletal", "rig": id, "loop", "durationMs": 1 … 600,000, "bones": { bone name: { "translation" | "rotation" | "scale": [ { "ms", "value", "interp": "linear" | "step" } ] } }, "slots": { slot name: { "attachment" | "colour" | "drawOrder": [ … ] } }, "events": [ { "ms", "name", "value" } ] }`, keys ascending by `ms` within each track, at most 4,096 keys per track. A skeletal clip's events fire for every event time crossed in `(T(k − 1), T(k)]`, by the rule of `Zenith2/Docs/Design/TWOD/Simulation.md → "Animation events"` applied to times instead of frame boundaries.

```json
{
	"$schema": "zenith2.twod.spriteanim",
	"schemaVersion": 1,
	"id": "6b7c8d9e-0f1a-4b2c-9d3e-4f5a6b7c8d9e",
	"parameters": [ { "name": "Speed", "kind": "float", "default": 0 }, { "name": "Jump", "kind": "trigger" } ],
	"clips": {
		"idle": { "kind": "flipbook", "atlas": "2f0c6e1a-7b3d-4e5f-9a10-3c4d5e6f7a81", "loop": "loop", "frames": [ { "sprite": "hero_idle_0", "ms": 200 }, { "sprite": "hero_idle_1", "ms": 200 } ] },
		"run": { "kind": "flipbook", "atlas": "2f0c6e1a-7b3d-4e5f-9a10-3c4d5e6f7a81", "loop": "loop", "frames": [ { "sprite": "hero_run_0", "ms": 80, "events": [ { "name": "Footstep", "value": 0 } ] }, { "sprite": "hero_run_1", "ms": 80 } ] }
	},
	"states": [ { "name": "Idle", "clip": "idle" }, { "name": "Run", "clip": "run", "speed": 1.0 } ],
	"defaultState": "Idle",
	"transitions": [
		{ "from": "Idle", "to": "Run", "conditions": [ { "parameter": "Speed", "op": ">", "value": 0.1 } ], "exitTime": null, "canTransitionToSelf": false },
		{ "from": "Run", "to": "Idle", "conditions": [ { "parameter": "Speed", "op": "<", "value": 0.1 } ], "exitTime": null, "canTransitionToSelf": false }
	]
}
```

## Tileset document schema

`*.tileset.json`, schema tag `zenith2.twod.tileset`:

| Member | Type | Rule |
|---|---|---|
| `atlas` | id | required: the sprite atlas the tiles' frames come from |
| `tileWidth`, `tileHeight` | integer | 4 … 512 texels; every tile frame's untrimmed size equals them |
| `tiles` | array of `{ "sprite", "collision": "none" \| "full" \| "oneWayUp" \| "custom", "polygon": [[u, v], …], "animation": { "frames": [sprite names], "ms" } }` | 1 … 65,535; tile index = array position + 1; `polygon` exactly when `custom`, 3 … 8 vertices; animation frames 2 … 64, `ms` 1 … 60,000 |
| `terrains` | array of `{ "name", "rules": [ { "mask": 0 … 255, "tile": index } ] }` | authoring only (the tilemap painter's auto-tiling, `Zenith2/Docs/Design/TWOD/Authoring.md → "Tilemap painter"`); validated, not cooked |

The mask bits are the eight neighbours N, NE, E, SE, S, SW, W, NW at bits 0 … 7; a corner bit counts only when both adjacent edge bits are set (the 47-tile blob reduction).

## Tilemap document schema

`*.tilemap.json`, schema tag `zenith2.twod.tilemap`:

| Member | Type | Rule |
|---|---|---|
| `tileset` | id | required |
| `cellSize` | `[w, h]` metres | each 0.001 … 1,000 |
| `layers` | array of layer objects | 1 … 16 |

A layer: `{ "name", "id" (persistent ID, the stable authoring ID that overrides key on), "sortingLayerOffset" (−31 … 31), "orderOffset" (sint16), "zOffset" (metres), "parallax": [x, y] (0 … 4, default [1, 1]), "opacity" (0 … 1), "chunks": [ { "x", "y", "cells" } ] }`; `x`, `y` are chunk coordinates (cell / 32, floored), at most 2^15 in magnitude; `cells` is the base64 of 1,024 little-endian `uint32` cells, row-major from the chunk's bottom row (4,096 bytes, 5,464 base64 characters); a chunk whose cells are all empty is omitted; chunks are sorted by (y, x) in canonical form, one per line.

## Rig document schema

`*.rig2d.json`, schema tag `zenith2.twod.rig`:

| Member | Type | Rule |
|---|---|---|
| `atlas` | id | required |
| `pixelsPerUnit` | number | 1 … 1,024 |
| `bones` | array of `{ "name", "parent": name or null, "translation": [x, y], "rotation", "scale": [x, y] }` | 1 … 128; exactly one root (parent null); a parent appears before its children |
| `slots` | array of `{ "name", "bone", "attachment": name or null, "drawOrder", "colour" }` | 0 … 256 |
| `skins` | object, skin name → { slot name → { attachment name → `{ "sprite", "translation", "rotation", "scale" }` } } | 1 … 64 skins; the skin named `default` is required |

## Image generator document schema

`*.generator.json`, schema tag `zenith2.twod.imagegen`:

| Member | Type | Presence | Rule |
|---|---|---|---|
| `width`, `height` | integer | required | 1 … 4,096 |
| `seed` | string | required | a decimal unsigned 64-bit integer (a string because JSON numbers lose precision above 2^53) |
| `filter` | `"point"` or `"linear"` | default `"point"` | the usage class when `twod.imageGenerator` imports the document on its own; ignored when an atlas names it |
| `operations` | array of operation objects | required, 1 … 256 | `{ "op": <name>, … }` with the members of `Authoring.md → "Seeded image generator"`; integer members are pixel counts 0 … 4,096 (radius, cell and edge widths as stated there); colours `"#RRGGBBAA"` |

```json
{
	"$schema": "zenith2.twod.imagegen",
	"schemaVersion": 1,
	"id": "8d1e2f30-4a5b-4c6d-8e7f-9a0b1c2d3e4f",
	"width": 64, "height": 48, "seed": "1234",
	"operations": [ { "op": "sheet", "cellWidth": 16, "cellHeight": 24, "columns": 4, "rows": 2, "outline": "#202020FF" } ]
}
```

## Validation rules

Run by the cook's `--validate-only` mode and by the editor on save; each is an error (the document does not cook) or a warning:

| # | Severity | Rule | Message |
|---|---|---|---|
| 1 | error | a member's type, range or presence differs from its schema table | `<path>: <member>: expected <rule>` |
| 2 | error | a duplicate name among frames, clips, states, parameters, bones, slots or skins | `<path>: duplicate <kind> '<name>'` |
| 3 | error | a reference to a name the document or its referenced asset does not define (a frame, a state, a parameter, a bone, a slot, an attachment) | `<path>: unknown <kind> '<name>'` |
| 4 | error | an asset reference of the wrong kind or to a missing GUID, an atlas `image` path that is missing, outside the content roots or not a PNG, TGA or BMP, or a `generator` path that is missing or not a valid generator document | `<path>: <member>: '<id or path>' is not a <kind>` |
| 5 | error | a frame larger than `pageSize − 2 × padding` in either axis | `<path>: frame '<name>' (<w>×<h>) does not fit a <pageSize> page` |
| 6 | error | the frames need more than 64 pages | `<path>: atlas needs <n> pages (limit 64)` |
| 7 | error | a tile frame whose untrimmed size differs from `tileWidth × tileHeight` | `<path>: tile <i> frame '<name>' is <w>×<h>, expected <tw>×<th>` |
| 8 | error | a custom polygon that is not convex and counter-clockwise, or outside [0, 1]² | `<path>: tile <i> polygon is not a convex counter-clockwise polygon in the unit square` |
| 9 | error | skeletal keys not ascending, or a key time beyond `durationMs` | `<path>: <track>: keys must ascend within 0 … <durationMs>` |
| 10 | error | a bone cycle, two roots, or a parent listed after its child | `<path>: bones: <reason>` |
| 11 | error | a chunk whose `cells` does not decode to 4,096 bytes, or a cell with bits 19–31 set | `<path>: layer '<layer>' chunk (<x>, <y>): <reason>` |
| 12 | warning | a tile index above the tileset's count in a chunk | `<path>: layer '<layer>' chunk (<x>, <y>): tile <t> > <count>; drawn empty` |
| 13 | warning | a `point` atlas whose source image is not 8-bit RGBA | `<path>: source '<path>' is 16-bit; point atlases convert to 8-bit and may lose values` |
| 14 | warning | a flipbook frame from a `point` atlas whose PPU differs from the rig's or another clip frame's atlas PPU | `<path>: mixed pixels per unit (<a> and <b>) in clip '<clip>'` |
| 15 | warning | an unknown member | `MIGRATION_WARNING_UNKNOWN_FIELD` ([Z2-N-CONTRACT-544]) |

`Test_TwoDValidation_EveryRule` holds one failing fixture document per rule, generated in the test, and asserts each rule's message and severity.

## Cooked sprite atlas format

`.zatlas`, magic `ZATL`, kind `ASSET_KIND_SPRITE_ATLAS` (row TD3), format version 1.

| Section kind | Value | Content |
|---|---|---|
| `TWOD_SECTION_ATLAS_INFO` | `0x0201` | 32 bytes: frame count u32; page count u16; filter u8 (`Zenith_SpriteAtlasFilter`); reserved u8; pixels per unit f32; page extent u16; padding u8; extrude u8; reserved to 32 bytes |
| `TWOD_SECTION_ATLAS_FRAMES` | `0x0202` | frame count × 40-byte records: page u16; rect x0, y0, x1, y1 u16 ×4; pivot f32 ×2; border u16 ×4; trim offset u16 ×2; source size u16 ×2; reserved u16 |
| `TWOD_SECTION_NAME_INDEX` | `0x0203` | frame count × 16-byte entries ("Common rules") |
| `TWOD_SECTION_STRINGS` | `0x0204` | UTF-8 names, each `uint16` length then bytes |
| `TWOD_SECTION_DEPENDENCIES` | `0x0205` | page count × 16-byte page texture GUIDs (`ASSET_KIND_TEXTURE`), page i at entry i |

## Cooked sprite animation format

`.zsanim`, magic `ZSAN`, kind `ASSET_KIND_SPRITE_ANIMATION`, format version 1.

| Section kind | Value | Content |
|---|---|---|
| `TWOD_SECTION_ANIM_INFO` | `0x0210` | 32 bytes: parameter count u8; state count u8; default state u8; reserved u8; transition count u16; condition count u16; clip count u16; frame count u32; event count u32; track count u32; key count u32; reserved to 32 |
| `TWOD_SECTION_ANIM_PARAMETERS` | `0x0211` | 8 bytes each: kind u8; reserved u8 ×3; default f32 |
| `TWOD_SECTION_ANIM_STATES` | `0x0212` | 8 bytes each: clip u16; reserved u16; speed f32 |
| `TWOD_SECTION_ANIM_TRANSITIONS` | `0x0213` | 16 bytes each, in authored order: from u8 (0xFF = ANY); to u8; condition first u16; condition count u8; can-self u8; reserved u16; exit time f32 (negative = none); reserved u32 |
| `TWOD_SECTION_ANIM_CONDITIONS` | `0x0214` | 8 bytes each: parameter u8; op u8; reserved u16; threshold f32 |
| `TWOD_SECTION_ANIM_CLIPS` | `0x0215` | 32 bytes each: kind u8 (0 flipbook, 1 skeletal); loop u8 (0 loop, 1 once, 2 ping-pong); dependency u16 (atlas or rig); first u32 (frame or track); count u32; period ms u32 (D, or P for ping-pong); event first u32; event count u32; duration ms u32 (skeletal); reserved u32 |
| `TWOD_SECTION_ANIM_FRAMES` | `0x0216` | 16 bytes each: atlas frame index u32; duration ms u32; prefix ms u32 (start time within the clip); event first u16; event count u16 |
| `TWOD_SECTION_ANIM_EVENTS` | `0x0217` | 12 bytes each: name index u32; value f32; time ms u32 (skeletal; 0 for frame events) |
| `TWOD_SECTION_ANIM_TRACKS` | `0x0218` | 12 bytes each: target u16 (bone or slot index); channel u8 (0 translation, 1 rotation, 2 scale, 3 attachment, 4 colour, 5 draw order); reserved u8; key first u32; key count u32 |
| `TWOD_SECTION_ANIM_KEYS` | `0x0219` | 24 bytes each: time ms u32; interpolation u8 (0 linear, 1 step); reserved u8 ×3; value f32 ×4 (translation/scale use 2, rotation 1, colour 4, attachment and draw order as integers in value 0) |
| `TWOD_SECTION_NAME_INDEX`, `TWOD_SECTION_STRINGS` | `0x0203`, `0x0204` | state names (elements 0 … states − 1), then parameter names, then event names |
| `TWOD_SECTION_DEPENDENCIES` | `0x0205` | atlas and rig GUIDs, in first-use order |

## Cooked tileset format

`.ztileset`, magic `ZTSE`, kind `ASSET_KIND_TILESET`, format version 1.

| Section kind | Value | Content |
|---|---|---|
| `TWOD_SECTION_TILESET_INFO` | `0x0220` | 16 bytes: tile count u32; tile width u16; tile height u16; polygon vertex count u32; animation frame count u32 |
| `TWOD_SECTION_TILESET_TILES` | `0x0221` | tile count × 16 bytes, tile i + 1 at record i: atlas frame index u32; collision kind u8; polygon vertex count u8; animation frame count u8; reserved u8; polygon first u32 (vertex index); animation first u16; animation ms u16 |
| `TWOD_SECTION_TILESET_POLYGONS` | `0x0222` | f32 pairs in tile units |
| `TWOD_SECTION_TILESET_ANIM_FRAMES` | `0x0223` | u32 atlas frame indices |
| `TWOD_SECTION_DEPENDENCIES` | `0x0205` | one GUID: the atlas |

## Cooked tilemap format

`.ztilemap`, magic `ZTMP`, kind `ASSET_KIND_TILEMAP`, format version 1.

| Section kind | Value | Content |
|---|---|---|
| `TWOD_SECTION_TILEMAP_INFO` | `0x0230` | 32 bytes: layer count u8; reserved u8 ×3; cell size f32 ×2; min cell x, min cell y, max cell x, max cell y i32 ×4 (max exclusive); chunk count u32 |
| `TWOD_SECTION_TILEMAP_LAYERS` | `0x0231` | layer count × 40 bytes: name string index u32; layer persistent ID (16 bytes); sorting-layer offset i8; reserved u8; order offset i16; z offset f32; parallax f32 ×2; opacity f32 |
| `TWOD_SECTION_TILEMAP_CHUNK_INDEX` | `0x0232` | chunk count × 16 bytes, sorted by (layer, y, x): layer u8; reserved u8 ×3; chunk x i32; chunk y i32; cell block index u32 |
| `TWOD_SECTION_TILEMAP_CELLS` | `0x0233` | chunk count × 4,096 bytes: each chunk's 1,024 cells |
| `TWOD_SECTION_STRINGS` | `0x0204` | layer names |
| `TWOD_SECTION_DEPENDENCIES` | `0x0205` | one GUID: the tileset |

`GetAuthoredCell` finds a chunk by binary search over the chunk index: O(log c).

## Cooked rig format

`.zrig2d`, magic `ZR2D`, kind `ASSET_KIND_RIG_2D`, format version 1.

| Section kind | Value | Content |
|---|---|---|
| `TWOD_SECTION_RIG_INFO` | `0x0240` | 16 bytes: bone count u8; reserved u8; slot count u16; skin count u8; reserved u8; attachment count u16; pixels per unit f32; reserved u32 |
| `TWOD_SECTION_RIG_BONES` | `0x0241` | 24 bytes each: parent u8 (0xFF root); reserved u8 ×3; translation f32 ×2; rotation f32; scale f32 ×2 |
| `TWOD_SECTION_RIG_SLOTS` | `0x0242` | 24 bytes each: bone u8; reserved u8; draw order u16; default attachment u32 (an attachment-name string index, 0xFFFFFFFF none); colour f32 ×4 |
| `TWOD_SECTION_RIG_ATTACHMENTS` | `0x0243` | 32 bytes each, sorted by (skin, slot, name string): skin u8; reserved u8; slot u16; name string u32; atlas frame u32; translation f32 ×2; rotation f32; scale f32 ×2 (packed as the last two words: x, y) |
| `TWOD_SECTION_NAME_INDEX`, `TWOD_SECTION_STRINGS` | `0x0203`, `0x0204` | bone names (elements 0 … bones − 1), then slot names, then skin names, then attachment names |
| `TWOD_SECTION_DEPENDENCIES` | `0x0205` | one GUID: the atlas |

## Cooked reader checks

Every TWOD kind loader first runs ASSET's container checks (C1–C12 of `Zenith2/Docs/Design/ASSET/Formats.md → "Checks"`), then these, in order; the first failure fails the load with `TWOD_ERROR_CORRUPT_ASSET` and the check number in `m_uDetail` (`TWOD_ERROR_VERSION_MISMATCH` for check 1), never an assert:

| # | Check |
|---|---|
| 1 | magic, kind and format version equal the kind's (a defence; ASSET checked them) |
| 2 | every required section of the kind's table is present exactly once |
| 3 | each section's decoded size equals its count times its record size (from the INFO counts) |
| 4 | every count is within "Parser limits" |
| 5 | every index (page, frame, clip, state, parameter, condition, event, track, key, bone, slot, attachment, string, dependency, chunk block) is below its table's count |
| 6 | prefix times ascend, durations are non-zero, keys ascend within each track, periods equal the sums |
| 7 | every string is valid UTF-8 within the strings section; the name index is sorted by (hash, bytes) with no duplicate |
| 8 | bone parents precede their children; exactly one root |
| 9 | polygons convex and counter-clockwise in [0, 1]²; tile cells with bits 19–31 clear |
| 10 | atlas frames inside their page's extent; pivots finite |

`Test_TwoDFormats_ReaderChecks` corrupts each field a check covers in a cooked fixture generated at test time and asserts the check number; `Zenith_Fuzz_TwoDAsset` (`Zenith2/Engine/Zenith2D/Tests/Fuzz/TwoDAsset.cpp`) feeds arbitrary bytes to every TWOD reader after `Zenith_ParseCookedHeader`.

## TWOD save section format

The participant section of `Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation"`, section version 1, inside SAVE's participant framing (`Zenith2/Docs/Design/SAVE/Formats.md → "Participant section framing"`). Little-endian, written field by field; every record list is in the order stated, and a list out of order is `SAVE_ERROR_CORRUPT`. A seam ID is two `uint32` (slot, generation). The 80-byte header:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `TDSV` |
| 4 | 2 | version 1 |
| 6 | 2 | flags: bit 0 the backend state is EXACT (a blob follows and the module tables carry seam IDs); bit 1 the backend is candidate J (0: B); bits 2–15 zero |
| 8 | 4 | overlay chunk count O |
| 12 | 4 | body record count B |
| 16 | 4 | shape record count S |
| 20 | 4 | joint record count K |
| 24 | 4 | map record count M |
| 28 | 4 | chain index entry count C |
| 32 | 4 | pair record count P |
| 36 | 4 | one-way disabled pair count D |
| 40 | 4 | pending operation count Q |
| 44 | 4 | collision dirty key count R |
| 48 | 4 | posed entity count E |
| 52 | 8 | gravity f32 ×2 |
| 60 | 8 | ground body seam ID (zero when flag bit 0 is clear) |
| 68 | 8 | ground body recorded position `g` f32 ×2 (the binary32 `−ΣS` of `Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D model"`; each component finite) |
| 76 | 4 | reserved, zero |

An EXACT restore restores the ground body's seam ID (the backend blob restores the body itself) and `g`; a COHERENT restore ignores both, keeping the ground body attach created at the origin with `g` = 0 (`Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation"`). A non-finite `g`, or a non-zero reserved word, is `SAVE_ERROR_CORRUPT`. The header grew from 64 to 80 bytes before any TWOD save shipped (the participant is an S3 deliverable), so section version 1 is the 80-byte form and no migration exists.

The lists, in this order:

| List | Order | Record |
|---|---|---|
| O overlay chunks | ascending (map simulation ID, layer, chunk y, chunk x) | map simulation ID u64, layer u8, reserved u8 ×3, chunk x i32, chunk y i32, revision u32, edited bits u32 ×32, cells u32 ×1,024: 4,248 bytes |
| B body records | ascending simulation ID | simulation ID u64; body seam ID (8); type u8; reserved u8 ×3; written local position f32 ×3; written local rotation f32 ×4 (x, y, z, w); written linear velocity f32 ×2; written angular velocity f32; reserved u32: 64 bytes |
| S shape records | ascending collider simulation ID | collider simulation ID u64; owner body simulation ID u64; shape seam ID (8): 24 bytes |
| K joint records | ascending joint simulation ID | joint simulation ID u64; joint seam ID (8); body A simulation ID u64; body B simulation ID u64 (0: the world's ground body); broken u8; reserved u8 ×7: 40 bytes |
| M map records | ascending map simulation ID | map simulation ID u64; tilemap asset GUID (16); recorded world position f32 ×3; layer count L u8; reserved u8 ×7; then L static body seam IDs (8 each, layer order; zero for a layer without collision): 44 + 8L bytes |
| C chain index entries | ascending (map simulation ID, layer, chunk y, chunk x) | map simulation ID u64, layer u8, reserved u8 ×3, chunk x i32, chunk y i32, chain count H u32 (24 bytes); then H chains: shape seam ID (8), unit-edge count G u32, reserved u32, then G cell keys u64: 16 + 8G bytes each |
| P pair records | ascending (low, high) | low simulation ID u64, high simulation ID u64, touching count u32, flags u32 (bit 0 sensor pair): 24 bytes |
| D one-way disabled pairs | ascending (platform, platform sub-key, other) | platform collider simulation ID u64, platform sub-key u64 (a cell key, or 0), other collider simulation ID u64: 24 bytes |
| Q pending operations | list order | kind u8 (`Zenith_Body2DCommandKind`), reserved u8 ×7, simulation ID u64, vector f32 ×2, point f32 ×2, scalar f32, reserved u32: 40 bytes |
| R collision dirty keys | ascending | map simulation ID u64, layer u8, reserved u8 ×3, chunk x i32, chunk y i32, reserved u32: 24 bytes |
| E posed entities | ascending simulation ID | simulation ID u64, bone count u16, slot count u16, reserved u32 (16 bytes); then the bone matrices (`Zenith_Affine2D`, f32 ×6, 24 bytes each) and the slots (`Zenith_Slot2DPose`, 12 bytes each, draw order): 16 + 24 × bones + 12 × slots bytes |
| the backend blob | — | length u32 then bytes; length 0 exactly when flag bit 0 is clear |

When flag bit 0 is clear the seam-ID fields are written as zero and ignored at restore. A cell key is `Zenith_MakeTileCellKey` of `Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"`; a key whose bit 63 is clear in a chain table, or whose bits 48–62 are not zero, is `SAVE_ERROR_CORRUPT`. Every count is checked against "Parser limits" before use; a violation is `SAVE_ERROR_CORRUPT` (`Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"`). `Zenith_Fuzz_TwoDSaveSection` and `Test_TwoDSave_CorruptSectionRefused` cover every field.

## Document types and migration

| Stable name | Schema tag | Suffix | Version | Registered by |
|---|---|---|---|---|
| `Zenith.TwoD.SpriteAtlasDocument` | `zenith2.twod.spriteatlas` | `.spriteatlas.json` | 1 | `Zenith_RegisterTwoDDocumentTypes` |
| `Zenith.TwoD.SpriteAnimationDocument` | `zenith2.twod.spriteanim` | `.spriteanim.json` | 1 | same |
| `Zenith.TwoD.TilesetDocument` | `zenith2.twod.tileset` | `.tileset.json` | 1 | same |
| `Zenith.TwoD.TilemapDocument` | `zenith2.twod.tilemap` | `.tilemap.json` | 1 | same |
| `Zenith.TwoD.RigDocument` | `zenith2.twod.rig` | `.rig2d.json` | 1 | same |
| `Zenith.TwoD.ImageGeneratorDocument` | `zenith2.twod.imagegen` | `.generator.json` | 1 | same |

- `Zenith_RegisterTwoDDocumentTypes(Zenith_MigrationRegistry&)` lives in the `ZenithAuthoring` tool library (`Zenith2/Docs/Design/TWOD/Authoring.md → "ZenithAuthoring functions"`), called by ZenithCook's and the editor's compositions before `Freeze`; each type has a `Zenith_DocumentTypeDesc` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`) with its canonical schema and no embedded field sets.
- Version 1 has no migration step; each subsequent version adds exactly one step (`Zenith_PfnMigrationStep`, pure and deterministic, [Z2-N-CONTRACT-538]) and its fixtures in the same diff ([Z2-N-CONTRACT-549]).
- Fixtures: `Zenith2/Tools/Authoring/Tests/TwoD/Migration/<Type>/v1.json` and `v1.expected.json` per type (text documents, committed by hand: spec-derivable); `Test_TwoDMigration_Fixtures` migrates each to the current version and compares.
- The six types join the authoring-document list of [Z2-N-CONTRACT-552] through the core-doc batch (row TD13, which names six types).

## Parser limits

| Parser | Limits (a violation is a recoverable error) |
|---|---|
| atlas document | sources 4,096; frames 65,535; pages 64; page extent 4,096; grid count 4,096 per source |
| animation document | parameters 16; states 64; transitions 256; conditions 4 per transition; clips 256; frames 1,024 per clip; events 8 per frame and 256 per skeletal clip; keys 4,096 per track; tracks 384 bone tracks + 768 slot tracks per clip |
| tileset document | tiles 65,535; polygon vertices 8; animation frames 64 per tile; terrains 64; rules 256 per terrain |
| tilemap document | layers 16; chunks 16,384 per map (every layer); chunk coordinate magnitude 2^15; cells base64 exactly 5,464 characters |
| rig document | bones 128; slots 256; skins 64; attachments 1,024 per skin |
| generator document | width and height 4,096; operations 256; the generated image 64 MiB (4,096 × 4,096 × 4) |
| cooked TWOD files | the same counts; strings 1,048,576 bytes; names 255 bytes each |
| TWOD save section | overlay chunks 65,536; body records 65,536; shape records 262,144; joint records 65,536; map records 65,536; layers 16 per map record; chain index entries 262,144; chains 262,144 in total; unit edges 8,192 per chain; pair records 262,144; one-way disabled pairs 262,144; pending operations 65,536; collision dirty keys 65,536; posed entities 65,536; bones 128 and slots 256 per posed entity; backend blob 256 MiB |

The rows join `Zenith2/Docs/Architecture.md → "Security architecture"` ("Parser limits") through the core-doc batch (row TD14); each limit is a constant in Zenith2D or ZenithAuthoring, tested at the limit and at limit + 1 ([Z2-N-ARCH-953]) by `Test_TwoDFormats_LimitsAtAndAbove`.
