#pragma once

#include "Maths/Zenith_Maths.h"                      // Vector3

#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_SCENE_ID / ZM_SceneConnection
#include "Zenithmon/Source/World/ZM_HumanBody.h"     // fZM_HUMAN_BODY_HALF_HEIGHT

#include <cmath>                                     // cos/sin -- the settled camera only

// ============================================================================
// ZM_ThornacrePlacement (S8 item 2, R1-1) -- the compiled anchors Thornacre is
// authored from, and the ONE place its entity names are spelled.
//
// ★★ THORNACRE IS A TRAVERSAL STUB IN THIS MILESTONE, BY RULING (ZM-D-196). It
// gets terrain, ONE arrival marker, a player, a camera and a return trigger --
// and NO GYM DOOR. The compiled world table already carries the
// Thornacre -> Gym1 ("Door") edge; that edge is deliberately UNBACKED here.
// Nothing in this file may name a gym entity, and a later slice lands a byte
// needle asserting the committed Thornacre.zscen contains no gym door at all.
// DO NOT "FINISH" THE TOWN IN THIS HEADER. The five-name inventory below and
// uZM_THORNACRE_PLACEMENT_ENTITY_COUNT are the tripwire: a sixth name has to
// move a literal, which reds a boot unit and forces a conscious decision
// instead of quietly authoring a door into a room nobody built.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless Null_* build where Project_RegisterEditorAutomationSteps is
// compiled out entirely. HEADER-ONLY: every accessor is `inline`, so there is no
// paired .cpp to keep in step. Dawnmere's .cpp exists only because it owns
// measured tables; this file owns one too (see THE GROUND, below), but it holds it
// as an `inline constexpr` array at namespace scope instead -- the ONLY structural
// difference between it and the three tables in
// Source/World/ZM_DawnmerePlacement.cpp.
//
// ★ NEVER RE-SPELL A LITERAL FROM THIS FILE AT A CALL SITE. A constant spelled
// twice cannot red a drift -- both sides move together and the assertion becomes
// decorative. If you need a number here, include this header and read it.
//
// ★ THE BUILD INDEX IS NOT HERE, AND NEITHER IS THE SPAWN TAG. Thornacre's build
// index, scene kind, connections and offered tags live in the compiled world
// table -- Source/Data/ZM_WorldSpec.cpp, row ZM_SCENE_THORNACRE. Read the index
// with ZM_GetWorldSpec(ZM_SCENE_THORNACRE).m_uBuildIndex. Duplicating either
// here would create a second inventory nothing reconciles, which is why the
// return-edge accessors below are RESOLVERS that WALK the table rather than
// constants that mirror it -- the same shape ZM_GetProfLabExitTargetBuildIndex /
// ZM_GetProfLabExitSpawnTag already ship in the sibling header.
//
// ★ THIS HEADER DOES NOT INCLUDE ZM_TerrainAuthoring.h. The Thornacre recipe
// drags <filesystem>, <string> and Zenith_Vector into every consumer, and this
// file is included by leaf test TUs. The anchors below are stated here and
// RECONCILED against the recipe's landmarks and pads by the boot units, which
// may include it. It does not include ZM_Route1Placement.h either: Route 1 and
// Thornacre share exactly one seam -- the compiled world table -- and both read
// it independently.
//
// EVERY ACCESSOR BELOW IS TOTAL, in the ZM_GetProfLabBlock house style: no
// argument value, however degenerate, is UB, and NONE OF THEM CALLS
// Zenith_Assert. Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration,
// and the whole unit suite runs at boot before any scene loads, so an assert
// here would not fail one test -- it would end the run. Note in particular that
// ZM_GetWorldSpec ASSERTS FATALLY on an out-of-range id, so every resolver below
// guards BEFORE calling it.
// ============================================================================

// ---- The scene, and the five entities the stub authors ----------------------
//
// The name AddStep_CreateScene will take. The SAVE PATH is deliberately not
// here: GAME_ASSETS_DIR "Scenes/Thornacre" ZENITH_SCENE_EXT is compile-time
// literal concatenation, which cannot consume a const char* constant. The file
// STEM this name matches lives in Source/World/ZM_SceneRegistry.h, which is the
// one enumerable inventory of "which scene has a .zscen, under what stem".
//
// ★ NOT AN ENTITY NAME. "Thornacre" is a strict substring of every entity name
// below, so it must never join a name battery or a byte needle -- see the
// comment on ZM_GetThornacrePlacementEntityName.
inline constexpr const char* szZM_THORNACRE_SCENE_NAME = "Thornacre";

// The Terrain + Collider + ZM_TerrainGrass host entity.
//
// ★ DO NOT BUILD A "== 1" BYTE NEEDLE ON THIS NAME. Zenith_TerrainComponent's
// load path names its rebuilt materials from the host entity's name, so a scene
// that is ever LOADED-then-RE-SAVED can carry this string more than once. Needle
// the player, arrival or gate names instead.
inline constexpr const char* szZM_THORNACRE_TERRAIN_ENTITY_NAME = "ThornacreTerrain";

