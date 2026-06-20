#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Static member definitions






// Frame-amortized regeneration state

// Render-graph pass indices — populated by SetupRenderGraph, consumed every
// frame by UpdateGraphPassEnables.

// Per-pass user data is now pointer-stable storage on Flux_IBLImpl
// (m_auIrradianceFaceData / m_axPrefilterPassData), populated + registered in
// SetupRenderGraph and handed back to the Execute*Pass trampolines as void*.
// IBLPrefilterPassData is a nested type of Flux_IBLImpl (see Flux_IBLImpl.h).

DEBUGVAR bool dbg_bIBLShowBRDFLUT = false;
DEBUGVAR bool dbg_bIBLForceRoughness = false;
DEBUGVAR float dbg_fIBLForcedRoughness = 0.5f;
DEBUGVAR bool dbg_bIBLRegenerateBRDFLUT = false;

void Flux_IBLImpl::BuildPipelines()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBRDFLUTShader, m_xBRDFLUTPipeline,
		FluxShaderProgram::IBL_BRDFIntegration, m_xBRDFLUT.m_xSurfaceInfo.m_eFormat);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xIrradianceConvolveShader, m_xIrradianceConvolvePipeline,
		FluxShaderProgram::IBL_IrradianceConvolution, m_xIrradianceMap.m_xSurfaceInfo.m_eFormat);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xPrefilterShader, m_xPrefilterPipeline,
		FluxShaderProgram::IBL_PrefilterEnvMap, m_xPrefilteredMap.m_xSurfaceInfo.m_eFormat);
}

void Flux_IBLImpl::Initialise()
{
	CreateRenderTargets();
	BuildPipelines();

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	// BRDF LUT will be generated on first frame via render graph ExecuteIBLUpdate()
	// This ensures the render loop is active when the command list is submitted

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL Initialised");
}

void Flux_IBLImpl::Shutdown()
{
	DestroyRenderTargets();
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL shut down");
}

void Flux_IBLImpl::Reset()
{
	m_bSkyIBLDirty = true;
	m_bIBLReady = false;  // Need to regenerate IBL on next frame
	m_bFirstGeneration = true;  // Force non-amortized generation after reset

	// Reset amortized regeneration state
	m_eRegenState = IBL_REGEN_IDLE;
	m_uRegenFace = 0;
	m_uRegenMip = 0;
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

// Reset the IBL regeneration state machine when a recompile is pending. The
// validator inside Compile() requires every IBL-texture read to have at least
// one enabled writer; without this, the steady-state amortised path would
// leave all 49 IBL passes disabled and trip the validator. Re-running first-
// generation refills the textures with identical contents (cheap) and gives
// the barrier generator a consistent view.
void Flux_IBLImpl::ResetIBLRegenStateForRecompile()
{
	m_bBRDFLUTGenerated = false;
	m_bSkyIBLDirty = true;
	m_bFirstGeneration = true;
	m_bIBLReady = false;
	m_eRegenState = IBL_REGEN_IDLE;
	m_uRegenFace = 0;
	m_uRegenMip = 0;
}

// BRDF LUT runs on the first frame and on manual regenerate. Side-effects:
// clears the regenerate-LUT debug flag and resets the generated bit when the
// regenerate flag was set so the LUT runs THIS frame.
bool Flux_IBLImpl::ResolveBRDFLUTRun()
{
	if (!m_bBRDFLUTGenerated || dbg_bIBLRegenerateBRDFLUT)
	{
		if (dbg_bIBLRegenerateBRDFLUT)
		{
#ifdef ZENITH_DEBUG_VARIABLES
			dbg_bIBLRegenerateBRDFLUT = false;
#endif
			m_bBRDFLUTGenerated = false;
		}
		return true;
	}
	return false;
}

// First generation: enable every irradiance face and every prefilter mip+face
// in one frame. Required so all mip levels have valid layouts before deferred
// shading binds the cubemap.
void Flux_IBLImpl::RunFirstGenerationFrame(bool (&abRunIrradiance)[6],
	bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6])
{
	for (u_int uFace = 0; uFace < 6; uFace++)
		abRunIrradiance[uFace] = true;
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		for (u_int uFace = 0; uFace < 6; uFace++)
			abRunPrefilter[uMip][uFace] = true;
	m_bSkyIBLDirty = false;
	m_bFirstGeneration = false;
	m_eRegenState = IBL_REGEN_IDLE;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: First generation - processing all passes this frame");
}

