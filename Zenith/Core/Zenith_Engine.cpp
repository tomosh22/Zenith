#include "Zenith.h"

#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/FrameContext.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "ZenithECS/Internal/Zenith_EntityStore.h"
#include "ZenithECS/Zenith_SceneSystem.h"
// ECS leaf-extraction Phase 4: needed so Initialise can install the engine-side
// built-in component registrar onto the ECS reflection core
// (SetComponentRegistrar) and force the one-time EnsureInitialized() drain. The
// core itself names no concrete type; the engine wires the registrar in here.
#include "ZenithECS/Zenith_ComponentMeta.h"
// Engine-side (NOT the ECS leaf): needed so the m_pfnAddDefaultComponents hook
// installed below can name Zenith_TransformComponent and add it via the
// AddComponent<> template. Keeping this name on the engine side is exactly how
// the ECS core stays Transform-free.
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_TouchInput.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_Swapchain.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/MeshAnimation/Flux_AnimationControllerStore.h"
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
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Particles/Flux_ParticleGPUImpl.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "ZenithECS/Zenith_Scene.h"
#include "Flux/Flux_GraphicsImpl.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "Editor/Zenith_Gizmo.h"
#include "Editor/Zenith_SelectionSystem.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#endif
#include "Physics/Zenith_Physics.h"
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

// Engine-side built-in component registrar (ECS leaf-extraction Phase 4). Defined
// in EntityComponent/Zenith_ComponentMeta_Registration.cpp -- the single TU that
// knows the concrete built-in component set. Forward-declared here (not via an
// include) so the engine can install it on the ECS reflection core without the
// core ever naming a concrete component type. Installed + invoked in Initialise.
void Zenith_RegisterEngineComponents();

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

Zenith_Multithreading& Zenith_Engine::Threading()
{
	// NOTE: no assert here. Threading() is read on the IsMainThread
	// hot path (every Zenith_Assert that gates on main-thread fires
	// it), and the engine guarantees m_pxThreading exists before any
	// thread can call RegisterThread / IsMainThread (see Initialise).
	return *m_pxThreading;
}

Zenith_TaskSystem& Zenith_Engine::Tasks()
{
	// No assert: SubmitTask is a hot path, and Zenith_Engine::Initialise
	// allocates m_pxTasks before g_xEngine.Tasks().Initialise() (the
	// forwarder that brings worker threads online).
	return *m_pxTasks;
}

