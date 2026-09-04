#include "Zenith.h"

// ============================================================================
// ZM_Tests_Gym1Placement (S8, Gym 1 -- slice G1-1, ZM-D-209) -- the boot units for
// the PURE half of Gym 1: the hedge-maze placement header, and the three data rows
// that ship with it (Fenna's roster entry, the Bloom Badge index and name, and the
// Verdant Lash TM + move).
//
// ★ THE FILE NAME IS NARROWER THAN ITS CONTENTS, DELIBERATELY AND WITH THE COST
// STATED. Slice G1-1's whole claim is that four separate tables AGREE about Gym 1 --
// a placement header, a trainer row, a badge index and a teach-move -- and the unit
// that can red on a disagreement between them cannot live inside any one of the four
// suites those tables already have. It lives here, with the maze, because the maze
// is the other half of the same slice. If a later stage grows this file, split the
// data half out rather than renaming the maze half.
//
// PURE: no scene, no entity, no physics, no assets, no graphics, no g_xEngine.
// Everything reads COMPILED CONSTANTS -- ZM_Gym1Placement.h, ZM_WorldSpec,
// ZM_TrainerData, ZM_BadgeData, ZM_ItemData, ZM_MoveData, ZM_SpeciesData, and the
// pure statics of ZM_FollowCamera / ZM_SpawnPoint (nothing is constructed). The one
// object built anywhere below is a by-value ZM_GameState, which is what
// ZM_Tests_SaveModel already does in a boot unit.
//
// ★ WHAT THESE UNITS CANNOT DO, STATED UP FRONT. They run BEFORE the initial scene
// loads, so they can see NEITHER the scene registry NOR one byte of any .zscen.
// There IS no Gym1.zscen yet -- G1-1 authors nothing, by ruling (ZM-D-209), and
// G1-2 (ZM-69) creates the scene from this header afterwards. Do not read this
// file's greenness as "Gym 1 exists" or "Gym 1 is reachable"; read it as "the room
// G1-2 is about to build is solvable, and the reward it pays out agrees with
// itself".
//
// ★ AND THESE UNITS CREATE NO ENTITY. That is a hard constraint, not a style note:
// scene authoring bakes in the entity indices assigned during the boot it runs in,
// and the boot-time unit suite allocates entities FIRST -- so one entity-creating
// boot unit re-authors different .zscen bytes and invalidates the identical-SHA256
// two-boot proof G1-2 will owe.
// ============================================================================

#include <cmath>     // fabs / isfinite
#include <cstring>   // strcmp -- the name-distinctness and mirror claims

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"                    // the totality proof
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"              // the camera's PURE statics (nothing is constructed)
#include "Zenithmon/Components/ZM_SpawnPoint.h"                // IsTagValid / uTAG_CAPACITY -- the tag contract
#include "Zenithmon/Source/Data/ZM_BadgeData.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"
#include "Zenithmon/Source/Data/ZM_TrainerData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"  // the SHIPPED ZM_ForwardFromRotation -- run, not re-derived
#include "Zenithmon/Source/Party/ZM_GameState.h"               // the EXISTING badge mask this slice wires an index to
#include "Zenithmon/Source/World/ZM_Gym1Placement.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"               // THE body contract -- by name, not by literal

namespace
{
	// "These two floats are the same authored number." Generous by float standards
	// and far below any placement quantity Gym 1 uses.
	constexpr float fGYM1_EXACT_EPSILON = 1.0e-4f;

	// The clear gap the doorway must leave BESIDE the player's shoulders, per side.
	// Small on purpose: this is a "the aperture has not been walled up" floor, not a
	// comfort target.
	constexpr float fGYM1_APERTURE_SIDE_MARGIN = 0.25f;

	// ...and the clear gap above the player's head under the lintel.
	constexpr float fGYM1_APERTURE_HEAD_MARGIN = 0.20f;

	// A closed room needs a floor plus four bounding sides at minimum. Asserted so
	// the [0, ZM_GYM1_BLOCK_COUNT) walks below cannot pass vacuously on an empty or
	// half-deleted table.
	constexpr u_int uGYM1_MIN_BLOCK_COUNT = 5u;

	// A serpentine needs at least two bands to be a maze rather than a doorway. Same
	// anti-vacuity role for the [0, ZM_GYM1_HEDGE_COUNT) walks.
	constexpr u_int uGYM1_MIN_HEDGE_COUNT = 2u;

	// The badge roster, spelled in the TEST rather than read back off the table, so
	// "the region gained or lost a badge" is a failure rather than a tautology.
	// Docs/GameDesignDocument.md section 3.4 lists exactly eight (GDD 194-201).
	constexpr u_int uGYM1_EXPECTED_BADGES = 8u;

	// ------------------------------------------------------------------------
	// THE LATTICE THE SOLVABILITY PROOF WALKS.
	//
	// It lives HERE and not in ZM_Gym1Placement.h on that header's own rule: an
	// unread constant is not a check, it is a number that looks like one, and the
	// only reader of a grid is this search. The header owns the GEOMETRY and the
	// walkability predicate; this file owns the lattice and the search.
	//
	// Every figure is DERIVED from the header, so a room resize moves the grid with
	// it rather than silently sampling outside the walls.
	// ------------------------------------------------------------------------
	constexpr float fGYM1_GRID_STEP = 0.5f;

	constexpr u_int uGYM1_GRID_COLS =
		(u_int)((fZM_GYM1_INNER_MAX_X - fZM_GYM1_INNER_MIN_X) / fGYM1_GRID_STEP);
	constexpr u_int uGYM1_GRID_ROWS =
		(u_int)((fZM_GYM1_INNER_MAX_Z - fZM_GYM1_INNER_MIN_Z) / fGYM1_GRID_STEP);
	constexpr u_int uGYM1_GRID_CELLS = uGYM1_GRID_COLS * uGYM1_GRID_ROWS;

	// Centred on the room, so x = 0 and z = 0 are lattice lines. The arrival marker,
	// the leader's station and every hedge centreline sit on x = 0 or on a whole
	// multiple of the step, which is what lets a cell index be an EXACT answer rather
	// than a nearest-neighbour approximation.
	static_assert(uGYM1_GRID_COLS % 2u == 1u,
		"an even column count puts x = 0 BETWEEN lattice lines, and the arrival, the "
		"leader and the blocked-centreline arm all live on x = 0");
	static_assert(uGYM1_GRID_ROWS % 2u == 1u,
		"an even row count puts z = 0 between lattice lines, and the middle hedge "
		"band sits on z = 0");
	static_assert(uGYM1_GRID_CELLS > 0u, "an empty lattice searches nothing");

	constexpr float fGYM1_GRID_MIN_X =
		-0.5f * fGYM1_GRID_STEP * (float)(uGYM1_GRID_COLS - 1u);
	constexpr float fGYM1_GRID_MIN_Z =
		-0.5f * fGYM1_GRID_STEP * (float)(uGYM1_GRID_ROWS - 1u);

	constexpr u_int uGYM1_UNREACHED = 0xFFFFFFFFu;

	float Gym1CellX(u_int uCol) { return fGYM1_GRID_MIN_X + fGYM1_GRID_STEP * (float)uCol; }
	float Gym1CellZ(u_int uRow) { return fGYM1_GRID_MIN_Z + fGYM1_GRID_STEP * (float)uRow; }

	// Nearest lattice column / row. The subtraction is non-negative for any point
	// inside the room, so the truncating cast is a round-to-nearest.
	u_int Gym1ColFor(float fX)
	{
		return (u_int)((fX - fGYM1_GRID_MIN_X) / fGYM1_GRID_STEP + 0.5f);
	}
	u_int Gym1RowFor(float fZ)
	{
		return (u_int)((fZ - fGYM1_GRID_MIN_Z) / fGYM1_GRID_STEP + 0.5f);
	}

	// How the search asks the SHIPPED geometry its one question. Both halves come
	// from ZM_Gym1Placement.h -- the shell test and the maze test are never
	// re-derived here, because a predicate written twice is a predicate that can
	// disagree with itself and leave every unit below green.
	//
	// bMazePresent == false is the EMPTY-ROOM baseline the anti-vacuity arm needs;
	// eSkip removes exactly one band from an otherwise live maze.
	bool Gym1CellIsWalkable(u_int uCol, u_int uRow, bool bMazePresent,
		ZM_GYM1_HEDGE eSkip, float fMaxAbsX)
	{
		const float fX = Gym1CellX(uCol);
		const float fZ = Gym1CellZ(uRow);
		if (std::fabs(fX) > fMaxAbsX)
		{
			return false;
		}
		if (ZM_Gym1ShellBlocksBody(fX, fZ, fZM_GYM1_PLAYER_RADIUS))
		{
			return false;
		}
		return !bMazePresent
			|| !ZM_Gym1MazeBlocksBody(fX, fZ, fZM_GYM1_PLAYER_RADIUS, eSkip);
	}

	struct Gym1SearchResult
	{
		bool  m_bReached;
		u_int m_uSteps;      // BFS depth, in lattice cells
		bool  m_bEndsWalkable;   // were the arrival and the station themselves standable?
	};

