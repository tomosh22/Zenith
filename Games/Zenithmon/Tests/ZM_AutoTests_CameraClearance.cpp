#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_CameraClearance (ZM-D-173) -- the two REAL-SCENE guards behind the
// Home relocation. Both load the COMMITTED Dawnmere against the REAL baked
// terrain; neither creates, moves or teleports anything.
//
//   ZM_DawnmereHomeGroundTruth_Test  -- the MEASUREMENT ORACLE. Casts a real
//     downward ray at every Home placement column and reds if a compiled row in
//     Source/World/ZM_DawnmerePlacement.h has drifted from the surface the world
//     actually has. It also LOGS every measured value at INFO on every run: this
//     is how those constants are (re-)obtained after a terrain recipe change.
//
//   ZM_DawnmereCameraClearance_Test  -- the CONTRACT GUARD. At every sample of
//     the authoritative table below it runs the SHIPPED camera maths against the
//     SHIPPED physics world and requires the clamped arm to keep at least half
//     the authored pivot->camera distance.
//
// ★ WHY NEITHER TEST MOVES THE PLAYER. The clearance calculation is a function of
// GROUND and GEOMETRY, not of where the player happens to be standing, so driving
// or teleporting a capsule to each sample would add controller state, settling
// velocity and physics->transform sync as three ways for the measurement to be
// about something other than the world. The samples are evaluated where they are.
// (It also keeps ZM-D-156's teleport hazards entirely out of scope.)
//
// ★ THE COVERAGE BOUNDARY, STATED SO IT CANNOT BE OVERSOLD. The table below is
// the ENFORCEABLE boundary: the critical movement routes and the actor-free
// interaction approaches. It does NOT prove every mathematically standable point
// in Dawnmere satisfies the contract, and it deliberately carries no rings around
// NPCs -- a live NPC can legitimately occupy the camera ray, which would make a
// STATIC-LAYOUT guard nondeterministic. A newly authored region must add ITS
// primary traversal paths, warp approaches and actor-free interaction approaches
// to this table as part of authoring it.
//
// Both run on the NULL backend (m_bRequiresGraphics = false): they need physics
// and a terrain bake, not a swapchain. They SKIP only when the committed scene or
// the terrain bake is genuinely absent -- a MOVED entity, a MISSING entity and a
// STALE constant all FAIL.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Query.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
// The Dawnmere terrain recipe, for the authored Lab walkway. Compiled tables
// only -- no bake is read here, so this include adds no asset dependency.
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <array>
#include <cmath>
#include <cstdio>     // snprintf -- the blocking-entity descriptions
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
	constexpr float fCC_FIXED_DT = 1.0f / 60.0f;
	constexpr int iCC_DAWNMERE_BUILD_INDEX = 2;

	// Authored entity names. Spelled here on purpose: RENAMING one in
	// Zenithmon.cpp must fail these tests loudly rather than quietly remove what
	// they were measuring.
	constexpr const char* szCC_TERRAIN_ENTITY = "DawnmereTerrain";
	constexpr const char* szCC_PLAYER_ENTITY = "Player";

	// The lab shell SC-E will author. It does NOT exist in the committed Dawnmere
	// yet, and every use of this name below is written to tolerate that -- see the
	// SC-D oracle's resolve step.
	constexpr const char* szCC_LAB_SHELL_ENTITY = "DawnmereLabShell";

	// The terrain recipe's name for the lab walkway. Spelled once.
	constexpr const char* szCC_LAB_PATH_NAME = "Lab";

	// The SAME tolerance the W5 NPC oracle uses, so "the compiled height is stale"
	// means the same thing in both files.
	constexpr float fCC_HEIGHT_TOLERANCE = 0.15f;

	// The probe window. It starts well ABOVE the tallest Home blockout (the shell
	// stands ~6 m proud of its ground) so a ray never begins INSIDE a box, which
	// is the one starting condition a convex ray cast answers ambiguously.
	constexpr float fCC_RAY_START_HEIGHT = 10.0f;
	constexpr float fCC_RAY_MAX_DISTANCE = 20.0f;

	// The contract: at least half the authored pivot->camera distance must survive
	// the clamp. See the ZM-D-173 block in Source/World/ZM_DawnmerePlacement.h.
	constexpr float fCC_MIN_ARM_FRACTION = 0.5f;

	// The captured scene yaw must still be the one every sample DIRECTION was
	// derived at, or the table is measuring rays the game does not cast.
	constexpr float fCC_YAW_EPSILON = 1.0e-3f;

	constexpr int iCC_RESOLVE_DEADLINE_FRAMES = 900;
	constexpr int iCC_PROBE_DEADLINE_FRAMES = 900;

	// ---- Shared fixtures ----------------------------------------------------

	bool CCFilePresentAndNonEmpty(const std::string& strPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(strPath, xError) || xError)
		{
			return false;
		}
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
		return !xError && ulSize != 0u;
	}

	// ★ THE TWO PREREQUISITE FAMILIES ARE DIFFERENT KINDS OF THING, AND ONLY ONE
	// OF THEM MAY EVER JUSTIFY A SKIP.
	//
	// The terrain bake is GITIGNORED: a fresh clone genuinely has no heightfield
	// until a *_True boot makes one, so "there is nothing to measure against" is a
	// legitimate skip. The Dawnmere SCENE is COMMITTED (ZM-D-148, one of the six
	// tracked assets), so its absence is a DEFECT -- something deleted a tracked
	// file -- and skipping on it would turn that into a silent pass in CI, which
	// runs on exactly the tree where it must never happen.
	//
	// The two shipped tests below still gate on BOTH via CCRequiredAssetsPresent
	// (their behaviour is unchanged by this split); the SC-D lab oracle skips on
	// the bake alone and FAILS on a missing tracked scene.
	bool CCTerrainBakePresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 6> astrRequired = {
			strRoot + "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			if (!CCFilePresentAndNonEmpty(strPath))
			{
				return false;
			}
		}
		return true;
	}

	bool CCCommittedDawnmereScenePresent()
	{
		return CCFilePresentAndNonEmpty(
			std::string(GAME_ASSETS_DIR) + "Scenes/Dawnmere" ZENITH_SCENE_EXT);
	}

	bool CCRequiredAssetsPresent()
	{
		return CCCommittedDawnmereScenePresent() && CCTerrainBakePresent();
	}

	// A NAME for an entity id that is safe to print even when the id no longer
	// resolves -- a raw "%u:%u" is still information, an empty string is not.
	void CCDescribeEntity(Zenith_EntityID xID, char (&acOut)[96])
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

	struct CCGroundProbe
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
	// conflating them is what turns a five-frame diagnosis into a 900-frame
	// timeout that blames the wrong subsystem:
	//   * HIT SOMETHING THAT IS NOT THE TERRAIN -> TERMINAL, and a failure. The
	//     column is under a solid body, so it is not a place a player can stand
	//     and no amount of waiting will change the answer. The blocking entity is
	//     NAMED.
	//   * HIT NOTHING AT ALL -> NOT terminal. The terrain physics body arrives
	//     with streaming rather than with the scene, so "no ground yet" is a WAIT,
	//     and only the phase deadline turns it into a failure.
	//
	// Legitimate overhead geometry (a door jamb standing on the very column whose
	// ground is being measured) is handled by the caller's per-column ignore
	// entity, NOT by stepping through hits: stepping past an entry point lands the
	// next cast INSIDE the same convex box, where a ray cast reports no hit at
	// all, and the probe would then mis-report "the terrain never streamed in".
	CCGroundProbe CCProbeGroundAt(float fX, float fZ, float fReferenceFeetY,
		Zenith_EntityID xIgnoreEntity, Zenith_EntityID xTerrainEntity)
	{
		CCGroundProbe xProbe;
		const Zenith_Maths::Vector3 xOrigin(
			fX, fReferenceFeetY + fCC_RAY_START_HEIGHT, fZ);
		const Zenith_Physics::RaycastResult xHit =
			Zenith_PhysicsQuery::RaycastIgnoring(
				xOrigin, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f),
				fCC_RAY_MAX_DISTANCE, xIgnoreEntity);
		if (!xHit.m_bHit)
		{
			return xProbe;
		}

		xProbe.m_bResolved = true;
		xProbe.m_xFinalHitEntity = xHit.m_xHitEntity;
		if (xHit.m_xHitEntity == xTerrainEntity
			&& xTerrainEntity != INVALID_ENTITY_ID)
		{
			xProbe.m_bHitTerrain = true;
			xProbe.m_fFeetY = xHit.m_xHitPoint.y;
		}
		return xProbe;
	}

	Zenith_Entity CCFindEntity(Zenith_SceneData* pxData, const char* szName)
	{
		return pxData != nullptr ? pxData->FindEntityByName(szName) : Zenith_Entity();
	}

	bool CCDawnmereIsActive(Zenith_SceneData*& pxDataOut)
	{
		pxDataOut = g_xEngine.Scenes().GetActiveSceneData();
		return pxDataOut != nullptr
			&& g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex
					== iCC_DAWNMERE_BUILD_INDEX;
	}
}

