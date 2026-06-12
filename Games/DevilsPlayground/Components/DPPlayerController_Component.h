#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPPlayerController_Component - global input router for DevilsPlayground.
 *
 * Attached to a single GameManager entity. Watches for click-to-possess
 * events and forwards them via DP_Player::SetPossessedVillager. Per-villager
 * movement input is read inside DPVillager_Component, not here.
 */

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
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

#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_Vector.h"

// VillagerHeldRecord lives outside the class so callers (DP_Player
// namespace functions in DP_Player.cpp) can refer to it without
// pulling the whole component into their header chain.
struct DPVillagerHeldRecord
{
	Zenith_EntityID m_xItem = INVALID_ENTITY_ID;
	DP_ItemTag      m_eTag  = DP_ItemTag::None;
};

class DPPlayerController_Component ZENITH_FINAL
{
public:
	DPPlayerController_Component() = delete;
	DPPlayerController_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Heap-stability: s_pxInstance points at `this`, and component pools
	// relocate on resize / swap-and-pop / cross-scene transfer. Hand-written
	// moves repoint the singleton at the new address; copies deleted.
	DPPlayerController_Component(const DPPlayerController_Component&) = delete;
	DPPlayerController_Component& operator=(const DPPlayerController_Component&) = delete;

	DPPlayerController_Component(DPPlayerController_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xHeldItems(std::move(xOther.m_xHeldItems))
		, m_xDemonScent(std::move(xOther.m_xDemonScent))
		, m_xLastWrittenHighScent(xOther.m_xLastWrittenHighScent)
		, m_bHighScentBlackboardDirty(xOther.m_bHighScentBlackboardDirty)
		, m_xPossessedVillager(xOther.m_xPossessedVillager)
		, m_fPossessionCooldownSec(xOther.m_fPossessionCooldownSec)
		, m_xPossessionAnchor(xOther.m_xPossessionAnchor)
		, m_bHasPossessionAnchor(xOther.m_bHasPossessionAnchor)
		, m_xChannelTarget(xOther.m_xChannelTarget)
		, m_fChannelRemaining(xOther.m_fChannelRemaining)
		, m_bChannelActive(xOther.m_bChannelActive)
		, m_uCollectedObjectivesMask(xOther.m_uCollectedObjectivesMask)
		, m_bHasWon(xOther.m_bHasWon)
		, m_fNightRemainingSec(xOther.m_fNightRemainingSec)
		, m_bNightActive(xOther.m_bNightActive)
		, m_bDawnDispatched(xOther.m_bDawnDispatched)
	{
		if (s_pxInstance == &xOther) s_pxInstance = this;
	}

	DPPlayerController_Component& operator=(DPPlayerController_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity             = xOther.m_xParentEntity;
			m_xHeldItems                = std::move(xOther.m_xHeldItems);
			m_xDemonScent               = std::move(xOther.m_xDemonScent);
			m_xLastWrittenHighScent     = xOther.m_xLastWrittenHighScent;
			m_bHighScentBlackboardDirty = xOther.m_bHighScentBlackboardDirty;
			m_xPossessedVillager        = xOther.m_xPossessedVillager;
			m_fPossessionCooldownSec    = xOther.m_fPossessionCooldownSec;
			m_xPossessionAnchor         = xOther.m_xPossessionAnchor;
			m_bHasPossessionAnchor      = xOther.m_bHasPossessionAnchor;
			m_xChannelTarget            = xOther.m_xChannelTarget;
			m_fChannelRemaining         = xOther.m_fChannelRemaining;
			m_bChannelActive            = xOther.m_bChannelActive;
			m_uCollectedObjectivesMask  = xOther.m_uCollectedObjectivesMask;
			m_bHasWon                   = xOther.m_bHasWon;
			m_fNightRemainingSec        = xOther.m_fNightRemainingSec;
			m_bNightActive              = xOther.m_bNightActive;
			m_bDawnDispatched           = xOther.m_bDawnDispatched;
			if (s_pxInstance == &xOther) s_pxInstance = this;
		}
		return *this;
	}