	// Four-connected breadth-first search from the arrival cell to the leader's cell.
	//
	// ★ WHAT A LATTICE PROOF IS AND IS NOT, STATED HONESTLY. Two lattice-adjacent
	// standable positions are 0.5 m apart with a 0.4 m body radius, so their discs
	// OVERLAP -- but the swept capsule between them pokes about 7 cm outside their
	// union at the waist, so "a 4-connected free path exists" is not a formal proof
	// that a continuous free path exists. It is decisive here because the tightest
	// place in this room is a 3.0 m corridor between two hedge bands, which leaves
	// 2.2 m of standable width for a 0.8 m body -- thirty times that 7 cm. If a later
	// edit ever narrows an opening or a corridor toward the body width, this search
	// stops being the right instrument and a swept test has to replace it.
	Gym1SearchResult Gym1ShortestPath(bool bMazePresent, ZM_GYM1_HEDGE eSkip,
		float fMaxAbsX)
	{
		Gym1SearchResult xOut = { false, 0u, false };

		const u_int uStartCol = Gym1ColFor(fZM_GYM1_SPAWN_X);
		const u_int uStartRow = Gym1RowFor(fZM_GYM1_SPAWN_Z);
		const u_int uGoalCol  = Gym1ColFor(fZM_GYM1_FENNA_X);
		const u_int uGoalRow  = Gym1RowFor(fZM_GYM1_FENNA_Z);

		if (uStartCol >= uGYM1_GRID_COLS || uStartRow >= uGYM1_GRID_ROWS
			|| uGoalCol >= uGYM1_GRID_COLS || uGoalRow >= uGYM1_GRID_ROWS)
		{
			return xOut;
		}

		xOut.m_bEndsWalkable =
			Gym1CellIsWalkable(uStartCol, uStartRow, bMazePresent, eSkip, fMaxAbsX)
			&& Gym1CellIsWalkable(uGoalCol, uGoalRow, bMazePresent, eSkip, fMaxAbsX);
		if (!xOut.m_bEndsWalkable)
		{
			return xOut;
		}

		// LOCAL STATICS (ls_), not stack arrays: the lattice is ~1800 cells, and two
		// u_int arrays that size are 15 KB of stack per call in a suite that runs at
		// boot. Not reentrant, which is fine -- the unit suite is single-threaded and
		// every caller below is sequential. ls_auDist is fully reset on entry, and the
		// queue is only ever read back over what this call wrote.
		static u_int ls_auDist[uGYM1_GRID_CELLS];
		static u_int ls_auQueue[uGYM1_GRID_CELLS];
		for (u_int uCell = 0u; uCell < uGYM1_GRID_CELLS; ++uCell)
		{
			ls_auDist[uCell] = uGYM1_UNREACHED;
		}

		const u_int uStart = uStartRow * uGYM1_GRID_COLS + uStartCol;
		const u_int uGoal  = uGoalRow * uGYM1_GRID_COLS + uGoalCol;

		u_int uHead = 0u;
		u_int uTail = 0u;
		ls_auDist[uStart] = 0u;
		ls_auQueue[uTail++] = uStart;

		while (uHead < uTail)
		{
			const u_int uCell = ls_auQueue[uHead++];
			if (uCell == uGoal)
			{
				xOut.m_bReached = true;
				xOut.m_uSteps   = ls_auDist[uCell];
				return xOut;
			}

			const u_int uCol = uCell % uGYM1_GRID_COLS;
			const u_int uRow = uCell / uGYM1_GRID_COLS;

			// -X, +X, -Z, +Z. Written as four explicit guarded steps rather than an
			// offset table so a column wrap (uCol 0 stepping to uCol COLS-1 of the row
			// below) is impossible by construction rather than by arithmetic.
			for (u_int uDir = 0u; uDir < 4u; ++uDir)
			{
				u_int uNextCol = uCol;
				u_int uNextRow = uRow;
				if      (uDir == 0u) { if (uCol == 0u) { continue; } uNextCol = uCol - 1u; }
				else if (uDir == 1u) { if (uCol + 1u >= uGYM1_GRID_COLS) { continue; } uNextCol = uCol + 1u; }
				else if (uDir == 2u) { if (uRow == 0u) { continue; } uNextRow = uRow - 1u; }
				else                 { if (uRow + 1u >= uGYM1_GRID_ROWS) { continue; } uNextRow = uRow + 1u; }

				const u_int uNext = uNextRow * uGYM1_GRID_COLS + uNextCol;
				if (ls_auDist[uNext] != uGYM1_UNREACHED)
				{
					continue;
				}
				if (!Gym1CellIsWalkable(uNextCol, uNextRow, bMazePresent, eSkip, fMaxAbsX))
				{
					continue;
				}
				ls_auDist[uNext] = ls_auDist[uCell] + 1u;
				ls_auQueue[uTail++] = uNext;
			}
		}

		return xOut;
	}

	// "No restriction at all", for the fMaxAbsX parameter.
	constexpr float fGYM1_NO_X_LIMIT = 1.0e6f;

	// The ids no hedge accessor may ever treat as a band: the sentinel, one past it,
	// and two pieces of outright garbage. Mirrors ZM_Tests_TrainerData's battery.
	const ZM_GYM1_HEDGE aeGYM1_UNREGISTERED_HEDGES[] =
	{
		ZM_GYM1_HEDGE_NONE,
		(ZM_GYM1_HEDGE)((u_int)ZM_GYM1_HEDGE_COUNT + 7u),
		(ZM_GYM1_HEDGE)9999u,
		(ZM_GYM1_HEDGE)~0u
	};
	constexpr u_int uGYM1_UNREGISTERED_HEDGE_COUNT =
		(u_int)(sizeof(aeGYM1_UNREGISTERED_HEDGES) / sizeof(aeGYM1_UNREGISTERED_HEDGES[0]));

	const ZM_BADGE_ID aeGYM1_UNREGISTERED_BADGES[] =
	{
		ZM_BADGE_NONE,
		(ZM_BADGE_ID)((u_int)ZM_BADGE_ID_COUNT + 7u),
		(ZM_BADGE_ID)9999u,
		(ZM_BADGE_ID)~0u
	};
	constexpr u_int uGYM1_UNREGISTERED_BADGE_COUNT =
		(u_int)(sizeof(aeGYM1_UNREGISTERED_BADGES) / sizeof(aeGYM1_UNREGISTERED_BADGES[0]));
}

// ############################################################################
// 1. The header and the compiled world table agree
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_HeaderSpawnTagMatchesTheWorldSpecRow)
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_GYM1);

	ZENITH_ASSERT_EQ((u_int)xRow.m_eKind, (u_int)ZM_SCENE_KIND_GYM,
		"the compiled Gym 1 row stopped being a GYM, which is what decides whether it "
		"is talkable, encounterable and battle-biomed");

	// A GYM INTERIOR HAS NO TERRAIN, which is exactly why this header follows the two
	// interior placements and not Route 1's or Thornacre's.
	ZENITH_ASSERT_NOT_NULL(xRow.m_szTerrainSet, "the world row has a null terrain set");
	ZENITH_ASSERT_EQ(xRow.m_szTerrainSet[0], '\0',
		"Gym 1 named a terrain set -- an interior shell has no terrain, and this "
		"header derives nothing from one");

	// The MIRROR. szZM_GYM1_SPAWN_TAG is the one value in this header that is a copy
	// of the table rather than a derivation from it, so it is the one that can drift.
	bool bTagOffered = false;
	for (u_int uTag = 0u; uTag < xRow.m_uSpawnTagCount; ++uTag)
	{
		if (xRow.m_pszSpawnTags[uTag] != nullptr
			&& strcmp(xRow.m_pszSpawnTags[uTag], szZM_GYM1_SPAWN_TAG) == 0)
		{
			bTagOffered = true;
		}
	}
	ZENITH_ASSERT_TRUE(bTagOffered,
		"the compiled Gym 1 row does not offer the '%s' tag this header authors its "
		"arrival marker with -- IsWarpDestinationValid consults ONLY that list, so a "
		"renamed tag stalls the warp machine behind an opaque fade (ZM-D-200)",
		szZM_GYM1_SPAWN_TAG);

	// ...and the tag has to survive being written into a ZM_SpawnPoint at authoring
	// time. A tag longer than the component's capacity is TRUNCATED, which produces a
	// marker whose tag silently is not the one the table offers.
	ZENITH_ASSERT_TRUE(ZM_SpawnPoint::IsTagValid(szZM_GYM1_SPAWN_TAG),
		"'%s' is not a tag ZM_SpawnPoint can hold (capacity %u)",
		szZM_GYM1_SPAWN_TAG, (u_int)ZM_SpawnPoint::uTAG_CAPACITY);

	// The EXIT edge, resolved rather than mirrored.
	ZENITH_ASSERT_NOT_NULL(ZM_GetGym1ExitConnection(),
		"the compiled Gym 1 row carries no edge back to Thornacre, so the exit this "
		"header describes would target nothing");
	ZENITH_ASSERT_NE(ZM_GetGym1ExitTargetBuildIndex(), uZM_GYM1_EXIT_TARGET_UNRESOLVED,
		"the exit resolver produced its UNRESOLVED sentinel, which no live table may");
	ZENITH_ASSERT_EQ(ZM_GetGym1ExitTargetBuildIndex(),
		ZM_GetWorldSpec(ZM_SCENE_THORNACRE).m_uBuildIndex,
		"the exit resolves to a build index that is not Thornacre's");
	ZENITH_ASSERT_NOT_NULL(ZM_GetGym1ExitSpawnTag(), "the exit resolver returned null");
	ZENITH_ASSERT_NE(ZM_GetGym1ExitSpawnTag()[0], '\0',
		"the Gym1 -> Thornacre edge carries no spawn tag, so the exit would ask "
		"Thornacre for the empty string");

	// ...and Thornacre must actually OFFER whatever that edge names. This is the half
	// that the ProfLab door had to learn the hard way (ZM-D-200): the warp validator
	// never looks at the destination scene, so an edge naming a tag the target does
	// not offer is a door that validates and then hangs.
	const ZM_WorldSpec& xTarget = ZM_GetWorldSpec(ZM_SCENE_THORNACRE);
	bool bTargetOffersTag = false;
	for (u_int uTag = 0u; uTag < xTarget.m_uSpawnTagCount; ++uTag)
	{
		if (xTarget.m_pszSpawnTags[uTag] != nullptr
			&& strcmp(xTarget.m_pszSpawnTags[uTag], ZM_GetGym1ExitSpawnTag()) == 0)
		{
			bTargetOffersTag = true;
		}
	}
	ZENITH_ASSERT_TRUE(bTargetOffersTag,
		"Thornacre does not offer the '%s' tag the Gym 1 exit edge names",
		ZM_GetGym1ExitSpawnTag());
}

