#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_Party.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_GameState -- the S5 item-5 (SC2) windowed gate. ONE test,
// m_bRequiresGraphics = true: ZM_GameStatePersistence_Test proves that the
// persistent ZM_GameStateManager (order 104, DontDestroyOnLoad) OWNS a seeded
// ZM_GameState reachable through the frozen accessor
//
//     static bool ZM_GameStateManager::TryGetGameState(ZM_GameState*&)
//
// and -- because the manager is DontDestroyOnLoad -- that the owned GameState
// SURVIVES a scene load. The proof is a real mutation written through the
// resolved pointer BEFORE the load and re-observed through a FRESH resolution
// AFTER it: only the one persistent instance could carry that mutation across.
//
// The seed is the fixed D4 starter (ZM_MakeStarterGameState): a party whose sole
// lead is Fernfawn at L5, with Fernfawn marked in the caught-set. The between-
// tests hook re-seeds the GameState to this starter before every test, so the
// starter assertions below are deterministic in the batch and this test needs no
// manual GameState reset -- it only leaves the scene clean on the way out.
//
// GATING (C4/C6): requires graphics, so the headless CI batch skips it (no GPU)
// and the unit baseline is unchanged. Setup RequestSkip()s when the baked
// Dawnmere terrain / scene or the authored PlayerHome scene are absent -- all
// git-ignored, so a fresh CI checkout skips rather than fails. Only a windowed
// *_True run bakes + loads the scenes and drives the slice.
//
// SC2's pure surface (ZM_MakeStarterGameState) is already covered by SC1 units;
// the manager accessor is ECS-coupled (it needs the live persistent singleton),
// so it is proven here by this one windowed test and NOT by any pure T0 unit.
// ============================================================================

namespace
{
	// -------------------------------------------------------------------------
	// Asset guards (mirror ZM_AutoTests_BattleTransition.cpp)
	// -------------------------------------------------------------------------

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

