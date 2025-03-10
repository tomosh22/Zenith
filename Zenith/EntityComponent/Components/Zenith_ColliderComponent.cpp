#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"

Zenith_ColliderComponent::Zenith_ColliderComponent(Zenith_Entity& xEntity) : m_xParentEntity(xEntity) {
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
	case COLLISION_VOLUME_TYPE_CAPSULE:
	{
		reactphysics3d::CapsuleShape* pxCapsuleShape = Zenith_Physics::s_xPhysicsCommon.createCapsuleShape(glm::length(xTrans.m_xScale), glm::length(xTrans.m_xScale) * 2);

		m_pxCollider = m_pxRigidBody->addCollider(pxCapsuleShape, reactphysics3d::Transform::identity());
	}
	break;
	case COLLISION_VOLUME_TYPE_TERRAIN:
	{
		Zenith_Assert(m_xParentEntity.HasComponent<Zenith_TerrainComponent>(), "Can't have a terrain collider without a terrain component");
		const Zenith_TerrainComponent& xTerrain = m_xParentEntity.GetComponent<Zenith_TerrainComponent>();

		const Flux_MeshGeometry& xMesh = xTerrain.GetPhysicsMeshGeometry();

		const Zenith_Maths::Vector3* pxPositions = xMesh.m_pxPositions;
		const Zenith_Maths::Vector3* pxNormals = xMesh.m_pxNormals;
		const uint32_t* puIndices = xMesh.m_puIndices;

		m_pxTriArray = new reactphysics3d::TriangleVertexArray(xMesh.m_uNumVerts, pxPositions, sizeof(pxPositions[0]), pxNormals, sizeof(pxNormals[0]), xMesh.m_uNumIndices / 3, puIndices, sizeof(puIndices[0]) * 3, reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE, reactphysics3d::TriangleVertexArray::NormalDataType::NORMAL_FLOAT_TYPE, reactphysics3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE);

		std::vector<reactphysics3d::Message> xMessages;
		m_pxTriMesh = Zenith_Physics::s_xPhysicsCommon.createTriangleMesh(*m_pxTriArray, xMessages);

		m_pxConcaveShape = Zenith_Physics::s_xPhysicsCommon.createConcaveMeshShape(m_pxTriMesh);

		m_pxCollider = m_pxRigidBody->addCollider(m_pxConcaveShape, reactphysics3d::Transform::identity());
		m_pxCollider->getMaterial().setBounciness(0);
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