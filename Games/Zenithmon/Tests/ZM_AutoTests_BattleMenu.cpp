#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "UI/Zenith_UIRect.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleDirector.h"           // GetCore / GetHudMenuScreen / GetHudMenuCursor
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"          // TryGetGameState (persistent lead exp-persist proof, SC3)
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"    // ZM_SetInstantBattlesForTests, GetWinner
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"          // GetEngine event stream
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"           // ZM_BATTLE_EVENT_FLEE
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"           // ZM_SIDE_PLAYER / ZM_SIDE_COUNT
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"                 // ZM_ITEM_PRIMEORB / ZM_ITEM_CATCHORB (catch-ball override)
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"              // ZM_GameState (persistent lead read, SC3)
#include "Zenithmon/Source/Party/ZM_Monster.h"                // ZM_Monster::m_uCurrentExp / m_uLevel
#include "Zenithmon/Source/Party/ZM_Party.h"                  // ZM_Party::Lead()
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"              // ZM_BattleMenuScreen + ZM_BATTLE_MENU_* enums
#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_BattleMenu -- the windowed gate for the player-driven battle menu
// on ZM_UI_BattleHUD (owned by ZM_BattleDirector). THREE tests, all
// m_bRequiresGraphics = true:
//   * ZM_BattleMenuWin_Test -- ENTER-spam drives Fight->move0 every turn; the
//     placeholder L5 player KOs a deliberately weak L2 wild FERNFAWN, so the
//     DIRECTOR ends the battle with the PLAYER as winner and an HP bar at 0.
//   * ZM_BattleMenuRun_Test -- reads the live menu screen/cursor and confirms
//     Run once the cursor is on it; the faster L5 player flees the same weak
//     enemy, so the engine emits a FLEE event and the battle ends with NO winner.
//   * ZM_BattleMenuCatch_Test (S5 item-5 SC4) -- forces a DISTINCT wild KINDLET and
//     installs a GUARANTEED-catch ball (ZM_ITEM_PRIMEORB), then drives the menu to
//     Catch; the wild monster is caught, so the core ends with the PLAYER as winner
//     and the persistent GameState gains a party member + a marked caught-set entry.
//
// Both CLONE ZM_AutoTests_BattleHUD.cpp's shipped phase machine (its Dawnmere
// runtime-ready gate, the forced wild encounter, fixed-dt 1/30, zm_instant_battles
// on in Setup / off in teardown, the RequestSkip guard order, and the director-
// ended + exact-resume invariants), differing only in the input DRIVE (menu-
// aware) and the win/flee assertions. The shared file-local helpers are internal
// linkage in the shipped TU and cannot be linked across TUs, so they are
// re-declared verbatim here; the two tests share one phase machine that branches
// on a MenuTestMode set by each test's thin Setup.
//
// Since the director no longer auto-submits (SC5), a battle that receives NO menu
// input never resolves -- these tests are the player-input side of the SC5 slice.
// ============================================================================

namespace
{
	// -------------------------------------------------------------------------
	// Shared asset guards + entity views (re-declared from ZM_AutoTests_Battle
	// HUD.cpp -- those copies are file-local and not linkable across TUs).
	// -------------------------------------------------------------------------

