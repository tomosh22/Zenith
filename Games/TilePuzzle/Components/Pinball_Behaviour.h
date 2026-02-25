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

#include "TilePuzzle/Components/TilePuzzle_SaveData.h"
#include "SaveData/Zenith_SaveData.h"

#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <cmath>
#include <filesystem>
#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Forward declarations for TilePuzzle shared resources
namespace TilePuzzle
{
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MeshGeometry* g_pxSphereGeometry;
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

// Peg layout: array of 2D positions for each peg
struct PinballPegLayout
{
	uint32_t uPegCount;
	float afX[s_uPB_MaxPegs];
	float afY[s_uPB_MaxPegs];
};

// ============================================================================
// Pinball State Enum
// ============================================================================
enum PinballState : uint8_t
{
	PINBALL_STATE_READY,
	PINBALL_STATE_LAUNCHING,
	PINBALL_STATE_PLAYING,
	PINBALL_STATE_BALL_LOST
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
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Pinball_Behaviour)

	Pinball_Behaviour() = delete;
	Pinball_Behaviour(Zenith_Entity& /*xParentEntity*/)
		: m_eState(PINBALL_STATE_READY)
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
	{
		m_xSaveData.Reset();
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
		m_pxCubeGeometry = TilePuzzle::g_pxCubeGeometry;
		m_pxSphereGeometry = TilePuzzle::g_pxSphereGeometry;

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
		}

		// Create the dynamic pinball scene
		m_xPinballScene = Zenith_SceneManager::CreateEmptyScene("PinballPlay");
		Zenith_SceneManager::SetActiveScene(m_xPinballScene);

		CreatePlayfield();
		SpawnBall();

		m_eState = PINBALL_STATE_READY;

