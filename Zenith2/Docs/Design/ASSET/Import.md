# ASSET design: import and reimport

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. It specifies the importer interface CONTENT's importers implement, the importer registry, importer settings (schema, resolution, the compression setting), the import algorithm, import diagnostics, cross-asset inputs, intermediates, and reimport. The code lives in `Zenith2/Tools/ZenithCook/Internal/Import/`.

## Scope of this file

| Topic | Heading |
|---|---|
| what an importer implements | "Importer interface" |
| which importer handles a file | "Importer registry" |
| `.meta` settings | "Import settings" |
| what an import run does | "Import algorithm" |
| import diagnostics, their codes, storage and replay | "Import diagnostics" |
| inputs read from other assets: published properties, documents relative to another source | "Cross-asset inputs" |
| import outputs | "Intermediates" |
| running again | "Reimport" |

## Importer interface

### Zenith_ImporterDesc

```cpp
// Zenith2/Tools/ZenithCook/Internal/Import/Zenith_Importer.h
/// @contract enum Zenith_ImporterSettingType
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
enum Zenith_ImporterSettingType : uint8
{
	IMPORTER_SETTING_TYPE_BOOL = 1, IMPORTER_SETTING_TYPE_INT, IMPORTER_SETTING_TYPE_FLOAT,
	IMPORTER_SETTING_TYPE_STRING, IMPORTER_SETTING_TYPE_ENUM, IMPORTER_SETTING_TYPE_FLOAT_ARRAY,
};

/// @contract value Zenith_ImporterSettingDesc
/// Ownership: constexpr data of the importer.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_fMin and m_fMax in the setting's own unit (its description names it).
/// Errors: Validated at registration (IMPORT_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
struct Zenith_ImporterSettingDesc
{
	const char* const m_szKey = nullptr;          // dotted lower camel case: "lod.ratios", "texture.usage"
	const Zenith_ImporterSettingType m_eType = {};
	const char* const m_szDefaultJson = nullptr;  // the default as JSON text: "true", "0.5", "\"albedo\"", "[1,0.5,0.25]"
	const float m_fMin = 0.0f, m_fMax = 0.0f;     // INT and FLOAT settings; both 0 means unbounded; INT bounds are exact integers ≤ 2^24
	const Zenith_Span<const char* const> m_aszEnumValues = {};
	const uint32 m_uMaxArrayCount = 0;
	const bool m_bPlatformOverridable = false;    // may appear under "platforms"
	const char* const m_szDescription = nullptr;
};

/// @contract value Zenith_ImporterDesc
/// Ownership: constexpr data of the importer.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Registration rejects a duplicate ID, a claimed extension already claimed, a settings key outside the grammar or a
///   default that does not parse or validate (IMPORT_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 2. m_uVersion is an import key input: bumping it re-imports every source the importer owns.
/// @endcontract
struct Zenith_ImporterDesc
{
	const char* const m_szID = nullptr;           // "gltf", "image", "audio", "raw", "zenith-doc"
	const uint32 m_uVersion = 0;                 // bumped by any change to intermediate bytes for unchanged inputs
	const uint32 m_uSettingsVersion = 0;         // the importer's settings schema version (Zenith.ImporterSettings.<id>)
	const Zenith_Span<const char* const> m_aszExtensions = {}; // lower case, with the dot: ".gltf", ".glb"
	const Zenith_Span<const Zenith_ImporterSettingDesc> m_axSettings = {};
	const Zenith_Span<const Zenith_AssetKind> m_aeProducedKinds = {};
	const bool m_bOutOfProcess = false;           // run in a cook worker process (CONTENT sets it for third-party parsers)
	const Zenith_Span<const char* const> m_aszToolVersions = {}; // manifest names whose pinned versions are import key inputs
	// The settings chain: element v − 1 migrates settings version v to v + 1; exactly m_uSettingsVersion − 1 elements.
	const Zenith_Span<const Zenith_PfnMigrationStep> m_apfnSettingsSteps = {};
};

/// @contract enum Zenith_ImportDiagnosticLevel
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; ERROR diagnostics do not fail the import by themselves (Import's result does).
/// Version: since Z2 Stage 2. Tool-side; the report's "severity" strings are "info", "warning", "error".
/// @endcontract
enum Zenith_ImportDiagnosticLevel : uint8
{
	IMPORT_DIAGNOSTIC_LEVEL_INFO = 1,
	IMPORT_DIAGNOSTIC_LEVEL_WARNING,
	IMPORT_DIAGNOSTIC_LEVEL_ERROR,
};

/// @contract value Zenith_ResolvedCompression
/// Ownership: Plain value inside Zenith_ResolvedSettings.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its settings.
/// Units: m_iLevel a codec level.
/// Errors: None; built only from a fragment Zenith_ValidateCompressionSetting accepted, or from the step's default.
/// Version: since Z2 Stage 2. Tool-side; every member is a cook-key input (DDC.md "Key inputs").
/// @endcontract
struct Zenith_ResolvedCompression
{
	const Zenith_CompressionCodecID m_eCodec = {}; // the codec the fragment names, or the cook step's default when absent
	const sint32 m_iLevel = 0;                   // resolved through the codec's ResolveLevel
	const Zenith_AssetGUID m_xDictionary = {};    // null when none
	const bool m_bFromMeta = false;               // false: the step's default applied
};

/// @contract type Zenith_ResolvedSettings
/// Ownership: Built by the import driver per (source, platform) from the four layers of "Settings resolution"; owns its JSON
///   document; borrowed by importers and cook steps for the call.
/// Thread affinity: IMMUTABLE after construction; readable from any thread.
/// Invalidation: With the plan snapshot that owns it (Cook.md "Plan construction").
/// Units: As each setting's description.
/// Errors: Getters return IMPORT_ERROR_SETTING_INVALID for an unknown key or a type mismatch (a step or importer defect,
///   never a content error, because resolution validated every value).
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
class Zenith_ResolvedSettings
{
public:
	Zenith_Result<bool, Zenith_ImportError> GetBool(Zenith_StringView strKey) const;
	Zenith_Result<sint64, Zenith_ImportError> GetInt(Zenith_StringView strKey) const;
	Zenith_Result<float, Zenith_ImportError> GetFloat(Zenith_StringView strKey) const;
	Zenith_Result<Zenith_StringView, Zenith_ImportError> GetString(Zenith_StringView strKey) const;   // STRING and ENUM
	Zenith_Result<Zenith_Span<const float>, Zenith_ImportError> GetFloatArray(Zenith_StringView strKey) const;
	const Zenith_ResolvedCompression& GetCompression() const;    // the resolved "compression" fragment
	const Zenith_Hash128& GetHash() const;                        // the settings hash (a key input)
	Zenith_StringView GetCanonicalText() const;                   // the hashed canonical form
	Zenith_AssetPlatform GetPlatform() const;
};

/// @contract value Zenith_ImportResult
/// Ownership: Built by the import driver for Zenith_AssetDatabase::UpdateAfterImport; spans point into the driver's arena.
/// Thread affinity: OWNER_THREAD (the cook's main thread).
/// Invalidation: The driver's next import.
/// Units: Bytes; hashes.
/// Errors: None.
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
struct Zenith_ImportProduct { Zenith_StringView m_strKey; Zenith_AssetGUID m_xGUID; Zenith_AssetKind m_eKind; bool m_bPrimary; };
struct Zenith_ImportInput { Zenith_StringView m_strPath; Zenith_Hash128 m_xHash; };   // Zenith2-relative, sorted by path
// A .meta subAssets entry as it stood before this import ("Product members and fingerprints").
struct Zenith_ImportPreviousProduct
{
	Zenith_StringView m_strKey;
	Zenith_AssetGUID m_xGUID;
	Zenith_AssetKind m_eKind;
	bool m_bOrphaned;
	bool m_bHasFingerprint;
	uint64 m_ulFingerprint;
	Zenith_StringView m_strMembersJson;           // the entry's owner members (exclude, materialRemap, annotation, rig) as canonical compact JSON, "{}" when none
};
// One product of another source, as ReadSourceProducts records it ("Source product lists").
struct Zenith_ImportProductListInput { Zenith_AssetGUID m_xSource; Zenith_Hash128 m_xHash; };
// A published property another asset's cook step publishes, read by ReadPublishedProperty ("Published-property inputs").
struct Zenith_ImportPropertyInput
{
	Zenith_AssetGUID m_xAsset;                    // the producing asset (a source or a sub-asset)
	Zenith_AssetPlatform m_ePlatform;
	Zenith_StringView m_strName;                  // the property name, ^[a-z][a-zA-Z0-9.]{0,63}$
	Zenith_Hash128 m_xHash;                       // Zenith_Hash128Of of the property's bytes: the import-key input
	Zenith_Hash128 m_xProducerKey;                // the producing item's cook key when it was read: a cache hint only, never a key input
};
// One stored import diagnostic ("Stored diagnostics"); strings point into the result's arena.
struct Zenith_StoredImportDiagnostic
{
	Zenith_ImportDiagnosticLevel m_eLevel;
	Zenith_StringView m_strCode;                  // [A-Z0-9_]{1,64}, or empty for a diagnostic reported without a code
	Zenith_StringView m_strMessage;               // at most uIMPORT_DIAGNOSTIC_MAX_MESSAGE_BYTES
};
inline constexpr uint32 uIMPORT_DIAGNOSTIC_MAX_PER_IMPORT = 256;
inline constexpr uint32 uIMPORT_DIAGNOSTIC_MAX_CODE_BYTES = 64;
inline constexpr uint32 uIMPORT_DIAGNOSTIC_MAX_MESSAGE_BYTES = 4096;
struct Zenith_ImportResult
{
	Zenith_Hash128 m_xImportKey;
	Zenith_Hash128 m_xSourceHash;
	Zenith_Hash128 m_xSettingsHash;
	uint32 m_uImporterVersion = 0;
	Zenith_Span<const Zenith_ImportProduct> m_axProducts;     // declared products in key byte order
	Zenith_Span<const Zenith_StringView> m_astrOrphanedKeys;  // keys of the .meta no longer produced
	Zenith_Span<const Zenith_ImportInput> m_axInputs;         // every ReadInput and every document input, sorted by path
	Zenith_Span<const Zenith_ImportPropertyInput> m_axPropertyInputs;   // every ReadPublishedProperty, sorted by (GUID bytes, platform, name bytes)
	Zenith_Span<const Zenith_ImportProductListInput> m_axProductListInputs;   // every ReadSourceProducts, sorted by source GUID bytes
	Zenith_Span<const uint64> m_aulFingerprints;             // parallel to m_axProducts; meaningful where m_abHasFingerprint
	Zenith_Span<const bool> m_abHasFingerprint;
	Zenith_Span<const bool> m_abDerived;                      // parallel to m_axProducts: declared by DeclareDerivedProduct
	Zenith_Span<const Zenith_CookReportMemberInput> m_axReportMembers;  // AddReportMember calls, in call order
	Zenith_Span<const Zenith_AssetGUID> m_axReferences;       // targets ResolveReferencePath returned (settings-input and reference edges)
	Zenith_Span<const Zenith_StoredImportDiagnostic> m_axDiagnostics;   // in Report order, at most uIMPORT_DIAGNOSTIC_MAX_PER_IMPORT
	uint32 m_uDiagnosticsDropped = 0;                        // Report calls beyond the cap
	bool m_bFromDDC = false;                                  // the intermediates came from a verified DDC hit
};
```

