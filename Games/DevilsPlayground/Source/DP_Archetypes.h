#pragma once

#include <string>

// ============================================================================
// DP_Archetypes -- runtime accessor for Devil's Playground villager archetypes.
//
// Loads Games/DevilsPlayground/Config/Archetypes.json once at boot and exposes
// each archetype struct by id via Get(id). The full 24-entry table is loaded
// regardless of the "mvp": true flag; consumers filter via IsMvp(id) where
// MVP-only behaviour is required.
//
// Asserts on miss for Get(id). Returns nullptr from FindByIndex when out of
// range. Initialize is idempotent.
// ============================================================================
namespace DP_Archetypes
{
	struct Archetype
	{
		std::string id;
		std::string display_name_key;
		bool        mvp                  = false;
		float       life_timer_s         = 0.0f;
		float       walk_speed_mps       = 0.0f;
		float       jog_speed_mps        = 0.0f;
		float       sprint_speed_mps     = 0.0f;
		float       possession_channel_s = 0.0f;
		float       demon_scent_floor    = 0.0f;
		float       rarity               = 0.0f;
		int         min_spawns           = 0;
		int         max_spawns           = 0;
		float       tint_r               = 1.0f;
		float       tint_g               = 1.0f;
		float       tint_b               = 1.0f;
	};

	void Initialize();
	void Shutdown();

	// Lookup by archetype id (e.g. "Farmhand"). Asserts on miss.
	// Pointer is stable for the lifetime of the cache (until Shutdown).
	const Archetype* Get(const char* szId);

	// Returns total number of archetypes loaded (24 in the ratified config).
	size_t Count();

	// Returns nullptr if uIdx >= Count().
	const Archetype* GetByIndex(size_t uIdx);

	// Convenience: true iff Get(szId)->mvp == true.
	bool IsMvp(const char* szId);
}