		UpdateUI();
	}

	void OnStart() ZENITH_FINAL override
	{
	}

	void OnUpdate(const float fDeltaTime) ZENITH_FINAL override
	{
		// Handle escape to return to menu
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
		{
			ReturnToMenu();
			return;
		}

		// Decrement cooldowns
		if (m_fTargetCooldown > 0.f)
			m_fTargetCooldown -= fDeltaTime;

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
			RespawnBall();
			m_eState = PINBALL_STATE_READY;
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

		const char* aszStateNames[] = { "Ready", "Launching", "Playing", "Ball Lost" };
		ImGui::Text("State: %s", aszStateNames[m_eState]);

		if (ImGui::Button("Respawn Ball"))
		{
			RespawnBall();
			m_eState = PINBALL_STATE_READY;
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

	// ========================================================================
	// Scene Transitions
	// ========================================================================

	void ReturnToMenu()
	{
		// Update total score and save
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Cleanup dynamic scene
		if (m_xPinballScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xPinballScene);
			m_xPinballScene = Zenith_Scene();
		}

		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	// ========================================================================
	// Material Creation
	// ========================================================================

	void CreateMaterials()
	{
		auto& xRegistry = Zenith_AssetRegistry::Get();
		Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

		// Ball - bright white
		m_xBallMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
		m_xBallMaterial.Get()->SetName("PinballBall");
		m_xBallMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
		m_xBallMaterial.Get()->SetBaseColor({ 0.9f, 0.9f, 0.95f, 1.f });
		m_xBallMaterial.Get()->SetEmissiveColor(Zenith_Maths::Vector3(0.3f, 0.3f, 0.4f));
		m_xBallMaterial.Get()->SetEmissiveIntensity(0.3f);

		// Walls - dark blue
		m_xWallMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
		m_xWallMaterial.Get()->SetName("PinballWall");
		m_xWallMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
		m_xWallMaterial.Get()->SetBaseColor({ 0.15f, 0.18f, 0.3f, 1.f });

		// Pegs - teal
		m_xObstacleMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
		m_xObstacleMaterial.Get()->SetName("PinballObstacle");
		m_xObstacleMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
		m_xObstacleMaterial.Get()->SetBaseColor({ 0.1f, 0.4f, 0.5f, 1.f });

		// Plunger - red
		m_xPlungerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
		m_xPlungerMaterial.Get()->SetName("PinballPlunger");
		m_xPlungerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
		m_xPlungerMaterial.Get()->SetBaseColor({ 0.8f, 0.15f, 0.15f, 1.f });

		// Target - bright green
		m_xTargetMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
		m_xTargetMaterial.Get()->SetName("PinballTarget");
		m_xTargetMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
		m_xTargetMaterial.Get()->SetBaseColor({ 0.1f, 0.8f, 0.2f, 1.f });
		m_xTargetMaterial.Get()->SetEmissiveColor(Zenith_Maths::Vector3(0.1f, 0.8f, 0.2f));
		m_xTargetMaterial.Get()->SetEmissiveIntensity(0.5f);

		// Floor - very dark
		m_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
		m_xFloorMaterial.Get()->SetName("PinballFloor");
		m_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
		m_xFloorMaterial.Get()->SetBaseColor({ 0.06f, 0.06f, 0.1f, 1.f });
	}

	// ========================================================================
	// Peg Layout Loading & Spawning
	// ========================================================================

	void LoadPegLayouts()
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(GAME_ASSETS_DIR "Pinball/PegLayouts.bin");
		if (!xStream.IsValid())
		{
			Zenith_Warning(LOG_CATEGORY_GENERAL, "Failed to load pinball peg layouts");
			m_uLayoutCount = 0;
			return;
		}

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
		xModel.AddMeshEntry(*m_pxCubeGeometry, *xMaterial.Get());

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
		xTransform.SetScale(Zenith_Maths::Vector3(fScale));

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxSphereGeometry, *xMaterial.Get());

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
		AddWall(pxScene, "PB_WallLeft",
			{ s_fPB_FieldLeft - s_fPB_WallThickness * 0.5f, fCenterY, 0.f },
			{ s_fPB_WallThickness, fFieldH + s_fPB_WallThickness, 0.5f });

		// Right wall
		AddWall(pxScene, "PB_WallRight",
			{ s_fPB_FieldRight + s_fPB_WallThickness * 0.5f, fCenterY, 0.f },
			{ s_fPB_WallThickness, fFieldH + s_fPB_WallThickness, 0.5f });

		// Top wall
		AddWall(pxScene, "PB_WallTop",
			{ fCenterX, s_fPB_FieldTop + s_fPB_WallThickness * 0.5f, 0.f },
			{ fFieldW + s_fPB_WallThickness * 2.f, s_fPB_WallThickness, 0.5f });

		// Bottom wall left (gap in center for ball exit)
		float fGapHalfWidth = 0.6f;
		float fBottomWallLeftW = (fFieldW * 0.5f - fGapHalfWidth);
		AddWall(pxScene, "PB_WallBotL",
			{ s_fPB_FieldLeft + fBottomWallLeftW * 0.5f, s_fPB_FieldBottom - s_fPB_WallThickness * 0.5f, 0.f },
			{ fBottomWallLeftW, s_fPB_WallThickness, 0.5f });

		// Bottom wall right
		AddWall(pxScene, "PB_WallBotR",
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
			xModel.AddMeshEntry(*m_pxCubeGeometry, *m_xWallMaterial.Get());

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
			xModel.AddMeshEntry(*m_pxCubeGeometry, *m_xWallMaterial.Get());

			Zenith_ColliderComponent& xCollider = xCurve2.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
			Zenith_Physics::SetFriction(xCollider.GetBodyID(), 0.f);
		}

		// === Pegs (loaded from pre-generated layouts) ===
		LoadPegLayouts();
		m_uCurrentLayout = 0;
		CreatePegs(m_uCurrentLayout);

		// === Score Target (bottom center) ===
		{
			Zenith_Entity xTarget = CreateStaticBox(pxScene, "PB_Target",
				{ 0.f, 0.8f, 0.f },
				{ 1.0f, 0.3f, 0.5f },
				m_xTargetMaterial);
			m_xTargetEntityID = xTarget.GetEntityID();
		}

		// === Plunger (visual only, no collider) ===
		{
			Zenith_Entity xPlunger = CreateStaticBox(pxScene, "PB_Plunger",
				{ (s_fPB_ChannelLeft + s_fPB_ChannelRight) * 0.5f, s_fPB_PlungerRestY, 0.f },
				{ 0.5f, 0.4f, 0.3f },
				m_xPlungerMaterial, false);
			m_xPlungerEntityID = xPlunger.GetEntityID();
		}

		// Back wall to constrain Z movement (front wall omitted so camera can see the playfield)
		AddWall(pxScene, "PB_WallBack",
			{ fCenterX, fCenterY, 0.35f },
			{ fFieldW + s_fPB_WallThickness * 2.f, fFieldH + s_fPB_WallThickness * 2.f, 0.1f });
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
		xModel.AddMeshEntry(*m_pxSphereGeometry, *m_xBallMaterial.Get());

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
		xScript.SetBehaviour<Pinball_BallBehaviour>();

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

		// Cycle to next peg layout
		if (m_uLayoutCount > 0)
		{
			DestroyPegs();
			m_uCurrentLayout = (m_uCurrentLayout + 1) % m_uLayoutCount;
			CreatePegs(m_uCurrentLayout);
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

		Pinball_BallBehaviour* pxBallBehaviour = xBall.GetComponent<Zenith_ScriptComponent>().GetBehaviour<Pinball_BallBehaviour>();
		if (!pxBallBehaviour)
			return;

		// Check target via collision callback
		if (m_fTargetCooldown <= 0.f && pxBallBehaviour->DidCollideWith(m_xTargetEntityID))
		{
			m_uSessionScore += s_uPB_TargetScore;
			m_xSaveData.uPinballScore += s_uPB_TargetScore;
			m_fTargetCooldown = s_fPB_TargetCooldown;

			Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
				TilePuzzle_WriteSaveData, &m_xSaveData);
		}

		pxBallBehaviour->ClearCollisions();
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

		char szBuffer[64];

		snprintf(szBuffer, sizeof(szBuffer), "Score: %u", m_uSessionScore);
		Zenith_UI::Zenith_UIText* pxScore = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballScore");
		if (pxScore) pxScore->SetText(szBuffer);

		snprintf(szBuffer, sizeof(szBuffer), "Total: %u", m_xSaveData.uPinballScore);
		Zenith_UI::Zenith_UIText* pxHighScore = xUI.FindElement<Zenith_UI::Zenith_UIText>("PinballHighScore");
		if (pxHighScore) pxHighScore->SetText(szBuffer);
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Game state
	PinballState m_eState;
	uint32_t m_uSessionScore;
	TilePuzzleSaveData m_xSaveData;

	// Entity IDs (dynamic scene)
	Zenith_EntityID m_xBallEntityID;
	Zenith_EntityID m_xPlungerEntityID;
	Zenith_EntityID m_xTargetEntityID;
	Zenith_EntityID m_axWallEntityIDs[s_uPB_MaxWalls];
	uint32_t m_uWallCount;
	Zenith_EntityID m_axPegEntityIDs[s_uPB_MaxPegs];

	// Peg layouts (loaded from disk)
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
};
