#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Input/Zenith_Input.h"
#include "Physics/Zenith_Physics.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandler.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// Marble Resources - Global access
// Defined in Marble.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================
class Zenith_Prefab;

namespace Marble
{
	extern Flux_MeshGeometry* g_pxSphereGeometry;
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MaterialAsset* g_pxBallMaterial;
	extern Flux_MaterialAsset* g_pxPlatformMaterial;
	extern Flux_MaterialAsset* g_pxGoalMaterial;
	extern Flux_MaterialAsset* g_pxCollectibleMaterial;
	extern Flux_MaterialAsset* g_pxFloorMaterial;

	// Prefabs for runtime instantiation
	extern Zenith_Prefab* g_pxBallPrefab;
	extern Zenith_Prefab* g_pxPlatformPrefab;
	extern Zenith_Prefab* g_pxGoalPrefab;
	extern Zenith_Prefab* g_pxCollectiblePrefab;
}

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================
static constexpr float s_fBallRadius = 0.5f;
static constexpr float s_fMoveSpeed = 0.5f;  // Velocity increment per frame (impulse-based)
static constexpr float s_fJumpImpulse = 8.0f;
static constexpr float s_fCameraDistance = 8.0f;
static constexpr float s_fCameraHeight = 5.0f;
static constexpr float s_fCameraSmoothSpeed = 5.0f;
static constexpr uint32_t s_uPlatformCount = 8;
static constexpr uint32_t s_uCollectibleCount = 5;
static constexpr float s_fCollectibleRadius = 0.3f;
// ============================================================================

enum class MarbleGameState
{
	PLAYING,
	PAUSED,
	WON,
	LOST
};

class Marble_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Marble_Behaviour)

	Marble_Behaviour() = delete;
	Marble_Behaviour(Zenith_Entity& xParentEntity)
		: m_eGameState(MarbleGameState::PLAYING)
		, m_uScore(0)
		, m_fTimeRemaining(60.0f)
		, m_uCollectedCount(0)
		, m_uBallEntityID(0)
		, m_xRng(std::random_device{}())
	{
	}
	~Marble_Behaviour() = default;

	/**
	 * OnAwake - Lifecycle hook.
	 * Called when behavior is attached at RUNTIME (not during scene loading).
	 * Use for: Getting asset references, procedural generation.
	 */
	void OnAwake() ZENITH_FINAL override
	{
		// Always use properly-initialized global resources
		// Deserialized materials from backup restore may not have GPU resources (textures) loaded
		m_pxSphereGeometry = Marble::g_pxSphereGeometry;
		m_pxCubeGeometry = Marble::g_pxCubeGeometry;
		m_pxBallMaterial = Marble::g_pxBallMaterial;
		m_pxPlatformMaterial = Marble::g_pxPlatformMaterial;
		m_pxGoalMaterial = Marble::g_pxGoalMaterial;
		m_pxCollectibleMaterial = Marble::g_pxCollectibleMaterial;
		m_pxFloorMaterial = Marble::g_pxFloorMaterial;

		GenerateLevel();
	}

	/**
	 * OnStart - Lifecycle hook.
	 * Called before first update, for ALL entities (including loaded ones).
	 * Can be used for initialization that depends on other components being ready.
	 */
	void OnStart() ZENITH_FINAL override
	{
		if (m_uBallEntityID == 0)
		{
			GenerateLevel();
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		if (m_eGameState == MarbleGameState::PAUSED)
		{
			HandlePauseInput();
			return;
		}

		if (m_eGameState == MarbleGameState::PLAYING)
		{
			// Update timer
			m_fTimeRemaining -= fDt;
			if (m_fTimeRemaining <= 0.0f)
			{
				m_fTimeRemaining = 0.0f;
				m_eGameState = MarbleGameState::LOST;
			}

			HandleInput(fDt);
			CheckCollectibles();
			UpdateCollectibleRotation(fDt);
			UpdateUI();
		}

		// Pause toggle
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
		{
			TogglePause();
		}

		// Reset level
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
		{
			ResetLevel();
		}

		// Update camera follow (done in OnUpdate for immediate response)
		UpdateCameraFollow(fDt);
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Marble Ball Game");
		ImGui::Separator();
		ImGui::Text("Score: %u", m_uScore);
		ImGui::Text("Time: %.1f", m_fTimeRemaining);
		ImGui::Text("Collected: %u / %u", m_uCollectedCount, static_cast<uint32_t>(m_axCollectibleEntityIDs.size()));

		const char* szStates[] = { "PLAYING", "PAUSED", "WON", "LOST" };
		ImGui::Text("State: %s", szStates[static_cast<int>(m_eGameState)]);

		if (ImGui::Button("Reset Level"))
		{
			ResetLevel();
		}
		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD: Move ball");
		ImGui::Text("  Space: Jump");
		ImGui::Text("  P/Esc: Pause");
		ImGui::Text("  R: Reset");
#endif
	}

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_fTimeRemaining;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_fTimeRemaining;
		}
	}

