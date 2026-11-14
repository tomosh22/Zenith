# SCENE design — transforms, hierarchy and propagation

This file is part of the SCENE design family; the entry document is `Zenith2/Docs/Design/SCENE.md`. It specifies the transform components, the per-world hierarchy cache SCENE builds on ECS's parent relation, the phase-8 propagation system, transform change bits, teleport and the interpolation data render extraction reads. The phase contract is `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases" (phase 8, Z2-D-080); the parent relation is ECS's (`Zenith2/Docs/Design/ECS/Backends.md` → "Entity table"), and SCENE never keeps a second parent store (cross-area note E9).

## Transform value type

`Zenith_Transform` is the value type `Zenith2/Docs/Architecture.md` → "Purpose and scope" (table "Assumed types") assigns to SCENE: a local position, rotation and scale.

```cpp
// Zenith2/Engine/ZenithScene/Zenith_Transform.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Matrix.h"

/// @contract value Zenith_Transform
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: the value lives in component rows that systems, playback and property
///   sets write in place.
/// Invalidation: None.
/// Units: m_xPosition in metres; m_xRotation a unit quaternion (Zenith2/Docs/Design/CORE/Maths.md "Zenith_Quaternion");
///   m_xScale a dimensionless factor per local axis.
/// Errors: None. A non-unit rotation is normalised by every function below (Zenith_NormalizeOrIdentity); a scale
///   component whose magnitude is below fSCENE_MIN_SCALE is clamped to ±fSCENE_MIN_SCALE by Zenith_ToMatrix4 alone.
/// Version: since Z2 Stage 2. Serialized only through the Zenith.LocalTransform component's field set.
/// @endcontract
struct Zenith_Transform
{
	Zenith_Vector3 m_xPosition = xVECTOR3_ZERO;
	Zenith_Quaternion m_xRotation;                 // identity
	Zenith_Vector3 m_xScale = xVECTOR3_ONE;
	friend constexpr bool operator==(const Zenith_Transform&, const Zenith_Transform&) = default;
};
static_assert(sizeof(Zenith_Transform) == 40);

inline constexpr float fSCENE_MIN_SCALE = 1.0e-6f;

/// @contract functions Zenith_TransformFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Zenith_Transform; matrices act on column vectors (Zenith2/Docs/Design/CORE/Maths.md "Coordinate systems").
/// Errors: Zenith_TryInvertAffine returns false for a singular matrix and leaves xOut unchanged.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Matrix4 Zenith_ToMatrix4(const Zenith_Transform& xTransform);          // T · R · S
Zenith_Transform Zenith_DecomposeAffine(const Zenith_Matrix4& xMatrix, bool& bHasShearOut);
bool Zenith_TryInvertAffine(const Zenith_Matrix4& xMatrix, Zenith_Matrix4& xOut);
Zenith_Transform Zenith_ComposePlacement(const Zenith_Transform& xAt, const Zenith_Transform& xAuthored);
bool Zenith_IsBitwiseEqual(const Zenith_Transform& xA, const Zenith_Transform& xB);
```

| Function | Definition |
|---|---|
| `Zenith_ToMatrix4` | `Zenith_MakeTRS(position, NormalizeOrIdentity(rotation), clampedScale)` of CORE; the bottom row is `(0, 0, 0, 1)` |
| `Zenith_DecomposeAffine` | position = column 3; scale = the lengths of columns 0–2, with the sign of the determinant of the upper 3×3 applied to the x component (a mirror is carried in x); rotation = `Zenith_FromMatrix3` of the columns divided by their lengths; `bHasShearOut` is true when any pair of columns has `|dot| / (|a||b|)` above `fSCENE_SHEAR_EPSILON = 1e-5` |
| `Zenith_TryInvertAffine` | the inverse of the upper 3×3 by the adjugate over the determinant, translation `−R⁻¹ t`; `|det| < 1e-20` is singular |
| `Zenith_ComposePlacement` | position and rotation of `xAt`; scale = `xAt.scale × xAuthored.scale` per component; the prefab-root rule of `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Runtime instantiation" |
| `Zenith_IsBitwiseEqual` | a byte compare of the ten floats (so `−0` differs from `0` and NaN compares by its bits), the comparison propagation uses to decide "changed" |

<!-- informative -->
World transforms are matrices, not composed position-rotation-scale triples, because CORE fixes "world = parent × local" in matrix form (`Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"): a non-uniformly scaled, rotated parent produces shear in its children, which a triple cannot hold. Zenith kept a matrix per entity too; Unreal's `FTransform` hierarchy composes triples and drops the shear. The decomposed triple is kept beside the matrix because gameplay, physics, AI and audio read positions and rotations, never matrices.
<!-- /informative -->

## Transform components

