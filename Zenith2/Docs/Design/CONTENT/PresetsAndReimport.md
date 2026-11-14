# CONTENT design: import presets and override-preserving reimport

This file is part of the CONTENT design family; the entry document is `Zenith2/Docs/Design/CONTENT.md`. It specifies the import preset document type (schema, validation, registration, migration), how a preset is chosen when a source's `.meta` is created, the presets the engine ships, and reimport that preserves authored overrides: CONTENT's members of the `.meta` sub-asset entries, product fingerprints, rename recognition, and how overrides are applied and kept. Settings resolution itself is ASSET's (`Zenith2/Docs/Design/ASSET/Import.md → "Settings resolution"`): layer 2 reads a preset's `settings` object; this file owns what a preset is.

## Scope of this file

| Topic | Heading |
|---|---|
| the preset document | "Preset document schema", "Preset validation", "Preset document type registration" |
| choosing a preset | "Preset matching" |
| engine presets | "Shipped presets" |
| reimport | "Override-preserving reimport", "Product overrides schema", "Rename recognition", "Applying product overrides", "Overrides across settings migration" |

## Preset document schema

A preset is an authoring document `<name>.importpreset.json` under a content root, with its own `.meta` (importer `zenith-doc`, no product: the source GUID names no cooked asset, `Zenith2/Docs/Design/ASSET/Database.md → "Sub-asset identity"`). Its GUID is the `.meta` GUID, which every `.meta` that uses it names in its `preset` member.

```json
{
	"$schema": "zenith2.import-preset",
	"schemaVersion": 1,
	"id": "4c1d9a0e-6b2f-4e8a-9d3c-0f7a5e2b1c86",
	"name": "Character mesh",
	"description": "Skinned characters: generated LODs, 8 influences, height check.",
	"importer": { "id": "gltf", "settingsVersion": 1 },
	"settings": {
		"lod.mode": "generate",
		"lod.ratios": [0.5, 0.25, 0.125],
		"validate.heightRangeMetres": [0.3, 3.0]
	},
	"match": {
		"extensions": [".glb", ".gltf"],
		"pathGlobs": ["Characters/**"],
		"priority": 100
	},
	"labels": ["character"]
}
```

| Key | Type | Presence | Rule |
|---|---|---|---|
| `$schema` | string | always | `zenith2.import-preset` |
| `schemaVersion` | integer | always | 1 (current); older migrate; newer open read-only ([Z2-N-CONTRACT-537]) |
| `id` | persistent ID text | always | equal to the preset source's `.meta` GUID (P3) |
| `name` | string | always | 1 … 64 bytes; shown in the editor's preset picker |
| `description` | string | optional | at most 1,024 bytes |
| `importer.id` | string | always | a registered importer ID (P4) |
| `importer.settingsVersion` | integer | always | 1 … the importer's current; older migrate through the importer's settings chain (P5) |
| `settings` | keyed map | always (may be empty) | keys of the importer's settings, values of their types and ranges (P6, P7) |
| `match` | object | optional | absent: the preset is never chosen automatically, only by hand |
| `match.extensions` | array of string | required in `match` | lower case with the dot, each claimed by `importer.id` (P8) |
| `match.pathGlobs` | array of string | required in `match` | 1 … 32 globs over the source's content-root-relative path (`*` one segment part, `?` one character, `**` any number of segments), each at most 256 bytes (P9) |
| `match.priority` | integer | required in `match` | −1,000 … 1,000; the higher wins ("Preset matching") |
| `labels` | array of string | optional | `.meta` labels (`^[a-z][a-z0-9-]{0,31}$`, unique, sorted) the matcher writes into a new `.meta`, so a preset assigns the content-rule class (`Zenith2/Docs/Design/ASSET/Formats.md → "Content rule table"`) |
| `x-<anything>` | any | optional | preserved byte for byte |

The file is at most 1 MiB, parsed under the JSON limits with duplicate keys refused. A preset carries no per-platform object: ASSET's resolution reads only its `settings` (layer 2), and platform overrides belong to each `.meta`'s `platforms` (layer 4).