	~DPPlayerController_Component()
	{
		// Pool relocation destructs the moved-from source; only clear the
		// singleton when it still points at THIS instance.
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	void OnAwake()
	{
		// Singleton: per-scene component, set as soon as OnAwake fires
		// (engine fires Awake on every entity before any OnStart).
		Zenith_Assert(s_pxInstance == nullptr,
			"DPPlayerController_Component singleton double-instantiated");
		s_pxInstance = this;
		// B2: force a fresh high-scent blackboard write on the first OnUpdate
		// so freshly-created AI agents (spawned at scene start) get the value.
		m_bHighScentBlackboardDirty = true;
	}

	void OnDestroy()
	{
		// m_xHeldItems + m_xDemonScent are auto-cleared by their
		// destructors when this component is freed. Scene unload is the
		// only path that destroys the component, so the maps' lifetimes
		// are bounded by the scene -- no stale rows can leak across
		// scene transitions. Replaces the previous process-global
		// g_xHeldItems + g_xDemonScent + their manual clear() calls
		// in DP_Player::ResetForNewRun (Phase B of the bot-test
		// batched-mode investigation, 2026-05-17).
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	// Component contract: version-only payload (per-run state never persists).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

	static DPPlayerController_Component* Instance() { return s_pxInstance; }

	//==========================================================================
	// Held-item registry. Keyed by villager EntityID. Mutated by
	// DP_Player::SetHeldItem / RemoveHeldItem; read by GetHeldItem*.
	//==========================================================================
	void SetHeldItemRecord(Zenith_EntityID xVillager, const DPVillagerHeldRecord& xRec)
	{
		m_xHeldItems.Insert(xVillager, xRec);
	}

	void RemoveHeldItem(Zenith_EntityID xVillager)
	{
		m_xHeldItems.Remove(xVillager);
	}

	DP_ItemTag GetHeldItemTag(Zenith_EntityID xVillager) const
	{
		const DPVillagerHeldRecord* pxRec = m_xHeldItems.TryGet(xVillager);
		if (pxRec == nullptr) return DP_ItemTag::None;
		return pxRec->m_eTag;
	}

	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager) const
	{
		const DPVillagerHeldRecord* pxRec = m_xHeldItems.TryGet(xVillager);
		if (pxRec == nullptr) return INVALID_ENTITY_ID;
		return pxRec->m_xItem;
	}

	//==========================================================================
	// Demon-scent registry. Per-villager scalar, decays on tick + bumps
	// on successful possession (MVP-1.6 hound-target heuristic).
	//==========================================================================
	float GetDemonScent(Zenith_EntityID xVillager) const
	{
		const float* pxScent = m_xDemonScent.TryGet(xVillager);
		if (pxScent == nullptr) return 0.0f;
		return *pxScent;
	}

	void BumpDemonScent(Zenith_EntityID xVillager, float fAddAmount, float fMax)
	{
		const float* pxExisting = m_xDemonScent.TryGet(xVillager);
		float fNew = (pxExisting != nullptr ? *pxExisting : 0.0f) + fAddAmount;
		if (fNew > fMax) fNew = fMax;
		m_xDemonScent.Insert(xVillager, fNew);
	}

	// Decays every entry by fDecayPerSec * fDt and erases rows that
	// hit zero. Two-pass collect-then-remove pattern because
	// Zenith_HashMap::Iterator does not support iterate-and-erase.
	void DecayDemonScent(float fDecayPerSec, float fDt)
	{
		if (m_xDemonScent.IsEmpty()) return;
		const float fDecay = fDecayPerSec * fDt;
		Zenith_Vector<Zenith_EntityID> axDead;
		Zenith_HashMap<Zenith_EntityID, float>::Iterator it(m_xDemonScent);
		while (!it.Done())
		{
			float& fScent = it.GetValueMutable();
			fScent -= fDecay;
			if (fScent <= 0.0f) axDead.PushBack(it.GetKey());
			it.Next();
		}
		for (u_int u = 0; u < axDead.GetSize(); ++u)
		{
			m_xDemonScent.Remove(axDead.Get(u));
		}
	}

	// Iterate every (villager, scent) entry. Callback signature:
	// void(Zenith_EntityID, float). Used by DP_Player::WriteHighestScentToBlackboard.
	template <typename TFn>
	void ForEachDemonScentEntry(TFn xFn) const
	{
		Zenith_HashMap<Zenith_EntityID, float>::Iterator it(m_xDemonScent);
		while (!it.Done())
		{
			xFn(it.GetKey(), it.GetValue());
			it.Next();
		}
	}

	bool HasDemonScentEntries() const { return !m_xDemonScent.IsEmpty(); }

	void OnUpdate(const float fDt)
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

		// 2026-05-21: archetype-aware aura updates.
		//
		// BeggarStealthAura: emit while the player is possessing a
		// Beggar -- telegraphs "the priest will not pursue this body."
		// Without the visual the Beggar-ignored rule is a silent
		// BridgePerceptionToBlackboard filter the player can't observe.
		//
		// DevoutChannel: emit while DP_Player is mid-channel onto a
		// Devout target. The 0.8 s channel is otherwise invisible --
		// the player clicks, nothing happens for 0.8 s, then possession
		// snaps. The candlelight aura makes the "ritual in progress"
		// state legible.
		{
			Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
			bool bIsBeggar = xPossessed.IsValid() && DP_Player::IsBeggarVillager(xPossessed);
			DP_Particles::UpdateBeggarStealthAura(xPossessed, bIsBeggar);

			const Zenith_EntityID xChannelTarget = DP_Player::GetChannelTarget();
			const bool bChanneling = DP_Player::IsChanneling();
			DP_Particles::UpdateDevoutChannelAura(xChannelTarget, bChanneling);
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

		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
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
		g_xEngine.Input().GetMousePosition(xMousePos);

		Zenith_Maths::Matrix4 xView, xProj;
		pxCam->BuildViewMatrix(xView);
		pxCam->BuildProjectionMatrix(xProj);
		const Zenith_Maths::Matrix4 xVP = xProj * xView;

		// Pixel tolerance for picking — generous enough that a click within
		// the visual silhouette of a humanoid (~80 pixels at typical zoom)
		// still snaps onto it.
		constexpr double kMaxPixelDistSq = 120.0 * 120.0;

		// Screen-space pick context. Carried through DP_Player::ForEach-
		// VillagerInActiveScene's function-pointer callback so the villager
		// type-filter lives in DP_Player.cpp (the controller header no
		// longer needs DPVillager_Component.h). The picking math itself is
		// unchanged from the previous inline lambda.
		PickContext xCtx;
		xCtx.m_xVP        = xVP;
		xCtx.m_iW         = iW;
		xCtx.m_iH         = iH;
		xCtx.m_xMousePos  = xMousePos;
		xCtx.m_fBestSq    = kMaxPixelDistSq;

		DP_Player::ForEachVillagerInActiveScene(&PickClosestVillagerCb, &xCtx);

		if (xCtx.m_xBest.IsValid())
		{
			// Voluntary switch path -- triggers the cooldown gate. If the
			// click lands within the cooldown window (1.5 s default) the
			// possession refuses, silently. Cosmetic feedback (HUD flash /
			// audio) is a future-MVP polish item.
			DP_Player::TryVoluntaryPossessSwitch(xCtx.m_xBest);
		}
	}

	// Screen-space pick state shared between HandleClickToPossess and the
	// function-pointer callback below.
	struct PickContext
	{
		Zenith_Maths::Matrix4    m_xVP;
		int32_t                  m_iW = 0;
		int32_t                  m_iH = 0;
		Zenith_Maths::Vector2_64 m_xMousePos;
		double                   m_fBestSq = 0.0;
		Zenith_EntityID          m_xBest;
	};

	// Per-villager screen-space projection + nearest-to-cursor pick. Identical
	// math to the previous inline DP_Query lambda; relocated to a static
	// callback so DP_Player::ForEachVillagerInActiveScene can drive it without
	// the controller naming DPVillager_Component.
	static void PickClosestVillagerCb(Zenith_EntityID xId, void* pUser)
	{
		PickContext& xCtx = *static_cast<PickContext*>(pUser);
		Zenith_SceneData* pxS = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxS == nullptr) return;
		Zenith_Entity xEnt = pxS->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_Maths::Vector3 xWorld;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xWorld);
		// Aim at body centre rather than feet — the visible silhouette
		// is centred ~1 m above the entity origin.
		xWorld.y += 1.0f;
		const Zenith_Maths::Vector4 xClip = xCtx.m_xVP *
			Zenith_Maths::Vector4(xWorld.x, xWorld.y, xWorld.z, 1.0f);
		if (xClip.w <= 1e-4f) return;
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;
		const double fSx = (fNdcX + 1.0f) * 0.5f * static_cast<float>(xCtx.m_iW);
		const double fSy = (fNdcY + 1.0f) * 0.5f * static_cast<float>(xCtx.m_iH);
		const double fDx = fSx - xCtx.m_xMousePos.x;
		const double fDy = fSy - xCtx.m_xMousePos.y;
		const double fSq = fDx * fDx + fDy * fDy;
		if (fSq < xCtx.m_fBestSq) { xCtx.m_fBestSq = fSq; xCtx.m_xBest = xId; }
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
			g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxVScene == nullptr) return;
		Zenith_Entity xV = pxVScene->TryGetEntity(xVillager);
		if (!xV.IsValid() || !xV.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_Maths::Vector3 xFootPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xFootPos);

		// Resolve the item entity (may be in a different scene if
		// MoveEntityToScene was used -- defensive scene lookup).
		Zenith_SceneData* pxIScene =
			g_xEngine.Scenes().GetSceneDataForEntity(xItem);
		if (pxIScene == nullptr) return;
		Zenith_Entity xI = pxIScene->TryGetEntity(xItem);
		if (!xI.IsValid() || !xI.HasComponent<Zenith_TransformComponent>()) return;
		xI.GetComponent<Zenith_TransformComponent>().SetPosition(xFootPos);

		// Arm the item's post-drop cooldown so DPItemBase::OnUpdate
		// doesn't immediately re-pick-up from the foot position.
		DP_Items::BeginPostDropCooldownForItem(xItem);

		// Clear the held-item side-table entry. The villager's
		// floating held-item visual is rebuilt on the next OnUpdate
		// when GetHeldItemTag flips back to None.
		DP_Player::RemoveHeldItem(xVillager);
	}

	// Internal commit helper shared by the immediate-possession path and
	// the channel-completion path in DP_Player::TryVoluntaryPossessSwitch /
	// TickChannel. Was an anon-namespace free function in DP_Player.cpp;
	// promoted to a private static member so it can write the private
	// possession/anchor/scent state without befriending an unnameable
	// anon-namespace function (Phase: encapsulation of the per-run state
	// block below).
	static void CommitVoluntaryPossession(
		DPPlayerController_Component& xCtrl,
		Zenith_EntityID xId,
		const Zenith_Maths::Vector3& xNewPos,
		bool bGotNewPos);

	static inline DPPlayerController_Component* s_pxInstance = nullptr;

	Zenith_Entity m_xParentEntity;

	// State block (held-items + demon-scent maps + the per-run state
	// migrated in Phase 5.2 from PublicInterfaces.cpp anon-namespace
	// globals). PRIVATE: the DP_Player / DP_Win / DP_Night namespace
	// functions in Source/ that read + write these fields are befriended
	// individually below (the function declarations are visible because
	// this header already includes Source/PublicInterfaces.h). Lifetime
	// is tied to this component instance, which is per-scene singleton —
	// scene unload destroys every field via the component's OnDestroy.
