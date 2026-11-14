# PHYS design — the neutral physics API

This file is part of the PHYS design family; the entry document is `Zenith2/Docs/Design/PHYS.md`. It specifies, header by header, the backend-neutral physics API of `ZenithPhysics` that both backends implement: the identity and handle types, the error enumeration, the process-wide `Zenith_Physics` service, shapes, materials, the per-world `Zenith_PhysicsWorld` simulation interface, queries and their deterministic collector, the two backend factory headers, the module entry header and the collision-shape asset type. The ECS-facing half (components, events, the world resource, commands, ragdolls, the character controller, vehicles, bindings) is `Zenith2/Docs/Design/PHYS/ComponentsAPI.md`.

## Scope of this file

| Header (all under `Zenith2/Engine/ZenithPhysics/` unless stated) | Declares | Heading |
|---|---|---|
| `Zenith_PhysicsTypes.h` | limits, handle types, motion enums, layer masks, the backend kind | "Zenith_PhysicsTypes header" |
| `Zenith_PhysicsErrors.h` | `Zenith_PhysicsError`, `Zenith_ToString` | "Zenith_PhysicsErrors header" |
| `Zenith_Physics.h` | `Zenith_PhysicsDesc`, `Zenith_PhysicsCapabilities`, `Zenith_PhysicsBackendInfo`, `Zenith_Physics`, `Zenith_PfnCreatePhysics` | "Zenith_Physics header" |
| `Zenith_PhysicsShape.h` | shape kinds and descriptors, the cooked-shape view, shape information | "Zenith_PhysicsShape header" |
| `Zenith_PhysicsMaterial.h` | combine modes, material data, interaction rules, `Zenith_PhysicsMaterialAsset`, `Zenith_CombinePhysicsMaterials`, `Zenith_MakePhysicsMaterialNameID` | "Zenith_PhysicsMaterial header" |
| `Zenith_PhysicsWorld.h` | world, body and constraint descriptors, body state, step statistics, the contact sink, `Zenith_PhysicsWorld` | "Zenith_PhysicsWorld header" |
| `Zenith_PhysicsQuery.h` | query filters, query descriptors, hits, `Zenith_PhysicsHitCollector` | "Zenith_PhysicsQuery header" |
| `ZenithPhysics_Jolt/Zenith_Physics_Jolt.h`, `ZenithPhysics_PhysX/Zenith_Physics_PhysX.h` | the two factories | "Backend factory headers" |
| `Zenith_Module_Physics.h` | the module entry points | "Zenith_Module_Physics header" |
| `Zenith_CollisionShapeAsset.h` | `Zenith_CollisionShapeAsset` (asset kind 16) | "Zenith_CollisionShapeAsset header" |

Rules that hold for every header of this file:

- Every fallible function returns `Zenith_Result<T, Zenith_PhysicsError>`.
- No header includes a Jolt, PhysX, STL or platform header ([Z2-N-CONTRACT-043]); `Libgraph`'s leakage rule proves it.
- Every type carries one contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type"; value blocks name trivially copyable types.
- Units follow `Zenith2/Docs/Design/CORE/Maths.md` → "Units": metres, kilograms, seconds, radians, newtons; physical-material quantities are the dimensionless coefficients this document names, and density is kg/m³.
- Every interface names its implementation's first base as itself, and the backends implement `Zenith_Physics` and `Zenith_PhysicsWorld` alone ([Z2-N-CONTRACT-034]).

## Header list

```
Zenith2/Engine/ZenithPhysics/
	Zenith_Module_Physics.h        Zenith_PhysicsTypes.h      Zenith_PhysicsErrors.h      Zenith_Physics.h
	Zenith_PhysicsShape.h          Zenith_PhysicsMaterial.h   Zenith_PhysicsWorld.h       Zenith_PhysicsQuery.h
	Zenith_CollisionShapeAsset.h   Zenith_PhysicsComponents.h Zenith_PhysicsEvents.h      Zenith_PhysicsWorldResource.h
	Zenith_PhysicsCommands.h       Zenith_Ragdoll.h           Zenith_CharacterController.h Zenith_PhysicsVehicle.h
	Zenith_PhysicsDebug.h
Zenith2/Engine/ZenithPhysics_Jolt/Zenith_Physics_Jolt.h
Zenith2/Engine/ZenithPhysics_PhysX/Zenith_Physics_PhysX.h
```

The last eight headers of `ZenithPhysics` are specified in `Zenith2/Docs/Design/PHYS/ComponentsAPI.md`.

## Zenith_PhysicsTypes header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract functions Zenith_PhysicsLimits
/// Ownership: constexpr data.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Counts; seconds for fPHYSICS_MAX_STEP_SECONDS.
/// Errors: None; every configuration key bounded by a limit is range-checked by the configuration schema.
/// Version: since Z2 Stage 3. A limit changes only with its row of PHYS.md "Limits and constants".
/// @endcontract
inline constexpr uint32 uPHYSICS_MAX_SUBSTEPS_PER_TICK = 8;          // the substep cap; physics.substepsPerTick is 1 … 8
inline constexpr uint32 uPHYSICS_MAX_LAYERS = 32;                    // object layers 0 … 31
inline constexpr uint32 uPHYSICS_MAX_BODIES_PER_WORLD = 8388607;     // 2^23 − 1, the body-index field
inline constexpr uint32 uPHYSICS_MAX_CONSTRAINTS_PER_WORLD = 1048575; // 2^20 − 1
inline constexpr uint32 uPHYSICS_MAX_COMPOUND_CHILDREN = 64;          // children of one runtime compound
inline constexpr uint32 uPHYSICS_MAX_COOKED_COMPOUND_CHILDREN = 65535; // children of one cooked static compound
inline constexpr uint32 uPHYSICS_MAX_MATERIAL_PALETTE = 256;          // materials of one cooked shape
inline constexpr uint32 uPHYSICS_MAX_CONVEX_VERTICES = 256;
inline constexpr uint32 uPHYSICS_MAX_MESH_TRIANGLES = 2097152;        // 2^21 per cooked triangle mesh
inline constexpr uint32 uPHYSICS_MAX_HEIGHTFIELD_SAMPLES = 4097;      // per side; heightfields are square
inline constexpr uint32 uPHYSICS_MAX_HEIGHTFIELD_PALETTE = 127;       // PhysX's 7-bit per-triangle material index (127 = eHOLE)
inline constexpr uint32 uPHYSICS_MAX_WORLD_TAG = 4095;                // the 12-bit world-tag field of body and constraint handles
inline constexpr uint32 uPHYSICS_MAX_QUERY_HITS = 256;                // the largest K of a Zenith_PhysicsHitCollector
inline constexpr uint32 uPHYSICS_MAX_RAGDOLL_PARTS = 64;
inline constexpr uint32 uPHYSICS_MAX_VEHICLE_WHEELS = 8;
inline constexpr uint32 uPHYSICS_MAX_BUOYANCY_SAMPLES = 32;
inline constexpr float fPHYSICS_MAX_STEP_SECONDS = 0.1f;               // a substep longer than this is PHYSICS_ERROR_INVALID_DESC
inline constexpr uint32 uPHYSICS_NO_INDEX = 0xFFFFFFFFu;

/// @contract enum Zenith_PhysicsBackendKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. The values equal BACKEND_ID_PHYSICS_JOLT and BACKEND_ID_PHYSICS_PHYSX of
///   ZenithSave/Zenith_ReplayLog.h (a static_assert in ZenithPhysics/Internal/Save/ checks it); never renumbered.
/// @endcontract
enum Zenith_PhysicsBackendKind : uint8
{
	PHYSICS_BACKEND_NONE = 0,
	PHYSICS_BACKEND_JOLT = 1,
	PHYSICS_BACKEND_PHYSX = 2,
};

/// @contract enum Zenith_PhysicsMotionType
/// Ownership: Plain enumeration; serialized as u8 in components and cooked data.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Any other value read from data is PHYSICS_ERROR_INVALID_DESC at body creation.
/// Version: since Z2 Stage 3. Values never move.
/// @endcontract
enum Zenith_PhysicsMotionType : uint8
{
	PHYSICS_MOTION_TYPE_STATIC = 1,      // never moves under simulation; infinite mass
	PHYSICS_MOTION_TYPE_KINEMATIC = 2,   // moved by targets with a velocity; infinite mass; pushes dynamics
	PHYSICS_MOTION_TYPE_DYNAMIC = 3,     // integrated by the solver
};

/// @contract enum Zenith_PhysicsMotionQuality
/// Ownership: Plain enumeration; serialized as u8.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Zenith_PhysicsMotionType.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_PhysicsMotionQuality : uint8
{
	PHYSICS_MOTION_QUALITY_DISCRETE = 1,     // speculative contacts alone
	PHYSICS_MOTION_QUALITY_CONTINUOUS = 2,   // swept (linear-cast) continuous collision detection
};

/// @contract enum Zenith_PhysicsActivation
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_PhysicsActivation : uint8
{
	PHYSICS_ACTIVATION_ACTIVATE = 1,         // wake the body (and what it touches) when its state is set
	PHYSICS_ACTIVATION_LEAVE = 2,            // keep its sleeping state
};

