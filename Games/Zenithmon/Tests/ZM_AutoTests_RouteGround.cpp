#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_RouteGround (S8 item 2, R1-2 step 2) -- the TWO REGION GROUND
// ORACLES behind the Route 1 / Thornacre authoring.
//
//   ZM_Route1GroundTruth_Test     -- casts a real downward ray at every column
//   ZM_ThornacreGroundTruth_Test     the region's placement header anchors, LOGS
//                                    every value at LOG_CATEGORY_UNITTEST on
//                                    every run, and reds when a compiled row has
//                                    drifted from the surface the world actually
//                                    has.
//
// They are the same instrument the Dawnmere oracles in
// Tests/ZM_AutoTests_CameraClearance.cpp are (ZM_DawnmereHomeGroundTruth_Test,
// ZM_DawnmereLabGroundTruth_Test and -- the closest model, written two commits
// ago for exactly this purpose -- ZM_DawnmereRouteSeamGroundTruth_Test). The
// phase enum, the deadline constants, the maxFrames backstop, the
// RequestSkip-before-any-state ordering, the per-row Verify triple and the
// PASTE-line format are all deliberately the same shape. What could NOT be
// reused is the code: every CC* fixture in that file is static to that TU, so
// the equivalents below are re-written, with their SEMANTICS preserved.
//
// ★★ THESE TWO TESTS ARE RED FROM THE MOMENT THEY LAND, AND THAT IS CORRECT.
// Assets/Scenes/Route1.zscen and Assets/Scenes/Thornacre.zscen do not exist yet
// -- the orchestrator's authoring boot creates them in the NEXT step of this
// slice. On a machine that has the Route 1 / Thornacre terrain bake, each test
// therefore FAILS on "the committed scene is missing" until then. It is not
// skipped, on purpose: see the skip-vs-fail star below. On a machine with no
// bake at all both SKIP, and a skip counts as a PASS, so nothing is measured and
// nothing may be frozen from that run.
//
// ★★ WHAT THEY EXIST TO PRODUCE. Since R1-2 each placement header carries TWO
// DIFFERENT GROUND CLAIMS, and only the second of them is this file's business:
//
//   (a) A RECIPE MIRROR -- fZM_ROUTE1_RECIPE_TARGET_GROUND_Y (26.0) and
//       fZM_THORNACRE_RECIPE_TARGET_GROUND_Y (28.0) -- which MIRRORS its region's
//       terrain recipe m_fTargetHeight: the height every FLATTEN dab drives its
//       pads and lanes TO. It is exact, it is not a measurement, and its readers
//       are the boot units that reconcile it against the recipe.
//       ★ NOTHING THIS FILE PRINTS MAY EVER BE PASTED INTO IT. A measured value
//       there turns a recipe-vs-recipe reconciliation into a recipe-vs-raycast
//       comparison, which reds for a reason its message does not name.
//
//   (b) A MEASURED TABLE, ONE ROW PER COLUMN -- axZM_ROUTE1_GROUND_SAMPLES (six
//       rows: two arrivals, two gates, two trainer stations) and
//       axZM_THORNACRE_GROUND_SAMPLES (two: the arrival and the return gate).
//       Everything that means "the real surface under THIS anchor" derives from
//       these. THEY are what the rows below measure, and each row's own m_fFeetY
//       is the one number its `paste=` literal replaces.
//
// ★★ EVERY ROW OF BOTH TABLES IS STILL SEEDED FROM ITS REGION'S MIRROR, WHICH IS
// WHY NO COMPILED-CONSTANT UNIT IN THIS GAME CAN TELL THEM APART FROM FROZEN ONES.
// A boot unit asserting "the rows sit near the recipe target" passes VACUOUSLY
// today and would keep passing if the freeze never happened. A real downward
// raycast against a baked terrain body is the only instrument that can see it, and
// this file is that instrument.
//
// R1-2 step 1 measured Dawnmere's route seam and found target + 0.366 there,
// BECAUSE that column sits inside a flatten corridor; the ~+2 m gap its neighbours
// read is a property of UNFLATTENED ground. Every column measured below is likewise
// asserted (by the R1-1 boot units) to lie inside a flattened pad or a lane's
// flatten corridor, so the seeds are expected to be good to well under a metre --
// but "expected" is not "measured", and the whole point of this file is to replace
// the one with the other.
//
// ★ SO THE REAL OUTPUT OF A RUN IS THE PER-COLUMN SPREAD -- AND IT IS PER COLUMN
// PRECISELY BECAUSE THESE COLUMNS ARE NOT ONE NUMBER. Route 1's two arrivals stand
// 1312 m apart on eroded terrain and its two gates 1336 m. A run where all six
// columns measure within a few centimetres of each other says "the flatten
// corridors did their job, freeze the set"; a run where ONE column disagrees with
// its neighbours by metres says "that anchor is outside the flattened region its
// header claims" -- which is a FINDING about the placement, not a tolerance to
// widen.
//
// ★★ AND A SET IS FROZEN AS A SET, ROW BY ROW. Pasting one measured value into one
// shared scalar is exactly the per-column collapse the R1-2 split exists to
// prevent, which is why every PASTE line below names a single indexed table row
// rather than a constant.
//
// ★★ THE PLAYER-HIT GUARD IS THE WHOLE POINT OF THE PROBE, AND IT IS WHY EVERY
// PASTE LINE NAMES THE ENTITY THE RAY STOPPED ON. A downward ray that terminates
// on the authored Player capsule (or on a trainer, or on a gate box) reports that
// BODY's top as if it were the ground. Freeze that into a placement header and
// every compiled-constant unit downstream stays green while the real anchors
// float a body-height above the world or sink into it. So: each row carries the
// ONE body that may legitimately stand on its own column, that body is the
// raycast's ignore entity, "hit terrain" is set ONLY when the hit entity IS the
// region's terrain entity, and ANY other hit is TERMINAL and is NAMED in the
// failure. There is no silent pass, and no row is judged on a hit it cannot
// identify.
//
// ★ AN ABSENT IGNORE BODY IS "THERE IS NOTHING TO IGNORE", NEVER A FAILURE.
// Zenith_PhysicsQuery::RaycastIgnoring falls back to an unfiltered raycast for
// INVALID_ENTITY_ID -- "ignore nothing" is a supported request. Only the TERRAIN
// is required: it is what every probe must terminate on, and its absence means
// the committed scene is not the scene this test was written against.
//
// ★★ THE RAY WINDOW IS SIZED FROM THE RECIPE'S TARGET HEIGHT, NOT FROM THE ROW
// BEING MEASURED, AND ON THESE TWO REGIONS THAT IS LOAD-BEARING RATHER THAN
// TIDY. A window centred on the row would make a wrong row un-fixable: the ray
// would start in the wrong place, find nothing, and time out at the phase
// deadline WITHOUT EVER PRINTING A MEASUREMENT -- i.e. the measure -> freeze ->
// rebuild loop could never close. (That is why the lab and route-seam oracles
// size theirs from Dawnmere's town-centre anchor.) Here the reference is read
// straight off the recipe, so it cannot drift from the terrain it is describing.
// The start height clears the tallest thing either recipe builds -- Route 1's
// tallest landform is 45 m over a 26 m target, Thornacre's is 48 m over 28 m --
// so a ray never BEGINS INSIDE terrain, which is the one starting condition a
// convex ray cast answers ambiguously, and it still reaches far below the target.
//
// ★ SKIPS ON THE GITIGNORED BAKE ONLY. The heightfield is machine-local, so "no
// bake" genuinely means "there is nothing to measure against" and a skip is
// honest. A missing COMMITTED .zscen is a different kind of thing -- and note a
// skip counts as a PASS in CI, so skipping on it would turn a deleted (or, right
// now, an un-authored) tracked asset into a green run on the exact tree where
// that must never happen. The shipped Dawnmere oracles draw the line in the same
// place. Bake presence is asked of the SHIPPED predicate ZM_IsTerrainBakeWarm
// rather than a hand-listed file set, so no path is re-spelled here; note that a
// bake with a STALE manifest stamp answers false and therefore SKIPS, which is
// right -- a rejected chunk leaves the terrain with no physics body at all, so
// there would again be nothing to measure.
//
// ★ NOTHING IN THIS FILE AUTHORS, MOVES OR TELEPORTS ANYTHING. It loads the
// committed scene and casts rays. In particular, MEASURING A GATE COLUMN IS NOT
// AUTHORING A TRIGGER: the gate rows below exist so each sensor box's OWN column --
// the row its centre height is derived from -- is measured BEFORE R1-3 authors it,
// which is the same ordering that put the route-seam oracle a commit ahead of the
// marker it serves.
//
// Both run on the NULL backend (m_bRequiresGraphics = false): they need physics
// and a terrain bake, not a swapchain.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_Route1Placement.h"
#include "Zenithmon/Source/World/ZM_SceneRegistry.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"
#include "Zenithmon/Source/World/ZM_ThornacrePlacement.h"

