#pragma once

// Shared types used across multiple DP_* namespaces: math aliases, event
// structs, and enums dispatched via Zenith_EventDispatcher. Lifted out of
// the original PublicInterfaces.h on 2026-05-22 as part of the per-namespace
// header split — each DP_* namespace now owns its own .h/.cpp pair, but the
// event payloads + the Vec3 alias are still cross-cutting.

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "Maths/Zenith_Maths.h"
#include "DevilsPlayground_Tags.h"

#include <cstdint>

// ============================================================================
// Math type aliases used throughout DevilsPlayground game code.
// ============================================================================
using Vec2 = Zenith_Maths::Vector2;
using Vec3 = Zenith_Maths::Vector3;
using Vec4 = Zenith_Maths::Vector4;
using Quat = Zenith_Maths::Quat;

// ============================================================================
// Event struct types.
// Dispatched via Zenith_EventDispatcher::Get().Dispatch<TEvent>(evt).
// ============================================================================
struct DP_OnItemPickedUp
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xItem;
	// 2026-05-21: tag carried in the payload so the tutorialisation
	// system + telemetry consumers don't have to re-resolve via
	// DP_Items::GetItemTag (which can race against item-destroy in
	// special-behaviour paths like BellSoul). Default-init covers
	// existing dispatchers that don't yet pass the tag explicitly.
	DP_ItemTag      m_eTag = DP_ItemTag::None;
};

struct DP_OnInteract
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xTarget;
};

struct DP_OnInteractionBegin
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xTarget;
};

struct DP_OnInteractionEnd
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xTarget;
};

struct DP_OnInteractionCancelled
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xTarget;
};

struct DP_OnVillagerDied
{
	Zenith_EntityID m_xVillager;
};

struct DP_OnVictory
{
	// Phase-5-audit follow-up (2026-05-16): added entity payload so
	// telemetry consumers (visualiser, replay UIs, analytics) can place
	// the Victory marker at a meaningful world position instead of the
	// bounding-box centre fallback. Both fields default to INVALID_ENTITY_ID
	// so existing dispatchers that fire `DP_OnVictory{}` keep compiling
	// (the fields just stay invalid -- only diagnostic value lost). The
	// production dispatcher in DP_Win::NotifyObjectiveCollected (called
	// from DPPentagram_Behaviour::HandleInteract) populates both.
	Zenith_EntityID m_xVillager;   // who placed the 5th objective (the winner)
	Zenith_EntityID m_xPentagram;  // the ritual altar entity
};

// MVP-1.3.1: run-loss cause enum. The dispatcher fires DP_OnRunLost{cause}
// when the player's current possession ends in a way other than the
// player voluntarily switching to another villager. Three causes:
//   * Apprehended -- the priest got within priest.apprehend_range_m of
//     the possessed villager and held the channel for
//     priest.apprehend_channel_s seconds. MVP-1.3.2's Apprehend BT
//     branch dispatches this.
//   * Dawn -- the night timer ran out before the player delivered all
//     5 objectives. MVP-1.3.5 / MVP-4.2.2 wires this.
//   * NoVessels -- every villager in the level has died (life timer)
//     and no fresh body is available for possession. MVP-1.3.5 /
//     MVP-4.2.3 wires this from DPVillager_Behaviour::TickLife.
//
// Subscribers (DPHUDController, GameManager / state machine, future
// "game over" overlay) treat all three as "run is over" and switch
// the player to the run-over screen. The cause field lets the UI tell
// the player WHY they lost (the GDD's three failure copy variants).
enum class DP_RunLostCause : uint8_t
{
	Apprehended,
	Dawn,
	NoVessels
};

struct DP_OnRunLost
{
	DP_RunLostCause m_eCause;
	// Phase-5-audit follow-up (2026-05-16): cause-specific entity
	// context so the telemetry visualiser can place the RunLost marker
	// at a meaningful world position instead of falling back to the
	// world-bounds centre. Both default INVALID; existing dispatchers
	// that fire `DP_OnRunLost{cause}` keep compiling.
	//
	//   Apprehended : m_xVillager = caught villager, m_xOther = priest who caught them
	//   Dawn        : m_xVillager = currently-possessed villager (may be INVALID
	//                                if dawn hit between possessions),
	//                  m_xOther    = INVALID
	//   NoVessels   : m_xVillager = last villager to die, m_xOther = INVALID
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xOther;
};

