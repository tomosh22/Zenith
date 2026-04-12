#include "Zenith.h"

#include "Flux_IBL.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "AssetHandling/Zenith_TextureAsset.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Static member definitions
Flux_RenderAttachment Flux_IBL::s_xBRDFLUT;
Flux_TargetSetup Flux_IBL::s_xBRDFLUTSetup;
bool Flux_IBL::s_bBRDFLUTGenerated = false;

Flux_RenderAttachment Flux_IBL::s_xIrradianceMap;
Flux_TargetSetup Flux_IBL::s_axIrradianceFaceSetup[6];

Flux_RenderAttachment Flux_IBL::s_xPrefilteredMap;
Flux_TargetSetup Flux_IBL::s_axPrefilteredFaceSetup[6];

// Per-mip-per-face RTVs and target setups for prefiltered map
// Index as [mip][face]
static Flux_RenderTargetView s_axPrefilteredMipFaceRTVs[IBLConfig::uPREFILTER_MIP_COUNT][6];
static Flux_TargetSetup s_axPrefilteredMipFaceSetup[IBLConfig::uPREFILTER_MIP_COUNT][6];

// Configure a target setup to render to a single cubemap face
// Copies surface info from the source cubemap attachment, then overrides to 2D single-layer
// If uMipSize > 0, also overrides mip level and dimensions (for per-mip rendering)
static void ConfigureCubeFaceTarget(
	Flux_TargetSetup& xSetup,
	const Flux_RenderAttachment& xSourceMap,
	u_int uFace,
	const Flux_RenderTargetView& xRTV,
	u_int uMip = 0,
	u_int uMipSize = 0)
{
	Flux_RenderAttachment& xAttach = xSetup.m_axColourAttachments[0];
	xAttach.m_xSurfaceInfo = xSourceMap.m_xSurfaceInfo;
	xAttach.m_xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_2D;
	xAttach.m_xSurfaceInfo.m_uNumLayers = 1;
	xAttach.m_xSurfaceInfo.m_uBaseLayer = uFace;
	xAttach.m_xVRAMHandle = xSourceMap.m_xVRAMHandle;
	xAttach.m_pxRTV = xRTV;
	if (uMipSize > 0)
	{
		xAttach.m_xSurfaceInfo.m_uBaseMip = uMip;
		xAttach.m_xSurfaceInfo.m_uWidth = uMipSize;
		xAttach.m_xSurfaceInfo.m_uHeight = uMipSize;
	}
}

Flux_Pipeline Flux_IBL::s_xBRDFLUTPipeline;
Flux_Pipeline Flux_IBL::s_xIrradianceConvolvePipeline;
Flux_Pipeline Flux_IBL::s_xPrefilterPipeline;

Flux_Shader Flux_IBL::s_xBRDFLUTShader;
Flux_Shader Flux_IBL::s_xIrradianceConvolveShader;
Flux_Shader Flux_IBL::s_xPrefilterShader;

bool Flux_IBL::s_bEnabled = true;
float Flux_IBL::s_fIntensity = 1.0f;
bool Flux_IBL::s_bDiffuseEnabled = true;
bool Flux_IBL::s_bSpecularEnabled = true;
bool Flux_IBL::s_bSkyIBLDirty = true;
bool Flux_IBL::s_bIBLReady = false;  // Set true after BRDF LUT AND sky IBL are generated
bool Flux_IBL::s_bFirstGeneration = true;  // First generation must be non-amortized

// Frame-amortized regeneration state
IBL_RegenState Flux_IBL::s_eRegenState = IBL_REGEN_IDLE;
u_int Flux_IBL::s_uRegenFace = 0;
u_int Flux_IBL::s_uRegenMip = 0;

// Render-graph pass indices — populated by SetupRenderGraph, consumed every
// frame by UpdateGraphPassEnables.
u_int Flux_IBL::s_uBRDFLUTPassIdx = UINT32_MAX;
u_int Flux_IBL::s_auIrradianceFacePassIdx[6] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
u_int Flux_IBL::s_auPrefilterMipFacePassIdx[IBLConfig::uPREFILTER_MIP_COUNT][6] = {};

