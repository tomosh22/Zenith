#pragma once

#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Phase 7h: per-Engine state for HDR subsystem.
class Flux_HDRImpl
{
public:
	Flux_HDRImpl() = default;
	~Flux_HDRImpl() = default;

	Flux_HDRImpl(const Flux_HDRImpl&) = delete;
	Flux_HDRImpl& operator=(const Flux_HDRImpl&) = delete;

	// HDR scene target (transient).
	Flux_TransientHandle      m_xHDRSceneTargetHandle;

	// Bloom transient chain (5 mips).
	Flux_TransientHandle      m_axBloomChainHandles[5];

	// Cached graph pointer (used by debug SRV resolvers).
	Flux_RenderGraph*         m_pxGraph = nullptr;

	// Auto-exposure prev-state tracking for state change detection.
	bool                      m_bAutoExposureWasEnabled = false;

	// Pipelines.
	Flux_Pipeline             m_xToneMappingPipeline;
	Flux_Pipeline             m_xBloomDownsamplePipeline;
	Flux_Pipeline             m_xBloomUpsamplePipeline;
	Flux_Pipeline             m_xBloomThresholdPipeline;
	Flux_Pipeline             m_xLuminanceHistogramPipeline;
	Flux_Pipeline             m_xAdaptationPipeline;

	// Shaders.
	Flux_Shader               m_xToneMappingShader;
	Flux_Shader               m_xBloomThresholdShader;
	Flux_Shader               m_xBloomDownsampleShader;
	Flux_Shader               m_xBloomUpsampleShader;
	Flux_Shader               m_xLuminanceHistogramShader;
	Flux_Shader               m_xAdaptationShader;

	// Root sigs (compute).
	Flux_RootSig              m_xLuminanceRootSig;
	Flux_RootSig              m_xAdaptationRootSig;

	// Auto-exposure compute buffers.
	Flux_ReadWriteBuffer      m_xHistogramBuffer;
	Flux_ReadWriteBuffer      m_xExposureBuffer;

	// Manual exposure / tone mapping.
	float                     m_fExposure          = 1.0f;
	float                     m_fBloomIntensity    = 0.5f;
	float                     m_fBloomThreshold    = 1.0f;
	ToneMappingOperator       m_eToneMappingOperator = TONEMAPPING_ACES;

	// Auto-exposure state.
	float                     m_fCurrentExposure   = 1.0f;
	float                     m_fAverageLuminance  = 0.18f;
	float                     m_fAdaptationSpeed   = 2.0f;
	float                     m_fTargetLuminance   = 0.18f;
	float                     m_fMinExposure       = 0.1f;
	float                     m_fMaxExposure       = 10.0f;
	float                     m_fMinLogLuminance   = -10.0f;
	float                     m_fLogLuminanceRange = 12.0f;
};
