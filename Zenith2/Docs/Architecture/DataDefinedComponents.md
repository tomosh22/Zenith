# Architecture — Data-defined components

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "Data-defined components". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 591–639.

## Settled rules and ownership

Games contain no C++ (Z2-D-002), so component schemas are defined in data (Z2-D-020): one format, `Games/<Name>/Schemas/*.schema.json`, registered at game load as reflected types with stable IDs, stored natively in every ECS backend (Z2-D-039), serialized, shown in the inspector, migrated like any authoring document (Z2-D-048) and exposed to Luau through generated `.d.luau`. The ECS area owns the format and registration; the schema's Luau view is specified in Zenith2/Docs/Scripting.md → "Modules, bindings and generated types".

- [Z2-N-ARCH-591] `*.schema.json` is the one front end for game-defined component types; no second schema language, no Luau-side type declaration and no editor-only format defines a component.
- [Z2-N-ARCH-614] Component types the engine and its modules ship (for example the lighting, camera and renderable-mesh components) are C++-registered structs registered from their library's registration function with explicit property IDs, never engine-content schemas; `*.schema.json` defines game types alone, under the game's `<Game>.<Name>` stable names.
- [Z2-N-ARCH-592] A data-defined type is registered with `ZenithReflect` alongside the engine's C++-registered types, and no consumer — serialization, the inspector, migration, `.d.luau` generation, chunk-system column access, phase and authority checks, network replication — distinguishes the two (Zenith2/Docs/Contracts.md → "Reflection").

## The schema file

### Example

`Games/OpenWorldSlice/Schemas/Stats.schema.json`:

```json
{
	"$schema": "zenith2.component-schema",
	"schemaFormatVersion": 1,
	"kind": "component",
	"component": "Stats",
	"stableName": "OpenWorldSlice.Stats",
	"version": 3,
	"authority": "LateScripts",
	"networkAuthority": "Server",
	"replicated": false,
	"fields": [
		{ "id": 1, "name": "Stamina", "type": "f32", "default": 100.0, "min": 0.0 },
		{ "id": 2, "name": "MaxStamina", "type": "f32", "default": 100.0, "authority": "CommandsOnly" },
		{ "id": 4, "name": "RegenPerSecond", "type": "f32", "default": 5.0, "authority": "EarlyScripts" },
		{ "id": 5, "name": "LastHitBy", "type": "entity", "serialized": false },
		{ "id": 6, "name": "Portrait", "type": "asset<Texture>" },
		{ "id": 7, "name": "DamageLog", "type": "container<DamageRecord>", "maxCount": 64 },
		{ "id": 8, "name": "Title", "type": "string", "restoreAfterCinematic": true }
	],
	"structs": [
		{
			"name": "DamageRecord",
			"fields": [
				{ "id": 1, "name": "Amount", "type": "f32" },
				{ "id": 2, "name": "Source", "type": "persistentId" },
				{ "id": 3, "name": "Tick", "type": "u32" }
			]
		}
	],
	"retiredFieldIds": [3],
	"migrations": [
		{ "fromVersion": 1, "ops": [ { "op": "add", "id": 6 } ] },
		{ "fromVersion": 2, "ops": [ { "op": "retire", "id": 3 }, { "op": "retype", "id": 4, "to": "f32", "conversion": "numeric" } ] }
	]
}
```

### Top-level keys

