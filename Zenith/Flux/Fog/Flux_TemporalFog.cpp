#include "Zenith.h"

#include "Flux/Fog/Flux_TemporalFog.h"
#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Fog/Flux_FroxelFog.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

/*
 * Flux_TemporalFog - Temporal Reprojection for Volumetric Fog
 *
 * Technique: Accumulates fog samples over multiple frames using reprojection
 *            and neighborhood clamping to reduce aliasing and increase quality.
 *
 * Pipeline:
 *   1. Sample current frame fog (from FroxelFog with sub-voxel jitter)
 *   2. Resolve Pass (compute): Reproject history, blend with current frame
 *
 * Resources:
 *   - s_axHistoryBuffers[2] (ping-pong 3D RGBA16F) - matches froxel grid size
 *   - Motion vectors from frame constants
 *   - Debug visualization textures
 *
 * Works as enhancement to Froxel technique - not standalone
 *
 * Performance: +0.5ms overhead on top of froxel technique
 */

// Command list for resolve pass
static Flux_CommandList g_xResolveCommandList("TemporalFog_Resolve");

// Compute pipeline
static Zenith_Vulkan_Shader s_xResolveShader;
static Zenith_Vulkan_Pipeline s_xResolvePipeline;
static Zenith_Vulkan_RootSig s_xResolveRootSig;

// Temporal history configuration (matches froxel grid)
static constexpr u_int TEMPORAL_WIDTH = 160;
static constexpr u_int TEMPORAL_HEIGHT = 90;
static constexpr u_int TEMPORAL_DEPTH = 64;

// Ping-pong history buffers
static Flux_RenderAttachment s_axHistoryBuffers[2];
static u_int s_uCurrentHistoryIndex = 0;

// Output buffer (resolved result)
static Flux_RenderAttachment s_xResolvedOutput;

// Debug output for motion vectors
static Flux_RenderAttachment s_xDebugMotionVectors;

// Halton sequence for sub-voxel jitter (16 samples)
static const Zenith_Maths::Vector2 s_axHaltonJitter[16] = {
	{ 0.5f, 0.333333f },
	{ 0.25f, 0.666667f },
	{ 0.75f, 0.111111f },
	{ 0.125f, 0.444444f },
	{ 0.625f, 0.777778f },
	{ 0.375f, 0.222222f },
	{ 0.875f, 0.555556f },
	{ 0.0625f, 0.888889f },
	{ 0.5625f, 0.037037f },
	{ 0.3125f, 0.370370f },
	{ 0.8125f, 0.703704f },
	{ 0.1875f, 0.148148f },
	{ 0.6875f, 0.481481f },
	{ 0.4375f, 0.814815f },
	{ 0.9375f, 0.259259f },
	{ 0.03125f, 0.592593f }
};

static u_int s_uJitterIndex = 0;

// Debug variables
DEBUGVAR float dbg_fTemporalBlendWeight = 0.9f;
DEBUGVAR bool dbg_bTemporalJitterEnabled = true;

// Push constant structure (must match shader)
struct ResolveConstants
{
	Zenith_Maths::Vector4 m_xGridDimensions;   // x = width, y = height, z = depth, w = unused
	Zenith_Maths::Vector4 m_xJitterOffset;     // xy = current jitter, zw = previous jitter
	float m_fBlendWeight;
	float m_fNearZ;
	float m_fFarZ;
	u_int m_uDebugMode;
	u_int m_uFrameIndex;
	float m_fPad0;
	float m_fPad1;
	float m_fPad2;
};

static ResolveConstants s_xResolveConstants;

// Previous frame jitter for reprojection
static Zenith_Maths::Vector2 s_xPreviousJitter = { 0.0f, 0.0f };

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xResolveFrameConstantsBinding;
static Flux_BindingHandle s_xResolveCurrentFogBinding;
static Flux_BindingHandle s_xResolveHistoryFogBinding;
static Flux_BindingHandle s_xResolveOutputBinding;
static Flux_BindingHandle s_xResolveDebugMotionBinding;

