#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include <cmath>
#include <cstdio>
#include <cstdlib>   // std::abs(int)
#include <filesystem>
#include <string>

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_TestTGA.h"

#include "Flux/Flux_Screenshot.h"
#include "Flux/TAA/Flux_TAAImpl.h"

#include "RenderTest/Components/RenderTest_GameplayState.h"

// =====================================================================
// TAATemporalStability -- the flicker + smear gate. TWO phases, two failure
// modes, two different observables.
//
// Every other TAA test in the tree counts passes, views and VRAM; none of them
// looks at a pixel across two frames, so the whole "the screen shimmers" /
// "the sky smears" failure class was untested.
//
// --- PHASE 1: STILL camera -> temporal STABILITY ---------------------
// TAA exists to make a still image STILL, so: park a camera, hold it still, and
// compare consecutive frames.
//
// THE ASSERTION IS RELATIVE, ON PURPOSE. A golden-pixel compare is ruled out
// here (Flux/TAA/CLAUDE.md: the SSAO/SSR/SSGI denoisers accumulate, so two runs
// of the same build differ), and an absolute delta floor would just be a pin on
// that denoiser noise. So the reference is the SAME SCENE WITH TAA OFF, captured
// in the same run seconds apart: TAA must not make a still image less stable
// than no TAA at all. That self-calibrates against whatever the denoiser floor
// happens to be on this machine, this scene and this build.
//
// A correct TAA should come in comfortably UNDER the off-state number -- it is a
// temporal filter, so it damps the denoiser noise too. A broken one walks the
// image through its 8-phase Halton jitter cycle every frame and comes in far
// above it.
//
// --- PHASE 2: ROTATING camera -> temporal LAG ------------------------
// Phase 1 cannot see a missing motion vector. With the camera still, every
// correct motion vector is (0,0) -- which is exactly what an absent one reads
// as. So a pass that writes no velocity (Skybox, Primitives) is indistinguishable
// from a correct one, and the whole camera-rotation smear class was invisible.
//
// ★ AND THE PHASE-1 OBSERVABLE CANNOT BE REUSED. Inter-frame delta measures
// INSTABILITY; a missing motion vector produces the OPPOSITE -- the resolve
// fetches history at uv instead of uv-velocity, so it blends a stale image in
// and the result is over-smoothed. Consecutive frames get MORE similar, not
// less: the stability ratio moves the WRONG WAY on the very bug this phase
// exists to catch. (Phase 2 still logs the ratio, because a rotation-only
// flicker regression is worth catching too -- it is just not the primary gate.)
//
// The observable that does see it is LAG: with the yaw driven from a per-window
// frame counter, ON capture k and OFF capture k are taken at the IDENTICAL
// camera pose, so
//
//   |TAA-on[k] - TAA-off[k]|
//
// is "how far TAA displaced the picture from the truth". Correct motion vectors
// make that a sub-pixel anti-aliasing difference. A missing one makes it a
// multi-frame lag, so the natural relative reference is ONE FRAME OF CAMERA
// MOTION -- the TAA-off consecutive-frame delta, measured in the same window:
//
//   meanDelta(on[k] vs off[k])  <=  ratio * meanDelta(off[k] vs off[k+1])
//
// Same shape as phase 1 (TAA-on measured against TAA-off in the same run, so it
// self-calibrates), just against a lag reference instead of a noise floor.
//
// ★ WHAT PHASE 2 DOES *NOT* GATE, AND WHY -- READ THIS BEFORE TIGHTENING IT.
// It is a gross-regression gate on the motion vectors as a whole (terrain, mesh,
// grass, sky). It is NOT a gate on the SKY's motion vector specifically, and it
// cannot be made into one, for a structural reason:
//
//   the resolve clips history into the current 3x3 neighbourhood's colour range,
//   so however wrong a motion vector is, the resulting error is bounded by that
//   neighbourhood's sigma.
//
// Both of this engine's skies are smooth gradients -- the cubemap band measures a
// local 3x3 sigma of ~0.6 of an 8-bit level -- so the bound is under one LSB and
// there is simply no smear to measure. A/B'd with --taa-no-skyvelocity, which
// suppresses the sky's motion vectors entirely: this number moves 0.012809 ->
// 0.012805 on the atmosphere sky (0.03%) and 0.014904 -> 0.014500 on the cubemap
// (2.7%), against ~0.05% run-to-run noise. Real, correct, and far too small to
// hang a stable threshold on.
//
// So the sky's motion vector is gated where the evidence is unambiguous instead:
// TAAToggleStress asserts the "Skybox Velocity" pass exists iff the velocity latch
// is on and renders the main view, and Flux_TAAJitter.Tests.inl pins the w = 0
// point-at-infinity maths (camera translation must not move the sky; yaw must, and
// linearly). The threshold here is deliberately loose -- it is watching for a
// motion vector that is WRONG, not one that is subtly imprecise.
//
// Only the editor VIEWPORT rect is sampled: Flux_Screenshot dumps the whole
// swapchain, and the ImGui chrome contains a live profiling graph that redraws
// every frame and would swamp the measurement.
//
// Windowed + Vulkan only (m_bRequiresGraphics) -- a Null build rasterises
// nothing and the dump is a no-op there.
//   rendertest.exe --automated-test TAATemporalStability --skip-unit-tests
// =====================================================================

