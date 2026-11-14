# SCENE design — the ZenithScene public API

This file is part of the SCENE design family; the entry document is `Zenith2/Docs/Design/SCENE.md`. It declares every public header of `Zenith2/Engine/ZenithScene/` with its contract blocks, in Zenith Hungarian. `Zenith_Transform.h` and `Zenith_TransformComponents.h` are declared in `Zenith2/Docs/Design/SCENE/Transforms.md`; `ZenithDocument`'s headers in `Zenith2/Docs/Design/SCENE/DocumentFramework.md`. Every fallible function returns a `Zenith_Result`. Each header listing below begins with `#pragma once` and its complete `#include` list, then the forward declarations of the types it names only by reference or pointer ([Z2-N-CONTRACT-029]); a header includes only ZenithScene's own public headers, the public headers of ZenithCore, ZenithReflect, ZenithECS, ZenithAssets, ZenithJobs, ZenithProfiling, ZenithDocument and ZenithCompression, and the allowed standard headers, and `HeaderSelfContained` compiles each alone.

## Header list

| Header (`Zenith2/Engine/ZenithScene/`) | Declares |
|---|---|
| `Zenith_SceneErrors.h` | `Zenith_SceneErrorCode`, `Zenith_SceneError`, `Zenith_ToString`, the SCENE limits (`uSCENE_MAX_…`) |
| `Zenith_Transform.h` | `Zenith_Transform` and its functions (Transforms.md) |
| `Zenith_TransformComponents.h` | the four components, `Zenith_RegisterSceneTypes`, `Zenith_MakeTransformInits` (Transforms.md) |
| `Zenith_TransformHierarchy.h` | `uSCENE_MAX_HIERARCHY_DEPTH`, `Zenith_HierarchyNode`, `Zenith_TransformHierarchy`, `Zenith_InterpolateWorldTransform`, `Zenith_PropagateTransformsOutOfTick` |
| `Zenith_LifecycleListener.h` | `Zenith_LifecycleEventKind`, `Zenith_LifecycleEvent`, `Zenith_LifecycleListener`, `Zenith_LifecycleDispatcher` |
| `Zenith_LifecycleCommands.h` | `Zenith_TeleportFlags`, `Zenith_TeleportPayload`, `Zenith_RecordTeleport`, `Zenith_RecordInstantiatePrefab`, `Zenith_PrefabSpawnDesc`, `Zenith_RecordInstantiatePrefabEx`, `Zenith_RecordSetParentKeepWorld`, `Zenith_RecordRequestSceneTransition` |
| `Zenith_AuthoringID.h` | `Zenith_DeriveAuthoringID`, `Zenith_AuthoringIDRemap`, `Zenith_RemapAuthoringIDs` |
| `Zenith_SceneDocument.h` | `Zenith_SceneDocumentKind`, `Zenith_SceneDocument`, `Zenith_SceneEntityView`, `Zenith_SceneInstanceView`, `Zenith_MakeSortKeyBetween`, `Zenith_MakeEvenSortKeys` |
| `Zenith_SceneEdit.h` | `Zenith_SceneEditKind`, `Zenith_SceneEdit` (twelve builders), `Zenith_SceneEditSet`, `Zenith_ApplySceneEdit`, `Zenith_PasteEditSet` |
| `Zenith_SceneData.h` | `Zenith_UnknownTypePolicy`, `Zenith_SceneEntityRecord`, `Zenith_UnknownComponentData`, `Zenith_SceneData`, `Zenith_SceneDiagnostics` |
| `Zenith_PrefabResolver.h` | `Zenith_PrefabProvider`, `Zenith_ResolveDocument`, `Zenith_UnpackPrefabInstance`, the nesting limits |
| `Zenith_SceneCooker.h` | `uSCENE_COOKED_FORMAT_VERSION`, `Zenith_SceneSectionKind`, `Zenith_CookedSceneSections`, `Zenith_SceneCookSettings`, `Zenith_CookedSceneInfo`, `Zenith_CookSceneDocument`, `Zenith_DecodeCookedScene`, `Zenith_DecodeCookedSceneFile` |
| `Zenith_SceneAssets.h` | `Zenith_Scene`, `Zenith_Prefab`, `Zenith_RegisterSceneAssetKinds` |
| `Zenith_SceneLoader.h` | `Zenith_SceneLoadDesc`, `Zenith_PreparedScene`, `Zenith_ActivationProgress`, `Zenith_SceneLoader`, `Zenith_ActivatePreparedOutOfTick`, `Zenith_ActivateSceneData`; Stage 4 additions `Zenith_ScenePreparationStage`, `Zenith_PfnScenePreparationProbe`, `SetPreparationProbe` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Activation hook declarations") |
| `Zenith_AdditiveScenes.h` | `Zenith_SceneInstanceID`, `Zenith_SceneLoadTicket`, `Zenith_AdditiveLoadDesc`, `Zenith_SceneInstanceState`, `Zenith_CommittedSceneLoad`, `Zenith_SceneLoadResult`, `uSCENE_LOAD_RESULT_BYTES`, `Zenith_EncodeSceneLoadResult`, `Zenith_DecodeSceneLoadResult`, `Zenith_AdditiveSceneSet`, `Zenith_RecordLoadAdditiveScene`, `Zenith_RecordUnloadAdditiveScene`, `Zenith_PfnScenePublish`, `Zenith_ScenePublicationSink`; Stage 3 additions `Zenith_SavedSceneInstance`, `Zenith_PfnVisitSceneInstance`, `ForEachInstance`, `RestoreInstances`, `m_ulHookTag`; Stage 4 additions `Zenith_SceneComponentAmender`, `Zenith_SceneActivationHook`, `Zenith_PfnScenePublicationResolve`, `SetPublicationResolver`, `GetLiveInstances`, `GetErrorDetail`, `IsCaptureCoherentOnly` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Activation hook declarations") |
| `Zenith_SceneCapture.h` | `Zenith_CaptureEntities`, `Zenith_CaptureWorld` |
| `Zenith_ReferenceRepair.h` | `Zenith_ReferenceRepairKind`, `Zenith_ReferenceRepairRequest`, `Zenith_ReferenceIndex`, `Zenith_ReferenceRepairPlan`, `Zenith_PlanReferenceRepair` |
| `Zenith_SceneDocumentTypes.h` | `Zenith_RegisterSceneDocumentTypes`, `Zenith_GetSceneCanonicalSchema`, `Zenith_GetPrefabCanonicalSchema`, `Zenith_RewriteOverridePaths`, the document type IDs |
| `Zenith_SceneWorld.h` | `Zenith_SceneServicesDesc`, `Zenith_SceneServices`, `Zenith_RegisterSceneCommandKinds`, `Zenith_RegisterSceneScriptBindings`, `Zenith_SceneTransitionSink`, `Zenith_SceneWorldDesc`, `Zenith_SceneWorldState`, `Zenith_SceneStructuralObserver`; Stage 3 addition: the non-const `GetAdditiveScenes`; Stage 4 additions `SetActivationHook`, `GetWorldOrigin` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Activation hook declarations") |
| `Zenith_WorldOrigin.h` (Stage 4) | `Zenith_WorldOrigin`, `Zenith_WorldOriginShift`, `Zenith_RecordShiftWorldOrigin` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_WorldOrigin header") |
| `Zenith_FluidQuery.h` (Stage 4) | `Zenith_FluidSample`, `Zenith_FluidQuery` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_FluidQuery header") |
| `Zenith_CollisionQuery.h` (Stage 3) | `Zenith_CollisionSweepDesc`, `Zenith_CollisionHit`, `Zenith_CollisionQuery` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_CollisionQuery header") |

Every public header is in the Z2 categories' `reviewPaths` ([Z2-N-CONTRACT-492]).

## Zenith_SceneErrors header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

inline constexpr uint32 uSCENE_MAX_PREFAB_NESTING = 16;
inline constexpr uint32 uSCENE_MAX_VARIANT_CHAIN = 16;
inline constexpr uint32 uSCENE_MAX_DOCUMENT_ENTITIES = 1u << 20;
inline constexpr uint32 uSCENE_MAX_DOCUMENT_INSTANCES = 65536;
inline constexpr uint32 uSCENE_ACTIVATION_CHUNK_ENTITIES = 4096;
inline constexpr uint32 uSCENE_MAX_SORT_KEY_CHARS = 64;

/// @contract enum Zenith_SceneErrorCode
/// Ownership: Plain enumeration, the code of Zenith_SceneError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: DOCUMENT: a ZenithDocument error (logged with its code). SCHEMA: a document violates the scene schema.
///   DUPLICATE_ID: an ID produced twice. PREFAB_NOT_FOUND. PREFAB_CYCLE. PREFAB_TOO_DEEP. HIERARCHY_TOO_DEEP. LIMIT: a
///   document limit. UNKNOWN_TYPE: a component type absent from the registry where the policy fails. COOKED_VERSION.
///   COOKED_CORRUPT. ASSET_UNAVAILABLE: the scene or a dependency could not be made resident. MEMORY_REFUSED: the
///   governor refused a reservation. ACTIVATION_TOO_LARGE. ACTIVATION_CAPACITY: merge-time rejection persisted beyond
///   the retry limit. DUPLICATE_INSTANCE. INSTANCE_NOT_FOUND. CANCELLED. INVALID_DESC. OUT_OF_MEMORY. WORLD_NOT_ATTACHED:
///   an operation named a world without SCENE state. ECS: an ECS call failed (its Zenith_ECSError is logged).
///   READ_ONLY: an edit of a read-only document. EDIT_INVALID: an edit whose preconditions fail (unknown target, a
///   parent that would form a cycle). NOT_FOUND: a lookup by authoring ID (Zenith_SceneDocument::FindEntity,
///   FindInstance) of an ID the document does not hold.
/// Version: since Z2 Stage 2. An enumerator never changes meaning.
/// @endcontract
enum Zenith_SceneErrorCode : uint8
{
	SCENE_ERROR_DOCUMENT = 1,
	SCENE_ERROR_SCHEMA,
	SCENE_ERROR_DUPLICATE_ID,
	SCENE_ERROR_PREFAB_NOT_FOUND,
	SCENE_ERROR_PREFAB_CYCLE,
	SCENE_ERROR_PREFAB_TOO_DEEP,
	SCENE_ERROR_HIERARCHY_TOO_DEEP,
	SCENE_ERROR_LIMIT,
	SCENE_ERROR_UNKNOWN_TYPE,
	SCENE_ERROR_COOKED_VERSION,
	SCENE_ERROR_COOKED_CORRUPT,
	SCENE_ERROR_ASSET_UNAVAILABLE,
	SCENE_ERROR_MEMORY_REFUSED,
	SCENE_ERROR_ACTIVATION_TOO_LARGE,
	SCENE_ERROR_ACTIVATION_CAPACITY,
	SCENE_ERROR_DUPLICATE_INSTANCE,
	SCENE_ERROR_INSTANCE_NOT_FOUND,
	SCENE_ERROR_CANCELLED,
	SCENE_ERROR_INVALID_DESC,
	SCENE_ERROR_OUT_OF_MEMORY,
	SCENE_ERROR_WORLD_NOT_ATTACHED,
	SCENE_ERROR_ECS,
	SCENE_ERROR_READ_ONLY,
	SCENE_ERROR_EDIT_INVALID,
	SCENE_ERROR_NOT_FOUND,     // 25: FindEntity / FindInstance of an absent authoring ID
};

/// @contract value Zenith_SceneError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480] (code first, 24 bytes).
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: m_uEntityIndex indexes the document's or payload's entity order (0xFFFFFFFF when not applicable).
/// Errors: None; document paths, IDs and text are logged in category scene.* at the point of failure.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SceneError
{
	Zenith_SceneErrorCode m_eCode;
	uint8 m_uDetail = 0;                 // the Zenith_DocumentErrorCode or Zenith_ECSErrorCode for DOCUMENT and ECS
	uint16 m_uReserved = 0;
	uint32 m_uEntityIndex = 0xFFFFFFFFu;
	Zenith_TypeID m_xType;                 // the component type involved, zero when not applicable
	uint64 m_ulReserved = 0;
};
static_assert(sizeof(Zenith_SceneError) == 24);