### Zenith_Importer

```cpp
/// @contract interface Zenith_Importer
/// Ownership: Implemented by CONTENT (and ASSET's passthrough importers); one static instance per importer, registered by
///   the cook services at start.
/// Thread affinity: ANY_THREAD. Import may run concurrently for different sources, in process or in a worker process.
/// Invalidation: None.
/// Units: Bytes; the importer's settings carry their own units in their descriptions.
/// Errors: Import returns Zenith_Result<void, Zenith_ImportError>; malformed source bytes are IMPORT_ERROR_SOURCE_INVALID with
///   the detail reported through the context, never an assert or a crash of the cook (an out-of-process importer that crashes
///   is reported by the worker pool as IMPORT_ERROR_WORKER_CRASHED).
/// Reentrancy: NOT_APPLICABLE. It calls only its context.
/// Version: since Z2 Stage 2. Source API of the tool.
/// @endcontract
class Zenith_Importer
{
public:
	virtual ~Zenith_Importer() = default;
	virtual const Zenith_ImporterDesc& GetDesc() const = 0;
	// Pure function of the source bytes, the declared inputs it reads, the resolved settings and the importer version.
	virtual Zenith_Result<void, Zenith_ImportError> Import(Zenith_ImportContext& xContext) = 0;
};
```

### Zenith_ImportContext