/// @contract enum Zenith_PhysicsBodyFlags
/// Ownership: Plain bit flags; Zenith_PhysicsBodyDesc::m_uFlags and property 9 of Zenith.Physics.RigidBody.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: An undefined bit read from data is ignored and counted in physics.body.unknown_flags_delta.
/// Version: since Z2 Stage 3. Bit positions never move.
/// @endcontract
enum Zenith_PhysicsBodyFlags : uint32
{
	PHYSICS_BODY_FLAG_NONE = 0,
	PHYSICS_BODY_FLAG_REPORT_CONTACTS = 1u << 0,      // ContactBegan / ContactEnded events for pairs with this body
	PHYSICS_BODY_FLAG_REPORT_INTERACTIONS = 1u << 1,  // Interaction events for pairs with this body
	PHYSICS_BODY_FLAG_START_ASLEEP = 1u << 2,
	PHYSICS_BODY_FLAG_LOCK_ROTATION_X = 1u << 3,
	PHYSICS_BODY_FLAG_LOCK_ROTATION_Y = 1u << 4,
	PHYSICS_BODY_FLAG_LOCK_ROTATION_Z = 1u << 5,
	PHYSICS_BODY_FLAG_LOCK_TRANSLATION_X = 1u << 6,
	PHYSICS_BODY_FLAG_LOCK_TRANSLATION_Y = 1u << 7,
	PHYSICS_BODY_FLAG_LOCK_TRANSLATION_Z = 1u << 8,
	PHYSICS_BODY_FLAG_IGNORE_SAME_GROUP = 1u << 9,    // no collision with bodies of the same non-zero collision group (PHYS/Backends.md
	                                                  //   "Jolt layer mapping", "PhysX filter shader"); either body's flag suffices
	PHYSICS_BODY_FLAG_ALLOW_SLEEPING = 1u << 10,
	PHYSICS_BODY_FLAG_TRIGGER = 1u << 11,             // a sensor: overlaps reported, no contact response; always created KINEMATIC and awake
	                                                  //   (PHYS/Simulation.md "Body composition" rule 9, "Trigger semantics")
	PHYSICS_BODY_FLAG_QUERY_ONLY = 1u << 12,          // never collides and overlaps no trigger; found exactly by queries whose filter has
	                                                  //   m_bIncludeQueryOnly (controller capsules during a ragdoll, frozen bodies, editor picks)
};

/// @contract value Zenith_PhysicsLayerMask
/// Ownership: Plain value; bit n set selects object layer n.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
using Zenith_PhysicsLayerMask = uint32;
inline constexpr Zenith_PhysicsLayerMask uPHYSICS_LAYER_MASK_ALL = 0xFFFFFFFFu;

/// @contract value Zenith_PhysicsBodyHandle
/// Ownership: Plain value issued by a Zenith_PhysicsWorld; a world handle (Zenith2/Docs/Contracts.md "Handle kinds"):
///   the issuing world owns the body; copying never extends its lifetime.
/// Thread affinity: IMMUTABLE. Resolving it follows Zenith_PhysicsWorld's affinity. Members non-const: a handle is
///   copy-assigned wherever it is stored (component fields, containers, Zenith_Result).
/// Invalidation: Stale after DestroyBody (generation compare) and in any other world (world-tag compare), in every
///   configuration. Serialized only by the physics save section (handleBits, PHYS/Formats.md "Save section layout") and
///   restored through CreateBodyWithHandle; never in component data.
/// Units: None.
/// Errors: Resolving a null, stale or foreign handle returns PHYSICS_ERROR_NULL_HANDLE, PHYSICS_ERROR_STALE_HANDLE or
///   PHYSICS_ERROR_WRONG_WORLD; nothing asserts.
/// Version: since Z2 Stage 3. Bits 0–22 index, 23–50 generation (1 … 2^28 − 1), 51–62 world tag (12 bits, 1 … 4,095, the ECS
///   world tag range; CreateWorld refuses a larger tag), 63 zero. The index and generation come from "Slot allocation" of
///   PHYS/Simulation.md: the lowest free index and that slot's next generation.
/// @endcontract
class Zenith_PhysicsBodyHandle
{
public:
	constexpr Zenith_PhysicsBodyHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	friend constexpr bool operator==(Zenith_PhysicsBodyHandle, Zenith_PhysicsBodyHandle) = default;
private:
	friend class Zenith_PhysicsHandleCodec;           // ZenithPhysics/Internal/Zenith_PhysicsBackendBase.h (with Zenith_PhysicsSlotTable and
	                                                  //   Zenith_PhysicsWorldBase, PHYS.md "ZenithPhysics files")
	explicit constexpr Zenith_PhysicsBodyHandle(uint64 ulBits) : m_ulBits(ulBits) {}
	uint64 m_ulBits = 0;
};

/// @contract value Zenith_PhysicsConstraintHandle
/// Ownership: As Zenith_PhysicsBodyHandle, for constraints.
/// Thread affinity: IMMUTABLE. Members non-const: a handle is copy-assigned wherever it is stored (component fields,
///   containers, Zenith_Result).
/// Invalidation: Stale after DestroyConstraint, after either body's DestroyBody, and in any other world. Serialized only by
///   the physics save section (handleBits, PHYS/Formats.md "Save section layout") and restored through
///   CreateConstraintWithHandle; never in component data.
/// Units: None.
/// Errors: As Zenith_PhysicsBodyHandle.
/// Version: since Z2 Stage 3. Bits 0–19 index, 20–47 generation, 48–59 world tag (12 bits, as Zenith_PhysicsBodyHandle), 60–63
///   zero; allocated as Zenith_PhysicsBodyHandle.
/// @endcontract
class Zenith_PhysicsConstraintHandle
{
public:
	constexpr Zenith_PhysicsConstraintHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	friend constexpr bool operator==(Zenith_PhysicsConstraintHandle, Zenith_PhysicsConstraintHandle) = default;
private:
	friend class Zenith_PhysicsHandleCodec;
	explicit constexpr Zenith_PhysicsConstraintHandle(uint64 ulBits) : m_ulBits(ulBits) {}
	uint64 m_ulBits = 0;
};

/// @contract value Zenith_PhysicsShapeHandle
/// Ownership: Plain value issued by Zenith_Physics::CreateShape; a device-style handle owned by the process's one
///   Zenith_Physics service. The creator holds one reference and releases it with ReleaseShape; each body built on the
///   shape holds one more, released when the body is destroyed.
/// Thread affinity: IMMUTABLE. Members non-const: a handle is copy-assigned wherever it is stored (component fields,
///   containers, Zenith_Result).
/// Invalidation: Stale once its last reference is released (generation compare); never valid in another
///   Zenith_Physics instance. Never serialized: data names a Physics.CollisionShape asset or a primitive.
/// Units: None.
/// Errors: As Zenith_PhysicsBodyHandle (PHYSICS_ERROR_NULL_HANDLE, PHYSICS_ERROR_STALE_HANDLE).
/// Version: since Z2 Stage 3. Bits 0–31 slot, 32–55 generation, 56–63 kind 1.
/// @endcontract
class Zenith_PhysicsShapeHandle
{
public:
	constexpr Zenith_PhysicsShapeHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	friend constexpr bool operator==(Zenith_PhysicsShapeHandle, Zenith_PhysicsShapeHandle) = default;
private:
	friend class Zenith_PhysicsHandleCodec;
	explicit constexpr Zenith_PhysicsShapeHandle(uint64 ulBits) : m_ulBits(ulBits) {}
	uint64 m_ulBits = 0;
};

/// @contract value Zenith_PhysicsMaterialHandle
/// Ownership: Plain value issued by Zenith_Physics::CreateMaterial; owned like Zenith_PhysicsShapeHandle (each shape
///   referencing the material holds a reference).
/// Thread affinity: IMMUTABLE. Members non-const: a handle is copy-assigned wherever it is stored (component fields,
///   containers, Zenith_Result).
/// Invalidation: As Zenith_PhysicsShapeHandle.
/// Units: None.
/// Errors: As Zenith_PhysicsShapeHandle.
/// Version: since Z2 Stage 3. Bits 0–31 slot, 32–55 generation, 56–63 kind 2. The null handle names the default
///   material, which is Zenith_PhysicsMaterialData's member defaults on both backends (friction 0.5).
/// @endcontract
class Zenith_PhysicsMaterialHandle
{
public:
	constexpr Zenith_PhysicsMaterialHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	friend constexpr bool operator==(Zenith_PhysicsMaterialHandle, Zenith_PhysicsMaterialHandle) = default;
private:
	friend class Zenith_PhysicsHandleCodec;
	explicit constexpr Zenith_PhysicsMaterialHandle(uint64 ulBits) : m_ulBits(ulBits) {}
	uint64 m_ulBits = 0;
};
```

<!-- elaboration -->
The body and constraint handles are world handles in the sense of `Zenith2/Docs/Contracts.md` → "Handle kinds" (owner scope: the issuing world, generational, carrying the world tag, never serialized); the shape and material handles are device-style handles of the physics service (owner scope: the one `Zenith_Physics` instance of the process, generational with a kind byte). No new handle kind exists; the four types are new types of two existing kinds. Each library carries the handle tests of [Z2-N-CONTRACT-096] (`Zenith2/Docs/Design/PHYS/Tests.md` → "Handle and table tests").

## Zenith_PhysicsErrors header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_PhysicsError
/// Ownership: Plain enumeration; the E of every Zenith_Result the physics API returns.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator's meaning is the table of PHYS.md "Error codes"; an enumerator shipped in a fix version keeps
///   its meaning ([Z2-N-CONTRACT-467]).
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_PhysicsError : uint8
{
	PHYSICS_ERROR_INVALID_DESC = 1,
	PHYSICS_ERROR_OUT_OF_MEMORY,
	PHYSICS_ERROR_BACKEND_INIT,
	PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE,
	PHYSICS_ERROR_BODY_CAPACITY,
	PHYSICS_ERROR_CONSTRAINT_CAPACITY,
	PHYSICS_ERROR_SHAPE_CAPACITY,
	PHYSICS_ERROR_INVALID_SHAPE,
	PHYSICS_ERROR_NULL_HANDLE,
	PHYSICS_ERROR_STALE_HANDLE,
	PHYSICS_ERROR_WRONG_WORLD,
	PHYSICS_ERROR_COOKED_CORRUPT,
	PHYSICS_ERROR_COOKED_BACKEND_MISMATCH,
	PHYSICS_ERROR_UNSUPPORTED,
	PHYSICS_ERROR_STATE_MISMATCH,
	PHYSICS_ERROR_STEP_FAILED,
	PHYSICS_ERROR_MOTION_TYPE,
	PHYSICS_ERROR_LAYER_CONFIG,
	PHYSICS_ERROR_RAGDOLL_INVALID,
	PHYSICS_ERROR_WORLD_ACTIVE,
};

constexpr const char* Zenith_ToString(Zenith_PhysicsError eError);   // the enumerator name without the prefix, e.g. "INVALID_DESC"
```

