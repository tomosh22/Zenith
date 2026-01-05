#pragma once
/**
 * Survival_Config.h - DataAsset for Survival game configuration
 *
 * Demonstrates the DataAsset system for game configuration.
 * Designers can create .zdata files with different configurations
 * for difficulty levels, spawn rates, crafting times, etc.
 *
 * Usage:
 *   m_pxConfig = Zenith_DataAssetManager::LoadDataAsset<Survival_Config>("Assets/SurvivalConfig.zdata");
 */

#include "AssetHandling/Zenith_DataAsset.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

class Survival_Config : public Zenith_DataAsset
{
public:
	ZENITH_DATA_ASSET_TYPE_NAME(Survival_Config)

	// World generation
	uint32_t m_uTreeCount = 15;
	uint32_t m_uRockCount = 10;
	uint32_t m_uBerryBushCount = 8;
	float m_fWorldRadius = 50.0f;
	float m_fResourceMinDistance = 5.0f;

	// Player settings
	float m_fPlayerMoveSpeed = 8.0f;
	float m_fInteractionRange = 3.0f;

	// Resource node settings
	uint32_t m_uTreeHitsToChop = 3;
	uint32_t m_uRockHitsToMine = 4;
	uint32_t m_uBerryHitsToPick = 1;
	float m_fResourceRespawnTime = 30.0f;

	// Yield amounts
	uint32_t m_uWoodPerTree = 3;
	uint32_t m_uStonePerRock = 2;
	uint32_t m_uBerriesPerBush = 5;

	// Crafting settings
	float m_fCraftingTime = 2.0f;
	uint32_t m_uAxeWoodCost = 3;
	uint32_t m_uAxeStoneCost = 2;
	uint32_t m_uPickaxeWoodCost = 2;
	uint32_t m_uPickaxeStoneCost = 3;

	// Tool bonuses (multipliers)
	float m_fAxeWoodBonus = 2.0f;
	float m_fPickaxeStoneBonus = 2.0f;

	// Camera settings
	float m_fCameraDistance = 15.0f;
	float m_fCameraHeight = 10.0f;
	float m_fCameraSmoothSpeed = 5.0f;

	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// World generation
		xStream << m_uTreeCount;
		xStream << m_uRockCount;
		xStream << m_uBerryBushCount;
		xStream << m_fWorldRadius;
		xStream << m_fResourceMinDistance;

		// Player settings
		xStream << m_fPlayerMoveSpeed;
		xStream << m_fInteractionRange;

		// Resource node settings
		xStream << m_uTreeHitsToChop;
		xStream << m_uRockHitsToMine;
		xStream << m_uBerryHitsToPick;
		xStream << m_fResourceRespawnTime;

		// Yield amounts
		xStream << m_uWoodPerTree;
		xStream << m_uStonePerRock;
		xStream << m_uBerriesPerBush;

		// Crafting settings
		xStream << m_fCraftingTime;
		xStream << m_uAxeWoodCost;
		xStream << m_uAxeStoneCost;
		xStream << m_uPickaxeWoodCost;
		xStream << m_uPickaxeStoneCost;

		// Tool bonuses
		xStream << m_fAxeWoodBonus;
		xStream << m_fPickaxeStoneBonus;

		// Camera settings
		xStream << m_fCameraDistance;
		xStream << m_fCameraHeight;
		xStream << m_fCameraSmoothSpeed;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			// World generation
			xStream >> m_uTreeCount;
			xStream >> m_uRockCount;
			xStream >> m_uBerryBushCount;
			xStream >> m_fWorldRadius;
			xStream >> m_fResourceMinDistance;

			// Player settings
			xStream >> m_fPlayerMoveSpeed;
			xStream >> m_fInteractionRange;

			// Resource node settings
			xStream >> m_uTreeHitsToChop;
			xStream >> m_uRockHitsToMine;
			xStream >> m_uBerryHitsToPick;
			xStream >> m_fResourceRespawnTime;

			// Yield amounts
			xStream >> m_uWoodPerTree;
			xStream >> m_uStonePerRock;
			xStream >> m_uBerriesPerBush;

			// Crafting settings
			xStream >> m_fCraftingTime;
			xStream >> m_uAxeWoodCost;
			xStream >> m_uAxeStoneCost;
			xStream >> m_uPickaxeWoodCost;
			xStream >> m_uPickaxeStoneCost;

