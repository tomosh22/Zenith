#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_SelectionSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "Maths/Zenith_Maths.h"
#include <limits>

// Bounding box implementation
bool BoundingBox::Intersects(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, float& outDistance) const
{
	// Use slab method for ray-AABB intersection
	Zenith_Maths::Vector3 invDir = 1.0f / rayDir;
	
	Zenith_Maths::Vector3 t0 = (m_xMin - rayOrigin) * invDir;
	Zenith_Maths::Vector3 t1 = (m_xMax - rayOrigin) * invDir;
	
	Zenith_Maths::Vector3 tmin = glm::min(t0, t1);
	Zenith_Maths::Vector3 tmax = glm::max(t0, t1);
	
	float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
	float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
	
	if (tNear > tFar || tFar < 0.0f)
	{
		return false;
	}
	
	outDistance = tNear > 0.0f ? tNear : tFar;
	return true;
}

bool BoundingBox::Contains(const Zenith_Maths::Vector3& point) const
{
	return point.x >= m_xMin.x && point.x <= m_xMax.x &&
	       point.y >= m_xMin.y && point.y <= m_xMax.y &&
	       point.z >= m_xMin.z && point.z <= m_xMax.z;
}

void BoundingBox::ExpandToInclude(const Zenith_Maths::Vector3& point)
{
	m_xMin = glm::min(m_xMin, point);
	m_xMax = glm::max(m_xMax, point);
}

void BoundingBox::Transform(const Zenith_Maths::Matrix4& transform)
{
	// Transform all 8 corners of the AABB and recompute bounds
	Zenith_Maths::Vector3 corners[8] = {
		Zenith_Maths::Vector3(m_xMin.x, m_xMin.y, m_xMin.z),
		Zenith_Maths::Vector3(m_xMax.x, m_xMin.y, m_xMin.z),
		Zenith_Maths::Vector3(m_xMin.x, m_xMax.y, m_xMin.z),
		Zenith_Maths::Vector3(m_xMax.x, m_xMax.y, m_xMin.z),
		Zenith_Maths::Vector3(m_xMin.x, m_xMin.y, m_xMax.z),
		Zenith_Maths::Vector3(m_xMax.x, m_xMin.y, m_xMax.z),
		Zenith_Maths::Vector3(m_xMin.x, m_xMax.y, m_xMax.z),
		Zenith_Maths::Vector3(m_xMax.x, m_xMax.y, m_xMax.z)
	};
	
	m_xMin = Zenith_Maths::Vector3(std::numeric_limits<float>::max());
	m_xMax = Zenith_Maths::Vector3(std::numeric_limits<float>::lowest());
	
	for (int i = 0; i < 8; ++i)
	{
		Zenith_Maths::Vector4 transformed = transform * Zenith_Maths::Vector4(corners[i], 1.0f);
		Zenith_Maths::Vector3 transformedPos = Zenith_Maths::Vector3(transformed) / transformed.w;
		ExpandToInclude(transformedPos);
	}
}

// Helper: Ray-triangle intersection using Möller–Trumbore algorithm
// Returns true if ray intersects triangle, with distance in outT
static bool RayTriangleIntersect(
	const Zenith_Maths::Vector3& rayOrigin,
	const Zenith_Maths::Vector3& rayDir,
	const Zenith_Maths::Vector3& v0,
	const Zenith_Maths::Vector3& v1,
	const Zenith_Maths::Vector3& v2,
	float& outT)
{
	const float EPSILON = 0.0000001f;

	Zenith_Maths::Vector3 edge1 = v1 - v0;
	Zenith_Maths::Vector3 edge2 = v2 - v0;

	Zenith_Maths::Vector3 h = Zenith_Maths::Cross(rayDir, edge2);
	float a = Zenith_Maths::Dot(edge1, h);

	// Ray is parallel to triangle
	if (a > -EPSILON && a < EPSILON)
		return false;

	float f = 1.0f / a;
	Zenith_Maths::Vector3 s = rayOrigin - v0;
	float u = f * Zenith_Maths::Dot(s, h);

	if (u < 0.0f || u > 1.0f)
		return false;

	Zenith_Maths::Vector3 q = Zenith_Maths::Cross(s, edge1);
	float v = f * Zenith_Maths::Dot(rayDir, q);

	if (v < 0.0f || u + v > 1.0f)
		return false;

	// Compute t to find intersection point
	float t = f * Zenith_Maths::Dot(edge2, q);

	if (t > EPSILON) // Ray intersection
	{
		outT = t;
		return true;
	}

	// Line intersection but not ray
	return false;
}

