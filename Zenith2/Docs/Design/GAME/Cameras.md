# GAME design — cameras

This file is part of the GAME design family; the entry document is `Zenith2/Docs/Design/GAME.md`. It specifies the gameplay camera layer of `ZenithGameplay` (inventory items Z2-I-GAME-03 cameras, Z2-I-GAME-17 camera collision and Z2-I-GAME-18 camera blending): virtual camera rigs, the per-camera director that selects and blends them, the collision query the director sweeps against, the system that runs it all in phase 7, and the camera-cut contract with RPROD's camera extractor. The render camera itself — the lens, the main-camera selection and the view request — is RPROD's `Zenith.Render.Camera` (`Zenith2/Docs/Design/RPROD/Extractors.md` → "Flux_RenderComponents header", "Camera extractor"); GAME never declares a second render camera type and never writes a view request.

## Scope of this file

| Topic | Heading |
|---|---|
| the model: rigs, directors, the output camera | "Camera model" |
| the two components | "Camera components" |
| the collision interface GAME relies on | "Collision query interface" |
| the per-tick algorithm | "Camera director system" |
| pure math | "Camera math functions" |
| what scripts call | "Camera bindings" |
| diagnostics | "Camera debug view" |

## Camera model

| Concept | Entity carries | Role |
|---|---|---|
| output camera | `Zenith.LocalTransform` (and the derived transforms), `Zenith.Render.Camera`, `Zenith.Gameplay.CameraDirector` | the one entity RPROD renders from; its pose is written only by its director (or by a cinematic while the director is suspended) |
| camera rig | `Zenith.Gameplay.CameraRig` (and a transform for the `FIXED` and `LOOK_AT` kinds) | a virtual camera: a rule that turns a target and inputs into a desired pose and field of view; never rendered itself |
| director | the `Zenith.Gameplay.CameraDirector` component of the output camera | selects one rig, blends from the previous pose to it, applies collision and smoothing state, and writes the output camera's local transform and field of view by commands |

Switching between rigs is a blend on one output camera, never a switch of the render camera: RPROD's extractor treats a change of the main camera as a camera cut that drops every temporal history (`Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera cuts"), so a smooth gameplay transition keeps one render camera and moves it. A real cut — a teleport, a scripted cut, a blend of zero seconds — is requested explicitly through `Zenith.Render.Camera`'s `CutRequest` (§ "Output and cuts").

## Camera components

### Zenith_GameplayCameraRigComponent

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayCameras.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract enum Zenith_GameplayCameraRigKind
/// Ownership: Plain enumeration; the values of Zenith_GameplayCameraRigComponent::m_eKind (property 16).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; any other value makes the rig ineligible for selection (counted gameplay.camera.bad_kind).
/// Version: since Z2 Stage 3. Serialized as a uint8; values never move.
/// @endcontract
enum Zenith_GameplayCameraRigKind : uint8
{
	CAMERA_RIG_KIND_FIXED = 1,          // the rig entity's own current world pose
	CAMERA_RIG_KIND_FOLLOW = 2,         // third-person orbit around the target with a spring-arm distance and collision
	CAMERA_RIG_KIND_FIRST_PERSON = 3,   // at the target's eye offset, oriented by the rig's yaw and pitch
	CAMERA_RIG_KIND_LOOK_AT = 4,        // at the rig entity's position, looking at the target
	CAMERA_RIG_KIND_TRACK_ENTITY = 5,   // exactly the target's current world pose (a cinematic camera entity, a camera socket)
};

/// @contract enum Zenith_GameplayCameraBlendCurve
/// Ownership: Plain enumeration; the values of the rig's BlendCurve (property 17) and the director's BlendCurve (property 14).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None; w is the blend weight for the normalised blend time s in [0, 1].
/// Errors: None; any other value is evaluated as CAMERA_BLEND_CURVE_LINEAR and counted (gameplay.camera.clamped).
/// Version: since Z2 Stage 3. Serialized as a uint8; values never move.
/// @endcontract
enum Zenith_GameplayCameraBlendCurve : uint8
{
	CAMERA_BLEND_CURVE_LINEAR = 0,      // w = s
	CAMERA_BLEND_CURVE_EASE_IN = 1,     // w = s²
	CAMERA_BLEND_CURVE_EASE_OUT = 2,    // w = 1 − (1 − s)²
	CAMERA_BLEND_CURVE_EASE_IN_OUT = 3, // w = s² (3 − 2s)
};

