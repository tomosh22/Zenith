#pragma once

// Memory categories are BUDGETABLE BUCKETS, deliberately coarser than the 27-entry
// Zenith_LogCategory set (rendering sub-passes etc. are not meaningful memory
// budgets and would bloat every MEMORY_CATEGORY_COUNT-sized array). Attribution is
// via the thread-local category scope stack (ZENITH_MEMORY_SCOPE) — see
// Zenith_MemoryManagement.h.
//
// Ordering is additive (GENERAL = 0 fixed); append new buckets before COUNT so the
// per-category stat/atomic arrays keep their indices.
enum Zenith_MemoryCategory : u_int8
{
	MEMORY_CATEGORY_GENERAL = 0,
	MEMORY_CATEGORY_ENGINE,
	MEMORY_CATEGORY_RENDERER,
	MEMORY_CATEGORY_PHYSICS,
	MEMORY_CATEGORY_SCENE,
	MEMORY_CATEGORY_ASSET,
	MEMORY_CATEGORY_ANIMATION,
	MEMORY_CATEGORY_AI,
	MEMORY_CATEGORY_UI,
	MEMORY_CATEGORY_AUDIO,
	MEMORY_CATEGORY_SCRIPTING,
	MEMORY_CATEGORY_TOOLS,
	MEMORY_CATEGORY_TEMP,
	MEMORY_CATEGORY_ECS,          // entity/component pools (was lumped into SCENE/ENGINE)
	MEMORY_CATEGORY_TERRAIN,      // terrain streaming: large, bursty, budgetable
	MEMORY_CATEGORY_NETWORK,      // forward-looking; zero cost if unused
	MEMORY_CATEGORY_GPU_STAGING,  // CPU-side upload/staging buffers feeding VMA

	MEMORY_CATEGORY_COUNT
};

// Sentinel passed by the global operator new/delete overloads meaning "resolve the
// current ZENITH_MEMORY_SCOPE inside the allocator, AFTER the init-flag check".
// Deliberately outside [0, MEMORY_CATEGORY_COUNT) so it is never used as an array
// index — the allocator replaces it with GetCurrentCategory() (or GENERAL pre-init)
// before any per-category bookkeeping. Must fit in the u_int8 underlying type.
static constexpr Zenith_MemoryCategory MEMORY_CATEGORY_FROM_SCOPE = static_cast<Zenith_MemoryCategory>(0xFF);

inline constexpr const char* g_aszMemoryCategoryNames[MEMORY_CATEGORY_COUNT] = {
	"General",
	"Engine",
	"Renderer",
	"Physics",
	"Scene",
	"Asset",
	"Animation",
	"AI",
	"UI",
	"Audio",
	"Scripting",
	"Tools",
	"Temp",
	"ECS",
	"Terrain",
	"Network",
	"GPUStaging"
};

inline const char* GetMemoryCategoryName(Zenith_MemoryCategory eCategory)
{
	if (eCategory < MEMORY_CATEGORY_COUNT)
	{
		return g_aszMemoryCategoryNames[eCategory];
	}
	return "Unknown";
}