// ############################################################################
// 2. The shell is a closed room with a walkable way in
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_ShellIsAClosedRoomWithAWalkableAperture)
{
	ZENITH_ASSERT_GE((u_int)ZM_GYM1_BLOCK_COUNT, uGYM1_MIN_BLOCK_COUNT,
		"a room with fewer than a floor and four sides is not closed, and every walk "
		"below would be near-vacuous");

	// Names: present, non-empty, and pairwise distinct. TWO PASSES, because the assert
	// macros record and continue -- a single interleaved loop would strcmp a name it
	// had not yet null-checked.
	for (u_int uBlock = 0u; uBlock < (u_int)ZM_GYM1_BLOCK_COUNT; ++uBlock)
	{
		const char* szName = ZM_GetGym1BlockName((ZM_GYM1_BLOCK)uBlock);
		ZENITH_ASSERT_NOT_NULL(szName, "shell block %u has no name", uBlock);
		ZENITH_ASSERT_TRUE(szName != nullptr && szName[0] != '\0',
			"shell block %u has an empty name", uBlock);
	}
	for (u_int uA = 0u; uA < (u_int)ZM_GYM1_BLOCK_COUNT; ++uA)
	{
		const char* szA = ZM_GetGym1BlockName((ZM_GYM1_BLOCK)uA);
		if (szA == nullptr || szA[0] == '\0') { continue; }
		for (u_int uB = uA + 1u; uB < (u_int)ZM_GYM1_BLOCK_COUNT; ++uB)
		{
			const char* szB = ZM_GetGym1BlockName((ZM_GYM1_BLOCK)uB);
			if (szB == nullptr || szB[0] == '\0') { continue; }
			ZENITH_ASSERT_NE(strcmp(szA, szB), 0,
				"shell blocks %u and %u share the name '%s' -- G1-2 looks these up by "
				"name, so a duplicate is two entities the authoring cannot tell apart",
				uA, uB, szA);
		}
	}

	// The floor's TOP FACE is the y = 0 every FEET height in this room assumes.
	const ZM_Gym1Blockout xFloor = ZM_GetGym1Block(ZM_GYM1_BLOCK_FLOOR);
	ZENITH_ASSERT_EQ_FLOAT(xFloor.Max().y, fZM_GYM1_FLOOR_TOP_Y, fGYM1_EXACT_EPSILON,
		"the floor's top face is not y = 0, so every spawn marker in this room stands "
		"in or above the slab");

	// Every wall stands ON that face rather than sunk into it or floating over it.
	for (u_int uBlock = 0u; uBlock < (u_int)ZM_GYM1_BLOCK_COUNT; ++uBlock)
	{
		if (uBlock == (u_int)ZM_GYM1_BLOCK_FLOOR || uBlock == (u_int)ZM_GYM1_BLOCK_LINTEL)
		{
			continue;
		}
		const ZM_Gym1Blockout x = ZM_GetGym1Block((ZM_GYM1_BLOCK)uBlock);
		ZENITH_ASSERT_EQ_FLOAT(x.Min().y, fZM_GYM1_FLOOR_TOP_Y, fGYM1_EXACT_EPSILON,
			"shell block %u does not have its foot on the floor's top face", uBlock);
		ZENITH_ASSERT_EQ_FLOAT(x.Max().y, fZM_GYM1_WALL_HEIGHT, fGYM1_EXACT_EPSILON,
			"shell block %u does not reach the wall height", uBlock);
	}

	// THE APERTURE. It has to admit the body contract's own footprint with room to
	// spare, in both axes -- and the lintel has to bridge it exactly rather than
	// overlapping the panels or leaving a slot of sky.
	const ZM_Gym1Blockout xLeft   = ZM_GetGym1Block(ZM_GYM1_BLOCK_FRONT_LEFT);
	const ZM_Gym1Blockout xRight  = ZM_GetGym1Block(ZM_GYM1_BLOCK_FRONT_RIGHT);
	const ZM_Gym1Blockout xLintel = ZM_GetGym1Block(ZM_GYM1_BLOCK_LINTEL);

	const float fClearWidth = xRight.Min().x - xLeft.Max().x;
	ZENITH_ASSERT_GT(fClearWidth,
		fZM_HUMAN_BODY_FOOTPRINT + 2.0f * fGYM1_APERTURE_SIDE_MARGIN,
		"the entrance leaves %.3f m of clear width for a %.3f m body", fClearWidth,
		fZM_HUMAN_BODY_FOOTPRINT);

	const float fClearHeight = xLintel.Min().y - fZM_GYM1_FLOOR_TOP_Y;
	ZENITH_ASSERT_GT(fClearHeight, fZM_HUMAN_BODY_HEIGHT + fGYM1_APERTURE_HEAD_MARGIN,
		"the lintel leaves %.3f m of headroom for a %.3f m body", fClearHeight,
		fZM_HUMAN_BODY_HEIGHT);

	ZENITH_ASSERT_EQ_FLOAT(xLintel.Max().y, fZM_GYM1_WALL_HEIGHT, fGYM1_EXACT_EPSILON,
		"the lintel does not reach the wall height, so the doorway has a slot of open "
		"sky above it");
	ZENITH_ASSERT_EQ_FLOAT(xLintel.Min().x, xLeft.Max().x, fGYM1_EXACT_EPSILON,
		"the lintel and the left panel do not meet");
	ZENITH_ASSERT_EQ_FLOAT(xLintel.Max().x, xRight.Min().x, fGYM1_EXACT_EPSILON,
		"the lintel and the right panel do not meet");
}

// ############################################################################
// 3. The hedge bands are walls, one opening each, alternating sides
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_HedgeBandsAreWallsWithExactlyOneOpeningEach)
{
	ZENITH_ASSERT_GE((u_int)ZM_GYM1_HEDGE_COUNT, uGYM1_MIN_HEDGE_COUNT,
		"a single band is a doorway, not a maze, and every walk below would be "
		"near-vacuous");
	ZENITH_ASSERT_EQ((u_int)ZM_GYM1_HEDGE_NONE, (u_int)ZM_GYM1_HEDGE_COUNT,
		"the hedge sentinel must keep aliasing COUNT -- ZM_IsGym1Hedge rejects both "
		"with one compare, and 'skip nothing' is spelled as the sentinel");

	// ★ THE HEDGE MUST BE TALLER THAN A PERSON. Without this the maze is a decoration:
	// the player reads the route off the floor and the puzzle stops existing.
	ZENITH_ASSERT_GT(fZM_GYM1_HEDGE_HEIGHT, fZM_HUMAN_BODY_HEIGHT,
		"a %.2f m hedge does not hide the route from a %.2f m player, so the maze is "
		"decoration rather than a puzzle",
		fZM_GYM1_HEDGE_HEIGHT, fZM_HUMAN_BODY_HEIGHT);

	// ...and SHORTER than the shell it stands inside, or it is a wall.
	ZENITH_ASSERT_LT(fZM_GYM1_HEDGE_HEIGHT, fZM_GYM1_WALL_HEIGHT,
		"the hedge is as tall as the room's own walls");

	bool bSeenMinusX = false;
	bool bSeenPlusX  = false;

	for (u_int uHedge = 0u; uHedge < (u_int)ZM_GYM1_HEDGE_COUNT; ++uHedge)
	{
		const ZM_GYM1_HEDGE eHedge = (ZM_GYM1_HEDGE)uHedge;
		const ZM_Gym1Blockout x = ZM_GetGym1Hedge(eHedge);
		const char* szName = ZM_GetGym1HedgeName(eHedge);

		ZENITH_ASSERT_NOT_NULL(szName, "hedge %u has no name", uHedge);
		ZENITH_ASSERT_TRUE(ZM_IsGym1Hedge(eHedge),
			"hedge %u is in the table but does not register", uHedge);

		// Seated on the floor, not sunk into it.
		ZENITH_ASSERT_EQ_FLOAT(x.Min().y, fZM_GYM1_FLOOR_TOP_Y, fGYM1_EXACT_EPSILON,
			"%s does not have its foot on the floor", szName);
		ZENITH_ASSERT_EQ_FLOAT(x.Max().y, fZM_GYM1_FLOOR_TOP_Y + fZM_GYM1_HEDGE_HEIGHT,
			fGYM1_EXACT_EPSILON, "%s is not the authored hedge height", szName);

		// A BAND, not a bush: it has to reach one inner wall exactly and stop short of
		// the other by exactly the opening. Anything less at the far end is a SECOND
		// way past that band, which silently unmakes the puzzle.
		// A side of 0 is the accessor's degenerate answer, and it would send this walk
		// down the "+X" arm below for a band that declares no side at all.
		const float fSide = ZM_GetGym1HedgeOpeningSide(eHedge);
		ZENITH_ASSERT_GT(std::fabs(fSide), 0.5f, "%s declares no opening side", szName);
		if (fSide < 0.0f)
		{
			bSeenMinusX = true;
			ZENITH_ASSERT_EQ_FLOAT(x.Max().x, fZM_GYM1_INNER_MAX_X, fGYM1_EXACT_EPSILON,
				"%s does not reach the +X inner wall, so there is a second way past it",
				szName);
			ZENITH_ASSERT_EQ_FLOAT(x.Min().x - fZM_GYM1_INNER_MIN_X,
				fZM_GYM1_HEDGE_OPENING, fGYM1_EXACT_EPSILON,
				"%s leaves an opening that is not the authored width", szName);
		}
		else
		{
			bSeenPlusX = true;
			ZENITH_ASSERT_EQ_FLOAT(x.Min().x, fZM_GYM1_INNER_MIN_X, fGYM1_EXACT_EPSILON,
				"%s does not reach the -X inner wall, so there is a second way past it",
				szName);
			ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_INNER_MAX_X - x.Max().x,
				fZM_GYM1_HEDGE_OPENING, fGYM1_EXACT_EPSILON,
				"%s leaves an opening that is not the authored width", szName);
		}

		// The opening has to admit the body with room to spare on both sides.
		//
		// ★ THIS IS A LOWER BOUND ONLY, AND THE UPPER ONE IS NOT HERE. Nothing about a
		// SINGLE band says how wide is too wide -- a band with a 20 m gap is still a
		// band that reaches one wall and leaves one opening, which is all this walk can
		// see. The ceiling is a property of the MAZE and lives in
		// Gym1_EveryHedgeBandBlocksTheDirectRouteToTheLeader's middle-of-the-room arm.
		// Do not add a literal ceiling here: it would be a second, unreconciled opinion
		// about the same number.
		ZENITH_ASSERT_GT(fZM_GYM1_HEDGE_OPENING,
			fZM_HUMAN_BODY_FOOTPRINT + 2.0f * fGYM1_APERTURE_SIDE_MARGIN,
			"%s leaves %.3f m for a %.3f m body", szName, fZM_GYM1_HEDGE_OPENING,
			fZM_HUMAN_BODY_FOOTPRINT);
	}

	// ★ THE ALTERNATION CLAIM. Three bands that all opened on the same side would pass
	// every clause above and would be a straight run down one edge of the room -- a
	// corridor with three doors in it, not a maze.
	ZENITH_ASSERT_TRUE(bSeenMinusX && bSeenPlusX,
		"every hedge band opens on the same side of the room, which makes the route a "
		"straight run down one edge rather than a serpentine");

	// The bands must not touch each other, or the corridors between them close.
	for (u_int uHedge = 1u; uHedge < (u_int)ZM_GYM1_HEDGE_COUNT; ++uHedge)
	{
		const ZM_Gym1Blockout xPrev = ZM_GetGym1Hedge((ZM_GYM1_HEDGE)(uHedge - 1u));
		const ZM_Gym1Blockout xThis = ZM_GetGym1Hedge((ZM_GYM1_HEDGE)uHedge);
		const float fCorridor = xPrev.Min().z - xThis.Max().z;
		ZENITH_ASSERT_GT(fCorridor,
			fZM_HUMAN_BODY_FOOTPRINT + 2.0f * fGYM1_APERTURE_SIDE_MARGIN,
			"the corridor between bands %u and %u is %.3f m deep for a %.3f m body",
			uHedge - 1u, uHedge, fCorridor, fZM_HUMAN_BODY_FOOTPRINT);
	}
}

