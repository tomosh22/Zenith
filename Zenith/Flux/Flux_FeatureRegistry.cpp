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

#include "Flux/Zenith_GameRenderFeatures.h" // generic game render-feature interleave + anchor verify
#include "Flux/Flux_BackendTypes.h"

#include <cstring> // strcmp — golden-order verification only; deliberately not in the header.

// Moved here from Flux.cpp: no-op record callback for the final-layout-transition
// pass. The pass carries no commands — it exists only so the render graph emits a
// prologue barrier putting the Final RT into SHADER_READ_ONLY_OPTIMAL for the
// swapchain copy (which lives outside the graph). File-static, not header-exposed;
// referenced solely by the @FinalRTLayoutTransition setup step below.
static void Flux_FinalLayoutTransitionNoOp(Flux_CommandList*, void*)
{
}

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

	// Irregular setup-step pseudo-names (NOT registered features). Referenced by
	// both the setup walk in RegisterDefaultFeatures and the golden-order check
	// in VerifyOrder, so a typo in one place can't silently agree with the other.
	constexpr const char* szFLUX_STEP_TRANSIENTS_GRAPHICS = "@SetupTransients:FluxGraphics";
	constexpr const char* szFLUX_STEP_TRANSIENTS_HDR      = "@SetupTransients:HDR";
	constexpr const char* szFLUX_STEP_AERIAL_PERSPECTIVE  = "@Skybox:AerialPerspective";
	constexpr const char* szFLUX_STEP_FINAL_RT_TRANSITION = "@FinalRTLayoutTransition";
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
	m_uNumInitDeps = 0;
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

