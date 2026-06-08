#pragma once

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"

#include <chrono>

// Forward-declared so Initialise(Zenith_Multithreading&) can take the thread
// subsystem by reference (injected at the composition root) without dragging
// the full Multithreading header into this widely-included file.
class Zenith_Multithreading;

enum Zenith_ProfileIndex
{
	ZENITH_PROFILE_INDEX__TOTAL_FRAME,

	ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM,
	ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX,

	ZENITH_PROFILE_INDEX__ANIMATION,
	ZENITH_PROFILE_INDEX__SCENE_UPDATE,
	ZENITH_PROFILE_INDEX__PHYSICS,

	ZENITH_PROFILE_INDEX__FLUX_SHADOWS,
	ZENITH_PROFILE_INDEX__FLUX_SHADOWS_UPDATE_MATRICES,

	ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING,
	ZENITH_PROFILE_INDEX__FLUX_DYNAMIC_LIGHTS,
	ZENITH_PROFILE_INDEX__FLUX_SKYBOX,
	ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES,
	ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES,
	ZENITH_PROFILE_INDEX__FLUX_INSTANCED_MESHES,
	ZENITH_PROFILE_INDEX__FLUX_COMPUTE,
	ZENITH_PROFILE_INDEX__FLUX_GRASS,
	ZENITH_PROFILE_INDEX__FLUX_TERRAIN,
	ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING,

	ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING,
	ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_STREAM_IN_LOD,
	ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_EVICT,
	ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_ALLOCATE,

	ZENITH_PROFILE_INDEX__FLUX_PRIMITIVES,
	ZENITH_PROFILE_INDEX__FLUX_WATER,
	ZENITH_PROFILE_INDEX__FLUX_SSAO,
	ZENITH_PROFILE_INDEX__FLUX_HIZ,
	ZENITH_PROFILE_INDEX__FLUX_SSR,
	ZENITH_PROFILE_INDEX__FLUX_SSGI,
	ZENITH_PROFILE_INDEX__FLUX_FOG,
	ZENITH_PROFILE_INDEX__FLUX_HDR,
	ZENITH_PROFILE_INDEX__FLUX_ATMOSPHERE,
	ZENITH_PROFILE_INDEX__FLUX_SDFS,
	ZENITH_PROFILE_INDEX__FLUX_PFX,
	ZENITH_PROFILE_INDEX__FLUX_TEXT,
	ZENITH_PROFILE_INDEX__FLUX_QUADS,
	ZENITH_PROFILE_INDEX__FLUX_GIZMOS,
	ZENITH_PROFILE_INDEX__FLUX_MEMORY_MANAGER,
	ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME,
	ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_END_FRAME,
	ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_BEGIN_FRAME,
	ZENITH_PROFILE_INDEX__FLUX_PLATFORMAPI_END_FRAME,

	ZENITH_PROFILE_INDEX__FLUX_ITERATE_COMMANDS,
	ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS,
	ZENITH_PROFILE_INDEX__FLUX_RECORD_PASS,   // Per-pass record scope — used with a runtime label argument to BeginProfile.

	ZENITH_PROFILE_INDEX__FLUX_MESH_GEOMETRY_LOAD_FROM_FILE,

	ZENITH_PROFILE_INDEX__ASSET_LOAD,

	//#TO_TODO: rename these at runtime
	ZENITH_PROFILE_INDEX__VULKAN_UPDATE_DESCRIPTOR_SETS,
	ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_UPLOAD,
	ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_FLUSH,
	ZENITH_PROFILE_INDEX__VULKAN_WAIT_FOR_GPU,
	ZENITH_PROFILE_INDEX__VULKAN_RESET_DESCRIPTOR_POOLS,
	ZENITH_PROFILE_INDEX__VULKAN_RECORD_COMMAND_BUFFERS,
	

	ZENITH_PROFILE_INDEX__VISIBILITY_CHECK,

