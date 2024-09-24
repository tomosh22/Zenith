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

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_Texture* s_pxNormalTex = nullptr;
static Flux_Texture* s_pxDisplacementTex = nullptr;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR float dbg_fVisibilityThresholdMultiplier = 0.5f;

void Flux_Water::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Water/Flux_Water.vert", "Water/Flux_Water.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONE, true });

	Flux_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		true,
		true,
		DEPTH_COMPARE_FUNC_LESSEQUAL,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{ 1,0 },
		{ 0,1 },
		Flux_Graphics::s_xFinalRenderTarget,
		false
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	s_pxNormalTex = &Zenith_AssetHandler::GetTexture("Water_Normal");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Water" }, dbg_bEnable);
	Zenith_DebugVariables::AddFloat({ "Render", "Water", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
#endif

	Zenith_Log("Flux_Water initialised");
}

void Flux_Water::Render()
{
	if (!dbg_bEnable)
	{
		return;
	}

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	std::vector<Zenith_TerrainComponent*> xTerrainComponents;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(xTerrainComponents);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_DRAW);

	const Zenith_CameraComponent& xCam = Zenith_Scene::GetCurrentScene().GetMainCamera();

	for (Zenith_TerrainComponent* pxTerrain : xTerrainComponents)
	{
		if (!pxTerrain->IsVisible(dbg_fVisibilityThresholdMultiplier, xCam))
		{
			continue;
		}

		s_xCommandBuffer.SetVertexBuffer(pxTerrain->GetWaterGeometry().GetVertexBuffer());
		s_xCommandBuffer.SetIndexBuffer(pxTerrain->GetWaterGeometry().GetIndexBuffer());

		s_xCommandBuffer.BindTexture(s_pxNormalTex, 0);

		s_xCommandBuffer.DrawIndexed(pxTerrain->GetWaterGeometry().GetNumIndices());
	}

	s_xCommandBuffer.EndRecording(RENDER_ORDER_WATER);
}