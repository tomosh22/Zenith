#pragma once

#include "Collections/Zenith_Vector.h"

enum Zenith_ProfileIndex
{
	ZENITH_PROFILE_INDEX__TOTAL_FRAME,

	ZENITH_PROFILE_INDEX__ANIMATION,
	ZENITH_PROFILE_INDEX__SCENE_UPDATE,
	ZENITH_PROFILE_INDEX__PHYSICS,

	ZENITH_PROFILE_INDEX__FLUX_SHADOWS,
	ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING,
	ZENITH_PROFILE_INDEX__FLUX_SKYBOX,
	ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES,
	ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES,
	ZENITH_PROFILE_INDEX__FLUX_TERRAIN,
	ZENITH_PROFILE_INDEX__FLUX_WATER,
	ZENITH_PROFILE_INDEX__FLUX_FOG,
	ZENITH_PROFILE_INDEX__FLUX_SDFS,
	ZENITH_PROFILE_INDEX__FLUX_PFX,
	ZENITH_PROFILE_INDEX__FLUX_TEXT,
	ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME,

	ZENITH_PROFILE_INDEX__VISIBILITY_CHECK,

	ZENITH_PROFILE_INDEX__COUNT,
};

static const char* g_aszProfileNames[]
{
	"Total Frame",
	"Animation",
	"Scene Update",
	"Physics",
	"Flux Shadows",
	"Flux Deferred Shading",
	"Flux Skybox",
	"Flux Static Meshes",
	"Flux Animated Meshes",
	"Flux Terrain",
	"Flux Water",
	"Flux Fog",
	"Flux SDFs",
	"Flux PFX",
	"Flux Text",
	"Flux Swapchain Begin Frame",
	"Visibility Check",
};
static_assert(COUNT_OF(g_aszProfileNames) == ZENITH_PROFILE_INDEX__COUNT, "g_aszProfileNames mismatch");

#define ZENITH_PROFILING_FUNCTION_WRAPPER(x, eProfile, ...) \
Zenith_Profiling::BeginProfile(eProfile); \
x(__VA_ARGS__); \
Zenith_Profiling::EndProfile(eProfile);

class Zenith_Profiling
{
	friend class Zenith_UnitTests;
public:
	struct Event
	{
		Event(const std::chrono::time_point<std::chrono::high_resolution_clock>& xBegin, const std::chrono::time_point<std::chrono::high_resolution_clock>& xEnd, const Zenith_ProfileIndex eIndex, const u_int uDepth)
			: m_xBegin(xBegin)
			, m_xEnd(xEnd)
			, m_eIndex(eIndex)
			, m_uDepth(uDepth)
		{
		}
		std::chrono::time_point<std::chrono::high_resolution_clock> m_xBegin;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_xEnd;
		Zenith_ProfileIndex m_eIndex;
		u_int m_uDepth;
	};

	static void Initialise();

	static void RegisterThread();

	static void BeginFrame();
	static void EndFrame();
	#ifdef ZENITH_TOOLS
	static void RenderToImGui();
	#endif

	static void BeginProfile(const Zenith_ProfileIndex eIndex);
	static void EndProfile(const Zenith_ProfileIndex eIndex);

	static const Zenith_ProfileIndex GetCurrentIndex();

	static const std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>>& GetEvents();
};