// Cached binding handles
static Flux_BindingHandle s_xBRDFLUTFrameConstantsBinding;
Flux_BindingHandle Flux_IBL::s_xIrradianceFrameConstantsBinding;
Flux_BindingHandle Flux_IBL::s_xPrefilterFrameConstantsBinding;
static Flux_BindingHandle s_xIrradianceSkyboxBinding;
static Flux_BindingHandle s_xPrefilterSkyboxBinding;

// Per-pass user data structs — small PODs holding the (mip, face) the pass
// targets. Pointer-stable file-static storage so the graph can hand them as
// void* without lifetime concerns.
struct IBLPrefilterPassData
{
	u_int m_uMip;
	u_int m_uFace;
};
static IBLPrefilterPassData s_axPrefilterPassData[IBLConfig::uPREFILTER_MIP_COUNT][6];
static u_int s_auIrradianceFaceData[6] = { 0, 1, 2, 3, 4, 5 };

DEBUGVAR bool dbg_bIBLShowBRDFLUT = false;
DEBUGVAR bool dbg_bIBLForceRoughness = false;
DEBUGVAR float dbg_fIBLForcedRoughness = 0.5f;
DEBUGVAR bool dbg_bIBLRegenerateBRDFLUT = false;

void Flux_IBL::Initialise()
{
	CreateRenderTargets();

	// Initialize BRDF LUT shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xBRDFLUTShader, s_xBRDFLUTPipeline,
		"IBL/Flux_BRDFIntegration.frag", &s_xBRDFLUTSetup);

	s_xBRDFLUTFrameConstantsBinding = s_xBRDFLUTShader.GetReflection().GetBinding("FrameConstants");

	// Initialize irradiance convolution shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xIrradianceConvolveShader, s_xIrradianceConvolvePipeline,
		"IBL/Flux_IrradianceConvolution.frag", &s_axIrradianceFaceSetup[0]);

	{
		const Flux_ShaderReflection& xReflection = s_xIrradianceConvolveShader.GetReflection();
		s_xIrradianceFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
		s_xIrradianceSkyboxBinding = xReflection.GetBinding("g_xSkyboxCubemap");
	}

	// Initialize prefilter shader and pipeline
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xPrefilterShader, s_xPrefilterPipeline,
		"IBL/Flux_PrefilterEnvMap.frag", &s_axPrefilteredFaceSetup[0]);

	{
		const Flux_ShaderReflection& xReflection = s_xPrefilterShader.GetReflection();
		s_xPrefilterFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
		s_xPrefilterSkyboxBinding = xReflection.GetBinding("g_xSkyboxCubemap");
	}

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	// BRDF LUT will be generated on first frame via render graph ExecuteIBLUpdate()
	// This ensures the render loop is active when the command list is submitted

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL Initialised");
}

void Flux_IBL::Shutdown()
{
	DestroyRenderTargets();
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL shut down");
}

void Flux_IBL::Reset()
{
	s_bSkyIBLDirty = true;
	s_bIBLReady = false;  // Need to regenerate IBL on next frame
	s_bFirstGeneration = true;  // Force non-amortized generation after reset

	// Reset amortized regeneration state
	s_eRegenState = IBL_REGEN_IDLE;
	s_uRegenFace = 0;
	s_uRegenMip = 0;
}

// ---------------------------------------------------------------------------
// First-class IBL graph passes
//
// One graph pass per output subresource (1 BRDF LUT + 6 irradiance faces + 42
// prefilter mip-face combinations = 49 passes). UpdateGraphPassEnables (called
// from Zenith_Core::ExecuteRenderGraph BEFORE Compile) advances the amortised
// state machine and toggles per-pass enable bits via SetPassEnabled. Disabled
// passes are excluded from Phase 0 (OnPrepare), Phase 1 (record), and Phase 2
// (submit) — so their loadOp=CLEAR never fires and their previously-rendered
// contents are preserved across idle frames.
//
// The state machine MUST run as a free function (not as a pass OnPrepare),
// because Phase 0 only invokes OnPrepare for *enabled* passes — once everything
// is disabled the state machine could never re-enable anything.
// ---------------------------------------------------------------------------