/// @contract value Zenith_GameplayCameraRigComponent
/// Ownership: Stored as component "Zenith.Gameplay.CameraRig".
/// Thread affinity: IMMUTABLE as a value; members non-const: an ECS component the store relocates and systems write
///   property by property; rows follow the authority column of Cameras.md "Zenith_GameplayCameraRigComponent".
/// Invalidation: None.
/// Units: Metres; radians for yaw and pitch; radians per second and metres per second for the rates; seconds for lags and
///   the blend; degrees for the field of view.
/// Errors: None; an out-of-range value is clamped by the director at evaluation and counted (gameplay.camera.clamped).
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_GameplayCameraRigComponent                 // 144 bytes
{
	Zenith_EntityHandle m_xTarget;                       // 1  Target
	Zenith_EntityHandle m_xDirector;                     // 2  Director: null = eligible for every director of the world
	Zenith_Vector3 m_xPivotOffset{ 0.0f, 1.6f, 0.0f };   // 3  PivotOffset: in the target's frame (FOLLOW, LOOK_AT), the camera's yaw frame (FIRST_PERSON)
	float m_fDistance = 4.0f;                            // 4  Distance: the default arm length (FOLLOW)
	float m_fMinDistance = 0.5f;                         // 5  MinDistance
	float m_fMaxDistance = 12.0f;                        // 6  MaxDistance
	float m_fMinPitchRadians = -1.2f;                    // 7  MinPitch
	float m_fMaxPitchRadians = 1.2f;                     // 8  MaxPitch
	float m_fPositionLagSeconds = 0.1f;                  // 9  PositionLag: 0 = rigid
	float m_fRotationLagSeconds = 0.05f;                 // 10 RotationLag: 0 = rigid
	float m_fFovYDegrees = 60.0f;                        // 11 FovYDegrees, (0, 179]
	float m_fCollisionRadius = 0.25f;                    // 12 CollisionRadius
	uint32 m_uCollisionLayerMask = 0xFFFFFFFFu;         // 13 CollisionLayerMask
	float m_fBlendInSeconds = 0.5f;                      // 14 BlendInSeconds: a blend toward this rig; 0 = cut
	sint32 m_iPriority = 0;                             // 15 Priority
	uint8 m_eKind = CAMERA_RIG_KIND_FOLLOW;             // 16 Kind
	uint8 m_eBlendCurve = CAMERA_BLEND_CURVE_EASE_IN_OUT;   // 17 BlendCurve
	uint8 m_bActive = 1;                                // 18 Active
	uint8 m_bCollision = 1;                             // 19 Collision
	uint8 m_bFollowPossessedPawn = 0;                   // 20 FollowPossessedPawn: target = the director's player's pawn
	uint8 m_auReserved[3] = {};
	float m_fYawRate = 0.0f;                             // 21 YawRate (input)
	float m_fPitchRate = 0.0f;                           // 22 PitchRate (input)
	float m_fZoomRate = 0.0f;                            // 23 ZoomRate (input)
	float m_fYaw = 0.0f;                                 // 24 Yaw (state)
	float m_fPitch = 0.0f;                               // 25 Pitch (state)
	float m_fArmDistance = 0.0f;                         // 26 ArmDistance (state): after zoom, before collision
	Zenith_Vector3 m_xSmoothedPivot;                     // 27 SmoothedPivot (state)
	Zenith_Quaternion m_xSmoothedRotation;               // 28 SmoothedRotation (state)
	float m_fCollidedDistance = 0.0f;                    // 29 CollidedDistance (state)
	uint8 m_bHasState = 0;                              // 30 HasState (state)
	uint8 m_auReserved2[3] = {};
	static const Zenith_TypeID xTYPE_ID;                 // "Zenith.Gameplay.CameraRig"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};
static_assert(sizeof(Zenith_GameplayCameraRigComponent) == 144);
```

| Properties | Authority | Written by |
|---|---|---|
| 1–20 (configuration) | `CommandsOnly` | authoring; scripts through `SetProperty` commands (§ "Camera bindings") |
| 21–23 (look and zoom input rates) | `EarlyScripts` | the game's input behaviour on the rig entity, directly (own entity, phase 2) — for example OpenWorldSlice's `Player/CameraInput.luau` (`Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Gameplay sample rules") |
| 24–30 (state) | `AINavigation` | `Gameplay.Cameras` alone; serialized, so a save restores the camera mid-orbit |

### Zenith_GameplayCameraDirectorComponent

```cpp
/// @contract value Zenith_GameplayCameraDirectorComponent
/// Ownership: Stored as component "Zenith.Gameplay.CameraDirector" on the output camera entity.
/// Thread affinity: IMMUTABLE as a value; members non-const: an ECS component the store relocates and systems write
///   property by property; rows follow the authority table below.
/// Invalidation: None.
/// Units: Metres; seconds; degrees for the field of view.
/// Errors: None; a director on an entity without Zenith.Render.Camera or Zenith.LocalTransform is skipped and logged once.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_GameplayCameraDirectorComponent            // 120 bytes
{
	Zenith_EntityHandle m_xManualRig;                    // 1  ManualRig: null = select by priority
	Zenith_EntityHandle m_xPlayer;                       // 2  Player: the player entity this director serves; null = whichever player names it
	Zenith_EntityHandle m_xActiveRig;                    // 3  ActiveRig (state)
	Zenith_Vector3 m_xBlendFromPosition;                 // 4  BlendFromPosition (state)
	Zenith_Quaternion m_xBlendFromRotation;              // 5  BlendFromRotation (state)
	float m_fBlendFromFov = 60.0f;                       // 6  BlendFromFov (state)
	float m_fBlendElapsed = 0.0f;                        // 7  BlendElapsed (state), seconds
	float m_fBlendDuration = 0.0f;                       // 8  BlendDuration (state), 0 = not blending
	Zenith_Vector3 m_xOutputPosition;                    // 9  OutputPosition (state)
	Zenith_Quaternion m_xOutputRotation;                 // 10 OutputRotation (state)
	float m_fOutputFov = 60.0f;                          // 11 OutputFov (state)
	uint32 m_uCutCounter = 0;                           // 12 CutCounter: scripts increment it to request a cut
	uint32 m_uLastCutCounter = 0;                       // 13 LastCutCounter (state)
	uint8 m_eBlendCurve = CAMERA_BLEND_CURVE_LINEAR;    // 14 BlendCurve (state)
	uint8 m_bSuspended = 0;                             // 15 Suspended: set at playback by the cinematic listener
	uint8 m_bHasOutput = 0;                             // 16 HasOutput (state)
	uint8 m_bResumeBlend = 0;                           // 17 ResumeBlend (state): a blend from the camera's current pose is due
	uint8 m_auReserved[4] = {};
	Zenith_EntityHandle m_xFollowPawn;                   // 18 FollowPawn: the possessed pawn of the player that names this director
	static const Zenith_TypeID xTYPE_ID;                 // "Zenith.Gameplay.CameraDirector"
};
static_assert(sizeof(Zenith_GameplayCameraDirectorComponent) == 120);
```

| Properties | Authority | Written by |
|---|---|---|
| 1, 2, 12 | `CommandsOnly` | authoring; scripts by `SetProperty` |
| 15 | `CommandsOnly` | the module's lifecycle listener, through the playback access at a sync point (§ "Cinematic suspension") |
| 3–11, 13, 14, 16, 17 | `AINavigation` | `Gameplay.Cameras` alone |
| 18 | `AINavigation` | `Gameplay.Rules` alone (`Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm" step 9), from the possession state of the player whose `CameraDirector` names this entity; it is how the director learns the possessed pawn without reading the player's `Pawn`, which `Gameplay.Rules` writes in the same phase |

## Collision query interface

Camera collision needs a geometric sweep against the world's physics geometry, but ZenithGameplay declares no dependency on ZenithPhysics (`Zenith2/Docs/Architecture.md` → "Library dependency graph"), and games that select Gameplay without Physics exist (`WorkloadIndoor`, `WorkloadUI2D` and `Game2D` in `Zenith2/Docs/Design/MOD.md` → "Repository games"). Module-to-module collaboration goes through world resources ([Z2-N-ARCH-850]), so the sweep is a world resource whose interface is declared in a library both modules depend on: ZenithScene, where `Zenith_LifecycleListener` is declared for the same reason.

<!-- assumption: pending SCENE, PHYS, ECS -->
GAME assumes the following header in ZenithScene (content specified here; SCENE lists the file and registers the resource type; cross-area rows GA2 and GA3), implemented per world by the Physics module, which adds the resource in its `AttachWorld` with owning phase `PHASE_PHYSICS` and declares `WriteResource` of it on `Physics.Step`. GAME also assumes from ECS that a system declaring `ReadResource` of a registered resource type that no module added to the world is accepted by `Freeze` and receives null from `Zenith_SystemContext::GetResource` (cross-area row GA1 (a)), so a game without Physics compiles its schedule and runs without camera collision.

```cpp
// Zenith2/Engine/ZenithScene/Zenith_CollisionQuery.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract value Zenith_CollisionSweepDesc
/// Ownership: Plain value passed by reference for one query.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
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
	const float m_fRadius = 0.0f;                // 0 = a ray
	const uint32 m_uLayerMask = 0xFFFFFFFFu;    // the physics collision layers the query hits
	const Zenith_EntityHandle m_xIgnoreRoot = {};     // bodies of this entity and of its descendants are ignored; null = none
};

/// @contract value Zenith_CollisionHit
/// Ownership: Plain value filled by a query.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
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
<!-- /assumption -->

## Camera director system

### Declared access

```cpp
Zenith_SystemBuilder& xCameras = xSystems.AddSystem(Zenith_SystemDesc{
	.m_szName = "Gameplay.Cameras",
	.m_ePhase = PHASE_AI_NAVIGATION,
	.m_pfnRun = &Zenith_GameplayCameras_Run,
	.m_pUserData = &xWorldState,
	.m_eAffinity = JOB_AFFINITY_ANY,
	.m_uCostHint = 10 });
xCameras.Write(Zenith_GameplayCameraRigComponent::xTYPE_ID, axRigStateProps)            // 24–30
	.Read(Zenith_GameplayCameraRigComponent::xTYPE_ID, axRigConfigAndInputProps)     // 1–23
	.Write(Zenith_GameplayCameraDirectorComponent::xTYPE_ID, axDirectorStateProps)   // 3–11, 13, 14, 16, 17
	.Read(Zenith_GameplayCameraDirectorComponent::xTYPE_ID, axDirectorConfigProps)   // 1, 2, 12, 15, 18 (18 written by Gameplay.Rules)
	.ReadAnyRow(Zenith_LocalTransform::xTYPE_ID, axLocalProps)                       // targets' and ancestors' local transforms
	.ReadAnyRow(Zenith_WorldTransform::xTYPE_ID, axWorldProps)                       // the TELEPORTED flag; orphan fallback
	.Read(xRenderCameraType, axRenderCameraProps)                                    // FovYDegrees, CutRequest (by stable name), own row
	.ReadResource(Zenith_TransformHierarchyResourceTypeID())
	.ReadResource(Zenith_CollisionQuery::xTYPE_ID)
	.EmitsCommands()
	.After("Gameplay.Rules")
	.Query(xDirectorsQuery, xState.m_xDirectorsSlot)   // With Director (state write), Render.Camera (read), LocalTransform (read)
	.Query(xRigsQuery, xState.m_xRigsSlot)             // With CameraRig (state write)
	.Query(xTransformsQuery, xState.m_xTransformsSlot); // With LocalTransform, WorldTransform (any-row reads)
```

`xRenderCameraType` and the two property IDs are resolved at `AttachWorld` with `Zenith_TypeRegistry::FindTypeByStableName("Zenith.Render.Camera")` and the type's property list (names `FovYDegrees`, type `PROPERTY_TYPE_F32`; `CutRequest`, `PROPERTY_TYPE_U32`), because ZenithGameplay declares no dependency on ZenithFlux, which holds `Flux_RenderComponents.h`; a missing type or a property of another type fails the attach with `GAMEPLAY_ERROR_MISSING_TYPE` (the stable names and property names are RPROD's permanent data contract, `Zenith2/Docs/Design/RPROD/Extractors.md` → "Render component registration").

