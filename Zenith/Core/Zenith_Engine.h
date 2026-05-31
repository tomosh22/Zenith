#pragma once

#include <type_traits>

// Forward decls -- keep this header light. Full subsystem headers are
// only included by Zenith_Engine.cpp where the accessor bodies live.
class FrameContext;
class Zenith_AssetRegistry;
class Zenith_EntityStore;
class Flux_GraphicsImpl;
class Flux_AnimatedMeshesImpl;
class Flux_AnimationControllerStore;
class Flux_DecalsImpl;
class Flux_DeferredShadingImpl;
class Flux_DynamicLightsImpl;
class Flux_FogImpl;
class Flux_FroxelFogImpl;
class Flux_GodRaysFogImpl;
class Flux_HiZImpl;
class Flux_LightClusteringImpl;
class Flux_QuadsImpl;
class Flux_RaymarchFogImpl;
class Flux_RendererImpl;
class Flux_SDFsImpl;
class Flux_SSAOImpl;
class Flux_ShadowsImpl;
class Flux_StaticMeshesImpl;
class Flux_TerrainStreamingManagerImpl;
class Flux_VolumeFogImpl;
class Flux_GizmosImpl;
class Flux_InstancedMeshesImpl;
class Flux_ParticlesImpl;
class Flux_ParticleGPUImpl;
class Flux_TextImpl;
class Flux_SSRImpl;
class Flux_SSGIImpl;
class Flux_IBLImpl;
class Flux_SkyboxImpl;
class Flux_GrassImpl;
class Flux_PrimitivesImpl;
class Flux_HDRImpl;
class Flux_TerrainImpl;
class Zenith_Vulkan;
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan_Swapchain;
class Zenith_DebugVariables;
class Zenith_EditorAutomation;
class Zenith_Editor;
class Zenith_EditorMaterialUI;
class Zenith_Gizmo;
class Zenith_Input;
class Zenith_SelectionSystem;
class Zenith_TouchInput;
class Zenith_UndoSystem;
class Zenith_Multithreading;
class Zenith_Physics;
class Zenith_Profiling;
class Zenith_SceneSystem;
class Zenith_TaskSystem;

// Zenith_Engine is the single owner of the engine's mutable runtime
// state. Phase 0 introduces the class and moves the bootstrap ordering
// from Zenith_Core::Zenith_Init / Zenith_Shutdown into
// Zenith_Engine::Initialise / Shutdown — subsystem APIs stay static
// for now; later phases migrate them onto Zenith_Engine as members.
//
// Construction model:
// - g_xEngine is constinit (zero static-init cost; nullptr-default
//   members are constant expressions).
// - Default ctor/dtor are trivial. All subsystem ownership / teardown
//   work runs explicitly in Initialise() / Shutdown(). NEVER add a
//   non-trivial destructor — process shutdown must not run static
//   destructors in undefined order vs. other globals.
// - Refactor plan + rationale:
//   ~/.claude/plans/the-zenith-engine-has-playful-canyon.md
class Zenith_Engine
{
public:
	Zenith_Engine() = default;
	~Zenith_Engine() = default;

	Zenith_Engine(const Zenith_Engine&) = delete;
	Zenith_Engine& operator=(const Zenith_Engine&) = delete;
	Zenith_Engine(Zenith_Engine&&) = delete;
	Zenith_Engine& operator=(Zenith_Engine&&) = delete;

	// Bootstrap. Replaces Zenith_Init / Zenith_Shutdown bodies; the
	// free Zenith_Core::Zenith_Init / Zenith_Shutdown are now thin
	// wrappers that forward here.
	void Initialise();
	void Shutdown();