// ============================================================================
// (1) ZM_DawnmereHomeGroundTruth_Test -- the measurement oracle.
// ============================================================================

namespace
{
	enum class HGTPhase { Resolve, Measure, Done };

	HGTPhase g_eHGTPhase = HGTPhase::Done;
	int  g_iHGTFrames = 0;
	bool g_bHGTPrereqs = false;
	bool g_bHGTSkipped = false;
	bool g_bHGTResolved = false;
	bool g_bHGTMeasured = false;
	const char* g_szHGTFailure = "test did not reach verification";
	Zenith_EntityID g_xHGTTerrainID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xHGTShellID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xHGTPlayerID = INVALID_ENTITY_ID;

	constexpr u_int uHGT_SAMPLE_SLOTS = 16u;
	static_assert(uHGT_SAMPLE_SLOTS >= (u_int)ZM_DAWNMERE_HOME_SAMPLE_COUNT,
		"the ground-truth probe needs one slot per Home ground sample");
	CCGroundProbe g_axHGTProbes[uHGT_SAMPLE_SLOTS];

	void FailHGT(const char* szReason)
	{
		g_szHGTFailure = szReason;
		g_eHGTPhase = HGTPhase::Done;
	}

	bool HGTStepResolve()
	{
		Zenith_SceneData* pxData = nullptr;
		if (!CCDawnmereIsActive(pxData))
		{
			if (g_iHGTFrames > iCC_RESOLVE_DEADLINE_FRAMES)
			{
				FailHGT("Dawnmere never became the active scene");
				return false;
			}
			return true;
		}

		const Zenith_Entity xTerrain = CCFindEntity(pxData, szCC_TERRAIN_ENTITY);
		const Zenith_Entity xShell = CCFindEntity(pxData, "DawnmereHomeShell");
		const Zenith_Entity xPlayer = CCFindEntity(pxData, szCC_PLAYER_ENTITY);
		if (!xTerrain.IsValid() || !xShell.IsValid() || !xPlayer.IsValid())
		{
			if (g_iHGTFrames > iCC_RESOLVE_DEADLINE_FRAMES)
			{
				FailHGT("the committed Dawnmere does not contain 'DawnmereTerrain', "
					"'DawnmereHomeShell' and 'Player' -- a renamed or deleted "
					"authored entity, not a missing bake");
				return false;
			}
			return true;
		}

		g_xHGTTerrainID = xTerrain.GetEntityID();
		g_xHGTShellID = xShell.GetEntityID();
		g_xHGTPlayerID = xPlayer.GetEntityID();
		g_bHGTResolved = true;
		g_eHGTPhase = HGTPhase::Measure;
		g_iHGTFrames = 0;
		return true;
	}

	// The ONE authored body that legitimately stands over a given column, and which
	// the probe must therefore look past to reach the ground beneath it. Anything
	// ELSE the ray lands on is a genuine malformed sample and is reported by name.
	//
	//   * EVERY HOME COLUMN IGNORES THE SHELL. Four of them are under it by
	//     construction (the footprint corners), and the rest must stay measurable
	//     while the shell is being MOVED: the committed scene carries the previous
	//     placement until the re-authoring boot, so a probe that stopped on the old
	//     shell could never produce the heights the new one needs. The shell is the
	//     thing this table POSITIONS, so looking past it is the correct rule rather
	//     than a workaround.
	//   * The town centre is under the authored PLAYER capsule, which stands on it
	//     -- the same self-confirming-measurement hazard the W5 NPC oracle guards
	//     against, and the reason that oracle ignores each NPC at its own anchor.
	//   * The door jambs, the lintel and the sensor need no entry: the door ground
	//     samples sit half a metre out into the forecourt, clear of the jambs' own
	//     0.25 m protrusion (see the geometry note in ZM_DawnmerePlacement.cpp), and
	//     ordinary raycasts have skipped sensor bodies since ZM-D-173.
	//
	// ★ AND IGNORING THE SHELL HERE DOES NOT WEAKEN ANYTHING, because the question
	// "is this column somewhere a player can actually stand?" belongs to the OTHER
	// test in this file. ZM_DawnmereCameraClearance_Test ignores only the player, so
	// a route or approach that ends up under the building fails there, by name. The
	// two probes ask different questions and are filtered accordingly.
	Zenith_EntityID HGTIgnoreBodyForSample(u_int uSample)
	{
		return uSample == (u_int)ZM_DAWNMERE_HOME_SAMPLE_TOWN_CENTER
			? g_xHGTPlayerID
			: g_xHGTShellID;
	}

	bool HGTStepMeasure()
	{
		const u_int uCount = ZM_GetDawnmereHomeSampleCount();
		u_int uResolved = 0u;
		for (u_int u = 0u; u < uCount && u < uHGT_SAMPLE_SLOTS; ++u)
		{
			if (g_axHGTProbes[u].m_bResolved)
			{
				++uResolved;
				continue;
			}
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereHomeSample(u);
			g_axHGTProbes[u] = CCProbeGroundAt(xSample.m_fX, xSample.m_fZ,
				xSample.m_fFeetY, HGTIgnoreBodyForSample(u), g_xHGTTerrainID);
			if (g_axHGTProbes[u].m_bResolved)
			{
				++uResolved;
			}
		}

		// Only columns that found NOTHING are still waiting; a column that landed
		// on the wrong body is already decided and is judged in Verify.
		if (uResolved != uCount)
		{
			if (g_iHGTFrames > iCC_PROBE_DEADLINE_FRAMES)
			{
				FailHGT("a downward probe found no ground at all under a Home "
					"placement column -- either the terrain physics body never "
					"streamed in, or a compiled height is now more than the probe "
					"window away from the real surface (per-column detail below)");
				return false;
			}
			return true;
		}

		g_bHGTMeasured = true;
		g_eHGTPhase = HGTPhase::Done;
		return false;
	}

