# SCENE design — authoring identities, prefabs, overrides, variants and reference repair

This file is part of the SCENE design family; the entry document is `Zenith2/Docs/Design/SCENE.md`. It specifies stable authoring identities and their derivation, prefab resolution with nested prefabs, the override block and its application order, variants, runtime instantiation, the cook's resolution, reference repair and the document edits EDIT's command system wraps. The formats are `Zenith2/Docs/Design/SCENE/Format.md` → "Scene document schema"; the types are `Zenith2/Docs/Design/SCENE/SceneAPI.md`.

## Authoring identities

Authoring IDs are `Zenith_PersistentID` values ([Z2-N-CONTRACT-066]); SCENE adds no handle type of its own, because Contracts makes the stable authoring IDs of entities, prefab instances and override targets persistent IDs, and a distinct type would be a sixth handle kind.

| Object | Identity | Issued by | Stable across |
|---|---|---|---|
| document (scene, prefab) | the document's `id` | `Zenith_GeneratePersistentID` at creation (editor, `ZenithNewGame`) | rename, move, migration, cook |
| entity authored in a document | its key in `entities` | generated when the entity is created | reparenting, reordering, renaming, migration, copy between documents only through remapping |
| prefab instance | its key in `instances` | generated when the instance is placed | as above |
| entity created by an instance | `Zenith_DeriveAuthoringID(instanceID, prefabLocalID)` | computed at resolution; stored wherever a document, a cooked payload or a save names that entity (§ "Derivation") | as long as the instance and the prefab entity exist |
| component | (entity ID, stable name) | — | — |
| property | (entity ID, stable name, `Zenith_PropertyPath`) | — | — |

### Derivation

`Zenith_DeriveAuthoringID(xScope, xLocal)` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AuthoringID header"):