	bool RequiredDawnmereAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 7> astrRequired = {
			strRoot + "Scenes/Dawnmere" + ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" + ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			if (!DiskFilePresent(strPath))
			{
				return false;
			}
		}
		return true;
	}

	// The active scene's registered build index (0 = FrontEnd, 1 = Battle,
	// 2 = Dawnmere, 40 = PlayerHome). Used only to detect when a SINGLE load has
	// finished (the load is asynchronous, so the active scene changes over frames).
	int ActiveBuildIndex()
	{
		return g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
	}

	// -------------------------------------------------------------------------
	// ZM_GameStatePersistence_Test
	// -------------------------------------------------------------------------

	// Setup loads Dawnmere (build 2) so the persistent, boot-authored manager is
	// present + seeded, then Step performs a SECOND scene load (PlayerHome, build
	// 40) to prove the owned GameState outlives a scene change.
	constexpr int iSCENE_ID_DAWNMERE   = 2;
	constexpr int iSCENE_ID_PLAYERHOME = 40;

	constexpr int iGS_READY_DEADLINE  = 420;   // Dawnmere first-load ready window (model parity)
	constexpr int iGS_RELOAD_DEADLINE = 420;   // PlayerHome load window
	constexpr int iGS_SETTLE_FRAMES   = 8;     // let the post-load frame settle before re-resolving
	// 420 + 420 + 8 = 848 < the 1000-frame cap, so every phase owns a real deadline.

	enum class GSPhase
	{
		AwaitDawnmere,
		AwaitPlayerHome,
		Done,
	};

	GSPhase     g_eGSPhase          = GSPhase::Done;
	int         g_iGSPhaseFrames    = 0;
	int         g_iGSSettle         = 0;
	bool        g_bGSPrereqsPresent = false;
	bool        g_bGSActive         = false;
	bool        g_bGSFailed         = false;
	const char* g_szGSFailure       = "test did not reach verification";

	// ---- Captured BEFORE the second scene load (at Dawnmere) ----
	bool          g_bGSResolvedBefore   = false;
	u_int         g_uGSPartyCountBefore = 0u;
	bool          g_bGSLeadValidBefore  = false;
	ZM_SPECIES_ID g_eGSLeadSpeciesBefore = ZM_SPECIES_NONE;
	u_int         g_uGSLeadLevelBefore  = 0u;
	bool          g_bGSFernfawnCaughtBefore = false;
	bool          g_bGSKindletCaughtBefore  = false;   // expected false pre-mutation
	u_int         g_uGSCaughtCountBefore    = 0u;       // pre-mutation caught count (expect 1)
	u_int         g_uGSCaughtCountMutated   = 0u;       // post-mutation caught count (expect 2) == pre-load baseline

	// ---- Captured AFTER the second scene load (at PlayerHome), fresh resolution ----
	bool          g_bGSResolvedAfter    = false;
	u_int         g_uGSPartyCountAfter  = 0u;
	bool          g_bGSLeadValidAfter   = false;
	ZM_SPECIES_ID g_eGSLeadSpeciesAfter = ZM_SPECIES_NONE;
	u_int         g_uGSLeadLevelAfter   = 0u;
	bool          g_bGSFernfawnCaughtAfter = false;
	bool          g_bGSKindletCaughtAfter  = false;   // the mutation must have survived (expect true)
	u_int         g_uGSCaughtCountAfter    = 0u;       // must equal g_uGSCaughtCountMutated

	void FailGS(const char* szReason)
	{
		g_szGSFailure = szReason;
		g_bGSFailed = true;
		g_eGSPhase = GSPhase::Done;
	}

	void Setup_ZMGameStatePersistence()
	{
		g_eGSPhase          = GSPhase::Done;
		g_iGSPhaseFrames    = 0;
		g_iGSSettle         = 0;
		g_bGSPrereqsPresent = false;
		g_bGSActive         = false;
		g_bGSFailed         = false;
		g_szGSFailure       = "test did not reach verification";

		g_bGSResolvedBefore     = false;
		g_uGSPartyCountBefore   = 0u;
		g_bGSLeadValidBefore    = false;
		g_eGSLeadSpeciesBefore  = ZM_SPECIES_NONE;
		g_uGSLeadLevelBefore    = 0u;
		g_bGSFernfawnCaughtBefore = false;
		g_bGSKindletCaughtBefore  = false;
		g_uGSCaughtCountBefore    = 0u;
		g_uGSCaughtCountMutated   = 0u;

		g_bGSResolvedAfter      = false;
		g_uGSPartyCountAfter    = 0u;
		g_bGSLeadValidAfter     = false;
		g_eGSLeadSpeciesAfter   = ZM_SPECIES_NONE;
		g_uGSLeadLevelAfter     = 0u;
		g_bGSFernfawnCaughtAfter = false;
		g_bGSKindletCaughtAfter  = false;
		g_uGSCaughtCountAfter    = 0u;

		// Asset guard FIRST -- RequestSkip bypasses Verify, so install NO scene
		// state until every git-ignored input is confirmed present. Dawnmere is the
		// Setup scene; PlayerHome is the second-load scene Step drives to.
		const std::string strPlayerHomePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/PlayerHome" + ZENITH_SCENE_EXT;
		g_bGSPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strPlayerHomePath);
		if (!g_bGSPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere scene/terrain bake or the PlayerHome scene is absent");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		g_xEngine.Scenes().LoadSceneByIndex(iSCENE_ID_DAWNMERE, SCENE_LOAD_SINGLE);

		g_eGSPhase = GSPhase::AwaitDawnmere;
		g_bGSActive = true;
	}

	bool Step_ZMGameStatePersistence(int)
	{
		if (!g_bGSActive || g_bGSFailed || g_eGSPhase == GSPhase::Done)
		{
			return false;
		}

		++g_iGSPhaseFrames;
		switch (g_eGSPhase)
		{
		case GSPhase::AwaitDawnmere:
		{
			// Wait for the Setup SINGLE load to make Dawnmere the active scene.
			if (ActiveBuildIndex() != iSCENE_ID_DAWNMERE)
			{
				if (g_iGSPhaseFrames > iGS_READY_DEADLINE)
				{
					FailGS("Dawnmere did not become the active scene in time");
					return false;
				}
				return true;
			}

			// (1) Resolve the seeded starter through the frozen SC2 accessor. The
			// manager is a persistent, boot-authored singleton, so it must resolve
			// here regardless of which overworld scene is active.
			ZM_GameState* pxState = nullptr;
			g_bGSResolvedBefore = ZM_GameStateManager::TryGetGameState(pxState)
				&& pxState != nullptr;
			if (!g_bGSResolvedBefore)
			{
				FailGS("TryGetGameState returned no persistent GameState at Dawnmere "
					"(the SC2 manager is not seeded/owning a GameState)");
				return false;
			}

			// (2) The starter seed: exactly one party member -- Fernfawn L5 -- and
			// Fernfawn marked caught. Lead()/Count() confirmed against ZM_Party.h;
			// IsSet against ZM_SpeciesSet in ZM_GameState.h.
			g_uGSPartyCountBefore     = pxState->m_xParty.Count();
			const ZM_Monster& xLead   = pxState->m_xParty.Lead();
			g_bGSLeadValidBefore      = xLead.IsValid();
			g_eGSLeadSpeciesBefore    = xLead.m_eSpecies;
			g_uGSLeadLevelBefore      = xLead.m_uLevel;
			g_bGSFernfawnCaughtBefore = pxState->m_xCaught.IsSet(ZM_SPECIES_FERNFAWN);
			g_bGSKindletCaughtBefore  = pxState->m_xCaught.IsSet(ZM_SPECIES_KINDLET);
			g_uGSCaughtCountBefore    = pxState->m_xCaught.Count();

			// (3) Mutate through the resolved pointer to prove it is the REAL
			// persistent instance -- mark a second species caught, then record the
			// post-mutation count as the pre-load baseline. This is written to the
			// live owned GameState BEFORE any scene load, so no cached pointer
			// crosses the load boundary.
			pxState->m_xCaught.Mark(ZM_SPECIES_KINDLET);
			g_uGSCaughtCountMutated = pxState->m_xCaught.Count();

			// (4) Load an entirely different scene (PlayerHome interior, build 40),
			// SINGLE. A non-persistent manager would be torn down here; the
			// DontDestroyOnLoad manager -- and its owned GameState with our mutation
			// -- must survive.
			g_xEngine.Scenes().LoadSceneByIndex(iSCENE_ID_PLAYERHOME, SCENE_LOAD_SINGLE);
			g_eGSPhase = GSPhase::AwaitPlayerHome;
			g_iGSPhaseFrames = 0;
			g_iGSSettle = 0;
			return true;
		}

		case GSPhase::AwaitPlayerHome:
		{
			// Wait for the second SINGLE load to make PlayerHome the active scene.
			if (ActiveBuildIndex() != iSCENE_ID_PLAYERHOME)
			{
				if (g_iGSPhaseFrames > iGS_RELOAD_DEADLINE)
				{
					FailGS("PlayerHome did not become the active scene after the "
						"second load");
					return false;
				}
				return true;
			}

			// Let the post-load frame settle before re-resolving (mirrors the model
			// settle window).
			++g_iGSSettle;
			if (g_iGSSettle < iGS_SETTLE_FRAMES)
			{
				return true;
			}

			// Re-resolve FRESH -- never reuse the pre-load pointer (the component
			// pool can relocate entries). The SAME persistent instance must answer.
			ZM_GameState* pxState = nullptr;
			g_bGSResolvedAfter = ZM_GameStateManager::TryGetGameState(pxState)
				&& pxState != nullptr;
			if (!g_bGSResolvedAfter)
			{
				FailGS("TryGetGameState returned no persistent GameState after the "
					"scene load (the manager did not survive DontDestroyOnLoad)");
				return false;
			}

			g_uGSPartyCountAfter     = pxState->m_xParty.Count();
			const ZM_Monster& xLead  = pxState->m_xParty.Lead();
			g_bGSLeadValidAfter      = xLead.IsValid();
			g_eGSLeadSpeciesAfter    = xLead.m_eSpecies;
			g_uGSLeadLevelAfter      = xLead.m_uLevel;
			g_bGSFernfawnCaughtAfter = pxState->m_xCaught.IsSet(ZM_SPECIES_FERNFAWN);
			g_bGSKindletCaughtAfter  = pxState->m_xCaught.IsSet(ZM_SPECIES_KINDLET);
			g_uGSCaughtCountAfter    = pxState->m_xCaught.Count();

			g_eGSPhase = GSPhase::Done;
			return false;
		}

		case GSPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMGameStatePersistence()
	{
		bool bPassed = true;

		if (g_bGSActive)
		{
			// One line dumping EVERY captured value so a failure is fully localisable
			// from the log alone.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_GameStatePersistence] captured: failed=%s (%s) "
				"resolvedBefore=%s countBefore=%u (want 1) leadValidBefore=%s "
				"speciesBefore=%d (want FERNFAWN=%d) levelBefore=%u (want 5) "
				"fernfawnCaughtBefore=%s (want true) kindletCaughtBefore=%s (want false) "
				"caughtCountBefore=%u (want 1) caughtCountMutated=%u (want 2) "
				"resolvedAfter=%s countAfter=%u (want 1) leadValidAfter=%s "
				"speciesAfter=%d (want FERNFAWN=%d) levelAfter=%u (want 5) "
				"fernfawnCaughtAfter=%s (want true) kindletCaughtAfter=%s (want true) "
				"caughtCountAfter=%u (want == caughtCountMutated)",
				g_bGSFailed ? "true" : "false", g_szGSFailure,
				g_bGSResolvedBefore ? "true" : "false",
				g_uGSPartyCountBefore,
				g_bGSLeadValidBefore ? "true" : "false",
				(int)g_eGSLeadSpeciesBefore, (int)ZM_SPECIES_FERNFAWN,
				g_uGSLeadLevelBefore,
				g_bGSFernfawnCaughtBefore ? "true" : "false",
				g_bGSKindletCaughtBefore ? "true" : "false",
				g_uGSCaughtCountBefore,
				g_uGSCaughtCountMutated,
				g_bGSResolvedAfter ? "true" : "false",
				g_uGSPartyCountAfter,
				g_bGSLeadValidAfter ? "true" : "false",
				(int)g_eGSLeadSpeciesAfter, (int)ZM_SPECIES_FERNFAWN,
				g_uGSLeadLevelAfter,
				g_bGSFernfawnCaughtAfter ? "true" : "false",
				g_bGSKindletCaughtAfter ? "true" : "false",
				g_uGSCaughtCountAfter);

			if (g_bGSFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] %s", g_szGSFailure);
				bPassed = false;
			}

			// --- (1)+(2) the seeded starter is reachable and correct ---
			if (!g_bGSResolvedBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] the seeded GameState was not reachable "
					"through TryGetGameState at Dawnmere");
				bPassed = false;
			}
			if (g_uGSPartyCountBefore != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] starter party count was %u, expected 1",
					g_uGSPartyCountBefore);
				bPassed = false;
			}
			if (!g_bGSLeadValidBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] the starter party lead was not a valid "
					"monster record");
				bPassed = false;
			}
			if (g_eGSLeadSpeciesBefore != ZM_SPECIES_FERNFAWN)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] starter lead species was %d, expected "
					"FERNFAWN %d", (int)g_eGSLeadSpeciesBefore, (int)ZM_SPECIES_FERNFAWN);
				bPassed = false;
			}
			if (g_uGSLeadLevelBefore != 5u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] starter lead level was %u, expected 5",
					g_uGSLeadLevelBefore);
				bPassed = false;
			}
			if (!g_bGSFernfawnCaughtBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] the starter caught-set did not mark "
					"FERNFAWN");
				bPassed = false;
			}
			if (g_bGSKindletCaughtBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] KINDLET was already marked before the "
					"mutation -- the starter seed is not clean (between-tests re-seed "
					"did not run)");
				bPassed = false;
			}
			if (g_uGSCaughtCountBefore != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] starter caught count was %u, expected 1 "
					"(FERNFAWN only)", g_uGSCaughtCountBefore);
				bPassed = false;
			}

			// --- (3) the mutation took effect on the live instance ---
			if (g_uGSCaughtCountMutated != g_uGSCaughtCountBefore + 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] marking KINDLET did not raise the caught "
					"count (%u -> %u, expected +1)",
					g_uGSCaughtCountBefore, g_uGSCaughtCountMutated);
				bPassed = false;
			}

			// --- (4) the SAME persistent instance survived the scene load ---
			if (!g_bGSResolvedAfter)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] the GameState was not reachable after the "
					"scene load -- DontDestroyOnLoad persistence failed");
				bPassed = false;
			}
			if (g_uGSPartyCountAfter != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] party count after the load was %u, "
					"expected 1", g_uGSPartyCountAfter);
				bPassed = false;
			}
			if (!g_bGSLeadValidAfter)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] the party lead after the load was not a "
					"valid monster record");
				bPassed = false;
			}
			if (g_eGSLeadSpeciesAfter != ZM_SPECIES_FERNFAWN)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] lead species after the load was %d, "
					"expected FERNFAWN %d",
					(int)g_eGSLeadSpeciesAfter, (int)ZM_SPECIES_FERNFAWN);
				bPassed = false;
			}
			if (g_uGSLeadLevelAfter != 5u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] lead level after the load was %u, "
					"expected 5", g_uGSLeadLevelAfter);
				bPassed = false;
			}
			if (!g_bGSFernfawnCaughtAfter)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] FERNFAWN was no longer marked caught "
					"after the load");
				bPassed = false;
			}
			if (!g_bGSKindletCaughtAfter)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] the KINDLET mutation did NOT survive the "
					"scene load -- the resolved GameState is not the persistent instance");
				bPassed = false;
			}
			if (g_uGSCaughtCountAfter != g_uGSCaughtCountMutated)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_GameStatePersistence] caught count after the load was %u, "
					"expected %u (unchanged from the pre-load mutation)",
					g_uGSCaughtCountAfter, g_uGSCaughtCountMutated);
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), even on a terminal failure:
		// wipe input, then restore the FrontEnd boot scene. The between-tests hook
		// re-seeds the GameState, so no manual GameState reset is needed here -- this
		// only leaves the scene clean for the next test.
		Zenith_InputSimulator::ResetAllInputState();
		if (g_bGSActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		g_bGSActive = false;

		return bPassed || !g_bGSPrereqsPresent;
	}
}

static const Zenith_AutomatedTest g_xZMGameStatePersistenceTest = {
	"ZM_GameStatePersistence_Test",
	&Setup_ZMGameStatePersistence,
	&Step_ZMGameStatePersistence,
	&Verify_ZMGameStatePersistence,
	/* maxFrames */ 1000,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMGameStatePersistenceTest);

#endif // ZENITH_INPUT_SIMULATOR