private:
	// ========================================================================
	// Input Handling
	// ========================================================================
	void HandleInput(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (m_uBallEntityID == 0 || !xScene.EntityExists(m_uBallEntityID))
			return;

		Zenith_Entity xBall = xScene.GetEntityByID(m_uBallEntityID);
		if (!xBall.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody())
			return;

		const JPH::BodyID& xBodyID = xCollider.GetBodyID();

		// Get camera for input direction
		Zenith_EntityID uCamID = xScene.GetMainCameraEntity();
		if (uCamID == INVALID_ENTITY_ID || !xScene.EntityExists(uCamID))
			return;

		Zenith_Entity xCamEntity = xScene.GetEntityByID(uCamID);
		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		// Get camera forward/right (projected onto XZ plane)
		Zenith_Maths::Vector3 xCamPos;
		xCamera.GetPosition(xCamPos);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);
		Zenith_Maths::Vector3 xToBall = xBallPos - xCamPos;
		xToBall.y = 0.0f;
		if (glm::length(xToBall) > 0.001f)
			xToBall = glm::normalize(xToBall);
		else
			xToBall = Zenith_Maths::Vector3(0.f, 0.f, 1.f);

		Zenith_Maths::Vector3 xForward = xToBall;
		Zenith_Maths::Vector3 xRight = glm::cross(Zenith_Maths::Vector3(0.f, 1.f, 0.f), xForward);

		// Calculate input force
		Zenith_Maths::Vector3 xForce(0.f);

		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W) || Zenith_Input::IsKeyHeld(ZENITH_KEY_UP))
			xForce += xForward;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S) || Zenith_Input::IsKeyHeld(ZENITH_KEY_DOWN))
			xForce -= xForward;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A) || Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT))
			xForce -= xRight;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D) || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT))
			xForce += xRight;

		if (glm::length(xForce) > 0.0f)
		{
			xForce = glm::normalize(xForce) * s_fMoveSpeed;
			Zenith_Physics::AddImpulse(xBodyID, xForce);
		}

		// Jump (allow if not already moving upward significantly - prevents double jump)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE))
		{
			Zenith_Maths::Vector3 xVel = Zenith_Physics::GetLinearVelocity(xBodyID);
			// Only jump if not already moving upward (prevents air jumps)
			if (xVel.y < 1.0f)
			{
				Zenith_Physics::AddImpulse(xBodyID, Zenith_Maths::Vector3(0.f, s_fJumpImpulse, 0.f));
			}
		}

		// Check if ball fell off
		if (xBallPos.y < -10.0f)
		{
			m_eGameState = MarbleGameState::LOST;
		}
	}

	void HandlePauseInput()
	{
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
		{
			TogglePause();
		}
	}

	void TogglePause()
	{
		if (m_eGameState == MarbleGameState::PLAYING)
		{
			m_eGameState = MarbleGameState::PAUSED;
		}
		else if (m_eGameState == MarbleGameState::PAUSED)
		{
			m_eGameState = MarbleGameState::PLAYING;
		}
	}

	// ========================================================================
	// Camera Follow
	// ========================================================================
	void UpdateCameraFollow(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (m_uBallEntityID == 0 || !xScene.EntityExists(m_uBallEntityID))
			return;

		Zenith_EntityID uCamID = xScene.GetMainCameraEntity();
		if (uCamID == INVALID_ENTITY_ID || !xScene.EntityExists(uCamID))
			return;

		Zenith_Entity xBall = xScene.GetEntityByID(m_uBallEntityID);
		Zenith_Entity xCamEntity = xScene.GetEntityByID(uCamID);

		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);
		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		// Target position: behind and above the ball
		Zenith_Maths::Vector3 xTargetCamPos = xBallPos + Zenith_Maths::Vector3(0.f, s_fCameraHeight, -s_fCameraDistance);

		// Smooth follow
		Zenith_Maths::Vector3 xCurrentPos;
		xCamera.GetPosition(xCurrentPos);
		Zenith_Maths::Vector3 xNewPos = glm::mix(xCurrentPos, xTargetCamPos, fDt * s_fCameraSmoothSpeed);
		xCamera.SetPosition(xNewPos);

		// Look at ball
		Zenith_Maths::Vector3 xDir = xBallPos - xNewPos;
		if (glm::length(xDir) > 0.001f)
		{
			xDir = glm::normalize(xDir);
			float fPitch = asin(xDir.y);  // Negative xDir.y (ball below) -> negative pitch -> look down
			float fYaw = atan2(xDir.x, xDir.z);
			xCamera.SetPitch(fPitch);
			xCamera.SetYaw(fYaw);
		}
	}

	// ========================================================================
	// Collectible System
	// ========================================================================
	void CheckCollectibles()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (m_uBallEntityID == 0 || !xScene.EntityExists(m_uBallEntityID))
			return;

		Zenith_Entity xBall = xScene.GetEntityByID(m_uBallEntityID);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		for (size_t i = 0; i < m_axCollectibleEntityIDs.size(); i++)
		{
			Zenith_EntityID uCollID = m_axCollectibleEntityIDs[i];
			if (!xScene.EntityExists(uCollID))
				continue;

			Zenith_Entity xColl = xScene.GetEntityByID(uCollID);
			Zenith_Maths::Vector3 xCollPos;
			xColl.GetComponent<Zenith_TransformComponent>().GetPosition(xCollPos);

			float fDist = glm::length(xBallPos - xCollPos);
			if (fDist < s_fBallRadius + s_fCollectibleRadius + 0.2f)
			{
				// Collected!
				Zenith_Scene::Destroy(uCollID);
				m_axCollectibleEntityIDs.erase(m_axCollectibleEntityIDs.begin() + i);
				i--;

				m_uCollectedCount++;
				m_uScore += 100;

				// Check win condition
				if (m_axCollectibleEntityIDs.empty())
				{
					m_eGameState = MarbleGameState::WON;
				}
			}
		}
	}

	void UpdateCollectibleRotation(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		for (Zenith_EntityID uID : m_axCollectibleEntityIDs)
		{
			if (!xScene.EntityExists(uID))
				continue;

			Zenith_Entity xColl = xScene.GetEntityByID(uID);
			Zenith_TransformComponent& xTransform = xColl.GetComponent<Zenith_TransformComponent>();

			// Get current rotation as quaternion, convert to euler, add Y rotation, convert back
			Zenith_Maths::Quat xRot;
			xTransform.GetRotation(xRot);
			Zenith_Maths::Vector3 xEuler = glm::eulerAngles(xRot);
			xEuler.y += fDt * 2.0f;  // Rotate around Y axis
			xTransform.SetRotation(Zenith_Maths::Quat(xEuler));
		}
	}

	// ========================================================================
	// Level Generation
	// ========================================================================
	void GenerateLevel()
	{
		DestroyLevel();

		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Create starting platform (large)
		CreatePlatform(Zenith_Maths::Vector3(0.f, 0.f, 0.f), Zenith_Maths::Vector3(5.f, 0.5f, 5.f));

		// Create ball on starting platform
		CreateBall(Zenith_Maths::Vector3(0.f, s_fBallRadius + 0.5f, 0.f));

		// Generate random platforms
		std::uniform_real_distribution<float> xSizeDist(2.0f, 5.0f);
		std::uniform_real_distribution<float> xHeightDist(-1.0f, 2.0f);
		std::uniform_real_distribution<float> xAngleDist(0.f, 6.28f);

		float fRadius = 8.0f;
		for (uint32_t i = 0; i < s_uPlatformCount; i++)
		{
			float fAngle = xAngleDist(m_xRng);
			float fX = cos(fAngle) * fRadius;
			float fZ = sin(fAngle) * fRadius;
			float fY = xHeightDist(m_xRng);

			float fSizeX = xSizeDist(m_xRng);
			float fSizeZ = xSizeDist(m_xRng);

			CreatePlatform(
				Zenith_Maths::Vector3(fX, fY, fZ),
				Zenith_Maths::Vector3(fSizeX, 0.5f, fSizeZ)
			);

			fRadius += 5.0f;
		}

		// Create goal platform at end
		float fGoalAngle = xAngleDist(m_xRng);
		float fGoalX = cos(fGoalAngle) * (fRadius + 5.0f);
		float fGoalZ = sin(fGoalAngle) * (fRadius + 5.0f);
		CreateGoalPlatform(Zenith_Maths::Vector3(fGoalX, 1.0f, fGoalZ));

		// Scatter collectibles on platforms
		CreateCollectibles();

		// Reset game state
		m_eGameState = MarbleGameState::PLAYING;
		m_uScore = 0;
		m_fTimeRemaining = 60.0f;
		m_uCollectedCount = 0;
	}

	void CreatePlatform(const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xScale)
	{
		// Prefab-based Instantiate
		Zenith_Entity xPlatform = Zenith_Scene::Instantiate(*Marble::g_pxPlatformPrefab, "Platform");
		Zenith_TransformComponent& xTransform = xPlatform.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(xScale);

		// Add ModelComponent after setting transform (mesh pointers can't be serialized in prefabs)
		Zenith_ModelComponent& xModel = xPlatform.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxCubeGeometry, *m_pxPlatformMaterial);

		// Add ColliderComponent AFTER setting position/scale (physics body uses transform)
		xPlatform.AddComponent<Zenith_ColliderComponent>().AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		m_axPlatformEntityIDs.push_back(xPlatform.GetEntityID());
	}

	void CreateGoalPlatform(const Zenith_Maths::Vector3& xPos)
	{
		// Prefab-based Instantiate
		Zenith_Entity xGoal = Zenith_Scene::Instantiate(*Marble::g_pxGoalPrefab, "Goal");
		Zenith_TransformComponent& xTransform = xGoal.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(Zenith_Maths::Vector3(4.f, 0.3f, 4.f));

		// Add ModelComponent after setting transform (mesh pointers can't be serialized in prefabs)
		Zenith_ModelComponent& xModel = xGoal.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxCubeGeometry, *m_pxGoalMaterial);

		// Add ColliderComponent AFTER setting position/scale (physics body uses transform)
		xGoal.AddComponent<Zenith_ColliderComponent>().AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		m_uGoalEntityID = xGoal.GetEntityID();
	}

	void CreateBall(const Zenith_Maths::Vector3& xPos)
	{
		// Prefab-based Instantiate
		Zenith_Entity xBall = Zenith_Scene::Instantiate(*Marble::g_pxBallPrefab, "Ball");
		Zenith_TransformComponent& xTransform = xBall.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(Zenith_Maths::Vector3(s_fBallRadius * 2.f));

		// Add ModelComponent after setting transform (mesh pointers can't be serialized in prefabs)
		Zenith_ModelComponent& xModel = xBall.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxSphereGeometry, *m_pxBallMaterial);

		// Add ColliderComponent AFTER setting position/scale (physics body uses transform)
		xBall.AddComponent<Zenith_ColliderComponent>().AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

		m_uBallEntityID = xBall.GetEntityID();
	}

	void CreateCollectibles()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Place collectibles above platforms
		std::uniform_int_distribution<size_t> xPlatformDist(0, m_axPlatformEntityIDs.size() - 1);

		for (uint32_t i = 0; i < s_uCollectibleCount && i < m_axPlatformEntityIDs.size(); i++)
		{
			size_t uPlatformIdx = i;  // One per platform initially
			if (i >= m_axPlatformEntityIDs.size())
				uPlatformIdx = xPlatformDist(m_xRng);

			Zenith_EntityID uPlatformID = m_axPlatformEntityIDs[uPlatformIdx];
			if (!xScene.EntityExists(uPlatformID))
				continue;

			Zenith_Entity xPlatform = xScene.GetEntityByID(uPlatformID);
			Zenith_Maths::Vector3 xPlatPos, xPlatScale;
			xPlatform.GetComponent<Zenith_TransformComponent>().GetPosition(xPlatPos);
			xPlatform.GetComponent<Zenith_TransformComponent>().GetScale(xPlatScale);

			// Place collectible above platform center
			Zenith_Maths::Vector3 xCollPos = xPlatPos + Zenith_Maths::Vector3(0.f, xPlatScale.y + 1.0f, 0.f);

			// Prefab-based Instantiate
			Zenith_Entity xCollectible = Zenith_Scene::Instantiate(*Marble::g_pxCollectiblePrefab, "Collectible");
			Zenith_TransformComponent& xTransform = xCollectible.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(xCollPos);
			xTransform.SetScale(Zenith_Maths::Vector3(s_fCollectibleRadius * 2.f));

			// Add ModelComponent after instantiation (mesh pointers can't be serialized in prefabs)
			Zenith_ModelComponent& xModel = xCollectible.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*m_pxSphereGeometry, *m_pxCollectibleMaterial);

			m_axCollectibleEntityIDs.push_back(xCollectible.GetEntityID());
		}
	}

	void DestroyLevel()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		if (m_uBallEntityID != 0 && xScene.EntityExists(m_uBallEntityID))
		{
			Zenith_Scene::Destroy(m_uBallEntityID);
			m_uBallEntityID = 0;
		}

		for (Zenith_EntityID uID : m_axPlatformEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		m_axPlatformEntityIDs.clear();

		for (Zenith_EntityID uID : m_axCollectibleEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		m_axCollectibleEntityIDs.clear();

		if (m_uGoalEntityID != 0 && xScene.EntityExists(m_uGoalEntityID))
		{
			Zenith_Scene::Destroy(m_uGoalEntityID);
			m_uGoalEntityID = 0;
		}
	}

	void ResetLevel()
	{
		GenerateLevel();
	}

	// ========================================================================
	// UI
	// ========================================================================
	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxScore = xUI.FindElement<Zenith_UI::Zenith_UIText>("Score");
		if (pxScore)
		{
			char acBuffer[64];
			snprintf(acBuffer, sizeof(acBuffer), "Score: %u", m_uScore);
			pxScore->SetText(acBuffer);
		}

		Zenith_UI::Zenith_UIText* pxTime = xUI.FindElement<Zenith_UI::Zenith_UIText>("Time");
		if (pxTime)
		{
			char acBuffer[64];
			snprintf(acBuffer, sizeof(acBuffer), "Time: %.1f", m_fTimeRemaining);
			pxTime->SetText(acBuffer);
		}

		Zenith_UI::Zenith_UIText* pxCollected = xUI.FindElement<Zenith_UI::Zenith_UIText>("Collected");
		if (pxCollected)
		{
			char acBuffer[64];
			snprintf(acBuffer, sizeof(acBuffer), "Collected: %u / %zu", m_uCollectedCount, m_axCollectibleEntityIDs.size() + m_uCollectedCount);
			pxCollected->SetText(acBuffer);
		}

		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			switch (m_eGameState)
			{
			case MarbleGameState::WON:
				pxStatus->SetText("YOU WIN!");
				pxStatus->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));
				break;
			case MarbleGameState::LOST:
				pxStatus->SetText("GAME OVER");
				pxStatus->SetColor(Zenith_Maths::Vector4(1.f, 0.2f, 0.2f, 1.f));
				break;
			case MarbleGameState::PAUSED:
				pxStatus->SetText("PAUSED");
				pxStatus->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 0.2f, 1.f));
				break;
			default:
				pxStatus->SetText("");
				break;
			}
		}
	}

	// ========================================================================
	// Member Variables
	// ========================================================================
	MarbleGameState m_eGameState;
	uint32_t m_uScore;
	float m_fTimeRemaining;
	uint32_t m_uCollectedCount;

	// Entity IDs
	Zenith_EntityID m_uBallEntityID;
	Zenith_EntityID m_uGoalEntityID = 0;
	std::vector<Zenith_EntityID> m_axPlatformEntityIDs;
	std::vector<Zenith_EntityID> m_axCollectibleEntityIDs;

	// Random number generator
	std::mt19937 m_xRng;

public:
	// Resource pointers (set by Marble.cpp, or by editor/serialization)
	Flux_MeshGeometry* m_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MaterialAsset* m_pxBallMaterial = nullptr;
	Flux_MaterialAsset* m_pxPlatformMaterial = nullptr;
	Flux_MaterialAsset* m_pxGoalMaterial = nullptr;
	Flux_MaterialAsset* m_pxCollectibleMaterial = nullptr;
	Flux_MaterialAsset* m_pxFloorMaterial = nullptr;
};
