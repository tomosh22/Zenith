#include "Zenith.h"

// ============================================================================
// ZM_Tests_NewGameEntry (ZM-D-176) -- the boot units that pin WHERE A NEW RUN
// BEGINS, and that keep the new-game flow and the whiteout flow from silently
// re-merging.
//
// PURE: no scene, no entity, no physics, no assets, no graphics, no g_xEngine.
// Everything here reads COMPILED CONSTANTS -- ZM_GameStateManager's four static
// destination constants and the compiled ZM_WorldSpec table -- plus
// ZM_GameStateManager::IsWarpDestinationValid, which is a public static that
// consults only ZM_FindSceneByBuildIndex and the target row's offered tag list
// (ZM_GameStateManager.cpp) and so needs no live manager.
//
// ★ AND THESE UNITS CREATE NO ENTITY. Hard constraint, not a style note: scene
// authoring bakes in the entity indices assigned during the boot it runs in, and
// the boot-time unit suite allocates entities FIRST -- so one entity-creating
// boot unit re-authors different .zscen bytes and invalidates the two-boot
// byte-stability property ZM-D-148 established.
//
// ★ WHAT THESE UNITS CANNOT DO. They run BEFORE the initial scene loads, so they
// see neither the scene registry nor one byte of Assets/Scenes/PlayerHome.zscen.
// They cannot prove the destination is REGISTERED (an unregistered index passes
// IsWarpDestinationValid, is ACCEPTED, and then parks forever in
// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN, which has no timeout), and they cannot
// prove a real Enter on the title actually goes there. Those two claims belong to
// ZM_SaveContinue_Test's AwaitPlayerHome phase. Do not read this file's greenness
// as "New Game works".
//
// ★ AND NOTHING HERE RE-SPELLS 40 OR "Door". Every clause reads
// ZM_GameStateManager::uNEW_GAME_BUILD_INDEX / szNEW_GAME_SPAWN_TAG and
// reconciles them against the compiled world table. A unit that restated the
// pair on both sides of an equality would compare a literal against itself and
// could not red a drift in the constants the game actually uses.
// ============================================================================

#include <cstring>   // strcmp -- the tag-identity claims

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

namespace
{
	// Does a scene row OFFER this spawn tag? The referential-closure helper every
	// clause below shares, so "offers" means one thing in this file.
	bool NewGameEntrySceneOffersTag(const ZM_WorldSpec& xSpec, const char* szTag)
	{
		if (szTag == nullptr || xSpec.m_pszSpawnTags == nullptr)
		{
			return false;
		}
		for (u_int uTag = 0u; uTag < xSpec.m_uSpawnTagCount; ++uTag)
		{
			if (xSpec.m_pszSpawnTags[uTag] != nullptr
				&& std::strcmp(xSpec.m_pszSpawnTags[uTag], szTag) == 0)
			{
				return true;
			}
		}
		return false;
	}
}

// ============================================================================
// U1 -- the destination a new run is queued to.
// ============================================================================

