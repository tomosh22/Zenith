#pragma once
/**
 * Sokoban_Rendering.h - 3D visualization module
 *
 * Demonstrates:
 * - Zenith_Scene::Instantiate() for prefab-based entity creation
 * - Zenith_TransformComponent for position/scale
 * - Zenith_ModelComponent for mesh rendering
 * - Multi-scene architecture (entities in puzzle scene, camera in persistent scene)
 * - FindMainCameraAcrossScenes for cross-scene camera access
 *
 * Key concepts:
 * - Prefabs as entity templates
 * - Transform must be set BEFORE adding physics components
 * - Scene transitions clean up entities automatically (no manual Destroy3DLevel)
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Prefab/Zenith_Prefab.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Maths/Zenith_Maths.h"
#include "Sokoban_GridLogic.h"  // For SokobanTileType enum
#include <vector>
#include <cmath>

// Forward declarations
class Zenith_Prefab;

// Visual constants
static constexpr float s_fTileScale = 0.9f;      // Gap between tiles
static constexpr float s_fFloorHeight = 0.1f;    // Floor tile height
static constexpr float s_fWallHeight = 0.8f;     // Wall tile height
static constexpr float s_fBoxHeight = 0.5f;      // Box height
static constexpr float s_fPlayerHeight = 0.5f;   // Player height

/**
 * Sokoban_Renderer - Manages 3D visualization of the Sokoban level
 *
 * Responsibilities:
 * - Create 3D entities for tiles, boxes, and player in the puzzle scene
 * - Update entity positions during animation
 * - Position camera (in persistent scene) to fit the level
 */
class Sokoban_Renderer
{
public:
	static Zenith_Maths::Vector3 GridToWorld(
		float fGridX, float fGridY, float fHeight,
		uint32_t uGridWidth, uint32_t uGridHeight)
	{
		float fWorldX = fGridX - static_cast<float>(uGridWidth) * 0.5f;
		float fWorldZ = static_cast<float>(uGridHeight) * 0.5f - fGridY;
		return {fWorldX, fHeight * 0.5f, fWorldZ};
	}

	static Zenith_MaterialAsset* GetMaterialForTile(
		const SokobanTileType* aeTiles,
		const bool* abTargets,
		uint32_t uIndex,
		Zenith_MaterialAsset* pxFloorMaterial,
		Zenith_MaterialAsset* pxWallMaterial,
		Zenith_MaterialAsset* pxTargetMaterial)
	{
		if (aeTiles[uIndex] == SOKOBAN_TILE_WALL)
			return pxWallMaterial;

		if (abTargets[uIndex])
			return pxTargetMaterial;

		return pxFloorMaterial;
	}

	static float GetTileHeight(const SokobanTileType* aeTiles, uint32_t uIndex)
	{
		if (aeTiles[uIndex] == SOKOBAN_TILE_WALL)
			return s_fWallHeight;
		return s_fFloorHeight;
	}

