#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"   // creature-model mesh count (failure-message context only)
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIRect.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleArena.h"               // the arena platforms the creature pixels are measured against
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
#include "Zenithmon/Tests/ZM_GrassBaseline.h"

#ifdef ZENITH_TOOLS
#include "Core/Zenith_EditorQuery.h"
#endif

#include "Core/Zenith_TestTGA.h"                                 // the engine-written BGRA swapchain dump reader

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_BattleMenu -- the windowed gate for the player-driven battle menu
// on ZM_UI_BattleHUD (owned by ZM_BattleDirector). FOUR tests, all
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
//   * ZM_BattleMenuWhiteout_Test (S5 item-5 SC5) -- forces a HIGH-LEVEL L60 enemy and
//     runs the SAME ENTER-spam drive, so the L5 lead LOSES; the write-back latches
//     m_bPendingWhiteout and the manager consumes it -> HealAllFull + a second warp to
//     Dawnmere/TownCenter (build 2). A pre-damaged (1 HP) lead heals to full and the
//     latch clears; the exact-restore drift/grass locks are replaced by whiteout ones.
//
// All CLONE ZM_AutoTests_BattleHUD.cpp's shipped phase machine (its Dawnmere
// runtime-ready gate, the forced wild encounter, fixed-dt 1/30, zm_instant_battles
// on in Setup / off in teardown, the RequestSkip guard order, and the director-
// ended + exact-resume invariants), differing only in the input DRIVE (menu-
// aware) and the win/flee/catch/whiteout assertions. The shared file-local helpers
// are internal linkage in the shipped TU and cannot be linked across TUs, so they are
// re-declared verbatim here; the tests share one phase machine that branches
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
				// A Null (headless) build never APPLIES grass: the blades are GPU-only
				// content the backend deliberately does not author (see
				// Zenith/Null/CLAUDE.md). The component still exists and has reached
				// its terminal inert state, which is what this gate is really asking:
				// "has the overworld finished coming up?". The windowed assertion is
				// unchanged -- there, applied-ness is still required.
				bReady = bReady || xGrass.IsGrassApplied() || Zenith_IsNullRenderer();
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

	// Absolute Build/artifacts/zenithmon/visual_audit dir derived from
	// GAME_ASSETS_DIR (<repo>/Games/Zenithmon/Assets/ -> up three -> <repo>), so
	// the framebuffer evidence path is independent of the process working directory.
	std::filesystem::path BattleMenuVisualAuditDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "visual_audit";
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

	// The persistent ZM_GameStateManager singleton, resolved FRESH each frame (same
	// swap-and-pop hazard as the battle transition). The Whiteout test reads its warp
	// state + issued-load count to prove the loss-driven whiteout warp ran.
	ZM_GameStateManager* ResolveSingletonGameStateManager()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_GameStateManager>()
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

	// Four tests, one machine. Set by each test's thin Setup; the drive + the
	// win/flee/catch/whiteout assertions branch on it.
	enum class MenuTestMode { Win, Run, Catch, Whiteout };

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

	// The Whiteout test forces a HIGH-LEVEL enemy so the L5 lead reliably LOSES: the L60
	// KINDLET one-shots the L5 lead, so the ENTER-spam Fight->move0 drive ends with the
	// ENEMY as winner -> the write-back latches m_bPendingWhiteout -> the manager whiteout-
	// warps. The single knob: if an L5 lead ever survives to win, raise this toward 100.
	constexpr ZM_SPECIES_ID eBM_WHITEOUT_ENEMY_SPECIES = ZM_SPECIES_KINDLET;
	constexpr u_int         uBM_WHITEOUT_ENEMY_LEVEL   = 60u;

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
		AwaitWhiteoutWarp,   // Whiteout only: the manager consumes the loss latch + warps to Dawnmere/TownCenter
		Done,
	};

	constexpr int iBM_READY_DEADLINE       = 420;   // Dawnmere first-load ready window (round-trip parity)
	constexpr int iBM_BASELINE_FRAMES      = 4;     // let OnUpdate record its baseline tile before we drive
	constexpr int iBM_WALK_DEADLINE        = 460;   // ample budget to reach a <= 24 m grass tile at walk speed
	constexpr int iBM_INBATTLE_DEADLINE    = 600;   // fade-out + additive load + arena build + fade-in
	constexpr int iBM_RESUME_DEADLINE      = 600;   // player drives the menu to resolution + fade + unload + regrow
	constexpr int iBM_RESUME_SETTLE_FRAMES = 8;     // let the resume settle before sampling the exact state
	constexpr int iBM_WHITEOUT_DEADLINE    = 700;   // Whiteout only: fade-out + Dawnmere reload + spawn/camera + fade-in
	constexpr int iBM_RUN_VISUAL_DWELL_FRAMES = 90; // Run only: leave ACTION_ROOT visible for external pixel capture
	constexpr int iBM_RUN_VISUAL_CAPTURE_FRAME = iBM_RUN_VISUAL_DWELL_FRAMES / 2;
	constexpr const char* szBM_RUN_MENU_PANEL_NAME = "BattleHUD_MenuPanel";
	constexpr const char* szBM_RUN_FIGHT_NAME      = "BattleHUD_ActionFight";
	constexpr const char* szBM_RUN_CATCH_NAME      = "BattleHUD_ActionCatch";
	constexpr const char* szBM_RUN_RUN_NAME        = "BattleHUD_ActionRun";
	constexpr const char* szBM_RUN_ENEMY_HPBAR_NAME = "BattleHUD_EnemyHPBar";
	constexpr const char* szBM_RUN_LOG_NAME         = "BattleHUD_Log";
	constexpr u_int uBM_RUN_ACTION_BUTTON_COUNT     = 3u;

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
	int            g_iBMRunVisualDwellFrames = 0;
	bool           g_bBMRunVisualShotRequested = false;
	bool           g_bBMRunRootVisualsValid = false;
	std::string    g_strBMRunVisualShotPath;

	// ---- Entry captures (before the encounter) ----
	u_int          g_uBMEntryGrassBlades  = 0u;   // grass-restore baseline

	// ---- IN_BATTLE captures ----
	Zenith_Maths::Vector3 g_xBMParkedPos  = Zenith_Maths::Vector3(0.0f);  // THE drift baseline
	u_int          g_uBMGrassAtPark = 0u;   // THE grass baseline

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

	// ---- Whiteout captures (SC5: a LOSS latches m_bPendingWhiteout; the manager consumes
	// it -> HealAllFull + warp to Dawnmere/TownCenter; only asserted in the Whiteout test) ----
	bool    g_bBMWhiteoutMgrCapturedBefore = false;  // the manager resolved before the encounter
	u_int   g_uBMWhiteoutMaxHP             = 0u;     // the lead's full HP, captured BEFORE we damage it to 1
	bool    g_bBMWhiteoutPreDamaged        = false;  // the lead was pre-damaged to 1 (so the heal is observable)
	u_int   g_uBMWhiteoutIssuedLoadBefore  = 0u;     // manager issued-load count pre-encounter (want 0; warp adds 1)
	bool    g_bBMWhiteoutWarpSettled       = false;  // the whiteout warp reached IDLE on Dawnmere with the load issued
	u_int   g_uBMWhiteoutIssuedLoadAfter   = 0u;     // manager issued-load count post-warp
	u_int   g_uBMWhiteoutWarpStateAfter    = (u_int)ZM_WARP_TRANSITION_IDLE;   // manager warp state post-warp
	bool    g_bBMWhiteoutGameStateAfter    = false;  // a GameState resolved after the warp settled
	bool    g_bBMWhiteoutPendingAfter      = true;   // m_bPendingWhiteout post-warp (want false = consumed)
	u_int   g_uBMWhiteoutLeadHpAfter       = 0u;     // persistent lead curHP post-warp (want == max)
	u_int   g_uBMWhiteoutLeadMaxHpAfter    = 0u;     // persistent lead max HP post-warp

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

	bool BattleMenuRunRootVisualsMatch(Zenith_UIComponent& xUI)
	{
		Zenith_UI::Zenith_UIRect* pxPanel =
			xUI.FindElement<Zenith_UI::Zenith_UIRect>(szBM_RUN_MENU_PANEL_NAME);
		Zenith_UI::Zenith_UIButton* pxFight =
			xUI.FindElement<Zenith_UI::Zenith_UIButton>(szBM_RUN_FIGHT_NAME);
		Zenith_UI::Zenith_UIButton* pxCatch =
			xUI.FindElement<Zenith_UI::Zenith_UIButton>(szBM_RUN_CATCH_NAME);
		Zenith_UI::Zenith_UIButton* pxRun =
			xUI.FindElement<Zenith_UI::Zenith_UIButton>(szBM_RUN_RUN_NAME);

		return pxPanel != nullptr && pxPanel->IsVisible()
			&& pxFight != nullptr && pxFight->IsVisible() && pxFight->GetText() == "Fight"
			&& pxCatch != nullptr && pxCatch->IsVisible() && pxCatch->GetText() == "Catch"
			&& pxRun != nullptr && pxRun->IsVisible() && pxRun->GetText() == "Run";
	}

	// =========================================================================
	// THE ACTION_ROOT PIXEL ASSERTIONS (ZM-D-170) -- audit finding 2 of ZM-D-168,
	// "creature models and the battle HUD are UNVERIFIED by pixels".
	//
	// The Run test ALREADY dwelt 90 frames in ACTION_ROOT and ALREADY wrote a real
	// swapchain TGA. It then asserted only that the FILE EXISTED, plus UI element
	// visibility and button text -- i.e. evidence was produced and never read,
	// which reads as coverage and is not. Every assertion above this block samples
	// an INPUT to rendering; these are the only ones that can tell "submitted" from
	// "drawn", which is the distinction ZM-D-168's standing rule turns on.
	//
	// ★ EVERY THRESHOLD BELOW IS MEASURED OFF THE BYTES A REAL RUN WROTE
	// (Build/artifacts/zenithmon/visual_audit/battle_menu_run_root.tga, 1280x720,
	// tools build) and never predicted from a submitted colour. Predicting instead
	// of measuring is exactly how ZM-D-169's two "low blue" scans reported zero
	// matches across 539 frames of a marker that was rendering perfectly.
	//
	// ★ AND EVERY REGION IS RESTRICTED TO THE TOOLS VIEWPORT RECT. The dump is the
	// FULL SWAPCHAIN, so in a tools build it carries the ImGui editor chrome around
	// the game. On THIS capture a frame-wide "bright green" scan matches the
	// Console panel's green tick marks at x[1089,1142] y[436,447] as well as the
	// HP bar -- a frame-wide scan here would be measuring ImGui, not the HUD.
	//
	// ★ THE CANVAS -> SWAPCHAIN MAPPING. UI elements are laid out in CANVAS space
	// (Zenith_UICanvas::GetSize() == the window size) and the editor composites the
	// whole game render into the viewport rect, so
	//     pixel = viewportPos + canvas * (viewportSize / canvasSize)
	// -- the exact inverse of Zenith_UIElement::GetTransformedMousePosition's
	// window->canvas remap. Verified on the capture: the enemy HP bar's canvas span
	// x[40,280] predicts pixels x[264,408] and the measured green run is x[264,407].
	// =========================================================================

	// A patch radius of 4 gives a 9x9 sample; the rendered Fernfawn body measures
	// ~25x27 px in a 1280x720 dump, so the patch sits well inside it. The platform
	// slabs are far larger, hence the wider radius there.
	constexpr u_int uBM_PIX_BODY_RADIUS     = 4u;
	constexpr u_int uBM_PIX_PLATFORM_RADIUS = 6u;
	// Metres above the creature entity's ORIGIN to sample. The origin itself
	// projects onto the legs/ground: measured, the body centre sits ~0.35 m up, the
	// sampled colour moves by <= 0.15 across +/-0.15 m of jitter (a plateau, not a
	// knife edge), and 0.70 m clears the model entirely.
	constexpr float fBM_PIX_BODY_LIFT       = 0.35f;
	// Metres to either side at the SAME height -- the local BACKGROUND controls.
	// The creature is ~0.5 m wide, so 1.0 m is four half-widths clear of it.
	constexpr float fBM_PIX_SIDE_OFFSET     = 1.0f;
	// Onto the slab's top face (the platform is a unit cube scaled y=0.4).
	constexpr float fBM_PIX_PLATFORM_LIFT   = 0.2f;

	// ---- creature thresholds (all separations are RGB euclidean in [0,1]) ----
	// ★ ALL THREE ARE CENTRED BETWEEN A MEASURED PASS STATE AND A MEASURED FAIL
	// STATE, not merely set below whatever the passing run happened to produce. The
	// fail state is the mutation that keeps both creature ENTITIES and drops only
	// their Zenith_ModelComponent model, i.e. "placed but nothing to draw".
	//
	// ★ AND ALL THREE ARE LOAD-BEARING, which the mutation is what proved: on the
	// PLAYER side the body-vs-side arm still PASSED with no model (0.834 / 0.927),
	// because that projected point falls on pale stone rather than on the local
	// background. One arm alone would have let half the defect through.
	//
	// Body vs its own local background 1 m to either side.
	// PASS  0.191 / 0.234 (player), 0.219 / 0.241 (enemy) -- in-batch; standalone
	//       reads 0.190 / 0.232 and 0.222 / 0.243, so batch drift is <= 0.003.
	// FAIL  0.068 / 0.001 (enemy, model-less).
	constexpr float fBM_PIX_MIN_BODY_VS_SIDE     = 0.12f;
	// The two platforms carry the same species, so two REAL renders read alike --
	// not identically, because the two sides are lit differently.
	// PASS  0.140 in-batch / 0.141 standalone.   FAIL  0.851 (model-less).
	constexpr float fBM_PIX_MAX_BODY_VS_BODY     = 0.30f;
	// Body vs the slab under it. Written as a sample-placement guard -- "the patch
	// has not slid onto the platform" -- and it earned its keep: it is the arm that
	// caught the PLAYER side of the model-less mutation.
	//
	// RE-DERIVED 2026-08-09 against ZM-D-171 physically-grounded lighting, which is
	// what the pre-ZM-D-171 numbers below were measured before. BOTH bands compressed
	// because the arena slab is no longer the "pale stone" this arm was written
	// against: it now reads a mid-tone blue-grey (0.389, 0.461, 0.567) under
	// sky+ground ambient, much closer in luminance to a green Fernfawn
	// (0.338, 0.683, 0.503) than the old bright slab was. The creature is still
	// plainly on the patch -- its green channel is 0.68 against the slab's 0.46, and
	// the two body-vs-side arms actually read HIGHER than before (0.365/0.533 player,
	// 0.514/0.321 enemy vs 0.191/0.234 and 0.219/0.241) -- so this is a look change,
	// not a render gap. Derived ZM-D-171's way: re-run for PASS, then re-run the
	// mutation for FAIL (the model-less fall-through was reproduced by pointing the
	// body patch at the platform NDC, which is the same geometric state it produces).
	// PASS  0.236 (player) / 0.229 (enemy).   FAIL  0.041 / 0.038 (patch on the slab).
	//   pre-ZM-D-171, for the record: PASS 0.918 / 1.052, FAIL 0.007.
	// 0.12 sits ~3x above the fail band and ~half the pass band.
	constexpr float fBM_PIX_MIN_BODY_VS_PLATFORM = 0.12f;

	// ---- HUD thresholds ----
	// ★ Same discipline as the creature block: each band was measured with the HUD
	// drawing and again with it suppressed (the mutation hides BattleHUD_Log +
	// BattleHUD_EnemyHPBar at reveal, and separately hides the three root buttons).
	//
	// Enemy HP bar, over its left 40% only, so the clause survives any fill >= 0.4
	// (it is 1.0 at capture -- the Run drive dwells before submitting anything).
	// PASS  (0.496, 0.924, 0.590): G-R +0.428, G-B +0.334.
	// FAIL  (0.742, 0.749, 0.754): G-R +0.007, G-B -0.005.
	// ★ NOTE WHICH ARM ACTUALLY DISCRIMINATES: the suppressed bar reads the pale sky
	// behind it at green 0.749, which CLEARS the 0.60 level floor. The two CHROMA
	// arms are what fire (28x margin). The level floor is kept as a sanity bound and
	// is deliberately NOT raised above 0.749 -- that would be fitting the threshold
	// to "the thing behind the bar happens to be sky", which is not a property of
	// the HUD at all.
	constexpr float fBM_PIX_HPBAR_SAMPLE_FRACTION = 0.4f;
	constexpr float fBM_PIX_MIN_HPBAR_GREEN       = 0.60f;
	constexpr float fBM_PIX_MIN_HPBAR_G_OVER_R    = 0.20f;
	constexpr float fBM_PIX_MIN_HPBAR_G_OVER_B    = 0.15f;
	// Action buttons against the panel interior strip they sit on. The strip is
	// derived from the panel's and the first button's OWN bounds, so no authored
	// layout number is respelled.
	// PASS  luminance 0.586-0.631, delta over the strip +0.310..+0.355.
	// FAIL  luminance 0.277-0.339, delta +0.000..+0.063.
	constexpr float fBM_PIX_MIN_BUTTON_LUM        = 0.45f;
	constexpr float fBM_PIX_MIN_BUTTON_OVER_PANEL = 0.15f;
	constexpr float fBM_PIX_PANEL_STRIP_INSET     = 12.0f;   // canvas px, off both panel edges
	constexpr float fBM_PIX_PANEL_STRIP_MARGIN    = 4.0f;    // canvas px, off the strip's top/bottom
	// Log glyphs. PASS 522 strict-white px in the log box against 0 in the same-sized
	// control box immediately above it; FAIL 0 against 0. The strict channel floor
	// matters -- the pale stone platform measures (228, 203, 199) and the sky
	// (184, 188, 195), and a looser "bright" filter accepts the stone.
	constexpr u_int uBM_PIX_GLYPH_MIN_CHANNEL     = 220u;
	constexpr u_int uBM_PIX_GLYPH_MAX_SPREAD      = 25u;
	constexpr u_int uBM_PIX_MIN_LOG_GLYPHS        = 100u;
	constexpr u_int uBM_PIX_MAX_CONTROL_GLYPHS    = 25u;
	// The log box is 900 canvas px wide and its right end runs under the menu panel;
	// clip it clear so no button pixel can be counted as a glyph.
	constexpr float fBM_PIX_LOG_PANEL_CLEARANCE   = 8.0f;

	struct BMPixCanvasRect
	{
		bool  m_bValid  = false;
		float m_fLeft   = 0.0f;
		float m_fTop    = 0.0f;
		float m_fRight  = 0.0f;
		float m_fBottom = 0.0f;
	};

	struct BMPixNdcPoint
	{
		bool  m_bValid = false;
		float m_fX     = 0.0f;
		float m_fY     = 0.0f;
	};

	struct BMPixRegionStats
	{
		Zenith_Maths::Vector3 m_xMean       = Zenith_Maths::Vector3(0.0f);
		float                 m_fLuminance  = 0.0f;
		u_int                 m_uGlyphPixels = 0u;
		u_int                 m_uSamples    = 0u;
	};

	// ---- latched ON the capture frame; every pixel read happens in Verify ----
	bool                  g_bBMPixLatched      = false;
	const char*           g_szBMPixLatchFail   = "the ACTION_ROOT pixel geometry was never latched";
	Zenith_Maths::Vector2 g_xBMPixViewportPos  = Zenith_Maths::Vector2(0.0f);
	Zenith_Maths::Vector2 g_xBMPixViewportSize = Zenith_Maths::Vector2(0.0f);
	Zenith_Maths::Vector2 g_xBMPixCanvasSize   = Zenith_Maths::Vector2(0.0f);
	BMPixCanvasRect       g_xBMPixEnemyHpBar;
	BMPixCanvasRect       g_xBMPixMenuPanel;
	BMPixCanvasRect       g_axBMPixActionButton[uBM_RUN_ACTION_BUTTON_COUNT];
	BMPixCanvasRect       g_xBMPixLogBox;
	BMPixNdcPoint         g_axBMPixBody[ZM_SIDE_COUNT];
	BMPixNdcPoint         g_axBMPixSideLeft[ZM_SIDE_COUNT];
	BMPixNdcPoint         g_axBMPixSideRight[ZM_SIDE_COUNT];
	BMPixNdcPoint         g_axBMPixPlatform[ZM_SIDE_COUNT];
	u_int                 g_auBMPixCreatureMeshes[ZM_SIDE_COUNT] = {};
	std::string           g_astrBMPixCreatureName[ZM_SIDE_COUNT];

	const char* BMPixSideName(u_int uSide)
	{
		return uSide == (u_int)ZM_SIDE_PLAYER ? "player" : "enemy";
	}

	void BMPixFailLatch(const char* szReason)
	{
		g_szBMPixLatchFail = szReason;
	}

	float BMPixCanvasToPixelX(float fCanvasX)
	{
		return g_xBMPixViewportPos.x + fCanvasX * (g_xBMPixViewportSize.x / g_xBMPixCanvasSize.x);
	}

	float BMPixCanvasToPixelY(float fCanvasY)
	{
		return g_xBMPixViewportPos.y + fCanvasY * (g_xBMPixViewportSize.y / g_xBMPixCanvasSize.y);
	}

	// Zenith_CameraComponent's Vulkan projection already flips Y, so NDC -1 is the
	// TOP of the displayed viewport (the ZM_NpcRenderedPalette_Test convention).
	float BMPixNdcToPixelX(float fNdcX)
	{
		return g_xBMPixViewportPos.x + (fNdcX * 0.5f + 0.5f) * g_xBMPixViewportSize.x;
	}

	float BMPixNdcToPixelY(float fNdcY)
	{
		return g_xBMPixViewportPos.y + (fNdcY * 0.5f + 0.5f) * g_xBMPixViewportSize.y;
	}

	float BMPixSeparation(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const Zenith_Maths::Vector3 xDelta = xA - xB;
		return std::sqrt(xDelta.x * xDelta.x + xDelta.y * xDelta.y + xDelta.z * xDelta.z);
	}

	bool BMPixMeasurePixelRect(const Zenith_TestTGAImage& xImage,
		float fLeft, float fTop, float fRight, float fBottom, BMPixRegionStats& xOut)
	{
		xOut = BMPixRegionStats{};
		if (!xImage.IsValid())
		{
			return false;
		}
		const int64_t iLeft   = static_cast<int64_t>(std::lround(fLeft));
		const int64_t iTop    = static_cast<int64_t>(std::lround(fTop));
		const int64_t iRight  = static_cast<int64_t>(std::lround(fRight));
		const int64_t iBottom = static_cast<int64_t>(std::lround(fBottom));
		if (iLeft < 0 || iTop < 0
			|| iRight > static_cast<int64_t>(xImage.m_uWidth)
			|| iBottom > static_cast<int64_t>(xImage.m_uHeight)
			|| iRight - iLeft < 2 || iBottom - iTop < 2)
		{
			return false;
		}

		uint64_t ulRed = 0u;
		uint64_t ulGreen = 0u;
		uint64_t ulBlue = 0u;
		uint64_t ulSamples = 0u;
		uint64_t ulGlyphs = 0u;
		for (int64_t iY = iTop; iY < iBottom; ++iY)
		{
			for (int64_t iX = iLeft; iX < iRight; ++iX)
			{
				const uint8_t* puBGRA = xImage.GetPixelBGRA(
					static_cast<uint32_t>(iX), static_cast<uint32_t>(iY));
				const u_int uBlue  = puBGRA[0];
				const u_int uGreen = puBGRA[1];
				const u_int uRed   = puBGRA[2];
				ulBlue  += uBlue;
				ulGreen += uGreen;
				ulRed   += uRed;
				++ulSamples;

				const u_int uMax = uRed > uGreen
					? (uRed > uBlue ? uRed : uBlue)
					: (uGreen > uBlue ? uGreen : uBlue);
				const u_int uMin = uRed < uGreen
					? (uRed < uBlue ? uRed : uBlue)
					: (uGreen < uBlue ? uGreen : uBlue);
				if (uMin >= uBM_PIX_GLYPH_MIN_CHANNEL
					&& (uMax - uMin) <= uBM_PIX_GLYPH_MAX_SPREAD)
				{
					++ulGlyphs;
				}
			}
		}
		if (ulSamples == 0u)
		{
			return false;
		}

		const float fNormalise = 1.0f / (255.0f * static_cast<float>(ulSamples));
		xOut.m_xMean = Zenith_Maths::Vector3(
			static_cast<float>(ulRed) * fNormalise,
			static_cast<float>(ulGreen) * fNormalise,
			static_cast<float>(ulBlue) * fNormalise);
		xOut.m_fLuminance = (xOut.m_xMean.x + xOut.m_xMean.y + xOut.m_xMean.z) / 3.0f;
		xOut.m_uGlyphPixels = static_cast<u_int>(ulGlyphs);
		xOut.m_uSamples = static_cast<u_int>(ulSamples);
		return true;
	}

	bool BMPixMeasureCanvasRect(const Zenith_TestTGAImage& xImage,
		float fLeft, float fTop, float fRight, float fBottom, BMPixRegionStats& xOut)
	{
		return BMPixMeasurePixelRect(xImage,
			BMPixCanvasToPixelX(fLeft), BMPixCanvasToPixelY(fTop),
			BMPixCanvasToPixelX(fRight), BMPixCanvasToPixelY(fBottom), xOut);
	}

	bool BMPixMeasurePatch(const Zenith_TestTGAImage& xImage,
		const BMPixNdcPoint& xPoint, u_int uRadius, Zenith_Maths::Vector3& xOut)
	{
		if (!xPoint.m_bValid)
		{
			return false;
		}
		const float fCentreX = BMPixNdcToPixelX(xPoint.m_fX);
		const float fCentreY = BMPixNdcToPixelY(xPoint.m_fY);
		const float fRadius = static_cast<float>(uRadius);
		BMPixRegionStats xStats;
		if (!BMPixMeasurePixelRect(xImage,
			fCentreX - fRadius, fCentreY - fRadius,
			fCentreX + fRadius + 1.0f, fCentreY + fRadius + 1.0f, xStats))
		{
			return false;
		}
		xOut = xStats.m_xMean;
		return true;
	}

	// ---- capture-frame latching. Nothing here reads a pixel: the TGA does not
	// exist until Zenith_Vulkan_Swapchain::EndFrame consumes the dump request, so
	// every screen-space geometry is captured now and evaluated in Verify. ----

	bool BMPixLatchViewport(Zenith_UIComponent& xUI)
	{
		g_xBMPixCanvasSize = xUI.GetCanvas().GetSize();
#ifdef ZENITH_TOOLS
		if (g_xEditorQuery.m_pfnGetViewportPos == nullptr
			|| g_xEditorQuery.m_pfnGetViewportSize == nullptr)
		{
			BMPixFailLatch("the tools viewport query seam is not installed");
			return false;
		}
		g_xBMPixViewportPos = g_xEditorQuery.m_pfnGetViewportPos();
		g_xBMPixViewportSize = g_xEditorQuery.m_pfnGetViewportSize();
#else
		// No editor chrome: the game owns the whole window, so canvas space IS
		// swapchain space and the mapping degenerates to the identity.
		g_xBMPixViewportPos = Zenith_Maths::Vector2(0.0f);
		g_xBMPixViewportSize = g_xBMPixCanvasSize;
#endif
		if (g_xBMPixCanvasSize.x <= 0.0f || g_xBMPixCanvasSize.y <= 0.0f
			|| g_xBMPixViewportSize.x < 320.0f || g_xBMPixViewportSize.y < 180.0f)
		{
			// Not an error yet -- the editor layout may not have settled. The caller
			// retries every remaining dwell frame before giving up.
			BMPixFailLatch("the viewport/canvas rect never reached a sampleable size");
			return false;
		}
		return true;
	}

	bool BMPixLatchElementRect(Zenith_UIComponent& xUI, const char* szName, BMPixCanvasRect& xOut)
	{
		xOut = BMPixCanvasRect{};
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(szName);
		if (pxElement == nullptr)
		{
			return false;
		}
		// ★ DELIBERATELY NOT GATED ON IsVisible(). The visible FLAG is an input to
		// rendering; gating the latch on it would abort before a single pixel was
		// read, and a hidden element would then red as "geometry could not be
		// latched" instead of as "this never reached the framebuffer". The flag is
		// already covered separately by BattleMenuRunRootVisualsMatch.
		//
		// The element's OWN computed canvas rect. Reading it here rather than
		// respelling ZM_ConfigureBattleHUD's anchor/offset/size numbers is deliberate:
		// two sites holding the same layout is how a "checked" geometry silently
		// drifts away from the drawn one.
		const Zenith_Maths::Vector4 xBounds = pxElement->GetScreenBounds();
		if (!(xBounds.z > xBounds.x) || !(xBounds.w > xBounds.y))
		{
			return false;
		}
		xOut.m_fLeft   = xBounds.x;
		xOut.m_fTop    = xBounds.y;
		xOut.m_fRight  = xBounds.z;
		xOut.m_fBottom = xBounds.w;
		xOut.m_bValid  = true;
		return true;
	}

	bool BMPixLatchHudRects(Zenith_UIComponent& xUI)
	{
		const char* const aszButtonName[uBM_RUN_ACTION_BUTTON_COUNT] =
		{
			szBM_RUN_FIGHT_NAME, szBM_RUN_CATCH_NAME, szBM_RUN_RUN_NAME
		};
		bool bAll = BMPixLatchElementRect(xUI, szBM_RUN_ENEMY_HPBAR_NAME, g_xBMPixEnemyHpBar);
		bAll = BMPixLatchElementRect(xUI, szBM_RUN_MENU_PANEL_NAME, g_xBMPixMenuPanel) && bAll;
		bAll = BMPixLatchElementRect(xUI, szBM_RUN_LOG_NAME, g_xBMPixLogBox) && bAll;
		for (u_int u = 0u; u < uBM_RUN_ACTION_BUTTON_COUNT; ++u)
		{
			bAll = BMPixLatchElementRect(xUI, aszButtonName[u], g_axBMPixActionButton[u]) && bAll;
		}
		if (!bAll)
		{
			BMPixFailLatch("a HUD element (enemy HP bar / log / menu panel / Fight / "
				"Catch / Run) is absent from the canvas or has no usable rect");
		}
		return bAll;
	}

	bool BMPixProject(const Zenith_Maths::Matrix4& xViewProjection,
		const Zenith_Maths::Vector3& xWorld, BMPixNdcPoint& xOut)
	{
		xOut = BMPixNdcPoint{};
		const Zenith_Maths::Vector4 xClip =
			xViewProjection * Zenith_Maths::Vector4(xWorld, 1.0f);
		if (!std::isfinite(xClip.x) || !std::isfinite(xClip.y)
			|| !std::isfinite(xClip.w) || xClip.w <= 1.0e-4f)
		{
			return false;
		}
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;
		// Safe interior only: a sample patch straddling the viewport edge would read
		// the editor chrome next to it.
		if (fNdcX <= -0.95f || fNdcX >= 0.95f || fNdcY <= -0.95f || fNdcY >= 0.95f)
		{
			return false;
		}
		xOut.m_fX = fNdcX;
		xOut.m_fY = fNdcY;
		xOut.m_bValid = true;
		return true;
	}

	bool BMPixResolveArenaPlatforms(Zenith_EntityID (&axOut)[ZM_SIDE_COUNT])
	{
		axOut[ZM_SIDE_PLAYER] = INVALID_ENTITY_ID;
		axOut[ZM_SIDE_ENEMY]  = INVALID_ENTITY_ID;
		u_int uArenaCount = 0u;
		g_xEngine.Scenes().QueryAllScenes<ZM_BattleArena>().ForEach(
			[&axOut, &uArenaCount](Zenith_EntityID, ZM_BattleArena& xArena)
			{
				++uArenaCount;
				if (uArenaCount == 1u)
				{
					// Child index order is the arena's own published contract:
					// 1 = player platform, 2 = enemy platform (ZM_BattleArena.h).
					axOut[ZM_SIDE_PLAYER] = xArena.GetChildEntityID(1u);
					axOut[ZM_SIDE_ENEMY]  = xArena.GetChildEntityID(2u);
				}
			});
		return uArenaCount == 1u
			&& axOut[ZM_SIDE_PLAYER] != INVALID_ENTITY_ID
			&& axOut[ZM_SIDE_ENEMY] != INVALID_ENTITY_ID;
	}

	bool BMPixLatchArenaPoints(ZM_BattleDirector& xDirector)
	{
		const Zenith_EntityID xCameraID = g_xEngine.Scenes().FindMainCameraEntityAcrossScenes();
		Zenith_Entity xCameraEntity = g_xEngine.Scenes().ResolveEntity(xCameraID);
		Zenith_CameraComponent* pxCamera = xCameraEntity.IsValid()
			? xCameraEntity.TryGetComponent<Zenith_CameraComponent>()
			: nullptr;
		if (pxCamera == nullptr)
		{
			BMPixFailLatch("the live battle camera did not resolve");
			return false;
		}
		// The camera's OWN matrices, so the projection is exactly the one that drew
		// the frame -- including whatever aspect ratio the editor left on it.
		Zenith_Maths::Matrix4 xView;
		Zenith_Maths::Matrix4 xProjection;
		pxCamera->BuildViewMatrix(xView);
		pxCamera->BuildProjectionMatrix(xProjection);
		const Zenith_Maths::Matrix4 xViewProjection = xProjection * xView;

		Zenith_EntityID axPlatformID[ZM_SIDE_COUNT];
		if (!BMPixResolveArenaPlatforms(axPlatformID))
		{
			BMPixFailLatch("the unique battle arena / its two platforms did not resolve");
			return false;
		}

		for (u_int uSide = 0u; uSide < (u_int)ZM_SIDE_COUNT; ++uSide)
		{
			const Zenith_EntityID xCreatureID =
				xDirector.GetCreatureModelEntityID(static_cast<ZM_SIDE>(uSide));
			Zenith_Entity xCreature = g_xEngine.Scenes().ResolveEntity(xCreatureID);
			Zenith_TransformComponent* pxCreatureTransform = xCreature.IsValid()
				? xCreature.TryGetComponent<Zenith_TransformComponent>()
				: nullptr;
			if (pxCreatureTransform == nullptr)
			{
				BMPixFailLatch("a side's creature model entity did not resolve -- "
					"ZM_BattleDirector::PlaceCreatureModels placed nothing to photograph");
				return false;
			}
			// Context for the failure message only. A mesh count is an INPUT to
			// rendering and is never treated here as evidence that anything drew.
			Zenith_ModelComponent* pxModel = xCreature.TryGetComponent<Zenith_ModelComponent>();
			g_auBMPixCreatureMeshes[uSide] = pxModel != nullptr ? pxModel->GetNumMeshes() : 0u;
			g_astrBMPixCreatureName[uSide] = xCreature.GetName();

			Zenith_Entity xPlatform = g_xEngine.Scenes().ResolveEntity(axPlatformID[uSide]);
			Zenith_TransformComponent* pxPlatformTransform = xPlatform.IsValid()
				? xPlatform.TryGetComponent<Zenith_TransformComponent>()
				: nullptr;
			if (pxPlatformTransform == nullptr)
			{
				BMPixFailLatch("a side's arena platform transform did not resolve");
				return false;
			}

			Zenith_Maths::Vector3 xCreaturePos;
			Zenith_Maths::Vector3 xPlatformPos;
			pxCreatureTransform->GetPosition(xCreaturePos);
			pxPlatformTransform->GetPosition(xPlatformPos);

			const Zenith_Maths::Vector3 xBody = xCreaturePos
				+ Zenith_Maths::Vector3(0.0f, fBM_PIX_BODY_LIFT, 0.0f);
			const bool bProjected =
				BMPixProject(xViewProjection, xBody, g_axBMPixBody[uSide])
				&& BMPixProject(xViewProjection,
					xBody - Zenith_Maths::Vector3(fBM_PIX_SIDE_OFFSET, 0.0f, 0.0f),
					g_axBMPixSideLeft[uSide])
				&& BMPixProject(xViewProjection,
					xBody + Zenith_Maths::Vector3(fBM_PIX_SIDE_OFFSET, 0.0f, 0.0f),
					g_axBMPixSideRight[uSide])
				&& BMPixProject(xViewProjection,
					xPlatformPos + Zenith_Maths::Vector3(0.0f, fBM_PIX_PLATFORM_LIFT, 0.0f),
					g_axBMPixPlatform[uSide]);
			if (!bProjected)
			{
				BMPixFailLatch("a creature body / side control / platform point did not "
					"project into the safe viewport interior");
				return false;
			}
		}
		return true;
	}

	bool BMPixLatchActionRootGeometry(Zenith_Entity& xDirectorEntity, ZM_BattleDirector& xDirector)
	{
		Zenith_UIComponent* pxUI = xDirectorEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
		{
			BMPixFailLatch("the director entity carries no UI component");
			return false;
		}
		g_bBMPixLatched = BMPixLatchViewport(*pxUI)
			&& BMPixLatchHudRects(*pxUI)
			&& BMPixLatchArenaPoints(xDirector);
		return g_bBMPixLatched;
	}

	// ---- Verify-time evaluation against the bytes the swapchain actually wrote ----

	void BMPixVerifyEnemyHpBar(const Zenith_TestTGAImage& xImage, bool& bPassed)
	{
		const float fBarRight = g_xBMPixEnemyHpBar.m_fLeft
			+ (g_xBMPixEnemyHpBar.m_fRight - g_xBMPixEnemyHpBar.m_fLeft)
			* fBM_PIX_HPBAR_SAMPLE_FRACTION;
		BMPixRegionStats xBar;
		if (!BMPixMeasureCanvasRect(xImage, g_xBMPixEnemyHpBar.m_fLeft,
			g_xBMPixEnemyHpBar.m_fTop, fBarRight, g_xBMPixEnemyHpBar.m_fBottom, xBar))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the enemy HP bar's mapped rect fell outside the capture");
			bPassed = false;
			return;
		}
		if (xBar.m_xMean.y < fBM_PIX_MIN_HPBAR_GREEN
			|| xBar.m_xMean.y - xBar.m_xMean.x < fBM_PIX_MIN_HPBAR_G_OVER_R
			|| xBar.m_xMean.y - xBar.m_xMean.z < fBM_PIX_MIN_HPBAR_G_OVER_B)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the enemy HP bar did NOT reach the framebuffer: its mapped "
				"rect reads (%.3f, %.3f, %.3f) over %u px, wanted green >= %.2f with G-R >= %.2f "
				"and G-B >= %.2f. An HP bar that is 'visible' in the UI tree but absent from the "
				"swapchain is exactly the gap this clause exists to catch",
				(double)xBar.m_xMean.x, (double)xBar.m_xMean.y, (double)xBar.m_xMean.z,
				xBar.m_uSamples, (double)fBM_PIX_MIN_HPBAR_GREEN,
				(double)fBM_PIX_MIN_HPBAR_G_OVER_R, (double)fBM_PIX_MIN_HPBAR_G_OVER_B);
			bPassed = false;
			return;
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_BattleMenuRun] HUD HP bar OBSERVED IN PIXELS: mean (%.3f, %.3f, %.3f) over %u px "
			"(G-R %+.3f, G-B %+.3f)",
			(double)xBar.m_xMean.x, (double)xBar.m_xMean.y, (double)xBar.m_xMean.z,
			xBar.m_uSamples, (double)(xBar.m_xMean.y - xBar.m_xMean.x),
			(double)(xBar.m_xMean.y - xBar.m_xMean.z));
	}

	void BMPixVerifyActionButtons(const Zenith_TestTGAImage& xImage, bool& bPassed)
	{
		// The reference is the panel's OWN interior, in the band between its top edge
		// and the first button -- so the comparison is "button against the surface it
		// is drawn on", not "button against a colour someone chose".
		BMPixRegionStats xStrip;
		if (!BMPixMeasureCanvasRect(xImage,
			g_xBMPixMenuPanel.m_fLeft + fBM_PIX_PANEL_STRIP_INSET,
			g_xBMPixMenuPanel.m_fTop + fBM_PIX_PANEL_STRIP_MARGIN,
			g_xBMPixMenuPanel.m_fRight - fBM_PIX_PANEL_STRIP_INSET,
			g_axBMPixActionButton[0].m_fTop - fBM_PIX_PANEL_STRIP_MARGIN, xStrip))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the menu panel's interior reference strip could not be measured");
			bPassed = false;
			return;
		}

		const char* const aszButtonName[uBM_RUN_ACTION_BUTTON_COUNT] =
		{
			szBM_RUN_FIGHT_NAME, szBM_RUN_CATCH_NAME, szBM_RUN_RUN_NAME
		};
		for (u_int u = 0u; u < uBM_RUN_ACTION_BUTTON_COUNT; ++u)
		{
			BMPixRegionStats xButton;
			if (!BMPixMeasureCanvasRect(xImage,
				g_axBMPixActionButton[u].m_fLeft, g_axBMPixActionButton[u].m_fTop,
				g_axBMPixActionButton[u].m_fRight, g_axBMPixActionButton[u].m_fBottom, xButton))
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuRun] %s's mapped rect fell outside the capture", aszButtonName[u]);
				bPassed = false;
				continue;
			}
			const float fOverPanel = xButton.m_fLuminance - xStrip.m_fLuminance;
			if (xButton.m_fLuminance < fBM_PIX_MIN_BUTTON_LUM
				|| fOverPanel < fBM_PIX_MIN_BUTTON_OVER_PANEL)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuRun] %s did NOT reach the framebuffer: its mapped rect reads "
					"luminance %.3f against a panel interior of %.3f (delta %+.3f), wanted >= %.2f "
					"and a delta >= %.2f",
					aszButtonName[u], (double)xButton.m_fLuminance, (double)xStrip.m_fLuminance,
					(double)fOverPanel, (double)fBM_PIX_MIN_BUTTON_LUM,
					(double)fBM_PIX_MIN_BUTTON_OVER_PANEL);
				bPassed = false;
				continue;
			}
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] %s OBSERVED IN PIXELS: luminance %.3f vs panel interior %.3f "
				"(delta %+.3f) over %u px",
				aszButtonName[u], (double)xButton.m_fLuminance, (double)xStrip.m_fLuminance,
				(double)fOverPanel, xButton.m_uSamples);
		}
	}

	void BMPixVerifyBattleLog(const Zenith_TestTGAImage& xImage, bool& bPassed)
	{
		// Clip the 900-px-wide log box clear of the menu panel so no button pixel can
		// ever be counted as a glyph.
		const float fPanelClip = g_xBMPixMenuPanel.m_fLeft - fBM_PIX_LOG_PANEL_CLEARANCE;
		const float fLogRight = g_xBMPixLogBox.m_fRight < fPanelClip
			? g_xBMPixLogBox.m_fRight : fPanelClip;
		const float fLogHeight = g_xBMPixLogBox.m_fBottom - g_xBMPixLogBox.m_fTop;

		BMPixRegionStats xLog;
		BMPixRegionStats xControl;
		// The NEGATIVE CONTROL is the same box translated up by exactly its own
		// height: same width, same scene, no text. Without it a "bright pixels exist"
		// count is a claim about the scene, not about the log.
		if (!BMPixMeasureCanvasRect(xImage, g_xBMPixLogBox.m_fLeft, g_xBMPixLogBox.m_fTop,
				fLogRight, g_xBMPixLogBox.m_fBottom, xLog)
			|| !BMPixMeasureCanvasRect(xImage, g_xBMPixLogBox.m_fLeft,
				g_xBMPixLogBox.m_fTop - fLogHeight, fLogRight, g_xBMPixLogBox.m_fTop, xControl))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the battle log box / its negative control could not be measured");
			bPassed = false;
			return;
		}
		if (xControl.m_uGlyphPixels > uBM_PIX_MAX_CONTROL_GLYPHS)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the log's negative control already holds %u glyph-white px "
				"(<= %u expected) -- the search region is NOT clean, so any count inside the log "
				"box is uninterpretable",
				xControl.m_uGlyphPixels, uBM_PIX_MAX_CONTROL_GLYPHS);
			bPassed = false;
			return;
		}
		if (xLog.m_uGlyphPixels < uBM_PIX_MIN_LOG_GLYPHS)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the battle text log did NOT reach the framebuffer: %u glyph-white "
				"px in its mapped box (wanted >= %u) against %u in the clean control below it",
				xLog.m_uGlyphPixels, uBM_PIX_MIN_LOG_GLYPHS, xControl.m_uGlyphPixels);
			bPassed = false;
			return;
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_BattleMenuRun] battle text log OBSERVED IN PIXELS: %u glyph-white px in %u sampled, "
			"against %u in the same-sized control box directly above it",
			xLog.m_uGlyphPixels, xLog.m_uSamples, xControl.m_uGlyphPixels);
	}

	void BMPixVerifyCreatures(const Zenith_TestTGAImage& xImage, bool& bPassed)
	{
		Zenith_Maths::Vector3 axBody[ZM_SIDE_COUNT] = {};
		bool abBodySampled[ZM_SIDE_COUNT] = {};

		for (u_int uSide = 0u; uSide < (u_int)ZM_SIDE_COUNT; ++uSide)
		{
			Zenith_Maths::Vector3 xBody;
			Zenith_Maths::Vector3 xLeft;
			Zenith_Maths::Vector3 xRight;
			Zenith_Maths::Vector3 xPlatform;
			if (!BMPixMeasurePatch(xImage, g_axBMPixBody[uSide], uBM_PIX_BODY_RADIUS, xBody)
				|| !BMPixMeasurePatch(xImage, g_axBMPixSideLeft[uSide], uBM_PIX_BODY_RADIUS, xLeft)
				|| !BMPixMeasurePatch(xImage, g_axBMPixSideRight[uSide], uBM_PIX_BODY_RADIUS, xRight)
				|| !BMPixMeasurePatch(xImage, g_axBMPixPlatform[uSide], uBM_PIX_PLATFORM_RADIUS, xPlatform))
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuRun] the %s creature's sample patches were unreadable in the capture",
					BMPixSideName(uSide));
				bPassed = false;
				continue;
			}

			const float fVsLeft     = BMPixSeparation(xBody, xLeft);
			const float fVsRight    = BMPixSeparation(xBody, xRight);
			const float fVsPlatform = BMPixSeparation(xBody, xPlatform);
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] %s creature '%s' (%u meshes) body RGB (%.3f, %.3f, %.3f) at "
				"NDC (%+.3f, %+.3f); vs left control %.3f, vs right control %.3f, vs platform %.3f",
				BMPixSideName(uSide), g_astrBMPixCreatureName[uSide].c_str(),
				g_auBMPixCreatureMeshes[uSide],
				(double)xBody.x, (double)xBody.y, (double)xBody.z,
				(double)g_axBMPixBody[uSide].m_fX, (double)g_axBMPixBody[uSide].m_fY,
				(double)fVsLeft, (double)fVsRight, (double)fVsPlatform);
			// The slab RGB is logged alongside the body: when this arm trips, the first
			// question is always "did the patch move, or did the slab change colour?"
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] %s platform RGB (%.3f, %.3f, %.3f) at NDC (%+.3f, %+.3f)",
				BMPixSideName(uSide),
				(double)xPlatform.x, (double)xPlatform.y, (double)xPlatform.z,
				(double)g_axBMPixPlatform[uSide].m_fX, (double)g_axBMPixPlatform[uSide].m_fY);

			if (fVsLeft < fBM_PIX_MIN_BODY_VS_SIDE || fVsRight < fBM_PIX_MIN_BODY_VS_SIDE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuRun] NO %s creature reached the framebuffer: the pixels at its "
					"projected body are %.3f / %.3f from the background %.1f m to either side "
					"(wanted >= %.2f on both). Its entity '%s' exists with %u meshes, so this is a "
					"RENDER gap, not a placement one",
					BMPixSideName(uSide), (double)fVsLeft, (double)fVsRight,
					(double)fBM_PIX_SIDE_OFFSET, (double)fBM_PIX_MIN_BODY_VS_SIDE,
					g_astrBMPixCreatureName[uSide].c_str(), g_auBMPixCreatureMeshes[uSide]);
				bPassed = false;
			}
			// The placement guard (see the constant's comment) -- and the arm that
			// caught the player half of the model-less mutation, where the body
			// point falls straight through onto pale stone.
			if (fVsPlatform < fBM_PIX_MIN_BODY_VS_PLATFORM)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuRun] the %s body sample is only %.3f from its own platform "
					"(wanted >= %.2f) -- the patch has slid onto the slab, so nothing it reads is "
					"evidence about a creature",
					BMPixSideName(uSide), (double)fVsPlatform, (double)fBM_PIX_MIN_BODY_VS_PLATFORM);
				bPassed = false;
			}

			axBody[uSide] = xBody;
			abBodySampled[uSide] = true;
		}

		if (!abBodySampled[ZM_SIDE_PLAYER] || !abBodySampled[ZM_SIDE_ENEMY])
		{
			return;
		}
		// Both platforms carry the SAME species in this fixture, so two real renders
		// read alike. Two unrelated background patches do not: measured, the
		// model-less mutation puts pale stone on the player side and sky/water on the
		// enemy side, 0.851 apart against 0.141 when both models really draw.
		const float fBodies = BMPixSeparation(axBody[ZM_SIDE_PLAYER], axBody[ZM_SIDE_ENEMY]);
		if (fBodies > fBM_PIX_MAX_BODY_VS_BODY)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the two arena creatures do not read alike: '%s' vs '%s' differ "
				"by %.3f (wanted <= %.2f). Two renders of the same species agree; two patches of "
				"unrelated background do not",
				g_astrBMPixCreatureName[ZM_SIDE_PLAYER].c_str(),
				g_astrBMPixCreatureName[ZM_SIDE_ENEMY].c_str(),
				(double)fBodies, (double)fBM_PIX_MAX_BODY_VS_BODY);
			bPassed = false;
			return;
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_BattleMenuRun] BOTH arena creature models OBSERVED IN PIXELS: '%s' and '%s' agree "
			"to %.3f and each stands clear of its local background",
			g_astrBMPixCreatureName[ZM_SIDE_PLAYER].c_str(),
			g_astrBMPixCreatureName[ZM_SIDE_ENEMY].c_str(), (double)fBodies);
	}

	void BMPixVerifyActionRootCapture(bool& bPassed)
	{
		if (!g_bBMPixLatched)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] no ACTION_ROOT pixel geometry to read the capture with: %s",
				g_szBMPixLatchFail);
			bPassed = false;
			return;
		}

		Zenith_TestTGAImage xImage;
		if (!Zenith_TestLoadTGA(g_strBMRunVisualShotPath.c_str(), xImage))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the ACTION_ROOT swapchain capture is missing or not a 32-bit "
				"top-left BGRA TGA: %s", g_strBMRunVisualShotPath.c_str());
			bPassed = false;
			return;
		}

		const float fViewportRight  = g_xBMPixViewportPos.x + g_xBMPixViewportSize.x;
		const float fViewportBottom = g_xBMPixViewportPos.y + g_xBMPixViewportSize.y;
		if (g_xBMPixViewportPos.x < 0.0f || g_xBMPixViewportPos.y < 0.0f
			|| fViewportRight > static_cast<float>(xImage.m_uWidth) + 1.0f
			|| fViewportBottom > static_cast<float>(xImage.m_uHeight) + 1.0f)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleMenuRun] the latched viewport (%.1f,%.1f %.1fx%.1f) is outside the "
				"%ux%u capture, so no mapped region can be trusted",
				(double)g_xBMPixViewportPos.x, (double)g_xBMPixViewportPos.y,
				(double)g_xBMPixViewportSize.x, (double)g_xBMPixViewportSize.y,
				xImage.m_uWidth, xImage.m_uHeight);
			bPassed = false;
			return;
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_BattleMenuRun] reading the ACTION_ROOT capture %ux%u; canvas %.0fx%.0f mapped "
			"into viewport (%.0f,%.0f %.0fx%.0f). TGA=%s",
			xImage.m_uWidth, xImage.m_uHeight,
			(double)g_xBMPixCanvasSize.x, (double)g_xBMPixCanvasSize.y,
			(double)g_xBMPixViewportPos.x, (double)g_xBMPixViewportPos.y,
			(double)g_xBMPixViewportSize.x, (double)g_xBMPixViewportSize.y,
			g_strBMRunVisualShotPath.c_str());

		BMPixVerifyEnemyHpBar(xImage, bPassed);
		BMPixVerifyActionButtons(xImage, bPassed);
		BMPixVerifyBattleLog(xImage, bPassed);
		BMPixVerifyCreatures(xImage, bPassed);
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
		if (g_eBMMode == MenuTestMode::Win || g_eBMMode == MenuTestMode::Whiteout)
		{
			// The default menu is ACTION_ROOT, cursor 0 = Fight: one ENTER opens the
			// move list, the next ENTER submits move 0. Pressing ENTER every frame
			// therefore picks Fight->move0 each turn (a HIDDEN-menu press is inert). The
			// Whiteout test uses the SAME drive as Win -- only the enemy level differs, so
			// the L5 lead LOSES (Whiteout) instead of winning (Win).
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		}
		else if (g_eBMMode == MenuTestMode::Catch)
		{
			// Menu-aware catch: only confirm once the committed cursor is on Catch, so
			// we never accidentally open the move list. DOWN walks Fight(0)->Catch(1);
			// ENTER on Catch submits {ZM_ACTION_ITEM, catch ball}. With ZM_ITEM_PRIMEORB
			// installed in Setup the capture succeeds on turn 1 (zero RNG).
			// The cursor is resolved to an ENTRY through MenuRootItemAtIndex rather than
			// compared to an item id: the root list is gated on the battle's can-catch
			// flag, so an index only means what the LIVE list says it means (it is the
			// identity here, this being a wild battle).
			const ZM_BattleMenuScreen eScreen = pxDirector->GetHudMenuScreen();
			const int                 iCursor = pxDirector->GetHudMenuCursor();
			const bool                bCanCatch = pxDirector->GetCore().IsCatchAllowed();
			if (eScreen == ZM_BATTLE_MENU_ACTION_ROOT)
			{
				if (ZM_UI_BattleHUD::MenuRootItemAtIndex(iCursor, bCanCatch) == ZM_BATTLE_MENU_CATCH)
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
			const bool                bCanCatch = pxDirector->GetCore().IsCatchAllowed();
			if (eScreen == ZM_BATTLE_MENU_ACTION_ROOT)
			{
				if (g_iBMRunVisualDwellFrames < iBM_RUN_VISUAL_DWELL_FRAMES)
				{
					++g_iBMRunVisualDwellFrames;
					// >= rather than == : the dump is only requested once the pixel
					// geometry has been latched for the SAME frame, and the editor
					// layout may not be sampleable on the first attempt. Retrying
					// across the rest of the dwell costs nothing and the first
					// attempt normally succeeds, so the capture frame is unchanged.
					if (g_iBMRunVisualDwellFrames >= iBM_RUN_VISUAL_CAPTURE_FRAME
						&& !g_bBMRunVisualShotRequested)
					{
						Zenith_UIComponent* pxUI = xEntity.TryGetComponent<Zenith_UIComponent>();
						// Latched on EVERY attempt, not only the one that captures: the
						// UI-visibility clause must stay independent of whether the pixel
						// geometry latched, or one defect would red both and neither
						// failure would mean what it says.
						g_bBMRunRootVisualsValid = pxUI != nullptr
							&& BattleMenuRunRootVisualsMatch(*pxUI);
						if (BMPixLatchActionRootGeometry(xEntity, *pxDirector))
						{
							Flux_Screenshot::RequestDump(g_strBMRunVisualShotPath.c_str());
							g_bBMRunVisualShotRequested = true;
							Zenith_Log(LOG_CATEGORY_UNITTEST,
								"[ZM_BattleMenuRun] requested ACTION_ROOT framebuffer evidence -> %s",
								g_strBMRunVisualShotPath.c_str());
						}
					}
				}
				else
				{
					// Resolved through the LIVE list (see the Catch drive above): with catching
					// gated off, Run would sit at index 1, not 2.
					if (ZM_UI_BattleHUD::MenuRootItemAtIndex(iCursor, bCanCatch) == ZM_BATTLE_MENU_RUN)
					{
						Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
					}
					else
					{
						Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
					}
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
		Zenith_InputSimulator::SimulateKeyUp(g_eBMWalkKey);
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
		g_iBMRunVisualDwellFrames  = 0;
		g_bBMRunVisualShotRequested = false;
		g_bBMRunRootVisualsValid   = false;
		g_strBMRunVisualShotPath.clear();

		// The ACTION_ROOT pixel geometry (batch mode reuses the process).
		g_bBMPixLatched      = false;
		g_szBMPixLatchFail   = "the ACTION_ROOT pixel geometry was never latched";
		g_xBMPixViewportPos  = Zenith_Maths::Vector2(0.0f);
		g_xBMPixViewportSize = Zenith_Maths::Vector2(0.0f);
		g_xBMPixCanvasSize   = Zenith_Maths::Vector2(0.0f);
		g_xBMPixEnemyHpBar   = BMPixCanvasRect{};
		g_xBMPixMenuPanel    = BMPixCanvasRect{};
		g_xBMPixLogBox       = BMPixCanvasRect{};
		for (u_int u = 0u; u < uBM_RUN_ACTION_BUTTON_COUNT; ++u)
		{
			g_axBMPixActionButton[u] = BMPixCanvasRect{};
		}
		for (u_int u = 0u; u < (u_int)ZM_SIDE_COUNT; ++u)
		{
			g_axBMPixBody[u]      = BMPixNdcPoint{};
			g_axBMPixSideLeft[u]  = BMPixNdcPoint{};
			g_axBMPixSideRight[u] = BMPixNdcPoint{};
			g_axBMPixPlatform[u]  = BMPixNdcPoint{};
			g_auBMPixCreatureMeshes[u] = 0u;
			g_astrBMPixCreatureName[u].clear();
		}

		g_uBMEntryGrassBlades      = 0u;

		g_xBMParkedPos             = Zenith_Maths::Vector3(0.0f);
		g_uBMGrassAtPark           = 0u;

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

		g_bBMWhiteoutMgrCapturedBefore = false;
		g_uBMWhiteoutMaxHP             = 0u;
		g_bBMWhiteoutPreDamaged        = false;
		g_uBMWhiteoutIssuedLoadBefore  = 0u;
		g_bBMWhiteoutWarpSettled       = false;
		g_uBMWhiteoutIssuedLoadAfter   = 0u;
		g_uBMWhiteoutWarpStateAfter    = (u_int)ZM_WARP_TRANSITION_IDLE;
		g_bBMWhiteoutGameStateAfter    = false;
		g_bBMWhiteoutPendingAfter      = true;
		g_uBMWhiteoutLeadHpAfter       = 0u;
		g_uBMWhiteoutLeadMaxHpAfter    = 0u;

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
		if (!g_bBMPrereqsPresent)
		{
			return;
		}

		const std::filesystem::path xVisualDir = BattleMenuVisualAuditDir();
		std::error_code xDirError;
		std::filesystem::create_directories(xVisualDir, xDirError);
		if (xDirError)
		{
			FailBM("could not create the battle-menu visual-audit artifact directory");
			return;
		}
		g_strBMRunVisualShotPath = (xVisualDir / "battle_menu_run_root.tga").string();
		std::error_code xRemoveError;
		std::filesystem::remove(g_strBMRunVisualShotPath, xRemoveError);
		if (xRemoveError)
		{
			FailBM("could not remove the stale battle-menu visual-audit capture");
		}
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

	void Setup_ZMBattleMenuWhiteout()
	{
		g_eBMMode = MenuTestMode::Whiteout;
		SetupCommon();
		// No catch-ball override and no extra setup: the forced HIGH-LEVEL enemy (installed
		// in AwaitReady) plus the ENTER-spam Fight->move0 drive is all the loss needs. The
		// pre-damage of the lead + issued-load baseline are also captured in AwaitReady, where
		// the manager + persistent GameState first resolve.
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
			// changes both the party count and the caught-set. The Whiteout test forces a
			// HIGH-LEVEL enemy so the L5 lead reliably LOSES and triggers the whiteout warp.
			if (g_eBMMode == MenuTestMode::Catch)
			{
				pxSystem->ForceEncounterOnNextTransitionForTests(eBM_CATCH_ENEMY_SPECIES, uBM_CATCH_ENEMY_LEVEL);
			}
			else if (g_eBMMode == MenuTestMode::Whiteout)
			{
				pxSystem->ForceEncounterOnNextTransitionForTests(eBM_WHITEOUT_ENEMY_SPECIES, uBM_WHITEOUT_ENEMY_LEVEL);
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
			g_uBMEntryGrassBlades = g_xEngine.Grass().GetScheduledInstanceCount();

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

					// SC5 whiteout baseline (only used by the Whiteout test): pre-damage the
					// persistent lead to 1 HP so the manager's HealAllFull is OBSERVABLE
					// (a full lead would make the heal check vacuous), capturing its full HP
					// first. Also snapshot the manager's issued-load count so Verify can prove
					// the whiteout warp issued exactly one load: the between-tests reset zeroes
					// it and the Setup's direct scene load never touches it, so before == 0.
					if (g_eBMMode == MenuTestMode::Whiteout)
					{
						g_uBMWhiteoutMaxHP = pxGameState->m_xParty.Lead().GetMaxHP();
						pxGameState->m_xParty.Lead().m_uCurrentHp = 1u;
						g_bBMWhiteoutPreDamaged = true;
						if (ZM_GameStateManager* pxManager = ResolveSingletonGameStateManager())
						{
							g_uBMWhiteoutIssuedLoadBefore  = pxManager->GetIssuedLoadRequestCount();
							g_bBMWhiteoutMgrCapturedBefore = true;
						}
					}
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
			Zenith_InputSimulator::SimulateKeyDown(g_eBMWalkKey);
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
				// THE grass baseline, latched with the encounter -- i.e. at the parked
				// position, for the same reason the drift baseline is the parked position:
				// the player must WALK to trigger an encounter. Grass is GPU-regenerated
				// around the camera every frame, so its blade count is a function of where
				// the camera is, and comparing the resumed count against the ENTRY count
				// asserted that walking does not change the grass -- false by construction.
				g_uBMGrassAtPark = g_xEngine.Grass().GetScheduledInstanceCount();
				// The encounter latched and the machine started; release the key now.
				Zenith_InputSimulator::SimulateKeyUp(g_eBMWalkKey);
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

					// Whiteout: the battle round-trip resolved, but a LOSS latched
					// m_bPendingWhiteout, so the manager is about to warp to Dawnmere/
					// TownCenter. The exact-restore settle/sample below would RACE that
					// incoming warp (it moves the player + reloads the scene), so record
					// the round-trip's completion NOW and hand off to the warp-wait phase.
					if (g_eBMMode == MenuTestMode::Whiteout)
					{
						g_uBMCompletedAfter = pxTransition->GetCompletedBattleCount();
						g_uBMAbortedAfter   = pxTransition->GetAbortedTransitionCount();
						g_bBMBattleSceneUnloaded = !g_xEngine.Scenes().FindLoadedSceneByPath(
							std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT).IsValid();
						g_eBMPhase = BMPhase::AwaitWhiteoutWarp;
						g_iBMPhaseFrames = 0;
					}
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
			g_uBMGrassAfter = g_xEngine.Grass().GetScheduledInstanceCount();

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

		case BMPhase::AwaitWhiteoutWarp:
		{
			// The loss latched m_bPendingWhiteout in the write-back; the manager's OnUpdate
			// consumes it once the battle transition is idle -> HealAllFull + warp to
			// Dawnmere/TownCenter. Poll until that warp FULLY settles, then sample once.
			ZM_GameStateManager* pxManager = ResolveSingletonGameStateManager();
			if (pxManager == nullptr)
			{
				FailBM("the ZM_GameStateManager singleton stopped resolving during the whiteout warp");
				return false;
			}

			ZM_GameState* pxGameState = nullptr;
			const bool bHaveGS = ZM_GameStateManager::TryGetGameState(pxGameState)
				&& pxGameState != nullptr;
			const bool bPending  = bHaveGS && pxGameState->m_bPendingWhiteout;
			const bool bWarpIdle = pxManager->GetTransitionState() == ZM_WARP_TRANSITION_IDLE;
			const bool bIssued   = pxManager->GetIssuedLoadRequestCount() > g_uBMWhiteoutIssuedLoadBefore;
			const int  iBuildNow = g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;

			// Settled == the latch was consumed (pending cleared), the warp returned to IDLE,
			// it issued the whiteout load, and we are back on Dawnmere (build 2).
			if (!bPending && bWarpIdle && bIssued && iBuildNow == 2)
			{
				g_bBMWhiteoutWarpSettled    = true;
				g_uBMWhiteoutIssuedLoadAfter = pxManager->GetIssuedLoadRequestCount();
				g_uBMWhiteoutWarpStateAfter  = (u_int)pxManager->GetTransitionState();
				g_iBMBuildIndexAfter         = iBuildNow;

				// The heal ran BEFORE the warp was accepted, so a fresh GameState read now
				// shows the pre-damaged lead restored to full and the latch cleared.
				if (bHaveGS)
				{
					g_bBMWhiteoutPendingAfter     = pxGameState->m_bPendingWhiteout;   // false
					g_uBMWhiteoutLeadHpAfter      = pxGameState->m_xParty.Lead().m_uCurrentHp;
					g_uBMWhiteoutLeadMaxHpAfter   = pxGameState->m_xParty.Lead().GetMaxHP();
					g_bBMWhiteoutGameStateAfter   = true;
				}

				// The warped-in overworld player is live + movement re-enabled.
				PlayerView xPlayerWO;
				if (FindActivePlayer(xPlayerWO))
				{
					g_xBMResumePlayerPos       = xPlayerWO.m_xPosition;
					g_bBMPlayerMovementEnabled = xPlayerWO.m_pxController->IsMovementEnabled();
					g_bBMPlayerResolved        = true;
				}

				g_eBMPhase = BMPhase::Done;
				return false;
			}

			if (g_iBMPhaseFrames > iBM_WHITEOUT_DEADLINE)
			{
				// Capture what we can for the failure diagnostics.
				g_uBMWhiteoutIssuedLoadAfter = pxManager->GetIssuedLoadRequestCount();
				g_uBMWhiteoutWarpStateAfter  = (u_int)pxManager->GetTransitionState();
				g_bBMWhiteoutPendingAfter    = bPending;
				FailBM("whiteout warp never settled (want: pending cleared + warp IDLE + "
					"issued-load risen + active build 2)");
				return false;
			}
			return true;
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
			g_eBMMode == MenuTestMode::Win      ? "ZM_BattleMenuWin"
		  : g_eBMMode == MenuTestMode::Catch    ? "ZM_BattleMenuCatch"
		  : g_eBMMode == MenuTestMode::Whiteout ? "ZM_BattleMenuWhiteout"
		  :                                       "ZM_BattleMenuRun";

		if (g_bBMActive)
		{
			const float fDrift = glm::length(g_xBMResumePlayerPos - g_xBMParkedPos);

			// One localisable line dumping every captured value.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[%s] captured: failed=%s (%s) directorSeen=%s completedAfter=%u (want 1) "
				"abortedAfter=%u (want 0) buildAfter=%d (want 2) battleUnloaded=%s "
				"playerResolved=%s movementEnabled=%s drift=%f (want <0.05) entryGrass=%u "
				"parkedGrass=%u grassAfter=%u (want ==parkedGrass) winnerCaptured=%s winner=%d (Win wants PLAYER=%d, "
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
				g_uBMEntryGrassBlades, g_uBMGrassAtPark, g_uBMGrassAfter,
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
			// The drift + grass-restore invariants hold for the plain round trip, but the
			// Whiteout test DELIBERATELY breaks them: the loss-driven warp relocates the
			// player to TownCenter and reloads Dawnmere, so its exact-restore locks are the
			// whiteout-specific ones below (heal + build 2 + IDLE), not these.
			if (g_eBMMode != MenuTestMode::Whiteout)
			{
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
				if (!ZM_GrassBaseline::Restored(g_uBMGrassAtPark, g_uBMGrassAfter))
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[%s] resumed grass blade count was %u against a parked %u -- "
						"either the field is EMPTY or it converged by more than %u tiles "
						"(see ZM_GrassBaseline.h for why this is not an equality)",
						szTag, g_uBMGrassAfter, g_uBMGrassAtPark,
						ZM_GrassBaseline::uMAX_CONVERGENCE_TILES);
					bPassed = false;
				}
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
			else if (g_eBMMode == MenuTestMode::Whiteout)
			{
				// The ENTER-spam Fight->move0 sent the L5 lead against the forced L60 enemy,
				// which KO'd it: the core ends with the ENEMY as winner, the write-back latched
				// m_bPendingWhiteout, and the manager consumed it -> HealAllFull + warp to
				// Dawnmere/TownCenter. Prove the whole SC5 chain landed.
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleMenuWhiteout] whiteout: preDamaged=%s maxHP=%u issuedBefore=%u "
					"issuedAfter=%u warpState=%u buildAfter=%d warpSettled=%s gsAfter=%s "
					"pendingAfter=%s leadHpAfter=%u leadMaxHpAfter=%u",
					g_bBMWhiteoutPreDamaged ? "true" : "false", g_uBMWhiteoutMaxHP,
					g_uBMWhiteoutIssuedLoadBefore, g_uBMWhiteoutIssuedLoadAfter,
					g_uBMWhiteoutWarpStateAfter, g_iBMBuildIndexAfter,
					g_bBMWhiteoutWarpSettled ? "true" : "false",
					g_bBMWhiteoutGameStateAfter ? "true" : "false",
					g_bBMWhiteoutPendingAfter ? "true" : "false",
					g_uBMWhiteoutLeadHpAfter, g_uBMWhiteoutLeadMaxHpAfter);

				// ...the ENEMY won (the loss that triggers the whiteout).
				if (!g_bBMWinnerCaptured)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWhiteout] the core never reached OVER while the Battle scene was "
						"loaded -- no winner was captured");
					bPassed = false;
				}
				else if (g_eBMWinner != ZM_SIDE_ENEMY)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWhiteout] the battle winner was side %d, expected ENEMY %d (the "
						"forced L60 enemy must KO the L5 lead)", (int)g_eBMWinner, (int)ZM_SIDE_ENEMY);
					bPassed = false;
				}

				// The lead MUST have been pre-damaged, else the heal check is vacuous.
				if (!g_bBMWhiteoutPreDamaged)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWhiteout] the lead was never pre-damaged -- the heal check is vacuous");
					bPassed = false;
				}

				// Guarded on the manager resolving before the encounter: skip-safe if absent.
				if (g_bBMWhiteoutMgrCapturedBefore && !g_bBMWhiteoutWarpSettled)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuWhiteout] the whiteout warp never settled -- the whiteout could not be "
						"observed");
					bPassed = false;
				}
				else if (g_bBMWhiteoutMgrCapturedBefore)
				{
					// The whiteout warp issued exactly one scene load (0 -> 1): the manager
					// never loads during the ZM_BattleTransition round trip, and the Setup's
					// direct scene load never touches the manager's counter.
					if (g_uBMWhiteoutIssuedLoadAfter != g_uBMWhiteoutIssuedLoadBefore + 1u)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuWhiteout] manager issued-load count went %u -> %u, expected +1 "
							"(exactly one whiteout warp load)",
							g_uBMWhiteoutIssuedLoadBefore, g_uBMWhiteoutIssuedLoadAfter);
						bPassed = false;
					}
					// The warp returned to IDLE on Dawnmere (build 2).
					if (g_uBMWhiteoutWarpStateAfter != (u_int)ZM_WARP_TRANSITION_IDLE)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuWhiteout] warp state after the whiteout was %u, expected IDLE %u",
							g_uBMWhiteoutWarpStateAfter, (u_int)ZM_WARP_TRANSITION_IDLE);
						bPassed = false;
					}
					if (g_iBMBuildIndexAfter != 2)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuWhiteout] active build index after the whiteout was %d, expected 2 "
							"(Dawnmere)", g_iBMBuildIndexAfter);
						bPassed = false;
					}
					// The latch was consumed and the whole party healed to full.
					if (!g_bBMWhiteoutGameStateAfter)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BattleMenuWhiteout] the persistent GameState did not resolve after the whiteout");
						bPassed = false;
					}
					else
					{
						if (g_bBMWhiteoutPendingAfter)
						{
							Zenith_Error(LOG_CATEGORY_UNITTEST,
								"[ZM_BattleMenuWhiteout] m_bPendingWhiteout was still set after the warp -- the "
								"latch was not consumed");
							bPassed = false;
						}
						// HealAllFull restored the pre-damaged (1 HP) lead to full.
						if (g_uBMWhiteoutLeadHpAfter != g_uBMWhiteoutLeadMaxHpAfter)
						{
							Zenith_Error(LOG_CATEGORY_UNITTEST,
								"[ZM_BattleMenuWhiteout] lead HP after the whiteout was %u / max %u, expected "
								"full (the whiteout HealAllFull must restore the pre-damaged lead)",
								g_uBMWhiteoutLeadHpAfter, g_uBMWhiteoutLeadMaxHpAfter);
							bPassed = false;
						}
						if (g_uBMWhiteoutLeadHpAfter <= 1u)
						{
							Zenith_Error(LOG_CATEGORY_UNITTEST,
								"[ZM_BattleMenuWhiteout] lead HP after the whiteout was %u, expected > 1 (healed "
								"up from the pre-damaged 1)", g_uBMWhiteoutLeadHpAfter);
							bPassed = false;
						}
					}
				}
			}
			else
			{
				// The menu-driven Run reached the engine as a successful flee: a FLEE
				// event, and NO winner (a flee ends the battle as a draw).
				if (g_iBMRunVisualDwellFrames != iBM_RUN_VISUAL_DWELL_FRAMES)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuRun] ACTION_ROOT visual dwell reached %d frames, expected exactly %d "
						"before menu navigation began",
						g_iBMRunVisualDwellFrames, iBM_RUN_VISUAL_DWELL_FRAMES);
					bPassed = false;
				}
				if (!g_bBMRunVisualShotRequested)
				{
					// The dump is only requested once the pixel geometry latches, so the
					// latch's own reason is the actionable half of this failure.
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuRun] ACTION_ROOT framebuffer evidence was never requested: %s",
						g_szBMPixLatchFail);
					bPassed = false;
				}
				else if (!DiskFilePresent(g_strBMRunVisualShotPath))
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuRun] ACTION_ROOT framebuffer evidence produced no non-empty TGA: %s",
						g_strBMRunVisualShotPath.c_str());
					bPassed = false;
				}
				else
				{
					// ===== THE PIXEL ASSERTIONS (ZM-D-168 audit finding 2) ==========
					// Everything else in this test -- including the two clauses just
					// above -- is about an INPUT to rendering: a UI element's visible
					// flag, its text, an engine event, a file's existence. Only this
					// call opens the bytes the swapchain wrote. It is gated on no
					// Zenith_IsNullRenderer() check by design: this test is
					// m_bRequiresGraphics = true, so it SKIPS entirely on the Null
					// backend and can never reach here with a dump that was never
					// written. (ZM_RivalVesperAuthored_Test's marker clause needs the
					// opposite treatment because that test runs for real headless.)
					BMPixVerifyActionRootCapture(bPassed);
				}
				if (!g_bBMRunRootVisualsValid)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleMenuRun] midpoint ACTION_ROOT UI did not resolve a visible menu panel "
						"plus visible Fight/Catch/Run buttons carrying their expected text");
					bPassed = false;
				}
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
		Zenith_InputSimulator::SimulateKeyUp(g_eBMWalkKey);
		Zenith_InputSimulator::ClearFixedDt();
		ZM_SetInstantBattlesForTests(false);
		ZM_SetCatchBallForTests(ZM_ITEM_CATCHORB);
		ZM_BattleTransition::ResetRuntimeStateForTests();
		// Whiteout only: this is the sole test that drives the manager's warp machine, so
		// reset it to IDLE (also zeroes its issued-load count) -- a mid-warp manager (e.g. a
		// deadline-failed run) must not bleed into the next batched test. Skip-safe no-op
		// when no manager exists; guarded so the other three tests' teardown is untouched.
		if (g_eBMMode == MenuTestMode::Whiteout)
		{
			ZM_GameStateManager::ResetRuntimeStateForTests();
		}
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

// Larger frame budget than the other three: there are now TWO transitions -- the battle
// round trip AND the loss-driven whiteout warp (fade-out + Dawnmere reload + spawn/camera
// + fade-in) -- so the frame budget must cover both back to back.
static const Zenith_AutomatedTest g_xZMBattleMenuWhiteoutTest = {
	"ZM_BattleMenuWhiteout_Test",
	&Setup_ZMBattleMenuWhiteout,
	&Step_ZMBattleMenu,
	&Verify_ZMBattleMenu,
	/* maxFrames */ 4000,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleMenuWhiteoutTest);

#endif // ZENITH_INPUT_SIMULATOR
