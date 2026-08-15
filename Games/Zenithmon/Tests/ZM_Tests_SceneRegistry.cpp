#include "Zenith.h"

// ============================================================================
// ZM_Tests_SceneRegistry (S8 item 2, R1-1) -- the boot units that walk the ONE
// enumerable scene-registration table, the SAME rows Project_LoadInitialScene
// walks, so that "the gate trigger shipped, the registration did not" reds at
// boot instead of shipping.
//
// ★ WHY THIS SUITE EXISTS, AND WHAT IT IS ACTUALLY GUARDING.
// ZM_GameStateManager::IsWarpDestinationValid consults ONLY ZM_SpawnPoint::
// IsTagValid, ZM_FindSceneByBuildIndex and the target row's offered spawn-tag
// list. It never looks at the scene registry and never looks inside the
// destination scene -- so a warp aimed at a build index NOTHING REGISTERED is
// ACCEPTED, and the machine then parks in ZM_WARP_TRANSITION_WAITING_FOR_SCENE
// or _WAITING_FOR_SPAWN, NEITHER OF WHICH HAS A TIMEOUT. Every failure path in
// PollForSpawnAndPlacePlayer is a bare return with no counter and no error
// state. The symptom is a permanent black screen behind a fully opaque fade
// with the player frozen: not a crash, not an exception, and -- until this
// file -- not a red test either.
//
// Registration used to be five hand-written RegisterSceneBuildIndex calls with
// no compiled table behind them, which is precisely why no unit could see it.
// Source/World/ZM_SceneRegistry.h is that table, and U4 below is the unit that
// makes the hole unreachable: it takes every warp target the compiled placement
// headers NAME and requires that target to be registered for load.
//
// PURE: no scene, no entity, no physics, no assets, no graphics, no g_xEngine.
// Everything here reads COMPILED CONSTANTS -- the registration table, the world
// table, and the placement headers' resolvers -- plus the pure static
// ZM_GameStateManager::IsWarpDestinationValid, which needs no live manager.
//
// ★ AND NOTHING HERE TOUCHES THE LIVE SCENE REGISTRY. That is a hard
// constraint, not a style note. Three existing boot units call
// g_xEngine.Scenes().RegisterSceneBuildIndex(2, "<fake path>")
// (ZM_Tests_WorldTraversal.cpp), and RegisterSceneBuildIndex ASSERTS on a
// re-registration with a DIFFERENT path -- Zenith_SceneSystem::ResetForNextTest
// is the only thing that keeps that safe. These units read the COMPILED TABLE
// and never the live one, so they cannot participate in that hazard.
//
// ★ AND THESE UNITS CREATE NO ENTITY. Scene authoring bakes in the entity
// indices assigned during the boot it runs in, and the boot-time unit suite
// allocates entities FIRST -- so one entity-creating boot unit re-authors
// different .zscen bytes and invalidates the two-boot byte-stability property
// ZM-D-148 established.
//
// ★ WHAT THESE UNITS CANNOT DO. They run BEFORE the initial scene loads, so
// they see neither the live registry nor one byte of any .zscen. They prove
// that the TABLE covers every gate target; they cannot prove the file on disk
// exists, and for Route1 / Thornacre it deliberately does not yet (the row
// lands before the scene, because a registration without a scene is inert while
// a scene without a registration is fatal). Do not read this file's greenness
// as "Route 1 is reachable".
// ============================================================================

#include <cstring>   // strcmp / strchr -- the stem-identity and stem-shape claims

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"          // the SHIPPED warp validator, run rather than re-derived
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"        // the already-shipped seam, closed for free
#include "Zenithmon/Source/World/ZM_Route1Placement.h"
#include "Zenithmon/Source/World/ZM_SceneRegistry.h"
#include "Zenithmon/Source/World/ZM_ThornacrePlacement.h"

namespace
{
	// "(null)" rather than a crash inside vsnprintf. Every message below that
	// prints a string a clause has just found suspect goes through this.
	const char* SceneRegistrySafe(const char* szText)
	{
		return szText != nullptr ? szText : "(null)";
	}