// ★ THE PLAYER ENTITY IS STILL NAMED "Player" -- BUT THAT IS NOW CONVENTION, NOT
// CONTRACT (Q-2026-08-15-002, fixed 2026-08-15).
//
// ZM_FollowCamera::ResolveTarget used to acquire its subject with
// FindEntityByName("Player") -- the only FindEntityByName call in the whole game
// layer -- so a rename in ONE scene cleared the camera's target and
// ZM_GameStateManager::PollForCameraAndBeginFadeIn bare-returned on a camera with
// no target: a barrier with NO TIMEOUT, i.e. a permanent black screen behind an
// opaque fade. Not a crash, not a red test.
//
// It now acquires the unique ZM_PlayerController in the camera's OWN scene. Keep
// the name for consistency with the shipped scenes; a rename is no longer fatal.
//
// ★ WHAT IS LOAD-BEARING NOW: a scene authoring a ZM_FollowCamera must also author
// a ZM_PlayerController on EXACTLY ONE entity -- zero or two give no target rather
// than a guess. Tests/ZM_Tests_FollowCamera.cpp pins all three cases.
//
// The byte-needle consequence is UNCHANGED: "Player" IS a substring of the
// component type name "ZM_PlayerController", so a needle on it uses the
// STRICTLY-MORE clause ZM_Tests_CommittedSceneBytes.cpp already ships, never a
// "== 1" equality -- and it is excluded from the type-name battery entirely.
inline constexpr const char* szZM_THORNACRE_PLAYER_ENTITY_NAME = "Player";

// The follow camera. Deliberately NOT "ThornacrePlayerCamera", which would make
// another authored name a strict prefix of it (the FromLab / FromLabSpawn trap).
inline constexpr const char* szZM_THORNACRE_CAMERA_ENTITY_NAME = "ThornacreCamera";

// The ZM_SpawnPoint the Route1 -> Thornacre warp lands on.
//
// ★ THE NAME DELIBERATELY DOES NOT CONTAIN ITS TAG. The tag is the world
// table's, resolved by ZM_GetThornacreReturnSpawnTag's opposite number on the
// Route 1 side and never spelled in this file -- and a marker whose NAME
// embedded that tag would make every future tag byte-needle count the entity
// name too. Named this way, a later slice can assert tagHits == 1 AND
// nameHits == 1 as plain equalities instead of reproducing the strictly-more
// clause.
inline constexpr const char* szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME =
	"ThornacreSouthArrival";

// The return ZM_WarpTrigger: the ONE entity that takes the player back out of
// this town, and the whole reason the stub is traversable rather than a dead end.
inline constexpr const char* szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME =
	"ThornacreSouthGate";

// ★ THE RULING-2 TRIPWIRE. Five entities, and this literal is what a sixth has
// to move. It is spelled as a number rather than derived from an enum on
// purpose: adding a gym door means a human edits this line, and the boot unit
// Thornacre_StubAuthorsNoGymDoorAndItsNamesAreUniqueLookupKeys spells 5u on its
// own side, so the edit reds instead of shipping.
inline constexpr u_int uZM_THORNACRE_PLACEMENT_ENTITY_COUNT = 5u;

// The five ENTITY names, walkable by index so a name battery can enumerate them
// instead of re-spelling them. The ORDER is contract (the units assert this
// accessor returns the i-th declared constant); appending is a ruling-level act,
// reordering is merely a diff nobody can read.
//
// ★ THE SCENE NAME IS NOT IN THIS LIST, DELIBERATELY. "Thornacre" is a strict
// substring of three of the five, so including it would red the pairwise
// non-substring clause on the first pair it reached -- and the scene name is not
// an entity anyway.
//
// TOTAL: "" (never nullptr) out of range, so a caller that walks past the end
// fails its own lookup instead of silently matching a real entity.
inline const char* ZM_GetThornacrePlacementEntityName(u_int uIndex)
{
	switch (uIndex)
	{
	case 0u: return szZM_THORNACRE_TERRAIN_ENTITY_NAME;
	case 1u: return szZM_THORNACRE_PLAYER_ENTITY_NAME;
	case 2u: return szZM_THORNACRE_CAMERA_ENTITY_NAME;
	case 3u: return szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME;
	case 4u: return szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME;
	default: break;
	}
	return "";
}