```cpp
/// @contract type Zenith_ImportContext
/// Ownership: Owned by the import driver for one import; the importer borrows it for the call.
/// Thread affinity: OWNER_THREAD (the importer's thread).
/// Invalidation: Spans returned by ReadSource and ReadInput are valid until Import returns.
/// Units: Bytes.
/// Errors: Zenith_Result<…, Zenith_ImportError>.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ImportContext
{
public:
	Zenith_ByteSpan ReadSource() const;
	Zenith_StringView GetSourcePath() const;                 // for diagnostics
	// Another file the import reads, relative to the source's directory, or a thirdparty:<entry>/<p> path of the read-only
	// input root (ASSET.md "Content root mapping"): recorded as an import-input edge and hashed into the import key of the next
	// run. Paths are validated like virtual paths (no .., no absolute paths); outside the content roots is
	// IMPORT_ERROR_INPUT_OUTSIDE_ROOT; a thirdparty: entry ZenithCook does not name is IMPORT_ERROR_INPUT_NOT_FOUND.
	Zenith_Result<Zenith_ByteSpan, Zenith_ImportError> ReadInput(Zenith_StringView strRelativePath);
	const Zenith_ResolvedSettings& GetSettings() const;
	// Declare a product; returns its GUID (kept across reimports by key, "Identity preservation on reimport").
	Zenith_Result<Zenith_AssetGUID, Zenith_ImportError> DeclareProduct(Zenith_StringView strKey, Zenith_AssetKind eKind,
		bool bPrimary);
	// Move an existing product's GUID from its old key to a new key before DeclareProduct(newKey, …); the old key must be in
	// the .meta and the new key must not (IMPORT_ERROR_DUPLICATE_PRODUCT_KEY otherwise).
	Zenith_Result<void, Zenith_ImportError> RenameProduct(Zenith_StringView strOldKey, Zenith_StringView strNewKey);
	// The platform-independent intermediate of a product: named blobs, stored in the DDC under the import key.
	Zenith_Result<void, Zenith_ImportError> WriteIntermediate(const Zenith_AssetGUID& xProduct,
		Zenith_StringView strBlobName, Zenith_ByteSpan xBytes);
	// References the product's content makes to other assets by GUID (a material's textures), or by source-relative path,
	// which the driver resolves to the target's GUID through the database.
	Zenith_Result<Zenith_AssetGUID, Zenith_ImportError> ResolveReferencePath(Zenith_StringView strRelativePath);
	void Report(Zenith_ImportDiagnosticLevel eLevel, Zenith_StringView strMessage);   // into the cook report (at most 256 per import; further ones counted)
	// The same, with a code that becomes the cook report entry's "code" ("Diagnostic codes"): strCode matches [A-Z0-9_]{1,64}
	// and does not begin with "COOK_" or "IMPORT_"; empty is a diagnostic without a code. Shares the 256-per-import cap.
	void Report(Zenith_ImportDiagnosticLevel eLevel, Zenith_StringView strCode, Zenith_StringView strMessage);
	// A property another asset's cook step publishes for ePlatform (Zenith_CookContext::PublishProperty), recorded as an import
	// input keyed by the property bytes' Zenith_Hash128Of ("Published-property inputs"). INPUT_NOT_FOUND when the asset has no
	// live record, its producing item fails, or its step does not publish strName; PROPERTY_PENDING when the property is not yet
	// resolved in this run (the import is then re-run in the next wave and this run's outcome is discarded); INVALID_DESC for a
	// null GUID, an invalid platform or name, or a call from a worker process.
	Zenith_Result<Zenith_ByteSpan, Zenith_ImportError> ReadPublishedProperty(const Zenith_AssetGUID& xAsset,
		Zenith_AssetPlatform ePlatform, Zenith_StringView strName);
	// An authoring document by a path relative to the directory of another asset's source (a terrain's tileFiles), read whole,
	// recorded as an import input (Zenith2-relative path and Zenith_Hash128Of of its bytes) and migrated in memory through a
	// call-local migrator over those bytes ("Documents relative to another asset"). found = false, with no error and the absent
	// path still recorded, when no file exists at the path.
	Zenith_Result<bool, Zenith_ImportError> OpenInputDocumentRelativeTo(const Zenith_AssetGUID& xAnchor,
		Zenith_StringView strRelativePath, Zenith_OpenedDocument& xOut);
	// The source's .meta subAssets entries before this import, in key byte order ("Product members and fingerprints").
	Zenith_Span<const Zenith_ImportPreviousProduct> GetPreviousProducts() const;
	// Records a product's fingerprint after DeclareProduct; written into its entry in sidecar-writing modes.
	Zenith_Result<void, Zenith_ImportError> SetProductFingerprint(const Zenith_AssetGUID& xProduct, uint64 ulFingerprint);
	// A product whose GUID the importer derives; never written into the .meta subAssets ("Derived products").
	Zenith_Result<void, Zenith_ImportError> DeclareDerivedProduct(Zenith_StringView strKey, Zenith_AssetKind eKind,
		const Zenith_AssetGUID& xGUID, bool bPrimary);
	// A cook-report member this import contributes ("Import report members").
	void AddReportMember(Zenith_StringView strMember, Zenith_ByteSpan xJsonValue);
	// The source itself, another document relative to the source's directory, or another asset's source document, migrated in
	// memory and recorded as import inputs ("Input documents").
	Zenith_Result<void, Zenith_ImportError> OpenSourceDocument(Zenith_OpenedDocument& xOut);
	Zenith_Result<void, Zenith_ImportError> OpenInputDocument(Zenith_StringView strRelativePath, Zenith_OpenedDocument& xOut);
	Zenith_Result<void, Zenith_ImportError> OpenInputDocumentByGUID(const Zenith_AssetGUID& xAsset, Zenith_OpenedDocument& xOut);
	// The products another source declares, recorded as an input keyed by the list's hash ("Source product lists").
	Zenith_Result<Zenith_Span<const Zenith_ImportProduct>, Zenith_ImportError> ReadSourceProducts(const Zenith_AssetGUID& xSource);
	Zenith_Arena& GetScratch();
	Zenith_JobContext* GetJobContext();                                    // null in a worker process
};
```

`Zenith_OpenedDocument` is SCENE's (`ZenithDocument/Zenith_DocumentMigrator.h`), forward-declared in `Zenith_Importer.h` as `Zenith_CookContext`'s header does. The members added after the first eleven are tool-side since Z2 Stage 2 (`Report` with a code, `OpenInputDocumentRelativeTo`, `OpenSourceDocument`, `OpenInputDocument`, `OpenInputDocumentByGUID`), Z2 Stage 3 (`GetPreviousProducts`, `SetProductFingerprint`, `DeclareDerivedProduct`, `AddReportMember`) and Z2 Stage 4 (`ReadPublishedProperty`, `ReadSourceProducts`); each is specified in "Import diagnostics", "Cross-asset inputs" and "Reimport" below. `Zenith_CookReportMemberInput` is `{ Zenith_StringView m_strMember; Zenith_ByteSpan m_xJsonValue; }`, the type `AddReportMember` of `Zenith_CookContext` feeds.

## Importer registry

