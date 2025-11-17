#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_SelectionSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
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

// Selection system implementation
static std::unordered_map<Zenith_EntityID, BoundingBox> s_xEntityBoundingBoxes;
static Zenith_Entity* s_pxSelectedEntity = nullptr;

void Zenith_SelectionSystem::Initialise()
{
	s_xEntityBoundingBoxes.clear();
	s_pxSelectedEntity = nullptr;
}

void Zenith_SelectionSystem::Shutdown()
{
	s_xEntityBoundingBoxes.clear();
	s_pxSelectedEntity = nullptr;
}

void Zenith_SelectionSystem::UpdateBoundingBoxes()
{
	s_xEntityBoundingBoxes.clear();
	
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	
	// Get all entities with model components
	Zenith_Vector<Zenith_ModelComponent*> xModelComponents;
	xScene.GetAllOfComponentType<Zenith_ModelComponent>(xModelComponents);
	
	for (u_int i = 0; i < xModelComponents.GetSize(); ++i)
	{
		Zenith_ModelComponent* pxModel = xModelComponents.Get(i);
		Zenith_Entity xEntity = pxModel->GetParentEntity();
		Zenith_EntityID uEntityID = xEntity.GetEntityID();
		
		BoundingBox xBoundingBox = CalculateBoundingBox(&xEntity);
		s_xEntityBoundingBoxes[uEntityID] = xBoundingBox;
	}
}

BoundingBox Zenith_SelectionSystem::GetEntityBoundingBox(Zenith_Entity* pxEntity)
{
	if (!pxEntity)
	{
		return BoundingBox();
	}
	
	Zenith_EntityID uEntityID = pxEntity->GetEntityID();
	auto it = s_xEntityBoundingBoxes.find(uEntityID);
	if (it != s_xEntityBoundingBoxes.end())
	{
		return it->second;
	}
	
	// Calculate on-demand if not cached
	return CalculateBoundingBox(pxEntity);
}

Zenith_Entity* Zenith_SelectionSystem::RaycastSelect(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	float closestDistance = std::numeric_limits<float>::max();
	Zenith_Entity* pxClosestEntity = nullptr;
	
	// Check all cached bounding boxes
	for (auto& [uEntityID, xBoundingBox] : s_xEntityBoundingBoxes)
	{
		float distance;
		if (xBoundingBox.Intersects(rayOrigin, rayDir, distance))
		{
			if (distance < closestDistance)
			{
				closestDistance = distance;
				Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
				Zenith_Entity xEntity = xScene.GetEntityByID(uEntityID);
				pxClosestEntity = new Zenith_Entity(xEntity);  // Allocate entity on heap to return pointer
			}
		}
	}
	
	if (pxClosestEntity)
	{
		s_pxSelectedEntity = pxClosestEntity;
	}
	
	return pxClosestEntity;
}

BoundingBox Zenith_SelectionSystem::CalculateBoundingBox(Zenith_Entity* pxEntity)
{
	BoundingBox xBoundingBox;
	
	if (!pxEntity)
	{
		return xBoundingBox;
	}
	
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	
	// Check if entity has a model component
	if (!xScene.EntityHasComponent<Zenith_ModelComponent>(pxEntity->GetEntityID()))
	{
		return xBoundingBox;
	}
	
	Zenith_ModelComponent& xModel = xScene.GetComponentFromEntity<Zenith_ModelComponent>(pxEntity->GetEntityID());
	
	// Initialize min/max to extreme values
	Zenith_Maths::Vector3 xMin(std::numeric_limits<float>::max());
	Zenith_Maths::Vector3 xMax(std::numeric_limits<float>::lowest());
	
	// Iterate through all mesh entries in the model
	for (u_int i = 0; i < xModel.GetNumMeshEntires(); ++i)
	{
		Flux_MeshGeometry& xGeometry = xModel.GetMeshGeometryAtIndex(i);
		
		// Use the position array directly
		const Zenith_Maths::Vector3* pPositions = xGeometry.m_pxPositions;
		const u_int uVertexCount = xGeometry.GetNumVerts();
		
		if (!pPositions || uVertexCount == 0)
		{
			continue;
		}
		
		// Find min/max from positions
		for (u_int v = 0; v < uVertexCount; ++v)
		{
			xMin = glm::min(xMin, pPositions[v]);
			xMax = glm::max(xMax, pPositions[v]);
		}
	}
	
	// Apply entity transform
	if (xScene.EntityHasComponent<Zenith_TransformComponent>(pxEntity->GetEntityID()))
	{
		Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(pxEntity->GetEntityID());
		Zenith_Maths::Matrix4 xTransformMatrix;
		xTransform.BuildModelMatrix(xTransformMatrix);
		
		xBoundingBox.m_xMin = xMin;
		xBoundingBox.m_xMax = xMax;
		xBoundingBox.Transform(xTransformMatrix);
	}
	else
	{
		xBoundingBox.m_xMin = xMin;
		xBoundingBox.m_xMax = xMax;
	}
	
	return xBoundingBox;
}

void Zenith_SelectionSystem::RenderBoundingBoxes()
{
	// TODO: Implement debug rendering of all bounding boxes
	// This will require integration with the rendering system to draw wireframe boxes
}

void Zenith_SelectionSystem::RenderSelectedBoundingBox(Zenith_Entity* pxEntity)
{
	// TODO: Implement debug rendering of selected entity's bounding box
	// This will require integration with the rendering system to draw a highlighted wireframe box
}

#endif // ZENITH_TOOLS
