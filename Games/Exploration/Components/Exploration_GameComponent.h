#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Exploration_GameComponent.h - Main game coordinator
 *
 * This is the central game component that coordinates all game systems:
 * - Player movement (Exploration_PlayerController.h)
 * - Terrain interaction (Exploration_TerrainExplorer.h)
 * - Day/night cycle (Exploration_AtmosphereController.h)
 * - UI management (Exploration_UIManager.h)
 *
 * Key lifecycle hooks (concept-detected by the component-meta registry):
 * - OnAwake()  - Called at RUNTIME creation only
 * - OnStart()  - Called before first OnUpdate
 * - OnUpdate() - Called every frame
 * - RenderPropertiesPanel() - Editor UI (tools build)
 *
 * Engine Features Demonstrated:
 * - Game ECS component lifecycle hooks
 * - Zenith_TerrainComponent terrain rendering
 * - Day/night cycle and weather
 * - Fog and atmospheric effects
 * - First-person camera controls
 * - Multi-scene architecture (persistent GameManager + world scene)
 * - Zenith_UIButton for clickable/tappable menu
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Core/Zenith_PropertySystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Input/Zenith_Input.h"
#include "UI/Zenith_UIButton.h"

// Include game modules
#include "Exploration_Config.h"
#include "Exploration_PlayerController.h"
#include "Exploration_TerrainExplorer.h"
#include "Exploration_AtmosphereController.h"
#include "Exploration_UIManager.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// World content creation/cleanup (defined in Exploration.cpp)
extern void Exploration_CreateWorldContent(Zenith_SceneData* pxSceneData);
extern void Exploration_CleanupWorldContent();

// ============================================================================
// Game State
// ============================================================================
enum class ExplorationGameState : uint8_t
{
	MAIN_MENU = 0,
	PLAYING
};

// ============================================================================
// Main Game Component Class
// ============================================================================
class Exploration_GameComponent
{
public:
	Exploration_GameComponent() = delete;
	Exploration_GameComponent(Zenith_Entity& xParentEntity)
		: m_bInitialized(false)
		, m_fFPSAccumulator(0.0f)
		, m_uFrameCount(0)
		, m_fCurrentFPS(60.0f)
		, m_fFPSUpdateInterval(0.5f)
		, m_eGameState(ExplorationGameState::MAIN_MENU)
		, m_iFocusIndex(0)
		, m_xParentEntity(xParentEntity)
	{
	}

	~Exploration_GameComponent() = default;

