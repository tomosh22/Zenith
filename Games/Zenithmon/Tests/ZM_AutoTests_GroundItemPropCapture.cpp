#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_GroundItemPropCapture (ZM-67) -- the BEFORE/AFTER CAPTURE HARNESS
// for the three Route 1 ground-item props.
//
// ★ WHAT IT IS FOR. ZM-67 replaced the three props' ZM_GreyboxVisual blockout
// cubes with purpose-built meshes and materials, and it is a `human-gate` ticket:
// a person signs the visual result off. A sign-off with no BEFORE image is a
// sign-off of nothing -- so this file exists to produce a COMPARABLE PAIR of runs:
// photograph the props, change something about how they look, photograph them
// again, and put the two sets side by side.
//
// ★ IT KNOWS NOTHING ABOUT WHAT A PROP LOOKS LIKE, AND MUST NOT LEARN. It aims the
// shipped lens at whatever the scene has authored and dumps the framebuffer. That
// is what lets one build of it photograph blockout cubes and the next photograph
// generated models, with no edit in between -- which is the only way two captures
// are of the same thing.
//
// ★ SO THE FRAMING IS THE CONTRACT, NOT THE CONTENT. Two captures are comparable
// only while the lens derivation, the azimuth, the settle budget and the shot
// stems are unchanged; move any of them and a difference between two runs stops
// meaning "the prop changed". If a framing choice genuinely has to move, both
// halves of the comparison have to be re-shot from the same build.
//
// (This file was WRITTEN one commit ahead of the visual change so a BEFORE set
// could be taken off a tree that did not yet have it. That was a dispatch
// ordering, not a property of the repo: both halves ship in the same commit, and
// nothing in this file is "stage 1" of anything. Do not read the captures it
// produces today as pictures of grey cubes.)
//
// ★ IT MOVES THE CAMERA. IT NEVER MOVES, CREATES OR TELEPORTS ANYTHING ELSE.
// Tests/ZM_AutoTests_GroundItemProp.cpp states the rule this file inherits: an
// entity-creating boot would re-author different .zscen bytes for every committed
// scene in the game, and a windowed *_True boot AUTHORS before it tests. So there
// is no CreateEntity and no SetPosition on any subject in this file. The camera it
// drives is the one the scene already authored -- szZM_ROUTE1_CAMERA_ENTITY_NAME.
//
// ---- THE ONE MECHANISM THAT MAKES THAT POSSIBLE ---------------------------
//
// ★★ THE AUTHORED CAMERA IS DRIVEN BY ZM_FollowCamera, AND A TEST Step CANNOT
// OUT-WRITE IT. Zenith_Core::Zenith_MainLoop runs PumpAutomatedTest (this file's
// Step) BEFORE UpdateGameLogic, and ZM_FollowCamera::OnLateUpdate writes
// SetPosition/SetYaw/SetPitch onto the camera from inside that later block -- so a
// pose written here is overwritten in the SAME frame, before anything renders.
// A capture harness that simply called SetPosition would photograph the player's
// back three times and pass every structural check.
//
// The fix is the mechanism the game already ships for exactly this: the scene is
// PAUSED. Zenith_SceneSystem::SetScenePaused gates ONLY the ECS update dispatch --
// ZM_BattleTransition.h says so at its round-trip note, and uses it to freeze the
// overworld while a battle owns the screen. With Route 1 paused:
//   * ZM_FollowCamera::OnLateUpdate does not run, so the pose written here STANDS;
//   * the render snapshot is unaffected -- Zenith_FillSceneSnapshotImpl walks
//     QueryAllScenes<Zenith_ModelComponent>, which is pause-independent, so the
//     paused scene still draws;
//   * terrain streaming is unaffected -- Zenith_TerrainComponent has no OnUpdate;
//     streaming is driven by the Flux terrain feature off the live camera, which
//     is what lets this harness jump the lens 1,000 m up the route between shots;
//   * ZM_GreyboxVisual's only OnUpdate is gated to the PROP population and does
//     nothing unless a prop's desired MODEL changes (ZM-67), and OnStart already
//     built the one this save wants -- so a paused prop is a fully-built prop.
// Physics keeps stepping globally, exactly as it does behind a battle. Nothing in
// this file writes a position, so there is nothing for it to fight over.
//
// ★ AND THE FREEZE IS ASSERTED, NOT ASSUMED. GIPCTakeShot re-reads the LIVE camera
// position immediately before requesting the dump and fails if it has drifted from
// the pose GIPCAimAtSubject wrote. If the pause ever stops gating OnLateUpdate,
// this test goes RED naming ZM_FollowCamera instead of quietly dumping three
// pictures of the player.
//
// ---- THE FRAMING, AND THE JUDGEMENT IT IS FOR ------------------------------
//
// ZM-67's Definition of Done asks that the three props be "distinguishable from
// each other on sight at the distance a player walks past them". So the lens is
// put where the SHIPPED follow camera would put it if the player were standing on
// that prop -- derived from ZM_FollowCamera's own constants, never from numbers
// invented here:
//
//   eye height above the ground under the prop
//       = fZM_HUMAN_BODY_HALF_HEIGHT            (player transform IS the body CENTRE)
//       + ZM_FollowCamera::GetCameraHeight()    (the lens sits this far above it)
//   eye horizontal distance from the prop
//       = ZM_FollowCamera::GetArmLength()       (ComputeDesiredPosition at yaw 0 is
//                                                exactly (0, +height, -arm))
//   vertical field of view
//       = ZM_FollowCamera::GetFOVDegrees()      (CaptureAuthoredYaw forces this FOV
//                                                on the authored camera every frame)
//
// ★ THAT IS DELIBERATELY NOT A MACRO CLOSE-UP, AND THE COST IS REAL AND BOOKED. At
// ~6.6 m through a 65-degree frame a 0.6 m prop (fZM_ROUTE1_PROP_CUBE_EDGE) is
// about 7% of frame height -- roughly 75 px on a 1080-tall capture. A tighter
// framing would make every candidate mesh look good and would hide the exact defect
// the ticket exists to fix, which is whether these things read AT ALL from the
// lane. If a reviewer cannot judge MATERIAL at this range, that is a finding about
// the material, not about this harness. Do not zoom in to make the pictures nicer.
//
// The one genuinely free choice is the AZIMUTH: a lens dead astern of a cube sees
// one face. fGIPC_VIEW_AZIMUTH_DEGREES swings the shipped -Z trail direction round
// so two faces and the top are visible, which is what a silhouette judgement needs.
// It is a photographic decision belonging to this file and to nothing else.
//
// ---- WHAT IT ASSERTS, SO A REAL FAILURE IS NOT MISTAKEN FOR A SKIP ----------
//
// ★★ m_bRequiresGraphics = true, BECAUSE IT READS AND WRITES PIXELS BY DEFINITION,
// AND THAT MAKES IT SILENT WHERE CI LOOKS. A requiresGraphics test is SKIPPED on
// the Null backend and a skip COUNTS AS A PASS, so the headless zm-tests gate can
// never see this file rot. That is a known failure class in this repo (11 of 12
// graphics-gated tests had rotted before they were swept). It is why every clause
// below is a hard failure with a named diagnostic rather than a log line, and why
// this test only means anything on a windowed Vulkan_*_True run whose OUTPUT FILES
// were actually looked at.
//
// Asserted, each naming what broke:
//   1. the compiled Dawnmere row yields an inbound Route 1 spawn tag;
//   2. RequestWarp is accepted, and the warp completes, inside their deadlines;
//   3. Route 1 is the settled active scene and has scene data;
//   4. the authored camera entity exists and carries a Zenith_CameraComponent;
//   5. all THREE prop entities exist, carry a ZM_GroundItemProp, and carry the id
//      their placement row names (a scene that authored one and dropped two would
//      otherwise produce one good picture and two silent nothings);
//   6. the camera pose written for a shot is still the live pose when the shot is
//      taken (the ZM_FollowCamera freeze guard above);
//   7. the prop's centre projects inside the safe viewport interior -- i.e. the
//      subject is IN FRAME. A null A/B diff is usually a subject that was not in
//      frame, and that is not something a human notices from a filename;
//   8. every shot landed on disk AND re-loads as a valid Flux_Screenshot TGA, with
//      the prop's projected patch inside the image bounds.
//
// ★ CLAUSE 8 IS THE STAND-IN FOR "the dump call returned false", WHICH DOES NOT
// EXIST: Flux_Screenshot::RequestDump returns void. Asserting that a readable file
// with the subject inside it landed on disk is strictly stronger than a return
// code would have been, so nothing is lost by the substitution.
//
// LOGGED, never asserted: the mean RGB and red/blue of the patch at each prop's
// projected centre, the eye pose, the viewport rect and the three file paths. They
// are CONTEXT for the human sign-off and a datum the OTHER half of a comparison can
// be quoted against -- an absolute framebuffer ratio tracks the scene's lighting
// rather than the prop (the retracted premise recorded at length in
// ZM_AutoTests_PlayerHomeTintPixels.cpp), so pinning one here would red this test
// every time the atmosphere is re-tuned.
//
// ★ NO GRAPHICS OPTION IS TOUCHED. Auto-exposure, bloom, TAA, the skybox and the UI
// all run AS SHIPPED, because the question a sign-off answers is what the PLAYER
// sees, not what a studio rig can be made to show.
//
// ★ THE SHOTS ARE DELETED AT Setup. A stale TGA from a previous run that silently
// survived a failed capture would be pasted into a work log as though it were
// fresh -- and in a BEFORE/AFTER comparison a stale file is worse than no file.
// Which also means: EVERY RUN DESTROYS THE PREVIOUS RUN'S CAPTURES, so the first
// half of a comparison must be copied out of the capture directory before the
// second half is run. There is no second directory and no run counter; the
// deletion is what keeps a failed capture from being mistaken for a fresh one.
// ============================================================================

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/Zenith_TestTGA.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"      // the SHIPPED framing constants
#include "Zenithmon/Components/ZM_GameStateManager.h"  // the warp into Route 1
#include "Zenithmon/Components/ZM_GroundItemProp.h"    // the component a prop must carry
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"        // build index + terrain set, READ never spelled
#include "Zenithmon/Source/World/ZM_GroundItem.h"      // ZM_GroundItemName, for the diagnostics
#include "Zenithmon/Source/World/ZM_HumanBody.h"       // THE body contract -- the eye height derivation
#include "Zenithmon/Source/World/ZM_Route1Placement.h" // entity names + the prop cube edge