- The cook services register ASSET's passthrough importers (`raw`: any extension an owner maps to `ASSET_KIND_DATA`; `zenith-doc`: Zenith-native JSON documents such as `*.scene.json`, `*.prefab.json`, `*.material.json`, whose product kinds are registered by their owners' steps; for a `*.material.json` it writes the product's `geom.materialDocument` intermediate, the source file's bytes unchanged, which GEOM's material step reads, `Zenith2/Docs/Design/GEOM/Materials.md → "Material document intermediate"`; for a `*.vfx.json` it declares one `ASSET_KIND_PARTICLE_EFFECT` product, whose GUID is the source's `.meta` GUID and must equal the document's `id`, and writes its `vfx.effectDocument` intermediate, the source bytes unchanged, cross-area row VX5 (c); a `*.importpreset.json` is a CONTENT import preset document: `zenith-doc` validates that it parses and declares no product, so its source GUID names no cooked asset and only `.meta` `preset` members and settings-input edges reach it, cross-area row CT3 (b)), then CONTENT's importers, WORLD's and every module's in the order of Cook.md "Cook services and composition" item 4, then freeze the registry.
- An extension is claimed by exactly one importer; a `.meta` may name a different importer only if it claims the extension too (`IMPORT_ERROR_IMPORTER_MISMATCH` otherwise).
```cpp
// Zenith2/Tools/ZenithCook/Internal/Import/Zenith_ImporterRegistry.h
/// @contract type Zenith_ImporterRegistry
/// Ownership: Created by Create as a Zenith_Owned; one per tool process, owned by the cook services (or the editor's services);
///   borrows every registered importer (static instances).
/// Thread affinity: MAIN_THREAD until Freeze; ANY_THREAD and IMMUTABLE afterwards (lookups only).
/// Invalidation: None.
/// Units: None.
/// Errors: Register returns IMPORT_ERROR_INVALID_DESC for a desc failing the rules of "Zenith_ImporterDesc", a settings chain
///   whose length is not m_uSettingsVersion − 1 or that holds a null step, a duplicate ID or an extension already claimed
///   (naming both importers), and a registration after Freeze.
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
class Zenith_ImporterRegistry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ImporterRegistry>, Zenith_ImportError> Create(Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_ImportError> Register(const Zenith_Importer& xImporter);
	Zenith_Result<void, Zenith_ImportError> Freeze();
	const Zenith_Importer* FindByExtension(Zenith_StringView strExtension) const;   // lower-case, with the dot
	const Zenith_Importer* FindByID(Zenith_StringView strID) const;
	// The step migrating importer strID's settings from uFromVersion to uFromVersion + 1; null when absent.
	Zenith_PfnMigrationStep FindSettingsStep(Zenith_StringView strID, uint32 uFromVersion) const;
	Zenith_Span<const Zenith_Importer* const> GetImporters() const;   // ascending ID
};
```

- `FindByExtension` and `FindByID` are O(log n) over sorted tables; `Freeze` sorts them and checks every settings chain is gap-free ([Z2-N-CONTRACT-539] applied to settings versions).

## Import settings

### Settings schema

- Each importer's settings have schema version `m_uSettingsVersion` (named `Zenith.ImporterSettings.<id>` in diagnostics and fixtures) and a migration chain in `m_apfnSettingsSteps` (one `Zenith_PfnMigrationStep` per version increment over a settings object); the `.meta` records `importer.settingsVersion`, and the `.meta` document type's embedded visitor runs the chain inside the framework's open, preview, backup and journaled write (`Zenith2/Docs/Design/ASSET/Migration.md → "Importer settings inside meta documents"`).
- A key outside the importer's settings is kept (unknown-field preservation) and warned once per file; a value that fails its type, range or enumeration is `IMPORT_ERROR_SETTING_INVALID` naming the file, key path and rule.

### Settings resolution

The resolved settings of a source for a platform are computed in this order, each subsequent layer overriding earlier ones key by key:

1. the importer's defaults (`m_szDefaultJson`);
2. the import preset named by `.meta` `preset` (a CONTENT preset document, itself an asset source with a GUID; CONTENT defines its file format): ASSET opens it through the read-only migrator, validates it with CONTENT's `Zenith_ValidateImportPresetJson` (`Zenith2/Tools/ContentCore/Zenith_ContentValidation.h`) against the importer's settings schema, refuses a preset whose `importer.id` differs from the `.meta`'s importer with `IMPORT_ERROR_SETTING_INVALID` naming both importers and both paths (cross-area row CT3 (a)), reads its `settings` object and records a settings-input edge;
3. the `.meta` `settings` object;
4. the `.meta` `platforms.<platform>` object (keys with `m_bPlatformOverridable` only).