private:
	// --- Friends: the exact DP_* namespace functions that read/write the
	//     per-run state members below. Keeping the data private while these
	//     impls (in DP_Player.cpp / DP_Win.cpp / DP_Night.cpp) reach in via
	//     Instance()->m_field requires friending each one by its exact
	//     signature. CommitVoluntaryPossession is itself a private static
	//     member (declared above), so its sole caller is in the list too.
	friend Zenith_EntityID DP_Player::GetPossessedVillager();
	friend void            DP_Player::SetPossessedVillager(Zenith_EntityID xId);
	friend bool            DP_Player::TryVoluntaryPossessSwitch(Zenith_EntityID xId);
	friend void            DP_Player::TickPossessionCooldown(float fDt);
	friend float           DP_Player::GetPossessionCooldownRemaining();
	friend void            DP_Player::WriteHighestScentToBlackboard();
	friend bool            DP_Player::IsChanneling();
	friend Zenith_EntityID DP_Player::GetChannelTarget();
	friend float           DP_Player::GetChannelRemaining();
	friend void            DP_Player::TickChannel(float fDt);
	friend void            DP_Player::InterruptChannel();
	friend void            DP_Player::ResetForNewRun();

	friend uint32_t        DP_Win::GetCollectedObjectivesMask();
	friend bool            DP_Win::HasWon();
	friend void            DP_Win::NotifyObjectiveCollected(DP_ItemTag eObjective,
	                                                        Zenith_EntityID xVillager,
	                                                        Zenith_EntityID xPentagram);
	friend void            DP_Win::Reset();

	friend void            DP_Night::StartNight(float fDurationSeconds);
	friend void            DP_Night::TickNight(float fDt);
	friend float           DP_Night::GetNightTimeRemaining();
	friend bool            DP_Night::IsNightActive();
	friend bool            DP_Night::HasDawnReached();
	friend void            DP_Night::Reset();

	// Held-item registry: one entry per villager holding an item.
	Zenith_HashMap<Zenith_EntityID, DPVillagerHeldRecord> m_xHeldItems;

	// Demon-scent registry: per-villager scalar, accumulates on possession
	// + decays per-frame via DP_Player::TickDemonScent.
	Zenith_HashMap<Zenith_EntityID, float> m_xDemonScent;

	// B2: cache for DP_Player::WriteHighestScentToBlackboard. The dirty flag
	// forces the first write after OnAwake / ResetForNewRun even when the
	// highest-scent id is INVALID (a legitimate value), so newly-created or
	// reset AI blackboards always receive an authoritative value.
	Zenith_EntityID m_xLastWrittenHighScent     = INVALID_ENTITY_ID;
	bool            m_bHighScentBlackboardDirty = true;

	// Possession state.
	Zenith_EntityID       m_xPossessedVillager       = INVALID_ENTITY_ID;
	float                 m_fPossessionCooldownSec   = 0.0f;
	Zenith_Maths::Vector3 m_xPossessionAnchor        = Zenith_Maths::Vector3(0.0f);
	bool                  m_bHasPossessionAnchor     = false;

	// Devout-channel state.
	Zenith_EntityID       m_xChannelTarget           = INVALID_ENTITY_ID;
	float                 m_fChannelRemaining        = 0.0f;
	bool                  m_bChannelActive           = false;

	// Win state (objectives bitmask + has-won flag). Pentagram delivery
	// is the only writer; HUD + tests read.
	uint32_t              m_uCollectedObjectivesMask = 0;
	bool                  m_bHasWon                  = false;

	// Night-timer state. StartNight seeds, TickNight decrements.
	float                 m_fNightRemainingSec       = 0.0f;
	bool                  m_bNightActive             = false;
	bool                  m_bDawnDispatched          = false;
};
