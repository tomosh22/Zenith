# ASSET design: file formats

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. It specifies every ASSET file format byte for byte or key by key: the cooked asset file container, the catalogue, the `.zpak` archive the runtime reader mounts, the `.meta` sidecar schema, the tombstone list, the asset database file, DDC entries, the content-rule table the cook reads, the cook journal, the cook report, provenance lines, the dependency file, cook server and worker messages and the load result payload.

## Scope of this file

| Format | Heading | Reader |
|---|---|---|
| cooked asset file | "Cooked asset file" | runtime (`Zenith_ParseCookedHeader`) and the cook |
| catalogue | "Asset catalogue file" | runtime (`Zenith_AssetCatalogue::Load`, `Merge`) |
| `.zpak` archive | "Archive file" | runtime (`Zenith_CreateArchiveMountSource`); PACK's packer writes it |
| `.meta` | "Meta sidecar schema" | tools |
| tombstones | "Tombstone list" | tools |
| database | "Asset database file" | tools |
| DDC entry | "DDC entry file" | tools |
| stored import diagnostics | "Import diagnostics blob" | tools (the database, the DDC's import record entries) |
| content rules | "Content rule table" | the cook |
| journal, lock files, report, provenance, depfile | "Cook journal", "Cook lock file", "Cook report", "Provenance record lines", "Dependency file" | tools, Ninja, DEVT |
| server and worker messages | "Cook server messages", "Worker messages" | the editor, the cook |
| load result | "Load result payload" | replay readers |

## Common rules

- Binary formats are little-endian, written and read field by field; no reader or writer copies a C++ object's memory (Z2-D-038).
- Every binary format begins with a four-byte magic and a `uint16` format version; a reader refuses any other version with a recoverable error ([Z2-N-CONTRACT-528]).
- Every size, count and offset is checked against the input length with overflow-checked arithmetic before use ([Z2-N-ARCH-951]); every field this file marks reserved must be zero and a non-zero one is refused (`ASSET_CORRUPTION_BAD_RESERVED_FIELD`, `CATALOGUE_INVALID` in a catalogue, `FILE_ERROR_ARCHIVE_CORRUPT` in an archive), so a future version can use it. No format stores data in a reserved field.
- Checks are `Zenith_Hash64` truncated to its low 32 bits (a "check") or `Zenith_Hash128Of` (a "hash"), per D-SEL-HASH (open, Stage 1); checks detect accidents, not attacks (patches are authenticated by PLAT's and SEC's signed manifest, `Zenith2/Docs/Architecture.md → "Security architecture"`).
- JSON formats are written by `Zenith_JsonWriter` with LF line endings and one trailing LF, read with `Zenith_JsonDocument` under `Zenith_JsonLimits` with duplicate keys refused.

<!-- assumption: pending ARCH (X111) -->
The limits of every binary format here join `Zenith2/Docs/Architecture.md → "Security architecture"` table "Parser limits" through core-doc item X111 (pending); until the batch lands, the limits tables of this file are the parser limits the fuzz targets and the at-limit tests assert.
<!-- /assumption -->

## Cooked asset file

### Header layout

The 64-byte header every cooked asset file begins with ([Z2-N-CONTRACT-527]); GEOM's formats use it field for field (`Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`):

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | the kind's magic (entry document "Asset kinds"), for example `ZTEX` as bytes `5A 54 45 58` |
| 4 | 2 | format version | the kind owner's format version, 1 … 65,535 |
| 6 | 2 | flags | 0 in container version 1 |
| 8 | 4 | engine cook version | `uZENITH_ENGINE_COOK_VERSION` |
| 12 | 4 | section count N | 0 … 4,096 |
| 16 | 16 | content hash | the cook item's DDC key Hash128 (low word, then high word) |
| 32 | 16 | asset GUID | `Zenith_PersistentID` binary form |
| 48 | 2 | platform | `Zenith_AssetPlatform` |
| 50 | 2 | kind | `Zenith_AssetKind` |
| 52 | 4 | header check | check over bytes `[0, 64 + 40 N)` with this field read as zero |
| 56 | 8 | file bytes | the total file size |

### Section table

N entries of 40 bytes at offset 64, sorted by (kind, index), no two equal:

| Offset in entry | Size | Field | Rule |
|---|---|---|---|
| 0 | 2 | section kind | defined by the format owner; 0xFF00 … 0xFFFF reserved to ASSET (none defined in version 1) |
| 2 | 2 | section index | 0 … 65,535 |
| 4 | 4 | stored check | check over the section's stored bytes |
| 8 | 8 | offset | from the file start |
| 16 | 24 | descriptor | COMPRESS's `Zenith_CompressedSectionDesc` bytes (`Zenith2/Docs/Design/COMPRESS.md → "Compressed section descriptor on disk"`): codec ID, level, flags, dictionary ID, stored bytes, decoded bytes |

The codec of every compressed section is therefore recorded in the header's table, and readers select the codec from it (Z2-D-004).

### Section placement and alignment

- Sections follow the table in table order; the first begins at `align16(64 + 40 N)`, each next at `align16(previous offset + previous stored bytes)`; the gaps are zero bytes; the file ends at the last section's end.
- 16-byte alignment (`uASSET_SECTION_ALIGNMENT`) lets stored sections that are GPU images by design (vertex streams, texture blocks) be read directly into upload staging.

### Checks

`Zenith_ParseCookedHeader` applies these in order and returns the first failure:

| # | Check | Failure |
|---|---|---|
| C1 | at least 64 bytes; magic equals the expectation | `CORRUPT` / `BAD_MAGIC` |
| C2 | N ≤ 4,096, else `LIMIT_EXCEEDED`; then the input holds at least `64 + 40 N` bytes, else `CORRUPT` / `BAD_SECTION_TABLE` | as stated, in that order |
| C3 | header check | `CORRUPT` / `BAD_HEADER_CHECK` |
| C4 | flags and reserved fields zero | `CORRUPT` / `BAD_RESERVED_FIELD` |
| C5 | format version equals the registered loader's; engine cook version equals the build's | `VERSION_MISMATCH` |
| C6 | platform equals the registry's | `PLATFORM_MISMATCH` |
| C7 | kind and GUID equal the catalogue entry's | `CORRUPT` / `KIND_MISMATCH`, `GUID_MISMATCH` |
| C8 | content hash equals the catalogue's key hash | `CORRUPT` / `CONTENT_HASH_MISMATCH` |
| C9 | file bytes equal the catalogue's and ≤ 16 GiB | `CORRUPT` / `BAD_FILE_SIZE` |
| C10 | table sorted with no duplicate (kind, index) | `CORRUPT` / `BAD_SECTION_TABLE` |
| C11 | each descriptor decodes and validates (`Zenith_DecodeCompressedSectionDesc`) | `CORRUPT` / `BAD_DESCRIPTOR` |
| C12 | each section's offset is at its placement position and `offset + stored bytes ≤ file bytes`, with no overlap | `CORRUPT` / `BAD_SECTION_BOUNDS` |

Section bytes are checked when read (`Zenith_VerifySection`: stored size and stored check, `BAD_SECTION_CHECK`) and when decoded (`DECODE_FAILED`).

### Limits

| Quantity | Limit | Constant |
|---|---|---|
| sections per file | 4,096 | `uASSET_MAX_SECTIONS` |
| file size | 16 GiB | `ulASSET_MAX_COOKED_FILE_BYTES` |
| a section's stored or decoded bytes | 4 GiB | COMPRESS's section limits |
| header and table | 163,904 bytes | 64 + 40 × 4,096 |

Each limit is tested at the limit and at limit + 1 ([Z2-N-ARCH-953]).

## Asset catalogue file

### Catalogue header

`assets.zcat` and every fragment:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZCAT` |
| 4 | 2 | format version | 1 |
| 6 | 2 | flags | bit 0: a fragment; bit 1: a test-kind table follows the kind table (test fixtures only, "Kind table"); other bits 0 |
| 8 | 4 | engine cook version | equal to the build's `uZENITH_ENGINE_COOK_VERSION`, else `COOK_IDENTITY_MISMATCH` (base catalogue) or `CATALOGUE_INVALID` (fragment) |
| 12 | 2 | platform | `Zenith_AssetPlatform` |
| 14 | 2 | kind table count K | equal to the build's `uASSET_KIND_DEFINED_COUNT` (34 in version 1), else `COOK_IDENTITY_MISMATCH` (base) or `CATALOGUE_INVALID` (fragment) |
| 16 | 4 | entry count E | ≤ 1,048,576 |
| 20 | 4 | file entry count F | ≤ 1,048,576 |
| 24 | 4 | dependency count D | ≤ 16,777,216 |
| 28 | 4 | string table bytes S | ≤ 256 MiB |
| 32 | 4 | external reference count X | ≤ 1,048,576; 0 in a base catalogue |
| 36 | 4 | header check | check over the whole file with this field zero |
| 40 | 4 | fallback count P | ≤ 4,096; 0 in a fragment |
| 44 | 4 | reserved | 0 |
| 48 | 8 | content hash | the catalogue's content hash, the one definition every ASSET document uses: the low 64 bits of `Zenith_Hash128Of` over the bytes of the entry table, the file entry table, the external reference table, the fallback table and the string table, concatenated in that order (the entry table holds every asset's key and output hash, the file entry table every non-asset cooked file's hash) |
| 56 | 8 | total bytes | the file size, ≤ 512 MiB |

Then, each at an 8-byte-aligned offset in this order, with zero padding between them: the kind table (K × `uint16`), the test-kind table (16 × `uint16`, only when flag bit 1 is set), the entry table (E × 120 bytes), the path index (E × `uint32`), the dependency table (D × 8 bytes), the file entry table (F × 40 bytes), the external reference table (X × 16 bytes), the fallback table (P × 24 bytes), the string table (S bytes of UTF-8, no terminators). The file ends at the string table's end.

### Kind table

- Element k is the format version of kind code k in this catalogue: the value every entry of kind k carries, or 0 when the catalogue lists no entry of kind k ("no content"). Element 0 (`ASSET_KIND_NONE`) is 0.
- An entry whose format version differs from its kind's element, or of a kind whose element is 0, is `CATALOGUE_INVALID`.
- `Zenith_AssetRegistry::RegisterKindLoader` compares the loader's `m_uFormatVersion` with the merged catalogue's element for the kind: equal passes, 0 passes (the game has no content of that kind), any other value fails the registration with `VERSION_MISMATCH` (RuntimeAPI.md "Zenith_AssetRegistry").
- A merge requires, for each kind, every catalogue whose element is non-zero to carry the same value (`CATALOGUE_INVALID` otherwise); the merged element is that value or 0.
- The test-kind table (header flag bit 1) holds, as element n, the format version of test kind `0x7F00 + n` in this catalogue, 0 for none, under the same entry, registration and merge rules as the kind table. Only test fixtures carry it: `Zenith_AssetCatalogue::Load` refuses flag bit 1, and any entry of a test code, with `CATALOGUE_INVALID` unless its caller passes `bAllowTestKinds` (a registry created with `m_bAllowTestKinds`); an entry of a test code in a catalogue without the table, and an entry of any other code at or above K, is `CATALOGUE_INVALID` everywhere. The cook never writes the table (a test kind in game content is `COOK_ERROR_TEST_KIND_IN_CONTENT`); `Zenith_AssetFixtureWriter` does (`Zenith2/Docs/Design/ASSET/Tests.md → "Fixtures"`). The content hash does not cover the kind tables, which the header check covers.

### Entry table

120 bytes per entry, sorted by GUID (byte order of the binary form), GUIDs unique:

| Offset | Size | Field |
|---|---|---|
| 0 | 16 | asset GUID |
| 16 | 2 | kind |
| 18 | 2 | format version |
| 20 | 2 | flags: bit 0 the asset has hard dependencies; others 0 |
| 22 | 2 | reserved (0) |
| 24 | 4 | object path offset into the string table |
| 28 | 2 | object path length |
| 30 | 2 | logical path length |
| 32 | 4 | logical path offset |
| 36 | 4 | header bytes |
| 40 | 8 | file bytes |
| 48 | 8 | resident estimate bytes |
| 56 | 16 | key hash |
| 72 | 16 | output hash (of the whole cooked file) |
| 88 | 4 | first dependency index |
| 92 | 4 | dependency count: at most D in total; of these, the entries flagged HARD number at most `uASSET_MAX_HARD_DEPENDENCIES` (4,096) per asset, and soft dependencies are bounded only by D |
| 96 | 24 | reserved (0) |

The object path must be `content:/Objects/<xx>/<guid>.<ext>` for the entry's GUID and kind; the logical path must be unique and valid (entry document "Content root mapping"); the path index lists entry indices sorted by logical-path bytes.

### Dependency and string tables

- A dependency is `uint32` target, `uint8` `Zenith_AssetDependencyFlags`, 3 reserved bytes. The flags carry exactly one of `ASSET_DEPENDENCY_FLAG_HARD` (bit 0) and `ASSET_DEPENDENCY_FLAG_SOFT` (bit 1), optionally with `ASSET_DEPENDENCY_FLAG_EXTERNAL` (bit 2); any other bit is `CATALOGUE_INVALID`.
- Without EXTERNAL the target is an entry index of this catalogue; with EXTERNAL it is an index into this catalogue's external reference table ("Catalogue fragments"), and EXTERNAL in a base catalogue is `CATALOGUE_INVALID`.
- An entry's dependencies are contiguous and sorted by (EXTERNAL flag, target); a target out of range, a self-dependency or a cycle among HARD dependencies (checked by Kahn's algorithm over E entries after merge resolution) is `CATALOGUE_INVALID`.
- Strings are referenced by (offset, length) and must be valid UTF-8 within the table.

### File entries

40 bytes each, sorted by path: path offset `uint32`, path length `uint16`, reserved `uint16`, bytes `uint64`, Hash128 (16), reserved 8 bytes. They list every non-asset cooked file (bytecode, the schema blob, shader containers, the precache set, merged configuration) that `Zenith_AssetReader::ReadVerifiedFile` verifies.

### Fallback table

P entries of 24 bytes, sorted by (kind, role), no two equal: kind `uint16`, role `uint16`, entry index `uint32`, the entry's GUID (16 bytes). Role 0 is the kind's SUBSTITUTE fallback; roles 1 … 65,535 are the owner's further always-resident engine assets (GEOM's per-usage fallback textures are roles `usage + 1`). Each row's entry must exist, carry that GUID and be of that kind (`CATALOGUE_INVALID` otherwise). The cook writes the table from the engine fallbacks the ASSET-phase steps declare (`Zenith2/Docs/Design/ASSET/Cook.md → "Engine fallbacks"`); only the base catalogue carries it.

### Catalogue fragments

- A fragment (flag bit 0) has the same layout and lists a subset of assets (an asset pack's, a patch's). PACK decides the partition.
- A dependency of a fragment entry on an asset listed in another catalogue is written with `ASSET_DEPENDENCY_FLAG_EXTERNAL`: its target indexes the fragment's external reference table, whose X elements are the 16-byte binary GUIDs of the referenced assets, sorted by GUID bytes and unique. A dependency on an entry of the fragment itself is an ordinary entry-index dependency.
- `Zenith_AssetCatalogue::Merge` resolves every external reference against the merged entry set: an unresolvable GUID is `CATALOGUE_INVALID` naming the fragment and the GUID, and the previous snapshot stays current. The merged catalogue holds entry-index dependencies only.
- A fragment carries no fallback table (P = 0) and no file entries of the engine's global outputs; a fragment whose K, engine cook version or platform differs from the base's is `CATALOGUE_INVALID`.

### Catalogue limits

| Quantity | Limit | Constant |
|---|---|---|
| entries | 1,048,576 | `uASSET_MAX_CATALOGUE_ENTRIES` |
| file entries | 1,048,576 | `uASSET_MAX_CATALOGUE_ENTRIES` |
| dependencies | 16,777,216 | `uASSET_MAX_CATALOGUE_DEPENDENCIES` |
| hard dependencies per asset | 4,096 | `uASSET_MAX_HARD_DEPENDENCIES` |
| external references | 1,048,576 | `uASSET_MAX_EXTERNAL_REFERENCES` |
| fallback rows | 4,096 | `uASSET_MAX_FALLBACK_ROWS` |
| string table | 256 MiB | `uASSET_MAX_CATALOGUE_STRING_BYTES` |
| file | 512 MiB | `ulASSET_MAX_CATALOGUE_BYTES` |
| fragments merged | 256 | `uASSET_MAX_CATALOGUE_FRAGMENTS` |

## Archive file

The `.zpak` archive is the package and patch mount's container (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount sources"`, archive row). PACK's packer writes it and owns the packing policy (which files go into which archive, their order, alignment beyond the minimum, chunking into several archives, the archive names the mount manifest lists); ASSET's reader in ZenithAssets (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetArchive header"`) accepts exactly the version-1 layout below. A layout change is a format version bump edited in ASSET's and PACK's designs in the same diff.

<!-- assumption: pending PACK -->
PACK's design does not exist yet; this layout is the reader's contract and PACK's writer produces it (cross-area row AS24).
<!-- /assumption -->

Archive location: every archive a mount manifest names sits in the manifest's own directory, in every build. In a development build that directory is the cooked tree the build's `<Name>_Content` target writes (`mount-manifest.json` at its top, the archives beside it); in a packaged build the manifest is `<install>/Content/mount-manifest.json` and the archives are in `<install>/Content/`. The package mount's root is therefore always the directory of the manifest it was read from, never a second configured path (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema": `archive` is a file name relative to the manifest's directory).