void Flux_IBL::UpdateGraphPassEnables(Flux_RenderGraph& xGraph)
{
	// If a full recompile is pending (e.g. fog technique was just switched, the
	// window was resized, the graph was rebuilt, or any other system called
	// MarkDirty), the validator will run inside Compile() and require every
	// read of an IBL texture (BRDF LUT, irradiance, prefiltered) to have at
	// least one *enabled* writer in the graph. Without this, the per-frame
	// amortised state machine below would have all 49 IBL passes disabled in
	// steady state, BuildResourceTraffic would skip them, and the validator
	// would fire "Resource '<image>' is read but never written".
	//
	// Reset the state machine to first-generation mode so the next frame
	// re-runs all 49 passes in one shot. This re-fills the IBL textures with
	// identical contents (cheap) and gives the barrier generator a fresh,
	// consistent view of writers vs readers. After this single frame the state
	// machine drops back to amortised idle on the very next frame.
	//
	// IMPORTANT ordering: this check must run AFTER any system that may have
	// called MarkDirty() this frame (e.g. Flux_Fog::ApplyTechniqueSelectionToGraph),
	// so the call sequence in Zenith_Core::ExecuteRenderGraph is:
	//   1. Flux_Fog::ApplyTechniqueSelectionToGraph(xGraph)  // may call MarkDirty
	//   2. Flux_IBL::UpdateGraphPassEnables(xGraph)          // sees IsDirty()
	//   3. xGraph.Compile()                                  // full recompile
	//   4. xGraph.Execute()
	if (xGraph.IsDirty())
	{
		s_bBRDFLUTGenerated = false;
		s_bSkyIBLDirty = true;
		s_bFirstGeneration = true;
		s_bIBLReady = false;
		s_eRegenState = IBL_REGEN_IDLE;
		s_uRegenFace = 0;
		s_uRegenMip = 0;
	}

	// Default: nothing runs this frame.
	bool bRunBRDF = false;
	bool abRunIrradiance[6] = {};
	bool abRunPrefilter[IBLConfig::uPREFILTER_MIP_COUNT][6] = {};

	// BRDF LUT — generate on first frame or on manual regenerate.
	if (!s_bBRDFLUTGenerated || dbg_bIBLRegenerateBRDFLUT)
	{
		bRunBRDF = true;
		if (dbg_bIBLRegenerateBRDFLUT)
		{
#ifdef ZENITH_DEBUG_VARIABLES
			dbg_bIBLRegenerateBRDFLUT = false;
#endif
			s_bBRDFLUTGenerated = false;
		}
	}

	// Sky IBL state machine.
	if (!s_bSkyIBLDirty && s_eRegenState == IBL_REGEN_IDLE)
	{
		// Mark ready once everything has been generated at least once.
		if (s_bBRDFLUTGenerated && !s_bIBLReady)
		{
			s_bIBLReady = true;
			Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: All IBL textures ready");
		}
	}
	else if (s_bFirstGeneration)
	{
		// First generation must complete in a single frame to ensure all mip
		// levels have valid layouts before deferred shading binds the cubemap.
		for (u_int uFace = 0; uFace < 6; uFace++)
			abRunIrradiance[uFace] = true;
		for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
			for (u_int uFace = 0; uFace < 6; uFace++)
				abRunPrefilter[uMip][uFace] = true;
		s_bSkyIBLDirty = false;
		s_bFirstGeneration = false;
		s_eRegenState = IBL_REGEN_IDLE;
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: First generation - processing all passes this frame");
	}
	else
	{
		// Begin amortised regeneration if dirty and idle.
		if (s_bSkyIBLDirty && s_eRegenState == IBL_REGEN_IDLE)
		{
			s_eRegenState = IBL_REGEN_IRRADIANCE;
			s_uRegenFace = 0;
			s_uRegenMip = 0;
			Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Starting amortized IBL regeneration");
		}

		u_int uPassesThisFrame = 0;

		// Process irradiance faces (6 total)
		while (s_eRegenState == IBL_REGEN_IRRADIANCE && uPassesThisFrame < IBLConfig::uPASSES_PER_FRAME)
		{
			abRunIrradiance[s_uRegenFace] = true;
			s_uRegenFace++;
			uPassesThisFrame++;

			if (s_uRegenFace >= 6)
			{
				s_eRegenState = IBL_REGEN_PREFILTER;
				s_uRegenFace = 0;
				s_uRegenMip = 0;
			}
		}

		// Process prefilter mip-face combinations (42 total)
		while (s_eRegenState == IBL_REGEN_PREFILTER && uPassesThisFrame < IBLConfig::uPASSES_PER_FRAME)
		{
			abRunPrefilter[s_uRegenMip][s_uRegenFace] = true;
			uPassesThisFrame++;

			s_uRegenFace++;
			if (s_uRegenFace >= 6)
			{
				s_uRegenFace = 0;
				s_uRegenMip++;

				if (s_uRegenMip >= IBLConfig::uPREFILTER_MIP_COUNT)
				{
					s_eRegenState = IBL_REGEN_IDLE;
					s_bSkyIBLDirty = false;
					Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Completed amortized IBL regeneration");
				}
			}
		}
	}

	// Push the resolved enable bits into the graph. SetPassEnabled is a no-op
	// when the bit hasn't changed, so this is cheap in steady state. The IBL
	// passes have no explicit dependency edges, so SetPassEnabled takes the
	// cheap m_bEnabledMaskDirty path (clear-flag re-resolve only, no full
	// recompile).
	xGraph.SetPassEnabled(s_uBRDFLUTPassIdx, bRunBRDF);
	for (u_int uFace = 0; uFace < 6; uFace++)
		xGraph.SetPassEnabled(s_auIrradianceFacePassIdx[uFace], abRunIrradiance[uFace]);
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		for (u_int uFace = 0; uFace < 6; uFace++)
			xGraph.SetPassEnabled(s_auPrefilterMipFacePassIdx[uMip][uFace], abRunPrefilter[uMip][uFace]);
}

