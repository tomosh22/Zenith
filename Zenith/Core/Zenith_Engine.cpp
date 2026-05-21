#include "Zenith.h"

#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/FrameContext.h"
#include "Core/Multithreading/Zenith_MultithreadingImpl.h"
#include "Profiling/Zenith_ProfilingImpl.h"
#include "TaskSystem/Zenith_TaskSystemImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "DebugVariables/Zenith_DebugVariablesImpl.h"
#include "EntityComponent/Zenith_EntityStore.h"
#include "EntityComponent/Internal/Zenith_SceneCallbackBusImpl.h"
#include "EntityComponent/Internal/Zenith_SceneLifecycleSchedulerImpl.h"
#include "EntityComponent/Internal/Zenith_SceneOperationQueueImpl.h"
#include "EntityComponent/Internal/Zenith_SceneRegistryImpl.h"
#include "Input/Zenith_InputImpl.h"
#include "Input/Zenith_TouchInputImpl.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Vulkan/Zenith_VulkanImpl.h"
#include "Vulkan/Zenith_Vulkan_MemoryManagerImpl.h"
#include "Vulkan/Zenith_Vulkan_SwapchainImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/SDFs/Flux_SDFsImpl.h"
#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/Fog/Flux_FroxelFogImpl.h"
#include "Flux/Fog/Flux_GodRaysFogImpl.h"
#include "Flux/Fog/Flux_RaymarchFogImpl.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Flux/SSAO/Flux_SSAOImpl.h"
#include "Flux/Decals/Flux_DecalsImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Flux/Flux_Graphics.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorImpl.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_EditorAutomationImpl.h"
#include "Editor/Zenith_EditorMaterialUIImpl.h"
#include "Editor/Zenith_GizmoImpl.h"
#include "Editor/Zenith_SelectionSystemImpl.h"
#include "Editor/Zenith_UndoSystemImpl.h"
#endif
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsImpl.h"
#include "UnitTests/Zenith_UnitTests.h"

#ifdef ZENITH_WINDOWS
#include <cstring>
#endif

#ifdef ZENITH_TOOLS
extern void ExportAllMeshes();
extern void ExportAllTextures();
extern void ExportHeightmap();
extern void ExportDefaultFontAtlas();
extern void GenerateTestAssets();
#endif

extern void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions);
extern const char* Project_GetGameAssetsDir();
extern void Project_RegisterScriptBehaviours();
extern void Project_Shutdown();

#ifdef ZENITH_TOOLS
extern void Project_InitializeResources();
extern void Project_RegisterEditorAutomationSteps();
#endif
extern void Project_LoadInitialScene();

// The single engine instance. constinit guarantees zero static-init
// cost — every member of Zenith_Engine has a constant default
// initialiser (currently none, but the rule must hold as members are
// added in later phases).
constinit Zenith_Engine g_xEngine;

namespace
{
	bool HasCommandLineFlag(const char* szFlag)
	{
#ifdef ZENITH_WINDOWS
		for (int i = 1; i < __argc; i++)
		{
			if (std::strcmp(__argv[i], szFlag) == 0)
				return true;
		}
#else
		(void)szFlag;
#endif
		return false;
	}
}

FrameContext& Zenith_Engine::Frame()
{
	Zenith_Assert(m_pxFrame != nullptr,
		"Zenith_Engine::Frame() called before Initialise() or after Shutdown(). "
		"Frame timing is unavailable outside the engine lifetime.");
	return *m_pxFrame;
}

Zenith_MultithreadingImpl& Zenith_Engine::Threading()
{
	// NOTE: no assert here. Threading() is read on the IsMainThread
	// hot path (every Zenith_Assert that gates on main-thread fires
	// it), and the engine guarantees m_pxThreading exists before any
	// thread can call RegisterThread / IsMainThread (see Initialise).
	return *m_pxThreading;
}

Zenith_TaskSystemImpl& Zenith_Engine::Tasks()
{
	// No assert: SubmitTask is a hot path, and Zenith_Engine::Initialise
	// allocates m_pxTasks before Zenith_TaskSystem::Inititalise() (the
	// forwarder that brings worker threads online).
	return *m_pxTasks;
}

