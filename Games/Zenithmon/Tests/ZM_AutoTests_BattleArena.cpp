#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleArena.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_BattleArena -- the S5 windowed BATTLE-ARENA scene gate. It
// additively loads the authored Battle scene (build index 1) over the running
// game, lets the persistent ZM_BattleArena's OnStart build its always-visible
// dome + 2 platforms + 6 per-biome dressing sets at world Y = -2000, then drives
// one biome switch (SetBiome VOLCANIC) and verifies the built/active-biome
// contract plus the arena root's world placement.
//
// It exercises the component's WINDOWED surface (OnStart build + SetBiome +
// GetActiveBiome + IsBuilt) that the pure T0 ZM_Tests_BattleArena.cpp cannot
// reach; the two are complementary. Because entity-level per-dressing enabled
// state is NOT exposed by the frozen public API, the "exactly one dressing
// shown" invariant is proven headlessly by the T0 VisibilityExactlyOne mask
// case; here we assert the built + active-biome + arena-root-Y placement
// fallback the S5 test plan prescribes.
//
// GATING (C5/C6): m_bRequiresGraphics = true, so the headless CI batch skips it
// (no GPU). Setup RequestSkip()s when the baked PROP family is absent/stale (the
// dressing sets) OR the authored Battle.zscen is missing -- both git-ignored, so
// a fresh CI checkout skips rather than fails and the unit baseline is unchanged.
// Only a windowed *_True run bakes + loads + drives the arena.
// ============================================================================

namespace
{
	constexpr float fZM_BA_FIXED_DT     = 1.0f / 60.0f;
	constexpr float fZM_BA_ROOT_Y_EPS   = 0.5f;   // arena-root world-Y tolerance
	constexpr int   iZM_BA_SETTLE_FRAME  = 30;    // let OnStart + GPU upload settle
	constexpr int   iZM_BA_FIND_DEADLINE = 120;   // arena instance must appear by here
	constexpr int   iZM_BA_BUILT_DEADLINE = 200;  // arena must report IsBuilt() by here

	// ---- Captured test state (all reset in Setup; batch mode reuses the process) ----
	bool            g_bBAActive        = false;   // scene built (Setup got past the skips)
	bool            g_bBAFailed        = false;
	const char*     g_szBAFailure      = "test did not reach verification";
	bool            g_bBADone          = false;   // Step reached its terminal capture

	bool            g_bBAFound         = false;   // a unique arena instance was found
	u_int           g_uBACount         = 0u;      // how many arena instances were seen
	bool            g_bBABuilt         = false;   // IsBuilt() at capture time
	bool            g_bBAFullyBuilt    = false;   // IsFullyBuilt() (all 9 child entities spawned)
	bool            g_bBASetBiomeReturn = false;  // SetBiome(VOLCANIC) return value
	ZM_BATTLE_BIOME g_eBABiome         = ZM_BATTLE_BIOME_COUNT;  // GetActiveBiome() after SetBiome
	bool            g_bBARootResolved  = false;   // arena root entity + transform resolved
	float           g_fBARootY         = 0.0f;    // arena root entity world Y

	Zenith_EntityID g_xBAEntityID      = INVALID_ENTITY_ID;
	Zenith_Scene    g_xBAPreviousScene;
	Zenith_Scene    g_xBAScene;

	void FailBA(const char* szReason)
	{
		g_szBAFailure = szReason;
		g_bBAFailed = true;
	}

	// is_regular_file + size>0 exists-guard (mirrors WorldTraversal's helper).
	bool DiskFilePresent(const std::string& strPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(strPath, xError) || xError)
		{
			return false;
		}
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
		return !xError && ulSize != 0u;
	}

	struct ArenaView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_BattleArena* m_pxArena   = nullptr;
		u_int           m_uCount    = 0u;
	};

	// Find the unique ZM_BattleArena across every loaded scene (the additive
	// Battle scene owns it), mirroring WorldTraversal's FindUniqueManager.
	bool FindUniqueArena(ArenaView& xOut)
	{
		xOut = ArenaView{};
		g_xEngine.Scenes().QueryAllScenes<ZM_BattleArena>().ForEach(
			[&xOut](Zenith_EntityID xEntityID, ZM_BattleArena& xArena)
			{
				++xOut.m_uCount;
				if (xOut.m_uCount == 1u)
				{
					xOut.m_xEntityID = xEntityID;
					xOut.m_pxArena = &xArena;
				}
			});
		return xOut.m_uCount == 1u && xOut.m_pxArena != nullptr;
	}

	void CleanupBAScene()
	{
		Zenith_InputSimulator::ClearFixedDt();
		if (g_xBAPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xBAPreviousScene);
		}
		if (g_xBAScene.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(g_xBAScene);
			g_xBAScene = Zenith_Scene();
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bBAActive = false;
	}
}

