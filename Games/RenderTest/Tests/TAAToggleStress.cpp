#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RendererImpl.h"                 // GetRenderGraph
#include "Flux/Flux_GraphicsImpl.h"                 // IsVelocityMRTActive
#include "Flux/TAA/Flux_TAAImpl.h"                  // SetEnabled / IsResolveActive
#include "Flux/RenderGraph/Flux_RenderGraph.h"      // GetPasses / Flux_RenderGraph_Pass

#ifdef ZENITH_VULKAN
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"     // GetVMAStats (real VRAM, Vulkan only)
#endif

#include <cstring>

// =====================================================================
// TAAToggleStress (plan Stage 4.4) — flips TAA on/off every 60 frames x10 and
// asserts the graph reconfigures cleanly + no VRAM leak. A windowed, manual-run
// verification (needs the live compiled render graph + real VRAM), driven by:
//   rendertest.exe --automated-test TAAToggleStress
//
// Uses the runtime latch override (g_xEngine.TAA().SetEnabled) added in Stage 4.4 —
// the debug var is ImGui-only and --taa is boot-only, so neither can flip mid-run.
//
// HARD invariants (Verify fails if any is violated on any cycle):
//   - the 3 TAA passes ("TAA Resolve/CopyToHistory/Sharpen") are present IFF TAA is on,
//   - every TAA pass renders the MAIN view (m_uViewSlot == 0) — never a preview/cascade,
//   - IsResolveActive() matches the requested state,
//   - the total pass count is constant per state and on == off + 3 (the 3 TAA passes),
//     so a flip returns the graph to its exact prior shape.
// VRAM A/B (Vulkan): the actual allocated bytes are sampled per state; the leak check
// fails if the OFF-state VRAM grows > 5% first->last cycle (a real per-toggle leak of the
// velocity/history transients would accumulate); the on-vs-off delta is logged (directional).
// =====================================================================

namespace
{
	// --- accumulated results (Setup resets, Step samples, Verify judges) ---
	bool    s_bExpectedOn      = true;
	int     s_iFlipCount       = 0;
	int     s_iSampleCount     = 0;
	bool    s_bAllInvariantsOK = true;

	int     s_iOffPassCount    = -1;   // first off-state total pass count (later off samples must match)
	int     s_iOnPassCount     = -1;   // first on-state total pass count
	bool    s_bOffCountStable  = true;
	bool    s_bOnCountStable   = true;

	u_int64 s_ulOffVRAMFirst   = 0;
	u_int64 s_ulOffVRAMLast    = 0;
	u_int64 s_ulOnVRAMLast     = 0;
	int     s_iOffSamples      = 0;
	int     s_iOnSamples       = 0;

	// Count the live TAA passes + confirm they all render the main view.
	int CountTAAPasses(bool& bAnyOnNonMainView)
	{
		bAnyOnNonMainView = false;
		Flux_RenderGraph& xGraph = g_xEngine.FluxRenderer().GetRenderGraph();
		const Zenith_Vector<Flux_RenderGraph_Pass*>& xPasses = xGraph.GetPasses();
		int iCount = 0;
		for (u_int u = 0; u < xPasses.GetSize(); ++u)
		{
			const Flux_RenderGraph_Pass* pxPass = xPasses.Get(u);
			if (pxPass && pxPass->m_szName && std::strncmp(pxPass->m_szName, "TAA ", 4) == 0)
			{
				++iCount;
				if (pxPass->m_uViewSlot != kuFluxViewSlotMain) { bAnyOnNonMainView = true; }
			}
		}
		return iCount;
	}

	u_int64 SampleVRAMBytes()
	{
#ifdef ZENITH_VULKAN
		return g_xEngine.FluxMemory().GetVMAStats().m_ulTotalUsedBytes;
#else
		return 0u;
#endif
	}