// Helper: Raycast against a single physics mesh
// Returns true if hit, with distance in outDistance
static bool RaycastPhysicsMesh(
	const Zenith_Maths::Vector3& rayOrigin,
	const Zenith_Maths::Vector3& rayDir,
	const Flux_MeshGeometry* pxPhysicsMesh,
	const Zenith_Maths::Matrix4& transform,
	float& outDistance)
{
	if (!pxPhysicsMesh || !pxPhysicsMesh->m_pxPositions || !pxPhysicsMesh->m_puIndices)
		return false;

	if (pxPhysicsMesh->GetNumIndices() < 3)
		return false;

	bool bHit = false;
	float fClosestDistance = std::numeric_limits<float>::max();

	const uint32_t uNumTriangles = pxPhysicsMesh->GetNumIndices() / 3;

	for (uint32_t t = 0; t < uNumTriangles; ++t)
	{
		// Get triangle indices
		uint32_t i0 = pxPhysicsMesh->m_puIndices[t * 3 + 0];
		uint32_t i1 = pxPhysicsMesh->m_puIndices[t * 3 + 1];
		uint32_t i2 = pxPhysicsMesh->m_puIndices[t * 3 + 2];

		// Get vertices in model space
		Zenith_Maths::Vector3 v0 = pxPhysicsMesh->m_pxPositions[i0];
		Zenith_Maths::Vector3 v1 = pxPhysicsMesh->m_pxPositions[i1];
		Zenith_Maths::Vector3 v2 = pxPhysicsMesh->m_pxPositions[i2];

		// Transform to world space
		Zenith_Maths::Vector4 v0World = transform * Zenith_Maths::Vector4(v0, 1.0f);
		Zenith_Maths::Vector4 v1World = transform * Zenith_Maths::Vector4(v1, 1.0f);
		Zenith_Maths::Vector4 v2World = transform * Zenith_Maths::Vector4(v2, 1.0f);

		Zenith_Maths::Vector3 v0w(v0World.x, v0World.y, v0World.z);
		Zenith_Maths::Vector3 v1w(v1World.x, v1World.y, v1World.z);
		Zenith_Maths::Vector3 v2w(v2World.x, v2World.y, v2World.z);

		// Test ray-triangle intersection
		float fDistance;
		if (RayTriangleIntersect(rayOrigin, rayDir, v0w, v1w, v2w, fDistance))
		{
			if (fDistance < fClosestDistance)
			{
				fClosestDistance = fDistance;
				bHit = true;
			}
		}
	}

	if (bHit)
	{
		outDistance = fClosestDistance;
		return true;
	}

	return false;
}

void Zenith_SelectionSystem::Initialise()
{
	m_xEntityBoundingBoxes.Clear();
}

void Zenith_SelectionSystem::Shutdown()
{
	m_xEntityBoundingBoxes.Clear();
}

void Zenith_SelectionSystem::UpdateBoundingBoxes()
{
	m_xEntityBoundingBoxes.Clear();

	// Iterate every entity in every loaded scene via TransformComponent (every
	// entity is guaranteed to have one — see EntityComponent/CLAUDE.md). This
	// makes cameras, lights, and empty transform nodes pickable too;
	// CalculateBoundingBox returns a small cube at the transform position when
	// the entity has no ModelComponent.
	Zenith_Vector<Zenith_TransformComponent*> xTransforms;
	xTransforms.Clear();
	g_xEngine.Scenes().QueryAllScenes<Zenith_TransformComponent>().ForEach([&xTransforms](Zenith_EntityID, Zenith_TransformComponent& xTransform){ xTransforms.PushBack(&xTransform); });

	for (u_int i = 0; i < xTransforms.GetSize(); ++i)
	{
		Zenith_TransformComponent* pxTransform = xTransforms.Get(i);
		Zenith_Entity xEntity = pxTransform->GetEntity();
		Zenith_EntityID uEntityID = xEntity.GetEntityID();

		BoundingBox xBoundingBox = CalculateBoundingBox(&xEntity);
		m_xEntityBoundingBoxes[uEntityID] = xBoundingBox;
	}
	
	// PERFORMANCE NOTE:
	// This is called every frame and is O(n * m) where:
	// - n = number of entities with models
	// - m = average number of vertices per model
	//
	// OPTIMIZATION OPPORTUNITIES:
	// 1. Only update bounding boxes for entities whose transform changed (dirty flagging)
	// 2. Cache model-space bounding boxes, only transform them when position changes
	// 3. Use spatial partitioning (octree/BVH) for large scenes
	// 4. Compute bounding boxes on model load, not every frame
}

