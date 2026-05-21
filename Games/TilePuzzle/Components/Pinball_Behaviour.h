#pragma once
/**
 * Pinball_Behaviour.h - Pinball minigame controller
 *
 * A simple pinball-style minigame: drag a plunger on the left, release to
 * launch a ball upward. The ball bounces off bumpers and obstacles, scoring
 * points. A target at the bottom awards bonus points. The ball falls via
 * real Jolt gravity on the -Y axis.
 *
 * Architecture:
 * - PinballManager entity (scene file): camera + UI + script
 * - PinballPlay scene (created/destroyed dynamically): ball, walls, obstacles
 *
 * State machine: READY -> LAUNCHING -> PLAYING -> BALL_LOST -> READY
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Physics/Zenith_Physics.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_ModelInstance.h"

#include "TilePuzzle/Components/TilePuzzle_SaveData.h"
#include "SaveData/Zenith_SaveData.h"
#include "UI/Zenith_UIOverlay.h"
#include "UI/Zenith_UIText.h"
#include "Flux/Text/Flux_TextImpl.h"

#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Core/Zenith_GraphicsOptions.h"

#include "DataStream/Zenith_DataStream.h"

#include <cmath>
#include <ctime>
#include <filesystem>
#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Phase 8: pinball resources accessed via TilePuzzle::Resources() -- see TilePuzzle_Behaviour.h.
namespace TilePuzzleUI
{
	// Pinball HUD text sizes
	static constexpr float fPB_OBJECTIVE_FONT = 52.f;
	static constexpr float fPB_COUNTER_FONT = 48.f;
	static constexpr float fPB_BALLS_FONT = 52.f;
	static constexpr float fPB_GATE_STATUS_FONT = 52.f;
	static constexpr float fPB_GATE_NUM_FONT = 48.f;

	// Top padding to clear Android status bar
	static constexpr float fPB_TOP_PADDING = 60.f;

	// Pinball HUD Y positions (scaled up for readability)
	static constexpr float fPB_OBJECTIVE_Y = fPB_TOP_PADDING + 20.f;
	static constexpr float fPB_PEG_COUNT_Y = fPB_TOP_PADDING + 76.f;
	static constexpr float fPB_TARGET_COUNT_Y = fPB_TOP_PADDING + 130.f;
	static constexpr float fPB_BALLS_Y = fPB_TOP_PADDING + 60.f;
	static constexpr float fPB_GATE_STATUS_Y = -40.f;
}

// ============================================================================
// Configuration Constants
// ============================================================================
static constexpr float s_fPB_LaunchForceMax = 35.f;
static constexpr float s_fPB_BallRadius = 0.15f;
static constexpr float s_fPB_BallScale = 0.3f;
static constexpr float s_fPB_TargetCooldown = 1.0f;
static constexpr uint32_t s_uPB_TargetScore = 500;

// Playfield bounds
static constexpr float s_fPB_FieldLeft = -2.4f;
static constexpr float s_fPB_FieldRight = 2.4f;
static constexpr float s_fPB_FieldBottom = 0.f;
static constexpr float s_fPB_FieldTop = 8.f;
static constexpr float s_fPB_WallThickness = 0.3f;

// Launcher channel
static constexpr float s_fPB_ChannelLeft = -2.4f;
static constexpr float s_fPB_ChannelRight = -1.6f;
static constexpr float s_fPB_ChannelBottom = 0.5f;
static constexpr float s_fPB_PlungerRestY = 1.5f;
static constexpr float s_fPB_PlungerMaxPull = 1.0f;
static constexpr float s_fPB_BallStartY = 2.0f;

static constexpr uint32_t s_uPB_MaxPegs = 8;
static constexpr uint32_t s_uPB_MaxLayouts = 6;
static constexpr float s_fPB_PegMinSeparation = 0.7f;
static constexpr uint32_t s_uPB_MaxWalls = 10;
static constexpr uint32_t s_uPB_PegScore = 100;
static constexpr float s_fPB_PegFlashDuration = 0.3f;
static constexpr float s_fPB_GateCelebrationDuration = 2.5f;
static constexpr uint32_t s_uPB_FirstClearBonus = 50;
static constexpr uint32_t s_uPB_ScoreToCoinDivisor = 100;
static constexpr uint32_t s_uPB_MinCoinsPerSession = 5;

// Peg layout: array of 2D positions for each peg
struct PinballPegLayout
{
	uint32_t uPegCount;
	float afX[s_uPB_MaxPegs];
	float afY[s_uPB_MaxPegs];
};

// ============================================================================
// Pinball Gate Objective Types and Data
// ============================================================================
static constexpr uint32_t s_uPB_MaxGates = 10;
static constexpr uint32_t s_uPB_MaxGatePegs = 8;

enum PinballObjectiveType : uint8_t
{
	PINBALL_OBJ_SCORE_THRESHOLD = 0,
	PINBALL_OBJ_HIT_ALL_PEGS,
	PINBALL_OBJ_TARGET_HITS,
	PINBALL_OBJ_COMBINED
};

struct PinballGateData
{
	PinballObjectiveType eObjectiveType;
	uint32_t uScoreThreshold;
	uint32_t uTargetHitsRequired;
	uint32_t uMaxBalls;  // 0 = unlimited
	uint8_t uNumPegs;
	float afPegPositionsX[s_uPB_MaxGatePegs];
	float afPegPositionsY[s_uPB_MaxGatePegs];
	bool bHasAllPegsObjective;
};

// ============================================================================
// Pinball State Enum
// ============================================================================
enum PinballState : uint8_t
{
	PINBALL_STATE_GATE_SELECT,
	PINBALL_STATE_READY,
	PINBALL_STATE_LAUNCHING,
	PINBALL_STATE_PLAYING,
	PINBALL_STATE_BALL_LOST,
	PINBALL_STATE_LEVEL_COMPLETE
};

// ============================================================================
// Ball Collision Behaviour (attached to ball entity to receive callbacks)
// ============================================================================
class Pinball_BallBehaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Pinball_BallBehaviour)

	Pinball_BallBehaviour() = delete;
	Pinball_BallBehaviour(Zenith_Entity&)
		: m_uCollisionCount(0)
	{
	}

	void OnCollisionEnter(Zenith_Entity xOther) ZENITH_FINAL override
	{
		if (m_uCollisionCount < s_uMaxCollisions)
		{
			m_axCollidedEntities[m_uCollisionCount] = xOther.GetEntityID();
			m_uCollisionCount++;
		}
	}

	bool DidCollideWith(Zenith_EntityID xEntityID) const
	{
		for (uint32_t i = 0; i < m_uCollisionCount; ++i)
		{
			if (m_axCollidedEntities[i] == xEntityID)
				return true;
		}
		return false;
	}

	void ClearCollisions() { m_uCollisionCount = 0; }

private:
	static constexpr uint32_t s_uMaxCollisions = 16;
	Zenith_EntityID m_axCollidedEntities[s_uMaxCollisions];
	uint32_t m_uCollisionCount;
};

// ============================================================================
// Main Behaviour Class
// ============================================================================
class Pinball_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
	friend class TilePuzzle_AutoTest;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Pinball_Behaviour)

	Pinball_Behaviour() = delete;
	Pinball_Behaviour(Zenith_Entity& /*xParentEntity*/)
		: m_eState(PINBALL_STATE_GATE_SELECT)
		, m_uSessionScore(0)
		, m_fPlungerPull(0.f)
		, m_bPlungerDragging(false)
		, m_bMouseWasDown(false)
		, m_fTargetCooldown(0.f)
		, m_fPlayingTime(0.f)
		, m_pxCubeGeometry(nullptr)
		, m_pxSphereGeometry(nullptr)
		, m_uWallCount(0)
		, m_uLayoutCount(0)
		, m_uCurrentLayout(0)
		, m_uCurrentGate(0)
		, m_uPegsHit(0)
		, m_uTargetHitCount(0)
		, m_uBallsRemaining(0)
		, m_bGateActive(false)
		, m_bGateCleared(false)
		, m_bGateFailed(false)
		, m_fGateCelebrationTimer(0.f)
		, m_uCurrentGatePegCount(0)
		, m_bHUDCreated(false)
	{
		m_xSaveData.Reset();
		memset(m_abPegHit, 0, sizeof(m_abPegHit));
		memset(m_afPegFlashTimer, 0, sizeof(m_afPegFlashTimer));
		memset(&m_xCurrentGateData, 0, sizeof(m_xCurrentGateData));
	}