<!-- elaboration -->
1. `auBytes` = the 36 bytes `"ZSID"` (ASCII), then `xScope` and `xLocal` in their 16-byte cooked binary form, the high word then the low word, each little-endian ([Z2-N-CONTRACT-067]). `xScope` may be the null ID: that is the scope `Zenith_MakePathDocumentID` uses (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_MigrationRegistry header"), and the null ID is accepted as an input though it is never issued ([Z2-N-CONTRACT-065]).
2. `xDigest = Zenith_Sha256Digest(auBytes)`, CORE's SHA-256 returning a `Zenith_Sha256Value` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256"; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Digests"): one compression block, because 36 bytes plus padding fit in 64.
3. `ulHigh` = digest bytes 0 to 7 read little-endian, `ulLow` = bytes 8 to 15 read little-endian; `m_ulHigh = (ulHigh & ~0xF000ull) | 0x4000ull`, `m_ulLow = (ulLow & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull` — the version-4 layout of RFC 9562, so a derived ID is indistinguishable in form from a generated one and is never null.

Derivation is deterministic across runs, machines and tools, and it depends on no open decision: it uses SHA-256 (FIPS 180-4, fixed forever), not `Zenith_Hash64`, so the D-SEL-HASH (open, Stage 1) ruling changes no derived ID. That independence is required because derived IDs are stored, in three places:

| Where | What holds a derived ID | What a change of derivation would need |
|---|---|---|
| authoring documents | every override target, `parent`, `root` or ENTITY-typed value that names an entity of a nested instance (`Zenith2/Docs/Design/SCENE/Format.md` → "Resolved ID space"): `Town.scene.json` overriding the bulb of a Lamp inside its Street instance keys the override by `Derive(LampInStreet, bulb)`, an ID of Street's resolved space | a rewrite of every document of the game over the full resolution with both derivations; a per-document migration step cannot do it, because the (instance, local) pair behind a stored derived ID lives in the instanced prefab and a step reads no other document ([Z2-N-CONTRACT-538]) |
| cooked payloads | the ENTITIES records (`Zenith2/Docs/Design/SCENE/Format.md` → "Sections") | a re-cook ([Z2-N-CONTRACT-529]) |
| saves | the persistent IDs of instanced entities | a SAVE migration step |

The derivation is therefore a permanent format property, as the persistent ID's JSON form is: it changes only with a decision whose ticket supplies all three rows. `Test_AuthoringID_DeriveIsDeterministicAndVersion4` pins fixed vectors and `Test_AuthoringID_DeriveIndependentOfHash64` proves that no `Zenith_Hash64` call reaches it; `Test_PrefabResolve_NestedOverrideTargetIsStoredDerivedID` shows the document-side storage the table names.

Cost and where it is paid: one derivation is one SHA-256 compression block, several times the cost of a 64-bit non-cryptographic hash of the same 36 bytes, so SCENE never derives on a simulation node in bulk. `Bench_Scene_DeriveAuthoringID` measures derivations per millisecond (a report figure, `Zenith2/Docs/Design/SCENE/Tests.md` → "Benchmarks and the budget harness"). The places that derive:

| Caller | When | Per |
|---|---|---|
| resolution (`Zenith_ResolveDocument`: nested instances) | editor, cook, reference repair; tool threads | entity of a nested instance |
| preparation of a duplicate additive instance (`Zenith_SceneLoadDesc::m_xIDScope` non-null) | `BUILDING`, in background slices (`Zenith2/Docs/Design/SCENE/Loading.md` → "Instance identity") | entity of the instance |
| `Zenith_RecordInstantiatePrefabEx` with a non-null instance ID | record time of its caller, which is SAVE's restore out of tick; every phase-time caller (`ctx:Spawn`, `Scene.Instantiate`, `Zenith_RecordInstantiatePrefab`) passes the null ID and derives nothing | entity of the prefab |
| carry-over capture | out of tick, after the old world's final playback | captured entity without a persistent ID |

Uniqueness: within one resolution the resolver checks every produced ID against the set of IDs produced so far and fails `SCENE_ERROR_DUPLICATE_ID` naming both sources (a 128-bit collision is not expected; the check turns an authoring error — the same instance ID reused — into a diagnostic).

### Identity rules the editor foundation builds on

EDIT's command system (Stage 3) keys selection, undo and document ownership by authoring IDs; SCENE guarantees:

1. An ID never changes while its object exists: saving, loading, migrating, reordering, reparenting and renaming keep it.
2. An ID is never reused for another object of the same document, including after deletion: `Zenith_SceneDocument` keeps the IDs deleted in the session (`GetRetiredIDs`) and refuses to insert one (`SCENE_ERROR_DUPLICATE_ID`).
3. Lookup by ID is O(1): the document keeps a hash index from ID to node.
4. Copy and paste, duplication and cross-document moves go through `Zenith_RemapAuthoringIDs` (§ "Remapping"), which issues fresh IDs and rewrites every reference inside the copied set.
5. Play-in-editor copies the edit world by serialization ([Z2-N-ARCH-766]); the play world's entities carry the same persistent IDs, so the editor maps a play-world entity back to its document object.

### Remapping

`Zenith_RemapAuthoringIDs` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AuthoringID header"): for every entity and instance ID in the copied set, in ascending order, draw a fresh ID; then rewrite `parent` members, ENTITY-typed field values and override targets that name an ID of the set; references to IDs outside the set are kept. The remap table (one `Zenith_AuthoringIDRemap` per ID, in ascending old ID, in the caller's arena) is returned so EDIT can select the pasted objects.

## Resolution algorithm

`Zenith_ResolveDocument` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_PrefabResolver header") produces the flat resolved form every consumer uses (the edit world, the cook, reference repair). `Resolve(P)` of a prefab P:

1. **Cycle and depth.** Push P on the resolution stack; P already on it → `SCENE_ERROR_PREFAB_CYCLE` naming the chain; stack depth above `uSCENE_MAX_PREFAB_NESTING` (16) → `SCENE_ERROR_PREFAB_TOO_DEEP`.
2. **Base.** If `variantOf` = B: R = `Resolve(B)` (the variant chain counts toward `uSCENE_MAX_VARIANT_CHAIN`), then apply P's `base` block to R (§ "Applying an override block"). Otherwise R is empty.
3. **Own entities.** Add P's `entities` with their IDs unchanged; an ID already in R is `SCENE_ERROR_DUPLICATE_ID`.
4. **Nested instances,** in ascending instance ID. For instance J of prefab Q: `R_J = Resolve(Q)`; apply J's `overrides` to `R_J`; re-identify `R_J` (§ "Re-identification") with scope J; set the parent of `R_J`'s root to J's `parent`; add `R_J` to R.
5. **Validation.** Every parent resolves inside R; no cycle; depth ≤ 64; exactly one root and it equals P's `root`.
6. **Cache.** Pop P; store R in the provider's resolved cache keyed by (P, the content digests of P and every document it read), so a document shared by many instances resolves once per session.

`Resolve(S)` of a scene is steps 3–5 with no base and no single-root rule.

### Applying an override block

In this order, the order being part of the contract (`Test_PrefabResolve_OverrideOrder`):

1. `removedEntities`: remove each target and its subtree; a target not in R is kept in the document and reported (`DOCUMENT_WARNING_DANGLING_OVERRIDE`).
2. `removedComponents`: remove each named component from its target; an absent component or target is reported the same way.
3. `addedComponents`: add each component; a component already present is `SCENE_ERROR_SCHEMA` ("added component already present; use properties").
4. `properties`: for each target, component and property path, replace the resolved value with the override's value; a component absent from the target is reported as dangling.

Precedence follows from the recursion: a nested instance's own overrides are applied when its level resolves, the containing prefab's variant block and instance blocks after, and the scene's instance block last, so the outermost override wins. A removed entity's overrides at an outer level are dangling, never errors.

### Re-identification

For every entity e of `R_J` (ID `t`): its new ID is `Zenith_DeriveAuthoringID(J, t)`; its `parent` is re-derived the same way when it names an ID of `R_J`; every ENTITY-typed value inside its known components that names an ID of `R_J` is re-derived (found by walking the component's field set with `Zenith_WalkProperties` over its `Zenith_TypeInfo`); values naming IDs outside `R_J` (null, or IDs the prefab author wrote that do not exist) are kept and reported as `DOCUMENT_WARNING_REFERENCE_UNRESOLVED` by validation. Unknown components are carried unchanged (`Zenith2/Docs/Design/SCENE/Format.md` → "Unknown components").

Complexity: O(total entities × nesting depth) for re-identification, O(E log E) for the ID index; resolution is linear in the resolved size apart from the cache.

### Override semantics per property kind

| Property kind | Override granularity | Merge-conflict behaviour (CONTENT) |
|---|---|---|
| scalar, vector, quaternion, colour, string, persistent ID, entity, asset | the whole value | a textual conflict on the line |
| struct | per nested property ID (a partial nested field set) | per line |
| fixed array | the whole array | an explicit textual-conflict marker (Z2-I-CONTENT-29) |
| container | the whole container | an explicit textual-conflict marker |

## Variants

A variant is a prefab whose `variantOf` names its base: `Resolve(V)` starts from `Resolve(base)`, applies V's `base` block, then adds V's entities and nested instances. Entities of the base keep their IDs, so an instance of V accepts every override written against the base; replacing an instance's `prefab` from B to one of B's variants (or back) is an ID-compatible edit (`Test_PrefabVariant_InstanceOverridesSurviveBaseSwap`). A variant chain V → B → A resolves recursively; a change to A reaches V at the next resolution. A variant may remove base entities and components, but may not declare `root` different from the base's (`SCENE_ERROR_SCHEMA`).

## Prefab provider

`Zenith_PrefabProvider` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_PrefabResolver header") is the interface resolution reads prefab documents and caches resolved data through. Its implementations: the editor's (open documents first, then on-disk documents through its `Zenith_DocumentMigrator`), the cook's (the game's documents through the cook's read-only migrator) and the tests' (in-memory documents). `FindPrefab` returns the document of a prefab ID; `FindResolved` and `StoreResolved` hold the resolved cache keyed by (prefab ID, content key), where the content key is `Zenith_Hash64` over the SHA-256 digests of the prefab and of every document its resolution read, in reading order (a cache key, never persisted, so D-SEL-HASH (open, Stage 1) may change it freely); `Invalidate` drops a prefab's entries when its document or a dependency changed.

A null provider is accepted by resolution and cooking: it resolves a document with no prefab instance and no `variantOf`, which is how scenes cook at Stage 2 and flat prefab documents cook at Stage 3 (§ "Flat prefabs at Stage 3"); a document with an instance or a variant base and a null provider fails `SCENE_ERROR_PREFAB_NOT_FOUND` naming the first such reference.

### Flat prefabs at Stage 3

<!-- elaboration -->
Scripting's `ctx:Spawn(prefab)` ([Z2-N-SCRIPT-220]) is a Stage 3 obligation, while nested prefabs, overrides and variants are Stage 4 (Z2-I-SCENE-02 to -04). The Stage 3 subset that serves it is the flat prefab document: kind PREFAB (`$schema` `zenith2.prefab`), `variantOf` null, no `base` member, `instances` empty, exactly one root. Such a document is loaded and validated by the Stage 2 document model (slice 10), cooked with a null provider by the Stage 2 cooker (slice 11, `Test_SceneCooker_FlatPrefabWithNullProvider`), made resident as a `Zenith.Prefab` asset and instantiated at runtime (slice 18, § "Runtime instantiation"). Nothing of it changes at Stage 4: slice 22 adds the provider, nesting, override blocks and variants on top, and a flat prefab's resolved and cooked form is identical with or without a provider.

## Cooking resolved prefabs

SCENE's two cook steps in ASSET's `ZenithCook` (§ "Scene and prefab cook steps") call `Zenith_CookSceneDocument` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneCooker header") for each `.scene.json` and `.prefab.json` of the game:

1. Open through `Zenith_CookContext::OpenSourceDocument` (a call-local migrator over the verified bytes) (§ "Scene and prefab cook steps"; in memory; the cook never writes documents); a READ_ONLY_NEWER document is `SCENE_ERROR_DOCUMENT` with `DOCUMENT_ERROR_NEWER_VERSION` logged, a cook error.
2. `Zenith_ResolveDocument` against the cook's provider and the game's frozen type registry (the one of `Zenith2/Docs/Design/ECS/Schemas.md` → "Cook integration").
3. An unknown component type is a cook error `SCENE_ERROR_UNKNOWN_TYPE` naming document, entity and stable name, because the game's Player cannot instantiate it; a dangling override or unresolved reference is a cook warning.
4. Add `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` to nothing: derived components are created at activation, not cooked.
5. Order entities for activation (parents before children; siblings by sort key, then ID); encode the payload (`Zenith2/Docs/Design/SCENE/Format.md` → "Cooked scene payload"); list the asset references.
6. Declare the asset references as hard dependencies and put the document and every prefab its resolution read into the step's derived-data key (§ "Scene and prefab cook steps"), so a prefab edit re-cooks every scene and prefab that instances it.

The cooked form of a prefab is its resolved form, rooted at index 0. Cooking is deterministic: the same inputs give identical bytes (`Test_SceneCooker_Deterministic`).

### Scene and prefab cook steps

<!-- elaboration -->
SCENE owns the cooked scene and prefab outputs, so it implements their two `Zenith_CookStep`s (`Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookStep"), which reach ASSET only through `Zenith_CookContext` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookContext"). They compile inside ZenithCook's SCENE directory (`Zenith2/Docs/Design/ASSET/Cook.md` → "Cook services and composition"):

| File (under `Zenith2/Tools/ZenithCook/`) | Contents |
|---|---|
| `Internal/Scene/Zenith_SceneCookSteps.h`, `Internal/Scene/Zenith_SceneCookSteps.cpp` | the two step classes `Zenith_SceneCookStep` and `Zenith_PrefabCookStep` (one static instance each), `Zenith_RegisterSceneCookSteps`, and the per-item provider `Zenith_SceneCookPrefabProvider` |
| `Tests/Scene/Zenith_SceneCookSteps_Tests.cpp`, `Tests/Fixtures/Scene/` | `Test_SceneCookSteps_KeysAndHardReferences` and its fixture game (`Zenith2/Docs/Design/SCENE/Tests.md` → "Cook and load tests") |

```cpp
// Zenith2/Tools/ZenithCook/Internal/Scene/Zenith_SceneCookSteps.h — ZenithCook-internal
class Zenith_CookStepRegistry;
class Zenith_CookServices;
class Zenith_TypeRegistry;
// Includes ZenithCook's Internal/Cook/Zenith_CookStep.h (Zenith_CookStep, Zenith_CookContext) and
// ZenithScene/Zenith_PrefabResolver.h (Zenith_PrefabProvider).

/// @contract functions Zenith_SceneCookStepRegistration
/// Ownership: Registers the two static step instances with the caller's registry; the steps borrow xServices' frozen
///   Zenith_TypeRegistry (services item 3) and root heap (item 1) for the life of the services.
/// Thread affinity: MAIN_THREAD, cook services item 6 (SCENE's place in its fixed order), before the registry's Freeze.
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's COOK_ERROR_INVALID_STEP, returned unchanged after the first refused step.
/// Version: since Z2 Stage 2 (scene.scene) and Stage 3 (scene.prefab, flat prefabs).
/// @endcontract
Zenith_Result<void, Zenith_CookError> Zenith_RegisterSceneCookSteps(Zenith_CookStepRegistry& xRegistry,
	Zenith_CookServices& xServices);

/// @contract type Zenith_SceneCookStep
/// Ownership: One static instance, registered by Zenith_RegisterSceneCookSteps, which sets m_pxServices.
/// Thread affinity: ANY_THREAD. Items run concurrently; a call touches only its context and item-local objects.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: As Zenith_CookStep, with the mapping of "Errors" below.
/// Version: since Z2 Stage 2. Its desc is the scene.scene column of the table below.
/// @endcontract
class Zenith_SceneCookStep final : public Zenith_CookStep
{
public:
	const Zenith_CookStepDesc& GetDesc() const override;
	void AddKeyInputs(Zenith_CookContext& xContext, Zenith_DerivedDataKeyBuilder& xKey) const override;
	Zenith_Result<void, Zenith_CookError> Cook(Zenith_CookContext& xContext) override;
private:
	Zenith_CookServices* m_pxServices = nullptr;
};

/// @contract type Zenith_PrefabCookStep
/// Ownership: As Zenith_SceneCookStep.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: As Zenith_SceneCookStep.
/// Version: since Z2 Stage 3 (flat prefabs; nested prefabs and variants at Stage 4). Its desc is the scene.prefab column of
///   the table below.
/// @endcontract
class Zenith_PrefabCookStep final : public Zenith_CookStep
{
public:
	const Zenith_CookStepDesc& GetDesc() const override;
	void AddKeyInputs(Zenith_CookContext& xContext, Zenith_DerivedDataKeyBuilder& xKey) const override;
	Zenith_Result<void, Zenith_CookError> Cook(Zenith_CookContext& xContext) override;
private:
	Zenith_CookServices* m_pxServices = nullptr;
};

/// @contract type Zenith_SceneCookPrefabProvider
/// Ownership: A stack object of one item's AddKeyInputs or Cook; borrows the item's context and the type registry; the
///   documents it loads and its resolved cache live in the context's scratch arena.
/// Thread affinity: OWNER_THREAD. The item's thread.
/// Invalidation: Every document and resolved datum it returned is invalid when the item's call returns.
/// Units: None.
/// Errors: As Zenith_PrefabProvider; the context's COOK_ERROR_MISSING_REFERENCE maps to SCENE_ERROR_PREFAB_NOT_FOUND and any
///   other context error to SCENE_ERROR_DOCUMENT.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SceneCookPrefabProvider final : public Zenith_PrefabProvider
{
public:
	Zenith_SceneCookPrefabProvider(Zenith_CookContext& xContext, const Zenith_TypeRegistry& xTypes);
	Zenith_Result<const Zenith_SceneDocument*, Zenith_SceneError> FindPrefab(const Zenith_PersistentID& xPrefab) override;
	const Zenith_SceneData* FindResolved(const Zenith_PersistentID& xPrefab, uint64 ulContentKey) override;
	void StoreResolved(const Zenith_PersistentID& xPrefab, uint64 ulContentKey, const Zenith_SceneData& xData) override;
	void Invalidate(const Zenith_PersistentID& xPrefab) override;
private:
	Zenith_CookContext* m_pxContext;
	const Zenith_TypeRegistry* m_pxTypes;
};
```

| `Zenith_CookStepDesc` member | `scene.scene` (`Zenith_SceneCookStep`) | `scene.prefab` (`Zenith_PrefabCookStep`) |
|---|---|---|
| `m_szID` | `"scene.scene"` | `"scene.prefab"` |
| `m_uVersion` | 1 | 1 |
| `m_ePhase` | `COOK_PHASE_ASSET` | `COOK_PHASE_ASSET` |
| `m_eKind` | `ASSET_KIND_SCENE` | `ASSET_KIND_PREFAB` |
| `m_aszRunsAfter`, `m_aszToolVersions`, `m_aszOutputPrefixes` | empty | empty |
| `m_axEngineFallbacks` | empty: the kind's fallback policy is FAIL, so it has no role-0 row (`Zenith2/Docs/Design/ASSET/Cook.md` → "Engine fallbacks") | empty, for the same reason |
| `m_bOutOfProcess` | false | false |
| `m_uPlatformMask` | every platform | every platform |

`m_uVersion` is bumped whenever the step's output changes for the same key inputs; a change of the cooked layout also bumps `uSCENE_COOKED_FORMAT_VERSION` (`Zenith2/Docs/Design/SCENE/Format.md` → "Sections").

Both steps open source documents through ASSET's `Zenith_Result<void, Zenith_CookError> Zenith_CookContext::OpenSourceDocument(const Zenith_AssetGUID& xAsset, Zenith_OpenedDocument& xOut)` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Source documents in cook steps"; cross-area row S24, answered). It opens the source document of an asset of the plan's record snapshot (`Zenith2/Docs/Design/ASSET/Cook.md` → "Plan construction" step 6) through a call-local migrator over the verified bytes, from the item's thread, so no OWNER_THREAD object is shared. An unknown GUID is `COOK_ERROR_MISSING_REFERENCE`; a source changed since the scan is `COOK_ERROR_SOURCE_CHANGED`; a framework error is `COOK_ERROR_STEP_FAILED` with the `Zenith_DocumentErrorCode` as detail; a document newer than the registry opens with state `DOCUMENT_OPEN_READ_ONLY_NEWER` and succeeds. The registration shape is ASSET's (`Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions").

**`AddKeyInputs`** adds, after the driver's rows (`Zenith2/Docs/Design/ASSET/DDC.md` → "Key inputs"), in this order:

1. `AddU32("scene.cookedFormatVersion", uSCENE_COOKED_FORMAT_VERSION)`.
2. The documents the item's resolution reads, in reading order, each once. `Visit(D)`, starting with the item's own asset: if D was already visited, return; otherwise mark it, open it with `OpenSourceDocument` and load it with `Zenith_SceneDocument::Load` against the services' frozen type registry, then `AddGUID("scene.document", D)` and `AddString("scene.document.sha256", <64-character lowercase hex of GetFileDigest(), written with Zenith_FormatSha256Hex>)`; then, for a prefab whose `GetVariantOf()` is not null, `Visit(base)`; then for each instance of `ListInstances`, in ascending instance ID, `Visit(instance.m_xPrefab)`. This is the order of § "Resolution algorithm" (the variant base in step 2 before the nested instances of step 4, instances ascending), so the key lists exactly the documents `Zenith_ResolveDocument` reads, in first-read order.
3. A document that cannot be opened or loaded adds `AddGUID("scene.document.unreadable", D)` and `AddU32("scene.document.error", <its Zenith_SceneErrorCode, or 0 for a context error>)` and is not descended into; the walk descends at most `uSCENE_MAX_PREFAB_NESTING` + 1 levels and a revisit adds nothing, so a cycle or an over-deep chain still gives a finite key. `Cook` then fails with the same error, so the key of a failing item is still a function of its inputs alone.

A prefab edit therefore changes the key of every scene and prefab whose resolution reads it, and re-cooks exactly those (cross-area row AS11: prefab dependencies are derived-data key inputs).

**`Cook(xContext)`**:

1. Open the item's document (`OpenSourceDocument(xContext.GetAsset()->m_xGUID)`) and load it as in step 2 above. A document of the other kind (a prefab under `scene.scene`, a scene under `scene.prefab`) is `SCENE_ERROR_SCHEMA`; a `DOCUMENT_OPEN_READ_ONLY_NEWER` document is `SCENE_ERROR_DOCUMENT` with `DOCUMENT_ERROR_NEWER_VERSION` logged (§ "Cooking resolved prefabs" step 1).
2. Create the item's `Zenith_SceneCookPrefabProvider`, the cook's `Zenith_PrefabProvider` (§ "Prefab provider"). `FindPrefab` opens and loads through `OpenSourceDocument`, refuses a document whose kind is not PREFAB (`SCENE_ERROR_PREFAB_NOT_FOUND`) and maps `COOK_ERROR_MISSING_REFERENCE` to `SCENE_ERROR_PREFAB_NOT_FOUND`; it keeps each loaded document for the item. `FindResolved` and `StoreResolved` hold an item-local cache in `GetScratch()`, and `Invalidate` drops a prefab's entries. Nothing is shared between items, so concurrent items need no lock. Also create the item's `Zenith_StringTable` with `Zenith_StringTable::Create({ m_uMaxStrings = 1,048,576, m_ulMaxTextBytes = 67,108,864 })`, keeping the default page and chunk sizes (`Zenith2/Docs/Design/CORE/Text.md` → "String interning").
3. `Zenith_CookedFileWriterDesc xDesc = xContext.MakeWriterDesc(uMagic, uSCENE_COOKED_FORMAT_VERSION)`, where `uMagic` is `Zenith_GetAssetKindInfo(eKind)->m_uMagic` (`ZSCN` or `ZPFB`, `Zenith2/Docs/Design/ASSET.md` → "Asset kinds"); set `xDesc.m_pxCodecs = &xContext.GetCodecs()` and `xDesc.m_pxHeap` to the services' root heap; construct the `Zenith_CookedFileWriter` from it (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_CookedFileWriter").
4. Call `Zenith_CookSceneDocument(document, &provider, types, strings, settings, xContext.GetScratch(), writer, xInfo, xDiagnostics)`. `settings` is the `Zenith_SceneCookSettings` of the asset's resolved compression choice (`xContext.GetSettings()`, COMPRESS's per-asset-class choice, `Zenith2/Docs/Design/COMPRESS.md` → "Cook integration"). Every stored diagnostic is then reported with `xContext.Report`: `COOK_DIAGNOSTIC_LEVEL_ERROR` when `IsError(i)`, else `COOK_DIAGNOSTIC_LEVEL_WARNING`, with `GetMessage(i)` as the text.
5. Declare the references: `xContext.DeclareReference(id, ASSET_DEPENDENCY_FLAG_HARD)` for every element of `xInfo.m_axAssetReferences` (the ASSET_REFS records, in their order), and in a scene also for every element of `xInfo.m_axInstancedPrefabs` (the prefabs its own instances name, ascending). A prefab declares only its ASSET_REFS set: its nested prefabs and its variant base are resolved into it, so its hard dependencies are exactly the records the prefab kind loader matches its handles to (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneAssets header"). The catalogue therefore lists these as hard dependencies (cross-area row AS11), and ASSET loads them before the dependent's loader runs.
6. `writer.Finish(xBytes)`, then `xContext.WriteAssetFile(xBytes, xInfo.m_ulSectionBytes)`: the resident estimate is the decoded bytes of the six sections.

Errors: a `Zenith_SceneError` is `COOK_ERROR_STEP_FAILED`, with the `Zenith_SceneErrorCode` as `m_uDetail` and the item's GUID as `m_xAsset`. `SCENE_ERROR_OUT_OF_MEMORY`, and the string table's or writer's out-of-memory, are `COOK_ERROR_OUT_OF_MEMORY`. An error of the context (`OpenSourceDocument`, `WriteAssetFile`) is returned unchanged. A failed item writes no file. `Test_SceneCookSteps_KeysAndHardReferences` covers the registration, the key order, the references and the re-cook set.

## Runtime instantiation

`Zenith_RecordInstantiatePrefab` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleCommands header"; [Z2-N-ARCH-545]; `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Spawn") records ordinary ECS records into the caller's buffer, so the instance is structural playback at step 6 of the sync point after the issuing phase and consumes merge-time capacity like any spawn. From Stage 3 it instantiates every cooked prefab, which at Stage 3 means a flat one (§ "Flat prefabs at Stage 3"); a cooked prefab is already resolved, so nesting adds no runtime step at Stage 4:

1. The prefab's cooked data must be resident as simulation observes it (the asset's published state, pending ASSET); otherwise nothing is recorded, the null ticket is returned and `scene.instantiate.not_resident` is counted — scripts see nil from `ctx:Spawn`.
2. `Reserve(bytes)` for the whole instance; `ECS_ERROR_CAPACITY` → nothing recorded, the null ticket, `scene.instantiate.too_large` (so an instance is never partially recorded).
3. For each entity i in activation order: `Spawn({components, null persistent ID, parent = the ticket handle of its parent's spawn or null})`; components are the decoded field sets converted to `Zenith_ComponentInit`s (strings interned, asset references converted to handle bytes through the prefab's already-held dependency handles); the root's `Zenith.LocalTransform` is `Zenith_ComposePlacement(xAt, authoredRootLocal)`; every entity gets the three transform components when it has a local transform (`Zenith_MakeTransformInits`), and `Zenith.EntityName` when named.
4. ENTITY-typed values referring to entities of the same instance are recorded as the ticket handles of those spawns.
5. Return the root's ticket.

<!-- assumption: pending ECS -->
Step 4 relies on ECS resolving a ticket handle of the same buffer inside an ENTITY-typed property value of a `SPAWN`, `ADD_COMPONENT` or `SET_PROPERTY` record at playback, as it resolves a ticket parent (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandBuffer header" accepts tickets for `m_xParent` only); cross-area note S8.
<!-- /assumption -->

Runtime-instantiated entities have null persistent IDs; SAVE gives saved runtime entities its own identities. `Zenith_RecordInstantiatePrefabEx` takes a `Zenith_PrefabSpawnDesc` (placement, parent handle, a persistent instance ID from which every entity's persistent ID is derived with `Zenith_DeriveAuthoringID` — used by scene activation and by SAVE's restore — and whether to add the transform components) and follows the same steps; a prefab whose cooked data carried an unknown type never became resident (the cook refuses it, § "Cooking resolved prefabs"), so no unknown-type policy applies here.

## Reference repair

CONTENT triggers repairs (asset rename, move and delete, Z2-I-CONTENT-19 to -21); SCENE resolves what the repair means for scene and prefab documents and produces the edits, which CONTENT's recoverable batch operation writes through `Zenith_DocumentMigrator::SaveDocuments`.

`Zenith_PlanReferenceRepair` and `Zenith_ReferenceIndex` are declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_ReferenceRepair header": the plan reads the documents through the caller's migrator, finds the referencing documents through the index and resolves instances through the provider.

| Request kind | Effect on scene and prefab documents |
|---|---|
| `REFERENCE_REPAIR_ASSET_MOVED` (rename or move) | IDs are unchanged, so no reference breaks; every prefab-instance `hint` naming the old path is rewritten to the new path; the plan lists the documents touched |
| `REFERENCE_REPAIR_ASSET_DELETED` with a replacement ID | every asset-typed field value, override value and prefab-instance `prefab` naming the deleted ID is rewritten to the replacement (which must have the same asset kind, else `SCENE_ERROR_SCHEMA`); instance overrides are kept and re-validated against the replacement prefab, dangling ones reported |
| `REFERENCE_REPAIR_ASSET_DELETED` without a replacement | asset-typed values become null; a prefab instance of the deleted prefab is converted to its resolved entities (`Zenith_UnpackPrefabInstance`, below) when `m_bUnpackInstances`, else kept and reported as a missing prefab (the scene then fails to cook until repaired) |
| `REFERENCE_REPAIR_ENTITY_DELETED` (a prefab's entity removed by an edit of the prefab) | overrides targeting it in every instancing document are kept and flagged dangling ([Z2-N-CONTRACT-547]); `m_bPruneDangling` removes them as an explicit edit recorded in the plan |
| `REFERENCE_REPAIR_DUPLICATE_DOCUMENT` (a copied file shares its `id`) | the newer copy gets a fresh document ID; references keep pointing at the original |

`Zenith_ReferenceIndex` (built by scanning documents once) maps every persistent ID to the (document, JSON path) pairs that reference it, so a plan is O(references) after the scan. The plan carries, per document, the new canonical bytes, the diff and the expected old digest, and is written as one journaled batch; nothing is written by planning.

`Zenith_UnpackPrefabInstance` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_PrefabResolver header") replaces an instance by its resolved entities as ordinary entities with the derived IDs (so references to them stay valid) and the overrides folded into their component values, and returns the inverse edit, a `SCENE_EDIT_PLACE_INSTANCE` holding the original instance object and the IDs of the unpacked entities it removes, so EDIT can undo it.

## Document edits for the editor

EDIT's command system wraps these edits; each edit applied to a `Zenith_SceneDocument` returns its inverse, so undo is applying the inverse (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneEdit header"):

| Edit | Inverse |
|---|---|
| `SCENE_EDIT_CREATE_ENTITY` (ID, parent, sort key, name; components follow as `SCENE_EDIT_ADD_COMPONENT` edits of the same EDIT command) | `SCENE_EDIT_DELETE_ENTITY` of it |
| `SCENE_EDIT_DELETE_ENTITY` (ID; its subtree) | `SCENE_EDIT_CREATE_SUBTREE` holding the removed objects verbatim |
| `SCENE_EDIT_SET_PARENT` (ID, new parent, new sort key) | the old parent and sort key |
| `SCENE_EDIT_RENAME` (ID, name) | the old name |
| `SCENE_EDIT_SET_PROPERTY` (target, stable name, property path, value) | the old value; on an instance entity it writes or removes an override, and the inverse restores the previous override state (present with a value, or absent) |
| `SCENE_EDIT_ADD_COMPONENT` / `SCENE_EDIT_REMOVE_COMPONENT` | the other, with the removed component's full value |
| `SCENE_EDIT_PLACE_INSTANCE` (instance ID, prefab ID, hint, parent, sort key, override block; optionally the IDs of entities it replaces, used by the inverse of an unpack) / `SCENE_EDIT_DELETE_INSTANCE` | the other, with the removed instance's full object |
| `SCENE_EDIT_REVERT_OVERRIDE` (target, stable name, property path) | re-adds the reverted override value |
| `SCENE_EDIT_APPLY_OVERRIDES_TO_PREFAB` (instance, selection) | a two-document edit: the prefab gains the values, the instance loses the overrides; its inverse restores both |

An edit never changes an ID other than those it creates or deletes, and applying an edit then its inverse leaves the document's canonical bytes unchanged (`Test_SceneEdit_InverseRestoresBytes`). The full builder list is `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneEdit header".

### Sort keys for edits

<!-- elaboration -->
Every edit that places an object among siblings (`CREATE_ENTITY`, `SET_PARENT`, `PLACE_INSTANCE`, `CREATE_SUBTREE`, and paste) takes its sort key from the caller, and the caller computes it with SCENE's functions (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header", `functions Zenith_SortKeyFunctions`; the algorithm is `Zenith2/Docs/Design/SCENE/Format.md` → "Sort key insertion algorithm"):

1. EDIT lists the destination's children in sibling order with `Zenith_SceneDocument::ListChildrenInSiblingOrder` and takes the keys of the two neighbours of the insertion point (an empty view for "before the first" or "after the last").
2. `Zenith_MakeSortKeyBetween(strBefore, strAfter, xOut)` returns a key strictly between them; the edit is built with it, so the insertion rewrites only the new object's lines.
3. Before calling it, EDIT checks the destination group for equal keys: the keys of the listed children are in sibling order, so two adjacent equal keys anywhere in the group (the state `DOCUMENT_WARNING_DUPLICATE_SORT_KEY` reports at load) are found in the same O(group) pass. A group holding equal keys is rebalanced (step 4) whatever the insertion point, because an insertion between two equal keys has no key strictly between them (`Zenith_MakeSortKeyBetween` returns `SCENE_ERROR_SCHEMA` for `strBefore >= strAfter`) and an insertion elsewhere would leave the duplicates in place.
4. Rebalance, also taken when step 2 fails with `SCENE_ERROR_LIMIT` (no key of at most 64 characters lies between the neighbours): `Zenith_MakeEvenSortKeys(uCount, xArena, astrOut)` gives `uCount` evenly spaced keys for the group in its current sibling order with the new object at its insertion point, and EDIT applies one `SCENE_EDIT_SET_PARENT` per sibling whose key changes (parent unchanged, the new key), in sibling order, followed by the placing edit, as one undo step; the rewritten objects are exactly that sibling group, and the edit set lists their IDs. Nothing rewrites a group at load or save.

`SCENE_EDIT_SET_PARENT` and its builder `Zenith_SceneEdit::SetParent` accept the ID of an entity or of a prefab instance, because both are siblings in one group; applied to an instance ID, the edit sets the instance object's `parent` and `sortKey`.

Paste (`Zenith_PasteEditSet`) places the pasted roots after the destination's last child, each with `Zenith_MakeSortKeyBetween` of the preceding key (the destination's last child for the first pasted root, then the previous pasted root) and the empty upper bound; a `SCENE_ERROR_LIMIT` there, or a destination group holding equal keys, rebalances as step 4 does.
