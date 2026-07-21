#include "Zenith.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "SaveData/Zenith_SaveData.h"
#include "Zenithmon/Components/ZM_BattleArena.h"
#include "Zenithmon/Components/ZM_BattleDirector.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_GameComponent.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"   // ResetRuntimeStateForTests (between-tests hook)
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"                   // DeleteAllSlotsForTests (between-tests hook)
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"   // sz*_NAME element contract (dialogue authoring)
#include "Zenithmon/Source/UI/ZM_UI_Bag.h"           // sz*_NAME + RowElementName + geometry contract (bag authoring)
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"     // fZM_BATTLE_MENU_ROOT_* row constants (battle menu authoring)
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"           // sz*_NAME + geometry contract (dex authoring)
#include "Zenithmon/Source/UI/ZM_UI_Party.h"         // sz*_NAME + SlotElementName contract (party authoring)
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"          // sz*_NAME + RowElementName + geometry contract (shop authoring)
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

#ifdef ZENITH_TOOLS
#include "Core/Zenith_CommandLine.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <filesystem>
#include <string>
#endif

// Asset-free blockout visual used by the authored S3 interiors and doorway.
// The scene persists this marker while each runtime scene generation rebuilds
// a unit-cube model on its owning entity. Replacing the marker with final art
// later does not affect collision or traversal authoring.
class ZM_GreyboxVisual
{
public:
	ZM_GreyboxVisual() = delete;
	explicit ZM_GreyboxVisual(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	ZM_GreyboxVisual(const ZM_GreyboxVisual&) = delete;
	ZM_GreyboxVisual& operator=(const ZM_GreyboxVisual&) = delete;
	ZM_GreyboxVisual(ZM_GreyboxVisual&&) noexcept = default;
	ZM_GreyboxVisual& operator=(ZM_GreyboxVisual&&) noexcept = default;

	void OnStart()
	{
		if (m_bInitialised || !m_xParentEntity.IsValid())
		{
			return;
		}

		m_xCubeGeometry = Zenith_MeshGeometryAsset::CreateUnitCube();
		m_xMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MeshGeometryAsset* pxGeometryAsset = m_xCubeGeometry.GetDirect();
		Zenith_MaterialAsset* pxMaterial = m_xMaterial.GetDirect();
		if (pxGeometryAsset == nullptr || pxMaterial == nullptr)
		{
			return;
		}

		pxMaterial->SetName("ZM_Greybox");
		pxMaterial->SetBaseColor({ 0.52f, 0.55f, 0.60f, 1.0f });
		pxMaterial->SetRoughness(0.90f);
		pxMaterial->SetMetallic(0.0f);

		Zenith_ModelComponent* pxModel =
			m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
		{
			pxModel = &m_xParentEntity.AddComponent<Zenith_ModelComponent>();
		}
		Flux_MeshGeometry* pxGeometry = pxGeometryAsset->GetGeometry();
		if (pxGeometry == nullptr)
		{
			return;
		}
		pxModel->AddMeshEntry(*pxGeometry, *pxMaterial);
		m_bInitialised = true;
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << 1u;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0u;
		xStream >> uVersion;
		(void)uVersion;
		m_bInitialised = false;
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::TextUnformatted("Replaceable S3 greybox unit cube");
	}
#endif

private:
	Zenith_Entity m_xParentEntity;
	MeshGeometryHandle m_xCubeGeometry;
	MaterialHandle m_xMaterial;
	bool m_bInitialised = false;
};

// ============================================================================
// Zenithmon -- Pokemon-style monster-collecting RPG (see Docs/ for the GDD,
// roadmap, and stage plan).
//
// The game component registers with the component-meta registry via the
// static-init macro (NOT a direct call from Project_RegisterGameComponents: the
// registry is sealed before that hook runs). Dead-strip safe: this TU defines
// the Project_* entry points the engine references, so its static initializers
// always run. Serialization orders: ZM components claim 100+.
// ============================================================================
ZENITH_REGISTER_COMPONENT(ZM_GameComponent, "ZM_Game", 100u)
ZENITH_REGISTER_COMPONENT(ZM_TerrainGrass, "ZM_TerrainGrass", 101u)
ZENITH_REGISTER_COMPONENT(ZM_PlayerController, "ZM_PlayerController", 102u)
ZENITH_REGISTER_COMPONENT(ZM_FollowCamera, "ZM_FollowCamera", 103u)
ZENITH_REGISTER_COMPONENT(ZM_GameStateManager, "ZM_GameStateManager", 104u)
ZENITH_REGISTER_COMPONENT(ZM_SpawnPoint, "ZM_SpawnPoint", 105u)
ZENITH_REGISTER_COMPONENT(ZM_WarpTrigger, "ZM_WarpTrigger", 106u)
ZENITH_REGISTER_COMPONENT(ZM_GreyboxVisual, "ZM_GreyboxVisual", 107u)
ZENITH_REGISTER_COMPONENT(ZM_BattleArena, "ZM_BattleArena", 108u)
ZENITH_REGISTER_COMPONENT(ZM_TallGrassSystem, "ZM_TallGrassSystem", 109u)
ZENITH_REGISTER_COMPONENT(ZM_BattleTransition, "ZM_BattleTransition", 110u)
ZENITH_REGISTER_COMPONENT(ZM_BattleDirector, "ZM_BattleDirector", 111u)
ZENITH_REGISTER_COMPONENT(ZM_UI_MenuStack, "ZM_UI_MenuStack", 112u)
ZENITH_REGISTER_COMPONENT(ZM_Interactable, "ZM_Interactable", 113u)

#ifdef ZENITH_TOOLS
namespace
{
	MaterialHandle g_axDawnmereTerrainMaterials[4];

	void ZM_ConfigureWarpFade()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"WarpFade authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		Zenith_UI::Zenith_UIElement* pxFade = pxUI->FindElement("WarpFade");
		Zenith_Assert(pxFade != nullptr
			&& pxFade->GetType() == Zenith_UI::UIElementType::Overlay,
			"WarpFade must be an Overlay on ZM_GameStateRoot");
		if (pxFade == nullptr
			|| pxFade->GetType() != Zenith_UI::UIElementType::Overlay)
		{
			return;
		}

		Zenith_UI::Zenith_UIOverlay* pxFadeOverlay =
			static_cast<Zenith_UI::Zenith_UIOverlay*>(pxFade);
		pxFadeOverlay->SetContentSize(0.0f, 0.0f);
		pxFadeOverlay->SetAnchorAndPivot(
			Zenith_UI::AnchorPreset::StretchAll);
		pxFadeOverlay->SetSortOrder(10000);
		pxFadeOverlay->SetDimColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		pxFadeOverlay->SetFadeDuration(0.0f);
		pxFadeOverlay->SetGroupAlpha(0.0f);
		pxFadeOverlay->SetVisible(false);
	}

	// ZM_BattleTransition's OWN full-canvas fade, on its OWN persistent root. Sort
	// order 10001 puts it one above WarpFade's 10000, so the two overlays never
	// fight for the top of the canvas (ZM-D-097).
	void ZM_ConfigureBattleFade()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"BattleFade authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		Zenith_UI::Zenith_UIElement* pxFade = pxUI->FindElement("BattleFade");
		Zenith_Assert(pxFade != nullptr
			&& pxFade->GetType() == Zenith_UI::UIElementType::Overlay,
			"BattleFade must be an Overlay on ZM_BattleTransitionRoot");
		if (pxFade == nullptr
			|| pxFade->GetType() != Zenith_UI::UIElementType::Overlay)
		{
			return;
		}