BoundingBox Zenith_SelectionSystem::GetEntityBoundingBox(Zenith_Entity* pxEntity)
{
	if (!pxEntity)
	{
		return BoundingBox();
	}
	
	Zenith_EntityID uEntityID = pxEntity->GetEntityID();
	const BoundingBox* pxCached = m_xEntityBoundingBoxes.TryGet(uEntityID);
	if (pxCached)
	{
		return *pxCached;
	}
	
	// Calculate on-demand if not cached
	return CalculateBoundingBox(pxEntity);
}

bool Zenith_SelectionSystem::TestEntityHit(Zenith_ModelComponent* pxModel,
	const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir,
	float fMaxDistance,
	float& fOutDistance)
{
	Zenith_Entity xEntity = pxModel->GetParentEntity();
	const Zenith_EntityID uEntityID = xEntity.GetEntityID();

	// AABB cull: reject before touching mesh data.
	const BoundingBox* pxBox = m_xEntityBoundingBoxes.TryGet(uEntityID);
	if (pxBox)
	{
		float fBBoxDistance;
		if (!pxBox->Intersects(xRayOrigin, xRayDir, fBBoxDistance))
			return false;
		if (fBBoxDistance > fMaxDistance)
			return false;
	}

	if (!xEntity.HasComponent<Zenith_TransformComponent>())
		return false;

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Matrix4 xTransformMatrix;
	xTransform.BuildModelMatrix(xTransformMatrix);

	// Triangle-level raycast if we have a physics mesh; otherwise fall back to
	// the AABB hit we already computed.
	Flux_MeshGeometry* pxPhysicsMesh = pxModel->GetPhysicsMesh();
	if (pxPhysicsMesh)
	{
		return RaycastPhysicsMesh(xRayOrigin, xRayDir, pxPhysicsMesh, xTransformMatrix, fOutDistance);
	}

	if (!pxBox)
		return false;

	float fBBoxDistance;
	if (!pxBox->Intersects(xRayOrigin, xRayDir, fBBoxDistance))
		return false;

	fOutDistance = fBBoxDistance;
	return true;
}

Zenith_EntityID Zenith_SelectionSystem::RaycastSelect(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	float fClosestDistance = std::numeric_limits<float>::max();
	Zenith_EntityID uClosestEntityID = INVALID_ENTITY_ID;

	Zenith_Vector<Zenith_ModelComponent*> xModelComponents;
	xModelComponents.Clear();
	g_xEngine.Scenes().QueryAllScenes<Zenith_ModelComponent>().ForEach([&xModelComponents](Zenith_EntityID, Zenith_ModelComponent& xModel){ xModelComponents.PushBack(&xModel); });

	for (u_int i = 0; i < xModelComponents.GetSize(); ++i)
	{
		Zenith_ModelComponent* pxModel = xModelComponents.Get(i);
		if (!pxModel)
			continue;

		float fHitDistance;
		if (!TestEntityHit(pxModel, rayOrigin, rayDir, fClosestDistance, fHitDistance))
			continue;

		if (fHitDistance < fClosestDistance)
		{
			fClosestDistance = fHitDistance;
			uClosestEntityID = pxModel->GetParentEntity().GetEntityID();
		}
	}

	return uClosestEntityID;
}

// Default picking AABB for entities without a ModelComponent (cameras, lights,
// empty nodes). Centred on the transform position with 0.5-unit half-extents
// so the user can still click on otherwise-invisible entities.
static BoundingBox CalculateAABBForNoModelEntity(Zenith_SceneData* pxSceneData, Zenith_EntityID xEntityID)
{
	Zenith_TransformComponent& xTransform =
		pxSceneData->GetEntity(xEntityID).GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);
	constexpr float fHalfExtent = 0.5f;
	BoundingBox xBox;
	xBox.m_xMin = xPos - Zenith_Maths::Vector3(fHalfExtent);
	xBox.m_xMax = xPos + Zenith_Maths::Vector3(fHalfExtent);
	return xBox;
}