namespace
{
	constexpr float fGIPC_FIXED_DT = 1.0f / 60.0f;

	// The warp barrier's frame budget, the same figure and for the same reason as
	// ZM_AutoTests_GroundItemProp's: ZM_GameStateManager stalls in
	// WAITING_FOR_SCENE / WAITING_FOR_SPAWN rather than failing loudly (ZM-D-200),
	// so this deadline is what turns a warp to nowhere into a diagnostic.
	constexpr int iGIPC_WARP_DEADLINE_FRAMES = 900;

	// The props and the camera are all authored, so they exist the instant the
	// scene is activated; this budget only absorbs the activation itself.
	constexpr int iGIPC_RESOLVE_DEADLINE_FRAMES = 300;

	// ★ TWO SECONDS PER SHOT, AND EVERY ONE OF THEM IS SPENT ON SOMETHING. Between
	// two consecutive shots the lens jumps up to ~1,000 m along the route, which
	// re-triggers terrain LOD streaming, invalidates the whole TAA history and
	// leaves auto-exposure adapting. Auto-exposure alone is the binding constraint:
	// ZM_AutoTests_ShellLighting measured 120 frames of fixed dt (speed 4/s over
	// 2 s) as full adaptation, and this file inherits that figure rather than
	// re-deriving it. A shorter settle produces a dark, smeared, half-streamed
	// picture that a reviewer would read as a defect in the PROP.
	constexpr int iGIPC_AIM_SETTLE_FRAMES = 120;

	// The swapchain consumes ONE pending dump per EndFrame, before present, so the
	// file is written a frame or two after RequestDump returns. Hold before aiming
	// somewhere else, or the capture would be of a camera already moving away.
	constexpr int iGIPC_SHOT_HOLD_FRAMES = 12;

	// The 9x9 patch the logged context sample averages, the same radius
	// ZM_AutoTests_ShellLighting and ZM_AutoTests_PlayerHomeTintPixels use, so the
	// three probes are talking about the same kind of measurement.
	constexpr u_int uGIPC_SAMPLE_RADIUS = 4u;

	// ★ THE ONE FREE FRAMING CHOICE IN THIS FILE -- see the header block. The
	// shipped follow camera trails its subject along -Z at yaw 0; swinging that
	// trail direction 30 degrees about +Y puts the lens off the approach axis so a
	// prop presents two faces and its top rather than one flat square. Everything
	// else about the pose is read out of ZM_FollowCamera.
	constexpr float fGIPC_VIEW_AZIMUTH_DEGREES = 30.0f;

	// How far the live camera pose may sit from the pose this file wrote before the
	// shot is declared untrustworthy. Not a tolerance budget -- nothing should move
	// it AT ALL while the scene is paused -- but a float round trip through
	// SetPosition/GetPosition, so it is tight. ZM_FollowCamera's spring would move
	// it by metres, not by millimetres.
	constexpr float fGIPC_POSE_DRIFT_EPSILON = 0.01f;

