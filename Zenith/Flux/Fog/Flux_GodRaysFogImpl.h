#pragma once

#include "Flux/Flux.h"

class Flux_CommandList;

// Phase 9: state + behaviour for GodRaysFog subsystem.
class Flux_GodRaysFogImpl
{
public:
	Flux_GodRaysFogImpl() = default;
	~Flux_GodRaysFogImpl() = default;

	Flux_GodRaysFogImpl(const Flux_GodRaysFogImpl&) = delete;
	Flux_GodRaysFogImpl& operator=(const Flux_GodRaysFogImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Reset();
	void Render(Flux_CommandList* pxCommandList);

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;
};