// ---- The return edge, RESOLVED from the compiled world table -----------------
//
// ★ WHY THESE ARE FUNCTIONS AND NOT A BUILD-INDEX LITERAL AND A TAG LITERAL.
// See the fourth star in the file banner: the build index and the spawn tag are
// the world table's property, and a second spelling of either here would be an
// inventory nothing reconciles. These walk the ZM_SCENE_THORNACRE row for the
// edge that targets Route 1 and return what the TABLE says, so a table edit that
// re-pointed or re-tagged the edge moves the authoring, the boot units and the
// live-scene clause together.
//
// ★ AND THE WALK IS BY TARGET, NOT BY INDEX 0. Thornacre already carries TWO
// edges -- the Route 1 return and the gym door this stub deliberately does not
// back -- so m_pxConnections[0] would be a magic index that silently hands back
// the gym the day the table is reordered.
//
// ★ THE GYM EDGE HAS NO ACCESSOR HERE, AND THAT IS THE RULING. A
// ZM_GetThornacreGymConnection() in this header would be a gym-named surface in
// a file whose whole point is that the gym does not exist yet. The unit that
// documents the deliberate asymmetry walks the row for it locally, in
// Tests/ZM_Tests_ThornacrePlacement.cpp.

// The answer when the compiled table carries NO Thornacre -> Route1 edge at all.
// Deliberately a build index no scene can hold rather than a plausible one, so a
// caller that skips the resolution check authors an obviously dead warp instead
// of a subtly wrong one.
inline constexpr u_int uZM_THORNACRE_RETURN_TARGET_UNRESOLVED = 0xFFFFFFFFu;

// TOTAL: a table with no such edge (or no connection array at all) yields
// nullptr rather than UB, and the three accessors below turn that into their own
// stated sentinels.
inline const ZM_SceneConnection* ZM_GetThornacreReturnConnection()
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_THORNACRE);
	if (xRow.m_pxConnections == nullptr)
	{
		return nullptr;
	}
	for (u_int uEdge = 0u; uEdge < xRow.m_uConnectionCount; ++uEdge)
	{
		if (xRow.m_pxConnections[uEdge].m_eTarget == ZM_SCENE_ROUTE1)
		{
			return &xRow.m_pxConnections[uEdge];
		}
	}
	return nullptr;
}

inline ZM_SCENE_ID ZM_GetThornacreReturnTargetScene()
{
	const ZM_SceneConnection* pxEdge = ZM_GetThornacreReturnConnection();
	return pxEdge != nullptr ? pxEdge->m_eTarget : ZM_SCENE_NONE;
}

// ★ THE RANGE GUARD IS NOT DEFENSIVE DECORATION. ZM_GetWorldSpec ASSERTS FATALLY
// on an out-of-range id and Zenith_Assert breaks in every configuration, so a
// table that ever handed back ZM_SCENE_NONE here would end the whole boot-unit
// run rather than report one failure.
inline u_int ZM_GetThornacreReturnTargetBuildIndex()
{
	const ZM_SCENE_ID eTarget = ZM_GetThornacreReturnTargetScene();
	return eTarget < ZM_SCENE_COUNT
		? ZM_GetWorldSpec(eTarget).m_uBuildIndex
		: uZM_THORNACRE_RETURN_TARGET_UNRESOLVED;
}

// The tag the return trigger asks Route 1 for -- and therefore ALSO the tag the
// Route 1 arrival marker must carry. ONE spelling for both sides of the seam is
// the whole point: ZM_GameStateManager::IsWarpDestinationValid consults only this
// table and never the destination scene, so a marker tagged with anything else
// passes validation and then parks the warp machine in
// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN forever.
//
// TOTAL: "" on a miss, NEVER nullptr.
inline const char* ZM_GetThornacreReturnSpawnTag()
{
	const ZM_SceneConnection* pxEdge = ZM_GetThornacreReturnConnection();
	return pxEdge != nullptr && pxEdge->m_szSpawnTag != nullptr
		? pxEdge->m_szSpawnTag
		: "";
}

// ============================================================================
// ---- THE ANCHOR COLUMNS ------------------------------------------------------
//
// ★★ BOTH ANCHORS' XZ ARE DECLARED HERE, IN ONE BLOCK, ABOVE THE SECTIONS THAT
// DISCUSS THEM -- AND THAT ORDER IS FORCED, NOT STYLISTIC. The MEASURED GROUND
// table below carries one row per column and each row NAMES its column by
// spelling that column's XZ. A `constexpr` initialiser cannot forward-reference a
// constant declared further down the file, so the table cannot sit above these;
// and it cannot be pushed to the end of the file either, because the arrival and
// gate heights are DERIVED from it in the sections that follow.
//
// ★ THE ALTERNATIVE -- GIVING EACH TABLE ROW ITS OWN INDEPENDENT XZ LITERALS --
// WAS REJECTED OUTRIGHT. This file's loudest rule is that a number has ONE
// spelling (third star in the banner): a second copy of an anchor's XZ inside the
// ground table is exactly the drift-invisible duplication that rule exists to
// forbid, and a static_assert reconciling the two copies would merely be a THIRD
// spelling of the same number.
// ============================================================================

