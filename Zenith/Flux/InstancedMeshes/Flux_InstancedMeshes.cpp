#include "Zenith.h"

#include "Flux/InstancedMeshes/Flux_InstancedMeshes.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Flux/InstancedMeshes/Flux_InstanceCulling.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include <vector>

//=============================================================================
// Push Constants for Instanced Meshes (128 bytes)
// Different from MaterialPushConstants - has animation params instead of emissive
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

static Zenith_Task g_xCullingTask(ZENITH_PROFILE_INDEX__FLUX_COMPUTE, Flux_InstancedMeshes::DispatchCulling, nullptr);
static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_INSTANCED_MESHES, Flux_InstancedMeshes::RenderToGBuffer, nullptr);

static Flux_CommandList g_xCullingCommandList("Instanced Meshes Culling");
static Flux_CommandList g_xGBufferCommandList("Instanced Meshes GBuffer");

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
static Zenith_Vulkan_RootSig s_xCullingRootSig;
static Flux_DynamicConstantBuffer s_xCullingConstantsBuffer;
static bool s_bCullingInitialized = false;
static bool s_bCullingEnabled = true;  // GPU culling enabled by default

// GBuffer shader binding handles
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xScratchBufferBinding;
static Flux_BindingHandle s_xDiffuseTexBinding;
static Flux_BindingHandle s_xNormalTexBinding;
static Flux_BindingHandle s_xRoughnessMetallicTexBinding;
static Flux_BindingHandle s_xOcclusionTexBinding;
static Flux_BindingHandle s_xEmissiveTexBinding;
static Flux_BindingHandle s_xTransformBufferBinding;
static Flux_BindingHandle s_xAnimDataBufferBinding;
static Flux_BindingHandle s_xVisibleIndexBufferBinding;
static Flux_BindingHandle s_xAnimationTexBinding;

// Shadow shader binding handles
static Flux_BindingHandle s_xShadowFrameConstantsBinding;
static Flux_BindingHandle s_xShadowScratchBufferBinding;
static Flux_BindingHandle s_xShadowMatrixBinding;
static Flux_BindingHandle s_xShadowTransformBufferBinding;
static Flux_BindingHandle s_xShadowVisibleIndexBufferBinding;

// Culling shader binding handles
static Flux_BindingHandle s_xCullingConstantsBinding;
static Flux_BindingHandle s_xCullingTransformBufferBinding;
static Flux_BindingHandle s_xCullingAnimDataBufferBinding;
static Flux_BindingHandle s_xCullingVisibleIndexBufferBinding;
static Flux_BindingHandle s_xCullingVisibleCountBufferBinding;
static Flux_BindingHandle s_xCullingIndirectBufferBinding;

// Statistics
static uint32_t s_uTotalInstances = 0;
static uint32_t s_uVisibleInstances = 0;

DEBUGVAR bool dbg_bEnableInstancedMeshes = true;
DEBUGVAR bool dbg_bEnableGPUCulling = true;  // GPU culling enabled

//=============================================================================
// Initialise / Shutdown
//=============================================================================

