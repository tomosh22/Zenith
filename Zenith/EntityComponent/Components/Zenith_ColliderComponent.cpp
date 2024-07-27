#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"

Zenith_ColliderComponent::Zenith_ColliderComponent(Zenith_Entity& xEntity) :  m_xParentEntity(xEntity) {
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	m_pxRigidBody = Zenith_Physics::s_pxPhysicsWorld->createRigidBody(xTrans.GetTransform_Unsafe());

	m_pxRigidBody->setUserData(reinterpret_cast<void*>((GUIDType)xEntity.GetGUID()));

	xTrans.m_pxRigidBody = m_pxRigidBody;
}

void Zenith_ColliderComponent::AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType) {
	Zenith_Assert(m_pxCollider == nullptr, "This ColliderComponent already has a collider");
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	m_eVolumeType = eVolumeType;
	m_eRigidBodyType = eRigidBodyType;
	switch (eVolumeType) {
	case COLLISION_VOLUME_TYPE_OBB:
	{
		reactphysics3d::BoxShape* pxOBBShape = Zenith_Physics::s_xPhysicsCommon.createBoxShape(reactphysics3d::Vector3(xTrans.m_xScale.x, xTrans.m_xScale.y, xTrans.m_xScale.z));

		m_pxCollider = m_pxRigidBody->addCollider(pxOBBShape, reactphysics3d::Transform::identity());
	}
	break;
	case COLLISION_VOLUME_TYPE_SPHERE:
	{
		reactphysics3d::SphereShape* pxSphereShape = Zenith_Physics::s_xPhysicsCommon.createSphereShape(glm::length(xTrans.m_xScale));

		m_pxCollider = m_pxRigidBody->addCollider(pxSphereShape, reactphysics3d::Transform::identity());
	}
	break;
	case COLLISION_VOLUME_TYPE_TERRAIN:
	{
		STUBBED
#if 0
		VCE_Assert(m_xParentEntity.HasComponent<TerrainComponent>(), "Can't have a terrain collider without a terrain component");
		const TerrainComponent& xTerrain = m_xParentEntity.GetComponent<TerrainComponent>();

		const Mesh* pxMesh = xTerrain.m_pxMesh;

		if (!pxMesh->m_bInitialised) {
			VCE_TRACE("Terrain mesh not initialised");
			break;
		}

		glm::highp_vec3* pxPositions = pxMesh->m_pxVertexPositions;
		const glm::highp_vec3* pxNormals = pxMesh->m_pxNormals;
		uint32_t* puIndices = pxMesh->m_puIndices;

		//#TO_TODO: do i want a separate physics mesh? that way i can delete mesh data once it's on the GPU
		for (uint32_t i = 0; i < pxMesh->m_uNumVerts; i++) {
			pxPositions[i].y *= -1.f;
			pxPositions[i].z *= -1.f;
			//pxPositions[i].x *= -1.f;
		}

		m_pxTriArray = new reactphysics3d::TriangleVertexArray(pxMesh->m_uNumVerts, pxPositions, sizeof(pxPositions[0]), pxNormals, sizeof(pxNormals[0]), pxMesh->m_uNumIndices / 3, puIndices, sizeof(puIndices[0]) * 3, reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE, reactphysics3d::TriangleVertexArray::NormalDataType::NORMAL_FLOAT_TYPE, reactphysics3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE);

		m_pxTriMesh = Physics::s_xPhysicsCommon.createTriangleMesh();
		m_pxTriMesh->addSubpart(m_pxTriArray);

		m_pxConcaveShape = Physics::s_xPhysicsCommon.createConcaveMeshShape(m_pxTriMesh);

		m_pxCollider = m_pxRigidBody->addCollider(m_pxConcaveShape, reactphysics3d::Transform::identity());
#endif
	}
	break;
	}

	switch (eRigidBodyType) {
	case RIGIDBODY_TYPE_DYNAMIC:
		m_pxRigidBody->setType(reactphysics3d::BodyType::DYNAMIC);
		break;
	case RIGIDBODY_TYPE_STATIC:
		m_pxRigidBody->setType(reactphysics3d::BodyType::STATIC);
		break;
	}
}