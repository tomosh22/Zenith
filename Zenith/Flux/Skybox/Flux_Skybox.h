#pragma once

#include "Flux/Flux.h"

// ============================================================================
// Physical Atmosphere Constants
// Based on Earth's atmosphere for physically-based sky rendering
// ============================================================================

namespace AtmosphereConfig
{
	// Planet parameters
	constexpr float fEARTH_RADIUS = 6360000.0f;         // meters
	constexpr float fATMOSPHERE_RADIUS = 6420000.0f;    // meters (60km above surface)
	constexpr float fATMOSPHERE_HEIGHT = fATMOSPHERE_RADIUS - fEARTH_RADIUS;

	// Rayleigh scattering coefficients at sea level (per meter)
	// These produce the blue sky color
	constexpr float afRAYLEIGH_SCATTER[3] = { 5.8e-6f, 13.5e-6f, 33.1e-6f };  // RGB
	constexpr float fRAYLEIGH_SCALE_HEIGHT = 8000.0f;   // meters (density halves every 8km)

	// Mie scattering coefficients at sea level (per meter)
	// These produce the sun haze and horizon glow
	constexpr float fMIE_SCATTER = 3.996e-6f;           // RGB (achromatic)
	constexpr float fMIE_ABSORB = 4.4e-6f;              // Absorption coefficient
	constexpr float fMIE_SCALE_HEIGHT = 1200.0f;        // meters (concentrated lower)
	constexpr float fMIE_G = 0.76f;                     // Henyey-Greenstein asymmetry parameter

	// Sun parameters
	constexpr float fSUN_ANGULAR_RADIUS = 0.00935f;     // radians (~0.53 degrees)
	constexpr float fSUN_INTENSITY = 20.0f;             // Base sun intensity multiplier

	// Ray marching defaults
	constexpr u_int uDEFAULT_SKY_SAMPLES = 16;          // Samples for sky ray march
	constexpr u_int uDEFAULT_LIGHT_SAMPLES = 8;         // Samples for light ray march

	// Transmittance LUT dimensions
	constexpr u_int uTRANSMITTANCE_LUT_WIDTH = 256;     // View zenith angle
	constexpr u_int uTRANSMITTANCE_LUT_HEIGHT = 64;     // Altitude

	// Aerial perspective volume
	constexpr u_int uAERIAL_VOLUME_WIDTH = 32;          // X slices
	constexpr u_int uAERIAL_VOLUME_HEIGHT = 32;         // Y slices
	constexpr u_int uAERIAL_VOLUME_DEPTH = 32;          // Depth slices
	constexpr float fAERIAL_MAX_DISTANCE = 128000.0f;   // meters (128km max distance)
}

enum Skybox_DebugMode : u_int
{
	SKYBOX_DEBUG_NONE,
	SKYBOX_DEBUG_RAYLEIGH_ONLY,      // Rayleigh scattering isolated
	SKYBOX_DEBUG_MIE_ONLY,           // Mie scattering isolated
	SKYBOX_DEBUG_TRANSMITTANCE,      // Optical depth visualization
	SKYBOX_DEBUG_SCATTER_DIRECTION,  // View/sun angle relationship
	SKYBOX_DEBUG_AERIAL_DEPTH,       // Aerial perspective depth slices
	SKYBOX_DEBUG_SUN_DISK,           // Sun disk intensity falloff
	SKYBOX_DEBUG_LUT_PREVIEW,        // Transmittance LUT as overlay
	SKYBOX_DEBUG_RAY_STEPS,          // Number of ray march steps heatmap
	SKYBOX_DEBUG_PHASE_FUNCTION,     // Phase function visualization
	SKYBOX_DEBUG_COUNT
};

class Flux_Skybox
{
public:
	Flux_Skybox() = delete;
	~Flux_Skybox() = delete;

	static void Initialise();
	static void Shutdown();
	static void Reset();

	// Main sky render (cubemap or procedural atmosphere)
	static void Render(void*);
	static void SubmitRenderTask();
	static void WaitForRenderTask();

	// Aerial perspective pass (renders to HDR target when atmosphere enabled)
	static void RenderAerialPerspective(void*);
	static void SubmitAerialPerspectiveTask();
	static void WaitForAerialPerspectiveTask();

	// Atmosphere configuration
	static void SetAtmosphereEnabled(bool bEnabled);
	static void SetSunIntensity(float fIntensity);
	static void SetRayleighScale(float fScale);
	static void SetMieScale(float fScale);
	static void SetMieG(float fG);
	static void SetAerialPerspectiveEnabled(bool bEnabled);
	static void SetAerialPerspectiveStrength(float fStrength);

	// Getters
	static bool IsAtmosphereEnabled();
	static float GetSunIntensity();
	static float GetRayleighScale();
	static float GetMieScale();
	static float GetMieG();
	static bool IsAerialPerspectiveEnabled();
	static float GetAerialPerspectiveStrength();

	// Access transmittance LUT for other systems (IBL, fog)
	static Flux_ShaderResourceView& GetTransmittanceLUTSRV();

#ifdef ZENITH_TOOLS
	static void RegisterDebugVariables();
#endif

private:
	static void RenderCubemapSky();
	static void RenderAtmosphereSky();

	static void CreateRenderTargets();
	static void DestroyRenderTargets();

	// Transmittance LUT (precomputed for atmosphere)
	static Flux_RenderAttachment s_xTransmittanceLUT;
	static Flux_TargetSetup s_xTransmittanceLUTSetup;
	static bool s_bLUTNeedsUpdate;

	// Pipelines
	static Flux_Pipeline s_xCubemapPipeline;
	static Flux_Pipeline s_xAtmospherePipeline;
	static Flux_Pipeline s_xAerialPerspectivePipeline;

	// Shaders
	static Flux_Shader s_xCubemapShader;
	static Flux_Shader s_xAtmosphereShader;
	static Flux_Shader s_xAerialPerspectiveShader;

	// Atmosphere configuration state
	static bool s_bAtmosphereEnabled;
	static float s_fSunIntensity;
	static float s_fRayleighScale;
	static float s_fMieScale;
	static float s_fMieG;
	static bool s_bAerialPerspectiveEnabled;
	static float s_fAerialPerspectiveStrength;

	// Constants buffer for atmosphere parameters
	static Flux_DynamicConstantBuffer s_xAtmosphereConstantsBuffer;
};