	// Is this string a FILE STEM -- the thing Project_LoadInitialScene
	// concatenates as GAME_ASSETS_DIR "Scenes/" <stem> ZENITH_SCENE_EXT?
	//
	// A stem carries no directory and no extension: a stem that gained ".zscen"
	// produces "Scenes/Route1.zscen.zscen", and a stem carrying a separator
	// escapes the Scenes directory entirely. Both register a path that no scene
	// file will ever be found at, and BOTH ARE SILENT -- the registration
	// succeeds, and the failure only appears as a load that never completes,
	// behind a fade with no timeout.
	bool SceneRegistryStemIsAFileStem(const char* szStem)
	{
		if (szStem == nullptr || szStem[0] == '\0')
		{
			return false;
		}
		for (u_int uIndex = 0u; szStem[uIndex] != '\0'; ++uIndex)
		{
			const unsigned char uCharacter =
				static_cast<unsigned char>(szStem[uIndex]);
			if (uCharacter < 0x20u || uCharacter > 0x7eu)
			{
				return false;
			}
		}
		return std::strchr(szStem, '/') == nullptr
			&& std::strchr(szStem, '\\') == nullptr
			&& std::strchr(szStem, '.') == nullptr;
	}

	// Does a scene row OFFER this spawn tag? The referential-closure helper U4
	// shares with the shipped validator's own inner loop, so "offers" means one
	// thing in this file.
	bool SceneRegistrySceneOffersTag(const ZM_WorldSpec& xSpec, const char* szTag)
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

	// ------------------------------------------------------------------------
	// U2's CLAIM, spelled as literals on this side of the comparison.
	//
	// ★ THIS IS NOT A RE-SPELLING OF THE SUBJECT, and the distinction is the
	// whole reason the unit is worth writing. The table in ZM_SceneRegistry.h is
	// the SUBJECT; this array is the CLAIM about it, in the same idiom as
	// ZM_Tests_StoryFlags' axEXPECTED_WIRE_BITS. A row silently added, removed,
	// reordered or re-stemmed moves the subject and leaves the claim behind.
	// ------------------------------------------------------------------------
	struct SceneRegistryExpectedRow
	{
		ZM_SCENE_ID	m_eScene;
		const char*	m_szFileStem;
	};

	constexpr SceneRegistryExpectedRow axSCENE_REGISTRY_EXPECTED[] =
	{
		{ ZM_SCENE_FRONTEND,   "FrontEnd"   },
		{ ZM_SCENE_BATTLE,     "Battle"     },
		{ ZM_SCENE_DAWNMERE,   "Dawnmere"   },
		{ ZM_SCENE_THORNACRE,  "Thornacre"  },
		{ ZM_SCENE_ROUTE1,     "Route1"     },
		{ ZM_SCENE_PLAYERHOME, "PlayerHome" },
		{ ZM_SCENE_PROFLAB,    "ProfLab"    },
	};

	constexpr u_int uSCENE_REGISTRY_EXPECTED_COUNT =
		(u_int)(sizeof(axSCENE_REGISTRY_EXPECTED)
			/ sizeof(axSCENE_REGISTRY_EXPECTED[0]));

	// ...and the count as its OWN literal, so growing the claim array to match a
	// grown subject is still a third deliberate edit rather than a tidy-up.
	constexpr u_int uSCENE_REGISTRY_FILE_BACKED_SCENES = 7u;

	// ------------------------------------------------------------------------
	// U4's subjects: every warp target the compiled placement headers NAME.
	// ------------------------------------------------------------------------
	struct SceneRegistryGateTarget
	{
		const char*	m_szLabel;             // which authored sensor this is
		ZM_SCENE_ID	m_eTarget;             // where the compiled table says it goes
		u_int		m_uTargetBuildIndex;   // ...and under which build index
		const char*	m_szSpawnTag;          // ...asking for which arrival marker
	};
}

// ============================================================================
// U1 -- every row is a distinct, resolvable, registerable row.
// ============================================================================