### Archive header

64 bytes at offset 0:

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZPAK` |
| 4 | 2 | format version | 1 |
| 6 | 2 | flags | 0 |
| 8 | 4 | entry count N | 0 … 1,048,576 |
| 12 | 4 | string table bytes S | ≤ 128 MiB |
| 16 | 8 | table offset | offset of the entry table; a multiple of 8; ≥ 64 |
| 24 | 8 | archive bytes | the file size; equals the size the container source reports; ≤ 64 GiB |
| 32 | 8 | reserved | 0 |
| 40 | 16 | table hash | `Zenith_Hash128Of` over the entry table and the string table bytes |
| 56 | 4 | header check | check over bytes `[0, 64)` with this field zero |
| 60 | 4 | reserved | 0 |

### Archive entry table

N entries of 40 bytes at the table offset, then S string bytes; the string table ends exactly at the archive's end (the table is the archive's last part, so the packer writes data first):

| Offset in entry | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | path offset | into the string table |
| 4 | 2 | path length | 1 … 503 bytes (a `content:/` virtual path is at most 512 bytes) |
| 6 | 2 | flags | 0 in version 1 (every entry is stored; cooked asset files carry their own section compression) |
| 8 | 8 | data offset | a multiple of `uASSET_SECTION_ALIGNMENT` (16) |
| 16 | 8 | data bytes | ≤ 16 GiB |
| 24 | 16 | data hash | `Zenith_Hash128Of` of the entry's bytes; checked by PACK's packer and by `Zenith_VerifyArchiveData`, never on a run-time read (the cooked header's checks and `ReadVerifiedFile` cover run-time integrity) |

### Archive checks

`Zenith_CreateArchiveMountSource` applies these in order and fails with `FILE_ERROR_ARCHIVE_CORRUPT` (the check number logged) on the first failure, or `FILE_ERROR_OUT_OF_MEMORY`:

| # | Check |
|---|---|
| A1 | the container entry is at least 64 bytes and at most 64 GiB; magic, version, flags and reserved fields as the header table states |
| A2 | header check |
| A3 | N ≤ 1,048,576, S ≤ 128 MiB, table offset a multiple of 8 and ≥ 64, `table offset + 40 N + S` = archive bytes = the container entry size (overflow-checked) |
| A4 | table hash over the bytes read |
| A5 | every path within the string table, valid UTF-8, and `Zenith_ParseVirtualPath("content:/" + path)` succeeds with a relative part equal to the path (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Validation rules and order"`) |
| A6 | entries sorted by path bytes, strictly (no duplicate), and no entry's path followed by `/` is a prefix of another entry's path (a name cannot be both a file and a directory) |
| A7 | every data offset a multiple of 16, every data range within `[64, table offset)`, and the ranges pairwise disjoint (checked on a copy sorted by offset) |

