#include "Zenith.h"

#include "Flux/Terrain/Flux_Terrain.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_Enable = true;

void Flux_Terrain::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Terrain/Flux_Terrain.vert", "Terrain/Flux_Terrain.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });

	std::vector<ColourFormat> xFormats;
	for (ColourFormat eFormat : Flux_Graphics::s_aeMRTFormats)
	{
		xFormats.push_back(eFormat);
	}

	Flux_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		true,
		true,
		DEPTH_COMPARE_FUNC_GREATEREQUAL,
		xFormats,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{1,0},
		{0,8},
		Flux_Graphics::s_xMRTTarget,
		RENDER_TARGET_USAGE_RENDERTARGET
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

#ifdef DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Terrain" }, dbg_Enable);
#endif

	Zenith_Log("Flux_Terrain initialised");
}

void Flux_Terrain::Render()
{
	if (!dbg_Enable)
	{
		return;
	}

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	std::vector<Zenith_TerrainComponent*> xTerrainComponents;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xTerrainComponents);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_DRAW);

	for (Zenith_TerrainComponent* pxTerrain : xTerrainComponents)
	{
		s_xCommandBuffer.SetVertexBuffer(pxTerrain->GetMeshGeometry().GetVertexBuffer());
		s_xCommandBuffer.SetIndexBuffer(pxTerrain->GetMeshGeometry().GetIndexBuffer());

		Flux_Material& xMaterial0 = pxTerrain->GetMaterial0();
		Flux_Material& xMaterial1 = pxTerrain->GetMaterial1();

		s_xCommandBuffer.BindTexture(xMaterial0.GetDiffuse(), 0);
		s_xCommandBuffer.BindTexture(xMaterial0.GetNormal(), 1);
		s_xCommandBuffer.BindTexture(xMaterial0.GetRoughness(), 2);
		s_xCommandBuffer.BindTexture(xMaterial0.GetMetallic(), 3);

		s_xCommandBuffer.BindTexture(xMaterial1.GetDiffuse(), 4);
		s_xCommandBuffer.BindTexture(xMaterial1.GetNormal(), 5);
		s_xCommandBuffer.BindTexture(xMaterial1.GetRoughness(), 6);
		s_xCommandBuffer.BindTexture(xMaterial1.GetMetallic(), 7);

		s_xCommandBuffer.DrawIndexed(pxTerrain->GetMeshGeometry().GetNumIndices());
	}

	s_xCommandBuffer.EndRecording(RENDER_ORDER_TERRAIN);
}