// ############################################################################
// 4. THE HEADLINE CLAIM: the maze is solvable, headless, from the header alone
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_MazeIsSolvableFromTheArrivalToTheLeaderStation)
{
	// The lattice has to be the room, not something near it. A room resize that
	// knocked the arrival or the station off the lattice would silently sample a
	// neighbouring cell, and every arm below would then be about a different point.
	const u_int uStartCol = Gym1ColFor(fZM_GYM1_SPAWN_X);
	const u_int uStartRow = Gym1RowFor(fZM_GYM1_SPAWN_Z);
	const u_int uGoalCol  = Gym1ColFor(fZM_GYM1_FENNA_X);
	const u_int uGoalRow  = Gym1RowFor(fZM_GYM1_FENNA_Z);

	ZENITH_ASSERT_LT(uStartCol, uGYM1_GRID_COLS, "the arrival falls off the lattice");
	ZENITH_ASSERT_LT(uStartRow, uGYM1_GRID_ROWS, "the arrival falls off the lattice");
	ZENITH_ASSERT_LT(uGoalCol,  uGYM1_GRID_COLS, "the leader falls off the lattice");
	ZENITH_ASSERT_LT(uGoalRow,  uGYM1_GRID_ROWS, "the leader falls off the lattice");

	if (uStartCol < uGYM1_GRID_COLS && uStartRow < uGYM1_GRID_ROWS
		&& uGoalCol < uGYM1_GRID_COLS && uGoalRow < uGYM1_GRID_ROWS)
	{
		ZENITH_ASSERT_EQ_FLOAT(Gym1CellX(uStartCol), fZM_GYM1_SPAWN_X, fGYM1_EXACT_EPSILON,
			"the arrival's lattice cell is not the arrival point");
		ZENITH_ASSERT_EQ_FLOAT(Gym1CellZ(uStartRow), fZM_GYM1_SPAWN_Z, fGYM1_EXACT_EPSILON,
			"the arrival's lattice cell is not the arrival point");
		ZENITH_ASSERT_EQ_FLOAT(Gym1CellX(uGoalCol), fZM_GYM1_FENNA_X, fGYM1_EXACT_EPSILON,
			"the leader's lattice cell is not her station");
		ZENITH_ASSERT_EQ_FLOAT(Gym1CellZ(uGoalRow), fZM_GYM1_FENNA_Z, fGYM1_EXACT_EPSILON,
			"the leader's lattice cell is not her station");
	}

	// ---- ARM 1: THE SEARCH IS SOUND ------------------------------------------
	// Over an EMPTY room the four-connected shortest path is exactly the row
	// difference, because the two ends share a column. If that does not hold, the
	// search is broken and arm 2 would be measuring nothing.
	const Gym1SearchResult xOpen =
		Gym1ShortestPath(false, ZM_GYM1_HEDGE_NONE, fGYM1_NO_X_LIMIT);
	ZENITH_ASSERT_TRUE(xOpen.m_bEndsWalkable,
		"the arrival or the leader's station is not standable even with the maze "
		"removed, so both ends are inside the shell rather than inside the room");
	ZENITH_ASSERT_TRUE(xOpen.m_bReached,
		"the search could not cross an EMPTY room, so it is the search that is broken "
		"and not the maze");
	const u_int uExpectedOpen = (uStartRow > uGoalRow)
		? (uStartRow - uGoalRow) : (uGoalRow - uStartRow);
	ZENITH_ASSERT_EQ(xOpen.m_uSteps, uExpectedOpen,
		"over an empty room the shortest four-connected path must be the straight run "
		"down the shared column (%u cells), not %u -- the search is not returning a "
		"shortest path", uExpectedOpen, xOpen.m_uSteps);

	// ---- ARM 2: THE MAZE IS SOLVABLE -----------------------------------------
	const Gym1SearchResult xMaze =
		Gym1ShortestPath(true, ZM_GYM1_HEDGE_NONE, fGYM1_NO_X_LIMIT);
	ZENITH_ASSERT_TRUE(xMaze.m_bEndsWalkable,
		"the arrival or the leader's station stands INSIDE a hedge -- the player would "
		"warp in embedded in the maze, or the leader would be unreachable by "
		"construction");
	ZENITH_ASSERT_TRUE(xMaze.m_bReached,
		"THE MAZE IS UNSOLVABLE. No path exists from the arrival marker to the "
		"leader's station with the authored hedge bands in place, so a player who "
		"walked through the gym door could never challenge Fenna");

	// ---- ARM 3: AND IT COSTS SOMETHING ---------------------------------------
	// The claim that stops a reachability test from passing over an empty room: the
	// authored route is strictly longer than the open-room one.
	ZENITH_ASSERT_GT(xMaze.m_uSteps, xOpen.m_uSteps,
		"the maze route (%u cells) is no longer than the open-room route (%u cells), "
		"so the hedges cost the player nothing and this whole unit would pass over a "
		"room with no maze in it", xMaze.m_uSteps, xOpen.m_uSteps);
}