The result is serialised canonically (keys in byte order, numbers from their raw text, `Zenith_JsonWriter` compact style) and hashed with `Zenith_Hash128Of` into the settings hash, an import-key input. Two `.meta` members join the same hashed text after the resolved settings, so a change to either reimports the source: the `physics` object (canonical compact text, PHYS's collision import settings, cross-area row CT1 (a)), and the sub-asset entries' owner members `exclude`, `materialRemap`, `annotation` and `rig`, entry by entry in GUID byte order and keyed by GUID, never by key, so a rename that moves an entry changes no input (row CT22 (c)). `fingerprint`, `orphaned` and `provenance` never join it.

### Compression setting

The `.meta` member `compression` is COMPRESS's fragment (`Zenith2/Docs/Design/COMPRESS.md → "Compression setting fragment"`), validated with `Zenith_ParseCompressionCodecName` and `Zenith_ValidateCompressionSetting`; absent, the kind's cook step applies its own default (COMPRESS's recommendation: development cooks zstd 3, shipping cooks as PACK's policy). The resolved codec, level, dictionary GUID and the codec's `GetInfo().m_szLibraryVersion` are cook-key inputs (DDC.md "Key inputs"); an invalid fragment is `IMPORT_ERROR_SETTING_INVALID`.

## Import algorithm

For one source in state NEW or DIRTY:

1. Resolve the importer and the settings; compute the import key (DDC.md "Key inputs") over the source hash, the declared inputs' hashes from the previous run, the current hashes of the previous run's published-property inputs (resolved first, "Published-property inputs"), the settings hash, the importer ID, version and settings version, and its declared tool versions.
2. Look the key up in the DDC: the import record entry and every intermediate entry it lists (DDC.md "Import entries"); a verified hit of all of them supplies the intermediates, the recorded product list, inputs and property inputs, and the stored diagnostics, which are replayed ("Replay into the cook report"): skip to step 5.
3. Run `Import` (in process, or in a worker process when `m_bOutOfProcess`; Cook.md "Worker processes"). The importer reads only through its context; the driver records every `ReadInput`, every document opened through `OpenInputDocumentRelativeTo` and every `ReadPublishedProperty`, and keeps every `Report` ("Stored diagnostics"). A run in which a `ReadPublishedProperty` answered `IMPORT_ERROR_INPUT_PENDING` is deferred: its outcome, intermediates and diagnostics are discarded, whatever `Import` returned, and the source is imported again in a later wave ("Published-property inputs").
4. If the set of inputs or property inputs read differs from the set hashed in step 1, recompute the key over the actual inputs and store under that key (the next run hashes the right set).
5. Reconcile products with the `.meta` sub-assets ("Identity preservation on reimport"); a `.meta` change is written only in sidecar-writing modes; in the build's cook a product without a GUID fails `COOK_ERROR_META_SUBASSET_MISSING`.
6. Store the intermediates and the import record entry in the DDC; update the record (`Zenith_AssetDatabase::UpdateAfterImport`, which stores the import's diagnostics with the record) and its edges.

Until the DDC exists (Stage 2 and 3), step 2 is skipped and step 6 stores the intermediates in the database's intermediate area `${ZENITH_OUT}/AssetDB/<Name>/Intermediates/` and the diagnostics only with the record; a CURRENT source is then never imported, and its stored diagnostics are replayed.

An import is deterministic: no clock, thread count, address, hash-map iteration order or host path reaches an intermediate or a diagnostic (`Test_Import_SameInputsSameIntermediateBytes`).

## Import diagnostics

### Diagnostic codes

- `Report(eLevel, strMessage)` is `Report(eLevel, "", strMessage)`: a diagnostic without a code. Both overloads share one cap: the first `uIMPORT_DIAGNOSTIC_MAX_PER_IMPORT` = 256 calls of an import are kept in call order and every further call only increments the import's dropped count; when an import ends with n > 0 dropped, one closing record follows its kept ones (level INFO, no code, message `"<n> further diagnostics not recorded"`, n in decimal).
- The context validates each call before it reaches the driver's sink, in this order: a level outside 1 … 3 is recorded as `IMPORT_DIAGNOSTIC_LEVEL_ERROR`; a message that is not valid UTF-8 has each invalid byte sequence replaced by U+FFFD; a message longer than `uIMPORT_DIAGNOSTIC_MAX_MESSAGE_BYTES` = 4,096 bytes is cut to the longest prefix of at most 4,096 bytes that ends on a code-point boundary; a non-empty `strCode` must match `^[A-Z0-9_]{1,64}$` and must not begin with `COOK_` or `IMPORT_` (ASSET's own code names), and a code that fails is dropped and the message becomes `"invalid diagnostic code: "` followed by the message (cut again to 4,096 bytes).
- The cook report entry of an import diagnostic (Formats.md "Cook report") is: `severity` `"info"`, `"warning"` or `"error"` from the level; `code` the diagnostic's code, or `"COOK_IMPORT_DIAGNOSTIC"` without one (Cook.md "Cook diagnostics", `uCOOK_DIAGNOSTIC_CODE_NAMED` and `uCOOK_DIAGNOSTIC_CODE_IMPORT`); `asset` the source's GUID; `path` the source's current Zenith2-relative path; `message` the stored message. An ERROR diagnostic does not fail the import by itself (`Import`'s result does) and does not change the cook's exit code.
- A diagnostic is part of the import's output: the same import-key inputs give the same diagnostics in the same order. A message may name the source's path (`GetSourcePath`); because the source path is not an import-key input, a source moved without other change and imported through a DDC hit replays a message naming the old path while the entry's `path` names the new one. A consumer matches `code`, never message text (PACK's stale-bake check, `Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"` step 1b, matches `WORLD_WARNING_BAKE_STALE` and `WORLD_WARNING_BAKE_MISSING`, the codes WORLD's importer reports, cross-area row WD35 (c)).

### Stored diagnostics

- The driver keeps every import's kept diagnostics as `Zenith_StoredImportDiagnostic` records (level, code, message) and its dropped count, and stores them, for a successful import that was not deferred, in two places, both in the blob format of Formats.md "Import diagnostics blob" (`ZIDG` version 1), byte-identical for the same import:
	1. the database record: `UpdateAfterImport` copies `m_axDiagnostics` and `m_uDiagnosticsDropped` from the `Zenith_ImportResult` into the record (`Zenith_AssetRecord::m_axImportDiagnostics`, `m_uImportDiagnosticsDropped`, Database.md "Records"), and `Save` writes them in the "import diagnostics" part of Formats.md "Asset database file";
	2. from Stage 4, the payload `asset.importDiagnostics` of the import's record entry in the DDC (DDC.md "Import entries"), so a cook on another machine, or after a database rebuild, that hits the entry has them too.
- A failed import (state ERROR) stores none: its diagnostics are reported live by the cook that ran it, together with its `COOK_ERROR_IMPORT_FAILED`, and the record's previous stored diagnostics are cleared. A deferred run stores none.
- An out-of-process import's worker writes the import record entry, payload included, into the local DDC itself (Formats.md "Worker messages": its `workResult` carries no diagnostics for an import item), and the driver reads the diagnostics from that entry.

### Replay into the cook report

1. After plan step 2 (Cook.md "Plan construction"), for every source record in state CURRENT whose import did not run in this cook (CURRENT after the scan, or imported through a verified DDC hit in step 2 of "Import algorithm"), the driver appends one cook diagnostic per stored diagnostic, in stored order, then the closing record when the stored dropped count n is above 0, each with `m_bReplayed` true, `m_xAsset` the source's GUID and `m_strPath` the record's current source path.
2. A source whose import ran in this cook contributes the same entries live (`m_bReplayed` false), from the diagnostics it is about to store.
3. The report sorts diagnostics as Formats.md "Cook report" states; two entries of one source that are equal in that order keep their stored order. A replayed entry is written with `"replayed": true`; a live one has no `replayed` member. The summary's `warnings` count includes replayed entries.
4. The cook server's `diagnostic` notifications carry live diagnostics only; the report a server `cook` request writes includes the replayed entries.
5. A source contributes at most 257 entries (256 kept and the closing record), live or replayed, so replay never exceeds the per-import cap.

The consequence (`Test_CookReport_ReplayedEqualsLive`): the reports of a clean cook, an incremental cook and a cook served from a DDC, over the same inputs, are equal once every `"replayed": true` member is removed.

### Import report members

`AddReportMember(strMember, xJsonValue)` contributes a member to the cook report's `members` exactly as `Zenith_CookContext::AddReportMember` does (CONTENT's `contentImports` array, cross-area row CT2): `strMember` matches `^[a-z][a-zA-Z0-9]{0,63}$`, `xJsonValue` must parse as one JSON value of at most 1 MiB (otherwise the call is recorded as an ERROR diagnostic without a code and dropped). The members of every import and every step are merged by name: one name contributed more than once becomes a JSON array of the values in (source GUID, call order), so `contentImports` holds one element per contribution; a name used both as an import member and as a step member is `COOK_ERROR_INVALID_STEP` at report time naming both. Report members are stored with the import's intermediates: in the intermediate area as `<intermediate root>/<32 hex import key>/report-members.json` (a JSON array of `{member, value}` objects in call order, written by `Zenith_JsonWriter` in compact style, at most 64 MiB) and, from Stage 4, in the import record entry's manifest (`reportMembers`, DDC.md "Import entries", which a hit materialises back into that file); they are replayed, like the stored diagnostics, for every CURRENT source whose import did not run, so a cook report's `members` are equal for a clean and an incremental cook.