	void Setup_DawnmereHomeGroundTruth()
	{
		g_eHGTPhase = HGTPhase::Done;
		g_iHGTFrames = 0;
		g_bHGTPrereqs = false;
		g_bHGTSkipped = false;
		g_bHGTResolved = false;
		g_bHGTMeasured = false;
		g_szHGTFailure = "test did not reach verification";
		g_xHGTTerrainID = INVALID_ENTITY_ID;
		g_xHGTShellID = INVALID_ENTITY_ID;
		g_xHGTPlayerID = INVALID_ENTITY_ID;
		for (u_int u = 0u; u < uHGT_SAMPLE_SLOTS; ++u)
		{
			g_axHGTProbes[u] = CCGroundProbe();
		}

		Zenith_InputSimulator::ResetAllInputState();

		// The ONE skip, and deliberately narrow: "the heightfield this test
		// measures against does not exist". RequestSkip bypasses Verify, so no
		// fixed-dt or scene-load state may be installed before this point.
		g_bHGTPrereqs = CCRequiredAssetsPresent();
		if (!g_bHGTPrereqs)
		{
			g_bHGTSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_DawnmereHomeGroundTruth] the Dawnmere scene / terrain bake is "
				"absent or incomplete -- there is no heightfield to measure the "
				"compiled Home placement against (run a *_True config once to bake it)");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fCC_FIXED_DT);
		g_eHGTPhase = HGTPhase::Resolve;
		g_xEngine.Scenes().LoadSceneByIndex(
			iCC_DAWNMERE_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_DawnmereHomeGroundTruth(int)
	{
		if (g_eHGTPhase == HGTPhase::Done)
		{
			return false;
		}
		++g_iHGTFrames;
		switch (g_eHGTPhase)
		{
		case HGTPhase::Resolve: return HGTStepResolve();
		case HGTPhase::Measure: return HGTStepMeasure();
		case HGTPhase::Done:    return false;
		}
		return false;
	}

	bool Verify_DawnmereHomeGroundTruth()
	{
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();

		if (g_bHGTSkipped)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereHomeGroundTruth] SKIPPED -- no baked Dawnmere terrain, so "
				"nothing was measured. The ZM-D-173 Home ground table in "
				"Source/World/ZM_DawnmerePlacement.h is UNVERIFIED on this run.");
			return true;
		}

		// ★ THE PASTE-READY LOG, EMITTED ON EVERY RUN, PASS OR FAIL. This is how
		// the ZM-D-173 table is (re-)obtained after a terrain recipe change.
		const u_int uCount = ZM_GetDawnmereHomeSampleCount();
		for (u_int u = 0u; u < uCount && u < uHGT_SAMPLE_SLOTS; ++u)
		{
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereHomeSample(u);
			char acFinal[96];
			CCDescribeEntity(g_axHGTProbes[u].m_xFinalHitEntity, acFinal);
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereHomeGroundTruth] MEASURED FEET Y (paste into the "
				"ZM-D-173 block in Source/World/ZM_DawnmerePlacement.h): name=%s "
				"xz=(%.1f, %.1f) measured=%.5f table=%.5f tableError=%.5f | "
				"resolved=%d hitTerrain=%d finalHit='%s'",
				xSample.m_szEntityName, (double)xSample.m_fX, (double)xSample.m_fZ,
				(double)g_axHGTProbes[u].m_fFeetY, (double)xSample.m_fFeetY,
				(double)(g_axHGTProbes[u].m_fFeetY - xSample.m_fFeetY),
				(int)g_axHGTProbes[u].m_bResolved,
				(int)g_axHGTProbes[u].m_bHitTerrain, acFinal);
		}

		// Also log the four DERIVED authored Y values, so a re-measure round can
		// be checked without re-deriving the formulas by hand.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereHomeGroundTruth] DERIVED authored Y: shell=%.6f "
			"doorLeft=%.6f doorRight=%.6f lintel=%.6f trigger=%.6f spawnFeet=%.6f",
			(double)ZM_GetDawnmereHomeShell().m_xCenter.y,
			(double)ZM_GetDawnmereHomeDoorLeft().m_xCenter.y,
			(double)ZM_GetDawnmereHomeDoorRight().m_xCenter.y,
			(double)ZM_GetDawnmereHomeDoorLintel().m_xCenter.y,
			(double)ZM_GetDawnmereHomeDoorTrigger().m_xCenter.y,
			(double)ZM_GetDawnmereFromHomeSpawnFeet().y);

		if (!g_bHGTPrereqs || !g_bHGTResolved || !g_bHGTMeasured)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereHomeGroundTruth] %s (prereqs=%d resolved=%d measured=%d)",
				g_szHGTFailure, (int)g_bHGTPrereqs, (int)g_bHGTResolved,
				(int)g_bHGTMeasured);
			return false;
		}

		bool bPassed = true;
		for (u_int u = 0u; u < uCount && u < uHGT_SAMPLE_SLOTS; ++u)
		{
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereHomeSample(u);
			// The measurement must be GROUND. Anything else is a body top.
			if (!g_axHGTProbes[u].m_bHitTerrain
				|| g_axHGTProbes[u].m_xFinalHitEntity != g_xHGTTerrainID)
			{
				char acFinal[96];
				CCDescribeEntity(g_axHGTProbes[u].m_xFinalHitEntity, acFinal);
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereHomeGroundTruth] the probe under '%s' terminated on "
					"'%s' rather than on '%s' -- the measurement would be a body top, "
					"not a ground height",
					xSample.m_szEntityName, acFinal, szCC_TERRAIN_ENTITY);
				continue;
			}
			const float fError =
				std::fabs(g_axHGTProbes[u].m_fFeetY - xSample.m_fFeetY);
			if (fError > fCC_HEIGHT_TOLERANCE)
			{
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereHomeGroundTruth] '%s': the compiled feet height %.5f "
					"is %.5f m off the terrain surface %.5f (tolerance %.3f) -- "
					"re-measure the ZM-D-173 block in "
					"Source/World/ZM_DawnmerePlacement.h",
					xSample.m_szEntityName, (double)xSample.m_fFeetY, (double)fError,
					(double)g_axHGTProbes[u].m_fFeetY, (double)fCC_HEIGHT_TOLERANCE);
			}
		}
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMDawnmereHomeGroundTruthTest = {
	"ZM_DawnmereHomeGroundTruth_Test",
	&Setup_DawnmereHomeGroundTruth,
	&Step_DawnmereHomeGroundTruth,
	&Verify_DawnmereHomeGroundTruth,
	// Both waiting phases own a deadline that FAILS with a diagnostic; this cap
	// is only a backstop above their sum.
	/* maxFrames */ 2400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDawnmereHomeGroundTruthTest);

// ============================================================================
// (2) ZM_DawnmereCameraClearance_Test -- the contract guard.
// ============================================================================

namespace
{
	enum CC_GROUP : u_int
	{
		CC_GROUP_TOWN_TO_STAGING,
		CC_GROUP_STAGING_TO_TRIGGER,
		CC_GROUP_HOME_DIRT_PATH,
		CC_GROUP_SPAWN_RING,
		CC_GROUP_TOWN_RING,
		// S8 SC-D: the lab site's routes and its warp arrival. Added with the
		// PLACEMENT rather than with the authoring, on purpose -- the coverage rule
		// at the top of this file says a newly authored region adds its traversal
		// paths, warp approaches and interaction approaches HERE, and doing it now
		// means SC-E's shell lands into a table that already measures the ground it
		// will stand on. Until SC-E these groups run over open terrain, which is
		// what makes the before/after comparison meaningful.
		CC_GROUP_TOWN_TO_LAB_STAGING,
		CC_GROUP_LAB_STAGING_TO_TRIGGER,
		CC_GROUP_LAB_DIRT_PATH,
		CC_GROUP_LAB_SPAWN_RING,

		CC_GROUP_COUNT
	};

	// The bound is DEDUCED, never spelled: written as [CC_GROUP_COUNT] the
	// static_assert below would be a tautology and a forgotten name would simply
	// zero-initialise into a NULL that every failure report then prints.
	const char* g_aszCCGroupNames[] = {
		"townCentre->doorStaging",
		"doorStaging->doorTrigger",
		"homeDirtPath",
		"fromHomeSpawnRing",
		"townCentreRing",
		"townCentre->labStaging",
		"labStaging->labTrigger",
		"labDirtPath",
		"fromLabSpawnRing",
	};

	static_assert(
		sizeof(g_aszCCGroupNames) / sizeof(g_aszCCGroupNames[0]) == CC_GROUP_COUNT,
		"every CC_GROUP needs a name -- the failure reports index this array");

	// Sized for the authoritative table plus generous headroom; overflow is
	// REPORTED as a failure rather than silently truncating coverage.
	//
	// ★ RAISED 512 -> 1024 BY S8 SC-D, WITH THE ARITHMETIC WRITTEN DOWN so the next
	// region does not have to re-derive it. The Home-era table is 308 samples
	// (130 + 17 + 143 + 9 + 9); the four lab groups add 295 (136 + 13 + 137 + 9),
	// for 603. At 512 this change would have OVERFLOWED -- reported as a failure,
	// never truncated, which is the property that made the budget worth checking
	// rather than trusting. 1024 leaves 421 spare, i.e. room for a region of the
	// lab's size again plus change.
	constexpr u_int uCC_MAX_SAMPLES = 1024u;
	constexpr float fCC_ROUTE_SPACING = 1.0f;
	constexpr float fCC_APPROACH_SPACING = 0.25f;
	constexpr float fCC_RING_RADIUS = 1.5f;
	constexpr u_int uCC_RING_POINTS = 8u;

	struct CCSample
	{
		float m_fX = 0.0f;
		float m_fZ = 0.0f;
		u_int m_uGroup = 0u;
		u_int m_uIndexInGroup = 0u;
		CCGroundProbe m_xProbe;
		bool m_bEvaluated = false;
		bool m_bViolated = false;
		float m_fDesiredArm = 0.0f;
		float m_fClampedArm = 0.0f;
		float m_fHitDistance = -1.0f;
		bool m_bArmHit = false;
		Zenith_EntityID m_xBlocker = INVALID_ENTITY_ID;
	};

	CCSample g_axCCSamples[uCC_MAX_SAMPLES];
	u_int g_uCCSampleCount = 0u;
	u_int g_auCCGroupCounts[CC_GROUP_COUNT] = {};
	bool g_bCCSampleOverflow = false;
	// The lab dirt path is looked up in the terrain recipe BY NAME. A rename or a
	// deletion there must not silently drop a whole coverage group, so it is
	// recorded and reported as a failure exactly like an overflow.
	bool g_bCCLabPathMissing = false;

	enum class CCPhase { Resolve, Probe, Evaluate, Done };

	CCPhase g_eCCPhase = CCPhase::Done;
	int  g_iCCFrames = 0;
	bool g_bCCPrereqs = false;
	bool g_bCCSkipped = false;
	bool g_bCCResolved = false;
	bool g_bCCProbed = false;
	bool g_bCCEvaluated = false;
	const char* g_szCCFailure = "test did not reach verification";
	Zenith_EntityID g_xCCTerrainID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xCCPlayerID = INVALID_ENTITY_ID;
	float g_fCCAuthoredYaw = 0.0f;
	bool g_bCCYawCaptured = false;
	float g_fCCCapsuleHalfExtent = -1.0f;   // fails closed
	u_int g_uCCViolationCount = 0u;
	u_int g_uCCMalformedCount = 0u;