	// AI System
	ZENITH_PROFILE_INDEX__AI_PERCEPTION_UPDATE,
	ZENITH_PROFILE_INDEX__AI_PERCEPTION_SIGHT,
	ZENITH_PROFILE_INDEX__AI_SQUAD_UPDATE,
	ZENITH_PROFILE_INDEX__AI_TACTICAL_UPDATE,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_AGENT_UPDATE,
	ZENITH_PROFILE_INDEX__AI_PATHFINDING,
	ZENITH_PROFILE_INDEX__AI_AGENT_UPDATE,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE,
	// Sub-stages of AI_NAVMESH_GENERATE -- each wrapped individually so the
	// profile report shows which Recast-style phase dominates the total.
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_COLLECT_GEOMETRY,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_COMPUTE_BOUNDS,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_VOXELIZE,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_FILTER_WALKABLE,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_COMPACT_HF,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_REGIONS,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_TRACE_CONTOURS,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_POLY_MESH,
	ZENITH_PROFILE_INDEX__AI_NAVMESH_GENERATE_BUILD_NAVMESH,
	ZENITH_PROFILE_INDEX__AI_DEBUG_DRAW,

	// TilePuzzle Level Generation
	ZENITH_PROFILE_INDEX__TILEPUZZLE_GENERATE_LEVEL,
	ZENITH_PROFILE_INDEX__TILEPUZZLE_GENERATE_ATTEMPT,
	ZENITH_PROFILE_INDEX__TILEPUZZLE_GRID_SETUP,
	ZENITH_PROFILE_INDEX__TILEPUZZLE_SCRAMBLE,
	ZENITH_PROFILE_INDEX__TILEPUZZLE_REVERSE_BFS_SCRAMBLE,
	ZENITH_PROFILE_INDEX__TILEPUZZLE_SOLVER,
	ZENITH_PROFILE_INDEX__TILEPUZZLE_SOLVER_WITH_PATH,
	ZENITH_PROFILE_INDEX__TILEPUZZLE_SOLVER_BFS_DEPTH,

	#ifdef ZENITH_TOOLS
	ZENITH_PROFILE_INDEX__RENDER_IMGUI,
	ZENITH_PROFILE_INDEX__RENDER_IMGUI_PROFILING,
	#endif

	ZENITH_PROFILE_INDEX__COUNT,
};

static const char* g_aszProfileNames[]
{
	"Total Frame",
	"Wait for Task System",
	"Wait for Mutex",
	"Animation",
	"Scene Update",
	"Physics",
	"Flux Shadows",
	"Flux Shadows Update Matrices",
	"Flux Deferred Shading",
	"Flux Dynamic Lights",
	"Flux Skybox",
	"Flux Static Meshes",
	"Flux Animated Meshes",
	"Flux Instanced Meshes",
	"Flux Compute",
	"Flux Grass",
	"Flux Terrain",
	"Flux Terrain Culling",
	"Flux Terrain Streaming",
	"Flux Terrain Streaming Stream In LOD",
	"Flux Terrain Streaming Evict",
	"Flux Terrain Streaming Allocate",
	"Flux Primitives",
	"Flux Water",
	"Flux SSAO",
	"Flux HiZ",
	"Flux SSR",
	"Flux SSGI",
	"Flux Fog",
	"Flux HDR",
	"Flux Atmosphere",
	"Flux SDFs",
	"Flux PFX",
	"Flux Text",
	"Flux Quads",
	"Flux Gizmos",
	"Flux Memory Manager",
	"Flux Swapchain Begin Frame",
	"Flux Swapchain End Frame",
	"Flux PlatformAPI Begin Frame",
	"Flux PlatformAPI End Frame",
	"Flux Iterate Commands",
	"Flux Record Command Buffers",
	"Flux Record Pass",
	"Flux Mesh Geometry Load From File",

	"Asset Load",

	//#TO_TODO: rename these at runtime
	"Vulkan Update Descriptor Sets",
	"Vulkan Memory Manager Upload",
	"Vulkan Memory Manager Flush",
	"Vulkan Wait For GPU",
	"Vulkan Reset Descriptor Pools",
	"Vulkan Record Command Buffers",

	"Visibility Check",

	// AI System
	"AI Perception Update",
	"AI Perception Sight",
	"AI Squad Update",
	"AI Tactical Update",
	"AI NavMesh Agent Update",
	"AI Pathfinding",
	"AI Agent Update",
	"AI NavMesh Generate",
	"AI NavMesh Generate / Collect Geometry",
	"AI NavMesh Generate / Compute Bounds",
	"AI NavMesh Generate / Voxelize",
	"AI NavMesh Generate / Filter Walkable",
	"AI NavMesh Generate / Build Compact HF",
	"AI NavMesh Generate / Build Regions",
	"AI NavMesh Generate / Trace Contours",
	"AI NavMesh Generate / Build Poly Mesh",
	"AI NavMesh Generate / Build NavMesh",
	"AI Debug Draw",

	// TilePuzzle Level Generation
	"TilePuzzle Generate Level",
	"TilePuzzle Generate Attempt",
	"TilePuzzle Grid Setup",
	"TilePuzzle Scramble",
	"TilePuzzle Reverse BFS Scramble",
	"TilePuzzle Solver",
	"TilePuzzle Solver With Path",
	"TilePuzzle Solver BFS Depth",

	#ifdef ZENITH_TOOLS
	"ImGUI",
	"ImGUI Profiling",
	#endif
};
static_assert(COUNT_OF(g_aszProfileNames) == ZENITH_PROFILE_INDEX__COUNT, "g_aszProfileNames mismatch");

