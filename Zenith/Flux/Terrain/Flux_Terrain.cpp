#include "Zenith.h"
#include "Flux/Terrain/Flux_Terrain_Shaders.h"

#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_BackendTypes.h"
#include "Profiling/Zenith_Profiling.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
// Zenith_Query.h arrives transitively via Zenith_SceneSystem.h (QueryAllScenes needs it);
// including it explicitly here would add a new EC<->Flux cross-layer edge.
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Flux_MaterialTable.h"   // Phase 4c: terrain registers its 4 layer materials into the GPU table
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Terrain.h"

// Phase 7h: subsystem state moved to Flux_TerrainImpl held by Zenith_Engine.

// Phase 4c: the per-slot material-texture binding table (s_axTerrainTexBindings,
// 4 slots x 5 channels) was deleted — terrain layer textures are now bindless,
// sampled in the shader via g_axTextures[g_axMaterials[slotIdx].<map>Idx]. The 4
// slots' material-table indices are resolved on the main thread and ride the
// terrain material constants.

// Fallback splatmap used when a Zenith_TerrainComponent has no splatmap
// texture set. 1x1 RGBA8 with R=255 (full weight on material slot 0), other
// channels zero — terrain renders entirely from material 0, which mirrors
// the legacy "splatmap absent → use base material only" behaviour without
// leaving the descriptor unbound (Vulkan validation rejects an unbound
// SRV slot the shader is declared to read).
// Pinned via TextureHandle so UnloadUnused never frees the fallback splatmap mid-frame.
// (Owned by Flux_TerrainImpl as m_xFallbackSplatmap.)

const Flux_ShaderResourceView& Flux_TerrainImpl::GetFallbackSplatmapSRV()
{
	if (!m_xFallbackSplatmap)
	{
		const u_int8 aucRGBA[4] = { 255u, 0u, 0u, 0u };
		Flux_SurfaceInfo xInfo;
		xInfo.m_eFormat       = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_uWidth        = 1;
		xInfo.m_uHeight       = 1;
		xInfo.m_uDepth        = 1;
		xInfo.m_uNumMips      = 1;
		xInfo.m_uNumLayers    = 1;
		xInfo.m_uMemoryFlags  = 1u << MEMORY_FLAGS__SHADER_READ;

		Zenith_TextureAsset* pxFallback = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
		Zenith_Assert(pxFallback != nullptr, "Failed to create terrain fallback splatmap texture asset");
		pxFallback->CreateFromData(aucRGBA, xInfo, /*bCreateMips*/ false);
		m_xFallbackSplatmap.Set(pxFallback);
	}
	return m_xFallbackSplatmap.GetDirect()->m_xSRV;
}

// GPU-Driven Terrain Culling Pipeline + ResetCounters + per-frame stats all
// owned by Flux_TerrainImpl.

// Sized to match the reflected std140 / Vulkan-uniform-block layout from
// Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB. The actual
// CB on the GPU is 16-byte aligned even though the only field is a single
// float; allocating a 4-byte CPU struct and uploading sizeof(it) under-fills
// the GPU descriptor and reads garbage on the GPU side. The static_asserts
// below pin this in lockstep with the codegen header.
struct TerrainConstants
{
	float m_fUVScale = 0.07f;
	float m_afPad[3] = { 0.0f, 0.0f, 0.0f };
} s_xTerrainConstants;
static_assert(sizeof(TerrainConstants) == sizeof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB),
	"TerrainConstants CPU size != reflected CB size — regenerate codegen or update padding");
static_assert(offsetof(TerrainConstants, m_fUVScale) == 0,
	"TerrainConstants.m_fUVScale must remain at offset 0 to match the reflected layout");

bool dbg_bWireframe = false;
DEBUGVAR float dbg_fVisibilityThresholdMultiplier = 0.5f;
DEBUGVAR bool dbg_bIgnoreVisibilityCheck = false;
DEBUGVAR bool dbg_bLogTerrainMetrics = false;  // Log terrain performance metrics
u_int dbg_uDebugMode = 0;  // Debug visualization mode (0=Off, 1=LOD, 2=Normals, 3=UVs, etc.)