constexpr const char* Zenith_ToString(Zenith_SceneErrorCode eCode);
```

Fault points of ZenithScene carry the tag `0x5343` (`SC`): `uFAULT_POINT_SCENE_WORLD_STATE_HIERARCHY = 0x53430001`, `…_ACTIVATION_QUEUE = 0x53430002`, `…_ADDITIVE_SET = 0x53430003`, `…_LOADER_OPERATIONS = 0x53430004`, `…_LOADER_ARENA = 0x53430005`, `…_SERVICES_KINDS = 0x53430006`.

## Zenith_TransformHierarchy header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_TransformHierarchy.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithScene/Zenith_SceneErrors.h"
#include "ZenithScene/Zenith_TransformComponents.h"

class Zenith_WorldInspector;
class Zenith_WorldSimulation;
class Zenith_SceneWorldState;

inline constexpr uint32 uSCENE_MAX_HIERARCHY_DEPTH = 64;

/// @contract value Zenith_HierarchyNode
/// Ownership: Plain value stored in Zenith_TransformHierarchy's node map; returned by pointer from Find.
/// Thread affinity: IMMUTABLE as a value; the stored nodes follow Zenith_TransformHierarchy's rule. Members non-const:
///   Zenith_TransformHierarchy rewrites stored nodes in place as entities are reparented.
/// Invalidation: A pointer from Find is invalid after the next mutation of the hierarchy.
/// Units: Simulation IDs; depth from 0 (a root); child count.
/// Errors: None.
/// Version: since Z2 Stage 2. Derived state; never serialized.
/// @endcontract
struct Zenith_HierarchyNode
{
	Zenith_EntityHandle m_xEntity;
	uint64 m_ulParentSimulationID = 0;     // 0 = root (simulation IDs start at 1)
	uint16 m_uDepth = 0;                   // 0 for a root
	uint16 m_uChildCount = 0;
	uint32 m_uReserved = 0;
};
static_assert(sizeof(Zenith_HierarchyNode) == 24);

/// @contract type Zenith_TransformHierarchy
/// Ownership: Owned by Zenith_SceneWorldState (one per world); storage from the world's heap; registered as the world
///   resource "Zenith.TransformHierarchy".
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Mutated on the world's sync-point node (the structural observer), out of
///   tick, and by the merge of Scene.PropagateTransforms (the resource's phase-8 writer), which clears the dirty set;
///   read by the propagation node in phase 8 and by any node declaring a read of the resource.
/// Invalidation: Spans returned by GetLevel are invalid after the next mutation.
/// Units: Simulation IDs; depths from 0.
/// Errors: None; a depth-limit violation is corrected at playback (Transforms.md "Hierarchy maintenance at playback").
/// Version: since Z2 Stage 2. Derived state; never serialized.
/// @endcontract
class Zenith_TransformHierarchy
{
public:
	const Zenith_HierarchyNode* Find(uint64 ulSimulationID) const;
	uint16 GetDepth(uint64 ulSimulationID) const;             // 0 for a root or an unknown ID
	uint16 GetMaxDepth() const;
	Zenith_Span<const uint64> GetLevel(uint16 uDepth) const;  // ascending simulation ID
	bool IsDirty(uint64 ulSimulationID) const;
	uint32 GetVersion() const;
	uint64 GetResidentBytes() const;
	Zenith_Result<void, Zenith_SceneError> Rebuild(const Zenith_WorldInspector& xWorld);
	bool Equals(const Zenith_TransformHierarchy& xOther) const;  // for validation and tests
};

/// @contract functions Zenith_TransformPropagationFunctions
/// Ownership: Pure functions over the caller's values, or out-of-tick writes through the world simulation.
/// Thread affinity: ANY_THREAD_EXCEPT(Zenith_PropagateTransformsOutOfTick). Zenith_InterpolateWorldTransform is a pure
///   function; Zenith_PropagateTransformsOutOfTick runs on the main thread with no frame of the world running.
/// Invalidation: None.
/// Units: fAlpha in [0, 1] (JOBS's sim.alpha); matrices on column vectors.
/// Errors: Zenith_PropagateTransformsOutOfTick returns SCENE_ERROR_ECS for a failed property write (logged) and
///   SCENE_ERROR_WORLD_NOT_ATTACHED for a world without SCENE state.
/// Version: since Z2 Stage 2.
/// @endcontract
void Zenith_InterpolateWorldTransform(const Zenith_PreviousWorldTransform& xPrevious, const Zenith_WorldTransform& xCurrent,
	float fAlpha, Zenith_Matrix4& xOut);
Zenith_Result<void, Zenith_SceneError> Zenith_PropagateTransformsOutOfTick(Zenith_SceneWorldState& xState,
	Zenith_WorldSimulation& xWorld);
```

## Zenith_LifecycleListener header

The listener interface is `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Listener interface" verbatim, with its contract written in the block form, and one dispatcher added.

```cpp
// Zenith2/Engine/ZenithScene/Zenith_LifecycleListener.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_WorldPlaybackAccess;

/// @contract enum Zenith_LifecycleEventKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Values are Architecture's, in its order.
/// @endcontract
enum Zenith_LifecycleEventKind : uint8
{
	LIFECYCLE_EVENT_SPAWNED, LIFECYCLE_EVENT_DESTROY_PENDING, LIFECYCLE_EVENT_DESTROYED, LIFECYCLE_EVENT_TELEPORTED,
	LIFECYCLE_EVENT_RAGDOLL_ENTER, LIFECYCLE_EVENT_RAGDOLL_RECOVER, LIFECYCLE_EVENT_CINEMATIC_START,
	LIFECYCLE_EVENT_CINEMATIC_INTERRUPT, LIFECYCLE_EVENT_CINEMATIC_SKIP, LIFECYCLE_EVENT_CINEMATIC_END,
};

/// @contract value Zenith_LifecycleEvent
/// Ownership: Plain value; m_pPayload points at the command's payload struct for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: m_pPayload is invalid after the listener returns.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_LifecycleEvent
{
	const Zenith_LifecycleEventKind m_eKind = {};
	const Zenith_EntityHandle m_xEntity = {};
	const uint64 m_ulTick = 0;
	const void* const m_pPayload = nullptr;
};

/// @contract interface Zenith_LifecycleListener
/// Ownership: Implemented by modules (physics, animation, cinematics, streaming) and by the runtime's render-extraction
///   state; registered per world by the composition root.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Called on the world's sync-point node, one world at a time; the executor
///   serialises the calls.
/// Invalidation: None.
/// Units: None.
/// Errors: A listener logs and counts its own failures and never asserts on event contents.
/// Reentrancy: NONE. A listener records into xFollowUp and never calls back into playback ([Z2-N-ARCH-544]).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_LifecycleListener
{
public:
	virtual ~Zenith_LifecycleListener() = default;
	virtual uint32 GetInterestMask() const = 0;
	virtual void OnLifecycleEvent(const Zenith_LifecycleEvent& xEvent, Zenith_WorldPlaybackAccess& xWorld,
		Zenith_CommandBuffer& xFollowUp) = 0;
};

/// @contract type Zenith_LifecycleDispatcher
/// Ownership: Owned by Zenith_SceneWorldState; holds up to uSCENE_MAX_LIFECYCLE_LISTENERS (64) borrowed listeners.
/// Thread affinity: ANY_THREAD_EXCEPT(Dispatch). Register and Unregister on the main thread before the first tick or
///   between frames; Dispatch on the world's sync-point node.
/// Invalidation: None.
/// Units: None.
/// Errors: Register returns SCENE_ERROR_LIMIT past 64 listeners; Register after the first tick is ZENITH_CHECK.
/// Reentrancy: NONE. A Zenith_ReentrancyGuard reports a Dispatch from inside a listener.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_LifecycleDispatcher
{
public:
	Zenith_Result<void, Zenith_SceneError> Register(Zenith_LifecycleListener& xListener);
	void Unregister(Zenith_LifecycleListener& xListener);
	// Calls every listener whose mask has the event's bit, in registration order ([Z2-N-ARCH-543]).
	void Dispatch(const Zenith_LifecycleEvent& xEvent, Zenith_WorldPlaybackAccess& xWorld, Zenith_CommandBuffer& xFollowUp);
	uint32 GetListenerCount() const;
};
```

The enumeration gains `LIFECYCLE_EVENT_ORIGIN_SHIFTED` (value 10, after `LIFECYCLE_EVENT_CINEMATIC_END`), which Architecture's list already declares (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Listener interface", [Z2-N-ARCH-572]; core-doc item X264 (j), applied by core-doc patch f). The listing above stays the Stage 2 form; the enumerator joins it in the diff that implements the `Scene.ShiftWorldOrigin` playback, which dispatches it (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shifted lifecycle event").

Modules that play back their own lifecycle commands (ragdoll, cinematic) call `Dispatch` from their playback functions, reaching the world's dispatcher through `Zenith_SceneWorldState::GetLifecycleDispatcher`; the follow-up buffer is the one the structural observer or the playback function received ([Z2-N-ARCH-542]).

## Zenith_LifecycleCommands header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_LifecycleCommands.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithScene/Zenith_Transform.h"
#include "ZenithScene/Zenith_SceneAssets.h"

/// @contract enum Zenith_TeleportFlags
/// Ownership: Plain enumeration; the bits of a teleport's uTeleportFlags and of Zenith_TeleportPayload::m_uFlags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; bits outside the enumeration are ignored at playback and counted in scene.teleport.count only.
/// Version: since Z2 Stage 2. A bit never changes meaning; listeners (physics, animation) read the two bits.
/// @endcontract
enum Zenith_TeleportFlags : uint32
{
	TELEPORT_FLAG_NONE = 0,
	TELEPORT_FLAG_KEEP_VELOCITY = 1u << 0,
	TELEPORT_FLAG_KEEP_ANIMATION_PHASE = 1u << 1,
};

/// @contract value Zenith_TeleportPayload
/// Ownership: Plain value; the payload of the Scene.Teleport command kind, copied into the command buffer at record time;
///   listeners receive a pointer to it in Zenith_LifecycleEvent::m_pPayload.
/// Thread affinity: IMMUTABLE.
/// Invalidation: The listener's pointer is invalid after OnLifecycleEvent returns.
/// Units: m_xTarget as Zenith_Transform (a local transform: relative to the entity's parent); m_uFlags are
///   Zenith_TeleportFlags bits.
/// Errors: None; a stale m_xEntity is detected at playback and counted (scene.teleport.stale).
/// Version: since Z2 Stage 2. 56 bytes; the command kind's max payload.
/// @endcontract
struct Zenith_TeleportPayload
{
	const Zenith_EntityHandle m_xEntity = {};
	const Zenith_Transform m_xTarget = {};
	const uint32 m_uFlags = TELEPORT_FLAG_NONE;
	const uint32 m_uReserved = 0;
};
static_assert(sizeof(Zenith_TeleportPayload) == 56);

/// @contract value Zenith_PrefabSpawnDesc
/// Ownership: Plain value read by Zenith_RecordInstantiatePrefabEx for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_xAt as Zenith_Transform, relative to m_xParent (or to the world when m_xParent is null).
/// Errors: None of its own; a stale or foreign m_xParent is dropped at playback by ECS and counted.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PrefabSpawnDesc
{
	const Zenith_Transform m_xAt = {};
	const Zenith_EntityHandle m_xParent = {};      // null for a root; a ticket handle of the same buffer is accepted
	const Zenith_PersistentID m_xInstanceID = {};  // null: runtime-only entities (null persistent IDs)
	const bool m_bAddTransformComponents = true;
};

/// @contract functions Zenith_LifecycleCommandRecording
/// Ownership: Record into the caller's command buffer; own nothing.
/// Thread affinity: OWNER_THREAD — the buffer's.
/// Invalidation: A returned ticket is meaningful only for xBuffer.
/// Units: As Zenith_Transform.
/// Errors: Recording returns void or a ticket; a prefab that is not resident, or an instance larger than the buffer's cap,
///   records nothing and returns the null ticket (counters scene.instantiate.not_resident, scene.instantiate.too_large);
///   semantic errors (stale entities) are detected at playback and counted, never asserted.
/// Version: since Z2 Stage 2 (teleport, set-parent-keep-world), Stage 3 (prefab instantiation, scene transition requests).
/// @endcontract
void Zenith_RecordTeleport(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, const Zenith_Transform& xTarget,
	uint32 uTeleportFlags);
Zenith_SpawnTicket Zenith_RecordInstantiatePrefab(Zenith_CommandBuffer& xBuffer,
	Zenith_AssetHandle<Zenith_Prefab> xPrefab, const Zenith_Transform& xAt);
Zenith_SpawnTicket Zenith_RecordInstantiatePrefabEx(Zenith_CommandBuffer& xBuffer,
	Zenith_AssetHandle<Zenith_Prefab> xPrefab, const Zenith_PrefabSpawnDesc& xDesc);
void Zenith_RecordSetParentKeepWorld(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xChild, Zenith_EntityHandle xNewParent);
void Zenith_RecordRequestSceneTransition(Zenith_CommandBuffer& xBuffer, Zenith_AssetHandle<Zenith_Scene> xScene,
	Zenith_Span<const Zenith_PersistentID> axCarryOver);   // at most 256 carry-over roots; more are dropped and counted
```

