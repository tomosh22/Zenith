#pragma once
/**
 * Sokoban_Rendering.h - 3D visualization module
 *
 * Demonstrates:
 * - Zenith_Scene::Instantiate() for prefab-based entity creation
 * - Zenith_TransformComponent for position/scale
 * - Zenith_ModelComponent for mesh rendering
 * - Dynamic entity creation and destruction
 * - Coordinate space conversion (grid to world)
 *
 * Key concepts:
 * - Prefabs as entity templates
 * - Transform must be set BEFORE adding physics components
 * - Entity lifetime management with scene queries
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Prefab/Zenith_Prefab.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
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
 * - Create 3D entities for tiles, boxes, and player
 * - Update entity positions during animation
 * - Clean up entities on level change
 * - Position camera to fit the level
 */
class Sokoban_Renderer
{
public:
	/**
	 * GridToWorld - Convert grid coordinates to world position
	 *
	 * Centers the grid at the world origin.
	 * Negates Z to match camera orientation (looking down -Y axis).
	 *
	 * @param fGridX   Grid X position (can be fractional for animation)
	 * @param fGridY   Grid Y position (can be fractional for animation)
	 * @param fHeight  Height of the object
	 * @param uGridWidth  Grid width for centering
	 * @param uGridHeight Grid height for centering
	 */
	static Zenith_Maths::Vector3 GridToWorld(
		float fGridX, float fGridY, float fHeight,
		uint32_t uGridWidth, uint32_t uGridHeight)
	{
		// Center grid at origin
		float fWorldX = fGridX - static_cast<float>(uGridWidth) * 0.5f;
		// Negate Z for camera orientation (positive Z = toward camera)
		float fWorldZ = static_cast<float>(uGridHeight) * 0.5f - fGridY;
		// Y is up, height is centered at half
		return {fWorldX, fHeight * 0.5f, fWorldZ};
	}

	/**
	 * GetMaterialForTile - Select material based on tile state
	 */
	static Flux_MaterialAsset* GetMaterialForTile(
		const SokobanTileType* aeTiles,
		const bool* abTargets,
		uint32_t uIndex,
		Flux_MaterialAsset* pxFloorMaterial,
		Flux_MaterialAsset* pxWallMaterial,
		Flux_MaterialAsset* pxTargetMaterial)
	{
		if (aeTiles[uIndex] == SOKOBAN_TILE_WALL)
			return pxWallMaterial;

		if (abTargets[uIndex])
			return pxTargetMaterial;

		return pxFloorMaterial;
	}

	/**
	 * GetTileHeight - Get visual height for a tile
	 */
	static float GetTileHeight(const SokobanTileType* aeTiles, uint32_t uIndex)
	{
		if (aeTiles[uIndex] == SOKOBAN_TILE_WALL)
			return s_fWallHeight;
		return s_fFloorHeight;
	}

