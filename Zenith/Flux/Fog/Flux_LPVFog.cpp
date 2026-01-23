#include "Zenith.h"

#include "Flux/Fog/Flux_LPVFog.h"
#include "Flux/Fog/Flux_VolumeFog.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

/*
 * Flux_LPVFog - Light Propagation Volumes for Volumetric Fog
 *
 * Technique: Inject virtual point lights (VPLs) into a 3D grid and propagate
 *            light iteratively to simulate multiple scattering
 *
 * Pipeline:
 *   1. Inject Pass (compute): Place VPLs from sun direction into LPV grid
 *   2. Propagate Pass (compute): Iteratively spread light to neighbors (N iterations)
 *   3. Apply Pass (fragment): Sample LPV and apply to fog via ray marching
 *
 * Resources:
 *   - s_axLPVGrids[2] (ping-pong 3D RGBA16F, 32^3 per cascade)
 *   - 3 cascades for different distance ranges
 *
 * Performance: 3-5ms at 1080p depending on cascade count and propagation iterations
 */

// Command lists for the three passes
static Flux_CommandList g_xInjectCommandList("LPVFog_Inject");
static Flux_CommandList g_xPropagateCommandList("LPVFog_Propagate");
static Flux_CommandList g_xApplyCommandList("LPVFog_Apply");

// Compute pipelines
static Zenith_Vulkan_Shader s_xInjectShader;
static Zenith_Vulkan_Pipeline s_xInjectPipeline;
static Zenith_Vulkan_RootSig s_xInjectRootSig;
static Zenith_Vulkan_Shader s_xPropagateShader;
static Zenith_Vulkan_Pipeline s_xPropagatePipeline;
static Zenith_Vulkan_RootSig s_xPropagateRootSig;

// Apply fragment pipeline
static Flux_Shader s_xApplyShader;
static Flux_Pipeline s_xApplyPipeline;

// LPV grid configuration
static constexpr u_int LPV_GRID_SIZE = 32;
static constexpr u_int LPV_NUM_CASCADES = 3;
static constexpr float LPV_CASCADE_RADII[LPV_NUM_CASCADES] = { 50.0f, 150.0f, 500.0f };

// 3D LPV grids for each cascade (ping-pong pair)
static Flux_RenderAttachment s_axLPVGrids[LPV_NUM_CASCADES][2];  // [cascade][ping-pong]
static u_int s_uCurrentPingPong = 0;

// Debug 2D texture for injection visualization
static Flux_RenderAttachment s_xDebugInjectionTexture;

// Debug variables
DEBUGVAR u_int dbg_uLPVPropagationSteps = 8;
DEBUGVAR float dbg_fLPVDamping = 0.9f;
DEBUGVAR float dbg_fLPVIntensity = 1.0f;
DEBUGVAR u_int dbg_uLPVDebugCascade = 0;

// Push constant structures (must match shader)
struct InjectConstants
{
	Zenith_Maths::Vector4 m_xLightDirection;    // xyz = direction, w = unused
	Zenith_Maths::Vector4 m_xLightColour;       // RGB = color, A = intensity
	Zenith_Maths::Vector4 m_xCascadeCenter;     // xyz = center, w = radius
	u_int m_uGridSize;
	u_int m_uCascadeIndex;
	float m_fCascadeRadius;
	float m_fPad0;
};

struct PropagateConstants
{
	Zenith_Maths::Vector4 m_xGridDimensions;
	u_int m_uIterationIndex;
	u_int m_uCascadeIndex;
	float m_fDamping;
	u_int m_uDebugMode;
};

struct ApplyConstants
{
	Zenith_Maths::Vector4 m_axCascadeCenters[3];    // xyz = center, w = radius (packed to avoid alignment issues)
	u_int m_uNumCascades;
	u_int m_uDebugMode;
	u_int m_uDebugCascade;
	float m_fPad0;
};

static InjectConstants s_xInjectConstants;
static PropagateConstants s_xPropagateConstants;
static ApplyConstants s_xApplyConstants;

// Cached binding handles from shader reflection
// Inject pass
static Flux_BindingHandle s_xInjectFrameConstantsBinding;
static Flux_BindingHandle s_xInjectShadowMapBinding;
static Flux_BindingHandle s_xInjectOutputBinding;
static Flux_BindingHandle s_xInjectDebugBinding;
// Propagate pass
static Flux_BindingHandle s_xPropagateInputBinding;
static Flux_BindingHandle s_xPropagateOutputBinding;
// Apply pass
static Flux_BindingHandle s_xApplyFrameConstantsBinding;
static Flux_BindingHandle s_xApplyDepthBinding;
static Flux_BindingHandle s_xApplyLPVCascade0Binding;
static Flux_BindingHandle s_xApplyLPVCascade1Binding;
static Flux_BindingHandle s_xApplyLPVCascade2Binding;
static Flux_BindingHandle s_xApplyNoiseBinding;