### Archive limits

| Quantity | Limit | Constant |
|---|---|---|
| entries | 1,048,576 | `uASSET_MAX_ARCHIVE_ENTRIES` |
| string table | 128 MiB | `uASSET_MAX_ARCHIVE_STRING_BYTES` |
| archive | 64 GiB | `ulASSET_MAX_ARCHIVE_BYTES` |
| entry | 16 GiB | `ulASSET_MAX_COOKED_FILE_BYTES` |
| path | 503 bytes | the virtual-path rule R1 |

## Meta sidecar schema

### Keys

`<source file name>.meta` beside the source, JSON:

```json
{
	"$schema": "zenith2.asset-meta",
	"schemaVersion": 1,
	"guid": "0b6e2d4c-8f1a-4c2e-9d3b-5a7e1f0c2b44",
	"importer": { "id": "gltf", "settingsVersion": 1 },
	"preset": null,
	"settings": { "lod.mode": "generate", "lod.ratios": [1.0, 0.5, 0.25] },
	"platforms": { "android": { "texture.quality": "medium" } },
	"compression": { "codec": "zstd", "level": 19, "dictionary": null },
	"subAssets": [
		{ "key": "material:Skin", "guid": "3c1f…", "kind": "Material" },
		{ "key": "mesh:Body", "guid": "9a02…", "kind": "Mesh" }
	],
	"labels": ["hero"],
	"physics": { "collision": "ConvexHull", "material": "5d1c0e3a-7b42-4f8e-a1c9-2e6b8d4f0a17" },
	"provenance": { "schema": 1, "origin": "thirdParty", "tool": "Blender", "toolVersion": "4.2.3", "licence": "CC-BY-4.0" }
}
```