// ---- The arrival marker's column ---------------------------------------------
//
// The XZ of the Thornacre recipe's ROUTE-ARRIVAL landmark -- the one named after
// the inbound Route 1 edge -- which sits inside the flattened "RouteGate" pad.
// Both claims are reconciled against the recipe by the boot unit; nothing here
// reads the recipe itself (see the banner), and the landmark's name is not
// spelled here because it is also a spawn tag.
inline constexpr float fZM_THORNACRE_SOUTH_ARRIVAL_X = 512.0f;
inline constexpr float fZM_THORNACRE_SOUTH_ARRIVAL_Z = 112.0f;

// ---- The return gate's column ------------------------------------------------
//
// 12 m SOUTH of the arrival marker, still inside the flattened "RouteGate" pad.
//
// ★ NO GATE ENTITY IS AUTHORED BY THIS SLICE -- the return trigger is R1-3's, in
// one commit, after the destination markers exist. The column is measured anyway
// because the gate's Y derives from the ground under it: leaving it out of the
// table below would strand it on the very scalar this split is retiring, and a
// stranded column is how a sensor ends up 28 m under the player.
inline constexpr float fZM_THORNACRE_SOUTH_GATE_X = 512.0f;
inline constexpr float fZM_THORNACRE_SOUTH_GATE_Z = 100.0f;

// ============================================================================
// ---- THE GROUND: A RECIPE MIRROR **AND** A MEASURED TABLE --------------------
//
// ★★ TWO DIFFERENT CLAIMS, AND THEY MUST NOT BE COLLAPSED BACK INTO ONE SCALAR.
// Until R1-2 this file carried a single fZM_THORNACRE_PROVISIONAL_GROUND_Y that
// meant BOTH "the height the recipe flattens to" AND "the surface under this
// anchor". Those are the same number only while the table below is unfrozen; the
// day a real raycast lands they stop being the same number, and they stop being
// the same number PER COLUMN.
//
//   (a) fZM_THORNACRE_RECIPE_TARGET_GROUND_Y MIRRORS the Thornacre terrain
//       recipe's m_fTargetHeight. It is exact, it is not a measurement, and it
//       must stay exactly equal to the recipe. Its reader is the shipped boot unit
//       Thornacre_ArrivalSitsOnTheRecipeLandmarkTheTerrainFlattened, which
//       reconciles it against xRecipe.m_fTargetHeight.
//       ★ NEVER GIVE THIS CONSTANT A MEASURED VALUE. That would turn a
//       recipe-vs-recipe reconciliation into a recipe-vs-raycast comparison, which
//       reds for a reason its message does not name.
//
//   (b) axZM_THORNACRE_GROUND_SAMPLES is the MEASURED surface, one row per column.
//       EVERYTHING that means "the real ground under this anchor" reads it.
// ============================================================================

// (a) THE RECIPE MIRROR.
inline constexpr float fZM_THORNACRE_RECIPE_TARGET_GROUND_Y = 28.0f;

// ---- (b) THE MEASURED GROUND -------------------------------------------------
//
// Same row shape and same idiom as the three measured tables in
// Source/World/ZM_DawnmerePlacement.cpp (the ZM-D-173 Home block, the SC-D lab
// block and the R1-2 route-seam block): name, X, Z, feet Y, ONE VALUE PER LINE, so
// a re-measure is a per-row replacement rather than a rewrite.
struct ZM_ThornacreGroundSample
{
	// The column's name. Each row is named by the ENTITY that stands on it, read
	// from the one place that spells it -- the gate is not authored until R1-3, but
	// its name already exists above and re-typing it here would be a second
	// spelling. It is also the key a ground-truth oracle prints its `paste=`
	// literal against, so it is contract.
	const char* m_szEntityName;
	float       m_fX;
	float       m_fZ;
	// The TERRAIN SURFACE at (m_fX, m_fZ) -- never a body centre. Callers that want
	// a centre add the body half-extent through the accessors further down, so the
	// capsule arithmetic exists in exactly one place.
	float       m_fFeetY;
};

// ★ ITS OWN ENUM, ITS OWN ARRAY AND ITS OWN COUNT, for the reason
// ZM_DawnmerePlacement.h spells out on its lab enum: appending a row to another
// table's enum measures it up to THAT table's fixed probe-slot bound and silently
// DROPS it beyond, with every test still green.
//
// The ORDER is contract -- the deduced-bound static_assert below only checks the
// COUNT, so a reordering that kept the count would silently swap two columns'
// heights.
enum ZM_THORNACRE_GROUND_SAMPLE : u_int
{
	ZM_THORNACRE_GROUND_SAMPLE_SOUTH_ARRIVAL,
	ZM_THORNACRE_GROUND_SAMPLE_SOUTH_GATE,

	ZM_THORNACRE_GROUND_SAMPLE_COUNT
};

