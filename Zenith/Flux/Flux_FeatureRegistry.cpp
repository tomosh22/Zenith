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
#include "Flux/Translucency/Flux_TranslucencyImpl.h"
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
#include "Flux/Present/Flux_PresentImpl.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "Flux/MaterialPreview/Flux_MaterialPreviewImpl.h"
#include "Flux/Gizmos/Flux_Gizmos_Shaders.h"
#include "Flux/MaterialPreview/Flux_MaterialPreview_Shaders.h"
#endif

// Per-feature shader decls — each feature passes its apxALL to RegisterFeature so
// the registry carries structural shader ownership (drives hot-reload + the
// catalog parity check). Pure-data leaf headers; no layering impact.
#include "Flux/IBL/Flux_IBL_Shaders.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes_Shaders.h"
#include "Flux/Terrain/Flux_Terrain_Shaders.h"
#include "Flux/Primitives/Flux_Primitives_Shaders.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes_Shaders.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshes_Shaders.h"
#include "Flux/Skybox/Flux_Skybox_Shaders.h"
#include "Flux/Decals/Flux_Decals_Shaders.h"
#include "Flux/HiZ/Flux_HiZ_Shaders.h"
#include "Flux/SSR/Flux_SSR_Shaders.h"
#include "Flux/SSGI/Flux_SSGI_Shaders.h"
#include "Flux/SSAO/Flux_SSAO_Shaders.h"
#include "Flux/DynamicLights/Flux_LightClustering_Shaders.h"
#include "Flux/DeferredShading/Flux_DeferredShading_Shaders.h"
#include "Flux/Vegetation/Flux_Grass_Shaders.h"
#include "Flux/Translucency/Flux_Translucency_Shaders.h"
#include "Flux/Fog/Flux_Fog_Shaders.h"
#include "Flux/SDFs/Flux_SDFs_Shaders.h"
#include "Flux/Particles/Flux_Particles_Shaders.h"
#include "Flux/HDR/Flux_HDR_Shaders.h"
#include "Flux/Quads/Flux_Quads_Shaders.h"
#include "Flux/Text/Flux_Text_Shaders.h"
#include "Flux/Present/Flux_Present_Shaders.h"

#include "Flux/Zenith_GameRenderFeatures.h" // generic game render-feature interleave + anchor verify
#include "Flux/Flux_BackendTypes.h"

#include <cstring> // strcmp — name lookups (Register dup-check / FindFeatureByName / HasSetupStepNamed); deliberately not in the header.
#include <type_traits> // std::remove_cvref_t — strip the accessor's returned T& for the FluxRenderFeature constraint on RegisterFeature.

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
}

void Flux_FeatureRegistry::Register(const char* szName,
	void (*pfnInitialise)(),
	void (*pfnSetupRenderGraph)(Flux_RenderGraph&),
	void (*pfnShutdown)(),
	void (*pfnBuildPipelines)(),
	const Flux_ShaderDecl* const* paxShaders,
	u_int uShaderCount)
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
	xDesc.m_pfnBuildPipelines = pfnBuildPipelines;
	xDesc.m_paxShaders = paxShaders;
	xDesc.m_uShaderCount = uShaderCount;
	m_uNumFeatures++;

	// One call wires everything: append the feature's SetupRenderGraph trampoline to
	// the setup walk at THIS position. The registration order (= the render-graph
	// declaration order) thus drives init (forward), setup (here), and shutdown
	// (reverse) from a single RegisterFeature call. Every feature has a
	// SetupRenderGraph — a no-op for features that declare no passes (FluxGraphics /
	// DynamicLights) — so every feature is in the walk; the no-op ones add nothing to
	// the graph. (Guarded for null only so a direct Register(...) test call can omit
	// it; RegisterFeature always passes a non-null trampoline.)
	if (pfnSetupRenderGraph != nullptr)
		AddSetupStep(szName, pfnSetupRenderGraph);
}

const Flux_FeatureDesc* Flux_FeatureRegistry::FindFeatureByName(const char* szName) const
{
	if (szName == nullptr) return nullptr;
	for (u_int u = 0; u < m_uNumFeatures; u++)
	{
		if (strcmp(m_axFeatures[u].m_szName, szName) == 0)
			return &m_axFeatures[u];
	}
	return nullptr;
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
	// Reverse registration order. This is a correct teardown order because no
	// feature's Shutdown reads another feature (only foundation + own state —
	// verified), so reverse-of-init needs no hand-tuning. Features with no Shutdown
	// trampoline (Fog — RAII / stateless) are skipped.
	for (u_int u = m_uNumFeatures; u-- > 0; )
	{
		if (m_axFeatures[u].m_pfnShutdown != nullptr)
			m_axFeatures[u].m_pfnShutdown();
	}
}