static void Setup_ZMBattleArena()
{
	g_bBAActive         = false;
	g_bBAFailed         = false;
	g_szBAFailure       = "test did not reach verification";
	g_bBADone           = false;
	g_bBAFound          = false;
	g_uBACount          = 0u;
	g_bBABuilt          = false;
	g_bBAFullyBuilt     = false;
	g_bBASetBiomeReturn = false;
	g_eBABiome          = ZM_BATTLE_BIOME_COUNT;
	g_bBARootResolved   = false;
	g_fBARootY          = 0.0f;
	g_xBAEntityID       = INVALID_ENTITY_ID;
	g_xBAPreviousScene  = Zenith_Scene();
	g_xBAScene          = Zenith_Scene();

	// 1. Warm-bake guard FIRST -- the arena's dressing sets are PROP-family assets.
	//    RequestSkip bypasses Verify, so no fixed-dt / scene state is installed
	//    until the bake is known warm. A tools build bakes stale families and
	//    stamps them; a non-tools build only checks the prop manifest. CI has no
	//    baked Assets tree -> skip rather than fail (C6).
#ifdef ZENITH_TOOLS
	const bool bWarm = ZM_BakeAllAssets();
#else
	const bool bWarm = ZM_BakeManifestCheck(
		ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
	if (!bWarm)
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"baked prop assets absent/stale -- run a *_True build");
		return;
	}

	// 2. Exists-guard the authored Battle scene (git-ignored; absent on CI).
	const std::string strBattlePath =
		std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
	if (!DiskFilePresent(strBattlePath))
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"Battle.zscen absent -- run a *_True build");
		return;
	}

	// 3. Deterministic frame timing (restored in cleanup).
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fZM_BA_FIXED_DT);

	// 4. Register build index 1 for Battle (idempotent -- the game boot already
	//    registers the identical path, so this is a same-path no-op re-assign, not
	//    the mismatch assert) + additively load it over the current scene, keeping
	//    the prior active scene to restore in cleanup.
	g_xEngine.Scenes().RegisterSceneBuildIndex(
		1, GAME_ASSETS_DIR "Scenes/Battle" ZENITH_SCENE_EXT);
	g_xBAPreviousScene = g_xEngine.Scenes().GetActiveScene();
	g_xBAScene = g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_ADDITIVE);
	if (!g_xBAScene.IsValid() || !g_xEngine.Scenes().SetActiveScene(g_xBAScene))
	{
		FailBA("could not additively load + activate the Battle scene");
		return;
	}
	g_bBAActive = true;
}

static bool Step_ZMBattleArena(int iFrame)
{
	if (!g_bBAActive || g_bBAFailed || g_bBADone)
	{
		return false;
	}

	// Let OnStart build the arena + the GPU upload settle before probing.
	if (iFrame < iZM_BA_SETTLE_FRAME)
	{
		return true;
	}

	// Locate the unique arena; poll (LoadScene is synchronous but Start dispatch
	// + build may span a couple of frames) until a deadline.
	ArenaView xArena;
	if (!FindUniqueArena(xArena))
	{
		g_uBACount = xArena.m_uCount;
		if (iFrame > iZM_BA_FIND_DEADLINE)
		{
			g_bBAFound = false;
			FailBA("no unique ZM_BattleArena instance found across scenes");
			return false;
		}
		return true;
	}
	g_bBAFound = true;
	g_uBACount = xArena.m_uCount;
	g_xBAEntityID = xArena.m_xEntityID;

	// Wait for the arena to finish building before switching biomes.
	if (!xArena.m_pxArena->IsBuilt())
	{
		if (iFrame > iZM_BA_BUILT_DEADLINE)
		{
			g_bBABuilt = false;
			FailBA("ZM_BattleArena never reported IsBuilt()");
			return false;
		}
		return true;
	}
	g_bBABuilt = true;

	// Structural check: every child entity (dome + 2 platforms + 6 dressing sets)
	// actually spawned, not just that BuildArena returned.
	g_bBAFullyBuilt = xArena.m_pxArena->IsFullyBuilt();

	// Drive one biome switch and capture the resulting active biome.
	g_bBASetBiomeReturn = xArena.m_pxArena->SetBiome(ZM_BATTLE_BIOME_VOLCANIC);
	g_eBABiome = xArena.m_pxArena->GetActiveBiome();

	// Capture the arena root entity's world Y (fallback placement check, since
	// per-dressing entity enabled-state is not on the public API).
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(g_xBAEntityID);
	if (xEntity.IsValid())
	{
		if (Zenith_TransformComponent* pxTransform =
			xEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			Zenith_Maths::Vector3 xPosition(0.0f);
			pxTransform->GetPosition(xPosition);
			g_fBARootY = xPosition.y;
			g_bBARootResolved = true;
		}
	}

	g_bBADone = true;
	return false;
}

