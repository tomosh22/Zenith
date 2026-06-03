#pragma once

/**
 * DPTelemetry - DevilsPlayground-specific event emission to the engine
 * telemetry recorder.
 *
 * Phase 2 of the verification system (2026-05-16): defines the
 * game-specific event-type enum + a Hooks helper that subscribes to
 * every DP event the verification analyzer cares about and routes
 * them into Zenith_Telemetry::Recorder.
 *
 * Layering:
 *   - Zenith_Telemetry (engine) -- generic record/read/JSON of game-
 *     agnostic record types (FrameSample + Event).
 *   - DPTelemetry (here)        -- defines what game-side events look
 *     like (DPEventType enum), name resolver for JSON export, and
 *     the Hooks RAII helper that wires DP event dispatch -> recorder.
 *   - DPTelemetryRecorder_Behaviour (Phase 2.5 / Phase 3) -- a script
 *     that owns a Hooks instance + drives the per-frame
 *     NextFrame / position-sample loop.
 *
 * Event payload conventions (per DPEventType):
 *   Possession     entityA = newly-possessed villager.
 *                  entityB = previously-possessed (INVALID on first possess).
 *   Unpossession   entityA = villager that lost possession.
 *   ItemPickup     entityA = villager. entityB = item. ints[0] = DP_ItemTag.
 *   ItemDrop       entityA = villager. entityB = item. ints[0] = DP_ItemTag.
 *   Interact       entityA = villager. entityB = target.
 *   InteractionBegin/End/Cancelled = same shape as Interact.
 *   VillagerDied   entityA = villager.
 *   Victory        no payload.
 *   RunLost        ints[0] = DP_RunLostCause as int.
 *   BellRing       entityA = villager. entityB = bell.
 *                  floats[0..2] = world position xyz.
 *   PriestStateChange  entityA = priest. ints[0] = AelfricState as int.
 *   PossessedSwitched  same as Possession; emitted by the recorder when
 *                      it detects a poll-based change in DP_Player::
 *                      GetPossessedVillager() that no Possession event
 *                      accompanied (sanity probe -- shouldn't normally fire).
 */

#include "Telemetry/Zenith_Telemetry.h"
#include "ZenithECS/Zenith_EventSystem.h"

#include "Source/PublicInterfaces.h"

#include <cstdint>

namespace DPTelemetry
{
	// =========== Event-type enum ===========
	// Stable integer IDs -- file format compatibility depends on these
	// not being reordered. Add new entries at the END before _Count.
	//
	// PossessionChanged is the canonical possession-transition event;
	// payload carries both old + new villager so subscribers can
	// distinguish first-possess (old=INVALID), un-possess (new=INVALID)
	// and voluntary-switch (both valid) without needing two separate
	// event types.
	enum class DPEventType : uint16_t
	{
		None              = 0,
		ItemPickup        = 1,
		ItemDrop          = 2,
		Interact          = 3,
		InteractionBegin  = 4,
		InteractionEnd    = 5,
		InteractionCancel = 6,
		VillagerDied      = 7,
		Victory           = 8,
		RunLost           = 9,
		BellRing          = 10,
		PriestStateChange = 11,
		PossessedSwitched = 12,
		// Phase-5-audit (2026-05-16) granular gameplay milestones. Each
		// has a corresponding DP_On* struct in PublicInterfaces.h and a
		// subscription wired in DPTelemetry::Hooks.
		PossessionChanged = 13, // entityA=old, entityB=new
		DoorOpened        = 14, // entityA=villager, entityB=door
		ChestOpened       = 15, // entityA=villager, entityB=chest
		ForgeCrafted      = 16, // entityA=villager, entityB=forge,  payload.ints[0] = output tag
		ObjectivePlaced   = 17, // entityA=villager, entityB=pentagram, payload.ints[0] = bit index 0..4

		// Telemetry-v3 (2026-05-19) additions. See PublicInterfaces.h
		// for the matching DP_On* event structs.
		ApprehendChannelStart       = 18, // entityA=priest, entityB=victim
		ApprehendChannelComplete    = 19, // entityA=priest, entityB=victim
		ApprehendChannelInterrupted = 20, // entityA=priest, entityB=victim, ints[0]=DP_ApprehendInterruptReason
		PauseToggle                 = 21, // ints[0]=1 if pausing, 0 if unpausing
		PerceptionContactBegin      = 22, // entityA=observer, entityB=target, ints[0]=stimulus mask, floats[0]=awareness
		PerceptionContactEnd        = 23, // entityA=observer, entityB=target, ints[0]=stimulus mask
		// 2026-05-25: doors became two-way (F-press an open door closes
		// it). DoorClosed mirrors DoorOpened so the analyser can pair
		// them on the timeline.
		DoorClosed                  = 24, // entityA=villager, entityB=door