		Zenith_UI::Zenith_UIOverlay* pxFadeOverlay =
			static_cast<Zenith_UI::Zenith_UIOverlay*>(pxFade);
		pxFadeOverlay->SetContentSize(0.0f, 0.0f);
		pxFadeOverlay->SetAnchorAndPivot(
			Zenith_UI::AnchorPreset::StretchAll);
		pxFadeOverlay->SetSortOrder(10001);
		pxFadeOverlay->SetDimColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		pxFadeOverlay->SetFadeDuration(0.0f);
		pxFadeOverlay->SetGroupAlpha(0.0f);
		pxFadeOverlay->SetVisible(false);
	}

	// The director-owned battle HUD (S5 item 4 SC4). Authors the five HUD elements on
	// the selected BattleDirector entity's UI component: sort order 10002 (one above
	// BattleFade's 10001, so the end-fade never clips them), sensible anchor / position
	// / size / font, and hidden (SetVisible(false)) -- ZM_BattleDirector shows them at
	// Setup and hides them at Hide. Element names are the ZM_UI_BattleHUD contract.
	void ZM_ConfigureBattleHUD()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"BattleHUD authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		// Shared placement: every HUD element sits above BattleFade (10001) and is
		// authored hidden. Anchor == pivot keeps the offset intuitive per corner.
		auto fnPlace = [](Zenith_UI::Zenith_UIElement* pxElement,
			Zenith_UI::AnchorPreset ePreset, float fX, float fY, float fW, float fH)
		{
			if (pxElement == nullptr)
			{
				return;
			}
			pxElement->SetSortOrder(10002);
			pxElement->SetAnchor(ePreset);
			pxElement->SetPivot(ePreset);
			pxElement->SetPosition(fX, fY);
			pxElement->SetSize(fW, fH);
			pxElement->SetVisible(false);
		};

		const Zenith_Maths::Vector4 xWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
		const Zenith_Maths::Vector4 xHpGreen = { 0.20f, 0.85f, 0.30f, 1.0f };

		// Battle text log -- bottom-centre, wide, centred, word-wrapped.
		Zenith_UI::Zenith_UIText* pxLog =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>("BattleHUD_Log");
		fnPlace(pxLog, Zenith_UI::AnchorPreset::BottomCenter, 0.0f, -48.0f, 900.0f, 72.0f);
		if (pxLog != nullptr)
		{
			pxLog->SetFontSize(30.0f);
			pxLog->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxLog->SetMaxWidth(900.0f);
			pxLog->SetColor(xWhite);
		}

		// Enemy active panel -- top-left, left-aligned; its HP bar just below.
		Zenith_UI::Zenith_UIText* pxEnemyPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>("BattleHUD_EnemyPanel");
		fnPlace(pxEnemyPanel, Zenith_UI::AnchorPreset::TopLeft, 40.0f, 36.0f, 320.0f, 32.0f);
		if (pxEnemyPanel != nullptr)
		{
			pxEnemyPanel->SetFontSize(24.0f);
			pxEnemyPanel->SetAlignment(Zenith_UI::TextAlignment::Left);
			pxEnemyPanel->SetColor(xWhite);
		}

		Zenith_UI::Zenith_UIRect* pxEnemyHpBar =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>("BattleHUD_EnemyHPBar");
		fnPlace(pxEnemyHpBar, Zenith_UI::AnchorPreset::TopLeft, 40.0f, 76.0f, 240.0f, 16.0f);
		if (pxEnemyHpBar != nullptr)
		{
			pxEnemyHpBar->SetFillDirection(Zenith_UI::FillDirection::LeftToRight);
			pxEnemyHpBar->SetColor(xHpGreen);
		}

		// Player active panel -- bottom-right, right-aligned; its HP bar just above.
		Zenith_UI::Zenith_UIText* pxPlayerPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>("BattleHUD_PlayerPanel");
		fnPlace(pxPlayerPanel, Zenith_UI::AnchorPreset::BottomRight, -40.0f, -120.0f, 320.0f, 32.0f);
		if (pxPlayerPanel != nullptr)
		{
			pxPlayerPanel->SetFontSize(24.0f);
			pxPlayerPanel->SetAlignment(Zenith_UI::TextAlignment::Right);
			pxPlayerPanel->SetColor(xWhite);
		}

		Zenith_UI::Zenith_UIRect* pxPlayerHpBar =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>("BattleHUD_PlayerHPBar");
		fnPlace(pxPlayerHpBar, Zenith_UI::AnchorPreset::BottomRight, -40.0f, -100.0f, 240.0f, 16.0f);
		if (pxPlayerHpBar != nullptr)
		{
			pxPlayerHpBar->SetFillDirection(Zenith_UI::FillDirection::LeftToRight);
			pxPlayerHpBar->SetColor(xHpGreen);
		}

		// --- Interactive battle menu (8 elements). Sort order 10003 sits above the SC4
		//     HUD (10002) so the menu reads on top; all authored hidden -- ZM_BattleDirector
		//     reveals/highlights/hides them via UpdateMenu/HideMenu. A bottom-right box:
		//     Fight/Catch/Run as a vertical stack (SC4 adds Catch), or a 2x2 move grid in
		//     its place (mutually exclusive screens, so they share the box). ---
		auto fnPlaceMenu = [](Zenith_UI::Zenith_UIElement* pxElement,
			Zenith_UI::AnchorPreset ePreset, float fX, float fY, float fW, float fH)
		{
			if (pxElement == nullptr)
			{
				return;
			}
			pxElement->SetSortOrder(10003);
			pxElement->SetAnchor(ePreset);
			pxElement->SetPivot(ePreset);
			pxElement->SetPosition(fX, fY);
			pxElement->SetSize(fW, fH);
			pxElement->SetVisible(false);
		};

		const Zenith_Maths::Vector4 xMenuPanelColour = { 0.05f, 0.06f, 0.10f, 0.85f };

		// Backing panel -- the box behind the buttons, bottom-right. Tall enough for the
		// 3-row root stack AND wide enough for the 2x2 move grid (they share the box).
		Zenith_UI::Zenith_UIRect* pxMenuPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>("BattleHUD_MenuPanel");
		fnPlaceMenu(pxMenuPanel, Zenith_UI::AnchorPreset::BottomRight, -24.0f, -24.0f, 388.0f, 184.0f);
		if (pxMenuPanel != nullptr)
		{
			pxMenuPanel->SetColor(xMenuPanelColour);
		}

		// Root actions -- a single vertical stack inside the panel, authored top to bottom
		// in Fight / Catch / Run ENTRY order. That is entry IDENTITY order, NOT cursor
		// order: ZM_BATTLE_MENU_FIGHT/CATCH/RUN are entry ids, and the Catch entry is
		// HIDDEN outright when the battle disallows catching (a trainer battle, or the
		// Battle Tower), which makes Run cursor index 1. A cursor is therefore resolved to
		// an entry through ZM_UI_BattleHUD::MenuRootItemAtIndex, never by reading this
		// layout -- and the presenter re-anchors each visible button to the row of its
		// resolved index, so hiding Catch closes the gap instead of leaving a hole. The
		// row Y values come from the SHARED constants for exactly that reason.
		auto fnRootRowY = [](int iRow)
		{
			return fZM_BATTLE_MENU_ROOT_FIRST_ROW_Y + fZM_BATTLE_MENU_ROOT_ROW_PITCH_Y * (float)iRow;
		};

		Zenith_UI::Zenith_UIButton* pxFight =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionFight");
		fnPlaceMenu(pxFight, Zenith_UI::AnchorPreset::BottomRight, -133.0f, fnRootRowY(0), 170.0f, 44.0f);
		if (pxFight != nullptr)
		{
			pxFight->SetFontSize(26.0f);
		}

		Zenith_UI::Zenith_UIButton* pxCatch =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionCatch");
		fnPlaceMenu(pxCatch, Zenith_UI::AnchorPreset::BottomRight, -133.0f, fnRootRowY(1), 170.0f, 44.0f);
		if (pxCatch != nullptr)
		{
			pxCatch->SetFontSize(26.0f);
		}

		Zenith_UI::Zenith_UIButton* pxRun =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionRun");
		fnPlaceMenu(pxRun, Zenith_UI::AnchorPreset::BottomRight, -133.0f, fnRootRowY(2), 170.0f, 44.0f);
		if (pxRun != nullptr)
		{
			pxRun->SetFontSize(26.0f);
		}

		// Move slots -- a 2x2 grid in the same box (shown only in MOVE_SELECT). The
		// director rewrites the labels to the active's move names each frame.
		struct MenuButtonPlace { const char* m_szName; float m_fX; float m_fY; };
		const MenuButtonPlace axMoves[4] =
		{
			{ "BattleHUD_Move0", -206.0f, -80.0f },
			{ "BattleHUD_Move1",  -30.0f, -80.0f },
			{ "BattleHUD_Move2", -206.0f, -32.0f },
			{ "BattleHUD_Move3",  -30.0f, -32.0f },
		};
		for (const MenuButtonPlace& xMove : axMoves)
		{
			Zenith_UI::Zenith_UIButton* pxMove =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xMove.m_szName);
			fnPlaceMenu(pxMove, Zenith_UI::AnchorPreset::BottomRight, xMove.m_fX, xMove.m_fY, 170.0f, 44.0f);
			if (pxMove != nullptr)
			{
				pxMove->SetFontSize(22.0f);
			}
		}
	}

	// The SC7 shop screen, authored WHOLE like the SC6 bag: a centred panel, the
	// mode/money/quantity header, six list rows and the eight controls in two bands
	// below them. Split out of ZM_ConfigureMenuRoot (which is already five screens long)
	// rather than inlined; it owns exactly the ZM_UI_Shop::sz*_NAME / RowElementName
	// contract and reads all of its geometry off the ZM_UI_Shop f*_ constants, so this
	// site and the presenter can never drift apart. ALL authored HIDDEN.
	void ZM_ConfigureMenuRootShopScreen(Zenith_UIComponent& xUI)
	{
		Zenith_UI::Zenith_UIRect* pxPanel =
			xUI.FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Shop::szPANEL_NAME);
		if (pxPanel != nullptr)
		{
			pxPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPosition(0.0f, 0.0f);
			// Fully COVERS the header band, the row stack and BOTH control bands, so nothing
			// the screen draws bleeds outside the panel it sits on (ZM-D-112).
			pxPanel->SetSize(ZM_UI_Shop::fPANEL_WIDTH, ZM_UI_Shop::fPANEL_HEIGHT);
			pxPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxHeader =
			xUI.FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Shop::szHEADER_NAME);
		if (pxHeader != nullptr)
		{
			pxHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPosition(0.0f, ZM_UI_Shop::fHEADER_CENTRE_Y);
			// Size == the wrap width == SetMaxWidth, with a matching alignment -- all three
			// together, always (the SC2 lesson: the default 100x100 Left-aligned bounds flow
			// the line clean off the right of the screen). The header carries the transaction
			// report too, so it is authored two lines tall.
			pxHeader->SetSize(ZM_UI_Shop::fHEADER_WIDTH, ZM_UI_Shop::fHEADER_HEIGHT);
			pxHeader->SetFontSize(22.0f);
			pxHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxHeader->SetMaxWidth(ZM_UI_Shop::fHEADER_WIDTH);
			pxHeader->SetVisible(false);
		}

		// The rows carry NO explicit navigation links -- the party / bag idiom, and here it
		// is a CORRECTNESS requirement, not a preference: ZM_UI_Shop::Present hides every
		// row past the live entry count, and Zenith_UICanvas::NavigateDown only falls back
		// to the spatial search when the link is NULL. A bake-time link from the last LIVE
		// row into a row Present has just hidden would be fetched, fail the
		// visible+focusable test, and swallow the press -- dead navigation on every partial
		// page. Liveness is per-page runtime state, so it cannot be wired at bake time.
		for (u_int uRow = 0u; uRow < ZM_UI_Shop::uROWS_PER_PAGE; ++uRow)
		{
			Zenith_UI::Zenith_UIButton* pxRow =
				xUI.FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Shop::RowElementName(uRow));
			if (pxRow == nullptr)
			{
				continue;
			}
			pxRow->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxRow->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPosition(0.0f,
				ZM_UI_Shop::fROW_FIRST_CENTRE_Y + ZM_UI_Shop::fROW_PITCH_Y * (float)uRow);
			pxRow->SetSize(ZM_UI_Shop::fROW_WIDTH, ZM_UI_Shop::fROW_HEIGHT);
			pxRow->SetFontSize(22.0f);
			pxRow->SetFocusable(true);
			pxRow->SetVisible(false);
		}

		// The eight controls, in two bands below the list and likewise unlinked. CONFIRM
		// sits ALONE at x == 0 in the first band, directly under the row column: the engine
		// scores spatial candidates on raw squared distance, so from any live row (they all
		// share x == 0) it is always the nearest element below -- ONE Down press reaches the
		// primary action even when the page holds a single row.
		struct ShopControl { const char* m_szName; float m_fX; float m_fY; };
		const ShopControl axShopControls[ZM_UI_Shop::uCONTROL_COUNT] =
		{
			{ ZM_UI_Shop::szBUY_TAB_NAME,   -ZM_UI_Shop::fCONTROL_OUTER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szSELL_TAB_NAME,  -ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szCONFIRM_NAME,                            0.0f, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szPREV_PAGE_NAME,  ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szNEXT_PAGE_NAME,  ZM_UI_Shop::fCONTROL_OUTER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szQTY_DOWN_NAME,  -ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND2_Y },
			{ ZM_UI_Shop::szQTY_UP_NAME,                             0.0f, ZM_UI_Shop::fCONTROL_BAND2_Y },
			{ ZM_UI_Shop::szEXIT_NAME,       ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND2_Y },
		};
		for (const ShopControl& xControl : axShopControls)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				xUI.FindElement<Zenith_UI::Zenith_UIButton>(xControl.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(xControl.m_fX, xControl.m_fY);
			pxButton->SetSize(ZM_UI_Shop::fCONTROL_WIDTH, ZM_UI_Shop::fCONTROL_HEIGHT);
			pxButton->SetFontSize(20.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}
	}

	// The overworld pause menu (S6 item 2 SC1). Authors the ROOT screen's backing panel
	// + Party/Bag/Dex/Exit entries on the selected ZM_MenuRoot entity's UI component:
	// centred vertical stack, sort band 9000/9001 (BELOW WarpFade 10000 / BattleFade
	// 10001 so a fade always covers the menu), each entry focusable + navigation-wired
	// (up/down) for deterministic engine focus-nav, and ALL authored hidden --
	// ZM_UI_MenuStack shows/hides + focuses them at runtime. Element names are the
	// ZM_UI_MenuStack::sz*_NAME contract. SC2 adds the dialogue box (a bottom-centre
	// panel + wrapped text, also authored hidden) under the ZM_UI_DialogueBox::sz*_NAME
	// contract -- to which SC8 adds the two yes/no prompt buttons inside that same panel
	// -- SC4 the party screen (list panel + six slot rows + summary panel and
	// body, all hidden) under the ZM_UI_Party::sz*_NAME / SlotElementName contract, and
	// SC5 the dex screen's STATIC half (panel + completion header + two page buttons)
	// under the ZM_UI_Dex::sz*_NAME contract -- its grid is built at runtime, not here --
	// and SC6 the bag screen (panel + header + eight list rows + four nav buttons),
	// authored WHOLE under the ZM_UI_Bag::sz*_NAME / RowElementName contract. SC7's
	// shop screen (panel + header + six list rows + eight controls) is authored the same
	// way, in ZM_ConfigureMenuRootShopScreen above.
	void ZM_ConfigureMenuRoot()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"MenuRoot authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		// Backing panel -- centred box behind the entries, authored hidden.
		Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_MenuStack::szROOT_PANEL_NAME);
		if (pxPanel != nullptr)
		{
			pxPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPosition(0.0f, 0.0f);
			pxPanel->SetSize(260.0f, 232.0f);
			pxPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPanel->SetVisible(false);
		}

		// The four entries, top to bottom (== ZM_MENU_ROOT_PARTY..EXIT). 48 px pitch,
		// centred, focusable, authored hidden.
		struct MenuEntry { ZM_MENU_ROOT_ITEM m_eItem; float m_fY; };
		const MenuEntry axEntries[ZM_MENU_ROOT_ITEM_COUNT] =
		{
			{ ZM_MENU_ROOT_PARTY, -72.0f },
			{ ZM_MENU_ROOT_BAG,   -24.0f },
			{ ZM_MENU_ROOT_DEX,    24.0f },
			{ ZM_MENU_ROOT_EXIT,   72.0f },
		};
		Zenith_UI::Zenith_UIButton* apxButtons[ZM_MENU_ROOT_ITEM_COUNT] = {};
		for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(
					ZM_UI_MenuStack::RootItemElementName(axEntries[i].m_eItem));
			apxButtons[i] = pxButton;
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(0.0f, axEntries[i].m_fY);
			pxButton->SetSize(220.0f, 44.0f);
			pxButton->SetFontSize(26.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}

		// Explicit up/down navigation links (no wrap); left/right null. The engine's
		// focus-nav follows these first, falling back to the spatial search otherwise.
		for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
		{
			if (apxButtons[i] == nullptr)
			{
				continue;
			}
			Zenith_UI::Zenith_UIElement* pxUp   = (i > 0u) ? apxButtons[i - 1u] : nullptr;
			Zenith_UI::Zenith_UIElement* pxDown = (i + 1u < ZM_MENU_ROOT_ITEM_COUNT) ? apxButtons[i + 1u] : nullptr;
			apxButtons[i]->SetNavigation(pxUp, pxDown, nullptr, nullptr);
		}

		// The SC2 dialogue box: a wide bottom-centre panel + its wrapped text line, in
		// the same 9000/9001 sort band and likewise authored HIDDEN -- ZM_UI_DialogueBox
		// shows them while the DIALOGUE screen is on top. NOT focusable: the box advances
		// on confirm, never by focus-nav.
		Zenith_UI::Zenith_UIRect* pxDialoguePanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_DialogueBox::szPANEL_NAME);
		if (pxDialoguePanel != nullptr)
		{
			pxDialoguePanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxDialoguePanel->SetAnchor(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialoguePanel->SetPivot(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialoguePanel->SetPosition(0.0f, -32.0f);
			pxDialoguePanel->SetSize(880.0f, 160.0f);
			pxDialoguePanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.90f });
			pxDialoguePanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxDialogueText =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_DialogueBox::szTEXT_NAME);
		if (pxDialogueText != nullptr)
		{
			pxDialogueText->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxDialogueText->SetAnchor(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialogueText->SetPivot(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialogueText->SetPosition(0.0f, -56.0f);
			// Size == the wrap width (the BattleHUD_Log idiom above): text is drawn inside
			// the element's bounds, so leaving the default 100x100 would start a Left-
			// aligned line at centre-50 and flow it clean off the right of the screen. 120
			// tall keeps the box inside the 160-tall panel at the -56 offset.
			pxDialogueText->SetSize(820.0f, 120.0f);
			pxDialogueText->SetFontSize(24.0f);
			pxDialogueText->SetAlignment(Zenith_UI::TextAlignment::Center);   // matches the BottomCenter anchor
			pxDialogueText->SetMaxWidth(820.0f);   // > 0 enables word wrap inside the panel
			pxDialogueText->SetVisible(false);
		}

		// The SC8 yes/no prompt buttons, side by side in the dialogue panel's lower-right
		// band (the geometry constants are ZM_UI_DialogueBox's, so this site and the
		// presenter can never drift). They sit INSIDE the 880x160 panel -- a question the
		// player answers must never float over the world (ZM-D-112) -- and BELOW where a
		// one/two-line prompt actually renders. Authored HIDDEN and NOT focusable:
		// ZM_UI_DialogueBox::Present raises + labels them only while a choice is awaiting an
		// answer, and turns both off again afterwards.
		//
		// NO SetNavigation between them, deliberately: the engine consults the explicit
		// link FIRST and only falls back to the spatial search when it is null, so a
		// bake-time link into a button Present has just hidden would swallow the press
		// outright (the SC6/SC7 rule). Side by side at the same Y, the spatial search walks
		// Yes <-> No on Left/Right with no links at all.
		struct DialogueChoiceButton { const char* m_szName; const char* m_szLabel; float m_fX; };
		const DialogueChoiceButton axChoiceButtons[2] =
		{
			{ ZM_UI_DialogueBox::szYES_NAME, "Yes", ZM_UI_DialogueBox::fCHOICE_YES_X },
			{ ZM_UI_DialogueBox::szNO_NAME,  "No",  ZM_UI_DialogueBox::fCHOICE_NO_X  },
		};
		for (const DialogueChoiceButton& xChoice : axChoiceButtons)
		{
			Zenith_UI::Zenith_UIButton* pxChoice =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xChoice.m_szName);
			if (pxChoice == nullptr)
			{
				continue;
			}
			pxChoice->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxChoice->SetAnchor(Zenith_UI::AnchorPreset::BottomCenter);
			pxChoice->SetPivot(Zenith_UI::AnchorPreset::BottomCenter);
			pxChoice->SetPosition(xChoice.m_fX, ZM_UI_DialogueBox::fCHOICE_Y);
			pxChoice->SetSize(ZM_UI_DialogueBox::fCHOICE_WIDTH, ZM_UI_DialogueBox::fCHOICE_HEIGHT);
			pxChoice->SetFontSize(20.0f);
			pxChoice->SetText(xChoice.m_szLabel);   // the arming caller overwrites this at runtime
			pxChoice->SetFocusable(false);
			pxChoice->SetVisible(false);
		}

		// The SC4 party screen: a centred list panel, six slot buttons stacked inside it,
		// and a summary panel + body text on top -- same 9000/9001 sort band, ALL authored
		// HIDDEN (ZM_UI_Party shows what the live party fills). The slots are focusable and
		// navigation-wired exactly like the ROOT entries; ZM_UI_Party turns the unfilled
		// ones back off at runtime so nav can never reach an empty slot.
		Zenith_UI::Zenith_UIRect* pxPartyPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Party::szPANEL_NAME);
		if (pxPartyPanel != nullptr)
		{
			pxPartyPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPartyPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPartyPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPartyPanel->SetPosition(0.0f, 0.0f);
			pxPartyPanel->SetSize(560.0f, 360.0f);
			pxPartyPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPartyPanel->SetVisible(false);
		}

		// Six rows at a 52 px pitch, centred on the panel (5 gaps -> a 260 px span, so
		// +/-130 keeps the stack inside the 360-tall panel).
		Zenith_UI::Zenith_UIButton* apxPartySlots[ZM_UI_Party::uMAX_SLOTS] = {};
		for (u_int u = 0u; u < ZM_UI_Party::uMAX_SLOTS; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxSlot =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Party::SlotElementName(u));
			apxPartySlots[u] = pxSlot;
			if (pxSlot == nullptr)
			{
				continue;
			}
			pxSlot->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxSlot->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxSlot->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxSlot->SetPosition(0.0f, -130.0f + 52.0f * static_cast<float>(u));
			pxSlot->SetSize(500.0f, 44.0f);
			pxSlot->SetFontSize(22.0f);
			pxSlot->SetFocusable(true);
			pxSlot->SetVisible(false);
		}

		// DELIBERATELY NO SetNavigation links on the slot pool (unlike the ROOT entries,
		// whose four items are always visible). Slot LIVENESS is per-page runtime state:
		// ZM_UI_Party::Present hides + SetFocusable(false)s every slot past the party
		// count, and Zenith_UICanvas::NavigateDown consults the explicit link FIRST,
		// falling back to the spatial FindNearestFocusable only when that link is NULL --
		// a link into a slot that Present has just hidden therefore SWALLOWS the press
		// instead of degrading. The slots share x, so the spatial search walks the live
		// column correctly and re-reads liveness every frame. (SC6 hit exactly this on
		// the bag list, where it also blocked the walk down onto the nav band.) Bake-time
		// links are additionally NOT serialized by Zenith_UIElement::WriteToDataStream,
		// so they would only exist in tools builds -- another reason not to rely on them.

		Zenith_UI::Zenith_UIRect* pxSummaryPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Party::szSUMMARY_PANEL_NAME);
		if (pxSummaryPanel != nullptr)
		{
			// +2/+3 inside the SAME menu band (still far below the WarpFade 10000 /
			// BattleFade 10001 overlays): the summary OVERLAYS the list, and the slot
			// buttons sit at 9001, so a flat 9000/9001 pair would draw the rows straight
			// through it.
			pxSummaryPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER + 2);
			pxSummaryPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxSummaryPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxSummaryPanel->SetPosition(0.0f, 0.0f);
			// 340 tall, NOT 300: the slot stack spans y [-152,+152] (slot 0 sits at -130
			// with a 44-tall Center pivot, slot 5 at +130), so a 300-tall overlay would
			// leave the top 2 px of slot 0 and the bottom 2 px of slot 5 rendering OUTSIDE
			// it -- the exact bleed-through class the S5 visual gate (ZM-D-112) caught.
			// 340 still fits inside the 360-tall list panel.
			pxSummaryPanel->SetSize(520.0f, 340.0f);
			pxSummaryPanel->SetColor({ 0.08f, 0.09f, 0.14f, 0.95f });
			pxSummaryPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxSummaryText =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Party::szSUMMARY_TEXT_NAME);
		if (pxSummaryText != nullptr)
		{
			pxSummaryText->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER + 2);
			pxSummaryText->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxSummaryText->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxSummaryText->SetPosition(0.0f, 0.0f);
			// Size == the wrap width == SetMaxWidth, with a matching alignment: the SC2
			// lesson is that leaving the default 100x100 Left-aligned bounds flows the body
			// clean off the right of the screen. All three are set together, always.
			pxSummaryText->SetSize(470.0f, 260.0f);
			pxSummaryText->SetFontSize(20.0f);
			pxSummaryText->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxSummaryText->SetMaxWidth(470.0f);
			pxSummaryText->SetVisible(false);
		}

		// The SC5 dex screen: only the STATIC widgets are authored here (a centred panel,
		// the completion header and the two page buttons) -- the 5x6
		// Zenith_UIGridLayoutGroup and its 30 cells are built ONCE AT RUNTIME by
		// ZM_UI_Dex::Present, because the engine exposes no CreateGridLayoutGroup /
		// AddStep_* for a grid and adding one is out of SC5's scope. Same 9000/9001 sort
		// band, ALL authored HIDDEN. Geometry comes from the ZM_UI_Dex f*_ constants so
		// this site and the runtime grid build can never drift apart.
		Zenith_UI::Zenith_UIRect* pxDexPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Dex::szPANEL_NAME);
		if (pxDexPanel != nullptr)
		{
			pxDexPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxDexPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxDexPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxDexPanel->SetPosition(0.0f, 0.0f);
			// Fully COVERS the grid (912x270 at +10) plus the header and page-button bands,
			// so nothing the screen draws bleeds outside the panel it sits on (ZM-D-112).
			pxDexPanel->SetSize(ZM_UI_Dex::fPANEL_WIDTH, ZM_UI_Dex::fPANEL_HEIGHT);
			pxDexPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxDexPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxDexHeader =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Dex::szHEADER_NAME);
		if (pxDexHeader != nullptr)
		{
			pxDexHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxDexHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxDexHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxDexHeader->SetPosition(0.0f, ZM_UI_Dex::fHEADER_CENTRE_Y);
			// Size == the wrap width == SetMaxWidth, with a matching alignment -- all three
			// together, always (the SC2 lesson: the default 100x100 Left-aligned bounds flow
			// the line clean off the right of the screen).
			pxDexHeader->SetSize(ZM_UI_Dex::fHEADER_WIDTH, ZM_UI_Dex::fHEADER_HEIGHT);
			pxDexHeader->SetFontSize(26.0f);
			pxDexHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxDexHeader->SetMaxWidth(ZM_UI_Dex::fHEADER_WIDTH);
			pxDexHeader->SetVisible(false);
		}

		// The two page buttons sit BELOW the grid and carry NO explicit navigation links:
		// the engine's spatial focus-nav walks down off the last grid row onto them and
		// back up again, which is the whole point of using a grid.
		struct DexPageButton { const char* m_szName; float m_fX; };
		const DexPageButton axDexPageButtons[2] =
		{
			{ ZM_UI_Dex::szPREV_NAME, -ZM_UI_Dex::fPAGE_BUTTON_CENTRE_X },
			{ ZM_UI_Dex::szNEXT_NAME,  ZM_UI_Dex::fPAGE_BUTTON_CENTRE_X },
		};
		for (const DexPageButton& xPageButton : axDexPageButtons)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xPageButton.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(xPageButton.m_fX, ZM_UI_Dex::fPAGE_BUTTON_CENTRE_Y);
			pxButton->SetSize(ZM_UI_Dex::fPAGE_BUTTON_WIDTH, ZM_UI_Dex::fPAGE_BUTTON_HEIGHT);
			pxButton->SetFontSize(22.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}

		// The SC6 bag screen, authored WHOLE (unlike the dex): a centred panel, the
		// pocket/money header, eight list rows stacked inside it and four nav buttons
		// below them. It is a 1-D LIST, so there is nothing a Zenith_UIGridLayoutGroup
		// would buy -- the rows are a plain authored pool like the party slots, with no
		// runtime construction and none of SC5's reparenting ownership hazard. Same
		// 9000/9001 sort band, ALL authored HIDDEN; geometry comes from the ZM_UI_Bag
		// f*_ constants so this site and the presenter can never drift apart.
		Zenith_UI::Zenith_UIRect* pxBagPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Bag::szPANEL_NAME);
		if (pxBagPanel != nullptr)
		{
			pxBagPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxBagPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxBagPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxBagPanel->SetPosition(0.0f, 0.0f);
			// Fully COVERS the header band, the row stack and the nav band, so nothing the
			// screen draws bleeds outside the panel it sits on (ZM-D-112).
			pxBagPanel->SetSize(ZM_UI_Bag::fPANEL_WIDTH, ZM_UI_Bag::fPANEL_HEIGHT);
			pxBagPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxBagPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxBagHeader =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Bag::szHEADER_NAME);
		if (pxBagHeader != nullptr)
		{
			pxBagHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxBagHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxBagHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxBagHeader->SetPosition(0.0f, ZM_UI_Bag::fHEADER_CENTRE_Y);
			// Size == the wrap width == SetMaxWidth, with a matching alignment -- all three
			// together, always (the SC2 lesson: the default 100x100 Left-aligned bounds flow
			// the line clean off the right of the screen).
			pxBagHeader->SetSize(ZM_UI_Bag::fHEADER_WIDTH, ZM_UI_Bag::fHEADER_HEIGHT);
			pxBagHeader->SetFontSize(26.0f);
			pxBagHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxBagHeader->SetMaxWidth(ZM_UI_Bag::fHEADER_WIDTH);
			pxBagHeader->SetVisible(false);
		}

		// The rows carry NO explicit navigation links -- the dex idiom, and here it is a
		// CORRECTNESS requirement, not a preference: ZM_UI_Bag::Present hides every row past
		// the live stack count, and Zenith_UICanvas::NavigateDown only falls back to the
		// spatial search when the link is NULL. A bake-time link from the last LIVE row to a
		// row Present has just hidden would be fetched, fail the visible+focusable test, and
		// swallow the press -- so on every partial page (the starter BALL pocket holds ONE
		// stack) Down would be dead. Liveness is per-page runtime state, so it cannot be
		// wired at bake time; the spatial search reads it correctly every frame because it
		// collects only visible + focusable elements.
		for (u_int u = 0u; u < ZM_UI_Bag::uROWS_PER_PAGE; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxRow =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Bag::RowElementName(u));
			if (pxRow == nullptr)
			{
				continue;
			}
			pxRow->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxRow->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPosition(0.0f,
				ZM_UI_Bag::fROW_FIRST_CENTRE_Y + ZM_UI_Bag::fROW_PITCH_Y * static_cast<float>(u));
			pxRow->SetSize(ZM_UI_Bag::fROW_WIDTH, ZM_UI_Bag::fROW_HEIGHT);
			pxRow->SetFontSize(22.0f);
			pxRow->SetFocusable(true);
			pxRow->SetVisible(false);
		}

		// The four nav buttons sit BELOW the rows and carry no explicit links either: the
		// rows all share x == 0, so the spatial search walks the live column vertically and,
		// off the LAST LIVE row, onto this band (and back up again). Pocket pair on the
		// left, page pair on the right.
		struct BagNavButton { const char* m_szName; float m_fX; };
		const BagNavButton axBagNavButtons[4] =
		{
			{ ZM_UI_Bag::szPREV_POCKET_NAME, -ZM_UI_Bag::fNAV_BUTTON_OUTER_X },
			{ ZM_UI_Bag::szNEXT_POCKET_NAME, -ZM_UI_Bag::fNAV_BUTTON_INNER_X },
			{ ZM_UI_Bag::szPREV_PAGE_NAME,    ZM_UI_Bag::fNAV_BUTTON_INNER_X },
			{ ZM_UI_Bag::szNEXT_PAGE_NAME,    ZM_UI_Bag::fNAV_BUTTON_OUTER_X },
		};
		for (const BagNavButton& xNavButton : axBagNavButtons)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xNavButton.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(xNavButton.m_fX, ZM_UI_Bag::fNAV_BUTTON_CENTRE_Y);
			pxButton->SetSize(ZM_UI_Bag::fNAV_BUTTON_WIDTH, ZM_UI_Bag::fNAV_BUTTON_HEIGHT);
			pxButton->SetFontSize(20.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}

		ZM_ConfigureMenuRootShopScreen(*pxUI);
	}

	bool ZM_SetSelectedSpawnPointTag(const char* szTag)
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_SpawnPoint* pxSpawnPoint = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_SpawnPoint>()
			: nullptr;
		Zenith_Assert(pxSpawnPoint != nullptr,
			"Spawn authoring requires the selected ZM_SpawnPoint");
		return pxSpawnPoint != nullptr && pxSpawnPoint->SetTag(szTag);
	}

	bool ZM_ConfigureSelectedWarpTrigger(
		u_int uTargetBuildIndex, const char* szSpawnTag)
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_WarpTrigger* pxWarpTrigger = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_WarpTrigger>()
			: nullptr;
		Zenith_Assert(pxWarpTrigger != nullptr,
			"Warp authoring requires the selected ZM_WarpTrigger");
		return pxWarpTrigger != nullptr
			&& pxWarpTrigger->Configure(uTargetBuildIndex, szSpawnTag);
	}

	void ZM_ConfigureTownCenterSpawnPoint()
	{
		const bool bTagSet = ZM_SetSelectedSpawnPointTag("TownCenter");
		Zenith_Assert(bTagSet, "TownCenter is not a valid spawn tag");
	}

	void ZM_ConfigureDoorSpawnPoint()
	{
		Zenith_Assert(ZM_SetSelectedSpawnPointTag("Door"),
			"Door is not a valid spawn tag");
	}

	void ZM_ConfigureFromHomeSpawnPoint()
	{
		Zenith_Assert(ZM_SetSelectedSpawnPointTag("FromHome"),
			"FromHome is not a valid spawn tag");
	}

	void ZM_ConfigurePlayerHomeExitTrigger()
	{
		Zenith_Assert(ZM_ConfigureSelectedWarpTrigger(2u, "FromHome"),
			"PlayerHome exit warp configuration is invalid");
	}

	void ZM_ConfigureHomeDoorTrigger()
	{
		Zenith_Assert(ZM_ConfigureSelectedWarpTrigger(40u, "Door"),
			"Dawnmere Home doorway warp configuration is invalid");
	}

	// ---- S6 item 3 SC5: the authored Dawnmere NPCs ---------------------------
	//
	// Reach BONUS authored onto every Dawnmere NPC. 0.4 is this NPC's OWN AABB
	// half-width (the greybox body is 0.8 m wide), so the global 2.5 m reach is not
	// silently spent crossing the NPC's own body. Note the player capsule adds a
	// further 0.4 m of its own radius, so contact actually happens at ~0.8 m from
	// the NPC's transform centre against a 2.9 m effective reach -- do NOT read 0.4
	// as "the distance the capsule stops at". Well inside
	// ZM_Interactable::fMAX_RADIUS (8.0), and far too small to let one NPC swallow a
	// neighbour's press: the closest NPC PAIR in this town is 16.1 m apart, 5.5x the
	// effective reach.
	constexpr float fZM_NPC_AUTHORED_RADIUS = 0.4f;

	// The shared body of the three stationary configure functions below. A PER-NPC function is
	// unavoidable: AddStep_Custom takes a captureless `void (*)()`, so there is no
	// way to hand one parameterised step the row it should install.
	bool ZM_ConfigureSelectedNpc(ZM_NPC_ID eId)
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_Interactable* pxInteractable = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_Interactable>()
			: nullptr;
		Zenith_Assert(pxInteractable != nullptr,
			"NPC authoring requires the selected ZM_Interactable");
		if (pxInteractable == nullptr)
		{
			return false;
		}

		// SetInteractable LAST, deliberately: SetNpcId fails CLOSED by clearing the
		// candidacy flag, so arming before the row is installed would be silently
		// undone by a bad id and the NPC would author itself mute.
		const bool bIdSet = pxInteractable->SetNpcId(eId);
		const bool bRadiusSet = pxInteractable->SetRadius(fZM_NPC_AUTHORED_RADIUS);
		pxInteractable->SetInteractable(true);
		// IsInteractable() is the LIVE candidacy answer the picker reads, so assert on
		// it rather than on the setters alone -- that is the property authoring owes.
		return bIdSet && bRadiusSet && pxInteractable->IsInteractable();
	}

	void ZM_ConfigureVillagerNpc()
	{
		Zenith_Assert(ZM_ConfigureSelectedNpc(ZM_NPC_VILLAGER),
			"Dawnmere Villager NPC authoring is invalid");
	}

	void ZM_ConfigureTradePostClerkNpc()
	{
		Zenith_Assert(ZM_ConfigureSelectedNpc(ZM_NPC_TRADE_POST_CLERK),
			"Dawnmere Trade Post clerk NPC authoring is invalid");
	}

	void ZM_ConfigureCaretakerNpc()
	{
		Zenith_Assert(ZM_ConfigureSelectedNpc(ZM_NPC_CARETAKER),
			"Dawnmere Caretaker NPC authoring is invalid");
	}

	// S7 item 2 SC1: the story-gated NPC. Nothing about the AUTHORING differs from
	// the other stationary talkers -- the gate lives entirely in the compiled row
	// (ZM_NpcData.cpp), so a gated NPC costs no extra authoring step and no extra
	// component state.
	void ZM_ConfigureRouteWardenNpc()
	{
		const bool bConfigured = ZM_ConfigureSelectedNpc(ZM_NPC_ROUTE_WARDEN);
		Zenith_Assert(bConfigured, "Dawnmere Route Warden NPC authoring is invalid");
	}

	void ZM_ConfigureWandererNpc()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_Interactable* pxInteractable = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_Interactable>()
			: nullptr;
		Zenith_Assert(pxInteractable != nullptr,
			"Wanderer authoring requires the selected ZM_Interactable");
		if (pxInteractable == nullptr)
		{
			return;
		}

		ZM_WalkerWaypoints xWaypoints{};
		xWaypoints.m_uCount = 2u;
		// A north/south loop at x=540 stays 28 m east of TownCenter spawn, beyond
		// every existing straight-line traversal corridor and all stationary NPCs.
		// Waypoint Y is serialized only as an authored reference: ZM_StepWalker is
		// explicitly XZ-only, while the dynamic capsule owns Y and follows terrain.
		xWaypoints.m_axPoints[0] = { 540.0f, 26.88577f, 476.0f };
		xWaypoints.m_axPoints[1] = { 540.0f, 26.88577f, 484.0f };

		const bool bNpcConfigured = ZM_ConfigureSelectedNpc(ZM_NPC_WANDERER);
		const bool bPatrolConfigured =
			pxInteractable->ConfigureWander(xWaypoints, ZM_WalkerTuning{});
		Zenith_Assert(bNpcConfigured && bPatrolConfigured
			&& pxInteractable->IsWanderEnabled()
			&& pxInteractable->GetWaypointCount() == 2u,
			"Dawnmere Wanderer NPC authoring is invalid");
	}

	// One authored NPC: a greybox body the player can SEE, a STATIC AABB it can
	// physically bump into (so walking up to one ends in contact rather than in
	// walking through it), the ZM_Interactable that makes it talkable, and the
	// captureless step that installs its row. Step order mirrors HomeDoorTrigger --
	// transform, collider, components, then the configure custom step.
	void ZM_QueueDawnmereNpc(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const Zenith_Maths::Vector3& xCenter,
		const Zenith_Maths::Vector3& xScale,
		void (*pfnConfigure)())
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(xCenter.x, xCenter.y, xCenter.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_Interactable");
		xAuto.AddStep_Custom(pfnConfigure);
	}

	// SC8's one moving NPC intentionally does NOT reuse the stationary helper: the
	// authored body contract is a solid dynamic capsule, driven only by XZ velocity.
	void ZM_QueueDawnmereWanderer(
		Zenith_EditorAutomation& xAuto,
		const Zenith_Maths::Vector3& xCenter,
		const Zenith_Maths::Vector3& xScale)
	{
		xAuto.AddStep_CreateEntity("Npc_Wanderer");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(xCenter.x, xCenter.y, xCenter.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_Interactable");
		xAuto.AddStep_Custom(&ZM_ConfigureWandererNpc);
	}

	void ZM_QueueGreyboxBlock(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xScale)
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xPosition.x, xPosition.y, xPosition.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
	}

	const char* ZM_TerrainBakeQueueResultToString(
		ZM_TERRAIN_BAKE_QUEUE_RESULT eResult)
	{
		switch (eResult)
		{
		case ZM_TERRAIN_BAKE_HEADLESS: return "HEADLESS";
		case ZM_TERRAIN_BAKE_WARM: return "WARM";
		case ZM_TERRAIN_BAKE_QUEUED: return "QUEUED";
		case ZM_TERRAIN_BAKE_PREPARE_FAILED: return "PREPARE_FAILED";
		default: return "INVALID";
		}
	}

	void ZM_InitializeDawnmereTerrainMaterials()
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		for (u_int uSlot = 0; uSlot < 4u; ++uSlot)
		{
			const ZM_TerrainMaterialSpec& xSpec = xRecipe.m_pxMaterials[uSlot];
			MaterialHandle& xHandle = g_axDawnmereTerrainMaterials[uSlot];
			xHandle = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();

			Zenith_MaterialAsset* pxMaterial = xHandle.GetDirect();
			pxMaterial->SetName(xSpec.m_szName);
			pxMaterial->SetBaseColor({
				xSpec.m_afBaseColour[0],
				xSpec.m_afBaseColour[1],
				xSpec.m_afBaseColour[2],
				xSpec.m_afBaseColour[3] });
			pxMaterial->SetRoughness(xSpec.m_fRoughness);
			pxMaterial->SetMetallic(xSpec.m_fMetallic);
		}
	}
}
#endif

