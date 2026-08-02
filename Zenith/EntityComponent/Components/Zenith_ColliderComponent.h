#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics_Fwd.h"
#include "AssetHandling/Zenith_AssetHandle.h"   // MeshGeometryHandle (owns the physics collision mesh)
#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

namespace JPH
{
	class Body;
	class Shape;
	class BodyID;
}

// Physics collision-mesh types used only by-ref / by-ptr in this header; the full
// definitions live in the .cpp (Flux/MeshGeometry/Flux_MeshGeometry.h and
// Physics/Zenith_PhysicsMeshGenerator.h).
class Flux_MeshGeometry;
struct PhysicsMeshConfig;

// Forward declarations for RegisterProperties (cycle-avoidance — see TransformComponent.h).
template<typename T> class Zenith_Vector;
struct Zenith_PropertyDescriptor;

class Zenith_ColliderComponent {
public:
	Zenith_ColliderComponent() = delete;
	Zenith_ColliderComponent(Zenith_Entity& xEntity);
	~Zenith_ColliderComponent();

	// Property registration for prefab-variant overrides.
	static void RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties);

	// Move constructor - transfers ownership of physics body
	// Critical for component pool reallocation to not destroy bodies
	Zenith_ColliderComponent(Zenith_ColliderComponent&& xOther) noexcept;
	Zenith_ColliderComponent& operator=(Zenith_ColliderComponent&& xOther) noexcept;

	// Delete copy operations - physics bodies shouldn't be copied
	Zenith_ColliderComponent(const Zenith_ColliderComponent&) = delete;
	Zenith_ColliderComponent& operator=(const Zenith_ColliderComponent&) = delete;

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	Zenith_PhysicsBodyID GetBodyID() const { return m_xBodyID; }
	bool HasValidBody() const;
	Zenith_EntityID GetEntityID() { return m_xParentEntity.GetEntityID(); }
	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }
	CollisionVolumeType GetCollisionVolumeType() const { return m_eVolumeType; }
	RigidBodyType GetRigidBodyType() const { return m_eRigidBodyType; }
	void SetDebugDrawPhysicsMesh(bool bEnable) { m_bDebugDrawPhysicsMesh = bEnable; }
	bool GetDebugDrawPhysicsMesh() const { return m_bDebugDrawPhysicsMesh; }

	// Collision mesh derived from the entity's Zenith_ModelComponent geometry. Built
	// lazily at AddCollider/RebuildCollider time (MODEL_MESH volumes only), never
	// serialized, and consumed by the MODEL_MESH shape builder, editor picking, and
	// debug draw. Both accessors report ONLY revision-current geometry: if the model's
	// geometry changed since the cached mesh was built (or there is no model), the mesh
	// is treated as absent (HasPhysicsMesh() false / GetPhysicsMesh() null). It reappears
	// after the next RebuildCollider(); the live Jolt body is left unchanged until then.
	bool HasPhysicsMesh() const;
	const Flux_MeshGeometry* GetPhysicsMesh() const;

	// NavMesh-input flag: when false, Zenith_NavMeshGenerator skips this
	// collider during geometry collection. The collider still participates
	// in physics. Use this for static obstacles that AI should be ABLE to
	// path through dynamically (doors, breakable barriers, lift gates) --
	// pathfinding then uses Zenith_NavMesh::SetPolygonBlocked / SetBlockedAtPoint
	// at runtime to mark the corresponding polygons as blocked when the
	// obstacle is in its "closed" state. Defaults to true (existing
	// behaviour: every static collider becomes navmesh geometry).
	void SetIncludeInNavMesh(bool bInclude) { m_bIncludeInNavMesh = bInclude; }
	bool GetIncludeInNavMesh() const { return m_bIncludeInNavMesh; }

	void AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType);
	void AddCapsuleCollider(float fRadius, float fHalfHeight, RigidBodyType eRigidBodyType);
	void RebuildCollider(); // Rebuild collider with current transform (e.g., after scale change)

	//--------------------------------------------------------------------------
	// Explicit shape dimensions on an ALREADY-CONFIGURED collider.
	//
	// WHY THEY EXIST. Scale-derived sizing is only correct while the entity's
	// transform scale IS its body box. It stops being correct the moment the
	// entity carries a MODEL, because the model then dictates the scale — and a
	// UNIFORM scale degenerates a scale-derived capsule into a sphere
	// (Editor/Zenith_EditorAutomation.h says so in as many words). These setters
	// are how a caller states the body it means, independently of how big the
	// thing is drawn. Explicit capsule dimensions do NOT serialize
	// (WriteToDataStream emits {volumeType, bodyType, debugFlag} only, and
	// ReadFromDataStream re-adds a scale-derived collider), so whoever owns the
	// body must install them again after every load.
	//
	// UNITS. The capsule pair matches AddCapsuleCollider exactly: a radius, and
	// the CYLINDER half-height EXCLUDING the caps — so the capsule stands
	// (fRadius + fCylinderHalfHeight) tall in each direction. The box takes
	// HALF-extents, matching CreateBoxShape and ComputeBoxDimensionsAndOffset.
	//
	// THEY REPLACE A SHAPE; THEY NEVER CREATE ONE. Neither takes a RigidBodyType:
	// each preserves the collider's existing m_eRigidBodyType. Called on a
	// collider with no live body they WARN and change nothing — which is also
	// what protects them from a freshly constructed component, whose
	// m_eVolumeType/m_eRigidBodyType are uninitialised until AddCollider or
	// ReadFromDataStream has run. A caller that may be first-in must create the
	// body the normal way (AddCapsuleCollider / AddCollider) and use the setter
	// only when replacing an existing configured one.
	//
	// VALIDATION IS FAIL-CLOSED. Every component must be finite and > 0; values
	// below JPH::cDefaultConvexRadius are clamped exactly as CreateBoxShape
	// already clamps them. Invalid input warns and leaves the body untouched —
	// never an assert, because these are fed derived numbers and an assert would
	// kill the whole boot unit suite rather than log a bad call.
	//
	// A REQUEST THAT MATCHES THE CURRENT SHAPE IS A NO-OP, compared against the
	// validated-and-clamped STORED values rather than the raw arguments (so a
	// request that clamps onto the current shape does not churn the body either).
	// That keeps the body ID stable across repeated OnStart, which matters to
	// anything keyed on body-ID identity.
	//
	// ★ THEY DO NOT PRESERVE BODY CONFIGURATION. Replacing the shape goes through
	// RebuildCollider, which destroys and recreates the Jolt body and therefore
	// drops sensor state, gravity-enabled and locked axes. Zenith_Physics exposes
	// getters for friction and restitution ONLY, so a capture-and-restore is not
	// implementable through the public API — whoever configured the body must
	// re-apply that configuration after calling these.
	//--------------------------------------------------------------------------
	void SetExplicitCapsuleDimensions(float fRadius, float fCylinderHalfHeight);
	void SetExplicitBoxHalfExtents(const Zenith_Maths::Vector3& xHalfExtents);

	// 2026-05-25: toggle the body between solid (default) and sensor.
	// Sensor bodies still register overlap events but don't physically
	// collide -- other bodies pass straight through. Used by DPDoor to
	// let the player walk through a swinging-open door without being
	// pushed by the rotating collider. No-op if the body isn't valid yet.
	void SetIsSensor(bool bSensor);
	void QueueDebugDraw(const Zenith_Maths::Vector3& xColor) const;

