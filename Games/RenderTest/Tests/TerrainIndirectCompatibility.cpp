#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

// =====================================================================
// TerrainIndirectCompatibility — graphics-required A/B gate for the
// terrain indirect-count compatibility plan (Phase 7).
//
// Windowed + Vulkan only (m_bRequiresGraphics = true): a Null build
// rasterises nothing (the dump is a no-op and DownloadBufferData zero-fills
// the destination, so an A/B comparison there would compare two no-ops and
// "pass" meaninglessly). The harness skips this test entirely in Null builds
// through the m_bRequiresGraphics path.
//
// Three independent gates, per the plan's acceptance criteria:
//
//   1. NON-VACUITY. Debug mode 13 writes a terrain-only, unlit magenta
//      sentinel. The test classifies that sentinel inside the inset editor
//      viewport and requires a fixed minimum coverage. Generic non-black
//      pixels never count, so sky/UI/other meshes cannot make a missing
//      terrain draw pass.
//
//   2. STALE TAIL. Slow test-only GPU readbacks assert many -> few -> zero ->
//      many visible counts and all five words of every padded tail record.
//
//   3. EQUIVALENCE. Compare matched native/auto and forced-padded captures
//      with frozen mean/max RGB-delta budgets. The budget is FROZEN
//      from the native baseline and capped — never derived from the
//      fallback candidate under test — so a blank/identical fallback
//      cannot game the threshold.
//
// Stale-tail stress sequence (the spec's the central correctness invariant
// for the zero-padded fallback — many → few → none visibility must not
// replay stale chunks):
//   settle at a pose with many visible chunks;
//   move to a pose with few visible chunks;
//   assert the read-back visible count is small (and on a cull-all hook,
//   zero);
//   return to the original pose;
//   read the count and all 4,096 command records in test code via the
//   slow DownloadBufferData path — assert the live prefix contains
//   structurally valid commands and every tail record has all five words
//   zero.
//
// THREE-PROCESS SHAPE: the test does NOT mutate backend mode while worker
// recording is live (the plan forbids it). The dedicated wrapper
// `Games/RenderTest/RunTerrainIndirectCompatibility.ps1` launches the exe
// three times — `auto`, `padded`, and `single` — each in its own process,
// writing independently named artifacts; the wrapper compares the artifacts
// after all three processes finish.
// This separation keeps the boot-time override immutable per process, which
// is the safety contract the plan demands.
//
// Within ONE process invocation this test runs only ONE of the three A/B
// arms, selected by the parsed `--indirect-count-mode`. The screenshot
// artifact path encodes the mode so the wrapper can match the two
// processes' outputs.
//
//   rendertest.exe --automated-test TerrainIndirectCompatibility \
//     --indirect-count-mode=auto --skip-unit-tests --skip-tool-exports
//   rendertest.exe --automated-test TerrainIndirectCompatibility \
//     --indirect-count-mode=padded --skip-unit-tests --skip-tool-exports
// =====================================================================

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_TestTGA.h"

#include "Flux/Flux_Buffers.h"               // Flux_IndirectBuffer (for DownloadBufferData's argument buffer accessors)
#include "Flux/Flux_Screenshot.h"
#include "Flux/TAA/Flux_TAAImpl.h"

#include "Flux/Backend/Flux_IndirectDraw.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"

#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Query.h"

#include "EntityComponent/Components/Zenith_TerrainComponent.h"

#include "RenderTest/Components/RenderTest_GameplayState.h"

// Telemetry: the ZENITH_TESTING per-device recorder counters (Phase 6) —
// used to prove WHICH execution tier ran (native vs padded-multi vs
// padded-single), not just that the CLI flag was set. Only compiled in a
// Vulkan build (the entire Zenith/Vulkan/ directory is compiled out of
// Null/D3D12 configs). The counters are zero-cost in production (gated
// on ZENITH_TESTING in the recorder).
#ifdef ZENITH_VULKAN
#include "Vulkan/Zenith_Vulkan.h"
#endif

namespace
{
	// ---- Schedule -------------------------------------------------------
	// A many-visible settle, explicit few-visible and cull-all transitions
	// (read back at each pose), a return to the many-visible pose, then a
	// sentinel capture for the A/B compare. Times in frames at fixed-dt 1/60.
	//
	// KEY CORRECTNESS RULE: the three readbacks happen at distinct camera
	// poses after 60-frame settle windows: FEW, deterministic CULL-ALL, then
	// RETURN-to-many. The previous design sampled the same pose twice and
	// omitted the required zero-visible phase; that could false-pass stale
	// tails, so keep these phases and readbacks separate.
	constexpr int iTIC_PARK_END         = 60;                         // settle on the terrain campus
	constexpr int iTIC_MANY_VISIBLE_END = iTIC_PARK_END + 60;        // many-visible settle
	constexpr int iTIC_FEW_VISIBLE_END  = iTIC_MANY_VISIBLE_END + 60; // few-visible + readback
	constexpr int iTIC_ZERO_VISIBLE_END = iTIC_FEW_VISIBLE_END + 60;  // deterministic cull-all + readback
	constexpr int iTIC_RETURN_END       = iTIC_ZERO_VISIBLE_END + 60; // back to many-visible + readback
	constexpr int iTIC_CAP_SETTLE_END   = iTIC_RETURN_END + 60;       // re-settle for the A/B capture
	constexpr int iTIC_CAP_END          = iTIC_CAP_SETTLE_END + 1;    // one screenshot frame
	constexpr int iTIC_DRAIN_END        = iTIC_CAP_END + 12;          // Flux_Screenshot pending slot

