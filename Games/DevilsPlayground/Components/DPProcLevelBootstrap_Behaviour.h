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
#include "Physics/Zenith_Physics.h"  // SetGravityEnabled / LockRotation for character bodies
#include "Maths/Zenith_Maths.h"

#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"
#include "Source/DevilsPlayground_Tags.h"

// Behaviour types attached to procgen-spawned game elements. The bootstrap
// is the canonical place game scripts get wired up onto procgen-generated
// geometry, so it owns the include list rather than forwarding.
#include "Components/DPPentagram_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/DummyNoiseMachine_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPOrbitCamera_Behaviour.h"
#include "Source/PublicInterfaces.h"
#include "Source/DPParticles.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
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

		// Seed source: env var DP_PROCGEN_SEED (decimal uint64) when
		// present and non-empty; otherwise the m_uSeed value baked in
		// at construction (default 0) so the bootstrap stays
		// deterministic for the unit tests. Test_ProcLevelBootstrap +
		// Test_ProcLevelScene still drive m_uSeed directly via
		// SetSeedForTest before OnAwake fires, so they're unaffected
		// by whatever the env var says.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)  // std::getenv "may be unsafe"
#endif
		if (const char* szEnv = std::getenv("DP_PROCGEN_SEED"))
		{
			if (szEnv[0] != '\0')
			{
				char* pszEnd = nullptr;
				const unsigned long long uParsed = std::strtoull(szEnv, &pszEnd, 10);
				if (pszEnd != szEnv)
				{
					m_uSeed = static_cast<uint64_t>(uParsed);
				}
			}
		}
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
		DPProcLevel::GenConfig xConfig;  // defaults from header
		// Override the wall half-thickness so total thickness (2*half =
		// 0.8 m) exceeds the navmesh voxelizer's cell width (~0.36 m
		// for a 100x100m level). The GenConfig default of 0.15
		// (0.30 m total) is thinner than one navmesh cell, so the
		// Recast-style voxelizer was missing walls -- the priest's
		// NavMeshAgent would happily route a path through them and
		// the SetPosition writes literally teleported the priest
		// through walls (reported 2026-05-19).
		xConfig.fWallHalfThickness = 0.4f;
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

		// P4c: instantiate the gameplay elements (pentagram, forge,
		// door, chest, noise machine, iron + 5 objective pickups).
		// SpawnPoint elements are skipped here -- they're consumed
		// by SpawnVillagers (P4d) as the player-first-possession
		// anchor, not entities in their own right.
		SpawnGameElements();

		// P4d: AI agents -- 17 villagers + 1 priest. Each is a
		// CAPSULE-collider DYNAMIC body with the proper character
		// script attached. The priest's OnAwake also auto-adds the
		// AIAgent component so behaviour-tree pursuit + perception
		// wire up exactly like the authored GameLevel priest.
		SpawnVillagers();
		SpawnPriest();
	}

	void OnStart() ZENITH_FINAL override
	{
		// Camera auto-framing runs in OnStart rather than OnAwake because
		// DPOrbitCamera_Behaviour::OnAwake initialises its orbit target
		// + distance to authored defaults. If we set them in OnAwake the
		// orbit's own OnAwake (which may fire after ours -- script
		// OnAwake order across entities isn't guaranteed) would clobber
		// our values. OnStart fires after ALL entities' OnAwake calls,
		// so by this point the orbit defaults are stable and our
		// override sticks.
		FrameCameraToLevel();

		// 2026-05-21: lay down the per-scene particle emitter entities
		// (forge sparks, door dust, pentagram ritual swirl, etc) in the
		// persistent scene so subsequent gameplay events can fire bursts.
		// OnStart timing means the persistent scene exists and is ready
		// for entity creation; placement here means we don't need to
		// expose the call from DevilsPlayground.cpp's lifecycle hooks
		// (which fire BEFORE the persistent scene is loaded on some
		// boot paths).
		DP_Particles::EnsureEmittersInScene();
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

	// Count of game-element entities the bootstrap created. Excludes
	// SpawnPoint elements (P4c skips those -- the spawn anchor is
	// consumed by P4d's villager-spawn pass). Useful for the smoke test.
	uint32_t GetSpawnedGameElementCount() const { return m_uSpawnedGameElements; }

	// AI-agent spawn counts (P4d). Villagers always go through the same
	// path; the priest is single-valued so this is bool-shaped.
	uint32_t GetSpawnedVillagerCount() const { return m_uSpawnedVillagers; }
	bool     GetSpawnedPriest()        const { return m_bSpawnedPriest; }

private:
	// Spawn one entity per WallSegment. SM_Cube is a UNIT cube anchored
	// at its (0, 0, 0) corner -- mesh bounds [0, 1]³, NOT the
	// [-1, 1] × [0, 4] × [-1, 1] the previous comment claimed (verified
	// against the .gltf min/max). Two consequences for sizing:
	//
	// 1. Total mesh extent along each axis = 1 unit, so to get a wall of
	//    world half-extents (hx, _, hz) we scale by (2*hx, sy, 2*hz).
	//    The earlier code scaled by (hx, _, hz), producing walls half
	//    the correct length -- reported 2026-05-18.
	//
	// 2. The mesh is corner-anchored, NOT centre-anchored. The visible
	//    mesh extends from entity position toward +X / +Y / +Z. So a
	//    wall positioned at the LAYOUT'S centre would render offset by
	//    (+hx, _, +hz) in world space (or rotated by yaw). To put the
	//    wall's geometric centre at (cx, _, cz), we offset the entity
	//    position by R(yaw) * (-hx, 0, -hz) -- the rotated vector from
	//    centre to the min corner of the wall in wall-local space.
	//
	// Wall Y: floor occupies y=[0, 1] (matches GameLevel's SM_Floor).
	// Walls sit on top of the floor at y=1, scale.y=4 so they span
	// y=[1, 5]. The bot's pathfinder raycasts from y=10 downward and
	// treats anything > y=1.5 as a "tall obstacle" -- walls comfortably
	// clear that threshold.
	//
	// Rotation convention (visualiser's R_y, PR #95):
	//   local (lx, lz) -> world (lx*cos + lz*sin, -lx*sin + lz*cos)
	// So the world offset from wall centre to mesh-corner (-hx, 0, -hz):
	//   wx_offset = -hx*cos - hz*sin
	//   wz_offset =  hx*sin - hz*cos
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

			if (xEntity.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_TransformComponent& xT = xEntity.GetComponent<Zenith_TransformComponent>();
				// Corner-offset compensates for the mesh's [0, 1]³
				// corner anchoring so the wall ends up centred on
				// (xW.fCentreX, _, xW.fCentreZ) after rotation.
				const float fCosY = std::cos(xW.fYawRadians);
				const float fSinY = std::sin(xW.fYawRadians);
				const float fOffsetX = -xW.fHalfExtentX * fCosY - xW.fHalfExtentZ * fSinY;
				const float fOffsetZ =  xW.fHalfExtentX * fSinY - xW.fHalfExtentZ * fCosY;
				xT.SetPosition(Zenith_Maths::Vector3(
					xW.fCentreX + fOffsetX,
					1.0f,  // sit on top of the floor (floor spans y=[0, 1])
					xW.fCentreZ + fOffsetZ));
				const Zenith_Maths::Quat xRot =
					Zenith_Maths::AngleAxis(xW.fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
				xT.SetRotation(xRot);
				// Scale = full wall dimensions: 2*hx along local X,
				// 4 m tall, 2*hz along local Z.
				xT.SetScale(Zenith_Maths::Vector3(
					2.0f * xW.fHalfExtentX,
					4.0f,
					2.0f * xW.fHalfExtentZ));
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

	// Spawn one entity per GameElement. Each element type maps to a
	// behaviour script + a collider profile that mirrors the GameLevel
	// authoring patterns in DevilsPlayground.cpp:
	//   * Pentagram: AABB collider, scaled 2x for "ritual disc" look,
	//     y=1 anchor (matching the authored pentagram).
	//   * Door: OBB collider so the navmesh blocker registers correctly
	//     on the wall-aligned door footprint.
	//   * Chest: AABB collider (mirrors AuthorPlacementBatch's default).
	//   * Forge, NoiseMachine: no collider -- the F-press interaction
	//     reads transform position directly via HandleInteract; the
	//     collider would only be needed for click-to-interact raycasts
	//     which the GameLevel forge/noise authoring also omits.
	//   * Iron + Objective1..5: sphere collider, DPItemBase + SetTag.
	//     Matches DPItemManager_Behaviour::SpawnItemEntity exactly so
	//     pickup + tinting work identically to authored-spawn items.
	//
	// SpawnPoint elements are intentionally skipped here -- the
	// villager-spawn pass (P4d) consumes the SpawnPoint as the first
	// possessable villager's anchor, not as a free-standing entity.
	void SpawnGameElements()
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) return;

		const std::string strMeshPath =
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" + ZENITH_MODEL_EXT;

		uint32_t uSpawned = 0;
		uint32_t uSkipped = 0;
		const uint32_t uN = m_xLayout.axGameElements.GetSize();
		for (uint32_t i = 0; i < uN; ++i)
		{
			const DPProcLevel::GameElement& xElem = m_xLayout.axGameElements.Get(i);

			if (xElem.eType == DPProcLevel::GameElementType::SpawnPoint)
			{
				++uSkipped;
				continue;
			}

			char szName[64];
			std::snprintf(szName, sizeof(szName), "ProcElem_%u_%s",
				i, GameElementTypeToShortName(xElem.eType));
			Zenith_Entity xEntity(pxScene, std::string(szName));
			if (!xEntity.IsValid()) continue;

			// Transform: world XZ from element, y per-type (most live at
			// y=0 with the cube anchored to the floor; pentagram floats
			// to y=1 + 0.5y scale so it reads as a flat ritual disc).
			if (xEntity.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_TransformComponent& xT = xEntity.GetComponent<Zenith_TransformComponent>();
				if (xElem.eType == DPProcLevel::GameElementType::Pentagram)
				{
					xT.SetPosition(Zenith_Maths::Vector3(xElem.fX, 1.0f, xElem.fZ));
					xT.SetScale(Zenith_Maths::Vector3(2.0f, 0.5f, 2.0f));
				}
				else
				{
					xT.SetPosition(Zenith_Maths::Vector3(xElem.fX, 0.0f, xElem.fZ));
				}
			}

			Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
			xModel.LoadModel(strMeshPath);

			switch (xElem.eType)
			{
			case DPProcLevel::GameElementType::Pentagram:
			{
				Zenith_ColliderComponent& xCol = xEntity.AddComponent<Zenith_ColliderComponent>();
				xCol.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
				// 2026-05-22: same rationale as the Iron / Objective items
				// below -- exclude from navmesh so the bot can path TO the
				// pentagram for delivery. Pentagram is a wide ritual disc
				// (2x2 footprint at y=1.0); leaving its collider in the
				// navmesh creates a hole the bot can't path to. F-press
				// pickup-delivery is proximity-based (1.5 m), not collision-
				// based.
				xCol.SetIncludeInNavMesh(false);
				xEntity.AddComponent<Zenith_ScriptComponent>()
					.AddScript<DPPentagram_Behaviour>();
				break;
			}
			case DPProcLevel::GameElementType::Forge:
			{
				xEntity.AddComponent<Zenith_ScriptComponent>()
					.AddScript<DPForge_Behaviour>();
				break;
			}
			case DPProcLevel::GameElementType::Door:
			{
				// 2026-05-22: previously the door spawned with default
				// scale (1,1,1) at y=0 -- a 1 m cube at floor level. That
				// failed to actually block movement because:
				//   * Bot's path-grid raycast threshold is `hit.y < 1.5 m`
				//     => walkable; the 1 m-tall door's top sits at y=1.0
				//     and is invisible to the bot's pathfinder.
				//   * The 1 m-wide door only occupied half the 2 m wall
				//     gap (fDoorGapHalfWidth=1.0), leaving 0.5 m of open
				//     space on each side that a 0.5 m-radius villager
				//     capsule could squeeze through.
				// Result: keyless bots (Zealot, Relay) walked straight
				// through "locked" doors -- the priest was the only AI
				// that respected them, because the priest uses navmesh
				// pathing and DPDoor toggles a navmesh BLOCKED flag
				// independently of the physics collider.
				//
				// Fix: spawn the door with the same y=1 floor offset and
				// wall-tall (4 m) height as walls, scaled (0.3, 4, 2) so
				// the collider's wide axis fills the 2 m wall gap. The
				// element yaw (set by the generator from the corridor's
				// perpendicular axis) rotates the door so its wide axis
				// lies ALONG the wall. The same corner-anchor offset
				// SpawnWalls uses applies here because the SM_Cube mesh
				// is corner-anchored, not centre-anchored.
				//
				// When the door opens (DPDoor::ApplyRotation rotates the
				// entity 90° around Y), Zenith_TransformComponent::SetRotation
				// propagates to the Jolt body via SetRotation(EActivation::
				// Activate) -- the collider rotates in sync with the mesh
				// and the corridor gap opens. No RebuildCollider needed.
				if (xEntity.HasComponent<Zenith_TransformComponent>())
				{
					Zenith_TransformComponent& xT = xEntity.GetComponent<Zenith_TransformComponent>();
					// Door dimensions: thin (0.3 m), wall-tall (4 m), and
					// 2 m wide along the wall (matches the 2 m wall gap).
					constexpr float fHalfThick = 0.15f; // along local +X
					constexpr float fHalfWide  = 1.0f;  // along local +Z
					const float fCosY = std::cos(xElem.fYawRadians);
					const float fSinY = std::sin(xElem.fYawRadians);
					// Same corner->centre compensation SpawnWalls uses:
					// the mesh extends from entity origin to +scale; offset
					// the entity by R(yaw) * (-halfX, 0, -halfZ) so the
					// mesh's geometric centre lands on (fX, _, fZ).
					const float fOffsetX = -fHalfThick * fCosY - fHalfWide * fSinY;
					const float fOffsetZ =  fHalfThick * fSinY - fHalfWide * fCosY;
					xT.SetPosition(Zenith_Maths::Vector3(
						xElem.fX + fOffsetX,
						1.0f,  // sit on top of the floor (matches walls)
						xElem.fZ + fOffsetZ));
					xT.SetRotation(Zenith_Maths::AngleAxis(
						xElem.fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
					xT.SetScale(Zenith_Maths::Vector3(
						2.0f * fHalfThick,  // 0.3 m thick door panel
						4.0f,               // wall-tall: matches SpawnWalls
						2.0f * fHalfWide)); // 2 m wide: fills the wall gap
				}
				xEntity.AddComponent<Zenith_ColliderComponent>()
					.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
				xEntity.AddComponent<Zenith_ScriptComponent>()
					.AddScript<DPDoor_Behaviour>();
				break;
			}
			case DPProcLevel::GameElementType::Chest:
			{
				Zenith_ColliderComponent& xCol = xEntity.AddComponent<Zenith_ColliderComponent>();
				xCol.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
				// 2026-05-22: navmesh-exclude (same rationale as the
				// pentagram + items above). Bot walks to the chest to
				// F-press; its collider should not gate routing.
				xCol.SetIncludeInNavMesh(false);
				xEntity.AddComponent<Zenith_ScriptComponent>()
					.AddScript<DPChest_Behaviour>();
				break;
			}
			case DPProcLevel::GameElementType::NoiseMachine:
			{
				xEntity.AddComponent<Zenith_ScriptComponent>()
					.AddScript<DummyNoiseMachine_Behaviour>();
				break;
			}
			case DPProcLevel::GameElementType::Iron:
			case DPProcLevel::GameElementType::Objective1:
			case DPProcLevel::GameElementType::Objective2:
			case DPProcLevel::GameElementType::Objective3:
			case DPProcLevel::GameElementType::Objective4:
			case DPProcLevel::GameElementType::Objective5:
			{
				Zenith_ColliderComponent& xCol = xEntity.AddComponent<Zenith_ColliderComponent>();
				xCol.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_STATIC);
				// 2026-05-22: exclude pickups from navmesh generation. The
				// navmesh-based pathfinder needs to ROUTE TO these items
				// to pick them up, but a static sphere collider at the
				// item position carves a HOLE in the navmesh at exactly
				// that position -- FindPath then reports "End position not
				// on navmesh" because the item's XZ is in a non-walkable
				// gap surrounded by polygons. Pickup is proximity-based
				// (1.5 m), not collision-based, so the item's collider
				// doesn't need to block navigation -- only the visual mesh
				// + pickup-distance check matter for gameplay. The doors
				// use this same SetIncludeInNavMesh(false) pattern; this
				// extends it to all pickups.
				xCol.SetIncludeInNavMesh(false);
				DPItemBase_Behaviour* pxItem = xEntity
					.AddComponent<Zenith_ScriptComponent>()
					.AddScript<DPItemBase_Behaviour>();
				if (pxItem != nullptr)
				{
					pxItem->SetTag(GameElementToItemTag(xElem.eType));
				}
				break;
			}
			case DPProcLevel::GameElementType::SpawnPoint:
				// Already handled above (continue). Listed here so the
				// switch is exhaustive and a future GameElementType
				// addition forces a deliberate decision.
				break;
			}

			++uSpawned;
		}
		m_uSpawnedGameElements = uSpawned;
		std::printf("[DPProcLevelBootstrap] spawned %u game-element entities "
			"(skipped %u spawn-points, total elements=%u)\n",
			uSpawned, uSkipped, uN);
		std::fflush(stdout);
	}

	static const char* GameElementTypeToShortName(DPProcLevel::GameElementType eType)
	{
		using T = DPProcLevel::GameElementType;
		switch (eType)
		{
		case T::SpawnPoint:   return "Spawn";
		case T::Pentagram:    return "Pentagram";
		case T::Forge:        return "Forge";
		case T::Door:         return "Door";
		case T::Chest:        return "Chest";
		case T::NoiseMachine: return "Noise";
		case T::Iron:         return "Iron";
		case T::Objective1:   return "Obj1";
		case T::Objective2:   return "Obj2";
		case T::Objective3:   return "Obj3";
		case T::Objective4:   return "Obj4";
		case T::Objective5:   return "Obj5";
		}
		return "Unknown";
	}

	static DP_ItemTag GameElementToItemTag(DPProcLevel::GameElementType eType)
	{
		using T = DPProcLevel::GameElementType;
		switch (eType)
		{
		case T::Iron:       return DP_ItemTag::Iron;
		case T::Objective1: return DP_ItemTag::Objective1;
		case T::Objective2: return DP_ItemTag::Objective2;
		case T::Objective3: return DP_ItemTag::Objective3;
		case T::Objective4: return DP_ItemTag::Objective4;
		case T::Objective5: return DP_ItemTag::Objective5;
		default:            return DP_ItemTag::None;
		}
	}

	// Auto-frame the orbit camera so the entire generated level fits in
	// the view frustum regardless of the level's bounds. Called from
	// OnStart -- by that point DPOrbitCamera_Behaviour::OnAwake has
	// installed its authored defaults, which we override here.
	//
	// Math derivation (constant pitch + FOV from DPOrbitCamera_Behaviour
	// defaults: pitch = 1.20 rad ≈ 68.75° down, vertical FOV = 55°):
	//
	// Camera offset from target at orbit distance d:
	//   (-cos(α)*d, sin(α)*d, 0)        (for yaw_eff = 3π/2)
	// Lower frustum edge points at angle α + θv/2 = 96.25° down from
	// horizontal (i.e., past straight down -- it covers ground BEHIND
	// the camera relative to the look direction).
	//
	// Ground intersection of the lower edge in world X:
	//   x_near = cx + d * (-cos(α) + sin(α)/tan(α + θv/2))
	//          = cx + d * (-0.362 - 0.103)
	//          = cx - 0.465 * d
	//
	// For the level's near edge (cx - W/2) to be inside the frustum:
	//   cx - 0.465d <= cx - W/2
	//   d >= W / (2 * 0.465)  ≈  1.075 * W
	//
	// The "near edge" is the binding constraint -- the upper frustum
	// edge reaches well past the far edge of the level even at the
	// minimum distance. With a 20% safety margin and using max(W, D)
	// (so the level still fits when the player rotates Q/E and the
	// camera-near axis switches), the formula collapses to:
	//   d = max(W, D) * 1.075 * 1.20  ≈  1.29 * max(W, D)
	//
	// Bumping the orbit's m_fMaxDistance prevents the SetOrbitDistance
	// clamp from silently capping the computed distance on big levels.
	void FrameCameraToLevel()
	{
		DPOrbitCamera_Behaviour* pxOrbit = nullptr;
		DP_Query::ForEachScriptInActiveScene<DPOrbitCamera_Behaviour>(
			[&pxOrbit](Zenith_EntityID, DPOrbitCamera_Behaviour& xOrbit)
			{
				pxOrbit = &xOrbit;
			});
		if (pxOrbit == nullptr)
		{
			std::printf("[DPProcLevelBootstrap] FrameCameraToLevel: "
				"no DPOrbitCamera_Behaviour found, skipping\n");
			std::fflush(stdout);
			return;
		}

		const float fBoundsW = m_xLayout.fBoundsMaxX - m_xLayout.fBoundsMinX;
		const float fBoundsD = m_xLayout.fBoundsMaxZ - m_xLayout.fBoundsMinZ;
		const float fCentreX = (m_xLayout.fBoundsMinX + m_xLayout.fBoundsMaxX) * 0.5f;
		const float fCentreZ = (m_xLayout.fBoundsMinZ + m_xLayout.fBoundsMaxZ) * 0.5f;

		constexpr float kFitFactor = 1.075f;  // derived from pitch + FOV (see header)
		constexpr float kMargin    = 1.20f;   // 20% padding around the level edge
		const float fLevelExtent  = std::max(fBoundsW, fBoundsD);
		const float fOrbitDistance = kFitFactor * fLevelExtent * kMargin;

		pxOrbit->SetMaxOrbitDistance(std::max(150.0f, fOrbitDistance * 1.10f));
		pxOrbit->SetOrbitTarget(Zenith_Maths::Vector3(fCentreX, 0.0f, fCentreZ));
		pxOrbit->SetOrbitDistance(fOrbitDistance);

		std::printf("[DPProcLevelBootstrap] camera framed: "
			"target=(%.1f, 0, %.1f) distance=%.1f m (level %.1f x %.1f m)\n",
			fCentreX, fCentreZ, fOrbitDistance, fBoundsW, fBoundsD);
		std::fflush(stdout);
	}

	// Spawn 17 villagers + 1 priest from the layout. Both use SM_Cube as
	// a placeholder mesh rather than the UE-imported SM_Peasant /
	// SM_Pope. Reason: even with the 12x scale baked into those .gltfs
	// the resulting mesh is only ~0.2 m wide × 0.15 m tall, so at the
	// procgen orbit camera's 129 m distance characters render as
	// sub-pixel specks -- effectively invisible (reported 2026-05-19).
	// SM_Cube gives a clearly visible humanoid-sized placeholder. Real
	// character meshes can be reinstated once the export pipeline has
	// produced rigid (non-12x-baked) versions, or per-mesh transform
	// scales are tuned to undo whatever scaling the import did.
	void SpawnVillagers()
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) return;

		const std::string strMeshPath = GetCubeMeshPath();

		const uint32_t uN = m_xLayout.axVillagerSpawns.GetSize();
		uint32_t uSpawned = 0;
		for (uint32_t i = 0; i < uN; ++i)
		{
			const DPProcLevel::VillagerSpawn& xV = m_xLayout.axVillagerSpawns.Get(i);
			if (SpawnCharacterEntity(pxScene, "ProcVillager", i, xV.fX, xV.fZ,
				xV.fYawRadians, strMeshPath, /*bIsPriest=*/false))
			{
				++uSpawned;
			}
		}
		m_uSpawnedVillagers = uSpawned;
		std::printf("[DPProcLevelBootstrap] spawned %u/%u villager entities\n",
			uSpawned, uN);
		std::fflush(stdout);
	}

	// Spawn the single priest. Same SM_Cube placeholder path as
	// villagers but scaled bigger (1.5 × 3 × 0.75 m) so the priest is
	// visibly distinct from a wandering villager. Priest_Behaviour's
	// OnAwake auto-adds the AIAgent component so behaviour-tree +
	// perception wire up identically to the authored GameLevel priest.
	void SpawnPriest()
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) return;
		if (!m_xLayout.xPriestSpawn.bValid) return;

		const std::string strMeshPath = GetCubeMeshPath();

		const DPProcLevel::PriestSpawn& xP = m_xLayout.xPriestSpawn;
		m_bSpawnedPriest = SpawnCharacterEntity(pxScene, "ProcPriest", 0,
			xP.fX, xP.fZ, xP.fYawRadians, strMeshPath, /*bIsPriest=*/true);
		std::printf("[DPProcLevelBootstrap] spawned priest=%d\n",
			static_cast<int>(m_bSpawnedPriest));
		std::fflush(stdout);
	}

	static std::string GetCubeMeshPath()
	{
		return std::string(GAME_ASSETS_DIR)
			+ "Meshes/LevelPrototyping_Meshes_SM_Cube"
			+ ZENITH_MODEL_EXT;
	}

	// Shared character-spawn helper. Returns true on success.
	//
	// SM_Cube is corner-anchored at (0, 0, 0) → (1, 1, 1) in mesh local
	// space. To get the visual + capsule collider CENTRED on the
	// layout's (fX, _, fZ), the entity position must be offset by
	// -R(yaw) * (sx/2, _, sz/2) -- the same corner-compensation pattern
	// SpawnWalls uses. Without this offset, characters render up to
	// (sx/2, _, sz/2) away from where the bot's pathfinder + ai
	// proximity queries expect them, which made the bot appear to be
	// "trying to walk through walls" in 2026-05-19 testing (the path
	// targeted a wall-overlapping centre while the visible cube was
	// offset into the adjacent room).
	//
	// Visualiser's R_y convention (PR #95):
	//   local (lx, lz) -> world (lx*cos + lz*sin, -lx*sin + lz*cos)
	// World offset from layout centre to mesh corner (-sx/2, -sz/2):
	//   wx = -sx/2*cos - sz/2*sin
	//   wz =  sx/2*sin - sz/2*cos
	//
	// Y-anchor 1.0 (floor top) so the cube's bottom sits on the floor
	// (which spans y=[0, 1] from the PR #109 floor fix).
	//
	// Physics setup after AddCollider mirrors what DPVillager_Behaviour
	// and Priest_Behaviour do in their own OnAwake hooks, but we apply
	// it INLINE here too because:
	//   1. There's an apparent timing window where the script's OnAwake
	//      sees HasValidBody() == false (the body has been created but
	//      Jolt's activate hasn't run yet), causing the lock + gravity
	//      disable to silently skip. Doing it here, immediately after
	//      AddCollider, sidesteps that race.
	//   2. The user observed "the cubes are rotating" on 2026-05-19,
	//      which is exactly the symptom of the rotation lock not
	//      taking effect.
	// LockRotation(X=true, Y=false, Z=true) keeps yaw free but freezes
	// pitch + roll so glancing wall hits can't tip the capsule over.
	//
	// Villager cube: 1 × 2 × 0.5 m. Priest cube: 1.5 × 3 × 0.75 m
	// (50% larger so priest is visually distinguishable from villagers).
	bool SpawnCharacterEntity(
		Zenith_SceneData* pxScene,
		const char* szPrefix,
		uint32_t uIndex,
		float fX,
		float fZ,
		float fYawRadians,
		const std::string& strMeshPath,
		bool bIsPriest)
	{
		char szName[64];
		std::snprintf(szName, sizeof(szName), "%s_%u", szPrefix, uIndex);
		Zenith_Entity xEntity(pxScene, std::string(szName));
		if (!xEntity.IsValid()) return false;

		const Zenith_Maths::Vector3 xScale = bIsPriest
			? Zenith_Maths::Vector3(1.5f, 3.0f, 0.75f)
			: Zenith_Maths::Vector3(1.0f, 2.0f, 0.5f);

		if (xEntity.HasComponent<Zenith_TransformComponent>())
		{
			Zenith_TransformComponent& xT =
				xEntity.GetComponent<Zenith_TransformComponent>();

			// Corner-offset: rotate (-sx/2, 0, -sz/2) by yaw and add
			// to the layout's (fX, _, fZ). End result: cube CENTRE is
			// at (fX, _, fZ) instead of the cube CORNER landing there.
			const float fCosY = std::cos(fYawRadians);
			const float fSinY = std::sin(fYawRadians);
			const float fHalfSx = xScale.x * 0.5f;
			const float fHalfSz = xScale.z * 0.5f;
			const float fOffsetX = -fHalfSx * fCosY - fHalfSz * fSinY;
			const float fOffsetZ =  fHalfSx * fSinY - fHalfSz * fCosY;

			xT.SetPosition(Zenith_Maths::Vector3(
				fX + fOffsetX, 1.0f, fZ + fOffsetZ));
			const Zenith_Maths::Quat xRot = Zenith_Maths::AngleAxis(
				fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			xT.SetRotation(xRot);
			xT.SetScale(xScale);
		}

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.LoadModel(strMeshPath);

		Zenith_ColliderComponent& xCol = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);

		// Explicit physics setup (see method header). This is belt-and-
		// braces against the DPVillager_Behaviour / Priest_Behaviour
		// HasValidBody() check failing on the OnAwake immediately after
		// AddScript fires.
		//
		// LockRotation differs from the GameLevel character pattern:
		// GameLevel locks X+Z but leaves Y (yaw) free so character
		// meshes can face their movement direction. For ProcLevel's
		// SM_Cube placeholders, leaving Y free makes the cube SWING
		// around its corner anchor every time the script updates yaw
		// (the corner-offset compensation in this method's transform
		// setup only holds for the SPAWN yaw -- runtime yaw changes
		// reveal the offset). User reported "the priest and villagers
		// are still rotating" on 2026-05-19; locking yaw too makes the
		// cubes statically translate through the level which is the
		// correct visual for placeholders.
		if (xCol.HasValidBody())
		{
			const JPH::BodyID& xBodyID = xCol.GetBodyID();
			Zenith_Physics::SetGravityEnabled(xBodyID, false);
			Zenith_Physics::LockRotation(xBodyID, /*X=*/true, /*Y=*/true, /*Z=*/true);
		}

		if (bIsPriest)
		{
			xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<Priest_Behaviour>();
		}
		else
		{
			xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DPVillager_Behaviour>();
		}

		return true;
	}

	uint64_t                  m_uSeed = 0ull;
	uint32_t                  m_uSpawnedWalls = 0u;
	uint32_t                  m_uSpawnedGameElements = 0u;
	uint32_t                  m_uSpawnedVillagers = 0u;
	bool                      m_bSpawnedPriest = false;
	DPProcLevel::LevelLayout  m_xLayout;

	static inline DPProcLevelBootstrap_Behaviour* s_pxInstance = nullptr;
};