#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel();

private:
	void DestroyExistingCollider();
	void RenderAddColliderUI();
	void RenderConfiguredColliderUI();
public:
#endif
	
public:
	// Public mesh-aware box sizing -- shared between CreateBoxShape (which builds the
	// Jolt body) and external consumers that need to know the actual half-extents +
	// local offset the physics body was built with. Telemetry obstacle scanning
	// (Test_PersonalityPlaythrough) uses this to emit world-space wall OBBs that
	// match the colliders the bot is actually navigating against, rather than
	// guessing from raw transform scale (which would miss the mesh-aware offset
	// the BuildingAssetKit walls rely on -- mesh bounds (-1,0,-1)..(1,4,1) means
	// the Y offset is mid-height, not zero).
	void ComputeBoxDimensionsAndOffset(const Zenith_Maths::Vector3& xScale,
		Zenith_Maths::Vector3& xHalfExtentsOut,
		Zenith_Maths::Vector3& xLocalOffsetOut,
		bool bWarnOnDegenerateBounds) const;

private:
	// Shape factories used by AddCollider. Kept private because they can (and do) mutate
	// member state — CreateTerrainShape and CreateConvexOrMeshShape allocate
	// m_pxTerrainMeshData for later cleanup, so they cannot be free functions.
	//
	// Returning JPH::RefConst<JPH::Shape> keeps the lifetime explicit. Box/Sphere/Capsule
	// paths construct the Ref from a fresh `new`'d shape (refcount 0 → 1 on AddRef).
	// Terrain/ConvexOrMesh paths construct the Ref from the ShapeResult's payload, which
	// AddRefs the live shape so it survives the local ShapeResult's destruction at function
	// exit. Either way the caller adopts a single owning reference.
	JPH::RefConst<JPH::Shape> CreateBoxShape(const Zenith_Maths::Vector3& xScale) const;
	JPH::RefConst<JPH::Shape> CreateSphereShape(const Zenith_Maths::Vector3& xScale) const;
	JPH::RefConst<JPH::Shape> CreateCapsuleShape(const Zenith_Maths::Vector3& xScale, float fMinScale) const;
	JPH::RefConst<JPH::Shape> CreateTerrainShape();
	JPH::RefConst<JPH::Shape> CreateConvexOrMeshShape(const Zenith_Maths::Vector3& xScale, RigidBodyType eRigidBodyType);

	// Physics collision mesh (derived from the entity's Zenith_ModelComponent). Generated
	// lazily and gated on the model's geometry revision, so an unchanged model reuses the
	// cached mesh across a RebuildCollider (e.g. a scale change) instead of regenerating.
	// EnsurePhysicsMeshCurrent is the single (re)generation entry point, called from
	// CreateConvexOrMeshShape.
	void EnsurePhysicsMeshCurrent();
	void GeneratePhysicsMeshWithConfig(const PhysicsMeshConfig& xConfig);
	void ClearPhysicsMesh();

	Zenith_Entity m_xParentEntity;
	JPH::Body* m_pxRigidBody = nullptr;
	Zenith_PhysicsBodyID m_xBodyID;

	CollisionVolumeType m_eVolumeType;
	RigidBodyType m_eRigidBodyType;

	// Explicit capsule dimensions (used when AddCapsuleCollider is called, or
	// SetExplicitCapsuleDimensions).
	float m_fExplicitCapsuleRadius = 0.0f;
	float m_fExplicitCapsuleHalfHeight = 0.0f;
	bool m_bUseExplicitCapsuleDimensions = false;
	// Explicit BOX half-extents (SetExplicitBoxHalfExtents). When set,
	// ComputeBoxDimensionsAndOffset returns these verbatim with a ZERO local
	// offset — mesh bounds and transform scale are both bypassed — so the Jolt
	// shape, the debug wireframe and the navmesh geometry builder all see one box.
	// MUST be transferred by the move constructor and move assignment, like the
	// capsule fields above: components live in relocating pools, and a missed
	// field silently reverts a body to scale-derived sizing on the next Grow.
	Zenith_Maths::Vector3 m_xExplicitBoxHalfExtents = Zenith_Maths::Vector3(0.0f);
	bool m_bUseExplicitBoxHalfExtents = false;
	bool m_bDebugDrawPhysicsMesh = false;
	// See SetIncludeInNavMesh comment. Defaults to true so existing colliders
	// (floors, walls, props) continue to contribute navmesh geometry without
	// requiring an opt-in change. Callers that author runtime-blockable
	// obstacles (doors, gates) opt OUT via SetIncludeInNavMesh(false).
	bool m_bIncludeInNavMesh = true;

	struct TerrainMeshData
	{
		float* m_pfVertices = nullptr;
		uint32_t* m_puIndices = nullptr;
		uint32_t m_uNumVertices = 0;
		uint32_t m_uNumIndices = 0;
	};
	TerrainMeshData* m_pxTerrainMeshData = nullptr;

	// Cached collision mesh + the model geometry revision it was built from. Runtime-only
	// (never serialized); recreated lazily by EnsurePhysicsMeshCurrent.
	MeshGeometryHandle m_xPhysicsMeshAsset;
	uint32_t m_uPhysicsMeshGeometryRevision = 0;

};