	/**
	 * Create3DLevel - Create all 3D entities in the specified puzzle scene
	 *
	 * @param pxSceneData  The puzzle scene to create entities in (NOT the persistent scene)
	 */
	void Create3DLevel(
		uint32_t uGridWidth,
		uint32_t uGridHeight,
		const SokobanTileType* aeTiles,
		const bool* abBoxes,
		const bool* abTargets,
		uint32_t uPlayerX,
		uint32_t uPlayerY,
		Zenith_Prefab* pxTilePrefab,
		Zenith_Prefab* pxBoxPrefab,
		Zenith_Prefab* pxPlayerPrefab,
		Flux_MeshGeometry* pxCubeGeometry,
		Zenith_MaterialAsset* pxFloorMaterial,
		Zenith_MaterialAsset* pxWallMaterial,
		Zenith_MaterialAsset* pxTargetMaterial,
		Zenith_MaterialAsset* pxBoxMaterial,
		Zenith_MaterialAsset* pxBoxOnTargetMaterial,
		Zenith_MaterialAsset* pxPlayerMaterial,
		Zenith_SceneData* pxSceneData)
	{
		ClearEntityIDs();

		m_uGridWidth = uGridWidth;
		m_uGridHeight = uGridHeight;

		// Create floor and wall tiles
		for (uint32_t uY = 0; uY < uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < uGridWidth; uX++)
			{
				uint32_t uIndex = uY * uGridWidth + uX;

				float fHeight = GetTileHeight(aeTiles, uIndex);
				Zenith_Maths::Vector3 xPos = GridToWorld(
					static_cast<float>(uX), static_cast<float>(uY), fHeight,
					uGridWidth, uGridHeight);

				Zenith_Entity xTileEntity = pxTilePrefab->Instantiate(pxSceneData, "Tile");

				Zenith_TransformComponent& xTransform = xTileEntity.GetComponent<Zenith_TransformComponent>();
				xTransform.SetPosition(xPos);
				xTransform.SetScale({s_fTileScale, fHeight, s_fTileScale});

				Zenith_ModelComponent& xModel = xTileEntity.AddComponent<Zenith_ModelComponent>();
				xModel.AddMeshEntry(
					*pxCubeGeometry,
					*GetMaterialForTile(aeTiles, abTargets, uIndex, pxFloorMaterial, pxWallMaterial, pxTargetMaterial));

				m_axTileEntityIDs.push_back(xTileEntity.GetEntityID());
			}
		}