// Amortised regeneration: process up to PASSES_PER_FRAME irradiance/prefilter
// passes per frame. Two phases — irradiance (6 faces) then prefilter
// (mip × face). State (g_xEngine.IBL().m_eRegenState, g_xEngine.IBL().m_uRegenFace, g_xEngine.IBL().m_uRegenMip) advances each
// frame; idle is reached after all faces of all mips have run.
void Flux_IBLImpl::AdvanceAmortizedRegen(bool (&abRunIrradiance)[6],
	bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6])
{
	if (m_bSkyIBLDirty && m_eRegenState == IBL_REGEN_IDLE)
	{
		m_eRegenState = IBL_REGEN_IRRADIANCE;
		m_uRegenFace = 0;
		m_uRegenMip = 0;
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Starting amortized IBL regeneration");
	}

	u_int uPassesThisFrame = 0;

	while (m_eRegenState == IBL_REGEN_IRRADIANCE && uPassesThisFrame < IBLConfig::uPASSES_PER_FRAME)
	{
		abRunIrradiance[m_uRegenFace] = true;
		m_uRegenFace++;
		uPassesThisFrame++;

		if (m_uRegenFace >= 6)
		{
			m_eRegenState = IBL_REGEN_PREFILTER;
			m_uRegenFace = 0;
			m_uRegenMip = 0;
		}
	}

	while (m_eRegenState == IBL_REGEN_PREFILTER && uPassesThisFrame < IBLConfig::uPASSES_PER_FRAME)
	{
		abRunPrefilter[m_uRegenMip][m_uRegenFace] = true;
		uPassesThisFrame++;

		m_uRegenFace++;
		if (m_uRegenFace >= 6)
		{
			m_uRegenFace = 0;
			m_uRegenMip++;

			if (m_uRegenMip >= IBLConfig::uPREFILTER_MIP_COUNT)
			{
				m_eRegenState = IBL_REGEN_IDLE;
				m_bSkyIBLDirty = false;
				Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Completed amortized IBL regeneration");
			}
		}
	}
}

// Push resolved enable bits into the graph. SetEnabled no-ops when the bit
// hasn't changed (cheap in steady state); IBL passes have no explicit
// dependency edges, so this takes the m_bEnabledMaskDirty fast path rather
// than triggering a full recompile.
void Flux_IBLImpl::ApplyResolvedIBLEnables(Flux_RenderGraph& xGraph,
	bool bRunBRDF,
	const bool (&abRunIrradiance)[6],
	const bool (&abRunPrefilter)[IBLConfig::uPREFILTER_MIP_COUNT][6])
{
	xGraph.SetEnabled(m_xBRDFLUTPassHandle, bRunBRDF);
	for (u_int uFace = 0; uFace < 6; uFace++)
		xGraph.SetEnabled(m_axIrradianceFacePassHandles[uFace], abRunIrradiance[uFace]);
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		for (u_int uFace = 0; uFace < 6; uFace++)
			xGraph.SetEnabled(m_axPrefilterMipFacePassHandles[uMip][uFace], abRunPrefilter[uMip][uFace]);
}

void Flux_IBLImpl::UpdateGraphPassEnables(Flux_RenderGraph& xGraph)
{
	// IMPORTANT ordering: this dirty check must run AFTER any system that may
	// have called MarkDirty() this frame (e.g. g_xEngine.Fog().ApplyTechniqueSelectionToGraph),
	// so the call sequence in Zenith_Core::ExecuteRenderGraph is:
	//   1. g_xEngine.Fog().ApplyTechniqueSelectionToGraph(xGraph)  // may call MarkDirty
	//   2. Flux_IBLImpl::UpdateGraphPassEnables(xGraph)          // sees IsDirty()
	//   3. xGraph.Compile()                                  // full recompile
	//   4. xGraph.Execute()
	if (xGraph.IsDirty())
	{
		ResetIBLRegenStateForRecompile();
	}

	bool bRunBRDF = ResolveBRDFLUTRun();
	bool abRunIrradiance[6] = {};
	bool abRunPrefilter[IBLConfig::uPREFILTER_MIP_COUNT][6] = {};

	// Sky IBL state machine: idle → first-generation OR amortised regen.
	if (!m_bSkyIBLDirty && m_eRegenState == IBL_REGEN_IDLE)
	{
		// Mark ready once everything has been generated at least once.
		if (m_bBRDFLUTGenerated && !m_bIBLReady)
		{
			m_bIBLReady = true;
			Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: All IBL textures ready");
		}
	}
	else if (m_bFirstGeneration)
	{
		RunFirstGenerationFrame(abRunIrradiance, abRunPrefilter);
	}
	else
	{
		AdvanceAmortizedRegen(abRunIrradiance, abRunPrefilter);
	}

	ApplyResolvedIBLEnables(xGraph, bRunBRDF, abRunIrradiance, abRunPrefilter);
}

