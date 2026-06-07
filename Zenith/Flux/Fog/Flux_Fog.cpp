#include "Zenith.h"
#include "Core/Zenith_Engine.h"

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
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Render graph pass indices for dynamic enable/disable

// Game-side override flag (EXT-1). When true, all 6 fog passes are explicitly
// disabled on the active render graph and ApplyTechniqueSelectionToGraph
// short-circuits.
static bool s_bExternallyOverridden = false;

void Flux_FogImpl::DisableAllFogPasses(Flux_RenderGraph& xGraph)
{
	xGraph.SetEnabled(m_xSimpleFogPass,        false);
	xGraph.SetEnabled(m_xFroxelInjectPass,     false);
	xGraph.SetEnabled(m_xFroxelLightPass,      false);
	xGraph.SetEnabled(m_xFroxelApplyPass,      false);
	xGraph.SetEnabled(m_xRaymarchPass,         false);
	xGraph.SetEnabled(m_xGodRaysPass,          false);
}


u_int dbg_uVolFogDebugMode = 0;  // Debug visualization mode (non-static for external linkage)

static struct Flux_FogConstants
{
	Zenith_Maths::Vector4 m_xColour_Falloff = { 0.5,0.6,0.7,0.000075 };
	// Henyey-Greenstein phase function asymmetry parameter
	// g = 0.0: isotropic, g = 0.8: typical atmospheric haze, g = 0.95: Mie scattering
	float m_fPhaseG = 0.8f;
	float m_fPad[3] = { 0.f, 0.f, 0.f };
} dbg_xConstants;

