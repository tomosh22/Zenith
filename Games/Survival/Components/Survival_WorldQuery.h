#pragma once
/**
 * Survival_WorldQuery.h - Entity queries using Zenith_Query
 *
 * Demonstrates the Zenith_Query system for finding entities with
 * specific component combinations.
 *
 * Key features:
 * - Find nearest resource node to player
 * - Find all entities in range
 * - Query entities by component combination
 *
 * Usage:
 *   // Find nearest tree
 *   Zenith_EntityID uNearest = Survival_WorldQuery::FindNearestResourceInRange(
 *       xPlayerPos, fInteractionRange, xResourceManager);
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Maths/Zenith_Maths.h"
#include "Survival_ResourceNode.h"
#include <vector>
#include <limits>

/**
 * Survival_WorldQuery - Entity query utilities
 */
class Survival_WorldQuery
{
public:
	/**
	 * QueryResult - Result of a proximity query
	 */
	struct QueryResult
	{
		Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
		float m_fDistance = FLT_MAX;
		uint32_t m_uNodeIndex = static_cast<uint32_t>(-1);
	};

	/**
	 * FindNearestResourceInRange - Find the closest non-depleted resource node
	 *
	 * @param xPlayerPos    Player's world position
	 * @param fMaxRange     Maximum interaction range
	 * @param xResourceMgr  Resource node manager to search
	 * @return QueryResult with nearest entity info, or invalid if none found
	 */
	static QueryResult FindNearestResourceInRange(
		const Zenith_Maths::Vector3& xPlayerPos,
		float fMaxRange,
		Survival_ResourceNodeManager& xResourceMgr)
	{
		QueryResult xResult;
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		xResourceMgr.ForEachActive([&](Survival_ResourceNodeData& xNode, uint32_t uIndex)
		{
			if (!pxSceneData->EntityExists(xNode.m_uEntityID))
				return;

			float fDist = glm::distance(xPlayerPos, xNode.m_xPosition);
			if (fDist <= fMaxRange && fDist < xResult.m_fDistance)
			{
				xResult.m_uEntityID = xNode.m_uEntityID;
				xResult.m_fDistance = fDist;
				xResult.m_uNodeIndex = uIndex;
			}
		});

		return xResult;
	}

	/**
	 * FindAllResourcesInRange - Find all non-depleted resources within range
	 *
	 * @param xPlayerPos    Player's world position
	 * @param fMaxRange     Maximum range
	 * @param xResourceMgr  Resource node manager
	 * @param axResults     Output vector of query results
	 */
	static void FindAllResourcesInRange(
		const Zenith_Maths::Vector3& xPlayerPos,
		float fMaxRange,
		Survival_ResourceNodeManager& xResourceMgr,
		std::vector<QueryResult>& axResults)
	{
		axResults.clear();
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		xResourceMgr.ForEachActive([&](Survival_ResourceNodeData& xNode, uint32_t uIndex)
		{
			if (!pxSceneData->EntityExists(xNode.m_uEntityID))
				return;

			float fDist = glm::distance(xPlayerPos, xNode.m_xPosition);
			if (fDist <= fMaxRange)
			{
				QueryResult xResult;
				xResult.m_uEntityID = xNode.m_uEntityID;
				xResult.m_fDistance = fDist;
				xResult.m_uNodeIndex = uIndex;
				axResults.push_back(xResult);
			}
		});

		// Sort by distance
		std::sort(axResults.begin(), axResults.end(),
			[](const QueryResult& a, const QueryResult& b)
			{
				return a.m_fDistance < b.m_fDistance;
			});
	}

	/**
	 * CountEntitiesWithTransform - Count all entities with TransformComponent
	 * Demonstrates basic Query usage
	 */
	static uint32_t CountEntitiesWithTransform()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		return pxSceneData->Query<Zenith_TransformComponent>().Count();
	}

	/**
	 * CountRenderableEntities - Count entities with both Transform and Model
	 * Demonstrates multi-component Query
	 */
	static uint32_t CountRenderableEntities()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		return pxSceneData->Query<Zenith_TransformComponent, Zenith_ModelComponent>().Count();
	}

	/**
	 * ForEachRenderableInRange - Iterate all renderable entities within range of a point
	 * Demonstrates ForEach with lambda and distance filtering
	 */
	template<typename Func>
	static void ForEachRenderableInRange(
		const Zenith_Maths::Vector3& xCenter,
		float fRange,
		Func&& fn)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		pxSceneData->Query<Zenith_TransformComponent, Zenith_ModelComponent>()
			.ForEach([&](Zenith_EntityID uID, Zenith_TransformComponent& xTransform, Zenith_ModelComponent& xModel)
			{
				Zenith_Maths::Vector3 xPos;
				xTransform.GetPosition(xPos);

				float fDist = glm::distance(xCenter, xPos);
				if (fDist <= fRange)
				{
					fn(uID, xTransform, xModel, fDist);
				}
			});
	}

	/**
	 * FindFirstEntityWithTransform - Find any entity with a transform
	 * Demonstrates First() method
	 */
	static Zenith_EntityID FindFirstEntityWithTransform()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		return pxSceneData->Query<Zenith_TransformComponent>().First();
	}

	/**
	 * HasAnyRenderableEntities - Check if scene has any renderable entities
	 * Demonstrates Any() method
	 */
	static bool HasAnyRenderableEntities()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		return pxSceneData->Query<Zenith_TransformComponent, Zenith_ModelComponent>().Any();
	}

	/**
	 * GetEntityPosition - Helper to get position of any entity
	 */
	static bool GetEntityPosition(Zenith_EntityID uEntityID, Zenith_Maths::Vector3& xPosOut)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(uEntityID))
			return false;

		Zenith_Entity xEntity = pxSceneData->GetEntity(uEntityID);
		if (!xEntity.HasComponent<Zenith_TransformComponent>())
			return false;

		xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPosOut);
		return true;
	}

	/**
	 * GetResourceAtPosition - Find resource node at approximate position
	 */
	static QueryResult GetResourceAtPosition(
		const Zenith_Maths::Vector3& xPos,
		float fTolerance,
		Survival_ResourceNodeManager& xResourceMgr)
	{
		QueryResult xResult;

		xResourceMgr.ForEach([&](Survival_ResourceNodeData& xNode, uint32_t uIndex)
		{
			float fDist = glm::distance(xPos, xNode.m_xPosition);
			if (fDist <= fTolerance && fDist < xResult.m_fDistance)
			{
				xResult.m_uEntityID = xNode.m_uEntityID;
				xResult.m_fDistance = fDist;
				xResult.m_uNodeIndex = uIndex;
			}
		});

		return xResult;
	}
};