			// Tool bonuses
			xStream >> m_fAxeWoodBonus;
			xStream >> m_fPickaxeStoneBonus;

			// Camera settings
			xStream >> m_fCameraDistance;
			xStream >> m_fCameraHeight;
			xStream >> m_fCameraSmoothSpeed;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override
	{
		ImGui::Text("Survival Game Configuration");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("World Generation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragScalar("Tree Count", ImGuiDataType_U32, &m_uTreeCount, 1.0f);
			ImGui::DragScalar("Rock Count", ImGuiDataType_U32, &m_uRockCount, 1.0f);
			ImGui::DragScalar("Berry Bush Count", ImGuiDataType_U32, &m_uBerryBushCount, 1.0f);
			ImGui::DragFloat("World Radius", &m_fWorldRadius, 1.0f, 20.0f, 200.0f);
			ImGui::DragFloat("Resource Min Distance", &m_fResourceMinDistance, 0.5f, 1.0f, 20.0f);
		}

		if (ImGui::CollapsingHeader("Player Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Move Speed", &m_fPlayerMoveSpeed, 0.5f, 1.0f, 20.0f);
			ImGui::DragFloat("Interaction Range", &m_fInteractionRange, 0.5f, 1.0f, 10.0f);
		}

		if (ImGui::CollapsingHeader("Resource Nodes"))
		{
			ImGui::DragScalar("Tree Hits to Chop", ImGuiDataType_U32, &m_uTreeHitsToChop, 1.0f);
			ImGui::DragScalar("Rock Hits to Mine", ImGuiDataType_U32, &m_uRockHitsToMine, 1.0f);
			ImGui::DragScalar("Berry Hits to Pick", ImGuiDataType_U32, &m_uBerryHitsToPick, 1.0f);
			ImGui::DragFloat("Respawn Time (sec)", &m_fResourceRespawnTime, 1.0f, 5.0f, 120.0f);
		}

		if (ImGui::CollapsingHeader("Yields"))
		{
			ImGui::DragScalar("Wood per Tree", ImGuiDataType_U32, &m_uWoodPerTree, 1.0f);
			ImGui::DragScalar("Stone per Rock", ImGuiDataType_U32, &m_uStonePerRock, 1.0f);
			ImGui::DragScalar("Berries per Bush", ImGuiDataType_U32, &m_uBerriesPerBush, 1.0f);
		}

		if (ImGui::CollapsingHeader("Crafting"))
		{
			ImGui::DragFloat("Crafting Time (sec)", &m_fCraftingTime, 0.1f, 0.5f, 10.0f);
			ImGui::Separator();
			ImGui::Text("Axe Recipe:");
			ImGui::DragScalar("  Wood Cost", ImGuiDataType_U32, &m_uAxeWoodCost, 1.0f);
			ImGui::DragScalar("  Stone Cost", ImGuiDataType_U32, &m_uAxeStoneCost, 1.0f);
			ImGui::Separator();
			ImGui::Text("Pickaxe Recipe:");
			ImGui::DragScalar("  Wood Cost", ImGuiDataType_U32, &m_uPickaxeWoodCost, 1.0f);
			ImGui::DragScalar("  Stone Cost", ImGuiDataType_U32, &m_uPickaxeStoneCost, 1.0f);
		}

		if (ImGui::CollapsingHeader("Tool Bonuses"))
		{
			ImGui::DragFloat("Axe Wood Bonus", &m_fAxeWoodBonus, 0.1f, 1.0f, 5.0f);
			ImGui::DragFloat("Pickaxe Stone Bonus", &m_fPickaxeStoneBonus, 0.1f, 1.0f, 5.0f);
		}

		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::DragFloat("Camera Distance", &m_fCameraDistance, 0.5f, 5.0f, 30.0f);
			ImGui::DragFloat("Camera Height", &m_fCameraHeight, 0.5f, 3.0f, 20.0f);
			ImGui::DragFloat("Camera Smooth Speed", &m_fCameraSmoothSpeed, 0.5f, 1.0f, 20.0f);
		}
	}
#endif
};

// Register the DataAsset type (call once at startup)
inline void RegisterSurvivalDataAssets()
{
	Zenith_DataAssetManager::RegisterDataAssetType<Survival_Config>();
}
