#include "Zenith.h"
#include "Flux/UnifiedMesh/Flux_UnifiedMeshImpl.h"
#include "Flux/UnifiedMesh/Flux_UnifiedMesh_Shaders.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_GPUScene.h"
#include "Flux/UnifiedMesh/Flux_Skinning.h"   // Flux_GPUSkinJob (skin-job buffer element)
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_MaterialTable.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/InstancedMeshes/Flux_InstanceCulling.h"   // Flux_FrustumPlaneGPU + ExtractFrustumPlanes (reused)
#include "Flux/Shadows/Flux_ShadowsImpl.h"               // ZENITH_FLUX_NUM_CSMS, CSM_FORMAT, cascade matrices
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"  // per-bucket VAT texture resolve (Stage 3)
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"           // white dummy VAT texture SRV
#include "Core/Zenith_GraphicsOptions.h"                 // m_bShadowsEnabled (active-view count)
#include "Profiling/Zenith_Profiling.h"

// =====================================================================
// Flux_UnifiedMesh — unified GPU-driven opaque static-mesh feature (Stage 1).
// See Flux_UnifiedMeshImpl.h for the architecture overview. Binding is BY NAME
// (the Flux_ShaderBinder name overloads) so the feature builds without the
// generated typed-handle header — the runtime compiles the .slang; the generated
// header is an offline/CI artifact (FluxCompiler) for the typed-handle convention.
// =====================================================================

namespace
{
	// Generous initial GPU-buffer capacities so the common scene never grows mid-run
	// (RenderTest is ~21 draw-items / 21 buckets). Growth reuses the same buffer
	// object (stable address), so the render-graph barrier keys stay valid.
	constexpr u_int kuINITIAL_DRAWITEMS = 8192u;
	constexpr u_int kuINITIAL_OBJECTS   = 4096u;
	constexpr u_int kuINITIAL_BUCKETS   = 1024u;

	// Stage 5 compute-skinning initial capacities (grow-only). The arena + bind-pose pool
	// are in VERTICES; the palette is in matrices (~40 skeletons * 100 bones).
	constexpr u_int kuINITIAL_SKIN_VERTS   = 65536u;  // arena output verts + bind-pose pool input verts
	constexpr u_int kuINITIAL_SKIN_JOBS    = 256u;
	constexpr u_int kuINITIAL_PALETTE_MATS = 4096u;
	constexpr u_int kuSKIN_INPUT_WORDS     = 26u;     // 104B bind-pose vertex (raw words) — matches uFLUX_SKIN_INPUT_WORDS
	constexpr u_int kuSKIN_OUTPUT_WORDS    = 18u;     //  72B static vertex (raw words) — matches uFLUX_SKIN_OUTPUT_WORDS

	constexpr u_int kuINDIRECT_WORDS = 5u;   // VkDrawIndexedIndirectCommand = 5 uints
	constexpr u_int kuINDIRECT_STRIDE = kuINDIRECT_WORDS * sizeof(u_int); // 20 bytes

	// Views the cull/draw fan out over (Stage 2): view 0 = camera, views 1..N = the CSM
	// cascades. The shared cull-output buffers (visible-index / indirect) are sized to this
	// many views; the camera path is view 0 so its offsets are unchanged from Stage 1. MUST
	// match kUNIFIED_NUM_VIEWS in Flux_UnifiedMesh_Culling.slang.
	constexpr u_int kuUNIFIED_NUM_VIEWS = 1u + ZENITH_FLUX_NUM_CSMS;   // 1 camera + 4 cascades
	static_assert(kuUNIFIED_NUM_VIEWS == 5u, "kUNIFIED_NUM_VIEWS in Flux_UnifiedMesh_Culling.slang assumes 5 views");

	// Reset constants (mirrors ResetConstantsLayout in Flux_UnifiedMesh_Reset.slang).
	struct UnifiedResetConstants
	{
		u_int m_uNumBuckets;   // per-view bucket stride
		u_int m_uNumViews;     // active views this frame
		u_int m_uPad0;
		u_int m_uPad1;
	};
	static_assert(sizeof(UnifiedResetConstants) == 16, "UnifiedResetConstants must be 16 bytes");

	// Skinning constants (mirrors SkinConstantsLayout in Flux_UnifiedMesh_Skinning.slang).
	struct UnifiedSkinConstants
	{
		u_int m_uNumJobs;       // active skin-job count (compute id.y guard)
		u_int m_uPaletteCount;  // total matrices in the palette (bone-index bounds guard)
		u_int m_uPad0;
		u_int m_uPad1;
	};
	static_assert(sizeof(UnifiedSkinConstants) == 16, "UnifiedSkinConstants must be 16 bytes");

	// Per-draw constants (mirrors UnifiedDrawConstantsLayout in the GBuffer + ToShadowmap shaders).
	struct UnifiedDrawConstants
	{
		u_int m_uMaterialIndex;
		u_int m_uBucketOffset;   // per-view base: view*totalDrawItems + bucketOffset[slot]
		u_int m_uShadowCascade;  // shadow VS cascade select; 0 for the camera GBuffer
		u_int m_uPad0;
		Zenith_Maths::Vector4 m_xVATParams;  // xy = VAT texture size, z = >0 when the bucket has VAT, w unused
	};
	static_assert(sizeof(UnifiedDrawConstants) == 32, "UnifiedDrawConstants must be 32 bytes (matches UnifiedDrawConstantsLayout)");

	// Resolve a bucket's VAT texture from its key id (raw Flux_AnimationTexture* used as the
	// in-session grouping id, like the material id) + compute the (texW, texH, enabled, 0) params.
	// Returns null + zero params for static buckets (VAT id 0) or a VAT without GPU resources.
	// LIFETIME: the id is reinterpreted back to a live Flux_AnimationTexture* — safe because the
	// bucket key is rebuilt every frame from the current snapshot/instance-group VAT pointers, so a
	// retired VAT never survives into a later frame's key (no use-after-free across frames).
	const Flux_AnimationTexture* ResolveBucketVAT(u_int64 ulVATTextureId, Zenith_Maths::Vector4& xParamsOut)
	{
		xParamsOut = Zenith_Maths::Vector4(0.0f);
		const Flux_AnimationTexture* pxVAT = reinterpret_cast<const Flux_AnimationTexture*>(ulVATTextureId);
		if (pxVAT != nullptr && pxVAT->HasGPUResources())
		{
			xParamsOut = Zenith_Maths::Vector4(
				static_cast<float>(pxVAT->GetTextureWidth()),
				static_cast<float>(pxVAT->GetTextureHeight()), 1.0f, 0.0f);
			return pxVAT;
		}
		return nullptr;
	}