// ---------------------------------------------------------------------------
// Default feature set. Every feature — without exception — is added through the
// single RegisterFeature<&Zenith_Engine::X> one-liner; the NTTP helpers below
// stamp out whichever of Initialise / SetupRenderGraph / Shutdown / BuildPipelines
// the subsystem implements (each instantiation is a distinct, breakpointable,
// captureless free function; no std::function).
// ---------------------------------------------------------------------------

// pfnAccessor is a Zenith_Engine member-accessor pointer (e.g. &Zenith_Engine::HDR).
// One captureless trampoline per lifecycle method; every feature implements all
// four (a no-op where it has nothing to do — see FluxRenderFeature), so all four
// are wired unconditionally — no presence detection.
template<auto pfnAccessor> static void FluxFeatureInitialise()                       { (g_xEngine.*pfnAccessor)().Initialise(); }
template<auto pfnAccessor> static void FluxFeatureSetup(Flux_RenderGraph& xGraph)    { (g_xEngine.*pfnAccessor)().SetupRenderGraph(xGraph); }
template<auto pfnAccessor> static void FluxFeatureShutdown()                         { (g_xEngine.*pfnAccessor)().Shutdown(); }
template<auto pfnAccessor> static void FluxFeatureBuildPipelines()                   { (g_xEngine.*pfnAccessor)().BuildPipelines(); }

// Compile-time self-test for the FluxRenderFeature concept. Zero runtime cost — the
// mock methods are only DECLARED (never defined/called) and the asserts evaluate at
// compile time. A normal build only proves the real features SATISFY the concept;
// these also prove it REJECTS a type that is missing one of the four mandatory
// lifecycle methods (which the build, where every feature is complete, can't show).
namespace
{
	struct FeatureCheck_Full        { void Initialise(); void SetupRenderGraph(Flux_RenderGraph&); void Shutdown(); void BuildPipelines(); };
	struct FeatureCheck_MissingBuild{ void Initialise(); void SetupRenderGraph(Flux_RenderGraph&); void Shutdown(); };  // missing BuildPipelines
	struct FeatureCheck_NoInit      { void SetupRenderGraph(Flux_RenderGraph&); void Shutdown(); void BuildPipelines(); }; // missing Initialise
	static_assert( FluxRenderFeature<FeatureCheck_Full>,         "a feature implementing all four methods must satisfy FluxRenderFeature");
	static_assert(!FluxRenderFeature<FeatureCheck_MissingBuild>, "a feature missing BuildPipelines must be rejected — add a (no-op) stub");
	static_assert(!FluxRenderFeature<FeatureCheck_NoInit>,       "a feature missing Initialise must be rejected — add a (no-op) stub");
}

// The ONE way every Flux feature is registered. Wires all four lifecycle
// trampolines (every feature implements all four — a no-op where it has nothing to
// do) and auto-appends the feature's setup step to the render-graph walk. If a
// render system does not fit this form, fix the render system (give it the missing
// method, a no-op stub if there's nothing to do) — do not hand-roll an
// xReg.Register(...) here. Constrained on FluxRenderFeature so a feature missing any
// method fails to compile HERE (with the concept name) instead of silently
// registering a feature that drops part of its lifecycle at runtime.
// Shader-owning features: pass the feature's apxALL (array bound N deduced). The
// decls are static-lifetime (inline constexpr in the per-feature _Shaders.h), so
// storing the array pointer + N in the desc is safe for the registry's lifetime.
template<auto pfnAccessor, u_int N>
	requires FluxRenderFeature<std::remove_cvref_t<decltype((g_xEngine.*pfnAccessor)())>>
static void RegisterFeature(Flux_FeatureRegistry& xReg, const char* szName,
							const Flux_ShaderDecl* const (&axShaders)[N])
{
	xReg.Register(szName,
		&FluxFeatureInitialise<pfnAccessor>,
		&FluxFeatureSetup<pfnAccessor>,
		&FluxFeatureShutdown<pfnAccessor>,
		&FluxFeatureBuildPipelines<pfnAccessor>,
		axShaders, N);
}

// No-shader features (FluxGraphics / Shadows / DynamicLights own no pipelines).
template<auto pfnAccessor>
	requires FluxRenderFeature<std::remove_cvref_t<decltype((g_xEngine.*pfnAccessor)())>>
