#pragma once

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"

#include <chrono>

class Zenith_Multithreading;

// Single source of truth for the profile indices: each X(enumerator, display name)
// generates both the Zenith_ProfileIndex enum and g_aszProfileNames below.
#define ZENITH_PROFILE_INDEX_LIST_COMMON(X) \
	X(ZENITH_PROFILE_INDEX__TOTAL_FRAME, "Total Frame") \
	X(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM, "Wait for Task System") \
	X(ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX, "Wait for Mutex") \
	X(ZENITH_PROFILE_INDEX__ANIMATION, "Animation") \
	X(ZENITH_PROFILE_INDEX__SCENE_UPDATE, "Scene Update") \
	X(ZENITH_PROFILE_INDEX__UI_UPDATE, "UI Update") \
	X(ZENITH_PROFILE_INDEX__PHYSICS, "Physics") \
	X(ZENITH_PROFILE_INDEX__FLUX_SHADOWS, "Flux Shadows") \
	X(ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES, "Flux Shadows Update Matrices") \
	X(ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING, "Flux Deferred Shading") \
	X(ZENITH_PROFILE_INDEX__FLUX_DYNAMIC_LIGHTS, "Flux Dynamic Lights") \
	X(ZENITH_PROFILE_INDEX__FLUX_SKYBOX, "Flux Skybox") \
	X(ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES, "Flux Static Meshes") \
	X(ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES, "Flux Animated Meshes") \
	X(ZENITH_PROFILE_INDEX__FLUX_INSTANCED_MESHES, "Flux Instanced Meshes") \
	X(ZENITH_PROFILE_INDEX__FLUX_COMPUTE, "Flux Compute") \
	X(ZENITH_PROFILE_INDEX__FLUX_GRASS, "Flux Grass") \
	X(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, "Flux Terrain") \
	X(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING, "Flux Terrain Culling") \
	X(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING, "Flux Terrain Streaming") \
	X(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_STREAM_IN_LOD, "Flux Terrain Streaming Stream In LOD") \
	X(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_EVICT, "Flux Terrain Streaming Evict") \
	X(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_ALLOCATE, "Flux Terrain Streaming Allocate") \
	X(ZENITH_PROFILE_INDEX__FLUX_PRIMITIVES, "Flux Primitives") \
	X(ZENITH_PROFILE_INDEX__FLUX_WATER, "Flux Water") \
	X(ZENITH_PROFILE_INDEX__FLUX_SSAO, "Flux SSAO") \
	X(ZENITH_PROFILE_INDEX__FLUX_HIZ, "Flux HiZ") \
	X(ZENITH_PROFILE_INDEX__FLUX_SSR, "Flux SSR") \
	X(ZENITH_PROFILE_INDEX__FLUX_SSGI, "Flux SSGI") \
	X(ZENITH_PROFILE_INDEX__FLUX_FOG, "Flux Fog") \
	X(ZENITH_PROFILE_INDEX__FLUX_HDR, "Flux HDR") \
	X(ZENITH_PROFILE_INDEX__FLUX_ATMOSPHERE, "Flux Atmosphere") \
	X(ZENITH_PROFILE_INDEX__FLUX_SDFS, "Flux SDFs") \
	X(ZENITH_PROFILE_INDEX__FLUX_PFX, "Flux PFX") \
	X(ZENITH_PROFILE_INDEX__FLUX_TEXT, "Flux Text") \
	X(ZENITH_PROFILE_INDEX__FLUX_QUADS, "Flux Quads") \
	X(ZENITH_PROFILE_INDEX__FLUX_GIZMOS, "Flux Gizmos") \
	X(ZENITH_PROFILE_INDEX__FLUX_MEMORY_MANAGER, "Flux Memory Manager") \
	X(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME, "Flux Swapchain Begin Frame") \
	X(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_END_FRAME, "Flux Swapchain End Frame") \
	X(ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_BEGIN_FRAME, "Flux PlatformAPI Begin Frame") \
	X(ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_END_FRAME, "Flux PlatformAPI End Frame") \
	/* FLUX_RECORD_PASS is per-pass: used with a runtime label argument to BeginProfile */ \
	X(ZENITH_PROFILE_INDEX__FLUX_RECORD_PASS, "Flux Record Pass") \
	X(ZENITH_PROFILE_INDEX__FLUX_MESH_GEOMETRY_LOAD_FROM_FILE, "Flux Mesh Geometry Load From File") \
	X(ZENITH_PROFILE_INDEX__ASSET_LOAD, "Asset Load") \
	/* #TO_TODO: rename these at runtime */ \
	X(ZENITH_PROFILE_INDEX__VULKAN_UPDATE_DESCRIPTOR_SETS, "Vulkan Update Descriptor Sets") \
	X(ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_UPLOAD, "Vulkan Memory Manager Upload") \
	X(ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_FLUSH, "Vulkan Memory Manager Flush") \
	X(ZENITH_PROFILE_INDEX__VULKAN_WAIT_FOR_GPU, "Vulkan Wait For GPU") \
	X(ZENITH_PROFILE_INDEX__VULKAN_RESET_DESCRIPTOR_POOLS, "Vulkan Reset Descriptor Pools") \
	X(ZENITH_PROFILE_INDEX__VULKAN_RECORD_COMMAND_BUFFERS, "Vulkan Record Command Buffers") \
	X(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK, "Visibility Check") \
	X(ZENITH_PROFILE_INDEX__AI_PERCEPTION_UPDATE, "AI Perception Update") \
	X(ZENITH_PROFILE_INDEX__AI_PERCEPTION_SIGHT, "AI Perception Sight") \
	X(ZENITH_PROFILE_INDEX__AI_SQUAD_UPDATE, "AI Squad Update") \
	X(ZENITH_PROFILE_INDEX__AI_TACTICAL_UPDATE, "AI Tactical Update") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_AGENT_UPDATE, "AI NavMesh Agent Update") \
	X(ZENITH_PROFILE_INDEX__AI_PATHFINDING, "AI Pathfinding") \
	X(ZENITH_PROFILE_INDEX__AI_AGENT_UPDATE, "AI Agent Update") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE, "AI NavMesh Generate") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_COLLECT_GEOMETRY, "AI NavMesh Generate / Collect Geometry") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_COMPUTE_BOUNDS, "AI NavMesh Generate / Compute Bounds") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_VOXELIZE, "AI NavMesh Generate / Voxelize") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_FILTER_WALKABLE, "AI NavMesh Generate / Filter Walkable") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_COMPACT_HF, "AI NavMesh Generate / Build Compact HF") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_REGIONS, "AI NavMesh Generate / Build Regions") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_TRACE_CONTOURS, "AI NavMesh Generate / Trace Contours") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_POLY_MESH, "AI NavMesh Generate / Build Poly Mesh") \
	X(ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_NAVMESH, "AI NavMesh Generate / Build NavMesh") \
	X(ZENITH_PROFILE_INDEX__AI_DEBUG_DRAW, "AI Debug Draw") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_GENERATE_LEVEL, "TilePuzzle Generate Level") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_GENERATE_ATTEMPT, "TilePuzzle Generate Attempt") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_GRID_SETUP, "TilePuzzle Grid Setup") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_SCRAMBLE, "TilePuzzle Scramble") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_REVERSE_BFS_SCRAMBLE, "TilePuzzle Reverse BFS Scramble") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_SOLVER, "TilePuzzle Solver") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_SOLVER_WITH_PATH, "TilePuzzle Solver With Path") \
	X(ZENITH_PROFILE_INDEX__TILEPUZZLE_SOLVER_BFS_DEPTH, "TilePuzzle Solver BFS Depth")