static void ExecuteResetCounters(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteCulling(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteGBuffer(Flux_CommandBuffer* pxCmdList, void* pUserData);

void Flux_TerrainImpl::BuildPipelines()
{
	m_xTerrainGBufferShader.Initialise(Flux_TerrainShaders::xTerrain_ToGBuffer);
	m_xTerrainShadowShader.Initialise(Flux_TerrainShaders::xTerrain_ToShadowmap);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);           // Position (12 bytes)
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);             // UV (8 bytes — FLOAT2 not HALF2: HALF mantissa is too small for heightmap-pixel-scale UVs above 2048, which collapses adjacent vertex UVs and shows up as a vertex-spacing-period strip pattern in the diffuse)
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_SNORM10_10_10_2);   // Normal (4 bytes)
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_SNORM10_10_10_2);   // Tangent + BitangentSign (4 bytes)
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE] = MRT_FORMAT_EMISSIVE;
		xPipelineSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &m_xTerrainGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		m_xTerrainGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(m_xTerrainGBufferPipeline, xPipelineSpec);

		xPipelineSpec.m_bWireframe = true;
		Flux_PipelineBuilder::FromSpecification(m_xTerrainWireframePipeline, xPipelineSpec);
	}


	{
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &m_xTerrainShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		m_xTerrainShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		xShadowPipelineSpec.m_bDepthTestEnabled = true;
		xShadowPipelineSpec.m_bDepthWriteEnabled = true;
		xShadowPipelineSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;

		// Fixed-function slope-scaled depth bias (set per-cascade via vkCmdSetDepthBias
		// in Flux_Shadows::ExecuteShadowCascade). Dynamic so it is runtime-tunable.
		// (Inert until terrain shadow casting is enabled — RenderToShadowMap is stubbed.)
		xShadowPipelineSpec.m_bDepthBias = true;
		xShadowPipelineSpec.m_bDynamicDepthBias = true;
		xShadowPipelineSpec.m_fDepthBiasConstant = 1.75f;
		xShadowPipelineSpec.m_fDepthBiasSlope = 3.0f;

		Flux_PipelineBuilder::FromSpecification(m_xTerrainShadowPipeline, xShadowPipelineSpec);
	}

	{
		m_xWaterShader.Initialise(Flux_TerrainShaders::xWater);

		Flux_VertexInputDescription xWaterVertexDesc;
		xWaterVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
		xWaterVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
		xWaterVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
		xWaterVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[0] = FINAL_RT_FORMAT;
		xPipelineSpec.m_uNumColourAttachments = 1;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &m_xWaterShader;
		xPipelineSpec.m_xVertexInputDesc = xWaterVertexDesc;

		m_xWaterShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		xPipelineSpec.m_bDepthWriteEnabled = false;

		Flux_PipelineBuilder::FromSpecification(m_xWaterPipeline, xPipelineSpec);
	}

	// ========== GPU-Driven Terrain Culling Compute Pipeline ==========
	m_xCullingShader.Initialise(Flux_TerrainShaders::xTerrainCulling);

	// Build compute root signature from shader reflection
	const Flux_ShaderReflection& xCullingReflection = m_xCullingShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(m_xCullingRootSig, xCullingReflection);

	// Build compute pipeline
	Flux_ComputePipelineBuilder xCullingBuilder;
	xCullingBuilder.WithShader(m_xCullingShader)
		.WithLayout(m_xCullingRootSig.m_xLayout)
		.Build(m_xCullingPipeline);

	m_xCullingPipeline.m_xRootSig = m_xCullingRootSig;

	// ========== Visible-Count Reset Compute Pipeline ==========
	// Single dispatch, single thread. See the slang module for rationale.
	m_xResetCountersShader.Initialise(Flux_TerrainShaders::xTerrainResetCounters);
	const Flux_ShaderReflection& xResetReflection = m_xResetCountersShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(m_xResetCountersRootSig, xResetReflection);
	Flux_ComputePipelineBuilder xResetBuilder;
	xResetBuilder.WithShader(m_xResetCountersShader)
		.WithLayout(m_xResetCountersRootSig.m_xLayout)
		.Build(m_xResetCountersPipeline);
	m_xResetCountersPipeline.m_xRootSig = m_xResetCountersRootSig;
}

