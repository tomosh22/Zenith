#include "Zenith.h"
#include "Flux/IBL/Flux_IBL_Shaders.h"
#include "Profiling/Zenith_Profiling.h"
#include "Core/Zenith_Engine.h"

#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/IBL.h" // typed binding handles
#include "Flux/Skybox/Flux_SkyboxImpl.h"
// MultiScatterConstants + BuildMultiScatterConstants: the capture bakes its own
// copy of the multiple-scattering LUT from its frozen snapshot.
#include "Flux/Skybox/Flux_AtmosphereTransmittance.h"
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

// The CPU pass-constant structs are hand-written (readable field names) but must
// match the Slang reflection byte-for-byte. Pin both ends here: FluxCompiler
// regenerates Flux_Generated_IBL from the .slang, so a layout edit that forgets
// one side fails to compile rather than uploading garbage.
static_assert(sizeof(Flux_IBLPassConstants::Irradiance)
	== sizeof(Flux_Generated_IBL::IBL_IrradianceConvolution::IrradianceConstants_CB),
	"IBL irradiance constants drifted from the generated Slang reflection");
static_assert(sizeof(Flux_IBLPassConstants::Prefilter)
	== sizeof(Flux_Generated_IBL::IBL_PrefilterEnvMap::PrefilterConstants_CB),
	"IBL prefilter constants drifted from the generated Slang reflection");
static_assert(sizeof(Flux_AtmosphereTransmittance::MultiScatterConstants)
	== sizeof(Flux_Generated_IBL::IBL_MultiScatterLUT::MultiScatterConstants_CB),
	"multi-scatter bake constants drifted from the generated Slang reflection");

float Flux_IBLImpl::GetCaptureCosThreshold()
{
	return Flux_IBLEnvironment::CosThresholdFromDegrees(
		Zenith_GraphicsOptions::Get().m_fIBLCaptureThresholdDegrees);
}

u_int Flux_IBLImpl::GetPassesPerFrame()
{
	// 0 would wedge the state machine (a generation that can never advance and
	// therefore never completes, so the schedule never drains).
	const uint32_t u = Zenith_GraphicsOptions::Get().m_uIBLPassesPerFrame;
	return (u < 1u) ? 1u : static_cast<u_int>(u);
}

DEBUGVAR bool dbg_bIBLShowBRDFLUT = false;
DEBUGVAR bool dbg_bIBLForceRoughness = false;
DEBUGVAR float dbg_fIBLForcedRoughness = 0.5f;
DEBUGVAR bool dbg_bIBLRegenerateBRDFLUT = false;

void Flux_IBLImpl::BuildPipelines()
{
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xBRDFLUTShader, m_xBRDFLUTPipeline,
		Flux_IBLShaders::xIBL_BRDFIntegration, m_xBRDFLUT.m_xSurfaceInfo.m_eFormat);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xMultiScatterLUTShader, m_xMultiScatterLUTPipeline,
		Flux_IBLShaders::xIBL_MultiScatterLUT, m_xMultiScatterLUT.m_xSurfaceInfo.m_eFormat);

	// Both buffers share format/size, so one pipeline serves either target.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xIrradianceConvolveShader, m_xIrradianceConvolvePipeline,
		Flux_IBLShaders::xIBL_IrradianceConvolution, m_axIrradianceMap[0].m_xSurfaceInfo.m_eFormat);

	Flux_PipelineHelper::BuildFullscreenPipeline(
		m_xPrefilterShader, m_xPrefilterPipeline,
		Flux_IBLShaders::xIBL_PrefilterEnvMap, m_axPrefilteredMap[0].m_xSurfaceInfo.m_eFormat);
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

	// Drop the environment capture: the next resolved environment re-seeds the
	// active target (a first generation, which writes BOTH buffers).
	m_bHasActiveEnvironment = false;
	m_bHasPendingEnvironment = false;
	m_bPublishPending = false;
	m_uFrontBufferIndex = 0u;
	m_uCompletedGenerations = 0u;
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
	// A first generation seeds BOTH buffers from one snapshot, so any half-run
	// publication is moot; drop the armed swap so front/back stay where they are.
	m_bPublishPending = false;
	// The captured target survives a recompile (the environment did not change);
	// only the in-flight cursor is rewound.
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
// in one frame, for BOTH buffers. Required so all mip levels have valid layouts
// before deferred shading binds the cubemap -- and, with double buffering, so
// the compile-time validator sees an enabled writer for each cube the consumers
// declare a Read of (they read both; only one is ever sampled). Both buffers
// are seeded from the SAME snapshot, so front and back start identical.
void Flux_IBLImpl::RunFirstGenerationFrame(Flux_IBLRegenFrameWork& xWork)
{
	for (u_int uFace = 0; uFace < 6; uFace++)
		xWork.m_abIrradiance[uFace] = true;
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		for (u_int uFace = 0; uFace < 6; uFace++)
			xWork.m_abPrefilter[uMip][uFace] = true;
	xWork.m_bWriteBothBuffers = true;
	m_bSkyIBLDirty = false;
	m_bFirstGeneration = false;
	m_eRegenState = IBL_REGEN_IDLE;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: First generation - processing all passes this frame");
}