Four components, registered by `Zenith_RegisterSceneTypes(Zenith_TypeRegistry&)` at startup step 6 (the engine-type declarations, `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"), each through `Zenith_TypeBuilder` (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder"):

| Component (C++ type) | Stable name | Version | Properties (ID, name, type, default) | Authority | Serialized |
|---|---|---|---|---|---|
| `Zenith_LocalTransform` | `Zenith.LocalTransform` | 1 | 1 `Position` VEC3 (0, 0, 0); 2 `Rotation` QUAT identity; 3 `Scale` VEC3 (1, 1, 1) | `PHASE_POST_PHYSICS`, with an arbitration rule `ARBITRATION_RULE_PRECEDENCE` over properties 1–3 in the default order | yes |
| `Zenith_WorldTransform` | `Zenith.WorldTransform` | 1 | 1 `Position` VEC3; 2 `Rotation` QUAT; 3 `Scale` VEC3; 4 `Row0` VEC4; 5 `Row1` VEC4; 6 `Row2` VEC4; 7 `Flags` U32 | `PHASE_TRANSFORM_PROPAGATION` | no (derived; recomputed at load) |
| `Zenith_PreviousWorldTransform` | `Zenith.PreviousWorldTransform` | 1 | 1 `Position` VEC3; 2 `Rotation` QUAT; 3 `Scale` VEC3 | `PHASE_TRANSFORM_PROPAGATION` | no |
| `Zenith_EntityName` | `Zenith.EntityName` | 1 | 1 `Name` STRING_ID (empty) | `PHASE_COMMANDS_ONLY` | yes |

```cpp
// Zenith2/Engine/ZenithScene/Zenith_TransformComponents.h
#pragma once
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithScene/Zenith_Transform.h"

class Zenith_TypeRegistry;

/// @contract value Zenith_LocalTransform
/// Ownership: Stored by the world's ECS backend as component "Zenith.LocalTransform".
/// Thread affinity: IMMUTABLE as a value; stored rows follow the world's phase access rules. Members non-const: the
///   value lives in component rows that systems, playback and property sets write in place.
/// Invalidation: None.
/// Units: Metres; unit quaternion; scale factors. Relative to the parent's world transform, or to the world for a root.
/// Errors: None; a spawn carrying a non-finite value is clamped by nothing: propagation writes the non-finite result and
///   counts scene.transform.non_finite (Zenith2/Docs/Design/SCENE.md "Failure handling").
/// Version: since Z2 Stage 2. Stable name and property IDs 1–3 are permanent.
/// @endcontract
struct Zenith_LocalTransform
{
	Zenith_Vector3 m_xPosition = xVECTOR3_ZERO;
	Zenith_Quaternion m_xRotation;
	Zenith_Vector3 m_xScale = xVECTOR3_ONE;
	static constexpr Zenith_PropertyID uPROP_POSITION{ 1 };
	static constexpr Zenith_PropertyID uPROP_ROTATION{ 2 };
	static constexpr Zenith_PropertyID uPROP_SCALE{ 3 };
	static const Zenith_TypeID xTYPE_ID;          // Zenith_MakeTypeID("Zenith.LocalTransform"), defined constexpr in the .inl
};
static_assert(sizeof(Zenith_LocalTransform) == 40);

/// @contract enum Zenith_WorldTransformFlags
/// Ownership: Plain enumeration; the bits of Zenith_WorldTransform::m_uFlags (property 7).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. A bit never changes meaning; new bits are appended.
/// @endcontract
enum Zenith_WorldTransformFlags : uint32
{
	WORLD_TRANSFORM_FLAG_NONE = 0,
	WORLD_TRANSFORM_FLAG_HAS_SHEAR = 1u << 0,      // Row0..Row2 is not representable by Position/Rotation/Scale
	WORLD_TRANSFORM_FLAG_TELEPORTED = 1u << 1,     // set by a teleport playback; cleared by the next propagation pass
	WORLD_TRANSFORM_FLAG_ORPHANED = 1u << 2,       // has a parent without a transform: computed as a root
	WORLD_TRANSFORM_FLAG_NON_FINITE = 1u << 3,     // the last computed matrix contains a NaN or an infinity
};

/// @contract value Zenith_WorldTransform
/// Ownership: Stored as component "Zenith.WorldTransform"; written only by Scene.PropagateTransforms (phase 8) and by
///   SCENE's playback functions (teleport, out-of-tick propagation).
/// Thread affinity: IMMUTABLE as a value; rows are readable by any phase, writable in phase 8 alone. Members non-const:
///   the value lives in component rows that systems, playback and property sets write in place.
/// Invalidation: None.
/// Units: Row0..Row2 are the first three rows of the local-to-world matrix (column vectors, metres); Position,
///   Rotation and Scale are Zenith_DecomposeAffine of that matrix.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized: derived state, recomputed after every load.
/// @endcontract
struct Zenith_WorldTransform
{
	Zenith_Vector3 m_xPosition = xVECTOR3_ZERO;
	Zenith_Quaternion m_xRotation;
	Zenith_Vector3 m_xScale = xVECTOR3_ONE;
	Zenith_Vector4 m_xRow0{ 1.0f, 0.0f, 0.0f, 0.0f };
	Zenith_Vector4 m_xRow1{ 0.0f, 1.0f, 0.0f, 0.0f };
	Zenith_Vector4 m_xRow2{ 0.0f, 0.0f, 1.0f, 0.0f };
	uint32 m_uFlags = WORLD_TRANSFORM_FLAG_NONE;
	static constexpr Zenith_PropertyID uPROP_POSITION{ 1 };
	static constexpr Zenith_PropertyID uPROP_ROTATION{ 2 };
	static constexpr Zenith_PropertyID uPROP_SCALE{ 3 };
	static constexpr Zenith_PropertyID uPROP_ROW0{ 4 };
	static constexpr Zenith_PropertyID uPROP_ROW1{ 5 };
	static constexpr Zenith_PropertyID uPROP_ROW2{ 6 };
	static constexpr Zenith_PropertyID uPROP_FLAGS{ 7 };
	static const Zenith_TypeID xTYPE_ID;
};
static_assert(sizeof(Zenith_WorldTransform) == 92);   // 40 decomposed + 3 × 16 rows + 4 flags; every member 4-byte aligned

/// @contract value Zenith_PreviousWorldTransform
/// Ownership: Stored as component "Zenith.PreviousWorldTransform"; written by Scene.PropagateTransforms and teleport playback.
/// Thread affinity: IMMUTABLE as a value; rows readable by any phase, writable in phase 8 alone. Members non-const: the
///   value lives in component rows that systems, playback and property sets write in place.
/// Invalidation: None.
/// Units: The decomposed world transform of the previous tick (metres, unit quaternion, factors).
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_PreviousWorldTransform
{
	Zenith_Vector3 m_xPosition = xVECTOR3_ZERO;
	Zenith_Quaternion m_xRotation;
	Zenith_Vector3 m_xScale = xVECTOR3_ONE;
	static constexpr Zenith_PropertyID uPROP_POSITION{ 1 };
	static constexpr Zenith_PropertyID uPROP_ROTATION{ 2 };
	static constexpr Zenith_PropertyID uPROP_SCALE{ 3 };
	static const Zenith_TypeID xTYPE_ID;
};
static_assert(sizeof(Zenith_PreviousWorldTransform) == 40);

/// @contract value Zenith_EntityName
/// Ownership: Stored as component "Zenith.EntityName"; set from a document entity's "name" at load and by commands.
/// Thread affinity: IMMUTABLE as a value. Members non-const: the value lives in component rows that systems, playback
///   and property sets write in place.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EntityName
{
	Zenith_StringID m_xName;
	static constexpr Zenith_PropertyID uPROP_NAME{ 1 };
	static const Zenith_TypeID xTYPE_ID;
};

/// @contract functions Zenith_SceneTypeRegistration
/// Ownership: Registers into the caller's registry; owns nothing.
/// Thread affinity: MAIN_THREAD, in the registry's declaration phase (startup step 6).
/// Invalidation: None.
/// Units: None.
/// Errors: Returns the registry's Zenith_ReflectError unchanged (a duplicate stable name, a missing authority).
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_ReflectError> Zenith_RegisterSceneTypes(Zenith_TypeRegistry& xTypes);
```

- `Zenith_RegisterSceneTypes` registers the four components above, the world-resource type `Zenith.TransformHierarchy` (owning phase `PHASE_TRANSFORM_PROPAGATION`), the world-resource type `Zenith.SceneActivationQueue` (owning phase `PHASE_INPUT`) and the payload structs of SCENE's command kinds (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleCommands header"), in that order.
- `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` register every property without `PROPERTY_FLAG_SERIALIZED` (ECS's opt-in flag, on by default: `Zenith_PropertyBuilder::Flags` is given a mask that excludes it), and properties 4 to 7 of `Zenith.WorldTransform` with `PROPERTY_FLAG_EDITOR_HIDDEN`, so `Zenith_CaptureFieldSet(…, bSerializedOnly = true, …)` excludes them (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_FieldSet header"); a document entity that lists either component is a `SCENE_ERROR_SCHEMA` load error naming the entity.
- `Zenith.WorldTransform` properties 1–7 carry `PROPERTY_FLAG_EDITOR_HIDDEN` except 1–3, so the inspector shows the decomposed world transform read-only and never the rows.

<!-- elaboration -->
<!-- assumption: pending GAME, PHYS, CINE -->
The authority of `Zenith.LocalTransform` is `PHASE_POST_PHYSICS` with a precedence arbitration over the default order cinematic > ragdoll > physics > animation > script, because phase 6 owns "physics-to-transform sync" (`Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases") and the cinematic, ragdoll and physics writers of a character's placement meet there. Every other writer — behaviours, chunk systems, AI, gameplay, the editor — changes a local transform through `Zenith_CommandBuffer::SetProperty` (applied at the next sync point) or `Zenith_RecordTeleport`. The D-PHASES refinement (open, Stage 1) may move the owning phase; propagation reads the property in phase 8 whichever phase owns it, so a move changes one registration line. The rows are cross-area notes S1 and S2.
<!-- /assumption -->

### Transform component helpers

```cpp
// Zenith2/Engine/ZenithScene/Zenith_TransformComponents.h (continued)
/// @contract value Zenith_TransformInitStorage
/// Ownership: Caller-owned bytes (a stack value, or arena storage per spawn) that Zenith_MakeTransformInits points the
///   three component inits into.
/// Thread affinity: OWNER_THREAD. The thread building the spawn.
/// Invalidation: The inits built over it are invalid when it is destroyed or reused.
/// Units: Bytes: one component value each (sizeof of the three components), plus the 13 property values (3 local, 7
///   world, 3 previous).
/// Errors: None.
/// Version: since Z2 Stage 2. The array sizes follow the static_asserts of the three components.
/// @endcontract
struct Zenith_TransformInitStorage
{
	uint8 m_auLocal[sizeof(Zenith_LocalTransform)];            // 40
	uint8 m_auWorld[sizeof(Zenith_WorldTransform)];            // 92
	uint8 m_auPrevious[sizeof(Zenith_PreviousWorldTransform)]; // 40
	Zenith_PropertyValue m_axValues[13];
};

/// @contract functions Zenith_TransformInitHelpers
/// Ownership: Writes into the caller's storage; the returned inits borrow it.
/// Thread affinity: ANY_THREAD.
/// Invalidation: The inits are invalid when xStorage is destroyed.
/// Units: As Zenith_Transform.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
uint32 Zenith_MakeTransformInits(const Zenith_Transform& xLocal, Zenith_TransformInitStorage& xStorage,
	Zenith_Span<Zenith_ComponentInit> axOut);   // writes 3 inits (local, world = local, previous = local); returns 3
```

A spawn made through `Zenith_MakeTransformInits` carries all three transform components from its first sync point. A spawn that carries `Zenith.LocalTransform` without the other two is repaired by the structural observer (§ "Hierarchy maintenance at playback", step 3).

## Hierarchy cache

`Zenith_TransformHierarchy` is a per-world, SCENE-owned world resource derived from ECS's parent relation; it holds no parent store of its own authority: every parent link it caches is copied from `Zenith_WorldPlaybackAccess`/`Zenith_EntityTable::GetParent` at the playback that changed it.

| Member | Type | Content |
|---|---|---|
| `m_xNodes` | `Zenith_HashMap<uint64, Zenith_HierarchyNode>` | one node per live entity that has a parent or has children, keyed by stable simulation ID |
| `m_axLevels[uSCENE_MAX_HIERARCHY_DEPTH + 1]` | `Zenith_Array<uint64>` per depth | simulation IDs of child entities at depth d ≥ 1, kept sorted ascending |
| `m_xDirty` | `Zenith_HashSet<uint64>` | children whose parent changed or whose depth changed since the last propagation pass |
| `m_uVersion` | `uint32` | incremented by every change; read by tests and the `scene.hierarchy.dump` command |

`Zenith_HierarchyNode` (24 bytes: the entity handle, the parent's simulation ID with 0 for a root, the depth, the child count) and `uSCENE_MAX_HIERARCHY_DEPTH` (64) are declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_TransformHierarchy header".

Memory: 24 bytes per node plus the hash overhead, 8 bytes per child in the level arrays; counted in `scene.hierarchy_bytes` and in Z2-B-SCENE-001.

### Hierarchy maintenance at playback

`Zenith_SceneStructuralObserver` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneWorld header") implements `Zenith_StructuralObserver` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSObservers header") and runs on the world's sync-point node, one event at a time, in merge order:

1. `STRUCTURAL_EVENT_SPAWNED`: read the entity's parent through the playback access; if non-null, insert the node with depth = parent depth + 1 (the parent inserted too if absent, as a depth-0 node), add the ID to the level array (binary-search insertion, O(n) worst case), increment the parent's child count, mark the ID dirty.
2. `STRUCTURAL_EVENT_PARENT_CHANGED` (the ECS event SCENE assumes, below): recompute the entity's depth and every descendant's (a depth-first walk over `Zenith_EntityTable::GetChildren` in ascending simulation ID), move each between level arrays, mark each dirty; a depth above `uSCENE_MAX_HIERARCHY_DEPTH` sets the entity back to root through `Zenith_WorldPlaybackAccess::SetParent(entity, null)`, logs `scene.hierarchy: depth limit 64 exceeded by <simID>; reparented to root`, and counts `scene.hierarchy.depth_rejections`.
3. `STRUCTURAL_EVENT_SPAWNED` with `Zenith.LocalTransform` present and `Zenith.WorldTransform` or `Zenith.PreviousWorldTransform` absent: add the missing components through `Zenith_WorldPlaybackAccess::AddComponent` with values equal to the local transform, count `scene.transform.auto_added`.
4. `STRUCTURAL_EVENT_DESTROYED`: remove the node, its level entry and the dirty mark; decrement the parent's child count; remove parent nodes that became childless roots.
5. Forward the event to the world's `Zenith_LifecycleDispatcher` (`LIFECYCLE_EVENT_SPAWNED`, `_DESTROY_PENDING`, `_DESTROYED`).

Out-of-tick playback (`Zenith_WorldSimulation::ApplyOutOfTick`, `Zenith2/Docs/Design/ECS/Scheduling.md` → "Out-of-tick playback") produces the same events, so a loaded scene's hierarchy is cached before its first tick. `Zenith_TransformHierarchy::Rebuild(const Zenith_WorldInspector&)` rebuilds the cache from scratch (every entity in ascending simulation ID) and is used by `Test_TransformHierarchy_IncrementalEqualsRebuild` and by the development console command `scene.hierarchy.validate`.

<!-- assumption: pending ECS -->
ECS's `Zenith_StructuralEventKind` has no event for a parent change. SCENE assumes a fourth enumerator, `STRUCTURAL_EVENT_PARENT_CHANGED`, delivered at playback step 6 in merge order after each accepted `ECS_RECORD_SET_PARENT` (and after `Zenith_WorldPlaybackAccess::SetParent`), with `m_xEntity` the child, and assumes that a `SET_PARENT` that would make an entity its own ancestor is dropped at playback with the counter `ecs.commands_rejected` and a log line; cross-area note S3.
<!-- /assumption -->

## Propagation system

One C++ system per world, registered by `Zenith_SceneWorldState::Attach` into the world's `Zenith_SystemRegistry` (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"):

```cpp
Zenith_SystemBuilder& xBuilder = xSystems.AddSystem(Zenith_SystemDesc{
	.m_szName = "Scene.PropagateTransforms",
	.m_ePhase = PHASE_TRANSFORM_PROPAGATION,
	.m_pfnRun = &Zenith_ScenePropagate_Run,
	.m_pfnMerge = &Zenith_ScenePropagate_Merge,
	.m_pUserData = pxSceneWorldState,
	.m_eAffinity = JOB_AFFINITY_ANY,
	.m_uCostHint = 50 });
xBuilder.Read(Zenith_LocalTransform::xTYPE_ID)
	.Write(Zenith_WorldTransform::xTYPE_ID)
	.Write(Zenith_PreviousWorldTransform::xTYPE_ID)
	.WriteResource(Zenith_TransformHierarchyResourceTypeID())
	.EmitsCommands()
	.Query(xAllDesc, pxState->m_xQueryAll)          // With Local (read), With World (write), With Previous (write)
	.Query(xMissingDesc, pxState->m_xQueryMissing)  // With Local (read), Without World
	.Query(xDerivedOnlyDesc, pxState->m_xQueryDerivedOnly); // With World (write), Without Local
```

| Declared access | Why |
|---|---|
| read `Zenith.LocalTransform` (all properties) | the input |
| write `Zenith.WorldTransform`, `Zenith.PreviousWorldTransform` (all properties) | the outputs; phase 8 owns them |
| write resource `Zenith.TransformHierarchy` | phase 8 is the resource's owning phase (`Zenith_RegisterSceneTypes` registers it with `PHASE_TRANSFORM_PROPAGATION`), so the propagation system is its one phase writer; ECS declares the write on the system's merge node (`Zenith2/Docs/Design/ECS/Scheduling.md` → "EmitInto translation"), the node body and its sub-jobs only read the resource, and the merge function clears the dirty set; the structural observer mutates the cache at sync points, which no node overlaps |
| emits commands | the late-add and derived-removal paths of steps 5 and 6 below |

Its node is `Scene.PropagateTransforms@W<tag>` in stage 8 of the TICK subgraph (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure"); its zone has the same name and is the MP-CPUZONE span of Z2-B-SCENE-002.

### Propagation state

<!-- elaboration -->
`Zenith_ScenePropagationState` is an internal member of `Zenith_SceneWorldState` (`Internal/Zenith_ScenePropagate.h`), not a world resource: only the propagation node, its continuations and its merge function touch it, and they run one after another. It is where the marks that carry work from one pass to the next live, because the hierarchy cache holds only what ECS's parent relation implies. It holds no change version: the row filter reads ECS's previous read version (`Zenith_SystemContext::GetPreviousReadVersion`, `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header"), which ECS stores when the merge completes and resets at `RestoreSimulationState` and at a version-wrap sweep, where a copy kept here would survive and then lie ahead of every re-stamped row (cross-area rows E20 and E24; `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries").

| Member | Type | Written by | Content |
|---|---|---|---|
| `m_bMergePending` | `bool` | the node (step 0 sets it); the merge (step 7 clears it) | an invocation's node has run and its merge has not: at the next step 0 it is still set exactly when a cancelled frame let the node finish and skipped its merge (`Zenith2/Docs/Design/ECS.md` → "Cancellation" item 2) |
| `m_bForcedPass` | `bool` | the node (step 0) | this pass is the one forced pass after such a cancellation: it writes and marks changed every row it visits whatever the bitwise comparison says (§ "Propagation algorithm" step 0) |
| `m_bFullPass` | `bool` | the node (step 0); out-of-tick propagation; the merge (clears it) | the pass compares every row: the first pass, the first pass after `RestoreSimulationState` or a version-wrap sweep reset, a pass after out-of-tick propagation |
| `m_axChangedLastPass` | `Zenith_Array<Zenith_EntityHandle>` | the merge | entities whose world transform the previous pass changed, ascending simulation ID |
| `m_xChangedThisPass` | `Zenith_HashSet<uint64>` | the node's continuations | simulation IDs whose world transform this pass changed |
| `m_xRevisit` | `Zenith_HashSet<uint64>` | the node (step 0) | simulation IDs this pass recomputes unconditionally, together with their children |
| `m_xRevisitNext` | `Zenith_HashSet<uint64>` | the final continuation (steps 5 and 6) | the marks for the next pass: every entity whose derived components this pass records to add or remove |

Each set is reserved at `Zenith_SceneWorldState::Create` for the world's entity capacity, so a pass allocates only in its node arena.

The tests read the state through one reader declared in the same internal header; it changes nothing, and nothing outside `ZenithScene_Tests` calls it:

```cpp
// Zenith2/Engine/ZenithScene/Internal/Zenith_ScenePropagate.h (continued)
/// @contract functions Zenith_ScenePropagationTestAccess
/// Ownership: Static readers; no state.
/// Thread affinity: MAIN_THREAD, out of tick (between frames of the world), so no pass is running.
/// Invalidation: None.
/// Units: Counts of simulation IDs; an entity is named by its handle and looked up by its simulation ID.
/// Errors: None; a stale handle is reported as not contained.
/// Version: since Z2 Stage 2. Test-only (ZenithScene_Tests).
/// @endcontract
class Zenith_ScenePropagationTestAccess
{
public:
	static uint32 GetRevisitCount(const Zenith_SceneWorldState& xWorld);            // m_xRevisit
	static uint32 GetRevisitNextCount(const Zenith_SceneWorldState& xWorld);        // m_xRevisitNext
	static bool RevisitContains(const Zenith_SceneWorldState& xWorld, Zenith_EntityHandle xEntity);
	static bool RevisitNextContains(const Zenith_SceneWorldState& xWorld, Zenith_EntityHandle xEntity);
	static bool IsFullPass(const Zenith_SceneWorldState& xWorld);                    // m_bFullPass
	static bool IsMergePending(const Zenith_SceneWorldState& xWorld);                // m_bMergePending
	static uint32 GetChangedLastPassCount(const Zenith_SceneWorldState& xWorld);    // m_axChangedLastPass
};
```

### Propagation algorithm

Per invocation (one per tick of the world):

0. **Pass setup.** `uPrev := GetPreviousReadVersion()`; `m_bForcedPass := m_bMergePending`; then `m_bMergePending := true`. When `uPrev == 0` (the system's first invocation, or the first after `RestoreSimulationState` or a sweep reset, when the world may have moved under the carried state): `m_bFullPass := true`, clear `m_axChangedLastPass` and both revisit sets. Otherwise, when `m_bForcedPass` is set: insert every element of `m_xRevisitNext` into `m_xRevisit` (the union of both sets, instead of the swap), then clear `m_xRevisitNext`; `m_axChangedLastPass` is kept as it is. Otherwise swap `m_xRevisit` and `m_xRevisitNext`, then clear `m_xRevisitNext`. Clear `m_xChangedThisPass`.
   - **Why the forced pass.** `m_bForcedPass` is set exactly when the previous invocation's node ran and its merge did not: a frame cancelled between the two lets the started node finish and skips the merge (`Zenith2/Docs/Design/ECS.md` → "Cancellation" item 2). That node had already swapped the sets and cleared the old next marks, and it wrote world rows in place, where they stay uncommitted. A plain next pass would therefore lose the marks the unmerged pass consumed (they are still in `m_xRevisit`, because only the merge clears it, and the union keeps them), and its bitwise comparison (steps 2.6 and 4.3) would find each moved row equal to the bits already stored and never mark it, so a `QUERY_TERM_CHANGED` consumer would miss the move and `Previous` would freeze. The forced pass visits what an ordinary pass visits (`uPrev` is still the read version of the last merged invocation, so every local commit since then is seen, together with the union of the marks, the dirty set the skipped merge did not clear, and `m_bFullPass` when it is still set), and it writes, marks changed and appends to the changed list every row it visits whatever the comparison says. That over-marks once, which is harmless under ECS's rule that a committed result does not depend on whether an unchanged row was handed to it (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"). A forced pass that is also a full pass (`uPrev == 0`) marks every row.
1. **Previous refresh.** For each handle in `m_axChangedLastPass`, in the list's order: resolve it with `GetChunkOfEntity(m_xQueryAll, …)`; a stale handle is skipped; set `Previous := World.{Position, Rotation, Scale}` and mark the Previous row changed. This keeps `Previous` equal to the preceding tick's world value for every entity, including one that stopped moving.
2. **Root pass** (continuation 1). `ParallelFor(GetChunkCount(m_xQueryAll), 1, RootBody)`, grain one chunk. For each row r of chunk c, with simulation ID s:
   1. `bLocalChanged = Zenith_IsVersionNewer(v1, uPrev) || Zenith_IsVersionNewer(v2, uPrev) || Zenith_IsVersionNewer(v3, uPrev)` over the row versions of the three Local properties (`Zenith_ChunkView::GetRowChangeVersions`), each compared under the ring order; the integer maximum of the three is never taken, because it picks the wrong version across a wrap of 1 … 2^32 − 1.
   2. `bTeleported = World.Flags & WORLD_TRANSFORM_FLAG_TELEPORTED`: clear the flag and mark the row changed (the teleport already wrote the transform).
   3. If `!bLocalChanged && !m_xRevisit.Contains(s) && !m_bFullPass`: continue.
   4. If s has a node in `m_xNodes` with a non-zero parent: continue (the level pass computes it).
   5. `M = Zenith_ToMatrix4(Local)`; compute `Decomposed`, flags.
   6. If `!m_bForcedPass` and `M`, `Decomposed` and flags are bitwise equal to the current row: continue.
   7. Write `World := (Decomposed, M rows, flags)`; `MarkRowChanged`; append s to the leaf's changed list (leaf arena).
3. **Merge of the changed set** (continuation 2): insert the leaves' lists into `m_xChangedThisPass`; the set's content does not depend on the leaf partition.
4. **Level passes** (continuations 3 … 2 + D, one per non-empty depth d = 1 … D in ascending order): `ParallelFor(m_axLevels[d].GetCount(), 256, LevelBody)`. For each child c (simulation ID at index i, parent p):
   1. `bDirty = m_bFullPass || bLocalChanged(c) || m_xChangedThisPass.Contains(p) || m_xDirty.Contains(c) || m_xRevisit.Contains(c) || m_xRevisit.Contains(p)`; if false, continue. The last two terms are the carried marks: c itself was late-added, or its parent lost or regained its derived components, since the previous pass.
   2. Resolve c and p with `GetChunkOfEntity`; if p lacks `Zenith.WorldTransform` (it is not in `m_xQueryAll`), compute c as a root and set `WORLD_TRANSFORM_FLAG_ORPHANED` (counter `scene.transform.orphaned`); otherwise the flag is clear in the result.
   3. `M = ParentRows × Zenith_ToMatrix4(Local(c))` (the parent's rows with bottom row `(0, 0, 0, 1)`); decompose; compare bitwise, flags included, except in a forced pass, which skips the comparison; write and mark as in step 2.6–2.7; append c to the leaf list.
   4. After the level's sub-jobs have joined (the next continuation), insert the level's lists into `m_xChangedThisPass` before level d + 1 starts.
5. **Late add** (final continuation): for each chunk of `m_xQueryMissing`, for each row (simulation ID s): record `AddComponent(Zenith.WorldTransform)` and `AddComponent(Zenith.PreviousWorldTransform)` into `Commands(s)`, with the world value step 4.2–4.3 would compute (from the parent's current world transform, as a root with `WORLD_TRANSFORM_FLAG_ORPHANED` when the parent has none, or as a plain root) and `Previous` equal to it; insert s into `m_xRevisitNext`; count `scene.transform.late_world_added`. The entity joins `m_xQueryAll` at phase 8's sync point, and the next pass recomputes it and its children through `m_xRevisit`, so children orphaned while it had no transform lose `WORLD_TRANSFORM_FLAG_ORPHANED` on the tick after the add.
6. **Derived removal** (same continuation): for each chunk of `m_xQueryDerivedOnly` (an entity that lost `Zenith.LocalTransform` through `RemoveComponent` and kept the derived components), for each row (simulation ID s): record `RemoveComponent(Zenith.WorldTransform)` and `RemoveComponent(Zenith.PreviousWorldTransform)` into `Commands(s)`, insert s into `m_xRevisitNext`, count `scene.transform.derived_removed`. From phase 8's sync point the entity has no transform at all, so no consumer reads a frozen world transform. On the next pass s is in `m_xRevisit`, so step 4.1 marks each child whose cached parent is s dirty whatever its own versions say, and step 4.2 computes it as a root with `WORLD_TRANSFORM_FLAG_ORPHANED`; its descendants follow through `m_xChangedThisPass`. During the tick of the removal the stale world transform is the last one computed, and its row is not marked changed.
7. **Merge** (`Zenith_ScenePropagate_Merge`, on the merge node): for each command buffer of the invocation in ascending base key, `AcceptBuffer` when `GetCapacity().TryAccept(ComputeDemand(buffer))` succeeds, else `RejectBuffer` (the entity still matches its query next tick, so its late add or removal is recorded again); `CommitAllWrittenRows()`; then the bookkeeping: `m_axChangedLastPass := m_xChangedThisPass` as handles in ascending simulation ID; clear the hierarchy's `m_xDirty` (the declared resource write); clear `m_xRevisit`; `m_bFullPass := false`; `m_bMergePending := false`; publish the counters. ECS then records this invocation's read version (`Zenith_MergeContext::GetReadVersion`, the merge point preceding the node in stage 8's order, never the merge version) as the previous read version the next pass reads in step 0, so a local-transform commit ordered after the node and before this merge is newer than it and is seen next tick (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_MergeContext header").

The merge commits every marked row at the system's precomputed merge version (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions"), so the row versions of `Zenith.WorldTransform` are the transform change bits phase 8 publishes (`Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases": "Publishes at its end: transform change bits"). A consumer reads them with a `QUERY_TERM_CHANGED` term on `Zenith.WorldTransform` or through `GetRowChangeVersions`.

| Property | Guarantee |
|---|---|
| deterministic | every output is a pure function of the committed local transforms, the parent links, the carried marks and the previous outputs; iteration order never reaches a value (levels in ascending depth, rows independent within a level; the sets are queried, never iterated, except `m_xChangedThisPass`, which step 7 sorts) |
| complexity | O(chunks + changed roots + Σ children that are dirty) per tick; an unchanged static world costs one version compare per row |
| parallelism | one leaf per chunk for roots; grain 256 for children; a level waits for the previous level only |
| exactness | a row is marked changed exactly when its bits changed, except in the one forced pass after a cancellation between the node and its merge (step 0), which marks every row it visits, so `QUERY_TERM_CHANGED` otherwise never fires for an unchanged world transform; every local-transform commit after the previous pass's read point is seen by the next pass; re-propagating an unchanged row, which a full pass or ECS's conservative filter may cause, writes nothing, so the world transforms do not depend on it |
| completeness | an entity whose parent gains or loses its derived components is recomputed on the next pass even when no version of its own moved |

### Out-of-tick propagation

`Zenith_PropagateTransformsOutOfTick` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_TransformHierarchy header") runs the same computation single-threaded on the main thread through the playback access, outside any frame of the world: every root, then every level, writing `World` and `Previous := World` for every entity (no change test). SCENE calls it after every activation of a scene, capture restore or carry-over deserialisation, so the first tick of a world sees correct world transforms and interpolation does not sweep from the origin; it clears the hierarchy's dirty set and both revisit sets and sets `m_bFullPass` (§ "Propagation state"), so the next pass compares every row; it leaves `m_bMergePending` as it is, so a forced pass still follows a cancellation between the node and its merge.

## Teleport

`Zenith_RecordTeleport` (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Teleport"; declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleCommands header") records the command kind `Scene.Teleport` with the 56-byte payload `Zenith_TeleportPayload` (entity, target transform, flags). Its playback function, on the sync-point node at step 7 in merge order:

1. Resolve the entity; stale → drop, `ecs.commands_stale` semantics through SCENE's counter `scene.teleport.stale`.
2. `SetProperty` the three Local properties to the target.
3. Compute the entity's world matrix from its parent's current world (or as a root) and write `World`, set `WORLD_TRANSFORM_FLAG_TELEPORTED`, write `Previous := World`.
4. Walk the descendants depth first in ascending simulation ID (`Zenith_EntityTable::GetChildren` through the playback access's entity table), recomputing each world from its parent's new world, setting the flag and `Previous := World` for each.
5. Dispatch `LIFECYCLE_EVENT_TELEPORTED` (payload `&Zenith_TeleportPayload`) to the listeners, then one `LIFECYCLE_EVENT_TELEPORTED` per descendant with the same payload pointer (listeners filter by entity), in the walk's order.

The next phase sees consistent world transforms; phase 8's previous refresh skips teleported rows (their `Previous` already equals `World`), and render extraction reads `WORLD_TRANSFORM_FLAG_TELEPORTED` to reset the instance's motion-vector history ([Z2-N-ARCH-550]).

## Interpolation data

SCENE owns the data, JOBS owns the interpolation factor (`sim.alpha`, `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_SimulationClock header"), and the extractors own the blend:

| Case | What an extractor renders |
|---|---|
| `WORLD_TRANSFORM_FLAG_HAS_SHEAR` clear | `Zenith_MakeTRS(Lerp(Previous.Position, World.Position, α), Nlerp(Previous.Rotation, World.Rotation, α), Lerp(Previous.Scale, World.Scale, α))` |
| `WORLD_TRANSFORM_FLAG_HAS_SHEAR` set | the current rows, uninterpolated |
| `WORLD_TRANSFORM_FLAG_TELEPORTED` set | the current transform, α ignored, motion-vector history reset |

`Zenith_InterpolateWorldTransform` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_TransformHierarchy header") implements the table so every extractor uses one formula.

## Reparenting helpers

`Zenith_RecordSetParentKeepWorld(xBuffer, xChild, xNewParent)` records the command kind `Scene.SetParentKeepWorld`. Its playback (step 7): read the child's current world matrix and the new parent's; compute `Local := Decompose(ParentWorld⁻¹ × ChildWorld)` (a singular parent matrix keeps the old local and counts `scene.reparent.singular`); `SetProperty` the local transform; call `Zenith_WorldPlaybackAccess::SetParent`. A plain `Zenith_CommandBuffer::SetParent` keeps the local transform instead, so the child moves with its new parent.

## Transform tests

The test catalogue is `Zenith2/Docs/Design/SCENE/Tests.md` → "Transform and hierarchy tests".