// ############################################################################
// 5. THE ANTI-VACUITY ARM: every band genuinely blocks a shorter route
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_EveryHedgeBandBlocksTheDirectRouteToTheLeader)
{
	ZENITH_ASSERT_GT((u_int)ZM_GYM1_HEDGE_COUNT, 0u,
		"an empty maze makes the walk below vacuous");

	// The arrival and the leader share a column, so "the direct route" is literally
	// the line x = fZM_GYM1_SPAWN_X. Every band must sit across it -- and, one band at
	// a time, must be the ONLY thing sitting across it at that band's own depth. The
	// second half is what makes this a proof that THAT WALL blocks the shorter route,
	// rather than a proof that SOMETHING does.
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_FENNA_X, fZM_GYM1_SPAWN_X, fGYM1_EXACT_EPSILON,
		"the leader no longer shares the arrival's column, so 'the direct route' is "
		"not the line this unit walks and every clause below is about the wrong line");

	for (u_int uHedge = 0u; uHedge < (u_int)ZM_GYM1_HEDGE_COUNT; ++uHedge)
	{
		const ZM_GYM1_HEDGE eHedge = (ZM_GYM1_HEDGE)uHedge;
		const char* szName = ZM_GetGym1HedgeName(eHedge);
		const float fZ = ZM_GetGym1HedgeZ(eHedge);

		ZENITH_ASSERT_TRUE(
			ZM_Gym1MazeBlocksBody(fZM_GYM1_SPAWN_X, fZ, fZM_GYM1_PLAYER_RADIUS,
				ZM_GYM1_HEDGE_NONE),
			"%s does not cross the straight line from the arrival to the leader, so a "
			"player could walk past it without solving anything", szName);

		ZENITH_ASSERT_FALSE(
			ZM_Gym1MazeBlocksBody(fZM_GYM1_SPAWN_X, fZ, fZM_GYM1_PLAYER_RADIUS, eHedge),
			"removing %s alone leaves the centreline at its own depth still blocked, so "
			"the previous clause was satisfied by a DIFFERENT band and this unit cannot "
			"say that %s blocks anything", szName, szName);
	}

	// ★★ THE STRONGEST FORM, AND THE ONE A WIDENED OPENING REDS. Restrict the walk to
	// the middle HALF of the room and the maze must be UNSOLVABLE, because every band's
	// only gap lies out against a side wall. In other words the walls do not merely
	// lengthen the route, they FORCE it out to the edges of the room. An author who
	// widened the openings toward the centreline until the player could weave straight
	// down the middle would still pass arms 1-3 of the solvability unit; this is the
	// clause that stops them.
	//
	// ★★ THE BOUND COMES FROM THE ROOM, NEVER FROM THE OPENING, AND THAT IS THE WHOLE
	// CLAUSE. It was first written as `fZM_GYM1_INNER_MAX_X - fZM_GYM1_HEDGE_OPENING`,
	// which is a guard comparing a value against a re-computation of itself and could
	// not red for ANY opening width. A -X band spans [innerMin + opening, innerMax], so
	// its gap ends at exactly `innerMin + opening` = `-(innerMax - opening)` -- the
	// restriction's own edge. Widening the opening widened the restriction by the
	// identical amount, the body radius put the first passable cell strictly outside
	// either way, and the answer was invariant: an opening of 11.75 m -- half the
	// room's inner width, i.e. three token stubs rather than a maze -- left this unit
	// and the whole suite green. Half the inner HALF-width is a fraction of the SHELL,
	// so the restricted strip stays put while a widened gap moves into it.
	const float fMIDDLE_LIMIT = fZM_GYM1_INNER_MAX_X * 0.5f;

	// ---- The two properties that bound is REQUIRED to have, as arithmetic ------
	//
	// A -X band is clear of a body of fZM_GYM1_PLAYER_RADIUS only at
	// x <= innerMin + opening - radius, and the restriction admits x >= -fMIDDLE_LIMIT.
	// Equating the two gives the opening width at which the restricted strip first
	// touches a gap -- the width at which this arm FLIPS from pass to fail. A +X band
	// is the mirror image and yields the same number, because the room and the
	// restriction are both symmetric in X.
	const float fREDS_AT_OPENING =
		fZM_GYM1_INNER_MAX_X - fMIDDLE_LIMIT + fZM_GYM1_PLAYER_RADIUS;

	// PROPERTY 1 -- IT STILL PASSES ON THE SHIPPED LAYOUT, and as a consequence of the
	// geometry rather than as an observation about one lattice. The authored opening is
	// strictly below the flip, so EVERY band's gap is strictly outside the restricted
	// strip; a route has to cross all three bands (Fenna stands behind the last one), so
	// no route inside the strip can exist. This is the CONSERVATIVE of the two flip
	// widths below -- it reds BEFORE the lattice one does, which is the right order: a
	// layout that has stopped being provably sealed, and is only still sealed because
	// the search happens to sample every 0.5 m, is a layout worth stopping on.
	//
	// Read as a bound on fMIDDLE_LIMIT, this clause is the CEILING: a strip loosened
	// out past the gaps reds here.
	ZENITH_ASSERT_GT(fREDS_AT_OPENING, fZM_GYM1_HEDGE_OPENING,
		"the middle-of-the-room restriction no longer seals the maze at the authored "
		"%.3f m opening -- it flips at %.3f m -- so the clause below is about to fail "
		"for the BOUND's sake rather than the maze's",
		fZM_GYM1_HEDGE_OPENING, fREDS_AT_OPENING);

	// PROPERTY 2 -- AND IT REDS ON A MATERIALLY WIDER OPENING, which is what the old
	// self-referential bound could not do at ANY width. Read as a bound on
	// fMIDDLE_LIMIT these are the FLOOR: a strip tightened toward the centreline makes
	// the flip width climb, and a flip width high enough is an arm that polices
	// nothing. Two of them, because they say different things:
	//   * the flip is under HALF THE ROOM'S INNER WIDTH, so the exact counterexample
	//     the old bound could not see -- 11.75 m gaps, three token stubs rather than a
	//     maze -- reds here;
	//   * and it is under TWICE the authored opening, so the arm bites well before the
	//     doorways could double, rather than only at some width no band could leave.
	ZENITH_ASSERT_LT(fREDS_AT_OPENING, fZM_GYM1_INNER_MAX_X,
		"the opening width that would red this arm (%.3f m) is at or past half the "
		"room's own inner width (%.3f m), so a maze of three token stubs would satisfy "
		"this clause and the restriction is policing nothing",
		fREDS_AT_OPENING, fZM_GYM1_INNER_MAX_X);
	ZENITH_ASSERT_LT(fREDS_AT_OPENING, 2.0f * fZM_GYM1_HEDGE_OPENING,
		"the opening width that would red this arm (%.3f m) is at or past twice the "
		"authored %.3f m, so the restriction has been tightened until it no longer "
		"notices an opening being widened materially",
		fREDS_AT_OPENING, fZM_GYM1_HEDGE_OPENING);

	// ...and the width the LATTICE flips at, which is the number a reader who reds this
	// clause actually wants. The search samples columns, so the strip's effective edge
	// is the outermost column it admits rather than the bound itself -- x = +/-5.5
	// against a 5.875 m bound, worth 0.375 m of opening. Found by asking every column
	// the same question Gym1CellIsWalkable asks, so it cannot disagree with the search.
	float fOutermostAllowedX = 0.0f;
	for (u_int uCol = 0u; uCol < uGYM1_GRID_COLS; ++uCol)
	{
		const float fAbsX = std::fabs(Gym1CellX(uCol));
		if (fAbsX <= fMIDDLE_LIMIT && fAbsX > fOutermostAllowedX)
		{
			fOutermostAllowedX = fAbsX;
		}
	}
	const float fLATTICE_REDS_AT_OPENING =
		fZM_GYM1_INNER_MAX_X - fOutermostAllowedX + fZM_GYM1_PLAYER_RADIUS;

	// ...which is only a claim at all while both ends are INSIDE the restriction. A
	// bound that excluded the arrival would make the search fail for a reason that has
	// nothing to do with the maze.
	ZENITH_ASSERT_LE(std::fabs(fZM_GYM1_SPAWN_X), fMIDDLE_LIMIT,
		"the arrival is outside the middle-of-the-room restriction, so the clause "
		"below would fail vacuously");
	ZENITH_ASSERT_LE(std::fabs(fZM_GYM1_FENNA_X), fMIDDLE_LIMIT,
		"the leader is outside the middle-of-the-room restriction, so the clause below "
		"would fail vacuously");

	const Gym1SearchResult xMiddle =
		Gym1ShortestPath(true, ZM_GYM1_HEDGE_NONE, fMIDDLE_LIMIT);
	// ...and the search must have actually RUN. A restriction that made either end
	// unstandable would produce the same "not reached" answer for a reason that has
	// nothing to do with the hedges.
	ZENITH_ASSERT_TRUE(xMiddle.m_bEndsWalkable,
		"the middle-of-the-room restriction excluded the arrival or the leader, so the "
		"clause below would report 'unreachable' without ever entering the maze");
	ZENITH_ASSERT_FALSE(xMiddle.m_bReached,
		"a player who never left the middle %.3f m of the room reached the leader "
		"anyway -- the bands leave %.3f m openings against the %.3f m at which a gap "
		"reaches this strip, so the maze no longer forces the route to the walls",
		2.0f * fMIDDLE_LIMIT, fZM_GYM1_HEDGE_OPENING, fLATTICE_REDS_AT_OPENING);

	// ...and the same restriction over an EMPTY room MUST succeed, or the clause above
	// proved only that the restriction itself is impassable.
	const Gym1SearchResult xMiddleOpen =
		Gym1ShortestPath(false, ZM_GYM1_HEDGE_NONE, fMIDDLE_LIMIT);
	ZENITH_ASSERT_TRUE(xMiddleOpen.m_bReached,
		"the middle-of-the-room restriction is impassable even with NO maze, so the "
		"clause above says nothing about the hedges");
}

// ############################################################################
// 6. The leader's station
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_LeaderStationStandsClearBehindTheLastHedge)
{
	// BEHIND the last band, which is the room's entire premise: the only way to
	// challenge her is through the maze.
	const ZM_GYM1_HEDGE eLast = (ZM_GYM1_HEDGE)((u_int)ZM_GYM1_HEDGE_COUNT - 1u);
	ZENITH_ASSERT_LT(fZM_GYM1_FENNA_Z, ZM_GetGym1Hedge(eLast).Min().z,
		"the leader stands in front of (or inside) the last hedge band, so the player "
		"can reach her without solving the maze");

	// Her BODY, not her anchor, has to fit -- an anchor clearance a body does not
	// actually have is the kind of number that looks like a check and is not one.
	ZENITH_ASSERT_GT(ZM_GetGym1FennaLastHedgeClearance(), 0.0f,
		"the leader's body overlaps the last hedge band (%.3f m)",
		ZM_GetGym1FennaLastHedgeClearance());
	ZENITH_ASSERT_GT(ZM_GetGym1FennaBackWallClearance(), 0.0f,
		"the leader's body overlaps the back wall (%.3f m)",
		ZM_GetGym1FennaBackWallClearance());

	// She must be standable in the first place, which is the same question the
	// solvability search asks of her cell.
	ZENITH_ASSERT_TRUE(
		ZM_Gym1PositionIsWalkable(fZM_GYM1_FENNA_X, fZM_GYM1_FENNA_Z,
			fZM_GYM1_PLAYER_RADIUS, ZM_GYM1_HEDGE_NONE),
		"the leader's station is not a position a body can occupy");

	// ★ THE PLAYER WALKS UP TO TALK. Arriving already inside her reach would let a
	// stray interact press open the badge battle from the doormat, before the maze had
	// been solved at all. ZM_PickInteractTarget is INCLUSIVE at the boundary, so
	// "outside reach" means strictly greater.
	ZENITH_ASSERT_GT(ZM_GetGym1FennaArrivalStandoff(), fZM_GYM1_FENNA_EFFECTIVE_REACH,
		"the arriving player stands %.3f m from the leader against an effective reach "
		"of %.3f m -- the gym battle could be started from the doorway",
		ZM_GetGym1FennaArrivalStandoff(), fZM_GYM1_FENNA_EFFECTIVE_REACH);

	// The reach bonus is her own half footprint, spelled from the body contract rather
	// than as a literal, so a re-tune of the body lands here.
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_FENNA_REACH_BONUS,
		fZM_HUMAN_BODY_FOOTPRINT * 0.5f, fGYM1_EXACT_EPSILON,
		"the leader's reach bonus stopped being her own half footprint");
}

