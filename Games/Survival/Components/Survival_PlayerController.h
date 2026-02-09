#pragma once
/**
 * Survival_PlayerController.h - Player movement and interaction
 *
 * Demonstrates:
 * - Third-person character movement
 * - Interaction with nearby objects
 * - Camera-relative input handling
 * - Interaction range checking
 *
 * Movement: WASD/Arrows for movement
 * Interaction: E key to interact with nearby resources
 */

#include "Input/Zenith_Input.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Maths/Zenith_Maths.h"
#include "Survival_EventBus.h"

/**
 * Survival_PlayerController - Handles player input and movement
 */
class Survival_PlayerController
{
public:
	/**
	 * GetMovementDirection - Get normalized movement direction from input
	 * Uses camera-relative movement (WASD moves relative to camera facing)
	 */
	static Zenith_Maths::Vector3 GetMovementDirection(
		const Zenith_Maths::Vector3& xCamPos,
		const Zenith_Maths::Vector3& xPlayerPos)
	{
		// Calculate forward direction from camera to player (projected to XZ)
		Zenith_Maths::Vector3 xToPlayer = xPlayerPos - xCamPos;
		xToPlayer.y = 0.0f;

		if (glm::length(xToPlayer) > 0.001f)
		{
			xToPlayer = glm::normalize(xToPlayer);
		}
		else
		{
			xToPlayer = Zenith_Maths::Vector3(0.f, 0.f, 1.f);
		}

		// Forward is toward player (camera facing), right is perpendicular
		Zenith_Maths::Vector3 xForward = xToPlayer;
		Zenith_Maths::Vector3 xRight = glm::cross(Zenith_Maths::Vector3(0.f, 1.f, 0.f), xForward);

		Zenith_Maths::Vector3 xDirection(0.f);

		// Forward/backward
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W) || Zenith_Input::IsKeyHeld(ZENITH_KEY_UP))
		{
			xDirection += xForward;
		}
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S) || Zenith_Input::IsKeyHeld(ZENITH_KEY_DOWN))
		{
			xDirection -= xForward;
		}

		// Left/right strafe
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A) || Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT))
		{
			xDirection -= xRight;
		}
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D) || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT))
		{
			xDirection += xRight;
		}

		if (glm::length(xDirection) > 0.001f)
		{
			xDirection = glm::normalize(xDirection);
		}

		return xDirection;
	}

	/**
	 * ApplyMovement - Move the player entity
	 */
	static void ApplyMovement(
		Zenith_EntityID uPlayerEntityID,
		const Zenith_Maths::Vector3& xDirection,
		float fSpeed,
		float fDt)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uPlayerEntityID))
			return;

		Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerEntityID);
		if (!xPlayer.HasComponent<Zenith_TransformComponent>())
			return;

		Zenith_TransformComponent& xTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xPos;
		xTransform.GetPosition(xPos);

		// Apply movement
		xPos += xDirection * fSpeed * fDt;

		// Keep on ground (Y = 0)
		xPos.y = 0.8f;  // Half height of capsule

		xTransform.SetPosition(xPos);

		// Rotate to face movement direction
		if (glm::length(xDirection) > 0.001f)
		{
			float fAngle = atan2(-xDirection.x, -xDirection.z);
			Zenith_Maths::Quat xRot = glm::angleAxis(fAngle, Zenith_Maths::Vector3(0.f, 1.f, 0.f));
			xTransform.SetRotation(xRot);
		}
	}

	/**
	 * WasInteractPressed - Check if interaction key was pressed
	 */
	static bool WasInteractPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_E);
	}

	/**
	 * WasCraftingKeyPressed - Check if crafting menu key was pressed
	 */
	static bool WasCraftingKeyPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_C);
	}

	/**
	 * WasInventoryKeyPressed - Check if inventory key was pressed
	 */
	static bool WasInventoryKeyPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_TAB);
	}

	/**
	 * WasResetPressed - Check for reset key
	 */
	static bool WasResetPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R);
	}

	/**
	 * WasCraftAxePressed - Number key for crafting axe
	 */
	static bool WasCraftAxePressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_1);
	}

	/**
	 * WasCraftPickaxePressed - Number key for crafting pickaxe
	 */
	static bool WasCraftPickaxePressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_2);
	}

	/**
	 * GetPlayerPosition - Get current player world position
	 */
	static Zenith_Maths::Vector3 GetPlayerPosition(Zenith_EntityID uPlayerEntityID)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uPlayerEntityID))
			return Zenith_Maths::Vector3(0.f);

		Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerEntityID);
		if (!xPlayer.HasComponent<Zenith_TransformComponent>())
			return Zenith_Maths::Vector3(0.f);

		Zenith_Maths::Vector3 xPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	/**
	 * DistanceToEntity - Calculate distance between player and another entity
	 */
	static float DistanceToEntity(
		Zenith_EntityID uPlayerEntityID,
		Zenith_EntityID uTargetEntityID)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uPlayerEntityID) || !pxSceneData->EntityExists(uTargetEntityID))
			return FLT_MAX;

		Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerEntityID);
		Zenith_Entity xTarget = pxSceneData->GetEntity(uTargetEntityID);

		if (!xPlayer.HasComponent<Zenith_TransformComponent>() ||
			!xTarget.HasComponent<Zenith_TransformComponent>())
			return FLT_MAX;

		Zenith_Maths::Vector3 xPlayerPos, xTargetPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);
		xTarget.GetComponent<Zenith_TransformComponent>().GetPosition(xTargetPos);

		return glm::distance(xPlayerPos, xTargetPos);
	}

	/**
	 * IsInRange - Check if player is within interaction range of target
	 */
	static bool IsInRange(
		Zenith_EntityID uPlayerEntityID,
		Zenith_EntityID uTargetEntityID,
		float fRange)
	{
		return DistanceToEntity(uPlayerEntityID, uTargetEntityID) <= fRange;
	}
};