| Key | Type | Presence | Rule |
|---|---|---|---|
| `$schema` | string | always | `zenith2.asset-meta` (the document type's schema tag, `Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"`) |
| `schemaVersion` | integer | always | 1 (current); older versions migrate; newer are read-only |
| `guid` | persistent ID text | always | non-null, unique across the game's database; the document's ID |
| `importer.id` | string | always | a registered importer ID claiming the source's extension |
| `importer.settingsVersion` | integer | always | 1 … the importer's current; older migrate ("Importer settings inside meta documents" in Migration.md) |
| `preset` | persistent ID text or `null` | always | a preset asset's GUID (CONTENT's document) |
| `settings` | object | always (may be empty) | keys of the importer's settings; unknown keys kept and warned; `x-removed.<key>` members kept and ignored |
| `platforms` | object | optional | keys `windows`, `android`, each an object of platform-overridable settings |
| `compression` | object | optional | COMPRESS's fragment |
| `subAssets` | array | always (may be empty) | "Sub-asset entries"; sorted by `key` byte order |
| `labels` | array of string | optional | `^[a-z][a-z0-9-]{0,31}$`, unique, sorted; content-rule class selectors and search |
| `physics` | object | optional | PHYS's collision import settings (`Zenith2/Docs/Design/PHYS/Formats.md → "Collision importer settings"`), carried verbatim and validated by PHYS's and CONTENT's code, never by ASSET (cross-area row CT1 (a)); its canonical text joins the settings hash |
| `provenance` | object | optional | CONTENT's origin metadata (`schema`, `origin`, …, `Zenith2/Docs/Design/CONTENT/SharedCache.md → "Content provenance"`), carried verbatim and migrated by its own `schema` member (Migration.md "Member migrations"); never an import-key input |
| `x-<anything>` | any | optional | preserved byte for byte in position |

The file is at most 1 MiB, UTF-8 without a byte-order mark, parsed under the JSON limits with duplicate keys rejected.

### Sub-asset entries

| Key | Type | Rule |
|---|---|---|
| `key` | string | 1 … 255 bytes, `^[a-z]+:[^\u0000-\u001f]+$` (a kind prefix, a colon, a name); unique within the file |
| `guid` | persistent ID text | unique across the database |
| `kind` | string | a kind's script name |
| `orphaned` | boolean | optional, default `false`; `true` when the last import no longer produced the key |
| `exclude` | boolean | optional; CONTENT's owner member (cross-area row CT22 (a)) |
| `materialRemap` | object | optional; CONTENT's owner member: GUID text → GUID text |
| `annotation`, `rig` | persistent ID text | optional; ANIM's owner members |
| `fingerprint` | string | optional; 16 lowercase hex digits, written only by ASSET's reconciliation from `SetProductFingerprint` (Import.md "Product members and fingerprints"); never a key input |
| `x-<anything>` | any | optional; preserved |

Any other member of an entry is an error (`unknown key`). The owner members are validated by their owners (CONTENT's `Zenith_ValidateProductOverridesJson`, ANIM's steps); ASSET checks only their JSON types above.

### Validation messages

Every failure is reported as `<meta path>: <keyPath>: <message>` with one of: `missing`, `unknown key` (warning for non-`x-` keys inside `settings`; error at the top level), `expected <type>, found <type>`, `invalid persistent ID`, `duplicate GUID <guid> (also in <other path>)`, `unknown importer '<id>'`, `importer '<id>' does not claim '<ext>'`, `settingsVersion <n> is newer than importer '<id>' version <m>`, `schemaVersion <n> is newer than this tool's <m>; opened read-only`, `subAssets not sorted by key` (warning; the next tool write sorts), `labels not sorted` (warning; the same), and COMPRESS's validation messages for `compression`.

### Meta migration chain

The document type `Zenith.AssetMeta` is registered into SCENE's framework by `Zenith_RegisterAssetMetaMigrations` (`Zenith2/Docs/Design/ASSET/Migration.md → "The meta document type"`) with schema version 1 and no step; each version increment adds one `Zenith_PfnMigrationStep` over the document tree and the fixtures `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/AssetMeta/v<N>.json` and `v<N>.expected.json` ([Z2-N-CONTRACT-548]). Importer settings objects migrate by their importers' chains inside the type's embedded visitor when `importer.settingsVersion` is older (`Zenith2/Docs/Design/ASSET/Migration.md → "Importer settings inside meta documents"`).

## Tombstone list

`.zenith-tombstones` at the top of each content storage root: `Zenith2/Games/<Name>/<R>/.zenith-tombstones` for each content root `R` of `game.json`, and `Zenith2/Engine/<Lib>/Internal/Content/.zenith-tombstones` for an engine library. It is a tracked file inside a content-root directory (so no new top-level entry of `Games/<Name>/` exists, [Z2-N-ARCH-246]); the scan skips it because its name begins with `.`, so it is never content.

| Rule | Value |
|---|---|
| encoding | UTF-8, LF line endings, one trailing LF, no byte-order mark |
| line | `<GUID in its 36-character text form>` TAB `<the last logical path the GUID had>` |
| order | ascending byte order of the GUID text; written sorted and without duplicates |
| reading | leniently: duplicate lines and unsorted order (the results of a line merge) are accepted and collapsed; a line that does not parse is `COOK_ERROR_GAME_INVALID` naming the file and line |
| limits | 1,048,576 lines, 256 MiB |
| writer | sidecar-writing tool modes (`Zenith2/Docs/Design/ASSET/Database.md → "Orphaned meta files and tombstones"`), by `Zenith_AssetDatabase::WriteTombstoneLists` under the storage root's writer lock and independently of the database lock (it re-reads the list under the lock and merges its lines), through the storage's `WriteFlushed` of `.zenith-tombstones.z2tmp` then `ReplaceAtomically`; and every diff that deletes an asset source's `.meta`, which appends the deleted GUID's line in the same diff (the same section); build cooks never write it |

A line-sorted file lets a line merge of two branches that tombstoned different GUIDs keep both lines; a conflict marker is a parse failure a person resolves by keeping both sides.

## Asset database file