// The facing claim, run through the SHIPPED forward function rather than restated.
// A guard that re-spells the same quaternion the authoring spells cannot see the
// authoring move; the property worth holding is "she faces the player".
ZENITH_TEST(ZM_WorldTraversal, Gym1_LeaderFacesTheArrivalAtIdentity)
{
	// glm::quat's constructor is (w, x, y, z).
	const Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xIdentity);

	const Zenith_Maths::Vector3 xStation = ZM_GetGym1FennaFeet();
	const Zenith_Maths::Vector3 xPivot   = ZM_GetGym1ArrivalPivot();

	const float fToPlayerX = xPivot.x - xStation.x;
	const float fToPlayerZ = xPivot.z - xStation.z;
	const float fDot = xForward.x * fToPlayerX + xForward.z * fToPlayerZ;

	ZENITH_ASSERT_GT(fDot, 0.0f,
		"authored at IDENTITY the leader's forward (%.3f, %.3f) does not point at the "
		"arrival -- she would greet every player with her back turned, which is exactly "
		"the defect ProfLab had to pay a frozen half-turn to fix and which this room "
		"avoids by PLACING her on the camera's axis instead",
		xForward.x, xForward.z);

	// The anti-vacuity arm: the same predicate must REJECT a station on the wrong side
	// of the arrival. Without it, a forward of (0,0) or a dot of exactly 0 would be
	// indistinguishable from a pass on a badly-typed comparison.
	const float fMirroredDot =
		xForward.x * fToPlayerX + xForward.z * (-fToPlayerZ);
	ZENITH_ASSERT_LT(fMirroredDot, 0.0f,
		"the facing predicate accepts a leader standing on the FAR side of the arrival "
		"just as readily, so it is not testing a direction at all");
}

// ############################################################################
// 7. The arrival camera, and why the hedge height is what it is
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_FollowCameraLooksOverTheFirstHedgeAtTheArrival)
{
	// The MIRRORS first. Every camera constant in the header is a copy of a private
	// tuning value in the shipped component; if one drifts, every clearance in this
	// room is void.
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_CAMERA_ARM, ZM_FollowCamera::GetArmLength(),
		fGYM1_EXACT_EPSILON, "the mirrored camera arm drifted from the component");
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_CAMERA_HEIGHT, ZM_FollowCamera::GetCameraHeight(),
		fGYM1_EXACT_EPSILON, "the mirrored camera height drifted from the component");
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_CAMERA_FOV_DEGREES, ZM_FollowCamera::GetFOVDegrees(),
		fGYM1_EXACT_EPSILON, "the mirrored camera FOV drifted from the component");
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_CAMERA_PIVOT_HEIGHT, ZM_FollowCamera::GetPivotHeight(),
		fGYM1_EXACT_EPSILON, "the mirrored camera pivot drifted from the component");

	// The camera trails toward -Z, which is what puts the maze in front of the player
	// and the doorway behind them.
	ZENITH_ASSERT_LT(ZM_GetGym1CameraPosition().z, fZM_GYM1_SPAWN_Z,
		"the authored camera is not BEHIND the arrival point");
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_CAMERA_YAW, 0.0f, fGYM1_EXACT_EPSILON,
		"the authored yaw is not 0, so 'the camera trails to -Z' -- and every clearance "
		"figure derived from it -- is void");

	// ...and it is inside the room, not through the back wall.
	ZENITH_ASSERT_GT(ZM_GetGym1CameraPosition().z, fZM_GYM1_INNER_MIN_Z,
		"the trailing camera lands behind the back wall");

	// ★ THE CLAIM THE HEDGE HEIGHT WAS CHOSEN FOR. The 5.5 m arm parks the settled
	// camera INSIDE the maze's first corridor, so the arrival shot looks back over the
	// first band at the player. That only works while the hedge is short enough.
	const float fClearance = ZM_GetGym1CameraSightClearanceOverFirstHedge();
	ZENITH_ASSERT_TRUE(std::isfinite(fClearance),
		"the arrival sight-line clearance is not a finite number");
	ZENITH_ASSERT_GT(fClearance, 0.0f,
		"the arrival sight line passes %.3f m BELOW the top of the first hedge, so the "
		"player warps in behind a wall of foliage", -fClearance);

	// THE ANTI-VACUITY ARM, fed through the SHIPPED function at a different height: a
	// hedge as tall as the room's own walls MUST fail the same check. Without this the
	// clause above would also pass on a clearance function that returned a constant.
	//
	// ★ IT CALLS ZM_GetGym1CameraSightClearanceOverFirstHedgeAtHeight RATHER THAN
	// RE-SPELLING THE INTERPOLATION. This block used to carry its own copy of the fT /
	// fSightY arithmetic, which made it a test of a SECOND predicate that happened to
	// agree with the first -- the shipped one could have drifted underneath it and this
	// arm would have gone on passing. The zero-argument accessor is now literally that
	// call with fZM_GYM1_HEDGE_HEIGHT, so the two cannot disagree.
	const float fWallHeightClearance =
		ZM_GetGym1CameraSightClearanceOverFirstHedgeAtHeight(fZM_GYM1_WALL_HEIGHT);
	ZENITH_ASSERT_LT(fWallHeightClearance, 0.0f,
		"a hedge at the shell's own %.2f m wall height still clears the arrival sight "
		"line by %.3f m, so the positive clearance above is not evidence that the hedge "
		"height was chosen at all", fZM_GYM1_WALL_HEIGHT, fWallHeightClearance);

	// ...and the two really are the same predicate: the authored answer must equal the
	// parameterised one at the authored height. A delegate that stopped passing
	// fZM_GYM1_HEDGE_HEIGHT -- or grew a second copy of the interpolation -- would show
	// up here rather than nowhere.
	ZENITH_ASSERT_EQ_FLOAT(fClearance,
		ZM_GetGym1CameraSightClearanceOverFirstHedgeAtHeight(fZM_GYM1_HEDGE_HEIGHT),
		fGYM1_EXACT_EPSILON,
		"the authored sight-clearance accessor is no longer the height-parameterised "
		"one evaluated at the authored hedge height");

	// The settled camera sits one capsule half extent above the authored one -- the
	// ZM-D-173 gap that must never be "fixed" by writing the settled Y into the header.
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GetGym1SettledCameraPosition().y - ZM_GetGym1CameraPosition().y,
		fZM_GYM1_PLAYER_CAPSULE_HALF_EXTENT, fGYM1_EXACT_EPSILON,
		"the settled camera is no longer one capsule half extent above the authored "
		"one, so either the authored Y was 'corrected' or the body contract moved");
}

// ############################################################################
// 8. The exit sensor
// ############################################################################

ZENITH_TEST(ZM_WorldTraversal, Gym1_ExitSensorFillsTheApertureAndClearsTheArrivingBody)
{
	const ZM_Gym1Blockout xSensor = ZM_GetGym1ExitTrigger();

	// (1) It spans the aperture exactly: narrower leaves a strip of doorway a player
	// can slip through, wider pokes into the panels either side.
	const ZM_Gym1Blockout xLeft  = ZM_GetGym1Block(ZM_GYM1_BLOCK_FRONT_LEFT);
	const ZM_Gym1Blockout xRight = ZM_GetGym1Block(ZM_GYM1_BLOCK_FRONT_RIGHT);
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Min().x, xLeft.Max().x, fGYM1_EXACT_EPSILON,
		"the exit sensor does not reach the left doorway panel");
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Max().x, xRight.Min().x, fGYM1_EXACT_EPSILON,
		"the exit sensor does not reach the right doorway panel");

	// (2) It is seated on the floor and fills the aperture height: a short sensor can
	// be jumped, a tall one sticks through the lintel.
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Min().y, fZM_GYM1_FLOOR_TOP_Y, fGYM1_EXACT_EPSILON,
		"the exit sensor is not seated on the floor");
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Max().y, fZM_GYM1_APERTURE_HEIGHT, fGYM1_EXACT_EPSILON,
		"the exit sensor does not fill the aperture height");

	// (3) Its far face is the doorway wall's inner plane, so nothing reaches the
	// aperture without crossing the sensor first.
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Max().z, fZM_GYM1_INNER_MAX_Z, fGYM1_EXACT_EPSILON,
		"the exit sensor does not reach the doorway wall, so there is floor between it "
		"and the opening the player can stand on");

	// (4) ★★ AND ITS NEAR FACE CLEARS THE ARRIVING BODY, OR THE DOOR IS AN INFINITE
	// LOOP. ZM_WarpTrigger fires from OnCollisionEnter, so an overlapping arrival warps
	// the player straight back out on the first contact tick.
	ZENITH_ASSERT_GT(ZM_GetGym1ExitTriggerArrivalClearance(), 0.0f,
		"the arriving capsule overlaps the exit sensor by %.4f m -- the player would be "
		"bounced back to Thornacre on the tick they arrived, forever",
		-ZM_GetGym1ExitTriggerArrivalClearance());

	// (5) The sensor owns the LAST QUARTER of the walk-in span and no more -- a
	// derivation, so widening the room moves it rather than leaving it behind.
	ZENITH_ASSERT_EQ_FLOAT(fZM_GYM1_EXIT_TRIGGER_SCALE_Z,
		fZM_GYM1_WALK_IN_SPAN * 0.25f, fGYM1_EXACT_EPSILON,
		"the exit sensor stopped owning exactly the last quarter of the walk-in span");

	// (6) It must not reach the maze. The first band is the thing the player has to
	// walk toward; a sensor that touched it would warp them out mid-puzzle.
	ZENITH_ASSERT_GT(xSensor.Min().z, ZM_GetGym1Hedge(ZM_GYM1_HEDGE_FIRST).Max().z,
		"the exit sensor reaches the first hedge band, so walking into the maze would "
		"warp the player out of the gym");
}

