// This file creates Jolt Physics objects - disable memory tracking macro to avoid conflicts
// with Jolt's custom operator new
#include "Zenith.h"
#define ZENITH_PLACEMENT_NEW_ZONE
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

ZENITH_REGISTER_COMPONENT(Zenith_ColliderComponent, "Collider")

void Zenith_ColliderComponent::RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties)
{
	// Intentionally registers no overrideable properties.
	//
	// All ColliderComponent fields are STATEFUL: the Jolt body, shape, mass,
	// layer, and capsule dimensions are baked into the Jolt simulation when
	// AddCollider() / ReadFromDataStream() runs. After that, mutating the raw
	// fields does NOT update the body — Jolt won't see the change. Properly
	// applying a variant override would require destroying and recreating the
	// body via BodyInterface, mid-Instantiate, which is invasive and out of
	// scope for the current property reflection layer.
	//
	// If you need different colliders per variant, override at the prefab
	// level (different .zpfb with a different ColliderComponent serialisation),
	// not via the override list.
	(void)axProperties;
}

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
	, m_bDebugDrawPhysicsMesh(xOther.m_bDebugDrawPhysicsMesh)
	, m_pxTerrainMeshData(xOther.m_pxTerrainMeshData)
{
	// Nullify source so its destructor doesn't destroy the physics body
	xOther.m_pxRigidBody = nullptr;
	xOther.m_xBodyID = JPH::BodyID();  // Reset to invalid
	xOther.m_bDebugDrawPhysicsMesh = false;
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
		m_bDebugDrawPhysicsMesh = xOther.m_bDebugDrawPhysicsMesh;
		m_pxTerrainMeshData = xOther.m_pxTerrainMeshData;

		// Nullify source
		xOther.m_pxRigidBody = nullptr;
		xOther.m_xBodyID = JPH::BodyID();
		xOther.m_bDebugDrawPhysicsMesh = false;
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
	if (m_xBodyID.IsInvalid() == false && Zenith_Physics::s_pxPhysicsSystem != nullptr)
	{
		JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
		// Check if the body actually exists in the physics system before trying to destroy it.
		// This handles cases where scene restore loads stale body IDs that don't exist.
		if (xBodyInterface.IsAdded(m_xBodyID))
		{
			xBodyInterface.RemoveBody(m_xBodyID);
			xBodyInterface.DestroyBody(m_xBodyID);
		}
	}

	if (m_pxTerrainMeshData != nullptr)
	{
		delete[] m_pxTerrainMeshData->m_pfVertices;
		delete[] m_pxTerrainMeshData->m_puIndices;
		delete m_pxTerrainMeshData;
		m_pxTerrainMeshData = nullptr;
	}
}

namespace
{
	// Central min-scale floor; capsule computation also uses it for its degenerate case.
	constexpr float g_fMinColliderScale = 0.001f;

	Zenith_Maths::Vector3 ClampScale(const Zenith_Maths::Vector3& xScale)
	{
		Zenith_Maths::Vector3 xClamped = xScale;
		if (xClamped.x < g_fMinColliderScale) xClamped.x = g_fMinColliderScale;
		if (xClamped.y < g_fMinColliderScale) xClamped.y = g_fMinColliderScale;
		if (xClamped.z < g_fMinColliderScale) xClamped.z = g_fMinColliderScale;
		return xClamped;
	}

	void AddWireBoxEdges(const Zenith_Maths::Vector3 axCorners[8], const Zenith_Maths::Vector3& xColor)
	{
		static constexpr uint32_t s_auEdgeIndices[] = {
			0, 1, 1, 3, 3, 2, 2, 0,
			4, 5, 5, 7, 7, 6, 6, 4,
			0, 4, 1, 5, 2, 6, 3, 7
		};

		for (uint32_t u = 0; u < 12; ++u)
		{
			Flux_Primitives::AddLine(
				axCorners[s_auEdgeIndices[u * 2 + 0]],
				axCorners[s_auEdgeIndices[u * 2 + 1]],
				xColor);
		}
	}