// ★ ZM_GetWorldSpec IS NOT CALLED UNTIL THE RESOLVE IS PROVEN. It asserts
// (fatally, in every configuration) on the ZM_SCENE_NONE an unresolvable index
// returns, and the whole unit suite runs at boot -- so an unguarded call would not
// fail one test, it would end the boot unit run and take the gate down. Every
// lookup below is behind its resolve.
ZENITH_TEST(ZM_Data, NewGameEntry_DestinationIsThePlayerHomeDoor)
{
	const ZM_SCENE_ID eDestination =
		ZM_FindSceneByBuildIndex(ZM_GameStateManager::uNEW_GAME_BUILD_INDEX);

	ZENITH_ASSERT_EQ((u_int)eDestination, (u_int)ZM_SCENE_PLAYERHOME,
		"a new run is queued to build index %u, which resolves to %s -- ZM-D-176 "
		"moved the entry point into the player's bedroom, and a destination that "
		"is not PlayerHome means the constant and the world table disagree",
		ZM_GameStateManager::uNEW_GAME_BUILD_INDEX,
		ZM_GetSceneName(eDestination));

	if (eDestination != ZM_SCENE_PLAYERHOME)
	{
		return;   // already reported; every clause below would read the wrong row
	}

	const ZM_WorldSpec& xSpec = ZM_GetWorldSpec(eDestination);

	// The row must OFFER the tag the flow asks for. Without this the warp is
	// accepted by nothing, or -- worse -- accepted and then parked in
	// WAITING_FOR_SPAWN, which has no timeout: a silent hang, not a crash.
	ZENITH_ASSERT_NOT_NULL(ZM_GameStateManager::szNEW_GAME_SPAWN_TAG,
		"the new-game spawn tag constant is null");
	ZENITH_ASSERT_TRUE(
		NewGameEntrySceneOffersTag(
			xSpec, ZM_GameStateManager::szNEW_GAME_SPAWN_TAG),
		"%s does not offer spawn tag '%s', which is where a new run is sent -- "
		"the transition would queue and then wait forever for a marker no scene "
		"authors", xSpec.m_szName,
		ZM_GameStateManager::szNEW_GAME_SPAWN_TAG != nullptr
			? ZM_GameStateManager::szNEW_GAME_SPAWN_TAG : "(null)");

	// An INTERIOR with an EMPTY terrain set. This is not decoration: the empty
	// terrain set is exactly what puts PlayerHome in the ALWAYS-RUN authoring
	// section rather than behind the AUTHOR_DAWNMERE gate, so a new run cannot
	// depend on a warm terrain bake to have a room to start in. Give this row a
	// terrain set and a fresh headless boot has no entry point at all.
	ZENITH_ASSERT_EQ((u_int)xSpec.m_eKind, (u_int)ZM_SCENE_KIND_INTERIOR,
		"the new-game destination is authored as kind %s; a new run must begin in "
		"a terrain-independent interior that every tools boot authors",
		ZM_SceneKindToString(xSpec.m_eKind));
	ZENITH_ASSERT_NOT_NULL(xSpec.m_szTerrainSet,
		"the new-game destination's terrain set is null rather than empty");
	ZENITH_ASSERT_TRUE(
		xSpec.m_szTerrainSet != nullptr && xSpec.m_szTerrainSet[0] == '\0',
		"the new-game destination declares terrain set '%s' -- that moves its "
		"authoring behind the AUTHOR_DAWNMERE gate, so a headless/cold-terrain "
		"boot would not author the room a new run starts in",
		xSpec.m_szTerrainSet != nullptr ? xSpec.m_szTerrainSet : "(null)");

	// ...and the SHIPPED validator agrees. Running the real predicate rather than
	// re-deriving it is what stops this unit passing while RequestNewGame's own
	// TryQueueWarp refuses the destination for a reason this file did not model.
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::IsWarpDestinationValid(
			ZM_GameStateManager::uNEW_GAME_BUILD_INDEX,
			ZM_GameStateManager::szNEW_GAME_SPAWN_TAG),
		"IsWarpDestinationValid rejects the new-game pair (%u, '%s') -- "
		"RequestNewGame queues through that same validator, so the title's New "
		"Game would be refused outright",
		ZM_GameStateManager::uNEW_GAME_BUILD_INDEX,
		ZM_GameStateManager::szNEW_GAME_SPAWN_TAG != nullptr
			? ZM_GameStateManager::szNEW_GAME_SPAWN_TAG : "(null)");
}

// ============================================================================
// U2 -- new game and whiteout are two flows, not one.
// ============================================================================