// ############################################################################
// 9. Accessor totality
// ############################################################################

// Written in the local-hit-count form this repo's suites use, for two mandatory
// reasons: (1) NO ZENITH_ASSERT_* may appear INSIDE the capture scope -- while one
// is active Zenith_TestRunner::HandleFailure swallows framework failures and merely
// bumps the hit count, so an in-scope assertion could never red this test; (2) the
// count MUST be copied to a local before the closing brace, because
// ~Zenith_AssertCaptureScope restores the previous hit count. Scopes do not nest.
ZENITH_TEST(ZM_WorldTraversal, Gym1_AccessorsAreTotalOnTheSentinelAndGarbage)
{
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;

		for (u_int u = 0u; u < uGYM1_UNREGISTERED_HEDGE_COUNT; ++u)
		{
			const ZM_GYM1_HEDGE eBad = aeGYM1_UNREGISTERED_HEDGES[u];
			const ZM_Gym1Blockout xBlock = ZM_GetGym1Hedge(eBad);
			const char* szName = ZM_GetGym1HedgeName(eBad);
			const bool bBlocks = ZM_Gym1HedgeBlocksBody(eBad, 0.0f, 0.0f, 1.0f);
			(void)xBlock;
			(void)szName;
			(void)bBlocks;
		}
		for (u_int u = 0u; u < (u_int)ZM_GYM1_BLOCK_COUNT + 4u; ++u)
		{
			const ZM_Gym1Blockout xBlock = ZM_GetGym1Block((ZM_GYM1_BLOCK)u);
			const char* szName = ZM_GetGym1BlockName((ZM_GYM1_BLOCK)u);
			(void)xBlock;
			(void)szName;
		}
		for (u_int u = 0u; u < uGYM1_UNREGISTERED_BADGE_COUNT; ++u)
		{
			const ZM_BadgeData& xBadge = ZM_GetBadgeData(aeGYM1_UNREGISTERED_BADGES[u]);
			const char* szName = ZM_GetBadgeName(aeGYM1_UNREGISTERED_BADGES[u]);
			const bool bRegistered = ZM_IsRegisteredBadge(aeGYM1_UNREGISTERED_BADGES[u]);
			(void)xBadge;
			(void)szName;
			(void)bRegistered;
		}

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"an accessor asserted on a garbage id -- Zenith_Assert breaks in EVERY config, "
		"so this would kill the whole boot unit run rather than fail one test");

	// ...and the answers are the DEGENERATE ones, not plausible-looking rows. A caller
	// that skipped its range check must fail loudly at its own assertion rather than
	// silently matching a real block.
	for (u_int u = 0u; u < uGYM1_UNREGISTERED_HEDGE_COUNT; ++u)
	{
		const ZM_GYM1_HEDGE eBad = aeGYM1_UNREGISTERED_HEDGES[u];
		ZENITH_ASSERT_FALSE(ZM_IsGym1Hedge(eBad),
			"unregistered hedge id %u registered", (u_int)eBad);
		ZENITH_ASSERT_EQ_FLOAT(ZM_GetGym1Hedge(eBad).m_xScale.x, 0.0f, fGYM1_EXACT_EPSILON,
			"unregistered hedge id %u yielded a non-degenerate blockout", (u_int)eBad);
		// ...and, crucially, a degenerate blockout must not BLOCK anything: an all-zero
		// AABB at the origin inflated by a body radius would otherwise swallow the
		// middle of the room.
		ZENITH_ASSERT_FALSE(ZM_Gym1HedgeBlocksBody(eBad, 0.0f, 0.0f, 1.0f),
			"unregistered hedge id %u blocks a body standing at the origin -- the "
			"degenerate row is being treated as geometry", (u_int)eBad);
		ZENITH_ASSERT_STREQ(ZM_GetGym1HedgeName(eBad), "Gym1InvalidHedge",
			"unregistered hedge id %u did not yield the invalid name", (u_int)eBad);
	}

	ZENITH_ASSERT_STREQ(ZM_GetGym1BlockName((ZM_GYM1_BLOCK)ZM_GYM1_BLOCK_COUNT),
		"Gym1InvalidBlock", "an id at the end of the block enum did not yield the "
		"invalid name");
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GetGym1Block((ZM_GYM1_BLOCK)((u_int)ZM_GYM1_BLOCK_COUNT + 3u)).m_xScale.y,
		0.0f, fGYM1_EXACT_EPSILON,
		"an id past the block enum yielded a non-degenerate blockout");
}

// ############################################################################
// 10. The Bloom Badge: an INDEX and a NAME, wired to the existing mask
// ############################################################################

ZENITH_TEST(ZM_Data, Gym1_BadgeTableNamesTheEightRecordedBadges)
{
	// Spelled in the TEST, not read back off the table.
	ZENITH_ASSERT_EQ((u_int)ZM_BADGE_ID_COUNT, uGYM1_EXPECTED_BADGES,
		"the badge enum gained or lost an id -- Docs/GameDesignDocument.md section 3.4 "
		"lists exactly eight");
	ZENITH_ASSERT_EQ((u_int)ZM_BADGE_NONE, (u_int)ZM_BADGE_ID_COUNT,
		"the sentinel must keep aliasing COUNT -- ZM_IsRegisteredBadge rejects both "
		"with one compare");

	// ★ THE MAPPING THAT MAKES AN INDEX MEAN ANYTHING. Docs/SaveFormat.md pins the
	// mask as "Bits 0..7 = badges 1..8", so badge index i is gym i+1 and Gym 1's badge
	// is index ZERO. Spelled here because it is the one claim that turns
	// AwardBadge(0u) from a magic number into a statement about Gym 1.
	ZENITH_ASSERT_EQ((u_int)ZM_BADGE_BLOOM, 0u,
		"the Bloom Badge is not bit 0 -- SaveFormat.md maps bit i to gym i+1, and the "
		"Bloom Badge is Gym 1's (GDD 194)");
	ZENITH_ASSERT_STREQ(ZM_GetBadgeName(ZM_BADGE_BLOOM), "Bloom Badge",
		"Gym 1's badge is not the Bloom Badge");

	for (u_int u = 0u; u < (u_int)ZM_BADGE_ID_COUNT; ++u)
	{
		const ZM_BadgeData& x = ZM_GetBadgeData((ZM_BADGE_ID)u);
		ZENITH_ASSERT_EQ((u_int)x.m_eId, u, "badge row %u has a mismatched m_eId", u);
		ZENITH_ASSERT_NOT_NULL(x.m_szDisplayName, "badge row %u has no name", u);
		ZENITH_ASSERT_TRUE(x.m_szDisplayName != nullptr && x.m_szDisplayName[0] != '\0',
			"badge row %u has an empty name", u);
		ZENITH_ASSERT_TRUE(ZM_IsRegisteredBadge((ZM_BADGE_ID)u),
			"badge %u is in the table but does not register", u);
	}
	for (u_int uA = 0u; uA < (u_int)ZM_BADGE_ID_COUNT; ++uA)
	{
		const char* szA = ZM_GetBadgeData((ZM_BADGE_ID)uA).m_szDisplayName;
		if (szA == nullptr || szA[0] == '\0') { continue; }
		for (u_int uB = uA + 1u; uB < (u_int)ZM_BADGE_ID_COUNT; ++uB)
		{
			const char* szB = ZM_GetBadgeData((ZM_BADGE_ID)uB).m_szDisplayName;
			if (szB == nullptr || szB[0] == '\0') { continue; }
			ZENITH_ASSERT_NE(strcmp(szA, szB), 0,
				"badge rows %u and %u share the name '%s'", uA, uB, szA);
		}
	}

	// The accessors' fail-closed answers.
	ZENITH_ASSERT_STREQ(ZM_GetBadgeName(ZM_BADGE_NONE), "NONE",
		"the sentinel is a legal value to name, not garbage");
	ZENITH_ASSERT_STREQ(ZM_GetBadgeName((ZM_BADGE_ID)9999u), "UNKNOWN",
		"a garbage id must name UNKNOWN");
	ZENITH_ASSERT_FALSE(ZM_IsRegisteredBadge(ZM_BADGE_NONE),
		"the NONE sentinel aliases COUNT and must never register");

	// ONE shared fallback row, never a copy, and never a registered one.
	const ZM_BadgeData* pxFirst = nullptr;
	for (u_int u = 0u; u < uGYM1_UNREGISTERED_BADGE_COUNT; ++u)
	{
		const ZM_BadgeData& x = ZM_GetBadgeData(aeGYM1_UNREGISTERED_BADGES[u]);
		ZENITH_ASSERT_EQ((u_int)x.m_eId, (u_int)ZM_BADGE_NONE,
			"unregistered badge id %u did not yield the UNKNOWN row's id",
			(u_int)aeGYM1_UNREGISTERED_BADGES[u]);
		ZENITH_ASSERT_STREQ(x.m_szDisplayName, "UNKNOWN",
			"unregistered badge id %u did not yield the UNKNOWN row",
			(u_int)aeGYM1_UNREGISTERED_BADGES[u]);
		if (pxFirst == nullptr) { pxFirst = &x; }
		ZENITH_ASSERT_EQ(&x, pxFirst,
			"every unregistered badge id must share ONE fallback row, never a copy");
	}
}

