#pragma once

#include "Flux/Flux.h"
#include "Maths/Zenith_Maths.h"

class Flux_CommandList;
class Flux_VolumeFogImpl;
class FrameContext;
class Flux_RendererImpl;
class Flux_GraphicsImpl;
class Flux_ShadowsImpl;

// Ray march specific parameters
struct Flux_RaymarchConstants
{
	Zenith_Maths::Vector4 m_xFogColour;        // RGB = fog color, A = unused
	Zenith_Maths::Vector4 m_xFogParams;        // x = density, y = scattering, z = absorption, w = max distance
	Zenith_Maths::Vector4 m_xNoiseParams;      // x = scale, y = speed, z = detail, w = time
	Zenith_Maths::Vector4 m_xHeightParams;     // x = base height, y = falloff, z = unused, w = unused
	u_int m_uNumSteps;
	u_int m_uDebugMode;
	u_int m_uFrameIndex;
	float m_fPhaseG;                           // Henyey-Greenstein asymmetry: -1=back, 0=isotropic, 0.6=forward
	float m_fVolShadowBias;                    // Shadow bias for volumetric samples (matches Froxel fog)
	float m_fVolShadowConeRadius;              // Cone spread radius in shadow space (matches Froxel fog)
	float m_fAmbientIrradianceRatio;           // Sky/sun light ratio for ambient fog contribution
	float m_fNoiseWorldScale;                  // World-to-texture coordinate scale for noise sampling
};

// Phase 9: state + behaviour for RaymarchFog subsystem.
class Flux_RaymarchFogImpl
{
public:
	Flux_RaymarchFogImpl() = default;
	~Flux_RaymarchFogImpl() = default;

	Flux_RaymarchFogImpl(const Flux_RaymarchFogImpl&) = delete;
	Flux_RaymarchFogImpl& operator=(const Flux_RaymarchFogImpl&) = delete;

	void Initialise(Flux_VolumeFogImpl& xVolumeFog, FrameContext& xFrame, Flux_RendererImpl& xFluxRenderer, Flux_GraphicsImpl& xFluxGraphics, Flux_ShadowsImpl& xShadows);
	void BuildPipelines();
	void Reset();
	void Render(Flux_CommandList* pxCommandList);

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;

private:
	Flux_VolumeFogImpl* m_pxVolumeFog = nullptr;
	FrameContext* m_pxFrame = nullptr;
	Flux_RendererImpl* m_pxFluxRenderer = nullptr;
	Flux_GraphicsImpl* m_pxFluxGraphics = nullptr;
	Flux_ShadowsImpl* m_pxShadows = nullptr;

	// Cached constants for push constant (per-frame transient).
	Flux_RaymarchConstants m_xConstants;
};