	// The safe viewport interior a subject must project into. The frame edge is
	// where the tonemapper's vignette and (in a tools build) the editor chrome
	// live, so a patch read there is not a reading off the subject.
	constexpr float fGIPC_NDC_SAFE_LIMIT = 0.95f;

	// ---- The three subjects ------------------------------------------------
	//
	// Same shape as axGIP_EXPECTED in Tests/ZM_AutoTests_GroundItemProp.cpp, and
	// deliberately so: that file's lookup is the one this harness reuses -- entity
	// name from ZM_Route1Placement.h, id checked against the placement row, live
	// component resolved through the scene every time. It could not be shared at
	// link time (it lives in that TU's anonymous namespace, exactly as the two
	// gallery tests' scaffolding does), so it is mirrored here rather than
	// re-invented; both sides read the SAME name constants and the SAME ids, which
	// is what stops the two drifting.
	//
	// ★ m_szShotStem IS THE BEFORE/AFTER JOIN KEY. Two runs' captures are matched by
	// FILENAME and by nothing else, so renaming one of these silently breaks the
	// comparison a human is relying on -- a reviewer would be holding two files that
	// look like a pair and are not. Treat a stem as frozen: a re-shoot of one half of
	// a comparison cannot rename it, and a NEW subject appends a row rather than
	// re-using an existing stem.
	struct GIPCSubject
	{
		const char*       m_szEntityName;
		ZM_GROUND_ITEM_ID m_eId;
		const char*       m_szShotStem;
	};

	const GIPCSubject axGIPC_SUBJECTS[] =
	{
		{ szZM_ROUTE1_PROP_SOUTH_SALVE_ENTITY_NAME,
		  ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE,   "route1_south_salve"   },
		{ szZM_ROUTE1_PROP_LANE_CATCHORB_ENTITY_NAME,
		  ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB, "route1_lane_catchorb" },
		{ szZM_ROUTE1_PROP_NORTH_SALVE_ENTITY_NAME,
		  ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE,   "route1_north_salve"   },
	};

	// The bound is DEDUCED, never spelled -- the house rule in
	// ZM_Route1Placement.h. Written [3] this would be a tautology and a dropped row
	// would zero-initialise into a NULL-named subject that every diagnostic prints.
	constexpr u_int uGIPC_SUBJECT_COUNT =
		(u_int)(sizeof(axGIPC_SUBJECTS) / sizeof(axGIPC_SUBJECTS[0]));

	static_assert(uGIPC_SUBJECT_COUNT == (u_int)ZM_GROUND_ITEM_COUNT,
		"the capture roster must photograph every registered ground item -- a prop "
		"the registry carries and this table does not is a prop whose look can change "
		"with no image on either side to sign it off against");

	struct GIPCShot
	{
		bool        m_bResolved  = false;
		bool        m_bProjected = false;
		bool        m_bRequested = false;
		std::string m_strPath;
		Zenith_Maths::Vector3 m_xPropCentre   = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xEye          = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector2 m_xNdc          = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector2 m_xViewportPos  = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector2 m_xViewportSize = Zenith_Maths::Vector2(0.0f);
	};

	enum class GIPCPhase
	{
		RequestWarp,
		AwaitWarp,
		Resolve,
		Aim,
		Settle,
		Shoot,
		Hold,
		Done
	};

	GIPCPhase g_eGIPCPhase       = GIPCPhase::Done;
	int       g_iGIPCPhaseFrames = 0;
	u_int     g_uGIPCSubject     = 0u;
	bool      g_bGIPCActive      = false;
	bool      g_bGIPCSkipped     = false;
	bool      g_bGIPCFailed      = false;
	bool      g_bGIPCScenePaused = false;

	const char* g_szGIPCFailure = "test did not reach verification";
	char        g_aszGIPCDetail[640] = {};

	Zenith_Scene    g_xGIPCScene;
	Zenith_EntityID g_xGIPCCameraID = INVALID_ENTITY_ID;
	GIPCShot        g_axGIPCShots[uGIPC_SUBJECT_COUNT];

	void FailGIPC(const char* szWhy)
	{
		if (!g_bGIPCFailed)
		{
			g_szGIPCFailure = szWhy;
		}
		g_bGIPCFailed = true;
		g_eGIPCPhase  = GIPCPhase::Done;
	}