	void BuildUnitCubeCorners(const Zenith_Maths::Matrix4& xModelMatrix, Zenith_Maths::Vector3 axCornersOut[8])
	{
		static const Zenith_Maths::Vector3 s_axLocalCorners[8] = {
			Zenith_Maths::Vector3(-0.5f, -0.5f, -0.5f),
			Zenith_Maths::Vector3( 0.5f, -0.5f, -0.5f),
			Zenith_Maths::Vector3(-0.5f,  0.5f, -0.5f),
			Zenith_Maths::Vector3( 0.5f,  0.5f, -0.5f),
			Zenith_Maths::Vector3(-0.5f, -0.5f,  0.5f),
			Zenith_Maths::Vector3( 0.5f, -0.5f,  0.5f),
			Zenith_Maths::Vector3(-0.5f,  0.5f,  0.5f),
			Zenith_Maths::Vector3( 0.5f,  0.5f,  0.5f)
		};

		for (uint32_t u = 0; u < 8; ++u)
		{
			const Zenith_Maths::Vector4 xWorld =
				xModelMatrix * Zenith_Maths::Vector4(s_axLocalCorners[u], 1.0f);
			axCornersOut[u] = Zenith_Maths::Vector3(xWorld.x, xWorld.y, xWorld.z);
		}
	}
}

JPH::RefConst<JPH::Shape> Zenith_ColliderComponent::CreateBoxShape(const Zenith_Maths::Vector3& xScale) const
{
	// BoxShape takes half-extents; unit cube is -0.5..0.5 so half-extent = scale * 0.5
	return new JPH::BoxShape(JPH::Vec3(xScale.x * 0.5f, xScale.y * 0.5f, xScale.z * 0.5f));
}

JPH::RefConst<JPH::Shape> Zenith_ColliderComponent::CreateSphereShape(const Zenith_Maths::Vector3& xScale) const
{
	// Unit sphere has radius 0.5, so physics radius = max_scale * 0.5
	const float fRadius = std::max({ xScale.x, xScale.y, xScale.z }) * 0.5f;
	return new JPH::SphereShape(fRadius);
}

JPH::RefConst<JPH::Shape> Zenith_ColliderComponent::CreateCapsuleShape(const Zenith_Maths::Vector3& xScale, float fMinScale) const
{
	float fRadius;
	float fHalfHeight;

	if (m_bUseExplicitCapsuleDimensions)
	{
		// Caller provided explicit dimensions via AddCapsuleCollider.
		fRadius = m_fExplicitCapsuleRadius;
		fHalfHeight = m_fExplicitCapsuleHalfHeight;
	}
	else
	{
		// Capsule extends along Y. Radius comes from horizontal extents (X/Z); the
		// cylindrical middle half-length is Y_half minus the cap radius. If Y is too
		// short for the implied caps the capsule is degenerate, so clamp the cylinder
		// length to fMinScale rather than letting it go negative.
		fRadius = std::max(xScale.x, xScale.z) * 0.5f;
		const float fHalfY = xScale.y * 0.5f;
		fHalfHeight = std::max(fMinScale, fHalfY - fRadius);
		if (fHalfY <= fRadius)
		{
			fHalfHeight = fMinScale;
		}
	}
	return new JPH::CapsuleShape(fHalfHeight, fRadius);
}

