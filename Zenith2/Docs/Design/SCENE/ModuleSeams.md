# SCENE design — module seams: the activation hook, the additive-set save entries, the world origin and the fluid query

This file is part of the SCENE design family; the entry document is `Zenith2/Docs/Design/SCENE.md`. It specifies the ZenithScene declarations and behaviour that layer-7 modules and ZenithRuntime build on and that the Stage 2 and Stage 3 listings of `Zenith2/Docs/Design/SCENE/SceneAPI.md` do not carry: STREAM's activation hook on additive loads (cross-area row ST2 (a) to (j)), the additive set's save enumeration and restore entry (row SV4), the world-origin resource and its shift command (rows ST3 and PH20), the fluid-query interface that PHYS reads and ENV implements (rows EN26 and PH8), and the collision-query interface that GAME's cameras read and PHYS implements (rows GA2 (a) and GA3 (a); added 2026-10-17). It was written by the SCENE post-signoff patch of 2026-10-09. The requirements are `Zenith2/Docs/Requirements/SCENE.md` (Z2-R-SCENE-163 to -165, Stage 3) and `Zenith2/Docs/Requirements/SCENE/Stage4Plus.md` (Z2-R-SCENE-166 to -182, Stage 4); the tests are `Zenith2/Docs/Design/SCENE/Tests.md` → "Module seam tests".

## Scope of the module seams

