#pragma once

#include "Flux/Flux.h"

enum IBL_DebugMode : u_int
{
	IBL_DEBUG_NONE,
	IBL_DEBUG_IRRADIANCE_MAP,       // Show irradiance cubemap as sphere
	IBL_DEBUG_PREFILTERED_MIPS,     // Show all roughness mip levels
	IBL_DEBUG_BRDF_LUT,             // Show BRDF integration texture
	IBL_DEBUG_DIFFUSE_ONLY,         // Scene lit with only diffuse IBL
	IBL_DEBUG_SPECULAR_ONLY,        // Scene lit with only specular IBL
	IBL_DEBUG_FRESNEL,              // Fresnel term visualization
	IBL_DEBUG_REFLECTION_VECTOR,    // Visualize reflect(V, N) directions
	IBL_DEBUG_PROBE_VOLUMES,        // Show probe influence volumes
	IBL_DEBUG_PROBE_CAPTURE,        // Preview probe capture in corner
	IBL_DEBUG_ROUGHNESS_LOD,        // Which mip level is being sampled
	IBL_DEBUG_COUNT
};

// IBL regeneration state machine for frame-amortized updates
// Spreads expensive convolution work across multiple frames to avoid hitches
enum IBL_RegenState : u_int
{
	IBL_REGEN_IDLE,                 // No regeneration in progress
	IBL_REGEN_IRRADIANCE,           // Processing irradiance cubemap faces
	IBL_REGEN_PREFILTER             // Processing prefiltered cubemap mips/faces
};

// IBL configuration constants
namespace IBLConfig
{
	constexpr u_int uBRDF_LUT_SIZE = 512;             // BRDF LUT resolution (512x512)
	constexpr u_int uIRRADIANCE_SIZE = 32;            // Irradiance cubemap face size
	constexpr u_int uPREFILTER_SIZE = 128;            // Prefiltered env base resolution
	// Prefilter mip levels: 128->64->32->16->8->4->2 (7 mips)
	// More mips provides better rough surface quality at minimal VRAM cost (~10% more)
	// Roughness 0.0 samples mip 0 (128px), roughness 1.0 samples mip 6 (2px)
	constexpr u_int uPREFILTER_MIP_COUNT = 7;
	constexpr u_int uMAX_PROBES = 16;                 // Maximum environment probes

	// Frame-amortized regeneration: process up to 8 passes per frame
	// Total passes: 6 irradiance + 42 prefilter (7 mips × 6 faces) = 48
	// At 8 passes/frame, regeneration completes in 6 frames (~100ms at 60fps)
	// This prevents hitches when skybox changes during gameplay.
	//
	// NOTE: First generation after startup/reset is always non-amortized (all 48 passes).
	// This ensures all mip levels have valid Vulkan image layouts before the deferred
	// shader binds the prefiltered cubemap. Subsequent regenerations use amortization.
	constexpr u_int uPASSES_PER_FRAME = 8;
}

class Flux_IBL
{
public:
	Flux_IBL() = delete;
	~Flux_IBL() = delete;

	static void Initialise();
	static void Shutdown();
	static void Reset();

	// Generate BRDF LUT (called once at initialization)
	static void GenerateBRDFLUT();

	// Update sky-based IBL from current atmosphere/skybox
	static void UpdateSkyIBL();

	// Per-frame update - checks if BRDF LUT needs generation
	static void SubmitRenderTask();
	static void WaitForRenderTask();

	// Mark all probes as needing update
	static void MarkAllProbesDirty();

	// Access IBL textures for shaders
	// Returns const reference - do not hold across frames as textures may be regenerated
	static const Flux_ShaderResourceView& GetBRDFLUTSRV();
	static const Flux_ShaderResourceView& GetIrradianceMapSRV();
	static const Flux_ShaderResourceView& GetPrefilteredMapSRV();

	// Configuration
	static void SetEnabled(bool bEnabled);
	static void SetIntensity(float fIntensity);
	static void SetDiffuseEnabled(bool bEnabled);
	static void SetSpecularEnabled(bool bEnabled);

	// Getters
	static bool IsEnabled();
	static bool IsReady();  // Returns true only after all IBL textures have been generated
	static float GetIntensity();
	static bool IsDiffuseEnabled();
	static bool IsSpecularEnabled();
	static bool IsShowBRDFLUT();
	static bool IsForceRoughness();
	static float GetForcedRoughness();

#ifdef ZENITH_TOOLS
	static void RegisterDebugVariables();
#endif

private:
	static void CreateRenderTargets();
	static void DestroyRenderTargets();

	// Legacy functions that process all faces at once (kept for reference)
	static void GenerateIrradianceMap();
	static void GeneratePrefilteredMap();

	// Frame-amortized helpers that process a single face
	static void GenerateIrradianceFace(u_int uFace);
	static void GeneratePrefilteredFace(u_int uMip, u_int uFace);

	// BRDF Integration LUT (2D texture, computed once)
	static Flux_RenderAttachment s_xBRDFLUT;
	static Flux_TargetSetup s_xBRDFLUTSetup;
	static bool s_bBRDFLUTGenerated;

	// Sky-based irradiance map (cubemap for diffuse)
	static Flux_RenderAttachment s_xIrradianceMap;
	static Flux_TargetSetup s_axIrradianceFaceSetup[6];  // Per-face target setups

	// Sky-based prefiltered environment map (cubemap with mips for specular)
	static Flux_RenderAttachment s_xPrefilteredMap;
	static Flux_TargetSetup s_axPrefilteredFaceSetup[6];  // Per-face target setups (mip 0)

	// Pipelines
	static Flux_Pipeline s_xBRDFLUTPipeline;
	static Flux_Pipeline s_xIrradianceConvolvePipeline;
	static Flux_Pipeline s_xPrefilterPipeline;

	// Shaders
	static Flux_Shader s_xBRDFLUTShader;
	static Flux_Shader s_xIrradianceConvolveShader;
	static Flux_Shader s_xPrefilterShader;

	// Convolution binding handles
	static Flux_BindingHandle s_xIrradianceFrameConstantsBinding;
	static Flux_BindingHandle s_xPrefilterFrameConstantsBinding;

	// Configuration state
	static bool s_bEnabled;
	static float s_fIntensity;
	static bool s_bDiffuseEnabled;
	static bool s_bSpecularEnabled;

	// Dirty flags
	static bool s_bSkyIBLDirty;
	static bool s_bIBLReady;  // True after all IBL textures have been generated
	static bool s_bFirstGeneration;  // True until first full generation completes

	// Frame-amortized regeneration state
	static IBL_RegenState s_eRegenState;
	static u_int s_uRegenFace;  // Current face being processed (0-5)
	static u_int s_uRegenMip;   // Current mip being processed (0-6, prefilter only)
};
