#pragma once

// ============================================================================
// ZM_Tests_ComponentTypeNames -- the ONE inventory of serialized component
// TYPE names shared by every boot-unit "needle swallow" battery that needs to
// prove a candidate entity name is not a substring of one.
//
// Landed by ZM-57 as the durable fix Docs/Status.md had recorded as owed since
// R1-1: Tests/ZM_Tests_Route1Placement.cpp and
// Tests/ZM_Tests_ThornacrePlacement.cpp used to each transcribe the SAME two
// tables under different identifiers (aszROUTE1_ENGINE_TYPE_NAMES /
// s_aszEngineComponentTypeNames and their game counterparts), and nothing
// cross-checked the copies -- a component registered later could update one
// and silently weaken the other file's needle-swallow battery, with both
// files green and no diff to read. There is now exactly one copy of each
// table; both suites include this header and alias the arrays under their
// own existing local names, so the loops that walk them are unchanged.
//
// ★ WHY THESE ARE A DELIBERATE COPY, NOT A LIVE READ. A committed .zscen
// carries entity names, component TYPE names, spawn tags and asset paths in
// one byte range, and a committed-bytes needle search (see
// Tests/ZM_Tests_CommittedSceneBytes.cpp) matches BARE NAME BYTES with no
// NUL -- an entity name that is a substring of a serialized type name is
// SWALLOWED, and the count silently includes components it should not. The
// tables below are the CLAIM the placement suites check candidate entity
// names against, before any scene exists for a committed-bytes needle to run
// over. They are transcribed from Zenith_ComponentMeta_Registration.cpp and
// Games/Zenithmon/Zenithmon.cpp on purpose: reading the LIVE component-meta
// registry instead would need g_xEngine, and every unit that reaches these
// tables is a PURE boot unit -- no scene, no entity, no g_xEngine, no file
// I/O -- that runs before the engine has one.
//
// ★ THE ANTI-VACUITY COUNT ASSERTIONS STAY IN EACH CONSUMING FILE, ON
// PURPOSE, RATHER THAN MOVING HERE. A shared table means an edit here can no
// longer diverge the two suites FROM EACH OTHER, but it can still diverge
// from the REAL registries it transcribes -- a component registered and
// never added below would leave both suites green while their needle-swallow
// batteries silently check fewer types than actually serialize.
// Tests/ZM_Tests_Route1Placement.cpp and Tests/ZM_Tests_ThornacrePlacement.cpp
// each still pin their own ZENITH_ASSERT_EQ(..., 15u, ...) /
// ZENITH_ASSERT_EQ(..., 17u, ...) against the counts below, so an edit here
// that is not reconciled against the real registries reds BOTH files rather
// than neither.
// ============================================================================

// Engine set, 17 rows: the 16 registered directly in
// Zenith_ComponentMeta_Registration.cpp's Zenith_RegisterEngineComponents,
// plus "AIAgent", which that function reaches only through the
// Zenith_AI_RegisterComponents() forwarder -- Zenith_AIAgentComponent.cpp
// defines the forwarder and registers "AIAgent" at order 90 inside it, so
// reading Zenith_ComponentMeta_Registration.cpp alone is easy to miscount by
// one.
inline constexpr const char* const aszZM_ENGINE_COMPONENT_TYPE_NAMES[] =
{
	"Transform", "Model", "Tween", "Animator", "Camera", "Light", "Sun",
	"Atmosphere", "Terrain", "Collider", "Graph", "UI", "InstancedMesh",
	"ParticleEmitter", "Attachment", "NavMesh", "AIAgent",
};

inline constexpr u_int uZM_ENGINE_COMPONENT_TYPE_COUNT =
	(u_int)(sizeof(aszZM_ENGINE_COMPONENT_TYPE_NAMES)
		/ sizeof(aszZM_ENGINE_COMPONENT_TYPE_NAMES[0]));

// Game set, 15 rows: Games/Zenithmon/Zenithmon.cpp's ZENITH_REGISTER_COMPONENT
// block.
inline constexpr const char* const aszZM_GAME_COMPONENT_TYPE_NAMES[] =
{
	"ZM_Game", "ZM_TerrainGrass", "ZM_PlayerController", "ZM_FollowCamera",
	"ZM_GameStateManager", "ZM_SpawnPoint", "ZM_WarpTrigger", "ZM_GreyboxVisual",
	"ZM_BattleArena", "ZM_TallGrassSystem", "ZM_BattleTransition",
	"ZM_BattleDirector", "ZM_UI_MenuStack", "ZM_Interactable",
	"ZM_TouchLayoutController",
};

inline constexpr u_int uZM_GAME_COMPONENT_TYPE_COUNT =
	(u_int)(sizeof(aszZM_GAME_COMPONENT_TYPE_NAMES)
		/ sizeof(aszZM_GAME_COMPONENT_TYPE_NAMES[0]));
