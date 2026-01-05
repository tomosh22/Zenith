#pragma once

#include "AssetHandling/Zenith_DataAsset.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

/**
 * Sokoban_Config - DataAsset for Sokoban game configuration
 *
 * This demonstrates the DataAsset system.
 * Game designers can create .zdata files with different configurations for
 * difficulty levels, visual tweaks, etc.
 *
 * Usage:
 *   // In Sokoban_Behaviour::OnAwake():
 *   m_pxConfig = Zenith_DataAssetManager::LoadDataAsset<Sokoban_Config>("Assets/SokobanConfig.zdata");
 *
 *   // Or create programmatically:
 *   Sokoban_Config* pxConfig = Zenith_DataAssetManager::CreateDataAsset<Sokoban_Config>();
 *   pxConfig->m_uMinGridSize = 10;
 *   Zenith_DataAssetManager::SaveDataAsset(pxConfig, "Assets/HardMode.zdata");
 */
class Sokoban_Config : public Zenith_DataAsset
{
public:
	ZENITH_DATA_ASSET_TYPE_NAME(Sokoban_Config)

	// Grid generation
	uint32_t m_uMinGridSize = 8;
	uint32_t m_uMaxGridSize = 16;
	uint32_t m_uMinBoxes = 2;
	uint32_t m_uMaxBoxes = 5;

	// Animation
	float m_fAnimationDuration = 0.1f;

	// Visual settings
	float m_fTileScale = 0.9f;
	float m_fFloorHeight = 0.1f;
	float m_fWallHeight = 0.8f;
	float m_fBoxHeight = 0.5f;
	float m_fPlayerHeight = 0.5f;

	// Solver settings
	uint32_t m_uMinMovesSolution = 5;
	uint32_t m_uMaxSolverStates = 100000;

	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		// Version for future compatibility
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// Grid settings
		xStream << m_uMinGridSize;
		xStream << m_uMaxGridSize;
		xStream << m_uMinBoxes;
		xStream << m_uMaxBoxes;

		// Animation
		xStream << m_fAnimationDuration;

		// Visual settings
		xStream << m_fTileScale;
		xStream << m_fFloorHeight;
		xStream << m_fWallHeight;
		xStream << m_fBoxHeight;
		xStream << m_fPlayerHeight;

		// Solver settings
		xStream << m_uMinMovesSolution;
		xStream << m_uMaxSolverStates;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			// Grid settings
			xStream >> m_uMinGridSize;
			xStream >> m_uMaxGridSize;
			xStream >> m_uMinBoxes;
			xStream >> m_uMaxBoxes;

			// Animation
			xStream >> m_fAnimationDuration;

			// Visual settings
			xStream >> m_fTileScale;
			xStream >> m_fFloorHeight;
			xStream >> m_fWallHeight;
			xStream >> m_fBoxHeight;
			xStream >> m_fPlayerHeight;

			// Solver settings
			xStream >> m_uMinMovesSolution;
			xStream >> m_uMaxSolverStates;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override
	{
		ImGui::Text("Sokoban Configuration");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Grid Generation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragScalar("Min Grid Size", ImGuiDataType_U32, &m_uMinGridSize, 1.0f);
			ImGui::DragScalar("Max Grid Size", ImGuiDataType_U32, &m_uMaxGridSize, 1.0f);
			ImGui::DragScalar("Min Boxes", ImGuiDataType_U32, &m_uMinBoxes, 1.0f);
			ImGui::DragScalar("Max Boxes", ImGuiDataType_U32, &m_uMaxBoxes, 1.0f);
		}

		if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Animation Duration", &m_fAnimationDuration, 0.01f, 0.01f, 1.0f);
		}

		if (ImGui::CollapsingHeader("Visual Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Tile Scale", &m_fTileScale, 0.01f, 0.1f, 1.0f);
			ImGui::DragFloat("Floor Height", &m_fFloorHeight, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("Wall Height", &m_fWallHeight, 0.01f, 0.1f, 2.0f);
			ImGui::DragFloat("Box Height", &m_fBoxHeight, 0.01f, 0.1f, 1.0f);
			ImGui::DragFloat("Player Height", &m_fPlayerHeight, 0.01f, 0.1f, 1.0f);
		}

		if (ImGui::CollapsingHeader("Solver Settings"))
		{
			ImGui::DragScalar("Min Moves Solution", ImGuiDataType_U32, &m_uMinMovesSolution, 1.0f);
			ImGui::DragScalar("Max Solver States", ImGuiDataType_U32, &m_uMaxSolverStates, 1000.0f);
		}
	}
#endif
};

// Register the DataAsset type (call once at startup)
inline void RegisterSokobanDataAssets()
{
	Zenith_DataAssetManager::RegisterDataAssetType<Sokoban_Config>();
}