// The three mutations this catches, none of which any other unit can see:
//
//   * a DUPLICATED row -- RegisterSceneBuildIndex fires its "already registered
//     for '%s'" assert at boot the moment the loop reaches the second copy with
//     a different path, i.e. a hard boot failure rather than a test failure;
//   * a stem that gained ".zscen" or a directory -- registers
//     "Scenes/Route1.zscen.zscen", which loads nothing, forever, behind a fade
//     with no timeout;
//   * a re-pointed build index -- the row registers one scene's path under
//     another scene's index, so a warp resolves to the wrong world.
ZENITH_TEST(ZM_SceneRegistry, Registry_EveryRowResolvesToADistinctWorldRowAndBuildIndex)
{
	const u_int uCount = ZM_GetSceneRegistrationCount();

	// ANTI-VACUITY. Empty the table and every walk in this file passes by never
	// iterating -- while the game registers nothing and cannot load its title.
	ZENITH_ASSERT_GT(uCount, 0u,
		"the scene-registration table is empty; Project_LoadInitialScene walks "
		"it, so an empty table registers no scene at all and every clause in "
		"this suite passes vacuously");

	for (u_int uRow = 0u; uRow < uCount; ++uRow)
	{
		const ZM_SceneRegistration& xRow = ZM_GetSceneRegistration(uRow);

		// The row must name a REAL world-table row. Anything past the end would
		// also make ZM_GetSceneRegistrationBuildIndex hand back its unresolved
		// sentinel, which the registration loop SKIPS -- a scene silently left
		// unregistered.
		ZENITH_ASSERT_LT((u_int)xRow.m_eScene, (u_int)ZM_SCENE_COUNT,
			"registration row %u names scene id %u, which is past the end of the "
			"compiled world table", uRow, (u_int)xRow.m_eScene);
		if (xRow.m_eScene >= ZM_SCENE_COUNT)
		{
			continue;   // already reported; every clause below would read nonsense
		}

		ZENITH_ASSERT_NOT_NULL(xRow.m_szFileStem,
			"registration row %u (%s) carries a null file stem",
			uRow, ZM_GetSceneName(xRow.m_eScene));
		ZENITH_ASSERT_TRUE(SceneRegistryStemIsAFileStem(xRow.m_szFileStem),
			"registration row %u (%s) carries file stem '%s', which is not a "
			"STEM -- it must be printable, non-empty, and carry no '/', no '\\' "
			"and no '.' (the directory and the extension are the loop's "
			"business: GAME_ASSETS_DIR \"Scenes/\" <stem> ZENITH_SCENE_EXT)",
			uRow, ZM_GetSceneName(xRow.m_eScene),
			SceneRegistrySafe(xRow.m_szFileStem));

		// The build index the loop will actually register this row under.
		const u_int uBuildIndex = ZM_GetSceneRegistrationBuildIndex(xRow);
		ZENITH_ASSERT_NE(uBuildIndex, uZM_SCENE_REGISTRATION_BUILD_INDEX_UNRESOLVED,
			"registration row %u (%s) resolves to the unresolved-build-index "
			"sentinel; Project_LoadInitialScene SKIPS such a row, so this scene "
			"would never be registered and any warp to it would be ACCEPTED and "
			"then park forever in WAITING_FOR_SCENE",
			uRow, ZM_GetSceneName(xRow.m_eScene));

		// ...and it round-trips. An index that does not resolve back to this row
		// means the registration and the warp machine disagree about which file
		// backs which scene.
		ZENITH_ASSERT_EQ(
			(u_int)ZM_FindSceneByBuildIndex(uBuildIndex),
			(u_int)xRow.m_eScene,
			"registration row %u registers '%s' under build index %u, which "
			"resolves back to %s rather than to %s",
			uRow, SceneRegistrySafe(xRow.m_szFileStem), uBuildIndex,
			ZM_GetSceneName(ZM_FindSceneByBuildIndex(uBuildIndex)),
			ZM_GetSceneName(xRow.m_eScene));

		// Pairwise distinctness, on all three columns. Two rows sharing an index
		// is the boot-assert case; two rows sharing a stem means two scenes read
		// the same file; two rows sharing a scene id is a duplicate row that the
		// count-based clauses in U2 would still let through if the stems differ.
		for (u_int uOther = uRow + 1u; uOther < uCount; ++uOther)
		{
			const ZM_SceneRegistration& xOther = ZM_GetSceneRegistration(uOther);
			if (xOther.m_eScene >= ZM_SCENE_COUNT
				|| xOther.m_szFileStem == nullptr
				|| xRow.m_szFileStem == nullptr)
			{
				continue;   // reported by that row's own clauses
			}

			ZENITH_ASSERT_NE((u_int)xRow.m_eScene, (u_int)xOther.m_eScene,
				"registration rows %u and %u both name scene %s",
				uRow, uOther, ZM_GetSceneName(xRow.m_eScene));
			ZENITH_ASSERT_NE(
				std::strcmp(xRow.m_szFileStem, xOther.m_szFileStem), 0,
				"registration rows %u (%s) and %u (%s) share file stem '%s' -- "
				"two scenes would load the same file",
				uRow, ZM_GetSceneName(xRow.m_eScene),
				uOther, ZM_GetSceneName(xOther.m_eScene),
				SceneRegistrySafe(xRow.m_szFileStem));
			ZENITH_ASSERT_NE(
				uBuildIndex, ZM_GetSceneRegistrationBuildIndex(xOther),
				"registration rows %u (%s) and %u (%s) share build index %u -- "
				"RegisterSceneBuildIndex asserts on the second one at boot",
				uRow, ZM_GetSceneName(xRow.m_eScene),
				uOther, ZM_GetSceneName(xOther.m_eScene), uBuildIndex);
		}
	}
}