// ★ THE UNIT THE SHIPPED SEPARATION COMMENT EXISTS TO DEMAND. The two pairs sat
// on the SAME destination for the whole of S5-S7 and were kept semantically
// separate purely by a comment; ZM-D-176 moved the new-game pair and left the
// whiteout pair alone, so they now differ in fact. Nothing enforced that before
// this unit -- and the enforcement matters in BOTH directions:
//
//   * moving whiteout to follow new-game sends a fainted player to their bedroom
//     instead of the town square, which is a gameplay change nobody asked for;
//   * moving new-game back onto whiteout re-merges the flows and re-creates
//     exactly the condition the separation comment was written to prevent.
//
// The last two clauses are what stop a lazy "fix": someone who satisfies the
// difference by moving WHITEOUT, rather than by leaving it alone, reds here.
ZENITH_TEST(ZM_Data, NewGameEntry_DiffersFromTheWhiteoutDestination)
{
	ZENITH_ASSERT_NE(ZM_GameStateManager::uNEW_GAME_BUILD_INDEX,
		ZM_GameStateManager::uWHITEOUT_BUILD_INDEX,
		"the new-game and whiteout destinations share build index %u -- the two "
		"flows have silently re-merged, and either one can now be moved by an "
		"edit aimed at the other",
		ZM_GameStateManager::uNEW_GAME_BUILD_INDEX);

	ZENITH_ASSERT_NOT_NULL(ZM_GameStateManager::szNEW_GAME_SPAWN_TAG,
		"the new-game spawn tag constant is null");
	ZENITH_ASSERT_NOT_NULL(ZM_GameStateManager::szWHITEOUT_SPAWN_TAG,
		"the whiteout spawn tag constant is null");
	if (ZM_GameStateManager::szNEW_GAME_SPAWN_TAG != nullptr
		&& ZM_GameStateManager::szWHITEOUT_SPAWN_TAG != nullptr)
	{
		ZENITH_ASSERT_NE(
			std::strcmp(ZM_GameStateManager::szNEW_GAME_SPAWN_TAG,
				ZM_GameStateManager::szWHITEOUT_SPAWN_TAG), 0,
			"the new-game and whiteout flows both arrive at spawn tag '%s'",
			ZM_GameStateManager::szNEW_GAME_SPAWN_TAG);
	}

	// ★ AND WHITEOUT IS STILL DAWNMERE'S TOWN SQUARE. Without these two clauses
	// the difference above could be satisfied by moving the WRONG pair, and this
	// unit would go green on a build where a fainted player wakes up somewhere
	// nobody intended. The whiteout destination has exactly one reader
	// (ZM_GameStateManager.cpp's whiteout consume block) and its runtime oracles
	// are ZM_RivalVesper_Test / ZM_BattleMenu_Test -- both of which are automated
	// and neither of which is cheap. This is the boot-cost guard.
	const ZM_SCENE_ID eWhiteout =
		ZM_FindSceneByBuildIndex(ZM_GameStateManager::uWHITEOUT_BUILD_INDEX);
	ZENITH_ASSERT_EQ((u_int)eWhiteout, (u_int)ZM_SCENE_DAWNMERE,
		"the whiteout destination (build index %u) resolves to %s -- ZM-D-176 "
		"moved the NEW GAME entry point and left whiteout alone, so a fainted "
		"player must still wake in Dawnmere Village",
		ZM_GameStateManager::uWHITEOUT_BUILD_INDEX, ZM_GetSceneName(eWhiteout));

	if (eWhiteout == ZM_SCENE_DAWNMERE)
	{
		ZENITH_ASSERT_TRUE(
			NewGameEntrySceneOffersTag(ZM_GetWorldSpec(eWhiteout),
				ZM_GameStateManager::szWHITEOUT_SPAWN_TAG),
			"Dawnmere no longer offers the whiteout spawn tag '%s'. NOTE, because "
			"this is the trap ZM-D-176 created: after the FrontEnd row was "
			"re-pointed at PlayerHome, that tag has NO inbound connection edge "
			"anywhere in the table -- the whiteout constant is its only remaining "
			"reader, so a tidy-up that deleted it as 'unreferenced' would break "
			"the faint flow and nothing else would say so",
			ZM_GameStateManager::szWHITEOUT_SPAWN_TAG != nullptr
				? ZM_GameStateManager::szWHITEOUT_SPAWN_TAG : "(null)");
	}
}

// ============================================================================
// U3 -- the FrontEnd connection row mirrors the constants.
// ============================================================================

