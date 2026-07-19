#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_UI -- the windowed gate for the overworld pause menu (S6 item 2
// SC1). ONE test, m_bRequiresGraphics = true:
//   * ZM_MenuOpenClose_Test -- in a runtime-ready Dawnmere, press the menu key
//     (M) and assert the ROOT menu opens with the player FROZEN; press an arrow
//     (Down) and assert the focus cursor advances (engine focus-nav); press
//     Escape and assert the menu closes, player movement is re-enabled, and the
//     canvas focus is cleared.
//
// It CLONES the ZM_AutoTests_BattleMenu Dawnmere runtime-ready gate, its fixed-dt
// 1/30, its RequestSkip-when-assets-absent guard order, and its FRESH-resolve-
// every-frame discipline (the component pool relocates on swap-and-pop, so no
// pointer is cached across frames). The pause menu lives on the persistent
// ZM_MenuRoot entity (authored in FrontEnd, DontDestroyOnLoad), so it is reachable
// from Dawnmere without re-authoring; the test drives it with STATE-SETTERS only
// (SimulateKeyPress), never reentrant StepFrame calls.
// ============================================================================

namespace
{
	// -------------------------------------------------------------------------
	// Shared asset guards + entity views (re-declared from ZM_AutoTests_BattleMenu
	// -- those copies are file-local and not linkable across TUs).
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