// ============================================================================
// U2 -- the table's exact contents, and the gym's deliberate absence.
// ============================================================================

// ★ THE GYM IS OUT OF S8 ITEM 2'S SCOPE, AND ITS ABSENCE IS ASSERTED RATHER
// THAN ASSUMED. Thornacre's compiled row already carries a Thornacre -> Gym1
// ("Door") edge, so IsWarpDestinationValid ACCEPTS a warp into the gym today
// (U4's independence arm runs exactly that call and proves it). Nothing backs
// it: no scene, and -- by this clause -- no registration either. Adding the row
// has to be a conscious act, which is what this unit costs it.
ZENITH_TEST(ZM_SceneRegistry, Registry_CoversEveryFileBackedSceneAndDeliberatelyOmitsTheGym)
{
	// The claim array's own size is pinned first, so growing BOTH the subject
	// and the claim in one edit still leaves this literal behind.
	ZENITH_ASSERT_EQ(uSCENE_REGISTRY_EXPECTED_COUNT,
		uSCENE_REGISTRY_FILE_BACKED_SCENES,
		"this unit's expected-row array holds %u rows but claims %u file-backed "
		"scenes -- if the world really gained a scene file, move both, "
		"deliberately", uSCENE_REGISTRY_EXPECTED_COUNT,
		uSCENE_REGISTRY_FILE_BACKED_SCENES);

	ZENITH_ASSERT_EQ(ZM_GetSceneRegistrationCount(),
		uSCENE_REGISTRY_EXPECTED_COUNT,
		"the scene-registration table holds %u rows; R1-1 ships %u (the five "
		"scenes with committed .zscen files, plus Thornacre and Route1 whose "
		"rows land BEFORE their files on purpose)",
		ZM_GetSceneRegistrationCount(), uSCENE_REGISTRY_EXPECTED_COUNT);

	if (ZM_GetSceneRegistrationCount() != uSCENE_REGISTRY_EXPECTED_COUNT)
	{
		return;   // already reported; a row-by-row walk would report the same drift again
	}

	for (u_int uRow = 0u; uRow < uSCENE_REGISTRY_EXPECTED_COUNT; ++uRow)
	{
		const ZM_SceneRegistration& xRow = ZM_GetSceneRegistration(uRow);
		const SceneRegistryExpectedRow& xExpected = axSCENE_REGISTRY_EXPECTED[uRow];

		ZENITH_ASSERT_EQ((u_int)xRow.m_eScene, (u_int)xExpected.m_eScene,
			"registration row %u names %s; R1-1's table is in ascending scene-id "
			"order and this slot belongs to %s -- a reorder rewrites nothing but "
			"the boot log, so it is caught here or not at all",
			uRow, ZM_GetSceneName(xRow.m_eScene),
			ZM_GetSceneName(xExpected.m_eScene));

		ZENITH_ASSERT_NOT_NULL(xRow.m_szFileStem,
			"registration row %u carries a null file stem", uRow);
		if (xRow.m_szFileStem != nullptr)
		{
			ZENITH_ASSERT_STREQ(xRow.m_szFileStem, xExpected.m_szFileStem,
				"registration row %u (%s) carries file stem '%s' rather than "
				"'%s' -- the stem IS the file name under Assets/Scenes/, so a "
				"re-stem registers a path nothing will ever be found at",
				uRow, ZM_GetSceneName(xRow.m_eScene),
				SceneRegistrySafe(xRow.m_szFileStem), xExpected.m_szFileStem);
		}

		// ...and the lookup by scene id agrees with the walk by index. The two
		// accessors are what the loop and the units respectively use, so a
		// divergence between them is a divergence between subject and claim.
		ZENITH_ASSERT_TRUE(ZM_IsSceneRegisteredForLoad(xExpected.m_eScene),
			"%s is not registered for load, though it is row %u of the table",
			ZM_GetSceneName(xExpected.m_eScene), uRow);
		ZENITH_ASSERT_STREQ(
			ZM_FindSceneFileStem(xExpected.m_eScene), xExpected.m_szFileStem,
			"ZM_FindSceneFileStem(%s) answers '%s' rather than '%s' -- the "
			"by-id lookup and the by-index walk disagree",
			ZM_GetSceneName(xExpected.m_eScene),
			SceneRegistrySafe(ZM_FindSceneFileStem(xExpected.m_eScene)),
			xExpected.m_szFileStem);
	}

	// The gym, absent on purpose.
	ZENITH_ASSERT_NOT_NULL(ZM_FindSceneFileStem(ZM_SCENE_GYM1),
		"ZM_FindSceneFileStem returns null rather than \"\" for an unregistered "
		"scene; every caller indexes [0] without checking");
	ZENITH_ASSERT_EQ(ZM_FindSceneFileStem(ZM_SCENE_GYM1)[0], '\0',
		"%s carries file stem '%s' -- the gym is OUT of S8 item 2's scope and "
		"has no scene file; adding its row must be a conscious act, which is "
		"what this clause costs it",
		ZM_GetSceneName(ZM_SCENE_GYM1),
		SceneRegistrySafe(ZM_FindSceneFileStem(ZM_SCENE_GYM1)));
	ZENITH_ASSERT_FALSE(ZM_IsSceneRegisteredForLoad(ZM_SCENE_GYM1),
		"%s reports as registered for load; nothing in S8 item 2 authors a gym, "
		"so a registration here points at a file that does not exist",
		ZM_GetSceneName(ZM_SCENE_GYM1));
}

