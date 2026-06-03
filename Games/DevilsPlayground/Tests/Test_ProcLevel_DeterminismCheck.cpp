#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

// ============================================================================
// Test_ProcLevel_DeterminismCheck (2026-05-19)
//
// PROCGEN MUST BE BIT-DETERMINISTIC. The same seed + config must produce
// the same LevelLayout regardless of build configuration, optimisation
// level, or which platform the test runs on.
//
// This test exercises two layers of determinism:
//
//   1. WITHIN-CONFIG REPEAT: call Generate(seed, cfg, ...) twice and
//      assert the two outputs are byte-identical. Catches accidental
//      static-state leaks (RNG init, global counters) where a second
//      invocation drifts even though the first looks fine.
//
//   2. CROSS-CONFIG HASH: emit a stable FNV-1a hash of the layout to
//      stdout (also stored in the test result JSON). The CI harness
//      diffs the hash across vs2022_Debug_Win64_False and
//      vs2022_Release_Win64_False -- if the hashes differ, procgen is
//      non-deterministic and the cross-config gameplay tests are
//      effectively testing two different games.
//
// The previous generator (pre-rewrite) failed (2) immediately: 48 walls
// in Debug, 47 in Release for seed = 0. The rewrite (integer-coord
// internals + explicit FP-conversion pin) closes that gap.
//
// Seeds tested: 0, 1, 12345, 0xFFFFFFFF. Multiple seeds catch cases
// where one specific layout happens to be deterministic by accident
// (e.g., no near-corner door points to flip a classification).
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	// FNV-1a hash of a LevelLayout. FIELD-BY-FIELD on every entry (NOT
	// raw memcpy of the struct) so we don't accidentally hash padding
	// bytes. Several DP types (e.g., GameElement starts with a uint8_t
	// enum followed by floats) have 3-7 bytes of padding that are
	// uninitialised in stack copies; hashing them would produce
	// false-positive "non-deterministic" verdicts.
	uint64_t HashLayout(const DPProcLevel::LevelLayout& xL)
	{
		uint64_t h = 0xcbf29ce484222325ull;
		auto Bytes = [&h](const void* p, size_t n)
		{
			const uint8_t* b = static_cast<const uint8_t*>(p);
			for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 0x100000001b3ull; }
		};
		auto HashF = [&Bytes](float v)    { Bytes(&v, sizeof(v)); };
		auto HashU = [&Bytes](uint32_t v) { Bytes(&v, sizeof(v)); };
		auto HashI = [&Bytes](int32_t v)  { Bytes(&v, sizeof(v)); };
		auto HashB = [&Bytes](bool v)     { uint8_t x = v ? 1 : 0; Bytes(&x, 1); };
		auto HashE = [&Bytes](DPProcLevel::GameElementType v) { uint8_t x = static_cast<uint8_t>(v); Bytes(&x, 1); };

		HashU(xL.axRooms.GetSize());
		for (uint32_t i = 0; i < xL.axRooms.GetSize(); ++i)
		{
			const auto& r = xL.axRooms.Get(i);
			HashI(r.id); HashF(r.fCentreX); HashF(r.fCentreZ);
			HashF(r.fHalfExtentX); HashF(r.fHalfExtentZ); HashF(r.fYawRadians);
		}

		HashU(xL.axDoorPoints.GetSize());
		for (uint32_t i = 0; i < xL.axDoorPoints.GetSize(); ++i)
		{
			const auto& d = xL.axDoorPoints.Get(i);
			// 2026-05-25: fWallYawRadians is the new integer-derived
			// wall yaw -- needs hashing for both within-config determinism
			// AND cross-config (Debug/Release) parity.
			HashF(d.fX); HashF(d.fZ); HashI(d.xRoomId); HashF(d.fWallYawRadians);
		}

		HashU(xL.axCorridors.GetSize());
		for (uint32_t i = 0; i < xL.axCorridors.GetSize(); ++i)
		{
			const auto& c = xL.axCorridors.Get(i);
			HashI(c.iDoorA); HashI(c.iDoorB);
		}

		HashU(xL.axWallSegments.GetSize());
		for (uint32_t i = 0; i < xL.axWallSegments.GetSize(); ++i)
		{
			const auto& w = xL.axWallSegments.Get(i);
			HashF(w.fCentreX); HashF(w.fCentreZ);
			HashF(w.fHalfExtentX); HashF(w.fHalfExtentZ); HashF(w.fYawRadians);
		}

		HashU(xL.axGameElements.GetSize());
		for (uint32_t i = 0; i < xL.axGameElements.GetSize(); ++i)
		{
			const auto& e = xL.axGameElements.Get(i);
			// 2026-05-25: pre-existing gap -- fYawRadians was tracked
			// in the struct but never hashed. Now hashed alongside the
			// new bDoorLocked so determinism comparisons cover door
			// orientation + lock state.
			HashE(e.eType); HashF(e.fX); HashF(e.fZ); HashI(e.xRoomId); HashI(e.iCorridorId);
			HashF(e.fYawRadians); HashB(e.bDoorLocked);
		}

		HashU(xL.axVillagerSpawns.GetSize());
		for (uint32_t i = 0; i < xL.axVillagerSpawns.GetSize(); ++i)
		{
			const auto& v = xL.axVillagerSpawns.Get(i);
			HashF(v.fX); HashF(v.fZ); HashF(v.fYawRadians); HashI(v.xRoomId);
		}

		HashF(xL.xPriestSpawn.fX); HashF(xL.xPriestSpawn.fZ);
		HashF(xL.xPriestSpawn.fYawRadians); HashI(xL.xPriestSpawn.xRoomId);
		HashB(xL.xPriestSpawn.bValid);

		HashU(xL.axPatrolNodes.GetSize());
		for (uint32_t i = 0; i < xL.axPatrolNodes.GetSize(); ++i)
		{
			const auto& p = xL.axPatrolNodes.Get(i);
			HashF(p.fX); HashF(p.fZ); HashI(p.xRoomId);
		}

		return h;
	}

	bool RunDeterminismCheckForSeed(uint64_t uSeed,
	                                const DPProcLevel::GenConfig& xCfg,
	                                uint64_t& uHashOut)
	{
		DPProcLevel::LevelLayout L1, L2;
		if (!DPProcLevel::Generate(uSeed, xCfg, L1))
		{
			g_szFailureReason = "Generate() returned false on first call";
			return false;
		}
		if (!DPProcLevel::Generate(uSeed, xCfg, L2))
		{
			g_szFailureReason = "Generate() returned false on second call";
			return false;
		}
		const uint64_t h1 = HashLayout(L1);
		const uint64_t h2 = HashLayout(L2);
		uHashOut = h1;
		if (h1 != h2)
		{
			g_szFailureReason = "Repeat Generate() produced different layouts";
			return false;
		}
		// Sanity: layout shouldn't be empty (would mean Generate succeeded
		// but produced nothing).
		if (L1.axRooms.GetSize() == 0u || L1.axWallSegments.GetSize() == 0u)
		{
			g_szFailureReason = "Layout had zero rooms or walls";
			return false;
		}
		return true;
	}
}