#ifdef ZENITH_TOOLS
	static void GenerateAndWriteLayouts()
	{
		// Playfield bounds for peg placement (avoid channel, walls, target zone, top curves)
		const float fMinX = s_fPB_ChannelRight + 1.0f;
		const float fMaxX = s_fPB_FieldRight - 0.5f;
		const float fMinY = 1.5f;
		const float fMaxY = 6.5f;

		Zenith_DataStream xStream;
		uint32_t uLayoutCount = s_uPB_MaxLayouts;
		xStream << uLayoutCount;

		for (uint32_t uLayout = 0; uLayout < s_uPB_MaxLayouts; ++uLayout)
		{
			std::mt19937 xRng(uLayout * 31337u + 42u);
			std::uniform_real_distribution<float> xDistX(fMinX, fMaxX);
			std::uniform_real_distribution<float> xDistY(fMinY, fMaxY);

			PinballPegLayout xLayout;
			xLayout.uPegCount = s_uPB_MaxPegs;
			uint32_t uPlaced = 0;

			while (uPlaced < s_uPB_MaxPegs)
			{
				float fCandX = xDistX(xRng);
				float fCandY = xDistY(xRng);

				// Check minimum separation against all already-placed pegs
				bool bTooClose = false;
				for (uint32_t j = 0; j < uPlaced; ++j)
				{
					float fDx = fCandX - xLayout.afX[j];
					float fDy = fCandY - xLayout.afY[j];
					if (fDx * fDx + fDy * fDy < s_fPB_PegMinSeparation * s_fPB_PegMinSeparation)
					{
						bTooClose = true;
						break;
					}
				}

				if (!bTooClose)
				{
					xLayout.afX[uPlaced] = fCandX;
					xLayout.afY[uPlaced] = fCandY;
					++uPlaced;
				}
			}

			// Serialize layout
			xStream << xLayout.uPegCount;
			for (uint32_t i = 0; i < xLayout.uPegCount; ++i)
			{
				xStream << xLayout.afX[i];
				xStream << xLayout.afY[i];
			}
		}

		std::filesystem::create_directories(GAME_ASSETS_DIR "Pinball");
		xStream.WriteToFile(GAME_ASSETS_DIR "Pinball/PegLayouts.bin");
	}

	static void GenerateAndWriteGateData()
	{
		// Playfield bounds for gate peg placement
		const float fMinX = s_fPB_ChannelRight + 1.0f;
		const float fMaxX = s_fPB_FieldRight - 0.5f;
		const float fMinY = 1.5f;
		const float fMaxY = 6.5f;

		PinballGateData axGates[s_uPB_MaxGates];
		memset(axGates, 0, sizeof(axGates));

		// Gate 1: After level 10, SCORE_THRESHOLD 1000, 6 pegs
		axGates[0].eObjectiveType = PINBALL_OBJ_SCORE_THRESHOLD;
		axGates[0].uScoreThreshold = 1000;
		axGates[0].uTargetHitsRequired = 0;
		axGates[0].uMaxBalls = 0;
		axGates[0].uNumPegs = 6;
		axGates[0].bHasAllPegsObjective = false;

		// Gate 2: After level 20, SCORE_THRESHOLD 2000, 6 pegs
		axGates[1].eObjectiveType = PINBALL_OBJ_SCORE_THRESHOLD;
		axGates[1].uScoreThreshold = 2000;
		axGates[1].uTargetHitsRequired = 0;
		axGates[1].uMaxBalls = 0;
		axGates[1].uNumPegs = 6;
		axGates[1].bHasAllPegsObjective = false;

		// Gate 3: After level 30, HIT_ALL_PEGS, 6 pegs
		axGates[2].eObjectiveType = PINBALL_OBJ_HIT_ALL_PEGS;
		axGates[2].uScoreThreshold = 0;
		axGates[2].uTargetHitsRequired = 0;
		axGates[2].uMaxBalls = 0;
		axGates[2].uNumPegs = 6;
		axGates[2].bHasAllPegsObjective = false;

		// Gate 4: After level 40, SCORE_THRESHOLD 2000, max 4 balls, 7 pegs
		axGates[3].eObjectiveType = PINBALL_OBJ_SCORE_THRESHOLD;
		axGates[3].uScoreThreshold = 2000;
		axGates[3].uTargetHitsRequired = 0;
		axGates[3].uMaxBalls = 4;
		axGates[3].uNumPegs = 7;
		axGates[3].bHasAllPegsObjective = false;

		// Gate 5: After level 50, HIT_ALL_PEGS, 8 pegs
		axGates[4].eObjectiveType = PINBALL_OBJ_HIT_ALL_PEGS;
		axGates[4].uScoreThreshold = 0;
		axGates[4].uTargetHitsRequired = 0;
		axGates[4].uMaxBalls = 0;
		axGates[4].uNumPegs = 8;
		axGates[4].bHasAllPegsObjective = false;

		// Gate 6: After level 60, SCORE_THRESHOLD 4000, max 3 balls, 7 pegs
		axGates[5].eObjectiveType = PINBALL_OBJ_SCORE_THRESHOLD;
		axGates[5].uScoreThreshold = 4000;
		axGates[5].uTargetHitsRequired = 0;
		axGates[5].uMaxBalls = 3;
		axGates[5].uNumPegs = 7;
		axGates[5].bHasAllPegsObjective = false;

		// Gate 7: After level 70, TARGET_HITS 5, 7 pegs
		axGates[6].eObjectiveType = PINBALL_OBJ_TARGET_HITS;
		axGates[6].uScoreThreshold = 0;
		axGates[6].uTargetHitsRequired = 5;
		axGates[6].uMaxBalls = 0;
		axGates[6].uNumPegs = 7;
		axGates[6].bHasAllPegsObjective = false;

		// Gate 8: After level 80, SCORE_THRESHOLD 5000, max 3 balls, 8 pegs
		axGates[7].eObjectiveType = PINBALL_OBJ_SCORE_THRESHOLD;
		axGates[7].uScoreThreshold = 5000;
		axGates[7].uTargetHitsRequired = 0;
		axGates[7].uMaxBalls = 3;
		axGates[7].uNumPegs = 8;
		axGates[7].bHasAllPegsObjective = false;

		// Gate 9: After level 90, COMBINED score 3000 + hit all pegs, 8 pegs
		axGates[8].eObjectiveType = PINBALL_OBJ_COMBINED;
		axGates[8].uScoreThreshold = 3000;
		axGates[8].uTargetHitsRequired = 0;
		axGates[8].uMaxBalls = 0;
		axGates[8].uNumPegs = 8;
		axGates[8].bHasAllPegsObjective = true;

		// Gate 10: After level 100, TARGET_HITS 10, 8 pegs
		axGates[9].eObjectiveType = PINBALL_OBJ_TARGET_HITS;
		axGates[9].uScoreThreshold = 0;
		axGates[9].uTargetHitsRequired = 10;
		axGates[9].uMaxBalls = 0;
		axGates[9].uNumPegs = 8;
		axGates[9].bHasAllPegsObjective = false;

		// Generate deterministic peg positions for each gate
		for (uint32_t uGate = 0; uGate < s_uPB_MaxGates; ++uGate)
		{
			std::mt19937 xRng(uGate * 7919u + 1337u);
			std::uniform_real_distribution<float> xDistX(fMinX, fMaxX);
			std::uniform_real_distribution<float> xDistY(fMinY, fMaxY);

			uint32_t uPlaced = 0;
			while (uPlaced < axGates[uGate].uNumPegs)
			{
				float fCandX = xDistX(xRng);
				float fCandY = xDistY(xRng);

				bool bTooClose = false;
				for (uint32_t j = 0; j < uPlaced; ++j)
				{
					float fDx = fCandX - axGates[uGate].afPegPositionsX[j];
					float fDy = fCandY - axGates[uGate].afPegPositionsY[j];
					if (fDx * fDx + fDy * fDy < s_fPB_PegMinSeparation * s_fPB_PegMinSeparation)
					{
						bTooClose = true;
						break;
					}
				}

				if (!bTooClose)
				{
					axGates[uGate].afPegPositionsX[uPlaced] = fCandX;
					axGates[uGate].afPegPositionsY[uPlaced] = fCandY;
					++uPlaced;
				}
			}
		}

		// Serialize gate data to disk
		Zenith_DataStream xStream;
		uint32_t uGateCount = s_uPB_MaxGates;
		xStream << uGateCount;

		for (uint32_t uGate = 0; uGate < s_uPB_MaxGates; ++uGate)
		{
			const PinballGateData& xGate = axGates[uGate];
			xStream << xGate.eObjectiveType;
			xStream << xGate.uScoreThreshold;
			xStream << xGate.uTargetHitsRequired;
			xStream << xGate.uMaxBalls;
			xStream << xGate.uNumPegs;
			for (uint32_t i = 0; i < s_uPB_MaxGatePegs; ++i)
			{
				xStream << xGate.afPegPositionsX[i];
				xStream << xGate.afPegPositionsY[i];
			}
			xStream << xGate.bHasAllPegsObjective;
		}

		std::filesystem::create_directories(GAME_ASSETS_DIR "Pinball");
		xStream.WriteToFile(GAME_ASSETS_DIR "Pinball/GateData.bin");
	}