// ==== R1-2 MEASURED THORNACRE GROUND -- PROVISIONAL, NOT YET FROZEN ====
//
// ★★ BOTH ROWS ARE SEEDED WITH THE RECIPE TARGET HEIGHT, NOT WITH AN OUT-OF-BAND
// SENTINEL, AND THAT IS DELIBERATE AND LOAD-BEARING. Dawnmere's unfrozen rows
// shipped on fZM_DAWNMERE_LAB_GROUND_UNMEASURED -- a finite value a million metres
// below the world -- because NOTHING WAS AUTHORED from them while they were
// unfrozen. Thornacre is the opposite case: this milestone authors the stub from
// these rows BEFORE any oracle exists, and one of the entities it authors is a
// DYNAMIC player capsule. A -1000000.0f seed would author that capsule a million
// metres under the world on the provisional boot, and physics would take it from
// there.
//
// ★★ AND THE SEED IS GOOD TO WELL UNDER A METRE, WHICH IS A MEASUREMENT RATHER
// THAN A HOPE (R1-2 step 1). Dawnmere's route-seam column froze at 24.36592
// against a 24.0 recipe target -- target + 0.366 -- while every OTHER measured
// Dawnmere column reads ~2 m above its target. The difference is that the seam
// column is the first measured Dawnmere column INSIDE A FLATTEN CORRIDOR, and a
// FLATTEN dab drives ground TO the target; the ~+2 m on the others is the
// region-wide hydraulic-erosion deposit pass sitting on top of UNFLATTENED ground.
// Both columns here sit inside the flattened "RouteGate" pad, so each seed is
// expected to be within a fraction of a metre of the truth.
// ★ DO NOT READ A NEAR-TARGET RESULT AS A BROKEN PROBE when the oracle finally
// runs. Near-target is the PREDICTION here, not a symptom.
//
// ★★ BE HONEST ABOUT WHAT THE SEED COSTS: BECAUSE BOTH ROWS EQUAL THE MIRROR, NO
// COMPILED-CONSTANT UNIT IN THIS GAME CAN TELL THIS TABLE IS UNFROZEN. A boot unit
// asserting "the rows sit near the recipe target" passes VACUOUSLY today and would
// keep passing if the freeze never happened. The only thing that can see it is a
// real downward raycast against a baked Thornacre terrain body -- and there is
// none yet, because no Thornacre.zscen exists to raycast against. That oracle is
// the freeze slice's first deliverable, not this one's; until it lands, the
// unfrozen-ness of this table is carried by this comment and nothing else.
//
// TO FREEZE, once that oracle exists: run it against a warm Thornacre terrain
// bake, take the `paste=` literal it prints for each row NAME below, put each in
// place of that row's seed, rebuild, and re-author Thornacre from a windowed tools
// boot. Every derived height in the sections that follow moves with it
// automatically.
//
// ★ DO NOT HAND-EDIT A ROW TO MAKE A TEST PASS. If the oracle reds, the terrain
// moved under the town and the correct response is to RE-RUN it and re-paste, AS A
// SET. Hand-tuning one row is how a band guard ends up guarding nothing.
//
// ★ WHAT RE-MEASURES THEM AFTERWARDS: any change to the Thornacre terrain recipe
// (a pad, path, landmark, seed, flatten radius or density divisor) OR to the
// terrain collision topology. Such a change re-measures EVERY ground table in the
// game or leaves a silent staleness behind -- ZM-D-182 and ZM-D-186 are both
// worked examples of exactly that going wrong.
inline constexpr ZM_ThornacreGroundSample axZM_THORNACRE_GROUND_SAMPLES[] =
{
	// ★★ FROZEN 2026-08-15 AS A SET, from ZM_ThornacreGroundTruth_Test against a
	// warm Thornacre bake and the committed Thornacre.zscen. Both rows:
	// resolved=1, hitTerrain=1, finalHit='ThornacreTerrain' -- never 'Player'.
	// OBSERVED, never derived.
	//
	// Both sit +0.376 .. +0.630 above the 28.0 recipe target, i.e. NEAR it, which
	// is what flattened ground reads (see the sibling note in
	// ZM_Route1Placement.h). The two columns are only 12 m apart inside one 30 m
	// flatten pad and differ by 0.254 m, so do NOT add a minimum-spread clause
	// here -- a spread assertion copied from a table whose columns are hundreds of
	// metres apart would red on entirely correct content.
	//
	// entity name,                              x,                             z,                             feet Y (MEASURED)
	{ szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME,  fZM_THORNACRE_SOUTH_ARRIVAL_X, fZM_THORNACRE_SOUTH_ARRIVAL_Z, 28.63044f },
	{ szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME,     fZM_THORNACRE_SOUTH_GATE_X,    fZM_THORNACRE_SOUTH_GATE_Z,    28.37605f },
};
// ==== END R1-2 MEASURED THORNACRE GROUND ====

