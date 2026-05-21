#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Source/DPHeuristicBot.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPInputActions.h"

#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPInteractable_Behaviour.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Physics/Zenith_PhysicsImpl.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Windows/Zenith_Windows_Window.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace DPHeuristicBot
{
	// =========================================================
	// Tuning constants. Sized for the current GameLevel scale
	// (~100 m perimeter, 17 villagers, 30 s life budget).
	// =========================================================
	static constexpr float kFleeDistance        = 7.0f;
	static constexpr float kSwapLifeThreshold   = 4.0f;
	static constexpr float kSprintLifeThreshold = 10.0f;
	static constexpr float kPriestSuspiciousDist= 14.0f;
	static constexpr float kStopAtTargetDist    = 1.4f;
	static constexpr float kStuckMoveThresholdSq= 0.25f;   // moved < 0.5 m -> stuck increment
	static constexpr int   kStuckFrameLimit     = 90;       // ~1.5 s @ 60 fps

	// =========================================================
	// Grid-A* pathing (Phase 3b). Mirrors Test_HumanPlaythrough's
	// approach: lazy one-shot walkability grid built via downward
	// raycasts, A* over the grid, waypoint-follow with replan on
	// target drift or stuck-detector trigger.
	//
	// Smaller grid than HumanPlaythrough (60x60 @ 2 m) -- the bot
	// doesn't need 1 m fidelity, and the smaller grid is ~4x faster
	// to build (3600 raycasts vs 14400). Bake cost is one-time.
	// =========================================================
	static constexpr int   kPathGridDim   = 60;
	static constexpr float kPathCellSize  = 2.0f;
	static constexpr float kPathOriginX   = -10.0f;
	static constexpr float kPathOriginZ   = -10.0f;
	static constexpr float kPathFloorY    = 1.0f;
	static constexpr float kPathFloorYTop = 1.5f;
	static constexpr float kWaypointReachedDist = 1.8f;

	// Lazily built. The previous design ("once per process") leaked the
	// grid across scenes when a future caller loaded a different scene
	// after the bot had run on the first -- e.g. multi-scene procgen
	// runs in the same process would route through the OLD scene's
	// walls. Fixed in Phase-5-audit (2026-05-16) by tracking the scene
	// handle the grid was built for + comparing against the active
	// scene on every BuildPathGrid call. If the handle differs we
	// rebuild. If it matches we skip (~3600 raycasts saved).
	//
	// Caveat: if the SAME scene reloads (handle reused, geometry
	// changed via editor automation), the grid stays stale. Reset()
	// clears the cached handle so a `Reset() -> Tick()` cycle forces
	// a fresh build -- the bot's test always does this.
	static bool g_bPathGridBuilt = false;
	static int  g_iPathGridSceneHandle = -1;
	static bool g_abPathWalkable[kPathGridDim * kPathGridDim] = {};
	// Sentinel handle used by TestSurface::SetWalkabilityGridForTest to
	// flag the grid as "test-injected, do NOT rebuild from raycasts".
	// Picked at INT_MIN so it can't collide with any real Zenith_Scene
	// handle (those are small non-negative ints from the scene registry).
	// BuildPathGrid checks for this value and skips its rebuild path.
	static constexpr int kTestInjectedHandle = (-2147483647 - 1); // = INT_MIN, avoid <climits> include

	// Active path. Replanned when target drifts > kReplanIfTargetMoves
	// or when stuck > kStuckFrameLimit. Cleared on Reset().
	static Zenith_Vector<Zenith_Maths::Vector3> g_axCurrentPath;
	static int   g_iPathWaypoint = 0;
	static Zenith_Maths::Vector3 g_xLastPlannedTarget{1e9f, 0.0f, 0.0f};
	static constexpr float kReplanIfTargetMoves = 4.0f; // metres

	// =========================================================
	// Persistent state.
	// =========================================================
	static Goal g_eCurrentGoal = Goal::Idle;
	static Zenith_Maths::Vector3 g_xLastPos{1e9f, 0.0f, 0.0f};
	static int   g_iStuckFrames        = 0;
	static uint32_t g_uSprintFrames    = 0;
	static uint32_t g_uWalkQuietFrames = 0;
	static uint32_t g_uInteractPresses = 0;
	static uint32_t g_uDropPresses     = 0;
	static uint32_t g_uPossessClicks   = 0;
	// Periodic interact / drop pacing. Holding F or G has no effect
	// per the DP input contract (single-frame edge); we throttle so we
	// don't blast presses on every frame.
	static int   g_iLastInteractFrame  = -1000;
	static int   g_iLastDropFrame      = -1000;
	static constexpr int kInteractCooldownFrames = 12;
	static constexpr int kDropCooldownFrames     = 18;
	static constexpr int kPossessCooldownFrames  = 30;
	static int   g_iLastPossessFrame   = -1000;

	// =========================================================
	// Inline helpers (mirror Test_HumanPlaythrough's static helpers
	// so Phase 3a doesn't depend on extracting them yet).
	// =========================================================
	static bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	static DPVillager_Behaviour* GetVillager(Zenith_EntityID xV)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xV);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xV);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}

	static bool GetCameraBasis(Zenith_Maths::Vector3& xForward, Zenith_Maths::Vector3& xRight)
	{
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCam) return false;
		pxCam->GetFacingDir(xForward);
		xForward.y = 0.0f;
		const float fLen = glm::length(xForward);
		if (fLen < 1e-3f) return false;
		xForward = glm::normalize(xForward);
		xRight = glm::normalize(glm::cross(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xForward));
		return true;
	}

	static bool WorldToScreen(const Zenith_Maths::Vector3& xWorld, double& fOutX, double& fOutY)
	{
		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCam) return false;
		Zenith_Window* pxWindow = Zenith_Window::GetInstance();
		if (!pxWindow) return false;
		int32_t iW = 0, iH = 0;
		pxWindow->GetSize(iW, iH);
		if (iW <= 0 || iH <= 0) return false;

		Zenith_Maths::Matrix4 xView, xProj;
		pxCam->BuildViewMatrix(xView);
		pxCam->BuildProjectionMatrix(xProj);
		Zenith_Maths::Vector4 xClip = xProj * xView *
			Zenith_Maths::Vector4(xWorld.x, xWorld.y, xWorld.z, 1.0f);
		if (xClip.w <= 1e-4f) return false;
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;
		fOutX = static_cast<double>((fNdcX + 1.0f) * 0.5f * static_cast<float>(iW));
		fOutY = static_cast<double>((fNdcY + 1.0f) * 0.5f * static_cast<float>(iH));
		return true;
	}

	static void ClearMovementKeys()
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
	}

	static void ClearAllKeys()
	{
		ClearMovementKeys();
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, false);
	}

	// =========================================================
	// Queries that walk the active scene.
	// =========================================================
	static Zenith_EntityID FindClosestVillager(const Zenith_Maths::Vector3& xRef,
	                                           bool bAliveOnly,
	                                           Zenith_EntityID xExclude = Zenith_EntityID{})
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xBest, &fBestSq, &xRef, bAliveOnly, xExclude]
			(Zenith_EntityID xId, DPVillager_Behaviour& xVilla)
			{
				if (xExclude.IsValid() && xId == xExclude) return;
				if (bAliveOnly && xVilla.GetRemainingLife() <= 0.0f) return;
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xRef.x;
				const float fDz = xPos.z - xRef.z;
				const float fSq = fDx*fDx + fDz*fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});
		return xBest;
	}

	static Zenith_EntityID FindClosestPriest(const Zenith_Maths::Vector3& xRef, float& fOutDist)
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xBest, &fBestSq, &xRef](Zenith_EntityID xId, Priest_Behaviour&)
			{
				Zenith_Maths::Vector3 xPos;
				if (!TryGetEntityPos(xId, xPos)) return;
				const float fDx = xPos.x - xRef.x;
				const float fDz = xPos.z - xRef.z;
				const float fSq = fDx*fDx + fDz*fDz;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});
		fOutDist = (fBestSq < 1e29f) ? std::sqrt(fBestSq) : -1.0f;
		return xBest;
	}

	static Zenith_EntityID FindPentagram()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachScriptInActiveScene<DPPentagram_Behaviour>(
			[&xResult](Zenith_EntityID xId, DPPentagram_Behaviour&)
			{
				if (!xResult.IsValid()) xResult = xId;
			});
		return xResult;
	}

	static Zenith_EntityID FindForge()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachScriptInActiveScene<DPForge_Behaviour>(
			[&xResult](Zenith_EntityID xId, DPForge_Behaviour&)
			{
				if (!xResult.IsValid()) xResult = xId;
			});
		return xResult;
	}

	// Find the nearest objective item on the ground (not yet picked up).
	// DP_Items::FindItemByTag returns the spawned item with that tag; we
	// pick the closest of any uncollected objective.
	static Zenith_EntityID FindNearestObjectiveItem(const Zenith_Maths::Vector3& xRef)
	{
		Zenith_EntityID xBest;
		float fBestSq = 1e30f;
		for (int iObj = 0; iObj < 5; ++iObj)
		{
			const DP_ItemTag eTag = static_cast<DP_ItemTag>(
				static_cast<int>(DP_ItemTag::Objective1) + iObj);
			const uint32_t uMask = DP_Win::GetCollectedObjectivesMask();
			if (uMask & (1u << iObj)) continue; // already delivered
			const Zenith_EntityID xId = DP_Items::FindItemByTag(eTag);
			if (!xId.IsValid()) continue;
			Zenith_Maths::Vector3 xPos;
			if (!TryGetEntityPos(xId, xPos)) continue;
			const float fDx = xPos.x - xRef.x;
			const float fDz = xPos.z - xRef.z;
			const float fSq = fDx*fDx + fDz*fDz;
			if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
		}
		return xBest;
	}

	// =========================================================
	// Grid-A* helpers. Mirrors Test_HumanPlaythrough's pathfinder but
	// with a coarser grid + simpler walkability sample (1 ray, not 5).
	// =========================================================
	static inline bool IsCellWalkable(int x, int z)
	{
		if (x < 0 || x >= kPathGridDim || z < 0 || z >= kPathGridDim) return false;
		return g_abPathWalkable[z * kPathGridDim + x];
	}

	static void BuildPathGrid()
	{
		// Test-injection short-circuit: TestSurface::SetWalkabilityGridForTest
		// stamps the sentinel kTestInjectedHandle so unit tests can build a
		// hand-crafted grid + know A* will use exactly that grid rather than
		// overwriting it with raycast output from whatever scene happens to
		// be loaded.
		if (g_iPathGridSceneHandle == kTestInjectedHandle) return;
		// Scene-aware cache: rebuild iff the active scene's handle
		// changed since the previous build. Cheap to check (one
		// SceneManager call) and avoids the multi-scene wall-routing
		// bug previously latent in the once-per-process design.
		const int iActiveHandle = Zenith_SceneManager::GetActiveScene().GetHandle();
		if (g_bPathGridBuilt && g_iPathGridSceneHandle == iActiveHandle) return;
		uint32_t uWalkable = 0;
		for (int z = 0; z < kPathGridDim; ++z)
		{
			for (int x = 0; x < kPathGridDim; ++x)
			{
				const float cx = kPathOriginX + (x + 0.5f) * kPathCellSize;
				const float cz = kPathOriginZ + (z + 0.5f) * kPathCellSize;
				// Single centre raycast -- coarser than HumanPlaythrough's
				// 5-ray capsule test but ~5x cheaper. Wall AABBs sit Y=1..5;
				// floor at Y=0..1. Threshold 1.5 m sits between, so floor
				// hits are walkable + wall hits are blocked.
				const Zenith_PhysicsImpl::RaycastResult xH = g_xEngine.Physics().Raycast(
					Zenith_Maths::Vector3(cx, 10.0f, cz),
					Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 12.0f);
				const bool bWalkable = xH.m_bHit && xH.m_xHitPoint.y < kPathFloorYTop;
				g_abPathWalkable[z * kPathGridDim + x] = bWalkable;
				if (bWalkable) ++uWalkable;
			}
		}
		g_bPathGridBuilt = true;
		g_iPathGridSceneHandle = iActiveHandle;
		std::printf("[DPHeuristicBot] path grid built: %u/%d cells walkable (scene handle %d)\n",
			uWalkable, kPathGridDim * kPathGridDim, iActiveHandle);
		std::fflush(stdout);
	}

	static inline void WorldToCell(const Zenith_Maths::Vector3& xWorld, int& x, int& z)
	{
		x = static_cast<int>((xWorld.x - kPathOriginX) / kPathCellSize);
		z = static_cast<int>((xWorld.z - kPathOriginZ) / kPathCellSize);
	}

	static inline Zenith_Maths::Vector3 CellToWorld(int x, int z)
	{
		return Zenith_Maths::Vector3(
			kPathOriginX + (x + 0.5f) * kPathCellSize,
			kPathFloorY,
			kPathOriginZ + (z + 0.5f) * kPathCellSize);
	}

	// Snap a world-space cell to the nearest walkable cell. Used when
	// start or end falls inside a wall (rare but possible after
	// possession because villager spawn points can clip).
	static int SnapToWalkable(int x, int z, int iMaxRing = 8)
	{
		if (IsCellWalkable(x, z)) return z * kPathGridDim + x;
		for (int r = 1; r <= iMaxRing; ++r)
		{
			for (int dz = -r; dz <= r; ++dz)
			{
				for (int dx = -r; dx <= r; ++dx)
				{
					const int aDx = (dx < 0) ? -dx : dx;
					const int aDz = (dz < 0) ? -dz : dz;
					if (aDx != r && aDz != r) continue; // ring only
					const int nx = x + dx, nz = z + dz;
					if (IsCellWalkable(nx, nz)) return nz * kPathGridDim + nx;
				}
			}
		}
		return -1;
	}

	// A* over the grid. Open list is a vector with linear-scan
	// min-extract -- with kN=3600 + open << kN in practice, this is
	// cheaper than dragging in std::priority_queue.
	static bool ComputePathAStar(const Zenith_Maths::Vector3& xStart,
	                             const Zenith_Maths::Vector3& xEnd,
	                             Zenith_Vector<Zenith_Maths::Vector3>& xOutPath)
	{
		BuildPathGrid();
		int sx, sz, ex, ez;
		WorldToCell(xStart, sx, sz);
		WorldToCell(xEnd, ex, ez);

		const int iSnapStart = SnapToWalkable(sx, sz, 16);
		const int iSnapEnd   = SnapToWalkable(ex, ez, 16);
		if (iSnapStart < 0 || iSnapEnd < 0) return false;
		sx = iSnapStart % kPathGridDim; sz = iSnapStart / kPathGridDim;
		ex = iSnapEnd   % kPathGridDim; ez = iSnapEnd   / kPathGridDim;

		constexpr int kN = kPathGridDim * kPathGridDim;
		std::vector<float> axGScore(kN, 1e30f);
		std::vector<int>   aiCameFrom(kN, -1);
		std::vector<bool>  abVisited(kN, false);
		std::vector<std::pair<float, int>> axOpen;
		axOpen.reserve(64);

		auto Heuristic = [ex, ez](int x, int z) {
			const float fDx = static_cast<float>(x - ex);
			const float fDz = static_cast<float>(z - ez);
			return std::sqrt(fDx * fDx + fDz * fDz);
		};

		const int iStartIdx = sz * kPathGridDim + sx;
		const int iEndIdx   = ez * kPathGridDim + ex;
		axGScore[iStartIdx] = 0.0f;
		axOpen.push_back({Heuristic(sx, sz), iStartIdx});

		bool bFound = (iStartIdx == iEndIdx);
		while (!axOpen.empty() && !bFound)
		{
			size_t uBest = 0;
			for (size_t i = 1; i < axOpen.size(); ++i)
				if (axOpen[i].first < axOpen[uBest].first) uBest = i;
			const int iIdx = axOpen[uBest].second;
			axOpen[uBest] = axOpen.back();
			axOpen.pop_back();

			if (abVisited[iIdx]) continue;
			abVisited[iIdx] = true;
			if (iIdx == iEndIdx) { bFound = true; break; }

			const int x = iIdx % kPathGridDim;
			const int z = iIdx / kPathGridDim;
			for (int dz = -1; dz <= 1; ++dz)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					if (dx == 0 && dz == 0) continue;
					const int nx = x + dx, nz = z + dz;
					if (!IsCellWalkable(nx, nz)) continue;
					const int iNIdx = nz * kPathGridDim + nx;
					if (abVisited[iNIdx]) continue;
					const float fStep = (dx != 0 && dz != 0) ? 1.41421f : 1.0f;
					const float fNewG = axGScore[iIdx] + fStep;
					if (fNewG < axGScore[iNIdx])
					{
						axGScore[iNIdx] = fNewG;
						aiCameFrom[iNIdx] = iIdx;
						axOpen.push_back({fNewG + Heuristic(nx, nz), iNIdx});
					}
				}
			}
		}

		if (!bFound) return false;

		xOutPath.Clear();
		std::vector<int> aiReverse;
		int iCur = iEndIdx;
		while (iCur >= 0)
		{
			aiReverse.push_back(iCur);
			iCur = aiCameFrom[iCur];
		}
		for (auto it = aiReverse.rbegin(); it != aiReverse.rend(); ++it)
		{
			xOutPath.PushBack(CellToWorld(*it % kPathGridDim, *it / kPathGridDim));
		}
		// Replace final waypoint with the actual target so we don't
		// stop short by up to ~1.4 m.
		if (xOutPath.GetSize() > 0)
		{
			Zenith_Maths::Vector3& xLast = xOutPath.Get(xOutPath.GetSize() - 1);
			xLast.x = xEnd.x;
			xLast.z = xEnd.z;
		}
		return true;
	}

	// =========================================================
	// Movement primitives.
	// =========================================================
	static void DriveWASDInDirection(const Zenith_Maths::Vector3& xMyPos,
	                                 const Zenith_Maths::Vector3& xTarget)
	{
		const float fDx = xTarget.x - xMyPos.x;
		const float fDz = xTarget.z - xMyPos.z;
		Zenith_Maths::Vector3 xDelta(fDx, 0.0f, fDz);
		const float fLen = glm::length(xDelta);
		if (fLen < 1e-3f) { ClearMovementKeys(); return; }

		Zenith_Maths::Vector3 xForward, xRight;
		if (!GetCameraBasis(xForward, xRight)) { ClearMovementKeys(); return; }
		const Zenith_Maths::Vector3 xDir = xDelta / fLen;
		const float fForwardDot = glm::dot(xDir, xForward);
		const float fRightDot   = glm::dot(xDir, xRight);

		constexpr float kAxisThresh = 0.25f;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, fForwardDot >  kAxisThresh);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, fForwardDot < -kAxisThresh);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, fRightDot   >  kAxisThresh);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, fRightDot   < -kAxisThresh);
	}

	// Drive toward xTarget via A* path-following. Replans when the
	// final target drifts > kReplanIfTargetMoves OR when stuck.
	// Returns the current waypoint's xy distance for caller's
	// "within interact range" checks.
	static float DriveTowardViaPath(const Zenith_Maths::Vector3& xMyPos,
	                                const Zenith_Maths::Vector3& xTarget)
	{
		// Replan triggers:
		//   - no path cached
		//   - target moved significantly (chasing a moving objective)
		//   - we've been stuck for > kStuckFrameLimit (need a new route)
		const float fTdx = xTarget.x - g_xLastPlannedTarget.x;
		const float fTdz = xTarget.z - g_xLastPlannedTarget.z;
		bool bReplan = (g_axCurrentPath.GetSize() == 0u)
		            || (fTdx*fTdx + fTdz*fTdz > kReplanIfTargetMoves * kReplanIfTargetMoves);

		// Stuck detector: tracks meaningful displacement of g_xLastPos.
		const float fSdx = xMyPos.x - g_xLastPos.x;
		const float fSdz = xMyPos.z - g_xLastPos.z;
		if (fSdx*fSdx + fSdz*fSdz > kStuckMoveThresholdSq)
		{
			g_xLastPos = xMyPos;
			g_iStuckFrames = 0;
		}
		else
		{
			++g_iStuckFrames;
			if (g_iStuckFrames > kStuckFrameLimit)
			{
				bReplan = true;
				g_iStuckFrames = 0;
			}
		}

		if (bReplan)
		{
			g_axCurrentPath.Clear();
			g_iPathWaypoint = 0;
			ComputePathAStar(xMyPos, xTarget, g_axCurrentPath);
			g_xLastPlannedTarget = xTarget;
		}

		// Advance through already-reached waypoints (except the last
		// one -- that's our final target).
		while (g_iPathWaypoint + 1 < static_cast<int>(g_axCurrentPath.GetSize()))
		{
			const Zenith_Maths::Vector3& wp = g_axCurrentPath.Get(g_iPathWaypoint);
			const float fWdx = wp.x - xMyPos.x;
			const float fWdz = wp.z - xMyPos.z;
			if (fWdx*fWdx + fWdz*fWdz < kWaypointReachedDist * kWaypointReachedDist)
				++g_iPathWaypoint;
			else
				break;
		}

		// Drive toward the current waypoint (or the target if path empty).
		Zenith_Maths::Vector3 xWaypoint = xTarget;
		if (g_iPathWaypoint < static_cast<int>(g_axCurrentPath.GetSize()))
			xWaypoint = g_axCurrentPath.Get(g_iPathWaypoint);
		DriveWASDInDirection(xMyPos, xWaypoint);

		const float fFx = xTarget.x - xMyPos.x;
		const float fFz = xTarget.z - xMyPos.z;
		return std::sqrt(fFx*fFx + fFz*fFz);
	}

	// =========================================================
	// Click-to-possess. Resolves world -> screen and issues mouse press.
	// =========================================================
	static void ClickAtWorld(const Zenith_Maths::Vector3& xWorld)
	{
		double fSx = 0.0, fSy = 0.0;
		if (!WorldToScreen(xWorld, fSx, fSy)) return;
		Zenith_InputSimulator::SimulateMousePosition(fSx, fSy);
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	}

	// =========================================================
	// Goal selection -- pure dispatch on the observation tuple.
	// =========================================================
	struct Observation
	{
		bool bPossessed;
		Zenith_EntityID xV;
		Zenith_Maths::Vector3 xMyPos;
		DPVillager_Behaviour* pxVilla;
		DP_ItemTag eHeld;
		float fLife;
		float fPriestDist;
		Zenith_EntityID xObjectiveItem;
		Zenith_EntityID xPentagram;
		Zenith_EntityID xForge;
	};

	// Pure dispatch over primitives -- testable in isolation. Lives
	// next to the wrapper so the production read-path stays one call
	// away from the documented decision table.
	static Goal PickGoalForStateImpl(bool bPossessed,
	                                 float fPriestDist,
	                                 float fLife,
	                                 DP_ItemTag eHeldTag,
	                                 bool bPentagramPresent,
	                                 bool bForgePresent,
	                                 bool bObjectiveItemAvailable)
	{
		if (!bPossessed)
			return Goal::PossessClosest;
		if (fPriestDist >= 0.0f && fPriestDist < kFleeDistance)
			return Goal::FleeFromPriest;
		if (fLife > 0.0f && fLife < kSwapLifeThreshold)
			return Goal::BodySwap;
		// Holding-objective path: deliver.
		if (DP_IsObjectiveTag(eHeldTag) && bPentagramPresent)
			return Goal::WalkToPentagram;
		// Holding iron + forge known -> forge it.
		if (eHeldTag == DP_ItemTag::Iron && bForgePresent)
			return Goal::WalkToForge;
		// Default: chase nearest objective.
		if (bObjectiveItemAvailable)
			return Goal::WalkToObjective;
		return Goal::Idle;
	}

	static Goal PickGoal(const Observation& xObs)
	{
		return PickGoalForStateImpl(
			xObs.bPossessed,
			xObs.fPriestDist,
			xObs.fLife,
			xObs.eHeld,
			xObs.xPentagram.IsValid(),
			xObs.xForge.IsValid(),
			xObs.xObjectiveItem.IsValid());
	}

	// =========================================================
	// Public API.
	// =========================================================
	void Reset()
	{
		g_eCurrentGoal      = Goal::Idle;
		g_xLastPos          = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
		g_iStuckFrames      = 0;
		g_uSprintFrames     = 0;
		g_uWalkQuietFrames  = 0;
		g_uInteractPresses  = 0;
		g_uDropPresses      = 0;
		g_uPossessClicks    = 0;
		g_iLastInteractFrame= -1000;
		g_iLastDropFrame    = -1000;
		g_iLastPossessFrame = -1000;
		g_axCurrentPath.Clear();
		g_iPathWaypoint     = 0;
		g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
		// g_bPathGridBuilt is intentionally NOT reset -- the grid is
		// derived from static scene geometry, so a rebuild across bot
		// reruns within the same scene would waste 3600 raycasts.
		ClearAllKeys();
	}

	void Tick(int iFrame, float /*fDt*/)
	{
		// 1) Build observation.
		Observation xObs;
		xObs.xV = DP_Player::GetPossessedVillager();
		xObs.bPossessed = xObs.xV.IsValid();
		xObs.xMyPos = Zenith_Maths::Vector3(0.0f);
		xObs.pxVilla = nullptr;
		xObs.eHeld = DP_ItemTag::None;
		xObs.fLife = 0.0f;
		xObs.fPriestDist = -1.0f;
		xObs.xObjectiveItem = Zenith_EntityID{};
		xObs.xPentagram = FindPentagram();
		xObs.xForge = FindForge();

		if (xObs.bPossessed)
		{
			TryGetEntityPos(xObs.xV, xObs.xMyPos);
			xObs.pxVilla = GetVillager(xObs.xV);
			if (xObs.pxVilla != nullptr) xObs.fLife = xObs.pxVilla->GetRemainingLife();
			xObs.eHeld = DP_Player::GetHeldItemTag(xObs.xV);
			FindClosestPriest(xObs.xMyPos, xObs.fPriestDist);
			xObs.xObjectiveItem = FindNearestObjectiveItem(xObs.xMyPos);
		}

		// 2) Pick goal.
		const Goal eGoal = PickGoal(xObs);
		g_eCurrentGoal = eGoal;

		// 3) Modifier keys based on observation.
		const bool bSprintNow = xObs.bPossessed && xObs.fLife > 0.0f
		                      && (xObs.fLife < kSprintLifeThreshold
		                       || (xObs.fPriestDist >= 0.0f && xObs.fPriestDist < kFleeDistance));
		const bool bQuietNow  = xObs.bPossessed
		                      && xObs.fPriestDist >= 0.0f
		                      && xObs.fPriestDist < kPriestSuspiciousDist
		                      && !bSprintNow;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, bSprintNow);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, bQuietNow);
		if (bSprintNow) ++g_uSprintFrames;
		if (bQuietNow)  ++g_uWalkQuietFrames;

		// 4) Execute goal.
		switch (eGoal)
		{
		case Goal::PossessClosest:
		case Goal::BodySwap:
		{
			// Click the nearest live villager that isn't already possessed.
			Zenith_Maths::Vector3 xClickRef = xObs.xMyPos;
			if (!xObs.bPossessed)
			{
				// Use the camera target if we don't have a body yet.
				Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
				if (pxCam != nullptr) pxCam->GetPosition(xClickRef);
			}
			const Zenith_EntityID xTarget = FindClosestVillager(
				xClickRef, /*bAliveOnly=*/true, /*exclude=*/xObs.xV);
			if (xTarget.IsValid() && (iFrame - g_iLastPossessFrame) >= kPossessCooldownFrames)
			{
				Zenith_Maths::Vector3 xTargetPos;
				if (TryGetEntityPos(xTarget, xTargetPos))
				{
					ClickAtWorld(xTargetPos);
					g_iLastPossessFrame = iFrame;
					++g_uPossessClicks;
				}
			}
			ClearMovementKeys();
			break;
		}
		case Goal::WalkToObjective:
		{
			if (!xObs.xObjectiveItem.IsValid()) { ClearMovementKeys(); break; }
			Zenith_Maths::Vector3 xTargetPos;
			if (!TryGetEntityPos(xObs.xObjectiveItem, xTargetPos)) { ClearMovementKeys(); break; }
			const float fDistToTarget = DriveTowardViaPath(xObs.xMyPos, xTargetPos);
			if (fDistToTarget < kStopAtTargetDist
			    && (iFrame - g_iLastInteractFrame) >= kInteractCooldownFrames)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
				g_iLastInteractFrame = iFrame;
				++g_uInteractPresses;
			}
			break;
		}
		case Goal::WalkToForge:
		{
			Zenith_Maths::Vector3 xTargetPos;
			if (!xObs.xForge.IsValid() || !TryGetEntityPos(xObs.xForge, xTargetPos))
			{
				ClearMovementKeys();
				break;
			}
			const float fDistToTarget = DriveTowardViaPath(xObs.xMyPos, xTargetPos);
			if (fDistToTarget < kStopAtTargetDist
			    && (iFrame - g_iLastInteractFrame) >= kInteractCooldownFrames)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
				g_iLastInteractFrame = iFrame;
				++g_uInteractPresses;
			}
			break;
		}
		case Goal::WalkToPentagram:
		{
			Zenith_Maths::Vector3 xTargetPos;
			if (!xObs.xPentagram.IsValid() || !TryGetEntityPos(xObs.xPentagram, xTargetPos))
			{
				ClearMovementKeys();
				break;
			}
			const float fDistToTarget = DriveTowardViaPath(xObs.xMyPos, xTargetPos);
			if (fDistToTarget < kStopAtTargetDist
			    && (iFrame - g_iLastInteractFrame) >= kInteractCooldownFrames)
			{
				Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
				g_iLastInteractFrame = iFrame;
				++g_uInteractPresses;
			}
			break;
		}
		case Goal::FleeFromPriest:
		{
			Zenith_Maths::Vector3 xPriestPos;
			float fIgnored = 0.0f;
			const Zenith_EntityID xPriest = FindClosestPriest(xObs.xMyPos, fIgnored);
			if (xPriest.IsValid() && TryGetEntityPos(xPriest, xPriestPos))
			{
				// Walk straight away from the priest -- short-range escape
				// doesn't need pathing (and pathing toward an extrapolated
				// "away" point on the grid usually doesn't find a route
				// because the point falls outside walkable cells).
				const Zenith_Maths::Vector3 xAway(
					xObs.xMyPos.x + (xObs.xMyPos.x - xPriestPos.x),
					xObs.xMyPos.y,
					xObs.xMyPos.z + (xObs.xMyPos.z - xPriestPos.z));
				DriveWASDInDirection(xObs.xMyPos, xAway);
			}
			else
			{
				ClearMovementKeys();
			}
			break;
		}
		case Goal::PickupItem:
		case Goal::Idle:
		default:
			ClearMovementKeys();
			break;
		}

		// 5) Periodic G-drop when we're holding the WRONG kind of item
		// for our current goal -- e.g. holding Wood when we're chasing
		// an objective. Frees us up for the next pickup.
		if (xObs.bPossessed
		    && xObs.eHeld != DP_ItemTag::None
		    && !DP_IsObjectiveTag(xObs.eHeld)
		    && xObs.eHeld != DP_ItemTag::Iron
		    && xObs.eHeld != DP_ItemTag::SkeletonKey
		    && (iFrame - g_iLastDropFrame) >= kDropCooldownFrames
		    && eGoal == Goal::WalkToObjective)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_G);
			g_iLastDropFrame = iFrame;
			++g_uDropPresses;
		}
	}

	Goal GetCurrentGoal()      { return g_eCurrentGoal; }
	uint32_t GetSprintFrameCount()    { return g_uSprintFrames; }
	uint32_t GetWalkQuietFrameCount() { return g_uWalkQuietFrames; }
	uint32_t GetInteractPressCount()  { return g_uInteractPresses; }
	uint32_t GetDropPressCount()      { return g_uDropPresses; }
	uint32_t GetPossessClickCount()   { return g_uPossessClicks; }

	// =========================================================
	// TestSurface implementations -- thin pass-throughs to the
	// file-static helpers above. Lives in the same translation
	// unit so it can reach the file-static state (g_abPathWalkable,
	// g_bPathGridBuilt) without making those symbols externally
	// visible.
	// =========================================================
	namespace TestSurface
	{
		int   GetGridDim()  { return kPathGridDim; }
		float GetCellSize() { return kPathCellSize; }
		float GetOriginX()  { return kPathOriginX; }
		float GetOriginZ()  { return kPathOriginZ; }

		void SetWalkabilityGridForTest(const bool* abGrid)
		{
			if (abGrid == nullptr) return;
			for (int i = 0; i < kPathGridDim * kPathGridDim; ++i)
			{
				g_abPathWalkable[i] = abGrid[i];
			}
			g_bPathGridBuilt = true;
			// Flag the grid as test-injected via the kTestInjectedHandle
			// sentinel. BuildPathGrid short-circuits when it sees this
			// handle and skips its raycast rebuild, so A* operates on
			// exactly the grid the test set up.
			g_iPathGridSceneHandle = kTestInjectedHandle;
		}

		void ResetForTest()
		{
			for (int i = 0; i < kPathGridDim * kPathGridDim; ++i)
			{
				g_abPathWalkable[i] = false;
			}
			g_bPathGridBuilt = false;
			g_iPathGridSceneHandle = -1;
			g_axCurrentPath.Clear();
			g_iPathWaypoint = 0;
			g_xLastPlannedTarget = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
		}

		// Returns the scene handle the current grid was built for, or -1
		// if no grid has been built since the last reset. Used by the
		// scene-swap unit test to verify the cache flip.
		int GetPathGridSceneHandleForTest()
		{
			return g_iPathGridSceneHandle;
		}

		void WorldToCell(const Zenith_Maths::Vector3& xWorld, int& iOutX, int& iOutZ)
		{
			::DPHeuristicBot::WorldToCell(xWorld, iOutX, iOutZ);
		}
		Zenith_Maths::Vector3 CellToWorld(int iX, int iZ)
		{
			return ::DPHeuristicBot::CellToWorld(iX, iZ);
		}
		bool IsCellWalkable(int iX, int iZ)
		{
			return ::DPHeuristicBot::IsCellWalkable(iX, iZ);
		}
		int SnapToWalkable(int iX, int iZ, int iMaxRing)
		{
			return ::DPHeuristicBot::SnapToWalkable(iX, iZ, iMaxRing);
		}
		bool ComputePathAStar(const Zenith_Maths::Vector3& xStart,
		                     const Zenith_Maths::Vector3& xEnd,
		                     Zenith_Vector<Zenith_Maths::Vector3>& xOutPath)
		{
			return ::DPHeuristicBot::ComputePathAStar(xStart, xEnd, xOutPath);
		}
		Goal PickGoalForState(bool bPossessed,
		                      float fPriestDist,
		                      float fLife,
		                      DP_ItemTag eHeldTag,
		                      bool bPentagramPresent,
		                      bool bForgePresent,
		                      bool bObjectiveItemAvailable)
		{
			return PickGoalForStateImpl(bPossessed, fPriestDist, fLife, eHeldTag,
				bPentagramPresent, bForgePresent, bObjectiveItemAvailable);
		}
	}
}

#endif // ZENITH_INPUT_SIMULATOR