#include <cmath>
#include <cstdio>       // snprintf -- the resolved-hit entity descriptions
#include <filesystem>
#include <string>

namespace
{
	// ---- Shared constants ---------------------------------------------------

	constexpr float fRG_FIXED_DT = 1.0f / 60.0f;

	// The SAME tolerance the shipped Dawnmere oracles use, so "the compiled
	// height is stale" means the same thing in every ground table in this game.
	constexpr float fRG_HEIGHT_TOLERANCE = 0.15f;

	// The probe window, stated RELATIVE TO THE RECIPE'S TARGET HEIGHT -- see the
	// window star in the file banner for why it is not relative to the row.
	//   Route 1:   26 + 60 = 86 down to -34   (tallest landform 45)
	//   Thornacre: 28 + 60 = 88 down to -32   (tallest landform 48)
	constexpr float fRG_RAY_START_ABOVE_TARGET = 60.0f;
	constexpr float fRG_RAY_MAX_DISTANCE = 120.0f;

	// Both waiting phases own their own deadline, and each FAILS with a
	// diagnostic naming what it was waiting for. Same figures as the shipped
	// oracles.
	constexpr int iRG_RESOLVE_DEADLINE_FRAMES = 900;
	constexpr int iRG_PROBE_DEADLINE_FRAMES = 900;

	// ---- Shared fixtures ----------------------------------------------------