	// Bind the per-bucket VAT texture (POINT sampler) into the DRAW set's g_xAnimationTex; falls
	// back to the white dummy when the bucket has no VAT so the descriptor is always valid.
	void BindBucketVAT(Flux_ShaderBinder& xBinder, Flux_Shader& xShader, const Flux_AnimationTexture* pxVAT)
	{
		Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
		if (pxVAT != nullptr)
		{
			xBinder.BindSRV(xShader, "g_xAnimationTex",
				&pxVAT->GetPositionTexture()->m_xSRV, &xGraphics.m_xPointSampler);
		}
		else
		{
			xBinder.BindSRV(xShader, "g_xAnimationTex",
				&xGraphics.m_xWhiteTexture.GetDirect()->m_xSRV, &xGraphics.m_xPointSampler);
		}
	}

	// Multi-view culling constants (mirrors CullingConstantsLayout in Flux_UnifiedMesh_Culling.slang).
	// Per-view 6 frustum planes (camera + each cascade's sun ortho view-proj), extracted CPU-side.
	struct UnifiedCullingConstants
	{
		Flux_FrustumPlaneGPU  m_axFrustumPlanes[kuUNIFIED_NUM_VIEWS * 6];  // 30 * 16 = 480
		Zenith_Maths::Vector4 m_xCameraPosition;                          // 16
		u_int                 m_uTotalDrawItemCount;                      // per-view draw-item count
		u_int                 m_uNumViews;                                // active views this frame
		u_int                 m_uNumBuckets;                              // per-view indirect stride
		u_int                 m_uNumObjects;                              // object-array size (bounds-guards Objects[] in the cull)
	};
	static_assert(sizeof(UnifiedCullingConstants) == 512, "UnifiedCullingConstants must match CullingConstantsLayout (kUNIFIED_NUM_VIEWS*6 planes)");

	void GrowDynamicRW(Flux_DynamicReadWriteBuffer& xBuf, u_int& uCap, u_int uNeeded, size_t ulElemSize)
	{
		if (uNeeded <= uCap) return;
		const u_int uNewCap = (uCap * 2u > uNeeded) ? uCap * 2u : uNeeded;
		if (uCap > 0u) g_xEngine.FluxMemory().DestroyDynamicReadWriteBuffer(xBuf);
		g_xEngine.FluxMemory().InitialiseDynamicReadWriteBuffer(nullptr, uNewCap * ulElemSize, xBuf);
		uCap = uNewCap;
	}
}

static void ExecuteUnifiedSkinning(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteUnifiedReset(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteUnifiedCulling(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteUnifiedGBuffer(Flux_CommandBuffer* pxCmdList, void* pUserData);

//=============================================================================
// Pipelines
//=============================================================================

void Flux_UnifiedMeshImpl::BuildPipelines()
{
	m_xGBufferShader.Initialise(Flux_UnifiedMeshShaders::xUnifiedMesh_ToGBuffer);

	// Static-mesh vertex layout (position, UV, normal, tangent, bitangent, color).
	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Position
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);  // UV
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Normal
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Tangent
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Bitangent
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);  // Color
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{
		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE]       = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL]      = MRT_FORMAT_MATERIAL;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE]      = MRT_FORMAT_EMISSIVE;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_pxShader = &m_xGBufferShader;
		xSpec.m_xVertexInputDesc = xVertexDesc;
		xSpec.m_eCullMode = CULL_MODE_BACK;
		m_xGBufferShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		for (Flux_BlendState& xBlend : xSpec.m_axBlendStates)
		{
			xBlend.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlend.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlend.m_bBlendEnabled = false;
		}
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipeline, xSpec);

		// Two-sided variant for materials flagged m_bTwoSided.
		xSpec.m_eCullMode = CULL_MODE_NONE;
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipelineTwoSided, xSpec);
	}

	// Stage 2: depth-only shadow caster pipeline. Mirrors Flux_StaticMeshes' single shadow
	// pipeline (no per-material cull split for shadows) — one pipeline draws every bucket.
	// Same vertex layout as the GBuffer so the shadow VS's two-level indirection reads the
	// identical vertex stream; depth bias is dynamic (vkCmdSetDepthBias per cascade in
	// Flux_Shadows::ExecuteShadowCascade).
	{
		m_xShadowShader.Initialise(Flux_UnifiedMeshShaders::xUnifiedMesh_ToShadowmap);

		Flux_PipelineSpecification xShadowSpec;
		xShadowSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowSpec.m_uNumColourAttachments = 0u;
		xShadowSpec.m_pxShader = &m_xShadowShader;
		xShadowSpec.m_xVertexInputDesc = xVertexDesc;
		xShadowSpec.m_bDepthBias = true;
		xShadowSpec.m_bDynamicDepthBias = true;
		xShadowSpec.m_fDepthBiasConstant = 1.75f;
		xShadowSpec.m_fDepthBiasSlope = 3.0f;
		m_xShadowShader.GetReflection().PopulateLayout(xShadowSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(m_xShadowPipeline, xShadowSpec);
	}

	// Cull compute.
	{
		m_xCullingShader.Initialise(Flux_UnifiedMeshShaders::xUnifiedMesh_Culling);
		Flux_RootSigBuilder::FromReflection(m_xCullingRootSig, m_xCullingShader.GetReflection());
		Flux_ComputePipelineBuilder xBuilder;
		xBuilder.WithShader(m_xCullingShader).WithLayout(m_xCullingRootSig.m_xLayout).Build(m_xCullingPipeline);
		m_xCullingPipeline.m_xRootSig = m_xCullingRootSig;
	}

	// Reset compute.
	{
		m_xResetShader.Initialise(Flux_UnifiedMeshShaders::xUnifiedMesh_Reset);
		Flux_RootSigBuilder::FromReflection(m_xResetRootSig, m_xResetShader.GetReflection());
		Flux_ComputePipelineBuilder xBuilder;
		xBuilder.WithShader(m_xResetShader).WithLayout(m_xResetRootSig.m_xLayout).Build(m_xResetPipeline);
		m_xResetPipeline.m_xRootSig = m_xResetRootSig;
	}

	// Stage 5: compute-skinning pre-pass.
	{
		m_xSkinningShader.Initialise(Flux_UnifiedMeshShaders::xUnifiedMesh_Skinning);
		Flux_RootSigBuilder::FromReflection(m_xSkinningRootSig, m_xSkinningShader.GetReflection());
		Flux_ComputePipelineBuilder xBuilder;
		xBuilder.WithShader(m_xSkinningShader).WithLayout(m_xSkinningRootSig.m_xLayout).Build(m_xSkinningPipeline);
		m_xSkinningPipeline.m_xRootSig = m_xSkinningRootSig;
	}
}