void Flux_TemporalFog::Initialise()
{
	// Create history buffers (3D, matching froxel grid size)
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = TEMPORAL_WIDTH;
	xBuilder.m_uHeight = TEMPORAL_HEIGHT;
	xBuilder.m_uDepth = TEMPORAL_DEPTH;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.m_eTextureType = TEXTURE_TYPE_3D;
	xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ) | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);

	xBuilder.BuildColour(s_axHistoryBuffers[0], "TemporalHistory_A");
	xBuilder.BuildColour(s_axHistoryBuffers[1], "TemporalHistory_B");
	xBuilder.BuildColour(s_xResolvedOutput, "TemporalResolved");

	// Create 2D debug motion vector texture
	Flux_RenderAttachmentBuilder xDebugBuilder;
	xDebugBuilder.m_uWidth = TEMPORAL_WIDTH;
	xDebugBuilder.m_uHeight = TEMPORAL_HEIGHT;
	xDebugBuilder.m_uDepth = 1;
	xDebugBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xDebugBuilder.m_eTextureType = TEXTURE_TYPE_2D;
	xDebugBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ) | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);
	xDebugBuilder.BuildColour(s_xDebugMotionVectors, "TemporalDebugMotion");

	// Initialize resolve compute shader
	s_xResolveShader.InitialiseCompute("Fog/Flux_TemporalFog_Resolve.comp");

	// Build resolve root signature
	Flux_PipelineLayout xResolveLayout;
	xResolveLayout.m_uNumDescriptorSets = 1;
	xResolveLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;        // Frame constants
	xResolveLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;        // Scratch buffer for push constants
	xResolveLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;       // Current fog (from froxel) (was 1)
	xResolveLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;       // History fog (was 2)
	xResolveLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_STORAGE_IMAGE; // Output fog (was 3)
	xResolveLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_STORAGE_IMAGE; // Debug motion vectors (was 4)
	xResolveLayout.m_axDescriptorSetLayouts[0].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_MAX;
	Zenith_Vulkan_RootSigBuilder::FromSpecification(s_xResolveRootSig, xResolveLayout);

	// Build resolve compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xResolveBuilder;
	xResolveBuilder.WithShader(s_xResolveShader)
		.WithLayout(s_xResolveRootSig.m_xLayout)
		.Build(s_xResolvePipeline);
	s_xResolvePipeline.m_xRootSig = s_xResolveRootSig;

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xResolveReflection = s_xResolveShader.GetReflection();
	s_xResolveFrameConstantsBinding = xResolveReflection.GetBinding("FrameConstants");
	s_xResolveCurrentFogBinding = xResolveReflection.GetBinding("g_xCurrentFog");
	s_xResolveHistoryFogBinding = xResolveReflection.GetBinding("g_xHistoryFog");
	s_xResolveOutputBinding = xResolveReflection.GetBinding("g_xOutput");
	s_xResolveDebugMotionBinding = xResolveReflection.GetBinding("g_xDebugMotion");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Temporal", "Blend Weight" }, dbg_fTemporalBlendWeight, 0.0f, 1.0f);
	Zenith_DebugVariables::AddBoolean({ "Render", "Volumetric Fog", "Temporal", "Jitter Enabled" }, dbg_bTemporalJitterEnabled);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_TemporalFog initialised (%ux%ux%u history buffers)",
		TEMPORAL_WIDTH, TEMPORAL_HEIGHT, TEMPORAL_DEPTH);
}

void Flux_TemporalFog::Reset()
{
	g_xResolveCommandList.Reset(true);
	s_uCurrentHistoryIndex = 0;
	s_uJitterIndex = 0;
	s_xPreviousJitter = { 0.0f, 0.0f };
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_TemporalFog::Reset()");
}

void Flux_TemporalFog::SubmitResolveTask()
{
	// Not using task system for now - direct render
}

void Flux_TemporalFog::WaitForResolveTask()
{
}

Zenith_Maths::Vector2 Flux_TemporalFog::GetTemporalJitter()
{
	if (!dbg_bTemporalJitterEnabled)
	{
		return { 0.0f, 0.0f };
	}

	// Return current frame's jitter offset (in -0.5 to 0.5 range)
	Zenith_Maths::Vector2 xJitter = s_axHaltonJitter[s_uJitterIndex % 16];
	xJitter.x -= 0.5f;
	xJitter.y -= 0.5f;
	return xJitter;
}