JPH::RefConst<JPH::Shape> Zenith_ColliderComponent::CreateTerrainShape()
{
	Zenith_Assert(m_xParentEntity.HasComponent<Zenith_TerrainComponent>(), "Can't have a terrain collider without a terrain component");
	const Zenith_TerrainComponent& xTerrain = m_xParentEntity.GetComponent<Zenith_TerrainComponent>();
	// Terrain physics geometry can be absent if the underlying chunk meshes
	// failed to load (the all-failed path in LoadCombinedPhysicsGeometry
	// intentionally leaves m_pxPhysicsGeometry null). Skip body creation
	// in that case rather than dereferencing through a null pointer.
	if (!xTerrain.HasPhysicsGeometry())
	{
		Zenith_Warning(LOG_CATEGORY_PHYSICS, "Zenith_ColliderComponent::CreateTerrainShape: terrain has no physics geometry; skipping body creation");
		return nullptr;
	}
	const Flux_MeshGeometry& xMesh = xTerrain.GetPhysicsMeshGeometry();

	// Copy mesh data into our owned storage so the physics body outlives the
	// TerrainComponent's internal geometry; the destructor frees m_pxTerrainMeshData.
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
			const uint32_t uIdx = xMesh.m_puIndices[i + j];
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
	if (!xShapeResult.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_PHYSICS, "Zenith_ColliderComponent::CreateTerrainShape: MeshShape creation failed: %s", xShapeResult.GetError().c_str());
		return nullptr;
	}
	// xShapeResult holds the only ref. Returning the inner Ref by value (as
	// RefConst<Shape>) AddRefs the shape; xShapeResult's destruction at function
	// exit then drops its ref, leaving the returned RefConst as the sole owner.
	return xShapeResult.Get();
}

JPH::RefConst<JPH::Shape> Zenith_ColliderComponent::CreateConvexOrMeshShape(const Zenith_Maths::Vector3& xScale, RigidBodyType eRigidBodyType)
{
	Zenith_Assert(m_xParentEntity.HasComponent<Zenith_ModelComponent>(), "Can't have a model mesh collider without a model component");
	Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	if (!xModel.HasPhysicsMesh())
	{
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Model does not have physics mesh, generating...");
		xModel.GeneratePhysicsMesh();
	}

	const Flux_MeshGeometry* pxPhysicsMesh = xModel.GetPhysicsMesh();
	if (!pxPhysicsMesh || !pxPhysicsMesh->m_pxPositions || pxPhysicsMesh->GetNumVerts() < 3)
	{
		// Model has no usable physics mesh — fall back to a scaled box so the entity
		// still has a collider of roughly the right size.
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Invalid physics mesh, falling back to OBB collider");
		return CreateBoxShape(xScale);
	}

	Zenith_Log(LOG_CATEGORY_PHYSICS, "Creating collider from model physics mesh: %u verts, %u tris",
		pxPhysicsMesh->GetNumVerts(),
		pxPhysicsMesh->GetNumIndices() / 3);

	// Cache the mesh for the lifetime of the body. Uses the transform's scale
	// (not the clamped xScale) so the owned copy matches the rendered geometry.
	Zenith_Maths::Vector3 xModelScale;
	xTrans.GetScale(xModelScale);

	m_pxTerrainMeshData = new TerrainMeshData();
	m_pxTerrainMeshData->m_uNumVertices = pxPhysicsMesh->m_uNumVerts;
	m_pxTerrainMeshData->m_uNumIndices = pxPhysicsMesh->m_uNumIndices;

	m_pxTerrainMeshData->m_pfVertices = new float[pxPhysicsMesh->m_uNumVerts * 3];
	for (uint32_t i = 0; i < pxPhysicsMesh->m_uNumVerts; ++i)
	{
		m_pxTerrainMeshData->m_pfVertices[i * 3 + 0] = pxPhysicsMesh->m_pxPositions[i].x * xModelScale.x;
		m_pxTerrainMeshData->m_pfVertices[i * 3 + 1] = pxPhysicsMesh->m_pxPositions[i].y * xModelScale.y;
		m_pxTerrainMeshData->m_pfVertices[i * 3 + 2] = pxPhysicsMesh->m_pxPositions[i].z * xModelScale.z;
	}

	m_pxTerrainMeshData->m_puIndices = new uint32_t[pxPhysicsMesh->m_uNumIndices];
	memcpy(m_pxTerrainMeshData->m_puIndices, pxPhysicsMesh->m_puIndices, pxPhysicsMesh->m_uNumIndices * sizeof(uint32_t));

	// Convex hulls work for both dynamic and static bodies and are cheaper than a
	// full mesh, so try this path first. If Jolt rejects the input (degenerate mesh
	// etc.) fall back to a static MeshShape, then to a box as last resort.
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
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Created convex hull collider successfully");
		// See CreateTerrainShape for the lifetime contract: returning the inner
		// Ref by value AddRefs, the local ShapeResult releases on exit.
		return xConvexResult.Get();
	}

	Zenith_Log(LOG_CATEGORY_PHYSICS, " Convex hull failed, falling back to mesh shape (static only)");

	if (eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC)
	{
		// MeshShape can't be used on dynamic bodies — box is the best we can do.
		Zenith_Log(LOG_CATEGORY_PHYSICS, " WARNING: Dynamic body requires convex shape, using box fallback");
		return CreateBoxShape(xScale);
	}

	JPH::TriangleList xTriangles;
	for (uint32_t i = 0; i < pxPhysicsMesh->m_uNumIndices; i += 3)
	{
		JPH::Triangle xTri;
		for (int j = 0; j < 3; ++j)
		{
			const uint32_t uIdx = pxPhysicsMesh->m_puIndices[i + j];
			xTri.mV[j] = JPH::Float3(
				pxPhysicsMesh->m_pxPositions[uIdx].x * xScale.x,
				pxPhysicsMesh->m_pxPositions[uIdx].y * xScale.y,
				pxPhysicsMesh->m_pxPositions[uIdx].z * xScale.z
			);
		}
		xTriangles.push_back(xTri);
	}

	JPH::MeshShapeSettings xMeshSettings(xTriangles);
	JPH::Shape::ShapeResult xMeshResult = xMeshSettings.Create();
	if (xMeshResult.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_PHYSICS, " Created mesh collider successfully");
		// Same lifetime contract as CreateTerrainShape.
		return xMeshResult.Get();
	}

	Zenith_Log(LOG_CATEGORY_PHYSICS, " Mesh shape failed, using box fallback");
	return CreateBoxShape(xScale);
}

