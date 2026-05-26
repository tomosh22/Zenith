#include "Zenith.h"

#include "Components/DPProcLevelBootstrap_Behaviour.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "Physics/Zenith_PhysicsImpl.h"
#include "Maths/Zenith_Maths.h"

#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_AI.h"
#include "Source/DP_Tuning.h"
#include "Source/DPMaterials.h"
#include "Source/DPParticles.h"

#include "Components/DPPentagram_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/DummyNoiseMachine_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPOrbitCamera_Behaviour.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

void DPProcLevelBootstrap_Behaviour::OnAwake()
{
	s_pxInstance = this;

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)
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
	DPProcLevel::GenConfig xConfig;
	xConfig.fWallHalfThickness = 0.4f;
	const float fRawLockedFrac = DP_Tuning::Get<float>("interactables.door_locked_fraction");
	xConfig.fDoorLockedFraction = glm::clamp(fRawLockedFrac, 0.0f, 1.0f);
	const bool bOk = DPProcLevel::Generate(m_uSeed, xConfig, m_xLayout);

	{
		Zenith_Vector<Zenith_Maths::Vector3> axPatrolPositions;
		const uint32_t uPatrolN = m_xLayout.axPatrolNodes.GetSize();
		for (uint32_t i = 0; i < uPatrolN; ++i)
		{
			const DPProcLevel::PatrolNode& xP = m_xLayout.axPatrolNodes.Get(i);
			axPatrolPositions.PushBack(
				Zenith_Maths::Vector3(xP.fX, 2.5f, xP.fZ));
		}
		DP_AI::SetPatrolNodes(axPatrolPositions);
	}

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] seed=%llu generated=%d "
		"rooms=%u walls=%u elements=%u villagers=%u patrol=%u priest=%d",
		static_cast<unsigned long long>(m_uSeed),
		static_cast<int>(bOk),
		m_xLayout.axRooms.GetSize(),
		m_xLayout.axWallSegments.GetSize(),
		m_xLayout.axGameElements.GetSize(),
		m_xLayout.axVillagerSpawns.GetSize(),
		m_xLayout.axPatrolNodes.GetSize(),
		static_cast<int>(m_xLayout.xPriestSpawn.bValid));

	SpawnWalls();
	SpawnGameElements();
	SpawnVillagers();
	SpawnPriest();
}

void DPProcLevelBootstrap_Behaviour::OnStart()
{
	FrameCameraToLevel();
	DP_Particles::EnsureEmittersInScene();
}

void DPProcLevelBootstrap_Behaviour::OnDestroy()
{
	if (s_pxInstance == this) s_pxInstance = nullptr;
}

void DPProcLevelBootstrap_Behaviour::SpawnWalls()
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
			const float fCosY = std::cos(xW.fYawRadians);
			const float fSinY = std::sin(xW.fYawRadians);
			const float fOffsetX = -xW.fHalfExtentX * fCosY - xW.fHalfExtentZ * fSinY;
			const float fOffsetZ =  xW.fHalfExtentX * fSinY - xW.fHalfExtentZ * fCosY;
			xT.SetPosition(Zenith_Maths::Vector3(
				xW.fCentreX + fOffsetX,
				1.0f,
				xW.fCentreZ + fOffsetZ));
			const Zenith_Maths::Quat xRot =
				Zenith_Maths::AngleAxis(xW.fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			xT.SetRotation(xRot);
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
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] spawned %u/%u wall entities",
		uSpawned, uN);
}

void DPProcLevelBootstrap_Behaviour::SpawnGameElements()
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
			if (xEntity.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_TransformComponent& xT = xEntity.GetComponent<Zenith_TransformComponent>();
				constexpr float fHalfThick = 0.15f;
				constexpr float fHalfWide  = 1.0f;
				const float fCosY = std::cos(xElem.fYawRadians);
				const float fSinY = std::sin(xElem.fYawRadians);
				const float fOffsetX = -fHalfThick * fCosY - fHalfWide * fSinY;
				const float fOffsetZ =  fHalfThick * fSinY - fHalfWide * fCosY;
				xT.SetPosition(Zenith_Maths::Vector3(
					xElem.fX + fOffsetX,
					1.0f,
					xElem.fZ + fOffsetZ));
				xT.SetRotation(Zenith_Maths::AngleAxis(
					xElem.fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
				xT.SetScale(Zenith_Maths::Vector3(
					2.0f * fHalfThick,
					4.0f,
					2.0f * fHalfWide));
			}
			xEntity.AddComponent<Zenith_ColliderComponent>()
				.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

			DPDoor_Behaviour* pxDoor =
				xEntity.AddComponent<Zenith_ScriptComponent>()
					.AddScript<DPDoor_Behaviour>();
			if (pxDoor != nullptr)
			{
				pxDoor->SetLogicalCentre(Zenith_Maths::Vector3(xElem.fX, 1.0f, xElem.fZ));
				pxDoor->SetRequiredKey(xElem.bDoorLocked
					? DP_ItemTag::Key
					: DP_ItemTag::None);
			}
			break;
		}
		case DPProcLevel::GameElementType::Chest:
		{
			Zenith_ColliderComponent& xCol = xEntity.AddComponent<Zenith_ColliderComponent>();
			xCol.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
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
			break;
		}

		++uSpawned;
	}
	m_uSpawnedGameElements = uSpawned;
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] spawned %u game-element entities "
		"(skipped %u spawn-points, total elements=%u)",
		uSpawned, uSkipped, uN);
}

