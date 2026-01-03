#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandler.h"

#include <random>
#include <queue>
#include <unordered_set>
#include <algorithm>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// CONFIGURATION CONSTANTS - Modify these to tune gameplay
// ============================================================================
static constexpr uint32_t s_uMinGridSize = 8;
static constexpr uint32_t s_uMaxGridSize = 16;
static constexpr float s_fAnimationDuration = 0.1f;
static constexpr uint32_t s_uMinBoxes = 2;
static constexpr uint32_t s_uMaxBoxes = 5;
static constexpr uint32_t s_uMinMovesSolution = 5;  // Minimum moves for a valid level
static constexpr uint32_t s_uMaxSolverStates = 100000;  // Limit solver state space
static constexpr float s_fTileScale = 0.9f;  // Scale of tiles (gap between them)
static constexpr float s_fFloorHeight = 0.1f;
static constexpr float s_fWallHeight = 0.8f;
static constexpr float s_fBoxHeight = 0.5f;
static constexpr float s_fPlayerHeight = 0.5f;
// ============================================================================

// ============================================================================
// STATIC RESOURCES - Shared geometry, textures, and materials
// ============================================================================
static Flux_MeshGeometry* s_pxCubeGeometry = nullptr;

// 1x1 pixel textures for each tile type
static Flux_Texture* s_pxFloorTexture = nullptr;
static Flux_Texture* s_pxWallTexture = nullptr;
static Flux_Texture* s_pxBoxTexture = nullptr;
static Flux_Texture* s_pxBoxOnTargetTexture = nullptr;
static Flux_Texture* s_pxPlayerTexture = nullptr;
static Flux_Texture* s_pxTargetTexture = nullptr;

// Materials using the textures
static Flux_MaterialAsset* s_pxFloorMaterial = nullptr;
static Flux_MaterialAsset* s_pxWallMaterial = nullptr;
static Flux_MaterialAsset* s_pxBoxMaterial = nullptr;
static Flux_MaterialAsset* s_pxBoxOnTargetMaterial = nullptr;
static Flux_MaterialAsset* s_pxPlayerMaterial = nullptr;
static Flux_MaterialAsset* s_pxTargetMaterial = nullptr;
static bool s_bStaticResourcesInitialized = false;

// Helper to create a 1x1 colored texture
static Flux_Texture* CreateColoredTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
	Flux_SurfaceInfo xTexInfo;
	xTexInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xTexInfo.m_uWidth = 1;
	xTexInfo.m_uHeight = 1;
	xTexInfo.m_uDepth = 1;
	xTexInfo.m_uNumMips = 1;
	xTexInfo.m_uNumLayers = 1;
	xTexInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	uint8_t aucPixelData[] = { r, g, b, a };

	Zenith_AssetHandler::TextureData xTexData;
	xTexData.pData = aucPixelData;
	xTexData.xSurfaceInfo = xTexInfo;
	xTexData.bCreateMips = false;
	xTexData.bIsCubemap = false;

	return Zenith_AssetHandler::AddTexture(xTexData);
}

static void InitializeStaticResources()
{
	if (s_bStaticResourcesInitialized)
		return;

	// Create shared cube geometry using engine's built-in generator
	s_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*s_pxCubeGeometry);

	// Create 1x1 pixel textures for each tile type
	s_pxFloorTexture = CreateColoredTexture(77, 77, 89);           // Dark gray floor (0.3, 0.3, 0.35)
	s_pxWallTexture = CreateColoredTexture(102, 64, 38);           // Brown walls (0.4, 0.25, 0.15)
	s_pxBoxTexture = CreateColoredTexture(204, 128, 51);           // Orange box (0.8, 0.5, 0.2)
	s_pxBoxOnTargetTexture = CreateColoredTexture(51, 204, 51);    // Green box on target (0.2, 0.8, 0.2)
	s_pxPlayerTexture = CreateColoredTexture(51, 102, 230);        // Blue player (0.2, 0.4, 0.9)
	s_pxTargetTexture = CreateColoredTexture(51, 153, 51);         // Green target (0.2, 0.6, 0.2)

	// Create materials and assign textures
	s_pxFloorMaterial = Flux_MaterialAsset::Create("SokobanFloor");
	s_pxFloorMaterial->SetDiffuseTexture(s_pxFloorTexture);

	s_pxWallMaterial = Flux_MaterialAsset::Create("SokobanWall");
	s_pxWallMaterial->SetDiffuseTexture(s_pxWallTexture);

	s_pxBoxMaterial = Flux_MaterialAsset::Create("SokobanBox");
	s_pxBoxMaterial->SetDiffuseTexture(s_pxBoxTexture);

	s_pxBoxOnTargetMaterial = Flux_MaterialAsset::Create("SokobanBoxOnTarget");
	s_pxBoxOnTargetMaterial->SetDiffuseTexture(s_pxBoxOnTargetTexture);

	s_pxPlayerMaterial = Flux_MaterialAsset::Create("SokobanPlayer");
	s_pxPlayerMaterial->SetDiffuseTexture(s_pxPlayerTexture);

	s_pxTargetMaterial = Flux_MaterialAsset::Create("SokobanTarget");
	s_pxTargetMaterial->SetDiffuseTexture(s_pxTargetTexture);

	s_bStaticResourcesInitialized = true;
}
// ============================================================================