	// ---- Pass criterion (FROZEN from baseline: see the plan) ----------
	// Debug mode 13 writes a terrain-only unlit magenta sentinel. At least 3%
	// of the inset viewport must match its deliberately narrow chromatic
	// signature. This is a fixed floor, not learned from any candidate arm.
	constexpr double fTIC_NONVACUITY_FRAC = 0.03;
	// The flat (1,0,1) G-buffer value reaches this swapchain at roughly
	// (151,31,141) after the normal display/tone-map path on the reference
	// Vulkan run. Keep the classifier chromatically narrow but allow normal
	// cross-driver quantisation and display-transform variation.
	constexpr uint8_t uTIC_SENTINEL_MIN_RB = 128u;
	constexpr uint8_t uTIC_SENTINEL_MAX_G  = 64u;
	constexpr uint8_t uTIC_SENTINEL_MAX_RB_DELTA = 32u;

	// The min visible-count below which NonVacuity holds even on a "few-visible"
	// pose — a sanity floor, not the A/B image gate. 1 visible chunk is the
	// minimum count before non-vacuity is credible.
	constexpr uint32_t uTIC_MIN_VISIBLE_FOR_NONVACUITY = 1u;

	// The inset sampled rect, trimming dock borders / viewport edges (the
	// same convention TAATemporalStability.cpp uses).
	constexpr double fTIC_RECT_INSET = 0.12;

	// Fixed camera pose: a campus-centred landscape view (the same seed-1337
	// terrain RenderTest always authors). Looks down at +X+Z hill country,
	// which frames terrain + tree line; sky box is intentionally minimised
	// so the non-vacuity gate targets terrain pixels and not 70% blue sky.
	constexpr float fTIC_CAM_OFFSET_X = 0.0f;
	constexpr float fTIC_CAM_OFFSET_Y = 2.5f;
	constexpr float fTIC_CAM_OFFSET_Z = 0.0f;
	constexpr float fTIC_CAM_YAW      = 2.3562f;
	constexpr float fTIC_CAM_PITCH    = -0.20f;   // mild down-tilt: terrain > sky

	// The "few visible" pose: pitch UP so the lower screen half is sky, with
	// only the far horizon in frame — this exercises the stale-tail path
	// because visibility shrinks from thousands of chunks to roughly the
	// horizon line. The readback after this pose asserts the count dropped
	// materially (the culling shader compacts live records into [0, N) and
	// the reset pass kept the [N, 4096) tail zeroed, so no chunk replays).
	constexpr float fTIC_CAM_FEW_PITCH = 0.45f;  // up: ~80% sky, thin horizon

	// Cull-all pose: from the same camera position, look almost straight up.
	// Every terrain AABB is below the camera/frustum, while streaming remains
	// parked on the campus (unlike teleporting far outside the world). After
	// 60 frames the count must be exactly zero and all 4,096 records zero.
	constexpr float fTIC_CAM_ZERO_PITCH = 1.50f;
	constexpr uint32_t uTIC_DEBUG_SENTINEL_MODE = 13u;

	// ---- File-static state (RAII'd by Setup/Teardown) ------------------
	bool        g_bTICFailed  = false;
	const char* g_szTICFailure = nullptr;
	bool        g_bTICSkipped = false;
	uint32_t    g_uTICVisibleCountAtFew = 0xFFFFFFFFu;
	uint32_t    g_uTICVisibleCountAtZero = 0xFFFFFFFFu;
	uint32_t    g_uTICVisibleCountAtReturn = 0xFFFFFFFFu;
	uint32_t    g_uTICCommandTailZeroed   = 0;  // count of tail records with all 5 words zero at return-to-many (should be TOTAL_CHUNKS - visible)
	uint32_t    g_uTICCommandTailStale     = 0;  // count of tail records with any non-zero word at return-to-many (should be 0)
	uint32_t    g_uTICCommandTailZeroedAtFew  = 0;  // same, at the few-visible pose
	uint32_t    g_uTICCommandTailStaleAtFew    = 0;  // same, at the few-visible pose
	uint32_t    g_uTICCommandTailZeroedAtZero = 0;
	uint32_t    g_uTICCommandTailStaleAtZero  = 0;
	int         g_iTICShotRequested = 0;
	uint32_t    g_uTICPreviousDebugMode = 0u;
	bool        g_bTICOverridesApplied = false;

	struct TICShotPath { char m_aszPath[260] = {}; };
	TICShotPath g_xTICShotPath;

	// The mode this process is running under (parsed once in Setup from the
	// immutable boot --indirect-count-mode). The screenshot artifact path
	// encodes the mode so the wrapper matches all three processes' outputs.
	const char* g_szTICModeName = "auto";

	void TICFail(const char* szReason)
	{
		if (g_szTICFailure == nullptr) g_szTICFailure = szReason;
		g_bTICFailed = true;
	}

