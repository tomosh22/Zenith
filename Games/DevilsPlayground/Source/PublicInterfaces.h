#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Zenith_Query.h"
#include "Maths/Zenith_Maths.h"
#include "DevilsPlayground_Tags.h"

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
// Subscribers: future HUD overlay flash, Aelfric heightened-awareness
// state. The audible-everywhere effect lives in a separate
// EmitSoundStimulus call with a very large radius (200 m -- bigger
// than the priest's 30 m hearing range, so the priest hears it
// guaranteed regardless of where on the map the pickup happened).
struct DP_OnBellRing
{
	Zenith_EntityID m_xVillager;       // who picked it up
	Zenith_EntityID m_xBellSoul;       // the BellSoul item handle (now destroyed/held)
	Zenith_Maths::Vector3 m_xPosition; // world-space ring origin
};

// ============================================================================
// Phase-5-audit (2026-05-16): granular gameplay-milestone events. Surface
// the moments the analyzer + visualiser want to inspect rather than
// inferring them from generic DP_OnInteract noise. Each dispatched by
// the relevant subsystem at the moment the milestone fires:
//
//   DP_OnPossessionChanged  -- DP_Player::SetPossessedVillager
//   DP_OnDoorOpened         -- DPDoor_Behaviour rising edge of "opened"
//   DP_OnChestOpened        -- DPChest_Behaviour rising edge of "open"
//   DP_OnForgeCrafted       -- DPForge_Behaviour after recipe consumes
//   DP_OnObjectivePlaced    -- DPPentagram_Behaviour per delivered objective
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
// Telemetry-v3 events (2026-05-19). These give the recorder a richer
// timeline of priest + system behaviour than the prior "VillagerDied at
// the end" black box. Subscribed by DPTelemetry::Hooks and forwarded to
// the engine recorder as DPEventType::* values.
// ============================================================================

// Apprehend-channel lifecycle. The priest's Apprehend BT branch
// channels for ~3s before firing DP_OnRunLost{cause=Apprehended}.
// Surfacing the three transitions makes "the channel started but
// was interrupted before it completed" visible in the timeline.
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

// Interrupt reasons. Stable integer IDs -- never reorder.
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

// Pause-state transitions. DPPauseMenuController fires this on Esc-press
// rising/falling edge so the visualiser can render a "paused frames"
// band on the timeline.
struct DP_OnPauseToggle
{
	bool m_bIsPaused;
};

// Perception contact transitions. Emitted by the per-frame DP telemetry
// sampler when an agent's awareness of a target crosses kContactThreshold
// (rising = Begin, falling = End). Stimulus mask records which sense
// triggered the contact -- SIGHT / HEARING / DAMAGE per the engine
// perception system's bit flags.
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
// Player-feedback events (2026-05-21). These telegraph state transitions
// to the player via in-world particles + HUD messages. Distinct from the
// telemetry events above -- those exist for offline analysis, these
// exist so the player can see what just happened.
// ============================================================================

// Player pressed F at a locked door without the right key in hand. The
// door stays closed and the held item is NOT consumed (DPDoor's
// HandleInteract short-circuits before the lerp). The particles system
// + HUD subscribe so the player sees a "rejection" puff and a
// "Locked -- needs <key tag>" line.
struct DP_OnDoorLockRejected
{
	Zenith_EntityID m_xVillager;
	Zenith_EntityID m_xDoor;
	DP_ItemTag      m_eRequiredKey;  // what tag the door wants
};

// An item with special_behaviour="evaporates_after_drop" finished its
// evaporate countdown and is about to be destroyed. The position field
// is captured BEFORE destruction so the particles system can fire a
// steam burst at the now-empty floor location. MVP only triggers this
// for BogWater (Reagents.json sets the special_behaviour); the post-MVP
// reagents may also use it.
struct DP_OnItemEvaporated
{
	Zenith_EntityID         m_xItem;        // entity about to be destroyed
	DP_ItemTag              m_eTag;         // BogWater for MVP
	Zenith_Maths::Vector3   m_xPosition;    // world position at evaporation
};

// Priest's BT acquired a stimulus rising-edge. The priest's BT runs four
// branches (Patrol / Investigate / Pursue / Apprehend); this event fires
// at the moment a "higher-priority" branch first wins over Patrol /
// Idle. Subscribed by the particles system (which fires a "!" alert
// burst above the priest's head) and the HUD (which raises the
// awareness indicator to the appropriate state). Falling edge (priest
// loses target + returns to patrol) is NOT emitted; the particles +
// HUD time out on their own.
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

