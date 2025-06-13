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

#ifndef ZENITH_MERGE_GBUFFER_PASSES
static Flux_CommandBuffer s_xCommandBuffer;
#endif

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;
static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;
static Flux_Pipeline s_xWireframePipeline;


struct TerrainConstants
{
	float m_fUVScale = 0.07;
} s_xTerrainConstants;
static Flux_DynamicConstantBuffer s_xTerrainConstantsBuffer;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR bool dbg_bWireframe = false;
DEBUGVAR float dbg_fVisibilityThresholdMultiplier = 0.5f;
DEBUGVAR bool dbg_bIgnoreVisibilityCheck = true;

void Flux_Terrain::Initialise()
{
	#ifndef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.Initialise();
	#endif

	s_xGBufferShader.Initialise("Terrain/Flux_Terrain_ToGBuffer.vert", "Terrain/Flux_Terrain_ToGBuffer.frag");
	s_xShadowShader.Initialise("Terrain/Flux_Terrain_ToShadowmap.vert", "Terrain/Flux_Terrain_ToShadowmap.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;
		xPipelineSpec.m_pxShader = &s_xGBufferShader;
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
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[7].m_eType = DESCRIPTOR_TYPE_TEXTURE;

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}
#if 0
		(
			xVertexDesc,
			&s_xGBufferShader,
			xBlendStates,
			true,
			true,
			DEPTH_COMPARE_FUNC_LESSEQUAL,
			DEPTHSTENCIL_FORMAT_D32_SFLOAT,
			true,
			false,
			{ 2,0 },
			{ 0,8 },
			Flux_Graphics::s_xMRTTarget,
			false
		);
#endif

		Flux_PipelineBuilder::FromSpecification(s_xGBufferPipeline, xPipelineSpec);

		xPipelineSpec.m_bWireframe = true;
		Flux_PipelineBuilder::FromSpecification(s_xWireframePipeline, xPipelineSpec);
	}

	
	{
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_pxTargetSetup = &Flux_Shadows::GetCSMTargetSetup(0);
		xShadowPipelineSpec.m_pxShader = &s_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		Flux_PipelineLayout& xLayout = xShadowPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;

		xShadowPipelineSpec.m_bDepthTestEnabled = true;
		xShadowPipelineSpec.m_bDepthWriteEnabled = true;
		xShadowPipelineSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;

#if 0
		(
			xVertexDesc,
			&s_xShadowShader,
			xBlendStates,
			true,
			true,
			DEPTH_COMPARE_FUNC_LESSEQUAL,
			DEPTHSTENCIL_FORMAT_D32_SFLOAT,
			true,
			false,
			{ 2,0 },
			{ 1,0 },
			Flux_Shadows::GetCSMTargetSetup(0),
			false
		);
#endif

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}
	


	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(struct TerrainConstants
		), s_xTerrainConstantsBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Terrain" }, dbg_bEnable);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "UV Scale" }, s_xTerrainConstants.m_fUVScale, 0., 10.);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Wireframe" }, dbg_bWireframe);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Ignore Visibility Check" }, dbg_bIgnoreVisibilityCheck);
#endif

	Zenith_Log("Flux_Terrain initialised");
}

void Flux_Terrain::RenderToGBuffer()
{
	if (!dbg_bEnable)
	{
		return;
	}

	Flux_MemoryManager::UploadBufferData(s_xTerrainConstantsBuffer.GetBuffer(), &s_xTerrainConstants, sizeof(TerrainConstants));

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	//#TO_TODO: fix up naming convention
	Flux_CommandBuffer& s_xCommandBuffer = Flux_DeferredShading::GetTerrainCommandBuffer();
	s_xCommandBuffer.BeginRecording();
	#else
	s_xCommandBuffer.BeginRecording();
	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget);
	#endif

	s_xCommandBuffer.SetPipeline(dbg_bWireframe ? &s_xWireframePipeline : &s_xGBufferPipeline);

	std::vector<Zenith_TerrainComponent*> xTerrainComponents;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xTerrainComponents);

	s_xCommandBuffer.BeginBind(0);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindBuffer(&s_xTerrainConstantsBuffer.GetBuffer(), 1);

	s_xCommandBuffer.BeginBind(1);

	const Zenith_CameraComponent& xCam = Zenith_Scene::GetCurrentScene().GetMainCamera();

	for (Zenith_TerrainComponent* pxTerrain : xTerrainComponents)
	{
		if (!dbg_bIgnoreVisibilityCheck && !pxTerrain->IsVisible(dbg_fVisibilityThresholdMultiplier, xCam))
		{
			continue;
		}

		s_xCommandBuffer.SetVertexBuffer(pxTerrain->GetRenderMeshGeometry().GetVertexBuffer());
		s_xCommandBuffer.SetIndexBuffer(pxTerrain->GetRenderMeshGeometry().GetIndexBuffer());

		const Flux_Material& xMaterial0 = pxTerrain->GetMaterial0();
		const Flux_Material& xMaterial1 = pxTerrain->GetMaterial1();

		s_xCommandBuffer.BindTexture(xMaterial0.GetDiffuse(), 0);
		s_xCommandBuffer.BindTexture(xMaterial0.GetNormal(), 1);
		s_xCommandBuffer.BindTexture(xMaterial0.GetRoughness(), 2);
		s_xCommandBuffer.BindTexture(xMaterial0.GetMetallic(), 3);

		s_xCommandBuffer.BindTexture(xMaterial1.GetDiffuse(), 4);
		s_xCommandBuffer.BindTexture(xMaterial1.GetNormal(), 5);
		s_xCommandBuffer.BindTexture(xMaterial1.GetRoughness(), 6);
		s_xCommandBuffer.BindTexture(xMaterial1.GetMetallic(), 7);

		s_xCommandBuffer.DrawIndexed(pxTerrain->GetRenderMeshGeometry().GetNumIndices());
	}

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.EndRecording(RENDER_ORDER_GBUFFER);
	#else
	s_xCommandBuffer.EndRecording(RENDER_ORDER_TERRAIN);
	#endif
}

void Flux_Terrain::RenderToShadowMap(Flux_CommandBuffer& xCmdBuf)
{
	std::vector<Zenith_TerrainComponent*> xTerrainComponents;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xTerrainComponents);

	//#TO_TODO: skip terrain components that aren't visibile from CSM

	for (Zenith_TerrainComponent* pxTerrain : xTerrainComponents)
	{

		xCmdBuf.SetVertexBuffer(pxTerrain->GetRenderMeshGeometry().GetVertexBuffer());
		xCmdBuf.SetIndexBuffer(pxTerrain->GetRenderMeshGeometry().GetIndexBuffer());

		xCmdBuf.DrawIndexed(pxTerrain->GetRenderMeshGeometry().GetNumIndices());
	}
}

Flux_Pipeline& Flux_Terrain::GetShadowPipeline()
{
	return s_xShadowPipeline;
}

Flux_DynamicConstantBuffer& Flux_Terrain::GetTerrainConstantsBuffer()
{
	return s_xTerrainConstantsBuffer;
}