	std::filesystem::path TICArtifactDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "rendertest" / "terrain_indirect";
	}

	const char* TICModeNameFromCLI()
	{
		switch (Zenith_CommandLine::GetIndirectCountMode())
		{
		case Zenith_IndirectCountMode::Native: return "native";
		case Zenith_IndirectCountMode::Padded: return "padded";
		case Zenith_IndirectCountMode::Single: return "single";
		case Zenith_IndirectCountMode::Auto:   return "auto";
		}
		return "auto";
	}

	Flux_IndirectDrawOverride TICOverrideFromCLI()
	{
		switch (Zenith_CommandLine::GetIndirectCountMode())
		{
		case Zenith_IndirectCountMode::Native: return Flux_IndirectDrawOverride::NATIVE;
		case Zenith_IndirectCountMode::Padded: return Flux_IndirectDrawOverride::PADDED;
		case Zenith_IndirectCountMode::Single: return Flux_IndirectDrawOverride::SINGLE;
		case Zenith_IndirectCountMode::Auto:   return Flux_IndirectDrawOverride::AUTO;
		}
		return Flux_IndirectDrawOverride::AUTO;
	}

	const char* TICExecutionModeName(Flux_IndirectExecutionMode eMode)
	{
		switch (eMode)
		{
		case Flux_IndirectExecutionMode::NATIVE_COUNT:  return "NATIVE_COUNT";
		case Flux_IndirectExecutionMode::PADDED_MULTI:  return "PADDED_MULTI";
		case Flux_IndirectExecutionMode::PADDED_SINGLE: return "PADDED_SINGLE";
		case Flux_IndirectExecutionMode::FAILED_CLOSED: return "FAILED_CLOSED";
		}
		return "FAILED_CLOSED";
	}

	// Sampled rect: editor viewport, inset, clamped into the image. Falls
	// back to a centred half-frame when the tools query seam is unavailable.
	void TICSampleRect(const Zenith_TestTGAImage& xImage,
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
				fLeft   = static_cast<double>(xPos.x) + fTIC_RECT_INSET * xSize.x;
				fTop    = static_cast<double>(xPos.y) + fTIC_RECT_INSET * xSize.y;
				fWidth  = (1.0 - 2.0 * fTIC_RECT_INSET) * xSize.x;
				fHeight = (1.0 - 2.0 * fTIC_RECT_INSET) * xSize.y;
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

	// Count only debug-mode-13's unlit magenta sentinel in the intended
	// viewport crop. RGB thresholding tolerates swapchain quantisation and the
	// final tone-map, but generic non-black scene pixels never qualify.
	double TICMeasureSentinelCoverage(const Zenith_TestTGAImage& xImage,
		uint64_t& ulSentinelPixelsOut, uint64_t& ulSamplePixelsOut)
	{
		ulSentinelPixelsOut = 0u;
		ulSamplePixelsOut = 0u;
		if (!xImage.IsValid()) return 0.0;
		uint32_t uX0 = 0u, uY0 = 0u, uX1 = 0u, uY1 = 0u;
		TICSampleRect(xImage, uX0, uY0, uX1, uY1);
		if (uX1 <= uX0 || uY1 <= uY0) return 0.0;

		for (uint32_t uY = uY0; uY < uY1; ++uY)
		{
			for (uint32_t uX = uX0; uX < uX1; ++uX)
			{
				const uint8_t* puRGBA = xImage.GetPixelBGRA(uX, uY);
				const uint8_t uB = puRGBA[0];
				const uint8_t uG = puRGBA[1];
				const uint8_t uR = puRGBA[2];
				const uint8_t uRBDelta = (uR > uB) ? (uR - uB) : (uB - uR);
				if (uR >= uTIC_SENTINEL_MIN_RB && uB >= uTIC_SENTINEL_MIN_RB &&
					uG <= uTIC_SENTINEL_MAX_G && uRBDelta <= uTIC_SENTINEL_MAX_RB_DELTA)
				{
					++ulSentinelPixelsOut;
				}
				++ulSamplePixelsOut;
			}
		}
		if (ulSamplePixelsOut == 0u) return 0.0;
		return static_cast<double>(ulSentinelPixelsOut) /
			static_cast<double>(ulSamplePixelsOut);
	}

	// Slow, test-only readback of the per-terrain indirect argument +
	// visible-count buffers (the path Vulkan/CLAUDE.md pins as
	// "explicit-call-only, never frame path"). Drains uploads + idles the
	// device. This is the design's stale-tail proof:
	//   * assert the live prefix contains structurally valid records;
	//   * assert every tail record [visibleCount, TOTAL_CHUNKS) has all
	//     five words zero — the zero-padded-to-max invariant.
	bool TICReadbackAndAssertTail(uint32_t& uVisibleCountOut, uint32_t& uTailZeroedOut, uint32_t& uTailStaleOut)
	{
		uVisibleCountOut = 0xFFFFFFFFu;
		uTailZeroedOut = 0u;
		uTailStaleOut  = 0u;

		// Find the first terrain component in the active scene (RenderTest
		// has exactly one terrain). Use ForEach with an early-out flag
		// (the Query First() API has no callback variant that captures the
		// component pointer; ForEach's lambda carries it).
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetActiveSceneData();
		if (pxScene == nullptr) return false;

		Zenith_TerrainComponent* pxTerrain = nullptr;
		pxScene->Query<Zenith_TerrainComponent>().ForEach(
			[&pxTerrain](Zenith_EntityID, Zenith_TerrainComponent& xT)
			{
				if (pxTerrain == nullptr) pxTerrain = &xT;
			});
		if (pxTerrain == nullptr) return false;
		if (!pxTerrain->HasPhysicsGeometry()) return false;  // proxy for "render resources live"

		// Read the visible-count buffer (single uint32_t).
		const Flux_IndirectBuffer& rxCount = pxTerrain->GetVisibleCountBuffer();
		uint32_t auCountBytes[1] = { 0u };
		g_xEngine.FluxMemory().DownloadBufferData(
			rxCount.GetBuffer().m_xVRAMHandle, auCountBytes, sizeof(auCountBytes));
		uVisibleCountOut = auCountBytes[0];
		if (uVisibleCountOut > Flux_TerrainConfig::TOTAL_CHUNKS)
			return false;

		// Read all 4096 records (81920 bytes) — the max-sized slow readback.
		const Flux_IndirectBuffer& rxArgs = pxTerrain->GetIndirectDrawBuffer();
		constexpr uint32_t uRECORDS = Flux_TerrainConfig::TOTAL_CHUNKS;
		Flux_IndirectDrawIndexedCommand axCommands[uRECORDS];
		std::memset(axCommands, 0xCD, sizeof(axCommands));  // poison fill
		g_xEngine.FluxMemory().DownloadBufferData(
			rxArgs.GetBuffer().m_xVRAMHandle, axCommands, sizeof(axCommands));

		// Live prefix: records [0, visibleCount) must be structurally
		// valid (instanceCount == 1 for terrain, indexCount > 0). Tail
		// records [visibleCount, TOTAL_CHUNKS) must be all-zero no-ops.
		const uint32_t uVisible = uVisibleCountOut;
		for (uint32_t u = 0u; u < uVisible; ++u)
		{
			const Flux_IndirectDrawIndexedCommand& rxCmd = axCommands[u];
			if (rxCmd.m_uInstanceCount != 1u) return false;  // live record contract
			if (rxCmd.m_uIndexCount    == 0u) return false;
		}
		for (uint32_t u = uVisible; u < uRECORDS; ++u)
		{
			const Flux_IndirectDrawIndexedCommand& rxCmd = axCommands[u];
			const bool bAllZero =
				rxCmd.m_uIndexCount == 0u && rxCmd.m_uInstanceCount == 0u &&
				rxCmd.m_uFirstIndex == 0u && rxCmd.m_iVertexOffset == 0 &&
				rxCmd.m_uFirstInstance == 0u;
			if (bAllZero) ++uTailZeroedOut;
			else          ++uTailStaleOut;
		}
		return true;
	}
}

