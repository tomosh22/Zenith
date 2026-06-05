#pragma once

#include "CityBuilder/Source/CB_CityStats.h"
#include <cstdint>

class CB_CityGrid;
class CB_RoadNetwork;
class CB_BuildingManager;
class CB_ServiceManager;
class CB_EconomyManager;
class CB_CitizenManager;

// ============================================================================
// CB_SimulationTick — fixed-timestep orchestrator. Accumulates real time scaled
// by the sim speed and fires discrete ticks; each tick runs the phases in order
// (population -> services -> demand -> growth -> derived/happiness -> taxes).
// Deterministic (no RNG in the core loop), so a seeded city replays identically.
// ============================================================================

class CB_SimulationTick
{
public:
	void Initialize(CB_CityGrid* pxGrid, CB_RoadNetwork* pxRoads, CB_BuildingManager* pxBuildings,
	                CB_ServiceManager* pxServices, CB_EconomyManager* pxEconomy, CB_CitizenManager* pxCitizens);
	void Shutdown();
	bool IsInitialized() const { return m_pxBuildings != nullptr; }

	void SetStartingTreasury(float fTreasury) { m_xStats.m_fTreasury = fTreasury; }

	// Per-frame: advance the accumulator and fire any due ticks.
	void Update(float fRealDt);
	// Run exactly one tick now (also used by tests).
	void RunTick();
	// Run N ticks back-to-back (tests).
	void RunTicks(uint32_t uCount) { for (uint32_t u = 0; u < uCount; ++u) { RunTick(); } }

	void          SetSpeed(CB_ESimSpeed eSpeed) { m_eSpeed = eSpeed; }
	CB_ESimSpeed  GetSpeed() const              { return m_eSpeed; }

	const CB_CityStats& GetStats() const { return m_xStats; }
	uint64_t            GetTickCount() const { return m_ulTickCount; }

	static float SpeedMultiplier(CB_ESimSpeed eSpeed);

private:
	void RefreshPopulationStats();

	CB_CityGrid*        m_pxGrid      = nullptr;
	CB_RoadNetwork*     m_pxRoads     = nullptr;
	CB_BuildingManager* m_pxBuildings = nullptr;
	CB_ServiceManager*  m_pxServices  = nullptr;
	CB_EconomyManager*  m_pxEconomy   = nullptr;
	CB_CitizenManager*  m_pxCitizens  = nullptr;

	CB_CityStats m_xStats;
	CB_ESimSpeed m_eSpeed       = CB_SIM_NORMAL;
	float        m_fAccumulator = 0.0f;
	uint64_t     m_ulTickCount  = 0;

	static constexpr float fTICK_INTERVAL = 1.0f;  // seconds of game-time per tick
};
