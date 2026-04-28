#include "Zenith.h"

#include "Flux/InstancedMeshes/Flux_InstancedMeshes.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Flux/InstancedMeshes/Flux_InstanceCulling.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include <vector>

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

//=============================================================================
// Push Constants for Instanced Meshes (128 bytes)
// Different from MaterialDrawConstants - has animation params instead of emissive
//=============================================================================
struct InstancedMeshPushConstants
{
	Zenith_Maths::Matrix4 m_xModelMatrix;       // 64 bytes (unused - per-instance in buffer)
	Zenith_Maths::Vector4 m_xBaseColor;         // 16 bytes
	Zenith_Maths::Vector4 m_xMaterialParams;    // 16 bytes (metallic, roughness, alphaCutoff, occlusionStrength)
	Zenith_Maths::Vector4 m_xUVParams;          // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xAnimTexParams;     // 16 bytes (textureWidth, textureHeight, enableVAT, unused)
};
static_assert(sizeof(InstancedMeshPushConstants) == 128, "InstancedMeshPushConstants must be 128 bytes");

//=============================================================================
// Static Data
//=============================================================================


// Registered instance groups
static std::vector<Flux_InstanceGroup*> s_apxInstanceGroups;

// GBuffer rendering pipeline
static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

// Shadow map rendering pipeline
static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

// Culling compute pipeline
static Flux_Shader s_xCullingShader;
static Flux_Pipeline s_xCullingPipeline;
static Flux_RootSig s_xCullingRootSig;
static Flux_DynamicConstantBuffer s_xCullingConstantsBuffer;
static bool s_bCullingInitialized = false;
static bool s_bCullingEnabled = true;  // GPU culling enabled by default

// Statistics
static uint32_t s_uTotalInstances = 0;
static uint32_t s_uVisibleInstances = 0;


//=============================================================================
// Initialise / Shutdown
//=============================================================================

void Flux_InstancedMeshes::BuildPipelines()
{
	// Load shaders
	s_xGBufferShader.Initialise(FluxShaderProgram::InstancedMesh_ToGBuffer);
	s_xShadowShader.Initialise(FluxShaderProgram::InstancedMesh_ToShadowmap);

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
		xPipelineSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &s_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		s_xGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(s_xGBufferPipeline, xPipelineSpec);
	}

	// Shadow pipeline
	{
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &s_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;
		xShadowPipelineSpec.m_bDepthBias = false;

		s_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

	// Culling compute pipeline
	{
		s_xCullingShader.Initialise(FluxShaderProgram::InstanceCulling);

		// Build compute root signature from shader reflection
		const Flux_ShaderReflection& xCullingReflection = s_xCullingShader.GetReflection();
		Flux_RootSigBuilder::FromReflection(s_xCullingRootSig, xCullingReflection);

		// Build compute pipeline
		Flux_ComputePipelineBuilder xComputeBuilder;
		xComputeBuilder.WithShader(s_xCullingShader)
			.WithLayout(s_xCullingRootSig.m_xLayout)
			.Build(s_xCullingPipeline);

		s_xCullingPipeline.m_xRootSig = s_xCullingRootSig;
	}
}

void Flux_InstancedMeshes::Initialise()
{
	BuildPipelines();

	// One-time setup that hot-reload must NOT repeat (would leak VRAM).
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_CullingConstants), s_xCullingConstantsBuffer);
	s_bCullingInitialized = true;

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::InstancedMesh_ToGBuffer,
		FluxShaderProgram::InstancedMesh_ToShadowmap,
		FluxShaderProgram::InstanceCulling,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_InstancedMeshes::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes initialised (GPU culling enabled)");
}

void Flux_InstancedMeshes::Shutdown()
{
	ClearAllGroups();
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xCullingConstantsBuffer);
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes shutdown");
}

void Flux_InstancedMeshes::Reset()
{
	// Reset is handled by the render graph
	// Update statistics
	s_uTotalInstances = 0;
	s_uVisibleInstances = 0;

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes::Reset()");
}

//=============================================================================
// Instance Group Registration
//=============================================================================

void Flux_InstancedMeshes::RegisterInstanceGroup(Flux_InstanceGroup* pxGroup)
{
	if (!pxGroup)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Flux_InstancedMeshes::RegisterInstanceGroup - null group");
		return;
	}

	// Check if already registered
	for (size_t i = 0; i < s_apxInstanceGroups.size(); ++i)
	{
		if (s_apxInstanceGroups[i] == pxGroup)
		{
			return;  // Already registered
		}
	}

	s_apxInstanceGroups.push_back(pxGroup);
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Registered instance group (total: %zu)", s_apxInstanceGroups.size());
}

void Flux_InstancedMeshes::UnregisterInstanceGroup(Flux_InstanceGroup* pxGroup)
{
	for (size_t i = 0; i < s_apxInstanceGroups.size(); ++i)
	{
		if (s_apxInstanceGroups[i] == pxGroup)
		{
			// Swap with last and pop
			s_apxInstanceGroups[i] = s_apxInstanceGroups.back();
			s_apxInstanceGroups.pop_back();
			Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Unregistered instance group (remaining: %zu)", s_apxInstanceGroups.size());
			return;
		}
	}
}

