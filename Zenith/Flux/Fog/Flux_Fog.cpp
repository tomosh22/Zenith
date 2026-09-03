#include "Zenith.h"
#include "Flux/Fog/Flux_Fog_Shaders.h"
#include "Core/Zenith_Engine.h"
#include "Profiling/Zenith_Profiling.h"

#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "Flux/Fog/Flux_GodRaysFogImpl.h"
#include "Flux/Fog/Flux_RaymarchFogImpl.h"
#include "Flux/Fog/Flux_FroxelFogImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Core/FrameContext.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Fog.h" // typed binding handles
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include <cstddef>   // offsetof — pins the fog constant-block layout below

// Render graph pass indices for dynamic enable/disable.
//
// A game disables engine fog generically via the render graph's force-disable
// overlay (xGraph.SetOwnerForceDisabled("Fog", true)), which masks all 6 fog
// passes by owner WITHOUT touching their base enable bits — so the technique
// selection below keeps running harmlessly and the engine fog returns intact the
// moment the game lifts the override. There is no longer a fog-specific override
// flag or short-circuit here.

u_int dbg_uVolFogDebugMode = 0;  // Debug visualization mode (non-static for external linkage)

// Simple-fog draw constants. Colour and falloff are SEPARATE members even though
// the shader reads them as one float4 (FogConstantsLayout::g_xFogColour_Falloff):
// they used to share a Vector4, and the debug panel registered BOTH a vec4
// "Colour" (range 0..1) and a float "Density" (range 0..0.02) over it — two
// sliders aliasing one float with a 50x range mismatch, so dragging Colour's W
// slammed density to a value Density could never express, and vice versa.
// A vec3 + float keeps the 16-byte GPU layout byte-identical while giving each
// slider its own storage.
static struct Flux_FogConstants
{
	Zenith_Maths::Vector3 m_xColour = { 0.5f, 0.6f, 0.7f };
	float m_fFalloff = 0.000025f;   // ground-level Mie EXTINCTION (1/m): the aerial-perspective haze density (~150 km visibility; 7.5e-5 was the old exponential-fog falloff)
	// Henyey-Greenstein phase function asymmetry parameter
	// g = 0.0: isotropic, g = 0.8: typical atmospheric haze, g = 0.95: Mie scattering
	float m_fPhaseG = 0.8f;
	float m_fPad[3] = { 0.f, 0.f, 0.f };
} dbg_xConstants;
// The split must not move a byte: the shader still reads {colour.rgb, falloff} as
// one float4 followed by the phase term.
static_assert(sizeof(Flux_FogConstants) == 32, "Flux_FogConstants must stay 32B to match FogConstantsLayout in Shaders/Fog/Flux_Fog.slang");
static_assert(offsetof(Flux_FogConstants, m_fFalloff) == 12, "falloff must occupy the w lane of g_xFogColour_Falloff");
static_assert(offsetof(Flux_FogConstants, m_fPhaseG) == 16, "phase G must follow the colour/falloff float4");