void Flux_TerrainImpl::Initialise()
{
	BuildPipelines();

	// Take a ref-counted copy of the global water normal texture handle (set during init in Zenith_Main).
	m_xWaterNormalTexture = g_xEngine.FluxGraphics().m_xWaterNormalTexture;

	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(nullptr, sizeof(struct TerrainConstants
		), m_xTerrainConstantsBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({ "Render", "Terrain", "UV Scale" }, s_xTerrainConstants.m_fUVScale, 0., 10.);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Wireframe" }, dbg_bWireframe);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Terrain", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Ignore Visibility Check" }, dbg_bIgnoreVisibilityCheck);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Terrain", "Debug Mode" }, dbg_uDebugMode, 0, 12);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Log Metrics" }, dbg_bLogTerrainMetrics);
#endif

	// ========== Initialize Terrain Streaming Manager ==========
	g_xEngine.TerrainStreaming().Initialize();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain initialised");
}

void Flux_TerrainImpl::Reset()
{
	// Reset is handled by the render graph
	// Clear cached terrain components (will be repopulated next frame)
	m_xTerrainRenderRecords.Clear();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainImpl::Reset()");
}

void Flux_TerrainImpl::ReleaseAssetReferences()
{
	m_xWaterNormalTexture.Clear();
	m_xFallbackSplatmap.Clear();
}

void Flux_TerrainImpl::Shutdown()
{
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_xTerrainConstantsBuffer);

	// Manager Shutdown asserts the per-terrain state registry is empty —
	// any terrain component still alive at engine teardown is a leak that
	// will trip the assert here, instead of silently freeing the manager
	// out from under live state.
	g_xEngine.TerrainStreaming().Shutdown();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_Terrain shut down");
}

void Flux_TerrainImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Gather all live terrain components ahead of declaring per-pass resource
	// usage. SetupRenderGraph re-runs whenever the graph rebuilds (Flux::
	// RequestGraphRebuild on terrain construct/destroy), so the registry walked
	// here always reflects the current scene's terrain set.
	Zenith_Vector<Flux_TerrainRenderRecord> xTerrains;
	if (g_pfnZenithTerrainGather) g_pfnZenithTerrainGather(xTerrains);

	// Pass 0: Reset visible-count buffers. One dispatch per terrain, each
	// writes a single uint32 to the corresponding visible-count buffer. The
	// culling pass DependsOn this pass and re-declares each buffer as a UAV
	// write — the graph synthesises a UAV→UAV barrier between the two so the
	// culling dispatch's atomic increments observe the cleared value.
	Flux_PassHandle xResetPass = xGraph.AddPass("Terrain Reset Counters", ExecuteResetCounters);
	for (u_int u = 0; u < xTerrains.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrains.Get(u).m_pxState;
		if (!pxState->m_bCullingResourcesInitialized) continue;
		xGraph.WriteBuffer(xResetPass, pxState->m_xVisibleCountBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
	}

	// Pass 1: Terrain culling compute. PreRenderUpdate runs as a Prepare
	// callback on this pass — it's the actual consumer of streaming + chunk
	// data uploads, so the dependency reads correctly here (was previously
	// attached to the GBuffer pass, where it happened to work because all
	// Prepare callbacks fire before any pass records).
	Flux_PassHandle xCullingPass = xGraph.AddPass("Terrain Culling Compute", ExecuteCulling)
		.Prepare([](void* p){ g_xEngine.Terrain().PreRenderUpdate(p); })
		.DependsOn(xResetPass);

	for (u_int u = 0; u < xTerrains.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrains.Get(u).m_pxState;
		if (!pxState->m_bCullingResourcesInitialized) continue;
		// m_xChunkDataBuffer (frame-indexed RW) and m_xFrustumPlanesBuffer
		// (frame-indexed constant) are intentionally NOT declared to the graph —
		// each frame's compute dispatch binds a different instance via
		// GetSRV()/GetCBV(). See the RENDER-GRAPH CONTRACT on
		// Flux_FrameIndexedBufferBase (Flux_Buffers.h).

		// Indirect command + visible-count + LOD-level buffers are produced by
		// this pass. LODLevelBuffer is now a read-modify-write (the GPU LOD
		// hysteresis check reads the prior frame's value before writing) so it
		// is declared READWRITE_UAV.
		xGraph.WriteBuffer(xCullingPass, pxState->m_xIndirectDrawBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
		xGraph.WriteBuffer(xCullingPass, pxState->m_xVisibleCountBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
		xGraph.WriteBuffer(xCullingPass, pxState->m_xLODLevelBuffer.GetBuffer(),     RESOURCE_ACCESS_READWRITE_UAV);
	}

	// Pass 2: Terrain GBuffer. The DependsOn(xCullingPass) edge documents
	// intent; the buffer Read declarations below also implicitly schedule the
	// pass after culling and let the graph synthesise the correct memory +
	// pipeline-stage barriers between the compute writes and these reads.
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_PassHandle xGBufferPass = xGraph.AddPass("Terrain GBuffer", ExecuteGBuffer)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),			RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT),	RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),						RESOURCE_ACCESS_WRITE_DSV)
		.DependsOn(xCullingPass);

	for (u_int u = 0; u < xTerrains.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrains.Get(u).m_pxState;
		if (!pxState->m_bCullingResourcesInitialized) continue;
		// DrawIndexedIndirectCount reads the indirect-args buffer and the
		// count buffer at the GPU command-processor stage; LODLevelBuffer is
		// sampled in the vertex shader as StructuredBuffer<uint> (read-only).
		xGraph.ReadBuffer(xGBufferPass, pxState->m_xIndirectDrawBuffer.GetBuffer(), RESOURCE_ACCESS_READ_INDIRECT_ARG);
		xGraph.ReadBuffer(xGBufferPass, pxState->m_xVisibleCountBuffer.GetBuffer(), RESOURCE_ACCESS_READ_INDIRECT_ARG);
		xGraph.ReadBuffer(xGBufferPass, pxState->m_xLODLevelBuffer.GetBuffer(),     RESOURCE_ACCESS_READ_BUFFER_SRV);
	}
}

