#pragma once
/**
 * Exploration_UIManager.h - HUD management
 *
 * Demonstrates:
 * - Minimal HUD overlay (coordinates, time of day, FPS)
 * - Debug information toggle
 * - UI anchoring
 * - Dynamic text updates
 *
 * Engine APIs used:
 * - Zenith_UIComponent
 * - Zenith_UIText
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "Maths/Zenith_Maths.h"

#include <cstdio>

namespace Exploration_UIManager
{
	// ========================================================================
	// UI Element Names
	// ========================================================================
	static constexpr const char* UI_TIME = "Time";
	static constexpr const char* UI_POSITION = "Position";
	static constexpr const char* UI_CHUNK = "Chunk";
	static constexpr const char* UI_WEATHER = "Weather";
	static constexpr const char* UI_FPS = "FPS";
	static constexpr const char* UI_CONTROLS = "Controls";
	static constexpr const char* UI_LOADING = "Loading";
	static constexpr const char* UI_TERRAIN_LOD = "TerrainLOD";
	static constexpr const char* UI_STREAMING = "Streaming";

	// ========================================================================
	// Configuration
	// ========================================================================
	static constexpr float s_fMarginLeft = 20.0f;
	static constexpr float s_fMarginTop = 20.0f;
	static constexpr float s_fLineHeight = 22.0f;
	static constexpr float s_fFontSize = 14.0f;
	static constexpr float s_fTitleFontSize = 16.0f;

	static bool s_bShowDebugHUD = false;
	static bool s_bShowControls = true;

	/**
	 * Create UI elements for the HUD
	 */
	inline void CreateUI(Zenith_UIComponent& xUI)
	{
		float fYOffset = 0.0f;
		Zenith_Maths::Vector4 xWhite(1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_Maths::Vector4 xGray(0.7f, 0.7f, 0.7f, 1.0f);
		Zenith_Maths::Vector4 xYellow(1.0f, 0.9f, 0.5f, 1.0f);
		Zenith_Maths::Vector4 xCyan(0.5f, 0.9f, 1.0f, 1.0f);

		// Time display
		Zenith_UI::Zenith_UIText* pxTime = xUI.CreateText(UI_TIME, "Time: 06:00");
		pxTime->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxTime->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxTime->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxTime->SetFontSize(s_fTitleFontSize);
		pxTime->SetColor(xYellow);
		fYOffset += s_fLineHeight;

		// Weather display
		Zenith_UI::Zenith_UIText* pxWeather = xUI.CreateText(UI_WEATHER, "Weather: Clear");
		pxWeather->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxWeather->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxWeather->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxWeather->SetFontSize(s_fFontSize);
		pxWeather->SetColor(xWhite);
		fYOffset += s_fLineHeight;

		// Position display
		Zenith_UI::Zenith_UIText* pxPosition = xUI.CreateText(UI_POSITION, "Position: 0, 0, 0");
		pxPosition->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxPosition->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxPosition->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxPosition->SetFontSize(s_fFontSize);
		pxPosition->SetColor(xGray);
		fYOffset += s_fLineHeight;

		// Chunk display
		Zenith_UI::Zenith_UIText* pxChunk = xUI.CreateText(UI_CHUNK, "Chunk: 0, 0");
		pxChunk->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxChunk->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxChunk->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxChunk->SetFontSize(s_fFontSize);
		pxChunk->SetColor(xGray);
		fYOffset += s_fLineHeight;

		// Terrain LOD display (debug)
		Zenith_UI::Zenith_UIText* pxTerrainLOD = xUI.CreateText(UI_TERRAIN_LOD, "");
		pxTerrainLOD->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxTerrainLOD->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxTerrainLOD->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxTerrainLOD->SetFontSize(s_fFontSize);
		pxTerrainLOD->SetColor(xCyan);
		fYOffset += s_fLineHeight;

		// Streaming stats (debug)
		Zenith_UI::Zenith_UIText* pxStreaming = xUI.CreateText(UI_STREAMING, "");
		pxStreaming->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxStreaming->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxStreaming->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxStreaming->SetFontSize(s_fFontSize);
		pxStreaming->SetColor(xCyan);
		fYOffset += s_fLineHeight;

		// FPS counter (top right)
		Zenith_UI::Zenith_UIText* pxFPS = xUI.CreateText(UI_FPS, "FPS: 60");
		pxFPS->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxFPS->SetPosition(-s_fMarginLeft, s_fMarginTop);
		pxFPS->SetAlignment(Zenith_UI::TextAlignment::Right);
		pxFPS->SetFontSize(s_fFontSize);
		pxFPS->SetColor(xWhite);

		// Loading status (top right, below FPS)
		Zenith_UI::Zenith_UIText* pxLoading = xUI.CreateText(UI_LOADING, "");
		pxLoading->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxLoading->SetPosition(-s_fMarginLeft, s_fMarginTop + s_fLineHeight);
		pxLoading->SetAlignment(Zenith_UI::TextAlignment::Right);
		pxLoading->SetFontSize(s_fFontSize);
		pxLoading->SetColor(xYellow);

		// Controls hint (bottom left)
		Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText(UI_CONTROLS,
			"WASD: Move | Mouse: Look | Shift: Sprint | Tab: Debug | Esc: Menu");
		pxControls->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
		pxControls->SetPosition(s_fMarginLeft, -s_fMarginTop);
		pxControls->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxControls->SetFontSize(s_fFontSize * 0.9f);
		pxControls->SetColor(xGray);
	}

	/**
	 * Update time display
	 */
	inline void UpdateTime(Zenith_UIComponent& xUI, const char* szTimeStr)
	{
		Zenith_UI::Zenith_UIText* pxTime = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_TIME);
		if (pxTime)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Time: %s", szTimeStr);
			pxTime->SetText(szBuffer);
		}
	}

	/**
	 * Update weather display
	 */
	inline void UpdateWeather(Zenith_UIComponent& xUI, const char* szWeatherName, float fTransition)
	{
		Zenith_UI::Zenith_UIText* pxWeather = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_WEATHER);
		if (pxWeather)
		{
			char szBuffer[64];
			if (fTransition < 1.0f)
			{
				snprintf(szBuffer, sizeof(szBuffer), "Weather: %s (%.0f%%)",
					szWeatherName, fTransition * 100.0f);
			}
			else
			{
				snprintf(szBuffer, sizeof(szBuffer), "Weather: %s", szWeatherName);
			}
			pxWeather->SetText(szBuffer);
		}
	}

	/**
	 * Update position display
	 */
	inline void UpdatePosition(Zenith_UIComponent& xUI, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_UI::Zenith_UIText* pxPosition = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_POSITION);
		if (pxPosition)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Position: %.0f, %.0f, %.0f",
				xPos.x, xPos.y, xPos.z);
			pxPosition->SetText(szBuffer);
		}
	}

	/**
	 * Update chunk display
	 */
	inline void UpdateChunk(Zenith_UIComponent& xUI, int32_t iChunkX, int32_t iChunkY)
	{
		Zenith_UI::Zenith_UIText* pxChunk = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_CHUNK);
		if (pxChunk)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Chunk: %d, %d", iChunkX, iChunkY);
			pxChunk->SetText(szBuffer);
		}
	}

	/**
	 * Update terrain LOD display
	 */
	inline void UpdateTerrainLOD(Zenith_UIComponent& xUI, const char* szLODName, uint32_t uResidentLOD)
	{
		Zenith_UI::Zenith_UIText* pxTerrainLOD = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_TERRAIN_LOD);
		if (pxTerrainLOD)
		{
			if (s_bShowDebugHUD)
			{
				char szBuffer[64];
				snprintf(szBuffer, sizeof(szBuffer), "Terrain LOD: %s (Resident: LOD%u)",
					szLODName, uResidentLOD);
				pxTerrainLOD->SetText(szBuffer);
			}
			else
			{
				pxTerrainLOD->SetText("");
			}
		}
	}

	/**
	 * Update streaming statistics display
	 */
	inline void UpdateStreaming(Zenith_UIComponent& xUI,
		float fVertexUsedMB, float fVertexTotalMB,
		uint32_t uHighLODChunks, uint32_t uStreamsPerFrame)
	{
		Zenith_UI::Zenith_UIText* pxStreaming = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_STREAMING);
		if (pxStreaming)
		{
			if (s_bShowDebugHUD)
			{
				char szBuffer[128];
				snprintf(szBuffer, sizeof(szBuffer),
					"Streaming: %.0f/%.0f MB | HiLOD: %u | Rate: %u/frame",
					fVertexUsedMB, fVertexTotalMB, uHighLODChunks, uStreamsPerFrame);
				pxStreaming->SetText(szBuffer);
			}
			else
			{
				pxStreaming->SetText("");
			}
		}
	}

	/**
	 * Update FPS display
	 */
	inline void UpdateFPS(Zenith_UIComponent& xUI, float fFPS)
	{
		Zenith_UI::Zenith_UIText* pxFPS = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_FPS);
		if (pxFPS)
		{
			char szBuffer[32];
			snprintf(szBuffer, sizeof(szBuffer), "FPS: %.0f", fFPS);
			pxFPS->SetText(szBuffer);

			// Color based on FPS
			if (fFPS >= 55.0f)
			{
				pxFPS->SetColor(Zenith_Maths::Vector4(0.3f, 1.0f, 0.3f, 1.0f));  // Green
			}
			else if (fFPS >= 30.0f)
			{
				pxFPS->SetColor(Zenith_Maths::Vector4(1.0f, 1.0f, 0.3f, 1.0f));  // Yellow
			}
			else
			{
				pxFPS->SetColor(Zenith_Maths::Vector4(1.0f, 0.3f, 0.3f, 1.0f));  // Red
			}
		}
	}

	/**
	 * Update loading status display
	 */
	inline void UpdateLoading(Zenith_UIComponent& xUI, const char* szStatus)
	{
		Zenith_UI::Zenith_UIText* pxLoading = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_LOADING);
		if (pxLoading)
		{
			pxLoading->SetText(szStatus);
		}
	}

	/**
	 * Toggle debug HUD visibility
	 */
	inline void ToggleDebugHUD()
	{
		s_bShowDebugHUD = !s_bShowDebugHUD;
	}

	/**
	 * Set debug HUD visibility
	 */
	inline void SetDebugHUDVisible(bool bVisible)
	{
		s_bShowDebugHUD = bVisible;
	}

	/**
	 * Check if debug HUD is visible
	 */
	inline bool IsDebugHUDVisible()
	{
		return s_bShowDebugHUD;
	}

	/**
	 * Set controls hint visibility
	 */
	inline void SetControlsVisible(Zenith_UIComponent& xUI, bool bVisible)
	{
		s_bShowControls = bVisible;
		Zenith_UI::Zenith_UIText* pxControls = xUI.FindElement<Zenith_UI::Zenith_UIText>(UI_CONTROLS);
		if (pxControls)
		{
			if (bVisible)
			{
				pxControls->SetText("WASD: Move | Mouse: Look | Shift: Sprint | Tab: Debug | Esc: Menu");
			}
			else
			{
				pxControls->SetText("");
			}
		}
	}

	/**
	 * Update all HUD elements at once
	 */
	inline void UpdateAll(
		Zenith_UIComponent& xUI,
		const char* szTimeStr,
		const char* szWeatherName,
		float fWeatherTransition,
		const Zenith_Maths::Vector3& xPosition,
		int32_t iChunkX,
		int32_t iChunkY,
		const char* szLODName,
		uint32_t uResidentLOD,
		float fVertexUsedMB,
		float fVertexTotalMB,
		uint32_t uHighLODChunks,
		uint32_t uStreamsPerFrame,
		float fFPS,
		const char* szLoadingStatus)
	{
		UpdateTime(xUI, szTimeStr);
		UpdateWeather(xUI, szWeatherName, fWeatherTransition);
		UpdatePosition(xUI, xPosition);
		UpdateChunk(xUI, iChunkX, iChunkY);
		UpdateTerrainLOD(xUI, szLODName, uResidentLOD);
		UpdateStreaming(xUI, fVertexUsedMB, fVertexTotalMB, uHighLODChunks, uStreamsPerFrame);
		UpdateFPS(xUI, fFPS);
		UpdateLoading(xUI, szLoadingStatus);
	}

} // namespace Exploration_UIManager
