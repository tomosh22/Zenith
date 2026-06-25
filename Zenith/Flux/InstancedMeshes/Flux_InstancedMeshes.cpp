#include "Zenith.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshes_Shaders.h"
#include "Core/Zenith_Engine.h"

#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Flux/InstancedMeshes/Flux_InstanceCulling.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RendererImpl.h"  // RequestGraphRebuild on group register/unregister
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/InstancedMeshes.h" // typed binding handles
#include "Flux/Flux_BackendTypes.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "ZenithECS/Zenith_Scene.h"
#include <vector>

//=============================================================================
// The instanced path uses the SHARED bindless material system — the GPU material
// record (Flux_MaterialGPU / g_axMaterials, selected by MeshDrawConstants::
// m_uMaterialIndex) + the bindless g_axTextures table — exactly like StaticMeshes/
// AnimatedMeshes. VAT animation-texture params (texW, texH, vat-enabled) ride the
// appended MeshDrawConstants::m_xVATParams field rather than a separate constant
// buffer: the backend allows only ONE scratch/draw-constants buffer per descriptor
// set (Zenith_Vulkan_CommandBuffer::BindDrawConstants), so a second BindDrawConstants
// on the DRAW set would clobber the DrawConstants binding. See BuildInstancedVATParams below.
//=============================================================================

// Build the per-group VAT params written into MeshDrawConstants::m_xVATParams:
// (texture width, texture height, vat-enabled, unused). Zero when the group has no
// VAT (vat-enabled = 0 -> the shader skips the VAT sample).
static Zenith_Maths::Vector4 BuildInstancedVATParams(const Flux_AnimationTexture* pxAnimTex)
{
	const bool bHasVAT = (pxAnimTex != nullptr && pxAnimTex->HasGPUResources());
	return bHasVAT
		? Zenith_Maths::Vector4(static_cast<float>(pxAnimTex->GetTextureWidth()),
			static_cast<float>(pxAnimTex->GetTextureHeight()), 1.0f, 0.0f)
		: Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
}

//=============================================================================
// Reset compute draw constants (16 bytes). Mirrors ResetDrawConstantsLayout in
// Flux_InstanceReset.slang: the GPU reset pass writes the per-group mesh index
// count into the indirect command's indexCount[0] every frame (replacing the
// old host seed that raced an in-flight indirect draw on a runtime SetMesh).
//=============================================================================
struct InstanceResetDrawConstants
{
	uint32_t m_uIndexCount;
	uint32_t m_uPad0;
	uint32_t m_uPad1;
	uint32_t m_uPad2;
};
static_assert(sizeof(InstanceResetDrawConstants) == 16, "InstanceResetDrawConstants must be 16 bytes");

//=============================================================================
// Static Data
//=============================================================================


// Registered instance groups

// GBuffer rendering pipeline

// Shadow map rendering pipeline

// Culling compute pipeline

// Statistics


//=============================================================================
// Initialise / Shutdown
//=============================================================================

static void ExecuteCullReset(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteCulling(Flux_CommandBuffer* pxCmdList, void* pUserData);
static void ExecuteInstancedGBuffer(Flux_CommandBuffer* pxCmdList, void* pUserData);

void Flux_InstancedMeshesImpl::BuildPipelines()
{
	// Load shaders
	m_xGBufferShader.Initialise(Flux_InstancedMeshesShaders::xInstancedMesh_ToGBuffer);
	m_xShadowShader.Initialise(Flux_InstancedMeshesShaders::xInstancedMesh_ToShadowmap);

	// Vertex input description - same as static meshes (position, UV, normal, tangent, bitangent, color)
	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Position
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);  // UV
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Normal
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Tangent
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Bitangent
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);  // Color
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	// GBuffer pipeline
	{
		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE] = MRT_FORMAT_EMISSIVE;
		xPipelineSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &m_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;
		// Backface-cull. Flux_PipelineSpecification defaults to CULL_MODE_NONE, but
		// instanced meshes are AUTHORED for backface culling: the procedural tree
		// leaf cards emit BOTH triangle windings per quad (coplanar duplicates) so a
		// leaf is visible from either side AFTER the backface cull drops the away-
		// facing winding (CreateTreeLeavesMesh, Tools/Zenith_Tools_TreeAssetExport.cpp:
		// "the instanced pipeline backface-culls; leaves must read from both sides").
		// With CULL_MODE_NONE both coplanar windings drew at once and z-fought; the
		// leaves' 7.5deg VAT sway shifted the per-pixel depth contest every frame ->
		// the canopy flickered (trunk is single-winding solid, so it was stable).
		xPipelineSpec.m_eCullMode = CULL_MODE_BACK;

		m_xGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipeline, xPipelineSpec);

		// Two-sided variant (CULL_MODE_NONE) for materials flagged m_bTwoSided, mirroring
		// StaticMeshes/AnimatedMeshes: ExecuteInstancedGBuffer selects per-group so a
		// two-sided instanced material keeps its back faces (its shader normal-flip stays
		// meaningful). The double-winding leaf cards are NOT flagged two-sided, so they
		// stay on the one-sided CULL_MODE_BACK pipeline above -> the flicker fix is preserved.
		xPipelineSpec.m_eCullMode = CULL_MODE_NONE;
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipelineTwoSided, xPipelineSpec);
	}

	// Shadow pipeline
	{
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &m_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;
		// Match the GBuffer: backface-cull so the leaf cards' coplanar double-winding
		// doesn't double-write the shadow depth (same authoring contract as above).
		xShadowPipelineSpec.m_eCullMode = CULL_MODE_BACK;
		// Fixed-function slope-scaled depth bias (set per-cascade via vkCmdSetDepthBias
		// in Flux_Shadows::ExecuteShadowCascade). Dynamic so it is runtime-tunable.
		xShadowPipelineSpec.m_bDepthBias = true;
		xShadowPipelineSpec.m_bDynamicDepthBias = true;
		xShadowPipelineSpec.m_fDepthBiasConstant = 1.75f;
		xShadowPipelineSpec.m_fDepthBiasSlope = 3.0f;

		m_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(m_xShadowPipeline, xShadowPipelineSpec);
	}

	// Culling compute pipeline
	{
		m_xCullingShader.Initialise(Flux_InstancedMeshesShaders::xInstanceCulling);

		// Build compute root signature from shader reflection
		const Flux_ShaderReflection& xCullingReflection = m_xCullingShader.GetReflection();
		Flux_RootSigBuilder::FromReflection(m_xCullingRootSig, xCullingReflection);

		// Build compute pipeline
		Flux_ComputePipelineBuilder xComputeBuilder;
		xComputeBuilder.WithShader(m_xCullingShader)
			.WithLayout(m_xCullingRootSig.m_xLayout)
			.Build(m_xCullingPipeline);

		m_xCullingPipeline.m_xRootSig = m_xCullingRootSig;
	}

	// Reset compute pipeline (zeroes the persistent visible-count + indirect
	// instanceCount each frame, before culling). Mirrors the culling setup.
	{
		m_xResetShader.Initialise(Flux_InstancedMeshesShaders::xInstanceReset);

		const Flux_ShaderReflection& xResetReflection = m_xResetShader.GetReflection();
		Flux_RootSigBuilder::FromReflection(m_xResetRootSig, xResetReflection);

		Flux_ComputePipelineBuilder xResetBuilder;
		xResetBuilder.WithShader(m_xResetShader)
			.WithLayout(m_xResetRootSig.m_xLayout)
			.Build(m_xResetPipeline);

		m_xResetPipeline.m_xRootSig = m_xResetRootSig;
	}
}