const char* Project_GetName()
{
	return "Zenithmon";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterGameComponents()
{
	// Meta-registry registration is the ZENITH_REGISTER_COMPONENT macro above.
	// Tools builds additionally register with the editor "Add Component" menu
	// (append-anytime registry, not sealed).
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GameComponent>("ZM_Game");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_TerrainGrass>("ZM_TerrainGrass");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_PlayerController>("ZM_PlayerController");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_FollowCamera>("ZM_FollowCamera");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GameStateManager>("ZM_GameStateManager");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_SpawnPoint>("ZM_SpawnPoint");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_WarpTrigger>("ZM_WarpTrigger");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GreyboxVisual>("ZM_GreyboxVisual");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_BattleArena>("ZM_BattleArena");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_TallGrassSystem>("ZM_TallGrassSystem");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_BattleTransition>("ZM_BattleTransition");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_BattleDirector>("ZM_BattleDirector");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_UI_MenuStack>("ZM_UI_MenuStack");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_Interactable>("ZM_Interactable");

	// Runtime toggle for the battle presenter's instant-battle mode (collapses all
	// presentation timing). Bound by reference to the ZM_BattleDirectorCore backing
	// store (ZM-D-101); flip it in the Debug Variables panel under Zenithmon/Battle.
	g_xEngine.DebugVariables().AddBoolean({ "Zenithmon", "Battle", "zm_instant_battles" }, ZM_InstantBattlesRef());
#endif

	// Save/load persistence root: %APPDATA%/Zenith/Zenithmon/. The versioned
	// per-module save schema lands at S7 (Docs/SaveFormat.md); initialising from
	// S0 keeps the test-hook plumbing live from the first commit.
	Zenith_SaveData::Initialise("Zenithmon");

#ifdef ZENITH_INPUT_SIMULATOR
	// Between-tests reset for batched automated tests. The harness force-loads
	// scene 0 before firing this hook, so entity-owned state is already cleared
	// via OnDestroy; only ownerless game globals need explicit reset here. Keep
	// this hook current as systems land (the DP hook is the reference).
	Zenith_AutomatedTestRunner::RegisterBetweenTestsHook([]()
	{
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		// The interaction latches are process-global (the runtime rides on whichever
		// player exists), so a batched test must not inherit the previous test's
		// interaction outcome or raise count.
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		// The persistent manager's GameState survives DontDestroyOnLoad across tests;
		// re-seed the starter so a caught/levelled party cannot leak into the next test.
		ZM_GameStateManager::ResetGameStateForTests();
		ZM_SetInstantBattlesForTests(false);
		// Disk hygiene FIRST: Zenith_SaveData::ClearForTest wipes only the in-memory
		// write log and readback stash and explicitly does NOT delete files
		// (Zenith_SaveData.h:119), so a .zsave written by one test would otherwise
		// survive into the next test AND into the next process.
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
	});
#endif
}

void Project_Shutdown()
{
#ifdef ZENITH_TOOLS
	for (MaterialHandle& xMaterial : g_axDawnmereTerrainMaterials)
	{
		xMaterial = MaterialHandle{};
	}
#endif
}

void Project_LoadInitialScene();	// forward decl for the automation step below

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// Automation borrows these handles while serializing Dawnmere. The saved
	// terrain owns its material data; these temporary handles live until shutdown.
	ZM_InitializeDawnmereTerrainMaterials();
}