static bool Verify_ZMBattleArena()
{
	bool bPassed = true;

	if (g_bBAFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_BattleArena] %s", g_szBAFailure);
		bPassed = false;
	}

	// Assertions are only meaningful once Setup built the scene (a skip returns
	// early and never reaches Verify; a Setup failure sets g_bBAActive true only
	// after the scene loads).
	if (g_bBAActive)
	{
		// Log EVERY captured value so a failure is fully localisable from the log.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_BattleArena] captured: found=%s count=%u built=%s fullyBuilt=%s setBiome=%s "
			"activeBiome=%d (want VOLCANIC=%d) rootResolved=%s rootY=%f (want ~%f)",
			g_bBAFound ? "true" : "false", g_uBACount,
			g_bBABuilt ? "true" : "false",
			g_bBAFullyBuilt ? "true" : "false",
			g_bBASetBiomeReturn ? "true" : "false",
			(int)g_eBABiome, (int)ZM_BATTLE_BIOME_VOLCANIC,
			g_bBARootResolved ? "true" : "false",
			(double)g_fBARootY, (double)ZM_BattleArena::fARENA_WORLD_Y);

		if (!g_bBADone)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] test did not reach its terminal capture");
			bPassed = false;
		}
		if (!g_bBAFound)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] expected a unique arena instance, saw %u", g_uBACount);
			bPassed = false;
		}
		if (!g_bBABuilt)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] arena was not IsBuilt() at capture");
			bPassed = false;
		}
		if (!g_bBAFullyBuilt)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] arena was not IsFullyBuilt() -- a child entity "
				"(dome / platform / dressing set) failed to spawn");
			bPassed = false;
		}
		if (!g_bBASetBiomeReturn)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] SetBiome(ZM_BATTLE_BIOME_VOLCANIC) returned false");
			bPassed = false;
		}
		if (g_eBABiome != ZM_BATTLE_BIOME_VOLCANIC)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] active biome after SetBiome was %d, expected VOLCANIC %d",
				(int)g_eBABiome, (int)ZM_BATTLE_BIOME_VOLCANIC);
			bPassed = false;
		}
		if (!g_bBARootResolved)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] arena root entity/transform did not resolve");
			bPassed = false;
		}
		else if (std::fabs(g_fBARootY - ZM_BattleArena::fARENA_WORLD_Y) > fZM_BA_ROOT_Y_EPS)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArena] arena root world Y %f is not within %f of %f",
				(double)g_fBARootY, (double)fZM_BA_ROOT_Y_EPS,
				(double)ZM_BattleArena::fARENA_WORLD_Y);
			bPassed = false;
		}
	}

	// Always restore the prior active scene + unload the additive Battle scene +
	// clear fixed dt (all guarded), even on a terminal failure.
	CleanupBAScene();
	return bPassed;
}

static const Zenith_AutomatedTest g_xZMBattleArenaTest = {
	"ZM_BattleArena_Test",
	&Setup_ZMBattleArena,
	&Step_ZMBattleArena,
	&Verify_ZMBattleArena,
	/* maxFrames */ 240,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleArenaTest);

// ============================================================================
// ZM_BattleArenaOwnScene_Test -- the ZM-D-089 REGRESSION LOCK for "BuildArena
// spawns into the arena's OWN scene, not the ACTIVE one".
//
// Unlike the sibling ZM_BattleArena_Test above, this test NEVER calls
// SetActiveScene: it additively loads Battle (build index 1) over the still-
// active FrontEnd (build index 0) and leaves focus exactly where it was. That
// is the S5 item-3 shape -- a battle loaded over a live overworld -- and it is
// precisely the condition the pre-fix code got wrong. A LoadSceneByIndex issued
// from a component OnUpdate / event handler is DEFERRED and drains at the end of
// Zenith_SceneSystem::Update, and the NEXT frame's DispatchPendingStarts fires
// the arena's OnStart BEFORE any component OnUpdate runs -- so no game-side code
// can move focus in between and the arena MUST resolve its own scene. While
// BuildArena used GetActiveSceneData(), all 9 children were created as roots of
// the OVERWORLD scene: IsFullyBuilt() still returned true (the entities exist,
// just in the wrong scene), so only a per-child scene-ownership check catches it.
// This test therefore FAILS before the fix and PASSES after.
//
// Note a non-focused additive scene still ticks: IsSceneUpdatable() gates on
// IsActivated(), which is a LOAD state (SCENE_STATE_LOADED), NOT "is the active
// scene" -- so OnStart genuinely runs here with FrontEnd in focus.
//
// GATING mirrors the sibling test (m_bRequiresGraphics = true + the same two
// RequestSkip guards), so a fresh CI checkout with no baked Assets tree skips.
// ============================================================================

