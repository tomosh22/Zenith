#include "Zenith.h"

#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Profiling/Zenith_Profiling.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

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

static Zenith_TextureAsset* s_pxWaterNormalTexture = nullptr;
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

// Cached binding handles for named resource binding (populated at init from shader reflection)
// GBuffer shader - set 0 bindings (per-frame)
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xTerrainConstantsBinding;
// GBuffer shader - set 1 bindings (per-draw)
static Flux_BindingHandle s_xScratchBufferBinding;  // For PushConstant calls
static Flux_BindingHandle s_xLODLevelBufferBinding;
// Material 0 textures (5 per material - full material system)
static Flux_BindingHandle s_xDiffuseTex0Binding;
static Flux_BindingHandle s_xNormalTex0Binding;
static Flux_BindingHandle s_xRoughnessMetallicTex0Binding;
static Flux_BindingHandle s_xOcclusionTex0Binding;
static Flux_BindingHandle s_xEmissiveTex0Binding;
// Material 1 textures
static Flux_BindingHandle s_xDiffuseTex1Binding;
static Flux_BindingHandle s_xNormalTex1Binding;
static Flux_BindingHandle s_xRoughnessMetallicTex1Binding;
static Flux_BindingHandle s_xOcclusionTex1Binding;
static Flux_BindingHandle s_xEmissiveTex1Binding;

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
		// Set 0: Per-frame (FrameConstants + TerrainConstants - bound once per command list)
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Terrain constants
		// Set 1: Per-draw (scratch buffer + LOD level buffer + 10 textures for 2 materials)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // LOD level buffer
		// Material 0 textures (diffuse, normal, RM, occlusion, emissive)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		// Material 1 textures (diffuse, normal, RM, occlusion, emissive)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[7].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[8].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[9].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[10].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[11].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(s_xTerrainGBufferPipeline, xPipelineSpec);

		xPipelineSpec.m_bWireframe = true;
		Flux_PipelineBuilder::FromSpecification(s_xTerrainWireframePipeline, xPipelineSpec);

		// Cache binding handles from shader reflection for named resource binding
		const Flux_ShaderReflection& xGBufferReflection = s_xTerrainGBufferShader.GetReflection();
		// Set 0 bindings (per-frame)
		s_xFrameConstantsBinding = xGBufferReflection.GetBinding("FrameConstants");
		s_xTerrainConstantsBinding = xGBufferReflection.GetBinding("TerrainConstants");
		// Set 1 bindings (per-draw)
		s_xScratchBufferBinding = xGBufferReflection.GetBinding("TerrainMaterialConstants");  // Scratch buffer for per-draw data
		s_xLODLevelBufferBinding = xGBufferReflection.GetBinding("LODLevelBuffer");
		// Material 0 texture bindings (5 textures - full material system)
		s_xDiffuseTex0Binding = xGBufferReflection.GetBinding("g_xDiffuseTex0");
		s_xNormalTex0Binding = xGBufferReflection.GetBinding("g_xNormalTex0");
		s_xRoughnessMetallicTex0Binding = xGBufferReflection.GetBinding("g_xRoughnessMetallicTex0");
		s_xOcclusionTex0Binding = xGBufferReflection.GetBinding("g_xOcclusionTex0");
		s_xEmissiveTex0Binding = xGBufferReflection.GetBinding("g_xEmissiveTex0");
		// Material 1 texture bindings (5 textures - full material system)
		s_xDiffuseTex1Binding = xGBufferReflection.GetBinding("g_xDiffuseTex1");
		s_xNormalTex1Binding = xGBufferReflection.GetBinding("g_xNormalTex1");
		s_xRoughnessMetallicTex1Binding = xGBufferReflection.GetBinding("g_xRoughnessMetallicTex1");
		s_xOcclusionTex1Binding = xGBufferReflection.GetBinding("g_xOcclusionTex1");
		s_xEmissiveTex1Binding = xGBufferReflection.GetBinding("g_xEmissiveTex1");
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
	s_pxWaterNormalTexture = Flux_Graphics::s_pxWaterNormalTexture;

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

	// Build compute root signature from shader reflection
	const Flux_ShaderReflection& xCullingReflection = s_xCullingShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(s_xCullingRootSig, xCullingReflection);

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

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(g_xTerrainCommandList);

	// Bind set 0 (per-frame data) once per command list
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xBinder.BindCBV(s_xTerrainConstantsBinding, &s_xTerrainConstantsBuffer.GetCBV());

	for (u_int u = 0; u < g_xTerrainComponentsToRender.GetSize(); u++)
	{
		Zenith_TerrainComponent* const pxTerrain = g_xTerrainComponentsToRender.Get(u);
		if(!pxTerrain->GetUnifiedVertexBuffer().GetBuffer().m_ulSize) continue;

		Zenith_MaterialAsset& xMaterial0 = *pxTerrain->GetMaterial0();
		Zenith_MaterialAsset& xMaterial1 = *pxTerrain->GetMaterial1();

		// Build and push terrain material constants (128 bytes) - uses scratch buffer in set 1
		TerrainMaterialPushConstants xTerrainMatConst;
		BuildTerrainMaterialPushConstants(xTerrainMatConst, &xMaterial0, &xMaterial1, dbg_bVisualizeLOD);
		xBinder.PushConstant(s_xScratchBufferBinding, &xTerrainMatConst, sizeof(xTerrainMatConst));

		// Bind LOD level buffer (per-terrain, set 1)
		xBinder.BindUAV_Buffer(s_xLODLevelBufferBinding, &pxTerrain->GetLODLevelBuffer().GetUAV());

		g_xTerrainCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&pxTerrain->GetUnifiedVertexBuffer());
		g_xTerrainCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&pxTerrain->GetUnifiedIndexBuffer());

		// Bind material textures (set 1, named bindings) - full material system (5 textures per material)
		// Material 0 textures
		xBinder.BindSRV(s_xDiffuseTex0Binding, &xMaterial0.GetDiffuseTexture()->m_xSRV);
		xBinder.BindSRV(s_xNormalTex0Binding, &xMaterial0.GetNormalTexture()->m_xSRV);
		xBinder.BindSRV(s_xRoughnessMetallicTex0Binding, &xMaterial0.GetRoughnessMetallicTexture()->m_xSRV);
		xBinder.BindSRV(s_xOcclusionTex0Binding, &xMaterial0.GetOcclusionTexture()->m_xSRV);
		xBinder.BindSRV(s_xEmissiveTex0Binding, &xMaterial0.GetEmissiveTexture()->m_xSRV);
		// Material 1 textures
		xBinder.BindSRV(s_xDiffuseTex1Binding, &xMaterial1.GetDiffuseTexture()->m_xSRV);
		xBinder.BindSRV(s_xNormalTex1Binding, &xMaterial1.GetNormalTexture()->m_xSRV);
		xBinder.BindSRV(s_xRoughnessMetallicTex1Binding, &xMaterial1.GetRoughnessMetallicTexture()->m_xSRV);
		xBinder.BindSRV(s_xOcclusionTex1Binding, &xMaterial1.GetOcclusionTexture()->m_xSRV);
		xBinder.BindSRV(s_xEmissiveTex1Binding, &xMaterial1.GetEmissiveTexture()->m_xSRV);

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

void Flux_Terrain::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
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