enum SokobanTileType
{
	SOKOBAN_TILE_FLOOR,
	SOKOBAN_TILE_WALL,
	SOKOBAN_TILE_TARGET,
	SOKOBAN_TILE_BOX,
	SOKOBAN_TILE_BOX_ON_TARGET,
	SOKOBAN_TILE_PLAYER,
	SOKOBAN_TILE_COUNT
};

enum SokobanDirection
{
	SOKOBAN_DIR_UP,
	SOKOBAN_DIR_DOWN,
	SOKOBAN_DIR_LEFT,
	SOKOBAN_DIR_RIGHT,
	SOKOBAN_DIR_NONE
};

class Sokoban_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Sokoban_Behaviour)

	static constexpr uint32_t s_uMaxGridCells = s_uMaxGridSize * s_uMaxGridSize;

	Sokoban_Behaviour() = delete;
	Sokoban_Behaviour(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
		, m_uGridWidth(8)
		, m_uGridHeight(8)
		, m_uPlayerX(0)
		, m_uPlayerY(0)
		, m_uMoveCount(0)
		, m_uTargetCount(0)
		, m_uMinMoves(0)
		, m_bWon(false)
		, m_bAnimating(false)
		, m_fAnimationTimer(0.f)
		, m_fPlayerVisualX(0.f)
		, m_fPlayerVisualY(0.f)
		, m_fPlayerStartX(0.f)
		, m_fPlayerStartY(0.f)
		, m_uPlayerTargetX(0)
		, m_uPlayerTargetY(0)
		, m_bBoxAnimating(false)
		, m_uAnimBoxFromX(0)
		, m_uAnimBoxFromY(0)
		, m_uAnimBoxToX(0)
		, m_uAnimBoxToY(0)
		, m_fBoxVisualX(0.f)
		, m_fBoxVisualY(0.f)
		, m_xRng(std::random_device{}())
	{
		memset(m_aeTiles, 0, sizeof(m_aeTiles));
		memset(m_abTargets, false, sizeof(m_abTargets));
		memset(m_abBoxes, false, sizeof(m_abBoxes));
	}
	~Sokoban_Behaviour() = default;

	void OnCreate() ZENITH_FINAL override
	{
		InitializeStaticResources();
		GenerateRandomLevel();
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		if (m_bAnimating)
		{
			UpdateAnimation(fDt);
		}
		else if (!m_bWon)
		{
			HandleKeyboardInput();
		}
		Update3DVisuals();
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Sokoban Puzzle Game");
		ImGui::Separator();
		ImGui::Text("Grid Size: %u x %u", m_uGridWidth, m_uGridHeight);
		ImGui::Text("Moves: %u", m_uMoveCount);
		ImGui::Text("Min Moves: %u", m_uMinMoves);
		ImGui::Text("Boxes on targets: %u / %u", CountBoxesOnTargets(), m_uTargetCount);
		if (m_bWon)
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "LEVEL COMPLETE!");
		}
		if (ImGui::Button("Reset Level"))
		{
			ResetLevel();
		}
		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD / Arrow Keys: Move");
		ImGui::Text("  R: Reset Level");
		ImGui::Text("  Mouse Click: Move toward click");
#endif
	}

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override {}
	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override {}

