#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Panels/Zenith_EditorPanel_AnimStateMachine.h"
#include "Flux/Flux_Screenshot.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"

#include <filesystem>
#include <string>

// ============================================================================
// ZM_AnimEditorShowcase -- a manual-only VISUAL demo of the animation editor
// (r3, Phase D/B/E) driving REAL Zenithmon creature data: the Animation
// panel's dope sheet + curve view on a baked creature's clips (promoted to an
// authored override, since the generated clips themselves are read-only --
// D21), then the state-machine panel authoring a small Idle/Walk/Attack graph
// from the same creature's clips.
//
// Every panel call goes through the SAME public entry points the editor's own
// buttons and units use (PromoteAndOpenAuthoredOverride, ShowFlag,
// RequestWindowPlacement) or through Zenith_EditorAutomation for the steps
// that have one -- no synthesized mouse/keyboard input, and no scene/camera
// setup: both panels render their preview into their OWN persistent render
// view (kuFluxViewSlotPreviewAnim), independent of whatever scene is loaded
// behind them, so a full-swapchain screenshot captures the editor exactly as
// a person would see it.
//
// m_bManualOnly-style: no per-commit signal, no baseline pin. Run by name:
//   zenithmon.exe --automated-test ZM_AnimEditorShowcase --skip-unit-tests
// GATING: m_bRequiresGraphics = true -- the headless CI batch skips it. Only a
// windowed Vulkan_*_True run bakes (if needed) + renders + captures.
// ============================================================================

namespace
{
	constexpr ZM_SPECIES_ID eZM_SHOWCASE_SPECIES = ZM_SPECIES_FERNFAWN;   // always-buildable reference species

	enum ZMAnimShowcasePhase
	{
		ZM_ANIMSHOWCASE_PHASE_SETTLE_IDLE,
		ZM_ANIMSHOWCASE_PHASE_SHOOT_IDLE,
		ZM_ANIMSHOWCASE_PHASE_ADVANCE_TO_WALK,
		ZM_ANIMSHOWCASE_PHASE_SETTLE_WALK_CURVE,
		ZM_ANIMSHOWCASE_PHASE_SHOOT_WALK_CURVE,
		ZM_ANIMSHOWCASE_PHASE_ADVANCE_TO_SM,
		ZM_ANIMSHOWCASE_PHASE_SETTLE_SM,
		ZM_ANIMSHOWCASE_PHASE_SHOOT_SM,
		ZM_ANIMSHOWCASE_PHASE_CLOSE_SM,
		ZM_ANIMSHOWCASE_PHASE_DONE,
	};

	bool        g_bShowcaseActive  = false;
	bool        g_bShowcaseFailed  = false;
	const char* g_szShowcaseFailure = "test did not reach verification";
	ZMAnimShowcasePhase g_ePhase   = ZM_ANIMSHOWCASE_PHASE_SETTLE_IDLE;
	int         g_iSettleCountdown = 0;
	u_int       g_uShotsRequested = 0u;
	constexpr u_int uZM_SHOWCASE_SHOT_COUNT = 3u;

	std::string g_strIdleRef;
	std::string g_strWalkRef;
	std::string g_strAttackRef;
	std::string g_strScratchControllerRef;

	std::string g_astrShotPath[uZM_SHOWCASE_SHOT_COUNT];
	bool        g_abShotWritten[uZM_SHOWCASE_SHOT_COUNT] = { false, false, false };

	void FailShowcase(const char* szReason)
	{
		g_szShowcaseFailure = szReason;
		g_bShowcaseFailed = true;
	}

	bool ResolveCreatureRef(ZM_CREATURE_ASSET_KIND eKind, std::string& strOut)
	{
		char szRef[256];
		if (!ZM_CreatureAssetPath(eZM_SHOWCASE_SPECIES, eKind, szRef, static_cast<u_int>(sizeof(szRef))))
		{
			return false;
		}
		strOut = szRef;
		return true;
	}

	bool DiskFilePresent(const std::string& strRef)
	{
		// Every ref is "game:<relative>"; the disk path is GAME_ASSETS_DIR + tail.
		const char* szPrefix = "game:";
		const size_t uPrefixLen = std::strlen(szPrefix);
		if (strRef.compare(0, uPrefixLen, szPrefix) != 0)
		{
			return false;
		}
		const std::string strDisk = std::string(GAME_ASSETS_DIR) + strRef.substr(uPrefixLen);
		std::error_code xError;
		return std::filesystem::is_regular_file(strDisk, xError) && !xError
			&& std::filesystem::file_size(strDisk, xError) != 0u && !xError;
	}