static void RegisterFeature(Flux_FeatureRegistry& xReg, const char* szName)
{
	xReg.Register(szName,
		&FluxFeatureInitialise<pfnAccessor>,
		&FluxFeatureSetup<pfnAccessor>,
		&FluxFeatureShutdown<pfnAccessor>,
		&FluxFeatureBuildPipelines<pfnAccessor>,
		nullptr, 0);
}

void Flux_FeatureRegistry::RegisterDefaultFeatures()
{
	// Engine wrapper: reset the singleton (the main-thread assert lives in Reset)
	// then populate it. The feature list lives in RegisterDefaultFeaturesInto so
	// tooling can build a snapshot WITHOUT Reset()'s g_xEngine threading assert.
	// Idempotent: headless boots skip LateInitialise and unit tests re-init Flux
	// in-process — Reset() returns to a known-empty table on every entry.
	Get().Reset();
	RegisterDefaultFeaturesInto(Get());
}

Flux_FeatureRegistry Flux_FeatureRegistry::CreateDefaultSnapshotForValidation()
{
	// No Reset(), no g_xEngine deref — the RegisterFeature<&Engine::X> trampolines
	// reference g_xEngine only when CALLED, never during registration. Returned by
	// value (trivially copyable: fixed arrays + counts, no owning pointers).
	Flux_FeatureRegistry xSnapshot;
	RegisterDefaultFeaturesInto(xSnapshot);
	return xSnapshot;
}

