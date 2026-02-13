#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"

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
	HDR_DEBUG_TONEMAP_PASS_TEST,    // Mode 9: Output magenta to verify tone mapping runs
	HDR_DEBUG_RAW_HDR_TEXTURE,      // Mode 10: Output raw HDR texture values (clamped)
	HDR_DEBUG_COUNT
};

class Flux_HDR
{
public:
	Flux_HDR() = delete;
	~Flux_HDR() = delete;

	static void Initialise();
	static void Shutdown();
	static void Reset();

	static void Render(void*);
	static void SubmitRenderTask();
	static void WaitForRenderTask();

	static Flux_ShaderResourceView& GetHDRSceneSRV();
	static Flux_RenderAttachment& GetHDRSceneTarget();
	static Flux_TargetSetup& GetHDRSceneTargetSetup();
	static Flux_TargetSetup& GetHDRSceneTargetSetupWithDepth();  // For passes that need depth testing

	static void SetToneMappingOperator(ToneMappingOperator eOperator);
	static void SetExposure(float fExposure);
	static void SetBloomEnabled(bool bEnabled);
	static void SetBloomIntensity(float fIntensity);
	static void SetBloomThreshold(float fThreshold);

	static float GetCurrentExposure();
	static float GetAverageLuminance();
	static bool IsEnabled();

	// Auto-exposure control
	static void SetAutoExposureEnabled(bool bEnabled);
	static void SetAdaptationSpeed(float fSpeed);
	static void SetTargetLuminance(float fLuminance);
	static void SetExposureRange(float fMin, float fMax);

	static bool IsAutoExposureEnabled();
	static float GetAdaptationSpeed();
	static float GetTargetLuminance();

#ifdef ZENITH_TOOLS
	static void RegisterDebugVariables();
#endif

private:
	static void SyncDebugVariables();
	static void InitializeAutoExposure();
	static void RenderToneMapping();
	static void RenderBloom();
	static void ComputeLuminanceHistogram();
	static void ComputeExposureAdaptation();

	static void CreateRenderTargets();
	static void DestroyRenderTargets();

	static Flux_RenderAttachment s_xHDRSceneTarget;
	static Flux_TargetSetup s_xHDRSceneTargetSetup;
	static Flux_TargetSetup s_xHDRSceneTargetSetupWithDepth;

	static Flux_RenderAttachment s_axBloomChain[5];
	static Flux_TargetSetup s_axBloomChainSetup[5];

	static Flux_Pipeline s_xToneMappingPipeline;
	static Flux_Pipeline s_xBloomDownsamplePipeline;
	static Flux_Pipeline s_xBloomUpsamplePipeline;
	static Flux_Pipeline s_xBloomThresholdPipeline;

	static float s_fExposure;
	static float s_fBloomIntensity;
	static float s_fBloomThreshold;
	static ToneMappingOperator s_eToneMappingOperator;
	static bool s_bBloomEnabled;
	static bool s_bAutoExposure;

	static float s_fCurrentExposure;
	static float s_fAverageLuminance;

	// Auto-exposure parameters
	static float s_fAdaptationSpeed;
	static float s_fTargetLuminance;
	static float s_fMinExposure;
	static float s_fMaxExposure;
	static float s_fMinLogLuminance;
	static float s_fLogLuminanceRange;

	// Auto-exposure compute resources
	static Flux_ReadWriteBuffer s_xHistogramBuffer;
	static Flux_ReadWriteBuffer s_xExposureBuffer;
	static Zenith_Vulkan_Pipeline s_xLuminanceHistogramPipeline;
	static Zenith_Vulkan_Pipeline s_xAdaptationPipeline;
	static Zenith_Vulkan_Shader s_xLuminanceHistogramShader;
	static Zenith_Vulkan_Shader s_xAdaptationShader;
	static Zenith_Vulkan_RootSig s_xLuminanceRootSig;
	static Zenith_Vulkan_RootSig s_xAdaptationRootSig;
};
