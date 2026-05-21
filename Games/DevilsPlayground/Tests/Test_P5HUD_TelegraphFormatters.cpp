#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPHUDController_Behaviour.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P5HUD_TelegraphFormatters (2026-05-21)
//
// Pins the pure-formatter helpers added in the 2026-05-21 HUD-upgrades
// pass. Each formatter takes raw data + caller-supplied buffer and
// returns formatted text; testing them in isolation lets the test stay
// off the UI canvas while still pinning the player-facing strings.
//
// Surfaces covered:
//   BuildAwarenessText(state)            -> "[ glyph ] Aelfric: <state>"
//   AwarenessColor(state, flashT)        -> Vec4 risk-band gradient
//   BuildScentBar(scent, threshold)      -> "Scent [###|##....]"
//   ScentBarColor(scent, threshold)      -> Vec4 (red when >= threshold)
//   BuildArchetypeStatusText(archetype)  -> "<archetype rule>" or nullptr
//   BuildLockedDoorAlertText(tag)        -> "LOCKED -- needs <key>"
//
// Doesn't author a UI canvas. Pure synchronous Verify covers all six
// surfaces in 0 frames.
// ============================================================================

static void Setup_P5HUDTelegraphs() {}

static bool Step_P5HUDTelegraphs(int /*iFrame*/) { return false; }

