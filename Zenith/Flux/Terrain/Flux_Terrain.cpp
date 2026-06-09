#include "Zenith.h"

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
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Terrain.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7h: subsystem state moved to Flux_TerrainImpl held by Zenith_Engine.

// Material-texture binding table used by ExecuteGBuffer's slot/channel loop.
// Names are codegen-emitted const-char* literals from Flux_Generated_Terrain
// so each entry has a stable pointer identity — required by Flux_ShaderBinder's
// pointer-identity name cache. Composing names at runtime into a reused
// stack buffer would alias every binding to the first cache hit.
namespace
{
	namespace TerrainShader = Flux_Generated_Terrain::Terrain_ToGBuffer;
	struct TerrainTexBinding
	{
		const char* m_szName;
		u_int       m_uMaterialSlot;
		Zenith_TextureAsset* (Zenith_MaterialAsset::*m_pfnGet)();
	};
	static const TerrainTexBinding s_axTerrainTexBindings[] = {
		// Slot 0
		{ TerrainShader::kg_xDiffuseTex0_Name,           0, &Zenith_MaterialAsset::GetDiffuseTexture           },
		{ TerrainShader::kg_xNormalTex0_Name,            0, &Zenith_MaterialAsset::GetNormalTexture            },
		{ TerrainShader::kg_xRoughnessMetallicTex0_Name, 0, &Zenith_MaterialAsset::GetRoughnessMetallicTexture },
		{ TerrainShader::kg_xOcclusionTex0_Name,         0, &Zenith_MaterialAsset::GetOcclusionTexture         },
		{ TerrainShader::kg_xEmissiveTex0_Name,          0, &Zenith_MaterialAsset::GetEmissiveTexture          },
		// Slot 1
		{ TerrainShader::kg_xDiffuseTex1_Name,           1, &Zenith_MaterialAsset::GetDiffuseTexture           },
		{ TerrainShader::kg_xNormalTex1_Name,            1, &Zenith_MaterialAsset::GetNormalTexture            },
		{ TerrainShader::kg_xRoughnessMetallicTex1_Name, 1, &Zenith_MaterialAsset::GetRoughnessMetallicTexture },
		{ TerrainShader::kg_xOcclusionTex1_Name,         1, &Zenith_MaterialAsset::GetOcclusionTexture         },
		{ TerrainShader::kg_xEmissiveTex1_Name,          1, &Zenith_MaterialAsset::GetEmissiveTexture          },
		// Slot 2
		{ TerrainShader::kg_xDiffuseTex2_Name,           2, &Zenith_MaterialAsset::GetDiffuseTexture           },
		{ TerrainShader::kg_xNormalTex2_Name,            2, &Zenith_MaterialAsset::GetNormalTexture            },
		{ TerrainShader::kg_xRoughnessMetallicTex2_Name, 2, &Zenith_MaterialAsset::GetRoughnessMetallicTexture },
		{ TerrainShader::kg_xOcclusionTex2_Name,         2, &Zenith_MaterialAsset::GetOcclusionTexture         },
		{ TerrainShader::kg_xEmissiveTex2_Name,          2, &Zenith_MaterialAsset::GetEmissiveTexture          },
		// Slot 3
		{ TerrainShader::kg_xDiffuseTex3_Name,           3, &Zenith_MaterialAsset::GetDiffuseTexture           },
		{ TerrainShader::kg_xNormalTex3_Name,            3, &Zenith_MaterialAsset::GetNormalTexture            },
		{ TerrainShader::kg_xRoughnessMetallicTex3_Name, 3, &Zenith_MaterialAsset::GetRoughnessMetallicTexture },
		{ TerrainShader::kg_xOcclusionTex3_Name,         3, &Zenith_MaterialAsset::GetOcclusionTexture         },
		{ TerrainShader::kg_xEmissiveTex3_Name,          3, &Zenith_MaterialAsset::GetEmissiveTexture          },
	};
}

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