void Flux_InstancedMeshesImpl::Initialise()
{
	BuildPipelines();

	// One-time setup that hot-reload must NOT repeat (would leak VRAM).
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_CullingConstants), m_xCullingConstantsBuffer);
	m_bCullingInitialized = true;

#ifdef ZENITH_DEBUG_VARIABLES
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes initialised (GPU culling enabled)");
}

void Flux_InstancedMeshesImpl::Shutdown()
{
	ClearAllGroups();
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_xCullingConstantsBuffer);

	// Pipelines reference their shaders, so destroy pipelines first (mirrors
	// Flux_StaticMeshesImpl::Shutdown). Reset() is idempotent on an unbuilt
	// pipeline/shader, so this is safe even if GPU culling never initialised.
	m_xGBufferPipeline.Reset();
	m_xGBufferPipelineTwoSided.Reset();
	m_xShadowPipeline.Reset();
	m_xCullingPipeline.Reset();
	m_xResetPipeline.Reset();
	m_xGBufferShader.Reset();
	m_xShadowShader.Reset();
	m_xCullingShader.Reset();
	m_xResetShader.Reset();

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes shutdown");
}

void Flux_InstancedMeshesImpl::Reset()
{
	// Reset is handled by the render graph
	// Update statistics
	m_uTotalInstances = 0;
	m_uVisibleInstances = 0;

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshesImpl::Reset()");
}

//=============================================================================
// Instance Group Registration
//=============================================================================

void Flux_InstancedMeshesImpl::RegisterInstanceGroup(Flux_InstanceGroup* pxGroup)
{
	if (!pxGroup)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Flux_InstancedMeshesImpl::RegisterInstanceGroup - null group");
		return;
	}

	// Check if already registered
	for (u_int i = 0; i < m_apxInstanceGroups.GetSize(); ++i)
	{
		if (m_apxInstanceGroups.Get(i) == pxGroup)
		{
			return;  // Already registered
		}
	}

	m_apxInstanceGroups.PushBack(pxGroup);
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Registered instance group (total: %u)", m_apxInstanceGroups.GetSize());

	// The group set is part of SetupRenderGraph's per-group buffer declarations, so
	// a change to it must re-run SetupRenderGraph (re-runs every subsystem's setup
	// + recompiles cached barriers). RequestGraphRebuild defers the rebuild to the
	// next safe point (mirrors Zenith_TerrainComponent on terrain create/destroy).
	g_xEngine.FluxRenderer().RequestGraphRebuild();
}

void Flux_InstancedMeshesImpl::UnregisterInstanceGroup(Flux_InstanceGroup* pxGroup)
{
	for (u_int i = 0; i < m_apxInstanceGroups.GetSize(); ++i)
	{
		if (m_apxInstanceGroups.Get(i) == pxGroup)
		{
			// Swap with last and pop
			m_apxInstanceGroups.RemoveSwap(i);
			Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Unregistered instance group (remaining: %u)", m_apxInstanceGroups.GetSize());

			// Group set changed -> re-run SetupRenderGraph (see RegisterInstanceGroup).
			g_xEngine.FluxRenderer().RequestGraphRebuild();
			return;
		}
	}
}

void Flux_InstancedMeshesImpl::ClearAllGroups()
{
	m_apxInstanceGroups.Clear();
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Cleared all instance groups");
}