The system declares `ReadAnyRow` of exactly two types, and each is legal under [Z2-N-ARCH-711] (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Authority and schedule checks" step 7) because no node of phase 7 declares a write of it:

| Type | Owning phase and direct writers | Why the system reads other rows |
|---|---|---|
| `Zenith.LocalTransform` | phase 6 (PostPhysics): the physics-to-transform sync, the ragdoll blend and the cinematic tracks; every other writer, `Gameplay.Cameras` included, uses `SetProperty` commands or teleport playback, which are not node writes | the targets' and their ancestors' poses (§ "Target pose evaluation") |
| `Zenith.WorldTransform` | phase 8 (TransformPropagation): `Scene.PropagateTransforms` and teleport playback | the `WORLD_TRANSFORM_FLAG_TELEPORTED` flag of a target, and the orphan fallback of the pose walk |

Every other column the system reads is read on the row being processed: the rig and director properties (which this system or `Gameplay.Rules` writes in phase 7, so `Freeze` marks them own-row-only) and the output camera's `Zenith.Render.Camera` properties, which live on the director's own entity. The possessed pawn reaches the director through its own property 18 `FollowPawn`, which `Gameplay.Rules` writes before this system runs (`.After("Gameplay.Rules")`), never through a read of `Zenith.Gameplay.Player`'s `Pawn`, which `Gameplay.Rules` writes in this phase. The passes of § "Director algorithm" keep every read and write of an own-row-only column on the row being iterated, carrying facts between directors and rigs through the node arena (the same discipline as `Zenith2/Docs/Design/GAME/Framework.md` → "Row ownership in the rules node").

### Director algorithm

`Zenith_GameplayCameras_Run`, once per tick t, dt = `GetTickDurationUs() / 1e6`, in four passes; within a pass rows are visited in chunk order, and no result depends on that order because every cross-row fact is collected into a node-arena table first and consumed in a following pass, sorted by simulation ID:

1. **Directors in.** Over the directors query, for each director row D of camera entity C: if C lacks `Zenith.Render.Camera` or `Zenith.LocalTransform`, log `gameplay.camera: director <simID> has no render camera or transform` once per entity and exclude D; otherwise record D's entry `{ simulation ID, ManualRig, FollowPawn, Suspended, ResumeBlend, ActiveRig, HasOutput, CutCounter, LastCutCounter, Output pose and fov, blend state, C's current world pose and FovYDegrees }` in the director table.
2. **Rigs in.** Over the rigs query, for each rig row R: record `{ simulation ID, Director, Active, Priority, Kind, BlendInSeconds, BlendCurve, FollowPossessedPawn, Target }` in the rig table. Then, from the two tables alone, run § "Rig selection" for every director in ascending simulation ID, resolve each eligible rig's effective target (rule 4) and compute each director's snap flag (step 6 of § "Output and cuts": D has no output, D's cut counter changed, the selected rig's effective target or C carries `WORLD_TRANSFORM_FLAG_TELEPORTED`, or a new selection with a zero blend).
3. **Rigs update.** Over the rigs query again, for each rig R eligible for some director (the selected one of each director and every other active rig naming that director or no director): when R is the selection of a snapping director, re-initialise its smoothing (§ "Output and cuts" step 1); update its state (§ "Look input integration", § "Smoothing"), so a rig that is not selected keeps a current state and a subsequent switch to it does not start from a stale orbit; when R is some director's selection, evaluate its live pose P_R = (position, rotation, fov) (§ "Rig kind evaluation", § "Collision resolution") into the rig table.
4. **Directors out.** Over the directors query, for each director D in the table: if D is suspended, write nothing to C, set `m_bResumeBlend = 1` and continue; otherwise apply the selection's switch and resume rules (§ "Rig selection" rules 5 and 6), blend (§ "Blending") to the output pose O, write O by commands (§ "Output and cuts") and update D's state properties in place.

Complexity O(directors × rigs + rigs × hierarchy depth of each target), one sweep per selected rig; one director per player is the expected count (single-player), and each rig update is a few dozen float operations.

### Rig selection

For director D at tick t, over the director and rig tables of § "Director algorithm" passes 1 and 2:

1. **Manual.** If `D.ManualRig` resolves to a live rig with `Active != 0` and an eligible director field (null or D), select it.
2. **Priority.** Otherwise, among live rigs with `Active != 0` whose `Director` is null or D, select the highest `Priority`; ties go to the lowest stable simulation ID. Rigs are gathered over the rigs query and compared by (priority descending, simulation ID ascending), so the choice is independent of chunk order.
3. **None.** No eligible rig: keep the last output (write nothing), counter `gameplay.camera.no_rig` once per tick.
4. **Target.** A rig's effective target is: when `FollowPossessedPawn != 0` and the `FollowPawn` of its director resolves, that pawn; otherwise `Target`. A rig's director is the director that selected it; for an unselected rig, the director its `Director` field names, or, when that field is null, the lowest-simulation-ID director of the table. `FollowPawn` is the director's own property 18, which `Gameplay.Rules` set earlier in the same phase from the possession of the player that names the director (`Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm" step 9), so a possession drained in this tick's rules pass retargets the rig in this tick. A target that does not resolve makes the rig fall back to its own entity's pose (`FIXED` semantics) for the tick, counter `gameplay.camera.lost_target`.
5. **Switch.** When the selected rig differs from `D.ActiveRig`: `D.ActiveRig = R`; if R's `BlendInSeconds > 0` and D has an output: `BlendFrom = D.Output` (position, rotation, fov), `BlendElapsed = 0`, `BlendDuration = R.BlendInSeconds`, `BlendCurve = R.BlendCurve`; otherwise a snap.
6. **Resume.** When `m_bResumeBlend` is set (the director was suspended and is no longer): `BlendFrom` = C's current world pose (the pose the cinematic left; the composition of § "Target pose evaluation" applied to C) and its current `FovYDegrees`; `BlendDuration = R.BlendInSeconds`, curve R's; clear `m_bResumeBlend`.