	bool RGFilePresentAndNonEmpty(const std::string& strPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(strPath, xError) || xError)
		{
			return false;
		}
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
		return !xError && ulSize != 0u;
	}

	// The committed scene's path, built from the ONE enumerable stem inventory
	// (Source/World/ZM_SceneRegistry.h) rather than re-spelled here. A scene with
	// no registration answers "", which the caller reports as its own failure --
	// an unregistered scene is a permanent-black-screen defect in its own right
	// (IsWarpDestinationValid never consults the registry), not a missing file.
	std::string RGCommittedScenePath(ZM_SCENE_ID eScene)
	{
		const char* szStem = ZM_FindSceneFileStem(eScene);
		if (szStem[0] == '\0')
		{
			return std::string();
		}
		return std::string(GAME_ASSETS_DIR) + "Scenes/" + szStem + ZENITH_SCENE_EXT;
	}

	// A NAME for an entity id that is safe to print even when the id no longer
	// resolves -- a raw "%u:%u" is still information, an empty string is not.
	void RGDescribeEntity(Zenith_EntityID xID, char (&acOut)[96])
	{
		if (xID == INVALID_ENTITY_ID)
		{
			std::snprintf(acOut, sizeof(acOut), "<no entity>");
			return;
		}
		const Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		if (!xEntity.IsValid())
		{
			std::snprintf(acOut, sizeof(acOut), "<unresolved id %u:%u>",
				xID.m_uIndex, xID.m_uGeneration);
			return;
		}
		std::snprintf(acOut, sizeof(acOut), "%s", xEntity.GetName().c_str());
	}

	struct RGGroundProbe
	{
		// TERMINAL: this column has an answer and retrying cannot change it.
		bool m_bResolved = false;
		bool m_bHitTerrain = false;
		float m_fFeetY = -1.0e9f;              // fails closed
		Zenith_EntityID m_xFinalHitEntity = INVALID_ENTITY_ID;
	};

	// ONE downward cast per column, and exactly one.
	//
	// ★ THE TWO OUTCOMES ARE DELIBERATELY DIFFERENT KINDS OF THING, because
	// conflating them turns a five-frame diagnosis into a 900-frame timeout that
	// blames the wrong subsystem:
	//   * HIT SOMETHING THAT IS NOT THE TERRAIN -> TERMINAL, and a failure. The
	//     column is under a solid body, so the measurement would be a body TOP;
	//     no amount of waiting changes that, and the blocking entity is NAMED.
	//   * HIT NOTHING AT ALL -> NOT terminal. The terrain physics body arrives
	//     with streaming rather than with the scene, so "no ground yet" is a
	//     WAIT, and only the phase deadline turns it into a failure.
	//
	// The one body that may legitimately stand on a column is handled by the
	// caller's per-row ignore entity, NOT by stepping through hits: stepping past
	// an entry point lands the next cast INSIDE the same convex box, where a ray
	// cast reports no hit at all, and the probe would then mis-report "the
	// terrain never streamed in".
	RGGroundProbe RGProbeGroundAt(float fX, float fZ, float fRecipeTargetHeight,
		Zenith_EntityID xIgnoreEntity, Zenith_EntityID xTerrainEntity)
	{
		RGGroundProbe xProbe;
		const Zenith_Maths::Vector3 xOrigin(
			fX, fRecipeTargetHeight + fRG_RAY_START_ABOVE_TARGET, fZ);
		const Zenith_Physics::RaycastResult xHit =
			Zenith_PhysicsQuery::RaycastIgnoring(
				xOrigin, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f),
				fRG_RAY_MAX_DISTANCE, xIgnoreEntity);
		if (!xHit.m_bHit)
		{
			return xProbe;
		}

		xProbe.m_bResolved = true;
		xProbe.m_xFinalHitEntity = xHit.m_xHitEntity;
		// ★ "hit terrain" ONLY when the hit entity IS this region's terrain. A
		// body top is never accepted as a ground height, however plausible its
		// value looks.
		if (xHit.m_xHitEntity == xTerrainEntity
			&& xTerrainEntity != INVALID_ENTITY_ID)
		{
			xProbe.m_bHitTerrain = true;
			xProbe.m_fFeetY = xHit.m_xHitPoint.y;
		}
		return xProbe;
	}

	Zenith_Entity RGFindEntity(Zenith_SceneData* pxData, const char* szName)
	{
		return pxData != nullptr && szName != nullptr && szName[0] != '\0'
			? pxData->FindEntityByName(szName)
			: Zenith_Entity();
	}

	// ---- One measured column ------------------------------------------------

	struct RGRow
	{
		// The row key the PASTE line is keyed on -- THE PLACEMENT TABLE'S OWN KEY,
		// read from that table rather than re-typed -- and the exact indexed table
		// ROW the measured literal is destined for. Both are printed on every run.
		//
		// ★ THE FREEZE TARGET NAMES A ROW, NEVER A SCALAR. A `paste=` literal that
		// told an operator to write one measured value into one shared constant would
		// re-collapse the per-column table the R1-2 split created, and would do it
		// with the operator following instructions correctly.
		const char* m_szRowName = "";
		const char* m_szFreezeTarget = "";
		// The ONE body that may legitimately stand on THIS column. "" means
		// "nothing may" -- and an absent named body is not a failure either.
		const char* m_szIgnoreEntityName = "";
		float m_fX = 0.0f;
		float m_fZ = 0.0f;
		// What the compiled placement header currently claims the ground here is.
		float m_fTableY = 0.0f;
		Zenith_EntityID m_xIgnoreEntityID = INVALID_ENTITY_ID;
		bool m_bIgnoreEntityPresent = false;
		RGGroundProbe m_xProbe;
	};

	// ★ THE BOUND IS TIED TO THE TWO MEASURED TABLES, NOT PICKED. Each region's rows
	// ARE that region's ground-sample table, so appending a column to either header
	// either fits or reds AT COMPILE TIME rather than silently dropping the new
	// column past the slot bound -- which is the truncation hazard the lab oracle
	// documents in the shipped file, and which would leave a real anchor unmeasured
	// with this test still green.
	constexpr u_int uRG_MAX_ROWS = 8u;
	static_assert(uRG_MAX_ROWS >= (u_int)ZM_ROUTE1_GROUND_SAMPLE_COUNT,
		"the Route 1 ground oracle needs one slot per row of "
		"axZM_ROUTE1_GROUND_SAMPLES");
	static_assert(uRG_MAX_ROWS >= (u_int)ZM_THORNACRE_GROUND_SAMPLE_COUNT,
		"the Thornacre ground oracle needs one slot per row of "
		"axZM_THORNACRE_GROUND_SAMPLES");

	// ---- One region's whole run ---------------------------------------------

	enum class RGPhase { Resolve, Measure, Done };

	struct RGRegion
	{
		// -- configuration, spelled once at each test's registration --
		const char* m_szTestName = "";
		const char* m_szPlacementHeader = "";
		const char* m_szTerrainEntityName = "";
		ZM_SCENE_ID m_eScene = ZM_SCENE_NONE;
		void (*m_pfnBuildRows)(RGRegion&) = nullptr;

		// -- resolved from the compiled tables at Setup --
		const ZM_TerrainAuthoringRecipe* m_pxRecipe = nullptr;
		float m_fRecipeTargetHeight = 0.0f;
		u_int m_uBuildIndex = 0xFFFFFFFFu;

		RGRow m_axRows[uRG_MAX_ROWS];
		u_int m_uRowCount = 0u;
		bool m_bRowOverflow = false;

		// -- run state --
		RGPhase m_ePhase = RGPhase::Done;
		int m_iFrames = 0;
		bool m_bBakePresent = false;
		bool m_bScenePresent = false;
		bool m_bSkipped = false;
		bool m_bResolved = false;
		bool m_bMeasured = false;
		Zenith_EntityID m_xTerrainID = INVALID_ENTITY_ID;
		const char* m_szFailure = "test did not reach verification";
	};

	void RGFail(RGRegion& xRegion, const char* szReason)
	{
		xRegion.m_szFailure = szReason;
		xRegion.m_ePhase = RGPhase::Done;
	}

	// Overflow is RECORDED and reported as a failure rather than silently
	// truncating coverage -- a shorter table would still pass, having measured
	// less.
	void RGAddRow(RGRegion& xRegion, const char* szRowName,
		const char* szFreezeTarget, const char* szIgnoreEntityName,
		float fX, float fZ, float fTableY)
	{
		if (xRegion.m_uRowCount >= uRG_MAX_ROWS)
		{
			xRegion.m_bRowOverflow = true;
			return;
		}
		RGRow& xRow = xRegion.m_axRows[xRegion.m_uRowCount++];
		xRow.m_szRowName = szRowName;
		xRow.m_szFreezeTarget = szFreezeTarget;
		xRow.m_szIgnoreEntityName = szIgnoreEntityName;
		xRow.m_fX = fX;
		xRow.m_fZ = fZ;
		xRow.m_fTableY = fTableY;
	}

	// ---- The two row tables -------------------------------------------------
	//
	// ★★ THE ROWS **ARE** THE PLACEMENT HEADER'S MEASURED TABLE, WALKED. A row's
	// key, its XZ and the figure it is judged against all come out of the very row
	// its `paste=` literal replaces, so none of the three can drift from the others.
	// An earlier draft typed its own keys -- "SouthArrival", "SouthGate" -- which
	// matched NO row of either table: both are keyed by ENTITY NAME
	// ("Route1SouthArrival", "ThornacreSouthGate"), so every printed `name=` was a
	// key an operator could not look up and the paste mapping survived on positional
	// order alone. Walking the table removes the re-typing that made that possible.
	//
	// ★ AND THAT IS ALSO WHY NO COORDINATE OR HEIGHT IS SPELLED HERE. A constant
	// spelled twice cannot red a drift: both sides would move together and the whole
	// oracle would become decorative.
	//
	// ★ WHAT THIS FILE DOES **NOT** MEASURE, SAID PLAINLY SO NOBODY ADDS IT BACK:
	// the DERIVED heights -- a marker's feet, a gate box's Min().y, a trainer's
	// resting centre. Since the R1-2 split every one of them is defined as its own
	// column's row plus a compiled offset, and the boot units in
	// Tests/ZM_Tests_Route1Placement.cpp and Tests/ZM_Tests_ThornacrePlacement.cpp
	// reconcile those derivations against those same rows. The subject HERE is the
	// ROW: the one number a freeze actually rewrites.

	// The ONE body that may legitimately stand on a Route 1 column.
	//
	// ★ EACH TRAINER IGNORES HIMSELF, AND ONLY HIMSELF. He is a DYNAMIC capsule
	// authored standing on his own anchor, so his own body is the one thing over that
	// column -- and a ray that stopped on it would freeze his shoulder height into the
	// ground table. Ignoring the Player at his column instead would leave the trainer
	// in the way; ignoring a trainer at somebody else's column would hide a real
	// obstruction. Every other column names the authored Player: he stands on the
	// SOUTH arrival, and once R1-3's return leg lands a warp puts a capsule on the
	// north one too -- the self-confirming-measurement hazard the Dawnmere Home oracle
	// guards against at the town centre. Naming him on the gate columns costs nothing
	// today and keeps them measurable afterwards.
	//
	// TOTAL, and a SWITCH rather than a positional array on purpose: a reordering of
	// ZM_ROUTE1_GROUND_SAMPLE cannot pair a column with somebody else's body.
	const char* RGRoute1IgnoreEntityName(ZM_ROUTE1_GROUND_SAMPLE eSample)
	{
		switch (eSample)
		{
		case ZM_ROUTE1_GROUND_SAMPLE_TRAINER_RAMBLER:
			return szZM_ROUTE1_TRAINER_RAMBLER_ENTITY_NAME;
		case ZM_ROUTE1_GROUND_SAMPLE_TRAINER_FORAGER:
			return szZM_ROUTE1_TRAINER_FORAGER_ENTITY_NAME;
		default: break;
		}
		return szZM_ROUTE1_PLAYER_ENTITY_NAME;
	}

	// The EXACT table row a `paste=` literal replaces, spelled the way an operator
	// has to type it. It is printed verbatim in the PASTE log AND in the
	// tolerance-failure message, so it is an INSTRUCTION, not a label: it must name
	// something that exists and something that is per-column. Naming a shared scalar
	// here (as the pre-split spellings did) would tell an operator to collapse six
	// measured columns back into one number and would look like a correct freeze.
	//
	// TOTAL, and a switch for the same reason as above.
	const char* RGRoute1FreezeTarget(ZM_ROUTE1_GROUND_SAMPLE eSample)
	{
		switch (eSample)
		{
		case ZM_ROUTE1_GROUND_SAMPLE_SOUTH_ARRIVAL:
			return "axZM_ROUTE1_GROUND_SAMPLES["
				"ZM_ROUTE1_GROUND_SAMPLE_SOUTH_ARRIVAL].m_fFeetY";
		case ZM_ROUTE1_GROUND_SAMPLE_NORTH_ARRIVAL:
			return "axZM_ROUTE1_GROUND_SAMPLES["
				"ZM_ROUTE1_GROUND_SAMPLE_NORTH_ARRIVAL].m_fFeetY";
		case ZM_ROUTE1_GROUND_SAMPLE_SOUTH_GATE:
			return "axZM_ROUTE1_GROUND_SAMPLES["
				"ZM_ROUTE1_GROUND_SAMPLE_SOUTH_GATE].m_fFeetY";
		case ZM_ROUTE1_GROUND_SAMPLE_NORTH_GATE:
			return "axZM_ROUTE1_GROUND_SAMPLES["
				"ZM_ROUTE1_GROUND_SAMPLE_NORTH_GATE].m_fFeetY";
		case ZM_ROUTE1_GROUND_SAMPLE_TRAINER_RAMBLER:
			return "axZM_ROUTE1_GROUND_SAMPLES["
				"ZM_ROUTE1_GROUND_SAMPLE_TRAINER_RAMBLER].m_fFeetY";
		case ZM_ROUTE1_GROUND_SAMPLE_TRAINER_FORAGER:
			return "axZM_ROUTE1_GROUND_SAMPLES["
				"ZM_ROUTE1_GROUND_SAMPLE_TRAINER_FORAGER].m_fFeetY";
		default: break;
		}
		return "<no such row of axZM_ROUTE1_GROUND_SAMPLES -- freeze nothing>";
	}

	const char* RGThornacreFreezeTarget(ZM_THORNACRE_GROUND_SAMPLE eSample)
	{
		switch (eSample)
		{
		case ZM_THORNACRE_GROUND_SAMPLE_SOUTH_ARRIVAL:
			return "axZM_THORNACRE_GROUND_SAMPLES["
				"ZM_THORNACRE_GROUND_SAMPLE_SOUTH_ARRIVAL].m_fFeetY";
		case ZM_THORNACRE_GROUND_SAMPLE_SOUTH_GATE:
			return "axZM_THORNACRE_GROUND_SAMPLES["
				"ZM_THORNACRE_GROUND_SAMPLE_SOUTH_GATE].m_fFeetY";
		default: break;
		}
		return "<no such row of axZM_THORNACRE_GROUND_SAMPLES -- freeze nothing>";
	}

	void RGBuildRoute1Rows(RGRegion& xRegion)
	{
		// Six columns: two warp arrivals, two gates and one per trainer station,
		// walked in the table's own order. Appending a column to the header adds its
		// row here with no edit at all -- and if it does not fit the slot budget the
		// static_assert above reds instead of the coverage silently shrinking.
		for (u_int uSample = 0u; uSample < ZM_GetRoute1GroundSampleCount(); ++uSample)
		{
			const ZM_ROUTE1_GROUND_SAMPLE eSample =
				(ZM_ROUTE1_GROUND_SAMPLE)uSample;
			const ZM_Route1GroundSample& xSample = ZM_GetRoute1GroundSample(eSample);
			RGAddRow(xRegion, xSample.m_szEntityName,
				RGRoute1FreezeTarget(eSample), RGRoute1IgnoreEntityName(eSample),
				xSample.m_fX, xSample.m_fZ, xSample.m_fFeetY);
		}
	}

	void RGBuildThornacreRows(RGRegion& xRegion)
	{
		// ★ TWO ROWS, AND NO THIRD. Thornacre is a TRAVERSAL STUB by ruling
		// (ZM-D-196): terrain, one arrival, a player, a camera and -- in a LATER
		// slice -- one return trigger. The player and camera stand on the arrival
		// column, so they need no rows of their own, and there is deliberately no
		// gym anything to measure. The loop bound is the GROUND-SAMPLE count, never
		// uZM_THORNACRE_PLACEMENT_ENTITY_COUNT: that literal counts authored
		// ENTITIES (index 4 of which is the trigger this milestone does not author),
		// and a column is not an entity.
		for (u_int uSample = 0u; uSample < ZM_GetThornacreGroundSampleCount();
			++uSample)
		{
			const ZM_THORNACRE_GROUND_SAMPLE eSample =
				(ZM_THORNACRE_GROUND_SAMPLE)uSample;
			const ZM_ThornacreGroundSample& xSample =
				ZM_GetThornacreGroundSample(eSample);
			// ONE BODY FOR BOTH COLUMNS: the stub authors no NPC at all, and the
			// authored Player stands on the arrival 12 m from the gate. The return
			// gate's own ground is measured BEFORE the sensor exists -- see the
			// "measuring a gate column is not authoring a trigger" star above.
			RGAddRow(xRegion, xSample.m_szEntityName,
				RGThornacreFreezeTarget(eSample), szZM_THORNACRE_PLAYER_ENTITY_NAME,
				xSample.m_fX, xSample.m_fZ, xSample.m_fFeetY);
		}
	}

	// ---- The shared driver --------------------------------------------------

	// ★ GetSceneInfo's build index is INT and the world table's is U_INT. The
	// comparison is spelled with an explicit range check and cast so /W4 /WX has
	// nothing to say and a negative (i.e. "no scene") can never alias a valid
	// index.
	bool RGRegionSceneIsActive(const RGRegion& xRegion, Zenith_SceneData*& pxDataOut)
	{
		pxDataOut = g_xEngine.Scenes().GetActiveSceneData();
		if (pxDataOut == nullptr)
		{
			return false;
		}
		const int iActiveBuildIndex = g_xEngine.Scenes().GetSceneInfo(
			g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
		return iActiveBuildIndex >= 0
			&& (u_int)iActiveBuildIndex == xRegion.m_uBuildIndex;
	}

	bool RGStepResolve(RGRegion& xRegion)
	{
		Zenith_SceneData* pxData = nullptr;
		if (!RGRegionSceneIsActive(xRegion, pxData))
		{
			if (xRegion.m_iFrames > iRG_RESOLVE_DEADLINE_FRAMES)
			{
				RGFail(xRegion, "the region's scene never became the active scene");
				return false;
			}
			return true;
		}

		// The terrain is the ONLY required entity: it is what every probe must
		// terminate on, and its absence means the committed scene is not the
		// scene this test was written against.
		const Zenith_Entity xTerrain =
			RGFindEntity(pxData, xRegion.m_szTerrainEntityName);
		if (!xTerrain.IsValid())
		{
			if (xRegion.m_iFrames > iRG_RESOLVE_DEADLINE_FRAMES)
			{
				RGFail(xRegion, "the committed scene does not contain the region's "
					"terrain entity -- a renamed or deleted authored entity, not a "
					"missing bake");
				return false;
			}
			return true;
		}
		xRegion.m_xTerrainID = xTerrain.GetEntityID();

		// OPTIONAL, every one of them. An absent body is "there is nothing to
		// ignore", which RaycastIgnoring accepts as INVALID_ENTITY_ID.
		for (u_int u = 0u; u < xRegion.m_uRowCount; ++u)
		{
			RGRow& xRow = xRegion.m_axRows[u];
			const Zenith_Entity xIgnore =
				RGFindEntity(pxData, xRow.m_szIgnoreEntityName);
			xRow.m_bIgnoreEntityPresent = xIgnore.IsValid();
			xRow.m_xIgnoreEntityID = xRow.m_bIgnoreEntityPresent
				? xIgnore.GetEntityID()
				: INVALID_ENTITY_ID;
		}

		xRegion.m_bResolved = true;
		xRegion.m_ePhase = RGPhase::Measure;
		xRegion.m_iFrames = 0;
		return true;
	}

	bool RGStepMeasure(RGRegion& xRegion)
	{
		u_int uResolved = 0u;
		for (u_int u = 0u; u < xRegion.m_uRowCount; ++u)
		{
			RGRow& xRow = xRegion.m_axRows[u];
			if (xRow.m_xProbe.m_bResolved)
			{
				++uResolved;
				continue;
			}
			// The reference height sizes the WINDOW only, and it is deliberately
			// the RECIPE's target rather than xRow.m_fTableY -- see the banner.
			xRow.m_xProbe = RGProbeGroundAt(xRow.m_fX, xRow.m_fZ,
				xRegion.m_fRecipeTargetHeight, xRow.m_xIgnoreEntityID,
				xRegion.m_xTerrainID);
			if (xRow.m_xProbe.m_bResolved)
			{
				++uResolved;
			}
		}

		// Only columns that found NOTHING AT ALL are still waiting for the terrain
		// body to stream in; a column that landed on the wrong body already has
		// its answer and is judged in Verify.
		if (uResolved != xRegion.m_uRowCount)
		{
			if (xRegion.m_iFrames > iRG_PROBE_DEADLINE_FRAMES)
			{
				RGFail(xRegion, "a downward probe found no ground at all under a "
					"placement column -- either the terrain physics body never "
					"streamed in, or that column lies outside the baked heightfield "
					"(per-column detail below)");
				return false;
			}
			return true;
		}

		xRegion.m_bMeasured = true;
		xRegion.m_ePhase = RGPhase::Done;
		return false;
	}

	void RGSetupRegion(RGRegion& xRegion)
	{
		// Everything except the configuration fields, which the registration
		// below owns and this function must not touch.
		xRegion.m_pxRecipe = nullptr;
		xRegion.m_fRecipeTargetHeight = 0.0f;
		xRegion.m_uBuildIndex = 0xFFFFFFFFu;
		xRegion.m_uRowCount = 0u;
		xRegion.m_bRowOverflow = false;
		for (u_int u = 0u; u < uRG_MAX_ROWS; ++u)
		{
			xRegion.m_axRows[u] = RGRow();
		}
		xRegion.m_ePhase = RGPhase::Done;
		xRegion.m_iFrames = 0;
		xRegion.m_bBakePresent = false;
		xRegion.m_bScenePresent = false;
		xRegion.m_bSkipped = false;
		xRegion.m_bResolved = false;
		xRegion.m_bMeasured = false;
		xRegion.m_xTerrainID = INVALID_ENTITY_ID;
		xRegion.m_szFailure = "test did not reach verification";

		Zenith_InputSimulator::ResetAllInputState();

		// ★ ZM_GetWorldSpec ASSERTS FATALLY out of range and Zenith_Assert breaks
		// in EVERY configuration, so the id is range-checked BEFORE it is used.
		if (xRegion.m_eScene >= ZM_SCENE_COUNT || xRegion.m_pfnBuildRows == nullptr)
		{
			RGFail(xRegion, "the region is misconfigured (no scene id or no row "
				"builder) -- nothing was measured");
			return;
		}
		xRegion.m_uBuildIndex = ZM_GetWorldSpec(xRegion.m_eScene).m_uBuildIndex;

		// The compiled terrain recipe is what sizes the ray window. Its absence is
		// a DEFECT (a region with anchors but no recipe), not a missing asset.
		xRegion.m_pxRecipe = ZM_FindTerrainAuthoringRecipe(xRegion.m_eScene);
		if (xRegion.m_pxRecipe == nullptr)
		{
			RGFail(xRegion, "no compiled terrain recipe exists for this region, so "
				"the probe window has no reference height -- a table defect, not a "
				"missing bake");
			return;
		}
		xRegion.m_fRecipeTargetHeight = xRegion.m_pxRecipe->m_fTargetHeight;

		xRegion.m_pfnBuildRows(xRegion);

		// ★ THE ONE SKIP, AND IT IS NARROW ON PURPOSE: the GITIGNORED heightfield
		// this test measures against does not exist (or its manifest stamp is
		// stale, which leaves the terrain with no physics body and therefore
		// nothing to measure either). RequestSkip bypasses Verify, so no fixed-dt
		// and no scene-load state may be installed before this point.
		xRegion.m_bBakePresent = ZM_IsTerrainBakeWarm(
			*xRegion.m_pxRecipe, std::filesystem::path(GAME_ASSETS_DIR));
		if (!xRegion.m_bBakePresent)
		{
			xRegion.m_bSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_RouteGround] this region's terrain bake is absent, incomplete or "
				"stale -- there is no heightfield to measure its placement against "
				"(run a *_True config once to bake it)");
			return;
		}

		// NOT a skip. A .zscen that the registry claims exists is a TRACKED asset;
		// skipping on its absence would turn a deleted -- or, in this slice, an
		// un-authored -- committed file into a green run. See the banner.
		const std::string strScenePath = RGCommittedScenePath(xRegion.m_eScene);
		if (strScenePath.empty())
		{
			RGFail(xRegion, "this region has no row in Source/World/ZM_SceneRegistry.h, "
				"so no committed scene can be loaded for it -- an unregistered scene "
				"is a permanent-black-screen defect, not a missing bake");
			return;
		}
		xRegion.m_bScenePresent = RGFilePresentAndNonEmpty(strScenePath);
		if (!xRegion.m_bScenePresent)
		{
			RGFail(xRegion, "the region's committed .zscen is missing or empty -- "
				"until the R1-2 authoring boot creates it this test is EXPECTED to "
				"fail, and once it exists this message means a tracked asset was "
				"deleted. Either way it is not a missing bake, so this run FAILS "
				"rather than skipping");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fRG_FIXED_DT);
		xRegion.m_ePhase = RGPhase::Resolve;
		g_xEngine.Scenes().LoadSceneByIndex(
			(int)xRegion.m_uBuildIndex, SCENE_LOAD_SINGLE);
	}

	bool RGStepRegion(RGRegion& xRegion)
	{
		if (xRegion.m_ePhase == RGPhase::Done)
		{
			return false;
		}
		++xRegion.m_iFrames;
		switch (xRegion.m_ePhase)
		{
		case RGPhase::Resolve: return RGStepResolve(xRegion);
		case RGPhase::Measure: return RGStepMeasure(xRegion);
		case RGPhase::Done:    return false;
		}
		return false;
	}

	bool RGVerifyRegion(RGRegion& xRegion)
	{
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();

		if (xRegion.m_bSkipped)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[%s] SKIPPED -- no usable terrain bake for this region, so NOTHING "
				"was measured. The provisional ground table in %s is UNVERIFIED on "
				"this run, and it CANNOT be frozen from a run that skipped.",
				xRegion.m_szTestName, xRegion.m_szPlacementHeader);
			return true;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[%s] scene='%s' buildIndex=%u recipeTargetHeight=%.5f rayWindow=[%.3f "
			"down %.1f m] rows=%u tolerance=%.3f",
			xRegion.m_szTestName, ZM_GetSceneName(xRegion.m_eScene),
			xRegion.m_uBuildIndex, (double)xRegion.m_fRecipeTargetHeight,
			(double)(xRegion.m_fRecipeTargetHeight + fRG_RAY_START_ABOVE_TARGET),
			(double)fRG_RAY_MAX_DISTANCE, xRegion.m_uRowCount,
			(double)fRG_HEIGHT_TOLERANCE);

		// ★ THE PASTE-READY LOG, EMITTED ON EVERY RUN, PASS OR FAIL. This is how
		// the provisional heights are replaced by measured ones, and re-obtained
		// after any terrain recipe change. `finalHit` is part of the record on
		// purpose: a literal must never be frozen from a row whose ray stopped on
		// a body (read: on 'Player') instead of on the region's terrain.
		for (u_int u = 0u; u < xRegion.m_uRowCount; ++u)
		{
			const RGRow& xRow = xRegion.m_axRows[u];
			char acFinal[96];
			RGDescribeEntity(xRow.m_xProbe.m_xFinalHitEntity, acFinal);
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[%s] MEASURED FEET Y -- PASTE row %u into %s: name=%s paste=%.5ff "
				"freezes=%s xz=(%.3f, %.3f) table=%.5f tableError=%.5f | resolved=%d "
				"hitTerrain=%d finalHit='%s' ignored='%s' ignorePresent=%d",
				xRegion.m_szTestName, u, xRegion.m_szPlacementHeader,
				xRow.m_szRowName, (double)xRow.m_xProbe.m_fFeetY,
				xRow.m_szFreezeTarget, (double)xRow.m_fX, (double)xRow.m_fZ,
				(double)xRow.m_fTableY,
				(double)(xRow.m_xProbe.m_fFeetY - xRow.m_fTableY),
				(int)xRow.m_xProbe.m_bResolved, (int)xRow.m_xProbe.m_bHitTerrain,
				acFinal, xRow.m_szIgnoreEntityName,
				(int)xRow.m_bIgnoreEntityPresent);
		}

		bool bPassed = true;

		// A truncated table would silently shrink coverage while still passing...
		if (xRegion.m_bRowOverflow)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[%s] the row table overflowed its %u-slot budget -- coverage was "
				"TRUNCATED, so a pass would be meaningless",
				xRegion.m_szTestName, uRG_MAX_ROWS);
		}

		// ...and so would a table that never got built at all.
		if (xRegion.m_uRowCount == 0u)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[%s] the row table is EMPTY -- this run measured nothing and its "
				"coverage is vacuous", xRegion.m_szTestName);
		}

		if (!xRegion.m_bBakePresent || !xRegion.m_bScenePresent
			|| !xRegion.m_bResolved || !xRegion.m_bMeasured)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[%s] %s (bake=%d scene=%d resolved=%d measured=%d)",
				xRegion.m_szTestName, xRegion.m_szFailure,
				(int)xRegion.m_bBakePresent, (int)xRegion.m_bScenePresent,
				(int)xRegion.m_bResolved, (int)xRegion.m_bMeasured);
			return false;
		}

		for (u_int u = 0u; u < xRegion.m_uRowCount; ++u)
		{
			const RGRow& xRow = xRegion.m_axRows[u];

			// ★ THE MEASUREMENT MUST BE GROUND, AND THE ONLY BODY A ROW MAY LOOK
			// PAST IS ITS OWN NAMED IGNORE ENTITY. Anything else under a column --
			// the player capsule, a trainer, a neighbouring block -- would turn a
			// body TOP into a "terrain height" and freeze it into a placement
			// header, which is a spawn height that floats or buries whoever lands
			// on it. Named and failed, never passed silently.
			if (!xRow.m_xProbe.m_bHitTerrain
				|| xRow.m_xProbe.m_xFinalHitEntity != xRegion.m_xTerrainID)
			{
				char acFinal[96];
				RGDescribeEntity(xRow.m_xProbe.m_xFinalHitEntity, acFinal);
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] the probe under '%s' terminated on '%s' rather than on "
					"'%s' -- the measurement would be a body top, not a ground "
					"height (this column's ONE legitimate body is '%s', and it is "
					"already ignored). DO NOT freeze this row's value.",
					xRegion.m_szTestName, xRow.m_szRowName, acFinal,
					xRegion.m_szTerrainEntityName, xRow.m_szIgnoreEntityName);
				continue;
			}

			const float fError = std::fabs(xRow.m_xProbe.m_fFeetY - xRow.m_fTableY);
			if (fError > fRG_HEIGHT_TOLERANCE)
			{
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[%s] '%s': the compiled MEASURED-TABLE row reads %.5f, which is "
					"%.5f m off the terrain surface %.5f (tolerance %.3f). The row is "
					"most likely still carrying its recipe-target SEED -- paste the "
					"`paste=` literal above into %s at %s and rebuild, and freeze the "
					"WHOLE SET rather than this one row. Do NOT paste it into the "
					"region's recipe mirror: that constant must stay exactly equal to "
					"the recipe. If instead this row disagrees with its NEIGHBOURS by "
					"metres, that is a FINDING that the anchor sits outside the "
					"flattened region its header claims, not a tolerance to widen.",
					xRegion.m_szTestName, xRow.m_szRowName, (double)xRow.m_fTableY,
					(double)fError, (double)xRow.m_xProbe.m_fFeetY,
					(double)fRG_HEIGHT_TOLERANCE, xRegion.m_szPlacementHeader,
					xRow.m_szFreezeTarget);
			}
		}
		return bPassed;
	}
}