	// ---- Prerequisites ------------------------------------------------------
	//
	// ★ THE TWO PREREQUISITE FAMILIES ARE DIFFERENT KINDS OF THING, and only one of
	// them may ever justify a skip -- the split ZM_AutoTests_CameraClearance spells
	// out. The Route 1 terrain bake is GITIGNORED, so a fresh clone genuinely has
	// no ground to photograph a prop standing on and skipping is honest. The Route 1
	// SCENE is COMMITTED (ZM-D-199), so its absence is a DEFECT: this file does not
	// even look for it, and lets the warp fail loudly instead.
	bool GIPCFilePresent(const std::string& strPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(strPath, xError) || xError)
		{
			return false;
		}
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
		return !xError && ulSize != 0u;
	}

	// The terrain SET NAME is read out of the compiled world row, never spelled --
	// the same rule ZM_QueueTerrainHostEntity follows when it builds the splatmap
	// path, so a re-pointed set moves the authoring and this guard together.
	//
	// THREE files, not the six ZM_AutoTests_CameraClearance lists. Every extra file
	// is another way to SKIP a run that could have captured, and a skip counts as a
	// pass -- so the guard is kept to the smallest set that cannot be present
	// without a real bake: the heightfield, the splatmap and the first render chunk.
	bool GIPCTerrainBakePresent()
	{
		const char* szSet = ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_szTerrainSet;
		if (szSet == nullptr || szSet[0] == '\0')
		{
			return false;
		}
		const std::string strDir =
			std::string(GAME_ASSETS_DIR) + "Terrain/" + szSet + "/";
		return GIPCFilePresent(strDir + "Height" ZENITH_TEXTURE_EXT)
			&& GIPCFilePresent(strDir + "Splatmap_RGBA" ZENITH_TEXTURE_EXT)
			&& GIPCFilePresent(strDir + "Render_0_0" ZENITH_GEOMETRY_EXT);
	}

	// Build/artifacts/zenithmon/visual_audit/grounditem_props, derived from
	// GAME_ASSETS_DIR (<repo>/Games/Zenithmon/Assets/ -> up three -> <repo>) so it
	// resolves whatever the process working directory is. Build/artifacts is
	// gitignored and is where every runner in this repo writes, which is what keeps
	// a capture out of Assets/ and out of git.
	std::filesystem::path GIPCCaptureDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "visual_audit"
			/ "grounditem_props";
	}

	// ---- Scene + entity resolution ------------------------------------------

	// The spawn tag Route 1's SOUTH ARRIVAL marker carries, resolved by WALKING the
	// compiled Dawnmere row rather than spelled.
	//
	// ★ THIS IS NOT ZM_GetRoute1SouthGateSpawnTag(). That one is what Route 1's
	// south GATE asks Dawnmere for on the way OUT; arriving ON Route 1 needs the tag
	// Route 1 itself offers, and passing the gate tag makes TryQueueWarp refuse the
	// destination silently. The same trap, and the same resolution, as
	// ZM_AutoTests_GroundItemProp's GIPInboundRoute1SpawnTag.
	const char* GIPCInboundRoute1SpawnTag()
	{
		const ZM_WorldSpec& xDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);
		for (u_int u = 0u; u < xDawnmere.m_uConnectionCount; ++u)
		{
			if (xDawnmere.m_pxConnections[u].m_eTarget == ZM_SCENE_ROUTE1)
			{
				return xDawnmere.m_pxConnections[u].m_szSpawnTag;
			}
		}
		return nullptr;
	}

	Zenith_Entity GIPCFindEntity(const char* szName)
	{
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		return pxData != nullptr ? pxData->FindEntityByName(szName) : Zenith_Entity();
	}

	// Resolved through the scene EVERY time rather than cached: ECS pools RELOCATE
	// their elements, so a component pointer held across a frame is a dangling
	// pointer waiting to happen.
	Zenith_CameraComponent* GIPCResolveCamera()
	{
		const Zenith_Entity xCamera =
			g_xEngine.Scenes().ResolveEntity(g_xGIPCCameraID);
		return xCamera.IsValid()
			? xCamera.TryGetComponent<Zenith_CameraComponent>()
			: nullptr;
	}

	// ---- The pose ------------------------------------------------------------

	// Where the lens goes for one prop. Every term is read from a shipped constant;
	// see the framing block in the file header for why each one is the right one.
	Zenith_Maths::Vector3 GIPCEyeForProp(const Zenith_Maths::Vector3& xPropCentre)
	{
		// The prop's own ground: its centre is authored at surface + half the cube
		// (ZM_Route1PropCentreY), so subtracting the half edge recovers the surface
		// WITHOUT reading the measured table -- which means a re-frozen ground table
		// moves this lens with the prop and no literal here has to be touched.
		const float fGroundY = xPropCentre.y - 0.5f * fZM_ROUTE1_PROP_CUBE_EDGE;

		// The shipped lens height over that ground: the player's body CENTRE stands
		// one half height up (ZM_HumanBody.h), and ZM_FollowCamera puts the camera
		// GetCameraHeight() above the transform it follows.
		const float fEyeY = fGroundY
			+ fZM_HUMAN_BODY_HALF_HEIGHT
			+ ZM_FollowCamera::GetCameraHeight();

		// The shipped horizontal stand-off: at yaw 0 ComputeDesiredPosition puts the
		// camera at exactly (0, +GetCameraHeight(), -GetArmLength()) from its
		// subject, so the arm IS the horizontal distance.
		const float fDistance = ZM_FollowCamera::GetArmLength();

		// ...swung off the approach axis so a cube shows more than one face. Rotating
		// the trail direction (0, -1) in XZ by the azimuth about +Y gives
		// (-sin a, -cos a), i.e. the lens sits south-west of the prop looking
		// north-east at it. std::sin/std::cos are safe here for the reason
		// ZM_Route1Placement.h states about its settled-camera accessor: this value
		// is a CHECK and a camera pose, and is never authored into a committed
		// .zscen, so a 1-2 ULP Debug/Release disagreement cannot move any file.
		const float fAzimuth = glm::radians(fGIPC_VIEW_AZIMUTH_DEGREES);
		return Zenith_Maths::Vector3(
			xPropCentre.x - fDistance * std::sin(fAzimuth),
			fEyeY,
			xPropCentre.z - fDistance * std::cos(fAzimuth));
	}

	// Aim FROM xEye AT xTarget in the engine's yaw/pitch convention (GetFacingDir:
	// pitch = asin(dir.y), yaw = atan2(-dir.x, dir.z)).
	bool GIPCPointCameraAt(Zenith_CameraComponent& xCamera,
		const Zenith_Maths::Vector3& xEye, const Zenith_Maths::Vector3& xTarget)
	{
		const Zenith_Maths::Vector3 xAim = xTarget - xEye;
		const float fLength = std::sqrt(
			xAim.x * xAim.x + xAim.y * xAim.y + xAim.z * xAim.z);
		if (!(fLength > 1.0e-3f) || !std::isfinite(fLength))
		{
			return false;
		}
		const Zenith_Maths::Vector3 xDir = xAim / fLength;

		xCamera.SetPosition(xEye);
		xCamera.SetPitch(std::asin(glm::clamp(xDir.y, -1.0f, 1.0f)));
		xCamera.SetYaw(std::atan2(-xDir.x, xDir.z));
		// The SHIPPED overworld field of view -- ZM_FollowCamera::CaptureAuthoredYaw
		// forces this on the authored camera on every start, so a capture taken at
		// any other FOV would not be the player's framing.
		xCamera.SetFOV(glm::radians(ZM_FollowCamera::GetFOVDegrees()));
		// The route's own clip range, not an interior's: Route 1 is 1,536 m deep and
		// a 100 m far plane would clip the world away a few strides ahead.
		xCamera.SetNearPlane(fZM_ROUTE1_CAMERA_NEAR);
		xCamera.SetFarPlane(fZM_ROUTE1_CAMERA_FAR);
		return true;
	}

	// Project a world point and reject anything behind the lens or outside the safe
	// viewport interior. Borrowed verbatim in shape from
	// ZM_AutoTests_PlayerHomeTintPixels::PTProjectPoint.
	bool GIPCProjectPoint(Zenith_CameraComponent& xCamera,
		const Zenith_Maths::Vector3& xWorld, Zenith_Maths::Vector2& xNdcOut)
	{
		Zenith_Maths::Matrix4 xView;
		Zenith_Maths::Matrix4 xProjection;
		xCamera.BuildViewMatrix(xView);
		xCamera.BuildProjectionMatrix(xProjection);

		const Zenith_Maths::Vector4 xClip =
			xProjection * xView * Zenith_Maths::Vector4(xWorld, 1.0f);
		if (!std::isfinite(xClip.x) || !std::isfinite(xClip.y)
			|| !std::isfinite(xClip.w) || xClip.w <= 1.0e-4f)
		{
			return false;
		}
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;
		if (fNdcX <= -fGIPC_NDC_SAFE_LIMIT || fNdcX >= fGIPC_NDC_SAFE_LIMIT
			|| fNdcY <= -fGIPC_NDC_SAFE_LIMIT || fNdcY >= fGIPC_NDC_SAFE_LIMIT)
		{
			return false;
		}
		xNdcOut = Zenith_Maths::Vector2(fNdcX, fNdcY);
		return true;
	}

	// The rectangle inside the dumped swapchain image that the 3D view occupies. In
	// a tools build that is the editor's docked viewport (the dump is the whole
	// swapchain, ImGui chrome included); in a runtime build it is the whole window.
	// Identical to the split ZM_AutoTests_PlayerHomeTintPixels makes, and the reason
	// the logged patch coordinates are meaningful to a human cropping the capture.
	bool GIPCResolveViewport(Zenith_CameraComponent& xCamera, GIPCShot& xShot)
	{
#ifdef ZENITH_TOOLS
		if (g_xEditorQuery.m_pfnGetViewportPos == nullptr
			|| g_xEditorQuery.m_pfnGetViewportSize == nullptr)
		{
			FailGIPC("the tools viewport query seam is not installed, so a captured "
				"patch cannot be located inside the swapchain dump");
			return false;
		}
		xShot.m_xViewportPos  = g_xEditorQuery.m_pfnGetViewportPos();
		xShot.m_xViewportSize = g_xEditorQuery.m_pfnGetViewportSize();
		if (xShot.m_xViewportSize.x < 320.0f || xShot.m_xViewportSize.y < 180.0f)
		{
			return false;   // the editor layout has not reached a sampleable size yet
		}
#else
		const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		if (xOpts.m_uWindowWidth == 0u || xOpts.m_uWindowHeight == 0u)
		{
			return false;
		}
		xShot.m_xViewportPos  = Zenith_Maths::Vector2(0.0f);
		xShot.m_xViewportSize = Zenith_Maths::Vector2(
			static_cast<float>(xOpts.m_uWindowWidth),
			static_cast<float>(xOpts.m_uWindowHeight));
#endif
		xCamera.SetAspectRatio(
			xShot.m_xViewportSize.x / xShot.m_xViewportSize.y);
		return true;
	}

	// ---- The logged-only context sample --------------------------------------

	bool GIPCReadMeanRGB(const Zenith_TestTGAImage& xImage,
		float fCenterX, float fCenterY, Zenith_Maths::Vector3& xOut)
	{
		if (!xImage.IsValid() || !std::isfinite(fCenterX) || !std::isfinite(fCenterY))
		{
			return false;
		}
		const int64_t iCenterX = static_cast<int64_t>(std::lround(fCenterX));
		const int64_t iCenterY = static_cast<int64_t>(std::lround(fCenterY));
		const int64_t iRadius  = static_cast<int64_t>(uGIPC_SAMPLE_RADIUS);
		if (iCenterX - iRadius < 0 || iCenterY - iRadius < 0
			|| iCenterX + iRadius >= static_cast<int64_t>(xImage.m_uWidth)
			|| iCenterY + iRadius >= static_cast<int64_t>(xImage.m_uHeight))
		{
			return false;
		}

		uint64_t ulRed = 0u;
		uint64_t ulGreen = 0u;
		uint64_t ulBlue = 0u;
		uint64_t ulSamples = 0u;
		for (int64_t iY = iCenterY - iRadius; iY <= iCenterY + iRadius; ++iY)
		{
			for (int64_t iX = iCenterX - iRadius; iX <= iCenterX + iRadius; ++iX)
			{
				const uint8_t* puBGRA = xImage.GetPixelBGRA(
					static_cast<uint32_t>(iX), static_cast<uint32_t>(iY));
				ulBlue  += puBGRA[0];
				ulGreen += puBGRA[1];
				ulRed   += puBGRA[2];
				++ulSamples;
			}
		}
		if (ulSamples == 0u)
		{
			return false;
		}
		const float fNormalise = 1.0f / (255.0f * static_cast<float>(ulSamples));
		xOut = Zenith_Maths::Vector3(
			static_cast<float>(ulRed)   * fNormalise,
			static_cast<float>(ulGreen) * fNormalise,
			static_cast<float>(ulBlue)  * fNormalise);
		return true;
	}

	// ---- The phases ----------------------------------------------------------

	bool GIPCStepRequestWarp()
	{
		// RETRIED, not asserted once. The FrontEnd manager becomes the authoritative
		// singleton some frames into the boot and a request made before then is
		// legitimately refused -- so a refusal is "not yet" until the deadline.
		const char* szInboundTag = GIPCInboundRoute1SpawnTag();
		if (szInboundTag == nullptr)
		{
			FailGIPC("the compiled Dawnmere row carries no connection targeting "
				"Route 1, so there is no inbound spawn tag to warp to and the props "
				"are unreachable");
			return false;
		}

		const u_int uRoute1 = ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_uBuildIndex;
		if (ZM_GameStateManager::RequestWarp(uRoute1, szInboundTag))
		{
			g_eGIPCPhase       = GIPCPhase::AwaitWarp;
			g_iGIPCPhaseFrames = 0;
			return true;
		}

		if (g_iGIPCPhaseFrames > iGIPC_WARP_DEADLINE_FRAMES)
		{
			FailGIPC("RequestWarp to Route 1 was still being refused after the "
				"bootstrap deadline, so the capture never reached the scene that "
				"holds the props");
			return false;
		}
		return true;   // not yet -- keep asking
	}

	bool GIPCStepAwaitWarp()
	{
		if (ZM_GameStateManager::IsWarpInProgress())
		{
			if (g_iGIPCPhaseFrames > iGIPC_WARP_DEADLINE_FRAMES)
			{
				FailGIPC("the warp to Route 1 never completed -- the machine is still "
					"mid-transition after the deadline (ZM-D-200: a warp to nowhere "
					"stalls rather than failing)");
				return false;
			}
			return true;
		}

		// IsWarpInProgress() only goes false at IDLE, i.e. AFTER the fade-in has
		// finished, so the screen is genuinely clear by the time we get here.
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxData == nullptr)
		{
			FailGIPC("the warp completed but the active scene has no data");
			return false;
		}
		const int iRoute1BuildIndex =
			static_cast<int>(ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_uBuildIndex);
		if (g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex
			!= iRoute1BuildIndex)
		{
			FailGIPC("the warp completed somewhere that is not Route 1, so whatever "
				"would have been captured is not a Route 1 prop");
			return false;
		}

		g_xGIPCScene       = xScene;
		g_eGIPCPhase       = GIPCPhase::Resolve;
		g_iGIPCPhaseFrames = 0;
		return true;
	}

	bool GIPCStepResolve()
	{
		// The authored camera. Named from ZM_Route1Placement.h rather than found by
		// component, so a renamed camera entity fails HERE with a name in the message
		// instead of silently handing the harness some other camera.
		Zenith_Entity xCamera =
			GIPCFindEntity(szZM_ROUTE1_CAMERA_ENTITY_NAME);
		Zenith_CameraComponent* pxCamera = xCamera.IsValid()
			? xCamera.TryGetComponent<Zenith_CameraComponent>()
			: nullptr;
		if (pxCamera == nullptr)
		{
			if (g_iGIPCPhaseFrames > iGIPC_RESOLVE_DEADLINE_FRAMES)
			{
				std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
					"the committed Route1.zscen has no entity '%s' carrying a "
					"Zenith_CameraComponent, so there is no authored lens to move -- "
					"and this harness may not create one (an entity-creating boot "
					"re-authors scene bytes)",
					szZM_ROUTE1_CAMERA_ENTITY_NAME);
				FailGIPC(g_aszGIPCDetail);
				return false;
			}
			return true;
		}
		g_xGIPCCameraID = xCamera.GetEntityID();

		// ★ ALL THREE, not just the first. A scene that authored one prop and dropped
		// two would otherwise produce one good picture and two silent nothings, and a
		// human comparing three BEFORE files with three AFTER files would never learn
		// which of them had never existed.
		for (u_int u = 0u; u < uGIPC_SUBJECT_COUNT; ++u)
		{
			const GIPCSubject& xSubject = axGIPC_SUBJECTS[u];

			Zenith_Entity xEntity = GIPCFindEntity(xSubject.m_szEntityName);
			if (!xEntity.IsValid())
			{
				if (g_iGIPCPhaseFrames > iGIPC_RESOLVE_DEADLINE_FRAMES)
				{
					std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
						"prop entity '%s' is not in the committed Route1.zscen, so "
						"there is nothing to photograph for ground item '%s'",
						xSubject.m_szEntityName, ZM_GroundItemName(xSubject.m_eId));
					FailGIPC(g_aszGIPCDetail);
					return false;
				}
				return true;
			}

			const ZM_GroundItemProp* pxProp =
				xEntity.TryGetComponent<ZM_GroundItemProp>();
			if (pxProp == nullptr)
			{
				std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
					"prop entity '%s' exists but carries NO ZM_GroundItemProp, so it "
					"is not the prop this capture claims to be photographing",
					xSubject.m_szEntityName);
				FailGIPC(g_aszGIPCDetail);
				return false;
			}
			if (pxProp->GetGroundItemId() != xSubject.m_eId)
			{
				std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
					"prop entity '%s' carries ground-item id %u ('%s') but this "
					"capture roster names id %u ('%s') -- the shot would be filed "
					"under the wrong prop's name",
					xSubject.m_szEntityName,
					(u_int)pxProp->GetGroundItemId(),
					ZM_GroundItemName(pxProp->GetGroundItemId()),
					(u_int)xSubject.m_eId, ZM_GroundItemName(xSubject.m_eId));
				FailGIPC(g_aszGIPCDetail);
				return false;
			}

			// NON-const, deliberately: Zenith_TransformComponent::GetPosition is not a
			// const member (its sibling GetScale is, which makes the asymmetry easy to
			// trip over). Nothing below writes through it.
			Zenith_TransformComponent* pxTransform =
				xEntity.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr)
			{
				std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
					"prop entity '%s' has no transform, so there is no point in space "
					"to aim at", xSubject.m_szEntityName);
				FailGIPC(g_aszGIPCDetail);
				return false;
			}

			// READ ONLY. The live authored centre is where the lens is aimed; nothing
			// in this file ever writes it back.
			Zenith_Maths::Vector3 xCentre(0.0f);
			pxTransform->GetPosition(xCentre);
			g_axGIPCShots[u].m_xPropCentre = xCentre;
			g_axGIPCShots[u].m_bResolved   = true;
		}

		// ★ THE FREEZE. From here the scene's ECS update dispatch is gated, so
		// ZM_FollowCamera::OnLateUpdate stops writing the camera and a pose set from
		// a test Step survives into the frame that renders it. See the mechanism
		// block in the file header; GIPCTakeShot asserts that it held.
		g_xEngine.Scenes().SetScenePaused(g_xGIPCScene, true);
		g_bGIPCScenePaused = true;

		g_uGIPCSubject     = 0u;
		g_eGIPCPhase       = GIPCPhase::Aim;
		g_iGIPCPhaseFrames = 0;
		return true;
	}

	bool GIPCStepAim()
	{
		Zenith_CameraComponent* pxCamera = GIPCResolveCamera();
		if (pxCamera == nullptr)
		{
			FailGIPC("the authored Route 1 camera disappeared mid-capture");
			return false;
		}

		GIPCShot& xShot = g_axGIPCShots[g_uGIPCSubject];
		xShot.m_xEye = GIPCEyeForProp(xShot.m_xPropCentre);
		if (!GIPCPointCameraAt(*pxCamera, xShot.m_xEye, xShot.m_xPropCentre))
		{
			std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
				"the derived lens for '%s' sits on top of the prop it is aiming at, "
				"so there is no view direction", ZM_GroundItemName(
					axGIPC_SUBJECTS[g_uGIPCSubject].m_eId));
			FailGIPC(g_aszGIPCDetail);
			return false;
		}

		g_eGIPCPhase       = GIPCPhase::Settle;
		g_iGIPCPhaseFrames = 0;
		return true;
	}

	bool GIPCTakeShot()
	{
		Zenith_CameraComponent* pxCamera = GIPCResolveCamera();
		if (pxCamera == nullptr)
		{
			FailGIPC("the authored Route 1 camera disappeared before a shot");
			return false;
		}
		GIPCShot& xShot = g_axGIPCShots[g_uGIPCSubject];

		// ★ THE FREEZE GUARD. Nothing may have moved this lens since GIPCStepAim
		// wrote it. If ZM_FollowCamera has resumed driving it, the capture would be
		// three pictures of the player's back with every other clause still green.
		Zenith_Maths::Vector3 xLive(0.0f);
		pxCamera->GetPosition(xLive);
		const Zenith_Maths::Vector3 xDrift = xLive - xShot.m_xEye;
		const float fDrift = std::sqrt(
			xDrift.x * xDrift.x + xDrift.y * xDrift.y + xDrift.z * xDrift.z);
		if (!(fDrift <= fGIPC_POSE_DRIFT_EPSILON))
		{
			std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
				"the lens aimed at '%s' has moved %.4f m from the pose this test "
				"wrote (%.3f, %.3f, %.3f) to (%.3f, %.3f, %.3f). Something is still "
				"driving the authored camera -- almost certainly ZM_FollowCamera, "
				"which means SetScenePaused stopped gating OnLateUpdate. The capture "
				"would be a picture of the player, not of the prop",
				ZM_GroundItemName(axGIPC_SUBJECTS[g_uGIPCSubject].m_eId),
				(double)fDrift,
				(double)xShot.m_xEye.x, (double)xShot.m_xEye.y,
				(double)xShot.m_xEye.z,
				(double)xLive.x, (double)xLive.y, (double)xLive.z);
			FailGIPC(g_aszGIPCDetail);
			return false;
		}

		if (!GIPCResolveViewport(*pxCamera, xShot))
		{
			return false;   // fails inside, or "not a sampleable size yet"
		}

		// IS THE SUBJECT ACTUALLY IN FRAME? A null A/B diff is usually a subject that
		// was never in the picture, and that is not something a human notices from a
		// filename -- so it is a hard failure here rather than a surprise later.
		xShot.m_bProjected =
			GIPCProjectPoint(*pxCamera, xShot.m_xPropCentre, xShot.m_xNdc);
		if (!xShot.m_bProjected)
		{
			std::snprintf(g_aszGIPCDetail, sizeof(g_aszGIPCDetail),
				"'%s' does not project into the safe viewport interior from the "
				"derived lens -- the framing is wrong for this prop and the capture "
				"would not contain it",
				ZM_GroundItemName(axGIPC_SUBJECTS[g_uGIPCSubject].m_eId));
			FailGIPC(g_aszGIPCDetail);
			return false;
		}

		// METHODOLOGY: log the state the engine is actually in, so a lever that
		// silently failed to take is visible rather than inferred.
		const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_GroundItemPropCapture] shooting '%s': prop (%.3f, %.3f, %.3f) "
			"lens (%.3f, %.3f, %.3f) fov %.1f deg, viewport %.0fx%.0f at "
			"(%.0f, %.0f), autoExposure=%d bloom=%d -> %s",
			ZM_GroundItemName(axGIPC_SUBJECTS[g_uGIPCSubject].m_eId),
			(double)xShot.m_xPropCentre.x, (double)xShot.m_xPropCentre.y,
			(double)xShot.m_xPropCentre.z,
			(double)xShot.m_xEye.x, (double)xShot.m_xEye.y, (double)xShot.m_xEye.z,
			(double)ZM_FollowCamera::GetFOVDegrees(),
			(double)xShot.m_xViewportSize.x, (double)xShot.m_xViewportSize.y,
			(double)xShot.m_xViewportPos.x, (double)xShot.m_xViewportPos.y,
			xOpts.m_bHDRAutoExposureEnabled ? 1 : 0,
			xOpts.m_bHDRBloomEnabled ? 1 : 0,
			xShot.m_strPath.c_str());

		Flux_Screenshot::RequestDump(xShot.m_strPath.c_str());
		xShot.m_bRequested = true;
		return true;
	}
}

