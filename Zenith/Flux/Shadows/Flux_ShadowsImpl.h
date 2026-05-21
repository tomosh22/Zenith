#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Phase 7c: per-Engine state for Shadows subsystem.
class Flux_ShadowsImpl
{
public:
	Flux_ShadowsImpl() = default;
	~Flux_ShadowsImpl() = default;

	Flux_ShadowsImpl(const Flux_ShadowsImpl&) = delete;
	Flux_ShadowsImpl& operator=(const Flux_ShadowsImpl&) = delete;

	Flux_TransientHandle       m_axCSMHandles[ZENITH_FLUX_NUM_CSMS];
	Flux_RenderGraph*          m_pxGraph = nullptr;
	Zenith_Maths::Matrix4      m_axShadowMatrices[ZENITH_FLUX_NUM_CSMS];
	Flux_DynamicConstantBuffer m_xShadowMatrixBuffers[ZENITH_FLUX_NUM_CSMS];
	Zenith_Maths::Matrix4      m_axSunViewProjMats[ZENITH_FLUX_NUM_CSMS];
};