private:
	// ========================================================================
	// Animation System
	// ========================================================================
	void UpdateAnimation(float fDt)
	{
		m_fAnimationTimer += fDt;
		float fProgress = std::min(m_fAnimationTimer / s_fAnimationDuration, 1.f);

		// Lerp player position
		m_fPlayerVisualX = m_fPlayerStartX + (static_cast<float>(m_uPlayerTargetX) - m_fPlayerStartX) * fProgress;
		m_fPlayerVisualY = m_fPlayerStartY + (static_cast<float>(m_uPlayerTargetY) - m_fPlayerStartY) * fProgress;

		// Lerp box position if pushing
		if (m_bBoxAnimating)
		{
			m_fBoxVisualX = static_cast<float>(m_uAnimBoxFromX) + (static_cast<float>(m_uAnimBoxToX) - static_cast<float>(m_uAnimBoxFromX)) * fProgress;
			m_fBoxVisualY = static_cast<float>(m_uAnimBoxFromY) + (static_cast<float>(m_uAnimBoxToY) - static_cast<float>(m_uAnimBoxFromY)) * fProgress;
		}

		// Animation complete
		if (fProgress >= 1.f)
		{
			m_bAnimating = false;
			m_bBoxAnimating = false;
			m_fPlayerVisualX = static_cast<float>(m_uPlayerTargetX);
			m_fPlayerVisualY = static_cast<float>(m_uPlayerTargetY);

			if (CheckWinCondition())
			{
				m_bWon = true;
				UpdateStatusText();
			}
		}
	}

	void StartAnimation(uint32_t uFromX, uint32_t uFromY, uint32_t uToX, uint32_t uToY)
	{
		m_bAnimating = true;
		m_fAnimationTimer = 0.f;
		m_fPlayerStartX = static_cast<float>(uFromX);
		m_fPlayerStartY = static_cast<float>(uFromY);
		m_fPlayerVisualX = m_fPlayerStartX;
		m_fPlayerVisualY = m_fPlayerStartY;
		m_uPlayerTargetX = uToX;
		m_uPlayerTargetY = uToY;
	}

	void StartBoxAnimation(uint32_t uFromX, uint32_t uFromY, uint32_t uToX, uint32_t uToY)
	{
		m_bBoxAnimating = true;
		m_uAnimBoxFromX = uFromX;
		m_uAnimBoxFromY = uFromY;
		m_uAnimBoxToX = uToX;
		m_uAnimBoxToY = uToY;
		m_fBoxVisualX = static_cast<float>(uFromX);
		m_fBoxVisualY = static_cast<float>(uFromY);
	}

	// ========================================================================
	// Input Handling
	// ========================================================================
	void HandleKeyboardInput()
	{
		if (m_bAnimating) return;

		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_UP) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W))
		{
			TryMove(SOKOBAN_DIR_UP);
		}
		else if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_DOWN) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_S))
		{
			TryMove(SOKOBAN_DIR_DOWN);
		}
		else if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_LEFT) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_A))
		{
			TryMove(SOKOBAN_DIR_LEFT);
		}
		else if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_RIGHT) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_D))
		{
			TryMove(SOKOBAN_DIR_RIGHT);
		}

		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
		{
			ResetLevel();
		}
	}

	// ========================================================================
	// Movement Logic
	// ========================================================================
	bool TryMove(SokobanDirection eDir)
	{
		if (m_bAnimating) return false;

		int32_t iDeltaX = 0, iDeltaY = 0;
		switch (eDir)
		{
		case SOKOBAN_DIR_UP:    iDeltaY = -1; break;
		case SOKOBAN_DIR_DOWN:  iDeltaY = 1;  break;
		case SOKOBAN_DIR_LEFT:  iDeltaX = -1; break;
		case SOKOBAN_DIR_RIGHT: iDeltaX = 1;  break;
		default: return false;
		}

		uint32_t uNewX = m_uPlayerX + iDeltaX;
		uint32_t uNewY = m_uPlayerY + iDeltaY;

		if (uNewX >= m_uGridWidth || uNewY >= m_uGridHeight)
		{
			return false;
		}

		uint32_t uNewIndex = uNewY * m_uGridWidth + uNewX;

		if (m_aeTiles[uNewIndex] == SOKOBAN_TILE_WALL)
		{
			return false;
		}

		uint32_t uOldX = m_uPlayerX;
		uint32_t uOldY = m_uPlayerY;
		bool bPushingBox = false;
		uint32_t uBoxDestX = 0, uBoxDestY = 0;

		if (m_abBoxes[uNewIndex])
		{
			if (!CanPushBox(uNewX, uNewY, eDir))
			{
				return false;
			}
			bPushingBox = true;
			uBoxDestX = uNewX + iDeltaX;
			uBoxDestY = uNewY + iDeltaY;
			PushBox(uNewX, uNewY, eDir);
		}

		m_uPlayerX = uNewX;
		m_uPlayerY = uNewY;
		m_uMoveCount++;

		// Start animation
		StartAnimation(uOldX, uOldY, uNewX, uNewY);
		if (bPushingBox)
		{
			StartBoxAnimation(uNewX, uNewY, uBoxDestX, uBoxDestY);
		}

		UpdateStatusText();
		return true;
	}

	bool CanPushBox(uint32_t uBoxX, uint32_t uBoxY, SokobanDirection eDir) const
	{
		int32_t iDeltaX = 0, iDeltaY = 0;
		switch (eDir)
		{
		case SOKOBAN_DIR_UP:    iDeltaY = -1; break;
		case SOKOBAN_DIR_DOWN:  iDeltaY = 1;  break;
		case SOKOBAN_DIR_LEFT:  iDeltaX = -1; break;
		case SOKOBAN_DIR_RIGHT: iDeltaX = 1;  break;
		default: return false;
		}

		uint32_t uDestX = uBoxX + iDeltaX;
		uint32_t uDestY = uBoxY + iDeltaY;

		if (uDestX >= m_uGridWidth || uDestY >= m_uGridHeight)
		{
			return false;
		}

		uint32_t uDestIndex = uDestY * m_uGridWidth + uDestX;

		if (m_aeTiles[uDestIndex] == SOKOBAN_TILE_WALL)
		{
			return false;
		}

		if (m_abBoxes[uDestIndex])
		{
			return false;
		}

		return true;
	}

	void PushBox(uint32_t uFromX, uint32_t uFromY, SokobanDirection eDir)
	{
		int32_t iDeltaX = 0, iDeltaY = 0;
		switch (eDir)
		{
		case SOKOBAN_DIR_UP:    iDeltaY = -1; break;
		case SOKOBAN_DIR_DOWN:  iDeltaY = 1;  break;
		case SOKOBAN_DIR_LEFT:  iDeltaX = -1; break;
		case SOKOBAN_DIR_RIGHT: iDeltaX = 1;  break;
		default: return;
		}

		uint32_t uFromIndex = uFromY * m_uGridWidth + uFromX;
		uint32_t uToX = uFromX + iDeltaX;
		uint32_t uToY = uFromY + iDeltaY;
		uint32_t uToIndex = uToY * m_uGridWidth + uToX;

		m_abBoxes[uFromIndex] = false;
		m_abBoxes[uToIndex] = true;
	}

	// ========================================================================
	// 3D Rendering
	// ========================================================================

	// Convert grid coordinates to world position
	Zenith_Maths::Vector3 GridToWorld(float fGridX, float fGridY, float fHeight) const
	{
		// Center grid at origin, negate Z to match camera orientation
		float fWorldX = fGridX - static_cast<float>(m_uGridWidth) * 0.5f;
		float fWorldZ = static_cast<float>(m_uGridHeight) * 0.5f - fGridY;
		return {fWorldX, fHeight * 0.5f, fWorldZ};
	}

	// Get material for a tile at given index
	Flux_MaterialAsset* GetMaterialForTile(uint32_t uIndex, bool bIsBox = false, bool bIsPlayer = false) const
	{
		if (bIsPlayer)
			return s_pxPlayerMaterial;

		if (bIsBox)
		{
			return m_abTargets[uIndex] ? s_pxBoxOnTargetMaterial : s_pxBoxMaterial;
		}

		if (m_aeTiles[uIndex] == SOKOBAN_TILE_WALL)
			return s_pxWallMaterial;

		if (m_abTargets[uIndex])
			return s_pxTargetMaterial;

		return s_pxFloorMaterial;
	}

	// Get height for a tile type
	float GetTileHeight(uint32_t uIndex) const
	{
		if (m_aeTiles[uIndex] == SOKOBAN_TILE_WALL)
			return s_fWallHeight;
		return s_fFloorHeight;
	}

	// Destroy all 3D entities
	void Destroy3DLevel()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		for (Zenith_EntityID uID : m_axTileEntityIDs)
		{
			if (xScene.EntityExists(uID))
				xScene.RemoveEntity(uID);
		}
		m_axTileEntityIDs.clear();

		for (Zenith_EntityID uID : m_axBoxEntityIDs)
		{
			if (xScene.EntityExists(uID))
				xScene.RemoveEntity(uID);
		}
		m_axBoxEntityIDs.clear();

		if (m_uPlayerEntityID != 0 && xScene.EntityExists(m_uPlayerEntityID))
		{
			xScene.RemoveEntity(m_uPlayerEntityID);
			m_uPlayerEntityID = 0;
		}
	}

	// Create all 3D entities for the current level
	void Create3DLevel()
	{
		// Destroy old entities first
		Destroy3DLevel();

		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Create floor and wall tiles
		for (uint32_t uY = 0; uY < m_uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;

				Zenith_Entity xTileEntity(&xScene, "Tile");

				float fHeight = GetTileHeight(uIndex);
				Zenith_Maths::Vector3 xPos = GridToWorld(static_cast<float>(uX), static_cast<float>(uY), fHeight);

				Zenith_TransformComponent& xTransform = xTileEntity.GetComponent<Zenith_TransformComponent>();
				xTransform.SetPosition(xPos);
				xTransform.SetScale({s_fTileScale, fHeight, s_fTileScale});

				Zenith_ModelComponent& xModel = xTileEntity.AddComponent<Zenith_ModelComponent>();
				xModel.AddMeshEntry(*s_pxCubeGeometry, *GetMaterialForTile(uIndex));

				m_axTileEntityIDs.push_back(xTileEntity.GetEntityID());
			}
		}

		// Create box entities
		for (uint32_t uY = 0; uY < m_uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;
				if (m_abBoxes[uIndex])
				{
					Zenith_Entity xBoxEntity(&xScene, "Box");

					Zenith_Maths::Vector3 xPos = GridToWorld(static_cast<float>(uX), static_cast<float>(uY), s_fBoxHeight);
					xPos.y += s_fFloorHeight;  // Sit on top of floor

					Zenith_TransformComponent& xTransform = xBoxEntity.GetComponent<Zenith_TransformComponent>();
					xTransform.SetPosition(xPos);
					xTransform.SetScale({s_fTileScale * 0.8f, s_fBoxHeight, s_fTileScale * 0.8f});

					Zenith_ModelComponent& xModel = xBoxEntity.AddComponent<Zenith_ModelComponent>();
					xModel.AddMeshEntry(*s_pxCubeGeometry, *GetMaterialForTile(uIndex, true));

					m_axBoxEntityIDs.push_back(xBoxEntity.GetEntityID());
				}
			}
		}

		// Create player entity
		{
			Zenith_Entity xPlayerEntity(&xScene, "Player");

			Zenith_Maths::Vector3 xPos = GridToWorld(static_cast<float>(m_uPlayerX), static_cast<float>(m_uPlayerY), s_fPlayerHeight);
			xPos.y += s_fFloorHeight;  // Sit on top of floor

			Zenith_TransformComponent& xTransform = xPlayerEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(xPos);
			xTransform.SetScale({s_fTileScale * 0.7f, s_fPlayerHeight, s_fTileScale * 0.7f});

			Zenith_ModelComponent& xModel = xPlayerEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*s_pxCubeGeometry, *s_pxPlayerMaterial);

			m_uPlayerEntityID = xPlayerEntity.GetEntityID();
		}

		// Initialize visual positions
		m_fPlayerVisualX = static_cast<float>(m_uPlayerX);
		m_fPlayerVisualY = static_cast<float>(m_uPlayerY);
	}

	// Update 3D entity positions (called every frame)
	void Update3DVisuals()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Update player position
		if (m_uPlayerEntityID != 0 && xScene.EntityExists(m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = xScene.GetEntityByID(m_uPlayerEntityID);
			if (xPlayer.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_TransformComponent& xTransform = xPlayer.GetComponent<Zenith_TransformComponent>();

				float fVisualX = m_bAnimating ? m_fPlayerVisualX : static_cast<float>(m_uPlayerX);
				float fVisualY = m_bAnimating ? m_fPlayerVisualY : static_cast<float>(m_uPlayerY);

				Zenith_Maths::Vector3 xPos = GridToWorld(fVisualX, fVisualY, s_fPlayerHeight);
				xPos.y += s_fFloorHeight;
				xTransform.SetPosition(xPos);
			}
		}

		// Update box entities
		// Rebuild box entity list based on current game state
		// This handles boxes moving to new positions after push
		size_t uBoxIdx = 0;
		for (uint32_t uY = 0; uY < m_uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;
				if (m_abBoxes[uIndex] && uBoxIdx < m_axBoxEntityIDs.size())
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

							// If this is the animating box, use visual position
							if (m_bBoxAnimating && uX == m_uAnimBoxToX && uY == m_uAnimBoxToY)
							{
								fVisualX = m_fBoxVisualX;
								fVisualY = m_fBoxVisualY;
							}

							Zenith_Maths::Vector3 xPos = GridToWorld(fVisualX, fVisualY, s_fBoxHeight);
							xPos.y += s_fFloorHeight;
							xTransform.SetPosition(xPos);

							// Update material based on whether box is on target
							if (xBox.HasComponent<Zenith_ModelComponent>())
							{
								// Note: Material change would require recreating mesh entry
								// For now, boxes keep their initial material
							}
						}
					}
					uBoxIdx++;
				}
			}
		}
	}

	// Reposition camera to fit the grid in view
	void RepositionCamera()
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

		// Calculate grid world dimensions (each tile is 1 unit)
		float fGridWorldWidth = static_cast<float>(m_uGridWidth);
		float fGridWorldHeight = static_cast<float>(m_uGridHeight);

		// Add padding (10% margin on each side)
		float fPadding = 1.2f;
		fGridWorldWidth *= fPadding;
		fGridWorldHeight *= fPadding;

		// For a camera looking straight down (pitch ≈ -90°):
		// Vertical visible distance = 2 * height * tan(FOV/2)
		// Horizontal visible distance = vertical * aspectRatio
		float fHalfFOVTan = tan(fFOV * 0.5f);

		// Calculate required height to fit grid in view
		// For vertical fit: height = gridHeight / (2 * tan(FOV/2))
		// For horizontal fit: height = gridWidth / (2 * tan(FOV/2) * aspectRatio)
		float fHeightForVertical = fGridWorldHeight / (2.0f * fHalfFOVTan);
		float fHeightForHorizontal = fGridWorldWidth / (2.0f * fHalfFOVTan * fAspectRatio);

		// Use the larger of the two to ensure both dimensions fit
		float fRequiredHeight = std::max(fHeightForVertical, fHeightForHorizontal);

		// Update camera position (keep X and Z at 0, adjust Y height)
		xCamera.SetPosition(Zenith_Maths::Vector3(0.f, fRequiredHeight, 0.f));
	}

	// Legacy helper kept for compatibility
	Zenith_Maths::Vector4 GetTileColor(SokobanTileType eTile) const
	{
		switch (eTile)
		{
		case SOKOBAN_TILE_FLOOR:
			return Zenith_Maths::Vector4(0.3f, 0.3f, 0.35f, 1.0f);
		case SOKOBAN_TILE_WALL:
			return Zenith_Maths::Vector4(0.15f, 0.1f, 0.08f, 1.0f);
		case SOKOBAN_TILE_TARGET:
			return Zenith_Maths::Vector4(0.2f, 0.6f, 0.2f, 1.0f);
		case SOKOBAN_TILE_BOX:
			return Zenith_Maths::Vector4(0.8f, 0.5f, 0.2f, 1.0f);
		case SOKOBAN_TILE_BOX_ON_TARGET:
			return Zenith_Maths::Vector4(0.2f, 0.8f, 0.2f, 1.0f);
		case SOKOBAN_TILE_PLAYER:
			return Zenith_Maths::Vector4(0.2f, 0.4f, 0.9f, 1.0f);
		default:
			return Zenith_Maths::Vector4(1.0f, 0.0f, 1.0f, 1.0f);
		}
	}

	// ========================================================================
	// Sokoban Solver (BFS)
	// ========================================================================
	struct SolverState
	{
		uint32_t uPlayerX;
		uint32_t uPlayerY;
		std::vector<uint32_t> axBoxPositions;  // Sorted box positions as indices

		bool operator==(const SolverState& xOther) const
		{
			return uPlayerX == xOther.uPlayerX &&
				   uPlayerY == xOther.uPlayerY &&
				   axBoxPositions == xOther.axBoxPositions;
		}
	};

	struct SolverStateHash
	{
		size_t operator()(const SolverState& xState) const
		{
			size_t uHash = std::hash<uint32_t>()(xState.uPlayerX);
			uHash ^= std::hash<uint32_t>()(xState.uPlayerY) << 1;
			for (uint32_t uPos : xState.axBoxPositions)
			{
				uHash ^= std::hash<uint32_t>()(uPos) + 0x9e3779b9 + (uHash << 6) + (uHash >> 2);
			}
			return uHash;
		}
	};

	int32_t SolveLevel() const
	{
		// Create initial state
		SolverState xInitialState;
		xInitialState.uPlayerX = m_uPlayerX;
		xInitialState.uPlayerY = m_uPlayerY;

		for (uint32_t i = 0; i < m_uGridWidth * m_uGridHeight; i++)
		{
			if (m_abBoxes[i])
			{
				xInitialState.axBoxPositions.push_back(i);
			}
		}
		std::sort(xInitialState.axBoxPositions.begin(), xInitialState.axBoxPositions.end());

		// Check if already solved
		if (IsStateSolved(xInitialState))
		{
			return 0;
		}

		// BFS
		std::queue<std::pair<SolverState, int32_t>> xQueue;
		std::unordered_set<SolverState, SolverStateHash> xVisited;

		xQueue.push({xInitialState, 0});
		xVisited.insert(xInitialState);

		int32_t aDeltaX[] = {0, 0, -1, 1};
		int32_t aDeltaY[] = {-1, 1, 0, 0};

		while (!xQueue.empty() && xVisited.size() < s_uMaxSolverStates)
		{
			auto [xCurrentState, iMoves] = xQueue.front();
			xQueue.pop();

			for (int iDir = 0; iDir < 4; iDir++)
			{
				int32_t iNewX = static_cast<int32_t>(xCurrentState.uPlayerX) + aDeltaX[iDir];
				int32_t iNewY = static_cast<int32_t>(xCurrentState.uPlayerY) + aDeltaY[iDir];

				if (iNewX < 0 || iNewY < 0 ||
					static_cast<uint32_t>(iNewX) >= m_uGridWidth ||
					static_cast<uint32_t>(iNewY) >= m_uGridHeight)
				{
					continue;
				}

				uint32_t uNewIndex = iNewY * m_uGridWidth + iNewX;

				// Can't walk into walls
				if (m_aeTiles[uNewIndex] == SOKOBAN_TILE_WALL)
				{
					continue;
				}

				// Check if there's a box at the new position
				auto it = std::find(xCurrentState.axBoxPositions.begin(),
									xCurrentState.axBoxPositions.end(),
									uNewIndex);

				SolverState xNewState = xCurrentState;
				xNewState.uPlayerX = iNewX;
				xNewState.uPlayerY = iNewY;

				if (it != xCurrentState.axBoxPositions.end())
				{
					// There's a box, try to push it
					int32_t iBoxNewX = iNewX + aDeltaX[iDir];
					int32_t iBoxNewY = iNewY + aDeltaY[iDir];

					if (iBoxNewX < 0 || iBoxNewY < 0 ||
						static_cast<uint32_t>(iBoxNewX) >= m_uGridWidth ||
						static_cast<uint32_t>(iBoxNewY) >= m_uGridHeight)
					{
						continue;
					}

					uint32_t uBoxNewIndex = iBoxNewY * m_uGridWidth + iBoxNewX;

					// Can't push into wall
					if (m_aeTiles[uBoxNewIndex] == SOKOBAN_TILE_WALL)
					{
						continue;
					}

					// Can't push into another box
					if (std::find(xCurrentState.axBoxPositions.begin(),
								  xCurrentState.axBoxPositions.end(),
								  uBoxNewIndex) != xCurrentState.axBoxPositions.end())
					{
						continue;
					}

					// Update box position in new state
					xNewState.axBoxPositions.erase(
						std::find(xNewState.axBoxPositions.begin(),
								  xNewState.axBoxPositions.end(),
								  uNewIndex));
					xNewState.axBoxPositions.push_back(uBoxNewIndex);
					std::sort(xNewState.axBoxPositions.begin(), xNewState.axBoxPositions.end());
				}

				if (xVisited.find(xNewState) != xVisited.end())
				{
					continue;
				}

				if (IsStateSolved(xNewState))
				{
					return iMoves + 1;
				}

				xVisited.insert(xNewState);
				xQueue.push({xNewState, iMoves + 1});
			}
		}

		return -1;  // Unsolvable or too complex
	}

	bool IsStateSolved(const SolverState& xState) const
	{
		for (uint32_t uBoxPos : xState.axBoxPositions)
		{
			if (!m_abTargets[uBoxPos])
			{
				return false;
			}
		}
		return !xState.axBoxPositions.empty();
	}

	// ========================================================================
	// Random Level Generation
	// ========================================================================
	void GenerateRandomLevel()
	{
		int iAttempts = 0;
		const int iMaxAttempts = 1000;

		while (iAttempts < iMaxAttempts)
		{
			iAttempts++;
			GenerateRandomLevelAttempt();

			int32_t iMinMoves = SolveLevel();
			if (iMinMoves >= static_cast<int32_t>(s_uMinMovesSolution))
			{
				m_uMinMoves = static_cast<uint32_t>(iMinMoves);
				Create3DLevel();
				RepositionCamera();
				UpdateUIPositions();
				UpdateStatusText();
				return;
			}
		}

		// If we failed to generate a solvable level, use fallback
		Zenith_Log("Warning: Failed to generate solvable level after %d attempts, using fallback", iMaxAttempts);
		GenerateFallbackLevel();
		m_uMinMoves = SolveLevel();
		if (m_uMinMoves < 0) m_uMinMoves = 0;
		Create3DLevel();
		RepositionCamera();
		UpdateUIPositions();
		UpdateStatusText();
	}

	void GenerateRandomLevelAttempt()
	{
		std::uniform_int_distribution<uint32_t> xSizeDist(s_uMinGridSize, s_uMaxGridSize);
		std::uniform_int_distribution<uint32_t> xBoxDist(s_uMinBoxes, s_uMaxBoxes);

		m_uGridWidth = xSizeDist(m_xRng);
		m_uGridHeight = xSizeDist(m_xRng);

		// Clear arrays
		memset(m_aeTiles, 0, sizeof(m_aeTiles));
		memset(m_abTargets, false, sizeof(m_abTargets));
		memset(m_abBoxes, false, sizeof(m_abBoxes));
		m_uMoveCount = 0;
		m_bWon = false;
		m_bAnimating = false;

		// Fill with walls on border, floor inside
		for (uint32_t uY = 0; uY < m_uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;
				if (uX == 0 || uY == 0 || uX == m_uGridWidth - 1 || uY == m_uGridHeight - 1)
				{
					m_aeTiles[uIndex] = SOKOBAN_TILE_WALL;
				}
				else
				{
					m_aeTiles[uIndex] = SOKOBAN_TILE_FLOOR;
				}
			}
		}

		// Collect inner floor positions
		std::vector<uint32_t> axFloorPositions;
		for (uint32_t uY = 1; uY < m_uGridHeight - 1; uY++)
		{
			for (uint32_t uX = 1; uX < m_uGridWidth - 1; uX++)
			{
				axFloorPositions.push_back(uY * m_uGridWidth + uX);
			}
		}

		// Add random internal walls (10-20% of inner cells)
		uint32_t uInnerCells = (m_uGridWidth - 2) * (m_uGridHeight - 2);
		std::uniform_int_distribution<uint32_t> xWallPctDist(10, 20);
		uint32_t uWallCount = (uInnerCells * xWallPctDist(m_xRng)) / 100;

		std::shuffle(axFloorPositions.begin(), axFloorPositions.end(), m_xRng);

		for (uint32_t i = 0; i < uWallCount && i < axFloorPositions.size(); i++)
		{
			m_aeTiles[axFloorPositions[i]] = SOKOBAN_TILE_WALL;
		}

		// Recollect floor positions (excluding walls)
		axFloorPositions.clear();
		for (uint32_t uY = 1; uY < m_uGridHeight - 1; uY++)
		{
			for (uint32_t uX = 1; uX < m_uGridWidth - 1; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;
				if (m_aeTiles[uIndex] == SOKOBAN_TILE_FLOOR)
				{
					axFloorPositions.push_back(uIndex);
				}
			}
		}

		if (axFloorPositions.size() < s_uMaxBoxes * 2 + 1)
		{
			// Not enough space, this attempt will fail
			return;
		}

		std::shuffle(axFloorPositions.begin(), axFloorPositions.end(), m_xRng);

		// Place targets and boxes
		uint32_t uNumBoxes = xBoxDist(m_xRng);
		uNumBoxes = std::min(uNumBoxes, static_cast<uint32_t>(axFloorPositions.size() / 2));
		m_uTargetCount = uNumBoxes;

		uint32_t uPlaceIndex = 0;

		// Place targets
		for (uint32_t i = 0; i < uNumBoxes; i++)
		{
			m_abTargets[axFloorPositions[uPlaceIndex++]] = true;
		}

		// Place boxes (on non-target floors)
		for (uint32_t i = 0; i < uNumBoxes; i++)
		{
			m_abBoxes[axFloorPositions[uPlaceIndex++]] = true;
		}

		// Place player
		m_uPlayerX = axFloorPositions[uPlaceIndex] % m_uGridWidth;
		m_uPlayerY = axFloorPositions[uPlaceIndex] / m_uGridWidth;
	}

	void GenerateFallbackLevel()
	{
		// Simple known-solvable 8x8 level
		m_uGridWidth = 8;
		m_uGridHeight = 8;

		memset(m_aeTiles, 0, sizeof(m_aeTiles));
		memset(m_abTargets, false, sizeof(m_abTargets));
		memset(m_abBoxes, false, sizeof(m_abBoxes));
		m_uMoveCount = 0;
		m_bWon = false;
		m_bAnimating = false;

		// Border walls
		for (uint32_t uY = 0; uY < m_uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;
				if (uX == 0 || uY == 0 || uX == m_uGridWidth - 1 || uY == m_uGridHeight - 1)
				{
					m_aeTiles[uIndex] = SOKOBAN_TILE_WALL;
				}
				else
				{
					m_aeTiles[uIndex] = SOKOBAN_TILE_FLOOR;
				}
			}
		}

		// Simple layout with 2 boxes
		m_abTargets[2 * 8 + 5] = true;  // Target at (5, 2)
		m_abTargets[5 * 8 + 5] = true;  // Target at (5, 5)
		m_uTargetCount = 2;

		m_abBoxes[3 * 8 + 3] = true;    // Box at (3, 3)
		m_abBoxes[4 * 8 + 4] = true;    // Box at (4, 4)

		m_uPlayerX = 2;
		m_uPlayerY = 2;
	}

	// ========================================================================
	// UI Management
	// ========================================================================
	void UpdateUIPositions()
	{
		// UI elements use anchor system (TopRight) so no position updates needed
	}

	void UpdateStatusText()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			return;
		}

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			char acBuffer[64];
			snprintf(acBuffer, sizeof(acBuffer), "Moves: %u", m_uMoveCount);
			pxStatus->SetText(acBuffer);
		}

		Zenith_UI::Zenith_UIText* pxProgress = xUI.FindElement<Zenith_UI::Zenith_UIText>("Progress");
		if (pxProgress)
		{
			char acBuffer[64];
			snprintf(acBuffer, sizeof(acBuffer), "Boxes: %u / %u", CountBoxesOnTargets(), m_uTargetCount);
			pxProgress->SetText(acBuffer);
		}

		Zenith_UI::Zenith_UIText* pxWin = xUI.FindElement<Zenith_UI::Zenith_UIText>("WinText");
		if (pxWin)
		{
			pxWin->SetText(m_bWon ? "LEVEL COMPLETE!" : "");
		}

		Zenith_UI::Zenith_UIText* pxMinMoves = xUI.FindElement<Zenith_UI::Zenith_UIText>("MinMoves");
		if (pxMinMoves)
		{
			char acBuffer[64];
			snprintf(acBuffer, sizeof(acBuffer), "Min Moves: %u", m_uMinMoves);
			pxMinMoves->SetText(acBuffer);
		}
	}

	void ResetLevel()
	{
		GenerateRandomLevel();
	}

	bool CheckWinCondition() const
	{
		return CountBoxesOnTargets() == m_uTargetCount && m_uTargetCount > 0;
	}

	uint32_t CountBoxesOnTargets() const
	{
		uint32_t uCount = 0;
		for (uint32_t i = 0; i < m_uGridWidth * m_uGridHeight; i++)
		{
			if (m_abBoxes[i] && m_abTargets[i])
			{
				uCount++;
			}
		}
		return uCount;
	}

	// ========================================================================
	// Member Variables
	// ========================================================================
	Zenith_Entity m_xParentEntity;

	// Grid state - sized for max possible grid
	uint32_t m_uGridWidth;
	uint32_t m_uGridHeight;
	SokobanTileType m_aeTiles[s_uMaxGridCells];
	bool m_abTargets[s_uMaxGridCells];
	bool m_abBoxes[s_uMaxGridCells];

	// Player state
	uint32_t m_uPlayerX;
	uint32_t m_uPlayerY;

	// Game state
	uint32_t m_uMoveCount;
	uint32_t m_uTargetCount;
	uint32_t m_uMinMoves;
	bool m_bWon;

	// Animation state
	bool m_bAnimating;
	float m_fAnimationTimer;
	float m_fPlayerVisualX;
	float m_fPlayerVisualY;
	float m_fPlayerStartX;
	float m_fPlayerStartY;
	uint32_t m_uPlayerTargetX;
	uint32_t m_uPlayerTargetY;

	// Box animation
	bool m_bBoxAnimating;
	uint32_t m_uAnimBoxFromX;
	uint32_t m_uAnimBoxFromY;
	uint32_t m_uAnimBoxToX;
	uint32_t m_uAnimBoxToY;
	float m_fBoxVisualX;
	float m_fBoxVisualY;

	// Random number generator
	std::mt19937 m_xRng;

	// 3D rendering entities
	std::vector<Zenith_EntityID> m_axTileEntityIDs;
	std::vector<Zenith_EntityID> m_axBoxEntityIDs;
	Zenith_EntityID m_uPlayerEntityID = 0;
};