## Preset validation

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentValidation.h
/// @contract value Zenith_ImporterSettingSchemaView
/// Ownership: Plain view over an importer's settings description, built by the caller from Zenith_ImporterDesc (ZenithCook) or
///   from the editor's importer list; ContentCore never links ASSET's types.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the desc it views.
/// Units: As each setting's description.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentSettingView
{
	const Zenith_StringView m_strKey = {};
	const uint8 m_uType = 0;                        // the Zenith_ImporterSettingType value: 1 BOOL, 2 INT, 3 FLOAT, 4 STRING, 5 ENUM, 6 FLOAT_ARRAY
	const Zenith_StringView m_strDefaultJson = {};
	const float m_fMin = 0.0f, m_fMax = 0.0f;        // both 0: unbounded
	const Zenith_Span<const Zenith_StringView> m_astrEnumValues = {};
	const uint32 m_uMaxArrayCount = 0;
	const bool m_bPlatformOverridable = false;
};
struct Zenith_ImporterSettingSchemaView
{
	const Zenith_StringView m_strImporterID = {};
	const uint32 m_uSettingsVersion = 0;
	const Zenith_Span<const Zenith_StringView> m_astrExtensions = {};
	const Zenith_Span<const Zenith_ContentSettingView> m_axSettings = {}; // ascending key
};

/// @contract value Zenith_ContentValidationMessage
/// Ownership: Plain value; the strings point into the caller's arena.
/// Thread affinity: IMMUTABLE once returned.
/// Invalidation: With the arena.
/// Units: 1-based line and column of the offending member.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentValidationMessage { const uint16 m_uRule = 0; const bool m_bError = false; const Zenith_StringView m_strKeyPath = {}; const Zenith_StringView m_strMessage = {}; const uint32 m_uLine = 0, m_uColumn = 0; };

/// @contract functions Zenith_ImportPresetValidation
/// Ownership: Messages are appended to the caller's array, their text in the caller's arena.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: CONTENT_ERROR_PRESET_INVALID (m_uDetail the first error rule) when any error message was produced; CONTENT_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
Zenith_Result<void, Zenith_ContentError> Zenith_ValidateImportPresetJson(const Zenith_JsonDocument& xPreset,
	const Zenith_PersistentID& xMetaGUID, Zenith_Span<const Zenith_ImporterSettingSchemaView> axImporters,
	Zenith_Array<Zenith_ContentValidationMessage>& axMessagesOut, Zenith_Arena& xArena);
```

| Rule | Check | Severity |
|---|---|---|
| P1 | `$schema` is `zenith2.import-preset` and `schemaVersion` is an integer ≥ 1 | error |
| P2 | every required key present with its type; `name` 1 … 64 bytes; `description` ≤ 1,024 bytes | error |
| P3 | `id` is a canonical persistent ID equal to `xMetaGUID` | error |
| P4 | `importer.id` names an importer of `axImporters` | error |
| P5 | `importer.settingsVersion` ≤ that importer's `m_uSettingsVersion` | error |
| P6 | every `settings` key is one of the importer's settings | warning (unknown keys are kept, [Z2-N-CONTRACT-543]) |
| P7 | every known setting's value has the declared type, range, enumeration and array count | error |
| P8 | every `match.extensions` entry is lower case, starts with `.`, and is claimed by the importer | error |
| P9 | `match.pathGlobs` has 1 … 32 entries, each 1 … 256 bytes, containing no `..`, no `\`, no leading `/` | error |
| P10 | `match.priority` is an integer in −1,000 … 1,000 | error |
| P11 | `labels` entries match `^[a-z][a-z0-9-]{0,31}$`, unique, sorted | error (unsorted: warning; the next canonical write sorts) |
| P12 | no member other than the table's and `x-` members at the top level | warning |

ASSET's resolution rejects a preset whose `importer.id` differs from the `.meta`'s importer with `IMPORT_ERROR_SETTING_INVALID` naming both (pending ASSET, row CT3); the editor validates a preset on every save of its document (row CT18); the cook validates every preset a resolution reads through the same function inside ASSET's settings resolution (row CT3).

## Preset matching

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
/// @contract value Zenith_ImportPresetEntry
/// Ownership: Plain value; views point into the documents the caller keeps alive for the set's life.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the caller's documents.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ImportPresetEntry
{
	const Zenith_PersistentID m_xGUID = {};
	const Zenith_StringView m_strLogicalPath = {};   // content:/… of the preset source, the tie-break
	const Zenith_JsonDocument* const m_pxDocument = nullptr; // a document that passed Zenith_ValidateImportPresetJson
};

/// @contract value Zenith_ImportPresetChoice
/// Ownership: Plain value; m_astrLabels points into the set.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the set.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ImportPresetChoice
{
	const Zenith_PersistentID m_xPreset = {};            // null: no preset matched
	const Zenith_Span<const Zenith_StringView> m_astrLabels = {};
	const Zenith_StringView m_strMatchedGlob = {};       // the match.pathGlobs entry that selected the preset; empty for no match
};

/// @contract type Zenith_ImportPresetSet
/// Ownership: Created by Build as a Zenith_Owned; copies the match rules and labels it needs; borrows nothing after Build.
/// Thread affinity: IMMUTABLE after Build; ANY_THREAD.
/// Invalidation: None; rebuilt when a preset document changes (the cook rebuilds it per run, the server per scan).
/// Units: None.
/// Errors: Build returns CONTENT_ERROR_PRESET_INVALID for an entry whose document lacks a valid match block (entries without
///   `match` are accepted and never selected) and CONTENT_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
class Zenith_ImportPresetSet
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ImportPresetSet>, Zenith_ContentError> Build(
		Zenith_Span<const Zenith_ImportPresetEntry> axEntries, Zenith_Heap& xHeap);
	// The preset a new .meta of this source gets: importer ID, lower-case extension with the dot, content-root-relative path.
	Zenith_ImportPresetChoice Select(Zenith_StringView strImporterID, Zenith_StringView strExtension,
		Zenith_StringView strContentRelativePath) const;
	Zenith_Span<const Zenith_PersistentID> GetPresetsFor(Zenith_StringView strImporterID) const;   // for the picker, name order
};
```