const char* DPProcLevelBootstrap_Behaviour::GameElementTypeToShortName(DPProcLevel::GameElementType eType)
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

DP_ItemTag DPProcLevelBootstrap_Behaviour::GameElementToItemTag(DPProcLevel::GameElementType eType)
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

void DPProcLevelBootstrap_Behaviour::FrameCameraToLevel()
{
	DPOrbitCamera_Behaviour* pxOrbit = nullptr;
	DP_Query::ForEachScriptInActiveScene<DPOrbitCamera_Behaviour>(
		[&pxOrbit](Zenith_EntityID, DPOrbitCamera_Behaviour& xOrbit)
		{
			pxOrbit = &xOrbit;
		});
	if (pxOrbit == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY,
			"[DPProcLevelBootstrap] FrameCameraToLevel: "
			"no DPOrbitCamera_Behaviour found, skipping");
		return;
	}

	const float fBoundsW = m_xLayout.fBoundsMaxX - m_xLayout.fBoundsMinX;
	const float fBoundsD = m_xLayout.fBoundsMaxZ - m_xLayout.fBoundsMinZ;
	const float fCentreX = (m_xLayout.fBoundsMinX + m_xLayout.fBoundsMaxX) * 0.5f;
	const float fCentreZ = (m_xLayout.fBoundsMinZ + m_xLayout.fBoundsMaxZ) * 0.5f;

	constexpr float kFitFactor = 1.075f;
	constexpr float kMargin    = 1.20f;
	const float fLevelExtent  = std::max(fBoundsW, fBoundsD);
	const float fOrbitDistance = kFitFactor * fLevelExtent * kMargin;

	pxOrbit->SetMaxOrbitDistance(std::max(150.0f, fOrbitDistance * 1.10f));
	pxOrbit->SetOrbitTarget(Zenith_Maths::Vector3(fCentreX, 0.0f, fCentreZ));
	pxOrbit->SetOrbitDistance(fOrbitDistance);

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] camera framed: "
		"target=(%.1f, 0, %.1f) distance=%.1f m (level %.1f x %.1f m)",
		fCentreX, fCentreZ, fOrbitDistance, fBoundsW, fBoundsD);
}

void DPProcLevelBootstrap_Behaviour::SpawnVillagers()
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
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] spawned %u/%u villager entities",
		uSpawned, uN);
}

void DPProcLevelBootstrap_Behaviour::SpawnPriest()
{
	Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
	if (pxScene == nullptr) return;
	if (!m_xLayout.xPriestSpawn.bValid) return;

	const std::string strMeshPath = GetCubeMeshPath();

	const DPProcLevel::PriestSpawn& xP = m_xLayout.xPriestSpawn;
	m_bSpawnedPriest = SpawnCharacterEntity(pxScene, "ProcPriest", 0,
		xP.fX, xP.fZ, xP.fYawRadians, strMeshPath, /*bIsPriest=*/true);
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] spawned priest=%d",
		static_cast<int>(m_bSpawnedPriest));
}

std::string DPProcLevelBootstrap_Behaviour::GetCubeMeshPath()
{
	return std::string(GAME_ASSETS_DIR)
		+ "Meshes/LevelPrototyping_Meshes_SM_Cube"
		+ ZENITH_MODEL_EXT;
}

bool DPProcLevelBootstrap_Behaviour::SpawnCharacterEntity(
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
	Zenith_Assert(xModel.GetModelInstance(), "Character has no model instance");

	if (bIsPriest)
	{
		Flux_ModelInstance* pxInst = xModel.GetModelInstance();
		const uint32_t uMatCount = pxInst->GetNumMaterials();
		const Zenith_Maths::Vector3 xRgb{ 0.95f, 0.10f, 0.10f };
		for (uint32_t u = 0; u < uMatCount; ++u)
		{
			Zenith_MaterialAsset* pxBase = pxInst->GetMaterial(u);
			Zenith_MaterialAsset* pxRed = DPMaterials::GetOrCreateColouredVariant(pxBase, xRgb, "Priest");
			pxInst->SetMaterial(u, pxRed);
		}
	}

	Zenith_ColliderComponent& xCol = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCol.AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);

	if (xCol.HasValidBody())
	{
		const JPH::BodyID& xBodyID = xCol.GetBodyID();
		g_xEngine.Physics().SetGravityEnabled(xBodyID, false);
		g_xEngine.Physics().LockRotation(xBodyID, /*X=*/true, /*Y=*/true, /*Z=*/true);
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