/**
 * Survival_CameraController - Third-person camera follow
 */
class Survival_CameraController
{
public:
	/**
	 * UpdateCamera - Smoothly follow the player
	 */
	static void UpdateCamera(
		Zenith_EntityID uPlayerEntityID,
		float fDistance,
		float fHeight,
		float fSmoothSpeed,
		float fDt)
	{
		// Get player from active scene (world scene)
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uPlayerEntityID))
			return;

		Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerEntityID);
		if (!xPlayer.HasComponent<Zenith_TransformComponent>())
			return;

		// Get camera from persistent scene
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCamera)
			return;

		Zenith_Maths::Vector3 xPlayerPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);

		// Target camera position: behind and above player
		Zenith_Maths::Vector3 xTargetPos = xPlayerPos + Zenith_Maths::Vector3(0.f, fHeight, -fDistance);

		// Get current camera position
		Zenith_Maths::Vector3 xCurrentPos;
		pxCamera->GetPosition(xCurrentPos);

		// Smooth interpolation
		Zenith_Maths::Vector3 xNewPos = glm::mix(xCurrentPos, xTargetPos, fSmoothSpeed * fDt);
		pxCamera->SetPosition(xNewPos);

		// Look at player
		Zenith_Maths::Vector3 xLookDir = xPlayerPos - xNewPos;
		if (glm::length(xLookDir) > 0.001f)
		{
			xLookDir = glm::normalize(xLookDir);

			// Calculate pitch (looking down)
			float fPitch = -asin(xLookDir.y);

			// Calculate yaw
			float fYaw = atan2(xLookDir.x, xLookDir.z);

			pxCamera->SetPitch(fPitch);
			pxCamera->SetYaw(fYaw);
		}
	}

	/**
	 * GetCameraPosition - Get current camera position
	 */
	static Zenith_Maths::Vector3 GetCameraPosition()
	{
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCamera)
			return Zenith_Maths::Vector3(0.f);

		Zenith_Maths::Vector3 xPos;
		pxCamera->GetPosition(xPos);
		return xPos;
	}
};
