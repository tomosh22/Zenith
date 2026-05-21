#pragma once

#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_RenderTargets.h"
#include "AssetHandling/Zenith_AssetHandle.h"

// Phase 7g: per-Engine state for Skybox subsystem.
class Flux_SkyboxImpl
{
public:
	Flux_SkyboxImpl() = default;
	~Flux_SkyboxImpl() = default;

	Flux_SkyboxImpl(const Flux_SkyboxImpl&) = delete;
	Flux_SkyboxImpl& operator=(const Flux_SkyboxImpl&) = delete;

	// Ref-counted copy of g_xEngine.FluxGraphics().m_xCubemapTexture (set during init).
	// Owned by handle so the cubemap survives any UnloadUnused calls during the frame.
	TextureHandle              m_xCubemapTexture;

	// Transmittance LUT (precomputed for atmosphere).
	Flux_RenderAttachment      m_xTransmittanceLUT;
	bool                       m_bLUTNeedsUpdate = true;

	// Pipelines.
	Flux_Pipeline              m_xCubemapPipeline;
	Flux_Pipeline              m_xAtmospherePipeline;
	Flux_Pipeline              m_xAerialPerspectivePipeline;
	Flux_Pipeline              m_xSolidColourPipeline;

	// Shaders.
	Flux_Shader                m_xCubemapShader;
	Flux_Shader                m_xAtmosphereShader;
	Flux_Shader                m_xAerialPerspectiveShader;
	Flux_Shader                m_xSolidColourShader;

	// Constant buffers.
	Flux_DynamicConstantBuffer m_xAtmosphereConstantsBuffer;
	Flux_DynamicConstantBuffer m_xSolidColourConstantsBuffer;

	// Atmosphere configuration state (continuous parameters).
	float                      m_fSunIntensity              = AtmosphereConfig::fSUN_INTENSITY;
	float                      m_fRayleighScale             = 1.0f;
	float                      m_fMieScale                  = 1.0f;
	float                      m_fMieG                      = AtmosphereConfig::fMIE_G;
	float                      m_fAerialPerspectiveStrength = 1.0f;
};