// Amortised regeneration: process up to PASSES_PER_FRAME irradiance/prefilter
// passes per frame. Two phases — irradiance (6 faces) then prefilter
// (mip × face). State (g_xEngine.IBL().m_eRegenState, g_xEngine.IBL().m_uRegenFace, g_xEngine.IBL().m_uRegenMip) advances each
// frame; idle is reached after all faces of all mips have run.
void Flux_IBLImpl::AdvanceAmortizedRegen(Flux_IBLRegenFrameWork& xWork)
{
	const bool bWasIdle = m_eRegenState == IBL_REGEN_IDLE;
	const u_int uBudget = GetPassesPerFrame();
	for (u_int uPass = 0u; uPass < uBudget; uPass++)
	{
		const Flux_IBLRegen::Pass xPass = Flux_IBLRegen::Next(
			m_bSkyIBLDirty, m_eRegenState, m_uRegenFace, m_uRegenMip);
		if (xPass.m_eType == Flux_IBLRegen::PASS_NONE)
		{
			break;
		}
		if (xPass.m_eType == Flux_IBLRegen::PASS_IRRADIANCE)
		{
			xWork.m_abIrradiance[xPass.m_uFace] = true;
		}
		else
		{
			xWork.m_abPrefilter[xPass.m_uMip][xPass.m_uFace] = true;
		}
	}

	if (bWasIdle && m_eRegenState != IBL_REGEN_IDLE)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Starting amortized IBL regeneration");
	}
	if (!bWasIdle && m_eRegenState == IBL_REGEN_IDLE)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Completed amortized IBL regeneration");
	}
}

// ---------------------------------------------------------------------------
// Runtime environment capture.
// ---------------------------------------------------------------------------
void Flux_IBLImpl::RequestEnvironmentUpdate(const Flux_IBLEnvironmentSnapshot& xLive)
{
	if (!m_bHasActiveEnvironment)
	{
		// Boot / post-Reset: the IBL already starts dirty, so the first
		// generation simply adopts the first resolved environment. Marking
		// dirty here would be redundant, and would fight m_bFirstGeneration.
		m_xActiveEnvironment = xLive;
		m_bHasActiveEnvironment = true;
		m_bHasPendingEnvironment = false;
		return;
	}

	if (IsGenerationInFlight())
	{
		// NEVER disturb an in-flight generation's frozen snapshot. Restarting
		// on every live change is exactly what starved regeneration at low
		// frame rates (each MarkDirty rewound the 48-pass cursor to face 0
		// before it could finish). Coalesce into ONE latest pending target.
		if (Flux_IBLEnvironment::Differs(m_xActiveEnvironment, xLive, GetCaptureCosThreshold()))
		{
			m_xPendingEnvironment = xLive;
			m_bHasPendingEnvironment = true;
		}
		else
		{
			// The live environment drifted back onto the in-flight target --
			// there is nothing left to schedule after this generation.
			m_bHasPendingEnvironment = false;
		}
		return;
	}

	// Idle: m_xActiveEnvironment is the last COMPLETED capture, so displacement
	// accumulates against it. Comparing against the PREVIOUS FRAME (and then
	// overwriting that baseline every frame) is why a 0.05 deg/frame sun never
	// crossed the 0.081 deg threshold and the IBL never updated at 60 FPS.
	if (Flux_IBLEnvironment::Differs(m_xActiveEnvironment, xLive, GetCaptureCosThreshold()))
	{
		m_xActiveEnvironment = xLive;
		m_bHasPendingEnvironment = false;
		MarkAllProbesDirty();
	}
}