void Flux_InstancedMeshes::Initialise()
{
	// Load shaders
	s_xGBufferShader.Initialise("InstancedMeshes/Flux_InstancedMeshes_ToGBuffer.vert", "InstancedMeshes/Flux_InstancedMeshes_ToGBuffer.frag");
	s_xShadowShader.Initialise("InstancedMeshes/Flux_InstancedMeshes_ToShadowMap.vert", "InstancedMeshes/Flux_InstancedMeshes_ToShadowMap.frag");

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
		xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;
		xPipelineSpec.m_pxShader = &s_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;

		// Set 0: Per-frame (FrameConstants only - bound once per command list)
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;

		// Set 1: Per-draw (scratch buffer + textures + instance buffers + animation texture)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;   // Scratch buffer for push constants
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Diffuse
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Normal
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // RoughnessMetallic
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Occlusion
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Emissive
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Transform buffer
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[7].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // AnimData buffer
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[8].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // VisibleIndex buffer
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[9].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Animation texture (VAT)

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
		xShadowPipelineSpec.m_pxTargetSetup = &Flux_Shadows::GetCSMTargetSetup(0);
		xShadowPipelineSpec.m_pxShader = &s_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;
		xShadowPipelineSpec.m_bDepthBias = false;

		Flux_PipelineLayout& xLayout = xShadowPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;

		// Set 0: Per-frame (FrameConstants only)
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;

		// Set 1: Per-draw - must match shader bindings in Flux_InstancedMeshes_VertCommon.fxh
		// The shader uses binding 0 for PushConstants, binding 1 for ShadowMatrix,
		// and bindings 6, 7, 8 for instance buffers
		// Note: Must declare ALL bindings 0-8 since pipeline builder stops at first gap
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;          // Scratch buffer (binding 0)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;          // Shadow matrix (binding 1)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;         // Placeholder (binding 2) - not used by shadow shader
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;         // Placeholder (binding 3) - not used by shadow shader
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;         // Placeholder (binding 4) - not used by shadow shader
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_TEXTURE;         // Placeholder (binding 5) - not used by shadow shader
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Transform buffer (binding 6)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[7].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // AnimData buffer (binding 7)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[8].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // VisibleIndex buffer (binding 8)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[9].m_eType = DESCRIPTOR_TYPE_TEXTURE;         // Animation texture (binding 9)

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xGBufferReflection = s_xGBufferShader.GetReflection();
	s_xFrameConstantsBinding = xGBufferReflection.GetBinding("FrameConstants");
	s_xScratchBufferBinding = xGBufferReflection.GetBinding("PushConstants");
	s_xDiffuseTexBinding = xGBufferReflection.GetBinding("g_xDiffuseTex");
	s_xNormalTexBinding = xGBufferReflection.GetBinding("g_xNormalTex");
	s_xRoughnessMetallicTexBinding = xGBufferReflection.GetBinding("g_xRoughnessMetallicTex");
	s_xOcclusionTexBinding = xGBufferReflection.GetBinding("g_xOcclusionTex");
	s_xEmissiveTexBinding = xGBufferReflection.GetBinding("g_xEmissiveTex");
	s_xTransformBufferBinding = xGBufferReflection.GetBinding("TransformBuffer");
	s_xAnimDataBufferBinding = xGBufferReflection.GetBinding("AnimDataBuffer");
	s_xVisibleIndexBufferBinding = xGBufferReflection.GetBinding("VisibleIndexBuffer");
	s_xAnimationTexBinding = xGBufferReflection.GetBinding("g_xAnimationTex");

	// Shadow shader bindings
	const Flux_ShaderReflection& xShadowReflection = s_xShadowShader.GetReflection();
	s_xShadowFrameConstantsBinding = xShadowReflection.GetBinding("FrameConstants");
	s_xShadowScratchBufferBinding = xShadowReflection.GetBinding("PushConstants");
	s_xShadowMatrixBinding = xShadowReflection.GetBinding("ShadowMatrix");
	s_xShadowTransformBufferBinding = xShadowReflection.GetBinding("TransformBuffer");
	s_xShadowVisibleIndexBufferBinding = xShadowReflection.GetBinding("VisibleIndexBuffer");

	// Culling compute pipeline
	{
		s_xCullingShader.InitialiseCompute("InstancedMeshes/Flux_InstanceCulling.comp");

		// Build compute root signature
		Flux_PipelineLayout xComputeLayout;
		xComputeLayout.m_uNumDescriptorSets = 1;
		xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;          // CullingConstants
		xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // TransformBuffer
		xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // AnimDataBuffer
		xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // VisibleIndexBuffer
		xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // VisibleCount
		xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // IndirectCommand

		Zenith_Vulkan_RootSigBuilder::FromSpecification(s_xCullingRootSig, xComputeLayout);

		// Build compute pipeline
		Zenith_Vulkan_ComputePipelineBuilder xComputeBuilder;
		xComputeBuilder.WithShader(s_xCullingShader)
			.WithLayout(s_xCullingRootSig.m_xLayout)
			.Build(s_xCullingPipeline);

		s_xCullingPipeline.m_xRootSig = s_xCullingRootSig;

		// Cache culling shader binding handles
		const Flux_ShaderReflection& xCullingReflection = s_xCullingShader.GetReflection();
		s_xCullingConstantsBinding = xCullingReflection.GetBinding("CullingConstants");
		s_xCullingTransformBufferBinding = xCullingReflection.GetBinding("TransformBuffer");
		s_xCullingAnimDataBufferBinding = xCullingReflection.GetBinding("AnimDataBuffer");
		s_xCullingVisibleIndexBufferBinding = xCullingReflection.GetBinding("VisibleIndexBuffer");
		s_xCullingVisibleCountBufferBinding = xCullingReflection.GetBinding("visibleCount");
		s_xCullingIndirectBufferBinding = xCullingReflection.GetBinding("indirectInstanceCount");

		// Initialize culling constants buffer
		Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_CullingConstants), s_xCullingConstantsBuffer);

		s_bCullingInitialized = true;
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Instanced Meshes" }, dbg_bEnableInstancedMeshes);
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Instanced GPU Culling" }, dbg_bEnableGPUCulling);
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes initialised (GPU culling enabled)");
}