		// Create box entities
		for (uint32_t uY = 0; uY < uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < uGridWidth; uX++)
			{
				uint32_t uIndex = uY * uGridWidth + uX;
				if (abBoxes[uIndex])
				{
					Zenith_Maths::Vector3 xPos = GridToWorld(
						static_cast<float>(uX), static_cast<float>(uY), s_fBoxHeight,
						uGridWidth, uGridHeight);
					xPos.y += s_fFloorHeight;

					Zenith_Entity xBoxEntity = pxBoxPrefab->Instantiate(pxSceneData, "Box");

					Zenith_TransformComponent& xTransform = xBoxEntity.GetComponent<Zenith_TransformComponent>();
					xTransform.SetPosition(xPos);
					xTransform.SetScale({s_fTileScale * 0.8f, s_fBoxHeight, s_fTileScale * 0.8f});

					Zenith_MaterialAsset* pxMaterial = abTargets[uIndex] ? pxBoxOnTargetMaterial : pxBoxMaterial;

					Zenith_ModelComponent& xModel = xBoxEntity.AddComponent<Zenith_ModelComponent>();
					xModel.AddMeshEntry(*pxCubeGeometry, *pxMaterial);

					m_axBoxEntityIDs.push_back(xBoxEntity.GetEntityID());
				}
			}
		}

		// Create player entity
		{
			Zenith_Maths::Vector3 xPos = GridToWorld(
				static_cast<float>(uPlayerX), static_cast<float>(uPlayerY), s_fPlayerHeight,
				uGridWidth, uGridHeight);
			xPos.y += s_fFloorHeight;

			Zenith_Entity xPlayerEntity = pxPlayerPrefab->Instantiate(pxSceneData, "Player");

			Zenith_TransformComponent& xTransform = xPlayerEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(xPos);
			xTransform.SetScale({s_fTileScale * 0.7f, s_fPlayerHeight, s_fTileScale * 0.7f});

			Zenith_ModelComponent& xModel = xPlayerEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*pxCubeGeometry, *pxPlayerMaterial);

			m_uPlayerEntityID = xPlayerEntity.GetEntityID();
		}
	}

	/**
	 * ClearEntityIDs - Reset tracked entity IDs without destroying entities.
	 * Called when the puzzle scene is unloaded (entities are cleaned up by scene unload).
	 */
	void ClearEntityIDs()
	{
		m_axTileEntityIDs.clear();
		m_axBoxEntityIDs.clear();
		m_uPlayerEntityID = INVALID_ENTITY_ID;
	}

	/**
	 * UpdatePlayerPosition - Update player entity position during animation
	 * @param pxSceneData  The puzzle scene containing the player entity
	 */
	void UpdatePlayerPosition(float fVisualX, float fVisualY, Zenith_SceneData* pxSceneData)
	{
		if (!pxSceneData) return;

		if (m_uPlayerEntityID.IsValid() && pxSceneData->EntityExists(m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = pxSceneData->GetEntity(m_uPlayerEntityID);
			if (xPlayer.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_TransformComponent& xTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
				Zenith_Maths::Vector3 xPos = GridToWorld(fVisualX, fVisualY, s_fPlayerHeight, m_uGridWidth, m_uGridHeight);
				xPos.y += s_fFloorHeight;
				xTransform.SetPosition(xPos);
			}
		}
	}

	/**
	 * UpdateBoxPositions - Update all box entity positions
	 * @param pxSceneData  The puzzle scene containing the box entities
	 */
	void UpdateBoxPositions(
		const bool* abBoxes,
		uint32_t uGridWidth,
		uint32_t uGridHeight,
		bool bBoxAnimating,
		uint32_t uAnimBoxToX,
		uint32_t uAnimBoxToY,
		float fBoxVisualX,
		float fBoxVisualY,
		Zenith_SceneData* pxSceneData)
	{
		if (!pxSceneData) return;

		size_t uBoxIdx = 0;
		for (uint32_t uY = 0; uY < uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < uGridWidth; uX++)
			{
				uint32_t uIndex = uY * uGridWidth + uX;
				if (abBoxes[uIndex] && uBoxIdx < m_axBoxEntityIDs.size())
				{
					Zenith_EntityID uBoxID = m_axBoxEntityIDs[uBoxIdx];
					if (pxSceneData->EntityExists(uBoxID))
					{
						Zenith_Entity xBox = pxSceneData->GetEntity(uBoxID);
						if (xBox.HasComponent<Zenith_TransformComponent>())
						{
							Zenith_TransformComponent& xTransform = xBox.GetComponent<Zenith_TransformComponent>();

							float fVisualX = static_cast<float>(uX);
							float fVisualY = static_cast<float>(uY);

							if (bBoxAnimating && uX == uAnimBoxToX && uY == uAnimBoxToY)
							{
								fVisualX = fBoxVisualX;
								fVisualY = fBoxVisualY;
							}

							Zenith_Maths::Vector3 xPos = GridToWorld(fVisualX, fVisualY, s_fBoxHeight, uGridWidth, uGridHeight);
							xPos.y += s_fFloorHeight;
							xTransform.SetPosition(xPos);
						}
					}
					uBoxIdx++;
				}
			}
		}
	}

	/**
	 * RepositionCamera - Adjust camera to fit the level in view
	 * Uses FindMainCameraAcrossScenes to find the camera in the persistent scene.
	 */
	void RepositionCamera(uint32_t uGridWidth, uint32_t uGridHeight)
	{
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCamera)
			return;

		float fFOV = pxCamera->GetFOV();
		float fAspectRatio = pxCamera->GetAspectRatio();

		float fGridWorldWidth = static_cast<float>(uGridWidth) * 1.2f;
		float fGridWorldHeight = static_cast<float>(uGridHeight) * 1.2f;

		float fHalfFOVTan = tan(fFOV * 0.5f);
		float fHeightForVertical = fGridWorldHeight / (2.0f * fHalfFOVTan);
		float fHeightForHorizontal = fGridWorldWidth / (2.0f * fHalfFOVTan * fAspectRatio);

		float fRequiredHeight = std::max(fHeightForVertical, fHeightForHorizontal);

		pxCamera->SetPosition(Zenith_Maths::Vector3(0.f, fRequiredHeight, 0.f));
	}

	// Accessors for entity IDs
	Zenith_EntityID GetPlayerEntityID() const { return m_uPlayerEntityID; }
	const std::vector<Zenith_EntityID>& GetBoxEntityIDs() const { return m_axBoxEntityIDs; }
	const std::vector<Zenith_EntityID>& GetTileEntityIDs() const { return m_axTileEntityIDs; }

private:
	uint32_t m_uGridWidth = 0;
	uint32_t m_uGridHeight = 0;
	std::vector<Zenith_EntityID> m_axTileEntityIDs;
	std::vector<Zenith_EntityID> m_axBoxEntityIDs;
	Zenith_EntityID m_uPlayerEntityID = INVALID_ENTITY_ID;
};
