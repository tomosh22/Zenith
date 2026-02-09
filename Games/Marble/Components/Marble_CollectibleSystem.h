#pragma once
/**
 * Marble_CollectibleSystem.h - Pickup detection and scoring
 *
 * Demonstrates:
 * - Distance-based collision detection (no physics callbacks)
 * - Entity destruction on collection
 * - Score and win condition tracking
 *
 * This is a simple approach suitable for:
 * - Non-physics pickups (floating collectibles)
 * - Low collectible counts
 * - When you don't need physics callback overhead
 *
 * For physics-based pickups, use Zenith_ColliderComponent's
 * collision callbacks (OnCollisionEnter, etc.)
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include <vector>

// Pickup configuration
static constexpr float s_fCollectiblePickupRadius = 0.3f;
static constexpr float s_fBallPickupRadius = 0.5f;
static constexpr float s_fPickupMargin = 0.2f;
static constexpr uint32_t s_uCollectibleScore = 100;

/**
 * Marble_CollectibleSystem - Distance-based pickup system
 */
class Marble_CollectibleSystem
{
public:
	/**
	 * CollectionResult - Result of checking collectibles
	 */
	struct CollectionResult
	{
		uint32_t uCollectedCount = 0;    // Number collected this frame
		uint32_t uScoreGained = 0;       // Score from collections
		bool bAllCollected = false;      // Win condition
	};

	/**
	 * CheckCollectibles - Check for pickups and collect any in range
	 *
	 * Iterates through all collectibles and checks distance to ball.
	 * Collected entities are destroyed and removed from the list.
	 *
	 * @param xBallPos               Ball position
	 * @param axCollectibleEntityIDs Vector of collectible IDs (modified on collection)
	 * @param uTotalCollected        Running total of collected items
	 * @return Collection result for this frame
	 */
	static CollectionResult CheckCollectibles(
		const Zenith_Maths::Vector3& xBallPos,
		std::vector<Zenith_EntityID>& axCollectibleEntityIDs,
		uint32_t uTotalCollected)
	{
		CollectionResult xResult;
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		float fPickupDist = s_fBallPickupRadius + s_fCollectiblePickupRadius + s_fPickupMargin;

		// Iterate backwards so we can safely remove during iteration
		for (int i = static_cast<int>(axCollectibleEntityIDs.size()) - 1; i >= 0; i--)
		{
			Zenith_EntityID uCollID = axCollectibleEntityIDs[i];
			if (!pxSceneData->EntityExists(uCollID))
			{
				// Clean up stale ID
				axCollectibleEntityIDs.erase(axCollectibleEntityIDs.begin() + i);
				continue;
			}

			Zenith_Entity xColl = pxSceneData->GetEntity(uCollID);
			Zenith_Maths::Vector3 xCollPos;
			xColl.GetComponent<Zenith_TransformComponent>().GetPosition(xCollPos);

			float fDist = glm::length(xBallPos - xCollPos);
			if (fDist < fPickupDist)
			{
				// Collected!
				Zenith_SceneManager::Destroy(xColl);
				axCollectibleEntityIDs.erase(axCollectibleEntityIDs.begin() + i);

				xResult.uCollectedCount++;
				xResult.uScoreGained += s_uCollectibleScore;
			}
		}

		// Check win condition (all collectibles collected)
		xResult.bAllCollected = axCollectibleEntityIDs.empty() && (uTotalCollected + xResult.uCollectedCount) > 0;

		return xResult;
	}

	/**
	 * UpdateCollectibleRotation - Animate collectibles (rotating)
	 *
	 * Makes collectibles visually interesting by spinning them.
	 */
	static void UpdateCollectibleRotation(
		const std::vector<Zenith_EntityID>& axCollectibleEntityIDs,
		float fDt)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		for (Zenith_EntityID uID : axCollectibleEntityIDs)
		{
			if (!pxSceneData->EntityExists(uID))
				continue;

			Zenith_Entity xColl = pxSceneData->GetEntity(uID);
			Zenith_TransformComponent& xTransform = xColl.GetComponent<Zenith_TransformComponent>();

			// Get current rotation, add Y rotation, set back
			Zenith_Maths::Quat xRot;
			xTransform.GetRotation(xRot);
			Zenith_Maths::Vector3 xEuler = glm::eulerAngles(xRot);
			xEuler.y += fDt * 2.0f; // Rotate 2 radians per second
			xTransform.SetRotation(Zenith_Maths::Quat(xEuler));
		}
	}
};