	void FailCC(const char* szReason)
	{
		g_szCCFailure = szReason;
		g_eCCPhase = CCPhase::Done;
	}

	void CCAddSample(float fX, float fZ, u_int uGroup)
	{
		if (g_uCCSampleCount >= uCC_MAX_SAMPLES)
		{
			g_bCCSampleOverflow = true;
			return;
		}
		CCSample& xSample = g_axCCSamples[g_uCCSampleCount++];
		xSample.m_fX = fX;
		xSample.m_fZ = fZ;
		xSample.m_uGroup = uGroup;
		xSample.m_uIndexInGroup = g_auCCGroupCounts[uGroup]++;
	}

	// Both endpoints are always sampled, and the realised spacing is <= the
	// requested one -- so a segment can never be covered more coarsely than the
	// table claims.
	void CCAddSegment(float fAX, float fAZ, float fBX, float fBZ,
		float fSpacing, u_int uGroup)
	{
		const float fDX = fBX - fAX;
		const float fDZ = fBZ - fAZ;
		const float fLength = std::sqrt(fDX * fDX + fDZ * fDZ);
		u_int uIntervals = (u_int)std::ceil(fLength / fSpacing);
		if (uIntervals == 0u)
		{
			uIntervals = 1u;
		}
		for (u_int u = 0u; u <= uIntervals; ++u)
		{
			const float fT = (float)u / (float)uIntervals;
			CCAddSample(fAX + fDX * fT, fAZ + fDZ * fT, uGroup);
		}
	}

	void CCAddRing(float fX, float fZ, float fRadius, u_int uPoints, u_int uGroup)
	{
		CCAddSample(fX, fZ, uGroup);
		for (u_int u = 0u; u < uPoints; ++u)
		{
			const float fAngle =
				6.28318530718f * ((float)u / (float)uPoints);
			CCAddSample(fX + fRadius * std::cos(fAngle),
				fZ + fRadius * std::sin(fAngle), uGroup);
		}
	}

	// ★ THE AUTHORITATIVE SAMPLE TABLE. Deterministic, actor-free, and limited to
	// the critical movement areas -- see the coverage-boundary note at the top of
	// this file before adding or removing anything here.
	void CCBuildSampleTable()
	{
		g_uCCSampleCount = 0u;
		g_bCCSampleOverflow = false;
		g_bCCLabPathMissing = false;
		for (u_int u = 0u; u < CC_GROUP_COUNT; ++u)
		{
			g_auCCGroupCounts[u] = 0u;
		}
		for (u_int u = 0u; u < uCC_MAX_SAMPLES; ++u)
		{
			g_axCCSamples[u] = CCSample();
		}

		const Zenith_Maths::Vector3 xStaging = ZM_GetDawnmereHomeDoorStagingXZ();
		const Zenith_Maths::Vector3 xTarget = ZM_GetDawnmereHomeDoorTargetXZ();
		const Zenith_Maths::Vector3 xSpawn = ZM_GetDawnmereFromHomeSpawnFeet();

		// (a) The blind drive ZM_PlayerHomeRoundTrip_Test runs, at 1 m.
		CCAddSegment(fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
			xStaging.x, xStaging.z, fCC_ROUTE_SPACING, CC_GROUP_TOWN_TO_STAGING);

		// (b) The doorway approach itself, four times finer, because this is the
		// stretch the old placement broke and a 1 m grid could step over it.
		CCAddSegment(xStaging.x, xStaging.z, xTarget.x, xTarget.z,
			fCC_APPROACH_SPACING, CC_GROUP_STAGING_TO_TRIGGER);

		// (c) The authored Home dirt path, segment by segment. Its interior vertex
		// is sampled twice (once as each segment's endpoint); a duplicate sample
		// costs one probe and keeps the per-segment endpoints exact.
		CCAddSegment(512.0f, 512.0f, 454.0f, 486.0f,
			fCC_ROUTE_SPACING, CC_GROUP_HOME_DIRT_PATH);
		CCAddSegment(454.0f, 486.0f, 384.0f, 456.0f,
			fCC_ROUTE_SPACING, CC_GROUP_HOME_DIRT_PATH);

		// (d)+(e) The two warp arrival points, each with a ring the player can
		// step onto immediately after arriving.
		CCAddRing(xSpawn.x, xSpawn.z, fCC_RING_RADIUS, uCC_RING_POINTS,
			CC_GROUP_SPAWN_RING);
		CCAddRing(fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
			fCC_RING_RADIUS, uCC_RING_POINTS, CC_GROUP_TOWN_RING);

		// ---- S8 SC-D: the lab site ------------------------------------------
		const Zenith_Maths::Vector3 xLabStaging = ZM_GetDawnmereLabDoorStagingXZ();
		const Zenith_Maths::Vector3 xLabTarget = ZM_GetDawnmereLabDoorTargetXZ();

		// (f) The town-centre -> lab-door approach a blind DriveTowardXZ leg would
		// take, at 1 m. It passes 4.93 m from the wanderer's NORTH patrol endpoint
		// (540, 484) at its closest -- comfortably outside the 3.33 m (a 2.93 m
		// horizontal gap plus the body's own 0.4 m half-width) a body needs to
		// violate the arm contract, which is why a MOVING NPC beside this route
		// cannot make it nondeterministic. That margin is not a coincidence to be
		// re-discovered: ZM_Interaction/
		// LabApproach_ClearsEveryAuthoredNpcAndPatrolEndpoint pins it as a unit.
		//
		// ★ NO TEST DRIVES THIS LEG YET, AND A VIOLATION ON IT MEANS SOMETHING
		// DIFFERENT FROM ONE ON THE OTHERS. Its middle stretch leaves the Plaza
		// pad's 60 m flatten radius and does not reach the Lab pad's 48 m one, and
		// it runs ~25 m south of the Lab walkway's 13 m flatten band -- so roughly
		// 40 m of it crosses NATURAL eroded terrain, where the blocker would be the
		// GROUND rather than a building. That is a real finding about walking
		// straight from the square to the lab (the camera would clip on a slope
		// steeper than ~42 degrees within 3.2 m), not about the lab placement, and
		// the fix would be to route the walk along the authored dirt path rather
		// than to move the building. Read the reported blocker name before
		// concluding anything: 'DawnmereTerrain' says slope, a lab entity says
		// placement.
		CCAddSegment(fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
			xLabStaging.x, xLabStaging.z, fCC_ROUTE_SPACING,
			CC_GROUP_TOWN_TO_LAB_STAGING);

		// (g) The lab doorway approach itself, four times finer, for the reason
		// (b) is fine: this is the stretch a building can break.
		CCAddSegment(xLabStaging.x, xLabStaging.z, xLabTarget.x, xLabTarget.z,
			fCC_APPROACH_SPACING, CC_GROUP_LAB_STAGING_TO_TRIGGER);

		// (h) The authored Lab dirt path, segment by segment, READ FROM THE RECIPE
		// rather than re-typed. The Home group above spells its path as literals
		// (it predates the rule); a route spelled twice cannot red a drift, and
		// this particular route is load-bearing -- it is what forced the lab
		// entrance plane to 527 rather than 528 (see ZM_DawnmerePlacement.h).
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		const ZM_TerrainPathSpec* pxLabPath = nullptr;
		for (u_int u = 0u; u < xRecipe.m_uPathCount; ++u)
		{
			if (std::strcmp(xRecipe.m_pxPaths[u].m_szName, szCC_LAB_PATH_NAME) == 0)
			{
				pxLabPath = &xRecipe.m_pxPaths[u];
				break;
			}
		}
		g_bCCLabPathMissing = pxLabPath == nullptr || pxLabPath->m_uPointCount < 2u;
		if (!g_bCCLabPathMissing)
		{
			for (u_int u = 0u; u + 1u < pxLabPath->m_uPointCount; ++u)
			{
				CCAddSegment(pxLabPath->m_pxPoints[u].m_fX,
					pxLabPath->m_pxPoints[u].m_fZ,
					pxLabPath->m_pxPoints[u + 1u].m_fX,
					pxLabPath->m_pxPoints[u + 1u].m_fZ,
					fCC_ROUTE_SPACING, CC_GROUP_LAB_DIRT_PATH);
			}
		}

		// (i) The FromLab warp arrival, with the same ring the other two get. Spelt
		// from the two placement constants rather than from
		// ZM_GetDawnmereFromLabSpawnFeet(): that accessor's Y is still the SC-D
		// placeholder, and reading only .x/.z off it would invite a reader to think
		// the height mattered here. It does not -- every column measures its own.
		CCAddRing(fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_FROM_LAB_SPAWN_Z,
			fCC_RING_RADIUS, uCC_RING_POINTS, CC_GROUP_LAB_SPAWN_RING);
	}