static void ExecuteSimpleFog(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteFroxelInject(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteFroxelLight(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteFroxelApply(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteRaymarch(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteGodRays(Flux_CommandBuffer* pxCommandList, void* pUserData);

void Flux_FogImpl::BuildPipelines()
{
	m_xShader.Initialise(Flux_FogShaders::xFog_Simple);

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	// No vertex input: this is a vertex-pulling / fullscreen program whose VS reads
	// no attributes. m_pxVertexLayout stays null, which is the canonical spelling the
	// validation tripwire matches against an empty reflection table.
	xPipelineSpec.m_eTopology = MESH_TOPOLOGY_NONE;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(m_xPipeline, xPipelineSpec);

	// Fog is an orchestrator: shader hot-reload routes EVERY "Fog"-subsystem
	// program to this one callback (all fog .slang share that grouping), so a
	// rebuild must refresh the simple-fog pipeline AND every technique. Each
	// technique's BuildPipelines is leak-safe and self-contained (its own
	// Initialise calls it first), so this is safe both at init — where Initialise
	// runs it AFTER the techniques are initialised — and on a live reload.
	auto& xEngine = g_xEngine;
	xEngine.GodRaysFog().BuildPipelines();
	xEngine.RaymarchFog().BuildPipelines();
	xEngine.FroxelFog().BuildPipelines();
}

void Flux_FogImpl::Initialise()
{
	auto& xEngine = g_xEngine;

	// Initialize shared infrastructure + every technique FIRST (each builds its
	// own pipelines + resources), THEN BuildPipelines(). BuildPipelines() now also
	// (re)builds the techniques' pipelines for hot-reload, so running it after
	// their Initialise keeps that a harmless leak-safe refresh rather than a build
	// against not-yet-initialised state.
	xEngine.VolumeFog().Initialise();

	// Initialize all volumetric fog techniques (spatial-only, no temporal).
	xEngine.GodRaysFog().Initialise();
	xEngine.RaymarchFog().Initialise();
	xEngine.FroxelFog().Initialise();

	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	// Bound from the enum, not a literal: the slider used to stop at 23 while
	// VolumetricFogDebugMode only defines 0..VOLFOG_DEBUG_MAX-1, so its top eight
	// positions selected nothing.
	xEngine.DebugVariables().AddUInt32({ "Render", "Volumetric Fog", "Debug Mode" }, dbg_uVolFogDebugMode, 0, VOLFOG_DEBUG_MAX - 1);
	xEngine.DebugVariables().AddVector3({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour, 0., 1.);
	xEngine.DebugVariables().AddFloat({ "Render", "Fog", "Density" }, dbg_xConstants.m_fFalloff, 0., 0.02);
	xEngine.DebugVariables().AddFloat({ "Render", "Fog", "Phase G" }, dbg_xConstants.m_fPhaseG, -0.99f, 0.99f);
#endif

	// Note: Fog ambient irradiance ratio defaults to 0.25 (FOG_AMBIENT_IRRADIANCE_RATIO_DEFAULT
	// in Common/Volumetric.slang); the froxel/raymarch passes already take it as a runtime CB
	// field (u_fAmbientIrradianceRatio).
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Fog initialised (4 spatial-only techniques: Simple, Froxel, Raymarch, GodRays)");
}

void Flux_FogImpl::Reset()
{
	auto& xEngine = g_xEngine;
	// Reset all volumetric fog techniques (spatial-only, no temporal)
	xEngine.VolumeFog().Reset();
	xEngine.GodRaysFog().Reset();
	xEngine.RaymarchFog().Reset();
	xEngine.FroxelFog().Reset();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_FogImpl::Reset() - Reset all fog systems");
}

void Flux_FogImpl::ApplyTechniqueSelectionToGraph(Flux_RenderGraph& xGraph)
{
	ZENITH_PROFILE_SCOPE("Flux Fog Technique Selection");
	// Always keep the per-pass BASE enable bits in sync with the active technique.
	// If a game force-disables owner "Fog" via the render graph overlay, these
	// passes are masked regardless of their base bit; when the game lifts the
	// override the base state (kept current here every frame) re-enables exactly
	// the active technique's passes — so there is no cached-technique staleness to
	// go wrong, and no override-awareness needed here.
	const u_int uTechnique = Zenith_GraphicsOptions::Get().m_uVolFogTechnique;
	if (uTechnique == m_uLastFogTechnique)
		return;

	m_uLastFogTechnique = uTechnique;

	xGraph.SetEnabled(m_xSimpleFogPass, uTechnique == 0);
	xGraph.SetEnabled(m_xFroxelInjectPass, uTechnique == 1);
	xGraph.SetEnabled(m_xFroxelLightPass, uTechnique == 1);
	xGraph.SetEnabled(m_xFroxelApplyPass, uTechnique == 1);
	xGraph.SetEnabled(m_xRaymarchPass, uTechnique == 2);
	xGraph.SetEnabled(m_xGodRaysPass, uTechnique == 3);

	// Force a full recompile. SetPassEnabled's cheap m_bEnabledMaskDirty path
	// only re-resolves per-target-setup clear ownership; it does NOT rebuild
	// the topological execution order or regenerate prologue barriers.
	//
	// This matters because TopologicalSort() (Flux_RenderGraph.cpp) excludes
	// disabled passes from the topo order. A pass that was disabled at the
	// last Compile is *not in the execution order at all* — merely flipping
	// its m_bEnabled bit does not put it back. Without a full recompile, the
	// newly-enabled technique's passes would simply never run, and the
	// previously-enabled technique's passes would stop being submitted — with
	// their side-effect barrier transitions (e.g. depth WRITE_DSV→READ_SRV)
	// disappearing along with them, leaving downstream passes to read a
	// depth buffer that is still in DEPTH_STENCIL_ATTACHMENT_OPTIMAL when the
	// render pass it begins expects DEPTH_STENCIL_READ_ONLY_OPTIMAL.
	xGraph.MarkDirty();
}

static void ExecuteSimpleFog(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	// Per-view parity: fog is scene-derived and the preview view's flags carry no
	// FLUX_VIEW_FLAG_SCENE_CONTENT, so only the main view ever renders fog — the
	// "Fog_Simple (Preview)" instance exists for structural parity and records
	// nothing.
	if (Flux_RenderGraph::GetCurrentRecordingPassViewSlot() != kuFluxViewSlotMain)
	{
		return;
	}
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}

	// Trampoline: recover the subsystem singleton; sibling deps via g_xEngine.
	auto& xEngine = g_xEngine;
	Flux_FogImpl& xFog = xEngine.Fog();
	Flux_GraphicsImpl& xGfx = xEngine.FluxGraphics();

	pxCommandList->SetPipeline(&xFog.m_xPipeline);

	pxCommandList->SetIndexBuffer(xGfx.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	namespace FS = Flux_Generated_Fog::Fog_Simple;
	xBinder.BindSRV(FS::hg_xDepthTex, xGfx.GetDepthStencilSRV());
	xBinder.BindDrawConstants(FS::hFogConstants, &dbg_xConstants, sizeof(Flux_FogConstants));

	pxCommandList->DrawIndexed(6);
}

static void ExecuteFroxelInject(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.FroxelFog().RenderInject(pxCommandList);
}

static void ExecuteFroxelLight(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.FroxelFog().RenderLight(pxCommandList);
}

static void ExecuteFroxelApply(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.FroxelFog().RenderApply(pxCommandList);
}

static void ExecuteRaymarch(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.RaymarchFog().Render(pxCommandList);
}

static void ExecuteGodRays(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.GodRaysFog().Render(pxCommandList);
}

void Flux_FogImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// All fog technique passes are registered, but only the active technique's
	// passes are enabled. ApplyTechniqueSelectionToGraph (called every frame
	// from Zenith_Core::ExecuteRenderGraph BEFORE Compile) handles dynamic
	// switching by toggling per-pass enables and calling MarkDirty() to force a
	// full barrier recompute. It cannot live as a pass OnPrepare callback
	// because Phase 0 only fires OnPrepare for *enabled* passes -- once the
	// previously-active technique is disabled, an OnPrepare-based switcher
	// would never run again and the user could never switch back.
	m_uLastFogTechnique = UINT32_MAX; // Force initial enable/disable

	auto& xEngine = g_xEngine;
	Flux_FroxelFogImpl& xFroxelFog = xEngine.FroxelFog();
	Flux_GraphicsImpl&  xGraphics  = xEngine.FluxGraphics();

	// Let FroxelFog create its transient resources (must happen before pass registration)
	xFroxelFog.SetupTransients(xGraph);

	// All technique passes are registered up front; ApplyTechniqueSelectionToGraph
	// toggles their enable bits each frame based on dbg_uVolFogTechnique and the
	// stored handles (s_x…Pass). Handles captured via the builder's implicit
	// Flux_PassHandle conversion.

	m_xSimpleFogPass = xGraph.AddPass("Fog_Simple", ExecuteSimpleFog)
		.Writes(xGraphics.GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (xGraphics.GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	m_xFroxelInjectPass = xGraph.AddPass("Fog_FroxelInject", ExecuteFroxelInject)
		.WritesTransient(xFroxelFog.GetDensityGridHandle(), RESOURCE_ACCESS_WRITE_UAV);

	// Light shader writes both lighting and scattering grids (see the two
	// UAV binding points in Flux_FroxelFog.cpp).
	m_xFroxelLightPass = xGraph.AddPass("Fog_FroxelLight", ExecuteFroxelLight)
		.ReadsTransient (xFroxelFog.GetDensityGridHandle(),    RESOURCE_ACCESS_READ_SRV)
		// Samples the CSM array for volumetric shadows — declare a full-array read so
		// the graph orders this (compute) pass after the cascade writers with the
		// WRITE_DSV → SHADER_READ barrier (Phase 4b: was implicit before).
		.ReadsTransient (g_xEngine.Shadows().GetCSMArrayHandle(), RESOURCE_ACCESS_READ_SRV, 0, 1, 0, FLUX_RG_ALL_LAYERS)
		.WritesTransient(xFroxelFog.GetLightingGridHandle(),   RESOURCE_ACCESS_WRITE_UAV)
		.WritesTransient(xFroxelFog.GetScatteringGridHandle(), RESOURCE_ACCESS_WRITE_UAV);

	// Apply shader samples both lighting and scattering grids — both must
	// be declared so the graph transitions them out of GENERAL before the
	// SRV bind.
	m_xFroxelApplyPass = xGraph.AddPass("Fog_FroxelApply", ExecuteFroxelApply)
		.Writes        (xGraphics.GetHDRSceneTarget(),               RESOURCE_ACCESS_WRITE_RTV)
		.Reads         (xGraphics.GetDepthAttachment(),         RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(xFroxelFog.GetLightingGridHandle(),     RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(xFroxelFog.GetScatteringGridHandle(),   RESOURCE_ACCESS_READ_SRV);

	m_xRaymarchPass = xGraph.AddPass("Fog_Raymarch", ExecuteRaymarch)
		.Writes(xGraphics.GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (xGraphics.GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
		// Samples the CSM array for volumetric shadows (Phase 4b full-array read).
		.ReadsTransient(g_xEngine.Shadows().GetCSMArrayHandle(), RESOURCE_ACCESS_READ_SRV, 0, 1, 0, FLUX_RG_ALL_LAYERS);

	m_xGodRaysPass = xGraph.AddPass("Fog_GodRays", ExecuteGodRays)
		.Writes(xGraphics.GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (xGraphics.GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	// Preview view (S5c): a structural-parity instance of the simple-fog pass
	// ONLY — the froxel 3D volumes / raymarch / god-rays are deliberately NOT
	// duplicated (their grid memory and cost are unjustified for a 512²
	// single-mesh preview, which never wants volumetrics). Fog is scene-derived
	// and the preview view's flags carry no FLUX_VIEW_FLAG_SCENE_CONTENT, so
	// ExecuteSimpleFog early-outs for non-main views and this pass renders
	// nothing. No ClearTargets — the preview HDR clear is owned by
	// "Apply Lighting (Preview)" and a declared clear here would clobber the
	// lit result (declared clears still run for passes that record nothing).
	if (xGraphics.RenderViews().IsViewActive(kuFluxViewSlotPreview))
	{
		xGraph.AddPass("Fog_Simple (Preview)", ExecuteSimpleFog)
			.View  (kuFluxViewSlotPreview)
			.Writes(xGraphics.GetHDRSceneTarget(kuFluxViewSlotPreview),  RESOURCE_ACCESS_WRITE_RTV)
			.Reads (xGraphics.GetDepthAttachment(kuFluxViewSlotPreview), RESOURCE_ACCESS_READ_SRV);
	}

	// A game that overrides fog force-disables owner "Fog" on the graph; that
	// overlay persists across graph rebuilds (it is NOT cleared by Clear()), so a
	// RequestGraphRebuild() while a game holds the override automatically re-masks
	// these freshly-rebuilt passes — no fog-specific re-apply needed here.
}