static void ExecuteResetCounters(Flux_CommandList* pxCmdList, void* pUserData);
static void ExecuteCulling(Flux_CommandList* pxCmdList, void* pUserData);
static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void* pUserData);

void Flux_TerrainImpl::BuildPipelines()
{
	m_xTerrainGBufferShader.Initialise(FluxShaderProgram::Terrain_ToGBuffer);
	m_xTerrainShadowShader.Initialise(FluxShaderProgram::Terrain_ToShadowmap);

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

		Flux_PipelineBuilder::FromSpecification(m_xTerrainShadowPipeline, xShadowPipelineSpec);
	}

	{
		m_xWaterShader.Initialise(FluxShaderProgram::Water);

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
	m_xCullingShader.Initialise(FluxShaderProgram::TerrainCulling);

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
	m_xResetCountersShader.Initialise(FluxShaderProgram::TerrainResetCounters);
	const Flux_ShaderReflection& xResetReflection = m_xResetCountersShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(m_xResetCountersRootSig, xResetReflection);
	Flux_ComputePipelineBuilder xResetBuilder;
	xResetBuilder.WithShader(m_xResetCountersShader)
		.WithLayout(m_xResetCountersRootSig.m_xLayout)
		.Build(m_xResetCountersPipeline);
	m_xResetCountersPipeline.m_xRootSig = m_xResetCountersRootSig;
}

void Flux_TerrainImpl::Initialise(Flux_MemoryManager& xVulkanMemory, Flux_GraphicsImpl& xFluxGraphics, Zenith_Profiling& xProfiling, Flux_TerrainStreamingManagerImpl& xTerrainStreaming)
{
	// De-globalisation DI seam: store the injected cross-subsystem deps. Every
	// later reach (including from the static graph trampolines, which re-acquire
	// the singleton via g_xEngine.Terrain()) routes through these members.
	m_pxVulkanMemory     = &xVulkanMemory;
	m_pxFluxGraphics     = &xFluxGraphics;
	m_pxProfiling        = &xProfiling;
	m_pxTerrainStreaming = &xTerrainStreaming;

	BuildPipelines();

	// Take a ref-counted copy of the global water normal texture handle (set during init in Zenith_Main).
	m_xWaterNormalTexture = m_pxFluxGraphics->m_xWaterNormalTexture;

	m_pxVulkanMemory->InitialiseDynamicConstantBuffer(nullptr, sizeof(struct TerrainConstants
		), m_xTerrainConstantsBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({ "Render", "Terrain", "UV Scale" }, s_xTerrainConstants.m_fUVScale, 0., 10.);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Wireframe" }, dbg_bWireframe);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Terrain", "Visiblity Multiplier" }, dbg_fVisibilityThresholdMultiplier, 0.1f, 1.f);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Ignore Visibility Check" }, dbg_bIgnoreVisibilityCheck);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Terrain", "Debug Mode" }, dbg_uDebugMode, 0, 12);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Log Metrics" }, dbg_bLogTerrainMetrics);
