#pragma once
/**
 * Sokoban_Behaviour.h - Main game coordinator
 *
 * Demonstrates: Zenith_ScriptBehaviour lifecycle hooks
 *
 * This is the main behavior that coordinates all game systems:
 * - Input handling (Sokoban_Input.h)
 * - Game logic (Sokoban_GridLogic.h)
 * - 3D rendering (Sokoban_Rendering.h)
 * - Level generation (Sokoban_LevelGenerator.h)
 * - Level validation (Sokoban_Solver.h)
 * - UI updates (Sokoban_UIManager.h)
 *
 * Key lifecycle hooks:
 * - OnAwake()  - Called at RUNTIME creation only
 * - OnStart()  - Called before first OnUpdate
 * - OnUpdate() - Called every frame
 * - RenderPropertiesPanel() - Editor UI (tools build)
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"

// Include extracted modules
#include "Sokoban_Input.h"
#include "Sokoban_GridLogic.h"
#include "Sokoban_Rendering.h"
#include "Sokoban_LevelGenerator.h"
#include "Sokoban_Solver.h"
#include "Sokoban_UIManager.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"
#include <filesystem>
#endif

// ============================================================================
// Sokoban Resources - Global access
// Defined in Sokoban.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================
class Zenith_Prefab;
class Flux_ParticleEmitterConfig;

namespace Sokoban
{
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern MaterialHandle g_xFloorMaterial;
	extern MaterialHandle g_xWallMaterial;
	extern MaterialHandle g_xBoxMaterial;
	extern MaterialHandle g_xBoxOnTargetMaterial;
	extern MaterialHandle g_xPlayerMaterial;
	extern MaterialHandle g_xTargetMaterial;

	extern Zenith_Prefab* g_pxTilePrefab;
	extern Zenith_Prefab* g_pxBoxPrefab;
	extern Zenith_Prefab* g_pxPlayerPrefab;

	// Particle effects
	extern Flux_ParticleEmitterConfig* g_pxDustConfig;
	extern Zenith_EntityID g_uDustEmitterID;
}

// Note: SokobanTileType and SokobanDirection enums are defined in Sokoban_GridLogic.h

// ============================================================================
// Configuration Constants
// ============================================================================
static constexpr uint32_t s_uMaxGridSizeConfig = 16;
static constexpr float s_fAnimationDuration = 0.1f;

// ============================================================================
// Main Behavior Class
// ============================================================================
class Sokoban_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Sokoban_Behaviour)

	static constexpr uint32_t s_uMaxGridCells = s_uMaxGridSizeConfig * s_uMaxGridSizeConfig;

	Sokoban_Behaviour() = delete;
	Sokoban_Behaviour(Zenith_Entity& /*xParentEntity*/)
		: m_uGridWidth(8)
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

	~Sokoban_Behaviour()
	{
		if (m_bOwnsGeometry && m_pxCubeGeometry)
		{
			delete m_pxCubeGeometry;
			m_pxCubeGeometry = nullptr;
		}
	}

	// ========================================================================
	// Lifecycle Hooks - Called by engine
	// ========================================================================

	/**
	 * OnAwake - Called when behavior is attached at RUNTIME
	 * NOT called during scene loading/deserialization.
	 * Use for: Initial resource setup, procedural generation.
	 */
	void OnAwake() ZENITH_FINAL override
	{
		// Use global resources (initialized in Sokoban.cpp)
		m_pxCubeGeometry = Sokoban::g_pxCubeGeometry;
		m_xFloorMaterial = Sokoban::g_xFloorMaterial;
		m_xWallMaterial = Sokoban::g_xWallMaterial;
		m_xBoxMaterial = Sokoban::g_xBoxMaterial;
		m_xBoxOnTargetMaterial = Sokoban::g_xBoxOnTargetMaterial;
		m_xPlayerMaterial = Sokoban::g_xPlayerMaterial;
		m_xTargetMaterial = Sokoban::g_xTargetMaterial;

		GenerateNewLevel();
	}

	/**
	 * OnStart - Called before first OnUpdate, for ALL entities
	 * Called even for entities loaded from scene file.
	 * Use for: Late initialization that depends on other components.
	 */
	void OnStart() ZENITH_FINAL override
	{
		if (!m_xRenderer.GetPlayerEntityID().IsValid())
		{
			GenerateNewLevel();
		}
	}

	/**
	 * OnUpdate - Called every frame
	 * Main game loop: input -> logic -> animation -> rendering
	 */
	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		if (m_bAnimating)
		{
			UpdateAnimation(fDt);
		}
		else if (!m_bWon)
		{
			HandleInput();
		}
		UpdateVisuals();
	}

	/**
	 * RenderPropertiesPanel - Editor UI (tools build only)
	 * Renders ImGui controls for debugging and configuration.
	 */
	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Sokoban Puzzle Game");
		ImGui::Separator();
		ImGui::Text("Grid Size: %u x %u", m_uGridWidth, m_uGridHeight);
		ImGui::Text("Moves: %u", m_uMoveCount);
		ImGui::Text("Min Moves: %u", m_uMinMoves);
		ImGui::Text("Boxes on targets: %u / %u",
			Sokoban_GridLogic::CountBoxesOnTargets(m_abBoxes, m_abTargets, m_uGridWidth * m_uGridHeight),
			m_uTargetCount);

		if (m_bWon)
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "LEVEL COMPLETE!");
		}

		if (ImGui::Button("New Level"))
		{
			GenerateNewLevel();
		}

		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD / Arrow Keys: Move");
		ImGui::Text("  R: New Level");

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Visual Assets", ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderMeshSlot("Cube Mesh", m_pxCubeGeometry);
			ImGui::Separator();
			RenderMaterialSlot("Floor Material", m_xFloorMaterial);
			RenderMaterialSlot("Wall Material", m_xWallMaterial);
			RenderMaterialSlot("Box Material", m_xBoxMaterial);
			RenderMaterialSlot("Box On Target", m_xBoxOnTargetMaterial);
			RenderMaterialSlot("Player Material", m_xPlayerMaterial);
			RenderMaterialSlot("Target Material", m_xTargetMaterial);
		}
