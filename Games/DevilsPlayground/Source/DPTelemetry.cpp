#include "Zenith.h"
#include "Source/DPTelemetry.h"
#include "Source/DevilsPlayground_Tags.h"

#include "Telemetry/Zenith_Telemetry.h"

#include <cstring>

namespace DPTelemetry
{
	// =========================================================
	// DPEventType -> string. Keep in sync with the enum.
	// =========================================================
	const char* DPEventTypeToString(uint16_t uType)
	{
		switch (static_cast<DPEventType>(uType))
		{
		case DPEventType::None:              return "None";
		case DPEventType::ItemPickup:        return "ItemPickup";
		case DPEventType::ItemDrop:          return "ItemDrop";
		case DPEventType::Interact:          return "Interact";
		case DPEventType::InteractionBegin:  return "InteractionBegin";
		case DPEventType::InteractionEnd:    return "InteractionEnd";
		case DPEventType::InteractionCancel: return "InteractionCancel";
		case DPEventType::VillagerDied:      return "VillagerDied";
		case DPEventType::Victory:           return "Victory";
		case DPEventType::RunLost:           return "RunLost";
		case DPEventType::BellRing:          return "BellRing";
		case DPEventType::PriestStateChange: return "PriestStateChange";
		case DPEventType::PossessedSwitched: return "PossessedSwitched";
		case DPEventType::PossessionChanged: return "PossessionChanged";
		case DPEventType::DoorOpened:        return "DoorOpened";
		case DPEventType::DoorClosed:        return "DoorClosed";
		case DPEventType::ChestOpened:       return "ChestOpened";
		case DPEventType::ForgeCrafted:      return "ForgeCrafted";
		case DPEventType::ObjectivePlaced:   return "ObjectivePlaced";
		case DPEventType::ApprehendChannelStart:       return "ApprehendChannelStart";
		case DPEventType::ApprehendChannelComplete:    return "ApprehendChannelComplete";
		case DPEventType::ApprehendChannelInterrupted: return "ApprehendChannelInterrupted";
		case DPEventType::PauseToggle:                 return "PauseToggle";
		case DPEventType::PerceptionContactBegin:      return "PerceptionContactBegin";
		case DPEventType::PerceptionContactEnd:        return "PerceptionContactEnd";
		case DPEventType::_Count:
		default:
			return nullptr;
		}
	}

	const char* PriestIntentToString(uint8_t uIntent)
	{
		switch (static_cast<PriestIntent>(uIntent))
		{
		case PriestIntent::None:        return "None";
		case PriestIntent::Idle:        return "Idle";
		case PriestIntent::Patrol:      return "Patrol";
		case PriestIntent::Investigate: return "Investigate";
		case PriestIntent::Pursue:      return "Pursue";
		case PriestIntent::Apprehend:   return "Apprehend";
		default:                        return nullptr;
		}
	}

	// =========================================================
	// Low-level emit. All Hooks subscriptions route through here.
	// =========================================================
	// Local helper -- copy a C-string into a fixed buffer with manual
	// byte-by-byte fill (avoids MSVC's std::strncpy deprecation warning
	// under /W4 + treat-as-errors).
	static void CopyToFixed(char* pszDst, size_t uDstSize, const char* pszSrc)
	{
		if (pszDst == nullptr || uDstSize == 0u) return;
		if (pszSrc == nullptr) { pszDst[0] = '\0'; return; }
		const size_t uMax = uDstSize - 1u;
		size_t uI = 0;
		for (; uI < uMax && pszSrc[uI] != '\0'; ++uI)
		{
			pszDst[uI] = pszSrc[uI];
		}
		pszDst[uI] = '\0';
	}

	void EmitEvent(DPEventType eType,
	               Zenith_EntityID xA,
	               Zenith_EntityID xB,
	               int32_t iInt0,
	               float fFloat0,
	               const char* szLabel,
	               const char* szSource)
	{
		Zenith_Telemetry::Event xE;
		xE.uEventType = static_cast<uint16_t>(eType);
		xE.xPayload.xEntityA = xA;
		xE.xPayload.xEntityB = xB;
		xE.xPayload.aiInts[0] = iInt0;
		xE.xPayload.afFloats[0] = fFloat0;
		CopyToFixed(xE.xPayload.szLabel,  sizeof(xE.xPayload.szLabel),  szLabel);
		CopyToFixed(xE.xPayload.szSource, sizeof(xE.xPayload.szSource), szSource);
		Zenith_Telemetry::GetRecorder().RecordEvent(xE);
	}