## Cross-asset inputs

### Published-property inputs

`ReadPublishedProperty(xAsset, ePlatform, strName)` reads a property the cook step of another asset publishes with `Zenith_CookContext::PublishProperty` for one platform (WORLD reads GEOM's 48-byte `geom.lod0Summary` of each baked mesh on `ASSET_PLATFORM_WINDOWS`, cross-area rows WD30 and WD31, `Zenith2/Docs/Design/WORLD/Bakes.md → "Mesh summaries"`). The triple (asset, platform, name) is the input; the producer item is the asset's ASSET-phase cook item for that platform (Cook.md "Plan construction" step 3).

**The call.** It validates its arguments (a null GUID, a platform other than `ASSET_PLATFORM_WINDOWS` or `ASSET_PLATFORM_ANDROID`, a name failing `^[a-z][a-zA-Z0-9.]{0,63}$`, or a call in a worker process: `IMPORT_ERROR_INVALID_DESC`), then looks the triple up in the run's resolved-property table T, which the main thread fills between waves and the import threads only read during one:

| T holds | Answer | Recorded |
|---|---|---|
| the property's bytes | the bytes (valid until `Import` returns) | once per triple: a `Zenith_ImportPropertyInput` with the bytes' `Zenith_Hash128Of` and the producer item's key |
| a failure | `IMPORT_ERROR_INPUT_NOT_FOUND`, the failure's text reported at ERROR without a code | the triple with the all-zero hash, so a later change of the producer re-imports this source |
| nothing | `IMPORT_ERROR_INPUT_PENDING`; the triple is added to the run's requested set and the import is marked deferred | nothing |

A deferred import may keep reading further properties before it returns, which adds their triples to the same wave; an importer that reads many properties (WORLD's summaries) should do so, because each wave resolves every requested triple at once.

**Resolution of a triple** (the main thread, `Internal/Import/Zenith_ImportPropertyResolver.cpp`):

1. The database record of `xAsset` must be live with a kind (not TOMBSTONE, not MISSING_SOURCE, not `ASSET_KIND_NONE`): otherwise the failure "<guid> has no live asset record".
2. Its source record must be CURRENT in this run: a source still NEW or DIRTY leaves the triple unresolved until a later wave; a source in ERROR is the failure "<source path> failed to import".
3. The property sub-plan is the producer item and the closure of its cook-input and hard-reference upstream items for that platform (Database.md "Queries"), ordered and keyed by Cook.md "Plan construction" steps 4 and 5; a closure item whose source is not CURRENT leaves the triple unresolved.
4. The cook driver runs the sub-plan with publication disabled: each item is reused when the tree's own record proves it up to date (Cook.md "Keys and invalidation"), taken from a verified DDC hit, or cooked and then populated into the DDC; nothing is written into the output tree, the journal, the catalogue or the provenance lines. An item that fails is the failure "<step> failed for <guid> on <platform>: <COOK_ERROR_…>".
5. The producer item's published properties: no property named `strName` is the failure "<step> publishes no property <name>"; otherwise T gets the bytes, their `Zenith_Hash128Of` and the item's key.

So a property of the cook's own platform is cooked once per run (the plan's own item later has the same key and is reused from the DDC or the tree), and a property of another platform exists only in the DDC: an Android cook that reads the Windows `geom.lod0Summary` cooks the Windows mesh item into the DDC and never into its tree (row WD31 (a)).

**Waves.** `ImportPending` runs in waves over P, the NEW and DIRTY sources, and C, the CURRENT sources whose last import recorded property inputs (from Stage 4, the only sources with any):

1. Resolve, in ascending triple order (GUID bytes, platform, name bytes), every unresolved triple that a source of P or C recorded in its previous import or that a deferred run requested, whose resolution steps 1 to 3 allow now.
2. Move every source of C whose recorded triples are all resolved: to P (DIRTY, reimport trigger `REIMPORT_TRIGGER_PROPERTY_INPUT`) when any hash differs from its recorded one, else out of C (it stays CURRENT and is replayed).
3. Import, concurrently as "Import algorithm" states, every source of P whose recorded triples are all resolved (its key is computable); a run that is not deferred leaves P.
4. Stop when P and C are empty. A wave that resolves no triple and completes no import ends the loop: every source still in P fails with `IMPORT_ERROR_INPUT_NOT_FOUND` ("import cycle through published properties: <source path> needs <guid>/<platform>/<name>"), and every source still in C becomes DIRTY and fails the same way.

Product-list inputs ("Source product lists") take part in the same waves: a recorded or requested product list counts as a triple that resolves once its source is CURRENT in this run, and a CURRENT source whose recorded list hash differs moves to P with `REIMPORT_TRIGGER_PRODUCT_LIST_INPUT`. Each wave resolves at least one triple or completes at least one import, or it is the last, so the number of waves is at most |P| + |C| + the number of distinct triples + 1. A NEW world therefore imports twice (once to learn its triples, once with them), and an unchanged world in a later cook imports zero times after one wave of resolutions. `IMPORT_ERROR_INPUT_PENDING` never reaches the cook's boundary or the record.

**Key, edges and records.** Each property input is an import-key row (DDC.md "Key inputs": the producing GUID, the platform, the name and the bytes' Hash128, sorted by triple); the producer key is a cache hint for the resolver, never a key input, so a re-cook of the producer that leaves the property's bytes alone leaves the importing source's key unchanged (WORLD's lighting fingerprint survives a `uZENITH_ENGINE_COOK_VERSION` bump). `UpdateAfterImport` adds one `ASSET_EDGE_KIND_IMPORT_INPUT` edge from the source to each producing asset. The previous run's property inputs are kept in the intermediate area's `Inputs/<source GUID>.txt` after its path lines, one line each: `@property`, TAB, the 36-character GUID, TAB, the platform's script name (`windows`, `android`), TAB, the name, TAB, the 32-hex hash, TAB, the 32-hex producer key, LF, sorted by triple (a Zenith2-relative path never begins with `@`).

**The cook server.** The `property` request (Formats.md "Requests") answers the editor's question for a set of assets: the server scans and imports what the closures need, resolves each (GUID, platform, name) as above, in ascending GUID byte order with duplicates answered once, and pages `{guid, bytes}` items (standard base64 with padding) or `{guid, error}` items (`COOK_ERROR_MISSING_REFERENCE` for a resolution failure of step 1 or step 5, `COOK_ERROR_IMPORT_FAILED` for step 2, the failing item's error code for step 4, `COOK_ERROR_DEPENDENCY_CYCLE` for a triple still unresolved once a resolution pass resolves nothing more). WORLD's editor sends it with `name` `geom.lod0Summary` (`Zenith2/Docs/Design/WORLD/Editing.md → "World editor extension"`).