void Flux_InstancedMeshes::ClearAllGroups()
{
	s_apxInstanceGroups.clear();
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Cleared all instance groups");
}

//=============================================================================
// Per-Frame Rendering
//=============================================================================

void Flux_InstancedMeshes::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Pass 1: GPU culling compute (no declared resources — per-instance-group
	// output buffers are dynamic and not graph-tracked, so the GBuffer pass's
	// dependency is expressed as an explicit DependsOn below).
	Flux_PassHandle xCullingPass = xGraph.AddPass("Instanced Meshes Culling", ExecuteCulling);

	// Pass 2: GBuffer render
	xGraph.AddPass("Instanced Meshes GBuffer", ExecuteGBuffer)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),			RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT),	RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),		RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetDepthAttachment(),						RESOURCE_ACCESS_WRITE_DSV)
		.DependsOn(xCullingPass);
}

void Flux_InstancedMeshes::ExecuteCulling(Flux_CommandList* pxCmdList, void*)
{
	// Check if GPU culling should run
	if (!s_bCullingInitialized || !s_bCullingEnabled || !Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled)
	{
		return;
	}

	if (s_apxInstanceGroups.empty())
	{
		return;
	}

	// Get camera matrices from Flux_Graphics (already computed this frame)
	Zenith_Maths::Matrix4 xViewProjMatrix = Flux_Graphics::GetViewProjMatrix();
	Zenith_Maths::Vector3 xCameraPos = Flux_Graphics::GetCameraPosition();

	pxCmdList->AddCommand<Flux_CommandBindComputePipeline>(&s_xCullingPipeline);

	// Create binder for compute shader bindings
	Flux_ShaderBinder xBinder(*pxCmdList);

	for (size_t uGroup = 0; uGroup < s_apxInstanceGroups.size(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = s_apxInstanceGroups[uGroup];
		if (!pxGroup || pxGroup->IsEmpty())
		{
			continue;
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// Upload CPU data to GPU before culling (ensures transforms and anim data are current)
		pxGroup->UpdateGPUBuffers();

		// Reset visible count to 0 before culling
		pxGroup->ResetVisibleCount();

		// Build culling constants
		Flux_CullingConstants xCullingConstants;
		Flux_InstanceCullingUtil::ExtractFrustumPlanes(xViewProjMatrix, xCullingConstants.m_axFrustumPlanes);
		xCullingConstants.m_xCameraPosition = Zenith_Maths::Vector4(xCameraPos, 0.0f);
		xCullingConstants.m_uTotalInstanceCount = pxGroup->GetInstanceCount();
		xCullingConstants.m_uMeshIndexCount = pxMesh->GetNumIndices();
		xCullingConstants.m_fBoundingSphereRadius = pxGroup->GetBounds().m_fRadius;
		xCullingConstants.m_fPadding = 0.0f;

		// Upload culling constants
		Flux_MemoryManager::UploadBufferData(
			s_xCullingConstantsBuffer.GetBuffer().m_xVRAMHandle,
			&xCullingConstants,
			sizeof(xCullingConstants));

		// Bind resources
		xBinder.BindCBV(s_xCullingShader, "CullingConstants", &s_xCullingConstantsBuffer.GetCBV());
		xBinder.BindUAV_Buffer(s_xCullingShader, "TransformBuffer", &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingShader, "AnimDataBuffer", &pxGroup->GetAnimDataBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingShader, "VisibleIndexBuffer", &pxGroup->GetVisibleIndexBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingShader, "visibleCount", &pxGroup->GetVisibleCountBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingShader, "indirectInstanceCount", &pxGroup->GetIndirectBuffer().GetUAV());

		// Dispatch compute shader: 64 threads per workgroup
		uint32_t uNumWorkgroups = (pxGroup->GetInstanceCount() + 63) / 64;
		pxCmdList->AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);
	}
}

// Bind per-batch material, animation texture, push constants, and instance
// buffers for the GBuffer pass. Caller must have bound the shared pipeline
// and FrameConstants before invoking this.
static void BindBatchDescriptors(Flux_ShaderBinder& xBinder, Flux_InstanceGroup* pxGroup)
{
	Zenith_MaterialAsset* pxMaterial = pxGroup->GetMaterial();
	if (!pxMaterial)
	{
		pxMaterial = Flux_Graphics::s_pxBlankMaterial;
	}

	Flux_AnimationTexture* pxAnimTex = pxGroup->GetAnimationTexture();
	const bool bHasVAT = (pxAnimTex != nullptr && pxAnimTex->HasGPUResources());

	// Build and push material constants
	InstancedMeshPushConstants xPushConstants;
	xPushConstants.m_xModelMatrix = glm::identity<glm::mat4>();  // Per-instance in buffer
	xPushConstants.m_xBaseColor = pxMaterial->GetBaseColor();
	xPushConstants.m_xMaterialParams = Zenith_Maths::Vector4(
		pxMaterial->GetMetallic(),
		pxMaterial->GetRoughness(),
		pxMaterial->GetAlphaCutoff(),
		pxMaterial->GetOcclusionStrength()
	);
	const Zenith_Maths::Vector2& xTiling = pxMaterial->GetUVTiling();
	const Zenith_Maths::Vector2& xOffset = pxMaterial->GetUVOffset();
	xPushConstants.m_xUVParams = Zenith_Maths::Vector4(xTiling.x, xTiling.y, xOffset.x, xOffset.y);

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

	xBinder.BindDrawConstants(s_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

	// Bind material textures
	xBinder.BindSRV(s_xGBufferShader, "g_xDiffuseTex", &pxMaterial->GetDiffuseTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xNormalTex", &pxMaterial->GetNormalTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xRoughnessMetallicTex", &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xOcclusionTex", &pxMaterial->GetOcclusionTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xEmissiveTex", &pxMaterial->GetEmissiveTexture()->m_xSRV);

	// Bind animation texture (VAT) if available, else bind blank texture
	if (bHasVAT)
	{
		xBinder.BindSRV(s_xGBufferShader, "g_xAnimationTex", &pxAnimTex->GetPositionTexture()->m_xSRV);
	}
	else
	{
		xBinder.BindSRV(s_xGBufferShader, "g_xAnimationTex", &Flux_Graphics::s_pxWhiteTexture->m_xSRV);
	}

	// Bind instance buffers
	xBinder.BindUAV_Buffer(s_xGBufferShader, "TransformBuffer", &pxGroup->GetTransformBuffer().GetUAV());
	xBinder.BindUAV_Buffer(s_xGBufferShader, "AnimDataBuffer", &pxGroup->GetAnimDataBuffer().GetUAV());
	xBinder.BindUAV_Buffer(s_xGBufferShader, "VisibleIndexBuffer", &pxGroup->GetVisibleIndexBuffer().GetUAV());
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

void Flux_InstancedMeshes::ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bInstancedMeshesEnabled)
	{
		return;
	}

	if (s_apxInstanceGroups.empty())
	{
		return;
	}

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&s_xGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xGBufferShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	// Track statistics
	s_uTotalInstances = 0;
	s_uVisibleInstances = 0;

	const bool bUseGPUCulling = s_bCullingEnabled && Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled && s_bCullingInitialized;

	for (size_t uGroup = 0; uGroup < s_apxInstanceGroups.size(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = s_apxInstanceGroups[uGroup];
		if (!pxGroup || pxGroup->IsEmpty())
		{
			continue;
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		if (!pxMesh)
		{
			continue;
		}

		// When GPU culling is enabled, buffers are updated by the culling pass (ExecuteCulling)
		// When disabled (CPU fallback), update buffers here including CPU-side visible list
		if (!bUseGPUCulling)
		{
			// CPU fallback: UpdateGPUBuffers builds visible index list on CPU
			pxGroup->UpdateGPUBuffers();
		}

		// Set vertex and index buffers from mesh
		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxMesh->GetVertexBuffer());
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxMesh->GetIndexBuffer());

		BindBatchDescriptors(xBinder, pxGroup);
		IssueBatchDraw(pxCmdList, pxGroup, pxMesh, bUseGPUCulling);

		s_uTotalInstances += pxGroup->GetInstanceCount();
		// Note: visible count is not accurate for GPU culling path (would need GPU readback)
		s_uVisibleInstances += bUseGPUCulling ? pxGroup->GetInstanceCount() : pxGroup->GetVisibleCount();
	}
}

void Flux_InstancedMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	if (!Zenith_GraphicsOptions::Get().m_bInstancedMeshesEnabled)
	{
		return;
	}

	if (s_apxInstanceGroups.empty())
	{
		return;
	}

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Shadow pass: DrawConstants + ShadowMatrix + transform/visible-index
	// SSBOs only — Slang reflection won't show FrameConstants.

	for (size_t uGroup = 0; uGroup < s_apxInstanceGroups.size(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = s_apxInstanceGroups[uGroup];
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
		xBinder.BindDrawConstants(s_xShadowShader, "DrawConstants", &xIdentity, sizeof(xIdentity));
		xBinder.BindCBV(s_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());

		// Bind instance buffers
		xBinder.BindUAV_Buffer(s_xShadowShader, "TransformBuffer", &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xShadowShader, "VisibleIndexBuffer", &pxGroup->GetVisibleIndexBuffer().GetUAV());

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

uint32_t Flux_InstancedMeshes::GetTotalInstanceCount()
{
	return s_uTotalInstances;
}

uint32_t Flux_InstancedMeshes::GetVisibleInstanceCount()
{
	return s_uVisibleInstances;
}

uint32_t Flux_InstancedMeshes::GetGroupCount()
{
	return static_cast<uint32_t>(s_apxInstanceGroups.size());
}
