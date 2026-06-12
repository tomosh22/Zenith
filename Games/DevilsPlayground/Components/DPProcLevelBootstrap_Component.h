#pragma once
/**
 * DPProcLevelBootstrap_Component - runs the procgen generator in OnAwake
 * and stashes the resulting LevelLayout for downstream consumption.
 *
 * Attach this component to any entity in the scene. On the scene's first
 * OnAwake pass, Generate() runs with seed = m_uSeed (default 0; the
 * DP_PROCGEN_SEED env var overrides) and the full level is spawned:
 * walls, game elements, villagers, priest.
 *
 * Singleton pattern mirrors DPFogPass_Component / DPItemManager_Component:
 * s_pxInstance is set in OnAwake and cleared in OnDestroy, so entity-spawn
 * code + tests can pull the cached layout via Instance(). The hand-written
 * move operations keep s_pxInstance valid across component-pool relocation.
 *
 * Implementation lives in DPProcLevelBootstrap_Component.cpp -- the header
 * keeps only declarations + small inline accessors so the ~5 translation
 * units that include this header don't have to recompile every spawn
 * helper when only one method body changes.
 */

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"

#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cstdint>
#include <string>
#include <utility>

namespace DPProcLevel
{
	enum class GameElementType : uint8_t;
}

class Zenith_SceneData;

class DPProcLevelBootstrap_Component ZENITH_FINAL
{
public:
	DPProcLevelBootstrap_Component() = delete;
	DPProcLevelBootstrap_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Heap-stability: s_pxInstance points at `this`, and component pools
	// relocate on resize / swap-and-pop / cross-scene transfer. Hand-written
	// moves repoint the singleton at the new address; copies deleted.
	DPProcLevelBootstrap_Component(const DPProcLevelBootstrap_Component&) = delete;
	DPProcLevelBootstrap_Component& operator=(const DPProcLevelBootstrap_Component&) = delete;

	DPProcLevelBootstrap_Component(DPProcLevelBootstrap_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_uSeed(xOther.m_uSeed)
		, m_uSpawnedWalls(xOther.m_uSpawnedWalls)
		, m_uSpawnedGameElements(xOther.m_uSpawnedGameElements)
		, m_uSpawnedVillagers(xOther.m_uSpawnedVillagers)
		, m_bSpawnedPriest(xOther.m_bSpawnedPriest)
		, m_xLayout(std::move(xOther.m_xLayout))
	{
		if (s_pxInstance == &xOther) s_pxInstance = this;
	}

	DPProcLevelBootstrap_Component& operator=(DPProcLevelBootstrap_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity        = xOther.m_xParentEntity;
			m_uSeed                = xOther.m_uSeed;
			m_uSpawnedWalls        = xOther.m_uSpawnedWalls;
			m_uSpawnedGameElements = xOther.m_uSpawnedGameElements;
			m_uSpawnedVillagers    = xOther.m_uSpawnedVillagers;
			m_bSpawnedPriest       = xOther.m_bSpawnedPriest;
			m_xLayout              = std::move(xOther.m_xLayout);
			if (s_pxInstance == &xOther) s_pxInstance = this;
		}
		return *this;
	}

	~DPProcLevelBootstrap_Component()
	{
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	void OnAwake();
	void OnStart();
	void OnDestroy();

	// Component contract: version-only payload (the layout is regenerated
	// from the seed on every scene load).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

	static DPProcLevelBootstrap_Component* Instance() { return s_pxInstance; }

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

	// #9: deterministic per-villager archetype assignment. Honours each MVP
	// archetype's min_spawns (from DP_Archetypes) so all four MVP archetypes
	// appear; Farmhand (the bulk of the body pool) fills the rest; then a
	// splitmix64 shuffle seeded by uSeed spreads the specials spatially and
	// varies them per seed. Public+static so a test can pin it without a scene.
	// The returned const char* point into the DP_Archetypes cache / string
	// literals (stable for the spawn's duration).
	static void BuildVillagerArchetypeAssignment(uint32_t uN, uint64_t uSeed,
		Zenith_Vector<const char*>& aszOut);

private:
	void SpawnWalls();
	void SpawnGameElements();
	void SpawnVillagers();
	void SpawnPriest();
	void FrameCameraToLevel();

	static const char* GameElementTypeToShortName(DPProcLevel::GameElementType eType);
	static DP_ItemTag  GameElementToItemTag(DPProcLevel::GameElementType eType);

	bool SpawnCharacterEntity(
		Zenith_SceneData* pxScene,
		const char* szPrefix,
		uint32_t uIndex,
		float fX,
		float fZ,
		float fYawRadians,
		bool bIsPriest,
		const char* szArchetype = nullptr);

	Zenith_Entity             m_xParentEntity;

	uint64_t                  m_uSeed = 0ull;
	uint32_t                  m_uSpawnedWalls = 0u;
	uint32_t                  m_uSpawnedGameElements = 0u;
	uint32_t                  m_uSpawnedVillagers = 0u;
	bool                      m_bSpawnedPriest = false;
	DPProcLevel::LevelLayout  m_xLayout;

	static inline DPProcLevelBootstrap_Component* s_pxInstance = nullptr;
};
