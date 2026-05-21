#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

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
	static void BuildPipelines();

	// Generate BRDF LUT (called once at initialization)
	static void GenerateBRDFLUT();

	// Update sky-based IBL from current atmosphere/skybox
	static void UpdateSkyIBL();

	// Per-frame update - checks if BRDF LUT needs generation
	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Per-frame state-machine update — must be called BEFORE Flux_RenderGraph::Compile()
	// each frame. Toggles SetPassEnabled for the 49 IBL passes based on the
	// amortised regen state machine. Cannot live as a pass OnPrepare callback
	// because OnPrepare only runs for *enabled* passes — once everything is
	// disabled the state machine could never re-enable anything.
	static void UpdateGraphPassEnables(Flux_RenderGraph& xGraph);

	// Mark all probes as needing update
	static void MarkAllProbesDirty();

	// Access IBL textures for shaders
	// Returns const reference - do not hold across frames as textures may be regenerated
	static const Flux_ShaderResourceView& GetBRDFLUTSRV();
	static const Flux_ShaderResourceView& GetIrradianceMapSRV();
	static const Flux_ShaderResourceView& GetPrefilteredMapSRV();

	// Configuration (continuous parameters; on/off toggles live in Zenith_GraphicsOptions)
	static void SetIntensity(float fIntensity);

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

	// Render-graph execute callbacks (public so the file-static graph wiring
	// can take their address as Flux_RenderGraph_OnRecordFunc).
	static void ExecuteBRDFLUTPass(Flux_CommandList* pxCmd, void* pUserData);
	static void ExecuteIrradianceFacePass(Flux_CommandList* pxCmd, void* pUserData);
	static void ExecutePrefilterMipFacePass(Flux_CommandList* pxCmd, void* pUserData);

	// Phase 7f: state moved to Flux_IBLImpl held by Zenith_Engine.
	// External consumers (DeferredShading) reach the render-attachments
	// via g_xEngine.IBL().m_xBRDFLUT etc.

private:
	static void CreateRenderTargets();
	static void DestroyRenderTargets();

	// Legacy functions retained as no-op compat shims; actual work happens in
	// the render-graph execute callbacks above.
	static void GenerateIrradianceMap();
	static void GeneratePrefilteredMap();
	static void GenerateIrradianceFace(u_int uFace);
	static void GeneratePrefilteredFace(u_int uMip, u_int uFace);

	// UpdateGraphPassEnables phases — split out so each step (reset / BRDF /
	// first-gen / amortised regen / apply) is independently readable.
	static void ResetIBLRegenStateForRecompile();
	static bool ResolveBRDFLUTRun();
	static void RunFirstGenerationFrame(bool (&abRunIrradiance)[6],
		bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6]);
	static void AdvanceAmortizedRegen(bool (&abRunIrradiance)[6],
		bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6]);
	static void ApplyResolvedIBLEnables(Flux_RenderGraph& xGraph,
		bool bRunBRDF,
		const bool (&abRunIrradiance)[6],
		const bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6]);

	// Phase 7f: pipelines, shaders, regen state, pass handles moved to
	// Flux_IBLImpl held by Zenith_Engine.
};