### Input documents

`OpenSourceDocument(xOut)`, `OpenInputDocument(strRelativePath, xOut)` and `OpenInputDocumentByGUID(xAsset, xOut)` open authoring documents migrated in memory, the import-side counterparts of `Zenith_CookContext::OpenSourceDocument` (cross-area row WD1):

| Member | Document | Recorded |
|---|---|---|
| `OpenSourceDocument` | the import's own source bytes (`ReadSource`) | nothing extra: the source bytes are already the key's first input |
| `OpenInputDocument` | a path relative to the source's directory, with `ReadInput`'s rules (an input-only document such as a region scene, Database.md "What is an asset source") | the path and the bytes' Hash128, as `ReadInput` records |
| `OpenInputDocumentByGUID` | the source document of the database record of `xAsset` (a sub-asset maps to its source), absent, TOMBSTONE or MISSING_SOURCE being `IMPORT_ERROR_INPUT_NOT_FOUND` | the source's Zenith2-relative path and the bytes' Hash128 |

Each reads the file whole (at most 64 MiB, `IMPORT_ERROR_LIMIT_EXCEEDED`), then migrates it exactly as step 6 of "Documents relative to another asset" does; a worker-process call is `IMPORT_ERROR_INVALID_DESC`, so an importer that calls them declares `m_bOutOfProcess` false. Opening one document twice in one import reuses the bytes and records it once.

### Source product lists

`ReadSourceProducts(xSource)` returns the products another source's current import declares, in key byte order (WORLD reads ENV's terrain tile, foliage, collision and navigation products, cross-area row WD2):