// The bound is DEDUCED, never spelled. With an explicit
// [ZM_THORNACRE_GROUND_SAMPLE_COUNT] this static_assert would be a tautology and a
// forgotten row would merely zero-initialise the tail into a NULL-named column at
// the world origin AT HEIGHT ZERO -- 28 m under the town.
static_assert(
	sizeof(axZM_THORNACRE_GROUND_SAMPLES) / sizeof(axZM_THORNACRE_GROUND_SAMPLES[0])
		== ZM_THORNACRE_GROUND_SAMPLE_COUNT,
	"the Thornacre ground-sample table must have exactly one row per ZM_THORNACRE_GROUND_SAMPLE");

// The answer for an id no row covers. Every field is DEFINED and INERT: a name no
// column carries, a coordinate at the world's (0, _, 0) corner -- outside every pad
// and flatten radius -- and a FINITE height, so a caller that skipped the range
// check gets an obviously-wrong-but-legal anchor rather than a NaN in a transform
// or a body a million metres down.
inline constexpr ZM_ThornacreGroundSample xZM_THORNACRE_INVALID_GROUND_SAMPLE =
{
	"ThornacreInvalidGroundSample", 0.0f, 0.0f, fZM_THORNACRE_RECIPE_TARGET_GROUND_Y
};

inline constexpr u_int ZM_GetThornacreGroundSampleCount()
{
	return (u_int)ZM_THORNACRE_GROUND_SAMPLE_COUNT;
}

// TOTAL, and `constexpr` because every derived height below is constexpr. NOTHING
// HERE ASSERTS -- Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration
// and the whole unit suite runs at boot, so an assert on an argument a unit
// deliberately feeds does not fail one test, it ends the run.
inline constexpr const ZM_ThornacreGroundSample& ZM_GetThornacreGroundSample(
	ZM_THORNACRE_GROUND_SAMPLE eSample)
{
	return eSample < ZM_THORNACRE_GROUND_SAMPLE_COUNT
		? axZM_THORNACRE_GROUND_SAMPLES[eSample]
		: xZM_THORNACRE_INVALID_GROUND_SAMPLE;
}

// ★ THIS IS THE FUNCTION EVERY "the real ground under this anchor" DERIVATION
// READS. If you are reaching for fZM_THORNACRE_RECIPE_TARGET_GROUND_Y to place
// something, you want this instead.
inline constexpr float ZM_ThornacreGroundFeetY(ZM_THORNACRE_GROUND_SAMPLE eSample)
{
	return ZM_GetThornacreGroundSample(eSample).m_fFeetY;
}

// ---- The arrival marker ------------------------------------------------------
//
// ★ A ZM_SpawnPoint's AUTHORED TRANSFORM IS THE FEET, NOT THE BODY CENTRE.
// ZM_GameStateManager::CalculateSpawnCenter adds fZM_HUMAN_BODY_HALF_HEIGHT at
// warp time, so authoring a body centre on a marker would warp every arriving
// player half a body into the air.
inline constexpr float fZM_THORNACRE_ARRIVAL_FEET_Y =
	ZM_ThornacreGroundFeetY(ZM_THORNACRE_GROUND_SAMPLE_SOUTH_ARRIVAL);

inline Zenith_Maths::Vector3 ZM_GetThornacreSouthArrivalFeet()
{
	return Zenith_Maths::Vector3(
		fZM_THORNACRE_SOUTH_ARRIVAL_X,
		fZM_THORNACRE_ARRIVAL_FEET_Y,
		fZM_THORNACRE_SOUTH_ARRIVAL_Z);
}

// The RESTING body centre a placed or warped-in player occupies -- the
// vocabulary every human position in this game is stated in (ZM_HumanBody.h).
// Read by the gate-clearance and camera arithmetic below.
inline Zenith_Maths::Vector3 ZM_GetThornacreSouthArrivalBodyCentre()
{
	Zenith_Maths::Vector3 xCentre = ZM_GetThornacreSouthArrivalFeet();
	xCentre.y += fZM_HUMAN_BODY_HALF_HEIGHT;
	return xCentre;
}

// ---- The authored player -----------------------------------------------------
//
// ★ ZM-D-184: AN AUTHORED DYNAMIC BODY SPAWNS WITH CLEARANCE, NEVER AT EXACT
// CONTACT. A body authored touching the terrain bursts physics substeps on the
// first frame and falls THROUGH the ground (that is how Vesper vanished). One
// extra half-extent is the rule ZM_DawnmereTrainerSpawnY / ZM_DawnmereWandererSpawnY
// already follow, and the boot unit pins the gap at exactly one half-extent --
// not zero, not two.
inline constexpr float fZM_THORNACRE_AUTHORED_PLAYER_CLEARANCE =
	fZM_HUMAN_BODY_HALF_HEIGHT;

// The value AddStep_SetTransformPosition takes for the Player entity: the resting
// centre, lifted by one clearance.
inline Zenith_Maths::Vector3 ZM_GetThornacreAuthoredPlayerCentre()
{
	Zenith_Maths::Vector3 xCentre = ZM_GetThornacreSouthArrivalBodyCentre();
	xCentre.y += fZM_THORNACRE_AUTHORED_PLAYER_CLEARANCE;
	return xCentre;
}