#endif

	~Pinball_Behaviour() = default;

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	void OnAwake() ZENITH_FINAL override
	{
		// Load save data
		if (!Zenith_SaveData::Load("autosave", TilePuzzle_ReadSaveData, &m_xSaveData))
		{
			m_xSaveData.Reset();
		}
		m_uSessionScore = 0;

		// Cache geometry
		m_pxCubeGeometry = TilePuzzle::Resources().m_pxCubeGeometry;
		m_pxSphereGeometry = TilePuzzle::Resources().m_pxSphereGeometry;

		// Create materials
		CreateMaterials();

		// Wire up UI buttons
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

			Zenith_UI::Zenith_UIButton* pxBackBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("PinballBackBtn");
			if (pxBackBtn)
			{
				pxBackBtn->SetOnClick(&OnBackClicked, this);
			}

			// Apply top padding to score group to clear Android status bar
			Zenith_UI::Zenith_UIElement* pxScoreGroup = xUI.FindElement("PinballScoreGroup");
			if (pxScoreGroup)
			{
				pxScoreGroup->SetPosition(-30.f, TilePuzzleUI::fPB_TOP_PADDING + 30.f);
			}

			// Gate select widgets
			m_pxGateSelectBg = xUI.FindElement("GateSelectBg");
			m_pxGateSelectTitle = xUI.FindElement("GateSelectTitle");
			m_pxGateFreeplayBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("GateFreeplayBtn");
			m_pxGateBackBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("GateBackBtn");

			for (uint32_t i = 0; i < s_uPB_MaxGates; ++i)
			{
				char szName[32];
				snprintf(szName, sizeof(szName), "GateBtn_%u", i);
				m_apxGateBtns[i] = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
				if (m_apxGateBtns[i])
				{
					s_axGateButtonData[i].pxBehaviour = this;
					s_axGateButtonData[i].uGateIndex = i;
					m_apxGateBtns[i]->SetOnClick(&OnGateButtonClicked, &s_axGateButtonData[i]);
				}
			}
			if (m_pxGateFreeplayBtn)
				m_pxGateFreeplayBtn->SetOnClick(&OnFreeplayClicked, this);
			if (m_pxGateBackBtn)
				m_pxGateBackBtn->SetOnClick(&OnGateBackClicked, this);

			// Tutorial overlay
			m_pxTutorialOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>("TutorialOverlay");
			m_pxTutorialText = xUI.FindElement<Zenith_UI::Zenith_UIText>("TutorialText");
			m_pxTutorialHintText = xUI.FindElement<Zenith_UI::Zenith_UIText>("TutorialHintText");
		}

		// Load gate data and determine which gate is active
		LoadGateData();
		DetermineCurrentGate();

		// Create HUD elements for objective display
		CreateHUDElements();

		// Check if a specific gate was requested (from level select / next level / continue)
		if (TilePuzzle::g_uPinballRequestedGate != UINT32_MAX
			&& TilePuzzle::g_uPinballRequestedGate < s_uPB_MaxGates)
		{
			m_uCurrentGate = TilePuzzle::g_uPinballRequestedGate;
			m_xCurrentGateData = m_axGateData[m_uCurrentGate];
			m_uCurrentGatePegCount = m_xCurrentGateData.uNumPegs;
			m_bGateActive = true;
			TilePuzzle::g_uPinballRequestedGate = UINT32_MAX;

			m_xPinballScene = Zenith_SceneManager::CreateEmptyScene("PinballPlay");
			Zenith_SceneManager::SetActiveScene(m_xPinballScene);

			g_xEngine.HDR().SetBloomIntensity(0.8f);
			g_xEngine.HDR().SetBloomThreshold(0.8f);
			g_xEngine.HDR().SetExposure(1.2f);
			Zenith_GraphicsOptions::Get().m_xSkyboxColour = Zenith_Maths::Vector3(0.02f, 0.02f, 0.06f);

			CreatePlayfield();
			SpawnBall();
			ResetGateAttempt();

			m_eState = PINBALL_STATE_READY;

			// Show pinball gate tutorial on first gate level
			TryShowPinballTutorial();
		}
		else
		{
			TilePuzzle::g_uPinballRequestedGate = UINT32_MAX;

			// Fallback: check if any gates are cleared for gate select screen
			bool bHasClearedGates = false;
			for (uint32_t i = 0; i < s_uPB_MaxGates; ++i)
			{
				if (m_xSaveData.IsPinballGateCleared(i))
				{
					bHasClearedGates = true;
					break;
				}
			}

			if (bHasClearedGates)
			{
				m_eState = PINBALL_STATE_GATE_SELECT;
			}
			else
			{
				m_xPinballScene = Zenith_SceneManager::CreateEmptyScene("PinballPlay");
				Zenith_SceneManager::SetActiveScene(m_xPinballScene);

				g_xEngine.HDR().SetBloomIntensity(0.8f);
				g_xEngine.HDR().SetBloomThreshold(0.8f);
				g_xEngine.HDR().SetExposure(1.2f);
				Zenith_GraphicsOptions::Get().m_xSkyboxColour = Zenith_Maths::Vector3(0.02f, 0.02f, 0.06f);

				CreatePlayfield();
				SpawnBall();
				ResetGateAttempt();

				m_eState = PINBALL_STATE_READY;
			}
		}

		UpdateUI();
	}

	void OnStart() ZENITH_FINAL override
	{
	}

	void OnUpdate(const float fDeltaTime) ZENITH_FINAL override
	{
		// Tutorial overlay blocks all input while active
		if (m_bTutorialActive)
		{
			UpdatePinballTutorial(fDeltaTime);
			return;
		}

		// Handle escape to return to menu
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
		{
			if (m_eState == PINBALL_STATE_LEVEL_COMPLETE)
			{
				HidePinballVictoryOverlay();
				m_bGateCleared = false;
				m_bDailyBonusAwarded = false;
			}
			ReturnToMenu();
			return;
		}

		// Gate selection screen
		if (m_eState == PINBALL_STATE_GATE_SELECT)
		{
			UpdateGateSelectUI();
			return;
		}

		// Level complete overlay - just wait for button click
		if (m_eState == PINBALL_STATE_LEVEL_COMPLETE)
			return;

		// Decrement cooldowns
		if (m_fTargetCooldown > 0.f)
			m_fTargetCooldown -= fDeltaTime;

		// Update peg flash timers
		UpdatePegFlashTimers(fDeltaTime);

		// Handle gate celebration / failure display timer
		if (m_fGateCelebrationTimer > 0.f)
		{
			m_fGateCelebrationTimer -= fDeltaTime;
			if (m_fGateCelebrationTimer <= 0.f)
			{
				m_fGateCelebrationTimer = 0.f;
				if (m_bGateCleared)
				{
					// Show victory overlay
					ShowPinballVictoryOverlay();
					m_eState = PINBALL_STATE_LEVEL_COMPLETE;
				}
				else if (m_bGateFailed)
				{
					// Retry current gate
					m_bGateFailed = false;
					ResetGateAttempt();
					RebuildPegsForCurrentGate();
					RespawnBall();
				}
			}
			UpdateUI();
			return;
		}

		switch (m_eState)
		{
		case PINBALL_STATE_READY:
		case PINBALL_STATE_LAUNCHING:
			HandleLauncherInput();
			break;

		case PINBALL_STATE_PLAYING:
			m_fPlayingTime += fDeltaTime;
			ConstrainBallToPlane();
			CheckScoringCollisions();
			CheckBallLost();
			break;

		case PINBALL_STATE_BALL_LOST:
			HandleBallLost();
			break;

		default:
			break;
		}

		UpdateUI();
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Pinball Minigame");
		ImGui::Separator();
		ImGui::Text("Score: %u", m_uSessionScore);
		ImGui::Text("Total Score: %u", m_xSaveData.uPinballScore);
		ImGui::Text("Plunger Pull: %.2f", m_fPlungerPull);

		const char* aszStateNames[] = { "Gate Select", "Ready", "Launching", "Playing", "Ball Lost", "Level Complete" };
		ImGui::Text("State: %s", aszStateNames[m_eState]);

		ImGui::Separator();
		ImGui::Text("Gate: %u / %u", m_uCurrentGate + 1, s_uPB_MaxGates);
		ImGui::Text("Gate Active: %s", m_bGateActive ? "Yes" : "No");
		if (m_bGateActive)
		{
			const char* aszObjNames[] = { "Score", "Hit All Pegs", "Target Hits", "Combined" };
			ImGui::Text("Objective: %s", aszObjNames[m_xCurrentGateData.eObjectiveType]);
			ImGui::Text("Pegs Hit: %u / %u", m_uPegsHit, m_uCurrentGatePegCount);
			ImGui::Text("Target Hits: %u", m_uTargetHitCount);
			if (m_xCurrentGateData.uMaxBalls > 0)
				ImGui::Text("Balls: %u", m_uBallsRemaining);
		}

		if (ImGui::Button("Respawn Ball"))
		{
			RespawnBall();
			m_eState = PINBALL_STATE_READY;
		}
		if (ImGui::Button("Clear Current Gate"))
		{
			OnGateCleared();
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
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
	}

private:
	// ========================================================================
	// Button Callbacks
	// ========================================================================

	static void OnBackClicked(void* pxUserData)
	{
		Pinball_Behaviour* pxSelf = static_cast<Pinball_Behaviour*>(pxUserData);
		pxSelf->ReturnToMenu();
	}

	static void OnGateButtonClicked(void* pxUserData)
	{
		GateButtonCallbackData* pxData = static_cast<GateButtonCallbackData*>(pxUserData);
		pxData->pxBehaviour->SetGateSelectVisible(false);
		pxData->pxBehaviour->EnterGateFromSelect(pxData->uGateIndex);
	}

	static void OnFreeplayClicked(void* pxUserData)
	{
		Pinball_Behaviour* pxSelf = static_cast<Pinball_Behaviour*>(pxUserData);
		pxSelf->SetGateSelectVisible(false);
		pxSelf->EnterFreeplayFromSelect();
	}

	static void OnGateBackClicked(void* pxUserData)
	{
		Pinball_Behaviour* pxSelf = static_cast<Pinball_Behaviour*>(pxUserData);
		pxSelf->ReturnToMenu();
	}

	// ========================================================================
	// Scene Transitions
	// ========================================================================

	void ReturnToMenu()
	{
		// Award score-based coins for freeplay sessions (gate clears already awarded)
		if (!m_bGateCleared && m_uSessionScore > 0)
		{
			uint32_t uCoins = m_uSessionScore / s_uPB_ScoreToCoinDivisor;
			if (uCoins < s_uPB_MinCoinsPerSession) uCoins = s_uPB_MinCoinsPerSession;
			m_xSaveData.AddCoins(static_cast<int32_t>(uCoins));
		}

		// Update total score and save
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Restore HDR defaults
		g_xEngine.HDR().SetBloomIntensity(0.5f);
		g_xEngine.HDR().SetBloomThreshold(1.0f);
		g_xEngine.HDR().SetExposure(1.0f);

		// Cleanup dynamic scene
		if (m_xPinballScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xPinballScene);
			m_xPinballScene = Zenith_Scene();
		}

		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void ReturnToGateSelect()
	{
		// Clean up gameplay scene
		if (m_xPinballScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xPinballScene);
			m_xPinballScene = Zenith_Scene();
		}

		// Restore HDR defaults
		g_xEngine.HDR().SetBloomIntensity(0.5f);
		g_xEngine.HDR().SetBloomThreshold(1.0f);
		g_xEngine.HDR().SetExposure(1.0f);
		Zenith_GraphicsOptions::Get().m_xSkyboxColour = Zenith_Maths::Vector3(0.f, 0.f, 0.f);

		DetermineCurrentGate();
		m_eState = PINBALL_STATE_GATE_SELECT;
	}

	// ========================================================================
	// Material Creation
	// ========================================================================

	void CreateMaterials()
	{
		const TextureHandle& xGridTex = g_xEngine.FluxGraphics().m_xGridTexture;

		// Ball - cat-themed orange, metallic chrome
		m_xBallMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xBallMaterial.GetDirect()->SetName("PinballBall");
		m_xBallMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		m_xBallMaterial.GetDirect()->SetBaseColor({ 0.95f, 0.6f, 0.2f, 1.f });
		m_xBallMaterial.GetDirect()->SetRoughness(0.2f);
		m_xBallMaterial.GetDirect()->SetMetallic(1.0f);
		m_xBallMaterial.GetDirect()->SetEmissiveColor(Zenith_Maths::Vector3(0.5f, 0.3f, 0.1f));
		m_xBallMaterial.GetDirect()->SetEmissiveIntensity(0.8f);

		// Walls - dark steel blue with PBR textures
		m_xWallMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xWallMaterial.GetDirect()->SetName("PinballWall");
		m_xWallMaterial.GetDirect()->SetBaseColor({ 0.15f, 0.18f, 0.3f, 1.f });
		if (TilePuzzle::Resources().m_xPinballWallDiffuseTex)
			m_xWallMaterial.GetDirect()->SetDiffuseTexture(TilePuzzle::Resources().m_xPinballWallDiffuseTex);
		else
			m_xWallMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		if (TilePuzzle::Resources().m_xPinballWallRMTex)
			m_xWallMaterial.GetDirect()->SetRoughnessMetallicTexture(TilePuzzle::Resources().m_xPinballWallRMTex);

		// Wall trim - neon blue emissive for boundary walls
		m_xWallTrimMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xWallTrimMaterial.GetDirect()->SetName("PinballWallTrim");
		m_xWallTrimMaterial.GetDirect()->SetBaseColor({ 0.1f, 0.15f, 0.4f, 1.f });
		if (TilePuzzle::Resources().m_xPinballWallDiffuseTex)
			m_xWallTrimMaterial.GetDirect()->SetDiffuseTexture(TilePuzzle::Resources().m_xPinballWallDiffuseTex);
		else
			m_xWallTrimMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		if (TilePuzzle::Resources().m_xPinballWallRMTex)
			m_xWallTrimMaterial.GetDirect()->SetRoughnessMetallicTexture(TilePuzzle::Resources().m_xPinballWallRMTex);

		// Pegs - warm paw-themed with PBR bumper textures
		m_xObstacleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xObstacleMaterial.GetDirect()->SetName("PinballObstacle");
		m_xObstacleMaterial.GetDirect()->SetBaseColor({ 0.55f, 0.35f, 0.3f, 1.f });
		if (TilePuzzle::Resources().m_xPinballBumperDiffuseTex)
			m_xObstacleMaterial.GetDirect()->SetDiffuseTexture(TilePuzzle::Resources().m_xPinballBumperDiffuseTex);
		else
			m_xObstacleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		if (TilePuzzle::Resources().m_xPinballBumperRMTex)
			m_xObstacleMaterial.GetDirect()->SetRoughnessMetallicTexture(TilePuzzle::Resources().m_xPinballBumperRMTex);

		// Plunger - red with chrome shaft texture
		m_xPlungerMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xPlungerMaterial.GetDirect()->SetName("PinballPlunger");
		m_xPlungerMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		m_xPlungerMaterial.GetDirect()->SetBaseColor({ 0.8f, 0.15f, 0.15f, 1.f });
		m_xPlungerMaterial.GetDirect()->SetEmissiveColor(Zenith_Maths::Vector3(0.8f, 0.1f, 0.05f));
		m_xPlungerMaterial.GetDirect()->SetEmissiveIntensity(0.4f);
		if (TilePuzzle::Resources().m_xPinballPlungerRMTex)
			m_xPlungerMaterial.GetDirect()->SetRoughnessMetallicTexture(TilePuzzle::Resources().m_xPinballPlungerRMTex);

		// Target - bright green with chevron texture
		m_xTargetMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xTargetMaterial.GetDirect()->SetName("PinballTarget");
		m_xTargetMaterial.GetDirect()->SetBaseColor({ 0.1f, 0.8f, 0.2f, 1.f });
		m_xTargetMaterial.GetDirect()->SetEmissiveColor(Zenith_Maths::Vector3(0.1f, 0.8f, 0.2f));
		m_xTargetMaterial.GetDirect()->SetEmissiveIntensity(1.5f);
		if (TilePuzzle::Resources().m_xPinballTargetDiffuseTex)
			m_xTargetMaterial.GetDirect()->SetDiffuseTexture(TilePuzzle::Resources().m_xPinballTargetDiffuseTex);
		else
			m_xTargetMaterial.GetDirect()->SetDiffuseTexture(xGridTex);

		// Floor - dark wood playfield with PBR textures
		m_xFloorMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xFloorMaterial.GetDirect()->SetName("PinballFloor");
		m_xFloorMaterial.GetDirect()->SetBaseColor({ 1.f, 1.f, 1.f, 1.f });
		m_xFloorMaterial.GetDirect()->SetEmissiveColor(Zenith_Maths::Vector3(0.02f, 0.02f, 0.06f));
		m_xFloorMaterial.GetDirect()->SetEmissiveIntensity(0.2f);
		if (TilePuzzle::Resources().m_xPinballFloorDiffuseTex)
			m_xFloorMaterial.GetDirect()->SetDiffuseTexture(TilePuzzle::Resources().m_xPinballFloorDiffuseTex);
		else
			m_xFloorMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		if (TilePuzzle::Resources().m_xPinballFloorRMTex)
			m_xFloorMaterial.GetDirect()->SetRoughnessMetallicTexture(TilePuzzle::Resources().m_xPinballFloorRMTex);

		// Lit peg material - warm glow for hit pegs
		m_xPegHitMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xPegHitMaterial.GetDirect()->SetName("PinballPegHit");
		m_xPegHitMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		m_xPegHitMaterial.GetDirect()->SetBaseColor({ 0.7f, 0.5f, 0.4f, 1.f });
		m_xPegHitMaterial.GetDirect()->SetRoughness(0.3f);
		m_xPegHitMaterial.GetDirect()->SetMetallic(0.0f);
		m_xPegHitMaterial.GetDirect()->SetEmissiveColor(Zenith_Maths::Vector3(0.8f, 0.5f, 0.2f));
		m_xPegHitMaterial.GetDirect()->SetEmissiveIntensity(2.0f);

		// Flash peg material - bright warm emissive spike for the moment of impact
		m_xPegFlashMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		m_xPegFlashMaterial.GetDirect()->SetName("PinballPegFlash");
		m_xPegFlashMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
		m_xPegFlashMaterial.GetDirect()->SetBaseColor({ 1.0f, 0.8f, 0.5f, 1.f });
		m_xPegFlashMaterial.GetDirect()->SetRoughness(0.2f);
		m_xPegFlashMaterial.GetDirect()->SetMetallic(0.0f);
		m_xPegFlashMaterial.GetDirect()->SetEmissiveColor(Zenith_Maths::Vector3(1.0f, 0.7f, 0.3f));
		m_xPegFlashMaterial.GetDirect()->SetEmissiveIntensity(5.0f);

		// Use loaded procedural materials when available
		if (TilePuzzle::Resources().m_pxPinballBallMaterial)
			m_xBallMaterial.Set(TilePuzzle::Resources().m_pxPinballBallMaterial);
		if (TilePuzzle::Resources().m_pxPinballPegMaterial)
			m_xObstacleMaterial.Set(TilePuzzle::Resources().m_pxPinballPegMaterial);
		if (TilePuzzle::Resources().m_pxPinballPegHitMaterial)
			m_xPegHitMaterial.Set(TilePuzzle::Resources().m_pxPinballPegHitMaterial);
	}

	// ========================================================================
	// Peg Layout Loading & Spawning
	// ========================================================================

	void LoadPegLayouts()
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(GAME_ASSETS_DIR "Pinball/PegLayouts.bin");

		xStream >> m_uLayoutCount;
		if (m_uLayoutCount > s_uPB_MaxLayouts)
			m_uLayoutCount = s_uPB_MaxLayouts;

		for (uint32_t uLayout = 0; uLayout < m_uLayoutCount; ++uLayout)
		{
			xStream >> m_axLayouts[uLayout].uPegCount;
			if (m_axLayouts[uLayout].uPegCount > s_uPB_MaxPegs)
				m_axLayouts[uLayout].uPegCount = s_uPB_MaxPegs;

			for (uint32_t i = 0; i < m_axLayouts[uLayout].uPegCount; ++i)
			{
				xStream >> m_axLayouts[uLayout].afX[i];
				xStream >> m_axLayouts[uLayout].afY[i];
			}
		}
	}

	void CreatePegs(uint32_t uLayoutIndex)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || uLayoutIndex >= m_uLayoutCount)
			return;

		const PinballPegLayout& xLayout = m_axLayouts[uLayoutIndex];
		for (uint32_t i = 0; i < xLayout.uPegCount; ++i)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "PB_Peg%u", i);
			Zenith_Entity xPeg = CreateStaticSphere(pxScene, szName,
				{ xLayout.afX[i], xLayout.afY[i], 0.f }, 0.4f, m_xObstacleMaterial);
			m_axPegEntityIDs[i] = xPeg.GetEntityID();
		}
	}

	void DestroyPegs()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene)
			return;

		for (uint32_t i = 0; i < s_uPB_MaxPegs; ++i)
		{
			if (m_axPegEntityIDs[i].IsValid() && pxScene->EntityExists(m_axPegEntityIDs[i]))
			{
				Zenith_Entity xPeg = pxScene->GetEntity(m_axPegEntityIDs[i]);
				Zenith_SceneManager::Destroy(xPeg);
			}
			m_axPegEntityIDs[i] = Zenith_EntityID();
		}
	}

	void CreateGatePegs(Zenith_SceneData* pxScene)
	{
		if (!pxScene || !m_bGateActive)
			return;

		for (uint32_t i = 0; i < m_uCurrentGatePegCount; ++i)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "PB_Peg%u", i);
			Zenith_Entity xPeg = CreateStaticSphere(pxScene, szName,
				{ m_xCurrentGateData.afPegPositionsX[i], m_xCurrentGateData.afPegPositionsY[i], 0.f },
				0.4f, m_xObstacleMaterial);
			m_axPegEntityIDs[i] = xPeg.GetEntityID();
		}
	}

	// ========================================================================
	// Playfield Construction
	// ========================================================================

	Zenith_Entity CreateStaticBox(Zenith_SceneData* pxScene, const char* szName,
		const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xScale,
		MaterialHandle& xMaterial, bool bAddCollider = true)
	{
		Zenith_Entity xEntity(pxScene, szName);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(xScale);

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxCubeGeometry, *xMaterial.GetDirect());

		if (bAddCollider)
		{
			Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
			Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);
		}

		return xEntity;
	}

	Zenith_Entity CreateStaticSphere(Zenith_SceneData* pxScene, const char* szName,
		const Zenith_Maths::Vector3& xPos, float fScale,
		MaterialHandle& xMaterial)
	{
		Zenith_Entity xEntity(pxScene, szName);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		// Paw-pad shape: flattened in Z for thematic paw-print look
		xTransform.SetScale(Zenith_Maths::Vector3(fScale, fScale, fScale * 0.6f));

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		Flux_MeshGeometry* pxMesh = TilePuzzle::Resources().m_pxBumperGeometry ? TilePuzzle::Resources().m_pxBumperGeometry : m_pxSphereGeometry;
		xModel.AddMeshEntry(*pxMesh, *xMaterial.GetDirect());

		Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_STATIC);
		Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);

		return xEntity;
	}

	void CreatePlayfield()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene)
			return;

		m_uWallCount = 0;

		// Floor visual (no collider - just for background)
		float fFieldW = s_fPB_FieldRight - s_fPB_FieldLeft;
		float fFieldH = s_fPB_FieldTop - s_fPB_FieldBottom;
		float fCenterX = (s_fPB_FieldLeft + s_fPB_FieldRight) * 0.5f;
		float fCenterY = (s_fPB_FieldBottom + s_fPB_FieldTop) * 0.5f;
		CreateStaticBox(pxScene, "PB_Floor",
			{ fCenterX, fCenterY, 0.15f },
			{ fFieldW, fFieldH, 0.1f },
			m_xFloorMaterial, false);

		// === Boundary Walls ===

		// Left wall
		AddBoundaryWall(pxScene, "PB_WallLeft",
			{ s_fPB_FieldLeft - s_fPB_WallThickness * 0.5f, fCenterY, 0.f },
			{ s_fPB_WallThickness, fFieldH + s_fPB_WallThickness, 0.5f });

		// Right wall
		AddBoundaryWall(pxScene, "PB_WallRight",
			{ s_fPB_FieldRight + s_fPB_WallThickness * 0.5f, fCenterY, 0.f },
			{ s_fPB_WallThickness, fFieldH + s_fPB_WallThickness, 0.5f });

		// Top wall
		AddBoundaryWall(pxScene, "PB_WallTop",
			{ fCenterX, s_fPB_FieldTop + s_fPB_WallThickness * 0.5f, 0.f },
			{ fFieldW + s_fPB_WallThickness * 2.f, s_fPB_WallThickness, 0.5f });

		// Bottom wall left (gap in center for ball exit)
		float fGapHalfWidth = 0.6f;
		float fBottomWallLeftW = (fFieldW * 0.5f - fGapHalfWidth);
		AddBoundaryWall(pxScene, "PB_WallBotL",
			{ s_fPB_FieldLeft + fBottomWallLeftW * 0.5f, s_fPB_FieldBottom - s_fPB_WallThickness * 0.5f, 0.f },
			{ fBottomWallLeftW, s_fPB_WallThickness, 0.5f });

		// Bottom wall right
		AddBoundaryWall(pxScene, "PB_WallBotR",
			{ s_fPB_FieldRight - fBottomWallLeftW * 0.5f, s_fPB_FieldBottom - s_fPB_WallThickness * 0.5f, 0.f },
			{ fBottomWallLeftW, s_fPB_WallThickness, 0.5f });

		// === Launcher Channel ===
		// Right wall of launcher channel (separates channel from main field)
		float fChannelH = 5.5f;
		float fChannelCenterY = s_fPB_ChannelBottom + fChannelH * 0.5f;
		AddWall(pxScene, "PB_ChannelWall",
			{ s_fPB_ChannelRight, fChannelCenterY, 0.f },
			{ s_fPB_WallThickness * 0.5f, fChannelH, 0.5f });

		// === Top Curve (redirect ball from launcher channel into main playfield) ===
		// Ball exits channel at X~-2.0, Y=7.0 moving upward.
		// Positive Z rotation = left end lower, right end higher.
		// Ball hitting the underside gets deflected rightward.
		{
			Zenith_Entity xCurve(pxScene, "PB_TopCurve");
			Zenith_TransformComponent& xT = xCurve.GetComponent<Zenith_TransformComponent>();
			xT.SetPosition({ -1.8f, 7.5f, 0.f });
			xT.SetScale({ 1.6f, s_fPB_WallThickness, 0.5f });
			Zenith_Maths::Quaternion xRot = glm::angleAxis(glm::radians(30.f), Zenith_Maths::Vector3(0.f, 0.f, 1.f));
			xT.SetRotation(glm::normalize(xRot));

			Zenith_ModelComponent& xModel = xCurve.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*m_pxCubeGeometry, *m_xWallMaterial.GetDirect());

			Zenith_ColliderComponent& xCollider = xCurve.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
			Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);
		}

		// Second curve piece - continues guiding ball rightward into main field
		// Ball arrives here traveling right and slightly down after curve 1
		{
			Zenith_Entity xCurve2(pxScene, "PB_TopCurve2");
			Zenith_TransformComponent& xT = xCurve2.GetComponent<Zenith_TransformComponent>();
			xT.SetPosition({ 0.8f, 7.0f, 0.f });
			xT.SetScale({ 1.4f, s_fPB_WallThickness, 0.5f });
			Zenith_Maths::Quaternion xRot = glm::angleAxis(glm::radians(10.f), Zenith_Maths::Vector3(0.f, 0.f, 1.f));
			xT.SetRotation(glm::normalize(xRot));

			Zenith_ModelComponent& xModel = xCurve2.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*m_pxCubeGeometry, *m_xWallMaterial.GetDirect());

			Zenith_ColliderComponent& xCollider = xCurve2.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
			Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);
		}

		// === Pegs ===
		if (m_bGateActive)
		{
			// Use gate-specific peg positions
			CreateGatePegs(pxScene);
		}
		else
		{
			// Freeplay: use pre-generated layouts
			LoadPegLayouts();
			m_uCurrentLayout = 0;
			CreatePegs(m_uCurrentLayout);
		}

		// === Score Target (bottom center) ===
		{
			Zenith_Entity xTarget(pxScene, "PB_Target");
			Zenith_TransformComponent& xT = xTarget.GetComponent<Zenith_TransformComponent>();
			xT.SetPosition({ 0.f, 0.8f, 0.f });
			xT.SetScale({ 1.5f, 0.3f, 0.5f });

			Flux_MeshGeometry* pxMesh = TilePuzzle::Resources().m_pxTargetRampGeometry ? TilePuzzle::Resources().m_pxTargetRampGeometry : m_pxCubeGeometry;
			Zenith_ModelComponent& xModel = xTarget.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*pxMesh, *m_xTargetMaterial.GetDirect());

			Zenith_ColliderComponent& xCollider = xTarget.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
			Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);

			m_xTargetEntityID = xTarget.GetEntityID();
		}

		// === Plunger (visual only, no collider) ===
		{
			Zenith_Entity xPlunger(pxScene, "PB_Plunger");
			Zenith_TransformComponent& xT = xPlunger.GetComponent<Zenith_TransformComponent>();
			xT.SetPosition({ (s_fPB_ChannelLeft + s_fPB_ChannelRight) * 0.5f, s_fPB_PlungerRestY, 0.f });
			xT.SetScale({ 0.5f, 0.4f, 0.3f });

			Flux_MeshGeometry* pxMesh = TilePuzzle::Resources().m_pxPlungerGeometry ? TilePuzzle::Resources().m_pxPlungerGeometry : m_pxCubeGeometry;
			Zenith_ModelComponent& xModel = xPlunger.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*pxMesh, *m_xPlungerMaterial.GetDirect());

			m_xPlungerEntityID = xPlunger.GetEntityID();
		}

		// Back wall to constrain Z movement (front wall omitted so camera can see the playfield)
		AddWall(pxScene, "PB_WallBack",
			{ fCenterX, fCenterY, 0.35f },
			{ fFieldW + s_fPB_WallThickness * 2.f, fFieldH + s_fPB_WallThickness * 2.f, 0.1f });

		// Dynamic lights
		CreateLights(pxScene);
	}

	void CreateLights(Zenith_SceneData* pxScene)
	{
		// Top accent (cool blue)
		{
			Zenith_Entity xLight(pxScene, "PB_LightTop");
			xLight.GetComponent<Zenith_TransformComponent>().SetPosition(
				Zenith_Maths::Vector3(0.f, 7.0f, -1.5f));
			Zenith_LightComponent& xLC = xLight.AddComponent<Zenith_LightComponent>();
			xLC.SetLightType(LIGHT_TYPE_POINT);
			xLC.SetColor(Zenith_Maths::Vector3(0.4f, 0.5f, 1.0f));
			xLC.SetIntensity(200.f);
			xLC.SetRange(8.f);
		}

		// Bottom accent (warm amber)
		{
			Zenith_Entity xLight(pxScene, "PB_LightBottom");
			xLight.GetComponent<Zenith_TransformComponent>().SetPosition(
				Zenith_Maths::Vector3(0.f, 1.0f, -1.5f));
			Zenith_LightComponent& xLC = xLight.AddComponent<Zenith_LightComponent>();
			xLC.SetLightType(LIGHT_TYPE_POINT);
			xLC.SetColor(Zenith_Maths::Vector3(1.0f, 0.6f, 0.2f));
			xLC.SetIntensity(150.f);
			xLC.SetRange(6.f);
		}
	}

	Zenith_Entity CreateBoundaryWall(Zenith_SceneData* pxScene, const char* szName,
		const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xScale)
	{
		Zenith_Entity xEntity(pxScene, szName);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(xScale);

		Flux_MeshGeometry* pxMesh = TilePuzzle::Resources().m_pxBeveledCubeGeometry ? TilePuzzle::Resources().m_pxBeveledCubeGeometry : m_pxCubeGeometry;
		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxMesh, *m_xWallTrimMaterial.GetDirect());

		Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);

		return xEntity;
	}

	void AddWall(Zenith_SceneData* pxScene, const char* szName,
		const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xScale)
	{
		if (m_uWallCount >= s_uPB_MaxWalls)
			return;
		Zenith_Entity xWall = CreateStaticBox(pxScene, szName, xPos, xScale, m_xWallMaterial);
		m_axWallEntityIDs[m_uWallCount] = xWall.GetEntityID();
		m_uWallCount++;
	}

	void AddBoundaryWall(Zenith_SceneData* pxScene, const char* szName,
		const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xScale)
	{
		if (m_uWallCount >= s_uPB_MaxWalls)
			return;
		Zenith_Entity xWall = CreateBoundaryWall(pxScene, szName, xPos, xScale);
		m_axWallEntityIDs[m_uWallCount] = xWall.GetEntityID();
		m_uWallCount++;
	}

	// ========================================================================
	// Ball Management
	// ========================================================================

	void SpawnBall()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene)
			return;

		float fChannelCenterX = (s_fPB_ChannelLeft + s_fPB_ChannelRight) * 0.5f;

		Zenith_Entity xBall(pxScene, "PB_Ball");
		Zenith_TransformComponent& xTransform = xBall.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition({ fChannelCenterX, s_fPB_BallStartY, 0.f });
		xTransform.SetScale(Zenith_Maths::Vector3(s_fPB_BallScale));

		Zenith_ModelComponent& xModel = xBall.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxSphereGeometry, *m_xBallMaterial.GetDirect());

		Zenith_ColliderComponent& xCollider = xBall.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

		// Gravity OFF until launch — ball rests on plunger
		Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

		// Lock rotation for cleaner 2D behavior
		Zenith_Physics::LockRotation(xCollider.GetBodyID(), true, true, true);

		// Zero velocity so ball sits still
		Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0.f));

		// Bounce off walls and obstacles
		Zenith_Physics::SetRestitution(xCollider.GetBodyID(), 0.6f);
		Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);

		// Attach collision behaviour to receive OnCollisionEnter callbacks
		Zenith_ScriptComponent& xScript = xBall.AddComponent<Zenith_ScriptComponent>();
		xScript.AddScript<Pinball_BallBehaviour>();

		m_xBallEntityID = xBall.GetEntityID();
	}

	void DestroyBall()
	{
		if (!m_xBallEntityID.IsValid())
			return;

		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || !pxScene->EntityExists(m_xBallEntityID))
			return;

		Zenith_Entity xBall = pxScene->GetEntity(m_xBallEntityID);
		Zenith_SceneManager::Destroy(xBall);
		m_xBallEntityID = Zenith_EntityID();
	}

	void RespawnBall()
	{
		DestroyBall();

		// Cycle to next peg layout (only in freeplay; gate mode keeps pegs stable)
		if (!m_bGateActive && m_uLayoutCount > 0)
		{
			DestroyPegs();
			m_uCurrentLayout = (m_uCurrentLayout + 1) % m_uLayoutCount;
			CreatePegs(m_uCurrentLayout);
			// Reset peg hit tracking for new layout
			memset(m_abPegHit, 0, sizeof(m_abPegHit));
			memset(m_afPegFlashTimer, 0, sizeof(m_afPegFlashTimer));
			m_uPegsHit = 0;
		}

		SpawnBall();
		m_fPlungerPull = 0.f;
		m_bPlungerDragging = false;
		m_bMouseWasDown = false;
		UpdatePlungerVisual();
	}

	// ========================================================================
	// Input Handling
	// ========================================================================

	void HandleLauncherInput()
	{
		// Keep ball positioned on the plunger while waiting / dragging
		PositionBallOnPlunger();

		bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		Zenith_Maths::Vector2_64 xMousePos64;
		Zenith_Input::GetMousePosition(xMousePos64);
		float fScreenX = static_cast<float>(xMousePos64.x);
		float fScreenY = static_cast<float>(xMousePos64.y);

		if (bMouseDown && !m_bMouseWasDown)
		{
			// Mouse just pressed - check if near plunger
			float fWorldX, fWorldY;
			if (ScreenToWorld(fScreenX, fScreenY, fWorldX, fWorldY))
			{
				float fChannelCenterX = (s_fPB_ChannelLeft + s_fPB_ChannelRight) * 0.5f;
				if (fabsf(fWorldX - fChannelCenterX) < 0.8f && fWorldY < s_fPB_PlungerRestY + 1.0f && fWorldY > s_fPB_ChannelBottom - 0.5f)
				{
					m_bPlungerDragging = true;
					m_fPlungerPull = 0.f;
					m_eState = PINBALL_STATE_LAUNCHING;
				}
			}
		}
		else if (bMouseDown && m_bPlungerDragging)
		{
			// Track drag - pull = how far below rest position
			float fWorldX, fWorldY;
			if (ScreenToWorld(fScreenX, fScreenY, fWorldX, fWorldY))
			{
				float fPull = (s_fPB_PlungerRestY - fWorldY) / s_fPB_PlungerMaxPull;
				m_fPlungerPull = glm::clamp(fPull, 0.f, 1.f);
				UpdatePlungerVisual();
			}
		}
		else if (!bMouseDown && m_bPlungerDragging)
		{
			// Release - launch ball!
			if (m_fPlungerPull > 0.01f)
			{
				LaunchBall();
				m_fPlayingTime = 0.f;
				m_eState = PINBALL_STATE_PLAYING;
			}
			else
			{
				m_eState = PINBALL_STATE_READY;
			}
			m_bPlungerDragging = false;
			m_fPlungerPull = 0.f;
			UpdatePlungerVisual();
		}

		m_bMouseWasDown = bMouseDown;
	}

	void LaunchBall()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || !m_xBallEntityID.IsValid() || !pxScene->EntityExists(m_xBallEntityID))
			return;

		Zenith_Entity xBall = pxScene->GetEntity(m_xBallEntityID);
		if (!xBall.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody())
			return;

		// Enable gravity now that the ball is being launched
		Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), true);

		float fForce = m_fPlungerPull * s_fPB_LaunchForceMax;
		// Launch upward (+Y)
		Zenith_Physics::AddImpulse(xCollider.GetBodyID(),
			Zenith_Maths::Vector3(0.f, fForce, 0.f));
	}

	void PositionBallOnPlunger()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || !m_xBallEntityID.IsValid() || !pxScene->EntityExists(m_xBallEntityID))
			return;

		Zenith_Entity xBall = pxScene->GetEntity(m_xBallEntityID);
		float fChannelCenterX = (s_fPB_ChannelLeft + s_fPB_ChannelRight) * 0.5f;
		float fPlungerY = s_fPB_PlungerRestY - m_fPlungerPull * s_fPB_PlungerMaxPull;
		float fBallY = fPlungerY + 0.35f; // Sit on top of plunger

		Zenith_TransformComponent& xTransform = xBall.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition({ fChannelCenterX, fBallY, 0.f });

		// Keep velocity zeroed while on plunger
		if (xBall.HasComponent<Zenith_ColliderComponent>())
		{
			Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();
			if (xCollider.HasValidBody())
			{
				Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0.f));
			}
		}
	}

	void UpdatePlungerVisual()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || !m_xPlungerEntityID.IsValid() || !pxScene->EntityExists(m_xPlungerEntityID))
			return;

		Zenith_Entity xPlunger = pxScene->GetEntity(m_xPlungerEntityID);
		Zenith_TransformComponent& xTransform = xPlunger.GetComponent<Zenith_TransformComponent>();

		float fChannelCenterX = (s_fPB_ChannelLeft + s_fPB_ChannelRight) * 0.5f;
		float fY = s_fPB_PlungerRestY - m_fPlungerPull * s_fPB_PlungerMaxPull;
		xTransform.SetPosition({ fChannelCenterX, fY, 0.f });
	}

	// ========================================================================
	// Physics Helpers
	// ========================================================================

	void ConstrainBallToPlane()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || !m_xBallEntityID.IsValid() || !pxScene->EntityExists(m_xBallEntityID))
			return;

		Zenith_Entity xBall = pxScene->GetEntity(m_xBallEntityID);
		Zenith_TransformComponent& xTransform = xBall.GetComponent<Zenith_TransformComponent>();

		Zenith_Maths::Vector3 xPos;
		xTransform.GetPosition(xPos);

		if (fabsf(xPos.z) > 0.01f)
		{
			xPos.z = 0.f;
			xTransform.SetPosition(xPos);

			if (xBall.HasComponent<Zenith_ColliderComponent>())
			{
				Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();
				if (xCollider.HasValidBody())
				{
					Zenith_Maths::Vector3 xVel = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
					xVel.z = 0.f;
					Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVel);
				}
			}
		}
	}

	// ========================================================================
	// Collision / Scoring
	// ========================================================================

	void CheckScoringCollisions()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || !m_xBallEntityID.IsValid() || !pxScene->EntityExists(m_xBallEntityID))
			return;

		Zenith_Entity xBall = pxScene->GetEntity(m_xBallEntityID);
		if (!xBall.HasComponent<Zenith_ScriptComponent>())
			return;

		Pinball_BallBehaviour* pxBallBehaviour = xBall.GetComponent<Zenith_ScriptComponent>().GetScript<Pinball_BallBehaviour>();
		if (!pxBallBehaviour)
			return;

		// Check target via collision callback
		if (m_fTargetCooldown <= 0.f && pxBallBehaviour->DidCollideWith(m_xTargetEntityID))
		{
			m_uSessionScore += s_uPB_TargetScore;
			m_xSaveData.uPinballScore += s_uPB_TargetScore;
			m_fTargetCooldown = s_fPB_TargetCooldown;
			m_uTargetHitCount++;

			Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
				TilePuzzle_WriteSaveData, &m_xSaveData);
		}

		// Check peg collisions for hit tracking and material swap
		uint32_t uPegCount = m_bGateActive ? m_uCurrentGatePegCount : s_uPB_MaxPegs;
		for (uint32_t i = 0; i < uPegCount; ++i)
		{
			if (!m_axPegEntityIDs[i].IsValid())
				continue;

			if (pxBallBehaviour->DidCollideWith(m_axPegEntityIDs[i]))
			{
				// Score points for hitting a peg
				if (!m_abPegHit[i])
				{
					m_abPegHit[i] = true;
					m_uPegsHit++;
					m_uSessionScore += s_uPB_PegScore;
					m_xSaveData.uPinballScore += s_uPB_PegScore;

					// Start flash timer and swap to flash material
					// Hit-all-pegs gates: short flash then steady glow
					// Other gates: 1-second emissive then revert to normal
					m_afPegFlashTimer[i] = IsHitAllPegsGate() ? s_fPB_PegFlashDuration : 1.0f;
					SwapPegMaterial(pxScene, i, IsHitAllPegsGate() ? m_xPegFlashMaterial : m_xPegHitMaterial);
				}
			}
		}

		pxBallBehaviour->ClearCollisions();
	}

	// ========================================================================
	// Gate Objective System
	// ========================================================================

	void LoadGateData()
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(GAME_ASSETS_DIR "Pinball/GateData.bin");

		uint32_t uGateCount = 0;
		xStream >> uGateCount;
		if (uGateCount > s_uPB_MaxGates)
			uGateCount = s_uPB_MaxGates;

		for (uint32_t uGate = 0; uGate < uGateCount; ++uGate)
		{
			xStream >> m_axGateData[uGate].eObjectiveType;
			xStream >> m_axGateData[uGate].uScoreThreshold;
			xStream >> m_axGateData[uGate].uTargetHitsRequired;
			xStream >> m_axGateData[uGate].uMaxBalls;
			xStream >> m_axGateData[uGate].uNumPegs;
			for (uint32_t i = 0; i < s_uPB_MaxGatePegs; ++i)
			{
				xStream >> m_axGateData[uGate].afPegPositionsX[i];
				xStream >> m_axGateData[uGate].afPegPositionsY[i];
			}
			xStream >> m_axGateData[uGate].bHasAllPegsObjective;
		}
	}

	void DetermineCurrentGate()
	{
		// Find first uncleared gate
		m_bGateActive = false;
		for (uint32_t i = 0; i < s_uPB_MaxGates; ++i)
		{
			if (!m_xSaveData.IsPinballGateCleared(i))
			{
				m_uCurrentGate = i;
				m_xCurrentGateData = m_axGateData[i];
				m_uCurrentGatePegCount = m_xCurrentGateData.uNumPegs;
				m_bGateActive = true;
				return;
			}
		}
		// All gates cleared - freeplay mode
		m_uCurrentGate = 0;
	}

	void ResetGateAttempt()
	{
		m_uSessionScore = 0;
		m_uPegsHit = 0;
		m_uTargetHitCount = 0;
		m_bGateCleared = false;
		m_bGateFailed = false;
		m_fGateCelebrationTimer = 0.f;
		m_uSessionCoinsEarned = 0;
		m_bFirstClearBonusAwarded = false;
		m_bDailyBonusAwarded = false;
		m_bDailyHintTokenAwarded = false;
		memset(m_abPegHit, 0, sizeof(m_abPegHit));
		memset(m_afPegFlashTimer, 0, sizeof(m_afPegFlashTimer));

		if (m_bGateActive && m_xCurrentGateData.uMaxBalls > 0)
		{
			m_uBallsRemaining = m_xCurrentGateData.uMaxBalls;
		}
		else
		{
			m_uBallsRemaining = 0; // Unlimited
		}
	}

	void RebuildPegsForCurrentGate()
	{
		DestroyPegs();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene)
			return;

		if (m_bGateActive)
		{
			CreateGatePegs(pxScene);
		}
		else if (m_uLayoutCount > 0)
		{
			m_uCurrentLayout = (m_uCurrentLayout + 1) % m_uLayoutCount;
			CreatePegs(m_uCurrentLayout);
		}
	}

	bool IsHitAllPegsGate() const
	{
		if (!m_bGateActive)
			return false;
		return m_xCurrentGateData.eObjectiveType == PINBALL_OBJ_HIT_ALL_PEGS
			|| (m_xCurrentGateData.eObjectiveType == PINBALL_OBJ_COMBINED && m_xCurrentGateData.bHasAllPegsObjective);
	}

	bool CheckGateObjectiveMet() const
	{
		if (!m_bGateActive)
			return false;

		switch (m_xCurrentGateData.eObjectiveType)
		{
		case PINBALL_OBJ_SCORE_THRESHOLD:
			return m_uSessionScore >= m_xCurrentGateData.uScoreThreshold;

		case PINBALL_OBJ_HIT_ALL_PEGS:
			return m_uPegsHit >= m_uCurrentGatePegCount;

		case PINBALL_OBJ_TARGET_HITS:
			return m_uTargetHitCount >= m_xCurrentGateData.uTargetHitsRequired;

		case PINBALL_OBJ_COMBINED:
		{
			bool bMet = true;
			if (m_xCurrentGateData.uScoreThreshold > 0)
				bMet = bMet && (m_uSessionScore >= m_xCurrentGateData.uScoreThreshold);
			if (m_xCurrentGateData.bHasAllPegsObjective)
				bMet = bMet && (m_uPegsHit >= m_uCurrentGatePegCount);
			if (m_xCurrentGateData.uTargetHitsRequired > 0)
				bMet = bMet && (m_uTargetHitCount >= m_xCurrentGateData.uTargetHitsRequired);
			return bMet;
		}
		}
		return false;
	}

	void HandleBallLost()
	{
		// Decrement ball counter for limited-ball objectives
		if (m_bGateActive && m_xCurrentGateData.uMaxBalls > 0 && m_uBallsRemaining > 0)
		{
			m_uBallsRemaining--;
		}

		// Check if objective is met
		if (m_bGateActive && CheckGateObjectiveMet())
		{
			OnGateCleared();
			return;
		}

		// Check if out of balls (gate failed)
		if (m_bGateActive && m_xCurrentGateData.uMaxBalls > 0 && m_uBallsRemaining == 0)
		{
			OnGateFailed();
			return;
		}

		// Normal respawn
		RespawnBall();
		m_eState = PINBALL_STATE_READY;
	}

	void OnGateCleared()
	{
		m_bGateCleared = true;
		m_fGateCelebrationTimer = s_fPB_GateCelebrationDuration;

		// Save gate as cleared
		m_xSaveData.SetPinballGateCleared(m_uCurrentGate);

		// Score-based coin reward: max(5, score / 100)
		uint32_t uCoins = m_uSessionScore / s_uPB_ScoreToCoinDivisor;
		if (uCoins < s_uPB_MinCoinsPerSession) uCoins = s_uPB_MinCoinsPerSession;
		m_uSessionCoinsEarned = uCoins;

		// First-clear bonus: +50 coins + 1 hint token (one-time per gate)
		m_bFirstClearBonusAwarded = false;
		if (!m_xSaveData.HasClaimedFirstClearBonus(m_uCurrentGate))
		{
			m_xSaveData.ClaimFirstClearBonus(m_uCurrentGate);
			m_uSessionCoinsEarned += s_uPB_FirstClearBonus;
			m_xSaveData.AddHintToken(1);
			m_bFirstClearBonusAwarded = true;
		}

		// Daily pinball bonus: score-based coins already awarded above + 1 hint token
		m_bDailyHintTokenAwarded = false;
		uint32_t uToday = GetTodayDate();
		if (m_xSaveData.HasDailyPinballBonus(uToday))
		{
			m_xSaveData.ClaimDailyPinballBonus(uToday);
			m_xSaveData.AddHintToken(1);
			m_bDailyBonusAwarded = true;
			m_bDailyHintTokenAwarded = true;
		}

		m_xSaveData.AddCoins(static_cast<int32_t>(m_uSessionCoinsEarned));

		// Unlock next puzzle level now that gate is cleared
		uint32_t uGateLevel = (m_uCurrentGate + 1) * 10;
		if (uGateLevel < TilePuzzleSaveData::uMAX_LEVELS)
		{
			if (uGateLevel >= m_xSaveData.uHighestLevelReached)
			{
				m_xSaveData.uHighestLevelReached = uGateLevel + 1;
			}
			// Advance current level past the gate so Continue doesn't replay it
			if (m_xSaveData.uCurrentLevel <= uGateLevel)
			{
				m_xSaveData.uCurrentLevel = uGateLevel + 1;
			}
		}

		// Mark gate level as completed in level records for unified level select
		if (uGateLevel > 0 && uGateLevel <= TilePuzzleSaveData::uMAX_LEVELS)
		{
			m_xSaveData.axLevelRecords[uGateLevel - 1].bCompleted = true;
			m_xSaveData.SetStarRating(uGateLevel, 3);
		}

		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Destroy ball during celebration
		DestroyBall();
		m_eState = PINBALL_STATE_READY;
	}

	void OnGateFailed()
	{
		m_bGateFailed = true;
		m_fGateCelebrationTimer = s_fPB_GateCelebrationDuration;

		// Destroy ball during failure message
		DestroyBall();
		m_eState = PINBALL_STATE_READY;
	}

	// ========================================================================
	// Pinball Victory Overlay
	// ========================================================================

	void ShowPinballVictoryOverlay()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIElement* pxBg = xUI.FindElement("PinballVictoryBg");
		if (pxBg) pxBg->SetVisible(true);

		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballVictoryTitle");
		if (pxTitle) pxTitle->SetVisible(true);

		Zenith_UI::Zenith_UIText* pxCoins = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballVictoryCoins");
		if (pxCoins)
		{
			char szCoins[64];
			snprintf(szCoins, sizeof(szCoins), "+%u coins", m_uSessionCoinsEarned);
			pxCoins->SetText(szCoins);
			pxCoins->SetVisible(true);
		}

		Zenith_UI::Zenith_UIButton* pxNextBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("PinballVictoryNextBtn");
		if (pxNextBtn)
		{
			// Check if there are more levels
			uint32_t uGateLevel = (m_uCurrentGate + 1) * 10;
			if (uGateLevel >= TilePuzzleSaveData::uMAX_LEVELS)
			{
				pxNextBtn->SetText("Menu");
			}
			else
			{
				pxNextBtn->SetText("Next Level");
			}
			pxNextBtn->SetVisible(true);
		}

		// Hide HUD elements behind overlay
		Zenith_UI::Zenith_UIElement* pxGateStatus = xUI.FindElement("PinballGateStatus");
		if (pxGateStatus) pxGateStatus->SetVisible(false);
	}

	void HidePinballVictoryOverlay()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		const char* aszElements[] = {
			"PinballVictoryBg", "PinballVictoryTitle",
			"PinballVictoryCoins", "PinballVictoryNextBtn"
		};
		for (const char* szName : aszElements)
		{
			Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement(szName);
			if (pxElem) pxElem->SetVisible(false);
		}
	}

	static void OnPinballNextLevelClicked(void* pxUserData)
	{
		Pinball_Behaviour* pxSelf = static_cast<Pinball_Behaviour*>(pxUserData);
		if (pxSelf->m_eState != PINBALL_STATE_LEVEL_COMPLETE)
			return;

		pxSelf->HidePinballVictoryOverlay();

		uint32_t uGateLevel = (pxSelf->m_uCurrentGate + 1) * 10;
		uint32_t uNextLevel = uGateLevel + 1;

		if (uNextLevel > TilePuzzleSaveData::uMAX_LEVELS)
		{
			// No more levels, return to menu
			pxSelf->m_bGateCleared = false;
			pxSelf->m_bDailyBonusAwarded = false;
			pxSelf->ReturnToMenu();
			return;
		}

		// Check if next level is another pinball gate
		uint32_t uNextGateIndex = 0;
		if (TilePuzzle_IsGateLevel(uNextLevel, &uNextGateIndex)
			&& !pxSelf->m_xSaveData.IsPinballGateCleared(uNextGateIndex))
		{
			// Stay in pinball scene, load next gate
			pxSelf->m_bGateCleared = false;
			pxSelf->m_bDailyBonusAwarded = false;
			pxSelf->m_uCurrentGate = uNextGateIndex;
			pxSelf->m_xCurrentGateData = pxSelf->m_axGateData[uNextGateIndex];
			pxSelf->m_uCurrentGatePegCount = pxSelf->m_xCurrentGateData.uNumPegs;
			pxSelf->RebuildPegsForCurrentGate();
			pxSelf->ResetGateAttempt();
			pxSelf->RespawnBall();
			pxSelf->m_eState = PINBALL_STATE_READY;
		}
		else
		{
			// Next level is a puzzle - save and transition
			pxSelf->m_bGateCleared = false;
			pxSelf->m_bDailyBonusAwarded = false;
			pxSelf->m_xSaveData.uCurrentLevel = uNextLevel;
			Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
				TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);
			pxSelf->ReturnToMenuAndLoadPuzzle();
		}
	}

	void ReturnToMenuAndLoadPuzzle()
	{
		// Save and transition to puzzle scene
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Restore HDR defaults
		g_xEngine.HDR().SetBloomIntensity(0.5f);
		g_xEngine.HDR().SetBloomThreshold(1.0f);
		g_xEngine.HDR().SetExposure(1.0f);

		// Cleanup dynamic scene
		if (m_xPinballScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xPinballScene);
			m_xPinballScene = Zenith_Scene();
		}

		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	// ========================================================================
	// Peg Visual Feedback
	// ========================================================================

	void SwapPegMaterial(Zenith_SceneData* pxScene, uint32_t uPegIndex, MaterialHandle& xMaterial)
	{
		if (!m_axPegEntityIDs[uPegIndex].IsValid() || !pxScene->EntityExists(m_axPegEntityIDs[uPegIndex]))
			return;

		Zenith_Entity xPeg = pxScene->GetEntity(m_axPegEntityIDs[uPegIndex]);
		if (xPeg.HasComponent<Zenith_ModelComponent>())
		{
			Zenith_ModelComponent& xModel = xPeg.GetComponent<Zenith_ModelComponent>();
			if (xModel.GetNumMeshes() > 0 && xModel.GetModelInstance())
			{
				xModel.GetModelInstance()->SetMaterial(0, xMaterial.GetDirect());
			}
		}
	}

	void UpdatePegFlashTimers(float fDeltaTime)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene)
			return;

		for (uint32_t i = 0; i < s_uPB_MaxPegs; ++i)
		{
			if (m_afPegFlashTimer[i] > 0.f)
			{
				m_afPegFlashTimer[i] -= fDeltaTime;
				if (m_afPegFlashTimer[i] <= 0.f)
				{
					m_afPegFlashTimer[i] = 0.f;
					if (m_abPegHit[i])
					{
						if (IsHitAllPegsGate())
						{
							// Hit-all-pegs: flash -> steady emissive (stays lit)
							SwapPegMaterial(pxScene, i, m_xPegHitMaterial);
						}
						else
						{
							// Other gates: revert to normal material
							SwapPegMaterial(pxScene, i, m_xObstacleMaterial);
							m_abPegHit[i] = false;
						}
					}
				}
			}
		}
	}

	// ========================================================================
	// HUD Element Creation
	// ========================================================================

	void CreateHUDElements()
	{
		if (m_bHUDCreated)
			return;
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Objective text - top left
		Zenith_UI::Zenith_UIText* pxObjective = xUI.CreateText("PinballObjective", "");
		pxObjective->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxObjective->SetPosition(20.f, TilePuzzleUI::fPB_OBJECTIVE_Y);
		pxObjective->SetFontSize(TilePuzzleUI::fPB_OBJECTIVE_FONT);
		pxObjective->SetColor({ 1.f, 0.9f, 0.5f, 1.f });

		// Peg counter - below objective
		Zenith_UI::Zenith_UIText* pxPegCount = xUI.CreateText("PinballPegCount", "");
		pxPegCount->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxPegCount->SetPosition(20.f, TilePuzzleUI::fPB_PEG_COUNT_Y);
		pxPegCount->SetFontSize(TilePuzzleUI::fPB_COUNTER_FONT);
		pxPegCount->SetColor({ 0.7f, 1.f, 0.9f, 1.f });

		// Target hit counter - below peg counter
		Zenith_UI::Zenith_UIText* pxTargetCount = xUI.CreateText("PinballTargetCount", "");
		pxTargetCount->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxTargetCount->SetPosition(20.f, TilePuzzleUI::fPB_TARGET_COUNT_Y);
		pxTargetCount->SetFontSize(TilePuzzleUI::fPB_COUNTER_FONT);
		pxTargetCount->SetColor({ 0.7f, 1.f, 0.9f, 1.f });

		// Balls remaining - top right
		Zenith_UI::Zenith_UIText* pxBalls = xUI.CreateText("PinballBalls", "");
		pxBalls->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxBalls->SetPosition(-20.f, TilePuzzleUI::fPB_BALLS_Y);
		pxBalls->SetFontSize(TilePuzzleUI::fPB_BALLS_FONT);
		pxBalls->SetAlignment(Zenith_UI::TextAlignment::Right);
		pxBalls->SetColor({ 1.f, 0.7f, 0.5f, 1.f });

		// Gate status (cleared/failed) - center screen
		Zenith_UI::Zenith_UIText* pxGateStatus = xUI.CreateText("PinballGateStatus", "");
		pxGateStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxGateStatus->SetPosition(0.f, TilePuzzleUI::fPB_GATE_STATUS_Y);
		pxGateStatus->SetFontSize(TilePuzzleUI::fPB_GATE_STATUS_FONT);
		pxGateStatus->SetAlignment(Zenith_UI::TextAlignment::Center);
		pxGateStatus->SetColor({ 1.f, 1.f, 0.3f, 1.f });
		pxGateStatus->SetVisible(false);

		// Gate number display - top center, below score
		Zenith_UI::Zenith_UIText* pxGateNum = xUI.CreateText("PinballGateNum", "");
		pxGateNum->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopCenter);
		pxGateNum->SetPosition(0.f, TilePuzzleUI::fPB_TOP_PADDING + 130.f);
		pxGateNum->SetFontSize(TilePuzzleUI::fPB_GATE_NUM_FONT);
		pxGateNum->SetAlignment(Zenith_UI::TextAlignment::Center);
		pxGateNum->SetColor({ 0.6f, 0.6f, 0.8f, 1.f });

		// Victory overlay elements (initially hidden)
		Zenith_UI::Zenith_UIRect* pxVictoryBg = xUI.CreateRect("PinballVictoryBg");
		pxVictoryBg->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxVictoryBg->SetPosition(0.f, 0.f);
		pxVictoryBg->SetSize(9999.f, 9999.f);
		pxVictoryBg->SetColor({ 0.05f, 0.05f, 0.15f, 0.9f });
		pxVictoryBg->SetVisible(false);

		Zenith_UI::Zenith_UIText* pxVictoryTitle = xUI.CreateText("PinballVictoryTitle", "Level Complete!");
		pxVictoryTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxVictoryTitle->SetPosition(0.f, -80.f);
		pxVictoryTitle->SetFontSize(64.f);
		pxVictoryTitle->SetAlignment(Zenith_UI::TextAlignment::Center);
		pxVictoryTitle->SetColor({ 1.f, 1.f, 0.5f, 1.f });
		pxVictoryTitle->SetVisible(false);

		Zenith_UI::Zenith_UIText* pxVictoryCoins = xUI.CreateText("PinballVictoryCoins", "");
		pxVictoryCoins->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxVictoryCoins->SetPosition(0.f, 0.f);
		pxVictoryCoins->SetFontSize(44.f);
		pxVictoryCoins->SetAlignment(Zenith_UI::TextAlignment::Center);
		pxVictoryCoins->SetColor({ 1.f, 0.85f, 0.2f, 1.f });
		pxVictoryCoins->SetVisible(false);

		Zenith_UI::Zenith_UIButton* pxVictoryNextBtn = xUI.CreateButton("PinballVictoryNextBtn", "Next Level");
		pxVictoryNextBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxVictoryNextBtn->SetPosition(0.f, 80.f);
		pxVictoryNextBtn->SetSize(280.f, 70.f);
		pxVictoryNextBtn->SetFontSize(40.f);
		pxVictoryNextBtn->SetNormalColor({ 0.15f, 0.4f, 0.2f, 1.f });
		pxVictoryNextBtn->SetHoverColor({ 0.25f, 0.55f, 0.3f, 1.f });
		pxVictoryNextBtn->SetPressedColor({ 0.1f, 0.3f, 0.15f, 1.f });
		pxVictoryNextBtn->SetOnClick(&OnPinballNextLevelClicked, this);
		pxVictoryNextBtn->SetVisible(false);

		m_bHUDCreated = true;
	}

	void CheckBallLost()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
		if (!pxScene || !m_xBallEntityID.IsValid() || !pxScene->EntityExists(m_xBallEntityID))
			return;

		Zenith_Entity xBall = pxScene->GetEntity(m_xBallEntityID);
		Zenith_Maths::Vector3 xPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);

		// Ball reached the bottom area of the playfield (on bottom walls or through the gap)
		if (xPos.y < s_fPB_FieldBottom + 0.5f)
		{
			m_eState = PINBALL_STATE_BALL_LOST;
			return;
		}

		// Ball fell back into the launcher channel (grace period avoids false trigger right after launch)
		if (m_fPlayingTime > 0.3f
			&& xPos.x < s_fPB_ChannelRight + s_fPB_BallRadius
			&& xPos.y < s_fPB_PlungerRestY)
		{
			m_eState = PINBALL_STATE_BALL_LOST;
		}
	}

	// ========================================================================
	// Screen-to-World Conversion
	// ========================================================================

	bool ScreenToWorld(float fScreenX, float fScreenY, float& fWorldX, float& fWorldY)
	{
		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return false;

		Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

		Zenith_Maths::Vector3 xNear = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 0.f));
		Zenith_Maths::Vector3 xFar = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 1.f));

		Zenith_Maths::Vector3 xDir = xFar - xNear;
		if (fabsf(xDir.z) < 1e-6f)
			return false;

		// Intersect ray with Z=0 plane
		float fT = (0.f - xNear.z) / xDir.z;
		if (fT < 0.f)
			return false;

		fWorldX = xNear.x + fT * xDir.x;
		fWorldY = xNear.y + fT * xDir.y;
		return true;
	}

	// ========================================================================
	// UI
	// ========================================================================

	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		char szBuffer[128];

		// Score display (top center, existing element)
		snprintf(szBuffer, sizeof(szBuffer), "Score: %u", m_uSessionScore);
		Zenith_UI::Zenith_UIText* pxScore = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballScore");
		if (pxScore) pxScore->SetText(szBuffer);

		snprintf(szBuffer, sizeof(szBuffer), "Total: %u", m_xSaveData.uPinballScore);
		Zenith_UI::Zenith_UIText* pxHighScore = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballHighScore");
		if (pxHighScore) pxHighScore->SetText(szBuffer);

		// Objective text
		Zenith_UI::Zenith_UIText* pxObjective = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballObjective");
		if (pxObjective)
		{
			if (m_bGateActive)
			{
				GetObjectiveText(szBuffer, sizeof(szBuffer));
				pxObjective->SetText(szBuffer);
				pxObjective->SetVisible(true);
			}
			else
			{
				pxObjective->SetText("Freeplay - All gates cleared!");
				pxObjective->SetVisible(true);
			}
		}

		// Peg hit counter (visible when relevant)
		Zenith_UI::Zenith_UIText* pxPegCount = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballPegCount");
		if (pxPegCount)
		{
			if (m_bGateActive && (m_xCurrentGateData.eObjectiveType == PINBALL_OBJ_HIT_ALL_PEGS
				|| m_xCurrentGateData.bHasAllPegsObjective))
			{
				snprintf(szBuffer, sizeof(szBuffer), "Pegs: %u/%u", m_uPegsHit, m_uCurrentGatePegCount);
				pxPegCount->SetText(szBuffer);
				pxPegCount->SetVisible(true);
			}
			else
			{
				pxPegCount->SetVisible(false);
			}
		}

		// Target hit counter (visible when relevant)
		Zenith_UI::Zenith_UIText* pxTargetCount = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballTargetCount");
		if (pxTargetCount)
		{
			if (m_bGateActive && (m_xCurrentGateData.eObjectiveType == PINBALL_OBJ_TARGET_HITS
				|| (m_xCurrentGateData.eObjectiveType == PINBALL_OBJ_COMBINED && m_xCurrentGateData.uTargetHitsRequired > 0)))
			{
				snprintf(szBuffer, sizeof(szBuffer), "Targets: %u/%u", m_uTargetHitCount, m_xCurrentGateData.uTargetHitsRequired);
				pxTargetCount->SetText(szBuffer);
				pxTargetCount->SetVisible(true);
			}
			else
			{
				pxTargetCount->SetVisible(false);
			}
		}

		// Balls remaining (visible when limited)
		Zenith_UI::Zenith_UIText* pxBalls = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballBalls");
		if (pxBalls)
		{
			if (m_bGateActive && m_xCurrentGateData.uMaxBalls > 0)
			{
				snprintf(szBuffer, sizeof(szBuffer), "Balls: %u", m_uBallsRemaining);
				pxBalls->SetText(szBuffer);
				pxBalls->SetVisible(true);
			}
			else
			{
				pxBalls->SetVisible(false);
			}
		}

		// Gate status overlay (celebration / failure)
		Zenith_UI::Zenith_UIText* pxGateStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballGateStatus");
		if (pxGateStatus)
		{
			if (m_bGateCleared)
			{
				char szRewards[128];
				int iLen = snprintf(szRewards, sizeof(szRewards), "Gate %u Cleared! +%u Coins", m_uCurrentGate + 1, m_uSessionCoinsEarned);
				if (m_bFirstClearBonusAwarded && iLen < static_cast<int>(sizeof(szRewards)))
					iLen += snprintf(szRewards + iLen, sizeof(szRewards) - iLen, " (incl. +%u bonus)", s_uPB_FirstClearBonus);
				if (m_bDailyHintTokenAwarded && iLen < static_cast<int>(sizeof(szRewards)))
					snprintf(szRewards + iLen, sizeof(szRewards) - iLen, " +Hint Token!");
				pxGateStatus->SetText(szRewards);
				pxGateStatus->SetColor({ 0.2f, 1.f, 0.3f, 1.f });
				pxGateStatus->SetVisible(true);
			}
			else if (m_bGateFailed)
			{
				pxGateStatus->SetText("Try Again");
				pxGateStatus->SetColor({ 1.f, 0.3f, 0.2f, 1.f });
				pxGateStatus->SetVisible(true);
			}
			else
			{
				pxGateStatus->SetVisible(false);
			}
		}

		// Gate number
		Zenith_UI::Zenith_UIText* pxGateNum = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballGateNum");
		if (pxGateNum)
		{
			if (m_bGateActive)
			{
				snprintf(szBuffer, sizeof(szBuffer), "Gate %u / %u", m_uCurrentGate + 1, s_uPB_MaxGates);
				pxGateNum->SetText(szBuffer);
				pxGateNum->SetVisible(true);
			}
			else
			{
				pxGateNum->SetVisible(false);
			}
		}
	}

	void GetObjectiveText(char* szBuffer, size_t uBufferSize) const
	{
		switch (m_xCurrentGateData.eObjectiveType)
		{
		case PINBALL_OBJ_SCORE_THRESHOLD:
			snprintf(szBuffer, uBufferSize, "Score %u points", m_xCurrentGateData.uScoreThreshold);
			break;
		case PINBALL_OBJ_HIT_ALL_PEGS:
			snprintf(szBuffer, uBufferSize, "Hit all %u pegs", m_uCurrentGatePegCount);
			break;
		case PINBALL_OBJ_TARGET_HITS:
			snprintf(szBuffer, uBufferSize, "Hit the target %u times", m_xCurrentGateData.uTargetHitsRequired);
			break;
		case PINBALL_OBJ_COMBINED:
		{
			// Build combined objective text
			if (m_xCurrentGateData.uScoreThreshold > 0 && m_xCurrentGateData.bHasAllPegsObjective)
			{
				snprintf(szBuffer, uBufferSize, "Score %u + Hit all pegs", m_xCurrentGateData.uScoreThreshold);
			}
			else if (m_xCurrentGateData.uScoreThreshold > 0 && m_xCurrentGateData.uTargetHitsRequired > 0)
			{
				snprintf(szBuffer, uBufferSize, "Score %u + %u target hits", m_xCurrentGateData.uScoreThreshold, m_xCurrentGateData.uTargetHitsRequired);
			}
			else
			{
				snprintf(szBuffer, uBufferSize, "Complete all objectives");
			}
			break;
		}
		}
	}

	// ========================================================================
	// Gate Selection UI
	// ========================================================================

	void SetGateSelectVisible(bool bVisible)
	{
		if (m_pxGateSelectBg) m_pxGateSelectBg->SetVisible(bVisible);
		if (m_pxGateSelectTitle) m_pxGateSelectTitle->SetVisible(bVisible);
		if (m_pxGateBackBtn) m_pxGateBackBtn->SetVisible(bVisible);

		// Update gate button states and visibility
		bool bAllCleared = true;
		for (uint32_t i = 0; i < s_uPB_MaxGates; ++i)
		{
			if (!m_xSaveData.IsPinballGateCleared(i))
			{
				bAllCleared = false;
				break;
			}
		}

		for (uint32_t i = 0; i < s_uPB_MaxGates; ++i)
		{
			if (!m_apxGateBtns[i]) continue;
			m_apxGateBtns[i]->SetVisible(bVisible);

			if (bVisible)
			{
				bool bCleared = m_xSaveData.IsPinballGateCleared(i);
				bool bSelectable = bCleared || (m_bGateActive && i == m_uCurrentGate);

				// Update button color based on state
				if (bCleared)
				{
					m_apxGateBtns[i]->SetNormalColor(Zenith_Maths::Vector4(0.15f, 0.4f, 0.7f, 1.f));
					m_apxGateBtns[i]->SetHoverColor(Zenith_Maths::Vector4(0.25f, 0.5f, 0.8f, 1.f));
				}
				else if (bSelectable)
				{
					m_apxGateBtns[i]->SetNormalColor(Zenith_Maths::Vector4(0.2f, 0.5f, 0.3f, 1.f));
					m_apxGateBtns[i]->SetHoverColor(Zenith_Maths::Vector4(0.3f, 0.6f, 0.4f, 1.f));
				}
				else
				{
					m_apxGateBtns[i]->SetNormalColor(Zenith_Maths::Vector4(0.3f, 0.3f, 0.3f, 1.f));
					m_apxGateBtns[i]->SetHoverColor(Zenith_Maths::Vector4(0.3f, 0.3f, 0.3f, 1.f));
				}

				// Update button text to show status
				char szLabel[16];
				if (bCleared)
					snprintf(szLabel, sizeof(szLabel), "%u OK", i + 1);
				else if (!bSelectable)
					snprintf(szLabel, sizeof(szLabel), "%u", i + 1);
				else
					snprintf(szLabel, sizeof(szLabel), "%u", i + 1);
				m_apxGateBtns[i]->SetText(szLabel);

				// Disable interaction for locked gates
				m_apxGateBtns[i]->SetGroupInteractable(bSelectable);
			}
		}

		// Freeplay button only visible when all gates cleared
		if (m_pxGateFreeplayBtn)
			m_pxGateFreeplayBtn->SetVisible(bVisible && bAllCleared);
	}

	void UpdateGateSelectUI()
	{
		// Widget-based gate select handles its own rendering and input
		// Just ensure visibility is set correctly on first frame
		if (m_pxGateSelectBg && !m_pxGateSelectBg->IsVisible())
		{
			SetGateSelectVisible(true);
		}
	}

	void EnterGateFromSelect(uint32_t uGateIndex)
	{
		m_uCurrentGate = uGateIndex;
		m_xCurrentGateData = m_axGateData[uGateIndex];
		m_uCurrentGatePegCount = m_xCurrentGateData.uNumPegs;
		m_bGateActive = true;

		// Create dynamic scene and enter gameplay
		if (!m_xPinballScene.IsValid())
		{
			m_xPinballScene = Zenith_SceneManager::CreateEmptyScene("PinballPlay");
			Zenith_SceneManager::SetActiveScene(m_xPinballScene);

			g_xEngine.HDR().SetBloomIntensity(0.8f);
			g_xEngine.HDR().SetBloomThreshold(0.8f);
			g_xEngine.HDR().SetExposure(1.2f);
			Zenith_GraphicsOptions::Get().m_xSkyboxColour = Zenith_Maths::Vector3(0.02f, 0.02f, 0.06f);

			CreatePlayfield();
			SpawnBall();
		}
		else
		{
			// Rebuild pegs for selected gate
			RebuildPegsForCurrentGate();
			RespawnBall();
		}

		ResetGateAttempt();
		m_eState = PINBALL_STATE_READY;
	}

	void EnterFreeplayFromSelect()
	{
		m_bGateActive = false;
		m_uCurrentGate = 0;

		// Create dynamic scene and enter gameplay
		if (!m_xPinballScene.IsValid())
		{
			m_xPinballScene = Zenith_SceneManager::CreateEmptyScene("PinballPlay");
			Zenith_SceneManager::SetActiveScene(m_xPinballScene);

			g_xEngine.HDR().SetBloomIntensity(0.8f);
			g_xEngine.HDR().SetBloomThreshold(0.8f);
			g_xEngine.HDR().SetExposure(1.2f);
			Zenith_GraphicsOptions::Get().m_xSkyboxColour = Zenith_Maths::Vector3(0.02f, 0.02f, 0.06f);

			CreatePlayfield();
			SpawnBall();
		}
		else
		{
			DestroyPegs();
			LoadPegLayouts();
			m_uCurrentLayout = 0;
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(m_xPinballScene);
			if (pxScene)
			{
				CreatePegs(m_uCurrentLayout);
			}
			RespawnBall();
		}

		ResetGateAttempt();
		m_eState = PINBALL_STATE_READY;
	}

	// ========================================================================
	// Pinball Gate Tutorial
	// ========================================================================

	static constexpr uint32_t uPINBALL_TUTORIAL_INDEX = 2;
	static constexpr float s_fPinballTutorialFadeDuration = 0.3f;

	void TryShowPinballTutorial()
	{
		if (m_xSaveData.IsTutorialShown(uPINBALL_TUTORIAL_INDEX))
			return;

		m_bTutorialActive = true;
		m_fTutorialTimer = 0.0f;
		m_bTutorialMouseWasDown = false;

		if (m_pxTutorialOverlay)
		{
			int32_t iWinWidth, iWinHeight;
			Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
			float fScreenW = static_cast<float>(iWinWidth);
			float fOverlayW = glm::min(fScreenW - 40.f, 810.f);
			m_pxTutorialOverlay->SetContentSize(fOverlayW, 180.f);

			if (m_pxTutorialText)
			{
				float fTextW = fOverlayW - 60.f;
				m_pxTutorialText->SetSize(fTextW, 100.f);

				const char* szText = "Every 10 levels is a pinball gate.\nClear it to continue!";
				float fDesiredFont = 48.f;
				// Find longest line
				uint32_t uMaxLineLen = 0, uCurrentLen = 0;
				for (const char* p = szText; *p; ++p)
				{
					if (*p == '\n') { if (uCurrentLen > uMaxLineLen) uMaxLineLen = uCurrentLen; uCurrentLen = 0; }
					else uCurrentLen++;
				}
				if (uCurrentLen > uMaxLineLen) uMaxLineLen = uCurrentLen;
				if (uMaxLineLen > 0)
				{
					float fLineWidth = static_cast<float>(uMaxLineLen) * fDesiredFont * fCHAR_SPACING;
					if (fLineWidth > fTextW)
						fDesiredFont = fTextW / (static_cast<float>(uMaxLineLen) * fCHAR_SPACING);
				}
				m_pxTutorialText->SetFontSize(fDesiredFont);
				m_pxTutorialText->SetText(szText);
			}
			m_pxTutorialOverlay->Show();
		}
	}

	void UpdatePinballTutorial(float fDeltaTime)
	{
		m_fTutorialTimer += fDeltaTime / s_fPinballTutorialFadeDuration;
		if (m_fTutorialTimer > 1.0f)
			m_fTutorialTimer = 1.0f;

		if (m_pxTutorialHintText)
		{
			float fHintAlpha = 0.5f + 0.5f * sinf(m_fTutorialTimer * 6.0f);
			m_pxTutorialHintText->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, fHintAlpha));
		}

		bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		if (bMouseDown && !m_bTutorialMouseWasDown && m_fTutorialTimer >= 0.5f)
		{
			m_bTutorialActive = false;
			if (m_pxTutorialOverlay)
				m_pxTutorialOverlay->Hide();
			m_xSaveData.SetTutorialShown(uPINBALL_TUTORIAL_INDEX);
			Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
				TilePuzzle_WriteSaveData, &m_xSaveData);
		}
		m_bTutorialMouseWasDown = bMouseDown;
	}

	// ========================================================================
	// Date Helper
	// ========================================================================

	static uint32_t GetTodayDate()
	{
		time_t xTime = time(nullptr);
		struct tm xTm;
#ifdef ZENITH_WINDOWS
		localtime_s(&xTm, &xTime);
#else
		localtime_r(&xTime, &xTm);
#endif
		return static_cast<uint32_t>((xTm.tm_year + 1900) * 10000 + (xTm.tm_mon + 1) * 100 + xTm.tm_mday);
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Game state
	PinballState m_eState;
	uint32_t m_uSessionScore;
	TilePuzzleSaveData m_xSaveData;

	// Tutorial overlay
	Zenith_UI::Zenith_UIOverlay* m_pxTutorialOverlay = nullptr;
	Zenith_UI::Zenith_UIText* m_pxTutorialText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxTutorialHintText = nullptr;
	bool m_bTutorialActive = false;
	float m_fTutorialTimer = 0.0f;
	bool m_bTutorialMouseWasDown = false;

	// Entity IDs (dynamic scene)
	Zenith_EntityID m_xBallEntityID;
	Zenith_EntityID m_xPlungerEntityID;
	Zenith_EntityID m_xTargetEntityID;
	Zenith_EntityID m_axWallEntityIDs[s_uPB_MaxWalls];
	uint32_t m_uWallCount;
	Zenith_EntityID m_axPegEntityIDs[s_uPB_MaxPegs];

	// Peg layouts (loaded from disk, freeplay mode)
	PinballPegLayout m_axLayouts[s_uPB_MaxLayouts];
	uint32_t m_uLayoutCount;
	uint32_t m_uCurrentLayout;

	// Launcher state
	float m_fPlungerPull;
	bool m_bPlungerDragging;
	bool m_bMouseWasDown;

	// Scoring cooldowns
	float m_fTargetCooldown;
	float m_fPlayingTime;

	// Scene handle
	Zenith_Scene m_xPinballScene;

	// Cached resources
	Flux_MeshGeometry* m_pxCubeGeometry;
	Flux_MeshGeometry* m_pxSphereGeometry;

	// Materials
	MaterialHandle m_xBallMaterial;
	MaterialHandle m_xWallMaterial;
	MaterialHandle m_xObstacleMaterial;
	MaterialHandle m_xPlungerMaterial;
	MaterialHandle m_xTargetMaterial;
	MaterialHandle m_xFloorMaterial;
	MaterialHandle m_xPegHitMaterial;
	MaterialHandle m_xPegFlashMaterial;
	MaterialHandle m_xWallTrimMaterial;

	// Gate objective system
	PinballGateData m_axGateData[s_uPB_MaxGates];
	PinballGateData m_xCurrentGateData;
	uint32_t m_uCurrentGate;
	uint32_t m_uCurrentGatePegCount;
	bool m_bGateActive;
	bool m_bGateCleared;
	bool m_bGateFailed;
	float m_fGateCelebrationTimer;

	// Peg hit tracking
	bool m_abPegHit[s_uPB_MaxPegs];
	float m_afPegFlashTimer[s_uPB_MaxPegs];
	uint32_t m_uPegsHit;

	// Target hit tracking
	uint32_t m_uTargetHitCount;

	// Ball counter (limited-ball objectives)
	uint32_t m_uBallsRemaining;

	// HUD state
	bool m_bHUDCreated;

	// Session rewards
	uint32_t m_uSessionCoinsEarned = 0;
	bool m_bFirstClearBonusAwarded = false;
	bool m_bDailyBonusAwarded = false;
	bool m_bDailyHintTokenAwarded = false;

	// Gate select UI (widget-based)
	Zenith_UI::Zenith_UIElement* m_pxGateSelectBg = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxGateSelectTitle = nullptr;
	Zenith_UI::Zenith_UIButton* m_apxGateBtns[s_uPB_MaxGates] = {};
	Zenith_UI::Zenith_UIButton* m_pxGateFreeplayBtn = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxGateBackBtn = nullptr;

	struct GateButtonCallbackData
	{
		Pinball_Behaviour* pxBehaviour;
		uint32_t uGateIndex;
	};
	static GateButtonCallbackData s_axGateButtonData[s_uPB_MaxGates];
};

Pinball_Behaviour::GateButtonCallbackData Pinball_Behaviour::s_axGateButtonData[10] = {};