void Flux_FeatureRegistry::RegisterDefaultFeaturesInto(Flux_FeatureRegistry& xReg)
{

	// ONE ordered list, in render-graph DECLARATION order. Each feature is added
	// with a single RegisterFeature<&Zenith_Engine::X> call that wires init
	// (forward) + setup (auto-appended here) + shutdown (reverse) + hot-reload. The
	// handful of non-feature "irregular" setup steps are interleaved with
	// AddSetupStep at their render-graph position. Init and shutdown are safe in
	// this order because NO feature's Initialise/Shutdown reads another feature
	// (verified — only foundation + own state); only the render-graph
	// producer-before-consumer order is load-bearing, and THIS is it (see the
	// ORDERING note in the header). Game render features anchored runAfter="Fog"
	// are interleaved by RunSetup right after the Fog step.

	// FluxGraphics is the foundation (quad mesh, frame constants, G-buffer formats,
	// fallback assets) AND the owner of every shared cross-feature render-graph
	// transient (G-buffer / depth / final-RT / HDR scene). Registered FIRST so its
	// SetupRenderGraph — which creates those transients — runs before any feature
	// declares a pass on them. (This replaced the former @SetupTransients:FluxGraphics
	// and @SetupTransients:HDR raw steps.) BuildPipelines is a no-op (owns no shaders).
	RegisterFeature<&Zenith_Engine::FluxGraphics>(xReg, "FluxGraphics");

	RegisterFeature<&Zenith_Engine::IBL>(xReg, "IBL", Flux_IBLShaders::apxALL);
	RegisterFeature<&Zenith_Engine::Shadows>(xReg, "Shadows");
	RegisterFeature<&Zenith_Engine::StaticMeshes>(xReg, "StaticMeshes", Flux_StaticMeshesShaders::apxALL);
	RegisterFeature<&Zenith_Engine::Terrain>(xReg, "Terrain", Flux_TerrainShaders::apxALL);
	RegisterFeature<&Zenith_Engine::Primitives>(xReg, "Primitives", Flux_PrimitivesShaders::apxALL);
	RegisterFeature<&Zenith_Engine::AnimatedMeshes>(xReg, "AnimatedMeshes", Flux_AnimatedMeshesShaders::apxALL);
	RegisterFeature<&Zenith_Engine::InstancedMeshes>(xReg, "InstancedMeshes", Flux_InstancedMeshesShaders::apxALL);
	// Skybox renders AFTER all opaque G-buffer writers (above) so its fullscreen
	// atmosphere/cubemap draw depth-TESTS against scene depth and only shades pixels
	// where sky is actually visible (depth still at the far-cleared 1.0), instead of
	// shading 100% of the screen first and being overdrawn by geometry. It still
	// owns the G-buffer clear request; the render graph floats the actual clear up
	// to the first opaque writer (and the skybox clears itself in a no-geometry
	// scene). Declared BEFORE Decals so the sky is an earlier producer for Decals'
	// depth/normals reads and the depth attachment doesn't ping-pong layouts.
	RegisterFeature<&Zenith_Engine::Skybox>(xReg, "Skybox", Flux_SkyboxShaders::apxALL);
	RegisterFeature<&Zenith_Engine::Decals>(xReg, "Decals", Flux_DecalsShaders::apxALL);
	RegisterFeature<&Zenith_Engine::HiZ>(xReg, "HiZ", Flux_HiZShaders::apxALL);
	RegisterFeature<&Zenith_Engine::SSR>(xReg, "SSR", Flux_SSRShaders::apxALL);
	RegisterFeature<&Zenith_Engine::SSGI>(xReg, "SSGI", Flux_SSGIShaders::apxALL);
	// SSAO feeds the DeferredShading ambient term (it no longer composites
	// post-lighting), so it must declare BEFORE DeferredShading: its transient
	// handles must exist when DeferredShading's setup reads them. Natural home
	// alongside HiZ/SSR/SSGI.
	RegisterFeature<&Zenith_Engine::SSAO>(xReg, "SSAO", Flux_SSAOShaders::apxALL);
	// DynamicLights is a gather/upload front-end — no graph passes and no pipelines of
	// its own (the LightClustering feature owns the clustering compute), so its
	// SetupRenderGraph/BuildPipelines are no-op stubs (its no-op setup adds nothing to
	// the walk). Declared next to its consumer LightClustering for readability.
	RegisterFeature<&Zenith_Engine::DynamicLights>(xReg, "DynamicLights");
	RegisterFeature<&Zenith_Engine::LightClustering>(xReg, "LightClustering", Flux_LightClusteringShaders::apxALL);
	RegisterFeature<&Zenith_Engine::DeferredShading>(xReg, "DeferredShading", Flux_DeferredShadingShaders::apxALL);
	// Grass is a FORWARD pass over the lit HDR scene (depth-tested, read-only depth)
	// — it must declare AFTER DeferredShading, whose pass CLEARS the HDR target
	// (declaring Grass earlier put its output before the clear, wiped every frame).
	// Before Fog/Particles so atmosphere + effects composite over the blades.
	RegisterFeature<&Zenith_Engine::Grass>(xReg, "Grass", Flux_GrassShaders::apxALL);
	// Forward translucency after lighting and before Fog: glass is lit in its own
	// forward pass and fog must composite over it.
	RegisterFeature<&Zenith_Engine::Translucency>(xReg, "Translucency", Flux_TranslucencyShaders::apxALL);
	// Fog is an orchestrator: all fog .slang programs share the "Fog" subsystem
	// grouping, so hot-reload auto-wires them to Flux_FogImpl::BuildPipelines, which
	// rebuilds every technique (Simple + GodRays + Raymarch + Froxel). It is RAII, so
	// its Shutdown is a no-op stub.
	RegisterFeature<&Zenith_Engine::Fog>(xReg, "Fog", Flux_FogShaders::apxALL);
	RegisterFeature<&Zenith_Engine::SDFs>(xReg, "SDFs", Flux_SDFsShaders::apxALL);
	RegisterFeature<&Zenith_Engine::Particles>(xReg, "Particles", Flux_ParticlesShaders::apxALL);
	// HDR tonemap composites last (reads the fully-lit HDR scene + bloom). The shared
	// HDR scene target it reads is created/owned by FluxGraphics (first step); HDR
	// creates only its private bloom chain in SetupRenderGraph and its histogram /
	// exposure buffers in Initialise (order-free).
	RegisterFeature<&Zenith_Engine::HDR>(xReg, "HDR", Flux_HDRShaders::apxALL);
	RegisterFeature<&Zenith_Engine::Quads>(xReg, "Quads", Flux_QuadsShaders::apxALL);
	RegisterFeature<&Zenith_Engine::Text>(xReg, "Text", Flux_TextShaders::apxALL);
#ifdef ZENITH_TOOLS
	RegisterFeature<&Zenith_Engine::Gizmos>(xReg, "Gizmos", Flux_GizmosShaders::apxALL);
	// Material-preview offscreen passes last — they own persistent targets and
	// early-out when the editor panel is closed, so placement is cosmetic.
	RegisterFeature<&Zenith_Engine::MaterialPreview>(xReg, "MaterialPreview", Flux_MaterialPreviewShaders::apxALL);
#endif
	// Present is the LAST feature: its SetupRenderGraph adds the final-RT
	// layout-transition reader (subsuming the former @FinalRTLayoutTransition raw
	// step), and it owns the backend-neutral present blit pipeline the swapchain
	// uses to copy the Final RT to the backbuffer. Declared after every Final-RT
	// writer (tonemap/quads/text/gizmos) so its reader sorts strictly last.
	RegisterFeature<&Zenith_Engine::Present>(xReg, "Present", Flux_PresentShaders::apxALL);
}
