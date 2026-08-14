#include "Zenith.h"
#include "Flux/Terrain/Flux_Terrain_Shaders.h"

#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Terrain/Flux_TerrainPipelineSelect.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"

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
#include "Flux/Terrain/Flux_TerrainVertexQuant.h"   // the authored dequant box the terrain CB carries
#include "Flux/Shaders/Generated/Terrain.h"
// Phase 1 of the terrain indirect-count compatibility plan: the terrain call
// site uses the shared 20-byte / five-word indirect-command ABI constants and
// passes ZERO_PADDED_TO_MAX (the GPU reset pass + per-frame zero-tail invariant
// makes a fixed indexed-indirect draw over the full range valid).
#include "Flux/Backend/Flux_IndirectDraw.h"

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

		auto xhFallback = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
		Zenith_TextureAsset* pxFallback = xhFallback.GetDirect();
		Zenith_Assert(pxFallback != nullptr, "Failed to create terrain fallback splatmap texture asset");
		pxFallback->CreateFromData(aucRGBA, xInfo, /*bCreateMips*/ false);
		m_xFallbackSplatmap.Set(pxFallback);
	}
	return m_xFallbackSplatmap.GetDirect()->m_xSRV;
}

// GPU-Driven Terrain Culling Pipeline + ResetCounters + per-frame stats all
// owned by Flux_TerrainImpl.

// Sized to match the reflected std140 / Vulkan-uniform-block layout from
// Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB. std140 puts each
// float4 on a 16-byte boundary, so the leading scalar carries 12 bytes of pad;
// allocating a tighter CPU struct and uploading sizeof(it) under-fills the GPU
// descriptor and reads garbage on the GPU side. The static_asserts below pin this
// in lockstep with the codegen header.
//
// The quant lanes are FILLED IN Initialise from Flux_MakeTerrainPosQuant, not spelled
// here: the box is a constant of the baked-chunk file format, and the exporter, the
// live-edit hooks and this upload must all read it from the one helper or a chunk
// decodes somewhere the shader is not looking.
struct TerrainConstants
{
	float m_fUVScale = 0.07f;
	float m_afPad[3] = { 0.0f, 0.0f, 0.0f };
	float m_afPosQuantScale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };   // xyz = box extent, w = UV dequant scale
	float m_afPosQuantBias[4] = { 0.0f, 0.0f, 0.0f, 0.0f };    // xyz = box min
} s_xTerrainConstants;
static_assert(sizeof(TerrainConstants) == sizeof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB),
	"TerrainConstants CPU size != reflected CB size — regenerate codegen or update padding");
static_assert(offsetof(TerrainConstants, m_fUVScale) == 0,
	"TerrainConstants.m_fUVScale must remain at offset 0 to match the reflected layout");
static_assert(offsetof(TerrainConstants, m_afPosQuantScale) == 16 && offsetof(TerrainConstants, m_afPosQuantBias) == 32,
	"TerrainConstants dequant lanes must sit on the std140 float4 boundaries the shader reads them from");

// The velocity and shadow TerrainConstants blocks are HAND-MAINTAINED COPIES in
// their own .slang files (same hazard as the VsIn copies, pinned in
// Flux_TerrainStreamingManager.cpp): each generated mirror is emitted from its OWN
// program's reflection, so its sizeof/offsetof asserts stay green even when the
// three copies drift apart — and the upload above writes the ToGBuffer field order
// for every pipeline variant. TAA ships ON, so the VELOCITY copy is the one every
// default frame actually dequantises with; a member swap there would move terrain
// only when TAA is on, with nothing to fail the build. Offsets move on any reorder,
// which is what these catch (a same-offset retype is caught by the size pins).
static_assert(
	sizeof(Flux_Generated_Terrain::Terrain_ToGBufferVelocity::TerrainConstants_CB) ==
		sizeof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB) &&
	offsetof(Flux_Generated_Terrain::Terrain_ToGBufferVelocity::TerrainConstants_CB, m_fg_fUVScale) ==
		offsetof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB, m_fg_fUVScale) &&
	offsetof(Flux_Generated_Terrain::Terrain_ToGBufferVelocity::TerrainConstants_CB, m_ag_xPosQuantScale) ==
		offsetof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB, m_ag_xPosQuantScale) &&
	offsetof(Flux_Generated_Terrain::Terrain_ToGBufferVelocity::TerrainConstants_CB, m_ag_xPosQuantBias) ==
		offsetof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB, m_ag_xPosQuantBias),
	"Terrain_ToGBufferVelocity's hand-copied TerrainConstants has drifted from Terrain_ToGBuffer's — "
	"the one CPU upload would feed the DEFAULT (TAA-on) pipeline a differently-laid-out box");
