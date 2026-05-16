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
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Windows/Zenith_Windows_Window.h"

#include <cmath>
#include <cstdint>

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
	static constexpr int   kStrafeBurstFrames   = 30;       // ~0.5 s
	static constexpr int   kAxisThreshDecisive  = 1;        // 1=binary direction commit

	// =========================================================
	// Persistent state.
	// =========================================================
	static Goal g_eCurrentGoal = Goal::Idle;
	static Zenith_Maths::Vector3 g_xLastPos{1e9f, 0.0f, 0.0f};
	static int   g_iStuckFrames        = 0;
	static int   g_iStrafeRemaining    = 0;
	static int   g_iStrafeDirection    = 0;  // 0 = right (+A), 1 = left (+D)
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
	// Movement primitives. DriveToward sets WASD + tracks stuck.
	// =========================================================
	static void DriveToward(const Zenith_Maths::Vector3& xMyPos,
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

	// While "strafing" (stuck-recovery), push the villager perpendicular
	// to the camera right vector so we punch out of a wall corner.
	static void DriveStrafe()
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		if (g_iStrafeDirection == 0)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		}
		else
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		}
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
	}

	// Update the stuck detector. Called every frame while moving.
	// Returns true if the bot is stuck right now (caller should strafe).
	static bool UpdateStuck(const Zenith_Maths::Vector3& xMyPos)
	{
		const float fDx = xMyPos.x - g_xLastPos.x;
		const float fDz = xMyPos.z - g_xLastPos.z;
		if (fDx*fDx + fDz*fDz > kStuckMoveThresholdSq)
		{
			g_xLastPos = xMyPos;
			g_iStuckFrames = 0;
			return false;
		}
		++g_iStuckFrames;
		return g_iStuckFrames > kStuckFrameLimit;
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

	static Goal PickGoal(const Observation& xObs)
	{
		if (!xObs.bPossessed)
			return Goal::PossessClosest;
		if (xObs.fPriestDist >= 0.0f && xObs.fPriestDist < kFleeDistance)
			return Goal::FleeFromPriest;
		if (xObs.fLife > 0.0f && xObs.fLife < kSwapLifeThreshold)
			return Goal::BodySwap;
		// Holding-objective path: deliver.
		if (DP_IsObjectiveTag(xObs.eHeld) && xObs.xPentagram.IsValid())
			return Goal::WalkToPentagram;
		// Holding iron + forge known -> forge it.
		if (xObs.eHeld == DP_ItemTag::Iron && xObs.xForge.IsValid())
			return Goal::WalkToForge;
		// Default: chase nearest objective.
		if (xObs.xObjectiveItem.IsValid())
			return Goal::WalkToObjective;
		return Goal::Idle;
	}

	// =========================================================
	// Public API.
	// =========================================================
	void Reset()
	{
		g_eCurrentGoal      = Goal::Idle;
		g_xLastPos          = Zenith_Maths::Vector3(1e9f, 0.0f, 0.0f);
		g_iStuckFrames      = 0;
		g_iStrafeRemaining  = 0;
		g_iStrafeDirection  = 0;
		g_uSprintFrames     = 0;
		g_uWalkQuietFrames  = 0;
		g_uInteractPresses  = 0;
		g_uDropPresses      = 0;
		g_uPossessClicks    = 0;
		g_iLastInteractFrame= -1000;
		g_iLastDropFrame    = -1000;
		g_iLastPossessFrame = -1000;
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
			if (UpdateStuck(xObs.xMyPos) || g_iStrafeRemaining > 0)
			{
				if (g_iStrafeRemaining <= 0)
				{
					g_iStrafeRemaining = kStrafeBurstFrames;
					g_iStrafeDirection = (iFrame >> 5) & 1;
					g_iStuckFrames = 0;
				}
				DriveStrafe();
				--g_iStrafeRemaining;
			}
			else
			{
				DriveToward(xObs.xMyPos, xTargetPos);
			}
			// Within pickup range -> attempt F press for tagged items
			// (items use the proximity-pickup path; F is still useful for
			// chest-enclosed items in the placeholder layout).
			const float fDx = xTargetPos.x - xObs.xMyPos.x;
			const float fDz = xTargetPos.z - xObs.xMyPos.z;
			if (fDx*fDx + fDz*fDz < kStopAtTargetDist * kStopAtTargetDist
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
			DriveToward(xObs.xMyPos, xTargetPos);
			const float fDx = xTargetPos.x - xObs.xMyPos.x;
			const float fDz = xTargetPos.z - xObs.xMyPos.z;
			if (fDx*fDx + fDz*fDz < kStopAtTargetDist * kStopAtTargetDist
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
			DriveToward(xObs.xMyPos, xTargetPos);
			const float fDx = xTargetPos.x - xObs.xMyPos.x;
			const float fDz = xTargetPos.z - xObs.xMyPos.z;
			if (fDx*fDx + fDz*fDz < kStopAtTargetDist * kStopAtTargetDist
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
				// Target = MyPos + (MyPos - PriestPos) -> walking away.
				const Zenith_Maths::Vector3 xAway(
					xObs.xMyPos.x + (xObs.xMyPos.x - xPriestPos.x),
					xObs.xMyPos.y,
					xObs.xMyPos.z + (xObs.xMyPos.z - xPriestPos.z));
				DriveToward(xObs.xMyPos, xAway);
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
}

#endif // ZENITH_INPUT_SIMULATOR
