#include "Zenith.h"

#include "Components/DPProcLevelBootstrap_Behaviour.h"

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include "Prefab/Zenith_Prefab.h"
#include "Source/DPResources.h"
#include "Source/DP_Archetypes.h"

#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_AI.h"
#include "Source/DP_Tuning.h"
#include "Source/DPMaterials.h"
#include "Source/DPParticles.h"

// Contract exception (creation/lifecycle, not cross-state reads): the bootstrap
// is the scene's master spawn-factory -- it instantiates one entity per procgen
// layout element using these concrete behaviours. Read cross-queries still go
// through DP_* forwarders. See Components/CLAUDE.md.
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
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

void DPProcLevelBootstrap_Behaviour::OnAwake()
{
	Zenith_Assert(s_pxInstance == nullptr,
		"DPProcLevelBootstrap_Behaviour singleton double-instantiated");
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
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxScene == nullptr) return;

	Zenith_Prefab* pxWallPrefab = DevilsPlayground::Resources().m_xWallPrefab.GetDirect();
	if (pxWallPrefab == nullptr) return;

	uint32_t uSpawned = 0;
	const uint32_t uN = m_xLayout.axWallSegments.GetSize();
	for (uint32_t i = 0; i < uN; ++i)
	{
		const DPProcLevel::WallSegment& xW = m_xLayout.axWallSegments.Get(i);
		char szName[64];
		std::snprintf(szName, sizeof(szName), "ProcWall_%u", i);

		const float fCosY = std::cos(xW.fYawRadians);
		const float fSinY = std::sin(xW.fYawRadians);
		const float fOffsetX = -xW.fHalfExtentX * fCosY - xW.fHalfExtentZ * fSinY;
		const float fOffsetZ =  xW.fHalfExtentX * fSinY - xW.fHalfExtentZ * fCosY;
		const Zenith_Maths::Vector3 xPos(xW.fCentreX + fOffsetX, 1.0f, xW.fCentreZ + fOffsetZ);
		const Zenith_Maths::Quat xRot =
			Zenith_Maths::AngleAxis(xW.fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const Zenith_Maths::Vector3 xScale(2.0f * xW.fHalfExtentX, 4.0f, 2.0f * xW.fHalfExtentZ);

		// Model + OBB collider are baked into the prefab; Instantiate applies the
		// transform and rebuilds the collider body to match.
		Zenith_Entity xEntity = pxWallPrefab->Instantiate(pxScene, std::string(szName), xPos, xRot, xScale);
		if (!xEntity.IsValid()) continue;

		++uSpawned;
	}
	m_uSpawnedWalls = uSpawned;
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] spawned %u/%u wall entities",
		uSpawned, uN);
}

