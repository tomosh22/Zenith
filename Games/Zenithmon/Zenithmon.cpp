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
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"
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

		// Root actions -- Fight / Catch / Run, a single vertical stack inside the panel
		// (top to bottom, matching the ZM_BATTLE_MENU_FIGHT/CATCH/RUN cursor order). 48px
		// row pitch, shown only in ACTION_ROOT.
		Zenith_UI::Zenith_UIButton* pxFight =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionFight");
		fnPlaceMenu(pxFight, Zenith_UI::AnchorPreset::BottomRight, -133.0f, -128.0f, 170.0f, 44.0f);
		if (pxFight != nullptr)
		{
			pxFight->SetFontSize(26.0f);
		}

		Zenith_UI::Zenith_UIButton* pxCatch =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionCatch");
		fnPlaceMenu(pxCatch, Zenith_UI::AnchorPreset::BottomRight, -133.0f, -80.0f, 170.0f, 44.0f);
		if (pxCatch != nullptr)
		{
			pxCatch->SetFontSize(26.0f);
		}

		Zenith_UI::Zenith_UIButton* pxRun =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionRun");
		fnPlaceMenu(pxRun, Zenith_UI::AnchorPreset::BottomRight, -133.0f, -32.0f, 170.0f, 44.0f);
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

	// The overworld pause menu (S6 item 2 SC1). Authors the ROOT screen's backing panel
	// + Party/Bag/Dex/Exit entries on the selected ZM_MenuRoot entity's UI component:
	// centred vertical stack, sort band 9000/9001 (BELOW WarpFade 10000 / BattleFade
	// 10001 so a fade always covers the menu), each entry focusable + navigation-wired
	// (up/down) for deterministic engine focus-nav, and ALL authored hidden --
	// ZM_UI_MenuStack shows/hides + focuses them at runtime. Element names are the
	// ZM_UI_MenuStack::sz*_NAME contract.
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
		ZM_GameStateManager::ResetRuntimeStateForTests();
		// The persistent manager's GameState survives DontDestroyOnLoad across tests;
		// re-seed the starter so a caught/levelled party cannot leak into the next test.
		ZM_GameStateManager::ResetGameStateForTests();
		ZM_SetInstantBattlesForTests(false);
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
	// component (the ROOT panel + Party/Bag/Dex/Exit entries, authored hidden by
	// ZM_ConfigureMenuRoot) + the ZM_UI_MenuStack machine. Persistent so the menu is
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