	// Component pools move-construct on resize. m_xConfig derives from
	// Zenith_Asset (neither copyable nor movable), so the moves are hand-
	// written: plain members transfer, config VALUES copy across.
	Exploration_GameComponent(Exploration_GameComponent&& xOther) noexcept
		: m_bInitialized(xOther.m_bInitialized)
		, m_fFPSAccumulator(xOther.m_fFPSAccumulator)
		, m_uFrameCount(xOther.m_uFrameCount)
		, m_fCurrentFPS(xOther.m_fCurrentFPS)
		, m_fFPSUpdateInterval(xOther.m_fFPSUpdateInterval)
		, m_eGameState(xOther.m_eGameState)
		, m_iFocusIndex(xOther.m_iFocusIndex)
		, m_xParentEntity(xOther.m_xParentEntity)
	{
		m_xConfig.CopyValuesFrom(xOther.m_xConfig);
		FixUpInstancePointerAfterMove(xOther);
	}
	Exploration_GameComponent& operator=(Exploration_GameComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_bInitialized = xOther.m_bInitialized;
			m_xConfig.CopyValuesFrom(xOther.m_xConfig);
			m_fFPSAccumulator = xOther.m_fFPSAccumulator;
			m_uFrameCount = xOther.m_uFrameCount;
			m_fCurrentFPS = xOther.m_fCurrentFPS;
			m_fFPSUpdateInterval = xOther.m_fFPSUpdateInterval;
			m_eGameState = xOther.m_eGameState;
			m_iFocusIndex = xOther.m_iFocusIndex;
			m_xParentEntity = xOther.m_xParentEntity;
			FixUpInstancePointerAfterMove(xOther);
		}
		return *this;
	}
	Exploration_GameComponent(const Exploration_GameComponent&) = delete;
	Exploration_GameComponent& operator=(const Exploration_GameComponent&) = delete;

	// ========================================================================
	// Lifecycle Hooks - Called by engine (concept-detected)
	// ========================================================================

	void OnAwake()
	{
		// Static instance pointer for the characterization-test surface (reset every
		// OnAwake so menu->play->menu cycles repoint it at the live manager). Behaviour-
		// neutral: no production code reads it.
		s_pxInstance = this;

		InitializeFromConfig();
		m_bInitialized = true;

		// Wire menu button callbacks
		bool bHasMenu = false;
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;

			// The Play click is wired by the Exploration_GameFlow graph's
			// OnUIButtonClicked("MenuPlay") node (self-wires the button's onClick when
			// the graph ticks) -> LoadSceneByIndex, so no C++ SetOnClick is needed.
			Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlay)
			{
				pxPlay->SetFocused(true);
				bHasMenu = true;
			}
		}

		if (bHasMenu)
		{
			m_eGameState = ExplorationGameState::MAIN_MENU;
			SetMenuVisible(true);
		}
		else
		{
			// No menu UI (gameplay scene) - start game directly
			StartGame();
		}
	}

	void OnStart()
	{
		if (!m_bInitialized)
		{
			InitializeFromConfig();
			m_bInitialized = true;
		}

		// Create HUD elements via UIManager
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;
			Exploration_UIManager::CreateUI(xUI);
		}

		// Hide HUD while in menu
		if (m_eGameState == ExplorationGameState::MAIN_MENU)
		{
			SetHUDVisible(false);
		}

		// Set debug HUD visibility from config
		Exploration_UIManager::SetDebugHUDVisible(m_xConfig.m_bShowDebugHUD);
	}

	// OnDestroy - concept-detected by the meta registry; fires once before the
	// component is removed (scene unload). Added solely to null the test-surface
	// instance pointer so a menu<->play scene swap does not leave it dangling.
	// Behaviour-neutral: no other teardown (Exploration has no events/tasks).
	void OnDestroy()
	{
		if (s_pxInstance == this)
		{
			s_pxInstance = nullptr;
		}
	}

	void OnUpdate(const float fDt)
	{
		// The menu-focus / Escape->menu / Play-click input DECISIONS live in the
		// Exploration_GameFlow graph now (order 60, before this systems dispatch at
		// order 100). Escape's ReturnToMenu runs at order 60 but does NOT set
		// m_eGameState, so this switch still runs one more PLAYING frame on the
		// doomed scene (the scene-0 load defers to frame end) - the AIShowcase /
		// Combat / Survival-precedented, unobservable divergence. This switch is now
		// systems-only.
		switch (m_eGameState)
		{
		case ExplorationGameState::MAIN_MENU:
			break;

		case ExplorationGameState::PLAYING:
		{
			// Update FPS counter
			UpdateFPS(fDt);

			// The Tab debug-HUD toggle DECISION lives in the Exploration_PlayerActions
			// graph now (OnKeyPressed(TAB) @60 -> ExplorationToggleDebugHUD; UpdateUI
			// @100 reads the toggled bool the same frame).

			// Get camera from persistent scene
			Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
			if (!pxCamera)
				return;

			// Get current player position
			Zenith_Maths::Vector3 xPlayerPos;
			pxCamera->GetPosition(xPlayerPos);

			// Get terrain height at player position
			float fTerrainHeight = Exploration_TerrainExplorer::GetTerrainHeightAt(
				xPlayerPos.x, xPlayerPos.z);

			// Update player controller (movement + mouse look)
			Exploration_PlayerController::Update(*pxCamera, fTerrainHeight, fDt);

			// Get updated position after movement
			pxCamera->GetPosition(xPlayerPos);

			// Clamp to terrain bounds
			xPlayerPos = Exploration_TerrainExplorer::ClampToTerrainBounds(xPlayerPos);
			pxCamera->SetPosition(xPlayerPos);

			// Update atmosphere: the day/night time-advance (A) + weather FSM (B)
			// DECISIONS live in the Exploration_Atmosphere graph now - fire its driving
			// event (dt on the payload) where AtmosphereController::Update sat; the two
			// chains (AdvanceTime then TickWeather, node-add order) run synchronously.
			// Then apply the sun/fog MATH + engine upload (C-I) directly - the C++
			// systems shim reads the just-advanced time/weather (byte-identical order).
			FireGraphEvent("ExplorationAtmosphereTick", fDt);
			Exploration_AtmosphereController::ApplyAtmosphere(fDt);

			// Update UI
			UpdateUI(xPlayerPos);
			break;
		}
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Exploration Game");
		ImGui::Separator();

		const char* aszStates[] = { "MENU", "PLAYING" };
		ImGui::Text("State: %s", aszStates[static_cast<int>(m_eGameState)]);

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

		if (m_eGameState == ExplorationGameState::MAIN_MENU)
		{
			if (ImGui::Button("Start Game"))
				StartGame();
		}
		else
		{
			if (ImGui::Button("Return to Menu"))
				ReturnToMenu();
		}
	}
#endif

	// ========================================================================
	// Serialization
	// ========================================================================
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// Component-contract leading version (required by the meta registry).
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;

		// Parameter payload (byte-identical to the pre-migration parameter block,
		// including its own internal version field).
		const uint32_t uVersion = 1;
		xStream << uVersion;
		m_xConfig.WriteToDataStream(xStream);
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;

		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			m_xConfig.ReadFromDataStream(xStream);
		}
	}

	// ========================================================================
	// Characterization-test surface (behaviour-neutral) + graph shims.
	//
	// Read-only observers the behaviour-graph characterization tests assert
	// against, plus test-only pokes (set time/weather, force reduced world) and
	// the Graph_* decision shims the (later) graph nodes call. No production code
	// reads s_pxInstance / calls Test_*; the Graph_* bodies are the verbatim
	// decisions moved to graph nodes. Mirrors Survival's Test_/Graph_ surface.
	// ========================================================================

	ExplorationGameState GetGameState() const { return m_eGameState; }
	int32_t Graph_GetGameStateInt() const { return static_cast<int32_t>(m_eGameState); }

	// Behaviour-graph decision shims (Exploration_GameFlow.bgraph): the decision
	// bodies stay verbatim in the private ReturnToMenu / UpdateMenuInput; the graph
	// nodes resolve the self component and call these synchronously. m_eGameState
	// stays the per-instance source of truth (ExplorationGetGameState mirrors it to
	// the blackboard to gate the Escape chain).
	void Graph_ReturnToMenu()    { ReturnToMenu(); }
	void Graph_FocusPlayButton() { UpdateMenuInput(); }

	// Exploration_PlayerActions.bgraph - the Tab debug-HUD toggle decision.
	void Graph_ToggleDebugHUD()  { Exploration_UIManager::ToggleDebugHUD(); }

	// Exploration_Atmosphere.bgraph - the day/night time-advance (A) and weather FSM
	// (B) decisions. Bodies stay verbatim in the AtmosphereController; the graph nodes
	// call these, then OnUpdate calls ApplyAtmosphere (C-I) directly (the systems shim).
	// dt rides the "ExplorationAtmosphereTick" payload (custom-event context dt is 0).
	void Graph_AdvanceTime(float fDt) { Exploration_AtmosphereController::AdvanceTimeOfDay(fDt); }
	void Graph_TickWeather(float fDt) { Exploration_AtmosphereController::UpdateWeather(fDt); }

	// Fires a custom event on this manager's graph component (the inline C++->graph
	// seam for "ExplorationAtmosphereTick"; dt on the payload). No-op if no graph.
	void FireGraphEvent(const char* szName, float fDt)
	{
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
			return;
		Zenith_PropertyValue xDt;
		xDt.SetFloat(fDt);
		pxGraph->FireCustomEvent(szName, &xDt);
	}

	// The currently-awake ExplorationGame component (MenuManager's or GameManager's,
	// per the active scene). A menu<->game transition swaps the whole scene (SINGLE)
	// and thus the live instance; this tracks whichever manager is awake now.
	static Exploration_GameComponent* Test_GetLiveInstance() { return s_pxInstance; }
	static ExplorationGameState Test_GetLiveGameState()
	{
		return s_pxInstance ? s_pxInstance->m_eGameState : ExplorationGameState::MAIN_MENU;
	}

	// Atmosphere observers (read the namespace-static AtmosphereState / weather timer).
	static float   Test_GetTimeOfDay()        { return Exploration_AtmosphereController::GetState().m_fTimeOfDay; }
	static int32_t Test_GetWeatherState()     { return static_cast<int32_t>(Exploration_AtmosphereController::GetState().m_eWeatherState); }
	static float   Test_GetWeatherTransition(){ return Exploration_AtmosphereController::GetState().m_fWeatherTransition; }
	static float   Test_GetFogDensity()       { return Exploration_AtmosphereController::GetState().m_fFogDensity; }
	static float   Test_GetSunIntensity()     { return Exploration_AtmosphereController::GetState().m_fSunIntensity; }
	static void    Test_GetSunDirection(Zenith_Maths::Vector3& xOut) { xOut = Exploration_AtmosphereController::GetState().m_xSunDirection; }
	static void    Test_GetFogColor(Zenith_Maths::Vector3& xOut)     { xOut = Exploration_AtmosphereController::GetState().m_xFogColor; }
	static float   Test_GetWeatherTimer()     { return Exploration_AtmosphereController::GetWeatherTimer(); }

	// Atmosphere pokes (all wrap existing namespace setters; Test_ResetAtmosphere
	// calls the otherwise-dead Reset() ONLY as test scaffolding - never wired into
	// the production/conversion flow, where state persistence is load-bearing).
	static void Test_SetTimeOfDay(float fTime)      { Exploration_AtmosphereController::SetTimeOfDay(fTime); }
	static void Test_SetDayCycleEnabled(bool bOn)   { Exploration_AtmosphereController::SetDayCycleEnabled(bOn); }
	static void Test_SetWeather(int32_t eWeather)   { Exploration_AtmosphereController::SetWeather(static_cast<Exploration_AtmosphereController::WeatherState>(eWeather)); }
	static void Test_ResetAtmosphere()              { Exploration_AtmosphereController::Reset(); }

	static bool Test_IsDebugHUDVisible()            { return Exploration_UIManager::IsDebugHUDVisible(); }
	static bool Test_IsMouseCaptured()              { return Exploration_PlayerController::IsMouseCaptured(); }

	// Camera position via the cross-scene main-camera resolver (the same path
	// OnUpdate uses); false if no loaded scene has a resolvable main camera.
	static bool Test_GetCameraPosition(Zenith_Maths::Vector3& xOut)
	{
		Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
		if (pxCamera == nullptr) return false;
		pxCamera->GetPosition(xOut);
		return true;
	}

	// Reduced-world test flag: when set, StartGame's world creation skips the
	// 10k-tree spawn (KEEPS the terrain entity so GetStreamingStats /
	// GetChunkResidentLOD don't null-deref). Read by Exploration_CreateWorldContent.
	// Behaviour-neutral for every DECISION the graphs own (trees are pure visuals).
	static void Test_SetReducedWorld(bool bReduced) { s_bReducedWorld = bReduced; }
	static bool Test_IsReducedWorld()               { return s_bReducedWorld; }