static void Setup_ZMGroundItemPropCapture()
{
	g_eGIPCPhase       = GIPCPhase::Done;
	g_iGIPCPhaseFrames = 0;
	g_uGIPCSubject     = 0u;
	g_bGIPCActive      = false;
	g_bGIPCSkipped     = false;
	g_bGIPCFailed      = false;
	g_bGIPCScenePaused = false;
	g_szGIPCFailure    = "test did not reach verification";
	g_aszGIPCDetail[0] = '\0';
	g_xGIPCScene       = Zenith_Scene();
	g_xGIPCCameraID    = INVALID_ENTITY_ID;
	for (u_int u = 0u; u < uGIPC_SUBJECT_COUNT; ++u)
	{
		g_axGIPCShots[u] = GIPCShot();
	}

	Zenith_InputSimulator::ResetAllInputState();

	// The ONE skip, and deliberately narrow: "there is no baked ground for these
	// props to be standing on". RequestSkip bypasses Verify, so no fixed-dt, scene
	// or camera state may be installed before this point.
	if (!GIPCTerrainBakePresent())
	{
		g_bGIPCSkipped = true;
		Zenith_AutomatedTestRunner::RequestSkip(
			"[ZM_GroundItemPropCapture] the Route 1 terrain bake is absent, so a "
			"capture would show three props floating over nothing (run a *_True "
			"config once to bake it). NO BEFORE/AFTER IMAGES WERE WRITTEN.");
		return;
	}

	// The capture destination. Created up front -- the swapchain dump uses fopen
	// directly and does NOT create parent directories.
	const std::filesystem::path xCaptureDir = GIPCCaptureDir();
	std::error_code xDirError;
	std::filesystem::create_directories(xCaptureDir, xDirError);
	if (xDirError)
	{
		g_bGIPCActive = true;
		FailGIPC("could not create "
			"Build/artifacts/zenithmon/visual_audit/grounditem_props");
		return;
	}

	for (u_int u = 0u; u < uGIPC_SUBJECT_COUNT; ++u)
	{
		g_axGIPCShots[u].m_strPath =
			(xCaptureDir / (std::string(axGIPC_SUBJECTS[u].m_szShotStem) + ".tga"))
				.string();
		// ★ REMOVED, NOT OVERWRITTEN. A stale TGA that survived a failed capture
		// would be pasted into a work log as though it were this run's, and in a
		// BEFORE/AFTER comparison a stale file is worse than a missing one.
		std::remove(g_axGIPCShots[u].m_strPath.c_str());
	}

	Zenith_InputSimulator::SetFixedDt(fGIPC_FIXED_DT);
	// DELIBERATELY NO graphics-option changes: a visual sign-off judges the game
	// as shipped, auto-exposure and bloom included.

	g_eGIPCPhase  = GIPCPhase::RequestWarp;
	g_bGIPCActive = true;
}