void Flux_IBLImpl::ExecuteBRDFLUTPass(Flux_CommandBuffer* pxCmd, void*)
{
	// Trampoline (non-capturing graph callback): recover the singleton first,
	// then route IBL state through it.
	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	// No per-frame gate — disabled passes are skipped before record runs
	// (see Flux_RenderGraph::Execute Phase 1/2 enable check).
	auto& xFG = g_xEngine.FluxGraphics();
	pxCmd->SetPipeline(&xIBL.m_xBRDFLUTPipeline);
	pxCmd->SetVertexBuffer(xFG.m_xQuadMesh.GetVertexBuffer());
	pxCmd->SetIndexBuffer(xFG.m_xQuadMesh.GetIndexBuffer());

	// BRDF integration only reads its UV input; the Slang version exposes no
	// CBs in reflection so no binder calls are needed before the draw.
	pxCmd->DrawIndexed(6);
	xIBL.m_bBRDFLUTGenerated = true;
}

void Flux_IBLImpl::ExecuteIrradianceFacePass(Flux_CommandBuffer* pxCmd, void* pUserData)
{
	// Trampoline (non-capturing graph callback): recover the singleton first.
	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	const u_int uFace = *static_cast<const u_int*>(pUserData);

	struct IrradianceConstants { u_int m_uUseAtmosphere; float m_fSunIntensity; u_int m_uFaceIndex; float m_fPad; };
	IrradianceConstants xConsts;
	xConsts.m_uUseAtmosphere = 1;
	xConsts.m_fSunIntensity = g_xEngine.Skybox().GetSunIntensity();
	xConsts.m_uFaceIndex = uFace;
	xConsts.m_fPad = 0.0f;

	auto& xFG = g_xEngine.FluxGraphics();
	pxCmd->SetPipeline(&xIBL.m_xIrradianceConvolvePipeline);
	pxCmd->SetVertexBuffer(xFG.m_xQuadMesh.GetVertexBuffer());
	pxCmd->SetIndexBuffer(xFG.m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindCBV(xIBL.m_xIrradianceConvolveShader, "FrameConstants", &xFG.m_xFrameConstantsBuffer.GetCBV());
		xBinder.BindDrawConstants(xIBL.m_xIrradianceConvolveShader, "IrradianceConstants", &xConsts, sizeof(xConsts));
		if (Zenith_TextureAsset* pxCubemap = xFG.m_xCubemapTexture.GetDirect())
			xBinder.BindSRV(xIBL.m_xIrradianceConvolveShader, "g_xSkyboxCubemap", &pxCubemap->GetSRV());
		else if (Zenith_TextureAsset* pxBlack = xFG.m_xBlackTexture.GetDirect())
			xBinder.BindSRV(xIBL.m_xIrradianceConvolveShader, "g_xSkyboxCubemap", &pxBlack->GetSRV());
	}
	pxCmd->DrawIndexed(6);
}

void Flux_IBLImpl::ExecutePrefilterMipFacePass(Flux_CommandBuffer* pxCmd, void* pUserData)
{
	// Trampoline (non-capturing graph callback): recover the singleton first.
	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	const IBLPrefilterPassData* pxData = static_cast<const IBLPrefilterPassData*>(pUserData);

	struct PrefilterConstants { float m_fRoughness; u_int m_uUseAtmosphere; float m_fSunIntensity; u_int m_uFaceIndex; };
	PrefilterConstants xConsts;
	xConsts.m_fRoughness = static_cast<float>(pxData->m_uMip) / static_cast<float>(IBLConfig::uPREFILTER_MIP_COUNT - 1);
	xConsts.m_uUseAtmosphere = 1;
	xConsts.m_fSunIntensity = g_xEngine.Skybox().GetSunIntensity();
	xConsts.m_uFaceIndex = pxData->m_uFace;

	auto& xFG = g_xEngine.FluxGraphics();
	pxCmd->SetPipeline(&xIBL.m_xPrefilterPipeline);
	pxCmd->SetVertexBuffer(xFG.m_xQuadMesh.GetVertexBuffer());
	pxCmd->SetIndexBuffer(xFG.m_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindCBV(xIBL.m_xPrefilterShader, "FrameConstants", &xFG.m_xFrameConstantsBuffer.GetCBV());
		xBinder.BindDrawConstants(xIBL.m_xPrefilterShader, "PrefilterConstants", &xConsts, sizeof(xConsts));
		if (Zenith_TextureAsset* pxCubemap = xFG.m_xCubemapTexture.GetDirect())
			xBinder.BindSRV(xIBL.m_xPrefilterShader, "g_xSkyboxCubemap", &pxCubemap->GetSRV());
		else if (Zenith_TextureAsset* pxBlack = xFG.m_xBlackTexture.GetDirect())
			xBinder.BindSRV(xIBL.m_xPrefilterShader, "g_xSkyboxCubemap", &pxBlack->GetSRV());
	}
	pxCmd->DrawIndexed(6);
}