`Select`:

1. Candidates: every entry with a `match` block whose `importer.id` equals the importer and whose `match.extensions` contains the extension.
2. Keep the candidates for which at least one `match.pathGlobs` entry matches the content-root-relative path (byte-wise, case-sensitive, `/` separators).
3. Choose the highest `match.priority`; ties go to the byte-order-smallest logical path; none: the null preset and no labels.

`Build` sorts entries by (priority descending, logical path ascending) once, so `Select` is O(P × G) for P candidate presets and G globs per preset, with no allocation.

Where it runs (pending ASSET, row CT3): `Zenith_AssetDatabase::AssignMissingGuids` (`--assign-guids`) and the cook server's `assignGuids` build the set from every live preset record of the scan (the game's roots and the linked engine roots), call `Select` for each source receiving a new `.meta`, and write the chosen GUID into its `preset` member and the chosen labels into its `labels`, and list every choice in their output (`assignGuids`' result gains `presets [{source, preset, glob}]`, `preset` null for no match; `--assign-guids` prints one line per source), so the editor shows each proposal for review before the `.meta` is committed (pending ASSET, row CT24); the result is committed with the `.meta`, so the choice is a recorded declaration, never re-evaluated: renaming a file or editing a preset's `match` afterwards changes no existing `.meta`. A preset assigned afterwards is changed through the server's `setSettings` with its `preset` argument (row CT1) or a `setPreset` batch (`Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Reimport and settings batches"`).

A preset document's change re-imports every source whose `.meta` names it: ASSET records the settings-input edge and hashes the preset's settings into the import key (`Zenith2/Docs/Design/ASSET/Database.md → "Dependency graph"`); `Test_ImportPreset_EditReimportsUsers` asserts it end to end.

## Shipped presets

The engine's presets live in `Zenith2/Engine/ZenithAssets/Internal/Content/ImportPresets/` (pending ASSET, row CT3: a CONTENT-owned subdirectory of ZenithAssets' engine content root, scanned for every game because every game links ZenithAssets), each with a committed `.meta` whose GUID the ticket that adds it generates; other areas ship their own presets in their own libraries' content roots (UI's `ui.sprite`, row UI11). Their logical paths are `content:/Engine/ImportPresets/<file>`.