The command kinds (registered by `Zenith_RegisterSceneCommandKinds` at startup step 15, `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandKinds header"):

| Kind name | Payload | Max payload | Phases | Playback |
|---|---|---|---|---|
| `Scene.Teleport` | `Zenith_TeleportPayload` | 56 | 1–10 | Transforms.md → "Teleport" |
| `Scene.SetParentKeepWorld` | two entity handles | 16 | 1–10 | Transforms.md → "Reparenting helpers" |
| `Scene.BindReferences` | the bind list | 1 MiB | 1–10 | Loading.md → "Out-of-tick activation" |
| `Scene.LoadAdditive` | scene asset ID, flags, the buffer-local ticket | 32 | 1–10 | Loading.md → "Additive load sequence" |
| `Scene.ForgetInstance` | instance ID | 4 | 1–10 | Loading.md → "Unload" |
| `Scene.RequestTransition` | scene asset ID, carry-over count, IDs | 4,112 | 1–10 | calls `Zenith_SceneTransitionSink::RequestTransition` |

## Zenith_AuthoringID header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_AuthoringID.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Random.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_SceneEditSet;

/// @contract value Zenith_AuthoringIDRemap
/// Ownership: Plain value; Zenith_RemapAuthoringIDs returns a span of them in the caller's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: The returned span is invalid when the arena is reset.
/// Units: Persistent IDs; the span is in ascending m_xOld.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AuthoringIDRemap
{
	const Zenith_PersistentID m_xOld = {};
	const Zenith_PersistentID m_xNew = {};
};

/// @contract functions Zenith_AuthoringIDFunctions
/// Ownership: Pure functions; Zenith_RemapAuthoringIDs edits the caller's set.
/// Thread affinity: ANY_THREAD_EXCEPT(Zenith_RemapAuthoringIDs). The remap runs on the thread that owns the set, the
///   random source and the arena.
/// Invalidation: None.
/// Units: Persistent IDs (Zenith2/Docs/Contracts.md "Handle kinds").
/// Errors: Zenith_RemapAuthoringIDs returns SCENE_ERROR_OUT_OF_MEMORY and leaves the set unchanged.
/// Version: since Z2 Stage 2. The derivation (SHA-256 of "ZSID", scope and local ID with the version-4 bits forced,
///   Zenith2/Docs/Design/SCENE/Prefabs.md "Derivation") is a permanent format property: derived IDs are stored in
///   documents, cooked payloads and saves, so a change needs a decision, a document-set rewrite, a re-cook and a SAVE
///   step. It depends on no open decision (not on D-SEL-HASH (open, Stage 1)). A null xScope is accepted.
/// @endcontract
Zenith_PersistentID Zenith_DeriveAuthoringID(const Zenith_PersistentID& xScope, const Zenith_PersistentID& xLocal);
Zenith_Result<Zenith_Span<const Zenith_AuthoringIDRemap>, Zenith_SceneError> Zenith_RemapAuthoringIDs(
	Zenith_SceneEditSet& xSet, Zenith_RandomSource& xRandom, Zenith_Arena& xArena);
```

## Zenith_SceneDocument header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneDocument.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithDocument/Zenith_DocumentMigrator.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_TypeRegistry;
class Zenith_SceneDiagnostics;

/// @contract enum Zenith_SceneDocumentKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. SCENE is a *.scene.json document ($schema zenith2.scene), PREFAB a *.prefab.json one.
/// @endcontract
enum Zenith_SceneDocumentKind : uint8
{
	SCENE_DOCUMENT_KIND_SCENE = 1,
	SCENE_DOCUMENT_KIND_PREFAB,
};

/// @contract value Zenith_SceneEntityView
/// Ownership: A borrowed view of one entity object of a document.
/// Thread affinity: OWNER_THREAD — the document's.
/// Invalidation: Invalid after any edit of the document.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SceneEntityView
{
	Zenith_PersistentID m_xID;
	Zenith_PersistentID m_xParent;           // null for a root
	Zenith_StringView m_strName;
	Zenith_StringView m_strSortKey;
	Zenith_JsonNode m_xComponents;           // the components map node in the document tree
};

/// @contract value Zenith_SceneInstanceView
/// Ownership: A borrowed view of one prefab-instance object.
/// Thread affinity: OWNER_THREAD — the document's.
/// Invalidation: Invalid after any edit of the document.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_SceneInstanceView
{
	Zenith_PersistentID m_xID;
	Zenith_PersistentID m_xPrefab;
	Zenith_PersistentID m_xParent;
	Zenith_StringView m_strSortKey;
	Zenith_JsonNode m_xOverrides;
};

/// @contract type Zenith_SceneDocument
/// Ownership: Created by Load or CreateEmpty as a Zenith_Owned<Zenith_SceneDocument>; owns its Zenith_JsonDocument
///   (the tree is the document: unknown members and components live in it), the ID index and the retired-ID set.
/// Thread affinity: OWNER_THREAD. The editor's main thread, or the cook's document thread.
/// Invalidation: Views and JSON nodes it returns are invalid after the next edit.
/// Units: None.
/// Errors: Load returns SCENE_ERROR_DOCUMENT (a framework error, including NEWER_VERSION: the document is then opened
///   read-only and Load succeeds with IsReadOnly()), SCENE_ERROR_SCHEMA, SCENE_ERROR_DUPLICATE_ID, SCENE_ERROR_LIMIT,
///   SCENE_ERROR_HIERARCHY_TOO_DEEP; every validation diagnostic of a pass is reported through xDiagnostics before the
///   error returns. Edits of a read-only document return SCENE_ERROR_READ_ONLY. FindEntity and FindInstance of an ID
///   the document does not hold return SCENE_ERROR_NOT_FOUND. Untrusted bytes never assert.
/// Version: since Z2 Stage 2. The document format version is the SceneDocument / PrefabDocument type version.
/// @endcontract
class Zenith_SceneDocument
{
public:
	// Adopts a document the migrator opened (already at the current version, or read-only newer).
	static Zenith_Result<Zenith_Owned<Zenith_SceneDocument>, Zenith_SceneError> Load(Zenith_OpenedDocument&& xOpened,
		const Zenith_TypeRegistry& xTypes, Zenith_SceneDiagnostics& xDiagnostics);
	static Zenith_Result<Zenith_Owned<Zenith_SceneDocument>, Zenith_SceneError> CreateEmpty(Zenith_SceneDocumentKind eKind,
		const Zenith_PersistentID& xID, Zenith_StringView strName);

	Zenith_SceneDocumentKind GetKind() const;
	Zenith_PersistentID GetID() const;
	bool IsReadOnly() const;
	uint32 GetEntityCount() const;
	uint32 GetInstanceCount() const;
	// Enumerate in ascending ID (the canonical order).
	uint32 ListEntities(uint32 uFirst, Zenith_Span<Zenith_SceneEntityView> axOut) const;
	uint32 ListInstances(uint32 uFirst, Zenith_Span<Zenith_SceneInstanceView> axOut) const;
	Zenith_Result<Zenith_SceneEntityView, Zenith_SceneError> FindEntity(const Zenith_PersistentID& xID) const;
	Zenith_Result<Zenith_SceneInstanceView, Zenith_SceneError> FindInstance(const Zenith_PersistentID& xID) const;
	bool ContainsOrRetired(const Zenith_PersistentID& xID) const;
	Zenith_Span<const Zenith_PersistentID> GetRetiredIDs() const;
	// The entities and instances whose parent is xParent (null: the roots), in sibling order (sort key, then ID),
	// from index uFirst; returns the number written. The sort keys are read through FindEntity / FindInstance.
	uint32 ListChildrenInSiblingOrder(const Zenith_PersistentID& xParent, uint32 uFirst,
		Zenith_Span<Zenith_PersistentID> axOut) const;
	// Prefab documents.
	Zenith_PersistentID GetRoot() const;
	Zenith_PersistentID GetVariantOf() const;
	// The tree, for the framework (Serialize, migration steps) and for EDIT's inspector of unknown data.
	const Zenith_JsonDocument& GetTree() const;
	Zenith_Result<void, Zenith_SceneError> Serialize(Zenith_Array<char>& xOut) const;   // canonical bytes
};

/// @contract functions Zenith_SortKeyFunctions
/// Ownership: Keys appended to the caller's array (MakeSortKeyBetween) or built in the caller's arena (MakeEvenSortKeys).
/// Thread affinity: ANY_THREAD. Pure functions.
/// Invalidation: The views MakeEvenSortKeys writes are invalid when the arena is reset.
/// Units: Keys of 1 to uSCENE_MAX_SORT_KEY_CHARS characters of 0-9A-Za-z, never ending in '0'.
/// Errors: SCENE_ERROR_SCHEMA for an invalid bound or strBefore >= strAfter; SCENE_ERROR_LIMIT when no key of at most 64
///   characters lies between the bounds, or uCount exceeds uSCENE_MAX_DOCUMENT_ENTITIES; SCENE_ERROR_OUT_OF_MEMORY. On
///   failure xOut and astrOut are unchanged. The algorithm is Zenith2/Docs/Design/SCENE/Format.md "Sort key insertion
///   algorithm".
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_SceneError> Zenith_MakeSortKeyBetween(Zenith_StringView strBefore,
	Zenith_StringView strAfter, Zenith_Array<char>& xOut);   // empty strBefore = first, empty strAfter = last
Zenith_Result<void, Zenith_SceneError> Zenith_MakeEvenSortKeys(uint32 uCount, Zenith_Arena& xArena,
	Zenith_Span<Zenith_StringView> astrOut);                 // astrOut holds uCount views, ascending
bool Zenith_IsValidSortKey(Zenith_StringView strKey);
```

## Zenith_SceneEdit header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneEdit.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithReflect/Zenith_ReflectionWalk.h"
#include "ZenithScene/Zenith_SceneDocument.h"

class Zenith_PrefabProvider;

/// @contract enum Zenith_SceneEditKind
/// Ownership: Plain enumeration; the kind of a Zenith_SceneEdit.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Values are never reordered; a new kind is appended. The inverse of every kind is the table
///   of Zenith2/Docs/Design/SCENE/Prefabs.md "Document edits for the editor".
/// @endcontract
enum Zenith_SceneEditKind : uint8
{
	SCENE_EDIT_CREATE_ENTITY = 1,
	SCENE_EDIT_DELETE_ENTITY,
	SCENE_EDIT_CREATE_SUBTREE,
	SCENE_EDIT_SET_PARENT,
	SCENE_EDIT_RENAME,
	SCENE_EDIT_SET_PROPERTY,
	SCENE_EDIT_ADD_COMPONENT,
	SCENE_EDIT_REMOVE_COMPONENT,
	SCENE_EDIT_PLACE_INSTANCE,
	SCENE_EDIT_DELETE_INSTANCE,
	SCENE_EDIT_REVERT_OVERRIDE,
	SCENE_EDIT_APPLY_OVERRIDES_TO_PREFAB,
};

/// @contract value Zenith_SceneOverrideRef
/// Ownership: Plain value; m_strStableName is borrowed for the builder call and copied into the edit's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_xTarget is an ID of the instance's resolved ID space (Zenith2/Docs/Design/SCENE/Format.md "Resolved ID
///   space"); m_xPath is a property-ID path.
/// Errors: None; an override the instance does not hold makes the edit fail SCENE_ERROR_EDIT_INVALID at application.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_SceneOverrideRef
{
	const Zenith_PersistentID m_xTarget = {};
	const Zenith_StringView m_strStableName = {};
	const Zenith_PropertyPath m_xPath = {};
};

/// @contract type Zenith_SceneEdit
/// Ownership: A value whose variable-size parts (names, sort keys, component JSON subtrees, removed objects, override
///   references) are copied into the arena passed to its builder; owned by EDIT's command record for its undo lifetime.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when its arena is reset.
/// Units: IDs are authoring IDs of the document the edit applies to; a target is an ID of that document's resolved ID
///   space; sort keys follow Zenith2/Docs/Design/SCENE/Format.md "Sort keys".
/// Errors: Every builder returns SCENE_ERROR_OUT_OF_MEMORY, and SCENE_ERROR_SCHEMA for an invalid sort key, a null ID
///   where one is required or an empty stable name; semantic preconditions (the target exists, no cycle) are checked by
///   Zenith_ApplySceneEdit.
/// Version: since Z2 Stage 3 (entity, parent, rename, property and component kinds), Stage 4 (instance and override kinds).
/// @endcontract
class Zenith_SceneEdit
{
public:
	Zenith_SceneEditKind GetKind() const;
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> CreateEntity(Zenith_Arena& xArena,
		const Zenith_PersistentID& xID, const Zenith_PersistentID& xParent, Zenith_StringView strSortKey,
		Zenith_StringView strName);
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> DeleteEntity(Zenith_Arena& xArena,
		const Zenith_PersistentID& xID);                                  // with its whole subtree
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> CreateSubtree(Zenith_Arena& xArena,
		const Zenith_SceneEditSet& xObjects, const Zenith_PersistentID& xParent);   // objects verbatim, IDs kept
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> SetParent(Zenith_Arena& xArena,
		const Zenith_PersistentID& xID, const Zenith_PersistentID& xNewParent, Zenith_StringView strSortKey);
		// xID names an entity or a prefab instance (both are siblings in one group)
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> Rename(Zenith_Arena& xArena,
		const Zenith_PersistentID& xID, Zenith_StringView strName);
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> SetProperty(Zenith_Arena& xArena,
		const Zenith_PersistentID& xTarget, Zenith_StringView strStableName, const Zenith_PropertyPath& xPath,
		const Zenith_JsonDocument& xValueDocument, Zenith_JsonNode xValue);   // the value in the default JSON form
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> AddComponent(Zenith_Arena& xArena,
		const Zenith_PersistentID& xTarget, Zenith_StringView strStableName, const Zenith_JsonDocument& xComponentDocument,
		Zenith_JsonNode xComponent);                                      // a component object { version, fields }
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> RemoveComponent(Zenith_Arena& xArena,
		const Zenith_PersistentID& xTarget, Zenith_StringView strStableName);
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> PlaceInstance(Zenith_Arena& xArena,
		const Zenith_PersistentID& xInstanceID, const Zenith_PersistentID& xPrefab, Zenith_StringView strHint,
		const Zenith_PersistentID& xParent, Zenith_StringView strSortKey, const Zenith_JsonDocument* pxOverridesDocument,
		Zenith_JsonNode xOverrides, Zenith_Span<const Zenith_PersistentID> axReplacedEntities);
		// pxOverridesDocument null: an empty override block; axReplacedEntities: entities the placement removes (the
		// inverse of an unpack), empty otherwise
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> DeleteInstance(Zenith_Arena& xArena,
		const Zenith_PersistentID& xInstanceID);
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> RevertOverride(Zenith_Arena& xArena,
		const Zenith_PersistentID& xTarget, Zenith_StringView strStableName, const Zenith_PropertyPath& xPath);
	static Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> ApplyOverridesToPrefab(Zenith_Arena& xArena,
		const Zenith_PersistentID& xInstanceID, Zenith_Span<const Zenith_SceneOverrideRef> axSelection);
		// an empty selection applies every override of the instance
};

/// @contract type Zenith_SceneEditSet
/// Ownership: A caller-owned group of objects copied out of documents (for copy, paste, duplicate), in an arena.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: With its arena.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SceneEditSet
{
public:
	static Zenith_Result<Zenith_SceneEditSet, Zenith_SceneError> Copy(const Zenith_SceneDocument& xDocument,
		Zenith_Span<const Zenith_PersistentID> axIDs, Zenith_Arena& xArena);   // subtrees included
	uint32 GetObjectCount() const;
};

/// @contract functions Zenith_SceneEditApplication
/// Ownership: Edits the caller's document; the inverse is built in xArena.
/// Thread affinity: OWNER_THREAD — the document's.
/// Invalidation: Views of the document are invalid after the call.
/// Units: None.
/// Errors: SCENE_ERROR_EDIT_INVALID (unknown target, a parent that would form a cycle or exceed the depth limit, an ID
///   already present or retired, a missing pxPrefabs or pxPrefabDocument the kind requires), SCENE_ERROR_READ_ONLY,
///   SCENE_ERROR_OUT_OF_MEMORY; on any error both documents are unchanged.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> Zenith_ApplySceneEdit(Zenith_SceneDocument& xDocument,
	const Zenith_SceneEdit& xEdit, Zenith_PrefabProvider* pxPrefabs, Zenith_SceneDocument* pxPrefabDocument,
	Zenith_Arena& xArena);   // returns the inverse
	// pxPrefabs: required for an edit that names an instance entity (to map a target of the resolved ID space to its
	// instance and prefab-local ID); pxPrefabDocument: the instanced prefab's own writable document, required for
	// SCENE_EDIT_APPLY_OVERRIDES_TO_PREFAB (whose inverse is an edit of the same kind restoring both documents) and
	// null for every other kind
Zenith_Result<void, Zenith_SceneError> Zenith_PasteEditSet(Zenith_SceneDocument& xDocument, const Zenith_SceneEditSet& xSet,
	const Zenith_PersistentID& xParent, Zenith_Arena& xArena, Zenith_SceneEdit& xInverseOut);
```

## Zenith_SceneData header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneData.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithReflect/Zenith_FieldSet.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithScene/Zenith_SceneErrors.h"

/// @contract enum Zenith_UnknownTypePolicy
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. FAIL: a component type absent from the registry fails the decode or preparation (SCENE_ERROR_UNKNOWN_TYPE);
///   the default and the startup scene's policy. SKIP: the component is not created, its field set is kept in the data's
///   unknown table and counted (scene.load.unknown_components); for tools and tests.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_UnknownTypePolicy : uint8
{
	UNKNOWN_TYPE_POLICY_FAIL = 1,
	UNKNOWN_TYPE_POLICY_SKIP,
};

/// @contract value Zenith_SceneEntityRecord
/// Ownership: Plain value; spans point into the owning Zenith_SceneData's arena.
/// Thread affinity: IMMUTABLE once the data is complete. Members non-const: the scene builder fills each record field
///   by field until the data is complete.
/// Invalidation: With the owning data.
/// Units: Indices into the owning data's entity array. 72 bytes: 16 + 4 + 4 + a 16-byte Zenith_StringView + two 16-byte
///   Zenith_Spans (Zenith2/Docs/Design/CORE/Text.md "Zenith_StringView", Zenith2/Docs/Design/CORE/Containers.md "Zenith_Span").
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SceneEntityRecord
{
	Zenith_PersistentID m_xID;
	uint32 m_uParentIndex = 0xFFFFFFFFu;                         // always lower than the record's own index
	uint32 m_uInstanceOrdinal = 0xFFFFFFFFu;
	Zenith_StringView m_strName;
	Zenith_Span<const Zenith_SerializedFieldSet> m_axComponents;  // ascending type ID; ECS field sets
	Zenith_Span<const uint32> m_auUnknownComponents;            // indices into Zenith_SceneData::GetUnknownComponents
};
static_assert(sizeof(Zenith_SceneEntityRecord) == 72);

/// @contract value Zenith_UnknownComponentData
/// Ownership: Plain value; the text lives in the owning data's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the owning data.
/// Units: Bytes of compact JSON.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_UnknownComponentData
{
	const uint32 m_uEntityIndex = 0;
	const Zenith_StringView m_strStableName = {};
	const Zenith_StringView m_strJson = {}; // the component object, compact, raw numbers preserved
};

/// @contract type Zenith_SceneData
/// Ownership: A resolved, flat scene; built by Zenith_ResolveDocument, Zenith_DecodeCookedScene or Zenith_CaptureEntities
///   into a caller-owned arena and owned by whoever owns that arena (the editor, the cook, a prepared scene).
/// Thread affinity: OWNER_THREAD while built; IMMUTABLE afterwards (a prepared scene hands it between the background
///   operation and the main thread only at completion).
/// Invalidation: Invalid when its arena is reset.
/// Units: Entities in activation order.
/// Errors: None of its own.
/// Version: since Z2 Stage 2. Never persisted as such: its persistent forms are the document and the cooked payload.
/// @endcontract
class Zenith_SceneData
{
public:
	Zenith_PersistentID GetSourceID() const;
	Zenith_Span<const Zenith_SceneEntityRecord> GetEntities() const;
	Zenith_Span<const Zenith_UnknownComponentData> GetUnknownComponents() const;
	Zenith_Span<const Zenith_PersistentID> GetAssetReferences() const;
	Zenith_Span<const Zenith_AssetKind> GetAssetReferenceKinds() const;   // parallel to GetAssetReferences
	uint64 GetResidentBytes() const;
};

/// @contract type Zenith_SceneDiagnostics
/// Ownership: Caller-owned collector of warnings and errors with positions and codes; text in its own arena, at most 4,096
///   entries (further entries are counted, not stored).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Views are invalid after Reset.
/// Units: 1-based lines and columns; entries in report order, uIndex from 0 to GetEntryCount() - 1.
/// Errors: None; an index at or beyond GetEntryCount() returns false, 0 and an empty view.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SceneDiagnostics
{
public:
	uint32 GetErrorCount() const;
	uint32 GetWarningCount() const;
	uint32 GetDroppedCount() const;
	uint32 GetEntryCount() const;                        // stored entries
	bool IsError(uint32 uIndex) const;
	// The Zenith_SceneErrorCode value when IsError(uIndex), else the Zenith_DocumentWarningCode value.
	uint8 GetCode(uint32 uIndex) const;
	Zenith_StringView GetMessage(uint32 uIndex) const;   // "<path>:<line>:<column>: <keyPath>: <message>"
	void Reset();
};
```

## Zenith_PrefabResolver header

The provider's role is `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Prefab provider"; resolution is `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Resolution algorithm".

```cpp
// Zenith2/Engine/ZenithScene/Zenith_PrefabResolver.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithScene/Zenith_SceneDocument.h"
#include "ZenithScene/Zenith_SceneData.h"
#include "ZenithScene/Zenith_SceneEdit.h"

class Zenith_TypeRegistry;
class Zenith_StringTable;

/// @contract interface Zenith_PrefabProvider
/// Ownership: Implemented by the editor (open and on-disk documents), the cook (the game's documents through its
///   read-only Zenith_DocumentMigrator) and tests (in-memory documents); owned by its implementer; borrowed by resolution,
///   cooking, edits and reference repair for the call.
/// Thread affinity: OWNER_THREAD. The thread resolving.
/// Invalidation: A document or resolved data a member returns is valid until the next call of Invalidate on the provider.
/// Units: ulContentKey is Zenith_Hash64 over the SHA-256 digests of the prefab and every document its resolution read,
///   in reading order (a session cache key, never persisted).
/// Errors: FindPrefab returns SCENE_ERROR_PREFAB_NOT_FOUND for an unknown ID and passes document errors through as
///   SCENE_ERROR_DOCUMENT (the Zenith_DocumentError is logged); FindResolved returns null on a miss; StoreResolved copies
///   the data and, when its cache is full, evicts the least recently used entry.
/// Reentrancy: NONE. Resolution calls the provider and the provider never calls back into resolution.
/// Version: since Z2 Stage 4. Resolution of scenes and flat prefabs takes a null provider (Stage 2 and Stage 3).
/// @endcontract
class Zenith_PrefabProvider
{
public:
	virtual ~Zenith_PrefabProvider() = default;
	virtual Zenith_Result<const Zenith_SceneDocument*, Zenith_SceneError> FindPrefab(const Zenith_PersistentID& xPrefab) = 0;
	virtual const Zenith_SceneData* FindResolved(const Zenith_PersistentID& xPrefab, uint64 ulContentKey) = 0;
	virtual void StoreResolved(const Zenith_PersistentID& xPrefab, uint64 ulContentKey, const Zenith_SceneData& xData) = 0;
	virtual void Invalidate(const Zenith_PersistentID& xPrefab) = 0;   // the document or a dependency changed
};

/// @contract functions Zenith_PrefabResolution
/// Ownership: Output data in the caller's arena.
/// Thread affinity: OWNER_THREAD — the provider's.
/// Invalidation: None.
/// Units: None.
/// Errors: SCENE_ERROR_PREFAB_NOT_FOUND, SCENE_ERROR_PREFAB_CYCLE, SCENE_ERROR_PREFAB_TOO_DEEP, SCENE_ERROR_DUPLICATE_ID,
///   SCENE_ERROR_SCHEMA, SCENE_ERROR_HIERARCHY_TOO_DEEP, SCENE_ERROR_OUT_OF_MEMORY; dangling overrides and unresolved
///   references are warnings in xDiagnostics. On failure xOut is empty.
/// Version: since Z2 Stage 2 (scenes without instances, null provider), Stage 3 (flat prefab documents, null provider,
///   Zenith2/Docs/Design/SCENE/Prefabs.md "Flat prefabs at Stage 3"), Stage 4 (instances, nesting, variants, a provider).
/// @endcontract
Zenith_Result<void, Zenith_SceneError> Zenith_ResolveDocument(const Zenith_SceneDocument& xDocument,
	Zenith_PrefabProvider* pxPrefabs, const Zenith_TypeRegistry& xTypes, Zenith_StringTable& xStrings,
	Zenith_Arena& xArena, Zenith_SceneData& xOut, Zenith_SceneDiagnostics& xDiagnostics);
Zenith_Result<Zenith_SceneEdit, Zenith_SceneError> Zenith_UnpackPrefabInstance(Zenith_SceneDocument& xDocument,
	const Zenith_PersistentID& xInstance, Zenith_PrefabProvider& xPrefabs, Zenith_Arena& xArena);   // returns the inverse
```

A null provider resolves a document that has no instance and no variant base, which is the Stage 2 path for scenes and the Stage 3 path for flat prefabs; a document with an instance or a variant base and a null provider is `SCENE_ERROR_PREFAB_NOT_FOUND`. `Zenith_UnpackPrefabInstance` returns the inverse edit, a `SCENE_EDIT_PLACE_INSTANCE` whose replaced-entity list is the unpacked entities.

## Zenith_SceneCooker header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneCooker.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCompression/Zenith_CompressionTypes.h"
#include "ZenithCompression/Zenith_CompressionCodecSet.h"
#include "ZenithAssets/Zenith_CookedFile.h"
#include "ZenithScene/Zenith_SceneData.h"
#include "ZenithScene/Zenith_SceneDocument.h"
#include "ZenithScene/Zenith_PrefabResolver.h"

class Zenith_TypeRegistry;
class Zenith_StringTable;

inline constexpr uint16 uSCENE_COOKED_FORMAT_VERSION = 1;

/// @contract enum Zenith_SceneSectionKind
/// Ownership: Plain enumeration; the section kinds of Format.md "Sections" in ASSET's section table.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. A value never changes meaning; a new kind bumps uSCENE_COOKED_FORMAT_VERSION.
/// @endcontract
enum Zenith_SceneSectionKind : uint16
{
	SCENE_SECTION_HEADER = 1,
	SCENE_SECTION_STRINGS,
	SCENE_SECTION_ENTITIES,
	SCENE_SECTION_COMPONENTS,
	SCENE_SECTION_ASSET_REFS,
	SCENE_SECTION_INSTANCES,
	SCENE_SECTION_UNKNOWN,
};

/// @contract value Zenith_CookedSceneSections
/// Ownership: Plain value; each span borrows decoded section bytes owned by the caller (a kind loader's object, a test).
/// Thread affinity: IMMUTABLE. Members non-const: the scene kind loader fills it section by section.
/// Invalidation: With the borrowed bytes.
/// Units: Decoded bytes of index 0 of each section kind.
/// Errors: None; an empty span for a required section makes the decode fail SCENE_ERROR_COOKED_CORRUPT.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CookedSceneSections
{
	Zenith_ByteSpan m_xHeader;
	Zenith_ByteSpan m_xStrings;
	Zenith_ByteSpan m_xEntities;
	Zenith_ByteSpan m_xComponents;
	Zenith_ByteSpan m_xAssetRefs;
	Zenith_ByteSpan m_xInstances;
};

/// @contract value Zenith_SceneCookSettings
/// Ownership: Plain value supplied by the cook per asset class.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_iLevel in the codec's own level units.
/// Errors: None of its own; a level or codec the writer refuses returns its error mapped to SCENE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SceneCookSettings
{
	const Zenith_CompressionCodecID m_eCodec = {}; // for STRINGS and COMPONENTS; the other sections are stored
	const sint32 m_iLevel = 0;
};

/// @contract value Zenith_CookedSceneInfo
/// Ownership: Plain value filled by the cooker; its two spans point into the cook's xScratch and are valid until that arena
///   is reset.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of the cooked-scene reader.
/// Invalidation: None.
/// Units: Counts; bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CookedSceneInfo
{
	uint32 m_uEntityCount = 0;
	uint32 m_uAssetReferenceCount = 0;
	uint32 m_uWarningCount = 0;
	uint64 m_ulSectionBytes = 0;            // decoded bytes over every section
	Zenith_Span<const Zenith_PersistentID> m_axAssetReferences;   // the IDs of the ASSET_REFS records, in their order
	Zenith_Span<const Zenith_PersistentID> m_axInstancedPrefabs;  // distinct prefab IDs the document's own instances name, ascending
};

/// @contract functions Zenith_SceneCooking
/// Ownership: Sections added to the caller's Zenith_CookedFileWriter (ASSET's), which the cook finishes; decoded data in
///   the caller's arena.
/// Thread affinity: OWNER_THREAD. The calling thread (the cook's worker, a background preparation slice, a test) owns the
///   output; the registry and the codec set are read-only for the call.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Cook: SCENE_ERROR_UNKNOWN_TYPE, every resolution error, SCENE_ERROR_INVALID_DESC (the writer refused a
///   section), SCENE_ERROR_OUT_OF_MEMORY. Decode: SCENE_ERROR_COOKED_VERSION, SCENE_ERROR_COOKED_CORRUPT (every bound
///   checked before use, [Z2-N-ARCH-951]), SCENE_ERROR_UNKNOWN_TYPE under ePolicy FAIL, SCENE_ERROR_OUT_OF_MEMORY.
///   Zenith_DecodeCookedSceneFile maps ASSET's parse errors to SCENE_ERROR_COOKED_CORRUPT (ASSET_ERROR_VERSION_MISMATCH to
///   SCENE_ERROR_COOKED_VERSION) and COMPRESS's decode errors to SCENE_ERROR_COOKED_CORRUPT. Never asserts on bytes.
/// Version: since Z2 Stage 2. The format of Format.md "Cooked scene payload", version uSCENE_COOKED_FORMAT_VERSION.
/// @endcontract
Zenith_Result<void, Zenith_SceneError> Zenith_CookSceneDocument(const Zenith_SceneDocument& xDocument,
	Zenith_PrefabProvider* pxPrefabs, const Zenith_TypeRegistry& xTypes, Zenith_StringTable& xStrings,
	const Zenith_SceneCookSettings& xSettings, Zenith_Arena& xScratch, Zenith_CookedFileWriter& xWriter,
	Zenith_CookedSceneInfo& xInfoOut, Zenith_SceneDiagnostics& xDiagnostics);
Zenith_Result<void, Zenith_SceneError> Zenith_DecodeCookedScene(const Zenith_CookedSceneSections& xSections,
	const Zenith_TypeRegistry& xTypes, Zenith_UnknownTypePolicy ePolicy, Zenith_Arena& xArena, Zenith_SceneData& xOut,
	Zenith_SceneDiagnostics& xDiagnostics);
// Tools, tests and the fuzz target: parses the container with Zenith_ParseCookedHeader against xExpect, verifies and
// decompresses each section through Zenith_DecompressSection, then calls Zenith_DecodeCookedScene.
Zenith_Result<void, Zenith_SceneError> Zenith_DecodeCookedSceneFile(Zenith_ByteSpan xFile,
	const Zenith_CookedFileExpectation& xExpect, const Zenith_CompressionCodecSet& xCodecs, const Zenith_TypeRegistry& xTypes,
	Zenith_UnknownTypePolicy ePolicy, Zenith_Arena& xArena, Zenith_SceneData& xOut, Zenith_SceneDiagnostics& xDiagnostics);
```

SCENE's cook steps build the `Zenith_CookedFileWriter`, pass it to `Zenith_CookSceneDocument`, which adds the six sections and fills `Zenith_CookedSceneInfo`, finish it, declare the hard references and write the file (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Scene and prefab cook steps"). `ZenithScene` keeps the `ZenithCompression` edge for `Zenith_CompressionCodecID`, `Zenith_CompressionCodecSet` and `Zenith_DecompressSection` (cross-area note S4; it is layer 1, so the layer is unchanged).

## Zenith_SceneAssets header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneAssets.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Hash.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithScene/Zenith_SceneCooker.h"
#include "ZenithScene/Zenith_SceneData.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_AssetRegistry;
class Zenith_SceneServices;

/// @contract type Zenith_Scene
/// Ownership: A cooked scene asset owned by the asset registry and referenced through Zenith_AssetHandle<Zenith_Scene>;
///   created by SCENE's scene kind loader, it owns copies of its decoded sections.
/// Thread affinity: ANY_THREAD for the accessors once resident (immutable).
/// Invalidation: Retired by the registry at reference count zero ([Z2-N-CONTRACT-083]); scenes are not cacheable.
/// Units: Bytes.
/// Errors: None; loading errors are the loader's (ASSET's load context).
/// Version: since Z2 Stage 2. Asset kind ASSET_KIND_SCENE (5), Zenith2/Docs/Design/ASSET.md "Asset kinds".
/// @endcontract
class Zenith_Scene
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_SCENE;
	const Zenith_CookedSceneSections& GetSections() const;
	Zenith_PersistentID GetDocumentID() const;
	Zenith_Hash128 GetContentHash() const;     // the cooked header's content hash
};

/// @contract type Zenith_Prefab
/// Ownership: As Zenith_Scene; additionally holds, once resident, the decoded Zenith_SceneData and the asset handles of its
///   dependencies, so instantiation is a copy with no decode.
/// Thread affinity: ANY_THREAD for the accessors once resident.
/// Invalidation: As Zenith_Scene; prefabs are cacheable, so a zero-reference prefab may stay resident in ASSET's cache.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Asset kind ASSET_KIND_PREFAB (6).
/// @endcontract
class Zenith_Prefab
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_PREFAB;
	const Zenith_SceneData& GetResolved() const;
	Zenith_PersistentID GetDocumentID() const;
};

/// @contract functions Zenith_SceneAssetKindRegistration
/// Ownership: Registers the two kind loaders the services own with the caller's registry.
/// Thread affinity: MAIN_THREAD, startup step 11, before Zenith_AssetRegistry::CloseRegistration.
/// Invalidation: None; the loaders live as long as the services, which the composition root destroys only after
///   UnregisterKindLoader of both kinds at shutdown step 6.
/// Units: None.
/// Errors: ASSET's RegisterKindLoader error, logged and mapped to SCENE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_SceneError> Zenith_RegisterSceneAssetKinds(Zenith_AssetRegistry& xAssets,
	Zenith_SceneServices& xServices);
```

`Zenith_RegisterSceneAssetKinds` calls `RegisterKindLoader(ASSET_KIND_SCENE, …, { "Scene.SceneLoader", uSCENE_COOKED_FORMAT_VERSION })` and `RegisterKindLoader(ASSET_KIND_PREFAB, …, { "Scene.PrefabLoader", uSCENE_COOKED_FORMAT_VERSION })` with the two `Zenith_AssetKindLoader` implementations `Zenith_SceneServices` owns (`Internal/Zenith_SceneAssets.cpp`; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader", "Zenith_AssetRegistry"). Their members:

| Member | Scene loader | Prefab loader |
|---|---|---|
| `BeginLoad` | check the section table `GetInfo().m_pxFile->m_axSections`: it must hold exactly six entries, of section kinds 1 to 6 in table order, each at index 0 (ASSET defines no reserved kind in container version 1); a `SCENE_SECTION_UNKNOWN` (7) entry, a second index of a kind, a missing kind or an undefined kind is `Fail(SCENE_ERROR_COOKED_CORRUPT)` with nothing requested, as `Zenith_DecodeCookedSceneFile` refuses the same file (`Zenith2/Docs/Design/SCENE/Format.md` → "Sections"); otherwise request those six table indices (0 to 5) in one round | the same |
| `OnSectionsLoaded` | copy the decoded sections into the `Zenith_Scene` object, report its resident bytes (`MEMORY_SUBSYSTEM_SIMULATION`), `Activate` | decode with `Zenith_DecodeCookedScene` (policy FAIL); copy the handles of `GetInfo().m_axDependencies` into the object, a counted reference each, in ASSET_REFS order: the span is in catalogue order and holds exactly the prefab's hard dependencies, its ASSET_REFS set (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Scene and prefab cook steps"), so each record takes the handle whose `GetPersistentID()` and `GetKind()` equal its ID and kind; acquire nothing, wait for nothing, then `Activate`. ASSET runs the loader only once every hard dependency is ACTIVE, or FAILED with an ACTIVE substitute (`Zenith2/Docs/Design/ASSET/Loading.md` → "Dependency loading"); a dependency that failed without one never reaches the loader, because ASSET fails the prefab `ASSET_ERROR_DEPENDENCY_FAILED`, which a preparation reports as `SCENE_ERROR_ASSET_UNAVAILABLE`. A decode failure, or a record with no matching handle (a count mismatch included, `SCENE_ERROR_COOKED_CORRUPT`), is `Fail` with the `Zenith_SceneErrorCode` as loader code, so "resident" for simulation implies instantiable |
| `Cancel` | release the partial object, `AcknowledgeCancel` | release the decoded data and the handles copied so far, `AcknowledgeCancel` |
| `Unload` | release the object, `AcknowledgeUnload` | release the data and the dependency handles, `AcknowledgeUnload` |
| `DropCpuCopies` | 0 (no GPU-resident data) | 0 |

## Zenith_SceneLoader header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneLoader.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_OperationHandle.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_MemoryGovernor.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithScene/Zenith_SceneAssets.h"
#include "ZenithScene/Zenith_SceneData.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_JobSystem;
class Zenith_AssetRegistry;
class Zenith_Heap;
class Zenith_FaultInjector;
class Zenith_WorldSimulation;
class Zenith_SceneServices;
class Zenith_SceneWorldState;

/// @contract type Zenith_SceneLoadDesc
/// Ownership: A value that owns one counted reference to its scene asset while m_xScene is non-null
///   (Zenith_AssetHandle, Zenith2/Docs/Design/ASSET/RuntimeAPI.md "Zenith_AssetHandle"): copying the desc adds a
///   reference, destroying it drops one. BeginPrepare copies the handle into the operation (one more reference, dropped
///   when the operation releases its scene) and copies m_strVirtualPath's bytes; the caller may destroy its desc on return.
/// Thread affinity: OWNER_THREAD. The thread that builds it; BeginPrepare reads it on the main thread.
/// Invalidation: m_strVirtualPath borrows the caller's bytes until BeginPrepare returns.
/// Units: None.
/// Errors: Neither or both of m_strVirtualPath and m_xScene set is SCENE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SceneLoadDesc
{
	Zenith_StringView m_strVirtualPath;                  // "content:/Scenes/Main.scene.json", or empty
	Zenith_AssetHandle<Zenith_Scene> m_xScene;           // or a handle (one counted reference)
	Zenith_PersistentID m_xIDScope;                      // null: authored IDs; otherwise every ID derived with this scope in BUILDING
	Zenith_UnknownTypePolicy m_eUnknownTypePolicy = UNKNOWN_TYPE_POLICY_FAIL;
	const char* m_szReason = "load";                     // static text, for logs and the operation's zone name suffix
};

/// @contract type Zenith_PreparedScene
/// Ownership: Created by a preparation operation, then owned by the requester once taken (TakePrepared) as a
///   Zenith_Owned<Zenith_PreparedScene>; owns its arena, scene data, init storage and every asset handle it acquired.
/// Thread affinity: OWNER_THREAD. The background operation until completion; the requester afterwards.
/// Invalidation: None while owned.
/// Units: Bytes.
/// Errors: None of its own.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_PreparedScene
{
public:
	const Zenith_SceneData& GetData() const;
	uint32 GetEntityCount() const;
	uint64 GetEncodedSpawnBytes() const;   // the command-buffer bytes one activation needs
	uint64 GetResidentBytes() const;
};

/// @contract value Zenith_ActivationProgress
/// Ownership: Plain value kept by the caller between activation slices.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Entities.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ActivationProgress
{
	uint32 m_uNextEntity = 0;
	bool m_bComplete = false;
};

/// @contract type Zenith_SceneLoader
/// Ownership: Created by Create as a Zenith_Owned<Zenith_SceneLoader>; one per process, owned by the composition root
///   (startup step 11, after the asset registry); tracks its operations in a fixed table of m_uMaxOperations (64).
/// Thread affinity: MAIN_THREAD for every member; the preparation runs on background jobs.
/// Invalidation: A prepared scene not taken is released when its handle is destroyed.
/// Units: Microseconds for budgets.
/// Errors: BeginPrepare returns SCENE_ERROR_INVALID_DESC, SCENE_ERROR_LIMIT (the operation table is full) or the job
///   system's refusal mapped to SCENE_ERROR_OUT_OF_MEMORY; subsequent failures are the operation's failure code (a
///   Zenith_SceneErrorCode). TakePrepared of an incomplete or failed operation is ZENITH_ASSERT and returns null.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SceneLoader
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_SceneLoader>, Zenith_SceneError> Create(const Zenith_SceneServices& xServices,
		Zenith_JobSystem& xJobs, Zenith_AssetRegistry& xAssets, Zenith_MemoryGovernor& xGovernor, Zenith_Heap& xHeap,
		Zenith_FaultInjector* pxFaults);
	Zenith_Result<Zenith_OperationHandle, Zenith_SceneError> BeginPrepare(const Zenith_SceneLoadDesc& xDesc);
	Zenith_Owned<Zenith_PreparedScene> TakePrepared(Zenith_OperationHandle& xOperation);
	Zenith_Result<void, Zenith_SceneError> LoadStartupScene(Zenith_StringView strVirtualPath,
		Zenith_WorldSimulation& xWorld, Zenith_SceneWorldState& xState);
	// Prologue step 7: first calls BeginPrepare for every pending additive preparation of every attached world (world-tag
	// order, then commit order; Loading.md "Additive load sequence" step 3), then advances every isolated-world activation
	// the runtime registered, within the budget.
	void StepActivations(uint64 ulBudgetUs);
	Zenith_Result<void, Zenith_SceneError> RegisterIsolatedActivation(Zenith_Owned<Zenith_PreparedScene> xPrepared,
		Zenith_WorldSimulation& xWorld, Zenith_SceneWorldState& xState, Zenith_OperationHandle& xCompletionOut);
	uint32 GetLiveOperationCount() const;
	Zenith_MemoryConsumer& GetMemoryConsumer();    // MEMORY_SUBSYSTEM_SIMULATION, Z2-B-SCENE-001
};

/// @contract functions Zenith_SceneActivationFunctions
/// Ownership: Spawn into the given world; the prepared scene or data stays owned by the caller.
/// Thread affinity: MAIN_THREAD with no frame of the world running (ApplyOutOfTick's rule).
/// Invalidation: None.
/// Units: Microseconds; entities.
/// Errors: SCENE_ERROR_ECS (a playback error, logged; the other records are applied — the caller then destroys the
///   isolated world, which is why activation into a live world never uses this path), SCENE_ERROR_DUPLICATE_ID,
///   SCENE_ERROR_WORLD_NOT_ATTACHED.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_SceneError> Zenith_ActivatePreparedOutOfTick(Zenith_PreparedScene& xPrepared,
	Zenith_WorldSimulation& xWorld, Zenith_SceneWorldState& xState, uint64 ulBudgetUs, Zenith_ActivationProgress& xProgress);