namespace
{
	// --- schedule -------------------------------------------------------
	// Park, converge, capture; flip TAA off, converge again, capture again.
	// The settle windows must outlast BOTH the TAA accumulation window
	// (~1/BlendMinAlpha = ~10 frames) and the screen-space denoisers, which are
	// far slower -- hence 100 rather than 20.
	constexpr int iTS_PARK_END       = 40;
	constexpr int iTS_ON_SETTLE_END  = iTS_PARK_END + 100;
	constexpr int iTS_CAPTURE_FRAMES = 6;                                  // -> 5 consecutive-frame deltas
	constexpr int iTS_ON_CAP_END     = iTS_ON_SETTLE_END + iTS_CAPTURE_FRAMES;
	constexpr int iTS_OFF_SETTLE_END = iTS_ON_CAP_END + 100;
	constexpr int iTS_OFF_CAP_END    = iTS_OFF_SETTLE_END + iTS_CAPTURE_FRAMES;

	// --- phase 2 schedule: rotating camera ------------------------------
	// Re-park onto the sky pose, put TAA back on, then run the SAME
	// settle/capture shape twice with the camera yawing at a constant rate.
	constexpr int iTS_ROT_PARK_END       = iTS_OFF_CAP_END + 20;
	constexpr int iTS_ROT_ON_SETTLE_END  = iTS_ROT_PARK_END + 100;
	constexpr int iTS_ROT_ON_CAP_END     = iTS_ROT_ON_SETTLE_END + iTS_CAPTURE_FRAMES;
	constexpr int iTS_ROT_OFF_SETTLE_END = iTS_ROT_ON_CAP_END + 100;
	constexpr int iTS_ROT_OFF_CAP_END    = iTS_ROT_OFF_SETTLE_END + iTS_CAPTURE_FRAMES;
	// Flux_Screenshot consumes one pending request per EndFrame, and Verify runs
	// mid-frame -- so the last request needs frames in hand to reach the disk.
	constexpr int iTS_DRAIN_END      = iTS_ROT_OFF_CAP_END + 12;

	// --- pass criterion -------------------------------------------------
	// Derived from measurement, not taste. The OBSERVED line this test logs is the
	// authority for re-deriving them (same convention ZM_InteriorTintPixels uses for
	// its tint gap).
	//
	//   BROKEN (measured at the pre-fix HEAD): on=0.005399  off=0.001089  ratio 4.96
	//     ... reprojected expected-depth only:  on=0.003229  off=0.001089  ratio 2.97
	//   FIXED (+ neighbourhood-range test):    on=0.001302  off=0.001085  ratio 1.20
	//   Reference: rejection disabled entirely on=0.001254  off=0.001092  ratio 1.15
	//
	// That last line is the floor this gate can reach, so the shipped 1.20 is within 4%
	// of "no disocclusion test at all" while still rejecting genuine disocclusions.
	//
	// The off-state is the noise floor of the screen-space denoisers plus grass wind
	// and reads ~0.00109 with remarkable consistency (five pairs within 0.5% of each
	// other), which is what makes it a usable reference. A working TAA should land at
	// or below it -- it is a temporal filter, so it damps that noise as well as the
	// aliasing.
	constexpr double fTS_STABILITY_RATIO = 1.5;    // on <= 1.5x off ...
	constexpr double fTS_STABILITY_FLOOR = 0.0005; // ... + a small floor, so a near-frozen
	                                               //     off-state cannot make the bar
	                                               //     unreachably tight.
	// => budget ~= 0.00212 against the measured off-state: 2.5x BELOW the broken
	//    result, so the red is unambiguous. Confirm the green has headroom too once
	//    the fix lands, and tighten here if it turns out to be sitting close.
	constexpr double fTS_MOVING_PIXEL_EPS = 2.0 / 255.0;  // "visibly changed" for the diagnostic fraction

