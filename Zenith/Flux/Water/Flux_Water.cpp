#include "Zenith.h"

#include "Flux/Water/Flux_Water.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_WATER, Flux_Water::Render, nullptr);

static Flux_CommandList g_xCommandList("Water");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_Texture* s_pxNormalTex = nullptr;
static Flux_Texture* s_pxDisplacementTex = nullptr;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR float dbg_fVisibilityThresholdMultiplier = 0.5f;

void Flux_Water::Initialise()
{
	s_xShader.Initialise("Water/Flux_Water.vert", "Water/Flux_Water.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 2;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_TEXTURE;

	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	s_pxNormalTex = Zenith_AssetHandler::GetTexture("Water_Normal");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Water" }, dbg_bEnable);
	Zenith_DebugVariables::AddFloat({ "Render", "Water", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
#endif

	Zenith_Log("Flux_Water initialised");
}

void Flux_Water::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Water::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Water::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	Zenith_Vector<Zenith_TerrainComponent*> xTerrainComponents;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xTerrainComponents);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(1);

	const Zenith_CameraComponent& xCam = Zenith_Scene::GetCurrentScene().GetMainCamera();

	for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xTerrainComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_TerrainComponent* pxTerrain = xIt.GetData();
		if (!pxTerrain->IsVisible(dbg_fVisibilityThresholdMultiplier, xCam))
		{
			continue;
		}

		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&pxTerrain->GetWaterGeometry().GetVertexBuffer());
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&pxTerrain->GetWaterGeometry().GetIndexBuffer());

		g_xCommandList.AddCommand<Flux_CommandBindTexture>(s_pxNormalTex, 0);

		g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(pxTerrain->GetWaterGeometry().GetNumIndices());
	}

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget, RENDER_ORDER_WATER);
}