**Contention.** Directors select in ascending simulation ID (pass 2 of § "Director algorithm"), and a rig already selected this tick by a lower-simulation-ID director is not eligible for any director that follows it: a following director whose manual rig (rule 1) or rule-2 choice was taken counts `gameplay.camera.rig_contended` + 1 (once per director per tick) and continues with rule 2 over the rigs not yet taken (a taken manual rig falls through to rule 2), then rule 3 when none remains. A rig is therefore selected by at most one director per tick — a rig whose `Director` is null can be eligible for several — so its director, its effective target (rule 4: that director's `FollowPawn`) and its single state update in pass 3 are defined.

### Target pose evaluation

`Gameplay.Cameras` runs in phase 7, before phase 8 propagates this tick's world transforms, so it computes the current world pose of a target E from local transforms, which are final after phase 6:

1. Walk E's ancestors through the hierarchy resource (`Zenith_TransformHierarchy::Find(simID)->m_ulParentSimulationID`, `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_TransformHierarchy header") up to the root or 64 links, collecting the chain [root, …, E].
2. From the root down: `M = parentM × Zenith_ToMatrix4(LocalTransform)` (the root's parentM is the identity); an ancestor without `Zenith.LocalTransform` contributes its `Zenith.WorldTransform` rows as its matrix and the walk above it stops (the orphan rule of `Zenith2/Docs/Design/SCENE/Transforms.md` → "Propagation system").
3. Decompose M (`Zenith_DecomposeAffine`) to position and rotation; a matrix with shear uses its translation and the rotation of its normalised basis.

For a root target this is its local transform exactly; for a parented target (a camera socket on a vehicle) it equals what phase 8 will compute, so the camera never lags its target by a tick.

### Rig kind evaluation

With target pose (p_T, q_T), the rig entity's own pose (p_R, q_R) from § "Target pose evaluation", the rig's state (yaw ψ, pitch θ, arm a) and offset o:

| Kind | Desired pivot | Desired rotation | Desired position |
|---|---|---|---|
| `FIXED` | p_R | q_R | p_R |
| `FOLLOW` | p_T + q_T · o | `Zenith_MakeEuler(ψ, θ, 0)` | pivot − (rotation · (0, 0, −1)) × a, then collision (§ "Collision resolution") |
| `FIRST_PERSON` | p_T + `Zenith_MakeEuler(ψ, 0, 0)` · o | `Zenith_MakeEuler(ψ, θ, 0)` | pivot |
| `LOOK_AT` | p_T + q_T · o | `Zenith_MakeLookRotation(−(pivot − p_R), +Y)` (`Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems": a camera looks along its local −Z) | p_R |
| `TRACK_ENTITY` | p_T | q_T | p_T |

The field of view is the rig's `FovYDegrees` clamped to (0°, 179°]. Smoothing (§ "Smoothing") applies to the pivot and the rotation of `FOLLOW`, `FIRST_PERSON` and `LOOK_AT`; `FIXED` and `TRACK_ENTITY` are never smoothed, so a cinematic camera entity tracked by a rig is reproduced exactly.

### Look input integration

For `FOLLOW` and `FIRST_PERSON` rigs, per tick:

1. First evaluation (`HasState == 0`): ψ = `Zenith_GameplayWrapAngle(yaw of q_T + π)` (`Zenith_ToEuler`; the camera looks along its local −Z, so the added half turn puts it behind a target facing its +Z front), θ = 0 clamped to [MinPitch, MaxPitch], a = `Distance` clamped to [MinDistance, MaxDistance]; `SmoothedPivot` = the desired pivot, `SmoothedRotation` = the desired rotation, `CollidedDistance = a`; `HasState = 1`.
2. ψ += `YawRate` × dt, then wrapped into (−π, π] by subtracting or adding 2π once per overflow; θ = clamp(θ + `PitchRate` × dt, MinPitch, MaxPitch); a = clamp(a + `ZoomRate` × dt, MinDistance, MaxDistance).
3. A non-finite rate is treated as 0 and counted `gameplay.camera.non_finite_input`.

Rates, not deltas, so an input behaviour that runs once per tick in phase 2 integrates identically at any tick rate; a mouse delta is converted by the behaviour (`rate = delta / dt`).

### Smoothing

`αp = PositionLag > 0 ? 1 − exp(−dt / PositionLag) : 1`, `αr = RotationLag > 0 ? 1 − exp(−dt / RotationLag) : 1` (`std::exp` of `<cmath>`; deterministic per build and platform, the guarantee's scope, Z2-D-019); `SmoothedPivot = Lerp(SmoothedPivot, desiredPivot, αp)`; `SmoothedRotation = Zenith_Slerp(SmoothedRotation, desiredRotation, αr)` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Quaternion"). The `FOLLOW` desired position is then computed from the smoothed pivot and smoothed rotation, so collision tests the arm the camera actually uses.

### Collision resolution

For `FOLLOW` rigs with `Collision != 0`, given the smoothed pivot p, the arm direction d = −(rotation · (0, 0, −1)) and arm length a:

1. q = the `Zenith_CollisionQuery` resource (null → step 5).
2. Sweep `{ origin p, direction d, distance a, radius CollisionRadius, layers CollisionLayerMask, ignore root = the effective target }`.
3. Hit: `c = max(MinDistance, hit.m_fDistance)`; no hit: `c = a`.
4. Pull in at once, recover with the position lag: `CollidedDistance = (c < CollidedDistance) ? c : CollidedDistance + (c − CollidedDistance) × αp`.
5. No provider: `CollidedDistance = a`; counter `gameplay.camera.no_collision_provider` once per world (the first tick it happens).
6. Desired position = p + d × `CollidedDistance`.

The sweep starts at the pivot, which sits inside the target's capsule, so the ignore root (the target and its descendants) keeps the target's own bodies from pulling the camera in.

### Blending

While `BlendDuration > 0`: `BlendElapsed += dt`; s = clamp(BlendElapsed / BlendDuration, 0, 1); w = the curve of `BlendCurve` (enumeration comments); O.position = Lerp(BlendFrom.position, P_R.position, w); O.rotation = `Zenith_Slerp(BlendFrom.rotation, P_R.rotation, w)`; O.fov = BlendFrom.fov + (P_R.fov − BlendFrom.fov) × w; when s reaches 1, `BlendDuration = 0`. Otherwise O = P_R. A switch during a blend starts the new blend from the current blended output (§ "Rig selection" step 5 copies `D.Output`), so a chain of switches never jumps.

### Output and cuts

1. **Snap.** A director snaps when the snap flag of § "Director algorithm" pass 2 is set: in pass 3 the selected rig's state is re-initialised from step 1 of § "Look input integration" keeping ψ, θ and a (the orbit is not reset, the smoothing is); in pass 4 any blend is cancelled, O = P_R, and a cut is requested.
2. **Cut request.** `SetProperty(C, Zenith.Render.Camera, CutRequest, current + 1)` (the camera extractor treats a change of `CutRequest` as a camera cut, `Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera cuts"; current is read from C's row); `LastCutCounter = CutCounter`; counter `gameplay.camera.cuts`.
3. **Pose.** When O's position or rotation differs bitwise from C's current `Zenith.LocalTransform` (converted into C's parent frame when C has a parent: `local = inverse(parentWorldNow) × O`): `SetProperty(C, Zenith.LocalTransform, Position, …)` and `(…, Rotation, …)`. The commands are applied at sync point 7, before phase 8 propagates C's world transform, so the camera extractor of this frame renders the pose of this tick.
4. **Field of view.** When O.fov differs bitwise from C's `FovYDegrees`: `SetProperty(C, Zenith.Render.Camera, FovYDegrees, O.fov)`.
5. **State.** `OutputPosition`, `OutputRotation`, `OutputFov` = O; `HasOutput = 1`.

A teleport of the camera entity C itself is already a cut for the extractor (`Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera cuts", teleport condition); a teleport of the target is not, which is why the director requests one (answering LIGHT row L17: the teleported target's camera never smears its temporal history).

### Cinematic suspension

The module's lifecycle listener (`Zenith2/Docs/Design/GAME/Framework.md` → "Declared access") handles the cinematic events at the sync point that plays them:

| Event (`m_xEntity` = a bound entity) | Effect when the bound entity carries `Zenith.Gameplay.CameraDirector` |
|---|---|
| `LIFECYCLE_EVENT_CINEMATIC_START` | `SetProperty(Suspended, 1)` through the playback access |
| `LIFECYCLE_EVENT_CINEMATIC_INTERRUPT` | none (the cinematic blends out under its own precedence; the director stays suspended) |
| `LIFECYCLE_EVENT_CINEMATIC_SKIP`, `LIFECYCLE_EVENT_CINEMATIC_END` | `SetProperty(Suspended, 0)`; the next pass resumes with a blend from the camera's current pose (§ "Rig selection" step 6) |

While suspended the director writes nothing to C, so the cinematic's camera track, a `WRITER_SOURCE_CINEMATIC` writer of C's `Zenith.LocalTransform` in phase 6, is the only writer and nothing contradicts the default precedence ([Z2-N-ARCH-554]).

<!-- assumption: pending CINE -->
GAME assumes CINE dispatches each cinematic lifecycle event once per bound entity with `m_xEntity` set to that entity (the per-descendant dispatch pattern SCENE uses for teleports, `Zenith2/Docs/Design/SCENE/Transforms.md` → "Teleport" step 5), in addition to any event with a null entity; `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Listener interface" allows a null entity "for cinematic events that bind several entities". Cross-area row GA4.
<!-- /assumption -->

## Camera math functions

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayMath.h
/// @contract functions GameplayCameraMath
/// Ownership: Pure functions over the caller's values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Radians, metres, seconds, degrees as named.
/// Errors: None; non-finite inputs give the documented fallbacks.
/// Version: since Z2 Stage 3.
/// @endcontract
float Zenith_GameplayBlendWeight(Zenith_GameplayCameraBlendCurve eCurve, float fS);   // fS clamped to [0, 1]
float Zenith_GameplayLagAlpha(float fDeltaSeconds, float fLagSeconds);               // 1 for lag ≤ 0 or non-finite
float Zenith_GameplayWrapAngle(float fRadians);                                       // into (−π, π]
Zenith_Quaternion Zenith_GameplayOrbitRotation(float fYaw, float fPitch);            // Zenith_MakeEuler(yaw, pitch, 0)
Zenith_Vector3 Zenith_GameplayArmPosition(const Zenith_Vector3& xPivot, const Zenith_Quaternion& xRotation,
	float fArm);                                                                                     // pivot − (rotation · −Z) × arm
```

These are the functions the system uses and its unit tests check against analytic values (`Zenith2/Docs/Design/GAME/Tests.md` → "Camera tests").

## Camera bindings

Rig and director configuration is property data, so scripts change it with ordinary commands; the `@gameplay/Cameras` Luau helpers (`Zenith2/Docs/Design/GAME/Scripting.md` → "Library layout") wrap the common cases:

| Helper | Records |
|---|---|
| `Cameras.SetRigActive(ctx, rig, active)` | `ctx:Command(rig):Set(T.CameraRig.Active, active and 1 or 0)` |
| `Cameras.SetRigPriority(ctx, rig, priority)` | `ctx:Command(rig):Set(T.CameraRig.Priority, priority)` |
| `Cameras.SetRigTarget(ctx, rig, entity)` | `ctx:Command(rig):Set(T.CameraRig.Target, entity)` |
| `Cameras.ForceRig(ctx, director, rig?)` | `ctx:Command(director):Set(T.CameraDirector.ManualRig, rig)` (nil restores priority selection) |
| `Cameras.Cut(ctx, director)` | `ctx:Command(director):Set(T.CameraDirector.CutCounter, current + 1)`, reading the current value through `ctx:View(director, T.CameraDirector)` |

The look-input rates are written directly by a behaviour on the rig entity (properties 21–23, authority `EarlyScripts`), as OpenWorldSlice's input behaviour does.

## Camera debug view

The development console command `gameplay.camera.debug <0|1>` (registered through DEVT's console, `Zenith2/Docs/Design/GAME.md` → "Debug views and console commands") toggles a debug overlay drawn through DEVT's debug-draw path in the epilogue: each rig's desired and smoothed pivot (spheres), the arm and the sweep (a capsule, red where it hit), the active rig's name and priority, the blend weight, and the director's suspended state. The overlay reads the components only; it changes nothing the simulation sees.
