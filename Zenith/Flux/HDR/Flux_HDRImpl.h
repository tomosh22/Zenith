#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

enum ToneMappingOperator : u_int
{
	TONEMAPPING_ACES,
	TONEMAPPING_ACES_FITTED,
	TONEMAPPING_REINHARD,
	TONEMAPPING_UNCHARTED2,
	TONEMAPPING_NEUTRAL,
	TONEMAPPING_AGX,        // modern filmic default (matches shader case 5)
	TONEMAPPING_COUNT
};

enum HDR_DebugMode : u_int
{
	HDR_DEBUG_NONE,
	HDR_DEBUG_LUMINANCE_HEAT,
	HDR_DEBUG_HISTOGRAM_OVERLAY,
	HDR_DEBUG_EXPOSURE_METER,
	HDR_DEBUG_BLOOM_ONLY,
	HDR_DEBUG_BLOOM_MIPS,
	HDR_DEBUG_PRE_TONEMAP,
	HDR_DEBUG_CLIPPING,
	HDR_DEBUG_EV_ZONES,
	HDR_DEBUG_TONEMAP_PASS_TEST,
	HDR_DEBUG_RAW_HDR_TEXTURE,
	HDR_DEBUG_COUNT
};

// Phase 9: state + behaviour for HDR subsystem.
class Flux_HDRImpl
{
public:
	Flux_HDRImpl() = default;
	~Flux_HDRImpl() = default;

	Flux_HDRImpl(const Flux_HDRImpl&) = delete;
	Flux_HDRImpl& operator=(const Flux_HDRImpl&) = delete;

	void Initialise();
	void Shutdown();
	void Reset();
	void BuildPipelines();

	// HDR no longer owns the scene target — Flux_Graphics does (it's a shared target
	// created before the first writer). HDR reads it via g_xEngine.FluxGraphics()
	// .GetHDRSceneTarget() and owns only its private per-view bloom chains (below).
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Create one view's 5-mip bloom-chain transients (half the view's dims at the
	// base) + its threshold/downsample/upsample pass chain. Slot 0 keeps the
	// historical pass names; the preview slot uses " (Preview)" static name
	// tables. Mirrors Flux_HiZImpl::SetupViewPasses.
	void SetupBloomViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight);

	void SetToneMappingOperator(ToneMappingOperator eOperator);
	void SetExposure(float fExposure);
	void SetBloomIntensity(float fIntensity);
	void SetBloomThreshold(float fThreshold);

	float GetCurrentExposure();
	float GetAverageLuminance();
	bool IsEnabled();

	Flux_RenderAttachment& GetBloomChainAttachment(u_int uIndex, u_int uViewSlot = kuFluxViewSlotMain);

	void SetAdaptationSpeed(float fSpeed);
	void SetTargetLuminance(float fLuminance);
	void SetExposureRange(float fMin, float fMax);

	bool IsAutoExposureEnabled();
	float GetAdaptationSpeed();
	float GetTargetLuminance();

#ifdef ZENITH_TOOLS
	void RegisterDebugVariables();

	const Flux_ShaderResourceView* GetDebugSRV_Bloom0();
	const Flux_ShaderResourceView* GetDebugSRV_Bloom1();
	const Flux_ShaderResourceView* GetDebugSRV_Bloom2();
#endif

	void SyncDebugVariables();

	// Bloom chain length (base + 4 downsampled mips).
	static constexpr u_int    uHDR_BLOOM_MIP_COUNT = 5u;

	// Per-view bloom chains: [view slot][mip]. Slot 0 (main) is created every
	// SetupRenderGraph at half swapchain dims; the preview slot only while the
	// preview view is active (half of 512 = 256 base). Other slots are unused.
	Flux_TransientHandle      m_aaxBloomChainHandles[FLUX_MAX_RENDER_VIEWS][uHDR_BLOOM_MIP_COUNT];
	Flux_RenderGraph*         m_pxGraph = nullptr;
	bool                      m_bAutoExposureWasEnabled = false;

	// Strongly-typed per-pass user data fed to AddPass(... , void*): each bloom
	// downsample / upsample pass gets &m_axBloom*UserData[i]. Stored here (rather
	// than module-scope statics) so the pointers stay stable for the graph's
	// lifetime — the Impl is engine-owned and outlives the graph. The Execute*
	// trampolines recover the MIP index purely via pUserData and derive the VIEW
	// from the recording pass's slot (mirrors the HiZ per-view conversion), so
	// both view instances of a pass share the same per-mip payload.
	u_int                     m_axBloomMipUserData[uHDR_BLOOM_MIP_COUNT]          = { 0, 1, 2, 3, 4 };
	u_int                     m_axBloomUpsampleUserData[uHDR_BLOOM_MIP_COUNT - 1] = { 0, 1, 2, 3 };

	Flux_Pipeline             m_xToneMappingPipeline;
	Flux_Pipeline             m_xBloomDownsamplePipeline;
	Flux_Pipeline             m_xBloomUpsamplePipeline;
	Flux_Pipeline             m_xBloomThresholdPipeline;
	Flux_Pipeline             m_xLuminanceHistogramPipeline;
	Flux_Pipeline             m_xAdaptationPipeline;

	Flux_Shader               m_xToneMappingShader;
	Flux_Shader               m_xBloomThresholdShader;
	Flux_Shader               m_xBloomDownsampleShader;
	Flux_Shader               m_xBloomUpsampleShader;
	Flux_Shader               m_xLuminanceHistogramShader;
	Flux_Shader               m_xAdaptationShader;

	Flux_RootSig              m_xLuminanceRootSig;
	Flux_RootSig              m_xAdaptationRootSig;

	Flux_ReadWriteBuffer      m_xHistogramBuffer;
	Flux_ReadWriteBuffer      m_xExposureBuffer;

	float                     m_fExposure          = 1.0f;
	float                     m_fBloomIntensity    = 0.5f;
	float                     m_fBloomThreshold    = 1.0f;
	ToneMappingOperator       m_eToneMappingOperator = TONEMAPPING_AGX;

	float                     m_fCurrentExposure   = 1.0f;
	float                     m_fAverageLuminance  = 0.18f;
	float                     m_fAdaptationSpeed   = 2.0f;
	float                     m_fTargetLuminance   = 0.18f;
	float                     m_fMinExposure       = 0.1f;
	float                     m_fMaxExposure       = 10.0f;
	float                     m_fMinLogLuminance   = -10.0f;
	float                     m_fLogLuminanceRange = 12.0f;
};