void Flux_UnifiedMeshImpl::Initialise()
{
	BuildPipelines();

	// Allocate the GPU buffers up front (before the first SetupRenderGraph) so the
	// persistent cull-output buffers are always declarable and only ever GROW (reuse
	// the object) thereafter. Initial capacities cover typical scenes.
	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();

	// Persistent cull-output buffers are sized for ALL views (camera + cascades): the
	// visible-index list is kuUNIFIED_NUM_VIEWS per-view slices; the indirect buffer is one
	// command per (view,bucket). Capacities below track the PER-VIEW count; allocation
	// multiplies by kuUNIFIED_NUM_VIEWS.
	xMem.InitialiseReadWriteBuffer(nullptr, kuINITIAL_DRAWITEMS * kuUNIFIED_NUM_VIEWS * sizeof(u_int), m_xVisibleIndexBuffer);
	m_uVisibleIndexCapacity = kuINITIAL_DRAWITEMS;   // per-view draw-item capacity

	xMem.InitialiseIndirectBuffer(kuINITIAL_BUCKETS * kuUNIFIED_NUM_VIEWS * kuINDIRECT_STRIDE, m_xIndirectBuffer);
	m_uIndirectBucketCapacity = kuINITIAL_BUCKETS;   // per-view bucket capacity

	// Dynamic inputs are view-INVARIANT (one record set shared across all views).
	xMem.InitialiseDynamicReadWriteBuffer(nullptr, kuINITIAL_OBJECTS * sizeof(Flux_GPUSceneObject), m_xObjectsBuffer);
	m_uObjectCapacity = kuINITIAL_OBJECTS;

	xMem.InitialiseDynamicReadWriteBuffer(nullptr, kuINITIAL_DRAWITEMS * sizeof(Flux_GPUSceneDrawItem), m_xDrawItemsBuffer);
	m_uDrawItemCapacity = kuINITIAL_DRAWITEMS;

	xMem.InitialiseDynamicReadWriteBuffer(nullptr, kuINITIAL_BUCKETS * sizeof(u_int), m_xBucketOffsetBuffer);
	xMem.InitialiseDynamicReadWriteBuffer(nullptr, kuINITIAL_BUCKETS * sizeof(u_int), m_xBucketIndexCountBuffer);
	m_uBucketMetaCapacity = kuINITIAL_BUCKETS;

	xMem.InitialiseDynamicConstantBuffer(nullptr, sizeof(UnifiedCullingConstants), m_xCullingConstantsBuffer);

	// Stage 5 compute-skinning buffers. The arena is the only persistent one (grow-only,
	// graph-tracked, BOTH UAV + VERTEX usage so it is the skinned draws' vertex source).
	// The bind-pose pool / palette / skin-jobs are host-uploaded each frame (dynamic).
	xMem.InitialiseReadWriteBuffer(nullptr, (size_t)kuINITIAL_SKIN_VERTS * kuSKIN_OUTPUT_WORDS * sizeof(u_int),
		m_xSkinnedArenaBuffer, /*bAlsoVertexBuffer*/ true);
	m_uSkinnedArenaVertCapacity = kuINITIAL_SKIN_VERTS;

	xMem.InitialiseDynamicReadWriteBuffer(nullptr, (size_t)kuINITIAL_SKIN_VERTS * kuSKIN_INPUT_WORDS * sizeof(u_int), m_xBindPosePoolBuffer);
	m_uBindPosePoolWordCapacity = kuINITIAL_SKIN_VERTS * kuSKIN_INPUT_WORDS;

	xMem.InitialiseDynamicReadWriteBuffer(nullptr, (size_t)kuINITIAL_PALETTE_MATS * sizeof(Zenith_Maths::Matrix4), m_xBonePaletteBuffer);
	m_uBonePaletteMatCapacity = kuINITIAL_PALETTE_MATS;

	xMem.InitialiseDynamicReadWriteBuffer(nullptr, (size_t)kuINITIAL_SKIN_JOBS * sizeof(Flux_GPUSkinJob), m_xSkinJobsBuffer);
	m_uSkinJobCapacity = kuINITIAL_SKIN_JOBS;

	xMem.InitialiseDynamicConstantBuffer(nullptr, sizeof(UnifiedSkinConstants), m_xSkinConstantsBuffer);

	// Defensive zero-init of the two persistent GPU buffers (whole allocation, all views).
	// The per-frame reset compute only rewrites the live (view,bucket) commands, so slots
	// beyond the live count would otherwise hold garbage device memory. Those slots are
	// never drawn (the GBuffer/shadow loops iterate only live buckets), but a garbage
	// VkDrawIndexedIndirectCommand is the classic GPU-TDR landmine, so seed both buffers to
	// zero once at allocation. Init-time only.
	{
		const u_int uZeroWords = kuINITIAL_DRAWITEMS * kuUNIFIED_NUM_VIEWS;   // covers both buffers
		Zenith_Vector<u_int> auZeros;
		auZeros.Reserve(uZeroWords);
		for (u_int u = 0; u < uZeroWords; ++u) auZeros.PushBack(0u);
		xMem.UploadBufferData(m_xIndirectBuffer.GetBuffer().m_xVRAMHandle,
			auZeros.GetDataPointer(), kuINITIAL_BUCKETS * kuUNIFIED_NUM_VIEWS * kuINDIRECT_STRIDE);
		xMem.UploadBufferData(m_xVisibleIndexBuffer.GetBuffer().m_xVRAMHandle,
			auZeros.GetDataPointer(), kuINITIAL_DRAWITEMS * kuUNIFIED_NUM_VIEWS * sizeof(u_int));
	}

	m_bResourcesReady = true;
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_UnifiedMesh initialised (GPU-driven opaque static path)");
}

