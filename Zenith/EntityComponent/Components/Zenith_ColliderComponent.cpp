#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Physics/Zenith_Physics.h"
#include "DataStream/Zenith_DataStream.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

ZENITH_REGISTER_COMPONENT(Zenith_ColliderComponent, "Collider")

Zenith_ColliderComponent::Zenith_ColliderComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
	, m_xBodyID(JPH::BodyID())
{
}

Zenith_ColliderComponent::Zenith_ColliderComponent(Zenith_ColliderComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxRigidBody(xOther.m_pxRigidBody)
	, m_xBodyID(xOther.m_xBodyID)
	, m_eVolumeType(xOther.m_eVolumeType)
	, m_eRigidBodyType(xOther.m_eRigidBodyType)
	, m_pxTerrainMeshData(xOther.m_pxTerrainMeshData)
{
	// Nullify source so its destructor doesn't destroy the physics body
	xOther.m_pxRigidBody = nullptr;
	xOther.m_xBodyID = JPH::BodyID();  // Reset to invalid
	xOther.m_pxTerrainMeshData = nullptr;
}

Zenith_ColliderComponent& Zenith_ColliderComponent::operator=(Zenith_ColliderComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Clean up our existing physics body first
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
		}

		// Take ownership from source
		m_xParentEntity = xOther.m_xParentEntity;
		m_pxRigidBody = xOther.m_pxRigidBody;
		m_xBodyID = xOther.m_xBodyID;
		m_eVolumeType = xOther.m_eVolumeType;
		m_eRigidBodyType = xOther.m_eRigidBodyType;
		m_pxTerrainMeshData = xOther.m_pxTerrainMeshData;

		// Nullify source
		xOther.m_pxRigidBody = nullptr;
		xOther.m_xBodyID = JPH::BodyID();
		xOther.m_pxTerrainMeshData = nullptr;
	}
	return *this;
}

