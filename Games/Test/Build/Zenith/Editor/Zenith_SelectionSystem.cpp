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

Zenith_Entity* Zenith_SelectionSystem::RaycastSelect(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir)
{
	float closestDistance = std::numeric_limits<float>::max();
	Zenith_Entity* pxClosestEntity = nullptr;
	
	// TODO: Find the entity with the closest bounding box intersection
	//
	// ALGORITHM:
	// 1. Iterate through all cached bounding boxes (from UpdateBoundingBoxes())
	// 2. For each bounding box:
	//    a. Test ray-AABB intersection using BoundingBox::Intersects()
	//    b. If intersected, check if this is closer than previous hits
	//    c. Keep track of closest hit entity and distance
	// 3. Return closest hit entity, or nullptr if no hits
	//
	// IMPLEMENTATION:
	// for (auto& [uEntityID, xBoundingBox] : s_xEntityBoundingBoxes)
	// {
	//     float distance;
	//     if (xBoundingBox.Intersects(rayOrigin, rayDir, distance))
	//     {
	//         if (distance < closestDistance)
	//         {
	//             closestDistance = distance;
	//             // Get entity from scene by ID
	//             Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	//             Zenith_Entity xEntity = xScene.GetEntityByID(uEntityID);
	//             pxClosestEntity = ???; // How to return entity pointer?
	//         }
	//     }
	// }
	//
	// CRITICAL ISSUE: Returning pointer to local Zenith_Entity object!
	// The entity returned by GetEntityByID() is a copy, not a reference.
	// Returning a pointer to this copy will result in undefined behavior.
	//
	// POSSIBLE SOLUTIONS:
	// 1. Return Zenith_EntityID instead of pointer (RECOMMENDED)
	//    - Change function signature to return EntityID
	//    - Let caller get entity from scene using ID
	//    - More robust, no pointer lifetime issues
	//
	// 2. Store selected entity in static variable
	//    - Allocate on heap: new Zenith_Entity(xEntity)
	//    - Remember to delete previous selection
	//    - Memory management complexity
	//
	// 3. Make Zenith_Entity a handle/reference type
	//    - Redesign entity system (major change)
	//    - Entity stores SceneID + EntityID
	//    - Always valid as long as entity exists in scene
	//
	// 4. Use smart pointers (shared_ptr)
	//    - Requires changing entity storage in scene
	//    - More overhead but automatic lifetime management
	
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
				// WARNING: This leaks memory! Need to delete old selection first
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
	
	// Iterate through all mesh entries in the model
	// A model can contain multiple sub-meshes (LODs, parts, etc.)
	for (u_int i = 0; i < xModel.GetNumMeshEntires(); ++i)
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
