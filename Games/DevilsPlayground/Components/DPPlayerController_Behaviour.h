#pragma once
/**
 * DPPlayerController_Behaviour - global input router for DevilsPlayground.
 *
 * Attached to a single GameManager entity. Watches for click-to-possess
 * events and forwards them via DP_Player::SetPossessedVillager. Per-villager
 * movement input is read inside DPVillager_Behaviour, not here.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Physics/Zenith_Physics.h"
#include "Input/Zenith_Input.h"
// NOTE: do not include "Windows/Zenith_Windows_Window.h" directly. Zenith.h
// (already included by the .cpp via the PCH) pulls in the platform-correct
// window header through Zenith_OS_Include.h — Windows on win64, Android's
// own Zenith_Window on AGDE. Hard-coding the Windows path here broke the
// Android build when DevilsPlayground was added to zenith_agde.sln.
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPInputActions.h"
#include "Source/DPParticles.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"

#include <unordered_map>

// VillagerHeldRecord lives outside the class so callers (DP_Player
// namespace functions in PublicInterfaces.cpp) can refer to it without
// pulling the whole behaviour into their header chain.
struct DPVillagerHeldRecord
{
	Zenith_EntityID m_xItem = INVALID_ENTITY_ID;
	DP_ItemTag      m_eTag  = DP_ItemTag::None;
};

class DPPlayerController_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPPlayerController_Behaviour)

	DPPlayerController_Behaviour() = delete;
	DPPlayerController_Behaviour(Zenith_Entity& /*xParentEntity*/)
	{}

	void OnAwake() ZENITH_FINAL override
	{
		// Singleton: per-scene script, set as soon as OnAwake fires
		// (engine fires Awake on every script before any OnStart).
		s_pxInstance = this;
	}

	void OnDestroy() ZENITH_FINAL override
	{
		// m_xHeldItems + m_xDemonScent are auto-cleared by their
		// destructors when this script is freed. Scene unload is the
		// only path that destroys the script, so the maps' lifetimes
		// are bounded by the scene -- no stale rows can leak across
		// scene transitions. Replaces the previous process-global
		// g_xHeldItems + g_xDemonScent + their manual clear() calls
		// in DP_Player::ResetForNewRun (Phase B of the bot-test
		// batched-mode investigation, 2026-05-17).
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	static DPPlayerController_Behaviour* Instance() { return s_pxInstance; }

	//==========================================================================
	// Held-item registry. Keyed by packed villager EntityID. Mutated by
	// DP_Player::SetHeldItem / RemoveHeldItem; read by GetHeldItem*.
	//==========================================================================
	static uint64_t PackEntityID(Zenith_EntityID xID)
	{
		return (static_cast<uint64_t>(xID.m_uGeneration) << 32)
		     | static_cast<uint64_t>(xID.m_uIndex);
	}

	void SetHeldItemRecord(Zenith_EntityID xVillager, const DPVillagerHeldRecord& xRec)
	{
		m_xHeldItems[PackEntityID(xVillager)] = xRec;
	}

	void RemoveHeldItem(Zenith_EntityID xVillager)
	{
		m_xHeldItems.erase(PackEntityID(xVillager));
	}

	DP_ItemTag GetHeldItemTag(Zenith_EntityID xVillager) const
	{
		auto it = m_xHeldItems.find(PackEntityID(xVillager));
		if (it == m_xHeldItems.end()) return DP_ItemTag::None;
		return it->second.m_eTag;
	}

	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager) const
	{
		auto it = m_xHeldItems.find(PackEntityID(xVillager));
		if (it == m_xHeldItems.end()) return INVALID_ENTITY_ID;
		return it->second.m_xItem;
	}

	//==========================================================================
	// Demon-scent registry. Per-villager scalar, decays on tick + bumps
	// on successful possession (MVP-1.6 hound-target heuristic).
	//==========================================================================
	float GetDemonScent(Zenith_EntityID xVillager) const
	{
		auto it = m_xDemonScent.find(PackEntityID(xVillager));
		if (it == m_xDemonScent.end()) return 0.0f;
		return it->second;
	}

	void BumpDemonScent(Zenith_EntityID xVillager, float fAddAmount, float fMax)
	{
		const uint64_t uKey = PackEntityID(xVillager);
		float fNew = m_xDemonScent[uKey] + fAddAmount;
		if (fNew > fMax) fNew = fMax;
		m_xDemonScent[uKey] = fNew;
	}

	// Decays every entry by fDecayPerSec * fDt and erases rows that
	// hit zero. Iterate-and-erase to keep the table from accumulating
	// dead handles.
	void DecayDemonScent(float fDecayPerSec, float fDt)
	{
		if (m_xDemonScent.empty()) return;
		const float fDecay = fDecayPerSec * fDt;
		for (auto it = m_xDemonScent.begin(); it != m_xDemonScent.end(); )
		{
			it->second -= fDecay;
			if (it->second <= 0.0f) it = m_xDemonScent.erase(it);
			else ++it;
		}
	}

	// Iterate every (villager, scent) entry. Callback signature:
	// void(Zenith_EntityID, float). Used by DP_Player::WriteHighestScentToBlackboard.
	template <typename TFn>
	void ForEachDemonScentEntry(TFn xFn) const
	{
		for (const auto& [uPacked, fScent] : m_xDemonScent)
		{
			Zenith_EntityID xId;
			xId.m_uIndex      = static_cast<uint32_t>(uPacked & 0xFFFFFFFFu);
			xId.m_uGeneration = static_cast<uint32_t>(uPacked >> 32);
			xFn(xId, fScent);
		}
	}

	bool HasDemonScentEntries() const { return !m_xDemonScent.empty(); }

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// Drive the perception system. The engine does NOT auto-tick it from
		// the main loop (matches AIShowcase's pattern); a game-side controller
		// has to call Update once per frame so registered agents (priest)
		// process sight/hearing/damage stimuli.
		Zenith_PerceptionSystem::Update(fDt);

		// MVP-1.5: drain the possession cooldown per frame. The cooldown is
		// set by TryVoluntaryPossessSwitch on a successful voluntary switch
		// and gates subsequent attempts until it reaches zero. Death + priest
		// apprehend paths don't touch the cooldown.
		DP_Player::TickPossessionCooldown(fDt);

		// MVP-1.6: decay scent on every villager that's carrying any, then
		// push the highest-scent villager handle to every AIAgentComponent's
		// BB_KEY_HIGH_SCENT_TARGET slot. The priest reads this in
		// DP_BTAction_FindPosInSuspicionSphere (2026-05-21) to bias
		// patrol toward the high-scent villager.
		DP_Player::TickDemonScent(fDt);
		DP_Player::WriteHighestScentToBlackboard();

		// 2026-05-21: update the visible scent-aura emitter. Following
		// the highest-scent villager so the player can SEE which body
		// is "marked" by repeated possession -- a strong cue to switch
		// to a fresher villager. Threshold matches the priest's
		// hound-bark threshold so the visual and the BT-side bias
		// agree on "when does scent matter".
		{
			Zenith_EntityID xHighestId = INVALID_ENTITY_ID;
			float fHighestScent = 0.0f;
			ForEachDemonScentEntry(
				[&xHighestId, &fHighestScent]
				(Zenith_EntityID xId, float fScent)
				{
					if (fScent > fHighestScent)
					{
						fHighestScent = fScent;
						xHighestId = xId;
					}
				});
			const float fThreshold =
				DP_Tuning::Get<float>("possession.demon_scent_hound_bark_threshold");
			DP_Particles::UpdateHighScentAura(
				xHighestId, fHighestScent >= fThreshold);
		}

		// MVP-2.1.1/2 Devout channel: per-frame countdown + priest
		// interrupt check. Quiet no-op until TryVoluntaryPossessSwitch
		// starts a channel onto a Devout target.
		DP_Player::TickChannel(fDt);

		// MVP-2.4.5 memory fog: age all existing memory entries. The
		// recording side (DPFogPass) refreshes entries for currently-
		// visible cells, so this only ages cells that are NOT being
		// revealed right now -- producing the "tiles the villager
		// walked through earlier" memory effect.
		DP_Fog::TickMemoryFog(fDt);

		// MVP-1.3.5 Dawn: tick the night-timer countdown. Quiet
		// no-op until DP_Night::StartNight has been called (production
		// path: scene-entry hook or future Begin-Run menu; test path:
		// tests call StartNight directly). On the frame the timer
		// crosses 0, DP_OnRunLost{Dawn} dispatches exactly once.
		DP_Night::TickNight(fDt);

		HandleClickToPossess();
		HandleDropItem();
	}

