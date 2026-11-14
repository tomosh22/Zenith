# ASSET design: meta documents on the authoring-document framework

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. The authoring-document migration framework of `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` (per-type schema version and chain, upgrade preview, automatic backup, journaled atomic write, override and unknown-field preservation, fixtures) and canonical serialization are SCENE's engine library `ZenithDocument` and its tool-side storage `ZenithDocumentHost` (`Zenith2/Docs/Design/SCENE.md → "ZenithDocument target"`, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`, → "Zenith_DocumentMigrator header"). ASSET registers one document type into that framework — the importer-settings sidecar `.meta` (`Zenith.AssetMeta`) — and keeps only what is specific to it: the type registration and its canonical schema, the migration of the importer-settings objects embedded in a `.meta`, the removed-setting rule, the cook's journal refusal and recovery calls, and the `.meta` fixtures. Nothing here re-specifies a framework rule; every framework name is SCENE's.

<!-- assumption: pending SCENE -->
This file conforms to SCENE revision 1's split (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Ownership split with ASSET"`, cross-area row S15). The library rows of `ZenithDocument` and `ZenithDocumentHost` and the `ZenithCook` edges to them are core-doc item X109 (a) (pending).
<!-- /assumption -->

## Scope of this file

| Topic | Heading |
|---|---|
| who owns what after the ruling | "Ownership after the framework ruling" |
| the `Zenith.AssetMeta` registration and its canonical schema | "The meta document type" |
| importer-settings versions inside a `.meta` | "Importer settings inside meta documents" |
| how ZenithCook and ZenithEditor open, migrate and write `.meta` files | "Reading and writing meta documents through the framework" |
| settings a new importer version removes; unknown members | "Removed settings and unknown fields" |
| journals left by an interrupted batch | "Recovery before a cook" |
| fixtures | "Meta migration fixtures" |
| the other types ZenithCook registers | "Registered document types" |
| error mapping | "Migration errors at the ASSET boundary" |

## Ownership after the framework ruling

| Piece | Owner | Where |
|---|---|---|
| `Zenith_MigrationRegistry`, `Zenith_DocumentTypeDesc`, `Zenith_RegisterMigrationStep`, `Zenith_DocumentMigrator` (open, plan, apply, save, recovery), `Zenith_CanonicalSchema` and `Zenith_WriteCanonicalJson`, the journal format, backups, `Zenith_DocumentError`, `Zenith_DocumentStorage` and `Zenith_HostDocumentStorage` | SCENE | `Zenith2/Engine/ZenithDocument/`, `Zenith2/Tools/DocumentHost/` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md`) |
| `Zenith_MigrationContext`, `Zenith_PfnMigrationStep`, `Zenith_MigrationError` | ECS | `Zenith2/Engine/ZenithReflect/Zenith_Migration.h` (`Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_Migration header"`) |
| the `Zenith.AssetMeta` document type, its canonical schema, its steps, the importer-settings visitor, the removed-setting rule, the `.meta` fixtures | ASSET | `Zenith2/Tools/Authoring/Zenith_AuthoringAsset.h` and `Zenith2/Tools/Authoring/Internal/Asset/` after ASSET's ZenithAuthoring move (row ED42); `Zenith2/Tools/ZenithCook/Internal/Database/Zenith_AssetMetaDocument.cpp`, `.h` before it and, after it, the importer-registry adapter |
| each importer's settings chain (`Zenith.ImporterSettings.<id>`) | the importer's owner (CONTENT; ASSET for `raw` and `zenith-doc`) | registered through `Zenith_ImporterRegistry` ("Importer settings inside meta documents") |
| the editor's migration UI | EDIT | through the cook server's `migratePreview` and `migrateApply` (`Zenith2/Docs/Design/ASSET/Formats.md → "Cook server messages"`) |

ASSET declares no migration registry, no migration run, no journal and no error enumeration of its own for migration.

