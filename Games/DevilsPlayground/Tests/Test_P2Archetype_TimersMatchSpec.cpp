#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DP_Archetypes.h"

#include <cmath>
#include <cstring>

// ============================================================================
// Test_P2Archetype_TimersMatchSpec (MVP-0.2.1)
//
// Verifies that the DP_Archetypes subsystem (initialised during
// DevilsPlayground::InitializeResources) has loaded Config/Archetypes.json
// and exposes the 4 MVP-priority archetypes with the expected stats.
//
// Per TestPlan section 3.1 (updated 2026-05-12), the MVP expectation table
// contains only Farmhand, Beggar, Devout, Child (Sexton was swapped out for
// Beggar; see DecisionLog 2026-05-12). The remaining 20 archetypes are
// present in Archetypes.json with mvp=false; this test does NOT iterate
// them (Test_P3Inventory_ArchetypeCount in MVP-0.2.4 handles that).
//
// Coverage classes:
//   1. Exact-equality spot checks against the ratified Archetypes.json
//      (catches accidental edits to load-bearing constants like the 30 s
//      life timer or movement speed defaults).
//   2. Cross-archetype invariants (walk < jog < sprint speeds; mvp flag
//      true for the 4 MVP archetypes and false for the rest).
//
// 1-phase pattern: Setup is a no-op, Step terminates on iFrame==0 (no
// ticking needed -- we're reading cached values), Verify holds all
// assertions.
// ============================================================================

namespace
{
	int g_iFailures = 0;
}

static void Setup_P2Archetype_TimersMatchSpec()
{
	g_iFailures = 0;
}

static bool Step_P2Archetype_TimersMatchSpec(int iFrame)
{
	(void)iFrame;
	return false;
}

static bool CheckFloatEqual(const char* szLabel, float fActual, float fExpected)
{
	const float fTol = 0.001f;
	if (std::fabs(fActual - fExpected) >= fTol)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: '%s' expected %f got %f",
			szLabel, fExpected, fActual);
		++g_iFailures;
		return false;
	}
	return true;
}

static bool CheckIntEqual(const char* szLabel, int iActual, int iExpected)
{
	if (iActual != iExpected)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: '%s' expected %d got %d",
			szLabel, iExpected, iActual);
		++g_iFailures;
		return false;
	}
	return true;
}

static bool CheckBoolEqual(const char* szLabel, bool bActual, bool bExpected)
{
	if (bActual != bExpected)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: '%s' expected %d got %d",
			szLabel, bExpected ? 1 : 0, bActual ? 1 : 0);
		++g_iFailures;
		return false;
	}
	return true;
}

struct MvpExpect
{
	const char* szId;
	float       fLifeTimer;
	float       fWalk;
	float       fJog;
	float       fSprint;
	float       fPossessionChannel;
	float       fDemonScentFloor;
	int         iMinSpawns;
	int         iMaxSpawns;
};

// Ratified Archetypes.json values for the 4 MVP archetypes (TestPlan §3.1
// expectation table updated 2026-05-12 -- Sexton -> Beggar swap; 2026-05-22
// -- MVP archetype life timers doubled across the board as part of the
// game-balance pass, preserving inter-archetype ratios).
static const MvpExpect g_axMvpExpect[] = {
	{ "Farmhand", 45.0f, 4.0f, 8.0f, 12.0f, 0.0f, 0.0f, 6, 10 },
	{ "Beggar",   37.5f, 4.0f, 8.0f, 12.0f, 0.0f, 0.0f, 1,  2 },
	{ "Devout",   45.0f, 4.0f, 8.0f, 12.0f, 0.8f, 0.4f, 1,  3 },
	{ "Child",    22.5f, 4.0f, 8.0f, 12.0f, 0.0f, 0.0f, 1,  2 },
};

