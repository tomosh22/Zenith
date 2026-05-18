#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DPProcLevel/DPProcLevel_Generator.h"
#include "Source/DPProcLevel/DPProcLevel_JsonExport.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

// ============================================================================
// Test_ProcLevel_BSP -- P0 unit tests for the BSP layout generator.
//
// Pins the generator's contracts at the data level:
//   1. Determinism: Generate(seed=N) twice produces byte-identical
//      LevelLayouts (room count + every room's centre/extent/yaw + every
//      door point + every corridor).
//   2. Bounds: every room's OBB centre sits inside the config bounds and
//      the room's far corners do not exceed the bounds by more than a
//      tolerance margin.
//   3. Connectivity: starting from room 0, BFS over the corridor graph
//      reaches every other room (no orphaned partitions).
//   4. No-overlap: applies the OBB SAT check (mirrored in the generator)
//      to every pair of rooms and asserts no intersection.
//
// As a side effect, the test emits a layout JSON per seed under
// %TEMP%/dp_proclevel_seed_<N>.json for the PowerShell visualiser to
// pick up. That's the "look at the PNG" half of the dev loop -- the
// test itself only verifies the contract, the visualiser shows what
// the generator's actually producing.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";
	int  g_iFailureSeed   = -1;

	// Mirror of the generator's OBBsOverlap for the no-overlap test.
	// Kept private to the test so the generator's helper can be tweaked
	// without silently breaking the verification, and so any divergence
	// surfaces here as a build break or a failing assertion.
	bool OBBsOverlap(const DPProcLevel::Room& xA, const DPProcLevel::Room& xB)
	{
		const float fCosA = std::cos(xA.fYawRadians);
		const float fSinA = std::sin(xA.fYawRadians);
		const float fCosB = std::cos(xB.fYawRadians);
		const float fSinB = std::sin(xB.fYawRadians);

		const float fA0x = fCosA;  const float fA0z = -fSinA;
		const float fA1x = fSinA;  const float fA1z = fCosA;
		const float fB0x = fCosB;  const float fB0z = -fSinB;
		const float fB1x = fSinB;  const float fB1z = fCosB;

		const float fDx = xB.fCentreX - xA.fCentreX;
		const float fDz = xB.fCentreZ - xA.fCentreZ;

		auto IsSeparating = [&](float nx, float nz) -> bool
		{
			const float fAProj =
				xA.fHalfExtentX * std::fabs(fA0x * nx + fA0z * nz) +
				xA.fHalfExtentZ * std::fabs(fA1x * nx + fA1z * nz);
			const float fBProj =
				xB.fHalfExtentX * std::fabs(fB0x * nx + fB0z * nz) +
				xB.fHalfExtentZ * std::fabs(fB1x * nx + fB1z * nz);
			const float fCentreProj = std::fabs(fDx * nx + fDz * nz);
			return fCentreProj > (fAProj + fBProj);
		};

		if (IsSeparating(fA0x, fA0z)) return false;
		if (IsSeparating(fA1x, fA1z)) return false;
		if (IsSeparating(fB0x, fB0z)) return false;
		if (IsSeparating(fB1x, fB1z)) return false;
		return true;
	}

	bool LayoutsIdentical(const DPProcLevel::LevelLayout& xA,
	                      const DPProcLevel::LevelLayout& xB)
	{
		if (xA.uSeed                       != xB.uSeed)                       return false;
		if (xA.axRooms.GetSize()           != xB.axRooms.GetSize())           return false;
		if (xA.axDoorPoints.GetSize()      != xB.axDoorPoints.GetSize())      return false;
		if (xA.axCorridors.GetSize()       != xB.axCorridors.GetSize())       return false;
		if (xA.axWallSegments.GetSize()    != xB.axWallSegments.GetSize())    return false;
		for (uint32_t i = 0; i < xA.axWallSegments.GetSize(); ++i)
		{
			const auto& wA = xA.axWallSegments.Get(i);
			const auto& wB = xB.axWallSegments.Get(i);
			if (wA.fCentreX != wB.fCentreX) return false;
			if (wA.fCentreZ != wB.fCentreZ) return false;
			if (wA.fHalfExtentX != wB.fHalfExtentX) return false;
			if (wA.fHalfExtentZ != wB.fHalfExtentZ) return false;
			if (wA.fYawRadians != wB.fYawRadians) return false;
		}
		for (uint32_t i = 0; i < xA.axRooms.GetSize(); ++i)
		{
			const auto& rA = xA.axRooms.Get(i);
			const auto& rB = xB.axRooms.Get(i);
			if (rA.id != rB.id) return false;
			if (rA.fCentreX     != rB.fCentreX)     return false;
			if (rA.fCentreZ     != rB.fCentreZ)     return false;
			if (rA.fHalfExtentX != rB.fHalfExtentX) return false;
			if (rA.fHalfExtentZ != rB.fHalfExtentZ) return false;
			if (rA.fYawRadians  != rB.fYawRadians)  return false;
		}
		for (uint32_t i = 0; i < xA.axDoorPoints.GetSize(); ++i)
		{
			const auto& dA = xA.axDoorPoints.Get(i);
			const auto& dB = xB.axDoorPoints.Get(i);
			if (dA.fX != dB.fX) return false;
			if (dA.fZ != dB.fZ) return false;
			if (dA.xRoomId != dB.xRoomId) return false;
		}
		for (uint32_t i = 0; i < xA.axCorridors.GetSize(); ++i)
		{
			const auto& cA = xA.axCorridors.Get(i);
			const auto& cB = xB.axCorridors.Get(i);
			if (cA.iDoorA != cB.iDoorA) return false;
			if (cA.iDoorB != cB.iDoorB) return false;
		}
		return true;
	}

	bool LayoutConnected(const DPProcLevel::LevelLayout& xLayout)
	{
		const uint32_t uN = xLayout.axRooms.GetSize();
		if (uN == 0) return true;
		// BFS from room 0 over the corridor graph (corridors connect
		// rooms via door points). Build adjacency on the fly.
		Zenith_Vector<bool> abVisited;
		for (uint32_t i = 0; i < uN; ++i) abVisited.PushBack(false);
		abVisited.Get(0) = true;
		Zenith_Vector<int32_t> axFrontier;
		axFrontier.PushBack(0);
		uint32_t uReached = 1;
		while (axFrontier.GetSize() > 0)
		{
			const int32_t iCur = axFrontier.Get(axFrontier.GetSize() - 1);
			axFrontier.Remove(axFrontier.GetSize() - 1);
			for (uint32_t i = 0; i < xLayout.axCorridors.GetSize(); ++i)
			{
				const auto& xC = xLayout.axCorridors.Get(i);
				const int32_t xRoomA = xLayout.axDoorPoints.Get(xC.iDoorA).xRoomId;
				const int32_t xRoomB = xLayout.axDoorPoints.Get(xC.iDoorB).xRoomId;
				int32_t iOther = -1;
				if      (xRoomA == iCur) iOther = xRoomB;
				else if (xRoomB == iCur) iOther = xRoomA;
				if (iOther < 0) continue;
				if (abVisited.Get(static_cast<uint32_t>(iOther))) continue;
				abVisited.Get(static_cast<uint32_t>(iOther)) = true;
				axFrontier.PushBack(iOther);
				++uReached;
			}
		}
		return uReached == uN;
	}

	bool LayoutInBounds(const DPProcLevel::LevelLayout& xLayout)
	{
		const float kTol = 0.001f;
		// Project each room's 4 corners through its rotation matrix and
		// check those world-space points stay inside the bounds. Using
		// the bounding-circle radius as a conservative check would
		// reject perfectly-fitting tall-thin rooms whose long axis
		// happens to align parallel to the boundary.
		for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i)
		{
			const auto& xR = xLayout.axRooms.Get(i);
			const float fCos = std::cos(xR.fYawRadians);
			const float fSin = std::sin(xR.fYawRadians);
			const float aLocal[4][2] = {
				{ -xR.fHalfExtentX, -xR.fHalfExtentZ },
				{  xR.fHalfExtentX, -xR.fHalfExtentZ },
				{  xR.fHalfExtentX,  xR.fHalfExtentZ },
				{ -xR.fHalfExtentX,  xR.fHalfExtentZ },
			};
			for (uint32_t k = 0; k < 4; ++k)
			{
				const float fLx = aLocal[k][0];
				const float fLz = aLocal[k][1];
				const float fWx = xR.fCentreX + fLx * fCos + fLz * fSin;
				const float fWz = xR.fCentreZ - fLx * fSin + fLz * fCos;
				if (fWx < xLayout.fBoundsMinX - kTol) return false;
				if (fWz < xLayout.fBoundsMinZ - kTol) return false;
				if (fWx > xLayout.fBoundsMaxX + kTol) return false;
				if (fWz > xLayout.fBoundsMaxZ + kTol) return false;
			}
		}
		return true;
	}

	bool LayoutNoOverlap(const DPProcLevel::LevelLayout& xLayout)
	{
		for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i)
		{
			for (uint32_t j = i + 1; j < xLayout.axRooms.GetSize(); ++j)
			{
				if (OBBsOverlap(xLayout.axRooms.Get(i), xLayout.axRooms.Get(j)))
					return false;
			}
		}
		return true;
	}

	std::string TempPath(uint64_t uSeed)
	{
		std::error_code xErr;
		std::filesystem::path xDir = std::filesystem::temp_directory_path(xErr);
		if (xErr) xDir = ".";
		char buf[64];
		std::snprintf(buf, sizeof(buf), "dp_proclevel_seed_%llu.json",
			static_cast<unsigned long long>(uSeed));
		xDir /= buf;
		return xDir.string();
	}

	bool Fail(const char* sz, int iSeed)
	{
		g_szFailureReason = sz;
		g_iFailureSeed    = iSeed;
		return false;
	}

	// Verifies the contract on a single seed AND emits a JSON for the
	// visualiser. Returns false on first failed assertion.
	bool RunOneSeed(uint64_t uSeed)
	{
		DPProcLevel::GenConfig xCfg;  // defaults
		DPProcLevel::LevelLayout xA, xB;

		if (!DPProcLevel::Generate(uSeed, xCfg, xA))
			return Fail("Generate returned false", static_cast<int>(uSeed));
		if (xA.axRooms.GetSize() == 0)
			return Fail("zero rooms generated", static_cast<int>(uSeed));

		// Determinism: run again, expect identical output.
		if (!DPProcLevel::Generate(uSeed, xCfg, xB))
			return Fail("second Generate returned false", static_cast<int>(uSeed));
		if (!LayoutsIdentical(xA, xB))
			return Fail("not deterministic across runs", static_cast<int>(uSeed));

		if (!LayoutInBounds(xA))
			return Fail("room exits world bounds", static_cast<int>(uSeed));
		if (!LayoutNoOverlap(xA))
			return Fail("rooms overlap", static_cast<int>(uSeed));
		if (!LayoutConnected(xA))
			return Fail("corridor graph is disconnected", static_cast<int>(uSeed));

		// P1 wall-emission invariants:
		//   * Each room contributes >= 4 wall segments BEFORE doors are
		//     subtracted (one per edge). With door gaps, some edges
		//     might be fully removed only in the pathological case
		//     where an edge has multiple wide doors -- not currently
		//     possible because the BSP layout only puts one door per
		//     shared partition edge. So the lower bound is 4*rooms -
		//     doors, conservatively check >= 3*rooms which covers any
		//     reasonable splitting.
		//   * No degenerate (zero-half-extent) walls.
		const uint32_t uExpectedMinWalls = 3u * xA.axRooms.GetSize();
		if (xA.axWallSegments.GetSize() < uExpectedMinWalls)
			return Fail("too few wall segments emitted", static_cast<int>(uSeed));
		for (uint32_t i = 0; i < xA.axWallSegments.GetSize(); ++i)
		{
			const auto& xW = xA.axWallSegments.Get(i);
			if (xW.fHalfExtentX < 0.001f || xW.fHalfExtentZ < 0.001f)
				return Fail("degenerate wall segment", static_cast<int>(uSeed));
		}

		// P2 game-element invariants: exactly one of each type (except
		// objectives, which come in 5), and the solvability check
		// validated by the generator itself (re-run here as a guard
		// against regressions in either side).
		uint32_t auCounts[12] = {0};
		for (uint32_t i = 0; i < xA.axGameElements.GetSize(); ++i)
		{
			const uint8_t u = static_cast<uint8_t>(xA.axGameElements.Get(i).eType);
			if (u < 12u) auCounts[u]++;
		}
		const uint8_t kSpawn   = static_cast<uint8_t>(DPProcLevel::GameElementType::SpawnPoint);
		const uint8_t kPent    = static_cast<uint8_t>(DPProcLevel::GameElementType::Pentagram);
		const uint8_t kForge   = static_cast<uint8_t>(DPProcLevel::GameElementType::Forge);
		const uint8_t kDoor    = static_cast<uint8_t>(DPProcLevel::GameElementType::Door);
		const uint8_t kChest   = static_cast<uint8_t>(DPProcLevel::GameElementType::Chest);
		const uint8_t kNoise   = static_cast<uint8_t>(DPProcLevel::GameElementType::NoiseMachine);
		const uint8_t kIron    = static_cast<uint8_t>(DPProcLevel::GameElementType::Iron);
		const uint8_t kObj1    = static_cast<uint8_t>(DPProcLevel::GameElementType::Objective1);
		const uint8_t kObj5    = static_cast<uint8_t>(DPProcLevel::GameElementType::Objective5);
		if (auCounts[kSpawn] != 1u) return Fail("expected exactly 1 SpawnPoint",   static_cast<int>(uSeed));
		if (auCounts[kPent]  != 1u) return Fail("expected exactly 1 Pentagram",    static_cast<int>(uSeed));
		if (auCounts[kForge] != 1u) return Fail("expected exactly 1 Forge",        static_cast<int>(uSeed));
		// Pentagram has 1..N incident corridors -- each gets its own door
		// so the bot can't bypass the puzzle via an unlocked alternate path.
		// Any count >= 1 is acceptable.
		if (auCounts[kDoor]  <  1u) return Fail("expected >= 1 Door",              static_cast<int>(uSeed));
		if (auCounts[kChest] != 1u) return Fail("expected exactly 1 Chest",        static_cast<int>(uSeed));
		if (auCounts[kNoise] != 1u) return Fail("expected exactly 1 NoiseMachine", static_cast<int>(uSeed));
		if (auCounts[kIron]  != 1u) return Fail("expected exactly 1 Iron",         static_cast<int>(uSeed));
		for (uint8_t u = kObj1; u <= kObj5; ++u)
		{
			if (auCounts[u] != 1u) return Fail("expected exactly 1 of each Objective", static_cast<int>(uSeed));
		}

		// Emit JSON for the visualiser.
		const std::string strPath = TempPath(uSeed);
		if (!DPProcLevel::ExportLayoutJson(xA, strPath.c_str()))
			return Fail("ExportLayoutJson returned false", static_cast<int>(uSeed));

		std::printf("[ProcLevelBSP] seed=%llu rooms=%u doors=%u corridors=%u walls=%u elements=%u json=%s\n",
			static_cast<unsigned long long>(uSeed),
			xA.axRooms.GetSize(), xA.axDoorPoints.GetSize(),
			xA.axCorridors.GetSize(), xA.axWallSegments.GetSize(),
			xA.axGameElements.GetSize(),
			strPath.c_str());
		std::fflush(stdout);
		return true;
	}
}

static void Setup_ProcLevelBSP()
{
	g_bPassed = false;
	g_szFailureReason = "";
	g_iFailureSeed = -1;

	// Run a handful of seeds covering "default", a few small variations,
	// and one large value to make sure the PRNG handles big seeds.
	const uint64_t auSeeds[] = { 0ull, 1ull, 2ull, 42ull, 0xFEEDC0DEull };
	for (uint64_t uSeed : auSeeds)
	{
		if (!RunOneSeed(uSeed)) return;
	}

	g_bPassed = true;
}

static bool Step_ProcLevelBSP(int /*iFrame*/) { return false; }

static bool Verify_ProcLevelBSP()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"ProcLevelBSP: FAIL seed=%d %s",
			g_iFailureSeed, g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xProcLevelBSPTest = {
	"Test_ProcLevel_BSP",
	&Setup_ProcLevelBSP,
	&Step_ProcLevelBSP,
	&Verify_ProcLevelBSP,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xProcLevelBSPTest);

#endif // ZENITH_INPUT_SIMULATOR
