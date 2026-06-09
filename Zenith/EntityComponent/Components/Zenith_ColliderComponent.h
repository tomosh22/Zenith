#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
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

	const JPH::BodyID& GetBodyID() const { return m_xBodyID; }
	bool HasValidBody() const;
	Zenith_EntityID GetEntityID() { return m_xParentEntity.GetEntityID(); }
	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }
	CollisionVolumeType GetCollisionVolumeType() const { return m_eVolumeType; }
	RigidBodyType GetRigidBodyType() const { return m_eRigidBodyType; }
	void SetDebugDrawPhysicsMesh(bool bEnable) { m_bDebugDrawPhysicsMesh = bEnable; }
	bool GetDebugDrawPhysicsMesh() const { return m_bDebugDrawPhysicsMesh; }

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

	Zenith_Entity m_xParentEntity;
	JPH::Body* m_pxRigidBody = nullptr;
	JPH::BodyID m_xBodyID;

	CollisionVolumeType m_eVolumeType;
	RigidBodyType m_eRigidBodyType;

	// Explicit capsule dimensions (used when AddCapsuleCollider is called)
	float m_fExplicitCapsuleRadius = 0.0f;
	float m_fExplicitCapsuleHalfHeight = 0.0f;
	bool m_bUseExplicitCapsuleDimensions = false;
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

};