void Flux_UnifiedMeshImpl::Shutdown()
{
	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();
	if (m_bResourcesReady)
	{
		xMem.DestroyReadWriteBuffer(m_xVisibleIndexBuffer);
		xMem.DestroyIndirectBuffer(m_xIndirectBuffer);
		xMem.DestroyDynamicReadWriteBuffer(m_xObjectsBuffer);
		xMem.DestroyDynamicReadWriteBuffer(m_xDrawItemsBuffer);
		xMem.DestroyDynamicReadWriteBuffer(m_xBucketOffsetBuffer);
		xMem.DestroyDynamicReadWriteBuffer(m_xBucketIndexCountBuffer);
		xMem.DestroyDynamicConstantBuffer(m_xCullingConstantsBuffer);
		xMem.DestroyReadWriteBuffer(m_xSkinnedArenaBuffer);
		xMem.DestroyDynamicReadWriteBuffer(m_xBindPosePoolBuffer);
		xMem.DestroyDynamicReadWriteBuffer(m_xBonePaletteBuffer);
		xMem.DestroyDynamicReadWriteBuffer(m_xSkinJobsBuffer);
		xMem.DestroyDynamicConstantBuffer(m_xSkinConstantsBuffer);
		// The renderer's persistent bind-pose pool is cleared on its shutdown; reset our GPU-upload
		// generation so a re-init re-uploads from scratch (never reads stale device data).
		m_uBindPosePoolUploadedGen = 0u;
		m_uBindPosePoolDirtyFrames = 0u;
		m_bResourcesReady = false;
	}

	m_xGBufferPipeline.Reset();
	m_xGBufferPipelineTwoSided.Reset();
	m_xShadowPipeline.Reset();
	m_xCullingPipeline.Reset();
	m_xResetPipeline.Reset();
	m_xSkinningPipeline.Reset();
	m_xGBufferShader.Reset();
	m_xShadowShader.Reset();
	m_xCullingShader.Reset();
	m_xResetShader.Reset();
	m_xSkinningShader.Reset();

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_UnifiedMesh shutdown");
}

void Flux_UnifiedMeshImpl::Reset()
{
	m_uTotalDrawItems = 0u;
	m_uBucketSlotCount = 0u;
	m_uNumViews = 1u;
	m_axBucketDraws.Clear();
	m_uSkinJobCount = 0u;
	m_uSkinMaxVertCount = 0u;
	m_uBonePaletteCount = 0u;
}

//=============================================================================
// Render graph
//=============================================================================

void Flux_UnifiedMeshImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	if (!m_bResourcesReady)
	{
		return;
	}

	// Pass 0 (Stage 5): compute-skinning pre-pass — skins animated submesh-instances into the
	// object-space arena BEFORE cull/draw read it. View-independent (one pose feeds camera +
	// every cascade). Self-guards on m_uSkinJobCount == 0 (no animated work -> no dispatch).
	Flux_PassHandle xSkinPass = xGraph.AddPass("Unified Skinning", ExecuteUnifiedSkinning);

	// Pass 1: per-bucket indirect-command reset (compute).
	Flux_PassHandle xResetPass = xGraph.AddPass("Unified Cull Reset", ExecuteUnifiedReset);

	// Pass 2: per-draw-item culling (compute). The main-thread gather is hung here.
	Flux_PassHandle xCullPass = xGraph.AddPass("Unified Mesh Culling", ExecuteUnifiedCulling)
		.Prepare([](void* p) { g_xEngine.UnifiedMesh().GatherUnifiedPacket(p); })
		.DependsOn(xResetPass);

	// Pass 3: indirect G-buffer draw (graphics).
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_PassHandle xGBufferPass = xGraph.AddPass("Unified Mesh GBuffer", ExecuteUnifiedGBuffer)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV)
		.DependsOn(xCullPass);

	// The two persistent cull-output buffers — the only graph-tracked buffers. Their
	// declaration is independent of bucket count, so the graph structure is static.
	Flux_Buffer& xVisible  = m_xVisibleIndexBuffer.GetBuffer();
	Flux_Buffer& xIndirect = m_xIndirectBuffer.GetBuffer();

	// Reset fully overwrites the indirect commands.
	xGraph.WriteBuffer(xResetPass, xIndirect, RESOURCE_ACCESS_WRITE_UAV);

	// Cull writes the visible-index list and read-modify-writes the per-bucket
	// instanceCount (atomics) → READWRITE on the indirect so it orders after reset.
	xGraph.WriteBuffer(xCullPass, xVisible,  RESOURCE_ACCESS_WRITE_UAV);
	xGraph.WriteBuffer(xCullPass, xIndirect, RESOURCE_ACCESS_READWRITE_UAV);

	// GBuffer reads the visible-index list (SSBO) and the indirect args.
	xGraph.ReadBuffer(xGBufferPass, xVisible,  RESOURCE_ACCESS_READ_BUFFER_SRV);
	xGraph.ReadBuffer(xGBufferPass, xIndirect, RESOURCE_ACCESS_READ_INDIRECT_ARG);

	// Stage 5: the skinning pass writes the persistent arena; the GBuffer (and, via the same
	// buffer, the shadow cascades) read it as the skinned buckets' vertex source. Declaring
	// the write+read makes the graph order skinning before the consumers and synthesise the
	// compute-write -> vertex-read barrier. (Inert until the gather produces skin-jobs.)
	Flux_Buffer& xArena = m_xSkinnedArenaBuffer.GetBuffer();
	xGraph.WriteBuffer(xSkinPass,    xArena, RESOURCE_ACCESS_WRITE_UAV);
	xGraph.ReadBuffer(xGBufferPass,  xArena, RESOURCE_ACCESS_READ_VERTEX_BUFFER);   // fetched as a vertex stream
}

//=============================================================================
// Gather (main-thread .Prepare — single writer of the per-frame state)
//=============================================================================

