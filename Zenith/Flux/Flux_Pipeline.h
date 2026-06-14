#pragma once

// Graphics pipeline specification + the fullscreen-pipeline build helper.
// Split out of Flux.h.
#include "Flux/Flux_BackendTypes.h"             // Flux_Shader, Flux_BlendState, Flux_PipelineLayout, Flux_VertexInputDescription, Flux_Pipeline
#include "Flux/Flux_Enums.h"                    // TextureFormat, DepthCompareFunc, LoadAction, CullMode
#include "Flux/Shaders/Generated/FluxShaderProgram.h"  // FluxShaderProgram (by-value param of Flux_PipelineHelper)

struct Flux_PipelineSpecification
{
	Flux_PipelineSpecification() = default;

	Flux_Shader* m_pxShader;

	Flux_BlendState m_axBlendStates[FLUX_MAX_TARGETS];

	bool m_bDepthTestEnabled = true;
	bool m_bDepthWriteEnabled = true;
	DepthCompareFunc m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
	TextureFormat m_eDepthStencilFormat = TEXTURE_FORMAT_NONE;
	bool m_bUsePushConstants = true;
	bool m_bUseTesselation = false;

	Flux_PipelineLayout m_xPipelineLayout;

	Flux_VertexInputDescription m_xVertexInputDesc;

	TextureFormat m_aeColourAttachmentFormats[FLUX_MAX_TARGETS] = {};
	uint32_t m_uNumColourAttachments = 0;
	LoadAction m_eColourLoadAction;
	LoadAction m_eColourStoreAction;
	LoadAction m_eDepthStencilLoadAction;
	LoadAction m_eDepthStencilStoreAction;
	bool m_bWireframe = false;

	CullMode m_eCullMode = CULL_MODE_NONE;  // No culling by default (matches previous hardcoded behavior)

	bool m_bDepthBias = false;
	float m_fDepthBiasConstant = 0.0f;
	float m_fDepthBiasSlope = 0.0f;
	float m_fDepthBiasClamp = 0.0f;

	// Dynamic depth-bias: when set, this spec value is the initial/default but can
	// be overridden per-draw via Flux_CommandSetDepthBias (vkCmdSetDepthBias, core
	// Vulkan 1.0). NOTE: dynamic CULL mode was removed — cull is baked statically
	// into the pipeline via m_eCullMode above; vkCmdSetCullMode is
	// VK_EXT_extended_dynamic_state (Vulkan 1.3) and is not exported by the Android
	// NDK libvulkan loader.
	bool m_bDynamicDepthBias = false;
};

// Helper to reduce boilerplate when creating fullscreen post-processing pipelines.
// The pattern of init shader -> create spec -> populate layout -> build pipeline
// is repeated 10+ times across Flux subsystems.
class Flux_PipelineHelper
{
public:
	Flux_PipelineHelper() = delete;

	// Initialises a shader from the Slang program registry and builds a
	// fullscreen pipeline with no depth test/write. Covers the common case
	// used by HDR, SSR, SSGI, IBL, SSAO etc.
	static void BuildFullscreenPipeline(
		Flux_Shader& xShader,
		Flux_Pipeline& xPipeline,
		FluxShaderProgram eProgram,
		TextureFormat eColourFormat,
		TextureFormat eDepthStencilFormat = TEXTURE_FORMAT_NONE);

	// Creates a pre-populated fullscreen spec without building.
	// Use when you need to customise blend states or other settings before building.
	static Flux_PipelineSpecification CreateFullscreenSpec(
		Flux_Shader& xShader,
		FluxShaderProgram eProgram,
		TextureFormat eColourFormat,
		TextureFormat eDepthStencilFormat = TEXTURE_FORMAT_NONE);

	// Multi-render-target variant — covers N>=1 colour targets. The single-RTV
	// CreateFullscreenSpec above delegates to this with uNumColourAttachments==1.
	static Flux_PipelineSpecification CreateFullscreenSpecMRT(
		Flux_Shader& xShader,
		FluxShaderProgram eProgram,
		const TextureFormat* aeColourFormats,
		u_int uNumColourAttachments,
		TextureFormat eDepthStencilFormat = TEXTURE_FORMAT_NONE);
};
