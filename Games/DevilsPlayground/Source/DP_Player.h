#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "DevilsPlayground_Tags.h"

// ============================================================================
// DP_Player — published by B2 (player + camera + input).
// Possession, cooldown, anchor-range gate, Devout channel, held-item table,
// demon-scent table. State lives in the .cpp's anon namespace + on the
// scene-owned DPPlayerController_Component (held items, scent).
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
	bool TryVoluntaryPossessSwitch(Zenith_EntityID xId);

	// Per-frame cooldown decrement. Called once per frame.
	void TickPossessionCooldown(float fDt);
	float GetPossessionCooldownRemaining();

	// MVP-1.6: demon-scent table.
	float GetDemonScent(Zenith_EntityID xVillager);
	void  TickDemonScent(float fDt);
	void  WriteHighestScentToBlackboard();

	// MVP-2.1.1: Devout possession channel.
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
	// channel state.
	void ResetForNewRun();

	// ========================================================================
	// Cross-component villager forwarders. These mediate other components'
	// access to DPVillager_Component state so the caller's header doesn't have
	// to include the villager header (cross-component rule). Each resolves the
	// villager component on the entity and reads the requested field.
	// ========================================================================

	// True if the candidate resolves to a possessed DPVillager_Component.
	// Used by Priest_Component's perception->BB bridge.
	bool IsPossessedVillager(Zenith_EntityID xCandidate);

	// True if the candidate is a villager whose archetype id == "Beggar".
	// Used by Priest_Component (Beggar invisible to Aelfric) + the player
	// controller's BeggarStealthAura.
	bool IsBeggarVillager(Zenith_EntityID xCandidate);

	// True if the villager is a "Child" archetype AND the tag is a tool tag.
	// Used by DPItemBase_Component's child-tool-refusal path.
	bool IsChildVillagerWithToolTag(Zenith_EntityID xVillager, DP_ItemTag eTag);

	// Possessed-villager life accessors for the HUD. Return 0.0f when the
	// villager can't be resolved.
	float GetVillagerRemainingLife(Zenith_EntityID xVillager);
	float GetVillagerMaxLife(Zenith_EntityID xVillager);

	// Possessed-villager archetype id for the HUD. Returns "" when the
	// villager can't be resolved. The returned pointer is owned by the
	// villager (valid while it exists -- fine for same-frame HUD use).
	const char* GetVillagerArchetypeId(Zenith_EntityID xVillager);

	// Possessed-villager movement-state flags for the HUD MovementMode
	// readout. Both default false when the villager can't be resolved.
	bool IsVillagerSprintingNow(Zenith_EntityID xVillager);
	bool IsVillagerWalkQuietNow(Zenith_EntityID xVillager);

	// Count villagers in the active scene. Writes total villager count to
	// iOutTotal and the count with RemainingLife > 0 to iOutAlive. Both are
	// initialised to 0 before the scan. Used by the HUD VillagersAlive readout.
	void CountVillagers(int& iOutAlive, int& iOutTotal);

	// Enumerate every villager EntityID in the active scene via a plain
	// function-pointer callback (std::function is forbidden engine-wide). The
	// callback receives the villager id + the opaque pUserData. Lets a caller
	// iterate villagers WITHOUT naming DPVillager_Component at its call site
	// (the type filter lives in the .cpp), so the caller's header doesn't need
	// the villager header. Used by DPPlayerController's click-to-possess pick.
	void ForEachVillagerInActiveScene(void (*pfnCallback)(Zenith_EntityID, void*),
	                                  void* pUserData);

#ifdef ZENITH_INPUT_SIMULATOR
	// Backward-compatible alias for tests that pre-date the rename.
	inline void ResetForTest() { ResetForNewRun(); }
#endif
}