void Flux_IBLImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Note: the IBL state-machine reset that used to live here has been moved
	// into UpdateGraphPassEnables, where it now triggers off the graph's
	// IsDirty() flag. That covers SetupRenderGraph rebuilds (resize) AND any
	// other system that calls MarkDirty() (e.g. Flux_Fog technique switching),
	// without needing per-call-site reset code.

	// BRDF LUT pass. The amortised state machine that decides which IBL passes
	// run this frame lives in Flux_IBLImpl::UpdateGraphPassEnables, called from
	// Zenith_Core::ExecuteRenderGraph BEFORE Compile (it cannot live as a pass
	// OnPrepare because Phase 0 only fires OnPrepare for *enabled* passes).
	// SetPassClearTargets(true) is safe even when the pass is disabled —
	// ResolveClearFlags filters disabled passes out of clear ownership.
	m_xBRDFLUTPassHandle = xGraph.AddPass("IBL BRDF LUT", ExecuteBRDFLUTPass)
		.ClearTargets()
		.Writes(m_xBRDFLUT, RESOURCE_ACCESS_WRITE_RTV);

	// 6 irradiance face passes — each writes layer N of the irradiance cubemap.
	static const char* const s_aszIrradianceFaceNames[6] = {
		"IBL Irradiance Face 0", "IBL Irradiance Face 1", "IBL Irradiance Face 2",
		"IBL Irradiance Face 3", "IBL Irradiance Face 4", "IBL Irradiance Face 5"
	};
	for (u_int uFace = 0; uFace < 6; uFace++)
	{
		// Per-(mip 0, face uFace) slice write — the graph picks the per-(mip, face)
		// RTV and emits a tight subresource barrier.
		m_axIrradianceFacePassHandles[uFace] = xGraph.AddPass(
				s_aszIrradianceFaceNames[uFace], ExecuteIrradianceFacePass, &m_auIrradianceFaceData[uFace])
			.ClearTargets()
			.Writes(m_xIrradianceMap, RESOURCE_ACCESS_WRITE_RTV, 0, 1, uFace, 1);
	}

	// 42 prefilter mip-face passes — each writes one (mip, face) slot of the
	// prefiltered cubemap.
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
			m_axPrefilterPassData[uMip][uFace].m_uMip = uMip;
			m_axPrefilterPassData[uMip][uFace].m_uFace = uFace;
			m_axPrefilterMipFacePassHandles[uMip][uFace] = xGraph.AddPass(
					s_aszPrefilterPassNames[uMip * 6 + uFace],
					ExecutePrefilterMipFacePass, &m_axPrefilterPassData[uMip][uFace])
				.ClearTargets()
				.Writes(m_xPrefilteredMap, RESOURCE_ACCESS_WRITE_RTV, uMip, 1, uFace, 1);
		}
	}
}

void Flux_IBLImpl::CreateRenderTargets()
{
	Flux_RenderAttachmentBuilder xBuilder;

	// BRDF LUT - 2D RG16F texture (NdotV x Roughness -> scale, bias)
	xBuilder.m_uWidth = IBLConfig::uBRDF_LUT_SIZE;
	xBuilder.m_uHeight = IBLConfig::uBRDF_LUT_SIZE;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16_SFLOAT;

	xBuilder.BuildColour(m_xBRDFLUT, "IBL BRDF LUT");

	// Irradiance map - cubemap for diffuse IBL
	xBuilder.m_uWidth = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_uHeight = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.BuildColourCubemap(m_xIrradianceMap, "IBL Irradiance Map");

	// Prefiltered environment map - cubemap for specular IBL (with mip chain for roughness levels)
	xBuilder.m_uWidth = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uHeight = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uNumMips = IBLConfig::uPREFILTER_MIP_COUNT;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.BuildColourCubemap(m_xPrefilteredMap, "IBL Prefiltered Map");
}