// ============================================================================
// U3 -- the TOTAL accessors really are total, and fail CLOSED.
// ============================================================================

// ★ THE HAZARD THIS UNIT IS ABOUT IS NOT A WRONG ANSWER, IT IS A DEAD RUN.
// ZM_GetWorldSpec ASSERTS FATALLY on an out-of-range id and Zenith_Assert calls
// Zenith_DebugBreak() in EVERY configuration -- so an accessor that passed
// ZM_SCENE_NONE straight through would not fail one test, it would END the
// whole boot-unit run and take the gate down with it. Every accessor in
// ZM_SceneRegistry.h guards before calling it; this unit is what proves the
// guards are still there, by feeding each one the degenerate input it was
// written for.
ZENITH_TEST(ZM_SceneRegistry, Registry_TotalAccessorsAreFailClosedOnGarbage)
{
	// One past the end, and the far end of the u_int range.
	const ZM_SceneRegistration& xPastEnd =
		ZM_GetSceneRegistration(uZM_SCENE_REGISTRATION_COUNT);
	ZENITH_ASSERT_EQ((u_int)xPastEnd.m_eScene, (u_int)ZM_SCENE_NONE,
		"ZM_GetSceneRegistration(count) answers scene %s rather than the "
		"invalid registration -- an out-of-range walk would read a real row",
		ZM_GetSceneName(xPastEnd.m_eScene));
	ZENITH_ASSERT_NOT_NULL(xPastEnd.m_szFileStem,
		"the invalid registration carries a null stem rather than \"\"");
	ZENITH_ASSERT_EQ(xPastEnd.m_szFileStem[0], '\0',
		"the invalid registration carries stem '%s' rather than \"\"",
		SceneRegistrySafe(xPastEnd.m_szFileStem));

	const ZM_SceneRegistration& xGarbage = ZM_GetSceneRegistration(0xFFFFFFFFu);
	ZENITH_ASSERT_EQ((u_int)xGarbage.m_eScene, (u_int)ZM_SCENE_NONE,
		"ZM_GetSceneRegistration(0xFFFFFFFF) answers scene %s rather than the "
		"invalid registration", ZM_GetSceneName(xGarbage.m_eScene));

	// ★ THE CLAUSE THAT KEEPS ZM_SCENE_NONE OUT OF ZM_GetWorldSpec. The
	// registration loop passes whatever this returns to
	// RegisterSceneBuildIndex after a static_cast<int>, and -1 trips that API's
	// own "build index must be non-negative" assert -- which is why the loop
	// skips the sentinel rather than registering it.
	ZENITH_ASSERT_EQ(
		ZM_GetSceneRegistrationBuildIndex(xZM_INVALID_SCENE_REGISTRATION),
		uZM_SCENE_REGISTRATION_BUILD_INDEX_UNRESOLVED,
		"ZM_GetSceneRegistrationBuildIndex answers %u for the invalid "
		"registration; it must answer the unresolved sentinel WITHOUT reaching "
		"ZM_GetWorldSpec, which asserts fatally on ZM_SCENE_NONE and would end "
		"the entire boot-unit run",
		ZM_GetSceneRegistrationBuildIndex(xZM_INVALID_SCENE_REGISTRATION));

	// The by-id lookups, on the sentinel scene.
	ZENITH_ASSERT_NOT_NULL(ZM_FindSceneFileStem(ZM_SCENE_NONE),
		"ZM_FindSceneFileStem(ZM_SCENE_NONE) returns null rather than \"\"");
	ZENITH_ASSERT_EQ(ZM_FindSceneFileStem(ZM_SCENE_NONE)[0], '\0',
		"ZM_FindSceneFileStem(ZM_SCENE_NONE) answers '%s'",
		SceneRegistrySafe(ZM_FindSceneFileStem(ZM_SCENE_NONE)));
	ZENITH_ASSERT_FALSE(ZM_IsSceneRegisteredForLoad(ZM_SCENE_NONE),
		"ZM_IsSceneRegisteredForLoad(ZM_SCENE_NONE) is true -- the 'no scene' "
		"sentinel would report as loadable");

	// The by-build-index lookup, on an index no scene holds and on the gym's.
	ZENITH_ASSERT_FALSE(ZM_IsBuildIndexRegisteredForLoad(0xFFFFFFFFu),
		"ZM_IsBuildIndexRegisteredForLoad(0xFFFFFFFF) is true -- an index the "
		"world table cannot resolve reports as loadable");
	ZENITH_ASSERT_FALSE(
		ZM_IsBuildIndexRegisteredForLoad(
			ZM_GetWorldSpec(ZM_SCENE_GYM1).m_uBuildIndex),
		"the gym's build index %u reports as registered for load; nothing in "
		"S8 item 2 authors a gym scene",
		ZM_GetWorldSpec(ZM_SCENE_GYM1).m_uBuildIndex);

	// ★ ANTI-VACUITY, and it is not decoration here: every clause above asserts
	// a NEGATIVE, so a predicate hard-wired to false would satisfy all of them.
	// One positive control through the identical accessor is what proves the
	// answers above are being computed rather than defaulted.
	ZENITH_ASSERT_TRUE(
		ZM_IsBuildIndexRegisteredForLoad(
			ZM_GetWorldSpec(ZM_SCENE_FRONTEND).m_uBuildIndex),
		"FrontEnd's build index %u does not report as registered for load, so "
		"the fail-closed clauses above prove nothing -- and the title screen, "
		"which Project_LoadInitialScene loads by that very index, has no "
		"registered path",
		ZM_GetWorldSpec(ZM_SCENE_FRONTEND).m_uBuildIndex);
}