## Zenith_Physics header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_Physics.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"
#include "ZenithPhysics/Zenith_PhysicsErrors.h"
#include "ZenithPhysics/Zenith_PhysicsShape.h"
#include "ZenithPhysics/Zenith_PhysicsMaterial.h"

class Zenith_Heap;
class Zenith_FaultInjector;
class Zenith_Arena;
class Zenith_PhysicsWorld;
struct Zenith_PhysicsWorldDesc;

/// @contract value Zenith_PhysicsDesc
/// Ownership: Plain value copied by the factory; the pointers are borrowed and outlive the created service.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Counts and bytes.
/// Errors: A null heap, a zero capacity or a capacity above its limit is PHYSICS_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsDesc
{
	Zenith_Heap* m_pxHeap = nullptr;                  // the root heap; every backend allocation is routed to it
	uint32 m_uMaxShapes = 65536;                     // live shapes in the process (physics.maxShapes)
	uint32 m_uMaxMaterials = 4096;                   // live materials in the process (physics.maxMaterials)
	bool m_bDevelopmentFeatures = true;               // debug geometry readback for the debug views; false in Shipping
	Zenith_FaultInjector* m_pxFaults = nullptr;       // tests: one fault point per construction step
};

/// @contract enum Zenith_PhysicsCapabilities
/// Ownership: Plain bit flags returned by Zenith_Physics::GetBackendInfo.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. The capability matrix of PHYS.md "Capability matrix".
/// @endcontract
enum Zenith_PhysicsCapabilities : uint32
{
	PHYSICS_CAPABILITY_NONE = 0,
	PHYSICS_CAPABILITY_EXACT_STATE_RESTORE = 1u << 0,   // SaveState/RestoreState continue bit-identically
	PHYSICS_CAPABILITY_PARALLEL_STEP = 1u << 1,         // StepOnce fans the solver out over sub-jobs
	PHYSICS_CAPABILITY_BACKEND_SHAPES = 1u << 2,        // BakeShape produces a backend section the loader accepts
	PHYSICS_CAPABILITY_ORIGIN_SHIFT = 1u << 3,          // ShiftOrigin is native (otherwise body-by-body)
};

/// @contract value Zenith_PhysicsBackendInfo
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. m_ulDataTag changes whenever the backend's binary shape or state layout can change (a
///   version or configuration pin of PHYS/Backends.md "Backend data tag").
/// @endcontract
struct Zenith_PhysicsBackendInfo
{
	const Zenith_PhysicsBackendKind m_eKind = PHYSICS_BACKEND_NONE;
	const char* const m_szName = nullptr;             // "Jolt", "PhysX": the BACKEND_NAME
	const char* const m_szVersion = nullptr;          // the manifest version string, e.g. "5.4.0"
	const uint64 m_ulDataTag = 0;
	const uint32 m_uCapabilities = PHYSICS_CAPABILITY_NONE;
};

/// @contract interface Zenith_Physics
/// Ownership: Created by Zenith_CreatePhysics_Jolt or Zenith_CreatePhysics_PhysX as a Zenith_Owned<Zenith_Physics>; the
///   Physics module instance owns the one instance of the process, and tools and conformance tests own their own; at
///   most one instance per backend kind is alive in a process (PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE). Every world it
///   created, every shape and every material is destroyed or released before it; the destructor reports and frees
///   any left (counter physics.leaked_handles at destruction) and ZENITH_CHECKs that no world is alive.
/// Thread affinity: ANY_THREAD_EXCEPT(CreateWorld). CreateShape, CreateMaterial, Release*, GetShapeInfo and BakeShape
///   are callable concurrently from any thread (asset-loader jobs, cook workers); CreateWorld is MAIN_THREAD.
/// Invalidation: None.
/// Units: Metres, kilograms, bytes.
/// Errors: Zenith_Result<…, Zenith_PhysicsError>; corrupt cooked data is PHYSICS_ERROR_COOKED_CORRUPT and never
///   asserts; a backend section built by another backend or data tag is PHYSICS_ERROR_COOKED_BACKEND_MISMATCH.
///   A null descriptor pointer member that the kind needs is PHYSICS_ERROR_INVALID_DESC.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callback.
/// Locks: uLOCK_LEVEL_PHYSICS_HANDLE_TABLES (500), short-hold, around slot allocation and release only.
/// Version: since Z2 Stage 3. Source API; a new member gets a default implementation returning PHYSICS_ERROR_UNSUPPORTED.
/// Example: Zenith2/Engine/ZenithPhysics/Tests/Conformance/Examples/Zenith_Physics_Example.cpp
/// @endcontract
class Zenith_Physics
{
public:
	virtual ~Zenith_Physics() = default;

	virtual Zenith_PhysicsBackendInfo GetBackendInfo() const = 0;

	virtual Zenith_Result<Zenith_PhysicsMaterialHandle, Zenith_PhysicsError> CreateMaterial(
		const Zenith_PhysicsMaterialData& xData) = 0;
	virtual void ReleaseMaterial(Zenith_PhysicsMaterialHandle xMaterial) = 0;

	virtual Zenith_Result<Zenith_PhysicsShapeHandle, Zenith_PhysicsError> CreateShape(
		const Zenith_PhysicsShapeDesc& xDesc) = 0;
	// From cooked data: uses the backend section when its kind and data tag match this backend, otherwise builds the
	// shape from the neutral section (counter physics.shapes.built_from_neutral).
	virtual Zenith_Result<Zenith_PhysicsShapeHandle, Zenith_PhysicsError> CreateShapeFromCooked(
		const Zenith_PhysicsCookedShapeView& xCooked, Zenith_Span<const Zenith_PhysicsMaterialHandle> axPalette) = 0;
	virtual void ReleaseShape(Zenith_PhysicsShapeHandle xShape) = 0;
	virtual Zenith_Result<Zenith_PhysicsShapeInfo, Zenith_PhysicsError> GetShapeInfo(
		Zenith_PhysicsShapeHandle xShape) const = 0;

	// Tools: the backend section bytes for a neutral cooked shape, written into xOut (arena-backed). Unsupported
	// (no PHYSICS_CAPABILITY_BACKEND_SHAPES) returns PHYSICS_ERROR_UNSUPPORTED.
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_PhysicsError> BakeShape(
		const Zenith_PhysicsCookedShapeView& xNeutral, Zenith_Arena& xOut) = 0;

	virtual Zenith_Result<Zenith_Owned<Zenith_PhysicsWorld>, Zenith_PhysicsError> CreateWorld(
		const Zenith_PhysicsWorldDesc& xDesc) = 0;

	virtual uint64 GetResidentBytes() const = 0;   // shapes, materials, service tables
};

/// @contract functions Zenith_PfnCreatePhysics
/// Ownership: A function pointer to a backend factory, passed by the generated composition to Zenith_Module_Physics_Start.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: As the factory it points to.
/// Version: since Z2 Stage 3. Exactly the declaration of Zenith2/Docs/Architecture/CompositionRoot.md (one parameter): no
///   backend needs the job system at construction; the PhysX dispatcher reads its worker count from the Zenith_JobContext
///   of each step (PHYS/Backends.md "PhysX step and the dispatcher").
/// @endcontract
using Zenith_PfnCreatePhysics = Zenith_Result<Zenith_Owned<Zenith_Physics>, Zenith_PhysicsError> (*)(
	const Zenith_PhysicsDesc& xDesc);
