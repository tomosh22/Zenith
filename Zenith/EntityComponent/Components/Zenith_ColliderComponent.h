#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"

class Zenith_ColliderComponent {
public:
	Zenith_ColliderComponent() = delete;
	Zenith_ColliderComponent(Zenith_Entity& xEntity);
	~Zenith_ColliderComponent() {
		Zenith_Physics::s_pxPhysicsWorld->destroyRigidBody(m_pxRigidBody);
		Zenith_Assert(!(m_pxTriArray != nullptr ^ m_pxConcaveShape != nullptr), "If we have a TriangleVertexArray then we must also have a ConcaveMeshShape, and vice-versa");
		if (m_pxTriArray != nullptr) {
			Zenith_Assert(m_eVolumeType == COLLISION_VOLUME_TYPE_TERRAIN, "A collider component that isn't a terrain collider has somehow ended up with a triangle vertex array");
			//#TO_TODO: deprecated?
#if 0
			for (uint32_t i = 0; i < m_pxTriMesh->getNbSubparts(); i++) {
				delete m_pxTriMesh->getSubpart(i);
			}
#endif
			Zenith_Physics::s_xPhysicsCommon.destroyTriangleMesh(m_pxTriMesh);
			Zenith_Physics::s_xPhysicsCommon.destroyConcaveMeshShape(m_pxConcaveShape);
			delete m_pxTriArray;
			m_pxTriArray = nullptr;
		}
	}
	void Serialize(std::ofstream& xOut);
	reactphysics3d::RigidBody* GetRigidBody() const { return m_pxRigidBody; }
	reactphysics3d::Collider* GetCollider() const { return m_pxCollider; }
	Zenith_EntityID GetEntityID() { return m_xParentEntity.GetEntityID(); }

	void AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType);
private:
	Zenith_Entity m_xParentEntity;
	reactphysics3d::RigidBody* m_pxRigidBody = nullptr;
	reactphysics3d::Collider* m_pxCollider = nullptr;

	CollisionVolumeType m_eVolumeType;
	RigidBodyType m_eRigidBodyType;

	//#TO currently only used for terrain colliders
	reactphysics3d::TriangleVertexArray* m_pxTriArray = nullptr;
	reactphysics3d::ConcaveMeshShape* m_pxConcaveShape = nullptr;
	reactphysics3d::TriangleMesh* m_pxTriMesh = nullptr;
};