Zenith_Result<void, Zenith_SceneError> Zenith_ActivateSceneData(const Zenith_SceneData& xData,
	Zenith_WorldSimulation& xWorld, Zenith_SceneWorldState& xState, Zenith_AssetRegistry& xAssets);
```

## Zenith_AdditiveScenes header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_AdditiveScenes.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithScene/Zenith_SceneAssets.h"
#include "ZenithScene/Zenith_SceneData.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_TickBoundaryAccess;

/// @contract value Zenith_SceneInstanceID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None; an ID is never reused within its world, and names nothing in another world.
/// Units: A per-world counter from 1; 0 is none.
/// Errors: None.
/// Version: since Z2 Stage 3. Saved with the world's additive set (SAVE).
/// @endcontract
struct Zenith_SceneInstanceID
{
	uint32 m_uValue = 0;
};

/// @contract value Zenith_SceneLoadTicket
/// Ownership: Plain value returned by Zenith_RecordLoadAdditiveScene.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: Meaningful only for the buffer that recorded it, until the sync point that plays the buffer, where
///   Zenith_AdditiveSceneSet::GetCommittedLoads maps it to its instance ID.
/// Units: The record's index within its buffer; 0xFFFFFFFF is the null ticket.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SceneLoadTicket
{
	uint32 m_uBufferLocalIndex = 0xFFFFFFFFu;
};

/// @contract enum Zenith_SceneInstanceState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the transitions are Zenith2/Docs/Design/SCENE/Loading.md "Additive scene set".
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_SceneInstanceState : uint8
{
	SCENE_INSTANCE_STATE_LOADING = 1,
	SCENE_INSTANCE_STATE_QUEUED,
	SCENE_INSTANCE_STATE_ACTIVE,
	SCENE_INSTANCE_STATE_FAILED,
	SCENE_INSTANCE_STATE_UNLOADED,
};

/// @contract value Zenith_AdditiveLoadDesc
/// Ownership: Plain value copied into the command at record time.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own; a duplicate without m_bAllowDuplicate fails at commit with SCENE_ERROR_DUPLICATE_INSTANCE.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AdditiveLoadDesc
{
	const bool m_bAllowDuplicate = false;
	const Zenith_UnknownTypePolicy m_eUnknownTypePolicy = UNKNOWN_TYPE_POLICY_FAIL;
};

/// @contract value Zenith_CommittedSceneLoad
/// Ownership: Plain value in the span Zenith_AdditiveSceneSet::GetCommittedLoads returns.
/// Thread affinity: IMMUTABLE. Members non-const: held in the additive set's Zenith_Array, whose entries are removed in
///   place.
/// Invalidation: The span is replaced at the next sync point.
/// Units: m_xKey is the recording's merge key; the ticket is buffer-local.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CommittedSceneLoad
{
	Zenith_MergeKey m_xKey;
	Zenith_SceneLoadTicket m_xTicket;
	Zenith_SceneInstanceID m_xInstance;         // 0 when the commit failed
	Zenith_SceneErrorCode m_eError;             // 0 on success; DUPLICATE_INSTANCE or LIMIT
};

/// @contract value Zenith_SceneLoadResult
/// Ownership: Plain value; the 8-byte publication payload of ASYNC_SOURCE_SCENE_LOAD, encoded field by field
///   little-endian, never by memcpy (Z2-D-038).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_uInstance a Zenith_SceneInstanceID value (0: the load failed before an instance existed); m_uCode 0 or a
///   Zenith_SceneErrorCode; m_uDetail as Zenith_SceneError::m_uDetail.
/// Errors: Zenith_DecodeSceneLoadResult returns SCENE_ERROR_COOKED_CORRUPT for a payload that is not 8 bytes, a non-zero
///   reserved field or an undefined code.
/// Version: since Z2 Stage 3. Recorded in replay logs; the layout never changes.
/// @endcontract
struct Zenith_SceneLoadResult
{
	const uint32 m_uInstance = 0;
	const uint8 m_uCode = 0;
	const uint8 m_uDetail = 0;
	const uint16 m_uReserved = 0;
};
static_assert(sizeof(Zenith_SceneLoadResult) == 8);
inline constexpr uint32 uSCENE_LOAD_RESULT_BYTES = 8;

/// @contract functions Zenith_SceneLoadResultCoding
/// Ownership: Pure functions over the caller's bytes.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: As Zenith_SceneLoadResult.
/// Version: since Z2 Stage 3.
/// @endcontract
void Zenith_EncodeSceneLoadResult(const Zenith_SceneLoadResult& xResult, uint8 (&auOut)[8]);
Zenith_Result<Zenith_SceneLoadResult, Zenith_SceneError> Zenith_DecodeSceneLoadResult(Zenith_ByteSpan xPayload);

/// @contract type Zenith_AdditiveSceneSet
/// Ownership: Owned by Zenith_SceneWorldState; at most scene.maxAdditiveInstances (256) live records; storage from the
///   world heap reserved at creation.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Mutated on the world's sync-point node, its tick-boundary node and the
///   Scene.ActivateAdditive node; read from any node that declares a read of the resource "Zenith.SceneActivationQueue".
/// Invalidation: Spans returned by GetCommittedLoads are replaced at the next sync point.
/// Units: Instance IDs; ticks.
/// Errors: GetState of an unknown instance returns SCENE_INSTANCE_STATE_UNLOADED and SCENE_ERROR_INSTANCE_NOT_FOUND
///   through GetError; a commit beyond 256 live records fails that load with SCENE_ERROR_LIMIT.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AdditiveSceneSet
{
public:
	Zenith_SceneInstanceState GetState(Zenith_SceneInstanceID xInstance) const;
	Zenith_SceneErrorCode GetError(Zenith_SceneInstanceID xInstance) const;   // 0 when none
	Zenith_Span<const Zenith_EntityHandle> GetRoots(Zenith_SceneInstanceID xInstance) const;
	Zenith_PersistentID GetScene(Zenith_SceneInstanceID xInstance) const;
	// The loads committed at the latest sync point: (merge key of the record, ticket, instance ID).
	Zenith_Span<const Zenith_CommittedSceneLoad> GetCommittedLoads() const;
	uint32 GetLiveCount() const;
};

/// @contract functions Zenith_AdditiveSceneRecording
/// Ownership: Record into the caller's buffer.
/// Thread affinity: OWNER_THREAD — the buffer's; xSet is read-only and requires a declared read of the queue resource.
/// Invalidation: The ticket is meaningful only for xBuffer and resolves at its sync point.
/// Units: None.
/// Errors: None at record time; failures are the instance's state and error.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_SceneLoadTicket Zenith_RecordLoadAdditiveScene(Zenith_CommandBuffer& xBuffer,
	Zenith_AssetHandle<Zenith_Scene> xScene, const Zenith_AdditiveLoadDesc& xDesc);
void Zenith_RecordUnloadAdditiveScene(Zenith_CommandBuffer& xBuffer, const Zenith_AdditiveSceneSet& xSet,
	Zenith_SceneInstanceID xInstance);

/// @contract functions Zenith_ScenePublishCallback
/// Ownership: A function pointer SCENE passes to SetPublishHandler with its per-world user data.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Invoked on the world's tick-boundary node, step 1, one publication at a time
///   in completion-ID order, by ZenithRuntime's Zenith_AsyncPublicationHandler forwarder.
/// Invalidation: xPayload and xWorld are valid for the call only.
/// Units: As Zenith_ScenePublicationSink; xPayload is a Zenith_SceneLoadResult encoding.
/// Errors: None; a publication naming an unknown completion ID (an untagged instance unloaded while loading, or a
///   restore re-request SCENE refused) is counted and ignored.
/// Reentrancy: NONE. SCENE's callback changes only SCENE's per-world records and queue (Loading.md "Publication
///   service"); it uses no member of xWorld, records no command and never calls back into the sink.
/// Version: since Z2 Stage 3. Declared before the interface that takes it. xWorld is the tick boundary's
///   Zenith_TickBoundaryAccess, the access SCRIPT's handler receives (Zenith2/Docs/Design/SCRIPT/PublicAPI.md
///   "Zenith_ScriptHost header").
/// @endcontract
using Zenith_PfnScenePublish = void (*)(uint64 ulCompletionID, uint8 uPublicationKind, Zenith_ByteSpan xPayload,
	Zenith_TickBoundaryAccess& xWorld, void* pUserData);

/// @contract interface Zenith_ScenePublicationSink
/// Ownership: Implemented by ZenithRuntime per world over SCRIPT's asynchronous result table; borrowed by
///   Zenith_SceneWorldState for the world's lifetime (Zenith_SceneWorldDesc::m_pxPublication).
/// Thread affinity: ANY_THREAD_EXCEPT(SetPublishHandler, AllocateCompletionID). SetPublishHandler on the main thread in
///   Zenith_SceneWorldState::Attach; AllocateCompletionID on the world's sync-point node in merge order; Park from any
///   thread (a preparation's completion continuation); the handler is invoked on the world's tick-boundary node at step 1
///   in completion-ID order.
/// Invalidation: None.
/// Units: Completion IDs are the world's; uPublicationKind is a Zenith_PublicationKind value (Zenith2/Docs/Architecture.md
///   "Multiplayer foundations") carried as uint8.
/// Errors: AllocateCompletionID (C++ requests only) returns 0 when the ASYNC_SOURCE_SCENE_LOAD source is at its in-flight
///   limit (SCENE then fails the load with SCENE_ERROR_LIMIT); Park of a completion ID the table did not allocate for
///   ASYNC_SOURCE_SCENE_LOAD (by this sink or by SCRIPT's commit of a script request) is ZENITH_ASSERT, a programmer error
///   of engine code.
/// Reentrancy: NONE. The handler never calls back into the sink.
/// Version: since Z2 Stage 3. The mapping onto SCRIPT's table is Zenith2/Docs/Design/SCENE/Loading.md "Publication service".
/// @endcontract
class Zenith_ScenePublicationSink
{
public:
	virtual ~Zenith_ScenePublicationSink() = default;
	virtual void SetPublishHandler(Zenith_PfnScenePublish pfnPublish, void* pUserData) = 0;
	virtual uint64 AllocateCompletionID() = 0;
	virtual void Park(uint64 ulCompletionID, uint8 uPublicationKind, Zenith_ByteSpan xPayload) = 0;
};
```

