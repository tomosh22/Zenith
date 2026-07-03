#include "Zenith.h"

#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
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
// Behaviour Graphs: the node-type registry the engine installs its node
// registrar onto (the Scripting module's twin of the component-meta inversion).
#include "Scripting/Zenith_GraphNodeRegistry.h"
// Engine-side (NOT the ECS leaf): needed so the m_pfnAddDefaultComponents hook
// installed below can name Zenith_TransformComponent and add it via the
// AddComponent<> template. Keeping this name on the engine side is exactly how
// the ECS core stays Transform-free.
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_UISystem.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_TouchInput.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"   // complete type to allocate the by-ptr snapshot in AllocateRenderer
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_BackendTypes.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
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
#include "Flux/UnifiedMesh/Flux_UnifiedMeshImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/Translucency/Flux_TranslucencyImpl.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/TAA/Flux_TAAImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Present/Flux_PresentImpl.h"
#include "ZenithECS/Zenith_Scene.h"
#include "Flux/Flux_GraphicsImpl.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_SceneGraphDebug.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "Editor/Zenith_Gizmo.h"
#include "Editor/Zenith_SelectionSystem.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"
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
extern void Project_RegisterGameComponents();
extern void Project_Shutdown();

// Engine-side built-in component registrar (ECS leaf-extraction Phase 4). Defined
// in EntityComponent/Zenith_ComponentMeta_Registration.cpp -- the single TU that
// knows the concrete built-in component set. Forward-declared here (not via an
// include) so the engine can install it on the ECS reflection core without the
// core ever naming a concrete component type. Installed + invoked in Initialise.
void Zenith_RegisterEngineComponents();

// Engine-side Behaviour Graph node registrar (the scripting system's twin of the
// component registrar above). Defined in
// EntityComponent/Zenith_GraphNode_Registration.cpp; installed on the (leaf-clean)
// Zenith_GraphNodeRegistry in Initialise, drained lazily on first registry use.
void Zenith_RegisterEngineGraphNodes();

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

// -----------------------------------------------------------------------------
// Subsystem accessors. Two policies, named so the choice is visible at the call
// instead of buried in per-accessor prose:
//   ZENITH_ENGINE_ACCESSOR         — asserts the subsystem pointer is non-null.
//        For lifecycle-bounded subsystems whose accessor can be reached outside
//        [Initialise, Shutdown]; the assert turns a use-before-init / use-after-
//        shutdown into a clear message.
//   ZENITH_ENGINE_ACCESSOR_HOTPATH — NO assert. For accessors on a per-frame /
//        worker-thread hot path (render record callbacks reach these from worker
//        threads every frame) that Initialise() guarantees are allocated before
//        any caller runs — the null branch isn't worth paying there.
// Per-accessor assert status is UNCHANGED from the hand-written originals; this
// only makes the existing policy explicit and uniform.
// -----------------------------------------------------------------------------
#define ZENITH_ENGINE_ACCESSOR(Type, Name, Member) \
	Type& Zenith_Engine::Name() \
	{ \
		Zenith_Assert(Member != nullptr, \
			"Zenith_Engine::" #Name "() called before Initialise() or after Shutdown()."); \
		return *Member; \
	}
#define ZENITH_ENGINE_ACCESSOR_HOTPATH(Type, Name, Member) \
	Type& Zenith_Engine::Name() { return *Member; }

ZENITH_ENGINE_ACCESSOR        (FrameContext,          Frame,        m_pxFrame)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_Multithreading, Threading,    m_pxThreading)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_TaskSystem,     Tasks,        m_pxTasks)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_Profiling,      Profiling,    m_pxProfiling)
ZENITH_ENGINE_ACCESSOR        (Zenith_AssetRegistry,  Assets,       m_pxAssets)
ZENITH_ENGINE_ACCESSOR        (Zenith_Physics,        Physics,      m_pxPhysics)