#endif
	}

	// ========================================================================
	// Serialization - Save/load behavior state
	// ========================================================================

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// Mesh path
		std::string strMeshPath = (m_pxCubeGeometry && !m_pxCubeGeometry->m_strSourcePath.empty())
			? m_pxCubeGeometry->m_strSourcePath : "";
		xStream << strMeshPath;

		// Materials
		auto WriteMaterial = [&xStream](const MaterialHandle& xMat)
		{
			Zenith_MaterialAsset* pxMat = xMat.Get();
			if (pxMat)
			{
				pxMat->WriteToDataStream(xStream);
			}
			else
			{
				// Write empty material placeholder - use local to avoid registry leak
				Zenith_MaterialAsset xEmptyMat;
				xEmptyMat.SetName("Empty");
				xEmptyMat.WriteToDataStream(xStream);
			}
		};

		WriteMaterial(m_xFloorMaterial);
		WriteMaterial(m_xWallMaterial);
		WriteMaterial(m_xBoxMaterial);
		WriteMaterial(m_xBoxOnTargetMaterial);
		WriteMaterial(m_xPlayerMaterial);
		WriteMaterial(m_xTargetMaterial);
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			std::string strMeshPath;
			xStream >> strMeshPath;
			if (!strMeshPath.empty())
			{
				// Clean up old geometry if we own it
				if (m_bOwnsGeometry && m_pxCubeGeometry)
				{
					delete m_pxCubeGeometry;
				}
				m_pxCubeGeometry = new Flux_MeshGeometry();
				m_bOwnsGeometry = true;
				Flux_MeshGeometry::LoadFromFile(strMeshPath.c_str(), *m_pxCubeGeometry, 0, true);
			}

			auto ReadMaterial = [&xStream](MaterialHandle& xMat, const char* szName)
			{
				Zenith_MaterialAsset* pxLoaded = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
				pxLoaded->SetName(szName);
				pxLoaded->ReadFromDataStream(xStream);
				xMat.Set(pxLoaded);
			};

			ReadMaterial(m_xFloorMaterial, "Sokoban_Floor");
			ReadMaterial(m_xWallMaterial, "Sokoban_Wall");
			ReadMaterial(m_xBoxMaterial, "Sokoban_Box");
			ReadMaterial(m_xBoxOnTargetMaterial, "Sokoban_BoxOnTarget");
			ReadMaterial(m_xPlayerMaterial, "Sokoban_Player");
			ReadMaterial(m_xTargetMaterial, "Sokoban_Target");
		}
	}

