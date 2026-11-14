# SCENE design — document and cooked formats

This file is part of the SCENE design family; the entry document is `Zenith2/Docs/Design/SCENE.md`. It specifies the scene and prefab authoring documents (JSON, Z2-D-038) in full, their canonical form, the cooked scene and prefab payloads, and SCENE's diagnostic dump. The migration journal and backups are `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Journal format" and "Backups".

## Files and virtual paths

| File | Kind | Location | Cooked to |
|---|---|---|---|
| `<Name>.scene.json` | scene document | any content root of a game, conventionally `Games/<Game>/Scenes/` | `ASSET_KIND_SCENE` (code 5, stable name `Zenith.Scene`, extension `.zscene`) |
| `<Name>.prefab.json` | prefab document | any content root, conventionally `Games/<Game>/Assets/Prefabs/` | `ASSET_KIND_PREFAB` (code 6, stable name `Zenith.Prefab`, extension `.zprefab`) |

- A scene's virtual path is MOD's mapping of its game-relative path (`Zenith2/Docs/Design/MOD.md` → "Startup scene and loose content root"): `Scenes/Main.scene.json` is `content:/Scenes/Main.scene.json`, and that virtual path is how `content.startupScene` and `Zenith_SceneLoadDesc` name a scene; the asset registry maps it to the cooked asset.
- The document's `id` is its asset GUID; references between documents (a prefab instance naming its prefab, a variant naming its base) use that ID, never the path, so renaming or moving a document breaks nothing (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Reference repair").

The asset side is ASSET's, and SCENE conforms to it (cross-area row AS11 answers S7):

- The two kinds, their codes, magics, extensions, fallback policy (FAIL) and cacheability (prefabs cacheable, scenes not) are rows 5 and 6 of `Zenith2/Docs/Design/ASSET.md` → "Asset kinds"; `Zenith_Scene` and `Zenith_Prefab` declare `static constexpr Zenith_AssetKind eASSET_KIND` accordingly (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneAssets header"), which `Zenith_AssetHandle<T>` requires (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetHandle").
- A `content:/` virtual path of a source document resolves to the asset of its `id` through `Zenith_AssetRegistry::AcquireByLogicalPath`, and a persistent ID through `Acquire` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetRegistry").
- The cooked bytes are ASSET's cooked asset file with SCENE's sections (§ "Cooked scene payload"); they reach SCENE's decoder through SCENE's kind loaders, registered with `RegisterKindLoader` before `CloseRegistration` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader").
- Prefab dependencies are hard catalogue dependencies and derived-data key inputs of the scenes that instance them (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Cooking resolved prefabs").

## Scene document schema

Version 1. Every key not listed is an unknown member: kept, warned and re-emitted in position ([Z2-N-CONTRACT-543]).

### Root object

| Key | Type | Required | Meaning |
|---|---|---|---|
| `$schema` | string | yes | `zenith2.scene` |
| `schemaVersion` | integer | yes | 1 |
| `id` | persistent ID string | yes | the scene's GUID and asset ID |
| `name` | string (1–128 bytes) | yes | display name |
| `entities` | ID map of entity objects | yes (may be `{}`) | the scene's own entities, keyed by authoring ID |
| `instances` | ID map of prefab-instance objects | yes (may be `{}`) | prefab instances, keyed by instance ID |

### Entity object

| Key | Type | Required | Meaning |
|---|---|---|---|
| `name` | string (0–128 bytes) | yes | becomes `Zenith.EntityName` (an empty string adds no component) |
| `parent` | persistent ID string or `null` | yes | an entity of the document's resolved ID space (§ "Resolved ID space"), or null for a root |
| `sortKey` | string | yes | fractional index among siblings (§ "Sort keys") |
| `components` | keyed map (stable name → component object) | yes (may be `{}`) | the entity's components |

### Component object

| Key | Type | Required | Meaning |
|---|---|---|---|
| `version` | integer | yes | the component type's schema version the fields were written at |
| `fields` | field set | yes | ECS's JSON field-set form: keys are decimal property IDs, values in the default JSON form (`Zenith2/Docs/Design/ECS/Reflect.md` → "JSON field-set form") |

A component whose stable name the registry does not know is kept as an opaque object (§ "Unknown components"); a component whose `version` is older than the registry's migrates through ECS's chain at open; newer opens the document read-only.

### Prefab-instance object

| Key | Type | Required | Meaning |
|---|---|---|---|
| `prefab` | persistent ID string | yes | the prefab document's `id` |
| `hint` | string | no | the prefab's game-relative path when last saved; informative only, updated by reference repair |
| `parent` | persistent ID string or `null` | yes | parent of the instance's root, in the document's resolved ID space |
| `sortKey` | string | yes | as for entities |
| `overrides` | override block | yes (may be `{}`) | § "Override block" |

### Override block

The same shape serves a scene's prefab instance, a prefab's nested instance and a variant's changes to its base. Target IDs are IDs of the instanced (or base) prefab's resolved ID space, never of the containing document's.

| Key | Type | Meaning |
|---|---|---|
| `removedEntities` | ordered array of target IDs, sorted ascending | entities of the prefab not created, with their whole subtree |
| `removedComponents` | ID map: target → ordered array of stable names, sorted | components of prefab entities not created |
| `addedComponents` | ID map: target → keyed map of component objects | components added to prefab entities |
| `properties` | ID map: target → keyed map: stable name → partial component object (`version`, `fields` holding only the overridden property IDs) | property overrides |

An absent key is an empty collection. The `fields` of a partial component object override exactly the property IDs present; a struct property's nested field set overrides exactly its nested IDs present; a fixed array or a container property is overridden whole ([Z2-N-CONTRACT-546]: the key of every override is the target's persistent ID plus the property-ID path).