// ============================================================================
// (1) ZM_Route1GroundTruth_Test
// ============================================================================

namespace
{
	RGRegion g_xRoute1Region;

	void Setup_Route1GroundTruth()
	{
		g_xRoute1Region.m_szTestName = "ZM_Route1GroundTruth";
		g_xRoute1Region.m_szPlacementHeader =
			"Source/World/ZM_Route1Placement.h";
		g_xRoute1Region.m_szTerrainEntityName = szZM_ROUTE1_TERRAIN_ENTITY_NAME;
		g_xRoute1Region.m_eScene = ZM_SCENE_ROUTE1;
		g_xRoute1Region.m_pfnBuildRows = &RGBuildRoute1Rows;
		RGSetupRegion(g_xRoute1Region);
	}

	bool Step_Route1GroundTruth(int)
	{
		return RGStepRegion(g_xRoute1Region);
	}

	bool Verify_Route1GroundTruth()
	{
		return RGVerifyRegion(g_xRoute1Region);
	}
}

static const Zenith_AutomatedTest g_xZMRoute1GroundTruthTest = {
	"ZM_Route1GroundTruth_Test",
	&Setup_Route1GroundTruth,
	&Step_Route1GroundTruth,
	&Verify_Route1GroundTruth,
	// Both waiting phases own a deadline that FAILS with a diagnostic; this cap
	// is only a backstop above their sum.
	/* maxFrames */ 2400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMRoute1GroundTruthTest);