bool Zenith_ColliderComponent::HasValidBody() const
{
	return !m_xBodyID.IsInvalid();
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

	// Get scale and ensure it's valid (non-zero, positive) for shape creation
	Zenith_Maths::Vector3 xScale = xTrans.m_xScale;
	const float fMinScale = 0.001f;
	if (xScale.x < fMinScale) xScale.x = fMinScale;
	if (xScale.y < fMinScale) xScale.y = fMinScale;
	if (xScale.z < fMinScale) xScale.z = fMinScale;

	JPH::RefConst<JPH::Shape> pxShape;

	switch (eVolumeType)
	{
	case COLLISION_VOLUME_TYPE_AABB:
	{
		// AABB uses BoxShape but ignores entity rotation (always axis-aligned)
		pxShape = new JPH::BoxShape(JPH::Vec3(xScale.x, xScale.y, xScale.z));
	}
	break;
	case COLLISION_VOLUME_TYPE_OBB:
	{
		// OBB uses BoxShape and respects entity rotation
		pxShape = new JPH::BoxShape(JPH::Vec3(xScale.x, xScale.y, xScale.z));
	}
	break;
	case COLLISION_VOLUME_TYPE_SPHERE:
	{
		// Sphere uses the maximum scale component as radius
		// (using glm::length was incorrect - it computes vector magnitude, not max component)
		float fRadius = std::max({ xScale.x, xScale.y, xScale.z });
		pxShape = new JPH::SphereShape(fRadius);
	}
	break;
	case COLLISION_VOLUME_TYPE_CAPSULE:
	{
		float fRadius, fHalfHeight;

		if (m_bUseExplicitCapsuleDimensions)
		{
			// Use explicitly specified dimensions
			fRadius = m_fExplicitCapsuleRadius;
			fHalfHeight = m_fExplicitCapsuleHalfHeight;
		}
		else
		{
			// Capsule extends along Y axis (vertical orientation)
			// Radius is based on horizontal extents (X and Z)
			// Half-height is the cylindrical portion's half-length (Y scale minus radius for cap)
			fRadius = std::max(xScale.x, xScale.z);
			// Ensure half-height is non-negative (if Y <= radius, we get a sphere-like capsule)
			fHalfHeight = std::max(fMinScale, xScale.y - fRadius);
			if (xScale.y <= fRadius)
			{
				// Y scale is smaller than radius - use Y as half-height to prevent degenerate capsule
				fHalfHeight = fMinScale;
			}
		}
		pxShape = new JPH::CapsuleShape(fHalfHeight, fRadius);
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
	case COLLISION_VOLUME_TYPE_MODEL_MESH:
	{
		Zenith_Assert(m_xParentEntity.HasComponent<Zenith_ModelComponent>(), "Can't have a model mesh collider without a model component");
		Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();

		// Ensure physics mesh is generated
		if (!xModel.HasPhysicsMesh())
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Model does not have physics mesh, generating...");
			xModel.GeneratePhysicsMesh();
		}

		const Flux_MeshGeometry* pxPhysicsMesh = xModel.GetPhysicsMesh();
		if (!pxPhysicsMesh || !pxPhysicsMesh->m_pxPositions || pxPhysicsMesh->GetNumVerts() < 3)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Invalid physics mesh, falling back to OBB collider");
			pxShape = new JPH::BoxShape(JPH::Vec3(xTrans.m_xScale.x, xTrans.m_xScale.y, xTrans.m_xScale.z));
			break;
		}

		Zenith_Log(LOG_CATEGORY_PHYSICS, "Creating collider from model physics mesh: %u verts, %u tris",
			pxPhysicsMesh->GetNumVerts(),
			pxPhysicsMesh->GetNumIndices() / 3);

		// Store mesh data for later cleanup (reuse TerrainMeshData structure)
		// Apply transform scale to match visually rendered size
		m_pxTerrainMeshData = new TerrainMeshData();
		m_pxTerrainMeshData->m_uNumVertices = pxPhysicsMesh->m_uNumVerts;
		m_pxTerrainMeshData->m_uNumIndices = pxPhysicsMesh->m_uNumIndices;

		m_pxTerrainMeshData->m_pfVertices = new float[pxPhysicsMesh->m_uNumVerts * 3];
		Zenith_Maths::Vector3 xScale;
		xTrans.GetScale(xScale);
		for (uint32_t i = 0; i < pxPhysicsMesh->m_uNumVerts; ++i)
		{
			// Apply scale to match rendered geometry size
			m_pxTerrainMeshData->m_pfVertices[i * 3 + 0] = pxPhysicsMesh->m_pxPositions[i].x * xScale.x;
			m_pxTerrainMeshData->m_pfVertices[i * 3 + 1] = pxPhysicsMesh->m_pxPositions[i].y * xScale.y;
			m_pxTerrainMeshData->m_pfVertices[i * 3 + 2] = pxPhysicsMesh->m_pxPositions[i].z * xScale.z;
		}

		m_pxTerrainMeshData->m_puIndices = new uint32_t[pxPhysicsMesh->m_uNumIndices];
		memcpy(m_pxTerrainMeshData->m_puIndices, pxPhysicsMesh->m_puIndices, pxPhysicsMesh->m_uNumIndices * sizeof(uint32_t));

		// Try to create as convex hull first (works for both dynamic and static bodies)
		// Convex hulls are more efficient and work with dynamic bodies
		// Apply scale to match rendered geometry size
		JPH::Array<JPH::Vec3> xHullPoints;
		for (uint32_t i = 0; i < pxPhysicsMesh->m_uNumVerts; ++i)
		{
			xHullPoints.push_back(JPH::Vec3(
				pxPhysicsMesh->m_pxPositions[i].x * xScale.x,
				pxPhysicsMesh->m_pxPositions[i].y * xScale.y,
				pxPhysicsMesh->m_pxPositions[i].z * xScale.z
			));
		}
		
		Zenith_Log(LOG_CATEGORY_PHYSICS, "Creating convex hull with scale (%.3f, %.3f, %.3f), %u points",
			xScale.x, xScale.y, xScale.z, pxPhysicsMesh->m_uNumVerts);

		JPH::ConvexHullShapeSettings xConvexSettings(xHullPoints);
		JPH::Shape::ShapeResult xConvexResult = xConvexSettings.Create();
		
		if (xConvexResult.IsValid())
		{
			pxShape = xConvexResult.Get();
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Created convex hull collider successfully");
		}
		else
		{
			// Fall back to mesh shape (only works for static bodies)
			Zenith_Log(LOG_CATEGORY_PHYSICS, " Convex hull failed, falling back to mesh shape (static only)");
			
			if (eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC)
			{
				Zenith_Log(LOG_CATEGORY_PHYSICS, " WARNING: Dynamic body requires convex shape, using box fallback");
				pxShape = new JPH::BoxShape(JPH::Vec3(xTrans.m_xScale.x, xTrans.m_xScale.y, xTrans.m_xScale.z));
			}
			else
			{
				JPH::TriangleList xTriangles;
				for (uint32_t i = 0; i < pxPhysicsMesh->m_uNumIndices; i += 3)
				{
					JPH::Triangle xTri;
					for (int j = 0; j < 3; ++j)
					{
						uint32_t uIdx = pxPhysicsMesh->m_puIndices[i + j];
						// Apply scale to match rendered geometry size
						xTri.mV[j] = JPH::Float3(
							pxPhysicsMesh->m_pxPositions[uIdx].x * xScale.x,
							pxPhysicsMesh->m_pxPositions[uIdx].y * xScale.y,
							pxPhysicsMesh->m_pxPositions[uIdx].z * xScale.z
						);
					}
					xTriangles.push_back(xTri);
				}				JPH::MeshShapeSettings xMeshSettings(xTriangles);
				JPH::Shape::ShapeResult xMeshResult = xMeshSettings.Create();
				if (xMeshResult.IsValid())
				{
					pxShape = xMeshResult.Get();
					Zenith_Log(LOG_CATEGORY_PHYSICS, " Created mesh collider successfully");
				}
				else
				{
					Zenith_Log(LOG_CATEGORY_PHYSICS, " Mesh shape failed, using box fallback");
					pxShape = new JPH::BoxShape(JPH::Vec3(xTrans.m_xScale.x, xTrans.m_xScale.y, xTrans.m_xScale.z));
				}
			}
		}
	}
	break;
	}

	// Ensure shape was created
	if (pxShape == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_PHYSICS, "ERROR: Failed to create shape for volume type %d", static_cast<int>(eVolumeType));
		Zenith_Assert(false, "Failed to create physics shape - unhandled volume type?");
		return;
	}

	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	xTrans.GetPosition(xPos);
	xTrans.GetRotation(xRot);

	JPH::Vec3 xJoltPos(xPos.x, xPos.y, xPos.z);

	// AABB colliders are always axis-aligned (identity rotation)
	// OBB and other colliders use the entity's rotation
	JPH::Quat xJoltRot = (eVolumeType == COLLISION_VOLUME_TYPE_AABB)
		? JPH::Quat::sIdentity()
		: JPH::Quat(xRot.x, xRot.y, xRot.z, xRot.w);

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

		// Store entity ID as user data so we can retrieve it during collision callbacks
		// Use packed 64-bit representation of EntityID (index + generation)
		m_pxRigidBody->SetUserData(m_xParentEntity.GetEntityID().GetPacked());
	}
}

