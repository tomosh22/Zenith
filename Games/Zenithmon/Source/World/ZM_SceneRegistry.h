#pragma once

#include "Zenithmon/Source/Data/ZM_WorldSpec.h"   // the compiled world table -- build indices are READ from it, never mirrored

// ============================================================================
// ZM_SceneRegistry (S8 item 2, R1-1) -- the ONE enumerable inventory of which
// ZM_SCENE_ID has a .zscen on disk, and under which file stem.
//
// PURE. No ECS, no scene system, no g_xEngine, no allocation, no I/O, no
// ZENITH_TOOLS guard. It holds STEMS, never paths: GAME_ASSETS_DIR and
// ZENITH_SCENE_EXT are the caller's business, so this header stays free of
// <string> and of the build-system defines.
//
// ★ WHY IT EXISTS. Project_LoadInitialScene used to be five hand-written
// RegisterSceneBuildIndex calls with no compiled table behind them, so NO boot
// unit could guard "the gate trigger shipped, the registration did not".
// IsWarpDestinationValid consults only the compiled world table and never the
// registry, so an unregistered index is ACCEPTED, and the machine then parks in
// ZM_WARP_TRANSITION_WAITING_FOR_SCENE / _WAITING_FOR_SPAWN, NEITHER OF WHICH
// HAS A TIMEOUT -- a permanent black screen behind an opaque fade with the
// player frozen. Not a crash, not a red test.
//
// ★ ROUTE1 AND THORNACRE ARE IN THIS TABLE BEFORE THEIR SCENES EXIST, ON
// PURPOSE. A registration without a scene is INERT (nothing loads that index
// until a trigger points at it); a scene without a registration is FATAL. Doing
// the harmless half first makes the fatal half unreachable for the slices that
// author those scenes. The accepted cost is two unloadable entries in the tools
// editor toolbar (Zenith_EditorPanel_Toolbar.cpp enumerates the whole registry),
// and it goes away when those slices land the files.
//
// ★ GYM1 IS DELIBERATELY ABSENT. It is out of S8 item 2's scope; adding a row
// must be a conscious act, which the boot unit
// ZM_SceneRegistry/Registry_CoversEveryFileBackedSceneAndDeliberatelyOmitsTheGym
// forces.
//
// EVERY accessor below is TOTAL: no argument value, however degenerate, is UB,
// and none of them calls Zenith_Assert (which debug-breaks in EVERY
// configuration and would END the boot unit run). Note ZM_GetWorldSpec ASSERTS
// out of range, so every accessor here guards before calling it.
// ============================================================================

struct ZM_SceneRegistration
{
	ZM_SCENE_ID	m_eScene;      // the compiled world-table row this file backs
	const char*	m_szFileStem;  // Assets/Scenes/<stem>.zscen -- a STEM: no dir, no dot
};

inline constexpr ZM_SceneRegistration xZM_INVALID_SCENE_REGISTRATION =
	{ ZM_SCENE_NONE, "" };

// The answer ZM_GetSceneRegistrationBuildIndex gives for a row whose scene id is
// out of the world table's range. NAMED because the registration loop has to
// SKIP it: static_cast<int> of this is -1, and RegisterSceneBuildIndex asserts
// "Build index must be non-negative" (Zenith_SceneSystem_Registry.cpp:445).
inline constexpr u_int uZM_SCENE_REGISTRATION_BUILD_INDEX_UNRESOLVED = 0xFFFFFFFFu;

// In ascending ZM_SCENE_ID order. Registration ORDER is behaviourally irrelevant
// (the registry is a dense map keyed by build index), so the order is chosen to
// be the one a reader can check at a glance.
inline constexpr ZM_SceneRegistration axZM_SCENE_REGISTRATIONS[] =
{
	{ ZM_SCENE_FRONTEND,   "FrontEnd"   },
	{ ZM_SCENE_BATTLE,     "Battle"     },
	{ ZM_SCENE_DAWNMERE,   "Dawnmere"   },
	{ ZM_SCENE_THORNACRE,  "Thornacre"  },   // R1-2 authors the file; the row lands now
	{ ZM_SCENE_ROUTE1,     "Route1"     },   // R1-2 authors the file; the row lands now
	{ ZM_SCENE_PLAYERHOME, "PlayerHome" },
	{ ZM_SCENE_PROFLAB,    "ProfLab"    },
};

inline constexpr u_int uZM_SCENE_REGISTRATION_COUNT =
	(u_int)(sizeof(axZM_SCENE_REGISTRATIONS) / sizeof(axZM_SCENE_REGISTRATIONS[0]));

inline u_int ZM_GetSceneRegistrationCount() { return uZM_SCENE_REGISTRATION_COUNT; }

inline const ZM_SceneRegistration& ZM_GetSceneRegistration(u_int uIndex)
{
	return uIndex < uZM_SCENE_REGISTRATION_COUNT
		? axZM_SCENE_REGISTRATIONS[uIndex]
		: xZM_INVALID_SCENE_REGISTRATION;
}

// TOTAL: "" (never nullptr) for a scene with no row, including ZM_SCENE_NONE.
inline const char* ZM_FindSceneFileStem(ZM_SCENE_ID eScene)
{
	for (u_int u = 0u; u < uZM_SCENE_REGISTRATION_COUNT; ++u)
	{
		if (axZM_SCENE_REGISTRATIONS[u].m_eScene == eScene)
		{
			return axZM_SCENE_REGISTRATIONS[u].m_szFileStem;
		}
	}
	return "";
}

inline bool ZM_IsSceneRegisteredForLoad(ZM_SCENE_ID eScene)
{
	return eScene < ZM_SCENE_COUNT && ZM_FindSceneFileStem(eScene)[0] != '\0';
}

// TOTAL: a build index the world table does not resolve is simply not registered.
inline bool ZM_IsBuildIndexRegisteredForLoad(u_int uBuildIndex)
{
	const ZM_SCENE_ID eScene = ZM_FindSceneByBuildIndex(uBuildIndex);
	return eScene != ZM_SCENE_NONE && ZM_IsSceneRegisteredForLoad(eScene);
}

// TOTAL: an out-of-range row answers uZM_SCENE_REGISTRATION_BUILD_INDEX_UNRESOLVED
// rather than reaching ZM_GetWorldSpec, which asserts fatally out of range.
// Provided so the registration loop and the boot units share ONE spelling of
// "this row's build index".
inline u_int ZM_GetSceneRegistrationBuildIndex(const ZM_SceneRegistration& xRow)
{
	return xRow.m_eScene < ZM_SCENE_COUNT
		? ZM_GetWorldSpec(xRow.m_eScene).m_uBuildIndex
		: uZM_SCENE_REGISTRATION_BUILD_INDEX_UNRESOLVED;
}