	// Subsystem accessors. Bodies live in Zenith_Engine.cpp where the
	// full subsystem headers are visible; this header only forward-
	// declares the return types.
	//
	// Accessor null-assert policy (see per-accessor docs in
	// Zenith_Engine.cpp for specifics):
	//   - Some steady-state hot-path accessors omit the null assert when
	//     Initialise() guarantees allocation before the main loop.
	//   - Others keep the null assert; the choice is case-by-case per
	//     accessor, not a blanket rule.
	//   - Calling any accessor before its Initialise phase is undefined;
	//     see Initialise() for the ordering.
	FrameContext& Frame();
	Zenith_Multithreading& Threading();
	Zenith_TaskSystem& Tasks();
	Zenith_Profiling& Profiling();
	Zenith_AssetRegistry& Assets();
	Zenith_Physics& Physics();
	Zenith_EntityStore& EntityStore();
	Zenith_SceneSystem& Scenes();
	Zenith_Input& Input();
	Zenith_TouchInput& Touch();
	Flux_RendererImpl& FluxRenderer();
	Flux_GraphicsImpl& FluxGraphics();
	Zenith_Vulkan& Vulkan();
	Zenith_Vulkan_MemoryManager& VulkanMemory();
	Zenith_Vulkan_Swapchain& VulkanSwapchain();
	Flux_HiZImpl& HiZ();
	Flux_StaticMeshesImpl& StaticMeshes();
	Flux_AnimatedMeshesImpl& AnimatedMeshes();
	// Heap-stable owning store of one Flux_AnimationController per entity
	// (Wave-19 ownership relocation). Zenith_AnimatorComponent is a thin
	// forwarding handle into this store, keyed by EntityID slot.
	Flux_AnimationControllerStore& AnimationControllers();
	Flux_DeferredShadingImpl& DeferredShading();
	Flux_SDFsImpl& SDFs();
	Flux_QuadsImpl& Quads();
	Flux_ShadowsImpl& Shadows();
	Flux_DynamicLightsImpl& DynamicLights();
	Flux_LightClusteringImpl& LightClustering();
	Flux_FroxelFogImpl& FroxelFog();
	Flux_GodRaysFogImpl& GodRaysFog();
	Flux_RaymarchFogImpl& RaymarchFog();
	Flux_TerrainStreamingManagerImpl& TerrainStreaming();
	Flux_SSAOImpl& SSAO();
	Flux_DecalsImpl& Decals();
	Flux_FogImpl& Fog();
	Flux_VolumeFogImpl& VolumeFog();
	Flux_ParticlesImpl& Particles();
	Flux_ParticleGPUImpl& ParticleGPU();
	Flux_TextImpl& Text();
	// Like Text(), but returns nullptr if the subsystem isn't yet constructed
	// (engine init, headless tests, shutdown). Safe to call any time. Used
	// by Zenith_FontAsset::GetActiveOrDefaultMetrics for UI metric fallback.
	Flux_TextImpl* TryGetText() const { return m_pxText; }
	Flux_InstancedMeshesImpl& InstancedMeshes();
	Flux_SSRImpl& SSR();
	Flux_SSGIImpl& SSGI();
	Flux_IBLImpl& IBL();
	Flux_SkyboxImpl& Skybox();
	Flux_GrassImpl& Grass();
	Flux_PrimitivesImpl& Primitives();
	Flux_HDRImpl& HDR();
	Flux_TerrainImpl& Terrain();
#ifdef ZENITH_TOOLS
	Flux_GizmosImpl& Gizmos();
#endif
#ifdef ZENITH_TOOLS
	Zenith_Editor& Editor();
	// True when Initialise() has run far enough to have allocated the
	// editor Impl. Lets call sites that fire during static init (e.g.
	// component-registration logs) gracefully skip writing to a not-yet-
	// allocated console buffer.
	bool HasEditor() const { return m_pxEditor != nullptr; }
	Zenith_Gizmo& Gizmo();
	Zenith_SelectionSystem& Selection();
	Zenith_UndoSystem& UndoSystem();
	Zenith_EditorAutomation& EditorAutomation();
	Zenith_EditorMaterialUI& EditorMaterialUI();
	Zenith_DebugVariables& DebugVariables();
#endif

private:
	// Subsystem members. The scene system is a single Zenith_SceneSystem
	// instance exposed via Scenes() — there are no per-subsystem accessors
	// (Registry/Operations/Lifecycle/Callbacks were all merged into it).
	// Raw pointers to forward-declared types so the default ctor/dtor stay
	// trivial and the constinit global has zero static-init cost. Each is
	// allocated in Initialise() and deleted in Shutdown().
	FrameContext*              m_pxFrame       = nullptr;
	Zenith_Multithreading* m_pxThreading   = nullptr;
	Zenith_TaskSystem*     m_pxTasks       = nullptr;
	Zenith_Profiling*      m_pxProfiling   = nullptr;
	Zenith_AssetRegistry*        m_pxAssets         = nullptr;
	Zenith_Physics*          m_pxPhysics        = nullptr;
	Zenith_EntityStore*          m_pxEntityStore    = nullptr;
	Zenith_SceneSystem*             m_pxScenes          = nullptr;
	Zenith_Input*                   m_pxInput           = nullptr;
	Zenith_TouchInput*              m_pxTouch           = nullptr;
	Flux_RendererImpl*                  m_pxFluxRenderer    = nullptr;
	Flux_GraphicsImpl*                  m_pxFluxGraphics    = nullptr;
	Zenith_Vulkan*                  m_pxVulkan          = nullptr;
	Zenith_Vulkan_MemoryManager*    m_pxVulkanMemory    = nullptr;
	Zenith_Vulkan_Swapchain*        m_pxVulkanSwapchain = nullptr;
	Flux_HiZImpl*                       m_pxHiZ              = nullptr;
	Flux_StaticMeshesImpl*              m_pxStaticMeshes     = nullptr;
	Flux_AnimatedMeshesImpl*            m_pxAnimatedMeshes   = nullptr;
	Flux_AnimationControllerStore*      m_pxAnimationControllers = nullptr;
	Flux_DeferredShadingImpl*           m_pxDeferredShading  = nullptr;
	Flux_SDFsImpl*                      m_pxSDFs             = nullptr;
	Flux_QuadsImpl*                     m_pxQuads            = nullptr;
	Flux_ShadowsImpl*                   m_pxShadows          = nullptr;
	Flux_DynamicLightsImpl*             m_pxDynamicLights    = nullptr;
	Flux_LightClusteringImpl*           m_pxLightClustering  = nullptr;
	Flux_FroxelFogImpl*                 m_pxFroxelFog        = nullptr;
	Flux_GodRaysFogImpl*                m_pxGodRaysFog       = nullptr;
	Flux_RaymarchFogImpl*               m_pxRaymarchFog      = nullptr;
	Flux_TerrainStreamingManagerImpl*   m_pxTerrainStreaming = nullptr;
	Flux_SSAOImpl*                      m_pxSSAO             = nullptr;
	Flux_DecalsImpl*                    m_pxDecals           = nullptr;
	Flux_FogImpl*                       m_pxFog              = nullptr;
	Flux_VolumeFogImpl*                 m_pxVolumeFog        = nullptr;
	Flux_ParticlesImpl*                 m_pxParticles        = nullptr;
	Flux_ParticleGPUImpl*               m_pxParticleGPU      = nullptr;
	Flux_TextImpl*                      m_pxText             = nullptr;
	Flux_InstancedMeshesImpl*           m_pxInstancedMeshes  = nullptr;
	Flux_SSRImpl*                       m_pxSSR              = nullptr;
	Flux_SSGIImpl*                      m_pxSSGI             = nullptr;
	Flux_IBLImpl*                       m_pxIBL              = nullptr;
	Flux_SkyboxImpl*                    m_pxSkybox           = nullptr;
	Flux_GrassImpl*                     m_pxGrass            = nullptr;
	Flux_PrimitivesImpl*                m_pxPrimitives       = nullptr;
	Flux_HDRImpl*                       m_pxHDR              = nullptr;
	Flux_TerrainImpl*                   m_pxTerrain          = nullptr;
#ifdef ZENITH_TOOLS
	Flux_GizmosImpl*                    m_pxGizmos           = nullptr;
#endif
#ifdef ZENITH_TOOLS
	Zenith_Editor*                  m_pxEditor             = nullptr;
	Zenith_Gizmo*                   m_pxGizmo              = nullptr;
	Zenith_SelectionSystem*         m_pxSelection          = nullptr;
	Zenith_UndoSystem*              m_pxUndoSystem         = nullptr;
	Zenith_EditorAutomation*        m_pxEditorAutomation   = nullptr;
	Zenith_EditorMaterialUI*        m_pxEditorMaterialUI   = nullptr;
	Zenith_DebugVariables*          m_pxDebugVariables     = nullptr;
#endif
};

// Compile-time guard: enforce trivial destruction so the
// process-shutdown destructor-order fiasco can't reappear if a
// well-meaning member with a non-trivial dtor is added later.
static_assert(std::is_trivially_destructible_v<Zenith_Engine>,
	"Zenith_Engine must be trivially destructible. Subsystem cleanup "
	"belongs in Zenith_Engine::Shutdown(), not in member dtors that "
	"run at static-destruction time.");

extern Zenith_Engine g_xEngine;
