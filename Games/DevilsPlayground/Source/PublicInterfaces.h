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
	uint32_t m_uPlaceholder = 0;
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
};

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

#ifdef ZENITH_INPUT_SIMULATOR
	// MVP-1.9: opt-in test-only "omniscient fallback" toggle. Pre-1.9,
	// `Priest_Behaviour::BridgePerceptionToBlackboard` unconditionally
	// dropped back to `DP_Player::GetPossessedVillager` if no perceived
	// target qualified, making the priest effectively omniscient. That
	// behaviour stays in TEST builds by default (true) so existing
	// pursuit/apprehend tests don't have to position priests with
	// line-of-sight setup; new MVP-1.9 sight tests call
	// `SetTestOmniscientFallback(false)` in Setup to disable it and
	// verify the production-shaped sight-driven detection path.
	//
	// Production builds (no ZENITH_INPUT_SIMULATOR) don't compile the
	// fallback at all -- the priest is sight-driven only, matching
	// the GDD's intent.
	void SetTestOmniscientFallback(bool bEnabled);
	bool IsTestOmniscientFallbackEnabled();
#endif

	DP_ItemTag GetHeldItemTag(Zenith_EntityID xVillager);
	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager);
	void SetHeldItem(Zenith_EntityID xVillager, Zenith_EntityID xItem);
	void RemoveHeldItem(Zenith_EntityID xVillager);

	// Drop possessed-villager and held-item state. Used by the harness
	// between batched automated tests; not part of game runtime. Also
	// resets the possession cooldown so tests start with a clean slate.
	void ResetForTest();
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
}

// ============================================================================
// DP_Win — published by B3 (Pentagram).
// ============================================================================
namespace DP_Win
{
	uint32_t GetCollectedObjectivesMask();
	bool HasWon();
	void NotifyObjectiveCollected(DP_ItemTag eObjective);
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
