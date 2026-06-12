#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPHUDController_Component.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P2HUD_DawnAndScentReadouts (MVP-2.5.2 + MVP-2.5.4)
//
// Pins the formatting + data-binding contract for the two new HUD
// readouts:
//   * DawnGauge -- "Dawn: X.X s" while a night is active. Reads
//     DP_Night::GetNightTimeRemaining().
//   * ScentIndicator -- "Scent: X.XX". Reads
//     DP_Player::GetDemonScent(possessedVillager).
//
// The HUD class exposes BuildDawnText / BuildScentText as static
// helpers. Production OnUpdate uses these to populate
// Zenith_UIText elements; this test uses them directly so the
// assertion doesn't depend on the UI/font/render pipeline being
// active in headless mode.
//
// Procedure (no scene needed):
//   1. BuildDawnText(buf, sizeof(buf), 12.3f). Assert buf starts
//      with "Dawn: " and contains "12.3".
//   2. BuildDawnText(buf, sizeof(buf), -5.0f). Assert non-negative
//      formatting (clamp to 0). Catches a regression where the
//      formatter prints negative seconds when DP_Night's clamp is
//      off.
//   3. BuildScentText(buf, sizeof(buf), 0.42f). Assert "Scent: 0.42".
//   4. BuildScentText(buf, sizeof(buf), 0.0f). Assert "Scent: 0.00".
//
// Why a unit-style test: the actual HUD-element visibility/text
// flow goes through Zenith_UIText + a real scene + a real UI canvas.
// Asserting "the UI element's text == 'Dawn: 12.3 s'" requires a
// scene with the authored UI element. That's a heavier integration
// test that doesn't add coverage value over the formatter itself.
// The data path (DP_Night, DP_Player::GetDemonScent) is already
// covered by Test_P1Dawn_* and Test_P1Scent_*.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	bool ContainsSubstring(const char* sz, const char* szNeedle)
	{
		return sz != nullptr && std::strstr(sz, szNeedle) != nullptr;
	}
}

static void Setup_P2HUDReadouts()
{
	g_bPassed = false;
	g_szFailureReason = "";

	char buf[64];

	// Step 1: positive dawn timer.
	DPHUDController_Component::BuildDawnText(buf, sizeof(buf), 12.3f);
	if (!ContainsSubstring(buf, "Dawn: "))
	{
		g_szFailureReason = "dawn text missing 'Dawn: ' prefix";
		return;
	}
	if (!ContainsSubstring(buf, "12.3"))
	{
		g_szFailureReason = "dawn text missing the 12.3 value";
		return;
	}
	if (!ContainsSubstring(buf, " s"))
	{
		g_szFailureReason = "dawn text missing seconds suffix";
		return;
	}

	// Step 2: clamp negative input to non-negative output.
	DPHUDController_Component::BuildDawnText(buf, sizeof(buf), -5.0f);
	if (ContainsSubstring(buf, "-"))
	{
		g_szFailureReason = "dawn text with -5s input contains a minus sign -- clamp missing";
		return;
	}

	// Step 3: scent value.
	DPHUDController_Component::BuildScentText(buf, sizeof(buf), 0.42f);
	if (!ContainsSubstring(buf, "Scent: "))
	{
		g_szFailureReason = "scent text missing 'Scent: ' prefix";
		return;
	}
	if (!ContainsSubstring(buf, "0.42"))
	{
		g_szFailureReason = "scent text missing the 0.42 value";
		return;
	}

	// Step 4: zero scent.
	DPHUDController_Component::BuildScentText(buf, sizeof(buf), 0.0f);
	if (!ContainsSubstring(buf, "0.00"))
	{
		g_szFailureReason = "scent text at 0 doesn't render 0.00";
		return;
	}

	g_bPassed = true;
	std::printf("[P2HUDReadouts] all formatter cases passed\n");
	std::fflush(stdout);
}

static bool Step_P2HUDReadouts(int /*iFrame*/)
{
	return false;
}

static bool Verify_P2HUDReadouts()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2HUDReadouts: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2HUDReadoutsTest = {
	"Test_P2HUD_DawnAndScentReadouts",
	&Setup_P2HUDReadouts,
	&Step_P2HUDReadouts,
	&Verify_P2HUDReadouts,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2HUDReadoutsTest);

#endif // ZENITH_INPUT_SIMULATOR
