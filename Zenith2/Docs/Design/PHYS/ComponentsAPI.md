# PHYS design — the ECS-facing physics API

This file is part of the PHYS design family; the entry document is `Zenith2/Docs/Design/PHYS.md`. It specifies the public headers of `ZenithPhysics` that gameplay, scripts and the other modules use: the components and their per-property authority, the events, the world resource `Zenith.Physics.World`, the module commands and their recording helpers, ragdolls (including the declarations `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Ragdoll enter and recover" fixes), the character controller, vehicles, the debug-view flags, the two ZenithScene interfaces the Physics module implements, the Luau bindings, the configuration keys and the authority-census rows. The backend-neutral simulation API is `Zenith2/Docs/Design/PHYS/PublicAPI.md`.

## Scope of this file

| Header | Declares | Heading |
|---|---|---|
| `Zenith_PhysicsComponents.h` | `Zenith_RigidBodyComponent`, `Zenith_ColliderComponent`, `Zenith_JointComponent`, `Zenith_BuoyancyComponent`, the registration function | "Zenith_PhysicsComponents header" |
| `Zenith_PhysicsEvents.h` | the six event payload types | "Zenith_PhysicsEvents header" |
| `Zenith_PhysicsWorldResource.h` | `Zenith_PhysicsWorldResource`, `Zenith_PhysicsEntityHit`, `Zenith_PhysicsCommandKinds` | "Zenith_PhysicsWorldResource header" |
| `Zenith_PhysicsCommands.h` | the command payloads and `Zenith_RecordPhysics*` helpers | "Zenith_PhysicsCommands header" |
| `Zenith_Ragdoll.h` | `uPHYSICS_NO_IMPULSE_BONE`, `Zenith_RagdollEnterDesc`, `Zenith_RagdollRecoverDesc`, `Zenith_RecordEnterRagdoll`, `Zenith_RecordRecoverFromRagdoll`, `Zenith_RagdollState`, `Zenith_RagdollComponent`, `Zenith_RagdollPoseSource`, `Zenith_RagdollBodyPose`, `Zenith_RagdollAsset` | "Zenith_Ragdoll header" |
| `Zenith_CharacterController.h` | `Zenith_CharacterControllerMode`, `Zenith_CharacterControllerComponent` | "Zenith_CharacterController header" |
| `Zenith_PhysicsVehicle.h` | `Zenith_VehicleWheel`, `Zenith_VehicleWheelState`, `Zenith_VehicleComponent` | "Zenith_PhysicsVehicle header" |
| `Zenith_PhysicsDebug.h` | `Zenith_PhysicsDebugView` | "Zenith_PhysicsDebug header" |
| ZenithScene `Zenith_CollisionQuery.h`, `Zenith_FluidQuery.h` | the interfaces PHYS implements | "ZenithScene interfaces the Physics module implements" |

## Zenith_PhysicsComponents header

Every component is a C++-registered reflected type ([Z2-N-CONTRACT-372]) registered in `Zenith_Module_Physics_RegisterTypes` through `Zenith_RegisterPhysicsComponents`. Property IDs are never reused ([Z2-N-CONTRACT-374]); the property tables after the block are the registration, one row per `Zenith_TypeBuilder::Property` call. "Authority" is the owning phase of [Z2-N-ARCH-482]; `CommandsOnly` properties are authored data changed by `SetProperty` commands; "Serialized" is `PROPERTY_FLAG_SERIALIZED`.

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsComponents.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"
#include "ZenithPhysics/Zenith_PhysicsShape.h"
#include "ZenithPhysics/Zenith_PhysicsWorld.h"
#include "ZenithPhysics/Zenith_PhysicsMaterial.h"
#include "ZenithPhysics/Zenith_CollisionShapeAsset.h"
#include "ZenithPhysics/Zenith_PhysicsErrors.h"

class Zenith_TypeRegistry;

/// @contract value Zenith_RigidBodyComponent
/// Ownership: Component value stored through "Zenith.Physics.RigidBody" (version 1). An entity carrying it is one physics
///   body; its shape is its own Zenith.Physics.Collider plus the colliders of descendants without a rigid body of their
///   own (PHYS/Simulation.md "Body composition").
/// Thread affinity: Authority per property (table below): configuration CommandsOnly; runtime state PHASE_PHYSICS,
///   written by Physics.Step alone.
/// Invalidation: None.
/// Units: Kilograms, 1/s, m/s, rad/s, metres.
/// Errors: An invalid configuration value is reported at body creation (physics.body.invalid_delta, log category
///   physics) and the entity gets no body; never an assert.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_RigidBodyComponent
{
	uint8 m_eMotionType = PHYSICS_MOTION_TYPE_DYNAMIC;              // 1 MotionType
	uint8 m_eMotionQuality = PHYSICS_MOTION_QUALITY_DISCRETE;       // 6 MotionQuality
	uint8 m_uLayer = 0;                                             // 7 Layer
	bool m_bAllowSleeping = true;                                    // 8 AllowSleeping
	float m_fMass = 0.0f;                                            // 2 Mass (0: from shape and density)
	float m_fLinearDamping = 0.05f;                                  // 3 LinearDamping
	float m_fAngularDamping = 0.05f;                                 // 4 AngularDamping
	float m_fGravityFactor = 1.0f;                                   // 5 GravityFactor
	uint32 m_uFlags = PHYSICS_BODY_FLAG_REPORT_INTERACTIONS;        // 9 Flags (Zenith_PhysicsBodyFlags, TRIGGER excluded)
	uint32 m_uCollisionGroup = 0;                                   // 10 CollisionGroup
	Zenith_Vector3 m_xInitialLinearVelocity;                         // 11 InitialLinearVelocity (at creation only)
	Zenith_Vector3 m_xInitialAngularVelocity;                        // 12 InitialAngularVelocity
	float m_fMaxLinearSpeed = 0.0f;                                  // 13 MaxLinearSpeed (0: physics.maxLinearSpeed)
	Zenith_Vector3 m_xCentreOfMassOffset;                            // 14 CentreOfMassOffset
	Zenith_Vector3 m_xLinearVelocity;                                // 20 LinearVelocity (runtime)
	Zenith_Vector3 m_xAngularVelocity;                               // 21 AngularVelocity (runtime)
	bool m_bSleeping = false;                                        // 22 Sleeping (runtime)
	bool m_bFrozen = false;                                          // 23 Frozen (runtime: outside the active region)
	static const Zenith_TypeID xTYPE_ID;                             // Zenith_MakeTypeID("Zenith.Physics.RigidBody")
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract value Zenith_ColliderComponent
/// Ownership: Component value stored through "Zenith.Physics.Collider" (version 1). On an entity with a rigid body it
///   is that body's (first) shape; on a descendant without one it is a child of the nearest ancestor body's compound;
///   with no rigid body on itself or any ancestor it makes an implicit static body.
/// Thread affinity: CommandsOnly for every property.
/// Invalidation: None.
/// Units: Metres; unit quaternion.
/// Errors: An asset kind needing a resident Physics.CollisionShape that is absent or failed gives the entity no body
///   (physics.collider.shape_missing_delta); invalid sizes as Zenith_RigidBodyComponent.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_ColliderComponent
{
	uint8 m_eShapeKind = PHYSICS_SHAPE_KIND_BOX;                    // 1 ShapeKind
	bool m_bIsTrigger = false;                                       // 7 IsTrigger
	uint8 m_uStaticLayer = 0;                                       // 8 StaticLayer (implicit static bodies alone)
	uint8 m_uReserved = 0;
	Zenith_Vector3 m_xSize{ 0.5f, 0.5f, 0.5f };                     // 2 Size: sphere x = radius; box half extents;
	                                                                 //   capsule and cylinder x = radius, y = half height
	Zenith_AssetHandle<Zenith_CollisionShapeAsset> m_xShape;         // 3 Shape (cooked kinds 5 … 8)
	Zenith_AssetHandle<Zenith_PhysicsMaterialAsset> m_xMaterial;     // 4 Material (null: default; overrides a cooked palette)
	Zenith_Vector3 m_xLocalPosition;                                 // 5 LocalPosition (relative to the entity)
	Zenith_Quaternion m_xLocalRotation;                              // 6 LocalRotation
	static const Zenith_TypeID xTYPE_ID;                             // "Zenith.Physics.Collider"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract value Zenith_JointComponent
/// Ownership: Component value stored through "Zenith.Physics.Joint" (version 1); one constraint between the bodies of
///   BodyA and BodyB (BodyB null: the world). The component may sit on either body's entity or a third entity.
/// Thread affinity: Configuration CommandsOnly; Broken PHASE_PHYSICS.
/// Invalidation: A joint naming a stale or bodiless entity is inactive (physics.joint.unresolved_delta) until both
///   resolve.
/// Units: Metres and radians (limits, targets), newtons and newton-metres (motor, break thresholds), Hz.
/// Errors: As Zenith_PhysicsConstraintDesc, reported at constraint creation and counted.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_JointComponent
{
	uint8 m_eKind = PHYSICS_CONSTRAINT_FIXED;                       // 1 Kind
	uint8 m_eMotorMode = PHYSICS_MOTOR_OFF;                         // 12 MotorMode
	bool m_bCollideConnected = false;                                // 19 CollideConnected
	bool m_bEnabled = true;                                          // 20 Enabled
	Zenith_EntityHandle m_xBodyA;                                    // 2 BodyA
	Zenith_EntityHandle m_xBodyB;                                    // 3 BodyB
	Zenith_Vector3 m_xFrameAPosition;                                // 4 FrameAPosition
	Zenith_Quaternion m_xFrameARotation;                             // 5 FrameARotation
	Zenith_Vector3 m_xFrameBPosition;                                // 6 FrameBPosition
	Zenith_Quaternion m_xFrameBRotation;                             // 7 FrameBRotation
	Zenith_Vector3 m_xLinearLimitMin;                                // 8 LinearLimitMin
	Zenith_Vector3 m_xLinearLimitMax;                                // 9 LinearLimitMax
	Zenith_Vector3 m_xAngularLimitMin;                               // 10 AngularLimitMin
	Zenith_Vector3 m_xAngularLimitMax;                               // 11 AngularLimitMax
	Zenith_Vector3 m_xMotorTargetVelocity;                           // 13 MotorTargetVelocity
	Zenith_Vector3 m_xMotorTargetPosition;                           // 14 MotorTargetPosition
	float m_fMotorMaxForce = 0.0f;                                   // 15 MotorMaxForce
	float m_fMotorMaxTorque = 0.0f;                                  // 16 MotorMaxTorque
	float m_fBreakForce = 0.0f;                                      // 17 BreakForce (0: unbreakable)
	float m_fBreakTorque = 0.0f;                                     // 18 BreakTorque
	bool m_bBroken = false;                                          // 21 Broken (runtime)
	static const Zenith_TypeID xTYPE_ID;                             // "Zenith.Physics.Joint"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract value Zenith_BuoyancyComponent
/// Ownership: Component value stored through "Zenith.Physics.Buoyancy" (version 1, Stage 4) on a dynamic body.
/// Thread affinity: CommandsOnly.
/// Invalidation: None.
/// Units: kg/m³; 1/s.
/// Errors: Without a Zenith.FluidQuery resource in the world the component has no effect
///   (physics.buoyancy.no_fluid, once per world).
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_BuoyancyComponent
{
	float m_fFluidDensity = 1000.0f;                                 // 1 FluidDensity (0: the fluid sample's density)
	float m_fLinearDrag = 0.5f;                                      // 2 LinearDrag
	float m_fAngularDrag = 0.5f;                                     // 3 AngularDrag
	float m_fVolumeScale = 1.0f;                                     // 4 VolumeScale (tuning: displaced volume factor)
	bool m_bEnabled = true;                                          // 5 Enabled
	static const Zenith_TypeID xTYPE_ID;                             // "Zenith.Physics.Buoyancy"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract functions Zenith_PhysicsComponentRegistration
/// Ownership: Registers the components, struct and event types of PHYS into the registry it is given.
/// Thread affinity: MAIN_THREAD, startup step 6 (from Zenith_Module_Physics_RegisterTypes) and in tests.
/// Invalidation: None.
/// Units: None.
/// Errors: Registration errors are reported by Freeze ([Z2-N-ARCH-847]); the function returns
///   PHYSICS_ERROR_INVALID_DESC when the registry is frozen.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_PhysicsError> Zenith_RegisterPhysicsComponents(Zenith_TypeRegistry& xTypes);
```

### Rigid body properties

| ID | Name | Type | Default | Authority | Serialized | Range |
|---|---|---|---|---|---|---|
| 1 | MotionType | u8 | 3 (dynamic) | CommandsOnly | yes | 1 … 3 |
| 2 | Mass | f32 | 0 | CommandsOnly | yes | 0 … 1e6 |
| 3 | LinearDamping | f32 | 0.05 | CommandsOnly | yes | 0 … 100 |
| 4 | AngularDamping | f32 | 0.05 | CommandsOnly | yes | 0 … 100 |
| 5 | GravityFactor | f32 | 1 | CommandsOnly | yes | −10 … 10 |
| 6 | MotionQuality | u8 | 1 | CommandsOnly | yes | 1 … 2 |
| 7 | Layer | u8 | 0 | CommandsOnly | yes | 0 … 31 |
| 8 | AllowSleeping | bool | true | CommandsOnly | yes | — |
| 9 | Flags | u32 | REPORT_INTERACTIONS | CommandsOnly | yes | — |
| 10 | CollisionGroup | u32 | 0 | CommandsOnly | yes | — |
| 11 | InitialLinearVelocity | vec3 | 0 | CommandsOnly | yes | — |
| 12 | InitialAngularVelocity | vec3 | 0 | CommandsOnly | yes | — |
| 13 | MaxLinearSpeed | f32 | 0 | CommandsOnly | yes | 0 … 10,000 |
| 14 | CentreOfMassOffset | vec3 | 0 | CommandsOnly | yes | — |
| 20 | LinearVelocity | vec3 | 0 | Physics | yes | — |
| 21 | AngularVelocity | vec3 | 0 | Physics | yes | — |
| 22 | Sleeping | bool | false | Physics | yes | — |
| 23 | Frozen | bool | false | Physics | no (recomputed from Zenith.Streaming.RegionBound) | — |

A `SetProperty` command on property 20 or 21 recorded in phases 1–4 is applied at its sync point and read by `Physics.Step`'s ingest as a velocity write (PHYS/Simulation.md "External writes"); the step then overwrites the property with the simulated value ([Z2-N-ARCH-489]).

### Collider properties

| ID | Name | Type | Default | Authority | Serialized |
|---|---|---|---|---|---|
| 1 | ShapeKind | u8 | 2 (box) | CommandsOnly | yes |
| 2 | Size | vec3 | (0.5, 0.5, 0.5) | CommandsOnly | yes |
| 3 | Shape | asset `Physics.CollisionShape` | null | CommandsOnly | yes (persistent ID) |
| 4 | Material | asset `Physics.Material` | null | CommandsOnly | yes |
| 5 | LocalPosition | vec3 | 0 | CommandsOnly | yes |
| 6 | LocalRotation | quat | identity | CommandsOnly | yes |
| 7 | IsTrigger | bool | false | CommandsOnly | yes |
| 8 | StaticLayer | u8 | 0 | CommandsOnly | yes |

### Joint properties

| ID | Name | Type | Authority | Serialized |
|---|---|---|---|---|
| 1 | Kind | u8 | CommandsOnly | yes |
| 2, 3 | BodyA, BodyB | entity | CommandsOnly | yes (entity references) |
| 4 … 7 | FrameAPosition, FrameARotation, FrameBPosition, FrameBRotation | vec3, quat | CommandsOnly | yes |
| 8 … 11 | LinearLimitMin, LinearLimitMax, AngularLimitMin, AngularLimitMax | vec3 | CommandsOnly | yes |
| 12 | MotorMode | u8 | EarlyScripts | yes |
| 13, 14 | MotorTargetVelocity, MotorTargetPosition | vec3 | EarlyScripts | yes |
| 15, 16 | MotorMaxForce, MotorMaxTorque | f32 | CommandsOnly | yes |
| 17, 18 | BreakForce, BreakTorque | f32 | CommandsOnly | yes |
| 19 | CollideConnected | bool | CommandsOnly | yes (false, the default: the two bodies never collide with each other, on both backends, PHYS/Backends.md "Constraint mapping") |
| 20 | Enabled | bool | EarlyScripts | yes |
| 21 | Broken | bool | Physics | yes |

### Buoyancy properties

| ID | Name | Type | Default | Authority | Serialized |
|---|---|---|---|---|---|
| 1 | FluidDensity | f32 | 1000 | CommandsOnly | yes |
| 2 | LinearDrag | f32 | 0.5 | CommandsOnly | yes |
| 3 | AngularDrag | f32 | 0.5 | CommandsOnly | yes |
| 4 | VolumeScale | f32 | 1 | CommandsOnly | yes |
| 5 | Enabled | bool | true | EarlyScripts | yes |

With BodyB null, frames B (properties 6 and 7) are expressed in the space of the joint component's own entity: the constraint's world anchor is `W_joint · T(FrameBPosition, FrameBRotation)` with `W_joint` that entity's `Zenith.WorldTransform` when the constraint is created (PHYS/Simulation.md "Ingest algorithm" step 5). A world-anchored joint therefore moves with its entity under origin shifts and streaming root translations; to anchor at a fixed world point, place the joint component on a static entity there with identity frame B.

## Zenith_PhysicsEvents header

Events are `TYPE_KIND_EVENT` types emitted by `Physics.Step` in phase 5 and delivered in phase 9 of the same tick ([Z2-N-ARCH-478]); within a tick their order is merge order: ascending event key ID (the low simulation ID of a pair, the joint's simulation ID for `JointBroken`), then, within one key ID, began/entered, ended/exited, interaction, joint broken, each in ascending high simulation ID (PHYS/Simulation.md "Event emission"). Entity properties are world handles; a handle of an entity destroyed at the end of the previous tick resolves to `ECS_ERROR_STALE_HANDLE`, which consumers treat as "gone".

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsEvents.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"

/// @contract value Zenith_PhysicsContactBeganEvent
/// Ownership: Event payload of "Zenith.Physics.ContactBegan" (version 1), emitted when two bodies, at least one with
///   PHYSICS_BODY_FLAG_REPORT_CONTACTS, begin touching during the tick.
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: Delivered in phase 9; valid for the tick.
/// Units: World metres; unit normal from A to B; m/s.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsContactBeganEvent
{
	Zenith_EntityHandle m_xEntityA;          // 1 EntityA: the lower stable simulation ID of the pair
	Zenith_EntityHandle m_xEntityB;          // 2 EntityB
	Zenith_Vector3 m_xPoint;                 // 3 Point
	Zenith_Vector3 m_xNormal;                // 4 Normal
	float m_fApproachSpeed = 0.0f;           // 5 ApproachSpeed
	Zenith_PersistentID m_xMaterialA;        // 6 MaterialA (the material asset's ID; null: default)
	Zenith_PersistentID m_xMaterialB;        // 7 MaterialB
	static const Zenith_TypeID xTYPE_ID;     // "Zenith.Physics.ContactBegan"
};

/// @contract value Zenith_PhysicsContactEndedEvent
/// Ownership: Event payload of "Zenith.Physics.ContactEnded" (version 1): the pair stopped touching, or one body left the
///   simulation (destroyed, frozen, its collider removed).
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: As ContactBegan.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsContactEndedEvent
{
	Zenith_EntityHandle m_xEntityA;          // 1 EntityA
	Zenith_EntityHandle m_xEntityB;          // 2 EntityB
	static const Zenith_TypeID xTYPE_ID;     // "Zenith.Physics.ContactEnded"
};

/// @contract value Zenith_PhysicsTriggerEnteredEvent
/// Ownership: Event payload of "Zenith.Physics.TriggerEntered" (version 1): Other's body began overlapping the trigger
///   body of Trigger (the entity carrying the IsTrigger collider; for a split trigger child, the child collider's entity,
///   PHYS/Simulation.md "Trigger semantics").
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: As ContactBegan.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. The names GAME's interaction sample subscribes to (cross-area row GA3).
/// @endcontract
struct Zenith_PhysicsTriggerEnteredEvent
{
	Zenith_EntityHandle m_xTrigger;          // 1 Trigger
	Zenith_EntityHandle m_xOther;            // 2 Other
	static const Zenith_TypeID xTYPE_ID;     // "Zenith.Physics.TriggerEntered"
};

/// @contract value Zenith_PhysicsTriggerExitedEvent
/// Ownership: Event payload of "Zenith.Physics.TriggerExited" (version 1).
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: As ContactBegan.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsTriggerExitedEvent
{
	Zenith_EntityHandle m_xTrigger;          // 1 Trigger
	Zenith_EntityHandle m_xOther;            // 2 Other
	static const Zenith_TypeID xTYPE_ID;     // "Zenith.Physics.TriggerExited"
};

/// @contract value Zenith_PhysicsInteractionEvent
/// Ownership: Event payload of "Zenith.Physics.Interaction" (version 1): an impact or a slide raised by the pair's
///   material interaction rule (Zenith_FindPhysicsInteraction), for pairs with PHYSICS_BODY_FLAG_REPORT_INTERACTIONS.
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: As ContactBegan.
/// Units: World metres; unit normal; m/s.
/// Errors: None.
/// Version: since Z2 Stage 3. AUDIO and VFX map m_xInteraction to their effects.
/// @endcontract
struct Zenith_PhysicsInteractionEvent
{
	Zenith_EntityHandle m_xEntityA;          // 1 EntityA
	Zenith_EntityHandle m_xEntityB;          // 2 EntityB
	Zenith_StringID m_xInteraction;          // 3 Interaction
	uint8 m_eKind = 1;                      // 4 Kind (Zenith_PhysicsInteractionKind)
	Zenith_Vector3 m_xPoint;                 // 5 Point
	Zenith_Vector3 m_xNormal;                // 6 Normal
	float m_fSpeed = 0.0f;                   // 7 Speed: approach speed (impact) or tangential speed (slide)
	static const Zenith_TypeID xTYPE_ID;     // "Zenith.Physics.Interaction"
};

/// @contract value Zenith_PhysicsJointBrokenEvent
/// Ownership: Event payload of "Zenith.Physics.JointBroken" (version 1).
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: As ContactBegan.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsJointBrokenEvent
{
	Zenith_EntityHandle m_xJoint;            // 1 Joint: the entity carrying Zenith.Physics.Joint
	Zenith_EntityHandle m_xBodyA;            // 2 BodyA
	Zenith_EntityHandle m_xBodyB;            // 3 BodyB
	static const Zenith_TypeID xTYPE_ID;     // "Zenith.Physics.JointBroken"
};
```

## Zenith_PhysicsWorldResource header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsWorldResource.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"
#include "ZenithPhysics/Zenith_PhysicsQuery.h"

class Zenith_RagdollPoseSource;
struct Zenith_RagdollBodyPose;

/// @contract value Zenith_PhysicsCommandKinds
/// Ownership: Plain value: the command-kind IDs the module resolved at its first AttachWorld through
///   Zenith_WorldAttachContext::FindCommandKind (PHYS.md "Assumed interfaces").
/// Thread affinity: IMMUTABLE after the first AttachWorld. Members non-const: default-constructed, then filled from the
///   command registry at AttachWorld.
/// Invalidation: None while the module is started.
/// Units: None.
/// Errors: None; a zero ID means the module is not started (recording helpers then record nothing and count
///   physics.command.unresolved_kind_delta).
/// Version: since Z2 Stage 3. One entry per row of PHYS.md "Command kinds", in that order.
/// @endcontract
struct Zenith_PhysicsCommandKinds
{
	uint32 m_uAddImpulse = 0;
	uint32 m_uAddForce = 0;
	uint32 m_uSetVelocity = 0;
	uint32 m_uSetSleeping = 0;
	uint32 m_uEnterRagdoll = 0;
	uint32 m_uRecoverRagdoll = 0;
	uint32 m_uRemoveRagdollBodies = 0;
	uint32 m_uSetGravity = 0;
};

/// @contract value Zenith_PhysicsEntityHit
/// Ownership: Plain value filled by the resource's queries.
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: m_xEntity follows Zenith_EntityHandle's rules.
/// Units: World metres; unit normal.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsEntityHit
{
	Zenith_EntityHandle m_xEntity;           // the body's entity; null for an entity-less body (none exist in v1)
	uint64 m_ulSimulationID = 0;
	Zenith_Vector3 m_xPosition;
	Zenith_Vector3 m_xNormal;
	float m_fDistance = 0.0f;
	float m_fFraction = 0.0f;
	uint32 m_uSubShape = 0;
	Zenith_PersistentID m_xMaterial;         // the hit sub-shape's material asset ID (null: default)
};

/// @contract type Zenith_PhysicsWorldResource
/// Ownership: One per world, owned by the Physics module's world state; registered by AttachWorld as the world resource
///   "Zenith.Physics.World" with owning phase PHASE_PHYSICS; borrowed by readers through
///   Zenith_SystemContext::GetResource. It wraps the world's Zenith_PhysicsWorld and the module's body table.
/// Thread affinity: The const query members are ANY_THREAD for every node that declares ReadResource of
///   "Zenith.Physics.World" (phases 1 to 4, 6 to 12 and the epilogue); they read the state after the latest
///   Physics.Step. GetRagdollBodyPoses is callable from phase 6 to phase 12. SetRagdollPoseSource is MAIN_THREAD during
///   an AttachWorld of another module (ANIM's). Writers: Physics.Step alone.
/// Invalidation: Spans and handles returned follow their types; the resource itself lives as long as the world.
/// Units: World metres; unit quaternions; radians.
/// Errors: Queries return false or 0 for an invalid descriptor (physics.query.invalid_delta); never assert on inputs.
/// Reentrancy: NOT_APPLICABLE. The resource invokes no callback; the pose source is called by the module's own nodes,
///   never by the resource's query members.
/// Version: since Z2 Stage 3. Results are identical for identical physics state and arguments on every run; equal
///   distances resolve to the lower stable simulation ID.
/// @endcontract
class Zenith_PhysicsWorldResource
{
public:
	bool RayCast(const Zenith_Vector3& xOrigin, const Zenith_Vector3& xUnitDirection, float fMaxDistance,
		Zenith_PhysicsLayerMask uLayers, Zenith_PhysicsEntityHit& xHitOut, Zenith_EntityHandle xIgnoreRoot = {}) const;
	// Every hit up to axHitsOut.GetCount() (≤ uPHYSICS_MAX_QUERY_HITS), ascending distance; returns the count stored.
	uint32 RayCastAll(const Zenith_Vector3& xOrigin, const Zenith_Vector3& xUnitDirection, float fMaxDistance,
		Zenith_PhysicsLayerMask uLayers, Zenith_Span<Zenith_PhysicsEntityHit> axHitsOut) const;
	bool SphereCast(const Zenith_Vector3& xOrigin, float fRadius, const Zenith_Vector3& xUnitDirection,
		float fMaxDistance, Zenith_PhysicsLayerMask uLayers, Zenith_PhysicsEntityHit& xHitOut,
		Zenith_EntityHandle xIgnoreRoot = {}) const;
	bool CapsuleCast(const Zenith_Vector3& xOrigin, const Zenith_Quaternion& xRotation, float fRadius,
		float fHalfHeight, const Zenith_Vector3& xUnitDirection, float fMaxDistance, Zenith_PhysicsLayerMask uLayers,
		Zenith_PhysicsEntityHit& xHitOut, Zenith_EntityHandle xIgnoreRoot = {}) const;
	bool BoxCast(const Zenith_Vector3& xCentre, const Zenith_Quaternion& xRotation, const Zenith_Vector3& xHalfExtents,
		const Zenith_Vector3& xUnitDirection, float fMaxDistance, Zenith_PhysicsLayerMask uLayers,
		Zenith_PhysicsEntityHit& xHitOut, Zenith_EntityHandle xIgnoreRoot = {}) const;
	// The entities whose bodies overlap the sphere or box, ascending simulation ID, at most the span's count.
	uint32 OverlapSphere(const Zenith_Vector3& xCentre, float fRadius, Zenith_PhysicsLayerMask uLayers,
		bool bIncludeTriggers, Zenith_Span<Zenith_EntityHandle> axEntitiesOut) const;
	uint32 OverlapBox(const Zenith_Vector3& xCentre, const Zenith_Quaternion& xRotation,
		const Zenith_Vector3& xHalfExtents, Zenith_PhysicsLayerMask uLayers, bool bIncludeTriggers,
		Zenith_Span<Zenith_EntityHandle> axEntitiesOut) const;

	// Body state of an entity's body after the latest step; false when the entity has no body.
	bool GetBodyState(Zenith_EntityHandle xEntity, Zenith_PhysicsBodyState& xStateOut) const;
	bool GetPointVelocity(Zenith_EntityHandle xEntity, const Zenith_Vector3& xWorldPoint,
		Zenith_Vector3& xVelocityOut) const;

	// Ragdolls (cross-area row AN6).
	void SetRagdollPoseSource(Zenith_RagdollPoseSource* pxSource);   // null uninstalls
	uint32 GetRagdollBodyPoses(uint64 ulSimulationID, Zenith_Span<Zenith_RagdollBodyPose> axPosesOut) const;

	// Layers and kinds.
	uint32 FindLayer(Zenith_StringView strName) const;          // uPHYSICS_NO_INDEX when unknown
	Zenith_PhysicsLayerMask GetLayerCollisionMask(uint32 uLayer) const;
	const Zenith_PhysicsCommandKinds& GetCommandKinds() const;

	static const Zenith_TypeID xTYPE_ID;     // Zenith_MakeTypeID("Zenith.Physics.World")
};
```

Each query builds a `Zenith_PhysicsQueryFilter` from its arguments (`m_bIncludeTriggers` false unless the overlap asks; `m_aulIgnoreUserData` the simulation IDs of `xIgnoreRoot` and its descendants, sorted, collected through `Zenith_EntityTable::GetChildren` to the hierarchy's depth limit), runs the `Zenith_PhysicsWorld` query into a stack collector of the capacity it needs and maps each body's user data to its entity through the module's body table (PHYS/Simulation.md "Query execution").

## Zenith_PhysicsCommands header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsCommands.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithPhysics/Zenith_PhysicsWorldResource.h"

/// @contract value Zenith_PhysicsBodyCommandPayload
/// Ownership: Plain value copied into a command record; a registered TYPE_KIND_STRUCT "Zenith.Physics.BodyCommand" (56 bytes, the static_assert below).
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: None.
/// Units: N·s (impulse), N (force), m/s and rad/s (velocity), world metres (point).
/// Errors: Validated at playback: a non-finite vector or an unknown flag drops the command (physics.command.invalid_delta).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsBodyCommandPayload
{
	Zenith_EntityHandle m_xEntity;           // 8
	Zenith_Vector3 m_xLinear;                // impulse, force or linear velocity
	Zenith_Vector3 m_xAngular;               // angular impulse, torque or angular velocity
	Zenith_Vector3 m_xPoint;                 // world application point (flag bit 0)
	uint32 m_uFlags = 0;                    // bit 0 at point; bit 1 linear present; bit 2 angular present; bit 3 wake
	uint32 m_uReserved = 0;
};
static_assert(sizeof(Zenith_PhysicsBodyCommandPayload) == 56);

/// @contract functions Zenith_PhysicsCommandRecording
/// Ownership: Record into the caller's command buffer; own nothing.
/// Thread affinity: OWNER_THREAD — the buffer's (any system or behaviour of phases 1 to 10).
/// Invalidation: None.
/// Units: As Zenith_PhysicsBodyCommandPayload.
/// Errors: Record nothing and count physics.command.unresolved_kind_delta when xKinds is unresolved; stale entities are
///   dropped at playback with ecs.commands_stale_delta semantics (physics.command.stale_delta), never asserted.
/// Version: since Z2 Stage 3. The commands apply at the sync point after the recording phase into the world's pending
///   operations, which Physics.Step applies in merge order at its next ingest (PHYS/Simulation.md "Pending operations").
/// @endcontract
void Zenith_RecordPhysicsAddImpulse(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_Vector3& xImpulse, const Zenith_Vector3* pxWorldPoint);
void Zenith_RecordPhysicsAddAngularImpulse(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_Vector3& xAngularImpulse);
void Zenith_RecordPhysicsAddForce(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_Vector3& xForce, const Zenith_Vector3* pxWorldPoint);   // over the next step
void Zenith_RecordPhysicsAddTorque(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_Vector3& xTorque);
void Zenith_RecordPhysicsSetVelocity(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_Vector3* pxLinear, const Zenith_Vector3* pxAngular);
void Zenith_RecordPhysicsSetSleeping(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, bool bSleeping);
void Zenith_RecordPhysicsSetGravity(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	const Zenith_Vector3& xGravity);                                                                  // the world's
```

## Zenith_Ragdoll header

The first four declarations are the ones `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Ragdoll enter and recover" fixes, with two amendments carried by cross-area rows: the get-up clip is a `Zenith_PersistentID` (row AN2, because ZenithPhysics cannot name ANIM's `Zenith_AnimationClip`), and the recording functions take the resolved `Zenith_PhysicsCommandKinds` (row PH6, because a module learns its kind IDs only at attach).

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_Ragdoll.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithPhysics/Zenith_PhysicsWorldResource.h"

/// @contract functions Zenith_RagdollConstants
/// Ownership: constexpr data.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
inline constexpr uint32 uPHYSICS_NO_IMPULSE_BONE = 0xFFFFFFFFu;   // Physics-local sentinel: apply no impulse

/// @contract value Zenith_RagdollEnterDesc
/// Ownership: Plain value copied into the Physics.EnterRagdoll command.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Seconds of simulated time; N·s; a skeleton joint index.
/// Errors: A blend time outside [0, 10] s or a non-finite impulse drops the command at playback
///   (physics.ragdoll.invalid_delta).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_RagdollEnterDesc
{
	float m_fBlendSeconds = 0.1f;                        // animation → physics ownership blend
	Zenith_Vector3 m_xImpulse;                           // applied to m_uImpulseBone at the first physics step
	uint32 m_uImpulseBone = uPHYSICS_NO_IMPULSE_BONE;   // a skeleton joint index
};

/// @contract value Zenith_RagdollRecoverDesc
/// Ownership: Plain value copied into the Physics.RecoverRagdoll command; dispatched as the payload of
///   LIFECYCLE_EVENT_RAGDOLL_RECOVER.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Seconds of simulated time.
/// Errors: As Zenith_RagdollEnterDesc.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_RagdollRecoverDesc
{
	float m_fBlendSeconds = 0.4f;                        // physics → animation ownership blend
	Zenith_PersistentID m_xGetUpClip;                    // an Animation.Clip asset's ID; null: the graph's own states
};

/// @contract functions Zenith_RagdollRecording
/// Ownership: Record into the caller's command buffer.
/// Thread affinity: OWNER_THREAD — the buffer's; phases 1 to 10.
/// Invalidation: None.
/// Units: As the descs.
/// Errors: As Zenith_PhysicsCommandRecording.
/// Version: since Z2 Stage 3. The publication points of [Z2-N-ARCH-552]: issued in phases 1–4 the bodies are simulated
///   by phase 5 of the same tick, issued in phases 5–10 by phase 5 of the next tick.
/// @endcontract
void Zenith_RecordEnterRagdoll(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_RagdollEnterDesc& xDesc);
void Zenith_RecordRecoverFromRagdoll(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_RagdollRecoverDesc& xDesc);

/// @contract enum Zenith_RagdollState
/// Ownership: Plain enumeration; property 1 of Zenith.Physics.Ragdoll, stored as u8.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. The values ANIM reads (cross-area row AN6).
/// @endcontract
enum Zenith_RagdollState : uint8
{
	RAGDOLL_STATE_NONE = 0,
	RAGDOLL_STATE_ENTERING = 1,
	RAGDOLL_STATE_ACTIVE = 2,
	RAGDOLL_STATE_RECOVERING = 3,
};

/// @contract value Zenith_RagdollComponent
/// Ownership: Component value stored through "Zenith.Physics.Ragdoll" (version 1) on a character entity whose skeleton
///   the definition names joints of.
/// Thread affinity: State and BlendWeight have authority PHASE_POST_PHYSICS, written by Physics.RagdollBlend alone (and
///   by PHYS's command playback at sync points); Definition, DriveStrength and Layer are CommandsOnly.
/// Invalidation: None.
/// Units: BlendWeight a fraction: 0 animation owns the bones, 1 the ragdoll does.
/// Errors: An enter on an entity without a resident definition or pose source is refused (physics.ragdoll.enter_refused).
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_RagdollComponent
{
	uint8 m_eState = RAGDOLL_STATE_NONE;                // 1 State
	uint8 m_uLayer = 0;                                 // 5 Layer
	uint8 m_auReserved[2] = {};
	float m_fBlendWeight = 0.0f;                         // 2 BlendWeight
	Zenith_AssetHandle<class Zenith_RagdollAsset> m_xDefinition;   // 3 Definition (Physics.Ragdoll)
	float m_fDriveStrength = 0.0f;                       // 4 DriveStrength, [0, 1]: 0 limp, 1 full motor strength
	static const Zenith_TypeID xTYPE_ID;                 // "Zenith.Physics.Ragdoll"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract value Zenith_RagdollBodyPose
/// Ownership: Plain value filled by Zenith_PhysicsWorldResource::GetRagdollBodyPoses.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: None.
/// Units: World metres; unit quaternion.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_RagdollBodyPose
{
	uint64 m_ulJointNameHash = 0;                       // the bound joint's Zenith_Hash64 name hash
	Zenith_Vector3 m_xPosition;                          // the joint's world position (the part's frame, not its centre of mass)
	Zenith_Quaternion m_xRotation;
};

/// @contract interface Zenith_RagdollPoseSource
/// Ownership: Declared by PHYS, implemented by the Animation module (one per world), installed on the world's
///   Zenith_PhysicsWorldResource with SetRagdollPoseSource at ANIM's AttachWorld and uninstalled at its DetachWorld; the
///   implementer owns it.
/// Thread affinity: ANY_THREAD for the const members: PHYS calls GetEntryModelPose on the world's sync-point node (ragdoll
///   enter playback, where ANIM's implementation reads Zenith.Animation.Residency and Zenith.Animation.FinalPoses) and
///   GetAnimatedModelPose from Physics.Step's ingest in phase 5 (after phase 3 wrote the pose; Physics.Step declares
///   ReadResource of Zenith.Animation.World and Zenith.Animation.Residency, PHYS/Simulation.md "Frame-graph placement").
/// Invalidation: None while installed.
/// Units: Model space of the entity, metres; matrices are rigid transforms (Zenith2/Docs/Design/CORE/Maths.md conventions).
/// Errors: Each member returns false when the entity has no animator instance or the span is too small; PHYS then refuses
///   the enter or keeps the previous drive targets (counted).
/// Reentrancy: NOT_APPLICABLE. It invokes no callback.
/// Version: since Z2 Stage 3. The member set ANIM's design assumes (cross-area row AN6).
/// @endcontract
class Zenith_RagdollPoseSource
{
public:
	virtual ~Zenith_RagdollPoseSource() = default;
	virtual uint32 GetJointCount(uint64 ulSimulationID) const = 0;   // 0: no animator
	virtual uint32 FindJoint(uint64 ulSimulationID, uint64 ulNameHash) const = 0;   // uPHYSICS_NO_INDEX when absent
	// ANIM's final model pose of the latest phase 6 (one matrix per joint, joint order), unchanged between that phase 6 and
	// any sync point before the next one, so ANIM takes no snapshot at LIFECYCLE_EVENT_RAGDOLL_ENTER (row AN6 (f)).
	virtual bool GetEntryModelPose(uint64 ulSimulationID, Zenith_Span<Zenith_Matrix4> axModelOut) const = 0;
	// This tick's animated model pose (phase 3 output), the drive target of a powered ragdoll.
	virtual bool GetAnimatedModelPose(uint64 ulSimulationID, Zenith_Span<Zenith_Matrix4> axModelOut) const = 0;
};

/// @contract type Zenith_RagdollAsset
/// Ownership: The runtime object of asset kind Physics.Ragdoll (PHYS.md "Asset kinds"); created by the Physics module's
///   kind loader; owned by the asset registry; it holds one backend shape reference per part.
/// Thread affinity: ANY_THREAD for the const members after activation.
/// Invalidation: Invalid after retirement; live ragdoll bodies keep their shapes through their own references.
/// Units: Metres, kilograms, radians.
/// Errors: None after activation.
/// Version: since Z2 Stage 3. Format version 1 (PHYS/Formats.md "Ragdoll file").
/// @endcontract
class Zenith_RagdollAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_PHYSICS_RAGDOLL;
	uint32 GetPartCount() const;
	uint64 GetPartJointNameHash(uint32 uPart) const;
	uint32 GetPartParent(uint32 uPart) const;               // uPHYSICS_NO_INDEX for the root part
	Zenith_PhysicsShapeHandle GetPartShape(uint32 uPart) const;
	uint64 GetSkeletonHierarchyHash() const;                 // ANIM's skeleton hierarchy hash it was authored for
};
```

## Zenith_CharacterController header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_CharacterController.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"

/// @contract enum Zenith_CharacterControllerMode
/// Ownership: Plain enumeration; property 11 of Zenith.Physics.CharacterController.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value behaves as VELOCITY and counts physics.controller.bad_mode_delta.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_CharacterControllerMode : uint8
{
	CHARACTER_CONTROLLER_MODE_VELOCITY = 1,                 // horizontal motion from DesiredVelocity; gravity integrated
	CHARACTER_CONTROLLER_MODE_ROOT_MOTION_WITH_GRAVITY = 2, // horizontal motion and yaw from root motion; gravity integrated
	CHARACTER_CONTROLLER_MODE_ROOT_MOTION = 3,              // every component of root motion; no gravity while grounded
};

/// @contract enum Zenith_CharacterControllerFlags
/// Ownership: Plain bit flags; property 39 StateFlags (output) and 23 InputFlags (input).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_CharacterControllerFlags : uint32
{
	CHARACTER_FLAG_NONE = 0,
	CHARACTER_FLAG_HIT_CEILING = 1u << 0,         // state
	CHARACTER_FLAG_ON_STEEP_SLOPE = 1u << 1,      // state
	CHARACTER_FLAG_STEPPED_UP = 1u << 2,          // state
	CHARACTER_FLAG_ADOPTED_EXTERNAL = 1u << 3,    // state: the controller took a pose another writer produced
	CHARACTER_FLAG_DEPENETRATED = 1u << 4,        // state
	CHARACTER_FLAG_JUMPED = 1u << 5,              // state
	CHARACTER_FLAG_USE_DESIRED_YAW = 1u << 16,    // input: rotation yaw from DesiredYawRadians
	CHARACTER_FLAG_SUSPENDED = 1u << 17,          // input: no move this tick (cinematics, cut-scenes driven by scripts)
};

/// @contract value Zenith_CharacterControllerComponent
/// Ownership: Component value stored through "Zenith.Physics.CharacterController" (version 1). The entity's origin is
///   the bottom of the capsule (the feet); the capsule's axis is world +Y. The module creates one kinematic
///   QUERY-visible capsule body per controller so dynamic bodies collide with the character.
/// Thread affinity: Configuration CommandsOnly; input EarlyScripts (written by behaviours and chunk systems in phase 2);
///   state MovementArbitration, written by Physics.CharacterMove alone (and shifted by SCENE's origin-shift playback:
///   35 TargetPosition carries PROPERTY_FLAG_WORLD_POSITION). TargetPosition and TargetRotation are a world pose relative
///   to the world origin, for a root or a parented controller alike; the transform written for a parented controller is
///   its local equivalent (PHYS/CharacterRagdollVehicle.md "Character move algorithm" step 1).
/// Invalidation: None.
/// Units: Metres, radians, m/s, kilograms, ticks.
/// Errors: Invalid dimensions (radius ≤ 0, height < 2 × radius, step ≥ height) give the entity no controller
///   (physics.controller.invalid_delta).
/// Version: since Z2 Stage 3; type version 1. The algorithm is PHYS/CharacterRagdollVehicle.md "Character move algorithm".
/// @endcontract
struct Zenith_CharacterControllerComponent
{
	float m_fRadius = 0.35f;                          // 1 Radius
	float m_fHeight = 1.8f;                           // 2 Height (total, including both caps)
	float m_fStepHeight = 0.35f;                      // 3 StepHeight
	float m_fMaxSlopeRadians = 0.7853982f;            // 4 MaxSlopeRadians (45°)
	float m_fSkinWidth = 0.02f;                       // 5 SkinWidth
	float m_fGroundSnapDistance = 0.2f;               // 6 GroundSnapDistance
	float m_fMass = 80.0f;                            // 7 Mass (push strength against dynamics)
	float m_fPushStrength = 1.0f;                     // 8 PushStrength
	uint8 m_uLayer = 0;                              // 9 Layer
	uint8 m_eMode = CHARACTER_CONTROLLER_MODE_VELOCITY;   // 11 Mode
	uint8 m_auReserved[2] = {};
	Zenith_PhysicsLayerMask m_uCollisionMask = uPHYSICS_LAYER_MASK_ALL;   // 10 CollisionMask
	float m_fGravityScale = 1.0f;                     // 12 GravityScale
	float m_fMaxFallSpeed = 55.0f;                    // 13 MaxFallSpeed
	float m_fJumpSpeed = 5.0f;                        // 14 JumpSpeed
	uint32 m_uCoyoteTicks = 6;                       // 15 CoyoteTicks
	Zenith_Vector3 m_xDesiredVelocity;                // 20 DesiredVelocity (world space; the y component is ignored)
	uint64 m_ulJumpRequestTick = 0;                  // 21 JumpRequestTick: a jump requested at this tick index
	float m_fDesiredYawRadians = 0.0f;                // 22 DesiredYawRadians
	uint32 m_uInputFlags = 0;                        // 23 InputFlags (Zenith_CharacterControllerFlags input bits)
	Zenith_Vector3 m_xVelocity;                       // 30 Velocity
	bool m_bGrounded = false;                         // 31 Grounded
	uint8 m_auReserved2[3] = {};
	Zenith_Vector3 m_xGroundNormal{ 0.0f, 1.0f, 0.0f };   // 32 GroundNormal
	Zenith_EntityHandle m_xGroundEntity;              // 33 GroundEntity
	Zenith_Vector3 m_xGroundVelocity;                 // 34 GroundVelocity
	Zenith_Vector3 m_xTargetPosition;                 // 35 TargetPosition (PROPERTY_FLAG_WORLD_POSITION)
	Zenith_Quaternion m_xTargetRotation;              // 36 TargetRotation
	uint64 m_ulLastJumpTick = 0;                     // 37 LastJumpTick
	uint32 m_uAirTicks = 0;                          // 38 AirTicks
	uint32 m_uStateFlags = 0;                        // 39 StateFlags
	static const Zenith_TypeID xTYPE_ID;              // "Zenith.Physics.CharacterController"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};
```

| IDs | Group | Authority | Serialized |
|---|---|---|---|
| 1 … 15 | configuration | CommandsOnly | yes |
| 20 … 23 | input | EarlyScripts | yes |
| 30 … 39 | state | MovementArbitration | yes (30 … 38), no (39) |

Property 35 is registered with `PROPERTY_FLAG_WORLD_POSITION` (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "ECS world-position flag", pending ECS through STREAM's row ST4 and core-doc item X227 (a)), so `Scene.ShiftWorldOrigin` shifts it with `(float)((double)p − S)` like every transform; PHYS's origin-shift listener applies the same formula to the controller record's written local position (PHYS/Simulation.md "Origin shift").

## Zenith_PhysicsVehicle header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsVehicle.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"

/// @contract value Zenith_VehicleWheel
/// Ownership: Reflected struct "Zenith.Physics.VehicleWheel" (version 1), an element of Zenith.Physics.Vehicle's Wheels array.
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: None.
/// Units: Metres (chassis-local), N/m, N·s/m, kg·m², N·m, radians.
/// Errors: Validated at vehicle creation (physics.vehicle.invalid_delta).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_VehicleWheel
{
	Zenith_Vector3 m_xAttachment;            // 1 Attachment: the suspension's top in chassis space
	float m_fRadius = 0.35f;                 // 2 Radius
	float m_fWidth = 0.25f;                  // 3 Width
	float m_fSuspensionRest = 0.3f;          // 4 SuspensionRest: rest length
	float m_fSuspensionTravel = 0.25f;       // 5 SuspensionTravel: max compression from rest
	float m_fSpringRate = 35000.0f;          // 6 SpringRate
	float m_fDamperRate = 3500.0f;           // 7 DamperRate
	float m_fInertia = 1.2f;                 // 8 Inertia
	float m_fMaxSteerRadians = 0.0f;         // 9 MaxSteerRadians (0: not steered)
	float m_fBrakeTorque = 2500.0f;          // 10 BrakeTorque
	float m_fHandbrakeTorque = 0.0f;         // 11 HandbrakeTorque
	float m_fGrip = 1.0f;                    // 12 Grip: tyre friction multiplier
	bool m_bDriven = false;                  // 13 Driven
	uint8 m_auReserved[3] = {};
};

/// @contract value Zenith_VehicleWheelState
/// Ownership: Reflected struct "Zenith.Physics.VehicleWheelState" (version 1), an element of the WheelStates array.
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: None.
/// Units: Metres, rad/s, newtons.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_VehicleWheelState
{
	float m_fCompression = 0.0f;             // 1 Compression
	float m_fAngularVelocity = 0.0f;         // 2 AngularVelocity
	float m_fRotation = 0.0f;                // 3 Rotation (accumulated, wrapped to (−π, π])
	float m_fSteer = 0.0f;                   // 4 Steer
	float m_fLongitudinalSlip = 0.0f;        // 5 LongitudinalSlip
	float m_fLateralSlip = 0.0f;             // 6 LateralSlip
	Zenith_EntityHandle m_xContactEntity;    // 7 ContactEntity
	bool m_bContact = false;                 // 8 Contact
	uint8 m_auReserved[7] = {};
};

/// @contract value Zenith_VehicleComponent
/// Ownership: Component value stored through "Zenith.Physics.Vehicle" (version 1, Stage 4) on an entity with a dynamic
///   rigid body (the chassis).
/// Thread affinity: Configuration CommandsOnly; input EarlyScripts; state Physics.
/// Invalidation: None.
/// Units: As Zenith_VehicleWheel; RPM; N·m; dimensionless ratios.
/// Errors: A vehicle without a dynamic body or with 0 or more than 8 wheels is inactive (physics.vehicle.invalid_delta).
/// Version: since Z2 Stage 4; type version 1. The algorithm is PHYS/CharacterRagdollVehicle.md "Vehicle algorithm".
/// @endcontract
struct Zenith_VehicleComponent
{
	Zenith_VehicleWheel m_axWheels[uPHYSICS_MAX_VEHICLE_WHEELS];      // 1 Wheels (fixed array)
	uint32 m_uWheelCount = 4;                                        // 2 WheelCount
	float m_fMaxEngineTorque = 400.0f;                                // 3 MaxEngineTorque
	float m_fMaxRPM = 6500.0f;                                        // 4 MaxRPM
	float m_fIdleRPM = 900.0f;                                        // 5 IdleRPM
	float m_afGearRatios[8] = { -3.2f, 3.6f, 2.1f, 1.4f, 1.0f, 0.8f, 0.0f, 0.0f };   // 6 GearRatios (index 0 reverse)
	uint32 m_uGearCount = 6;                                         // 7 GearCount (including reverse)
	float m_fFinalDrive = 3.9f;                                       // 8 FinalDrive
	float m_fShiftUpRPM = 5800.0f;                                    // 9 ShiftUpRPM
	float m_fShiftDownRPM = 2200.0f;                                  // 10 ShiftDownRPM
	float m_fAntiRollStiffness = 6000.0f;                             // 11 AntiRollStiffness (front and rear pairs)
	Zenith_PhysicsLayerMask m_uGroundMask = uPHYSICS_LAYER_MASK_ALL;  // 12 GroundMask
	float m_fThrottle = 0.0f;                                         // 20 Throttle, [0, 1] (input)
	float m_fBrake = 0.0f;                                            // 21 Brake, [0, 1] (input)
	float m_fSteer = 0.0f;                                            // 22 Steer, [−1, 1] (input)
	float m_fHandbrake = 0.0f;                                        // 23 Handbrake, [0, 1] (input)
	float m_fEngineRPM = 900.0f;                                      // 30 EngineRPM (state)
	sint32 m_iGear = 1;                                              // 31 Gear (state; 0 reverse, 1 … n forward)
	Zenith_VehicleWheelState m_axWheelStates[uPHYSICS_MAX_VEHICLE_WHEELS];   // 32 WheelStates (state)
	static const Zenith_TypeID xTYPE_ID;                              // "Zenith.Physics.Vehicle"
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};
```

`Zenith_VehicleComponent` is 1,000 bytes or less (`static_assert(sizeof(Zenith_VehicleComponent) <= 1024)` beside the registration), within the 1,024-byte component limit of `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeRegistry header".

## Zenith_PhysicsDebug header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsDebug.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_PhysicsDebugView
/// Ownership: Plain bit flags: the value of the development configuration key physics.debug.views and of the console
///   command physics.debug.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Unknown bits are ignored.
/// Version: since Z2 Stage 3. Development builds alone draw anything (PHYS.md "Debug views").
/// @endcontract
enum Zenith_PhysicsDebugView : uint32
{
	PHYSICS_DEBUG_VIEW_NONE = 0,
	PHYSICS_DEBUG_VIEW_SHAPES = 1u << 0,          // wireframes, coloured by motion type and sleep state
	PHYSICS_DEBUG_VIEW_BOUNDS = 1u << 1,          // world AABBs
	PHYSICS_DEBUG_VIEW_CONTACTS = 1u << 2,        // this tick's contact points and normals
	PHYSICS_DEBUG_VIEW_TRIGGERS = 1u << 3,        // trigger volumes and their current overlaps
	PHYSICS_DEBUG_VIEW_CONSTRAINTS = 1u << 4,     // joint frames and limits
	PHYSICS_DEBUG_VIEW_CONTROLLERS = 1u << 5,     // capsules, ground normals, targets, step probes
	PHYSICS_DEBUG_VIEW_RAGDOLLS = 1u << 6,        // parts, drive targets, blend weight text
	PHYSICS_DEBUG_VIEW_QUERIES = 1u << 7,         // the last 256 queries of the frame
	PHYSICS_DEBUG_VIEW_VEHICLES = 1u << 8,        // suspension rays, wheel contacts, slip text
	PHYSICS_DEBUG_VIEW_LAYERS = 1u << 9,          // layer index text per body
	PHYSICS_DEBUG_VIEW_FROZEN = 1u << 10,         // bodies frozen outside the active region
};
```

## ZenithScene interfaces the Physics module implements

### Zenith_CollisionQuery

`Zenith2/Engine/ZenithScene/Zenith_CollisionQuery.h` is SCENE's: it is declared exactly as `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface" specifies, and the resource type `Zenith.CollisionQuery` (`TYPE_KIND_RESOURCE`, owning phase `PHASE_PHYSICS`) registered, as `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_CollisionQuery header" adopts it (cross-area row GA2; the GAME reviewer confirmed that a ZenithScene interface implemented by a module is a legal downward registration, [Z2-N-ARCH-078]). PHYS implements it in `ZenithPhysics/Internal/Query/Zenith_PhysicsCollisionQuery.h`:

```cpp
// Zenith2/Engine/ZenithPhysics/Internal/Query/Zenith_PhysicsCollisionQuery.h
class Zenith_PhysicsCollisionQuery final : public Zenith_CollisionQuery
{
public:
	explicit Zenith_PhysicsCollisionQuery(const Zenith_PhysicsWorldResource& xWorld);
	bool Sweep(const Zenith_CollisionSweepDesc& xDesc, Zenith_CollisionHit& xHitOut) const override;
private:
	const Zenith_PhysicsWorldResource* m_pxWorld = nullptr;
};
```

`Sweep`: validate the descriptor (finite fields, `|direction| = 1 ± 1e-4`, distance ≥ 0, radius ≥ 0; otherwise false and `physics.query.invalid_delta`); radius 0 runs `RayCast`, otherwise `SphereCast`, with `uLayers = m_uLayerMask` and `xIgnoreRoot = m_xIgnoreRoot`; the hit's distance, position, normal and entity are copied; equal distances resolve to the lowest simulation ID through the collector key. AttachWorld adds the instance with `AddWorldResource(Zenith_CollisionQuery::xTYPE_ID, PHASE_PHYSICS, &instance)`, and `Physics.Step` declares `WriteResource(Zenith_CollisionQuery::xTYPE_ID)`, so every reader is ordered against the step by declared access (row GA3 (a)).

### Zenith_FluidQuery

`Zenith2/Engine/ZenithScene/Zenith_FluidQuery.h` (`Zenith_FluidSample`, the interface `Zenith_FluidQuery` with `virtual bool Sample(float fX, float fZ, Zenith_FluidSample& xSampleOut) const = 0;` and `xTYPE_ID` "Zenith.FluidQuery") is SCENE's: it is declared, and the resource type `Zenith.FluidQuery` (`TYPE_KIND_RESOURCE`, owning phase `PHASE_INPUT`) registered, exactly as `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Zenith_FluidQuery header" specifies (row PH8, applied by SCENE; ENV implements it over its water state and adds it in its `AttachWorld`, `Zenith2/Docs/Design/ENV/WaterWeather.md` → "Fluid query provider"). Neither module depends on the other ([Z2-N-ARCH-850]).

PHYS's use: `Physics.Step` declares `ReadResource` of `Zenith.FluidQuery` by name (PHYS/Simulation.md "Frame-graph placement") and calls `Sample` from the buoyancy pre-substep work alone (PHYS/Simulation.md "Buoyancy algorithm"), with the sample's surface height, flow velocity and density. A world without the Environment module has no such resource, `GetResource` returns null, and every buoyancy body then integrates as a plain body (`Test_FluidQuery_AbsentResourceNoBuoyancy`); a `Sample` that returns false leaves that sample unsubmerged.

## Script bindings

The bindings are registered by `Zenith_Module_Physics_Start` through `Zenith_ModuleStartContext::RegisterScriptBindings`, in namespace `Physics` (Luau `Zenith.Physics`), over the Luau-free ABI of `Zenith2/Docs/Scripting/Bindings.md` → "Public binding types". Thunks live in `ZenithPhysics/Internal/Script/Zenith_PhysicsScriptBindings.cpp` and include `ZenithReflect/Zenith_ScriptBinding.h` alone ([Z2-N-ARCH-085]). Every `READ_WORLD` function names the resource `Zenith.Physics.World`; every `BUFFERED` function names `uSIDE_EFFECT_MASK_FIELD_COMMAND` (module commands travel as field commands, the ANIM convention) or `SIDE_EFFECT_KIND_ASYNC_REQUEST`. Contexts are `SCRIPT_FUNCTION_CONTEXT_SYSTEM | SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR` unless stated.

| Function | Effect | Arguments → results | Notes |
|---|---|---|---|
| `Physics.Layer(name)` | PURE | `string` (resolve kind `physicsLayer`) → `number` | the layer's bit mask (`1 << index`); the name table is the configured layer names, so an unknown name is ZS1807 at load |
| `Physics.RayCast(origin, direction, maxDistance, layers?, ignore?)` | READ_WORLD | vector, vector, number, number?, entity? → `Zenith.Physics.RayHit` | `RayHit` is the value struct `{ Hit: boolean, Entity: entity?, Position: vector, Normal: vector, Distance: number }` |
| `Physics.SphereCast(origin, radius, direction, maxDistance, layers?, ignore?)` | READ_WORLD | → `RayHit` | |
| `Physics.CapsuleCast(origin, rotation, radius, halfHeight, direction, maxDistance, layers?, ignore?)` | READ_WORLD | → `RayHit` | |
| `Physics.OverlapSphere(centre, radius, layers?, triggers?)` | READ_WORLD | → `Zenith.Physics.OverlapResult` `{ Count: number, Entities: { entity } }` (at most 16) | ascending simulation ID |
| `Physics.GetVelocity(entity)` | READ_WORLD | → vector, vector | the body's linear and angular velocity after the latest step; a live entity of this world with no body (or a body still uncreated) returns two zero vectors and raises nothing |
| `Physics.AddImpulse(entity, impulse, point?)` | BUFFERED | → nothing | `Physics.AddImpulse` command |
| `Physics.AddForce(entity, force, point?)` | BUFFERED | → nothing | |
| `Physics.SetVelocity(entity, linear?, angular?)` | BUFFERED | → nothing | |
| `Physics.SetSleeping(entity, sleeping)` | BUFFERED | → nothing | |
| `Physics.EnterRagdoll(entity, blendSeconds?, impulse?, bone?)` | BUFFERED | → nothing | `Physics.EnterRagdoll` |
| `Physics.RecoverRagdoll(entity, blendSeconds?, getUpClip?)` | BUFFERED | → nothing | `getUpClip` an asset value of kind `Animation.Clip`, passed as its persistent ID |
| `Physics.RequestQuery(query)` | BUFFERED (`SIDE_EFFECT_KIND_ASYNC_REQUEST`), BEHAVIOUR | `Zenith.Physics.QueryRequest` → `Zenith.AsyncTicket` | the deferred physics query of `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"; its source kind is `ASYNC_SOURCE_PHYSICS_QUERY` (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"); this binding is the one spelling of the request, and core-doc item X264 (h) renames the registry's `ctx:RequestQuery` entry to it; result decoded by PHYS's decoder (PHYS/Simulation.md "Deferred queries") |
| `Physics.MakeRayQuery(origin, direction, maxDistance, layers?)`, `Physics.MakeSphereQuery(…)`, `Physics.MakeOverlapQuery(centre, radius, layers?)` | PURE | → `Zenith.Physics.QueryRequest` | builders of the value struct |

**Misuse raises a controlled Luau error, always** ([Z2-N-CONTRACT-473]: "Script misuse never relies on asserts; it always raises a controlled Luau error"). Every binding validates its arguments before it reads the world or records anything, and a violation fails the call through `Zenith_ScriptFailThunk` (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md`, kind `SCRIPT_ERROR_KIND_RUNTIME`) with a catalogued code, records nothing and counts nothing in PHYS. The C++ query contract ("false or 0 and `physics.query.invalid_delta`", PHYS/Simulation.md "Query execution") is for C++ callers alone; a script never reaches it with invalid arguments. The codes:

| Code | Owner | Raised by | Trigger | Text |
|---|---|---|---|---|
| ZS1104 | SCRIPT | the dispatcher's argument conversion | `nil` where an entity is required | SCRIPT's catalogue text |
| ZS1101 | SCRIPT | the dispatcher's entity conversion for `READ_WORLD` functions (`GetVelocity`'s entity, the casts' and `OverlapSphere`'s `ignore`) | a stale entity handle (destroyed entity, generation mismatch), detected at the call because a `READ_WORLD` function reads the world | SCRIPT's catalogue text |
| ZS1102 | SCRIPT | the same conversion | an entity handle of another world | SCRIPT's catalogue text |
| ZS1511 | SCRIPT | the dispatcher's argument conversion | an argument of the wrong Luau type | SCRIPT's catalogue text |
| ZS2301 | PHYS | every cast and `Make*Query` builder | a direction whose length differs from 1 by more than 1e-4 | `<Function>: direction (argument <Index>) is not a unit vector` |
| ZS2302 | PHYS | casts, overlaps, builders, ragdoll calls | a negative or non-finite distance, radius, half height or blend time, or a blend time above 10 s | `<Function>: argument <Index> (<Name>) is out of range` |
| ZS2303 | PHYS | every function taking a vector or rotation | a non-finite component, or a rotation whose length differs from 1 by more than 1e-4 | `<Function>: argument <Index> is not finite` |
| ZS2304 | PHYS | every function taking `layers` | a layer mask with a bit at or above the configured layer count | `<Function>: layer mask <Mask> names an undeclared layer` |
| ZS2305 | PHYS | `Make*Query` builders | `maxHits` outside 1 … 16 | `<Function>: maxHits must be 1 … 16, got <Value>` |
| ZS1807, and the bad-argument error at the call | SCRIPT | the module loader and the dispatcher's name-table lookup ([Z2-N-SCRIPT-922]) | a `Physics.Layer` name the `physicsLayer` table lacks: a constant at load, a non-constant at the call | SCRIPT's catalogue text |

ZS2300–ZS2399 is PHYS's block of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges", assigned by the orchestrator's MISUSE-BLOCKS-2 ruling (2026-10-09; AI holds ZS2500–ZS2599, so no other area competes for it) and written into the catalogue through core-doc item X264 (k) before PHYS's bindings diff; SCRIPT's rule that the first reservation applied wins ([Z2-N-SCRIPT-926]) still orders the catalogue row, and the bindings ticket's tests read the five codes ZS2301–ZS2305 from that row. Two conditions of `BUFFERED` functions are not call-time misuse and are deliberately left to playback, because a `BUFFERED` function reads no world state at the call: a stale entity (it may also become stale between the call and the sync point) and a ragdoll call on an entity without `Zenith.Physics.Ragdoll` (the component may be added or removed by a command that plays first) are dropped at playback and counted (`physics.command.stale_delta`, `physics.ragdoll.enter_refused` with cause "no Zenith.Physics.Ragdoll component", or `physics.ragdoll.recover_ignored_delta`). A `READ_WORLD` function does read the world at the call, so SCRIPT's dispatcher raises ZS1101 or ZS1102 for its stale or foreign entity arguments. `Test_PhysicsBindings_MisuseRaisesCatalogueError` asserts each row: for each code, a call that triggers it raises a Luau error carrying that code, records no command and leaves every PHYS counter unchanged; a stale and a foreign entity passed to `GetVelocity` and as a cast's `ignore` raise ZS1101 and ZS1102; `GetVelocity` of a live bodiless entity returns two zero vectors; and a stale entity and a ragdoll call on an entity without the component, through `BUFFERED` functions, raise nothing and are counted at playback.

The module also calls `SetAsyncResultDecoder(ASYNC_SOURCE_PHYSICS_QUERY, &Zenith_PhysicsDecodeQueryResult)` and `DeclareScriptAsyncSource(ASYNC_SOURCE_PHYSICS_QUERY, uPHYSICS_QUERY_RESULT_BYTES)` (64), and `SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, xDeferredQueryConsumer)`, to which ZenithRuntime's multiplexer routes the requests of `ASYNC_SOURCE_PHYSICS_QUERY` (`Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime").

## Configuration keys

Registered by `Zenith_Module_Physics_RegisterTypes` through `Zenith_TypeRegistry::GetConfigSchema().Register` ([Z2-N-ARCH-848]). "Sim" marks `m_bSimulationAffecting` (recorded in every replay header); "Restart" marks `m_bRequiresRestart`. Allowed layers for simulation-affecting keys: engine default, platform, game, command line (never quality tier or user, so a player setting cannot change a simulation).

| Key | Type | Default | Range | Sim | Restart | Meaning |
|---|---|---|---|---|---|---|
| `physics.substepsPerTick` | INT | 1 | 1 … 8 (`uPHYSICS_MAX_SUBSTEPS_PER_TICK`) | yes | no | substeps per fixed tick |
| `physics.solverVelocityIterations` | INT | 10 | 1 … 64 | yes | no | |
| `physics.solverPositionIterations` | INT | 2 | 1 … 16 | yes | no | |
| `physics.gravityX`, `physics.gravityY`, `physics.gravityZ` | FLOAT | 0, −9.81, 0 | −1000 … 1000 | yes | no | m/s² |
| `physics.maxBodies` | INT | the pending row "Physics body capacity per world" (design default 65,536) | 1 … 8,388,607 | yes | yes | |
| `physics.maxBodyPairs` | INT | 65,536 | 1,024 … 4,194,304 | yes | yes | |
| `physics.maxContactConstraints` | INT | 32,768 | 1,024 … 4,194,304 | yes | yes | |
| `physics.maxConstraints` | INT | the pending row "Physics constraint capacity per world" (design default 16,384) | 1 … 1,048,575 | yes | yes | |
| `physics.maxContactRecordsPerTick` | INT | the pending row "Physics contact records per tick" (design default 8,192) | 256 … 1,048,576 | yes | yes | the contact sink's capacity |
| `physics.maxActiveRagdolls` | INT | the pending row "Active ragdolls per world" (design default 32) | 1 … 1,024 | yes | yes | the most ragdolls of one world whose `Zenith.Physics.Ragdoll` State is not NONE (ENTERING, ACTIVE or RECOVERING); an enter beyond it is refused |
| `physics.maxShapes`, `physics.maxMaterials` | INT | 65,536, 4,096 | 1 … 2^24 | no | yes | process tables |
| `physics.stepScratchBytes` | INT | 16 MiB | 1 MiB … 1 GiB | no | yes | per-world step arena (a member of Z2-B-PHYS-001) |
| `physics.speculativeContactDistance` | FLOAT | 0.02 | 0 … 1 | yes | no | metres |
| `physics.penetrationSlop` | FLOAT | 0.02 | 0 … 0.5 | yes | no | metres |
| `physics.maxLinearSpeed` | FLOAT | 500 | 1 … 100,000 | yes | no | m/s |
| `physics.sleepSeconds`, `physics.sleepVelocity` | FLOAT | 0.5, 0.03 | 0.01 … 60, 0 … 10 | yes | no | |
| `physics.layerNames` | STRING | `Default` | — | yes | yes | PHYS.md "Layer configuration" |
| `physics.layerCollisions` | STRING | `*:*` | — | yes | yes | PHYS.md "Layer configuration" |
| `physics.maxPendingOpsPerTick` | INT | 16,384 | 256 … 1,048,576 | yes | yes | recorded body operations applied per tick (PHYS/Simulation.md "Pending operations") |
| `physics.deferredQueriesPerTick` | INT | 256 | 1 … 65,536 | yes | yes | deferred queries executed per tick; the excess waits for the next tick in completion-ID order, never dropped (PHYS/Simulation.md "Deferred queries") |
| `physics.physx.parallelStep` | BOOL | true | — | no | no | PhysX drain leaves; results are identical either way (PHYS/Backends.md) |
| `physics.physx.dispatchQueueCapacity` | INT | 4,096 | 256 … 65,536 | no | yes | the PhysX dispatcher's task queue (PHYS/Backends.md "PhysX step and the dispatcher") |
| `physics.cook.backendSections` | BOOL | true | — | no | — | cook-side: write backend sections (ZenithCook reads it) |
| `physics.debug.views` | INT | 0 | — | no | no | `Zenith_PhysicsDebugView` bits; development builds |

`Zenith_ParsePhysicsLayers(Zenith_StringView strNames, Zenith_StringView strCollisions, Zenith_PhysicsLayerTable& xOut) → Zenith_Result<void, Zenith_PhysicsError>` (declared in `Zenith_PhysicsWorld.h`'s companion internal header `Internal/Zenith_PhysicsLayers.h`, tested publicly through the module) implements the grammar of PHYS.md "Layer configuration".

## Authority census rows

The rows PHYS adds to `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the diff that registers each type (change-control rule 3 of `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Contract stewardship"), in the census format of `Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census format":

| Area | Stable name | Kind | Properties | Owning phase | Writer sources | Arbitration | Writers | Readers outside the owning phase | Source |
|---|---|---|---|---|---|---|---|---|---|
| PHYS | `Zenith.Physics.RigidBody` | component | 1–14 | CommandsOnly | — | none | commands | `Physics.Step` (5), `Physics.SyncTransforms` (6) | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsComponents header" |
| PHYS | `Zenith.Physics.RigidBody` | component | 20–23 | Physics | — | none | `Physics.Step` | scripts (2, 10), AI (7), extractors | same |
| PHYS | `Zenith.Physics.Collider` | component | 1–8 | CommandsOnly | — | none | commands | `Physics.Step` (5) | same |
| PHYS | `Zenith.Physics.Joint` | component | 1–11, 15–19 | CommandsOnly | — | none | commands | `Physics.Step` (5) | same |
| PHYS | `Zenith.Physics.Joint` | component | 12–14, 20 | EarlyScripts | — | none | behaviours, chunk systems | `Physics.Step` (5) | same |
| PHYS | `Zenith.Physics.Joint` | component | 21 | Physics | — | none | `Physics.Step` | scripts (10) | same |
| PHYS | `Zenith.Physics.CharacterController` | component | 1–15 | CommandsOnly | — | none | commands | `Physics.CharacterMove` (4), `Physics.Step` (5) | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_CharacterController header" |
| PHYS | `Zenith.Physics.CharacterController` | component | 20–23 | EarlyScripts | — | none | behaviours, chunk systems | `Physics.CharacterMove` (4) | same |
| PHYS | `Zenith.Physics.CharacterController` | component | 30–39 | MovementArbitration | — | none | `Physics.CharacterMove` | `Physics.Step` (5), `Physics.SyncTransforms` (6), scripts (10), `Gameplay.Cameras` (7) | same |
| PHYS | `Zenith.Physics.Ragdoll` | component | 1–2 | PostPhysics | — | none | `Physics.RagdollBlend`; PHYS command playback | `Animation.Significance` (3, property 1 State, previous tick), `Animation.RagdollOwnership` (6, after the writer), `Animation.PostPhysicsBlend` (6, after the writer), scripts (10) | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_Ragdoll header" |
| PHYS | `Zenith.Physics.Ragdoll` | component | 3–5 | CommandsOnly | — | none | commands | PHYS playback, `Physics.Step` (5) | same |
| PHYS | `Zenith.Physics.Vehicle` | component | 1–12 | CommandsOnly | — | none | commands | `Physics.Step` (5) | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsVehicle header" |
| PHYS | `Zenith.Physics.Vehicle` | component | 20–23 | EarlyScripts | — | none | behaviours, chunk systems | `Physics.Step` (5) | same |
| PHYS | `Zenith.Physics.Vehicle` | component | 30–32 | Physics | — | none | `Physics.Step` | scripts (10), AUDIO, extractors | same |
| PHYS | `Zenith.Physics.Buoyancy` | component | 1–4 | CommandsOnly | — | none | commands | `Physics.Step` (5) | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsComponents header" |
| PHYS | `Zenith.Physics.Buoyancy` | component | 5 | EarlyScripts | — | none | behaviours | `Physics.Step` (5) | same |
| SCENE | `Zenith.LocalTransform` | component | 1–2 | PostPhysics | Physics, Ragdoll | Precedence (default order) | `Physics.SyncTransforms` (Physics), `Physics.RagdollBlend` (Ragdoll) | — | `Zenith2/Docs/Design/SCENE/Transforms.md` → "Transform components" (PHYS's writer rows, cross-area row S2) |
| PHYS | `Zenith.Physics.World` | resource | — | Physics | — | none | `Physics.Step` | ANIM (3, 6), `Physics.CharacterMove` (4), PHYS phase-6 nodes, AI (7), scripts (2, 10), debug epilogue | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsWorldResource header" |
| GAME/PHYS | `Zenith.CollisionQuery` | resource | — | Physics | — | none | `Physics.Step` | `Gameplay.Cameras` (7) | `Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface" |
| PHYS | `Zenith.Physics.ContactBegan`, `.ContactEnded`, `.TriggerEntered`, `.TriggerExited`, `.Interaction`, `.JointBroken` | event | — | — | — | — | `Physics.Step` (5) | phase 9 handlers, late scripts (10) | `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsEvents header" |