static_assert(
	sizeof(Flux_Generated_Terrain::Terrain_ToShadowmap::TerrainConstants_CB) ==
		sizeof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB) &&
	offsetof(Flux_Generated_Terrain::Terrain_ToShadowmap::TerrainConstants_CB, m_fg_fUVScale) ==
		offsetof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB, m_fg_fUVScale) &&
	offsetof(Flux_Generated_Terrain::Terrain_ToShadowmap::TerrainConstants_CB, m_ag_xPosQuantScale) ==
		offsetof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB, m_ag_xPosQuantScale) &&
	offsetof(Flux_Generated_Terrain::Terrain_ToShadowmap::TerrainConstants_CB, m_ag_xPosQuantBias) ==
		offsetof(Flux_Generated_Terrain::Terrain_ToGBuffer::TerrainConstants_CB, m_ag_xPosQuantBias),
	"Terrain_ToShadowmap's hand-copied TerrainConstants has drifted from Terrain_ToGBuffer's — "
	"the shadow caster would dequantise the terrain against a differently-laid-out box when enabled");

// Pin Flux_TerrainPipelineSelect.h's dependency-free attachment counts to the real MRT
// constants (that header is deliberately <cstdint>-only so it stays unit-testable). The
// middle two are the load-bearing pair: they say the WIREFRAME axis cannot move the
// framebuffer shape, which is what makes a wireframe twin of the velocity pipeline legal.
static_assert(Flux_TerrainGBufferAttachmentCountForVariant(
	Flux_TerrainGBufferVariant::SOLID) == uFLUX_MRT_CORE_COUNT,
	"terrain solid G-buffer variant must be the 4-MRT core set");
static_assert(Flux_TerrainGBufferAttachmentCountForVariant(
	Flux_TerrainGBufferVariant::WIREFRAME) == uFLUX_MRT_CORE_COUNT,
	"wireframe must not change the attachment count");
static_assert(Flux_TerrainGBufferAttachmentCountForVariant(
	Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME) == MRT_INDEX_COUNT,
	"the velocity wireframe twin must keep the 5-MRT framebuffer contract");
static_assert(Flux_TerrainGBufferAttachmentCountForVariant(
	Flux_TerrainGBufferVariant::VELOCITY_SOLID) == MRT_INDEX_COUNT,
	"terrain velocity variant must be the 5-MRT set");

// ----------------------------------------------------------------------------
// Indirect-command ABI mirror pin (the GPU shader text-#includes
// Flux_TerrainIndirectCommon.slang and the C++ side reads the twin in
// Flux/Backend/Flux_IndirectDraw.h). Slang constants are not reachable from
// C++, so the cross-language pin lives HERE as a frozen transcription of the
// shader-side constants. The shader's uFLUX_TERRAIN_INDIRECT_WORD_COUNT /
// uFLUX_TERRAIN_INDIRECT_BYTE_STRIDE / uFLUX_TERRAIN_TOTAL_CHUNKS are pinned by
// value below; a drift between the C++ ABI and the Slang shared include reads
// every command field after it at the wrong offset and produces plausible
// garbage rather than a crash, so the assertions are load-bearing.
// ----------------------------------------------------------------------------
static_assert(uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT == 5u,
	"Flux_IndirectDrawIndexedCommand word count must be 5 (indexCount/inC/firstIndex/vOff/firstInstance) — "
	"the Slang shared include pins the same constant");
static_assert(uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE == 20u,
	"Flux_IndirectDrawIndexedCommand byte stride must be 20 — the Slang shared include pins the same constant");
static_assert(Flux_TerrainConfig::TOTAL_CHUNKS == 4096u,
	"Terrain expects 4096 chunks — the Slang reset shader's bounds-check reads this; "
	"any change to TOTAL_CHUNKS requires regenerating the terrain shaders (FluxCompiler) "
	"and updating the reset dispatch group count in ExecuteResetCounters in lockstep");