```

## Zenith_PhysicsShape header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsShape.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"

/// @contract enum Zenith_PhysicsShapeKind
/// Ownership: Plain enumeration; serialized as u8 (property 1 of Zenith.Physics.Collider, cooked headers).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value is PHYSICS_ERROR_INVALID_SHAPE.
/// Version: since Z2 Stage 3. Values never move.
/// @endcontract
enum Zenith_PhysicsShapeKind : uint8
{
	PHYSICS_SHAPE_KIND_SPHERE = 1,
	PHYSICS_SHAPE_KIND_BOX = 2,
	PHYSICS_SHAPE_KIND_CAPSULE = 3,          // along local +Y
	PHYSICS_SHAPE_KIND_CYLINDER = 4,         // along local +Y
	PHYSICS_SHAPE_KIND_CONVEX_HULL = 5,      // cooked
	PHYSICS_SHAPE_KIND_TRIANGLE_MESH = 6,    // cooked; static and kinematic bodies alone
	PHYSICS_SHAPE_KIND_HEIGHTFIELD = 7,      // cooked; static bodies alone
	PHYSICS_SHAPE_KIND_COMPOUND = 8,         // runtime (children) or cooked (static compound)
};

/// @contract value Zenith_PhysicsCompoundChild
/// Ownership: Plain value inside a Zenith_PhysicsShapeDesc's borrowed child span.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None.
/// Units: Metres; unit quaternion.
/// Errors: A null or stale child shape, a compound child, or a non-unit rotation is PHYSICS_ERROR_INVALID_SHAPE.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsCompoundChild
{
	Zenith_PhysicsShapeHandle m_xShape;
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	uint32 m_uUserKey = 0;                  // returned in hits as the child's key (the child entity's simulation-ID rank)
};

/// @contract value Zenith_PhysicsShapeDesc
/// Ownership: Plain value read by CreateShape for the call; the child span is borrowed for the call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres. Scale is a per-axis factor applied to the shape's local geometry.
/// Errors: A non-finite or non-positive dimension, a kind needing cooked data (CONVEX_HULL, TRIANGLE_MESH,
///   HEIGHTFIELD), more than uPHYSICS_MAX_COMPOUND_CHILDREN children or a zero scale component is
///   PHYSICS_ERROR_INVALID_SHAPE; cooked kinds come through CreateShapeFromCooked.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsShapeDesc
{
	Zenith_PhysicsShapeKind m_eKind = PHYSICS_SHAPE_KIND_SPHERE;
	float m_fRadius = 0.5f;                  // SPHERE, CAPSULE, CYLINDER
	float m_fHalfHeight = 0.5f;              // CAPSULE, CYLINDER: half the length of the cylindrical part along +Y
	Zenith_Vector3 m_xHalfExtents{ 0.5f, 0.5f, 0.5f };   // BOX
	Zenith_Vector3 m_xScale{ 1.0f, 1.0f, 1.0f };
	Zenith_PhysicsMaterialHandle m_xMaterial;            // primitives and runtime compounds; null = default
	Zenith_Span<const Zenith_PhysicsCompoundChild> m_axChildren;   // COMPOUND
};

/// @contract value Zenith_PhysicsCookedShapeView
/// Ownership: A borrowed view over the decoded sections of one .zcol file (PHYS/Formats.md "Collision shape file"),
///   valid for the call it is passed to.
/// Thread affinity: IMMUTABLE. Members non-const: the collision-file reader builds it field by field from the file's
///   sections.
/// Invalidation: Invalid after the decoded section memory is released.
/// Units: As the format.
/// Errors: The consumer validates every count and offset against the spans (PHYSICS_ERROR_COOKED_CORRUPT).
/// Version: since Z2 Stage 3. Format version 1.
/// @endcontract
struct Zenith_PhysicsCookedShapeView
{
	Zenith_PhysicsShapeKind m_eKind = PHYSICS_SHAPE_KIND_CONVEX_HULL;
	Zenith_ByteSpan m_xNeutral;              // section PHYSICS_SECTION_NEUTRAL_GEOMETRY, decoded
	Zenith_ByteSpan m_xBackend;              // section PHYSICS_SECTION_BACKEND_<kind> for this backend, or empty
	Zenith_PhysicsBackendKind m_eBackendKind = PHYSICS_BACKEND_NONE;
	uint64 m_ulBackendDataTag = 0;
	Zenith_Vector3 m_xScale{ 1.0f, 1.0f, 1.0f };
};

/// @contract value Zenith_PhysicsShapeInfo
/// Ownership: Plain value returned by GetShapeInfo.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: Metres, cubic metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsShapeInfo
{
	Zenith_PhysicsShapeKind m_eKind = PHYSICS_SHAPE_KIND_SPHERE;
	Zenith_AABB m_xLocalBounds;
	float m_fVolume = 0.0f;                  // 0 for TRIANGLE_MESH and HEIGHTFIELD
	Zenith_Vector3 m_xCentreOfMass;          // local, at uniform density
	uint32 m_uSubShapeCount = 1;            // compound children, mesh triangles, heightfield quads × 2
	bool m_bDynamicCapable = true;           // false for TRIANGLE_MESH, HEIGHTFIELD and compounds holding them
	float m_fBackendHeightError = 0.0f;      // HEIGHTFIELD: the largest |runtime − neutral| height over every sample, measured by
	                                         //   the backend at build (PHYS/Backends.md "Heightfield builds"); 0 on PhysX and other kinds
};
```

## Zenith_PhysicsMaterial header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsMaterial.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"

/// @contract enum Zenith_PhysicsCombineMode
/// Ownership: Plain enumeration; serialized as u8 in material documents and .zpmat files.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value is a cook error (COOK_ERROR_STEP_FAILED with PHYSICS_COOK_DIAG_MATERIAL_INVALID).
/// Version: since Z2 Stage 3. The order is the combine priority: of two bodies' modes the higher value applies.
/// @endcontract
enum Zenith_PhysicsCombineMode : uint8
{
	PHYSICS_COMBINE_AVERAGE = 0,
	PHYSICS_COMBINE_MIN = 1,
	PHYSICS_COMBINE_MULTIPLY = 2,
	PHYSICS_COMBINE_MAX = 3,
};

/// @contract enum Zenith_PhysicsInteractionKind
/// Ownership: Plain enumeration; property 4 of the Zenith.Physics.Interaction event.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_PhysicsInteractionKind : uint8
{
	PHYSICS_INTERACTION_IMPACT = 1,
	PHYSICS_INTERACTION_SLIDE_START = 2,
	PHYSICS_INTERACTION_SLIDE_STOP = 3,
};

/// @contract value Zenith_PhysicsInteractionRule
/// Ownership: Plain value inside a material's rule span.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: m/s for the thresholds; ticks for the cooldown.
/// Errors: None; validated by the material cook step.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsInteractionRule
{
	Zenith_StringID m_xOtherSurface;         // the other material's surface type; the reserved ID of "*" matches any
	Zenith_StringID m_xInteraction;          // the interaction name AUDIO and VFX map to effects, e.g. "Impact.WoodOnStone"
	float m_fImpactSpeed = 1.0f;             // minimum approach speed of an IMPACT
	float m_fSlideSpeed = 0.5f;              // minimum tangential speed that starts a slide (stops below half of it)
	uint16 m_uCooldownTicks = 6;            // per body pair and rule, between two IMPACTs
};

/// @contract value Zenith_PhysicsMaterialData
/// Ownership: Plain value; the interaction span borrows the material asset's storage.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: m_axInteractions is invalid when its asset is released.
/// Units: Friction and restitution are dimensionless coefficients (friction ≥ 0, restitution 0 … 1); density kg/m³.
/// Errors: A negative friction, a restitution outside [0, 1] or a non-positive density is PHYSICS_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsMaterialData
{
	float m_fFriction = 0.5f;
	float m_fRestitution = 0.0f;
	float m_fDensity = 1000.0f;
	Zenith_PhysicsCombineMode m_eFrictionCombine = PHYSICS_COMBINE_AVERAGE;
	Zenith_PhysicsCombineMode m_eRestitutionCombine = PHYSICS_COMBINE_MAX;
	Zenith_StringID m_xSurfaceType;          // "Default" when the material names none
	Zenith_Span<const Zenith_PhysicsInteractionRule> m_axInteractions;
};

/// @contract value Zenith_PhysicsCombinedMaterial
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: As Zenith_PhysicsMaterialData.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsCombinedMaterial
{
	const float m_fFriction = 0.5f;
	const float m_fRestitution = 0.0f;
};

/// @contract functions Zenith_PhysicsMaterialFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As Zenith_PhysicsMaterialData.
/// Errors: None.
/// Version: since Z2 Stage 3. Jolt's contact listener calls Zenith_CombinePhysicsMaterials; PhysX's native combine uses
///   the same priority order and formulas, so the combined coefficient of a pair is the same function on both
///   (PHYS/Backends.md "Material combination"; Test_PhysicsConformance_MaterialCombineMatchesFunction).
/// @endcontract
Zenith_PhysicsCombinedMaterial Zenith_CombinePhysicsMaterials(const Zenith_PhysicsMaterialData& xA,
	const Zenith_PhysicsMaterialData& xB);
// The rule a pair raises: A's first rule naming B's surface, else B's first rule naming A's, else A's first "*" rule,
// else B's; null when none. Order: rule order within a material, so the result is a pure function of the pair.
const Zenith_PhysicsInteractionRule* Zenith_FindPhysicsInteraction(const Zenith_PhysicsMaterialData& xA,
	const Zenith_PhysicsMaterialData& xB);

/// @contract functions Zenith_PhysicsMaterialNames
/// Ownership: Pure function.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None. An empty name gives 0, the default material's ID.
/// Version: since Z2 Stage 3. The physical-material ID of a physical-material name (PHYS/Formats.md "Physical-material
///   names"): h = the low 32 bits of Zenith_Hash64 over the name's UTF-8 bytes, and the ID is h, or 1 when h is 0, so 0
///   stays reserved for the default material (the empty name). ENV's cooked terrain header and foliage collision
///   records carry this value; the formula is part of the contract and never changes, so any area recomputes it from
///   the name with CORE's Zenith_Hash64 alone.
/// @endcontract
uint32 Zenith_MakePhysicsMaterialNameID(Zenith_StringView strName);