	// =========================================================
	// Hooks: RAII subscription holder.
	// =========================================================
	Hooks::Hooks()
	{
		auto& xDisp = Zenith_EventDispatcher::Get();

		m_xPickup = xDisp.Subscribe<DP_OnItemPickedUp>(
			[](const DP_OnItemPickedUp& xEvt)
			{
				// Look up the picked-up item's tag for the analyzer so it
				// can distinguish iron / objective / reagent without
				// querying back into DP_Items at replay time.
				const DP_ItemTag eTag = DP_Items::GetItemTag(xEvt.m_xItem);
				EmitEvent(DPEventType::ItemPickup, xEvt.m_xVillager, xEvt.m_xItem,
					static_cast<int32_t>(eTag), 0.0f, DP_ItemTagToString(eTag));
			});

		m_xInteract = xDisp.Subscribe<DP_OnInteract>(
			[](const DP_OnInteract& xEvt)
			{
				EmitEvent(DPEventType::Interact, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xInteractBegin = xDisp.Subscribe<DP_OnInteractionBegin>(
			[](const DP_OnInteractionBegin& xEvt)
			{
				EmitEvent(DPEventType::InteractionBegin, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xInteractEnd = xDisp.Subscribe<DP_OnInteractionEnd>(
			[](const DP_OnInteractionEnd& xEvt)
			{
				EmitEvent(DPEventType::InteractionEnd, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xInteractCancel = xDisp.Subscribe<DP_OnInteractionCancelled>(
			[](const DP_OnInteractionCancelled& xEvt)
			{
				EmitEvent(DPEventType::InteractionCancel, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xDied = xDisp.Subscribe<DP_OnVillagerDied>(
			[](const DP_OnVillagerDied& xEvt)
			{
				EmitEvent(DPEventType::VillagerDied, xEvt.m_xVillager);
			});

		m_xVictory = xDisp.Subscribe<DP_OnVictory>(
			[](const DP_OnVictory& xEvt)
			{
				// entityA = winning villager, entityB = pentagram.
				// Either may be INVALID if the dispatcher (legacy test
				// call to DP_Win::NotifyObjectiveCollected without
				// scene context) didn't supply them; the visualiser's
				// FindEntityPosAtFrame falls back to bounds-centre in
				// that case, which is the documented behaviour.
				EmitEvent(DPEventType::Victory,
					xEvt.m_xVillager, xEvt.m_xPentagram);
			});

		m_xRunLost = xDisp.Subscribe<DP_OnRunLost>(
			[](const DP_OnRunLost& xEvt)
			{
				// entityA = primary actor (caught villager / dead
				//                          villager / last-possessed villager)
				// entityB = secondary actor (the priest, on Apprehended)
				// ints[0] = cause enum for downstream filtering.
				EmitEvent(DPEventType::RunLost,
					xEvt.m_xVillager, xEvt.m_xOther,
					static_cast<int32_t>(xEvt.m_eCause), 0.0f,
					/*szLabel=*/nullptr);
			});

		m_xBellRing = xDisp.Subscribe<DP_OnBellRing>(
			[](const DP_OnBellRing& xEvt)
			{
				Zenith_Telemetry::Event xE;
				xE.uEventType = static_cast<uint16_t>(DPEventType::BellRing);
				xE.xPayload.xEntityA = xEvt.m_xVillager;
				xE.xPayload.xEntityB = xEvt.m_xBellSoul;
				xE.xPayload.afFloats[0] = xEvt.m_xPosition.x;
				xE.xPayload.afFloats[1] = xEvt.m_xPosition.y;
				xE.xPayload.afFloats[2] = xEvt.m_xPosition.z;
				Zenith_Telemetry::GetRecorder().RecordEvent(xE);
			});

		// ----- Phase-5-audit (2026-05-16) granular gameplay events -----
		m_xPossessChanged = xDisp.Subscribe<DP_OnPossessionChanged>(
			[](const DP_OnPossessionChanged& xEvt)
			{
				// Unified PossessionChanged event with both old + new
				// in the payload -- subscribers distinguish
				// first-possess / un-possess / voluntary-switch by
				// inspecting which entity IDs are valid.
				EmitEvent(DPEventType::PossessionChanged,
					xEvt.m_xOldVillager, xEvt.m_xNewVillager);
			});

		m_xDoorOpened = xDisp.Subscribe<DP_OnDoorOpened>(
			[](const DP_OnDoorOpened& xEvt)
			{
				EmitEvent(DPEventType::DoorOpened, xEvt.m_xVillager, xEvt.m_xDoor);
			});

		// 2026-05-25: mirror DoorOpened. Doors became two-way; DoorClosed
		// fires on the Open -> Closing transition (the F-press that
		// swings the door shut, not the natural end of the close anim).
		m_xDoorClosed = xDisp.Subscribe<DP_OnDoorClosed>(
			[](const DP_OnDoorClosed& xEvt)
			{
				EmitEvent(DPEventType::DoorClosed, xEvt.m_xVillager, xEvt.m_xDoor);
			});

		m_xChestOpened = xDisp.Subscribe<DP_OnChestOpened>(
			[](const DP_OnChestOpened& xEvt)
			{
				EmitEvent(DPEventType::ChestOpened, xEvt.m_xVillager, xEvt.m_xChest);
			});

		m_xForgeCrafted = xDisp.Subscribe<DP_OnForgeCrafted>(
			[](const DP_OnForgeCrafted& xEvt)
			{
				// Output entity ID stashed in entityB's "second" slot
				// is unusual; we use ints[0] = -1 here as the tag is
				// not yet known here (forge sets up the item type but
				// the tag itself is a runtime side-table lookup). The
				// label gives the receiver a human-readable hint via
				// DP_ItemTagToString when the item-tag is resolved.
				const DP_ItemTag eTag = DP_Items::GetItemTag(xEvt.m_xOutputItem);
				EmitEvent(DPEventType::ForgeCrafted,
					xEvt.m_xVillager, xEvt.m_xForge,
					static_cast<int32_t>(eTag), 0.0f,
					DP_ItemTagToString(eTag));
			});

		m_xObjectivePlaced = xDisp.Subscribe<DP_OnObjectivePlaced>(
			[](const DP_OnObjectivePlaced& xEvt)
			{
				EmitEvent(DPEventType::ObjectivePlaced,
					xEvt.m_xVillager, xEvt.m_xPentagram,
					xEvt.m_iObjectiveBitIndex);
			});

		// ----- Telemetry-v3 (2026-05-19) subscriptions -----
		m_xApprehendStart = xDisp.Subscribe<DP_OnApprehendChannelStart>(
			[](const DP_OnApprehendChannelStart& xEvt)
			{
				EmitEvent(DPEventType::ApprehendChannelStart,
					xEvt.m_xPriest, xEvt.m_xVictim,
					/*iInt0=*/0, /*fFloat0=*/0.0f,
					/*szLabel=*/nullptr, /*szSource=*/"Priest_BT");
			});

		m_xApprehendComplete = xDisp.Subscribe<DP_OnApprehendChannelComplete>(
			[](const DP_OnApprehendChannelComplete& xEvt)
			{
				EmitEvent(DPEventType::ApprehendChannelComplete,
					xEvt.m_xPriest, xEvt.m_xVictim,
					/*iInt0=*/0, /*fFloat0=*/0.0f,
					/*szLabel=*/nullptr, /*szSource=*/"Priest_BT");
			});

		m_xApprehendInterrupted = xDisp.Subscribe<DP_OnApprehendChannelInterrupted>(
			[](const DP_OnApprehendChannelInterrupted& xEvt)
			{
				EmitEvent(DPEventType::ApprehendChannelInterrupted,
					xEvt.m_xPriest, xEvt.m_xVictim,
					static_cast<int32_t>(xEvt.m_eReason), 0.0f,
					/*szLabel=*/nullptr, /*szSource=*/"Priest_BT");
			});

		m_xPauseToggle = xDisp.Subscribe<DP_OnPauseToggle>(
			[](const DP_OnPauseToggle& xEvt)
			{
				EmitEvent(DPEventType::PauseToggle,
					Zenith_EntityID{}, Zenith_EntityID{},
					xEvt.m_bIsPaused ? 1 : 0, 0.0f,
					/*szLabel=*/nullptr, /*szSource=*/"PauseCtrl");
			});

		m_xPerceptionBegin = xDisp.Subscribe<DP_OnPerceptionContactBegin>(
			[](const DP_OnPerceptionContactBegin& xEvt)
			{
				EmitEvent(DPEventType::PerceptionContactBegin,
					xEvt.m_xObserver, xEvt.m_xTarget,
					static_cast<int32_t>(xEvt.m_uStimulusMask),
					xEvt.m_fAwareness,
					/*szLabel=*/nullptr, /*szSource=*/"PerceptionPoll");
			});

		m_xPerceptionEnd = xDisp.Subscribe<DP_OnPerceptionContactEnd>(
			[](const DP_OnPerceptionContactEnd& xEvt)
			{
				EmitEvent(DPEventType::PerceptionContactEnd,
					xEvt.m_xObserver, xEvt.m_xTarget,
					static_cast<int32_t>(xEvt.m_uStimulusMask), 0.0f,
					/*szLabel=*/nullptr, /*szSource=*/"PerceptionPoll");
			});
	}

	Hooks::~Hooks()
	{
		auto& xDisp = Zenith_EventDispatcher::Get();
		if (m_xPickup          != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xPickup);
		if (m_xInteract        != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xInteract);
		if (m_xInteractBegin   != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xInteractBegin);
		if (m_xInteractEnd     != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xInteractEnd);
		if (m_xInteractCancel  != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xInteractCancel);
		if (m_xDied            != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xDied);
		if (m_xVictory         != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xVictory);
		if (m_xRunLost         != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xRunLost);
		if (m_xBellRing        != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xBellRing);
		if (m_xPossessChanged  != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xPossessChanged);
		if (m_xDoorOpened      != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xDoorOpened);
		if (m_xDoorClosed      != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xDoorClosed);
		if (m_xChestOpened     != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xChestOpened);
		if (m_xForgeCrafted    != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xForgeCrafted);
		if (m_xObjectivePlaced != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xObjectivePlaced);
		// v3 (2026-05-19)
		if (m_xApprehendStart       != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xApprehendStart);
		if (m_xApprehendComplete    != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xApprehendComplete);
		if (m_xApprehendInterrupted != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xApprehendInterrupted);
		if (m_xPauseToggle          != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xPauseToggle);
		if (m_xPerceptionBegin      != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xPerceptionBegin);
		if (m_xPerceptionEnd        != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xPerceptionEnd);
	}
}
