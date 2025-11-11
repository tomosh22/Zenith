#include "Zenith.h"

#include "Flux/Terrain/Flux_Terrain.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static constexpr u_int uNUM_TASKS = 4;

static Zenith_TaskArray* g_pxRenderTask = nullptr;
static Zenith_Vector<Zenith_TerrainComponent*> g_xTerrainComponentsToRender;

static Flux_CommandList g_axPerThreadTerrainCommandLists[uNUM_TASKS] {"Terrain", "Terrain", "Terrain", "Terrain"};
static Flux_CommandList g_axPerThreadWaterCommandLists[uNUM_TASKS] {"Water", "Water" , "Water" , "Water"};

static Flux_Shader s_xTerrainGBufferShader;
static Flux_Pipeline s_xTerrainGBufferPipeline;
static Flux_Shader s_xTerrainShadowShader;
static Flux_Pipeline s_xTerrainShadowPipeline;
static Flux_Pipeline s_xTerrainWireframePipeline;

static Flux_Shader s_xWaterShader;
static Flux_Pipeline s_xWaterPipeline;

static Flux_Texture s_xWaterNormalTexture;
static uint32_t s_uWaterDisplacementTexHandle = UINT32_MAX;

struct TerrainConstants
{
	float m_fUVScale = 0.07;
} s_xTerrainConstants;
static Flux_DynamicConstantBuffer s_xTerrainConstantsBuffer;

DEBUGVAR bool dbg_bEnableTerrain = true;
DEBUGVAR bool dbg_bEnableWater = true;
DEBUGVAR bool dbg_bWireframe = false;
DEBUGVAR float dbg_fVisibilityThresholdMultiplier = 0.5f;
DEBUGVAR bool dbg_bIgnoreVisibilityCheck = false;

void Flux_Terrain::Initialise()
{
	s_xTerrainGBufferShader.Initialise("Terrain/Flux_Terrain_ToGBuffer.vert", "Terrain/Flux_Terrain_ToGBuffer.frag");
	s_xTerrainShadowShader.Initialise("Terrain/Flux_Terrain_ToShadowmap.vert", "Terrain/Flux_Terrain_ToShadowmap.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;
		xPipelineSpec.m_pxShader = &s_xTerrainGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_TEXTURE;

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(s_xTerrainGBufferPipeline, xPipelineSpec);

		xPipelineSpec.m_bWireframe = true;
		Flux_PipelineBuilder::FromSpecification(s_xTerrainWireframePipeline, xPipelineSpec);
	}

	
	{
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_pxTargetSetup = &Flux_Shadows::GetCSMTargetSetup(0);
		xShadowPipelineSpec.m_pxShader = &s_xTerrainShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		Flux_PipelineLayout& xLayout = xShadowPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;

		xShadowPipelineSpec.m_bDepthTestEnabled = true;
		xShadowPipelineSpec.m_bDepthWriteEnabled = true;
		xShadowPipelineSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;

		Flux_PipelineBuilder::FromSpecification(s_xTerrainShadowPipeline, xShadowPipelineSpec);
	}
	
	{
		s_xWaterShader.Initialise("Water/Flux_Water.vert", "Water/Flux_Water.frag");

		Flux_VertexInputDescription xVertexDesc;
		xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
		xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
		xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
		xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget;
		xPipelineSpec.m_pxShader = &s_xWaterShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_TEXTURE;

		xPipelineSpec.m_bDepthWriteEnabled = false;

		Flux_PipelineBuilder::FromSpecification(s_xWaterPipeline, xPipelineSpec);
	}

	s_xWaterNormalTexture = Zenith_AssetHandler::GetTexture("Water_Normal");

	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(struct TerrainConstants
		), s_xTerrainConstantsBuffer);

	//#TO_TODO: delete this on shutdown
	g_pxRenderTask = new Zenith_TaskArray(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, Flux_Terrain::RenderToGBuffer, nullptr, uNUM_TASKS);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Terrain" }, dbg_bEnableTerrain);
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Water" }, dbg_bEnableWater);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "UV Scale" }, s_xTerrainConstants.m_fUVScale, 0., 10.);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Wireframe" }, dbg_bWireframe);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Ignore Visibility Check" }, dbg_bIgnoreVisibilityCheck);
#endif

	Zenith_Log("Flux_Terrain initialised");
}

void Flux_Terrain::SubmitRenderToGBufferTask()
{
	if (!dbg_bEnableTerrain && !dbg_bEnableWater)
	{
		return;
	}

	g_xTerrainComponentsToRender.Clear();
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(g_xTerrainComponentsToRender);

	Flux_MemoryManager::UploadBufferData(s_xTerrainConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xTerrainConstants, sizeof(TerrainConstants));

	Zenith_TaskSystem::SubmitTaskArray(g_pxRenderTask);
}

void Flux_Terrain::WaitForRenderToGBufferTask()
{
	g_pxRenderTask->WaitUntilComplete();
}

