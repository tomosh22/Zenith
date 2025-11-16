#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

Zenith_ColliderComponent::Zenith_ColliderComponent(Zenith_Entity& xEntity) 
	: m_xParentEntity(xEntity)
	, m_xBodyID(JPH::BodyID())
{
}

Zenith_ColliderComponent::~Zenith_ColliderComponent() 
{
	if (m_xBodyID.IsInvalid() == false)
	{
		JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
		xBodyInterface.RemoveBody(m_xBodyID);
		xBodyInterface.DestroyBody(m_xBodyID);
	}

	if (m_pxTerrainMeshData != nullptr)
	{
		delete[] m_pxTerrainMeshData->m_pfVertices;
		delete[] m_pxTerrainMeshData->m_puIndices;
		delete m_pxTerrainMeshData;
		m_pxTerrainMeshData = nullptr;
	}
}

void Zenith_ColliderComponent::AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType) 
{
	Zenith_Assert(m_xBodyID.IsInvalid(), "This ColliderComponent already has a collider");
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	m_eVolumeType = eVolumeType;
	m_eRigidBodyType = eRigidBodyType;

	JPH::RefConst<JPH::Shape> pxShape;

	switch (eVolumeType) 
	{
	case COLLISION_VOLUME_TYPE_OBB:
	{
		pxShape = new JPH::BoxShape(JPH::Vec3(xTrans.m_xScale.x, xTrans.m_xScale.y, xTrans.m_xScale.z));
	}
	break;
	case COLLISION_VOLUME_TYPE_SPHERE:
	{
		float fRadius = glm::length(xTrans.m_xScale);
		pxShape = new JPH::SphereShape(fRadius);
	}
	break;
	case COLLISION_VOLUME_TYPE_CAPSULE:
	{
		float fRadius = glm::length(xTrans.m_xScale);
		float fHeight = fRadius * 2.0f;
		pxShape = new JPH::CapsuleShape(fHeight * 0.5f, fRadius);
	}
	break;
	case COLLISION_VOLUME_TYPE_TERRAIN:
	{
		Zenith_Assert(m_xParentEntity.HasComponent<Zenith_TerrainComponent>(), "Can't have a terrain collider without a terrain component");
		const Zenith_TerrainComponent& xTerrain = m_xParentEntity.GetComponent<Zenith_TerrainComponent>();

		const Flux_MeshGeometry& xMesh = xTerrain.GetPhysicsMeshGeometry();

		m_pxTerrainMeshData = new TerrainMeshData();
		m_pxTerrainMeshData->m_uNumVertices = xMesh.m_uNumVerts;
		m_pxTerrainMeshData->m_uNumIndices = xMesh.m_uNumIndices;

		m_pxTerrainMeshData->m_pfVertices = new float[xMesh.m_uNumVerts * 3];
		for (uint32_t i = 0; i < xMesh.m_uNumVerts; ++i)
		{
			m_pxTerrainMeshData->m_pfVertices[i * 3 + 0] = xMesh.m_pxPositions[i].x;
			m_pxTerrainMeshData->m_pfVertices[i * 3 + 1] = xMesh.m_pxPositions[i].y;
			m_pxTerrainMeshData->m_pfVertices[i * 3 + 2] = xMesh.m_pxPositions[i].z;
		}

		m_pxTerrainMeshData->m_puIndices = new uint32_t[xMesh.m_uNumIndices];
		memcpy(m_pxTerrainMeshData->m_puIndices, xMesh.m_puIndices, xMesh.m_uNumIndices * sizeof(uint32_t));

		JPH::TriangleList xTriangles;
		for (uint32_t i = 0; i < xMesh.m_uNumIndices; i += 3)
		{
			JPH::Triangle xTri;
			for (int j = 0; j < 3; ++j)
			{
				uint32_t uIdx = xMesh.m_puIndices[i + j];
				xTri.mV[j] = JPH::Float3(
					xMesh.m_pxPositions[uIdx].x,
					xMesh.m_pxPositions[uIdx].y,
					xMesh.m_pxPositions[uIdx].z
				);
			}
			xTriangles.push_back(xTri);
		}

		JPH::MeshShapeSettings xMeshSettings(xTriangles);
		JPH::Shape::ShapeResult xShapeResult = xMeshSettings.Create();
		pxShape = xShapeResult.Get();
	}
	break;
	}

	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	xTrans.GetPosition(xPos);
	xTrans.GetRotation(xRot);

	JPH::Vec3 xJoltPos(xPos.x, xPos.y, xPos.z);
	JPH::Quat xJoltRot(xRot.x, xRot.y, xRot.z, xRot.w);

	JPH::EMotionType eMotionType = (eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC) 
		? JPH::EMotionType::Dynamic 
		: JPH::EMotionType::Static;

	JPH::ObjectLayer uObjectLayer = (eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC) ? 1 : 0; // MOVING : NON_MOVING

	JPH::BodyCreationSettings xBodySettings(
		pxShape,
		xJoltPos,
		xJoltRot,
		eMotionType,
		uObjectLayer
	);

	JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
	m_xBodyID = xBodyInterface.CreateAndAddBody(xBodySettings, JPH::EActivation::Activate);
	
	if (m_xBodyID.IsInvalid())
	{
		Zenith_Assert(false, "Failed to create physics body");
		return;
	}

	JPH::BodyLockWrite xLock(Zenith_Physics::s_pxPhysicsSystem->GetBodyLockInterface(), m_xBodyID);
	if (xLock.Succeeded())
	{
		m_pxRigidBody = &xLock.GetBody();
		xTrans.m_pxRigidBody = m_pxRigidBody;
	}
}