// Compute model-space min/max from a ModelComponent. Prefers the physics
// mesh (raycast-optimised), falls back to per-sub-mesh asset bounds, and
// last-resort iterates procedural-mesh vertex positions directly.
static void CalculateModelSpaceBounds(Zenith_ModelComponent& xModel,
	Zenith_Maths::Vector3& xMinOut, Zenith_Maths::Vector3& xMaxOut)
{
	xMinOut = Zenith_Maths::Vector3(std::numeric_limits<float>::max());
	xMaxOut = Zenith_Maths::Vector3(std::numeric_limits<float>::lowest());

	// Physics mesh is optimised for raycasting and provides better selection accuracy.
	Flux_MeshGeometry* pxPhysicsMesh = xModel.GetPhysicsMesh();
	if (pxPhysicsMesh && pxPhysicsMesh->m_pxPositions && pxPhysicsMesh->GetNumVerts() > 0)
	{
		const Zenith_Maths::Vector3* pPositions = pxPhysicsMesh->m_pxPositions;
		const u_int uVertexCount = pxPhysicsMesh->GetNumVerts();
		for (u_int v = 0; v < uVertexCount; ++v)
		{
			xMinOut = glm::min(xMinOut, pPositions[v]);
			xMaxOut = glm::max(xMaxOut, pPositions[v]);
		}
		return;
	}

	if (!xModel.GetModelInstance()) return;

	// Use each sub-mesh's bounds: asset-backed meshes have pre-computed
	// bind-pose bounds (good enough for editor picking on skinned meshes);
	// procedurally-built meshes wrap a Flux_MeshGeometry whose positions we
	// iterate directly.
	Flux_ModelInstance* pxModelInst = xModel.GetModelInstance();
	for (u_int u = 0; u < pxModelInst->GetNumMeshes(); ++u)
	{
		Flux_MeshInstance* pxMeshInst = pxModelInst->GetMeshInstance(u);
		if (!pxMeshInst) continue;

		if (Zenith_MeshAsset* pxMeshAsset = pxMeshInst->GetSourceAsset())
		{
			xMinOut = glm::min(xMinOut, pxMeshAsset->m_xBoundsMin);
			xMaxOut = glm::max(xMaxOut, pxMeshAsset->m_xBoundsMax);
			continue;
		}

		if (const Flux_MeshGeometry* pxGeometry = pxMeshInst->GetProceduralGeometry())
		{
			const Zenith_Maths::Vector3* pPositions = pxGeometry->m_pxPositions;
			const u_int uVertexCount = pxGeometry->GetNumVerts();
			if (!pPositions || uVertexCount == 0) continue;
			for (u_int v = 0; v < uVertexCount; ++v)
			{
				xMinOut = glm::min(xMinOut, pPositions[v]);
				xMaxOut = glm::max(xMaxOut, pPositions[v]);
			}
		}
	}
}

BoundingBox Zenith_SelectionSystem::CalculateBoundingBox(Zenith_Entity* pxEntity)
{
	BoundingBox xBoundingBox;

	if (!pxEntity) return xBoundingBox;

	// Find the scene that owns this entity (not just active scene)
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(pxEntity->GetEntityID());
	if (!pxSceneData) return xBoundingBox;

	const Zenith_EntityID xEntityID = pxEntity->GetEntityID();

	// Entities without a ModelComponent (cameras, lights, empty nodes) get a
	// small picking cube centered at their transform position. Every entity is
	// guaranteed to have a TransformComponent, so no further check is needed.
	if (!pxSceneData->EntityHasComponent<Zenith_ModelComponent>(xEntityID))
	{
		return CalculateAABBForNoModelEntity(pxSceneData, xEntityID);
	}

	Zenith_ModelComponent& xModel = pxSceneData->GetEntity(xEntityID).GetComponent<Zenith_ModelComponent>();
	Zenith_Maths::Vector3 xMin, xMax;
	CalculateModelSpaceBounds(xModel, xMin, xMax);
	xBoundingBox.m_xMin = xMin;
	xBoundingBox.m_xMax = xMax;

	// Apply entity transform to convert from model space to world space.
	// After rotation the AABB may be larger than the OBB — accepted trade-off
	// for cheaper intersection tests.
	if (pxSceneData->EntityHasComponent<Zenith_TransformComponent>(xEntityID))
	{
		Zenith_TransformComponent& xTransform = pxSceneData->GetEntity(xEntityID).GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Matrix4 xTransformMatrix;
		xTransform.BuildModelMatrix(xTransformMatrix);
		xBoundingBox.Transform(xTransformMatrix);
	}

	return xBoundingBox;
}

#endif // ZENITH_TOOLS