// (DP_OnItemPickedUp lives at the top of the file -- its tag field
// was extended 2026-05-21 to carry the picked-up tag so the
// tutorialisation system + telemetry don't have to re-resolve.)

// ============================================================================
// DP_Player — published by B2 (player + camera + input).
// ============================================================================
namespace DP_Player
{
	Zenith_EntityID GetPossessedVillager();
	void SetPossessedVillager(Zenith_EntityID xId);

	// MVP-1.5 + 1.8: voluntary-switch possession path. Player-driven
	// possession (click-to-possess, future switch UI) goes through this
	// entry point so the cooldown + range gates fire. System paths --
	// villager death, priest apprehend -- keep calling SetPossessedVillager
	// directly so they don't trigger gates the player can't see coming.
	//
	// Returns true if the possession actually changed; false when the
	// call was rejected by the cooldown or range gates. Resolving to the
	// SAME villager that's already possessed returns true and is a no-op
	// (idempotent re-clicks don't waste the cooldown window or trip the
	// range gate).
	//
	// Gates:
	//   * Cooldown (MVP-1.5): refused if cooldown > 0; on success,
	//     cooldown = priest_tuning
	//     "possession.cooldown_after_voluntary_switch_s" (default 1.5 s).
	//   * Range (MVP-1.8): refused if the new villager's world position
	//     is more than "possession.range_from_anchor_m" (default 15 m)
	//     from the anchor. Anchor is set by SetPossessedVillager and
	//     TryVoluntaryPossessSwitch on success; cleared by
	//     SetPossessedVillager(INVALID_ENTITY_ID).
	//   * Resetting cooldown / range is automatic on death / apprehend
	//     (those paths use SetPossessedVillager and don't trigger the
	//     gates; "cooldown_after_burnout_s = 0" in tuning is the canon).
	bool TryVoluntaryPossessSwitch(Zenith_EntityID xId);

	// Per-frame cooldown decrement. DPPlayerController_Behaviour::OnUpdate
	// calls this once per frame so the cooldown drains at wall-clock rate
	// regardless of how many possession attempts are made. Safe to call
	// when cooldown is already zero (clamps).
	void TickPossessionCooldown(float fDt);

	// Remaining cooldown in seconds. Zero when no cooldown is active.
	// HUD / debug overlays can render this to show the player when their
	// next switch will be available.
	float GetPossessionCooldownRemaining();

	// MVP-1.6: demon-scent table. Successful possessions accumulate scent
	// on the possessed villager; the value decays over time and saturates
	// at `possession.demon_scent_max`.
	//
	// The accumulation happens INSIDE TryVoluntaryPossessSwitch (the
	// player path) -- death and apprehend never bump scent. This matches
	// Tuning.json:
	//   "demon_scent_per_possession ... Only applies on SUCCESSFUL
	//    possession. Failed attempts (cooldown-blocked, out-of-range,
	//    channel-interrupted) produce no scent."
	//
	// In MVP only the data-path is wired (scent table + decay + write to
	// the priest's BB_KEY_HIGH_SCENT_TARGET). No behaviour CONSUMES the
	// scent yet (hounds and variants are post-MVP).
	float GetDemonScent(Zenith_EntityID xVillager);
	void  TickDemonScent(float fDt);
	void  WriteHighestScentToBlackboard();

	// MVP-2.1.1: Devout possession channel.
	//
	// Devout-archetype villagers don't possess instantly. When the
	// player TryVoluntaryPossessSwitch's onto a Devout, the call
	// returns true but possession is NOT committed yet -- instead a
	// channel timer (`possession.channel_devout_s`, 0.8 s default)
	// starts, during which the player's possession stays on the
	// SOURCE villager. TickChannel (called per-frame by
	// DPPlayerController) decrements the timer; when it reaches 0
	// the possession commits onto the Devout.
	//
	// MVP-2.1.2 interrupt: each TickChannel iteration also scans for
	// any priest within `possession.channel_interrupt_distance_m`
	// (6 m default) of the channel TARGET. If found, the channel is
	// cancelled (InterruptChannel) -- the demon's faith snaps off
	// the partial possession. The cooldown is NOT consumed on
	// interrupt, so the player can immediately retry onto another
	// villager.
	//
	// Non-Devout villagers use `channel_default_s` (0 s) -- the
	// switch commits immediately as before. The Devout-vs-default
	// branch is internal to TryVoluntaryPossessSwitch.
	//
	// Idempotency: TryVoluntaryPossessSwitch refuses while a channel
	// is in progress (returns false). Wait for completion or
	// InterruptChannel().
	bool            IsChanneling();
	Zenith_EntityID GetChannelTarget();
	float           GetChannelRemaining();
	void            TickChannel(float fDt);
	void            InterruptChannel();