//=============================================================================
// Per-Frame Rendering
//=============================================================================

void Flux_InstancedMeshesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// SetupRenderGraph runs ONCE at init and on every RequestGraphRebuild() (which
	// re-runs every subsystem's SetupRenderGraph). The set of instance groups is
	// dynamic, so RegisterInstanceGroup / UnregisterInstanceGroup call
	// RequestGraphRebuild() — this loop re-declares the per-group buffer Reads/
	// Writes against the CURRENT group set each rebuild, and the cached compiled
	// barriers stay correct.
	//
	// Three passes, each declaring the PERSISTENT (graph-trackable) per-group GPU
	// buffers it touches. The transform/anim/culling-constants buffers are
	// frame-indexed (dynamic) and therefore intentionally NOT declared — the graph
	// cannot track a buffer whose physical handle changes per frame; their sync is
	// handled by frame indexing + the implicit host-write barrier (see Flux_Buffers.h).
	//
	//   1. "Instanced Cull Reset" (compute) — zeroes each group's persistent
	//      visible-count + indirect-instanceCount buffers on device.
	//   2. "Instanced Meshes Culling" (compute) — writes the visible-index list and
	//      bumps the indirect instanceCount; depends on the reset.
	//   3. "Instanced Meshes GBuffer" (graphics) — reads the visible-index +
	//      indirect buffers for the indirect draw; depends on the culling.
	//
	// WS7 keystone: the gather Prepare is hung on the culling pass. CallPrepareCallbacks
	// runs it on the main thread before any record task dispatches, so it is the
	// SINGLE writer of every group's per-frame CPU/GPU-sync state. All three record
	// callbacks then only READ that frozen state on their worker threads.

	// Pass 1: GPU reset compute.
	Flux_PassHandle xResetPass = xGraph.AddPass("Instanced Cull Reset", ExecuteCullReset);

	// Pass 2: GPU culling compute. The gather Prepare is hung here.
	Flux_PassHandle xCullingPass = xGraph.AddPass("Instanced Meshes Culling", ExecuteCulling)
		.Prepare([](void* p){ g_xEngine.InstancedMeshes().GatherInstancedPacket(p); })
		.DependsOn(xResetPass);

	// Pass 3: GBuffer render.
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_PassHandle xGBufferPass = xGraph.AddPass("Instanced Meshes GBuffer", ExecuteInstancedGBuffer)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),			RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT),	RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),						RESOURCE_ACCESS_WRITE_DSV)
		.DependsOn(xCullingPass);

	// Per-group buffer declarations. The builder chain is &&-qualified (single
	// expression only), so declare the loop-driven per-group buffers via the
	// graph's handle-taking ReadBuffer/WriteBuffer helpers (mirrors the HiZ per-mip
	// loop). Skip groups whose GPU buffers aren't initialised yet — the execute
	// callbacks apply the same skip, so the declared set matches the bound set.
	for (u_int uGroup = 0; uGroup < m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = m_apxInstanceGroups.Get(uGroup);
		if (!pxGroup || !pxGroup->HasGPUBuffers())
		{
			continue;
		}

		Flux_Buffer& xVisibleIndex = pxGroup->GetVisibleIndexBuffer().GetBuffer();
		Flux_Buffer& xVisibleCount = pxGroup->GetVisibleCountBuffer().GetBuffer();
		Flux_Buffer& xIndirect     = pxGroup->GetIndirectBuffer().GetBuffer();

		// Reset writes the count + indirect (pure write — full overwrite).
		xGraph.WriteBuffer(xResetPass, xVisibleCount, RESOURCE_ACCESS_WRITE_UAV);
		xGraph.WriteBuffer(xResetPass, xIndirect,     RESOURCE_ACCESS_WRITE_UAV);

		// Culling writes the visible-index list (pure write), and read-modify-writes
		// the count + indirect (atomic increments) — so READWRITE on those two so
		// the graph orders them after the reset's writes.
		xGraph.WriteBuffer(xCullingPass, xVisibleIndex, RESOURCE_ACCESS_WRITE_UAV);
		xGraph.WriteBuffer(xCullingPass, xVisibleCount, RESOURCE_ACCESS_READWRITE_UAV);
		xGraph.WriteBuffer(xCullingPass, xIndirect,     RESOURCE_ACCESS_READWRITE_UAV);

		// GBuffer reads the visible-index list (SSBO) and the indirect args.
		xGraph.ReadBuffer(xGBufferPass, xVisibleIndex, RESOURCE_ACCESS_READ_BUFFER_SRV);
		xGraph.ReadBuffer(xGBufferPass, xIndirect,     RESOURCE_ACCESS_READ_INDIRECT_ARG);
	}
}

