#pragma once

#include "Flux/Fog/Flux_RaymarchFog.h"
#include "Flux/Flux.h"

// Phase 7c: per-Engine state for RaymarchFog subsystem.
class Flux_RaymarchFogImpl
{
public:
	Flux_RaymarchFogImpl() = default;
	~Flux_RaymarchFogImpl() = default;

	Flux_RaymarchFogImpl(const Flux_RaymarchFogImpl&) = delete;
	Flux_RaymarchFogImpl& operator=(const Flux_RaymarchFogImpl&) = delete;

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	// Per-frame push constants stay file-static in the .cpp -- their
	// struct type Flux_RaymarchConstants is locally defined there.
};
