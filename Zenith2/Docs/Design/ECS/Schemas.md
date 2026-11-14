# ECS design — data-defined component schemas

This file is part of the ECS design family; the entry document is `Zenith2/Docs/Design/ECS.md`. It specifies the schema front end of ZenithReflect: validation of `Games/<Name>/Schemas/*.schema.json` with its complete error catalogue, default values, authority and arbitration in schemas, the `schemas.zreg` binary format, the schema lock file, schema migration, Luau type emission and the cook and editor integration. The schema file format itself (every key, the type grammar, the inline sizes, the example) is `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" and is not restated; this file adds what a worker needs to implement and test it (Z2-D-020).

## Scope of this file

| Piece | Where specified |
|---|---|
| the schema file format, keys, type grammar and limits | `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" |
| registration order and freeze | `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Registration at game load"; `Zenith2/Docs/Design/ECS/Reflect.md` → "Registry internals" |
| storage in every backend | `Zenith2/Docs/Design/ECS/Backends.md` |
| the Luau view of schema types | `Zenith2/Docs/Scripting/Bindings.md` → "Data-defined component types" (SCRIPT) |
| everything else about schemas | this file |

## Zenith_Schema header

`Zenith_SchemaErrorCode`, `Zenith_SchemaDiagnostic` and `Zenith_SchemaError` are `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Registration at game load" (subsection "Validation errors"), member for member; one code is appended.

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_Schema.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"

/// @contract enum Zenith_SchemaErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The meaning of each code is its rows in § "Validation rules catalogue".
/// Version: since Z2 Stage 2. An enumerator never changes meaning.
/// @endcontract
enum Zenith_SchemaErrorCode : uint16
{
	SCHEMA_ERROR_PARSE = 1,
	SCHEMA_ERROR_FORMAT_VERSION,
	SCHEMA_ERROR_MISSING_KEY,
	SCHEMA_ERROR_UNKNOWN_KEY,
	SCHEMA_ERROR_BAD_NAME,
	SCHEMA_ERROR_DUPLICATE_NAME,
	SCHEMA_ERROR_DUPLICATE_FIELD_ID,
	SCHEMA_ERROR_REUSED_FIELD_ID,
	SCHEMA_ERROR_BAD_TYPE,
	SCHEMA_ERROR_BAD_DEFAULT,
	SCHEMA_ERROR_LIMIT,
	SCHEMA_ERROR_TYPE_ID_COLLISION,
	SCHEMA_ERROR_MISSING_AUTHORITY,
	SCHEMA_ERROR_BAD_AUTHORITY,
	SCHEMA_ERROR_BAD_ARBITRATION,
	SCHEMA_ERROR_MIGRATION_CHAIN,
	SCHEMA_ERROR_BLOB_CORRUPT,
	// elaboration: appended, value 18
	SCHEMA_ERROR_REGISTRY_FROZEN,       // a schema registration after Freeze outside a hot-reload window
};

/// @contract value Zenith_SchemaDiagnostic
/// Ownership: Plain value held by the type registry; the three views reference registry-owned text.
/// Thread affinity: IMMUTABLE.
/// Invalidation: Invalid when the next schema registration starts ([Z2-N-ARCH-613]).
/// Units: None.
/// Errors: None; it describes one.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SchemaDiagnostic
{
	const Zenith_SchemaErrorCode m_eCode = {};
	const Zenith_StringView m_strSourcePath = {}; // schema file, or "schemas.zreg#<index>"
	const Zenith_StringView m_strKeyPath = {}; // JSON path, for example "fields[3].type"
	const Zenith_StringView m_strMessage = {};
};

/// @contract value Zenith_SchemaError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480].
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: None.
/// Errors: The code of the first diagnostic and the count; no text ([Z2-N-ARCH-613]).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SchemaError
{
	Zenith_SchemaErrorCode m_eCode = SCHEMA_ERROR_PARSE;
	uint32 m_uDiagnosticCount = 0;
};

