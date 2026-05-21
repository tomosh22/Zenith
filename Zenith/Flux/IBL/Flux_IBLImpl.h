#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

enum IBL_DebugMode : u_int
{
	IBL_DEBUG_NONE,
	IBL_DEBUG_IRRADIANCE_MAP,
	IBL_DEBUG_PREFILTERED_MIPS,
	IBL_DEBUG_BRDF_LUT,
	IBL_DEBUG_DIFFUSE_ONLY,
	IBL_DEBUG_SPECULAR_ONLY,
	IBL_DEBUG_FRESNEL,
	IBL_DEBUG_REFLECTION_VECTOR,
	IBL_DEBUG_PROBE_VOLUMES,
	IBL_DEBUG_PROBE_CAPTURE,
	IBL_DEBUG_ROUGHNESS_LOD,
	IBL_DEBUG_COUNT
};

// IBL regeneration state machine for frame-amortized updates.
enum IBL_RegenState : u_int
{
	IBL_REGEN_IDLE,
	IBL_REGEN_IRRADIANCE,
	IBL_REGEN_PREFILTER
};

namespace IBLConfig
{
	constexpr u_int uBRDF_LUT_SIZE = 512;
	constexpr u_int uIRRADIANCE_SIZE = 32;
	constexpr u_int uPREFILTER_SIZE = 128;
	constexpr u_int uPREFILTER_MIP_COUNT = 7;
	constexpr u_int uMAX_PROBES = 16;
	constexpr u_int uPASSES_PER_FRAME = 8;
}

// Phase 9: state + behaviour for IBL subsystem.
class Flux_IBLImpl
{
public:
	Flux_IBLImpl() = default;
	~Flux_IBLImpl() = default;

	Flux_IBLImpl(const Flux_IBLImpl&) = delete;
	Flux_IBLImpl& operator=(const Flux_IBLImpl&) = delete;

	void Initialise();
	void Shutdown();
	void Reset();
	void BuildPipelines();

	void GenerateBRDFLUT();
	void UpdateSkyIBL();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void UpdateGraphPassEnables(Flux_RenderGraph& xGraph);

	void MarkAllProbesDirty();

	const Flux_ShaderResourceView& GetBRDFLUTSRV();
	const Flux_ShaderResourceView& GetIrradianceMapSRV();
	const Flux_ShaderResourceView& GetPrefilteredMapSRV();

	void SetIntensity(float fIntensity) { m_fIntensity = fIntensity; }

	bool IsEnabled() const;
	bool IsReady() const { return m_bIBLReady; }
	float GetIntensity() const { return m_fIntensity; }
	bool IsDiffuseEnabled() const;
	bool IsSpecularEnabled() const;
	bool IsShowBRDFLUT() const;
	bool IsForceRoughness() const;
	float GetForcedRoughness() const;

#ifdef ZENITH_TOOLS
	void RegisterDebugVariables();
#endif

	// Render-graph execute callbacks -- stay static so they satisfy
	// Flux_RenderGraph_OnRecordFunc (void(*)(Flux_CommandList*, void*)).
	// They reach engine state via g_xEngine.IBL() at call time.
	static void ExecuteBRDFLUTPass(Flux_CommandList* pxCmd, void* pUserData);
	static void ExecuteIrradianceFacePass(Flux_CommandList* pxCmd, void* pUserData);
	static void ExecutePrefilterMipFacePass(Flux_CommandList* pxCmd, void* pUserData);

private:
	void CreateRenderTargets();
	void DestroyRenderTargets();

	void GenerateIrradianceMap();
	void GeneratePrefilteredMap();
	void GenerateIrradianceFace(u_int uFace);
	void GeneratePrefilteredFace(u_int uMip, u_int uFace);

	void ResetIBLRegenStateForRecompile();
	bool ResolveBRDFLUTRun();
	void RunFirstGenerationFrame(bool (&abRunIrradiance)[6],
		bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6]);
	void AdvanceAmortizedRegen(bool (&abRunIrradiance)[6],
		bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6]);
	void ApplyResolvedIBLEnables(Flux_RenderGraph& xGraph,
		bool bRunBRDF,
		const bool (&abRunIrradiance)[6],
		const bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6]);

public:
	// State flags.
	bool                       m_bBRDFLUTGenerated = false;
	bool                       m_bSkyIBLDirty      = true;
	bool                       m_bIBLReady         = false;
	bool                       m_bFirstGeneration  = true;

	// Render attachments.
	Flux_RenderAttachment      m_xBRDFLUT;
	Flux_RenderAttachmentCube  m_xIrradianceMap;
	Flux_RenderAttachmentCube  m_xPrefilteredMap;

	// Pipelines + shaders.
	Flux_Pipeline              m_xBRDFLUTPipeline;
	Flux_Pipeline              m_xIrradianceConvolvePipeline;
	Flux_Pipeline              m_xPrefilterPipeline;
	Flux_Shader                m_xBRDFLUTShader;
	Flux_Shader                m_xIrradianceConvolveShader;
	Flux_Shader                m_xPrefilterShader;

	// Configuration.
	float                      m_fIntensity = 1.0f;

	// Regen state machine.
	IBL_RegenState             m_eRegenState = IBL_REGEN_IDLE;
	u_int                      m_uRegenFace  = 0;
	u_int                      m_uRegenMip   = 0;

	// Pass handles.
	Flux_PassHandle            m_xBRDFLUTPassHandle = {};
	Flux_PassHandle            m_axIrradianceFacePassHandles[6] = {};
	Flux_PassHandle            m_axPrefilterMipFacePassHandles[IBLConfig::uPREFILTER_MIP_COUNT][6] = {};
};
