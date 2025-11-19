/**
 * Terrain Frustum Culling - Example Usage
 *
 * This file demonstrates how to use the terrain frustum culling system
 * in both CPU and GPU modes.
 *
 * To use this code:
 * 1. Copy the relevant sections to your game/editor code
 * 2. Adapt to your specific rendering architecture
 * 3. Enable GPU culling when you have 1000+ terrain components
 */

#include "Zenith.h"
#include "Flux/Terrain/Flux_TerrainCulling.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"

// ============================================================================
// Example 1: Basic CPU Frustum Culling
// ============================================================================

void Example_BasicCPUCulling()
{
	// Get all terrain in the scene
	Zenith_Vector<Zenith_TerrainComponent*> xAllTerrain;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xAllTerrain);

	// Ensure all terrain has valid AABBs (only needed once per terrain)
	for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xAllTerrain); !xIt.Done(); xIt.Next())
	{
		Zenith_TerrainComponent* pxTerrain = xIt.GetData();
		if (!pxTerrain->HasValidAABB())
		{
			Zenith_AABB xAABB = Flux_TerrainCulling::GenerateTerrainAABB(*pxTerrain);
			pxTerrain->SetAABB(xAABB);
		}
	}

	// Get the main camera
	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	// Perform frustum culling (this tests all AABBs against the camera frustum)
	Flux_TerrainCulling::PerformCulling(xCamera, xAllTerrain);

	// Get only the visible terrain
	const Zenith_Vector<Zenith_TerrainComponent*>& xVisibleTerrain =
		Flux_TerrainCulling::GetVisibleTerrainComponents();

	// Render only visible terrain
	for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xVisibleTerrain); !xIt.Done(); xIt.Next())
	{
		Zenith_TerrainComponent* pxTerrain = xIt.GetData();

		// Submit draw commands for this terrain
		// ... your rendering code here ...
	}

	// Check culling statistics
	const Flux_TerrainCulling::CullingStats& xStats = Flux_TerrainCulling::GetCullingStats();
	Zenith_Log("Terrain culling: %u visible, %u culled (%.1f%% reduction)",
		xStats.m_uVisibleTerrain,
		xStats.m_uCulledTerrain,
		(xStats.m_uCulledTerrain * 100.0f) / xStats.m_uTotalTerrain);
}

// ============================================================================
// Example 2: GPU Frustum Culling (for large terrain counts)
// ============================================================================

void Example_GPUCulling()
{
	// Enable GPU culling (do this once at startup or when terrain count is high)
	// GPU culling is beneficial when you have 1000+ terrain components
	const uint32_t uTerrainCount = 1500;
	if (uTerrainCount >= 1000)
	{
		Flux_TerrainCulling::SetGPUCullingEnabled(true);
	}

	// Get all terrain
	Zenith_Vector<Zenith_TerrainComponent*> xAllTerrain;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xAllTerrain);

	// Ensure AABBs are cached
	for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xAllTerrain); !xIt.Done(); xIt.Next())
	{
		Zenith_TerrainComponent* pxTerrain = xIt.GetData();
		if (!pxTerrain->HasValidAABB())
		{
			Zenith_AABB xAABB = Flux_TerrainCulling::GenerateTerrainAABB(*pxTerrain);
			pxTerrain->SetAABB(xAABB);
		}
	}

	// Get camera
	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	// Perform culling (GPU or CPU depending on enabled state)
	Flux_TerrainCulling::PerformCulling(xCamera, xAllTerrain);

	// For GPU culling, use indirect draw
	// For CPU culling, iterate the visible list
	if (Flux_TerrainCulling::IsGPUCullingEnabled())
	{
		// GPU path: Submit indirect draw commands
		// The GPU compute shader has already determined which terrain is visible
		// and filled the indirect draw buffer

		Flux_CommandList xCommandList("Terrain Rendering");
		// ... set up pipeline, bind resources ...

		// This call submits draw commands for all visible terrain using GPU-generated
		// indirect draw buffer (no CPU iteration!)
		Flux_TerrainCulling::SubmitGPUCulledDraws(xCommandList);
	}
	else
	{
		// CPU path: Iterate visible terrain list
		const Zenith_Vector<Zenith_TerrainComponent*>& xVisibleTerrain =
			Flux_TerrainCulling::GetVisibleTerrainComponents();

		for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xVisibleTerrain); !xIt.Done(); xIt.Next())
		{
			Zenith_TerrainComponent* pxTerrain = xIt.GetData();
			// ... submit draw commands ...
		}
	}
}

