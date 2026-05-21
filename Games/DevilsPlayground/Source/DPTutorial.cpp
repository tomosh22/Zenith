#include "Zenith.h"

#include "Source/DPTutorial.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "EntityComponent/Zenith_EventSystem.h"

#include <cstring>

namespace DP_Tutorial
{
	namespace
	{
		constexpr int kNumKinds = static_cast<int>(Kind::COUNT);

		// Per-kind text. Indexed by Kind. Strings are static literals
		// so the GetActiveTipText pointer stays valid across frames
		// without ownership concerns.
		constexpr const char* kKindText[kNumKinds] = {
			/*FirstPossession*/        "You are now in this villager. They burn out in 30 s -- find an objective and reach the pentagram before then.",
			/*FirstIronPickup*/        "That's Iron. Carry it to a Forge to craft a Key.",
			/*FirstKeyCrafted*/        "The Forge produced a Key. Use it to unlock a door.",
			/*FirstLockedDoor*/        "Locked. You need a matching Key in hand. Forge one from Iron.",
			/*FirstDoorUnlocked*/      "Door open. Objectives are usually behind doors.",
			/*FirstObjectivePickup*/   "An objective -- carry it to the pentagram in the centre of the village.",
			/*FirstObjectiveDelivered*/"Objective delivered. Four more for the ritual.",
			/*FirstPriestSpotted*/     "Aelfric sees you! Break line of sight, then [Shift] to sprint away.",
			/*FirstPriestInvestigate*/ "Aelfric heard something. Hold [Ctrl] to walk quietly while he investigates.",
			/*FirstBurnout*/           "The body burned out. Click another villager to possess them. The demon doesn't die -- only its hosts.",
			/*FirstSprintUse*/         "Sprint costs life. Faster, but the timer drains 1.5x.",
			/*FirstWalkQuietUse*/      "Walk quiet halves your footsteps. Aelfric can't hear you as far.",
			/*FirstBellSoulRing*/      "The BellSoul rang -- every priest on the map heard it. Move fast.",
			/*FirstBogWaterEvaporate*/ "BogWater evaporates 8 s after you drop it. Don't drop it until you need to.",
		};

		// Shown-flag table + active tip storage.
		bool        g_abShown[kNumKinds] = { false };
		uint32_t    g_uShownCount = 0;
		const char* g_szActiveText  = nullptr;
		float       g_fActiveRemain = 0.0f;
		bool        g_bInitialized  = false;

		Zenith_EventHandle g_xSubPossess        = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubPickup         = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubForge          = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubLocked         = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubDoorOpened     = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubObjective      = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubPriestAlerted  = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubVillagerDied   = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubBellRing       = INVALID_EVENT_HANDLE;
		Zenith_EventHandle g_xSubEvaporated     = INVALID_EVENT_HANDLE;

		void Show(Kind eKind)
		{
			const int i = static_cast<int>(eKind);
			if (i < 0 || i >= kNumKinds) return;
			if (g_abShown[i]) return;
			g_abShown[i] = true;
			++g_uShownCount;
			g_szActiveText  = kKindText[i];
			g_fActiveRemain = kDefaultTipDurationSeconds;
		}

		// Event handlers. Each translates an event into Show(Kind) IF
		// the trigger condition holds. Side-effect-free beyond Show.

		void OnPossessionChanged(const DP_OnPossessionChanged& xEv)
		{
			// First possession when the new villager is valid (treats
			// unpossess as not a possession). Avoids firing on every
			// villager swap.
			if (xEv.m_xNewVillager.IsValid()) Show(Kind::FirstPossession);
		}

		void OnPickup(const DP_OnItemPickedUp& xEv)
		{
			switch (xEv.m_eTag)
			{
			case DP_ItemTag::Iron:
				Show(Kind::FirstIronPickup);
				break;
			case DP_ItemTag::Objective1:
			case DP_ItemTag::Objective2:
			case DP_ItemTag::Objective3:
			case DP_ItemTag::Objective4:
			case DP_ItemTag::Objective5:
				Show(Kind::FirstObjectivePickup);
				break;
			case DP_ItemTag::BellSoul:
				Show(Kind::FirstBellSoulRing);
				break;
			default:
				// Other tags (Key picked up loose, Spike, etc) don't
				// have first-encounter tips today.
				break;
			}
		}

		void OnForgeCrafted(const DP_OnForgeCrafted& /*xEv*/)
		{
			// MVP default recipe is Iron -> Key. Post-MVP recipes
			// (Iron+Wood -> Spike, Iron+Brass -> SkeletonKey) would
			// want their own tips; deferred.
			Show(Kind::FirstKeyCrafted);
		}

		void OnLockedDoor(const DP_OnDoorLockRejected& /*xEv*/)
		{
			Show(Kind::FirstLockedDoor);
		}

		void OnDoorOpened(const DP_OnDoorOpened& /*xEv*/)
		{
			Show(Kind::FirstDoorUnlocked);
		}

		void OnObjectivePlaced(const DP_OnObjectivePlaced& /*xEv*/)
		{
			Show(Kind::FirstObjectiveDelivered);
		}

