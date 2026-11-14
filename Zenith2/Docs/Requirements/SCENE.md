# SCENE requirement catalogue — Scenes & prefabs

The requirements of the SCENE area (Scenes & prefabs): stable authoring identities, scene and prefab documents with canonical serialization, nested prefabs, overrides and variants, reference repair, the authoring-document migration framework, transactional loading, additive scenes, scene transitions, transform propagation and missing-type handling. The design that specifies them is `Zenith2/Docs/Design/SCENE.md` with its split files under `Zenith2/Docs/Design/SCENE/`.

## Purpose

This catalogue breaks every SCENE inventory item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" ("SCENE Scenes & prefabs") into testable requirements, together with the CONTENT item Z2-I-CONTENT-28 (canonical serialization, "a Stage 2 SCENE requirement") and the document-level half of the migration framework named by Z2-I-ECS-03 (ECS: schema evolution; the framework is delivered by SCENE and ASSET per `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind"). Each requirement has exactly one owner, this area; a requirement that relies on another area names it in `Zenith2/Docs/Design/SCENE.md` → "Dependencies". Stage is the stage whose acceptance the requirement serves; a ticket may be pulled forward into an earlier sprint (`Zenith2/Docs/Roadmap.md` → "Pull-forward rule"). The catalogue is authoritative for coverage (Z2-D-067). The catalogue is two files with one numbering: this file holds the purpose, the requirements Z2-R-SCENE-001 to -165, the coverage of inventory items (over both files) and the exclusions; `Zenith2/Docs/Requirements/SCENE/Stage4Plus.md` holds Z2-R-SCENE-166 to -182, the Stage 4 requirements of the module seams (`Zenith2/Docs/Design/SCENE/ModuleSeams.md`), added by the post-signoff patch of 2026-10-09. Requirements that cover another area's inventory item (Z2-I-SAVE-01, Z2-I-STREAM-05, Z2-I-PHYS-28, Z2-I-ENV-05) own SCENE's half of a cross-area item that rows SV4, ST3, PH20 and EN26 place in SCENE; the other area owns the rest of that item.

## Requirements

### Z2-R-SCENE-001 ZenithScene library target

- **Statement:** The library `ZenithScene` exists at `Zenith2/Engine/ZenithScene/`, declares `ZENITH_SUPPORTED` for every preset, its `DEPENDS` list is exactly the row of `Zenith2/Docs/Architecture.md` → "Library dependency graph" plus `ZenithDocument` and `ZenithCompression`, and its `TEST_DEPENDS` list is exactly the three ECS backends, linked into `ZenithScene_Tests` alone.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` gate step (declared edges equal real use; `testDepends` recorded in `declared_deps.json`) and `CompileCoverage` over every preset
- **Design:** Zenith2/Docs/Design/SCENE.md → "ZenithScene target"; Zenith2/Docs/Design/SCENE.md → "Libraries SCENE depends on"

### Z2-R-SCENE-002 ZenithDocument library target

- **Statement:** The library `ZenithDocument` exists at `Zenith2/Engine/ZenithDocument/` at layer 2 with `DEPENDS ZenithCore ZenithReflect ZenithProfiling`, supported on every preset, and performs no host file I/O (every read and write goes through `Zenith_DocumentStorage`).
- **Covers:** Z2-I-SCENE-10, Z2-I-CONTENT-28, Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` gate step and `ForbiddenCheck` F18 (no platform header in ZenithDocument)
- **Design:** Zenith2/Docs/Design/SCENE.md → "ZenithDocument target"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Why a separate library"

### Z2-R-SCENE-003 ZenithDocumentHost tool library

- **Statement:** The tool-side static library `ZenithDocumentHost` at `Zenith2/Tools/DocumentHost/`, declared `zenith_add_tool(ZenithDocumentHost DEPENDS ZenithDocument ZenithCore SUPPORTED ZENITH_HOST)` with no `Internal/Zenith_ZenithDocumentHostMain.cpp`, implements `Zenith_DocumentStorage` over the host filesystem on `win`, `win-asan` and `linux-tsan`, and no Player links it in any configuration.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` Shipping link-closure rule and `Test_HostDocumentStorage_WriteReplaceRead` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE.md → "ZenithDocumentHost target"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "ZenithDocumentHost"

### Z2-R-SCENE-004 Contract blocks on every SCENE header

- **Statement:** Every public type of ZenithScene, ZenithDocument and ZenithDocumentHost carries one contract block.
- **Covers:** Z2-I-SCENE-01, Z2-I-SCENE-08, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the `ApiContractCheck` and `HeaderSelfContained` gate steps
- **Design:** Zenith2/Docs/Design/SCENE.md → "API rules"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Header list"

### Z2-R-SCENE-005 Upward notifications through SCENE-declared interfaces

- **Statement:** SCENE notifies higher libraries only through interfaces it declares (`Zenith_LifecycleListener`, `Zenith_ScenePublicationSink`, `Zenith_SceneTransitionSink`, `Zenith_PrefabProvider`, `Zenith_DocumentStorage`) and stores no pointer into a higher library otherwise.
- **Covers:** Z2-I-SCENE-07, Z2-I-SCENE-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` gate step (no upward edge) and review of each registration
- **Design:** Zenith2/Docs/Design/SCENE.md → "Upward-callback interfaces SCENE declares"

### Z2-R-SCENE-006 Scene errors are records numbered from 1

- **Statement:** `Zenith_SceneError` and `Zenith_DocumentError` are error records of at most 32 bytes whose codes are numbered from 1 with a `Zenith_ToString` overload, and no SCENE or ZenithDocument path asserts on document bytes, cooked bytes or script input.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooked_CorruptInputs_AreRejected`, `Test_JournaledWrite_CorruptJournal_IsRefused` and the fuzz smokes `Zenith_Fuzz_SceneCooked`, `Zenith_Fuzz_SceneDocument` and `Zenith_Fuzz_DocumentJournal` (`Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneErrors header"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentErrors header"; Zenith2/Docs/Design/SCENE.md → "Error enumerations"

### Z2-R-SCENE-007 Authoring IDs are persistent IDs

- **Statement:** Every document, authored entity and prefab instance carries a `Zenith_PersistentID` authoring ID in its canonical 36-character form, generated at creation and never changed while the object exists.
- **Covers:** Z2-I-SCENE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneEdit_IdsStableAcrossEdits` and `Test_SceneDocument_RoundTripCanonicalBytes` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Authoring identities"; Zenith2/Docs/Design/SCENE/Format.md → "Scene document schema"

### Z2-R-SCENE-008 Authoring IDs are never reused within a document

- **Statement:** An ID deleted from a document during a session cannot be given to a new object of that document, and an ID appearing twice across entities, instances and the document ID fails loading with `SCENE_ERROR_DUPLICATE_ID`.
- **Covers:** Z2-I-SCENE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_RetiredIdCannotBeReused` and `Test_SceneDocument_Validation_ReportsEveryViolationOfAPass` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Identity rules the editor foundation builds on"; Zenith2/Docs/Design/SCENE/Format.md → "Document validation"

### Z2-R-SCENE-009 Deterministic ID derivation for instanced entities

- **Statement:** `Zenith_DeriveAuthoringID` computes the ID of an entity created by a prefab instance from the instance ID and the prefab-local ID as the first 16 bytes of SHA-256 over `"ZSID"`, the scope and the local ID with the version-4 bits forced, identical on every platform and run and accepting a null scope.
- **Covers:** Z2-I-SCENE-01, Z2-I-SCENE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AuthoringID_DeriveIsDeterministicAndVersion4` and `Test_AuthoringID_DeriveIndependentOfHash64` with the committed vectors (Test step, every preset)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_AuthoringID header"

### Z2-R-SCENE-010 Derived ID collisions are reported

- **Statement:** A resolution that produces the same ID twice fails with `SCENE_ERROR_DUPLICATE_ID` naming both sources.
- **Covers:** Z2-I-SCENE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AuthoringID_DerivedCollisionReported` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"

### Z2-R-SCENE-011 Copy and paste remaps IDs