	bool CCStepResolve()
	{
		Zenith_SceneData* pxData = nullptr;
		if (!CCDawnmereIsActive(pxData))
		{
			if (g_iCCFrames > iCC_RESOLVE_DEADLINE_FRAMES)
			{
				FailCC("Dawnmere never became the active scene");
				return false;
			}
			return true;
		}

		const Zenith_Entity xTerrain = CCFindEntity(pxData, szCC_TERRAIN_ENTITY);
		const Zenith_Entity xPlayer = CCFindEntity(pxData, szCC_PLAYER_ENTITY);
		if (!xTerrain.IsValid() || !xPlayer.IsValid())
		{
			if (g_iCCFrames > iCC_RESOLVE_DEADLINE_FRAMES)
			{
				FailCC("the committed Dawnmere does not contain both "
					"'DawnmereTerrain' and 'Player'");
				return false;
			}
			return true;
		}

		// The camera's captured yaw. Read from the LIVE component rather than
		// assumed, so a scene edit that rotates the camera invalidates the sample
		// directions loudly instead of silently.
		Zenith_EntityID xFollowID = INVALID_ENTITY_ID;
		float fYaw = 0.0f;
		g_xEngine.Scenes().QueryActiveScene<ZM_FollowCamera>().ForEach(
			[&xFollowID, &fYaw](Zenith_EntityID xID, ZM_FollowCamera& xFollow)
			{
				if (xFollowID != INVALID_ENTITY_ID)
				{
					return;
				}
				xFollowID = xID;
				fYaw = xFollow.GetAuthoredYaw();
			});
		if (xFollowID == INVALID_ENTITY_ID)
		{
			if (g_iCCFrames > iCC_RESOLVE_DEADLINE_FRAMES)
			{
				FailCC("no ZM_FollowCamera exists in the committed Dawnmere, so the "
					"authored heading every sample direction depends on is unknown");
				return false;
			}
			return true;
		}

		Zenith_Maths::Vector3 xPlayerScale(1.0f);
		xPlayer.GetComponent<Zenith_TransformComponent>().GetScale(xPlayerScale);

		g_xCCTerrainID = xTerrain.GetEntityID();
		g_xCCPlayerID = xPlayer.GetEntityID();
		g_fCCAuthoredYaw = fYaw;
		g_bCCYawCaptured = true;
		// THE BODY CONTRACT, never the transform scale. A human's body no longer
		// derives from how big its model is drawn, so judging clearance against a
		// scale-derived capsule would measure a body that does not exist.
		g_fCCCapsuleHalfExtent = fZM_HUMAN_BODY_HALF_HEIGHT;
		g_bCCResolved = true;
		g_eCCPhase = CCPhase::Probe;
		g_iCCFrames = 0;
		return true;
	}

	bool CCStepProbe()
	{
		u_int uResolved = 0u;
		for (u_int u = 0u; u < g_uCCSampleCount; ++u)
		{
			CCSample& xSample = g_axCCSamples[u];
			if (xSample.m_xProbe.m_bResolved)
			{
				++uResolved;
				continue;
			}
			// The reference height only sizes the probe WINDOW; the measured value
			// is whatever the terrain says. The town-centre anchor is the one
			// height every Dawnmere placement is already stated against.
			xSample.m_xProbe = CCProbeGroundAt(xSample.m_fX, xSample.m_fZ,
				fZM_DAWNMERE_TOWN_CENTER_FEET_Y, g_xCCPlayerID, g_xCCTerrainID);
			if (xSample.m_xProbe.m_bResolved)
			{
				++uResolved;
			}
		}

		// Only columns that found NOTHING AT ALL are still waiting for streaming;
		// a column that landed on the wrong body already has its answer and is
		// judged as a malformed sample in Verify, in the same frame.
		if (uResolved != g_uCCSampleCount)
		{
			if (g_iCCFrames > iCC_PROBE_DEADLINE_FRAMES)
			{
				g_bCCProbed = false;
				FailCC("a downward probe found no ground at all under an "
					"authoritative sample -- either the terrain physics body never "
					"streamed in, or a sample XZ is outside the baked heightfield "
					"(the unresolved samples are named below)");
				return false;
			}
			return true;
		}

		g_bCCProbed = true;
		g_eCCPhase = CCPhase::Evaluate;
		g_iCCFrames = 0;
		return true;
	}

	// ONE sample, run through the SHIPPED camera maths against the SHIPPED
	// physics world. Nothing here re-implements the camera: the desired position
	// and the clamp are both the production statics.
	void CCEvaluateSample(CCSample& xSample)
	{
		const Zenith_Maths::Vector3 xCentre(xSample.m_fX,
			xSample.m_xProbe.m_fFeetY + g_fCCCapsuleHalfExtent, xSample.m_fZ);
		const Zenith_Maths::Vector3 xPivot = xCentre
			+ Zenith_Maths::Vector3(0.0f, ZM_FollowCamera::GetPivotHeight(), 0.0f);
		const Zenith_Maths::Vector3 xDesired =
			ZM_FollowCamera::ComputeDesiredPosition(xCentre, g_fCCAuthoredYaw);
		const Zenith_Maths::Vector3 xArm = xDesired - xPivot;
		const float fDesiredArm = glm::length(xArm);
		xSample.m_fDesiredArm = fDesiredArm;
		if (!(fDesiredArm > 0.0001f))
		{
			xSample.m_bViolated = true;
			xSample.m_bEvaluated = true;
			return;
		}

		const Zenith_Physics::RaycastResult xHit =
			Zenith_PhysicsQuery::RaycastIgnoring(
				xPivot, xArm / fDesiredArm, fDesiredArm, g_xCCPlayerID);
		xSample.m_bArmHit = xHit.m_bHit;
		xSample.m_fHitDistance = xHit.m_bHit ? xHit.m_fDistance : -1.0f;
		xSample.m_xBlocker = xHit.m_bHit ? xHit.m_xHitEntity : INVALID_ENTITY_ID;
		xSample.m_fClampedArm = ZM_FollowCamera::ClampArmDistance(
			fDesiredArm, xHit.m_bHit, xHit.m_fDistance);
		xSample.m_bViolated =
			xSample.m_fClampedArm < fDesiredArm * fCC_MIN_ARM_FRACTION;
		xSample.m_bEvaluated = true;
	}

	bool CCStepEvaluate()
	{
		for (u_int u = 0u; u < g_uCCSampleCount; ++u)
		{
			// A column that is not on the terrain has no standable ground, so
			// there is no arm to evaluate; Verify reports it as malformed.
			if (g_axCCSamples[u].m_xProbe.m_bHitTerrain)
			{
				CCEvaluateSample(g_axCCSamples[u]);
			}
		}
		g_bCCEvaluated = true;
		g_eCCPhase = CCPhase::Done;
		return false;
	}

