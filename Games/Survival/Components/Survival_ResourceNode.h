#pragma once
/**
 * Survival_ResourceNode.h - Harvestable resource nodes (trees, rocks, berry bushes)
 *
 * Manages resource node state including:
 * - Health/hits remaining
 * - Depleted state and respawn timer
 * - Resource type and yield
 *
 * Features:
 * - Hit() to damage the node
 * - Automatic depletion when health reaches 0
 * - Event dispatch on harvest and depletion
 */

#include "Survival_EventBus.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include <cstdint>

/**
 * Survival_ResourceNodeData - Data for a single resource node
 */
struct Survival_ResourceNodeData
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
	SurvivalResourceType m_eResourceType = RESOURCE_TYPE_TREE;
	SurvivalItemType m_eYieldType = ITEM_TYPE_WOOD;

	uint32_t m_uMaxHits = 3;
	uint32_t m_uCurrentHits = 3;
	uint32_t m_uYieldAmount = 3;

	bool m_bDepleted = false;
	float m_fRespawnTimer = 0.f;
	float m_fRespawnDuration = 30.f;

	Zenith_Maths::Vector3 m_xPosition;
	Zenith_Maths::Vector3 m_xOriginalScale;

	/**
	 * Hit - Damage the resource node
	 * @return Amount of resources yielded (0 if already depleted)
	 */
	uint32_t Hit(float fBonusMultiplier = 1.0f)
	{
		if (m_bDepleted || m_uCurrentHits == 0)
			return 0;

		m_uCurrentHits--;

		// Calculate yield for this hit
		uint32_t uYield = static_cast<uint32_t>(static_cast<float>(m_uYieldAmount) * fBonusMultiplier / static_cast<float>(m_uMaxHits));
		if (uYield < 1)
			uYield = 1;

		// Fire harvest event
		Survival_EventBus::Dispatch(Survival_Event_ResourceHarvested{
			m_uEntityID,
			m_eYieldType,
			uYield
		});

		// Check if depleted
		if (m_uCurrentHits == 0)
		{
			m_bDepleted = true;
			m_fRespawnTimer = m_fRespawnDuration;

			Survival_EventBus::Dispatch(Survival_Event_ResourceDepleted{
				m_uEntityID,
				m_eResourceType
			});
		}

		return uYield;
	}

	/**
	 * Update - Update respawn timer
	 * @return true if node respawned this frame
	 */
	bool Update(float fDt)
	{
		if (!m_bDepleted)
			return false;

		m_fRespawnTimer -= fDt;
		if (m_fRespawnTimer <= 0.f)
		{
			// Respawn the node
			m_bDepleted = false;
			m_uCurrentHits = m_uMaxHits;
			m_fRespawnTimer = 0.f;

			Survival_EventBus::Dispatch(Survival_Event_ResourceRespawned{
				m_uEntityID,
				m_eResourceType
			});

			return true;
		}

		return false;
	}

	/**
	 * GetHealthPercentage - Get current health as 0-1
	 */
	float GetHealthPercentage() const
	{
		if (m_uMaxHits == 0)
			return 0.f;
		return static_cast<float>(m_uCurrentHits) / static_cast<float>(m_uMaxHits);
	}

	/**
	 * GetRespawnProgress - Get respawn progress as 0-1
	 */
	float GetRespawnProgress() const
	{
		if (!m_bDepleted || m_fRespawnDuration <= 0.f)
			return 1.f;
		return 1.f - (m_fRespawnTimer / m_fRespawnDuration);
	}
};

/**
 * Survival_ResourceNodeManager - Manages all resource nodes in the world
 */
class Survival_ResourceNodeManager
{
public:
	static constexpr uint32_t s_uMaxNodes = 64;

	Survival_ResourceNodeManager() = default;

	/**
	 * Clear - Remove all nodes
	 */
	void Clear()
	{
		m_uNodeCount = 0;
	}