| Key | Type | Presence | Rule |
|---|---|---|---|
| `$schema` | string | always | `zenith2.component-schema` |
| `schemaFormatVersion` | integer | always | `1`; the version of this file format, distinct from the type's `version` |
| `kind` | string | optional | `component` (default) or `event` |
| `component` | string | always | the type's script-visible name: `^[A-Z][A-Za-z0-9]{0,63}$`, unique among the game's types and the engine's |
| `stableName` | string | always | `<Game>.<Name>` segments, `^[A-Z][A-Za-z0-9]*(\.[A-Z][A-Za-z0-9]*)+$`; never changes after the first commit |
| `version` | integer | always | 1 … 2³²−1; incremented by any change to the field set or a field's type |
| `authority` | string | optional | default authority for fields: a phase name or `CommandsOnly` (Architecture/FramePhases.md → "Per-field authority") |
| `networkAuthority` | string | optional | `Server` (default), `OwnerClient` or `Local` (Architecture.md → "Multiplayer foundations") |
| `replicated` | boolean | optional | default for fields' `replicated`; default `false` |
| `fields` | array | always | 1 to 128 field objects |
| `structs` | array | optional | nested struct declarations, each `{ "name", "fields" }`, names unique in the file |
| `arbitration` | object | optional | Architecture/FramePhases.md → "Arbitration rules" |
| `retiredFieldIds` | array of integer | optional | IDs of removed fields; never reused |
| `migrations` | array | iff `version` > 1 | one entry per `fromVersion` 1 … `version`−1, in ascending order |
| `x-<anything>` | any | optional | tool extensions; preserved on rewrite, ignored by the engine |

### Field keys

