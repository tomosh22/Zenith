#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"                     // the nine feature toggles pinned in Setup

#include "Flux/Flux_RendererImpl.h"                          // GetRenderGraph / RequestGraphRebuild
#include "Flux/Flux_GraphicsImpl.h"                          // RenderViews()
#include "Flux/RenderGraph/Flux_RenderGraph.h"               // GetPasses / Flux_RenderGraph_Pass / violations
#include "Flux/RenderViews/Flux_RenderViews.h"               // slot constants + FluxRenderViewType
#include "Flux/RenderViews/Flux_ViewPassNames.h"             // Flux_ViewSlotSuffix (D0-A)
#include "Flux/RenderViews/Flux_MaterialPreviewController.h" // complete type for SetActive/IsActive
#include "Flux/HiZ/Flux_HiZImpl.h"                           // GetMipCount (the main view's chain length)

#include <cstring>

// =====================================================================
// RT_RenderGraphViewStructure (unit D0-T) — THE ORACLE, AS A TEST.
//
// The multi-view render graph has a naming law that nothing enforced: a
// per-view pass chain instantiated for a full-pipeline PREVIEW view must be
// named "<base> (<suffix>)" where <suffix> is that slot's entry in the
// Flux_ViewPassNames suffix table (D0-A), and every such <base> must also
// exist, exactly once, as a slot-0 pass under its bare name. The law is what
// makes "the preview view runs the SAME pipeline the main view runs" a
// checkable statement rather than a claim; today it is upheld only by ~45
// hand-written string literals scattered across a dozen features, each of
// which is one typo away from silently naming a pass into a shape no tool can
// pair with its main-view twin.
//
// This test reads the LIVE compiled graph and states the law out loud, in
// eight named clauses, against four samples of the view set:
//
//   A  main view only                          (both preview views inactive)
//   B  main + the material-preview view        (kuFluxViewSlotPreviewMaterial)
//   C  main + BOTH preview views               (+ kuFluxViewSlotPreviewAnim)
//   D  both preview views off again, settled   (the graph must return to A)
//
// Sample C is LIVE as of unit D2-b: slot 6 is a constructed PREVIEW-typed
// full-pipeline view, it owns its own persistent LDR, and every clause below runs
// over it. It was staged behind two constants while the slot did not exist; both
// are now true and the staging is gone.
//
// Every clause runs and reports on EVERY run (the Test_TennisBrainContract
// accumulator shape), so one failing run names every clause that moved rather
// than stopping at the first.
//
// DRIVER. g_xEngine.MaterialPreview().SetActive(true) is the whole of the
// slot-5 driver: Flux_MaterialPreviewController::Update() runs every frame
// from Flux_RendererImpl::SyncUnifiedBucketsFromSnapshot
// (Flux/Flux_GPUSceneBuilder.cpp:114-116, ZENITH_TOOLS) and both activates the
// view (SetViewActive + RequestGraphRebuild) and stages its 512x512 target
// dims (Flux_MaterialPreviewController.cpp:215-233). All of that is
// backend-neutral — no device call — so this test runs headless on Null.
// ★ The controller SELF-DEACTIVATES after kuLIVENESS_GRACE_FRAMES = 8 frames
// without a fresh SetActive(true) (.cpp:147-154, .h:233), which is why sample
// B refreshes it on EVERY Step frame rather than once.
//
// FRAME NUMBERS ARE NOT THE CONTRACT, SETTLE COUNTS ARE. A Step runs BEFORE
// that frame's SubmitRenderWork (Zenith_Core.cpp:394 vs :243), and
// SubmitRenderWork is SKIPPED entirely on frames the editor declines to render
// (Zenith_Core.cpp:162/:247) — so "the graph N frames after the edge" is not a
// fixed quantity. Each phase therefore runs kuPHASE_FRAMES and samples on the
// first frame at or after kuSETTLE_FRAMES on which the graph is not dirty;
// nothing keys off an exact frame index.
// =====================================================================

namespace
{
	// ------------------------------------------------------------------
	// Failure accumulator (Test_TennisBrainContract.cpp:76-124). Every check
	// runs and reports, so ONE run names EVERY clause that moved.
	// ------------------------------------------------------------------
	int g_iChecks = 0;
	int g_iFailures = 0;

	int g_iClauseChecksAtStart = 0;
	int g_iClauseFailuresAtStart = 0;
	const char* g_szClause = "";

	void ResetResults()
	{
		g_iChecks = 0;
		g_iFailures = 0;
		g_iClauseChecksAtStart = 0;
		g_iClauseFailuresAtStart = 0;
		g_szClause = "";
	}