Zenith_ProfilingImpl& Zenith_Engine::Profiling()
{
	// No assert: BeginProfile / EndProfile sit on the hottest path in
	// the engine (every ZENITH_PROFILING_FUNCTION_WRAPPER call fires
	// it). Zenith_Engine::Initialise allocates m_pxProfiling before
	// Zenith_Profiling::Initialise() so the impl is always available
	// once any thread starts profiling.
	return *m_pxProfiling;
}

Zenith_AssetRegistry& Zenith_Engine::Assets()
{
	Zenith_Assert(m_pxAssets != nullptr,
		"Zenith_Engine::Assets() called before Initialise() or after Shutdown(). "
		"AssetRegistry is unavailable outside the engine lifetime.");
	return *m_pxAssets;
}

Zenith_PhysicsImpl& Zenith_Engine::Physics()
{
	Zenith_Assert(m_pxPhysics != nullptr,
		"Zenith_Engine::Physics() called before Initialise() or after Shutdown(). "
		"Physics is unavailable outside the engine lifetime.");
	return *m_pxPhysics;
}

Zenith_EntityStore& Zenith_Engine::EntityStore()
{
	// No assert: EntityStore is read on the hot path for every entity
	// access (lifecycle queries, component lookups). Zenith_Engine::Initialise
	// allocates m_pxEntityStore very early -- before SceneManager / Physics
	// init -- so the store is always available once any subsystem touches
	// entity slots.
	return *m_pxEntityStore;
}

Zenith_SceneRegistryImpl& Zenith_Engine::SceneRegistry()
{
	// No assert: SceneRegistry is read every scene-handle resolution and
	// from inside the per-frame Update pipeline. Allocated alongside the
	// EntityStore VERY EARLY in Initialise so it's live before
	// Zenith_SceneManager::Initialise creates the persistent scene.
	return *m_pxSceneRegistry;
}

Zenith_SceneCallbackBusImpl& Zenith_Engine::SceneCallbacks()
{
	// No assert: callback registrations happen at static-init time (via
	// scene system bootstrap) and during gameplay. Allocated alongside
	// SceneRegistry / EntityStore VERY EARLY in Initialise.
	return *m_pxSceneCallbacks;
}

Zenith_SceneOperationQueueImpl& Zenith_Engine::SceneOperations()
{
	// No assert: operation map / async-job lists are read from the
	// Update pipeline and from worker-thread completion paths. Allocated
	// EARLY in Initialise alongside the other scene-system subsystems.
	return *m_pxSceneOperations;
}

Zenith_SceneLifecycleSchedulerImpl& Zenith_Engine::SceneLifecycle()
{
	// No assert: lifecycle flags and the FixedUpdate accumulator are read
	// every frame from the Update pipeline. Allocated EARLY alongside
	// the other scene-system Impls.
	return *m_pxSceneLifecycle;
}

Zenith_InputImpl& Zenith_Engine::Input()
{
	// No assert: input is read every frame and from GLFW callbacks (mouse
	// wheel, key down). Allocated EARLY in Initialise -- the window may
	// dispatch input events before the main loop kicks in.
	return *m_pxInput;
}

Zenith_TouchInputImpl& Zenith_Engine::Touch()
{
	// No assert: touch state is read every frame from gameplay code.
	// Allocated alongside m_pxInput.
	return *m_pxTouch;
}

Flux_RendererImpl& Zenith_Engine::FluxRenderer()
{
	// No assert: render graph + frame counter + pending command lists
	// are read every frame and from Vulkan worker threads. Allocated
	// EARLY in Initialise -- before Flux::EarlyInitialise creates the
	// graph and pipes through OnResChange / SubmitCommandList paths.
	return *m_pxFluxRenderer;
}

Flux_GraphicsImpl& Zenith_Engine::FluxGraphics()
{
	// No assert: graphics state (samplers, fallback textures, frame
	// constants, MRT formats) is read from every subsystem during every
	// frame. Allocated alongside m_pxFluxRenderer.
	return *m_pxFluxGraphics;
}

Zenith_VulkanImpl& Zenith_Engine::Vulkan()
{
	// No assert: Vulkan instance / device / queues / per-frame state are
	// read from every frame and from Vulkan worker threads. Allocated
	// EARLY in Initialise alongside FluxGraphics so the backend has a
	// live store from Flux::EarlyInitialise onward.
	return *m_pxVulkan;
}

