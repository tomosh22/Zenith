#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#define ZENITH_FLUX_NUM_CSMS 4
#define ZENITH_FLUX_CSM_RESOLUTION 2048

// CSM depth format — exposed here so subsystems that build shadow pipelines at
// Initialise() time can reference it without going through a graph-owned
// transient accessor (which requires the graph to exist).
static constexpr TextureFormat CSM_FORMAT = TEXTURE_FORMAT_D32_SFLOAT;

// Phase 9: state + behaviour for shadows subsystem.
class Flux_ShadowsImpl
{
public:
	Flux_ShadowsImpl() = default;
	~Flux_ShadowsImpl() = default;

	Flux_ShadowsImpl(const Flux_ShadowsImpl&) = delete;
	Flux_ShadowsImpl& operator=(const Flux_ShadowsImpl&) = delete;

	void Initialise();
	void Shutdown();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_RenderAttachment* GetCSMTargetSetup(const uint32_t uIndex, uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);
	Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex) { return m_axSunViewProjMats[uIndex]; }
	Flux_ShaderResourceView& GetCSMSRV(const uint32_t u);
	Flux_DynamicConstantBuffer& GetShadowMatrixBuffer(const uint32_t u) { return m_xShadowMatrixBuffers[u]; }

	void UpdateShadowMatrices();

	// CSM cascade split distances (m).
	static constexpr float s_afCSMLevels[ZENITH_FLUX_NUM_CSMS + 1]{ 2000, 50, 10, 5, 1 };

	Flux_TransientHandle       m_axCSMHandles[ZENITH_FLUX_NUM_CSMS];
	Flux_RenderGraph*          m_pxGraph = nullptr;
	Zenith_Maths::Matrix4      m_axShadowMatrices[ZENITH_FLUX_NUM_CSMS];
	Flux_DynamicConstantBuffer m_xShadowMatrixBuffers[ZENITH_FLUX_NUM_CSMS];
	Zenith_Maths::Matrix4      m_axSunViewProjMats[ZENITH_FLUX_NUM_CSMS];
};