void Zenith_ColliderComponent::AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType)
{
	Zenith_Assert(m_xBodyID.IsInvalid(), "This ColliderComponent already has a collider");
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	m_eVolumeType = eVolumeType;
	m_eRigidBodyType = eRigidBodyType;

	const Zenith_Maths::Vector3 xScale = ClampScale(xTrans.m_xScale);

	JPH::RefConst<JPH::Shape> pxShape;
	switch (eVolumeType)
	{
	case COLLISION_VOLUME_TYPE_AABB:
	case COLLISION_VOLUME_TYPE_OBB:
		// AABB and OBB share shape creation — they only differ in whether the body's
		// rotation is applied below (AABB forces identity).
		pxShape = CreateBoxShape(xScale);
		break;
	case COLLISION_VOLUME_TYPE_SPHERE:
		pxShape = CreateSphereShape(xScale);
		break;
	case COLLISION_VOLUME_TYPE_CAPSULE:
		pxShape = CreateCapsuleShape(xScale, g_fMinColliderScale);
		break;
	case COLLISION_VOLUME_TYPE_TERRAIN:
		pxShape = CreateTerrainShape();
		break;
	case COLLISION_VOLUME_TYPE_MODEL_MESH:
		pxShape = CreateConvexOrMeshShape(xScale, eRigidBodyType);
		break;
	}

	if (pxShape == nullptr)
	{
		// Terrain has an advertised "no physics geometry" path: when every
		// chunk's source mesh failed to load, CreateTerrainShape returns
		// nullptr (it already logged a warning). Treat that as a quiet
		// no-op rather than a fatal — the terrain renders nothing AND has
		// no body, but the engine shouldn't assert. Other shape paths still
		// trip on null because they don't have a documented "skip" mode.
		if (eVolumeType == COLLISION_VOLUME_TYPE_TERRAIN)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, "Zenith_ColliderComponent::AddCollider: skipping terrain body — no physics geometry available");
			return;
		}
		Zenith_Log(LOG_CATEGORY_PHYSICS, "ERROR: Failed to create shape for volume type %d", static_cast<int>(eVolumeType));
		Zenith_Assert(false, "Failed to create physics shape - unhandled volume type?");
		return;
	}

	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	xTrans.GetPosition(xPos);
	xTrans.GetRotation(xRot);

	// AABB colliders are always axis-aligned (identity rotation); OBB and the rest
	// use the entity's rotation.
	const JPH::Vec3 xJoltPos(xPos.x, xPos.y, xPos.z);
	const JPH::Quat xJoltRot = (eVolumeType == COLLISION_VOLUME_TYPE_AABB)
		? JPH::Quat::sIdentity()
		: JPH::Quat(xRot.x, xRot.y, xRot.z, xRot.w);

	const JPH::EMotionType eMotionType = (eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC)
		? JPH::EMotionType::Dynamic
		: JPH::EMotionType::Static;
	const JPH::ObjectLayer uObjectLayer = (eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC) ? 1 : 0; // MOVING : NON_MOVING

	JPH::BodyCreationSettings xBodySettings(pxShape, xJoltPos, xJoltRot, eMotionType, uObjectLayer);
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
		// Store entity ID as user data for collision callback lookup (packed 64-bit).
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