## The meta document type

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringAsset.h (ASSET's ZenithAuthoring root header, ruling Q84 (a), cross-area row ED42;
// bodies in Zenith2/Tools/Authoring/Internal/Asset/; ZenithCook's Internal/Database/Zenith_AssetMetaDocument.h includes it)
#pragma once
#include "ZenithDocument/Zenith_MigrationRegistry.h"
#include "ZenithDocument/Zenith_CanonicalJson.h"

/// @contract interface Zenith_AssetMetaSettingsSource
/// Ownership: Implemented by ZenithCook over its frozen Zenith_ImporterRegistry (Internal/Import/Zenith_ImporterRegistry.cpp);
///   borrowed by the migration registry as user data for its lifetime.
/// Thread affinity: ANY_THREAD (the importer registry is frozen before the migration registry is created).
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown importer is FindSettingsVersion returning 0.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2. Tool-side; lets the registration live in ZenithAuthoring without naming a ZenithCook type.
/// @endcontract
class Zenith_AssetMetaSettingsSource
{
public:
	virtual ~Zenith_AssetMetaSettingsSource() = default;
	virtual uint32 FindSettingsVersion(Zenith_StringView strImporterID) const = 0;   // 0: unknown importer
	virtual Zenith_PfnMigrationStep FindSettingsStep(Zenith_StringView strImporterID, uint32 uFromVersion) const = 0;
};

/// @contract functions Zenith_AssetMetaDocumentFunctions
/// Ownership: Registration writes into the caller's registry; the canonical schema is static data owned by ZenithCook.
/// Thread affinity: MAIN_THREAD (tool start-up, before Zenith_MigrationRegistry::Freeze); the schema getter ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_Result<…, Zenith_DocumentError> as RegisterDocumentType returns it (DOCUMENT_ERROR_DUPLICATE_REGISTRATION,
///   DOCUMENT_ERROR_INVALID_DESC, DOCUMENT_ERROR_REGISTRY_FROZEN); a step registration failure is DOCUMENT_ERROR_INVALID_DESC
///   with the Zenith_MigrationError logged.
/// Version: since Z2 Stage 2 (in ZenithCook); in ZenithAuthoring since ASSET's move. Tool-side; linked by ZenithCook and ZenithEditor
///   through ZenithAuthoring.
/// @endcontract
// pxSettings null (the editor): the importer-settings visitor leaves every settings object unchanged, because the editor never
// migrates a .meta in process (it asks the cook server); ZenithCook passes its importer-registry adapter.
Zenith_Result<Zenith_TypeID, Zenith_DocumentError> Zenith_RegisterAssetMetaMigrations(
	Zenith_MigrationRegistry& xRegistry, const Zenith_AssetMetaSettingsSource* pxSettings);
/// @contract value Zenith_AssetMetaMemberMigration
/// Ownership: constexpr data of ZenithAuthoring's ASSET files.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Tool-side; one row per (member, version step), "Member migrations".
/// @endcontract
struct Zenith_AssetMetaMemberMigration { const char* const m_szMember = nullptr; const uint32 m_uFromVersion = 0; const Zenith_PfnMigrationStep m_pfnStep = nullptr; };
Zenith_Span<const Zenith_AssetMetaMemberMigration> Zenith_GetAssetMetaMemberMigrations();
inline constexpr uint32 uASSET_META_PROVENANCE_SCHEMA_VERSION = 1;   // CONTENT's provenance object; no step at version 1
const Zenith_CanonicalSchema& Zenith_GetAssetMetaCanonicalSchema();
inline constexpr uint32 uASSET_META_SCHEMA_VERSION = 1;
```

`Zenith_RegisterAssetMetaMigrations` registers this `Zenith_DocumentTypeDesc` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`), then one `Zenith_RegisterMigrationStep` per envelope version increment (none at version 1):