// MVP-2.2.6: BellSoul ring-on-pickup event. Dispatched by DPItemBase
// when an item with special_behaviour="rings_bell_on_pickup" (only
// BellSoul in MVP) is picked up by the proximity-pickup path.
struct DP_OnBellRing
{
	Zenith_EntityID m_xVillager;       // who picked it up
	Zenith_EntityID m_xBellSoul;       // the BellSoul item handle (now destroyed/held)
	Zenith_Maths::Vector3 m_xPosition; // world-space ring origin
};

// ============================================================================
// Phase-5-audit (2026-05-16): granular gameplay-milestone events. Surface
// the moments the analyzer + visualiser want to inspect rather than
// inferring them from generic DP_OnInteract noise.
// ============================================================================
struct DP_OnPossessionChanged
{
	Zenith_EntityID m_xOldVillager;  // INVALID if no prior possession (first-possess case)
	Zenith_EntityID m_xNewVillager;  // INVALID when un-possessing (death, voluntary release)
};

struct DP_OnDoorOpened
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xDoor;
};

// 2026-05-25: doors are now TWO-WAY (F-press an open door swings it
// closed). Fires on the Open -> Closing transition. Same payload shape
// as DP_OnDoorOpened so the telemetry recorder can mirror the existing
// DoorOpened subscription pattern in DPTelemetry's Hooks.
struct DP_OnDoorClosed
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xDoor;
};

struct DP_OnChestOpened
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xChest;
};

struct DP_OnForgeCrafted
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xForge;
	Zenith_EntityID m_xOutputItem;
};

struct DP_OnObjectivePlaced
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xPentagram;
	int             m_iObjectiveBitIndex;  // 0..4, mirrors DP_Win::GetCollectedObjectivesMask bit
};

// ============================================================================
// Telemetry-v3 events (2026-05-19).
// ============================================================================
struct DP_OnApprehendChannelStart
{
	Zenith_EntityID m_xPriest;
	Zenith_EntityID m_xVictim;
};

struct DP_OnApprehendChannelComplete
{
	Zenith_EntityID m_xPriest;
	Zenith_EntityID m_xVictim;
};

// Stable integer IDs -- never reorder.
enum class DP_ApprehendInterruptReason : int32_t
{
	Unknown          = 0,
	TargetSwitched   = 1,  // possession switched to another villager mid-channel
	TargetLost       = 2,  // priest lost line-of-sight / awareness on the victim
	OutOfRange       = 3,  // victim moved outside apprehend_range_m
	PriestDespawned  = 4,
};

struct DP_OnApprehendChannelInterrupted
{
	Zenith_EntityID            m_xPriest;
	Zenith_EntityID            m_xVictim;
	DP_ApprehendInterruptReason m_eReason;
};

struct DP_OnPauseToggle
{
	bool m_bIsPaused;
};

struct DP_OnPerceptionContactBegin
{
	Zenith_EntityID m_xObserver;     // typically the priest
	Zenith_EntityID m_xTarget;
	uint32_t        m_uStimulusMask; // PERCEPTION_STIMULUS_* bits
	float           m_fAwareness;    // observer's awareness of target at contact
};

struct DP_OnPerceptionContactEnd
{
	Zenith_EntityID m_xObserver;
	Zenith_EntityID m_xTarget;
	uint32_t        m_uStimulusMask;
};

// ============================================================================
// Player-feedback events (2026-05-21).
// ============================================================================
struct DP_OnDoorLockRejected
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xDoor;
	DP_ItemTag      m_eRequiredKey;  // what tag the door wants
};

struct DP_OnItemEvaporated
{
	Zenith_EntityID         m_xItem;        // entity about to be destroyed
	DP_ItemTag              m_eTag;         // BogWater for MVP
	Zenith_Maths::Vector3   m_xPosition;    // world position at evaporation
};

enum class DP_PriestAlertKind : uint8_t
{
	HeardNoise   = 0,  // BB_KEY_HAS_INVESTIGATE_POS rose; priest will Investigate
	SawTarget    = 1,  // BB_KEY_TARGET_WITH_DEVIL rose; priest will Pursue
	WithinRange  = 2,  // priest within apprehend_range_m of target; will Apprehend
};

struct DP_OnPriestAlerted
{
	Zenith_EntityID         m_xPriest;
	DP_PriestAlertKind      m_eKind;
	Zenith_Maths::Vector3   m_xPosition;    // priest world pos at alert (for the "!" billboard anchor)
};

struct DP_OnChildToolRefused
{
	Zenith_EntityID         m_xVillager;
	Zenith_EntityID         m_xItem;
	DP_ItemTag              m_eTag;          // the tool tag the Child refused
	Zenith_Maths::Vector3   m_xPosition;     // villager world pos at refusal
};