static void ExecuteSimpleFog(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteFroxelInject(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteFroxelLight(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteFroxelApply(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteRaymarch(Flux_CommandList* pxCommandList, void* pUserData);
static void ExecuteGodRays(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_FogImpl::BuildPipelines()
{
	m_xShader.Initialise(FluxShaderProgram::Fog_Simple);

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
}

void Flux_FogImpl::Initialise(Flux_VolumeFogImpl& xVolumeFog, Flux_GodRaysFogImpl& xGodRaysFog,
	Flux_RaymarchFogImpl& xRaymarchFog, Flux_FroxelFogImpl& xFroxelFog,
	Flux_HDRImpl& xHDR, Flux_GraphicsImpl& xFluxGraphics, Flux_RendererImpl& xFluxRenderer,
	Flux_ShadowsImpl& xShadows, FrameContext& xFrame)
{
	m_pxVolumeFog    = &xVolumeFog;
	m_pxGodRaysFog   = &xGodRaysFog;
	m_pxRaymarchFog  = &xRaymarchFog;
	m_pxFroxelFog    = &xFroxelFog;
	m_pxHDR          = &xHDR;
	m_pxFluxGraphics = &xFluxGraphics;
	m_pxFluxRenderer = &xFluxRenderer;
	m_pxShadows      = &xShadows;
	m_pxFrame        = &xFrame;

	BuildPipelines();

	// Initialize shared volumetric fog infrastructure
	m_pxVolumeFog->Initialise();

	// Initialize all volumetric fog techniques (spatial-only, no temporal). Each
	// technique's cross-subsystem deps are threaded through from Fog's own injected
	// members (Wave-4 de-globalization) so the techniques carry no g_xEngine reach.
	m_pxGodRaysFog->Initialise(*m_pxFluxGraphics);
	m_pxRaymarchFog->Initialise(*m_pxVolumeFog, *m_pxFrame, *m_pxFluxRenderer, *m_pxFluxGraphics, *m_pxShadows);
	m_pxFroxelFog->Initialise(*m_pxVolumeFog, *m_pxFluxRenderer, *m_pxFluxGraphics, *m_pxShadows);

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Fog_Simple,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Fog().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Volumetric Fog", "Debug Mode" }, dbg_uVolFogDebugMode, 0, 23);
	g_xEngine.DebugVariables().AddVector3({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour_Falloff, 0., 1.);
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
	m_pxVolumeFog->Reset();
	m_pxGodRaysFog->Reset();
	m_pxRaymarchFog->Reset();
	m_pxFroxelFog->Reset();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_FogImpl::Reset() - Reset all fog systems");
}

void Flux_FogImpl::ApplyTechniqueSelectionToGraph(Flux_RenderGraph& xGraph)
{
	// EXT-1: when a game has taken over fog rendering via SetExternallyOverridden,
	// short-circuit completely — DON'T re-toggle SetEnabled per frame, otherwise
	// the per-frame technique cache would re-enable engine fog passes the next
	// time the cached technique compares unequal.
	if (s_bExternallyOverridden)
		return;

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

static void ExecuteSimpleFog(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}

	// Trampoline: recover the subsystem singleton, then route through its members.
	Flux_FogImpl& xFog = g_xEngine.Fog();
	Flux_GraphicsImpl& xGfx = *xFog.m_pxFluxGraphics;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xFog.m_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xGfx.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xGfx.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(xFog.m_xShader, "FrameConstants", &xGfx.m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(xFog.m_xShader, "g_xDepthTex", xGfx.GetDepthStencilSRV());
	xBinder.BindDrawConstants(xFog.m_xShader, "FogConstants", &dbg_xConstants, sizeof(Flux_FogConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteFroxelInject(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.Fog().m_pxFroxelFog->RenderInject(pxCommandList);
}

static void ExecuteFroxelLight(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.Fog().m_pxFroxelFog->RenderLight(pxCommandList);
}

static void ExecuteFroxelApply(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.Fog().m_pxFroxelFog->RenderApply(pxCommandList);
}

static void ExecuteRaymarch(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.Fog().m_pxRaymarchFog->Render(pxCommandList);
}

static void ExecuteGodRays(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bFogEnabled)
	{
		return;
	}
	g_xEngine.Fog().m_pxGodRaysFog->Render(pxCommandList);
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

	// Let FroxelFog create its transient resources (must happen before pass registration)
	m_pxFroxelFog->SetupTransients(xGraph);

	// All technique passes are registered up front; ApplyTechniqueSelectionToGraph
	// toggles their enable bits each frame based on dbg_uVolFogTechnique and the
	// stored handles (s_x…Pass). Handles captured via the builder's implicit
	// Flux_PassHandle conversion.

	m_xSimpleFogPass = xGraph.AddPass("Fog_Simple", ExecuteSimpleFog)
		.Writes(m_pxHDR->GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (m_pxFluxGraphics->GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	m_xFroxelInjectPass = xGraph.AddPass("Fog_FroxelInject", ExecuteFroxelInject)
		.WritesTransient(m_pxFroxelFog->GetDensityGridHandle(), RESOURCE_ACCESS_WRITE_UAV);

	// Light shader writes both lighting and scattering grids (see the two
	// UAV binding points in Flux_FroxelFog.cpp).
	m_xFroxelLightPass = xGraph.AddPass("Fog_FroxelLight", ExecuteFroxelLight)
		.ReadsTransient (m_pxFroxelFog->GetDensityGridHandle(),    RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(m_pxFroxelFog->GetLightingGridHandle(),   RESOURCE_ACCESS_WRITE_UAV)
		.WritesTransient(m_pxFroxelFog->GetScatteringGridHandle(), RESOURCE_ACCESS_WRITE_UAV);

	// Apply shader samples both lighting and scattering grids — both must
	// be declared so the graph transitions them out of GENERAL before the
	// SRV bind.
	m_xFroxelApplyPass = xGraph.AddPass("Fog_FroxelApply", ExecuteFroxelApply)
		.Writes        (m_pxHDR->GetHDRSceneTarget(),               RESOURCE_ACCESS_WRITE_RTV)
		.Reads         (m_pxFluxGraphics->GetDepthAttachment(),         RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(m_pxFroxelFog->GetLightingGridHandle(),     RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(m_pxFroxelFog->GetScatteringGridHandle(),   RESOURCE_ACCESS_READ_SRV);

	m_xRaymarchPass = xGraph.AddPass("Fog_Raymarch", ExecuteRaymarch)
		.Writes(m_pxHDR->GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (m_pxFluxGraphics->GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	m_xGodRaysPass = xGraph.AddPass("Fog_GodRays", ExecuteGodRays)
		.Writes(m_pxHDR->GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV)
		.Reads (m_pxFluxGraphics->GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	// EXT-1: re-apply game-side override after the graph has been (re)built.
	// Without this, a RequestGraphRebuild() while the flag is set would leave
	// fog passes in their default-enabled state until the next technique
	// switch. Pass handles above are freshly populated, so DisableAllFogPasses
	// is safe to call here.
	ReapplyOverrideToCurrentGraph();
}


void Flux_FogImpl::SetExternallyOverridden(bool bOverridden)
{
	s_bExternallyOverridden = bOverridden;

	// Touch the active graph immediately so the change takes effect this
	// frame, regardless of whether ApplyTechniqueSelectionToGraph runs.
	if (!m_pxFluxRenderer->IsRenderGraphValid()) return;
	Flux_RenderGraph& xGraph = m_pxFluxRenderer->GetRenderGraph();

	if (bOverridden)
	{
		DisableAllFogPasses(xGraph);
	}
	else
	{
		// Don't blanket-enable all 6 passes — let ApplyTechniqueSelectionToGraph
		// pick whichever subset matches the current technique on the next
		// frame. Invalidate the cached technique so the apply path runs.
		m_uLastFogTechnique = UINT32_MAX;
	}
	xGraph.MarkDirty();
}

void Flux_FogImpl::ReapplyOverrideToCurrentGraph()
{
	if (!s_bExternallyOverridden) return;
	if (!m_pxFluxRenderer->IsRenderGraphValid()) return;
	DisableAllFogPasses(m_pxFluxRenderer->GetRenderGraph());
}
