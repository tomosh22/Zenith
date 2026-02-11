#pragma once
/**
 * Runner_CollectibleSpawner.h - Spawning collectibles and obstacles
 *
 * Demonstrates:
 * - Procedural entity spawning
 * - Object pooling pattern
 * - Distance-based pickup detection
 * - Obstacle collision checking
 *
 * Spawns:
 * - Collectibles (coins/items) in lanes ahead of player
 * - Obstacles (jump over or slide under) in lanes
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Prefab/Zenith_Prefab.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Maths/Zenith_Maths.h"
#include <vector>
#include <random>

/**
 * Runner_CollectibleSpawner - Manages collectibles and obstacles
 */
class Runner_CollectibleSpawner
{
public:
	// ========================================================================
	// Configuration
	// ========================================================================
	struct Config
	{
		float m_fCollectibleSpawnDistance = 80.0f;
		float m_fCollectibleRadius = 0.5f;
		float m_fCollectibleBobSpeed = 3.0f;
		float m_fCollectibleBobHeight = 0.3f;
		float m_fCollectibleRotateSpeed = 2.0f;
		uint32_t m_uPointsPerCollectible = 10;

		float m_fObstacleSpawnDistance = 50.0f;
		float m_fMinObstacleGap = 15.0f;
		float m_fMaxObstacleGap = 30.0f;
		float m_fObstacleHeight = 1.5f;
		float m_fSlideObstacleHeight = 2.5f;

		uint32_t m_uLaneCount = 3;
		float m_fLaneWidth = 3.0f;
	};

	// ========================================================================
	// Collectible types
	// ========================================================================
	struct Collectible
	{
		Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xBasePosition;
		int32_t m_iLane = 0;
		bool m_bCollected = false;
	};

	enum class ObstacleType
	{
		JUMP,   // Low obstacle - jump over
		SLIDE   // High obstacle - slide under
	};

	struct Obstacle
	{
		Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition;
		Zenith_Maths::Vector3 m_xSize;
		int32_t m_iLane = 0;
		ObstacleType m_eType;
		bool m_bActive = true;
	};

	// ========================================================================
	// Result structures
	// ========================================================================
	struct CollectionResult
	{
		uint32_t m_uPointsGained = 0;
		uint32_t m_uCollectedCount = 0;
	};

	// ========================================================================
	// Initialization
	// ========================================================================
	static void Initialize(
		const Config& xConfig,
		Zenith_Prefab* pxCollectiblePrefab,
		Zenith_Prefab* pxObstaclePrefab,
		Flux_MeshGeometry* pxSphereGeometry,
		Flux_MeshGeometry* pxCubeGeometry,
		Zenith_MaterialAsset* pxCollectibleMaterial,
		Zenith_MaterialAsset* pxObstacleMaterial,
		std::mt19937& xRng)
	{
		s_xConfig = xConfig;
		s_pxCollectiblePrefab = pxCollectiblePrefab;
		s_pxObstaclePrefab = pxObstaclePrefab;
		s_pxSphereGeometry = pxSphereGeometry;
		s_pxCubeGeometry = pxCubeGeometry;
		s_pxCollectibleMaterial = pxCollectibleMaterial;
		s_pxObstacleMaterial = pxObstacleMaterial;
		s_pxRng = &xRng;

		Reset();
	}

	static void Reset()
	{
		// Destroy existing entities
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		for (auto& xColl : s_axCollectibles)
		{
			if (xColl.m_uEntityID.IsValid() && pxSceneData->EntityExists(xColl.m_uEntityID))
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(xColl.m_uEntityID);
				Zenith_SceneManager::Destroy(xEntity);
			}
		}
		s_axCollectibles.clear();

