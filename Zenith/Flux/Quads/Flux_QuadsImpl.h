#pragma once

#include "Flux/Quads/Flux_Quads.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

// Phase 7b: per-Engine state for Quads subsystem -- shader/pipeline/
// instance buffer + per-frame quad upload ring.
class Flux_QuadsImpl
{
public:
	Flux_QuadsImpl() = default;
	~Flux_QuadsImpl() = default;

	Flux_QuadsImpl(const Flux_QuadsImpl&) = delete;
	Flux_QuadsImpl& operator=(const Flux_QuadsImpl&) = delete;

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	Flux_DynamicVertexBuffer m_xInstanceBuffer;

	Flux_Quads::Quad         m_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME];
	uint32_t                 m_uQuadRenderIndex = 0;
};