	void Setup_DawnmereCameraClearance()
	{
		g_eCCPhase = CCPhase::Done;
		g_iCCFrames = 0;
		g_bCCPrereqs = false;
		g_bCCSkipped = false;
		g_bCCResolved = false;
		g_bCCProbed = false;
		g_bCCEvaluated = false;
		g_szCCFailure = "test did not reach verification";
		g_xCCTerrainID = INVALID_ENTITY_ID;
		g_xCCPlayerID = INVALID_ENTITY_ID;
		g_fCCAuthoredYaw = 0.0f;
		g_bCCYawCaptured = false;
		g_fCCCapsuleHalfExtent = -1.0f;
		g_uCCViolationCount = 0u;
		g_uCCMalformedCount = 0u;
		CCBuildSampleTable();

		Zenith_InputSimulator::ResetAllInputState();

		g_bCCPrereqs = CCRequiredAssetsPresent();
		if (!g_bCCPrereqs)
		{
			g_bCCSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_DawnmereCameraClearance] the Dawnmere scene / terrain bake is "
				"absent or incomplete -- there is no world to measure camera "
				"clearance in (run a *_True config once to bake it)");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fCC_FIXED_DT);
		g_eCCPhase = CCPhase::Resolve;
		g_xEngine.Scenes().LoadSceneByIndex(
			iCC_DAWNMERE_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_DawnmereCameraClearance(int)
	{
		if (g_eCCPhase == CCPhase::Done)
		{
			return false;
		}
		++g_iCCFrames;
		switch (g_eCCPhase)
		{
		case CCPhase::Resolve:  return CCStepResolve();
		case CCPhase::Probe:    return CCStepProbe();
		case CCPhase::Evaluate: return CCStepEvaluate();
		case CCPhase::Done:     return false;
		}
		return false;
	}

	// Every failing sample reports the whole picture: which group, which index,
	// where, the ground it measured, the heading the samples were derived at, both
	// arm lengths, the hit distance and WHAT blocked it.
	void CCReportSample(const CCSample& xSample, const char* szWhy)
	{
		char acBlocker[96];
		CCDescribeEntity(xSample.m_xBlocker, acBlocker);
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereCameraClearance] %s | group='%s' index=%u xz=(%.3f, %.3f) "
			"feetY=%.5f authoredYaw=%.6f desiredArm=%.4f clampedArm=%.4f "
			"(needs >= %.4f) hit=%d hitDistance=%.4f blocker='%s'",
			szWhy, g_aszCCGroupNames[xSample.m_uGroup], xSample.m_uIndexInGroup,
			(double)xSample.m_fX, (double)xSample.m_fZ,
			(double)xSample.m_xProbe.m_fFeetY, (double)g_fCCAuthoredYaw,
			(double)xSample.m_fDesiredArm, (double)xSample.m_fClampedArm,
			(double)(xSample.m_fDesiredArm * fCC_MIN_ARM_FRACTION),
			(int)xSample.m_bArmHit, (double)xSample.m_fHitDistance, acBlocker);
	}

	bool Verify_DawnmereCameraClearance()
	{
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();

		if (g_bCCSkipped)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] SKIPPED -- no baked Dawnmere terrain, so "
				"the camera-arm clearance contract is UNVERIFIED on this run.");
			return true;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereCameraClearance] samples=%u of %u slots authoredYaw=%.6f "
			"capsuleHalfExtent=%.4f minArmFraction=%.2f",
			g_uCCSampleCount, uCC_MAX_SAMPLES, (double)g_fCCAuthoredYaw,
			(double)g_fCCCapsuleHalfExtent, (double)fCC_MIN_ARM_FRACTION);
		// One line per group, so adding a region cannot outgrow the format string
		// (the five-group version of this log was a hand-written arg list).
		for (u_int u = 0u; u < (u_int)CC_GROUP_COUNT; ++u)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance]   group='%s' samples=%u",
				g_aszCCGroupNames[u], g_auCCGroupCounts[u]);
		}

		bool bPassed = true;

		// A truncated table would silently shrink coverage while still passing.
		if (g_bCCSampleOverflow)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] the sample table overflowed its %u-slot "
				"budget -- coverage was TRUNCATED, so a pass would be meaningless",
				uCC_MAX_SAMPLES);
		}

		// ...and so would a coverage group that never got built at all.
		if (g_bCCLabPathMissing)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] the Dawnmere terrain recipe has no "
				"usable path named '%s', so the lab walkway group is EMPTY -- a "
				"renamed or deleted recipe path silently removes coverage",
				szCC_LAB_PATH_NAME);
		}

		// Every group must actually carry samples. A group whose builder was
		// removed reports zero here rather than passing by having nothing to check.
		for (u_int u = 0u; u < (u_int)CC_GROUP_COUNT; ++u)
		{
			if (g_auCCGroupCounts[u] == 0u)
			{
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereCameraClearance] group '%s' contributed NO samples -- "
					"its coverage is vacuous", g_aszCCGroupNames[u]);
			}
		}

		const bool bPhasesComplete = g_bCCPrereqs && g_bCCResolved
			&& g_bCCProbed && g_bCCEvaluated;
		if (!bPhasesComplete)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] %s (prereqs=%d resolved=%d probed=%d "
				"evaluated=%d)",
				g_szCCFailure, (int)g_bCCPrereqs, (int)g_bCCResolved,
				(int)g_bCCProbed, (int)g_bCCEvaluated);
			// Fall through: the per-sample loop below names WHICH samples are bad,
			// which is the information a phase-level message cannot carry.
		}

		// ★ THE YAW GUARD. Every sample DIRECTION was derived at the authored
		// heading; if the scene no longer carries it, the whole table is measuring
		// rays the game does not cast, and a green result would be an artefact.
		if (!g_bCCYawCaptured
			|| std::fabs(g_fCCAuthoredYaw - fZM_DAWNMERE_AUTHORED_CAMERA_YAW)
				> fCC_YAW_EPSILON)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] the committed Dawnmere camera captured "
				"yaw %.6f, not the authored %.6f (captured=%d, epsilon %.4f) -- every "
				"sample direction in this table was derived at the authored heading, "
				"so a scene yaw edit invalidates all of them",
				(double)g_fCCAuthoredYaw, (double)fZM_DAWNMERE_AUTHORED_CAMERA_YAW,
				(int)g_bCCYawCaptured, (double)fCC_YAW_EPSILON);
		}

		if (g_fCCCapsuleHalfExtent <= 0.0f)
		{
			bPassed = false;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] the player's capsule half-extent "
				"resolved to %.4f -- every pivot height below would be nonsense",
				(double)g_fCCCapsuleHalfExtent);
		}

		// ★ A MALFORMED SAMPLE AND A VIOLATED SAMPLE ARE DIFFERENT FINDINGS AND ARE
		// REPORTED AS SUCH. "The camera would clip here" is a placement defect;
		// "this column has no standable ground" is a defect in the TABLE (or a
		// building standing on a route), and printing the second as the first --
		// with a fails-closed -1e9 feet height and no blocker -- reads as a
		// nonsense clearance number and hides the real cause.
		for (u_int u = 0u; u < g_uCCSampleCount; ++u)
		{
			const CCSample& xSample = g_axCCSamples[u];
			if (!xSample.m_xProbe.m_bResolved)
			{
				++g_uCCMalformedCount;
				bPassed = false;
				if (g_uCCMalformedCount <= 12u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DawnmereCameraClearance] MALFORMED SAMPLE | group='%s' "
						"index=%u xz=(%.3f, %.3f): the down-ray found NO ground at all "
						"within its %.1f m window -- the terrain body never streamed "
						"in here, or this XZ is outside the baked heightfield",
						g_aszCCGroupNames[xSample.m_uGroup], xSample.m_uIndexInGroup,
						(double)xSample.m_fX, (double)xSample.m_fZ,
						(double)fCC_RAY_MAX_DISTANCE);
				}
				continue;
			}
			if (!xSample.m_xProbe.m_bHitTerrain)
			{
				++g_uCCMalformedCount;
				bPassed = false;
				if (g_uCCMalformedCount <= 12u)
				{
					char acFinal[96];
					CCDescribeEntity(xSample.m_xProbe.m_xFinalHitEntity, acFinal);
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_DawnmereCameraClearance] MALFORMED SAMPLE | group='%s' "
						"index=%u xz=(%.3f, %.3f): the down-ray ended on '%s', not on "
						"'%s' -- this column is under a solid body, so it is not a "
						"place a player can stand and the route or the table is wrong",
						g_aszCCGroupNames[xSample.m_uGroup], xSample.m_uIndexInGroup,
						(double)xSample.m_fX, (double)xSample.m_fZ, acFinal,
						szCC_TERRAIN_ENTITY);
				}
				continue;
			}
			if (!xSample.m_bEvaluated)
			{
				++g_uCCMalformedCount;
				bPassed = false;
				CCReportSample(xSample, "SAMPLE HAS GROUND BUT WAS NEVER EVALUATED");
				continue;
			}
			if (!xSample.m_bViolated)
			{
				continue;
			}
			++g_uCCViolationCount;
			bPassed = false;
			if (g_uCCViolationCount <= 12u)
			{
				CCReportSample(xSample,
					"CAMERA ARM CLEARANCE VIOLATED");
			}
		}
		if (g_uCCMalformedCount > 12u)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] ...and %u further malformed samples "
				"were not individually reported (total %u of %u)",
				g_uCCMalformedCount - 12u, g_uCCMalformedCount, g_uCCSampleCount);
		}

		if (g_uCCViolationCount > 12u)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereCameraClearance] ...and %u further violated samples were "
				"not individually reported (total %u of %u)",
				g_uCCViolationCount - 12u, g_uCCViolationCount, g_uCCSampleCount);
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereCameraClearance] result: violations=%u malformed=%u of %u "
			"samples",
			g_uCCViolationCount, g_uCCMalformedCount, g_uCCSampleCount);
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMDawnmereCameraClearanceTest = {
	"ZM_DawnmereCameraClearance_Test",
	&Setup_DawnmereCameraClearance,
	&Step_DawnmereCameraClearance,
	&Verify_DawnmereCameraClearance,
	/* maxFrames */ 2400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDawnmereCameraClearanceTest);

