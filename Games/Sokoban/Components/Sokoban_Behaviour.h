#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_Input.h"
#include "Flux/Quads/Flux_Quads.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

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

// Level encoding: 0=Floor, 1=Wall, 2=Target, 3=Box, 4=Player
static const uint8_t s_aDefaultLevel[64] =
{
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 3, 0, 2, 0, 0, 1,
	1, 0, 0, 1, 1, 0, 0, 1,
	1, 0, 3, 2, 0, 3, 0, 1,
	1, 0, 0, 0, 2, 0, 4, 1,
	1, 0, 0, 0, 0, 0, 0, 1,
	1, 1, 1, 1, 1, 1, 1, 1
};

class Sokoban_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Sokoban_Behaviour)

	static constexpr uint32_t s_uGridSize = 8;
	static constexpr uint32_t s_uTileSize = 64;
	static constexpr uint32_t s_uGridOffsetX = 100;
	static constexpr uint32_t s_uGridOffsetY = 100;

	Sokoban_Behaviour() = delete;
	Sokoban_Behaviour(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
		, m_uPlayerX(0)
		, m_uPlayerY(0)
		, m_uMoveCount(0)
		, m_uTargetCount(0)
		, m_bWon(false)
	{
		memset(m_aeTiles, 0, sizeof(m_aeTiles));
		memset(m_abTargets, false, sizeof(m_abTargets));
		memset(m_abBoxes, false, sizeof(m_abBoxes));
	}
	~Sokoban_Behaviour() = default;

	void OnCreate() ZENITH_FINAL override
	{
		LoadLevel(s_aDefaultLevel);
		SetupUI();
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		if (!m_bWon)
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
		ImGui::Text("Moves: %u", m_uMoveCount);
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
	void HandleKeyboardInput()
	{
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

	bool TryMove(SokobanDirection eDir)
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

		uint32_t uNewX = m_uPlayerX + iDeltaX;
		uint32_t uNewY = m_uPlayerY + iDeltaY;

		if (uNewX >= s_uGridSize || uNewY >= s_uGridSize)
		{
			return false;
		}

		uint32_t uNewIndex = uNewY * s_uGridSize + uNewX;

		if (m_aeTiles[uNewIndex] == SOKOBAN_TILE_WALL)
		{
			return false;
		}

		if (m_abBoxes[uNewIndex])
		{
			if (!CanPushBox(uNewX, uNewY, eDir))
			{
				return false;
			}
			PushBox(uNewX, uNewY, eDir);
		}

		m_uPlayerX = uNewX;
		m_uPlayerY = uNewY;
		m_uMoveCount++;

		if (CheckWinCondition())
		{
			m_bWon = true;
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

		if (uDestX >= s_uGridSize || uDestY >= s_uGridSize)
		{
			return false;
		}

		uint32_t uDestIndex = uDestY * s_uGridSize + uDestX;

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

		uint32_t uFromIndex = uFromY * s_uGridSize + uFromX;
		uint32_t uToX = uFromX + iDeltaX;
		uint32_t uToY = uFromY + iDeltaY;
		uint32_t uToIndex = uToY * s_uGridSize + uToX;

		m_abBoxes[uFromIndex] = false;
		m_abBoxes[uToIndex] = true;
	}

	void RenderGame()
	{
		for (uint32_t uY = 0; uY < s_uGridSize; uY++)
		{
			for (uint32_t uX = 0; uX < s_uGridSize; uX++)
			{
				uint32_t uIndex = uY * s_uGridSize + uX;

				RenderTile(uX, uY, m_aeTiles[uIndex]);

				if (m_abTargets[uIndex] && m_aeTiles[uIndex] != SOKOBAN_TILE_WALL)
				{
					RenderTargetMarker(uX, uY);
				}
			}
		}

		for (uint32_t uY = 0; uY < s_uGridSize; uY++)
		{
			for (uint32_t uX = 0; uX < s_uGridSize; uX++)
			{
				uint32_t uIndex = uY * s_uGridSize + uX;
				if (m_abBoxes[uIndex])
				{
					SokobanTileType eBoxType = m_abTargets[uIndex]
						? SOKOBAN_TILE_BOX_ON_TARGET
						: SOKOBAN_TILE_BOX;
					RenderTile(uX, uY, eBoxType);
				}
			}
		}

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
		uint32_t uScreenX = s_uGridOffsetX + m_uPlayerX * s_uTileSize;
		uint32_t uScreenY = s_uGridOffsetY + m_uPlayerY * s_uTileSize;

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
		uint32_t uMsgX = s_uGridOffsetX + (s_uGridSize * s_uTileSize - uMsgWidth) / 2;
		uint32_t uMsgY = s_uGridOffsetY + (s_uGridSize * s_uTileSize - uMsgHeight) / 2;

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

	void LoadLevel(const uint8_t* pLevelData)
	{
		memset(m_aeTiles, 0, sizeof(m_aeTiles));
		memset(m_abTargets, false, sizeof(m_abTargets));
		memset(m_abBoxes, false, sizeof(m_abBoxes));
		m_uMoveCount = 0;
		m_uTargetCount = 0;
		m_bWon = false;

		for (uint32_t i = 0; i < s_uGridSize * s_uGridSize; i++)
		{
			switch (pLevelData[i])
			{
			case 0:
				m_aeTiles[i] = SOKOBAN_TILE_FLOOR;
				break;
			case 1:
				m_aeTiles[i] = SOKOBAN_TILE_WALL;
				break;
			case 2:
				m_aeTiles[i] = SOKOBAN_TILE_FLOOR;
				m_abTargets[i] = true;
				m_uTargetCount++;
				break;
			case 3:
				m_aeTiles[i] = SOKOBAN_TILE_FLOOR;
				m_abBoxes[i] = true;
				break;
			case 4:
				m_aeTiles[i] = SOKOBAN_TILE_FLOOR;
				m_uPlayerX = i % s_uGridSize;
				m_uPlayerY = i / s_uGridSize;
				break;
			}
		}
	}

	void ResetLevel()
	{
		LoadLevel(s_aDefaultLevel);
		UpdateStatusText();
	}

	void SetupUI()
	{
		Zenith_UIComponent& xUI = m_xParentEntity.AddComponent<Zenith_UIComponent>();

		static constexpr uint32_t s_uTextStartX = s_uGridOffsetX + s_uGridSize * s_uTileSize + 50;
		static constexpr uint32_t s_uTextStartY = s_uGridOffsetY;
		static constexpr float s_fBaseTextSize = 15.f;
		static constexpr float s_fLineHeight = 24.f;

		Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "SOKOBAN");
		pxTitle->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY));
		pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
		pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
		pxControls->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 2);
		pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
		pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

		Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "WASD / Arrows: Move");
		pxMove->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 3);
		pxMove->SetFontSize(s_fBaseTextSize * 3.0f);
		pxMove->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxMouse = xUI.CreateText("MouseInstr", "Mouse Click: Move");
		pxMouse->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 4);
		pxMouse->SetFontSize(s_fBaseTextSize * 3.0f);
		pxMouse->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxReset = xUI.CreateText("ResetInstr", "R: Reset Level");
		pxReset->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 5);
		pxReset->SetFontSize(s_fBaseTextSize * 3.0f);
		pxReset->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxGoal = xUI.CreateText("GoalHeader", "Goal:");
		pxGoal->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 7);
		pxGoal->SetFontSize(s_fBaseTextSize * 3.6f);
		pxGoal->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

		Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Push boxes onto targets");
		pxGoalDesc->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 8);
		pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
		pxGoalDesc->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Moves: 0");
		pxStatus->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 10);
		pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
		pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Boxes: 0 / 3");
		pxProgress->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 11);
		pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
		pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
		pxWin->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 13);
		pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
		pxWin->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));
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
	}

	bool CheckWinCondition() const
	{
		return CountBoxesOnTargets() == m_uTargetCount && m_uTargetCount > 0;
	}

	uint32_t CountBoxesOnTargets() const
	{
		uint32_t uCount = 0;
		for (uint32_t i = 0; i < s_uGridSize * s_uGridSize; i++)
		{
			if (m_abBoxes[i] && m_abTargets[i])
			{
				uCount++;
			}
		}
		return uCount;
	}

	Zenith_Entity m_xParentEntity;
	SokobanTileType m_aeTiles[s_uGridSize * s_uGridSize];
	bool m_abTargets[s_uGridSize * s_uGridSize];
	bool m_abBoxes[s_uGridSize * s_uGridSize];
	uint32_t m_uPlayerX;
	uint32_t m_uPlayerY;
	uint32_t m_uMoveCount;
	uint32_t m_uTargetCount;
	bool m_bWon;
};
