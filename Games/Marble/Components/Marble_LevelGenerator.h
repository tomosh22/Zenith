#pragma once
/**
 * Marble_LevelGenerator.h - Procedural level creation
 *
 * Demonstrates:
 * - Prefab-based entity creation
 * - Component order: Transform -> Model -> Collider
 * - Procedural platform placement in circular pattern
 * - Random distribution with std::uniform_real_distribution
 *
 * Level layout:
 * - Start platform at origin
 * - Platforms spiral outward
 * - Goal platform at the end
 * - Collectibles placed on platforms
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Prefab/Zenith_Prefab.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Maths/Zenith_Maths.h"
#include <vector>
#include <random>

// Level configuration
static constexpr uint32_t s_uMarblePlatformCount = 8;
static constexpr uint32_t s_uMarbleCollectibleCount = 5;
static constexpr float s_fMarbleCollectibleRadius = 0.3f;
static constexpr float s_fMarbleBallRadius = 0.5f;

/**
 * Marble_LevelGenerator - Procedural level generation
 */
class Marble_LevelGenerator
{
public:
	/**
	 * LevelEntities - Holds all created entity IDs for cleanup
	 */
	struct LevelEntities
	{
		Zenith_EntityID uBallEntityID = INVALID_ENTITY_ID;
		Zenith_EntityID uGoalEntityID = INVALID_ENTITY_ID;
		std::vector<Zenith_EntityID> axPlatformEntityIDs;
		std::vector<Zenith_EntityID> axCollectibleEntityIDs;
	};

	/**
	 * GenerateLevel - Create a complete level
	 *
	 * Creates:
	 * - Starting platform at origin
	 * - Ball on starting platform
	 * - Spiral of platforms outward
	 * - Goal platform at end
	 * - Collectibles on platforms
	 */
	static void GenerateLevel(
		LevelEntities& xEntities,
		std::mt19937& xRng,
		Zenith_Prefab* pxBallPrefab,
		Zenith_Prefab* pxPlatformPrefab,
		Zenith_Prefab* pxGoalPrefab,
		Zenith_Prefab* pxCollectiblePrefab,
		Flux_MeshGeometry* pxSphereGeometry,
		Flux_MeshGeometry* pxCubeGeometry,
		Flux_MaterialAsset* pxBallMaterial,
		Flux_MaterialAsset* pxPlatformMaterial,
		Flux_MaterialAsset* pxGoalMaterial,
		Flux_MaterialAsset* pxCollectibleMaterial)
	{
		// Random distributions
		std::uniform_real_distribution<float> xSizeDist(2.0f, 5.0f);
		std::uniform_real_distribution<float> xHeightDist(-1.0f, 2.0f);
		std::uniform_real_distribution<float> xAngleDist(0.f, 6.28f);

		// Create starting platform (large, at origin)
		CreatePlatform(xEntities, pxPlatformPrefab, pxCubeGeometry, pxPlatformMaterial,
			Zenith_Maths::Vector3(0.f, 0.f, 0.f),
			Zenith_Maths::Vector3(5.f, 0.5f, 5.f));

		// Create ball on starting platform
		CreateBall(xEntities, pxBallPrefab, pxSphereGeometry, pxBallMaterial,
			Zenith_Maths::Vector3(0.f, s_fMarbleBallRadius + 0.5f, 0.f));

		// Generate spiral of platforms
		float fRadius = 8.0f;
		for (uint32_t i = 0; i < s_uMarblePlatformCount; i++)
		{
			float fAngle = xAngleDist(xRng);
			float fX = cos(fAngle) * fRadius;
			float fZ = sin(fAngle) * fRadius;
			float fY = xHeightDist(xRng);

			float fSizeX = xSizeDist(xRng);
			float fSizeZ = xSizeDist(xRng);

			CreatePlatform(xEntities, pxPlatformPrefab, pxCubeGeometry, pxPlatformMaterial,
				Zenith_Maths::Vector3(fX, fY, fZ),
				Zenith_Maths::Vector3(fSizeX, 0.5f, fSizeZ));

			fRadius += 5.0f;
		}

		// Create goal platform at end
		float fGoalAngle = xAngleDist(xRng);
		float fGoalX = cos(fGoalAngle) * (fRadius + 5.0f);
		float fGoalZ = sin(fGoalAngle) * (fRadius + 5.0f);
		CreateGoalPlatform(xEntities, pxGoalPrefab, pxCubeGeometry, pxGoalMaterial,
			Zenith_Maths::Vector3(fGoalX, 1.0f, fGoalZ));

		// Create collectibles on platforms
		CreateCollectibles(xEntities, pxCollectiblePrefab, pxSphereGeometry, pxCollectibleMaterial, xRng);
	}

	/**
	 * DestroyLevel - Clean up all level entities
	 */
	static void DestroyLevel(LevelEntities& xEntities)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		if (xEntities.uBallEntityID.IsValid() && xScene.EntityExists(xEntities.uBallEntityID))
		{
			Zenith_Scene::Destroy(xEntities.uBallEntityID);
			xEntities.uBallEntityID = INVALID_ENTITY_ID;
		}