void Flux_UnifiedMeshImpl::GatherUnifiedPacket(void*)
{
	ZENITH_PROFILE_SCOPE("Flux Unified Gather");

	m_uTotalDrawItems = 0u;
	m_uBucketSlotCount = 0u;
	m_uNumViews = 1u;
	m_axBucketDraws.Clear();

	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	if (!m_bResourcesReady)
	{
		return;
	}

	const Flux_GPUSceneBuildResult&    xScene    = xRenderer.GetUnifiedGPUScene();
	const Flux_GPUSceneBucketRegistry& xRegistry = xRenderer.GetUnifiedBucketRegistry();

	const u_int uNumObjects   = xScene.m_xObjects.GetSize();
	const u_int uNumDrawItems = xScene.m_xDrawItems.GetSize();
	const u_int uSlotCount    = xRegistry.GetBucketSlotCount();
	if (uNumDrawItems == 0u || uSlotCount == 0u)
	{
		return;   // nothing to draw this frame
	}

	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();

	// Grow GPU buffers if the scene outgrew the current PER-VIEW capacity (reuses the object).
	// Both persistent buffers are sized for all kuUNIFIED_NUM_VIEWS views.
	if (uNumDrawItems > m_uVisibleIndexCapacity)
	{
		xMem.DestroyReadWriteBuffer(m_xVisibleIndexBuffer);
		m_uVisibleIndexCapacity = (m_uVisibleIndexCapacity * 2u > uNumDrawItems) ? m_uVisibleIndexCapacity * 2u : uNumDrawItems;
		xMem.InitialiseReadWriteBuffer(nullptr, m_uVisibleIndexCapacity * kuUNIFIED_NUM_VIEWS * sizeof(u_int), m_xVisibleIndexBuffer);
	}
	if (uSlotCount > m_uIndirectBucketCapacity)
	{
		xMem.DestroyIndirectBuffer(m_xIndirectBuffer);
		m_uIndirectBucketCapacity = (m_uIndirectBucketCapacity * 2u > uSlotCount) ? m_uIndirectBucketCapacity * 2u : uSlotCount;
		xMem.InitialiseIndirectBuffer(m_uIndirectBucketCapacity * kuUNIFIED_NUM_VIEWS * kuINDIRECT_STRIDE, m_xIndirectBuffer);
	}
	GrowDynamicRW(m_xObjectsBuffer,         m_uObjectCapacity,     uNumObjects,   sizeof(Flux_GPUSceneObject));
	GrowDynamicRW(m_xDrawItemsBuffer,       m_uDrawItemCapacity,   uNumDrawItems, sizeof(Flux_GPUSceneDrawItem));
	// The offset + index-count buffers share one bucket-meta capacity and must grow in
	// lockstep. GrowDynamicRW advances m_uBucketMetaCapacity by reference, so capture the
	// pre-grow capacity first — re-testing uSlotCount against the (already-advanced)
	// m_uBucketMetaCapacity would always be false and silently leave the index-count
	// buffer at its old size, overrunning it on the host upload at high bucket counts.
	const u_int uOldBucketMetaCapacity = m_uBucketMetaCapacity;
	GrowDynamicRW(m_xBucketOffsetBuffer, m_uBucketMetaCapacity, uSlotCount, sizeof(u_int));
	if (uSlotCount > uOldBucketMetaCapacity)
	{
		xMem.DestroyDynamicReadWriteBuffer(m_xBucketIndexCountBuffer);
		xMem.InitialiseDynamicReadWriteBuffer(nullptr, m_uBucketMetaCapacity * sizeof(u_int), m_xBucketIndexCountBuffer);
	}

	// --- per-bucket counts → prefix-sum offsets (indexed by stable slot) ---
	// Count draw-items per bucket, then offsets = exclusive prefix sum via the pure,
	// headless-tested Flux_BuildBucketOffsets (the index-count array is filled in the
	// resolve loop below). m_auBucketOffsetScratch is sized by Flux_BuildBucketOffsets.
	m_auBucketCountScratch.Clear();
	m_auBucketIndexCountScratch.Clear();
	for (u_int u = 0; u < uSlotCount; ++u)
	{
		m_auBucketCountScratch.PushBack(0u);
		m_auBucketIndexCountScratch.PushBack(0u);
	}
	for (u_int u = 0; u < uNumDrawItems; ++u)
	{
		const u_int uBucket = xScene.m_xDrawItems.Get(u).m_uBucketIndex;
		if (uBucket < uSlotCount)
		{
			++m_auBucketCountScratch.Get(uBucket);
		}
	}
	Flux_BuildBucketOffsets(m_auBucketCountScratch, m_auBucketOffsetScratch);

	// --- per-bucket mesh / material resolve + live-bucket draw list ---
	Flux_MaterialTable&   xTable = g_xEngine.FluxGraphics().MaterialTable();
	Zenith_MaterialAsset* pxBlank = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();
	for (u_int uSlot = 0; uSlot < uSlotCount; ++uSlot)
	{
		const Flux_GPUSceneBucketKey* pxKey = xRegistry.TryGetBucketKey(uSlot);
		if (pxKey == nullptr || m_auBucketCountScratch.Get(uSlot) == 0u)
		{
			continue;   // retired or unreferenced slot — indirect cmd left zeroed (no draw)
		}

		Flux_UnifiedBucketDraw xDraw;
		xDraw.m_uSlot          = uSlot;
		xDraw.m_uVisibleOffset = m_auBucketOffsetScratch.Get(uSlot);
		xDraw.m_uCullMode      = pxKey->m_uCullMode;

		// SKINNED bucket (Stage 5): the meshGeometryId is a synthetic skin index, NOT a registry
		// id — resolve the arena slice + the bind-pose mesh's IB from the renderer's per-frame
		// skinned-draw array. Static/foliage buckets resolve the shared registry geometry + VAT.
		Flux_MeshInstance* pxMesh = nullptr;
		if (Flux_IsSkinnedMeshGeometryId(pxKey->m_uMeshGeometryId))
		{
			// The low bits are now a STABLE skinned-instance id (not a per-frame array index);
			// resolve the arena slice + IB mesh through the per-frame id->draw map.
			const u_int uStableId = Flux_SkinnedMeshGeometryIndex(pxKey->m_uMeshGeometryId);
			const Flux_RendererImpl::Flux_UnifiedSkinnedDraw* pxSD = xRenderer.GetUnifiedSkinnedDrawById().TryGet(uStableId);
			if (pxSD == nullptr || pxSD->m_pxMesh == nullptr)
			{
				continue;
			}
			pxMesh                = pxSD->m_pxMesh;   // index buffer + count
			xDraw.m_bSkinned      = true;
			xDraw.m_uVertexOffset = pxSD->m_uVertexOffset;
		}
		else
		{
			pxMesh = xRenderer.GetUnifiedMeshGeometry(pxKey->m_uMeshGeometryId);
			if (pxMesh == nullptr)
			{
				continue;
			}
			xDraw.m_pxVATTexture = ResolveBucketVAT(pxKey->m_ulVATTextureId, xDraw.m_xVATParams);
		}
		m_auBucketIndexCountScratch.Get(uSlot) = pxMesh->GetNumIndices();
		xDraw.m_pxMesh = pxMesh;

		// Material registration is MAIN-THREAD only (this gather runs on the main
		// thread via .Prepare). The key stores the asset pointer as the in-session id.
		Zenith_MaterialAsset* pxMat = reinterpret_cast<Zenith_MaterialAsset*>(pxKey->m_ulMaterialAssetId);
		if (pxMat == nullptr) pxMat = pxBlank;
		u_int uMaterialIndex = 0u;
		if (pxMat != nullptr)
		{
			xTable.GetOrCreateIndex(pxMat);
			uMaterialIndex = pxMat->GetMaterialTableIndex();
			if (uMaterialIndex == uFLUX_INVALID_MATERIAL_INDEX) uMaterialIndex = 0u;
		}
		xDraw.m_uMaterialIndex = uMaterialIndex;
		m_axBucketDraws.PushBack(xDraw);
	}

	// --- host uploads into the current frame's dynamic buffers ---
	xMem.UploadBufferData(m_xObjectsBuffer.GetBuffer().m_xVRAMHandle,
		xScene.m_xObjects.GetDataPointer(), uNumObjects * sizeof(Flux_GPUSceneObject));
	xMem.UploadBufferData(m_xDrawItemsBuffer.GetBuffer().m_xVRAMHandle,
		xScene.m_xDrawItems.GetDataPointer(), uNumDrawItems * sizeof(Flux_GPUSceneDrawItem));
	xMem.UploadBufferData(m_xBucketOffsetBuffer.GetBuffer().m_xVRAMHandle,
		m_auBucketOffsetScratch.GetDataPointer(), uSlotCount * sizeof(u_int));
	xMem.UploadBufferData(m_xBucketIndexCountBuffer.GetBuffer().m_xVRAMHandle,
		m_auBucketIndexCountScratch.GetDataPointer(), uSlotCount * sizeof(u_int));

	// --- multi-view culling constants (per-view frustum planes + camera + counts) ---
	// Active views: camera (view 0) always; the 4 cascades (views 1..N) only when shadows are
	// enabled. When shadows are off the cascade passes early-out and never read the shadow-view
	// partitions, so culling them would be wasted — and UpdateShadowMatrices may not have run,
	// leaving stale frustums. The cascade frustums come from UpdateShadowMatrices, hoisted to
	// the main-thread seam (Zenith_Core) so they are current here regardless of .Prepare order.
	const u_int uNumViews = Zenith_GraphicsOptions::Get().m_bShadowsEnabled ? kuUNIFIED_NUM_VIEWS : 1u;

	UnifiedCullingConstants xCull = {};
	Flux_InstanceCullingUtil::ExtractFrustumPlanes(g_xEngine.FluxGraphics().GetViewProjMatrix(), &xCull.m_axFrustumPlanes[0]);
	if (uNumViews > 1u)
	{
		Flux_ShadowsImpl& xShadows = g_xEngine.Shadows();
		for (u_int uCascade = 0; uCascade < ZENITH_FLUX_NUM_CSMS; ++uCascade)
		{
			// The cascade ortho box already bakes in the caster near-plane extend, so the same
			// 6-plane sphere test retains occluders behind the cascade that still cast into it.
			Flux_InstanceCullingUtil::ExtractFrustumPlanes(
				xShadows.GetSunViewProjMatrix(uCascade), &xCull.m_axFrustumPlanes[(uCascade + 1u) * 6u]);
		}
	}
	xCull.m_xCameraPosition     = Zenith_Maths::Vector4(g_xEngine.FluxGraphics().GetCameraPosition(), 0.0f);
	xCull.m_uTotalDrawItemCount = uNumDrawItems;
	xCull.m_uNumViews           = uNumViews;
	xCull.m_uNumBuckets         = uSlotCount;
	xCull.m_uNumObjects         = uNumObjects;   // cull bounds-guards Objects[di.m_uObjectIndex]
	xMem.UploadBufferData(m_xCullingConstantsBuffer.GetBuffer().m_xVRAMHandle, &xCull, sizeof(xCull));

	// --- Stage 5: skinning inputs (host-uploaded) + arena grow ---
	// The renderer built the bind-pose pool / bone palette / skin-jobs CPU-side in
	// SyncUnifiedBucketsFromSnapshot; upload them + size the per-frame skinning dispatch. The arena
	// is persistent grow-only (reuse the object so the graph barrier key stays valid).
	const Zenith_Vector<u_int>&                 auBindPose = xRenderer.GetUnifiedBindPosePoolWords();
	const Zenith_Vector<Zenith_Maths::Matrix4>& axPalette  = xRenderer.GetUnifiedBonePalette();
	const Zenith_Vector<Flux_GPUSkinJob>&       axJobs     = xRenderer.GetUnifiedSkinJobs();
	m_uSkinJobCount     = axJobs.GetSize();
	m_uSkinMaxVertCount = xRenderer.GetUnifiedSkinMaxVerts();
	m_uBonePaletteCount = axPalette.GetSize();

	// Bind-pose pool refresh runs EVERY frame (NOT gated by skin-job count). The pool is persistent
	// + STATIC; it changes only when a new distinct skinned mesh appears (grow) or an eviction
	// re-packs it (shrink) — both bump the renderer's pool generation. Its GPU copy is frame-indexed,
	// so a change must re-upload for MAX_FRAMES_IN_FLIGHT *consecutive* frames to refresh every
	// physical copy. Gating this inside `m_uSkinJobCount > 0` would let a single no-skinned-content
	// frame interrupt that run and leave one copy stale for a later dispatch — so ShouldUpload (which
	// also advances the dirty counter) must tick every frame. The upload itself only fires when armed
	// AND the pool is non-empty.
	const bool bUploadBindPosePool = Flux_BindPosePoolShouldUpload(xRenderer.GetUnifiedBindPosePoolGeneration(),
		MAX_FRAMES_IN_FLIGHT, m_uBindPosePoolUploadedGen, m_uBindPosePoolDirtyFrames);
	if (bUploadBindPosePool && auBindPose.GetSize() > 0u)
	{
		GrowDynamicRW(m_xBindPosePoolBuffer, m_uBindPosePoolWordCapacity, auBindPose.GetSize(), sizeof(u_int));
		xMem.UploadBufferData(m_xBindPosePoolBuffer.GetBuffer().m_xVRAMHandle, auBindPose.GetDataPointer(), auBindPose.GetSize() * sizeof(u_int));
	}

	if (m_uSkinJobCount > 0u)
	{
		const u_int uTotalOutVerts = xRenderer.GetUnifiedSkinTotalOutVerts();
		if (uTotalOutVerts > m_uSkinnedArenaVertCapacity)
		{
			xMem.DestroyReadWriteBuffer(m_xSkinnedArenaBuffer);
			m_uSkinnedArenaVertCapacity = (m_uSkinnedArenaVertCapacity * 2u > uTotalOutVerts) ? m_uSkinnedArenaVertCapacity * 2u : uTotalOutVerts;
			xMem.InitialiseReadWriteBuffer(nullptr, (size_t)m_uSkinnedArenaVertCapacity * kuSKIN_OUTPUT_WORDS * sizeof(u_int),
				m_xSkinnedArenaBuffer, /*bAlsoVertexBuffer*/ true);
		}
		GrowDynamicRW(m_xBonePaletteBuffer,  m_uBonePaletteMatCapacity,   axPalette.GetSize(),  sizeof(Zenith_Maths::Matrix4));
		GrowDynamicRW(m_xSkinJobsBuffer,     m_uSkinJobCapacity,          axJobs.GetSize(),     sizeof(Flux_GPUSkinJob));
		xMem.UploadBufferData(m_xBonePaletteBuffer.GetBuffer().m_xVRAMHandle,  axPalette.GetDataPointer(),  axPalette.GetSize() * sizeof(Zenith_Maths::Matrix4));
		xMem.UploadBufferData(m_xSkinJobsBuffer.GetBuffer().m_xVRAMHandle,     axJobs.GetDataPointer(),     axJobs.GetSize() * sizeof(Flux_GPUSkinJob));
	}

	m_uTotalDrawItems  = uNumDrawItems;
	m_uBucketSlotCount = uSlotCount;
	m_uNumViews        = uNumViews;
}