	void CheckTrue(bool bCondition, const char* szWhat)
	{
		++g_iChecks;
		if (!bCondition)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] FAILED (%s): %s", g_szClause, szWhat);
		}
	}

	void CheckTrueNamed(bool bCondition, const char* szWhat, const char* szSubject)
	{
		++g_iChecks;
		if (!bCondition)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] FAILED (%s): %s -- '%s'",
				g_szClause, szWhat, szSubject ? szSubject : "<null>");
		}
	}

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] FAILED (%s): %s (expected %d, got %d)",
				g_szClause, szWhat, iExpected, iActual);
		}
	}

	void CheckEqIntNamed(int iActual, int iExpected, const char* szWhat, const char* szSubject)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] FAILED (%s): %s -- '%s' (expected %d, got %d)",
				g_szClause, szWhat, szSubject ? szSubject : "<null>", iExpected, iActual);
		}
	}

	void CheckEqStr(const char* szActual, const char* szExpected, const char* szWhat)
	{
		++g_iChecks;
		const bool bEqual = (szActual != nullptr) && (std::strcmp(szActual, szExpected) == 0);
		if (!bEqual)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] FAILED (%s): %s\n  expected: %s\n  actual:   %s",
				g_szClause, szWhat, szExpected, szActual ? szActual : "<null>");
		}
	}

	void BeginClause(const char* szClause)
	{
		g_szClause = szClause;
		g_iClauseChecksAtStart = g_iChecks;
		g_iClauseFailuresAtStart = g_iFailures;
	}

	void EndClause()
	{
		const int iClauseChecks = g_iChecks - g_iClauseChecksAtStart;
		const int iClauseFailures = g_iFailures - g_iClauseFailuresAtStart;
		Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] CLAUSE %-2s %-4s  %d checks, %d failed",
			g_szClause, (iClauseFailures == 0 && iClauseChecks > 0) ? "OK" : (iClauseChecks == 0 ? "VOID" : "FAIL"),
			iClauseChecks, iClauseFailures);
		// A clause that asserted nothing cannot fail — count that as a failure so a
		// broken rig can never masquerade as green (same rule as ReportContract).
		if (iClauseChecks == 0)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] FAILED (%s): the clause ran ZERO checks", g_szClause);
		}
		g_szClause = "";
	}

	// ------------------------------------------------------------------
	// THE GOLDEN LIST. Every per-view BASE the tree instantiates for a
	// full-pipeline preview view, derived from the AddPass call sites (not from
	// prose — the "41" at Flux_MaterialPreviewController.h:160 and the "~40" at
	// Zenith_EditorPanel_MaterialEditor.cpp:522 are stale narration and are NOT
	// sources). The count is taken from sizeof below; it is never hand-typed.
	//
	// Trailing comment = THE SYMBOL that owns the AddPass call, not a file:line.
	// The line numbers this list carried were stale within three units of being
	// written (D1 moved every per-view AddPass into a walk callback), and a stale
	// citation is worse than none: it sends a reader to an unrelated line and
	// reads as evidence while doing it. A function name survives the file moving.
	// ------------------------------------------------------------------
	const char* const kaszPERVIEW_BASES[] =
	{
		"Unified Mesh GBuffer",         // DeclareUnifiedGBufferPass (Flux_UnifiedMesh.cpp) — ONE space each side
		"Skybox Sky-View LUT",          // Flux_SkyboxImpl::SetupViewPasses
		"Skybox",                       // Flux_SkyboxImpl::SetupViewPasses
		"Decal Normals Copy",           // Flux_DecalsImpl::SetupViewPasses — ONE per-view site
		"Decal Apply",                  // Flux_DecalsImpl::SetupViewPasses — ONE per-view site
		"SSAO Generate",                // Flux_SSAOImpl::SetupViewPasses
		"SSAO Blur Legacy",             // Flux_SSAOImpl::SetupViewPasses
		"SSAO Blur H",                  // Flux_SSAOImpl::SetupViewPasses
		"SSAO Blur",                    // Flux_SSAOImpl::SetupViewPasses
		"HiZ Mip 0",                    // Flux_HiZImpl::SetupViewPasses — 10 rows at 512^2,
		"HiZ Mip 1",                    //   ComputeMipCount(512,512) = 10; the main view's
		"HiZ Mip 2",                    //   chain is 11 on the fixed 1280x720 Null swapchain
		"HiZ Mip 3",                    //
		"HiZ Mip 4",                    //
		"HiZ Mip 5",                    //
		"HiZ Mip 6",                    //
		"HiZ Mip 7",                    //
		"HiZ Mip 8",                    //
		"HiZ Mip 9",                    //
		"SSR RayMarch",                 // Flux_SSRImpl::SetupViewPasses
		"SSR Upsample",                 // Flux_SSRImpl::SetupViewPasses
		"SSR DenoiseH",                 // Flux_SSRImpl::SetupViewPasses — no space, unlike SSGI's
		"SSR DenoiseV",                 // Flux_SSRImpl::SetupViewPasses
		"SSGI RayMarch",                // Flux_SSGIImpl::SetupViewPasses
		"SSGI Upsample",                // Flux_SSGIImpl::SetupViewPasses
		"SSGI Denoise H",               // Flux_SSGIImpl::SetupViewPasses — space, unlike SSR's
		"SSGI Denoise V",               // Flux_SSGIImpl::SetupViewPasses
		"Apply Lighting",               // Flux_DeferredShadingImpl::SetupViewPasses — ONE per-view site
		"Fog_Simple",                   // Flux_FogImpl::SetupViewPasses
		"Particles",                    // Flux_ParticlesImpl::SetupViewPasses
		"SDFs",                         // Flux_SDFsImpl::SetupViewPasses
		"Translucency",                 // Flux_TranslucencyImpl::SetupViewPasses
		"HDR_BloomThreshold",           // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_BloomDownsample Mip1",     // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_BloomDownsample Mip2",     // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_BloomDownsample Mip3",     // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_BloomDownsample Mip4",     // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_BloomUpsample Mip3",       // Flux_HDRImpl::SetupBloomViewPasses — declared high mip first
		"HDR_BloomUpsample Mip2",       // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_BloomUpsample Mip1",       // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_BloomUpsample Mip0",       // Flux_HDRImpl::SetupBloomViewPasses
		"HDR_ToneMapping",              // Flux_HDRImpl::SetupPreviewViewPasses
	};
	constexpr u_int kuPERVIEW_BASE_COUNT =
		static_cast<u_int>(sizeof(kaszPERVIEW_BASES) / sizeof(kaszPERVIEW_BASES[0]));

	// The four rows that are the pipeline: geometry in, lighting, sky, tonemap
	// out. Asserted present REGARDLESS of the golden list, so a future edit that
	// legitimately shrinks the list cannot quietly delete the pipeline itself.
	const char* const kaszALWAYS_ON[] =
	{
		"Unified Mesh GBuffer",
		"Apply Lighting",
		"Skybox",
		"HDR_ToneMapping",
	};
	constexpr u_int kuALWAYS_ON_COUNT =
		static_cast<u_int>(sizeof(kaszALWAYS_ON) / sizeof(kaszALWAYS_ON[0]));

	// The ONE preview pass whose name is not "<base> (<suffix>)". It spells the
	// suffix as a PREFIX and is matched by its FULL literal
	// (Flux/RenderViews/Flux_ViewPassNames.cpp:79-82 keeps it as the single
	// legacy row). Unit D1-e landed: the pass is now declared with
	// .View(uViewSlot) and named through Flux_ViewPassName("LDR Transition",
	// uViewSlot) (Flux_HDR.cpp:746), so it RECORDS ON THE PREVIEW SLOT and its
	// base is "LDR Transition". Clause 4 pins exactly that below — the literal is
	// absent from slot 0 and present once on the preview slot.
	//
	// kszPREVIEW_ONLY_BASE / IsPreviewOnlyBase are LIVE as of D2-b. Clause 2
	// short-circuits on the literal before it ever strips a suffix, so the base
	// spelling was unreachable while only the material preview existed; the
	// animation preview has NO legacy row, so its transition pass composes
	// "LDR Transition (AnimPreview)", strips to this base, and is owed no slot-0
	// twin. That is the path IsPreviewOnlyBase exists for.
	const char* const kszPREVIEW_ONLY_LITERAL = "Preview LDR Transition";
	const char* const kszPREVIEW_ONLY_BASE    = "LDR Transition";

	// Bases that legitimately have NO slot-0 twin, because the pass exists only
	// for a preview view. Exactly one today.
	bool IsPreviewOnlyBase(const char* szBase)
	{
		return szBase != nullptr && std::strcmp(szBase, kszPREVIEW_ONLY_BASE) == 0;
	}

	// ------------------------------------------------------------------
	// Sample C. Both switches were staging while the animation-preview view did
	// not exist; D2-a constructed the slot and D0-B gave it its own persistent
	// LDR, so both are TRUE and sample C is part of the contract.
	//
	// They are kept as named constants rather than deleted because they are what
	// the two halves of the contract are called in the phase machine below:
	// kbSampleCEnabled is "run the phase at all", kbSampleCFullClauses is "hold
	// the naming law (2), the slot-0 stability (3) and the per-view inventory (5)
	// over the animation preview as well". Turning either off is how you bisect a
	// failure onto the second preview view; neither is a place to park one.
	// ------------------------------------------------------------------
	constexpr bool kbSampleCEnabled = true;
	constexpr bool kbSampleCFullClauses = true;
	static_assert(kbSampleCEnabled || !kbSampleCFullClauses,
		"the full clauses are held over sample C — they cannot be on while the sample is off");

	// The animation preview's slot comes from the registry's own constant. This
	// file used to compute it as "the material preview's slot + 1" while the real
	// constant did not exist; that provisional is gone and there is one spelling.
	static_assert(Flux_IsPreviewViewSlot(kuFluxViewSlotPreviewAnim),
		"sample C drives a slot the registry must agree is PREVIEW-class");
	static_assert(kuFluxViewNumPreviewSlots == 2u,
		"sample C is written for exactly two preview views — a third needs its own phase");

	// ------------------------------------------------------------------
	// Per-sample capture. Pass names are static-lifetime literals (the graph only
	// ever stores the pointer — Flux_RenderGraph.h:32-40), so a sample may hold
	// the pointers across a rebuild without copying the bytes.
	// ------------------------------------------------------------------
	constexpr u_int kuMAX_SAMPLED_SLOT0_PASSES   = 512u;
	constexpr u_int kuMAX_SAMPLED_PREVIEW_PASSES = 256u;
	constexpr u_int kuMAX_SAMPLED_SHADOW_PASSES  = 8u;
	// The longest composed name the pool will ever hold; a base is shorter still.
	constexpr u_int kuBASE_BUFFER_LEN = kuFLUX_VIEW_PASS_NAME_MAX_LEN;

	struct ViewStructureShadowSlot
	{
		u_int       m_uCount = 0u;
		const char* m_aszNames[kuMAX_SAMPLED_SHADOW_PASSES] = {};
	};

	struct ViewStructureSample
	{
		const char* m_szLabel = "";
		bool        m_bTaken = false;

		u_int m_uTotalPasses = 0u;
		u_int m_uExecutionOrderSize = 0u;
		u_int m_uViolations = 0u;
		bool  m_bDirtyAtSample = false;
		bool  m_bCompiled = false;
		bool  m_bPreviewViewActive = false;
		bool  m_bPreviewAnimViewActive = false;
		u_int m_uMainHiZMipCount = 0u;
		u_int m_uOverflowed = 0u;      // passes dropped because a capture array filled
		// Passes on a slot this sampler cannot classify: a non-main slot whose
		// registered view type is neither SHADOW_CASCADE nor PREVIEW. It is counted
		// (and asserted zero) rather than ignored because that is EXACTLY the shape
		// a new preview slot arrives in if the registry constructor does not TYPE
		// it: a default-constructed Flux_RenderView is FLUX_RENDER_VIEW_MAIN, so its
		// passes would be invisible to clause 2 and the naming law would silently
		// stop covering them. The animation preview is typed (D2-a), which is what
		// lets sample C see its passes at all.
		u_int m_uUnclassified = 0u;

		// EVERY slot-0 pass name, duplicates included (clause 4 counts them).
		const char* m_aszSlot0Names[kuMAX_SAMPLED_SLOT0_PASSES] = {};
		u_int       m_uSlot0Count = 0u;

		// Every pass whose VIEW TYPE is FLUX_RENDER_VIEW_PREVIEW, with its slot.
		const char* m_aszPreviewNames[kuMAX_SAMPLED_PREVIEW_PASSES] = {};
		u_int       m_auPreviewSlots[kuMAX_SAMPLED_PREVIEW_PASSES] = {};
		u_int       m_uPreviewCount = 0u;

		// Slots 1..kuFluxViewNumShadowSlots, indexed by cascade (slot - 1).
		ViewStructureShadowSlot m_axShadowSlots[kuFluxViewNumShadowSlots];

		// Total occurrences of kszPREVIEW_ONLY_LITERAL anywhere in the graph.
		u_int m_uLegacyLiteralCount = 0u;
	};

	enum ViewStructurePhase : u_int
	{
		VIEW_STRUCTURE_PHASE_A_MAIN_ONLY = 0,
		VIEW_STRUCTURE_PHASE_B_PREVIEW,
		VIEW_STRUCTURE_PHASE_C_PREVIEW_ANIM,
		VIEW_STRUCTURE_PHASE_D_SETTLED_OFF,
		VIEW_STRUCTURE_PHASE_COUNT,
		VIEW_STRUCTURE_PHASE_DONE = VIEW_STRUCTURE_PHASE_COUNT,
	};

	ViewStructureSample s_axSamples[VIEW_STRUCTURE_PHASE_COUNT];
	ViewStructurePhase  s_ePhase = VIEW_STRUCTURE_PHASE_A_MAIN_ONLY;
	u_int               s_uPhaseFrame = 0u;

	// Frames per phase, and the earliest in-phase frame a sample may be taken.
	// Both are settle counts, not schedule points — see the header note.
	constexpr u_int kuSETTLE_FRAMES = 24u;
	constexpr u_int kuPHASE_FRAMES  = 40u;

	// ------------------------------------------------------------------
	// Saved graphics options (Setup pins, Teardown restores).
	//
	// ★ THE PIN IS INSURANCE, NOT THE SOURCE OF EXACTNESS. Today NOT ONE of
	// these nine flags gates an AddPass: every feature's SetupRenderGraph is
	// called unconditionally by the registry walk (Flux_FeatureRegistry.cpp:142),
	// and the flags are read either inside an Execute (e.g.
	// Flux_Shadows.cpp:194) or through SetEnabled, which flips m_bEnabled on a
	// pass that has ALREADY been added and therefore still appears in
	// GetPasses() (Flux_SSAO.cpp:424-426, Flux_SSR.cpp:711-712,
	// Flux_SSGI.cpp:469-470). So the golden list below is the same list either
	// way. The pin exists so that if a future toggle ever DOES gate an AddPass,
	// this test fails on that change instead of quietly re-reading the golden
	// list as a permission slip. There is deliberately no Decals toggle to pin —
	// Zenith_GraphicsOptions has none.
	// ------------------------------------------------------------------
	struct ViewStructureSavedOptions
	{
		bool m_bValid = false;
		bool m_bHiZEnabled = true;
		bool m_bSSAOEnabled = true;
		bool m_bSSREnabled = true;
		bool m_bSSGIEnabled = false;
		bool m_bFogEnabled = true;
		bool m_bCPUParticlesEnabled = true;
		bool m_bGPUParticlesEnabled = true;
		bool m_bSDFsEnabled = true;
		bool m_bTranslucencyEnabled = true;
	};
	ViewStructureSavedOptions s_xSavedOptions;

	void PinGraphicsOptions()
	{
		Zenith_GraphicsOptions& xOptions = Zenith_GraphicsOptions::Get();
		s_xSavedOptions.m_bHiZEnabled          = xOptions.m_bHiZEnabled;
		s_xSavedOptions.m_bSSAOEnabled         = xOptions.m_bSSAOEnabled;
		s_xSavedOptions.m_bSSREnabled          = xOptions.m_bSSREnabled;
		s_xSavedOptions.m_bSSGIEnabled         = xOptions.m_bSSGIEnabled;
		s_xSavedOptions.m_bFogEnabled          = xOptions.m_bFogEnabled;
		s_xSavedOptions.m_bCPUParticlesEnabled = xOptions.m_bCPUParticlesEnabled;
		s_xSavedOptions.m_bGPUParticlesEnabled = xOptions.m_bGPUParticlesEnabled;
		s_xSavedOptions.m_bSDFsEnabled         = xOptions.m_bSDFsEnabled;
		s_xSavedOptions.m_bTranslucencyEnabled = xOptions.m_bTranslucencyEnabled;
		s_xSavedOptions.m_bValid = true;

		xOptions.m_bHiZEnabled          = true;
		xOptions.m_bSSAOEnabled         = true;
		xOptions.m_bSSREnabled          = true;
		xOptions.m_bSSGIEnabled         = true;   // default OFF — forced on for this test only
		xOptions.m_bFogEnabled          = true;
		xOptions.m_bCPUParticlesEnabled = true;
		xOptions.m_bGPUParticlesEnabled = true;
		xOptions.m_bSDFsEnabled         = true;
		xOptions.m_bTranslucencyEnabled = true;
	}

	void RestoreGraphicsOptions()
	{
		if (!s_xSavedOptions.m_bValid) { return; }
		Zenith_GraphicsOptions& xOptions = Zenith_GraphicsOptions::Get();
		xOptions.m_bHiZEnabled          = s_xSavedOptions.m_bHiZEnabled;
		xOptions.m_bSSAOEnabled         = s_xSavedOptions.m_bSSAOEnabled;
		xOptions.m_bSSREnabled          = s_xSavedOptions.m_bSSREnabled;
		xOptions.m_bSSGIEnabled         = s_xSavedOptions.m_bSSGIEnabled;
		xOptions.m_bFogEnabled          = s_xSavedOptions.m_bFogEnabled;
		xOptions.m_bCPUParticlesEnabled = s_xSavedOptions.m_bCPUParticlesEnabled;
		xOptions.m_bGPUParticlesEnabled = s_xSavedOptions.m_bGPUParticlesEnabled;
		xOptions.m_bSDFsEnabled         = s_xSavedOptions.m_bSDFsEnabled;
		xOptions.m_bTranslucencyEnabled = s_xSavedOptions.m_bTranslucencyEnabled;
		s_xSavedOptions.m_bValid = false;
	}

	// ------------------------------------------------------------------
	// String helpers. No std:: containers — fixed arrays + strcmp only.
	// ------------------------------------------------------------------
	bool ContainsName(const char* const* paszNames, u_int uCount, const char* szName)
	{
		if (szName == nullptr) { return false; }
		for (u_int u = 0; u < uCount; u++)
		{
			if (paszNames[u] != nullptr && std::strcmp(paszNames[u], szName) == 0) { return true; }
		}
		return false;
	}

	u_int CountName(const char* const* paszNames, u_int uCount, const char* szName)
	{
		u_int uHits = 0u;
		if (szName == nullptr) { return 0u; }
		for (u_int u = 0; u < uCount; u++)
		{
			if (paszNames[u] != nullptr && std::strcmp(paszNames[u], szName) == 0) { ++uHits; }
		}
		return uHits;
	}

	// Writes szName's "<base>" into pcOut when szName is exactly
	// "<base> (<suffix>)" for uSlot's suffix; returns false otherwise. The
	// suffix comes from Flux_ViewSlotSuffix (D0-A) — the SAME table the pool
	// composes from — so this never transcribes a suffix.
	bool TryStripViewSuffix(const char* szName, u_int uSlot, char* pcOut, u_int uOutLen)
	{
		const char* szSuffix = Flux_ViewSlotSuffix(uSlot);
		if (szName == nullptr || szSuffix == nullptr || pcOut == nullptr) { return false; }

		const size_t ulNameLen   = std::strlen(szName);
		const size_t ulSuffixLen = std::strlen(szSuffix);
		const size_t ulTailLen   = ulSuffixLen + 3u;   // ' ' '(' <suffix> ')'
		if (ulNameLen <= ulTailLen) { return false; }

		const char* szTail = szName + (ulNameLen - ulTailLen);
		if (szTail[0] != ' ' || szTail[1] != '(') { return false; }
		if (std::strncmp(szTail + 2, szSuffix, ulSuffixLen) != 0) { return false; }
		if (szTail[2 + ulSuffixLen] != ')') { return false; }

		const size_t ulBaseLen = ulNameLen - ulTailLen;
		if (ulBaseLen + 1u > static_cast<size_t>(uOutLen)) { return false; }
		std::memcpy(pcOut, szName, ulBaseLen);
		pcOut[ulBaseLen] = '\0';
		return true;
	}

	// "HiZ Mip <N>" -> N, or -1. Used ONLY to skip the slot-0-twin assertion for
	// a preview mip the main view's shorter-or-longer chain does not reach; the
	// main chain is ComputeMipCount(1280,720) = 11 on the fixed Null swapchain
	// and the preview's is 10, so today every preview mip HAS a twin and this
	// path never fires. It exists so a resolution change degrades the clause
	// rather than reddening it for the wrong reason.
	int ParseHiZMipIndex(const char* szBase)
	{
		const char* const szPrefix = "HiZ Mip ";
		const size_t ulPrefixLen = std::strlen(szPrefix);
		if (szBase == nullptr || std::strncmp(szBase, szPrefix, ulPrefixLen) != 0) { return -1; }
		const char* pcDigits = szBase + ulPrefixLen;
		if (*pcDigits == '\0') { return -1; }
		int iValue = 0;
		for (const char* pc = pcDigits; *pc != '\0'; ++pc)
		{
			if (*pc < '0' || *pc > '9') { return -1; }
			iValue = iValue * 10 + (*pc - '0');
		}
		return iValue;
	}

	// ------------------------------------------------------------------
	// Sampling.
	// ------------------------------------------------------------------
	void TakeSample(ViewStructureSample& xSample)
	{
		if (!g_xEngine.FluxRenderer().IsRenderGraphValid()) { return; }

		Flux_RenderGraph& xGraph = g_xEngine.FluxRenderer().GetRenderGraph();

		// Compiled-ness. There is NO IsCompiled(); Compile() returns the cached
		// m_bCompiled without doing any work when the graph is not dirty
		// (Flux_RenderGraph_Compilation.cpp:1250), so on a NOT-DIRTY graph this is
		// a pure read. When it IS dirty we do NOT call it — that would run a real
		// compile off-cycle — and the sample is simply not taken this frame.
		xSample.m_bDirtyAtSample = xGraph.IsDirty();
		if (xSample.m_bDirtyAtSample) { return; }
		xSample.m_bCompiled = xGraph.Compile();

		const Flux_RenderViewRegistry& xViews = g_xEngine.FluxGraphics().RenderViews();
		xSample.m_bPreviewViewActive     = xViews.IsViewActive(kuFluxViewSlotPreviewMaterial);
		xSample.m_bPreviewAnimViewActive = xViews.IsViewActive(kuFluxViewSlotPreviewAnim);
		xSample.m_uMainHiZMipCount       = g_xEngine.HiZ().GetMipCount(kuFluxViewSlotMain);
		xSample.m_uViolations            = xGraph.GetProducerBeforeConsumerViolationCount();
		xSample.m_uExecutionOrderSize    = xGraph.GetExecutionOrder().GetSize();

		const Zenith_Vector<Flux_RenderGraph_Pass*>& xPasses = xGraph.GetPasses();
		xSample.m_uTotalPasses = xPasses.GetSize();

		for (u_int u = 0; u < xPasses.GetSize(); u++)
		{
			const Flux_RenderGraph_Pass* pxPass = xPasses.Get(u);
			if (pxPass == nullptr || pxPass->m_szName == nullptr) { continue; }
			const char* szName = pxPass->m_szName;
			const u_int uSlot  = pxPass->m_uViewSlot;

			if (std::strcmp(szName, kszPREVIEW_ONLY_LITERAL) == 0) { ++xSample.m_uLegacyLiteralCount; }

			if (uSlot == kuFluxViewSlotMain)
			{
				if (xSample.m_uSlot0Count < kuMAX_SAMPLED_SLOT0_PASSES)
				{
					xSample.m_aszSlot0Names[xSample.m_uSlot0Count++] = szName;
				}
				else { ++xSample.m_uOverflowed; }
				continue;
			}

			if (uSlot >= FLUX_MAX_RENDER_VIEWS) { continue; }   // View() would assert

			const FluxRenderViewType eType = xViews.View(uSlot).m_eType;
			if (eType == FLUX_RENDER_VIEW_SHADOW_CASCADE
				&& uSlot >= kuFluxViewSlotShadowFirst
				&& uSlot < kuFluxViewSlotShadowFirst + kuFluxViewNumShadowSlots)
			{
				ViewStructureShadowSlot& xShadow = xSample.m_axShadowSlots[uSlot - kuFluxViewSlotShadowFirst];
				if (xShadow.m_uCount < kuMAX_SAMPLED_SHADOW_PASSES) { xShadow.m_aszNames[xShadow.m_uCount] = szName; }
				else { ++xSample.m_uOverflowed; }
				++xShadow.m_uCount;
				continue;
			}

			if (eType == FLUX_RENDER_VIEW_PREVIEW)
			{
				if (xSample.m_uPreviewCount < kuMAX_SAMPLED_PREVIEW_PASSES)
				{
					xSample.m_auPreviewSlots[xSample.m_uPreviewCount] = uSlot;
					xSample.m_aszPreviewNames[xSample.m_uPreviewCount++] = szName;
				}
				else { ++xSample.m_uOverflowed; }
				continue;
			}

			// Non-main slot, non-cascade, non-preview: nothing classifies it, so
			// no clause can see it. Counted so clause 1 can say so out loud.
			++xSample.m_uUnclassified;
			Zenith_Log(LOG_CATEGORY_RENDERER,
				"[ViewStructure] UNCLASSIFIED pass '%s' on slot %u (view type %u)", szName, uSlot,
				static_cast<u_int>(eType));
		}

		xSample.m_bTaken = true;

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[ViewStructure] SAMPLE %s taken: passes=%u execOrder=%u slot0=%u previewView=%u previewAnimView=%u "
			"previewPasses=%u legacyLiteral=%u violations=%u compiled=%d mainHiZMips=%u overflow=%u unclassified=%u",
			xSample.m_szLabel, xSample.m_uTotalPasses, xSample.m_uExecutionOrderSize, xSample.m_uSlot0Count,
			xSample.m_bPreviewViewActive ? 1u : 0u, xSample.m_bPreviewAnimViewActive ? 1u : 0u,
			xSample.m_uPreviewCount, xSample.m_uLegacyLiteralCount, xSample.m_uViolations,
			xSample.m_bCompiled ? 1 : 0, xSample.m_uMainHiZMipCount, xSample.m_uOverflowed,
			xSample.m_uUnclassified);
	}

	// ------------------------------------------------------------------
	// Setup / Step / Verify / Teardown.
	// ------------------------------------------------------------------
	void Setup_RenderGraphViewStructure()
	{
		ResetResults();
		s_ePhase = VIEW_STRUCTURE_PHASE_A_MAIN_ONLY;
		s_uPhaseFrame = 0u;

		for (u_int u = 0; u < VIEW_STRUCTURE_PHASE_COUNT; u++)
		{
			s_axSamples[u] = ViewStructureSample();
		}
		s_axSamples[VIEW_STRUCTURE_PHASE_A_MAIN_ONLY].m_szLabel    = "A(main-only)";
		s_axSamples[VIEW_STRUCTURE_PHASE_B_PREVIEW].m_szLabel      = "B(main+preview)";
		s_axSamples[VIEW_STRUCTURE_PHASE_C_PREVIEW_ANIM].m_szLabel = "C(main+preview+anim)";
		s_axSamples[VIEW_STRUCTURE_PHASE_D_SETTLED_OFF].m_szLabel  = "D(settled-off)";

		PinGraphicsOptions();

		// Start from a known view set: whatever a previous test in the same
		// process left open, drop it. The liveness grace would do this within 8
		// rendered frames anyway; being explicit makes phase A's "preview view
		// inactive" assertion mean what it says.
		//
		// BOTH preview views, not just the material one. The animation preview has
		// no liveness grace and no controller — whoever activates it must
		// deactivate it — so a previous test that left it up would make phase A's
		// "no PREVIEW pass exists" clause fail for a reason that has nothing to do
		// with this run.
		g_xEngine.MaterialPreview().SetActive(false);
		if (g_xEngine.FluxGraphics().RenderViews().SetViewActive(kuFluxViewSlotPreviewAnim, false))
		{
			g_xEngine.FluxRenderer().RequestGraphRebuild();
		}
	}

	bool Step_RenderGraphViewStructure(int iFrame)
	{
		(void)iFrame;
		if (s_ePhase >= VIEW_STRUCTURE_PHASE_DONE) { return false; }

		// Sample C is live; the skip-and-log branch that stood here while the
		// animation-preview slot did not exist is gone rather than left switched
		// off. `if constexpr` is still used below so a constant condition is not a
		// C4127 at /W4.
		switch (s_ePhase)
		{
		case VIEW_STRUCTURE_PHASE_A_MAIN_ONLY:
			// Nothing to drive: the preview view stays inactive on its own.
			break;

		case VIEW_STRUCTURE_PHASE_B_PREVIEW:
			// EVERY frame — the controller self-deactivates 8 frames after the
			// last refresh (Flux_MaterialPreviewController.h:233).
			g_xEngine.MaterialPreview().SetActive(true);
			break;

		case VIEW_STRUCTURE_PHASE_C_PREVIEW_ANIM:
			if constexpr (kbSampleCEnabled)
			{
				// Keep the material preview alive (it self-deactivates 8 frames
				// after its last refresh), then stage the animation preview exactly
				// as a session would: dims + activate + rebuild. The registry
				// already TYPES the slot PREVIEW/full-pipeline, so nothing here has
				// to say so — which is the point of D2-a having done it in the
				// constructor rather than at the call site.
				g_xEngine.MaterialPreview().SetActive(true);

				Flux_RenderViewRegistry& xViews = g_xEngine.FluxGraphics().RenderViews();
				xViews.View(kuFluxViewSlotPreviewAnim).m_xTargetDims =
					Zenith_Maths::UVector2(kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE);
				if (xViews.SetViewActive(kuFluxViewSlotPreviewAnim, true))
				{
					g_xEngine.FluxRenderer().RequestGraphRebuild();
				}
			}
			break;

		case VIEW_STRUCTURE_PHASE_D_SETTLED_OFF:
			if (s_uPhaseFrame == 0u)
			{
				g_xEngine.MaterialPreview().SetActive(false);
				if constexpr (kbSampleCEnabled)
				{
					Flux_RenderViewRegistry& xViews = g_xEngine.FluxGraphics().RenderViews();
					if (xViews.SetViewActive(kuFluxViewSlotPreviewAnim, false))
					{
						g_xEngine.FluxRenderer().RequestGraphRebuild();
					}
				}
			}
			break;

		default:
			break;
		}

		ViewStructureSample& xSample = s_axSamples[s_ePhase];
		if (!xSample.m_bTaken && s_uPhaseFrame >= kuSETTLE_FRAMES)
		{
			TakeSample(xSample);
		}

		++s_uPhaseFrame;
		if (s_uPhaseFrame >= kuPHASE_FRAMES)
		{
			s_ePhase = static_cast<ViewStructurePhase>(s_ePhase + 1u);
			s_uPhaseFrame = 0u;
		}

		return s_ePhase < VIEW_STRUCTURE_PHASE_DONE;
	}

	// --- clause bodies -------------------------------------------------

	// 1. No duplicate m_szName in GetPasses(). Near-vacuous — AddPass already
	//    Zenith_Checks it — and kept because it is the TEST-LEVEL statement of
	//    the thing every per-view name table exists to guarantee. A duplicate is
	//    also the exact failure a mis-suffixed preview pass would produce.
	void Clause1_NoDuplicateNames(const ViewStructureSample& xSample)
	{
		// Slot-0 names + preview names + cascade names, checked as one namespace.
		for (u_int u = 0; u < xSample.m_uSlot0Count; u++)
		{
			const u_int uHits = CountName(xSample.m_aszSlot0Names, xSample.m_uSlot0Count, xSample.m_aszSlot0Names[u])
				+ CountName(xSample.m_aszPreviewNames, xSample.m_uPreviewCount, xSample.m_aszSlot0Names[u]);
			CheckEqIntNamed(static_cast<int>(uHits), 1, "slot-0 pass name is not unique across the graph",
				xSample.m_aszSlot0Names[u]);
		}
		for (u_int u = 0; u < xSample.m_uPreviewCount; u++)
		{
			const u_int uHits = CountName(xSample.m_aszPreviewNames, xSample.m_uPreviewCount, xSample.m_aszPreviewNames[u])
				+ CountName(xSample.m_aszSlot0Names, xSample.m_uSlot0Count, xSample.m_aszPreviewNames[u]);
			CheckEqIntNamed(static_cast<int>(uHits), 1, "preview-view pass name is not unique across the graph",
				xSample.m_aszPreviewNames[u]);
		}
		// The cascade names share the same namespace and must not collide with
		// either of the above (clause 8 pins what they are; this pins that they
		// are not something else's name as well).
		for (u_int c = 0; c < kuFluxViewNumShadowSlots; c++)
		{
			const ViewStructureShadowSlot& xShadow = xSample.m_axShadowSlots[c];
			const u_int uStored = (xShadow.m_uCount < kuMAX_SAMPLED_SHADOW_PASSES)
				? xShadow.m_uCount : kuMAX_SAMPLED_SHADOW_PASSES;
			for (u_int u = 0; u < uStored; u++)
			{
				const u_int uHits = CountName(xSample.m_aszSlot0Names, xSample.m_uSlot0Count, xShadow.m_aszNames[u])
					+ CountName(xSample.m_aszPreviewNames, xSample.m_uPreviewCount, xShadow.m_aszNames[u]);
				CheckEqIntNamed(static_cast<int>(uHits), 0,
					"shadow-cascade pass name collides with a slot-0 or preview pass name", xShadow.m_aszNames[u]);
			}
		}
		CheckEqInt(static_cast<int>(xSample.m_uOverflowed), 0, "sample capture overflowed a fixed array");
		CheckEqInt(static_cast<int>(xSample.m_uUnclassified), 0,
			"a pass renders a view slot whose registered type is neither MAIN, SHADOW_CASCADE nor PREVIEW "
			"- no clause here can see it, and the naming law does not reach it");
	}

	// 2. The naming law, over FULL-PIPELINE PREVIEW views only.
	void Clause2_NamingLaw(const ViewStructureSample& xSample)
	{
		for (u_int u = 0; u < xSample.m_uPreviewCount; u++)
		{
			const char* szName = xSample.m_aszPreviewNames[u];
			const u_int uSlot  = xSample.m_auPreviewSlots[u];

			if (std::strcmp(szName, kszPREVIEW_ONLY_LITERAL) == 0)
			{
				// The one sanctioned exception, matched by its full literal.
				CheckTrue(true, "legacy preview literal accepted");
				continue;
			}

			char acBase[kuBASE_BUFFER_LEN] = {};
			const bool bParsed = TryStripViewSuffix(szName, uSlot, acBase, kuBASE_BUFFER_LEN);
			CheckTrueNamed(bParsed,
				"preview-view pass is not named '<base> (<suffix>)' for its slot and is not the legacy literal",
				szName);
			if (!bParsed) { continue; }

			if (IsPreviewOnlyBase(acBase))
			{
				// Preview-only base: no slot-0 twin is owed.
				CheckTrue(true, "preview-only base needs no slot-0 twin");
				continue;
			}

			const int iHiZMip = ParseHiZMipIndex(acBase);
			if (iHiZMip >= 0 && static_cast<u_int>(iHiZMip) >= xSample.m_uMainHiZMipCount)
			{
				// The main view's HiZ chain is shorter than this preview mip, so no
				// twin can exist. Report it rather than silently skipping.
				Zenith_Log(LOG_CATEGORY_RENDERER,
					"[ViewStructure] note: '%s' has no slot-0 twin because the main HiZ chain is %u mips",
					acBase, xSample.m_uMainHiZMipCount);
				continue;
			}

			CheckTrueNamed(ContainsName(xSample.m_aszSlot0Names, xSample.m_uSlot0Count, acBase),
				"per-view base has no slot-0 pass under its bare name", acBase);
		}
	}

	// 3. Slot-0 inventory identical between A and B, excluding the legacy literal.
	void Clause3_Slot0InventoryStable(const ViewStructureSample& xA, const ViewStructureSample& xB)
	{
		u_int uAEffective = 0u;
		for (u_int u = 0; u < xA.m_uSlot0Count; u++)
		{
			if (std::strcmp(xA.m_aszSlot0Names[u], kszPREVIEW_ONLY_LITERAL) == 0) { continue; }
			++uAEffective;
			CheckTrueNamed(ContainsName(xB.m_aszSlot0Names, xB.m_uSlot0Count, xA.m_aszSlot0Names[u]),
				"slot-0 pass present with the preview view OFF but absent with it ON", xA.m_aszSlot0Names[u]);
		}
		u_int uBEffective = 0u;
		for (u_int u = 0; u < xB.m_uSlot0Count; u++)
		{
			if (std::strcmp(xB.m_aszSlot0Names[u], kszPREVIEW_ONLY_LITERAL) == 0) { continue; }
			++uBEffective;
			CheckTrueNamed(ContainsName(xA.m_aszSlot0Names, xA.m_uSlot0Count, xB.m_aszSlot0Names[u]),
				"slot-0 pass present with the preview view ON but absent with it OFF", xB.m_aszSlot0Names[u]);
		}
		CheckEqInt(static_cast<int>(uBEffective), static_cast<int>(uAEffective),
			"slot-0 pass COUNT (excluding the legacy literal) changed when the preview view activated");
	}

	// 4. Slot-0 multiplicity is exactly 1 per per-view base; the legacy literal
	//    appears exactly once in total, and only while the preview view is up.
	void Clause4_Slot0Multiplicity(const ViewStructureSample& xA, const ViewStructureSample& xB,
		const ViewStructureSample& xD)
	{
		for (u_int u = 0; u < kuPERVIEW_BASE_COUNT; u++)
		{
			const char* szBase = kaszPERVIEW_BASES[u];
			if (IsPreviewOnlyBase(szBase)) { continue; }
			CheckEqIntNamed(static_cast<int>(CountName(xB.m_aszSlot0Names, xB.m_uSlot0Count, szBase)), 1,
				"per-view base does not appear EXACTLY ONCE at slot 0", szBase);
		}

		// The pass is only DECLARED while the preview view is active
		// (Flux_HDR.cpp:828-831 — the per-view walk visits it only when the
		// registry reports it active and full-pipeline). m_uLegacyLiteralCount is
		// counted over the WHOLE graph regardless of slot (see TakeSample), so
		// these three survived D1-e moving the pass off slot 0 unchanged.
		CheckEqInt(static_cast<int>(xB.m_uLegacyLiteralCount), 1,
			"'Preview LDR Transition' must appear exactly once while the preview view is active");
		if (xA.m_bTaken)
		{
			CheckEqInt(static_cast<int>(xA.m_uLegacyLiteralCount), 0,
				"'Preview LDR Transition' must not exist while the preview view is inactive (sample A)");
		}
		if (xD.m_bTaken)
		{
			CheckEqInt(static_cast<int>(xD.m_uLegacyLiteralCount), 0,
				"'Preview LDR Transition' must not survive the preview view going away (sample D)");
		}

		// D1-e: WHICH SLOT it records on, which the whole-graph count above
		// cannot see. Before D1-e the pass carried no .View(...) and therefore
		// recorded at slot 0 — a preview-only pass sitting in the main view's
		// inventory, where clause 3 had to exclude it by name to stay true. Both
		// halves are asserted: absent from slot 0, and present EXACTLY ONCE on
		// the material-preview slot. Checking only the first half would pass for
		// a pass that had vanished entirely.
		CheckEqInt(static_cast<int>(CountName(xB.m_aszSlot0Names, xB.m_uSlot0Count, kszPREVIEW_ONLY_LITERAL)), 0,
			"'Preview LDR Transition' still records at SLOT 0 — it must carry .View(kuFluxViewSlotPreviewMaterial)");

		u_int uOnPreviewSlot = 0u;
		for (u_int u = 0; u < xB.m_uPreviewCount; u++)
		{
			if (xB.m_aszPreviewNames[u] != nullptr
				&& std::strcmp(xB.m_aszPreviewNames[u], kszPREVIEW_ONLY_LITERAL) == 0
				&& xB.m_auPreviewSlots[u] == kuFluxViewSlotPreviewMaterial)
			{
				++uOnPreviewSlot;
			}
		}
		CheckEqInt(static_cast<int>(uOnPreviewSlot), 1,
			"'Preview LDR Transition' must record EXACTLY ONCE on the material-preview slot");
	}

	// 5. The per-view base inventory equals kaszPERVIEW_BASES EXACTLY, FOR ONE
	//    SLOT AT A TIME.
	//
	// ★ IT IS PER SLOT, AND THAT IS THE WHOLE POINT OF SAMPLE C. The sample's
	// preview capture is the UNION over every preview-typed slot, so a clause
	// written over the union would state "between them, the preview views
	// instantiate the golden list" — which is satisfied by the material preview
	// alone and would go green with the animation preview instantiating NOTHING.
	// That is precisely the failure a second preview view introduces, so the
	// clause is parameterised by slot and run once per ACTIVE preview view.
	void Clause5_PerViewInventory(const ViewStructureSample& xSample, u_int uOnlySlot)
	{
		bool abGoldenSeen[kuPERVIEW_BASE_COUNT] = {};
		u_int uConsidered = 0u;

		for (u_int u = 0; u < xSample.m_uPreviewCount; u++)
		{
			const char* szName = xSample.m_aszPreviewNames[u];
			const u_int uSlot  = xSample.m_auPreviewSlots[u];
			if (uSlot != uOnlySlot) { continue; }
			++uConsidered;
			if (std::strcmp(szName, kszPREVIEW_ONLY_LITERAL) == 0) { continue; }

			char acBase[kuBASE_BUFFER_LEN] = {};
			if (!TryStripViewSuffix(szName, uSlot, acBase, kuBASE_BUFFER_LEN))
			{
				// Clause 2 already reported the parse failure; do not double-count.
				continue;
			}
			if (IsPreviewOnlyBase(acBase)) { continue; }

			bool bMatched = false;
			for (u_int g = 0; g < kuPERVIEW_BASE_COUNT; g++)
			{
				if (std::strcmp(kaszPERVIEW_BASES[g], acBase) == 0) { abGoldenSeen[g] = true; bMatched = true; break; }
			}
			CheckTrueNamed(bMatched, "preview view instantiated a base that is NOT in the golden list", acBase);
		}

		// A slot with no passes at all would otherwise report only "base X missing"
		// once per golden base, which reads as kuPERVIEW_BASE_COUNT separate
		// defects rather than as one view that was never instantiated.
		CheckTrue(uConsidered > 0u, "the preview slot under test declared NO passes at all");
		Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] clause 5: slot %u (%s) declared %u passes",
			uOnlySlot, Flux_ViewSlotSuffix(uOnlySlot) != nullptr ? Flux_ViewSlotSuffix(uOnlySlot) : "<none>",
			uConsidered);

		for (u_int g = 0; g < kuPERVIEW_BASE_COUNT; g++)
		{
			CheckTrueNamed(abGoldenSeen[g], "golden per-view base was NOT instantiated for this preview view",
				kaszPERVIEW_BASES[g]);
		}

		// The four rows that ARE the pipeline, asserted independently of the list.
		for (u_int u = 0; u < kuALWAYS_ON_COUNT; u++)
		{
			bool bFound = false;
			for (u_int p = 0; p < xSample.m_uPreviewCount && !bFound; p++)
			{
				if (xSample.m_auPreviewSlots[p] != uOnlySlot) { continue; }
				char acBase[kuBASE_BUFFER_LEN] = {};
				if (!TryStripViewSuffix(xSample.m_aszPreviewNames[p], xSample.m_auPreviewSlots[p], acBase, kuBASE_BUFFER_LEN)) { continue; }
				if (std::strcmp(acBase, kaszALWAYS_ON[u]) == 0) { bFound = true; }
			}
			CheckTrueNamed(bFound, "always-on base missing from this preview view", kaszALWAYS_ON[u]);
		}
	}

	void Clause5_AlwaysOnAtSlot0(const ViewStructureSample& xSample)
	{
		for (u_int u = 0; u < kuALWAYS_ON_COUNT; u++)
		{
			CheckTrueNamed(ContainsName(xSample.m_aszSlot0Names, xSample.m_uSlot0Count, kaszALWAYS_ON[u]),
				"always-on base missing from slot 0", kaszALWAYS_ON[u]);
		}
	}

	// 8. Shadow slots 1..4 carry exactly "Shadow Cascade 0".."Shadow Cascade 3".
	void Clause8_ShadowSlots(const ViewStructureSample& xSample)
	{
		static const char* const s_aszExpectedCascadeNames[kuFluxViewNumShadowSlots] =
		{
			"Shadow Cascade 0",
			"Shadow Cascade 1",
			"Shadow Cascade 2",
			"Shadow Cascade 3",
		};
		for (u_int c = 0; c < kuFluxViewNumShadowSlots; c++)
		{
			const ViewStructureShadowSlot& xShadow = xSample.m_axShadowSlots[c];
			CheckEqIntNamed(static_cast<int>(xShadow.m_uCount), 1,
				"a shadow-cascade slot must carry EXACTLY ONE pass", s_aszExpectedCascadeNames[c]);
			if (xShadow.m_uCount >= 1u)
			{
				CheckEqStr(xShadow.m_aszNames[0], s_aszExpectedCascadeNames[c],
					"shadow-cascade slot carries the wrong pass");
			}
		}
	}

	// 9. One persistent preview LDR per preview view, and no two the same object.
	//    O(n^2) over kuFluxViewNumPreviewSlots (two today) and written as a walk
	//    over the RANGE so a third preview slot is covered with no edit here.
	void Clause9_PreviewLDRsAreDistinct()
	{
		Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
		for (u_int uA = 0; uA < kuFluxViewNumPreviewSlots; uA++)
		{
			const u_int uSlotA = kuFluxViewSlotPreviewFirst + uA;
			const void* pxA = &xGraphics.GetPreviewLDR(uSlotA);
			for (u_int uB = uA + 1u; uB < kuFluxViewNumPreviewSlots; uB++)
			{
				const u_int uSlotB = kuFluxViewSlotPreviewFirst + uB;
				const void* pxB = &xGraphics.GetPreviewLDR(uSlotB);
				CheckTrue(pxA != pxB,
					"two preview views share ONE persistent LDR — the second would overwrite the first's image");
			}
		}
	}

	bool Verify_RenderGraphViewStructure()
	{
		const ViewStructureSample& xA = s_axSamples[VIEW_STRUCTURE_PHASE_A_MAIN_ONLY];
		const ViewStructureSample& xB = s_axSamples[VIEW_STRUCTURE_PHASE_B_PREVIEW];
		const ViewStructureSample& xC = s_axSamples[VIEW_STRUCTURE_PHASE_C_PREVIEW_ANIM];
		const ViewStructureSample& xD = s_axSamples[VIEW_STRUCTURE_PHASE_D_SETTLED_OFF];

		// --- rig integrity: the samples the clauses need must actually exist ---
		BeginClause("0");
		CheckTrue(xA.m_bTaken, "sample A was never taken (the graph never settled undirty in phase A)");
		CheckTrue(xB.m_bTaken, "sample B was never taken (the graph never settled undirty in phase B)");
		CheckTrue(xD.m_bTaken, "sample D was never taken (the graph never settled undirty in phase D)");
		CheckTrue(!xA.m_bPreviewViewActive, "the material-preview view was ACTIVE during sample A");
		CheckTrue(!xA.m_bPreviewAnimViewActive, "the animation-preview view was ACTIVE during sample A");
		CheckTrue(xB.m_bPreviewViewActive, "the material-preview view was NOT active during sample B");
		CheckTrue(!xB.m_bPreviewAnimViewActive, "the animation-preview view was ACTIVE during sample B — B is the ONE-preview shape");
		CheckTrue(!xD.m_bPreviewViewActive, "the material-preview view was still ACTIVE during sample D");
		CheckTrue(!xD.m_bPreviewAnimViewActive, "the animation-preview view was still ACTIVE during sample D");
		if constexpr (kbSampleCEnabled)
		{
			CheckTrue(xC.m_bTaken, "sample C was never taken");
			CheckTrue(xC.m_bPreviewAnimViewActive, "the animation-preview view was NOT active during sample C");
			CheckTrue(xC.m_bPreviewViewActive, "the material-preview view was NOT active during sample C — C is BOTH previews at once");
		}
		EndClause();

		// --- 1 ---
		BeginClause("1");
		if (xA.m_bTaken) { Clause1_NoDuplicateNames(xA); }
		if (xB.m_bTaken) { Clause1_NoDuplicateNames(xB); }
		if (xD.m_bTaken) { Clause1_NoDuplicateNames(xD); }
		if constexpr (kbSampleCEnabled) { if (xC.m_bTaken) { Clause1_NoDuplicateNames(xC); } }
		EndClause();

		// --- 2 ---
		BeginClause("2");
		if (xA.m_bTaken)
		{
			CheckEqInt(static_cast<int>(xA.m_uPreviewCount), 0,
				"a FLUX_RENDER_VIEW_PREVIEW pass exists with every preview view inactive (sample A)");
		}
		if (xB.m_bTaken) { Clause2_NamingLaw(xB); }
		if (xD.m_bTaken)
		{
			CheckEqInt(static_cast<int>(xD.m_uPreviewCount), 0,
				"a FLUX_RENDER_VIEW_PREVIEW pass survived the preview view going away (sample D)");
		}
		if constexpr (kbSampleCEnabled && kbSampleCFullClauses) { if (xC.m_bTaken) { Clause2_NamingLaw(xC); } }
		EndClause();

		// --- 3 ---
		BeginClause("3");
		if (xA.m_bTaken && xB.m_bTaken) { Clause3_Slot0InventoryStable(xA, xB); }
		else { CheckTrue(false, "clause 3 needs BOTH sample A and sample B"); }
		if constexpr (kbSampleCEnabled && kbSampleCFullClauses)
		{
			if (xA.m_bTaken && xC.m_bTaken) { Clause3_Slot0InventoryStable(xA, xC); }
		}
		EndClause();

		// --- 4 ---
		BeginClause("4");
		if (xB.m_bTaken) { Clause4_Slot0Multiplicity(xA, xB, xD); }
		else { CheckTrue(false, "clause 4 needs sample B"); }
		EndClause();

		// --- 5 ---
		BeginClause("5");
		Zenith_Log(LOG_CATEGORY_RENDERER, "[ViewStructure] golden per-view base count = %u (from sizeof)",
			kuPERVIEW_BASE_COUNT);
		if (xB.m_bTaken) { Clause5_PerViewInventory(xB, kuFluxViewSlotPreviewMaterial); }
		else { CheckTrue(false, "clause 5 needs sample B"); }
		if (xA.m_bTaken) { Clause5_AlwaysOnAtSlot0(xA); }
		if (xB.m_bTaken) { Clause5_AlwaysOnAtSlot0(xB); }
		if (xD.m_bTaken) { Clause5_AlwaysOnAtSlot0(xD); }
		if constexpr (kbSampleCEnabled && kbSampleCFullClauses)
		{
			// ONCE PER PREVIEW SLOT, not once over the union — with both views up,
			// a union check is satisfied by the material preview alone. The walk is
			// over the RANGE, so a third preview view is covered without an edit.
			if (xC.m_bTaken)
			{
				for (u_int u = 0; u < kuFluxViewNumPreviewSlots; u++)
				{
					Clause5_PerViewInventory(xC, kuFluxViewSlotPreviewFirst + u);
				}
			}
		}
		EndClause();

		// --- 6 ---
		BeginClause("6");
		if (xA.m_bTaken) { CheckEqInt(static_cast<int>(xA.m_uViolations), 0, "producer-before-consumer violations, sample A"); }
		if (xB.m_bTaken) { CheckEqInt(static_cast<int>(xB.m_uViolations), 0, "producer-before-consumer violations, sample B"); }
		if (xD.m_bTaken) { CheckEqInt(static_cast<int>(xD.m_uViolations), 0, "producer-before-consumer violations, sample D"); }
		if constexpr (kbSampleCEnabled) { if (xC.m_bTaken) { CheckEqInt(static_cast<int>(xC.m_uViolations), 0, "producer-before-consumer violations, sample C"); } }
		EndClause();

		// --- 7 ---
		BeginClause("7");
		if (xA.m_bTaken)
		{
			CheckTrue(xA.m_bCompiled, "graph not compiled, sample A");
			CheckTrue(xA.m_uExecutionOrderSize > 0u, "empty execution order, sample A");
		}
		if (xB.m_bTaken)
		{
			CheckTrue(xB.m_bCompiled, "graph not compiled, sample B");
			CheckTrue(xB.m_uExecutionOrderSize > 0u, "empty execution order, sample B");
		}
		if (xD.m_bTaken)
		{
			CheckTrue(xD.m_bCompiled, "graph not compiled, sample D");
			CheckTrue(xD.m_uExecutionOrderSize > 0u, "empty execution order, sample D");
		}
		if (xA.m_bTaken && xD.m_bTaken)
		{
			CheckEqInt(static_cast<int>(xD.m_uTotalPasses), static_cast<int>(xA.m_uTotalPasses),
				"the graph did not return to its main-only shape after the preview view went away");
		}
		if constexpr (kbSampleCEnabled)
		{
			if (xC.m_bTaken)
			{
				CheckTrue(xC.m_bCompiled, "graph not compiled, sample C");
				CheckTrue(xC.m_uExecutionOrderSize > 0u, "empty execution order, sample C");
			}
		}
		EndClause();

		// --- 8 ---
		BeginClause("8");
		if (xA.m_bTaken) { Clause8_ShadowSlots(xA); }
		if (xB.m_bTaken) { Clause8_ShadowSlots(xB); }
		if (xD.m_bTaken) { Clause8_ShadowSlots(xD); }
		if constexpr (kbSampleCEnabled) { if (xC.m_bTaken) { Clause8_ShadowSlots(xC); } }
		EndClause();

		// --- 9 ---
		// EVERY preview view owns a DISTINCT persistent LDR. This was a TODO on the
		// sample-C driver ("the reduced spike contract additionally asserts
		// &GetPreviewLDR(6) != &GetPreviewLDR(5)"); it is a permanent clause now,
		// and it is UNCONDITIONAL — it reads the graphics object, not a sample, so
		// it holds whether or not the phase machine ever settled.
		//
		// It is a structural clause, not a rig check: the two tonemap passes name
		// their target through GetPreviewLDR(uViewSlot), so if two preview slots
		// resolved to ONE attachment the graph would still compile, still carry no
		// duplicate pass names and still satisfy every clause above — while the
		// second view silently overwrote the first view's image every frame. No
		// clause 1-8 can see that, because none of them look at resources.
		BeginClause("9");
		Clause9_PreviewLDRsAreDistinct();
		EndClause();

		// xC is named unconditionally so it is referenced even when sample C is
		// staged off (an `if constexpr`-discarded reference is not a use MSVC's
		// unreferenced-local analysis is obliged to see).
		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[ViewStructure] SUMMARY %d checks, %d failed | passes A=%u B=%u C=%u D=%u | "
			"previewPasses A=%u B=%u C=%u D=%u | taken A=%d B=%d C=%d D=%d | goldenBases=%u mainHiZMips=%u",
			g_iChecks, g_iFailures,
			xA.m_uTotalPasses, xB.m_uTotalPasses, xC.m_uTotalPasses, xD.m_uTotalPasses,
			xA.m_uPreviewCount, xB.m_uPreviewCount, xC.m_uPreviewCount, xD.m_uPreviewCount,
			xA.m_bTaken ? 1 : 0, xB.m_bTaken ? 1 : 0, xC.m_bTaken ? 1 : 0, xD.m_bTaken ? 1 : 0,
			kuPERVIEW_BASE_COUNT, xB.m_uMainHiZMipCount);

		// A test that asserted nothing is a test that cannot fail.
		return g_iFailures == 0 && g_iChecks > 0;
	}

	void Teardown_RenderGraphViewStructure()
	{
		// Everything this test installed that no lifecycle owns: the preview
		// controller's active flag and the nine pinned graphics options.
		g_xEngine.MaterialPreview().SetActive(false);
		if constexpr (kbSampleCEnabled)
		{
			Flux_RenderViewRegistry& xViews = g_xEngine.FluxGraphics().RenderViews();
			if (xViews.SetViewActive(kuFluxViewSlotPreviewAnim, false))
			{
				g_xEngine.FluxRenderer().RequestGraphRebuild();
			}
		}
		RestoreGraphicsOptions();
	}

	const Zenith_AutomatedTest g_xRenderGraphViewStructure = {
		"RT_RenderGraphViewStructure",
		&Setup_RenderGraphViewStructure,
		&Step_RenderGraphViewStructure,
		&Verify_RenderGraphViewStructure,
		300,
		false /* m_bRequiresGraphics — reads graph STRUCTURE, never a pixel */,
		false /* m_bManualOnly */,
		&Teardown_RenderGraphViewStructure
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xRenderGraphViewStructure);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
