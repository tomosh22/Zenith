#pragma once

#include "Flux/Flux.h"

// Phase 7b: per-Engine state for deferred-shading subsystem.
class Flux_DeferredShadingImpl
{
public:
	Flux_DeferredShadingImpl() = default;
	~Flux_DeferredShadingImpl() = default;

	Flux_DeferredShadingImpl(const Flux_DeferredShadingImpl&) = delete;
	Flux_DeferredShadingImpl& operator=(const Flux_DeferredShadingImpl&) = delete;

	Flux_Shader   m_xShader;
	Flux_Pipeline m_xPipeline;
};
