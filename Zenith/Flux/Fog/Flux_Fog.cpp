#include "Zenith.h"

#include "Flux/Fog/Flux_Fog.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Fog/Flux_GodRaysFog.h"
#include "Flux/Fog/Flux_RaymarchFog.h"
#include "Flux/Fog/Flux_FroxelFog.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

bool Flux_Fog::s_bEnabled = true;

// Render graph pass indices for dynamic enable/disable
static u_int s_uSimpleFogPass = UINT32_MAX;
static u_int s_uFroxelInjectPass = UINT32_MAX;
static u_int s_uFroxelLightPass = UINT32_MAX;
static u_int s_uFroxelApplyPass = UINT32_MAX;
static u_int s_uRaymarchPass = UINT32_MAX;
static u_int s_uGodRaysPass = UINT32_MAX;
static u_int s_uLastFogTechnique = UINT32_MAX;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR u_int dbg_uVolFogTechnique = 0;  // 0 = Simple, 1 = Froxel, 2 = Raymarch, 3 = God Rays
u_int dbg_uVolFogDebugMode = 0;  // Debug visualization mode (non-static for external linkage)

static struct Flux_FogConstants
{
	Zenith_Maths::Vector4 m_xColour_Falloff = { 0.5,0.6,0.7,0.000075 };
	// Henyey-Greenstein phase function asymmetry parameter
	// g = 0.0: isotropic, g = 0.8: typical atmospheric haze, g = 0.95: Mie scattering
	float m_fPhaseG = 0.8f;
	float m_fPad[3] = { 0.f, 0.f, 0.f };
} dbg_xConstants;

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xDepthBinding;

void Flux_Fog::Initialise()
{
	// Initialize simple fog shader
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_Fog.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetup();
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xReflection = s_xShader.GetReflection();
	s_xFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
	s_xDepthBinding = xReflection.GetBinding("g_xDepthTex");

	// Initialize shared volumetric fog infrastructure
	Flux_VolumeFog::Initialise();

	// Initialize all volumetric fog techniques (spatial-only, no temporal)
	Flux_GodRaysFog::Initialise();
	Flux_RaymarchFog::Initialise();
	Flux_FroxelFog::Initialise();

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Fog" }, dbg_bEnable);
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Technique" }, dbg_uVolFogTechnique, 0, 3);
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Debug Mode" }, dbg_uVolFogDebugMode, 0, 23);
	Zenith_DebugVariables::AddVector3({ "Render", "Fog", "Colour" }, dbg_xConstants.m_xColour_Falloff, 0., 1.);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Density" }, dbg_xConstants.m_xColour_Falloff.w, 0., 0.02);
	Zenith_DebugVariables::AddFloat({ "Render", "Fog", "Phase G" }, dbg_xConstants.m_fPhaseG, -0.99f, 0.99f);
#endif

	// Note: Fog ambient irradiance ratio is unified at 0.25 in Flux_VolumetricCommon.fxh
	// To make it runtime-adjustable, add to Flux_VolumeFogConstants and pass through uniform buffers
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Fog initialised (4 spatial-only techniques: Simple, Froxel, Raymarch, GodRays)");
}

void Flux_Fog::Reset()
{
	// Reset all volumetric fog techniques (spatial-only, no temporal)
	Flux_VolumeFog::Reset();
	Flux_GodRaysFog::Reset();
	Flux_RaymarchFog::Reset();
	Flux_FroxelFog::Reset();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Fog::Reset() - Reset all fog systems");
}

void Flux_Fog::ApplyTechniqueSelectionToGraph(Flux_RenderGraph& xGraph)
{
	if (dbg_uVolFogTechnique == s_uLastFogTechnique)
		return;

	s_uLastFogTechnique = dbg_uVolFogTechnique;

	xGraph.SetPassEnabled(s_uSimpleFogPass, dbg_uVolFogTechnique == 0);
	xGraph.SetPassEnabled(s_uFroxelInjectPass, dbg_uVolFogTechnique == 1);
	xGraph.SetPassEnabled(s_uFroxelLightPass, dbg_uVolFogTechnique == 1);
	xGraph.SetPassEnabled(s_uFroxelApplyPass, dbg_uVolFogTechnique == 1);
	xGraph.SetPassEnabled(s_uRaymarchPass, dbg_uVolFogTechnique == 2);
	xGraph.SetPassEnabled(s_uGodRaysPass, dbg_uVolFogTechnique == 3);

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

void Flux_Fog::ExecuteSimpleFog(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV(s_xDepthBinding, Flux_Graphics::GetDepthStencilSRV());
	xBinder.PushConstant(&dbg_xConstants, sizeof(Flux_FogConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_Fog::ExecuteFroxelInject(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}
	Flux_FroxelFog::RenderInject(pxCommandList);
}

void Flux_Fog::ExecuteFroxelLight(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}
	Flux_FroxelFog::RenderLight(pxCommandList);
}

void Flux_Fog::ExecuteFroxelApply(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}
	Flux_FroxelFog::RenderApply(pxCommandList);
}

void Flux_Fog::ExecuteRaymarch(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}
	Flux_RaymarchFog::Render(pxCommandList);
}