		for (auto& xObs : s_axObstacles)
		{
			if (xObs.m_uEntityID.IsValid() && pxSceneData->EntityExists(xObs.m_uEntityID))
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(xObs.m_uEntityID);
				Zenith_SceneManager::Destroy(xEntity);
			}
		}
		s_axObstacles.clear();

		s_fNextCollectibleZ = 20.0f;  // First collectible at Z=20
		s_fNextObstacleZ = 40.0f;     // First obstacle at Z=40
		s_fTotalTime = 0.0f;
	}

	// ========================================================================
	// Update
	// ========================================================================
	static void Update(float fDt, float fPlayerZ)
	{
		s_fTotalTime += fDt;

		// Spawn collectibles ahead
		SpawnCollectiblesAhead(fPlayerZ);

		// Spawn obstacles ahead
		SpawnObstaclesAhead(fPlayerZ);

		// Animate collectibles
		AnimateCollectibles(fDt);

		// Remove passed entities
		RemovePassedEntities(fPlayerZ);
	}

	// ========================================================================
	// Collection Detection
	// ========================================================================
	static CollectionResult CheckCollectibles(const Zenith_Maths::Vector3& xPlayerPos, float fPlayerRadius)
	{
		CollectionResult xResult;
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		for (auto& xColl : s_axCollectibles)
		{
			if (xColl.m_bCollected || !xColl.m_uEntityID.IsValid())
			{
				continue;
			}

			if (!pxSceneData->EntityExists(xColl.m_uEntityID))
			{
				xColl.m_bCollected = true;
				continue;
			}

			Zenith_Entity xEntity = pxSceneData->GetEntity(xColl.m_uEntityID);
			Zenith_Maths::Vector3 xCollPos;
			xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xCollPos);

			float fDist = glm::length(xCollPos - xPlayerPos);
			float fCollectDist = fPlayerRadius + s_xConfig.m_fCollectibleRadius;

			if (fDist < fCollectDist)
			{
				// Collected!
				xColl.m_bCollected = true;
				xResult.m_uPointsGained += s_xConfig.m_uPointsPerCollectible;
				xResult.m_uCollectedCount++;

				// Destroy entity
				Zenith_SceneManager::Destroy(xEntity);
				xColl.m_uEntityID = INVALID_ENTITY_ID;
			}
		}

		return xResult;
	}

	// ========================================================================
	// Obstacle Collision Detection
	// ========================================================================
	static bool CheckObstacleCollision(
		const Zenith_Maths::Vector3& xPlayerPos,
		float fPlayerRadius,
		float fPlayerHeight)
	{
		for (const auto& xObs : s_axObstacles)
		{
			if (!xObs.m_bActive)
			{
				continue;
			}

			// Simple AABB collision check
			Zenith_Maths::Vector3 xObsMin = xObs.m_xPosition - xObs.m_xSize * 0.5f;
			Zenith_Maths::Vector3 xObsMax = xObs.m_xPosition + xObs.m_xSize * 0.5f;

			// Player bounds
			Zenith_Maths::Vector3 xPlayerMin(
				xPlayerPos.x - fPlayerRadius,
				xPlayerPos.y - fPlayerHeight * 0.5f,
				xPlayerPos.z - fPlayerRadius
			);
			Zenith_Maths::Vector3 xPlayerMax(
				xPlayerPos.x + fPlayerRadius,
				xPlayerPos.y + fPlayerHeight * 0.5f,
				xPlayerPos.z + fPlayerRadius
			);

			// Check overlap
			bool bOverlap =
				xPlayerMax.x > xObsMin.x && xPlayerMin.x < xObsMax.x &&
				xPlayerMax.y > xObsMin.y && xPlayerMin.y < xObsMax.y &&
				xPlayerMax.z > xObsMin.z && xPlayerMin.z < xObsMax.z;

			if (bOverlap)
			{
				// For slide obstacles, check if player is low enough
				if (xObs.m_eType == ObstacleType::SLIDE)
				{
					// Slide obstacle has gap at bottom
					float fGapTop = xObs.m_xPosition.y - xObs.m_xSize.y * 0.5f;
					if (xPlayerMax.y < fGapTop)
					{
						continue;  // Player is below obstacle
					}
				}

				return true;  // Collision!
			}
		}

		return false;
	}

	// ========================================================================
	// Accessors
	// ========================================================================
	static const std::vector<Collectible>& GetCollectibles() { return s_axCollectibles; }
	static const std::vector<Obstacle>& GetObstacles() { return s_axObstacles; }