void Flux_IBLImpl::DestroyRenderTargets()
{
	// Route through the builder so per-mip RTVs / SRVs / UAVs for multi-mip
	// attachments (prefiltered cube has 7 mips × 6 faces = 42 RTVs alone) get
	// released — a hand-rolled loop here previously STUBBED the cube path and
	// only queued mip 0 for the 2D path, leaking GPU memory every Shutdown.
	Flux_RenderAttachmentBuilder::Destroy(m_xBRDFLUT);
	Flux_RenderAttachmentBuilder::Destroy(m_xIrradianceMap);
	Flux_RenderAttachmentBuilder::Destroy(m_xPrefilteredMap);
}

// No-op compatibility shims — all IBL generation work is performed by graph-
// driven per-pass execute callbacks (IBLBRDFLUTExecute / IBLIrradianceFaceExecute /
// IBLPrefilterMipFaceExecute). These entry points remain only for external
// callers that expect the imperative API; consider deleting them if unused.
void Flux_IBLImpl::GenerateBRDFLUT()
{
	// No-op: BRDF LUT generation is driven by the render graph + per-frame
	// IBLPrepareCallback. Setting m_bBRDFLUTGenerated=false marks it for
	// regeneration on the next graph pass.
	m_bBRDFLUTGenerated = false;
}

void Flux_IBLImpl::UpdateSkyIBL()
{
	// No-op: regeneration is driven by IBLPrepareCallback. Marking the dirty
	// flag is enough to schedule per-face/per-mip work over the next frames.
	m_bSkyIBLDirty = true;
}

// GenerateIrradianceMap / GeneratePrefilteredMap / GenerateIrradianceFace /
// GeneratePrefilteredFace were the bypass-submit helpers — now obsolete.
// All work is performed by the per-pass execute callbacks above.
void Flux_IBLImpl::GenerateIrradianceMap() {}
void Flux_IBLImpl::GeneratePrefilteredMap() {}
void Flux_IBLImpl::GenerateIrradianceFace(u_int /*uFace*/) {}
void Flux_IBLImpl::GeneratePrefilteredFace(u_int /*uMip*/, u_int /*uFace*/) {}

void Flux_IBLImpl::MarkAllProbesDirty()
{
	m_bSkyIBLDirty = true;
}

// Accessors - return const references to prevent modification and signal temporary nature
const Flux_ShaderResourceView& Flux_IBLImpl::GetBRDFLUTSRV()
{
	return m_xBRDFLUT.SRV();
}

const Flux_ShaderResourceView& Flux_IBLImpl::GetIrradianceMapSRV()
{
	return m_xIrradianceMap.SRV();
}

const Flux_ShaderResourceView& Flux_IBLImpl::GetPrefilteredMapSRV()
{
	return m_xPrefilteredMap.SRV();
}

// Setters (continuous parameters; on/off toggles live in Zenith_GraphicsOptions)

// Getters
bool Flux_IBLImpl::IsEnabled() const { return Zenith_GraphicsOptions::Get().m_bIBLEnabled; }
bool Flux_IBLImpl::IsDiffuseEnabled() const { return Zenith_GraphicsOptions::Get().m_bIBLDiffuseEnabled; }
bool Flux_IBLImpl::IsSpecularEnabled() const { return Zenith_GraphicsOptions::Get().m_bIBLSpecularEnabled; }
bool Flux_IBLImpl::IsShowBRDFLUT() const { return dbg_bIBLShowBRDFLUT; }
bool Flux_IBLImpl::IsForceRoughness() const { return dbg_bIBLForceRoughness; }
float Flux_IBLImpl::GetForcedRoughness() const { return dbg_fIBLForcedRoughness; }

#ifdef ZENITH_TOOLS
void Flux_IBLImpl::RegisterDebugVariables()
{
	g_xEngine.DebugVariables().AddBoolean({ "Flux", "IBL", "ShowBRDFLUT" }, dbg_bIBLShowBRDFLUT);
	g_xEngine.DebugVariables().AddBoolean({ "Flux", "IBL", "ForceRoughness" }, dbg_bIBLForceRoughness);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "IBL", "ForcedRoughness" }, dbg_fIBLForcedRoughness, 0.0f, 1.0f);
	g_xEngine.DebugVariables().AddBoolean({ "Flux", "IBL", "RegenerateBRDFLUT" }, dbg_bIBLRegenerateBRDFLUT);

	g_xEngine.DebugVariables().AddTexture({ "Flux", "IBL", "Textures", "BRDF_LUT" }, m_xBRDFLUT.SRV());
	// Irradiance and Prefiltered maps are cubemaps (VK_IMAGE_VIEW_TYPE_CUBE).
	// ImGui's shader expects 2D textures, so these can't be displayed directly.
	// TODO: create per-face 2D SRVs for cubemap debug display.
}
#endif