static bool Step_ZMGroundItemPropCapture(int)
{
	if (!g_bGIPCActive || g_bGIPCFailed || g_eGIPCPhase == GIPCPhase::Done)
	{
		return false;
	}
	++g_iGIPCPhaseFrames;

	switch (g_eGIPCPhase)
	{
	case GIPCPhase::RequestWarp:
		return GIPCStepRequestWarp();

	case GIPCPhase::AwaitWarp:
		return GIPCStepAwaitWarp();

	case GIPCPhase::Resolve:
		return GIPCStepResolve();

	case GIPCPhase::Aim:
		return GIPCStepAim();

	case GIPCPhase::Settle:
		// The lens is already where it belongs; these frames buy terrain streaming,
		// TAA convergence and auto-exposure adaptation. See iGIPC_AIM_SETTLE_FRAMES.
		if (g_iGIPCPhaseFrames < iGIPC_AIM_SETTLE_FRAMES)
		{
			return true;
		}
		g_eGIPCPhase       = GIPCPhase::Shoot;
		g_iGIPCPhaseFrames = 0;
		return true;

	case GIPCPhase::Shoot:
		if (!GIPCTakeShot())
		{
			// Either a hard failure (already recorded) or a viewport that is not yet
			// sampleable. The latter retries until the settle budget is spent again.
			if (g_bGIPCFailed)
			{
				return false;
			}
			if (g_iGIPCPhaseFrames > iGIPC_AIM_SETTLE_FRAMES)
			{
				FailGIPC("the framebuffer capture could not obtain a valid viewport "
					"rectangle for a prop shot");
				return false;
			}
			return true;
		}
		g_eGIPCPhase       = GIPCPhase::Hold;
		g_iGIPCPhaseFrames = 0;
		return true;

	case GIPCPhase::Hold:
		if (g_iGIPCPhaseFrames < iGIPC_SHOT_HOLD_FRAMES)
		{
			return true;
		}
		++g_uGIPCSubject;
		if (g_uGIPCSubject >= uGIPC_SUBJECT_COUNT)
		{
			g_eGIPCPhase = GIPCPhase::Done;
			return false;
		}
		g_eGIPCPhase       = GIPCPhase::Aim;
		g_iGIPCPhaseFrames = 0;
		return true;

	case GIPCPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ZMGroundItemPropCapture()
{
	if (g_bGIPCSkipped)
	{
		return true;
	}

	bool bPassed = !g_bGIPCFailed;
	if (g_bGIPCFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_GroundItemPropCapture] %s", g_szGIPCFailure);
	}

	for (u_int u = 0u; u < uGIPC_SUBJECT_COUNT; ++u)
	{
		const GIPCSubject& xSubject = axGIPC_SUBJECTS[u];
		GIPCShot& xShot = g_axGIPCShots[u];
		const char* szProp = ZM_GroundItemName(xSubject.m_eId);

		if (!xShot.m_bResolved || !xShot.m_bProjected || !xShot.m_bRequested)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_GroundItemPropCapture] '%s' was never captured "
				"(resolved=%d projected=%d requested=%d) -- this run produced no image "
				"of it, so it is missing from whichever half of the comparison this is "
				"and nothing can sign its look off",
				szProp, (int)xShot.m_bResolved, (int)xShot.m_bProjected,
				(int)xShot.m_bRequested);
			bPassed = false;
			continue;
		}

		// The dump has no return code (Flux_Screenshot::RequestDump is void), so
		// this is the assertion that stands in for one: a readable file with the
		// subject inside it actually landed on disk.
		Zenith_TestTGAImage xImage;
		if (!Zenith_TestLoadTGA(xShot.m_strPath.c_str(), xImage))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_GroundItemPropCapture] '%s': no readable capture at %s -- the "
				"requested swapchain dump never landed, or landed in a format "
				"Zenith_TestLoadTGA does not accept",
				szProp, xShot.m_strPath.c_str());
			bPassed = false;
			continue;
		}

		const float fPatchX = xShot.m_xViewportPos.x
			+ (xShot.m_xNdc.x * 0.5f + 0.5f) * xShot.m_xViewportSize.x;
		const float fPatchY = xShot.m_xViewportPos.y
			+ (xShot.m_xNdc.y * 0.5f + 0.5f) * xShot.m_xViewportSize.y;
		Zenith_Maths::Vector3 xRGB(0.0f);
		if (!GIPCReadMeanRGB(xImage, fPatchX, fPatchY, xRGB))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_GroundItemPropCapture] '%s': the prop's projected patch "
				"(%.1f, %.1f) falls outside the %ux%u capture -- the viewport "
				"rectangle and the dumped image disagree, so the picture is not "
				"of what this test measured",
				szProp, (double)fPatchX, (double)fPatchY,
				xImage.m_uWidth, xImage.m_uHeight);
			bPassed = false;
			continue;
		}

		// ★ CONTEXT, NOT A THRESHOLD -- and nothing here asserts on it. An absolute
		// framebuffer ratio tracks the SCENE'S LIGHTING, not the prop (the retracted
		// premise recorded in ZM_AutoTests_PlayerHomeTintPixels.cpp). It is printed so
		// each half of a comparison has a number the other half can be quoted against,
		// and so a reviewer can tell "the prop changed" from "the sun moved".
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_GroundItemPropCapture] OBSERVED '%s' entity='%s' patch (%.1f, %.1f) "
			"RGB=(%.4f, %.4f, %.4f) red/blue=%.4f | capture %ux%u | %s",
			szProp, xSubject.m_szEntityName, (double)fPatchX, (double)fPatchY,
			(double)xRGB.x, (double)xRGB.y, (double)xRGB.z,
			(double)(xRGB.x / (xRGB.z > 1.0e-4f ? xRGB.z : 1.0e-4f)),
			xImage.m_uWidth, xImage.m_uHeight, xShot.m_strPath.c_str());
	}

	// ★ THE LINES A WORK LOG IS BUILT FROM. Printed on every run, pass or fail, so
	// the paths never have to be reconstructed from this file's constants -- and
	// emitted per subject rather than as one line with fixed indices, so adding a
	// fourth prop cannot leave a capture unmentioned.
	for (u_int u = 0u; u < uGIPC_SUBJECT_COUNT; ++u)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_GroundItemPropCapture] CAPTURE '%s' -> %s (copy it elsewhere before "
			"the NEXT run, which deletes it at Setup)",
			ZM_GroundItemName(axGIPC_SUBJECTS[u].m_eId),
			g_axGIPCShots[u].m_strPath.c_str());
	}

	return bPassed;
}

