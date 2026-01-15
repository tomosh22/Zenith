#include "Zenith.h"

#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Profiling/Zenith_Profiling.h"
#include "Flux/Flux_MaterialBinding.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, Flux_Terrain::RenderToGBuffer, nullptr);
static Zenith_Vector<Zenith_TerrainComponent*> g_xTerrainComponentsToRender;

static Flux_CommandList g_xTerrainCommandList("Terrain");

static Flux_Shader s_xTerrainGBufferShader;
static Flux_Pipeline s_xTerrainGBufferPipeline;
static Flux_Shader s_xTerrainShadowShader;
static Flux_Pipeline s_xTerrainShadowPipeline;
static Flux_Pipeline s_xTerrainWireframePipeline;

static Flux_Shader s_xWaterShader;
static Flux_Pipeline s_xWaterPipeline;

static Flux_Texture s_xWaterNormalTexture;
static uint32_t s_uWaterDisplacementTexHandle = UINT32_MAX;

// ========== GPU-Driven Terrain Culling Pipeline ==========
// Moved from Flux_TerrainCulling to centralize all pipeline ownership in Flux_Terrain
static Flux_Pipeline s_xCullingPipeline;
static Flux_Shader s_xCullingShader;
static Flux_RootSig s_xCullingRootSig;
static Flux_CommandList s_xCullingCommandList("Terrain Culling Compute");

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

DEBUGVAR bool dbg_bEnableTerrain = true;
DEBUGVAR bool dbg_bWireframe = false;
DEBUGVAR float dbg_fVisibilityThresholdMultiplier = 0.5f;
DEBUGVAR bool dbg_bIgnoreVisibilityCheck = false;
DEBUGVAR bool dbg_bLogTerrainMetrics = false;  // Log terrain performance metrics
DEBUGVAR bool dbg_bVisualizeLOD = false;  // Toggle LOD visualization (Red=LOD0, Green=LOD1, Blue=LOD2, Magenta=LOD3)

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
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Terrain constants (was 1)
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // LOD level buffer (was 2)
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

	// Use the global water normal texture pointer set during initialization in Zenith_Main.cpp
	s_xWaterNormalTexture = *Flux_Graphics::s_pxWaterNormalTexture;

	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(struct TerrainConstants
		), s_xTerrainConstantsBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Terrain" }, dbg_bEnableTerrain);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "UV Scale" }, s_xTerrainConstants.m_fUVScale, 0., 10.);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Wireframe" }, dbg_bWireframe);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Ignore Visibility Check" }, dbg_bIgnoreVisibilityCheck);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Visualize LOD" }, dbg_bVisualizeLOD);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Metrics" }, dbg_bLogTerrainMetrics);
#endif

	// ========== Initialize GPU-Driven Terrain Culling Compute Pipeline ==========
	// Moved from Flux_TerrainCulling::Initialise() to centralize pipeline ownership
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain - Initializing terrain culling compute pipeline");

	s_xCullingShader.InitialiseCompute("Terrain/Flux_TerrainCulling.comp");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain - Loaded terrain culling compute shader");

	// Build compute root signature
	Flux_PipelineLayout xCullingLayout;
	xCullingLayout.m_uNumDescriptorSets = 1;
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Chunk data (read)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frustum planes (read)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Indirect commands (write)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Visible count (read/write atomic)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // LOD levels (write)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_MAX;

	Zenith_Vulkan_RootSigBuilder::FromSpecification(s_xCullingRootSig, xCullingLayout);

	// Build compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xCullingBuilder;
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
	// Reset command lists to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xTerrainCommandList.Reset(true);
	s_xCullingCommandList.Reset(true);

	// Clear cached terrain components (will be repopulated next frame)
	g_xTerrainComponentsToRender.Clear();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain::Reset() - Reset command lists and cleared cached terrain components");
}

void Flux_Terrain::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xTerrainConstantsBuffer);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain shut down");
}

