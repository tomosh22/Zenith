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
#include "EntityComponent/Zenith_SceneManager.h"

#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"

#include <cstdio>
#include <cstdint>

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
		// future P4b/c/d entity-spawn paths) can query the layout
		// before they themselves run.
		s_pxInstance = this;

		// Seed source TODO (P4 later): read from Tuning.json + allow
		// --procgen-seed CLI override. For P4a we hardcode m_uSeed
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

private:
	uint64_t                  m_uSeed = 0ull;
	DPProcLevel::LevelLayout  m_xLayout;

	static inline DPProcLevelBootstrap_Behaviour* s_pxInstance = nullptr;
};