private:
	// ========================================================================
	// Spawning
	// ========================================================================
	static void SpawnCollectiblesAhead(float fPlayerZ)
	{
		float fSpawnLimit = fPlayerZ + s_xConfig.m_fCollectibleSpawnDistance;

		while (s_fNextCollectibleZ < fSpawnLimit)
		{
			// Random lane
			std::uniform_int_distribution<int32_t> xLaneDist(0, static_cast<int32_t>(s_xConfig.m_uLaneCount) - 1);
			int32_t iLane = xLaneDist(*s_pxRng);

			// Calculate X position from lane
			int32_t iHalfLanes = static_cast<int32_t>(s_xConfig.m_uLaneCount) / 2;
			float fX = (static_cast<float>(iLane) - static_cast<float>(iHalfLanes)) * s_xConfig.m_fLaneWidth;

			// Spawn collectible
			SpawnCollectible(Zenith_Maths::Vector3(fX, 1.0f, s_fNextCollectibleZ), iLane);

			// Next collectible spacing
			std::uniform_real_distribution<float> xGapDist(8.0f, 15.0f);
			s_fNextCollectibleZ += xGapDist(*s_pxRng);
		}
	}

	static void SpawnObstaclesAhead(float fPlayerZ)
	{
		float fSpawnLimit = fPlayerZ + s_xConfig.m_fObstacleSpawnDistance;

		while (s_fNextObstacleZ < fSpawnLimit)
		{
			// Random lane
			std::uniform_int_distribution<int32_t> xLaneDist(0, static_cast<int32_t>(s_xConfig.m_uLaneCount) - 1);
			int32_t iLane = xLaneDist(*s_pxRng);

			// Random type
			std::uniform_int_distribution<int32_t> xTypeDist(0, 1);
			ObstacleType eType = static_cast<ObstacleType>(xTypeDist(*s_pxRng));

			// Calculate X position from lane
			int32_t iHalfLanes = static_cast<int32_t>(s_xConfig.m_uLaneCount) / 2;
			float fX = (static_cast<float>(iLane) - static_cast<float>(iHalfLanes)) * s_xConfig.m_fLaneWidth;

			// Y position and size based on type
			float fY, fHeight;
			if (eType == ObstacleType::JUMP)
			{
				fHeight = s_xConfig.m_fObstacleHeight;
				fY = fHeight * 0.5f;
			}
			else
			{
				// Slide obstacle - positioned higher with gap below
				fHeight = 1.0f;
				fY = s_xConfig.m_fSlideObstacleHeight;
			}

			// Spawn obstacle
			SpawnObstacle(Zenith_Maths::Vector3(fX, fY, s_fNextObstacleZ), iLane, eType);

			// Next obstacle spacing
			std::uniform_real_distribution<float> xGapDist(s_xConfig.m_fMinObstacleGap, s_xConfig.m_fMaxObstacleGap);
			s_fNextObstacleZ += xGapDist(*s_pxRng);
		}
	}

	static void SpawnCollectible(const Zenith_Maths::Vector3& xPos, int32_t iLane)
	{
		if (s_pxCollectiblePrefab == nullptr || s_pxSphereGeometry == nullptr || s_pxCollectibleMaterial == nullptr)
		{
			return;
		}

		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		Zenith_Entity xColl = s_pxCollectiblePrefab->Instantiate(pxSceneData, "Collectible");

		Zenith_TransformComponent& xTransform = xColl.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(Zenith_Maths::Vector3(s_xConfig.m_fCollectibleRadius * 2.0f));

		Zenith_ModelComponent& xModel = xColl.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*s_pxSphereGeometry, *s_pxCollectibleMaterial);

		Collectible xCollData;
		xCollData.m_uEntityID = xColl.GetEntityID();
		xCollData.m_xBasePosition = xPos;
		xCollData.m_iLane = iLane;
		xCollData.m_bCollected = false;

		s_axCollectibles.push_back(xCollData);
	}

	static void SpawnObstacle(const Zenith_Maths::Vector3& xPos, int32_t iLane, ObstacleType eType)
	{
		if (s_pxObstaclePrefab == nullptr || s_pxCubeGeometry == nullptr || s_pxObstacleMaterial == nullptr)
		{
			return;
		}

		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		Zenith_Entity xObs = s_pxObstaclePrefab->Instantiate(pxSceneData, "Obstacle");

		// Size based on type
		Zenith_Maths::Vector3 xSize;
		if (eType == ObstacleType::JUMP)
		{
			xSize = Zenith_Maths::Vector3(s_xConfig.m_fLaneWidth * 0.8f, s_xConfig.m_fObstacleHeight, 1.0f);
		}
		else
		{
			// Slide obstacle - wide beam at height
			xSize = Zenith_Maths::Vector3(s_xConfig.m_fLaneWidth * 0.8f, 1.0f, 2.0f);
		}

		Zenith_TransformComponent& xTransform = xObs.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(xSize);

		Zenith_ModelComponent& xModel = xObs.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*s_pxCubeGeometry, *s_pxObstacleMaterial);

		Obstacle xObsData;
		xObsData.m_uEntityID = xObs.GetEntityID();
		xObsData.m_xPosition = xPos;
		xObsData.m_xSize = xSize;
		xObsData.m_iLane = iLane;
		xObsData.m_eType = eType;
		xObsData.m_bActive = true;

		s_axObstacles.push_back(xObsData);
	}

	// ========================================================================
	// Animation
	// ========================================================================
	static void AnimateCollectibles(float fDt)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		for (auto& xColl : s_axCollectibles)
		{
			if (xColl.m_bCollected || !xColl.m_uEntityID.IsValid())
			{
				continue;
			}

			if (!pxSceneData->EntityExists(xColl.m_uEntityID))
			{
				continue;
			}

			Zenith_Entity xEntity = pxSceneData->GetEntity(xColl.m_uEntityID);
			Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

			// Bob up and down
			float fBob = sin(s_fTotalTime * s_xConfig.m_fCollectibleBobSpeed + xColl.m_xBasePosition.z) * s_xConfig.m_fCollectibleBobHeight;
			Zenith_Maths::Vector3 xPos = xColl.m_xBasePosition;
			xPos.y += fBob;
			xTransform.SetPosition(xPos);

			// Rotate around Y axis
			Zenith_Maths::Quat xRot;
			xTransform.GetRotation(xRot);
			Zenith_Maths::Quat xDeltaRot = glm::angleAxis(s_xConfig.m_fCollectibleRotateSpeed * fDt, Zenith_Maths::Vector3(0.f, 1.f, 0.f));
			xTransform.SetRotation(xDeltaRot * xRot);
		}
	}

	// ========================================================================
	// Cleanup
	// ========================================================================
	static void RemovePassedEntities(float fPlayerZ)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		float fRemoveThreshold = fPlayerZ - 20.0f;

		// Remove passed collectibles
		for (auto it = s_axCollectibles.begin(); it != s_axCollectibles.end();)
		{
			if (it->m_xBasePosition.z < fRemoveThreshold)
			{
				if (it->m_uEntityID.IsValid() && pxSceneData->EntityExists(it->m_uEntityID))
				{
					Zenith_Entity xEntity = pxSceneData->GetEntity(it->m_uEntityID);
					Zenith_SceneManager::Destroy(xEntity);
				}
				it = s_axCollectibles.erase(it);
			}
			else
			{
				++it;
			}
		}

		// Remove passed obstacles
		for (auto it = s_axObstacles.begin(); it != s_axObstacles.end();)
		{
			if (it->m_xPosition.z < fRemoveThreshold)
			{
				if (it->m_uEntityID.IsValid() && pxSceneData->EntityExists(it->m_uEntityID))
				{
					Zenith_Entity xEntity = pxSceneData->GetEntity(it->m_uEntityID);
					Zenith_SceneManager::Destroy(xEntity);
				}
				it = s_axObstacles.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	// ========================================================================
	// Static State
	// ========================================================================
	static inline Config s_xConfig;
	static inline std::vector<Collectible> s_axCollectibles;
	static inline std::vector<Obstacle> s_axObstacles;
	static inline float s_fNextCollectibleZ = 20.0f;
	static inline float s_fNextObstacleZ = 40.0f;
	static inline float s_fTotalTime = 0.0f;

	static inline Zenith_Prefab* s_pxCollectiblePrefab = nullptr;
	static inline Zenith_Prefab* s_pxObstaclePrefab = nullptr;
	static inline Flux_MeshGeometry* s_pxSphereGeometry = nullptr;
	static inline Flux_MeshGeometry* s_pxCubeGeometry = nullptr;
	static inline Zenith_MaterialAsset* s_pxCollectibleMaterial = nullptr;
	static inline Zenith_MaterialAsset* s_pxObstacleMaterial = nullptr;
	static inline std::mt19937* s_pxRng = nullptr;
};