| Desc member | Value |
|---|---|
| `m_szStableName` | `"Zenith.AssetMeta"` |
| `m_szSchemaTag` | `"zenith2.asset-meta"` (the `$schema` member of `Zenith2/Docs/Design/ASSET/Formats.md → "Keys"`) |
| `m_szVersionKey` | `"schemaVersion"` |
| `m_szIDKey` | `"guid"` (the document ID is the asset source's GUID) |
| `m_aszSuffixes` | `{ ".meta" }` |
| `m_uCurrentVersion` | `uASSET_META_SCHEMA_VERSION` |
| `m_pxCanonical` | `&Zenith_GetAssetMetaCanonicalSchema()` |
| `m_pfnVisitFieldSets` | the importer-settings visitor of "Importer settings inside meta documents" |
| `m_pUserData` | `pxSettings` (ZenithCook's adapter over the frozen importer registry, which outlives the migration registry; null in the editor) |

The canonical schema (`Zenith_CanonicalRule` table, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_CanonicalJson header"`) is the canonical form of `Zenith2/Docs/Design/ASSET/Database.md → "Canonical meta form"`:

| Rule | Kind | Members in canonical order (required marked R) |
|---|---|---|
| 0 root | `CANONICAL_NODE_OBJECT` | `$schema` R, `schemaVersion` R, `guid` R, `importer` R (rule 1), `preset` R, `settings` R (rule 2), `platforms` (rule 3), `compression` (rule 4), `subAssets` R (rule 5), `labels` (rule 7), `physics` (`CANONICAL_NODE_OPAQUE`), `provenance` (`CANONICAL_NODE_OPAQUE`) |
| 1 importer | `CANONICAL_NODE_OBJECT` | `id` R, `settingsVersion` R |
| 2 settings | `CANONICAL_NODE_KEYED_MAP` | members sorted by key byte order; each value `CANONICAL_NODE_VALUE` (numbers keep their raw text) or `CANONICAL_NODE_INLINE_ARRAY` for float arrays |
| 3 platforms | `CANONICAL_NODE_KEYED_MAP` | `android`, `windows` in key byte order; each an object of rule 2 |
| 4 compression | `CANONICAL_NODE_OBJECT` | `codec` R, `level`, `dictionary` |
| 5 subAssets | `CANONICAL_NODE_ORDERED_ARRAY` | elements of rule 6; ASSET keeps the array sorted by `key` byte order (the framework preserves element order) |
| 6 sub-asset | `CANONICAL_NODE_OBJECT` | `key` R, `guid` R, `kind` R, `orphaned`, `fingerprint`, `exclude`, `materialRemap` (`CANONICAL_NODE_KEYED_MAP` of GUID text), `annotation`, `rig` (cross-area row CT22 (a)) |
| 7 labels | `CANONICAL_NODE_INLINE_ARRAY` | strings, kept sorted by ASSET |

Unknown members, `x-` members included, are anchored and re-emitted by the framework's rule C3 ([Z2-N-CONTRACT-543]).

Rule 2 uses `CANONICAL_NODE_KEYED_MAP` for keys that are importer setting keys (`lod.ratios`): rule C5's keyed map accepts any non-empty UTF-8 key without control characters and sorts by key byte order, and `m_pfnVisitFieldSets` is a generic visitor of any separately versioned embedded object (SCENE's answer to AS23, cross-area row S15). The type's ID is `Zenith_MakeTypeID("Zenith.AssetMeta")`.

## Member migrations

A top-level `.meta` member that carries its own `schema` integer migrates by its own chain inside the same embedded visitor, after the importer settings (cross-area row CT24 (a)); CONTENT's `provenance` is the one such member today (schema 1, no step). The chain is the constexpr table `Zenith_GetAssetMetaMemberMigrations()` in ZenithAuthoring's ASSET files, not a run-time registration function, because a registry filled at start-up would be mutable global state (`Zenith2/Docs/Contracts.md → "No mutable global state"`): an owner that bumps its member's schema appends its `{member, fromVersion, step}` row to that table in its own diff (ASSET reviewing), with fixtures `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Asset/AssetMeta/<member>/v<N>.json` and `v<N>.expected.json`.

1. For each member named in the table, in table order: absent → nothing; `schema` not a positive integer → `MIGRATION_ERROR_BAD_INPUT` at path `<member>.schema`.
2. `schema` above the newest version the table reaches (`uASSET_META_PROVENANCE_SCHEMA_VERSION` for `provenance`) → the member is left unchanged and the document opens read-only, as an importer-settings version does.
3. Below → each row's step from `schema` upward over the member's subtree, then `schema = v + 1`; a missing row in the chain is refused when the migration registry freezes (`DOCUMENT_ERROR_INVALID_DESC`, the member and the version named).

`Test_AssetMeta_MemberMigrationTable` (Tests.md "Meta document tests") asserts the table is gap-free and that the provenance fixture `v1.json` opens unchanged.

## Importer settings inside meta documents

A `.meta` carries two versions: the envelope's `schemaVersion` (the chain above) and `importer.settingsVersion`, the version of the importer's settings objects (`settings` and each `platforms.<p>` object). The framework runs one chain per document type, so ASSET migrates the settings objects through the type's embedded visitor, which the framework calls after the envelope chain and also for a current envelope (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Opening a document"` step 7):

1. Read `importer.id` and `importer.settingsVersion` from the tree (`DOCUMENT_ERROR_SHAPE` through `MIGRATION_ERROR_BAD_INPUT` when absent or not a positive integer).
2. Find the importer through the user data's `Zenith_AssetMetaSettingsSource` (null: leave every settings object unchanged and return success); an unknown ID leaves the tree unchanged and reports `DOCUMENT_WARNING_UNKNOWN_FIELD` at path `importer.id` (the database reports `IMPORT_ERROR_UNKNOWN_IMPORTER` separately, `Zenith2/Docs/Design/ASSET/Import.md → "Import errors"`).
3. `settingsVersion` above the importer's `m_uSettingsVersion`: leave the tree unchanged, log `settingsVersion <n> is newer than importer '<id>' version <m>; opened read-only`, and return success; `Zenith_ReadAssetMeta`, given the frozen importer registry, then marks the meta read-only (`IsReadOnly()`; refused with `META_NEWER_VERSION` when read-only is not allowed), so the cook uses it only when every key it needs parses and no tool writes it (Z2-R-ASSET-131, the importer-settings form of [Z2-N-CONTRACT-537]).
4. Below: for v from `settingsVersion` to `m_uSettingsVersion − 1`, call the importer's step for v (`Zenith_ImporterRegistry::FindSettingsStep(id, v)`, a `Zenith_PfnMigrationStep`) once over the `settings` object and once over each `platforms.<p>` object, each as a `Zenith_DocumentTree` rooted at that object, with the context's document ID the `.meta`'s GUID; then write `importer.settingsVersion = v + 1`.
5. Equal: nothing.

Because the visitor runs inside the framework's open, a settings upgrade is previewed, backed up and journaled exactly like an envelope upgrade: `PlanMigration` sees new canonical bytes and marks the entry changed.

`Zenith_ImporterRegistry::Freeze` refuses an importer whose chain has a gap below its `m_uSettingsVersion` (`IMPORT_ERROR_INVALID_DESC`, naming the importer and the missing version), the importer-settings form of [Z2-N-CONTRACT-539].

## Reading and writing meta documents through the framework

| Tool mode | Storage (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "ZenithDocumentHost"`) | Migrator calls |
|---|---|---|
| the build's cook (`--game … --platform … --out …`) | one `Zenith_HostDocumentStorage` per content storage root (the game directory `Zenith2/Games/<Name>/`, and each linked engine library's `Internal/Content/`), `m_bReadOnly = true` | `Open` of each `.meta`; no plan, apply or save |
| `--assign-guids`, `--fix-duplicate-guids`, the cook server's `assignGuids`, `setSettings`, `import` | the same roots, writable | `RecoverPendingJournals`, then `SaveDocuments` with one batch per request |
| `--migrate [--preview]`, the cook server's `migratePreview`, `migrateApply` | the same roots, writable | `RecoverPendingJournals`, `PlanMigrationOfDirectory` per root restricted to `.meta` paths (`WriteReportJson` is the preview), `ApplyPlan` |
| ZenithEditor | ZenithEditor's own migrator over the same roots, writable | its scene and prefab saves, which the migrator wraps in the root's writer lock per batch itself; `.meta` documents only through the cook server (the editor never writes a `.meta` itself; it asks the server) |

- Every writable ZenithCook row takes the storage root's writer lock through its storage's `GetWriterLock()` (SCENE's `Zenith_DocumentWriterLock` over ASSET's file `<storage root>/.migration/writer.lock`, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Storage-root writer lock"`) before `RecoverPendingJournals` and holds it through its last write of the batch — a CLI mode for its run, the server for one request (`Zenith2/Docs/Design/ASSET/Cook.md → "Output tree and database locks"` item 5); the migrator's writing members reuse that hold. It re-opens every document it will write inside the hold, so the framework's journal at a root always belongs to the lock's holder or to a writer that has exited.
- `Zenith_ReadAssetMeta` takes the framework's `Zenith_OpenedDocument` (`Zenith2/Docs/Design/ASSET/Database.md → "Reading and writing meta documents"`); a document opened `DOCUMENT_OPEN_READ_ONLY_NEWER` is used by the cook only when every key it needs parses, and no mode writes it ([Z2-N-CONTRACT-537]).
- Every `.meta` write is a `Zenith_DocumentWrite` whose bytes are `Zenith_WriteAssetMetaCanonical`'s output and whose expected old digest is the digest the framework recorded when the document was opened (null for a new sidecar), so an external edit made after the open is never overwritten (`DOCUMENT_ERROR_STALE_PLAN`).
- Backups of `.meta` writes follow the framework's rules (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Backups"`): migration batches under `.migration/backups/<UTC timestamp>/` of the storage root, save batches under `.migration/backups/save-<UTC timestamp>/`. For an engine library's content root the storage root is `Zenith2/Engine/<Lib>/Internal/Content/`, so its `.migration/` directory lies inside that root; the nested `Zenith2/.gitignore` excludes `.migration/` directories wherever they occur, and the scan skips every `.`-prefixed name.
- The batch timestamp (`m_szBatchTimestamp`) is the tool's UTC time at the start of the request; it names backups and journals and never reaches a document or a cooked byte.

## Removed settings and unknown fields

- Unknown members of a `.meta`, `x-` members included, survive every open, migration and write in their anchored positions, and each is reported once per document per field path per load (`DOCUMENT_WARNING_UNKNOWN_FIELD`, [Z2-N-CONTRACT-544]); ASSET's canonical schema lists every known member, so nothing ASSET defines is ever treated as unknown.
- An importer-settings step that renames a setting key rewrites it in `settings` and in every `platforms.<p>` object in the same step ([Z2-N-CONTRACT-547]); that is the override-preservation rule for `.meta` documents, whose overrides are keyed by setting key. Before it removes the old key it calls `m_pfnDeclareRemoved` of its `Zenith_MigrationDiagnostics` with the old key's dotted path (`settings.<old>`, `platforms.<p>.<old>`), because the framework's unknown-member guard otherwise treats the vanished member as a deleted unknown field and fails the step (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Unknown members across migration steps"`, cross-area row S18).
- A step whose importer version removes a setting moves the member to `x-removed.<key>` (same value, same object) instead of deleting it, declares the old path removed through `m_pfnDeclareRemoved` in the same way, and reports `DOCUMENT_WARNING_DANGLING_OVERRIDE` at the old key path; the framework's preview lists it. `x-removed.` members are ignored by settings resolution and kept like every `x-` member ([Z2-N-CONTRACT-545]).

## Recovery before a cook

- Every sidecar-writing mode (the rows above that write) takes each writable root's writer lock through `GetWriterLock()->Acquire(cook.sourceLockWaitMs, …)` and then calls `Zenith_DocumentMigrator::RecoverPendingJournals` on that root before anything else, as the framework requires; any journal it finds was left by a writer that has exited, so recovery never touches a live batch. The recovery report's counts go to the `cook.database` log. A mode whose `Acquire` returns `DOCUMENT_ERROR_STORAGE_LOCKED` exits with code 6 (`COOK_ERROR_SOURCE_LOCKED`; a server request is answered with it) having recovered and written nothing.
- The build's cook never writes sources: it calls `RecoverPendingJournals` over each read-only storage before scanning, which changes nothing and reports `DOCUMENT_ERROR_JOURNAL_PENDING` naming the first journal when one exists (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"`, recovery over read-only storage). Its read-only storage has no writer lock (`GetWriterLock()` is null), so it then probes that root's lock file read-only (`Zenith_ProbeStorageRootWriterLock`, `Zenith2/Docs/Design/ASSET/Cook.md → "Output tree and database locks"` item 6): held, a writer is active, so it waits up to `cook.sourceLockWaitMs` for the lock and the journal to go and then scans, or exits with code 6 and `COOK_ERROR_SOURCE_LOCKED` naming the holder and recommending only a retry of the build; not held, the journal is a dead writer's and the cook stops with `COOK_ERROR_GAME_INVALID` naming the journal and the command that recovers it (`ZenithCook --game <Name> --assign-guids`, or opening the editor), exit code 3.
- ZenithEditor's scene and prefab saves join the same lock: SCENE's migrator takes it through `GetWriterLock()` around every `SaveDocuments`, `ApplyPlan` and `RecoverPendingJournals` over a writable host storage and recovers only under it (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Storage-root writer lock"`, cross-area rows AS31 and AS32), so an editor save batch and a ZenithCook writer at one root never overlap, and an engine content root shared by several games is serialised the same way.
- A journal found corrupt (`DOCUMENT_ERROR_JOURNAL_CORRUPT`) stops every mode with `COOK_ERROR_GAME_INVALID`; a person resolves it.

## Meta migration fixtures

- For `Zenith.AssetMeta` and for the settings chains of ASSET's own importers (`raw`, `zenith-doc`), `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/<Type>/v<N>.json` and `v<N>.expected.json` exist for every version from 1 (`<Type>` is `AssetMeta`, `ImporterSettings/raw`, `ImporterSettings/zenith-doc`) ([Z2-N-CONTRACT-548]); version 1's pair is the identity (the expected output is the canonical form of the input), which proves the unknown-field, `x-` and canonical paths.
- `Test_MetaMigration_Fixtures` enumerates versions 1 … current for each type, requires both files of each version and migrates each through a `Zenith_MemoryDocumentStorage` holding the fixture, comparing bytes, so a version bump without its pair fails ([Z2-N-CONTRACT-549], the rule of `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Migration fixtures"`).
- CONTENT's importers ship their own settings fixtures under the same directory layout, `ImporterSettings/<id>/`, tested by the same test.

## Registered document types

ZenithCook's composition root and ZenithEditor register these types into their one `Zenith_MigrationRegistry` before `Freeze`, in this order. Every registration function is tool-side and lives in its owner's `ZenithAuthoring` root header (`Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`), which ZenithCook and ZenithEditor both link through `ZenithAuthoring`; a row whose move has not landed yet is marked "(move pending)" and is linked from its owner's ZenithCook `Internal/<Area>/` header until then (cross-area row AuB20):

| Type | Stable name | Registered by | Specified in |
|---|---|---|---|
| importer-settings `.meta` | `Zenith.AssetMeta` | `Zenith_RegisterAssetMetaMigrations` (`Zenith_AuthoringAsset.h`; move pending, row ED42) | this file |
| scene and prefab documents | SCENE's | SCENE's registration function | `Zenith2/Docs/Design/SCENE.md → "Migration hooks"` |
| schemas, materials, `game.json` and animation documents | their owners' | their owners' registration functions (GEOM's `Zenith_RegisterMaterialMigrations` in `Zenith_AuthoringGeom.h`, ANIM's `Zenith_RegisterAnimationDocumentTypes` in `Zenith_AuthoringAnimation.h`) | each owner's "Migration hooks" |
| terrain, terrain tile, terrain layer, foliage type, placement, atmosphere, weather, water and cloud-noise documents | the nine `Zenith.Environment.<Type>Document` names (`Zenith.Environment.TerrainDocument`, `.TerrainTileDocument`, `.TerrainLayerDocument`, `.FoliageTypeDocument`, `.PlacementDocument`, `.AtmosphereDocument`, `.WeatherDocument`, `.WaterDocument`, `.CloudNoiseDocument`) | ENV's `Zenith_RegisterEnvironmentDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith_AuthoringEnvironment.h`; move pending, row ED40; ZenithEnvironment itself has no `ZenithDocument` edge; cross-area row EN25) | `Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"` |
| PLAT's bindings and configuration documents | PLAT's | PLAT's step table (version 1 has none) | `Zenith2/Docs/Design/PLAT.md → "Migration hooks"` |
| audio bank and mixer documents (`.bank.json`, `.mixer.json`) | `Zenith.Audio.BankDocument`, `Zenith.Audio.MixerDocument` | AUDIO's `Zenith_RegisterAudioDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith_AuthoringAudio.h`; cross-area rows AU9 (b), AuB20) | AUDIO's design, "Registration of document types" |
| particle effect documents (`.vfx.json`) | `Zenith.VFX.EffectDocument` | VFX's `Zenith_RegisterVFXDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith_AuthoringVFX.h`; move pending, Stage 5, row ED44; cross-area row VX5 (b)) | VFX's design, "Importer and intermediate" |
| UI, theme, font family, localization project and string table documents | `Zenith.UI.UIDocument`, `Zenith.UI.ThemeDocument`, `Zenith.UI.FontFamilyDocument`, `Zenith.UI.LocalizationProjectDocument`, `Zenith.UI.StringTableDocument` | UI's `Zenith_RegisterUIDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith_AuthoringUI.h`; cross-area row UI12) | UI's design, "Migration chains and fixtures" |
| cinematic sequence documents (`.cinematic.json`, schema tag `zenith2.cinematic`, version 1) | `Zenith.Cinematics.SequenceDocument` | CINE's `Zenith_RegisterCinematicDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith_AuthoringCinematics.h`; cross-area row CN1 (b)) | `Zenith2/Docs/Design/CINE/PublicAPI.md → "Tool-side headers"` |
| import preset documents (`*.importpreset.json`) and CONTENT's other document types | CONTENT's | CONTENT's `Zenith_RegisterContentDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith_AuthoringContent.h`; cross-area row CT4) | `Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Preset document type registration"` |
| WORLD's documents | WORLD's | WORLD's `Zenith_RegisterWorldDocumentTypes` (`Zenith_AuthoringWorld.h`) | `Zenith2/Docs/Design/WORLD.md → "Migration hooks"` |

The importer registry is created and frozen before the migration registry, because `Zenith.AssetMeta`'s user data is the frozen importer registry.

## Migration errors at the ASSET boundary

| Framework result | ASSET error | Where |
|---|---|---|
| `DOCUMENT_ERROR_PARSE`, `_SHAPE` on a `.meta` | `ASSET_DATABASE_ERROR_META_PARSE`, `_META_SCHEMA` with the key path logged | `Zenith2/Docs/Design/ASSET/Database.md → "Database errors"` |
| `DOCUMENT_ERROR_NEWER_VERSION`, `_READ_ONLY` | `ASSET_DATABASE_ERROR_META_NEWER_VERSION` | the same |
| `DOCUMENT_ERROR_JOURNAL_PENDING`, `_JOURNAL_CORRUPT`, `_RECOVERY_CONFLICT` | `ASSET_DATABASE_ERROR_JOURNAL_PENDING`, then `COOK_ERROR_GAME_INVALID` (exit 3) | the same; `Zenith2/Docs/Design/ASSET/Cook.md → "Cook errors"` |
| `DOCUMENT_ERROR_LOW_DISK` (the framework's free-space refusal before a journaled write; ASSET has no check of its own) | `ASSET_DATABASE_ERROR_DOCUMENT`, reported as `COOK_ERROR_LOW_DISK` (exit code 4), nothing written | the same |
| `DOCUMENT_ERROR_STORAGE_LOCKED` (from `Zenith_DocumentWriterLock::Acquire`, or from a migrator writing member that found the lock held by another process) | `COOK_ERROR_SOURCE_LOCKED` naming the root and the holder's PID and mode: exit code 6 for a CLI mode, the error of a server request; nothing recovered or written | `Zenith2/Docs/Design/ASSET/Cook.md → "Output tree and database locks"` item 5 |
| `DOCUMENT_ERROR_STALE_PLAN` | `ASSET_DATABASE_ERROR_STALE_DOCUMENT`; the server answers `COOK_ERROR_META_CHANGED` and the editor re-reads; a sidecar-writing CLI mode exits with code 6 | the same |
| any other document error | `ASSET_DATABASE_ERROR_DOCUMENT` with the `Zenith_DocumentErrorCode` in the cook error's detail | the same |

Each mapping logs the framework's error record (document index, line, column) with the `.meta` path ([Z2-N-CONTRACT-463]).