void Flux_Terrain::SubmitRenderToGBufferTask()
{
	s_uFrameCounter++;
	
	// Get all terrain components
	g_xTerrainComponentsToRender.Clear();
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(g_xTerrainComponentsToRender);

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

	// ========== Per-Component Terrain Culling Dispatch ==========
	// Each terrain component dispatches its own culling compute pass
	// using its own chunk/LOD metadata and indirect draw buffers
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING);
	s_xCullingCommandList.Reset(false);  // No render targets to clear

	// Bind the terrain culling compute pipeline once (owned by Flux_Terrain)
	s_xCullingCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xCullingPipeline);

	// For each terrain component, dispatch culling using its own buffers
	for (u_int u = 0; u < g_xTerrainComponentsToRender.GetSize(); u++)
	{
		Zenith_TerrainComponent* pxTerrain = g_xTerrainComponentsToRender.Get(u);

		// Component records buffer bindings and dispatch (assumes pipeline already bound)
		pxTerrain->UpdateCullingAndLod(s_xCullingCommandList, Flux_Graphics::s_xFrameConstants.m_xViewProjMat);
	}

	// Submit culling compute command list before terrain rendering
	Flux::SubmitCommandList(&s_xCullingCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_TERRAIN_CULLING);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING);

	// ========== Log Performance Metrics (periodic) ==========
	if (dbg_bLogTerrainMetrics && (s_uFrameCounter % 120 == 0))
	{
		const Flux_TerrainStreamingManager::StreamingStats& xStats = Flux_TerrainStreamingManager::GetStats();
		Zenith_Log(LOG_CATEGORY_TERRAIN, "=== Terrain Performance Metrics (Frame %u) ===", s_uFrameCounter);
		Zenith_Log(LOG_CATEGORY_TERRAIN, "  High-LOD chunks resident: %u", xStats.m_uHighLODChunksResident);
		Zenith_Log(LOG_CATEGORY_TERRAIN, "  Streaming vertex buffer: %u/%u MB (%.1f%%)",
			xStats.m_uVertexBufferUsedMB, xStats.m_uVertexBufferTotalMB,
			(xStats.m_uVertexBufferUsedMB * 100.0f) / xStats.m_uVertexBufferTotalMB);
		Zenith_Log(LOG_CATEGORY_TERRAIN, "  Streaming index buffer: %u/%u MB (%.1f%%)",
			xStats.m_uIndexBufferUsedMB, xStats.m_uIndexBufferTotalMB,
			(xStats.m_uIndexBufferUsedMB * 100.0f) / xStats.m_uIndexBufferTotalMB);
		Zenith_Log(LOG_CATEGORY_TERRAIN, "  Buffer fragmentation: %u vertex blocks, %u index blocks",
			xStats.m_uVertexFragments, xStats.m_uIndexFragments);
	}

	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Terrain::WaitForRenderToGBufferTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Terrain::RenderToGBuffer(void*)
{
	if (!dbg_bEnableTerrain)
	{
		return;
	}

	g_xTerrainCommandList.Reset(false);

	g_xTerrainCommandList.AddCommand<Flux_CommandSetPipeline>(dbg_bWireframe ? &s_xTerrainWireframePipeline : &s_xTerrainGBufferPipeline);

	for (u_int u = 0; u < g_xTerrainComponentsToRender.GetSize(); u++)
	{
		Zenith_TerrainComponent* const pxTerrain = g_xTerrainComponentsToRender.Get(u);
		if(!pxTerrain->GetUnifiedVertexBuffer().GetBuffer().m_ulSize) continue;

		Flux_MaterialAsset& xMaterial0 = pxTerrain->GetMaterial0();
		Flux_MaterialAsset& xMaterial1 = pxTerrain->GetMaterial1();

		// Bind per-frame constants and terrain constants (set 0)
		g_xTerrainCommandList.AddCommand<Flux_CommandBeginBind>(0);
		g_xTerrainCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);

		// Build and push terrain material constants (128 bytes)
		TerrainMaterialPushConstants xTerrainMatConst;
		BuildTerrainMaterialPushConstants(xTerrainMatConst, &xMaterial0, &xMaterial1, dbg_bVisualizeLOD);
		g_xTerrainCommandList.AddCommand<Flux_CommandPushConstant>(&xTerrainMatConst, sizeof(xTerrainMatConst));

		g_xTerrainCommandList.AddCommand<Flux_CommandBindCBV>(&s_xTerrainConstantsBuffer.GetCBV(), 2);

		// Bind LOD level buffer for visualization (binding 3 in set 0)
		// Each component has its own LOD buffer
		g_xTerrainCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&pxTerrain->GetLODLevelBuffer().GetUAV(), 3);


		g_xTerrainCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&pxTerrain->GetUnifiedVertexBuffer());
		g_xTerrainCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&pxTerrain->GetUnifiedIndexBuffer());

		// Bind materials (set 1)
		g_xTerrainCommandList.AddCommand<Flux_CommandBeginBind>(1);
		BindTerrainMaterialTextures(g_xTerrainCommandList, &xMaterial0, 0);
		BindTerrainMaterialTextures(g_xTerrainCommandList, &xMaterial1, 3);

		// GPU-driven indirect rendering with front-to-back sorted visible chunks
		// Each component uses its own indirect draw buffer and visible count buffer
		g_xTerrainCommandList.AddCommand<Flux_CommandDrawIndexedIndirectCount>(
			&pxTerrain->GetIndirectDrawBuffer(),  // Per-component indirect buffer with sorted draw commands
			&pxTerrain->GetVisibleCountBuffer(),   // Per-component count buffer with actual number of visible chunks
			pxTerrain->GetMaxDrawCount(),          // Max 4096 draws (theoretical maximum)
			0,                                      // Indirect buffer offset (bytes)
			0,                                      // Count buffer offset (bytes)
			20                                      // Stride between commands (5 * sizeof(uint32_t))
		);
		
	}

	Flux::SubmitCommandList(&g_xTerrainCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_TERRAIN);
}

void Flux_Terrain::RenderToShadowMap(Flux_CommandList& xCmdBuf)
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