The listing above is the Stage 3 form the API-only slice declares. The Stage 3 save entries (`Zenith_SavedSceneInstance`, `ForEachInstance`, `RestoreInstances`, `m_ulHookTag`) and the Stage 4 activation-hook members are additions declared by their own slices, specified in `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Activation hook declarations".

## Zenith_SceneCapture header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneCapture.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithScene/Zenith_SceneData.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_WorldInspector;
class Zenith_TypeRegistry;
class Zenith_StringTable;
class Zenith_Arena;

/// @contract functions Zenith_SceneCaptureFunctions
/// Ownership: Output data in the caller's arena.
/// Thread affinity: MAIN_THREAD while no frame of the world runs (the inspector's rule).
/// Invalidation: None.
/// Units: None.
/// Errors: SCENE_ERROR_ECS (an inspector error, logged), SCENE_ERROR_OUT_OF_MEMORY; roots that name no live entity are
///   skipped and counted (scene.carryover.missing).
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_SceneError> Zenith_CaptureEntities(const Zenith_WorldInspector& xWorld,
	const Zenith_TypeRegistry& xTypes, const Zenith_StringTable& xStrings, Zenith_Span<const Zenith_PersistentID> axRoots,
	Zenith_Arena& xArena, Zenith_SceneData& xOut);
Zenith_Result<void, Zenith_SceneError> Zenith_CaptureWorld(const Zenith_WorldInspector& xWorld,
	const Zenith_TypeRegistry& xTypes, const Zenith_StringTable& xStrings, Zenith_Arena& xArena, Zenith_SceneData& xOut);
```