Zenith_Profiling& Zenith_Engine::Profiling()
{
	// No assert: BeginProfile / EndProfile sit on the hottest path in
	// the engine (every ZENITH_PROFILING_FUNCTION_WRAPPER call fires
	// it). Zenith_Engine::Initialise allocates m_pxProfiling before
	// g_xEngine.Profiling().Initialise() so the impl is always available
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

Zenith_Physics& Zenith_Engine::Physics()
{
	Zenith_Assert(m_pxPhysics != nullptr,
		"Zenith_Engine::Physics() called before Initialise() or after Shutdown(). "
		"Physics is unavailable outside the engine lifetime.");
	return *m_pxPhysics;
}

Zenith_EntityStore& Zenith_Engine::EntityStore()
{
	// Phase 2.1 (ECS leaf-extraction): the entity store is now OWNED by the
	// Zenith_SceneSystem (m_pxScenes), not by the engine. This accessor forwards
	// so every existing g_xEngine.EntityStore() call site keeps compiling and
	// behaving identically; the bulk repoint of those call sites onto
	// Zenith_SceneSystem::Get().GetEntityStore() is Phase 2.2.
	//
	// No assert (unchanged hot-path rationale): EntityStore is read on the hot
	// path for every entity access (lifecycle queries, component lookups).
	// Zenith_Engine::Initialise allocates m_pxScenes very early -- before
	// Physics init / scene bootstrap -- and the SceneSystem ctor allocates the
	// store, so it is always available once any subsystem touches entity slots.
	return m_pxScenes->GetEntityStore();
}

Zenith_SceneSystem& Zenith_Engine::Scenes()
{
	// One scene-system instance. Allocated VERY EARLY in Initialise so it's
	// live before any subsystem touches scene state. Holds all scene state
	// directly (registry / operations / lifecycle / callbacks / entity-
	// ownership all merged in — no per-subsystem pointers any more).
	return *m_pxScenes;
}

Zenith_Input& Zenith_Engine::Input()
{
	// No assert: input is read every frame and from GLFW callbacks (mouse
	// wheel, key down). Allocated EARLY in Initialise -- the window may
	// dispatch input events before the main loop kicks in.
	return *m_pxInput;
}

Zenith_TouchInput& Zenith_Engine::Touch()
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

Zenith_Vulkan& Zenith_Engine::Vulkan()
{
	// No assert: Vulkan instance / device / queues / per-frame state are
	// read from every frame and from Vulkan worker threads. Allocated
	// EARLY in Initialise alongside FluxGraphics so the backend has a
	// live store from Flux::EarlyInitialise onward.
	return *m_pxVulkan;
}

Zenith_Vulkan_MemoryManager& Zenith_Engine::VulkanMemory()
{
	return *m_pxVulkanMemory;
}

Zenith_Vulkan_Swapchain& Zenith_Engine::VulkanSwapchain()
{
	return *m_pxVulkanSwapchain;
}

Flux_HiZImpl&             Zenith_Engine::HiZ()             { return *m_pxHiZ; }
Flux_StaticMeshesImpl&    Zenith_Engine::StaticMeshes()    { return *m_pxStaticMeshes; }
Flux_AnimatedMeshesImpl&  Zenith_Engine::AnimatedMeshes()  { return *m_pxAnimatedMeshes; }
Flux_AnimationControllerStore& Zenith_Engine::AnimationControllers() { return *m_pxAnimationControllers; }
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
Flux_ParticlesImpl&                Zenith_Engine::Particles()         { return *m_pxParticles; }
Flux_ParticleGPUImpl&              Zenith_Engine::ParticleGPU()       { return *m_pxParticleGPU; }
Flux_TextImpl&                     Zenith_Engine::Text()              { return *m_pxText; }
Flux_InstancedMeshesImpl&          Zenith_Engine::InstancedMeshes()   { return *m_pxInstancedMeshes; }
Flux_SSRImpl&                      Zenith_Engine::SSR()               { return *m_pxSSR; }
Flux_SSGIImpl&                     Zenith_Engine::SSGI()              { return *m_pxSSGI; }
Flux_IBLImpl&                      Zenith_Engine::IBL()               { return *m_pxIBL; }
Flux_SkyboxImpl&                   Zenith_Engine::Skybox()            { return *m_pxSkybox; }
Flux_GrassImpl&                    Zenith_Engine::Grass()             { return *m_pxGrass; }
Flux_PrimitivesImpl&               Zenith_Engine::Primitives()        { return *m_pxPrimitives; }
Flux_HDRImpl&                      Zenith_Engine::HDR()               { return *m_pxHDR; }
Flux_TerrainImpl&                  Zenith_Engine::Terrain()           { return *m_pxTerrain; }
#ifdef ZENITH_TOOLS
Flux_GizmosImpl&                   Zenith_Engine::Gizmos()            { return *m_pxGizmos; }
#endif

#ifdef ZENITH_TOOLS
Zenith_Editor& Zenith_Engine::Editor()
{
	// No assert: editor state is read every editor frame and from log
	// callbacks. Allocated in Initialise.
	return *m_pxEditor;
}

Zenith_Gizmo&            Zenith_Engine::Gizmo()              { return *m_pxGizmo; }
Zenith_SelectionSystem&  Zenith_Engine::Selection()          { return *m_pxSelection; }
Zenith_UndoSystem&       Zenith_Engine::UndoSystem()         { return *m_pxUndoSystem; }
Zenith_EditorAutomation& Zenith_Engine::EditorAutomation()   { return *m_pxEditorAutomation; }
Zenith_EditorMaterialUI& Zenith_Engine::EditorMaterialUI()   { return *m_pxEditorMaterialUI; }
Zenith_DebugVariables&   Zenith_Engine::DebugVariables()     { return *m_pxDebugVariables; }
#endif

void Zenith_Engine::Initialise()
{
	// Phase 2: per-frame timing state lives here now. Construct
	// FIRST so any subsystem that reads dt during init sees a sane
	// zero. Last-frame time is bumped at the end of Initialise()
	// below, matching the historical Zenith_Init behaviour.
	Zenith_Assert(m_pxFrame == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFrame = new FrameContext();

	// Scene system state (slot table, generations, active / persistent handles,
	// build-index map, name cache, callbacks, lifecycle). Allocate VERY EARLY --
	// any subsystem that touches entity slots reads g_xEngine.EntityStore(),
	// which now forwards to m_pxScenes->GetEntityStore(), and would dereference
	// nullptr if we wait.
	//
	// Phase 2.1 (ECS leaf-extraction): the global entity storage (formerly the
	// engine-owned m_pxEntityStore, allocated here VERY EARLY) is now owned by
	// the Zenith_SceneSystem and allocated inside its ctor. So constructing
	// m_pxScenes here brings the entity store online too -- preserving the old
	// invariant that the store exists before Zenith_SceneSystem::InitialiseSubsystems
	// (and before Physics::Initialise / the first scene load) creates entities.
	Zenith_Assert(m_pxScenes == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxScenes = new Zenith_SceneSystem();

	// Phase 5.5a: per-frame input state (key presses, mouse delta + wheel,
	// gamepad ring buffers). Allocate EARLY so GLFW callbacks fired by
	// g_xEngine.FluxRenderer().EarlyInitialise (which spins up the window) have a live store
	// to write to.
	Zenith_Assert(m_pxInput == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxInput = new Zenith_Input();

	// Phase 5.5b: touch-gesture state. Allocated alongside m_pxInput.
	Zenith_Assert(m_pxTouch == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTouch = new Zenith_TouchInput();

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
	m_pxVulkan = new Zenith_Vulkan();

	Zenith_Assert(m_pxVulkanMemory == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkanMemory = new Zenith_Vulkan_MemoryManager();

	Zenith_Assert(m_pxVulkanSwapchain == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkanSwapchain = new Zenith_Vulkan_Swapchain();

	// Phase 7a: HiZ subsystem state.
	Zenith_Assert(m_pxHiZ == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxHiZ = new Flux_HiZImpl();

	// Phase 7b: 5 small Flux subsystems -- mesh pipelines, deferred
	// shading, SDFs, quads.
	Zenith_Assert(m_pxStaticMeshes == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxStaticMeshes    = new Flux_StaticMeshesImpl();
	m_pxAnimatedMeshes  = new Flux_AnimatedMeshesImpl();
	// Wave-19: heap-stable owning store of per-entity Flux_AnimationControllers.
	// Allocated alongside the mesh subsystems; freed in Shutdown BEFORE the
	// Vulkan device (each controller owns a GPU bone buffer).
	m_pxAnimationControllers = new Flux_AnimationControllerStore();
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

	// Phase 7e: 4 more.
	m_pxParticles       = new Flux_ParticlesImpl();
	m_pxParticleGPU     = new Flux_ParticleGPUImpl();
	m_pxText            = new Flux_TextImpl();
	m_pxInstancedMeshes = new Flux_InstancedMeshesImpl();

	// Phase 7f: multi-pass effects.
	m_pxSSR  = new Flux_SSRImpl();
	m_pxSSGI = new Flux_SSGIImpl();
	m_pxIBL  = new Flux_IBLImpl();

	// Phase 7g: large subsystems.
	m_pxSkybox     = new Flux_SkyboxImpl();
	m_pxGrass      = new Flux_GrassImpl();
	m_pxPrimitives = new Flux_PrimitivesImpl();

	// Phase 7h: biggest subsystems.
	m_pxHDR     = new Flux_HDRImpl();
	m_pxTerrain = new Flux_TerrainImpl();
#ifdef ZENITH_TOOLS
	m_pxGizmos          = new Flux_GizmosImpl();
#endif

#ifdef ZENITH_TOOLS
	// Phase 5.5c: editor state (selection, viewport, content browser,
	// console, panel visibility, camera, ImGui texture cache). Allocate
	// EARLY -- Zenith_Log fires AddLogMessage which writes to the console
	// list, and editor automation runs before the main loop.
	Zenith_Assert(m_pxEditor == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEditor = new Zenith_Editor();

	// Phase 5.5d: editor sub-systems (Gizmo / Selection / Undo / Automation
	// / MaterialUI). Allocate alongside the main editor Impl.
	Zenith_Assert(m_pxGizmo == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxGizmo = new Zenith_Gizmo();
	Zenith_Assert(m_pxSelection == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxSelection = new Zenith_SelectionSystem();
	Zenith_Assert(m_pxUndoSystem == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxUndoSystem = new Zenith_UndoSystem();
	Zenith_Assert(m_pxEditorAutomation == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEditorAutomation = new Zenith_EditorAutomation();
	Zenith_Assert(m_pxEditorMaterialUI == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEditorMaterialUI = new Zenith_EditorMaterialUI();

	// Phase 5.7: debug-variable tree. Allocate alongside editor; many
	// subsystems register vars during their own Initialise.
	Zenith_Assert(m_pxDebugVariables == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxDebugVariables = new Zenith_DebugVariables();
#endif

	// Phase 3a: multithreading registry (thread-ID allocator +
	// main-thread ID) lives on the engine now. Allocate BEFORE
	// g_xEngine.Threading().RegisterThread(true) below, which reads
	// from g_xEngine.Threading() to issue the main thread's ID.
	Zenith_Assert(m_pxThreading == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxThreading = new Zenith_Multithreading();

	// Populate graphics options from the game project FIRST
	// Must happen before any Flux initialisation reads from Zenith_GraphicsOptions::Get()
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());

	// CRITICAL: Memory tracking must be initialized FIRST to capture all allocations
	Zenith_MemoryManagement::Initialise();

	// Phase 3b: per-Engine Profiling state. Allocate BEFORE
	// g_xEngine.Threading().RegisterThread(true) below --
	// RegisterThread transitively calls Zenith_Profiling::RegisterThread
	// (which reads g_xEngine.Profiling().m_xEvents). The Profiling
	// impl also has to exist before g_xEngine.Profiling().Initialise()
	// further down for the same reason.
	Zenith_Assert(m_pxProfiling == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxProfiling = new Zenith_Profiling();

	g_xEngine.Threading().RegisterThread(true);
	g_xEngine.Profiling().Initialise(g_xEngine.Threading());

	// Phase 3b: per-Engine TaskSystem state. Allocate BEFORE
	// g_xEngine.Tasks().Initialise() below, which spawns worker
	// threads whose ThreadFunc reads from g_xEngine.Tasks().
	Zenith_Assert(m_pxTasks == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTasks = new Zenith_TaskSystem();

	g_xEngine.Tasks().Initialise();

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
		g_xEngine.FluxRenderer().EarlyInitialise();
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Physics::Initialise...");
	// Phase 4: per-Engine Physics state lives on Zenith_Physics.
	// Allocate BEFORE g_xEngine.Physics().Initialise() below -- the static
	// facade now reads/writes g_xEngine.Physics().m_pxXxx for every
	// piece of state it used to keep as Zenith_Physics::s_*.
	Zenith_Assert(m_pxPhysics == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxPhysics = new Zenith_Physics();
	g_xEngine.Physics().Initialise(g_xEngine.Input());

	// ECS leaf-extraction Phase 4: install the engine-side built-in component
	// registrar onto the ECS reflection core, then force the one-time
	// EnsureInitialized() drain. Done BEFORE scene bootstrap (and before the first
	// scene load / serialize / lifecycle dispatch that would otherwise trigger the
	// lazy init) so the registry is populated + sealed deterministically here. The
	// registrar names every concrete built-in (and, in TOOLS, mirrors them into
	// the editor "Add Component" menu); the ECS core stays leaf-clean by reaching
	// it only through this opaque function pointer.
	Zenith_ComponentMetaRegistry::Get().SetComponentRegistrar(&Zenith_RegisterEngineComponents);
	Zenith_ComponentMetaRegistry::Get().EnsureInitialized();

	// W6.3: registration-verification diagnostic (lifecycle hardening). The registrar
	// above explicitly names every engine built-in, so they cannot be dead-stripped
	// individually — but if the registrar TU were stripped, EnsureInitialized() never
	// ran, or a built-in were dropped from Zenith_ComponentMeta_Registration.cpp (e.g. a
	// botched merge), the registry would be missing types and component (de)serialization
	// would silently fail downstream. Surface that loudly at boot, release-survivable via
	// the Zenith_Check tier. (A per-game expected-component manifest can extend this.)
	{
		Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
		const auto& xRegisteredMetas = xRegistry.GetAllMetasSorted();
		Zenith_Check(xRegisteredMetas.GetSize() != 0,
			"Component registry is EMPTY after EnsureInitialized() — the registrar was dead-stripped or never ran; component (de)serialization will fail");
		Zenith_Log(LOG_CATEGORY_CORE, "W6.3: %u component types registered at boot", xRegisteredMetas.GetSize());

		static const char* const s_aszCoreEngineComponents[] = { "Transform", "Model", "Camera", "Light", "Collider", "Terrain" };
		for (const char* szExpected : s_aszCoreEngineComponents)
		{
			Zenith_Check(xRegistry.GetMetaByName(szExpected) != nullptr,
				"Core engine component '%s' is NOT registered after boot — registrar regression", szExpected);
		}
	}

	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: scene system bootstrap...");
	Zenith_SceneSystem::InitialiseSubsystems();

	// Install the ECS-leaf runtime hooks (see Zenith_ECSRuntimeHooks.h). The ECS
	// core's leaf-unsafe actions -- render-system reset, asset unload, physics
	// reset, and the main-thread predicate -- are forwarded through these
	// captureless function pointers so the ECS never names g_xEngine / Flux /
	// Physics / AssetHandling directly. The reset bodies are copied VERBATIM from
	// the former Zenith_SceneSystem::ResetAllRenderSystems() / UnloadUnusedAssets()
	// / the SINGLE-teardown physics-reset line, so SCENE_LOAD_SINGLE behaviour is
	// byte-for-byte identical. Installed here -- after the subsystems exist
	// (Physics::Initialise above; Flux EarlyInitialise above; the rest are reached
	// only when a SINGLE teardown fires, which cannot happen before the first
	// scene load below) and before that first scene load -- so any teardown the
	// initial load triggers sees the hooks already wired.
	Zenith_ECSRuntimeHooks xHooks;
	xHooks.m_pfnIsMainThread       = []() -> bool { return g_xEngine.Threading().IsMainThread(); };
	xHooks.m_pfnResetRenderSystems = []() { g_xEngine.Terrain().Reset(); g_xEngine.Text().Reset(); g_xEngine.Particles().Reset(); g_xEngine.Skybox().Reset(); g_xEngine.Fog().Reset();
#ifdef ZENITH_TOOLS
		g_xEngine.Gizmos().Reset();
#endif
	};
	xHooks.m_pfnUnloadUnusedAssets = []() { Zenith_AssetRegistry::UnloadUnused(); };
	xHooks.m_pfnResetPhysics       = []() { g_xEngine.Physics().Reset(); };
	// Default-components hook (Phase 3, ECS leaf-extraction): SceneSystem::CreateEntity
	// runs this after allocating the slot so every newly-created (non-bare) entity
	// gets a Transform — exactly what the old creating ctor did with its
	// AddComponent<Zenith_TransformComponent>() call. Installed engine-side so the
	// ECS leaf never names the component type.
	xHooks.m_pfnAddDefaultComponents = [](Zenith_Entity& xEntity) { xEntity.AddComponent<Zenith_TransformComponent>(); };
	g_xEngine.Scenes().SetRuntimeHooks(xHooks);

	//#TO_TODO: move somewhere sensible
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.VulkanMemory().BeginFrame();
		Zenith_AssetRegistry::InitializeGPUDependentAssets();  // Must be after g_xEngine.FluxRenderer().EarlyInitialise()

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

		g_xEngine.VulkanMemory().EndFrame(false);
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::LateInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxRenderer().LateInitialise();
	}

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_GraphicsOptions::RegisterDebugVariables();
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.Editor().Initialise();
		g_xEngine.DebugVariables().AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
		g_xEngine.DebugVariables().AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
		g_xEngine.DebugVariables().AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
		g_xEngine.DebugVariables().AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
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
		// Install the ECS per-test reset seam here, where the engine TU
		// legitimately owns the L1 reference (Zenith_SceneSystem). Keeps the
		// L0 Core test runner free of any ECS include/symbol dependency.
		Zenith_TestRunner::SetResetHook(&Zenith_SceneSystem::ResetForNextTest);
		Zenith_TestRunner::Instance().RunAllTests();
	}
#endif

#ifdef ZENITH_TOOLS
	// Initialize game-specific resources (geometry, materials, prefabs, particle configs)
	// Must be inside BeginFrame/EndFrame for GPU resource allocation
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.VulkanMemory().BeginFrame();
	}
	Project_InitializeResources();
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.VulkanMemory().EndFrame(false);
	}

	// Register automation steps and begin execution (one step per frame in main loop)
	Project_RegisterEditorAutomationSteps();
	g_xEngine.EditorAutomation().Begin();
#else
	// Non-tools: load pre-generated scene files
	// Run a tools build first to generate .zscen files
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.VulkanMemory().BeginFrame();
	}
	g_xEngine.Scenes().SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	{
		Zenith_LifecycleDeferralGuard xLoadingGuard(g_xEngine.Scenes().MutableLifecycleLoadingFlagForGuard());
		Project_LoadInitialScene();
	}
	// The lifecycle-deferral guard above holds m_bIsLoadingScene true while
	// Project_LoadInitialScene runs, so LoadSceneByIndex/LoadScene take their
	// re-entrancy DEFER branch and queue the initial load to m_xPendingLoad
	// instead of loading it synchronously. The tools path drains that pending
	// load on the first main-loop Update; the non-tools bootstrap asserts an
	// active scene below before any Update runs, so drain it explicitly here
	// (the guard has cleared m_bIsLoadingScene, so the drained load runs
	// synchronously). Without this the initial scene never loads.
	g_xEngine.Scenes().DrainPendingLoadIfAny();
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.VulkanMemory().EndFrame(false);
	}
	Zenith_Assert(g_xEngine.Scenes().GetActiveScene().IsValid(),
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
		g_xEngine.Vulkan().WaitForGPUIdle();
	}

#ifdef ZENITH_TOOLS
	// 2. Shutdown editor (processes pending deletions, cleans up editor state)
	g_xEngine.Editor().Shutdown();
#endif

	// 3. Shutdown scene system (unloads all scenes, releases resources)
	// Must happen before physics (colliders need to remove bodies) and before
	// memory manager (model/mesh components hold VRAM handles)
	Zenith_SceneSystem::ShutdownSubsystems();

	// 4. Shutdown physics system. The static facade drains state out of
	// g_xEngine.Physics().m_pxXxx; engine then reclaims the Impl below
	// (Phase 4 makes Zenith_Engine the sole owner).
	g_xEngine.Physics().Shutdown();
	delete m_pxPhysics;
	m_pxPhysics = nullptr;

	// 5. Project shutdown - clean up game-specific resources
	Project_Shutdown();

	// 6. Release Flux's asset-system references BEFORE the registry shuts down.
	// Flux statics hold TextureHandle / MaterialHandle defaults that must drop their
	// refs while the registry still owns its assets — g_xEngine.FluxRenderer().Shutdown() runs too late.
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxRenderer().ReleaseAssetReferences();
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
		g_xEngine.FluxRenderer().Shutdown();
	}

	// 9. Shutdown task system (terminates worker threads)
	g_xEngine.Tasks().Shutdown();

	// 10. Free the per-Engine TaskSystem state. Must come AFTER
	// g_xEngine.Tasks().Shutdown above -- the forwarder there reads
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

	// 14/15. Free the scene system. ShutdownSubsystems above already drained
	// the slot table, cleared callback lists, terminated the animation task,
	// etc.; this just reclaims the holder + its internal members.
	//
	// Phase 2.1 (ECS leaf-extraction): the per-process entity store is now OWNED
	// by the Zenith_SceneSystem -- its dtor frees the store. There is no longer a
	// separate `delete m_pxEntityStore`. This delete therefore frees the store
	// too, and it stays VERY LATE (after SceneManager::ShutdownSubsystems step 3,
	// Physics::Shutdown step 4, Project_Shutdown step 5, FluxRenderer::Shutdown
	// step 8 -- all of which may touch entity slots during teardown), preserving
	// the old store-teardown ordering since the store's lifetime == the
	// SceneSystem's.
	delete m_pxScenes;
	m_pxScenes = nullptr;

	// 19. Free per-frame input state. Done LATE -- some Flux/window
	// teardown paths can fire one last GLFW callback.
	delete m_pxInput;
	m_pxInput = nullptr;

	// 20. Free touch-gesture state.
	delete m_pxTouch;
	m_pxTouch = nullptr;

	// Free Flux subsystem holders before Vulkan holders. Flux::Shutdown above
	// has drained subsystem-owned state, but these deletes run member dtors;
	// many members are Flux_Shader / Flux_Pipeline wrappers that need the
	// Vulkan device to destroy any remaining native handles.
	delete m_pxHiZ;
	m_pxHiZ = nullptr;
	delete m_pxStaticMeshes;    m_pxStaticMeshes = nullptr;
	delete m_pxAnimatedMeshes;  m_pxAnimatedMeshes = nullptr;
	// Wave-19: free the animation-controller store here (before the Vulkan
	// device teardown below). By this point scene teardown has destroyed all
	// entities, so each component's OnDestroy/dtor has already Destroy()'d its
	// controller; this delete reclaims the (now-empty) store and frees any
	// straggler controllers' GPU bone buffers while the device is still alive.
	delete m_pxAnimationControllers; m_pxAnimationControllers = nullptr;
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
	delete m_pxParticles;       m_pxParticles = nullptr;
	delete m_pxParticleGPU;     m_pxParticleGPU = nullptr;
	delete m_pxText;            m_pxText = nullptr;
	delete m_pxInstancedMeshes; m_pxInstancedMeshes = nullptr;
	delete m_pxSSR;  m_pxSSR  = nullptr;
	delete m_pxSSGI; m_pxSSGI = nullptr;
	delete m_pxIBL;  m_pxIBL  = nullptr;
	delete m_pxSkybox;     m_pxSkybox     = nullptr;
	delete m_pxGrass;      m_pxGrass      = nullptr;
	delete m_pxPrimitives; m_pxPrimitives = nullptr;
	delete m_pxHDR;     m_pxHDR     = nullptr;
	delete m_pxTerrain; m_pxTerrain = nullptr;
#ifdef ZENITH_TOOLS
	delete m_pxGizmos;          m_pxGizmos = nullptr;
#endif

	// Free Flux renderer state. Flux::Shutdown above has already freed the
	// render graph and cleared the callback lists; this just reclaims the
	// holder.
	delete m_pxFluxRenderer;
	m_pxFluxRenderer = nullptr;

	// Free Flux_Graphics state. g_xEngine.FluxGraphics().Shutdown above has
	// already freed GPU resources; this just reclaims the holder.
	delete m_pxFluxGraphics;
	m_pxFluxGraphics = nullptr;

	// Free Vulkan backend state last among graphics holders so device-backed
	// member destructors above can still call g_xEngine.Vulkan().GetDevice().
	delete m_pxVulkanSwapchain;
	m_pxVulkanSwapchain = nullptr;
	delete m_pxVulkanMemory;
	m_pxVulkanMemory = nullptr;
	delete m_pxVulkan;
	m_pxVulkan = nullptr;

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