// ============================================================================
// Example 3: Manual AABB Testing (for custom culling)
// ============================================================================

void Example_ManualAABBTesting()
{
	// You can also manually test AABBs against the frustum
	// This is useful for custom culling or when you want to test
	// arbitrary objects (not just terrain)

	// Get camera and extract frustum
	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	Zenith_Maths::Matrix4 xView, xProj;
	xCamera.BuildViewMatrix(xView);
	xCamera.BuildProjectionMatrix(xProj);
	Zenith_Maths::Matrix4 xViewProj = xProj * xView;

	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProj);

	// Create a test AABB
	Zenith_AABB xTestAABB;
	xTestAABB.m_xMin = Zenith_Maths::Vector3(-10.0f, 0.0f, -10.0f);
	xTestAABB.m_xMax = Zenith_Maths::Vector3(10.0f, 5.0f, 10.0f);

	// Test if the AABB is visible
	bool bIsVisible = Zenith_FrustumCulling::TestAABBFrustum(xFrustum, xTestAABB);

	if (bIsVisible)
	{
		Zenith_Log("AABB is visible!");
		// Render the object...
	}
	else
	{
		Zenith_Log("AABB is culled (outside frustum)");
		// Skip rendering
	}
}

// ============================================================================
// Example 4: AABB Generation from Mesh
// ============================================================================

void Example_AABBGeneration()
{
	// Generate AABB from a mesh's vertex positions
	Flux_MeshGeometry xMesh;
	// ... load mesh ...

	if (xMesh.m_pxPositions != nullptr && xMesh.GetNumVerts() > 0)
	{
		Zenith_AABB xAABB = Zenith_FrustumCulling::GenerateAABBFromVertices(
			xMesh.m_pxPositions,
			xMesh.GetNumVerts()
		);

		Zenith_Log("Mesh AABB: min(%.2f, %.2f, %.2f) max(%.2f, %.2f, %.2f)",
			xAABB.m_xMin.x, xAABB.m_xMin.y, xAABB.m_xMin.z,
			xAABB.m_xMax.x, xAABB.m_xMax.y, xAABB.m_xMax.z);

		Zenith_Maths::Vector3 xCenter = xAABB.GetCenter();
		Zenith_Maths::Vector3 xExtents = xAABB.GetExtents();

		Zenith_Log("  Center: (%.2f, %.2f, %.2f)", xCenter.x, xCenter.y, xCenter.z);
		Zenith_Log("  Extents: (%.2f, %.2f, %.2f)", xExtents.x, xExtents.y, xExtents.z);
	}
}

// ============================================================================
// Example 5: Transform AABB (for dynamic objects)
// ============================================================================

