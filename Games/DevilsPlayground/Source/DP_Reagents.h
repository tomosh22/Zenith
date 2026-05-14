#pragma once

#include <string>

// ============================================================================
// DP_Reagents -- runtime accessor for Devil's Playground reagent registry.
//
// Loads Games/DevilsPlayground/Config/Reagents.json once at boot and exposes
// each reagent struct by id via Get(id). The full 14-entry table is loaded
// regardless of the "mvp": true flag; consumers filter via IsMvp(id) where
// MVP-only behaviour is required.
//
// Reagent IDs correspond to DP_ItemTag enum names (Caul, HareTongue,
// BogWater, BurialCoin, BellSoul are the 5 MVP entries) -- DPItemBase
// looks up its reagent properties at OnAwake via
// DP_Reagents::Get(DP_ItemTagToString(m_eTag)).
//
// special_behaviour values (strings in the JSON):
//   "none"                    -- no special behaviour, just a pickup channel
//   "evaporates_after_drop"   -- BogWater: 8s timer + entity destroy on drop
//   "rings_bell_on_pickup"    -- BellSoul: dispatch DP_OnBellRing on pickup
//
// Asserts on miss for Get(id). Returns nullptr from FindByIndex when out
// of range. Initialize is idempotent.
// ============================================================================
namespace DP_Reagents
{
	struct Reagent
	{
		std::string id;
		std::string display_name_key;
		bool        mvp                  = false;
		float       pickup_channel_s     = 0.0f;
		std::string special_behaviour;            // "none" / "evaporates_after_drop" / "rings_bell_on_pickup"
		float       evaporate_duration_s = 0.0f;  // only set for BogWater (and post-MVP reagents)
		float       rarity               = 0.0f;
		float       tint_r               = 1.0f;
		float       tint_g               = 1.0f;
		float       tint_b               = 1.0f;
	};

	void Initialize();
	void Shutdown();

	// Lookup by reagent id (e.g. "BogWater"). Asserts on miss.
	// Pointer is stable for the lifetime of the cache (until Shutdown).
	const Reagent* Get(const char* szId);

	// Non-asserting variant -- returns nullptr if the id isn't in the
	// registry. Used by DPItemBase to silently skip non-reagent tags
	// (Iron / Key / Objective1..5 / etc.).
	const Reagent* TryGet(const char* szId);

	// Returns total number of reagents loaded (14 in the ratified config).
	size_t Count();

	// Returns nullptr if uIdx >= Count().
	const Reagent* GetByIndex(size_t uIdx);

	// Convenience: true iff Get(szId)->mvp == true.
	bool IsMvp(const char* szId);
}