		for (Zenith_EntityID uID : xEntities.axPlatformEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		xEntities.axPlatformEntityIDs.clear();

		for (Zenith_EntityID uID : xEntities.axCollectibleEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		xEntities.axCollectibleEntityIDs.clear();

		if (xEntities.uGoalEntityID.IsValid() && xScene.EntityExists(xEntities.uGoalEntityID))
		{
			Zenith_Scene::Destroy(xEntities.uGoalEntityID);
			xEntities.uGoalEntityID = INVALID_ENTITY_ID;
		}
	}

private:
	/**
	 * CreatePlatform - Create a static platform entity
	 *
	 * Order matters:
	 * 1. Instantiate from prefab (gets TransformComponent)
	 * 2. Set position and scale on transform
	 * 3. Add ModelComponent for rendering
	 * 4. Add ColliderComponent LAST (reads transform for physics body)
	 */
	static void CreatePlatform(
		LevelEntities& xEntities,
		Zenith_Prefab* pxPrefab,
		Flux_MeshGeometry* pxMesh,
		Flux_MaterialAsset* pxMaterial,
		const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Vector3& xScale)
	{
		Zenith_Entity xPlatform = Zenith_Scene::Instantiate(*pxPrefab, "Platform");

		// 1. Set transform first
		Zenith_TransformComponent& xTransform = xPlatform.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(xScale);

		// 2. Add model for rendering
		Zenith_ModelComponent& xModel = xPlatform.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxMesh, *pxMaterial);

		// 3. Add collider last (physics body uses transform)
		xPlatform.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		xEntities.axPlatformEntityIDs.push_back(xPlatform.GetEntityID());
	}

	static void CreateGoalPlatform(
		LevelEntities& xEntities,
		Zenith_Prefab* pxPrefab,
		Flux_MeshGeometry* pxMesh,
		Flux_MaterialAsset* pxMaterial,
		const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xGoal = Zenith_Scene::Instantiate(*pxPrefab, "Goal");

		Zenith_TransformComponent& xTransform = xGoal.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(Zenith_Maths::Vector3(4.f, 0.3f, 4.f));

		Zenith_ModelComponent& xModel = xGoal.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxMesh, *pxMaterial);

		xGoal.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		xEntities.uGoalEntityID = xGoal.GetEntityID();
	}

	static void CreateBall(
		LevelEntities& xEntities,
		Zenith_Prefab* pxPrefab,
		Flux_MeshGeometry* pxMesh,
		Flux_MaterialAsset* pxMaterial,
		const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xBall = Zenith_Scene::Instantiate(*pxPrefab, "Ball");

		Zenith_TransformComponent& xTransform = xBall.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(Zenith_Maths::Vector3(s_fMarbleBallRadius * 2.f));

		Zenith_ModelComponent& xModel = xBall.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxMesh, *pxMaterial);

		// Dynamic body for ball
		xBall.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

		xEntities.uBallEntityID = xBall.GetEntityID();
	}

	static void CreateCollectibles(
		LevelEntities& xEntities,
		Zenith_Prefab* pxPrefab,
		Flux_MeshGeometry* pxMesh,
		Flux_MaterialAsset* pxMaterial,
		std::mt19937& xRng)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Place collectibles above platforms
		std::uniform_int_distribution<size_t> xPlatformDist(0, xEntities.axPlatformEntityIDs.size() - 1);

		for (uint32_t i = 0; i < s_uMarbleCollectibleCount && i < xEntities.axPlatformEntityIDs.size(); i++)
		{
			size_t uPlatformIdx = i; // One per platform initially

			Zenith_EntityID uPlatformID = xEntities.axPlatformEntityIDs[uPlatformIdx];
			if (!xScene.EntityExists(uPlatformID))
				continue;

			Zenith_Entity xPlatform = xScene.GetEntityByID(uPlatformID);
			Zenith_Maths::Vector3 xPlatPos, xPlatScale;
			xPlatform.GetComponent<Zenith_TransformComponent>().GetPosition(xPlatPos);
			xPlatform.GetComponent<Zenith_TransformComponent>().GetScale(xPlatScale);

			// Place collectible above platform center
			Zenith_Maths::Vector3 xCollPos = xPlatPos + Zenith_Maths::Vector3(0.f, xPlatScale.y + 1.0f, 0.f);

			Zenith_Entity xCollectible = Zenith_Scene::Instantiate(*pxPrefab, "Collectible");

			Zenith_TransformComponent& xTransform = xCollectible.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(xCollPos);
			xTransform.SetScale(Zenith_Maths::Vector3(s_fMarbleCollectibleRadius * 2.f));

			Zenith_ModelComponent& xModel = xCollectible.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*pxMesh, *pxMaterial);

			// Note: Collectibles have no physics - use distance check for pickup
			xEntities.axCollectibleEntityIDs.push_back(xCollectible.GetEntityID());
		}
	}
};
