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
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Render graph pass indices for dynamic enable/disable.
//
// A game disables engine fog generically via the render graph's force-disable
// overlay (xGraph.SetOwnerForceDisabled("Fog", true)), which masks all 6 fog
// passes by owner WITHOUT touching their base enable bits — so the technique
// selection below keeps running harmlessly and the engine fog returns intact the
// moment the game lifts the override. There is no longer a fog-specific override
// flag or short-circuit here.

u_int dbg_uVolFogDebugMode = 0;  // Debug visualization mode (non-static for external linkage)

static struct Flux_FogConstants
{
	Zenith_Maths::Vector4 m_xColour_Falloff = { 0.5,0.6,0.7,0.000075 };
	// Henyey-Greenstein phase function asymmetry parameter
	// g = 0.0: isotropic, g = 0.8: typical atmospheric haze, g = 0.95: Mie scattering
	float m_fPhaseG = 0.8f;
	float m_fPad[3] = { 0.f, 0.f, 0.f };
} dbg_xConstants;

static void ExecuteSimpleFog(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteFroxelInject(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteFroxelLight(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteFroxelApply(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteRaymarch(Flux_CommandBuffer* pxCommandList, void* pUserData);
static void ExecuteGodRays(Flux_CommandBuffer* pxCommandList, void* pUserData);

void Flux_FogImpl::BuildPipelines()
{
	m_xShader.Initialise(Flux_FogShaders::xFog_Simple);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

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
	g_xEngine.GodRaysFog().BuildPipelines();
	g_xEngine.RaymarchFog().BuildPipelines();
	g_xEngine.FroxelFog().BuildPipelines();
}

void Flux_FogImpl::Initialise()
{
	// Initialize shared infrastructure + every technique FIRST (each builds its
	// own pipelines + resources), THEN BuildPipelines(). BuildPipelines() now also
	// (re)builds the techniques' pipelines for hot-reload, so running it after
	// their Initialise keeps that a harmless leak-safe refresh rather than a build
	// against not-yet-initialised state.
	g_xEngine.VolumeFog().Initialise();

	// Initialize all volumetric fog techniques (spatial-only, no temporal).
	g_xEngine.GodRaysFog().Initialise();
	g_xEngine.RaymarchFog().Initialise();
	g_xEngine.FroxelFog().Initialise();

	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Volumetric Fog", "Debug Mode" }, dbg_uVolFogDebugMode, 0, 23);
	g_xEngine.DebugVariables().AddVector4({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour_Falloff, 0., 1.);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Fog", "Density" }, dbg_xConstants.m_xColour_Falloff.w, 0., 0.02);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Fog", "Phase G" }, dbg_xConstants.m_fPhaseG, -0.99f, 0.99f);
#endif

	// Note: Fog ambient irradiance ratio is unified at 0.25 in Flux_VolumetricCommon.fxh
	// To make it runtime-adjustable, add to Flux_VolumeFogConstants and pass through uniform buffers
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Fog initialised (4 spatial-only techniques: Simple, Froxel, Raymarch, GodRays)");
}

void Flux_FogImpl::Reset()
{
	// Reset all volumetric fog techniques (spatial-only, no temporal)
	g_xEngine.VolumeFog().Reset();
	g_xEngine.GodRaysFog().Reset();
	g_xEngine.RaymarchFog().Reset();
	g_xEngine.FroxelFog().Reset();

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
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}

	// Trampoline: recover the subsystem singleton; sibling deps via g_xEngine.
	Flux_FogImpl& xFog = g_xEngine.Fog();
	Flux_GraphicsImpl& xGfx = g_xEngine.FluxGraphics();

	pxCommandList->SetPipeline(&xFog.m_xPipeline);

	pxCommandList->SetVertexBuffer(xGfx.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGfx.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(xFog.m_xShader, "FrameConstants", &xGfx.m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(xFog.m_xShader, "g_xDepthTex", xGfx.GetDepthStencilSRV());
	xBinder.BindDrawConstants(xFog.m_xShader, "FogConstants", &dbg_xConstants, sizeof(Flux_FogConstants));

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

	Flux_FroxelFogImpl& xFroxelFog = g_xEngine.FroxelFog();
	Flux_GraphicsImpl&  xGraphics  = g_xEngine.FluxGraphics();

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
		.Reads (xGraphics.GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	m_xGodRaysPass = xGraph.AddPass("Fog_GodRays", ExecuteGodRays)
		.Writes(xGraphics.GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (xGraphics.GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	// A game that overrides fog force-disables owner "Fog" on the graph; that
	// overlay persists across graph rebuilds (it is NOT cleared by Clear()), so a
	// RequestGraphRebuild() while a game holds the override automatically re-masks
	// these freshly-rebuilt passes — no fog-specific re-apply needed here.
}
