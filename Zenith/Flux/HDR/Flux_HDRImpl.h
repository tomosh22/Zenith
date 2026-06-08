#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Cross-subsystem dependencies injected into Initialise (aggressive
// de-globalization pass): the engine-infra singletons VulkanMemory / Frame /
// VulkanSwapchain and the sibling FluxGraphics are passed by reference and
// stored as member pointers, rather than reached via g_xEngine.X() inside the
// instance methods. Forward-declared here; full headers are pulled in by
// Flux_HDR.cpp. The only g_xEngine survivors are the non-capturing fn-pointer
// trampolines (the Execute*/PreExecute* graph callbacks + the hot-reload
// lambda) which recover this singleton via g_xEngine.HDR() and then route
// their reach-ins through these members, plus the ZENITH_TOOLS DebugVariables
// reaches (conditional accessor).
class Flux_GraphicsImpl;
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan_Swapchain;
class FrameContext;

enum ToneMappingOperator : u_int
{
	TONEMAPPING_ACES,
	TONEMAPPING_ACES_FITTED,
	TONEMAPPING_REINHARD,
	TONEMAPPING_UNCHARTED2,
	TONEMAPPING_NEUTRAL,
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

	// Cross-subsystem deps are injected here and stored into the member pointers
	// below. xVulkanMemory owns the auto-exposure VRAM buffer lifetime + staging
	// uploads; xSwapchain supplies the back-buffer dims for the histogram/
	// adaptation dispatch sizing and transient creation; xFrame supplies the
	// delta-time for exposure adaptation; xGraphics owns the fullscreen quad mesh
	// + the final/depth render targets the bloom/tonemap passes draw into.
	void Initialise(Flux_GraphicsImpl& xGraphics, Zenith_Vulkan_MemoryManager& xVulkanMemory,
	                Zenith_Vulkan_Swapchain& xSwapchain, FrameContext& xFrame);
	void Shutdown();
	void Reset();
	void BuildPipelines();

	void SetupTransients(Flux_RenderGraph& xGraph);
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_ShaderResourceView& GetHDRSceneSRV();
	Flux_RenderAttachment&   GetHDRSceneTarget();
	void GetHDRSceneTargetSetup(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);
	void GetHDRSceneTargetSetupWithDepth(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);

	void SetToneMappingOperator(ToneMappingOperator eOperator);
	void SetExposure(float fExposure);
	void SetBloomIntensity(float fIntensity);
	void SetBloomThreshold(float fThreshold);

	float GetCurrentExposure();
	float GetAverageLuminance();
	bool IsEnabled();

	Flux_RenderAttachment& GetBloomChainAttachment(u_int uIndex);

	void SetAdaptationSpeed(float fSpeed);
	void SetTargetLuminance(float fLuminance);
	void SetExposureRange(float fMin, float fMax);

	bool IsAutoExposureEnabled();
	float GetAdaptationSpeed();
	float GetTargetLuminance();

#ifdef ZENITH_TOOLS
	void RegisterDebugVariables();

	const Flux_ShaderResourceView* GetDebugSRV_HDRScene();
	const Flux_ShaderResourceView* GetDebugSRV_Bloom0();
	const Flux_ShaderResourceView* GetDebugSRV_Bloom1();
	const Flux_ShaderResourceView* GetDebugSRV_Bloom2();
#endif

	void SyncDebugVariables();

	Flux_TransientHandle      m_xHDRSceneTargetHandle;
	Flux_TransientHandle      m_axBloomChainHandles[5];
	Flux_RenderGraph*         m_pxGraph = nullptr;
	bool                      m_bAutoExposureWasEnabled = false;

	// Strongly-typed per-pass user data fed to AddPass(... , void*): each bloom
	// downsample / upsample pass gets &m_axBloom*UserData[i]. Stored here (rather
	// than module-scope statics) so the pointers stay stable for the graph's
	// lifetime — the Impl is engine-owned and outlives the graph. The Execute*
	// trampolines recover the index purely via pUserData.
	u_int                     m_axBloomMipUserData[5]      = { 0, 1, 2, 3, 4 };
	u_int                     m_axBloomUpsampleUserData[4] = { 0, 1, 2, 3 };

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
	ToneMappingOperator       m_eToneMappingOperator = TONEMAPPING_ACES;

	float                     m_fCurrentExposure   = 1.0f;
	float                     m_fAverageLuminance  = 0.18f;
	float                     m_fAdaptationSpeed   = 2.0f;
	float                     m_fTargetLuminance   = 0.18f;
	float                     m_fMinExposure       = 0.1f;
	float                     m_fMaxExposure       = 10.0f;
	float                     m_fMinLogLuminance   = -10.0f;
	float                     m_fLogLuminanceRange = 12.0f;

	// Injected cross-subsystem deps (stored from Initialise; nulled in Shutdown).
	// Public so the non-capturing graph trampolines can route through them after
	// recovering this instance via g_xEngine.HDR().
	Flux_GraphicsImpl*           m_pxGraphics      = nullptr;
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory  = nullptr;
	Zenith_Vulkan_Swapchain*     m_pxSwapchain     = nullptr;
	FrameContext*                m_pxFrame         = nullptr;
};
