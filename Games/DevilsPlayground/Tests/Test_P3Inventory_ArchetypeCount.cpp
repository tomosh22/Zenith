#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DP_Archetypes.h"

#include <cstring>

// ============================================================================
// Test_P3Inventory_ArchetypeCount (MVP-0.2.4)
//
// Proves GDD section 6.2: all 24 archetypes are registered and instantiable.
//
// Coverage:
//   1. DP_Archetypes::Count() == 24 (every entry in Archetypes.json loaded).
//   2. Each MVP archetype (Farmhand, Beggar, Devout, Child) is queryable
//      by id and returns a non-empty struct with positive life timer.
//   3. The full 24-id roster matches the ratified Archetypes.json -- no
//      ids accidentally renamed, dropped, or duplicated.
//
// Per TestPlan section 3.1, post-MVP archetypes (20 entries) sit behind a
// DP_POST_MVP_ARCHETYPES guard. When that macro is defined the test
// additionally walks every post-MVP archetype's struct and asserts
// life_timer > 0 + movement speeds in band. Default build (DP_POST_MVP_
// ARCHETYPES undefined) skips that sweep; only the count + MVP-4
// instantiation are asserted.
// ============================================================================

namespace
{
	int g_iFailures = 0;

	// Ratified 24-archetype roster -- order matches Archetypes.json's
	// "archetypes" array. Adding a new archetype to the JSON requires
	// adding its id here AND -- per the TestPlan -- gating its post-MVP
	// behaviour test behind DP_POST_MVP_ARCHETYPES.
	const char* const g_apszAllArchetypes[] = {
		"Farmhand", "Sexton", "Devout", "Child",
		"Carter", "SmithApp", "Beggar", "Stoneborn",
		"Seer", "Stonemason", "Drunk", "BellRinger",
		"Cooper", "Weaver", "Miller", "Hunter",
		"Midwife", "Tinker", "Pilgrim", "Shepherd",
		"Reeve", "WidowWalker", "Greenwoman", "BlacksmithMaster",
	};
	const size_t g_uExpectedCount = sizeof(g_apszAllArchetypes) / sizeof(g_apszAllArchetypes[0]);

	const char* const g_apszMvpArchetypes[] = {
		"Farmhand", "Beggar", "Devout", "Child",
	};
	const size_t g_uExpectedMvpCount = sizeof(g_apszMvpArchetypes) / sizeof(g_apszMvpArchetypes[0]);
}

static void Setup_P3Inventory_ArchetypeCount()
{
	g_iFailures = 0;
}

static bool Step_P3Inventory_ArchetypeCount(int iFrame)
{
	(void)iFrame;
	return false;
}

static bool Verify_P3Inventory_ArchetypeCount()
{
	g_iFailures = 0;

	// 1) Count invariant.
	const size_t uActualCount = DP_Archetypes::Count();
	if (uActualCount != g_uExpectedCount)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P3Inventory_ArchetypeCount: Count()=%zu expected %zu",
			uActualCount, g_uExpectedCount);
		++g_iFailures;
	}

	// 2) Each MVP archetype is queryable and has positive life timer +
	//    monotonic movement speeds.
	int iMvpFound = 0;
	for (const char* szId : g_apszMvpArchetypes)
	{
		const DP_Archetypes::Archetype* pxA = DP_Archetypes::Get(szId);
		if (pxA == nullptr)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: MVP archetype '%s' missing", szId);
			++g_iFailures;
			continue;
		}
		if (!pxA->mvp)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: '%s' should have mvp=true", szId);
			++g_iFailures;
		}
		if (pxA->life_timer_s <= 0.0f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: '%s' life_timer_s=%f must be > 0",
				szId, pxA->life_timer_s);
			++g_iFailures;
		}
		if (!(pxA->walk_speed_mps < pxA->jog_speed_mps && pxA->jog_speed_mps < pxA->sprint_speed_mps))
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: '%s' speeds not monotonic (walk=%f jog=%f sprint=%f)",
				szId, pxA->walk_speed_mps, pxA->jog_speed_mps, pxA->sprint_speed_mps);
			++g_iFailures;
		}
		++iMvpFound;
	}
	if (iMvpFound != static_cast<int>(g_uExpectedMvpCount))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P3Inventory_ArchetypeCount: found %d MVP archetypes, expected %zu",
			iMvpFound, g_uExpectedMvpCount);
		++g_iFailures;
	}

	// 3) Roster integrity: every expected id is present in the registry and
	//    every registry id is in the expected roster (no extras / no drops).
	for (const char* szId : g_apszAllArchetypes)
	{
		const DP_Archetypes::Archetype* pxA = DP_Archetypes::Get(szId);
		if (pxA == nullptr)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: roster archetype '%s' missing from registry",
				szId);
			++g_iFailures;
		}
	}
	for (size_t u = 0; u < DP_Archetypes::Count(); ++u)
	{
		const DP_Archetypes::Archetype* pxA = DP_Archetypes::GetByIndex(u);
		if (pxA == nullptr) continue;
		bool bInRoster = false;
		for (const char* szId : g_apszAllArchetypes)
		{
			if (pxA->id == szId) { bInRoster = true; break; }
		}
		if (!bInRoster)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: registry has archetype '%s' "
				"not in the expected 24-roster (add it to g_apszAllArchetypes or "
				"remove from Archetypes.json)",
				pxA->id.c_str());
			++g_iFailures;
		}
	}

#ifdef DP_POST_MVP_ARCHETYPES
	// 4) Post-MVP archetype sweep: every non-MVP entry has plausible stats.
	// Only enabled once the post-MVP archetypes are actually implemented
	// (DP_POST_MVP_ARCHETYPES macro is undefined in the MVP build).
	for (size_t u = 0; u < DP_Archetypes::Count(); ++u)
	{
		const DP_Archetypes::Archetype* pxA = DP_Archetypes::GetByIndex(u);
		if (pxA == nullptr || pxA->mvp) continue;
		if (pxA->life_timer_s <= 0.0f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: post-MVP archetype '%s' life_timer_s=%f must be > 0",
				pxA->id.c_str(), pxA->life_timer_s);
			++g_iFailures;
		}
		if (!(pxA->walk_speed_mps < pxA->jog_speed_mps && pxA->jog_speed_mps < pxA->sprint_speed_mps))
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_P3Inventory_ArchetypeCount: post-MVP archetype '%s' speeds not monotonic",
				pxA->id.c_str());
			++g_iFailures;
		}
	}
#endif // DP_POST_MVP_ARCHETYPES

	if (g_iFailures > 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P3Inventory_ArchetypeCount: %d assertion(s) failed", g_iFailures);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xArchetypeCountTest = {
	"Test_P3Inventory_ArchetypeCount",
	&Setup_P3Inventory_ArchetypeCount,
	&Step_P3Inventory_ArchetypeCount,
	&Verify_P3Inventory_ArchetypeCount,
	30,
	// m_bRequiresGraphics: false -- reads DP_Archetypes' cached registry,
	// no scene load or entity spawning required. Runs cleanly in headless CI.
	false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xArchetypeCountTest);

#endif // ZENITH_INPUT_SIMULATOR