<!-- elaboration -->
| File | Importer | `settings` | `match` | `labels` |
|---|---|---|---|---|
| `mesh.static.importpreset.json` | `gltf` | `lod.mode` `generate`, `import.animations` false, `import.skins` false | `.glb`, `.gltf`; `**`; priority 0 | `prop` |
| `mesh.character.importpreset.json` | `gltf` | `lod.mode` `generate`, `lod.ratios` [0.5, 0.25, 0.125], `validate.heightRangeMetres` [0.3, 3.0], `precision.posedMetres` 0.001 | `.glb`, `.gltf`; `Characters/**`; priority 100 | `character` |
| `mesh.foliage.importpreset.json` | `gltf` | `lod.mode` `generate`, `lod.ratios` [0.5, 0.25], `cull.customDistance` true, `import.cameras` false, `import.lights` false | `.glb`, `.gltf`; `Foliage/**`; priority 100 | `foliage` |
| `mesh.animationOnly.importpreset.json` | `gltf` | `import.meshes` false, `import.materials` false, `import.textures` false, `import.prefab` false | `.glb`, `.gltf`; `Animations/**`; priority 100 | `animation` |
| `texture.colour.importpreset.json` | `image` | `texture.usage` `colour` | none (chosen by hand) | — |
| `texture.colourAlphaTest.importpreset.json` | `image` | `texture.usage` `colour`, `texture.alphaCoverageCutoff` 0.5 | `.png`, `.tga`; `**/*_cutout.png`, `**/*_cutout.tga`; priority 50 | — |
| `texture.normal.importpreset.json` | `image` | `texture.usage` `normal` | the image extensions; `**/*_n.*`, `**/*_normal.*`, `**/*_nrm.*`; priority 50 | — |
| `texture.normalDirectX.importpreset.json` | `image` | `texture.usage` `normal`, `normal.flipGreen` true | none (chosen by hand) | — |
| `texture.mask.importpreset.json` | `image` | `texture.usage` `mask` | the image extensions; `**/*_orm.*`, `**/*_mask.*`; priority 50 | — |
| `texture.linear.importpreset.json` | `image` | `texture.usage` `linear` | none (chosen by hand) | — |
| `audio.sfx.importpreset.json` | `audio` | `audio.loadMode` `auto`, `audio.channels` `keep` | `.wav`, `.flac`, `.ogg`, `.aif`, `.aiff`; `**`; priority 0 | — |
| `audio.music.importpreset.json` | `audio` | `audio.loadMode` `streamed`, `audio.codec` `codec` | the audio extensions and `.mp3`; `Music/**`; priority 100 | `music` |
| `audio.dialogue.importpreset.json` | `audio` | `audio.loadMode` `streamed`, `audio.normalise` true, `audio.targetLufs` −16, `audio.channels` `mono` | the audio extensions; `Dialogue/**`, `VO/**`; priority 100 | `dialogue` |