void Flux_IBL::ExecuteBRDFLUTPass(Flux_CommandList* pxCmd, void*)
{
	// No per-frame gate — disabled passes are skipped before record runs
	// (see Flux_RenderGraph::Execute Phase 1/2 enable check).
	pxCmd->AddCommand<Flux_CommandSetPipeline>(&s_xBRDFLUTPipeline);
	pxCmd->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCmd->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindCBV(s_xBRDFLUTFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	}
	pxCmd->AddCommand<Flux_CommandDrawIndexed>(6);
	s_bBRDFLUTGenerated = true;
}

void Flux_IBL::ExecuteIrradianceFacePass(Flux_CommandList* pxCmd, void* pUserData)
{
	const u_int uFace = *static_cast<const u_int*>(pUserData);

	struct IrradianceConstants { u_int m_uUseAtmosphere; float m_fSunIntensity; u_int m_uFaceIndex; float m_fPad; };
	IrradianceConstants xConsts;
	xConsts.m_uUseAtmosphere = 1;
	xConsts.m_fSunIntensity = Flux_Skybox::GetSunIntensity();
	xConsts.m_uFaceIndex = uFace;
	xConsts.m_fPad = 0.0f;

	pxCmd->AddCommand<Flux_CommandSetPipeline>(&s_xIrradianceConvolvePipeline);
	pxCmd->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCmd->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindCBV(s_xIrradianceFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.PushConstant(&xConsts, sizeof(xConsts));
		if (s_xIrradianceSkyboxBinding.IsValid())
		{
			if (Flux_Graphics::s_pxCubemapTexture)
				xBinder.BindSRV(s_xIrradianceSkyboxBinding, &Flux_Graphics::s_pxCubemapTexture->GetSRV());
			else if (Flux_Graphics::s_pxBlackTexture)
				xBinder.BindSRV(s_xIrradianceSkyboxBinding, &Flux_Graphics::s_pxBlackTexture->GetSRV());
		}
	}
	pxCmd->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_IBL::ExecutePrefilterMipFacePass(Flux_CommandList* pxCmd, void* pUserData)
{
	const IBLPrefilterPassData* pxData = static_cast<const IBLPrefilterPassData*>(pUserData);

	struct PrefilterConstants { float m_fRoughness; u_int m_uUseAtmosphere; float m_fSunIntensity; u_int m_uFaceIndex; };
	PrefilterConstants xConsts;
	xConsts.m_fRoughness = static_cast<float>(pxData->m_uMip) / static_cast<float>(IBLConfig::uPREFILTER_MIP_COUNT - 1);
	xConsts.m_uUseAtmosphere = 1;
	xConsts.m_fSunIntensity = Flux_Skybox::GetSunIntensity();
	xConsts.m_uFaceIndex = pxData->m_uFace;

	pxCmd->AddCommand<Flux_CommandSetPipeline>(&s_xPrefilterPipeline);
	pxCmd->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCmd->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindCBV(s_xPrefilterFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.PushConstant(&xConsts, sizeof(xConsts));
		if (s_xPrefilterSkyboxBinding.IsValid())
		{
			if (Flux_Graphics::s_pxCubemapTexture)
				xBinder.BindSRV(s_xPrefilterSkyboxBinding, &Flux_Graphics::s_pxCubemapTexture->GetSRV());
			else if (Flux_Graphics::s_pxBlackTexture)
				xBinder.BindSRV(s_xPrefilterSkyboxBinding, &Flux_Graphics::s_pxBlackTexture->GetSRV());
		}
	}
	pxCmd->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_IBL::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Note: the IBL state-machine reset that used to live here has been moved
	// into UpdateGraphPassEnables, where it now triggers off the graph's
	// IsDirty() flag. That covers SetupRenderGraph rebuilds (resize) AND any
	// other system that calls MarkDirty() (e.g. Flux_Fog technique switching),
	// without needing per-call-site reset code.

	// BRDF LUT pass. The amortised state machine that decides which IBL passes
	// run this frame lives in Flux_IBL::UpdateGraphPassEnables, called from
	// Zenith_Core::ExecuteRenderGraph BEFORE Compile (it cannot live as a pass
	// OnPrepare because Phase 0 only fires OnPrepare for *enabled* passes).
	// SetPassClearTargets(true) is safe even when the pass is disabled —
	// ResolveClearFlags filters disabled passes out of clear ownership.
	{
		s_uBRDFLUTPassIdx = xGraph.AddPass("IBL BRDF LUT", ExecuteBRDFLUTPass);
		xGraph.SetPassTargetSetup(s_uBRDFLUTPassIdx, s_xBRDFLUTSetup);
		xGraph.SetPassClearTargets(s_uBRDFLUTPassIdx, true);
		xGraph.PassWrites(s_uBRDFLUTPassIdx, &s_xBRDFLUT, RESOURCE_ACCESS_WRITE_RTV);
	}

	// 6 irradiance face passes — each writes layer N of the irradiance cubemap.
	static const char* const s_aszIrradianceFaceNames[6] = {
		"IBL Irradiance Face 0", "IBL Irradiance Face 1", "IBL Irradiance Face 2",
		"IBL Irradiance Face 3", "IBL Irradiance Face 4", "IBL Irradiance Face 5"
	};
	for (u_int uFace = 0; uFace < 6; uFace++)
	{
		s_auIrradianceFacePassIdx[uFace] = xGraph.AddPass(s_aszIrradianceFaceNames[uFace], ExecuteIrradianceFacePass, &s_auIrradianceFaceData[uFace]);
		xGraph.SetPassTargetSetup(s_auIrradianceFacePassIdx[uFace], s_axIrradianceFaceSetup[uFace]);
		xGraph.SetPassClearTargets(s_auIrradianceFacePassIdx[uFace], true);
		// Mip 0 (the only mip). Face distinction is handled by target setup —
		// render graph old-API only tracks mip ranges, so all face writes appear
		// as writers of the same resource and form a sequential chain that
		// correctly barriers between faces.
		xGraph.PassWrites(s_auIrradianceFacePassIdx[uFace], &s_xIrradianceMap, RESOURCE_ACCESS_WRITE_RTV, 0, 1);
	}

	// 42 prefilter mip-face passes — each writes one (mip, face) slot of the
	// prefiltered cubemap. The graph honours per-mip/per-layer ranges so each
	// slot is tracked independently and no phantom edges are added between
	// disjoint subresources.
	static const char* const s_aszPrefilterPassNames[IBLConfig::uPREFILTER_MIP_COUNT * 6] = {
		"IBL Prefilter M0 F0", "IBL Prefilter M0 F1", "IBL Prefilter M0 F2", "IBL Prefilter M0 F3", "IBL Prefilter M0 F4", "IBL Prefilter M0 F5",
		"IBL Prefilter M1 F0", "IBL Prefilter M1 F1", "IBL Prefilter M1 F2", "IBL Prefilter M1 F3", "IBL Prefilter M1 F4", "IBL Prefilter M1 F5",
		"IBL Prefilter M2 F0", "IBL Prefilter M2 F1", "IBL Prefilter M2 F2", "IBL Prefilter M2 F3", "IBL Prefilter M2 F4", "IBL Prefilter M2 F5",
		"IBL Prefilter M3 F0", "IBL Prefilter M3 F1", "IBL Prefilter M3 F2", "IBL Prefilter M3 F3", "IBL Prefilter M3 F4", "IBL Prefilter M3 F5",
		"IBL Prefilter M4 F0", "IBL Prefilter M4 F1", "IBL Prefilter M4 F2", "IBL Prefilter M4 F3", "IBL Prefilter M4 F4", "IBL Prefilter M4 F5",
		"IBL Prefilter M5 F0", "IBL Prefilter M5 F1", "IBL Prefilter M5 F2", "IBL Prefilter M5 F3", "IBL Prefilter M5 F4", "IBL Prefilter M5 F5",
		"IBL Prefilter M6 F0", "IBL Prefilter M6 F1", "IBL Prefilter M6 F2", "IBL Prefilter M6 F3", "IBL Prefilter M6 F4", "IBL Prefilter M6 F5",
	};
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
	{
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			s_axPrefilterPassData[uMip][uFace].m_uMip = uMip;
			s_axPrefilterPassData[uMip][uFace].m_uFace = uFace;
			s_auPrefilterMipFacePassIdx[uMip][uFace] = xGraph.AddPass(s_aszPrefilterPassNames[uMip * 6 + uFace],
				ExecutePrefilterMipFacePass, &s_axPrefilterPassData[uMip][uFace]);
			xGraph.SetPassTargetSetup(s_auPrefilterMipFacePassIdx[uMip][uFace], s_axPrefilteredMipFaceSetup[uMip][uFace]);
			xGraph.SetPassClearTargets(s_auPrefilterMipFacePassIdx[uMip][uFace], true);
			xGraph.PassWrites(s_auPrefilterMipFacePassIdx[uMip][uFace], &s_xPrefilteredMap, RESOURCE_ACCESS_WRITE_RTV, uMip, 1);
		}
	}
}