/// @contract type Zenith_PhysicsMaterialAsset
/// Ownership: The runtime object of asset kind Physics.Material (PHYS.md "Asset kinds"); created by the Physics module's
///   kind loader, owned by the asset registry, reached through Zenith_AssetHandle<Zenith_PhysicsMaterialAsset>.
/// Thread affinity: ANY_THREAD for the const members after activation.
/// Invalidation: Invalid after the registry retires the asset (deferred deletion).
/// Units: As Zenith_PhysicsMaterialData.
/// Errors: None after activation; load failures are the loader's (PHYS.md "Failure handling").
/// Version: since Z2 Stage 3. Format version 1 (PHYS/Formats.md "Material file").
/// @endcontract
class Zenith_PhysicsMaterialAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_PHYSICS_MATERIAL;
	const Zenith_PhysicsMaterialData& GetData() const;
	Zenith_PhysicsMaterialHandle GetHandle() const;   // the backend material of the process
};
```

<!-- elaboration -->
`ASSET_KIND_PHYSICS_MATERIAL` is a new row of `Zenith2/Docs/Design/ASSET.md` → "Asset kinds", added by the PHYS kind ticket with the code `uASSET_KIND_DEFINED_COUNT` names when it lands (row PH4; `Zenith2/Docs/Design/PHYS.md` → "Asset kinds").

## Zenith_PhysicsWorld header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsWorld.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"
#include "ZenithPhysics/Zenith_PhysicsErrors.h"
#include "ZenithPhysics/Zenith_PhysicsShape.h"
#include "ZenithPhysics/Zenith_PhysicsQuery.h"

class Zenith_Arena;
class Zenith_JobContext;
class Zenith_FaultInjector;

/// @contract value Zenith_PhysicsLayerTable
/// Ownership: Plain value: the parsed physics.layerNames / physics.layerCollisions configuration of PHYS.md
///   "Layer configuration".
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: None.
/// Units: None.
/// Errors: Built by Zenith_ParsePhysicsLayers (ComponentsAPI.md "Configuration keys"), which reports PHYSICS_ERROR_LAYER_CONFIG.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsLayerTable
{
	uint32 m_uLayerCount = 1;                                   // 1 … 32
	Zenith_PhysicsLayerMask m_auCollidesWith[uPHYSICS_MAX_LAYERS] = { uPHYSICS_LAYER_MASK_ALL };   // symmetric
};

/// @contract value Zenith_PhysicsWorldDesc
/// Ownership: Plain value copied by CreateWorld; pointers borrowed for the world's lifetime.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Counts, bytes, metres per second squared, metres, seconds.
/// Errors: A zero capacity, a capacity above its uPHYSICS_MAX_* limit, a world tag of 0 or above uPHYSICS_MAX_WORLD_TAG,
///   iteration counts outside their key ranges or a non-finite gravity is PHYSICS_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3. Every simulation-affecting member comes from a configuration key flagged
///   m_bSimulationAffecting, so the replay header records it.
/// @endcontract
struct Zenith_PhysicsWorldDesc
{
	uint16 m_uWorldTag = 0;                          // the ECS world's tag (1 … 4,095); stamped into every handle
	uint32 m_uMaxBodies = 65536;                     // physics.maxBodies
	uint32 m_uMaxBodyPairs = 65536;                  // physics.maxBodyPairs
	uint32 m_uMaxContactConstraints = 32768;         // physics.maxContactConstraints
	uint32 m_uMaxConstraints = 16384;                // physics.maxConstraints
	uint64 m_ulStepScratchBytes = 16ull << 20;       // physics.stepScratchBytes
	Zenith_Vector3 m_xGravity{ 0.0f, -9.81f, 0.0f };  // physics.gravityX/Y/Z
	uint32 m_uVelocityIterations = 10;               // physics.solverVelocityIterations
	uint32 m_uPositionIterations = 2;                // physics.solverPositionIterations
	float m_fSpeculativeContactDistance = 0.02f;      // physics.speculativeContactDistance
	float m_fPenetrationSlop = 0.02f;                 // physics.penetrationSlop
	float m_fMaxLinearSpeed = 500.0f;                 // physics.maxLinearSpeed
	float m_fSleepSeconds = 0.5f;                     // physics.sleepSeconds
	float m_fSleepVelocity = 0.03f;                   // physics.sleepVelocity
	Zenith_PhysicsLayerTable m_xLayers;
	uint32 m_uParallelLeaves = 0;                    // PhysX drain leaves (0: the step's Zenith_JobContext::GetWorkerCount()); the
	                                                  //   conditional Jolt job system's fixed concurrency W_J (0: 8, PHYS/Backends.md
	                                                  //   "Conditional Jolt parallel step")
	Zenith_FaultInjector* m_pxFaults = nullptr;
};

/// @contract value Zenith_PhysicsBodyDesc
/// Ownership: Plain value read by CreateBody for the call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres, kilograms, m/s, rad/s, unit quaternion.
/// Errors: A null or stale shape, a non-dynamic-capable shape on a DYNAMIC body (PHYSICS_ERROR_MOTION_TYPE), a layer
///   ≥ the table's count, a negative mass or damping, a non-unit rotation, a non-finite value or an m_uRagdollPart above
///   uPHYSICS_MAX_RAGDOLL_PARTS is PHYSICS_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsBodyDesc
{
	Zenith_PhysicsShapeHandle m_xShape;
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	Zenith_Vector3 m_xLinearVelocity;
	Zenith_Vector3 m_xAngularVelocity;
	Zenith_PhysicsMotionType m_eMotionType = PHYSICS_MOTION_TYPE_DYNAMIC;
	Zenith_PhysicsMotionQuality m_eMotionQuality = PHYSICS_MOTION_QUALITY_DISCRETE;
	uint8 m_uLayer = 0;
	uint32 m_uCollisionGroup = 0;
	uint32 m_uFlags = PHYSICS_BODY_FLAG_ALLOW_SLEEPING;
	float m_fMass = 0.0f;                             // 0: shape volume × material density
	float m_fLinearDamping = 0.05f;                   // 1/s
	float m_fAngularDamping = 0.05f;                  // 1/s
	float m_fGravityFactor = 1.0f;
	float m_fMaxLinearSpeed = 0.0f;                   // 0: the world's
	Zenith_Vector3 m_xCentreOfMassOffset;
	Zenith_PhysicsMaterialHandle m_xMaterialOverride; // non-null: every sub-shape uses it
	uint64 m_ulUserData = 0;                         // the owning entity's stable simulation ID (a ragdoll part: its character's)
	uint8 m_uRagdollPart = 0;                        // 0: not a ragdoll part; 1 … 64: part index + 1 (PHYS/Simulation.md "Ragdoll part
	                                                  //   identity"); kept per body slot and stamped into contact records
};

/// @contract value Zenith_PhysicsBodyState
/// Ownership: Plain value filled by GetBodyState.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: Metres, unit quaternion, m/s, rad/s.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsBodyState
{
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	Zenith_Vector3 m_xLinearVelocity;
	Zenith_Vector3 m_xAngularVelocity;
	Zenith_AABB m_xWorldBounds;
	Zenith_PhysicsMotionType m_eMotionType = PHYSICS_MOTION_TYPE_STATIC;
	bool m_bSleeping = false;
};

/// @contract enum Zenith_PhysicsConstraintKind
/// Ownership: Plain enumeration; property 1 of Zenith.Physics.Joint.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value is PHYSICS_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3. Values never move.
/// @endcontract
enum Zenith_PhysicsConstraintKind : uint8
{
	PHYSICS_CONSTRAINT_FIXED = 1,
	PHYSICS_CONSTRAINT_POINT = 2,          // ball and socket
	PHYSICS_CONSTRAINT_HINGE = 3,          // rotation about frame X
	PHYSICS_CONSTRAINT_SLIDER = 4,         // translation along frame X
	PHYSICS_CONSTRAINT_DISTANCE = 5,       // distance between the frame origins
	PHYSICS_CONSTRAINT_CONE = 6,           // point plus a cone limit about frame X
	PHYSICS_CONSTRAINT_SWING_TWIST = 7,    // twist about frame X, swing limits about Y and Z (ragdoll joints)
	PHYSICS_CONSTRAINT_SIX_DOF = 8,        // per-axis translation and rotation limits
};

/// @contract enum Zenith_PhysicsMotorMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_PhysicsMotorMode : uint8
{
	PHYSICS_MOTOR_OFF = 0,
	PHYSICS_MOTOR_VELOCITY = 1,
	PHYSICS_MOTOR_POSITION = 2,
};

/// @contract value Zenith_PhysicsConstraintDesc
/// Ownership: Plain value read by CreateConstraint for the call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres and radians for limits and targets; newtons and newton-metres for the motor and break thresholds.
///   Frames are each body's local space (a null m_xBodyB means world space for frame B).
/// Errors: A null, stale or foreign body A, body A equal to body B, min > max on any limit, a limit outside its kind's
///   range (PHYS/Simulation.md "Constraint kinds") or a non-unit frame rotation is PHYSICS_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsConstraintDesc
{
	Zenith_PhysicsConstraintKind m_eKind = PHYSICS_CONSTRAINT_FIXED;
	Zenith_PhysicsBodyHandle m_xBodyA;
	Zenith_PhysicsBodyHandle m_xBodyB;                // null: attached to the world
	Zenith_Vector3 m_xFrameAPosition;
	Zenith_Quaternion m_xFrameARotation;
	Zenith_Vector3 m_xFrameBPosition;
	Zenith_Quaternion m_xFrameBRotation;
	Zenith_Vector3 m_xLinearLimitMin;                 // per kind (PHYS/Simulation.md "Constraint kinds")
	Zenith_Vector3 m_xLinearLimitMax;
	Zenith_Vector3 m_xAngularLimitMin;
	Zenith_Vector3 m_xAngularLimitMax;
	Zenith_PhysicsMotorMode m_eMotorMode = PHYSICS_MOTOR_OFF;
	Zenith_Vector3 m_xMotorTargetVelocity;            // m/s or rad/s per driven axis
	Zenith_Vector3 m_xMotorTargetPosition;            // metres or radians per driven axis
	Zenith_Quaternion m_xMotorTargetRotation;         // SWING_TWIST and SIX_DOF position motors
	float m_fMotorMaxForce = 0.0f;
	float m_fMotorMaxTorque = 0.0f;
	float m_fMotorFrequency = 10.0f;                  // Hz, position motors
	float m_fMotorDamping = 1.0f;                     // damping ratio, position motors
	bool m_bCollideConnected = false;                 // false: the two bodies never collide with each other (PHYS/Backends.md
	                                                  //   "Constraint mapping"); with m_xBodyB null it has no effect
	uint32 m_uPriority = 0;                          // the constraint's creation rank; unique per world (determinism)
};

/// @contract value Zenith_PhysicsConstraintImpulse
/// Ownership: Plain value filled by GetConstraintImpulse.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: N·s and N·m·s accumulated over the last StepOnce.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsConstraintImpulse
{
	float m_fLinear = 0.0f;                           // magnitude
	float m_fAngular = 0.0f;
};

/// @contract enum Zenith_PhysicsContactEventKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_PhysicsContactEventKind : uint8
{
	PHYSICS_CONTACT_ADDED = 1,      // a sub-shape pair began touching (or, flag bit 0, began overlapping a trigger)
	PHYSICS_CONTACT_REMOVED = 2,    // a sub-shape pair stopped touching or overlapping
	PHYSICS_CONTACT_PERSISTED = 3,  // a touching pair's tangential speed, for interaction detection; never changes a pair's count
};

/// @contract value Zenith_PhysicsContactRecord
/// Ownership: Plain value appended by a backend to the step's Zenith_PhysicsContactSink.
/// Thread affinity: IMMUTABLE. Members non-const: the contact sink appends records to its arrays and sorts them in
///   place.
/// Invalidation: None.
/// Units: World metres; m/s; unit normal pointing from body A to body B.
/// Errors: None.
/// Version: since Z2 Stage 3. 64 bytes.
/// @endcontract
struct Zenith_PhysicsContactRecord
{
	uint64 m_ulUserDataA = 0;                        // body A: the body of the lower (user data, part code) pair
	uint64 m_ulUserDataB = 0;
	uint32 m_uSubShapeA = 0;
	uint32 m_uSubShapeB = 0;
	Zenith_Vector3 m_xPoint;                          // ADDED, PERSISTED: contact point 0 of the backend's manifold, on body A's
	                                                  //   surface (Jolt GetWorldSpaceContactPointOn1(0), PhysX extracted point 0); REMOVED: zero
	Zenith_Vector3 m_xNormal;
	float m_fApproachSpeed = 0.0f;                    // ADDED: −(v_B − v_A)·n at the point, ≥ 0
	float m_fTangentSpeed = 0.0f;                     // ADDED and persisted pairs: |tangential relative velocity|
	uint16 m_uSubstep = 0;
	Zenith_PhysicsContactEventKind m_eKind = PHYSICS_CONTACT_ADDED;
	uint8 m_uFlags = 0;                              // bit 0: a trigger pair; bit 1 (REMOVED only): "sleep", the removal of a contact
	                                                  //   of two live bodies at least one of which is non-static and asleep (Jolt; PHYS/Simulation.md
	                                                  //   "Sleeping"); other bits 0. Never a record of a destroyed body: the backend drops those.
	uint8 m_uPartA = 0;                              // body A's m_uRagdollPart (0: not a ragdoll part)
	uint8 m_uPartB = 0;                              // body B's
};

/// @contract type Zenith_PhysicsContactSink
/// Ownership: Owned by the Physics module's per-world state; passed by reference to StepOnce; storage from the world's
///   step arena, capacity physics.maxContactRecordsPerTick.
/// Thread affinity: EXTERNALLY_SYNCHRONISED; the backend provides the synchronisation. Jolt with the single-threaded job
///   system calls Append and CountStaleRemoved from the stepping thread alone; Jolt with PHYSICS_CAPABILITY_PARALLEL_STEP
///   calls Append from the contact callbacks on the drain leaves and the waiter, always under its listener's mutex at
///   lock level 530 (uLOCK_LEVEL_PHYSICS_CONTACT_SINK; PHYS/Backends.md "Conditional Jolt parallel step"), and resolves its
///   removed-contact staging on the stepping thread after Update; the PhysX backend calls both from the simulation event
///   callbacks that fetchResults runs inside ResumeStep, on the one thread that runs the step's final continuation
///   (PHYS/Backends.md "PhysX step and the dispatcher", "PhysX contact reporting"). The module reads it (SortAndGet,
///   GetDroppedCount, GetStaleRemovedCount) and clears it on the step node's thread, never during a StepOnce.
/// Invalidation: Cleared by the module at the start of each tick.
/// Units: Records.
/// Errors: Append never fails: when full, the record with the greatest sort key (the record key of PHYS/Simulation.md
///   "Event emission" step 1, user data and part codes first) among the stored records and the new one is dropped, so the stored set is the capacity's lowest keys whatever the arrival order (counter
///   physics.contacts.dropped_delta).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_PhysicsContactSink
{
public:
	void Append(const Zenith_PhysicsContactRecord& xRecord);
	Zenith_Span<const Zenith_PhysicsContactRecord> SortAndGet();   // ascending sort key
	uint32 GetDroppedCount() const;
	// A backend reports a removal of a body that is no longer its slot's live occupant here instead of appending it
	// (PHYS/Backends.md "Body identity on both backends"); the module publishes the count as physics.contacts.stale_removed_delta.
	void CountStaleRemoved();
	uint32 GetStaleRemovedCount() const;
	void Clear();                                                                // both counts included
};

/// @contract value Zenith_PhysicsStepStats
/// Ownership: Plain value returned by StepOnce.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsStepStats
{
	uint32 m_uActiveBodies = 0;
	uint32 m_uBodyPairs = 0;
	uint32 m_uContactConstraints = 0;
	uint32 m_uIslands = 0;
	bool m_bBodyPairsFull = false;                    // the backend reached m_uMaxBodyPairs (pairs beyond it ignored)
	bool m_bContactConstraintsFull = false;
	bool m_bPending = false;                          // work was scheduled on xJobs: call ResumeStep from a continuation
	uint16 m_uWaves = 0;                             // drain waves the substep took (PhysX; 0 on Jolt)
};

/// @contract interface Zenith_PhysicsWorld
/// Ownership: Created by Zenith_Physics::CreateWorld as a Zenith_Owned<Zenith_PhysicsWorld>; one per ECS world, owned by
///   the Physics module's world state and destroyed in DetachWorld before the ECS world; every body and constraint is
///   destroyed with it. Storage from the root heap at creation; the step scratch arena is reserved at creation.
/// Thread affinity: PHASE(Physics) for every mutating member, StepOnce and ResumeStep (the module's phase-5 node and its sub-jobs,
///   and the world's sync-point node, which never overlaps a phase node of the same world), and the main thread out of tick
///   during a restore (the save participant's RestoreSection and IsRestoreReady, while no frame of the world runs). The const query members
///   (CastRay, CastShape, Overlap, CollideShape, GetBodyState, GetConstraintImpulse) are ANY_THREAD while no mutating
///   member or StepOnce runs; the frame graph guarantees it through the declared access to Zenith.Physics.World.
///   SaveState and RestoreState are OWNER_THREAD on the phase-11 node and on the main thread out of tick.
/// Invalidation: Destroying a body invalidates its handle and every constraint attached to it; DestroyConstraint
///   invalidates the constraint handle; nothing else invalidates.
/// Units: Metres, kilograms, seconds, radians, newtons.
/// Errors: Zenith_Result<…, Zenith_PhysicsError> as each member states; queries return false for an invalid descriptor
///   (counted physics.query.invalid_delta) and never assert; StepOnce returns PHYSICS_ERROR_STEP_FAILED only when the
///   backend reports an internal failure (logged with the backend's code), and the world stays usable.
/// Reentrancy: NONE. The contact sink and the query collector are concrete types the backend appends to; no member calls
///   out of the backend during StepOnce except Zenith_PhysicsContactSink::Append and the pre-substep hook of
///   PHYS/Simulation.md "Substep loop", which the caller runs between StepOnce calls, not inside one.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_PhysicsWorld
{
public:
	virtual ~Zenith_PhysicsWorld() = default;

	// Bodies. CreateBody allocates the lowest free index and that slot's next generation (PHYS/Simulation.md "Slot
	// allocation"), so the handle depends on the live set and the per-slot generations alone; CreateBodyWithHandle
	// re-creates a body with a recorded handle (restore, after RestoreSlotTables) and fails with
	// PHYSICS_ERROR_STATE_MISMATCH when the index is live or its restored generation differs.
	virtual Zenith_Result<Zenith_PhysicsBodyHandle, Zenith_PhysicsError> CreateBody(
		const Zenith_PhysicsBodyDesc& xDesc) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> CreateBodyWithHandle(Zenith_PhysicsBodyHandle xHandle,
		const Zenith_PhysicsBodyDesc& xDesc) = 0;
	// Several bodies in one call, in span order (one broadphase insertion; used by streaming and restore).
	virtual Zenith_Result<uint32, Zenith_PhysicsError> CreateBodies(Zenith_Span<const Zenith_PhysicsBodyDesc> axDescs,
		Zenith_Span<Zenith_PhysicsBodyHandle> axHandlesOut) = 0;           // returns the count created before a failure
	virtual void DestroyBody(Zenith_PhysicsBodyHandle xBody) = 0;          // stale: counted, ignored
	virtual Zenith_Result<void, Zenith_PhysicsError> SetBodyTransform(Zenith_PhysicsBodyHandle xBody,
		const Zenith_Vector3& xPosition, const Zenith_Quaternion& xRotation, Zenith_PhysicsActivation eActivation) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> SetBodyVelocity(Zenith_PhysicsBodyHandle xBody,
		const Zenith_Vector3& xLinear, const Zenith_Vector3& xAngular) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> SetMotionType(Zenith_PhysicsBodyHandle xBody,
		Zenith_PhysicsMotionType eMotionType, Zenith_PhysicsActivation eActivation) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> SetBodyShape(Zenith_PhysicsBodyHandle xBody,
		Zenith_PhysicsShapeHandle xShape, bool bUpdateMass) = 0;
	// Layer, collision group and the filtering flags (TRIGGER, QUERY_ONLY, IGNORE_SAME_GROUP, REPORT_CONTACTS) at once;
	// used by region freeze and ragdoll enter (QUERY_ONLY on and off).
	virtual Zenith_Result<void, Zenith_PhysicsError> SetBodyFiltering(Zenith_PhysicsBodyHandle xBody,
		uint8 uLayer, uint32 uCollisionGroup, uint32 uFlags) = 0;
	// The body pairs that never collide (constraints with CollideConnected false): each (low index << 32) | high index,
	// sorted ascending, copied by the backend; a mutating member (PHASE(Physics)), called only by Physics.Step's ingest, step 12,
	// before the tick's first StepOnce (PHYS/Simulation.md "Excluded pairs maintenance"). Jolt's group filter reads it during
	// StepOnce alone; PhysX accepts it unused (its joint flag excludes the same pairs). An unsorted span is PHYSICS_ERROR_INVALID_DESC.
	virtual Zenith_Result<void, Zenith_PhysicsError> SetExcludedPairs(Zenith_Span<const uint64> aulPairs) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> AddForce(Zenith_PhysicsBodyHandle xBody,
		const Zenith_Vector3& xForce, const Zenith_Vector3* pxWorldPoint) = 0;        // null point: centre of mass
	virtual Zenith_Result<void, Zenith_PhysicsError> AddTorque(Zenith_PhysicsBodyHandle xBody,
		const Zenith_Vector3& xTorque) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> AddImpulse(Zenith_PhysicsBodyHandle xBody,
		const Zenith_Vector3& xImpulse, const Zenith_Vector3* pxWorldPoint) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> AddAngularImpulse(Zenith_PhysicsBodyHandle xBody,
		const Zenith_Vector3& xAngularImpulse) = 0;
	// Kinematic bodies: the velocity that reaches the target in fDeltaSeconds; static or dynamic is PHYSICS_ERROR_MOTION_TYPE.
	virtual Zenith_Result<void, Zenith_PhysicsError> MoveKinematic(Zenith_PhysicsBodyHandle xBody,
		const Zenith_Vector3& xTargetPosition, const Zenith_Quaternion& xTargetRotation, float fDeltaSeconds) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> SetSleeping(Zenith_PhysicsBodyHandle xBody, bool bSleeping) = 0;
	virtual Zenith_Result<Zenith_PhysicsBodyState, Zenith_PhysicsError> GetBodyState(
		Zenith_PhysicsBodyHandle xBody) const = 0;
	virtual uint32 GetBodyCount() const = 0;

	// Constraints.
	virtual Zenith_Result<Zenith_PhysicsConstraintHandle, Zenith_PhysicsError> CreateConstraint(
		const Zenith_PhysicsConstraintDesc& xDesc) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> CreateConstraintWithHandle(
		Zenith_PhysicsConstraintHandle xHandle, const Zenith_PhysicsConstraintDesc& xDesc) = 0;
	virtual void DestroyConstraint(Zenith_PhysicsConstraintHandle xConstraint) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> SetConstraintEnabled(Zenith_PhysicsConstraintHandle xConstraint,
		bool bEnabled) = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> SetConstraintMotor(Zenith_PhysicsConstraintHandle xConstraint,
		Zenith_PhysicsMotorMode eMode, const Zenith_Vector3& xTargetVelocity, const Zenith_Vector3& xTargetPosition,
		const Zenith_Quaternion& xTargetRotation) = 0;
	virtual Zenith_Result<Zenith_PhysicsConstraintImpulse, Zenith_PhysicsError> GetConstraintImpulse(
		Zenith_PhysicsConstraintHandle xConstraint) const = 0;

	// Simulation: one substep of fDeltaSeconds (0 < fDeltaSeconds ≤ fPHYSICS_MAX_STEP_SECONDS). The Jolt backend runs
	// it on the calling thread and never returns m_bPending. The PhysX backend starts the substep and schedules one drain
	// wave through xJobs.ParallelFor (PHYS/Backends.md "PhysX step and the dispatcher") and returns m_bPending = true;
	// the caller then registers a continuation (xJobs.Continue) that calls ResumeStep, which either schedules the next
	// wave and returns m_bPending = true again, or finishes the substep (contacts appended, stats final) and returns
	// m_bPending = false. No member of the world may be called between StepOnce and the ResumeStep that completes it.
	virtual Zenith_Result<Zenith_PhysicsStepStats, Zenith_PhysicsError> StepOnce(float fDeltaSeconds,
		Zenith_JobContext& xJobs, Zenith_PhysicsContactSink& xContacts) = 0;
	virtual Zenith_Result<Zenith_PhysicsStepStats, Zenith_PhysicsError> ResumeStep(
		Zenith_JobContext& xJobs, Zenith_PhysicsContactSink& xContacts) = 0;   // ZENITH_ASSERT: a step is pending
	virtual void SetGravity(const Zenith_Vector3& xGravity) = 0;
	// Every body origin p is set to the binary32 difference p − xShift, and every world-anchored constraint frame moves by
	// the same amount; contact caches are reset. The backend's internal centre-of-mass position may differ from the origin
	// plus the rotated offset by one rounding, so an origin read back is within one binary32 rounding of p − xShift; no module
	// comparison depends on backend bits after a shift.
	// Precondition (ZENITH_ASSERT, the module validates first): every component of xShift is a finite integer number of
	// metres. At most 2^24 m (every shift STREAM records) it is SCENE's S exactly, so p − xShift equals SCENE's
	// (float)((double)p − S); a larger component is the module's one-rounding conversion of a defective S
	// (PHYS/Simulation.md "Origin shift" step 1), and the origins then match SCENE's within one rounding. Called on the
	// sync-point node by the module's origin-shift listener (PHYS/Simulation.md "Origin shift").
	virtual void ShiftOrigin(const Zenith_Vector3& xShift) = 0;

	// Queries: deterministic through the collector (Zenith_PhysicsQuery.h). They read the state after the last StepOnce
	// or mutation.
	virtual void CastRay(const Zenith_PhysicsRayDesc& xDesc, Zenith_PhysicsHitCollector& xHits) const = 0;
	virtual void CastShape(const Zenith_PhysicsShapeCastDesc& xDesc, Zenith_PhysicsHitCollector& xHits) const = 0;
	virtual void Overlap(const Zenith_PhysicsOverlapDesc& xDesc, Zenith_PhysicsHitCollector& xHits) const = 0;
	virtual void CollideShape(const Zenith_PhysicsCollideDesc& xDesc, Zenith_PhysicsHitCollector& xHits) const = 0;

	// State: EXACT_STATE_RESTORE backends write and read an opaque blob of every simulation-internal value (contact
	// caches, warm-start impulses, sleep timers); others return PHYSICS_ERROR_UNSUPPORTED from both.
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_PhysicsError> SaveState(Zenith_Arena& xOut) const = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> RestoreState(Zenith_ByteSpan xState) = 0;
	// Slot tables (every backend): the body and constraint tables' high-water marks and per-slot generations, the bytes of
	// PHYS/Formats.md "Save section layout" block "slot tables". RestoreSlotTables is valid only on a world with no live
	// body or constraint (PHYSICS_ERROR_STATE_MISMATCH otherwise, and for a high-water mark above the capacity).
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_PhysicsError> SaveSlotTables(Zenith_Arena& xOut) const = 0;
	virtual Zenith_Result<void, Zenith_PhysicsError> RestoreSlotTables(Zenith_ByteSpan xTables) = 0;

	virtual uint64 GetResidentBytes() const = 0;
};
```