void Flux_IBLImpl::OnGenerationComplete()
{
	m_uCompletedGenerations++;

	// EVERYTHING here is deferred to the next frame's tick. The passes this
	// generation just enabled have NOT been recorded yet -- UpdateGraphPassEnables
	// runs before Compile/Execute -- so touching m_xActiveEnvironment now would
	// hand the completing frame's 8 passes the NEXT generation's sky, and
	// swapping the buffers now would send them to the cube consumers are reading.
	m_bPublishPending = true;
}

void Flux_IBLImpl::PublishCompletedGeneration()
{
	if (!m_bPublishPending)
	{
		return;
	}
	m_bPublishPending = false;

	// The back pair now holds a COMPLETE, coherent generation: publish it.
	m_uFrontBufferIndex = 1u - m_uFrontBufferIndex;

	if (!m_bHasPendingEnvironment)
	{
		return;
	}
	// Start the next generation from the LATEST coalesced target -- unless this
	// frame's live offer (RequestEnvironmentUpdate runs before this tick) has
	// already started a fresher one, in which case the coalesced target is stale.
	const bool bStillDiffers = Flux_IBLEnvironment::Differs(
		m_xActiveEnvironment, m_xPendingEnvironment, GetCaptureCosThreshold());
	m_bHasPendingEnvironment = false;
	if (bStillDiffers && !IsGenerationInFlight())
	{
		m_xActiveEnvironment = m_xPendingEnvironment;
		MarkAllProbesDirty();
	}
}

void Flux_IBLImpl::TickRegenerationForFrame(Flux_IBLRegenFrameWork& xWork)
{
	// Publish the generation that completed LAST frame (buffer swap + promotion
	// of the coalesced pending target) before resolving this frame's work. The
	// active snapshot is therefore only ever mutated BEFORE the frame's passes
	// are resolved and recorded, never between.
	PublishCompletedGeneration();

	// Latch ready once the BRDF LUT and at least one COMPLETE generation exist.
	// Deliberately NOT gated on being idle: under continuous day/night motion the
	// IBL is always mid-generation, and an idle-only latch would leave
	// DeferredShading/Translucency with m_bIBLEnabled=0 forever — i.e. no ambient
	// IBL at all for the whole day. The published front cubes are a complete
	// coherent capture the moment the first generation lands, which is exactly
	// what "ready" means.
	if (!m_bIBLReady && m_bBRDFLUTGenerated && m_uCompletedGenerations > 0u)
	{
		m_bIBLReady = true;
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: All IBL textures ready");
	}

	// Sky IBL state machine: idle → first-generation OR amortised regen.
	if (!IsGenerationInFlight())
	{
		return;
	}

	if (m_bFirstGeneration)
	{
		RunFirstGenerationFrame(xWork);
	}
	else
	{
		AdvanceAmortizedRegen(xWork);
	}

	if (!IsGenerationInFlight())
	{
		OnGenerationComplete();
	}
}

