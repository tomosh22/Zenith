#pragma once

#include "Flux/Flux.h"
#include "Maths/Zenith_Maths.h"

class Flux_CommandList;
class Flux_GraphicsImpl;

// God rays specific parameters
struct Flux_GodRaysConstants
{
	Zenith_Maths::Vector4 m_xLightScreenPos_Pad;  // xy = light screen pos (0-1), zw = unused
	Zenith_Maths::Vector4 m_xParams;              // x = decay, y = exposure, z = density, w = weight
	u_int m_uNumSamples;
	u_int m_uDebugMode;
	float m_fPad0;
	float m_fPad1;
};

// Phase 9: state + behaviour for GodRaysFog subsystem.
class Flux_GodRaysFogImpl
{
public:
	Flux_GodRaysFogImpl() = default;
	~Flux_GodRaysFogImpl() = default;

	Flux_GodRaysFogImpl(const Flux_GodRaysFogImpl&) = delete;
	Flux_GodRaysFogImpl& operator=(const Flux_GodRaysFogImpl&) = delete;

	void Initialise(Flux_GraphicsImpl& xFluxGraphics);
	void BuildPipelines();
	void Reset();
	void Render(Flux_CommandList* pxCommandList);

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;

private:
	Flux_GraphicsImpl* m_pxFluxGraphics = nullptr;

	// Cached constants for push constant (per-frame transient).
	Flux_GodRaysConstants m_xConstants = {};
};
