#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "DataStream/Zenith_DataStream.h"

#include "Source/DP_Save.h"
#include "Source/PublicInterfaces.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Save_RoundTripMeta (MVP-1.10.1)
//
// Pins the core save/load contract: a DP_RunState constructed with
// known non-default values can be serialised to a Zenith_DataStream
// and read back via TryLoad as an exact field-for-field copy.
//
// Procedure (executes entirely in Setup -- no per-frame state needed):
//   1. Construct DP_RunState xOrig with specific, non-default values
//      for every field (possessed villager, life, held items entry,
//      scent entry, objectives mask, dawn timer).
//   2. Save(xOrig, xStream).
//   3. Reset xStream cursor to 0.
//   4. TryLoad(xStream, xLoaded).
//   5. Assert TryLoad returned true AND every field of xLoaded
//      matches xOrig exactly.
//
// What this catches:
//   * Mis-aligned read/write ordering (e.g., Save writes index then
//     generation but Load reads generation then index).
//   * A field added to DP_RunState but only wired into Save, not Load
//     (the tail field would default-construct on load).
//   * Cursor management bugs in Zenith_DataStream (reads past the end
//     of written data).
//   * Vector serialization that loses or duplicates entries.
// ============================================================================

namespace
{
	bool ApproxEquals(float fA, float fB, float fTol = 0.001f)
	{
		const float fD = fA - fB;
		return fD > -fTol && fD < fTol;
	}

	bool g_bPassed = false;
	const char* g_szFailureReason = "";
}

static void Setup_P1SaveRoundTrip()
{
	g_bPassed = false;
	g_szFailureReason = "";

	// Build a state with all-distinct, all-non-default values so a
	// silent default-construction-on-load regression can't make the
	// test pass by accident.
	DP_RunState xOrig;
	xOrig.m_uSchemaVersion = DP_Save::kCURRENT_SCHEMA_VERSION;
	xOrig.m_xPossessedVillager.m_uIndex      = 42u;
	xOrig.m_xPossessedVillager.m_uGeneration = 7u;
	xOrig.m_fPossessedLife = 17.5f;
	{
		DP_HeldItemEntry xE;
		xE.m_xVillager.m_uIndex = 42u;
		xE.m_xVillager.m_uGeneration = 7u;
		xE.m_xItem.m_uIndex = 100u;
		xE.m_xItem.m_uGeneration = 1u;
		xE.m_eTag = DP_ItemTag::Key;
		xOrig.m_axHeldItems.PushBack(xE);

		DP_HeldItemEntry xE2;
		xE2.m_xVillager.m_uIndex = 43u;
		xE2.m_xVillager.m_uGeneration = 7u;
		xE2.m_xItem.m_uIndex = 101u;
		xE2.m_xItem.m_uGeneration = 1u;
		xE2.m_eTag = DP_ItemTag::Iron;
		xOrig.m_axHeldItems.PushBack(xE2);
	}
	{
		DP_ScentEntry xE;
		xE.m_xVillager.m_uIndex = 50u;
		xE.m_xVillager.m_uGeneration = 3u;
		xE.m_fScent = 0.42f;
		xOrig.m_axScent.PushBack(xE);
	}
	xOrig.m_uObjectivesMask = 0b10101u; // 3 of 5 objectives
	xOrig.m_fDawnTimerRemaining = 12.5f;

	// Save -> Load round trip.
	Zenith_DataStream xStream(1024);
	DP_Save::Save(xOrig, xStream);
	const uint64_t ulWritten = xStream.GetCursor();
	xStream.SetCursor(0);

	DP_RunState xLoaded;
	const bool bOk = DP_Save::TryLoad(xStream, xLoaded);

	std::printf("[P1SaveRoundTrip] wrote=%llu bytes, TryLoad=%d\n",
		static_cast<unsigned long long>(ulWritten), (int)bOk);

	if (!bOk)
	{
		g_szFailureReason = "TryLoad returned false on a freshly-written blob";
		return;
	}
	if (xLoaded.m_uSchemaVersion != xOrig.m_uSchemaVersion)
	{
		g_szFailureReason = "schema version round-trip mismatch";
		return;
	}
	if (xLoaded.m_xPossessedVillager.m_uIndex != xOrig.m_xPossessedVillager.m_uIndex
		|| xLoaded.m_xPossessedVillager.m_uGeneration != xOrig.m_xPossessedVillager.m_uGeneration)
	{
		g_szFailureReason = "possessed villager EntityID round-trip mismatch";
		return;
	}
	if (!ApproxEquals(xLoaded.m_fPossessedLife, xOrig.m_fPossessedLife))
	{
		g_szFailureReason = "possessed life round-trip mismatch";
		return;
	}
	if (xLoaded.m_axHeldItems.GetSize() != xOrig.m_axHeldItems.GetSize())
	{
		g_szFailureReason = "held-items count round-trip mismatch";
		return;
	}
	for (uint32_t u = 0; u < xOrig.m_axHeldItems.GetSize(); ++u)
	{
		const DP_HeldItemEntry& xO = xOrig.m_axHeldItems.Get(u);
		const DP_HeldItemEntry& xL = xLoaded.m_axHeldItems.Get(u);
		if (xO.m_xVillager.m_uIndex != xL.m_xVillager.m_uIndex
			|| xO.m_xVillager.m_uGeneration != xL.m_xVillager.m_uGeneration
			|| xO.m_xItem.m_uIndex != xL.m_xItem.m_uIndex
			|| xO.m_xItem.m_uGeneration != xL.m_xItem.m_uGeneration
			|| xO.m_eTag != xL.m_eTag)
		{
			g_szFailureReason = "held-items entry round-trip mismatch";
			return;
		}
	}
	if (xLoaded.m_axScent.GetSize() != xOrig.m_axScent.GetSize())
	{
		g_szFailureReason = "scent count round-trip mismatch";
		return;
	}
	for (uint32_t u = 0; u < xOrig.m_axScent.GetSize(); ++u)
	{
		const DP_ScentEntry& xO = xOrig.m_axScent.Get(u);
		const DP_ScentEntry& xL = xLoaded.m_axScent.Get(u);
		if (xO.m_xVillager.m_uIndex != xL.m_xVillager.m_uIndex
			|| xO.m_xVillager.m_uGeneration != xL.m_xVillager.m_uGeneration
			|| !ApproxEquals(xO.m_fScent, xL.m_fScent))
		{
			g_szFailureReason = "scent entry round-trip mismatch";
			return;
		}
	}
	if (xLoaded.m_uObjectivesMask != xOrig.m_uObjectivesMask)
	{
		g_szFailureReason = "objectives mask round-trip mismatch";
		return;
	}
	if (!ApproxEquals(xLoaded.m_fDawnTimerRemaining, xOrig.m_fDawnTimerRemaining))
	{
		g_szFailureReason = "dawn timer round-trip mismatch";
		return;
	}

	g_bPassed = true;
}

static bool Step_P1SaveRoundTrip(int /*iFrame*/)
{
	// All work happens in Setup; no per-frame state to advance.
	return false;
}

static bool Verify_P1SaveRoundTrip()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1SaveRoundTrip: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1SaveRoundTripTest = {
	"Test_P1Save_RoundTripMeta",
	&Setup_P1SaveRoundTrip,
	&Step_P1SaveRoundTrip,
	&Verify_P1SaveRoundTrip,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1SaveRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
