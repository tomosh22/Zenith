#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include <string>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"
#endif

class Zenith_UIComponent;
class Zenith_Prefab;
namespace Zenith_UI { class Zenith_UIRect; class Zenith_UIText; }

class Test_PlayerControllerComponent
{
public:
	static constexpr double s_dMoveSpeed = 20;
	static constexpr float s_fMaxHealth = 100.0f;
	static constexpr int s_iInventorySlots = 6;

	enum CameraType
	{
		CAMERA_TYPE_PERSPECTIVE,
		CAMERA_TYPE_ORTHOGRAPHIC,
		CAMERA_TYPE_MAX
	};
	Test_PlayerControllerComponent() = delete;
	Test_PlayerControllerComponent(Zenith_Entity& xParentEntity);

	void OnUpdate(const float fDt);
	void OnAwake();

	void Shoot();

	// Prefab API
	void SetBulletPrefabPath(const std::string& strPath);
	const std::string& GetBulletPrefabPath() const { return m_strBulletPrefabPath; }

	// Health API
	void SetHealth(float fHealth);
	float GetHealth() const { return m_fHealth; }
	void TakeDamage(float fDamage);
	void Heal(float fAmount);

	// Inventory API
	void SetSelectedSlot(int iSlot);
	int GetSelectedSlot() const { return m_iSelectedSlot; }

	// Serialization - no persistent tunables
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		uint32_t uVersion = 0;
		xStream >> uVersion;
	}

	// Editor UI for component-specific properties
	void RenderPropertiesPanel()
	{
#ifdef ZENITH_TOOLS
		ImGui::Checkbox("Fly Camera Enabled", &m_bFlyCamEnabled);
		ImGui::Text("Move Speed: %.1f", s_dMoveSpeed);
		ImGui::Separator();

		// Bullet Prefab drag-drop
		ImGui::Text("Bullet Prefab:");
		std::string strDisplayPath = m_strBulletPrefabPath.empty() ? "(None - drag .zprfb here)" : m_strBulletPrefabPath;
		ImGui::Button(strDisplayPath.c_str(), ImVec2(250, 20));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_PREFAB))
			{
				const DragDropFilePayload* pFilePayload =
					static_cast<const DragDropFilePayload*>(pPayload->Data);
				SetBulletPrefabPath(pFilePayload->m_szFilePath);
			}
			ImGui::EndDragDropTarget();
		}
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

	// Bullet prefab (per-instance) — loaded via the registry
	std::string m_strBulletPrefabPath = GAME_ASSETS_DIR"Prefabs/Bullet" ZENITH_PREFAB_EXT;
	PrefabHandle m_xBulletPrefab;
};