// ---- File-static state init in Setup ---------------------------------
void Setup_TerrainIndirectCompatibility()
{
	g_bTICFailed = false;
	g_szTICFailure = nullptr;
	g_bTICSkipped = false;
	g_uTICVisibleCountAtFew = 0xFFFFFFFFu;
	g_uTICVisibleCountAtZero = 0xFFFFFFFFu;
	g_uTICVisibleCountAtReturn = 0xFFFFFFFFu;
	g_uTICCommandTailZeroed = 0u;
	g_uTICCommandTailStale  = 0u;
	g_uTICCommandTailZeroedAtFew = 0u;
	g_uTICCommandTailStaleAtFew  = 0u;
	g_uTICCommandTailZeroedAtZero = 0u;
	g_uTICCommandTailStaleAtZero  = 0u;
	g_iTICShotRequested = 0;
	g_bTICOverridesApplied = false;
	g_szTICModeName = TICModeNameFromCLI();

	// Skip on Null (GPU-less: swapchain dump is a no-op, DownloadBufferData
	// zero-fills — the A/B would compare two no-ops) AND on D3D12 (the
	// reserved no-op stub backend renders nothing — m_bRequiresGraphics
	// already excludes both, but if the flag is ever bypassed or a new
	// --all-automated-tests path reaches Setup on these backends, refuse
	// rather than report a meaningless green).
	if constexpr (Zenith_IsNullRenderer())
	{
		g_bTICSkipped = true;
		Zenith_AutomatedTestRunner::RequestSkip(
			"TerrainIndirectCompatibility needs a real swapchain + GPU to read back");
		return;
	}
#if defined(ZENITH_D3D12)
	{
		g_bTICSkipped = true;
		Zenith_AutomatedTestRunner::RequestSkip(
			"TerrainIndirectCompatibility needs a real GPU — D3D12 stub has no swapchain");
		return;
	}
#endif

	std::error_code xError;
	const std::filesystem::path xDir = TICArtifactDir();
	std::filesystem::create_directories(xDir, xError);
	if (!std::filesystem::exists(xDir, xError))
	{
		TICFail("could not create Build/artifacts/rendertest/terrain_indirect");
		return;
	}

	// Encode the mode in the artifact filename so the wrapper matches the
	// three processes' outputs after all arms finish.
	std::snprintf(g_xTICShotPath.m_aszPath, sizeof(g_xTICShotPath.m_aszPath),
		"%s/terrain_%s.tga", xDir.string().c_str(), g_szTICModeName);
	std::remove(g_xTICShotPath.m_aszPath);

	// Park the camera at the campus-centred landscape pose. Force TAA off so
	// the three process captures have no history/jitter variance, and select
	// terrain's test-only unlit sentinel for a terrain-specific pixel gate.
	RenderTest_GameplayState::s_bPhotoModeActive = true;
	RenderTest_GameplayState::s_fPhotoOffsetX = fTIC_CAM_OFFSET_X;
	RenderTest_GameplayState::s_fPhotoOffsetY = fTIC_CAM_OFFSET_Y;
	RenderTest_GameplayState::s_fPhotoOffsetZ = fTIC_CAM_OFFSET_Z;
	RenderTest_GameplayState::s_fPhotoYaw     = fTIC_CAM_YAW;
	RenderTest_GameplayState::s_fPhotoPitch   = fTIC_CAM_PITCH;
	g_xEngine.TAA().SetEnabled(false);
	g_uTICPreviousDebugMode = g_xEngine.Terrain().GetDebugMode();
	g_xEngine.Terrain().GetDebugMode() = uTIC_DEBUG_SENTINEL_MODE;
	g_bTICOverridesApplied = true;

	Zenith_Log(LOG_CATEGORY_RENDERER,
		"[TerrainIndirectCompatibility] mode=%s, artifacts=%s",
		g_szTICModeName, xDir.string().c_str());
}

