#pragma once
/**
 * Exploration_Config.h - DataAsset for Exploration game configuration
 *
 * This demonstrates the DataAsset system for game settings.
 * Settings can be serialized to .zdata files for designer tweaking.
 *
 * Usage:
 *   Exploration_Config* pxConfig = Zenith_DataAssetManager::LoadDataAsset<Exploration_Config>("path.zdata");
 *   // Or create programmatically:
 *   Exploration_Config* pxConfig = Zenith_DataAssetManager::CreateDataAsset<Exploration_Config>();
 */

#include "AssetHandling/Zenith_DataAsset.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

class Exploration_Config : public Zenith_DataAsset
{
public:
	ZENITH_DATA_ASSET_TYPE_NAME(Exploration_Config)

	// ========================================================================
	// Player Movement Settings
	// ========================================================================
	float m_fMoveSpeed = 10.0f;           // Base walking speed (units/second)
	float m_fSprintMultiplier = 2.5f;     // Sprint speed multiplier
	float m_fMouseSensitivity = 0.002f;   // Mouse look sensitivity
	float m_fPlayerEyeHeight = 1.8f;      // Height above terrain
	float m_fGravity = 20.0f;             // Gravity strength for jumping
	float m_fJumpVelocity = 8.0f;         // Initial jump velocity

	// ========================================================================
	// Camera Settings
	// ========================================================================
	float m_fFOV = 70.0f;                 // Field of view (degrees)
	float m_fNearPlane = 0.1f;            // Near clipping plane
	float m_fFarPlane = 5000.0f;          // Far clipping plane (large for terrain)
	float m_fPitchLimit = 1.4f;           // Max pitch angle (radians, ~80 degrees)

	// ========================================================================
	// Day/Night Cycle Settings
	// ========================================================================
	float m_fDayCycleDuration = 600.0f;   // Real-time seconds for full day cycle
	float m_fStartTimeOfDay = 0.25f;      // Starting time (0.0-1.0, 0.25 = 6AM)
	bool m_bDayCycleEnabled = true;       // Enable day/night cycle

	// ========================================================================
	// Sun/Light Settings
	// ========================================================================
	float m_fSunIntensity = 1.0f;         // Base sun intensity
	float m_fAmbientIntensity = 0.15f;    // Ambient light when sun is up
	float m_fNightAmbient = 0.02f;        // Ambient light at night

	// Sun color temperatures (warm at sunrise/sunset, cool at midday)
	float m_afSunriseColor[3] = { 1.0f, 0.6f, 0.3f };
	float m_afMiddayColor[3] = { 1.0f, 0.98f, 0.95f };
	float m_afSunsetColor[3] = { 1.0f, 0.5f, 0.2f };
	float m_afNightColor[3] = { 0.1f, 0.1f, 0.2f };

	// ========================================================================
	// Fog Settings
	// ========================================================================
	float m_fFogDensityBase = 0.00015f;   // Base fog density (clear weather)
	float m_fFogDensityFoggy = 0.0015f;   // Fog density (foggy weather)
	float m_fFogTransitionSpeed = 0.5f;   // How fast fog transitions
	float m_afFogColorDay[3] = { 0.7f, 0.8f, 0.9f };
	float m_afFogColorNight[3] = { 0.02f, 0.02f, 0.05f };
	float m_afFogColorSunrise[3] = { 0.9f, 0.7f, 0.5f };

	// ========================================================================
	// Weather Settings
	// ========================================================================
	float m_fWeatherChangeInterval = 120.0f;  // Seconds between weather changes
	float m_fWeatherTransitionDuration = 30.0f; // Seconds for weather transition
	bool m_bRandomWeather = true;             // Enable random weather changes

	// ========================================================================
	// Terrain Settings
	// ========================================================================
	float m_fTerrainScale = 1.0f;         // World scale multiplier

	// ========================================================================
	// Debug Settings
	// ========================================================================
	bool m_bShowDebugHUD = false;         // Show debug information
	bool m_bShowTerrainLOD = false;       // Visualize terrain LOD levels
	bool m_bShowStreamingStats = false;   // Show streaming statistics