// Push resolved enable bits into the graph. SetEnabled no-ops when the bit
// hasn't changed (cheap in steady state); IBL passes have no explicit
// dependency edges, so this takes the m_bEnabledMaskDirty fast path rather
// than triggering a full recompile.
void Flux_IBLImpl::ApplyResolvedIBLEnables(Flux_RenderGraph& xGraph, bool bRunBRDF,
	const Flux_IBLRegenFrameWork& xWork)
{
	xGraph.SetEnabled(m_xBRDFLUTPassHandle, bRunBRDF);

	// The multi-scatter LUT runs on exactly the frames a convolution pass runs:
	// its readers are those passes, so the graph validator always sees an
	// enabled writer for the LUT they declare a Read of, and it is never baked
	// when nothing would sample it.
	xGraph.SetEnabled(m_xMultiScatterLUTPassHandle, xWork.HasWork());

	// A generation writes the BACK pair; a first generation seeds both.
	const u_int uBack = GetBackBufferIndex();
	for (u_int uBuffer = 0u; uBuffer < IBLConfig::uBUFFER_COUNT; uBuffer++)
	{
		const bool bBufferRuns = xWork.m_bWriteBothBuffers || uBuffer == uBack;
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			xGraph.SetEnabled(m_axIrradianceFacePassHandles[uBuffer][uFace],
				bBufferRuns && xWork.m_abIrradiance[uFace]);
		}
		for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		{
			for (u_int uFace = 0; uFace < 6; uFace++)
			{
				xGraph.SetEnabled(m_axPrefilterMipFacePassHandles[uBuffer][uMip][uFace],
					bBufferRuns && xWork.m_abPrefilter[uMip][uFace]);
			}
		}
	}
}

void Flux_IBLImpl::UpdateGraphPassEnables(Flux_RenderGraph& xGraph)
{
	ZENITH_PROFILE_SCOPE("Flux IBL Graph Setup");
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

	const bool bRunBRDF = ResolveBRDFLUTRun();
	Flux_IBLRegenFrameWork xWork;
	TickRegenerationForFrame(xWork);   // also publishes last frame's completed generation
	ApplyResolvedIBLEnables(xGraph, bRunBRDF, xWork);
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
	pxCmd->SetIndexBuffer(xFG.m_xQuadMesh.GetIndexBuffer());

	// BRDF integration only reads its UV input; the Slang version exposes no
	// CBs in reflection so no binder calls are needed before the draw.
	pxCmd->DrawIndexed(6);
	xIBL.m_bBRDFLUTGenerated = true;
}

void Flux_IBLImpl::ExecuteMultiScatterLUTPass(Flux_CommandBuffer* pxCmd, void*)
{
	Flux_IBLImpl& xIBL = g_xEngine.IBL();
	const Flux_IBLEnvironmentSnapshot& xEnv = xIBL.GetActiveEnvironment();

	// Baked from the FROZEN snapshot, like every other pass of the generation:
	// an authored atmosphere change mid-generation must not reach the faces
	// still being convolved.
	const Flux_AtmosphereTransmittance::MultiScatterConstants xConsts =
		Flux_AtmosphereTransmittance::BuildMultiScatterConstants(
			xEnv.m_fRayleighScale, xEnv.m_fMieScale,
			xEnv.m_fRayleighScaleHeight, xEnv.m_fMieScaleHeight,
			xEnv.m_fGroundAlbedo);

	auto& xFG = g_xEngine.FluxGraphics();
	pxCmd->SetPipeline(&xIBL.m_xMultiScatterLUTPipeline);
	pxCmd->SetIndexBuffer(xFG.m_xQuadMesh.GetIndexBuffer());

	{
		namespace MS = Flux_Generated_IBL::IBL_MultiScatterLUT;
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindDrawConstants(MS::hMultiScatterConstants, &xConsts, sizeof(xConsts));
		// No transmittance-LUT binding: the bake integrates the sun ray
		// analytically so it takes no dependency on the Skybox, which is
		// declared AFTER IBL in the feature setup walk (see the .slang).
	}
	pxCmd->DrawIndexed(6);
}