// ============================================================================
// U4 -- every warp target a placement header NAMES is registered for load.
// ============================================================================

// ★ THIS IS THE UNIT THE WHOLE SLICE EXISTS FOR. Author a gate trigger, forget
// the registration, and NOTHING ELSE IN THE TREE NOTICES: IsWarpDestinationValid
// accepts the destination (it consults only the compiled world table), the warp
// is queued, the fade goes fully opaque, and the machine parks in
// WAITING_FOR_SCENE / WAITING_FOR_SPAWN -- neither of which has a timeout, and
// every failure path in PollForSpawnAndPlacePlayer is a bare return. The player
// sees a black screen forever.
//
// So this unit walks the SAME compiled table Project_LoadInitialScene walks, and
// requires it to cover every target the placement headers point a sensor at --
// including ProfLab's already-shipped exit, which closes that seam for free.
//
// ★ AND ITS ANTI-VACUITY ARM IS AN INDEPENDENCE PROOF, NOT A SMOKE TEST. It
// asserts that the gym's (index, tag) pair is ACCEPTED by the shipped validator
// WHILE the gym is NOT registered for load. That pair of facts is the hole,
// demonstrated live: it proves this unit measures something
// IsWarpDestinationValid structurally cannot see, and therefore that these
// clauses are not merely re-running the validator under another name.
ZENITH_TEST(ZM_SceneRegistry, Registry_EveryPlacementGateTargetIsRegisteredForLoad)
{
	const SceneRegistryGateTarget axGates[] =
	{
		{ "Route 1 south gate (back to Dawnmere)",
			ZM_GetRoute1SouthGateTargetScene(),
			ZM_GetRoute1SouthGateTargetBuildIndex(),
			ZM_GetRoute1SouthGateSpawnTag() },
		{ "Route 1 north gate (on to Thornacre)",
			ZM_GetRoute1NorthGateTargetScene(),
			ZM_GetRoute1NorthGateTargetBuildIndex(),
			ZM_GetRoute1NorthGateSpawnTag() },
		{ "Thornacre return gate (back to Route 1)",
			ZM_GetThornacreReturnTargetScene(),
			ZM_GetThornacreReturnTargetBuildIndex(),
			ZM_GetThornacreReturnSpawnTag() },
		{ "ProfLab exit sensor (back to Dawnmere)",
			ZM_FindSceneByBuildIndex(ZM_GetProfLabExitTargetBuildIndex()),
			ZM_GetProfLabExitTargetBuildIndex(),
			ZM_GetProfLabExitSpawnTag() },
	};
	const u_int uGateCount = (u_int)(sizeof(axGates) / sizeof(axGates[0]));

	// ANTI-VACUITY on the walk itself.
	ZENITH_ASSERT_GT(uGateCount, 0u,
		"no placement gate targets to check -- every clause below would pass by "
		"never iterating, which is exactly the silence this unit exists to break");

	for (u_int uGate = 0u; uGate < uGateCount; ++uGate)
	{
		const SceneRegistryGateTarget& xGate = axGates[uGate];

		// (1) The resolver found an edge at all. A gate whose target is
		//     ZM_SCENE_NONE would be authored with the unresolved sentinel as
		//     its build index -- an obviously dead warp, by design, but only if
		//     somebody is looking.
		ZENITH_ASSERT_NE((u_int)xGate.m_eTarget, (u_int)ZM_SCENE_NONE,
			"%s resolves to no scene at all; the compiled world table carries "
			"no such edge, so the authored trigger would take the unresolved "
			"build-index sentinel", xGate.m_szLabel);
		if (xGate.m_eTarget >= ZM_SCENE_COUNT)
		{
			continue;   // reported; ZM_GetWorldSpec would assert fatally below
		}

		const ZM_WorldSpec& xTarget = ZM_GetWorldSpec(xGate.m_eTarget);

		// (2) The build index the trigger will carry is the target's own.
		ZENITH_ASSERT_EQ(xGate.m_uTargetBuildIndex, xTarget.m_uBuildIndex,
			"%s carries build index %u while %s lives at build index %u",
			xGate.m_szLabel, xGate.m_uTargetBuildIndex,
			ZM_GetSceneName(xGate.m_eTarget), xTarget.m_uBuildIndex);

		// (3) ★ THE CLAIM NOTHING ELSE IN THE GAME MAKES: the destination has a
		//     scene file registered under that index. Delete the Route1 row from
		//     ZM_SceneRegistry.h and this is the clause that reds.
		ZENITH_ASSERT_TRUE(ZM_IsSceneRegisteredForLoad(xGate.m_eTarget),
			"%s targets %s, which has NO registered scene file -- the warp "
			"would be ACCEPTED by IsWarpDestinationValid (which never consults "
			"the registry) and would then park in WAITING_FOR_SCENE, which has "
			"no timeout: a permanent black screen behind an opaque fade, with "
			"the player frozen. Add the row to ZM_SceneRegistry.h",
			xGate.m_szLabel, ZM_GetSceneName(xGate.m_eTarget));

		// ...and by build index too, which is the spelling the runtime holds.
		ZENITH_ASSERT_TRUE(
			ZM_IsBuildIndexRegisteredForLoad(xGate.m_uTargetBuildIndex),
			"%s holds build index %u, which is not registered for load",
			xGate.m_szLabel, xGate.m_uTargetBuildIndex);

		// (4) The tag half of the seam. A gate that asked for a tag its target
		//     does not offer passes nothing -- or worse, passes validation and
		//     parks in WAITING_FOR_SPAWN, the other timeout-free barrier.
		ZENITH_ASSERT_NOT_NULL(xGate.m_szSpawnTag,
			"%s resolves a null spawn tag; the accessors answer \"\" on a miss, "
			"never null, so this is a contract break rather than a miss",
			xGate.m_szLabel);
		ZENITH_ASSERT_TRUE(
			xGate.m_szSpawnTag != nullptr && xGate.m_szSpawnTag[0] != '\0',
			"%s resolves an EMPTY spawn tag -- the documented answer when the "
			"compiled table carries no such edge", xGate.m_szLabel);
		ZENITH_ASSERT_TRUE(
			SceneRegistrySceneOffersTag(xTarget, xGate.m_szSpawnTag),
			"%s asks %s for spawn tag '%s', which that row does not offer -- "
			"the warp would pass validation and then wait forever for a marker "
			"no scene authors",
			xGate.m_szLabel, ZM_GetSceneName(xGate.m_eTarget),
			SceneRegistrySafe(xGate.m_szSpawnTag));

		// (5) ...and the SHIPPED validator agrees. Running the real predicate
		//     rather than re-deriving it is what stops this unit passing while
		//     the runtime refuses the pair for a reason this file did not model.
		ZENITH_ASSERT_TRUE(
			ZM_GameStateManager::IsWarpDestinationValid(
				xGate.m_uTargetBuildIndex, xGate.m_szSpawnTag),
			"IsWarpDestinationValid rejects %s's pair (%u, '%s'); every warp "
			"queues through that validator, so the authored trigger would be "
			"refused outright", xGate.m_szLabel, xGate.m_uTargetBuildIndex,
			SceneRegistrySafe(xGate.m_szSpawnTag));
	}

	// ------------------------------------------------------------------------
	// ★ THE INDEPENDENCE ARM. Thornacre's compiled row carries a Gym1 edge, and
	// the gym row offers the tag that edge names -- so the shipped validator
	// ACCEPTS the pair. Nothing registers a gym scene. Both halves are asserted
	// here, together, because it is their COEXISTENCE that proves clause (3)
	// above measures something IsWarpDestinationValid cannot see.
	//
	// The tag is READ from the gym's own offered list rather than typed, so this
	// arm cannot drift into comparing a literal with itself.
	// ------------------------------------------------------------------------
	const ZM_WorldSpec& xGym = ZM_GetWorldSpec(ZM_SCENE_GYM1);
	ZENITH_ASSERT_GT(xGym.m_uSpawnTagCount, 0u,
		"the gym row offers no spawn tag, so this independence arm cannot be "
		"built and clause (3)'s falsifiability is no longer demonstrated");
	ZENITH_ASSERT_NOT_NULL(xGym.m_pszSpawnTags,
		"the gym row declares spawn tags but the array pointer is null");

	if (xGym.m_uSpawnTagCount > 0u && xGym.m_pszSpawnTags != nullptr
		&& xGym.m_pszSpawnTags[0] != nullptr)
	{
		ZENITH_ASSERT_TRUE(
			ZM_GameStateManager::IsWarpDestinationValid(
				xGym.m_uBuildIndex, xGym.m_pszSpawnTags[0]),
			"IsWarpDestinationValid REJECTS the gym pair (%u, '%s'), so the "
			"hole this suite guards no longer exists in the shape described -- "
			"re-derive the argument before weakening any clause above",
			xGym.m_uBuildIndex, SceneRegistrySafe(xGym.m_pszSpawnTags[0]));

		ZENITH_ASSERT_FALSE(ZM_IsSceneRegisteredForLoad(ZM_SCENE_GYM1),
			"the gym IS registered for load, so the two claims above are no "
			"longer independent and this arm proves nothing; if a gym scene has "
			"genuinely shipped, replace this arm with another unbacked target");
	}
}