	// ========================================================================
	// Serialization
	// ========================================================================
	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// Player movement
		xStream << m_fMoveSpeed;
		xStream << m_fSprintMultiplier;
		xStream << m_fMouseSensitivity;
		xStream << m_fPlayerEyeHeight;
		xStream << m_fGravity;
		xStream << m_fJumpVelocity;

		// Camera
		xStream << m_fFOV;
		xStream << m_fNearPlane;
		xStream << m_fFarPlane;
		xStream << m_fPitchLimit;

		// Day/Night cycle
		xStream << m_fDayCycleDuration;
		xStream << m_fStartTimeOfDay;
		xStream << m_bDayCycleEnabled;

		// Sun/Light
		xStream << m_fSunIntensity;
		xStream << m_fAmbientIntensity;
		xStream << m_fNightAmbient;
		xStream << m_afSunriseColor[0]; xStream << m_afSunriseColor[1]; xStream << m_afSunriseColor[2];
		xStream << m_afMiddayColor[0]; xStream << m_afMiddayColor[1]; xStream << m_afMiddayColor[2];
		xStream << m_afSunsetColor[0]; xStream << m_afSunsetColor[1]; xStream << m_afSunsetColor[2];
		xStream << m_afNightColor[0]; xStream << m_afNightColor[1]; xStream << m_afNightColor[2];

		// Fog
		xStream << m_fFogDensityBase;
		xStream << m_fFogDensityFoggy;
		xStream << m_fFogTransitionSpeed;
		xStream << m_afFogColorDay[0]; xStream << m_afFogColorDay[1]; xStream << m_afFogColorDay[2];
		xStream << m_afFogColorNight[0]; xStream << m_afFogColorNight[1]; xStream << m_afFogColorNight[2];
		xStream << m_afFogColorSunrise[0]; xStream << m_afFogColorSunrise[1]; xStream << m_afFogColorSunrise[2];

		// Weather
		xStream << m_fWeatherChangeInterval;
		xStream << m_fWeatherTransitionDuration;
		xStream << m_bRandomWeather;

		// Terrain
		xStream << m_fTerrainScale;

		// Debug
		xStream << m_bShowDebugHUD;
		xStream << m_bShowTerrainLOD;
		xStream << m_bShowStreamingStats;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			// Player movement
			xStream >> m_fMoveSpeed;
			xStream >> m_fSprintMultiplier;
			xStream >> m_fMouseSensitivity;
			xStream >> m_fPlayerEyeHeight;
			xStream >> m_fGravity;
			xStream >> m_fJumpVelocity;

			// Camera
			xStream >> m_fFOV;
			xStream >> m_fNearPlane;
			xStream >> m_fFarPlane;
			xStream >> m_fPitchLimit;

			// Day/Night cycle
			xStream >> m_fDayCycleDuration;
			xStream >> m_fStartTimeOfDay;
			xStream >> m_bDayCycleEnabled;

			// Sun/Light
			xStream >> m_fSunIntensity;
			xStream >> m_fAmbientIntensity;
			xStream >> m_fNightAmbient;
			xStream >> m_afSunriseColor[0]; xStream >> m_afSunriseColor[1]; xStream >> m_afSunriseColor[2];
			xStream >> m_afMiddayColor[0]; xStream >> m_afMiddayColor[1]; xStream >> m_afMiddayColor[2];
			xStream >> m_afSunsetColor[0]; xStream >> m_afSunsetColor[1]; xStream >> m_afSunsetColor[2];
			xStream >> m_afNightColor[0]; xStream >> m_afNightColor[1]; xStream >> m_afNightColor[2];

			// Fog
			xStream >> m_fFogDensityBase;
			xStream >> m_fFogDensityFoggy;
			xStream >> m_fFogTransitionSpeed;
			xStream >> m_afFogColorDay[0]; xStream >> m_afFogColorDay[1]; xStream >> m_afFogColorDay[2];
			xStream >> m_afFogColorNight[0]; xStream >> m_afFogColorNight[1]; xStream >> m_afFogColorNight[2];
			xStream >> m_afFogColorSunrise[0]; xStream >> m_afFogColorSunrise[1]; xStream >> m_afFogColorSunrise[2];

			// Weather
			xStream >> m_fWeatherChangeInterval;
			xStream >> m_fWeatherTransitionDuration;
			xStream >> m_bRandomWeather;

			// Terrain
			xStream >> m_fTerrainScale;