	void SampleState(bool bExpectOn, int iFrame)
	{
		bool bAnyOnNonMain = false;
		const int  iTAAPasses    = CountTAAPasses(bAnyOnNonMain);
		const bool bResolveOn    = g_xEngine.TAA().IsResolveActive();
		const int  iTotalPasses  = static_cast<int>(g_xEngine.FluxRenderer().GetRenderGraph().GetPasses().GetSize());
		const u_int64 ulVRAM     = SampleVRAMBytes();

		// Invariant 1: TAA passes present iff on (3 when on, 0 when off).
		const bool bPassPresenceOK = bExpectOn ? (iTAAPasses == 3) : (iTAAPasses == 0);
		// Invariant 2: never on a non-main view.
		const bool bViewOK = !bAnyOnNonMain;
		// Invariant 3: IsResolveActive matches requested.
		const bool bResolveMatchOK = (bResolveOn == bExpectOn);

		if (!bPassPresenceOK || !bViewOK || !bResolveMatchOK) { s_bAllInvariantsOK = false; }

		// Invariant 4: per-state total pass count is stable across cycles.
		if (bExpectOn)
		{
			if (s_iOnPassCount < 0) { s_iOnPassCount = iTotalPasses; }
			else if (iTotalPasses != s_iOnPassCount) { s_bOnCountStable = false; }
			if (s_iOnSamples == 0) { /* first */ }
			s_ulOnVRAMLast = ulVRAM; ++s_iOnSamples;
		}
		else
		{
			if (s_iOffPassCount < 0) { s_iOffPassCount = iTotalPasses; }
			else if (iTotalPasses != s_iOffPassCount) { s_bOffCountStable = false; }
			if (s_iOffSamples == 0) { s_ulOffVRAMFirst = ulVRAM; }
			s_ulOffVRAMLast = ulVRAM; ++s_iOffSamples;
		}
		++s_iSampleCount;

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TAAToggleStress] frame=%d expect=%s taaPasses=%d resolveActive=%d totalPasses=%d vram=%lluKB "
			"presenceOK=%d viewOK=%d resolveMatchOK=%d",
			iFrame, bExpectOn ? "ON" : "OFF", iTAAPasses, bResolveOn ? 1 : 0, iTotalPasses,
			(unsigned long long)(ulVRAM / 1024u), bPassPresenceOK ? 1 : 0, bViewOK ? 1 : 0, bResolveMatchOK ? 1 : 0);
	}

	void Setup_TAAToggleStress()
	{
		s_bExpectedOn      = true;
		s_iFlipCount       = 0;
		s_iSampleCount     = 0;
		s_bAllInvariantsOK = true;
		s_iOffPassCount    = -1;
		s_iOnPassCount     = -1;
		s_bOffCountStable  = true;
		s_bOnCountStable   = true;
		s_ulOffVRAMFirst   = 0;
		s_ulOffVRAMLast    = 0;
		s_ulOnVRAMLast     = 0;
		s_iOffSamples      = 0;
		s_iOnSamples       = 0;
		// Force TAA on to start from a known state (independent of debug var / --taa).
		g_xEngine.TAA().SetEnabled(true);
	}

	bool Step_TAAToggleStress(int iFrame)
	{
		// Flip every 60 frames (10 flips: frames 60,120,...,600). Toggles the requested state
		// and triggers the graph rebuild on the next frame's UpdateVelocityTargetSelection.
		if (iFrame > 0 && (iFrame % 60) == 0 && s_iFlipCount < 10)
		{
			s_bExpectedOn = !s_bExpectedOn;
			g_xEngine.TAA().SetEnabled(s_bExpectedOn);
			++s_iFlipCount;
		}

		// Sample 30 frames into each window — long after the flip's rebuild has settled.
		if (iFrame > 0 && (iFrame % 60) == 30)
		{
			SampleState(s_bExpectedOn, iFrame);
		}

		return iFrame < 660;
	}

	bool Verify_TAAToggleStress()
	{
		// Restore normal TAA control (debug var / --taa) for the rest of the process.
		g_xEngine.TAA().ClearEnabledOverride();

		const bool bCountDelta = (s_iOnPassCount >= 0 && s_iOffPassCount >= 0)
			&& (s_iOnPassCount == s_iOffPassCount + 3);

		// VRAM leak: OFF-state allocated bytes must not grow across the toggle cycles (a real
		// per-toggle leak of the velocity/history transients would accumulate). 5% tolerates
		// unrelated streaming/allocator noise; a genuine leak (~11MB x cycles) far exceeds it.
		bool bNoLeak = true;
		double fOffGrowthPct = 0.0;
		if (s_ulOffVRAMFirst > 0 && s_iOffSamples >= 2)
		{
			fOffGrowthPct = 100.0 * (double)((long long)s_ulOffVRAMLast - (long long)s_ulOffVRAMFirst) / (double)s_ulOffVRAMFirst;
			bNoLeak = (fOffGrowthPct <= 5.0);
		}
		const long long llDirectionalKB = ((long long)s_ulOnVRAMLast - (long long)s_ulOffVRAMLast) / 1024;

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TAAToggleStress] SUMMARY flips=%d samples=%d(on=%d off=%d) invariantsOK=%d offCountStable=%d onCountStable=%d "
			"offPasses=%d onPasses=%d countDelta+3=%d | vram off %lluKB->%lluKB (growth=%.2f%%) on=%lluKB directional(on-off)=%lldKB noLeak=%d",
			s_iFlipCount, s_iSampleCount, s_iOnSamples, s_iOffSamples,
			s_bAllInvariantsOK ? 1 : 0, s_bOffCountStable ? 1 : 0, s_bOnCountStable ? 1 : 0,
			s_iOffPassCount, s_iOnPassCount, bCountDelta ? 1 : 0,
			(unsigned long long)(s_ulOffVRAMFirst / 1024u), (unsigned long long)(s_ulOffVRAMLast / 1024u),
			fOffGrowthPct, (unsigned long long)(s_ulOnVRAMLast / 1024u), llDirectionalKB, bNoLeak ? 1 : 0);

		return s_bAllInvariantsOK
			&& s_bOffCountStable && s_bOnCountStable
			&& bCountDelta
			&& (s_iFlipCount == 10)
			&& (s_iOnSamples > 0) && (s_iOffSamples > 0)
			&& bNoLeak;
	}

	const Zenith_AutomatedTest g_xTAAToggleStress = {
		"TAAToggleStress",
		&Setup_TAAToggleStress,
		&Step_TAAToggleStress,
		&Verify_TAAToggleStress,
		720,
		true /* m_bRequiresGraphics */,
		true /* m_bManualOnly */
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xTAAToggleStress);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