void Flux_IBLImpl::ExecuteIrradianceFacePass(Flux_CommandBuffer* pxCmd, void* pUserData)
{
	// Trampoline (non-capturing graph callback): recover the singleton first.
	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	const u_int uFace = *static_cast<const u_int*>(pUserData);

	// Every input comes from the generation's FROZEN snapshot -- no live sun
	// direction out of the VIEW set, no live Skybox getter. That is what stops
	// a 6-frame generation mixing faces convolved from two different skies.
	const Flux_IBLPassConstants::Irradiance xConsts =
		Flux_IBLPassConstants::BuildIrradiance(xIBL.GetActiveEnvironment(), uFace);

	auto& xFG = g_xEngine.FluxGraphics();
	pxCmd->SetPipeline(&xIBL.m_xIrradianceConvolvePipeline);
	pxCmd->SetIndexBuffer(xFG.m_xQuadMesh.GetIndexBuffer());

	{
		namespace IC = Flux_Generated_IBL::IBL_IrradianceConvolution;
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindDrawConstants(IC::hIrradianceConstants, &xConsts, sizeof(xConsts));
		if (Zenith_TextureAsset* pxCubemap = xFG.m_xCubemapTexture.GetDirect())
			xBinder.BindSRV(IC::hg_xSkyboxCubemap, &pxCubemap->GetSRV());
		else if (Zenith_TextureAsset* pxBlack = xFG.m_xBlackTexture.GetDirect())
			xBinder.BindSRV(IC::hg_xSkyboxCubemap, &pxBlack->GetSRV());
		xBinder.BindSRV(IC::hg_xMultiScatterLUT, &xIBL.m_xMultiScatterLUT.SRV());
	}
	pxCmd->DrawIndexed(6);
}

