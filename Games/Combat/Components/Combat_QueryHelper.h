#pragma once
/**
 * Combat_QueryHelper.h - Entity query utilities
 *
 * Demonstrates:
 * - Zenith_Query for multi-component queries
 * - Finding entities within radius
 * - Tag-based entity filtering
 * - Distance-based sorting
 *
 * Usage:
 *   // Find nearest enemy to player
 *   Zenith_EntityID uNearest = Combat_QueryHelper::FindNearestEnemy(xPlayerPos);
 *
 *   // Find all enemies in attack range
 *   auto axEnemies = Combat_QueryHelper::FindEnemiesInRange(xPlayerPos, 2.0f);
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Maths/Zenith_Maths.h"
#include <vector>
#include <algorithm>

// ============================================================================
// Entity Tags (stored via naming convention)
// ============================================================================

// Entity name prefixes for identification
static constexpr const char* s_szPlayerPrefix = "Player";
static constexpr const char* s_szEnemyPrefix = "Enemy";
static constexpr const char* s_szArenaPrefix = "Arena";

// ============================================================================
// Query Result Types
// ============================================================================

struct Combat_EntityDistance
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
	float m_fDistance = 0.0f;
	Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
};

// ============================================================================
// Combat Query Helper
// ============================================================================

class Combat_QueryHelper
{
public:
	// ========================================================================
	// Entity Type Identification
	// ========================================================================

	/**
	 * IsPlayer - Check if entity is the player
	 */
	static bool IsPlayer(Zenith_EntityID uEntityID)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uEntityID))
			return false;

		const std::string& strName = pxSceneData->GetEntity(uEntityID).GetName();
		return strName.find(s_szPlayerPrefix) == 0;
	}

	/**
	 * IsEnemy - Check if entity is an enemy
	 */
	static bool IsEnemy(Zenith_EntityID uEntityID)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uEntityID))
			return false;

		const std::string& strName = pxSceneData->GetEntity(uEntityID).GetName();
		return strName.find(s_szEnemyPrefix) == 0;
	}

	/**
	 * IsArena - Check if entity is part of the arena
	 */
	static bool IsArena(Zenith_EntityID uEntityID)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uEntityID))
			return false;

		const std::string& strName = pxSceneData->GetEntity(uEntityID).GetName();
		return strName.find(s_szArenaPrefix) == 0;
	}

	// ========================================================================
	// Find Player
	// ========================================================================

	/**
	 * FindPlayer - Get the player entity ID
	 */
	static Zenith_EntityID FindPlayer()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		Zenith_EntityID uPlayerID = INVALID_ENTITY_ID;

		pxSceneData->Query<Zenith_TransformComponent>()
			.ForEach([&](Zenith_EntityID uID, Zenith_TransformComponent&)
			{
				if (IsPlayer(uID))
				{
					uPlayerID = uID;
				}
			});

		return uPlayerID;
	}

	/**
	 * GetPlayerPosition - Get the player's current position
	 */
	static Zenith_Maths::Vector3 GetPlayerPosition()
	{
		Zenith_EntityID uPlayerID = FindPlayer();
		if (uPlayerID == INVALID_ENTITY_ID)
			return Zenith_Maths::Vector3(0.0f);

		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerID);

		Zenith_Maths::Vector3 xPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	// ========================================================================
	// Find Enemies
	// ========================================================================

	/**
	 * FindAllEnemies - Get all enemy entity IDs
	 */
	static std::vector<Zenith_EntityID> FindAllEnemies()
	{
		std::vector<Zenith_EntityID> axEnemies;
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		pxSceneData->Query<Zenith_TransformComponent>()
			.ForEach([&](Zenith_EntityID uID, Zenith_TransformComponent&)
			{
				if (IsEnemy(uID))
				{
					axEnemies.push_back(uID);
				}
			});

		return axEnemies;
	}

	/**
	 * FindNearestEnemy - Find the closest enemy to a position
	 */
	static Zenith_EntityID FindNearestEnemy(const Zenith_Maths::Vector3& xPosition)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		Zenith_EntityID uNearestID = INVALID_ENTITY_ID;
		float fNearestDist = std::numeric_limits<float>::max();

		pxSceneData->Query<Zenith_TransformComponent>()
			.ForEach([&](Zenith_EntityID uID, Zenith_TransformComponent& xTransform)
			{
				if (!IsEnemy(uID))
					return;

				Zenith_Maths::Vector3 xEnemyPos;
				xTransform.GetPosition(xEnemyPos);
				float fDist = glm::distance(xPosition, xEnemyPos);

				if (fDist < fNearestDist)
				{
					fNearestDist = fDist;
					uNearestID = uID;
				}
			});

		return uNearestID;
	}

	/**
	 * FindEnemiesInRange - Find all enemies within a radius
	 */
	static std::vector<Combat_EntityDistance> FindEnemiesInRange(
		const Zenith_Maths::Vector3& xPosition, float fRadius)
	{
		std::vector<Combat_EntityDistance> axResults;
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		pxSceneData->Query<Zenith_TransformComponent>()
			.ForEach([&](Zenith_EntityID uID, Zenith_TransformComponent& xTransform)
			{
				if (!IsEnemy(uID))
					return;

				Zenith_Maths::Vector3 xEnemyPos;
				xTransform.GetPosition(xEnemyPos);
				float fDist = glm::distance(xPosition, xEnemyPos);

				if (fDist <= fRadius)
				{
					Combat_EntityDistance xResult;
					xResult.m_uEntityID = uID;
					xResult.m_fDistance = fDist;
					xResult.m_xPosition = xEnemyPos;
					axResults.push_back(xResult);
				}
			});

		// Sort by distance (nearest first)
		std::sort(axResults.begin(), axResults.end(),
			[](const Combat_EntityDistance& a, const Combat_EntityDistance& b)
			{
				return a.m_fDistance < b.m_fDistance;
			});

		return axResults;
	}

	/**
	 * CountLivingEnemies - Count enemies that are still alive
	 * Requires Combat_DamageSystem to be initialized
	 */
	static uint32_t CountLivingEnemies()
	{
		uint32_t uCount = 0;
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		pxSceneData->Query<Zenith_TransformComponent>()
			.ForEach([&](Zenith_EntityID uID, Zenith_TransformComponent&)
			{
				if (IsEnemy(uID))
				{
					// This will be checked against damage system in actual use
					uCount++;
				}
			});

		return uCount;
	}

	// ========================================================================
	// Generic Queries
	// ========================================================================

	/**
	 * FindEntitiesInRange - Find all entities with transform in radius
	 */
	static std::vector<Combat_EntityDistance> FindEntitiesInRange(
		const Zenith_Maths::Vector3& xPosition, float fRadius)
	{
		std::vector<Combat_EntityDistance> axResults;
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		pxSceneData->Query<Zenith_TransformComponent>()
			.ForEach([&](Zenith_EntityID uID, Zenith_TransformComponent& xTransform)
			{
				Zenith_Maths::Vector3 xEntityPos;
				xTransform.GetPosition(xEntityPos);
				float fDist = glm::distance(xPosition, xEntityPos);

				if (fDist <= fRadius)
				{
					Combat_EntityDistance xResult;
					xResult.m_uEntityID = uID;
					xResult.m_fDistance = fDist;
					xResult.m_xPosition = xEntityPos;
					axResults.push_back(xResult);
				}
			});

		return axResults;
	}

	/**
	 * GetEntityPosition - Get position of any entity
	 */
	static bool GetEntityPosition(Zenith_EntityID uEntityID, Zenith_Maths::Vector3& xOutPos)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uEntityID))
			return false;

		Zenith_Entity xEntity = pxSceneData->GetEntity(uEntityID);
		if (!xEntity.HasComponent<Zenith_TransformComponent>())
			return false;

		xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xOutPos);
		return true;
	}

	/**
	 * GetDirectionTo - Get normalized direction from one entity to another
	 */
	static Zenith_Maths::Vector3 GetDirectionTo(Zenith_EntityID uFromID, Zenith_EntityID uToID)
	{
		Zenith_Maths::Vector3 xFromPos, xToPos;
		if (!GetEntityPosition(uFromID, xFromPos) || !GetEntityPosition(uToID, xToPos))
			return Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);

		Zenith_Maths::Vector3 xDir = xToPos - xFromPos;
		xDir.y = 0.0f;  // Keep on XZ plane

		float fLen = glm::length(xDir);
		if (fLen > 0.001f)
			return xDir / fLen;

		return Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}

	/**
	 * GetDistanceBetween - Get distance between two entities
	 */
	static float GetDistanceBetween(Zenith_EntityID uEntityA, Zenith_EntityID uEntityB)
	{
		Zenith_Maths::Vector3 xPosA, xPosB;
		if (!GetEntityPosition(uEntityA, xPosA) || !GetEntityPosition(uEntityB, xPosB))
			return std::numeric_limits<float>::max();

		return glm::distance(xPosA, xPosB);
	}
};