	// --- phase 2 pass criterion (LAG) -----------------------------------
	// ON capture k and OFF capture k are the same camera pose, so their
	// difference is TAA's displacement of the picture. One frame of camera
	// motion -- the TAA-off consecutive-frame delta -- is the reference:
	// a resolve that tracks its motion vectors lands well inside it, one that
	// drops them lags by the history's whole accumulation window.
	//
	//   MEASURED (RenderTest campus, 0.006 rad/frame yaw): ratio(lag/motion) 0.46 - 0.48
	//   across atmosphere/cubemap skies and with sky velocity present or suppressed.
	//   Run-to-run reproducibility is ~0.05%.
	//
	// The budget is 1.0x -- roughly a 2x margin over the measured value -- because
	// this is watching for a motion vector that is WRONG (a whole missing writer, a
	// sign flip, a stale matrix), which costs multiples of a frame of motion, not
	// the few percent a single smooth surface can contribute. See the header note on
	// why a tighter threshold here would be pinning noise rather than gating a defect.
	// The OBSERVED line this test logs is the authority for re-deriving it (same
	// convention as phase 1 and as ZM_InteriorTintPixels' tint gap).
	constexpr double fTS_ROT_LAG_RATIO = 1.0;      // lag <= 1.0x one frame of motion ...
	constexpr double fTS_ROT_LAG_FLOOR = 0.0020;   // ... + a floor, so a nearly-frozen
	                                               //     reference cannot make it unreachable.
	// The stability ratio is still computed and logged for the rotating phase, but it
	// is a LOOSE guard only -- see the header: a dropped motion vector over-smooths, so
	// this number goes DOWN on the bug it would nominally be watching for. It is here to
	// catch a rotation-only FLICKER regression, nothing more.
	constexpr double fTS_ROT_STABILITY_RATIO = 2.0;
	constexpr double fTS_ROT_STABILITY_FLOOR = 0.0050;

	// --- camera ---------------------------------------------------------
	// Photo mode parks the follow camera at player + offset with a scripted
	// yaw/pitch (RenderTest_FollowCameraComponent). Looking back from +X+Z with a
	// downward pitch frames terrain, trees and the player -- high-frequency edge
	// content, which is where jitter shimmer shows. A sky-facing view (the
	// RenderTest default) would measure almost nothing; see RenderTest/CLAUDE.md.
	constexpr float fTS_CAM_OFFSET_X = 5.0f;
	constexpr float fTS_CAM_OFFSET_Y = 2.2f;
	constexpr float fTS_CAM_OFFSET_Z = 5.0f;
	constexpr float fTS_CAM_YAW      = 2.3562f;   // atan2(ox, -oz) for (+X,+Z) -> looks back at the player
	constexpr float fTS_CAM_PITCH    = -0.22f;

	// Phase 2 parks above the player and pitches UP, so the sampled rect is
	// dominated by SKY with the campus tree line along its lower edge. The sky is
	// the surface under test; the tree line keeps some correctly-vectored geometry
	// in frame so a regression there is not silently masked.
	constexpr float fTS_ROT_OFFSET_X = 0.0f;
	constexpr float fTS_ROT_OFFSET_Y = 2.5f;
	constexpr float fTS_ROT_OFFSET_Z = 0.0f;
	constexpr float fTS_ROT_YAW0     = 2.3562f;
	constexpr float fTS_ROT_PITCH    = 0.30f;   // up: ~70% sky in the sampled rect
	// Radians per FRAME, not per second: the yaw is a pure function of the window's
	// frame counter, which is what makes ON capture k and OFF capture k the same
	// pose (the harness pins dt, but frame-indexing is immune to it either way).
	// 0.006 rad/frame ~= 0.34 deg/frame ~= 7 px/frame at 1280x720/60deg -- inside
	// the 32 px velocity-reject ramp, so the blend stays history-heavy and a
	// dropped motion vector has room to show as lag rather than as rejection.
	constexpr float fTS_ROT_YAW_RATE = 0.006f;

	// Fraction trimmed off each side of the viewport before sampling, so dock
	// borders and the viewport's own edge never enter the measurement.
	constexpr double fTS_RECT_INSET = 0.12;

	struct TSMetrics
	{
		double m_fMeanDelta  = 0.0;   // mean |RGB delta| over the rect, normalised to [0,1]
		double m_fMaxDelta   = 0.0;   // worst single pixel
		double m_fMovingFrac = 0.0;   // fraction of pixels that visibly changed
		int    m_iPairs      = 0;
	};

	// Fixed buffers, not std::string: these are namespace-scope and therefore still
	// alive at Zenith_MemoryManagement's shutdown leak checkpoint (which runs before
	// static destruction), so a std::string here reports as a leak every run.
	struct TSShotPath { char m_aszPath[260] = {}; };

	bool        g_bTSFailed = false;
	const char* g_szTSFailure = nullptr;
	bool        g_bTSSkipped = false;
	TSShotPath  g_axTSOnShots[iTS_CAPTURE_FRAMES];
	TSShotPath  g_axTSOffShots[iTS_CAPTURE_FRAMES];
	TSShotPath  g_axTSRotOnShots[iTS_CAPTURE_FRAMES];
	TSShotPath  g_axTSRotOffShots[iTS_CAPTURE_FRAMES];
	int         g_iTSOnRequested  = 0;
	int         g_iTSOffRequested = 0;
	int         g_iTSRotOnRequested  = 0;
	int         g_iTSRotOffRequested = 0;