// ★ THE ONLY ASSERTION THAT CAN TELL "FrontEnd -> PlayerHome" FROM "FrontEnd ->
// anywhere in the connected component". WorldSpec_AllReachableFromFrontEnd
// provably cannot: it seeds a DFS at FrontEnd and requires every non-Battle scene
// to be visited, which holds for ANY target inside the component. So it stayed
// green across the ZM-D-176 re-point without noticing it, and would stay green if
// the row were re-pointed again tomorrow.
//
// ★ THE ROW CHANGES NO RUNTIME BEHAVIOUR, and stating that is the point.
// IsWarpDestinationValid reads ZM_FindSceneByBuildIndex plus the target's offered
// tags and NEVER m_pxConnections; the constants alone decide where New Game
// lands. The connection table is the declarative world model that tools and tests
// walk. This unit is what keeps the model honest about the flow -- it is a
// coherence check with teeth, not a behaviour check.
//
// BOTH SIDES READ THE CONSTANTS. Typing 40 / "Door" here would compare a literal
// against a literal and could not red a drift.
ZENITH_TEST(ZM_Data, NewGameEntry_FrontEndRowMirrorsTheNewGameConstants)
{
	const ZM_WorldSpec& xFrontEnd = ZM_GetWorldSpec(ZM_SCENE_FRONTEND);

	ZENITH_ASSERT_EQ(xFrontEnd.m_uConnectionCount, 1u,
		"the title screen declares %u connections; it has exactly one way "
		"forward -- the new-game entry point",
		xFrontEnd.m_uConnectionCount);
	ZENITH_ASSERT_NOT_NULL(xFrontEnd.m_pxConnections,
		"the title screen declares connections but the array pointer is null");

	if (xFrontEnd.m_uConnectionCount != 1u
		|| xFrontEnd.m_pxConnections == nullptr)
	{
		return;   // already reported
	}

	const ZM_SceneConnection& xConn = xFrontEnd.m_pxConnections[0];
	const ZM_SCENE_ID eExpected =
		ZM_FindSceneByBuildIndex(ZM_GameStateManager::uNEW_GAME_BUILD_INDEX);

	ZENITH_ASSERT_EQ((u_int)xConn.m_eTarget, (u_int)eExpected,
		"the title screen's one connection targets %s, but a new run is queued to "
		"%s -- the declarative world model and the flow that actually runs "
		"disagree about where the game begins",
		ZM_GetSceneName(xConn.m_eTarget), ZM_GetSceneName(eExpected));

	ZENITH_ASSERT_NOT_NULL(xConn.m_szSpawnTag,
		"the title screen's connection carries a null spawn tag");
	if (xConn.m_szSpawnTag != nullptr
		&& ZM_GameStateManager::szNEW_GAME_SPAWN_TAG != nullptr)
	{
		ZENITH_ASSERT_STREQ(xConn.m_szSpawnTag,
			ZM_GameStateManager::szNEW_GAME_SPAWN_TAG,
			"the title screen's connection arrives at spawn tag '%s' but a new run "
			"is queued to '%s' -- same scene, different marker",
			xConn.m_szSpawnTag, ZM_GameStateManager::szNEW_GAME_SPAWN_TAG);
	}
}

// ============================================================================
// U4 -- the world's single articulation point.
// ============================================================================

// After ZM-D-176 the ONLY edge out of the entry point is PlayerHome -> Dawnmere.
// Delete it and every scene past the bedroom becomes unreachable: the player
// starts a new game in a room with no declared way out.
//
// WorldSpec_AllReachableFromFrontEnd would red on that -- but it would red with
// "Route 1 is unreachable from FrontEnd", naming a scene four hops away and
// saying nothing about the edge that actually went missing. This unit exists to
// name the cause rather than a symptom, which is the whole reason it is worth its
// own boot slot.
ZENITH_TEST(ZM_Data, NewGameEntry_PlayerHomeConnectsBackToDawnmere)
{
	const ZM_WorldSpec& xPlayerHome = ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME);

	ZENITH_ASSERT_GE(xPlayerHome.m_uConnectionCount, 1u,
		"the new-game entry point declares no outbound connections at all -- a "
		"new run would begin in a room the world model cannot leave, and every "
		"scene beyond it is unreachable");
	ZENITH_ASSERT_NOT_NULL(xPlayerHome.m_pxConnections,
		"PlayerHome declares connections but the array pointer is null");

	bool bFound = false;
	for (u_int uConn = 0u;
		xPlayerHome.m_pxConnections != nullptr
			&& uConn < xPlayerHome.m_uConnectionCount;
		++uConn)
	{
		const ZM_SceneConnection& xConn = xPlayerHome.m_pxConnections[uConn];
		if (xConn.m_eTarget != ZM_SCENE_DAWNMERE)
		{
			continue;
		}
		bFound = true;

		// Referential closure, asserted without re-spelling the tag: whatever
		// string the edge names, Dawnmere must offer it. Re-typing "FromHome"
		// here would compare a literal against itself.
		ZENITH_ASSERT_TRUE(
			NewGameEntrySceneOffersTag(
				ZM_GetWorldSpec(ZM_SCENE_DAWNMERE), xConn.m_szSpawnTag),
			"PlayerHome's exit names spawn tag '%s' in Dawnmere, which that scene "
			"does not offer -- the warp would pass IsWarpDestinationValid and then "
			"park in WAITING_FOR_SPAWN, which has no timeout",
			xConn.m_szSpawnTag != nullptr ? xConn.m_szSpawnTag : "(null)");
	}

	ZENITH_ASSERT_TRUE(bFound,
		"PlayerHome declares no edge back to Dawnmere. Since ZM-D-176 re-pointed "
		"the title at PlayerHome, that edge is the world's SINGLE ARTICULATION "
		"POINT: without it a new run is sealed in the bedroom and every scene "
		"past it is unreachable from FrontEnd");
}
