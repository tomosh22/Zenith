#pragma once

#define ZENITH_FLUX_NUM_CSMS 4
#define ZENITH_FLUX_CSM_RESOLUTION 2048

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// CSM depth format — exposed here so subsystems that build shadow pipelines at
// Initialise() time can reference it without going through a graph-owned
// transient accessor (which requires the graph to exist).
static constexpr TextureFormat CSM_FORMAT = TEXTURE_FORMAT_D32_SFLOAT;

class Flux_Shadows
{
public:
	static void Initialise();
	static void Shutdown();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	static Flux_RenderAttachment* GetCSMTargetSetup(const uint32_t uIndex, uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);
	static Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex);
	static Flux_ShaderResourceView& GetCSMSRV(const uint32_t u);
	static Flux_DynamicConstantBuffer& GetShadowMatrixBuffer(const uint32_t u);

	static void UpdateShadowMatrices();

private:
	static constexpr float s_afCSMLevels[ZENITH_FLUX_NUM_CSMS + 1]{ 2000, 50, 10, 5, 1 };
};