void Flux_IBL::CreateRenderTargets()
{
	Flux_RenderAttachmentBuilder xBuilder;

	// BRDF LUT - 2D RG16F texture (NdotV x Roughness -> scale, bias)
	xBuilder.m_uWidth = IBLConfig::uBRDF_LUT_SIZE;
	xBuilder.m_uHeight = IBLConfig::uBRDF_LUT_SIZE;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16_SFLOAT;  // Only need RG channels for scale/bias

	xBuilder.BuildColour(s_xBRDFLUT, "IBL BRDF LUT");
	s_xBRDFLUTSetup.m_axColourAttachments[0] = s_xBRDFLUT;

	// Irradiance map - cubemap for diffuse IBL
	xBuilder.m_uWidth = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_uHeight = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.BuildColourCubemap(s_xIrradianceMap, "IBL Irradiance Map");

	// Set up per-face target setups for irradiance (using face RTVs)
	for (u_int i = 0; i < 6; i++)
	{
		ConfigureCubeFaceTarget(s_axIrradianceFaceSetup[i], s_xIrradianceMap, i, s_xIrradianceMap.m_axFaceRTVs[i]);
	}

	// Prefiltered environment map - cubemap for specular IBL (with mip chain for roughness levels)
	xBuilder.m_uWidth = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uHeight = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uNumMips = IBLConfig::uPREFILTER_MIP_COUNT;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.BuildColourCubemap(s_xPrefilteredMap, "IBL Prefiltered Map");

	// Set up per-face target setups for prefiltered map (mip 0 only, for backwards compatibility)
	for (u_int i = 0; i < 6; i++)
	{
		ConfigureCubeFaceTarget(s_axPrefilteredFaceSetup[i], s_xPrefilteredMap, i, s_xPrefilteredMap.m_axFaceRTVs[i]);
	}

	// Create per-mip-per-face RTVs and target setups for all roughness levels
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
	{
		u_int uMipSize = IBLConfig::uPREFILTER_SIZE >> uMip;

		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			s_axPrefilteredMipFaceRTVs[uMip][uFace] = Flux_MemoryManager::CreateRenderTargetViewForLayer(
				s_xPrefilteredMap.m_xVRAMHandle,
				s_xPrefilteredMap.m_xSurfaceInfo,
				uFace,
				uMip);

			ConfigureCubeFaceTarget(s_axPrefilteredMipFaceSetup[uMip][uFace], s_xPrefilteredMap,
				uFace, s_axPrefilteredMipFaceRTVs[uMip][uFace], uMip, uMipSize);
		}
	}
}