	void TSFail(const char* szReason)
	{
		if (g_szTSFailure == nullptr)
		{
			g_szTSFailure = szReason;
		}
		g_bTSFailed = true;
	}

	std::filesystem::path TSArtifactDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "rendertest" / "taa_stability";
	}

	// The sampled rect: the editor viewport, inset, clamped into the image. Falls
	// back to a centred half-frame when the tools query seam is unavailable or the
	// dock has not reached a sensible size.
	void TSSampleRect(const Zenith_TestTGAImage& xImage,
		uint32_t& uX0, uint32_t& uY0, uint32_t& uX1, uint32_t& uY1)
	{
		double fLeft   = 0.25 * xImage.m_uWidth;
		double fTop    = 0.25 * xImage.m_uHeight;
		double fWidth  = 0.50 * xImage.m_uWidth;
		double fHeight = 0.50 * xImage.m_uHeight;

		if (g_xEditorQuery.m_pfnGetViewportPos != nullptr
			&& g_xEditorQuery.m_pfnGetViewportSize != nullptr)
		{
			const Zenith_Maths::Vector2 xPos  = g_xEditorQuery.m_pfnGetViewportPos();
			const Zenith_Maths::Vector2 xSize = g_xEditorQuery.m_pfnGetViewportSize();
			if (xSize.x >= 320.0f && xSize.y >= 180.0f)
			{
				fLeft   = static_cast<double>(xPos.x) + fTS_RECT_INSET * xSize.x;
				fTop    = static_cast<double>(xPos.y) + fTS_RECT_INSET * xSize.y;
				fWidth  = (1.0 - 2.0 * fTS_RECT_INSET) * xSize.x;
				fHeight = (1.0 - 2.0 * fTS_RECT_INSET) * xSize.y;
			}
		}

		const double fMaxX = static_cast<double>(xImage.m_uWidth);
		const double fMaxY = static_cast<double>(xImage.m_uHeight);
		double fRight  = fLeft + fWidth;
		double fBottom = fTop + fHeight;
		fLeft   = (fLeft   < 0.0)   ? 0.0   : fLeft;
		fTop    = (fTop    < 0.0)   ? 0.0   : fTop;
		fRight  = (fRight  > fMaxX) ? fMaxX : fRight;
		fBottom = (fBottom > fMaxY) ? fMaxY : fBottom;

		uX0 = static_cast<uint32_t>(fLeft);
		uY0 = static_cast<uint32_t>(fTop);
		uX1 = static_cast<uint32_t>(fRight);
		uY1 = static_cast<uint32_t>(fBottom);
	}

	// Mean absolute per-channel difference between two captures over the rect.
	bool TSComparePair(const Zenith_TestTGAImage& xA, const Zenith_TestTGAImage& xB,
		double& fMeanOut, double& fMaxOut, double& fMovingOut)
	{
		if (!xA.IsValid() || !xB.IsValid()
			|| xA.m_uWidth != xB.m_uWidth || xA.m_uHeight != xB.m_uHeight)
		{
			return false;
		}

		uint32_t uX0 = 0u, uY0 = 0u, uX1 = 0u, uY1 = 0u;
		TSSampleRect(xA, uX0, uY0, uX1, uY1);
		if (uX1 <= uX0 || uY1 <= uY0)
		{
			return false;
		}

		double   fSum     = 0.0;
		double   fWorst   = 0.0;
		uint64_t ulMoving = 0u;
		uint64_t ulCount  = 0u;
		for (uint32_t uY = uY0; uY < uY1; ++uY)
		{
			for (uint32_t uX = uX0; uX < uX1; ++uX)
			{
				const uint8_t* puA = xA.GetPixelBGRA(uX, uY);
				const uint8_t* puB = xB.GetPixelBGRA(uX, uY);
				// BGRA: the alpha channel is not scene content, so only BGR is compared.
				const int iDB = static_cast<int>(puA[0]) - static_cast<int>(puB[0]);
				const int iDG = static_cast<int>(puA[1]) - static_cast<int>(puB[1]);
				const int iDR = static_cast<int>(puA[2]) - static_cast<int>(puB[2]);
				const double fDelta =
					(static_cast<double>(std::abs(iDB))
					+ static_cast<double>(std::abs(iDG))
					+ static_cast<double>(std::abs(iDR))) / (3.0 * 255.0);
				fSum += fDelta;
				fWorst = (fDelta > fWorst) ? fDelta : fWorst;
				if (fDelta > fTS_MOVING_PIXEL_EPS) { ++ulMoving; }
				++ulCount;
			}
		}

		if (ulCount == 0u) { return false; }
		fMeanOut   = fSum / static_cast<double>(ulCount);
		fMaxOut    = fWorst;
		fMovingOut = static_cast<double>(ulMoving) / static_cast<double>(ulCount);
		return true;
	}

	// Walk a capture sequence and average the consecutive-frame deltas.
	bool TSMeasureSequence(const TSShotPath* paxPaths, int iCount,
		const char* szLabel, TSMetrics& xOut)
	{
		double fSumMean   = 0.0;
		double fSumMoving = 0.0;
		double fWorst     = 0.0;
		int    iPairs     = 0;

		Zenith_TestTGAImage xPrev;
		for (int i = 0; i < iCount; ++i)
		{
			Zenith_TestTGAImage xCur;
			if (!Zenith_TestLoadTGA(paxPaths[i].m_aszPath, xCur))
			{
				Zenith_Error(LOG_CATEGORY_RENDERER,
					"[TAATemporalStability] %s capture %d missing/invalid: %s",
					szLabel, i, paxPaths[i].m_aszPath);
				return false;
			}
			if (i > 0)
			{
				double fMean = 0.0, fMax = 0.0, fMoving = 0.0;
				if (!TSComparePair(xPrev, xCur, fMean, fMax, fMoving))
				{
					Zenith_Error(LOG_CATEGORY_RENDERER,
						"[TAATemporalStability] %s captures %d/%d are not comparable",
						szLabel, i - 1, i);
					return false;
				}
				fSumMean   += fMean;
				fSumMoving += fMoving;
				fWorst      = (fMax > fWorst) ? fMax : fWorst;
				++iPairs;
				Zenith_Log(LOG_CATEGORY_RENDERER,
					"[TAATemporalStability] %s pair %d->%d meanDelta=%.6f maxDelta=%.4f movingFrac=%.4f",
					szLabel, i - 1, i, fMean, fMax, fMoving);
			}
			// Hand the just-loaded image to xPrev for the next pair (the image type
			// is move-only-by-hand: it owns a raw buffer and copying is deleted).
			delete[] xPrev.m_puPixels;
			xPrev.m_puPixels = xCur.m_puPixels;
			xPrev.m_uWidth   = xCur.m_uWidth;
			xPrev.m_uHeight  = xCur.m_uHeight;
			xCur.m_puPixels  = nullptr;
			xCur.m_uWidth    = 0u;
			xCur.m_uHeight   = 0u;
		}

		if (iPairs == 0) { return false; }
		xOut.m_fMeanDelta  = fSumMean / static_cast<double>(iPairs);
		xOut.m_fMaxDelta   = fWorst;
		xOut.m_fMovingFrac = fSumMoving / static_cast<double>(iPairs);
		xOut.m_iPairs      = iPairs;
		return true;
	}

	// Phase 2's LAG measurement: capture k of one sequence against capture k of the
	// other. Only meaningful because the yaw is driven from a per-window frame
	// counter, so the two sequences were shot at the SAME camera poses.
	bool TSMeasureMatchedPairs(const TSShotPath* paxA, const TSShotPath* paxB, int iCount,
		const char* szLabel, TSMetrics& xOut)
	{
		double fSumMean   = 0.0;
		double fSumMoving = 0.0;
		double fWorst     = 0.0;
		int    iPairs     = 0;

		for (int i = 0; i < iCount; ++i)
		{
			Zenith_TestTGAImage xA;
			Zenith_TestTGAImage xB;
			if (!Zenith_TestLoadTGA(paxA[i].m_aszPath, xA)
				|| !Zenith_TestLoadTGA(paxB[i].m_aszPath, xB))
			{
				Zenith_Error(LOG_CATEGORY_RENDERER,
					"[TAATemporalStability] %s pair %d missing/invalid (%s | %s)",
					szLabel, i, paxA[i].m_aszPath, paxB[i].m_aszPath);
				return false;
			}

			double fMean = 0.0, fMax = 0.0, fMoving = 0.0;
			if (!TSComparePair(xA, xB, fMean, fMax, fMoving))
			{
				Zenith_Error(LOG_CATEGORY_RENDERER,
					"[TAATemporalStability] %s pair %d is not comparable", szLabel, i);
				return false;
			}
			fSumMean   += fMean;
			fSumMoving += fMoving;
			fWorst      = (fMax > fWorst) ? fMax : fWorst;
			++iPairs;
			Zenith_Log(LOG_CATEGORY_RENDERER,
				"[TAATemporalStability] %s pair %d meanDelta=%.6f maxDelta=%.4f movingFrac=%.4f",
				szLabel, i, fMean, fMax, fMoving);
		}

		if (iPairs == 0) { return false; }
		xOut.m_fMeanDelta  = fSumMean / static_cast<double>(iPairs);
		xOut.m_fMaxDelta   = fWorst;
		xOut.m_fMovingFrac = fSumMoving / static_cast<double>(iPairs);
		xOut.m_iPairs      = iPairs;
		return true;
	}

	void Setup_TAATemporalStability()
	{
		g_bTSFailed          = false;
		g_szTSFailure        = nullptr;
		g_bTSSkipped         = false;
		g_iTSOnRequested     = 0;
		g_iTSOffRequested    = 0;
		g_iTSRotOnRequested  = 0;
		g_iTSRotOffRequested = 0;

		if constexpr (Zenith_IsNullRenderer())
		{
			// m_bRequiresGraphics already excludes this build, but a Null run that
			// somehow reached Setup would compare six identical no-op dumps and
			// "pass" -- refuse rather than report a meaningless green.
			g_bTSSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"TAATemporalStability needs a real swapchain to read back");
			return;
		}

		std::error_code xError;
		const std::filesystem::path xDir = TSArtifactDir();
		std::filesystem::create_directories(xDir, xError);
		if (!std::filesystem::exists(xDir, xError))
		{
			TSFail("could not create Build/artifacts/rendertest/taa_stability");
			return;
		}

		const std::string strDir = xDir.string();
		for (int i = 0; i < iTS_CAPTURE_FRAMES; ++i)
		{
			std::snprintf(g_axTSOnShots[i].m_aszPath, sizeof(g_axTSOnShots[i].m_aszPath),
				"%s/taa_on_%d.tga", strDir.c_str(), i);
			std::snprintf(g_axTSOffShots[i].m_aszPath, sizeof(g_axTSOffShots[i].m_aszPath),
				"%s/taa_off_%d.tga", strDir.c_str(), i);
			std::snprintf(g_axTSRotOnShots[i].m_aszPath, sizeof(g_axTSRotOnShots[i].m_aszPath),
				"%s/taa_rot_on_%d.tga", strDir.c_str(), i);
			std::snprintf(g_axTSRotOffShots[i].m_aszPath, sizeof(g_axTSRotOffShots[i].m_aszPath),
				"%s/taa_rot_off_%d.tga", strDir.c_str(), i);
			// A stale dump from a previous run would be read as this run's result.
			std::remove(g_axTSOnShots[i].m_aszPath);
			std::remove(g_axTSOffShots[i].m_aszPath);
			std::remove(g_axTSRotOnShots[i].m_aszPath);
			std::remove(g_axTSRotOffShots[i].m_aszPath);
		}

		// Park the camera and start from a known TAA state, independent of the
		// debug var / --taa (TAAToggleStress uses the same override).
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		RenderTest_GameplayState::s_fPhotoOffsetX = fTS_CAM_OFFSET_X;
		RenderTest_GameplayState::s_fPhotoOffsetY = fTS_CAM_OFFSET_Y;
		RenderTest_GameplayState::s_fPhotoOffsetZ = fTS_CAM_OFFSET_Z;
		RenderTest_GameplayState::s_fPhotoYaw     = fTS_CAM_YAW;
		RenderTest_GameplayState::s_fPhotoPitch   = fTS_CAM_PITCH;
		g_xEngine.TAA().SetEnabled(true);
	}

	bool Step_TAATemporalStability(int iFrame)
	{
		if (g_bTSSkipped || g_bTSFailed)
		{
			return false;
		}

		if (iFrame == iTS_ON_CAP_END)
		{
			// Flip to the reference state. The rebuild this triggers also
			// invalidates history, which is exactly why the off window settles
			// for as long as the on window did.
			g_xEngine.TAA().SetEnabled(false);
		}
		else if (iFrame == iTS_OFF_CAP_END)
		{
			// Phase 1 done: re-park onto the sky pose and put TAA back on.
			RenderTest_GameplayState::s_fPhotoOffsetX = fTS_ROT_OFFSET_X;
			RenderTest_GameplayState::s_fPhotoOffsetY = fTS_ROT_OFFSET_Y;
			RenderTest_GameplayState::s_fPhotoOffsetZ = fTS_ROT_OFFSET_Z;
			RenderTest_GameplayState::s_fPhotoPitch   = fTS_ROT_PITCH;
			g_xEngine.TAA().SetEnabled(true);
		}
		else if (iFrame == iTS_ROT_ON_CAP_END)
		{
			g_xEngine.TAA().SetEnabled(false);
		}

		// Rotating phase: yaw is a pure function of the frame index WITHIN the
		// current window, and both windows use the same origin -- so ON capture k
		// and OFF capture k are shot at the identical camera pose, which is what
		// makes the lag comparison in Verify meaningful. The camera keeps turning
		// through the settle windows too, so the history converges to a MOVING
		// steady state rather than being ambushed by motion at capture time.
		if (iFrame >= iTS_ROT_PARK_END)
		{
			const int iRotOrigin = (iFrame < iTS_ROT_ON_CAP_END) ? iTS_ROT_PARK_END : iTS_ROT_ON_CAP_END;
			RenderTest_GameplayState::s_fPhotoYaw =
				fTS_ROT_YAW0 + fTS_ROT_YAW_RATE * static_cast<float>(iFrame - iRotOrigin);
		}

		if (iFrame >= iTS_ON_SETTLE_END && iFrame < iTS_ON_CAP_END)
		{
			// One request per frame: Flux_Screenshot keeps a single pending slot
			// and a second request in the same frame would overwrite the first.
			Flux_Screenshot::RequestDump(g_axTSOnShots[iFrame - iTS_ON_SETTLE_END].m_aszPath);
			++g_iTSOnRequested;
		}
		else if (iFrame >= iTS_OFF_SETTLE_END && iFrame < iTS_OFF_CAP_END)
		{
			Flux_Screenshot::RequestDump(g_axTSOffShots[iFrame - iTS_OFF_SETTLE_END].m_aszPath);
			++g_iTSOffRequested;
		}
		else if (iFrame >= iTS_ROT_ON_SETTLE_END && iFrame < iTS_ROT_ON_CAP_END)
		{
			Flux_Screenshot::RequestDump(g_axTSRotOnShots[iFrame - iTS_ROT_ON_SETTLE_END].m_aszPath);
			++g_iTSRotOnRequested;
		}
		else if (iFrame >= iTS_ROT_OFF_SETTLE_END && iFrame < iTS_ROT_OFF_CAP_END)
		{
			Flux_Screenshot::RequestDump(g_axTSRotOffShots[iFrame - iTS_ROT_OFF_SETTLE_END].m_aszPath);
			++g_iTSRotOffRequested;
		}

		const char* szWindow = nullptr;
		if      (iFrame == iTS_ON_SETTLE_END)       { szWindow = "TAA-ON (still)"; }
		else if (iFrame == iTS_OFF_SETTLE_END)      { szWindow = "TAA-OFF (still)"; }
		else if (iFrame == iTS_ROT_ON_SETTLE_END)   { szWindow = "TAA-ON (rotating)"; }
		else if (iFrame == iTS_ROT_OFF_SETTLE_END)  { szWindow = "TAA-OFF (rotating)"; }
		if (szWindow != nullptr)
		{
			Zenith_Log(LOG_CATEGORY_RENDERER,
				"[TAATemporalStability] frame=%d capturing %s (resolveActive=%d yaw=%.4f)",
				iFrame, szWindow, g_xEngine.TAA().IsResolveActive() ? 1 : 0,
				static_cast<double>(RenderTest_GameplayState::s_fPhotoYaw));
		}

		return iFrame < iTS_DRAIN_END;
	}

	bool Verify_TAATemporalStability()
	{
		if (g_bTSSkipped)
		{
			return true;   // finalised as SKIPPED by the harness
		}
		if (g_bTSFailed)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER, "[TAATemporalStability] %s",
				g_szTSFailure ? g_szTSFailure : "setup failed");
			return false;
		}
		if (g_iTSOnRequested != iTS_CAPTURE_FRAMES || g_iTSOffRequested != iTS_CAPTURE_FRAMES
			|| g_iTSRotOnRequested != iTS_CAPTURE_FRAMES || g_iTSRotOffRequested != iTS_CAPTURE_FRAMES)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TAATemporalStability] capture schedule did not complete "
				"(still on=%d off=%d, rotating on=%d off=%d, expected %d each)",
				g_iTSOnRequested, g_iTSOffRequested,
				g_iTSRotOnRequested, g_iTSRotOffRequested, iTS_CAPTURE_FRAMES);
			return false;
		}

		// --- phase 1: still camera, temporal stability ----------------------
		TSMetrics xOn;
		TSMetrics xOff;
		if (!TSMeasureSequence(g_axTSOnShots, iTS_CAPTURE_FRAMES, "TAA-ON", xOn)
			|| !TSMeasureSequence(g_axTSOffShots, iTS_CAPTURE_FRAMES, "TAA-OFF", xOff))
		{
			return false;
		}

		const double fBudget = fTS_STABILITY_RATIO * xOff.m_fMeanDelta + fTS_STABILITY_FLOOR;
		const bool   bStable = (xOn.m_fMeanDelta <= fBudget);

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TAATemporalStability] OBSERVED still on meanDelta=%.6f max=%.4f moving=%.4f (%d pairs) | "
			"off meanDelta=%.6f max=%.4f moving=%.4f (%d pairs) | ratio(on/off)=%.3f "
			"budget=%.6f (%.2fx off + %.4f) -> %s",
			xOn.m_fMeanDelta, xOn.m_fMaxDelta, xOn.m_fMovingFrac, xOn.m_iPairs,
			xOff.m_fMeanDelta, xOff.m_fMaxDelta, xOff.m_fMovingFrac, xOff.m_iPairs,
			(xOff.m_fMeanDelta > 0.0) ? (xOn.m_fMeanDelta / xOff.m_fMeanDelta) : -1.0,
			fBudget, fTS_STABILITY_RATIO, fTS_STABILITY_FLOOR,
			bStable ? "STABLE" : "FLICKERING");

		if (!bStable)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TAATemporalStability] TAA is LESS temporally stable than no TAA on a still camera: "
				"on=%.6f > budget=%.6f. History is being rejected or the resolve is not converging; "
				"the captures are in Build/artifacts/rendertest/taa_stability.",
				xOn.m_fMeanDelta, fBudget);
		}

		// --- phase 2: rotating camera, temporal lag -------------------------
		TSMetrics xRotOn;
		TSMetrics xRotOff;
		TSMetrics xRotLag;
		if (!TSMeasureSequence(g_axTSRotOnShots, iTS_CAPTURE_FRAMES, "ROT-TAA-ON", xRotOn)
			|| !TSMeasureSequence(g_axTSRotOffShots, iTS_CAPTURE_FRAMES, "ROT-TAA-OFF", xRotOff)
			|| !TSMeasureMatchedPairs(g_axTSRotOnShots, g_axTSRotOffShots, iTS_CAPTURE_FRAMES,
				"ROT-LAG(on-vs-off)", xRotLag))
		{
			return false;
		}

		// The reference is ONE FRAME of camera motion, taken from the TAA-off
		// sequence in the same window (so it self-calibrates against the actual
		// rotation rate, scene and machine).
		const double fLagBudget = fTS_ROT_LAG_RATIO * xRotOff.m_fMeanDelta + fTS_ROT_LAG_FLOOR;
		const bool   bTracking  = (xRotLag.m_fMeanDelta <= fLagBudget);

		const double fRotStabBudget = fTS_ROT_STABILITY_RATIO * xRotOff.m_fMeanDelta + fTS_ROT_STABILITY_FLOOR;
		const bool   bRotStable     = (xRotOn.m_fMeanDelta <= fRotStabBudget);

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TAATemporalStability] OBSERVED rotating lag(on-vs-off, matched pose) meanDelta=%.6f max=%.4f "
			"moving=%.4f (%d pairs) | one frame of motion (off consecutive)=%.6f | ratio(lag/motion)=%.3f "
			"budget=%.6f (%.2fx motion + %.4f) -> %s",
			xRotLag.m_fMeanDelta, xRotLag.m_fMaxDelta, xRotLag.m_fMovingFrac, xRotLag.m_iPairs,
			xRotOff.m_fMeanDelta,
			(xRotOff.m_fMeanDelta > 0.0) ? (xRotLag.m_fMeanDelta / xRotOff.m_fMeanDelta) : -1.0,
			fLagBudget, fTS_ROT_LAG_RATIO, fTS_ROT_LAG_FLOOR,
			bTracking ? "TRACKING" : "SMEARING");

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TAATemporalStability] OBSERVED rotating stability on meanDelta=%.6f moving=%.4f | "
			"off meanDelta=%.6f moving=%.4f | ratio(on/off)=%.3f budget=%.6f -> %s "
			"(loose guard only: a dropped motion vector LOWERS this number -- see the header)",
			xRotOn.m_fMeanDelta, xRotOn.m_fMovingFrac, xRotOff.m_fMeanDelta, xRotOff.m_fMovingFrac,
			(xRotOff.m_fMeanDelta > 0.0) ? (xRotOn.m_fMeanDelta / xRotOff.m_fMeanDelta) : -1.0,
			fRotStabBudget, bRotStable ? "STABLE" : "FLICKERING");

		if (!bTracking)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TAATemporalStability] TAA LAGS the picture under camera rotation: at matched camera "
				"poses the resolved image differs from the un-resolved one by %.6f, more than the "
				"%.6f budget (%.2f x one frame of camera motion). Something visible is being resolved "
				"with a WRONG or ABSENT motion vector, so its history is fetched at the wrong place; "
				"the captures are in Build/artifacts/rendertest/taa_stability.",
				xRotLag.m_fMeanDelta, fLagBudget, fTS_ROT_LAG_RATIO);
		}
		if (!bRotStable)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TAATemporalStability] TAA is less temporally stable than no TAA under camera rotation: "
				"on=%.6f > budget=%.6f.", xRotOn.m_fMeanDelta, fRotStabBudget);
		}

		return bStable && bTracking && bRotStable;
	}

	void Teardown_TAATemporalStability()
	{
		// Neither of these is owned by an entity or the scene, so the harness's
		// world reset will not undo them.
		RenderTest_GameplayState::s_bPhotoModeActive = false;
		g_xEngine.TAA().ClearEnabledOverride();
	}

	const Zenith_AutomatedTest g_xTAATemporalStability = {
		"TAATemporalStability",
		&Setup_TAATemporalStability,
		&Step_TAATemporalStability,
		&Verify_TAATemporalStability,
		iTS_DRAIN_END + 60,
		true /* m_bRequiresGraphics */,
		false /* m_bManualOnly */,
		&Teardown_TAATemporalStability
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xTAATemporalStability);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