static void Setup_ProcLevelDeterminismCheck()
{
	g_bPassed = false;
	g_szFailureReason = "";
}

static bool Step_ProcLevelDeterminismCheck(int iFrame)
{
	if (iFrame > 0) return false;

	// Match the procgen bootstrap's wall-thickness override so the test
	// covers the actual at-runtime config the procgen scene uses.
	DPProcLevel::GenConfig xCfg;
	xCfg.fWallHalfThickness = 0.4f;

	const uint64_t aSeeds[] = { 0ull, 1ull, 12345ull, 0xFFFFFFFFull };
	for (size_t i = 0; i < sizeof(aSeeds)/sizeof(aSeeds[0]); ++i)
	{
		const uint64_t uSeed = aSeeds[i];
		uint64_t uHash = 0;
		if (!RunDeterminismCheckForSeed(uSeed, xCfg, uHash))
		{
			std::printf("[Test_ProcLevel_DeterminismCheck] FAIL seed=%llu reason=%s\n",
				static_cast<unsigned long long>(uSeed), g_szFailureReason);
			std::fflush(stdout);
			return false;
		}
		// Emit the hash so the CI harness can diff across configs. The
		// hashes for the same seed MUST match in Debug + Release for
		// procgen to be cross-config deterministic.
		std::printf("[ProcGenHash] seed=%llu wall_thickness=%.3f hash=0x%016llx\n",
			static_cast<unsigned long long>(uSeed),
			xCfg.fWallHalfThickness,
			static_cast<unsigned long long>(uHash));
		std::fflush(stdout);
	}
	g_bPassed = true;
	return false;
}

static bool Verify_ProcLevelDeterminismCheck()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"Test_ProcLevel_DeterminismCheck: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xProcLevelDeterminismCheckTest = {
	"Test_ProcLevel_DeterminismCheck",
	&Setup_ProcLevelDeterminismCheck,
	&Step_ProcLevelDeterminismCheck,
	&Verify_ProcLevelDeterminismCheck,
	10
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xProcLevelDeterminismCheckTest);

// ============================================================================
// Test_ProcLevel_RetriesUnsolvableSeed (D3)
//
// Seed 0 was historically unsolvable (Docs/Shortfalls.md excluded it from the
// canonical matrix). DPProcLevel::Generate now retries on a deterministically-
// derived seed until the layout is solvable, so Generate(0) must return true
// AND produce a layout that passes the public IsLayoutSolvable oracle.
// ============================================================================
static bool g_bRetryPassed = false;
static const char* g_szRetryReason = "";

static void Setup_ProcLevelRetriesUnsolvableSeed()
{
	g_bRetryPassed = false;
	g_szRetryReason = "";
}

static bool Step_ProcLevelRetriesUnsolvableSeed(int iFrame)
{
	if (iFrame > 0) return false;

	DPProcLevel::GenConfig xCfg;
	xCfg.fWallHalfThickness = 0.4f;  // match the procgen bootstrap's override
	DPProcLevel::LevelLayout xL;
	if (!DPProcLevel::Generate(0ull, xCfg, xL))
	{
		g_szRetryReason = "Generate(seed=0) returned false";
		return false;
	}
	if (!DPProcLevel::IsLayoutSolvable(xL))
	{
		g_szRetryReason = "Generate(seed=0) produced an unsolvable layout";
		return false;
	}
	g_bRetryPassed = true;
	return false;
}

static bool Verify_ProcLevelRetriesUnsolvableSeed()
{
	if (!g_bRetryPassed)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"Test_ProcLevel_RetriesUnsolvableSeed: %s", g_szRetryReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xProcLevelRetriesUnsolvableSeedTest = {
	"Test_ProcLevel_RetriesUnsolvableSeed",
	&Setup_ProcLevelRetriesUnsolvableSeed,
	&Step_ProcLevelRetriesUnsolvableSeed,
	&Verify_ProcLevelRetriesUnsolvableSeed,
	10
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xProcLevelRetriesUnsolvableSeedTest);

#endif // ZENITH_INPUT_SIMULATOR