void Flux_IBLImpl::ExecutePrefilterMipFacePass(Flux_CommandBuffer* pxCmd, void* pUserData)
{
	// Trampoline (non-capturing graph callback): recover the singleton first.
	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	const IBLPrefilterPassData* pxData = static_cast<const IBLPrefilterPassData*>(pUserData);

	// Same frozen-snapshot rule as the irradiance pass (see above).
	const Flux_IBLPassConstants::Prefilter xConsts = Flux_IBLPassConstants::BuildPrefilter(
		xIBL.GetActiveEnvironment(), pxData->m_uMip, pxData->m_uFace);

	auto& xFG = g_xEngine.FluxGraphics();
	pxCmd->SetPipeline(&xIBL.m_xPrefilterPipeline);
	pxCmd->SetIndexBuffer(xFG.m_xQuadMesh.GetIndexBuffer());

	{
		namespace PF = Flux_Generated_IBL::IBL_PrefilterEnvMap;
		Flux_ShaderBinder xBinder(*pxCmd);
		xBinder.BindDrawConstants(PF::hPrefilterConstants, &xConsts, sizeof(xConsts));
		if (Zenith_TextureAsset* pxCubemap = xFG.m_xCubemapTexture.GetDirect())
			xBinder.BindSRV(PF::hg_xSkyboxCubemap, &pxCubemap->GetSRV());
		else if (Zenith_TextureAsset* pxBlack = xFG.m_xBlackTexture.GetDirect())
			xBinder.BindSRV(PF::hg_xSkyboxCubemap, &pxBlack->GetSRV());
		xBinder.BindSRV(PF::hg_xMultiScatterLUT, &xIBL.m_xMultiScatterLUT.SRV());
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

	// The capture's multiple-scattering LUT. Declared BEFORE the convolution
	// passes that read it so producer-before-consumer holds inside IBL's own
	// setup walk. Enabled on exactly the frames a generation runs (it is baked
	// from the immutable snapshot, so re-baking it on each of a generation's
	// frames produces identical output and costs one 32x32 draw).
	m_xMultiScatterLUTPassHandle = xGraph.AddPass("IBL Multi-Scatter LUT", ExecuteMultiScatterLUTPass)
		.ClearTargets()
		.Writes(m_xMultiScatterLUT, RESOURCE_ACCESS_WRITE_RTV);

	// 6 irradiance face passes PER BUFFER — each writes layer N of one
	// irradiance cubemap. The double buffer means both cubes own a full pass
	// set; UpdateGraphPassEnables only ever enables the back one (or both, on a
	// first generation).
	static const char* const s_aszIrradianceFaceNames[IBLConfig::uBUFFER_COUNT][6] = {
		{ "IBL Irradiance B0 Face 0", "IBL Irradiance B0 Face 1", "IBL Irradiance B0 Face 2",
		  "IBL Irradiance B0 Face 3", "IBL Irradiance B0 Face 4", "IBL Irradiance B0 Face 5" },
		{ "IBL Irradiance B1 Face 0", "IBL Irradiance B1 Face 1", "IBL Irradiance B1 Face 2",
		  "IBL Irradiance B1 Face 3", "IBL Irradiance B1 Face 4", "IBL Irradiance B1 Face 5" }
	};
	for (u_int uBuffer = 0; uBuffer < IBLConfig::uBUFFER_COUNT; uBuffer++)
	{
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			// Per-(mip 0, face uFace) slice write — the graph picks the per-(mip, face)
			// RTV and emits a tight subresource barrier. The face index is the only
			// per-pass payload, so both buffers' passes share the same stable storage.
			m_axIrradianceFacePassHandles[uBuffer][uFace] = xGraph.AddPass(
					s_aszIrradianceFaceNames[uBuffer][uFace], ExecuteIrradianceFacePass,
					&m_auIrradianceFaceData[uFace])
				.ClearTargets()
				.Reads (m_xMultiScatterLUT, RESOURCE_ACCESS_READ_SRV)
				.Writes(m_axIrradianceMap[uBuffer], RESOURCE_ACCESS_WRITE_RTV, 0, 1, uFace, 1);
		}
	}

	// 42 prefilter mip-face passes PER BUFFER — each writes one (mip, face)
	// slot of one prefiltered cubemap.
	static const char* const s_aszPrefilterPassNames[IBLConfig::uBUFFER_COUNT][IBLConfig::uPREFILTER_MIP_COUNT * 6] = {
		{
			"IBL Prefilter B0 M0 F0", "IBL Prefilter B0 M0 F1", "IBL Prefilter B0 M0 F2", "IBL Prefilter B0 M0 F3", "IBL Prefilter B0 M0 F4", "IBL Prefilter B0 M0 F5",
			"IBL Prefilter B0 M1 F0", "IBL Prefilter B0 M1 F1", "IBL Prefilter B0 M1 F2", "IBL Prefilter B0 M1 F3", "IBL Prefilter B0 M1 F4", "IBL Prefilter B0 M1 F5",
			"IBL Prefilter B0 M2 F0", "IBL Prefilter B0 M2 F1", "IBL Prefilter B0 M2 F2", "IBL Prefilter B0 M2 F3", "IBL Prefilter B0 M2 F4", "IBL Prefilter B0 M2 F5",
			"IBL Prefilter B0 M3 F0", "IBL Prefilter B0 M3 F1", "IBL Prefilter B0 M3 F2", "IBL Prefilter B0 M3 F3", "IBL Prefilter B0 M3 F4", "IBL Prefilter B0 M3 F5",
			"IBL Prefilter B0 M4 F0", "IBL Prefilter B0 M4 F1", "IBL Prefilter B0 M4 F2", "IBL Prefilter B0 M4 F3", "IBL Prefilter B0 M4 F4", "IBL Prefilter B0 M4 F5",
			"IBL Prefilter B0 M5 F0", "IBL Prefilter B0 M5 F1", "IBL Prefilter B0 M5 F2", "IBL Prefilter B0 M5 F3", "IBL Prefilter B0 M5 F4", "IBL Prefilter B0 M5 F5",
			"IBL Prefilter B0 M6 F0", "IBL Prefilter B0 M6 F1", "IBL Prefilter B0 M6 F2", "IBL Prefilter B0 M6 F3", "IBL Prefilter B0 M6 F4", "IBL Prefilter B0 M6 F5",
		},
		{
			"IBL Prefilter B1 M0 F0", "IBL Prefilter B1 M0 F1", "IBL Prefilter B1 M0 F2", "IBL Prefilter B1 M0 F3", "IBL Prefilter B1 M0 F4", "IBL Prefilter B1 M0 F5",
			"IBL Prefilter B1 M1 F0", "IBL Prefilter B1 M1 F1", "IBL Prefilter B1 M1 F2", "IBL Prefilter B1 M1 F3", "IBL Prefilter B1 M1 F4", "IBL Prefilter B1 M1 F5",
			"IBL Prefilter B1 M2 F0", "IBL Prefilter B1 M2 F1", "IBL Prefilter B1 M2 F2", "IBL Prefilter B1 M2 F3", "IBL Prefilter B1 M2 F4", "IBL Prefilter B1 M2 F5",
			"IBL Prefilter B1 M3 F0", "IBL Prefilter B1 M3 F1", "IBL Prefilter B1 M3 F2", "IBL Prefilter B1 M3 F3", "IBL Prefilter B1 M3 F4", "IBL Prefilter B1 M3 F5",
			"IBL Prefilter B1 M4 F0", "IBL Prefilter B1 M4 F1", "IBL Prefilter B1 M4 F2", "IBL Prefilter B1 M4 F3", "IBL Prefilter B1 M4 F4", "IBL Prefilter B1 M4 F5",
			"IBL Prefilter B1 M5 F0", "IBL Prefilter B1 M5 F1", "IBL Prefilter B1 M5 F2", "IBL Prefilter B1 M5 F3", "IBL Prefilter B1 M5 F4", "IBL Prefilter B1 M5 F5",
			"IBL Prefilter B1 M6 F0", "IBL Prefilter B1 M6 F1", "IBL Prefilter B1 M6 F2", "IBL Prefilter B1 M6 F3", "IBL Prefilter B1 M6 F4", "IBL Prefilter B1 M6 F5",
		}
	};
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
	{
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			m_axPrefilterPassData[uMip][uFace].m_uMip = uMip;
			m_axPrefilterPassData[uMip][uFace].m_uFace = uFace;
		}
	}
	for (u_int uBuffer = 0; uBuffer < IBLConfig::uBUFFER_COUNT; uBuffer++)
	{
		for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		{
			for (u_int uFace = 0; uFace < 6; uFace++)
			{
				m_axPrefilterMipFacePassHandles[uBuffer][uMip][uFace] = xGraph.AddPass(
						s_aszPrefilterPassNames[uBuffer][uMip * 6 + uFace],
						ExecutePrefilterMipFacePass, &m_axPrefilterPassData[uMip][uFace])
					.ClearTargets()
					.Reads (m_xMultiScatterLUT, RESOURCE_ACCESS_READ_SRV)
					.Writes(m_axPrefilteredMap[uBuffer], RESOURCE_ACCESS_WRITE_RTV, uMip, 1, uFace, 1);
			}
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

	// The capture's own multiple-scattering LUT (32x32 RGBA16F, ~8 KB), baked
	// from the frozen generation snapshot rather than shared with the Skybox's
	// live-medium copy -- see Flux_AtmosphereTransmittance::MultiScatterConstants.
	xBuilder.m_uWidth = AtmosphereConfig::uMULTISCATTER_LUT_SIZE;
	xBuilder.m_uHeight = AtmosphereConfig::uMULTISCATTER_LUT_SIZE;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.BuildColour(m_xMultiScatterLUT, "IBL Multi-Scatter LUT");

	// Irradiance map - cubemap for diffuse IBL (double buffered: see the
	// "Coherent publication" note on the members).
	xBuilder.m_uWidth = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_uHeight = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	static const char* const s_aszIrradianceNames[IBLConfig::uBUFFER_COUNT] = {
		"IBL Irradiance Map 0", "IBL Irradiance Map 1" };
	for (u_int uBuffer = 0; uBuffer < IBLConfig::uBUFFER_COUNT; uBuffer++)
	{
		xBuilder.BuildColourCubemap(m_axIrradianceMap[uBuffer], s_aszIrradianceNames[uBuffer]);
	}

	// Prefiltered environment map - cubemap for specular IBL (with mip chain for roughness levels)
	xBuilder.m_uWidth = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uHeight = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uNumMips = IBLConfig::uPREFILTER_MIP_COUNT;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	static const char* const s_aszPrefilteredNames[IBLConfig::uBUFFER_COUNT] = {
		"IBL Prefiltered Map 0", "IBL Prefiltered Map 1" };
	for (u_int uBuffer = 0; uBuffer < IBLConfig::uBUFFER_COUNT; uBuffer++)
	{
		xBuilder.BuildColourCubemap(m_axPrefilteredMap[uBuffer], s_aszPrefilteredNames[uBuffer]);
	}
}

void Flux_IBLImpl::DestroyRenderTargets()
{
	// Route through the builder so per-mip RTVs / SRVs / UAVs for multi-mip
	// attachments (prefiltered cube has 7 mips × 6 faces = 42 RTVs alone) get
	// released — a hand-rolled loop here previously STUBBED the cube path and
	// only queued mip 0 for the 2D path, leaking GPU memory every Shutdown.
	Flux_RenderAttachmentBuilder::Destroy(m_xBRDFLUT);
	Flux_RenderAttachmentBuilder::Destroy(m_xMultiScatterLUT);
	for (u_int uBuffer = 0; uBuffer < IBLConfig::uBUFFER_COUNT; uBuffer++)
	{
		Flux_RenderAttachmentBuilder::Destroy(m_axIrradianceMap[uBuffer]);
		Flux_RenderAttachmentBuilder::Destroy(m_axPrefilteredMap[uBuffer]);
	}
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

// Schedule a regeneration. Runtime callers do NOT call this directly any more:
// the one runtime invalidation path is RequestEnvironmentUpdate, which owns the
// accumulate/coalesce/never-restart schedule. (The old duplicate spelling of
// this, UpdateSkyIBL(), lost its last caller with that change and was deleted.)
void Flux_IBLImpl::MarkAllProbesDirty()
{
	Flux_IBLRegen::MarkDirty(m_bFirstGeneration, m_bSkyIBLDirty,
		m_eRegenState, m_uRegenFace, m_uRegenMip);
}

// Accessors - return const references to prevent modification and signal temporary nature
const Flux_ShaderResourceView& Flux_IBLImpl::GetBRDFLUTSRV()
{
	return m_xBRDFLUT.SRV();
}

// The FRONT cube: the last generation that completed in full. Flux.cpp rewrites
// the persistent VIEW-set image every frame, so a publication swap is absorbed
// without any descriptor bookkeeping here.
const Flux_ShaderResourceView& Flux_IBLImpl::GetIrradianceMapSRV()
{
	return m_axIrradianceMap[m_uFrontBufferIndex].SRV();
}

const Flux_ShaderResourceView& Flux_IBLImpl::GetPrefilteredMapSRV()
{
	return m_axPrefilteredMap[m_uFrontBufferIndex].SRV();
}

void Flux_IBLImpl::DeclareConsumerReads(Flux_RenderGraph& xGraph, Flux_PassHandle xPass)
{
	// Cubemap reads default to FLUX_RG_ALL_MIPS / FLUX_RG_ALL_LAYERS.
	xGraph.Read(xPass, m_xBRDFLUT, RESOURCE_ACCESS_READ_SRV);
	for (u_int uBuffer = 0; uBuffer < IBLConfig::uBUFFER_COUNT; uBuffer++)
	{
		xGraph.Read(xPass, m_axIrradianceMap[uBuffer],  RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(xPass, m_axPrefilteredMap[uBuffer], RESOURCE_ACCESS_READ_SRV);
	}
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

#include "Flux/IBL/Flux_IBL.Tests.inl"