// ---- The camera --------------------------------------------------------------
//
// ★ ZM-D-173, restated for an outdoor arrival. ZM_FollowCamera keeps the yaw the
// SCENE authored and the spring places the camera BEHIND its subject; camera
// forward at yaw 0 is +Z, because ZM_ForwardFromRotation rotates the +Z basis.
// The player arrives at the town's SOUTH edge off Route 1 and walks NORTH into
// it, so yaw 0 looks into town and the camera trails to -Z, out over the route
// gate, where there is nothing to clip into.
inline constexpr float fZM_THORNACRE_CAMERA_YAW   = 0.0f;

// Radians, tilted DOWN. Chosen independently for this arrival; it is not a mirror
// of any other camera in the game, and the sign is load-bearing -- flip it and
// the eye ends up under the terrain. The boot unit
// Thornacre_SettledCameraStandsAboveGroundBehindTheArrival
// (Tests/ZM_Tests_ThornacrePlacement.cpp) runs the sign-flipped pitch through
// this same arithmetic and requires it to FAIL the clearance floor, so that claim
// is checked rather than believed.
inline constexpr float fZM_THORNACRE_CAMERA_PITCH = -0.22f;

inline constexpr float fZM_THORNACRE_CAMERA_ARM          = 6.0f;   // pivot -> camera
inline constexpr float fZM_THORNACRE_CAMERA_PIVOT_HEIGHT = 0.60f;  // above body centre
inline constexpr float fZM_THORNACRE_CAMERA_FOV_DEGREES  = 65.0f;
inline constexpr float fZM_THORNACRE_CAMERA_NEAR         = 0.1f;

// The town is a 1024 x 1024 m region and the camera looks up its length from the
// southern gate, so a 100 m far plane (the interiors' figure) would clip the
// world away in front of the player.
inline constexpr float fZM_THORNACRE_CAMERA_FAR          = 2400.0f;
inline constexpr float fZM_THORNACRE_CAMERA_ASPECT       = 16.0f / 9.0f;

// The floor the camera unit enforces above the provisional ground: a pitch sign
// error buries the eye in the terrain and nothing else in the game would notice.
// The reader is Thornacre_SettledCameraStandsAboveGroundBehindTheArrival, which
// also pins the far plane against the recipe's own Z extent and the settled eye
// behind the arriving body.
inline constexpr float fZM_THORNACRE_CAMERA_MIN_GROUND_CLEARANCE = 1.0f;

// The pose the arriving player is actually FILMED FROM: the pivot (the arrival
// body centre, lifted by the follow camera's pivot height) with the arm swung
// back along the camera's forward vector.
//
// ★ THE TRIGONOMETRY LIVES INSIDE THIS ACCESSOR AND NOWHERE ELSE, AND NOTHING
// IT PRODUCES IS EVER FROZEN INTO A SCENE. std::cos / std::sin are libm results
// that MSVC Debug and Release codegen disagree on by 1-2 ULP, which is exactly
// how ZM-D-183 made a committed .zscen ping-pong in git forever. This value is
// read by boot units and by the authoring's clearance reasoning; the authored
// camera ENTITY carries no rotation built from it.
inline Zenith_Maths::Vector3 ZM_GetThornacreSettledCameraPosition()
{
	Zenith_Maths::Vector3 xPivot = ZM_GetThornacreSouthArrivalBodyCentre();
	xPivot.y += fZM_THORNACRE_CAMERA_PIVOT_HEIGHT;

	// Forward at (yaw, pitch), in the convention ZM_ForwardFromRotation states:
	// yaw 0 is +Z, and a NEGATIVE pitch looks down.
	const float fCosPitch = std::cos(fZM_THORNACRE_CAMERA_PITCH);
	const Zenith_Maths::Vector3 xForward(
		std::sin(fZM_THORNACRE_CAMERA_YAW) * fCosPitch,
		std::sin(fZM_THORNACRE_CAMERA_PITCH),
		std::cos(fZM_THORNACRE_CAMERA_YAW) * fCosPitch);

	return xPivot - xForward * fZM_THORNACRE_CAMERA_ARM;
}

// ---- The return gate volume ---------------------------------------------------

// ★ BOOKED DUPLICATION, not an oversight: this struct is line-for-line identical
// to ZM_PlayerHomeBlockout in the sibling Source/World/ZM_PlayerHomePlacement.h
// (which itself duplicates ZM_ProfLabBlockout and ZM_DawnmereBlockout).
// Consolidating all of them into a shared Source/World/ZM_Blockout.h would touch
// the Dawnmere, ProfLab and PlayerHome authoring blocks and their test suites,
// which is out of R1-1's scope. Do the merge as its own change, not as a
// drive-by.
struct ZM_ThornacreVolume
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xScale;

	// Half-extents, i.e. what the AABB arithmetic actually wants.
	Zenith_Maths::Vector3 HalfExtent() const { return m_xScale * 0.5f; }
	Zenith_Maths::Vector3 Min() const { return m_xCenter - HalfExtent(); }
	Zenith_Maths::Vector3 Max() const { return m_xCenter + HalfExtent(); }
};