// Private bridge — declared first so inline header code below (the Scope
// RAII ctor/dtor and the ZENITH_PROFILING_FUNCTION_WRAPPER macro body)
// can reach the engine-owned instance without needing Zenith_Engine.h
// here, which would cycle. Definitions live in Zenith_Profiling.cpp
// where g_xEngine is visible. This is a documented header-include-cycle
// break, NOT a re-introduced static facade.
namespace Zenith_Profiling_Detail
{
	void BeginProfile(Zenith_ProfileIndex eIndex, const char* szLabel);
	void EndProfile(Zenith_ProfileIndex eIndex);
}

// State + behaviour for the Profiling subsystem. Held on g_xEngine and
// accessed via g_xEngine.Profiling(). Nested types Event and Scope are
// preserved on the class so existing call sites
// (Zenith_Profiling::Event / Zenith_Profiling::Scope) compile unchanged.
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

	// Nested RAII helper. Ctor/dtor are inline; they route through the
	// _Detail:: bridge above so this class type does not need to be
	// complete-at-use for the member call to compile (the type IS the
	// enclosing class — chicken-and-egg).
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

	// Public data members. Mirror of the former Zenith_ProfilingImpl —
	// kept public because Zenith_Profiling.cpp accesses them directly
	// from the method bodies (and a private/getter wrapper would add
	// nothing). The 5 thread-local profile-stack variables stay as
	// file-scope statics in the .cpp; they're per-OS-thread, not
	// per-Engine.
	// Injected at Initialise() from the composition root. Used for
	// per-thread id queries (RegisterThread / EndProfile / the static
	// GetOrCreateThreadEvents helper, which reaches it through the
	// recovered g_xEngine.Profiling() ref). Public so that static helper
	// can read it — matching the existing public-member design here.
	Zenith_Multithreading*                          m_pxThreading = nullptr;

	Zenith_HashMap<u_int, Zenith_Vector<Event>>     m_xEvents;
	Zenith_HashMap<u_int, Zenith_Vector<Event>>     m_xPreviousFrameEvents;
	Zenith_Mutex_NoProfiling                        m_xEventsMutex;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xFrameStart;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xFrameEnd;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xPreviousFrameStart;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_xPreviousFrameEnd;

	// Pause latch. Toggled at frame boundaries by EndFrame so the
	// recorded final frame is internally consistent vs. the ImGui
	// checkbox toggling mid-frame. See Zenith_Profiling.cpp for the
	// dbg_bPauseRequested coupling logic.
	bool m_bPauseEffective = false;
};

// Function-wrapper macro. The bridge forwarders are used (not the class
// methods) because the macro body is included into TUs that have not
// included Zenith_Engine.h — pulling the engine header into every
// caller of this macro would balloon the include graph.
#define ZENITH_PROFILING_FUNCTION_WRAPPER(x, eProfile, ...) \
	Zenith_Profiling_Detail::BeginProfile(eProfile, nullptr); \
	x(__VA_ARGS__); \
	Zenith_Profiling_Detail::EndProfile(eProfile);