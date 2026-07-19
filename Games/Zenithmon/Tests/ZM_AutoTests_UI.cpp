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
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"   // TryGetGameState (the live party the screen renders)
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"      // ZM_SPECIES_COUNT / ZM_GetSpeciesName (the dex gate)
#include "Zenithmon/Source/Party/ZM_Monster.h"         // ZM_Monster::m_eSpecies (the party lead's caught entry)
#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"
#include "Zenithmon/Source/UI/ZM_UI_Party.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_UI -- the windowed gates for the overworld pause menu (S6 item 2).
// TWO tests, both m_bRequiresGraphics = true:
//   * ZM_MenuOpenClose_Test (SC1) -- in a runtime-ready Dawnmere, press the menu
//     key (M) and assert the ROOT menu opens with the player FROZEN; press an
//     arrow (Down) and assert the focus cursor advances (engine focus-nav); press
//     Escape and assert the menu closes, player movement is re-enabled, and the
//     canvas focus is cleared.
//   * ZM_PartyScreen_Test (SC4) -- open the pause menu, confirm the focused Party
//     entry, and assert the PARTY screen really presents: the authored panel plus
//     exactly VisibleSlotCount(live party) slot widgets shown (the rest hidden), the
//     canvas focus parked on a party slot, confirm opening a non-empty summary,
//     Escape closing the summary WITHOUT leaving the screen, the next Escape
//     returning to ROOT, and the last one closing the menu and unfreezing the player.
//   * ZM_DexScreen_Test (SC5) -- open the pause menu, walk the ROOT focus down to
//     the Dex entry, confirm it, and assert the DEX screen really presents: the
//     authored panel / header / page buttons AND the RUNTIME-BUILT grid all resolve
//     by name, exactly VisibleCellCount(page 0) cells are shown, the header carries
//     the completion string, the party lead's species renders CAUGHT while an
//     uncaught entry renders with its name hidden, the ENGINE spatial focus-nav
//     walks down off the grid onto a page button, confirming Next advances the page,
//     and Escape returns to ROOT then closes the menu and unfreezes the player.
//   * ZM_DialogueTalk_Test (SC2) -- push two lines through
//     ZM_UI_MenuStack::TryPushDialogue and walk the whole conversation with spaced
//     Enter edges (complete the reveal, advance a line, close), asserting the
//     player freezes for the talk and is movable again afterwards, that Escape does
//     NOT dismiss the modal box, that the AUTHORED panel + text widgets are really
//     shown/typed/hidden, and that a second push STACKS on an open ROOT and returns
//     to it.
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

	// The persistent ZM_MenuRoot entity's UI component, resolved FRESH each call (the
	// pool relocates, so this is never cached across frames).
	Zenith_UIComponent* ResolveMenuRootUI()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}

	// The persistent ZM_MenuRoot canvas's currently-focused element (nullptr when
	// none). Read FRESH each call; used to prove focus is cleared on close.
	bool MenuFocusCleared()
	{
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return true;   // no menu root / UI -> vacuously "no focus held"
		}
		return pxUI->GetCanvas().GetFocusedElement() == nullptr;
	}

	// What the AUTHORED dialogue widgets actually report this frame. The model-only
	// assertions cannot see a typo'd element name, a missing AddStep_CreateUI*, or a
	// ZM_ConfigureMenuRoot FindElement that silently returned nullptr -- ZM_UI_DialogueBox
	// null-guards all three, so nothing would render while every model assertion stayed
	// green. This view is the on-screen half of the SC2 gate.
	struct DialogueElementView
	{
		bool        m_bResolved      = false;   // the ZM_MenuRoot UI component resolved
		bool        m_bPanelFound    = false;
		bool        m_bTextFound     = false;
		bool        m_bPanelVisible  = false;
		bool        m_bTextVisible   = false;
		std::string m_strText;                  // the element's text (want the pushed line)
		int         m_iVisibleGlyphs = -1;      // the element's revealed count (post-wrap)
		int         m_iTotalGlyphs   = -1;      // the element's post-wrap glyph total
	};

	DialogueElementView ReadDialogueElements()
	{
		DialogueElementView xView;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;

		Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_DialogueBox::szPANEL_NAME);
		Zenith_UI::Zenith_UIText* pxText =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_DialogueBox::szTEXT_NAME);
		xView.m_bPanelFound = (pxPanel != nullptr);
		xView.m_bTextFound  = (pxText != nullptr);
		if (pxPanel != nullptr)
		{
			xView.m_bPanelVisible = pxPanel->IsVisible();
		}
		if (pxText != nullptr)
		{
			xView.m_bTextVisible   = pxText->IsVisible();
			xView.m_strText        = pxText->GetText();
			xView.m_iVisibleGlyphs = pxText->GetVisibleGlyphCount();
			xView.m_iTotalGlyphs   = pxText->GetTotalGlyphCount();
		}
		return xView;
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

	// =========================================================================
	// ZM_DialogueTalk_Test (S6 item 2 SC2) -- the windowed gate for the dialogue
	// box. In a runtime-ready Dawnmere it walks BOTH push paths:
	//   1. from an EMPTY stack (an NPC talking with no pause menu open): push two
	//      lines through ZM_UI_MenuStack::TryPushDialogue (the S6 item 3 seam),
	//      assert the menu rose on the DIALOGUE screen with the player FROZEN, the
	//      AUTHORED panel + text elements shown and mid-reveal, that ESCAPE does NOT
	//      dismiss the modal box, then walk it with spaced Enter edges (press 1
	//      completes the reveal, press 2 advances to line 1, the rest close it) and
	//      assert the widgets are hidden again and the player is movable.
	//   2. STACKED on an open ROOT menu: open ROOT with M, push again, assert depth 2
	//      with DIALOGUE on top, read it out, and assert the stack returns to ROOT
	//      still open with the player still frozen -- then Escape out.
	//
	// It reuses this file's Dawnmere prerequisite guards / entity views / fixed dt
	// and keeps its OWN control + observation globals (the g_bUI* set belongs to the
	// sibling test). State-setters only -- never a reentrant StepFrame.
	// =========================================================================

	// The pushed conversation. Line 0 is deliberately LONG (59 glyphs > the 45
	// glyphs/sec reveal rate x the few frames before the first press), so the
	// typewriter is provably mid-reveal when the first Enter lands.
	constexpr const char* szDLG_LINE_0 = "Hello there! The tall grass is dangerous without a partner.";
	constexpr const char* szDLG_LINE_1 = "Take this Catchorb and be careful out there.";

	enum class DlgPhase
	{
		AwaitReady,
		Push,            // raise the dialogue via TryPushDialogue + capture the open state
		Typing,          // idle frames: the typewriter must be running but unfinished
		EscapeIgnored,   // ONE Escape edge -> the modal box must survive it untouched
		CompleteReveal,  // ONE Enter edge -> reveal completes, line index stays 0
		NextLine,        // ONE Enter edge -> line index advances to 1
		Finish,          // spaced Enter edges until the box closes
		OpenRoot,        // press M until the ROOT pause menu is up (the stacking fixture)
		PushOverRoot,    // push a SECOND conversation on top of ROOT -> depth 2
		FinishOverRoot,  // spaced Enter edges until the box pops back to ROOT
		CloseRoot,       // Escape -> the restored ROOT closes and the player is movable
		Done,
	};

	constexpr int iDLG_READY_DEADLINE  = 420;   // Dawnmere first-load ready window (parity with the sibling)
	constexpr int iDLG_TYPING_FRAMES   = 4;     // idle frames sampled mid-reveal (~0.13s -> 6 of 59 glyphs)
	constexpr int iDLG_PRESS_FRAME     = 2;     // the frame within a press phase that emits the edge
	constexpr int iDLG_SETTLE_FRAME    = 6;     // ...and the frame the outcome is sampled on
	constexpr int iDLG_FINISH_DEADLINE = 120;   // frames for the remaining presses to close the box
	constexpr int iDLG_OPEN_DEADLINE   = 120;   // frames for the M press to open the ROOT menu
	constexpr int iDLG_CLOSE_DEADLINE  = 120;   // frames for the final Escape to close the ROOT

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	DlgPhase g_eDlgPhase          = DlgPhase::Done;
	int      g_iDlgPhaseFrames    = 0;
	bool     g_bDlgPrereqsPresent = false;
	bool     g_bDlgActive         = false;
	bool     g_bDlgFailed         = false;
	const char* g_szDlgFailure    = "test did not reach verification";

	// ---- Captured observations ----
	bool  g_bDlgMovementEnabledBefore = false;   // player movable BEFORE the push (baseline)
	bool  g_bDlgPushAccepted          = false;   // TryPushDialogue returned true
	bool  g_bDlgMenuOpened            = false;   // the menu reported open after the push
	u_int g_eDlgTopWhileOpen          = (u_int)ZM_MENU_SCREEN_NONE;   // want DIALOGUE
	bool  g_bDlgActiveWhileOpen       = false;   // the dialogue model reported active
	bool  g_bDlgPlayerFrozen          = false;   // player movement disabled while talking
	int   g_iDlgVisibleWhileTyping    = -1;      // glyphs revealed mid-reveal (want > 0)
	int   g_iDlgTotalWhileTyping      = -1;      // the line's glyph total (want > the visible count)
	bool  g_bDlgRevealRunning         = false;   // reveal NOT complete while typing
	bool  g_bDlgRevealCompletedByPress = false;  // press 1 completed the reveal
	u_int g_uDlgIndexAfterReveal      = 99u;     // line index after press 1 (want 0)
	u_int g_uDlgIndexAfterAdvance     = 99u;     // line index after press 2 (want 1)
	bool  g_bDlgClosed                = false;   // the menu closed after the last press
	bool  g_bDlgInactiveOnClose       = false;   // the dialogue model is inactive after closing
	bool  g_bDlgMovementReenabled     = false;   // player movement re-enabled after closing

	// ---- The AUTHORED widgets (the on-screen half -- see DialogueElementView) ----
	DialogueElementView g_xDlgElementsWhileTyping;   // want: both found + visible, mid-reveal
	DialogueElementView g_xDlgElementsOnClose;       // want: both found + hidden

	// ---- Escape is ignored: a dialogue is modal ----
	bool  g_bDlgOpenAfterEscape       = false;   // the menu is STILL open after Escape
	u_int g_eDlgTopAfterEscape        = (u_int)ZM_MENU_SCREEN_NONE;   // want DIALOGUE
	u_int g_uDlgIndexAfterEscape      = 99u;     // want 0 (Escape consumed nothing)

	// ---- The second conversation, STACKED on an open ROOT ----
	bool  g_bDlgRootOpened            = false;   // ROOT came up on the M press
	u_int g_eDlgTopWithRoot           = (u_int)ZM_MENU_SCREEN_NONE;   // want DIALOGUE
	u_int g_uDlgDepthWithRoot         = 0u;      // want 2 (ROOT + DIALOGUE)
	bool  g_bDlgSecondPushAccepted    = false;   // TryPushDialogue accepted the stacked push
	bool  g_bDlgOpenAfterPop          = false;   // the menu is still open once the box pops
	u_int g_eDlgTopAfterPop           = (u_int)ZM_MENU_SCREEN_NONE;   // want ROOT (restored)
	bool  g_bDlgPlayerFrozenAfterPop  = false;   // ...and the player is still frozen by the ROOT
	bool  g_bDlgRootClosed            = false;   // the final Escape closed the ROOT
	bool  g_bDlgMovementReenabledFinal = false;  // ...and the player is movable again

	void FailDlg(const char* szReason)
	{
		g_szDlgFailure = szReason;
		g_bDlgFailed = true;
		g_eDlgPhase = DlgPhase::Done;
	}

	void Setup_ZMDialogueTalk()
	{
		g_eDlgPhase                 = DlgPhase::Done;
		g_iDlgPhaseFrames           = 0;
		g_bDlgActive                = false;
		g_bDlgFailed                = false;
		g_szDlgFailure              = "test did not reach verification";

		g_bDlgMovementEnabledBefore = false;
		g_bDlgPushAccepted          = false;
		g_bDlgMenuOpened            = false;
		g_eDlgTopWhileOpen          = (u_int)ZM_MENU_SCREEN_NONE;
		g_bDlgActiveWhileOpen       = false;
		g_bDlgPlayerFrozen          = false;
		g_iDlgVisibleWhileTyping    = -1;
		g_iDlgTotalWhileTyping      = -1;
		g_bDlgRevealRunning         = false;
		g_bDlgRevealCompletedByPress = false;
		g_uDlgIndexAfterReveal      = 99u;
		g_uDlgIndexAfterAdvance     = 99u;
		g_bDlgClosed                = false;
		g_bDlgInactiveOnClose       = false;
		g_bDlgMovementReenabled     = false;

		g_xDlgElementsWhileTyping   = DialogueElementView{};
		g_xDlgElementsOnClose       = DialogueElementView{};

		g_bDlgOpenAfterEscape       = false;
		g_eDlgTopAfterEscape        = (u_int)ZM_MENU_SCREEN_NONE;
		g_uDlgIndexAfterEscape      = 99u;

		g_bDlgRootOpened            = false;
		g_eDlgTopWithRoot           = (u_int)ZM_MENU_SCREEN_NONE;
		g_uDlgDepthWithRoot         = 0u;
		g_bDlgSecondPushAccepted    = false;
		g_bDlgOpenAfterPop          = false;
		g_eDlgTopAfterPop           = (u_int)ZM_MENU_SCREEN_NONE;
		g_bDlgPlayerFrozenAfterPop  = false;
		g_bDlgRootClosed            = false;
		g_bDlgMovementReenabledFinal = false;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO process
		// state (fixed dt, scene load) until every git-ignored input is confirmed
		// present. CI has no baked Assets tree -> skip.
		g_bDlgPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bDlgPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere assets absent -- run a *_True build");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fUI_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eDlgPhase = DlgPhase::AwaitReady;
		g_bDlgActive = true;
	}

	bool Step_ZMDialogueTalk(int)
	{
		if (!g_bDlgActive || g_bDlgFailed || g_eDlgPhase == DlgPhase::Done)
		{
			return false;
		}

		++g_iDlgPhaseFrames;
		switch (g_eDlgPhase)
		{
		case DlgPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iDlgPhaseFrames > iDLG_READY_DEADLINE)
				{
					FailDlg("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}
			if (ResolveSingletonMenuStack() == nullptr)
			{
				FailDlg("no unique ZM_UI_MenuStack singleton (ZM_MenuRoot missing)");
				return false;
			}

			g_bDlgMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();

			g_eDlgPhase = DlgPhase::Push;
			g_iDlgPhaseFrames = 0;
			return true;
		}

		case DlgPhase::Push:
		{
			// The NPC seam: a static singleton call, exactly what S6 item 3's
			// ZM_Interactable will make. Pushed ONCE, on the first frame of the phase.
			const char* aszLines[2] = { szDLG_LINE_0, szDLG_LINE_1 };
			g_bDlgPushAccepted = ZM_UI_MenuStack::TryPushDialogue(aszLines, 2u);
			if (!g_bDlgPushAccepted)
			{
				FailDlg("TryPushDialogue rejected the two-line conversation");
				return false;
			}

			// Resolve FRESH (the pool relocates on swap-and-pop).
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving after the push");
				return false;
			}
			g_bDlgMenuOpened      = pxMenu->IsOpen();
			g_eDlgTopWhileOpen    = (u_int)pxMenu->GetTopScreen();
			g_bDlgActiveWhileOpen = pxMenu->GetDialogue().IsActive();

			bool bEnabled = true;
			if (ReadActivePlayerMovementEnabled(bEnabled))
			{
				g_bDlgPlayerFrozen = !bEnabled;
			}

			g_eDlgPhase = DlgPhase::Typing;
			g_iDlgPhaseFrames = 0;
			return true;
		}

		case DlgPhase::Typing:
		{
			if (g_iDlgPhaseFrames < iDLG_TYPING_FRAMES)
			{
				return true;   // idle: let the typewriter run for a few fixed-dt frames
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving while typing");
				return false;
			}
			const ZM_UI_DialogueBox& xDialogue = pxMenu->GetDialogue();
			g_iDlgVisibleWhileTyping = xDialogue.GetVisibleGlyphCount();
			g_iDlgTotalWhileTyping   = xDialogue.GetCurrentLineGlyphTotal();
			g_bDlgRevealRunning      = !xDialogue.IsRevealComplete();
			// ...and what the AUTHORED widgets are actually showing this frame.
			g_xDlgElementsWhileTyping = ReadDialogueElements();

			g_eDlgPhase = DlgPhase::EscapeIgnored;
			g_iDlgPhaseFrames = 0;
			return true;
		}

		case DlgPhase::EscapeIgnored:
		{
			// A dialogue is MODAL: cancel is deliberately not routed while it is the top
			// screen, so this Escape must change nothing (were it routed, HandleCancel
			// would pop the box and close the menu outright).
			if (g_iDlgPhaseFrames == iDLG_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				return true;
			}
			if (g_iDlgPhaseFrames < iDLG_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving after the Escape press");
				return false;
			}
			g_bDlgOpenAfterEscape  = pxMenu->IsOpen();
			g_eDlgTopAfterEscape   = (u_int)pxMenu->GetTopScreen();
			g_uDlgIndexAfterEscape = pxMenu->GetDialogue().GetCurrentLineIndex();

			g_eDlgPhase = DlgPhase::CompleteReveal;
			g_iDlgPhaseFrames = 0;
			return true;
		}

		case DlgPhase::CompleteReveal:
		{
			// One Enter edge, with idle frames on either side so the edge-detected
			// ZM_InputActions::ReadConfirmPressed fires exactly once.
			if (g_iDlgPhaseFrames == iDLG_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iDlgPhaseFrames < iDLG_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving after the first press");
				return false;
			}
			const ZM_UI_DialogueBox& xDialogue = pxMenu->GetDialogue();
			g_bDlgRevealCompletedByPress = xDialogue.IsRevealComplete();
			g_uDlgIndexAfterReveal       = xDialogue.GetCurrentLineIndex();

			g_eDlgPhase = DlgPhase::NextLine;
			g_iDlgPhaseFrames = 0;
			return true;
		}

		case DlgPhase::NextLine:
		{
			if (g_iDlgPhaseFrames == iDLG_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iDlgPhaseFrames < iDLG_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving after the second press");
				return false;
			}
			g_uDlgIndexAfterAdvance = pxMenu->GetDialogue().GetCurrentLineIndex();

			g_eDlgPhase = DlgPhase::Finish;
			g_iDlgPhaseFrames = 0;
			return true;
		}

		case DlgPhase::Finish:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving while finishing");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bDlgClosed          = true;
				g_bDlgInactiveOnClose = !pxMenu->GetDialogue().IsActive();
				// CloseMenu hides the widgets directly (the per-frame Present stops running
				// the moment the stack empties), so they must be down here.
				g_xDlgElementsOnClose = ReadDialogueElements();

				bool bEnabled = false;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bDlgMovementReenabled = bEnabled;
				}

				g_eDlgPhase = DlgPhase::OpenRoot;
				g_iDlgPhaseFrames = 0;
				return true;
			}
			if (g_iDlgPhaseFrames > iDLG_FINISH_DEADLINE)
			{
				FailDlg("the dialogue never closed after the remaining Enter presses");
				return false;
			}
			// Spaced edges (one every fourth frame): line 1's reveal completes on the
			// first, and the last line is consumed on the next -- closing the box.
			if ((g_iDlgPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case DlgPhase::OpenRoot:
		{
			// Second half: the STACKING path. Bring up the ROOT pause menu first so the
			// next push lands ON TOP of a live screen instead of an empty stack.
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving while opening ROOT");
				return false;
			}
			if (pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bDlgRootOpened = true;
				g_eDlgPhase = DlgPhase::PushOverRoot;
				g_iDlgPhaseFrames = 0;
				return true;
			}
			if (g_iDlgPhaseFrames > iDLG_OPEN_DEADLINE)
			{
				FailDlg("the ROOT menu never opened after the M press");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		case DlgPhase::PushOverRoot:
		{
			const char* aszLines[2] = { szDLG_LINE_0, szDLG_LINE_1 };
			g_bDlgSecondPushAccepted = ZM_UI_MenuStack::TryPushDialogue(aszLines, 2u);
			if (!g_bDlgSecondPushAccepted)
			{
				FailDlg("TryPushDialogue rejected the conversation stacked on ROOT");
				return false;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving after the stacked push");
				return false;
			}
			g_eDlgTopWithRoot   = (u_int)pxMenu->GetTopScreen();
			g_uDlgDepthWithRoot = pxMenu->GetDepth();

			g_eDlgPhase = DlgPhase::FinishOverRoot;
			g_iDlgPhaseFrames = 0;
			return true;
		}

		case DlgPhase::FinishOverRoot:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving while reading the stacked box");
				return false;
			}
			// Stop pressing the FRAME the box pops -- another Enter would land on the
			// restored ROOT and confirm its focused entry.
			if (pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				g_bDlgOpenAfterPop = pxMenu->IsOpen();
				g_eDlgTopAfterPop  = (u_int)pxMenu->GetTopScreen();

				bool bEnabled = true;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bDlgPlayerFrozenAfterPop = !bEnabled;
				}

				g_eDlgPhase = DlgPhase::CloseRoot;
				g_iDlgPhaseFrames = 0;
				return true;
			}
			if (g_iDlgPhaseFrames > iDLG_FINISH_DEADLINE)
			{
				FailDlg("the stacked dialogue never popped back to ROOT");
				return false;
			}
			// Four spaced edges read both lines out (complete + advance, complete + close).
			if ((g_iDlgPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case DlgPhase::CloseRoot:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDlg("the ZM_UI_MenuStack singleton stopped resolving while closing ROOT");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bDlgRootClosed = true;

				bool bEnabled = false;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bDlgMovementReenabledFinal = bEnabled;
				}

				g_eDlgPhase = DlgPhase::Done;
				return false;
			}
			if (g_iDlgPhaseFrames > iDLG_CLOSE_DEADLINE)
			{
				FailDlg("the restored ROOT never closed after the Escape press");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			return true;
		}

		case DlgPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMDialogueTalk()
	{
		bool bPassed = true;

		if (g_bDlgActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DialogueTalk] captured: failed=%s (%s) movBefore=%s pushed=%s opened=%s "
				"top=%u (want DIALOGUE=%u) dlgActive=%s frozen=%s visible=%d total=%d running=%s "
				"revealDone=%s idxAfterReveal=%u (want 0) idxAfterAdvance=%u (want 1) closed=%s "
				"inactive=%s movReenabled=%s",
				g_bDlgFailed ? "true" : "false", g_szDlgFailure,
				g_bDlgMovementEnabledBefore ? "true" : "false",
				g_bDlgPushAccepted ? "true" : "false",
				g_bDlgMenuOpened ? "true" : "false",
				g_eDlgTopWhileOpen, (u_int)ZM_MENU_SCREEN_DIALOGUE,
				g_bDlgActiveWhileOpen ? "true" : "false",
				g_bDlgPlayerFrozen ? "true" : "false",
				g_iDlgVisibleWhileTyping, g_iDlgTotalWhileTyping,
				g_bDlgRevealRunning ? "true" : "false",
				g_bDlgRevealCompletedByPress ? "true" : "false",
				g_uDlgIndexAfterReveal, g_uDlgIndexAfterAdvance,
				g_bDlgClosed ? "true" : "false",
				g_bDlgInactiveOnClose ? "true" : "false",
				g_bDlgMovementReenabled ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DialogueTalk] widgets: typing(resolved=%s panel=%s/%s text=%s/%s "
				"glyphs=%d of %d text='%s') onClose(panel=%s/%s text=%s/%s) | escape(open=%s "
				"top=%u idx=%u) | stacked(rootOpen=%s top=%u depth=%u pushed=%s afterPop(open=%s "
				"top=%u frozen=%s) rootClosed=%s movReenabled=%s)",
				g_xDlgElementsWhileTyping.m_bResolved ? "true" : "false",
				g_xDlgElementsWhileTyping.m_bPanelFound ? "found" : "MISSING",
				g_xDlgElementsWhileTyping.m_bPanelVisible ? "visible" : "hidden",
				g_xDlgElementsWhileTyping.m_bTextFound ? "found" : "MISSING",
				g_xDlgElementsWhileTyping.m_bTextVisible ? "visible" : "hidden",
				g_xDlgElementsWhileTyping.m_iVisibleGlyphs,
				g_xDlgElementsWhileTyping.m_iTotalGlyphs,
				g_xDlgElementsWhileTyping.m_strText.c_str(),
				g_xDlgElementsOnClose.m_bPanelFound ? "found" : "MISSING",
				g_xDlgElementsOnClose.m_bPanelVisible ? "visible" : "hidden",
				g_xDlgElementsOnClose.m_bTextFound ? "found" : "MISSING",
				g_xDlgElementsOnClose.m_bTextVisible ? "visible" : "hidden",
				g_bDlgOpenAfterEscape ? "true" : "false",
				g_eDlgTopAfterEscape, g_uDlgIndexAfterEscape,
				g_bDlgRootOpened ? "true" : "false",
				g_eDlgTopWithRoot, g_uDlgDepthWithRoot,
				g_bDlgSecondPushAccepted ? "true" : "false",
				g_bDlgOpenAfterPop ? "true" : "false",
				g_eDlgTopAfterPop,
				g_bDlgPlayerFrozenAfterPop ? "true" : "false",
				g_bDlgRootClosed ? "true" : "false",
				g_bDlgMovementReenabledFinal ? "true" : "false");

			if (g_bDlgFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_DialogueTalk] %s", g_szDlgFailure);
				bPassed = false;
			}

			// The baseline must be meaningful: the player was movable before talking.
			if (!g_bDlgMovementEnabledBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the player was not movable before the push -- the freeze "
					"assertion would be vacuous");
				bPassed = false;
			}

			// --- push: the DIALOGUE screen is up and the player is frozen ---
			if (!g_bDlgPushAccepted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] TryPushDialogue rejected the conversation");
				bPassed = false;
			}
			if (!g_bDlgMenuOpened)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the menu did not open when the dialogue was pushed");
				bPassed = false;
			}
			if (g_eDlgTopWhileOpen != (u_int)ZM_MENU_SCREEN_DIALOGUE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] top screen while talking was %u, expected DIALOGUE %u",
					g_eDlgTopWhileOpen, (u_int)ZM_MENU_SCREEN_DIALOGUE);
				bPassed = false;
			}
			if (!g_bDlgActiveWhileOpen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the dialogue model was not active after the push");
				bPassed = false;
			}
			if (!g_bDlgPlayerFrozen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the player was NOT frozen while the dialogue was up");
				bPassed = false;
			}

			// --- typing: the typewriter is running but unfinished ---
			if (g_iDlgVisibleWhileTyping <= 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] no glyphs were revealed mid-reveal (visible=%d) -- the "
					"typewriter clock is not being ticked", g_iDlgVisibleWhileTyping);
				bPassed = false;
			}
			if (g_iDlgVisibleWhileTyping >= g_iDlgTotalWhileTyping)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the line was already fully revealed (%d of %d) when sampled "
					"-- the mid-reveal assertion would be vacuous",
					g_iDlgVisibleWhileTyping, g_iDlgTotalWhileTyping);
				bPassed = false;
			}
			if (!g_bDlgRevealRunning)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the reveal reported complete while still typing");
				bPassed = false;
			}

			// --- the AUTHORED widgets are really on screen mid-reveal ---
			if (!g_xDlgElementsWhileTyping.m_bResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the ZM_MenuRoot UI component did not resolve while typing");
				bPassed = false;
			}
			else
			{
				if (!g_xDlgElementsWhileTyping.m_bPanelFound
					|| !g_xDlgElementsWhileTyping.m_bTextFound)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DialogueTalk] the authored dialogue widgets did not resolve by name "
						"(panel '%s' found=%s, text '%s' found=%s) -- ZM_ConfigureMenuRoot / the "
						"AddStep_CreateUI* contract is broken and NOTHING would render",
						ZM_UI_DialogueBox::szPANEL_NAME,
						g_xDlgElementsWhileTyping.m_bPanelFound ? "true" : "false",
						ZM_UI_DialogueBox::szTEXT_NAME,
						g_xDlgElementsWhileTyping.m_bTextFound ? "true" : "false");
					bPassed = false;
				}
				if (!g_xDlgElementsWhileTyping.m_bPanelVisible
					|| !g_xDlgElementsWhileTyping.m_bTextVisible)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DialogueTalk] the dialogue widgets were not shown while the box was up "
						"(panel visible=%s, text visible=%s)",
						g_xDlgElementsWhileTyping.m_bPanelVisible ? "true" : "false",
						g_xDlgElementsWhileTyping.m_bTextVisible ? "true" : "false");
					bPassed = false;
				}
				if (g_xDlgElementsWhileTyping.m_strText != szDLG_LINE_0)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DialogueTalk] the text element reads '%s', expected the pushed line 0 '%s'",
						g_xDlgElementsWhileTyping.m_strText.c_str(), szDLG_LINE_0);
					bPassed = false;
				}
				// The ELEMENT's own reveal counter (driven off the post-wrap total) must be
				// strictly inside (0, total) -- proof Present is running the typewriter and
				// not just showing the whole line.
				if (g_xDlgElementsWhileTyping.m_iVisibleGlyphs <= 0
					|| g_xDlgElementsWhileTyping.m_iVisibleGlyphs
						>= g_xDlgElementsWhileTyping.m_iTotalGlyphs)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DialogueTalk] the text element's revealed glyph count was %d of %d -- "
						"expected a partial reveal (Present is not driving SetVisibleGlyphCount)",
						g_xDlgElementsWhileTyping.m_iVisibleGlyphs,
						g_xDlgElementsWhileTyping.m_iTotalGlyphs);
					bPassed = false;
				}
			}

			// --- Escape is IGNORED: the dialogue is modal ---
			if (!g_bDlgOpenAfterEscape
				|| g_eDlgTopAfterEscape != (u_int)ZM_MENU_SCREEN_DIALOGUE
				|| g_uDlgIndexAfterEscape != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] Escape disturbed the modal dialogue (open=%s top=%u want %u "
					"lineIndex=%u want 0) -- cancel must never dismiss a prompt",
					g_bDlgOpenAfterEscape ? "true" : "false",
					g_eDlgTopAfterEscape, (u_int)ZM_MENU_SCREEN_DIALOGUE,
					g_uDlgIndexAfterEscape);
				bPassed = false;
			}

			// --- press 1: completes the reveal WITHOUT advancing ---
			if (!g_bDlgRevealCompletedByPress)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the first Enter did not complete the reveal");
				bPassed = false;
			}
			if (g_uDlgIndexAfterReveal != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the line index after the first Enter was %u, expected 0 "
					"(completing a reveal must not consume the line)", g_uDlgIndexAfterReveal);
				bPassed = false;
			}

			// --- press 2: advances to the second line ---
			if (g_uDlgIndexAfterAdvance != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the line index after the second Enter was %u, expected 1",
					g_uDlgIndexAfterAdvance);
				bPassed = false;
			}

			// --- close: menu down, dialogue empty, player movable ---
			if (!g_bDlgClosed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the menu never closed after the remaining Enter presses");
				bPassed = false;
			}
			if (!g_bDlgInactiveOnClose)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the dialogue model was still active after the box closed");
				bPassed = false;
			}
			if (!g_bDlgMovementReenabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] player movement was not re-enabled after the dialogue closed");
				bPassed = false;
			}
			// The *Found flags are asserted too, not just the *Visible ones: a default-
			// initialised DialogueElementView has both Visible flags false, so a
			// "hidden" check alone would pass VACUOUSLY if the close phase never
			// captured. Requiring Found proves the capture actually ran.
			if (!g_xDlgElementsOnClose.m_bPanelFound
				|| !g_xDlgElementsOnClose.m_bTextFound
				|| g_xDlgElementsOnClose.m_bPanelVisible
				|| g_xDlgElementsOnClose.m_bTextVisible)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the dialogue widgets were not captured hidden after the box "
					"closed (panel found=%s visible=%s, text found=%s visible=%s)",
					g_xDlgElementsOnClose.m_bPanelFound ? "true" : "false",
					g_xDlgElementsOnClose.m_bPanelVisible ? "true" : "false",
					g_xDlgElementsOnClose.m_bTextFound ? "true" : "false",
					g_xDlgElementsOnClose.m_bTextVisible ? "true" : "false");
				bPassed = false;
			}

			// --- the second conversation, STACKED on an open ROOT ---
			if (!g_bDlgRootOpened)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the ROOT menu never opened for the stacking half of the test");
				bPassed = false;
			}
			if (!g_bDlgSecondPushAccepted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] TryPushDialogue rejected the conversation stacked on ROOT");
				bPassed = false;
			}
			if (g_eDlgTopWithRoot != (u_int)ZM_MENU_SCREEN_DIALOGUE || g_uDlgDepthWithRoot != 2u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the stacked push gave top=%u depth=%u, expected DIALOGUE %u "
					"at depth 2 (it must stack ON TOP of ROOT, not replace it)",
					g_eDlgTopWithRoot, g_uDlgDepthWithRoot, (u_int)ZM_MENU_SCREEN_DIALOGUE);
				bPassed = false;
			}
			if (!g_bDlgOpenAfterPop || g_eDlgTopAfterPop != (u_int)ZM_MENU_SCREEN_ROOT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] after the stacked box was read out the menu was open=%s on "
					"top=%u, expected the ROOT %u to be restored underneath it",
					g_bDlgOpenAfterPop ? "true" : "false",
					g_eDlgTopAfterPop, (u_int)ZM_MENU_SCREEN_ROOT);
				bPassed = false;
			}
			if (!g_bDlgPlayerFrozenAfterPop)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the player was unfrozen when the stacked dialogue popped -- "
					"the ROOT menu underneath still owns the freeze");
				bPassed = false;
			}
			if (!g_bDlgRootClosed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] the restored ROOT never closed after the final Escape");
				bPassed = false;
			}
			if (!g_bDlgMovementReenabledFinal)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DialogueTalk] player movement was not re-enabled after the ROOT finally closed");
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), exactly like the sibling test.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_bDlgActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bDlgActive = false;

		return bPassed || !g_bDlgPrereqsPresent;
	}

	// =========================================================================
	// ZM_PartyScreen_Test (S6 item 2 SC4) -- the windowed gate for the party
	// screen. In a runtime-ready Dawnmere it walks the whole screen:
	//   M -> ROOT is up -> Enter on the focused Party entry -> the PARTY screen is
	//   on top with the authored panel and exactly VisibleSlotCount(live party) slot
	//   widgets shown (every other slot hidden) and the canvas focus parked on one of
	//   them -> Enter -> the summary panel + a NON-EMPTY body are shown -> Escape ->
	//   the summary is gone but the screen is STILL PARTY -> Escape -> back on ROOT ->
	//   Escape -> the menu is closed and the player is movable again.
	//
	// The model-only assertions cannot see a typo'd element name, a missing
	// AddStep_CreateUI*, or a ZM_ConfigureMenuRoot FindElement that silently returned
	// nullptr -- ZM_UI_Party null-guards all three -- so the widget view below is the
	// on-screen half of the gate. It reuses this file's Dawnmere prerequisite guards /
	// entity views / fixed dt and keeps its OWN control + observation globals.
	// =========================================================================

	// What the AUTHORED party widgets actually report this frame.
	struct PartyElementView
	{
		bool        m_bResolved            = false;   // the ZM_MenuRoot UI component resolved
		bool        m_bPanelFound          = false;
		bool        m_bPanelVisible        = false;
		bool        m_abSlotFound[ZM_UI_Party::uMAX_SLOTS]   = {};
		bool        m_abSlotVisible[ZM_UI_Party::uMAX_SLOTS] = {};
		u_int       m_uSlotsFound          = 0u;
		u_int       m_uSlotsVisible        = 0u;
		bool        m_bSummaryPanelFound   = false;
		bool        m_bSummaryPanelVisible = false;
		bool        m_bSummaryTextFound    = false;
		bool        m_bSummaryTextVisible  = false;
		std::string m_strSummaryText;
		std::string m_strFocusedName;                 // the canvas focus (want a party slot)
	};

	PartyElementView ReadPartyElements()
	{
		PartyElementView xView;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;

		if (Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Party::szPANEL_NAME))
		{
			xView.m_bPanelFound = true;
			xView.m_bPanelVisible = pxPanel->IsVisible();
		}
		for (u_int u = 0u; u < ZM_UI_Party::uMAX_SLOTS; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxSlot =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Party::SlotElementName(u));
			if (pxSlot == nullptr)
			{
				continue;
			}
			xView.m_abSlotFound[u] = true;
			++xView.m_uSlotsFound;
			xView.m_abSlotVisible[u] = pxSlot->IsVisible();
			if (xView.m_abSlotVisible[u])
			{
				++xView.m_uSlotsVisible;
			}
		}
		if (Zenith_UI::Zenith_UIRect* pxSummaryPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Party::szSUMMARY_PANEL_NAME))
		{
			xView.m_bSummaryPanelFound = true;
			xView.m_bSummaryPanelVisible = pxSummaryPanel->IsVisible();
		}
		if (Zenith_UI::Zenith_UIText* pxSummaryText =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Party::szSUMMARY_TEXT_NAME))
		{
			xView.m_bSummaryTextFound = true;
			xView.m_bSummaryTextVisible = pxSummaryText->IsVisible();
			xView.m_strSummaryText = pxSummaryText->GetText();
		}
		if (Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement())
		{
			xView.m_strFocusedName = pxFocused->GetName();
		}
		return xView;
	}

	// The live party's member count (0 when no game state resolves).
	u_int ReadLivePartyCount()
	{
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return 0u;
		}
		return pxState->m_xParty.Count();
	}

	enum class PtyPhase
	{
		AwaitReady,
		OpenMenu,      // press M until the ROOT pause menu is up
		EnterParty,    // ONE Enter edge on the focused Party entry -> the PARTY screen
		OpenSummary,   // ONE Enter edge -> the member summary opens
		CloseSummary,  // ONE Escape edge -> the summary closes, the screen SURVIVES
		BackToRoot,    // ONE Escape edge -> the party screen pops back to ROOT
		CloseMenu,     // Escape edges until the menu closes
		Done,
	};

	constexpr int iPTY_READY_DEADLINE = 420;   // Dawnmere first-load ready window (sibling parity)
	constexpr int iPTY_OPEN_DEADLINE  = 120;   // frames for the M press to open the ROOT menu
	constexpr int iPTY_PRESS_FRAME    = 2;     // the frame within a press phase that emits the edge
	constexpr int iPTY_SETTLE_FRAME   = 6;     // ...and the frame the outcome is sampled on
	constexpr int iPTY_CLOSE_DEADLINE = 120;   // frames for the final Escape to close the menu

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	PtyPhase g_ePtyPhase          = PtyPhase::Done;
	int      g_iPtyPhaseFrames    = 0;
	bool     g_bPtyPrereqsPresent = false;
	bool     g_bPtyActive         = false;
	bool     g_bPtyFailed         = false;
	const char* g_szPtyFailure    = "test did not reach verification";

	// ---- Captured observations ----
	bool  g_bPtyMovementEnabledBefore = false;   // player movable BEFORE opening (baseline)
	bool  g_bPtyRootOpened            = false;   // ROOT came up on the M press
	u_int g_ePtyTopOnParty            = (u_int)ZM_MENU_SCREEN_NONE;   // want PARTY after the confirm
	u_int g_uPtyLivePartyCount        = 0u;      // the live party's member count
	u_int g_uPtyExpectedVisibleSlots  = 0u;      // == VisibleSlotCount(live count)
	int   g_iPtyFocusedSlotOnEnter    = -99;     // the focused element's slot index (want >= 0)
	bool  g_bPtySummaryOpenModel      = false;   // the screen model reports the summary open
	u_int g_ePtyTopWithSummary        = (u_int)ZM_MENU_SCREEN_NONE;   // want PARTY
	bool  g_bPtySummaryClosedModel    = false;   // ...and closed again after the first Escape
	u_int g_ePtyTopAfterSummaryEscape = (u_int)ZM_MENU_SCREEN_NONE;   // want PARTY (NOT popped)
	bool  g_bPtyOpenAfterBack         = false;   // the menu is still open after the second Escape
	u_int g_ePtyTopAfterBack          = (u_int)ZM_MENU_SCREEN_NONE;   // want ROOT
	bool  g_bPtyMenuClosed            = false;   // the final Escape closed the menu
	bool  g_bPtyMovementReenabled     = false;   // ...and the player is movable again
	bool  g_bPtyFocusClearedOnClose   = false;   // canvas focus == nullptr after close

	// ---- The AUTHORED widgets ----
	PartyElementView g_xPtyElementsOnEnter;    // want: panel + exactly the filled slots shown
	PartyElementView g_xPtyElementsWithSummary;// want: summary panel + non-empty body shown
	PartyElementView g_xPtyElementsAfterBack;  // want: everything party-side hidden again

	void FailPty(const char* szReason)
	{
		g_szPtyFailure = szReason;
		g_bPtyFailed = true;
		g_ePtyPhase = PtyPhase::Done;
	}

	void Setup_ZMPartyScreen()
	{
		g_ePtyPhase                 = PtyPhase::Done;
		g_iPtyPhaseFrames           = 0;
		g_bPtyActive                = false;
		g_bPtyFailed                = false;
		g_szPtyFailure              = "test did not reach verification";

		g_bPtyMovementEnabledBefore = false;
		g_bPtyRootOpened            = false;
		g_ePtyTopOnParty            = (u_int)ZM_MENU_SCREEN_NONE;
		g_uPtyLivePartyCount        = 0u;
		g_uPtyExpectedVisibleSlots  = 0u;
		g_iPtyFocusedSlotOnEnter    = -99;
		g_bPtySummaryOpenModel      = false;
		g_ePtyTopWithSummary        = (u_int)ZM_MENU_SCREEN_NONE;
		g_bPtySummaryClosedModel    = false;
		g_ePtyTopAfterSummaryEscape = (u_int)ZM_MENU_SCREEN_NONE;
		g_bPtyOpenAfterBack         = false;
		g_ePtyTopAfterBack          = (u_int)ZM_MENU_SCREEN_NONE;
		g_bPtyMenuClosed            = false;
		g_bPtyMovementReenabled     = false;
		g_bPtyFocusClearedOnClose   = false;

		g_xPtyElementsOnEnter       = PartyElementView{};
		g_xPtyElementsWithSummary   = PartyElementView{};
		g_xPtyElementsAfterBack     = PartyElementView{};

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO process
		// state (fixed dt, scene load) until every git-ignored input is confirmed
		// present. CI has no baked Assets tree -> skip.
		g_bPtyPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bPtyPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere assets absent -- run a *_True build");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fUI_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_ePtyPhase = PtyPhase::AwaitReady;
		g_bPtyActive = true;
	}

	bool Step_ZMPartyScreen(int)
	{
		if (!g_bPtyActive || g_bPtyFailed || g_ePtyPhase == PtyPhase::Done)
		{
			return false;
		}

		++g_iPtyPhaseFrames;
		switch (g_ePtyPhase)
		{
		case PtyPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iPtyPhaseFrames > iPTY_READY_DEADLINE)
				{
					FailPty("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}
			if (ResolveSingletonMenuStack() == nullptr)
			{
				FailPty("no unique ZM_UI_MenuStack singleton (ZM_MenuRoot missing)");
				return false;
			}

			g_bPtyMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();
			// The live party is what the screen must render -- capture it before opening.
			g_uPtyLivePartyCount       = ReadLivePartyCount();
			g_uPtyExpectedVisibleSlots = ZM_UI_Party::VisibleSlotCount(g_uPtyLivePartyCount);

			g_ePtyPhase = PtyPhase::OpenMenu;
			g_iPtyPhaseFrames = 0;
			return true;
		}

		case PtyPhase::OpenMenu:
		{
			// Resolve FRESH each frame (the pool relocates on swap-and-pop).
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailPty("the ZM_UI_MenuStack singleton stopped resolving while opening");
				return false;
			}
			if (pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bPtyRootOpened = true;
				g_ePtyPhase = PtyPhase::EnterParty;
				g_iPtyPhaseFrames = 0;
				return true;
			}
			if (g_iPtyPhaseFrames > iPTY_OPEN_DEADLINE)
			{
				FailPty("the ROOT menu never opened after the M press");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		case PtyPhase::EnterParty:
		{
			// ONE Enter edge, with idle frames on either side so the edge-detected
			// ZM_InputActions::ReadConfirmPressed fires exactly once. The ROOT opens with
			// the Party entry focused (cursor 0), so this confirms Party.
			if (g_iPtyPhaseFrames == iPTY_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iPtyPhaseFrames < iPTY_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailPty("the ZM_UI_MenuStack singleton stopped resolving after the Party confirm");
				return false;
			}
			g_ePtyTopOnParty      = (u_int)pxMenu->GetTopScreen();
			g_xPtyElementsOnEnter = ReadPartyElements();
			g_iPtyFocusedSlotOnEnter = ZM_UI_Party::SlotIndexFromElementName(
				g_xPtyElementsOnEnter.m_strFocusedName.c_str());

			g_ePtyPhase = PtyPhase::OpenSummary;
			g_iPtyPhaseFrames = 0;
			return true;
		}

		case PtyPhase::OpenSummary:
		{
			if (g_iPtyPhaseFrames == iPTY_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iPtyPhaseFrames < iPTY_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailPty("the ZM_UI_MenuStack singleton stopped resolving after the summary confirm");
				return false;
			}
			g_bPtySummaryOpenModel    = pxMenu->GetPartyScreen().IsSummaryOpen();
			g_ePtyTopWithSummary      = (u_int)pxMenu->GetTopScreen();
			g_xPtyElementsWithSummary = ReadPartyElements();

			g_ePtyPhase = PtyPhase::CloseSummary;
			g_iPtyPhaseFrames = 0;
			return true;
		}

		case PtyPhase::CloseSummary:
		{
			// The party screen CONSUMES this Escape: the summary closes and the screen
			// stays on top (were the cancel routed straight to the stack, the screen
			// would pop here and the ROOT would already be back).
			if (g_iPtyPhaseFrames == iPTY_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				return true;
			}
			if (g_iPtyPhaseFrames < iPTY_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailPty("the ZM_UI_MenuStack singleton stopped resolving after the summary Escape");
				return false;
			}
			g_bPtySummaryClosedModel    = !pxMenu->GetPartyScreen().IsSummaryOpen();
			g_ePtyTopAfterSummaryEscape = (u_int)pxMenu->GetTopScreen();

			g_ePtyPhase = PtyPhase::BackToRoot;
			g_iPtyPhaseFrames = 0;
			return true;
		}

		case PtyPhase::BackToRoot:
		{
			if (g_iPtyPhaseFrames == iPTY_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				return true;
			}
			if (g_iPtyPhaseFrames < iPTY_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailPty("the ZM_UI_MenuStack singleton stopped resolving after the second Escape");
				return false;
			}
			g_bPtyOpenAfterBack     = pxMenu->IsOpen();
			g_ePtyTopAfterBack      = (u_int)pxMenu->GetTopScreen();
			g_xPtyElementsAfterBack = ReadPartyElements();

			g_ePtyPhase = PtyPhase::CloseMenu;
			g_iPtyPhaseFrames = 0;
			return true;
		}

		case PtyPhase::CloseMenu:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailPty("the ZM_UI_MenuStack singleton stopped resolving while closing");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bPtyMenuClosed          = true;
				g_bPtyFocusClearedOnClose = MenuFocusCleared();

				bool bEnabled = false;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bPtyMovementReenabled = bEnabled;
				}

				g_ePtyPhase = PtyPhase::Done;
				return false;
			}
			if (g_iPtyPhaseFrames > iPTY_CLOSE_DEADLINE)
			{
				FailPty("the ROOT menu never closed after the final Escape press");
				return false;
			}
			// Spaced edges (one every fourth frame) so each press is a clean edge.
			if ((g_iPtyPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			}
			return true;
		}

		case PtyPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMPartyScreen()
	{
		bool bPassed = true;

		if (g_bPtyActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_PartyScreen] captured: failed=%s (%s) movBefore=%s rootOpened=%s "
				"topOnParty=%u (want PARTY=%u) partyCount=%u wantVisibleSlots=%u focusedSlot=%d "
				"summaryOpen=%s topWithSummary=%u summaryClosed=%s topAfterSummaryEscape=%u "
				"openAfterBack=%s topAfterBack=%u (want ROOT=%u) closed=%s movReenabled=%s "
				"focusCleared=%s",
				g_bPtyFailed ? "true" : "false", g_szPtyFailure,
				g_bPtyMovementEnabledBefore ? "true" : "false",
				g_bPtyRootOpened ? "true" : "false",
				g_ePtyTopOnParty, (u_int)ZM_MENU_SCREEN_PARTY,
				g_uPtyLivePartyCount, g_uPtyExpectedVisibleSlots,
				g_iPtyFocusedSlotOnEnter,
				g_bPtySummaryOpenModel ? "true" : "false",
				g_ePtyTopWithSummary,
				g_bPtySummaryClosedModel ? "true" : "false",
				g_ePtyTopAfterSummaryEscape,
				g_bPtyOpenAfterBack ? "true" : "false",
				g_ePtyTopAfterBack, (u_int)ZM_MENU_SCREEN_ROOT,
				g_bPtyMenuClosed ? "true" : "false",
				g_bPtyMovementReenabled ? "true" : "false",
				g_bPtyFocusClearedOnClose ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_PartyScreen] widgets: onEnter(resolved=%s panel=%s/%s slotsFound=%u "
				"slotsVisible=%u focus='%s') summary(panel=%s/%s text=%s/%s body='%s') "
				"afterBack(panel=%s slotsVisible=%u summaryPanel=%s)",
				g_xPtyElementsOnEnter.m_bResolved ? "true" : "false",
				g_xPtyElementsOnEnter.m_bPanelFound ? "found" : "MISSING",
				g_xPtyElementsOnEnter.m_bPanelVisible ? "visible" : "hidden",
				g_xPtyElementsOnEnter.m_uSlotsFound,
				g_xPtyElementsOnEnter.m_uSlotsVisible,
				g_xPtyElementsOnEnter.m_strFocusedName.c_str(),
				g_xPtyElementsWithSummary.m_bSummaryPanelFound ? "found" : "MISSING",
				g_xPtyElementsWithSummary.m_bSummaryPanelVisible ? "visible" : "hidden",
				g_xPtyElementsWithSummary.m_bSummaryTextFound ? "found" : "MISSING",
				g_xPtyElementsWithSummary.m_bSummaryTextVisible ? "visible" : "hidden",
				g_xPtyElementsWithSummary.m_strSummaryText.c_str(),
				g_xPtyElementsAfterBack.m_bPanelVisible ? "visible" : "hidden",
				g_xPtyElementsAfterBack.m_uSlotsVisible,
				g_xPtyElementsAfterBack.m_bSummaryPanelVisible ? "visible" : "hidden");

			if (g_bPtyFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_PartyScreen] %s", g_szPtyFailure);
				bPassed = false;
			}

			// The baseline must be meaningful: the player was movable before opening, and
			// the live party actually holds a member (otherwise every slot / summary
			// assertion below would pass vacuously on an empty list).
			if (!g_bPtyMovementEnabledBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the player was not movable before opening -- the freeze "
					"assertions would be vacuous");
				bPassed = false;
			}
			if (g_uPtyExpectedVisibleSlots == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the live party is EMPTY (count=%u) -- the slot and summary "
					"assertions would be vacuous", g_uPtyLivePartyCount);
				bPassed = false;
			}

			if (!g_bPtyRootOpened)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the ROOT menu never opened after the M press");
				bPassed = false;
			}

			// --- confirm on Party: the PARTY screen is on top and really presented ---
			if (g_ePtyTopOnParty != (u_int)ZM_MENU_SCREEN_PARTY)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] top screen after confirming Party was %u, expected PARTY %u",
					g_ePtyTopOnParty, (u_int)ZM_MENU_SCREEN_PARTY);
				bPassed = false;
			}
			if (!g_xPtyElementsOnEnter.m_bResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the ZM_MenuRoot UI component did not resolve on the party screen");
				bPassed = false;
			}
			else
			{
				if (!g_xPtyElementsOnEnter.m_bPanelFound
					|| g_xPtyElementsOnEnter.m_uSlotsFound != ZM_UI_Party::uMAX_SLOTS
					|| !g_xPtyElementsOnEnter.m_bSummaryPanelFound
					|| !g_xPtyElementsOnEnter.m_bSummaryTextFound)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_PartyScreen] the authored party widgets did not all resolve by name "
						"(panel '%s' found=%s, slots found=%u of %u, summary panel found=%s, summary "
						"text found=%s) -- ZM_ConfigureMenuRoot / the AddStep_CreateUI* contract is "
						"broken and NOTHING would render",
						ZM_UI_Party::szPANEL_NAME,
						g_xPtyElementsOnEnter.m_bPanelFound ? "true" : "false",
						g_xPtyElementsOnEnter.m_uSlotsFound, ZM_UI_Party::uMAX_SLOTS,
						g_xPtyElementsOnEnter.m_bSummaryPanelFound ? "true" : "false",
						g_xPtyElementsOnEnter.m_bSummaryTextFound ? "true" : "false");
					bPassed = false;
				}
				if (!g_xPtyElementsOnEnter.m_bPanelVisible)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_PartyScreen] the party panel was not shown on the party screen");
					bPassed = false;
				}
				// EXACTLY the filled slots are visible: a hidden slot must stay hidden (and
				// non-focusable), or the nav could park on an empty row.
				if (g_xPtyElementsOnEnter.m_uSlotsVisible != g_uPtyExpectedVisibleSlots)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_PartyScreen] %u slot widgets were visible, expected %u (the live party "
						"holds %u member(s))",
						g_xPtyElementsOnEnter.m_uSlotsVisible, g_uPtyExpectedVisibleSlots,
						g_uPtyLivePartyCount);
					bPassed = false;
				}
				for (u_int u = 0u; u < ZM_UI_Party::uMAX_SLOTS; ++u)
				{
					const bool bWantVisible = (u < g_uPtyExpectedVisibleSlots);
					if (g_xPtyElementsOnEnter.m_abSlotVisible[u] != bWantVisible)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_PartyScreen] slot %u visible=%s, expected %s",
							u,
							g_xPtyElementsOnEnter.m_abSlotVisible[u] ? "true" : "false",
							bWantVisible ? "true" : "false");
						bPassed = false;
					}
				}
				// The summary must NOT be up before it is asked for.
				if (g_xPtyElementsOnEnter.m_bSummaryPanelVisible
					|| g_xPtyElementsOnEnter.m_bSummaryTextVisible)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_PartyScreen] the summary was already showing when the list opened "
						"(panel visible=%s, text visible=%s)",
						g_xPtyElementsOnEnter.m_bSummaryPanelVisible ? "true" : "false",
						g_xPtyElementsOnEnter.m_bSummaryTextVisible ? "true" : "false");
					bPassed = false;
				}
			}
			if (g_iPtyFocusedSlotOnEnter < 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the canvas focus was '%s', expected one of the party slots -- "
					"the screen must park the focus on a visible member or the arrow keys drive "
					"nothing", g_xPtyElementsOnEnter.m_strFocusedName.c_str());
				bPassed = false;
			}

			// --- confirm again: the member summary opens with a real body ---
			if (!g_bPtySummaryOpenModel)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the summary did not open on the second confirm");
				bPassed = false;
			}
			if (g_ePtyTopWithSummary != (u_int)ZM_MENU_SCREEN_PARTY)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] opening the summary changed the top screen to %u -- it must "
					"stay on PARTY %u", g_ePtyTopWithSummary, (u_int)ZM_MENU_SCREEN_PARTY);
				bPassed = false;
			}
			if (!g_xPtyElementsWithSummary.m_bSummaryPanelVisible
				|| !g_xPtyElementsWithSummary.m_bSummaryTextVisible)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the summary widgets were not shown (panel visible=%s, text "
					"visible=%s)",
					g_xPtyElementsWithSummary.m_bSummaryPanelVisible ? "true" : "false",
					g_xPtyElementsWithSummary.m_bSummaryTextVisible ? "true" : "false");
				bPassed = false;
			}
			if (g_xPtyElementsWithSummary.m_strSummaryText.empty())
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the summary text element is EMPTY -- Present is not writing "
					"the formatted body");
				bPassed = false;
			}

			// --- Escape 1: the SCREEN consumes it (summary closes, PARTY survives) ---
			if (!g_bPtySummaryClosedModel)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the summary was still open after the first Escape");
				bPassed = false;
			}
			if (g_ePtyTopAfterSummaryEscape != (u_int)ZM_MENU_SCREEN_PARTY)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the first Escape left top=%u, expected PARTY %u -- an open "
					"summary must SWALLOW the cancel instead of popping the screen",
					g_ePtyTopAfterSummaryEscape, (u_int)ZM_MENU_SCREEN_PARTY);
				bPassed = false;
			}

			// --- Escape 2: back on ROOT with the party widgets down ---
			if (!g_bPtyOpenAfterBack || g_ePtyTopAfterBack != (u_int)ZM_MENU_SCREEN_ROOT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] after the second Escape the menu was open=%s on top=%u, "
					"expected the ROOT %u to be restored",
					g_bPtyOpenAfterBack ? "true" : "false",
					g_ePtyTopAfterBack, (u_int)ZM_MENU_SCREEN_ROOT);
				bPassed = false;
			}
			// The *Found flags prove the capture actually ran (a default-initialised view
			// would pass the "hidden" checks vacuously).
			if (!g_xPtyElementsAfterBack.m_bPanelFound
				|| g_xPtyElementsAfterBack.m_bPanelVisible
				|| g_xPtyElementsAfterBack.m_uSlotsVisible != 0u
				|| g_xPtyElementsAfterBack.m_bSummaryPanelVisible
				|| g_xPtyElementsAfterBack.m_bSummaryTextVisible)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the party widgets were not all hidden once the screen popped "
					"(panel found=%s visible=%s, slots visible=%u, summary panel visible=%s, summary "
					"text visible=%s)",
					g_xPtyElementsAfterBack.m_bPanelFound ? "true" : "false",
					g_xPtyElementsAfterBack.m_bPanelVisible ? "true" : "false",
					g_xPtyElementsAfterBack.m_uSlotsVisible,
					g_xPtyElementsAfterBack.m_bSummaryPanelVisible ? "true" : "false",
					g_xPtyElementsAfterBack.m_bSummaryTextVisible ? "true" : "false");
				bPassed = false;
			}

			// --- Escape 3: the menu closes and the player is movable again ---
			if (!g_bPtyMenuClosed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the menu never closed after the final Escape press");
				bPassed = false;
			}
			if (!g_bPtyMovementReenabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] player movement was not re-enabled after the menu closed");
				bPassed = false;
			}
			if (!g_bPtyFocusClearedOnClose)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_PartyScreen] the canvas focus was not cleared (nullptr) after the menu "
					"closed -- arrow keys could still drive the hidden menu");
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), exactly like the sibling tests.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_bPtyActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bPtyActive = false;

		return bPassed || !g_bPtyPrereqsPresent;
	}

	// =========================================================================
	// ZM_DexScreen_Test (S6 item 2 SC5) -- the windowed gate for the dex screen,
	// and the first on-screen proof of the E4 Zenith_UIGridLayoutGroup. In a
	// runtime-ready Dawnmere:
	//   M -> ROOT -> Down, Down (the ROOT cursor walks Party -> Bag -> Dex) -> Enter
	//   -> the DEX screen is on top with the authored panel / header / page buttons
	//   AND the RUNTIME-BUILT grid resolving by name, exactly VisibleCellCount(page 0)
	//   cells visible, the header carrying the completion string, the party lead
	//   rendering CAUGHT and an uncaught entry rendering with its name hidden, and the
	//   canvas focus parked on a cell -> spaced Downs walk the ENGINE spatial nav off
	//   the grid onto a page button -> confirm Next -> the page advances -> Escape ->
	//   back on ROOT with every dex widget hidden -> Escape -> the menu closes and the
	//   player is movable again.
	//
	// The model-only assertions cannot see a typo'd element name, a missing
	// AddStep_CreateUI*, a ZM_ConfigureMenuRoot FindElement that silently returned
	// nullptr, or a grid whose cells were never parented -- ZM_UI_Dex null-guards all
	// of it -- so the widget view below is the on-screen half of the gate. It reuses
	// this file's Dawnmere prerequisite guards / entity views / fixed dt and keeps its
	// OWN control + observation globals.
	// =========================================================================

	// What the AUTHORED + RUNTIME-BUILT dex widgets actually report this frame.
	struct DexElementView
	{
		bool        m_bResolved      = false;   // the ZM_MenuRoot UI component resolved
		bool        m_bPanelFound    = false;
		bool        m_bPanelVisible  = false;
		bool        m_bGridFound     = false;   // the RUNTIME-built grid
		bool        m_bGridVisible   = false;
		bool        m_bHeaderFound   = false;
		bool        m_bHeaderVisible = false;
		std::string m_strHeaderText;
		bool        m_bPrevFound     = false;
		bool        m_bPrevVisible   = false;
		bool        m_bNextFound     = false;
		bool        m_bNextVisible   = false;
		u_int       m_uCellsFound    = 0u;
		u_int       m_uCellsVisible  = 0u;
		bool        m_abCellVisible[ZM_UI_Dex::uCELL_COUNT] = {};
		std::string m_astrCellText[ZM_UI_Dex::uCELL_COUNT];
		std::string m_strFocusedName;           // the canvas focus (want a cell, then a page button)
	};

	DexElementView ReadDexElements()
	{
		DexElementView xView;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;

		if (Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Dex::szPANEL_NAME))
		{
			xView.m_bPanelFound = true;
			xView.m_bPanelVisible = pxPanel->IsVisible();
		}
		// Resolved through the BASE element type: the grid is created at runtime, so a
		// name that resolves at all is the proof the build-once routine really ran.
		if (Zenith_UI::Zenith_UIElement* pxGrid = pxUI->FindElement(ZM_UI_Dex::szGRID_NAME))
		{
			xView.m_bGridFound = true;
			xView.m_bGridVisible = pxGrid->IsVisible();
		}
		if (Zenith_UI::Zenith_UIText* pxHeader =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Dex::szHEADER_NAME))
		{
			xView.m_bHeaderFound = true;
			xView.m_bHeaderVisible = pxHeader->IsVisible();
			xView.m_strHeaderText = pxHeader->GetText();
		}
		if (Zenith_UI::Zenith_UIElement* pxPrev = pxUI->FindElement(ZM_UI_Dex::szPREV_NAME))
		{
			xView.m_bPrevFound = true;
			xView.m_bPrevVisible = pxPrev->IsVisible();
		}
		if (Zenith_UI::Zenith_UIElement* pxNext = pxUI->FindElement(ZM_UI_Dex::szNEXT_NAME))
		{
			xView.m_bNextFound = true;
			xView.m_bNextVisible = pxNext->IsVisible();
		}
		for (u_int u = 0u; u < ZM_UI_Dex::uCELL_COUNT; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxCell =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Dex::CellElementName(u));
			if (pxCell == nullptr)
			{
				continue;
			}
			++xView.m_uCellsFound;
			xView.m_abCellVisible[u] = pxCell->IsVisible();
			xView.m_astrCellText[u] = pxCell->GetText();
			if (xView.m_abCellVisible[u])
			{
				++xView.m_uCellsVisible;
			}
		}
		if (Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement())
		{
			xView.m_strFocusedName = pxFocused->GetName();
		}
		return xView;
	}

	// True while the named element is one of the two page buttons.
	bool IsDexPageButtonName(const std::string& strName)
	{
		return strName == ZM_UI_Dex::szPREV_NAME || strName == ZM_UI_Dex::szNEXT_NAME;
	}

	enum class DexPhase
	{
		AwaitReady,
		OpenMenu,          // press M until the ROOT pause menu is up
		NavToBag,          // ONE Down edge -> the ROOT cursor advances Party(0) -> Bag(1)
		NavToDex,          // ONE Down edge -> Bag(1) -> Dex(2)
		EnterDex,          // ONE Enter edge -> the DEX screen
		WalkToPageButton,  // spaced Down edges: the ENGINE spatial nav walks off the grid
		ConfirmNextPage,   // park the focus on Next, ONE Enter edge -> the page advances
		BackToRoot,        // ONE Escape edge -> the dex pops back to ROOT
		CloseMenu,         // Escape edges until the menu closes
		Done,
	};

	constexpr int iDEX_READY_DEADLINE = 420;   // Dawnmere first-load ready window (sibling parity)
	constexpr int iDEX_OPEN_DEADLINE  = 120;   // frames for the M press to open the ROOT menu
	constexpr int iDEX_NAV_DEADLINE   = 120;   // frames for a Down press to advance the ROOT cursor
	constexpr int iDEX_PRESS_FRAME    = 2;     // the frame within a press phase that emits the edge
	constexpr int iDEX_SETTLE_FRAME   = 6;     // ...and the frame the outcome is sampled on
	constexpr int iDEX_FOCUS_FRAME    = 1;     // the frame ConfirmNextPage parks the focus on Next
	constexpr int iDEX_LATE_PRESS     = 4;     // ...and the (later) frame it presses Enter
	constexpr int iDEX_LATE_SETTLE    = 10;    // ...and the frame it samples the paged outcome
	constexpr int iDEX_WALK_DEADLINE  = 200;   // frames for the spatial nav to reach a page button
	constexpr int iDEX_CLOSE_DEADLINE = 120;   // frames for the final Escape to close the menu

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	DexPhase g_eDexPhase          = DexPhase::Done;
	int      g_iDexPhaseFrames    = 0;
	bool     g_bDexPrereqsPresent = false;
	bool     g_bDexActive         = false;
	bool     g_bDexFailed         = false;
	const char* g_szDexFailure    = "test did not reach verification";

	// ---- Captured observations ----
	bool  g_bDexMovementEnabledBefore = false;   // player movable BEFORE opening (baseline)
	bool  g_bDexRootOpened            = false;   // ROOT came up on the M press
	int   g_iDexRootCursorOnConfirm   = -99;     // the ROOT cursor when Dex was confirmed (want 2)
	u_int g_eDexTopOnEnter            = (u_int)ZM_MENU_SCREEN_NONE;   // want DEX
	u_int g_uDexExpectedVisibleCells  = 0u;      // == VisibleCellCount(page 0, ZM_SPECIES_COUNT)
	u_int g_uDexCaughtCount           = 0u;      // the live caught-set size
	// The party lead (caught) and a deliberately UNCAUGHT entry, both on page 0.
	u_int g_eDexLeadSpecies           = (u_int)ZM_SPECIES_COUNT;
	int   g_iDexLeadCell              = -1;
	u_int g_eDexUncaughtSpecies       = (u_int)ZM_SPECIES_COUNT;
	int   g_iDexUncaughtCell          = -1;
	int   g_iDexCursorOnEnter         = -99;     // the screen's focused-cell mirror (want >= 0)
	bool  g_bDexNavReachedPageButton  = false;   // the spatial nav walked off the grid
	std::string g_strDexNavFocusName;            // ...onto this element
	bool  g_bDexNextFocusParked       = false;   // the test parked the focus on Next
	int   g_iDexPageBeforeNext        = -99;     // want 0
	int   g_iDexPageAfterNext         = -99;     // want 1
	bool  g_bDexOpenAfterBack         = false;   // the menu is still open after the Escape
	u_int g_eDexTopAfterBack          = (u_int)ZM_MENU_SCREEN_NONE;   // want ROOT
	bool  g_bDexMenuClosed            = false;   // the final Escape closed the menu
	bool  g_bDexMovementReenabled     = false;   // ...and the player is movable again
	bool  g_bDexFocusClearedOnClose   = false;   // canvas focus == nullptr after close

	// ---- The AUTHORED + runtime-built widgets ----
	DexElementView g_xDexElementsOnEnter;    // want: panel + grid + header + every page-0 cell shown
	DexElementView g_xDexElementsAfterNext;  // want: the page-1 labels
	DexElementView g_xDexElementsAfterBack;  // want: everything dex-side hidden again

	void FailDex(const char* szReason)
	{
		g_szDexFailure = szReason;
		g_bDexFailed = true;
		g_eDexPhase = DexPhase::Done;
	}

	// Pick the party lead's (caught) page-0 cell and the first page-0 cell whose species
	// is NOT caught, so the caught/hidden label assertions are anchored on real state.
	void CaptureDexFixtureSpecies()
	{
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return;
		}
		g_uDexCaughtCount = pxState->GetCaughtCount();

		if (pxState->m_xParty.Count() > 0u)
		{
			const ZM_SPECIES_ID eLead = pxState->m_xParty.Get(0u).m_eSpecies;
			g_eDexLeadSpecies = (u_int)eLead;
			// Page 0 cell u shows species u, so a lead inside the first page has a cell.
			if (pxState->IsCaught(eLead) && (u_int)eLead < ZM_UI_Dex::uCELL_COUNT)
			{
				g_iDexLeadCell = (int)eLead;
			}
		}
		for (u_int u = 0u; u < ZM_UI_Dex::uCELL_COUNT; ++u)
		{
			const int iSpecies = ZM_UI_Dex::SpeciesIndexForCell(
				0u, u, (u_int)ZM_SPECIES_COUNT);
			if (iSpecies < 0)
			{
				continue;
			}
			if (!pxState->IsCaught((ZM_SPECIES_ID)iSpecies))
			{
				g_eDexUncaughtSpecies = (u_int)iSpecies;
				g_iDexUncaughtCell = (int)u;
				break;
			}
		}
	}

	void Setup_ZMDexScreen()
	{
		g_eDexPhase                 = DexPhase::Done;
		g_iDexPhaseFrames           = 0;
		g_bDexActive                = false;
		g_bDexFailed                = false;
		g_szDexFailure              = "test did not reach verification";

		g_bDexMovementEnabledBefore = false;
		g_bDexRootOpened            = false;
		g_iDexRootCursorOnConfirm   = -99;
		g_eDexTopOnEnter            = (u_int)ZM_MENU_SCREEN_NONE;
		g_uDexExpectedVisibleCells  = 0u;
		g_uDexCaughtCount           = 0u;
		g_eDexLeadSpecies           = (u_int)ZM_SPECIES_COUNT;
		g_iDexLeadCell              = -1;
		g_eDexUncaughtSpecies       = (u_int)ZM_SPECIES_COUNT;
		g_iDexUncaughtCell          = -1;
		g_iDexCursorOnEnter         = -99;
		g_bDexNavReachedPageButton  = false;
		g_strDexNavFocusName.clear();
		g_bDexNextFocusParked       = false;
		g_iDexPageBeforeNext        = -99;
		g_iDexPageAfterNext         = -99;
		g_bDexOpenAfterBack         = false;
		g_eDexTopAfterBack          = (u_int)ZM_MENU_SCREEN_NONE;
		g_bDexMenuClosed            = false;
		g_bDexMovementReenabled     = false;
		g_bDexFocusClearedOnClose   = false;

		g_xDexElementsOnEnter       = DexElementView{};
		g_xDexElementsAfterNext     = DexElementView{};
		g_xDexElementsAfterBack     = DexElementView{};

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO process
		// state (fixed dt, scene load) until every git-ignored input is confirmed
		// present. CI has no baked Assets tree -> skip.
		g_bDexPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bDexPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere assets absent -- run a *_True build");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fUI_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eDexPhase = DexPhase::AwaitReady;
		g_bDexActive = true;
	}

	bool Step_ZMDexScreen(int)
	{
		if (!g_bDexActive || g_bDexFailed || g_eDexPhase == DexPhase::Done)
		{
			return false;
		}

		++g_iDexPhaseFrames;
		switch (g_eDexPhase)
		{
		case DexPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iDexPhaseFrames > iDEX_READY_DEADLINE)
				{
					FailDex("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}
			if (ResolveSingletonMenuStack() == nullptr)
			{
				FailDex("no unique ZM_UI_MenuStack singleton (ZM_MenuRoot missing)");
				return false;
			}

			g_bDexMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();
			g_uDexExpectedVisibleCells = ZM_UI_Dex::VisibleCellCount(0u, (u_int)ZM_SPECIES_COUNT);
			CaptureDexFixtureSpecies();

			g_eDexPhase = DexPhase::OpenMenu;
			g_iDexPhaseFrames = 0;
			return true;
		}

		case DexPhase::OpenMenu:
		{
			// Resolve FRESH each frame (the pool relocates on swap-and-pop).
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDex("the ZM_UI_MenuStack singleton stopped resolving while opening");
				return false;
			}
			if (pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bDexRootOpened = true;
				g_eDexPhase = DexPhase::NavToBag;
				g_iDexPhaseFrames = 0;
				return true;
			}
			if (g_iDexPhaseFrames > iDEX_OPEN_DEADLINE)
			{
				FailDex("the ROOT menu never opened after the M press");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		case DexPhase::NavToBag:
		case DexPhase::NavToDex:
		{
			// The engine's per-canvas focus-nav runs AFTER the component's OnUpdate, so the
			// component's cursor MIRROR trails the canvas focus by a frame and a conditional
			// re-press would over-shoot. Press Down EXACTLY ONCE (on the first frame of the
			// phase) and then poll the mirror until it settles on the wanted entry.
			const int iWanted = (g_eDexPhase == DexPhase::NavToBag)
				? (int)ZM_MENU_ROOT_BAG
				: (int)ZM_MENU_ROOT_DEX;
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDex("the ZM_UI_MenuStack singleton stopped resolving while navigating ROOT");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				FailDex("the menu closed unexpectedly while navigating ROOT");
				return false;
			}
			if (pxMenu->GetCursor() >= iWanted)
			{
				g_eDexPhase = (g_eDexPhase == DexPhase::NavToBag)
					? DexPhase::NavToDex
					: DexPhase::EnterDex;
				g_iDexPhaseFrames = 0;
				return true;
			}
			if (g_iDexPhaseFrames == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			if (g_iDexPhaseFrames > iDEX_NAV_DEADLINE)
			{
				FailDex("the ROOT focus cursor never reached the Dex entry");
				return false;
			}
			return true;
		}

		case DexPhase::EnterDex:
		{
			// ONE Enter edge, with idle frames on either side so the edge-detected
			// ZM_InputActions::ReadConfirmPressed fires exactly once.
			if (g_iDexPhaseFrames == iDEX_PRESS_FRAME)
			{
				ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
				g_iDexRootCursorOnConfirm = (pxMenu != nullptr) ? pxMenu->GetCursor() : -99;
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iDexPhaseFrames < iDEX_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDex("the ZM_UI_MenuStack singleton stopped resolving after the Dex confirm");
				return false;
			}
			g_eDexTopOnEnter      = (u_int)pxMenu->GetTopScreen();
			g_iDexCursorOnEnter   = pxMenu->GetDexScreen().GetCursor();
			g_iDexPageBeforeNext  = pxMenu->GetDexScreen().GetPage();
			g_xDexElementsOnEnter = ReadDexElements();

			g_eDexPhase = DexPhase::WalkToPageButton;
			g_iDexPhaseFrames = 0;
			return true;
		}

		case DexPhase::WalkToPageButton:
		{
			// Spaced Down edges. Nothing links the cells or the page buttons: this walks
			// purely on the ENGINE SPATIAL search (FindNearestFocusable), which is the whole
			// reason the screen is a grid.
			Zenith_UIComponent* pxUI = ResolveMenuRootUI();
			if (pxUI == nullptr)
			{
				FailDex("the ZM_MenuRoot UI component stopped resolving while walking the grid");
				return false;
			}
			Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
			if (pxFocused != nullptr && IsDexPageButtonName(pxFocused->GetName()))
			{
				g_bDexNavReachedPageButton = true;
				g_strDexNavFocusName = pxFocused->GetName();
				g_eDexPhase = DexPhase::ConfirmNextPage;
				g_iDexPhaseFrames = 0;
				return true;
			}
			if (g_iDexPhaseFrames > iDEX_WALK_DEADLINE)
			{
				g_strDexNavFocusName = (pxFocused != nullptr) ? pxFocused->GetName() : std::string();
				FailDex("the spatial focus-nav never walked off the grid onto a page button");
				return false;
			}
			if ((g_iDexPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			return true;
		}

		case DexPhase::ConfirmNextPage:
		{
			// The walk above proved the page buttons are nav-REACHABLE; park the focus on
			// the NEXT button explicitly so the paging assertion does not also depend on the
			// left/right geometry between the two buttons.
			if (g_iDexPhaseFrames == iDEX_FOCUS_FRAME)
			{
				Zenith_UIComponent* pxUI = ResolveMenuRootUI();
				Zenith_UI::Zenith_UIElement* pxNext = (pxUI != nullptr)
					? pxUI->FindElement(ZM_UI_Dex::szNEXT_NAME)
					: nullptr;
				if (pxNext == nullptr)
				{
					FailDex("the Next-page button did not resolve by name");
					return false;
				}
				pxUI->GetCanvas().SetFocusedElement(pxNext);
				g_bDexNextFocusParked = true;
				return true;
			}
			if (g_iDexPhaseFrames == iDEX_LATE_PRESS)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iDexPhaseFrames < iDEX_LATE_SETTLE)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDex("the ZM_UI_MenuStack singleton stopped resolving after the Next confirm");
				return false;
			}
			g_iDexPageAfterNext     = pxMenu->GetDexScreen().GetPage();
			g_xDexElementsAfterNext = ReadDexElements();

			g_eDexPhase = DexPhase::BackToRoot;
			g_iDexPhaseFrames = 0;
			return true;
		}

		case DexPhase::BackToRoot:
		{
			if (g_iDexPhaseFrames == iDEX_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				return true;
			}
			if (g_iDexPhaseFrames < iDEX_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDex("the ZM_UI_MenuStack singleton stopped resolving after the Escape");
				return false;
			}
			g_bDexOpenAfterBack     = pxMenu->IsOpen();
			g_eDexTopAfterBack      = (u_int)pxMenu->GetTopScreen();
			g_xDexElementsAfterBack = ReadDexElements();

			g_eDexPhase = DexPhase::CloseMenu;
			g_iDexPhaseFrames = 0;
			return true;
		}

		case DexPhase::CloseMenu:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailDex("the ZM_UI_MenuStack singleton stopped resolving while closing");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bDexMenuClosed          = true;
				g_bDexFocusClearedOnClose = MenuFocusCleared();

				bool bEnabled = false;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bDexMovementReenabled = bEnabled;
				}

				g_eDexPhase = DexPhase::Done;
				return false;
			}
			if (g_iDexPhaseFrames > iDEX_CLOSE_DEADLINE)
			{
				FailDex("the ROOT menu never closed after the final Escape press");
				return false;
			}
			// Spaced edges (one every fourth frame) so each press is a clean edge.
			if ((g_iDexPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			}
			return true;
		}

		case DexPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMDexScreen()
	{
		bool bPassed = true;

		if (g_bDexActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DexScreen] captured: failed=%s (%s) movBefore=%s rootOpened=%s "
				"rootCursorOnConfirm=%d (want %u) topOnEnter=%u (want DEX=%u) wantVisibleCells=%u "
				"caught=%u/%u lead=%u cell=%d uncaught=%u cell=%d cursorOnEnter=%d navFocus='%s' "
				"pageBefore=%d pageAfter=%d openAfterBack=%s topAfterBack=%u (want ROOT=%u) "
				"closed=%s movReenabled=%s focusCleared=%s",
				g_bDexFailed ? "true" : "false", g_szDexFailure,
				g_bDexMovementEnabledBefore ? "true" : "false",
				g_bDexRootOpened ? "true" : "false",
				g_iDexRootCursorOnConfirm, (u_int)ZM_MENU_ROOT_DEX,
				g_eDexTopOnEnter, (u_int)ZM_MENU_SCREEN_DEX,
				g_uDexExpectedVisibleCells,
				g_uDexCaughtCount, (u_int)ZM_SPECIES_COUNT,
				g_eDexLeadSpecies, g_iDexLeadCell,
				g_eDexUncaughtSpecies, g_iDexUncaughtCell,
				g_iDexCursorOnEnter, g_strDexNavFocusName.c_str(),
				g_iDexPageBeforeNext, g_iDexPageAfterNext,
				g_bDexOpenAfterBack ? "true" : "false",
				g_eDexTopAfterBack, (u_int)ZM_MENU_SCREEN_ROOT,
				g_bDexMenuClosed ? "true" : "false",
				g_bDexMovementReenabled ? "true" : "false",
				g_bDexFocusClearedOnClose ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DexScreen] widgets: onEnter(resolved=%s panel=%s/%s grid=%s/%s header=%s/%s "
				"'%s' prev=%s/%s next=%s/%s cellsFound=%u cellsVisible=%u focus='%s') "
				"afterBack(panel=%s grid=%s cellsVisible=%u prev=%s)",
				g_xDexElementsOnEnter.m_bResolved ? "true" : "false",
				g_xDexElementsOnEnter.m_bPanelFound ? "found" : "MISSING",
				g_xDexElementsOnEnter.m_bPanelVisible ? "visible" : "hidden",
				g_xDexElementsOnEnter.m_bGridFound ? "found" : "MISSING",
				g_xDexElementsOnEnter.m_bGridVisible ? "visible" : "hidden",
				g_xDexElementsOnEnter.m_bHeaderFound ? "found" : "MISSING",
				g_xDexElementsOnEnter.m_bHeaderVisible ? "visible" : "hidden",
				g_xDexElementsOnEnter.m_strHeaderText.c_str(),
				g_xDexElementsOnEnter.m_bPrevFound ? "found" : "MISSING",
				g_xDexElementsOnEnter.m_bPrevVisible ? "visible" : "hidden",
				g_xDexElementsOnEnter.m_bNextFound ? "found" : "MISSING",
				g_xDexElementsOnEnter.m_bNextVisible ? "visible" : "hidden",
				g_xDexElementsOnEnter.m_uCellsFound,
				g_xDexElementsOnEnter.m_uCellsVisible,
				g_xDexElementsOnEnter.m_strFocusedName.c_str(),
				g_xDexElementsAfterBack.m_bPanelVisible ? "visible" : "hidden",
				g_xDexElementsAfterBack.m_bGridVisible ? "visible" : "hidden",
				g_xDexElementsAfterBack.m_uCellsVisible,
				g_xDexElementsAfterBack.m_bPrevVisible ? "visible" : "hidden");

			if (g_bDexFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_DexScreen] %s", g_szDexFailure);
				bPassed = false;
			}

			// The baselines must be meaningful: the player was movable before opening, the
			// page really holds cells, and the fixture found BOTH a caught and an uncaught
			// entry on page 0 (otherwise the label assertions would be vacuous).
			if (!g_bDexMovementEnabledBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the player was not movable before opening -- the freeze "
					"assertions would be vacuous");
				bPassed = false;
			}
			if (g_uDexExpectedVisibleCells == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] page 0 holds no cells -- every widget assertion would be vacuous");
				bPassed = false;
			}
			if (g_iDexLeadCell < 0 || g_iDexUncaughtCell < 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the fixture needs BOTH a caught page-0 entry (the party lead, "
					"cell=%d) and an uncaught one (cell=%d) -- the caught/hidden label assertions "
					"would be vacuous", g_iDexLeadCell, g_iDexUncaughtCell);
				bPassed = false;
			}

			if (!g_bDexRootOpened)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the ROOT menu never opened after the M press");
				bPassed = false;
			}
			if (g_iDexRootCursorOnConfirm != (int)ZM_MENU_ROOT_DEX)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the ROOT cursor was %d when Enter was pressed, expected %u (Dex) "
					"-- the two Down presses did not land on the Dex entry",
					g_iDexRootCursorOnConfirm, (u_int)ZM_MENU_ROOT_DEX);
				bPassed = false;
			}

			// --- confirm on Dex: the DEX screen is on top and really presented ---
			if (g_eDexTopOnEnter != (u_int)ZM_MENU_SCREEN_DEX)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] top screen after confirming Dex was %u, expected DEX %u",
					g_eDexTopOnEnter, (u_int)ZM_MENU_SCREEN_DEX);
				bPassed = false;
			}
			if (!g_xDexElementsOnEnter.m_bResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the ZM_MenuRoot UI component did not resolve on the dex screen");
				bPassed = false;
			}
			else
			{
				if (!g_xDexElementsOnEnter.m_bPanelFound
					|| !g_xDexElementsOnEnter.m_bHeaderFound
					|| !g_xDexElementsOnEnter.m_bPrevFound
					|| !g_xDexElementsOnEnter.m_bNextFound)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DexScreen] the AUTHORED dex widgets did not all resolve by name (panel=%s "
						"header=%s prev=%s next=%s) -- ZM_ConfigureMenuRoot / the AddStep_CreateUI* "
						"contract is broken and NOTHING would render",
						g_xDexElementsOnEnter.m_bPanelFound ? "true" : "false",
						g_xDexElementsOnEnter.m_bHeaderFound ? "true" : "false",
						g_xDexElementsOnEnter.m_bPrevFound ? "true" : "false",
						g_xDexElementsOnEnter.m_bNextFound ? "true" : "false");
					bPassed = false;
				}
				// The RUNTIME half: the grid and all 30 cells must exist and be findable by
				// name, which only happens if the build-once routine ran AND reparented every
				// cell under the grid (FindElement recurses, so a cell that was never
				// reparented would still resolve -- the grid's own presence is the proof).
				if (!g_xDexElementsOnEnter.m_bGridFound
					|| g_xDexElementsOnEnter.m_uCellsFound != ZM_UI_Dex::uCELL_COUNT)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DexScreen] the RUNTIME-built grid did not materialise (grid found=%s, "
						"cells found=%u of %u) -- ZM_UI_Dex::Present's build-once routine did not run",
						g_xDexElementsOnEnter.m_bGridFound ? "true" : "false",
						g_xDexElementsOnEnter.m_uCellsFound, ZM_UI_Dex::uCELL_COUNT);
					bPassed = false;
				}
				if (!g_xDexElementsOnEnter.m_bPanelVisible
					|| !g_xDexElementsOnEnter.m_bGridVisible
					|| !g_xDexElementsOnEnter.m_bHeaderVisible
					|| !g_xDexElementsOnEnter.m_bPrevVisible
					|| !g_xDexElementsOnEnter.m_bNextVisible)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DexScreen] the dex widgets were not all shown (panel=%s grid=%s header=%s "
						"prev=%s next=%s)",
						g_xDexElementsOnEnter.m_bPanelVisible ? "true" : "false",
						g_xDexElementsOnEnter.m_bGridVisible ? "true" : "false",
						g_xDexElementsOnEnter.m_bHeaderVisible ? "true" : "false",
						g_xDexElementsOnEnter.m_bPrevVisible ? "true" : "false",
						g_xDexElementsOnEnter.m_bNextVisible ? "true" : "false");
					bPassed = false;
				}
				// EXACTLY the live cells are visible: a dead trailing cell must stay hidden
				// (and non-focusable), or the nav could park on a blank entry.
				if (g_xDexElementsOnEnter.m_uCellsVisible != g_uDexExpectedVisibleCells)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DexScreen] %u cells were visible on page 0, expected %u",
						g_xDexElementsOnEnter.m_uCellsVisible, g_uDexExpectedVisibleCells);
					bPassed = false;
				}
				const std::string strWantHeader = ZM_UI_Dex::FormatCompletion(
					g_uDexCaughtCount, (u_int)ZM_SPECIES_COUNT);
				if (g_xDexElementsOnEnter.m_strHeaderText.find(strWantHeader) == std::string::npos)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DexScreen] the header element reads '%s', expected it to carry the "
						"completion string '%s' -- Present is not writing the header",
						g_xDexElementsOnEnter.m_strHeaderText.c_str(), strWantHeader.c_str());
					bPassed = false;
				}
				// The caught / hidden label pair, read straight off the widgets.
				if (g_iDexLeadCell >= 0)
				{
					const ZM_SPECIES_ID eLead = (ZM_SPECIES_ID)g_eDexLeadSpecies;
					const std::string& strLeadCell =
						g_xDexElementsOnEnter.m_astrCellText[g_iDexLeadCell];
					if (strLeadCell != ZM_UI_Dex::FormatCell(eLead, true)
						|| strLeadCell.find(ZM_GetSpeciesName(eLead)) == std::string::npos)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_DexScreen] the party lead's cell %d reads '%s', expected the CAUGHT "
							"label '%s' naming '%s'",
							g_iDexLeadCell, strLeadCell.c_str(),
							ZM_UI_Dex::FormatCell(eLead, true).c_str(), ZM_GetSpeciesName(eLead));
						bPassed = false;
					}
				}
				if (g_iDexUncaughtCell >= 0)
				{
					const ZM_SPECIES_ID eHidden = (ZM_SPECIES_ID)g_eDexUncaughtSpecies;
					const std::string& strHiddenCell =
						g_xDexElementsOnEnter.m_astrCellText[g_iDexUncaughtCell];
					if (strHiddenCell != ZM_UI_Dex::FormatCell(eHidden, false)
						|| strHiddenCell.find(ZM_GetSpeciesName(eHidden)) != std::string::npos)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_DexScreen] the uncaught cell %d reads '%s', expected the HIDDEN label "
							"'%s' -- an uncaught entry must never leak the species name '%s'",
							g_iDexUncaughtCell, strHiddenCell.c_str(),
							ZM_UI_Dex::FormatCell(eHidden, false).c_str(), ZM_GetSpeciesName(eHidden));
						bPassed = false;
					}
				}
			}
			if (g_iDexCursorOnEnter < 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the screen's focused-cell mirror was %d, expected a real cell -- "
					"the screen must park the focus on a live entry or the arrow keys drive nothing "
					"(canvas focus was '%s')",
					g_iDexCursorOnEnter, g_xDexElementsOnEnter.m_strFocusedName.c_str());
				bPassed = false;
			}

			// --- the ENGINE spatial nav walks off the grid onto a page button ---
			if (!g_bDexNavReachedPageButton)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the spatial focus-nav never reached a page button (last focus "
					"'%s') -- the page buttons carry no explicit links, so an unreachable button "
					"means the grid layout never placed the cells", g_strDexNavFocusName.c_str());
				bPassed = false;
			}

			// --- confirm Next: the page advances ---
			if (!g_bDexNextFocusParked)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the test never parked the focus on the Next-page button");
				bPassed = false;
			}
			if (g_iDexPageBeforeNext != 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the dex opened on page %d, expected page 0",
					g_iDexPageBeforeNext);
				bPassed = false;
			}
			if (g_iDexPageAfterNext != 1)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] confirming Next left the dex on page %d, expected 1 -- the "
					"by-name confirm dispatch did not page", g_iDexPageAfterNext);
				bPassed = false;
			}
			// Paging must actually RELABEL the grid, not just move a counter.
			if (g_iDexLeadCell >= 0
				&& g_xDexElementsAfterNext.m_astrCellText[g_iDexLeadCell]
					== g_xDexElementsOnEnter.m_astrCellText[g_iDexLeadCell])
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] cell %d still reads '%s' after paging -- Present is not "
					"refilling the grid from the new page",
					g_iDexLeadCell, g_xDexElementsAfterNext.m_astrCellText[g_iDexLeadCell].c_str());
				bPassed = false;
			}

			// --- Escape: back on ROOT with every dex widget down ---
			if (!g_bDexOpenAfterBack || g_eDexTopAfterBack != (u_int)ZM_MENU_SCREEN_ROOT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] after the Escape the menu was open=%s on top=%u, expected the "
					"ROOT %u to be restored",
					g_bDexOpenAfterBack ? "true" : "false",
					g_eDexTopAfterBack, (u_int)ZM_MENU_SCREEN_ROOT);
				bPassed = false;
			}
			// The *Found flags prove the capture actually ran (a default-initialised view
			// would pass the "hidden" checks vacuously).
			if (!g_xDexElementsAfterBack.m_bPanelFound
				|| !g_xDexElementsAfterBack.m_bGridFound
				|| g_xDexElementsAfterBack.m_bPanelVisible
				|| g_xDexElementsAfterBack.m_bGridVisible
				|| g_xDexElementsAfterBack.m_bHeaderVisible
				|| g_xDexElementsAfterBack.m_bPrevVisible
				|| g_xDexElementsAfterBack.m_bNextVisible
				|| g_xDexElementsAfterBack.m_uCellsVisible != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the dex widgets were not all hidden once the screen popped "
					"(panel found=%s visible=%s, grid found=%s visible=%s, header visible=%s, "
					"prev visible=%s, next visible=%s, cells visible=%u)",
					g_xDexElementsAfterBack.m_bPanelFound ? "true" : "false",
					g_xDexElementsAfterBack.m_bPanelVisible ? "true" : "false",
					g_xDexElementsAfterBack.m_bGridFound ? "true" : "false",
					g_xDexElementsAfterBack.m_bGridVisible ? "true" : "false",
					g_xDexElementsAfterBack.m_bHeaderVisible ? "true" : "false",
					g_xDexElementsAfterBack.m_bPrevVisible ? "true" : "false",
					g_xDexElementsAfterBack.m_bNextVisible ? "true" : "false",
					g_xDexElementsAfterBack.m_uCellsVisible);
				bPassed = false;
			}

			// --- the last Escape: the menu closes and the player is movable again ---
			if (!g_bDexMenuClosed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the menu never closed after the final Escape press");
				bPassed = false;
			}
			if (!g_bDexMovementReenabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] player movement was not re-enabled after the menu closed");
				bPassed = false;
			}
			if (!g_bDexFocusClearedOnClose)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DexScreen] the canvas focus was not cleared (nullptr) after the menu "
					"closed -- arrow keys could still drive the hidden menu");
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), exactly like the sibling tests.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_bDexActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bDexActive = false;

		return bPassed || !g_bDexPrereqsPresent;
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

static const Zenith_AutomatedTest g_xZMDialogueTalkTest = {
	"ZM_DialogueTalk_Test",
	&Setup_ZMDialogueTalk,
	&Step_ZMDialogueTalk,
	&Verify_ZMDialogueTalk,
	/* maxFrames */ 1200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDialogueTalkTest);

static const Zenith_AutomatedTest g_xZMPartyScreenTest = {
	"ZM_PartyScreen_Test",
	&Setup_ZMPartyScreen,
	&Step_ZMPartyScreen,
	&Verify_ZMPartyScreen,
	/* maxFrames */ 1400,   // ready window + one open + four spaced press phases + the close
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMPartyScreenTest);

static const Zenith_AutomatedTest g_xZMDexScreenTest = {
	"ZM_DexScreen_Test",
	&Setup_ZMDexScreen,
	&Step_ZMDexScreen,
	&Verify_ZMDexScreen,
	/* maxFrames */ 1600,   // ready window + open + two nav + confirm + the grid walk + close
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDexScreenTest);

#endif // ZENITH_INPUT_SIMULATOR
