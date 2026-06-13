#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Flux/InstancedMeshes/Flux_InstanceCulling.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux_BackendTypes.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "ZenithECS/Zenith_Scene.h"
#include <vector>

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

//=============================================================================
// Push Constants for Instanced Meshes (192 bytes)
// Mirrors MaterialDrawConstants except the 5th field carries VAT animation
// params instead of emissive (same 16 bytes, instanced flavour — see
// Common/DrawConstants.slang). Layout must stay byte-compatible with
// DrawConstantsLayout: the instanced fragment shader reads the params2/flags
// fields through the shared accessors.
//=============================================================================
struct InstancedMeshPushConstants
{
	Zenith_Maths::Matrix4 m_xModelMatrix;       // 64 bytes (unused - per-instance in buffer)
	Zenith_Maths::Vector4 m_xBaseColor;         // 16 bytes
	Zenith_Maths::Vector4 m_xMaterialParams;    // 16 bytes (metallic, roughness, alphaCutoff, occlusionStrength)
	Zenith_Maths::Vector4 m_xUVParams;          // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xAnimTexParams;     // 16 bytes (textureWidth, textureHeight, enableVAT, unused)
	Zenith_Maths::Vector4 m_xMaterialParams2;   // 16 bytes (specular, normalStrength, 0, 0)
	Zenith_Maths::Vector4 m_xParallaxParams;    // 16 bytes (unused on the instanced path)
	Zenith_Maths::Vector4 m_xDetailParams;      // 16 bytes (unused on the instanced path)
	Zenith_Maths::Vector4 m_xFlagsParams;       // 16 bytes (MaterialDrawFlags as uint bits, unused x3)
};
static_assert(sizeof(InstancedMeshPushConstants) == sizeof(MaterialDrawConstants),
	"InstancedMeshPushConstants must stay byte-compatible with MaterialDrawConstants / DrawConstantsLayout");

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

static void ExecuteCulling(Flux_CommandList* pxCmdList, void* pUserData);
static void ExecuteInstancedGBuffer(Flux_CommandList* pxCmdList, void* pUserData);

void Flux_InstancedMeshesImpl::BuildPipelines()
{
	// Load shaders
	m_xGBufferShader.Initialise(FluxShaderProgram::InstancedMesh_ToGBuffer);
	m_xShadowShader.Initialise(FluxShaderProgram::InstancedMesh_ToShadowmap);

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

		m_xGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipeline, xPipelineSpec);
	}

	// Shadow pipeline
	{
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &m_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;
		xShadowPipelineSpec.m_bDepthBias = false;

		m_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(m_xShadowPipeline, xShadowPipelineSpec);
	}

	// Culling compute pipeline
	{
		m_xCullingShader.Initialise(FluxShaderProgram::InstanceCulling);

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
}

void Flux_InstancedMeshesImpl::Initialise()
{
	BuildPipelines();

	// One-time setup that hot-reload must NOT repeat (would leak VRAM).
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_CullingConstants), m_xCullingConstantsBuffer);
	m_bCullingInitialized = true;

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::InstancedMesh_ToGBuffer,
		FluxShaderProgram::InstancedMesh_ToShadowmap,
		FluxShaderProgram::InstanceCulling,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.InstancedMeshes().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes initialised (GPU culling enabled)");
}

void Flux_InstancedMeshesImpl::Shutdown()
{
	ClearAllGroups();
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_xCullingConstantsBuffer);
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
	// Pass 1: GPU culling compute (no declared resources — per-instance-group
	// output buffers are dynamic and not graph-tracked, so the GBuffer pass's
	// dependency is expressed as an explicit DependsOn below).
	//
	// WS7 keystone: the gather Prepare is hung on this FIRST instanced pass
	// (Particles-Compute pattern). CallPrepareCallbacks runs it on the main thread
	// before any record task dispatches, so it is the SINGLE writer of every
	// group's per-frame state (UpdateGPUBuffers + ResetVisibleCount + culling-
	// constants upload + stats). Both ExecuteCulling and ExecuteInstancedGBuffer
	// then only READ that frozen state on their worker threads. Pass-registration
	// order is irrelevant to Prepare timing.
	Flux_PassHandle xCullingPass = xGraph.AddPass("Instanced Meshes Culling", ExecuteCulling)
		.Prepare([](void* p){ g_xEngine.InstancedMeshes().GatherInstancedPacket(p); });

	// Pass 2: GBuffer render
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	xGraph.AddPass("Instanced Meshes GBuffer", ExecuteInstancedGBuffer)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),			RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT),	RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),						RESOURCE_ACCESS_WRITE_DSV)
		.DependsOn(xCullingPass);
}

