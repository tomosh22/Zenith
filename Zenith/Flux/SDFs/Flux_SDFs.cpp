#include "Zenith.h"

#include "Flux/SDFs/Flux_SDFs.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/HDR/Flux_HDR.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_SDFS, Flux_SDFs::Render, nullptr);

static Flux_CommandList g_xCommandList("SDFs");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static constexpr uint32_t s_uMaxSpheres = 1000;
static Flux_DynamicConstantBuffer s_xSpheresBuffer;
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

DEBUGVAR bool dbg_bEnable = true;

void Flux_SDFs::Initialise()
{
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "SDFs/Flux_SDFs.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetupWithDepth();
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_axSphereData, sizeof(s_axSphereData), s_xSpheresBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "SDFs" }, dbg_bEnable);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SDFs initialised");
}

void Flux_SDFs::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SDFs::Reset() - Reset command list");
}

void Flux_SDFs::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xSpheresBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SDFs shut down");
}

void UploadSpheres()
{
	s_axSphereData.m_uNumSpheres = 2;

	{
		Sphere& xSphere = s_axSphereData.m_axSpheres[0];
		xSphere.m_xPosition_Radius = Zenith_Maths::Vector4(2000, 1500 + sin(Zenith_Core::GetTimePassed()) * 200, 2000, 100);
		xSphere.m_xColour = Zenith_Maths::Vector4(1., 0., 0., 1.);
	}
	{
		Sphere& xSphere = s_axSphereData.m_axSpheres[1];
		xSphere.m_xPosition_Radius = Zenith_Maths::Vector4(2000, 1500 + cos(Zenith_Core::GetTimePassed()) * 200, 2000, 100);
		xSphere.m_xColour = Zenith_Maths::Vector4(0., 1., 0., 1.);
	}

	Flux_MemoryManager::UploadBufferData(s_xSpheresBuffer.GetBuffer().m_xVRAMHandle, &s_axSphereData, sizeof(s_axSphereData));
}

void Flux_SDFs::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_SDFs::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_SDFs::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	UploadSpheres();

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&s_xSpheresBuffer.GetCBV(), 1);

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_HDR::GetHDRSceneTargetSetupWithDepth(), RENDER_ORDER_SDFS);
}