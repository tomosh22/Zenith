#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

namespace JPH
{
	class Body;
	class Shape;
	class BodyID;
}

class Zenith_ColliderComponent {
public:
	Zenith_ColliderComponent() = delete;
	Zenith_ColliderComponent(Zenith_Entity& xEntity);
	~Zenith_ColliderComponent();

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
	RigidBodyType GetRigidBodyType() const { return m_eRigidBodyType; }

	void AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType);
	void AddCapsuleCollider(float fRadius, float fHalfHeight, RigidBodyType eRigidBodyType);
	void RebuildCollider(); // Rebuild collider with current transform (e.g., after scale change)

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
	
private:
	// Shape factories used by AddCollider. Kept private because they can (and do) mutate
	// member state — CreateTerrainShape and CreateConvexOrMeshShape allocate
	// m_pxTerrainMeshData for later cleanup, so they cannot be free functions.
	//
	// Returning raw JPH::Shape* (ref count 0 on return) avoids pulling the Jolt
	// Reference.h template into this header; AddCollider assigns the result into a
	// JPH::RefConst<JPH::Shape> which takes ownership.
	JPH::Shape* CreateBoxShape(const Zenith_Maths::Vector3& xScale) const;
	JPH::Shape* CreateSphereShape(const Zenith_Maths::Vector3& xScale) const;
	JPH::Shape* CreateCapsuleShape(const Zenith_Maths::Vector3& xScale, float fMinScale) const;
	JPH::Shape* CreateTerrainShape();
	JPH::Shape* CreateConvexOrMeshShape(const Zenith_Maths::Vector3& xScale, RigidBodyType eRigidBodyType);

	Zenith_Entity m_xParentEntity;
	JPH::Body* m_pxRigidBody = nullptr;
	JPH::BodyID m_xBodyID;

	CollisionVolumeType m_eVolumeType;
	RigidBodyType m_eRigidBodyType;

	// Explicit capsule dimensions (used when AddCapsuleCollider is called)
	float m_fExplicitCapsuleRadius = 0.0f;
	float m_fExplicitCapsuleHalfHeight = 0.0f;
	bool m_bUseExplicitCapsuleDimensions = false;

	struct TerrainMeshData
	{
		float* m_pfVertices = nullptr;
		uint32_t* m_puIndices = nullptr;
		uint32_t m_uNumVertices = 0;
		uint32_t m_uNumIndices = 0;
	};
	TerrainMeshData* m_pxTerrainMeshData = nullptr;

};
