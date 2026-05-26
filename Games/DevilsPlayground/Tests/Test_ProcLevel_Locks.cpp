#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_LevelLayout.h"

#include <cstdio>

// ============================================================================
// Test_ProcLevel_Locks -- P0 unit tests for the door-locking + iron-auto-scale
// contracts added in the 2026-05-25 "doors at DoorPoints" overhaul.
//
//   Test_ProcLevel_LockedFractionRespected
//     fDoorLockedFraction = 0.0 -> only pentagram-side doors are locked.
//     fDoorLockedFraction = 0.5 -> extra non-pentagram-side doors get locked
//                                  (deterministic, seed-pinned, not statistical).
//
//   Test_ProcLevel_IronCountMatchesLocks
//     ironCount == max(lockedDoorCount, 1) -- the safety floor handles the
//     edge case of zero pentagram-incident corridors (artificial test config).
// ============================================================================

namespace
{
	uint32_t CountElement(const DPProcLevel::LevelLayout& xL, DPProcLevel::GameElementType eType)
	{
		uint32_t u = 0;
		for (uint32_t i = 0; i < xL.axGameElements.GetSize(); ++i)
			if (xL.axGameElements.Get(i).eType == eType) ++u;
		return u;
	}

	uint32_t CountLockedDoors(const DPProcLevel::LevelLayout& xL)
	{
		uint32_t u = 0;
		for (uint32_t i = 0; i < xL.axGameElements.GetSize(); ++i)
		{
			const auto& xE = xL.axGameElements.Get(i);
			if (xE.eType == DPProcLevel::GameElementType::Door && xE.bDoorLocked) ++u;
		}
		return u;
	}

	uint32_t CountPentagramIncidentCorridors(const DPProcLevel::LevelLayout& xL)
	{
		DPProcLevel::RoomId xPent = DPProcLevel::kInvalidRoomId;
		for (uint32_t i = 0; i < xL.axGameElements.GetSize(); ++i)
		{
			if (xL.axGameElements.Get(i).eType == DPProcLevel::GameElementType::Pentagram)
			{
				xPent = xL.axGameElements.Get(i).xRoomId;
				break;
			}
		}
		uint32_t u = 0;
		for (uint32_t iC = 0; iC < xL.axCorridors.GetSize(); ++iC)
		{
			const auto& xC = xL.axCorridors.Get(iC);
			const auto xA = xL.axDoorPoints.Get(xC.iDoorA).xRoomId;
			const auto xB = xL.axDoorPoints.Get(xC.iDoorB).xRoomId;
			if (xA == xPent || xB == xPent) ++u;
		}
		return u;
	}
}

// -------------------------------------------------------------------------
// Test_ProcLevel_LockedFractionRespected
// -------------------------------------------------------------------------
namespace LockedFractionState
{
	bool g_bPass = false;
	const char* g_szReason = "not run";
}

static void Setup_LockedFractionRespected() { LockedFractionState::g_bPass = false; LockedFractionState::g_szReason = "not run"; }

static bool Step_LockedFractionRespected(int /*iFrame*/)
{
	using namespace LockedFractionState;
	constexpr uint64_t kSeed = 12345ull;

	// 1) fraction = 0 -> locked-count == pentagram-incident corridor count.
	DPProcLevel::GenConfig xCfgZero;
	xCfgZero.fDoorLockedFraction = 0.0f;
	DPProcLevel::LevelLayout xLZero;
	if (!DPProcLevel::Generate(kSeed, xCfgZero, xLZero))
	{
		g_szReason = "Generate failed for fraction=0";
		return false;
	}
	const uint32_t uLockedZero = CountLockedDoors(xLZero);
	const uint32_t uPentCorridorsZero = CountPentagramIncidentCorridors(xLZero);
	if (uLockedZero != uPentCorridorsZero)
	{
		std::printf("[LockedFractionRespected] fraction=0 mismatch: locked=%u pentCorridors=%u\n",
			uLockedZero, uPentCorridorsZero);
		std::fflush(stdout);
		g_szReason = "fraction=0 lockedDoorCount != pentagramIncidentCorridorCount";
		return false;
	}

	// 2) fraction = 0.5 -> pin to the exact value the lock-pass RNG
	//    produces for seed 12345 (captured 2026-05-25, post-PR matrix run:
	//    16 locked doors total, 2 pentagram-side + 14 extras). Pinning
	//    an exact count rather than just `>baseline` catches subtle
	//    regressions in the lock-pass RNG stream (changing the
	//    `^ 0xD009B100CCEDD00Aull` salt or replacing the integer-bucket
	//    trick would shift this value), which a `>baseline` check would
	//    miss whenever the regression still happens to land >baseline.
	//    Update the baked value if the lock-pass mechanism is
	//    intentionally retuned.
	constexpr uint32_t kExpectedLockedHalfSeed12345 = 16;
	DPProcLevel::GenConfig xCfgHalf;
	xCfgHalf.fDoorLockedFraction = 0.5f;
	DPProcLevel::LevelLayout xLHalf;
	if (!DPProcLevel::Generate(kSeed, xCfgHalf, xLHalf))
	{
		g_szReason = "Generate failed for fraction=0.5";
		return false;
	}
	const uint32_t uLockedHalf = CountLockedDoors(xLHalf);
	std::printf("[LockedFractionRespected] seed=%llu: fraction=0 -> %u locked; fraction=0.5 -> %u locked (expected %u; %u pentCorridors)\n",
		static_cast<unsigned long long>(kSeed), uLockedZero, uLockedHalf,
		kExpectedLockedHalfSeed12345, uPentCorridorsZero);
	std::fflush(stdout);
	if (uLockedHalf != kExpectedLockedHalfSeed12345)
	{
		g_szReason = "fraction=0.5 locked-count diverged from seed-pinned expectation";
		return false;
	}

	// 3) The pentagram-only baseline locks must still be present at 0.5
	//    (the extra-lock pass only ADDS, never removes).
	uint32_t uPentSideAtHalf = 0;
	DPProcLevel::RoomId xPent = DPProcLevel::kInvalidRoomId;
	for (uint32_t i = 0; i < xLHalf.axGameElements.GetSize(); ++i)
	{
		if (xLHalf.axGameElements.Get(i).eType == DPProcLevel::GameElementType::Pentagram)
		{ xPent = xLHalf.axGameElements.Get(i).xRoomId; break; }
	}
	for (uint32_t i = 0; i < xLHalf.axGameElements.GetSize(); ++i)
	{
		const auto& xE = xLHalf.axGameElements.Get(i);
		if (xE.eType != DPProcLevel::GameElementType::Door) continue;
		if (xE.bDoorLocked && xE.xRoomId == xPent) ++uPentSideAtHalf;
	}
	if (uPentSideAtHalf != uPentCorridorsZero)
	{
		g_szReason = "fraction=0.5 lost pentagram-side locks";
		return false;
	}

	g_bPass = true;
	g_szReason = "ok";
	return false;
}