void Zenith_ColliderComponent::QueueDebugDraw(const Zenith_Maths::Vector3& xColor) const
{
	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPosition;
	Zenith_Maths::Vector3 xScale;
	Zenith_Maths::Quat xRotation;
	xTransform.GetPosition(xPosition);
	xTransform.GetScale(xScale);
	xTransform.GetRotation(xRotation);
	xScale = ClampScale(xScale);

	switch (m_eVolumeType)
	{
	case COLLISION_VOLUME_TYPE_AABB:
		Flux_Primitives::AddWireframeCube(xPosition, xScale * 0.5f, xColor);
		break;

	case COLLISION_VOLUME_TYPE_OBB:
	{
		Zenith_Maths::Matrix4 xModelMatrix;
		xTransform.BuildModelMatrix(xModelMatrix);
		Zenith_Maths::Vector3 axCorners[8];
		BuildUnitCubeCorners(xModelMatrix, axCorners);
		AddWireBoxEdges(axCorners, xColor);
		break;
	}

	case COLLISION_VOLUME_TYPE_SPHERE:
	{
		const float fRadius = std::max({ xScale.x, xScale.y, xScale.z }) * 0.5f;
		Flux_Primitives::AddSphere(xPosition, fRadius, xColor);
		break;
	}

	case COLLISION_VOLUME_TYPE_CAPSULE:
	{
		float fRadius;
		float fHalfHeight;
		if (m_bUseExplicitCapsuleDimensions)
		{
			fRadius = m_fExplicitCapsuleRadius;
			fHalfHeight = m_fExplicitCapsuleHalfHeight;
		}
		else
		{
			fRadius = std::max(xScale.x, xScale.z) * 0.5f;
			const float fHalfY = xScale.y * 0.5f;
			fHalfHeight = std::max(g_fMinColliderScale, fHalfY - fRadius);
			if (fHalfY <= fRadius)
			{
				fHalfHeight = g_fMinColliderScale;
			}
		}

		const Zenith_Maths::Vector3 xAxis = Zenith_Maths::Normalize(
			Zenith_Maths::RotateVector(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xRotation));
		const Zenith_Maths::Vector3 xStart = xPosition - xAxis * fHalfHeight;
		const Zenith_Maths::Vector3 xEnd = xPosition + xAxis * fHalfHeight;
		Flux_Primitives::AddCapsule(xStart, xEnd, fRadius, xColor);
		break;
	}

	case COLLISION_VOLUME_TYPE_TERRAIN:
	{
		if (!m_xParentEntity.HasComponent<Zenith_TerrainComponent>())
		{
			return;
		}

		Zenith_Maths::Matrix4 xModelMatrix;
		xTransform.BuildModelMatrix(xModelMatrix);
		const Zenith_TerrainComponent& xTerrain = m_xParentEntity.GetComponent<Zenith_TerrainComponent>();
		// Same null-tolerance contract as CreateTerrainShape — skip the
		// debug draw if the physics geometry never loaded.
		if (xTerrain.HasPhysicsGeometry())
		{
			Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(&xTerrain.GetPhysicsMeshGeometry(), xModelMatrix, xColor);
		}
		break;
	}

	case COLLISION_VOLUME_TYPE_MODEL_MESH:
	{
		if (!m_xParentEntity.HasComponent<Zenith_ModelComponent>())
		{
			return;
		}

		Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();
		const Flux_MeshGeometry* pxPhysicsMesh = xModel.GetPhysicsMesh();
		if (!pxPhysicsMesh)
		{
			return;
		}

		Zenith_Maths::Matrix4 xModelMatrix;
		xTransform.BuildModelMatrix(xModelMatrix);
		Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(pxPhysicsMesh, xModelMatrix, xColor);
		break;
	}
	}
}