	/**
	 * Create3DLevel - Create all 3D entities for the level
	 *
	 * Creates entities for:
	 * - Floor and wall tiles
	 * - Boxes
	 * - Player
	 *
	 * Uses prefab-based instantiation for consistent entity setup.
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
		Flux_MaterialAsset* pxFloorMaterial,
		Flux_MaterialAsset* pxWallMaterial,
		Flux_MaterialAsset* pxTargetMaterial,
		Flux_MaterialAsset* pxBoxMaterial,
		Flux_MaterialAsset* pxBoxOnTargetMaterial,
		Flux_MaterialAsset* pxPlayerMaterial)
	{
		// Clean up existing entities first
		Destroy3DLevel();

		// Store grid dimensions for coordinate conversion
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

				// Prefab-based entity creation
				// This creates an entity with TransformComponent already attached
				Zenith_Entity xTileEntity = Zenith_Scene::Instantiate(*pxTilePrefab, "Tile");

				// Set transform position and scale
				Zenith_TransformComponent& xTransform = xTileEntity.GetComponent<Zenith_TransformComponent>();
				xTransform.SetPosition(xPos);
				xTransform.SetScale({s_fTileScale, fHeight, s_fTileScale});

				// Add model component with mesh and material
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
					xPos.y += s_fFloorHeight; // Sit on top of floor

					Zenith_Entity xBoxEntity = Zenith_Scene::Instantiate(*pxBoxPrefab, "Box");

					Zenith_TransformComponent& xTransform = xBoxEntity.GetComponent<Zenith_TransformComponent>();
					xTransform.SetPosition(xPos);
					xTransform.SetScale({s_fTileScale * 0.8f, s_fBoxHeight, s_fTileScale * 0.8f});

					// Choose material based on whether box is on target
					Flux_MaterialAsset* pxMaterial = abTargets[uIndex] ? pxBoxOnTargetMaterial : pxBoxMaterial;

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
			xPos.y += s_fFloorHeight; // Sit on top of floor

			Zenith_Entity xPlayerEntity = Zenith_Scene::Instantiate(*pxPlayerPrefab, "Player");

			Zenith_TransformComponent& xTransform = xPlayerEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(xPos);
			xTransform.SetScale({s_fTileScale * 0.7f, s_fPlayerHeight, s_fTileScale * 0.7f});

			Zenith_ModelComponent& xModel = xPlayerEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*pxCubeGeometry, *pxPlayerMaterial);

			m_uPlayerEntityID = xPlayerEntity.GetEntityID();
		}
	}

	/**
	 * Destroy3DLevel - Remove all level entities
	 *
	 * Called before creating a new level to clean up old entities.
	 */
	void Destroy3DLevel()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		for (Zenith_EntityID uID : m_axTileEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		m_axTileEntityIDs.clear();

		for (Zenith_EntityID uID : m_axBoxEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		m_axBoxEntityIDs.clear();

		if (m_uPlayerEntityID.IsValid() && xScene.EntityExists(m_uPlayerEntityID))
		{
			Zenith_Scene::Destroy(m_uPlayerEntityID);
			m_uPlayerEntityID = INVALID_ENTITY_ID;
		}
	}

	/**
	 * UpdatePlayerPosition - Update player entity position during animation
	 */
	void UpdatePlayerPosition(float fVisualX, float fVisualY)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		if (m_uPlayerEntityID.IsValid() && xScene.EntityExists(m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = xScene.GetEntityByID(m_uPlayerEntityID);
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
	 *
	 * Handles both static boxes and the currently animating box.
	 */
	void UpdateBoxPositions(
		const bool* abBoxes,
		uint32_t uGridWidth,
		uint32_t uGridHeight,
		bool bBoxAnimating,
		uint32_t uAnimBoxToX,
		uint32_t uAnimBoxToY,
		float fBoxVisualX,
		float fBoxVisualY)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		size_t uBoxIdx = 0;
		for (uint32_t uY = 0; uY < uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < uGridWidth; uX++)
			{
				uint32_t uIndex = uY * uGridWidth + uX;
				if (abBoxes[uIndex] && uBoxIdx < m_axBoxEntityIDs.size())
				{
					Zenith_EntityID uBoxID = m_axBoxEntityIDs[uBoxIdx];
					if (xScene.EntityExists(uBoxID))
					{
						Zenith_Entity xBox = xScene.GetEntityByID(uBoxID);
						if (xBox.HasComponent<Zenith_TransformComponent>())
						{
							Zenith_TransformComponent& xTransform = xBox.GetComponent<Zenith_TransformComponent>();

							float fVisualX = static_cast<float>(uX);
							float fVisualY = static_cast<float>(uY);

							// Use animated position for the moving box
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
	 *
	 * Calculates required camera height based on:
	 * - Grid dimensions
	 * - Camera FOV and aspect ratio
	 * - Padding margin
	 */
	void RepositionCamera(uint32_t uGridWidth, uint32_t uGridHeight)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		Zenith_EntityID uCameraEntityID = xScene.GetMainCameraEntity();

		if (uCameraEntityID == INVALID_ENTITY_ID || !xScene.EntityExists(uCameraEntityID))
			return;

		Zenith_Entity xCameraEntity = xScene.GetEntityByID(uCameraEntityID);
		if (!xCameraEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_CameraComponent& xCamera = xCameraEntity.GetComponent<Zenith_CameraComponent>();

		// Get camera parameters
		float fFOV = xCamera.GetFOV();
		float fAspectRatio = xCamera.GetAspectRatio();

		// Calculate grid world dimensions with padding
		float fGridWorldWidth = static_cast<float>(uGridWidth) * 1.2f;  // 20% margin
		float fGridWorldHeight = static_cast<float>(uGridHeight) * 1.2f;

		// Calculate required height to fit grid
		// For camera looking down: visible distance = 2 * height * tan(FOV/2)
		float fHalfFOVTan = tan(fFOV * 0.5f);
		float fHeightForVertical = fGridWorldHeight / (2.0f * fHalfFOVTan);
		float fHeightForHorizontal = fGridWorldWidth / (2.0f * fHalfFOVTan * fAspectRatio);

		// Use larger value to ensure both dimensions fit
		float fRequiredHeight = std::max(fHeightForVertical, fHeightForHorizontal);

		// Position camera above grid center, looking down
		xCamera.SetPosition(Zenith_Maths::Vector3(0.f, fRequiredHeight, 0.f));
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
