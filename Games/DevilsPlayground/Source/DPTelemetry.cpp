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
		case DPEventType::Possession:        return "Possession";
		case DPEventType::Unpossession:      return "Unpossession";
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
		case DPEventType::ChestOpened:       return "ChestOpened";
		case DPEventType::ForgeCrafted:      return "ForgeCrafted";
		case DPEventType::ObjectivePlaced:   return "ObjectivePlaced";
		case DPEventType::_Count:
		default:
			return nullptr;
		}
	}

	// =========================================================
	// Low-level emit. All Hooks subscriptions route through here.
	// =========================================================
	void EmitEvent(DPEventType eType,
	               Zenith_EntityID xA,
	               Zenith_EntityID xB,
	               int32_t iInt0,
	               float fFloat0,
	               const char* szLabel)
	{
		Zenith_Telemetry::Event xE;
		xE.uEventType = static_cast<uint16_t>(eType);
		xE.xPayload.xEntityA = xA;
		xE.xPayload.xEntityB = xB;
		xE.xPayload.aiInts[0] = iInt0;
		xE.xPayload.afFloats[0] = fFloat0;
		if (szLabel != nullptr)
		{
			// Manual byte-by-byte copy. MSVC's /W4 + treat-as-errors flags
			// std::strncpy as deprecated; manual copy avoids the warning
			// without a SECURE_NO_WARNINGS macro that would silence other
			// useful diagnostics.
			const size_t uMax = sizeof(xE.xPayload.szLabel) - 1u;
			size_t uI = 0;
			for (; uI < uMax && szLabel[uI] != '\0'; ++uI)
			{
				xE.xPayload.szLabel[uI] = szLabel[uI];
			}
			xE.xPayload.szLabel[uI] = '\0';
		}
		Zenith_Telemetry::GetRecorder().RecordEvent(xE);
	}

	// =========================================================
	// Hooks: RAII subscription holder.
	// =========================================================
	Hooks::Hooks()
	{
		auto& xDisp = Zenith_EventDispatcher::Get();

		m_xPickup = xDisp.SubscribeLambda<DP_OnItemPickedUp>(
			[](const DP_OnItemPickedUp& xEvt)
			{
				// Look up the picked-up item's tag for the analyzer so it
				// can distinguish iron / objective / reagent without
				// querying back into DP_Items at replay time.
				const DP_ItemTag eTag = DP_Items::GetItemTag(xEvt.m_xItem);
				EmitEvent(DPEventType::ItemPickup, xEvt.m_xVillager, xEvt.m_xItem,
					static_cast<int32_t>(eTag), 0.0f, DP_ItemTagToString(eTag));
			});

		m_xInteract = xDisp.SubscribeLambda<DP_OnInteract>(
			[](const DP_OnInteract& xEvt)
			{
				EmitEvent(DPEventType::Interact, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xInteractBegin = xDisp.SubscribeLambda<DP_OnInteractionBegin>(
			[](const DP_OnInteractionBegin& xEvt)
			{
				EmitEvent(DPEventType::InteractionBegin, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xInteractEnd = xDisp.SubscribeLambda<DP_OnInteractionEnd>(
			[](const DP_OnInteractionEnd& xEvt)
			{
				EmitEvent(DPEventType::InteractionEnd, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xInteractCancel = xDisp.SubscribeLambda<DP_OnInteractionCancelled>(
			[](const DP_OnInteractionCancelled& xEvt)
			{
				EmitEvent(DPEventType::InteractionCancel, xEvt.m_xVillager, xEvt.m_xTarget);
			});

		m_xDied = xDisp.SubscribeLambda<DP_OnVillagerDied>(
			[](const DP_OnVillagerDied& xEvt)
			{
				EmitEvent(DPEventType::VillagerDied, xEvt.m_xVillager);
			});

		m_xVictory = xDisp.SubscribeLambda<DP_OnVictory>(
			[](const DP_OnVictory&)
			{
				EmitEvent(DPEventType::Victory);
			});

		m_xRunLost = xDisp.SubscribeLambda<DP_OnRunLost>(
			[](const DP_OnRunLost& xEvt)
			{
				EmitEvent(DPEventType::RunLost,
					Zenith_EntityID{}, Zenith_EntityID{},
					static_cast<int32_t>(xEvt.m_eCause), 0.0f,
					/*szLabel=*/nullptr);
			});

		m_xBellRing = xDisp.SubscribeLambda<DP_OnBellRing>(
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
		m_xPossessChanged = xDisp.SubscribeLambda<DP_OnPossessionChanged>(
			[](const DP_OnPossessionChanged& xEvt)
			{
				// Always emit the unified PossessionChanged event with
				// both old + new in the payload.
				EmitEvent(DPEventType::PossessionChanged,
					xEvt.m_xOldVillager, xEvt.m_xNewVillager);
				// Convenience legacy events so existing
				// analyzer / external tooling that key on Possession
				// vs Unpossession alone still get a discrete signal.
				if (xEvt.m_xNewVillager.IsValid())
				{
					EmitEvent(DPEventType::Possession,
						xEvt.m_xNewVillager, xEvt.m_xOldVillager);
				}
				else if (xEvt.m_xOldVillager.IsValid())
				{
					EmitEvent(DPEventType::Unpossession, xEvt.m_xOldVillager);
				}
			});

		m_xDoorOpened = xDisp.SubscribeLambda<DP_OnDoorOpened>(
			[](const DP_OnDoorOpened& xEvt)
			{
				EmitEvent(DPEventType::DoorOpened, xEvt.m_xVillager, xEvt.m_xDoor);
			});

		m_xChestOpened = xDisp.SubscribeLambda<DP_OnChestOpened>(
			[](const DP_OnChestOpened& xEvt)
			{
				EmitEvent(DPEventType::ChestOpened, xEvt.m_xVillager, xEvt.m_xChest);
			});

		m_xForgeCrafted = xDisp.SubscribeLambda<DP_OnForgeCrafted>(
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

		m_xObjectivePlaced = xDisp.SubscribeLambda<DP_OnObjectivePlaced>(
			[](const DP_OnObjectivePlaced& xEvt)
			{
				EmitEvent(DPEventType::ObjectivePlaced,
					xEvt.m_xVillager, xEvt.m_xPentagram,
					xEvt.m_iObjectiveBitIndex);
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
		if (m_xChestOpened     != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xChestOpened);
		if (m_xForgeCrafted    != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xForgeCrafted);
		if (m_xObjectivePlaced != INVALID_EVENT_HANDLE) xDisp.Unsubscribe(m_xObjectivePlaced);
	}
}