void DPProcLevelBootstrap_Behaviour::SpawnGameElements()
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxScene == nullptr) return;

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

		// Select the per-type prefab and compute its spawn transform. Most
		// elements sit at (fX, 0, fZ) with identity rotation/scale; pentagram and
		// door are special-cased.
		Zenith_Prefab* pxPrefab = nullptr;
		Zenith_Maths::Vector3 xPos(xElem.fX, 0.0f, xElem.fZ);
		Zenith_Maths::Quat xRot(1.0f, 0.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xScale(1.0f, 1.0f, 1.0f);
		const Zenith_Maths::Vector3 xDoorLogicalCentre(xElem.fX, 1.0f, xElem.fZ);

		switch (xElem.eType)
		{
		case DPProcLevel::GameElementType::Pentagram:
			pxPrefab = DevilsPlayground::Resources().m_xPentagramPrefab.GetDirect();
			xPos.y = 1.0f;
			xScale = Zenith_Maths::Vector3(2.0f, 0.5f, 2.0f);
			break;
		case DPProcLevel::GameElementType::Forge:
			pxPrefab = DevilsPlayground::Resources().m_xForgePrefab.GetDirect();
			break;
		case DPProcLevel::GameElementType::Door:
		{
			pxPrefab = DevilsPlayground::Resources().m_xDoorPrefab.GetDirect();
			constexpr float fHalfThick = 0.15f;
			constexpr float fHalfWide  = 1.0f;
			const float fCosY = std::cos(xElem.fYawRadians);
			const float fSinY = std::sin(xElem.fYawRadians);
			const float fOffsetX = -fHalfThick * fCosY - fHalfWide * fSinY;
			const float fOffsetZ =  fHalfThick * fSinY - fHalfWide * fCosY;
			xPos = Zenith_Maths::Vector3(xElem.fX + fOffsetX, 1.0f, xElem.fZ + fOffsetZ);
			xRot = Zenith_Maths::AngleAxis(xElem.fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			xScale = Zenith_Maths::Vector3(2.0f * fHalfThick, 4.0f, 2.0f * fHalfWide);
			break;
		}
		case DPProcLevel::GameElementType::Chest:
			pxPrefab = DevilsPlayground::Resources().m_xChestPrefab.GetDirect();
			break;
		case DPProcLevel::GameElementType::NoiseMachine:
			pxPrefab = DevilsPlayground::Resources().m_xNoiseMachinePrefab.GetDirect();
			break;
		case DPProcLevel::GameElementType::Iron:
		case DPProcLevel::GameElementType::Objective1:
		case DPProcLevel::GameElementType::Objective2:
		case DPProcLevel::GameElementType::Objective3:
		case DPProcLevel::GameElementType::Objective4:
		case DPProcLevel::GameElementType::Objective5:
			pxPrefab = DevilsPlayground::Resources().m_xItemPrefab.GetDirect();
			break;
		case DPProcLevel::GameElementType::SpawnPoint:
			break; // already skipped above
		}

		if (pxPrefab == nullptr) continue;

		// Model + (type-specific) collider are baked into the prefab; Instantiate
		// applies the transform and rebuilds any collider body to match.
		Zenith_Entity xEntity = pxPrefab->Instantiate(pxScene, std::string(szName), xPos, xRot, xScale);
		if (!xEntity.IsValid()) continue;

		// Per-instance config + scripts stay post-instantiation: navmesh flags
		// aren't serialized into the prefab, and scripts need their per-instance
		// setters after creation (see Source/DPResources.h).
		switch (xElem.eType)
		{
		case DPProcLevel::GameElementType::Pentagram:
			if (xEntity.HasComponent<Zenith_ColliderComponent>())
				xEntity.GetComponent<Zenith_ColliderComponent>().SetIncludeInNavMesh(false);
			xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DPPentagram_Behaviour>();
			break;
		case DPProcLevel::GameElementType::Forge:
			xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DPForge_Behaviour>();
			break;
		case DPProcLevel::GameElementType::Door:
		{
			// DPDoor_Behaviour::OnAwake sets SetIncludeInNavMesh(false) on its own
			// collider, so it isn't done here.
			DPDoor_Behaviour* pxDoor =
				xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DPDoor_Behaviour>();
			if (pxDoor != nullptr)
			{
				pxDoor->SetLogicalCentre(xDoorLogicalCentre);
				pxDoor->SetRequiredKey(xElem.bDoorLocked ? DP_ItemTag::Key : DP_ItemTag::None);
			}
			break;
		}
		case DPProcLevel::GameElementType::Chest:
			if (xEntity.HasComponent<Zenith_ColliderComponent>())
				xEntity.GetComponent<Zenith_ColliderComponent>().SetIncludeInNavMesh(false);
			xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DPChest_Behaviour>();
			break;
		case DPProcLevel::GameElementType::NoiseMachine:
			xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DummyNoiseMachine_Behaviour>();
			break;
		case DPProcLevel::GameElementType::Iron:
		case DPProcLevel::GameElementType::Objective1:
		case DPProcLevel::GameElementType::Objective2:
		case DPProcLevel::GameElementType::Objective3:
		case DPProcLevel::GameElementType::Objective4:
		case DPProcLevel::GameElementType::Objective5:
		{
			if (xEntity.HasComponent<Zenith_ColliderComponent>())
				xEntity.GetComponent<Zenith_ColliderComponent>().SetIncludeInNavMesh(false);
			DPItemBase_Behaviour* pxItem =
				xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DPItemBase_Behaviour>();
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

void DPProcLevelBootstrap_Behaviour::BuildVillagerArchetypeAssignment(
	uint32_t uN, uint64_t uSeed, Zenith_Vector<const char*>& aszOut)
{
	aszOut.Clear();
	if (uN == 0u) return;

	// Reserve min_spawns of each non-Farmhand MVP archetype so all four MVP
	// archetypes appear; Farmhand fills the remainder. Degrades gracefully to
	// all-Farmhand if DP_Archetypes wasn't initialised.
	for (size_t u = 0; u < DP_Archetypes::Count() && aszOut.GetSize() < uN; ++u)
	{
		const DP_Archetypes::Archetype* pxA = DP_Archetypes::GetByIndex(u);
		if (pxA == nullptr || !pxA->mvp) continue;
		if (pxA->id == "Farmhand") continue;
		const int iCount = (pxA->min_spawns > 0) ? pxA->min_spawns : 1;
		for (int k = 0; k < iCount && aszOut.GetSize() < uN; ++k)
		{
			aszOut.PushBack(pxA->id.c_str());  // stable: cache lives until Shutdown
		}
	}
	while (aszOut.GetSize() < uN) aszOut.PushBack("Farmhand");

	// Deterministic Fisher-Yates shuffle (splitmix64 seeded by the procgen
	// seed, salted to decorrelate from the layout RNG) so specials spread
	// spatially and vary per seed. Integer-only -> bit-reproducible.
	uint64_t z = uSeed ^ 0xA11CE5571CECAFEFull;
	auto Next = [&z]() -> uint64_t
	{
		z += 0x9e3779b97f4a7c15ull;
		uint64_t v = z;
		v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ull;
		v = (v ^ (v >> 27)) * 0x94d049bb133111ebull;
		return v ^ (v >> 31);
	};
	for (uint32_t i = aszOut.GetSize(); i > 1u; --i)
	{
		const uint32_t j = static_cast<uint32_t>(Next() % static_cast<uint64_t>(i));
		const char* szTmp        = aszOut.Get(i - 1u);
		aszOut.Get(i - 1u)       = aszOut.Get(j);
		aszOut.Get(j)            = szTmp;
	}

	// Visibility: log the archetype distribution (confirms #9 is active and
	// shows the per-seed mix).
	int iFarm = 0, iBeg = 0, iDev = 0, iChild = 0, iOther = 0;
	for (uint32_t i = 0; i < aszOut.GetSize(); ++i)
	{
		const char* s = aszOut.Get(i);
		if      (std::strcmp(s, "Farmhand") == 0) ++iFarm;
		else if (std::strcmp(s, "Beggar")   == 0) ++iBeg;
		else if (std::strcmp(s, "Devout")   == 0) ++iDev;
		else if (std::strcmp(s, "Child")    == 0) ++iChild;
		else                                      ++iOther;
	}
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] villager archetypes (n=%u): Farmhand=%d Beggar=%d Devout=%d Child=%d other=%d",
		aszOut.GetSize(), iFarm, iBeg, iDev, iChild, iOther);
}

void DPProcLevelBootstrap_Behaviour::SpawnVillagers()
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxScene == nullptr) return;

	const uint32_t uN = m_xLayout.axVillagerSpawns.GetSize();

	// #9: deterministic varied archetypes (Farmhand/Beggar/Devout/Child)
	// instead of all-Farmhand. Indexed by villager spawn index.
	Zenith_Vector<const char*> aszArchetype;
	BuildVillagerArchetypeAssignment(uN, m_uSeed, aszArchetype);

	uint32_t uSpawned = 0;
	for (uint32_t i = 0; i < uN; ++i)
	{
		const DPProcLevel::VillagerSpawn& xV = m_xLayout.axVillagerSpawns.Get(i);
		const char* szArch = (i < aszArchetype.GetSize()) ? aszArchetype.Get(i) : "Farmhand";
		if (SpawnCharacterEntity(pxScene, "ProcVillager", i, xV.fX, xV.fZ,
			xV.fYawRadians, /*bIsPriest=*/false, szArch))
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
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxScene == nullptr) return;
	if (!m_xLayout.xPriestSpawn.bValid) return;

	const DPProcLevel::PriestSpawn& xP = m_xLayout.xPriestSpawn;
	m_bSpawnedPriest = SpawnCharacterEntity(pxScene, "ProcPriest", 0,
		xP.fX, xP.fZ, xP.fYawRadians, /*bIsPriest=*/true);
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[DPProcLevelBootstrap] spawned priest=%d",
		static_cast<int>(m_bSpawnedPriest));
}