//=============================================================================
// Execute callbacks (worker threads — pure readers of the frozen gather state)
//=============================================================================

static void ExecuteUnifiedSkinning(Flux_CommandBuffer* pxCmdList, void*)
{
	Flux_UnifiedMeshImpl& xSelf = g_xEngine.UnifiedMesh();
	if (xSelf.m_uSkinJobCount == 0u)
	{
		return;   // no animated work this frame
	}

	pxCmdList->BindComputePipeline(&xSelf.m_xSkinningPipeline);
	Flux_ShaderBinder xBinder(*pxCmdList);

	UnifiedSkinConstants xConsts = {};
	xConsts.m_uNumJobs      = xSelf.m_uSkinJobCount;
	xConsts.m_uPaletteCount = xSelf.m_uBonePaletteCount;
	xBinder.BindDrawConstants(xSelf.m_xSkinningShader, "SkinConstants", &xConsts, sizeof(xConsts));
	xBinder.BindSRV_Buffer(xSelf.m_xSkinningShader, "bindPose",     xSelf.m_xBindPosePoolBuffer.GetSRV());
	xBinder.BindSRV_Buffer(xSelf.m_xSkinningShader, "bonePalette",  xSelf.m_xBonePaletteBuffer.GetSRV());
	xBinder.BindSRV_Buffer(xSelf.m_xSkinningShader, "skinJobs",     xSelf.m_xSkinJobsBuffer.GetSRV());
	xBinder.BindUAV_Buffer(xSelf.m_xSkinningShader, "skinnedArena", &xSelf.m_xSkinnedArenaBuffer.GetUAV());

	// 2D dispatch: X = vertices (max over jobs, 64/group), Y = one row per skin-job. Each
	// thread guards vx < job.vertCount and jobIdx < numJobs (the shader's own guards).
	const u_int uGroupsX = (xSelf.m_uSkinMaxVertCount + 63u) / 64u;
	pxCmdList->Dispatch(uGroupsX, xSelf.m_uSkinJobCount, 1u);
}

