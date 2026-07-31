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

	bool CCRequiredAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 7> astrRequired = {
			strRoot + "Scenes/Dawnmere" ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			std::error_code xError;
			if (!std::filesystem::is_regular_file(strPath, xError) || xError)
			{
				return false;
			}
			const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
			if (xError || ulSize == 0u)
			{
				return false;
			}
		}
		return true;
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

		CC_GROUP_COUNT
	};

	const char* g_aszCCGroupNames[CC_GROUP_COUNT] = {
		"townCentre->doorStaging",
		"doorStaging->doorTrigger",
		"homeDirtPath",
		"fromHomeSpawnRing",
		"townCentreRing",
	};

	// Sized for the authoritative table plus generous headroom; overflow is
	// REPORTED as a failure rather than silently truncating coverage.
	constexpr u_int uCC_MAX_SAMPLES = 512u;
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
		// The PLAYER'S own scale, never a literal: a re-scaled player must be
		// judged against ITS capsule.
		g_fCCCapsuleHalfExtent =
			ZM_PlayerController::CalculateCapsuleHalfExtent(xPlayerScale);
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
			"[ZM_DawnmereCameraClearance] samples=%u (%s=%u, %s=%u, %s=%u, %s=%u, "
			"%s=%u) authoredYaw=%.6f capsuleHalfExtent=%.4f minArmFraction=%.2f",
			g_uCCSampleCount,
			g_aszCCGroupNames[CC_GROUP_TOWN_TO_STAGING],
			g_auCCGroupCounts[CC_GROUP_TOWN_TO_STAGING],
			g_aszCCGroupNames[CC_GROUP_STAGING_TO_TRIGGER],
			g_auCCGroupCounts[CC_GROUP_STAGING_TO_TRIGGER],
			g_aszCCGroupNames[CC_GROUP_HOME_DIRT_PATH],
			g_auCCGroupCounts[CC_GROUP_HOME_DIRT_PATH],
			g_aszCCGroupNames[CC_GROUP_SPAWN_RING],
			g_auCCGroupCounts[CC_GROUP_SPAWN_RING],
			g_aszCCGroupNames[CC_GROUP_TOWN_RING],
			g_auCCGroupCounts[CC_GROUP_TOWN_RING],
			(double)g_fCCAuthoredYaw, (double)g_fCCCapsuleHalfExtent,
			(double)fCC_MIN_ARM_FRACTION);

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

#endif // ZENITH_INPUT_SIMULATOR
