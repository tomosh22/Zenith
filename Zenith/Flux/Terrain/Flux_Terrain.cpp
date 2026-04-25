#include "Zenith.h"

#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Profiling/Zenith_Profiling.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

static Zenith_Vector<Zenith_TerrainComponent*> g_xTerrainComponentsToRender;

static Flux_Shader s_xTerrainGBufferShader;
static Flux_Pipeline s_xTerrainGBufferPipeline;
static Flux_Shader s_xTerrainShadowShader;
static Flux_Pipeline s_xTerrainShadowPipeline;
static Flux_Pipeline s_xTerrainWireframePipeline;

static Flux_Shader s_xWaterShader;
static Flux_Pipeline s_xWaterPipeline;

static Zenith_TextureAsset* s_pxWaterNormalTexture = nullptr;
static uint32_t s_uWaterDisplacementTexHandle = UINT32_MAX;

// ========== GPU-Driven Terrain Culling Pipeline ==========
// Moved from Flux_TerrainCulling to centralize all pipeline ownership in Flux_Terrain
static Flux_Pipeline s_xCullingPipeline;
static Flux_Shader s_xCullingShader;
static Flux_RootSig s_xCullingRootSig;

// ========== Performance Metrics ==========
static uint32_t s_uFrameCounter = 0;
static uint32_t s_uLastVisibleChunks = 0;
static float s_fCullingTimeMs = 0.0f;
static float s_fStreamingTimeMs = 0.0f;

struct TerrainConstants
{
	float m_fUVScale = 0.07;
} s_xTerrainConstants;
static Flux_DynamicConstantBuffer s_xTerrainConstantsBuffer;

bool dbg_bWireframe = false;
DEBUGVAR float dbg_fVisibilityThresholdMultiplier = 0.5f;
DEBUGVAR bool dbg_bIgnoreVisibilityCheck = false;
DEBUGVAR bool dbg_bLogTerrainMetrics = false;  // Log terrain performance metrics
u_int dbg_uDebugMode = 0;  // Debug visualization mode (0=Off, 1=LOD, 2=Normals, 3=UVs, etc.)

void Flux_Terrain::Initialise()
{

	s_xTerrainGBufferShader.Initialise("Terrain/Flux_Terrain_ToGBuffer.vert", "Terrain/Flux_Terrain_ToGBuffer.frag");
	s_xTerrainShadowShader.Initialise("Terrain/Flux_Terrain_ToShadowmap.vert", "Terrain/Flux_Terrain_ToShadowmap.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);           // Position (12 bytes)
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_HALF2);             // UV (4 bytes)
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_SNORM10_10_10_2);   // Normal (4 bytes)
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_SNORM10_10_10_2);   // Tangent + BitangentSign (4 bytes)
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xPipelineSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &s_xTerrainGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		s_xTerrainGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

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
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &s_xTerrainShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		s_xTerrainShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		xShadowPipelineSpec.m_bDepthTestEnabled = true;
		xShadowPipelineSpec.m_bDepthWriteEnabled = true;
		xShadowPipelineSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;

		Flux_PipelineBuilder::FromSpecification(s_xTerrainShadowPipeline, xShadowPipelineSpec);
	}
	
	{
		s_xWaterShader.Initialise("Water/Flux_Water.vert", "Water/Flux_Water.frag");

		Flux_VertexInputDescription xWaterVertexDesc;
		xWaterVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
		xWaterVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
		xWaterVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
		xWaterVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[0] = FINAL_RT_FORMAT;
		xPipelineSpec.m_uNumColourAttachments = 1;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &s_xWaterShader;
		xPipelineSpec.m_xVertexInputDesc = xWaterVertexDesc;

		s_xWaterShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		xPipelineSpec.m_bDepthWriteEnabled = false;

		Flux_PipelineBuilder::FromSpecification(s_xWaterPipeline, xPipelineSpec);
	}

	// Use the global water normal texture pointer set during initialization in Zenith_Main.cpp
	s_pxWaterNormalTexture = Flux_Graphics::s_pxWaterNormalTexture;

	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(struct TerrainConstants
		), s_xTerrainConstantsBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "UV Scale" }, s_xTerrainConstants.m_fUVScale, 0., 10.);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Wireframe" }, dbg_bWireframe);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Ignore Visibility Check" }, dbg_bIgnoreVisibilityCheck);
	Zenith_DebugVariables::AddUInt32({ "Render", "Terrain", "Debug Mode" }, dbg_uDebugMode, 0, 11);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Metrics" }, dbg_bLogTerrainMetrics);