static void ExecuteUnifiedReset(Flux_CommandBuffer* pxCmdList, void*)
{
	Flux_UnifiedMeshImpl& xSelf = g_xEngine.UnifiedMesh();
	if (xSelf.m_uTotalDrawItems == 0u)
	{
		return;
	}

	pxCmdList->BindComputePipeline(&xSelf.m_xResetPipeline);
	Flux_ShaderBinder xBinder(*pxCmdList);

	UnifiedResetConstants xConsts = {};
	xConsts.m_uNumBuckets = xSelf.m_uBucketSlotCount;
	xConsts.m_uNumViews   = xSelf.m_uNumViews;
	xBinder.BindDrawConstants(xSelf.m_xResetShader, "ResetConstants", &xConsts, sizeof(xConsts));
	xBinder.BindUAV_Buffer(xSelf.m_xResetShader, "bucketIndexCount", &xSelf.m_xBucketIndexCountBuffer.GetUAV());
	xBinder.BindUAV_Buffer(xSelf.m_xResetShader, "indirect",         &xSelf.m_xIndirectBuffer.GetUAV());

	// One thread per (view, bucket) — reset every active view's indirect commands.
	const u_int uResetThreads = xSelf.m_uBucketSlotCount * xSelf.m_uNumViews;
	pxCmdList->Dispatch((uResetThreads + 63u) / 64u, 1u, 1u);
}

static void ExecuteUnifiedCulling(Flux_CommandBuffer* pxCmdList, void*)
{
	Flux_UnifiedMeshImpl& xSelf = g_xEngine.UnifiedMesh();
	if (xSelf.m_uTotalDrawItems == 0u)
	{
		return;
	}

	pxCmdList->BindComputePipeline(&xSelf.m_xCullingPipeline);
	Flux_ShaderBinder xBinder(*pxCmdList);

	xBinder.BindCBV(xSelf.m_xCullingShader,        "CullingConstants", &xSelf.m_xCullingConstantsBuffer.GetCBV());
	xBinder.BindUAV_Buffer(xSelf.m_xCullingShader, "Objects",          &xSelf.m_xObjectsBuffer.GetUAV());
	xBinder.BindUAV_Buffer(xSelf.m_xCullingShader, "DrawItems",        &xSelf.m_xDrawItemsBuffer.GetUAV());
	xBinder.BindUAV_Buffer(xSelf.m_xCullingShader, "bucketOffset",     &xSelf.m_xBucketOffsetBuffer.GetUAV());
	xBinder.BindUAV_Buffer(xSelf.m_xCullingShader, "visibleIndex",     &xSelf.m_xVisibleIndexBuffer.GetUAV());
	xBinder.BindUAV_Buffer(xSelf.m_xCullingShader, "indirect",         &xSelf.m_xIndirectBuffer.GetUAV());

	// One thread per (view, draw-item) — cull every active view's frustum in one dispatch.
	const u_int uCullThreads = xSelf.m_uTotalDrawItems * xSelf.m_uNumViews;
	pxCmdList->Dispatch((uCullThreads + 63u) / 64u, 1u, 1u);
}