void Flux_InstancedMeshes::Shutdown()
{
	ClearAllGroups();
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes shutdown");
}

void Flux_InstancedMeshes::Reset()
{
	// Reset command lists to clear stale GPU resource references
	g_xCullingCommandList.Reset(true);
	g_xGBufferCommandList.Reset(true);

	// Update statistics
	s_uTotalInstances = 0;
	s_uVisibleInstances = 0;

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes::Reset() - Reset command lists");
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

void Flux_InstancedMeshes::DispatchCulling(void*)
{
	// Check if GPU culling should run
	if (!s_bCullingInitialized || !s_bCullingEnabled || !dbg_bEnableGPUCulling)
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

	g_xCullingCommandList.Reset(true);
	g_xCullingCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xCullingPipeline);

	// Create binder for compute shader bindings
	Flux_ShaderBinder xBinder(g_xCullingCommandList);

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
		xBinder.BindCBV(s_xCullingConstantsBinding, &s_xCullingConstantsBuffer.GetCBV());
		xBinder.BindUAV_Buffer(s_xCullingTransformBufferBinding, &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingAnimDataBufferBinding, &pxGroup->GetAnimDataBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingVisibleIndexBufferBinding, &pxGroup->GetVisibleIndexBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingVisibleCountBufferBinding, &pxGroup->GetVisibleCountBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xCullingIndirectBufferBinding, &pxGroup->GetIndirectBuffer().GetUAV());

		// Dispatch compute shader: 64 threads per workgroup
		uint32_t uNumWorkgroups = (pxGroup->GetInstanceCount() + 63) / 64;
		g_xCullingCommandList.AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);
	}

	// Submit culling command list (compute pass - no render targets)
	Flux::SubmitCommandList(&g_xCullingCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_INSTANCE_CULLING);
}