`${ZENITH_OUT}/AssetDB/<Name>/assetdb.zdb`:

| Part | Content |
|---|---|
| header (48 bytes) | magic `ZADB`, format version 1 (u16), flags 0 (u16), record count (u32), edge count (u32), tombstone count (u32), string bytes (u32), header check (u32), total bytes (u64), game name hash (u64), reserved 8 |
| records | per record: GUID, source GUID, kind (u16), state (u8), reserved (u8), importer version (u32), source hash, settings hash, import key (3 × 16), source bytes (u64), stat cookie (u64), last error (u32), and string references (offset u32, length u16) for sub-asset key, source path, logical path, importer ID |
| edges | from GUID, to GUID, kind (u8, a `Zenith_AssetEdgeKind` value 1 … 5, Database.md "Edge kinds"), flags (u8, 0), reserved (u16) — sorted by (from, kind, to) |
| tombstones | GUIDs, sorted (a cache of the tracked tombstone lists) |
| strings | UTF-8 |
| import diagnostics (format version 3; after the cook items part that version 2 adds, Cook.md "Keys and invalidation") | a row count D (u32) and blob bytes B (u32); D rows of 24 bytes: source GUID (16), blob offset (u32, from the first blob), blob length (u32), sorted by GUID, one per source record whose stored diagnostics are not empty (Database.md "Records"); then the blobs, each an "Import diagnostics blob", packed in row order with no padding |

Limits: 4,194,304 records, 67,108,864 edges, 1 GiB total; D at most the record count, each blob at most 1,069,088 bytes, B equal to the sum of the rows' lengths; a violation, a row naming no source record, a blob that fails its own checks, or a bad check discards the file (rebuild). A reader refuses every format version other than its own, so a file of an earlier version is rebuilt by a full scan; a rebuilt record has no stored diagnostics until its source is imported or hits the DDC.

## DDC entry file

`<key>.zddc`:

| Part | Content |
|---|---|
| header (64 bytes) | magic `ZDDC`, format version 1 (u16), payload count P (u16, ≤ 256), key Hash128 (16), key schema version (u32), engine cook version (u32), step version (u32), step ID length (u16, ≤ 64), reserved (u16), total bytes (u64), header check (u32 over header, step ID and payload table with this field zero), reserved (u32) |
| step ID | the producer's step or importer ID, UTF-8 |
| payload table | P entries: name length (u16, ≤ 128), name bytes, padding to 8, stored bytes (u64), Hash128 of the bytes (16) |
| payloads | at 16-aligned offsets in table order |

Entries above 4 GiB are not stored (`DDC_ERROR_ENTRY_TOO_LARGE`).

## Import diagnostics blob

The stored diagnostics of one import (Import.md "Stored diagnostics"), the same bytes in the database file's "import diagnostics" part and in the DDC payload `asset.importDiagnostics`. Little-endian, field by field:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | magic `ZIDG` (`0x5A 0x49 0x44 0x47`) |
| 4 | 2 | format version 1 (u16) |
| 6 | 2 | flags, 0 (u16, reserved) |
| 8 | 4 | entry count E (u32), 0 … 256 |
| 12 | 4 | dropped count (u32): the import's `Report` calls beyond the cap |
| 16 | 4 | string bytes S (u32) |
| 20 | 4 | check (u32): `Zenith_Hash64` low 32 bits over the whole blob with this field zero |
| 24 | 8 | reserved, 0 |
| 32 | 16 × E | entries, in `Report` order: level (u8, 1 … 3), reserved (u8, 0), code length (u16, 0 … 64), code offset (u32), message length (u16, 0 … 4,096), reserved (u16, 0), message offset (u32); offsets from the start of the string area |
| 32 + 16E | S | string area: for each entry in order, its code bytes then its message bytes, no terminators, no padding |

Checks, each refused as a corrupt blob: the magic, the version, every reserved field zero, E ≤ 256, the total length exactly 32 + 16E + S, the check, each entry's level, each code matching `^[A-Z0-9_]{1,64}$` without the prefixes `COOK_` and `IMPORT_` (or empty), each message valid UTF-8 of at most 4,096 bytes, and every offset equal to the running sum of the lengths before it, so each set of diagnostics has exactly one blob. Limits: at most 1,069,088 bytes (32 + 256 × (16 + 64 + 4,096)). An import with no diagnostics and no dropped calls is the 32-byte blob with E, S and the dropped count zero; the database stores no row for it.

## Content rule table

`Zenith2/Docs/ContentSpecs.rules.json`, the machine-readable form of the numeric cells of `Zenith2/Docs/ContentSpecs.md`, which the cook reads at start (and lists in its depfile); the cook never parses Markdown.

<!-- assumption: pending BUDGET, CONTENT -->
The owner of `Zenith2/Docs/ContentSpecs.md` maintains this file beside it, and the test `Test_ContentRules_TableAgreesWithContentSpecs` keeps the two equal (cross-area row AS27).
<!-- /assumption -->

```json
{
	"$schema": "zenith2.content-rules",
	"schemaVersion": 1,
	"tables": [
		{
			"name": "Mesh triangle counts",
			"kind": "Mesh",
			"quantity": "triangles",
			"unit": "count",
			"classes": [ { "selector": "hero", "limit": 150000 }, { "selector": "*", "limit": 50000 } ]
		}
	]
}
```

| Key | Type | Rule |
|---|---|---|
| `$schema`, `schemaVersion` | string, integer | `zenith2.content-rules`, 1 |
| `tables[]` | array | unique `name`; sorted by `name` byte order |
| `tables[].name` | string | the heading of the ContentSpecs.md table the row set comes from, verbatim |
| `tables[].kind` | string | a kind's script name |
| `tables[].quantity` | string | `^[a-z][a-zA-Z0-9.]{0,63}$`; the name a step's `Zenith_ContentRule` measures (`Zenith2/Docs/Design/ASSET/Cook.md → "Content rules"`) |
| `tables[].unit` | string | `count`, `bytes`, `texels`, `metres`, `milliseconds` or `ratio` |
| `tables[].classes[]` | array | at least one; `selector` a `.meta` label (`^[a-z][a-z0-9-]{0,31}$`) or `*`; `limit` a finite number ≥ 0; the first class whose selector matches one of the asset's labels, else `*`, applies; `*` appears exactly once and last |

The file is at most 4 MiB; a rule failure is `COOK_ERROR_RULE_TABLE_INVALID` with the key path, exit code 3.

## Cook journal

`.cook/journal.jsonl`, one JSON object per line: `{"start":{"game":…,"platform":…,"planHash":"<32 hex>"}}` first, then `{"published":{"path":"Objects/0b/…ztex","key":"<32 hex>","hash":"<32 hex>","bytes":1234}}` per output, and `{"end":{"outcome":"ok|failed"}}` last. A line that does not parse ends the journal (a torn last line is ignored).

## Cook lock file