	// The persistent ZM_UI_MenuStack singleton, resolved FRESH each frame (the pool
	// relocates on swap-and-pop, so this pointer is never cached across frames).
	ZM_UI_MenuStack* ResolveSingletonMenuStack()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_UI_MenuStack>()
			: nullptr;
	}

	// The persistent ZM_MenuRoot canvas's currently-focused element (nullptr when
	// none). Read FRESH each call; used to prove focus is cleared on close.
	bool MenuFocusCleared()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return true;   // no menu root -> vacuously "no focus held"
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		Zenith_UIComponent* pxUI = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		if (pxUI == nullptr)
		{
			return true;
		}
		return pxUI->GetCanvas().GetFocusedElement() == nullptr;
	}

	// -------------------------------------------------------------------------
	// The phase machine
	// -------------------------------------------------------------------------

	constexpr float fUI_FIXED_DT = 1.0f / 30.0f;

	enum class UIPhase
	{
		AwaitReady,
		Opening,     // press the menu key each frame until the ROOT opens
		Navigating,  // press Down until the focus cursor advances Party(0) -> Bag(1)
		Closing,     // press Escape each frame until the menu closes
		Done,
	};

	constexpr int iUI_READY_DEADLINE  = 420;   // Dawnmere first-load ready window (round-trip parity)
	constexpr int iUI_OPEN_DEADLINE   = 120;   // frames for the M press to open the menu
	constexpr int iUI_NAV_DEADLINE    = 120;   // frames for the Down press to advance the cursor
	constexpr int iUI_CLOSE_DEADLINE  = 120;   // frames for the Escape press to close the menu

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	UIPhase g_eUIPhase          = UIPhase::Done;
	int     g_iUIPhaseFrames    = 0;
	bool    g_bUIPrereqsPresent = false;
	bool    g_bUIActive         = false;
	bool    g_bUIFailed         = false;
	const char* g_szUIFailure   = "test did not reach verification";

	// ---- Captured observations ----
	bool g_bUIMovementEnabledBefore = false;   // player movable BEFORE opening (baseline)
	bool g_bUIMenuOpened            = false;   // the menu reported open after M
	bool g_bUIPlayerFrozenWhileOpen = false;   // player movement disabled while the menu is open
	u_int g_uUIDepthWhileOpen       = 0u;      // stack depth while open (want 1 = ROOT only)
	u_int g_eUITopWhileOpen         = (u_int)ZM_MENU_SCREEN_NONE;   // top screen while open (want ROOT)
	int  g_iUICursorOnOpen          = -99;     // focus cursor right after open (want 0 = Party)
	int  g_iUICursorAfterNav        = -99;     // focus cursor after Down (want 1 = Bag)
	bool g_bUIMenuClosed            = false;   // the menu reported closed after Escape
	bool g_bUIMovementReenabled     = false;   // player movement re-enabled after close
	bool g_bUIFocusClearedOnClose   = false;   // canvas focus == nullptr after close

	bool ReadActivePlayerMovementEnabled(bool& bEnabledOut)
	{
		PlayerView xPlayer;
		if (!FindActivePlayer(xPlayer))
		{
			return false;
		}
		bEnabledOut = xPlayer.m_pxController->IsMovementEnabled();
		return true;
	}

	void FailUI(const char* szReason)
	{
		g_szUIFailure = szReason;
		g_bUIFailed = true;
		g_eUIPhase = UIPhase::Done;
	}

	void Setup_ZMMenuOpenClose()
	{
		g_eUIPhase                 = UIPhase::Done;
		g_iUIPhaseFrames           = 0;
		g_bUIActive                = false;
		g_bUIFailed                = false;
		g_szUIFailure              = "test did not reach verification";

		g_bUIMovementEnabledBefore = false;
		g_bUIMenuOpened            = false;
		g_bUIPlayerFrozenWhileOpen = false;
		g_uUIDepthWhileOpen        = 0u;
		g_eUITopWhileOpen          = (u_int)ZM_MENU_SCREEN_NONE;
		g_iUICursorOnOpen          = -99;
		g_iUICursorAfterNav        = -99;
		g_bUIMenuClosed            = false;
		g_bUIMovementReenabled     = false;
		g_bUIFocusClearedOnClose   = false;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO process
		// state (fixed dt, scene load) until every git-ignored input is confirmed
		// present. CI has no baked Assets tree -> skip.
		g_bUIPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bUIPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere assets absent -- run a *_True build");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fUI_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eUIPhase = UIPhase::AwaitReady;
		g_bUIActive = true;
	}

	bool Step_ZMMenuOpenClose(int)
	{
		if (!g_bUIActive || g_bUIFailed || g_eUIPhase == UIPhase::Done)
		{
			return false;
		}

		++g_iUIPhaseFrames;
		switch (g_eUIPhase)
		{
		case UIPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iUIPhaseFrames > iUI_READY_DEADLINE)
				{
					FailUI("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}

			// The pause menu is a persistent-scene singleton -- if it does not resolve
			// here, the persistent ZM_MenuRoot was never authored / loaded.
			if (ResolveSingletonMenuStack() == nullptr)
			{
				FailUI("no unique ZM_UI_MenuStack singleton (ZM_MenuRoot missing)");
				return false;
			}

			g_bUIMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();

			g_eUIPhase = UIPhase::Opening;
			g_iUIPhaseFrames = 0;
			return true;
		}

		case UIPhase::Opening:
		{
			// Resolve FRESH each frame (the pool relocates on swap-and-pop).
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailUI("the ZM_UI_MenuStack singleton stopped resolving while opening");
				return false;
			}
			if (pxMenu->IsOpen())
			{
				// Capture the open state (do NOT press M again -- an M press while open is
				// ignored by the component, but stopping here keeps the drive clean).
				g_bUIMenuOpened     = true;
				g_uUIDepthWhileOpen = pxMenu->GetDepth();
				g_eUITopWhileOpen   = (u_int)pxMenu->GetTopScreen();
				g_iUICursorOnOpen   = pxMenu->GetCursor();

				bool bEnabled = true;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bUIPlayerFrozenWhileOpen = !bEnabled;
				}

				g_eUIPhase = UIPhase::Navigating;
				g_iUIPhaseFrames = 0;
				return true;
			}
			if (g_iUIPhaseFrames > iUI_OPEN_DEADLINE)
			{
				FailUI("the menu never opened after the M press");
				return false;
			}
			// State-setter only: one edge press of the menu key (M) this frame. The
			// persistent ZM_UI_MenuStack::OnUpdate reads WasKeyPressedThisFrame + opens ROOT.
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		case UIPhase::Navigating:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailUI("the ZM_UI_MenuStack singleton stopped resolving while navigating");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				FailUI("the menu closed unexpectedly while navigating");
				return false;
			}
			// Advance the cursor Party(0) -> Bag(1). The engine's per-canvas focus-nav
			// runs AFTER this component's OnUpdate each frame, so the component's cursor
			// MIRROR trails the canvas focus by a frame; a conditional re-press would
			// therefore over-shoot. Press Down EXACTLY ONCE (on the first nav frame) --
			// one edge moves the focus exactly one entry -- then poll the mirror until it
			// settles on 1. Player movement is disabled while open, so the same Down key
			// never moves the parked player.
			const int iCursor = pxMenu->GetCursor();
			if (iCursor >= 1)
			{
				g_iUICursorAfterNav = iCursor;   // == 1: a single edge advances exactly one entry
				g_eUIPhase = UIPhase::Closing;
				g_iUIPhaseFrames = 0;
				return true;
			}
			if (g_iUIPhaseFrames == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			if (g_iUIPhaseFrames > iUI_NAV_DEADLINE)
			{
				g_iUICursorAfterNav = iCursor;
				FailUI("the focus cursor never advanced off the first entry");
				return false;
			}
			return true;
		}

		case UIPhase::Closing:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailUI("the ZM_UI_MenuStack singleton stopped resolving while closing");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bUIMenuClosed          = true;
				g_bUIFocusClearedOnClose = MenuFocusCleared();

				bool bEnabled = false;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bUIMovementReenabled = bEnabled;
				}

				g_eUIPhase = UIPhase::Done;
				return false;
			}
			if (g_iUIPhaseFrames > iUI_CLOSE_DEADLINE)
			{
				FailUI("the menu never closed after the Escape press");
				return false;
			}
			// One edge press of Escape this frame -- the component pops the ROOT, empties
			// the stack, unfreezes the player, and clears the canvas focus.
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			return true;
		}

		case UIPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMMenuOpenClose()
	{
		bool bPassed = true;

		if (g_bUIActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_MenuOpenClose] captured: failed=%s (%s) movBefore=%s opened=%s frozen=%s "
				"depth=%u (want 1) top=%u (want ROOT=%u) cursorOnOpen=%d (want 0) cursorAfterNav=%d "
				"(want 1) closed=%s movReenabled=%s focusCleared=%s",
				g_bUIFailed ? "true" : "false", g_szUIFailure,
				g_bUIMovementEnabledBefore ? "true" : "false",
				g_bUIMenuOpened ? "true" : "false",
				g_bUIPlayerFrozenWhileOpen ? "true" : "false",
				g_uUIDepthWhileOpen,
				g_eUITopWhileOpen, (u_int)ZM_MENU_SCREEN_ROOT,
				g_iUICursorOnOpen, g_iUICursorAfterNav,
				g_bUIMenuClosed ? "true" : "false",
				g_bUIMovementReenabled ? "true" : "false",
				g_bUIFocusClearedOnClose ? "true" : "false");

			if (g_bUIFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_MenuOpenClose] %s", g_szUIFailure);
				bPassed = false;
			}

			// The baseline must be meaningful: the player was movable before opening.
			if (!g_bUIMovementEnabledBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_MenuOpenClose] the player was not movable before opening -- the freeze "
					"assertion would be vacuous");
				bPassed = false;
			}

			// --- open: ROOT menu up, player frozen ---
			if (!g_bUIMenuOpened)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_MenuOpenClose] the menu never reported open after the M press");
				bPassed = false;
			}
			else
			{
				if (g_uUIDepthWhileOpen != 1u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_MenuOpenClose] stack depth while open was %u, expected 1 (ROOT only)",
						g_uUIDepthWhileOpen);
					bPassed = false;
				}
				if (g_eUITopWhileOpen != (u_int)ZM_MENU_SCREEN_ROOT)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_MenuOpenClose] top screen while open was %u, expected ROOT %u",
						g_eUITopWhileOpen, (u_int)ZM_MENU_SCREEN_ROOT);
					bPassed = false;
				}
				if (g_iUICursorOnOpen != 0)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_MenuOpenClose] focus cursor on open was %d, expected 0 (the first entry, "
						"Party)", g_iUICursorOnOpen);
					bPassed = false;
				}
				if (!g_bUIPlayerFrozenWhileOpen)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_MenuOpenClose] the player was NOT frozen while the menu was open");
					bPassed = false;
				}
			}

			// --- navigate: Down advances the focus cursor ---
			if (g_iUICursorAfterNav != 1)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_MenuOpenClose] focus cursor after Down was %d, expected 1 (Bag) -- the engine "
					"focus-nav did not advance the ROOT entry", g_iUICursorAfterNav);
				bPassed = false;
			}

			// --- close: menu down, player movable, focus cleared ---
			if (!g_bUIMenuClosed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_MenuOpenClose] the menu never closed after the Escape press");
				bPassed = false;
			}
			if (!g_bUIMovementReenabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_MenuOpenClose] player movement was not re-enabled after the menu closed");
				bPassed = false;
			}
			if (!g_bUIFocusClearedOnClose)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_MenuOpenClose] the canvas focus was not cleared (nullptr) after the menu "
					"closed -- arrow keys could still drive the hidden menu");
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded): force-close the menu on the live
		// singleton (skip-safe no-op if closed / absent) so a failed run cannot bleed an
		// open menu into the next batched test, drop the fixed timestep, restore FrontEnd,
		// then wipe input.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_bUIActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bUIActive = false;

		return bPassed || !g_bUIPrereqsPresent;
	}
}

static const Zenith_AutomatedTest g_xZMMenuOpenCloseTest = {
	"ZM_MenuOpenClose_Test",
	&Setup_ZMMenuOpenClose,
	&Step_ZMMenuOpenClose,
	&Verify_ZMMenuOpenClose,
	/* maxFrames */ 1200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMMenuOpenCloseTest);

#endif // ZENITH_INPUT_SIMULATOR