		void OnPriestAlerted(const DP_OnPriestAlerted& xEv)
		{
			// Distinct tips per alert kind so the player learns the
			// behaviours separately. Apprehend doesn't get its own
			// tutorial (the run-lost banner covers it).
			if      (xEv.m_eKind == DP_PriestAlertKind::SawTarget)  Show(Kind::FirstPriestSpotted);
			else if (xEv.m_eKind == DP_PriestAlertKind::HeardNoise) Show(Kind::FirstPriestInvestigate);
		}

		void OnVillagerDied(const DP_OnVillagerDied& xEv)
		{
			// Only fire for the possessed villager -- "your" villager
			// burning out is the teaching moment. NPCs dying for any
			// other reason don't warrant the tip.
			const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
			if (xEv.m_xVillager == xPossessed) Show(Kind::FirstBurnout);
		}

		void OnBellRing(const DP_OnBellRing& /*xEv*/)
		{
			Show(Kind::FirstBellSoulRing);
		}

		void OnItemEvaporated(const DP_OnItemEvaporated& xEv)
		{
			if (xEv.m_eTag == DP_ItemTag::BogWater) Show(Kind::FirstBogWaterEvaporate);
		}
	}

	// =====================================================================
	// Public API
	// =====================================================================

	void Initialize()
	{
		if (g_bInitialized) return;
		auto& xDispatcher = Zenith_EventDispatcher::Get();
		g_xSubPossess       = xDispatcher.Subscribe<DP_OnPossessionChanged>(&OnPossessionChanged);
		g_xSubPickup        = xDispatcher.Subscribe<DP_OnItemPickedUp>(&OnPickup);
		g_xSubForge         = xDispatcher.Subscribe<DP_OnForgeCrafted>(&OnForgeCrafted);
		g_xSubLocked        = xDispatcher.Subscribe<DP_OnDoorLockRejected>(&OnLockedDoor);
		g_xSubDoorOpened    = xDispatcher.Subscribe<DP_OnDoorOpened>(&OnDoorOpened);
		g_xSubObjective     = xDispatcher.Subscribe<DP_OnObjectivePlaced>(&OnObjectivePlaced);
		g_xSubPriestAlerted = xDispatcher.Subscribe<DP_OnPriestAlerted>(&OnPriestAlerted);
		g_xSubVillagerDied  = xDispatcher.Subscribe<DP_OnVillagerDied>(&OnVillagerDied);
		g_xSubBellRing      = xDispatcher.Subscribe<DP_OnBellRing>(&OnBellRing);
		g_xSubEvaporated    = xDispatcher.Subscribe<DP_OnItemEvaporated>(&OnItemEvaporated);
		ResetForNewRun();
		g_bInitialized = true;
	}

	void Shutdown()
	{
		if (!g_bInitialized) return;
		auto& xDispatcher = Zenith_EventDispatcher::Get();
		xDispatcher.Unsubscribe(g_xSubPossess);
		xDispatcher.Unsubscribe(g_xSubPickup);
		xDispatcher.Unsubscribe(g_xSubForge);
		xDispatcher.Unsubscribe(g_xSubLocked);
		xDispatcher.Unsubscribe(g_xSubDoorOpened);
		xDispatcher.Unsubscribe(g_xSubObjective);
		xDispatcher.Unsubscribe(g_xSubPriestAlerted);
		xDispatcher.Unsubscribe(g_xSubVillagerDied);
		xDispatcher.Unsubscribe(g_xSubBellRing);
		xDispatcher.Unsubscribe(g_xSubEvaporated);
		g_xSubPossess       = INVALID_EVENT_HANDLE;
		g_xSubPickup        = INVALID_EVENT_HANDLE;
		g_xSubForge         = INVALID_EVENT_HANDLE;
		g_xSubLocked        = INVALID_EVENT_HANDLE;
		g_xSubDoorOpened    = INVALID_EVENT_HANDLE;
		g_xSubObjective     = INVALID_EVENT_HANDLE;
		g_xSubPriestAlerted = INVALID_EVENT_HANDLE;
		g_xSubVillagerDied  = INVALID_EVENT_HANDLE;
		g_xSubBellRing      = INVALID_EVENT_HANDLE;
		g_xSubEvaporated    = INVALID_EVENT_HANDLE;
		ResetForNewRun();
		g_bInitialized = false;
	}

	void ResetForNewRun()
	{
		for (int i = 0; i < kNumKinds; ++i) g_abShown[i] = false;
		g_uShownCount   = 0;
		g_szActiveText  = nullptr;
		g_fActiveRemain = 0.0f;
	}

	void TriggerIfFirstTime(Kind eKind)
	{
		Show(eKind);
	}

	void Tick(float fDt)
	{
		if (g_fActiveRemain <= 0.0f) return;
		g_fActiveRemain -= fDt;
		if (g_fActiveRemain <= 0.0f)
		{
			g_fActiveRemain = 0.0f;
			g_szActiveText  = nullptr;
		}
	}

	const char* GetActiveTipText()      { return g_szActiveText; }
	float       GetActiveTipRemaining() { return g_fActiveRemain; }

	bool IsTipShown(Kind eKind)
	{
		const int i = static_cast<int>(eKind);
		if (i < 0 || i >= kNumKinds) return false;
		return g_abShown[i];
	}

	uint32_t GetShownCount() { return g_uShownCount; }

	const char* GetTipTextForKind(Kind eKind)
	{
		const int i = static_cast<int>(eKind);
		if (i < 0 || i >= kNumKinds) return nullptr;
		return kKindText[i];
	}
}
