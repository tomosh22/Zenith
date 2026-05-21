#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

// Phase 7b: per-Engine state for SDFs subsystem.
class Flux_SDFsImpl
{
public:
	Flux_SDFsImpl() = default;
	~Flux_SDFsImpl() = default;

	Flux_SDFsImpl(const Flux_SDFsImpl&) = delete;
	Flux_SDFsImpl& operator=(const Flux_SDFsImpl&) = delete;

	Flux_Shader                m_xShader;
	Flux_Pipeline              m_xPipeline;
	Flux_DynamicConstantBuffer m_xSpheresBuffer;
};
