#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DPProcLevel/DPProcLevel_Generator.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

// ============================================================================
// Test_ProcLevel_BuildingWallClosure -- regression for the corner-gap bug.
//
// EmitWallSegments_I emits each building as the four edges of a rotated
// rectangle, each edge a wall rectangle that runs from outer corner to
// outer corner -- length = 2*(axisMax + halfThick), thickness =
// 2*halfThick. Adjacent perpendicular walls overlap in a
// halfThick x halfThick square at every room corner. Door gaps are
// clipped to the room edge [-axisMax, +axisMax], so the halfThick
// corner-extension strips at each end of every wall always survive
// regardless of door position. Every corner is closed.
//
// This test probes the centre of each former gap square -- a point
// (halfThick/2) outside each room corner along both edge normals --
// and asserts the probe lies inside at least one emitted wall's OBB.
// Any uncovered corner is a regression in the wall emitter.
// Runs across the canonical 10-seed set (same list as the seed-matrix
// runner; see Games/DevilsPlayground/Tests/CLAUDE.md).
// ============================================================================

namespace
{
	bool        g_bPassed         = false;
	const char* g_szFailureReason = "";
	uint64_t    g_uFailureSeed    = 0ull;
	int32_t     g_iFailureRoom    = -1;
	int32_t     g_iFailureCorner  = -1;
	uint32_t    g_uCheckedCount   = 0;

	// Point-in-OBB against a wall stored as (centre, half-extents, yaw).
	// The generator stores Z-edge walls with swapped extents under the
	// same yaw as the room, so the wall's stored yaw already aligns its
	// local frame -- no 90-degree offset needed.
	bool PointInsideWall(float fPx, float fPz, const DPProcLevel::WallSegment& xW)
	{
		const float fDx   = fPx - xW.fCentreX;
		const float fDz   = fPz - xW.fCentreZ;
		const float fCos  = std::cos(xW.fYawRadians);
		const float fSin  = std::sin(xW.fYawRadians);
		// Inverse rotation: room/wall convention is R_y where
		//   world.x = local.x*cos + local.z*sin
		//   world.z = -local.x*sin + local.z*cos
		// So the inverse is local.x = world.x*cos - world.z*sin,
		// local.z = world.x*sin + world.z*cos.
		const float fLx   = fDx * fCos - fDz * fSin;
		const float fLz   = fDx * fSin + fDz * fCos;
		const float fAbsX = std::fabs(fLx);
		const float fAbsZ = std::fabs(fLz);
		return fAbsX <= xW.fHalfExtentX && fAbsZ <= xW.fHalfExtentZ;
	}

	bool PointInsideAnyWall(float fPx, float fPz,
	                        const Zenith_Vector<DPProcLevel::WallSegment>& axWalls)
	{
		const uint32_t uN = axWalls.GetSize();
		for (uint32_t i = 0; i < uN; ++i)
		{
			if (PointInsideWall(fPx, fPz, axWalls.Get(i))) return true;
		}
		return false;
	}

	bool Fail(const char* szReason, uint64_t uSeed, int32_t iRoom, int32_t iCorner)
	{
		g_szFailureReason = szReason;
		g_uFailureSeed    = uSeed;
		g_iFailureRoom    = iRoom;
		g_iFailureCorner  = iCorner;
		return false;
	}

