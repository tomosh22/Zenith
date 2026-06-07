#include "Zenith.h"

#include "Flux/Flux_FeatureRegistry.h"

#include "Core/Zenith_Engine.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Subsystem Impl headers — the trampolines below construct calls through the
// g_xEngine.X() accessors, which return the concrete *Impl references. This .cpp
// is a Flux-layer TU, so including these does NOT touch the Flux<->ECS layering
// gate (which only flags Flux -> EntityComponent includes; there are none here).
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/Decals/Flux_DecalsImpl.h"
#include "Flux/SSAO/Flux_SSAOImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/SDFs/Flux_SDFsImpl.h"
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Flux/Text/Flux_TextImpl.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#endif

#include "Vulkan/Zenith_Vulkan_Swapchain.h"

#include <cstring> // strcmp — golden-order verification only; deliberately not in the header.

// ---------------------------------------------------------------------------
// Feature name constants. Register() and the golden arrays both reference these
// SAME literals, so VerifyOrder is comparing transcribed order, not retyped
// strings (a typo in one place can't silently agree with a typo in the other).
// ---------------------------------------------------------------------------
namespace
{
	constexpr const char* szFLUX_FEATURE_GRAPHICS         = "FluxGraphics";
	constexpr const char* szFLUX_FEATURE_HDR              = "HDR";
#ifdef ZENITH_TOOLS
	constexpr const char* szFLUX_FEATURE_GIZMOS           = "Gizmos";
#endif
	constexpr const char* szFLUX_FEATURE_SHADOWS          = "Shadows";
	constexpr const char* szFLUX_FEATURE_SKYBOX           = "Skybox";
	constexpr const char* szFLUX_FEATURE_IBL              = "IBL";
	constexpr const char* szFLUX_FEATURE_STATIC_MESHES    = "StaticMeshes";
	constexpr const char* szFLUX_FEATURE_ANIMATED_MESHES  = "AnimatedMeshes";
	constexpr const char* szFLUX_FEATURE_INSTANCED_MESHES = "InstancedMeshes";
	constexpr const char* szFLUX_FEATURE_TERRAIN          = "Terrain";
	constexpr const char* szFLUX_FEATURE_GRASS            = "Grass";
	constexpr const char* szFLUX_FEATURE_PRIMITIVES       = "Primitives";
	constexpr const char* szFLUX_FEATURE_HIZ              = "HiZ";
	constexpr const char* szFLUX_FEATURE_SSR              = "SSR";
	constexpr const char* szFLUX_FEATURE_SSGI             = "SSGI";
	constexpr const char* szFLUX_FEATURE_DYNAMIC_LIGHTS   = "DynamicLights";
	constexpr const char* szFLUX_FEATURE_LIGHT_CLUSTERING = "LightClustering";
	constexpr const char* szFLUX_FEATURE_DEFERRED_SHADING = "DeferredShading";
	constexpr const char* szFLUX_FEATURE_DECALS           = "Decals";
	constexpr const char* szFLUX_FEATURE_SSAO             = "SSAO";
	constexpr const char* szFLUX_FEATURE_FOG              = "Fog";
	constexpr const char* szFLUX_FEATURE_SDFS             = "SDFs";
	constexpr const char* szFLUX_FEATURE_PARTICLES        = "Particles";
	constexpr const char* szFLUX_FEATURE_QUADS            = "Quads";
	constexpr const char* szFLUX_FEATURE_TEXT             = "Text";
}

// ---------------------------------------------------------------------------
// Registry plumbing.
// ---------------------------------------------------------------------------
Flux_FeatureRegistry& Flux_FeatureRegistry::Get()
{
	static Flux_FeatureRegistry s_xRegistry;
	return s_xRegistry;
}

void Flux_FeatureRegistry::Reset()
{
	// Mutating registry state — main-thread only. RegisterDefaultFeatures runs
	// during LateInitialise (main thread) and from the unit-test re-init path
	// (also main thread).
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"Flux_FeatureRegistry::Reset: must run on the main thread");

	for (u_int u = 0; u < m_uNumFeatures; u++)
	{
		m_axFeatures[u] = Flux_FeatureDesc();
	}
	m_uNumFeatures = 0;
	m_uNumSetup = 0;
	m_uNumShutdown = 0;
}