// The DoD's "wired to the EXISTING AwardBadge / HasBadge mask", made mechanical.
// This is NOT a re-test of the mask -- ZM_Tests_SaveModel::Badges_AwardQueryCountAndBounds
// owns that. It is the claim that the NAMED index this slice adds reaches it, and
// that the table cannot outgrow the mask silently.
ZENITH_TEST(ZM_Save, Gym1_BloomBadgeIndexReachesTheExistingBadgeMask)
{
	ZENITH_ASSERT_EQ((u_int)ZM_BADGE_ID_COUNT, uZM_BADGE_COUNT,
		"the badge NAME table and the badge MASK disagree on how many badges the "
		"region has -- an id past the mask's width is an award AwardBadge refuses, "
		"silently, so the player beats the gym and receives nothing");

	ZM_GameState xState;
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 0u, "a fresh state has no badges");
	ZENITH_ASSERT_FALSE(xState.HasBadge((u_int)ZM_BADGE_BLOOM),
		"a fresh state already holds the Bloom Badge");

	ZENITH_ASSERT_TRUE(xState.AwardBadge((u_int)ZM_BADGE_BLOOM),
		"awarding the Bloom Badge by its named index was refused");
	ZENITH_ASSERT_TRUE(xState.HasBadge((u_int)ZM_BADGE_BLOOM),
		"the Bloom Badge did not stick");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 1u,
		"awarding one badge did not leave exactly one");

	// ...and it awarded THAT badge and no other. An index that aliased another gym's
	// bit would satisfy every clause above.
	for (u_int u = 0u; u < (u_int)ZM_BADGE_ID_COUNT; ++u)
	{
		if (u == (u_int)ZM_BADGE_BLOOM) { continue; }
		ZENITH_ASSERT_FALSE(xState.HasBadge(u),
			"awarding the Bloom Badge also set %s", ZM_GetBadgeName((ZM_BADGE_ID)u));
	}

	// The sentinel is refused rather than wrapping onto a real bit.
	ZENITH_ASSERT_FALSE(xState.AwardBadge((u_int)ZM_BADGE_NONE),
		"the badge sentinel was accepted as an award");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 1u,
		"the refused award mutated the mask anyway");
}

// ############################################################################
// 11. The reward trio: leader, badge, teach-move -- all four halves agree
// ############################################################################

// ★ THIS UNIT IS WHY THIS FILE CARRIES DATA CLAIMS AT ALL. Gym 1's reward is spread
// over four tables that no single suite walks together: the trainer row says who and
// what type, the badge table says which badge, the item table says which TM, and the
// move table says what it teaches. Each of those four suites can only prove its own
// row is well-FORMED. Nothing but a cross-table unit can say they are about the
// SAME GYM.
ZENITH_TEST(ZM_Data, Gym1_LeaderBadgeAndTeachMoveAllDescribeTheSameGym)
{
	const ZM_TrainerData& xFenna = ZM_GetTrainerData(ZM_TRAINER_GYM1_FENNA);
	const ZM_ItemData&    xTm    = ZM_GetItemData(ZM_ITEM_TM_VERDANTLASH);
	const ZM_MoveData&    xMove  = ZM_GetMoveData(ZM_MOVE_VERDANTLASH);

	// The TM row, column by column. The roster-wide walks in ZM_Tests_Items prove a TM
	// is well-formed; only this says WHICH move this one teaches.
	ZENITH_ASSERT_STREQ(xTm.m_szName, "TM Verdant Lash", "the reward TM's name changed");
	ZENITH_ASSERT_EQ((u_int)xTm.m_eCategory, (u_int)ZM_ITEM_CATEGORY_TM,
		"the reward is not a TM");
	ZENITH_ASSERT_EQ((u_int)xTm.m_eEffect, (u_int)ZM_ITEM_EFFECT_TEACH_MOVE,
		"the reward TM does not use the EXISTING teach-move effect");
	ZENITH_ASSERT_EQ((u_int)xTm.m_eTaughtMove, (u_int)ZM_MOVE_VERDANTLASH,
		"the reward TM does not teach Verdant Lash");
	ZENITH_ASSERT_FALSE(xTm.m_bConsumable, "TMs in this game are reusable");

	// The move row.
	ZENITH_ASSERT_STREQ(xMove.m_szName, "Verdant Lash", "the reward move's name changed");
	ZENITH_ASSERT_EQ((u_int)xMove.m_eType, (u_int)ZM_TYPE_GRASS,
		"the Grass leader's reward move is not GRASS");

	// ★★ THE TUNING CLAUSE, AND IT IS NOT COSMETIC. ZM_Learnsets.cpp DERIVES every
	// species' level-up list by sorting the damaging moves of its type by power
	// ASCENDING, so adding a GRASS move re-sorts every GRASS species' learnset. Verdant
	// Lash must out-power Leafcut, or it lands among the first TWO same-type picks --
	// which are the only ones a level-5 build reaches -- and silently changes what
	// ZM_BuildWildEnemySpec answers for the starter and for every Route 1 encounter.
	//
	// ★ WHY TWO AND NOT THREE. The cap is 10 + evoStage*2 (12/14/16) and the levels
	// are spread as 1 + k*49/(count-1), so learnset entry k=1 sits at level 5/4/4 and
	// k=2 already at 9/8/7. A level-5 build therefore reaches k=0 and k=1 only, which
	// are auStab[0] and auStab[1]; the third STAB pick is auPicks[3], at level 14/12/10
	// and out of reach. Out-powering Leafcut puts Verdant Lash at sorted index 3 or
	// later, so this bound clears the requirement by one index -- slack that is worth
	// having and worth not mistaking for the requirement itself.
	//
	// Stated as a COMPARISON against the shipped row rather than as a literal 65, so
	// re-tuning Leafcut moves this bound with it.
	ZENITH_ASSERT_GT(xMove.m_uPower, ZM_GetMoveData(ZM_MOVE_LEAFCUT).m_uPower,
		"Verdant Lash (%u power) does not out-power Leafcut (%u), so it may sort into "
		"the first two same-type learnset picks of every GRASS species and move the "
		"moveset of the level-5 starter", xMove.m_uPower,
		ZM_GetMoveData(ZM_MOVE_LEAFCUT).m_uPower);

	// ★ THE CROSS-TABLE CLAIM. The badge, the leader's team and the teach-move must all
	// be about the same gym: GRASS, Gym 1, and the flag S6 reserved for it.
	ZENITH_ASSERT_EQ((u_int)xFenna.m_eDefeatFlag, (u_int)ZM_STORY_FLAG_GYM1_DEFEATED,
		"the leader whose reward this is does not write the Gym 1 flag");
	ZENITH_ASSERT_STREQ(ZM_GetBadgeName(ZM_BADGE_BLOOM), "Bloom Badge",
		"Gym 1's badge is not the Bloom Badge");

	ZENITH_ASSERT_NOT_NULL(xFenna.m_paxParty, "the leader brings no party array");
	ZENITH_ASSERT_GT(xFenna.m_uPartyCount, 0u,
		"the leader brings no team, so the type claim below is vacuous");
	if (xFenna.m_paxParty != nullptr && xFenna.m_uPartyCount > 0u
		&& xFenna.m_uPartyCount <= uZM_TRAINER_MAX_PARTY)
	{
		for (u_int uSlot = 0u; uSlot < xFenna.m_uPartyCount; ++uSlot)
		{
			const ZM_SPECIES_ID eSpecies = xFenna.m_paxParty[uSlot].m_eSpecies;
			if ((u_int)eSpecies >= (u_int)ZM_SPECIES_COUNT)
			{
				continue;   // ZM_GetSpeciesData ASSERTS out of range; the walk stops here
			}
			ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eSpecies).m_aeTypes[0],
				(u_int)xMove.m_eType,
				"the leader's slot-%u monster and her reward move disagree on type -- a "
				"gym whose badge, team and teach-move are not the same element is three "
				"unrelated rewards wearing one name", uSlot);
		}
	}

	// NOT asserted here, deliberately: "GYM1_DEFEATED still owns wire bit 5". That is a
	// ZM_StoryFlags property with its OWN pin (StoryFlags_WireBitsAreFrozen,
	// Tests/ZM_Tests_StoryFlags.cpp), and re-stating it here could only ever add a
	// second red to a defect in that table while naming Gym 1's reward as the culprit.
	// What this file legitimately owns is the sentence above it -- that the leader, the
	// badge and the teach-move are about the same gym.
	//
	// ★ AND THE THING THIS SLICE MUST NOT HAVE DONE, recorded for a reader rather than
	// asserted, because nothing mechanical can see the absence of a column: G1-3
	// (ZM-70) awards the badge and the teach-move on a leader win. G1-1 only makes them
	// NAMEABLE. If you are here adding a badge column to ZM_TrainerData, or a trainer
	// column to ZM_BadgeData, the three-slice split (ZM-D-209) has been collapsed --
	// which may well be right, but it is a decision, not a tidy-up.
}