bool Step_TerrainIndirectCompatibility(int iFrame)
{
	if (g_bTICSkipped || g_bTICFailed) return false;

	// Phase transition: switch the pitch to the few-visible pose.
	if (iFrame == iTIC_MANY_VISIBLE_END)
	{
		// Park at the few-visible (sky-dominant) pose for the stale-tail
		// stress. The GPU renders the next 60 frames at this pitch.
		RenderTest_GameplayState::s_fPhotoPitch = fTIC_CAM_FEW_PITCH;
	}

	// Readback #1: AT the few-visible pose. The GPU has been rendering at
	// the few-visible pitch for (iTIC_FEW_VISIBLE_END - iTIC_MANY_VISIBLE_END)
	// = 60 frames, so the indirect + count buffers reflect the FEW-visible
	// state. This readback drains + idles the device (the slow
	// DownloadBufferData path). After the readback, enter the deterministic
	// cull-all pose for the explicit zero-visible phase.
	if (iFrame == iTIC_FEW_VISIBLE_END)
	{
		if (!TICReadbackAndAssertTail(g_uTICVisibleCountAtFew,
			g_uTICCommandTailZeroedAtFew, g_uTICCommandTailStaleAtFew))
		{
			TICFail("few-visible readback returned false (no terrain / no culling resources)");
		}
		RenderTest_GameplayState::s_fPhotoPitch = fTIC_CAM_ZERO_PITCH;
	}

	// Explicit cull-all readback. Looking almost straight up keeps terrain
	// outside every frustum without moving the streaming origin. The reset +
	// cull passes have run for 60 frames, so count == 0 and all 4,096 command
	// records must be all-zero no-ops. Then restore the many-visible pose.
	if (iFrame == iTIC_ZERO_VISIBLE_END)
	{
		if (!TICReadbackAndAssertTail(g_uTICVisibleCountAtZero,
			g_uTICCommandTailZeroedAtZero, g_uTICCommandTailStaleAtZero))
		{
			TICFail("zero-visible readback returned false");
		}
		RenderTest_GameplayState::s_fPhotoPitch = fTIC_CAM_PITCH;
	}

	// Return-to-many readback after 60 frames back at the original pose.
	if (iFrame == iTIC_RETURN_END)
	{
		if (!TICReadbackAndAssertTail(g_uTICVisibleCountAtReturn,
			g_uTICCommandTailZeroed, g_uTICCommandTailStale))
		{
			TICFail("return-to-many readback returned false");
		}

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] readback: visibleFew=%u visibleZero=%u visibleReturn=%u "
			"tailZeroedReturn=%u tailStaleReturn=%u "
			"tailZeroedFew=%u tailStaleFew=%u "
			"tailZeroedZero=%u tailStaleZero=%u (TOTAL=%u)",
			g_uTICVisibleCountAtFew, g_uTICVisibleCountAtZero,
			g_uTICVisibleCountAtReturn,
			g_uTICCommandTailZeroed, g_uTICCommandTailStale,
			g_uTICCommandTailZeroedAtFew, g_uTICCommandTailStaleAtFew,
			g_uTICCommandTailZeroedAtZero, g_uTICCommandTailStaleAtZero,
			Flux_TerrainConfig::TOTAL_CHUNKS);
	}

	// A/B screenshot at the end (one frame, dump drains afterwards).
	if (iFrame == iTIC_CAP_END - 1)
	{
		Flux_Screenshot::RequestDump(g_xTICShotPath.m_aszPath);
		++g_iTICShotRequested;
	}

	return iFrame < iTIC_DRAIN_END;
}