`Zenith_CaptureWorld` captures every entity (editor play copy); both use `Zenith_CaptureFieldSet` with `bSerializedOnly = true` and a `Zenith_ReferenceResolver` that maps entity handles to persistent IDs (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_FieldSet header").

## Zenith_ReferenceRepair header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_ReferenceRepair.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithDocument/Zenith_DocumentMigrator.h"
#include "ZenithDocument/Zenith_DocumentDiff.h"
#include "ZenithScene/Zenith_PrefabResolver.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_Heap;

/// @contract enum Zenith_ReferenceRepairKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; each kind's effect is the table of Zenith2/Docs/Design/SCENE/Prefabs.md "Reference repair".
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_ReferenceRepairKind : uint8
{
	REFERENCE_REPAIR_ASSET_MOVED = 1,
	REFERENCE_REPAIR_ASSET_DELETED,
	REFERENCE_REPAIR_ENTITY_DELETED,
	REFERENCE_REPAIR_DUPLICATE_DOCUMENT,
};

/// @contract value Zenith_ReferenceRepairRequest
/// Ownership: Plain value; the path views are borrowed for the Zenith_PlanReferenceRepair call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Persistent IDs; game-relative document paths.
/// Errors: A field the kind requires left null or empty (m_xSubject always; m_xOwner for ENTITY_DELETED; both paths for
///   ASSET_MOVED) is SCENE_ERROR_INVALID_DESC at planning.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ReferenceRepairRequest
{
	const Zenith_ReferenceRepairKind m_eKind = {};
	const Zenith_PersistentID m_xSubject = {}; // the asset, prefab entity or duplicated document
	const Zenith_PersistentID m_xReplacement = {}; // ASSET_DELETED: null or the replacement asset
	const Zenith_PersistentID m_xOwner = {}; // ENTITY_DELETED: the prefab whose entity was removed
	const Zenith_StringView m_strOldPath = {}; // ASSET_MOVED
	const Zenith_StringView m_strNewPath = {};
	const bool m_bUnpackInstances = false;
	const bool m_bPruneDangling = false;
};

/// @contract type Zenith_ReferenceIndex
/// Ownership: Built by Build into its own heap storage; owned by the caller (EDIT's asset database session, CONTENT's
///   batch operation).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Stale after any document change it was not told about (Update).
/// Units: None.
/// Errors: Build returns the first document error as SCENE_ERROR_DOCUMENT; unreadable documents are listed and skipped.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_ReferenceIndex
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ReferenceIndex>, Zenith_SceneError> Build(Zenith_DocumentMigrator& xDocuments,
		Zenith_Span<const Zenith_StringView> astrPaths, Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_SceneError> Update(Zenith_DocumentMigrator& xDocuments, Zenith_StringView strPath);
	uint32 CountReferences(const Zenith_PersistentID& xID) const;
	uint32 ListReferencingDocuments(const Zenith_PersistentID& xID, Zenith_Span<Zenith_StringView> astrOut) const;
};

/// @contract type Zenith_ReferenceRepairPlan
/// Ownership: Owned by the caller; per document the new canonical bytes, the diff and the expected old digest.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Stale when a listed document changes (SaveDocuments then fails DOCUMENT_ERROR_STALE_PLAN).
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_ReferenceRepairPlan
{
public:
	Zenith_Span<const Zenith_DocumentWrite> GetWrites() const;
	Zenith_Span<const Zenith_DocumentDiff> GetDiffs() const;
	uint32 GetDanglingOverrideCount() const;
	Zenith_Result<void, Zenith_SceneError> WriteReportJson(Zenith_Array<char>& xOut) const;   // "zenith2.reference-repair" v1
};

Zenith_Result<void, Zenith_SceneError> Zenith_PlanReferenceRepair(const Zenith_ReferenceRepairRequest& xRequest,
	Zenith_DocumentMigrator& xDocuments, const Zenith_ReferenceIndex& xIndex, Zenith_PrefabProvider& xPrefabs,
	Zenith_ReferenceRepairPlan& xOut);
```

## Zenith_SceneDocumentTypes header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneDocumentTypes.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithReflect/Zenith_Migration.h"
#include "ZenithReflect/Zenith_ReflectionWalk.h"
#include "ZenithDocument/Zenith_CanonicalJson.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"

class Zenith_TypeRegistry;
class Zenith_DocumentTree;

inline constexpr const char* szSCENE_DOCUMENT_STABLE_NAME = "Zenith.SceneDocument";
inline constexpr const char* szPREFAB_DOCUMENT_STABLE_NAME = "Zenith.PrefabDocument";
inline constexpr uint32 uSCENE_DOCUMENT_VERSION = 1;
inline constexpr uint32 uPREFAB_DOCUMENT_VERSION = 1;

/// @contract functions Zenith_SceneDocumentTypeFunctions
/// Ownership: Registers into the caller's registry; returns static schemas.
/// Thread affinity: MAIN_THREAD before the registry's Freeze; the schema getters ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's Zenith_DocumentError, or the step registration's Zenith_MigrationError mapped to
///   DOCUMENT_ERROR_DUPLICATE_REGISTRATION.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterSceneDocumentTypes(Zenith_MigrationRegistry& xRegistry,
	const Zenith_TypeRegistry& xTypes);
const Zenith_CanonicalSchema& Zenith_GetSceneCanonicalSchema();
const Zenith_CanonicalSchema& Zenith_GetPrefabCanonicalSchema();
Zenith_Result<void, Zenith_MigrationError> Zenith_RewriteOverridePaths(Zenith_DocumentTree& xDocument,
	Zenith_StringView strStableName, const Zenith_PropertyPath& xFrom, const Zenith_PropertyPath& xTo);
```

`Zenith_RegisterSceneDocumentTypes` registers both types with suffixes `.scene.json` and `.prefab.json`, their canonical schemas, and an embedded field-set visitor that walks every entity's `components` and every override block's `addedComponents` and `properties` and applies ECS's component chains through `Zenith_MigrateFieldSet` (`Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration"); version 1 registers no document step.

## Zenith_SceneWorld header

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneWorld.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_ECSObservers.h"
#include "ZenithReflect/Zenith_ScriptBinding.h"
#include "ZenithScene/Zenith_TransformHierarchy.h"
#include "ZenithScene/Zenith_LifecycleListener.h"
#include "ZenithScene/Zenith_AdditiveScenes.h"
#include "ZenithScene/Zenith_SceneLoader.h"
#include "ZenithScene/Zenith_SceneErrors.h"

class Zenith_TypeRegistry;
class Zenith_StringTable;
class Zenith_AssetRegistry;
class Zenith_CompressionCodecSet;
class Zenith_Heap;
class Zenith_FaultInjector;
class Zenith_CommandKindTable;
class Zenith_WorldSimulation;

/// @contract value Zenith_SceneServicesDesc
/// Ownership: Plain value; pointers borrowed and outliving the services.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A null registry, string table, asset registry, codec set or heap is SCENE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SceneServicesDesc
{
	const Zenith_TypeRegistry* const m_pxTypes = nullptr;
	Zenith_StringTable* const m_pxStrings = nullptr;
	Zenith_AssetRegistry* const m_pxAssets = nullptr;
	const Zenith_CompressionCodecSet* const m_pxCodecs = nullptr;
	Zenith_Heap* const m_pxHeap = nullptr;
	Zenith_FaultInjector* const m_pxFaults = nullptr;
};

/// @contract type Zenith_SceneServices
/// Ownership: Created by Create as a Zenith_Owned<Zenith_SceneServices>; one per process, owned by the composition root;
///   holds the SCENE command-kind IDs after Zenith_RegisterSceneCommandKinds and owns the scene and prefab
///   Zenith_AssetKindLoader implementations Zenith_RegisterSceneAssetKinds registers (destroyed only after both kinds are
///   unregistered at shutdown step 6).
/// Thread affinity: MAIN_THREAD for Create and registration; IMMUTABLE afterwards.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns SCENE_ERROR_INVALID_DESC or SCENE_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SceneServices
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_SceneServices>, Zenith_SceneError> Create(const Zenith_SceneServicesDesc& xDesc);
	uint32 GetCommandKind(Zenith_StringView strName) const;   // 0 before registration
};