// ============================================================================
// (2) ZM_ThornacreGroundTruth_Test
// ============================================================================

namespace
{
	RGRegion g_xThornacreRegion;

	void Setup_ThornacreGroundTruth()
	{
		g_xThornacreRegion.m_szTestName = "ZM_ThornacreGroundTruth";
		g_xThornacreRegion.m_szPlacementHeader =
			"Source/World/ZM_ThornacrePlacement.h";
		g_xThornacreRegion.m_szTerrainEntityName =
			szZM_THORNACRE_TERRAIN_ENTITY_NAME;
		g_xThornacreRegion.m_eScene = ZM_SCENE_THORNACRE;
		g_xThornacreRegion.m_pfnBuildRows = &RGBuildThornacreRows;
		RGSetupRegion(g_xThornacreRegion);
	}

	bool Step_ThornacreGroundTruth(int)
	{
		return RGStepRegion(g_xThornacreRegion);
	}

	bool Verify_ThornacreGroundTruth()
	{
		return RGVerifyRegion(g_xThornacreRegion);
	}
}

static const Zenith_AutomatedTest g_xZMThornacreGroundTruthTest = {
	"ZM_ThornacreGroundTruth_Test",
	&Setup_ThornacreGroundTruth,
	&Step_ThornacreGroundTruth,
	&Verify_ThornacreGroundTruth,
	/* maxFrames */ 2400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMThornacreGroundTruthTest);

#endif // ZENITH_INPUT_SIMULATOR