bool Verify_TerrainIndirectCompatibility()
{
	if (g_bTICSkipped) return true;  // harness finalised as SKIPPED
	if (g_bTICFailed)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "[TerrainIndirectCompatibility] %s",
			g_szTICFailure ? g_szTICFailure : "setup failed");
		return false;
	}

	// ---- 1. Non-vacuity (readback count): terrain IS drawing -----
	// The PRIMARY vacuity gate is the visible-count readback: a non-zero
	// count at the many-visible pose proves terrain chunks passed frustum
	// culling, were compacted into the indirect buffer, and the recorder
	// issued draw calls. A fallback that skips terrain (the old behaviour)
	// leaves the count at zero — which fails HERE, not in the loose pixel
	// gate. The readback is terrain-specific by construction: the count
	// buffer is the per-terrain visible-chunk counter the culling shader
	// atomically increments, so the count is the count OF TERRAIN CHUNKS
	// that will be drawn.
	if (g_uTICVisibleCountAtFew == 0xFFFFFFFFu ||
		g_uTICVisibleCountAtZero == 0xFFFFFFFFu ||
		g_uTICVisibleCountAtReturn == 0xFFFFFFFFu)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] readback did not complete (few=0x%X zero=0x%X return=0x%X)",
			g_uTICVisibleCountAtFew, g_uTICVisibleCountAtZero,
			g_uTICVisibleCountAtReturn);
		return false;
	}
	if (g_uTICVisibleCountAtReturn < uTIC_MIN_VISIBLE_FOR_NONVACUITY)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] NON-VACUITY: visible at return=%u < min %u — the terrain is not drawing at the many-visible pose",
			g_uTICVisibleCountAtReturn, uTIC_MIN_VISIBLE_FOR_NONVACUITY);
		return false;
	}
	if (g_uTICVisibleCountAtZero != 0u)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] CULL-ALL: visible count=%u, expected exactly 0 after 60 frames at the upward pose",
			g_uTICVisibleCountAtZero);
		return false;
	}
	if (g_uTICVisibleCountAtFew >= g_uTICVisibleCountAtReturn)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] NON-VACUITY shrink: visible at few=%u must be STRICTLY < visible at return=%u — equality means no visibility shrink (no extended stale tail was exercised); the few-visible pose must reduce the compacted prefix",
			g_uTICVisibleCountAtFew, g_uTICVisibleCountAtReturn);
		return false;
	}

	// ---- 2. Stale-tail invariant: every tail record must be zeroed --------
	// The zero-padded-to-max contract: after the reset pass clears all
	// records + culling compacts live into [0, N), the tail [N, TOTAL_CHUNKS)
	// MUST have all five words zero, no exceptions. Checked at the
	// few-visible pose (the stale-tail regression's direct trigger: many →
	// few leaves the compaction prefix shorter and the tail longer), the
	// zero-visible pose (all 4,096 records are tail), and the
	// return-to-many pose (the replay risk: many → few → return must not
	// ghost-replay the prior frame's compacted prefix through the tail).
	if (g_uTICCommandTailStaleAtFew != 0u)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] STALE TAIL (few-visible): %u tail records have non-zero words — the many→few transition replayed stale chunks",
			g_uTICCommandTailStaleAtFew);
		return false;
	}
	if (g_uTICCommandTailStale != 0u)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] STALE TAIL (return): %u tail records have non-zero words — many→few→return replayed stale chunks (the zero-padded-to-max invariant is broken)",
			g_uTICCommandTailStale);
		return false;
	}
	if (g_uTICCommandTailStaleAtZero != 0u ||
		g_uTICCommandTailZeroedAtZero != Flux_TerrainConfig::TOTAL_CHUNKS)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] STALE TAIL (cull-all): zeroed=%u stale=%u, expected all %u command records to have all five words zero",
			g_uTICCommandTailZeroedAtZero, g_uTICCommandTailStaleAtZero,
			Flux_TerrainConfig::TOTAL_CHUNKS);
		return false;
	}
	const uint32_t uExpectedZeroedReturn = Flux_TerrainConfig::TOTAL_CHUNKS - g_uTICVisibleCountAtReturn;
	if (g_uTICCommandTailZeroed != uExpectedZeroedReturn)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] STALE TAIL (return): zeroed tail=%u != expected %u (TOTAL_CHUNKS %u - visible %u)",
			g_uTICCommandTailZeroed, uExpectedZeroedReturn,
			Flux_TerrainConfig::TOTAL_CHUNKS, g_uTICVisibleCountAtReturn);
		return false;
	}
	const uint32_t uExpectedZeroedFew = Flux_TerrainConfig::TOTAL_CHUNKS - g_uTICVisibleCountAtFew;
	if (g_uTICCommandTailZeroedAtFew != uExpectedZeroedFew)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] STALE TAIL (few): zeroed tail=%u != expected %u (TOTAL_CHUNKS %u - visible %u)",
			g_uTICCommandTailZeroedAtFew, uExpectedZeroedFew,
			Flux_TerrainConfig::TOTAL_CHUNKS, g_uTICVisibleCountAtFew);
		return false;
	}

	// ---- 3. Terrain-only sentinel reaches the swapchain ---------------------
	// A non-zero culling count alone does not prove any indirect draw reached
	// rasterisation. Debug mode 13 writes unlit magenta from terrain shaders
	// only; classify that sentinel inside the intended editor viewport crop.
	Zenith_TestTGAImage xShot;
	if (!Zenith_TestLoadTGA(g_xTICShotPath.m_aszPath, xShot))
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] missing/invalid screenshot artifact: %s",
			g_xTICShotPath.m_aszPath);
		return false;
	}
	uint64_t ulSentinelPixels = 0u;
	uint64_t ulSamplePixels = 0u;
	const double fCoverage = TICMeasureSentinelCoverage(
		xShot, ulSentinelPixels, ulSamplePixels);
	uint32_t uCropX0 = 0u, uCropY0 = 0u, uCropX1 = 0u, uCropY1 = 0u;
	TICSampleRect(xShot, uCropX0, uCropY0, uCropX1, uCropY1);
	Zenith_Log(LOG_CATEGORY_RENDERER,
		"[TerrainIndirectCompatibility] CROP mode=%s x0=%u y0=%u x1=%u y1=%u width=%u height=%u",
		g_szTICModeName, uCropX0, uCropY0, uCropX1, uCropY1,
		xShot.m_uWidth, xShot.m_uHeight);
	Zenith_Log(LOG_CATEGORY_RENDERER,
		"[TerrainIndirectCompatibility] SENTINEL mode=%s coverage=%.6f pixels=%llu samples=%llu",
		g_szTICModeName, fCoverage,
		static_cast<unsigned long long>(ulSentinelPixels),
		static_cast<unsigned long long>(ulSamplePixels));
	if (fCoverage < fTIC_NONVACUITY_FRAC)
	{
		Zenith_Error(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] NON-VACUITY sentinel coverage=%.6f < fixed floor %.6f — terrain debug mode 13 did not reach the swapchain",
			fCoverage, fTIC_NONVACUITY_FRAC);
		return false;
	}

	// ---- 4. Recorder telemetry — prove WHICH execution tier ran ----------
	// The ZENITH_TESTING per-device counters prove that the CLI override
	// actually selected the expected tier — not just that the flag was
	// set. Without this check both A/B arms could silently execute native
	// count and pass; the telemetry catches a ignored override.