Zenith_Vulkan_MemoryManagerImpl& Zenith_Engine::VulkanMemory()
{
	return *m_pxVulkanMemory;
}

Zenith_Vulkan_SwapchainImpl& Zenith_Engine::VulkanSwapchain()
{
	return *m_pxVulkanSwapchain;
}

Flux_HiZImpl&             Zenith_Engine::HiZ()             { return *m_pxHiZ; }
Flux_StaticMeshesImpl&    Zenith_Engine::StaticMeshes()    { return *m_pxStaticMeshes; }
Flux_AnimatedMeshesImpl&  Zenith_Engine::AnimatedMeshes()  { return *m_pxAnimatedMeshes; }
Flux_DeferredShadingImpl& Zenith_Engine::DeferredShading() { return *m_pxDeferredShading; }
Flux_SDFsImpl&            Zenith_Engine::SDFs()            { return *m_pxSDFs; }
Flux_QuadsImpl&                    Zenith_Engine::Quads()             { return *m_pxQuads; }
Flux_ShadowsImpl&                  Zenith_Engine::Shadows()           { return *m_pxShadows; }
Flux_DynamicLightsImpl&            Zenith_Engine::DynamicLights()     { return *m_pxDynamicLights; }
Flux_LightClusteringImpl&          Zenith_Engine::LightClustering()   { return *m_pxLightClustering; }
Flux_FroxelFogImpl&                Zenith_Engine::FroxelFog()         { return *m_pxFroxelFog; }
Flux_GodRaysFogImpl&               Zenith_Engine::GodRaysFog()        { return *m_pxGodRaysFog; }
Flux_RaymarchFogImpl&              Zenith_Engine::RaymarchFog()       { return *m_pxRaymarchFog; }
Flux_TerrainStreamingManagerImpl&  Zenith_Engine::TerrainStreaming()  { return *m_pxTerrainStreaming; }
Flux_SSAOImpl&                     Zenith_Engine::SSAO()              { return *m_pxSSAO; }
Flux_DecalsImpl&                   Zenith_Engine::Decals()            { return *m_pxDecals; }
Flux_FogImpl&                      Zenith_Engine::Fog()               { return *m_pxFog; }
Flux_VolumeFogImpl&                Zenith_Engine::VolumeFog()         { return *m_pxVolumeFog; }

#ifdef ZENITH_TOOLS
Zenith_EditorImpl& Zenith_Engine::Editor()
{
	// No assert: editor state is read every editor frame and from log
	// callbacks. Allocated in Initialise.
	return *m_pxEditor;
}

Zenith_GizmoImpl&            Zenith_Engine::Gizmo()              { return *m_pxGizmo; }
Zenith_SelectionSystemImpl&  Zenith_Engine::Selection()          { return *m_pxSelection; }
Zenith_UndoSystemImpl&       Zenith_Engine::UndoSystem()         { return *m_pxUndoSystem; }
Zenith_EditorAutomationImpl& Zenith_Engine::EditorAutomation()   { return *m_pxEditorAutomation; }
Zenith_EditorMaterialUIImpl& Zenith_Engine::EditorMaterialUI()   { return *m_pxEditorMaterialUI; }
Zenith_DebugVariablesImpl&   Zenith_Engine::DebugVariables()     { return *m_pxDebugVariables; }
#endif