void Zenith_ColliderComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write collision volume type and rigid body type
	xStream << static_cast<u_int>(m_eVolumeType);
	xStream << static_cast<u_int>(m_eRigidBodyType);
	xStream << m_bDebugDrawPhysicsMesh;

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
	m_bDebugDrawPhysicsMesh = false;

	// Older scenes only serialized the volume/body pair, so treat the debug
	// flag as optional to remain backward compatible.
	if (xStream.GetCursor() < xStream.GetSize())
	{
		xStream >> m_bDebugDrawPhysicsMesh;
	}

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

#ifdef ZENITH_TOOLS

#include "imgui.h"

namespace
{
	const char* const s_aszVolumeTypes[] = { "AABB", "OBB", "Sphere", "Capsule", "Terrain", "Model Mesh" };
	const char* const s_aszRigidBodyTypes[] = { "Dynamic", "Static" };
	constexpr int s_iNumVolumeTypes = 6;
	constexpr int s_iNumRigidBodyTypes = 2;
}

// Tear down the current physics body and associated terrain data so a new
// collider can be constructed in its place. No-op if there is nothing live.
void Zenith_ColliderComponent::DestroyExistingCollider()
{
	if (m_xBodyID.IsInvalid() == false)
	{
		JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
		xBodyInterface.RemoveBody(m_xBodyID);
		xBodyInterface.DestroyBody(m_xBodyID);
		m_xBodyID = JPH::BodyID();
		m_pxRigidBody = nullptr;
	}
	if (m_pxTerrainMeshData != nullptr)
	{
		delete[] m_pxTerrainMeshData->m_pfVertices;
		delete[] m_pxTerrainMeshData->m_puIndices;
		delete m_pxTerrainMeshData;
		m_pxTerrainMeshData = nullptr;
	}
}

void Zenith_ColliderComponent::RenderAddColliderUI()
{
	ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No collider attached");
	ImGui::Separator();

	static int s_iSelectedVolumeType = COLLISION_VOLUME_TYPE_SPHERE;
	static int s_iSelectedRigidBodyType = RIGIDBODY_TYPE_DYNAMIC;

	ImGui::Combo("Volume Type", &s_iSelectedVolumeType, s_aszVolumeTypes, s_iNumVolumeTypes);
	ImGui::Combo("Body Type", &s_iSelectedRigidBodyType, s_aszRigidBodyTypes, s_iNumRigidBodyTypes);

	if (ImGui::Button("Add Collider"))
	{
		AddCollider(static_cast<CollisionVolumeType>(s_iSelectedVolumeType),
		            static_cast<RigidBodyType>(s_iSelectedRigidBodyType));
		Zenith_Log(LOG_CATEGORY_PHYSICS, "[ColliderComponent] Added %s collider (%s)",
		           s_aszVolumeTypes[s_iSelectedVolumeType],
		           s_aszRigidBodyTypes[s_iSelectedRigidBodyType]);
	}
}