Flux_RenderAttachment& Flux_TemporalFog::GetResolvedOutput()
{
	return s_xResolvedOutput;
}

Flux_RenderAttachment& Flux_TemporalFog::GetHistoryBuffer()
{
	return s_axHistoryBuffers[s_uCurrentHistoryIndex];
}

Flux_RenderAttachment& Flux_TemporalFog::GetDebugMotionVectors()
{
	return s_xDebugMotionVectors;
}

void Flux_TemporalFog::Render(void*)
{
	// Get debug mode
	extern u_int dbg_uVolFogDebugMode;

	// Get current jitter
	Zenith_Maths::Vector2 xCurrentJitter = GetTemporalJitter();

	// Set up resolve constants
	s_xResolveConstants.m_xGridDimensions = Zenith_Maths::Vector4(
		static_cast<float>(TEMPORAL_WIDTH),
		static_cast<float>(TEMPORAL_HEIGHT),
		static_cast<float>(TEMPORAL_DEPTH),
		0.0f
	);
	s_xResolveConstants.m_xJitterOffset = Zenith_Maths::Vector4(
		xCurrentJitter.x,
		xCurrentJitter.y,
		s_xPreviousJitter.x,
		s_xPreviousJitter.y
	);
	// On first frame (or after reset), use blend weight 0 to avoid sampling uninitialized history
	// s_uJitterIndex starts at 0 after Reset(), so check if this is the first render call
	s_xResolveConstants.m_fBlendWeight = (s_uJitterIndex == 0) ? 0.0f : dbg_fTemporalBlendWeight;
	s_xResolveConstants.m_fNearZ = Flux_FroxelFog::GetNearZ();
	s_xResolveConstants.m_fFarZ = Flux_FroxelFog::GetFarZ();
	s_xResolveConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	s_xResolveConstants.m_uFrameIndex = Flux::GetFrameCounter();

	// Determine source and destination buffers (ping-pong)
	u_int uHistoryReadIdx = s_uCurrentHistoryIndex;
	u_int uHistoryWriteIdx = (s_uCurrentHistoryIndex + 1) % 2;

	// ========== RESOLVE PASS ==========
	g_xResolveCommandList.Reset(false);
	g_xResolveCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xResolvePipeline);

	Flux_ShaderBinder xResolveBinder(g_xResolveCommandList);
	xResolveBinder.BindCBV(s_xResolveFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	// Current fog comes from froxel lighting grid
	xResolveBinder.BindSRV(s_xResolveCurrentFogBinding, &Flux_FroxelFog::GetLightingGrid().m_pxSRV);
	// History from previous frame
	xResolveBinder.BindSRV(s_xResolveHistoryFogBinding, &s_axHistoryBuffers[uHistoryReadIdx].m_pxSRV);
	// Output to resolved buffer and also update history
	xResolveBinder.BindUAV_Texture(s_xResolveOutputBinding, &s_axHistoryBuffers[uHistoryWriteIdx].m_pxUAV);
	xResolveBinder.BindUAV_Texture(s_xResolveDebugMotionBinding, &s_xDebugMotionVectors.m_pxUAV);

	xResolveBinder.PushConstant(&s_xResolveConstants, sizeof(ResolveConstants));
	g_xResolveCommandList.AddCommand<Flux_CommandDispatch>(
		(TEMPORAL_WIDTH + 7) / 8,
		(TEMPORAL_HEIGHT + 7) / 8,
		(TEMPORAL_DEPTH + 7) / 8
	);

	Flux::SubmitCommandList(&g_xResolveCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_VOLUMEFOG_TEMPORAL);

	// Swap history buffers for next frame
	s_uCurrentHistoryIndex = uHistoryWriteIdx;

	// Store current jitter as previous for next frame
	s_xPreviousJitter = xCurrentJitter;

	// Advance jitter sequence
	s_uJitterIndex++;
}