Zenith_Result<void, Zenith_SceneError> Zenith_RegisterSceneCommandKinds(Zenith_CommandKindTable& xKinds,
	Zenith_SceneServices& xServices);
// SCENE's Luau bindings (§ "Script bindings") into the engine binding table before it is sealed; covered by the
// Zenith_SceneServices block (MAIN_THREAD, registration errors are the table's, mapped to SCENE_ERROR_LIMIT). The services
// supply the command-kind IDs the bound functions record.
Zenith_Result<void, Zenith_SceneError> Zenith_RegisterSceneScriptBindings(Zenith_ScriptBindingTable& xBindings,
	const Zenith_SceneServices& xServices);

/// @contract interface Zenith_SceneTransitionSink
/// Ownership: Implemented by ZenithRuntime; borrowed by Zenith_SceneWorldState.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Called on the world's sync-point node (Scene.RequestTransition playback).
/// Invalidation: None.
/// Units: None.
/// Errors: The runtime's refusal is logged by the runtime and counted in scene.transition.refused.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SceneTransitionSink
{
public:
	virtual ~Zenith_SceneTransitionSink() = default;
	virtual void RequestTransition(uint16 uFromWorldTag, const Zenith_PersistentID& xScene,
		Zenith_Span<const Zenith_PersistentID> axCarryOver) = 0;
};