	// Absolute Build/artifacts/zenithmon/animeditor dir, derived from
	// GAME_ASSETS_DIR (<repo>/Games/Zenithmon/Assets/ -> up three -> <repo>), so
	// it resolves regardless of the process working directory.
	std::filesystem::path ShowcaseVisualDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "animeditor";
	}

	void RequestShot(u_int uIndex)
	{
		if (uIndex >= uZM_SHOWCASE_SHOT_COUNT || g_astrShotPath[uIndex].empty())
		{
			return;
		}
		Flux_Screenshot::RequestDump(g_astrShotPath[uIndex].c_str());
		g_uShotsRequested++;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_AnimEditorShowcase] requested capture %u -> %s", uIndex, g_astrShotPath[uIndex].c_str());
	}
}

static void Setup_ZMAnimEditorShowcase()
{
	g_bShowcaseActive = false;
	g_bShowcaseFailed = false;
	g_szShowcaseFailure = "test did not reach verification";
	g_ePhase = ZM_ANIMSHOWCASE_PHASE_SETTLE_IDLE;
	g_iSettleCountdown = 0;
	g_uShotsRequested = 0u;
	g_abShotWritten[0] = g_abShotWritten[1] = g_abShotWritten[2] = false;

	ZM_BakeCreature(eZM_SHOWCASE_SPECIES);   // tools-only; no-op if the bundle is already current

	if (!ResolveCreatureRef(ZM_CREATURE_ASSET_ANIM_IDLE, g_strIdleRef)
		|| !ResolveCreatureRef(ZM_CREATURE_ASSET_ANIM_WALK, g_strWalkRef)
		|| !ResolveCreatureRef(ZM_CREATURE_ASSET_ANIM_ATTACK, g_strAttackRef))
	{
		Zenith_AutomatedTestRunner::RequestSkip("could not resolve the showcase creature's clip refs");
		return;
	}
	if (!DiskFilePresent(g_strIdleRef) || !DiskFilePresent(g_strWalkRef) || !DiskFilePresent(g_strAttackRef))
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"baked creature clips absent (run a *_True build to bake Assets/Creatures)");
		return;
	}

	// A scratch target for the fresh controller doc -- nothing is ever Saved, so
	// this path is never written to disk. Kept under the species' own folder
	// only so a stray future Save lands somewhere obviously temporary.
	g_strScratchControllerRef = "game:Creatures/" + std::string(ZM_GetSpeciesName(eZM_SHOWCASE_SPECIES))
		+ "/_AnimEditorShowcase_Scratch.zanimctrl";

	const std::filesystem::path xVisualDir = ShowcaseVisualDir();
	std::error_code xDirError;
	std::filesystem::create_directories(xVisualDir, xDirError);
	g_astrShotPath[0] = (xVisualDir / "01_dopesheet_idle.tga").string();
	g_astrShotPath[1] = (xVisualDir / "02_curveview_walk.tga").string();
	g_astrShotPath[2] = (xVisualDir / "03_statemachine_graph.tga").string();

	Zenith_EditorPanel_Animation& xAnimPanel = Zenith_EditorPanel_Animation::Instance();
	xAnimPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 640.0f);
	Zenith_EditorPanel_AnimStateMachine::Instance().RequestWindowPlacement(20.0f, 15.0f, 1180.0f, 720.0f);

	// The Idle clip is GENERATED (baked every tools boot), so opening it for
	// editing directly is refused (D21) -- promote it to an authored override
	// first, exactly the button the panel itself offers on that refusal.
	xAnimPanel.ShowFlag() = true;
	if (!xAnimPanel.PromoteAndOpenAuthoredOverride(g_strIdleRef))
	{
		FailShowcase("could not promote/open the Idle clip as an authored override");
		return;
	}

	g_bShowcaseActive = true;
	g_iSettleCountdown = 30;
}