| Seam | Header | Implemented or consumed by | Stage |
|---|---|---|---|
| additive-set save enumeration and restore entry | `Zenith_AdditiveScenes.h` (additions) | SAVE's `Zenith.Scene.AdditiveSet` participant in ZenithRuntime (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations") calls them | S3 |
| the activation hook, the component amender, the hook tag, the discard resolver, the error detail and the coherence input | `Zenith_AdditiveScenes.h`, `Zenith_SceneLoader.h`, `Zenith_SceneWorld.h` (additions) | STREAM implements the hook (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingActivationHook"); ZenithRuntime implements the resolver member of the sink (row ST21); SAVE's participant reads the coherence input | S4 |
| the world origin | `Zenith_WorldOrigin.h` (new) | STREAM records shifts (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Rebase rule"); PHYS, AI, AUDIO, RPROD and ENV read the resource or listen for the event | S4 |
| the fluid query | `Zenith_FluidQuery.h` (new) | ENV implements it (`Zenith2/Docs/Design/ENV/WaterWeather.md` → "Fluid query provider"); PHYS's buoyancy reads it (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Buoyancy algorithm") | S4 |
| the collision query | `Zenith_CollisionQuery.h` (new) | PHYS implements it (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_CollisionQuery"); GAME's `Gameplay.Cameras` reads it (`Zenith2/Docs/Design/GAME/Cameras.md` → "Collision resolution") | S3 |

Every declaration below obeys `Zenith2/Docs/Design/SCENE.md` → "API rules": one contract block per type, no STL, no `Internal/` type in a public class. The additions to existing headers are listed as additions; the Stage 2 and Stage 3 forms of those headers stay as `Zenith2/Docs/Design/SCENE/SceneAPI.md` lists them, and the API-only slices that land these additions are separate from the slices that created the headers.

## Activation hook declarations

```cpp
// Zenith2/Engine/ZenithScene/Zenith_AdditiveScenes.h — additions (Stage 3: the save entries; Stage 4: the rest)
// added includes: "ZenithCore/Zenith_Vector.h", "ZenithCore/Zenith_Array.h", "ZenithReflect/Zenith_ReflectIDs.h",
// "ZenithECS/Zenith_ECSTypes.h" (for Zenith_ComponentInit); added forward declarations: class Zenith_Arena; class Zenith_TypeRegistry;

/// @contract value Zenith_SavedSceneInstance
/// Ownership: Plain value; m_axRoots borrows the caller's handles for the call that receives it.
/// Thread affinity: IMMUTABLE.
/// Invalidation: m_axRoots is invalid when the visit callback or RestoreInstances returns.
/// Units: m_ulCompletionID a completion ID of the world (0 when none); m_ulHookTag the implementer's opaque tag.
/// Errors: None of its own; RestoreInstances validates it.
/// Version: since Z2 Stage 3. The field set is what SAVE's Zenith.Scene.AdditiveSet section stores.
/// @endcontract
struct Zenith_SavedSceneInstance
{
	const Zenith_SceneInstanceID m_xInstance = {};
	const Zenith_PersistentID m_xScene = {};
	const Zenith_SceneInstanceState m_eState = SCENE_INSTANCE_STATE_ACTIVE; // ACTIVE, FAILED or LOADING (QUEUED is saved as LOADING)
	const Zenith_SceneErrorCode m_eError = static_cast<Zenith_SceneErrorCode>(0); // FAILED: its code; 0 otherwise
	const uint8 m_uErrorDetail = 0;              // FAILED: GetErrorDetail; 0 otherwise
	const bool m_bAllowDuplicate = false;
	const bool m_bDerivedScope = false;           // true: the instance's IDs were derived with its instance scope at commit
	const Zenith_UnknownTypePolicy m_eUnknownTypePolicy = UNKNOWN_TYPE_POLICY_FAIL;
	const uint64 m_ulCompletionID = 0;           // LOADING: the in-flight ASYNC_SOURCE_SCENE_LOAD completion ID; 0 otherwise
	const uint64 m_ulHookTag = 0;                // 0: no hook call for this instance
	const Zenith_Span<const Zenith_EntityHandle> m_axRoots = {}; // ACTIVE: the roots in ascending simulation ID; empty otherwise
};

/// @contract functions Zenith_PfnVisitSceneInstanceType
/// Ownership: A function pointer passed to Zenith_AdditiveSceneSet::ForEachInstance with the caller's user data.
/// Thread affinity: The thread that calls ForEachInstance, synchronously.
/// Invalidation: xInstance is valid for the call only.
/// Units: None.
/// Errors: None.
/// Reentrancy: NONE. The callback never mutates the set it visits.
/// Version: since Z2 Stage 3.
/// @endcontract
using Zenith_PfnVisitSceneInstance = void (*)(const Zenith_SavedSceneInstance& xInstance, void* pUserData);

/// @contract type Zenith_SceneComponentAmender
/// Ownership: Constructed by SCENE's preparation on its stack for one AmendComponents call; borrowed by the hook for that
///   call only. It borrows the entity's init list, the operation's arena and the type registry.
/// Thread affinity: OWNER_THREAD (the preparation thread).
/// Invalidation: Invalid when AmendComponents returns.
/// Units: Component type IDs.
/// Errors: None returned; an unregistered type ID is ignored and counted (scene.amend.unknown_types); an allocation
///   failure in the operation's arena fails the preparation with SCENE_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_SceneComponentAmender
{
public:
	Zenith_SceneComponentAmender(Zenith_Array<Zenith_ComponentInit>& axInits, Zenith_Arena& xArena,
		const Zenith_TypeRegistry& xTypes);
	// Appends the type's default-constructed init block to the entity's init record; a no-op when the entity has the type.
	void AddComponent(Zenith_TypeID xType);
	// Drops the type's init block from the entity's init record; a no-op when the entity lacks the type.
	void RemoveComponent(Zenith_TypeID xType);
	bool HasFailed() const;               // an arena allocation failed during this call
	uint32 GetUnknownTypeCount() const;  // unregistered type IDs ignored during this call
private:
	Zenith_Array<Zenith_ComponentInit>* m_paxInits = nullptr;
	Zenith_Arena* m_pxArena = nullptr;
	const Zenith_TypeRegistry* m_pxTypes = nullptr;
	uint32 m_uUnknownTypes = 0;
	bool m_bFailed = false;
};

/// @contract interface Zenith_SceneActivationHook
/// Ownership: Implemented above ZenithScene (STREAM); borrowed by Zenith_SceneWorldState for the world's lifetime and by
///   each committed load that captured it (obligation (d) below) until that load's last call.
/// Thread affinity: Each member on the thread its comment names.
/// Invalidation: None.
/// Units: ulTag is the implementer's opaque tag from Zenith_AdditiveLoadDesc::m_ulHookTag (0 = no hook call for that load).
/// Errors: None; the implementer answers every call.
/// Reentrancy: NONE. No member calls back into SCENE, except the amender's two members during AmendComponents.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_SceneActivationHook
{
public:
	virtual ~Zenith_SceneActivationHook() = default;
	// Sync-point node, Scene.LoadAdditive playback (step 7), in merge order: the commit of a load with this tag
	// (xInstance non-zero) or its refusal (eError SCENE_ERROR_LIMIT or SCENE_ERROR_DUPLICATE_INSTANCE). Also called on the
	// main thread when SAVE's restore re-requests a pending load with its saved tag, after ENTITIES were restored (xInstance 0
	// and the refusal's code when SCENE refuses the re-request, obligation (j)).
	virtual void OnLoadCommitted(uint64 ulTag, Zenith_SceneInstanceID xInstance, Zenith_SceneErrorCode eError) = 0;
	// The world's tick-boundary handler, step 1, in completion-ID order: the publication of a tagged load whose instance
	// became UNLOADED while LOADING (obligations (a) and (b)).
	virtual void OnDiscardedPublication(uint64 ulTag) = 0;
	// Preparation thread, after DEPENDENCIES and before BUILDING (a GATED state between them): false keeps the operation
	// waiting and re-polls at its next slice, which is a cancellation point; BUILDING starts only once true.
	virtual bool IsPreparationGateOpen(uint64 ulTag) = 0;
	// Preparation thread, BUILDING (so always after the gate opened), once per entity in cooked order: true removes the
	// entity and its descendants from the prepared scene; ENTITY values naming a removed entity become null and count
	// scene.references.skipped (no log line).
	virtual bool ShouldSkipEntity(uint64 ulTag, const Zenith_PersistentID& xEntity) = 0;
	// Preparation thread, BUILDING, once per entity ShouldSkipEntity kept, before the entity's init record is final: the
	// implementer adds or removes components of the entity's init record through xAmender (obligation (e)).
	virtual void AmendComponents(uint64 ulTag, const Zenith_PersistentID& xEntity, Zenith_SceneComponentAmender& xAmender) = 0;
	// Exactly once per committed load with this tag, after its last call above and before its operation parks its
	// publication, whatever the status (SUCCEEDED, FAILED, CANCELLED): on the preparation thread; on the main thread in
	// StepActivations for a start the loader refused (operation table full, out of memory); on the sync-point node in the
	// Scene.ForgetInstance playback for a pending preparation removed before it started (obligation (c)).
	virtual void OnPreparationEnded(uint64 ulTag) = 0;
	// Scene.ActivateAdditive node (phase 1), for each QUEUED instance with this tag, in instance-ID order, before its spawns
	// are recorded: false leaves it QUEUED for the next tick and does not count against scene.activationRetryTicks.
	virtual bool AdmitActivation(uint64 ulTag, Zenith_SceneInstanceID xInstance) = 0;
	// Same node, for an admitted instance: a translation SCENE adds to every root's Zenith.LocalTransform position, and to
	// every recorded Zenith.WorldTransform and Zenith.PreviousWorldTransform position, of the instance's spawns.
	virtual Zenith_Vector3 GetRootTranslation(uint64 ulTag) = 0;
};

/// @contract functions Zenith_PfnScenePublicationResolveType
/// Ownership: A function pointer SCENE passes to SetPublicationResolver with its per-world user data.
/// Thread affinity: The world's tick-boundary node, step 1, live mode only, once per drained completion of
///   ASYNC_SOURCE_SCENE_LOAD in completion-ID order, after the drain-and-sort and before the publications are logged.
/// Invalidation: pPayload and the two references are valid for the call only.
/// Units: Bytes; uPublicationKind a Zenith_PublicationKind value carried as uint8; uPayloadCapacity is 8.
/// Errors: None; a completion SCENE does not recognise is left unchanged.
/// Reentrancy: NONE. It reads only SCENE's per-world records and calls nothing back.
/// Version: since Z2 Stage 4. Declared before the interface that takes it.
/// @endcontract
using Zenith_PfnScenePublicationResolve = void (*)(uint64 ulCompletionID, uint8& uPublicationKind, uint8* pPayload,
	uint32 uPayloadCapacity, uint32& uPayloadBytes, void* pUserData);

// Zenith_AdditiveLoadDesc gains (Stage 3):  uint64 m_ulHookTag = 0;   // 0: no hook call for this load
// Zenith_AdditiveSceneSet gains (Stage 3, row SV4):
//   // Any thread with no node of the world running, or a node declaring a read of "Zenith.SceneActivationQueue":
//   // visits every LOADING, QUEUED, ACTIVE and FAILED record in ascending instance ID (§ "Saved instance enumeration").
//   void ForEachInstance(Zenith_PfnVisitSceneInstance pfnVisit, void* pUserData) const;
//   // MAIN_THREAD, out of tick, into a world whose set holds no record (§ "Restore algorithm").
//   Zenith_Result<void, Zenith_SceneError> RestoreInstances(Zenith_Span<const Zenith_SavedSceneInstance> axInstances,
//       uint32 uNextInstanceID);
// Zenith_AdditiveSceneSet gains (Stage 4):
//   Zenith_Span<const Zenith_SceneInstanceID> GetLiveInstances() const;   // obligation (f)
//   uint8 GetErrorDetail(Zenith_SceneInstanceID xInstance) const;        // obligation (i); 0 when none
//   bool IsCaptureCoherentOnly() const;                                   // obligation (g)
// Zenith_ScenePublicationSink gains (Stage 4):
//   virtual void SetPublicationResolver(Zenith_PfnScenePublicationResolve pfnResolve, void* pUserData) = 0;
//   // MAIN_THREAD, once, in Zenith_SceneWorldState::Attach, after SetPublishHandler (obligation (b), row ST21)
// The hook tag is part of an instance's pending-load record, so SAVE saves it for a LOADING or QUEUED instance and its
// restore re-requests the load with it (rows ST14, SV10); an ACTIVE instance is restored from ENTITIES and calls no hook.
```

The record private to `Zenith_AdditiveSceneSet` (Z2T-SCENE-026's nested `Record`) gains `uint64 m_ulHookTag; Zenith_SceneActivationHook* m_pxCapturedHook; uint8 m_uErrorDetail; bool m_bAllowDuplicate; bool m_bDerivedScope; Zenith_UnknownTypePolicy m_eUnknownTypePolicy; bool m_bDiscardRequested;`, and the set gains `Zenith_Array<Zenith_SceneInstanceID> m_axLive` (the sorted live IDs `GetLiveInstances` returns), `bool m_bTaggedCommitRefusedThisTick` and a back pointer `Zenith_SceneWorldState* m_pxOwner` (set by the world state at `Create`), through which `RestoreInstances` appends pending preparations. All are public types, held by value or as borrowed pointers to public types.

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneLoader.h — additions (Stage 4)

/// @contract enum Zenith_ScenePreparationStage
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_ScenePreparationStage : uint8
{
	SCENE_PREPARATION_STAGE_DECODING = 1,
	SCENE_PREPARATION_STAGE_DEPENDENCIES,
	SCENE_PREPARATION_STAGE_BUILDING,
};

/// @contract functions Zenith_PfnScenePreparationProbeType
/// Ownership: A function pointer passed to Zenith_SceneLoader::SetPreparationProbe with the caller's user data.
/// Thread affinity: ANY_THREAD: called on preparation threads, possibly concurrently for different operations.
/// Invalidation: None.
/// Units: xInstance 0 for a preparation that is not an additive load.
/// Errors: None.
/// Reentrancy: NONE. The probe calls nothing of SCENE.
/// Version: since Z2 Stage 4. A test seam; no production composition installs a probe.
/// @endcontract
using Zenith_PfnScenePreparationProbe = bool (*)(Zenith_SceneInstanceID xInstance, Zenith_ScenePreparationStage eStage,
	void* pUserData);

// Zenith_SceneLoader gains:
//   // MAIN_THREAD, while GetLiveOperationCount() is 0 (ZENITH_ASSERT otherwise); null removes it. Called at the first slice
//   // of DECODING, DEPENDENCIES and BUILDING of every preparation: false re-submits the slice unchanged (a cancellation
//   // point, never blocking) and calls the probe again at the next slice (§ "Preparation probe and hold").
//   void SetPreparationProbe(Zenith_PfnScenePreparationProbe pfnProbe, void* pUserData);

// Zenith2/Engine/ZenithScene/Zenith_SceneWorld.h — additions
// Zenith_SceneWorldState gains (Stage 3): Zenith_AdditiveSceneSet& GetAdditiveScenes();
//   // MAIN_THREAD, out of tick: the set RestoreInstances is called on
// Zenith_SceneWorldState gains (Stage 4):
//   void SetActivationHook(Zenith_SceneActivationHook* pxHook);   // MAIN_THREAD, attach/detach
//   const Zenith_WorldOrigin& GetWorldOrigin() const;   // the world's "Zenith.WorldOrigin" resource
```

`Zenith_SceneError::m_uDetail`'s comment reads "the `Zenith_DocumentErrorCode` or `Zenith_ECSErrorCode` for DOCUMENT and ECS; the `Zenith_AssetErrorCode` for ASSET_UNAVAILABLE" (obligation (i)).

## Activation hook obligations

The obligations are STREAM's, from `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "SCENE activation hook", applied here in SCENE's terms. A load is tagged when its commit carried a non-zero `m_ulHookTag`. Untagged loads behave exactly as `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive scenes" specifies; a null hook makes no call, and a load committed while the hook is null behaves as untagged from then on.

### Hook call points

| Member | Where SCENE calls it | Hook used |
|---|---|---|
| `OnLoadCommitted(tag, instance, 0)` | the `Scene.LoadAdditive` playback (sync-point step 7, merge order) after it created the LOADING record | the world's current hook |
| `OnLoadCommitted(tag, 0, code)` | the same playback when it refuses the commit: `SCENE_ERROR_LIMIT` (the set full, or `AllocateCompletionID` returned 0) or `SCENE_ERROR_DUPLICATE_INSTANCE`; the playback also sets the set's "tagged commit refused this tick" flag | current |
| `OnLoadCommitted(tag, instance, 0)` or `(tag, 0, code)` | `RestoreInstances` on the main thread, for a LOADING saved instance with a non-zero tag (§ "Restore re-requests and the hook") | current |
| `IsPreparationGateOpen(tag)` | the preparation thread, each slice of the GATED state (§ "Gated preparation state") | captured |
| `ShouldSkipEntity(tag, id)` | the preparation thread, BUILDING pass 1, once per entity in cooked order whose ancestors were all kept | captured |
| `AmendComponents(tag, id, amender)` | the preparation thread, BUILDING pass 2, once per kept entity, before its init record is final | captured |
| `OnPreparationEnded(tag)` | exactly once per committed tagged load (§ "Hook captured per load") | captured |
| `AdmitActivation(tag, instance)` | `Scene.ActivateAdditive` (phase 1), per QUEUED tagged instance in instance-ID order, before recording its spawns; false leaves it QUEUED for the next tick and does not advance its retry count | current |
| `GetRootTranslation(tag)` | the same node, for an admitted instance | current |
| `OnDiscardedPublication(tag)` | the publication handler (tick boundary step 1, completion-ID order) for a discarded tagged load (§ "Discarded publications of tagged loads") | captured |

`GetRootTranslation` returns t; the activation adds t, in binary32 per component (`p + t`), to every root's `Zenith.LocalTransform` position, and to the position and the translation column (the `w` components of `Row0`, `Row1`, `Row2`) of every recorded `Zenith.WorldTransform` and the position of every recorded `Zenith.PreviousWorldTransform` of the instance's spawns, before `Reserve` sizes the buffer (the sizes do not change). A rejected activation buffer is recorded again next tick with the translation the hook then returns.

The playback of a refused tagged commit cannot write world state on the hook's behalf, which is why the refusal flag exists (§ "Live instances and capture coherence"). Script loads carry no tag: `Scene.LoadAdditive` from Luau has no tag argument, and `AcceptScriptLoadRequest` commits with tag 0.

### Gated preparation state

The preparation state machine of `Zenith2/Docs/Design/SCENE/Loading.md` → "Preparation state machine" gains, for tagged loads only, a state `GATED` between `DEPENDENCIES` and `BUILDING`:

| State | Work (in slices) | Cancellation point | Next |
|---|---|---|---|
| `GATED` | call `IsPreparationGateOpen(tag)` through the captured hook; false re-submits the continuation at the next slice (never blocking) | each slice | `BUILDING` once the call returns true |
| `BUILDING` (tagged) | pass 1: in cooked order, in chunks of 4,096 entities, mark an entity removed when its parent is removed (cooked order is parents first, `Test_SceneCooker_ActivationOrderParentsFirst`) and otherwise call `ShouldSkipEntity`, marking it and so its descendants removed when it returns true; pass 2: the untagged `BUILDING` work over the kept entities, with each ENTITY value naming a removed entity of the scene written null and counted in `scene.references.skipped`, and `AmendComponents` called once per kept entity after its field sets are converted and before its init record is final | between chunks | `READY` |

An untagged load passes from `DEPENDENCIES` to `BUILDING` directly and its `BUILDING` is single-pass. Progress for a tagged load spends 0.9 to 0.95 in pass 1 and 0.95 to 1.0 in pass 2 (`GATED` reports 0.9 throughout). Removed entities are absent from the prepared scene's activation records, its spawn count and `GetEncodedSpawnBytes`, so the activation-size checks of `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive load sequence" step 7 see only kept entities.

### Discarded publications of tagged loads

Obligation (a): every committed tagged load whose instance becomes `UNLOADED` while `LOADING`, whoever recorded the unload, yields exactly one published publication of its completion ID, whatever the operation's outcome (succeeded and then discarded, failed, cancelled, or never started).

Obligation (b): that publication is decided at publication and is RECORDED, through these steps:

1. **Flag.** Each record carries a plain flag "discard requested". Its only writer is the `Scene.ForgetInstance` playback of a tagged `LOADING` instance on the world's sync-point node. The playback sets the flag and the state `UNLOADED`, then either requests the operation's cancellation (a started preparation) or, for a pending preparation `StepActivations` has not started, removes it from the pending list, calls `OnPreparationEnded(tag)` through the captured hook and parks the discard at once: `Park(id, PUBLICATION_KIND_GENERIC_RECORDED, {instance, SCENE_ERROR_CANCELLED, 0})`. It never touches a parked result.
2. **Park.** A started operation's completion continuation always parks its own result, exactly as for a load nobody unloaded, after `OnPreparationEnded` (obligation (c)).
3. **Resolver.** `Zenith_SceneWorldState::Attach` installs SCENE's resolver with `SetPublicationResolver(pfn, this)` after `SetPublishHandler`. In live mode SCRIPT calls it once per drained completion of kind 8 in completion-ID order on the tick-boundary node, after the drain-and-sort and before logging (row ST21). The resolver looks the completion ID up in the world's records; when the record carries "discard requested" it sets `uPublicationKind = PUBLICATION_KIND_GENERIC_RECORDED`, writes the 8 bytes of `Zenith_EncodeSceneLoadResult({instance, SCENE_ERROR_CANCELLED, 0})` into `pPayload` and sets `uPayloadBytes = 8`, whatever the parked kind and payload were; every other completion it leaves unchanged. The flag is written on the sync-point node and read on the tick-boundary node of the same world, which the frame graph never runs concurrently, so a result parked before the playback, after it, or never is logged identically.
4. **Handler.** The publication handler finds the record by completion ID. A record with "discard requested" (state `UNLOADED`) is handled by the discard case: release any prepared scene the operation produced (`TakePrepared` and destroy, as the failure case does), call `OnDiscardedPublication(tag)` through the captured hook, then release the record and its slot. The handler's other permitted actions are unchanged.
5. **Tick-boundary release.** The tick-boundary handler's step 3 releases the records of instances that became `UNLOADED` since the previous boundary, except a tagged record whose discarded publication is unpublished; the publication handler releases that one after `OnDiscardedPublication`. So the discard publication always finds its record, and an unknown completion ID now arises only for an untagged instance unloaded while loading (counted and ignored, as before).
6. **Publication source.** Under a publication source (`--replay`, a seek) SCRIPT does not call the resolver: the logged discard publishes from the log at its recorded tick and never holds the tick, the handler takes the discard case of step 4, and the replay's own local completion of that ID is dropped by SCRIPT (row ST21 (c)). When that local completion arrives after the logged discard was published, the sink's `Park` returns without asserting (ZenithRuntime's mapping of SCRIPT's `UNKNOWN_COMPLETION`, counted in `runtime.async.late_local_completions`), and SCENE's next `StepActivations` finds an additive operation whose record no longer exists, releases its prepared scene or result, and counts `scene.prepare.orphaned`.

<!-- assumption: pending SCRIPT, MOD (row ST21; core-doc item X271) -->
ZenithRuntime's `Zenith_RuntimeScenePublicationSink::SetPublicationResolver` forwards to `Zenith_AsyncResultTable::SetPublicationResolver(ASYNC_SOURCE_SCENE_LOAD, pfnResolve, pUserData)` of the table the sink bound at the world's creation, with the live-mode call point of step 3 and the replay rule of step 6, as `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "SCRIPT publication resolver" states. Until MOD's forwarding lands the runtime's override stores the pair and forwards nothing, so a tagged discard keeps its parked kind in a live run; SCENE's own tests drive the resolver through the recording sink of `Tests/Support/Zenith_SceneTestSinks.h`.
<!-- /assumption -->

### Hook captured per load

Obligation (c): `OnPreparationEnded(tag)` is called exactly once per committed tagged load, before any park of its publication: on the preparation thread after its last other call (whatever its status: SUCCEEDED, FAILED, CANCELLED); on the main thread in `StepActivations` for a start the loader refused (operation table full, out of memory), before the failure is parked; on the sync-point node in the `Scene.ForgetInstance` playback for a pending preparation removed before it started, before the discard is parked.

Obligation (d): the pending-preparation record created at commit stores the world's hook pointer current at that commit (`m_pxCapturedHook`); `BeginPrepare` passes it into the operation; every subsequent call for that load (`IsPreparationGateOpen`, `ShouldSkipEntity`, `AmendComponents`, `OnPreparationEnded` and the discard's `OnDiscardedPublication`) uses the captured pointer, never the world's current one. `SetActivationHook(nullptr)` therefore affects only loads committed afterwards, and a world's destruction never races a running preparation's call, because STREAM keeps every captured hook alive until all its slots are free. The calls for new simulation events (`OnLoadCommitted`, `AdmitActivation`, `GetRootTranslation`) use the world's current hook; a null hook makes none.

### Component amendment

Obligation (e): `AmendComponents` is called once per entity `ShouldSkipEntity` kept, in `BUILDING` pass 2, before that entity's init record is final. SCENE constructs a `Zenith_SceneComponentAmender` on the preparation thread's stack over the entity's `Zenith_ComponentInit` list, the operation's arena and the type registry. `AddComponent(type)`: an unregistered type increments the call's unknown count (added to `scene.amend.unknown_types` when the call returns) and does nothing; a type already in the list does nothing; otherwise the type's construction default (the registry's default object for the type, `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder") is copied into a new arena block and appended. `RemoveComponent(type)`: a type absent from the list does nothing; otherwise its init entry is removed, keeping the order of the others. A failed arena allocation sets `HasFailed`; after the call the preparation fails with `SCENE_ERROR_OUT_OF_MEMORY` (reported through the ordinary `FAILED` path, so `OnPreparationEnded` still follows). The entity then spawns at the activation's sync point with the amended component set.

### Asset error detail

Obligation (i): whenever a preparation fails with `SCENE_ERROR_ASSET_UNAVAILABLE`, `Zenith_SceneError::m_uDetail` holds the `Zenith_AssetErrorCode` of the ASSET error that caused it, never 0: in `READING`, the registry's reported load error for the scene handle; in `DEPENDENCIES`, `AcquireAny`'s error; for `ASSET_ERROR_DEPENDENCY_FAILED` the failed dependency's root code once ASSET supplies it (row ST22, `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "ASSET dependency root cause"), otherwise `ASSET_ERROR_DEPENDENCY_FAILED` itself. The failure payload `Zenith_SceneLoadResult::m_uDetail` carries the same value, the publication handler stores it in the `FAILED` record with the code, and `GetErrorDetail(instance)` returns it (0 for a record with no detail and for an unknown instance). Every `Zenith_AssetErrorCode` fits the byte (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Errors").

### Live instances and capture coherence

Obligation (f): `GetLiveInstances()` returns the IDs of every `LOADING`, `QUEUED`, `ACTIVE` and `FAILED` record in ascending order, as a span over `m_axLive`, valid until the set's next change; it is the enumeration STREAM's rebuild after a restore reads.

Obligation (g): `IsCaptureCoherentOnly()` is true exactly when the set holds (1) a `LOADING` or `QUEUED` instance, (2) an `UNLOADED` tagged record whose discarded publication is unpublished (such a record is never saved: `ForEachInstance` skips it, so the restored world has none and STREAM rebuilds the cell `UNLOADED`), or (3) a tagged load whose commit the `Scene.LoadAdditive` playback refused at a sync point of the current tick (the flag `m_bTaggedCommitRefusedThisTick`, set by the playback and cleared by the tick-boundary handler at step 1 of the next boundary). SAVE's `Zenith.Scene.AdditiveSet` participant reports COHERENT from its `GetCaptureFidelity` whenever `IsCaptureCoherentOnly()` is true at the capture (phase 11), because the hook learns of a refusal only through `OnLoadCommitted`, which carries no world write access, so STREAM cannot persist the refusal before the phase-11 snapshot (`Zenith2/Docs/Design/STREAM/Runtime.md` → "What is saved").

### Preparation probe and hold

Obligation (h): SCENE counts a cancelled preparation in `scene.prepare.cancelled.<state>` for the state the cancellation reached it in (`REQUESTED`, `READING`, `DECODING`, `DEPENDENCIES`, `GATED`, `BUILDING`, and `PENDING` for a pending preparation the `Scene.ForgetInstance` playback removed before it started), beside the existing total `scene.prepare.cancelled`.

The test-support header `Zenith2/Engine/ZenithScene/Tests/Support/Zenith_ScenePreparationHold.h` is header-only ([Z2-N-ARCH-255]) and usable from other areas' integration suites, because it uses only the public probe seam:

```cpp
// Zenith2/Engine/ZenithScene/Tests/Support/Zenith_ScenePreparationHold.h (test support; may use the STL)
class Zenith_ScenePreparationHold
{
public:
	void Install(Zenith_SceneLoader& xLoader);       // SetPreparationProbe(&Probe, this); MAIN_THREAD, no live operation
	void Uninstall(Zenith_SceneLoader& xLoader);     // SetPreparationProbe(nullptr, nullptr)
	void Hold(Zenith_SceneInstanceID xInstance, Zenith_ScenePreparationStage eStage);   // arm one hold
	bool IsParked() const;             // the armed preparation has reached its stage and is waiting
	void Release();                                  // the next probe call for it returns true
	static bool Probe(Zenith_SceneInstanceID xInstance, Zenith_ScenePreparationStage eStage, void* pUserData);
private:
	std::atomic<uint32> m_uInstance{ 0 };
	std::atomic<uint8> m_uStage{ 0 };
	std::atomic<bool> m_bParked{ false };
	std::atomic<bool> m_bReleased{ false };
};
```

`Probe` returns true for every (instance, stage) pair except the armed one; for the armed pair it sets `m_bParked` and returns `m_bReleased`, so the preparation re-polls at each slice (each a cancellation point) until `Release`. The loader calls the probe at the first slice of `DECODING`, `DEPENDENCIES` and `BUILDING`, before any work of that state, so a held preparation has done none of that state's work and a cancellation while parked is counted under that state.

## Additive set save and restore

Row SV4: SAVE never instantiates prefabs or derives IDs itself; it captures the set through `ForEachInstance` and restores it through `RestoreInstances`, and an instance becomes `ACTIVE` only once every entity of it exists (the all-or-nothing activation of `Zenith2/Docs/Design/SCENE/Loading.md` → "Additive load sequence" step 7). A LOADING instance is re-requested at restore under its recorded completion ID (SV4 (e)), so a replay of a restored session publishes the recorded load result to the very request the restored set waits for.

### Saved instance enumeration

`ForEachInstance(pfnVisit, pUserData)` calls `pfnVisit` once per record in ascending instance ID, skipping `UNLOADED` records (a completed unload; or a tagged record whose discard is unpublished, obligation (g) (2)), with a `Zenith_SavedSceneInstance`:

| Record state | `m_eState` | `m_ulCompletionID` | `m_axRoots` | other fields |
|---|---|---|---|---|
| `LOADING` | `LOADING` | the record's completion ID | empty | scene, tag, duplicate flag, derived-scope flag, policy |
| `QUEUED` | `LOADING` (re-requested at restore) | the record's completion ID | empty | as LOADING |
| `ACTIVE` | `ACTIVE` | 0 | the roots, ascending simulation ID | scene, tag, flags, policy |
| `FAILED` | `FAILED` | 0 | empty | `m_eError`, `m_uErrorDetail`, scene, flags, policy |

It allocates nothing and is O(records). The visit order and content depend only on the set, so two captures of equal sets visit identically. `m_bDerivedScope` is true exactly when the record's commit gave it the instance scope of `Zenith2/Docs/Design/SCENE/Loading.md` → "Instance identity" (another live instance of the same scene existed), so a restore derives the same IDs even when that other instance has since been unloaded.

### Restore algorithm

`RestoreInstances(axInstances, uNextInstanceID)`, on the main thread out of tick, after SAVE restored ENTITIES and before the restored world's first tick:

1. A set that already holds a record returns `SCENE_ERROR_INVALID_DESC` and changes nothing.
2. Validate every entry before applying any: instance IDs strictly ascending, each non-zero and below `uNextInstanceID`; `m_eState` one of `ACTIVE`, `FAILED`, `LOADING`; a `LOADING` entry has a non-zero `m_ulCompletionID`, an `ACTIVE` one has every root live in the world, a `FAILED` one a non-zero `m_eError`; any violation returns `SCENE_ERROR_INVALID_DESC`, logged naming the entry's index and field, and changes nothing.
3. Set the instance counter to `uNextInstanceID`.
4. In ascending instance ID: an `ACTIVE` entry becomes an `ACTIVE` record holding its roots, scene, flags, policy and tag, with no hook call (an ACTIVE instance is restored from ENTITIES); a `FAILED` entry becomes a `FAILED` record with its code and detail. Each consumes one of the `scene.maxAdditiveInstances` live slots; more `ACTIVE` and `FAILED` entries than slots return `SCENE_ERROR_LIMIT`, logged, with the set emptied again.
5. Then, in ascending instance ID, each `LOADING` entry is re-requested: refused with `SCENE_ERROR_LIMIT` when the set is full and with `SCENE_ERROR_DUPLICATE_INSTANCE` when another live record of the same scene exists and the entry's duplicate flag is clear; otherwise a `LOADING` record holding the recorded completion ID (no `AllocateCompletionID` call, so no new ID is allocated) and a pending preparation `{ instance ID, scene, ID scope, policy, captured hook, tag }` are created, the ID scope being the instance scope when `m_bDerivedScope` is set and null otherwise. The next `StepActivations` starts it like any pending preparation.
6. A refused re-request creates no record and parks its failure under the recorded ID at once (`Park(id, PUBLICATION_KIND_GENERIC_RECORDED, {0, code, 0})`), so the in-flight unit the restored table holds for it is returned at the next tick boundary; the publication then names no record and is counted and ignored like any unknown completion ID. It is logged under `scene.load` naming the instance and the scene, and counted in `scene.restore.refused`.
7. Return success; refusals of step 5 are not errors of the restore.

Under a publication source the recorded publication of each re-requested ID completes the restored request at its recorded tick; the live completion of the restored preparation is dropped by SCRIPT like any recorded kind's local completion. An ordinary restore gets the live completion of the same ID. The completion-ID case of row ST2 (j) ("its completion IDs full") cannot arise here, because a re-request allocates no ID.

### Restore re-requests and the hook

For a `LOADING` entry with a non-zero `m_ulHookTag`, step 5 calls, through the world's current hook (a null hook makes no call and the instance is restored untagged):

- `OnLoadCommitted(tag, instance, 0)` on the main thread after it created the record and captured the hook into the pending preparation, so STREAM adopts the re-requested load as its own;
- `OnLoadCommitted(tag, 0, code)` for a refused re-request (obligation (j)), after the failure was parked; STREAM appends nothing, counts `stream.restore.unadopted` and needs nothing else from SCENE, because the refused instance does not exist and its cell, `UNLOADED` after the rebuild, loads afresh when wanted (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Restored loads beyond the pool").

STREAM installs its hook in its `AttachWorld`, before SAVE's restore runs, so a restored tagged load always meets the hook (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Rebuild").

## Zenith_WorldOrigin header

Rows ST3 and PH20. Origin rebasing is STREAM's policy, but transforms are SCENE's, so the one shift mechanism every module uses is a SCENE command.

```cpp
// Zenith2/Engine/ZenithScene/Zenith_WorldOrigin.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_CommandBuffer.h"

/// @contract value Zenith_WorldOrigin
/// Ownership: The world resource "Zenith.WorldOrigin" (owning phase PHASE_INPUT, no system writer), one per world, owned by
///   Zenith_SceneWorldState; written only by the Scene.ShiftWorldOrigin playback; saved with SCENE's world state.
/// Thread affinity: Readable by any node declaring a read of the resource.
/// Invalidation: None.
/// Units: Metres, absolute frame, double; m_uRebaseCount a count of shifts since the world was created.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldOrigin
{
	double m_dX = 0.0, m_dY = 0.0, m_dZ = 0.0;
	uint32 m_uRebaseCount = 0;
	uint64 m_ulLastRebaseTick = 0;
	static const Zenith_TypeID xTYPE_ID;      // Zenith_MakeTypeID("Zenith.WorldOrigin")
};

/// @contract value Zenith_WorldOriginShift
/// Ownership: Plain value copied into the command.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Metres; each component an integer multiple of 1 metre (exact in double).
/// Errors: A non-finite or non-integral component is counted and the command ignored.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldOriginShift { const double m_dX = 0.0, m_dY = 0.0, m_dZ = 0.0; };

/// @contract functions Zenith_WorldOriginRecording
/// Ownership: Records into the caller's buffer.
/// Thread affinity: OWNER_THREAD — the buffer's.
/// Invalidation: None.
/// Units: Metres.
/// Errors: None at record time; the playback validates and counts.
/// Version: since Z2 Stage 4.
/// @endcontract
// Records "Scene.ShiftWorldOrigin" (phases 1–10); its playback is § "Origin shift playback".
void Zenith_RecordShiftWorldOrigin(Zenith_CommandBuffer& xBuffer, const Zenith_WorldOriginShift& xShift);
```

`Zenith_RegisterSceneTypes` registers `Zenith.WorldOrigin` (`TYPE_KIND_RESOURCE`, version 1, owning phase `PHASE_INPUT`) with five serialized properties — 1 `X` F64, 2 `Y` F64, 3 `Z` F64, 4 `RebaseCount` U32, 5 `LastRebaseTick` U64 — so SAVE's resources section saves it with no participant (`Zenith2/Docs/Design/SAVE/Formats.md` → "Resources section"), and the payload struct `Zenith.WorldOriginShift` (`TYPE_KIND_STRUCT`, three F64 fields). `Zenith_RegisterSceneCommandKinds` registers `Scene.ShiftWorldOrigin` (payload 24 bytes, phases 1 to 10) with the play-last flag below. `Zenith_SceneWorldState::Create` constructs the world's `Zenith_WorldOrigin` (all zero) and a shift scratch of `(simulation ID, slot)` pairs, two ping-pong buffers each reserved for the world's entity-slot capacity,, so the playback allocates nothing; `Attach` adds the resource to the world beside the two existing ones.

### Origin shift playback

<!-- assumption: pending ECS (row S25) -->
ECS plays the records of a command kind registered with `Zenith_CommandKindDesc::m_uFlags` bit `COMMAND_KIND_FLAG_PLAY_LAST` (bit 0) after every other step-7 record of the sync point, in merge order among themselves, and `Zenith_WorldPlaybackAccess::GetPlaybackVersion()` returns the version the world simulation set for the playback in progress (distinct per sync point), cross-area row S25.
<!-- /assumption -->
<!-- assumption: pending ECS (row ST4; core-doc item X227 (a)) -->
`PROPERTY_FLAG_WORLD_POSITION` (`1u << 7`, VEC3 properties only) marks a world-space position property, as `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "ECS world-position flag" states.
<!-- /assumption -->

The `Scene.ShiftWorldOrigin` playback, single-threaded on the world's sync-point node, runs last in step 7 of its sync point: after every structural record of step 6 (spawns, including an activation's) and after every other step-7 record of that sync point (STREAM's `ApplyCellState` and `BindReferences`, SCENE's own activation records), whatever its merge key, because the kind is registered play-last. For one record with shift S:

1. **At most one per sync point.** When `GetPlaybackVersion()` equals the version of the shift last applied in this world, the record is a second shift of the sync point: counted in `scene.origin.shifts_ignored`, logged under `scene.transition` (`world <tag>: second origin shift in one sync point ignored`), and ignored.
2. **Validate.** A component that is not finite, not integral, or of magnitude above 2^53 m is counted in `scene.origin.invalid_shifts`, logged, and the record ignored.
3. **Origin.** O += S per axis in binary64 (exact: integer metres); `m_uRebaseCount + 1`; `m_ulLastRebaseTick` = the current tick (the `ulTickStarting` the tick-boundary handler last received); the applied playback version is stored.
4. **Visit order.** Collect `(simulation ID, slot)` of every live entity of the playback access's entity table into the shift scratch and radix-sort it by simulation ID (eight 8-bit LSD passes between the scratch's two ping-pong buffers, O(n)); every following step visits entities in ascending simulation ID. The order changes no value — each write is a function of its own old value — and fixes the order of the log and of the playback write set.
5. **Transforms.** For each entity with `Zenith.WorldTransform`: its `Position` and the translation column (`Row0.w`, `Row1.w`, `Row2.w` for x, y, z) become `(float)((double)p − S)` per axis; for each with `Zenith.PreviousWorldTransform`, its `Position` likewise; for each root entity (no parent in the entity table) with `Zenith.LocalTransform`, its `Position` likewise. A child's local transform is unchanged, because it is relative to its parent. Every write is a `SetProperty` through the playback access, so ECS stamps the change versions and phase 8 propagation sees the roots as changed.
6. **Flagged properties.** For each entity in the same order, every property of each of its components whose registered flags carry `PROPERTY_FLAG_WORLD_POSITION` becomes `(float)((double)p − S)` per axis (PHYS's `Zenith.Physics.CharacterController` property 35 `TargetPosition` among them, row PH20 (b)).
7. **Event.** Dispatch `LIFECYCLE_EVENT_ORIGIN_SHIFTED` (§ "Origin shifted lifecycle event").
8. Count `scene.origin.shifts` and log under `scene.transition` (`world <tag>: origin shifted by (<x>, <y>, <z>) m, count <n>`).

Cost: O(entities with a transform + flagged properties) property writes plus the O(n) sort, in one sync point, measured in the rebase-tick hitch row Z2-B-SHIP-016. Nothing is postponed: the shift plays last, so STREAM's rebase rule needs no "no activating cell" postponement, and a cell admitted this tick with a root translation computed from the pre-shift origin is shifted with every other position.

### Origin shifted lifecycle event

`Zenith_LifecycleEventKind` has `LIFECYCLE_EVENT_ORIGIN_SHIFTED` after `LIFECYCLE_EVENT_CINEMATIC_END` (value 10), as `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Listener interface" declares it with its event-table row ([Z2-N-ARCH-572]; cross-doc item X264 (j), applied by core-doc patch f): entity null, payload `const Zenith_WorldOriginShift*`, dispatched by the `Scene.ShiftWorldOrigin` playback at the end of step 7 after the shift is applied. `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleListener header" lists the enumeration as Architecture's, in its order, and gains this value in the diff that implements the shift playback.

Step 7 of the playback calls `Zenith_LifecycleDispatcher::Dispatch` of the world's dispatcher with `Zenith_LifecycleEvent{ LIFECYCLE_EVENT_ORIGIN_SHIFTED, null entity, the current tick, &shift }`, where `&shift` points at the played `Zenith_WorldOriginShift` for the duration of the dispatch, still inside step 7 of that sync point, after every transform and flagged property was shifted, with the follow-up buffer exactly as the teleport playback's dispatch (`Zenith2/Docs/Design/SCENE/Transforms.md` → "Teleport", step 5). A listener's interest bit is `1u << LIFECYCLE_EVENT_ORIGIN_SHIFTED`. So the physics world shifts at the same sync point as the transforms (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift"): phase-2 to phase-4 queries of the rebase tick see shifted transforms and a shifted physics world together. An ignored record (steps 1 and 2) dispatches nothing.

## Zenith_FluidQuery header

Rows EN26 and PH8. ZenithPhysics and ZenithEnvironment are layer-7 modules with no edge between them, and [Z2-N-ARCH-850] forbids one module querying another for a service object, so the interface belongs to the lower library both already declare, ZenithScene — the precedent of `Zenith_CollisionQuery` (`Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface") — and it is an upward-callback interface of ZenithScene ([Z2-N-ARCH-078]). The content is exactly `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_FluidQuery", whose `Sample(float fX, float fZ, Zenith_FluidSample&)` is the form ENV implements (`Zenith2/Docs/Design/ENV/WaterWeather.md` → "Fluid query provider"); PH8's older `Zenith_Vector3` point form is superseded.

```cpp
// Zenith2/Engine/ZenithScene/Zenith_FluidQuery.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract value Zenith_FluidSample
/// Ownership: Plain value filled by a query.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of Zenith_FluidQuery::Sample.
/// Invalidation: None.
/// Units: Metres; unit normal; m/s; kg/m³.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_FluidSample
{
	float m_fSurfaceHeight = 0.0f;           // the surface height above the queried XZ
	Zenith_Vector3 m_xSurfaceNormal{ 0.0f, 1.0f, 0.0f };
	Zenith_Vector3 m_xFlowVelocity;          // the surface particle velocity
	float m_fDensity = 1000.0f;
};

/// @contract interface Zenith_FluidQuery
/// Ownership: Implemented by the Environment module, one instance per world, owned by its world state; registered as the
///   world resource "Zenith.FluidQuery" (owning phase PHASE_INPUT) and borrowed by readers through GetResource.
/// Thread affinity: ANY_THREAD for queries from nodes of phases 2 to 12 that declare ReadResource of it; its writer (the
///   phase-1 water update) never overlaps a reader.
/// Invalidation: None while the world lives.
/// Units: Metres.
/// Errors: Sample returns false where no fluid covers the point; it never asserts.
/// Reentrancy: NOT_APPLICABLE. It invokes no callback.
/// Version: since Z2 Stage 4. Deterministic: identical state and arguments give identical samples.
/// @endcontract
class Zenith_FluidQuery
{
public:
	virtual ~Zenith_FluidQuery() = default;
	virtual bool Sample(float fX, float fZ, Zenith_FluidSample& xSampleOut) const = 0;
	static const Zenith_TypeID xTYPE_ID;     // Zenith_MakeTypeID("Zenith.FluidQuery")
};
```

`Zenith_RegisterSceneTypes` registers the resource type `Zenith.FluidQuery` (`TYPE_KIND_RESOURCE`, version 1, owning phase `PHASE_INPUT`, no property, so SAVE never saves it and it reaches no world hash), after `Zenith.SceneActivationQueue`. SCENE never adds the resource to a world: the Environment module adds its implementation in its `AttachWorld` (step 1) with `AddWorldResource(Zenith_FluidQuery::xTYPE_ID, PHASE_INPUT, &instance)`, and its phase-1 water update declares `WriteResource` of it. A world without the Environment module has no `Zenith.FluidQuery` resource; a system declaring `ReadResource` of the registered type then receives null from `GetResource` (the rule GAME assumes from ECS for a registered resource type no module added, cross-area row GA1 (a)), which PHYS treats as "no buoyancy" (`Test_FluidQuery_AbsentResourceNoBuoyancy`).

## Zenith_CollisionQuery header

Rows GA2 (a) and GA3 (a). ZenithGameplay declares no dependency on ZenithPhysics and games select Gameplay without Physics, and [Z2-N-ARCH-850] forbids one module querying another for a service object, so the camera's sweep is a world resource whose interface belongs to the lower library both modules already declare, ZenithScene, as an upward-callback interface of ZenithScene ([Z2-N-ARCH-078]). The content is exactly `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface", which specifies it; PHYS implements it per world (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_CollisionQuery"). The header includes ZenithCore, ZenithECS and ZenithReflect headers only, all of them existing ZenithScene dependencies.

```cpp
// Zenith2/Engine/ZenithScene/Zenith_CollisionQuery.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract value Zenith_CollisionSweepDesc
/// Ownership: Plain value passed by reference for one query.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Metres; m_xDirection is a unit vector.
/// Errors: None; a non-unit direction, a negative distance or radius, or a non-finite field makes the query return false.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CollisionSweepDesc
{
	const Zenith_Vector3 m_xOrigin = {};
	const Zenith_Vector3 m_xDirection = {};
	const float m_fDistance = 0.0f;
	const float m_fRadius = 0.0f;          // 0 = a ray
	const uint32 m_uLayerMask = 0xFFFFFFFFu; // the physics collision layers the query hits
	const Zenith_EntityHandle m_xIgnoreRoot = {}; // bodies of this entity and of its descendants are ignored; null = none
};

/// @contract value Zenith_CollisionHit
/// Ownership: Plain value filled by a query.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of Zenith_CollisionQuery::Sweep.
/// Invalidation: None.
/// Units: Metres; m_xNormal is a unit vector.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CollisionHit
{
	float m_fDistance = 0.0f;              // along the direction, from the origin to the sweep's first contact
	Zenith_Vector3 m_xPosition;
	Zenith_Vector3 m_xNormal;
	Zenith_EntityHandle m_xEntity;         // the hit body's entity; null for static geometry without an entity
};

/// @contract interface Zenith_CollisionQuery
/// Ownership: Implemented by the Physics module, one instance per world, owned by the module's world state; registered as
///   the world resource "Zenith.CollisionQuery" (owning phase PHASE_PHYSICS) and borrowed by readers through
///   Zenith_SystemContext::GetResource.
/// Thread affinity: ANY_THREAD for the queries from any node of phases 6 to 12 and of the next tick's phases 1 to 4; the
///   physics step (phase 5, the resource's writer) never overlaps a reader, because the frame graph orders readers by the
///   declared resource access.
/// Invalidation: None while the world lives.
/// Units: Metres.
/// Errors: A query returns false for no hit and for an invalid descriptor; it never asserts on its inputs.
/// Reentrancy: NOT_APPLICABLE. It invokes no callback.
/// Version: since Z2 Stage 3. Determinism: for identical physics state and descriptor the hit is identical on every run;
///   among contacts at equal distance the one whose entity has the lowest stable simulation ID is returned.
/// @endcontract
class Zenith_CollisionQuery
{
public:
	virtual ~Zenith_CollisionQuery() = default;
	virtual bool Sweep(const Zenith_CollisionSweepDesc& xDesc, Zenith_CollisionHit& xHitOut) const = 0;
	static const Zenith_TypeID xTYPE_ID;   // Zenith_MakeTypeID("Zenith.CollisionQuery")
};
```

`Internal/Zenith_TransformComponents.cpp` defines `Zenith_CollisionQuery::xTYPE_ID` (`Zenith_MakeTypeID("Zenith.CollisionQuery")`) beside `Zenith_FluidQuery::xTYPE_ID`, and `Zenith_RegisterSceneTypes` registers the resource type `Zenith.CollisionQuery` (`TYPE_KIND_RESOURCE`, version 1, owning phase `PHASE_PHYSICS`, no property, so SAVE never saves it and it reaches no world hash) immediately after `Zenith.FluidQuery`. Its authority-census row (`—` properties, owning phase `Physics`, Stage `S3`) is the one `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census rows added by subsequent designs" requires of the registering diff. SCENE never adds the resource to a world: the Physics module adds its implementation in its `AttachWorld` with `AddWorldResource(Zenith_CollisionQuery::xTYPE_ID, PHASE_PHYSICS, &instance)` (`Zenith2/Docs/Design/PHYS.md` → "Module start and world attach", row 4), and `Physics.Step` declares `WriteResource` of it, so every reader is ordered against the step by declared access. A world without the Physics module has no `Zenith.CollisionQuery` resource; `Gameplay.Cameras` then reads null and keeps the arm's length (the rule GAME assumes from ECS for a registered resource type no module added, cross-area row GA1 (a)).

## Module seam diagnostics

| Counter | Scope | Counts |
|---|---|---|
| `scene.prepare.cancelled.<state>` (`REQUESTED`, `READING`, `DECODING`, `DEPENDENCIES`, `GATED`, `BUILDING`, `PENDING`) | process | cancelled preparations by the state the cancellation reached them in |
| `scene.prepare.orphaned` | process | additive operations whose record no longer existed when `StepActivations` found them complete |
| `scene.amend.unknown_types` | process | unregistered type IDs passed to the amender |
| `scene.references.skipped` | process | ENTITY values nulled because they named an entity `ShouldSkipEntity` removed |
| `scene.restore.refused` | per world | `LOADING` saved instances whose re-request `RestoreInstances` refused |
| `scene.origin.shifts`, `scene.origin.shifts_ignored`, `scene.origin.invalid_shifts` | per world | applied shifts; second shifts of one sync point; invalid shifts |

The per-world counters join `Zenith_SceneWorldCounters` (`m_ulRestoreRefused`, `m_ulOriginShifts`, `m_ulOriginShiftsIgnored`, `m_ulOriginInvalidShifts`) and publish like the others (`Zenith2/Docs/Design/SCENE.md` → "Counters"). The zone of the shift playback is `Scene.Playback.ShiftWorldOrigin` (the `Scene.Playback.<Kind>` pattern).