void Flux_Fog::ExecuteGodRays(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!dbg_bEnable || !s_bEnabled)
	{
		return;
	}
	Flux_GodRaysFog::Render(pxCommandList);
}

void Flux_Fog::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// All fog technique passes are registered, but only the active technique's
	// passes are enabled. ApplyTechniqueSelectionToGraph (called every frame
	// from Zenith_Core::ExecuteRenderGraph BEFORE Compile) handles dynamic
	// switching by toggling per-pass enables and calling MarkDirty() to force a
	// full barrier recompute. It cannot live as a pass OnPrepare callback
	// because Phase 0 only fires OnPrepare for *enabled* passes — once the
	// previously-active technique is disabled, an OnPrepare-based switcher
	// would never run again and the user could never switch back.
	s_uLastFogTechnique = UINT32_MAX; // Force initial enable/disable

	// Simple fog pass
	{
		s_uSimpleFogPass = xGraph.AddPass("Fog_Simple", ExecuteSimpleFog);
		xGraph.SetPassTargetSetup(s_uSimpleFogPass, Flux_HDR::GetHDRSceneTargetSetup());
		xGraph.PassReads(s_uSimpleFogPass, &Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		xGraph.PassWrites(s_uSimpleFogPass, &Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
	}

	// Froxel fog passes (3 sub-passes with explicit resource dependencies)
	{
		s_uFroxelInjectPass = xGraph.AddPass("Fog_FroxelInject", ExecuteFroxelInject);
		xGraph.SetPassTargetSetup(s_uFroxelInjectPass, Flux_Graphics::s_xNullTargetSetup);
		xGraph.PassWrites(s_uFroxelInjectPass, &Flux_FroxelFog::GetDensityGrid(), RESOURCE_ACCESS_WRITE_UAV);

		s_uFroxelLightPass = xGraph.AddPass("Fog_FroxelLight", ExecuteFroxelLight);
		xGraph.SetPassTargetSetup(s_uFroxelLightPass, Flux_Graphics::s_xNullTargetSetup);
		xGraph.PassReads(s_uFroxelLightPass, &Flux_FroxelFog::GetDensityGrid(), RESOURCE_ACCESS_READ_SRV);
		xGraph.PassWrites(s_uFroxelLightPass, &Flux_FroxelFog::GetLightingGrid(), RESOURCE_ACCESS_WRITE_UAV);
		// Light shader also writes the scattering grid as a UAV (see
		// Flux_FroxelFog.cpp s_xLightScatteringOutputBinding bind site).
		xGraph.PassWrites(s_uFroxelLightPass, &Flux_FroxelFog::GetScatteringGrid(), RESOURCE_ACCESS_WRITE_UAV);

		s_uFroxelApplyPass = xGraph.AddPass("Fog_FroxelApply", ExecuteFroxelApply);
		xGraph.SetPassTargetSetup(s_uFroxelApplyPass, Flux_HDR::GetHDRSceneTargetSetup());
		xGraph.PassReads(s_uFroxelApplyPass, &Flux_FroxelFog::GetLightingGrid(), RESOURCE_ACCESS_READ_SRV);
		// Apply shader also samples the scattering grid alongside the lighting
		// grid — both must be declared so the graph transitions them out of
		// VK_IMAGE_LAYOUT_GENERAL (post-UAV-write) before the SRV bind.
		xGraph.PassReads(s_uFroxelApplyPass, &Flux_FroxelFog::GetScatteringGrid(), RESOURCE_ACCESS_READ_SRV);
		xGraph.PassReads(s_uFroxelApplyPass, &Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		xGraph.PassWrites(s_uFroxelApplyPass, &Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
	}

	// Raymarch fog pass
	{
		s_uRaymarchPass = xGraph.AddPass("Fog_Raymarch", ExecuteRaymarch);
		xGraph.SetPassTargetSetup(s_uRaymarchPass, Flux_HDR::GetHDRSceneTargetSetup());
		xGraph.PassReads(s_uRaymarchPass, &Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		xGraph.PassWrites(s_uRaymarchPass, &Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
	}

	// God rays fog pass
	{
		s_uGodRaysPass = xGraph.AddPass("Fog_GodRays", ExecuteGodRays);
		xGraph.SetPassTargetSetup(s_uGodRaysPass, Flux_HDR::GetHDRSceneTargetSetup());
		xGraph.PassReads(s_uGodRaysPass, &Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		xGraph.PassWrites(s_uGodRaysPass, &Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
	}
}
