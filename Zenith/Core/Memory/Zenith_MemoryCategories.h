#pragma once

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
	MEMORY_CATEGORY_COUNT
};

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
	"Temp"
};

inline const char* GetMemoryCategoryName(Zenith_MemoryCategory eCategory)
{
	if (eCategory < MEMORY_CATEGORY_COUNT)
	{
		return g_aszMemoryCategoryNames[eCategory];
	}
	return "Unknown";
}