	DP_ItemTag GetHeldItemTag(Zenith_EntityID xVillager);
	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager);
	void SetHeldItem(Zenith_EntityID xVillager, Zenith_EntityID xItem);
	void RemoveHeldItem(Zenith_EntityID xVillager);

	// Drop every per-run DP_Player state owner: possessed villager,
	// held-item table, possession cooldown, anchor, scent table,
	// channel state. Used by:
	//   - DPPauseMenuController::HandleRestart / HandleQuit -- the
	//     player's permanent run-over -> new-run transition.
	//   - The harness between batched automated tests -- next test
	//     starts from a clean slate.
	// "ForNewRun" reflects the production semantic. The historical
	// name was ResetForTest (test-only intent); when MVP-2.5.5 ran
	// the pause-menu restart through it, the name became misleading.
	void ResetForNewRun();

#ifdef ZENITH_INPUT_SIMULATOR
	// Backward-compatible alias for tests that pre-date the rename.
	// New call sites should prefer ResetForNewRun.
	inline void ResetForTest() { ResetForNewRun(); }
#endif
}

// ============================================================================
// DP_Items — published by B3.
// ============================================================================
namespace DP_Items
{
	DP_ItemTag GetItemTag(Zenith_EntityID xItem);
	Vec3 GetItemWorldPos(Zenith_EntityID xItem);

	bool TryConsumeKeyForUnlock(Zenith_EntityID xVillager, DP_ItemTag eRequiredKey);

	Zenith_EntityID FindItemByTag(DP_ItemTag eTag);

	// Internal — called by DPItemBase_Behaviour::OnAwake/OnDestroy to maintain
	// the item-tag side-table that backs GetItemTag / FindItemByTag.
	void Internal_RegisterItemTag(Zenith_EntityID xItem, DP_ItemTag eTag);
	void Internal_UnregisterItemTag(Zenith_EntityID xItem);
}

// ============================================================================
// DP_Interactables — published by B3.
// ============================================================================
namespace DP_Interactables
{
	enum class Kind : uint32_t
	{
		Door,
		DoubleDoor,
		Chest,
		Forge,
		Pentagram,
		NoiseMachine
	};

	void MarkAsInteractable(Zenith_EntityID xId, Kind eKind, void* pUserData);
}

// ============================================================================
// DP_AI — published by B4.
// ============================================================================
class Zenith_NavMesh;

namespace DP_AI
{
	constexpr const char* PRIEST_BEHAVIOUR_TYPE = "Priest_Behaviour";

	constexpr const char* BB_KEY_SELF_ACTOR          = "SelfActor";
	constexpr const char* BB_KEY_TARGET_WITH_DEVIL   = "TargetWithDevil";
	constexpr const char* BB_KEY_SUSPICION_RADIUS    = "SuspicionRadius";
	constexpr const char* BB_KEY_INVESTIGATE_POS     = "InvestigatePos";
	constexpr const char* BB_KEY_HAS_INVESTIGATE_POS = "HasInvestigatePos";
	constexpr const char* BB_KEY_PATROL_TARGET       = "PatrolTarget";
	// MVP-1.6: priest reads the highest-scent villager out of this slot.
	// Set by DP_Player::WriteHighestScentToBlackboard, called by
	// DPPlayerController_Behaviour::OnUpdate after the scent table
	// has been ticked / updated. Value is INVALID_ENTITY_ID when no
	// villager carries any scent. No production behaviour consumes
	// this key in MVP (the hound archetype is post-MVP); the test
	// suite is the only reader for now.
	constexpr const char* BB_KEY_HIGH_SCENT_TARGET   = "HighScentTarget";

	void EmitNoise(Vec3 xPos, float fLoudness, float fRadius, Zenith_EntityID xSource);

