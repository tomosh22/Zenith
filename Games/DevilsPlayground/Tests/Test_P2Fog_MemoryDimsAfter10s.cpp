#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "../Components/DPFogPass_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Fog_MemoryDimsAfter10s (MVP-2.4.5)
//
// Pins the memory fog state-machine contract from Tuning.json's
// `_comment_memory_states`:
//
//   NEVER_SEEN       : no entry at this position (default).
//   VISITED_VISIBLE  : age <= memory_visible_s (10 s).
//   VISITED_DIM      : memory_visible_s < age <= memory_dim_s
//                      (10 .. 30 s).
//   VISITED_HIDDEN   : age > memory_dim_s.
//
// Procedure (all in Setup/Step -- no scene-bound entities needed):
//   1. Pick a query position; assert state is NeverSeen (no entry).
//   2. Call DP_Fog::RecordMemoryReveal(P).
//   3. Assert state at P is VisitedVisible (age 0 <= 10).
//   4. Tick a moderate amount (5 s of game time via TickMemoryFog
//      with fDt=5.0 -- the test drives the clock directly, no
//      need to spin frames). Assert still VisitedVisible.
//   5. Tick 6 more s (total 11 s). Assert VisitedDim.
//   6. Tick 20 more s (total 31 s). Assert VisitedHidden.
//   7. Record a fresh reveal at P. Assert state goes back to
//      VisitedVisible (cell-age reset).
//   8. Pick a SECOND query position OUTSIDE the 1 m grid cell
//      containing P. Assert state is NeverSeen (different cell, no
//      entry).
//
// What this catches:
//   * TickMemoryFog doesn't decrement ages (state never advances).
//   * Threshold checks use wrong comparison (e.g., `<` vs `<=`).
//   * RecordMemoryReveal at an existing cell doesn't reset age
//     (the cell stays in whatever state it was, instead of going
//     back to VisitedVisible).
//   * GetMemoryStateAt picks the wrong cell (grid snap bug).
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";
	int g_iFailureStep = 0;

	const char* StateName(DP_Fog::MemoryTileState e)
	{
		switch (e)
		{
		case DP_Fog::MemoryTileState::NeverSeen:       return "NeverSeen";
		case DP_Fog::MemoryTileState::VisitedVisible:  return "VisitedVisible";
		case DP_Fog::MemoryTileState::VisitedDim:      return "VisitedDim";
		case DP_Fog::MemoryTileState::VisitedHidden:   return "VisitedHidden";
		}
		return "?";
	}
}

static void Setup_P2MemoryFog()
{
	g_bPassed = false;
	g_szFailureReason = "";
	g_iFailureStep = 0;

	// 2026-05-17 ownership refactor: DP_Fog memory-reveals table moved
	// onto DPFogPass_Behaviour::m_xMemoryReveals. Spin up a scene with
	// the script attached so the DP_Fog::Record/Get/Tick forwarders
	// actually do something (no-ops without an Instance()).
	Zenith_Scene xScene = g_xEngine.SceneRegistry().CreateEmptyScene("MemoryFogTest");
	Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneData(xScene);
	Zenith_Entity xFogEntity(pxScene, "FogPassEntity");
	xFogEntity.AddComponent<Zenith_ScriptComponent>()
		.AddScript<DPFogPass_Behaviour>();

	// Ensure clean state -- the between-tests reset hook should have
	// cleared this already, but belt-and-braces.
	DP_Fog::ClearAllMemoryReveals();

	const Zenith_Maths::Vector3 xP(123.0f, 0.0f, 456.0f);  // arbitrary
	const Zenith_Maths::Vector3 xQ(124.5f, 0.0f, 458.5f);  // different 1m cell

	auto FailAt = [](int iStep, const char* sz)
	{
		g_iFailureStep = iStep;
		g_szFailureReason = sz;
	};

	// Step 1: NeverSeen before any record.
	if (DP_Fog::GetMemoryStateAt(xP) != DP_Fog::MemoryTileState::NeverSeen)
	{
		FailAt(1, "expected NeverSeen before any RecordMemoryReveal");
		return;
	}

	// Step 2-3: record + assert VisitedVisible.
	DP_Fog::RecordMemoryReveal(xP);
	if (DP_Fog::GetMemoryStateAt(xP) != DP_Fog::MemoryTileState::VisitedVisible)
	{
		FailAt(3, "expected VisitedVisible immediately after RecordMemoryReveal");
		return;
	}

	// Step 4: tick 5 s; still VisitedVisible (10 s threshold).
	DP_Fog::TickMemoryFog(5.0f);
	if (DP_Fog::GetMemoryStateAt(xP) != DP_Fog::MemoryTileState::VisitedVisible)
	{
		FailAt(4, "expected VisitedVisible at age 5s (threshold 10s)");
		return;
	}

	// Step 5: tick 6 more s -> total 11 s -> VisitedDim.
	DP_Fog::TickMemoryFog(6.0f);
	if (DP_Fog::GetMemoryStateAt(xP) != DP_Fog::MemoryTileState::VisitedDim)
	{
		FailAt(5, "expected VisitedDim at age 11s (10..30s window)");
		return;
	}

	// Step 6: tick 20 more s -> total 31 s -> VisitedHidden.
	DP_Fog::TickMemoryFog(20.0f);
	if (DP_Fog::GetMemoryStateAt(xP) != DP_Fog::MemoryTileState::VisitedHidden)
	{
		FailAt(6, "expected VisitedHidden at age 31s (>30s)");
		return;
	}

	// Step 7: record again -> reset to VisitedVisible.
	DP_Fog::RecordMemoryReveal(xP);
	if (DP_Fog::GetMemoryStateAt(xP) != DP_Fog::MemoryTileState::VisitedVisible)
	{
		FailAt(7, "expected VisitedVisible after re-reveal (cell-age must reset)");
		return;
	}

	// Step 8: a different cell should still be NeverSeen.
	if (DP_Fog::GetMemoryStateAt(xQ) != DP_Fog::MemoryTileState::NeverSeen)
	{
		FailAt(8, "expected NeverSeen at a different 1m grid cell");
		return;
	}

	g_bPassed = true;

	std::printf("[P2MemoryFog] all 8 steps passed (P_state=%s reveals=%u)\n",
		StateName(DP_Fog::GetMemoryStateAt(xP)),
		DP_Fog::GetMemoryRevealCount());
	std::fflush(stdout);

	g_xEngine.SceneOperations().UnloadScene(xScene);
}

static bool Step_P2MemoryFog(int /*iFrame*/)
{
	// All work happens in Setup.
	return false;
}

static bool Verify_P2MemoryFog()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryFog: step %d failed -- %s",
			g_iFailureStep, g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2MemoryFogTest = {
	"Test_P2Fog_MemoryDimsAfter10s",
	&Setup_P2MemoryFog,
	&Step_P2MemoryFog,
	&Verify_P2MemoryFog,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2MemoryFogTest);

#endif // ZENITH_INPUT_SIMULATOR
