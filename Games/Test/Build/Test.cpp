#include "Zenith.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "Test/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UI.h"

void Project_RegisterScriptBehaviours()
{
	PlayerController_Behaviour::RegisterBehaviour();
	HookesLaw_Behaviour::RegisterBehaviour();
	RotationBehaviour_Behaviour::RegisterBehaviour();
}

void Project_LoadInitialScene()
{
	Zenith_Scene::GetCurrentScene().LoadFromFile(ASSETS_ROOT"Scenes/test_scene.zscen");

	// Create RPG HUD Entity
	Zenith_Entity xHUDEntity;
	xHUDEntity.Initialise(&Zenith_Scene::GetCurrentScene(), "RPG_HUD");

	Zenith_UIComponent& xUI = xHUDEntity.AddComponent<Zenith_UIComponent>();

	// ========== Health Bar ==========
	// Background (dark red)
	Zenith_UI::Zenith_UIRect* pxHealthBG = xUI.CreateRect("HealthBar_BG");
	pxHealthBG->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
	pxHealthBG->SetPosition(20, -60);
	pxHealthBG->SetSize(250, 30);
	pxHealthBG->SetColor({ 0.3f, 0.0f, 0.0f, 0.8f });
	pxHealthBG->SetBorderColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	pxHealthBG->SetBorderThickness(2.0f);

	// Foreground (bright red, with fill amount)
	Zenith_UI::Zenith_UIRect* pxHealthFill = xUI.CreateRect("HealthBar_Fill");
	pxHealthFill->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
	pxHealthFill->SetPosition(22, -62);
	pxHealthFill->SetSize(246, 26);
	pxHealthFill->SetColor({ 0.9f, 0.1f, 0.1f, 1.0f });
	pxHealthFill->SetFillAmount(1.0f);
	pxHealthFill->SetFillDirection(Zenith_UI::FillDirection::LeftToRight);

	// Health label
	Zenith_UI::Zenith_UIText* pxHealthLabel = xUI.CreateText("HealthLabel", "HP");
	pxHealthLabel->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
	pxHealthLabel->SetPosition(25, -85);
	pxHealthLabel->SetFontSize(18);
	pxHealthLabel->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	// ========== Compass (placeholder) ==========
	Zenith_UI::Zenith_UIRect* pxCompassBG = xUI.CreateRect("Compass_BG");
	pxCompassBG->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopCenter);
	pxCompassBG->SetPosition(0, 20);
	pxCompassBG->SetSize(300, 40);
	pxCompassBG->SetColor({ 0.1f, 0.1f, 0.1f, 0.7f });
	pxCompassBG->SetBorderColor({ 0.4f, 0.4f, 0.4f, 1.0f });
	pxCompassBG->SetBorderThickness(2.0f);

	// Compass direction text
	Zenith_UI::Zenith_UIText* pxCompassText = xUI.CreateText("CompassText", "N");
	pxCompassText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopCenter);
	pxCompassText->SetPosition(0, 28);
	pxCompassText->SetFontSize(24);
	pxCompassText->SetColor({ 1.0f, 0.9f, 0.6f, 1.0f });
	pxCompassText->SetAlignment(Zenith_UI::TextAlignment::Center);

	// ========== Inventory Hotbar ==========
	constexpr int INVENTORY_SLOTS = 6;
	constexpr float SLOT_SIZE = 64.0f;
	constexpr float SLOT_SPACING = 8.0f;
	constexpr float HOTBAR_WIDTH = INVENTORY_SLOTS * SLOT_SIZE + (INVENTORY_SLOTS - 1) * SLOT_SPACING;
	constexpr float HOTBAR_START_X = -HOTBAR_WIDTH / 2.0f;

	for (int i = 0; i < INVENTORY_SLOTS; i++)
	{
		std::string strSlotName = "InventorySlot_" + std::to_string(i);
		float fSlotX = HOTBAR_START_X + i * (SLOT_SIZE + SLOT_SPACING);

		// Slot background
		Zenith_UI::Zenith_UIRect* pxSlotBG = xUI.CreateRect(strSlotName + "_BG");
		pxSlotBG->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomCenter);
		pxSlotBG->SetPosition(fSlotX + SLOT_SIZE / 2.0f, -20);
		pxSlotBG->SetSize(SLOT_SIZE, SLOT_SIZE);
		pxSlotBG->SetColor({ 0.15f, 0.15f, 0.2f, 0.85f });
		pxSlotBG->SetBorderColor({ 0.4f, 0.4f, 0.5f, 1.0f });
		pxSlotBG->SetBorderThickness(2.0f);

		// Selected slot has glow
		if (i == 0)
		{
			pxSlotBG->SetGlowEnabled(true);
			pxSlotBG->SetGlowColor({ 1.0f, 0.8f, 0.2f, 0.6f });
		}

		// Slot number
		Zenith_UI::Zenith_UIText* pxSlotNum = xUI.CreateText(strSlotName + "_Num", std::to_string(i + 1));
		pxSlotNum->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomCenter);
		pxSlotNum->SetPosition(fSlotX + 8, -70);
		pxSlotNum->SetFontSize(14);
		pxSlotNum->SetColor({ 0.7f, 0.7f, 0.7f, 0.8f });
	}
}