	/**
	 * AddNode - Register a new resource node
	 */
	uint32_t AddNode(const Survival_ResourceNodeData& xNode)
	{
		if (m_uNodeCount >= s_uMaxNodes)
			return static_cast<uint32_t>(-1);

		m_axNodes[m_uNodeCount] = xNode;
		return m_uNodeCount++;
	}

	/**
	 * GetNode - Get node by index
	 */
	Survival_ResourceNodeData* GetNode(uint32_t uIndex)
	{
		if (uIndex >= m_uNodeCount)
			return nullptr;
		return &m_axNodes[uIndex];
	}

	/**
	 * GetNodeByEntityID - Find node by its entity ID
	 */
	Survival_ResourceNodeData* GetNodeByEntityID(Zenith_EntityID uEntityID)
	{
		for (uint32_t i = 0; i < m_uNodeCount; i++)
		{
			if (m_axNodes[i].m_uEntityID == uEntityID)
				return &m_axNodes[i];
		}
		return nullptr;
	}

	/**
	 * UpdateAll - Update all nodes (respawn timers)
	 * @return Number of nodes that respawned
	 */
	uint32_t UpdateAll(float fDt)
	{
		uint32_t uRespawned = 0;
		for (uint32_t i = 0; i < m_uNodeCount; i++)
		{
			if (m_axNodes[i].Update(fDt))
			{
				uRespawned++;
			}
		}
		return uRespawned;
	}

	/**
	 * UpdateNodeVisuals - Update visual representation based on depletion state
	 */
	void UpdateNodeVisuals()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		for (uint32_t i = 0; i < m_uNodeCount; i++)
		{
			Survival_ResourceNodeData& xNode = m_axNodes[i];
			if (!pxSceneData->EntityExists(xNode.m_uEntityID))
				continue;

			Zenith_Entity xEntity = pxSceneData->GetEntity(xNode.m_uEntityID);
			if (!xEntity.HasComponent<Zenith_TransformComponent>())
				continue;

			Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

			if (xNode.m_bDepleted)
			{
				// Scale down depleted nodes
				Zenith_Maths::Vector3 xScale = xNode.m_xOriginalScale * 0.3f;
				xTransform.SetScale(xScale);
			}
			else
			{
				// Scale based on health
				float fHealthScale = 0.7f + 0.3f * xNode.GetHealthPercentage();
				Zenith_Maths::Vector3 xScale = xNode.m_xOriginalScale * fHealthScale;
				xTransform.SetScale(xScale);
			}
		}
	}

	/**
	 * GetCount - Get number of registered nodes
	 */
	uint32_t GetCount() const { return m_uNodeCount; }

	/**
	 * GetActiveCount - Get number of non-depleted nodes
	 */
	uint32_t GetActiveCount() const
	{
		uint32_t uActive = 0;
		for (uint32_t i = 0; i < m_uNodeCount; i++)
		{
			if (!m_axNodes[i].m_bDepleted)
				uActive++;
		}
		return uActive;
	}

	/**
	 * GetDepletedCount - Get number of depleted nodes
	 */
	uint32_t GetDepletedCount() const
	{
		return m_uNodeCount - GetActiveCount();
	}

	/**
	 * ForEach - Iterate over all nodes with a callback
	 */
	template<typename Func>
	void ForEach(Func&& fn)
	{
		for (uint32_t i = 0; i < m_uNodeCount; i++)
		{
			fn(m_axNodes[i], i);
		}
	}

	/**
	 * ForEachActive - Iterate over non-depleted nodes
	 */
	template<typename Func>
	void ForEachActive(Func&& fn)
	{
		for (uint32_t i = 0; i < m_uNodeCount; i++)
		{
			if (!m_axNodes[i].m_bDepleted)
			{
				fn(m_axNodes[i], i);
			}
		}
	}

private:
	Survival_ResourceNodeData m_axNodes[s_uMaxNodes];
	uint32_t m_uNodeCount = 0;
};