static void Teardown_ZMGroundItemPropCapture()
{
	// The harness destroys the world between tests, so the paused flag on the scene
	// data dies with it -- this only matters for a single-test run that leaves the
	// process alive, and costs nothing.
	if (g_bGIPCScenePaused && g_xGIPCScene.IsValid())
	{
		g_xEngine.Scenes().SetScenePaused(g_xGIPCScene, false);
	}
	g_bGIPCScenePaused = false;
	Zenith_InputSimulator::ClearFixedDt();
	Zenith_InputSimulator::ResetAllInputState();
	g_xGIPCCameraID = INVALID_ENTITY_ID;
	g_bGIPCActive   = false;
}

static const Zenith_AutomatedTest g_xZMGroundItemPropCaptureTest = {
	"ZM_GroundItemPropCapture_Test",
	&Setup_ZMGroundItemPropCapture,
	&Step_ZMGroundItemPropCapture,
	&Verify_ZMGroundItemPropCapture,
	// Two 900-frame warp budgets, a 300-frame resolve budget, and three shots at
	// (120 settle + 120 shoot-retry + 12 hold). Every one of those owns a deadline
	// that FAILS with a diagnostic; this cap is only a backstop above their sum.
	/* maxFrames */ 3000,
	true /* m_bRequiresGraphics */,
	false /* m_bManualOnly */,
	&Teardown_ZMGroundItemPropCapture,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMGroundItemPropCaptureTest);

#endif // ZENITH_INPUT_SIMULATOR