#ifdef ZENITH_TOOLS
#define ZENITH_PROFILE_INDEX_LIST(X) \
	ZENITH_PROFILE_INDEX_LIST_COMMON(X) \
	X(ZENITH_PROFILE_INDEX__RENDER_IMGUI, "ImGUI") \
	X(ZENITH_PROFILE_INDEX__RENDER_IMGUI_PROFILING, "ImGUI Profiling")
#else
#define ZENITH_PROFILE_INDEX_LIST(X) ZENITH_PROFILE_INDEX_LIST_COMMON(X)
#endif

#define ZENITH_PROFILE_INDEX_AS_ENUM(eIndex, szName) eIndex,
enum Zenith_ProfileIndex
{
	ZENITH_PROFILE_INDEX_LIST(ZENITH_PROFILE_INDEX_AS_ENUM)
	ZENITH_PROFILE_INDEX__COUNT,
};
#undef ZENITH_PROFILE_INDEX_AS_ENUM

#define ZENITH_PROFILE_INDEX_AS_NAME(eIndex, szName) szName,
inline constexpr const char* g_aszProfileNames[]
{
	ZENITH_PROFILE_INDEX_LIST(ZENITH_PROFILE_INDEX_AS_NAME)
};
#undef ZENITH_PROFILE_INDEX_AS_NAME
static_assert(COUNT_OF(g_aszProfileNames) == ZENITH_PROFILE_INDEX__COUNT, "g_aszProfileNames mismatch");