// Boot-authored scene: a camera, a title, and the game component, saved to
// Assets/Scenes/FrontEnd.zscen (build index 0 -- the plan's scene table lives
// in Docs/GameDesignDocument.md). A _False / Android build LOADS that baked
// scene instead of authoring it (this function is tools-only), so the FIRST
// build+run must be a *_True config to bake FrontEnd.zscen.
void Project_RegisterEditorAutomationSteps()
{
	ZM_TerrainBakeSelection xTerrainSelection;
	if (!ZM_ParseTerrainBakeSelection(__argc, __argv, xTerrainSelection))
	{
		const bool bHasErrorArgument = xTerrainSelection.m_iErrorArgument >= 0 &&
			xTerrainSelection.m_iErrorArgument < __argc && __argv != nullptr &&
			__argv[xTerrainSelection.m_iErrorArgument] != nullptr;
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Selector rejected: result=%s, argvIndex=%d, argument='%s'; no automation queued",
			ZM_TerrainBakeSelectionParseResultToString(
				xTerrainSelection.m_eParseResult),
			xTerrainSelection.m_iErrorArgument,
			bHasErrorArgument ? __argv[xTerrainSelection.m_iErrorArgument] : "<null>");
		Zenith_Assert(false,
			"Invalid Zenithmon terrain-bake selector at argv index %d",
			xTerrainSelection.m_iErrorArgument);
		return;
	}

	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	xAuto.AddStep_CreateScene("FrontEnd");
	xAuto.AddStep_CreateEntity("ZM_GameStateRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIOverlay("WarpFade");
	xAuto.AddStep_Custom(&ZM_ConfigureWarpFade);
	xAuto.AddStep_AddComponent("ZM_GameStateManager");

	// Its OWN persistent root: ZM_GameStateManager drives WarpFade every frame and
	// its DontDestroyOnLoad relocates every component on ZM_GameStateRoot, so the
	// battle machine gets a separate entity + a separate overlay (ZM-D-097).
	xAuto.AddStep_CreateEntity("ZM_BattleTransitionRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIOverlay("BattleFade");
	xAuto.AddStep_Custom(&ZM_ConfigureBattleFade);
	xAuto.AddStep_AddComponent("ZM_BattleTransition");

	// The overworld pause menu (S6 item 2 SC1) on its OWN persistent root, mirroring
	// the two roots above: a non-transient DontDestroyOnLoad entity carrying a UI
	// component (the ROOT panel + Party/Bag/Dex/Exit entries plus the SC2 dialogue
	// box panel + text, the SC4 party screen, the SC5 dex screen's static widgets,
	// the SC6 bag screen and the SC7 shop screen, all authored hidden by
	// ZM_ConfigureMenuRoot) + the
	// ZM_UI_MenuStack machine. Persistent so the menu and the dialogue box are
	// reachable from every overworld scene (Dawnmere / PlayerHome / future towns)
	// without re-authoring, and separate so its 9000/9001 sort band never collides
	// with the two fade overlays' 10000/10001.
	xAuto.AddStep_CreateEntity("ZM_MenuRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIRect("Menu_RootPanel");
	xAuto.AddStep_CreateUIButton("Menu_RootParty", "Party");
	xAuto.AddStep_CreateUIButton("Menu_RootBag", "Bag");
	xAuto.AddStep_CreateUIButton("Menu_RootDex", "Dex");
	xAuto.AddStep_CreateUIButton("Menu_RootExit", "Exit");
	// The SC2 dialogue box lives on the same root (bottom-centre band, authored
	// hidden): names are the ZM_UI_DialogueBox::szPANEL_NAME / szTEXT_NAME contract.
	xAuto.AddStep_CreateUIRect(ZM_UI_DialogueBox::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_DialogueBox::szTEXT_NAME, "");
	// ...plus the SC8 yes/no prompt buttons on the same panel, likewise hidden. The
	// labels here are the defaults; the caller that ARMS a choice supplies the real ones
	// and ZM_UI_DialogueBox::Present writes them.
	xAuto.AddStep_CreateUIButton(ZM_UI_DialogueBox::szYES_NAME, "Yes");
	xAuto.AddStep_CreateUIButton(ZM_UI_DialogueBox::szNO_NAME, "No");
	// ...and the SC4 party screen (list panel + six slot rows + summary panel/body),
	// likewise authored hidden. SlotElementName returns string literals, so calling it
	// at authoring time is safe.
	xAuto.AddStep_CreateUIRect(ZM_UI_Party::szPANEL_NAME);
	for (u_int uSlot = 0u; uSlot < ZM_UI_Party::uMAX_SLOTS; ++uSlot)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_Party::SlotElementName(uSlot), "");
	}
	xAuto.AddStep_CreateUIRect(ZM_UI_Party::szSUMMARY_PANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Party::szSUMMARY_TEXT_NAME, "");
	// ...and the SC5 dex screen's STATIC widgets (panel + completion header + the two
	// page buttons), likewise authored hidden. The 5x6 grid and its 30 cells are NOT
	// authored -- ZM_UI_Dex::Present builds them once at runtime (there is no engine
	// surface for creating a Zenith_UIGridLayoutGroup from an automation step).
	xAuto.AddStep_CreateUIRect(ZM_UI_Dex::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Dex::szHEADER_NAME, "");
	xAuto.AddStep_CreateUIButton(ZM_UI_Dex::szPREV_NAME, "< Prev");
	xAuto.AddStep_CreateUIButton(ZM_UI_Dex::szNEXT_NAME, "Next >");
	// ...and the SC6 bag screen, authored WHOLE (panel + header + eight list rows + the
	// four pocket/page nav buttons), likewise hidden. Unlike the dex there is NO runtime
	// construction: a 1-D list needs no grid. RowElementName returns string literals, so
	// calling it at authoring time is safe.
	xAuto.AddStep_CreateUIRect(ZM_UI_Bag::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Bag::szHEADER_NAME, "");
	for (u_int uRow = 0u; uRow < ZM_UI_Bag::uROWS_PER_PAGE; ++uRow)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_Bag::RowElementName(uRow), "");
	}
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szPREV_POCKET_NAME, "< Pocket");
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szNEXT_POCKET_NAME, "Pocket >");
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szPREV_PAGE_NAME, "< Prev");
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szNEXT_PAGE_NAME, "Next >");
	// ...and the SC7 shop screen, authored WHOLE too (panel + header + six list rows +
	// the eight controls), likewise hidden. RowElementName / ControlElementName return
	// string literals, so calling them at authoring time is safe. The row labels are
	// written at runtime; the control labels are static and set here.
	xAuto.AddStep_CreateUIRect(ZM_UI_Shop::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Shop::szHEADER_NAME, "");
	for (u_int uRow = 0u; uRow < ZM_UI_Shop::uROWS_PER_PAGE; ++uRow)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_Shop::RowElementName(uRow), "");
	}
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szBUY_TAB_NAME, "Buy");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szSELL_TAB_NAME, "Sell");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szCONFIRM_NAME, "Confirm");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szPREV_PAGE_NAME, "< Prev");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szNEXT_PAGE_NAME, "Next >");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szQTY_DOWN_NAME, "Qty -");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szQTY_UP_NAME, "Qty +");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szEXIT_NAME, "Leave");
	xAuto.AddStep_Custom(&ZM_ConfigureMenuRoot);
	xAuto.AddStep_AddComponent("ZM_UI_MenuStack");

	xAuto.AddStep_CreateEntity("GameManager");
	xAuto.AddStep_AddCamera();
	xAuto.AddStep_SetCameraPosition(0.f, 3.f, 6.f);
	xAuto.AddStep_SetCameraPitch(-0.4f);
	xAuto.AddStep_SetCameraFOV(glm::radians(60.f));
	xAuto.AddStep_SetAsMainCamera();
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIText("Title", "Zenithmon");
	xAuto.AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	xAuto.AddStep_SetUIPosition("Title", 0.f, -220.f);
	xAuto.AddStep_SetUIFontSize("Title", 54.f);
	xAuto.AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	xAuto.AddStep_AddComponent("ZM_Game");
	xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// PlayerHome is a terrain-independent interior and is authored on every
	// tools boot, including headless/cold terrain runs. All shell pieces carry
	// a replaceable procedural greybox visual and their own static collider.
	xAuto.AddStep_CreateScene("PlayerHome");
	ZM_QueueGreyboxBlock(xAuto, "PlayerHomeFloor",
		{ 0.0f, -0.25f, 0.0f }, { 16.0f, 0.5f, 12.0f });
	ZM_QueueGreyboxBlock(xAuto, "PlayerHomeBackWall",
		{ 0.0f, 1.5f, -6.0f }, { 16.0f, 3.0f, 0.5f });
	ZM_QueueGreyboxBlock(xAuto, "PlayerHomeLeftWall",
		{ -8.0f, 1.5f, 0.0f }, { 0.5f, 3.0f, 12.0f });
	ZM_QueueGreyboxBlock(xAuto, "PlayerHomeRightWall",
		{ 8.0f, 1.5f, 0.0f }, { 0.5f, 3.0f, 12.0f });
	ZM_QueueGreyboxBlock(xAuto, "PlayerHomeFrontLeft",
		{ -5.0f, 1.5f, 6.0f }, { 6.0f, 3.0f, 0.5f });
	ZM_QueueGreyboxBlock(xAuto, "PlayerHomeFrontRight",
		{ 5.0f, 1.5f, 6.0f }, { 6.0f, 3.0f, 0.5f });
	ZM_QueueGreyboxBlock(xAuto, "PlayerHomeLintel",
		{ 0.0f, 2.75f, 6.0f }, { 4.0f, 0.5f, 0.5f });

	xAuto.AddStep_CreateEntity("DoorSpawn");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, 0.0f, 3.5f);
	xAuto.AddStep_AddComponent("ZM_SpawnPoint");
	xAuto.AddStep_Custom(&ZM_ConfigureDoorSpawnPoint);

	xAuto.AddStep_CreateEntity("Player");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, 0.9f, 3.5f);
	xAuto.AddStep_SetTransformScale(0.8f, 1.8f, 0.8f);
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(
		COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
	xAuto.AddStep_AddComponent("ZM_PlayerController");

	xAuto.AddStep_CreateEntity("PlayerHomeCamera");
	xAuto.AddStep_AddCamera();
	xAuto.AddStep_SetCameraPosition(0.0f, 3.0f, -2.0f);
	xAuto.AddStep_SetCameraYaw(0.0f);
	xAuto.AddStep_SetCameraPitch(0.0f);
	xAuto.AddStep_SetCameraFOV(glm::radians(65.0f));
	xAuto.AddStep_SetCameraNear(0.1f);
	xAuto.AddStep_SetCameraFar(100.0f);
	xAuto.AddStep_AddComponent("ZM_FollowCamera");
	xAuto.AddStep_SetAsMainCamera();

	xAuto.AddStep_CreateEntity("PlayerHomeExitTrigger");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, 1.0f, 5.2f);
	xAuto.AddStep_SetTransformScale(3.0f, 2.0f, 1.2f);
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(
		COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
	xAuto.AddStep_AddComponent("ZM_WarpTrigger");
	xAuto.AddStep_Custom(&ZM_ConfigurePlayerHomeExitTrigger);

	xAuto.AddStep_SaveScene(
		GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// Battle arena (build index 1) is a terrain-independent, self-contained scene
	// authored 2000 m below the overworld (ZM_BattleArena::fARENA_WORLD_Y) so the
	// S5 additive load never bleeds through the overworld. The ZM_BattleArena
	// component spawns the dome + platforms + six dressing sets in OnStart.
	xAuto.AddStep_CreateScene("Battle");

	xAuto.AddStep_CreateEntity("BattleArena");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, -2000.0f, 0.0f);   // ZM_BattleArena::fARENA_WORLD_Y
	xAuto.AddStep_AddComponent("ZM_BattleArena");

	// The battle presenter-driver (S5 item 4 SC3, order 111): a sibling entity in the
	// Battle scene that watches the persistent transition and, once IN_BATTLE, runs a
	// deterministic AI-vs-AI wild battle and ends it via RequestBattleEnd().
	xAuto.AddStep_CreateEntity("BattleDirector");
	xAuto.AddStep_SetEntityTransient(false);
	// The battle HUD (S5 item 4 SC4): a UI component + five elements authored on the
	// director entity, configured hidden by ZM_ConfigureBattleHUD. ZM_BattleDirector
	// owns a ZM_UI_BattleHUD by value and drives them (reveal / typewriter / hide).
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIText("BattleHUD_Log", "");
	xAuto.AddStep_CreateUIText("BattleHUD_PlayerPanel", "");
	xAuto.AddStep_CreateUIText("BattleHUD_EnemyPanel", "");
	xAuto.AddStep_CreateUIRect("BattleHUD_PlayerHPBar");
	xAuto.AddStep_CreateUIRect("BattleHUD_EnemyHPBar");
	// The interactive battle menu: a backing panel + three root buttons (Fight/Catch/Run,
	// SC4 adds Catch) + four move buttons, all authored hidden by ZM_ConfigureBattleHUD.
	// ZM_UI_BattleHUD (owned by ZM_BattleDirector) shows/highlights/hides them via
	// UpdateMenu/HideMenu.
	xAuto.AddStep_CreateUIRect("BattleHUD_MenuPanel");
	xAuto.AddStep_CreateUIButton("BattleHUD_ActionFight", "Fight");
	xAuto.AddStep_CreateUIButton("BattleHUD_ActionCatch", "Catch");
	xAuto.AddStep_CreateUIButton("BattleHUD_ActionRun",   "Run");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move0", "");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move1", "");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move2", "");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move3", "");
	xAuto.AddStep_Custom(&ZM_ConfigureBattleHUD);
	xAuto.AddStep_AddComponent("ZM_BattleDirector");

	xAuto.AddStep_CreateEntity("BattleCamera");
	xAuto.AddStep_AddCamera();
	// Camera forward at yaw 0 is +Z, so the camera sits on the -Z side looking
	// toward the arena/platforms at Z~=0 (pitched slightly down onto the -2000 m
	// platform plane). Mirrors the PlayerHome camera, which sits behind its subject.
	xAuto.AddStep_SetCameraPosition(0.0f, -1997.5f, -8.0f);
	xAuto.AddStep_SetCameraYaw(0.0f);
	xAuto.AddStep_SetCameraPitch(-0.25f);
	xAuto.AddStep_SetCameraFOV(glm::radians(55.0f));
	xAuto.AddStep_SetCameraNear(0.1f);
	xAuto.AddStep_SetCameraFar(200.0f);
	xAuto.AddStep_SetAsMainCamera();

	xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Battle" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// Terrain rendering requires a graphics device. Headless runs still author
	// the FrontEnd and terrain-independent PlayerHome scenes above, but neither
	// mutate terrain assets nor author the Dawnmere Terrain/Flux scene.
	const bool bHeadless = Zenith_CommandLine::IsHeadless();
	ZM_TerrainBakeBatchPlan xTerrainBatch;
	if (bHeadless)
	{
		xTerrainBatch = ZM_BuildTerrainBakeBatchPlan(
			xTerrainSelection, true, 0u);
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Batch result: mode=%s, result=HEADLESS, probes=0, warmMask=0x0, queueMask=0x0, queued=0, sceneAuthoring=DEFERRED",
			ZM_TerrainBakeSelectionModeToString(xTerrainSelection.m_eMode));
	}
	else
	{
		u_int uWarmRecipeMask = 0u;
		const u_int uRecipeCount = ZM_GetTerrainAuthoringRecipeCount();
		for (u_int i = 0; i < uRecipeCount; ++i)
		{
			const ZM_TerrainAuthoringRecipe& xRecipe =
				ZM_GetTerrainAuthoringRecipe(i);
			const bool bWarm = ZM_IsTerrainBakeWarm(
				xRecipe, std::filesystem::path(GAME_ASSETS_DIR));
			if (bWarm)
			{
				uWarmRecipeMask |= 1u << i;
			}
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch probe: index=%u, set='%s', warm=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				bWarm ? "true" : "false");
		}

		xTerrainBatch = ZM_BuildTerrainBakeBatchPlan(
			xTerrainSelection, false, uWarmRecipeMask);
		u_int uQueuedRecipeCount = 0u;
		for (u_int i = 0; i < uRecipeCount; ++i)
		{
			const ZM_TerrainAuthoringRecipe& xRecipe =
				ZM_GetTerrainAuthoringRecipe(i);
			const u_int uRecipeBit = 1u << i;
			const bool bQueue =
				(xTerrainBatch.m_uQueueRecipeMask & uRecipeBit) != 0u;
			const bool bWarm = (uWarmRecipeMask & uRecipeBit) != 0u;
			const char* szDecision = bQueue ? "QUEUE" :
				(bWarm ? "SKIP_WARM" : "SKIP_FILTERED");
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch decision: index=%u, set='%s', action=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet, szDecision);
			if (!bQueue)
			{
				continue;
			}

			const bool bForce = xTerrainSelection.m_eMode !=
				ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING;
			const ZM_TERRAIN_BAKE_QUEUE_RESULT eQueueResult =
				ZM_QueueTerrainBake(xAuto, xRecipe, false, bForce);
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch queue: index=%u, set='%s', result=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				ZM_TerrainBakeQueueResultToString(eQueueResult));
			if (eQueueResult == ZM_TERRAIN_BAKE_QUEUED)
			{
				++uQueuedRecipeCount;
				continue;
			}
			if (eQueueResult == ZM_TERRAIN_BAKE_WARM && !bForce)
			{
				// A cold-to-warm race is harmless, but the immutable pre-scan
				// plan still defers scene authoring until the next boot.
				continue;
			}

			// Preparation may fail after earlier recipes appended actions.
			// Reset makes this boot all-or-nothing: no partial batch executes.
			xAuto.Reset();
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch aborted: index=%u, set='%s', result=%s; automation reset",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				ZM_TerrainBakeQueueResultToString(eQueueResult));
			Zenith_Assert(false,
				"Terrain bake batch preparation failed for %s",
				xRecipe.m_pxWorldSpec->m_szTerrainSet);
			return;
		}

		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Batch result: mode=%s, result=%s, probes=%u, warmMask=0x%X, queueMask=0x%X, queued=%u, sceneAuthoring=%s",
			ZM_TerrainBakeSelectionModeToString(xTerrainSelection.m_eMode),
			xTerrainBatch.m_uQueueRecipeMask == 0u ? "NO_QUEUE" : "QUEUED",
			uRecipeCount, xTerrainBatch.m_uWarmRecipeMask,
			xTerrainBatch.m_uQueueRecipeMask, uQueuedRecipeCount,
			xTerrainBatch.m_bAuthorDawnmereScene ?
				"AUTHOR_DAWNMERE" : "DEFERRED");
	}

	// A queued cold/forced batch completes this boot. Author Dawnmere only when
	// the windowed pre-scan found every registered terrain warm and queued none.
	// Thornacre and Route1 remain measurement-only recipes in this milestone.
	if (xTerrainBatch.m_bAuthorDawnmereScene)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		const ZM_TerrainPreviewCameraSpec& xCamera = xRecipe.m_xPreviewCamera;
		const std::string strSplatmapPath = std::string("game:Terrain/") +
			xRecipe.m_pxWorldSpec->m_szTerrainSet + "/Splatmap_RGBA" ZENITH_TEXTURE_EXT;
		const Zenith_Maths::Vector3 xTownCenterFeet(512.0f, 25.98577f, 480.0f);
		const Zenith_Maths::Vector3 xPlayerScale(0.8f, 1.8f, 0.8f);
		const float fPlayerCapsuleHalfExtent =
			ZM_PlayerController::CalculateCapsuleHalfExtent(xPlayerScale);
		const Zenith_Maths::Vector3 xPlayerCenter =
			xTownCenterFeet + Zenith_Maths::Vector3(
				0.0f, fPlayerCapsuleHalfExtent, 0.0f);

		xAuto.AddStep_CreateScene("Dawnmere");
		xAuto.AddStep_CreateEntity("DawnmereTerrain");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_AddComponent("Terrain");
		xAuto.AddStep_TerrainSetAssetSet(xRecipe.m_pxWorldSpec->m_szTerrainSet);
		for (int iSlot = 0; iSlot < 4; ++iSlot)
		{
			xAuto.AddStep_SetTerrainMaterial(iSlot, g_axDawnmereTerrainMaterials[iSlot].GetDirect());
		}
		xAuto.AddStep_SetTerrainSplatmapPath(strSplatmapPath.c_str());
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_TerrainGrass");

		// Spawn markers are feet/surface anchors. Runtime warps and this authored
		// preview placement share the controller's scale-derived capsule extent.
		xAuto.AddStep_CreateEntity("TownCenterSpawn");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xTownCenterFeet.x, xTownCenterFeet.y, xTownCenterFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureTownCenterSpawnPoint);

		// The player and camera are Dawnmere-owned. SINGLE scene loads therefore
		// replace both entities instead of carrying movement/camera state between
		// scenes. TownCenter is the exact sampled terrain surface; adding the
		// scale-derived 0.9 m capsule half-extent produces the authored centre.
		xAuto.AddStep_CreateEntity("Player");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xPlayerCenter.x, xPlayerCenter.y, xPlayerCenter.z);
		xAuto.AddStep_SetTransformScale(
			xPlayerScale.x, xPlayerScale.y, xPlayerScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_PlayerController");

		// Replaceable outdoor Home blockout. The shell's front face meets the
		// sampled doorway at z=476; the sensor sits in front of that solid face
		// so a returning player overlaps it before physical contact.
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeShell",
			{ 384.0f, 27.440985f, 456.0f }, { 16.0f, 6.0f, 40.0f });
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeDoorLeft",
			{ 382.0f, 27.661484f, 476.0f }, { 1.0f, 3.0f, 0.5f });
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeDoorRight",
			{ 386.0f, 27.661484f, 476.0f }, { 1.0f, 3.0f, 0.5f });
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeDoorLintel",
			{ 384.0f, 29.411484f, 476.0f }, { 5.0f, 0.5f, 0.5f });

		xAuto.AddStep_CreateEntity("FromHomeSpawn");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(384.0f, 26.590313f, 482.0f);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureFromHomeSpawnPoint);

		xAuto.AddStep_CreateEntity("HomeDoorTrigger");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(384.0f, 27.161484f, 476.0f);
		xAuto.AddStep_SetTransformScale(3.0f, 2.0f, 2.0f);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_WarpTrigger");
		xAuto.AddStep_Custom(&ZM_ConfigureHomeDoorTrigger);

		// ---- The authored Dawnmere NPCs (S6 item 3; SC8 added the patrol, and ----
		// ---- S7 item 2 SC1 added the story-gated warden)                    ----
		//
		// Bodies share the PLAYER'S scale, so an NPC's AABB half-height IS
		// fPlayerCapsuleHalfExtent and every NPC centre sits at exactly the player's
		// authored centre height.
		//
		// ★ HEIGHT IS AN ASSUMPTION, NOT A GUARANTEE. Every NPC reuses the ONE feet
		// height sampled at the town centre (512, 480); this authoring cannot sample
		// the terrain at each NPC's own XZ, and the rest of this file DOES author
		// per-location measured heights (see the Home block's three distinct values).
		// The picker's band is |NPC.y - player.y|, and once the capsule settles the
		// player's y is terrain(x,z) + halfExtent, so the band reduces to the TERRAIN
		// DELTA between the spawn and each NPC. Dawnmere's neighbourhood is gently
		// rolling (the authored anchors differ by ~0.6 m over 128 m), so the +/-2 m
		// band holds comfortably -- but if a regenerated heightmap ever exceeded it,
		// that NPC becomes permanently un-talkable. The villager is covered by
		// ZM_NpcTalk_Test; the clerk and caretaker are NOT yet, so treat a mute one as
		// a height check first. (S9 should author sampled per-NPC feet.)
		//
		// SEPARATION is deliberately enormous -- the closest PAIR is 16.1 m, against
		// a 2.9 m effective reach (2.5 global + 0.4 per-NPC). The picker resolves the
		// NEAREST FACED candidate, so two NPCs within reach of each other would make
		// "which NPC answered?" a function of sub-metre walk error; at 16 m the
		// answer cannot be ambiguous, and the walk-up test can assert the winner BY
		// ENTITY ID. Exact distances are derived at the coordinates below.
		//
		// The VILLAGER is the walk-up target and sits straight +Z of the spawn on
		// purpose: +Z is the one movement axis with existing evidence
		// (ZM_DawnmerePlayerCamera_Test already proves held-W moves the yaw-zero
		// player +Z), so the walk needs no unproven basis assumption.
		//
		// ★★ THE OTHER TWO MUST STAY OFF z = 480. A solid STATIC AABB on that line
		// WEDGES A DIFFERENT, ALREADY-GREEN TEST: ZM_PlayerHomeRoundTrip_Test drives
		// the player from the TownCenter spawn (512, 480) to xDoorStaging
		// (384, 0, 480) with DriveTowardXZ, which has NO obstacle avoidance -- |dz|
		// is inside its 0.08 dead zone, so it holds ONLY 'A' and runs pure -X along
		// z = 480. An NPC box there stops the capsule head-on (the 1.8 m body is far
		// above the 0.40 m step assist), the staging tolerance is never met, and that
		// test dies at its frame cap with a timeout that names distance, not the NPC.
		// So both flank NPCs are pushed to z + 18, keeping 18 m of clearance from the
		// Home corridor while staying 14 m off the x = 512 spawn-to-villager corridor
		// and well clear of the Home shell (x 376..392, z 436..476).
		// A scene-placement change can regress a suite it never mentions -- check the
		// existing traversal routes before moving anything in this block.
		//
		const Zenith_Maths::Vector3 xNpcScale = xPlayerScale;
		const Zenith_Maths::Vector3 xVillagerCenter(
			xTownCenterFeet.x, xPlayerCenter.y, xTownCenterFeet.z + 10.0f);
		// z + 18 keeps both off the z = 480 Home-traversal corridor (see above).
		// Separations against the 2.9 m effective reach (2.5 global + 0.4 authored):
		//   villager <-> clerk      = sqrt(14^2 + 8^2) = 16.1 m
		//   villager <-> caretaker  = sqrt(14^2 + 8^2) = 16.1 m
		//   clerk    <-> caretaker  = 28.0 m
		//   spawn    <-> either     = sqrt(14^2 + 18^2) = 22.8 m
		// The closest pair is 5.5x reach, so the nearest-faced-candidate picker can
		// never confuse two of them and the walk-up test can assert the winner BY
		// ENTITY ID; and neither flank NPC is reachable from spawn, which keeps the
		// test's out-of-range negative unambiguous.
		const Zenith_Maths::Vector3 xClerkCenter(
			xTownCenterFeet.x + 14.0f, xPlayerCenter.y, xTownCenterFeet.z + 18.0f);
		const Zenith_Maths::Vector3 xCaretakerCenter(
			xTownCenterFeet.x - 14.0f, xPlayerCenter.y, xTownCenterFeet.z + 18.0f);
		ZM_QueueDawnmereNpc(xAuto, "Npc_Villager",
			xVillagerCenter, xNpcScale, &ZM_ConfigureVillagerNpc);
		ZM_QueueDawnmereNpc(xAuto, "Npc_TradePostClerk",
			xClerkCenter, xNpcScale, &ZM_ConfigureTradePostClerkNpc);
		ZM_QueueDawnmereNpc(xAuto, "Npc_Caretaker",
			xCaretakerCenter, xNpcScale, &ZM_ConfigureCaretakerNpc);
		// S7 item 2 SC1: the story-gated warden. He stands on the authored HOME
		// WALKWAY, not on the north road: (478, 498) is ~1.1 m off the Home path
		// centreline and ~36.8 m from the nearest point of the Route polyline
		// (ZM_TerrainAuthoring.cpp:36-49), so his lines are written as a lane warden
		// rather than a road-blocker. The position itself is derived under exactly the
		// constraints stated above, NOT eyeballed:
		//   * z + 18 is the SAME clearance the two flank NPCs use, so the warden is
		//     18 m off the z = 480 Home traversal corridor that
		//     ZM_PlayerHomeRoundTrip_Test drives blind along. Anything nearer would
		//     re-open the wedging hazard the block above is written to prevent.
		//   * x - 34 keeps it 34 m off the x = 512 spawn-to-villager corridor.
		//   * Separations from the existing roster, against the same 2.9 m effective
		//     reach: caretaker (498, 498) = 20.0 m (the new closest pair, still 6.9x
		//     reach and wider than the existing 16.1 m minimum, so the "closest pair"
		//     figure quoted at fZM_NPC_AUTHORED_RADIUS is unchanged); villager
		//     (512, 490) = sqrt(34^2 + 8^2) = 34.9 m; clerk (526, 498) = 48.0 m;
		//     wanderer patrol (540, 476..484) = 63.6 m at its nearest endpoint;
		//     TownCenter spawn = sqrt(34^2 + 18^2) = 38.5 m, so the warden is not
		//     reachable from spawn and the existing out-of-range negative stays clean.
		//   * The Home shell (x 376..392, z 436..476) lies WEST OF the warden, who
		//     stands at (478, 498): its east face (x = 392) is 86 m west of him, and
		//     its north face (z = 476) 22 m south. No overlap on either axis.
		// Height reuses xPlayerCenter.y like every other stationary NPC -- the same
		// single sampled town-centre feet height, with the caveat block above.
		// ★ When a later stage authors a real Route 1, a warden who is meant to BLOCK
		// the road belongs on the Route polyline itself. Re-place him there and
		// re-derive every separation above from scratch -- none of these figures carry
		// over -- and rewrite his lines in ZM_NpcData.cpp to match the new ground.
		const Zenith_Maths::Vector3 xRouteWardenCenter(
			xTownCenterFeet.x - 34.0f, xPlayerCenter.y, xTownCenterFeet.z + 18.0f);
		ZM_QueueDawnmereNpc(xAuto, "Npc_Warden",
			xRouteWardenCenter, xNpcScale, &ZM_ConfigureRouteWardenNpc);
		// SC8: the fourth row is a deterministic two-point patrol. Both endpoints are
		// 28 m east of the TownCenter spawn and outside the z=480 Home corridor's
		// x<=512 run; the nearest stationary NPC (the clerk) remains >19 m away.
		// Reusing xPlayerCenter.y directly starts this capsule about 0.414 m inside
		// the higher local terrain mesh. One existing capsule half-extent of clearance
		// authors it safely above the surface so gravity settles it from the front side.
		const Zenith_Maths::Vector3 xWandererCenter(
			540.0f, xPlayerCenter.y + fPlayerCapsuleHalfExtent, 476.0f);
		ZM_QueueDawnmereWanderer(xAuto, xWandererCenter, xNpcScale);

		xAuto.AddStep_CreateEntity("DawnmerePreviewCamera");
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(xCamera.m_xPosition.m_fX, xCamera.m_xPosition.m_fY, xCamera.m_xPosition.m_fZ);
		xAuto.AddStep_SetCameraYaw(0.0f);
		xAuto.AddStep_SetCameraPitch(xCamera.m_fPitch);
		xAuto.AddStep_SetCameraFOV(glm::radians(xCamera.m_fFovDegrees));
		xAuto.AddStep_SetCameraNear(xCamera.m_fNearPlane);
		xAuto.AddStep_SetCameraFar(xCamera.m_fFarPlane);
		xAuto.AddStep_AddComponent("ZM_FollowCamera");
		xAuto.AddStep_SetAsMainCamera();
		xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
		xAuto.AddStep_UnloadScene();
	}

	xAuto.AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Battle" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(2, GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(40, GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