/// @contract value Zenith_SceneWorldDesc
/// Ownership: Plain value; pointers borrowed for the world state's lifetime.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Counts.
/// Errors: A null services or world is SCENE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SceneWorldDesc
{
	const Zenith_SceneServices* const m_pxServices = nullptr;
	Zenith_WorldSimulation* const m_pxWorld = nullptr;
	Zenith_SceneLoader* const m_pxLoader = nullptr;                  // null in tools and tests without loading
	Zenith_ScenePublicationSink* const m_pxPublication = nullptr;     // null: additive loads are refused (SCENE_ERROR_INVALID_DESC at record playback, counted)
	Zenith_SceneTransitionSink* const m_pxTransitions = nullptr;
	const uint32 m_uMaxAdditiveInstances = 256;
	Zenith_FaultInjector* const m_pxFaults = nullptr;
};

/// @contract type Zenith_SceneWorldState
/// Ownership: Created by Create as a Zenith_Owned<Zenith_SceneWorldState>; one per world, owned by the composition root
///   beside the world's Zenith_WorldSimulation and destroyed before it; owns the hierarchy cache, the lifecycle
///   dispatcher, the structural observer, the additive scene set, the activation queue and the propagation state.
/// Thread affinity: MAIN_THREAD for Create, Attach and destruction; AcceptScriptLoadRequest on the world's merge nodes
///   (one call at a time per world, the side-effect consumer's rule); its parts follow their own blocks.
/// Invalidation: Destroying it detaches it from the loader and unregisters nothing from the world (the world must be
///   destroyed at the same frame boundary).
/// Units: None.
/// Errors: Create unwinds on each failed step (fault points above); Attach returns the system registry's or the
///   dispatcher's error mapped to SCENE_ERROR_ECS / SCENE_ERROR_LIMIT.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SceneWorldState
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_SceneWorldState>, Zenith_SceneError> Create(const Zenith_SceneWorldDesc& xDesc);
	// Before the world's first tick: registers the structural observer, the tick-boundary handler (Loading.md
	// "Tick-boundary handler"), the two systems (Scene.ActivateAdditive, Scene.PropagateTransforms) and the two world
	// resources, installs SCENE's publish handler with the publication sink (SetPublishHandler) when one was given, and
	// attaches the world to the desc's loader so StepActivations starts its pending preparations.
	Zenith_Result<void, Zenith_SceneError> Attach();
	// ZenithRuntime's SIDE_EFFECT_KIND_ASYNC_REQUEST multiplexer, source kind 8, on the committing merge node in merge
	// order: appends (ulCompletionID, the 24-byte request of Loading.md "Script requests") to the script-load inbox, which
	// the tick-boundary handler commits. A malformed request is committed as a failed load (SCENE_ERROR_INVALID_DESC).
	// Cannot fail: the inbox holds scene.maxAdditiveInstances entries, the source kind's in-flight limit.
	void AcceptScriptLoadRequest(uint64 ulCompletionID, Zenith_ByteSpan xRequest);
	Zenith_TransformHierarchy& GetHierarchy();
	Zenith_LifecycleDispatcher& GetLifecycleDispatcher();
	const Zenith_AdditiveSceneSet& GetAdditiveScenes() const;
	uint64 GetResidentBytes() const;
};
```

```cpp
// Zenith2/Engine/ZenithScene/Zenith_SceneWorld.h (continued)
/// @contract type Zenith_SceneStructuralObserver
/// Ownership: Owned by Zenith_SceneWorldState, which registers it with its world in Attach; constructible alone by tests
///   that build worlds without the full state (the runtime's tests); it borrows the hierarchy and the dispatcher.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. ECS calls it on the world's sync-point node and during out-of-tick playback,
///   one event at a time in merge order.
/// Invalidation: None.
/// Units: Simulation IDs.
/// Errors: None; a depth-limit violation is corrected, logged and counted (Zenith2/Docs/Design/SCENE/Transforms.md
///   "Hierarchy maintenance at playback").
/// Reentrancy: NONE. It forwards events to the lifecycle dispatcher, whose listeners never call back into it.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SceneStructuralObserver final : public Zenith_StructuralObserver
{
public:
	Zenith_SceneStructuralObserver(Zenith_TransformHierarchy& xHierarchy, Zenith_LifecycleDispatcher& xDispatcher);
	// Zenith_StructuralObserver override (Zenith2/Docs/Design/ECS/WorldAPI.md "Zenith_ECSObservers header"),.
};
```

`Zenith_SceneStructuralObserver` is public only because the runtime's tests construct worlds without the full state; `Zenith_SceneWorldState` is its only production owner.

## Script bindings

SCENE's Luau bindings (`Zenith2/Docs/Scripting/Bindings.md` → "Binding registration and the thunk ABI"; the thunks live in ZenithScene and include `ZenithReflect/Zenith_ScriptBinding.h` only, [Z2-N-ARCH-085]):

| Function | Records | Returns |
|---|---|---|
| `Scene.Teleport(entity, position, rotation, scale?, flags?)` | `Scene.Teleport` | nothing |
| `Scene.Instantiate(prefab, position, rotation, parent?)` | the prefab's spawn records (the prefab spawn of `ctx:Spawn`, [Z2-N-SCRIPT-220]; at Stage 3 over flat cooked prefabs, `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Flat prefabs at Stage 3") | the root entity (provisional) |
| `Scene.SetParentKeepWorld(child, parent)` | `Scene.SetParentKeepWorld` | nothing |
| `Scene.LoadAdditive(scene, allowDuplicate?)` | one `SIDE_EFFECT_KIND_ASYNC_REQUEST` record of source kind 8, no ECS command; behaviours only | a `Zenith.AsyncTicket`, remapped to the load's one completion ID at commit; `ticket:Result()` is `{ instance, ok, error }` (`Zenith2/Docs/Design/SCENE/Loading.md` → "Script requests") |
| `Scene.Unload(instance)` | the destroys and `Scene.ForgetInstance` | nothing |
| `Scene.GetInstanceState(instance)` | nothing (a read of the queue resource) | the state name |
| `Scene.RequestTransition(scene, carryOver)` | `Scene.RequestTransition` | nothing |

Misuse (a nil prefab, a non-finite transform, an out-of-range flag) raises the catalogued Luau misuse error, never an assert ([Z2-N-CONTRACT-473]). The functions are registered by `Zenith_RegisterSceneScriptBindings` (declared in § "Zenith_SceneWorld header"; defined in `Internal/ScriptBindings/Zenith_SceneScriptBindings.cpp`), which the composition root calls with the other engine bindings; `Test_SceneScriptBindings_RecordCommandsAndRejectMisuse` exercises every one of them through the binding table without a Luau VM (`Zenith2/Docs/Design/SCENE/Tests.md` → "Diagnostics and binding tests").