void Flux_InstancedMeshes::RenderToGBuffer(void*)
{
	if (!dbg_bEnableInstancedMeshes)
	{
		return;
	}

	if (s_apxInstanceGroups.empty())
	{
		return;
	}

	g_xGBufferCommandList.Reset(false);
	g_xGBufferCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(g_xGBufferCommandList);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	// Track statistics
	s_uTotalInstances = 0;
	s_uVisibleInstances = 0;

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

		// When GPU culling is enabled, buffers are updated by the culling pass (DispatchCulling)
		// When disabled (CPU fallback), update buffers here including CPU-side visible list
		bool bUseGPUCulling = s_bCullingEnabled && dbg_bEnableGPUCulling && s_bCullingInitialized;
		if (!bUseGPUCulling)
		{
			// CPU fallback: UpdateGPUBuffers builds visible index list on CPU
			pxGroup->UpdateGPUBuffers();
		}

		// Set vertex and index buffers from mesh
		g_xGBufferCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&pxMesh->GetVertexBuffer());
		g_xGBufferCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&pxMesh->GetIndexBuffer());

		// Get material (fall back to blank if none)
		Flux_MaterialAsset* pxMaterial = pxGroup->GetMaterial();
		if (!pxMaterial)
		{
			pxMaterial = Flux_Graphics::s_pxBlankMaterial;
		}

		// Get animation texture (optional)
		Flux_AnimationTexture* pxAnimTex = pxGroup->GetAnimationTexture();
		bool bHasVAT = (pxAnimTex != nullptr && pxAnimTex->HasGPUResources());

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

		// Animation texture parameters
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

		xBinder.PushConstant(s_xScratchBufferBinding, &xPushConstants, sizeof(xPushConstants));

		// Bind material textures
		xBinder.BindSRV(s_xDiffuseTexBinding, &pxMaterial->GetDiffuseTexture()->m_xSRV);
		xBinder.BindSRV(s_xNormalTexBinding, &pxMaterial->GetNormalTexture()->m_xSRV);
		xBinder.BindSRV(s_xRoughnessMetallicTexBinding, &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
		xBinder.BindSRV(s_xOcclusionTexBinding, &pxMaterial->GetOcclusionTexture()->m_xSRV);
		xBinder.BindSRV(s_xEmissiveTexBinding, &pxMaterial->GetEmissiveTexture()->m_xSRV);

		// Bind animation texture (VAT) if available, else bind blank texture
		if (bHasVAT)
		{
			xBinder.BindSRV(s_xAnimationTexBinding, &pxAnimTex->GetPositionTexture()->m_xSRV);
		}
		else
		{
			xBinder.BindSRV(s_xAnimationTexBinding, &Flux_Graphics::s_xWhiteBlankTexture2D.m_xSRV);
		}

		// Bind instance buffers
		xBinder.BindUAV_Buffer(s_xTransformBufferBinding, &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xAnimDataBufferBinding, &pxGroup->GetAnimDataBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xVisibleIndexBufferBinding, &pxGroup->GetVisibleIndexBuffer().GetUAV());

		// Draw visible instances
		if (bUseGPUCulling)
		{
			// GPU culling: use indirect draw with count from visible count buffer
			// The culling compute shader wrote the visible instance count to the indirect buffer
			g_xGBufferCommandList.AddCommand<Flux_CommandDrawIndexedIndirect>(
				&pxGroup->GetIndirectBuffer(),
				1,   // drawCount = 1 (single draw call)
				0,   // offset = 0
				20   // stride = sizeof(VkDrawIndexedIndirectCommand) = 20 bytes
			);
		}
		else
		{
			// CPU culling fallback: direct instanced draw
			uint32_t uVisibleCount = pxGroup->GetVisibleCount();
			if (uVisibleCount > 0)
			{
				g_xGBufferCommandList.AddCommand<Flux_CommandDrawIndexed>(pxMesh->GetNumIndices(), uVisibleCount);
			}
		}

		s_uTotalInstances += pxGroup->GetInstanceCount();
		// Note: visible count is not accurate for GPU culling path (would need GPU readback)
		s_uVisibleInstances += bUseGPUCulling ? pxGroup->GetInstanceCount() : pxGroup->GetVisibleCount();
	}

	Flux::SubmitCommandList(&g_xGBufferCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_INSTANCED_MESHES);
}

void Flux_InstancedMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	if (!dbg_bEnableInstancedMeshes)
	{
		return;
	}

	if (s_apxInstanceGroups.empty())
	{
		return;
	}

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Bind FrameConstants once per command list
	xBinder.BindCBV(s_xShadowFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

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
		xBinder.PushConstant(s_xShadowScratchBufferBinding, &xIdentity, sizeof(xIdentity));
		xBinder.BindCBV(s_xShadowMatrixBinding, &xShadowMatrixBuffer.GetCBV());

		// Bind instance buffers
		xBinder.BindUAV_Buffer(s_xShadowTransformBufferBinding, &pxGroup->GetTransformBuffer().GetUAV());
		xBinder.BindUAV_Buffer(s_xShadowVisibleIndexBufferBinding, &pxGroup->GetVisibleIndexBuffer().GetUAV());

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

void Flux_InstancedMeshes::SubmitCullingTask()
{
	if (s_bCullingEnabled && dbg_bEnableGPUCulling && s_bCullingInitialized)
	{
		Zenith_TaskSystem::SubmitTask(&g_xCullingTask);
	}
}

void Flux_InstancedMeshes::WaitForCullingTask()
{
	if (s_bCullingEnabled && dbg_bEnableGPUCulling && s_bCullingInitialized)
	{
		g_xCullingTask.WaitUntilComplete();
	}
}

void Flux_InstancedMeshes::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_InstancedMeshes::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

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
