#pragma once
/**
 * Exploration_Behaviour.h - Main game coordinator
 *
 * This is the central behavior that coordinates all game systems:
 * - Player movement (Exploration_PlayerController.h)
 * - Terrain interaction (Exploration_TerrainExplorer.h)
 * - Day/night cycle (Exploration_AtmosphereController.h)
 * - Asset streaming (Exploration_AsyncLoader.h)
 * - UI management (Exploration_UIManager.h)
 *
 * Key lifecycle hooks:
 * - OnAwake()  - Called at RUNTIME creation only
 * - OnStart()  - Called before first OnUpdate
 * - OnUpdate() - Called every frame
 * - RenderPropertiesPanel() - Editor UI (tools build)
 *
 * Engine Features Demonstrated:
 * - Zenith_ScriptBehaviour lifecycle
 * - Zenith_TerrainComponent terrain rendering
 * - Day/night cycle and weather
 * - Fog and atmospheric effects
 * - First-person camera controls
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Input/Zenith_Input.h"

// Include game modules
#include "Exploration_Config.h"
#include "Exploration_PlayerController.h"
#include "Exploration_TerrainExplorer.h"
#include "Exploration_AtmosphereController.h"
#include "Exploration_AsyncLoader.h"
#include "Exploration_UIManager.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// Main Behavior Class
// ============================================================================
class Exploration_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Exploration_Behaviour)

	Exploration_Behaviour() = delete;
	Exploration_Behaviour(Zenith_Entity& xParentEntity)
		: m_bInitialized(false)
		, m_fFPSAccumulator(0.0f)
		, m_uFrameCount(0)
		, m_fCurrentFPS(60.0f)
		, m_fFPSUpdateInterval(0.5f)
	{
	}

	~Exploration_Behaviour() = default;

	// ========================================================================
	// Lifecycle Hooks - Called by engine
	// ========================================================================

	/**
	 * OnAwake - Called when behavior is attached at RUNTIME
	 * NOT called during scene loading/deserialization.
	 */
	void OnAwake() ZENITH_FINAL override
	{
		InitializeFromConfig();
		m_bInitialized = true;
	}

	/**
	 * OnStart - Called before first OnUpdate, for ALL entities
	 * Called even for entities loaded from scene file.
	 */
	void OnStart() ZENITH_FINAL override
	{
		if (!m_bInitialized)
		{
			InitializeFromConfig();
			m_bInitialized = true;
		}

		// Create UI elements
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			Exploration_UIManager::CreateUI(xUI);
		}

		// Set debug HUD visibility from config
		Exploration_UIManager::SetDebugHUDVisible(m_xConfig.m_bShowDebugHUD);
	}

	/**
	 * OnUpdate - Called every frame
	 * Main game loop coordinating all systems
	 */
	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// Update FPS counter
		UpdateFPS(fDt);

		// Handle debug toggle
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_TAB))
		{
			Exploration_UIManager::ToggleDebugHUD();
		}

		// Get camera entity
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		Zenith_EntityID uCamID = xScene.GetMainCameraEntity();
		if (uCamID == INVALID_ENTITY_ID || !xScene.EntityExists(uCamID))
			return;

		Zenith_Entity xCamEntity = xScene.GetEntity(uCamID);
		if (!xCamEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		// Get current player position
		Zenith_Maths::Vector3 xPlayerPos;
		xCamera.GetPosition(xPlayerPos);

		// Get terrain height at player position
		float fTerrainHeight = Exploration_TerrainExplorer::GetTerrainHeightAt(
			xPlayerPos.x, xPlayerPos.z);

		// Update player controller (movement + mouse look)
		Exploration_PlayerController::Update(xCamera, fTerrainHeight, fDt);

		// Get updated position after movement
		xCamera.GetPosition(xPlayerPos);

		// Clamp to terrain bounds
		xPlayerPos = Exploration_TerrainExplorer::ClampToTerrainBounds(xPlayerPos);
		xCamera.SetPosition(xPlayerPos);

		// Update atmosphere (day/night cycle, weather)
		Exploration_AtmosphereController::Update(fDt);

		// Update async loader
		Exploration_AsyncLoader::Update();

		// Update UI
		UpdateUI(xPlayerPos);
	}

	/**
	 * RenderPropertiesPanel - Editor UI (tools build only)
	 */
	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Exploration Game");
		ImGui::Separator();

		// Time controls
		if (ImGui::CollapsingHeader("Time & Weather", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const auto& xAtmosphere = Exploration_AtmosphereController::GetState();

			char szTimeStr[16];
			Exploration_AtmosphereController::GetTimeString(szTimeStr, sizeof(szTimeStr));
			ImGui::Text("Time: %s", szTimeStr);

			float fTime = xAtmosphere.m_fTimeOfDay;
			if (ImGui::SliderFloat("Time of Day", &fTime, 0.0f, 1.0f))
			{
				Exploration_AtmosphereController::SetTimeOfDay(fTime);
			}

			bool bCycleEnabled = m_xConfig.m_bDayCycleEnabled;
			if (ImGui::Checkbox("Day Cycle Enabled", &bCycleEnabled))
			{
				m_xConfig.m_bDayCycleEnabled = bCycleEnabled;
				Exploration_AtmosphereController::SetDayCycleEnabled(bCycleEnabled);
			}

			ImGui::Text("Weather: %s",
				Exploration_AtmosphereController::GetWeatherName(xAtmosphere.m_eWeatherState));

			if (ImGui::Button("Clear"))
			{
				Exploration_AtmosphereController::SetWeather(
					Exploration_AtmosphereController::WEATHER_CLEAR);
			}
			ImGui::SameLine();
			if (ImGui::Button("Cloudy"))
			{
				Exploration_AtmosphereController::SetWeather(
					Exploration_AtmosphereController::WEATHER_CLOUDY);
			}
			ImGui::SameLine();
			if (ImGui::Button("Foggy"))
			{
				Exploration_AtmosphereController::SetWeather(
					Exploration_AtmosphereController::WEATHER_FOGGY);
			}
		}

		// Player controls
		if (ImGui::CollapsingHeader("Player"))
		{
			ImGui::DragFloat("Move Speed", &m_xConfig.m_fMoveSpeed, 0.5f, 1.0f, 50.0f);
			ImGui::DragFloat("Sprint Multiplier", &m_xConfig.m_fSprintMultiplier, 0.1f, 1.0f, 5.0f);
			ImGui::DragFloat("Mouse Sensitivity", &m_xConfig.m_fMouseSensitivity, 0.0001f, 0.0001f, 0.01f, "%.4f");

			// Apply changes
			if (ImGui::Button("Apply Player Settings"))
			{
				Exploration_PlayerController::Configure(
					m_xConfig.m_fMoveSpeed,
					m_xConfig.m_fSprintMultiplier,
					m_xConfig.m_fMouseSensitivity,
					m_xConfig.m_fPlayerEyeHeight,
					m_xConfig.m_fPitchLimit,
					m_xConfig.m_fGravity,
					m_xConfig.m_fJumpVelocity);
			}
		}

		// Debug info
		if (ImGui::CollapsingHeader("Debug"))
		{
			ImGui::Text("FPS: %.1f", m_fCurrentFPS);

			auto xStats = Exploration_TerrainExplorer::GetStreamingStats();
			ImGui::Text("Vertex Buffer: %.0f / %.0f MB",
				xStats.m_fVertexBufferUsageMB, xStats.m_fVertexBufferTotalMB);
			ImGui::Text("High LOD Chunks: %u", xStats.m_uHighLODChunksResident);
			ImGui::Text("Streams/Frame: %u", xStats.m_uStreamsThisFrame);

			if (ImGui::Checkbox("Show Debug HUD", &m_xConfig.m_bShowDebugHUD))
			{
				Exploration_UIManager::SetDebugHUDVisible(m_xConfig.m_bShowDebugHUD);
			}
		}

		// Atmosphere debug
		if (ImGui::CollapsingHeader("Atmosphere Debug"))
		{
			const auto& xAtmosphere = Exploration_AtmosphereController::GetState();
			ImGui::Text("Sun Dir: %.2f, %.2f, %.2f",
				xAtmosphere.m_xSunDirection.x,
				xAtmosphere.m_xSunDirection.y,
				xAtmosphere.m_xSunDirection.z);
			ImGui::Text("Sun Intensity: %.2f", xAtmosphere.m_fSunIntensity);
			ImGui::Text("Ambient: %.2f", xAtmosphere.m_fAmbientIntensity);
			ImGui::Text("Fog Density: %.5f", xAtmosphere.m_fFogDensity);
		}
#endif
	}

	// ========================================================================
	// Serialization
	// ========================================================================
	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		m_xConfig.WriteToDataStream(xStream);
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			m_xConfig.ReadFromDataStream(xStream);
		}
	}

