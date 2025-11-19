#include "Zenith.h"

#include "Flux/Terrain/Flux_TerrainCulling.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"

#include <chrono>

// GPU culling headers - only include when GPU culling is enabled
// #include "Vulkan/Zenith_Vulkan_Pipeline.h"
// #include "Vulkan/Zenith_Vulkan_Shader.h"
// #include "Vulkan/Zenith_Vulkan_MemoryManager.h"

// =============================================================================
// Internal Data Structures
// =============================================================================

namespace
{
	// Maximum number of terrain components we can handle
	constexpr uint32_t MAX_TERRAIN_COMPONENTS = 4096;

	// GPU data structures (must match shader layout)
	struct GPU_TerrainAABB
	{
		Zenith_Maths::Vector4 m_xMinAndIndex; // xyz = min, w = terrain index
		Zenith_Maths::Vector4 m_xMax;          // xyz = max, w = unused
	};

	struct GPU_FrustumPlane
	{
		Zenith_Maths::Vector4 m_xNormalAndDistance; // xyz = normal, w = distance
	};

	struct GPU_FrustumData
	{
		GPU_FrustumPlane m_axPlanes[6];
	};

	struct GPU_CullingConstants
	{
		uint32_t m_uTerrainCount;
		uint32_t m_uPadding[3];
	};

	struct GPU_IndirectDrawCommand
	{
		uint32_t m_uIndexCount;
		uint32_t m_uInstanceCount;
		uint32_t m_uFirstIndex;
		int32_t m_iVertexOffset;
		uint32_t m_uFirstInstance;
	};

	// CPU culling state
	Zenith_Vector<Zenith_TerrainComponent*> g_xVisibleTerrain;
	Zenith_Vector<Zenith_AABB> g_xTerrainAABBs;  // Cached AABBs (index matches terrain list)
	Zenith_Frustum g_xCurrentFrustum;
	Flux_TerrainCulling::CullingStats g_xCullingStats;

	// GPU culling state
	bool g_bGPUCullingEnabled = false;
	bool g_bGPUCullingInitialised = false;

	// GPU resources
	Flux_Buffer g_xAABBBuffer;              // Buffer of all terrain AABBs
	Flux_Buffer g_xFrustumBuffer;           // Current frame's frustum
	Flux_Buffer g_xVisibleIndicesBuffer;    // Output: indices of visible terrain
	Flux_Buffer g_xVisibleCountBuffer;      // Output: count of visible terrain
	Flux_Buffer g_xIndirectDrawBuffer;      // Indirect draw commands
	Flux_Buffer g_xCullingConstantsBuffer;  // Constants for culling shader

	// GPU culling resources - disabled for now
	// Zenith_Vulkan_Shader g_xCullingShader;
	// Zenith_Vulkan_Pipeline g_xCullingPipeline;
	// Zenith_Vulkan_RootSig g_xCullingRootSig;

	// Debug visualization
	DEBUGVAR bool dbg_bShowCullingStats = true;
	DEBUGVAR bool dbg_bShowVisibleAABBs = false;
	DEBUGVAR bool dbg_bShowCulledAABBs = false;
	DEBUGVAR bool dbg_bShowFrustum = false;

	// =============================================================================
	// CPU Frustum Culling
	// =============================================================================