#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Terrain_ToGBuffer,
		FluxShaderProgram::Terrain_ToShadowmap,
		FluxShaderProgram::TerrainCulling,
		FluxShaderProgram::TerrainResetCounters,
		FluxShaderProgram::Water,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Terrain().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	// ========== Initialize Terrain Streaming Manager ==========
	m_pxTerrainStreaming->Initialize();

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
	m_pxVulkanMemory->DestroyDynamicConstantBuffer(m_xTerrainConstantsBuffer);

	// Manager Shutdown asserts the per-terrain state registry is empty —
	// any terrain component still alive at engine teardown is a leak that
	// will trip the assert here, instead of silently freeing the manager
	// out from under live state.
	m_pxTerrainStreaming->Shutdown();

	m_pxVulkanMemory     = nullptr;
	m_pxFluxGraphics     = nullptr;
	m_pxProfiling        = nullptr;
	m_pxTerrainStreaming = nullptr;

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
		// m_xChunkDataBuffer and m_xFrustumPlanesBuffer are intentionally NOT
		// declared here: both are frame-indexed (Flux_DynamicReadWriteBuffer
		// and Flux_DynamicConstantBuffer respectively — one Flux_Buffer per
		// frame in flight). Declaring them via GetBuffer() at compile time
		// would lock the graph to frame 0's instance, but each frame's
		// compute dispatch binds a *different* instance via GetSRV() / GetCBV().
		//
		// Sync: both buffers live in HOST_VISIBLE | HOST_COHERENT memory so
		// the CPU memcpy in PreRenderUpdate becomes visible to the GPU read
		// via vkSubmit's implicit host-write-available barrier — no manual
		// TransferWrite→ShaderRead barrier is needed (that was only required
		// for the previous staged-upload path on a single shared buffer).
		// Cross-frame races on the underlying memory are eliminated by frame
		// indexing: slot K's write only ever targets slot K's buffer, which
		// the per-frame fence guarantees the GPU has finished reading.

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
	Flux_PassHandle xGBufferPass = xGraph.AddPass("Terrain GBuffer", ExecuteGBuffer)
		.Writes(m_pxFluxGraphics->GetMRTAttachment(MRT_INDEX_DIFFUSE),			RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxFluxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT),	RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxFluxGraphics->GetMRTAttachment(MRT_INDEX_MATERIAL),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxFluxGraphics->GetDepthAttachment(),						RESOURCE_ACCESS_WRITE_DSV)
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
	m_uFrameCounter++;

	// Get all terrain components
	m_xTerrainRenderRecords.Clear();
	// Wave 3: gather neutral render records EC-side (no Flux<-EntityComponent edge).
	if (g_pfnZenithTerrainGather) g_pfnZenithTerrainGather(m_xTerrainRenderRecords);

	m_pxVulkanMemory->UploadBufferData(m_xTerrainConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xTerrainConstants, sizeof(TerrainConstants));

	// ========== Per-Terrain Streaming + Chunk Data Upload ==========
	// Each terrain has its own Flux_TerrainStreamingState, so streaming
	// runs per-component. Both m_xChunkDataBuffer (frame-indexed
	// Flux_DynamicReadWriteBuffer) and m_xFrustumPlanesBuffer (frame-indexed
	// Flux_DynamicConstantBuffer) are host-visible per-frame buffers;
	// uploads happen here in the Prepare phase and become visible to the
	// compute pass via vkSubmit's implicit host-write-available barrier.
	// Frame indexing eliminates cross-frame CPU/GPU races on shared memory.
	m_pxProfiling->BeginProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING);
	const Zenith_Maths::Vector3 xCameraPos = m_pxFluxGraphics->GetCameraPosition();
	const Zenith_Maths::Matrix4& xViewProj = m_pxFluxGraphics->m_xFrameConstants.m_xViewProjMat;
	for (u_int u = 0; u < m_xTerrainRenderRecords.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = m_xTerrainRenderRecords.Get(u).m_pxState;
		m_pxTerrainStreaming->UpdateStreamingForTerrain(pxState, xCameraPos);
		m_pxTerrainStreaming->UpdateChunkLODAllocations(*pxState);
		m_pxTerrainStreaming->UploadFrustumPlanesForFrame(*pxState, xViewProj);

		// m_xChunkDataBuffer is now a frame-indexed host-visible buffer; no
		// MarkBufferHostWritten needed (vkSubmit's implicit host-write barrier
		// covers visibility, and frame indexing prevents cross-frame races).
		// See SetupRenderGraph for why it isn't in the graph at all.
	}
	m_pxProfiling->EndProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING);
}