private:
	// ========================================================================
	// Initialization
	// ========================================================================
	void InitializeFromConfig()
	{
		// Configure player controller
		Exploration_PlayerController::Configure(
			m_xConfig.m_fMoveSpeed,
			m_xConfig.m_fSprintMultiplier,
			m_xConfig.m_fMouseSensitivity,
			m_xConfig.m_fPlayerEyeHeight,
			m_xConfig.m_fPitchLimit,
			m_xConfig.m_fGravity,
			m_xConfig.m_fJumpVelocity);

		// Configure atmosphere controller
		Exploration_AtmosphereController::Configure(
			m_xConfig.m_fDayCycleDuration,
			m_xConfig.m_fStartTimeOfDay,
			m_xConfig.m_bDayCycleEnabled,
			m_xConfig.m_fSunIntensity,
			m_xConfig.m_fAmbientIntensity,
			m_xConfig.m_fNightAmbient,
			m_xConfig.m_afSunriseColor,
			m_xConfig.m_afMiddayColor,
			m_xConfig.m_afSunsetColor,
			m_xConfig.m_afNightColor,
			m_xConfig.m_fFogDensityBase,
			m_xConfig.m_fFogDensityFoggy,
			m_xConfig.m_fFogTransitionSpeed,
			m_xConfig.m_afFogColorDay,
			m_xConfig.m_afFogColorNight,
			m_xConfig.m_afFogColorSunrise,
			m_xConfig.m_fWeatherChangeInterval,
			m_xConfig.m_fWeatherTransitionDuration,
			m_xConfig.m_bRandomWeather);
	}

	// ========================================================================
	// FPS Calculation
	// ========================================================================
	void UpdateFPS(float fDt)
	{
		m_uFrameCount++;
		m_fFPSAccumulator += fDt;

		if (m_fFPSAccumulator >= m_fFPSUpdateInterval)
		{
			m_fCurrentFPS = static_cast<float>(m_uFrameCount) / m_fFPSAccumulator;
			m_uFrameCount = 0;
			m_fFPSAccumulator = 0.0f;
		}
	}

	// ========================================================================
	// UI Update
	// ========================================================================
	void UpdateUI(const Zenith_Maths::Vector3& xPlayerPos)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Get time string
		char szTimeStr[16];
		Exploration_AtmosphereController::GetTimeString(szTimeStr, sizeof(szTimeStr));

		// Get atmosphere state
		const auto& xAtmosphere = Exploration_AtmosphereController::GetState();

		// Get terrain info
		auto xTerrainInfo = Exploration_TerrainExplorer::GetTerrainInfo(xPlayerPos);

		// Get streaming stats
		auto xStreamingStats = Exploration_TerrainExplorer::GetStreamingStats();

		// Get resident LOD at current chunk
		uint32_t uResidentLOD = Exploration_TerrainExplorer::GetChunkResidentLOD(
			xTerrainInfo.m_iChunkX, xTerrainInfo.m_iChunkY);

		// Update all UI elements
		Exploration_UIManager::UpdateAll(
			xUI,
			szTimeStr,
			Exploration_AtmosphereController::GetWeatherName(xAtmosphere.m_eWeatherState),
			xAtmosphere.m_fWeatherTransition,
			xPlayerPos,
			xTerrainInfo.m_iChunkX,
			xTerrainInfo.m_iChunkY,
			Exploration_TerrainExplorer::GetLODDisplayName(xTerrainInfo.m_uCurrentLOD),
			uResidentLOD,
			xStreamingStats.m_fVertexBufferUsageMB,
			xStreamingStats.m_fVertexBufferTotalMB,
			xStreamingStats.m_uHighLODChunksResident,
			xStreamingStats.m_uStreamsThisFrame,
			m_fCurrentFPS,
			Exploration_AsyncLoader::GetStatusString());
	}

	// ========================================================================
	// Member Variables
	// ========================================================================
	bool m_bInitialized;
	Exploration_Config m_xConfig;

	// FPS tracking
	float m_fFPSAccumulator;
	uint32_t m_uFrameCount;
	float m_fCurrentFPS;
	float m_fFPSUpdateInterval;
};