void Example_TransformAABB()
{
	// If you have a dynamic object that moves/rotates, you need to transform its AABB

	// Original AABB (in local space)
	Zenith_AABB xLocalAABB;
	xLocalAABB.m_xMin = Zenith_Maths::Vector3(-1.0f, -1.0f, -1.0f);
	xLocalAABB.m_xMax = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

	// World transform matrix (position, rotation, scale)
	Zenith_Maths::Matrix4 xWorldTransform = glm::translate(Zenith_Maths::Vector3(100, 0, 50)) *
		glm::rotate(glm::radians(45.0f), Zenith_Maths::Vector3(0, 1, 0)) *
		glm::scale(Zenith_Maths::Vector3(2, 2, 2));

	// Transform the AABB to world space
	Zenith_AABB xWorldAABB = Zenith_FrustumCulling::TransformAABB(xLocalAABB, xWorldTransform);

	// Now test the world-space AABB against the frustum
	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
	Zenith_Maths::Matrix4 xView, xProj;
	xCamera.BuildViewMatrix(xView);
	xCamera.BuildProjectionMatrix(xProj);

	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xProj * xView);

	bool bIsVisible = Zenith_FrustumCulling::TestAABBFrustum(xFrustum, xWorldAABB);

	Zenith_Log("Transformed object is %s", bIsVisible ? "visible" : "culled");
}

// ============================================================================
// Example 6: Debugging and Profiling
// ============================================================================

void Example_DebuggingAndProfiling()
{
	// The culling system provides statistics for debugging and profiling

	// Perform culling as usual
	Zenith_Vector<Zenith_TerrainComponent*> xAllTerrain;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xAllTerrain);

	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
	Flux_TerrainCulling::PerformCulling(xCamera, xAllTerrain);

	// Get statistics
	const Flux_TerrainCulling::CullingStats& xStats = Flux_TerrainCulling::GetCullingStats();

	Zenith_Log("=== Terrain Culling Stats ===");
	Zenith_Log("Total terrain: %u", xStats.m_uTotalTerrain);
	Zenith_Log("Visible terrain: %u (%.1f%%)",
		xStats.m_uVisibleTerrain,
		(xStats.m_uVisibleTerrain * 100.0f) / xStats.m_uTotalTerrain);
	Zenith_Log("Culled terrain: %u (%.1f%%)",
		xStats.m_uCulledTerrain,
		(xStats.m_uCulledTerrain * 100.0f) / xStats.m_uTotalTerrain);
	Zenith_Log("Culling time: %.3f ms", xStats.m_fCullingTimeMS);
	Zenith_Log("Method: %s", xStats.m_bUsedGPUCulling ? "GPU" : "CPU");

	// Access the frustum for debugging
	const Zenith_Frustum& xFrustum = Flux_TerrainCulling::GetCurrentFrustum();

	// Print frustum planes
	const char* planeNames[] = { "Left", "Right", "Bottom", "Top", "Near", "Far" };
	for (int i = 0; i < 6; ++i)
	{
		const Zenith_Plane& xPlane = xFrustum.m_axPlanes[i];
		Zenith_Log("%s plane: normal(%.2f, %.2f, %.2f) distance=%.2f",
			planeNames[i],
			xPlane.m_xNormal.x, xPlane.m_xNormal.y, xPlane.m_xNormal.z,
			xPlane.m_fDistance);
	}
}

// ============================================================================
// Example 7: Complete Integration in Render Loop
// ============================================================================

