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

#include <cstring> // strcmp — name lookups (Register dup-check / FindFeatureIndex / HasSetupStepNamed); deliberately not in the header.

// Moved here from Flux.cpp: no-op record callback for the final-layout-transition
// pass. The pass carries no commands — it exists only so the render graph emits a
// prologue barrier putting the Final RT into SHADER_READ_ONLY_OPTIMAL for the
// swapchain copy (which lives outside the graph). File-static, not header-exposed;
// referenced solely by the @FinalRTLayoutTransition setup step below.
static void Flux_FinalLayoutTransitionNoOp(Flux_CommandList*, void*)
{
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

void Flux_FeatureRegistry::Register(const char* szName,
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

	Flux_FeatureDesc& xDesc = m_axFeatures[m_uNumFeatures];
	xDesc.m_szName = szName;
	xDesc.m_pfnInitialise = pfnInitialise;
	xDesc.m_pfnSetupRenderGraph = pfnSetupRenderGraph;
	xDesc.m_pfnShutdown = pfnShutdown;
	m_uNumFeatures++;
}

u_int Flux_FeatureRegistry::FindFeatureIndex(const char* szName) const
{
	Zenith_Assert(szName != nullptr, "Flux_FeatureRegistry::FindFeatureIndex: null feature name");
	for (u_int u = 0; u < m_uNumFeatures; u++)
	{
		if (strcmp(m_axFeatures[u].m_szName, szName) == 0)
			return u;
	}
	Zenith_Assert(false, "Flux_FeatureRegistry::FindFeatureIndex: no feature named '%s' is registered", szName);
	return 0;
}

void Flux_FeatureRegistry::AddToSetupWalk(const char* szFeatureName)
{
	const u_int uFeatureIndex = FindFeatureIndex(szFeatureName);
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

void Flux_FeatureRegistry::AddToShutdownWalk(const char* szFeatureName)
{
	Zenith_Assert(m_uNumShutdown < FLUX_MAX_FEATURES, "Flux_FeatureRegistry::AddToShutdownWalk: shutdown-order overflow");
	m_auShutdownOrder[m_uNumShutdown] = FindFeatureIndex(szFeatureName);
	m_uNumShutdown++;
}

void Flux_FeatureRegistry::RunSetup(Flux_RenderGraph& xGraph) const
{
#ifdef ZENITH_RUNTIME_CHECKS
	// Backstop: every registered game feature's runAfter anchor must name a real
	// engine setup step.
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
// Default feature set. Every regular feature's trampolines are mechanical —
// g_xEngine.X().Initialise() / .SetupRenderGraph(g) / .Shutdown() — so they are
// stamped out by the NTTP helpers below (each instantiation is a distinct,
// breakpointable function; captureless, no std::function). The irregulars
// (FluxGraphics / DynamicLights / Fog) stay longhand.
// ---------------------------------------------------------------------------

// pfnAccessor is a Zenith_Engine member-accessor pointer (e.g. &Zenith_Engine::HDR).
template<auto pfnAccessor> static void FluxFeatureInitialise()                    { (g_xEngine.*pfnAccessor)().Initialise(); }
template<auto pfnAccessor> static void FluxFeatureSetup(Flux_RenderGraph& xGraph) { (g_xEngine.*pfnAccessor)().SetupRenderGraph(xGraph); }
template<auto pfnAccessor> static void FluxFeatureShutdown()                      { (g_xEngine.*pfnAccessor)().Shutdown(); }

template<auto pfnAccessor>
static void RegisterFeature(Flux_FeatureRegistry& xReg, const char* szName)
{
	xReg.Register(szName, &FluxFeatureInitialise<pfnAccessor>, &FluxFeatureSetup<pfnAccessor>, &FluxFeatureShutdown<pfnAccessor>);
}

void Flux_FeatureRegistry::RegisterDefaultFeatures()
{
	Flux_FeatureRegistry& xReg = Get();

	// Idempotency: headless boots skip LateInitialise and unit tests re-init
	// Flux in-process — reset to a known-empty table on every entry.
	xReg.Reset();

	// --- Features, in INIT order (== registration order). The order encodes
	// real subsystem dependencies — see the dependency-graph comment in
	// Flux_RendererImpl::LateInitialise. FluxGraphics is brought up inline
	// before the walk (null init/setup trampolines; its transient creation is
	// the @SetupTransients step below); DynamicLights has no graph setup
	// (gather/upload front-end only); Fog has no Shutdown (RAII / stateless).
	xReg.Register("FluxGraphics", nullptr, nullptr, +[](){ g_xEngine.FluxGraphics().Shutdown(); });
	RegisterFeature<&Zenith_Engine::HDR>(xReg, "HDR");
#ifdef ZENITH_TOOLS
	RegisterFeature<&Zenith_Engine::Gizmos>(xReg, "Gizmos");
#endif
	RegisterFeature<&Zenith_Engine::Shadows>(xReg, "Shadows");
	RegisterFeature<&Zenith_Engine::Skybox>(xReg, "Skybox");
	RegisterFeature<&Zenith_Engine::IBL>(xReg, "IBL");
	RegisterFeature<&Zenith_Engine::StaticMeshes>(xReg, "StaticMeshes");
	RegisterFeature<&Zenith_Engine::AnimatedMeshes>(xReg, "AnimatedMeshes");
	RegisterFeature<&Zenith_Engine::InstancedMeshes>(xReg, "InstancedMeshes");
	RegisterFeature<&Zenith_Engine::Terrain>(xReg, "Terrain");
	RegisterFeature<&Zenith_Engine::Grass>(xReg, "Grass");
	RegisterFeature<&Zenith_Engine::Primitives>(xReg, "Primitives");
	RegisterFeature<&Zenith_Engine::HiZ>(xReg, "HiZ");
	RegisterFeature<&Zenith_Engine::SSR>(xReg, "SSR");
	RegisterFeature<&Zenith_Engine::SSGI>(xReg, "SSGI");
	xReg.Register("DynamicLights", +[](){ g_xEngine.DynamicLights().Initialise(); }, nullptr, +[](){ g_xEngine.DynamicLights().Shutdown(); });
	RegisterFeature<&Zenith_Engine::LightClustering>(xReg, "LightClustering");
	RegisterFeature<&Zenith_Engine::DeferredShading>(xReg, "DeferredShading");
	RegisterFeature<&Zenith_Engine::Decals>(xReg, "Decals");
	RegisterFeature<&Zenith_Engine::SSAO>(xReg, "SSAO");
	xReg.Register("Fog", +[](){ g_xEngine.Fog().Initialise(); }, +[](Flux_RenderGraph& xGraph){ g_xEngine.Fog().SetupRenderGraph(xGraph); }, nullptr);
	RegisterFeature<&Zenith_Engine::SDFs>(xReg, "SDFs");
	RegisterFeature<&Zenith_Engine::Particles>(xReg, "Particles");
	RegisterFeature<&Zenith_Engine::Quads>(xReg, "Quads");
	RegisterFeature<&Zenith_Engine::Text>(xReg, "Text");

	// --- Setup walk: ONE continuous ordered list (declaration order seeds the
	// graph's topo sort — producers before consumers; see the ORDERING note in
	// the header). The irregulars are ordinary ordered steps. Game render
	// features anchored runAfter="Fog" are interleaved by RunSetup right after
	// the Fog step (see Zenith_GameRenderFeatures).
	xReg.AddSetupStep("@SetupTransients:FluxGraphics", +[](Flux_RenderGraph& xGraph){ g_xEngine.FluxGraphics().SetupTransients(xGraph); });
	xReg.AddSetupStep("@SetupTransients:HDR",          +[](Flux_RenderGraph& xGraph){ g_xEngine.HDR().SetupTransients(xGraph); });
	xReg.AddToSetupWalk("IBL");
	xReg.AddToSetupWalk("Skybox");
	xReg.AddToSetupWalk("Shadows");
	xReg.AddToSetupWalk("StaticMeshes");
	xReg.AddToSetupWalk("Terrain");
	xReg.AddToSetupWalk("Primitives");
	xReg.AddToSetupWalk("AnimatedMeshes");
	xReg.AddToSetupWalk("InstancedMeshes");
	xReg.AddToSetupWalk("Grass");
	xReg.AddToSetupWalk("Decals");
	xReg.AddToSetupWalk("HiZ");
	xReg.AddToSetupWalk("SSR");
	xReg.AddToSetupWalk("SSGI");
	xReg.AddToSetupWalk("LightClustering");
	xReg.AddToSetupWalk("DeferredShading");
	xReg.AddSetupStep("@Skybox:AerialPerspective", +[](Flux_RenderGraph& xGraph){ g_xEngine.Skybox().SetupAerialPerspectiveRenderGraph(xGraph); });
	xReg.AddToSetupWalk("SSAO");
	xReg.AddToSetupWalk("Fog");
	xReg.AddToSetupWalk("SDFs");
	xReg.AddToSetupWalk("Particles");
	xReg.AddToSetupWalk("HDR");
	xReg.AddToSetupWalk("Quads");
	xReg.AddToSetupWalk("Text");
#ifdef ZENITH_TOOLS
	xReg.AddToSetupWalk("Gizmos");
#endif
	xReg.AddSetupStep("@FinalRTLayoutTransition", +[](Flux_RenderGraph& xGraph){
		// Leaves the Final Render Target in SHADER_READ_ONLY_OPTIMAL so the
		// swapchain copy (outside the graph) can sample it. Pure reader → sorts
		// strictly after every Final-RT writer (tonemap/quads/text/gizmos).
		xGraph.AddPass("Final RT Layout Transition", Flux_FinalLayoutTransitionNoOp)
		      .Reads(g_xEngine.FluxGraphics().GetFinalRenderTarget(), RESOURCE_ACCESS_READ_SRV);
	});

	// --- Shutdown walk, transcribed reverse order (NOT a mechanical
	// reverse(init)). FluxGraphics, HDR and (tools) Gizmos shut down INLINE in
	// Flux_RendererImpl::Shutdown and are deliberately absent; Fog has no
	// Shutdown and is absent too.
	xReg.AddToShutdownWalk("Text");
	xReg.AddToShutdownWalk("Quads");
	xReg.AddToShutdownWalk("Particles");
	xReg.AddToShutdownWalk("SDFs");
	xReg.AddToShutdownWalk("DeferredShading");
	xReg.AddToShutdownWalk("SSAO");
	xReg.AddToShutdownWalk("Decals");
	xReg.AddToShutdownWalk("LightClustering");
	xReg.AddToShutdownWalk("DynamicLights");
	xReg.AddToShutdownWalk("SSGI");
	xReg.AddToShutdownWalk("SSR");
	xReg.AddToShutdownWalk("HiZ");
	xReg.AddToShutdownWalk("Primitives");
	xReg.AddToShutdownWalk("Grass");
	xReg.AddToShutdownWalk("Terrain");
	xReg.AddToShutdownWalk("InstancedMeshes");
	xReg.AddToShutdownWalk("AnimatedMeshes");
	xReg.AddToShutdownWalk("StaticMeshes");
	xReg.AddToShutdownWalk("IBL");
	xReg.AddToShutdownWalk("Skybox");
	xReg.AddToShutdownWalk("Shadows");
}
