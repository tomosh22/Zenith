#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

class Zenith_UIComponent;
namespace Zenith_UI { class Zenith_UIRect; class Zenith_UIText; }

class PlayerController_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	// Serialization support - type name and factory registration
	ZENITH_BEHAVIOUR_TYPE_NAME(PlayerController_Behaviour)

	static constexpr double s_dMoveSpeed = 20;
	static constexpr float s_fMaxHealth = 100.0f;
	static constexpr int s_iInventorySlots = 6;

	enum CameraType
	{
		CAMERA_TYPE_PERSPECTIVE,
		CAMERA_TYPE_ORTHOGRAPHIC,
		CAMERA_TYPE_MAX
	};
	PlayerController_Behaviour() = delete;
	PlayerController_Behaviour(Zenith_Entity& xParentEntity);
	~PlayerController_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnCreate() ZENITH_FINAL override;

	void Shoot();

	// Health API
	void SetHealth(float fHealth);
	float GetHealth() const { return m_fHealth; }
	void TakeDamage(float fDamage);
	void Heal(float fAmount);

	// Inventory API
	void SetSelectedSlot(int iSlot);
	int GetSelectedSlot() const { return m_iSelectedSlot; }

	// Editor UI for behavior-specific properties
	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Checkbox("Fly Camera Enabled", &m_bFlyCamEnabled);
		ImGui::Text("Move Speed: %.1f", s_dMoveSpeed);
		ImGui::Separator();
		ImGui::Text("Health: %.1f / %.1f", m_fHealth, s_fMaxHealth);
		if (ImGui::SliderFloat("##Health", &m_fHealth, 0.0f, s_fMaxHealth))
		{
			UpdateHealthUI();
		}
		ImGui::Text("Selected Slot: %d", m_iSelectedSlot + 1);
		if (ImGui::SliderInt("##Slot", &m_iSelectedSlot, 0, s_iInventorySlots - 1))
		{
			UpdateInventoryUI();
		}
#endif
	}

private:
	void FindHUDElements();
	void UpdateHealthUI();
	void UpdateCompassUI();
	void UpdateInventoryUI();

	bool m_bFlyCamEnabled = false;
	Zenith_Entity m_xParentEntity;

	// Gameplay state
	float m_fHealth = s_fMaxHealth;
	int m_iSelectedSlot = 0;

	// Cached UI element pointers (found at runtime)
	Zenith_UI::Zenith_UIRect* m_pxHealthFill = nullptr;
	Zenith_UI::Zenith_UIText* m_pxCompassText = nullptr;
	Zenith_UI::Zenith_UIRect* m_apxInventorySlots[s_iInventorySlots] = {};
	bool m_bUIInitialized = false;
};