static void ExecuteResetCounters(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	// Non-capturing graph callback (void(*)(Flux_CommandList*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Terrain() to reach the singleton
	// instance, then routes all further reaches through the instance + its
	// injected members.
	Flux_TerrainImpl& xTerrain = g_xEngine.Terrain();

	pxCmdList->AddCommand<Flux_CommandBindComputePipeline>(&xTerrain.m_xResetCountersPipeline);

	for (u_int u = 0; u < xTerrain.m_xTerrainRenderRecords.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrain.m_xTerrainRenderRecords.Get(u).m_pxState;
		if (!pxState->m_bCullingResourcesInitialized) continue;

		// Bind set 0, slot 0: visibleCount UAV. Dispatch a single thread that
		// writes 0u. The graph emits a UAV→UAV barrier between this pass and
		// the culling pass, so the culling dispatch's atomic increments see
		// the cleared value.
		pxCmdList->AddCommand<Flux_CommandBindUAV_Buffer>(&pxState->m_xVisibleCountBuffer.GetUAV(), Flux_BindingSlot{ 0, 0, true });
		pxCmdList->AddCommand<Flux_CommandDispatch>(1, 1, 1);
	}
}

static void ExecuteCulling(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	// Non-capturing graph callback — re-acquire the singleton first, then route
	// the Profiling reaches through its injected member.
	Flux_TerrainImpl& xTerrain = g_xEngine.Terrain();

	xTerrain.m_pxProfiling->BeginProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING);

	// Bind the terrain culling compute pipeline once (owned by Flux_Terrain)
	pxCmdList->AddCommand<Flux_CommandBindComputePipeline>(&xTerrain.m_xCullingPipeline);

	// For each terrain component, dispatch culling using its own buffers
	for (u_int u = 0; u < xTerrain.m_xTerrainRenderRecords.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrain.m_xTerrainRenderRecords.Get(u).m_pxState;

		// Record buffer bindings + dispatch (pipeline already bound; frustum + visible-count upstream).
		xTerrain.m_pxTerrainStreaming->UpdateCullingAndLod(*pxState, *pxCmdList);
	}

	xTerrain.m_pxProfiling->EndProfile(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING);
}