// Bridge so header-inline code (Scope and ZENITH_PROFILING_FUNCTION_WRAPPER)
// can reach the engine-owned instance without including Zenith_Engine.h here,
// which would cycle. Definitions live in Zenith_Profiling.cpp.
namespace Zenith_Profiling_Detail
{
	void BeginProfile(Zenith_ProfileIndex eIndex, const char* szLabel);
	void EndProfile(Zenith_ProfileIndex eIndex);
}

// Held on g_xEngine, accessed via g_xEngine.Profiling().
class Zenith_Profiling
{
public:
	Zenith_Profiling() = default;
	~Zenith_Profiling() = default;
	Zenith_Profiling(const Zenith_Profiling&) = delete;
	Zenith_Profiling& operator=(const Zenith_Profiling&) = delete;

	struct Event
	{
		Event(const std::chrono::time_point<std::chrono::high_resolution_clock>& xBegin, const std::chrono::time_point<std::chrono::high_resolution_clock>& xEnd, const Zenith_ProfileIndex eIndex, const u_int uDepth, const char* szLabel = nullptr)
			: m_xBegin(xBegin)
			, m_xEnd(xEnd)
			, m_eIndex(eIndex)
			, m_uDepth(uDepth)
			, m_szLabel(szLabel)
		{
		}
		std::chrono::time_point<std::chrono::high_resolution_clock> m_xBegin;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_xEnd;
		Zenith_ProfileIndex m_eIndex;
		u_int m_uDepth;
		const char* m_szLabel;
	};

	void Initialise(Zenith_Multithreading& xThreading);

	void RegisterThread();

	void BeginFrame();
	void EndFrame();
	#ifdef ZENITH_TOOLS
	struct TimelineViewState
	{
		int m_iMinDepthToRender = 0;
		int m_iMaxDepthToRender = 10;
		int m_iMaxDepthToRenderSeparately = 3;
		float m_fTimelineZoom = 1.0f;
		float m_fTimelineScroll = 0.0f;
		float m_fVerticalScale = 1.0f;
	};

	void RenderToImGui();
	void RenderTimelineView(TimelineViewState& xState);
	void RenderThreadBreakdown(float fFrameDurationMs, u_int& uThreadID);
	#endif

	void BeginProfile(const Zenith_ProfileIndex eIndex, const char* szLabel = nullptr);
	void EndProfile(const Zenith_ProfileIndex eIndex);

	const Zenith_ProfileIndex GetCurrentIndex();

	const Zenith_HashMap<u_int, Zenith_Vector<Event>>& GetEvents();

	void ClearEvents();
	void WriteTextReport(FILE* pFile);

	// RAII begin/end. Routes through the _Detail:: bridge because g_xEngine
	// is not reachable from this header.
	class Scope
	{
	public:
		Scope() = delete;
		Scope(Zenith_ProfileIndex eIndex)
			: m_eIndex(eIndex)
		{
			Zenith_Profiling_Detail::BeginProfile(eIndex, nullptr);
		}
		~Scope()
		{
			Zenith_Profiling_Detail::EndProfile(m_eIndex);
		}
	private:
		Zenith_ProfileIndex m_eIndex;
	};

	// Public because the .cpp's static free-function helpers reach these
	// through g_xEngine.Profiling(). The per-thread profile stack lives as a
	// thread_local in the .cpp — per-OS-thread, not per-engine.
	Zenith_Multithreading*                          m_pxThreading = nullptr; // Injected at Initialise().

	Zenith_HashMap<u_int, Zenith_Vector<Event>>     m_xEvents;
	Zenith_HashMap<u_int, Zenith_Vector<Event>>     m_xPreviousFrameEvents;
	Zenith_Mutex_NoProfiling                        m_xEventsMutex;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xFrameStart;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xFrameEnd;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xPreviousFrameStart;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xPreviousFrameEnd;

	// Latched from dbg_bPauseRequested at frame boundaries (see EndFrame) so
	// the recorded final frame is internally consistent.
	bool m_bPauseEffective = false;
};

// Uses the bridge forwarders so callers do not need Zenith_Engine.h.
#define ZENITH_PROFILING_FUNCTION_WRAPPER(x, eProfile, ...) \
	Zenith_Profiling_Detail::BeginProfile(eProfile, nullptr); \
	x(__VA_ARGS__); \
	Zenith_Profiling_Detail::EndProfile(eProfile);