u_int Flux_FeatureRegistry::Register(const char* szName,
	void (*pfnInitialise)(),
	void (*pfnSetupRenderGraph)(Flux_RenderGraph&),
	void (*pfnShutdown)())
{
	Zenith_Assert(szName != nullptr, "Flux_FeatureRegistry::Register: null feature name");
	Zenith_Assert(m_uNumFeatures < FLUX_MAX_FEATURES,
		"Flux_FeatureRegistry::Register: FLUX_MAX_FEATURES (%u) exceeded registering '%s' — raise the cap in Flux_FeatureRegistry.h",
		FLUX_MAX_FEATURES, szName);

#if defined(ZENITH_ASSERT) || defined(ZENITH_DEBUG)
	for (u_int u = 0; u < m_uNumFeatures; u++)
	{
		Zenith_Assert(strcmp(m_axFeatures[u].m_szName, szName) != 0,
			"Flux_FeatureRegistry::Register: duplicate feature name '%s'", szName);
	}
#endif

	const u_int uIndex = m_uNumFeatures;
	Flux_FeatureDesc& xDesc = m_axFeatures[uIndex];
	xDesc.m_szName = szName;
	xDesc.m_pfnInitialise = pfnInitialise;
	xDesc.m_pfnSetupRenderGraph = pfnSetupRenderGraph;
	xDesc.m_pfnShutdown = pfnShutdown;
	m_uNumFeatures++;
	return uIndex;
}

void Flux_FeatureRegistry::AddToSetupWalk(u_int uFeatureIndex, Flux_FeatureSetupPhase ePhase)
{
	Zenith_Assert(uFeatureIndex < m_uNumFeatures,
		"Flux_FeatureRegistry::AddToSetupWalk: feature index %u out of range (%u registered)", uFeatureIndex, m_uNumFeatures);
	Zenith_Assert(m_uNumSetup < FLUX_MAX_FEATURES, "Flux_FeatureRegistry::AddToSetupWalk: setup-order overflow");
	m_auSetupOrder[m_uNumSetup] = uFeatureIndex;
	m_aeSetupPhase[m_uNumSetup] = ePhase;
	m_uNumSetup++;
}

void Flux_FeatureRegistry::AddToShutdownWalk(u_int uFeatureIndex)
{
	Zenith_Assert(uFeatureIndex < m_uNumFeatures,
		"Flux_FeatureRegistry::AddToShutdownWalk: feature index %u out of range (%u registered)", uFeatureIndex, m_uNumFeatures);
	Zenith_Assert(m_uNumShutdown < FLUX_MAX_FEATURES, "Flux_FeatureRegistry::AddToShutdownWalk: shutdown-order overflow");
	m_auShutdownOrder[m_uNumShutdown] = uFeatureIndex;
	m_uNumShutdown++;
}

void Flux_FeatureRegistry::RunSetupPhase(Flux_RenderGraph& xGraph, Flux_FeatureSetupPhase ePhase) const
{
	for (u_int u = 0; u < m_uNumSetup; u++)
	{
		if (m_aeSetupPhase[u] != ePhase)
			continue;
		const Flux_FeatureDesc& xDesc = m_axFeatures[m_auSetupOrder[u]];
		if (xDesc.m_pfnSetupRenderGraph != nullptr)
			xDesc.m_pfnSetupRenderGraph(xGraph);
	}
}

void Flux_FeatureRegistry::RunShutdown() const
{
	for (u_int u = 0; u < m_uNumShutdown; u++)
	{
		const Flux_FeatureDesc& xDesc = m_axFeatures[m_auShutdownOrder[u]];
		if (xDesc.m_pfnShutdown != nullptr)
			xDesc.m_pfnShutdown();
	}
}