namespace
{
	constexpr int iZM_BAOWN_BUILT_DEADLINE = 180;   // arena must be IsFullyBuilt() by here

	// FrontEnd is registered at build index 0 and is what the harness force-loads
	// between tests (SCENE_LOAD_SINGLE), so it is the active scene at Setup.
	constexpr int iZM_BAOWN_EXPECTED_ACTIVE_BUILD_INDEX = 0;

	// ---- Captured test state (all reset in Setup; batch mode reuses the process) ----
	bool            g_bPrereqsPresent    = false;  // both skip-guards passed
	bool            g_bOwnActive         = false;  // Setup loaded the additive scene
	bool            g_bOwnFailed         = false;
	const char*     g_szOwnFailure       = "test did not reach verification";
	bool            g_bOwnDone           = false;  // Step reached its terminal capture

	bool            g_bOwnFullyBuilt     = false;  // IsFullyBuilt() at capture time
	u_int           g_uOwnCount          = 0u;     // how many arena instances were seen
	bool            g_bOwnBattleDataResolved = false;  // GetSceneData(battle) was non-null
	u_int           g_uChildrenInBattleScene = 0u;  // children owned by the BATTLE scene
	int             g_iActiveBuildIndexDuringBuild = -1;  // active scene's build index

	Zenith_Scene    g_xOwnSceneBattle;

	void FailOwn(const char* szReason)
	{
		g_szOwnFailure = szReason;
		g_bOwnFailed = true;
	}

	// Cleanup on EVERY path incl. failure. Deliberately does NOT touch the active
	// scene: this test never moved focus, so there is nothing to restore.
	void CleanupOwnScene()
	{
		Zenith_InputSimulator::ClearFixedDt();
		if (g_xOwnSceneBattle.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(g_xOwnSceneBattle);
			g_xOwnSceneBattle = Zenith_Scene();
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bOwnActive = false;
	}
}

static void Setup_ZMBattleArenaOwnScene()
{
	g_bPrereqsPresent              = false;
	g_bOwnActive                   = false;
	g_bOwnFailed                   = false;
	g_szOwnFailure                 = "test did not reach verification";
	g_bOwnDone                     = false;
	g_bOwnFullyBuilt               = false;
	g_uOwnCount                    = 0u;
	g_bOwnBattleDataResolved       = false;
	g_uChildrenInBattleScene       = 0u;
	g_iActiveBuildIndexDuringBuild = -1;
	g_xOwnSceneBattle              = Zenith_Scene();

	// 1. Warm-bake guard FIRST (same order as the sibling test) -- RequestSkip
	//    bypasses Verify, so no fixed-dt / scene state is installed until the bake
	//    is known warm.
#ifdef ZENITH_TOOLS
	const bool bWarm = ZM_BakeAllAssets();
#else
	const bool bWarm = ZM_BakeManifestCheck(
		ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
	if (!bWarm)
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"baked prop assets absent/stale -- run a *_True build");
		return;
	}

	// 2. Exists-guard the authored Battle scene (git-ignored; absent on CI).
	const std::string strBattlePath =
		std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
	if (!DiskFilePresent(strBattlePath))
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"Battle.zscen absent -- run a *_True build");
		return;
	}
	g_bPrereqsPresent = true;

	// 3. Deterministic frame timing (restored in cleanup).
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fZM_BA_FIXED_DT);

	// 4. Additively load Battle and DELIBERATELY leave FrontEnd active -- the
	//    whole point of this test. Never call SetActiveScene here.
	g_xEngine.Scenes().RegisterSceneBuildIndex(
		1, GAME_ASSETS_DIR "Scenes/Battle" ZENITH_SCENE_EXT);
	g_xOwnSceneBattle = g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_ADDITIVE);
	if (!g_xOwnSceneBattle.IsValid())
	{
		FailOwn("could not additively load the Battle scene");
		return;
	}
	g_bOwnActive = true;
}