	struct PlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	struct CameraView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_FollowCamera* m_pxFollow = nullptr;
		Zenith_CameraComponent* m_pxCamera = nullptr;
	};

	bool FindActivePlayer(PlayerView& xOut)
	{
		xOut = PlayerView{};
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&xOut](Zenith_EntityID xID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent& xTransform)
			{
				if (xOut.m_xEntityID != INVALID_ENTITY_ID)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
				xTransform.GetPosition(xOut.m_xPosition);
			});
		return xOut.m_xEntityID != INVALID_ENTITY_ID;
	}

	bool FindActiveCamera(CameraView& xOut)
	{
		xOut = CameraView{};
		g_xEngine.Scenes().QueryActiveScene<
			ZM_FollowCamera,
			Zenith_CameraComponent>().ForEach(
			[&xOut](Zenith_EntityID xID,
				ZM_FollowCamera& xFollow,
				Zenith_CameraComponent& xCamera)
			{
				if (xOut.m_xEntityID != INVALID_ENTITY_ID)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxFollow = &xFollow;
				xOut.m_pxCamera = &xCamera;
			});
		return xOut.m_xEntityID != INVALID_ENTITY_ID;
	}

	bool ActiveGrassIsReady()
	{
		bool bReady = false;
		g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().ForEach(
			[&bReady](Zenith_EntityID, ZM_TerrainGrass& xGrass)
			{
				bReady = bReady || xGrass.IsGrassApplied();
			});
		return bReady;
	}

	bool FindActiveTerrainGrassEntity(Zenith_EntityID& xOut)
	{
		xOut = INVALID_ENTITY_ID;
		g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().ForEach(
			[&xOut](Zenith_EntityID xID, ZM_TerrainGrass&)
			{
				if (xOut == INVALID_ENTITY_ID)
				{
					xOut = xID;
				}
			});
		return xOut != INVALID_ENTITY_ID;
	}

	bool DawnmereRuntimeReady(PlayerView& xPlayer, CameraView& xCamera)
	{
		return FindActivePlayer(xPlayer)
			&& FindActiveCamera(xCamera)
			&& xPlayer.m_pxCollider->HasValidBody()
			&& xPlayer.m_pxController->IsGrounded()
			&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
			&& xCamera.m_pxFollow->GetCurrentArmDistance() > 0.0f
			&& ActiveGrassIsReady();
	}

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

	// The persistent ZM_BattleTransition singleton, resolved FRESH each frame. The
	// component pool relocates entries on swap-and-pop, so this pointer must never
	// be cached across frames -- every caller re-resolves through the generation-
	// bearing ID.
	ZM_BattleTransition* ResolveSingletonBattleTransition()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_BattleTransition::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_BattleTransition>()
			: nullptr;
	}

	// -------------------------------------------------------------------------
	// Direction search (re-declared from ZM_AutoTests_BattleHUD.cpp)
	// -------------------------------------------------------------------------

	constexpr float fBM_SEARCH_MIN_DIST = 1.5f;   // >= 1 tile so the destination is a genuine transition
	constexpr float fBM_SEARCH_MAX_DIST = 24.0f;
	constexpr float fBM_SEARCH_STEP     = 0.5f;

	struct WalkChoice
	{
		Zenith_KeyCode m_eKey         = ZENITH_KEY_W;
		float          m_fHitDistance = 0.0f;
		float          m_fHitDensity  = 0.0f;
		bool           m_bFound       = false;
	};

	WalkChoice ChooseWalkDirection(float fPX, float fPZ, const ZM_GrassDensityMap& xMap)
	{
		struct Candidate
		{
			Zenith_KeyCode m_eKey;
			float          m_fDx;
			float          m_fDz;
		};
		// Priority order only breaks ties on equal distance; the nearest hit wins
		// across all four (shortest walk = least follow-camera drift = most robust).
		const Candidate axCandidates[4] = {
			{ ZENITH_KEY_W,  0.0f,  1.0f },   // camera-forward +Z at spawn
			{ ZENITH_KEY_S,  0.0f, -1.0f },
			{ ZENITH_KEY_D,  1.0f,  0.0f },
			{ ZENITH_KEY_A, -1.0f,  0.0f },
		};

		const float fThreshold = ZM_TallGrassSystem::fGRASS_DENSITY_THRESHOLD;
		WalkChoice xBest;
		for (const Candidate& xCand : axCandidates)
		{
			for (float fDist = fBM_SEARCH_MIN_DIST;
				fDist <= fBM_SEARCH_MAX_DIST;
				fDist += fBM_SEARCH_STEP)
			{
				const float fSampleX = fPX + xCand.m_fDx * fDist;
				const float fSampleZ = fPZ + xCand.m_fDz * fDist;
				const float fDensity = xMap.SampleWorld(fSampleX, fSampleZ);
				if (fDensity >= fThreshold)
				{
					if (!xBest.m_bFound || fDist < xBest.m_fHitDistance)
					{
						xBest.m_eKey = xCand.m_eKey;
						xBest.m_fHitDistance = fDist;
						xBest.m_fHitDensity = fDensity;
						xBest.m_bFound = true;
					}
					break;   // nearest hit for this cardinal
				}
			}
		}
		return xBest;
	}

	// -------------------------------------------------------------------------
	// The shared menu-driven phase machine
	// -------------------------------------------------------------------------

	// Three tests, one machine. Set by each test's thin Setup; the drive + the
	// win/flee/catch assertions branch on it.
	enum class MenuTestMode { Win, Run, Catch };

	// A deliberately weak, low wild enemy so BOTH slices are reliable: the L5
	// placeholder player (ZM_BattleDirector.cpp) reliably KOs an L2 FERNFAWN
	// (Win), and is faster than it, so a wild flee always succeeds (Run). FERNFAWN
	// is dex row 0 -- a starter with a guaranteed level-1 move, so an L2 wild spec
	// is never moveless. If the orchestrator finds the player does not reliably
	// win/flee at L2, this is the single knob to lower/raise.
	constexpr ZM_SPECIES_ID eBM_ENEMY_SPECIES = ZM_SPECIES_FERNFAWN;
	constexpr u_int         uBM_ENEMY_LEVEL   = 2u;

	// The Catch test forces a DISTINCT wild species (KINDLET) so a successful catch
	// provably changes BOTH the party count (1 -> 2) and the caught-set (KINDLET is
	// unmarked at start -- the starter only marks FERNFAWN). L2 keeps it weak.
	constexpr ZM_SPECIES_ID eBM_CATCH_ENEMY_SPECIES = ZM_SPECIES_KINDLET;
	constexpr u_int         uBM_CATCH_ENEMY_LEVEL   = 2u;

	// Coarser than 1/60 (mirrors the shipped round trip): the trip must fit the
	// additive load + arena build + the director's headless battle + grass regen
	// poll chains into the frame budget, and 1/60 would be too tight.
	constexpr float fBM_FIXED_DT = 1.0f / 30.0f;

	enum class BMPhase
	{
		AwaitReady,
		Baseline,
		Walk,
		AwaitInBattle,
		AwaitResume,
		Done,
	};

	constexpr int iBM_READY_DEADLINE       = 420;   // Dawnmere first-load ready window (round-trip parity)
	constexpr int iBM_BASELINE_FRAMES      = 4;     // let OnUpdate record its baseline tile before we drive
	constexpr int iBM_WALK_DEADLINE        = 460;   // ample budget to reach a <= 24 m grass tile at walk speed
	constexpr int iBM_INBATTLE_DEADLINE    = 600;   // fade-out + additive load + arena build + fade-in
	constexpr int iBM_RESUME_DEADLINE      = 600;   // player drives the menu to resolution + fade + unload + regrow
	constexpr int iBM_RESUME_SETTLE_FRAMES = 8;     // let the resume settle before sampling the exact state

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	MenuTestMode   g_eBMMode              = MenuTestMode::Win;
	BMPhase        g_eBMPhase             = BMPhase::Done;
	int            g_iBMPhaseFrames       = 0;
	int            g_iBMResumeSettle      = 0;
	bool           g_bBMResumeReached     = false;
	bool           g_bBMInBattleCaptured  = false;
	bool           g_bBMPrereqsPresent    = false;
	bool           g_bBMActive            = false;
	bool           g_bBMFailed            = false;
	const char*    g_szBMFailure          = "test did not reach verification";
	Zenith_KeyCode g_eBMWalkKey           = ZENITH_KEY_W;

	// ---- Entry captures (before the encounter) ----
	u_int          g_uBMEntryGrassBlades  = 0u;   // grass-restore baseline

	// ---- IN_BATTLE captures ----
	Zenith_Maths::Vector3 g_xBMParkedPos  = Zenith_Maths::Vector3(0.0f);  // THE drift baseline

	// ---- Resume captures (exact restore) ----
	int                   g_iBMBuildIndexAfter       = -1;
	bool                  g_bBMBattleSceneUnloaded   = false;
	u_int                 g_uBMGrassAfter            = 0u;
	Zenith_Maths::Vector3 g_xBMResumePlayerPos       = Zenith_Maths::Vector3(0.0f);
	bool                  g_bBMPlayerMovementEnabled = false;
	bool                  g_bBMPlayerResolved        = false;
	u_int                 g_uBMCompletedAfter        = 0u;   // the DIRECTOR's end request lands here
	u_int                 g_uBMAbortedAfter          = 0u;

	// ---- Menu / outcome captures (latched every frame the Battle scene is loaded) ----
	bool    g_bBMDirectorSeen     = false;   // the BattleDirector entity resolved at least once
	bool    g_bBMWinnerCaptured   = false;   // the core reached OVER and its winner was read
	ZM_SIDE g_eBMWinner           = ZM_SIDE_COUNT;
	bool    g_bBMFleeSeen         = false;   // a ZM_BATTLE_EVENT_FLEE appeared in the engine stream
	float   g_fBMMinHudRectFill   = 2.0f;    // min fill over the HUD's two HP bars (min-latched)
	u_int   g_uBMHudRectCount     = 0u;      // how many HP bars the HUD exposed

	// ---- Persistent-exp captures (SC3: a win awards exp and writes it, plus level,
	// back to the persistent GameState lead; only asserted in the Win test) ----
	bool    g_bBMExpCapturedBefore = false;  // the manager resolved a GameState before the encounter
	u_int   g_uBMExpBefore         = 0u;     // persistent lead cumulative exp pre-battle
	u_int   g_uBMLevelBefore       = 0u;     // persistent lead level pre-battle
	bool    g_bBMExpCapturedAfter  = false;  // the manager resolved a GameState after the resume
	u_int   g_uBMExpAfter          = 0u;     // persistent lead cumulative exp post-resume
	u_int   g_uBMLevelAfter        = 0u;     // persistent lead level post-resume

	// ---- Catch captures (SC4: a successful catch adds the caught wild monster to the
	// persistent party and marks the caught-set; only asserted in the Catch test) ----
	bool    g_bBMPartyCapturedBefore = false;  // the manager resolved a GameState before the encounter
	u_int   g_uBMPartyCountBefore    = 0u;     // persistent party size pre-catch
	bool    g_bBMCatchSpeciesBefore  = false;  // the DISTINCT wild species already in the caught-set pre-catch
	bool    g_bBMPartyCapturedAfter  = false;  // the manager resolved a GameState after the resume
	u_int   g_uBMPartyCountAfter     = 0u;     // persistent party size post-catch
	bool    g_bBMCatchSpeciesAfter   = false;  // the DISTINCT wild species in the caught-set post-catch

	// Recursively take the minimum GetFillAmount() over every UIRect in an element
	// subtree. On the battle HUD the only rects are the two HP bars, so the min is
	// the fill of the more-depleted side.
	void ScanMinRectFill(Zenith_UI::Zenith_UIElement* pxElement, float& fMinFill, u_int& uRectCount)
	{
		if (pxElement == nullptr)
		{
			return;
		}
		if (pxElement->GetType() == Zenith_UI::UIElementType::Rect)
		{
			Zenith_UI::Zenith_UIRect* pxRect = static_cast<Zenith_UI::Zenith_UIRect*>(pxElement);
			const float fFill = pxRect->GetFillAmount();
			if (fFill < fMinFill)
			{
				fMinFill = fFill;
			}
			++uRectCount;
		}
		for (size_t i = 0; i < pxElement->GetChildCount(); ++i)
		{
			ScanMinRectFill(pxElement->GetChild(i), fMinFill, uRectCount);
		}
	}

	// Resolve the unique BattleDirector across every loaded scene, drive the menu
	// per the active mode, and latch the winner / flee / HP-bar-fill outcome. A
	// no-op when the Battle scene is not loaded (the director entity is absent).
	void DriveAndCaptureMenu()
	{
		Zenith_EntityID xDirectorID = INVALID_ENTITY_ID;
		g_xEngine.Scenes().QueryAllScenes<ZM_BattleDirector>().ForEach(
			[&xDirectorID](Zenith_EntityID xID, ZM_BattleDirector&)
			{
				if (xDirectorID == INVALID_ENTITY_ID)
				{
					xDirectorID = xID;
				}
			});
		if (xDirectorID == INVALID_ENTITY_ID)
		{
			return;   // Battle scene not loaded
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xDirectorID);
		if (!xEntity.IsValid())
		{
			return;
		}
		ZM_BattleDirector* pxDirector = xEntity.TryGetComponent<ZM_BattleDirector>();
		if (pxDirector == nullptr)
		{
			return;
		}
		g_bBMDirectorSeen = true;

		// ---- Drive the player's action THIS frame ----
		if (g_eBMMode == MenuTestMode::Win)
		{
			// The default menu is ACTION_ROOT, cursor 0 = Fight: one ENTER opens the
			// move list, the next ENTER submits move 0. Pressing ENTER every frame
			// therefore picks Fight->move0 each turn (a HIDDEN-menu press is inert).
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		else if (g_eBMMode == MenuTestMode::Catch)
		{
			// Menu-aware catch: only confirm once the committed cursor is on Catch, so
			// we never accidentally open the move list. DOWN walks Fight(0)->Catch(1);
			// ENTER on Catch submits {ZM_ACTION_ITEM, catch ball}. With ZM_ITEM_PRIMEORB
			// installed in Setup the capture succeeds on turn 1 (zero RNG).
			const ZM_BattleMenuScreen eScreen = pxDirector->GetHudMenuScreen();
			const int                 iCursor = pxDirector->GetHudMenuCursor();
			if (eScreen == ZM_BATTLE_MENU_ACTION_ROOT)
			{
				if (iCursor == (int)ZM_BATTLE_MENU_CATCH)
				{
					Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				}
				else
				{
					Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
				}
			}
			// HIDDEN (fading / between turns) or MOVE_SELECT (never reached): press nothing.
		}
		else   // Run
		{
			// Menu-aware flee: only confirm once the committed cursor is on Run, so we
			// never accidentally open the move list. DOWN moves Fight(0)->Catch(1)->Run(2);
			// ENTER on Run submits {ZM_ACTION_RUN}. A failed flee returns to ACTION_ROOT
			// and this converges again next turn.
			const ZM_BattleMenuScreen eScreen = pxDirector->GetHudMenuScreen();
			const int                 iCursor = pxDirector->GetHudMenuCursor();
			if (eScreen == ZM_BATTLE_MENU_ACTION_ROOT)
			{
				if (iCursor == (int)ZM_BATTLE_MENU_RUN)
				{
					Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				}
				else
				{
					Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
				}
			}
			// HIDDEN (fading / between turns) or MOVE_SELECT (never reached in this
			// gated flow): press nothing.
		}

		// ---- Latch the outcome while the scene is still loaded ----
		const ZM_BattleDirectorCore& xCore = pxDirector->GetCore();
		if (xCore.IsOver())
		{
			g_eBMWinner         = xCore.GetWinner();   // final + stable once over
			g_bBMWinnerCaptured = true;
		}

		const ZM_BattleEngine& xEngine = xCore.GetEngine();
		for (u_int i = 0u; i < xEngine.GetEventCount(); ++i)
		{
			if (xEngine.GetEvent(i).m_eKind == ZM_BATTLE_EVENT_FLEE)
			{
				g_bBMFleeSeen = true;
				break;
			}
		}

		Zenith_UIComponent* pxUI = xEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI != nullptr)
		{
			float fMinFill  = 2.0f;
			u_int uRectCount = 0u;
			for (Zenith_UI::Zenith_UIElement* pxRoot : pxUI->GetCanvas().GetElements())
			{
				ScanMinRectFill(pxRoot, fMinFill, uRectCount);
			}
			if (uRectCount >= 2u)   // only meaningful once both HP bars exist
			{
				if (fMinFill < g_fBMMinHudRectFill)
				{
					g_fBMMinHudRectFill = fMinFill;
				}
				g_uBMHudRectCount = uRectCount;
			}
		}
	}

	void FailBM(const char* szReason)
	{
		g_szBMFailure = szReason;
		g_bBMFailed = true;
		g_eBMPhase = BMPhase::Done;
		Zenith_InputSimulator::SetKeyHeld(g_eBMWalkKey, false);
	}

	void SetupCommon()
	{
		g_eBMPhase                 = BMPhase::Done;
		g_iBMPhaseFrames           = 0;
		g_iBMResumeSettle          = 0;
		g_bBMResumeReached         = false;
		g_bBMInBattleCaptured      = false;
		g_bBMPrereqsPresent        = false;
		g_bBMActive                = false;
		g_bBMFailed                = false;
		g_szBMFailure              = "test did not reach verification";
		g_eBMWalkKey               = ZENITH_KEY_W;

		g_uBMEntryGrassBlades      = 0u;

		g_xBMParkedPos             = Zenith_Maths::Vector3(0.0f);

		g_iBMBuildIndexAfter       = -1;
		g_bBMBattleSceneUnloaded   = false;
		g_uBMGrassAfter            = 0u;
		g_xBMResumePlayerPos       = Zenith_Maths::Vector3(0.0f);
		g_bBMPlayerMovementEnabled = false;
		g_bBMPlayerResolved        = false;
		g_uBMCompletedAfter        = 0u;
		g_uBMAbortedAfter          = 0u;

		g_bBMDirectorSeen          = false;
		g_bBMWinnerCaptured        = false;
		g_eBMWinner                = ZM_SIDE_COUNT;
		g_bBMFleeSeen              = false;
		g_fBMMinHudRectFill        = 2.0f;
		g_uBMHudRectCount          = 0u;

		g_bBMExpCapturedBefore     = false;
		g_uBMExpBefore             = 0u;
		g_uBMLevelBefore           = 0u;
		g_bBMExpCapturedAfter      = false;
		g_uBMExpAfter              = 0u;
		g_uBMLevelAfter            = 0u;

		g_bBMPartyCapturedBefore   = false;
		g_uBMPartyCountBefore      = 0u;
		g_bBMCatchSpeciesBefore    = false;
		g_bBMPartyCapturedAfter    = false;
		g_uBMPartyCountAfter       = 0u;
		g_bBMCatchSpeciesAfter     = false;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO
		// process state (fixed dt, instant-battles flag, scene load) until EVERY
		// git-ignored input is confirmed present -- the Dawnmere terrain/scene, the
		// authored Battle scene, and the baked PROP family (the arena's dressing
		// sets). A tools build bakes stale families and stamps them; a non-tools
		// build only checks the prop manifest. CI has no baked Assets tree -> skip.
		const std::string strBattlePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
#ifdef ZENITH_TOOLS
		const bool bWarm = ZM_BakeAllAssets();
#else
		const bool bWarm = ZM_BakeManifestCheck(
			ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
		g_bBMPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strBattlePath)
			&& bWarm;
		if (!g_bBMPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere / Battle / prop bake absent -- run a *_True build");
			return;
		}

		// Clear the transition's ownerless statics so an earlier batched test cannot
		// bleed a pending latch in. Does NOT touch the live subscription.
		ZM_BattleTransition::ResetRuntimeStateForTests();

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fBM_FIXED_DT);

		// Collapse every presentation op to zero duration so a turn drains in a single
		// Tick; the AWAIT_INPUT gate still needs one real frame per menu press, which
		// is what these tests supply.
		ZM_SetInstantBattlesForTests(true);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eBMPhase = BMPhase::AwaitReady;
		g_bBMActive = true;
	}

	void Setup_ZMBattleMenuWin()
	{
		g_eBMMode = MenuTestMode::Win;
		SetupCommon();
	}

	void Setup_ZMBattleMenuRun()
	{
		g_eBMMode = MenuTestMode::Run;
		SetupCommon();
	}

	void Setup_ZMBattleMenuCatch()
	{
		g_eBMMode = MenuTestMode::Catch;
		SetupCommon();
		// A GUARANTEED capture (the prime orb never fails), so the catch resolves on turn
		// 1 with zero RNG dependence. Installed ONLY when the test will actually run: a
		// RequestSkip bypasses Verify, so its teardown would never restore the gameplay
		// ball -- gating on g_bBMPrereqsPresent keeps the ball override from leaking into
		// a later batched test.
		if (g_bBMPrereqsPresent)
		{
			ZM_SetCatchBallForTests(ZM_ITEM_PRIMEORB);
		}
	}

	bool Step_ZMBattleMenu(int)
	{
		if (!g_bBMActive || g_bBMFailed || g_eBMPhase == BMPhase::Done)
		{
			return false;
		}

		++g_iBMPhaseFrames;
		switch (g_eBMPhase)
		{
		case BMPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iBMPhaseFrames > iBM_READY_DEADLINE)
				{
					FailBM("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}

			// The battle machine is a persistent-scene singleton -- if it does not
			// resolve here, the subscription under test cannot exist either.
			if (ResolveSingletonBattleTransition() == nullptr)
			{
				FailBM("no unique ZM_BattleTransition singleton");
				return false;
			}

			// Resolve the terrain entity and attach the gameplay tall-grass system.
			Zenith_EntityID xTerrainID = INVALID_ENTITY_ID;
			if (!FindActiveTerrainGrassEntity(xTerrainID))
			{
				FailBM("no ZM_TerrainGrass entity in the active Dawnmere scene");
				return false;
			}
			Zenith_Entity xTerrain = g_xEngine.Scenes().ResolveEntity(xTerrainID);
			if (!xTerrain.IsValid())
			{
				FailBM("terrain entity did not resolve to a live handle");
				return false;
			}

			// A runtime AddComponent returns a reference valid only until the next
			// ZM_TallGrassSystem pool mutation, so call OnAwake + the arm seams
			// IMMEDIATELY, with no intervening component add/remove.
			ZM_TallGrassSystem* pxSystem = xTerrain.TryGetComponent<ZM_TallGrassSystem>();
			if (pxSystem == nullptr)
			{
				pxSystem = &xTerrain.AddComponent<ZM_TallGrassSystem>();
			}
			pxSystem->OnAwake();
			if (!pxSystem->HasDensityMap())
			{
				FailBM("tall-grass density map did not load after manual OnAwake");
				return false;
			}
			pxSystem->SetRngSeedForTests(0xABCull);
			// A deliberately weak, low wild enemy so the L5 placeholder player reliably
			// WINS (Win) and reliably out-speeds it for a guaranteed flee (Run). The Catch
			// test forces a DISTINCT species (KINDLET) so a successful catch provably
			// changes both the party count and the caught-set.
			if (g_eBMMode == MenuTestMode::Catch)
			{
				pxSystem->ForceEncounterOnNextTransitionForTests(eBM_CATCH_ENEMY_SPECIES, uBM_CATCH_ENEMY_LEVEL);
			}
			else
			{
				pxSystem->ForceEncounterOnNextTransitionForTests(eBM_ENEMY_SPECIES, uBM_ENEMY_LEVEL);
			}

			// Data-driven direction pick from the SAME density map the system reads.
			ZM_TerrainGrass* pxGrass = xTerrain.TryGetComponent<ZM_TerrainGrass>();
			if (pxGrass == nullptr || !pxGrass->HasCPUMap())
			{
				FailBM("terrain density map is not available for direction sampling");
				return false;
			}
			const WalkChoice xChoice = ChooseWalkDirection(
				xPlayer.m_xPosition.x, xPlayer.m_xPosition.z, pxGrass->GetDensityMap());
			if (!xChoice.m_bFound)
			{
				FailBM("no cardinal direction from the spawn reaches a grass tile");
				return false;
			}
			g_eBMWalkKey = xChoice.m_eKey;

			// Entry capture: the deterministic overworld grass-blade count (restored on resume).
			g_uBMEntryGrassBlades = g_xEngine.Grass().GetGeneratedInstanceCount();

			// SC3 exp-persist baseline: capture the persistent lead's exp+level BEFORE
			// the encounter, so the resume can prove the win wrote progression back
			// through the DontDestroyOnLoad GameState. Skip-safe: the assertion is
			// guarded on this capture, so an absent manager leaves the check vacuous.
			{
				ZM_GameState* pxGameState = nullptr;
				if (ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr)
				{
					g_uBMExpBefore         = pxGameState->m_xParty.Lead().m_uCurrentExp;
					g_uBMLevelBefore       = pxGameState->m_xParty.Lead().m_uLevel;
					g_bBMExpCapturedBefore = true;

					// SC4 catch baseline: the persistent party size + whether the DISTINCT
					// wild species is ALREADY caught (it must NOT be, so a successful catch
					// provably changes BOTH). Only asserted in the Catch test.
					g_uBMPartyCountBefore    = pxGameState->m_xParty.Count();
					g_bBMCatchSpeciesBefore  = pxGameState->IsCaught(eBM_CATCH_ENEMY_SPECIES);
					g_bBMPartyCapturedBefore = true;
				}
			}

			g_eBMPhase = BMPhase::Baseline;
			g_iBMPhaseFrames = 0;
			return true;
		}

		case BMPhase::Baseline:
			// Let the system's OnUpdate establish its baseline tile (the first update
			// after OnAwake only records the tile; it never transitions).
			if (g_iBMPhaseFrames < iBM_BASELINE_FRAMES)
			{
				return true;
			}
			Zenith_InputSimulator::SetKeyHeld(g_eBMWalkKey, true);
			g_eBMPhase = BMPhase::Walk;
			g_iBMPhaseFrames = 0;
			return true;

		case BMPhase::Walk:
		{
			// Re-resolve EVERY frame: the pool relocates components on swap-and-pop.
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailBM("the ZM_BattleTransition singleton stopped resolving mid-walk");
				return false;
			}
			if (pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
			{
				// The encounter latched and the machine started; release the key now.
				Zenith_InputSimulator::SetKeyHeld(g_eBMWalkKey, false);
				g_eBMPhase = BMPhase::AwaitInBattle;
				g_iBMPhaseFrames = 0;
				return true;
			}
			if (g_iBMPhaseFrames > iBM_WALK_DEADLINE)
			{
				FailBM("walk deadline: never left IDLE");
				return false;
			}
			return true;
		}

		case BMPhase::AwaitInBattle:
		{
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailBM("the ZM_BattleTransition singleton stopped resolving before IN_BATTLE");
				return false;
			}

			// Start driving the menu the moment the Battle scene may exist.
			DriveAndCaptureMenu();

			if (!g_bBMInBattleCaptured
				&& pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
			{
				// The drift baseline: the parked overworld body must not move while the
				// overworld is paused.
				g_xBMParkedPos        = pxTransition->GetParkedPlayerPosition();
				g_bBMInBattleCaptured = true;
				g_eBMPhase = BMPhase::AwaitResume;
				g_iBMPhaseFrames = 0;
				return true;
			}

			if (g_iBMPhaseFrames > iBM_INBATTLE_DEADLINE)
			{
				FailBM("never reached IN_BATTLE before deadline");
				return false;
			}
			return true;
		}

		case BMPhase::AwaitResume:
		{
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailBM("the ZM_BattleTransition singleton stopped resolving during resume");
				return false;
			}

			// Keep driving the menu + latching the outcome until the resume unloads the
			// Battle scene: this is where the turns actually resolve (the player MUST
			// keep submitting actions -- the director no longer auto-submits).
			DriveAndCaptureMenu();

			if (!g_bBMResumeReached)
			{
				// The DIRECTOR ends the battle: we never call RequestBattleEnd, so
				// completed==1 here proves the player-driven battle resolved and the
				// component requested the exit itself.
				if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
					&& pxTransition->GetCompletedBattleCount() == 1u)
				{
					g_bBMResumeReached = true;
					g_iBMResumeSettle = 0;
					return true;
				}
				if (g_iBMPhaseFrames > iBM_RESUME_DEADLINE)
				{
					FailBM("resume deadline: the player-driven battle never ended "
						"(never returned to IDLE with completed==1)");
					return false;
				}
				return true;
			}

			// Let the resume settle before sampling the exact-restore invariants.
			++g_iBMResumeSettle;
			if (g_iBMResumeSettle < iBM_RESUME_SETTLE_FRAMES)
			{
				return true;
			}

			g_iBMBuildIndexAfter = g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
			g_bBMBattleSceneUnloaded = !g_xEngine.Scenes().FindLoadedSceneByPath(
				std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT).IsValid();
			g_uBMGrassAfter = g_xEngine.Grass().GetGeneratedInstanceCount();

			PlayerView xPlayer2;
			if (FindActivePlayer(xPlayer2))
			{
				g_xBMResumePlayerPos       = xPlayer2.m_xPosition;
				g_bBMPlayerMovementEnabled = xPlayer2.m_pxController->IsMovementEnabled();
				g_bBMPlayerResolved        = true;
			}

			g_uBMCompletedAfter = pxTransition->GetCompletedBattleCount();
			g_uBMAbortedAfter   = pxTransition->GetAbortedTransitionCount();

			// SC3: re-resolve the persistent GameState FRESH after the resume settles and
			// read the lead's exp+level, so Verify can prove the win's exp write-back
			// persisted across the additive battle + unload (only asserted in the Win test).
			{
				ZM_GameState* pxGameStateAfter = nullptr;
				if (ZM_GameStateManager::TryGetGameState(pxGameStateAfter) && pxGameStateAfter != nullptr)
				{
					g_uBMExpAfter         = pxGameStateAfter->m_xParty.Lead().m_uCurrentExp;
					g_uBMLevelAfter       = pxGameStateAfter->m_xParty.Lead().m_uLevel;
					g_bBMExpCapturedAfter = true;

					// SC4: the caught monster joined the party + marked the caught-set. Read
					// FRESH after the resume settles so Verify can prove the catch write-back
					// persisted across the additive battle + unload (only asserted in Catch).
					g_uBMPartyCountAfter    = pxGameStateAfter->m_xParty.Count();
					g_bBMCatchSpeciesAfter  = pxGameStateAfter->IsCaught(eBM_CATCH_ENEMY_SPECIES);
					g_bBMPartyCapturedAfter = true;
				}
			}

			g_eBMPhase = BMPhase::Done;
			return false;
		}

		case BMPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMBattleMenu()
	{
		bool bPassed = true;
		const bool bIsWin = (g_eBMMode == MenuTestMode::Win);
		const char* szTag =
			g_eBMMode == MenuTestMode::Win   ? "ZM_BattleMenuWin"
		  : g_eBMMode == MenuTestMode::Catch ? "ZM_BattleMenuCatch"
		  :                                    "ZM_BattleMenuRun";

		if (g_bBMActive)
		{
			const float fDrift = glm::length(g_xBMResumePlayerPos - g_xBMParkedPos);

			// One localisable line dumping every captured value.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[%s] captured: failed=%s (%s) directorSeen=%s completedAfter=%u (want 1) "
				"abortedAfter=%u (want 0) buildAfter=%d (want 2) battleUnloaded=%s "
				"playerResolved=%s movementEnabled=%s drift=%f (want <0.05) entryGrass=%u "
				"grassAfter=%u (want ==entry) winnerCaptured=%s winner=%d (Win wants PLAYER=%d, "
				"Run wants COUNT=%d) fleeSeen=%s minHudFill=%f (Win wants 0) hudRectCount=%u",
				szTag,
				g_bBMFailed ? "true" : "false", g_szBMFailure,
				g_bBMDirectorSeen ? "true" : "false",
				g_uBMCompletedAfter,
				g_uBMAbortedAfter,
				g_iBMBuildIndexAfter,
				g_bBMBattleSceneUnloaded ? "true" : "false",
				g_bBMPlayerResolved ? "true" : "false",
				g_bBMPlayerMovementEnabled ? "true" : "false",
				(double)fDrift,
				g_uBMEntryGrassBlades, g_uBMGrassAfter,
				g_bBMWinnerCaptured ? "true" : "false",
				(int)g_eBMWinner, (int)ZM_SIDE_PLAYER, (int)ZM_SIDE_COUNT,
				g_bBMFleeSeen ? "true" : "false",
				(double)g_fBMMinHudRectFill, g_uBMHudRectCount);

			if (g_bBMFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[%s] %s", szTag, g_szBMFailure);
				bPassed = false;
			}

			// --- the player-driven battle was observed at all ---
			if (!g_bBMDirectorSeen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] the BattleDirector entity never resolved -- the Battle scene / director "
					"was never observed", szTag);
				bPassed = false;
			}

			// --- the DIRECTOR ended it (a menu-driven action reached the core) ---
			if (g_uBMCompletedAfter != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] completed battle count was %u, expected exactly 1 (the test never calls "
					"RequestBattleEnd -- the player-driven menu must resolve the battle)",
					szTag, g_uBMCompletedAfter);
				bPassed = false;
			}
			if (g_uBMAbortedAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] the round trip recorded %u aborts, expected 0", szTag, g_uBMAbortedAfter);
				bPassed = false;
			}

			// --- EXACT resume (the item-3 invariants still hold under player input) ---
			if (g_iBMBuildIndexAfter != 2)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] active scene build index after resume was %d, expected 2 (Dawnmere)",
					szTag, g_iBMBuildIndexAfter);
				bPassed = false;
			}
			if (!g_bBMBattleSceneUnloaded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] the Battle scene was still loaded after resume", szTag);
				bPassed = false;
			}
			if (!g_bBMPlayerResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] the overworld player did not resolve after resume", szTag);
				bPassed = false;
			}
			if (!g_bBMPlayerMovementEnabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] player movement was not re-enabled after resume", szTag);
				bPassed = false;
			}
			if (fDrift >= 0.05f)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] the resumed player drifted %f m from its parked position, expected < 0.05",
					szTag, (double)fDrift);
				bPassed = false;
			}
			if (g_uBMEntryGrassBlades == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] the overworld had 0 grass blades before the encounter -- the grass-restore "
					"invariant is vacuous", szTag);
				bPassed = false;
			}
			if (g_uBMGrassAfter != g_uBMEntryGrassBlades)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] resumed grass blade count was %u, expected %u (resume must restore the same "
					"deterministic blade count)", szTag, g_uBMGrassAfter, g_uBMEntryGrassBlades);
				bPassed = false;
			}

			// --- the outcome the player's input drove ---
			if (bIsWin)
			{
				// ENTER-spam picked Fight->move0 each turn; the L5 player KO'd the L2 enemy.
				if (!g_bBMWinnerCaptured)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWin] the core never reached OVER while the Battle scene was "
						"loaded -- no winner was captured");
					bPassed = false;
				}
				else if (g_eBMWinner != ZM_SIDE_PLAYER)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWin] the battle winner was side %d, expected PLAYER %d (the "
						"Fight->move0 drive must KO the weak wild enemy)",
						(int)g_eBMWinner, (int)ZM_SIDE_PLAYER);
					bPassed = false;
				}
				// At least one HP bar reached 0 (a side fainted, so the HUD tracked HP
				// to empty). The min is taken over every HUD UIRect (the two HP bars).
				if (g_uBMHudRectCount < 2u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWin] the HUD exposed %u UIRect elements, expected >= 2 (the two "
						"HP bars)", g_uBMHudRectCount);
					bPassed = false;
				}
				if (g_fBMMinHudRectFill > 1e-4f)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWin] no HP bar reached 0 (min fill was %f) -- a side fainted, so "
						"the HUD must have tracked one HP bar to empty", (double)g_fBMMinHudRectFill);
					bPassed = false;
				}

				// --- SC3: the win awarded exp and wrote it back to the persistent lead ---
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuWin] exp-persist: capturedBefore=%s expBefore=%u levelBefore=%u "
					"capturedAfter=%s expAfter=%u levelAfter=%u",
					g_bBMExpCapturedBefore ? "true" : "false", g_uBMExpBefore, g_uBMLevelBefore,
					g_bBMExpCapturedAfter ? "true" : "false", g_uBMExpAfter, g_uBMLevelAfter);
				// Guarded on the pre-battle capture: skip-safe when the manager was absent.
				if (g_bBMExpCapturedBefore && !g_bBMExpCapturedAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWin] the persistent GameState did not resolve after resume -- the "
						"exp write-back could not be observed");
					bPassed = false;
				}
				else if (g_bBMExpCapturedBefore)
				{
					// Exp STRICTLY rose (the win awarded exp); the level never regresses. We
					// do NOT assert a specific level-up -- the weak enemy may not cross the
					// curve, so "exp rose" is the invariant.
					if (g_uBMExpAfter <= g_uBMExpBefore)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuWin] the persistent lead's exp did not rise across the win "
							"(before=%u after=%u) -- a win must award and persist exp to the lead",
							g_uBMExpBefore, g_uBMExpAfter);
						bPassed = false;
					}
					if (g_uBMLevelAfter < g_uBMLevelBefore)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuWin] the persistent lead's level fell across the win "
							"(before=%u after=%u)", g_uBMLevelBefore, g_uBMLevelAfter);
						bPassed = false;
					}
				}
			}
			else if (g_eBMMode == MenuTestMode::Catch)
			{
				// The menu-driven Catch threw the GUARANTEED ZM_ITEM_PRIMEORB: the wild
				// monster was caught, so the core ends with the PLAYER as winner...
				if (!g_bBMWinnerCaptured)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuCatch] the core never reached OVER while the Battle scene was "
						"loaded -- no winner was captured");
					bPassed = false;
				}
				else if (g_eBMWinner != ZM_SIDE_PLAYER)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuCatch] the battle winner was side %d, expected PLAYER %d (a "
						"successful catch ends the battle for the player)",
						(int)g_eBMWinner, (int)ZM_SIDE_PLAYER);
					bPassed = false;
				}

				// ...and the caught monster joined the persistent party + marked the caught-set.
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuCatch] catch-persist: capturedBefore=%s partyBefore=%u "
					"speciesCaughtBefore=%s capturedAfter=%s partyAfter=%u speciesCaughtAfter=%s",
					g_bBMPartyCapturedBefore ? "true" : "false", g_uBMPartyCountBefore,
					g_bBMCatchSpeciesBefore ? "true" : "false",
					g_bBMPartyCapturedAfter ? "true" : "false", g_uBMPartyCountAfter,
					g_bBMCatchSpeciesAfter ? "true" : "false");
				// Guarded on the pre-battle capture: skip-safe when the manager was absent.
				if (g_bBMPartyCapturedBefore && !g_bBMPartyCapturedAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuCatch] the persistent GameState did not resolve after resume -- the "
						"catch write-back could not be observed");
					bPassed = false;
				}
				else if (g_bBMPartyCapturedBefore)
				{
					// The DISTINCT wild species must have been UNMARKED before, else the
					// caught-set change would be vacuous.
					if (g_bBMCatchSpeciesBefore)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuCatch] the wild species was ALREADY caught before the encounter "
							"-- the caught-set change is vacuous");
						bPassed = false;
					}
					// The caught monster joined the party: count grew by exactly 1 (1 -> 2).
					if (g_uBMPartyCountAfter != g_uBMPartyCountBefore + 1u)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuCatch] persistent party count was %u before and %u after the "
							"catch, expected +1 (the caught monster must join the party)",
							g_uBMPartyCountBefore, g_uBMPartyCountAfter);
						bPassed = false;
					}
					// The caught species is now marked in the persistent caught-set.
					if (!g_bBMCatchSpeciesAfter)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuCatch] the caught species was not marked in the persistent "
							"caught-set after the catch");
						bPassed = false;
					}
				}
			}
			else
			{
				// The menu-driven Run reached the engine as a successful flee: a FLEE
				// event, and NO winner (a flee ends the battle as a draw).
				if (!g_bBMFleeSeen)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuRun] no ZM_BATTLE_EVENT_FLEE appeared in the engine stream -- the "
						"menu-driven Run never reached the core as a successful flee");
					bPassed = false;
				}
				if (!g_bBMWinnerCaptured)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuRun] the core never reached OVER while the Battle scene was "
						"loaded -- no winner was captured");
					bPassed = false;
				}
				else if (g_eBMWinner != ZM_SIDE_COUNT)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuRun] the battle winner was side %d, expected COUNT %d (a flee ends "
						"with no winner)", (int)g_eBMWinner, (int)ZM_SIDE_COUNT);
					bPassed = false;
				}
			}
		}

		// Always tear down, in order (all guarded), even on a terminal failure:
		// release the key, drop the fixed timestep, clear the instant-battles flag,
		// restore the gameplay catch ball (only the Catch test changed it, but the
		// restore is an unconditional skip-safe no-op otherwise), clear the transition's
		// ownerless statics, force-unload any lingering Battle scene, restore FrontEnd,
		// then wipe input.
		Zenith_InputSimulator::SetKeyHeld(g_eBMWalkKey, false);
		Zenith_InputSimulator::ClearFixedDt();
		ZM_SetInstantBattlesForTests(false);
		ZM_SetCatchBallForTests(ZM_ITEM_CATCHORB);
		ZM_BattleTransition::ResetRuntimeStateForTests();
		Zenith_Scene xBattle = g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT);
		if (xBattle.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(xBattle);
		}
		if (g_bBMActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bBMActive = false;

		return bPassed || !g_bBMPrereqsPresent;
	}
}

static const Zenith_AutomatedTest g_xZMBattleMenuWinTest = {
	"ZM_BattleMenuWin_Test",
	&Setup_ZMBattleMenuWin,
	&Step_ZMBattleMenu,
	&Verify_ZMBattleMenu,
	/* maxFrames */ 2200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleMenuWinTest);

static const Zenith_AutomatedTest g_xZMBattleMenuRunTest = {
	"ZM_BattleMenuRun_Test",
	&Setup_ZMBattleMenuRun,
	&Step_ZMBattleMenu,
	&Verify_ZMBattleMenu,
	/* maxFrames */ 2200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleMenuRunTest);

static const Zenith_AutomatedTest g_xZMBattleMenuCatchTest = {
	"ZM_BattleMenuCatch_Test",
	&Setup_ZMBattleMenuCatch,
	&Step_ZMBattleMenu,
	&Verify_ZMBattleMenu,
	/* maxFrames */ 2200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleMenuCatchTest);

#endif // ZENITH_INPUT_SIMULATOR