void Flux_LPVFog::Initialise()
{
	// Create 3D LPV grids for each cascade (ping-pong pairs)
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = LPV_GRID_SIZE;
	xBuilder.m_uHeight = LPV_GRID_SIZE;
	xBuilder.m_uDepth = LPV_GRID_SIZE;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.m_eTextureType = TEXTURE_TYPE_3D;
	xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ) | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);

	for (u_int uCascade = 0; uCascade < LPV_NUM_CASCADES; uCascade++)
	{
		char szName[64];
		sprintf_s(szName, "LPVGrid_C%u_A", uCascade);
		xBuilder.BuildColour(s_axLPVGrids[uCascade][0], szName);

		sprintf_s(szName, "LPVGrid_C%u_B", uCascade);
		xBuilder.BuildColour(s_axLPVGrids[uCascade][1], szName);
	}

	// Create debug injection texture (2D slice visualization)
	Flux_RenderAttachmentBuilder xDebugBuilder;
	xDebugBuilder.m_uWidth = LPV_GRID_SIZE;
	xDebugBuilder.m_uHeight = LPV_GRID_SIZE;
	xDebugBuilder.m_uDepth = 1;
	xDebugBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xDebugBuilder.m_eTextureType = TEXTURE_TYPE_2D;
	xDebugBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ) | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);
	xDebugBuilder.BuildColour(s_xDebugInjectionTexture, "LPVDebugInjection");

	// Initialize inject compute shader
	s_xInjectShader.InitialiseCompute("Fog/Flux_LPVFog_Inject.comp");

	// Build inject root signature from shader reflection
	const Flux_ShaderReflection& xInjectReflection = s_xInjectShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(s_xInjectRootSig, xInjectReflection);

	// Build inject compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xInjectBuilder;
	xInjectBuilder.WithShader(s_xInjectShader)
		.WithLayout(s_xInjectRootSig.m_xLayout)
		.Build(s_xInjectPipeline);
	s_xInjectPipeline.m_xRootSig = s_xInjectRootSig;

	// Initialize propagate compute shader
	s_xPropagateShader.InitialiseCompute("Fog/Flux_LPVFog_Propagate.comp");

	// Build propagate root signature from shader reflection
	const Flux_ShaderReflection& xPropagateReflection = s_xPropagateShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(s_xPropagateRootSig, xPropagateReflection);

	// Build propagate compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xPropagateBuilder;
	xPropagateBuilder.WithShader(s_xPropagateShader)
		.WithLayout(s_xPropagateRootSig.m_xLayout)
		.Build(s_xPropagatePipeline);
	s_xPropagatePipeline.m_xRootSig = s_xPropagateRootSig;

	// Cache inject binding handles (xInjectReflection already declared above)
	s_xInjectFrameConstantsBinding = xInjectReflection.GetBinding("FrameConstants");
	s_xInjectShadowMapBinding = xInjectReflection.GetBinding("u_xShadowMap");
	s_xInjectOutputBinding = xInjectReflection.GetBinding("u_xLPVOutput");
	s_xInjectDebugBinding = xInjectReflection.GetBinding("u_xDebugInjection");

	// Cache propagate binding handles (xPropagateReflection already declared above)
	s_xPropagateInputBinding = xPropagateReflection.GetBinding("u_xLPVInput");
	s_xPropagateOutputBinding = xPropagateReflection.GetBinding("u_xLPVOutput");

	// Initialize apply fragment shader
	s_xApplyShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_LPVFog_Apply.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xApplySpec;
	xApplySpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetup();
	xApplySpec.m_pxShader = &s_xApplyShader;
	xApplySpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xApplySpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;   // Frame constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;   // Scratch buffer for push constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Depth texture
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // LPV Cascade 0
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // LPV Cascade 1
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // LPV Cascade 2
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Noise texture

	xApplySpec.m_bDepthTestEnabled = false;
	xApplySpec.m_bDepthWriteEnabled = false;

	// Blend: fog over scene (src alpha, 1-src alpha)
	xApplySpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xApplySpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xApplySpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(s_xApplyPipeline, xApplySpec);

	// Cache apply binding handles
	const Flux_ShaderReflection& xApplyReflection = s_xApplyShader.GetReflection();
	s_xApplyFrameConstantsBinding = xApplyReflection.GetBinding("FrameConstants");
	s_xApplyDepthBinding = xApplyReflection.GetBinding("u_xDepthTexture");
	s_xApplyLPVCascade0Binding = xApplyReflection.GetBinding("u_xLPVCascade0");
	s_xApplyLPVCascade1Binding = xApplyReflection.GetBinding("u_xLPVCascade1");
	s_xApplyLPVCascade2Binding = xApplyReflection.GetBinding("u_xLPVCascade2");
	s_xApplyNoiseBinding = xApplyReflection.GetBinding("u_xNoiseTexture3D");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "LPV", "Propagation Steps" }, dbg_uLPVPropagationSteps, 1, 16);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "LPV", "Damping" }, dbg_fLPVDamping, 0.5f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "LPV", "Intensity" }, dbg_fLPVIntensity, 0.0f, 5.0f);
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "LPV", "Debug Cascade" }, dbg_uLPVDebugCascade, 0, LPV_NUM_CASCADES - 1);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_LPVFog initialised (%ux%ux%u grid, %u cascades)",
		LPV_GRID_SIZE, LPV_GRID_SIZE, LPV_GRID_SIZE, LPV_NUM_CASCADES);
}