static bool Step_ZMBattleArenaOwnScene(int iFrame)
{
	if (!g_bOwnActive || g_bOwnFailed || g_bOwnDone)
	{
		return false;
	}

	// Poll for the unique arena to finish building (Start dispatch + build may
	// span a couple of frames) until the deadline.
	ArenaView xArena;
	const bool bFound = FindUniqueArena(xArena);
	g_uOwnCount = xArena.m_uCount;
	if (!bFound || !xArena.m_pxArena->IsFullyBuilt())
	{
		if (iFrame > iZM_BAOWN_BUILT_DEADLINE)
		{
			FailOwn("no unique ZM_BattleArena reported IsFullyBuilt() before the deadline");
			return false;
		}
		return true;
	}
	g_bOwnFullyBuilt = true;

	// Everything below runs in THIS frame: a Zenith_SceneData* points into
	// recyclable scene storage and must never be cached across frames.
	Zenith_SceneData* pxBattleData = g_xEngine.Scenes().GetSceneData(g_xOwnSceneBattle);
	g_bOwnBattleDataResolved = (pxBattleData != nullptr);

	// The actual ZM-D-089 probe: every child must be owned by the BATTLE scene,
	// not by whatever scene happened to be active when OnStart ran.
	for (u_int i = 0; i < ZM_BattleArena::uCHILD_COUNT; ++i)
	{
		const Zenith_EntityID xChildID = xArena.m_pxArena->GetChildEntityID(i);
		if (pxBattleData != nullptr
			&& g_xEngine.Scenes().GetSceneDataForEntity(xChildID) == pxBattleData)
		{
			++g_uChildrenInBattleScene;
		}
	}

	// Focus never moved: this test issues no SetActiveScene, so the active scene
	// is still the boot/between-tests FrontEnd (build index 0) that was in focus
	// while the arena built.
	g_iActiveBuildIndexDuringBuild =
		g_xEngine.Scenes().GetSceneInfo(g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;

	g_bOwnDone = true;
	return false;
}

static bool Verify_ZMBattleArenaOwnScene()
{
	bool bPassed = true;

	if (g_bOwnFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_BattleArenaOwnScene] %s", g_szOwnFailure);
		bPassed = false;
	}

	if (g_bOwnActive)
	{
		// Log EVERY captured value FIRST so a failure is fully diagnosable from
		// the log alone.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_BattleArenaOwnScene] captured: done=%s count=%u fullyBuilt=%s "
			"battleDataResolved=%s childrenInBattleScene=%u (want %u) "
			"activeBuildIndex=%d (want %d)",
			g_bOwnDone ? "true" : "false", g_uOwnCount,
			g_bOwnFullyBuilt ? "true" : "false",
			g_bOwnBattleDataResolved ? "true" : "false",
			g_uChildrenInBattleScene, ZM_BattleArena::uCHILD_COUNT,
			g_iActiveBuildIndexDuringBuild, iZM_BAOWN_EXPECTED_ACTIVE_BUILD_INDEX);

		if (!g_bOwnDone)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArenaOwnScene] test did not reach its terminal capture");
			bPassed = false;
		}
		if (g_uChildrenInBattleScene != ZM_BattleArena::uCHILD_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArenaOwnScene] ZM-D-089 VIOLATED: only %u of %u arena children "
				"are owned by the additively-loaded Battle scene -- BuildArena spawned "
				"into the ACTIVE scene instead of the arena's OWN scene, orphaning its "
				"children into the overworld",
				g_uChildrenInBattleScene, ZM_BattleArena::uCHILD_COUNT);
			bPassed = false;
		}
		if (g_iActiveBuildIndexDuringBuild != iZM_BAOWN_EXPECTED_ACTIVE_BUILD_INDEX)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleArenaOwnScene] ZM-D-089 premise broken: the active scene's build "
				"index was %d, expected %d -- focus moved to the Battle scene, so this run "
				"never exercised the additive-over-live-overworld path",
				g_iActiveBuildIndexDuringBuild, iZM_BAOWN_EXPECTED_ACTIVE_BUILD_INDEX);
			bPassed = false;
		}
	}

	CleanupOwnScene();
	return bPassed || !g_bPrereqsPresent;
}

static const Zenith_AutomatedTest g_xZMBattleArenaOwnSceneTest = {
	"ZM_BattleArenaOwnScene_Test",
	&Setup_ZMBattleArenaOwnScene,
	&Step_ZMBattleArenaOwnScene,
	&Verify_ZMBattleArenaOwnScene,
	/* maxFrames */ 240,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleArenaOwnSceneTest);

#endif // ZENITH_INPUT_SIMULATOR