- **Statement:** `Zenith_RemapAuthoringIDs` gives every copied entity and instance a fresh ID and rewrites the parents, ENTITY-typed values and override targets inside the copied set, keeping references outside it.
- **Covers:** Z2-I-SCENE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AuthoringID_RemapRewritesInternalReferences` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Remapping"

### Z2-R-SCENE-012 Constant-time lookup by authoring ID

- **Statement:** `Zenith_SceneDocument::FindEntity` and `FindInstance` resolve an ID through a hash index, so EDIT's command system addresses objects by ID alone.
- **Covers:** Z2-I-SCENE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneEdit_IdsStableAcrossEdits` asserting lookups after every edit (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Identity rules the editor foundation builds on"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneDocument header"

### Z2-R-SCENE-013 Document edits return their inverse

- **Statement:** Every `Zenith_SceneEdit` kind applied through `Zenith_ApplySceneEdit` returns an inverse edit whose application restores the document's canonical bytes exactly, and a failing edit leaves the document unchanged.
- **Covers:** Z2-I-SCENE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneEdit_InverseRestoresBytes` and `Test_SceneEdit_ReparentCycle_IsRejected` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Document edits for the editor"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneEdit header"

### Z2-R-SCENE-014 Play-world entities map back to document objects

- **Statement:** Capturing an edit world for play and activating the capture keeps every entity's persistent ID, so a play-world entity maps back to its document object.
- **Covers:** Z2-I-SCENE-01, Z2-I-SCENE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCapture_PlayCopyRoundTrip` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Identity rules the editor foundation builds on"; Zenith2/Docs/Design/SCENE/Loading.md → "Carry-over"

### Z2-R-SCENE-015 Canonical byte format

- **Statement:** Canonical scene and prefab bytes use the CORE pretty byte format: UTF-8 without a byte-order mark, LF line endings, one tab per depth and one trailing LF.
- **Covers:** Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_RoundTripCanonicalBytes` over the golden fixtures (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"; Zenith2/Docs/Design/SCENE/Format.md → "Scene canonical schema"

### Z2-R-SCENE-016 Stable key order

- **Statement:** Known members of every object are written in the order of the document type's canonical schema regardless of input order.
- **Covers:** Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CanonicalJson_KnownKeysInRuleOrder` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical writing algorithm"

### Z2-R-SCENE-017 One ID-keyed child per line

- **Statement:** Every collection of identified children is an object keyed by persistent ID, sorted by key byte order, each member starting on its own line; a non-ID key in such an object is a shape error.
- **Covers:** Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CanonicalJson_IdMapSortedOneMemberPerLine`, `Test_CanonicalJson_IdMapNonIdKey_IsShapeError` and `Test_CanonicalJson_LineMergeOfDisjointEntityAdds` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"; Zenith2/Docs/Design/SCENE/Format.md → "Scene canonical schema"

### Z2-R-SCENE-018 Field sets in numeric property-ID order

- **Statement:** Component field sets are written keyed by decimal property ID in ascending numeric order, with vectors, quaternions and colours as one-line arrays.
- **Covers:** Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CanonicalJson_FieldSetNumericOrder` and `Test_CanonicalJson_InlineArraysForVectors` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"

### Z2-R-SCENE-019 Canonical numbers

- **Statement:** Known float properties are written as the shortest round-trip text and unknown members keep their raw number text.
- **Covers:** Z2-I-CONTENT-28, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CanonicalJson_ShortestFloatText` and `Test_CanonicalJson_UnknownRawNumbersPreserved` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"

### Z2-R-SCENE-020 Canonical writing is idempotent

- **Statement:** For every accepted document, writing the canonical form of a parse of the canonical form gives identical bytes.
- **Covers:** Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CanonicalJson_Idempotent`, a property test over 10,000 generated documents with its seed printed (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"

### Z2-R-SCENE-021 Sibling order through sort keys

- **Statement:** Author-defined sibling order is carried by each entity's and instance's fractional-index `sortKey`, so inserting a sibling rewrites only the inserted object's lines, and two siblings with equal keys are ordered by ID and reported once per sibling group with `DOCUMENT_WARNING_DUPLICATE_SORT_KEY`, never with `DOCUMENT_WARNING_NOT_CANONICAL`, and are not rewritten at load or save.
- **Covers:** Z2-I-CONTENT-28, Z2-I-SCENE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_RoundTripCanonicalBytes` with the sort-key cases of `Hierarchy6.scene.json`, `Test_SceneDocument_DuplicateSortKeys_WarnAndOrderById` and `Test_SceneDocument_InvalidSortKey_IsSchemaError` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Sort keys"

### Z2-R-SCENE-022 Canonical writer failure leaves output unchanged

- **Statement:** A failing canonical write returns `DOCUMENT_ERROR_WRITE` or `DOCUMENT_ERROR_SHAPE` and leaves the output array as it was.
- **Covers:** Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CanonicalJson_WriterFailure_LeavesOutputUnchanged` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical writing algorithm"

### Z2-R-SCENE-023 Scene document schema version 1

- **Statement:** A scene document carries `$schema` `zenith2.scene`, `schemaVersion` 1, `id`, `name`, `entities` and `instances` exactly as the root-object table specifies, and every other member is an unknown member.
- **Covers:** Z2-I-SCENE-01, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_RoundTripCanonicalBytes` and the fixture `v1.json` of `Test_SceneDocumentMigration_Fixtures` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Root object"; Zenith2/Docs/Design/SCENE/Format.md → "Entity object"; Zenith2/Docs/Design/SCENE/Format.md → "Component object"

### Z2-R-SCENE-024 Prefab document schema version 1

- **Statement:** A prefab document carries `$schema` `zenith2.prefab`, `schemaVersion` 1, `id`, `name`, `root`, `variantOf`, `base` (variants only), `entities` and `instances`, with exactly one root.
- **Covers:** Z2-I-SCENE-02, Z2-I-SCENE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabVariant_DifferentRoot_IsSchemaError` and the PrefabDocument fixtures (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Prefab document schema"

### Z2-R-SCENE-025 Derived components are never authored

- **Statement:** A document entity listing `Zenith.WorldTransform` or `Zenith.PreviousWorldTransform` fails loading with `SCENE_ERROR_SCHEMA` naming the entity.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_DerivedComponentAuthored_IsSchemaError` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Document validation"; Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"

### Z2-R-SCENE-026 Document validation reports every violation of a pass

- **Statement:** Document validation runs its five passes in order and reports every violation of a pass, each with path, line, column, key path and its error or warning code (`Zenith_SceneDiagnostics::IsError` and `GetCode`), before failing.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_Validation_ReportsEveryViolationOfAPass`, `Test_SceneDocument_DuplicateSortKeys_WarnAndOrderById` and `Test_SceneDocument_UnknownComponent_RoundTripsByteIdentical` asserting codes (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Document validation"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneData header"

### Z2-R-SCENE-027 Document limits

- **Statement:** Loading enforces the limits of the table "Document limits" (64 MiB, 1,048,576 entities, 65,536 instances, 64 components per entity, hierarchy depth 64, nesting 16, variant chain 16) with recoverable errors.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_LimitsEnforced` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Document limits"

### Z2-R-SCENE-028 Document references by ID never by path

- **Statement:** Prefab instances and variants name their prefab by document ID, and the `hint` path is informative only, so renaming or moving a document breaks no reference.
- **Covers:** Z2-I-SCENE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceRepair_MoveUpdatesHintsOnly` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Files and virtual paths"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"

### Z2-R-SCENE-029 Unknown components are preserved opaque

- **Statement:** A component whose stable name is not registered is kept as an opaque subtree, never decoded or migrated, and written back byte for byte.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_UnknownComponent_RoundTripsByteIdentical` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Unknown components"

### Z2-R-SCENE-030 Unknown components warn once per document and name

- **Statement:** Loading a document with an unknown component reports `DOCUMENT_WARNING_UNKNOWN_COMPONENT` once per (document, stable name) with the count of entities carrying it.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_UnknownComponent_RoundTripsByteIdentical` asserting the warning count (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Unknown components"

### Z2-R-SCENE-031 Unknown fields of known components round-trip

- **Statement:** An unknown property ID in a known component's field set is kept, warned with `DOCUMENT_WARNING_UNKNOWN_FIELD` and re-emitted in position.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_UnknownFieldInKnownComponent_RoundTrips` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Missing-type handling at load"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"

### Z2-R-SCENE-032 Unknown components are a cook error

- **Statement:** The cook fails with `SCENE_ERROR_UNKNOWN_TYPE` naming document, entity and stable name when a scene or prefab carries a component type absent from the game's registry.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooker_UnknownType_IsCookError` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Cooking resolved prefabs"; Zenith2/Docs/Design/SCENE/Loading.md → "Missing-type handling at load"

### Z2-R-SCENE-033 Runtime unknown-type policy

- **Statement:** Preparation of cooked data with an unknown type fails under `UNKNOWN_TYPE_POLICY_FAIL` (the default and the startup scene's policy) and, under `UNKNOWN_TYPE_POLICY_SKIP`, creates no component, keeps its field set and counts `scene.load.unknown_components`.
- **Covers:** Z2-I-SCENE-10, Z2-I-SCENE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_StartupUnknownType_Fails` and `Test_SceneLoader_SkipPolicyKeepsUnknownData` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Missing-type handling at load"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneLoader header"

### Z2-R-SCENE-034 Unknown components survive prefab instancing

- **Statement:** An unknown component on a prefab entity is carried unchanged through resolution, and its ENTITY references are not remapped, which the warning states.
- **Covers:** Z2-I-SCENE-10, Z2-I-SCENE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_UnknownComponentCarried` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Unknown components"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Re-identification"

### Z2-R-SCENE-035 Document types registered with version and chain

- **Statement:** Every authoring-document type handled by the framework is registered with a stable name, a type ID from it, a current `uint32` version, its suffixes and its canonical schema, and a duplicate registration is refused, and a suffix matches a storage path when `"/"` followed by the path ends with it, so the exact-name suffix `"/game.json"` matches the root document `game.json`.
- **Covers:** Z2-I-ECS-03, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_MigrationRegistry_DuplicateRegistration_IsRefused` and `Test_MigrationRegistry_SuffixMatchesRootAndNested` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"

### Z2-R-SCENE-036 Contracts step registration signature

- **Statement:** `Zenith_RegisterMigrationStep(Zenith_MigrationRegistry&, Zenith_TypeID, uint32, Zenith_PfnMigrationStep)` registers one step per (type, from-version) exactly as Contracts declares it and refuses invalid or post-freeze registrations.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_MigrationRegistry_StepAfterFreeze_IsRefused` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"

### Z2-R-SCENE-037 Chain gaps fail at tool start

- **Statement:** `Zenith_MigrationRegistry::Freeze` fails with `DOCUMENT_ERROR_CHAIN_GAP` naming the type and the missing version when any step between 1 and the current version is absent.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_MigrationRegistry_ChainGap_FailsFreezeNamingVersion` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Running a chain"

### Z2-R-SCENE-038 Older documents migrate through the chain in memory

- **Statement:** Opening a document older than its type runs one step per version in ascending order on the in-memory tree, then the component chains, without writing storage.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_OpenOlder_RunsChainInOrder` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Opening a document"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Running a chain"

### Z2-R-SCENE-039 Newer documents open read-only

- **Statement:** A document whose version, or one of whose components' versions, is newer than the tool's opens read-only with a diagnostic and is never written by that tool.
- **Covers:** Z2-I-ECS-03, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_OpenNewer_IsReadOnly` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Opening a document"

### Z2-R-SCENE-040 Failed migration leaves storage unchanged

- **Statement:** A failing step reports `DOCUMENT_ERROR_STEP_FAILED` and the stored bytes are untouched.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_StepFailure_LeavesStorageUnchanged` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Running a chain"

### Z2-R-SCENE-041 Upgrade preview is a dry run

- **Statement:** `PlanMigration` produces, for every document, its versions, new canonical bytes, a line diff and a structural path summary, and performs no storage mutation.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_Plan_WritesNothing` asserting a mutation count of 0 (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Planning a migration"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentDiff header"

### Z2-R-SCENE-042 Non-deterministic steps are blocked

- **Statement:** Planning migrates each document twice and blocks an entry with `DOCUMENT_ERROR_NOT_DETERMINISTIC` when the outputs differ, so the preview equals the write.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_Plan_NonDeterministicStep_IsBlocked` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Planning a migration"

### Z2-R-SCENE-043 Preview report for the migration UI

- **Statement:** `Zenith_MigrationPlan::WriteReportJson` writes the `zenith2.migration-preview` version 1 report that EDIT's migration UI and `ZenithNewGame --migrate --preview` display.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_Plan_WritesNothing` comparing the report against a golden file (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Planning a migration"; Zenith2/Docs/Design/SCENE.md → "What SCENE exposes to EDIT"

### Z2-R-SCENE-044 Automatic backup before any write

- **Statement:** Applying a migration copies every changed document to `.migration/backups/<UTC timestamp>/<path>` before the first document is written.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_Apply_BacksUpBeforeWriting` asserting the operation order (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Applying a plan"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Backups"

### Z2-R-SCENE-045 Stale plans are refused

- **Statement:** Applying a plan or saving documents whose on-disk digest differs from the expected one fails with `DOCUMENT_ERROR_STALE_PLAN` and writes nothing, so an external edit is never overwritten.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_Apply_StalePlan_IsRefused` and `Test_DocumentMigrator_Save_ExternalEditNotOverwritten` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Applying a plan"

### Z2-R-SCENE-046 Journaled atomic write

- **Statement:** Every authoring-document write the framework performs follows Contracts' five journal steps with the journal at the storage root as `<batch>.z2journal` (the placement cross-doc item X109(d) adopts), listing each entry's backup path, rewritten atomically at each state change.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_JournaledWrite_InterruptedAtEachStep_Recovers` and `Test_JournaledWrite_JournalAtStorageRoot` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journal format"

### Z2-R-SCENE-047 Interrupted writes recover all-or-nothing

- **Statement:** After an interruption at any journal step or rename, recovery leaves every document of the batch fully old or fully new, and a repeated recovery changes nothing.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_JournaledWrite_InterruptedAtEachStep_Recovers`, `Test_JournaledWrite_InterruptedDuringRecovery_Converges` and `Test_HostDocumentStorage_JournalRecoveryOnDisk` (Test step, `z2-stress`)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"

### Z2-R-SCENE-048 Pending journals block writes

- **Statement:** While an unrecovered journal exists in the storage root, every framework write returns `DOCUMENT_ERROR_JOURNAL_PENDING`, and a corrupt journal or a document matching neither recorded digest is reported without changing anything.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_PendingJournal_BlocksWrites`, `Test_JournaledWrite_CorruptJournal_IsRefused` and `Test_JournaledWrite_RecoveryConflict_IsReported` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"

### Z2-R-SCENE-049 Save backups are retained within a limit

- **Statement:** Ordinary saves back up to `.migration/backups/save-<timestamp>/` and keep the latest `m_uSaveBackupRetention` (20) save batches, never pruning a migration batch.
- **Covers:** Z2-I-ECS-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_Save_BackupRetention` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Applying a plan"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Backups"

### Z2-R-SCENE-050 Unknown fields preserved through migration

- **Statement:** Every JSON member the reader does not recognise is kept with its path and raw value through open, migration and write, produces one `MIGRATION_WARNING_UNKNOWN_FIELD` per document per field path per load, and is never deleted by a step.
- **Covers:** Z2-I-SCENE-10, Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_UnknownFields_WarnOncePerPathAndRoundTrip`, `Test_DocumentMigrator_StepCannotDeleteUnknownField` and `Test_DocumentMigrator_DeclaredRemovalIsAllowed` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Opening a document"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"

### Z2-R-SCENE-051 Overrides preserved through migration

- **Statement:** A step that renames or restructures a property rewrites every override keyed to it (`Zenith_RewriteOverridePaths`), and an override whose target no longer exists is kept, flagged in the preview and reported.
- **Covers:** Z2-I-SCENE-03, Z2-I-ECS-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocumentMigration_RenameRewritesOverrides` and `Test_PrefabResolve_DanglingOverrideKeptAndReported` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Override preservation in migration steps"

### Z2-R-SCENE-052 Component chains run inside documents

- **Statement:** Opening a scene or prefab document migrates every embedded component field set older than its type through ECS's chain, including those in override blocks.
- **Covers:** Z2-I-ECS-03, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocumentMigration_ComponentChainAppliedInDocument` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneDocumentTypes header"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Opening a document"

### Z2-R-SCENE-053 Migration fixtures per version per SCENE type

- **Statement:** SceneDocument and PrefabDocument each have `v<N>.json` and `v<N>.expected.json` for every version from 1, migrated and compared byte for byte, and a version bump without its pair fails the test.
- **Covers:** Z2-I-ECS-03, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocumentMigration_Fixtures` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Migration fixtures"

### Z2-R-SCENE-054 Framework fixtures for a multi-version type

- **Statement:** The framework's synthetic `Test.Doc` type at versions 1 to 3 has fixtures that migrate to their expected outputs byte for byte.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentFramework_TestDocFixtures` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Migration fixtures"

### Z2-R-SCENE-055 Document paths are validated

- **Statement:** Every storage implementation validates each path with `Zenith_ValidateStoragePath` (rules P1 to P5, admitting the framework's journal, temporary and backup names), the migrator validates every caller-supplied document path with `Zenith_ValidateDocumentPath` (P1 to P6) and case-fold uniqueness within a batch, and a host path is formed only inside ZenithDocumentHost's `Internal/` with an escape check against the root.
- **Covers:** Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentPath_Rules` and `Test_HostDocumentStorage_EscapingLink_IsRefused` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Document paths"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "ZenithDocumentHost"

### Z2-R-SCENE-056 Framework digests use CORE SHA-256

- **Statement:** Every digest of the framework (document, plan, write and journal digests, batch names, path document IDs) and every authoring-ID derivation uses CORE's FIPS 180-4 SHA-256 (`Zenith_Sha256Digest`, `Zenith_Sha256Stream` and `Zenith_Sha256Value` of `ZenithCore/Zenith_Sha256.h`), and neither ZenithDocument nor ZenithScene declares a SHA-256 of its own.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AuthoringID_DeriveIndependentOfHash64` over vectors generated with `hashlib.sha256` and `Test_JournaledWrite_InterruptedAtEachStep_Recovers` over journal digests (Test step); CORE's `Test_Sha256_NistVectors` and `Test_Sha256_StreamEqualsOneShot` cover the functions themselves
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Digests"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journal format"

### Z2-R-SCENE-057 Framework factories unwind on failure

- **Statement:** Every construction step of `Zenith_DocumentMigrator::Create`, `Zenith_MigrationRegistry::Create` and `Zenith_MemoryDocumentStorage::Create` has a failure-injection test asserting the error and zero leaks.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_Create_FailsAt<Step>_Unwinds` and `Test_MemoryDocumentStorage_Create_FailsAtAllocation_Unwinds` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentMigrator header"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentStorage header"

### Z2-R-SCENE-058 Complete migration coverage with the editor

- **Statement:** Every authoring-document type of Contracts' list is registered with the framework in the editor and cook with a fixture per version, and EDIT's migration UI drives preview, backup and resume through `Zenith_DocumentMigrator`.
- **Covers:** Z2-I-ECS-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the Stage 4 acceptance ticket's migration-coverage run over every registered type (acceptance suite) and EDIT's UI tests
- **Design:** Zenith2/Docs/Design/SCENE.md → "Stage 4"; Zenith2/Docs/Design/SCENE.md → "Migration hooks"

### Z2-R-SCENE-059 Compatibility demonstrated with migration fixtures

- **Statement:** SCENE's migration fixtures of every shipped version are part of the Stage 6 compatibility demonstration.
- **Covers:** Z2-I-ECS-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the Stage 6 acceptance ticket's compatibility-policy run including `Test_SceneDocumentMigration_Fixtures` (acceptance suite)
- **Design:** Zenith2/Docs/Design/SCENE.md → "Stages 5 to 7"

### Z2-R-SCENE-060 Transform components registered

- **Statement:** `Zenith_RegisterSceneTypes` registers `Zenith.LocalTransform`, `Zenith.WorldTransform`, `Zenith.PreviousWorldTransform` and `Zenith.EntityName` with the property IDs, types, defaults and authorities of the table "Transform components".
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneTypes_RegistrationMatchesTable` in `Zenith_Transform_Tests.cpp` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"

### Z2-R-SCENE-061 Local transform authority and arbitration

- **Statement:** `Zenith.LocalTransform` properties are owned by `PHASE_POST_PHYSICS` with a precedence arbitration in the default order, and every other writer changes them through commands.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_LocalTransform_DirectWriteOutsidePhase6_IsScheduleError` in `Zenith_TransformPropagation_Tests.cpp`, asserting ECS's `SCHEDULE_ERROR_AUTHORITY_VIOLATION` and `SCHEDULE_ERROR_WRITER_SOURCE_MISSING` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"

### Z2-R-SCENE-062 World transforms derived and not serialized

- **Statement:** `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` are written only by phase 8 and SCENE playback, carry the not-serialized flag, and are recomputed after every load.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformHierarchy_OutOfTickLoadBuildsCache` and `Test_SceneCapture_CarryOverKeepsPersistentIds` asserting captures omit them (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"; Zenith2/Docs/Design/SCENE/Transforms.md → "Out-of-tick propagation"

### Z2-R-SCENE-063 World equals parent times local

- **Statement:** Each entity's world matrix equals its parent's world matrix times the matrix of its local transform, and a root's equals its local matrix.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_WorldEqualsParentTimesLocal` against a double-precision oracle (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"; Zenith2/Docs/Design/SCENE/Transforms.md → "Transform value type"

### Z2-R-SCENE-064 Decomposed world with shear flag

- **Statement:** The world transform stores the decomposition of its matrix and sets `WORLD_TRANSFORM_FLAG_HAS_SHEAR` when the matrix is not representable by position, rotation and scale.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Transform_ToMatrixAndDecomposeRoundTrip` and `Test_Transform_ShearDetected` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Transform value type"

### Z2-R-SCENE-065 Propagation runs in phase 8

- **Statement:** Transform propagation is the system `Scene.PropagateTransforms` of phase 8 with the declared access of the table "Propagation system", one node per world.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** the schedule dump of `Test_TransformPropagation_IndependentOfWorkersAndLayout` naming the node in stage 8 (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation system"; Zenith2/Docs/Design/SCENE.md → "Frame-graph nodes SCENE runs"

### Z2-R-SCENE-066 Transform change bits

- **Statement:** A world-transform row is marked changed exactly when its bits changed, so `QUERY_TERM_CHANGED` on `Zenith.WorldTransform` sees exactly the entities that moved, except in the one forced pass after a frame cancelled between the propagation node and its merge, which marks every row it visits so that no move of the cancelled tick loses its change bit.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_UnchangedWorld_NoChangeBits`, `Test_TransformPropagation_LocalChange_MarksSubtree`, `Test_TransformPropagation_ReparentMarksSubtree` and the second case of `Test_TransformPropagation_CancelledFrameCatchesUp` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"

### Z2-R-SCENE-067 Propagation is deterministic across workers and layout

- **Statement:** World transforms and their change versions are bit-identical per backend with 1, 2, 8 and all workers, forced compaction and the alternate chunk capacity.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_IndependentOfWorkersAndLayout` (Test step, every backend)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"; Zenith2/Docs/Design/SCENE.md → "Determinism obligations"

### Z2-R-SCENE-068 Previous world transform is the preceding tick

- **Statement:** During a tick, `Zenith.PreviousWorldTransform` holds the entity's world transform of the preceding tick, including after it stops moving.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_PreviousIsLastTick` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"; Zenith2/Docs/Design/SCENE/Transforms.md → "Interpolation data"

### Z2-R-SCENE-069 One interpolation formula

- **Statement:** `Zenith_InterpolateWorldTransform` interpolates shear-free transforms by lerp and nlerp with JOBS's alpha, returns the current rows for sheared ones and ignores alpha for teleported ones.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_InterpolateWorldTransform_Table` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Interpolation data"

### Z2-R-SCENE-070 Hierarchy cache follows ECS parent relation

- **Statement:** The per-world `Zenith_TransformHierarchy` is maintained at playback from ECS's structural events and equals a full rebuild from the world at every tick, holding no parent link of its own authority.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformHierarchy_IncrementalEqualsRebuild` (Test step, every backend)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Hierarchy cache"; Zenith2/Docs/Design/SCENE/Transforms.md → "Hierarchy maintenance at playback"

### Z2-R-SCENE-071 Hierarchy depth limit

- **Statement:** A reparent that would exceed depth 64 is corrected to a root at playback, logged and counted in `scene.hierarchy.depth_rejections`.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformHierarchy_DepthLimit_ReparentsToRoot` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Hierarchy maintenance at playback"

### Z2-R-SCENE-072 Missing derived components repaired

- **Statement:** A spawn carrying a local transform without the derived components gets them in the same playback, and an entity gaining a local transform by `AddComponent` gets them at phase 8's sync point with the world value computed from its parent's current world transform (or as a root).
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_SpawnWithoutDerivedComponents_Repaired` and `Test_TransformPropagation_LateWorldComponentsAdded` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Hierarchy maintenance at playback"; Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"

### Z2-R-SCENE-073 Orphans computed as roots

- **Statement:** A child whose parent has no transform is computed as a root with `WORLD_TRANSFORM_FLAG_ORPHANED` and counted.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_OrphanComputedAsRoot` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"

### Z2-R-SCENE-074 Non-finite transforms flagged and never asserted

- **Statement:** A non-finite local transform yields a world transform flagged `WORLD_TRANSFORM_FLAG_NON_FINITE` and the counter `scene.transform.non_finite`, and never an assert.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_NonFiniteFlagged` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"; Zenith2/Docs/Design/SCENE.md → "Failure handling"

### Z2-R-SCENE-075 Out-of-tick propagation after loads

- **Statement:** After every activation, carry-over or capture restore, `Zenith_PropagateTransformsOutOfTick` sets every world and previous transform before the world's next tick.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_StartupSceneLoadsBeforeFirstTick` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Out-of-tick propagation"

### Z2-R-SCENE-076 Teleport resets the subtree immediately

- **Statement:** A teleport played back at a sync point sets the local transform, recomputes the world transform of the entity and every descendant, sets previous equal to world and the teleported flag, so the next phase of the same tick reads consistent transforms.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Teleport_ResetsWorldAndPreviousOfSubtree` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Teleport"

### Z2-R-SCENE-077 Teleport listener order and stale entities

- **Statement:** Teleport dispatches `LIFECYCLE_EVENT_TELEPORTED` for the entity then its descendants in depth-first ascending simulation ID, and a teleport of a stale entity is dropped and counted.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Teleport_ListenersSeeEntityThenDescendants` and `Test_Teleport_StaleEntity_IsCountedNotAsserted` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Teleport"

### Z2-R-SCENE-078 Reparent keeping the world transform

- **Statement:** `Zenith_RecordSetParentKeepWorld` reparents at playback so the child's world matrix is unchanged, keeping the old local and counting when the new parent is singular.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SetParentKeepWorld_PreservesWorldMatrix` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Reparenting helpers"

### Z2-R-SCENE-079 Lifecycle listener dispatch

- **Statement:** `Zenith_LifecycleDispatcher` invokes listeners in registration order for the kinds in their mask on the sync-point node, forwards spawn and destroy events from ECS's structural observer, and reports reentrant dispatch.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_LifecycleDispatcher_RegistrationOrderAndMask` and `Test_LifecycleDispatcher_Reentrancy_IsReported` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_LifecycleListener header"

### Z2-R-SCENE-080 Propagation within its budget row

- **Statement:** The p95 span of `Scene.PropagateTransforms` stays within Z2-B-SCENE-002 on every workload that measures it once the row is allocated.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SCENE-002
- **Acceptance evidence:** the WS-FRAME workload runs' evidence records (acceptance suite) and `Zenith_Bench_Scene` propagation scenarios
- **Design:** Zenith2/Docs/Design/SCENE.md → "Rows SCENE owns"; Zenith2/Docs/Design/SCENE/Tests.md → "Benchmarks and the budget harness"

### Z2-R-SCENE-081 SCENE budget rows allocated

- **Statement:** Z2-B-SCENE-001 and Z2-B-SCENE-002 receive values from the Stage 1 per-system budget ruling for SCENE.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-SCENE-001, Z2-B-SCENE-002
- **Acceptance evidence:** the ruling's `Zenith2/Docs/Budgets.md` diff and its `allocation.json` checked by `Check-Allocation.ps1`
- **Design:** Zenith2/Docs/Design/SCENE.md → "Stage 1"; Zenith2/Docs/Design/SCENE.md → "Rows SCENE owns"

### Z2-R-SCENE-082 World state per world

- **Statement:** `Zenith_SceneWorldState::Create` and `Attach` register the observer, handler, both systems and both world resources before the first tick, and every construction step unwinds on failure.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneWorldState_Create_FailsAt<Step>_Unwinds` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneWorld header"

### Z2-R-SCENE-083 Resolution algorithm

- **Statement:** `Zenith_ResolveDocument` resolves a document into flat `Zenith_SceneData` by the steps of "Resolution algorithm": base, own entities, nested instances in ascending ID, validation.
- **Covers:** Z2-I-SCENE-02, Z2-I-SCENE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_NestedIdsDerived` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Resolution algorithm"

### Z2-R-SCENE-084 Nested instance IDs and references re-derived

- **Statement:** Every entity created by a nested instance takes the derived ID of its instance scope, and parents and ENTITY-typed values naming IDs of the instanced prefab are re-derived with it.
- **Covers:** Z2-I-SCENE-02, Z2-I-SCENE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_NestedIdsDerived` over three levels (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Re-identification"

### Z2-R-SCENE-085 Prefab cycles detected

- **Statement:** A prefab that reaches itself through instances or variants fails with `SCENE_ERROR_PREFAB_CYCLE` naming the chain.
- **Covers:** Z2-I-SCENE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_CycleDetected` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Resolution algorithm"

### Z2-R-SCENE-086 Prefab nesting depth limit

- **Statement:** Resolution fails with `SCENE_ERROR_PREFAB_TOO_DEEP` beyond 16 nesting levels or a variant chain of 16.
- **Covers:** Z2-I-SCENE-02, Z2-I-SCENE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_NestingDepthLimit` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Resolution algorithm"

### Z2-R-SCENE-087 Resolved prefab cache

- **Statement:** Resolution caches each prefab's resolved data keyed by the content digests of the prefab and every document it read, and a change to any of them invalidates the entry.
- **Covers:** Z2-I-SCENE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_CacheInvalidatedByDependencyChange` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Resolution algorithm"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Prefab provider"

### Z2-R-SCENE-088 Override application order

- **Statement:** An override block applies removedEntities, removedComponents, addedComponents and properties in that order.
- **Covers:** Z2-I-SCENE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_OverrideOrder` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Applying an override block"

### Z2-R-SCENE-089 Outermost override wins

- **Statement:** Overrides apply innermost first, so a scene instance's override beats a variant's and a nested instance's for the same target and path.
- **Covers:** Z2-I-SCENE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_OverrideMatrix` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Applying an override block"

### Z2-R-SCENE-090 Override keys are target ID plus property path

- **Statement:** Every override is keyed by the target's authoring ID in the instanced prefab's resolved space, the component's stable name and the property-ID path, never by index, name or position.
- **Covers:** Z2-I-SCENE-03, Z2-I-SCENE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_OverrideMatrix` and `Test_PrefabVariant_InstanceOverridesSurviveBaseSwap` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Override block"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Override semantics per property kind"

### Z2-R-SCENE-091 Override granularity per property kind

- **Statement:** Struct properties are overridden per nested property ID, and fixed arrays and containers are overridden whole.
- **Covers:** Z2-I-SCENE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_OverrideMatrix` rows for struct, fixed array and container (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Override semantics per property kind"

### Z2-R-SCENE-092 Dangling overrides are kept and reported

- **Statement:** An override whose target entity or component no longer exists is kept in the document, reported as `DOCUMENT_WARNING_DANGLING_OVERRIDE`, and ignored by resolution.
- **Covers:** Z2-I-SCENE-03, Z2-I-SCENE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_DanglingOverrideKeptAndReported` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Applying an override block"

### Z2-R-SCENE-093 Variants keep base IDs

- **Statement:** A variant resolves from its base with the base's entity IDs unchanged, so an instance's overrides stay valid when its prefab changes between a base and its variant.
- **Covers:** Z2-I-SCENE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabVariant_BaseIdsKept` and `Test_PrefabVariant_InstanceOverridesSurviveBaseSwap` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Variants"

### Z2-R-SCENE-094 Variant chains resolve in order

- **Statement:** A variant chain applies the root base's values, then each variant's block from the base outward, and a variant declaring a different root is a schema error.
- **Covers:** Z2-I-SCENE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabVariant_ChainOfThree` and `Test_PrefabVariant_DifferentRoot_IsSchemaError` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Variants"

### Z2-R-SCENE-095 Cook resolves prefabs completely

- **Statement:** Cooked scenes contain no prefab instance and cooked prefabs no nesting or variant base, entities are in activation order with parents first, and the cook lists every read prefab as a dependency; at Stage 3 this is exercised over the flat subset (scenes without instances and flat prefab documents cooked with a null provider).
- **Covers:** Z2-I-SCENE-02, Z2-I-SCENE-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooker_ActivationOrderParentsFirst`, `Test_SceneCooked_DecodeRoundTrip` and `Test_SceneCooker_FlatPrefabWithNullProvider` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Cooking resolved prefabs"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Flat prefabs at Stage 3"

### Z2-R-SCENE-096 Cooking is deterministic

- **Statement:** Cooking the same document and dependencies twice gives identical payload bytes, equal to the committed golden payloads.
- **Covers:** Z2-I-SCENE-02, Z2-I-SCENE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooker_Deterministic` and `Test_SceneCooker_GoldenPayloads` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Cooking resolved prefabs"; Zenith2/Docs/Design/SCENE/Tests.md → "Golden fixtures"

### Z2-R-SCENE-097 Runtime prefab instantiation is one atomic buffer

- **Statement:** `Zenith_RecordInstantiatePrefab` records every entity of a resident prefab into the caller's buffer after reserving its whole size, or records nothing and returns the null ticket.
- **Covers:** Z2-I-SCENE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** Z2-B-ECS-003
- **Acceptance evidence:** `Test_PrefabInstantiate_RecordsOneBufferAtomically` and `Test_PrefabInstantiate_NotResident_ReturnsNullTicket` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Runtime instantiation"

### Z2-R-SCENE-098 Instantiated prefabs visible to the next phase

- **Statement:** A prefab instantiated in phase p is present, with its internal references resolved, from phase p+1, and its placement composes position and rotation from the call with the authored scale.
- **Covers:** Z2-I-SCENE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabInstantiate_VisibleNextPhase`, `Test_PrefabInstantiate_InternalReferencesResolved` and `Test_Transform_ComposePlacementKeepsAuthoredScale` (Test step, every backend)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Runtime instantiation"; Zenith2/Docs/Design/SCENE/Transforms.md → "Transform value type"

### Z2-R-SCENE-099 Instantiation deterministic across workers

- **Statement:** Prefab instantiation from parallel invocations yields identical handles, simulation IDs and tick hashes with 1, 2, 8 and all workers.
- **Covers:** Z2-I-SCENE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabInstantiate_DeterministicAcrossWorkers` (Test step, every backend)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Runtime instantiation"

### Z2-R-SCENE-100 Unpacking an instance

- **Statement:** `Zenith_UnpackPrefabInstance` replaces an instance by its resolved entities with the derived IDs and folded overrides, returning its inverse edit.
- **Covers:** Z2-I-SCENE-02, Z2-I-SCENE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceRepair_DeleteWithoutReplacementUnpacks` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"

### Z2-R-SCENE-101 Prefab editing edits for EDIT

- **Statement:** Setting a property of an instance entity writes an override, reverting removes it, and applying overrides to the prefab moves the values in one two-document edit with an inverse.
- **Covers:** Z2-I-SCENE-03, Z2-I-SCENE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneEdit_SetPropertyOnInstanceWritesOverride` and `Test_SceneEdit_InverseRestoresBytes` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Document edits for the editor"

### Z2-R-SCENE-102 Reference repair on move

- **Statement:** A move or rename of a referenced document or asset changes no ID and rewrites only prefab-instance hints.
- **Covers:** Z2-I-SCENE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceRepair_MoveUpdatesHintsOnly` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"

### Z2-R-SCENE-103 Reference repair on delete

- **Statement:** Deleting an asset rewrites references to a same-kind replacement or to null, and a deleted prefab's instances are unpacked when requested or reported otherwise.
- **Covers:** Z2-I-SCENE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceRepair_DeleteWithReplacement` and `Test_ReferenceRepair_DeleteWithoutReplacementUnpacks` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"

### Z2-R-SCENE-104 Reference repair on prefab entity deletion

- **Statement:** Removing a prefab entity flags every override targeting it as dangling and prunes them only when requested.
- **Covers:** Z2-I-SCENE-05, Z2-I-SCENE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceRepair_EntityDeletedFlagsAndPrunes` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"

### Z2-R-SCENE-105 Duplicate documents re-identified

- **Statement:** A document copy sharing an existing document ID receives a fresh ID and references keep naming the original.
- **Covers:** Z2-I-SCENE-05, Z2-I-SCENE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceRepair_DuplicateDocumentReIded` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"

### Z2-R-SCENE-106 Repair plans write one journaled batch

- **Statement:** Planning a repair writes nothing and yields per-document canonical bytes, diffs and expected digests, and applying it is one journaled batch.
- **Covers:** Z2-I-SCENE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceRepair_PlanWritesNothing_ApplyIsOneBatch` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_ReferenceRepair header"

### Z2-R-SCENE-107 Reference index incremental update

- **Statement:** `Zenith_ReferenceIndex` maps every persistent ID to its referencing documents and paths, and incremental updates equal a fresh build.
- **Covers:** Z2-I-SCENE-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReferenceIndex_UpdateMatchesRebuild` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_ReferenceRepair header"

### Z2-R-SCENE-108 Cooked scene container and sections

- **Statement:** Cooked scenes and prefabs are ASSET cooked asset files with the magic, kind and format version 1 of "Cooked scene payload", holding exactly SCENE's sections 1 to 6 at index 0 in ASSET's section table, with 48-byte entity records and 24-byte asset-reference records whose kind is a `Zenith_AssetKind` code.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooker_GoldenPayloads` and `Test_SceneCooked_DecodeRoundTrip` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Cooked scene payload"; Zenith2/Docs/Design/SCENE/Format.md → "Scene header section"; Zenith2/Docs/Design/SCENE/Format.md → "Sections"

### Z2-R-SCENE-109 Cooked decoder treats input as untrusted

- **Statement:** The decoder validates every count, offset and index with overflow-checked arithmetic and refuses corrupt input with `SCENE_ERROR_COOKED_CORRUPT`, and it is fuzzed.
- **Covers:** Z2-I-SCENE-07, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooked_CorruptInputs_AreRejected` and `Zenith_Fuzz_SceneCooked` (`Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Sections"

### Z2-R-SCENE-110 Mismatched cooked versions refused

- **Statement:** A cooked scene of another format version is refused with `SCENE_ERROR_COOKED_VERSION` (ASSET's version check at load, SCENE's own check in `Zenith_DecodeCookedSceneFile`) and never migrated.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooked_VersionMismatch_IsRefused` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Sections"

### Z2-R-SCENE-111 Startup scene loaded before the first tick

- **Statement:** `Zenith_SceneLoader::LoadStartupScene` loads the startup scene into the initial world at startup step 17 with the FAIL policy, so its entities and world transforms exist at the first tick boundary.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** Z2-B-SHIP-013, Z2-B-SHIP-014
- **Acceptance evidence:** `Test_SceneLoader_StartupSceneLoadsBeforeFirstTick` and `Test_Scene_MinimalGameBootsHeadless` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Startup scene"

### Z2-R-SCENE-112 Preparation touches no world

- **Statement:** A scene preparation decodes, resolves dependencies and builds spawn data on background jobs in slices of at most 2 ms and never reads or writes any world.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_PreparationTouchesNoWorld` and `Test_SceneLoader_SlicesWithinBackgroundBound` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Preparation operation"; Zenith2/Docs/Design/SCENE/Loading.md → "Preparation state machine"

### Z2-R-SCENE-113 Preparation is an operation handle

- **Statement:** `BeginPrepare` returns a `Zenith_OperationHandle` whose progress is monotonic across the stages and whose failure code is a `Zenith_SceneErrorCode`.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_CancelAtEachStage_ReleasesEverything` asserting progress and status (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Preparation state machine"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneLoader header"

### Z2-R-SCENE-114 Cancellation at every preparation stage

- **Statement:** Cancelling a preparation in any state releases its arena, asset handles and interned-string references on its executor thread before it reports `OPERATION_STATUS_CANCELLED`.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_CancelAtEachStage_ReleasesEverything` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Cancellation"

### Z2-R-SCENE-115 Cancellation stress

- **Statement:** 1,000 issue-then-cancel cycles at randomised stages leave no entity, asset reference, interned-string reference or allocation behind.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_CancelStress` (`z2-stress`, Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Cancellation"

### Z2-R-SCENE-116 Completion before cancel reports success

- **Statement:** A preparation that completes before it observes a cancel reports `OPERATION_STATUS_SUCCEEDED` and its result is released by the requester.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_SucceededBeforeCancel_ReportsSuccess` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Cancellation"

### Z2-R-SCENE-117 Preparation reserves memory with the governor

- **Statement:** A preparation larger than `scene.prepareReserveThresholdBytes` reserves its memory with the governor before decoding and fails with `SCENE_ERROR_MEMORY_REFUSED` when refused.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SCENE-001
- **Acceptance evidence:** `Test_SceneLoader_MemoryRefused_FailsCleanly` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Preparation state machine"

### Z2-R-SCENE-118 Scene memory within its row

- **Statement:** Scene and prefab runtime data (hierarchy caches, prepared scenes, resident prefab data, additive sets) is reported through the loader's memory consumer and stays within Z2-B-SCENE-001.
- **Covers:** Z2-I-SCENE-07, Z2-I-SCENE-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SCENE-001
- **Acceptance evidence:** the MP-SUBMEM figure in the WS-FRAME evidence records (acceptance suite)
- **Design:** Zenith2/Docs/Design/SCENE.md → "Rows SCENE owns"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneLoader header"

### Z2-R-SCENE-119 Isolated-world activation within a budget

- **Statement:** Activation into an isolated world runs in prologue step 7 (the widening cross-doc item X109(b) makes) in slices bounded by `scene.activationBudgetUs` plus one chunk, and the world contributes no node before its swap.
- **Covers:** Z2-I-SCENE-07, Z2-I-SCENE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLoader_IsolatedActivationWithinBudget` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Out-of-tick activation"; Zenith2/Docs/Design/SCENE/Loading.md → "Scene transitions"

### Z2-R-SCENE-120 References bound by persistent ID at activation

- **Statement:** ENTITY references from a loaded scene to entities outside its activation chunk are bound at playback by persistent ID, and an unresolved target is null and counted.
- **Covers:** Z2-I-SCENE-07, Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ExternalReferencesBound` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Out-of-tick activation"

### Z2-R-SCENE-121 Additive loads committed in merge order

- **Statement:** A C++ additive load (`Zenith_RecordLoadAdditiveScene`, played at sync-point step 7) and a script additive load (committed from the script-load inbox at the next tick boundary in inbox order) each assign their instance ID in that order, and `Zenith_SceneLoader::StepActivations` starts the pending preparations on the main thread in the next frame prologue in world-tag and commit order.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ActivateAtomicallyAtPublicationTick` asserting instance IDs across runs and `Test_AdditiveScenes_ScriptRequestAdoptsCompletionID` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneLoader header"

### Z2-R-SCENE-122 Additive completion published at a tick boundary

- **Statement:** An additive load's completion is published at a tick boundary of its world in completion-ID order and logged in the replay log.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ReplayPublishesAtRecordedTick` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"; Zenith2/Docs/Design/SCENE/Loading.md → "Publication service"

### Z2-R-SCENE-123 Additive activation is atomic at the publication tick

- **Statement:** A published additive scene is recorded as one buffer by `Scene.ActivateAdditive` in phase 1 and all its entities exist from phase 2 of the publication tick, or none do.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-053
- **Budgets:** Z2-B-ECS-003, Z2-B-ECS-004
- **Acceptance evidence:** `Test_AdditiveScenes_ActivateAtomicallyAtPublicationTick` (Test step, every backend)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"

### Z2-R-SCENE-124 Additive activation retries after merge rejection

- **Statement:** An activation rejected by merge-time capacity retries at the next tick's phase 1 up to `scene.activationRetryTicks` and then fails with `SCENE_ERROR_ACTIVATION_CAPACITY`, with nothing spawned.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ECS-003
- **Acceptance evidence:** `Test_AdditiveScenes_MergeRejectionRetriesThenFails` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"

### Z2-R-SCENE-125 Oversized additive scenes fail at once

- **Statement:** An additive scene whose spawns exceed `ecs.maxSpawnsPerTick` or one buffer's cap fails with `SCENE_ERROR_ACTIVATION_TOO_LARGE`.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ECS-003, Z2-B-ECS-004
- **Acceptance evidence:** `Test_AdditiveScenes_TooLarge_FailsAtOnce` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"

### Z2-R-SCENE-126 Duplicate additive instances derive IDs

- **Statement:** A second concurrent instance of a scene is refused with `SCENE_ERROR_DUPLICATE_INSTANCE` unless requested, and then derives every entity ID with the instance scope during the preparation's `BUILDING` slices, so its phase-1 activation derives nothing.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_DuplicateInstanceNeedsFlag` (Test step) and the report figure of `Bench_Scene_DeriveAuthoringID` (`Zenith_Bench_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Instance identity"; Zenith2/Docs/Design/SCENE/Loading.md → "Preparation state machine"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"

### Z2-R-SCENE-127 Additive unload

- **Statement:** Unloading an active instance destroys its roots and subtrees at the end of the tick; unloading while loading cancels the preparation and nothing appears.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_UnloadDestroysSubtrees` and `Test_AdditiveScenes_UnloadWhileLoadingCancels` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Unload"

### Z2-R-SCENE-128 Additive scene set states

- **Statement:** Each additive instance follows the state table of "Additive scene set" and its state and error are readable by C++ and by scripts.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ActivateAtomicallyAtPublicationTick` and `Test_AdditiveScenes_MergeRejectionRetriesThenFails` asserting states (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Additive scene set"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_AdditiveScenes header"

### Z2-R-SCENE-129 Additive replay determinism

- **Statement:** A replay activates every additive scene at its recorded publication tick, holding the tick until the recomputed preparation is ready.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ReplayPublishesAtRecordedTick` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Principles"; Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"

### Z2-R-SCENE-130 Transition preparation in isolation

- **Statement:** A scene transition prepares and activates the target scene into an isolated world while the old world keeps ticking, and no tick observes a half-loaded world.
- **Covers:** Z2-I-SCENE-09, Z2-I-SCENE-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_SceneTransition_SwapsAfterPublishedLoad` (integration suite, Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Scene transitions"

### Z2-R-SCENE-131 Transition swaps after the published load

- **Statement:** The swap happens in the frame prologue after the old-world tick that published the load completion, and the `WORLD_SWAP` record names that tick.
- **Covers:** Z2-I-SCENE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneTransition_SwapsAfterPublishedLoad` and `Test_SceneTransition_ReplaySwapsAtSameTick` (integration suite)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Scene transitions"

### Z2-R-SCENE-132 Transition carry-over keeps persistent IDs

- **Statement:** Carry-over entities are captured after the old world's final end-of-tick playback and activated into the new world with new handles and unchanged persistent IDs; duplicates and missing IDs are skipped and counted.
- **Covers:** Z2-I-SCENE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCapture_CarryOverKeepsPersistentIds` and `Test_SceneCapture_DuplicateCarryOverSkipped` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Carry-over"

### Z2-R-SCENE-133 In-flight results discarded at a transition

- **Statement:** Asynchronous results of the old world completing after the swap, including its additive loads, are discarded unpublished and counted.
- **Covers:** Z2-I-SCENE-09, Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneTransition_InFlightAdditiveDiscarded` (integration suite)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "In-flight asynchronous results across a transition"

### Z2-R-SCENE-134 Transition cancellation and refusal

- **Statement:** Cancelling a transition during activation destroys the isolated world with no leak, and a refused transition peak starts no preparation.
- **Covers:** Z2-I-SCENE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_SceneTransition_CancelDuringActivationDestroysIsolatedWorld` and `Test_SceneTransition_PeakRefused_NoLoadStarted` (integration suite)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Cancellation"; Zenith2/Docs/Design/SCENE/Loading.md → "Scene transitions"

### Z2-R-SCENE-135 Scripts request transitions through commands

- **Statement:** `Zenith_RecordRequestSceneTransition` and the Luau `Scene.RequestTransition` record `Scene.RequestTransition`, whose playback calls the runtime's transition sink in merge order.
- **Covers:** Z2-I-SCENE-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_RequestTransition_CallsSinkInMergeOrder` in `Zenith_SceneLoader_Tests.cpp` (Test step) and `Test_SceneTransition_SwapsAfterPublishedLoad` (integration suite)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Scene transitions"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Script bindings"

### Z2-R-SCENE-136 SCENE counters

- **Statement:** SCENE publishes the counters of the table "Counters" per world per tick through the counter hook.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCounters_NamesAndValues` in `Zenith_SceneDiagnostics_Tests.cpp` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE.md → "Counters"; Zenith2/Docs/Design/SCENE/Tests.md → "Diagnostics and binding tests"

### Z2-R-SCENE-137 SCENE log categories and dumps

- **Statement:** SCENE logs in the categories of "Log categories" with the listed message forms and writes `zenith2.scene-dump` version 1 on `scene.dump`.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneLog_CategoriesAndDump_Golden` in `Zenith_SceneDiagnostics_Tests.cpp` against `Tests/Fixtures/Dumps/Hierarchy6.dump.json` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE.md → "Log categories"; Zenith2/Docs/Design/SCENE/Format.md → "Scene diagnostic dump"; Zenith2/Docs/Design/SCENE/Tests.md → "Diagnostics and binding tests"

### Z2-R-SCENE-138 Scene script bindings

- **Statement:** SCENE's Luau bindings record the commands of `Zenith_LifecycleCommands.h` and `Zenith_AdditiveScenes.h`, except `Scene.LoadAdditive`, which appends one `SIDE_EFFECT_KIND_ASYNC_REQUEST` record of source kind 8 and returns a `Zenith.AsyncTicket`, and script misuse raises the catalogued Luau misuse error, never an assert.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-09, Z2-I-SCENE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneScriptBindings_RecordCommandsAndRejectMisuse` in `Zenith_SceneScriptBindings_Tests.cpp` (Test step), with SCRIPT's `Test_Script_Bindings_MarshallingLossless` and `Test_Script_Bindings_SideEffectMask` covering the marshalling every binding passes through
- **Design:** Zenith2/Docs/Design/SCENE/SceneAPI.md → "Script bindings"; Zenith2/Docs/Design/SCENE.md → "Luau errors versus asserts"; Zenith2/Docs/Design/SCENE/Tests.md → "Diagnostics and binding tests"

### Z2-R-SCENE-139 Scaffold template scenes are valid

- **Statement:** The three MOD template scene texts are canonical scene documents that load, cook and boot.
- **Covers:** Z2-I-SCENE-01, Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneDocument_RoundTripCanonicalBytes` over `ThreeTemplates/` and MOD's `Test_NewGame_CreatedGameBoots` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Example scene document"; Zenith2/Docs/Design/SCENE/Tests.md → "Golden fixtures"

### Z2-R-SCENE-140 Scene open within the editor row

- **Statement:** Opening OpenWorldSlice's largest scene in the editor, dominated by SCENE's open and resolve, meets Z2-B-EDIT-005.
- **Covers:** Z2-I-SCENE-01, Z2-I-SCENE-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-005
- **Acceptance evidence:** the W-Editor evidence record for scene open (acceptance suite) and `Bench_Scene_Resolve_Town`
- **Design:** Zenith2/Docs/Design/SCENE.md → "Rows SCENE is measured against"

### Z2-R-SCENE-141 Structure-aware merge support

- **Statement:** SCENE provides CONTENT's merge driver the canonical schemas, the ID-keyed structure and the override key rules, so disjoint edits of one scene merge without conflict and array-valued overrides fall back to a textual conflict marker.
- **Covers:** Z2-I-CONTENT-28, Z2-I-SCENE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCanonicalSchema_ClassifiesEveryGoldenMember` and `Test_CanonicalJson_LineMergeOfDisjointEntityAdds` (Test step), which CONTENT's merge-driver tests then build on
- **Design:** Zenith2/Docs/Design/SCENE.md → "Merge support for CONTENT"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Override semantics per property kind"

### Z2-R-SCENE-142 Scene work on Android

- **Statement:** The SCENE unit tests build for `android-arm64` and run on the S24 in the Stage 3 acceptance device run.
- **Covers:** Z2-I-SCENE-08, Z2-I-SCENE-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** the Stage 3 acceptance device run record (acceptance suite)
- **Design:** Zenith2/Docs/Design/SCENE.md → "Supported matrix"

### Z2-R-SCENE-143 Sort-key insertion algorithm

- **Statement:** `Zenith_MakeSortKeyBetween` returns a valid key strictly between its two bounds (an empty bound meaning first or last), never ending in `0` and at most one character longer than the longer bound, and fails `SCENE_ERROR_SCHEMA` for invalid or unordered bounds with its output unchanged.
- **Covers:** Z2-I-CONTENT-28, Z2-I-SCENE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SortKey_BetweenIsStrictlyOrderedAndBounded` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Sort key insertion algorithm"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Sort keys for edits"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneDocument header"

### Z2-R-SCENE-144 Sort-key exhaustion and rebalance

- **Statement:** When no key of at most 64 characters lies between two neighbours `Zenith_MakeSortKeyBetween` fails `SCENE_ERROR_LIMIT`, and `Zenith_MakeEvenSortKeys` gives any sibling count up to the entity limit evenly spaced ascending keys, which EDIT applies as one set of `SCENE_EDIT_SET_PARENT` edits over exactly that sibling group, both on `SCENE_ERROR_LIMIT` and whenever the destination group holds equal keys; `SCENE_EDIT_SET_PARENT` accepts an entity or a prefab-instance ID.
- **Covers:** Z2-I-CONTENT-28, Z2-I-SCENE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SortKey_ExhaustionFailsAndRebalanceOrders` and `Test_SceneEdit_SetParentMovesInstance` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Sort key insertion algorithm"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Sort keys for edits"

### Z2-R-SCENE-145 Unknown members anchored at write time

- **Statement:** The canonical writer anchors each unknown member to the nearest preceding member of the tree that its rule knows, evaluated over the tree as migration steps left it, so a removed anchor re-anchors to the previous surviving known member or the object start and an in-place rename keeps its followers, and the output stays idempotent.
- **Covers:** Z2-I-SCENE-10, Z2-I-ECS-03, Z2-I-CONTENT-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_CanonicalJson_UnknownMemberAnchorRemovedByStep` and `Test_CanonicalJson_UnknownMembersKeepAnchoredPosition` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Unknown members across migration steps"

### Z2-R-SCENE-146 Unknown-member guard around every step

- **Statement:** Around every migration step the framework checks, per object, that the members unknown to the current schema that the step did not declare removed through `m_pfnDeclareRemoved` survive exactly once in the same object, with unchanged value bytes and in unchanged relative order, ignoring members the step inserted, and fails any other step with `DOCUMENT_ERROR_STEP_FAILED` naming the step and the member's document pointer.
- **Covers:** Z2-I-SCENE-10, Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_StepCannotDeleteUnknownField` and `Test_DocumentMigrator_DeclaredRemovalIsAllowed` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Unknown members across migration steps"

### Z2-R-SCENE-147 Derived IDs depend on no open decision

- **Statement:** Authoring-ID derivation uses CORE's SHA-256 and never `Zenith_Hash64`, so the D-SEL-HASH (open, Stage 1) ruling changes no derived ID, and the derivation is a permanent format property changed only by a decision that supplies a document-set rewrite, a re-cook and a SAVE step.
- **Covers:** Z2-I-SCENE-01, Z2-I-SCENE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AuthoringID_DeriveIndependentOfHash64` over vectors generated outside the engine (Test step, every preset)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_AuthoringID header"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Digests"

### Z2-R-SCENE-148 Nested override targets store derived IDs

- **Statement:** An override target, `parent` or ENTITY value that names an entity of a nested instance is stored in the document as that entity's derived ID in the instanced prefab's resolved ID space, and resolution applies it to that entity alone.
- **Covers:** Z2-I-SCENE-01, Z2-I-SCENE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_PrefabResolve_NestedOverrideTargetIsStoredDerivedID` over the golden `Town.scene.json` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Format.md → "Resolved ID space"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"

### Z2-R-SCENE-149 Derived components removed with the local transform

- **Statement:** An entity that loses `Zenith.LocalTransform` loses `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` at phase 8's sync point, counted in `scene.transform.derived_removed`, and the next propagation pass recomputes its children as orphaned roots, and their descendants, through the revisit marks even when none of their own versions moved.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_LocalRemoved_DerivedRemoved` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"; Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation state"; Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation system"

### Z2-R-SCENE-150 Low-disk refusal before a journaled write

- **Statement:** A journaled write whose storage reports less free space than `2 × Σnew + Σold` (the new bytes of the batch and the old bytes its backups copy; the threshold of "Journaled write" step 0b) fails with `DOCUMENT_ERROR_LOW_DISK` naming the storage before any mutation.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_JournaledWrite_LowDisk_WritesNothing` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentStorage header"

### Z2-R-SCENE-151 Pending journals reported over read-only storage

- **Statement:** `RecoverPendingJournals` over a read-only storage changes nothing and returns `DOCUMENT_ERROR_JOURNAL_PENDING` naming the first journal when one exists, which the cook uses to refuse cooking half-migrated documents.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentMigrator_ReadOnlyStorage_PendingJournalReported` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Ownership split with ASSET"

### Z2-R-SCENE-152 Flat prefabs at Stage 3

- **Statement:** A flat prefab document (kind PREFAB, `variantOf` null, no instances) loads with the Stage 2 document model, cooks with a null provider to a `ZPFB` payload rooted at index 0, and instantiates through `Zenith_RecordInstantiatePrefab` and `Scene.Instantiate` for `ctx:Spawn`, while a variant or nested prefab with a null provider fails `SCENE_ERROR_PREFAB_NOT_FOUND`.
- **Covers:** Z2-I-SCENE-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCooker_FlatPrefabWithNullProvider`, `Test_PrefabInstantiate_RecordsOneBufferAtomically` and `Test_PrefabInstantiate_VisibleNextPhase` over `Lamp.prefab.json` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Flat prefabs at Stage 3"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_PrefabResolver header"

### Z2-R-SCENE-153 Document type IDs are stable-name type IDs

- **Statement:** A document type's ID is `Zenith_MakeTypeID` of its stable name, so a reflected type registered under the same stable name has the same ID and a step registered with either reaches the same chain.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_MigrationRegistry_TypeIdEqualsReflectedTypeId` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"

### Z2-R-SCENE-154 Additive loads fail when the publication source is full

- **Statement:** When the world's `ASYNC_SOURCE_SCENE_LOAD` source is at its in-flight limit, the playback of a C++ `Scene.LoadAdditive` fails that load with `SCENE_ERROR_LIMIT`, counts it in `scene.additive.failed` and starts no preparation; a script load was charged to the same limit at its merge and needs no second completion ID.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_PublicationSourceFull_FailsLoad` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Publication service"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_AdditiveScenes header"

### Z2-R-SCENE-155 Children rejoin when a parent regains its local transform

- **Statement:** An entity that regains `Zenith.LocalTransform` gets its derived components at phase 8's sync point, and the next propagation pass recomputes it and its children, which lose `WORLD_TRANSFORM_FLAG_ORPHANED` and take the parent's world transform, even when none of their own versions moved.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_LocalRegained_ChildrenRejoin` (Test step, every backend)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"; Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation state"

### Z2-R-SCENE-156 Propagation filters on the previous read version

- **Statement:** The propagation system keeps no change version of its own: it compares each Local property row version against `Zenith_SystemContext::GetPreviousReadVersion()` under the ring order, runs a full pass when that version is 0, and gives the same world transforms whether or not an unchanged row is recomputed, including after a cancelled frame, `RestoreSimulationState` and a version wrap.
- **Covers:** Z2-I-SCENE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_TransformPropagation_FiltersOnPreviousReadVersion`, `Test_TransformPropagation_CancelledFrameCatchesUp` and `Test_TransformPropagation_RestoreGivesFullPass` (Test step, every backend)
- **Design:** Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation state"; Zenith2/Docs/Design/SCENE/Transforms.md → "Propagation algorithm"; Zenith2/Docs/Design/SCENE.md → "Cancellation"

### Z2-R-SCENE-157 Document pointers address members by exact key

- **Statement:** Every path the framework accepts or produces inside a document (`FindPath`, removal declarations, the unknown-member guard, diff changed paths and warning field paths) is a document pointer of rules D1 to D4 (RFC 6901 tokens with `~0` and `~1` escapes), so a key containing `.` or `/` is addressed as one member.
- **Covers:** Z2-I-ECS-03, Z2-I-SCENE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentPointer_GrammarAndEscapes` and case 3 of `Test_DocumentMigrator_DeclaredRemovalIsAllowed` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Document pointers"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Document tree"

### Z2-R-SCENE-158 Script additive loads have one completion ID

- **Statement:** `Scene.LoadAdditive` in a script appends one asynchronous request of source kind 8 whose completion ID SCRIPT assigns at commit in merge order and SCENE adopts, and its ticket's `Result()` returns the instance ID, success and error decoded from the 8-byte `Zenith_SceneLoadResult` published at the load's tick boundary.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ScriptRequestAdoptsCompletionID` and `Test_SceneLoadResult_EncodeDecode` (Test step) and `Test_SceneScript_LoadAdditiveTicketResult` (`Zenith_Integration_Scene`, Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Script requests"; Zenith2/Docs/Design/SCENE/Loading.md → "Additive load sequence"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_AdditiveScenes header"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Script bindings"

### Z2-R-SCENE-159 Tick-boundary and publication handlers change only SCENE state

- **Statement:** SCENE's publish callback, which receives the tick boundary's `Zenith_TickBoundaryAccess`, and SCENE's tick-boundary handler change only SCENE's per-world records and queues: the handler commits the script-load inbox and releases `UNLOADED` records at step 3, after step 1's publications.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ScriptRequestAdoptsCompletionID` and `Test_AdditiveScenes_TickBoundaryHandlerReleasesUnloaded` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/Loading.md → "Tick-boundary handler"; Zenith2/Docs/Design/SCENE/Loading.md → "Publication service"; Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_AdditiveScenes header"

### Z2-R-SCENE-160 Scene asset kinds conform to ASSET

- **Statement:** `Zenith_Scene` and `Zenith_Prefab` declare `static constexpr Zenith_AssetKind eASSET_KIND` as `ASSET_KIND_SCENE` (5) and `ASSET_KIND_PREFAB` (6), and `Zenith_RegisterSceneAssetKinds` registers their two kind loaders at the cooked format version before `CloseRegistration`; both loaders refuse a section table that is not exactly kinds 1 to 6 at index 0 with `SCENE_ERROR_COOKED_CORRUPT`; the prefab loader takes its dependency handles from the hard dependencies ASSET hands it and acquires and waits for nothing, so a prefab becomes resident only with every dependency ACTIVE or substituted, and one whose dependency failed without a substitute fails `ASSET_ERROR_DEPENDENCY_FAILED` without its loader being called.
- **Covers:** Z2-I-SCENE-07, Z2-I-SCENE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneAssets_KindsAndLoaders` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneAssets header"; Zenith2/Docs/Design/SCENE/Format.md → "Files and virtual paths"

### Z2-R-SCENE-161 Scene and prefab cook steps

- **Statement:** SCENE registers the `Zenith_CookStep`s `scene.scene` and `scene.prefab` (`COOK_PHASE_ASSET`, version 1, no engine fallback) through `Zenith_RegisterSceneCookSteps`; their keys add the SHA-256 of the document and of every prefab document its resolution reads, in reading order, and their cook declares every asset reference, and in a scene every instanced prefab, with `ASSET_DEPENDENCY_FLAG_HARD` before writing the file through `MakeWriterDesc` and `WriteAssetFile`.
- **Covers:** Z2-I-SCENE-07, Z2-I-SCENE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneCookSteps_KeysAndHardReferences` (Test step, `ZenithCook_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/Prefabs.md → "Scene and prefab cook steps"; Zenith2/Docs/Design/SCENE/Prefabs.md → "Cooking resolved prefabs"; Zenith2/Docs/Design/SCENE.md → "Cook step"

### Z2-R-SCENE-162 Storage-root writer lock around every framework write

- **Statement:** Over a writable host storage, `RecoverPendingJournals`, `ApplyPlan` and `SaveDocuments` hold the storage root's cross-process writer lock (`<storage root>/.migration/writer.lock`, ASSET's lock file) from before their first decision-relevant read until they return, per batch and never per session, reuse a hold their caller already has, and fail `DOCUMENT_ERROR_STORAGE_LOCKED` with nothing recovered or written when another process holds it for the whole wait.
- **Covers:** Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_HostDocumentStorage_WriterLockSerialisesWriters` (Test step, `ZenithDocumentHost_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Storage-root writer lock"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentStorage header"; Zenith2/Docs/Design/SCENE/DocumentFramework.md → "ZenithDocumentHost"

### Z2-R-SCENE-163 Additive set enumeration for saves

- **Statement:** `Zenith_AdditiveSceneSet::ForEachInstance` visits every LOADING, QUEUED, ACTIVE and FAILED record exactly once in ascending instance ID with a `Zenith_SavedSceneInstance` (a QUEUED record reported as LOADING with its completion ID, an ACTIVE one with its roots in ascending simulation ID, a FAILED one with its code and detail), never visits an UNLOADED record, and allocates nothing.
- **Covers:** Z2-I-SCENE-06, Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_SaveEnumerationAndRestore` (Test step, `ZenithScene_Tests`); `Test_SceneStage3_AdditiveSetSaveRoundTrip` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Saved instance enumeration"; Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Activation hook declarations"

### Z2-R-SCENE-164 Additive set restore entry

- **Statement:** `Zenith_AdditiveSceneSet::RestoreInstances`, called out of tick on the main thread into a set holding no record, validates every entry before applying any (ascending non-zero IDs below `uNextInstanceID`, a defined state, a non-zero completion ID for LOADING, live roots for ACTIVE) and returns `SCENE_ERROR_INVALID_DESC` or `SCENE_ERROR_LIMIT` with the set unchanged or emptied, and otherwise restores ACTIVE and FAILED records with their roots, codes, details, flags, policy and tag and the instance counter, with no hook call for an ACTIVE instance.
- **Covers:** Z2-I-SCENE-06, Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_SaveEnumerationAndRestore` (Test step, `ZenithScene_Tests`); `Test_SceneStage3_AdditiveSetSaveRoundTrip` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Restore algorithm"

### Z2-R-SCENE-165 Restored loading instances re-requested under the recorded completion ID

- **Statement:** A LOADING (or QUEUED-saved-as-LOADING) entry passed to `RestoreInstances` is re-requested under its recorded completion ID without an `AllocateCompletionID` call, deriving its entity IDs with the instance scope exactly when `m_bDerivedScope` is set; a re-request refused for a full set (`SCENE_ERROR_LIMIT`) or a live duplicate (`SCENE_ERROR_DUPLICATE_INSTANCE`) creates no record, parks `{ 0, code, 0 }` under the recorded ID at once and counts `scene.restore.refused`.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07, Z2-I-SAVE-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_RestoreReRequestsUnderRecordedID` (Test step, `ZenithScene_Tests`); `Test_SceneStage3_AdditiveSetSaveRoundTrip` (Test step)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Restore algorithm"; Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Additive set save and restore"

## Coverage of inventory items

| Item | Classification | Requirements |
|---|---|---|
| Z2-I-SCENE-01 | required | Z2-R-SCENE-004, Z2-R-SCENE-007, Z2-R-SCENE-008, Z2-R-SCENE-009, Z2-R-SCENE-010, Z2-R-SCENE-011, Z2-R-SCENE-012, Z2-R-SCENE-013, Z2-R-SCENE-014, Z2-R-SCENE-021, Z2-R-SCENE-023, Z2-R-SCENE-084, Z2-R-SCENE-090, Z2-R-SCENE-101, Z2-R-SCENE-105, Z2-R-SCENE-126, Z2-R-SCENE-139, Z2-R-SCENE-140, Z2-R-SCENE-143, Z2-R-SCENE-144, Z2-R-SCENE-147, Z2-R-SCENE-148 |
| Z2-I-SCENE-02 | required | Z2-R-SCENE-009, Z2-R-SCENE-024, Z2-R-SCENE-034, Z2-R-SCENE-083, Z2-R-SCENE-084, Z2-R-SCENE-085, Z2-R-SCENE-086, Z2-R-SCENE-087, Z2-R-SCENE-095, Z2-R-SCENE-096, Z2-R-SCENE-097, Z2-R-SCENE-098, Z2-R-SCENE-099, Z2-R-SCENE-100, Z2-R-SCENE-138, Z2-R-SCENE-140, Z2-R-SCENE-147, Z2-R-SCENE-152, Z2-R-SCENE-160, Z2-R-SCENE-161 |
| Z2-I-SCENE-03 | required | Z2-R-SCENE-051, Z2-R-SCENE-088, Z2-R-SCENE-089, Z2-R-SCENE-090, Z2-R-SCENE-091, Z2-R-SCENE-092, Z2-R-SCENE-101, Z2-R-SCENE-104, Z2-R-SCENE-141, Z2-R-SCENE-148 |
| Z2-I-SCENE-04 | required | Z2-R-SCENE-024, Z2-R-SCENE-083, Z2-R-SCENE-086, Z2-R-SCENE-093, Z2-R-SCENE-094, Z2-R-SCENE-095 |
| Z2-I-SCENE-05 | required | Z2-R-SCENE-028, Z2-R-SCENE-092, Z2-R-SCENE-100, Z2-R-SCENE-102, Z2-R-SCENE-103, Z2-R-SCENE-104, Z2-R-SCENE-105, Z2-R-SCENE-106, Z2-R-SCENE-107 |
| Z2-I-SCENE-06 | required | Z2-R-SCENE-082, Z2-R-SCENE-120, Z2-R-SCENE-121, Z2-R-SCENE-122, Z2-R-SCENE-123, Z2-R-SCENE-124, Z2-R-SCENE-125, Z2-R-SCENE-126, Z2-R-SCENE-127, Z2-R-SCENE-128, Z2-R-SCENE-129, Z2-R-SCENE-133, Z2-R-SCENE-137, Z2-R-SCENE-138, Z2-R-SCENE-154, Z2-R-SCENE-158, Z2-R-SCENE-159, Z2-R-SCENE-163, Z2-R-SCENE-164, Z2-R-SCENE-165, Z2-R-SCENE-166, Z2-R-SCENE-167, Z2-R-SCENE-169, Z2-R-SCENE-170, Z2-R-SCENE-174, Z2-R-SCENE-175, Z2-R-SCENE-178 |
| Z2-I-SCENE-07 | required | Z2-R-SCENE-005, Z2-R-SCENE-033, Z2-R-SCENE-075, Z2-R-SCENE-081, Z2-R-SCENE-096, Z2-R-SCENE-108, Z2-R-SCENE-109, Z2-R-SCENE-110, Z2-R-SCENE-111, Z2-R-SCENE-112, Z2-R-SCENE-113, Z2-R-SCENE-114, Z2-R-SCENE-115, Z2-R-SCENE-116, Z2-R-SCENE-117, Z2-R-SCENE-118, Z2-R-SCENE-119, Z2-R-SCENE-120, Z2-R-SCENE-122, Z2-R-SCENE-123, Z2-R-SCENE-129, Z2-R-SCENE-130, Z2-R-SCENE-136, Z2-R-SCENE-160, Z2-R-SCENE-161, Z2-R-SCENE-165, Z2-R-SCENE-166, Z2-R-SCENE-167, Z2-R-SCENE-168, Z2-R-SCENE-169, Z2-R-SCENE-170, Z2-R-SCENE-171, Z2-R-SCENE-172, Z2-R-SCENE-173, Z2-R-SCENE-176, Z2-R-SCENE-177 |
| Z2-I-SCENE-08 | required | Z2-R-SCENE-001, Z2-R-SCENE-004, Z2-R-SCENE-025, Z2-R-SCENE-060, Z2-R-SCENE-061, Z2-R-SCENE-062, Z2-R-SCENE-063, Z2-R-SCENE-064, Z2-R-SCENE-065, Z2-R-SCENE-066, Z2-R-SCENE-067, Z2-R-SCENE-068, Z2-R-SCENE-069, Z2-R-SCENE-070, Z2-R-SCENE-071, Z2-R-SCENE-072, Z2-R-SCENE-073, Z2-R-SCENE-074, Z2-R-SCENE-075, Z2-R-SCENE-076, Z2-R-SCENE-077, Z2-R-SCENE-078, Z2-R-SCENE-079, Z2-R-SCENE-080, Z2-R-SCENE-081, Z2-R-SCENE-082, Z2-R-SCENE-118, Z2-R-SCENE-136, Z2-R-SCENE-137, Z2-R-SCENE-142, Z2-R-SCENE-149, Z2-R-SCENE-155, Z2-R-SCENE-156, Z2-R-SCENE-179, Z2-R-SCENE-180, Z2-R-SCENE-181, Z2-R-SCENE-182 |
| Z2-I-SCENE-09 | required | Z2-R-SCENE-005, Z2-R-SCENE-014, Z2-R-SCENE-079, Z2-R-SCENE-119, Z2-R-SCENE-130, Z2-R-SCENE-131, Z2-R-SCENE-132, Z2-R-SCENE-133, Z2-R-SCENE-134, Z2-R-SCENE-135, Z2-R-SCENE-138 |
| Z2-I-SCENE-10 | required | Z2-R-SCENE-001, Z2-R-SCENE-002, Z2-R-SCENE-003, Z2-R-SCENE-004, Z2-R-SCENE-006, Z2-R-SCENE-019, Z2-R-SCENE-023, Z2-R-SCENE-026, Z2-R-SCENE-027, Z2-R-SCENE-029, Z2-R-SCENE-030, Z2-R-SCENE-031, Z2-R-SCENE-032, Z2-R-SCENE-033, Z2-R-SCENE-034, Z2-R-SCENE-035, Z2-R-SCENE-039, Z2-R-SCENE-050, Z2-R-SCENE-052, Z2-R-SCENE-053, Z2-R-SCENE-055, Z2-R-SCENE-109, Z2-R-SCENE-142, Z2-R-SCENE-145, Z2-R-SCENE-146, Z2-R-SCENE-157 |
| Z2-I-CONTENT-28 (CONTENT) | required | Z2-R-SCENE-002, Z2-R-SCENE-015, Z2-R-SCENE-016, Z2-R-SCENE-017, Z2-R-SCENE-018, Z2-R-SCENE-019, Z2-R-SCENE-020, Z2-R-SCENE-021, Z2-R-SCENE-022, Z2-R-SCENE-139, Z2-R-SCENE-141, Z2-R-SCENE-143, Z2-R-SCENE-144, Z2-R-SCENE-145 |
| Z2-I-ECS-03 (ECS: document-level framework half) | required | Z2-R-SCENE-002, Z2-R-SCENE-035, Z2-R-SCENE-036, Z2-R-SCENE-037, Z2-R-SCENE-038, Z2-R-SCENE-039, Z2-R-SCENE-040, Z2-R-SCENE-041, Z2-R-SCENE-042, Z2-R-SCENE-043, Z2-R-SCENE-044, Z2-R-SCENE-045, Z2-R-SCENE-046, Z2-R-SCENE-047, Z2-R-SCENE-048, Z2-R-SCENE-049, Z2-R-SCENE-050, Z2-R-SCENE-051, Z2-R-SCENE-052, Z2-R-SCENE-053, Z2-R-SCENE-054, Z2-R-SCENE-056, Z2-R-SCENE-057, Z2-R-SCENE-058, Z2-R-SCENE-059, Z2-R-SCENE-145, Z2-R-SCENE-146, Z2-R-SCENE-150, Z2-R-SCENE-151, Z2-R-SCENE-153, Z2-R-SCENE-157, Z2-R-SCENE-162 |
| Z2-I-SAVE-01 (SAVE: the additive set's half of versioned saves) | required | Z2-R-SCENE-163, Z2-R-SCENE-164, Z2-R-SCENE-165, Z2-R-SCENE-175, Z2-R-SCENE-178 |
| Z2-I-STREAM-05 (STREAM: the world-origin resource and its shift) | required | Z2-R-SCENE-179, Z2-R-SCENE-180 |
| Z2-I-PHYS-28 (PHYS: the origin-shifted event its physics world shifts on) | required | Z2-R-SCENE-181 |
| Z2-I-ENV-05 (ENV: the fluid-query interface its water implements) | required | Z2-R-SCENE-182 |

## Excluded and deferred

SCENE has no excluded and no deferred inventory item: all ten SCENE items are required in Stages 2 to 4. Recorded boundaries, which carry no requirement:

- Mod-supplied scenes and prefabs are excluded with modding (Z2-D-075).
- Scene types registered by dynamic plugins do not exist; modules are static (Z2-D-024).
- A second parent store or hierarchy component is excluded by design: the parent relation is ECS's (`Zenith2/Docs/Design/SCENE.md` → "What SCENE does not own").
- Networked replication of transforms is NET's Stage 7 work (Z2-D-073); SCENE supplies the transform change bits it reads.