void Example_CompleteIntegration()
{
	// This shows how to integrate the culling system into your main render loop

	// === INITIALIZATION (once at startup) ===
	Flux_TerrainCulling::Initialise();

	// Optionally enable GPU culling for large scenes
	// You can toggle this at runtime based on terrain count
	#if 0
	Flux_TerrainCulling::SetGPUCullingEnabled(true);
	#endif

	// === EACH FRAME IN RENDER LOOP ===

	// 1. Get all terrain components
	Zenith_Vector<Zenith_TerrainComponent*> xAllTerrain;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xAllTerrain);

	// 2. Ensure AABBs are cached (only generates on first access)
	for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xAllTerrain); !xIt.Done(); xIt.Next())
	{
		Zenith_TerrainComponent* pxTerrain = xIt.GetData();
		if (!pxTerrain->HasValidAABB())
		{
			Zenith_AABB xAABB = Flux_TerrainCulling::GenerateTerrainAABB(*pxTerrain);
			pxTerrain->SetAABB(xAABB);
		}
	}

	// 3. Perform frustum culling
	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
	Flux_TerrainCulling::PerformCulling(xCamera, xAllTerrain);

	// 4. Render visible terrain
	const Zenith_Vector<Zenith_TerrainComponent*>& xVisibleTerrain =
		Flux_TerrainCulling::GetVisibleTerrainComponents();

	// Multi-threaded rendering using task system
	const uint32_t uNumTasks = 4;
	const uint32_t uVisibleCount = xVisibleTerrain.GetSize();
	const uint32_t uTerrainsPerTask = (uVisibleCount + uNumTasks - 1) / uNumTasks;

	for (uint32_t uTaskIndex = 0; uTaskIndex < uNumTasks; ++uTaskIndex)
	{
		const uint32_t uStartIndex = uTaskIndex * uTerrainsPerTask;
		const uint32_t uEndIndex = (uStartIndex + uTerrainsPerTask < uVisibleCount) ?
			uStartIndex + uTerrainsPerTask : uVisibleCount;

		if (uStartIndex >= uVisibleCount)
			break;

		// Submit task to render terrain range [uStartIndex, uEndIndex)
		for (uint32_t u = uStartIndex; u < uEndIndex; ++u)
		{
			Zenith_TerrainComponent* pxTerrain = xVisibleTerrain.Get(u);

			// Submit draw commands
			// ... your rendering code here ...
		}
	}

	// 5. Optional: Log stats periodically
	static uint32_t uFrameCounter = 0;
	if ((++uFrameCounter % 60) == 0)  // Every 60 frames
	{
		const Flux_TerrainCulling::CullingStats& xStats = Flux_TerrainCulling::GetCullingStats();
		Zenith_Log("Frame %u: %u/%u terrain visible (%.1f%% culled)",
			uFrameCounter,
			xStats.m_uVisibleTerrain,
			xStats.m_uTotalTerrain,
			(xStats.m_uCulledTerrain * 100.0f) / xStats.m_uTotalTerrain);
	}
}

// ============================================================================
// Example 8: Performance Comparison
// ============================================================================

void Example_PerformanceComparison()
{
	// Compare CPU vs GPU culling performance

	Zenith_Vector<Zenith_TerrainComponent*> xAllTerrain;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xAllTerrain);

	const Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	// Ensure AABBs are cached
	for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xAllTerrain); !xIt.Done(); xIt.Next())
	{
		Zenith_TerrainComponent* pxTerrain = xIt.GetData();
		if (!pxTerrain->HasValidAABB())
		{
			Zenith_AABB xAABB = Flux_TerrainCulling::GenerateTerrainAABB(*pxTerrain);
			pxTerrain->SetAABB(xAABB);
		}
	}

	// Test CPU culling
	{
		Flux_TerrainCulling::SetGPUCullingEnabled(false);

		auto startTime = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < 100; ++i)
		{
			Flux_TerrainCulling::PerformCulling(xCamera, xAllTerrain);
		}

		auto endTime = std::chrono::high_resolution_clock::now();
		float fCPUTimeMS = std::chrono::duration<float, std::milli>(endTime - startTime).count() / 100.0f;

		Zenith_Log("CPU culling: %.3f ms average", fCPUTimeMS);
	}

	// Test GPU culling
	{
		Flux_TerrainCulling::SetGPUCullingEnabled(true);

		auto startTime = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < 100; ++i)
		{
			Flux_TerrainCulling::PerformCulling(xCamera, xAllTerrain);
		}

		auto endTime = std::chrono::high_resolution_clock::now();
		float fGPUTimeMS = std::chrono::duration<float, std::milli>(endTime - startTime).count() / 100.0f;

		Zenith_Log("GPU culling: %.3f ms average", fGPUTimeMS);
	}

	Zenith_Log("Recommendation: Use %s culling for %u terrain",
		xAllTerrain.GetSize() > 1000 ? "GPU" : "CPU",
		xAllTerrain.GetSize());
}