void Flux_TerrainImpl::PreRenderUpdate(void* /*pUserData*/)
{
	// Get all terrain components
	m_xTerrainRenderRecords.Clear();
	// Wave 3: gather neutral render records EC-side (no Flux<-EntityComponent edge).
	if (g_pfnZenithTerrainGather) g_pfnZenithTerrainGather(m_xTerrainRenderRecords);

	// Phase 4c: register each terrain layer material into the GPU material table
	// (MAIN THREAD — GetOrCreateIndex mutates the index allocator + writes bindless
	// descriptors). A null slot resolves to the engine blank material so its
	// per-channel defaults (white albedo / flat normal / ...) get real bindless
	// indices, matching the pre-4c blank-material fallback. The resolved indices
	// are stored on the record for the worker ExecuteGBuffer to read lock-free.
	{
		Flux_MaterialTable& xMatTable = g_xEngine.FluxGraphics().MaterialTable();
		Zenith_MaterialAsset* pxBlank = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();
		for (u_int u = 0; u < m_xTerrainRenderRecords.GetSize(); u++)
		{
			Flux_TerrainRenderRecord& xRec = m_xTerrainRenderRecords.Get(u);
			for (u_int s = 0; s < 4; s++)
			{
				Zenith_MaterialAsset* pxMat = xRec.m_apxMaterials[s] ? xRec.m_apxMaterials[s] : pxBlank;
				xRec.m_auMaterialTableIndices[s] = xMatTable.GetOrCreateIndex(pxMat);
			}
		}
	}

	g_xEngine.FluxMemory().UploadBufferData(m_xTerrainConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xTerrainConstants, sizeof(TerrainConstants));

	// ========== Per-Terrain Streaming + Chunk Data Upload ==========
	// Each terrain has its own Flux_TerrainStreamingState, so streaming runs
	// per-component. The chunk-data + frustum-planes buffers are frame-indexed
	// host-visible buffers uploaded here in the Prepare phase (not graph-tracked —
	// see the RENDER-GRAPH CONTRACT on Flux_FrameIndexedBufferBase, Flux_Buffers.h).
	g_xEngine.Profiling().BeginProfileZone(ZENITH_PROFILE_ZONE("Flux Terrain Streaming"));
	const Zenith_Maths::Vector3 xCameraPos = g_xEngine.FluxGraphics().GetCameraPosition();
	const Zenith_Maths::Matrix4& xViewProj = g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewProjMat;
	Flux_TerrainStreamingManagerImpl& xTerrainStreaming = g_xEngine.TerrainStreaming();
	for (u_int u = 0; u < m_xTerrainRenderRecords.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = m_xTerrainRenderRecords.Get(u).m_pxState;
		xTerrainStreaming.UpdateStreamingForTerrain(pxState, xCameraPos);
		xTerrainStreaming.UpdateChunkLODAllocations(*pxState);
		xTerrainStreaming.UploadFrustumPlanesForFrame(*pxState, xViewProj);
		// No MarkBufferHostWritten: frame-indexed buffers aren't graph-tracked
		// (see Flux_FrameIndexedBufferBase contract).
	}
	g_xEngine.Profiling().EndProfileZone(ZENITH_PROFILE_ZONE("Flux Terrain Streaming"));
}

