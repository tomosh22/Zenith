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

	// MVP-1.5: voluntary-switch possession path. Player-driven possession
	// (click-to-possess, future switch UI) goes through this entry point
	// so the cooldown gate fires. System paths -- villager death, priest
	// apprehend -- keep calling SetPossessedVillager directly so they
	// don't trigger a cooldown the player can't see coming.
	//
	// Returns true if the possession actually changed; false when the
	// call was rejected by the cooldown gate. Resolving to the SAME
	// villager that's already possessed returns true and is a no-op
	// (idempotent re-clicks don't waste the cooldown window).
	//
	// Cooldown rules:
	//   * Switching to a different valid villager (incl. INVALID -> X
	//     while a cooldown is already burning down from a prior switch):
	//     refused if cooldown > 0; on success, cooldown = priest_tuning
	//     "possession.cooldown_after_voluntary_switch_s" (default 1.5 s).
	//   * Voluntarily releasing to INVALID while currently possessing
	//     a valid villager: cooldown = same as above.
	//   * Resetting cooldown is automatic on death / apprehend (those
	//     paths use SetPossessedVillager and don't touch the cooldown
	//     timer; "cooldown_after_burnout_s = 0" in tuning is the canon).
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