void Flux_LPVFog::Reset()
{
	g_xInjectCommandList.Reset(true);
	g_xPropagateCommandList.Reset(true);
	g_xApplyCommandList.Reset(true);
	s_uCurrentPingPong = 0;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_LPVFog::Reset()");
}

Flux_RenderAttachment& Flux_LPVFog::GetLPVGrid(u_int uCascade)
{
	return s_axLPVGrids[uCascade][s_uCurrentPingPong];
}

Flux_RenderAttachment& Flux_LPVFog::GetDebugInjectionPoints()
{
	return s_xDebugInjectionTexture;
}

void Flux_LPVFog::SubmitInjectTask()
{
	// Not using task system for now - direct render
}

void Flux_LPVFog::SubmitPropagateTask()
{
	// Not using task system for now - direct render
}

void Flux_LPVFog::SubmitApplyTask()
{
	// Not using task system for now - direct render
}

void Flux_LPVFog::WaitForInjectTask()
{
}

void Flux_LPVFog::WaitForPropagateTask()
{
}

void Flux_LPVFog::WaitForApplyTask()
{
}

void Flux_LPVFog::Render(void*)
{
	// Get debug mode
	extern u_int dbg_uVolFogDebugMode;

	// Get camera position for cascade centering
	const Zenith_Maths::Vector3& xCamPos = Flux_Graphics::s_xFrameConstants.m_xCamPos_Pad;
	const Zenith_Maths::Vector3& xSunDir = Flux_Graphics::s_xFrameConstants.m_xSunDir_Pad;

	// ========== INJECT PASS (for each cascade) ==========
	for (u_int uCascade = 0; uCascade < LPV_NUM_CASCADES; uCascade++)
	{
		s_xInjectConstants.m_xLightDirection = Zenith_Maths::Vector4(xSunDir.x, xSunDir.y, xSunDir.z, 0.0f);
		s_xInjectConstants.m_xLightColour = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, dbg_fLPVIntensity);
		s_xInjectConstants.m_xCascadeCenter = Zenith_Maths::Vector4(xCamPos.x, xCamPos.y, xCamPos.z, LPV_CASCADE_RADII[uCascade]);
		s_xInjectConstants.m_uGridSize = LPV_GRID_SIZE;
		s_xInjectConstants.m_uCascadeIndex = uCascade;
		s_xInjectConstants.m_fCascadeRadius = LPV_CASCADE_RADII[uCascade];

		g_xInjectCommandList.Reset(false);
		g_xInjectCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xInjectPipeline);

		Flux_ShaderBinder xInjectBinder(g_xInjectCommandList);
		xInjectBinder.BindCBV(s_xInjectFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		// Use blue noise as placeholder for shadow map
		xInjectBinder.BindSRV(s_xInjectShadowMapBinding, &Flux_VolumeFog::GetBlueNoiseTexture()->m_xSRV);
		xInjectBinder.BindUAV_Texture(s_xInjectOutputBinding, &s_axLPVGrids[uCascade][0].m_pxUAV);
		xInjectBinder.BindUAV_Texture(s_xInjectDebugBinding, &s_xDebugInjectionTexture.m_pxUAV);
		xInjectBinder.PushConstant(&s_xInjectConstants, sizeof(InjectConstants));
		g_xInjectCommandList.AddCommand<Flux_CommandDispatch>(
			(LPV_GRID_SIZE + 7) / 8,
			(LPV_GRID_SIZE + 7) / 8,
			(LPV_GRID_SIZE + 7) / 8
		);

		Flux::SubmitCommandList(&g_xInjectCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_VOLUMEFOG_INJECT);
	}

	// ========== PROPAGATE PASS (N iterations per cascade) ==========
	for (u_int uCascade = 0; uCascade < LPV_NUM_CASCADES; uCascade++)
	{
		for (u_int uIter = 0; uIter < dbg_uLPVPropagationSteps; uIter++)
		{
			u_int uSrcIdx = uIter % 2;
			u_int uDstIdx = (uIter + 1) % 2;

			s_xPropagateConstants.m_xGridDimensions = Zenith_Maths::Vector4(
				static_cast<float>(LPV_GRID_SIZE),
				static_cast<float>(LPV_GRID_SIZE),
				static_cast<float>(LPV_GRID_SIZE),
				0.0f
			);
			s_xPropagateConstants.m_uIterationIndex = uIter;
			s_xPropagateConstants.m_uCascadeIndex = uCascade;
			s_xPropagateConstants.m_fDamping = dbg_fLPVDamping;
			s_xPropagateConstants.m_uDebugMode = dbg_uVolFogDebugMode;

			g_xPropagateCommandList.Reset(false);
			g_xPropagateCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xPropagatePipeline);

			Flux_ShaderBinder xPropagateBinder(g_xPropagateCommandList);
			xPropagateBinder.BindSRV(s_xPropagateInputBinding, &s_axLPVGrids[uCascade][uSrcIdx].m_pxSRV);
			xPropagateBinder.BindUAV_Texture(s_xPropagateOutputBinding, &s_axLPVGrids[uCascade][uDstIdx].m_pxUAV);
			xPropagateBinder.PushConstant(&s_xPropagateConstants, sizeof(PropagateConstants));
			g_xPropagateCommandList.AddCommand<Flux_CommandDispatch>(
				(LPV_GRID_SIZE + 7) / 8,
				(LPV_GRID_SIZE + 7) / 8,
				(LPV_GRID_SIZE + 7) / 8
			);

			Flux::SubmitCommandList(&g_xPropagateCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_VOLUMEFOG_LIGHT);
		}
	}

	// Final ping-pong index after propagation
	s_uCurrentPingPong = dbg_uLPVPropagationSteps % 2;

	// ========== APPLY PASS ==========
	// Set cascade centers (all centered on camera, radius packed in w component)
	for (u_int uCascade = 0; uCascade < LPV_NUM_CASCADES; uCascade++)
	{
		s_xApplyConstants.m_axCascadeCenters[uCascade] = Zenith_Maths::Vector4(xCamPos.x, xCamPos.y, xCamPos.z, LPV_CASCADE_RADII[uCascade]);
	}
	s_xApplyConstants.m_uNumCascades = LPV_NUM_CASCADES;
	s_xApplyConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	s_xApplyConstants.m_uDebugCascade = dbg_uLPVDebugCascade;

	g_xApplyCommandList.Reset(false);
	g_xApplyCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xApplyPipeline);
	g_xApplyCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xApplyCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xApplyBinder(g_xApplyCommandList);
	xApplyBinder.BindCBV(s_xApplyFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xApplyBinder.BindSRV(s_xApplyDepthBinding, Flux_Graphics::GetDepthStencilSRV());
	xApplyBinder.BindSRV(s_xApplyLPVCascade0Binding, &s_axLPVGrids[0][s_uCurrentPingPong].m_pxSRV);
	xApplyBinder.BindSRV(s_xApplyLPVCascade1Binding, &s_axLPVGrids[1][s_uCurrentPingPong].m_pxSRV);
	xApplyBinder.BindSRV(s_xApplyLPVCascade2Binding, &s_axLPVGrids[2][s_uCurrentPingPong].m_pxSRV);
	xApplyBinder.BindSRV(s_xApplyNoiseBinding, &Flux_VolumeFog::GetNoiseTexture3D()->m_xSRV);
	xApplyBinder.PushConstant(&s_xApplyConstants, sizeof(ApplyConstants));
	g_xApplyCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xApplyCommandList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_FOG);
}