private:
	void HandleClickToPossess()
	{
		if (!DP_Input::ReadPossessClickPressed()) return;

		Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (pxCam == nullptr) return;

		// Pick the villager whose world position projects closest to the
		// current mouse cursor in screen space. We prefer screen-space
		// proximity over a physics raycast because:
		//   - Building wall colliders sit between the orbit camera and
		//     villagers standing inside, so a raycast catches the wall.
		//   - Multi-hit raycasts can't recover when the villager's sphere
		//     collider overlaps the wall AABB (advancing past the wall
		//     advances past the villager too).
		// Screen-space picking matches what the player sees: clicking near a
		// villager's silhouette possesses it, regardless of intervening
		// geometry. Bounded by a screen-pixel tolerance so misclicks in empty
		// space don't possess a far-away villager.
		Zenith_Window* pxWindow = Zenith_Window::GetInstance();
		if (pxWindow == nullptr) return;
		int32_t iW = 0, iH = 0;
		pxWindow->GetSize(iW, iH);
		if (iW <= 0 || iH <= 0) return;

		Zenith_Maths::Vector2_64 xMousePos;
		Zenith_Input::GetMousePosition(xMousePos);

		Zenith_Maths::Matrix4 xView, xProj;
		pxCam->BuildViewMatrix(xView);
		pxCam->BuildProjectionMatrix(xProj);
		const Zenith_Maths::Matrix4 xVP = xProj * xView;

		// Pixel tolerance for picking — generous enough that a click within
		// the visual silhouette of a humanoid (~80 pixels at typical zoom)
		// still snaps onto it.
		constexpr double kMaxPixelDistSq = 120.0 * 120.0;

		Zenith_EntityID xBest;
		double fBestSq = kMaxPixelDistSq;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				Zenith_SceneData* pxS = Zenith_SceneManager::GetSceneDataForEntity(xId);
				if (pxS == nullptr) return;
				Zenith_Entity xEnt = pxS->TryGetEntity(xId);
				if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>()) return;
				Zenith_Maths::Vector3 xWorld;
				xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xWorld);
				// Aim at body centre rather than feet — the visible silhouette
				// is centred ~1 m above the entity origin.
				xWorld.y += 1.0f;
				const Zenith_Maths::Vector4 xClip = xVP *
					Zenith_Maths::Vector4(xWorld.x, xWorld.y, xWorld.z, 1.0f);
				if (xClip.w <= 1e-4f) return;
				const float fNdcX = xClip.x / xClip.w;
				const float fNdcY = xClip.y / xClip.w;
				const double fSx = (fNdcX + 1.0f) * 0.5f * static_cast<float>(iW);
				const double fSy = (fNdcY + 1.0f) * 0.5f * static_cast<float>(iH);
				const double fDx = fSx - xMousePos.x;
				const double fDy = fSy - xMousePos.y;
				const double fSq = fDx * fDx + fDy * fDy;
				if (fSq < fBestSq) { fBestSq = fSq; xBest = xId; }
			});

		if (xBest.IsValid())
		{
			// Voluntary switch path -- triggers the cooldown gate. If the
			// click lands within the cooldown window (1.5 s default) the
			// possession refuses, silently. Cosmetic feedback (HUD flash /
			// audio) is a future-MVP polish item.
			DP_Player::TryVoluntaryPossessSwitch(xBest);
		}
	}

	// MVP-1.4.5: drop verb. G releases the possessed villager's held
	// item at the villager's foot position and starts a short pickup
	// cooldown on the item so it doesn't immediately re-pick-up.
	void HandleDropItem()
	{
		if (!DP_Input::ReadDropPressed()) return;
		const Zenith_EntityID xVillager = DP_Player::GetPossessedVillager();
		if (!xVillager.IsValid()) return;
		const Zenith_EntityID xItem = DP_Player::GetHeldItemEntity(xVillager);
		if (!xItem.IsValid()) return;

		// Read the villager's foot position (transform origin == feet by
		// authoring convention -- the visible mesh is centred ~1 m above).
		Zenith_SceneData* pxVScene =
			Zenith_SceneManager::GetSceneDataForEntity(xVillager);
		if (pxVScene == nullptr) return;
		Zenith_Entity xV = pxVScene->TryGetEntity(xVillager);
		if (!xV.IsValid() || !xV.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_Maths::Vector3 xFootPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xFootPos);

		// Resolve the item entity (may be in a different scene if
		// MoveEntityToScene was used -- defensive scene lookup).
		Zenith_SceneData* pxIScene =
			Zenith_SceneManager::GetSceneDataForEntity(xItem);
		if (pxIScene == nullptr) return;
		Zenith_Entity xI = pxIScene->TryGetEntity(xItem);
		if (!xI.IsValid() || !xI.HasComponent<Zenith_TransformComponent>()) return;
		xI.GetComponent<Zenith_TransformComponent>().SetPosition(xFootPos);

		// Arm the item's post-drop cooldown so DPItemBase::OnUpdate
		// doesn't immediately re-pick-up from the foot position.
		if (xI.HasComponent<Zenith_ScriptComponent>())
		{
			Zenith_ScriptComponent& xScr = xI.GetComponent<Zenith_ScriptComponent>();
			if (DPItemBase_Behaviour* pxItemBeh = xScr.GetScript<DPItemBase_Behaviour>())
			{
				pxItemBeh->BeginPostDropCooldown();
			}
		}

		// Clear the held-item side-table entry. The villager's
		// floating held-item visual is rebuilt on the next OnUpdate
		// when GetHeldItemTag flips back to None.
		DP_Player::RemoveHeldItem(xVillager);
	}

	static inline DPPlayerController_Behaviour* s_pxInstance = nullptr;

	// Held-item registry: one entry per villager holding an item.
	// Cleared automatically when this script is destroyed (scene unload).
	std::unordered_map<uint64_t, DPVillagerHeldRecord> m_xHeldItems;

	// Demon-scent registry: per-villager scalar, accumulates on possession
	// + decays per-frame via DP_Player::TickDemonScent. Cleared automatically
	// when this script is destroyed (scene unload).
	std::unordered_map<uint64_t, float> m_xDemonScent;
};