| Key | Type | Presence | Rule |
|---|---|---|---|
| `id` | integer | always | 1 … 2³¹−1; unique in its component or struct; never in `retiredFieldIds`; becomes the `Zenith_PropertyID` |
| `name` | string | always | `^[A-Z][A-Za-z0-9]{0,63}$`; unique in its component or struct |
| `type` | string | always | the type grammar below |
| `default` | JSON value | optional | a value of the field's type (below); absent means the type's zero value |
| `authority` | string | optional | overrides the component default; absent on struct fields (a struct field inherits its containing field's authority) |
| `min`, `max` | number | optional | numeric fields only; writes outside the range are clamped for commands and raise a Luau error for script writes |
| `maxCount` | integer | `container<T>` fields | 1 … 65,535 |
| `serialized` | boolean | optional | default `true`; `false` excludes the field from saves, cooked scenes and replays (transient) |
| `replicated` | boolean | optional | default from the component |
| `restoreAfterCinematic` | boolean | optional | default `false` (Architecture/LifecycleCommands.md → "Cinematic start, interrupt and skip") |
| `worldPosition` | boolean | optional | default `false`; `VEC3` and `VEC2` fields only: `true` sets `PROPERTY_FLAG_WORLD_POSITION` (`1u << 7`), a `VEC2` being shifted by (S.x, S.y), and on any other field type `Freeze` fails with `REFLECT_ERROR_BAD_FLAGS`; SCENE's `Scene.ShiftWorldOrigin` playback and STREAM's anchor-relative persistence honour it (`Zenith2/Docs/Design/STREAM.md`) |
| `editor` | object | optional | `{ "hidden": boolean, "range": [min, max], "step": number, "tooltip": string, "category": string }` |

### Field type grammar

```text
type      := scalar | vector | "string" | "persistentId" | "entity"
           | "asset<" AssetKind ">" | "array<" element "," count ">"
           | StructName | "container<" element ">"
scalar    := "bool" | "i8" | "i16" | "i32" | "i64" | "u8" | "u16" | "u32" | "u64" | "f32" | "f64"
vector    := "vec2" | "vec3" | "vec4" | "quat" | "colour"
element   := scalar | vector | "string" | "persistentId" | "entity" | "asset<" AssetKind ">" | StructName
count     := 1 … 64
AssetKind := a registered asset type name (Texture, Mesh, Material, AnimationClip, Prefab, Scene, Sound, …)
StructName:= the name of an entry in "structs"
```

| Type | Inline size (bytes) | Alignment | Storage | Default JSON form |
|---|---|---|---|---|
| `bool` | 1 | 1 | inline | `false` |
| `i8`/`u8`, `i16`/`u16`, `i32`/`u32`, `i64`/`u64` | 1, 2, 4, 8 | same | inline | integer (64-bit values above 2⁵³ as a decimal string) |
| `f32`, `f64` | 4, 8 | same | inline | number |
| `vec2`, `vec3`, `vec4` | 8, 12, 16 | 4 | inline, `f32` components | array of 2, 3, 4 numbers |
| `quat` | 16 | 4 | inline, x y z w | `[0, 0, 0, 1]` |
| `colour` | 16 | 4 | inline, linear RGBA `f32` | `[r, g, b, a]` |
| `string` | 4 | 4 | an interned `Zenith_StringID`: a plain value with no reference count; the text lives in the runtime's process-lifetime intern table and is serialized as text | string |
| `persistentId` | 16 | 8 | inline `Zenith_PersistentID` | 36-character form (Zenith2/Docs/Contracts.md → "Handle kinds") |
| `entity` | 8 | 8 | inline `Zenith_EntityHandle`; serialized as a persistent ID reference | `null` only (no persistent-ID form; [Z2-N-ARCH-984]) |
| `asset<K>` | 16 | 8 | inline `Zenith_AssetHandle<K>` with reference-count hooks | asset persistent ID string or `null` |
| `array<T, N>` | N × size(T) | align(T) | inline | array of N values |
| struct | sum of its fields, laid out as below | max of its fields | inline | object keyed by field name |
| `container<T>` | 8 | 4 | out of line: `Zenith_ContainerRef { uint32 m_uBlock; uint32 m_uCount; }` into the world's container heap | array of values |

- [Z2-N-ARCH-593] The type ID of a data-defined type is `Zenith_Hash64` of its `stableName` (the same derivation as a C++-registered type's stable name, Zenith2/Docs/Contracts.md → "Reflection"), so renaming `component` changes no ID and changing `stableName` is a new type.
- [Z2-N-ARCH-594] A field ID is never reused: removing a field adds its ID to `retiredFieldIds` in the same change, and a schema whose field ID appears in `retiredFieldIds`, or whose retired list shrinks relative to the committed previous version, fails validation; the committed previous version is the game's schema lock file `Games/<Name>/Schemas/schemas.lock.json`, written by the cook's `schema-lock` verb and committed with the schemas.
- [Z2-N-ARCH-595] Containers are stored out of line and never in a component's inline bytes; nested containers (`container<container<…>>`, or a struct containing a container used as a container element) are rejected.
- [Z2-N-ARCH-596] The schema limits are: at most 128 fields per component or struct, at most 1,024 inline bytes per component, struct nesting depth at most 4, `array` count at most 64, `maxCount` at most 65,535, and file size at most 1 MiB; exceeding any limit fails validation with the limit named.
- [Z2-N-ARCH-610] A schema with `kind: event` carries no `authority`, `arbitration`, `networkAuthority`, `replicated` or `restoreAfterCinematic` key at any level (each is `SCHEMA_ERROR_UNKNOWN_KEY` for events) and no `container<T>` field; its fields' `serialized` flag decides what the replay log records of each event.
- [Z2-N-ARCH-984] An `entity` field's `default` is `null` only: an entity persistent ID names an entity of one world while a type default is shared by every world of the process, so a non-null `entity` default is `SCHEMA_ERROR_BAD_DEFAULT`; `asset<K>` defaults keep the persistent-ID form of the "Default JSON form" column.

### Inline layout

The registry computes a `Zenith_TypeLayout` for every data-defined type and struct: fields are ordered by descending alignment, then ascending field ID; each field is placed at the next offset that satisfies its alignment; the size is rounded up to the largest alignment. The layout is recomputed at every registration and is not written into any file.

- [Z2-N-ARCH-597] Serialization, saves, cooked data and replication never depend on the computed inline layout: they read and write fields by field ID through reflection (Z2-D-038), so a layout change from adding a field requires no data migration of its own.

## Registration at game load

1. The cook validates every `Games/<Name>/Schemas/*.schema.json` and writes the game's schema registry blob `schemas.zreg` into the cooked content (versioned binary: header `ZREG`, format version, entry count, then per type the stable name, component name, version, kind, authorities, fields and structs, all little-endian with lengths bounded by the header).
2. At startup step 10, after the engine's and the selected modules' C++ registrations of step 6 and after `InstallValueKindHooks(PROPERTY_TYPE_ASSET, …)` (Architecture.md → "Composition root and Player"), `ZenithRuntime` loads `schemas.zreg` and calls `Zenith_TypeRegistry::RegisterSchemaBlob`; the cooked behaviour-state schemas register after it, still before `Freeze`.
3. The registry validates every entry again (the blob is untrusted input, Architecture.md → "Security architecture"), computes layouts, derives type IDs, checks collisions against every registered type, and registers the types with origin `TYPE_ORIGIN_SCHEMA`.
4. `Zenith_TypeRegistry::Freeze` runs; after it, the registry is immutable (Zenith2/Docs/Contracts.md → "Reflection").
5. At startup step 11, once the asset registry exists, `Zenith_TypeRegistry::ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, …)` resolves every `asset<K>` default from its persistent-ID form through ZenithAssets' resolver; an unresolvable default stops startup with exit code 3, and no world is created before it ran.
6. Each world's ECS backend is created with the frozen registry and creates storage for every component type on first use.

The editor registers schemas from JSON directly (`Zenith_TypeRegistry::RegisterSchemaJson`) and re-registers them between play sessions alone, through the migration path (§ "Migration").

```cpp
// Engine/ZenithReflect/Zenith_TypeRegistry.h (excerpt; the full API is in Architecture.md → "Reflection and runtime type registration")
Zenith_Result<void, Zenith_SchemaError> RegisterSchemaJson(Zenith_ByteSpan xJsonBytes,
	Zenith_StringView strSourcePath);
Zenith_Result<void, Zenith_SchemaError> RegisterSchemaBlob(Zenith_ByteSpan xBlobBytes);
```

### Validation errors

```cpp
enum Zenith_SchemaErrorCode : uint16
{
	SCHEMA_ERROR_PARSE = 1,              // not JSON, or exceeds a parser limit
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
	SCHEMA_ERROR_REGISTRY_FROZEN,        // a schema registration after Freeze outside a hot-reload window
};

// One diagnostic; held by the type registry (Zenith_TypeRegistry::GetSchemaDiagnostics). Not an error type.
struct Zenith_SchemaDiagnostic
{
	const Zenith_SchemaErrorCode m_eCode = {};
	const Zenith_StringView m_strSourcePath = {};      // schema file, or "schemas.zreg#<index>"
	const Zenith_StringView m_strKeyPath = {};         // JSON path, for example "fields[3].type"
	const Zenith_StringView m_strMessage = {};         // for example "unknown asset kind 'Textur'"
};

// The error record of [Z2-N-CONTRACT-480].
struct Zenith_SchemaError
{
	Zenith_SchemaErrorCode m_eCode = SCHEMA_ERROR_PARSE;   // the code of the first diagnostic
	uint32 m_uDiagnosticCount = 0;                        // at least 1
};
```

- [Z2-N-ARCH-613] A failed `RegisterSchemaJson` or `RegisterSchemaBlob` logs each diagnostic at the point of failure and keeps it in the registry, whose `GetSchemaDiagnostics` returns the diagnostics of the latest schema registration until the next one starts; the `Zenith_SchemaError` result carries the first code and the count, and no text.
- [Z2-N-ARCH-598] Every schema error names the source file, the JSON key path and the rule broken, in the form `<file>: <keyPath>: <message>`, and the cook reports every error of every schema in one run.
- [Z2-N-ARCH-599] An invalid schema fails the cook, and an invalid `schemas.zreg` at run time stops the game before its first tick with the error list and exit code 3; a data-defined type never silently degrades or registers partially.
- [Z2-N-ARCH-600] Unknown top-level or field keys that are not prefixed `x-` fail validation with `SCHEMA_ERROR_UNKNOWN_KEY` in the cook, and every tool that rewrites a schema preserves `x-` keys byte for byte in position.

## Storage in every backend

The `Zenith_World` API is type-erased by reflection ID (Architecture.md → "ECS model"), so data-defined components need no API change. Columns are per field: a chunk view exposes each field as a `Zenith_RawColumn` with a stride, which is the field's own size in a structure-of-arrays backend and the component's size in an array-of-structures backend.

| Backend | Storage of a data-defined component | Column stride | Lifecycle hooks |
|---|---|---|---|
| Custom | designed for runtime layouts from the start: each field is its own column in the archetype chunk, sized from `Zenith_TypeLayout` | field size | generated from the layout: construct defaults, release asset references and container blocks on destroy, adjust references on copy |
| flecs | a runtime component: `ecs_component_init` with the layout's size and alignment, native | component size | `ecs_type_hooks_t` ctor, dtor, copy and move generated from the layout |
| EnTT | a dynamic-storage adapter: a named `entt::basic_storage` per type, keyed by the type ID, whose element is a fixed-size byte block of the smallest size class (8, 16, 32, 64, 128, 256, 512, 1,024 bytes) that fits the layout | component size | the adapter runs the generated hooks on construct, destroy and copy |

- [Z2-N-ARCH-601] Every ECS backend stores data-defined components natively in its own storage and passes the full `ZenithECS_Conformance` suite with a fixture that registers the same component once from C++ and once from a schema, asserting identical observable behaviour.
- [Z2-N-ARCH-602] Fields whose type carries lifetime (asset handles and containers) are constructed, copied and destroyed through the generated hooks in every backend, so storage never copies such a field's bytes without its hook (Zenith2/Docs/Contracts.md → "Handle kinds").
- [Z2-N-ARCH-609] Interned string IDs are plain values: the intern table is owned by `ZenithRuntime` for the process lifetime, never shrinks while any world exists, and is serialized by the strings' text, so a string field needs no hook and a row containing only plain values and string IDs may be copied bytewise.

<!-- informative -->
The EnTT adapter is a flagged cost: EnTT's storage is typed at compile time, so a growing share of what the ECS research measures for EnTT is the adapter rather than EnTT itself. Flagged proposal P1 suggests scoping the EnTT research claim to C++-registered components and ruling after the Stage 5 report whether EnTT remains a shipping backend (D-SEL-EnTT-SHIPPING, a flagged proposal only; nothing in this document assumes its outcome). The adapter's cost is reported separately in the research methodology (Zenith2/Docs/Rendering.md → "Shading path research methodology" covers the rendering side; the ECS side is the RESEARCH design document).
<!-- /informative -->

## Generated Luau types

- [Z2-N-ARCH-603] `.d.luau` definitions are generated from the frozen registry — engine types, the selected modules' types and the game's schemas together — by ZenithCook's `luau-types` step into `Zenith2/out/<checkout-hash>/Generated/Luau/<Game>/` (Zenith2/Docs/Scripting.md → "Modules, bindings and generated types").
- [Z2-N-ARCH-611] The generated definitions are deterministic: types in byte order of stable name, fields in ascending field ID.
- [Z2-N-ARCH-612] Generated `.d.luau` files are never hand-edited and never tracked.
- [Z2-N-ARCH-604] A schema's `component` and field `name` strings are the Luau names verbatim; field `id`s never appear in Luau source, so renaming a field is a script-visible change that the schema's version bump and migration record.

## Migration

A schema change is an authoring-data migration (Z2-D-048, Zenith2/Docs/Contracts.md → "Compatibility policy per kind"). The chain in `migrations` has one entry per version step; each entry's ops are applied in order to the field set of the previous version.

| Op | Keys | Effect on stored data |
|---|---|---|
| `add` | `id` | the field appears with its default |
| `rename` | `id`, `from` | none: data is keyed by ID; the script-visible name changes |
| `retire` | `id` | the field's data is dropped by the migration (the one deliberate drop; unknown fields found in a document are otherwise preserved with a warning) |
| `retype` | `id`, `to`, `conversion` | `numeric` (value-preserving numeric conversion, clamped to the target range, rounded to nearest-even), `vectorResize` (truncate or zero-extend components), `stringToId` (intern), `default` (reset to the new default) |
| `script` | `function` | calls `function(old, new)` from `Schemas/<Component>.migrate.luau` in a sandboxed VM with no world access; editor and save migration only |

```cpp
// Engine/ZenithReflect/Zenith_SchemaMigrator.h
Zenith_Result<void, Zenith_MigrationError> Zenith_MigrateFieldSet(
	const Zenith_TypeInfo& xTarget, uint32 uFromVersion,
	const Zenith_SerializedFieldSet& xIn, Zenith_SerializedFieldSet& xOut,
	Zenith_MigrationContext& xContext);
```

- [Z2-N-ARCH-605] Authoring documents that contain a data-defined component are migrated through the authoring-document migration framework (preview, backup, journaled write, unknown-field preservation, per-version fixtures), and saves are migrated forward at load through the same `Zenith_MigrateFieldSet`; cooked data is re-cooked, never migrated.
- [Z2-N-ARCH-606] Every `version` increment ships its migration entry and a migration fixture under `Games/<Name>/Schemas/Fixtures/<Component>/v<from>.json` whose migrated output is checked by the game's schema test run in the `Test` step.
- [Z2-N-ARCH-607] A migration that cannot produce a valid target value (a `retype` whose conversion fails, a `script` that raises) fails the migration of that document or save with `Zenith_MigrationError` naming the component, field, versions and entity, and leaves the input unchanged.

## Required tests

The table "Data-defined component tests" lists the tests of this section; they live in `Engine/ZenithReflect/Tests/` except where the table names `ZenithECS_Conformance`.

- [Z2-N-ARCH-608] Every test in the table "Data-defined component tests" exists under the name given and asserts the case given.

Data-defined component tests:

| Test | Location | Asserts |
|---|---|---|
| `Test_Schema_ValidExample_Registers` | Reflect | the example above registers with the expected layout, IDs and authorities |
| `Test_Schema_EachErrorCode_Reported` | Reflect | one fixture per `SCHEMA_ERROR_*` code yields that code and the exact message format |
| `Test_Schema_ReusedFieldID_Rejected` | Reflect | a field ID present in `retiredFieldIds` fails |
| `Test_Schema_Limits_Enforced` | Reflect | each limit at its value passes and at value + 1 fails |
| `Test_Schema_TypeIDFromStableName` | Reflect | renaming `component` keeps the type ID; changing `stableName` changes it |
| `Test_Schema_CollisionWithCppType_Rejected` | Reflect | a schema whose stable name hashes to a C++ type's ID fails with `SCHEMA_ERROR_TYPE_ID_COLLISION` (hash collision forced through a test hash) |
| `Test_Schema_BlobRoundTrip` | Reflect | JSON → `schemas.zreg` → registry equals JSON → registry |
| `Test_Schema_CorruptBlob_Rejected` | Reflect | truncated, oversized-count and bad-magic blobs fail with `SCHEMA_ERROR_BLOB_CORRUPT`, never crash (also a fuzz target, `Zenith_Fuzz_SchemaBlob`) |
| `Test_Schema_UnknownExtensionKeysPreserved` | Reflect | a rewrite keeps `x-` keys in position byte for byte |
| `Test_Schema_Migration_ChainAppliesInOrder` | Reflect | a v1 field set migrates to v3 through each op of the example |
| `Test_Schema_Migration_FailureLeavesInput` | Reflect | a failing `retype` returns an error and leaves the input unchanged |
| `Test_Schema_LayoutIndependentSerialization` | Reflect | adding a field changes the layout and reads old serialized data unchanged |
| `Test_DataComponent_IdenticalToCppTwin` | `ZenithECS_Conformance` | per backend, the schema twin and the C++ twin of one component give identical query, command, serialization and change-tracking results |
| `Test_DataComponent_AssetFieldRefCounted` | `ZenithECS_Conformance` | per backend, copying and destroying entities with an `asset<>` field leaves the asset's reference count balanced |
| `Test_DataComponent_ContainerFieldReleased` | `ZenithECS_Conformance` | per backend, destroying entities with a `container<>` field returns every block to the container heap |
| `Test_DataComponent_AuthorityEnforced` | `Engine/ZenithECS/Tests/` | a schema field's authority produces the same schedule errors as a C++ field's |