static void ExecuteUnifiedGBuffer(Flux_CommandBuffer* pxCmdList, void*)
{
	Flux_UnifiedMeshImpl& xSelf = g_xEngine.UnifiedMesh();
	if (xSelf.m_axBucketDraws.GetSize() == 0u)
	{
		return;
	}

	Flux_ShaderBinder xBinder(*pxCmdList);

	// Two cull-mode passes (one-sided / two-sided), mirroring InstancedMeshes: the
	// pipeline + PASS block (scene SSBOs) + bindless table are bound once per pass.
	for (u_int uCullPass = 0; uCullPass < 2u; ++uCullPass)
	{
		const bool bTwoSidedPass = (uCullPass == 1u);
		pxCmdList->SetPipeline(bTwoSidedPass ? &xSelf.m_xGBufferPipelineTwoSided : &xSelf.m_xGBufferPipeline);
		pxCmdList->UseBindlessTextures(2);

		// PASS block (set 3) — scene records bound once per pass (BindDrawConstants on
		// the DRAW set 4 below never disturbs these).
		xBinder.BindSRV_Buffer(xSelf.m_xGBufferShader, "Objects",   xSelf.m_xObjectsBuffer.GetSRV());
		xBinder.BindSRV_Buffer(xSelf.m_xGBufferShader, "DrawItems", xSelf.m_xDrawItemsBuffer.GetSRV());

		for (u_int u = 0; u < xSelf.m_axBucketDraws.GetSize(); ++u)
		{
			const Flux_UnifiedBucketDraw& xDraw = xSelf.m_axBucketDraws.Get(u);
			const bool bDrawTwoSided = (xDraw.m_uCullMode == uFLUX_GPUSCENE_CULL_TWO_SIDED);
			if (bDrawTwoSided != bTwoSidedPass)
			{
				continue;
			}

			// SKINNED bucket: draw from the shared skinned arena at this instance's slice (byte
			// offset = arena vertex base * 72); the index buffer is the bind-pose mesh's. Static:
			// the registry mesh's own VB/IB.
			if (xDraw.m_bSkinned)
			{
				pxCmdList->SetVertexBuffer(xSelf.m_xSkinnedArenaBuffer, 0u, (size_t)xDraw.m_uVertexOffset * kuSKIN_OUTPUT_WORDS * sizeof(u_int));
			}
			else
			{
				pxCmdList->SetVertexBuffer(xDraw.m_pxMesh->GetVertexBuffer());
			}
			pxCmdList->SetIndexBuffer(xDraw.m_pxMesh->GetIndexBuffer());

			// DRAW block (set 4) — per-bucket constants + the bucket's visible slice + VAT texture.
			// BindDrawConstants resets set 4, so re-bind the visible-index SRV + VAT after it.
			UnifiedDrawConstants xConsts = {};
			xConsts.m_uMaterialIndex = xDraw.m_uMaterialIndex;
			xConsts.m_uBucketOffset  = xDraw.m_uVisibleOffset;
			xConsts.m_xVATParams     = xDraw.m_xVATParams;
			xBinder.BindDrawConstants(xSelf.m_xGBufferShader, "DrawConstants", &xConsts, sizeof(xConsts));
			xBinder.BindSRV_Buffer(xSelf.m_xGBufferShader, "VisibleIndexBuffer", xSelf.m_xVisibleIndexBuffer.GetSRV());
			BindBucketVAT(xBinder, xSelf.m_xGBufferShader, xDraw.m_pxVATTexture);

			// Index math via the shared Flux_Unified* helper (camera = view 0) — the same source of
			// truth RenderToShadowMap uses, so camera + shadow indirect addressing can never drift.
			const u_int uIndirectByteOffset =
				Flux_UnifiedIndirectCommandWord(0u, xDraw.m_uSlot, xSelf.m_uBucketSlotCount) * (u_int)sizeof(u_int);
			pxCmdList->DrawIndexedIndirect(&xSelf.m_xIndirectBuffer, 1u, uIndirectByteOffset, kuINDIRECT_STRIDE);
		}
	}
}

//=============================================================================
// Stage 2 — per-cascade shadow caster draw (invoked from Flux_Shadows::ExecuteShadowCascade,
// worker thread, inside cascade pass uCascade's record). Reads the shadow-view partition the
// cull populated for view (uCascade+1). One shadow pipeline draws every live bucket (no
// per-material cull split for shadows, matching Flux_StaticMeshes). Translucent/additive
// submeshes were already excluded from the GPU scene, so every bucket is a valid caster.
//=============================================================================
void Flux_UnifiedMeshImpl::RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade)
{
	if (m_axBucketDraws.GetSize() == 0u)
	{
		return;
	}
	// view 0 = camera; cascade c uses view c+1. If shadows were inactive this frame
	// (m_uNumViews == 1) the shadow-view partitions were never culled — skip.
	const u_int uView = uCascade + 1u;
	if (uView >= m_uNumViews)
	{
		return;
	}

	xCmdBuf.SetPipeline(&m_xShadowPipeline);
	Flux_ShaderBinder xBinder(xCmdBuf);
	xCmdBuf.UseBindlessTextures(2);   // base-colour table for the masked-cutout FS

	// PASS block (set 3) — scene records bound once for this cascade.
	xBinder.BindSRV_Buffer(m_xShadowShader, "Objects",   m_xObjectsBuffer.GetSRV());
	xBinder.BindSRV_Buffer(m_xShadowShader, "DrawItems", m_xDrawItemsBuffer.GetSRV());

	for (u_int u = 0; u < m_axBucketDraws.GetSize(); ++u)
	{
		const Flux_UnifiedBucketDraw& xDraw = m_axBucketDraws.Get(u);

		// SKINNED caster: same object-space arena slice the camera draws (one pose, all views).
		if (xDraw.m_bSkinned)
		{
			xCmdBuf.SetVertexBuffer(m_xSkinnedArenaBuffer, 0u, (size_t)xDraw.m_uVertexOffset * kuSKIN_OUTPUT_WORDS * sizeof(u_int));
		}
		else
		{
			xCmdBuf.SetVertexBuffer(xDraw.m_pxMesh->GetVertexBuffer());
		}
		xCmdBuf.SetIndexBuffer(xDraw.m_pxMesh->GetIndexBuffer());

		// DRAW block (set 4) — per-bucket constants (this view's visible base + cascade) + the
		// bucket's visible slice. BindDrawConstants resets set 4, so re-bind the SRV after it.
		// Index math via the shared Flux_Unified* helpers (CPU source of truth, unit-tested).
		UnifiedDrawConstants xConsts = {};
		xConsts.m_uMaterialIndex = xDraw.m_uMaterialIndex;
		xConsts.m_uBucketOffset  = Flux_UnifiedVisibleWriteIndex(uView, m_uTotalDrawItems, xDraw.m_uVisibleOffset, 0u);
		xConsts.m_uShadowCascade = uCascade;
		xConsts.m_xVATParams     = xDraw.m_xVATParams;
		xBinder.BindDrawConstants(m_xShadowShader, "DrawConstants", &xConsts, sizeof(xConsts));
		xBinder.BindSRV_Buffer(m_xShadowShader, "VisibleIndexBuffer", m_xVisibleIndexBuffer.GetSRV());
		BindBucketVAT(xBinder, m_xShadowShader, xDraw.m_pxVATTexture);

		const u_int uIndirectByteOffset =
			Flux_UnifiedIndirectCommandWord(uView, xDraw.m_uSlot, m_uBucketSlotCount) * (u_int)sizeof(u_int);
		xCmdBuf.DrawIndexedIndirect(&m_xIndirectBuffer, 1u, uIndirectByteOffset, kuINDIRECT_STRIDE);
	}
}