		_Count
	};

	// Priest behaviour-tree branch enum, packed into
	// EntitySnapshot::uAIIntent for the priest entity. Stable IDs --
	// downstream consumers (visualiser) interpret these directly.
	enum class PriestIntent : uint8_t
	{
		None        = 0,
		Idle        = 1,
		Patrol      = 2,
		Investigate = 3,
		Pursue      = 4,
		Apprehend   = 5,
	};

	// Returns nullptr for unknown values (current visualiser falls back
	// to the integer form in that case).
	const char* PriestIntentToString(uint8_t uIntent);

	// Game-side resolver used by the JSON exporter. Returns nullptr for
	// unknown values so the exporter falls back to the numeric form.
	const char* DPEventTypeToString(uint16_t uType);

	// =========== EntitySnapshot state-flag bit constants ===========
	// Packed into Zenith_Telemetry::EntitySnapshot::uStateFlags for
	// villager / priest / item entities so the analyzer can tell which
	// frames featured sprint, walk-quiet, possession, etc.
	namespace StateFlags
	{
		static constexpr uint32_t Possessed        = 1u << 0;
		static constexpr uint32_t Sprinting        = 1u << 1;
		static constexpr uint32_t WalkQuiet        = 1u << 2;
		static constexpr uint32_t Alive            = 1u << 3;
		static constexpr uint32_t HoldingItem      = 1u << 4;
		static constexpr uint32_t PriestSuspicious = 1u << 5;
		static constexpr uint32_t PriestPursuing   = 1u << 6;
		// Phase-5-audit follow-up (2026-05-17): entity-role tag, set
		// by the test's per-frame sampler when iterating
		// Priest_Behaviour. Lets the visualiser classify the priest
		// deterministically even when no Suspicious/Pursuing state is
		// active -- without this bit, an idle-patrolling priest had
		// flags=0 and fell into the "Unknown / priest?" legend bucket.
		static constexpr uint32_t IsPriest = 1u << 7;
	}

	// =========== Emit helpers ===========
	// Lower-level than Hooks: callers that want to emit a custom event
	// (e.g. solver hints, bot decisions) without dispatching a full
	// engine event can use these directly.
	//
	// szSource (v3) is an optional short identifier for the emitting
	// subsystem ("Priest_BT", "PauseCtrl", "PerceptionPoll", ...). Goes
	// into EventPayload::szSource so the timeline can disambiguate
	// multiple callers of the same semantic event.
	void EmitEvent(DPEventType eType,
	               Zenith_EntityID xA = Zenith_EntityID{},
	               Zenith_EntityID xB = Zenith_EntityID{},
	               int32_t iInt0 = 0,
	               float fFloat0 = 0.0f,
	               const char* szLabel = nullptr,
	               const char* szSource = nullptr);

	// =========== Hooks RAII helper ===========
	// On construction: subscribes to every DP event listed in DPEventType
	// (ItemPickup, Victory, RunLost, BellRing, VillagerDied, all 4
	// interaction variants). Each subscription forwards into
	// Zenith_Telemetry::GetRecorder().RecordEvent(...) with the conventions
	// documented above.
	//
	// On destruction: unsubscribes every handle (no dangling captures).
	//
	// Use pattern:
	//   {
	//       DPTelemetry::Hooks xHooks;
	//       Zenith_Telemetry::GetRecorder().Begin(...);
	//       // ... run gameplay, dispatch events ...
	//       Zenith_Telemetry::GetRecorder().End(...);
	//   } // Hooks unsubscribes here.
	//
	// Non-copyable, non-movable. Construct once per recording session.
	class Hooks
	{
	public:
		Hooks();
		~Hooks();

		Hooks(const Hooks&)            = delete;
		Hooks& operator=(const Hooks&) = delete;
		Hooks(Hooks&&)                 = delete;
		Hooks& operator=(Hooks&&)      = delete;

	private:
		Zenith_EventHandle m_xPickup           = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteract         = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteractBegin    = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteractEnd      = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteractCancel   = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xDied             = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xVictory          = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xRunLost          = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xBellRing         = INVALID_EVENT_HANDLE;
		// Phase-5-audit (2026-05-16) granular event subscriptions.
		Zenith_EventHandle m_xPossessChanged   = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xDoorOpened       = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xDoorClosed       = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xChestOpened      = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xForgeCrafted     = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xObjectivePlaced  = INVALID_EVENT_HANDLE;
		// Telemetry-v3 (2026-05-19) subscriptions.
		Zenith_EventHandle m_xApprehendStart       = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xApprehendComplete    = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xApprehendInterrupted = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xPauseToggle          = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xPerceptionBegin      = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xPerceptionEnd        = INVALID_EVENT_HANDLE;
	};
}