// EntityStore forwards to the SceneSystem-owned store (not a direct member), so
// it can't use the macro. No assert: same per-entity hot-path rationale as the
// HOTPATH accessors; m_pxScenes is allocated very early in Initialise and its
// ctor allocates the store.
Zenith_EntityStore& Zenith_Engine::EntityStore() { return m_pxScenes->GetEntityStore(); }

ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_SceneSystem,    Scenes,        m_pxScenes)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_UISystem,       UI,            m_pxUISystem)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_Input,          Input,         m_pxInput)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_TouchInput,     Touch,         m_pxTouch)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_RendererImpl,     FluxRenderer,  m_pxFluxRenderer)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_GraphicsImpl,     FluxGraphics,  m_pxFluxGraphics)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_PlatformAPI,      FluxBackend,   m_pxVulkan)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_MemoryManager,    FluxMemory,    m_pxVulkanMemory)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_Swapchain,        FluxSwapchain, m_pxVulkanSwapchain)

ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_HiZImpl,                      HiZ,                  m_pxHiZ)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_AnimationControllerStore,    AnimationControllers, m_pxAnimationControllers)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_DeferredShadingImpl,         DeferredShading,      m_pxDeferredShading)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_SDFsImpl,                    SDFs,                 m_pxSDFs)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_QuadsImpl,                   Quads,                m_pxQuads)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_ShadowsImpl,                 Shadows,              m_pxShadows)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_DynamicLightsImpl,           DynamicLights,        m_pxDynamicLights)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_LightClusteringImpl,         LightClustering,      m_pxLightClustering)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_FroxelFogImpl,               FroxelFog,            m_pxFroxelFog)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_GodRaysFogImpl,              GodRaysFog,           m_pxGodRaysFog)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_RaymarchFogImpl,             RaymarchFog,          m_pxRaymarchFog)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_TerrainStreamingManagerImpl, TerrainStreaming,     m_pxTerrainStreaming)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_SSAOImpl,                    SSAO,                 m_pxSSAO)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_DecalsImpl,                  Decals,               m_pxDecals)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_FogImpl,                     Fog,                  m_pxFog)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_VolumeFogImpl,               VolumeFog,            m_pxVolumeFog)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_ParticlesImpl,               Particles,            m_pxParticles)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_ParticleGPUImpl,             ParticleGPU,          m_pxParticleGPU)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_TextImpl,                    Text,                 m_pxText)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_InstancedMeshesImpl,         InstancedMeshes,      m_pxInstancedMeshes)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_UnifiedMeshImpl,             UnifiedMesh,          m_pxUnifiedMesh)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_SSRImpl,                     SSR,                  m_pxSSR)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_SSGIImpl,                    SSGI,                 m_pxSSGI)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_IBLImpl,                     IBL,                  m_pxIBL)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_SkyboxImpl,                  Skybox,               m_pxSkybox)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_GrassImpl,                   Grass,                m_pxGrass)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_TranslucencyImpl,            Translucency,         m_pxTranslucency)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_PrimitivesImpl,              Primitives,           m_pxPrimitives)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_HDRImpl,                     HDR,                  m_pxHDR)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_TAAImpl,                     TAA,                  m_pxTAA)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_TerrainImpl,                 Terrain,              m_pxTerrain)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_PresentImpl,                 Present,              m_pxPresent)
#ifdef ZENITH_TOOLS
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_GizmosImpl,                  Gizmos,               m_pxGizmos)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Flux_MaterialPreviewController,   MaterialPreview,      m_pxMaterialPreview)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_Editor,                   Editor,               m_pxEditor)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_Gizmo,                    Gizmo,                m_pxGizmo)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_SelectionSystem,          Selection,            m_pxSelection)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_UndoSystem,               UndoSystem,           m_pxUndoSystem)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_EditorAutomation,         EditorAutomation,     m_pxEditorAutomation)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_EditorMaterialUI,         EditorMaterialUI,     m_pxEditorMaterialUI)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_TerrainEditor,            TerrainEditor,        m_pxTerrainEditor)
ZENITH_ENGINE_ACCESSOR_HOTPATH(Zenith_DebugVariables,           DebugVariables,       m_pxDebugVariables)
#endif

#undef ZENITH_ENGINE_ACCESSOR
#undef ZENITH_ENGINE_ACCESSOR_HOTPATH

