#pragma once
/**
 * DPProcLevelBootstrap_Behaviour - runs the procgen generator in OnAwake
 * and stashes the resulting LevelLayout for downstream consumption.
 *
 * P4a scope (this commit): the bootstrap CALLS Generate() and stores the
 * layout. No entity spawning yet -- that lands in P4b (walls), P4c
 * (game elements), P4d (villagers + priest). Splitting the cutover into
 * sub-phases lets each step ship behind a still-green DP test suite
 * rather than landing a single megacommit that risks the full suite.
 *
 * Usage today: attach this script to any entity in the scene. On the
 * scene's first OnAwake pass, Generate() runs with seed = m_uSeed
 * (default 0; later PRs wire Tuning.json + a --procgen-seed CLI flag).
 *
 * Singleton pattern mirrors DPFogPass_Behaviour / DPItemManager_Behaviour:
 * s_pxInstance is set in OnAwake and cleared in OnDestroy, so later
 * P4b+ entity-spawn code can pull the cached layout via Instance().
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Physics/Zenith_Physics_Fwd.h"
#include "Maths/Zenith_Maths.h"

#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"

#include <cstdio>
#include <cstdint>
#include <string>

class DPProcLevelBootstrap_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPProcLevelBootstrap_Behaviour)

	DPProcLevelBootstrap_Behaviour() = delete;
	DPProcLevelBootstrap_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		// Set the singleton ASAP so downstream OnAwake hooks (the
		// future P4c/d entity-spawn paths) can query the layout
		// before they themselves run.
		s_pxInstance = this;

		// Seed source TODO (P4 later): read from Tuning.json + allow
		// --procgen-seed CLI override. For now we hardcode m_uSeed
		// (default 0) so the bootstrap is deterministic + the unit
		// test has a predictable layout to assert against.
		DPProcLevel::GenConfig xConfig;  // defaults from header
		const bool bOk = DPProcLevel::Generate(m_uSeed, xConfig, m_xLayout);

		std::printf("[DPProcLevelBootstrap] seed=%llu generated=%d "
			"rooms=%u walls=%u elements=%u villagers=%u patrol=%u priest=%d\n",
			static_cast<unsigned long long>(m_uSeed),
			static_cast<int>(bOk),
			m_xLayout.axRooms.GetSize(),
			m_xLayout.axWallSegments.GetSize(),
			m_xLayout.axGameElements.GetSize(),
			m_xLayout.axVillagerSpawns.GetSize(),
			m_xLayout.axPatrolNodes.GetSize(),
			static_cast<int>(m_xLayout.xPriestSpawn.bValid));
		std::fflush(stdout);

		// P4b: instantiate wall entities from the layout. Each
		// WallSegment becomes a static entity with the SM_Cube mesh
		// scaled to wall dimensions + an OBB collider. The bot's
		// pathfinder (which raycasts downward from y=10) sees these
		// as "tall obstacles" (top > y=1.5) and routes around them.
		SpawnWalls();
	}

	void OnDestroy() ZENITH_FINAL override
	{
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	static DPProcLevelBootstrap_Behaviour* Instance() { return s_pxInstance; }

	const DPProcLevel::LevelLayout& GetLayout() const { return m_xLayout; }
	uint64_t                        GetSeed()   const { return m_uSeed; }

	// Test hook -- the unit test sets a non-zero seed before OnAwake
	// fires so different test seeds produce different layouts. Once
	// the Tuning.json wiring lands this hook is redundant but it lets
	// the test be specific about which seed it's exercising.
	void SetSeedForTest(uint64_t uSeed) { m_uSeed = uSeed; }

	// Count of wall entities the bootstrap actually created (might be
	// less than axWallSegments.GetSize() if entity creation failed).
	// Useful for the P4b smoke test.
	uint32_t GetSpawnedWallCount() const { return m_uSpawnedWalls; }

private:
	// Spawn one entity per WallSegment. SM_Cube has mesh-local bounds
	// (-1, 0, -1) to (1, 4, 1), so to get a wall of world half-extents
	// (hx, _, hz) we scale by (hx, 1, hz) -- the mesh-aware OBB code in
	// Zenith_ColliderComponent then derives the same half-extents from
	// these bounds + scale.
	//
	// Wall Y position: 0 (the body anchor; mesh-aware offset puts the
	// OBB centre at y=2 so the wall spans y=0..4 above the floor).
	// Floor existence is assumed for now -- a later P4 sub-phase will
	// also spawn a ground plane if the scene doesn't already have one.
	void SpawnWalls()
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) return;

		const std::string strMeshPath =
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" + ZENITH_MODEL_EXT;

		uint32_t uSpawned = 0;
		const uint32_t uN = m_xLayout.axWallSegments.GetSize();
		for (uint32_t i = 0; i < uN; ++i)
		{
			const DPProcLevel::WallSegment& xW = m_xLayout.axWallSegments.Get(i);
			char szName[64];
			std::snprintf(szName, sizeof(szName), "ProcWall_%u", i);
			Zenith_Entity xEntity(pxScene, std::string(szName));
			if (!xEntity.IsValid()) continue;

			// Transform: world centre on XZ, anchor at y=0 (so the
			// mesh-aware OBB centre lands at y=2 = wall midpoint).
			if (xEntity.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_TransformComponent& xT = xEntity.GetComponent<Zenith_TransformComponent>();
				xT.SetPosition(Zenith_Maths::Vector3(xW.fCentreX, 0.0f, xW.fCentreZ));
				// Yaw around +Y. The visualiser's R_y matrix (PR #95)
				// is the same convention TransformComponent uses, so
				// the wall's yaw can pass through unchanged.
				const Zenith_Maths::Quat xRot =
					Zenith_Maths::AngleAxis(xW.fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
				xT.SetRotation(xRot);
				// Scale x/z to match the wall's world half-extents.
				// Mesh half-extent on X = 1 (bounds -1..1), so world
				// half-extent = scale * 1 = scale. Same for Z. Y
				// stays 1 so the wall's height stays at the mesh's
				// authored 4 m.
				xT.SetScale(Zenith_Maths::Vector3(
					xW.fHalfExtentX, 1.0f, xW.fHalfExtentZ));
			}

			Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
			xModel.LoadModel(strMeshPath);

			Zenith_ColliderComponent& xCol = xEntity.AddComponent<Zenith_ColliderComponent>();
			xCol.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

			++uSpawned;
		}
		m_uSpawnedWalls = uSpawned;
		std::printf("[DPProcLevelBootstrap] spawned %u/%u wall entities\n",
			uSpawned, uN);
		std::fflush(stdout);
	}

	uint64_t                  m_uSeed = 0ull;
	uint32_t                  m_uSpawnedWalls = 0u;
	DPProcLevel::LevelLayout  m_xLayout;

	static inline DPProcLevelBootstrap_Behaviour* s_pxInstance = nullptr;
};