void Flux_Terrain::RenderToGBuffer(void*, u_int uInvocationIndex, u_int uNumInvocations)
{
	const u_int uTotalTerrains = g_xTerrainComponentsToRender.GetSize();
	
	if (uInvocationIndex >= uTotalTerrains)
	{
		return;
	}

	const u_int uTerrainsPerInvocation = (uTotalTerrains + uNumInvocations - 1) / uNumInvocations;
	const u_int uStartIndex = uInvocationIndex * uTerrainsPerInvocation;
	const u_int uEndIndex = (uStartIndex + uTerrainsPerInvocation < uTotalTerrains) ? 
		uStartIndex + uTerrainsPerInvocation : uTotalTerrains;

	Flux_CommandList& xTerrainCmdList = g_axPerThreadTerrainCommandLists[uInvocationIndex];
	Flux_CommandList& xWaterCmdList = g_axPerThreadWaterCommandLists[uInvocationIndex];

	xTerrainCmdList.Reset(false);
	xWaterCmdList.Reset(false);

	xTerrainCmdList.AddCommand<Flux_CommandSetPipeline>(dbg_bWireframe ? &s_xTerrainWireframePipeline : &s_xTerrainGBufferPipeline);
	xWaterCmdList.AddCommand<Flux_CommandSetPipeline>(&s_xWaterPipeline);

	xTerrainCmdList.AddCommand<Flux_CommandBeginBind>(0);
	xTerrainCmdList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer().m_xCBV, 0);
	xTerrainCmdList.AddCommand<Flux_CommandBindCBV>(&s_xTerrainConstantsBuffer.GetBuffer().m_xCBV, 1);

	xTerrainCmdList.AddCommand<Flux_CommandBeginBind>(1);
	
	xWaterCmdList.AddCommand<Flux_CommandBeginBind>(0);
	xWaterCmdList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer().m_xCBV, 0);
	xWaterCmdList.AddCommand<Flux_CommandBeginBind>(1);

	const Zenith_CameraComponent& xCam = Zenith_Scene::GetCurrentScene().GetMainCamera();

	for (u_int u = uStartIndex; u < uEndIndex; u++)
	{
		Zenith_TerrainComponent* pxTerrain = g_xTerrainComponentsToRender.Get(u);
		
		if (!dbg_bIgnoreVisibilityCheck && !pxTerrain->IsVisible(dbg_fVisibilityThresholdMultiplier, xCam))
		{
			continue;
		}

		xTerrainCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&pxTerrain->GetRenderMeshGeometry().GetVertexBuffer());
		xTerrainCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&pxTerrain->GetRenderMeshGeometry().GetIndexBuffer());

		const Flux_Material& xMaterial0 = pxTerrain->GetMaterial0();
		const Flux_Material& xMaterial1 = pxTerrain->GetMaterial1();

		xTerrainCmdList.AddCommand<Flux_CommandBindSRV>(&xMaterial0.GetDiffuse()->m_xSRV, 0);
		xTerrainCmdList.AddCommand<Flux_CommandBindSRV>(&xMaterial0.GetNormal()->m_xSRV, 1);
		xTerrainCmdList.AddCommand<Flux_CommandBindSRV>(&xMaterial0.GetRoughnessMetallic()->m_xSRV, 2);

		xTerrainCmdList.AddCommand<Flux_CommandBindSRV>(&xMaterial1.GetDiffuse()->m_xSRV, 3);
		xTerrainCmdList.AddCommand<Flux_CommandBindSRV>(&xMaterial1.GetNormal()->m_xSRV, 4);
		xTerrainCmdList.AddCommand<Flux_CommandBindSRV>(&xMaterial1.GetRoughnessMetallic()->m_xSRV, 5);

		xTerrainCmdList.AddCommand<Flux_CommandDrawIndexed>(pxTerrain->GetRenderMeshGeometry().GetNumIndices());


		xWaterCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&pxTerrain->GetWaterGeometry().GetVertexBuffer());
		xWaterCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&pxTerrain->GetWaterGeometry().GetIndexBuffer());

		xWaterCmdList.AddCommand<Flux_CommandBindSRV>(&s_xWaterNormalTexture.m_xSRV, 0);

		xWaterCmdList.AddCommand<Flux_CommandDrawIndexed>(pxTerrain->GetWaterGeometry().GetNumIndices());
	}

	Flux::SubmitCommandList(&xTerrainCmdList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_TERRAIN);
	Flux::SubmitCommandList(&xWaterCmdList, Flux_Graphics::s_xFinalRenderTarget, RENDER_ORDER_WATER);
}

void Flux_Terrain::RenderToShadowMap(Flux_CommandList& xCmdBuf)
{
	Zenith_Vector<Zenith_TerrainComponent*> xTerrainComponents;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xTerrainComponents);

	//#TO_TODO: skip terrain components that aren't visibile from CSM

	for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator xIt(xTerrainComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_TerrainComponent* pxTerrain = xIt.GetData();

		xCmdBuf.AddCommand<Flux_CommandSetVertexBuffer>(&pxTerrain->GetRenderMeshGeometry().GetVertexBuffer());
		xCmdBuf.AddCommand<Flux_CommandSetIndexBuffer>(&pxTerrain->GetRenderMeshGeometry().GetIndexBuffer());

		xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxTerrain->GetRenderMeshGeometry().GetNumIndices());
	}
}

Flux_Pipeline& Flux_Terrain::GetShadowPipeline()
{
	return s_xTerrainShadowPipeline;
}

Flux_DynamicConstantBuffer& Flux_Terrain::GetTerrainConstantsBuffer()
{
	return s_xTerrainConstantsBuffer;
}