	bool RunOneSeed(uint64_t uSeed)
	{
		DPProcLevel::GenConfig xCfg;
		// Mirror the runtime override applied by
		// DPProcLevelBootstrap_Behaviour.h so the test exercises the
		// same wall thickness the player sees.
		xCfg.fWallHalfThickness = 0.4f;

		DPProcLevel::LevelLayout xLayout;
		if (!DPProcLevel::Generate(uSeed, xCfg, xLayout))
			return Fail("Generate returned false", uSeed, -1, -1);

		const float fHalfThick = xCfg.fWallHalfThickness;
		const float fProbe     = fHalfThick * 0.5f;

		const uint32_t uRooms = xLayout.axRooms.GetSize();
		for (uint32_t uR = 0; uR < uRooms; ++uR)
		{
			const DPProcLevel::Room& xR = xLayout.axRooms.Get(uR);

			// Four outside-corner probes in room-local coordinates.
			// Each probe sits halfThick/2 outside the room corner along
			// BOTH edge normals -- the centre of the former gap square.
			const float fLx = xR.fHalfExtentX + fProbe;
			const float fLz = xR.fHalfExtentZ + fProbe;
			const float aaLocal[4][2] = {
				{ -fLx, -fLz },   // 0: bottom-left
				{ +fLx, -fLz },   // 1: bottom-right
				{ +fLx, +fLz },   // 2: top-right
				{ -fLx, +fLz },   // 3: top-left
			};

			const float fCos = std::cos(xR.fYawRadians);
			const float fSin = std::sin(xR.fYawRadians);

			for (int32_t iC = 0; iC < 4; ++iC)
			{
				const float fLocalX = aaLocal[iC][0];
				const float fLocalZ = aaLocal[iC][1];
				// Forward R_y rotation: world.x = lx*cos + lz*sin,
				// world.z = -lx*sin + lz*cos.
				const float fWorldX = xR.fCentreX + (fLocalX * fCos + fLocalZ * fSin);
				const float fWorldZ = xR.fCentreZ + (-fLocalX * fSin + fLocalZ * fCos);

				++g_uCheckedCount;
				if (!PointInsideAnyWall(fWorldX, fWorldZ, xLayout.axWallSegments))
				{
					return Fail("external room corner not covered by any wall",
						uSeed, static_cast<int32_t>(uR), iC);
				}
			}
		}

		std::printf("[ProcLevelBuildingWallClosure] seed=%llu rooms=%u walls=%u OK\n",
			static_cast<unsigned long long>(uSeed),
			uRooms, xLayout.axWallSegments.GetSize());
		std::fflush(stdout);
		return true;
	}
}

static void Setup_ProcLevelBuildingWallClosure()
{
	g_bPassed         = false;
	g_szFailureReason = "";
	g_uFailureSeed    = 0ull;
	g_iFailureRoom    = -1;
	g_iFailureCorner  = -1;
	g_uCheckedCount   = 0;

	// Canonical 10-seed set (ratified 2026-05-22 balance pass; matches
	// the list used by Tools/dp_seed_matrix_run.ps1). Seed 0 is included
	// because the corner-coverage property still holds even though
	// seed 0's procgen layout is unsolvable.
	const uint64_t auSeeds[] = {
		0ull, 1ull, 5ull, 7ull, 42ull, 100ull,
		12345ull, 55555ull, 99999ull, 250000ull, 4276994270ull
	};
	for (uint64_t uSeed : auSeeds)
	{
		if (!RunOneSeed(uSeed)) return;
	}

	std::printf("[ProcLevelBuildingWallClosure] checked=%u all corners covered\n",
		g_uCheckedCount);
	std::fflush(stdout);

	g_bPassed = true;
}

static bool Step_ProcLevelBuildingWallClosure(int /*iFrame*/) { return false; }

static bool Verify_ProcLevelBuildingWallClosure()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"ProcLevelBuildingWallClosure: FAIL seed=%llu room=%d corner=%d %s",
			static_cast<unsigned long long>(g_uFailureSeed),
			g_iFailureRoom, g_iFailureCorner, g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xProcLevelBuildingWallClosureTest = {
	"Test_ProcLevel_BuildingWallClosure",
	&Setup_ProcLevelBuildingWallClosure,
	&Step_ProcLevelBuildingWallClosure,
	&Verify_ProcLevelBuildingWallClosure,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xProcLevelBuildingWallClosureTest);

#endif // ZENITH_INPUT_SIMULATOR