- **No image preset matches every image.** An image whose path no image preset's `match` block selects gets the null preset at `.meta` creation and fails its first import with `CONTENT_IMPORT_DETAIL_USAGE_UNSET`, naming the commands that assign one (`Zenith2/Docs/Design/CONTENT/Importers.md → "Image importer"`): the colour space of an image is declared, never defaulted (`Test_ShippedPresets_NoCatchAllImagePreset`).
- **The suffix presets are proposals.** The `_cutout`, `_n`, `_normal`, `_nrm`, `_orm` and `_mask` globs infer a usage from a file name, and they do it once, at `.meta` creation: `assignGuids` lists each choice with the glob that made it (`presets [{source, preset, glob}]`, "Preset matching"), the editor shows the list before the `.meta` is committed, and the committed `.meta` records the choice, which a rename never changes. Each suffix has exactly one meaning: `_cutout` an alpha-tested colour texture, `_mask` and `_orm` a mask texture, for every image extension.
- A game that wants different defaults adds its own presets at a higher priority under its content root; it deletes nothing.
- `cull.customDistance` is GEOM's preset key for ENV's foliage cull-distance flag (`Zenith2/Docs/Design/GEOM.md → "Import preset keys GEOM reads"`; ENV's need, row RP21).
- `Test_ShippedPresets_ValidateAgainstImporters` loads every shipped preset with its `.meta` and asserts P1 … P12 pass and every `settings` key is known; `Test_ContentImporters_DeclaredKeysAreGeomKeys` keeps the GEOM keys in step with GEOM's table.

## Preset document type registration

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
inline constexpr const char* szCONTENT_IMPORT_PRESET_STABLE_NAME = "Zenith.Content.ImportPresetDocument";
inline constexpr uint32 uCONTENT_IMPORT_PRESET_VERSION = 1;

/// @contract functions Zenith_ContentDocumentTypeFunctions
/// Ownership: Registers into the caller's registry; returns a static schema.
/// Thread affinity: MAIN_THREAD before the registry's Freeze; the schema getter ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's Zenith_DocumentError.
/// Version: since Z2 Stage 4. Tool-side; called by ZenithCook's services (item 5) and by EDIT's Zenith_RegisterEditorDocumentTypes.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterContentDocumentTypes(Zenith_MigrationRegistry& xRegistry);
const Zenith_CanonicalSchema& Zenith_GetImportPresetCanonicalSchema();
```

The registration declares the stable name, suffix `.importpreset.json`, `$schema` tag `zenith2.import-preset`, version key `schemaVersion`, ID key `id`, current version 1, no step, an embedded visitor that migrates the `settings` object through the importer's settings chain when `importer.settingsVersion` is older (the same visitor shape as ASSET's `.meta` type, `Zenith2/Docs/Design/ASSET/Migration.md → "Importer settings inside meta documents"`), and the canonical schema:

| Rule | Node | Members in canonical order, or element rule |
|---|---|---|
| 0 root | OBJECT | `$schema`, `schemaVersion`, `id`, `name`, `description`, `importer` (rule 1), `settings` (rule 2), `match` (rule 3), `labels` (INLINE_ARRAY) |
| 1 importer | OBJECT | `id`, `settingsVersion` |
| 2 settings | KEYED_MAP | element VALUE (arrays of scalars INLINE_ARRAY) |
| 3 match | OBJECT | `extensions` (INLINE_ARRAY, sorted on write), `pathGlobs` (ORDERED_ARRAY), `priority` |

The type joins [Z2-N-CONTRACT-552]'s list in the diff that registers it (cross-doc row X285 with the wording of its correction X342). Fixtures: `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/CONTENT/ImportPresetDocument/v1.json` and `v1.expected.json` under ZenithAuthoring's one fixture root (`Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"` rule 2, row ED48; [Z2-N-CONTRACT-548]); `Test_ImportPresetDocument_MigrationFixtures` enumerates versions 1 … current and requires both files for each ([Z2-N-CONTRACT-549]).

## Override-preserving reimport

### What an authored override is

| Class | Where it lives | Survives reimport because |
|---|---|---|
| importer settings and platform overrides | the `.meta` `settings` and `platforms` | reimport never writes them (`Zenith2/Docs/Design/ASSET/Import.md → "Import algorithm"` step 5 writes only `subAssets`) |
| the chosen preset | the `.meta` `preset` | the same |
| per-product overrides: material remaps and exclusions | CONTENT's members `materialRemap` and `exclude` of the product's `.meta` sub-asset entry ("Product overrides schema") | the entry is the product's identity record: ASSET keeps it, with every owner member, per key, moves it with the GUID on a rename, and keeps it when the key is orphaned |
| one clip's own values (range, loop, root motion, additive, compression, streaming, events, markers) and one skeleton's rig annotations | ANIM's clip and rig documents, attached by the members `annotation` and `rig` of the product's sub-asset entry (`Zenith2/Docs/Design/ANIM/Formats.md → "Document type table"`) | the same entry rule; the documents are separate authoring documents that reimport never writes |
| references from other documents to products (a scene's mesh-instance, a material's texture, a bank's sound) | the referencing documents | they name GUIDs ([Z2-N-CONTRACT-546]; `Zenith2/Docs/Design/ASSET/Import.md → "Identity preservation on reimport"` step 4) |
| overrides on model-prefab entities (scene instances, prefab variants) | scene and prefab documents | the generated entity IDs are derived from the prefab GUID and the node key (`Zenith2/Docs/Design/CONTENT/Importers.md → "Node keys"`) |

### Reimport guarantees

1. A product whose key is unchanged keeps its GUID (ASSET).
2. A product whose key changed by a rename the importer recognises keeps its GUID ("Rename recognition"); the rename is reported with both keys (`CONTENT_WARNING_RENAME_ASSUMED`, and the `renames` list of the import report), so the person can undo it with a sub-asset remap batch.
3. A model-prefab entity keeps its ID across exactly those edits that keep its node key (`Zenith2/Docs/Design/CONTENT/Importers.md → "Node keys"`): a node carrying `extras.zenithId` across any rename, reparent or reorder of itself or of any ancestor; a node without one across any edit that keeps every name on its path from its nearest `zenithId` ancestor (or the scene root) and its rank among equal-named siblings. Renaming or reparenting a node without a `zenithId`, or any of its ancestors below that point, gives it and every descendant keyed through it new IDs. No node rename is recognised.
4. A product no longer produced is orphaned: its GUID stays reserved, its references report missing, and its owner members are kept, reported dangling except `exclude` ("Applying product overrides"). A GUID is re-bound to a new key only when the old and new products' fingerprints are equal ("Product fingerprints"), and then always warned `CONTENT_WARNING_RENAME_ASSUMED` with both keys and listed in the import report's `renames`; a fingerprint is evidence, not proof, so two different objects with equal fingerprints (two cubes of one topology deleted and added in one edit) are re-bound, which the warning makes visible and a sub-asset remap batch undoes. The orphan repair and remap batches (`Zenith2/Docs/Design/CONTENT/BatchOperations.md → "Sub-asset remap and orphan repair"`) are how a person binds an orphan to its replacement or undoes an assumed rename.
5. A scene override of a removed or re-keyed model-prefab entity is SCENE's dangling override (`DOCUMENT_WARNING_DANGLING_OVERRIDE`, [Z2-N-CONTRACT-547]): kept, reported, never dropped and never re-targeted.

`Test_ContentPipeline_ReimportPreservesOverrides` (Tests.md "Integration suite") imports a fixture character, authors a material remap, an excluded mesh, an ANIM clip annotation document with `loop: true` attached to one clip's entry, a scene instance of the model prefab overriding the transform of a node carrying a `zenithId`, and a variant adding a child, then reimports five edits of the source (vertex changes; a renamed mesh; reordered nodes; the `zenithId` node renamed; one of its ancestors renamed and the node reparented under another bone) and asserts after each that every override still applies, the clip's cooked loop flag is the document's, and the cooked scene is the expected one.

## Product overrides schema

CONTENT's per-product overrides are members of the product's entry in the `.meta` `subAssets` array (pending ASSET, row CT22, which also makes the entry carry ANIM's `annotation` and `rig` and ASSET's `fingerprint`); there is no separate per-product map and no per-product settings:

```json
"subAssets": [
	{ "key": "anim:Run", "guid": "e41b8c02-5a7d-4e39-91c6-2f0a8d3b6e57", "kind": "AnimationClip", "fingerprint": "1f0a8d3b6e57e41b", "annotation": "5a7d2f0a-8d3b-4e57-9e41-b8c0291c6e39" },
	{ "key": "material:Skin", "guid": "3c1f7a20-1b4e-4d9a-8e21-6f0b2c7d9e15", "kind": "Material", "fingerprint": "8e216f0b2c7d9e15" },
	{ "key": "mesh:Body", "guid": "9a02d4e8-7c31-4b6f-a2e0-5d8c1f3b7a64", "kind": "Mesh", "fingerprint": "a2e05d8c1f3b7a64",
		"materialRemap": { "3c1f7a20-1b4e-4d9a-8e21-6f0b2c7d9e15": "7e5b0a91-2c8d-4f13-b6a7-0e9d4c2f1b38" } },
	{ "key": "mesh:Proxy", "guid": "c7d9e153-c1f7-4a20-9b4e-d9a8e216f0b2", "kind": "Mesh", "fingerprint": "6f0b2c7d9e153c1f", "orphaned": true, "exclude": true }
]
```

| Member | Owner | Type | Rule |
|---|---|---|---|
| `exclude` | CONTENT | boolean | optional, default false; true: the importer does not declare the product, now or on any subsequent import, whether the entry is live or orphaned (O2, O5) |
| `materialRemap` | CONTENT | ID map | `mesh:` entries only (O3); keys: GUIDs of this source's `material:` entries; values: GUIDs of Material assets of any source (O4, O6) |
| `annotation` | ANIM | persistent ID | `anim:` entries; ANIM's clip annotation document; validated by ANIM's step, never by CONTENT |
| `rig` | ANIM | persistent ID | `skel:` entries; ANIM's rig document; validated by ANIM's step |
| `fingerprint` | ASSET, from the importer | 16 lowercase hex digits | written by ASSET's reconciliation from `Zenith_ImportContext::SetProductFingerprint` ("Product fingerprints"); read-only to every person and tool; never a key input |

The owner members `exclude`, `materialRemap`, `annotation` and `rig` are import-key inputs: their canonical text, entry by entry in GUID byte order and keyed by GUID (so a rename that moves an entry to a new key changes no input), joins the settings hash (pending ASSET, row CT22). Changing an override therefore reimports exactly that source and re-cooks exactly the affected products (`Test_ProductOverrides_ChangeReimportsSource`).

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentValidation.h
/// @contract functions Zenith_ProductOverridesValidation
/// Ownership: As Zenith_ImportPresetValidation.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: CONTENT_ERROR_OVERRIDES_INVALID (m_uDetail the first error rule); CONTENT_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentSubAssetView
{
	Zenith_PersistentID m_xGUID;
	Zenith_StringView m_strKey;
	Zenith_StringView m_strKind;                     // ASSET's script name
	bool m_bOrphaned;
	bool m_bHasFingerprint;
	uint64 m_ulFingerprint;
	Zenith_JsonNode m_xEntry;                        // the whole entry object, or in an importer the owner-members object parsed from Zenith_ImportPreviousProduct::m_strMembersJson
};
// Validates CONTENT's members (exclude, materialRemap) of every entry of a .meta's subAssets array.
Zenith_Result<void, Zenith_ContentError> Zenith_ValidateProductOverridesJson(
	Zenith_Span<const Zenith_ContentSubAssetView> axSubAssets, Zenith_StringView strImporterID,
	Zenith_Array<Zenith_ContentValidationMessage>& axMessagesOut, Zenith_Arena& xArena);
```

| Rule | Check | Severity |
|---|---|---|
| O1 | a CONTENT member sits on an entry of this `.meta`'s `subAssets` (live or orphaned) | error |
| O2 | `exclude` is a boolean | error |
| O3 | `materialRemap` appears only on a `mesh:` entry | error |
| O4 | `materialRemap` keys are GUIDs of `material:` entries of this `.meta`; values are canonical GUIDs (resolved to a Material record by the cook, `COOK_ERROR_MATERIAL_*` otherwise) | error |
| O5 | a CONTENT member on an entry whose kind prefix the importer `strImporterID` never produces (`exclude` on an ANIM-only `.meta` of another importer, for example) | error |
| O6 | a `materialRemap` value equal to its key | warning (a no-op, kept) |
| O7 | a remap key naming an orphaned `material:` entry | warning: kept, not applied, reported `CONTENT_WARNING_OVERRIDE_DANGLING` |

The importer runs the validator before using the members; an error fails the import with `IMPORT_ERROR_SETTING_INVALID`, detail `CONTENT_IMPORT_DETAIL_OVERRIDES`, naming the entry key and member; the editor runs it before sending `setSettings` (row CT27).

## Rename recognition

The `gltf` importer (and the `fbx` importer through it) recognises renames per product kind prefix, in this order, before declaring products (Importers.md "Import algorithm" step 5).

### Product fingerprints

A product fingerprint is the evidence that a new key names the object an old key named. It is the first 8 bytes of `Zenith_Sha256Digest` (read big-endian as a `uint64`, written as 16 lowercase hex digits) over a tag and the object's identity description, all integers little-endian:

| Kind prefix | Tag | Identity description | Survives |
|---|---|---|---|
| `mesh:` | `ZFPM` | vertex count (u32), triangle count (u32), attribute mask (u8: bit 0 normals, 1 tangents, 2 UV0, 3 UV1, 4 colours), influences (u8: 0, 4 or 8), material slot count (u8), morph target count (u16) | vertex edits that keep the topology; a rename |
| `mesh:…@mirrored` | — | its base mesh's fingerprint | follows its base mesh |
| `material:` | `ZFPA` | the generated document's `module`, `type`, `domain`, `twoSided` and `alphaCutoff`, each parameter's name and IEEE-754 bit pattern in name order, and the names of the bound texture fields in name order (never the texture GUIDs, which change when an image is renamed) | a rename |
| `texture:<image>@<u>` | `ZFPT` | the SHA-256 of the encoded image bytes, then the usage name | a rename of the image |
| `texture:<material>/orm` | — | its material's fingerprint | follows its material |
| `prefab:` | `ZFPP` | node count (u32), mesh-instance count (u32), camera count (u32), light count (u32) | a scene rename |
| `collision:shape` | — | none: the key never changes | — |
| `skel:`, `anim:` | ANIM's | computed by ANIM's helpers with the same rule (row CT25): a skeleton's joint count and sorted joint names, a clip's channel count and duration | a rename |

The importer computes every product's fingerprint and records it with `SetProductFingerprint` after `DeclareProduct` (pending ASSET, row CT22); ASSET writes it into the entry in sidecar-writing modes, and `GetPreviousProducts` returns it on the next import.

### Recognition steps

1. **Explicit identity**: a product whose name came from `extras.zenithId` has the same key whatever its display name, so it is never a rename.
2. **Previous products**: P = the keys of `GetPreviousProducts()` of the kind prefix that are not orphaned, carry no `exclude: true`, and are not produced by this import; N = this import's keys of the prefix that the `.meta` does not hold. For the `texture:` prefix, P and N are formed separately per role suffix `@<u>` (`@colour`, `@normal`, …), so an embedded image used in two roles and renamed (`texture:X@colour` and `texture:X@normal` to `texture:Y@colour` and `texture:Y@normal`) is two one-to-one renames, each identified by its fingerprint (the image hash and the usage name), never one ambiguous P = N = 2. Keys of derived products, those ending `@mirrored` (a mirrored mesh variant) or `/orm` (a packed texture), are in neither set: they follow their base product's outcome at step 3, so renaming a mesh that has a mirrored variant is the one-to-one case (P = {`mesh:A`}, N = {`mesh:B`}), not the ambiguous P = 2, N = 2.
3. **One-to-one rule with evidence**: when |P| = 1 and |N| = 1 and the old entry's fingerprint equals the new product's, the single old key is renamed to the single new key (`RenameProduct(old, new)`) and the rename is reported `CONTENT_WARNING_RENAME_ASSUMED` with both keys: the common case of one object renamed in the DCC. Mirrored variants and packed ORM textures follow their base product's rename (`mesh:A@mirrored` → `mesh:B@mirrored`, `texture:A/orm` → `texture:B/orm`).
4. **No evidence**: when |P| = 1 and |N| = 1 but the fingerprints differ, or the old entry has none (written before fingerprints existed, or by a mode that does not write sidecars), no rename is made: the old key is orphaned, the new key gets a new GUID, and `CONTENT_WARNING_RENAME_NO_EVIDENCE` names both keys, so a deleted mesh A and an unrelated new mesh B never share a GUID.
5. **Ambiguity**: when |P| ≥ 1 and |N| ≥ 1 and not both are 1, no rename is made; the old keys are orphaned, the new keys get new GUIDs, and `CONTENT_WARNING_RENAME_AMBIGUOUS` lists both sets with their fingerprints, so the person can match them and map each with a sub-asset remap batch (BatchOperations.md "Sub-asset remap and orphan repair"), which moves each GUID back to its intended key.
6. **Skeletons and clips** are declared by ANIM's helpers, which apply the same rule, with their fingerprints, through the same context calls; the importer passes them the previous products.

Recognition is deterministic: it depends only on the previous entries (keys, flags, fingerprints) and the new keys and fingerprints (`Test_RenameRecognition_OneToOneRenames`, `_AmbiguousOrphansAndWarns`, `_DeletedAndAddedNeverRebound`, `_ZenithIdIsStable`).

## Applying product overrides

| Override | Applied at | Effect |
|---|---|---|
| `exclude` | declaring products (Importers.md "Import algorithm" step 6) | the importer finds the key's previous entry through `GetPreviousProducts()`, live or orphaned, and does not declare the product; ASSET then keeps (or marks) the entry orphaned, with the member, so the next import finds it again and the product stays excluded on every subsequent reimport; an excluded mesh is also removed from the model prefab's entities (its node entity stays, without a mesh-instance component); excluding a material falls its slots back to `material:@default`; the import report lists the key in `excluded` |
| `materialRemap` | writing a mesh's intermediate | each slot whose material GUID is a remap key takes the remap value instead; the generated material product itself is still declared unless excluded, so the remap is reversible |

- An orphaned entry carrying `exclude: true` is **excluded**, never dangling: rule O1 accepts it, it raises no warning, and step 2 of "Recognition steps" never treats it as a rename candidate (`Test_ProductOverrides_ExcludeStableAcrossThreeReimports`).
- A `materialRemap` on an orphaned mesh entry, or a remap key that is no longer one of the source's live materials, is kept in the `.meta`, not applied, and reported `CONTENT_WARNING_OVERRIDE_DANGLING` once per import; reimport never deletes a member.
- Per-clip and per-rig values are ANIM's documents ("What an authored override is"); CONTENT applies none of them.

## Overrides across settings migration

- The product members carry no settings, so an importer's settings chain (`m_apfnSettingsSteps`) never touches them: a step migrates the `.meta` `settings` and every preset's `settings` only (ASSET's visitor and "Preset document type registration").
- The owner members' schema is part of the `Zenith.AssetMeta` schema: a change to `exclude` or `materialRemap` is a `.meta` version step, which ASSET registers and fixtures under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Asset/AssetMeta/` at CONTENT's request (row CT22); an unknown member on an entry is ASSET's unknown-member rule, preserved byte for byte.
- The `provenance` member (`Zenith2/Docs/Design/CONTENT/SharedCache.md → "Content provenance"`) carries its own `schema` and migrates through its rows of ASSET's constexpr member-migration table `Zenith_GetAssetMetaMemberMigrations()` inside the `.meta` type's visitor (`Zenith2/Docs/Design/ASSET/Migration.md → "Member migrations"`, row CT24; a table rather than a run-time registration function, because a registry filled at start-up is mutable global state); version 1 has no step and so no row, a later schema is a CONTENT diff appending its `{member, fromVersion, step}` row with ASSET reviewing, and the fixtures are ASSET's `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Asset/AssetMeta/provenance/v1.json` and `v1.expected.json`.
- `Test_ProductOverrides_SurviveMetaMigration` runs a v1 → v2 test chain of the `.meta` type (a step that renames an importer setting) and asserts every `exclude`, `materialRemap`, `annotation`, `rig` and `fingerprint` member of every entry survives byte for byte in place.