// WS7 keystone gather (main thread, via .Prepare). This is the SINGLE writer of
// every instance group's per-frame CPU/GPU-sync state. It performs, once on the
// main thread, exactly the per-group work the record callbacks used to do
// concurrently:
//   - UpdateGPUBuffers(): uploads dirty transform/anim data and rebuilds the CPU
//     visible-index list + m_uVisibleCount. Needed by BOTH the GPU-culling path
//     (fresh transform/anim before the compute dispatch) and the CPU-fallback
//     path (the ExecuteInstancedGBuffer !bUseGPUCulling branch used to call this).
//   - When GPU culling is active: ResetVisibleCount() (zeroes the GPU atomic
//     counter + indirect command, and m_uVisibleCount) and uploads the per-group
//     culling constants into the shared subsystem buffer (same single-buffer
//     overwrite semantics as before — the dispatches bind+read it at GPU time).
//   - Accumulates the m_uTotalInstances / m_uVisibleInstances stats that
//     ExecuteInstancedGBuffer used to write in its draw loop.
// After this returns the group state is frozen; the record callbacks are readers.
void Flux_InstancedMeshesImpl::GatherInstancedPacket(void*)
{
	Flux_InstancedMeshesImpl& xSelf = *this;

	// Reset stats up-front (matches ExecuteInstancedGBuffer's old pre-loop reset).
	xSelf.m_uTotalInstances = 0;
	xSelf.m_uVisibleInstances = 0;

	if (xSelf.m_apxInstanceGroups.GetSize() == 0)
	{
		return;
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
		xViewProjMatrix = g_xEngine.FluxGraphics().GetViewProjMatrix();
		xCameraPos = g_xEngine.FluxGraphics().GetCameraPosition();
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

		// Upload CPU data to GPU + rebuild CPU visible list (sets m_uVisibleCount).
		pxGroup->UpdateGPUBuffers();

		if (bUseGPUCulling)
		{
			// Reset the GPU atomic counter + indirect command (also zeroes
			// m_uVisibleCount). Order matters: AFTER UpdateGPUBuffers, mirroring
			// the old ExecuteCulling sequence — the GPU compute writes the real
			// visible count on-device.
			pxGroup->ResetVisibleCount();

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

static void ExecuteCulling(Flux_CommandList* pxCmdList, void*)
{
	// PURE READER (worker thread). All CPU/GPU-sync mutation — UpdateGPUBuffers,
	// ResetVisibleCount, and the culling-constants upload — was relocated to
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

	pxCmdList->AddCommand<Flux_CommandBindComputePipeline>(&xZZ.m_xCullingPipeline);

	// Create binder for compute shader bindings
	Flux_ShaderBinder xBinder(*pxCmdList);

	for (u_int uGroup = 0; uGroup < xZZ.m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = xZZ.m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
		if (!pxGroup || pxGroup->IsEmpty())
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
		xBinder.BindCBV(xZZ.m_xCullingShader, "CullingConstants", &xZZ.m_xCullingConstantsBuffer.GetCBV());
		xBinder.BindUAV_Buffer(xZZ.m_xCullingShader, "TransformBuffer", &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(xZZ.m_xCullingShader, "AnimDataBuffer", &pxGroup->GetAnimDataBuffer().GetUAV());
		xBinder.BindUAV_Buffer(xZZ.m_xCullingShader, "VisibleIndexBuffer", &pxGroup->GetVisibleIndexBuffer().GetUAV());
		xBinder.BindUAV_Buffer(xZZ.m_xCullingShader, "visibleCount", &pxGroup->GetVisibleCountBuffer().GetUAV());
		xBinder.BindUAV_Buffer(xZZ.m_xCullingShader, "indirectInstanceCount", &pxGroup->GetIndirectBuffer().GetUAV());

		// Dispatch compute shader: 64 threads per workgroup
		uint32_t uNumWorkgroups = (pxGroup->GetInstanceCount() + 63) / 64;
		pxCmdList->AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);
	}
}

// Bind per-batch material, animation texture, push constants, and instance
// buffers for the GBuffer pass. Caller must have bound the shared pipeline
// and FrameConstants before invoking this.
void Flux_InstancedMeshesImpl::BindBatchDescriptors(Flux_ShaderBinder& xBinder, Flux_InstanceGroup* pxGroup)
{
	Zenith_MaterialAsset* pxMaterial = pxGroup->GetMaterial();
	if (!pxMaterial)
	{
		pxMaterial = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();
	}

	Flux_AnimationTexture* pxAnimTex = pxGroup->GetAnimationTexture();
	const bool bHasVAT = (pxAnimTex != nullptr && pxAnimTex->HasGPUResources());

	// Build and push material constants (instance-aware resolved view).
	const Zenith_MaterialResolved& xResolved = pxMaterial->GetResolved();
	const Zenith_MaterialParams& xParams = xResolved.m_xParams;

	InstancedMeshPushConstants xPushConstants;
	xPushConstants.m_xModelMatrix = glm::identity<glm::mat4>();  // Per-instance in buffer
	xPushConstants.m_xBaseColor = xParams.m_xBaseColor;
	// Opaque never alpha-tests (cutoff 0 disables the shader's discard);
	// Masked uses the authored cutoff — same rule as BuildMaterialDrawConstants.
	const float fAlphaCutoff = (xParams.m_eBlendMode == MATERIAL_BLEND_MASKED) ? xParams.m_fAlphaCutoff : 0.0f;
	xPushConstants.m_xMaterialParams = Zenith_Maths::Vector4(
		xParams.m_fMetallic,
		xParams.m_fRoughness,
		fAlphaCutoff,
		xParams.m_fOcclusionStrength
	);
	xPushConstants.m_xUVParams = Zenith_Maths::Vector4(
		xParams.m_xUVTiling.x, xParams.m_xUVTiling.y,
		xParams.m_xUVOffset.x, xParams.m_xUVOffset.y);

	if (bHasVAT)
	{
		xPushConstants.m_xAnimTexParams = Zenith_Maths::Vector4(
			static_cast<float>(pxAnimTex->GetTextureWidth()),
			static_cast<float>(pxAnimTex->GetTextureHeight()),
			1.0f,  // enableVAT = true
			0.0f   // unused
		);
	}
	else
	{
		xPushConstants.m_xAnimTexParams = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);  // VAT disabled
	}

	xPushConstants.m_xMaterialParams2 = Zenith_Maths::Vector4(xParams.m_fSpecular, xParams.m_fNormalStrength, 0.0f, 0.0f);
	xPushConstants.m_xParallaxParams = Zenith_Maths::Vector4(0.0f, 8.0f, 32.0f, 0.0f);
	xPushConstants.m_xDetailParams = Zenith_Maths::Vector4(4.0f, 4.0f, 1.0f, 1.0f);

	// Instanced path supports unlit + two-sided normal flip only (POM/detail/
	// clear coat are mesh-path features; emissive needs the VAT-aliased field).
	u_int uFlags = 0;
	if (xParams.m_eShadingModel == MATERIAL_SHADING_UNLIT) uFlags |= MATERIAL_DRAW_FLAG_UNLIT;
	if (xParams.m_bTwoSided) uFlags |= MATERIAL_DRAW_FLAG_TWO_SIDED_NORMAL_FLIP;
	Zenith_Maths::Vector4 xFlags(0.0f);
	memcpy(&xFlags.x, &uFlags, sizeof(u_int));
	xPushConstants.m_xFlagsParams = xFlags;

	xBinder.BindDrawConstants(m_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

	// Bind material textures (instance-aware; emissive slot intentionally
	// not bound — the instanced shader doesn't declare it).
	xBinder.BindSRV(m_xGBufferShader, "g_xBaseColorTex", &pxMaterial->GetResolvedTexture(MATERIAL_TEXTURE_BASE_COLOR)->m_xSRV);
	xBinder.BindSRV(m_xGBufferShader, "g_xNormalTex", &pxMaterial->GetResolvedTexture(MATERIAL_TEXTURE_NORMAL)->m_xSRV);
	xBinder.BindSRV(m_xGBufferShader, "g_xRoughnessMetallicTex", &pxMaterial->GetResolvedTexture(MATERIAL_TEXTURE_ROUGHNESS_METALLIC)->m_xSRV);
	xBinder.BindSRV(m_xGBufferShader, "g_xOcclusionTex", &pxMaterial->GetResolvedTexture(MATERIAL_TEXTURE_OCCLUSION)->m_xSRV);

	// Bind animation texture (VAT) if available, else bind blank texture
	if (bHasVAT)
	{
		xBinder.BindSRV(m_xGBufferShader, "g_xAnimationTex", &pxAnimTex->GetPositionTexture()->m_xSRV);
	}
	else
	{
		xBinder.BindSRV(m_xGBufferShader, "g_xAnimationTex", &g_xEngine.FluxGraphics().m_xWhiteTexture.GetDirect()->m_xSRV);
	}

	// Bind instance buffers
	xBinder.BindUAV_Buffer(m_xGBufferShader, "TransformBuffer", &pxGroup->GetTransformBuffer().GetUAV());
	xBinder.BindUAV_Buffer(m_xGBufferShader, "AnimDataBuffer", &pxGroup->GetAnimDataBuffer().GetUAV());
	xBinder.BindUAV_Buffer(m_xGBufferShader, "VisibleIndexBuffer", &pxGroup->GetVisibleIndexBuffer().GetUAV());
}

// Emit the draw call(s) for one instance group. GPU-culling path uses an
// indirect draw whose count was written by ExecuteCulling; CPU fallback
// uses a direct instanced draw sized by the CPU-built visible list.
static void IssueBatchDraw(Flux_CommandList* pxCmdList, Flux_InstanceGroup* pxGroup, Flux_MeshInstance* pxMesh, bool bUseGPUCulling)
{
	if (bUseGPUCulling)
	{
		// GPU culling: use indirect draw with count from visible count buffer
		// The culling compute shader wrote the visible instance count to the indirect buffer
		pxCmdList->AddCommand<Flux_CommandDrawIndexedIndirect>(
			&pxGroup->GetIndirectBuffer(),
			1,   // drawCount = 1 (single draw call)
			0,   // offset = 0
			20   // stride = sizeof(VkDrawIndexedIndirectCommand) = 20 bytes
		);
		return;
	}

	// CPU culling fallback: direct instanced draw
	uint32_t uVisibleCount = pxGroup->GetVisibleCount();
	if (uVisibleCount > 0)
	{
		pxCmdList->AddCommand<Flux_CommandDrawIndexed>(pxMesh->GetNumIndices(), uVisibleCount);
	}
}

static void ExecuteInstancedGBuffer(Flux_CommandList* pxCmdList, void*)
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

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&xZZ.m_xGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(xZZ.m_xGBufferShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());

	const bool bUseGPUCulling = xZZ.m_bCullingEnabled && Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled && xZZ.m_bCullingInitialized;

	for (u_int uGroup = 0; uGroup < xZZ.m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = xZZ.m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
		if (!pxGroup || pxGroup->IsEmpty())
		{
			continue;
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// Set vertex and index buffers from mesh
		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxMesh->GetVertexBuffer());
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxMesh->GetIndexBuffer());

		xZZ.BindBatchDescriptors(xBinder, pxGroup);
		IssueBatchDraw(pxCmdList, pxGroup, pxMesh, bUseGPUCulling);
	}
}

void Flux_InstancedMeshesImpl::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// C2 audit: PURE READER. Called from the shadow cascades' record path; it only
	// reads frozen group state (GetMesh / GetTransformBuffer / GetVisibleIndexBuffer
	// / GetVisibleCount) — no UpdateGPUBuffers, no ResetVisibleCount. The mutators it
	// would otherwise have called assert main-thread-only (AssertMainThreadMutation),
	// so any future worker-thread mutation here trips immediately.
	if (!Zenith_GraphicsOptions::Get().m_bInstancedMeshesEnabled)
	{
		return;
	}

	if (m_apxInstanceGroups.GetSize() == 0)
	{
		return;
	}

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Shadow pass: DrawConstants + ShadowMatrix + transform/visible-index
	// SSBOs only — Slang reflection won't show FrameConstants.

	for (u_int uGroup = 0; uGroup < m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = m_apxInstanceGroups.Get(static_cast<u_int>(uGroup));
		if (!pxGroup || pxGroup->IsEmpty())
		{
			continue;
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// Set vertex and index buffers
		xCmdBuf.AddCommand<Flux_CommandSetVertexBuffer>(&pxMesh->GetVertexBuffer());
		xCmdBuf.AddCommand<Flux_CommandSetIndexBuffer>(&pxMesh->GetIndexBuffer());

		// Bind shadow matrix
		Zenith_Maths::Matrix4 xIdentity = glm::identity<glm::mat4>();
		xBinder.BindDrawConstants(m_xShadowShader, "DrawConstants", &xIdentity, sizeof(xIdentity));
		xBinder.BindCBV(m_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());

		// Bind instance buffers
		xBinder.BindUAV_Buffer(m_xShadowShader, "TransformBuffer", &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(m_xShadowShader, "VisibleIndexBuffer", &pxGroup->GetVisibleIndexBuffer().GetUAV());

		// Draw all visible instances
		uint32_t uVisibleCount = pxGroup->GetVisibleCount();
		if (uVisibleCount > 0)
		{
			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMesh->GetNumIndices(), uVisibleCount);
		}
	}
}

//=============================================================================
// Task System
//=============================================================================


//=============================================================================
// Accessors
//=============================================================================