// Wide enough that the walked corridor has no way past it. The route gate sits in
// a 30 m-radius flattened pad on the town's south edge; 48 m centred on the lane
// leaves no walk-around inside that corridor. Side blockers are a later slice's
// business, not this one's.
inline constexpr float fZM_THORNACRE_GATE_SCALE_X = 48.0f;

// Taller than fZM_HUMAN_BODY_HEIGHT (1.8), so the sensor cannot be stepped over.
inline constexpr float fZM_THORNACRE_GATE_SCALE_Y = 4.0f;

// Depth. ZM_WarpTrigger fires from OnCollisionEnter -- a body-vs-body contact --
// so the window a walker spends touching the box is this depth plus two capsule
// radii, which is many physics ticks rather than a plane a running capsule can
// step over between them.
inline constexpr float fZM_THORNACRE_GATE_SCALE_Z = 6.0f;

// ★ THE GATE SITS **ON** THE GROUND, AND THAT IS THE WHOLE POINT OF THIS LINE.
// A trigger authored at world Y 0 is a tunnel 28 m below the player: it never
// fires, the return warp is dead, and every compiled scale constant above still
// looks perfectly correct. The rule is identical in form to Dawnmere's triggers
// (its ground + half its height).
//
// ★ AND THE GROUND IT READS IS THE GATE'S **OWN** MEASURED COLUMN, not the
// arrival's. The two columns are 12 m apart; they agree exactly while the table is
// unfrozen and they will not agree afterwards, and a sensor resting on its
// neighbour's ground is part-buried or hovering with every scale constant still
// correct. The gate's XZ lives in THE ANCHOR COLUMNS block above, because the
// measured table has to name the column and constexpr cannot forward-reference.
inline constexpr float fZM_THORNACRE_GATE_CENTRE_Y =
	ZM_ThornacreGroundFeetY(ZM_THORNACRE_GROUND_SAMPLE_SOUTH_GATE)
	+ fZM_THORNACRE_GATE_SCALE_Y * 0.5f;

// ★ THE Q-2026-08-15-001 LESSON, AS A NUMBER. An arriving player walks off the
// marker immediately, and a diagonal walk-up spends depth as fast as it spends
// width -- which is how a professor's body came to clip an exit sensor that
// "obviously" cleared him. The near face of a gate must clear the arrival body
// centre by at least this, or arriving in town instantly re-warps the player back
// to the route: an infinite ping-pong between two scenes, with every other unit
// green.
inline constexpr float fZM_THORNACRE_GATE_ARRIVAL_CLEARANCE_MIN = 4.0f;

inline ZM_ThornacreVolume ZM_GetThornacreSouthGate()
{
	return {
		Zenith_Maths::Vector3(
			fZM_THORNACRE_SOUTH_GATE_X,
			fZM_THORNACRE_GATE_CENTRE_Y,
			fZM_THORNACRE_SOUTH_GATE_Z),
		Zenith_Maths::Vector3(
			fZM_THORNACRE_GATE_SCALE_X,
			fZM_THORNACRE_GATE_SCALE_Y,
			fZM_THORNACRE_GATE_SCALE_Z) };
}

// ============================================================================
// ★ CLOSING NOTE -- NOTHING IN THIS STUB CARRIES AN AUTHORED ROTATION, AND THAT
// IS WHY THERE ARE NO FROZEN BITS IN THIS FILE.
//
// The terrain, the arrival marker and the return gate are all axis-aligned; the
// player is a CAPSULE because it also has to move; the camera's pose is computed,
// never serialized from trigonometry. So every static collider Thornacre authors
// is correctly COLLISION_VOLUME_TYPE_AABB, and "upgrading" one to OBB would
// rewrite committed bytes for zero behavioural gain.
//
// ★ IF A LATER SLICE ADDS SOMETHING THAT MUST FACE A DIRECTION -- an NPC, a sign,
// a shopkeeper -- READ ZM-D-193 FIRST. An AABB body forces JPH::Quat::sIdentity()
// and the physics->transform sync writes that identity straight back into the
// SAVED SCENE BYTES with every compiled-constant unit still green, so
// AddStep_SetTransformRotationQuat on an AABB entity is a SILENT NO-OP. Such an
// entity takes OBB (or a dynamic capsule if it walks), and its rotation is a
// FROZEN std::bit_cast constant emitted BETWEEN the scale step and the collider
// step -- never AddStep_SetTransformYaw, which builds its quaternion with libm at
// authoring time and made a committed scene differ Debug vs Release (ZM-D-183).
//
// And it is still not a gym door. See the top of this file.
// ============================================================================