void Flux_IBL::DestroyRenderTargets()
{
	auto DestroyAttachment = [](Flux_RenderAttachment& xAttachment)
	{
		if (xAttachment.m_xVRAMHandle.IsValid())
		{
			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.m_pxRTV.m_xImageViewHandle, xAttachment.m_pxDSV.m_xImageViewHandle,
				xAttachment.m_pxSRV.m_xImageViewHandle, xAttachment.m_pxUAV.m_xImageViewHandle);
		}
	};

	// Helper to clean up cubemap face RTVs and SRVs
	auto DestroyCubemapFaceViews = [](Flux_RenderAttachment& xAttachment)
	{
		for (u_int i = 0; i < 6; i++)
		{
			if (xAttachment.m_axFaceRTVs[i].m_xImageViewHandle.IsValid())
			{
				Flux_MemoryManager::QueueImageViewDeletion(xAttachment.m_axFaceRTVs[i].m_xImageViewHandle);
				xAttachment.m_axFaceRTVs[i] = Flux_RenderTargetView();
			}
			if (xAttachment.m_axFaceSRVs[i].m_xImageViewHandle.IsValid())
			{
				Flux_MemoryManager::QueueImageViewDeletion(xAttachment.m_axFaceSRVs[i].m_xImageViewHandle);
				xAttachment.m_axFaceSRVs[i] = Flux_ShaderResourceView();
			}
		}
	};

	DestroyAttachment(s_xBRDFLUT);

	// Clean up irradiance cubemap face views before destroying VRAM
	DestroyCubemapFaceViews(s_xIrradianceMap);
	DestroyAttachment(s_xIrradianceMap);

	// Clean up per-mip RTVs before destroying the prefiltered map VRAM
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
	{
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			if (s_axPrefilteredMipFaceRTVs[uMip][uFace].m_xImageViewHandle.IsValid())
			{
				Flux_MemoryManager::QueueImageViewDeletion(s_axPrefilteredMipFaceRTVs[uMip][uFace].m_xImageViewHandle);
				s_axPrefilteredMipFaceRTVs[uMip][uFace] = Flux_RenderTargetView();
			}
		}
	}

	// Clean up prefiltered cubemap base face views
	DestroyCubemapFaceViews(s_xPrefilteredMap);
	DestroyAttachment(s_xPrefilteredMap);
}