## Zenith_PhysicsQuery header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_PhysicsQuery.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"
#include "ZenithPhysics/Zenith_PhysicsShape.h"

/// @contract value Zenith_PhysicsQueryFilter
/// Ownership: Plain value; the ignore span is borrowed for the query.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Class gates: a trigger body is visible exactly when m_bIncludeTriggers, a query-only body
///   exactly when m_bIncludeQueryOnly, any other body when m_bIncludeStatic (static) or m_bIncludeDynamic (dynamic,
///   kinematic) is set; then the layer mask, m_uIgnoreGroup and the ignore list (PHYS/Simulation.md "Query execution").
/// @endcontract
struct Zenith_PhysicsQueryFilter
{
	Zenith_PhysicsLayerMask m_uLayerMask = uPHYSICS_LAYER_MASK_ALL;
	bool m_bIncludeTriggers = false;          // triggers are invisible to queries unless asked for
	bool m_bIncludeStatic = true;
	bool m_bIncludeDynamic = true;            // dynamic and kinematic bodies
	bool m_bIncludeQueryOnly = true;          // query-only bodies (PHYSICS_BODY_FLAG_QUERY_ONLY) are visible exactly when set
	uint32 m_uIgnoreGroup = 0;               // non-zero: bodies of this collision group are skipped (both backends,
	                                          //   PHYS/Backends.md "Jolt queries", "PhysX scene query filter")
	Zenith_Span<const uint64> m_aulIgnoreUserData;   // sorted ascending; bodies whose user data is listed are skipped
};