#endif

	// ========== Initialize GPU-Driven Terrain Culling Compute Pipeline ==========
	// Moved from Flux_TerrainCulling::Initialise() to centralize pipeline ownership
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain - Initializing terrain culling compute pipeline");

	s_xCullingShader.InitialiseCompute("Terrain/Flux_TerrainCulling.comp");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain - Loaded terrain culling compute shader");

	// Build compute root signature from shader reflection
	const Flux_ShaderReflection& xCullingReflection = s_xCullingShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(s_xCullingRootSig, xCullingReflection);

	// Build compute pipeline
	Flux_ComputePipelineBuilder xCullingBuilder;
	xCullingBuilder.WithShader(s_xCullingShader)
		.WithLayout(s_xCullingRootSig.m_xLayout)
		.Build(s_xCullingPipeline);

	s_xCullingPipeline.m_xRootSig = s_xCullingRootSig;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain - Built terrain culling compute pipeline");

	// ========== Initialize Terrain Streaming Manager ==========
	Flux_TerrainStreamingManager::Initialize();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain initialised");
}

void Flux_Terrain::Reset()
{
	// Reset is handled by the render graph
	// Clear cached terrain components (will be repopulated next frame)
	g_xTerrainComponentsToRender.Clear();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain::Reset()");
}

void Flux_Terrain::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xTerrainConstantsBuffer);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain shut down");
}

void Flux_Terrain::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Pass 1: Terrain culling compute. Touches per-Zenith_TerrainComponent
	// indirect-draw / visible-count buffers that are dynamic (created per
	// component) and not graph-tracked. The compute output is consumed by the
	// GBuffer pass via DrawIndexedIndirectCount, so the ordering is encoded
	// as an explicit DependsOn edge on the GBuffer pass.
	Flux_PassHandle xCullingPass = xGraph.AddPass("Terrain Culling Compute", ExecuteCulling);

	xGraph.AddPass("Terrain GBuffer", ExecuteGBuffer)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.DependsOn(xCullingPass);
}

void Flux_Terrain::PreRenderUpdate()
{
	s_uFrameCounter++;

	// Get all terrain components
	Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_TerrainComponent>(g_xTerrainComponentsToRender);

	Flux_MemoryManager::UploadBufferData(s_xTerrainConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xTerrainConstants, sizeof(TerrainConstants));

	// ========== Update Terrain LOD Streaming ==========
	// Process streaming requests and evictions based on camera position
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING);
	Zenith_Maths::Vector3 xCameraPos = Flux_Graphics::GetCameraPosition();
	Flux_TerrainStreamingManager::UpdateStreaming(xCameraPos);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING);

	// ========== Update Chunk LOD Allocations ==========
	// Update each terrain component's chunk data buffer with current LOD allocations
	for (u_int u = 0; u < g_xTerrainComponentsToRender.GetSize(); u++)
	{
		Zenith_TerrainComponent* pxTerrain = g_xTerrainComponentsToRender.Get(u);
		pxTerrain->UpdateChunkLODAllocations();
	}
}

void Flux_Terrain::ExecuteCulling(Flux_CommandList* pxCmdList, void*)
{
	// CPU-side update (was in PreRenderUpdate/SubmitRenderToGBufferTask)
	PreRenderUpdate();

	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING);

	// Bind the terrain culling compute pipeline once (owned by Flux_Terrain)
	pxCmdList->AddCommand<Flux_CommandBindComputePipeline>(&s_xCullingPipeline);

	// For each terrain component, dispatch culling using its own buffers
	for (u_int u = 0; u < g_xTerrainComponentsToRender.GetSize(); u++)
	{
		Zenith_TerrainComponent* pxTerrain = g_xTerrainComponentsToRender.Get(u);

		// Component records buffer bindings and dispatch (assumes pipeline already bound)
		pxTerrain->UpdateCullingAndLod(*pxCmdList, Flux_Graphics::s_xFrameConstants.m_xViewProjMat);
	}

	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING);
}