constexpr const char* Zenith_ToString(Zenith_SchemaErrorCode eCode);
```

A diagnostic's full line, as logged and as printed by the cook, is `<file>: <keyPath>: <message>` ([Z2-N-ARCH-598]); `<keyPath>` uses `.` for members and `[i]` for elements, `$` for the document root.

## Schema validation pipeline

`RegisterSchemaJson(xJsonBytes, strSourcePath)` runs, in order, collecting every diagnostic of the file (it never stops at the first):

1. **Size**: above 1 MiB is `SCHEMA_ERROR_LIMIT` at `$` and ends the file.
2. **Parse**: `Zenith_JsonDocument::Parse` with `Zenith_JsonLimits::ForSchemas()` (`Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"); a read error is `SCHEMA_ERROR_PARSE` with the reader's line and column in the message and ends the file (duplicate keys are a parse error).
3. **Shape**: the top-level keys and every field, struct, arbitration and migration object against the key tables of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file"; `x-`-prefixed keys are accepted anywhere and ignored ([Z2-N-ARCH-600]).
4. **Names**, **IDs**, **types**, **defaults**, **authority**, **arbitration**, **limits** — the rules of § "Validation rules catalogue", per object in document order.
5. **Lock comparison** against `Schemas/schemas.lock.json` (§ "The schema lock file"), when the caller supplies the lock (the cook and `ZenithNewGame validate` do; the runtime blob path does not, because the cook already enforced it).
6. **Migrations** (§ "Chain validation").
7. **Registration**: build the type through `Zenith_RuntimeTypeBuilder` (structs first, in dependency order, then the component or event), which computes the layout and derives the type ID; a registry-level failure maps to the schema code in parentheses: `TYPE_ID_COLLISION` (`SCHEMA_ERROR_TYPE_ID_COLLISION`), `DUPLICATE_STABLE_NAME` and `BAD_NAME` (`SCHEMA_ERROR_DUPLICATE_NAME`), `LAYOUT` (`SCHEMA_ERROR_LIMIT`), `FROZEN` (`SCHEMA_ERROR_REGISTRY_FROZEN`).

When any diagnostic exists, nothing of the file is registered and the call returns `Zenith_SchemaError { first code, count }`; a data-defined type never registers partially ([Z2-N-ARCH-599]). Struct names are file-local: a struct `DamageRecord` of `Stats.schema.json` registers as the struct type `<stableName>.DamageRecord` (stable name `OpenWorldSlice.Stats.DamageRecord`, script name `StatsDamageRecord`), so two schema files may declare structs of the same name.

## Validation rules catalogue

Every rule has one code and one message template; `Test_Schema_EachErrorCode_Reported` carries one fixture per row group and asserts the code and the exact message ([Z2-N-ARCH-598]). Placeholders in angle brackets are substituted; nothing else varies.