bool DPProcLevelBootstrap_Behaviour::SpawnCharacterEntity(
	Zenith_SceneData* pxScene,
	const char* szPrefix,
	uint32_t uIndex,
	float fX,
	float fZ,
	float fYawRadians,
	bool bIsPriest,
	const char* szArchetype)
{
	Zenith_Prefab* pxPrefab = bIsPriest
		? DevilsPlayground::Resources().m_xPriestPrefab.GetDirect()
		: DevilsPlayground::Resources().m_xVillagerPrefab.GetDirect();
	if (pxPrefab == nullptr) return false;

	char szName[64];
	std::snprintf(szName, sizeof(szName), "%s_%u", szPrefix, uIndex);

	const Zenith_Maths::Vector3 xScale = bIsPriest
		? Zenith_Maths::Vector3(1.5f, 3.0f, 0.75f)
		: Zenith_Maths::Vector3(1.0f, 2.0f, 0.5f);

	const float fCosY = std::cos(fYawRadians);
	const float fSinY = std::sin(fYawRadians);
	const float fHalfSx = xScale.x * 0.5f;
	const float fHalfSz = xScale.z * 0.5f;
	const float fOffsetX = -fHalfSx * fCosY - fHalfSz * fSinY;
	const float fOffsetZ =  fHalfSx * fSinY - fHalfSz * fCosY;
	const Zenith_Maths::Vector3 xPos(fX + fOffsetX, 1.0f, fZ + fOffsetZ);
	const Zenith_Maths::Quat xRot = Zenith_Maths::AngleAxis(
		fYawRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

	// Model + capsule collider are baked; Instantiate places the entity and
	// rebuilds the body to match the scale/rotation.
	Zenith_Entity xEntity = pxPrefab->Instantiate(pxScene, std::string(szName), xPos, xRot, xScale);
	if (!xEntity.IsValid()) return false;

	// Priest: tint the (per-instance) model red.
	if (bIsPriest && xEntity.HasComponent<Zenith_ModelComponent>())
	{
		Flux_ModelInstance* pxInst = xEntity.GetComponent<Zenith_ModelComponent>().GetModelInstance();
		if (pxInst != nullptr)
		{
			const uint32_t uMatCount = pxInst->GetNumMaterials();
			const Zenith_Maths::Vector3 xRgb{ 0.95f, 0.10f, 0.10f };
			for (uint32_t u = 0; u < uMatCount; ++u)
			{
				Zenith_MaterialAsset* pxBase = pxInst->GetMaterial(u);
				Zenith_MaterialAsset* pxRed = DPMaterials::GetOrCreateColouredVariant(pxBase, xRgb, "Priest");
				pxInst->SetMaterial(u, pxRed);
			}
		}
	}

	// Disable gravity + lock rotation on the (already-final) capsule body.
	if (xEntity.HasComponent<Zenith_ColliderComponent>())
	{
		Zenith_ColliderComponent& xCol = xEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCol.HasValidBody())
		{
			const JPH::BodyID& xBodyID = xCol.GetBodyID();
			g_xEngine.Physics().SetGravityEnabled(xBodyID, false);
			g_xEngine.Physics().LockRotation(xBodyID, /*X=*/true, /*Y=*/true, /*Z=*/true);
		}
	}

	if (bIsPriest)
	{
		xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<Priest_Behaviour>();
	}
	else
	{
		Zenith_ScriptComponent& xScr = xEntity.AddComponent<Zenith_ScriptComponent>();
		xScr.AddScript<DPVillager_Behaviour>();
		// #9: apply the assigned archetype before the villager's OnAwake wave
		// drains (OnAwake re-applies the same id -- idempotent). Without this
		// every procgen villager defaulted to Farmhand.
		if (szArchetype != nullptr)
		{
			if (DPVillager_Behaviour* pxV = xScr.GetScript<DPVillager_Behaviour>())
			{
				pxV->ApplyArchetype(szArchetype);
			}
		}
	}

	return true;
}