void Zenith_ColliderComponent::RenderConfiguredColliderUI()
{
	ImGui::Text("Body ID: %u", m_xBodyID.GetIndexAndSequenceNumber());

	const int iCurrentVolumeType = static_cast<int>(m_eVolumeType);
	if (iCurrentVolumeType < s_iNumVolumeTypes)
	{
		ImGui::Text("Volume Type: %s", s_aszVolumeTypes[iCurrentVolumeType]);
	}

	const int iCurrentRigidBodyType = static_cast<int>(m_eRigidBodyType);
	if (iCurrentRigidBodyType < s_iNumRigidBodyTypes)
	{
		ImGui::Text("Body Type: %s", s_aszRigidBodyTypes[iCurrentRigidBodyType]);
	}

	ImGui::Checkbox("Draw Debug Collider", &m_bDebugDrawPhysicsMesh);
	ImGui::TextDisabled("Shown only in stopped editor mode.");

	if (m_eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC)
	{
		ImGui::Separator();
		static bool s_bGravityEnabled = true;
		if (ImGui::Checkbox("Gravity Enabled", &s_bGravityEnabled))
		{
			Zenith_Physics::SetGravityEnabled(m_xBodyID, s_bGravityEnabled);
			Zenith_Log(LOG_CATEGORY_PHYSICS, "[ColliderComponent] Gravity %s", s_bGravityEnabled ? "enabled" : "disabled");
		}
	}

	if (m_pxTerrainMeshData)
	{
		ImGui::Separator();
		ImGui::Text("Terrain Mesh Collider:");
		ImGui::Text("  Vertices: %u", m_pxTerrainMeshData->m_uNumVertices);
		ImGui::Text("  Indices: %u", m_pxTerrainMeshData->m_uNumIndices);
		ImGui::Text("  Triangles: %u", m_pxTerrainMeshData->m_uNumIndices / 3);
	}

	ImGui::Separator();

	if (ImGui::TreeNode("Reconfigure Collider"))
	{
		static int s_iNewVolumeType = COLLISION_VOLUME_TYPE_SPHERE;
		static int s_iNewRigidBodyType = RIGIDBODY_TYPE_DYNAMIC;

		ImGui::Combo("New Volume Type", &s_iNewVolumeType, s_aszVolumeTypes, s_iNumVolumeTypes);
		ImGui::Combo("New Body Type", &s_iNewRigidBodyType, s_aszRigidBodyTypes, s_iNumRigidBodyTypes);

		if (ImGui::Button("Rebuild Collider"))
		{
			DestroyExistingCollider();
			AddCollider(static_cast<CollisionVolumeType>(s_iNewVolumeType),
			            static_cast<RigidBodyType>(s_iNewRigidBodyType));
			Zenith_Log(LOG_CATEGORY_PHYSICS, "[ColliderComponent] Rebuilt collider: %s (%s)",
			           s_aszVolumeTypes[s_iNewVolumeType],
			           s_aszRigidBodyTypes[s_iNewRigidBodyType]);
		}

		ImGui::TreePop();
	}

	if (ImGui::Button("Remove Collider"))
	{
		DestroyExistingCollider();
		Zenith_Log(LOG_CATEGORY_PHYSICS, "[ColliderComponent] Removed collider");
	}
}

void Zenith_ColliderComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (!m_pxRigidBody)
		RenderAddColliderUI();
	else
		RenderConfiguredColliderUI();
}

#endif // ZENITH_TOOLS
