#include "Zenith.h"

#include "Flux/SDFs/Flux_SDFsImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7b: state on Flux_SDFsImpl held by Zenith_Engine.

static constexpr uint32_t s_uMaxSpheres = 1000;
struct Sphere
{
	Zenith_Maths::Vector4 m_xPosition_Radius;
	Zenith_Maths::Vector4 m_xColour;
};
struct SphereData
{
	uint32_t m_uNumSpheres;
	uint32_t m_auPad[7];
	Sphere m_axSpheres[s_uMaxSpheres];
} s_axSphereData;


static void ExecuteSDFs(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_SDFsImpl::BuildPipelines()
{
	g_xEngine.SDFs().m_xShader.Initialise(FluxShaderProgram::SDFs);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xPipelineSpec.m_pxShader = &g_xEngine.SDFs().m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	g_xEngine.SDFs().m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;

	Flux_PipelineBuilder::FromSpecification(g_xEngine.SDFs().m_xPipeline, xPipelineSpec);
}

void Flux_SDFsImpl::Initialise()
{
	BuildPipelines();

	g_xEngine.VulkanMemory().InitialiseDynamicConstantBuffer(&s_axSphereData, sizeof(s_axSphereData), g_xEngine.SDFs().m_xSpheresBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::SDFs,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.SDFs().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SDFs initialised");
}

void Flux_SDFsImpl::Shutdown()
{
	g_xEngine.VulkanMemory().DestroyDynamicConstantBuffer(g_xEngine.SDFs().m_xSpheresBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SDFs shut down");
}

void UploadSpheres()
{
	s_axSphereData.m_uNumSpheres = 2;

	{
		Sphere& xSphere = s_axSphereData.m_axSpheres[0];
		xSphere.m_xPosition_Radius = Zenith_Maths::Vector4(2000, 1500 + sin(g_xEngine.Frame().GetTimePassed()) * 200, 2000, 100);
		xSphere.m_xColour = Zenith_Maths::Vector4(1., 0., 0., 1.);
	}
	{
		Sphere& xSphere = s_axSphereData.m_axSpheres[1];
		xSphere.m_xPosition_Radius = Zenith_Maths::Vector4(2000, 1500 + cos(g_xEngine.Frame().GetTimePassed()) * 200, 2000, 100);
		xSphere.m_xColour = Zenith_Maths::Vector4(0., 1., 0., 1.);
	}

	g_xEngine.VulkanMemory().UploadBufferData(g_xEngine.SDFs().m_xSpheresBuffer.GetBuffer().m_xVRAMHandle, &s_axSphereData, sizeof(s_axSphereData));
}

void Flux_SDFsImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSDFsEnabled)
	{
		return;
	}

	UploadSpheres();
}

static void ExecuteSDFs(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bSDFsEnabled)
	{
		return;
	}

	UploadSpheres();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.SDFs().m_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
	pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV(), 0);
	pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.SDFs().m_xSpheresBuffer.GetCBV(), 1);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_SDFsImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// The pipeline uses default depth-test+write enabled, so the depth attachment
	// is bound as a writable DSV for the renderpass.
	xGraph.AddPass("SDFs", ExecuteSDFs)
		.Writes(g_xEngine.HDR().GetHDRSceneTarget(),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(g_xEngine.FluxGraphics().GetDepthAttachment(),  RESOURCE_ACCESS_WRITE_DSV);
}