private:
	// ========================================================================
	// Input Handling
	// ========================================================================
	void HandleInput()
	{
		if (m_bAnimating) return;

		// Check for reset
		if (Sokoban_Input::WasResetPressed())
		{
			GenerateNewLevel();
			return;
		}

		// Check for movement
		SokobanDirection eDir = Sokoban_Input::GetInputDirection();
		if (eDir != SOKOBAN_DIR_NONE)
		{
			TryMove(eDir);
		}
	}

	// ========================================================================
	// Movement Logic
	// ========================================================================
	bool TryMove(SokobanDirection eDir)
	{
		if (m_bAnimating) return false;

		// Check if move is valid using grid logic module
		if (!Sokoban_GridLogic::CanMove(m_aeTiles, m_abBoxes, m_uPlayerX, m_uPlayerY,
			m_uGridWidth, m_uGridHeight, eDir))
		{
			return false;
		}

		int32_t iDeltaX, iDeltaY;
		Sokoban_GridLogic::GetDirectionDelta(eDir, iDeltaX, iDeltaY);

		uint32_t uOldX = m_uPlayerX;
		uint32_t uOldY = m_uPlayerY;
		uint32_t uNewX = m_uPlayerX + iDeltaX;
		uint32_t uNewY = m_uPlayerY + iDeltaY;
		uint32_t uNewIndex = uNewY * m_uGridWidth + uNewX;

		bool bPushingBox = false;
		uint32_t uBoxDestX = 0, uBoxDestY = 0;

		// Check if pushing a box
		if (m_abBoxes[uNewIndex])
		{
			bPushingBox = true;
			uBoxDestX = uNewX + iDeltaX;
			uBoxDestY = uNewY + iDeltaY;
			Sokoban_GridLogic::PushBox(m_abBoxes, uNewX, uNewY, m_uGridWidth, eDir);
		}

		// Update player position
		m_uPlayerX = uNewX;
		m_uPlayerY = uNewY;
		m_uMoveCount++;

		// Start animation
		StartAnimation(uOldX, uOldY, uNewX, uNewY);
		if (bPushingBox)
		{
			StartBoxAnimation(uNewX, uNewY, uBoxDestX, uBoxDestY);
		}

		UpdateUI();
		return true;
	}

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
			m_fBoxVisualX = static_cast<float>(m_uAnimBoxFromX) +
				(static_cast<float>(m_uAnimBoxToX) - static_cast<float>(m_uAnimBoxFromX)) * fProgress;
			m_fBoxVisualY = static_cast<float>(m_uAnimBoxFromY) +
				(static_cast<float>(m_uAnimBoxToY) - static_cast<float>(m_uAnimBoxFromY)) * fProgress;

			// Emit dust particles while box is moving
			UpdateDustParticles(fDt);
		}

		// Animation complete
		if (fProgress >= 1.f)
		{
			m_bAnimating = false;
			m_bBoxAnimating = false;
			m_fPlayerVisualX = static_cast<float>(m_uPlayerTargetX);
			m_fPlayerVisualY = static_cast<float>(m_uPlayerTargetY);

			// Stop dust emission
			StopDustParticles();

			if (Sokoban_GridLogic::CheckWinCondition(m_abBoxes, m_abTargets,
				m_uGridWidth * m_uGridHeight, m_uTargetCount))
			{
				m_bWon = true;
				UpdateUI();
			}
		}
	}

	void UpdateDustParticles(float /*fDt*/)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		if (Sokoban::g_uDustEmitterID == INVALID_ENTITY_ID ||
			!xScene.EntityExists(Sokoban::g_uDustEmitterID))
		{
			return;
		}

		Zenith_Entity xEmitterEntity = xScene.GetEntity(Sokoban::g_uDustEmitterID);
		if (!xEmitterEntity.HasComponent<Zenith_ParticleEmitterComponent>())
		{
			return;
		}

		Zenith_ParticleEmitterComponent& xEmitter = xEmitterEntity.GetComponent<Zenith_ParticleEmitterComponent>();

		// Calculate box world position (using the same calculation as Sokoban_Rendering)
		float fOffsetX = -static_cast<float>(m_uGridWidth) * 0.5f + 0.5f;
		float fOffsetZ = -static_cast<float>(m_uGridHeight) * 0.5f + 0.5f;
		Zenith_Maths::Vector3 xBoxPos(
			m_fBoxVisualX + fOffsetX,
			0.1f,  // At floor level
			m_fBoxVisualY + fOffsetZ
		);

		// Calculate movement direction for dust
		Zenith_Maths::Vector3 xMoveDir(
			static_cast<float>(m_uAnimBoxToX) - static_cast<float>(m_uAnimBoxFromX),
			0.0f,
			static_cast<float>(m_uAnimBoxToY) - static_cast<float>(m_uAnimBoxFromY)
		);

		// Dust emits perpendicular to movement, at floor level
		Zenith_Maths::Vector3 xDustDir = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

		xEmitter.SetEmitPosition(xBoxPos);
		xEmitter.SetEmitDirection(xDustDir);
		xEmitter.SetEmitting(true);
	}

	void StopDustParticles()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		if (Sokoban::g_uDustEmitterID == INVALID_ENTITY_ID ||
			!xScene.EntityExists(Sokoban::g_uDustEmitterID))
		{
			return;
		}

		Zenith_Entity xEmitterEntity = xScene.GetEntity(Sokoban::g_uDustEmitterID);
		if (xEmitterEntity.HasComponent<Zenith_ParticleEmitterComponent>())
		{
			xEmitterEntity.GetComponent<Zenith_ParticleEmitterComponent>().SetEmitting(false);
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
	// Visual Updates
	// ========================================================================
	void UpdateVisuals()
	{
		float fVisualX = m_bAnimating ? m_fPlayerVisualX : static_cast<float>(m_uPlayerX);
		float fVisualY = m_bAnimating ? m_fPlayerVisualY : static_cast<float>(m_uPlayerY);
		m_xRenderer.UpdatePlayerPosition(fVisualX, fVisualY);

		m_xRenderer.UpdateBoxPositions(m_abBoxes, m_uGridWidth, m_uGridHeight,
			m_bBoxAnimating, m_uAnimBoxToX, m_uAnimBoxToY, m_fBoxVisualX, m_fBoxVisualY);
	}

	// ========================================================================
	// Level Generation
	// ========================================================================
	void GenerateNewLevel()
	{
		// Reset state
		m_uMoveCount = 0;
		m_bWon = false;
		m_bAnimating = false;

		// Use level generator module
		Sokoban_LevelGenerator::LevelData xData;
		xData.aeTiles = m_aeTiles;
		xData.abTargets = m_abTargets;
		xData.abBoxes = m_abBoxes;

		// Try to generate a solvable level
		for (int i = 0; i < 1000; i++)
		{
			Sokoban_LevelGenerator::GenerateLevelAttempt(xData, m_xRng);

			m_uGridWidth = xData.uGridWidth;
			m_uGridHeight = xData.uGridHeight;
			m_uPlayerX = xData.uPlayerX;
			m_uPlayerY = xData.uPlayerY;
			m_uTargetCount = xData.uTargetCount;

			// Validate with solver
			int32_t iMinMoves = Sokoban_Solver::SolveLevel(
				m_aeTiles, m_abBoxes, m_abTargets,
				m_uPlayerX, m_uPlayerY, m_uGridWidth, m_uGridHeight);

			if (iMinMoves >= 5) // Minimum complexity
			{
				m_uMinMoves = static_cast<uint32_t>(iMinMoves);
				break;
			}
		}

		// Initialize visual positions
		m_fPlayerVisualX = static_cast<float>(m_uPlayerX);
		m_fPlayerVisualY = static_cast<float>(m_uPlayerY);

		// Create 3D entities using renderer module
		m_xRenderer.Create3DLevel(
			m_uGridWidth, m_uGridHeight,
			m_aeTiles, m_abBoxes, m_abTargets,
			m_uPlayerX, m_uPlayerY,
			Sokoban::g_pxTilePrefab, Sokoban::g_pxBoxPrefab, Sokoban::g_pxPlayerPrefab,
			m_pxCubeGeometry,
			m_xFloorMaterial.Get(), m_xWallMaterial.Get(), m_xTargetMaterial.Get(),
			m_xBoxMaterial.Get(), m_xBoxOnTargetMaterial.Get(), m_xPlayerMaterial.Get());

		m_xRenderer.RepositionCamera(m_uGridWidth, m_uGridHeight);
		UpdateUI();
	}

	// ========================================================================
	// UI Management
	// ========================================================================
	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		uint32_t uBoxesOnTargets = Sokoban_GridLogic::CountBoxesOnTargets(
			m_abBoxes, m_abTargets, m_uGridWidth * m_uGridHeight);

		Sokoban_UIManager::UpdateStatusText(xUI, m_uMoveCount, uBoxesOnTargets,
			m_uTargetCount, m_uMinMoves, m_bWon);
	}

	// ========================================================================
	// Editor Helpers
	// ========================================================================