// Per-frame timing, scene system (brings the entity store online), input, touch.
void Zenith_Engine::AllocateCoreState()
{
	// per-frame timing state lives here now. Construct
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
	// the global entity storage (formerly the
	// engine-owned m_pxEntityStore, allocated here VERY EARLY) is now owned by
	// the Zenith_SceneSystem and allocated inside its ctor. So constructing
	// m_pxScenes here brings the entity store online too -- preserving the old
	// invariant that the store exists before Zenith_SceneSystem::InitialiseSubsystems
	// (and before Physics::Initialise / the first scene load) creates entities.
	Zenith_Assert(m_pxScenes == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxScenes = new Zenith_SceneSystem();

	// per-frame UI orchestrator (walks all scenes' Zenith_UIComponents from
	// the main loop's render-work block). The scene system is injected here
	// so the UI system's TU never names the engine singleton.
	Zenith_Assert(m_pxUISystem == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxUISystem = new Zenith_UISystem();
	m_pxUISystem->Initialise(*m_pxScenes);

	// per-frame input state (key presses, mouse delta + wheel,
	// gamepad ring buffers). Allocate EARLY so GLFW callbacks fired by
	// g_xEngine.FluxRenderer().EarlyInitialise (which spins up the window) have a live store
	// to write to.
	Zenith_Assert(m_pxInput == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxInput = new Zenith_Input();

	// touch-gesture state. Allocated alongside m_pxInput.
	Zenith_Assert(m_pxTouch == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTouch = new Zenith_TouchInput();
}

// Flux renderer/graphics holders + render backend (device, memory, swapchain).
void Zenith_Engine::AllocateRenderer()
{
	// Flux renderer state (frame counter, render graph,
	// pending command-list queue, per-frame callbacks). Must exist
	// before Flux::EarlyInitialise creates the graph.
	Zenith_Assert(m_pxFluxRenderer == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFluxRenderer = new Flux_RendererImpl();

	// Phase 2: allocate the renderer-owned scene snapshot UNCONDITIONALLY here (not in
	// Flux::EarlyInitialise, which is headless-skipped) so GetSceneSnapshot() is valid for
	// the composition-root injection below in every config — including the headless
	// unit-test boot. Held by pointer to break the snapshot-header include cycle; freed in
	// Flux_RendererImpl::Shutdown.
	m_pxFluxRenderer->m_pxSceneSnapshot = new Flux_RenderSceneSnapshot();

	// Flux_Graphics state (samplers, fallback texture /
	// material handles, scene textures, MRT formats, frame constants,
	// transient render-graph handles).
	Zenith_Assert(m_pxFluxGraphics == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFluxGraphics = new Flux_GraphicsImpl();

	// Vulkan backend state (instance / device / queues / pools /
	// per-frame ring / VRAM registry / pending command buffers / ImGui).
	// Must exist before Flux::EarlyInitialise -> Zenith_Vulkan::Initialise.
	Zenith_Assert(m_pxVulkan == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkan = new Flux_PlatformAPI();

	Zenith_Assert(m_pxVulkanMemory == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkanMemory = new Flux_MemoryManager();

	Zenith_Assert(m_pxVulkanSwapchain == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxVulkanSwapchain = new Flux_Swapchain();
}

// Every Flux subsystem Impl, smallest to biggest.
void Zenith_Engine::AllocateFluxSubsystems()
{
	// HiZ subsystem state.
	Zenith_Assert(m_pxHiZ == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxHiZ = new Flux_HiZImpl();

	// Small Flux subsystems -- mesh pipelines, deferred shading, SDFs, quads.
	// Wave-19: heap-stable owning store of per-entity Flux_AnimationControllers.
	m_pxAnimationControllers = new Flux_AnimationControllerStore();
	m_pxDeferredShading = new Flux_DeferredShadingImpl();
	m_pxSDFs            = new Flux_SDFsImpl();
	m_pxQuads           = new Flux_QuadsImpl();

	// 7 more Flux subsystems.
	m_pxShadows          = new Flux_ShadowsImpl();
	m_pxDynamicLights    = new Flux_DynamicLightsImpl();
	m_pxLightClustering  = new Flux_LightClusteringImpl();
	m_pxFroxelFog        = new Flux_FroxelFogImpl();
	m_pxGodRaysFog       = new Flux_GodRaysFogImpl();
	m_pxRaymarchFog      = new Flux_RaymarchFogImpl();
	m_pxTerrainStreaming = new Flux_TerrainStreamingManagerImpl();

	// 4 more.
	m_pxSSAO       = new Flux_SSAOImpl();
	m_pxDecals     = new Flux_DecalsImpl();
	m_pxFog        = new Flux_FogImpl();
	m_pxVolumeFog  = new Flux_VolumeFogImpl();

	// 4 more.
	m_pxParticles       = new Flux_ParticlesImpl();
	m_pxParticleGPU     = new Flux_ParticleGPUImpl();
	m_pxText            = new Flux_TextImpl();
	m_pxInstancedMeshes = new Flux_InstancedMeshesImpl();
	m_pxUnifiedMesh     = new Flux_UnifiedMeshImpl();

	// multi-pass effects.
	m_pxSSR  = new Flux_SSRImpl();
	m_pxSSGI = new Flux_SSGIImpl();
	m_pxIBL  = new Flux_IBLImpl();

	// large subsystems.
	m_pxSkybox       = new Flux_SkyboxImpl();
	m_pxGrass        = new Flux_GrassImpl();
	m_pxTranslucency = new Flux_TranslucencyImpl();
	m_pxPrimitives   = new Flux_PrimitivesImpl();

	// biggest subsystems.
	m_pxHDR     = new Flux_HDRImpl();
	m_pxTAA     = new Flux_TAAImpl();
	m_pxTerrain = new Flux_TerrainImpl();
	m_pxPresent = new Flux_PresentImpl();
#ifdef ZENITH_TOOLS
	m_pxGizmos          = new Flux_GizmosImpl();
	m_pxMaterialPreview = new Flux_MaterialPreviewController();
#endif
}

// Editor + its sub-systems + debug variables (tools builds only).
void Zenith_Engine::AllocateEditorSubsystems()
{
#ifdef ZENITH_TOOLS
	// editor state (selection, viewport, content browser,
	// console, panel visibility, camera, ImGui texture cache). Allocate
	// EARLY -- Zenith_Log fires AddLogMessage which writes to the console
	// list, and editor automation runs before the main loop.
	Zenith_Assert(m_pxEditor == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEditor = new Zenith_Editor();

	// editor sub-systems (Gizmo / Selection / Undo / Automation
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
	Zenith_Assert(m_pxTerrainEditor == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTerrainEditor = new Zenith_TerrainEditor();

	// debug-variable tree. Allocate alongside editor; many
	// subsystems register vars during their own Initialise.
	Zenith_Assert(m_pxDebugVariables == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxDebugVariables = new Zenith_DebugVariables();
#endif
}

// Thread registry, graphics options, memory tracking, profiling, task workers.
#if ZENITH_MEMORY_TRACKING_ANY
#include "Memory/Zenith_MemoryAccounting.h"
#include "Memory/Zenith_MemoryFrameSample.h"

// Unified-accounting poll callbacks (captureless free fns). Registered once at boot;
// each reads a per-subsystem accessor at poll time (once/frame from EndFrame). Keeping
// them at the engine-composition layer means leaf libs never depend on the memory
// tracker. GPU/VRAM sources are only valid once Flux is up, so they no-op in headless.
namespace
{
	void PollEngineCPU(Zenith_MemorySource& xOut)
	{
		const Zenith_MemoryFrameSample xSample = Zenith_MemoryManagement::SampleFrame();
		xOut.m_ulBytes = xSample.m_ulTotalBytes;
		xOut.m_ulAllocCount = xSample.m_ulTotalAllocations;
	}
	void PollJolt(Zenith_MemorySource& xOut)
	{
		xOut.m_ulBytes = g_xEngine.Physics().GetJoltMemoryAllocated();
		xOut.m_ulAllocCount = g_xEngine.Physics().GetJoltAllocationCount();
	}
	void PollVMA(Zenith_MemorySource& xOut)
	{
		if (Zenith_CommandLine::IsHeadless())
		{
			xOut.m_ulBytes = 0;
			xOut.m_ulAllocCount = 0;
			return;
		}
		const auto xStats = g_xEngine.FluxMemory().GetVMAStats();
		xOut.m_ulBytes = xStats.m_ulTotalAllocatedBytes;
		xOut.m_ulAllocCount = xStats.m_ulAllocationCount;
	}
}
#endif

void Zenith_Engine::InitialiseRuntimeServices()
{
	// multithreading registry (thread-ID allocator +
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

#if ZENITH_MEMORY_TRACKING_ANY
	// Register the unified-accounting sources (polled once per frame from EndFrame).
	// m_bIsVRAM keeps GPU memory out of process-RAM sums. VRAM no-ops in headless.
	Zenith_MemoryAccounting::Initialise();
	Zenith_MemoryAccounting::RegisterSource("Engine CPU", &PollEngineCPU, 0, false);
	Zenith_MemoryAccounting::RegisterSource("Jolt Physics", &PollJolt, 0, false);
	Zenith_MemoryAccounting::RegisterSource("VMA VRAM", &PollVMA, 0, true);
#endif

	// per-Engine Profiling state. Allocate BEFORE
	// g_xEngine.Threading().RegisterThread(true) below --
	// RegisterThread transitively calls Zenith_Profiling::RegisterThread
	// (which reads g_xEngine.Profiling().m_xEvents). The Profiling
	// impl also has to exist before g_xEngine.Profiling().Initialise()
	// further down for the same reason.
	Zenith_Assert(m_pxProfiling == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxProfiling = new Zenith_Profiling();

	g_xEngine.Threading().RegisterThread(true);
	g_xEngine.Profiling().Initialise(g_xEngine.Threading());

	// per-Engine TaskSystem state. Allocate BEFORE
	// g_xEngine.Tasks().Initialise() below, which spawns worker
	// threads whose ThreadFunc reads from g_xEngine.Tasks().
	Zenith_Assert(m_pxTasks == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTasks = new Zenith_TaskSystem();

	g_xEngine.Tasks().Initialise();
}

// Asset directories + registry, then tools-only asset exports.
void Zenith_Engine::InitialiseAssets()
{
	// Set asset directories before registry initialization
	// Game assets dir comes from the game project (each game defines GAME_ASSETS_DIR)
	Zenith_AssetRegistry::SetGameAssetsDir(Project_GetGameAssetsDirectory());
#ifdef ENGINE_ASSETS_DIR
	Zenith_AssetRegistry::SetEngineAssetsDir(ENGINE_ASSETS_DIR);
#else
	Zenith_AssetRegistry::SetEngineAssetsDir("./Zenith/Assets/");
#endif

	// Engine owns the AssetRegistry instance. Allocate and
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
}

// Window + device spin-up (EarlyInitialise), then physics.
void Zenith_Engine::InitialiseRendererAndPhysics()
{
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::EarlyInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxRenderer().EarlyInitialise();
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Physics::Initialise...");
	// per-Engine Physics state lives on Zenith_Physics.
	// Allocate BEFORE g_xEngine.Physics().Initialise() below -- the static
	// facade now reads/writes g_xEngine.Physics().m_pxXxx for every
	// piece of state it used to keep as Zenith_Physics::s_*.
	Zenith_Assert(m_pxPhysics == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxPhysics = new Zenith_Physics();
	g_xEngine.Physics().Initialise();
}

// Component registrar install + verification, scene bootstrap, runtime hooks.
void Zenith_Engine::InitialiseECS()
{
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

	// Behaviour Graph node set: the same registrar inversion for the scripting
	// system. Installing the function pointer here also guarantees the node-
	// library TU survives /OPT:REF; the registry drains it on first use.
	Zenith_GraphNodeRegistry::Get().SetNodeRegistrar(&Zenith_RegisterEngineGraphNodes);

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

	// Install the AI-leaf world hooks (see AI/Zenith_AIWorldHooks.h): the AI core's
	// engine-side needs — entity transform read/write, collider body, NavMeshAgent
	// resolution, and TOOLS debug-draw — are forwarded through captureless function
	// pointers so the AI leaf never names a concrete component / Flux / g_xEngine.
	// Same inversion as the component registrar; the pointers are data (not invoked
	// here), so install order only needs to precede the first AI tick.
	extern void Zenith_AI_InstallWorldHooks();
	Zenith_AI_InstallWorldHooks();

	// Install the Physics-leaf world hook (see Physics/Zenith_PhysicsWorldHooks.h):
	// a body teleport (Zenith_Physics::TeleportBody) forwards to the owning
	// Zenith_TransformComponent so the scene-graph transform cache is invalidated
	// immediately, not next frame. Same inversion as the AI hooks above; the pointer
	// is data, so install order only needs to precede the first teleport.
	extern void Zenith_Physics_InstallWorldHooks();
	Zenith_Physics_InstallWorldHooks();

	// Phase 2 (scene graph): inject the renderer-owned uncullled scene snapshot into the
	// geometry consumers. The renderer rebuilds it once per frame (Zenith_Core.cpp); the
	// consumers' Prepare callbacks then derive their draw packets from it instead of each
	// scanning the ECS. The snapshot is heap-allocated above (AllocateRenderer) and held by
	// pointer on Flux_RendererImpl, so its address is stable until Shutdown/the dtor frees it.
	// Injection (vs the consumers reaching
	// g_xEngine.FluxRenderer()) keeps those Flux TUs off the singleton ratchet.
	const Flux_RenderSceneSnapshot* pxSceneSnapshot = &g_xEngine.FluxRenderer().GetSceneSnapshot();
	g_xEngine.Translucency().SetSnapshot(pxSceneSnapshot);

#ifdef ZENITH_TOOLS
	// Phase 3: install the scene-graph debug overlays (world-AABB wireframes + cull stats),
	// drawing through the injected Flux_PrimitivesImpl and registering toggles/readouts into
	// the injected Zenith_DebugVariables — so the diagnostics TU holds no g_xEngine.
	Zenith_SceneGraphDebug::Install(g_xEngine.DebugVariables(), g_xEngine.Primitives());
#endif
}

// GPU-dependent assets + pinned textures, then Flux LateInitialise.
void Zenith_Engine::InitialiseGPUAssets()
{
	//#TO_TODO: move somewhere sensible
	if (!Zenith_CommandLine::IsHeadless())
	{
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

		g_xEngine.FluxMemory().Flush();
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::LateInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxRenderer().LateInitialise();
	}
}

// Editor init + export debug buttons (tools builds only).
void Zenith_Engine::InitialiseEditor()
{
#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_GraphicsOptions::RegisterDebugVariables();
	if (!Zenith_CommandLine::IsHeadless())
	{
		// Frame deps passed by member (not read back via g_xEngine inside the
		// editor) so the relocated RenderImGuiFrame stays off the engine-
		// singleton ratchet for Zenith_Editor.cpp.
		m_pxEditor->Initialise(*m_pxVulkan, *m_pxFluxGraphics, *m_pxFrame, *m_pxDebugVariables, *m_pxProfiling, *m_pxTerrainEditor);
		g_xEngine.DebugVariables().AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
		g_xEngine.DebugVariables().AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
		g_xEngine.DebugVariables().AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
		g_xEngine.DebugVariables().AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
	}
#endif
}

// Game callbacks: behaviours, unit tests, resources/automation or initial scene.
void Zenith_Engine::InitialiseProject()
{
	Project_RegisterGameComponents();

#ifdef ZENITH_TOOLS

	// Brush-indicator decal textures are generated artifacts — rebuilt at
	// every editor boot so the files on disk always match the generator.
	// Must run before anything resolves them via Zenith_AssetRegistry (the
	// terrain editor lazy-loads on first cursor draw, long after boot).
	Zenith_TerrainEditor::RegenerateBrushTextures();
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
	// Initialize game-specific resources (geometry, materials, prefabs, particle configs).
	// GPU allocations record into the memory command buffer lazily; Flush drains
	// them synchronously before automation begins.
	Project_InitializeResources();
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxMemory().Flush();
	}

	// Register automation steps and begin execution (one step per frame in main loop)
	Project_RegisterEditorAutomationSteps();
	g_xEngine.EditorAutomation().Begin();
#else
	// Non-tools: load pre-generated scene files
	// Run a tools build first to generate .zscen files
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
		g_xEngine.FluxMemory().Flush();
	}
	Zenith_Assert(g_xEngine.Scenes().GetActiveScene().IsValid(),
		"No scene loaded. Run a ZENITH_TOOLS build first to generate .zscen files.");
#endif
}

void Zenith_Engine::Initialise()
{
	AllocateCoreState();
	AllocateRenderer();
	AllocateFluxSubsystems();
	AllocateEditorSubsystems();
	InitialiseRuntimeServices();
	InitialiseAssets();
	InitialiseRendererAndPhysics();
	InitialiseECS();
	InitialiseGPUAssets();
	InitialiseEditor();
	InitialiseProject();

	m_pxFrame->SetLastFrameTime(std::chrono::high_resolution_clock::now());
}

// GPU idle wait, then editor, scenes, physics, project teardown.
void Zenith_Engine::ShutdownGameSystems()
{
	// Wait for GPU to finish all pending work
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxBackend().WaitForGPUIdle();
	}

#ifdef ZENITH_TOOLS
	// Shutdown editor (processes pending deletions, cleans up editor state)
	g_xEngine.Editor().Shutdown();
#endif

	// Shutdown scene system (unloads all scenes, releases resources)
	// Must happen before physics (colliders need to remove bodies) and before
	// memory manager (model/mesh components hold VRAM handles)
	Zenith_SceneSystem::ShutdownSubsystems();

	// Shutdown physics system. The static facade drains state out of
	// g_xEngine.Physics().m_pxXxx; engine then reclaims the Impl below
	// (Phase 4 makes Zenith_Engine the sole owner).
	g_xEngine.Physics().Shutdown();
	delete m_pxPhysics;
	m_pxPhysics = nullptr;

	// Project shutdown - clean up game-specific resources
	Project_Shutdown();
}

// Release Flux asset refs, shut down the registry, then the renderer.
void Zenith_Engine::ShutdownAssetsAndRenderer()
{
	// Release Flux's asset-system references BEFORE the registry shuts down.
	// Flux statics hold TextureHandle / MaterialHandle defaults that must drop their
	// refs while the registry still owns its assets — g_xEngine.FluxRenderer().Shutdown() runs too late.
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxRenderer().ReleaseAssetReferences();
	}

	// Shutdown asset registry (unloads all assets). Engine then
	// reclaims the instance — Phase 4 makes Zenith_Engine the sole
	// owner; the static facade now drains state only.
	Zenith_AssetRegistry::Shutdown();
	delete m_pxAssets;
	m_pxAssets = nullptr;
	Zenith_AssetRegistry::s_pxInstance = nullptr;

	// Shutdown Flux (all subsystems + graphics + memory manager)
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxRenderer().Shutdown();
	}
}

// Task workers, profiling, frame timing, thread registry.
void Zenith_Engine::ShutdownRuntimeServices()
{
	// Shutdown task system (terminates worker threads)
	g_xEngine.Tasks().Shutdown();

	// Free the per-Engine TaskSystem state. Must come AFTER
	// g_xEngine.Tasks().Shutdown above -- the forwarder there reads
	// from g_xEngine.Tasks() to drive Shutdown.
	delete m_pxTasks;
	m_pxTasks = nullptr;

	// Free the per-Engine Profiling state. Comes AFTER TaskSystem
	// shutdown (step 9) -- worker threads may have profile scopes
	// pending until they exit, and AFTER tearing down m_pxTasks so no
	// stale ThreadFunc can call Profiling. Shutdown() frees the per-thread
	// rings + snapshots: the workers have joined + unregistered, so it frees
	// the main thread's ring and leaves any still-live producer (FileWatcher)
	// allocated to avoid a use-after-free.
	m_pxProfiling->Shutdown();
	delete m_pxProfiling;
	m_pxProfiling = nullptr;

	// Tear down per-frame timing state. Done late so any
	// subsystem shutdown that needs to log dt or read accumulated
	// time still can.
	delete m_pxFrame;
	m_pxFrame = nullptr;

	// Tear down the multithreading registry. Done after the task
	// system shutdown (step 9) so worker threads aren't still calling
	// IsMainThread while we're freeing the registry.
	delete m_pxThreading;
	m_pxThreading = nullptr;
}

// Scene system (frees the entity store), input, touch.
void Zenith_Engine::DeleteSceneAndInputState()
{
	// Free the UI orchestrator first -- it holds a pointer into the scene
	// system freed just below.
	delete m_pxUISystem;
	m_pxUISystem = nullptr;

	// Free the scene system. ShutdownSubsystems above already drained
	// the slot table, cleared callback lists, terminated the animation task,
	// etc.; this just reclaims the holder + its internal members.
	//
	// the per-process entity store is now OWNED
	// by the Zenith_SceneSystem -- its dtor frees the store. There is no longer a
	// separate `delete m_pxEntityStore`. This delete therefore frees the store
	// too, and it stays VERY LATE (after SceneManager::ShutdownSubsystems step 3,
	// Physics::Shutdown step 4, Project_Shutdown step 5, FluxRenderer::Shutdown
	// step 8 -- all of which may touch entity slots during teardown), preserving
	// the old store-teardown ordering since the store's lifetime == the
	// SceneSystem's.
	delete m_pxScenes;
	m_pxScenes = nullptr;

	// Free per-frame input state. Done LATE -- some Flux/window
	// teardown paths can fire one last GLFW callback.
	delete m_pxInput;
	m_pxInput = nullptr;

	// Free touch-gesture state.
	delete m_pxTouch;
	m_pxTouch = nullptr;
}

// Flux subsystem holders, then renderer/graphics, then the backend last.
void Zenith_Engine::DeleteRendererState()
{
	// Free Flux subsystem holders before Vulkan holders. Flux::Shutdown above
	// has drained subsystem-owned state, but these deletes run member dtors;
	// many members are Flux_Shader / Flux_Pipeline wrappers that need the
	// Vulkan device to destroy any remaining native handles.
	delete m_pxHiZ;
	m_pxHiZ = nullptr;
	// Wave-19: free the animation-controller store here (before the Vulkan
	// device teardown below). By this point scene teardown has destroyed all
	// entities, so each component's OnDestroy/dtor has already Destroy()'d its
	// controller; this delete reclaims the (now-empty) store.
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
	delete m_pxUnifiedMesh;     m_pxUnifiedMesh = nullptr;
	delete m_pxSSR;  m_pxSSR  = nullptr;
	delete m_pxSSGI; m_pxSSGI = nullptr;
	delete m_pxIBL;  m_pxIBL  = nullptr;
	delete m_pxSkybox;     m_pxSkybox     = nullptr;
	delete m_pxGrass;      m_pxGrass      = nullptr;
	delete m_pxTranslucency; m_pxTranslucency = nullptr;
	delete m_pxPrimitives; m_pxPrimitives = nullptr;
	delete m_pxHDR;     m_pxHDR     = nullptr;
	delete m_pxTAA;     m_pxTAA     = nullptr;
	delete m_pxTerrain; m_pxTerrain = nullptr;
	delete m_pxPresent; m_pxPresent = nullptr;
#ifdef ZENITH_TOOLS
	delete m_pxGizmos;          m_pxGizmos = nullptr;
	delete m_pxMaterialPreview; m_pxMaterialPreview = nullptr;
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

	// Free the render backend state last among graphics holders so device-backed
	// member destructors above can still reach the live backend (FluxBackend()).
	delete m_pxVulkanSwapchain;
	m_pxVulkanSwapchain = nullptr;
	delete m_pxVulkanMemory;
	m_pxVulkanMemory = nullptr;
	delete m_pxVulkan;
	m_pxVulkan = nullptr;
}

// Editor holders (tools builds only).
void Zenith_Engine::DeleteEditorState()
{
#ifdef ZENITH_TOOLS
	// Free editor state. Done LATE -- the editor's deferred-deletion
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
	delete m_pxTerrainEditor;
	m_pxTerrainEditor = nullptr;
	delete m_pxDebugVariables;
	m_pxDebugVariables = nullptr;
#endif
}

void Zenith_Engine::Shutdown()
{
	Zenith_Log(LOG_CATEGORY_CORE, "Beginning shutdown sequence...");

	ShutdownGameSystems();
	ShutdownAssetsAndRenderer();
	ShutdownRuntimeServices();
	DeleteSceneAndInputState();
	DeleteRendererState();
	DeleteEditorState();

	// LAST: read-only memory leak checkpoint, after every engine-owned tracked object
	// has been freed above. Deliberately the final step — it does NOT disable tracking
	// or tear down the allocator, so base recovery stays valid for any frees still
	// happening during static destruction. See Zenith_MemoryManagement::Shutdown().
	Zenith_MemoryManagement::Shutdown();

	Zenith_Log(LOG_CATEGORY_CORE, "Shutdown complete");
}