private:
	// Called by the move members after state transfer: repoints s_pxInstance at
	// the new location when the moved-from object was the live instance (component
	// pools relocate on resize/scene-transfer). Behaviour-neutral (test surface).
	void FixUpInstancePointerAfterMove(Exploration_GameComponent& xMovedFrom)
	{
		if (s_pxInstance == &xMovedFrom)
		{
			s_pxInstance = this;
		}
	}

	// (OnPlayClicked removed - the menu Play click is handled by the
	//  Exploration_GameFlow graph's OnUIButtonClicked -> LoadSceneByIndex node.)

	// ========================================================================
	// State Transitions
	// ========================================================================
	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create terrain + trees in the current scene
		Zenith_Scene xCurrentScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xCurrentScene);
		Exploration_CreateWorldContent(pxSceneData);

		m_eGameState = ExplorationGameState::PLAYING;
	}

	void ReturnToMenu()
	{
		// Leave PLAYING NOW so the systems-only OnUpdate switch (order 100) takes the
		// MAIN_MENU (no-op) branch for the rest of THIS frame - faithfully reproducing
		// the original C++ control flow, where the Escape check did `ReturnToMenu();
		// return;` and skipped the remaining PLAYING systems. Without this, the graph's
		// ReturnToMenu (order 60) defers the scene load but leaves the state PLAYING, so
		// the @100 block would run one EXTRA atmosphere tick + PlayerController::Update
		// on the doomed frame - and those mutate NAMESPACE-STATIC state that persists
		// across the SINGLE scene swap (the weather timer/transition/RNG schedule, and
		// s_bMouseCaptured via the otherwise-dead Esc-release), permanently diverging
		// from the pre-conversion build. Setting the state here is inert for the doomed
		// GameManager (destroyed at the scene swap) and for the editor "Return to Menu"
		// button (which is genuinely returning to the menu).
		m_eGameState = ExplorationGameState::MAIN_MENU;

		Exploration_CleanupWorldContent();
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	// ========================================================================
	// Menu UI
	// ========================================================================
	void SetMenuVisible(bool bVisible)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;
		Zenith_UIComponent& xUI = *pxUI;

		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
		if (pxTitle) pxTitle->SetVisible(bVisible);
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay) pxPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;
		Zenith_UIComponent& xUI = *pxUI;

		const char* aszElements[] = { "Time", "Position", "Chunk", "Weather", "FPS", "Controls", "Loading", "TerrainLOD", "Streaming" };
		for (const char* szName : aszElements)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText) pxText->SetVisible(bVisible);
		}
	}

	void UpdateMenuInput()
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;
		Zenith_UIComponent& xUI = *pxUI;

		// Single button - keep it focused
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay) pxPlay->SetFocused(true);
	}

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
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

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
			m_fCurrentFPS);
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Static instance pointer for the characterization-test surface. Set in
	// OnAwake, cleared in OnDestroy (if ==this), repointed by the move members.
	// Single live ExplorationGame per process by design (SCENE_LOAD_SINGLE).
	static inline Exploration_GameComponent* s_pxInstance = nullptr;

	// Test-only reduced-world flag (skip the 10k-tree spawn, keep terrain entity).
	static inline bool s_bReducedWorld = false;

	bool m_bInitialized;
	Exploration_Config m_xConfig;

	// FPS tracking
	float m_fFPSAccumulator;
	uint32_t m_uFrameCount;
	float m_fCurrentFPS;
	float m_fFPSUpdateInterval;

	// Game state
	ExplorationGameState m_eGameState;
	int32_t m_iFocusIndex;

	// Owning entity (explicit member now - was provided by the old script base)
	Zenith_Entity m_xParentEntity;
};