static void CheckMvpArchetype(const MvpExpect& xExp)
{
	const DP_Archetypes::Archetype* pxA = DP_Archetypes::Get(xExp.szId);
	if (pxA == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: archetype '%s' not found", xExp.szId);
		++g_iFailures;
		return;
	}

	// 1) MVP flag must be true for the 4 ratified MVP archetypes.
	{
		std::string strLabel = std::string(xExp.szId) + ".mvp";
		CheckBoolEqual(strLabel.c_str(), pxA->mvp, true);
	}

	// 2) Per-archetype stats. Life timer + spawn ranges vary by archetype
	//    (Child=15s frail, Beggar=25s, Farmhand+Devout=30s baseline);
	//    Devout has the unique possession_channel_s=0.8 and demon_scent_floor=0.4
	//    that gate the "harder to possess + always slightly noticeable" design
	//    (GDD section 5.4). Test asserts the exact ratified values.
	{
		std::string strLabel = std::string(xExp.szId) + ".life_timer_s";
		CheckFloatEqual(strLabel.c_str(), pxA->life_timer_s, xExp.fLifeTimer);
	}
	{
		std::string strLabel = std::string(xExp.szId) + ".walk_speed_mps";
		CheckFloatEqual(strLabel.c_str(), pxA->walk_speed_mps, xExp.fWalk);
	}
	{
		std::string strLabel = std::string(xExp.szId) + ".jog_speed_mps";
		CheckFloatEqual(strLabel.c_str(), pxA->jog_speed_mps, xExp.fJog);
	}
	{
		std::string strLabel = std::string(xExp.szId) + ".sprint_speed_mps";
		CheckFloatEqual(strLabel.c_str(), pxA->sprint_speed_mps, xExp.fSprint);
	}
	{
		std::string strLabel = std::string(xExp.szId) + ".possession_channel_s";
		CheckFloatEqual(strLabel.c_str(), pxA->possession_channel_s, xExp.fPossessionChannel);
	}
	{
		std::string strLabel = std::string(xExp.szId) + ".demon_scent_floor";
		CheckFloatEqual(strLabel.c_str(), pxA->demon_scent_floor, xExp.fDemonScentFloor);
	}
	{
		std::string strLabel = std::string(xExp.szId) + ".min_spawns";
		CheckIntEqual(strLabel.c_str(), pxA->min_spawns, xExp.iMinSpawns);
	}
	{
		std::string strLabel = std::string(xExp.szId) + ".max_spawns";
		CheckIntEqual(strLabel.c_str(), pxA->max_spawns, xExp.iMaxSpawns);
	}

	// 4) Movement speeds must monotonically increase walk < jog < sprint.
	if (!(pxA->walk_speed_mps < pxA->jog_speed_mps && pxA->jog_speed_mps < pxA->sprint_speed_mps))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: '%s' movement speeds out of order "
			"(walk=%f jog=%f sprint=%f)",
			xExp.szId, pxA->walk_speed_mps, pxA->jog_speed_mps, pxA->sprint_speed_mps);
		++g_iFailures;
	}

	// 5) Min/max spawns must be sane (min <= max, both >= 0).
	if (pxA->min_spawns < 0 || pxA->max_spawns < pxA->min_spawns)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: '%s' spawn range invalid (min=%d max=%d)",
			xExp.szId, pxA->min_spawns, pxA->max_spawns);
		++g_iFailures;
	}
}

static bool Verify_P2Archetype_TimersMatchSpec()
{
	g_iFailures = 0;

	// Per-MVP-archetype spot checks (4 entries) against the ratified table.
	for (const MvpExpect& xExp : g_axMvpExpect)
	{
		CheckMvpArchetype(xExp);
	}

	// Global invariants on the cache.
	const size_t uCount = DP_Archetypes::Count();
	CheckIntEqual("DP_Archetypes::Count() == 24",
		static_cast<int>(uCount), 24);

	// Exactly 4 archetypes have mvp=true.
	int iMvpCount = 0;
	for (size_t u = 0; u < uCount; ++u)
	{
		const DP_Archetypes::Archetype* pxA = DP_Archetypes::GetByIndex(u);
		if (pxA && pxA->mvp) ++iMvpCount;
	}
	CheckIntEqual("number of mvp:true archetypes == 4", iMvpCount, 4);

	// Sexton is in the table but is NOT mvp (was demoted to post-MVP per
	// DecisionLog 2026-05-12). Beggar replaced it.
	const DP_Archetypes::Archetype* pxSexton = DP_Archetypes::Get("Sexton");
	if (pxSexton == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: Sexton not found in archetypes "
			"(should be present with mvp=false)");
		++g_iFailures;
	}
	else
	{
		CheckBoolEqual("Sexton.mvp == false (post-MVP demotion)",
			pxSexton->mvp, false);
	}

	// IsMvp convenience returns the right answer for both buckets.
	CheckBoolEqual("IsMvp(Farmhand)", DP_Archetypes::IsMvp("Farmhand"), true);
	CheckBoolEqual("IsMvp(Sexton)",   DP_Archetypes::IsMvp("Sexton"),   false);

	if (g_iFailures > 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Archetype_TimersMatchSpec: %d assertion(s) failed", g_iFailures);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xArchetypeTimersTest = {
	"Test_P2Archetype_TimersMatchSpec",
	&Setup_P2Archetype_TimersMatchSpec,
	&Step_P2Archetype_TimersMatchSpec,
	&Verify_P2Archetype_TimersMatchSpec,
	30, // m_iMaxFrames -- pure cache read; no ticking needed
	// m_bRequiresGraphics: false -- reads cached config values, doesn't load
	// scenes or spawn entities. Runs cleanly in headless CI.
	false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xArchetypeTimersTest);

#endif // ZENITH_INPUT_SIMULATOR