| Code | Rule | Key path | Message |
|---|---|---|---|
| `PARSE` | not JSON, or a JSON reader limit exceeded | `$` | `not valid JSON: <reader error name> at line <l> column <c>` |
| `FORMAT_VERSION` | `$schema` is not `zenith2.component-schema` | `$schema` | `expected "zenith2.component-schema"` |
| `FORMAT_VERSION` | `schemaFormatVersion` is not 1 | `schemaFormatVersion` | `unsupported schema format version <v>; this build reads 1` |
| `MISSING_KEY` | a required key is absent (`$schema`, `schemaFormatVersion`, `component`, `stableName`, `version`, `fields`; per field `id`, `name`, `type`; `maxCount` on a `container<T>`; `migrations` when `version` > 1; per struct `name`, `fields`) | the containing object | `missing required key '<key>'` |
| `UNKNOWN_KEY` | a key outside the tables that is not `x-`-prefixed; for `kind: event`, any of `authority`, `arbitration`, `networkAuthority`, `replicated`, `restoreAfterCinematic` ([Z2-N-ARCH-610]) | the key | `unknown key '<key>'; extension keys start with x-` / `key '<key>' is not allowed on an event schema` |
| `BAD_NAME` | `component`, a field `name` or a struct `name` fails `^[A-Z][A-Za-z0-9]{0,63}$` | the key | `'<value>' is not a valid name: it must match ^[A-Z][A-Za-z0-9]{0,63}$` |
| `BAD_NAME` | `stableName` fails `^[A-Z][A-Za-z0-9]*(\.[A-Z][A-Za-z0-9]*)+$`, or its first segment is not the game's name | `stableName` | `'<value>' is not a valid stable name: <Game>.<Name> segments, first segment '<Game>'` |
| `BAD_NAME` | `stableName` differs from the lock's stable name for the same file | `stableName` | `stableName changed from '<old>' to '<new>'; a stable name never changes; create a new schema instead` |
| `DUPLICATE_NAME` | two fields of one component or struct share a name; two structs share a name; the `component` name equals another registered component, struct or event script name | the second occurrence | `name '<name>' is already used by <where>` |
| `DUPLICATE_FIELD_ID` | two fields of one component or struct share an `id` | the second field's `id` | `field id <id> is already used by field '<name>'` |
| `REUSED_FIELD_ID` | a field `id` appears in `retiredFieldIds` ([Z2-N-ARCH-594]) | `fields[i].id` | `field id <id> is retired and can never be reused` |
| `REUSED_FIELD_ID` | `retiredFieldIds` lacks an ID the lock lists as retired, or a field ID the lock lists as live is neither present nor retired | `retiredFieldIds` | `field id <id> was <retired|a field> in version <v>; add it to retiredFieldIds` |
| `BAD_TYPE` | `type` does not parse by the type grammar | `fields[i].type` | `'<type>' is not a valid field type` |
| `BAD_TYPE` | `asset<K>` with an unregistered asset kind | `fields[i].type` | `unknown asset kind '<K>'` |
| `BAD_TYPE` | a struct name that is not an entry of `structs` | `fields[i].type` | `unknown struct '<name>'` |
| `BAD_TYPE` | `container<container<…>>`, or a struct containing a container used as a container element, or a container inside a struct ([Z2-N-ARCH-595]) | `fields[i].type` | `containers cannot be nested` |
| `BAD_TYPE` | a `container<T>` field on an event schema | `fields[i].type` | `event schemas cannot hold containers` |
| `BAD_TYPE` | `min`, `max` on a non-numeric field; `maxCount` on a non-container | the key | `'<key>' applies to <numeric|container> fields only` |
| `BAD_DEFAULT` | `default` is not a value of the field's type (JSON kind, element count, integer range, 64-bit decimal string form, 36-character persistent ID) | `fields[i].default` | `default <json> is not a valid <type> value` |
| `BAD_DEFAULT` | a numeric default outside `[min, max]` | `fields[i].default` | `default <v> is outside [<min>, <max>]` |
| `BAD_DEFAULT` | a non-null default on an `entity` field (§ "Default values") | `fields[i].default` | `entity fields default to null; assign entity references in scenes or prefabs` |
| `BAD_DEFAULT` | an `asset<K>` default that is neither `null` nor the 36-character persistent-ID form | `fields[i].default` | `default <json> is not null or an asset persistent ID` |
| `BAD_DEFAULT` | an `asset<K>` default whose persistent ID the resolver does not map to an asset of kind K (reported by `ResolveReferenceDefaults`, § "Default values") | `fields[i].default` | `asset default <id> does not name a <K> asset` |
| `BAD_DEFAULT` | a string default above 65,535 UTF-8 bytes | `fields[i].default` | `string default is <n> bytes; the limit is 65535` |
| `LIMIT` | more than 128 fields in a component or struct; more than 1,024 inline bytes in a component; struct nesting deeper than 4; `array` count outside 1 … 64; `maxCount` outside 1 … 65,535; file above 1 MiB; `version` outside 1 … 2³²−1; `id` outside 1 … 2³¹−1; more than 64 structs in a file ([Z2-N-ARCH-596]) | the offending key | `<limit name> is <value>; the limit is <limit>` |
| `TYPE_ID_COLLISION` | the stable name's type ID equals a registered type's ID with a different stable name | `stableName` | `type ID 0x<id> of '<name>' collides with '<other>'` |
| `MISSING_AUTHORITY` | a component field has no `authority` and the component declares none ([Z2-N-ARCH-482]) | `fields[i]` | `field '<name>' has no authority; set "authority" on the field or the component` |
| `BAD_AUTHORITY` | an `authority` that is not a phase name of `Zenith_GetPhaseName` 1 … 10 or `CommandsOnly`; `SaveSnapshot` or `RenderExtraction` ([Z2-N-ARCH-476]) | the key | `'<value>' is not a writable authority: use a phase from Input to LateScripts, or CommandsOnly` |
| `BAD_AUTHORITY` | `authority` on a struct field | `structs[s].fields[i].authority` | `struct fields inherit the authority of the field that contains them` |
| `BAD_AUTHORITY` | `networkAuthority` not `Server`, `OwnerClient` or `Local` | `networkAuthority` | `'<value>' is not a network authority: Server, OwnerClient or Local` |
| `BAD_ARBITRATION` | `arbitration.fields` names an unknown ID or an ID twice, or is neither an array nor `"*"`; `rule` not `Precedence` or `WeightedBlend` (`Custom` is not available to schemas); `order` names an unknown source or a source twice; `WeightedBlend` over a non-blendable field ([Z2-N-ARCH-496]) | the key | `<detail>`, one of `unknown field id <id>`, `field id <id> listed twice`, `rule '<v>' is not Precedence or WeightedBlend`, `unknown writer source '<v>'`, `writer source '<v>' listed twice`, `WeightedBlend needs f32, f64, vec2, vec3, vec4, quat or colour; field '<name>' is <type>` |
| `MIGRATION_CHAIN` | § "Chain validation" | `migrations[...]` | per rule, listed there |
| `BLOB_CORRUPT` | § "Reading and validating a blob" | `schemas.zreg#<index>` | `schema blob corrupt: <detail>` |
| `REGISTRY_FROZEN` | a registration after `Freeze` outside a window | `$` | `the type registry is frozen; schemas register at startup step 10 or in the editor's reload window` |