void Zenith_ColliderComponent::AddCapsuleCollider(float fRadius, float fHalfHeight, RigidBodyType eRigidBodyType)
{
	// Store explicit dimensions
	m_fExplicitCapsuleRadius = fRadius;
	m_fExplicitCapsuleHalfHeight = fHalfHeight;
	m_bUseExplicitCapsuleDimensions = true;

	// Delegate to AddCollider which will use the explicit dimensions
	AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, eRigidBodyType);
}

void Zenith_ColliderComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write collision volume type and rigid body type
	xStream << static_cast<u_int>(m_eVolumeType);
	xStream << static_cast<u_int>(m_eRigidBodyType);

	// Note: m_pxRigidBody and m_xBodyID are runtime-only physics handles
	// They will be recreated by calling AddCollider during deserialization

	// Note: m_pxTerrainMeshData is also runtime-only and will be recreated
	// from the TerrainComponent during deserialization
}

void Zenith_ColliderComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read collision volume type and rigid body type
	u_int uVolumeType;
	u_int uRigidBodyType;
	xStream >> uVolumeType;
	xStream >> uRigidBodyType;

	m_eVolumeType = static_cast<CollisionVolumeType>(uVolumeType);
	m_eRigidBodyType = static_cast<RigidBodyType>(uRigidBodyType);

	// Call AddCollider to recreate the physics body
	// This must be done after the entity and transform component are fully deserialized
	AddCollider(m_eVolumeType, m_eRigidBodyType);

	// m_xParentEntity will be set by the entity deserialization system
}

void Zenith_ColliderComponent::RebuildCollider()
{
	// Store current velocity and other physics state if it's a dynamic body
	Zenith_Maths::Vector3 xLinearVel(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xAngularVel(0.0f, 0.0f, 0.0f);
	bool bWasDynamic = (m_eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC);
	
	if (bWasDynamic && HasValidBody())
	{
		xLinearVel = Zenith_Physics::GetLinearVelocity(m_xBodyID);
		xAngularVel = Zenith_Physics::GetAngularVelocity(m_xBodyID);
	}

	// Remove existing collider
	if (m_xBodyID.IsInvalid() == false)
	{
		JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
		xBodyInterface.RemoveBody(m_xBodyID);
		xBodyInterface.DestroyBody(m_xBodyID);
		m_xBodyID = JPH::BodyID();
		m_pxRigidBody = nullptr;
	}

	// Clean up mesh data
	if (m_pxTerrainMeshData != nullptr)
	{
		delete[] m_pxTerrainMeshData->m_pfVertices;
		delete[] m_pxTerrainMeshData->m_puIndices;
		delete m_pxTerrainMeshData;
		m_pxTerrainMeshData = nullptr;
	}

	// Recreate collider with current transform (including new scale)
	AddCollider(m_eVolumeType, m_eRigidBodyType);

	// Restore velocity if it was a dynamic body
	if (bWasDynamic && HasValidBody())
	{
		Zenith_Physics::SetLinearVelocity(m_xBodyID, xLinearVel);
		Zenith_Physics::SetAngularVelocity(m_xBodyID, xAngularVel);
	}

	Zenith_Log(LOG_CATEGORY_PHYSICS, " Rebuilt collider after scale change");
}