/// @contract value Zenith_PhysicsRayDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres; m_xDirection unit.
/// Errors: A non-unit direction (|len − 1| > 1e-4), a negative or non-finite distance is an invalid query.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsRayDesc
{
	Zenith_Vector3 m_xOrigin;
	Zenith_Vector3 m_xDirection{ 0.0f, -1.0f, 0.0f };
	float m_fMaxDistance = 1000.0f;
	bool m_bBackFaces = false;                // triangle back faces and rays starting inside convex shapes
	Zenith_PhysicsQueryFilter m_xFilter;
};

/// @contract value Zenith_PhysicsShapeCastDesc
/// Ownership: Plain value; the shape is borrowed for the query.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres; unit quaternion; m_xDirection unit.
/// Errors: As Zenith_PhysicsRayDesc, plus a null or stale shape or a compound, mesh or heightfield cast shape.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsShapeCastDesc
{
	Zenith_PhysicsShapeHandle m_xShape;       // a convex shape: sphere, box, capsule, cylinder, convex hull
	Zenith_Vector3 m_xStartPosition;
	Zenith_Quaternion m_xRotation;
	Zenith_Vector3 m_xDirection{ 0.0f, -1.0f, 0.0f };
	float m_fMaxDistance = 1.0f;
	bool m_bReportInitialOverlap = true;      // hits at distance 0 for shapes already overlapping at the start
	Zenith_PhysicsQueryFilter m_xFilter;
};