// ============================================================================
// (3) ZM_DawnmereLabGroundTruth_Test (S8 SC-D) -- the LAB measurement oracle.
//
// Same job as (1) for the lab site: cast a real downward ray at every column the
// lab placement names, LOG every value at INFO on every run, and red if a
// compiled row in Source/World/ZM_DawnmerePlacement.h has drifted from the
// surface the world actually has.
//
// ★★ THIS TEST RUNS TWICE IN ITS LIFETIME, WITH DIFFERENT MEANINGS, AND EVERY
// STRUCTURAL DECISION BELOW EXISTS TO SERVE BOTH.
//
//   RUN 1 -- NOW (SC-D), WITH NO LAB GEOMETRY IN THE WORLD AT ALL. Dawnmere
//   contains no shell, no jambs, no lintel and no lab sensor; this run measures
//   PURE TERRAIN and produces the ten numbers the placement table is frozen
//   from. On this run the table it is checking against is a deliberate
//   PLACEHOLDER, so the height clause is EXPECTED TO RED until those numbers are
//   pasted in and the binary rebuilt. That is the measure -> freeze -> rebuild
//   loop, and this test is its instrument.
//
//   RUN 2 -- AFTER SC-E, WITH THE SHELL AUTHORED. The same ten columns are
//   re-measured with the shell IGNORED, exactly as the Home oracle ignores the
//   Home shell, and the run confirms the frozen table still matches the ground.
//
// ★ SO AN ABSENT LAB SHELL IS "THERE IS NO BODY TO IGNORE", NEVER A FAILURE, and
// getting that wrong would have been fatal rather than untidy. The Home oracle's
// resolve step FAILS when 'DawnmereHomeShell' is missing, which is correct THERE
// -- that shell already exists and is only ever being MOVED. Copying that arm
// here would hard-fail this test on the one run it exists to perform. So the
// resolve step below requires only the terrain, records whether the lab shell
// happens to exist, and hands an INVALID entity id to the probe when it does not
// (Zenith_PhysicsQuery::RaycastIgnoring falls back to an unfiltered raycast for
// INVALID_ENTITY_ID -- "ignore nothing" is a supported request, not an error).
//
// ★ AND THE DOOR COLUMNS' HALF-METRE OFFSET IS NOT JUSTIFIED BY THIS RUN. The
// Home block's reasoning -- "a jamb's own column has two solid bodies over it and
// RaycastIgnoring takes only one" -- is about geometry that does not exist yet.
// It is why the lab's door rows are offset ANYWAY (so run 2 can measure the same
// columns run 1 froze); see the geometry note in ZM_DawnmerePlacement.cpp.
//
// ★ THE PROBE WINDOW IS SIZED FROM THE TOWN-CENTRE ANCHOR, NOT FROM THE ROW
// BEING MEASURED, AND THAT IS LOAD-BEARING ON RUN 1. The Home oracle centres each
// column's ray on that column's own compiled height, which works because those
// heights are real. Doing that here would start every lab ray a million metres
// below the world (the placeholder), find nothing, and time out at the phase
// deadline WITHOUT EVER PRINTING A MEASUREMENT -- i.e. the freeze loop could
// never close. The camera-clearance test above already sizes its window from
// fZM_DAWNMERE_TOWN_CENTER_FEET_Y for the same reason (it measures columns with
// no compiled height at all), so this is the established pattern rather than a
// special case. The resulting window brackets the whole graded band the boot unit
// LabGroundSamples_AreTenMeasurementsInsideTheGradedBand enforces.
//
// ★ SKIPS ON THE GITIGNORED BAKE ONLY. A missing Dawnmere.zscen is a missing
// TRACKED asset, which is a defect and is reported as a failure -- see the
// CCTerrainBakePresent / CCCommittedDawnmereScenePresent split at the top of this
// file.
// ============================================================================

namespace
{
	enum class LGTPhase { Resolve, Measure, Done };

	LGTPhase g_eLGTPhase = LGTPhase::Done;
	int  g_iLGTFrames = 0;
	bool g_bLGTBakePresent = false;
	bool g_bLGTScenePresent = false;
	bool g_bLGTSkipped = false;
	bool g_bLGTResolved = false;
	bool g_bLGTMeasured = false;
	bool g_bLGTShellPresent = false;
	const char* g_szLGTFailure = "test did not reach verification";
	Zenith_EntityID g_xLGTTerrainID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xLGTShellID = INVALID_ENTITY_ID;

	// ★ THE LAB GETS ITS OWN SLOT ARRAY AND ITS OWN static_assert. The Home probe
	// array above is bounded by uHGT_SAMPLE_SLOTS with an assert that only counts
	// HOME rows, and every loop over it reads `u < uCount && u < SLOTS` -- so lab
	// rows appended to the Home enum would have been measured up to the slot bound
	// and silently DROPPED beyond it, with nothing able to see the truncation.
	constexpr u_int uLGT_SAMPLE_SLOTS = 16u;
	static_assert(uLGT_SAMPLE_SLOTS >= (u_int)ZM_DAWNMERE_LAB_SAMPLE_COUNT,
		"the lab ground-truth probe needs one slot per lab ground sample");
	CCGroundProbe g_axLGTProbes[uLGT_SAMPLE_SLOTS];

	void FailLGT(const char* szReason)
	{
		g_szLGTFailure = szReason;
		g_eLGTPhase = LGTPhase::Done;
	}

	bool LGTStepResolve()
	{
		Zenith_SceneData* pxData = nullptr;
		if (!CCDawnmereIsActive(pxData))
		{
			if (g_iLGTFrames > iCC_RESOLVE_DEADLINE_FRAMES)
			{
				FailLGT("Dawnmere never became the active scene");
				return false;
			}
			return true;
		}

		// The terrain is the ONLY required entity: it is what every probe must
		// terminate on, and its absence means the committed scene is not the scene
		// this test was written against.
		const Zenith_Entity xTerrain = CCFindEntity(pxData, szCC_TERRAIN_ENTITY);
		if (!xTerrain.IsValid())
		{
			if (g_iLGTFrames > iCC_RESOLVE_DEADLINE_FRAMES)
			{
				FailLGT("the committed Dawnmere does not contain 'DawnmereTerrain' -- "
					"a renamed or deleted authored entity, not a missing bake");
				return false;
			}
			return true;
		}

		// OPTIONAL, and the whole point of this test's structure: pre-SC-E there is
		// no lab shell, so an invalid id here means "there is no body to ignore".
		const Zenith_Entity xShell = CCFindEntity(pxData, szCC_LAB_SHELL_ENTITY);
		g_bLGTShellPresent = xShell.IsValid();
		g_xLGTShellID = g_bLGTShellPresent ? xShell.GetEntityID() : INVALID_ENTITY_ID;

		g_xLGTTerrainID = xTerrain.GetEntityID();
		g_bLGTResolved = true;
		g_eLGTPhase = LGTPhase::Measure;
		g_iLGTFrames = 0;
		return true;
	}

	bool LGTStepMeasure()
	{
		const u_int uCount = ZM_GetDawnmereLabSampleCount();
		u_int uResolved = 0u;
		for (u_int u = 0u; u < uCount && u < uLGT_SAMPLE_SLOTS; ++u)
		{
			if (g_axLGTProbes[u].m_bResolved)
			{
				++uResolved;
				continue;
			}
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereLabSample(u);
			// The reference height sizes the WINDOW only, and it is deliberately
			// NOT xSample.m_fFeetY -- see the block comment above.
			g_axLGTProbes[u] = CCProbeGroundAt(xSample.m_fX, xSample.m_fZ,
				fZM_DAWNMERE_TOWN_CENTER_FEET_Y, g_xLGTShellID, g_xLGTTerrainID);
			if (g_axLGTProbes[u].m_bResolved)
			{
				++uResolved;
			}
		}

		// Only columns that found NOTHING are still waiting for the terrain body to
		// stream in; a column that landed on the wrong body is already decided and
		// is judged in Verify.
		if (uResolved != uCount)
		{
			if (g_iLGTFrames > iCC_PROBE_DEADLINE_FRAMES)
			{
				FailLGT("a downward probe found no ground at all under a lab placement "
					"column -- either the terrain physics body never streamed in, or "
					"that column lies outside the baked heightfield (per-column detail "
					"below)");
				return false;
			}
			return true;
		}

		g_bLGTMeasured = true;
		g_eLGTPhase = LGTPhase::Done;
		return false;
	}