// Phase 5.1: GenerateBRDFLUT/UpdateSkyIBL/GenerateIrradianceMap/GeneratePrefilteredMap
// were the old bypass-submit path. Their work is now performed by graph-driven
// per-pass execute callbacks (IBLBRDFLUTExecute / IBLIrradianceFaceExecute /
// IBLPrefilterMipFaceExecute) that fill per-pass command lists during Phase 1.
// The functions remain only as no-op compatibility shims for any external caller.
void Flux_IBL::GenerateBRDFLUT()
{
	// No-op: BRDF LUT generation is driven by the render graph + per-frame
	// IBLPrepareCallback. Setting s_bBRDFLUTGenerated=false marks it for
	// regeneration on the next graph pass.
	s_bBRDFLUTGenerated = false;
}

void Flux_IBL::UpdateSkyIBL()
{
	// No-op: regeneration is driven by IBLPrepareCallback. Marking the dirty
	// flag is enough to schedule per-face/per-mip work over the next frames.
	s_bSkyIBLDirty = true;
}

// GenerateIrradianceMap / GeneratePrefilteredMap / GenerateIrradianceFace /
// GeneratePrefilteredFace were the bypass-submit helpers — now obsolete.
// All work is performed by the per-pass execute callbacks above.
void Flux_IBL::GenerateIrradianceMap() {}
void Flux_IBL::GeneratePrefilteredMap() {}
void Flux_IBL::GenerateIrradianceFace(u_int /*uFace*/) {}
void Flux_IBL::GeneratePrefilteredFace(u_int /*uMip*/, u_int /*uFace*/) {}