static bool Step_ZMAnimEditorShowcase(int /*iFrame*/)
{
	if (!g_bShowcaseActive || g_bShowcaseFailed)
	{
		return false;
	}

	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	// A settle countdown lets the panel render several steady frames (the
	// skinned pose evaluates, the preview target stops being the clear colour)
	// before a capture is requested.
	if (g_iSettleCountdown > 0)
	{
		g_iSettleCountdown--;
		return true;
	}
	if (xAuto.IsRunning() && !xAuto.IsComplete())
	{
		return true;   // a queued automation batch is still draining, one step per frame
	}

	switch (g_ePhase)
	{
	// ★ EVERY "SHOOT" PHASE DOES NOTHING BUT REQUEST THE DUMP. Flux_Screenshot
	// captures whatever the CURRENT frame renders, and a Step_ mutation made in
	// the SAME call still lands before that frame's render pass records -- so a
	// promote/open done here would retitle the capture that follows it, not the
	// one just requested. Every state change therefore waits for the NEXT
	// phase, one frame after its shot was requested.
	case ZM_ANIMSHOWCASE_PHASE_SETTLE_IDLE:
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_SHOOT_IDLE;
		break;

	case ZM_ANIMSHOWCASE_PHASE_SHOOT_IDLE:
		RequestShot(0);
		g_abShotWritten[0] = true;
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_ADVANCE_TO_WALK;
		break;

	case ZM_ANIMSHOWCASE_PHASE_ADVANCE_TO_WALK:
	{
		Zenith_EditorPanel_Animation& xAnimPanel = Zenith_EditorPanel_Animation::Instance();
		if (!xAnimPanel.PromoteAndOpenAuthoredOverride(g_strWalkRef))
		{
			FailShowcase("could not promote/open the Walk clip as an authored override");
			return false;
		}
		xAuto.Reset();
		xAuto.AddStep_AnimCurveSetView(true);
		xAuto.AddStep_AnimScrub(0.5f);
		xAuto.Begin();
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_SETTLE_WALK_CURVE;
		break;
	}

	case ZM_ANIMSHOWCASE_PHASE_SETTLE_WALK_CURVE:
		g_iSettleCountdown = 30;
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_SHOOT_WALK_CURVE;
		break;

	case ZM_ANIMSHOWCASE_PHASE_SHOOT_WALK_CURVE:
		RequestShot(1);
		g_abShotWritten[1] = true;
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_ADVANCE_TO_SM;
		break;

	case ZM_ANIMSHOWCASE_PHASE_ADVANCE_TO_SM:
	{
		Zenith_EditorPanel_Animation::Instance().ShowFlag() = false;
		Zenith_EditorPanel_AnimStateMachine::Instance().ShowFlag() = true;

		// One queue: the dope-sheet teardown drains before the first AnimSm step
		// (a single FIFO, one step per frame, in push order).
		xAuto.Reset();
		xAuto.AddStep_AnimCurveSetView(false);
		xAuto.AddStep_AnimCloseClip();
		xAuto.AddStep_AnimSmOpenFresh(g_strScratchControllerRef.c_str());
		xAuto.AddStep_AnimSmAddClipPath(g_strIdleRef.c_str());
		xAuto.AddStep_AnimSmAddClipPath(g_strWalkRef.c_str());
		xAuto.AddStep_AnimSmAddClipPath(g_strAttackRef.c_str());
		xAuto.AddStep_AnimSmAddState("Idle");
		xAuto.AddStep_AnimSmAddState("Walk");
		xAuto.AddStep_AnimSmAddState("Attack");
		xAuto.AddStep_AnimSmSetStateClip("Idle", "Idle");
		xAuto.AddStep_AnimSmSetStateClip("Walk", "Walk");
		xAuto.AddStep_AnimSmSetStateClip("Attack", "Attack");
		xAuto.AddStep_AnimSmSetDefaultState("Idle");
		xAuto.AddStep_AnimSmAddTransition("Idle", "Walk");
		xAuto.AddStep_AnimSmAddTransition("Walk", "Attack");
		xAuto.AddStep_AnimSmAddTransition("Attack", "Idle");
		xAuto.Begin();
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_SETTLE_SM;
		break;
	}

	case ZM_ANIMSHOWCASE_PHASE_SETTLE_SM:
		g_iSettleCountdown = 30;
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_SHOOT_SM;
		break;

	case ZM_ANIMSHOWCASE_PHASE_SHOOT_SM:
		RequestShot(2);
		g_abShotWritten[2] = true;
		xAuto.Reset();
		xAuto.AddStep_AnimSmClose();
		xAuto.Begin();
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_CLOSE_SM;
		break;

	case ZM_ANIMSHOWCASE_PHASE_CLOSE_SM:
		Zenith_EditorPanel_AnimStateMachine::Instance().ShowFlag() = false;
		g_iSettleCountdown = 10;   // let the requested capture actually drain before the process ends
		g_ePhase = ZM_ANIMSHOWCASE_PHASE_DONE;
		break;

	case ZM_ANIMSHOWCASE_PHASE_DONE:
		return false;
	}

	return true;
}

static bool Verify_ZMAnimEditorShowcase()
{
	bool bPassed = true;
	if (g_bShowcaseFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_AnimEditorShowcase] %s", g_szShowcaseFailure);
		bPassed = false;
	}
	if (g_bShowcaseActive)
	{
		if (g_uShotsRequested != uZM_SHOWCASE_SHOT_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_AnimEditorShowcase] expected %u captures requested, saw %u",
				uZM_SHOWCASE_SHOT_COUNT, g_uShotsRequested);
			bPassed = false;
		}
		for (u_int u = 0; u < uZM_SHOWCASE_SHOT_COUNT; ++u)
		{
			if (!g_abShotWritten[u])
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_AnimEditorShowcase] capture %u was never requested", u);
				bPassed = false;
			}
		}
	}
	g_bShowcaseActive = false;
	return bPassed;
}

static const Zenith_AutomatedTest g_xZMAnimEditorShowcaseTest = {
	"ZM_AnimEditorShowcase",
	&Setup_ZMAnimEditorShowcase,
	&Step_ZMAnimEditorShowcase,
	&Verify_ZMAnimEditorShowcase,
	/* maxFrames */ 600,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMAnimEditorShowcaseTest);

#endif // ZENITH_TOOLS