void Zenith_Engine::Initialise()
{
	// Phase 2: per-frame timing state lives here now. Construct
	// FIRST so any subsystem that reads dt during init sees a sane
	// zero. Last-frame time is bumped at the end of Initialise()
	// below, matching the historical Zenith_Init behaviour.
	Zenith_Assert(m_pxFrame == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFrame = new FrameContext();

	// Phase 5a: global entity storage. Allocate VERY EARLY -- any
	// subsystem that touches entity slots reads g_xEngine.EntityStore()
	// and would dereference nullptr if we wait. Empty store is fine
	// until SceneManager / scene-load starts creating entities.
	Zenith_Assert(m_pxEntityStore == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEntityStore = new Zenith_EntityStore();

	// Phase 5b: scene-registry state (slot table, generations, active /
	// persistent handles, build-index map, name cache). Allocated alongside
	// EntityStore -- both must exist before Zenith_SceneManager::Initialise
	// creates the persistent scene.
	Zenith_Assert(m_pxSceneRegistry == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxSceneRegistry = new Zenith_SceneRegistryImpl();

	// Phase 5c: scene callback bus state (6 callback lists + handle
	// allocator + deferred-removal queue + dispatch-depth + active-scene
	// suppression flags). Must exist before any subsystem registers a
	// callback during init.
	Zenith_Assert(m_pxSceneCallbacks == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxSceneCallbacks = new Zenith_SceneCallbackBusImpl();

	// Phase 5d: async scene-operation pipeline state (operation map +
	// load/unload job queues + re-entrancy depth counters). Must exist
	// before Zenith_SceneManager::Initialise queues any bootstrap loads.
	Zenith_Assert(m_pxSceneOperations == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxSceneOperations = new Zenith_SceneOperationQueueImpl();

	// Phase 5e: scheduler state (lifecycle-deferral flags, fixed-timestep
	// accumulator + config, circular-load stacks, build-index plumb,
	// creation-target stack, main-loop flag, initial-scene-load hook).
	Zenith_Assert(m_pxSceneLifecycle == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxSceneLifecycle = new Zenith_SceneLifecycleSchedulerImpl();

	// Phase 5.5a: per-frame input state (key presses, mouse delta + wheel,
	// gamepad ring buffers). Allocate EARLY so GLFW callbacks fired by
	// Flux::EarlyInitialise (which spins up the window) have a live store
	// to write to.
	Zenith_Assert(m_pxInput == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxInput = new Zenith_InputImpl();

	// Phase 5.5b: touch-gesture state. Allocated alongside m_pxInput.
	Zenith_Assert(m_pxTouch == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTouch = new Zenith_TouchInputImpl();

	// Phase 6a-1: Flux renderer state (frame counter, render graph,
	// pending command-list queue, per-frame callbacks). Must exist
	// before Flux::EarlyInitialise creates the graph.
	Zenith_Assert(m_pxFluxRenderer == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFluxRenderer = new Flux_RendererImpl();

	// Phase 6a-2: Flux_Graphics state (samplers, fallback texture /
	// material handles, scene textures, MRT formats, frame constants,
	// transient render-graph handles).
	Zenith_Assert(m_pxFluxGraphics == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFluxGraphics = new Flux_GraphicsImpl();

	// Phase 6b: Vulkan backend state (instance / device / queues / pools /
	// per-frame ring / VRAM registry / pending command buffers / ImGui).
	// Must exist before Flux::EarlyInitialise -> Zenith_Vulkan::Initialise.
	Zenith_Assert(m_pxVulkan == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkan = new Zenith_VulkanImpl();

	Zenith_Assert(m_pxVulkanMemory == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkanMemory = new Zenith_Vulkan_MemoryManagerImpl();

	Zenith_Assert(m_pxVulkanSwapchain == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkanSwapchain = new Zenith_Vulkan_SwapchainImpl();

	// Phase 7a: HiZ subsystem state.
	Zenith_Assert(m_pxHiZ == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxHiZ = new Flux_HiZImpl();

	// Phase 7b: 5 small Flux subsystems -- mesh pipelines, deferred
	// shading, SDFs, quads.
	Zenith_Assert(m_pxStaticMeshes == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxStaticMeshes    = new Flux_StaticMeshesImpl();
	m_pxAnimatedMeshes  = new Flux_AnimatedMeshesImpl();
	m_pxDeferredShading = new Flux_DeferredShadingImpl();
	m_pxSDFs            = new Flux_SDFsImpl();
	m_pxQuads           = new Flux_QuadsImpl();

	// Phase 7c: 7 more Flux subsystems.
	m_pxShadows          = new Flux_ShadowsImpl();
	m_pxDynamicLights    = new Flux_DynamicLightsImpl();
	m_pxLightClustering  = new Flux_LightClusteringImpl();
	m_pxFroxelFog        = new Flux_FroxelFogImpl();
	m_pxGodRaysFog       = new Flux_GodRaysFogImpl();
	m_pxRaymarchFog      = new Flux_RaymarchFogImpl();
	m_pxTerrainStreaming = new Flux_TerrainStreamingManagerImpl();

	// Phase 7d: 4 more.
	m_pxSSAO       = new Flux_SSAOImpl();
	m_pxDecals     = new Flux_DecalsImpl();
	m_pxFog        = new Flux_FogImpl();
	m_pxVolumeFog  = new Flux_VolumeFogImpl();

#ifdef ZENITH_TOOLS
	// Phase 5.5c: editor state (selection, viewport, content browser,
	// console, panel visibility, camera, ImGui texture cache). Allocate
	// EARLY -- Zenith_Log fires AddLogMessage which writes to the console
	// list, and editor automation runs before the main loop.
	Zenith_Assert(m_pxEditor == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEditor = new Zenith_EditorImpl();

	// Phase 5.5d: editor sub-systems (Gizmo / Selection / Undo / Automation
	// / MaterialUI). Allocate alongside the main editor Impl.
	Zenith_Assert(m_pxGizmo == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxGizmo = new Zenith_GizmoImpl();
	Zenith_Assert(m_pxSelection == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxSelection = new Zenith_SelectionSystemImpl();
	Zenith_Assert(m_pxUndoSystem == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxUndoSystem = new Zenith_UndoSystemImpl();
	Zenith_Assert(m_pxEditorAutomation == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEditorAutomation = new Zenith_EditorAutomationImpl();
	Zenith_Assert(m_pxEditorMaterialUI == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEditorMaterialUI = new Zenith_EditorMaterialUIImpl();

	// Phase 5.7: debug-variable tree. Allocate alongside editor; many
	// subsystems register vars during their own Initialise.
	Zenith_Assert(m_pxDebugVariables == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxDebugVariables = new Zenith_DebugVariablesImpl();
#endif

	// Phase 3a: multithreading registry (thread-ID allocator +
	// main-thread ID) lives on the engine now. Allocate BEFORE
	// Zenith_Multithreading::RegisterThread(true) below, which reads
	// from g_xEngine.Threading() to issue the main thread's ID.
	Zenith_Assert(m_pxThreading == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxThreading = new Zenith_MultithreadingImpl();

	// Populate graphics options from the game project FIRST
	// Must happen before any Flux initialisation reads from Zenith_GraphicsOptions::Get()
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());

	// CRITICAL: Memory tracking must be initialized FIRST to capture all allocations
	Zenith_MemoryManagement::Initialise();

	// Phase 3b: per-Engine Profiling state. Allocate BEFORE
	// Zenith_Multithreading::RegisterThread(true) below --
	// RegisterThread transitively calls Zenith_Profiling::RegisterThread
	// (which reads g_xEngine.Profiling().m_xEvents). The Profiling
	// impl also has to exist before Zenith_Profiling::Initialise()
	// further down for the same reason.
	Zenith_Assert(m_pxProfiling == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxProfiling = new Zenith_ProfilingImpl();

	Zenith_Multithreading::RegisterThread(true);
	Zenith_Profiling::Initialise();

	// Phase 3b: per-Engine TaskSystem state. Allocate BEFORE
	// Zenith_TaskSystem::Inititalise() below, which spawns worker
	// threads whose ThreadFunc reads from g_xEngine.Tasks().
	Zenith_Assert(m_pxTasks == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTasks = new Zenith_TaskSystemImpl();

	Zenith_TaskSystem::Inititalise();

	// Set asset directories before registry initialization
	// Game assets dir comes from the game project (each game defines GAME_ASSETS_DIR)
	Zenith_AssetRegistry::SetGameAssetsDir(Project_GetGameAssetsDir());
#ifdef ENGINE_ASSETS_DIR
	Zenith_AssetRegistry::SetEngineAssetsDir(ENGINE_ASSETS_DIR);
#else
	Zenith_AssetRegistry::SetEngineAssetsDir("./Zenith/Assets/");
#endif

	// Phase 4: Engine owns the AssetRegistry instance. Allocate and
	// install the view-pointer BEFORE Zenith_AssetRegistry::Initialize()
	// (which now only registers loaders and asserts s_pxInstance is set).
	// The ~50 existing call sites of the static facade (Get<T>, Create<T>,
	// Save, etc.) keep working through s_pxInstance until Phase 9 sweeps
	// them away.
	Zenith_Assert(m_pxAssets == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxAssets = new Zenith_AssetRegistry();
	Zenith_AssetRegistry::s_pxInstance = m_pxAssets;
	Zenith_AssetRegistry::Initialize();

#ifdef ZENITH_TOOLS
	if (HasCommandLineFlag("--skip-tool-exports"))
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Tool asset exports skipped by --skip-tool-exports");
	}
	else
	{
		ExportAllMeshes();
		ExportAllTextures();
		//ExportHeightmap();
		ExportDefaultFontAtlas();  // Generate font atlas from TTF
		GenerateTestAssets();      // Generate procedural test assets (StickFigure, Tree)
	}
#endif

	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::EarlyInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::EarlyInitialise();
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Physics::Initialise...");
	// Phase 4: per-Engine Physics state lives on Zenith_PhysicsImpl.
	// Allocate BEFORE Zenith_Physics::Initialise() below -- the static
	// facade now reads/writes g_xEngine.Physics().m_pxXxx for every
	// piece of state it used to keep as Zenith_Physics::s_*.
	Zenith_Assert(m_pxPhysics == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxPhysics = new Zenith_PhysicsImpl();
	Zenith_Physics::Initialise();
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: SceneManager::Initialise...");
	Zenith_SceneManager::Initialise();

	//#TO_TODO: move somewhere sensible
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
		Zenith_AssetRegistry::InitializeGPUDependentAssets();  // Must be after Flux::EarlyInitialise()

		// Load cubemap texture (pinned)
		if (Zenith_TextureAsset* pxCubemap = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
		{
			pxCubemap->LoadCubemapFromFiles(
				ENGINE_ASSETS_DIR"Textures/Cubemap/px" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nx" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/py" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/ny" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/pz" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nz" ZENITH_TEXTURE_EXT
			);
			g_xEngine.FluxGraphics().m_xCubemapTexture.Set(pxCubemap);
		}

		// Load water normal texture (pinned)
		if (Zenith_TextureAsset* pxWaterNormal = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/Water/normal" ZENITH_TEXTURE_EXT))
		{
			g_xEngine.FluxGraphics().m_xWaterNormalTexture.Set(pxWaterNormal);
		}

		Flux_MemoryManager::EndFrame(false);
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::LateInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::LateInitialise();
	}

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_GraphicsOptions::RegisterDebugVariables();
	if (!Zenith_CommandLine::IsHeadless())
	{
		Zenith_Editor::Initialise();
		Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
		Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
		Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
		Zenith_DebugVariables::AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
	}
#endif

	Project_RegisterScriptBehaviours();

#ifdef ZENITH_TOOLS
	// Sync registered script behaviours to disk: write missing game:Scripts/<TypeName>.zscript
	// files for each behaviour that auto-registered via the ZENITH_BEHAVIOUR_TYPE_NAME macro,
	// and rename orphan .zscript files (whose behaviour is no longer registered) to .stale.
	// Must run AFTER static-init / Project_RegisterScriptBehaviours and BEFORE the test runner
	// (so tests can resolve .zscript assets via Zenith_AssetRegistry::Get) and before any scene load.
	Zenith_ScriptAsset::SyncRegisteredTypesToDisk();
#endif

	// Run unit tests BEFORE loading the game scene
	// This ensures tests don't corrupt game entities - scene loads fresh after tests
#ifdef ZENITH_TESTING
	if (HasCommandLineFlag("--skip-unit-tests"))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "Unit tests skipped by --skip-unit-tests");
	}
	else
	{
		Zenith_TestRunner::Instance().RunAllTests();
	}
#endif

#ifdef ZENITH_TOOLS
	// Initialize game-specific resources (geometry, materials, prefabs, particle configs)
	// Must be inside BeginFrame/EndFrame for GPU resource allocation
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
	}
	Project_InitializeResources();
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::EndFrame(false);
	}

	// Register automation steps and begin execution (one step per frame in main loop)
	Project_RegisterEditorAutomationSteps();
	Zenith_EditorAutomation::Begin();
#else
	// Non-tools: load pre-generated scene files
	// Run a tools build first to generate .zscen files
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
	}
	Zenith_SceneManager::SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	{
		Zenith_SceneManager::LifecycleDeferralGuard xLoadingGuard(g_xEngine.SceneLifecycle().m_bIsLoadingScene);
		Project_LoadInitialScene();
	}
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::EndFrame(false);
	}
	Zenith_Assert(Zenith_SceneManager::GetActiveScene().IsValid(),
		"No scene loaded. Run a ZENITH_TOOLS build first to generate .zscen files.");
#endif

	m_pxFrame->SetLastFrameTime(std::chrono::high_resolution_clock::now());
}

void Zenith_Engine::Shutdown()
{
	//--------------------------------------------------------------------------
	// Shutdown sequence - reverse order of initialization
	// Critical: Must wait for GPU before destroying resources it's using
	//--------------------------------------------------------------------------
	Zenith_Log(LOG_CATEGORY_CORE, "Beginning shutdown sequence...");

	// 1. Wait for GPU to finish all pending work
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_PlatformAPI::WaitForGPUIdle();
	}

#ifdef ZENITH_TOOLS
	// 2. Shutdown editor (processes pending deletions, cleans up editor state)
	Zenith_Editor::Shutdown();
#endif

	// 3. Shutdown SceneManager (unloads all scenes, releases resources)
	// Must happen before physics (colliders need to remove bodies) and before
	// memory manager (model/mesh components hold VRAM handles)
	Zenith_SceneManager::Shutdown();

	// 4. Shutdown physics system. The static facade drains state out of
	// g_xEngine.Physics().m_pxXxx; engine then reclaims the Impl below
	// (Phase 4 makes Zenith_Engine the sole owner).
	Zenith_Physics::Shutdown();
	delete m_pxPhysics;
	m_pxPhysics = nullptr;

	// 5. Project shutdown - clean up game-specific resources
	Project_Shutdown();

	// 6. Release Flux's asset-system references BEFORE the registry shuts down.
	// Flux statics hold TextureHandle / MaterialHandle defaults that must drop their
	// refs while the registry still owns its assets — Flux::Shutdown() runs too late.
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::ReleaseAssetReferences();
	}

	// 7. Shutdown asset registry (unloads all assets). Engine then
	// reclaims the instance — Phase 4 makes Zenith_Engine the sole
	// owner; the static facade now drains state only.
	Zenith_AssetRegistry::Shutdown();
	delete m_pxAssets;
	m_pxAssets = nullptr;
	Zenith_AssetRegistry::s_pxInstance = nullptr;

	// 8. Shutdown Flux (all subsystems + graphics + memory manager)
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::Shutdown();
	}

	// 9. Shutdown task system (terminates worker threads)
	Zenith_TaskSystem::Shutdown();

	// 10. Free the per-Engine TaskSystem state. Must come AFTER
	// Zenith_TaskSystem::Shutdown above -- the forwarder there reads
	// from g_xEngine.Tasks() to drive Shutdown.
	delete m_pxTasks;
	m_pxTasks = nullptr;

	// 11. Free the per-Engine Profiling state. Comes AFTER TaskSystem
	// shutdown (step 9) -- worker threads may have profile scopes
	// pending until they exit, and AFTER tearing down m_pxTasks so no
	// stale ThreadFunc can call Profiling.
	delete m_pxProfiling;
	m_pxProfiling = nullptr;

	// 12. Tear down per-frame timing state. Done late so any
	// subsystem shutdown that needs to log dt or read accumulated
	// time still can.
	delete m_pxFrame;
	m_pxFrame = nullptr;

	// 13. Tear down the multithreading registry. Done after the task
	// system shutdown (step 9) so worker threads aren't still calling
	// IsMainThread while we're freeing the registry.
	delete m_pxThreading;
	m_pxThreading = nullptr;

	// 14. Free the per-Engine entity store. Done VERY LATE -- after
	// SceneManager::Shutdown (step 3), Physics::Shutdown (step 4),
	// Project_Shutdown (step 5), Flux::Shutdown (step 8) which all
	// may touch entity slots during teardown.
	delete m_pxEntityStore;
	m_pxEntityStore = nullptr;

	// 15. Free the scene-registry state. SceneManager::Shutdown above
	// already drained the slot table; just reclaim the holder.
	delete m_pxSceneRegistry;
	m_pxSceneRegistry = nullptr;

	// 16. Free the scene callback bus state. SceneCallbackBus::Shutdown
	// (called from SceneManager::Shutdown) already cleared the lists.
	delete m_pxSceneCallbacks;
	m_pxSceneCallbacks = nullptr;

	// 17. Free the scene-operation queue state. SceneOperationQueue::Shutdown
	// (called from SceneManager::Shutdown) already waited for and freed
	// all in-flight load tasks + unload jobs + operation entries.
	delete m_pxSceneOperations;
	m_pxSceneOperations = nullptr;

	// 18. Free the scheduler state. SceneLifecycleScheduler::Shutdown
	// (called from SceneManager::Shutdown) already cleared all flags and
	// terminated the animation update task.
	delete m_pxSceneLifecycle;
	m_pxSceneLifecycle = nullptr;

	// 19. Free per-frame input state. Done LATE -- some Flux/window
	// teardown paths can fire one last GLFW callback.
	delete m_pxInput;
	m_pxInput = nullptr;

	// 20. Free touch-gesture state.
	delete m_pxTouch;
	m_pxTouch = nullptr;

	// Free Flux renderer state. Flux::Shutdown above has already freed the
	// render graph and cleared the callback lists; this just reclaims the
	// holder.
	delete m_pxFluxRenderer;
	m_pxFluxRenderer = nullptr;

	// Free Flux_Graphics state. Flux_Graphics::Shutdown above has already
	// freed GPU resources; this just reclaims the holder.
	delete m_pxFluxGraphics;
	m_pxFluxGraphics = nullptr;

	// Free Vulkan backend state. Flux::Shutdown above tore down the
	// Vulkan-side resources (device, command pools, descriptor sets,
	// memory allocator, swapchain). These delete only the holders.
	delete m_pxVulkanSwapchain;
	m_pxVulkanSwapchain = nullptr;
	delete m_pxVulkanMemory;
	m_pxVulkanMemory = nullptr;
	delete m_pxVulkan;
	m_pxVulkan = nullptr;

	delete m_pxHiZ;
	m_pxHiZ = nullptr;
	delete m_pxStaticMeshes;    m_pxStaticMeshes = nullptr;
	delete m_pxAnimatedMeshes;  m_pxAnimatedMeshes = nullptr;
	delete m_pxDeferredShading; m_pxDeferredShading = nullptr;
	delete m_pxSDFs;            m_pxSDFs = nullptr;
	delete m_pxQuads;           m_pxQuads = nullptr;
	delete m_pxShadows;          m_pxShadows = nullptr;
	delete m_pxDynamicLights;    m_pxDynamicLights = nullptr;
	delete m_pxLightClustering;  m_pxLightClustering = nullptr;
	delete m_pxFroxelFog;        m_pxFroxelFog = nullptr;
	delete m_pxGodRaysFog;       m_pxGodRaysFog = nullptr;
	delete m_pxRaymarchFog;      m_pxRaymarchFog = nullptr;
	delete m_pxTerrainStreaming; m_pxTerrainStreaming = nullptr;
	delete m_pxSSAO;       m_pxSSAO = nullptr;
	delete m_pxDecals;     m_pxDecals = nullptr;
	delete m_pxFog;        m_pxFog = nullptr;
	delete m_pxVolumeFog;  m_pxVolumeFog = nullptr;

#ifdef ZENITH_TOOLS
	// 21. Free editor state. Done LATE -- the editor's deferred-deletion
	// queue gets drained by Flux/Vulkan teardown above.
	delete m_pxEditor;
	m_pxEditor = nullptr;
	delete m_pxGizmo;
	m_pxGizmo = nullptr;
	delete m_pxSelection;
	m_pxSelection = nullptr;
	delete m_pxUndoSystem;
	m_pxUndoSystem = nullptr;
	delete m_pxEditorAutomation;
	m_pxEditorAutomation = nullptr;
	delete m_pxEditorMaterialUI;
	m_pxEditorMaterialUI = nullptr;
	delete m_pxDebugVariables;
	m_pxDebugVariables = nullptr;
#endif

	Zenith_Log(LOG_CATEGORY_CORE, "Shutdown complete");
}