### Prefab document schema

| Key | Type | Required | Meaning |
|---|---|---|---|
| `$schema` | string | yes | `zenith2.prefab` |
| `schemaVersion` | integer | yes | 1 |
| `id` | persistent ID string | yes | the prefab's GUID |
| `name` | string | yes | display name |
| `root` | persistent ID string | yes | the prefab's single root in its resolved ID space |
| `variantOf` | persistent ID string or `null` | yes | the base prefab of a variant |
| `base` | override block | yes when `variantOf` is set, absent otherwise | the variant's changes to its base |
| `entities` | ID map of entity objects | yes | entities the prefab (or variant) adds |
| `instances` | ID map of prefab-instance objects | yes | nested prefab instances |

### Resolved ID space

A document's resolved ID space is the set of IDs its resolution produces (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Resolution algorithm"): its own entity IDs, each variant base's IDs unchanged, and for each nested instance J the derived IDs `Zenith_DeriveAuthoringID(J, t)` of J's prefab's resolved IDs t. A `parent`, a `root` or an ENTITY-typed field value of the document names an ID of that space; an override's target and an override's ENTITY-typed value name IDs of the instanced prefab's space and of the containing document's space respectively. Such an ID is a stored derived ID whenever it names an entity of a nested instance, which is why the derivation is a permanent format property (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation").

### Sort keys

A `sortKey` is 1 to 64 characters of the 62-digit alphabet `0-9A-Za-z`, never ending in `0`. Digit values are 0 to 61 in that order, which is also byte order (`0` < `9` < `A` < `Z` < `a` < `z`), and a key reads as the base-62 fraction `0.d1 d2 … dn`. Because no key ends in `0`, byte-order comparison of two keys equals numeric comparison of their fractions (a proper prefix is the smaller key, since the longer key has a non-zero digit after it), and two distinct keys never denote one value. Siblings (the entities and instances with one `parent`, including `null`) are ordered by sort key, then by ID. Sort keys order the editor's outliner and the cooked activation order; they do not reach ECS, whose child order is by stable simulation ID (`Zenith2/Docs/Design/ECS/Backends.md` → "Entity table").

Two siblings with equal keys are valid and canonical: they are ordered by ID, the load reports `DOCUMENT_WARNING_DUPLICATE_SORT_KEY` once per sibling group naming the parent and both IDs, and no write rewrites them until an edit places an object into that group (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Sort keys for edits"). `DOCUMENT_WARNING_NOT_CANONICAL` is never used for them, because their bytes may already be canonical. A key that is empty, longer than 64 characters, holds a character outside the alphabet or ends in `0` fails validation pass 1 with `SCENE_ERROR_SCHEMA`.

### Sort key insertion algorithm

<!-- elaboration -->
`Zenith_MakeSortKeyBetween(strBefore, strAfter, xOut)` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header") appends a key K with `strBefore < K < strAfter`. An empty `strBefore` is the lower bound 0 (insert first); an empty `strAfter` is the upper bound 1 (insert last). Preconditions, each `SCENE_ERROR_SCHEMA` when violated: each non-empty bound is a valid key, and `strBefore < strAfter` when both are non-empty. Let `v(c)` be a digit's value and `a[i]` the digit of `strBefore` at position i.

1. Set `i = 0` and `bUpperIsOne = strAfter.IsEmpty()`.
2. Loop:
   1. `da = i < len(strBefore) ? v(strBefore[i]) : 0`.
   2. `db = bUpperIsOne ? 62 : (i < len(strAfter) ? v(strAfter[i]) : 0)`.
   3. `da == db`: emit digit `da`; `i += 1`; continue. (Only reachable while both bounds are real keys sharing a prefix, or while `strBefore` is exhausted and `strAfter` has a `0` digit at i.)
   4. `db − da ≥ 2`: emit digit `(da + db) / 2` (integer division; at least `da + 1`, so never `0`); stop.
   5. `db − da == 1` (adjacent): emit digit `da`; set `bUpperIsOne = true` (every key that continues with this prefix is below `strAfter`); `i += 1`; continue.
3. If the emitted key is longer than `uSCENE_MAX_SORT_KEY_CHARS` (64), fail `SCENE_ERROR_LIMIT` and leave `xOut` as it was; otherwise append it.

Properties, each asserted by `Test_SortKey_BetweenIsStrictlyOrderedAndBounded` over 100,000 random pairs and the edge cases (`"" , ""` gives `V`; `"", "1"` gives `0V`; `"z", ""` gives `zV`; `"a", "b"` gives `aV`; `"a", "a1"` gives `a0V`): the result is strictly between the bounds, never ends in `0` (step 2.4 emits a non-zero digit, and every other exit passes through it), and has at most `max(len(strBefore), len(strAfter)) + 1` characters, because each loop iteration consumes one position of the longer bound until step 2.4 stops it. The loop is O(result length).

`Zenith_MakeEvenSortKeys(uCount, xArena, astrOut)` (same header) is the rebalance: the smallest width w ≥ 1 with `62^w ≥ uCount + 1`; key j (1-based, j = 1 … uCount) is `floor(j × 62^w / (uCount + 1))` written as w base-62 digits with trailing `0` digits removed. The values are distinct and ascending because their spacing `62^w / (uCount + 1)` is at least 1, and non-zero because j ≥ 1; w ≤ 4 for every sibling count the document limits admit (62^4 = 14,776,336 > 1,048,577), and the arithmetic fits `uint64`. `uCount` above the entity limit is `SCENE_ERROR_LIMIT`. `Test_SortKey_ExhaustionFailsAndRebalanceOrders` asserts a 64-character exhaustion, the rebalance of 1,000 siblings, and that a rebalance followed by insertions never exhausts before 60 nested insertions at one point.

### Document limits

| Limit | Value | Error |
|---|---|---|
| file size | 64 MiB (the JSON parser limit) | `DOCUMENT_ERROR_TOO_LARGE` |
| entities per document | 1,048,576 | `SCENE_ERROR_LIMIT` |
| prefab instances per document | 65,536 | `SCENE_ERROR_LIMIT` |
| components per entity | 64 (`uECS_MAX_COMPONENTS_PER_ENTITY`) | `SCENE_ERROR_LIMIT` |
| hierarchy depth | 64 (`uSCENE_MAX_HIERARCHY_DEPTH`) | `SCENE_ERROR_HIERARCHY_TOO_DEEP` |
| prefab nesting depth | 16 (`uSCENE_MAX_PREFAB_NESTING`) | `SCENE_ERROR_PREFAB_TOO_DEEP` |
| variant chain length | 16 (`uSCENE_MAX_VARIANT_CHAIN`) | `SCENE_ERROR_PREFAB_TOO_DEEP` |

### Document validation

The loader (`Zenith_SceneDocument::Load`, `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header") validates in this order and reports every violation of a pass before stopping (each as `<path>:<line>:<column>: <keyPath>: <message>` in log category `scene.document`):

1. Shape: every required key present with its type; `$schema` matches the file suffix; every `sortKey` is a valid key (§ "Sort keys").
2. IDs: every key of `entities` and `instances` is a canonical, non-null persistent ID; no ID appears twice across both maps and the document `id` (`SCENE_ERROR_DUPLICATE_ID`).
3. Components: stable names are non-empty; `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` are refused (derived, never authored); known components' field sets decode (`Zenith_ReadFieldSetJson`), unknown field IDs are kept with `DOCUMENT_WARNING_UNKNOWN_FIELD`.
4. Hierarchy: every non-null `parent` resolves in the resolved ID space; no cycle; depth ≤ 64; a prefab has exactly one root, equal to `root`.
5. Prefab references: every `prefab` and `variantOf` names a prefab the provider can supply; no cycle through instances or variants (`SCENE_ERROR_PREFAB_CYCLE`).

### Example scene document

The Stage 2 3D template scene `Games/Harbour/Scenes/Main.scene.json` as `ZenithNewGame create Harbour --template 3D` writes it (IDs generated at creation; the rotations are `Zenith_MakeLookRotation` of the camera toward the origin and the sun at 50° elevation, values shown to their shortest round-trip text):

```json
{
	"$schema": "zenith2.scene",
	"schemaVersion": 1,
	"id": "6f1c0e0a-2b1d-4c7e-9a41-7d3b5e2a9c10",
	"name": "Main",
	"entities": {
		"3a9d4e2b-8c1f-4a6d-b2e7-0f5c9d1e4a83": {
			"name": "Camera",
			"parent": null,
			"sortKey": "a",
			"components": {
				"Zenith.LocalTransform": {
					"version": 1,
					"fields": {
						"1": [0, 1.7, -5],
						"2": [0, 0, 0, 1],
						"3": [1, 1, 1]
					}
				}
			}
		},
		"9b2e7c41-5d0a-4f3e-8c6b-2a1d9e7f0b54": {
			"name": "Sun",
			"parent": null,
			"sortKey": "b",
			"components": {
				"Zenith.LocalTransform": {
					"version": 1,
					"fields": {
						"1": [0, 0, 0],
						"2": [-0.42261827, 0, 0, 0.90630776],
						"3": [1, 1, 1]
					}
				}
			}
		}
	},
	"instances": {}
}
```

The camera's rotation is the identity because the model front is +Z and the camera at z = −5 looks toward the origin along +Z; a camera component (GAME's module) that looks along its local −Z is added with its module and supplies its own view convention (`Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"). MOD's scaffold holds the three templates as literal texts of this form (`Zenith2/Docs/Design/MOD/ProjectCreation.md` → "The written scene"; cross-area note M18).

### Example prefab instance and variant

```json
{
	"$schema": "zenith2.prefab",
	"schemaVersion": 1,
	"id": "1d7a3c90-4e2b-4f81-a6c5-8b0e3d9f2a17",
	"name": "Lamp",
	"root": "5e0b8d21-7a3c-4c9e-b1f4-6d2a0e8c7b35",
	"variantOf": null,
	"entities": {
		"5e0b8d21-7a3c-4c9e-b1f4-6d2a0e8c7b35": {
			"name": "Lamp",
			"parent": null,
			"sortKey": "a",
			"components": {
				"Zenith.LocalTransform": { "version": 1, "fields": { "1": [0, 0, 0], "2": [0, 0, 0, 1], "3": [1, 1, 1] } }
			}
		},
		"c4f2a917-0b6e-4d3a-9e8c-1a7b5d2e0f69": {
			"name": "Bulb",
			"parent": "5e0b8d21-7a3c-4c9e-b1f4-6d2a0e8c7b35",
			"sortKey": "a",
			"components": {
				"Zenith.LocalTransform": { "version": 1, "fields": { "1": [0, 2.1, 0], "2": [0, 0, 0, 1], "3": [1, 1, 1] } }
			}
		}
	},
	"instances": {}
}
```

(The inline component objects above abbreviate the multi-line canonical form; the canonical writer places each `fields` member on its own line as in the scene example.)

An instance of `Lamp` in a scene, moved and with its bulb removed:

```json
"instances": {
	"8a1e5c3d-2f7b-4e90-b4d6-0c9a3f1e7d28": {
		"prefab": "1d7a3c90-4e2b-4f81-a6c5-8b0e3d9f2a17",
		"hint": "Assets/Prefabs/Lamp.prefab.json",
		"parent": null,
		"sortKey": "c",
		"overrides": {
			"removedEntities": ["c4f2a917-0b6e-4d3a-9e8c-1a7b5d2e0f69"],
			"properties": {
				"5e0b8d21-7a3c-4c9e-b1f4-6d2a0e8c7b35": {
					"Zenith.LocalTransform": { "version": 1, "fields": { "1": [4, 0, 2] } }
				}
			}
		}
	}
}
```

A variant `TallLamp` of `Lamp` has `"variantOf": "1d7a3c90-…"`, a `base` block overriding the bulb's position (`"1": [0, 3.4, 0]`) and may add entities parented under base IDs; its entities keep `Lamp`'s IDs, so every override of a `Lamp` instance remains valid on a `TallLamp` instance.

## Scene canonical schema

`Zenith_GetSceneCanonicalSchema()` and `Zenith_GetPrefabCanonicalSchema()` (`Zenith2/Engine/ZenithScene/Zenith_SceneDocumentTypes.h`) return static `Zenith_CanonicalSchema` tables; the rule layout:

| Rule | Node | Members in canonical order, or element rule |
|---|---|---|
| 0 scene root | OBJECT | `$schema`, `schemaVersion`, `id`, `name`, `entities` (rule 1), `instances` (rule 5) |
| 0′ prefab root | OBJECT | `$schema`, `schemaVersion`, `id`, `name`, `root`, `variantOf`, `base` (rule 7), `entities` (rule 1), `instances` (rule 5) |
| 1 entities | ID_MAP | element rule 2 |
| 2 entity | OBJECT | `name`, `parent`, `sortKey`, `components` (rule 3) |
| 3 components | KEYED_MAP | element rule 4 (an unregistered stable name is written OPAQUE) |
| 4 component | OBJECT | `version`, `fields` (FIELD_SET) |
| 5 instances | ID_MAP | element rule 6 |
| 6 instance | OBJECT | `prefab`, `hint`, `parent`, `sortKey`, `overrides` (rule 7) |
| 7 override block | OBJECT | `removedEntities` (INLINE_ARRAY when ≤ 4 IDs, else ORDERED_ARRAY), `removedComponents` (ID_MAP of arrays), `addedComponents` (ID_MAP of rule 3), `properties` (ID_MAP of rule 3 with partial component objects) |

Inside `fields`, vectors, quaternions and colours are INLINE_ARRAY; fixed arrays of scalars are INLINE_ARRAY; fixed arrays of structs and containers of structs are ORDERED_ARRAY of FIELD_SET. `removedEntities` and each `removedComponents` array are sorted on write (a set, not a sequence).

## Unknown components

A component object whose stable name is not in the type registry is kept as an opaque subtree of the document tree: never decoded, never migrated, written back with `CANONICAL_NODE_OPAQUE` (raw numbers, member order as read), so a document round-trips byte for byte through a tool that lacks the type (`Test_SceneDocument_UnknownComponent_RoundTripsByteIdentical`). Each load reports `DOCUMENT_WARNING_UNKNOWN_COMPONENT` once per (document, stable name) with the count of entities carrying it. An unknown component on a prefab entity survives instancing in the resolved data as `Zenith_UnknownComponentData` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneData header"); ENTITY references inside it are not remapped (they cannot be found without the schema), which the warning's text states.

## Cooked scene payload

A cooked `Zenith.Scene` or `Zenith.Prefab` asset is a cooked asset file of ASSET's container (`Zenith2/Docs/Design/ASSET/Formats.md` → "Cooked asset file"; [Z2-N-CONTRACT-527]). Its 64-byte common header carries the kind's magic (`ZSCN`, 0x4E43535A, for `ASSET_KIND_SCENE`; `ZPFB`, 0x4246505A, for `ASSET_KIND_PREFAB`), SCENE's format version `uSCENE_COOKED_FORMAT_VERSION` (1), the kind, the document's persistent ID as the asset GUID and the cook item's content hash; its section table (`Zenith2/Docs/Design/ASSET/Formats.md` → "Section table") carries SCENE's sections below, each with its COMPRESS descriptor, so the codec of every section is recorded in the table ASSET owns. ZenithAssets validates the header and the table and decodes every section a loader requests before SCENE sees a byte; SCENE parses the container itself only in `Zenith_DecodeCookedSceneFile` (tools, tests and the fuzz target), which calls ASSET's `Zenith_ParseCookedHeader` first (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_CookedFile header"). Inside a section every integer is little-endian, and every offset and count is validated against the section's decoded length with overflow-checked arithmetic before use ([Z2-N-ARCH-951]).

### Scene header section

Section kind `SCENE_SECTION_HEADER` (1), index 0, stored uncompressed, exactly 16 bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | `uint32` | entity count E (≤ 1,048,576) |
| 4 | `uint32` | prefab root entity index: 0 for a prefab, 0xFFFFFFFF for a scene |
| 8 | `uint32` | flags: bit 0 prefab (must equal the header's kind being `ASSET_KIND_PREFAB`); bit 1 has an UNKNOWN section; every other bit 0 |
| 12 | `uint32` | reserved, 0 |

### Sections

| Kind | Value | Content (the decoded bytes of index 0) |
|---|---|---|
| SCENE_HEADER | 1 | § "Scene header section" |
| STRINGS | 2 | `uint32` count, then count × `uint32` byte offsets into the text that follows, then UTF-8 texts without terminators; entity names |
| ENTITIES | 3 | E × 48-byte records in activation order (parents before children, siblings by sort key then ID): persistent ID (16), `uint32` parent index (0xFFFFFFFF root; always lower than the record's own index), `uint32` name string index (0xFFFFFFFF none), `uint32` first component record offset into COMPONENTS, `uint16` component count, `uint16` flags (bit 0 root of a prefab instance), `uint32` instance ordinal (0xFFFFFFFF when not from an instance), `uint32` reserved, `uint64` reserved |
| COMPONENTS | 4 | the binary field-set records of ECS (`Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record") per entity, ascending type ID, contiguous per entity |
| ASSET_REFS | 5 | `uint32` count, then count × 24-byte records: persistent ID (16), `uint16` asset kind (a `Zenith_AssetKind` code, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Asset kinds and the kind table"), `uint16` reserved 0, `uint32` reserved 0; sorted by (kind, ID), unique: every asset any component references, the loader's dependency list |
| INSTANCES | 6 | `uint32` count, then count × (instance persistent ID (16), prefab persistent ID (16), `uint32` first entity index, `uint32` entity count): provenance for diagnostics and the editor's play-copy mapping |
| UNKNOWN | 7 | reserved for a future cook policy; a version 1 cook never writes it and a version 1 reader rejects a file carrying it with `SCENE_ERROR_COOKED_CORRUPT` |

- Sections 1 to 6 are each present exactly once at index 0; a missing one, a second index, or a kind SCENE does not define is `SCENE_ERROR_COOKED_CORRUPT`.
- The cook resolves prefabs completely: a cooked scene contains no instance to expand, a cooked prefab no nested instance or variant base, so runtime instantiation is a copy (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Cooking resolved prefabs").
- Section compression: COMPONENTS and STRINGS use the codec and level of the cook's `Zenith_SceneCookSettings` (COMPRESS's per-asset-class choice, zstd by default); SCENE_HEADER, ENTITIES, ASSET_REFS and INSTANCES are stored (`NONE`); PACK may override per asset class.
- An ASSET_REFS kind code for which `Zenith_GetAssetKindInfo` returns null, or `ASSET_KIND_NONE`, is `SCENE_ERROR_COOKED_CORRUPT`.
- Limits checked by the decoder: E ≤ 1,048,576; component records ≤ 64 per entity and bounded by the COMPONENTS size; strings ≤ 65,535 bytes; asset references ≤ 1,048,576; a parent index not lower than its record's index is `SCENE_ERROR_COOKED_CORRUPT`. `Zenith_Fuzz_SceneCooked` fuzzes the decoder through `Zenith_DecodeCookedSceneFile`.
- The format version is bumped with any change of this layout. A kind loader registers `uSCENE_COOKED_FORMAT_VERSION`, so ASSET refuses a file of another version at its check C5 (`ASSET_ERROR_VERSION_MISMATCH`), which the preparation reports as `SCENE_ERROR_COOKED_VERSION`; `Zenith_DecodeCookedSceneFile` compares the version itself and returns the same code ([Z2-N-CONTRACT-528]). Cooked data is never migrated ([Z2-N-CONTRACT-529]).

## Scene diagnostic dump

The development console command `scene.dump <worldTag> <path>` writes `zenith2.scene-dump` version 1 with the canonical writer: per world the hierarchy cache (`nodes` as an ID map keyed by simulation ID in decimal text, depth, parent), the additive scene instances (instance ID, scene persistent ID, state, entity count), the activation queue, and the counters of `Zenith2/Docs/Design/SCENE.md` → "Counters". `scene.hierarchy.validate <worldTag>` rebuilds the cache from the world and reports the first difference.
