#pragma once

#include "Zenith_PlatformGraphics_Include.h"
#include "Flux/Flux_Enums.h"

class Flux
{
public:
	Flux() = delete;
	~Flux() = delete;
	static void EarlyInitialise();
	static void LateInitialise();

	static const uint32_t GetFrameCounter() { return s_uFrameCounter; }

	static void AddResChangeCallback(void(*pfnCallback)()) { s_xResChangeCallbacks.push_back(pfnCallback); }
	static void OnResChange();
private:
	static uint32_t s_uFrameCounter;
	static std::vector<void(*)()> s_xResChangeCallbacks;
};

struct Flux_PipelineSpecification
{
	Flux_PipelineSpecification() = default;

	Flux_Shader* m_pxShader;

	Flux_BlendState m_axBlendStates[FLUX_MAX_TARGETS];

	bool m_bDepthTestEnabled = true;
	bool m_bDepthWriteEnabled = true;
	DepthCompareFunc m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_ALWAYS;
	DepthStencilFormat m_eDepthStencilFormat;
	bool m_bUsePushConstants = true;
	bool m_bUseTesselation = false;

	Flux_PipelineLayout m_xPipelineLayout;

	Flux_VertexInputDescription m_xVertexInputDesc;

	struct Flux_TargetSetup* m_pxTargetSetup;
	LoadAction m_eColourLoadAction;
	StoreAction m_eColourStoreAction;
	LoadAction m_eDepthStencilLoadAction;
	StoreAction m_eDepthStencilStoreAction;
	bool m_bWireframe = false;
};