The cook runs every schema of the game and reports every diagnostic of every file in one run before failing ([Z2-N-ARCH-598], [Z2-N-ARCH-599]).

## Default values

| Field type | Absent `default` | Accepted `default` |
|---|---|---|
| numeric scalars | 0 | a JSON number of the type's range; 64-bit values above 2⁵³ as a decimal string |
| `bool` | `false` | `true`, `false` |
| vectors, `quat`, `colour` | zeros; `quat` `[0, 0, 0, 1]` | an array of 2, 3 or 4 numbers; a `quat` default is normalised at registration (a zero-length one is `BAD_DEFAULT`) |
| `string` | the empty string (`Zenith_StringID` 0) | a JSON string, interned at registration through the bound string table |
| `persistentId` | the null ID | the 36-character form |
| `entity` | null | `null` only |
| `asset<K>` | null | `null`, or the 36-character persistent ID of an asset of kind K |
| `array<T, N>` | N element defaults | an array of exactly N values |
| struct | its fields' defaults | an object keyed by field name, missing fields taking their defaults |
| `container<T>` | empty | an empty array only |

<!-- elaboration -->
Both reference kinds follow the "Default JSON form" column of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" (a persistent-ID string or `null`); they differ in when the ID can be resolved:

- **`entity` defaults are null only.** An entity persistent ID names an entity of one world, and a type default is shared by every world of the process, so no world-independent resolution exists; content assigns entity references in scenes and prefabs, which SCENE resolves at load through `Zenith_ReferenceResolver`. A non-null entity default is `BAD_DEFAULT`.
- **`asset<K>` defaults resolve once per process.** Registration validates the form and records the 16 persistent-ID bytes as the property's declared default (`Zenith_PropertyInfo::m_xDefault` of an `ASSET` property holds the persistent ID, zero for null — the one property whose declared default is not in its in-memory encoding, because a handle exists only once resolved). After the asset registry exists, `Zenith_TypeRegistry::ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, pfnConstruct, pUserData)` calls `pfnConstruct` (ZenithAssets' `Zenith_ValueKindHooks::m_pfnConstructFromPersistentID`, user data the asset registry) once per non-null asset default of every registered type into the type's construction default object, which then holds one reference per resolved default for the registry's lifetime (released through the value-kind destroy hook when the registry, or a retired revision, is freed); an ID the resolver does not map to an asset of kind K is `BAD_DEFAULT` with the message of the catalogue, reported in the diagnostics of every such field in one pass. The runtime calls it at startup step 11, directly after creating the asset registry and before any world exists (a failure stops the game with exit code 3, a cooked-content validation failure); the cook calls it after its scratch registry's `Freeze` with its asset database's resolver, so a default naming a missing asset fails the cook; `ZenithNewGame validate` checks the form only. A world is never created over a registry whose reference defaults are unresolved (`Zenith_WorldDesc` validation).

<!-- assumption: pending MOD -->
The startup step-11 call of `ResolveReferenceDefaults` belongs to the composition root and the resolver to ZenithAssets; both are recorded in the cross-area notes (rows E13, E14).
<!-- /assumption -->

## Authority and arbitration in schemas

- The component-level `authority` is each field's default; a field's own `authority` overrides it (`Zenith2/Docs/Architecture/FramePhases.md` → "Declaration in data schemas"). The registered `Zenith_PropertyInfo::m_eAuthority` is the resolved phase (`Zenith_ParsePhaseName`), `PHASE_COMMANDS_ONLY` for `CommandsOnly`.
- A struct field has no authority of its own: the containing field's authority covers the whole struct value, which is written as a unit.
- `arbitration` becomes a `Zenith_ArbitrationDecl` with `m_axProperties` in ascending ID (`"*"` expands to every field), the rule, and the order (the default precedence when absent); schema types never carry a custom merge ([Z2-N-ARCH-495]).
- `Test_Authority_DataSchemaSameAsCpp` and `Test_DataComponent_AuthorityEnforced` register the same component from C++ and from a schema and assert identical schedule errors ([Z2-N-ARCH-592]).

## Event schemas

