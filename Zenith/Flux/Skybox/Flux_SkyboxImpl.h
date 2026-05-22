#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "AssetHandling/Zenith_AssetHandle.h"

namespace AtmosphereConfig
{
	constexpr float fEARTH_RADIUS = 6360000.0f;
	constexpr float fATMOSPHERE_RADIUS = 6420000.0f;
	constexpr float fATMOSPHERE_HEIGHT = fATMOSPHERE_RADIUS - fEARTH_RADIUS;

	constexpr float afRAYLEIGH_SCATTER[3] = { 5.8e-6f, 13.5e-6f, 33.1e-6f };
	constexpr float fRAYLEIGH_SCALE_HEIGHT = 8000.0f;

	constexpr float fMIE_SCATTER = 3.996e-6f;
	constexpr float fMIE_ABSORB = 4.4e-6f;
	constexpr float fMIE_SCALE_HEIGHT = 1200.0f;
	constexpr float fMIE_G = 0.76f;

	constexpr float fSUN_ANGULAR_RADIUS = 0.00935f;
	constexpr float fSUN_INTENSITY = 20.0f;

	constexpr u_int uDEFAULT_SKY_SAMPLES = 16;
	constexpr u_int uDEFAULT_LIGHT_SAMPLES = 8;

	constexpr u_int uTRANSMITTANCE_LUT_WIDTH = 256;
	constexpr u_int uTRANSMITTANCE_LUT_HEIGHT = 64;

	constexpr u_int uAERIAL_VOLUME_WIDTH = 32;
	constexpr u_int uAERIAL_VOLUME_HEIGHT = 32;
	constexpr u_int uAERIAL_VOLUME_DEPTH = 32;
	constexpr float fAERIAL_MAX_DISTANCE = 128000.0f;
}

enum Skybox_DebugMode : u_int
{
	SKYBOX_DEBUG_NONE,
	SKYBOX_DEBUG_RAYLEIGH_ONLY,
	SKYBOX_DEBUG_MIE_ONLY,
	SKYBOX_DEBUG_TRANSMITTANCE,
	SKYBOX_DEBUG_SCATTER_DIRECTION,
	SKYBOX_DEBUG_AERIAL_DEPTH,
	SKYBOX_DEBUG_SUN_DISK,
	SKYBOX_DEBUG_LUT_PREVIEW,
	SKYBOX_DEBUG_RAY_STEPS,
	SKYBOX_DEBUG_PHASE_FUNCTION,
	SKYBOX_DEBUG_COUNT
};

// Phase 9: state + behaviour for Skybox subsystem.
class Flux_SkyboxImpl
{
public:
	Flux_SkyboxImpl() = default;
	~Flux_SkyboxImpl() = default;

	Flux_SkyboxImpl(const Flux_SkyboxImpl&) = delete;
	Flux_SkyboxImpl& operator=(const Flux_SkyboxImpl&) = delete;

	void Initialise();
	void ReleaseAssetReferences();
	void Shutdown();
	void Reset();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void SetupAerialPerspectiveRenderGraph(Flux_RenderGraph& xGraph);

	void SetSunIntensity(float fIntensity)              { m_fSunIntensity = fIntensity; }
	void SetRayleighScale(float fScale)                 { m_fRayleighScale = fScale; }
	void SetMieScale(float fScale)                      { m_fMieScale = fScale; }
	void SetMieG(float fG)                              { m_fMieG = fG; }
	void SetAerialPerspectiveStrength(float fStrength)  { m_fAerialPerspectiveStrength = fStrength; }

	bool IsAtmosphereEnabled() const;
	float GetSunIntensity() const                       { return m_fSunIntensity; }
	float GetRayleighScale() const                      { return m_fRayleighScale; }
	float GetMieScale() const                           { return m_fMieScale; }
	float GetMieG() const                               { return m_fMieG; }
	bool IsAerialPerspectiveEnabled() const;
	float GetAerialPerspectiveStrength() const          { return m_fAerialPerspectiveStrength; }

	Flux_ShaderResourceView& GetTransmittanceLUTSRV();

#ifdef ZENITH_DEBUG_VARIABLES
	void RegisterDebugVariables();
#endif

	void CreateRenderTargets();
	void DestroyRenderTargets();

	TextureHandle              m_xCubemapTexture;

	Flux_RenderAttachment      m_xTransmittanceLUT;
	bool                       m_bLUTNeedsUpdate = true;

	Flux_Pipeline              m_xCubemapPipeline;
	Flux_Pipeline              m_xAtmospherePipeline;
	Flux_Pipeline              m_xAerialPerspectivePipeline;
	Flux_Pipeline              m_xSolidColourPipeline;

	Flux_Shader                m_xCubemapShader;
	Flux_Shader                m_xAtmosphereShader;
	Flux_Shader                m_xAerialPerspectiveShader;
	Flux_Shader                m_xSolidColourShader;

	Flux_DynamicConstantBuffer m_xAtmosphereConstantsBuffer;
	Flux_DynamicConstantBuffer m_xSolidColourConstantsBuffer;

	float                      m_fSunIntensity              = AtmosphereConfig::fSUN_INTENSITY;
	float                      m_fRayleighScale             = 1.0f;
	float                      m_fMieScale                  = 1.0f;
	float                      m_fMieG                      = AtmosphereConfig::fMIE_G;
	float                      m_fAerialPerspectiveStrength = 1.0f;
};