static void ExecuteResetCounters(Flux_CommandBuffer* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Terrain() to reach the singleton
	// instance; cross-subsystem deps are reached via g_xEngine at point of use.
	Flux_TerrainImpl& xTerrain = g_xEngine.Terrain();

	pxCmdList->BindComputePipeline(&xTerrain.m_xResetCountersPipeline);

	for (u_int u = 0; u < xTerrain.m_xTerrainRenderRecords.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrain.m_xTerrainRenderRecords.Get(u).m_pxState;
		if (!pxState->m_bCullingResourcesInitialized) continue;

		// Bind set 0, slot 0: visibleCount UAV. Dispatch a single thread that
		// writes 0u. The graph emits a UAV→UAV barrier between this pass and
		// the culling pass, so the culling dispatch's atomic increments see
		// the cleared value.
		Flux_ShaderBinder xBinder(*pxCmdList);
		xBinder.BindUAV_Buffer(Flux_Generated_Terrain::TerrainResetCounters::hvisibleCount, &pxState->m_xVisibleCountBuffer.GetUAV());
		pxCmdList->Dispatch(1, 1, 1);
	}
}

static void ExecuteCulling(Flux_CommandBuffer* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	// Non-capturing graph callback — re-acquire the singleton first; Profiling
	// is reached via g_xEngine at point of use.
	Flux_TerrainImpl& xTerrain = g_xEngine.Terrain();

	g_xEngine.Profiling().BeginProfileZone(ZENITH_PROFILE_ZONE("Flux Terrain Culling"));

	// Bind the terrain culling compute pipeline once (owned by Flux_Terrain)
	pxCmdList->BindComputePipeline(&xTerrain.m_xCullingPipeline);

	// For each terrain component, dispatch culling using its own buffers
	for (u_int u = 0; u < xTerrain.m_xTerrainRenderRecords.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrain.m_xTerrainRenderRecords.Get(u).m_pxState;

		// Record buffer bindings + dispatch (pipeline already bound; frustum + visible-count upstream).
		g_xEngine.TerrainStreaming().UpdateCullingAndLod(*pxState, *pxCmdList);
	}

	g_xEngine.Profiling().EndProfileZone(ZENITH_PROFILE_ZONE("Flux Terrain Culling"));
}