`kind: event` registers `TYPE_KIND_EVENT`. Its fields' `serialized` flag decides which fields the replay log and the world-state hash record for each event ([Z2-N-ARCH-610]); its layout is computed like a component's; a system emits it through `Zenith_EventWriter::Emit` with property values, and readers see the payload in that layout (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Events and reductions").

## The schema lock file

<!-- elaboration -->
`Games/<Name>/Schemas/schemas.lock.json` is a tracked JSON file the cook's `schema-lock` step writes and every validation compares against, which is how "the committed previous version" of [Z2-N-ARCH-594] is known without reading version control:

```json
{
	"$schema": "zenith2.schema-lock",
	"lockFormatVersion": 1,
	"schemas": [
		{
			"file": "Stats.schema.json",
			"stableName": "OpenWorldSlice.Stats",
			"version": 3,
			"fields": [ { "id": 1, "type": "f32" }, { "id": 2, "type": "f32" }, { "id": 4, "type": "f32" } ],
			"retiredFieldIds": [3]
		}
	]
}
```

Entries are sorted by `stableName` byte order, fields by ID, with the canonical JSON writer (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"). Comparison rules (each a `REUSED_FIELD_ID`, `BAD_NAME` or `MIGRATION_CHAIN` diagnostic, as the catalogue lists): the stable name of a file never changes; the retired list never loses an ID; a field that disappears is retired; if any field was added, removed or retyped relative to the lock, `version` is greater than the lock's version and the chain covers every step. The cook rewrites the lock after a successful validation only in its explicit `schema-lock` step, which the ticket that changes a schema runs and commits in the same diff; the `Test` step runs the schema tests against the committed lock, so a schema change without its lock and migration fails there.

## The schema registry blob format

`schemas.zreg` is the game's cooked schema registry blob ([Z2-N-ARCH-591] … step 1 of "Registration at game load"), a cooked file with the header of [Z2-N-CONTRACT-527], all integers little-endian:

### Header

| Offset | Type | Field |
|---|---|---|
| 0 | 4 bytes | magic `ZREG` |
| 4 | `uint16` | format version, 1 |
| 6 | `uint16` | flags, 0 |
| 8 | `uint32` | engine cook version |
| 12 | `uint32` | codec ID of the payload (`NONE` = 0: the blob is uncompressed) |
| 16 | `uint64` | content hash of the source schemas (`Zenith_Hash64` over the concatenated file bytes in file-name byte order) |
| 24 | `uint32` | entry count E (≤ 4,096) |
| 28 | `uint32` | payload bytes P |
| 32 | E × `uint32` | entry offsets from the payload start, ascending |
| 32 + 4E | P bytes | the entries |

### Entry records

One entry per schema file, in stable-name byte order; each entry carries its file's structs inline, so an entry never refers to another entry:

| Field | Type |
|---|---|
| entry bytes | `uint32` (≥ 40, ≤ P) |
| kind | `uint8`: 0 component, 1 event |
| network authority | `uint8` |
| component default authority | `uint8` (`Zenith_Phase`; 0 = none) |
| flags | `uint8`: bit 0 default `replicated` |
| version | `uint32` |
| stable name | `uint16` length, UTF-8 bytes (≤ 255) |
| script name | `uint16` length, bytes (≤ 64) |
| source path | `uint16` length, bytes (≤ 1,024) |
| struct count | `uint16` (≤ 64), then per struct: name (`uint16` length, bytes), field count, fields |
| field count | `uint16` (≤ 128), then fields |
| retired ID count | `uint16`, then `uint32` IDs |
| arbitration | `uint8` present flag; when 1: rule `uint8`, property count `uint16`, IDs, order count `uint8`, sources |
| migration count | `uint16`, then per migration: from-version `uint32`, op count `uint16`, ops |

### Field records

`uint32` id; name (`uint16` length, bytes); `uint8` value type (`Zenith_PropertyValueType`); `uint8` element value type; `uint16` struct index (0xFFFF = none); asset kind name (`uint16` length, bytes, 0 when none); `uint32` array count; `uint32` max count; `uint8` authority; `uint32` property flags; `float` min; `float` max; default (`uint32` length, bytes in the value encoding with strings as UTF-8 text and an `ASSET` default as its 16 persistent-ID bytes, zero for null); editor block (`uint8` present; hidden, range, step, tooltip, category).

Ops: `uint8` op kind (1 add, 2 rename, 3 retire, 4 retype, 5 script), `uint32` id, then per kind: rename the old name; retype the target value type and the conversion (`uint8`: 1 numeric, 2 vectorResize, 3 stringToId, 4 default); script the function name. A `script` op references `Schemas/<Component>.migrate.luau`, which the cook compiles into the game's bytecode and the migration runs in a sandboxed VM (§ "Script ops").

### Reading and validating a blob

`RegisterSchemaBlob` treats the blob as untrusted input ([Z2-N-ARCH-951], Architecture.md → "Security architecture", table "Parser limits": the schema limits): it checks the magic, the version, that `32 + 4E + P` equals the blob length, that offsets ascend and every entry lies inside the payload, every length before reading it (overflow-checked), every count against its limit and every enumerator against its range — each failure is `SCHEMA_ERROR_BLOB_CORRUPT` with the detail `bad magic`, `version <v>`, `length mismatch`, `entry <i> out of range`, `<field> length <n> exceeds <limit>` or `<field> value <v> out of range`, and nothing is registered. It then runs every rule of the catalogue that does not need the source file (names, IDs, types, defaults, authority, arbitration, chain, collisions) exactly as the JSON path does, because the blob may come from a patch mount ([Z2-N-ARCH-599]). An invalid blob at runtime stops the game before its first tick with exit code 3 and the diagnostics in the log. `Zenith_Fuzz_SchemaBlob` fuzzes this reader.

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_SchemaBlob.h
/// @contract functions Zenith_SchemaBlobWriter
/// Ownership: Writes into a caller-owned buffer; reads registered types from the registry.
/// Thread affinity: MAIN_THREAD (the cook).
/// Invalidation: None.
/// Units: Bytes.
/// Errors: SCHEMA_ERROR_LIMIT when the blob would exceed a limit; the buffer is left untouched on failure.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<uint32, Zenith_SchemaError> Zenith_WriteSchemaBlob(const Zenith_TypeRegistry& xTypes,
	uint64 ulContentHash, uint32 uEngineCookVersion, Zenith_MutableByteSpan xOut);   // every TYPE_ORIGIN_SCHEMA component and event type
uint32 Zenith_GetSchemaBlobBytes(const Zenith_TypeRegistry& xTypes);
```

`Test_Schema_BlobRoundTrip` asserts that JSON → blob → registry gives `Zenith_TypeInfo` shapes (every field of the shape hash) identical to JSON → registry, and that writing the blob twice gives identical bytes.

## Schema migration

A schema change is an authoring-data migration (Z2-D-048). Authoring documents holding the component migrate through SCENE's and ASSET's document framework, which calls the type's chain; saves migrate forward at load through the same `Zenith_MigrateFieldSet`; cooked data is re-cooked, never migrated ([Z2-N-ARCH-605]).

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_SchemaMigrator.h
/// @contract functions Zenith_SchemaMigrator
/// Ownership: xOut is allocated in the context's arena; xIn is never modified.
/// Thread affinity: The calling thread; the registry is read-only.
/// Invalidation: None.
/// Units: Schema versions.
/// Errors: MIGRATION_ERROR_CHAIN_GAP, MIGRATION_ERROR_NEWER_VERSION, MIGRATION_ERROR_CONVERSION, MIGRATION_ERROR_SCRIPT,
///   MIGRATION_ERROR_BAD_INPUT, MIGRATION_ERROR_OUT_OF_MEMORY; on failure xOut is empty and xIn unchanged ([Z2-N-ARCH-607]).
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_MigrationError> Zenith_MigrateFieldSet(
	const Zenith_TypeInfo& xTarget, uint32 uFromVersion,
	const Zenith_SerializedFieldSet& xIn, Zenith_SerializedFieldSet& xOut,
	Zenith_MigrationContext& xContext);
```

The signature is `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Migration" verbatim. The arena comes through `Zenith_MigrationDiagnostics`, which carries the caller's arena, the string table, an optional script VM factory (null outside editor and save loading) and the warning sink.

### Chain validation

| Rule | Message (`MIGRATION_CHAIN`) |
|---|---|
| exactly one entry per `fromVersion` 1 … `version` − 1, ascending | `migrations: expected fromVersion <n>, found <m>` / `missing migration from version <n>` |
| an op names an ID that exists in the version it applies to (add: absent before, present after; rename, retype, retire: present before) | `migrations[i].ops[j]: field id <id> does not exist in version <v>` |
| `retire` of an ID also listed in `retiredFieldIds` of the final schema | `migrations[i].ops[j]: retired field id <id> is not in retiredFieldIds` |
| `retype` to a type the conversion cannot reach: `numeric` needs numeric source and target, `vectorResize` vectors, `stringToId` a string target, `default` any | `migrations[i].ops[j]: conversion '<c>' cannot convert <from> to <to>` |
| `script` names a function the cook finds in `Schemas/<Component>.migrate.luau` | `migrations[i].ops[j]: no function '<f>' in <Component>.migrate.luau` |
| the final field set replayed through every op from version 1 equals the declared `fields` (IDs and types) | `migrations: applying the chain gives <description>, but fields declare <description>` |

### Op application

`Zenith_MigrateFieldSet(xTarget, uFrom, xIn, xOut)`:

1. `uFrom` greater than `xTarget.m_uVersion` is `MIGRATION_ERROR_NEWER_VERSION` (the document opens read-only, [Z2-N-CONTRACT-537]); equal copies `xIn` to `xOut`.
2. Copy `xIn`'s entries into a working set (arena).
3. For each step from `uFrom` to `m_uVersion − 1`, apply its ops in order: **add** inserts the field at its default of the version being built (no data); **rename** changes nothing (data is keyed by ID); **retire** removes the entry (the one deliberate drop, [Z2-N-ARCH-605]); **retype** converts the value (below); **script** calls the function (§ "Script ops"). Entries whose IDs no step mentions and the target does not know are unknown fields: kept, with one `MIGRATION_WARNING_UNKNOWN_FIELD` per field path ([Z2-N-CONTRACT-544]).
4. Sort by ID, set `m_uVersion`, publish to `xOut`.

C++ types migrate through their `Zenith_PfnMigrationStep`s over the JSON field-set form (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_Migration header"); `Zenith_MigrateFieldSet` renders the set as that object, runs the step, and reads it back.

### Numeric retype conversion

`numeric`: integer → integer saturates to the target range; float → integer rounds half to even then saturates; NaN converts to 0 and records a warning; integer → float rounds to nearest even; `f64` → `f32` rounds to nearest even and an out-of-range finite value saturates to ±FLT_MAX. `vectorResize`: truncate or append zeros (a `quat` target appends `w = 1`). `stringToId`: the text is interned through the context's string table (a table full error is `MIGRATION_ERROR_CONVERSION`). `default`: the new default. A conversion that cannot produce a valid target (a string whose text is not a number for `numeric`, a container above its new `maxCount`) fails the whole migration with `MIGRATION_ERROR_CONVERSION` naming the field and versions, leaving the input unchanged ([Z2-N-ARCH-607]).

### Script ops

A `script` op runs `function(old, new)` from `Schemas/<Component>.migrate.luau` in a sandboxed VM with no world access, obtained from the context's VM factory, which SCRIPT provides for editor migrations and save loading only ([Z2-N-ARCH-605] table row "script"); `old` and `new` are Luau tables keyed by field name (the names of the from- and to-versions); the budget is the recovery safepoint row Z2-B-SCRIPT-018; a raised error or a budget trip is `MIGRATION_ERROR_SCRIPT`. The runtime player loads saves through the same path with the cooked `migrate` bytecode. The VM factory is an upward interface ZenithReflect declares (`Zenith_MigrationScriptHost`, in `Zenith_SchemaMigrator.h`: `virtual Zenith_Result<void, Zenith_MigrationError> RunScriptOp(const char* szModule, const char* szFunction, const Zenith_SerializedFieldSet& xOld, Zenith_SerializedFieldSet& xNew) = 0;`), implemented by ZenithScript.

### Migration fixtures

Every `version` increment ships `Games/<Name>/Schemas/Fixtures/<Component>/v<from>.json` (a JSON field-set object at version `from`) and `v<from>.expected.json` (the object migrated to the current version), and the game's schema test run migrates each and compares byte for byte after canonical writing ([Z2-N-ARCH-606], [Z2-N-CONTRACT-548]). The run is `Test_Schema_GameFixtures_Migrate`, instantiated per game by the `<Name>_Types` target's CTest registration (MOD's target, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Content and type targets"), in the `Test` step.

## Luau type generation from schemas

<!-- elaboration -->
Z2-I-ECS-14 delivers `.d.luau` for data-defined schemas in Stage 2, before SCRIPT's emitter exists (Z2-I-SCRIPT-05 is Stage 3 and notes that the schema part lands in Stage 2 in ECS). ZenithReflect therefore owns the component and event part of the generated types, and SCRIPT's `Zenith_ScriptEmitDefinitions` calls it for those sections and adds bindings, the prelude and the environment:

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_LuauTypes.h
/// @contract functions Zenith_LuauTypes
/// Ownership: Writes UTF-8 text through the caller's writer callback.
/// Thread affinity: The calling thread; the registry is read-only.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Returns false and writes nothing when a derived name collides or is not a valid Luau identifier; the collision
///   is logged naming both sources ([Z2-N-SCRIPT-116]).
/// Version: since Z2 Stage 2.
/// @endcontract
using Zenith_PfnTextSink = void (*)(Zenith_StringView strText, void* pUserData);
bool Zenith_EmitComponentLuauTypes(const Zenith_TypeRegistry& xTypes, uint64 ulBindingSetHash,
	Zenith_PfnTextSink pfnSink, void* pUserData);     // components.d.luau: view classes and the ZenithRow class
bool Zenith_EmitComponentDescriptorTypes(const Zenith_TypeRegistry& xTypes,
	Zenith_PfnTextSink pfnSink, void* pUserData);     // the component and event entries of Types.luau
```

Rules (the declaration shapes are `Zenith2/Docs/Scripting/Bindings.md` → "Declaration shapes"; the type mapping is `Zenith2/Docs/Scripting/Bindings.md` → "Data-defined component types"):

1. Input: every `TYPE_KIND_COMPONENT`, `TYPE_KIND_STRUCT` (as nested view classes) and `TYPE_KIND_EVENT` of the frozen registry — engine, selected modules and schemas together, with no branch on origin ([Z2-N-ARCH-603], [Z2-N-ARCH-643]).
2. Order: types in byte order of stable name, fields in ascending field ID ([Z2-N-ARCH-611]); the file header carries the binding-set hash and no timestamp, so two generations from one registry are byte-identical.
3. Names: schema `component` and field names verbatim ([Z2-N-ARCH-604], [Z2-N-SCRIPT-661]); C++ types through the Hungarian mapping; one `<Component>View` class per component with one property per field, a `<Struct>View` per struct, and `ZenithRow` with one non-optional property per component.
4. Output files, into `Zenith2/out/<checkout-hash>/Generated/Luau/<Game>/`: `components.d.luau` (the view classes and `ZenithRow`) and the component and event part of `Types.luau`; never tracked, never hand-edited ([Z2-N-ARCH-612]).

Until SCRIPT's emitter lands (Stage 3), ZenithCook's `luau-types` step writes `components.d.luau` from `Zenith_EmitComponentLuauTypes` alone; `Test_LuauTypes_ComponentsDeterministic` and `Test_LuauTypes_SchemaAndCppTwinIdentical` prove determinism and the C++/schema equivalence.

<!-- assumption: pending SCRIPT -->
[Z2-N-SCRIPT-621] sorts generated declarations by Luau name; [Z2-N-ARCH-611] orders types by stable name and fields by field ID. This design follows [Z2-N-ARCH-611] for the component sections it emits (Architecture is the owning specification for data-defined components); the conflict is recorded for SCRIPT in the cross-area notes.
<!-- /assumption -->

## Cook integration

ZenithCook (ASSET's tool) runs, per game, in the `<Name>_Content` target's cook (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Content and type targets"):

1. Build a scratch registry: engine and every module's `RegisterTypes`, then only the modules the game's `composition.json` selects are kept for Luau generation (all modules are linked into the cook; the selection gates which types appear in the game's registry, [Z2-N-SCRIPT-114]).
2. Validate every `Games/<Name>/Schemas/*.schema.json` in file-name byte order with the game's lock, reporting every diagnostic of every file.
3. On success, `Freeze`, then `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, <the asset database's resolver>, <the asset database>)` (§ "Default values"; a failure fails the cook with every diagnostic, one per unresolvable field), then write `content:/schemas.zreg` with `Zenith_WriteSchemaBlob` and run the `luau-types` step.
4. The explicit `schema-lock` step (a cook command-line verb, `ZenithCook schema-lock <Game>`) rewrites `Schemas/schemas.lock.json` from the registered schema types.

`ZenithNewGame validate` runs step 2 on a registry holding no engine or module type (`Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Validation in C++"), which catches every error except collisions with engine or module types.

## Editor re-registration

The editor registers schemas from JSON directly and re-registers a changed schema only between play sessions, through the hot-reload window, after migrating every open document of the type ([Z2-N-ARCH-648]):

1. Validate the changed file into a scratch registry (errors shown in the editor's schema panel with the exact messages, nothing applied).
2. Preview: SCENE's migration framework dry-runs every open document and every document on disk holding the component (preview, backup, journaled write, [Z2-N-CONTRACT-531] … [Z2-N-CONTRACT-533]).
3. With no play world alive, open the window (the runtime's prologue step 6), `RegisterSchemaJson` the changed file, `EndHotReload`; on failure, abandon and keep the previous revision.
4. The edit world re-resolves its column tables by type ID for the new revision at the next frame: its backend migrates stored rows by capturing each row's field set, migrating it and re-applying it in a new archetype layout (`Zenith2/Docs/Design/ECS/Backends.md` → "Schema revision in the edit world"); the schedule recompiles (the recompile trigger "component schemas re-registered").
5. Rewrite `schemas.lock.json` when the user commits the schema change (the editor runs the `schema-lock` verb).

Rewrites of a schema file by any tool preserve `x-` keys byte for byte in position ([Z2-N-ARCH-600]), by editing the `Zenith_JsonDocument` in place and writing it with raw number text; `Test_Schema_UnknownExtensionKeysPreserved` asserts it.