#ifdef ZENITH_VULKAN
	{
		auto& xVulkan = static_cast<Zenith_Vulkan&>(g_xEngine.FluxBackend());
		auto& xTelem  = xVulkan.GetIndirectDrawTelemetry();
		const uint32_t uNative      = xTelem.m_uNativeCount.load(std::memory_order_relaxed);
		const uint32_t uPaddedMulti  = xTelem.m_uPaddedMulti.load(std::memory_order_relaxed);
		const uint32_t uPaddedSingle = xTelem.m_uPaddedSingle.load(std::memory_order_relaxed);
		const uint32_t uFailClosed   = xTelem.m_uFailClosed.load(std::memory_order_relaxed);
		const uint32_t uFixedIndirect = xTelem.m_uFixedIndirect.load(std::memory_order_relaxed);
		const Flux_IndirectDrawOverride eRequestedOverride = TICOverrideFromCLI();
		const Flux_IndirectDrawOverride eBackendOverride = xVulkan.GetIndirectDrawOverride();
		if (eBackendOverride != eRequestedOverride)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TerrainIndirectCompatibility] TELEMETRY: backend override %u != parsed CLI request %u",
				static_cast<uint32_t>(eBackendOverride),
				static_cast<uint32_t>(eRequestedOverride));
			return false;
		}

		const Flux_IndirectDrawCapabilities& xActualCaps = xVulkan.GetIndirectDrawCapabilities();
		const Flux_IndirectExecutionMode eActualExpected =
			Flux_SelectIndirectExecutionMode(xActualCaps, Flux_TerrainConfig::TOTAL_CHUNKS,
				Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, eRequestedOverride);
		const char* szExpected = TICExecutionModeName(eActualExpected);

		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectCompatibility] TELEMETRY mode=%s expected=%s native=%u paddedMulti=%u paddedSingle=%u failClosed=%u fixedIndirect=%u",
			g_szTICModeName, szExpected, uNative, uPaddedMulti, uPaddedSingle,
			uFailClosed, uFixedIndirect);

		// Require exactly the tier selected from the actual usable capability
		// record and this process's immutable request. Non-zero expected counter,
		// both other tier counters zero, and no fail-closed calls.
		bool bExactTier = false;
		switch (eActualExpected)
		{
		case Flux_IndirectExecutionMode::NATIVE_COUNT:
			bExactTier = uNative > 0u && uPaddedMulti == 0u &&
				uPaddedSingle == 0u && uFailClosed == 0u;
			break;
		case Flux_IndirectExecutionMode::PADDED_MULTI:
			bExactTier = uNative == 0u && uPaddedMulti > 0u &&
				uPaddedSingle == 0u && uFailClosed == 0u;
			break;
		case Flux_IndirectExecutionMode::PADDED_SINGLE:
			bExactTier = uNative == 0u && uPaddedMulti == 0u &&
				uPaddedSingle > 0u && uFailClosed == 0u;
			break;
		case Flux_IndirectExecutionMode::FAILED_CLOSED:
			bExactTier = false;
			break;
		}
		if (!bExactTier)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TerrainIndirectCompatibility] TELEMETRY exact-tier mismatch: expected=%s native=%u paddedMulti=%u paddedSingle=%u failClosed=%u",
				szExpected, uNative, uPaddedMulti, uPaddedSingle, uFailClosed);
			return false;
		}

		// On a capable device auto should use native count (native > 0,
		// padded = 0). Padded forces PADDED_MULTI (native = 0, paddedMulti >
		// 0 or paddedSingle > 0). Single forces PADDED_SINGLE (native = 0,
		// paddedSingle > 0, paddedMulti = 0). failClosed must ALWAYS be 0 —
		// terrain's ZERO_PADDED_TO_MAX policy never lands on it.
		if (uFailClosed != 0u)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TerrainIndirectCompatibility] TELEMETRY: failClosed=%u — the recorder hit FAILED_CLOSED (the CLI override + capabilities yielded no legal execution path)",
				uFailClosed);
			return false;
		}
		const bool bPaddedMode = (g_szTICModeName[0] == 'p' || g_szTICModeName[0] == 's');
		if (bPaddedMode)
		{
			// Padded or single: native count MUST be 0 — the override
			// forbids it. Padded-single requires paddedSingle > 0 and
			// paddedMulti == 0.
			if (uNative != 0u)
			{
				Zenith_Error(LOG_CATEGORY_RENDERER,
					"[TerrainIndirectCompatibility] TELEMETRY: mode=%s emitted %u native-count calls — a padded/single override must NOT call the native count command",
					g_szTICModeName, uNative);
				return false;
			}
			if (g_szTICModeName[0] == 's' && uPaddedMulti != 0u)
			{
				Zenith_Error(LOG_CATEGORY_RENDERER,
					"[TerrainIndirectCompatibility] TELEMETRY: single mode emitted %u padded-multi batches — SINGLE must force padded-single only",
					uPaddedMulti);
				return false;
			}
			if (uPaddedMulti == 0u && uPaddedSingle == 0u)
			{
				Zenith_Error(LOG_CATEGORY_RENDERER,
					"[TerrainIndirectCompatibility] TELEMETRY: mode=%s emitted no padded calls — the override was ignored or terrain is not drawing",
					g_szTICModeName);
				return false;
			}
		}
		else
		{
			// Auto mode: resolve the EXACT tier the selector SHOULD pick from
			// the device's usable capabilities + the request (TOTAL_CHUNKS, ZERO_PADDED) +
			// the AUTO override, then require EXACTLY that tier — not just "something ran".
			// On a count-capable device auto MUST select NATIVE_COUNT (native > 0, padded == 0).
			// A count-capable device that silently falls to padded would make the A/B test
			// compare fallback against itself (auto = padded, padded-arm = padded → identical).
			// On a no-count device auto selects PADDED_MULTI or PADDED_SINGLE — native MUST be 0.
			const Flux_IndirectDrawCapabilities& xCaps = xVulkan.GetIndirectDrawCapabilities();
			const Flux_IndirectExecutionMode eExpected =
				Flux_SelectIndirectExecutionMode(xCaps, Flux_TerrainConfig::TOTAL_CHUNKS,
					Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX, Flux_IndirectDrawOverride::AUTO);

			if (eExpected == Flux_IndirectExecutionMode::NATIVE_COUNT)
			{
				// Auto on capable hardware: native MUST be > 0, padded MUST be 0.
				if (uNative == 0u)
				{
					Zenith_Error(LOG_CATEGORY_RENDERER,
						"[TerrainIndirectCompatibility] TELEMETRY: auto mode on a count-capable device (nativeCount usable) emitted ZERO native calls — the selector chose NATIVE_COUNT but the recorder did not execute it. The A/B test would compare fallback against itself.");
					return false;
				}
				if (uPaddedMulti > 0u || uPaddedSingle > 0u)
				{
					Zenith_Error(LOG_CATEGORY_RENDERER,
						"[TerrainIndirectCompatibility] TELEMETRY: auto mode mixed tiers on capable hardware — native=%u + paddedMulti=%u + paddedSingle=%u. On a count-capable device auto must use NATIVE_COUNT only.",
						uNative, uPaddedMulti, uPaddedSingle);
					return false;
				}
			}
			else if (eExpected == Flux_IndirectExecutionMode::PADDED_MULTI)
			{
				// Auto on a no-count device with multi-draw: native MUST be 0, paddedMulti MUST be > 0.
				if (uNative != 0u)
				{
					Zenith_Error(LOG_CATEGORY_RENDERER,
						"[TerrainIndirectCompatibility] TELEMETRY: auto mode on a no-count device emitted %u native calls — native count should be 0 when the device lacks it", uNative);
					return false;
				}
				if (uPaddedMulti == 0u)
				{
					Zenith_Error(LOG_CATEGORY_RENDERER,
						"[TerrainIndirectCompatibility] TELEMETRY: auto mode expected PADDED_MULTI but emitted 0 padded-multi calls. The override may have been ignored or terrain is not drawing.");
					return false;
				}
			}
			else if (eExpected == Flux_IndirectExecutionMode::PADDED_SINGLE)
			{
				// Auto on a no-count no-multi-draw device: native = 0, paddedSingle > 0.
				if (uNative != 0u)
				{
					Zenith_Error(LOG_CATEGORY_RENDERER,
						"[TerrainIndirectCompatibility] TELEMETRY: auto mode on a no-count no-multi-draw device emitted %u native calls", uNative);
					return false;
				}
				if (uPaddedSingle == 0u)
				{
					Zenith_Error(LOG_CATEGORY_RENDERER,
						"[TerrainIndirectCompatibility] TELEMETRY: auto mode expected PADDED_SINGLE but emitted 0 padded-single calls.");
					return false;
				}
			}
			// FAILED_CLOSED should never happen with ZERO_PADDED_TO_MAX — the selector
			// always has a legal padded tier. failClosed was already checked above.
	}
	}