bool dbg_bWireframe = false;
u_int dbg_uDebugMode = 0;  // Debug visualization mode (0=Off, 1=LOD, 2=Normals, 3=UVs, etc.)
// Visibility culling is entirely GPU-side (Flux_TerrainCulling.slang reads the
// frustum planes from the CB; there is no CPU visibility test left to bias or
// bypass), so the old Visiblity Multiplier / Ignore Visibility Check knobs and
// the Log Metrics switch have no consumer and are not registered. Reinstating any
// of them means adding the CB field / log site FIRST.

static void ExecuteResetCounters(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteCulling(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteGBuffer(Flux_CommandBuffer* pxCmdList, void* pUserData);

void Flux_TerrainImpl::BuildPipelines()
{
	m_xTerrainGBufferShader.Initialise(Flux_TerrainShaders::xTerrain_ToGBuffer);
	m_xTerrainShadowShader.Initialise(Flux_TerrainShaders::xTerrain_ToShadowmap);

	// The three Terrain programs share one 20-byte VsIn: quantised position (8) /
	// normalised UV (4) / packed normal (4) / packed tangent+bitangent-sign (4). It is
	// frozen against the on-disk baked-chunk contract by the static_assert beside
	// Zenith_TerrainChunkLayout in Flux_TerrainStreamingManager.cpp. The UV is UNORM16
	// and not HALF2 on purpose: a half mantissa is too small for heightmap-pixel-scale
	// UVs above 2048, which collapses adjacent vertex UVs into a vertex-spacing-period
	// strip pattern in the diffuse — unorm16 is uniform across the whole range, at a
	// 1/16-pixel step.

	{

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE] = MRT_FORMAT_EMISSIVE;
		xPipelineSpec.m_uNumColourAttachments = uFLUX_MRT_CORE_COUNT;   // base terrain G-buffer pipeline (4 MRTs); the velocity variant (5 MRTs) is built separately
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &m_xTerrainGBufferShader;
		xPipelineSpec.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
		xPipelineSpec.m_pxVertexLayout = &Flux_Generated_Terrain::Terrain_ToGBuffer::kVertexLayout;

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

	// TAA velocity variant (Stage 4.3c): a 5-attachment pipeline (4 core MRTs + velocity), drawn
	// INSTEAD of the base terrain G-buffer pipeline when the velocity latch is on. Same vertex
	// layout + shading; the extra SV_Target4 carries the per-pixel camera-reprojection motion vector.
	{
		m_xTerrainGBufferVelocityShader.Initialise(Flux_TerrainShaders::xTerrain_ToGBufferVelocity);

		Flux_PipelineSpecification xVelocitySpec;
		xVelocitySpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE]        = MRT_FORMAT_DIFFUSE;
		xVelocitySpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xVelocitySpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL]       = MRT_FORMAT_MATERIAL;
		xVelocitySpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE]       = MRT_FORMAT_EMISSIVE;
		xVelocitySpec.m_aeColourAttachmentFormats[MRT_INDEX_VELOCITY]       = MRT_FORMAT_VELOCITY;
		xVelocitySpec.m_uNumColourAttachments = MRT_INDEX_COUNT;   // 5
		xVelocitySpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xVelocitySpec.m_pxShader = &m_xTerrainGBufferVelocityShader;
		xVelocitySpec.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
		xVelocitySpec.m_pxVertexLayout = &Flux_Generated_Terrain::Terrain_ToGBufferVelocity::kVertexLayout;

		m_xTerrainGBufferVelocityShader.GetReflection().PopulateLayout(xVelocitySpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xVelocitySpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(m_xTerrainGBufferVelocityPipeline, xVelocitySpec);

		// Wireframe twin of the velocity variant. ORDER IS LOAD-BEARING: m_bWireframe is
		// flipped AFTER the solid build (same shape as the base pair above), otherwise the
		// SOLID velocity pipeline would rasterize as eLine and every default TAA-on frame
		// would draw wireframe terrain.
		//
		// Only the rasterizer polygon mode differs — the 5 attachment formats and
		// m_uNumColourAttachments are inherited verbatim from xVelocitySpec, so this twin
		// still satisfies the 5-attachment framebuffer contract the "Terrain GBuffer" pass
		// declares in SetupRenderGraph, and the velocity shader still writes SV_Target4. No
		// shader variant is needed.
		xVelocitySpec.m_bWireframe = true;
		Flux_PipelineBuilder::FromSpecification(m_xTerrainWireframeVelocityPipeline, xVelocitySpec);
	}


	{
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &m_xTerrainShadowShader;
		xShadowPipelineSpec.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
		xShadowPipelineSpec.m_pxVertexLayout = &Flux_Generated_Terrain::Terrain_ToShadowmap::kVertexLayout;

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

	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(nullptr, sizeof(struct TerrainConstants
		), m_xTerrainConstantsBuffer);

	// The dequant box, from the one helper the exporter and the live-edit hooks pack
	// against. Constant for the process — it describes the authored terrain extent,
	// not any particular component — so it is written once here rather than per frame.
	{
		const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant();
		for (int i = 0; i < 3; i++)
		{
			s_xTerrainConstants.m_afPosQuantScale[i] = xQuant.m_xScale[i];
			s_xTerrainConstants.m_afPosQuantBias[i] = xQuant.m_xBias[i];
		}
		s_xTerrainConstants.m_afPosQuantScale[3] = Zenith_TerrainChunkLayout::fUV_BOX_MAX;
	}

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({ "Render", "Terrain", "UV Scale" }, s_xTerrainConstants.m_fUVScale, 0., 10.);
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Terrain", "Wireframe" }, dbg_bWireframe);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Terrain", "Debug Mode" }, dbg_uDebugMode, 0, 13);
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

	// Pass 0: Reset visible-count AND indirect-argument buffers. One dispatch per
	// terrain, ceil(TOTAL_CHUNKS / 64) groups of 64 threads; thread 0 writes
	// visibleCount[0] = 0 and each in-range thread writes one fully zero-
	// initialised indirect record. The culling pass DependsOn this pass and
	// re-declares both buffers as UAV writes — the graph synthesises the
	// UAV→UAV barrier between the two so the culling dispatch's atomic
	// increments AND its prefix compaction observe the cleared state. The
	// full-record clear is the zero-tail invariant that lets the recorder
	// execute a fixed indexed-indirect draw over [0, TOTAL_CHUNKS) on devices
	// missing vkCmdDrawIndexedIndirectCount[KHR]: culling writes the live
	// prefix, the cleared [visibleCount, TOTAL_CHUNKS) tail stays all-zero
	// no-ops, and a many→few→zero transition cannot replay stale chunks.
	Flux_PassHandle xResetPass = xGraph.AddPass("Terrain Reset Count and Indirect Arguments", ExecuteResetCounters);
	for (u_int u = 0; u < xTerrains.GetSize(); u++)
	{
		Flux_TerrainStreamingState* pxState = xTerrains.Get(u).m_pxState;
		if (!pxState->m_bCullingResourcesInitialized) continue;
		// UAV write for the count buffer — the reset pass clears it to zero.
		xGraph.WriteBuffer(xResetPass, pxState->m_xVisibleCountBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
		// UAV write for the argument buffer — the reset pass clears every
		// record in [0, TOTAL_CHUNKS) to the legal no-op (five words / 20
		// bytes zero). The cross-frame cyclic seed makes the prior frame's
		// indirect-ARG read by the GBuffer pass source this write's barrier
		// (the graph's per-resource last-access tracker carries the access
		// across the frame boundary — the same mechanism that already
		// protects the count buffer).
		xGraph.WriteBuffer(xResetPass, pxState->m_xIndirectDrawBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
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
		// this pass. visibleCount is READWRITE_UAV — the culling shader uses
		// InterlockedAdd(visibleCount[0], 1u, outIndex), a read-modify-write
		// that must observe the zero the reset pass wrote. Declaring it
		// WRITE_UAV omits the shader-read destination access so the reset
		// value is not guaranteed visible to the atomic. LODLevelBuffer is
		// the same pattern (reads priorLOD before writing its hysteresis
		// decision). The argument buffer is pure append (InterlockedAdd to
		// grab a slot, then a plain store into that slot) so WRITE_UAV is
		// correct for it.
		xGraph.WriteBuffer(xCullingPass, pxState->m_xIndirectDrawBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
		xGraph.WriteBuffer(xCullingPass, pxState->m_xVisibleCountBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);
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

	// TAA velocity (Stage 4.3c): make the terrain G-buffer pass a 5-attachment framebuffer when the
	// velocity latch is on, matching the velocity pipeline selected at record time. Terrain is
	// main-view only, so no view guard. Added non-fluently (the fluent builder is rvalue-only).
	if (xGraphics.IsVelocityMRTActive())
	{
		xGraph.Write(xGBufferPass, xGraphics.GetVelocityAttachment(), RESOURCE_ACCESS_WRITE_RTV);
	}

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
	// m_xFrameConstants.m_xViewProjMat is the CPU camera-matrix source and stays UNJITTERED
	// (TAA jitter is injected only into the slot-0 GPU CB payload), so terrain streaming
	// culls against the jitter-free frustum by construction — do NOT switch this to the
	// per-view GPU payload's m_xViewProjMat (that copy carries the sub-pixel jitter).
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

		// Bind set 0, slot 0: visibleCount UAV and slot 1: the indirect-
		// command UAV. The shader is [numthreads(64,1,1)]: thread 0 writes
		// visibleCount[0] = 0 and each in-range thread writes one zero-
		// initialised record. The bounds check reads the argument buffer's
		// dimensions, so 64 groups cover the shipping 4,096-record grid
		// (4096 = 64 * 64) and an arbitrary record count would also work.
		// The graph emits the UAV→UAV barrier between this pass and the
		// culling pass, so culling's atomic prefix compaction observes the
		// cleared state AND the cleared [visibleCount, TOTAL_CHUNKS) tail
		// stays all-zero no-ops for the recorder's fixed fallback path. Do
		// NOT clear LODLevelBuffer here — it carries prior-frame hysteresis
		// state that culling reads before writing.
		Flux_ShaderBinder xBinder(*pxCmdList);
		xBinder.BindUAV_Buffer(Flux_Generated_Terrain::TerrainResetCounters::hvisibleCount,
			&pxState->m_xVisibleCountBuffer.GetUAV());
		xBinder.BindUAV_Buffer(Flux_Generated_Terrain::TerrainResetCounters::hindirectCommands,
			&pxState->m_xIndirectDrawBuffer.GetUAV());
		// ceil(TOTAL_CHUNKS / 64) groups = 64 for the 4096-chunk shipping grid;
		// covers every in-range record. The shader bounds-checks inside.
		constexpr uint32_t uRESET_GROUP_COUNT_X = (Flux_TerrainConfig::TOTAL_CHUNKS + 63u) / 64u;
		pxCmdList->Dispatch(uRESET_GROUP_COUNT_X, 1, 1);
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

	// TAA velocity (Stage 4.3c) x wireframe debug: a full 2x2. The velocity latch decides the
	// ATTACHMENT COUNT (4 vs 5 — SetupRenderGraph reads the same latch when declaring the pass,
	// and SetupTransients freezes it for the whole graph build); the wireframe flag decides only
	// the RASTERIZER polygon mode. They are orthogonal, so every combination has a prebuilt
	// pipeline and neither axis suppresses the other.
	//
	// This was previously "velocity wins over wireframe", which made the terrain Wireframe
	// checkbox inert in every default run, because TAA ships ON.
	const bool bVelocity = g_xEngine.FluxGraphics().IsVelocityMRTActive();
	const Flux_TerrainGBufferVariant eVariant = Flux_TerrainSelectGBufferVariant(bVelocity, dbg_bWireframe);

	Flux_Pipeline* pxPipeline = nullptr;
	switch (eVariant)
	{
	case Flux_TerrainGBufferVariant::SOLID:
		pxPipeline = &xTerrain.m_xTerrainGBufferPipeline;           break;
	case Flux_TerrainGBufferVariant::WIREFRAME:
		pxPipeline = &xTerrain.m_xTerrainWireframePipeline;         break;
	case Flux_TerrainGBufferVariant::VELOCITY_SOLID:
		pxPipeline = &xTerrain.m_xTerrainGBufferVelocityPipeline;   break;
	case Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME:
		pxPipeline = &xTerrain.m_xTerrainWireframeVelocityPipeline; break;
	default:
		Zenith_Assert(false, "ExecuteGBuffer: unhandled terrain G-buffer pipeline variant %u",
			static_cast<u_int>(eVariant));
		pxPipeline = &xTerrain.m_xTerrainGBufferPipeline;           break;
	}
	pxCmdList->SetPipeline(pxPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Typed binding handles for the Terrain_ToGBuffer program (m_xTerrainGBufferShader
	// was Initialised from Flux_TerrainShaders::xTerrain_ToGBuffer).
	namespace TGB = Flux_Generated_Terrain::Terrain_ToGBuffer;

	// Spine: the camera matrix comes from the VIEW set (set 1) g_xView, sourced
	// from m_xViewConstantsBuffer (was the old per-frame FrameConstants bind).
	// The GBuffer shader reads only the camera (no sun/time), so only g_xView is
	// bound here. TerrainConstants (per-frame UV scale) is now a PassParams member.
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

		// GPU-driven indirect rendering via DRAW_INDEXED_INDIRECT_COUNT.
		//
		// Each component uses its own indirect draw buffer and visible count
		// buffer. The required eFallback policy is ZERO_PADDED_TO_MAX: the GPU
		// reset pass clears all 4,096 records to the legal no-op every frame
		// and culling compacts live records into [0, visibleCount), so the
		// remaining [visibleCount, TOTAL_CHUNKS) tail is always an all-zero
		// padded range — a fixed indexed-indirect draw over the entire range
		// is therefore valid even when visibility falls to zero. The recorder
		// selects the native counted call on capable devices (within
		// maxDrawIndirectCount), and falls back to legal padded fixed batches
		// otherwise. It NEVER splits one global counted draw across multiple
		// count-buffer batches (each batch would re-read the full count and
		// overdraw): an over-limit request selects padded fixed batches
		// instead. See Flux_IndirectDraw.h for the full selector contract.
		//
		// The argument stride is the named 20-byte ABI constant shared with
		// the Slang shared include, the per-backend recorders and the test
		// pinned ABI POD — the literal `20` is retired.
		//
		// Note: culling uses ATOMIC APPEND compaction; the resulting prefix
		// is NOT sorted front-to-back. Modern GPU early-Z handles the few
		// percent of depth overdraw far cheaper than an O(n²) sort would.
		pxCmdList->DrawIndexedIndirectCount(
			&pxState->m_xIndirectDrawBuffer,   // Per-component argument buffer with compacted live prefix + zero tail
			&pxState->m_xVisibleCountBuffer,  // Per-component count buffer with the live record count
			Flux_TerrainConfig::TOTAL_CHUNKS,  // Max 4096 padded records — the full legal range
			Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX,  // The GPU reset pass + tail-zero invariant makes this safe
			0,                                 // Indirect argument buffer offset (bytes)
			0,                                 // Count buffer offset (bytes)
			uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE  // 20-byte stride shared across ABI / Slang / recorders
		);

	}
}

// STUBBED — terrain does not cast (the call in Flux_Shadows::ExecuteShadowCascade is
// commented out). When it is enabled, this body must call UseBindlessTextures(2) for
// ITSELF, right after its SetPipeline and ABOVE any "nothing to draw" early-out —
// never lean on the caster ahead of it. Flux_UnifiedMeshImpl::RenderToShadowMap
// early-outs on zero buckets before its own bind, so on a scene with no unified
// opaque casters the terrain draw would be the first user of set 2 in that cascade's
// command buffer and would inherit nothing. The pre-draw validator in
// Zenith_Vulkan_CommandBuffer (ShouldDemandBindlessBind) asserts on exactly that, so
// the omission fails loudly instead of drawing correctly until a scene changes.
//
// It must ALSO bind m_xTerrainConstantsBuffer at set 3 binding 0
// (Terrain_ToShadowmap::hTerrainConstants): the caster's VS dequantises the packed
// snorm16 position against the box in that CB, so without the bind it reads an
// undefined descriptor and shadows a garbage terrain. The ZENITH_DEBUG pre-draw
// validator catches the omission (sets 3+ are per-draw), Release does not.
void Flux_TerrainImpl::RenderToShadowMap(Flux_CommandBuffer&, u_int)
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

// Unit tests for the G-buffer pipeline variant selection (the module-owns-its-tests
// pattern — same idiom as Flux_Grass.cpp / Flux_Decals.cpp). Hosted here because this
// TU is always linked; the ZENITH_TEST macros self-noop when ZENITH_TESTING is
// undefined, so the include stays unconditional.
#include "Flux/Terrain/Flux_Terrain.Tests.inl"