/// @contract value Zenith_PhysicsOverlapDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres; unit quaternion.
/// Errors: As Zenith_PhysicsShapeCastDesc.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsOverlapDesc
{
	Zenith_PhysicsShapeHandle m_xShape;
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	Zenith_PhysicsQueryFilter m_xFilter;
};

/// @contract value Zenith_PhysicsCollideDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres; unit quaternion.
/// Errors: As Zenith_PhysicsShapeCastDesc; a negative separation is invalid.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PhysicsCollideDesc
{
	Zenith_PhysicsShapeHandle m_xShape;
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	float m_fMaxSeparation = 0.0f;            // contacts within this distance are reported with negative depth
	Zenith_PhysicsQueryFilter m_xFilter;
};

/// @contract value Zenith_PhysicsHit
/// Ownership: Plain value inside a collector.
/// Thread affinity: IMMUTABLE. Members non-const: the hit collector replaces and reorders its kept hits in place.
/// Invalidation: None.
/// Units: World metres; m_fFraction in [0, 1] of the query distance; m_fDepth metres (CollideShape: penetration > 0).
/// Errors: None.
/// Version: since Z2 Stage 3. 64 bytes.
/// @endcontract
struct Zenith_PhysicsHit
{
	uint64 m_ulUserData = 0;                 // the hit body's user data (simulation ID); 0 for bodies without one
	Zenith_PhysicsBodyHandle m_xBody;
	uint32 m_uSubShape = 0;                  // the backend's sub-shape key, stable for a given cooked shape and backend
	uint32 m_uMaterialPaletteIndex = 0;
	Zenith_Vector3 m_xPosition;
	Zenith_Vector3 m_xNormal;                 // surface normal at the hit, unit, facing the query
	float m_fFraction = 0.0f;
	float m_fDistance = 0.0f;
	float m_fDepth = 0.0f;
};

/// @contract enum Zenith_PhysicsCollectMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_PhysicsCollectMode : uint8
{
	PHYSICS_COLLECT_CLOSEST = 1,              // the lowest key
	PHYSICS_COLLECT_ALL = 2,                  // the K lowest keys, K = the storage span's size
	PHYSICS_COLLECT_ANY = 3,                  // existence only; the stored hit is the lowest key seen
};

/// @contract container Zenith_PhysicsHitCollector
/// Ownership: Constructed by the caller over caller storage (a span of Zenith_PhysicsHit, 1 … uPHYSICS_MAX_QUERY_HITS);
///   never allocates.
/// Thread affinity: OWNER_THREAD. One query on one thread fills it.
/// Invalidation: Get's span is invalid at the next Add or Reset.
/// Units: Hits.
/// Errors: Add never fails; over capacity the hit with the greatest key is discarded (counted in GetDiscardedCount).
/// Alignment: The storage span's (alignof(Zenith_PhysicsHit) = 8).
/// Overflow: At most the span's size; the span size is checked in the constructor (ZENITH_ASSERT 1 … 256).
/// Allocation failure: Never allocates.
/// Complexity: Add O(log K) (a binary max-heap over the stored keys); Get O(K log K) once (heap sort), then O(1).
/// Version: since Z2 Stage 3. The key is (m_fDistance as its IEEE-754 bits, m_ulUserData, m_uSubShape, body index),
///   ascending; ±0 and NaN never enter (a NaN distance is discarded and counted), so the result is a pure function of
///   the set of candidate hits, independent of the order the backend reports them.
/// @endcontract
class Zenith_PhysicsHitCollector
{
public:
	Zenith_PhysicsHitCollector(Zenith_PhysicsCollectMode eMode, Zenith_Span<Zenith_PhysicsHit> axStorage);
	void Add(const Zenith_PhysicsHit& xHit);
	bool ShouldEarlyOut() const;         // ANY mode after the first hit
	float GetMaxDistance() const;        // CLOSEST: the current best, for backend culling
	Zenith_Span<const Zenith_PhysicsHit> Get();   // ascending key
	bool HasHit() const;
	uint32 GetDiscardedCount() const;
	void Reset();
private:
	Zenith_Span<Zenith_PhysicsHit> m_axStorage;
	uint32 m_uCount = 0;
	uint32 m_uDiscarded = 0;
	Zenith_PhysicsCollectMode m_eMode = PHYSICS_COLLECT_CLOSEST;
	bool m_bSorted = false;
};

/// @contract functions Zenith_PhysicsHitOrdering
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
bool Zenith_PhysicsHitLess(const Zenith_PhysicsHit& xA, const Zenith_PhysicsHit& xB);
```

<!-- elaboration -->
The collector keys on the distance's bit pattern rather than its value only after the backend's distance is canonicalised: `+0.0f` is stored for any zero, and a distance is the backend's own float, never recomputed. Ties at exactly equal distance resolve to the lower simulation ID, which is the determinism rule GAME's `Zenith_CollisionQuery` contract names (`Zenith2/Docs/Design/GAME/Cameras.md` → "Collision query interface").

## Backend factory headers

```cpp
// Zenith2/Engine/ZenithPhysics_Jolt/Zenith_Physics_Jolt.h
#pragma once
#include "ZenithPhysics/Zenith_Physics.h"

/// @contract functions Zenith_PhysicsJoltFactory
/// Ownership: Returns the process's Jolt Zenith_Physics as a Zenith_Owned; the caller owns it.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: PHYSICS_ERROR_INVALID_DESC, PHYSICS_ERROR_OUT_OF_MEMORY, PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE (a Jolt
///   instance is alive in the process: Jolt keeps its factory and type registry in process-global data),
///   PHYSICS_ERROR_BACKEND_INIT; each step unwinds what it constructed (PHYS/Backends.md "Jolt construction steps").
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_Physics>, Zenith_PhysicsError> Zenith_CreatePhysics_Jolt(
	const Zenith_PhysicsDesc& xDesc);
```

```cpp
// Zenith2/Engine/ZenithPhysics_PhysX/Zenith_Physics_PhysX.h
#pragma once
#include "ZenithPhysics/Zenith_Physics.h"

/// @contract functions Zenith_PhysicsPhysXFactory
/// Ownership: Returns the process's PhysX Zenith_Physics as a Zenith_Owned; the caller owns it.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: As Zenith_CreatePhysics_Jolt (PhysX allows one foundation per process); PHYS/Backends.md "PhysX
///   construction steps".
/// Version: since Z2 Stage 3. Available on Android only when D-SEL-PHYSX-ANDROID (open, Stage 1) admits it.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_Physics>, Zenith_PhysicsError> Zenith_CreatePhysics_PhysX(
	const Zenith_PhysicsDesc& xDesc);
```

These are the only public headers of the backend libraries ([Z2-N-CONTRACT-039]); the composition root's generated file, conformance executables, benchmarks and tools include them, and nothing else does ([Z2-N-CONTRACT-042], [Z2-N-CONTRACT-050]).

## Zenith_Module_Physics header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_Module_Physics.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"
#include "ZenithPhysics/Zenith_Physics.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module_Physics
/// Ownership: The two module entry points the generated composition calls (Zenith2/Docs/Architecture.md "Engine
///   modules and game projects"); Start returns the module instance ZenithRuntime owns.
/// Thread affinity: MAIN_THREAD; RegisterTypes at startup step 6, Start at startup step 15.
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION (a type, configuration key, command kind, binding or kind-loader registration was
///   refused, or the layer configuration is invalid, logged first), MODULE_ERROR_BACKEND_CREATE (the backend factory
///   failed; its Zenith_PhysicsError is logged), MODULE_ERROR_OUT_OF_MEMORY. Every step unwinds the steps before it.
/// Version: since Z2 Stage 3. The step tables of PHYS.md "Module start and world attach".
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Physics_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Physics_Start(
	Zenith_ModuleStartContext& xContext, Zenith_PfnCreatePhysics pfnCreateBackend);
```

## Zenith_CollisionShapeAsset header

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_CollisionShapeAsset.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithPhysics/Zenith_PhysicsTypes.h"
#include "ZenithPhysics/Zenith_PhysicsShape.h"
#include "ZenithPhysics/Zenith_PhysicsMaterial.h"

/// @contract type Zenith_CollisionShapeAsset
/// Ownership: The runtime object of asset kind 16, Physics.CollisionShape (Zenith2/Docs/Design/ASSET.md "Asset kinds");
///   created by the Physics module's kind loader on an asset-loader job, owned by the asset registry. It holds one
///   reference to its backend shape and a handle to each material of its palette, all released when the registry
///   retires it.
/// Thread affinity: ANY_THREAD for the const members after activation.
/// Invalidation: Invalid after the registry retires the asset; bodies built on it keep the backend shape alive through
///   their own reference (Zenith_PhysicsShapeHandle) until they are destroyed.
/// Units: Metres.
/// Errors: None after activation.
/// Version: since Z2 Stage 3. Format version 1 (PHYS/Formats.md "Collision shape file").
/// @endcontract
class Zenith_CollisionShapeAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_COLLISION_SHAPE;
	Zenith_PhysicsShapeHandle GetShape() const;
	Zenith_PhysicsShapeKind GetKind() const;
	const Zenith_AABB& GetLocalBounds() const;
	bool IsDynamicCapable() const;
	Zenith_Span<const Zenith_AssetHandle<Zenith_PhysicsMaterialAsset>> GetMaterialPalette() const;
	Zenith_Span<const Zenith_Vector3> GetBuoyancySamples() const;   // local points, 0 … 32
	float GetVolume() const;
};
```
