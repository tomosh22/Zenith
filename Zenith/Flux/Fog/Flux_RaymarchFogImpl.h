#pragma once

#include "Flux/Flux.h"

class Flux_CommandList;

// Phase 9: state + behaviour for RaymarchFog subsystem.
class Flux_RaymarchFogImpl
{
public:
	Flux_RaymarchFogImpl() = default;
	~Flux_RaymarchFogImpl() = default;

	Flux_RaymarchFogImpl(const Flux_RaymarchFogImpl&) = delete;
	Flux_RaymarchFogImpl& operator=(const Flux_RaymarchFogImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Reset();
	void Render(Flux_CommandList* pxCommandList);

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;
};