void Flux_Terrain::ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(dbg_bWireframe ? &s_xTerrainWireframePipeline : &s_xTerrainGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Bind set 0 (per-frame data) once per command list
	xBinder.BindCBV(s_xTerrainGBufferShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindCBV(s_xTerrainGBufferShader, "TerrainConstants", &s_xTerrainConstantsBuffer.GetCBV());

	for (u_int u = 0; u < g_xTerrainComponentsToRender.GetSize(); u++)
	{
		Zenith_TerrainComponent* const pxTerrain = g_xTerrainComponentsToRender.Get(u);
		if(!pxTerrain->GetUnifiedVertexBuffer().GetBuffer().m_ulSize) continue;

		// Gather 4 material pointers
		Zenith_MaterialAsset* apxMaterials[4];
		for (u_int m = 0; m < 4; m++)
			apxMaterials[m] = pxTerrain->GetMaterial(m);

		// Build and push terrain material constants (288 bytes) - uses scratch buffer in set 1
		TerrainMaterialDrawConstants xTerrainMatConst;
		BuildTerrainMaterialDrawConstants(xTerrainMatConst, apxMaterials, 4, dbg_uDebugMode,
			0.0f, 0.0f, Flux_TerrainConfig::TERRAIN_SIZE, Flux_TerrainConfig::TERRAIN_SIZE);
		xBinder.BindDrawConstants(s_xTerrainGBufferShader, "TerrainMaterialConstants", &xTerrainMatConst, sizeof(xTerrainMatConst));

		// Bind LOD level buffer (per-terrain, set 1)
		xBinder.BindUAV_Buffer(s_xTerrainGBufferShader, "LODLevelBuffer", &pxTerrain->GetLODLevelBuffer().GetUAV());

		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxTerrain->GetUnifiedVertexBuffer());
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxTerrain->GetUnifiedIndexBuffer());

		// Bind splatmap texture
		if (pxTerrain->GetSplatmapTexture())
			xBinder.BindSRV(s_xTerrainGBufferShader, "g_xSplatmap", &pxTerrain->GetSplatmapTexture()->m_xSRV);

		// Bind material textures (set 1, named bindings) - 4 materials x 5 textures each
		// Material 0 textures
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xDiffuseTex0", &apxMaterials[0]->GetDiffuseTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xNormalTex0", &apxMaterials[0]->GetNormalTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xRoughnessMetallicTex0", &apxMaterials[0]->GetRoughnessMetallicTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xOcclusionTex0", &apxMaterials[0]->GetOcclusionTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xEmissiveTex0", &apxMaterials[0]->GetEmissiveTexture()->m_xSRV);
		// Material 1 textures
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xDiffuseTex1", &apxMaterials[1]->GetDiffuseTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xNormalTex1", &apxMaterials[1]->GetNormalTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xRoughnessMetallicTex1", &apxMaterials[1]->GetRoughnessMetallicTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xOcclusionTex1", &apxMaterials[1]->GetOcclusionTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xEmissiveTex1", &apxMaterials[1]->GetEmissiveTexture()->m_xSRV);
		// Material 2 textures
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xDiffuseTex2", &apxMaterials[2]->GetDiffuseTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xNormalTex2", &apxMaterials[2]->GetNormalTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xRoughnessMetallicTex2", &apxMaterials[2]->GetRoughnessMetallicTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xOcclusionTex2", &apxMaterials[2]->GetOcclusionTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xEmissiveTex2", &apxMaterials[2]->GetEmissiveTexture()->m_xSRV);
		// Material 3 textures
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xDiffuseTex3", &apxMaterials[3]->GetDiffuseTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xNormalTex3", &apxMaterials[3]->GetNormalTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xRoughnessMetallicTex3", &apxMaterials[3]->GetRoughnessMetallicTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xOcclusionTex3", &apxMaterials[3]->GetOcclusionTexture()->m_xSRV);
		xBinder.BindSRV(s_xTerrainGBufferShader, "g_xEmissiveTex3", &apxMaterials[3]->GetEmissiveTexture()->m_xSRV);

		// GPU-driven indirect rendering with front-to-back sorted visible chunks
		// Each component uses its own indirect draw buffer and visible count buffer
		pxCmdList->AddCommand<Flux_CommandDrawIndexedIndirectCount>(
			&pxTerrain->GetIndirectDrawBuffer(),  // Per-component indirect buffer with sorted draw commands
			&pxTerrain->GetVisibleCountBuffer(),   // Per-component count buffer with actual number of visible chunks
			pxTerrain->GetMaxDrawCount(),          // Max 4096 draws (theoretical maximum)
			0,                                      // Indirect buffer offset (bytes)
			0,                                      // Count buffer offset (bytes)
			20                                      // Stride between commands (5 * sizeof(uint32_t))
		);

	}
}

void Flux_Terrain::RenderToShadowMap(Flux_CommandList&, const Flux_DynamicConstantBuffer&)
{
	STUBBED
}

Flux_Pipeline& Flux_Terrain::GetShadowPipeline()
{
	return s_xTerrainShadowPipeline;
}

Flux_DynamicConstantBuffer& Flux_Terrain::GetTerrainConstantsBuffer()
{
	return s_xTerrainConstantsBuffer;
}

Flux_Pipeline& Flux_Terrain::GetCullingPipeline()
{
	return s_xCullingPipeline;
}

u_int& Flux_Terrain::GetDebugMode()
{
	return dbg_uDebugMode;
}

bool& Flux_Terrain::GetWireframeMode()
{
	return dbg_bWireframe;
}