static bool Verify_P5HUDTelegraphs()
{
	char xBuf[80];

	// ----- Awareness text + colour -----
	DPHUDController_Behaviour::BuildAwarenessText(xBuf, sizeof(xBuf),
		DPHUDController_Behaviour::AelfricState::Calm);
	if (std::strstr(xBuf, "Patrolling") == nullptr || std::strstr(xBuf, "~") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Calm awareness text wrong: \"%s\"", xBuf);
		return false;
	}
	DPHUDController_Behaviour::BuildAwarenessText(xBuf, sizeof(xBuf),
		DPHUDController_Behaviour::AelfricState::Suspicious);
	if (std::strstr(xBuf, "SUSPICIOUS") == nullptr || std::strstr(xBuf, "?") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Suspicious awareness text wrong: \"%s\"", xBuf);
		return false;
	}
	DPHUDController_Behaviour::BuildAwarenessText(xBuf, sizeof(xBuf),
		DPHUDController_Behaviour::AelfricState::Pursuing);
	if (std::strstr(xBuf, "PURSUING") == nullptr || std::strstr(xBuf, "!") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Pursuing awareness text wrong: \"%s\"", xBuf);
		return false;
	}

	// Awareness colours: Calm is green-dominant, Pursuing is red-dominant.
	const Zenith_Maths::Vector4 xCalmColor =
		DPHUDController_Behaviour::AwarenessColor(
			DPHUDController_Behaviour::AelfricState::Calm, 0.0f);
	const Zenith_Maths::Vector4 xPursuingColor =
		DPHUDController_Behaviour::AwarenessColor(
			DPHUDController_Behaviour::AelfricState::Pursuing, 0.0f);
	if (!(xCalmColor.y > xCalmColor.x))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Calm awareness colour not green-dominant (G=%.2f R=%.2f)",
			xCalmColor.y, xCalmColor.x);
		return false;
	}
	if (!(xPursuingColor.x > xPursuingColor.y))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Pursuing awareness colour not red-dominant (R=%.2f G=%.2f)",
			xPursuingColor.x, xPursuingColor.y);
		return false;
	}

	// ----- Scent bar -----
	// At fScent=0, fThreshold=0.5: bar should be 10 cells, all empty,
	// with a threshold separator after the 5th cell.
	DPHUDController_Behaviour::BuildScentBar(xBuf, sizeof(xBuf), 0.0f, 0.5f);
	if (std::strstr(xBuf, "Scent") == nullptr
		|| std::strstr(xBuf, "[") == nullptr
		|| std::strstr(xBuf, "]") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: ScentBar(0, 0.5) missing brackets: \"%s\"", xBuf);
		return false;
	}
	// At fScent=0.5, fThreshold=0.5: ~5 cells filled, separator at the
	// halfway point. The exact format is "[#####|.....]" (10 cells +
	// separator). We confirm it contains "|" (the threshold marker)
	// and at least 4 '#' characters (one less than 5 is acceptable
	// rounding slack).
	DPHUDController_Behaviour::BuildScentBar(xBuf, sizeof(xBuf), 0.5f, 0.5f);
	{
		int iHashes = 0;
		for (const char* p = xBuf; *p; ++p) if (*p == '#') ++iHashes;
		bool bHasSeparator = std::strchr(xBuf, '|') != nullptr;
		if (!bHasSeparator)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P5HUDTelegraphs: ScentBar(0.5, 0.5) missing threshold separator: \"%s\"", xBuf);
			return false;
		}
		if (iHashes < 4)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P5HUDTelegraphs: ScentBar(0.5, 0.5) has only %d filled cells: \"%s\"",
				iHashes, xBuf);
			return false;
		}
	}
	// At fScent=1.0, fThreshold=0.5: 10 cells filled.
	DPHUDController_Behaviour::BuildScentBar(xBuf, sizeof(xBuf), 1.0f, 0.5f);
	{
		int iHashes = 0;
		for (const char* p = xBuf; *p; ++p) if (*p == '#') ++iHashes;
		if (iHashes < 9)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P5HUDTelegraphs: ScentBar(1.0) only %d filled cells, expected ~10: \"%s\"",
				iHashes, xBuf);
			return false;
		}
	}

	// Scent colour: below threshold is purple-leaning, above threshold
	// is red-dominant.
	const Zenith_Maths::Vector4 xLowScent =
		DPHUDController_Behaviour::ScentBarColor(0.2f, 0.5f);
	const Zenith_Maths::Vector4 xHighScent =
		DPHUDController_Behaviour::ScentBarColor(0.8f, 0.5f);
	if (!(xLowScent.z > xLowScent.y))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: low-scent colour not purple-leaning (B=%.2f G=%.2f)",
			xLowScent.z, xLowScent.y);
		return false;
	}
	if (!(xHighScent.x > xHighScent.y && xHighScent.x > xHighScent.z))
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: high-scent colour not red-dominant (R=%.2f G=%.2f B=%.2f)",
			xHighScent.x, xHighScent.y, xHighScent.z);
		return false;
	}

	// ----- Archetype status -----
	const char* szBeggar = DPHUDController_Behaviour::BuildArchetypeStatusText("Beggar");
	if (szBeggar == nullptr || std::strstr(szBeggar, "BEGGAR") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Beggar status text missing: \"%s\"",
			szBeggar != nullptr ? szBeggar : "(null)");
		return false;
	}
	const char* szDevout = DPHUDController_Behaviour::BuildArchetypeStatusText("Devout");
	if (szDevout == nullptr || std::strstr(szDevout, "DEVOUT") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Devout status text missing: \"%s\"",
			szDevout != nullptr ? szDevout : "(null)");
		return false;
	}
	const char* szChild = DPHUDController_Behaviour::BuildArchetypeStatusText("Child");
	if (szChild == nullptr || std::strstr(szChild, "CHILD") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Child status text missing: \"%s\"",
			szChild != nullptr ? szChild : "(null)");
		return false;
	}
	// Farmhand has no special rule -> nullptr (HUD line hides).
	const char* szFarmhand = DPHUDController_Behaviour::BuildArchetypeStatusText("Farmhand");
	if (szFarmhand != nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: Farmhand should return nullptr but got \"%s\"", szFarmhand);
		return false;
	}

	// ----- Locked-door alert -----
	DPHUDController_Behaviour::BuildLockedDoorAlertText(xBuf, sizeof(xBuf), DP_ItemTag::Key);
	if (std::strstr(xBuf, "LOCKED") == nullptr || std::strstr(xBuf, "Key") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: LockedDoor(Key) text wrong: \"%s\"", xBuf);
		return false;
	}
	DPHUDController_Behaviour::BuildLockedDoorAlertText(
		xBuf, sizeof(xBuf), DP_ItemTag::SkeletonKey);
	if (std::strstr(xBuf, "Skeleton") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5HUDTelegraphs: LockedDoor(SkeletonKey) text wrong: \"%s\"", xBuf);
		return false;
	}

	return true;
}

static const Zenith_AutomatedTest g_xP5HUDTelegraphsTest = {
	"Test_P5HUD_TelegraphFormatters",
	&Setup_P5HUDTelegraphs,
	&Step_P5HUDTelegraphs,
	&Verify_P5HUDTelegraphs,
	1
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP5HUDTelegraphsTest);

#endif // ZENITH_INPUT_SIMULATOR
