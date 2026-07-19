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
#include "Zenithmon/Source/Data/ZM_ItemData.h"         // ZM_GetItemName / ZM_ITEM_CATCHORB (the bag gate)
#include "Zenithmon/Source/Party/ZM_Bag.h"             // ZM_Bag / ZM_ItemStack (the live bag the screen renders)
#include "Zenithmon/Source/UI/ZM_UI_Bag.h"
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"
#include "Zenithmon/Source/UI/ZM_UI_Party.h"
#include "Zenithmon/Source/Shop/ZM_ShopLogic.h"        // ZM_ShopBuyPrice / ZM_SHOP_RESULT (the shop gate)
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"
#include "Zenithmon/Source/CareCenter/ZM_CareCenter.h" // the armed prompt labels (the SC8 gate)
#include "Zenithmon/Source/Party/ZM_GameState.h"       // ZM_GameState::m_xParty (the LIVE heal target)

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_UI -- the windowed gates for the overworld pause menu (S6 item 2).
// SEVEN tests, all m_bRequiresGraphics = true:
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
//   * ZM_BagScreen_Test (SC6) -- open the pause menu, walk the ROOT focus down to
//     the Bag entry, confirm it, and assert the BAG screen really presents: the
//     authored panel / header / eight rows / four nav buttons all resolve by name,
//     exactly VisibleRowCount(page 0) rows are shown, the header carries the seeded
//     money, row 0's label names the seeded Catch Orb, then confirming the
//     Next-Pocket button changes the pocket AND relabels the rows, and Escape
//     returns to ROOT then closes the menu and unfreezes the player.
//   * ZM_ShopScreen_Test (SC7) -- raise the mart straight over the overworld with
//     ZM_UI_MenuStack::TryOpenShop (no pause menu involved), and assert the SHOP
//     screen really presents: the authored panel / header / six rows / eight controls
//     all resolve by name, exactly VisibleRowCount(page 0) rows list the configured
//     stock with its TABLE prices, the player is frozen, real Down edges walk the
//     focus off the list onto the CONFIRM control, one Enter buys the SELECTED entry
//     and the LIVE ZM_GameState's money falls by exactly price x quantity while the
//     bag rises by exactly the quantity, and Escape closes the shop, hides every
//     widget and re-enables movement.
//   * ZM_CareCenterHeal_Test (SC8) -- damage the LIVE party, raise the Care Center's
//     yes/no prompt with ZM_UI_MenuStack::TryOpenCareCenterPrompt, read past its line
//     with spaced Enter edges, and assert the box HOLDS on the question rather than
//     closing: both AUTHORED buttons shown, focusable and labelled from ZM_CareCenter,
//     the canvas focus parked on Yes, real arrow edges walking Yes -> No -> Yes, one
//     Enter on Yes healing the REAL ZM_GameState to full and closing the menu with
//     movement restored -- then the same prompt answered NO healing nothing.
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
			// A dialogue's LINES are MODAL: cancel is deliberately not routed while one is
			// still being read, so this Escape must change nothing (were it routed,
			// PopTopScreen would pop the box and close the menu outright). Cancel only
			// means something once an SC8 yes/no question is up, where it answers NO --
			// and this conversation arms no choice.
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

	// =========================================================================
	// ZM_BagScreen_Test (S6 item 2 SC6) -- the windowed gate for the bag screen,
	// and the first on-screen proof of the SC3 model (ZM_Bag + m_uMoney). In a
	// runtime-ready Dawnmere:
	//   M -> ROOT -> Down (the ROOT cursor walks Party -> Bag) -> Enter -> the BAG
	//   screen is on top with the authored panel / header / eight rows / four nav
	//   buttons resolving by name, exactly VisibleRowCount(page 0) rows visible, the
	//   header carrying the seeded money, row 0 naming the seeded Catch Orb, and the
	//   canvas focus parked on a row -> Down edges WALK off the row list onto the nav
	//   band (the screen's one new traversal; the starter pocket is a PARTIAL page, so
	//   this is exactly where a stale explicit nav link would swallow the press) ->
	//   park + confirm the Next-Pocket button -> the pocket changes and the rows
	//   RELABEL -> keep confirming until an EMPTY pocket, where one non-focusable
	//   notice row shows and the screen re-parks the cleared focus itself -> Escape ->
	//   back on ROOT with every bag widget hidden -> Escape -> the menu closes and the
	//   player is movable again.
	//
	// The model-only assertions cannot see a typo'd element name, a missing
	// AddStep_CreateUI*, or a ZM_ConfigureMenuRoot FindElement that silently returned
	// nullptr -- ZM_UI_Bag null-guards all of it -- so the widget view below is the
	// on-screen half of the gate. It reuses this file's Dawnmere prerequisite guards /
	// entity views / fixed dt and keeps its OWN control + observation globals.
	// =========================================================================

	// What the AUTHORED bag widgets actually report this frame.
	struct BagElementView
	{
		bool        m_bResolved      = false;   // the ZM_MenuRoot UI component resolved
		bool        m_bPanelFound    = false;
		bool        m_bPanelVisible  = false;
		bool        m_bHeaderFound   = false;
		bool        m_bHeaderVisible = false;
		std::string m_strHeaderText;
		u_int       m_uNavFound      = 0u;      // of the four pocket / page buttons
		u_int       m_uNavVisible    = 0u;
		u_int       m_uRowsFound     = 0u;
		u_int       m_uRowsVisible   = 0u;
		bool        m_abRowVisible[ZM_UI_Bag::uROWS_PER_PAGE] = {};
		// Focusability is HALF the contract: a dead row (or the "(empty)" notice) must be
		// unreachable by the engine nav, which collects visible + FOCUSABLE elements.
		bool        m_abRowFocusable[ZM_UI_Bag::uROWS_PER_PAGE] = {};
		std::string m_astrRowText[ZM_UI_Bag::uROWS_PER_PAGE];
		std::string m_strFocusedName;           // the canvas focus (want a row, then a nav button)
	};

	// The four nav buttons, in the order ZM_ConfigureMenuRoot places them.
	const char* const aszBAG_NAV_NAMES[4] =
	{
		ZM_UI_Bag::szPREV_POCKET_NAME,
		ZM_UI_Bag::szNEXT_POCKET_NAME,
		ZM_UI_Bag::szPREV_PAGE_NAME,
		ZM_UI_Bag::szNEXT_PAGE_NAME,
	};

	// True while the named element is one of the four pocket / page buttons.
	bool IsBagNavButtonName(const std::string& strName)
	{
		for (const char* szNav : aszBAG_NAV_NAMES)
		{
			if (strName == szNav)
			{
				return true;
			}
		}
		return false;
	}

	BagElementView ReadBagElements()
	{
		BagElementView xView;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;

		if (Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Bag::szPANEL_NAME))
		{
			xView.m_bPanelFound = true;
			xView.m_bPanelVisible = pxPanel->IsVisible();
		}
		if (Zenith_UI::Zenith_UIText* pxHeader =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Bag::szHEADER_NAME))
		{
			xView.m_bHeaderFound = true;
			xView.m_bHeaderVisible = pxHeader->IsVisible();
			xView.m_strHeaderText = pxHeader->GetText();
		}
		for (const char* szNav : aszBAG_NAV_NAMES)
		{
			Zenith_UI::Zenith_UIElement* pxNav = pxUI->FindElement(szNav);
			if (pxNav == nullptr)
			{
				continue;
			}
			++xView.m_uNavFound;
			if (pxNav->IsVisible())
			{
				++xView.m_uNavVisible;
			}
		}
		for (u_int u = 0u; u < ZM_UI_Bag::uROWS_PER_PAGE; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxRow =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Bag::RowElementName(u));
			if (pxRow == nullptr)
			{
				continue;
			}
			++xView.m_uRowsFound;
			xView.m_abRowVisible[u] = pxRow->IsVisible();
			xView.m_abRowFocusable[u] = pxRow->IsFocusable();
			xView.m_astrRowText[u] = pxRow->GetText();
			if (xView.m_abRowVisible[u])
			{
				++xView.m_uRowsVisible;
			}
		}
		if (Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement())
		{
			xView.m_strFocusedName = pxFocused->GetName();
		}
		return xView;
	}

	enum class BagPhase
	{
		AwaitReady,
		OpenMenu,           // press M until the ROOT pause menu is up
		NavToBag,           // ONE Down edge -> the ROOT cursor advances Party(0) -> Bag(1)
		EnterBag,           // ONE Enter edge -> the BAG screen
		WalkToNavBand,      // spaced Down edges: the ENGINE spatial nav walks off the row list
		ConfirmNextPocket,  // park the focus on Next-Pocket, ONE Enter edge -> the pocket cycles
		CycleToEmptyPocket, // spaced Enter edges until the shown pocket holds nothing
		SampleEmptyPocket,  // ...and read the "(empty)" presentation + its focus fallback
		BackToRoot,         // ONE Escape edge -> the bag pops back to ROOT
		CloseMenu,          // Escape edges until the menu closes
		Done,
	};

	constexpr int iBAG_READY_DEADLINE = 420;   // Dawnmere first-load ready window (sibling parity)
	constexpr int iBAG_OPEN_DEADLINE  = 120;   // frames for the M press to open the ROOT menu
	constexpr int iBAG_NAV_DEADLINE   = 120;   // frames for the Down press to advance the ROOT cursor
	constexpr int iBAG_PRESS_FRAME    = 2;     // the frame within a press phase that emits the edge
	constexpr int iBAG_SETTLE_FRAME   = 6;     // ...and the frame the outcome is sampled on
	constexpr int iBAG_FOCUS_FRAME    = 1;     // the frame ConfirmNextPocket parks the focus
	constexpr int iBAG_LATE_PRESS     = 4;     // ...and the (later) frame it presses Enter
	constexpr int iBAG_LATE_SETTLE    = 10;    // ...and the frame it samples the outcome
	constexpr int iBAG_WALK_DEADLINE  = 200;   // frames for the spatial nav to reach the nav band
	constexpr int iBAG_CYCLE_DEADLINE = 200;   // frames for the pocket cycle to reach an empty pocket
	constexpr int iBAG_CLOSE_DEADLINE = 120;   // frames for the final Escape to close the menu

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	BagPhase g_eBagPhase          = BagPhase::Done;
	int      g_iBagPhaseFrames    = 0;
	bool     g_bBagPrereqsPresent = false;
	bool     g_bBagActive         = false;
	bool     g_bBagFailed         = false;
	const char* g_szBagFailure    = "test did not reach verification";

	// ---- Captured observations ----
	bool  g_bBagMovementEnabledBefore = false;   // player movable BEFORE opening (baseline)
	bool  g_bBagRootOpened            = false;   // ROOT came up on the M press
	int   g_iBagRootCursorOnConfirm   = -99;     // the ROOT cursor when Bag was confirmed (want 1)
	u_int g_eBagTopOnEnter            = (u_int)ZM_MENU_SCREEN_NONE;   // want BAG
	// The live model the screen must render, captured BEFORE the menu is opened.
	u_int g_uBagFirstPocketStacks     = 0u;      // stacks in the BALL pocket (the opening pocket)
	u_int g_uBagExpectedVisibleRows   = 0u;      // == VisibleRowCount(page 0, that count)
	u_int g_uBagMoney                 = 0u;
	u_int g_eBagRow0Item              = (u_int)ZM_ITEM_NONE;
	u_int g_uBagRow0Count             = 0u;
	u_int g_uBagCatchOrbCount         = 0u;      // the seeded Catch Orb count (want > 0)
	int   g_iBagCursorOnEnter         = -99;     // the screen's focused-row mirror (want >= 0)
	u_int g_eBagPocketOnEnter         = (u_int)ZM_ITEM_CATEGORY_COUNT;   // want BALL
	u_int g_eBagPocketAfterNext       = (u_int)ZM_ITEM_CATEGORY_COUNT;   // want != BALL
	bool  g_bBagNavReachedNavBand     = false;   // Down edges walked off the list onto the band
	std::string g_strBagNavFocusName;            // ...onto this element (or where it stalled)
	bool  g_bBagNextPocketFocusParked = false;   // the test parked the focus on Next-Pocket
	u_int g_eBagEmptyPocket           = (u_int)ZM_ITEM_CATEGORY_COUNT;   // the EMPTY pocket reached
	bool  g_bBagEmptyFocusCleared     = false;   // the test cleared the focus on that pocket
	int   g_iBagCursorOnEmpty         = -99;     // the row mirror over an empty pocket (want -1)
	bool  g_bBagOpenAfterBack         = false;   // the menu is still open after the Escape
	u_int g_eBagTopAfterBack          = (u_int)ZM_MENU_SCREEN_NONE;   // want ROOT
	bool  g_bBagMenuClosed            = false;   // the final Escape closed the menu
	bool  g_bBagMovementReenabled     = false;   // ...and the player is movable again
	bool  g_bBagFocusClearedOnClose   = false;   // canvas focus == nullptr after close

	// ---- The AUTHORED widgets ----
	BagElementView g_xBagElementsOnEnter;    // want: panel + header + every page-0 row shown
	BagElementView g_xBagElementsAfterNext;  // want: the NEXT pocket's labels
	BagElementView g_xBagElementsOnEmpty;    // want: ONE visible, NON-focusable "(empty)" row
	BagElementView g_xBagElementsAfterBack;  // want: everything bag-side hidden again

	void FailBag(const char* szReason)
	{
		g_szBagFailure = szReason;
		g_bBagFailed = true;
		g_eBagPhase = BagPhase::Done;
	}

	// Read the live bag facts the widget assertions are anchored on. The screen opens
	// on pocket 0 (BALL), so that is the pocket whose page 0 must be on screen.
	void CaptureBagFixture()
	{
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return;
		}
		g_uBagMoney = pxState->m_uMoney;
		g_uBagCatchOrbCount = pxState->m_xBag.GetCount(ZM_ITEM_CATCHORB);
		g_uBagFirstPocketStacks = pxState->m_xBag.PocketStackCount(ZM_ITEM_CATEGORY_BALL);
		g_uBagExpectedVisibleRows = ZM_UI_Bag::VisibleRowCount(0u, g_uBagFirstPocketStacks);
		if (g_uBagFirstPocketStacks > 0u)
		{
			const ZM_ItemStack& xStack = pxState->m_xBag.PocketStack(ZM_ITEM_CATEGORY_BALL, 0u);
			g_eBagRow0Item = (u_int)xStack.m_eItem;
			g_uBagRow0Count = xStack.m_uCount;
		}
	}

	void Setup_ZMBagScreen()
	{
		g_eBagPhase                 = BagPhase::Done;
		g_iBagPhaseFrames           = 0;
		g_bBagActive                = false;
		g_bBagFailed                = false;
		g_szBagFailure              = "test did not reach verification";

		g_bBagMovementEnabledBefore = false;
		g_bBagRootOpened            = false;
		g_iBagRootCursorOnConfirm   = -99;
		g_eBagTopOnEnter            = (u_int)ZM_MENU_SCREEN_NONE;
		g_uBagFirstPocketStacks     = 0u;
		g_uBagExpectedVisibleRows   = 0u;
		g_uBagMoney                 = 0u;
		g_eBagRow0Item              = (u_int)ZM_ITEM_NONE;
		g_uBagRow0Count             = 0u;
		g_uBagCatchOrbCount         = 0u;
		g_iBagCursorOnEnter         = -99;
		g_eBagPocketOnEnter         = (u_int)ZM_ITEM_CATEGORY_COUNT;
		g_eBagPocketAfterNext       = (u_int)ZM_ITEM_CATEGORY_COUNT;
		g_bBagNavReachedNavBand     = false;
		g_strBagNavFocusName.clear();
		g_bBagNextPocketFocusParked = false;
		g_eBagEmptyPocket           = (u_int)ZM_ITEM_CATEGORY_COUNT;
		g_bBagEmptyFocusCleared     = false;
		g_iBagCursorOnEmpty         = -99;
		g_bBagOpenAfterBack         = false;
		g_eBagTopAfterBack          = (u_int)ZM_MENU_SCREEN_NONE;
		g_bBagMenuClosed            = false;
		g_bBagMovementReenabled     = false;
		g_bBagFocusClearedOnClose   = false;

		g_xBagElementsOnEnter       = BagElementView{};
		g_xBagElementsAfterNext     = BagElementView{};
		g_xBagElementsOnEmpty       = BagElementView{};
		g_xBagElementsAfterBack     = BagElementView{};

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO process
		// state (fixed dt, scene load) until every git-ignored input is confirmed
		// present. CI has no baked Assets tree -> skip.
		g_bBagPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bBagPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere assets absent -- run a *_True build");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fUI_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eBagPhase = BagPhase::AwaitReady;
		g_bBagActive = true;
	}

	bool Step_ZMBagScreen(int)
	{
		if (!g_bBagActive || g_bBagFailed || g_eBagPhase == BagPhase::Done)
		{
			return false;
		}

		++g_iBagPhaseFrames;
		switch (g_eBagPhase)
		{
		case BagPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iBagPhaseFrames > iBAG_READY_DEADLINE)
				{
					FailBag("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}
			if (ResolveSingletonMenuStack() == nullptr)
			{
				FailBag("no unique ZM_UI_MenuStack singleton (ZM_MenuRoot missing)");
				return false;
			}

			g_bBagMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();
			CaptureBagFixture();

			g_eBagPhase = BagPhase::OpenMenu;
			g_iBagPhaseFrames = 0;
			return true;
		}

		case BagPhase::OpenMenu:
		{
			// Resolve FRESH each frame (the pool relocates on swap-and-pop).
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving while opening");
				return false;
			}
			if (pxMenu->IsOpen() && pxMenu->GetTopScreen() == ZM_MENU_SCREEN_ROOT)
			{
				g_bBagRootOpened = true;
				g_eBagPhase = BagPhase::NavToBag;
				g_iBagPhaseFrames = 0;
				return true;
			}
			if (g_iBagPhaseFrames > iBAG_OPEN_DEADLINE)
			{
				FailBag("the ROOT menu never opened after the M press");
				return false;
			}
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_M);
			return true;
		}

		case BagPhase::NavToBag:
		{
			// The engine's per-canvas focus-nav runs AFTER the component's OnUpdate, so the
			// component's cursor MIRROR trails the canvas focus by a frame and a conditional
			// re-press would over-shoot. Press Down EXACTLY ONCE and poll the mirror.
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving while navigating ROOT");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				FailBag("the menu closed unexpectedly while navigating ROOT");
				return false;
			}
			if (pxMenu->GetCursor() >= (int)ZM_MENU_ROOT_BAG)
			{
				g_eBagPhase = BagPhase::EnterBag;
				g_iBagPhaseFrames = 0;
				return true;
			}
			if (g_iBagPhaseFrames == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			if (g_iBagPhaseFrames > iBAG_NAV_DEADLINE)
			{
				FailBag("the ROOT focus cursor never reached the Bag entry");
				return false;
			}
			return true;
		}

		case BagPhase::EnterBag:
		{
			// ONE Enter edge, with idle frames on either side so the edge-detected
			// ZM_InputActions::ReadConfirmPressed fires exactly once.
			if (g_iBagPhaseFrames == iBAG_PRESS_FRAME)
			{
				ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
				g_iBagRootCursorOnConfirm = (pxMenu != nullptr) ? pxMenu->GetCursor() : -99;
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iBagPhaseFrames < iBAG_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving after the Bag confirm");
				return false;
			}
			g_eBagTopOnEnter      = (u_int)pxMenu->GetTopScreen();
			g_iBagCursorOnEnter   = pxMenu->GetBagScreen().GetCursor();
			g_eBagPocketOnEnter   = (u_int)pxMenu->GetBagScreen().GetPocket();
			g_xBagElementsOnEnter = ReadBagElements();

			g_eBagPhase = BagPhase::WalkToNavBand;
			g_iBagPhaseFrames = 0;
			return true;
		}

		case BagPhase::WalkToNavBand:
		{
			// Spaced Down edges from the focused row. NOTHING on this screen carries explicit
			// navigation links, so this walks purely on the ENGINE SPATIAL search -- and it is
			// the one traversal the bag adds over the party / dex screens. It must be driven
			// by real key edges: an explicit bake-time link into a row this page has hidden
			// would be fetched instead of the spatial fallback and swallow the press, and the
			// starter BALL pocket is exactly that case (ONE stack, so row 0 IS the last live
			// row). Parking the focus by hand would hide that entirely.
			Zenith_UIComponent* pxUI = ResolveMenuRootUI();
			if (pxUI == nullptr)
			{
				FailBag("the ZM_MenuRoot UI component stopped resolving while walking the row list");
				return false;
			}
			Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
			if (pxFocused != nullptr && IsBagNavButtonName(pxFocused->GetName()))
			{
				g_bBagNavReachedNavBand = true;
				g_strBagNavFocusName = pxFocused->GetName();
				g_eBagPhase = BagPhase::ConfirmNextPocket;
				g_iBagPhaseFrames = 0;
				return true;
			}
			if (g_iBagPhaseFrames > iBAG_WALK_DEADLINE)
			{
				g_strBagNavFocusName = (pxFocused != nullptr) ? pxFocused->GetName() : std::string();
				FailBag("the spatial focus-nav never walked off the row list onto the nav band");
				return false;
			}
			if ((g_iBagPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			return true;
		}

		case BagPhase::ConfirmNextPocket:
		{
			// The walk above proved the nav band is reachable by key; park the focus on
			// Next-Pocket explicitly so the pocket assertion does not ALSO depend on which of
			// the four buttons the walk happened to land on.
			if (g_iBagPhaseFrames == iBAG_FOCUS_FRAME)
			{
				Zenith_UIComponent* pxUI = ResolveMenuRootUI();
				Zenith_UI::Zenith_UIElement* pxNextPocket = (pxUI != nullptr)
					? pxUI->FindElement(ZM_UI_Bag::szNEXT_POCKET_NAME)
					: nullptr;
				if (pxNextPocket == nullptr)
				{
					FailBag("the Next-Pocket button did not resolve by name");
					return false;
				}
				pxUI->GetCanvas().SetFocusedElement(pxNextPocket);
				g_bBagNextPocketFocusParked = true;
				return true;
			}
			if (g_iBagPhaseFrames == iBAG_LATE_PRESS)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iBagPhaseFrames < iBAG_LATE_SETTLE)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving after the pocket confirm");
				return false;
			}
			g_eBagPocketAfterNext   = (u_int)pxMenu->GetBagScreen().GetPocket();
			g_xBagElementsAfterNext = ReadBagElements();

			g_eBagPhase = BagPhase::CycleToEmptyPocket;
			g_iBagPhaseFrames = 0;
			return true;
		}

		case BagPhase::CycleToEmptyPocket:
		{
			// More spaced Enter edges on the (still focused) Next-Pocket button until the
			// pocket on screen holds NOTHING -- the starter seeds only BALL and MEDICINE, so
			// the very next pocket qualifies. Driven off the LIVE bag rather than a counted
			// number of presses, so a re-tuned starter seed cannot silently leave this phase
			// asserting the empty-pocket presentation against a full pocket.
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving while cycling pockets");
				return false;
			}
			ZM_GameState* pxState = nullptr;
			if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
			{
				FailBag("the game state stopped resolving while cycling pockets");
				return false;
			}
			const ZM_ITEM_CATEGORY eCategory = pxMenu->GetBagScreen().GetPocket();
			if (pxState->m_xBag.PocketStackCount(eCategory) == 0u)
			{
				g_eBagEmptyPocket = (u_int)eCategory;
				g_eBagPhase = BagPhase::SampleEmptyPocket;
				g_iBagPhaseFrames = 0;
				return true;
			}
			if (g_iBagPhaseFrames > iBAG_CYCLE_DEADLINE)
			{
				FailBag("the Next-Pocket confirms never reached an empty pocket");
				return false;
			}
			// One edge every sixth frame: a pocket change re-lays the whole list, so this is
			// spaced wider than the Escape loop below.
			if ((g_iBagPhaseFrames % 6) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case BagPhase::SampleEmptyPocket:
		{
			// CLEAR the canvas focus first. With it still parked on Next-Pocket, Present takes
			// its nav-button branch and the empty-pocket FOCUS FALLBACK -- the branch that has
			// to park the focus itself because no focusable row exists -- would never run.
			// Clearing it makes the next Present exercise exactly that branch, so the sample
			// below reads a focus the SCREEN chose, not one the test set.
			if (g_iBagPhaseFrames == iBAG_FOCUS_FRAME)
			{
				Zenith_UIComponent* pxUI = ResolveMenuRootUI();
				if (pxUI == nullptr)
				{
					FailBag("the ZM_MenuRoot UI component stopped resolving on the empty pocket");
					return false;
				}
				pxUI->GetCanvas().SetFocusedElement(nullptr);
				g_bBagEmptyFocusCleared = true;
				return true;
			}
			if (g_iBagPhaseFrames < iBAG_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving on the empty pocket");
				return false;
			}
			g_iBagCursorOnEmpty   = pxMenu->GetBagScreen().GetCursor();
			g_xBagElementsOnEmpty = ReadBagElements();

			g_eBagPhase = BagPhase::BackToRoot;
			g_iBagPhaseFrames = 0;
			return true;
		}

		case BagPhase::BackToRoot:
		{
			if (g_iBagPhaseFrames == iBAG_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
				return true;
			}
			if (g_iBagPhaseFrames < iBAG_SETTLE_FRAME)
			{
				return true;
			}
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving after the Escape");
				return false;
			}
			g_bBagOpenAfterBack     = pxMenu->IsOpen();
			g_eBagTopAfterBack      = (u_int)pxMenu->GetTopScreen();
			g_xBagElementsAfterBack = ReadBagElements();

			g_eBagPhase = BagPhase::CloseMenu;
			g_iBagPhaseFrames = 0;
			return true;
		}

		case BagPhase::CloseMenu:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailBag("the ZM_UI_MenuStack singleton stopped resolving while closing");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bBagMenuClosed          = true;
				g_bBagFocusClearedOnClose = MenuFocusCleared();

				bool bEnabled = false;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bBagMovementReenabled = bEnabled;
				}

				g_eBagPhase = BagPhase::Done;
				return false;
			}
			if (g_iBagPhaseFrames > iBAG_CLOSE_DEADLINE)
			{
				FailBag("the ROOT menu never closed after the final Escape press");
				return false;
			}
			// Spaced edges (one every fourth frame) so each press is a clean edge.
			if ((g_iBagPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			}
			return true;
		}

		case BagPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMBagScreen()
	{
		bool bPassed = true;

		if (g_bBagActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BagScreen] captured: failed=%s (%s) movBefore=%s rootOpened=%s "
				"rootCursorOnConfirm=%d (want %u) topOnEnter=%u (want BAG=%u) ballStacks=%u "
				"wantVisibleRows=%u money=%u row0Item=%u row0Count=%u catchOrbs=%u cursorOnEnter=%d "
				"pocketOnEnter=%u (want BALL=%u) pocketAfterNext=%u openAfterBack=%s "
				"topAfterBack=%u (want ROOT=%u) closed=%s movReenabled=%s focusCleared=%s",
				g_bBagFailed ? "true" : "false", g_szBagFailure,
				g_bBagMovementEnabledBefore ? "true" : "false",
				g_bBagRootOpened ? "true" : "false",
				g_iBagRootCursorOnConfirm, (u_int)ZM_MENU_ROOT_BAG,
				g_eBagTopOnEnter, (u_int)ZM_MENU_SCREEN_BAG,
				g_uBagFirstPocketStacks, g_uBagExpectedVisibleRows,
				g_uBagMoney, g_eBagRow0Item, g_uBagRow0Count, g_uBagCatchOrbCount,
				g_iBagCursorOnEnter,
				g_eBagPocketOnEnter, (u_int)ZM_ITEM_CATEGORY_BALL,
				g_eBagPocketAfterNext,
				g_bBagOpenAfterBack ? "true" : "false",
				g_eBagTopAfterBack, (u_int)ZM_MENU_SCREEN_ROOT,
				g_bBagMenuClosed ? "true" : "false",
				g_bBagMovementReenabled ? "true" : "false",
				g_bBagFocusClearedOnClose ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BagScreen] nav walk: reachedNavBand=%s focus='%s' | empty pocket=%u (COUNT=%u) "
				"focusCleared=%s rowsVisible=%u row0='%s' row0Focusable=%s focus='%s' cursor=%d",
				g_bBagNavReachedNavBand ? "true" : "false", g_strBagNavFocusName.c_str(),
				g_eBagEmptyPocket, (u_int)ZM_ITEM_CATEGORY_COUNT,
				g_bBagEmptyFocusCleared ? "true" : "false",
				g_xBagElementsOnEmpty.m_uRowsVisible,
				g_xBagElementsOnEmpty.m_astrRowText[0].c_str(),
				g_xBagElementsOnEmpty.m_abRowFocusable[0] ? "true" : "false",
				g_xBagElementsOnEmpty.m_strFocusedName.c_str(),
				g_iBagCursorOnEmpty);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BagScreen] widgets: onEnter(resolved=%s panel=%s/%s header=%s/%s '%s' "
				"navFound=%u navVisible=%u rowsFound=%u rowsVisible=%u row0='%s' focus='%s') "
				"afterNext(header='%s' row0='%s' rowsVisible=%u) afterBack(panel=%s header=%s "
				"rowsVisible=%u navVisible=%u)",
				g_xBagElementsOnEnter.m_bResolved ? "true" : "false",
				g_xBagElementsOnEnter.m_bPanelFound ? "found" : "MISSING",
				g_xBagElementsOnEnter.m_bPanelVisible ? "visible" : "hidden",
				g_xBagElementsOnEnter.m_bHeaderFound ? "found" : "MISSING",
				g_xBagElementsOnEnter.m_bHeaderVisible ? "visible" : "hidden",
				g_xBagElementsOnEnter.m_strHeaderText.c_str(),
				g_xBagElementsOnEnter.m_uNavFound, g_xBagElementsOnEnter.m_uNavVisible,
				g_xBagElementsOnEnter.m_uRowsFound, g_xBagElementsOnEnter.m_uRowsVisible,
				g_xBagElementsOnEnter.m_astrRowText[0].c_str(),
				g_xBagElementsOnEnter.m_strFocusedName.c_str(),
				g_xBagElementsAfterNext.m_strHeaderText.c_str(),
				g_xBagElementsAfterNext.m_astrRowText[0].c_str(),
				g_xBagElementsAfterNext.m_uRowsVisible,
				g_xBagElementsAfterBack.m_bPanelVisible ? "visible" : "hidden",
				g_xBagElementsAfterBack.m_bHeaderVisible ? "visible" : "hidden",
				g_xBagElementsAfterBack.m_uRowsVisible,
				g_xBagElementsAfterBack.m_uNavVisible);

			if (g_bBagFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_BagScreen] %s", g_szBagFailure);
				bPassed = false;
			}

			// The baselines must be meaningful: the player was movable before opening, the
			// opening pocket really holds a stack, and the starter really seeded money and a
			// Catch Orb (otherwise every widget assertion below would pass vacuously).
			if (!g_bBagMovementEnabledBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the player was not movable before opening -- the freeze "
					"assertions would be vacuous");
				bPassed = false;
			}
			if (g_uBagExpectedVisibleRows == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the live BALL pocket is EMPTY (stacks=%u) -- the row assertions "
					"would be vacuous", g_uBagFirstPocketStacks);
				bPassed = false;
			}
			if (g_uBagCatchOrbCount == 0u || g_uBagMoney == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the starter state seeded catchOrbs=%u money=%u -- both must be "
					"non-zero or the label / header assertions would be vacuous",
					g_uBagCatchOrbCount, g_uBagMoney);
				bPassed = false;
			}

			if (!g_bBagRootOpened)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the ROOT menu never opened after the M press");
				bPassed = false;
			}
			if (g_iBagRootCursorOnConfirm != (int)ZM_MENU_ROOT_BAG)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the ROOT cursor was %d when Enter was pressed, expected %u (Bag) "
					"-- the Down press did not land on the Bag entry",
					g_iBagRootCursorOnConfirm, (u_int)ZM_MENU_ROOT_BAG);
				bPassed = false;
			}

			// --- confirm on Bag: the BAG screen is on top and really presented ---
			if (g_eBagTopOnEnter != (u_int)ZM_MENU_SCREEN_BAG)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] top screen after confirming Bag was %u, expected BAG %u",
					g_eBagTopOnEnter, (u_int)ZM_MENU_SCREEN_BAG);
				bPassed = false;
			}
			if (g_eBagPocketOnEnter != (u_int)ZM_ITEM_CATEGORY_BALL)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the bag opened on pocket %u, expected BALL %u",
					g_eBagPocketOnEnter, (u_int)ZM_ITEM_CATEGORY_BALL);
				bPassed = false;
			}
			if (!g_xBagElementsOnEnter.m_bResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the ZM_MenuRoot UI component did not resolve on the bag screen");
				bPassed = false;
			}
			else
			{
				if (!g_xBagElementsOnEnter.m_bPanelFound
					|| !g_xBagElementsOnEnter.m_bHeaderFound
					|| g_xBagElementsOnEnter.m_uRowsFound != ZM_UI_Bag::uROWS_PER_PAGE
					|| g_xBagElementsOnEnter.m_uNavFound != 4u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BagScreen] the AUTHORED bag widgets did not all resolve by name (panel=%s "
						"header=%s rows=%u of %u nav=%u of 4) -- ZM_ConfigureMenuRoot / the "
						"AddStep_CreateUI* contract is broken and NOTHING would render",
						g_xBagElementsOnEnter.m_bPanelFound ? "true" : "false",
						g_xBagElementsOnEnter.m_bHeaderFound ? "true" : "false",
						g_xBagElementsOnEnter.m_uRowsFound, ZM_UI_Bag::uROWS_PER_PAGE,
						g_xBagElementsOnEnter.m_uNavFound);
					bPassed = false;
				}
				if (!g_xBagElementsOnEnter.m_bPanelVisible
					|| !g_xBagElementsOnEnter.m_bHeaderVisible
					|| g_xBagElementsOnEnter.m_uNavVisible != 4u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BagScreen] the bag chrome was not all shown (panel=%s header=%s nav "
						"visible=%u of 4)",
						g_xBagElementsOnEnter.m_bPanelVisible ? "true" : "false",
						g_xBagElementsOnEnter.m_bHeaderVisible ? "true" : "false",
						g_xBagElementsOnEnter.m_uNavVisible);
					bPassed = false;
				}
				// EXACTLY the live rows are visible: a dead row must stay hidden (and
				// non-focusable), or the nav could park on a blank entry.
				if (g_xBagElementsOnEnter.m_uRowsVisible != g_uBagExpectedVisibleRows)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BagScreen] %u rows were visible on page 0, expected %u (the BALL pocket "
						"holds %u stack(s))",
						g_xBagElementsOnEnter.m_uRowsVisible, g_uBagExpectedVisibleRows,
						g_uBagFirstPocketStacks);
					bPassed = false;
				}
				for (u_int u = 0u; u < ZM_UI_Bag::uROWS_PER_PAGE; ++u)
				{
					const bool bWantVisible = (u < g_uBagExpectedVisibleRows);
					// Visibility AND focusability must agree: a dead row that stayed focusable
					// would still be collected by the engine nav and the arrows could park on a
					// blank entry.
					if (g_xBagElementsOnEnter.m_abRowVisible[u] != bWantVisible
						|| g_xBagElementsOnEnter.m_abRowFocusable[u] != bWantVisible)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_BagScreen] row %u visible=%s focusable=%s, expected both %s", u,
							g_xBagElementsOnEnter.m_abRowVisible[u] ? "true" : "false",
							g_xBagElementsOnEnter.m_abRowFocusable[u] ? "true" : "false",
							bWantVisible ? "true" : "false");
						bPassed = false;
					}
				}
				// The header carries the seeded money AND names the pocket.
				const std::string strWantHeader = ZM_UI_Bag::FormatHeader(
					ZM_ITEM_CATEGORY_BALL, g_uBagMoney);
				if (g_xBagElementsOnEnter.m_strHeaderText != strWantHeader)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BagScreen] the header element reads '%s', expected '%s' -- Present is "
						"not writing the pocket name + money",
						g_xBagElementsOnEnter.m_strHeaderText.c_str(), strWantHeader.c_str());
					bPassed = false;
				}
				// Row 0 names the seeded Catch Orb with its real count.
				const std::string strWantRow0 = ZM_UI_Bag::FormatRow(
					(ZM_ITEM_ID)g_eBagRow0Item, g_uBagRow0Count);
				if (g_xBagElementsOnEnter.m_astrRowText[0] != strWantRow0
					|| g_xBagElementsOnEnter.m_astrRowText[0].find(
						ZM_GetItemName(ZM_ITEM_CATCHORB)) == std::string::npos)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BagScreen] row 0 reads '%s', expected '%s' naming the seeded '%s'",
						g_xBagElementsOnEnter.m_astrRowText[0].c_str(), strWantRow0.c_str(),
						ZM_GetItemName(ZM_ITEM_CATCHORB));
					bPassed = false;
				}
			}
			if (g_iBagCursorOnEnter < 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the screen's focused-row mirror was %d, expected a real row -- "
					"the screen must park the focus on a live stack or the arrow keys drive nothing "
					"(canvas focus was '%s')",
					g_iBagCursorOnEnter, g_xBagElementsOnEnter.m_strFocusedName.c_str());
				bPassed = false;
			}

			// --- the nav band is KEY-REACHABLE from the list ---
			// Without this the whole screen could ship with its vertical navigation dead:
			// every other assertion here passes with the pocket / page buttons unreachable.
			if (!g_bBagNavReachedNavBand)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the Down edges never walked off the row list onto a pocket / "
					"page button (the focus stalled on '%s') -- the nav band is unreachable by "
					"keyboard, so the screen cannot be paged or pocket-switched",
					g_strBagNavFocusName.c_str());
				bPassed = false;
			}

			// --- confirm Next-Pocket: the pocket cycles and the rows RELABEL ---
			if (!g_bBagNextPocketFocusParked)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the test never parked the focus on the Next-Pocket button");
				bPassed = false;
			}
			if (g_eBagPocketAfterNext == g_eBagPocketOnEnter
				|| g_eBagPocketAfterNext >= (u_int)ZM_ITEM_CATEGORY_COUNT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] confirming Next-Pocket left the bag on pocket %u (was %u) -- the "
					"by-name confirm dispatch did not change pocket",
					g_eBagPocketAfterNext, g_eBagPocketOnEnter);
				bPassed = false;
			}
			// Changing pocket must actually REFILL the list, not just move a counter.
			if (g_xBagElementsAfterNext.m_astrRowText[0] == g_xBagElementsOnEnter.m_astrRowText[0]
				|| g_xBagElementsAfterNext.m_strHeaderText == g_xBagElementsOnEnter.m_strHeaderText)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the list did not relabel after the pocket change (row0 '%s' -> "
					"'%s', header '%s' -> '%s') -- Present is not refilling from the new pocket",
					g_xBagElementsOnEnter.m_astrRowText[0].c_str(),
					g_xBagElementsAfterNext.m_astrRowText[0].c_str(),
					g_xBagElementsOnEnter.m_strHeaderText.c_str(),
					g_xBagElementsAfterNext.m_strHeaderText.c_str());
				bPassed = false;
			}

			// --- an EMPTY pocket: one visible, NON-focusable notice row and a parked focus ---
			if (g_eBagEmptyPocket >= (u_int)ZM_ITEM_CATEGORY_COUNT || !g_bBagEmptyFocusCleared)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the cycle never reached an empty pocket (pocket=%u focusCleared=%s) "
					"-- the empty-pocket notice and its focus fallback are unverified",
					g_eBagEmptyPocket, g_bBagEmptyFocusCleared ? "true" : "false");
				bPassed = false;
			}
			else
			{
				const std::string strWantEmpty = ZM_UI_Bag::FormatEmptyPocket();
				if (g_xBagElementsOnEmpty.m_uRowsVisible != 1u
					|| g_xBagElementsOnEmpty.m_astrRowText[0] != strWantEmpty
					|| g_xBagElementsOnEmpty.m_abRowFocusable[0])
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BagScreen] the empty pocket %u showed %u visible row(s), row0='%s' "
						"focusable=%s -- expected exactly ONE visible row reading '%s' that the nav "
						"cannot select (it is a message, not an item)",
						g_eBagEmptyPocket, g_xBagElementsOnEmpty.m_uRowsVisible,
						g_xBagElementsOnEmpty.m_astrRowText[0].c_str(),
						g_xBagElementsOnEmpty.m_abRowFocusable[0] ? "true" : "false",
						strWantEmpty.c_str());
					bPassed = false;
				}
				// The test cleared the focus, so this is the SCREEN's own fallback: with no
				// focusable row it must park the focus on a nav button and report no cursor,
				// otherwise the arrows would drive nothing and the player would be stuck.
				if (g_xBagElementsOnEmpty.m_strFocusedName != ZM_UI_Bag::szNEXT_POCKET_NAME
					|| g_iBagCursorOnEmpty != -1)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BagScreen] over the empty pocket %u the focus was '%s' and the cursor %d, "
						"expected the '%s' button and -1 -- the empty-pocket focus fallback did not run",
						g_eBagEmptyPocket, g_xBagElementsOnEmpty.m_strFocusedName.c_str(),
						g_iBagCursorOnEmpty, ZM_UI_Bag::szNEXT_POCKET_NAME);
					bPassed = false;
				}
			}

			// --- Escape: back on ROOT with every bag widget down ---
			if (!g_bBagOpenAfterBack || g_eBagTopAfterBack != (u_int)ZM_MENU_SCREEN_ROOT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] after the Escape the menu was open=%s on top=%u, expected the "
					"ROOT %u to be restored",
					g_bBagOpenAfterBack ? "true" : "false",
					g_eBagTopAfterBack, (u_int)ZM_MENU_SCREEN_ROOT);
				bPassed = false;
			}
			// The *Found flags prove the capture actually ran (a default-initialised view
			// would pass the "hidden" checks vacuously).
			if (!g_xBagElementsAfterBack.m_bPanelFound
				|| g_xBagElementsAfterBack.m_uRowsFound != ZM_UI_Bag::uROWS_PER_PAGE
				|| g_xBagElementsAfterBack.m_bPanelVisible
				|| g_xBagElementsAfterBack.m_bHeaderVisible
				|| g_xBagElementsAfterBack.m_uRowsVisible != 0u
				|| g_xBagElementsAfterBack.m_uNavVisible != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the bag widgets were not all hidden once the screen popped "
					"(panel found=%s visible=%s, rows found=%u visible=%u, header visible=%s, nav "
					"visible=%u)",
					g_xBagElementsAfterBack.m_bPanelFound ? "true" : "false",
					g_xBagElementsAfterBack.m_bPanelVisible ? "true" : "false",
					g_xBagElementsAfterBack.m_uRowsFound,
					g_xBagElementsAfterBack.m_uRowsVisible,
					g_xBagElementsAfterBack.m_bHeaderVisible ? "true" : "false",
					g_xBagElementsAfterBack.m_uNavVisible);
				bPassed = false;
			}

			// --- the last Escape: the menu closes and the player is movable again ---
			if (!g_bBagMenuClosed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the menu never closed after the final Escape press");
				bPassed = false;
			}
			if (!g_bBagMovementReenabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] player movement was not re-enabled after the menu closed");
				bPassed = false;
			}
			if (!g_bBagFocusClearedOnClose)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BagScreen] the canvas focus was not cleared (nullptr) after the menu "
					"closed -- arrow keys could still drive the hidden menu");
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), exactly like the sibling tests.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_bBagActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bBagActive = false;

		return bPassed || !g_bBagPrereqsPresent;
	}

	// =========================================================================
	// ZM_ShopScreen_Test (S6 item 2 SC7) -- the windowed gate for the mart, and the
	// first proof that a menu screen can WRITE the live ZM_GameState. In a
	// runtime-ready Dawnmere:
	//   ZM_UI_MenuStack::TryOpenShop (the S6 item 3 mart-NPC seam) raises the SHOP
	//   screen straight over the overworld with NO pause menu involved -> the player
	//   is frozen, the authored panel / header / six rows / eight controls all resolve
	//   by name, exactly VisibleRowCount(page 0) rows are shown and they list the
	//   configured stock WITH its table prices -> Down edges WALK the focus off the row
	//   list onto the CONFIRM control (real arrow keys, deadline-guarded: parking the
	//   focus programmatically would prove nothing about whether the screen is playable)
	//   -> ONE Enter buys the SELECTED entry -> the LIVE money is down by exactly
	//   price x quantity and the LIVE bag is up by exactly the quantity -> Escape closes
	//   the shop and movement is re-enabled.
	//
	// The purchase assertions are anchored on the entry the CURSOR is actually on when
	// Enter lands (read off the screen at the press frame), not on a guessed row: the
	// walk decides where the selection ends up, and the point of the test is that the
	// selection SURVIVES the walk onto the control.
	// =========================================================================

	// The mart's stock. Both purchasable, in DIFFERENT pockets, and cheap enough for the
	// starter purse (the fixture asserts the price is real rather than assuming it).
	constexpr u_int uSHOP_STOCK_COUNT = 2u;
	const ZM_ITEM_ID aeSHOP_STOCK[uSHOP_STOCK_COUNT] = { ZM_ITEM_CATCHORB, ZM_ITEM_SALVE };
	static_assert(uSHOP_STOCK_COUNT <= ZM_UI_Shop::uROWS_PER_PAGE,
		"the shop fixture must fit on page 0 -- the row assertions below assume no paging");

	// What the AUTHORED shop widgets actually report this frame.
	struct ShopElementView
	{
		bool        m_bResolved       = false;   // the ZM_MenuRoot UI component resolved
		bool        m_bPanelFound     = false;
		bool        m_bPanelVisible   = false;
		bool        m_bHeaderFound    = false;
		bool        m_bHeaderVisible  = false;
		std::string m_strHeaderText;
		u_int       m_uControlsFound  = 0u;      // of the eight
		u_int       m_uControlsVisible = 0u;
		u_int       m_uRowsFound      = 0u;
		u_int       m_uRowsVisible    = 0u;
		bool        m_abRowVisible[ZM_UI_Shop::uROWS_PER_PAGE] = {};
		// Focusability is HALF the contract: a dead row must be unreachable by the engine
		// nav, which collects visible + FOCUSABLE elements.
		bool        m_abRowFocusable[ZM_UI_Shop::uROWS_PER_PAGE] = {};
		std::string m_astrRowText[ZM_UI_Shop::uROWS_PER_PAGE];
		std::string m_strFocusedName;            // the canvas focus (want a row, then Confirm)
	};

	ShopElementView ReadShopElements()
	{
		ShopElementView xView;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;

		if (Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Shop::szPANEL_NAME))
		{
			xView.m_bPanelFound = true;
			xView.m_bPanelVisible = pxPanel->IsVisible();
		}
		if (Zenith_UI::Zenith_UIText* pxHeader =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Shop::szHEADER_NAME))
		{
			xView.m_bHeaderFound = true;
			xView.m_bHeaderVisible = pxHeader->IsVisible();
			xView.m_strHeaderText = pxHeader->GetText();
		}
		for (u_int u = 0u; u < ZM_UI_Shop::uCONTROL_COUNT; ++u)
		{
			Zenith_UI::Zenith_UIElement* pxControl =
				pxUI->FindElement(ZM_UI_Shop::ControlElementName(u));
			if (pxControl == nullptr)
			{
				continue;
			}
			++xView.m_uControlsFound;
			if (pxControl->IsVisible())
			{
				++xView.m_uControlsVisible;
			}
		}
		for (u_int u = 0u; u < ZM_UI_Shop::uROWS_PER_PAGE; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxRow =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Shop::RowElementName(u));
			if (pxRow == nullptr)
			{
				continue;
			}
			++xView.m_uRowsFound;
			xView.m_abRowVisible[u] = pxRow->IsVisible();
			xView.m_abRowFocusable[u] = pxRow->IsFocusable();
			xView.m_astrRowText[u] = pxRow->GetText();
			if (xView.m_abRowVisible[u])
			{
				++xView.m_uRowsVisible;
			}
		}
		if (Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement())
		{
			xView.m_strFocusedName = pxFocused->GetName();
		}
		return xView;
	}

	enum class ShopPhase
	{
		AwaitReady,
		OpenShop,        // raise the mart via TryOpenShop + capture the open state
		SampleOnOpen,    // ...and what the authored widgets are showing
		WalkToConfirm,   // spaced Down edges: the ENGINE spatial nav walks onto CONFIRM
		BuySelected,     // ONE Enter edge -> the selected entry is bought
		CloseShop,       // Escape edges until the shop closes
		Done,
	};

	constexpr int iSHOP_READY_DEADLINE = 420;   // Dawnmere first-load ready window (sibling parity)
	constexpr int iSHOP_PRESS_FRAME    = 2;     // the frame within a press phase that emits the edge
	constexpr int iSHOP_SETTLE_FRAME   = 6;     // ...and the frame the outcome is sampled on
	constexpr int iSHOP_WALK_DEADLINE  = 200;   // frames for the spatial nav to reach CONFIRM
	constexpr int iSHOP_CLOSE_DEADLINE = 120;   // frames for the Escape to close the shop

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	ShopPhase g_eShopPhase          = ShopPhase::Done;
	int       g_iShopPhaseFrames    = 0;
	bool      g_bShopPrereqsPresent = false;
	bool      g_bShopActive         = false;
	bool      g_bShopFailed         = false;
	const char* g_szShopFailure     = "test did not reach verification";

	// ---- Captured observations ----
	bool  g_bShopMovementEnabledBefore = false;   // player movable BEFORE the shop (baseline)
	bool  g_bShopOpenAccepted          = false;   // TryOpenShop returned true
	bool  g_bShopMenuOpened            = false;   // the menu reported open after the raise
	u_int g_eShopTopOnOpen             = (u_int)ZM_MENU_SCREEN_NONE;   // want SHOP
	u_int g_uShopDepthOnOpen           = 0u;      // want 1 (the shop alone -- no pause menu)
	bool  g_bShopPlayerFrozen          = false;   // player movement disabled while trading
	u_int g_eShopModeOnOpen            = (u_int)ZM_SHOP_MODE_COUNT;   // want BUY
	u_int g_uShopQtyOnOpen             = 0u;      // want 1
	u_int g_uShopExpectedVisibleRows   = 0u;      // == VisibleRowCount(0, the stock count)
	u_int g_uShopMoneyOnOpen           = 0u;      // the LIVE purse when the widgets were sampled
	int   g_iShopCursorOnOpen          = -99;     // the screen's selection mirror (want >= 0)

	// ---- The Down walk onto CONFIRM (real arrow edges -- defaults to FAILING) ----
	bool        g_bShopReachedConfirm  = false;
	std::string g_strShopWalkFocusName;           // where the focus ended up (or stalled)
	// The screen's selection the LAST frame the focus was still on a list row. The walk
	// must move off row 0 (or the survival check below is vacuous), and the selection must
	// still read this once the focus has landed on the control.
	int         g_iShopCursorOnLastRow = -99;

	// ---- The purchase, anchored on the entry the cursor is ACTUALLY on ----
	int   g_iShopCursorAtConfirm  = -99;
	u_int g_eShopBoughtItem       = (u_int)ZM_ITEM_NONE;
	u_int g_uShopBoughtPrice      = 0u;
	u_int g_uShopBoughtQuantity   = 0u;
	u_int g_uShopMoneyBeforeBuy   = 0u;
	u_int g_uShopMoneyAfterBuy    = 0u;
	u_int g_uShopHeldBeforeBuy    = 0u;
	u_int g_uShopHeldAfterBuy     = 0u;
	u_int g_eShopResultAfterBuy   = (u_int)ZM_SHOP_RESULT_COUNT;   // want ZM_SHOP_OK
	bool  g_bShopReportedResult   = false;

	// ---- The close ----
	bool g_bShopClosed             = false;
	bool g_bShopMovementReenabled  = false;
	bool g_bShopFocusClearedOnClose = false;

	// ---- The AUTHORED widgets ----
	ShopElementView g_xShopElementsOnOpen;    // want: panel + header + every page-0 row shown
	ShopElementView g_xShopElementsOnClose;   // want: everything shop-side hidden again

	void FailShop(const char* szReason)
	{
		g_szShopFailure = szReason;
		g_bShopFailed = true;
		g_eShopPhase = ShopPhase::Done;
	}

	// The live purse + the held count of one item, or false when no game state resolves.
	bool ReadShopState(ZM_ITEM_ID eItem, u_int& uMoneyOut, u_int& uHeldOut)
	{
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return false;
		}
		uMoneyOut = pxState->m_uMoney;
		uHeldOut = ((u_int)eItem < (u_int)ZM_ITEM_COUNT) ? pxState->m_xBag.GetCount(eItem) : 0u;
		return true;
	}

	void Setup_ZMShopScreen()
	{
		g_eShopPhase                 = ShopPhase::Done;
		g_iShopPhaseFrames           = 0;
		g_bShopActive                = false;
		g_bShopFailed                = false;
		g_szShopFailure              = "test did not reach verification";

		g_bShopMovementEnabledBefore = false;
		g_bShopOpenAccepted          = false;
		g_bShopMenuOpened            = false;
		g_eShopTopOnOpen             = (u_int)ZM_MENU_SCREEN_NONE;
		g_uShopDepthOnOpen           = 0u;
		g_bShopPlayerFrozen          = false;
		g_eShopModeOnOpen            = (u_int)ZM_SHOP_MODE_COUNT;
		g_uShopQtyOnOpen             = 0u;
		g_uShopExpectedVisibleRows   = 0u;
		g_uShopMoneyOnOpen           = 0u;
		g_iShopCursorOnOpen          = -99;

		g_bShopReachedConfirm        = false;
		g_strShopWalkFocusName.clear();
		g_iShopCursorOnLastRow       = -99;

		g_iShopCursorAtConfirm       = -99;
		g_eShopBoughtItem            = (u_int)ZM_ITEM_NONE;
		g_uShopBoughtPrice           = 0u;
		g_uShopBoughtQuantity        = 0u;
		g_uShopMoneyBeforeBuy        = 0u;
		g_uShopMoneyAfterBuy         = 0u;
		g_uShopHeldBeforeBuy         = 0u;
		g_uShopHeldAfterBuy          = 0u;
		g_eShopResultAfterBuy        = (u_int)ZM_SHOP_RESULT_COUNT;
		g_bShopReportedResult        = false;

		g_bShopClosed                = false;
		g_bShopMovementReenabled     = false;
		g_bShopFocusClearedOnClose   = false;

		g_xShopElementsOnOpen        = ShopElementView{};
		g_xShopElementsOnClose       = ShopElementView{};

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO process
		// state (fixed dt, scene load) until every git-ignored input is confirmed
		// present. CI has no baked Assets tree -> skip.
		g_bShopPrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bShopPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere assets absent -- run a *_True build");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fUI_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eShopPhase = ShopPhase::AwaitReady;
		g_bShopActive = true;
	}

	bool Step_ZMShopScreen(int)
	{
		if (!g_bShopActive || g_bShopFailed || g_eShopPhase == ShopPhase::Done)
		{
			return false;
		}

		++g_iShopPhaseFrames;
		switch (g_eShopPhase)
		{
		case ShopPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iShopPhaseFrames > iSHOP_READY_DEADLINE)
				{
					FailShop("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}
			if (ResolveSingletonMenuStack() == nullptr)
			{
				FailShop("no unique ZM_UI_MenuStack singleton (ZM_MenuRoot missing)");
				return false;
			}

			g_bShopMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();
			g_uShopExpectedVisibleRows = ZM_UI_Shop::VisibleRowCount(0u, uSHOP_STOCK_COUNT);

			g_eShopPhase = ShopPhase::OpenShop;
			g_iShopPhaseFrames = 0;
			return true;
		}

		case ShopPhase::OpenShop:
		{
			// The mart seam: a static singleton call, exactly what S6 item 3's clerk will
			// make. Pushed ONCE, on the first frame of the phase, over an EMPTY stack -- a
			// shop must be reachable without the pause menu.
			if (g_iShopPhaseFrames == 1)
			{
				g_bShopOpenAccepted = ZM_UI_MenuStack::TryOpenShop(aeSHOP_STOCK, uSHOP_STOCK_COUNT);
				if (!g_bShopOpenAccepted)
				{
					FailShop("TryOpenShop rejected the two-item mart inventory");
					return false;
				}
				return true;
			}
			if (g_iShopPhaseFrames < iSHOP_SETTLE_FRAME)
			{
				return true;   // let a Present run so the widgets are filled before sampling
			}

			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_UI_MenuStack singleton stopped resolving after the shop raise");
				return false;
			}
			g_bShopMenuOpened  = pxMenu->IsOpen();
			g_eShopTopOnOpen   = (u_int)pxMenu->GetTopScreen();
			g_uShopDepthOnOpen = pxMenu->GetDepth();

			bool bEnabled = true;
			if (ReadActivePlayerMovementEnabled(bEnabled))
			{
				g_bShopPlayerFrozen = !bEnabled;
			}
			if (!g_bShopMenuOpened || g_eShopTopOnOpen != (u_int)ZM_MENU_SCREEN_SHOP)
			{
				// TryOpenShop is SYNCHRONOUS (it pushes and presents inside the call), so there is
				// nothing to wait for: if the screen is not up by the settle frame it never will be.
				FailShop("the shop screen never came up after TryOpenShop");
				return false;
			}

			g_eShopPhase = ShopPhase::SampleOnOpen;
			g_iShopPhaseFrames = 0;
			return true;
		}

		case ShopPhase::SampleOnOpen:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_UI_MenuStack singleton stopped resolving while sampling the shop");
				return false;
			}
			const ZM_UI_Shop& xShop = pxMenu->GetShopScreen();
			g_eShopModeOnOpen   = (u_int)xShop.GetMode();
			g_uShopQtyOnOpen    = xShop.GetQuantity();
			g_iShopCursorOnOpen = xShop.GetCursor();

			u_int uHeld = 0u;
			if (!ReadShopState(aeSHOP_STOCK[0], g_uShopMoneyOnOpen, uHeld))
			{
				FailShop("the live game state did not resolve while sampling the shop");
				return false;
			}
			g_xShopElementsOnOpen = ReadShopElements();

			g_eShopPhase = ShopPhase::WalkToConfirm;
			g_iShopPhaseFrames = 0;
			return true;
		}

		case ShopPhase::WalkToConfirm:
		{
			// Spaced Down edges from the focused row. NOTHING on this screen carries explicit
			// navigation links, so this walks purely on the ENGINE SPATIAL search -- and it is
			// the traversal the whole screen depends on, because CONFIRM is the only element
			// that moves money. It MUST be driven by real key edges: parking the focus by hand
			// would prove nothing about whether a player can ever reach the button.
			Zenith_UIComponent* pxUI = ResolveMenuRootUI();
			if (pxUI == nullptr)
			{
				FailShop("the ZM_MenuRoot UI component stopped resolving while walking to Confirm");
				return false;
			}
			Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
			// While the focus is still ON the list, mirror the screen's selection. The LAST
			// value captured here is the one the selection must STILL read once the walk has
			// landed on the control -- without it the survival deviation this screen makes from
			// the bag (ZM_UI_Shop::SettleFocus) is untested: resetting the cursor to 0 on a
			// control would simply buy stock entry 0 and every assertion below would still pass.
			if (pxFocused != nullptr
				&& ZM_UI_Shop::RowIndexFromElementName(pxFocused->GetName().c_str()) >= 0)
			{
				if (ZM_UI_MenuStack* pxWalkMenu = ResolveSingletonMenuStack())
				{
					g_iShopCursorOnLastRow = pxWalkMenu->GetShopScreen().GetCursor();
				}
			}
			if (pxFocused != nullptr && pxFocused->GetName() == ZM_UI_Shop::szCONFIRM_NAME)
			{
				g_bShopReachedConfirm = true;
				g_strShopWalkFocusName = pxFocused->GetName();
				g_eShopPhase = ShopPhase::BuySelected;
				g_iShopPhaseFrames = 0;
				return true;
			}
			if (g_iShopPhaseFrames > iSHOP_WALK_DEADLINE)
			{
				g_strShopWalkFocusName = (pxFocused != nullptr) ? pxFocused->GetName() : std::string();
				FailShop("the Down edges never walked the focus onto the Confirm control");
				return false;
			}
			if ((g_iShopPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_DOWN);
			}
			return true;
		}

		case ShopPhase::BuySelected:
		{
			// ONE Enter edge, with idle frames on either side so the edge-detected
			// ZM_InputActions::ReadConfirmPressed fires exactly once. The purchase is anchored
			// on the entry the CURSOR is on at the press frame -- the walk above decided that,
			// and the selection SURVIVING the walk is exactly what is under test.
			if (g_iShopPhaseFrames == iSHOP_PRESS_FRAME)
			{
				ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
				if (pxMenu == nullptr)
				{
					FailShop("the ZM_UI_MenuStack singleton stopped resolving before the purchase");
					return false;
				}
				const ZM_UI_Shop& xShop = pxMenu->GetShopScreen();
				g_iShopCursorAtConfirm = xShop.GetCursor();
				g_uShopBoughtQuantity = xShop.GetQuantity();

				ZM_GameState* pxState = nullptr;
				if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
				{
					FailShop("the live game state did not resolve before the purchase");
					return false;
				}
				// The FLAT entry index resolved BY THE SCREEN -- never the raw cursor, which is a
				// PAGE-RELATIVE row and would name the wrong stock entry on any page but the first.
				const int iEntry = xShop.GetSelectedEntryIndex(pxState->m_xBag);
				const ZM_ITEM_ID eItem = (iEntry >= 0)
					? xShop.GetInventoryItem((u_int)iEntry)
					: ZM_ITEM_NONE;
				g_eShopBoughtItem = (u_int)eItem;
				g_uShopBoughtPrice = ZM_ShopBuyPrice(eItem);
				if (!ReadShopState(eItem, g_uShopMoneyBeforeBuy, g_uShopHeldBeforeBuy))
				{
					FailShop("the live game state did not resolve before the purchase");
					return false;
				}

				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iShopPhaseFrames < iSHOP_SETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_UI_MenuStack singleton stopped resolving after the purchase");
				return false;
			}
			g_eShopResultAfterBuy = (u_int)pxMenu->GetShopScreen().GetLastResult();
			g_bShopReportedResult = pxMenu->GetShopScreen().HasResult();
			if (!ReadShopState((ZM_ITEM_ID)g_eShopBoughtItem, g_uShopMoneyAfterBuy, g_uShopHeldAfterBuy))
			{
				FailShop("the live game state did not resolve after the purchase");
				return false;
			}

			g_eShopPhase = ShopPhase::CloseShop;
			g_iShopPhaseFrames = 0;
			return true;
		}

		case ShopPhase::CloseShop:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailShop("the ZM_UI_MenuStack singleton stopped resolving while closing the shop");
				return false;
			}
			if (!pxMenu->IsOpen())
			{
				g_bShopClosed = true;
				g_bShopFocusClearedOnClose = MenuFocusCleared();
				// CloseMenu hides the widgets directly (the per-frame Present stops running the
				// moment the stack empties), so they must be down here.
				g_xShopElementsOnClose = ReadShopElements();

				bool bEnabled = false;
				if (ReadActivePlayerMovementEnabled(bEnabled))
				{
					g_bShopMovementReenabled = bEnabled;
				}

				g_eShopPhase = ShopPhase::Done;
				return false;
			}
			if (g_iShopPhaseFrames > iSHOP_CLOSE_DEADLINE)
			{
				FailShop("the shop never closed after the Escape press");
				return false;
			}
			// Spaced edges (one every fourth frame) so each press is a clean edge. The shop
			// was raised over an EMPTY stack, so one pop empties it and closes the menu.
			if ((g_iShopPhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
			}
			return true;
		}

		case ShopPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMShopScreen()
	{
		bool bPassed = true;

		if (g_bShopActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_ShopScreen] captured: failed=%s (%s) movBefore=%s openAccepted=%s opened=%s "
				"top=%u (want SHOP=%u) depth=%u (want 1) frozen=%s mode=%u (want BUY=%u) qty=%u "
				"cursorOnOpen=%d wantVisibleRows=%u moneyOnOpen=%u closed=%s movReenabled=%s "
				"focusCleared=%s",
				g_bShopFailed ? "true" : "false", g_szShopFailure,
				g_bShopMovementEnabledBefore ? "true" : "false",
				g_bShopOpenAccepted ? "true" : "false",
				g_bShopMenuOpened ? "true" : "false",
				g_eShopTopOnOpen, (u_int)ZM_MENU_SCREEN_SHOP,
				g_uShopDepthOnOpen,
				g_bShopPlayerFrozen ? "true" : "false",
				g_eShopModeOnOpen, (u_int)ZM_SHOP_MODE_BUY,
				g_uShopQtyOnOpen, g_iShopCursorOnOpen,
				g_uShopExpectedVisibleRows, g_uShopMoneyOnOpen,
				g_bShopClosed ? "true" : "false",
				g_bShopMovementReenabled ? "true" : "false",
				g_bShopFocusClearedOnClose ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_ShopScreen] purchase: reachedConfirm=%s focus='%s' cursorOnLastRow=%d "
				"cursorAtConfirm=%d item=%u "
				"('%s') price=%u qty=%u money %u->%u held %u->%u result=%u (want OK=%u) reported=%s",
				g_bShopReachedConfirm ? "true" : "false", g_strShopWalkFocusName.c_str(),
				g_iShopCursorOnLastRow,
				g_iShopCursorAtConfirm, g_eShopBoughtItem,
				ZM_GetItemName((ZM_ITEM_ID)g_eShopBoughtItem),
				g_uShopBoughtPrice, g_uShopBoughtQuantity,
				g_uShopMoneyBeforeBuy, g_uShopMoneyAfterBuy,
				g_uShopHeldBeforeBuy, g_uShopHeldAfterBuy,
				g_eShopResultAfterBuy, (u_int)ZM_SHOP_OK,
				g_bShopReportedResult ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_ShopScreen] widgets: onOpen(resolved=%s panel=%s/%s header=%s/%s '%s' "
				"controlsFound=%u controlsVisible=%u rowsFound=%u rowsVisible=%u row0='%s' row1='%s' "
				"focus='%s') onClose(panelFound=%s panelVisible=%s headerVisible=%s rowsVisible=%u "
				"controlsVisible=%u)",
				g_xShopElementsOnOpen.m_bResolved ? "true" : "false",
				g_xShopElementsOnOpen.m_bPanelFound ? "found" : "MISSING",
				g_xShopElementsOnOpen.m_bPanelVisible ? "visible" : "hidden",
				g_xShopElementsOnOpen.m_bHeaderFound ? "found" : "MISSING",
				g_xShopElementsOnOpen.m_bHeaderVisible ? "visible" : "hidden",
				g_xShopElementsOnOpen.m_strHeaderText.c_str(),
				g_xShopElementsOnOpen.m_uControlsFound, g_xShopElementsOnOpen.m_uControlsVisible,
				g_xShopElementsOnOpen.m_uRowsFound, g_xShopElementsOnOpen.m_uRowsVisible,
				g_xShopElementsOnOpen.m_astrRowText[0].c_str(),
				g_xShopElementsOnOpen.m_astrRowText[1].c_str(),
				g_xShopElementsOnOpen.m_strFocusedName.c_str(),
				g_xShopElementsOnClose.m_bPanelFound ? "true" : "false",
				g_xShopElementsOnClose.m_bPanelVisible ? "true" : "false",
				g_xShopElementsOnClose.m_bHeaderVisible ? "true" : "false",
				g_xShopElementsOnClose.m_uRowsVisible,
				g_xShopElementsOnClose.m_uControlsVisible);

			if (g_bShopFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_ShopScreen] %s", g_szShopFailure);
				bPassed = false;
			}

			// The baseline must be meaningful: the player was movable before the shop opened.
			if (!g_bShopMovementEnabledBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the player was not movable before the shop opened -- the freeze "
					"assertions would be vacuous");
				bPassed = false;
			}

			// --- the raise: SHOP on top of an otherwise EMPTY stack, player frozen ---
			if (!g_bShopOpenAccepted || !g_bShopMenuOpened)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] TryOpenShop accepted=%s and the menu opened=%s -- the mart seam "
					"S6 item 3 hangs its clerk off did not raise the screen",
					g_bShopOpenAccepted ? "true" : "false", g_bShopMenuOpened ? "true" : "false");
				bPassed = false;
			}
			if (g_eShopTopOnOpen != (u_int)ZM_MENU_SCREEN_SHOP)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] top screen after the raise was %u, expected SHOP %u",
					g_eShopTopOnOpen, (u_int)ZM_MENU_SCREEN_SHOP);
				bPassed = false;
			}
			if (g_uShopDepthOnOpen != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] stack depth after the raise was %u, expected 1 -- a mart must be "
					"reachable WITHOUT the pause menu", g_uShopDepthOnOpen);
				bPassed = false;
			}
			if (!g_bShopPlayerFrozen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the player was NOT frozen while the shop was open");
				bPassed = false;
			}
			if (g_eShopModeOnOpen != (u_int)ZM_SHOP_MODE_BUY || g_uShopQtyOnOpen != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the shop opened in mode %u qty %u, expected BUY %u qty 1",
					g_eShopModeOnOpen, g_uShopQtyOnOpen, (u_int)ZM_SHOP_MODE_BUY);
				bPassed = false;
			}

			// --- the AUTHORED widgets really render the stock ---
			if (!g_xShopElementsOnOpen.m_bResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the ZM_MenuRoot UI component did not resolve on the shop screen");
				bPassed = false;
			}
			else
			{
				if (!g_xShopElementsOnOpen.m_bPanelFound
					|| !g_xShopElementsOnOpen.m_bHeaderFound
					|| g_xShopElementsOnOpen.m_uRowsFound != ZM_UI_Shop::uROWS_PER_PAGE
					|| g_xShopElementsOnOpen.m_uControlsFound != ZM_UI_Shop::uCONTROL_COUNT)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_ShopScreen] the AUTHORED shop widgets did not all resolve by name (panel=%s "
						"header=%s rows=%u of %u controls=%u of %u) -- ZM_ConfigureMenuRoot / the "
						"AddStep_CreateUI* contract is broken and NOTHING would render",
						g_xShopElementsOnOpen.m_bPanelFound ? "true" : "false",
						g_xShopElementsOnOpen.m_bHeaderFound ? "true" : "false",
						g_xShopElementsOnOpen.m_uRowsFound, ZM_UI_Shop::uROWS_PER_PAGE,
						g_xShopElementsOnOpen.m_uControlsFound, ZM_UI_Shop::uCONTROL_COUNT);
					bPassed = false;
				}
				if (!g_xShopElementsOnOpen.m_bPanelVisible
					|| !g_xShopElementsOnOpen.m_bHeaderVisible
					|| g_xShopElementsOnOpen.m_uControlsVisible != ZM_UI_Shop::uCONTROL_COUNT)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_ShopScreen] the shop chrome was not all shown (panel=%s header=%s controls "
						"visible=%u of %u)",
						g_xShopElementsOnOpen.m_bPanelVisible ? "true" : "false",
						g_xShopElementsOnOpen.m_bHeaderVisible ? "true" : "false",
						g_xShopElementsOnOpen.m_uControlsVisible, ZM_UI_Shop::uCONTROL_COUNT);
					bPassed = false;
				}
				// EXACTLY the stocked rows are visible AND focusable; the rest are neither.
				if (g_xShopElementsOnOpen.m_uRowsVisible != g_uShopExpectedVisibleRows)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_ShopScreen] %u rows were visible on page 0, expected %u (the mart stocks 2)",
						g_xShopElementsOnOpen.m_uRowsVisible, g_uShopExpectedVisibleRows);
					bPassed = false;
				}
				for (u_int u = 0u; u < ZM_UI_Shop::uROWS_PER_PAGE; ++u)
				{
					const bool bWantVisible = (u < g_uShopExpectedVisibleRows);
					if (g_xShopElementsOnOpen.m_abRowVisible[u] != bWantVisible
						|| g_xShopElementsOnOpen.m_abRowFocusable[u] != bWantVisible)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_ShopScreen] row %u visible=%s focusable=%s, expected both %s", u,
							g_xShopElementsOnOpen.m_abRowVisible[u] ? "true" : "false",
							g_xShopElementsOnOpen.m_abRowFocusable[u] ? "true" : "false",
							bWantVisible ? "true" : "false");
						bPassed = false;
					}
				}
				// The rows list the STOCK, with its table prices.
				for (u_int u = 0u; u < g_uShopExpectedVisibleRows && u < 2u; ++u)
				{
					const std::string strWantRow = ZM_UI_Shop::FormatBuyRow(aeSHOP_STOCK[u]);
					if (g_xShopElementsOnOpen.m_astrRowText[u] != strWantRow)
					{
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ZM_ShopScreen] row %u reads '%s', expected '%s' -- Present is not listing the "
							"configured stock with its price", u,
							g_xShopElementsOnOpen.m_astrRowText[u].c_str(), strWantRow.c_str());
						bPassed = false;
					}
				}
				// ...and the header carries the live purse (no transaction yet, so no report).
				const std::string strWantHeader = ZM_UI_Shop::FormatHeaderLine(
					ZM_SHOP_MODE_BUY, g_uShopMoneyOnOpen, 1u, /* bHasResult */ false, ZM_SHOP_OK);
				if (g_xShopElementsOnOpen.m_strHeaderText != strWantHeader)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_ShopScreen] the header element reads '%s', expected '%s'",
						g_xShopElementsOnOpen.m_strHeaderText.c_str(), strWantHeader.c_str());
					bPassed = false;
				}
			}
			if (g_iShopCursorOnOpen < 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the screen's selection mirror was %d on open, expected a real entry "
					"-- with nothing selected the Confirm button could never transact (canvas focus was "
					"'%s')", g_iShopCursorOnOpen, g_xShopElementsOnOpen.m_strFocusedName.c_str());
				bPassed = false;
			}

			// --- CONFIRM is KEY-REACHABLE from the list ---
			// Without this the whole screen could ship unusable: every other assertion here
			// passes with the one button that moves money unreachable by keyboard.
			if (!g_bShopReachedConfirm)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the Down edges never walked the focus onto the Confirm control (the "
					"focus stalled on '%s') -- the shop cannot be transacted with by keyboard",
					g_strShopWalkFocusName.c_str());
				bPassed = false;
			}

			// --- the purchase moved the LIVE game state by exactly the right amounts ---
			if (g_iShopCursorAtConfirm < 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the selection was %d when Confirm was pressed -- the SELECTION did "
					"not survive the walk off the list onto the control, so Confirm had nothing to buy",
					g_iShopCursorAtConfirm);
				bPassed = false;
			}
			// The survival contract itself, and it has to be pinned by VALUE: with the cursor
			// simply reset to 0 on a control the purchase assertions below all still pass (they
			// are derived from whatever the cursor reads at the press frame), so only comparing
			// the selection ACROSS the walk can catch it. The walk must also have moved off row
			// 0 first, or the comparison is vacuous -- with a two-item mart it passes through
			// row 1 on its way to Confirm.
			if (g_iShopCursorOnLastRow <= 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the selection was %d on the last frame the focus was on a list "
					"row -- the walk never moved off row 0, so the selection-survival check below "
					"would prove nothing", g_iShopCursorOnLastRow);
				bPassed = false;
			}
			else if (g_iShopCursorAtConfirm != g_iShopCursorOnLastRow)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the selection was %d on the last list row but %d once the focus "
					"reached Confirm -- the SELECTION did not SURVIVE the walk onto the control "
					"(ZM_UI_Shop::SettleFocus), so Confirm buys whatever row 0 happens to be rather "
					"than what the player picked",
					g_iShopCursorOnLastRow, g_iShopCursorAtConfirm);
				bPassed = false;
			}
			if (g_eShopBoughtItem >= (u_int)ZM_ITEM_COUNT || g_uShopBoughtPrice == 0u
				|| g_uShopBoughtQuantity == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the selected entry was item %u at price %u quantity %u -- the "
					"purchase assertions below would be vacuous",
					g_eShopBoughtItem, g_uShopBoughtPrice, g_uShopBoughtQuantity);
				bPassed = false;
			}
			else
			{
				if (g_eShopResultAfterBuy != (u_int)ZM_SHOP_OK || !g_bShopReportedResult)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_ShopScreen] the purchase reported result %u (reported=%s), expected OK %u",
						g_eShopResultAfterBuy, g_bShopReportedResult ? "true" : "false",
						(u_int)ZM_SHOP_OK);
					bPassed = false;
				}
				const u_int uWantMoney = g_uShopMoneyBeforeBuy - g_uShopBoughtPrice * g_uShopBoughtQuantity;
				if (g_uShopMoneyBeforeBuy < g_uShopBoughtPrice * g_uShopBoughtQuantity
					|| g_uShopMoneyAfterBuy != uWantMoney)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_ShopScreen] the LIVE purse went %u -> %u, expected %u (price %u x qty %u) -- "
						"the confirm did not debit the real ZM_GameState",
						g_uShopMoneyBeforeBuy, g_uShopMoneyAfterBuy, uWantMoney,
						g_uShopBoughtPrice, g_uShopBoughtQuantity);
					bPassed = false;
				}
				if (g_uShopHeldAfterBuy != g_uShopHeldBeforeBuy + g_uShopBoughtQuantity)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_ShopScreen] the LIVE bag held %u -> %u of '%s', expected +%u -- the confirm "
						"did not deliver the goods into the real ZM_GameState",
						g_uShopHeldBeforeBuy, g_uShopHeldAfterBuy,
						ZM_GetItemName((ZM_ITEM_ID)g_eShopBoughtItem), g_uShopBoughtQuantity);
					bPassed = false;
				}
			}

			// --- Escape: the shop closes, its widgets go down, the player moves again ---
			if (!g_bShopClosed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the shop never closed after the Escape press");
				bPassed = false;
			}
			// The *Found flags prove the capture actually ran (a default-initialised view would
			// pass the "hidden" checks vacuously).
			if (!g_xShopElementsOnClose.m_bPanelFound
				|| g_xShopElementsOnClose.m_uRowsFound != ZM_UI_Shop::uROWS_PER_PAGE
				|| g_xShopElementsOnClose.m_bPanelVisible
				|| g_xShopElementsOnClose.m_bHeaderVisible
				|| g_xShopElementsOnClose.m_uRowsVisible != 0u
				|| g_xShopElementsOnClose.m_uControlsVisible != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the shop widgets were not all hidden once it closed (panel found=%s "
					"visible=%s, rows found=%u visible=%u, header visible=%s, controls visible=%u)",
					g_xShopElementsOnClose.m_bPanelFound ? "true" : "false",
					g_xShopElementsOnClose.m_bPanelVisible ? "true" : "false",
					g_xShopElementsOnClose.m_uRowsFound,
					g_xShopElementsOnClose.m_uRowsVisible,
					g_xShopElementsOnClose.m_bHeaderVisible ? "true" : "false",
					g_xShopElementsOnClose.m_uControlsVisible);
				bPassed = false;
			}
			if (!g_bShopMovementReenabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] player movement was not re-enabled after the shop closed");
				bPassed = false;
			}
			if (!g_bShopFocusClearedOnClose)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShopScreen] the canvas focus was not cleared (nullptr) after the shop closed -- "
					"arrow keys could still drive the hidden screen");
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), exactly like the sibling tests. The
		// purchase mutated the persistent GameState; the harness's between-tests hook
		// re-seeds the starter, so nothing leaks into the next test.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_bShopActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bShopActive = false;

		return bPassed || !g_bShopPrereqsPresent;
	}

	// =========================================================================
	// ZM_CareCenterHeal_Test (S6 item 2 SC8) -- the yes/no PROMPT, end to end, over a
	// runtime-ready Dawnmere:
	//   damage the LIVE party through ZM_GameState -> TryOpenCareCenterPrompt() (the
	//   seam S6 item 3's attendant will call) -> the DIALOGUE screen is up and the
	//   player is FROZEN -> spaced Enter edges read past the prompt line -> the box is
	//   AWAITING an answer, the two AUTHORED buttons are shown, focusable and labelled
	//   from ZM_CareCenter -> REAL arrow edges walk the focus Yes -> No -> Yes (both
	//   answers must be key-reachable; parking the focus programmatically would prove
	//   nothing) -> one Enter on Yes -> the LIVE party is at FULL HP and the menu closed
	//   with movement re-enabled. Then the NO branch: re-damage, prompt again, walk to
	//   No, confirm, and assert the party was NOT healed.
	//
	// The heal assertions are anchored on the LIVE ZM_GameState the game itself renders
	// (never a local fixture): the point of the gate is that the answer moves the real
	// party. The harness's between-tests hook re-seeds the starter afterwards.
	// =========================================================================

	// What the AUTHORED choice widgets actually report this frame. The model-only
	// assertions cannot see a typo'd element name, a missing AddStep_CreateUIButton or a
	// ZM_ConfigureMenuRoot FindElement that silently returned nullptr -- ZM_UI_DialogueBox
	// null-guards all three, so nothing would render while every model assertion stayed
	// green.
	struct ChoiceElementView
	{
		bool        m_bResolved     = false;   // the ZM_MenuRoot UI component resolved
		bool        m_bYesFound     = false;
		bool        m_bNoFound      = false;
		bool        m_bYesVisible   = false;
		bool        m_bNoVisible    = false;
		// Focusability is HALF the contract: the engine nav collects visible + FOCUSABLE
		// elements, so a non-focusable button is an unanswerable question.
		bool        m_bYesFocusable = false;
		bool        m_bNoFocusable  = false;
		std::string m_strYesText;
		std::string m_strNoText;
		std::string m_strFocusedName;
	};

	ChoiceElementView ReadChoiceElements()
	{
		ChoiceElementView xView;
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;

		if (Zenith_UI::Zenith_UIButton* pxYes =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_DialogueBox::szYES_NAME))
		{
			xView.m_bYesFound = true;
			xView.m_bYesVisible = pxYes->IsVisible();
			xView.m_bYesFocusable = pxYes->IsFocusable();
			xView.m_strYesText = pxYes->GetText();
		}
		if (Zenith_UI::Zenith_UIButton* pxNo =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_DialogueBox::szNO_NAME))
		{
			xView.m_bNoFound = true;
			xView.m_bNoVisible = pxNo->IsVisible();
			xView.m_bNoFocusable = pxNo->IsFocusable();
			xView.m_strNoText = pxNo->GetText();
		}
		if (Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement())
		{
			xView.m_strFocusedName = pxFocused->GetName();
		}
		return xView;
	}

	// The LIVE party's health, read fresh (never cached -- the pool relocates).
	struct CarePartyView
	{
		bool  m_bResolved       = false;
		u_int m_uCount          = 0u;
		u_int m_uFullHpMembers  = 0u;
		u_int m_uLeadCurrentHp  = 0u;
		u_int m_uLeadMaxHp      = 0u;
	};

	CarePartyView ReadLiveParty()
	{
		CarePartyView xView;
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return xView;
		}
		xView.m_bResolved = true;
		xView.m_uCount = pxState->m_xParty.Count();
		for (u_int u = 0u; u < xView.m_uCount; ++u)
		{
			const ZM_Monster& xMember = pxState->m_xParty.Get(u);
			if (xMember.m_uCurrentHp >= xMember.GetMaxHP())
			{
				++xView.m_uFullHpMembers;
			}
		}
		if (xView.m_uCount > 0u)
		{
			const ZM_Monster& xLead = pxState->m_xParty.Get(0u);
			xView.m_uLeadCurrentHp = xLead.m_uCurrentHp;
			xView.m_uLeadMaxHp = xLead.GetMaxHP();
		}
		return xView;
	}

	// Drop every member of the LIVE party to 1 HP. A state-setter on the real game state
	// (no battle needed to produce a damaged party); false when nothing could be damaged,
	// which would make every heal assertion vacuous.
	bool DamageLiveParty(u_int& uDamagedOut)
	{
		uDamagedOut = 0u;
		ZM_GameState* pxState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
		{
			return false;
		}
		for (u_int u = 0u; u < pxState->m_xParty.Count(); ++u)
		{
			ZM_Monster& xMember = pxState->m_xParty.Get(u);
			if (xMember.GetMaxHP() <= 1u)
			{
				continue;   // nothing to take off it
			}
			xMember.m_uCurrentHp = 1u;
			++uDamagedOut;
		}
		return uDamagedOut > 0u;
	}

	enum class CarePhase
	{
		AwaitReady,
		DamageParty,     // state-set the LIVE party to 1 HP
		OpenPrompt,      // TryOpenCareCenterPrompt + capture the raise
		ReadPrompt,      // spaced Enter edges until the box awaits the answer
		SampleChoice,    // ...and what the authored buttons are showing
		WalkToNo,        // REAL arrow edges: the focus must reach the No button
		WalkBackToYes,   // ...and come back, so BOTH answers are proved reachable
		ConfirmYes,      // ONE Enter on Yes -> the heal runs and the menu closes
		DamageAgain,     // re-damage for the NO branch
		OpenPromptNo,    // raise the prompt a second time
		ReadPromptNo,    // ...read past its line
		WalkToNoAgain,   // ...park the focus on No with real arrow edges
		ConfirmNo,       // ONE Enter on No -> nothing is healed, the menu closes
		Done,
	};

	constexpr int iCC_READY_DEADLINE = 420;   // Dawnmere first-load ready window (sibling parity)
	constexpr int iCC_PRESS_FRAME    = 2;     // the frame within a press phase that emits the edge
	constexpr int iCC_SETTLE_FRAME   = 6;     // ...and the frame the outcome is sampled on
	constexpr int iCC_READ_DEADLINE  = 120;   // frames for the Enter edges to read past the line
	constexpr int iCC_WALK_DEADLINE  = 160;   // frames for the arrows to reach the other button

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	CarePhase g_eCarePhase          = CarePhase::Done;
	int       g_iCarePhaseFrames    = 0;
	bool      g_bCarePrereqsPresent = false;
	bool      g_bCareActive         = false;
	bool      g_bCareFailed         = false;
	const char* g_szCareFailure     = "test did not reach verification";

	// ---- Captured observations: the YES branch ----
	bool  g_bCareMovementEnabledBefore = false;   // player movable BEFORE the prompt (baseline)
	u_int g_uCareDamagedMembers        = 0u;      // members knocked below max HP (want > 0)
	CarePartyView g_xCarePartyDamaged;            // ...and what that left behind
	bool  g_bCareOpenAccepted          = false;   // TryOpenCareCenterPrompt returned true
	bool  g_bCareMenuOpened            = false;
	u_int g_eCareTopOnOpen             = (u_int)ZM_MENU_SCREEN_NONE;   // want DIALOGUE
	u_int g_uCareDepthOnOpen           = 0u;      // want 1 (no pause menu involved)
	bool  g_bCarePlayerFrozen          = false;
	u_int g_eCareActionOnOpen          = (u_int)ZM_DIALOGUE_ACTION_NONE;   // want HEAL_PARTY
	bool  g_bCareAwaitingChoice        = false;   // the box reached the question
	int   g_iCareReadPresses           = 0;       // Enter edges it took (want > 0)
	ChoiceElementView g_xCareChoiceOnAwait;       // want: both found, visible, focusable, labelled
	ChoiceElementView g_xCareChoiceOnClose;       // want: both hidden + non-focusable

	// ---- The arrow walk (defaults to FAILING -- a parked focus proves nothing) ----
	bool        g_bCareReachedNo      = false;
	bool        g_bCareReturnedToYes  = false;
	std::string g_strCareWalkFocusName;

	// ---- The YES answer ----
	u_int g_eCareChoiceAfterYes    = (u_int)ZM_DIALOGUE_CHOICE_NONE;   // want YES
	u_int g_eCareActionAfterYes    = (u_int)ZM_DIALOGUE_ACTION_HEAL_PARTY;   // want NONE (consumed)
	CarePartyView g_xCarePartyAfterYes;
	bool  g_bCareClosedAfterYes    = false;
	bool  g_bCareMovementReenabled = false;
	bool  g_bCareFocusClearedOnClose = false;

	// ---- The NO branch ----
	u_int g_uCareDamagedMembersNo   = 0u;
	CarePartyView g_xCarePartyDamagedNo;
	bool  g_bCareOpenAcceptedNo     = false;
	bool  g_bCareAwaitingChoiceNo   = false;
	bool  g_bCareFocusedNoOnConfirm = false;
	u_int g_eCareChoiceAfterNo      = (u_int)ZM_DIALOGUE_CHOICE_NONE;   // want NO
	CarePartyView g_xCarePartyAfterNo;
	bool  g_bCareClosedAfterNo      = false;

	void FailCare(const char* szReason)
	{
		g_szCareFailure = szReason;
		g_bCareFailed = true;
		g_eCarePhase = CarePhase::Done;
	}

	// The canvas focus's element name this frame ("" when nothing holds it).
	std::string ReadMenuFocusName()
	{
		Zenith_UIComponent* pxUI = ResolveMenuRootUI();
		if (pxUI == nullptr)
		{
			return std::string();
		}
		Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
		return (pxFocused != nullptr) ? pxFocused->GetName() : std::string();
	}

	void Setup_ZMCareCenterHeal()
	{
		g_eCarePhase                 = CarePhase::Done;
		g_iCarePhaseFrames           = 0;
		g_bCareActive                = false;
		g_bCareFailed                = false;
		g_szCareFailure              = "test did not reach verification";

		g_bCareMovementEnabledBefore = false;
		g_uCareDamagedMembers        = 0u;
		g_xCarePartyDamaged          = CarePartyView{};
		g_bCareOpenAccepted          = false;
		g_bCareMenuOpened            = false;
		g_eCareTopOnOpen             = (u_int)ZM_MENU_SCREEN_NONE;
		g_uCareDepthOnOpen           = 0u;
		g_bCarePlayerFrozen          = false;
		g_eCareActionOnOpen          = (u_int)ZM_DIALOGUE_ACTION_NONE;
		g_bCareAwaitingChoice        = false;
		g_iCareReadPresses           = 0;
		g_xCareChoiceOnAwait         = ChoiceElementView{};
		g_xCareChoiceOnClose         = ChoiceElementView{};

		g_bCareReachedNo             = false;
		g_bCareReturnedToYes         = false;
		g_strCareWalkFocusName.clear();

		g_eCareChoiceAfterYes        = (u_int)ZM_DIALOGUE_CHOICE_NONE;
		g_eCareActionAfterYes        = (u_int)ZM_DIALOGUE_ACTION_HEAL_PARTY;
		g_xCarePartyAfterYes         = CarePartyView{};
		g_bCareClosedAfterYes        = false;
		g_bCareMovementReenabled     = false;
		g_bCareFocusClearedOnClose   = false;

		g_uCareDamagedMembersNo      = 0u;
		g_xCarePartyDamagedNo        = CarePartyView{};
		g_bCareOpenAcceptedNo        = false;
		g_bCareAwaitingChoiceNo      = false;
		g_bCareFocusedNoOnConfirm    = false;
		g_eCareChoiceAfterNo         = (u_int)ZM_DIALOGUE_CHOICE_NONE;
		g_xCarePartyAfterNo          = CarePartyView{};
		g_bCareClosedAfterNo         = false;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO process
		// state (fixed dt, scene load) until every git-ignored input is confirmed
		// present. CI has no baked Assets tree -> skip.
		g_bCarePrereqsPresent = RequiredDawnmereAssetsPresent();
		if (!g_bCarePrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere assets absent -- run a *_True build");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fUI_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eCarePhase = CarePhase::AwaitReady;
		g_bCareActive = true;
	}

	bool Step_ZMCareCenterHeal(int)
	{
		if (!g_bCareActive || g_bCareFailed || g_eCarePhase == CarePhase::Done)
		{
			return false;
		}

		++g_iCarePhaseFrames;
		switch (g_eCarePhase)
		{
		case CarePhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iCarePhaseFrames > iCC_READY_DEADLINE)
				{
					FailCare("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}
			if (ResolveSingletonMenuStack() == nullptr)
			{
				FailCare("no unique ZM_UI_MenuStack singleton (ZM_MenuRoot missing)");
				return false;
			}

			g_bCareMovementEnabledBefore = xPlayer.m_pxController->IsMovementEnabled();

			g_eCarePhase = CarePhase::DamageParty;
			g_iCarePhaseFrames = 0;
			return true;
		}

		case CarePhase::DamageParty:
		{
			if (!DamageLiveParty(g_uCareDamagedMembers))
			{
				FailCare("the LIVE party could not be damaged -- the heal gate would be vacuous");
				return false;
			}
			g_xCarePartyDamaged = ReadLiveParty();

			g_eCarePhase = CarePhase::OpenPrompt;
			g_iCarePhaseFrames = 0;
			return true;
		}

		case CarePhase::OpenPrompt:
		{
			// The attendant seam: a static singleton call, exactly what S6 item 3's Care
			// Center NPC will make. Raised ONCE, over an EMPTY stack -- a Care Center must be
			// reachable without the pause menu.
			if (g_iCarePhaseFrames == 1)
			{
				g_bCareOpenAccepted = ZM_UI_MenuStack::TryOpenCareCenterPrompt();
				if (!g_bCareOpenAccepted)
				{
					FailCare("TryOpenCareCenterPrompt refused to raise the prompt");
					return false;
				}
				return true;
			}
			if (g_iCarePhaseFrames < iCC_SETTLE_FRAME)
			{
				return true;   // let a Present run before sampling
			}

			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailCare("the ZM_UI_MenuStack singleton stopped resolving after the raise");
				return false;
			}
			g_bCareMenuOpened   = pxMenu->IsOpen();
			g_eCareTopOnOpen    = (u_int)pxMenu->GetTopScreen();
			g_uCareDepthOnOpen  = pxMenu->GetDepth();
			g_eCareActionOnOpen = (u_int)pxMenu->GetPendingDialogueAction();

			bool bEnabled = true;
			if (ReadActivePlayerMovementEnabled(bEnabled))
			{
				g_bCarePlayerFrozen = !bEnabled;
			}
			if (!g_bCareMenuOpened || g_eCareTopOnOpen != (u_int)ZM_MENU_SCREEN_DIALOGUE)
			{
				// The raise is SYNCHRONOUS (it queues, arms, pushes and presents inside the
				// call), so if the screen is not up by the settle frame it never will be.
				FailCare("the DIALOGUE screen never came up after TryOpenCareCenterPrompt");
				return false;
			}

			g_eCarePhase = CarePhase::ReadPrompt;
			g_iCarePhaseFrames = 0;
			return true;
		}

		case CarePhase::ReadPrompt:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailCare("the ZM_UI_MenuStack singleton stopped resolving while reading the prompt");
				return false;
			}
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				g_bCareAwaitingChoice = true;
				g_eCarePhase = CarePhase::SampleChoice;
				g_iCarePhaseFrames = 0;
				return true;
			}
			if (!pxMenu->IsOpen())
			{
				// The regression this whole SC exists to prevent: with a choice armed the last
				// line must NOT close the box.
				FailCare("the prompt CLOSED instead of awaiting an answer");
				return false;
			}
			if (g_iCarePhaseFrames > iCC_READ_DEADLINE)
			{
				FailCare("the Enter edges never read the prompt through to its question");
				return false;
			}
			// Spaced edges (one every fourth frame) so each press is a clean edge: the first
			// completes the typewriter, the second consumes the line and opens the question.
			if ((g_iCarePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				++g_iCareReadPresses;
			}
			return true;
		}

		case CarePhase::SampleChoice:
		{
			g_xCareChoiceOnAwait = ReadChoiceElements();

			g_eCarePhase = CarePhase::WalkToNo;
			g_iCarePhaseFrames = 0;
			return true;
		}

		case CarePhase::WalkToNo:
		{
			// REAL arrow edges. The two buttons carry NO explicit navigation links (a
			// bake-time link into an element Present hides would swallow the press), so this
			// walks purely on the ENGINE SPATIAL search -- and if it cannot cross between
			// them, half the answers are unreachable by keyboard and the prompt ships broken.
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szNO_NAME)
			{
				g_bCareReachedNo = true;
				g_strCareWalkFocusName = strFocus;
				g_eCarePhase = CarePhase::WalkBackToYes;
				g_iCarePhaseFrames = 0;
				return true;
			}
			if (g_iCarePhaseFrames > iCC_WALK_DEADLINE)
			{
				g_strCareWalkFocusName = strFocus;
				FailCare("the Right edges never walked the focus onto the No button");
				return false;
			}
			if ((g_iCarePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_RIGHT);
			}
			return true;
		}

		case CarePhase::WalkBackToYes:
		{
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szYES_NAME)
			{
				g_bCareReturnedToYes = true;
				g_strCareWalkFocusName = strFocus;
				g_eCarePhase = CarePhase::ConfirmYes;
				g_iCarePhaseFrames = 0;
				return true;
			}
			if (g_iCarePhaseFrames > iCC_WALK_DEADLINE)
			{
				g_strCareWalkFocusName = strFocus;
				FailCare("the Left edges never walked the focus back onto the Yes button");
				return false;
			}
			if ((g_iCarePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_LEFT);
			}
			return true;
		}

		case CarePhase::ConfirmYes:
		{
			// ONE Enter edge with idle frames on either side, so the edge-detected
			// ZM_InputActions::ReadConfirmPressed fires exactly once. The focus is on YES,
			// proved by the walk above rather than assumed.
			if (g_iCarePhaseFrames == iCC_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iCarePhaseFrames < iCC_SETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailCare("the ZM_UI_MenuStack singleton stopped resolving after the YES answer");
				return false;
			}
			g_eCareChoiceAfterYes = (u_int)pxMenu->GetLastDialogueAnswer();   // the HOST latch:
			// the box's own answer is wiped by the CloseMenu the resolve triggers.
			g_eCareActionAfterYes = (u_int)pxMenu->GetPendingDialogueAction();
			g_bCareClosedAfterYes = !pxMenu->IsOpen();
			g_xCarePartyAfterYes  = ReadLiveParty();
			// CloseMenu hides the widgets directly (the per-frame Present stops the moment
			// the stack empties), so they must be down here.
			g_xCareChoiceOnClose  = ReadChoiceElements();
			g_bCareFocusClearedOnClose = MenuFocusCleared();

			bool bEnabled = false;
			if (ReadActivePlayerMovementEnabled(bEnabled))
			{
				g_bCareMovementReenabled = bEnabled;
			}
			if (!g_bCareClosedAfterYes)
			{
				FailCare("the prompt never closed after the YES answer");
				return false;
			}

			g_eCarePhase = CarePhase::DamageAgain;
			g_iCarePhaseFrames = 0;
			return true;
		}

		case CarePhase::DamageAgain:
		{
			if (!DamageLiveParty(g_uCareDamagedMembersNo))
			{
				FailCare("the LIVE party could not be re-damaged for the NO branch");
				return false;
			}
			g_xCarePartyDamagedNo = ReadLiveParty();

			g_eCarePhase = CarePhase::OpenPromptNo;
			g_iCarePhaseFrames = 0;
			return true;
		}

		case CarePhase::OpenPromptNo:
		{
			if (g_iCarePhaseFrames == 1)
			{
				g_bCareOpenAcceptedNo = ZM_UI_MenuStack::TryOpenCareCenterPrompt();
				if (!g_bCareOpenAcceptedNo)
				{
					// The answered box must have reset itself completely -- a stale armed choice
					// would refuse the second prompt outright.
					FailCare("the SECOND TryOpenCareCenterPrompt was refused");
					return false;
				}
				return true;
			}
			if (g_iCarePhaseFrames < iCC_SETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr || pxMenu->GetTopScreen() != ZM_MENU_SCREEN_DIALOGUE)
			{
				FailCare("the DIALOGUE screen never came up for the NO branch");
				return false;
			}

			g_eCarePhase = CarePhase::ReadPromptNo;
			g_iCarePhaseFrames = 0;
			return true;
		}

		case CarePhase::ReadPromptNo:
		{
			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailCare("the ZM_UI_MenuStack singleton stopped resolving on the NO branch");
				return false;
			}
			if (pxMenu->IsDialogueAwaitingChoice())
			{
				g_bCareAwaitingChoiceNo = true;
				g_eCarePhase = CarePhase::WalkToNoAgain;
				g_iCarePhaseFrames = 0;
				return true;
			}
			if (g_iCarePhaseFrames > iCC_READ_DEADLINE)
			{
				FailCare("the NO branch never read its prompt through to the question");
				return false;
			}
			if ((g_iCarePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
			}
			return true;
		}

		case CarePhase::WalkToNoAgain:
		{
			const std::string strFocus = ReadMenuFocusName();
			if (strFocus == ZM_UI_DialogueBox::szNO_NAME)
			{
				g_bCareFocusedNoOnConfirm = true;
				g_eCarePhase = CarePhase::ConfirmNo;
				g_iCarePhaseFrames = 0;
				return true;
			}
			if (g_iCarePhaseFrames > iCC_WALK_DEADLINE)
			{
				FailCare("the focus never reached the No button on the NO branch");
				return false;
			}
			if ((g_iCarePhaseFrames % 4) == 1)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_RIGHT);
			}
			return true;
		}

		case CarePhase::ConfirmNo:
		{
			if (g_iCarePhaseFrames == iCC_PRESS_FRAME)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
				return true;
			}
			if (g_iCarePhaseFrames < iCC_SETTLE_FRAME)
			{
				return true;
			}

			ZM_UI_MenuStack* pxMenu = ResolveSingletonMenuStack();
			if (pxMenu == nullptr)
			{
				FailCare("the ZM_UI_MenuStack singleton stopped resolving after the NO answer");
				return false;
			}
			g_eCareChoiceAfterNo = (u_int)pxMenu->GetLastDialogueAnswer();    // ditto (see the YES branch)
			g_bCareClosedAfterNo = !pxMenu->IsOpen();
			g_xCarePartyAfterNo  = ReadLiveParty();

			g_eCarePhase = CarePhase::Done;
			return false;
		}

		case CarePhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMCareCenterHeal()
	{
		bool bPassed = true;

		if (g_bCareActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_CareCenterHeal] captured: failed=%s (%s) movBefore=%s damaged=%u "
				"leadHp %u/%u fullMembers=%u/%u openAccepted=%s opened=%s top=%u (want DIALOGUE=%u) "
				"depth=%u (want 1) frozen=%s action=%u (want HEAL=%u) awaiting=%s readPresses=%d",
				g_bCareFailed ? "true" : "false", g_szCareFailure,
				g_bCareMovementEnabledBefore ? "true" : "false",
				g_uCareDamagedMembers,
				g_xCarePartyDamaged.m_uLeadCurrentHp, g_xCarePartyDamaged.m_uLeadMaxHp,
				g_xCarePartyDamaged.m_uFullHpMembers, g_xCarePartyDamaged.m_uCount,
				g_bCareOpenAccepted ? "true" : "false",
				g_bCareMenuOpened ? "true" : "false",
				g_eCareTopOnOpen, (u_int)ZM_MENU_SCREEN_DIALOGUE,
				g_uCareDepthOnOpen,
				g_bCarePlayerFrozen ? "true" : "false",
				g_eCareActionOnOpen, (u_int)ZM_DIALOGUE_ACTION_HEAL_PARTY,
				g_bCareAwaitingChoice ? "true" : "false",
				g_iCareReadPresses);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_CareCenterHeal] widgets: onAwait(resolved=%s yes=%s/%s/%s '%s' no=%s/%s/%s '%s' "
				"focus='%s') walk(reachedNo=%s backToYes=%s last='%s') "
				"onClose(yesFound=%s yesVisible=%s yesFocusable=%s noVisible=%s noFocusable=%s)",
				g_xCareChoiceOnAwait.m_bResolved ? "true" : "false",
				g_xCareChoiceOnAwait.m_bYesFound ? "found" : "MISSING",
				g_xCareChoiceOnAwait.m_bYesVisible ? "visible" : "hidden",
				g_xCareChoiceOnAwait.m_bYesFocusable ? "focusable" : "INERT",
				g_xCareChoiceOnAwait.m_strYesText.c_str(),
				g_xCareChoiceOnAwait.m_bNoFound ? "found" : "MISSING",
				g_xCareChoiceOnAwait.m_bNoVisible ? "visible" : "hidden",
				g_xCareChoiceOnAwait.m_bNoFocusable ? "focusable" : "INERT",
				g_xCareChoiceOnAwait.m_strNoText.c_str(),
				g_xCareChoiceOnAwait.m_strFocusedName.c_str(),
				g_bCareReachedNo ? "true" : "false",
				g_bCareReturnedToYes ? "true" : "false",
				g_strCareWalkFocusName.c_str(),
				g_xCareChoiceOnClose.m_bYesFound ? "true" : "false",
				g_xCareChoiceOnClose.m_bYesVisible ? "true" : "false",
				g_xCareChoiceOnClose.m_bYesFocusable ? "true" : "false",
				g_xCareChoiceOnClose.m_bNoVisible ? "true" : "false",
				g_xCareChoiceOnClose.m_bNoFocusable ? "true" : "false");

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_CareCenterHeal] answers: YES(choice=%u action=%u closed=%s movReenabled=%s "
				"focusCleared=%s leadHp %u/%u full=%u/%u) NO(damaged=%u leadHp %u/%u accepted=%s "
				"awaiting=%s focusedNo=%s choice=%u closed=%s leadHp %u/%u full=%u/%u)",
				g_eCareChoiceAfterYes, g_eCareActionAfterYes,
				g_bCareClosedAfterYes ? "true" : "false",
				g_bCareMovementReenabled ? "true" : "false",
				g_bCareFocusClearedOnClose ? "true" : "false",
				g_xCarePartyAfterYes.m_uLeadCurrentHp, g_xCarePartyAfterYes.m_uLeadMaxHp,
				g_xCarePartyAfterYes.m_uFullHpMembers, g_xCarePartyAfterYes.m_uCount,
				g_uCareDamagedMembersNo,
				g_xCarePartyDamagedNo.m_uLeadCurrentHp, g_xCarePartyDamagedNo.m_uLeadMaxHp,
				g_bCareOpenAcceptedNo ? "true" : "false",
				g_bCareAwaitingChoiceNo ? "true" : "false",
				g_bCareFocusedNoOnConfirm ? "true" : "false",
				g_eCareChoiceAfterNo,
				g_bCareClosedAfterNo ? "true" : "false",
				g_xCarePartyAfterNo.m_uLeadCurrentHp, g_xCarePartyAfterNo.m_uLeadMaxHp,
				g_xCarePartyAfterNo.m_uFullHpMembers, g_xCarePartyAfterNo.m_uCount);

			if (g_bCareFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_CareCenterHeal] %s", g_szCareFailure);
				bPassed = false;
			}

			// The baselines must be meaningful: the player was movable, and the party really
			// was damaged before the prompt went up.
			if (!g_bCareMovementEnabledBefore)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the player was not movable before the prompt -- the freeze "
					"assertions would be vacuous");
				bPassed = false;
			}
			if (g_uCareDamagedMembers == 0u
				|| !g_xCarePartyDamaged.m_bResolved
				|| g_xCarePartyDamaged.m_uCount == 0u
				|| g_xCarePartyDamaged.m_uFullHpMembers != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the LIVE party was not damaged before the prompt (damaged=%u "
					"resolved=%s count=%u atFullHp=%u) -- the heal assertion would pass on a party "
					"that never needed healing",
					g_uCareDamagedMembers, g_xCarePartyDamaged.m_bResolved ? "true" : "false",
					g_xCarePartyDamaged.m_uCount, g_xCarePartyDamaged.m_uFullHpMembers);
				bPassed = false;
			}

			// --- the raise: DIALOGUE on an otherwise EMPTY stack, frozen, HEAL_PARTY armed ---
			if (!g_bCareOpenAccepted || !g_bCareMenuOpened
				|| g_eCareTopOnOpen != (u_int)ZM_MENU_SCREEN_DIALOGUE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] TryOpenCareCenterPrompt accepted=%s opened=%s top=%u (want "
					"DIALOGUE %u) -- the seam S6 item 3 hangs its attendant off did not raise the prompt",
					g_bCareOpenAccepted ? "true" : "false", g_bCareMenuOpened ? "true" : "false",
					g_eCareTopOnOpen, (u_int)ZM_MENU_SCREEN_DIALOGUE);
				bPassed = false;
			}
			if (g_uCareDepthOnOpen != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] stack depth after the raise was %u, expected 1 -- a Care Center "
					"must be reachable WITHOUT the pause menu", g_uCareDepthOnOpen);
				bPassed = false;
			}
			if (!g_bCarePlayerFrozen)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the player was NOT frozen while the prompt was up");
				bPassed = false;
			}
			if (g_eCareActionOnOpen != (u_int)ZM_DIALOGUE_ACTION_HEAL_PARTY)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the pending dialogue action was %u, expected HEAL_PARTY %u",
					g_eCareActionOnOpen, (u_int)ZM_DIALOGUE_ACTION_HEAL_PARTY);
				bPassed = false;
			}

			// --- the box HELD on its last line instead of closing ---
			if (!g_bCareAwaitingChoice || g_iCareReadPresses <= 0)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the box never reached its question (awaiting=%s after %d Enter "
					"edges) -- with a choice armed the last line must HOLD, not close",
					g_bCareAwaitingChoice ? "true" : "false", g_iCareReadPresses);
				bPassed = false;
			}

			// --- the AUTHORED buttons really render, labelled from ZM_CareCenter ---
			if (!g_xCareChoiceOnAwait.m_bResolved
				|| !g_xCareChoiceOnAwait.m_bYesFound
				|| !g_xCareChoiceOnAwait.m_bNoFound)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the AUTHORED choice buttons did not resolve by name "
					"(resolved=%s yes=%s no=%s) -- ZM_ConfigureMenuRoot / the AddStep_CreateUIButton "
					"contract is broken and NOTHING would render",
					g_xCareChoiceOnAwait.m_bResolved ? "true" : "false",
					g_xCareChoiceOnAwait.m_bYesFound ? "true" : "false",
					g_xCareChoiceOnAwait.m_bNoFound ? "true" : "false");
				bPassed = false;
			}
			else
			{
				if (!g_xCareChoiceOnAwait.m_bYesVisible || !g_xCareChoiceOnAwait.m_bYesFocusable
					|| !g_xCareChoiceOnAwait.m_bNoVisible || !g_xCareChoiceOnAwait.m_bNoFocusable)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_CareCenterHeal] the choice buttons were not both shown AND focusable "
						"(yes visible=%s focusable=%s, no visible=%s focusable=%s) -- a non-focusable "
						"button is an unanswerable question",
						g_xCareChoiceOnAwait.m_bYesVisible ? "true" : "false",
						g_xCareChoiceOnAwait.m_bYesFocusable ? "true" : "false",
						g_xCareChoiceOnAwait.m_bNoVisible ? "true" : "false",
						g_xCareChoiceOnAwait.m_bNoFocusable ? "true" : "false");
					bPassed = false;
				}
				if (g_xCareChoiceOnAwait.m_strYesText != ZM_CareCenterYesLabel()
					|| g_xCareChoiceOnAwait.m_strNoText != ZM_CareCenterNoLabel())
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_CareCenterHeal] the buttons read '%s' / '%s', expected the armed labels "
						"'%s' / '%s' -- Present is not writing what ArmChoice was given",
						g_xCareChoiceOnAwait.m_strYesText.c_str(),
						g_xCareChoiceOnAwait.m_strNoText.c_str(),
						ZM_CareCenterYesLabel(), ZM_CareCenterNoLabel());
					bPassed = false;
				}
				// The DIALOGUE screen normally CLEARS the canvas focus; while a prompt waits it
				// must own it, or the arrow keys drive nothing at all.
				if (g_xCareChoiceOnAwait.m_strFocusedName != ZM_UI_DialogueBox::szYES_NAME)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_CareCenterHeal] the canvas focus was '%s' when the question came up, "
						"expected the default '%s'",
						g_xCareChoiceOnAwait.m_strFocusedName.c_str(), ZM_UI_DialogueBox::szYES_NAME);
					bPassed = false;
				}
			}

			// --- BOTH answers are key-reachable ---
			if (!g_bCareReachedNo || !g_bCareReturnedToYes)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the arrow edges did not walk Yes -> No -> Yes (reachedNo=%s "
					"backToYes=%s, focus stalled on '%s') -- one of the two answers cannot be picked "
					"by keyboard",
					g_bCareReachedNo ? "true" : "false", g_bCareReturnedToYes ? "true" : "false",
					g_strCareWalkFocusName.c_str());
				bPassed = false;
			}

			// --- YES: the LIVE party is healed, the action is consumed, the menu closed ---
			if (g_eCareChoiceAfterYes != (u_int)ZM_DIALOGUE_CHOICE_YES)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the stored answer was %u, expected YES %u",
					g_eCareChoiceAfterYes, (u_int)ZM_DIALOGUE_CHOICE_YES);
				bPassed = false;
			}
			if (g_eCareActionAfterYes != (u_int)ZM_DIALOGUE_ACTION_NONE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the pending action was %u after the answer, expected NONE %u -- "
					"a stale HEAL_PARTY would fire on the next unrelated conversation",
					g_eCareActionAfterYes, (u_int)ZM_DIALOGUE_ACTION_NONE);
				bPassed = false;
			}
			if (!g_xCarePartyAfterYes.m_bResolved
				|| g_xCarePartyAfterYes.m_uCount == 0u
				|| g_xCarePartyAfterYes.m_uFullHpMembers != g_xCarePartyAfterYes.m_uCount)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] after YES the LIVE party had %u of %u members at full HP (lead "
					"%u/%u) -- the answer did not heal the real ZM_GameState",
					g_xCarePartyAfterYes.m_uFullHpMembers, g_xCarePartyAfterYes.m_uCount,
					g_xCarePartyAfterYes.m_uLeadCurrentHp, g_xCarePartyAfterYes.m_uLeadMaxHp);
				bPassed = false;
			}
			if (!g_bCareClosedAfterYes || !g_bCareMovementReenabled || !g_bCareFocusClearedOnClose)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] after YES the menu closed=%s, movement re-enabled=%s, focus "
					"cleared=%s -- an answered prompt must leave the player free and the canvas idle",
					g_bCareClosedAfterYes ? "true" : "false",
					g_bCareMovementReenabled ? "true" : "false",
					g_bCareFocusClearedOnClose ? "true" : "false");
				bPassed = false;
			}
			// The *Found flag proves the capture actually ran (a default-initialised view
			// would pass the "hidden" checks vacuously).
			if (!g_xCareChoiceOnClose.m_bYesFound
				|| g_xCareChoiceOnClose.m_bYesVisible || g_xCareChoiceOnClose.m_bYesFocusable
				|| g_xCareChoiceOnClose.m_bNoVisible || g_xCareChoiceOnClose.m_bNoFocusable)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the choice buttons were not hidden AND made inert once the "
					"prompt closed (found=%s yes visible=%s focusable=%s, no visible=%s focusable=%s)",
					g_xCareChoiceOnClose.m_bYesFound ? "true" : "false",
					g_xCareChoiceOnClose.m_bYesVisible ? "true" : "false",
					g_xCareChoiceOnClose.m_bYesFocusable ? "true" : "false",
					g_xCareChoiceOnClose.m_bNoVisible ? "true" : "false",
					g_xCareChoiceOnClose.m_bNoFocusable ? "true" : "false");
				bPassed = false;
			}

			// --- NO: the same prompt answered the other way heals NOTHING ---
			if (!g_bCareOpenAcceptedNo || !g_bCareAwaitingChoiceNo || !g_bCareFocusedNoOnConfirm)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the NO branch did not set up (raised=%s awaiting=%s focusedNo=%s)",
					g_bCareOpenAcceptedNo ? "true" : "false",
					g_bCareAwaitingChoiceNo ? "true" : "false",
					g_bCareFocusedNoOnConfirm ? "true" : "false");
				bPassed = false;
			}
			if (g_uCareDamagedMembersNo == 0u || g_xCarePartyDamagedNo.m_uFullHpMembers != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the party was not re-damaged for the NO branch (damaged=%u "
					"atFullHp=%u) -- the 'not healed' assertion would be vacuous",
					g_uCareDamagedMembersNo, g_xCarePartyDamagedNo.m_uFullHpMembers);
				bPassed = false;
			}
			if (g_eCareChoiceAfterNo != (u_int)ZM_DIALOGUE_CHOICE_NO)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the stored answer was %u on the NO branch, expected NO %u",
					g_eCareChoiceAfterNo, (u_int)ZM_DIALOGUE_CHOICE_NO);
				bPassed = false;
			}
			if (!g_xCarePartyAfterNo.m_bResolved
				|| g_xCarePartyAfterNo.m_uFullHpMembers != 0u
				|| g_xCarePartyAfterNo.m_uLeadCurrentHp != g_xCarePartyDamagedNo.m_uLeadCurrentHp)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] after NO the LIVE party had %u of %u at full HP and the lead "
					"read %u (was %u) -- declining the Care Center must heal NOTHING",
					g_xCarePartyAfterNo.m_uFullHpMembers, g_xCarePartyAfterNo.m_uCount,
					g_xCarePartyAfterNo.m_uLeadCurrentHp, g_xCarePartyDamagedNo.m_uLeadCurrentHp);
				bPassed = false;
			}
			if (!g_bCareClosedAfterNo)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_CareCenterHeal] the prompt never closed after the NO answer");
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), exactly like the sibling tests. This
		// test damaged and healed the persistent GameState; the harness's between-tests
		// hook re-seeds the starter, so nothing leaks into the next test.
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_bCareActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bCareActive = false;

		return bPassed || !g_bCarePrereqsPresent;
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

static const Zenith_AutomatedTest g_xZMBagScreenTest = {
	"ZM_BagScreen_Test",
	&Setup_ZMBagScreen,
	&Step_ZMBagScreen,
	&Verify_ZMBagScreen,
	/* maxFrames */ 1800,   // ready + open + nav + the list walk + three press phases + the close
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBagScreenTest);

static const Zenith_AutomatedTest g_xZMShopScreenTest = {
	"ZM_ShopScreen_Test",
	&Setup_ZMShopScreen,
	&Step_ZMShopScreen,
	&Verify_ZMShopScreen,
	/* maxFrames */ 1600,   // ready window + the raise + the walk to Confirm + the buy + the close
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMShopScreenTest);

static const Zenith_AutomatedTest g_xZMCareCenterHealTest = {
	"ZM_CareCenterHeal_Test",
	&Setup_ZMCareCenterHeal,
	&Step_ZMCareCenterHeal,
	&Verify_ZMCareCenterHeal,
	/* maxFrames */ 2000,   // ready window + TWO full prompts (raise + read + walk + answer)
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMCareCenterHealTest);

#endif // ZENITH_INPUT_SIMULATOR