Three kinds of lock file serialise the processes that write one cook output tree, one asset database or one document storage root (`Zenith2/Docs/Design/ASSET/Cook.md → "Output tree and database locks"`). ZenithCook implements the output-tree and database locks itself. The storage-root writer lock is taken through SCENE's `Zenith_DocumentWriterLock`, returned by `Zenith_DocumentStorage::GetWriterLock()` and implemented by ZenithDocumentHost over the file this section specifies (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Storage-root writer lock"`): ASSET owns that file's path, open mode and content line, SCENE's implementation follows them, and ZenithCook's only own access to it is the build cook's read-only probe.

| Lock | Path | Taken by | Held |
|---|---|---|---|
| output tree | `<out>/.cook/lock` | every mode that writes `<out>`: cook, `--validate-only`, `--serve` | for the run; the server for its whole session |
| database | `${ZENITH_OUT}/AssetDB/<Name>/assetdb.lock` | every process that would `Save` the database | from start to exit when taken; the server for its whole session |
| storage-root writer | `<storage root>/.migration/writer.lock`, one per document storage root (the game directory, each engine library's `Internal/Content/`) | every writer of the root's documents, through the writable storage's `GetWriterLock()`: the sidecar-writing modes, the server per sidecar-writing request, and SCENE's migrator for each editor save batch, `ApplyPlan` and recovery | a CLI mode for the run; the server and the migrator for one request or batch; a build cook only probes it, read-only |

- A lock is held by holding its file open; the file itself may outlive its holder, and an unheld file is no lock. The tools build only for Windows hosts (`ZENITH_TOOLS_HOST`), so the mechanism is Windows': the holder opens the file with `CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)` (creating `.migration/` first for a storage root) and truncates it; while that handle is open, another process's open requesting write access fails with `ERROR_SHARING_VIOLATION`, which means "held". A process refused the lock reads the holder's content with `CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)`: its share mode must admit the holder's write access, and the holder's `FILE_SHARE_READ` admits the read. The build cook's probe of a storage root (`Zenith_ProbeStorageRootWriterLock`) is the holder's open with `OPEN_EXISTING` in place of `OPEN_ALWAYS`, closed at once when it succeeds, so it never creates, truncates or writes the file; a missing file is an unheld lock. The operating system releases the lock when its holder exits by any path, crashes included, so no lock is ever broken by age or by a PID check.
- Content, written by the holder after taking it and readable by others while held: one JSON line `{"pid": <u32>, "mode": "cook" | "validate" | "serve" | "assign" | "fix-duplicates" | "migrate" | "schema-lock" | "editor", "game": <string>, "platform": <string> or null}` and LF. For the storage-root lock ZenithDocumentHost writes it from the storage desc: `mode` is `m_szWriterMode` (`"editor"`, its default, for SCENE's migrator in the editor process; ZenithCook's writers pass `"assign"`, `"fix-duplicates"`, `"migrate"` or `"serve"`), `game` is `m_szGame` and `platform` is `m_szPlatform` (null for ZenithCook's CLI modes). A process refused the lock reads it to name the holder in its error; content that does not parse is reported as "unknown holder".

## Cook report

`.cook/cook-report.json`:

```json
{
	"schema": "zenith2.cook-report",
	"schemaVersion": 1,
	"game": "OpenWorldSlice",
	"platform": "windows",
	"contentHash": "a3c9e7b22485d1f0",
	"summary": { "items": 812, "cooked": 3, "upToDate": 806, "ddcLocalHits": 2, "ddcSharedHits": 1, "failed": 0, "warnings": 4, "bytesWritten": 1048576 },
	"diagnostics": [ { "severity": "warning", "code": "COOK_WARNING_UNRECOGNISED_FILE", "asset": null, "path": "Games/OpenWorldSlice/Assets/readme.txt", "message": "…" } ],
	"validation": [ ],
	"members": { "vertexFallbacks": [ ], "meshes": [ ], "streamSets": [ ], "materials": [ ], "permutations": { }, "textures": [ ] }
}
```

A `diagnostics` element is `{severity, code, asset, path, message}` with `severity` `"error"`, `"warning"` or `"info"`, `asset` the 36-character GUID text or null and `path` a string (`""` when empty), plus `"replayed": true` on an import diagnostic replayed from storage (Import.md "Replay into the cook report"); a live element has no `replayed` member. An import diagnostic's `code` is the importer's code, or `"COOK_IMPORT_DIAGNOSTIC"` without one (Import.md "Diagnostic codes"). `diagnostics` and `validation` are sorted by (severity in the order error, warning, info; code; asset GUID; path), entries of one source equal in all four keeping their import's stored order and other ties broken by message bytes; the report's `schemaVersion` stays 1 (the report is rewritten by every cook and no released reader exists, so `replayed` is part of version 1); `members` holds each step's contributed member under its name, sorted by name; the report holds no time, host or user; `.cook/cook-timings.json` holds per-phase and per-step wall-clock times.

## Provenance record lines

`.cook/provenance.jsonl`, one line per output, sorted by output path, each the fields of Database.md "Provenance record content": `{"output":{"path":…,"hash":…,"bytes":…},"asset":{"guid":…,"kind":…,"logicalPath":…},"sources":[{"path":…,"hash":…}],"import":{"importer":…,"version":…,"settingsHash":…,"key":…},"cook":{"step":…,"version":…,"key":…,"platform":…},"toolchain":{…},"ddc":"miss|local|shared","origin":{…}}`.

## Dependency file

`cook.d` in the Make depfile form Ninja reads (`DEPFILE`, [Z2-N-ARCH-207]):

```text
<absolute path of .stamp>: \
  <absolute path of each input, one per line, spaces escaped as '\ '> \
  …
```

Inputs are sorted by byte order and listed once each; paths use `/`.

## Cook server messages

### Framing

- One UTF-8 JSON object per line, terminated by LF: requests on the server's stdin, everything the server sends on its stdout. The server's log records go to stderr and are never protocol.
- A line is at most `uCOOK_SERVER_MAX_LINE_BYTES` = 4,194,304 bytes excluding its LF (a `.meta` is at most 1 MiB, so every single record fits), parsed under `Zenith_JsonLimits` with duplicate keys refused.
- A result that can hold many records is paged: the server sends zero or more `partial` lines, each with at most `uCOOK_SERVER_MAX_ITEMS_PER_PAGE` = 256 items and within the line limit, then one final `result` line carrying `count` (items across all pages) and `pages`. The client concatenates the pages' items in `page` order. No other line of that request follows its final line.
- A page closes early when its next item would take its line past the limit. A single item whose own serialization exceeds the limit (a provenance record listing an extreme number of sources) ends its request with `error` `COOK_ERROR_RESULT_TOO_LARGE` naming the item, after the pages already sent, which stand; the session continues.
- The protocol version is `uCOOK_SERVER_PROTOCOL_VERSION` = 1; every change to a message in this section bumps it.

| Line | Direction | Shape |
|---|---|---|
| request | to the server | `{"id": <u64>, "request": "<name>", "args": { … }}`; `id` unique among the session's requests not yet finished (rule 2 of "Session rules" answers a duplicate); `args` required (may be `{}`) |
| result | from the server | `{"id": <u64>, "result": { … }}` — the request's final line on success |
| error | from the server | `{"id": <u64> or null, "error": {"code": "<COOK_ERROR_…>", "message": <string>, "asset": <guid> or null, "path": <string> or null}}` — the final line on failure; `id` null when the offending line carried no usable `id` |
| partial | from the server | `{"id": <u64>, "page": <u32 from 0>, "items": [ … ]}` |
| notification | from the server | `{"notify": "<name>", …}` ("Notifications") |

### Session rules

1. The first request of a session is `hello`; any other request before it is answered with `error` `COOK_ERROR_PROTOCOL` ("hello required") and not run. A second `hello` is answered with `error` `COOK_ERROR_PROTOCOL` ("hello already done") under its own `id` and changes nothing.
2. A line that is not a JSON object, exceeds the line limit (the server discards through the next LF), lacks `id`, `request` or `args`, names an unknown request, carries an unknown `args` key, or has a key of the wrong type or out of range, is answered with `error` `COOK_ERROR_PROTOCOL` whose message names the request name or the key path; the session continues. A request whose `id` equals that of a request not yet finished is answered with `error` `COOK_ERROR_PROTOCOL` with `id` null (so no final line is attributed to the unfinished request) and a message naming the duplicate `id`, and is not run. After 16 consecutive such lines — any of these refusals, a second `hello` included — the server exits with code 2.
3. Requests run one at a time in arrival order; `hello`, `cancel`, `watch` and `shutdown` are control requests answered on arrival even while another request runs.
4. `cancel` of a request still queued removes it and answers the target with `error` `COOK_ERROR_CANCELLED` at once; of the running request, interrupts it at its next item boundary, after which the target's final line is `error` `COOK_ERROR_CANCELLED` (pages already sent stand); of an unknown or finished `id`, changes nothing. The `cancel` request itself always gets a `result`, sent before the target's final line.
5. `shutdown`, on arrival: every queued request is answered `error` `COOK_ERROR_CANCELLED` at once; the running request, if any, is interrupted at its next item boundary and answered `COOK_ERROR_CANCELLED`; then the server saves the database, releases its locks ("Cook lock file"), answers the `shutdown` with `result`, closes stdout and exits with code 0. EOF on stdin is a `shutdown` with no reply.
6. The server holds its output tree's lock for the whole session ("Cook lock file"); a server that cannot take it at start answers the first `hello` with `error` `COOK_ERROR_OUTPUT_LOCKED` naming the holder and exits with code 6.

### Requests

Presence: R required, O optional (default in brackets). GUIDs are 36-character text; hashes lowercase hex; `kind` a script name; `state` a `Zenith_AssetRecordState` name without prefix (`CURRENT`).

| Request | `args` | Final `result` (and paged items) |
|---|---|---|
| `hello` | `protocolVersion` u32 R; `engineCookVersion` u32 R; `client` string R (1 … 64 bytes) | `protocolVersion` 1, `engineCookVersion` u32, `game` string, `platform` string, `serverBuildId` 32 hex; a `protocolVersion` or `engineCookVersion` differing from the server's is answered `COOK_ERROR_PROTOCOL_VERSION` and the server exits with code 2 |
| `list` | `prefix` string O [`content:/`]; `kinds` array of kind O [all]; `states` array of state O [all] | paged items: record summaries `{guid, kind, logicalPath, sourcePath, state, lastError}` (`lastError` a `COOK_ERROR_…` name or null), ascending logical path; `count`, `pages` |
| `find` | `text` string R (1 … 256 bytes); `limit` u32 O [1,000] (≤ 100,000) | paged record summaries whose logical path or a label contains `text` (ASCII case-insensitive), ascending logical path; `count`, `pages`, `truncated` bool |
| `record` | `guid` R | `record`: the summary fields plus `preset` (GUID or null), `provenance` (object or null), `metaPath`, `metaSha256` (64 hex: `Zenith_FormatSha256Hex` of `Zenith_Sha256Digest` over the `.meta` file's bytes, `Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Sha256"`; `expectedMetaSha256` of `setSettings` is compared in the same form), `importer {id, version, settingsVersion}`, `sourceHash`, `settingsHash`, `importKey`, `subAssets` (every entry with all its members, owner members and `fingerprint` included), `settings` object, `platforms` object, `labels` array (the `.meta` objects verbatim) |
| `dependencies`, `dependents` | `guid` R; `edges` array O [all] of `importInput`, `hardReference`, `softReference`, `cookInput`, `settingsInput`; `transitive` bool O [false] | paged items `{from, to, edge}` sorted by (from, edge, to); `count`, `pages` |
| `assignGuids` | `sources` array of Zenith2-relative source paths R (1 … 10,000) | `created [{source, metaPath, guid}]`, `presets [{source, preset, glob}]` (the preset each new `.meta` got from `Zenith_ImportPresetSet::Select`, `preset` null and `glob` `""` when none matched, cross-area rows CT3 (c), CT24 (e)), `suggestions [{source, orphanedMeta, guid}]` (probable moves for EDIT to confirm) |
| `setSettings` | `guid` R; `expectedMetaSha256` 64 hex R; `settings` object O [absent: unchanged]; `platform` `windows`, `android` or null O [null: the `settings` object]; `preset` GUID or null O; `provenance` object or null O; `subAssetMembers` array O of `{key, members}` (each replaces that entry's owner members, `members` an object of `exclude`, `materialRemap`, `annotation`, `rig` only); `remapSubAssets` array O of `{from, to}` (an orphaned key's GUID and members move onto the live key `to`, whose discarded GUID is tombstoned); `removeOrphanedSubAssets` array O of keys (each orphaned entry removed and its GUID tombstoned); `restoreSubAssets` array O of whole entries (each restored as recorded; its GUID must be on the entry now or tombstoned with this `.meta`'s logical path); at least one optional key present; arrays 1 … 10,000 | `metaPath`, `metaSha256`, `tombstoned [guid]`; one journaled batch for every change of the request; a `.meta` changed since the digest is `COOK_ERROR_META_CHANGED`; a key naming no entry, a remap whose `from` is live or `to` orphaned, a removal of a live entry, or a restore whose GUID belongs elsewhere is `COOK_ERROR_PROTOCOL` naming the element, and nothing is written (cross-area rows CT1 (b), CT22 (e)) |
| `import`, `reimport` | `guids` array R (1 … 10,000) | `changed [guid]`, `failed [{guid, code}]`; progress notifications during the run |
| `cook` | `guids` array R (1 … 10,000) or the string `all` | paged items `{path, guid (or null for a global output), hash}`; `count`, `pages`, `catalogueChanged` bool, `contentHash` 16 hex, `failed` u32 |
| `validate` | `guids` array or `all` R | paged findings `{severity, code, guid, logicalPath, sourcePath, rule, message}`; `count`, `pages`, `errors` u32, `warnings` u32 |
| `provenance` | `query` string R (a GUID, logical path or cooked path); `depth` u32 O [8] (1 … 8) | paged items: the provenance record objects of "Provenance record lines", the queried output first, then each cook input's record breadth-first to the depth, each record once; `count`, `pages`; a record larger than the line limit ends the request with `COOK_ERROR_RESULT_TOO_LARGE` ("Framing") |
| `property` | `guids` array of GUIDs R (1 … 100,000); `platform` `windows` or `android` R; `name` string R (`^[a-z][a-zA-Z0-9.]{0,63}$`) | paged items in ascending GUID byte order, one per distinct GUID: `{guid, bytes}` (`bytes` standard base64 with padding of the producer's published property) or `{guid, error}` (`error` a `COOK_ERROR_…` name, Import.md "Published-property inputs"); `count`, `pages`, `failed` u32; each producer item that misses is cooked into the DDC, never into the tree; an item larger than the line limit ends the request with `COOK_ERROR_RESULT_TOO_LARGE` ("Framing") |
| `migratePreview` | `paths` array of `.meta` document paths O [every `.meta` of the game's roots] | paged items: one object per plan entry, exactly as the framework's `WriteReportJson` writes an entry (`zenith2.migration-preview` v1); `count`, `pages`, `changed` u32, `blocked` u32, `previewToken` 32 hex (a hash over every entry's path and old and new digests) |
| `migrateApply` | `paths` array O [as the preview]; `previewToken` 32 hex R | `written` u32, `batch` string; a token differing from a fresh plan's is `COOK_ERROR_META_CHANGED` and nothing is written |
| `watch` | `enable` bool R; `debounceMs` u32 O [250] (50 … 5,000) | `enabled` bool, `roots` array of Zenith2-relative content roots watched |
| `cancel` | `target` u64 R | `cancelled` bool, `state` `queued`, `running` or `unknown` |
| `shutdown` | `{}` | `{}` |

### Notifications

| Notification | Shape | Sent |
|---|---|---|
| `progress` | `{"notify": "progress", "id": <u64> or null, "phase": "scan", "import", "cook", "validate" or "migrate", "done": <u32>, "total": <u32>}` | during a request (`id`), or during a watch-driven run (`id` null), at most one per 100 ms per request or run |
| `diagnostic` | `{"notify": "diagnostic", "id": <u64> or null, "severity": "error", "warning" or "info", "code": <string>, "guid": <guid> or null, "path": <string> or null, "message": <string>}` | a request's diagnostic (`id`), or a watch-driven run's (`id` null) |
| `changed` | `{"notify": "changed", "sources": [<Zenith2-relative paths>, at most 256], "more": <bool>}` | watching is enabled and sources changed on disk, after the debounce and before the server rescans; `more` true when further lines follow for the same change set |
| `published` | `{"notify": "published", "guids": [<guids>, at most 256], "more": <bool>, "catalogueChanged": <bool>, "contentHash": <16 hex>}` | after a watch-driven scan, import and cook of the changed sources published outputs; the editor then calls `OnMountsChanged` on its play worlds' registries and `RequestReload` for each listed GUID that is ACTIVE (`Zenith2/Docs/Design/ASSET/Loading.md → "Development reload"`) |

A watch-driven run is an internal request that runs in arrival order like a client request; a client request arriving during it waits.

## Worker messages

The driver and its `ZenithCook --worker <pipe name>` processes use the framing of "Cook server messages" over a named pipe (the tools build only for Windows hosts, `ZENITH_TOOLS_HOST`), with these lines:

| Line | Direction | Shape |
|---|---|---|
| `workerHello` | worker → driver, first line | `{"workerHello": {"protocolVersion": 1, "engineCookVersion": <u32>, "pid": <u32>}}`; a mismatch makes the driver kill the worker and stop the cook with exit code 5 (`COOK_ERROR_PROTOCOL_VERSION`) |
| `workItem` | driver → worker | `{"workItem": {"itemId": <u64>, "step": <id>, "stepVersion": <u32>, "key": <32 hex>, "platform": <string>, "guid": <guid> or null, "inputs": [<32 hex DDC keys>], "settings": { … }}}` |
| `workResult` | worker → driver | `{"workResult": {"itemId": <u64>, "key": <32 hex>, "outcome": "ok" or "failed", "error": {"code": <COOK_ERROR_…>, "detail": <u16>, "message": <string>} or null, "payloads": [<names>], "diagnostics": [<diagnostic objects, at most 64>]}}` |
| `workShutdown` | driver → worker | `{"workShutdown": {}}`; the worker exits with code 0 |

Inputs and outputs travel as DDC entries on disk, keyed by the item's input and output keys; a pipe line never carries payload bytes. For an import item the worker populates the import record entry and the intermediate entries of `Zenith2/Docs/Design/ASSET/DDC.md → "Import entries"` into the local store, `payloads` lists the product GUIDs it stored, and `diagnostics` is empty: the import's diagnostics travel in the record entry's `asset.importDiagnostics` payload, so none of the 256 is cut to the 64 a `workResult` line carries for a cook step.

## Load result payload

`Zenith_AssetLoadResult.h` declares the 24-byte record a requester writes as the payload of a `PUBLICATION_KIND_GENERIC_RECORDED` publication of a FAILED load (`Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"`, the `ctx:RequestLoad` rows; `Zenith2/Docs/Design/ASSET/Loading.md → "Publication to simulation"`). A SUCCEEDED load publishes `PUBLICATION_KIND_GENERIC_RECOMPUTED` with no payload, and a CANCELLED completion is never published, so version 1 writes status 3 only:

| Offset | Size | Field |
|---|---|---|
| 0 | 16 | asset GUID |
| 16 | 1 | status: 3 (`OPERATION_STATUS_FAILED`); any other value is refused |
| 17 | 1 | error code (`Zenith_AssetErrorCode`, non-zero) |
| 18 | 2 | error detail |
| 20 | 4 | reserved (0) |

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetLoadResult.h
/// @contract value Zenith_AssetLoadResultRecord
/// Ownership: Plain value; serialized only through the two functions below.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_EncodeAssetLoadResult returns ASSET_ERROR_INVALID_REQUEST for a status other than FAILED, a zero error code or
///   an output span shorter than 24 bytes; Zenith_DecodeAssetLoadResult returns ASSET_ERROR_CORRUPT for a buffer other than
///   24 bytes, a status other than 3, a zero error code or a non-zero reserved field.
/// Version: since Z2 Stage 2. Part of the replay payloads; a layout change is a replay format version bump.
/// @endcontract
struct Zenith_AssetLoadResultRecord { const Zenith_AssetGUID m_xAsset = {}; const Zenith_OperationStatus m_eStatus = {}; const Zenith_AssetErrorCode m_eError = {}; const uint16 m_uDetail = 0; };
Zenith_Result<void, Zenith_AssetError> Zenith_EncodeAssetLoadResult(const Zenith_AssetLoadResultRecord& xRecord,
	Zenith_MutableByteSpan xOut24);
Zenith_Result<Zenith_AssetLoadResultRecord, Zenith_AssetError> Zenith_DecodeAssetLoadResult(Zenith_ByteSpan xIn);
```

SCENE's scene loads publish under their own source kind with SCENE's 2-byte `Zenith_SceneErrorCode` payload (`Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"`): a scene load's failure is the scene loader's result, which already maps every asset failure it met into a scene error, so the two payloads describe different operations and are not merged.
