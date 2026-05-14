#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "DataStream/Zenith_DataStream.h"

#include "Source/DP_Save.h"

#include <cstdio>

// ============================================================================
// Test_P1Save_VersionMismatchFallsBackToDefault (MVP-1.10.3)
//
// Pins the version-policy contract from SaveFormat.md: a blob whose
// schema version is OUTSIDE the supported range
// [kMIN_SUPPORTED_SCHEMA_VERSION, kCURRENT_SCHEMA_VERSION] causes
// TryLoad to return false and leave the output state default-
// constructed.
//
// Two scenarios:
//   1. FUTURE version: write a blob with version = 999 (well above
//      kCURRENT). Simulates loading a V2 save in a V1 build. The
//      loader has no migration path UP, so it must fail-soft to
//      default.
//   2. ANCIENT version: write a blob with version = 0 (below
//      kMIN_SUPPORTED). Simulates an old V0 save that's no longer
//      supported by this build's migration window. Same fail-soft.
//
// Note on the "load returns default" assertion: TryLoad's failure
// path explicitly assigns xOutState = DP_RunState{} after detecting
// the version mismatch. So the test pre-pollutes the output state
// and asserts both (a) TryLoad returned false AND (b) the output
// was reset (which would catch a partial deserialise that DID write
// some fields before bailing -- the rejection happens early, but
// the pre-pollution catches "did anyone overwrite the magic bytes?"
// scenarios).
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	bool IsDefault(const DP_RunState& xState)
	{
		if (xState.m_uSchemaVersion != DP_Save::kCURRENT_SCHEMA_VERSION) return false;
		if (xState.m_xPossessedVillager.IsValid()) return false;
		if (xState.m_fPossessedLife != 0.0f) return false;
		if (xState.m_axHeldItems.GetSize() != 0) return false;
		if (xState.m_axScent.GetSize() != 0) return false;
		if (xState.m_uObjectivesMask != 0) return false;
		if (xState.m_fDawnTimerRemaining != 30.0f) return false;
		return true;
	}
}

static void Setup_P1SaveVersionMismatch()
{
	g_bPassed = false;
	g_szFailureReason = "";

	// ----- Case 1: FUTURE version. -----
	{
		Zenith_DataStream xStream(64);
		xStream << DP_Save::kMAGIC;
		// Write a version far above kCURRENT. Even if we write the
		// rest of a "valid-looking" V1 blob after, the version gate
		// runs before any field reads.
		const uint32_t uFutureVer = 999u;
		xStream << uFutureVer;
		// Pad with garbage so a missing-version-gate regression
		// would NOT fail by truncation but would actually try to
		// parse the rest of the bytes.
		for (int i = 0; i < 8; ++i)
		{
			xStream << uint32_t{0xDEADBEEFu};
		}
		xStream.SetCursor(0);
		DP_RunState xLoaded;
		// Pre-pollute so a missing fail-soft reset would be caught.
		xLoaded.m_xPossessedVillager.m_uIndex = 999;
		xLoaded.m_uObjectivesMask = 0xFFFFFFFFu;
		const bool bOk = DP_Save::TryLoad(xStream, xLoaded);
		if (bOk)
		{
			g_szFailureReason = "TryLoad accepted FUTURE version (999) but should have rejected";
			return;
		}
		if (!IsDefault(xLoaded))
		{
			g_szFailureReason = "Output state not default after FUTURE version rejection";
			return;
		}
	}

	// ----- Case 2: ANCIENT version. -----
	// Only meaningful if kMIN_SUPPORTED > 0. Today kMIN_SUPPORTED is
	// 1, so version=0 is below the floor. The test logic is robust to
	// kMIN_SUPPORTED moving forward (just always tries kMIN-1, with
	// 0 as the floor if kMIN is already 1).
	{
		Zenith_DataStream xStream(64);
		xStream << DP_Save::kMAGIC;
		const uint32_t uAncientVer =
			(DP_Save::kMIN_SUPPORTED_SCHEMA_VERSION > 0)
				? (DP_Save::kMIN_SUPPORTED_SCHEMA_VERSION - 1)
				: 0u;
		xStream << uAncientVer;
		for (int i = 0; i < 8; ++i)
		{
			xStream << uint32_t{0xDEADBEEFu};
		}
		xStream.SetCursor(0);
		DP_RunState xLoaded;
		xLoaded.m_uObjectivesMask = 0xFFFFFFFFu;
		// Only run the assertion if there IS an "ancient" version to
		// test (kMIN_SUPPORTED >= 1).
		if constexpr (DP_Save::kMIN_SUPPORTED_SCHEMA_VERSION >= 1)
		{
			const bool bOk = DP_Save::TryLoad(xStream, xLoaded);
			if (bOk)
			{
				g_szFailureReason = "TryLoad accepted ANCIENT version (below kMIN_SUPPORTED) but should have rejected";
				return;
			}
			if (!IsDefault(xLoaded))
			{
				g_szFailureReason = "Output state not default after ANCIENT version rejection";
				return;
			}
		}
	}

	g_bPassed = true;
}

static bool Step_P1SaveVersionMismatch(int /*iFrame*/)
{
	return false;
}

static bool Verify_P1SaveVersionMismatch()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1SaveVersionMismatch: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1SaveVersionMismatchTest = {
	"Test_P1Save_VersionMismatchFallsBackToDefault",
	&Setup_P1SaveVersionMismatch,
	&Step_P1SaveVersionMismatch,
	&Verify_P1SaveVersionMismatch,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1SaveVersionMismatchTest);

#endif // ZENITH_INPUT_SIMULATOR