#ifdef ZENITH_RUNTIME_CHECKS
	// W6.2: duplicate-registration integrity check now survives Release (Zenith_Check).
	for (u_int u = 0; u < m_uNumFeatures; u++)
	{
		Zenith_Check(strcmp(m_axFeatures[u].m_szName, szName) != 0,
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

void Flux_FeatureRegistry::AddToSetupWalk(u_int uFeatureIndex)
{
	Zenith_Assert(uFeatureIndex < m_uNumFeatures,
		"Flux_FeatureRegistry::AddToSetupWalk: feature index %u out of range (%u registered)", uFeatureIndex, m_uNumFeatures);
	Zenith_Assert(m_axFeatures[uFeatureIndex].m_pfnSetupRenderGraph != nullptr,
		"Flux_FeatureRegistry::AddToSetupWalk: feature '%s' has no SetupRenderGraph trampoline; do not add it to the setup walk",
		m_axFeatures[uFeatureIndex].m_szName);
	AddSetupStep(m_axFeatures[uFeatureIndex].m_szName, m_axFeatures[uFeatureIndex].m_pfnSetupRenderGraph);
}

void Flux_FeatureRegistry::AddSetupStep(const char* szName, void (*pfnSetup)(Flux_RenderGraph&))
{
	Zenith_Assert(szName != nullptr, "Flux_FeatureRegistry::AddSetupStep: null step name");
	Zenith_Assert(pfnSetup != nullptr, "Flux_FeatureRegistry::AddSetupStep: null step fn for '%s'", szName);
	Zenith_Assert(m_uNumSetup < FLUX_MAX_SETUP_STEPS, "Flux_FeatureRegistry::AddSetupStep: setup-walk overflow");
	m_axSetupSteps[m_uNumSetup].m_szName = szName;
	m_axSetupSteps[m_uNumSetup].m_pfnSetup = pfnSetup;
	m_uNumSetup++;
}

bool Flux_FeatureRegistry::HasSetupStepNamed(const char* szName) const
{
	if (szName == nullptr) return false;
	for (u_int u = 0; u < m_uNumSetup; u++)
	{
		if (m_axSetupSteps[u].m_szName != nullptr && strcmp(m_axSetupSteps[u].m_szName, szName) == 0)
			return true;
	}
	return false;
}

void Flux_FeatureRegistry::AddToShutdownWalk(u_int uFeatureIndex)
{
	Zenith_Assert(uFeatureIndex < m_uNumFeatures,
		"Flux_FeatureRegistry::AddToShutdownWalk: feature index %u out of range (%u registered)", uFeatureIndex, m_uNumFeatures);
	Zenith_Assert(m_uNumShutdown < FLUX_MAX_FEATURES, "Flux_FeatureRegistry::AddToShutdownWalk: shutdown-order overflow");
	m_auShutdownOrder[m_uNumShutdown] = uFeatureIndex;
	m_uNumShutdown++;
}

void Flux_FeatureRegistry::DeclareInitDependsOn(u_int uFeatureIndex, const char* szDependsOnName)
{
	Zenith_Assert(uFeatureIndex < m_uNumFeatures,
		"Flux_FeatureRegistry::DeclareInitDependsOn: feature index %u out of range (%u registered)", uFeatureIndex, m_uNumFeatures);
	Zenith_Assert(szDependsOnName != nullptr, "Flux_FeatureRegistry::DeclareInitDependsOn: null dependency name");
	Zenith_Assert(m_uNumInitDeps < FLUX_MAX_FEATURES * 4, "Flux_FeatureRegistry::DeclareInitDependsOn: init-dep overflow");
	m_auDepFeature[m_uNumInitDeps] = uFeatureIndex;
	m_aszDepName[m_uNumInitDeps] = szDependsOnName;
	m_uNumInitDeps++;
}

void Flux_FeatureRegistry::RunSetup(Flux_RenderGraph& xGraph) const
{
#ifdef ZENITH_RUNTIME_CHECKS
	// Backstop: every registered game feature's runAfter anchor must name a real
	// engine setup step (analogue of VerifyInitDependencies for game features).
	Zenith_GameRenderFeatures::VerifyGameFeatureAnchors();
#endif
	// Single ordered walk — no phase filtering. Every step's fn is non-null
	// (asserted at append time). For each engine step: tag the passes it adds with
	// the step name as their graph OWNER (so a game can force-disable a whole
	// feature group by name), run the step, clear the tag, then fire any game
	// render features anchored AFTER this step (their passes are owner-tagged with
	// the feature name inside InvokeFeaturesAnchoredAfter). This interleave is what
	// replaces the old hardcoded @GameHook:PostFog step — a game feature anchored
	// runAfter="Fog" lands in the exact declaration slot the hook used to occupy.
	// Pass execution order is then derived by the render graph's topological sort;
	// this walk is just the declaration order it falls back to.
	for (u_int u = 0; u < m_uNumSetup; u++)
	{
		xGraph.SetCurrentSetupOwner(m_axSetupSteps[u].m_szName);
		m_axSetupSteps[u].m_pfnSetup(xGraph);
		xGraph.SetCurrentSetupOwner(nullptr);
		Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter(m_axSetupSteps[u].m_szName, xGraph);
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
// Default feature set, registered in four steps that mirror the lifecycle:
//   RegisterDefaultFeatureSet  -> one Register(...) per feature, in INIT order
//   BuildDefaultSetupWalk      -> the single ordered SetupRenderGraph sequence
//   BuildDefaultShutdownWalk   -> the explicit (non-mechanical) shutdown order
//   DeclareDefaultInitDependencies -> the checkable init dependency graph
// The golden arrays + VerifyOrder/VerifyInitDependencies below are the
// release-survivable backstop that pins all three orders.
//
// Trampolines are captureless lambdas; the leading '+' decays each to a plain
// function pointer (std::function is forbidden engine-wide). HiZ, SSAO and
// friends take DI-injected dependency params, so their init trampolines gather
// those deps from g_xEngine (mirrors the explicit-injection call sites the
// inline LateInitialise used).
// ---------------------------------------------------------------------------

// Register() indices for the default features, used to wire the walks + deps.
struct DefaultFeatureIndices
{
	u_int uGraphics = 0;
	u_int uHDR = 0;
	u_int uShadows = 0;
	u_int uSkybox = 0;
	u_int uIBL = 0;
	u_int uStaticMeshes = 0;
	u_int uAnimatedMeshes = 0;
	u_int uInstancedMeshes = 0;
	u_int uTerrain = 0;
	u_int uGrass = 0;
	u_int uPrimitives = 0;
	u_int uHiZ = 0;
	u_int uSSR = 0;
	u_int uSSGI = 0;
	u_int uDynamicLights = 0;
	u_int uLightClustering = 0;
	u_int uDeferredShading = 0;
	u_int uDecals = 0;
	u_int uSSAO = 0;
	u_int uFog = 0;
	u_int uSDFs = 0;
	u_int uParticles = 0;
	u_int uQuads = 0;
	u_int uText = 0;
#ifdef ZENITH_TOOLS
	u_int uGizmos = 0;
#endif
};

// One Register(...) per feature, in INIT order (matches the pre-refactor
// LateInitialise block). The prologue (PerFrame / Vulkan / Swapchain / Slang /
// HotReload / ImGui) stays INLINE there; the init walk begins at FluxGraphics.
// Each Register also carries the feature's setup + shutdown function pointers
// (null where the feature lacks that phase).
static DefaultFeatureIndices RegisterDefaultFeatureSet(Flux_FeatureRegistry& xReg)
{
	DefaultFeatureIndices xIdx;
	xIdx.uGraphics = xReg.Register(szFLUX_FEATURE_GRAPHICS,
		nullptr, // FluxGraphics is brought up by its own Initialise() inline (core graphics, not a registry-init feature); its setup is the SetupTransients irregular below.
		nullptr,
		+[](){ g_xEngine.FluxGraphics().Shutdown(); });
	xIdx.uHDR = xReg.Register(szFLUX_FEATURE_HDR,
		+[](){ g_xEngine.HDR().Initialise(g_xEngine.FluxGraphics(), g_xEngine.FluxMemory(), g_xEngine.FluxSwapchain(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.HDR().SetupRenderGraph(g); }, // HDR's SECOND setup touch (bloom/tonemap); its FIRST touch (SetupTransients) is an inline irregular.
		+[](){ g_xEngine.HDR().Shutdown(); });
#ifdef ZENITH_TOOLS
	xIdx.uGizmos = xReg.Register(szFLUX_FEATURE_GIZMOS,
		+[](){ g_xEngine.Gizmos().Initialise(g_xEngine.FluxGraphics(), g_xEngine.Primitives(), g_xEngine.FluxMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Gizmos().SetupRenderGraph(g); },
		+[](){ g_xEngine.Gizmos().Shutdown(); });
#endif
	xIdx.uShadows = xReg.Register(szFLUX_FEATURE_SHADOWS,
		+[](){ g_xEngine.Shadows().Initialise(g_xEngine.FluxMemory(), g_xEngine.FluxGraphics(), g_xEngine.Profiling()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Shadows().SetupRenderGraph(g); },
		+[](){ g_xEngine.Shadows().Shutdown(); });
	xIdx.uSkybox = xReg.Register(szFLUX_FEATURE_SKYBOX,
		+[](){ g_xEngine.Skybox().Initialise(g_xEngine.FluxGraphics(), g_xEngine.HDR(), g_xEngine.FluxMemory(), g_xEngine.FluxBackend()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Skybox().SetupRenderGraph(g); }, // Skybox's aerial-perspective pass is a separate method invoked inline as an irregular.
		+[](){ g_xEngine.Skybox().Shutdown(); });
	xIdx.uIBL = xReg.Register(szFLUX_FEATURE_IBL,
		+[](){ g_xEngine.IBL().Initialise(); },
		+[](Flux_RenderGraph& g){ g_xEngine.IBL().SetupRenderGraph(g); },
		+[](){ g_xEngine.IBL().Shutdown(); });
	xIdx.uStaticMeshes = xReg.Register(szFLUX_FEATURE_STATIC_MESHES,
		// DI seam: StaticMeshes::Initialise takes (Graphics&). FluxGraphics is brought
		// up inline before this walk, so the dep is ready. The ECS reach inside
		// GatherDrawPacket (g_xEngine.Scenes()) stays self-routed by design.
		+[](){ g_xEngine.StaticMeshes().Initialise(g_xEngine.FluxGraphics()); },
		+[](Flux_RenderGraph& g){ g_xEngine.StaticMeshes().SetupRenderGraph(g); },
		+[](){ g_xEngine.StaticMeshes().Shutdown(); });
	xIdx.uAnimatedMeshes = xReg.Register(szFLUX_FEATURE_ANIMATED_MESHES,
		// DI seam: AnimatedMeshes::Initialise takes (Graphics&). FluxGraphics is
		// brought up inline at the top of LateInitialise before this walk, so the
		// dep is ready.
		+[](){ g_xEngine.AnimatedMeshes().Initialise(g_xEngine.FluxGraphics()); },
		+[](Flux_RenderGraph& g){ g_xEngine.AnimatedMeshes().SetupRenderGraph(g); },
		+[](){ g_xEngine.AnimatedMeshes().Shutdown(); });
	xIdx.uInstancedMeshes = xReg.Register(szFLUX_FEATURE_INSTANCED_MESHES,
		+[](){ g_xEngine.InstancedMeshes().Initialise(g_xEngine.FluxMemory(), g_xEngine.FluxGraphics()); },
		+[](Flux_RenderGraph& g){ g_xEngine.InstancedMeshes().SetupRenderGraph(g); },
		+[](){ g_xEngine.InstancedMeshes().Shutdown(); });
	xIdx.uTerrain = xReg.Register(szFLUX_FEATURE_TERRAIN,
		+[](){ g_xEngine.Terrain().Initialise(g_xEngine.FluxMemory(), g_xEngine.FluxGraphics(), g_xEngine.Profiling(), g_xEngine.TerrainStreaming()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Terrain().SetupRenderGraph(g); },
		+[](){ g_xEngine.Terrain().Shutdown(); });
	xIdx.uGrass = xReg.Register(szFLUX_FEATURE_GRASS,
		+[](){ g_xEngine.Grass().Initialise(g_xEngine.FluxMemory(), g_xEngine.Frame(), g_xEngine.FluxGraphics(), g_xEngine.HDR()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Grass().SetupRenderGraph(g); },
		+[](){ g_xEngine.Grass().Shutdown(); });
	xIdx.uPrimitives = xReg.Register(szFLUX_FEATURE_PRIMITIVES,
		// DI seam: Primitives::Initialise takes (Graphics&). FluxGraphics is brought
		// up before the walk reaches Primitives, so the trampoline forwards it.
		+[](){ g_xEngine.Primitives().Initialise(g_xEngine.FluxGraphics(), g_xEngine.FluxMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Primitives().SetupRenderGraph(g); },
		+[](){ g_xEngine.Primitives().Shutdown(); });
	xIdx.uHiZ = xReg.Register(szFLUX_FEATURE_HIZ,
		// DI seam: HiZ::Initialise takes (Swapchain&, Graphics&, Renderer&) — the
		// trampoline gathers them from g_xEngine, mirroring the inline call site.
		+[](){ g_xEngine.HiZ().Initialise(g_xEngine.FluxSwapchain(), g_xEngine.FluxGraphics(), g_xEngine.FluxRenderer()); },
		+[](Flux_RenderGraph& g){ g_xEngine.HiZ().SetupRenderGraph(g); },
		+[](){ g_xEngine.HiZ().Shutdown(); });
	xIdx.uSSR = xReg.Register(szFLUX_FEATURE_SSR,
		+[](){ g_xEngine.SSR().Initialise(g_xEngine.FluxMemory(), g_xEngine.FluxSwapchain(), g_xEngine.FluxGraphics(), g_xEngine.HiZ(), g_xEngine.VolumeFog(), g_xEngine.FluxRenderer()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SSR().SetupRenderGraph(g); },
		+[](){ g_xEngine.SSR().Shutdown(); }); // Shutdown inherited from Flux_ScreenSpaceEffectBase CRTP base.
	xIdx.uSSGI = xReg.Register(szFLUX_FEATURE_SSGI,
		+[](){ g_xEngine.SSGI().Initialise(g_xEngine.FluxSwapchain(), g_xEngine.HiZ(), g_xEngine.FluxGraphics(), g_xEngine.VolumeFog(), g_xEngine.FluxRenderer()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SSGI().SetupRenderGraph(g); },
		+[](){ g_xEngine.SSGI().Shutdown(); }); // Shutdown inherited from Flux_ScreenSpaceEffectBase CRTP base.
	xIdx.uDynamicLights = xReg.Register(szFLUX_FEATURE_DYNAMIC_LIGHTS,
		+[](){ g_xEngine.DynamicLights().Initialise(g_xEngine.FluxMemory(), g_xEngine.FluxGraphics()); },
		nullptr, // DynamicLights has no SetupRenderGraph (gather/upload front-end only).
		+[](){ g_xEngine.DynamicLights().Shutdown(); });
	xIdx.uLightClustering = xReg.Register(szFLUX_FEATURE_LIGHT_CLUSTERING,
		+[](){ g_xEngine.LightClustering().Initialise(g_xEngine.FluxMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.LightClustering().SetupRenderGraph(g); },
		+[](){ g_xEngine.LightClustering().Shutdown(); });
	xIdx.uDeferredShading = xReg.Register(szFLUX_FEATURE_DEFERRED_SHADING,
		+[](){ g_xEngine.DeferredShading().Initialise(g_xEngine.FluxGraphics(), g_xEngine.HDR(), g_xEngine.Shadows(), g_xEngine.IBL(), g_xEngine.SSR(), g_xEngine.SSGI(), g_xEngine.DynamicLights(), g_xEngine.LightClustering()); },
		+[](Flux_RenderGraph& g){ g_xEngine.DeferredShading().SetupRenderGraph(g); },
		+[](){ g_xEngine.DeferredShading().Shutdown(); });
	xIdx.uDecals = xReg.Register(szFLUX_FEATURE_DECALS,
		// DI seam: Decals::Initialise takes (Graphics&, Swapchain&).
		+[](){ g_xEngine.Decals().Initialise(g_xEngine.FluxGraphics(), g_xEngine.FluxSwapchain(), g_xEngine.FluxMemory(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Decals().SetupRenderGraph(g); },
		+[](){ g_xEngine.Decals().Shutdown(); });
	xIdx.uSSAO = xReg.Register(szFLUX_FEATURE_SSAO,
		// DI seam: SSAO::Initialise takes (Graphics&, Swapchain&, HDR&).
		+[](){ g_xEngine.SSAO().Initialise(g_xEngine.FluxGraphics(), g_xEngine.FluxSwapchain(), g_xEngine.HDR()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SSAO().SetupRenderGraph(g); },
		+[](){ g_xEngine.SSAO().Shutdown(); });
	xIdx.uFog = xReg.Register(szFLUX_FEATURE_FOG,
		+[](){ g_xEngine.Fog().Initialise(g_xEngine.VolumeFog(), g_xEngine.GodRaysFog(), g_xEngine.RaymarchFog(), g_xEngine.FroxelFog(), g_xEngine.HDR(), g_xEngine.FluxGraphics(), g_xEngine.FluxRenderer(), g_xEngine.Shadows(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Fog().SetupRenderGraph(g); },
		nullptr); // Fog has no Shutdown() — RAII / stateless.
	xIdx.uSDFs = xReg.Register(szFLUX_FEATURE_SDFS,
		// DI seam: SDFs::Initialise takes (Graphics&, HDR&) — the trampoline
		// gathers them from g_xEngine. HDR is registered first (above) so it
		// inits before the walk reaches SDFs.
		+[](){ g_xEngine.SDFs().Initialise(g_xEngine.FluxGraphics(), g_xEngine.HDR(), g_xEngine.FluxMemory(), g_xEngine.Frame()); },
		+[](Flux_RenderGraph& g){ g_xEngine.SDFs().SetupRenderGraph(g); },
		+[](){ g_xEngine.SDFs().Shutdown(); });
	xIdx.uParticles = xReg.Register(szFLUX_FEATURE_PARTICLES,
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
	xIdx.uQuads = xReg.Register(szFLUX_FEATURE_QUADS,
		// DI seam: Quads::Initialise takes (Graphics&). FluxGraphics is brought up
		// inline at the top of LateInitialise before this walk, so the dep is ready.
		+[](){ g_xEngine.Quads().Initialise(g_xEngine.FluxGraphics(), g_xEngine.FluxMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Quads().SetupRenderGraph(g); },
		+[](){ g_xEngine.Quads().Shutdown(); });
	xIdx.uText = xReg.Register(szFLUX_FEATURE_TEXT,
		// DI seam: Text::Initialise takes (Graphics&). FluxGraphics is brought up
		// inline at the top of LateInitialise before this walk, so the dep is ready.
		+[](){ g_xEngine.Text().Initialise(g_xEngine.FluxGraphics(), g_xEngine.FluxMemory()); },
		+[](Flux_RenderGraph& g){ g_xEngine.Text().SetupRenderGraph(g); },
		+[](){ g_xEngine.Text().Shutdown(); });
	return xIdx;
}

static void BuildDefaultSetupWalk(Flux_FeatureRegistry& xReg, const DefaultFeatureIndices& xIdx)
{
	// One continuous walk — NO discrete phases. The render graph computes pass
	// execution order by topologically sorting each pass's declared Reads/Writes,
	// but declaration order is load-bearing where it seeds that sort: producers
	// MUST precede consumers (a reader links only to an earlier-declared writer
	// of the same resource — see the ORDERING note in Flux_FeatureRegistry.h),
	// and same-resource writers run in declaration order. So this is transcribed
	// verbatim from the pre-collapse sequence (the four former sub-walks
	// concatenated + the former inline irregulars at their exact positions), and
	// the compiled order is unchanged. The irregulars — FluxGraphics/HDR transient
	// creation, the Skybox aerial-perspective pass, the post-fog game hook, and the
	// final-RT layout-transition pass — are ordinary ordered steps here.
	xReg.AddSetupStep(szFLUX_STEP_TRANSIENTS_GRAPHICS, +[](Flux_RenderGraph& g){ g_xEngine.FluxGraphics().SetupTransients(g); });
	xReg.AddSetupStep(szFLUX_STEP_TRANSIENTS_HDR,      +[](Flux_RenderGraph& g){ g_xEngine.HDR().SetupTransients(g); });
	xReg.AddToSetupWalk(xIdx.uIBL);
	xReg.AddToSetupWalk(xIdx.uSkybox);
	xReg.AddToSetupWalk(xIdx.uShadows);
	xReg.AddToSetupWalk(xIdx.uStaticMeshes);
	xReg.AddToSetupWalk(xIdx.uTerrain);
	xReg.AddToSetupWalk(xIdx.uPrimitives);
	xReg.AddToSetupWalk(xIdx.uAnimatedMeshes);
	xReg.AddToSetupWalk(xIdx.uInstancedMeshes);
	xReg.AddToSetupWalk(xIdx.uGrass);
	xReg.AddToSetupWalk(xIdx.uDecals);
	xReg.AddToSetupWalk(xIdx.uHiZ);
	xReg.AddToSetupWalk(xIdx.uSSR);
	xReg.AddToSetupWalk(xIdx.uSSGI);
	xReg.AddToSetupWalk(xIdx.uLightClustering);
	xReg.AddToSetupWalk(xIdx.uDeferredShading);
	xReg.AddSetupStep(szFLUX_STEP_AERIAL_PERSPECTIVE,  +[](Flux_RenderGraph& g){ g_xEngine.Skybox().SetupAerialPerspectiveRenderGraph(g); });
	xReg.AddToSetupWalk(xIdx.uSSAO);
	xReg.AddToSetupWalk(xIdx.uFog);
	// (The former @GameHook:PostFog step is gone. Game render features now anchor
	// runAfter="Fog" and are interleaved by RunSetup right here — see
	// Zenith_GameRenderFeatures + RunSetup's InvokeFeaturesAnchoredAfter.)
	xReg.AddToSetupWalk(xIdx.uSDFs);
	xReg.AddToSetupWalk(xIdx.uParticles);
	xReg.AddToSetupWalk(xIdx.uHDR);
	xReg.AddToSetupWalk(xIdx.uQuads);
	xReg.AddToSetupWalk(xIdx.uText);
#ifdef ZENITH_TOOLS
	xReg.AddToSetupWalk(xIdx.uGizmos);
#endif
	xReg.AddSetupStep(szFLUX_STEP_FINAL_RT_TRANSITION, +[](Flux_RenderGraph& g){
		// Leaves the Final Render Target in SHADER_READ_ONLY_OPTIMAL so the
		// swapchain copy (outside the graph) can sample it. Pure reader → sorts
		// strictly after every Final-RT writer (tonemap/quads/text/gizmos).
		g.AddPass("Final RT Layout Transition", Flux_FinalLayoutTransitionNoOp)
		 .Reads(g_xEngine.FluxGraphics().GetFinalRenderTarget(), RESOURCE_ACCESS_READ_SRV);
	});

	// FluxGraphics' own feature setup trampoline is null (its transient creation
	// is the @SetupTransients step above), so Graphics is intentionally absent
	// from the AddToSetupWalk calls.
}

static void BuildDefaultShutdownWalk(Flux_FeatureRegistry& xReg, const DefaultFeatureIndices& xIdx)
{
	// Matches Flux_RendererImpl::Shutdown's explicit reverse-order block. NOT a
	// mechanical reverse(init): it is transcribed exactly. FluxGraphics, HDR and
	// (tools) Gizmos shut down INLINE in Shutdown() — they are deliberately NOT
	// added here. Fog has no Shutdown and is therefore absent too.
	xReg.AddToShutdownWalk(xIdx.uText);
	xReg.AddToShutdownWalk(xIdx.uQuads);
	xReg.AddToShutdownWalk(xIdx.uParticles);
	xReg.AddToShutdownWalk(xIdx.uSDFs);
	// (Fog — no Shutdown())
	xReg.AddToShutdownWalk(xIdx.uDeferredShading);
	xReg.AddToShutdownWalk(xIdx.uSSAO);
	xReg.AddToShutdownWalk(xIdx.uDecals);
	xReg.AddToShutdownWalk(xIdx.uLightClustering);
	xReg.AddToShutdownWalk(xIdx.uDynamicLights);
	xReg.AddToShutdownWalk(xIdx.uSSGI);
	xReg.AddToShutdownWalk(xIdx.uSSR);
	xReg.AddToShutdownWalk(xIdx.uHiZ);
	xReg.AddToShutdownWalk(xIdx.uPrimitives);
	xReg.AddToShutdownWalk(xIdx.uGrass);
	xReg.AddToShutdownWalk(xIdx.uTerrain);
	xReg.AddToShutdownWalk(xIdx.uInstancedMeshes);
	xReg.AddToShutdownWalk(xIdx.uAnimatedMeshes);
	xReg.AddToShutdownWalk(xIdx.uStaticMeshes);
	xReg.AddToShutdownWalk(xIdx.uIBL);
	xReg.AddToShutdownWalk(xIdx.uSkybox);
	xReg.AddToShutdownWalk(xIdx.uShadows);
}

static void DeclareDefaultInitDependencies(Flux_FeatureRegistry& xReg, const DefaultFeatureIndices& xIdx)
{
	// Real init dependencies, read straight off each feature's Initialise trampoline
	// above (which consumes these features via g_xEngine, so they must be up first).
	// Declaring them makes the init ORDER checkable against the dependency GRAPH
	// (VerifyInitDependencies) — a reorder that puts a feature before a dependency it
	// consumes is now a structural boot error, not just a mismatch against the
	// transcribed golden snapshot (which a reorder + golden edit would silently pass).
	// FluxGraphics deps are omitted: it is brought up inline before the whole walk, so
	// it trivially precedes every feature.
	xReg.DeclareInitDependsOn(xIdx.uSkybox,          szFLUX_FEATURE_HDR);
	xReg.DeclareInitDependsOn(xIdx.uGrass,           szFLUX_FEATURE_HDR);
	xReg.DeclareInitDependsOn(xIdx.uSSR,             szFLUX_FEATURE_HIZ);
	xReg.DeclareInitDependsOn(xIdx.uSSGI,            szFLUX_FEATURE_HIZ);
	xReg.DeclareInitDependsOn(xIdx.uDeferredShading, szFLUX_FEATURE_HDR);
	xReg.DeclareInitDependsOn(xIdx.uDeferredShading, szFLUX_FEATURE_SHADOWS);
	xReg.DeclareInitDependsOn(xIdx.uDeferredShading, szFLUX_FEATURE_IBL);
	xReg.DeclareInitDependsOn(xIdx.uDeferredShading, szFLUX_FEATURE_SSR);
	xReg.DeclareInitDependsOn(xIdx.uDeferredShading, szFLUX_FEATURE_SSGI);
	xReg.DeclareInitDependsOn(xIdx.uDeferredShading, szFLUX_FEATURE_DYNAMIC_LIGHTS);
	xReg.DeclareInitDependsOn(xIdx.uDeferredShading, szFLUX_FEATURE_LIGHT_CLUSTERING);
	xReg.DeclareInitDependsOn(xIdx.uSSAO,            szFLUX_FEATURE_HDR);
	xReg.DeclareInitDependsOn(xIdx.uFog,             szFLUX_FEATURE_HDR);
	xReg.DeclareInitDependsOn(xIdx.uFog,             szFLUX_FEATURE_SHADOWS);
	xReg.DeclareInitDependsOn(xIdx.uSDFs,            szFLUX_FEATURE_HDR);
	xReg.DeclareInitDependsOn(xIdx.uParticles,       szFLUX_FEATURE_HDR);
}

void Flux_FeatureRegistry::RegisterDefaultFeatures()
{
	Flux_FeatureRegistry& xReg = Flux_FeatureRegistry::Get();

	// Idempotency: headless boots skip LateInitialise and unit tests re-init
	// Flux in-process — reset to a known-empty table on every entry.
	xReg.Reset();

	const DefaultFeatureIndices xIdx = RegisterDefaultFeatureSet(xReg);
	BuildDefaultSetupWalk(xReg, xIdx);
	BuildDefaultShutdownWalk(xReg, xIdx);
	DeclareDefaultInitDependencies(xReg, xIdx);

#ifdef ZENITH_RUNTIME_CHECKS
	xReg.VerifyOrder();
	xReg.VerifyInitDependencies();
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

	// SETUP walk — the single ordered list (feature setups + irregular steps),
	// matching RegisterDefaultFeatures' AddSetupStep / AddToSetupWalk sequence
	// exactly. This is the declaration order the render graph falls back to when
	// ordering writers of the same resource.
	const char* const s_aszGoldenSetupOrder[] =
	{
		szFLUX_STEP_TRANSIENTS_GRAPHICS,
		szFLUX_STEP_TRANSIENTS_HDR,
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
		szFLUX_STEP_AERIAL_PERSPECTIVE,
		szFLUX_FEATURE_SSAO,
		szFLUX_FEATURE_FOG,
		szFLUX_FEATURE_SDFS,
		szFLUX_FEATURE_PARTICLES,
		szFLUX_FEATURE_HDR,
		szFLUX_FEATURE_QUADS,
		szFLUX_FEATURE_TEXT,
#ifdef ZENITH_TOOLS
		szFLUX_FEATURE_GIZMOS,
#endif
		szFLUX_STEP_FINAL_RT_TRANSITION,
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

	// ---- SETUP walk (single flat ordered list) ----
	Zenith_Check(m_uNumSetup == COUNT_OF(s_aszGoldenSetupOrder),
		"Flux_FeatureRegistry::VerifyOrder: setup step count %u != golden %u — a setup step (feature or irregular) was added/removed without updating the golden setup array",
		m_uNumSetup, (u_int)COUNT_OF(s_aszGoldenSetupOrder));
	for (u_int u = 0; u < m_uNumSetup; u++)
	{
		Zenith_Check(strcmp(m_axSetupSteps[u].m_szName, s_aszGoldenSetupOrder[u]) == 0,
			"Flux_FeatureRegistry::VerifyOrder: setup order mismatch at %u: registry '%s' != golden '%s'",
			u, m_axSetupSteps[u].m_szName, s_aszGoldenSetupOrder[u]);
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

// ---------------------------------------------------------------------------
// W6.1 DEPENDENCY-GRAPH CHECK. Where VerifyOrder above asserts the init order
// equals a transcribed golden SNAPSHOT, this asserts it satisfies the declared
// dependency GRAPH: every feature initialises strictly after every feature it
// consumes. This catches a *semantically* wrong reorder (a feature moved ahead of
// a dependency) even if someone updated the golden array to match the bad order —
// the golden check alone would pass that, the graph check will not.
// ---------------------------------------------------------------------------
void Flux_FeatureRegistry::VerifyInitDependencies() const
{
	for (u_int u = 0; u < m_uNumInitDeps; u++)
	{
		const u_int uFeature = m_auDepFeature[u];
		const char* szDep    = m_aszDepName[u];

		// Resolve the dependency's init index by name (it must be a registered feature).
		u_int uDepIndex = FLUX_MAX_FEATURES;
		for (u_int v = 0; v < m_uNumFeatures; v++)
		{
			if (strcmp(m_axFeatures[v].m_szName, szDep) == 0)
			{
				uDepIndex = v;
				break;
			}
		}
		Zenith_Check(uDepIndex < m_uNumFeatures,
			"Flux_FeatureRegistry::VerifyInitDependencies: '%s' declares a dependency on '%s', which is not a registered feature",
			m_axFeatures[uFeature].m_szName, szDep);

		// Init order == registration order == index order, so the dependency must
		// have a strictly lower index (initialise first).
		Zenith_Check(uFeature > uDepIndex,
			"Flux_FeatureRegistry::VerifyInitDependencies: init order violates a declared dependency — '%s' (init #%u) must initialise AFTER '%s' (init #%u) which it consumes",
			m_axFeatures[uFeature].m_szName, uFeature, szDep, uDepIndex);
	}
}
#endif // ZENITH_RUNTIME_CHECKS