static void ExecuteGBuffer(Flux_CommandBuffer* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	// Non-capturing graph callback — re-acquire the singleton first; FluxGraphics
	// is reached via g_xEngine at point of use, the fallback splatmap through the
	// promoted member helper.
	Flux_TerrainImpl& xTerrain = g_xEngine.Terrain();

	pxCmdList->SetPipeline(dbg_bWireframe ? &xTerrain.m_xTerrainWireframePipeline : &xTerrain.m_xTerrainGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Typed binding handles for the Terrain_ToGBuffer program (m_xTerrainGBufferShader
	// was Initialised from Flux_TerrainShaders::xTerrain_ToGBuffer).
	namespace TGB = Flux_Generated_Terrain::Terrain_ToGBuffer;

	// Spine: the camera matrix comes from the VIEW set (set 1) g_xView, sourced
	// from m_xViewConstantsBuffer (was the old per-frame FrameConstants bind).
	// The GBuffer shader reads only the camera (no sun/time), so only g_xView is
	// bound here. TerrainConstants (per-frame UV scale) is now a PassParams member.
	xBinder.BindCBV(TGB::hg_xView, &g_xEngine.FluxGraphics().m_xViewConstantsBuffer.GetCBV());
	xBinder.BindCBV(TGB::hTerrainConstants, &xTerrain.m_xTerrainConstantsBuffer.GetCBV());

	// Phase 4c: bindless terrain layer textures via the material table. g_axMaterials is in
	// the persistent GLOBAL set (set 0, Phase 5.3); the per-slot material indices ride the
	// terrain material constants. UseBindlessTextures(2) binds the g_axTextures table.
	pxCmdList->UseBindlessTextures(2);

	for (u_int u = 0; u < xTerrain.m_xTerrainRenderRecords.GetSize(); u++)
	{
		const Flux_TerrainRenderRecord& xRec = xTerrain.m_xTerrainRenderRecords.Get(u);
		Flux_TerrainStreamingState* const pxState = xRec.m_pxState;
		if(!pxState->m_xUnifiedVertexBuffer.GetBuffer().m_ulSize) continue;

		Zenith_MaterialAsset* apxMaterials[4] = { xRec.m_apxMaterials[0], xRec.m_apxMaterials[1], xRec.m_apxMaterials[2], xRec.m_apxMaterials[3] };

		// Build and push terrain material constants - uses scratch buffer in set 3.
		TerrainMaterialDrawConstants xTerrainMatConst;
		BuildTerrainMaterialDrawConstants(xTerrainMatConst, apxMaterials, 4, dbg_uDebugMode,
			0.0f, 0.0f, Flux_TerrainConfig::TERRAIN_SIZE, Flux_TerrainConfig::TERRAIN_SIZE);
		// Phase 4c: the per-slot GPU material-table indices (resolved on the main
		// thread in PreRenderUpdate). The shader loads g_axMaterials[idx] per slot
		// and samples its bindless texture indices from g_axTextures.
		for (u_int s = 0; s < 4; s++) xTerrainMatConst.m_auMaterialTableIndices[s] = xRec.m_auMaterialTableIndices[s];
		xBinder.BindDrawConstants(TGB::hTerrainMaterialConstants, &xTerrainMatConst, sizeof(xTerrainMatConst));

		// Bind LOD level buffer (per-terrain, set 1). The shader declares this
		// as StructuredBuffer<uint> (read-only — see Generated/Terrain.h
		// kLODLevelBuffer kind: StructuredBuffer); route through BindSRV_Buffer
		// so the render-graph declaration RESOURCE_ACCESS_READ_BUFFER_SRV
		// matches the bind direction.
		xBinder.BindSRV_Buffer(TGB::hLODLevelBuffer, pxState->m_xLODLevelBuffer.GetSRV());

		pxCmdList->SetVertexBuffer(pxState->m_xUnifiedVertexBuffer);
		pxCmdList->SetIndexBuffer(pxState->m_xUnifiedIndexBuffer);

		// Bind splatmap texture — always bound (Vulkan rejects an unbound
		// SRV slot the shader is declared to read). Falls back to the 1x1
		// "material 0 only" texture when the component has no splatmap.
		Zenith_TextureAsset* pxSplatmap = xRec.m_pxSplatmap;
		xBinder.BindSRV(TGB::hg_xSplatmap,
			pxSplatmap ? &pxSplatmap->m_xSRV : &xTerrain.GetFallbackSplatmapSRV());

		// Phase 4c: the 4 slots' material textures are now bindless — sampled in
		// the shader via g_axTextures[g_axMaterials[idx].<map>Idx]. No per-channel
		// bind loop; the indices were resolved on the main thread (null slots ->
		// the engine blank material, whose record carries the default-channel
		// bindless indices).

		// GPU-driven indirect rendering with front-to-back sorted visible chunks
		// Each component uses its own indirect draw buffer and visible count buffer
		pxCmdList->DrawIndexedIndirectCount(
			&pxState->m_xIndirectDrawBuffer,  // Per-component indirect buffer with sorted draw commands
			&pxState->m_xVisibleCountBuffer,   // Per-component count buffer with actual number of visible chunks
			Flux_TerrainConfig::TOTAL_CHUNKS,          // Max 4096 draws (theoretical maximum)
			0,                                      // Indirect buffer offset (bytes)
			0,                                      // Count buffer offset (bytes)
			20                                      // Stride between commands (5 * sizeof(uint32_t))
		);

	}
}

void Flux_TerrainImpl::RenderToShadowMap(Flux_CommandBuffer&, const Flux_ShaderResourceView_Buffer&, u_int)
{
	STUBBED
}




u_int& Flux_TerrainImpl::GetDebugMode()
{
	return dbg_uDebugMode;
}

bool& Flux_TerrainImpl::GetWireframeMode()
{
	return dbg_bWireframe;
}