// WS7 keystone gather (main thread, via .Prepare). This is the SINGLE writer of
// every instance group's per-frame CPU/GPU-sync state. It performs, once on the
// main thread, exactly the per-group work the record callbacks used to do
// concurrently:
//   - UpdateGPUBuffers(): uploads dirty transform/anim data and rebuilds the CPU
//     visible-index list + m_uVisibleCount. Needed by BOTH the GPU-culling path
//     (fresh transform/anim before the compute dispatch) and the CPU-fallback
//     path (the ExecuteInstancedGBuffer !bUseGPUCulling branch used to call this).
//   - When GPU culling is active: uploads the per-group culling constants into the
//     shared subsystem buffer (same single-buffer overwrite semantics as before —
//     the dispatches bind+read it at GPU time). The persistent visible-count +
//     indirect-instanceCount are NOT host-reset here — the ExecuteCullReset compute
//     pass zeroes them on-device each frame (a host upload would race the prior
//     frame's indirect draw).
//   - Accumulates the m_uTotalInstances / m_uVisibleInstances stats that
//     ExecuteInstancedGBuffer used to write in its draw loop.
// After this returns the group state is frozen; the record callbacks are readers.
void Flux_InstancedMeshesImpl::GatherInstancedPacket(void*)
{
	ZENITH_PROFILE_SCOPE("Flux Instanced Gather & Update");
	Flux_InstancedMeshesImpl& xSelf = *this;

	// Reset stats up-front (matches ExecuteInstancedGBuffer's old pre-loop reset).
	xSelf.m_uTotalInstances = 0;
	xSelf.m_uVisibleInstances = 0;

	if (xSelf.m_apxInstanceGroups.GetSize() == 0)
	{
		return;
	}

	// Register every group's material with the GPU material table (MAIN THREAD —
	// assigns the table index + builds the record + makes its textures bindless). The
	// worker draw path (BindBatchDescriptors) reads the index off the asset lock-free.
	{
		Flux_MaterialTable& xTable = g_xEngine.FluxGraphics().MaterialTable();
		Zenith_MaterialAsset* pxBlank = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();
		for (u_int u = 0; u < xSelf.m_apxInstanceGroups.GetSize(); ++u)
		{
			Flux_InstanceGroup* pxRegGroup = xSelf.m_apxInstanceGroups.Get(u);
			if (!pxRegGroup) continue;
			Zenith_MaterialAsset* pxMat = pxRegGroup->GetMaterial();
			if (!pxMat) pxMat = pxBlank;
			if (pxMat) xTable.GetOrCreateIndex(pxMat);
		}
	}

	// Same predicate ExecuteCulling's outer guard / ExecuteInstancedGBuffer's
	// bUseGPUCulling used. When false we still rebuild the CPU visible list
	// (UpdateGPUBuffers) so the GBuffer CPU-fallback draw has a current count.
	const bool bUseGPUCulling = xSelf.m_bCullingInitialized && xSelf.m_bCullingEnabled && Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled;

	// Camera matrices for the culling constants (frame constants, already computed
	// this frame — main-thread-safe reads). Only needed on the GPU-culling path,
	// matching the old ExecuteCulling which fetched them under the same guard.
	Zenith_Maths::Matrix4 xViewProjMatrix(1.0f);
	Zenith_Maths::Vector3 xCameraPos(0.0f);
	if (bUseGPUCulling)
	{
		Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
		xViewProjMatrix = xGraphics.GetViewProjMatrix();
		xCameraPos = xGraphics.GetCameraPosition();
	}

	for (u_int uGroup = 0; uGroup < xSelf.m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = xSelf.m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
		if (!pxGroup || pxGroup->IsEmpty())
		{
			continue;
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// Upload CPU data to GPU + rebuild the enabled-index list (sets
		// m_uEnabledCount / m_uVisibleCount). In BOTH modes this uploads the
		// frame-indexed transform/anim buffers and the frame-indexed enabled-index
		// buffer (used by the shadow pass in all modes + the CPU-fallback GBuffer
		// draw). The persistent visible-index buffer is GPU-written by the culling
		// compute and is never host-uploaded here.
		pxGroup->UpdateGPUBuffers(bUseGPUCulling);

		if (bUseGPUCulling)
		{
			// GPU path: the persistent visible-count + indirect-instanceCount
			// buffers are reset on device by the graph-tracked GPU reset compute
			// pass (ExecuteCullReset) — NOT by a host upload here, which would race
			// the prior frame's indirect draw. So no ResetVisibleCount() call.

			// Build + upload culling constants into the shared subsystem buffer.
			// Single-buffer overwrite semantics preserved exactly (UploadBufferData
			// is an immediate, mutex-guarded copy).
			Flux_CullingConstants xCullingConstants;
			Flux_InstanceCullingUtil::ExtractFrustumPlanes(xViewProjMatrix, xCullingConstants.m_axFrustumPlanes);
			xCullingConstants.m_xCameraPosition = Zenith_Maths::Vector4(xCameraPos, 0.0f);
			xCullingConstants.m_uTotalInstanceCount = pxGroup->GetInstanceCount();
			xCullingConstants.m_uMeshIndexCount = pxMesh->GetNumIndices();
			xCullingConstants.m_fBoundingSphereRadius = pxGroup->GetBounds().m_fRadius;
			xCullingConstants.m_fPadding = 0.0f;

			g_xEngine.FluxMemory().UploadBufferData(
				xSelf.m_xCullingConstantsBuffer.GetBuffer().m_xVRAMHandle,
				&xCullingConstants,
				sizeof(xCullingConstants));
		}

		// Stats (moved from ExecuteInstancedGBuffer's draw loop). Visible count is
		// not GPU-accurate on the culling path (would need a readback), so we use
		// the instance count there — identical to the old behaviour.
		xSelf.m_uTotalInstances += pxGroup->GetInstanceCount();
		xSelf.m_uVisibleInstances += bUseGPUCulling ? pxGroup->GetInstanceCount() : pxGroup->GetVisibleCount();
	}
}

static void ExecuteCullReset(Flux_CommandBuffer* pxCmdList, void*)
{
	// PURE READER (worker thread). Zeroes each group's persistent visible-count +
	// indirect-instanceCount on device. The render graph synthesises the barriers
	// between this pass, the culling pass (which read-modify-writes the same two
	// buffers) and the GBuffer indirect draw, from the per-group buffer Reads/
	// Writes declared in SetupRenderGraph.
	Flux_InstancedMeshesImpl& xZZ = g_xEngine.InstancedMeshes();

	// Same predicate the culling pass + gather use. When GPU culling is disabled
	// the reset is a harmless no-op (CPU path doesn't use the indirect buffer).
	if (!xZZ.m_bCullingInitialized || !xZZ.m_bCullingEnabled || !Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled)
	{
		return;
	}

	if (xZZ.m_apxInstanceGroups.GetSize() == 0)
	{
		return;
	}

	pxCmdList->BindComputePipeline(&xZZ.m_xResetPipeline);

	Flux_ShaderBinder xBinder(*pxCmdList);
	namespace RS = Flux_Generated_InstancedMeshes::InstanceReset;

	for (u_int uGroup = 0; uGroup < xZZ.m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = xZZ.m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
		// Skip groups whose GPU buffers don't exist yet — matches the
		// SetupRenderGraph per-group declaration guard so the bound set equals the
		// declared set.
		if (!pxGroup || pxGroup->IsEmpty() || !pxGroup->HasGPUBuffers())
		{
			continue;
		}

		// The reset shader now writes the full VkDrawIndexedIndirectCommand,
		// including indexCount[0] from the mesh — so a group with no mesh has no
		// index count to write (and never draws). Skip it.
		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// Per-group draw constants: the mesh index count the reset writes into the
		// indirect command (mirrors the GBuffer/shadow DrawConstants binding).
		InstanceResetDrawConstants xResetConstants = {};
		xResetConstants.m_uIndexCount = pxMesh->GetNumIndices();
		xBinder.BindDrawConstants(RS::hDrawConstants, &xResetConstants, sizeof(xResetConstants));

		// binding 0 = visibleCount, binding 1 = indirectInstanceCount.
		xBinder.BindUAV_Buffer(RS::hvisibleCount, &pxGroup->GetVisibleCountBuffer().GetUAV());
		xBinder.BindUAV_Buffer(RS::hindirectInstanceCount, &pxGroup->GetIndirectBuffer().GetUAV());

		pxCmdList->Dispatch(1, 1, 1);
	}
}

static void ExecuteCulling(Flux_CommandBuffer* pxCmdList, void*)
{
	// PURE READER (worker thread). All CPU/GPU-sync mutation — UpdateGPUBuffers
	// and the culling-constants upload — was relocated to
	// GatherInstancedPacket (.Prepare, main thread). This callback only binds the
	// now-frozen per-group buffers and dispatches the culling compute.

	// Trampoline: recover the subsystem singleton first, then route all reaches
	// through its members.
	Flux_InstancedMeshesImpl& xZZ = g_xEngine.InstancedMeshes();

	// Check if GPU culling should run
	if (!xZZ.m_bCullingInitialized || !xZZ.m_bCullingEnabled || !Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled)
	{
		return;
	}

	if (xZZ.m_apxInstanceGroups.GetSize() == 0)
	{
		return;
	}

	pxCmdList->BindComputePipeline(&xZZ.m_xCullingPipeline);

	// Create binder for compute shader bindings
	Flux_ShaderBinder xBinder(*pxCmdList);
	namespace CL = Flux_Generated_InstancedMeshes::InstanceCulling;

	for (u_int uGroup = 0; uGroup < xZZ.m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = xZZ.m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
		// HasGPUBuffers skip matches the SetupRenderGraph per-group declaration
		// guard so the bound set equals the graph-declared set.
		if (!pxGroup || pxGroup->IsEmpty() || !pxGroup->HasGPUBuffers())
		{
			continue;
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// Bind resources (culling constants were uploaded into the shared buffer by
		// GatherInstancedPacket; we just bind its CBV here).
		xBinder.BindCBV(CL::hCullingConstants, &xZZ.m_xCullingConstantsBuffer.GetCBV());
		xBinder.BindUAV_Buffer(CL::hTransformBuffer, &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(CL::hAnimDataBuffer, &pxGroup->GetAnimDataBuffer().GetUAV());
		xBinder.BindUAV_Buffer(CL::hVisibleIndexBuffer, &pxGroup->GetVisibleIndexBuffer().GetUAV());
		xBinder.BindUAV_Buffer(CL::hvisibleCount, &pxGroup->GetVisibleCountBuffer().GetUAV());
		xBinder.BindUAV_Buffer(CL::hindirectInstanceCount, &pxGroup->GetIndirectBuffer().GetUAV());

		// Dispatch compute shader: 64 threads per workgroup
		uint32_t uNumWorkgroups = (pxGroup->GetInstanceCount() + 63) / 64;
		pxCmdList->Dispatch(uNumWorkgroups, 1, 1);
	}
}

// Bind per-batch material, animation texture, push constants, and instance
// buffers for the GBuffer pass. Caller must have bound the shared pipeline
// and FrameConstants before invoking this.
//
// bUseGPUCulling selects the index-list source for VisibleIndexBuffer:
//   - GPU cull: the PERSISTENT visible-index buffer (camera-culled compact list
//     the cull compute wrote on device), bound via its SRV. It is graph-tracked;
//     the GBuffer pass declares a READ on it (see SetupRenderGraph).
//   - CPU fallback: the FRAME-INDEXED enabled-index buffer (all enabled slots).
//     It is NOT graph-tracked, so the binder skips the declared-read check; the
//     persistent buffer's static graph READ declaration is harmlessly unused.
void Flux_InstancedMeshesImpl::BindBatchDescriptors(Flux_ShaderBinder& xBinder, Flux_InstanceGroup* pxGroup, bool bUseGPUCulling)
{
	namespace GB = Flux_Generated_InstancedMeshes::InstancedMesh_ToGBuffer;
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Zenith_MaterialAsset* pxMaterial = pxGroup->GetMaterial();
	if (!pxMaterial)
	{
		pxMaterial = xGraphics.m_xBlankMaterial.GetDirect();
	}

	Flux_AnimationTexture* pxAnimTex = pxGroup->GetAnimationTexture();
	const bool bHasVAT = (pxAnimTex != nullptr && pxAnimTex->HasGPUResources());

	// Per-draw constants. The per-instance model matrix comes from the transform
	// buffer, so the constants' model matrix is unused (identity); the material-table
	// index selects the per-material record. VAT params ride the appended m_xVATParams
	// field (single scratch CB per set). Material textures are bindless (set 2), bound
	// once per pass; the material was registered on the main thread at gather.
	u_int uMaterialIndex = pxMaterial->GetMaterialTableIndex();
	if (uMaterialIndex == uFLUX_INVALID_MATERIAL_INDEX) uMaterialIndex = 0u;
	MeshDrawConstants xMaterialConstants;
	BuildMeshDrawConstants(xMaterialConstants, glm::identity<glm::mat4>(), uMaterialIndex);
	xMaterialConstants.m_xVATParams = BuildInstancedVATParams(pxAnimTex);
	xBinder.BindDrawConstants(GB::hDrawConstants, &xMaterialConstants, sizeof(xMaterialConstants));

	// Bind animation texture (VAT) if available, else bind blank texture.
	// The VAT position texture MUST be sampled with the POINT (nearest/no-aniso)
	// sampler: its texels are exact per-(vertex,frame) positions, and adjacent
	// columns are different mesh vertices metres apart — the default linear+aniso
	// sampler cross-blends them, and because both vertices animate the blended
	// position varies every frame, distorting/jittering the whole mesh (only
	// visible while the animation advances). See InitialisePointClamp.
	if (bHasVAT)
	{
		xBinder.BindSRV(GB::hg_xAnimationTex, &pxAnimTex->GetPositionTexture()->m_xSRV,
			&xGraphics.m_xPointSampler);
	}
	else
	{
		xBinder.BindSRV(GB::hg_xAnimationTex, &xGraphics.m_xWhiteTexture.GetDirect()->m_xSRV);
	}

	// Bind instance buffers. Transform/AnimData are frame-indexed (not graph-
	// tracked) so the UAV bind is fine. The VisibleIndexBuffer source depends on
	// the culling mode (see header comment): GPU-cull binds the persistent
	// graph-tracked buffer (SRV, declared READ by the pass); CPU-fallback binds the
	// frame-indexed enabled-index buffer (SRV, not graph-tracked → skips the
	// declared-read check). Both are read-only StructuredBuffer<uint> in the shader.
	xBinder.BindUAV_Buffer(GB::hTransformBuffer, &pxGroup->GetTransformBuffer().GetUAV());
	xBinder.BindUAV_Buffer(GB::hAnimDataBuffer, &pxGroup->GetAnimDataBuffer().GetUAV());
	if (bUseGPUCulling)
	{
		xBinder.BindSRV_Buffer(GB::hVisibleIndexBuffer, pxGroup->GetVisibleIndexBuffer().GetSRV());
	}
	else
	{
		xBinder.BindSRV_Buffer(GB::hVisibleIndexBuffer, pxGroup->GetEnabledIndexBuffer().GetSRV());
	}
}

// Emit the draw call(s) for one instance group. GPU-culling path uses an
// indirect draw whose count was written by ExecuteCulling; CPU fallback
// uses a direct instanced draw sized by the CPU-built visible list.
static void IssueBatchDraw(Flux_CommandBuffer* pxCmdList, Flux_InstanceGroup* pxGroup, Flux_MeshInstance* pxMesh, bool bUseGPUCulling)
{
	if (bUseGPUCulling)
	{
		// GPU culling: use indirect draw with count from visible count buffer
		// The culling compute shader wrote the visible instance count to the indirect buffer
		pxCmdList->DrawIndexedIndirect(
			&pxGroup->GetIndirectBuffer(),
			1,   // drawCount = 1 (single draw call)
			0,   // offset = 0
			20   // stride = sizeof(VkDrawIndexedIndirectCommand) = 20 bytes
		);
		return;
	}

	// CPU culling fallback: direct instanced draw over every ENABLED instance
	// (the frame-indexed enabled-index buffer bound in BindBatchDescriptors maps
	// SV_InstanceID -> enabled slot). No camera culling on this path.
	uint32_t uEnabledCount = pxGroup->GetEnabledCount();
	if (uEnabledCount > 0)
	{
		pxCmdList->DrawIndexed(pxMesh->GetNumIndices(), uEnabledCount);
	}
}

static void ExecuteInstancedGBuffer(Flux_CommandBuffer* pxCmdList, void*)
{
	// PURE READER (worker thread). The CPU-fallback UpdateGPUBuffers and the
	// per-group stat writes were relocated to GatherInstancedPacket (.Prepare,
	// main thread). This callback binds the now-frozen per-group buffers and
	// emits the draws only.

	if (!Zenith_GraphicsOptions::Get().m_bInstancedMeshesEnabled)
	{
		return;
	}

	// Trampoline: recover the subsystem singleton first, then route all reaches
	// through its members.
	Flux_InstancedMeshesImpl& xZZ = g_xEngine.InstancedMeshes();

	if (xZZ.m_apxInstanceGroups.GetSize() == 0)
	{
		return;
	}

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);
	namespace GB = Flux_Generated_InstancedMeshes::InstancedMesh_ToGBuffer;

	const bool bUseGPUCulling = xZZ.m_bCullingEnabled && Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled && xZZ.m_bCullingInitialized;

	// Two cull passes, mirroring StaticMeshes/AnimatedMeshes: pass 0 is one-sided
	// (CULL_MODE_BACK — the default the double-winding leaf cards rely on), pass 1 is
	// two-sided (CULL_MODE_NONE, for materials flagged m_bTwoSided so their back faces +
	// shader normal-flip stay meaningful). Each group draws in exactly one pass; the
	// pipeline + FrameConstants are bound once per pass. Almost all groups are one-sided,
	// so pass 1 is usually empty.
	for (u_int uCullPass = 0; uCullPass < 2; ++uCullPass)
	{
		const bool bTwoSidedPass = (uCullPass == 1);
		pxCmdList->SetPipeline(bTwoSidedPass ? &xZZ.m_xGBufferPipelineTwoSided : &xZZ.m_xGBufferPipeline);
		// View constants (camera VIEW set) + bindless materials: g_axMaterials (GLOBAL
		// set) + the g_axTextures table (set 2), bound once per pass after SetPipeline.
		xBinder.BindCBV(GB::hg_xView, &g_xEngine.FluxGraphics().m_xViewConstantsBuffer.GetCBV());
		// g_axMaterials is a DRAW-set member: staged once here, re-written into the set each
		// draw alongside DrawConstants. Plus the g_axTextures table (set 2).
		xBinder.BindSRV_Buffer(GB::hg_axMaterials, g_xEngine.FluxGraphics().MaterialTable().GetSRV());
		pxCmdList->UseBindlessTextures(2);

		for (u_int uGroup = 0; uGroup < xZZ.m_apxInstanceGroups.GetSize(); ++uGroup)
		{
			Flux_InstanceGroup* pxGroup = xZZ.m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
			// HasGPUBuffers skip matches the SetupRenderGraph per-group declaration
			// guard so the buffers this pass reads were declared on it.
			if (!pxGroup || pxGroup->IsEmpty() || !pxGroup->HasGPUBuffers())
			{
				continue;
			}

			Zenith_MaterialAsset* pxGroupMat = pxGroup->GetMaterial();

			// Blend-mode parity with StaticMeshes/AnimatedMeshes: translucent/additive
			// materials don't render in the opaque G-buffer (they belong on the forward
			// Translucency path). There is no instanced forward path, so such groups simply
			// don't render — a documented limitation (the instanced consumer is trees:
			// opaque trunk + masked leaves). Instanced supports OPAQUE + MASKED.
			if (pxGroupMat != nullptr)
			{
				const MaterialBlendMode eBlend = pxGroupMat->GetResolved().m_xParams.m_eBlendMode;
				if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
				{
					continue;
				}
			}

			// Bin by cull mode: each group renders in exactly one pass. A null material
			// falls to the one-sided pass (the blank fallback BindBatchDescriptors uses is
			// opaque/one-sided), matching the descriptor it will bind.
			const bool bGroupTwoSided = (pxGroupMat != nullptr) && pxGroupMat->GetResolved().m_xParams.m_bTwoSided;
			if (bGroupTwoSided != bTwoSidedPass)
			{
				continue;
			}

			Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
			if (!pxMesh)
			{
				continue;
			}

			// Set vertex and index buffers from mesh
			pxCmdList->SetVertexBuffer(pxMesh->GetVertexBuffer());
			pxCmdList->SetIndexBuffer(pxMesh->GetIndexBuffer());

			xZZ.BindBatchDescriptors(xBinder, pxGroup, bUseGPUCulling);
			IssueBatchDraw(pxCmdList, pxGroup, pxMesh, bUseGPUCulling);
		}
	}
}

void Flux_InstancedMeshesImpl::RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// WIRED into Flux_Shadows::ExecuteShadowCascade (per cascade). Instanced meshes
	// (incl. the terrain trees) cast shadows over ALL ENABLED casters — there is NO
	// camera culling here: a shadow caster off-screen (or behind the camera) can
	// still cast into the view, so the camera-culled GPU compute output
	// (GetVisibleIndexBuffer) would be WRONG. Instead this draws the frame-indexed
	// enabled-index buffer (every slot with flags != 0) and GetEnabledCount().
	//
	// No render-graph declaration is needed for the buffers read here:
	//   - The enabled-index buffer + the transform buffer are BOTH frame-indexed
	//     (Flux_DynamicReadWriteBuffer) → never graph-tracked (their host upload in
	//     GatherInstancedPacket's Prepare runs on the main thread before any pass
	//     records, and frame indexing covers cross-frame sync). The binder skips
	//     the declared-access check for frame-indexed buffers.
	//   - The shadow depth target write is already declared by the cascade pass.
	// The shadow shader reads TransformBuffer[VisibleIndexBuffer[i]] — correct for
	// all enabled casters.
	if (!Zenith_GraphicsOptions::Get().m_bInstancedMeshesEnabled)
	{
		return;
	}

	if (m_apxInstanceGroups.GetSize() == 0)
	{
		return;
	}

	// Bind the instanced shadow pipeline here (not in the caller) so games with no
	// instanced casters don't pay a redundant per-cascade pipeline bind on the
	// shared shadow path, and the depth-bias dynamic state the cascade set earlier
	// carries through (it is per-command-buffer, not per-pipeline).
	xCmdBuf.SetPipeline(&m_xShadowPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);
	namespace SM = Flux_Generated_InstancedMeshes::InstancedMesh_ToShadowmap;
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	// Shadow pass: per-draw DrawConstants + ShadowMatrix + transform/visible-index
	// SSBOs; the masked cutout samples the base colour bindlessly. g_axMaterials
	// (a DRAW-set member: staged once, re-written into the set each draw alongside
	// DrawConstants) + the g_axTextures table (set 2) are bound once here (after the
	// shadow pipeline bind above).
	xBinder.BindSRV_Buffer(SM::hg_axMaterials, xGraphics.MaterialTable().GetSRV());
	xCmdBuf.UseBindlessTextures(2);

	for (u_int uGroup = 0; uGroup < m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
		// HasGPUBuffers skip: the enabled-index + transform buffers must exist.
		if (!pxGroup || pxGroup->IsEmpty() || !pxGroup->HasGPUBuffers())
		{
			continue;
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// Material carries the alpha cutoff the shadow FS reads for masked cutout.
		// Mirror BindBatchDescriptors' blank fallback for a null group material, and
		// skip translucent/additive casters (parity with the static shadow path).
		Zenith_MaterialAsset* pxMaterial = pxGroup->GetMaterial();
		if (!pxMaterial)
		{
			pxMaterial = xGraphics.m_xBlankMaterial.GetDirect();
		}
		const MaterialBlendMode eBlend = pxMaterial->GetResolved().m_xParams.m_eBlendMode;
		if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
		{
			continue;
		}

		// Set vertex and index buffers
		xCmdBuf.SetVertexBuffer(pxMesh->GetVertexBuffer());
		xCmdBuf.SetIndexBuffer(pxMesh->GetIndexBuffer());

		// Material constants carry the alpha cutoff + UV transform + base-colour tint
		// the masked-cutout fragment shader reads (opaque materials write cutoff 0 →
		// the FS uniform-branches out → depth-only). Model matrix is per-instance, so
		// the constants' matrix is unused (identity).
		// VAT params: the shadow shader applies the SAME vertex animation as the GBuffer
		// so its depth matches the lit geometry. Without this the shadow map holds the
		// rest pose while the GBuffer holds the swayed pose -> misaligned self-shadowing
		// that flickers as the trees animate. VAT params ride MeshDrawConstants::
		// m_xVATParams (single scratch CB per set), matching the GBuffer path.
		Flux_AnimationTexture* pxAnimTex = pxGroup->GetAnimationTexture();
		const bool bHasVAT = (pxAnimTex != nullptr && pxAnimTex->HasGPUResources());

		u_int uMaterialIndex = pxMaterial->GetMaterialTableIndex();
		if (uMaterialIndex == uFLUX_INVALID_MATERIAL_INDEX) uMaterialIndex = 0u;
		MeshDrawConstants xMaterialConstants;
		BuildMeshDrawConstants(xMaterialConstants, glm::identity<glm::mat4>(), uMaterialIndex);
		xMaterialConstants.m_xVATParams = BuildInstancedVATParams(pxAnimTex);
		xBinder.BindDrawConstants(SM::hDrawConstants, &xMaterialConstants, sizeof(xMaterialConstants));
		xBinder.BindCBV(SM::hShadowMatrix, &xShadowMatrixBuffer.GetCBV());

		// Bind instance buffers. Transform/AnimData are frame-indexed (not graph-
		// tracked) -> UAV bind; the enabled-index list via its SRV (read-only
		// StructuredBuffer<uint> in the shader). The enabled-index list (not the
		// camera-culled persistent visible-index buffer) is what makes off-screen
		// casters still cast. The VAT position texture uses the POINT sampler (exact
		// per-texel reads), matching the GBuffer bind.
		xBinder.BindUAV_Buffer(SM::hTransformBuffer, &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(SM::hAnimDataBuffer, &pxGroup->GetAnimDataBuffer().GetUAV());
		xBinder.BindSRV_Buffer(SM::hVisibleIndexBuffer, pxGroup->GetEnabledIndexBuffer().GetSRV());
		if (bHasVAT)
		{
			xBinder.BindSRV(SM::hg_xAnimationTex, &pxAnimTex->GetPositionTexture()->m_xSRV,
				&xGraphics.m_xPointSampler);
		}
		else
		{
			xBinder.BindSRV(SM::hg_xAnimationTex, &xGraphics.m_xWhiteTexture.GetDirect()->m_xSRV);
		}

		// Draw all enabled casters (no camera culling for shadows).
		uint32_t uEnabledCount = pxGroup->GetEnabledCount();
		if (uEnabledCount > 0)
		{
			xCmdBuf.DrawIndexed(pxMesh->GetNumIndices(), uEnabledCount);
		}
	}
}

//=============================================================================
// Task System
//=============================================================================


//=============================================================================
// Accessors
//=============================================================================


// Flux material-binding unit tests (pure CPU). Hosted here so the test TU lives
// in the Flux module rather than pulling Flux into an AssetHandling test TU.
#include "Flux/Flux_MaterialBinding.Tests.inl"

