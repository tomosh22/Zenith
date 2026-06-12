#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"

#include "Components/DPHUDController_Component.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P2HUD_AelfricStateReadouts (MVP-2.5.1 + MVP-2.5.3)
//
// Pins the formatting + state-mapping contract for two new HUD
// readouts reactive to the priest's awareness:
//
//   WhisperLine        -- vibe text reflecting priest's state
//                         ("He patrols." / "He stirs..." / "He sees you!")
//   AelfricAwareness   -- state-name placeholder
//                         ("Aelfric: CALM" / "SUSPICIOUS" / "PURSUING")
//
// As with Test_P2HUD_DawnAndScentReadouts, this is a unit test on
// the formatters. The actual state-determination logic
// (ComputeAelfricState) reads the priest's blackboard, which is
// exercised by the existing priest tests (Test_P1Priest_*,
// Test_P1Apprehend_*).
//
// What this catches:
//   * BuildWhisperText or BuildAwarenessText hardcoded the wrong
//     state-string mapping (e.g., "Pursuing" returns "He stirs...").
//   * AelfricState enum re-ordered without updating the formatter
//     switches.
//   * Buffer-too-small truncation (the snprintf calls use small
//     local buffers in production).
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	using AelfricState = DPHUDController_Component::AelfricState;

	bool Has(const char* sz, const char* szNeedle)
	{
		return sz != nullptr && std::strstr(sz, szNeedle) != nullptr;
	}
}

static void Setup_P2HUDAelfric()
{
	g_bPassed = false;
	g_szFailureReason = "";

	char buf[64];

	// --- WhisperLine ---
	DPHUDController_Component::BuildWhisperText(buf, sizeof(buf), AelfricState::Calm);
	if (!Has(buf, "patrols"))
	{
		g_szFailureReason = "whisper for Calm doesn't contain 'patrols'";
		return;
	}
	DPHUDController_Component::BuildWhisperText(buf, sizeof(buf), AelfricState::Suspicious);
	if (!Has(buf, "stirs"))
	{
		g_szFailureReason = "whisper for Suspicious doesn't contain 'stirs'";
		return;
	}
	DPHUDController_Component::BuildWhisperText(buf, sizeof(buf), AelfricState::Pursuing);
	if (!Has(buf, "sees"))
	{
		g_szFailureReason = "whisper for Pursuing doesn't contain 'sees'";
		return;
	}

	// --- AelfricAwareness ---
	// 2026-05-21: format upgraded to "[ glyph ] Aelfric: <state>" so
	// the icon line reads as a distinct status badge. Calm uses
	// "Patrolling" (more player-friendly than "CALM"). Suspicious /
	// Pursuing keep their uppercase emphasis for urgency. Test still
	// pins the "Aelfric: " prefix + the state-name substring.
	DPHUDController_Component::BuildAwarenessText(buf, sizeof(buf), AelfricState::Calm);
	if (!Has(buf, "Aelfric: ") || !Has(buf, "Patrolling"))
	{
		g_szFailureReason = "awareness for Calm doesn't contain 'Aelfric: Patrolling'";
		return;
	}
	DPHUDController_Component::BuildAwarenessText(buf, sizeof(buf), AelfricState::Suspicious);
	if (!Has(buf, "SUSPICIOUS"))
	{
		g_szFailureReason = "awareness for Suspicious doesn't contain 'SUSPICIOUS'";
		return;
	}
	DPHUDController_Component::BuildAwarenessText(buf, sizeof(buf), AelfricState::Pursuing);
	if (!Has(buf, "PURSUING"))
	{
		g_szFailureReason = "awareness for Pursuing doesn't contain 'PURSUING'";
		return;
	}

	g_bPassed = true;
	std::printf("[P2HUDAelfric] all 6 formatter cases passed\n");
	std::fflush(stdout);
}

static bool Step_P2HUDAelfric(int /*iFrame*/)
{
	return false;
}

static bool Verify_P2HUDAelfric()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2HUDAelfric: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2HUDAelfricTest = {
	"Test_P2HUD_AelfricStateReadouts",
	&Setup_P2HUDAelfric,
	&Step_P2HUDAelfric,
	&Verify_P2HUDAelfric,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2HUDAelfricTest);

#endif // ZENITH_INPUT_SIMULATOR
