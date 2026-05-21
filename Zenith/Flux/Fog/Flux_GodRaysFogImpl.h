#pragma once

#include "Flux/Fog/Flux_GodRaysFog.h"
#include "Flux/Flux.h"

// Phase 7c: per-Engine state for GodRaysFog subsystem.
class Flux_GodRaysFogImpl
{
public:
	Flux_GodRaysFogImpl() = default;
	~Flux_GodRaysFogImpl() = default;

	Flux_GodRaysFogImpl(const Flux_GodRaysFogImpl&) = delete;
	Flux_GodRaysFogImpl& operator=(const Flux_GodRaysFogImpl&) = delete;

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	// Per-frame push constants stay file-static in the .cpp -- their
	// struct type Flux_GodRaysConstants is locally defined there.
};