static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTerrainEnabled)
	{
		return;
	}

	// Non-capturing graph callback — re-acquire the singleton first, then route
	// the FluxGraphics reaches through its injected member + the fallback
	// splatmap through the promoted member helper.
	Flux_TerrainImpl& xTerrain = g_xEngine.Terrain();

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(dbg_bWireframe ? &xTerrain.m_xTerrainWireframePipeline : &xTerrain.m_xTerrainGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Bind set 0 (per-frame data) once per command list
	xBinder.BindCBV(xTerrain.m_xTerrainGBufferShader, "FrameConstants", &xTerrain.m_pxFluxGraphics->m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindCBV(xTerrain.m_xTerrainGBufferShader, "TerrainConstants", &xTerrain.m_xTerrainConstantsBuffer.GetCBV());

	for (u_int u = 0; u < xTerrain.m_xTerrainRenderRecords.GetSize(); u++)
	{
		const Flux_TerrainRenderRecord& xRec = xTerrain.m_xTerrainRenderRecords.Get(u);
		Flux_TerrainStreamingState* const pxState = xRec.m_pxState;
		if(!pxState->m_xUnifiedVertexBuffer.GetBuffer().m_ulSize) continue;

		Zenith_MaterialAsset* apxMaterials[4] = { xRec.m_apxMaterials[0], xRec.m_apxMaterials[1], xRec.m_apxMaterials[2], xRec.m_apxMaterials[3] };

		// Build and push terrain material constants (288 bytes) - uses scratch buffer in set 1
		TerrainMaterialDrawConstants xTerrainMatConst;
		BuildTerrainMaterialDrawConstants(xTerrainMatConst, apxMaterials, 4, dbg_uDebugMode,
			0.0f, 0.0f, Flux_TerrainConfig::TERRAIN_SIZE, Flux_TerrainConfig::TERRAIN_SIZE);
		xBinder.BindDrawConstants(xTerrain.m_xTerrainGBufferShader, "TerrainMaterialConstants", &xTerrainMatConst, sizeof(xTerrainMatConst));

		// Bind LOD level buffer (per-terrain, set 1). The shader declares this
		// as StructuredBuffer<uint> (read-only — see Generated/Terrain.h
		// kLODLevelBuffer kind: StructuredBuffer); route through BindSRV_Buffer
		// so the render-graph declaration RESOURCE_ACCESS_READ_BUFFER_SRV
		// matches the bind direction.
		xBinder.BindSRV_Buffer(xTerrain.m_xTerrainGBufferShader, "LODLevelBuffer", pxState->m_xLODLevelBuffer.GetSRV());

		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxState->m_xUnifiedVertexBuffer);
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxState->m_xUnifiedIndexBuffer);

		// Bind splatmap texture — always bound (Vulkan rejects an unbound
		// SRV slot the shader is declared to read). Falls back to the 1x1
		// "material 0 only" texture when the component has no splatmap.
		Zenith_TextureAsset* pxSplatmap = xRec.m_pxSplatmap;
		xBinder.BindSRV(xTerrain.m_xTerrainGBufferShader, "g_xSplatmap",
			pxSplatmap ? &pxSplatmap->m_xSRV : &xTerrain.GetFallbackSplatmapSRV());

		// Bind material textures (set 1, named bindings) — 4 slots × 5 channels.
		// Per-channel defaulting (white / normal-up) is already handled by
		// Zenith_MaterialAsset::GetXxxTexture(); the fallback here is at the
		// slot level: a null material falls back to FluxGraphics().m_xBlankMaterial,
		// whose channel getters return the engine-wide defaults. The binding
		// table at file scope (s_axTerrainTexBindings) holds stable codegen
		// name pointers — see comment there for why.
		Flux_GraphicsImpl* const pxFluxGraphics = xTerrain.m_pxFluxGraphics;
		auto ResolveSRV = [pxFluxGraphics](Zenith_MaterialAsset* pxMat,
			Zenith_TextureAsset* (Zenith_MaterialAsset::*pfn)()) -> const Flux_ShaderResourceView*
		{
			Zenith_MaterialAsset* pxResolved = pxMat ? pxMat : pxFluxGraphics->m_xBlankMaterial.GetDirect();
			Zenith_Assert(pxResolved != nullptr, "FluxGraphics().m_xBlankMaterial not initialised — FluxGraphics().Initialise must run before terrain renders");
			Zenith_TextureAsset* pxTex = (pxResolved->*pfn)();
			Zenith_Assert(pxTex != nullptr, "Material channel getter returned null — Zenith_MaterialAsset defaults should guarantee non-null");
			return &pxTex->m_xSRV;
		};

		for (const TerrainTexBinding& xB : s_axTerrainTexBindings)
		{
			xBinder.BindSRV(
				xTerrain.m_xTerrainGBufferShader,
				xB.m_szName,
				ResolveSRV(apxMaterials[xB.m_uMaterialSlot], xB.m_pfnGet));
		}

		// GPU-driven indirect rendering with front-to-back sorted visible chunks
		// Each component uses its own indirect draw buffer and visible count buffer
		pxCmdList->AddCommand<Flux_CommandDrawIndexedIndirectCount>(
			&pxState->m_xIndirectDrawBuffer,  // Per-component indirect buffer with sorted draw commands
			&pxState->m_xVisibleCountBuffer,   // Per-component count buffer with actual number of visible chunks
			Flux_TerrainConfig::TOTAL_CHUNKS,          // Max 4096 draws (theoretical maximum)
			0,                                      // Indirect buffer offset (bytes)
			0,                                      // Count buffer offset (bytes)
			20                                      // Stride between commands (5 * sizeof(uint32_t))
		);

	}
}

void Flux_TerrainImpl::RenderToShadowMap(Flux_CommandList&, const Flux_DynamicConstantBuffer&)
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