	static void PerformCPUCulling(
	const Zenith_CameraComponent& xCamera,
	const Zenith_Vector<Zenith_TerrainComponent*>& xAllTerrain)
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);

	// Start timing
	auto xStartTime = std::chrono::high_resolution_clock::now();

	// Extract frustum from camera
	Zenith_Maths::Matrix4 xView, xProj;
	xCamera.BuildViewMatrix(xView);
	xCamera.BuildProjectionMatrix(xProj);
	Zenith_Maths::Matrix4 xViewProj = xProj * xView;
	g_xCurrentFrustum.ExtractFromViewProjection(xViewProj);

	// Clear visible list
	g_xVisibleTerrain.Clear();

	// Resize AABB cache if needed
	const uint32_t uTerrainCount = xAllTerrain.GetSize();
	if (g_xTerrainAABBs.GetSize() != uTerrainCount)
	{
		g_xTerrainAABBs.Clear();
		g_xTerrainAABBs.Reserve(uTerrainCount);

		// Generate AABBs for all terrain
		for (uint32_t i = 0; i < uTerrainCount; ++i)
		{
			Zenith_AABB xAABB = Flux_TerrainCulling::GenerateTerrainAABB(*xAllTerrain.Get(i));
			g_xTerrainAABBs.PushBack(xAABB);
		}
	}

	// Test each terrain against frustum
	uint32_t uCulled = 0;
	for (uint32_t i = 0; i < uTerrainCount; ++i)
	{
		const Zenith_AABB& xAABB = g_xTerrainAABBs.Get(i);

		// Test AABB against frustum
		if (Zenith_FrustumCulling::TestAABBFrustum(g_xCurrentFrustum, xAABB))
		{
			g_xVisibleTerrain.PushBack(xAllTerrain.Get(i));
		}
		else
		{
			++uCulled;
		}
	}

	// End timing
	auto xEndTime = std::chrono::high_resolution_clock::now();
	float fCullingTimeMS = std::chrono::duration_cast<std::chrono::microseconds>(xEndTime - xStartTime).count() / 1000.0f;

	// Update stats
	g_xCullingStats.m_uTotalTerrain = uTerrainCount;
	g_xCullingStats.m_uVisibleTerrain = g_xVisibleTerrain.GetSize();
	g_xCullingStats.m_uCulledTerrain = uCulled;
	g_xCullingStats.m_fCullingTimeMS = fCullingTimeMS;
	g_xCullingStats.m_bUsedGPUCulling = false;

	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
}

// =============================================================================
// GPU Frustum Culling
// =============================================================================

static void InitialiseGPUCulling()
{
	// GPU culling disabled for now - requires full Vulkan integration
	// The infrastructure is in place (compute shader, buffer layouts)
	// but needs completion of indirect draw pipeline

	g_bGPUCullingEnabled = false;
	g_bGPUCullingInitialised = false;

	Zenith_Log("GPU terrain culling: Not yet implemented (CPU culling active)");

	/* TODO: Complete GPU culling implementation
	 *
	 * Requires:
	 * 1. Create compute shader: g_xCullingShader.InitialiseCompute("Terrain/TerrainCulling.comp")
	 * 2. Build compute pipeline with descriptor sets for:
	 *    - AABBs buffer
	 *    - Frustum buffer
	 *    - Base draw commands
	 *    - Visible indices output
	 *    - Visible count atomic
	 *    - Indirect draw commands output
	 * 3. Allocate GPU buffers (see GPU_TerrainAABB, GPU_FrustumData structures)
	 * 4. Implement dispatch in PerformGPUCulling()
	 * 5. Implement indirect draw in SubmitGPUCulledDraws()
	 *
	 * See TERRAIN_CULLING_GUIDE.md for full implementation details
	 */
}

static void PerformGPUCulling(
	const Zenith_CameraComponent& xCamera,
	const Zenith_Vector<Zenith_TerrainComponent*>& xAllTerrain)
{
	// GPU culling not implemented - fall back to CPU
	// The complete implementation would:
	// 1. Upload AABBs to g_xAABBBuffer
	// 2. Upload frustum to g_xFrustumBuffer
	// 3. Dispatch compute shader: (terrainCount + 63) / 64 workgroups
	// 4. Use indirect draw from g_xIndirectDrawBuffer
	//
	// For now, use CPU culling
	PerformCPUCulling(xCamera, xAllTerrain);
	g_xCullingStats.m_bUsedGPUCulling = false;  // Actually used CPU
}

}  // End anonymous namespace

// =============================================================================
// Initialization & Shutdown
// =============================================================================

