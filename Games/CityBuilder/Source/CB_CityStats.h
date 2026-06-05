#pragma once

#include <cstdint>

// ============================================================================
// CB_CityStats — the shared city-wide simulation state, produced by the sim
// tick and read by the HUD. CB_ESimSpeed controls tick cadence.
// ============================================================================

enum CB_ESimSpeed : uint8_t
{
	CB_SIM_PAUSED = 0,
	CB_SIM_SLOW   = 1,
	CB_SIM_NORMAL = 2,
	CB_SIM_FAST   = 3,
	CB_SIM_ULTRA  = 4,
	CB_SIM_SPEED_COUNT
};

struct CB_CityStats
{
	uint32_t m_uPopulation     = 0;
	uint32_t m_uJobs           = 0;   // commercial + industrial capacity
	uint32_t m_uCommercialJobs = 0;
	uint32_t m_uIndustrialJobs = 0;
	uint32_t m_uEmployed       = 0;   // min(population, jobs)

	float m_fResDemand  = 0.0f;   // -1..1
	float m_fComDemand  = 0.0f;
	float m_fIndDemand  = 0.0f;

	float m_fHappiness  = 0.5f;   // 0..1

	float m_fTreasury        = 100000.0f;
	float m_fIncomePerTick   = 0.0f;
	float m_fExpensesPerTick = 0.0f;

	float m_fPowerCapacity = 0.0f;
	float m_fPowerDemand   = 0.0f;
	float m_fWaterCapacity = 0.0f;
	float m_fWaterDemand   = 0.0f;

	bool m_bPowerOk = false;
	bool m_bWaterOk = false;
};
