#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "DevilsPlayground_Tags.h"

// ============================================================================
// DP_Player — published by B2 (player + camera + input).
// Possession, cooldown, anchor-range gate, Devout channel, held-item table,
// demon-scent table. State lives in the .cpp's anon namespace + on the
// scene-owned DPPlayerController_Behaviour (held items, scent).
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

#ifdef ZENITH_INPUT_SIMULATOR
	// Backward-compatible alias for tests that pre-date the rename.
	inline void ResetForTest() { ResetForNewRun(); }
#endif
}