void Flux_TerrainCulling::Initialise()
{
	Zenith_Log("Flux_TerrainCulling::Initialise()");

	g_xVisibleTerrain.Reserve(MAX_TERRAIN_COMPONENTS);
	g_xTerrainAABBs.Reserve(MAX_TERRAIN_COMPONENTS);

	// Initialize GPU culling resources
	InitialiseGPUCulling();

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Show Culling Stats" }, dbg_bShowCullingStats);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Show Visible AABBs" }, dbg_bShowVisibleAABBs);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Show Culled AABBs" }, dbg_bShowCulledAABBs);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Show Frustum" }, dbg_bShowFrustum);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "GPU Culling" }, g_bGPUCullingEnabled);

	// Culling statistics (read-only)
	Zenith_DebugVariables::AddUInt32_ReadOnly({ "Render", "Terrain", "Stats", "Total Terrain" }, g_xCullingStats.m_uTotalTerrain, 0, 0);
	Zenith_DebugVariables::AddUInt32_ReadOnly({ "Render", "Terrain", "Stats", "Visible Terrain" }, g_xCullingStats.m_uVisibleTerrain, 0, 0);
	Zenith_DebugVariables::AddUInt32_ReadOnly({ "Render", "Terrain", "Stats", "Culled Terrain" }, g_xCullingStats.m_uCulledTerrain, 0, 0);
	Zenith_DebugVariables::AddFloat_ReadOnly({ "Render", "Terrain", "Stats", "Culling Time (ms)" }, g_xCullingStats.m_fCullingTimeMS);
#endif

	Zenith_Log("Flux_TerrainCulling initialised");
}

void Flux_TerrainCulling::Shutdown()
{
	// Clean up GPU resources
	// Note: GPU culling is currently disabled, so no cleanup needed
	g_bGPUCullingInitialised = false;
}

// =============================================================================
// AABB Generation
// =============================================================================

Zenith_AABB Flux_TerrainCulling::GenerateTerrainAABB(const Zenith_TerrainComponent& xTerrain)
{
	const Flux_MeshGeometry& xMesh = xTerrain.GetRenderMeshGeometry();

	// Generate AABB from mesh vertex positions
	if (xMesh.m_pxPositions != nullptr && xMesh.GetNumVerts() > 0)
	{
		return Zenith_FrustumCulling::GenerateAABBFromVertices(
			xMesh.m_pxPositions,
			xMesh.GetNumVerts()
		);
	}

	// Fallback: empty AABB
	Zenith_Log("Warning: Terrain component has no vertex positions for AABB generation");
	return Zenith_AABB();
}

// =============================================================================
// Public API
// =============================================================================

void Flux_TerrainCulling::SetGPUCullingEnabled(bool bEnabled)
{
	if (bEnabled && !g_bGPUCullingInitialised)
	{
		Zenith_Log("Warning: GPU culling not initialized, staying in CPU mode");
		g_bGPUCullingEnabled = false;
		return;
	}
	g_bGPUCullingEnabled = bEnabled;
}

bool Flux_TerrainCulling::IsGPUCullingEnabled()
{
	return g_bGPUCullingEnabled && g_bGPUCullingInitialised;
}

void Flux_TerrainCulling::PerformCulling(
	const Zenith_CameraComponent& xCamera,
	const Zenith_Vector<Zenith_TerrainComponent*>& xAllTerrain)
{
	if (g_bGPUCullingEnabled && g_bGPUCullingInitialised)
	{
		PerformGPUCulling(xCamera, xAllTerrain);
	}
	else
	{
		PerformCPUCulling(xCamera, xAllTerrain);
	}
}

const Zenith_Vector<Zenith_TerrainComponent*>& Flux_TerrainCulling::GetVisibleTerrainComponents()
{
	return g_xVisibleTerrain;
}

const Zenith_Frustum& Flux_TerrainCulling::GetCurrentFrustum()
{
	return g_xCurrentFrustum;
}

const Flux_TerrainCulling::CullingStats& Flux_TerrainCulling::GetCullingStats()
{
	return g_xCullingStats;
}

void Flux_TerrainCulling::RenderDebugVisualization(
	const Zenith_CameraComponent& xCamera,
	bool bShowCulledAABBs,
	bool bShowVisibleAABBs,
	bool bShowFrustum)
{
	// TODO: Implement debug visualization
	// This would render wireframe AABBs and frustum planes for debugging
	// Requires a simple line rendering system
}

void Flux_TerrainCulling::SubmitGPUCulledDraws(Flux_CommandList& xCmdList)
{
	// GPU indirect draw not yet implemented
	Zenith_Assert(!g_bGPUCullingEnabled, "GPU culling not yet implemented");
}

void* Flux_TerrainCulling::GetGPUCullingPipeline()
{
	// GPU culling disabled
	return nullptr;
}