void Flux_IBL::MarkAllProbesDirty()
{
	s_bSkyIBLDirty = true;
}

// Accessors - return const references to prevent modification and signal temporary nature
const Flux_ShaderResourceView& Flux_IBL::GetBRDFLUTSRV()
{
	return s_xBRDFLUT.m_pxSRV;
}

const Flux_ShaderResourceView& Flux_IBL::GetIrradianceMapSRV()
{
	return s_xIrradianceMap.m_pxSRV;
}

const Flux_ShaderResourceView& Flux_IBL::GetPrefilteredMapSRV()
{
	return s_xPrefilteredMap.m_pxSRV;
}

// Setters
void Flux_IBL::SetEnabled(bool bEnabled)
{
	s_bEnabled = bEnabled;
}
void Flux_IBL::SetIntensity(float fIntensity)
{
	s_fIntensity = fIntensity;
}
void Flux_IBL::SetDiffuseEnabled(bool bEnabled)
{
	s_bDiffuseEnabled = bEnabled;
}
void Flux_IBL::SetSpecularEnabled(bool bEnabled)
{
	s_bSpecularEnabled = bEnabled;
}

// Getters - return actual state variables (synced from debug variables in ExecuteIBLUpdate)
bool Flux_IBL::IsEnabled() { return s_bEnabled; }
bool Flux_IBL::IsReady() { return s_bIBLReady; }
float Flux_IBL::GetIntensity() { return s_fIntensity; }
bool Flux_IBL::IsDiffuseEnabled() { return s_bDiffuseEnabled; }
bool Flux_IBL::IsSpecularEnabled() { return s_bSpecularEnabled; }
bool Flux_IBL::IsShowBRDFLUT() { return dbg_bIBLShowBRDFLUT; }
bool Flux_IBL::IsForceRoughness() { return dbg_bIBLForceRoughness; }
float Flux_IBL::GetForcedRoughness() { return dbg_fIBLForcedRoughness; }

#ifdef ZENITH_TOOLS
void Flux_IBL::RegisterDebugVariables()
{
	// NOTE: Texture debug variables are registered here during Initialise(), before
	// content is generated. The SRVs are valid (created in CreateRenderTargets) but
	// textures will appear black/undefined until GenerateBRDFLUT() and UpdateSkyIBL()
	// run on the first frame. This is expected behavior.
	Zenith_DebugVariables::AddBoolean({ "Flux", "IBL", "ShowBRDFLUT" }, dbg_bIBLShowBRDFLUT);
	Zenith_DebugVariables::AddBoolean({ "Flux", "IBL", "ForceRoughness" }, dbg_bIBLForceRoughness);
	Zenith_DebugVariables::AddFloat({ "Flux", "IBL", "ForcedRoughness" }, dbg_fIBLForcedRoughness, 0.0f, 1.0f);
	Zenith_DebugVariables::AddBoolean({ "Flux", "IBL", "RegenerateBRDFLUT" }, dbg_bIBLRegenerateBRDFLUT);

	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "BRDF_LUT" }, s_xBRDFLUT.m_pxSRV);

	// Register individual cubemap faces for irradiance map (face order: +X, -X, +Y, -Y, +Z, -Z)
	// Using PosX/NegX naming to avoid special characters in debug variable paths
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face0_PosX" }, s_xIrradianceMap.m_axFaceSRVs[0]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face1_NegX" }, s_xIrradianceMap.m_axFaceSRVs[1]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face2_PosY" }, s_xIrradianceMap.m_axFaceSRVs[2]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face3_NegY" }, s_xIrradianceMap.m_axFaceSRVs[3]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face4_PosZ" }, s_xIrradianceMap.m_axFaceSRVs[4]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face5_NegZ" }, s_xIrradianceMap.m_axFaceSRVs[5]);

	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face0_PosX" }, s_xPrefilteredMap.m_axFaceSRVs[0]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face1_NegX" }, s_xPrefilteredMap.m_axFaceSRVs[1]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face2_PosY" }, s_xPrefilteredMap.m_axFaceSRVs[2]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face3_NegY" }, s_xPrefilteredMap.m_axFaceSRVs[3]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face4_PosZ" }, s_xPrefilteredMap.m_axFaceSRVs[4]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face5_NegZ" }, s_xPrefilteredMap.m_axFaceSRVs[5]);
}
#endif