			// Debug
			xStream >> m_bShowDebugHUD;
			xStream >> m_bShowTerrainLOD;
			xStream >> m_bShowStreamingStats;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override
	{
		ImGui::Text("Exploration Configuration");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Player Movement", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Move Speed", &m_fMoveSpeed, 0.5f, 1.0f, 50.0f);
			ImGui::DragFloat("Sprint Multiplier", &m_fSprintMultiplier, 0.1f, 1.0f, 5.0f);
			ImGui::DragFloat("Mouse Sensitivity", &m_fMouseSensitivity, 0.0001f, 0.0001f, 0.01f, "%.4f");
			ImGui::DragFloat("Eye Height", &m_fPlayerEyeHeight, 0.1f, 0.5f, 5.0f);
			ImGui::DragFloat("Gravity", &m_fGravity, 0.5f, 5.0f, 50.0f);
			ImGui::DragFloat("Jump Velocity", &m_fJumpVelocity, 0.5f, 1.0f, 20.0f);
		}

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("FOV (degrees)", &m_fFOV, 1.0f, 30.0f, 120.0f);
			ImGui::DragFloat("Near Plane", &m_fNearPlane, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("Far Plane", &m_fFarPlane, 100.0f, 100.0f, 10000.0f);
			ImGui::DragFloat("Pitch Limit", &m_fPitchLimit, 0.01f, 0.5f, 1.57f);
		}

		if (ImGui::CollapsingHeader("Day/Night Cycle", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Cycle Duration (s)", &m_fDayCycleDuration, 10.0f, 60.0f, 3600.0f);
			ImGui::SliderFloat("Start Time", &m_fStartTimeOfDay, 0.0f, 1.0f, "%.2f");
			ImGui::Checkbox("Enable Cycle", &m_bDayCycleEnabled);
		}

		if (ImGui::CollapsingHeader("Sun/Light"))
		{
			ImGui::DragFloat("Sun Intensity", &m_fSunIntensity, 0.1f, 0.0f, 5.0f);
			ImGui::DragFloat("Ambient (Day)", &m_fAmbientIntensity, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Ambient (Night)", &m_fNightAmbient, 0.01f, 0.0f, 0.5f);
			ImGui::ColorEdit3("Sunrise Color", m_afSunriseColor);
			ImGui::ColorEdit3("Midday Color", m_afMiddayColor);
			ImGui::ColorEdit3("Sunset Color", m_afSunsetColor);
			ImGui::ColorEdit3("Night Color", m_afNightColor);
		}

		if (ImGui::CollapsingHeader("Fog"))
		{
			ImGui::DragFloat("Fog Density (Clear)", &m_fFogDensityBase, 0.00001f, 0.0f, 0.01f, "%.5f");
			ImGui::DragFloat("Fog Density (Foggy)", &m_fFogDensityFoggy, 0.0001f, 0.0f, 0.01f, "%.4f");
			ImGui::DragFloat("Transition Speed", &m_fFogTransitionSpeed, 0.1f, 0.1f, 5.0f);
			ImGui::ColorEdit3("Fog Color (Day)", m_afFogColorDay);
			ImGui::ColorEdit3("Fog Color (Night)", m_afFogColorNight);
			ImGui::ColorEdit3("Fog Color (Sunrise)", m_afFogColorSunrise);
		}

		if (ImGui::CollapsingHeader("Weather"))
		{
			ImGui::DragFloat("Change Interval (s)", &m_fWeatherChangeInterval, 10.0f, 30.0f, 600.0f);
			ImGui::DragFloat("Transition Duration (s)", &m_fWeatherTransitionDuration, 5.0f, 5.0f, 120.0f);
			ImGui::Checkbox("Random Weather", &m_bRandomWeather);
		}

		if (ImGui::CollapsingHeader("Debug"))
		{
			ImGui::Checkbox("Show Debug HUD", &m_bShowDebugHUD);
			ImGui::Checkbox("Show Terrain LOD", &m_bShowTerrainLOD);
			ImGui::Checkbox("Show Streaming Stats", &m_bShowStreamingStats);
		}
	}
#endif
};

// Register the DataAsset type (call once at startup)
inline void RegisterExplorationDataAssets()
{
	Zenith_DataAssetManager::RegisterDataAssetType<Exploration_Config>();
}