1. The record of `xSource` must be a live source record; otherwise `IMPORT_ERROR_INPUT_NOT_FOUND`.
2. When the source is NEW or DIRTY and not yet imported in this run, the call answers `IMPORT_ERROR_INPUT_PENDING` and the import is deferred to a later wave exactly as for a published property ("Published-property inputs", "Waves"); a source in ERROR is `IMPORT_ERROR_INPUT_NOT_FOUND`; a cycle (each of two sources reading the other's products) ends the waves with the import-cycle failure.
3. Otherwise the list is the source record's products (derived products included), and the input recorded once per source is `Zenith_ImportProductListInput { source GUID, Zenith_Hash128Of of the list's canonical text }`, the text being one line per product `key TAB 36-character GUID TAB kind script name TAB 0|1 (primary) LF`. The key rows are `productsSource` (GUID) and `productsHash` (hash), after the property rows (DDC.md "Key inputs"), and `Inputs/<GUID>.txt` keeps one line `@products TAB <source GUID> TAB <32-hex hash>` per input, so adding, removing or renaming one of the source's products re-imports this source and nothing else does.

The withdrawn halves of rows WD2 and WD21 (`ReadAssetImportKey`, `ReadCookItemKey`, the `cookKey` request and importer derived-data blobs) are not part of this file: WORLD reads published properties instead (row WD31 (d)).

### Documents relative to another asset

`OpenInputDocumentRelativeTo(xAnchor, strRelativePath, xOut)` opens an authoring document by a path relative to the directory of another asset's source (WORLD reads the tile documents a terrain's `tileFiles` lists, relative to the terrain document, row WD31 (b)):

1. `xOut.Reset()`. In a worker process the call is `IMPORT_ERROR_INVALID_DESC` ("<importer> opens input documents and cannot run out of process"); an importer that calls it declares `m_bOutOfProcess` false.
2. `strRelativePath` gets `ReadInput`'s syntactic checks (empty, absolute, a drive, a `\`, a `.` or `..` component: `IMPORT_ERROR_INPUT_OUTSIDE_ROOT`).
3. The anchor is the database record of `xAnchor`, or for a sub-asset its source record; absent, TOMBSTONE or MISSING_SOURCE is `IMPORT_ERROR_INPUT_NOT_FOUND` naming the GUID. The path is joined to the directory of the anchor's `m_strSourcePath`; a result outside every content root or under a skipped directory (ASSET.md "Content root mapping") is `IMPORT_ERROR_INPUT_OUTSIDE_ROOT`.
4. The file is read whole, at most 64 MiB (`IMPORT_ERROR_LIMIT_EXCEEDED` above it); a read that fails for any reason other than absence is `IMPORT_ERROR_INPUT_NOT_FOUND` with the operating-system error reported. An absent file returns found = false and records the path with the all-zero Hash128, so creating the file later re-imports the source.
5. The path is recorded once per import as an import input (its Zenith2-relative path and `Zenith_Hash128Of` of its bytes); a second open of the same path in one import reuses the bytes already read.
6. The bytes are migrated in memory exactly as `Zenith_CookContext::OpenSourceDocument` steps 5 to 7 do (Cook.md "Source documents in cook steps"): a call-local `Zenith_MemoryDocumentStorage` and `Zenith_DocumentMigrator` over the services' frozen migration and type registries, created on the calling thread and destroyed before the call returns; an open error is `IMPORT_ERROR_SOURCE_INVALID` with the `Zenith_DocumentErrorCode` reported (`IMPORT_ERROR_OUT_OF_MEMORY` for `DOCUMENT_ERROR_OUT_OF_MEMORY`); success is returned in every open state, `DOCUMENT_OPEN_READ_ONLY_NEWER` included, and the importer decides what a newer document means.

The anchor's own source is not an input of this call (an importer that depends on it opens it as well); a move of the anchor changes the joined path and therefore the input set, which re-imports the source.

## Intermediates

- An intermediate is a set of named blobs per product, platform-independent (CONTENT's intermediate mesh, decoded image, decoded audio), stored as one DDC entry per product keyed by (import key, product GUID).
- Intermediates are the inputs of cook steps (Cook.md "Cook steps"); a cook step never reads a source file directly, except an authoring document of the plan's record snapshot through `Zenith_CookContext::OpenSourceDocument`, verified against the scan's source hash (Cook.md "Source documents in cook steps").
- Their formats are the importer owner's (CONTENT, GEOM for the intermediate mesh consumed by its step), versioned by the importer version.

## Reimport

### Reimport triggers

A source is reimported when any import-key input changes: its bytes, any declared input's bytes (a document opened through `OpenInputDocumentRelativeTo` included, and its appearance where it was absent), the bytes of a published property it read ("Published-property inputs"), its resolved settings (the `.meta`, the preset, a platform override), the importer's version or settings version, a pinned tool version it declares. Scan detects each (Database.md "Scan algorithm"); the editor's reimport command forces a run even when the key matches (it bypasses the DDC hit, not the key).

### Identity preservation on reimport

1. The importer declares products by key; the driver matches each against `.meta` `subAssets` by exact key: a match keeps its GUID.
2. An unmatched new key gets a new GUID; an old key not produced becomes `orphaned` (kept, GUID reserved).
3. When an importer renames a product it can recognise (the same source node identity with a new name), it declares the new key with `DeclareProduct(newKey, kind, primary)` after calling `Zenith_ImportContext::RenameProduct(oldKey, newKey)`, which moves the GUID to the new key; the rules for recognising renames are the importer's (CONTENT).
4. Authored overrides that live in other documents (prefab overrides, material assignments) survive because they reference GUIDs, which step 1 keeps; how an importer merges authored changes inside its own `.meta` settings is CONTENT's (Z2-I-CONTENT-15).

5. Every member of an entry travels with its GUID: reconciliation keeps all of an entry's members on reimport, moves them with the GUID on `RenameProduct` and on the server's `remapSubAssets`, and keeps them when the entry is orphaned (cross-area row CT22 (b)); only `fingerprint` is rewritten, from `SetProductFingerprint`.

`Test_Import_ReimportKeepsSubAssetGuids` reimports a fixture glTF after changing its bytes and asserts every surviving key's GUID is unchanged, a removed key is orphaned, and a new key is new.

### Product members and fingerprints

- `GetPreviousProducts()` returns the source's `.meta` `subAssets` entries as they stood when the import began, in key byte order, orphaned entries included, each with its fingerprint and its owner members as canonical compact JSON; an importer recognises renames from them (CONTENT's and ANIM's rule: a GUID moves to a new key only when the fingerprints are equal, `Zenith2/Docs/Design/CONTENT/PresetsAndReimport.md → "Product fingerprints"`). The span is the import's own copy, valid until `Import` returns, and identical in a worker process (the work item carries the `.meta`).
- `SetProductFingerprint(xProduct, ulFingerprint)` after `DeclareProduct` or `DeclareDerivedProduct` of `xProduct`: a GUID this import did not declare is `IMPORT_ERROR_INVALID_DESC`; a second call for one product replaces the value. In sidecar-writing modes reconciliation writes it into the entry as `fingerprint`, 16 lowercase hex digits of the `uint64`, most significant first (cross-area rows CT22 (d), AnB23); in the build's cook it is kept in the import result and the import record entry only, and an entry whose recorded `fingerprint` differs is not rewritten (a build cook never writes a source). A product with no `SetProductFingerprint` call keeps no `fingerprint` member.
- The fingerprint never reaches a key: two imports that differ only in fingerprints have equal intermediates and keys.
- A sub-asset entry's GUID found in a tombstone list is accepted as Database.md "Duplicate GUIDs" accepts a `.meta`'s (row CT22 (f)).

### Derived products

`DeclareDerivedProduct(strKey, eKind, xGUID, bPrimary)` declares a product whose GUID the importer computes itself (WORLD's `Zenith_WorldDeriveProductGUID`, a SHA-256 truncation, for cell products that must be stable across machines without thousands of `.meta` rows, cross-area row WD4):

1. `strKey` follows the sub-asset key grammar; `xGUID` is non-null; a key or GUID already declared by this import, present in the `.meta`'s `subAssets`, held by another record of the database or tombstoned is `IMPORT_ERROR_DUPLICATE_PRODUCT_KEY` naming the other holder.
2. A derived product is never written into `subAssets`, in any mode, and the build's cook never fails `COOK_ERROR_META_SUBASSET_MISSING` for it; the database records it as a sub-asset record with `m_strSubAssetKey` the key and the derived GUID, and the import result marks it in `m_abDerived`.
3. A derived product the next import no longer declares is deleted from the database at that import's `UpdateAfterImport`: it is never orphaned and its GUID is never reserved, because the importer derives the same GUID again for the same identity.
4. A derived GUID colliding with any GUID of another source on a later scan is `COOK_ERROR_DUPLICATE_GUID` naming both, as for any GUID.

### Editor reimport

The editor requests `reimport` (one or more GUIDs) through the cook server; the server scans the affected sources, imports, cooks the affected assets for the editor's platform, publishes them into the loose tree and returns the changed GUIDs, which the editor passes to its play worlds' registries (`OnMountsChanged`, then `RequestReload` for each ACTIVE asset, `Zenith2/Docs/Design/ASSET/Loading.md → "Development reload"`). With watching enabled, the server does the same unasked after a source changes on disk and sends `changed` and `published` notifications (`Zenith2/Docs/Design/ASSET/Formats.md → "Notifications"`). The time from the editor's command to the reload's activation is measured against Z2-B-EDIT-004.

## Import errors

```cpp
// Zenith2/Tools/ZenithCook/Internal/Import/Zenith_Importer.h
/// @contract enum Zenith_ImportError
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; at the cook's boundary each is COOK_ERROR_IMPORT_FAILED with this value in the detail.
/// Version: since Z2 Stage 2. Tool-side; values are appended, never renumbered.
/// @endcontract
enum Zenith_ImportError : uint8
{
	IMPORT_ERROR_INVALID_DESC = 1,
	IMPORT_ERROR_UNKNOWN_IMPORTER,
	IMPORT_ERROR_IMPORTER_MISMATCH,
	IMPORT_ERROR_SETTING_INVALID,
	IMPORT_ERROR_SOURCE_INVALID,
	IMPORT_ERROR_INPUT_NOT_FOUND,
	IMPORT_ERROR_INPUT_OUTSIDE_ROOT,
	IMPORT_ERROR_DUPLICATE_PRODUCT_KEY,
	IMPORT_ERROR_LIMIT_EXCEEDED,                 // parser limits of Zenith2/Docs/Architecture.md "Security architecture"
	IMPORT_ERROR_WORKER_CRASHED,
	IMPORT_ERROR_WORKER_TIMEOUT,
	IMPORT_ERROR_OUT_OF_MEMORY,
	IMPORT_ERROR_INPUT_PENDING,               // a cross-asset input (a published property, another source's product list) not yet available in this run (13; "Published-property inputs", "Source product lists")
};
constexpr const char* Zenith_ToString(Zenith_ImportError eError);
```

## Boundary with CONTENT importers

CONTENT implements `Zenith_Importer` for glTF 2.0 (fastgltf), images, audio and, per D-SEL-FBX (open, Stage 4), FBX through a conversion path; decides `m_bOutOfProcess` (true for every third-party parser, Z2-I-CONTENT-22); defines product keys and rename recognition; owns presets and override merging. ASSET owns the interface, the registry, settings resolution, keys, identity and the worker mechanism, and its tests use its passthrough importers and a scripted fake importer (`Tests/Import/Zenith_FakeImporter.h`).