	// MVP-2.2.6+ map-wide bell broadcast. The perception system clamps each
	// agent's hearing at min(emit_radius, agent_max_range), so a 200 m
	// EmitNoise emit still cuts off at the priest's 35 m hearing_range_m --
	// breaking the GDD's "BellSoul audible from the entire map" promise.
	// This helper bypasses perception and writes directly to every
	// Priest_Behaviour agent's blackboard:
	//   BB_KEY_INVESTIGATE_POS      <- xPos
	//   BB_KEY_HAS_INVESTIGATE_POS  <- true
	// The investigate-pos slot is then consumed by the existing
	// DP_BTAction_ClearInvestigatePos sequence, so the rest of the BT
	// flow is unchanged.
	void NotifyAllPriestsOfInvestigatePos(Vec3 xPos);

	// Lazily-built level navmesh. First call generates a synthetic flat
	// 200m × 200m polygon centred on the priest start (the source UE map
	// uses rough authoring positions that aren't on a single navmesh
	// island; a flat polygon is correct for skeleton-grade pursuit). Wave-4
	// polish replaces this with a generator pass over real collider geometry
	// or a pre-baked .znavmesh asset. Idempotent — repeated calls return
	// the same pointer.
	const Zenith_NavMesh* GetOrBuildLevelNavMesh();

	// Reset state on scene unload — the next GetOrBuildLevelNavMesh rebuilds.
	void ResetLevelNavMesh();
}

// ============================================================================
// DP_Fog — published by B6. Clear-and-rebuild strategy each frame.
// ============================================================================
namespace DP_Fog
{
	void RegisterFogHole(Zenith_EntityID xId, float fRadius);
	void UnregisterFogHole(Zenith_EntityID xId);
	void ClearAllFogHoles();
	uint32_t GetFogHoleCount();

	// Render-side accessor — populates pxOutHoles (xyz=worldPos, w=radius)
	// with up to uMaxHoles entries. Returns the number actually written.
	// Holes whose entity has been destroyed (or has no transform) are
	// skipped silently; their slots simply aren't emitted, so the caller's
	// w==0 sentinel can be used to mark "unused" tail entries.
	uint32_t GatherFogHolePositions(Vec4* pxOutHoles, uint32_t uMaxHoles);

	// ========================================================================
	// MVP-2.4.5: Memory fog. Tiles the player has visited remain partially
	// visible after the villager moves away, with a state-based dimming
	// model.
	//
	// Per Tuning.json `_comment_memory_states`:
	//   NEVER_SEEN       : tile has no memory entry; full grey.
	//   VISITED_VISIBLE  : age <= possession.memory_visible_s (10 s default).
	//   VISITED_DIM      : memory_visible_s < age <= memory_dim_s (10..30 s).
	//   VISITED_HIDDEN   : age > memory_dim_s; visually grey but distinct
	//                      from NEVER_SEEN (the shader can treat them
	//                      identically, but tests + future "you've been
	//                      here before" UI flourishes distinguish them).
	//
	// Memory entries are dedup'd by 1 m grid cell. A new reveal at an
	// existing cell refreshes the cell's age to 0; a new reveal at a
	// fresh cell appends a new entry. TickMemoryFog increments all ages
	// each frame. Entries past memory_dim_s stay in VISITED_HIDDEN
	// (no implicit pruning for MVP -- production polish post-MVP).
	// ========================================================================
	enum class MemoryTileState : uint8_t
	{
		NeverSeen = 0,
		VisitedVisible,
		VisitedDim,
		VisitedHidden
	};

	// Record a reveal at a world position. Snaps to the 1 m grid; if the
	// cell already has an entry, its age resets to 0. DPFogPass calls
	// this once per villager / light fog-hole each frame (the same loop
	// that builds RegisterFogHole, so memory follows visibility).
	void RecordMemoryReveal(Vec3 xPosition);

	// Per-frame age tick. DPPlayerController drives this once per frame.
	void TickMemoryFog(float fDt);

	// Query the current memory state at a world position. Snaps to the
	// 1 m grid. Returns NeverSeen if no entry exists at that cell.
	MemoryTileState GetMemoryStateAt(Vec3 xPosition);

	// Test introspection: how many memory entries are in the table now.
	uint32_t GetMemoryRevealCount();

	// Reset memory state. Wired to the between-tests reset hook.
	void ClearAllMemoryReveals();
}

