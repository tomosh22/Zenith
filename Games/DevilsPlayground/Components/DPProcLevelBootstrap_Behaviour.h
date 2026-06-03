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
 *
 * Implementation lives in DPProcLevelBootstrap_Behaviour.cpp -- the header
 * keeps only declarations + small inline accessors so the ~5 translation
 * units that include this header don't have to recompile every spawn
 * helper when only one method body changes.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cstdint>
#include <string>

namespace DPProcLevel
{
	enum class GameElementType : uint8_t;
}

class Zenith_SceneData;

class DPProcLevelBootstrap_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPProcLevelBootstrap_Behaviour)

	DPProcLevelBootstrap_Behaviour() = delete;
	DPProcLevelBootstrap_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override;
	void OnStart() ZENITH_FINAL override;
	void OnDestroy() ZENITH_FINAL override;

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

	uint64_t                  m_uSeed = 0ull;
	uint32_t                  m_uSpawnedWalls = 0u;
	uint32_t                  m_uSpawnedGameElements = 0u;
	uint32_t                  m_uSpawnedVillagers = 0u;
	bool                      m_bSpawnedPriest = false;
	DPProcLevel::LevelLayout  m_xLayout;

	static inline DPProcLevelBootstrap_Behaviour* s_pxInstance = nullptr;
};