#ifdef ZENITH_TOOLS
	void RenderMaterialSlot(const char* szLabel, MaterialHandle& xMaterial)
	{
		ImGui::PushID(szLabel);
		Zenith_MaterialAsset* pxMat = xMaterial.Get();
		std::string strMaterialName = pxMat ? pxMat->GetName() : "(none)";
		ImGui::Text("%s:", szLabel);
		ImGui::SameLine();
		ImVec2 xButtonSize(150, 20);
		ImGui::Button(strMaterialName.c_str(), xButtonSize);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MATERIAL))
			{
				const DragDropFilePayload* pFilePayload =
					static_cast<const DragDropFilePayload*>(pPayload->Data);
				// Use SetPath for file-based assets so the path is stored for serialization
				xMaterial.SetPath(pFilePayload->m_szFilePath);
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Drop a .zmat material file here");
		}
		ImGui::PopID();
	}

	void RenderMeshSlot(const char* szLabel, Flux_MeshGeometry*& pxMesh)
	{
		ImGui::PushID(szLabel);
		std::string strMeshName = pxMesh ? "(loaded)" : "(none)";
		if (pxMesh && !pxMesh->m_strSourcePath.empty())
		{
			std::filesystem::path xPath(pxMesh->m_strSourcePath);
			strMeshName = xPath.filename().string();
		}
		ImGui::Text("%s:", szLabel);
		ImGui::SameLine();
		ImVec2 xButtonSize(150, 20);
		ImGui::Button(strMeshName.c_str(), xButtonSize);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MESH))
			{
				const DragDropFilePayload* pFilePayload =
					static_cast<const DragDropFilePayload*>(pPayload->Data);
				Flux_MeshGeometry* pxNewMesh = new Flux_MeshGeometry();
				Flux_MeshGeometry::LoadFromFile(pFilePayload->m_szFilePath, *pxNewMesh, 0, true);
				if (pxNewMesh->GetNumVerts() > 0)
				{
					// Clean up old geometry if we own it
					if (m_bOwnsGeometry && pxMesh)
					{
						delete pxMesh;
					}
					pxMesh = pxNewMesh;
					m_bOwnsGeometry = true;
				}
				else
				{
					delete pxNewMesh;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Drop a .zmesh mesh file here");
		}
		ImGui::PopID();
	}
#endif

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Grid state
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

	// Renderer module instance
	Sokoban_Renderer m_xRenderer;

public:
	// Resource pointers
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	bool m_bOwnsGeometry = false;  // true if we allocated m_pxCubeGeometry
	MaterialHandle m_xFloorMaterial;
	MaterialHandle m_xWallMaterial;
	MaterialHandle m_xBoxMaterial;
	MaterialHandle m_xBoxOnTargetMaterial;
	MaterialHandle m_xPlayerMaterial;
	MaterialHandle m_xTargetMaterial;
};