	void Setup_DawnmereLabGroundTruth()
	{
		g_eLGTPhase = LGTPhase::Done;
		g_iLGTFrames = 0;
		g_bLGTBakePresent = false;
		g_bLGTScenePresent = false;
		g_bLGTSkipped = false;
		g_bLGTResolved = false;
		g_bLGTMeasured = false;
		g_bLGTShellPresent = false;
		g_szLGTFailure = "test did not reach verification";
		g_xLGTTerrainID = INVALID_ENTITY_ID;
		g_xLGTShellID = INVALID_ENTITY_ID;
		for (u_int u = 0u; u < uLGT_SAMPLE_SLOTS; ++u)
		{
			g_axLGTProbes[u] = CCGroundProbe();
		}

		Zenith_InputSimulator::ResetAllInputState();

		// The ONE skip, and it is narrow on purpose: the GITIGNORED heightfield
		// this test measures against does not exist. RequestSkip bypasses Verify,
		// so no fixed-dt or scene-load state may be installed before this point.
		g_bLGTBakePresent = CCTerrainBakePresent();
		if (!g_bLGTBakePresent)
		{
			g_bLGTSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_DawnmereLabGroundTruth] the Dawnmere terrain bake is absent or "
				"incomplete -- there is no heightfield to measure the lab placement "
				"against (run a *_True config once to bake it)");
			return;
		}

		// NOT a skip. Dawnmere.zscen is one of the six TRACKED assets; its absence
		// is a deleted committed file, and skipping would turn that into a pass on
		// the exact tree where it must never happen.
		g_bLGTScenePresent = CCCommittedDawnmereScenePresent();
		if (!g_bLGTScenePresent)
		{
			FailLGT("the COMMITTED Assets/Scenes/Dawnmere" ZENITH_SCENE_EXT
				" is missing -- that is a deleted tracked asset, not a missing bake, "
				"so this run FAILS rather than skipping");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fCC_FIXED_DT);
		g_eLGTPhase = LGTPhase::Resolve;
		g_xEngine.Scenes().LoadSceneByIndex(
			iCC_DAWNMERE_BUILD_INDEX, SCENE_LOAD_SINGLE);
	}

	bool Step_DawnmereLabGroundTruth(int)
	{
		if (g_eLGTPhase == LGTPhase::Done)
		{
			return false;
		}
		++g_iLGTFrames;
		switch (g_eLGTPhase)
		{
		case LGTPhase::Resolve: return LGTStepResolve();
		case LGTPhase::Measure: return LGTStepMeasure();
		case LGTPhase::Done:    return false;
		}
		return false;
	}

	bool Verify_DawnmereLabGroundTruth()
	{
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::ClearFixedDt();

		if (g_bLGTSkipped)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereLabGroundTruth] SKIPPED -- no baked Dawnmere terrain, so "
				"nothing was measured. The S8 SC-D lab ground table in "
				"Source/World/ZM_DawnmerePlacement.cpp is UNVERIFIED on this run, and "
				"if it still holds its placeholder it CANNOT be frozen from a run that "
				"skipped.");
			return true;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereLabGroundTruth] mode=%s (labShellPresent=%d) -- %s",
			g_bLGTShellPresent ? "POST-SC-E" : "PRE-SC-E",
			(int)g_bLGTShellPresent,
			g_bLGTShellPresent
				? "the authored lab shell was found and is IGNORED by every probe"
				: "no lab shell exists yet, so every probe measures pure terrain "
					"with nothing to ignore");

		// ★ THE PASTE-READY LOG, EMITTED ON EVERY RUN, PASS OR FAIL. This is how the
		// SC-D table is obtained in the first place and re-obtained after a terrain
		// change: replace the row's fZM_DAWNMERE_LAB_GROUND_UNMEASURED initialiser
		// in Source/World/ZM_DawnmerePlacement.cpp with the `paste=` literal, keying
		// on the row NAME (the table is in this same order).
		const u_int uCount = ZM_GetDawnmereLabSampleCount();
		for (u_int u = 0u; u < uCount && u < uLGT_SAMPLE_SLOTS; ++u)
		{
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereLabSample(u);
			char acFinal[96];
			CCDescribeEntity(g_axLGTProbes[u].m_xFinalHitEntity, acFinal);
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereLabGroundTruth] MEASURED FEET Y -- PASTE row %u of the "
				"S8 SC-D LAB GROUND table in Source/World/ZM_DawnmerePlacement.cpp: "
				"name=%s paste=%.5ff xz=(%.3f, %.3f) table=%.5f tableError=%.5f | "
				"resolved=%d hitTerrain=%d finalHit='%s'",
				u, xSample.m_szEntityName, (double)g_axLGTProbes[u].m_fFeetY,
				(double)xSample.m_fX, (double)xSample.m_fZ, (double)xSample.m_fFeetY,
				(double)(g_axLGTProbes[u].m_fFeetY - xSample.m_fFeetY),
				(int)g_axLGTProbes[u].m_bResolved,
				(int)g_axLGTProbes[u].m_bHitTerrain, acFinal);
		}

		// The five DERIVED authored Y values, so a freeze round can be checked
		// without re-deriving the formulas by hand. These are what SC-E authors.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_DawnmereLabGroundTruth] DERIVED authored Y: shell=%.6f "
			"doorLeft=%.6f doorRight=%.6f lintel=%.6f trigger=%.6f spawnFeet=%.6f",
			(double)ZM_GetDawnmereLabShell().m_xCenter.y,
			(double)ZM_GetDawnmereLabDoorLeft().m_xCenter.y,
			(double)ZM_GetDawnmereLabDoorRight().m_xCenter.y,
			(double)ZM_GetDawnmereLabDoorLintel().m_xCenter.y,
			(double)ZM_GetDawnmereLabDoorTrigger().m_xCenter.y,
			(double)ZM_GetDawnmereFromLabSpawnFeet().y);

		if (!g_bLGTBakePresent || !g_bLGTScenePresent || !g_bLGTResolved
			|| !g_bLGTMeasured)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmereLabGroundTruth] %s (bake=%d scene=%d resolved=%d "
				"measured=%d)",
				g_szLGTFailure, (int)g_bLGTBakePresent, (int)g_bLGTScenePresent,
				(int)g_bLGTResolved, (int)g_bLGTMeasured);
			return false;
		}

		bool bPassed = true;
		for (u_int u = 0u; u < uCount && u < uLGT_SAMPLE_SLOTS; ++u)
		{
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereLabSample(u);
			// ★ THE MEASUREMENT MUST BE GROUND, AND THE ONLY BODY THIS RUN IS
			// ALLOWED TO LOOK PAST IS THE (POSSIBLY ABSENT) LAB SHELL. Anything else
			// under a lab column -- a neighbouring authored block, a prop, an NPC --
			// would silently turn a body TOP into a "terrain height" and freeze it
			// into the placement table, so it is named and failed here.
			if (!g_axLGTProbes[u].m_bHitTerrain
				|| g_axLGTProbes[u].m_xFinalHitEntity != g_xLGTTerrainID)
			{
				char acFinal[96];
				CCDescribeEntity(g_axLGTProbes[u].m_xFinalHitEntity, acFinal);
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereLabGroundTruth] the probe under '%s' terminated on "
					"'%s' rather than on '%s' -- the measurement would be a body top, "
					"not a ground height (the lab shell is the ONLY body a lab column "
					"may legitimately carry, and it is already ignored)",
					xSample.m_szEntityName, acFinal, szCC_TERRAIN_ENTITY);
				continue;
			}
			const float fError =
				std::fabs(g_axLGTProbes[u].m_fFeetY - xSample.m_fFeetY);
			if (fError > fCC_HEIGHT_TOLERANCE)
			{
				bPassed = false;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_DawnmereLabGroundTruth] '%s': the compiled feet height %.5f "
					"is %.5f m off the terrain surface %.5f (tolerance %.3f). If it "
					"reads %.1f this table is still the SC-D PLACEHOLDER and has never "
					"been frozen -- paste the `paste=` literals above into the S8 SC-D "
					"LAB GROUND block in Source/World/ZM_DawnmerePlacement.cpp and "
					"rebuild.",
					xSample.m_szEntityName, (double)xSample.m_fFeetY, (double)fError,
					(double)g_axLGTProbes[u].m_fFeetY, (double)fCC_HEIGHT_TOLERANCE,
					(double)fZM_DAWNMERE_LAB_GROUND_UNMEASURED);
			}
		}
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMDawnmereLabGroundTruthTest = {
	"ZM_DawnmereLabGroundTruth_Test",
	&Setup_DawnmereLabGroundTruth,
	&Step_DawnmereLabGroundTruth,
	&Verify_DawnmereLabGroundTruth,
	// Both waiting phases own a deadline that FAILS with a diagnostic; this cap is
	// only a backstop above their sum.
	/* maxFrames */ 2400,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDawnmereLabGroundTruthTest);

#endif // ZENITH_INPUT_SIMULATOR