// ============================================================================
// DP_Win — published by B3 (Pentagram).
// ============================================================================
namespace DP_Win
{
	uint32_t GetCollectedObjectivesMask();
	bool HasWon();
	// Adds the matching objective bit to the collected mask + (if this is
	// the 5th objective) dispatches DP_OnVictory.
	//
	// xVillager / xPentagram are forwarded into the DP_OnVictory payload so
	// the visualiser can locate the victory in world space. They're optional
	// (default INVALID_ENTITY_ID) because legacy tests call this directly
	// without scene context -- in those cases the Victory marker still fires,
	// just without entity provenance. The production call site
	// (DPPentagram_Behaviour::HandleInteract) always supplies both.
	void NotifyObjectiveCollected(DP_ItemTag eObjective,
	                              Zenith_EntityID xVillager  = Zenith_EntityID{},
	                              Zenith_EntityID xPentagram = Zenith_EntityID{});
	void Reset();
}

// ============================================================================
// DP_Night (MVP-1.3.5 Dawn half) -- night-timer countdown. When the
// timer expires, dispatches DP_OnRunLost{Dawn} EXACTLY ONCE.
//
// Design:
//   * StartNight(durationSeconds) seeds the timer. Idempotent re-arm:
//     calling Start while a night is already active resets the timer
//     to the new duration AND clears the dawn-dispatched flag (a
//     "new run begins" semantic).
//   * TickNight(fDt) is driven by DPPlayerController::OnUpdate at
//     game-frame rate. It decrements only while the timer is active;
//     it does NOT auto-start (production code calls StartNight on
//     scene entry; tests drive it directly).
//   * Dispatch happens on the frame the timer crosses zero. The
//     m_bDawnDispatched flag prevents repeat dispatch on subsequent
//     ticks at <= 0.
//
// Why DP_Night and not part of DP_AI / DP_Player: the night timer
// is a global run-state concept (like DP_Win), not a per-agent
// behaviour. Keeping it in its own namespace makes the
// reset/test-cleanup story symmetric with the other run-state
// systems.
// ============================================================================
namespace DP_Night
{
	// Begin a night countdown. duration_s typically reads from
	// possession.night_duration_s (~30 s default per GDD). Calling
	// while already active resets to the new duration and re-arms
	// the dispatch flag (think of it as "start a fresh run").
	void StartNight(float fDurationSeconds);

	// Per-frame tick. Decrements m_fRemaining while active; on the
	// frame it crosses 0, dispatches DP_OnRunLost{Dawn} exactly once
	// and stays active (remains at 0) until either StartNight() or
	// Reset() is called. Called from DPPlayerController::OnUpdate.
	void TickNight(float fDt);

	// Seconds remaining; 0 once the timer has expired; equal to the
	// last StartNight argument before any ticks.
	float GetNightTimeRemaining();

	// True iff StartNight was called and Reset wasn't called since.
	// Stays true after dawn (timer at 0).
	bool IsNightActive();

	// True iff the dawn-dispatch fired this run.
	bool HasDawnReached();

	// Between-tests cleanup hook. Called from DevilsPlayground.cpp's
	// between-tests reset.
	void Reset();
}

// ============================================================================
// DP_Query — script-iteration helpers. Scripts live INSIDE
// Zenith_ScriptComponent, so we cannot Query<T> them directly.
// ============================================================================
namespace DP_Query
{
	// Iterate every entity in the active scene that carries a script of type T.
	// Fn signature: void(Zenith_EntityID, T&)
	template<typename T, typename Fn>
	void ForEachScriptInActiveScene(Fn&& fn)
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) return;
		pxScene->Query<Zenith_ScriptComponent>().ForEach(
			[&fn](Zenith_EntityID xId, Zenith_ScriptComponent& xScript)
			{
				T* pxT = xScript.GetScript<T>();
				if (pxT != nullptr)
				{
					fn(xId, *pxT);
				}
			});
	}

	// Iterate every entity in ALL currently-loaded scenes that carries a script of type T.
	// Fn signature: void(Zenith_EntityID, T&)
	template<typename T, typename Fn>
	void ForEachScriptInLoadedScenes(Fn&& fn)
	{
		const uint32_t uSlotCount = Zenith_SceneManager::GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<Zenith_ScriptComponent>().ForEach(
				[&fn](Zenith_EntityID xId, Zenith_ScriptComponent& xScript)
				{
					T* pxT = xScript.GetScript<T>();
					if (pxT != nullptr)
					{
						fn(xId, *pxT);
					}
				});
		}
	}
}
