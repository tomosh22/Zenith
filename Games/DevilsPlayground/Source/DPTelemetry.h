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
#include "EntityComponent/Zenith_EventSystem.h"

#include "Source/PublicInterfaces.h"

#include <cstdint>

namespace DPTelemetry
{
	// =========== Event-type enum ===========
	// Stable integer IDs -- file format compatibility depends on these
	// not being reordered. Add new entries at the END before _Count.
	enum class DPEventType : uint16_t
	{
		None              = 0,
		Possession        = 1,
		Unpossession      = 2,
		ItemPickup        = 3,
		ItemDrop          = 4,
		Interact          = 5,
		InteractionBegin  = 6,
		InteractionEnd    = 7,
		InteractionCancel = 8,
		VillagerDied      = 9,
		Victory           = 10,
		RunLost           = 11,
		BellRing          = 12,
		PriestStateChange = 13,
		PossessedSwitched = 14,

		_Count
	};

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
	}

	// =========== Emit helpers ===========
	// Lower-level than Hooks: callers that want to emit a custom event
	// (e.g. solver hints, bot decisions) without dispatching a full
	// engine event can use these directly.
	void EmitEvent(DPEventType eType,
	               Zenith_EntityID xA = Zenith_EntityID{},
	               Zenith_EntityID xB = Zenith_EntityID{},
	               int32_t iInt0 = 0,
	               float fFloat0 = 0.0f,
	               const char* szLabel = nullptr);

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
		Zenith_EventHandle m_xPickup         = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteract       = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteractBegin  = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteractEnd    = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xInteractCancel = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xDied           = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xVictory        = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xRunLost        = INVALID_EVENT_HANDLE;
		Zenith_EventHandle m_xBellRing       = INVALID_EVENT_HANDLE;
	};
}
