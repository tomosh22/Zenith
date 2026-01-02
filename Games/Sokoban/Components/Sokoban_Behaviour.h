#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_Input.h"
#include "Flux/Quads/Flux_Quads.h"

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

	static constexpr uint32_t s_uTileSize = 64;
	static constexpr uint32_t s_uGridOffsetX = 100;
	static constexpr uint32_t s_uGridOffsetY = 100;
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
			HandleMouseInput();
		}
		RenderGame();
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

	void HandleMouseInput()
	{
		if (m_bAnimating) return;

		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
		{
			SokobanDirection eDir = GetDirectionFromMouse();
			if (eDir != SOKOBAN_DIR_NONE)
			{
				TryMove(eDir);
			}
		}
	}

	SokobanDirection GetDirectionFromMouse() const
	{
		Zenith_Maths::Vector2_64 xMousePos;
		Zenith_Input::GetMousePosition(xMousePos);

		float fPlayerCenterX = static_cast<float>(s_uGridOffsetX + m_uPlayerX * s_uTileSize) + s_uTileSize * 0.5f;
		float fPlayerCenterY = static_cast<float>(s_uGridOffsetY + m_uPlayerY * s_uTileSize) + s_uTileSize * 0.5f;

		float fDeltaX = static_cast<float>(xMousePos.x) - fPlayerCenterX;
		float fDeltaY = static_cast<float>(xMousePos.y) - fPlayerCenterY;

		if (fabs(fDeltaX) > fabs(fDeltaY))
		{
			return (fDeltaX > 0) ? SOKOBAN_DIR_RIGHT : SOKOBAN_DIR_LEFT;
		}
		else if (fabs(fDeltaY) > fabs(fDeltaX))
		{
			return (fDeltaY > 0) ? SOKOBAN_DIR_DOWN : SOKOBAN_DIR_UP;
		}

		return SOKOBAN_DIR_NONE;
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
	// Rendering
	// ========================================================================
	void RenderGame()
	{
		// Render tiles and targets
		for (uint32_t uY = 0; uY < m_uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;

				RenderTile(uX, uY, m_aeTiles[uIndex]);

				if (m_abTargets[uIndex] && m_aeTiles[uIndex] != SOKOBAN_TILE_WALL)
				{
					RenderTargetMarker(uX, uY);
				}
			}
		}

		// Render boxes (non-animating)
		for (uint32_t uY = 0; uY < m_uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
			{
				uint32_t uIndex = uY * m_uGridWidth + uX;
				if (m_abBoxes[uIndex])
				{
					// Skip the animating box - we'll render it separately
					if (m_bBoxAnimating && uX == m_uAnimBoxToX && uY == m_uAnimBoxToY)
					{
						continue;
					}
					SokobanTileType eBoxType = m_abTargets[uIndex]
						? SOKOBAN_TILE_BOX_ON_TARGET
						: SOKOBAN_TILE_BOX;
					RenderTile(uX, uY, eBoxType);
				}
			}
		}

		// Render animating box
		if (m_bBoxAnimating)
		{
			uint32_t uToIndex = m_uAnimBoxToY * m_uGridWidth + m_uAnimBoxToX;
			SokobanTileType eBoxType = m_abTargets[uToIndex]
				? SOKOBAN_TILE_BOX_ON_TARGET
				: SOKOBAN_TILE_BOX;
			RenderTileAtPosition(m_fBoxVisualX, m_fBoxVisualY, eBoxType);
		}

		// Render player
		RenderPlayer();

		if (m_bWon)
		{
			RenderWinMessage();
		}
	}

	void RenderTile(uint32_t uGridX, uint32_t uGridY, SokobanTileType eTile)
	{
		uint32_t uScreenX = s_uGridOffsetX + uGridX * s_uTileSize;
		uint32_t uScreenY = s_uGridOffsetY + uGridY * s_uTileSize;

		Zenith_Maths::Vector4 xColor = GetTileColor(eTile);

		uint32_t uPadding = 2;

		Flux_Quads::Quad xQuad(
			Zenith_Maths::UVector4(
				uScreenX + uPadding,
				uScreenY + uPadding,
				s_uTileSize - uPadding * 2,
				s_uTileSize - uPadding * 2
			),
			xColor,
			0,
			Zenith_Maths::Vector2(1.0f, 0.0f)
		);

		Flux_Quads::UploadQuad(xQuad);
	}

	void RenderTileAtPosition(float fGridX, float fGridY, SokobanTileType eTile)
	{
		uint32_t uScreenX = s_uGridOffsetX + static_cast<uint32_t>(fGridX * s_uTileSize);
		uint32_t uScreenY = s_uGridOffsetY + static_cast<uint32_t>(fGridY * s_uTileSize);

		Zenith_Maths::Vector4 xColor = GetTileColor(eTile);

		uint32_t uPadding = 2;

		Flux_Quads::Quad xQuad(
			Zenith_Maths::UVector4(
				uScreenX + uPadding,
				uScreenY + uPadding,
				s_uTileSize - uPadding * 2,
				s_uTileSize - uPadding * 2
			),
			xColor,
			0,
			Zenith_Maths::Vector2(1.0f, 0.0f)
		);

		Flux_Quads::UploadQuad(xQuad);
	}

	void RenderTargetMarker(uint32_t uGridX, uint32_t uGridY)
	{
		uint32_t uScreenX = s_uGridOffsetX + uGridX * s_uTileSize;
		uint32_t uScreenY = s_uGridOffsetY + uGridY * s_uTileSize;

		uint32_t uMarkerSize = 16;
		uint32_t uOffset = (s_uTileSize - uMarkerSize) / 2;

		Flux_Quads::Quad xQuad(
			Zenith_Maths::UVector4(
				uScreenX + uOffset,
				uScreenY + uOffset,
				uMarkerSize,
				uMarkerSize
			),
			GetTileColor(SOKOBAN_TILE_TARGET),
			0,
			Zenith_Maths::Vector2(1.0f, 0.0f)
		);

		Flux_Quads::UploadQuad(xQuad);
	}

	void RenderPlayer()
	{
		float fGridX = m_bAnimating ? m_fPlayerVisualX : static_cast<float>(m_uPlayerX);
		float fGridY = m_bAnimating ? m_fPlayerVisualY : static_cast<float>(m_uPlayerY);

		uint32_t uScreenX = s_uGridOffsetX + static_cast<uint32_t>(fGridX * s_uTileSize);
		uint32_t uScreenY = s_uGridOffsetY + static_cast<uint32_t>(fGridY * s_uTileSize);

		uint32_t uPlayerPadding = 8;

		Flux_Quads::Quad xQuad(
			Zenith_Maths::UVector4(
				uScreenX + uPlayerPadding,
				uScreenY + uPlayerPadding,
				s_uTileSize - uPlayerPadding * 2,
				s_uTileSize - uPlayerPadding * 2
			),
			GetTileColor(SOKOBAN_TILE_PLAYER),
			0,
			Zenith_Maths::Vector2(1.0f, 0.0f)
		);

		Flux_Quads::UploadQuad(xQuad);
	}

	void RenderWinMessage()
	{
		uint32_t uMsgWidth = 300;
		uint32_t uMsgHeight = 60;
		uint32_t uMsgX = s_uGridOffsetX + (m_uGridWidth * s_uTileSize - uMsgWidth) / 2;
		uint32_t uMsgY = s_uGridOffsetY + (m_uGridHeight * s_uTileSize - uMsgHeight) / 2;

		Flux_Quads::Quad xQuad(
			Zenith_Maths::UVector4(uMsgX, uMsgY, uMsgWidth, uMsgHeight),
			Zenith_Maths::Vector4(0.1f, 0.7f, 0.1f, 0.9f),
			0,
			Zenith_Maths::Vector2(1.0f, 0.0f)
		);

		Flux_Quads::UploadQuad(xQuad);
	}

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
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			return;
		}

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Calculate text X position based on grid size
		uint32_t uGridPixelWidth = m_uGridWidth * s_uTileSize;
		float fTextStartX = static_cast<float>(s_uGridOffsetX + uGridPixelWidth + 50);

		// Update all text elements' X position
		const char* aElementNames[] = {
			"Title", "ControlsHeader", "MoveInstr", "MouseInstr", "ResetInstr",
			"GoalHeader", "GoalDesc", "Status", "Progress", "WinText", "MinMoves"
		};

		for (const char* szName : aElementNames)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText)
			{
				Zenith_Maths::Vector2 xPos = pxText->GetPosition();
				pxText->SetPosition(fTextStartX, xPos.y);
			}
		}
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
};