// ---------------------------------------------------------------------------
// Default feature set. RegisterDefaultFeatures emits ONE Register(...) per
// feature in INIT order (matching the pre-refactor LateInitialise block), then
// records the setup-order sub-walk membership (matching SetupRenderGraph) and
// the shutdown order (matching the reverse-order Shutdown block).
//
// Trampolines are captureless +[]{} free functions routed through g_xEngine.
// HiZ and SSAO take DI-injected dependency params, so their init trampolines
// gather those deps from g_xEngine (mirrors the explicit-injection call sites
// the inline LateInitialise used).
// ---------------------------------------------------------------------------
void Flux_FeatureRegistry::RegisterDefaultFeatures()
{
	Flux_FeatureRegistry& xReg = Flux_FeatureRegistry::Get();

	// Idempotency: headless boots skip LateInitialise and unit tests re-init
	// Flux in-process, so this may be called more than once per process. Reset
	// to a known-empty table on every entry so re-registration is exact rather
	// than doubling the lists.
	xReg.Reset();

	// ===== INIT ORDER ======================================================
	// Matches Flux_RendererImpl::LateInitialise. The prologue (PerFrame /
	// Vulkan / Swapchain / Slang / HotReload / ImGui) stays INLINE there; the
	// init walk begins at FluxGraphics. Trampolines also carry each feature's
	// setup + shutdown function pointers (null where the feature lacks that
	// phase). Fog has no Shutdown() (RAII / stateless) -> null shutdown.
	const u_int uGraphics = xReg.Register(szFLUX_FEATURE_GRAPHICS,
		nullptr, // FluxGraphics is brought up by its own Initialise() inline (core graphics, not a registry-init feature); its setup is the SetupTransients irregular below.
		nullptr,
		+[](){ g_xEngine.FluxGraphics().Shutdown(); });
	const u_int uHDR = xReg.Register(szFLUX_FEATURE_HDR,
		+[](){ g_xEngine.HDR().Initialise(g_xEngine.FluxGraphics(), g_xEngine.VulkanMemory(), g_xEngine.VulkanSwapchain(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.HDR().SetupRenderGraph(g); }, // HDR's SECOND setup touch (bloom/tonemap); its FIRST touch (SetupTransients) is an inline irregular.
		+[](){ g_xEngine.HDR().Shutdown(); });
#ifdef ZENITH_TOOLS
	const u_int uGizmos = xReg.Register(szFLUX_FEATURE_GIZMOS,
		+[](){ g_xEngine.Gizmos().Initialise(g_xEngine.FluxGraphics(), g_xEngine.Primitives(), g_xEngine.VulkanMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Gizmos().SetupRenderGraph(g); },
		+[](){ g_xEngine.Gizmos().Shutdown(); });
#endif
	const u_int uShadows = xReg.Register(szFLUX_FEATURE_SHADOWS,
		+[](){ g_xEngine.Shadows().Initialise(g_xEngine.VulkanMemory(), g_xEngine.FluxGraphics(), g_xEngine.Profiling()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Shadows().SetupRenderGraph(g); },
		+[](){ g_xEngine.Shadows().Shutdown(); });
	const u_int uSkybox = xReg.Register(szFLUX_FEATURE_SKYBOX,
		+[](){ g_xEngine.Skybox().Initialise(g_xEngine.FluxGraphics(), g_xEngine.HDR(), g_xEngine.VulkanMemory(), g_xEngine.Vulkan()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Skybox().SetupRenderGraph(g); }, // Skybox's aerial-perspective pass is a separate method invoked inline as an irregular.
		+[](){ g_xEngine.Skybox().Shutdown(); });
	const u_int uIBL = xReg.Register(szFLUX_FEATURE_IBL,
		+[](){ g_xEngine.IBL().Initialise(); },
		+[](Flux_RenderGraph& g){ g_xEngine.IBL().SetupRenderGraph(g); },
		+[](){ g_xEngine.IBL().Shutdown(); });
	const u_int uStaticMeshes = xReg.Register(szFLUX_FEATURE_STATIC_MESHES,
		// DI seam: StaticMeshes::Initialise takes (Graphics&). FluxGraphics is brought
		// up inline before this walk, so the dep is ready. The ECS reach inside
		// GatherDrawPacket (g_xEngine.Scenes()) stays self-routed by design.
		+[](){ g_xEngine.StaticMeshes().Initialise(g_xEngine.FluxGraphics()); },
		+[](Flux_RenderGraph& g){ g_xEngine.StaticMeshes().SetupRenderGraph(g); },
		+[](){ g_xEngine.StaticMeshes().Shutdown(); });
	const u_int uAnimatedMeshes = xReg.Register(szFLUX_FEATURE_ANIMATED_MESHES,
		// DI seam: AnimatedMeshes::Initialise takes (Graphics&). FluxGraphics is
		// brought up inline at the top of LateInitialise before this walk, so the
		// dep is ready.
		+[](){ g_xEngine.AnimatedMeshes().Initialise(g_xEngine.FluxGraphics()); },
		+[](Flux_RenderGraph& g){ g_xEngine.AnimatedMeshes().SetupRenderGraph(g); },
		+[](){ g_xEngine.AnimatedMeshes().Shutdown(); });
	const u_int uInstancedMeshes = xReg.Register(szFLUX_FEATURE_INSTANCED_MESHES,
		+[](){ g_xEngine.InstancedMeshes().Initialise(g_xEngine.VulkanMemory(), g_xEngine.FluxGraphics()); },
		+[](Flux_RenderGraph& g){ g_xEngine.InstancedMeshes().SetupRenderGraph(g); },
		+[](){ g_xEngine.InstancedMeshes().Shutdown(); });
	const u_int uTerrain = xReg.Register(szFLUX_FEATURE_TERRAIN,
		+[](){ g_xEngine.Terrain().Initialise(g_xEngine.VulkanMemory(), g_xEngine.FluxGraphics(), g_xEngine.Profiling(), g_xEngine.TerrainStreaming()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Terrain().SetupRenderGraph(g); },
		+[](){ g_xEngine.Terrain().Shutdown(); });
	const u_int uGrass = xReg.Register(szFLUX_FEATURE_GRASS,
		+[](){ g_xEngine.Grass().Initialise(g_xEngine.VulkanMemory(), g_xEngine.Frame(), g_xEngine.FluxGraphics(), g_xEngine.HDR()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Grass().SetupRenderGraph(g); },
		+[](){ g_xEngine.Grass().Shutdown(); });
	const u_int uPrimitives = xReg.Register(szFLUX_FEATURE_PRIMITIVES,
		// DI seam: Primitives::Initialise takes (Graphics&). FluxGraphics is brought
		// up before the walk reaches Primitives, so the trampoline forwards it.
		+[](){ g_xEngine.Primitives().Initialise(g_xEngine.FluxGraphics(), g_xEngine.VulkanMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Primitives().SetupRenderGraph(g); },
		+[](){ g_xEngine.Primitives().Shutdown(); });
	const u_int uHiZ = xReg.Register(szFLUX_FEATURE_HIZ,
		// DI seam: HiZ::Initialise takes (Swapchain&, Graphics&, Renderer&) — the
		// trampoline gathers them from g_xEngine, mirroring the inline call site.
		+[](){ g_xEngine.HiZ().Initialise(g_xEngine.VulkanSwapchain(), g_xEngine.FluxGraphics(), g_xEngine.FluxRenderer()); },
		+[](Flux_RenderGraph& g){ g_xEngine.HiZ().SetupRenderGraph(g); },
		+[](){ g_xEngine.HiZ().Shutdown(); });
	const u_int uSSR = xReg.Register(szFLUX_FEATURE_SSR,
		+[](){ g_xEngine.SSR().Initialise(g_xEngine.VulkanMemory(), g_xEngine.VulkanSwapchain(), g_xEngine.FluxGraphics(), g_xEngine.HiZ(), g_xEngine.VolumeFog(), g_xEngine.FluxRenderer()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SSR().SetupRenderGraph(g); },
		+[](){ g_xEngine.SSR().Shutdown(); }); // Shutdown inherited from Flux_ScreenSpaceEffectBase CRTP base.
	const u_int uSSGI = xReg.Register(szFLUX_FEATURE_SSGI,
		+[](){ g_xEngine.SSGI().Initialise(g_xEngine.VulkanSwapchain(), g_xEngine.HiZ(), g_xEngine.FluxGraphics(), g_xEngine.VolumeFog(), g_xEngine.FluxRenderer()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SSGI().SetupRenderGraph(g); },
		+[](){ g_xEngine.SSGI().Shutdown(); }); // Shutdown inherited from Flux_ScreenSpaceEffectBase CRTP base.
	const u_int uDynamicLights = xReg.Register(szFLUX_FEATURE_DYNAMIC_LIGHTS,
		+[](){ g_xEngine.DynamicLights().Initialise(g_xEngine.VulkanMemory(), g_xEngine.FluxGraphics()); },
		nullptr, // DynamicLights has no SetupRenderGraph (gather/upload front-end only).
		+[](){ g_xEngine.DynamicLights().Shutdown(); });
	const u_int uLightClustering = xReg.Register(szFLUX_FEATURE_LIGHT_CLUSTERING,
		+[](){ g_xEngine.LightClustering().Initialise(g_xEngine.VulkanMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.LightClustering().SetupRenderGraph(g); },
		+[](){ g_xEngine.LightClustering().Shutdown(); });
	const u_int uDeferredShading = xReg.Register(szFLUX_FEATURE_DEFERRED_SHADING,
		+[](){ g_xEngine.DeferredShading().Initialise(g_xEngine.FluxGraphics(), g_xEngine.HDR(), g_xEngine.Shadows(), g_xEngine.IBL(), g_xEngine.SSR(), g_xEngine.SSGI(), g_xEngine.DynamicLights(), g_xEngine.LightClustering()); },
		+[](Flux_RenderGraph& g){ g_xEngine.DeferredShading().SetupRenderGraph(g); },
		+[](){ g_xEngine.DeferredShading().Shutdown(); });
	const u_int uDecals = xReg.Register(szFLUX_FEATURE_DECALS,
		// DI seam: Decals::Initialise takes (Graphics&, Swapchain&).
		+[](){ g_xEngine.Decals().Initialise(g_xEngine.FluxGraphics(), g_xEngine.VulkanSwapchain(), g_xEngine.VulkanMemory(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Decals().SetupRenderGraph(g); },
		+[](){ g_xEngine.Decals().Shutdown(); });
	const u_int uSSAO = xReg.Register(szFLUX_FEATURE_SSAO,
		// DI seam: SSAO::Initialise takes (Graphics&, Swapchain&, HDR&).
		+[](){ g_xEngine.SSAO().Initialise(g_xEngine.FluxGraphics(), g_xEngine.VulkanSwapchain(), g_xEngine.HDR()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SSAO().SetupRenderGraph(g); },
		+[](){ g_xEngine.SSAO().Shutdown(); });
	const u_int uFog = xReg.Register(szFLUX_FEATURE_FOG,
		+[](){ g_xEngine.Fog().Initialise(g_xEngine.VolumeFog(), g_xEngine.GodRaysFog(), g_xEngine.RaymarchFog(), g_xEngine.FroxelFog(), g_xEngine.HDR(), g_xEngine.FluxGraphics(), g_xEngine.FluxRenderer(), g_xEngine.Shadows(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Fog().SetupRenderGraph(g); },
		nullptr); // Fog has no Shutdown() — RAII / stateless.
	const u_int uSDFs = xReg.Register(szFLUX_FEATURE_SDFS,
		// DI seam: SDFs::Initialise takes (Graphics&, HDR&) — the trampoline
		// gathers them from g_xEngine. HDR is registered first (above) so it
		// inits before the walk reaches SDFs.
		+[](){ g_xEngine.SDFs().Initialise(g_xEngine.FluxGraphics(), g_xEngine.HDR(), g_xEngine.VulkanMemory(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SDFs().SetupRenderGraph(g); },
		+[](){ g_xEngine.SDFs().Shutdown(); });
	const u_int uParticles = xReg.Register(szFLUX_FEATURE_PARTICLES,
		// DI seam (Wave-17, heaviest leaf — 3 cross-subsystem deps): Particles::Initialise
		// takes (Graphics&, HDR&, ParticleGPU&). FluxGraphics is brought up inline before
		// this walk; HDR is registered first (above) so it inits before the walk reaches
		// Particles; ParticleGPU is an engine-owned sibling singleton constructed in
		// Zenith_Engine::Initialise (well before any feature init walk), so all three deps
		// are ready when this trampoline fires. The ECS reach (g_xEngine.Scenes()) inside
		// the WS7 emitter-sim Prepare-gather stays self-routed by design.
		+[](){ g_xEngine.Particles().Initialise(g_xEngine.FluxGraphics(), g_xEngine.HDR(), g_xEngine.ParticleGPU()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Particles().SetupRenderGraph(g); },
		+[](){ g_xEngine.Particles().Shutdown(); });
	const u_int uQuads = xReg.Register(szFLUX_FEATURE_QUADS,
		// DI seam: Quads::Initialise takes (Graphics&). FluxGraphics is brought up
		// inline at the top of LateInitialise before this walk, so the dep is ready.
		+[](){ g_xEngine.Quads().Initialise(g_xEngine.FluxGraphics(), g_xEngine.VulkanMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Quads().SetupRenderGraph(g); },
		+[](){ g_xEngine.Quads().Shutdown(); });
	const u_int uText = xReg.Register(szFLUX_FEATURE_TEXT,
		// DI seam: Text::Initialise takes (Graphics&). FluxGraphics is brought up
		// inline at the top of LateInitialise before this walk, so the dep is ready.
		+[](){ g_xEngine.Text().Initialise(g_xEngine.FluxGraphics(), g_xEngine.VulkanMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Text().SetupRenderGraph(g); },
		+[](){ g_xEngine.Text().Shutdown(); });

	// ===== SETUP ORDER (four sub-walks) ====================================
	// Matches Flux_RendererImpl::SetupRenderGraph EXACTLY. The inline irregulars
	// (FluxGraphics/HDR SetupTransients, Skybox aerial perspective, post-fog game
	// hook, final-RT pass) live in SetupRenderGraph between these phases.

	// Phase 1: preprocessing -> geometry -> decals -> screen-space -> lighting,
	// up to and including DeferredShading.
	xReg.AddToSetupWalk(uIBL,             FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uSkybox,          FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uShadows,         FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uStaticMeshes,    FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uTerrain,         FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uPrimitives,      FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uAnimatedMeshes,  FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uInstancedMeshes, FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uGrass,           FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uDecals,          FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uHiZ,             FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uSSR,             FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uSSGI,            FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uLightClustering, FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	xReg.AddToSetupWalk(uDeferredShading, FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING);
	// (inline irregular: Skybox aerial perspective)

	// Phase 2: SSAO + Fog.
	xReg.AddToSetupWalk(uSSAO, FLUX_SETUP_PHASE_SSAO_FOG);
	xReg.AddToSetupWalk(uFog,  FLUX_SETUP_PHASE_SSAO_FOG);
	// (inline irregular: post-fog game render hook)

	// Phase 3: SDFs + Particles + HDR (HDR second touch).
	xReg.AddToSetupWalk(uSDFs,      FLUX_SETUP_PHASE_POST_PROCESS);
	xReg.AddToSetupWalk(uParticles, FLUX_SETUP_PHASE_POST_PROCESS);
	xReg.AddToSetupWalk(uHDR,       FLUX_SETUP_PHASE_POST_PROCESS);

	// Phase 4: UI quads + text + (tools) gizmos.
	xReg.AddToSetupWalk(uQuads, FLUX_SETUP_PHASE_UI);
	xReg.AddToSetupWalk(uText,  FLUX_SETUP_PHASE_UI);
#ifdef ZENITH_TOOLS
	xReg.AddToSetupWalk(uGizmos, FLUX_SETUP_PHASE_UI);
#endif
	// (inline irregular: final-RT layout-transition pass)

	// Note: FluxGraphics' setup is the SetupTransients irregular (handled inline,
	// no registry setup trampoline), so uGraphics is intentionally absent from
	// the setup walk.
	(void)uGraphics;

	// ===== SHUTDOWN ORDER ==================================================
	// Matches Flux_RendererImpl::Shutdown's explicit reverse-order block. NOT a
	// mechanical reverse(init): it is transcribed exactly. FluxGraphics, HDR and
	// (tools) Gizmos shut down INLINE in Shutdown() — they are deliberately NOT
	// added here. Fog has no Shutdown and is therefore absent too.
	xReg.AddToShutdownWalk(uText);
	xReg.AddToShutdownWalk(uQuads);
	xReg.AddToShutdownWalk(uParticles);
	xReg.AddToShutdownWalk(uSDFs);
	// (Fog — no Shutdown())
	xReg.AddToShutdownWalk(uDeferredShading);
	xReg.AddToShutdownWalk(uSSAO);
	xReg.AddToShutdownWalk(uDecals);
	xReg.AddToShutdownWalk(uLightClustering);
	xReg.AddToShutdownWalk(uDynamicLights);
	xReg.AddToShutdownWalk(uSSGI);
	xReg.AddToShutdownWalk(uSSR);
	xReg.AddToShutdownWalk(uHiZ);
	xReg.AddToShutdownWalk(uPrimitives);
	xReg.AddToShutdownWalk(uGrass);
	xReg.AddToShutdownWalk(uTerrain);
	xReg.AddToShutdownWalk(uInstancedMeshes);
	xReg.AddToShutdownWalk(uAnimatedMeshes);
	xReg.AddToShutdownWalk(uStaticMeshes);
	xReg.AddToShutdownWalk(uIBL);
	xReg.AddToShutdownWalk(uSkybox);
	xReg.AddToShutdownWalk(uShadows);

#ifdef ZENITH_RUNTIME_CHECKS
	xReg.VerifyOrder();
#endif
}

#ifdef ZENITH_RUNTIME_CHECKS
// ---------------------------------------------------------------------------
// GOLDEN-ORDER CHECK (W6.2: release-survivable lifecycle backstop).
//
// Each golden array is transcribed from the pre-refactor Flux.cpp and is the
// authority. VerifyOrder checks the registry's emitted sequences EXACTLY match
// them, so any future accidental reorder of the Register / AddTo* calls is caught
// at boot (logged via Zenith_Check, which survives Release — only a future Final
// config that undefines ZENITH_RUNTIME_CHECKS strips it) rather than silently
// corrupting init / render / teardown.
// ---------------------------------------------------------------------------
namespace
{
	// INIT order — matches LateInitialise (walk begins at FluxGraphics).
	const char* const s_aszGoldenInitOrder[] =
	{
		szFLUX_FEATURE_GRAPHICS,
		szFLUX_FEATURE_HDR,
#ifdef ZENITH_TOOLS
		szFLUX_FEATURE_GIZMOS,
#endif
		szFLUX_FEATURE_SHADOWS,
		szFLUX_FEATURE_SKYBOX,
		szFLUX_FEATURE_IBL,
		szFLUX_FEATURE_STATIC_MESHES,
		szFLUX_FEATURE_ANIMATED_MESHES,
		szFLUX_FEATURE_INSTANCED_MESHES,
		szFLUX_FEATURE_TERRAIN,
		szFLUX_FEATURE_GRASS,
		szFLUX_FEATURE_PRIMITIVES,
		szFLUX_FEATURE_HIZ,
		szFLUX_FEATURE_SSR,
		szFLUX_FEATURE_SSGI,
		szFLUX_FEATURE_DYNAMIC_LIGHTS,
		szFLUX_FEATURE_LIGHT_CLUSTERING,
		szFLUX_FEATURE_DEFERRED_SHADING,
		szFLUX_FEATURE_DECALS,
		szFLUX_FEATURE_SSAO,
		szFLUX_FEATURE_FOG,
		szFLUX_FEATURE_SDFS,
		szFLUX_FEATURE_PARTICLES,
		szFLUX_FEATURE_QUADS,
		szFLUX_FEATURE_TEXT,
	};

	// SETUP sub-walks — match SetupRenderGraph between the inline irregulars.
	const char* const s_aszGoldenSetupPrepassToLighting[] =
	{
		szFLUX_FEATURE_IBL,
		szFLUX_FEATURE_SKYBOX,
		szFLUX_FEATURE_SHADOWS,
		szFLUX_FEATURE_STATIC_MESHES,
		szFLUX_FEATURE_TERRAIN,
		szFLUX_FEATURE_PRIMITIVES,
		szFLUX_FEATURE_ANIMATED_MESHES,
		szFLUX_FEATURE_INSTANCED_MESHES,
		szFLUX_FEATURE_GRASS,
		szFLUX_FEATURE_DECALS,
		szFLUX_FEATURE_HIZ,
		szFLUX_FEATURE_SSR,
		szFLUX_FEATURE_SSGI,
		szFLUX_FEATURE_LIGHT_CLUSTERING,
		szFLUX_FEATURE_DEFERRED_SHADING,
	};
	const char* const s_aszGoldenSetupSSAOFog[] =
	{
		szFLUX_FEATURE_SSAO,
		szFLUX_FEATURE_FOG,
	};
	const char* const s_aszGoldenSetupPostProcess[] =
	{
		szFLUX_FEATURE_SDFS,
		szFLUX_FEATURE_PARTICLES,
		szFLUX_FEATURE_HDR,
	};
	const char* const s_aszGoldenSetupUI[] =
	{
		szFLUX_FEATURE_QUADS,
		szFLUX_FEATURE_TEXT,
#ifdef ZENITH_TOOLS
		szFLUX_FEATURE_GIZMOS,
#endif
	};

	// SHUTDOWN order — matches the reverse-order Shutdown block (FluxGraphics /
	// HDR / Gizmos shut down inline and are absent; Fog has no Shutdown).
	const char* const s_aszGoldenShutdownOrder[] =
	{
		szFLUX_FEATURE_TEXT,
		szFLUX_FEATURE_QUADS,
		szFLUX_FEATURE_PARTICLES,
		szFLUX_FEATURE_SDFS,
		szFLUX_FEATURE_DEFERRED_SHADING,
		szFLUX_FEATURE_SSAO,
		szFLUX_FEATURE_DECALS,
		szFLUX_FEATURE_LIGHT_CLUSTERING,
		szFLUX_FEATURE_DYNAMIC_LIGHTS,
		szFLUX_FEATURE_SSGI,
		szFLUX_FEATURE_SSR,
		szFLUX_FEATURE_HIZ,
		szFLUX_FEATURE_PRIMITIVES,
		szFLUX_FEATURE_GRASS,
		szFLUX_FEATURE_TERRAIN,
		szFLUX_FEATURE_INSTANCED_MESHES,
		szFLUX_FEATURE_ANIMATED_MESHES,
		szFLUX_FEATURE_STATIC_MESHES,
		szFLUX_FEATURE_IBL,
		szFLUX_FEATURE_SKYBOX,
		szFLUX_FEATURE_SHADOWS,
	};
}

void Flux_FeatureRegistry::VerifyOrder() const
{
	// ---- INIT order ----
	Zenith_Check(m_uNumFeatures == COUNT_OF(s_aszGoldenInitOrder),
		"Flux_FeatureRegistry::VerifyOrder: init count %u != golden %u — a feature was added/removed without updating the golden init array",
		m_uNumFeatures, (u_int)COUNT_OF(s_aszGoldenInitOrder));
	for (u_int u = 0; u < m_uNumFeatures; u++)
	{
		Zenith_Check(strcmp(m_axFeatures[u].m_szName, s_aszGoldenInitOrder[u]) == 0,
			"Flux_FeatureRegistry::VerifyOrder: init order mismatch at %u: registry '%s' != golden '%s'",
			u, m_axFeatures[u].m_szName, s_aszGoldenInitOrder[u]);
	}

	// ---- SETUP sub-walks ----
	struct GoldenPhase
	{
		Flux_FeatureSetupPhase ePhase;
		const char* const*     aszGolden;
		u_int                  uCount;
	};
	const GoldenPhase axGolden[] =
	{
		{ FLUX_SETUP_PHASE_PREPASS_TO_LIGHTING, s_aszGoldenSetupPrepassToLighting, (u_int)COUNT_OF(s_aszGoldenSetupPrepassToLighting) },
		{ FLUX_SETUP_PHASE_SSAO_FOG,            s_aszGoldenSetupSSAOFog,           (u_int)COUNT_OF(s_aszGoldenSetupSSAOFog) },
		{ FLUX_SETUP_PHASE_POST_PROCESS,        s_aszGoldenSetupPostProcess,       (u_int)COUNT_OF(s_aszGoldenSetupPostProcess) },
		{ FLUX_SETUP_PHASE_UI,                  s_aszGoldenSetupUI,                (u_int)COUNT_OF(s_aszGoldenSetupUI) },
	};
	for (const GoldenPhase& xGolden : axGolden)
	{
		u_int uSeen = 0;
		for (u_int u = 0; u < m_uNumSetup; u++)
		{
			if (m_aeSetupPhase[u] != xGolden.ePhase)
				continue;
			Zenith_Check(uSeen < xGolden.uCount,
				"Flux_FeatureRegistry::VerifyOrder: setup phase %u has more features than golden (%u)",
				(u_int)xGolden.ePhase, xGolden.uCount);
			const char* szName = m_axFeatures[m_auSetupOrder[u]].m_szName;
			Zenith_Check(strcmp(szName, xGolden.aszGolden[uSeen]) == 0,
				"Flux_FeatureRegistry::VerifyOrder: setup phase %u mismatch at %u: registry '%s' != golden '%s'",
				(u_int)xGolden.ePhase, uSeen, szName, xGolden.aszGolden[uSeen]);
			uSeen++;
		}
		Zenith_Check(uSeen == xGolden.uCount,
			"Flux_FeatureRegistry::VerifyOrder: setup phase %u count %u != golden %u",
			(u_int)xGolden.ePhase, uSeen, xGolden.uCount);
	}

	// ---- SHUTDOWN order ----
	Zenith_Check(m_uNumShutdown == COUNT_OF(s_aszGoldenShutdownOrder),
		"Flux_FeatureRegistry::VerifyOrder: shutdown count %u != golden %u",
		m_uNumShutdown, (u_int)COUNT_OF(s_aszGoldenShutdownOrder));
	for (u_int u = 0; u < m_uNumShutdown; u++)
	{
		const char* szName = m_axFeatures[m_auShutdownOrder[u]].m_szName;
		Zenith_Check(strcmp(szName, s_aszGoldenShutdownOrder[u]) == 0,
			"Flux_FeatureRegistry::VerifyOrder: shutdown order mismatch at %u: registry '%s' != golden '%s'",
			u, szName, s_aszGoldenShutdownOrder[u]);
	}
}
#endif // ZENITH_RUNTIME_CHECKS