static bool Verify_LockedFractionRespected() { return LockedFractionState::g_bPass; }

static const Zenith_AutomatedTest g_xLockedFractionRespectedTest = {
	"Test_ProcLevel_LockedFractionRespected",
	&Setup_LockedFractionRespected,
	&Step_LockedFractionRespected,
	&Verify_LockedFractionRespected,
	/*maxFrames*/ 2,
	/*requiresGraphics*/ false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xLockedFractionRespectedTest);

// -------------------------------------------------------------------------
// Test_ProcLevel_IronCountMatchesLocks
// -------------------------------------------------------------------------
namespace IronCountState
{
	bool g_bPass = false;
	const char* g_szReason = "not run";
}

static void Setup_IronCountMatchesLocks() { IronCountState::g_bPass = false; IronCountState::g_szReason = "not run"; }

static bool Step_IronCountMatchesLocks(int /*iFrame*/)
{
	using namespace IronCountState;

	// Iron count must equal max(lockedDoorCount, 1) across both default
	// (fraction=0) and elevated (fraction=0.5) tuning -- the safety floor
	// kicks in only when lockedDoorCount==0, which doesn't happen on the
	// canonical seeds (every layout has >= 1 pentagram-incident corridor).
	const uint64_t aSeeds[] = { 12345ull, 99999ull, 42ull };
	const float    aFractions[] = { 0.0f, 0.5f };

	for (uint64_t uSeed : aSeeds)
	{
		for (float fFrac : aFractions)
		{
			DPProcLevel::GenConfig xCfg;
			xCfg.fDoorLockedFraction = fFrac;
			DPProcLevel::LevelLayout xL;
			if (!DPProcLevel::Generate(uSeed, xCfg, xL))
			{
				g_szReason = "Generate failed";
				return false;
			}
			const uint32_t uLocked = CountLockedDoors(xL);
			const uint32_t uIron   = CountElement(xL, DPProcLevel::GameElementType::Iron);
			const uint32_t uExpected = (uLocked > 0u) ? uLocked : 1u;
			if (uIron != uExpected)
			{
				std::printf("[IronCountMatchesLocks] seed=%llu frac=%.2f locked=%u iron=%u expected=%u\n",
					static_cast<unsigned long long>(uSeed), static_cast<double>(fFrac),
					uLocked, uIron, uExpected);
				std::fflush(stdout);
				g_szReason = "iron count != max(lockedDoorCount, 1)";
				return false;
			}
		}
	}

	g_bPass = true;
	g_szReason = "ok";
	return false;
}

static bool Verify_IronCountMatchesLocks() { return IronCountState::g_bPass; }

static const Zenith_AutomatedTest g_xIronCountMatchesLocksTest = {
	"Test_ProcLevel_IronCountMatchesLocks",
	&Setup_IronCountMatchesLocks,
	&Step_IronCountMatchesLocks,
	&Verify_IronCountMatchesLocks,
	/*maxFrames*/ 2,
	/*requiresGraphics*/ false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xIronCountMatchesLocksTest);

#endif // ZENITH_INPUT_SIMULATOR
