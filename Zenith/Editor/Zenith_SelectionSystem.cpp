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
	s_xEntityBoundingBoxes.clear();
}

void Zenith_SelectionSystem::Shutdown()
{
	s_xEntityBoundingBoxes.clear();
}

void Zenith_SelectionSystem::UpdateBoundingBoxes()
{
	s_xEntityBoundingBoxes.clear();
	
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	
	// Get all entities with model components
	// These are the entities that can be visually picked by the user
	Zenith_Vector<Zenith_ModelComponent*> xModelComponents;
	xScene.GetAllOfComponentType<Zenith_ModelComponent>(xModelComponents);
	
	// TODO: Also handle entities without models but with other pickable components
	// For example: cameras, lights, empty transform nodes, etc.
	// These would need default bounding boxes (small cube at transform position)
	
	for (u_int i = 0; i < xModelComponents.GetSize(); ++i)
	{
		Zenith_ModelComponent* pxModel = xModelComponents.Get(i);
		Zenith_Entity xEntity = pxModel->GetParentEntity();
		Zenith_EntityID uEntityID = xEntity.GetEntityID();
		
		BoundingBox xBoundingBox = CalculateBoundingBox(&xEntity);
		s_xEntityBoundingBoxes[uEntityID] = xBoundingBox;
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
	auto it = s_xEntityBoundingBoxes.find(uEntityID);
	if (it != s_xEntityBoundingBoxes.end())
	{
		return it->second;
	}
	
	// Calculate on-demand if not cached
	return CalculateBoundingBox(pxEntity);
}

Zenith_EntityID Zenith_SelectionSystem::RaycastSelect(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	float fClosestDistance = std::numeric_limits<float>::max();
	Zenith_EntityID uClosestEntityID = INVALID_ENTITY_ID;

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Get all model components for detailed raycasting
	Zenith_Vector<Zenith_ModelComponent*> xModelComponents;
	xScene.GetAllOfComponentType<Zenith_ModelComponent>(xModelComponents);

	for (u_int i = 0; i < xModelComponents.GetSize(); ++i)
	{
		Zenith_ModelComponent* pxModel = xModelComponents.Get(i);
		if (!pxModel)
			continue;

		Zenith_Entity xEntity = pxModel->GetParentEntity();
		Zenith_EntityID uEntityID = xEntity.GetEntityID();

		// First, do a quick AABB test to cull entities
		auto it = s_xEntityBoundingBoxes.find(uEntityID);
		if (it != s_xEntityBoundingBoxes.end())
		{
			float fBBoxDistance;
			if (!it->second.Intersects(rayOrigin, rayDir, fBBoxDistance))
			{
				// AABB miss - skip detailed test
				continue;
			}

			// AABB hit - early out if already further than closest hit
			if (fBBoxDistance > fClosestDistance)
				continue;
		}

		// Get transform matrix for this entity
		if (!xEntity.HasComponent<Zenith_TransformComponent>())
			continue;

		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Matrix4 xTransformMatrix;
		xTransform.BuildModelMatrix(xTransformMatrix);

		// Detailed triangle-level raycast against physics mesh
		Flux_MeshGeometry* pxPhysicsMesh = pxModel->GetPhysicsMesh();
		if (pxPhysicsMesh)
		{
			float fHitDistance;
			if (RaycastPhysicsMesh(rayOrigin, rayDir, pxPhysicsMesh, xTransformMatrix, fHitDistance))
			{
				if (fHitDistance < fClosestDistance)
				{
					fClosestDistance = fHitDistance;
					uClosestEntityID = uEntityID;
				}
			}
		}
		else
		{
			// Fallback: Use AABB-only selection if no physics mesh
			// (Already passed AABB test above)
			auto it = s_xEntityBoundingBoxes.find(uEntityID);
			if (it != s_xEntityBoundingBoxes.end())
			{
				float fBBoxDistance;
				if (it->second.Intersects(rayOrigin, rayDir, fBBoxDistance))
				{
					if (fBBoxDistance < fClosestDistance)
					{
						fClosestDistance = fBBoxDistance;
						uClosestEntityID = uEntityID;
					}
				}
			}
		}
	}

	return uClosestEntityID;
}

BoundingBox Zenith_SelectionSystem::CalculateBoundingBox(Zenith_Entity* pxEntity)
{
	BoundingBox xBoundingBox;

	if (!pxEntity)
		return xBoundingBox;

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Check if entity has a model component
	if (!xScene.EntityHasComponent<Zenith_ModelComponent>(pxEntity->GetEntityID()))
	{
		// TODO: Handle entities without models
		// Create default bounding box for non-renderable entities
		// This allows picking cameras, lights, empty nodes, etc.
		//
		// APPROACH:
		// 1. Check if entity has TransformComponent
		// 2. If yes, create small cube (e.g., 1 unit) centered at position
		// 3. If no transform, return empty/invalid bounding box
		//
		// EXAMPLE:
		// if (xScene.EntityHasComponent<Zenith_TransformComponent>(pxEntity->GetEntityID()))
		// {
		//     auto& transform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(pxEntity->GetEntityID());
		//     Vector3 pos;
		//     transform.GetPosition(pos);
		//     xBoundingBox.m_xMin = pos - Vector3(0.5f);
		//     xBoundingBox.m_xMax = pos + Vector3(0.5f);
		// }

		return xBoundingBox;
	}

	Zenith_ModelComponent& xModel = xScene.GetComponentFromEntity<Zenith_ModelComponent>(pxEntity->GetEntityID());

	// Initialize min/max to extreme values
	// These will be updated as we process vertices
	Zenith_Maths::Vector3 xMin(std::numeric_limits<float>::max());
	Zenith_Maths::Vector3 xMax(std::numeric_limits<float>::lowest());

	// CRITICAL: Use physics mesh for selection if available
	// Physics mesh is optimized for raycasting and provides better selection accuracy
	Flux_MeshGeometry* pxPhysicsMesh = xModel.GetPhysicsMesh();

	if (pxPhysicsMesh && pxPhysicsMesh->m_pxPositions && pxPhysicsMesh->GetNumVerts() > 0)
	{
		// Use physics mesh for bounding box calculation
		const Zenith_Maths::Vector3* pPositions = pxPhysicsMesh->m_pxPositions;
		const u_int uVertexCount = pxPhysicsMesh->GetNumVerts();

		for (u_int v = 0; v < uVertexCount; ++v)
		{
			xMin = glm::min(xMin, pPositions[v]);
			xMax = glm::max(xMax, pPositions[v]);
		}
	}
	else
	{
		// Fallback: Use render mesh if no physics mesh available
		// Iterate through all mesh entries in the model
		// A model can contain multiple sub-meshes (LODs, parts, etc.)
		for (u_int i = 0; i < xModel.GetNumMeshEntries(); ++i)
		{
			Flux_MeshGeometry& xGeometry = xModel.GetMeshGeometryAtIndex(i);

			// Use the position array directly
			// This is in model/local space, not world space
			const Zenith_Maths::Vector3* pPositions = xGeometry.m_pxPositions;
			const u_int uVertexCount = xGeometry.GetNumVerts();

			if (!pPositions || uVertexCount == 0)
				continue;

			// Find min/max from positions
			// This creates an axis-aligned bounding box in model space
			for (u_int v = 0; v < uVertexCount; ++v)
			{
				xMin = glm::min(xMin, pPositions[v]);
				xMax = glm::max(xMax, pPositions[v]);
			}
		}
	}
	
	// Apply entity transform to convert from model space to world space
	// The entity's transform may include translation, rotation, and scale
	if (xScene.EntityHasComponent<Zenith_TransformComponent>(pxEntity->GetEntityID()))
	{
		Zenith_TransformComponent& xTransform = xScene.GetComponentFromEntity<Zenith_TransformComponent>(pxEntity->GetEntityID());
		Zenith_Maths::Matrix4 xTransformMatrix;
		xTransform.BuildModelMatrix(xTransformMatrix);
		
		// Transform the bounding box
		// This will transform all 8 corners and recompute axis-aligned bounds
		// Note: After rotation, the AABB may be larger than the oriented bounding box
		xBoundingBox.m_xMin = xMin;
		xBoundingBox.m_xMax = xMax;
		xBoundingBox.Transform(xTransformMatrix);
		
		// OPTIMIZATION NOTE:
		// For better picking accuracy, could use Oriented Bounding Box (OBB)
		// instead of AABB, which doesn't expand when rotated
		// Trade-off: OBB intersection test is more expensive
	}
	else
	{
		// No transform - use model-space bounds
		xBoundingBox.m_xMin = xMin;
		xBoundingBox.m_xMax = xMax;
	}
	
	return xBoundingBox;
}

void Zenith_SelectionSystem::RenderBoundingBoxes()
{
	// TODO: Implement debug rendering of all bounding boxes
	// This is useful for debugging selection and seeing entity bounds
	//
	// IMPLEMENTATION APPROACH:
	// 1. Iterate through s_xEntityBoundingBoxes map
	// 2. For each bounding box:
	//    a. Get the 8 corners of the box
	//    b. Draw 12 edges as lines (wireframe cube)
	//    c. Use debug color (e.g., yellow for all, green for selected)
	//
	// RENDERING OPTIONS:
	// 
	// OPTION 1: ImGui Overlay (Simplest for prototyping)
	// - Project each corner to screen space using camera matrices
	// - Use ImGui::GetForegroundDrawList()->AddLine()
	// - Pros: Easy to implement, no 3D rendering setup
	// - Cons: Lines don't respect depth, may look confusing
	//
	// OPTION 2: Debug Line Renderer (Better)
	// - Integrate with or create a debug line rendering system
	// - Submit line primitives to rendering pipeline
	// - Render with depth testing in 3D
	// - Pros: Proper 3D visualization
	// - Cons: Requires debug rendering infrastructure
	//
	// OPTION 3: Flux Command List (Most integrated)
	// - Use Flux command list to submit line geometry
	// - Create simple line shader if needed
	// - Pros: Integrates with existing rendering system
	// - Cons: More complex setup
	//
	// RECOMMENDED: Start with ImGui overlay, migrate to debug renderer later
}

void Zenith_SelectionSystem::RenderSelectedBoundingBox(Zenith_Entity* pxEntity)
{
	// TODO: Implement debug rendering of selected entity's bounding box
	// Similar to RenderBoundingBoxes() but only for one entity
	//
	// IMPLEMENTATION:
	// 1. Get bounding box for selected entity
	// 2. Render wireframe box with highlight color (e.g., yellow/orange)
	// 3. Optionally make lines thicker for visibility
	//
	// USAGE:
	// Call from Zenith_Editor::RenderGizmos() after gizmo rendering
	// to provide visual feedback of selected object
	//
	// EXAMPLE PSEUDO-CODE:
	// if (!pxEntity) return;
	// BoundingBox box = GetEntityBoundingBox(pxEntity);
	// 
	// // Get 8 corners
	// Vector3 corners[8] = {
	//     {box.m_xMin.x, box.m_xMin.y, box.m_xMin.z},
	//     {box.m_xMax.x, box.m_xMin.y, box.m_xMin.z},
	//     // ... other 6 corners
	// };
	//
	// // Draw 12 edges
	// DrawLine3D(corners[0], corners[1], Color::Yellow);
	// // ... other 11 edges
}

#endif // ZENITH_TOOLS