#endif

	// ---- 5. A/B equivalence budget (wrapper) ----------------------------
	// The wrapper compares the RGB channels in the logged viewport crop for
	// auto-vs-padded and auto-vs-single against checked-in fixed budgets.

	Zenith_Log(LOG_CATEGORY_RENDERER,
		"[TerrainIndirectCompatibility] PASS mode=%s visibleFew=%u visibleZero=%u visibleReturn=%u sentinelCoverage=%.6f",
		g_szTICModeName, g_uTICVisibleCountAtFew, g_uTICVisibleCountAtZero,
		g_uTICVisibleCountAtReturn, fCoverage);
	return true;
}

void Teardown_TerrainIndirectCompatibility()
{
	RenderTest_GameplayState::s_bPhotoModeActive = false;
	if (g_bTICOverridesApplied)
	{
		g_xEngine.Terrain().GetDebugMode() = g_uTICPreviousDebugMode;
		g_xEngine.TAA().ClearEnabledOverride();
		g_bTICOverridesApplied = false;
	}
}

const Zenith_AutomatedTest g_xTerrainIndirectCompatibility = {
	"TerrainIndirectCompatibility",
	&Setup_TerrainIndirectCompatibility,
	&Step_TerrainIndirectCompatibility,
	&Verify_TerrainIndirectCompatibility,
	iTIC_DRAIN_END + 60,
	true  /* m_bRequiresGraphics — Null skips this test */,
	false /* m_bManualOnly — included in --all-automated-tests */,
	&Teardown_TerrainIndirectCompatibility
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainIndirectCompatibility);

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
