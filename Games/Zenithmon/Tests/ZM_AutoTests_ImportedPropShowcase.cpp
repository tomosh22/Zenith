#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_ImportedPropShowcase -- the pictures every IMPORTED prop is signed
// off from, and the one pair that shows PlayerHome is visible at all.
//
// Zenithmon's hand-made assets (`Assets/Props/<Name>/<Name>.glb`, via
// Zenith_Tools_GlbImport) replace the generated props of the same name. Whether
// one LOOKS right is a judgement no assertion can make, so this harness puts
// reproducible frames on disk for a human, and fails loudly when a frame could
// not have shown what it claims to.
//
// ★★ IT IS A ROSTER, NOT A PROP. This file began as ZM_BedShowcase_Test, keyed on
// one entity with one prop's framing hard-coded through it. The SECOND imported
// asset (the table) made the shape wrong: the two live in the same room, so a
// second test would have paid a second scene load and a second 600-frame room
// budget to photograph a prop already standing in the first one's frame. Adding a
// row to axIPS_SUBJECTS below is now the whole cost of a new asset, and
// ArtBrief.md section 1.1 still has unticked furniture rows. (It said "five to
// go" and was stale by two imports within the day -- a count of somebody else's
// checkboxes is a duplicate of a number this file cannot see change.)
//
// ★ EACH SUBJECT CARRIES ITS OWN AZIMUTHS, and that is not a style knob. The
// framing is a spherical offset from the prop, so which way the lens swings has
// to point at OPEN FLOOR -- and that depends on where in the room the prop
// stands. The bed is in the -X/-Z corner and the table is against +X; a single
// shared azimuth aims one of them into a wall. The eye is additionally CLAMPED
// into the room (see IPSAimAtSubject), which is what actually guarantees it.
//
// ---- THE SHOTS ------------------------------------------------------------
//
//   player_view   the LIVE ZM_FollowCamera, scene running, nothing overridden.
//                 "Can you see the room when you are standing in it?"
//   player_view_unclamped
//                 the SAME pivot from the pose the boom would have taken with no
//                 ceiling clamp -- the lens ~3.9 m up in a room whose ceiling
//                 slab starts at 3.0 m. Nothing is asserted about it; it is the
//                 "before" half of the pair, taken in the SAME run so the two
//                 cannot drift apart the way two runs a week apart do. See
//                 ZM_FollowCamera::ClampBoomBelowCeiling.
//   room_wide     from the doorway, looking in. The imported props IN CONTEXT --
//                 is each the right size against the room, the door and the
//                 generated furniture beside it? NOT every prop: a subject hard
//                 against a side wall falls outside a 65-degree lens, and the
//                 shelf does. The numbers, and why no pose fixes it, are at
//                 fIPS_WIDE_EYE_Z.
//   <name>_three_quarter   off-axis, at walking distance. Silhouette and
//                 proportion.
//   <name>_detail close and raking. The albedo, normal and roughness maps are
//                 2048^2 each and are the entire reason the import exists; at
//                 this distance they either read as material or they do not.
//
// ---- THE MECHANISM (borrowed wholesale from ZM_AutoTests_GroundItemPropCapture)
//
// ★★ A TEST Step CANNOT OUT-WRITE ZM_FollowCamera. Zenith_MainLoop runs
// PumpAutomatedTest BEFORE UpdateGameLogic, and ZM_FollowCamera::OnLateUpdate
// writes SetPosition/SetYaw/SetPitch from inside that later block -- so a pose
// written here is overwritten in the same frame, before anything renders. Every
// aimed shot therefore PAUSES the scene first (Zenith_SceneSystem::SetScenePaused
// gates only the ECS update dispatch; the render snapshot walks the component
// query and is pause-independent), and the pose is re-read immediately before
// each dump and asserted not to have drifted.
//
// ★ WHICH IS ALSO WHY player_view IS TAKEN FIRST, UNPAUSED. It is the only shot
// whose subject IS the follow camera's own behaviour, so it has to be taken
// before anything is frozen. Freeze first and it would photograph a pose this
// file chose, which is precisely the thing it is trying to check.
//
// ★ 120 SETTLE FRAMES PER SHOT. Auto-exposure adaptation, measured by
// ZM_AutoTests_ShellLighting as ~2 s at fixed 60 Hz and inherited here rather
// than re-derived. A shorter settle yields a dark, half-adapted frame that reads
// as a defect in the ASSET.
//
// ★ NO GRAPHICS OPTION IS TOUCHED -- auto-exposure, bloom, TAA, the skybox and
// the UI all run as shipped, because the question is what a player sees.
//
// ---- WHAT IS ASSERTED, AND WHAT IS ONLY LOOKED AT --------------------------
//
// ASSERTED: the room loads; every subject entity exists; its model actually
// LOADED (a bake-less clone would otherwise photograph an empty floor and pass);
// its authored transform matches what ZM_ComputePropFit says it should be, so a
// picture is never filed against a scene authored before the fit existed; the
// live follow camera sits BELOW the room's ceiling; every shot landed on disk and
// re-loads as a valid TGA; and in each aimed shot the subject's centre projects
// inside the safe viewport interior -- a null A/B diff is nearly always a subject
// that was not in frame, which is not something a human notices from a filename.
//
// NOT ASSERTED: anything about the pixels themselves. An absolute framebuffer
// ratio tracks the scene's lighting rather than the subject (the retracted
// premise recorded at length in ZM_AutoTests_PlayerHomeTintPixels.cpp), so
// pinning one here would red this test every time the interior lights are
// re-tuned. The mean RGB at each subject's projected centre is LOGGED as context.
//
// ★ m_bRequiresGraphics = true: it reads and writes pixels by definition, so a
// Null build skips it. The two properties that MUST NOT rot headlessly -- the
// ceiling clamp and the prop fit -- are pinned by plain units in
// Tests/ZM_Tests_FollowCameraCeiling.cpp and Tests/ZM_Tests_PropFit.cpp, which
// run in every configuration.
//
// ★ THE SHOTS ARE DELETED AT Setup, so a stale frame from a failed run can never
// be pasted into a work log as though it were fresh. EVERY RUN DESTROYS THE
// PREVIOUS RUN'S CAPTURES: copy the first half of a before/after comparison out
// of the directory before running the second.
// ============================================================================

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/Zenith_TestTGA.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_FrustumCulling.h"          // Zenith_AABB -- the mesh's local bounds
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"
#include "Zenithmon/Source/World/ZM_InteriorDressing.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_PropFit.h"

namespace
{
	constexpr float fIPS_FIXED_DT = 1.0f / 60.0f;

	// Auto-exposure adaptation, per ZM_AutoTests_ShellLighting's measurement.
	constexpr int iIPS_SETTLE_FRAMES = 120;

	// The swapchain consumes one pending dump per EndFrame, before present, so
	// the file lands a frame or two after RequestDump returns.
	constexpr int iIPS_HOLD_FRAMES = 12;

	constexpr int iIPS_ROOM_DEADLINE_FRAMES = 600;

	// The safe viewport interior a subject must project into. The frame edge is
	// where the tonemapper's vignette and the editor chrome live.
	constexpr float fIPS_NDC_SAFE_LIMIT = 0.90f;

	constexpr u_int uIPS_SAMPLE_RADIUS = 4u;

	// How far inside the room's own surfaces an aimed lens is kept. Enough that
	// the 0.05 m near plane never intersects a wall, floor or ceiling slab.
	constexpr float fIPS_ROOM_MARGIN = 0.40f;

	// Nothing may move a paused camera; this is a float round trip through
	// SetPosition/GetPosition, not a tolerance budget.
	constexpr float fIPS_POSE_DRIFT_EPSILON = 0.01f;

	// The authored transform must agree with ZM_ComputePropFit to within a float
	// round trip through the scene file, which stores plain 32-bit values.
	constexpr float fIPS_FIT_EPSILON = 1.0e-4f;

	// ---- The roster --------------------------------------------------------
	//
	// One row per IMPORTED prop standing in PlayerHome. Adding an asset means
	// adding a row; nothing else in this file is per-prop.
	//
	// The two poses are spherical offsets from the prop, in multiples of its own
	// LONGEST SCALED AXIS -- so re-exporting an asset at a different scale moves
	// the camera with it instead of leaving two photographs of a wall. Azimuth 0
	// looks along +Z at the subject and increases counter-clockwise about +Y.
	struct IPSPose
	{
		float m_fAzimuthDegrees;
		float m_fElevationDegrees;
		float m_fDistanceScale;
	};

	struct IPSSubject
	{
		const char* m_szKey;          // filename stem for its two shots
		const char* m_szEntityName;   // the authored entity, from ZM_InteriorDressing.h
		ZM_PROP_ID  m_eProp;
		IPSPose     m_xThreeQuarter;
		IPSPose     m_xDetail;
	};

	constexpr IPSSubject axIPS_SUBJECTS[] =
	{
		// The bed stands in the -X/-Z corner, so the lens swings toward +X/+Z.
		{ "bed", "HomeBed", ZM_PROP_BED,
			/* three-quarter */ {  38.0f, 22.0f, 1.55f },
			/* detail        */ { 118.0f, 20.0f, 0.95f } },

		// The table stands against +X, so its azimuths are NEGATIVE -- the mirror
		// of the bed's. A shared value would aim this one straight into the +X
		// wall, which the room clamp would then slide along it.
		{ "table", "HomeTable", ZM_PROP_TABLE,
			/* three-quarter */ { -50.0f, 24.0f, 1.55f },
			/* detail        */ { -115.0f, 26.0f, 0.95f } },

		// The chair shares the table's +X side, so its azimuths are negative too.
		// It is the SMALLEST subject, and its distances are multiples of its own
		// longest axis -- which is the whole reason the roster stores a scale
		// rather than a distance in metres: the same 1.55/0.95 that frame a 2 m
		// bed frame a 1 m chair.
		{ "chair", "HomeChair", ZM_PROP_CHAIR,
			/* three-quarter */ {  -60.0f, 20.0f, 1.60f },
			/* detail        */ { -125.0f, 22.0f, 0.85f } },

		// The shelf stands against the -X wall like the bed, so its azimuths are
		// POSITIVE -- the mirror of the table's and the chair's. It is the first
		// subject whose facing was MEASURED off the mesh rather than assumed: the
		// model's -X face is a continuous back panel and YAW0 leaves it against the
		// wall, so a lens at +X of it looks into the open bays instead of at a sheet
		// of plywood. Aim these NEGATIVE and both shots photograph the back.
		//
		// ★ ITS ELEVATIONS ARE THE LOWEST IN THIS TABLE, and that is the subject's
		// HEIGHT rather than a style choice. Every other row is waist-high furniture
		// that reads from above; the shelf is 2 m tall, and the same 20-26 degrees
		// would look down onto its top board and foreshorten the four shelves that
		// are the entire reason it was commissioned. The distances need no such
		// adjustment -- they are multiples of the subject's own longest axis, so the
		// 1.55/0.95 that frame a 1 m chair frame a 2 m shelf at 3.1 m and 1.9 m.
		{ "shelf", "HomeShelf", ZM_PROP_SHELF,
			/* three-quarter */ {   50.0f, 14.0f, 1.55f },
			/* detail        */ {  115.0f,  8.0f, 0.95f } },
	};
	constexpr u_int uIPS_SUBJECT_COUNT =
		(u_int)(sizeof(axIPS_SUBJECTS) / sizeof(axIPS_SUBJECTS[0]));

	// ---- The room-wide shot, which belongs to no single subject -------------
	//
	// A FIXED pose rather than an offset from a prop: its job is to show the
	// imported props at once against the room, and "3 m from the bed" frames the
	// bed. From just inside the doorway, looking down the room's axis.
	//
	// ★★ IT DOES NOT HOLD ALL OF THEM, AND IT CANNOT. This comment claimed the
	// "65-degree lens covers both long walls", which was true only while every
	// subject sat within about |x| <= 5.6. MEASURED from this pose: the bed is 33.4
	// degrees off the view axis, the table 36.4 and the chair 42.7, against a
	// horizontal half-frame of 48.6 degrees (65 vertical at 16:9) -- but the shelf,
	// hard against the -X wall at x = -6.90 and only 4 m deep into the shot, sits at
	// 60.0 degrees and is a clear 11 degrees outside the frame.
	//
	// ★ NO AXIAL POSE FIXES THAT, so nothing here was retuned to chase it. The room
	// is 15.5 m wide and 11.5 m deep; seeing x = -6.9 at z = +0.9 from anywhere on
	// the centre line needs a ~114-degree horizontal lens, and moving the eye toward
	// either wall throws the opposite wall's props out instead. Widening the FOV is
	// refused on this file's own terms (NO GRAPHICS OPTION IS TOUCHED, above): the
	// question this shot asks is what a PLAYER sees, and a player standing in the
	// doorway genuinely cannot see the shelf either.
	//
	// So this is the CONTEXT shot, not the coverage guarantee. Coverage is the two
	// aimed shots each subject owns, and those are the ones that assert a subject
	// actually projects into frame -- this one asserts nothing about its contents.
	constexpr float fIPS_WIDE_EYE_Z = 4.60f;
	constexpr float fIPS_WIDE_LOOK_Z = -2.00f;
	constexpr float fIPS_WIDE_LOOK_Y = 0.80f;

	// player_view, player_view_unclamped, room_wide, then two per subject.
	constexpr u_int uIPS_FIXED_SHOT_COUNT = 3u;
	constexpr u_int uIPS_SHOT_COUNT =
		uIPS_FIXED_SHOT_COUNT + uIPS_SUBJECT_COUNT * 2u;

	constexpr u_int uIPS_SHOT_PLAYER_VIEW = 0u;
	constexpr u_int uIPS_SHOT_UNCLAMPED = 1u;
	constexpr u_int uIPS_SHOT_ROOM_WIDE = 2u;

	struct IPSShotRecord
	{
		std::string m_strPath;
		Zenith_Maths::Vector3 m_xEye = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector2 m_xViewportPos = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector2 m_xViewportSize = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector2 m_xSubjectNdc = Zenith_Maths::Vector2(0.0f);
		bool m_bRequested = false;
		bool m_bSubjectInFrame = false;
		bool m_bAimed = false;
	};

	// Everything measured off one subject's live components.
	struct IPSSubjectState
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xWorldCentre = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xWorldSize = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xModelSize = Zenith_Maths::Vector3(0.0f);
		// The AUTHORED yaw, and the world-space footprint it actually produces.
		// See the measurement note in IPSMeasureSubject for why the rotation is
		// carried rather than assumed away.
		Zenith_Maths::Quat m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		float m_fAuthoredYawDegrees = 0.0f;
		Zenith_Maths::Vector3 m_xFacing = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xWorldFootprint = Zenith_Maths::Vector3(0.0f);
		float m_fAuthoredScale = 0.0f;
		float m_fAuthoredGroundY = 0.0f;
		bool m_bModelLoaded = false;
		bool m_bFitMatches = false;
	};

	enum class IPSPhase
	{
		AwaitRoom,
		LiveSettle,        // the follow camera runs; nothing is overridden
		LiveHold,
		UnclampedSettle,   // the pose the boom WOULD have taken, for comparison
		UnclampedHold,
		AimedSettle,
		AimedHold,
		Done,
	};

	IPSPhase g_eIPSPhase = IPSPhase::AwaitRoom;
	int  g_iIPSPhaseFrames = 0;
	// Walks [0, uIPS_SHOT_COUNT - uIPS_SHOT_ROOM_WIDE): 0 is room_wide, then two
	// per subject in roster order.
	u_int g_uIPSAimedIndex = 0u;
	bool g_bIPSActive = false;
	bool g_bIPSFailed = false;
	bool g_bIPSScenePaused = false;
	char g_aszIPSDetail[512] = {};
	const char* g_szIPSFailure = "test did not reach verification";

	Zenith_Scene g_xIPSScene;
	Zenith_EntityID g_xIPSCameraID = INVALID_ENTITY_ID;

	IPSSubjectState g_axIPSSubjects[uIPS_SUBJECT_COUNT];
	IPSShotRecord g_axIPSShots[uIPS_SHOT_COUNT];

	float g_fIPSResolvedCeiling = 0.0f;
	float g_fIPSLiveCameraY = 0.0f;
	Zenith_Maths::Vector3 g_xIPSPivot = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 g_xIPSUnclampedEye = Zenith_Maths::Vector3(0.0f);

	void FailIPS(const char* szReason)
	{
		if (!g_bIPSFailed)
		{
			g_szIPSFailure = szReason;
		}
		g_bIPSFailed = true;
		g_eIPSPhase = IPSPhase::Done;
	}

	// The same artifacts location ZM_AutoTests_PlayerHomeTintPixels writes to, so
	// a reviewer has one directory to look in.
	std::filesystem::path IPSCaptureDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "visual_audit";
	}

	Zenith_Entity IPSFindEntity(const char* szName)
	{
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		return (pxData != nullptr) ? pxData->FindEntityByName(szName) : Zenith_Entity();
	}

	Zenith_CameraComponent* IPSResolveCamera()
	{
		const Zenith_Entity xCamera = g_xEngine.Scenes().ResolveEntity(g_xIPSCameraID);
		return xCamera.IsValid()
			? xCamera.TryGetComponent<Zenith_CameraComponent>()
			: nullptr;
	}

	// Which subject an aimed-shot index belongs to, and whether it is the detail
	// pose. Index 0 is room_wide, which belongs to none.
	bool IPSAimedIndexToSubject(u_int uAimed, u_int& uSubjectOut, bool& bDetailOut)
	{
		if (uAimed == 0u)
		{
			return false;   // room_wide
		}
		const u_int uOffset = uAimed - 1u;
		uSubjectOut = uOffset / 2u;
		bDetailOut = (uOffset % 2u) != 0u;
		return uSubjectOut < uIPS_SUBJECT_COUNT;
	}

	// The rectangle inside the dumped swapchain image that the 3D view occupies.
	// In a tools build that is the editor's docked viewport; in a runtime build
	// the whole window.
	bool IPSResolveViewport(Zenith_CameraComponent& xCamera, IPSShotRecord& xShot)
	{
#ifdef ZENITH_TOOLS
		if (g_xEditorQuery.m_pfnGetViewportPos == nullptr
			|| g_xEditorQuery.m_pfnGetViewportSize == nullptr)
		{
			FailIPS("the tools viewport query seam is not installed, so a captured "
				"patch cannot be located inside the swapchain dump");
			return false;
		}
		xShot.m_xViewportPos = g_xEditorQuery.m_pfnGetViewportPos();
		xShot.m_xViewportSize = g_xEditorQuery.m_pfnGetViewportSize();
		if (xShot.m_xViewportSize.x < 320.0f || xShot.m_xViewportSize.y < 180.0f)
		{
			return false;   // the editor layout has not reached a sampleable size
		}
#else
		const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		if (xOpts.m_uWindowWidth == 0u || xOpts.m_uWindowHeight == 0u)
		{
			return false;
		}
		xShot.m_xViewportPos = Zenith_Maths::Vector2(0.0f);
		xShot.m_xViewportSize = Zenith_Maths::Vector2(
			static_cast<float>(xOpts.m_uWindowWidth),
			static_cast<float>(xOpts.m_uWindowHeight));
#endif
		xCamera.SetAspectRatio(xShot.m_xViewportSize.x / xShot.m_xViewportSize.y);
		return true;
	}

	bool IPSProjectPoint(Zenith_CameraComponent& xCamera,
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
		xNdcOut = Zenith_Maths::Vector2(xClip.x / xClip.w, xClip.y / xClip.w);
		return true;
	}

	bool IPSNdcInSafeInterior(const Zenith_Maths::Vector2& xNdc)
	{
		return xNdc.x > -fIPS_NDC_SAFE_LIMIT && xNdc.x < fIPS_NDC_SAFE_LIMIT
			&& xNdc.y > -fIPS_NDC_SAFE_LIMIT && xNdc.y < fIPS_NDC_SAFE_LIMIT;
	}

	// Park the lens at xEye looking at xLookAt, with the shipped field of view.
	void IPSPointCameraAt(Zenith_CameraComponent& xCamera,
		const Zenith_Maths::Vector3& xEye, const Zenith_Maths::Vector3& xLookAt)
	{
		const Zenith_Maths::Vector3 xAim = xLookAt - xEye;
		const float fLength = glm::length(xAim);
		if (!(fLength > 1.0e-3f))
		{
			return;
		}
		const Zenith_Maths::Vector3 xDir = xAim / fLength;
		xCamera.SetPosition(xEye);
		// GetFacingDir convention: pitch = asin(dir.y), yaw = atan2(-dir.x, dir.z).
		xCamera.SetPitch(std::asin(glm::clamp(xDir.y, -1.0f, 1.0f)));
		xCamera.SetYaw(std::atan2(-xDir.x, xDir.z));
		xCamera.SetFOV(glm::radians(ZM_FollowCamera::GetFOVDegrees()));
		xCamera.SetNearPlane(0.05f);
		xCamera.SetFarPlane(100.0f);
	}

	// Clamp a lens into the room's interior. See the header: a camera outside the
	// wall still PROJECTS the subject into frame, because projection knows nothing
	// about occlusion, so the shot passes every check and shows a blank wall.
	Zenith_Maths::Vector3 IPSClampIntoRoom(const Zenith_Maths::Vector3& xEye)
	{
		const ZM_InteriorRoomSpec xRoom =
			ZM_GetInteriorRoomSpec(ZM_INTERIOR_ROOM_PLAYER_HOME);
		return Zenith_Maths::Vector3(
			glm::clamp(xEye.x, -(xRoom.InnerHalfWidth() - fIPS_ROOM_MARGIN),
				xRoom.InnerHalfWidth() - fIPS_ROOM_MARGIN),
			glm::clamp(xEye.y, fIPS_ROOM_MARGIN, xRoom.m_fWallHeight - fIPS_ROOM_MARGIN),
			glm::clamp(xEye.z, -(xRoom.InnerHalfDepth() - fIPS_ROOM_MARGIN),
				xRoom.InnerHalfDepth() - fIPS_ROOM_MARGIN));
	}

	// Put the lens on a subject's spherical offset and look at it.
	bool IPSAimAtSubject(Zenith_CameraComponent& xCamera, u_int uSubject, bool bDetail)
	{
		if (uSubject >= uIPS_SUBJECT_COUNT)
		{
			return false;
		}
		const IPSSubjectState& xState = g_axIPSSubjects[uSubject];
		const IPSPose& xPose = bDetail
			? axIPS_SUBJECTS[uSubject].m_xDetail
			: axIPS_SUBJECTS[uSubject].m_xThreeQuarter;

		float fLongest = xState.m_xWorldSize.x;
		fLongest = (xState.m_xWorldSize.y > fLongest) ? xState.m_xWorldSize.y : fLongest;
		fLongest = (xState.m_xWorldSize.z > fLongest) ? xState.m_xWorldSize.z : fLongest;
		if (!(fLongest > 1.0e-3f))
		{
			return false;
		}

		const float fAzimuth = glm::radians(xPose.m_fAzimuthDegrees);
		const float fElevation = glm::radians(xPose.m_fElevationDegrees);
		const float fRadius = fLongest * xPose.m_fDistanceScale;

		const Zenith_Maths::Vector3 xOffset(
			std::sin(fAzimuth) * std::cos(fElevation) * fRadius,
			std::sin(fElevation) * fRadius,
			std::cos(fAzimuth) * std::cos(fElevation) * fRadius);

		const Zenith_Maths::Vector3 xEye =
			IPSClampIntoRoom(xState.m_xWorldCentre + xOffset);
		if (!(glm::length(xState.m_xWorldCentre - xEye) > 1.0e-3f))
		{
			return false;
		}
		IPSPointCameraAt(xCamera, xEye, xState.m_xWorldCentre);
		return true;
	}

	void IPSAimRoomWide(Zenith_CameraComponent& xCamera)
	{
		const ZM_InteriorRoomSpec xRoom =
			ZM_GetInteriorRoomSpec(ZM_INTERIOR_ROOM_PLAYER_HOME);
		const Zenith_Maths::Vector3 xEye = IPSClampIntoRoom(Zenith_Maths::Vector3(
			0.0f, xRoom.m_fWallHeight - fIPS_ROOM_MARGIN, fIPS_WIDE_EYE_Z));
		IPSPointCameraAt(xCamera, xEye,
			Zenith_Maths::Vector3(0.0f, fIPS_WIDE_LOOK_Y, fIPS_WIDE_LOOK_Z));
	}

	// Measure one subject off its own live components, and check the authored
	// transform against the fit re-derived from the mesh the scene is showing.
	//
	// ★ READ OFF THE LIVE COMPONENTS, not off ZM_InteriorFurniture. That component
	// is file-local to Zenithmon.cpp and exposing it just for this would put a
	// seam in the game to serve a test. Everything wanted here is already on the
	// entity: the ModelComponent says whether a model resolved, its mesh instance
	// carries the local bounds, and the transform carries what the authoring
	// chose. Re-deriving the fit from the SAME mesh the scene is showing is what
	// makes the comparison meaningful.
	void IPSMeasureSubject(u_int uSubject)
	{
		if (uSubject >= uIPS_SUBJECT_COUNT)
		{
			return;
		}
		IPSSubjectState& xState = g_axIPSSubjects[uSubject];
		const IPSSubject& xRow = axIPS_SUBJECTS[uSubject];

		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xState.m_xEntityID);
		if (!xEntity.IsValid())
		{
			return;
		}
		Zenith_ModelComponent* pxModel = xEntity.TryGetComponent<Zenith_ModelComponent>();
		Zenith_TransformComponent* pxTransform =
			xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxModel == nullptr || pxTransform == nullptr || pxModel->GetNumMeshes() == 0u)
		{
			return;
		}
		const Flux_MeshInstance* pxInstance = pxModel->GetMeshInstance(0u);
		if (pxInstance == nullptr)
		{
			return;
		}
		const Zenith_AABB& xLocal = pxInstance->GetLocalBounds();

		Zenith_Maths::Vector3 xPosition(0.0f);
		Zenith_Maths::Vector3 xScale(1.0f);
		Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);
		pxTransform->GetPosition(xPosition);
		pxTransform->GetScale(xScale);
		pxTransform->GetRotation(xRotation);

		// ★★ THE ROTATION IS MEASURED, NOT ASSUMED AWAY. The aiming below genuinely
		// does not need it, and an earlier version of this file said so and stopped
		// there -- which meant the harness could photograph a prop facing the wrong
		// way and report nothing but a correct SIZE. A chair has a front; a bed has
		// a head. So the authored yaw is recovered from the quaternion and the
		// world FOOTPRINT is computed through it, both logged.
		//
		// ★ AND THE YAW IS RECOVERED FROM THE QUATERNION RATHER THAN READ OFF THE
		// CONSTANT THAT PRODUCED IT, which is the only reason the naming defect in
		// ZM_InteriorDressing.h's fZM_INTERIOR_YAW* block is visible here at all:
		// those constants are (cos(a), sin(a)) where a quaternion wants
		// (cos(a/2), sin(a/2)), so every one of them names HALF the angle it
		// actually applies.
		xState.m_xRotation = xRotation;
		xState.m_fAuthoredYawDegrees = glm::degrees(
			2.0f * std::atan2(xRotation.y, xRotation.w));
		xState.m_xFacing = xRotation * Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);

		xState.m_bModelLoaded = true;
		xState.m_fAuthoredScale = xScale.x;
		xState.m_fAuthoredGroundY = xPosition.y;
		xState.m_xModelSize = xLocal.m_xMax - xLocal.m_xMin;
		// Local bounds scaled and offset by the authored transform. Rotation is
		// deliberately ignored: the aiming only needs a point and a radius, and
		// every interior prop's authored yaw is a multiple of a quarter turn about
		// Y, which leaves the centre put.
		xState.m_xWorldCentre =
			xPosition + (xLocal.m_xMin + xLocal.m_xMax) * 0.5f * xScale.x;
		xState.m_xWorldSize = xState.m_xModelSize * xScale.x;
		// The axis-aligned extent the prop actually occupies once its authored
		// rotation is applied -- which is what the room, the corridor clause and
		// the furniture beside it all care about.
		{
			const Zenith_Maths::Vector3 xHalf = xState.m_xWorldSize * 0.5f;
			const Zenith_Maths::Matrix3 xBasis = glm::mat3_cast(xRotation);
			xState.m_xWorldFootprint = Zenith_Maths::Vector3(
				std::fabs(xBasis[0].x) * xHalf.x + std::fabs(xBasis[1].x) * xHalf.y
					+ std::fabs(xBasis[2].x) * xHalf.z,
				std::fabs(xBasis[0].y) * xHalf.x + std::fabs(xBasis[1].y) * xHalf.y
					+ std::fabs(xBasis[2].y) * xHalf.z,
				std::fabs(xBasis[0].z) * xHalf.x + std::fabs(xBasis[1].z) * xHalf.y
					+ std::fabs(xBasis[2].z) * xHalf.z) * 2.0f;
		}

		// ★ THE AUTHORED TRANSFORM AGAINST THE FIT, re-derived here from the SAME
		// bounds. A scene authored before ZM_ComputePropFit existed would put a
		// half-size prop half-buried in the floor and still take perfectly valid
		// photographs of it.
		const ZM_PropData& xData = ZM_GetPropData(xRow.m_eProp);
		const ZM_PropFit xExpected = ZM_ComputePropFit(
			xLocal.m_xMin, xLocal.m_xMax,
			xData.m_fWidth, xData.m_fDepth, xData.m_fHeight);
		xState.m_bFitMatches =
			std::fabs(xScale.x - xExpected.m_fScale) <= fIPS_FIT_EPSILON
			&& std::fabs(xScale.y - xExpected.m_fScale) <= fIPS_FIT_EPSILON
			&& std::fabs(xScale.z - xExpected.m_fScale) <= fIPS_FIT_EPSILON
			&& std::fabs(xPosition.y - xExpected.m_fGroundY) <= fIPS_FIT_EPSILON;
		if (!xState.m_bFitMatches)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] '%s': authored scale (%.5f, %.5f, %.5f) / "
				"y %.5f vs fit scale %.5f / y %.5f over mesh bounds "
				"[%.4f..%.4f, %.4f..%.4f, %.4f..%.4f]",
				xRow.m_szEntityName,
				(double)xScale.x, (double)xScale.y, (double)xScale.z, (double)xPosition.y,
				(double)xExpected.m_fScale, (double)xExpected.m_fGroundY,
				(double)xLocal.m_xMin.x, (double)xLocal.m_xMax.x,
				(double)xLocal.m_xMin.y, (double)xLocal.m_xMax.y,
				(double)xLocal.m_xMin.z, (double)xLocal.m_xMax.z);
		}
	}

	// Request one dump, recording where the subject landed. Returns false while
	// the viewport is not yet sampleable (the caller retries).
	bool IPSTakeShot(u_int uIndex, const Zenith_Maths::Vector3& xSubject, bool bAimed)
	{
		Zenith_CameraComponent* pxCamera = IPSResolveCamera();
		if (pxCamera == nullptr)
		{
			FailIPS("the authored PlayerHome camera vanished before a shot");
			return false;
		}

		IPSShotRecord& xShot = g_axIPSShots[uIndex];
		if (!IPSResolveViewport(*pxCamera, xShot))
		{
			return false;
		}

		Zenith_Maths::Vector3 xEye(0.0f);
		pxCamera->GetPosition(xEye);
		xShot.m_xEye = xEye;
		xShot.m_bAimed = bAimed;

		Zenith_Maths::Vector2 xNdc(0.0f);
		xShot.m_bSubjectInFrame =
			IPSProjectPoint(*pxCamera, xSubject, xNdc) && IPSNdcInSafeInterior(xNdc);
		xShot.m_xSubjectNdc = xNdc;

		std::error_code xError;
		std::filesystem::create_directories(IPSCaptureDir(), xError);

		Flux_Screenshot::RequestDump(xShot.m_strPath.c_str());
		xShot.m_bRequested = true;
		return true;
	}

	bool IPSReadMeanRGB(const Zenith_TestTGAImage& xImage,
		float fCenterX, float fCenterY, Zenith_Maths::Vector3& xOut)
	{
		if (!xImage.IsValid() || !std::isfinite(fCenterX) || !std::isfinite(fCenterY))
		{
			return false;
		}
		const int64_t iCenterX = static_cast<int64_t>(std::lround(fCenterX));
		const int64_t iCenterY = static_cast<int64_t>(std::lround(fCenterY));
		const int64_t iRadius = static_cast<int64_t>(uIPS_SAMPLE_RADIUS);
		if (iCenterX - iRadius < 0 || iCenterY - iRadius < 0
			|| iCenterX + iRadius >= static_cast<int64_t>(xImage.m_uWidth)
			|| iCenterY + iRadius >= static_cast<int64_t>(xImage.m_uHeight))
		{
			return false;
		}

		uint64_t ulRed = 0u, ulGreen = 0u, ulBlue = 0u, ulSamples = 0u;
		for (int64_t iY = iCenterY - iRadius; iY <= iCenterY + iRadius; ++iY)
		{
			for (int64_t iX = iCenterX - iRadius; iX <= iCenterX + iRadius; ++iX)
			{
				const uint8_t* puBGRA = xImage.GetPixelBGRA(
					static_cast<uint32_t>(iX), static_cast<uint32_t>(iY));
				ulBlue += puBGRA[0];
				ulGreen += puBGRA[1];
				ulRed += puBGRA[2];
				++ulSamples;
			}
		}
		if (ulSamples == 0u)
		{
			return false;
		}
		const float fNormalise = 1.0f / (255.0f * static_cast<float>(ulSamples));
		xOut = Zenith_Maths::Vector3(
			static_cast<float>(ulRed) * fNormalise,
			static_cast<float>(ulGreen) * fNormalise,
			static_cast<float>(ulBlue) * fNormalise);
		return true;
	}

	// Everything the room has to yield before a picture means anything.
	bool IPSResolveRoom()
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxData == nullptr)
		{
			return false;
		}
		const int iPlayerHome =
			static_cast<int>(ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME).m_uBuildIndex);
		if (g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex != iPlayerHome)
		{
			return false;
		}

		const Zenith_Entity xCamera = IPSFindEntity("PlayerHomeCamera");
		if (!xCamera.IsValid()
			|| xCamera.TryGetComponent<Zenith_CameraComponent>() == nullptr)
		{
			return false;
		}

		// ★ EVERY subject, not just the first. A scene that authored one prop and
		// dropped another would otherwise produce one good picture and one silent
		// nothing, and a reviewer comparing files would never learn which of them
		// had never existed.
		for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
		{
			Zenith_Entity xEntity = IPSFindEntity(axIPS_SUBJECTS[u].m_szEntityName);
			if (!xEntity.IsValid())
			{
				return false;
			}
			g_axIPSSubjects[u].m_xEntityID = xEntity.GetEntityID();
		}

		g_xIPSScene = xScene;
		g_xIPSCameraID = xCamera.GetEntityID();
		return true;
	}
}

//-----------------------------------------------------------------------------

static void Setup_ZMImportedPropShowcase()
{
	g_eIPSPhase = IPSPhase::AwaitRoom;
	g_iIPSPhaseFrames = 0;
	g_uIPSAimedIndex = 0u;
	g_bIPSFailed = false;
	g_bIPSScenePaused = false;
	g_fIPSResolvedCeiling = 0.0f;
	g_fIPSLiveCameraY = 0.0f;
	g_xIPSPivot = Zenith_Maths::Vector3(0.0f);
	g_xIPSUnclampedEye = Zenith_Maths::Vector3(0.0f);
	g_szIPSFailure = "test did not reach verification";
	g_xIPSScene = Zenith_Scene();
	g_xIPSCameraID = INVALID_ENTITY_ID;
	for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
	{
		g_axIPSSubjects[u] = IPSSubjectState();
	}

	// ★ STALE SHOTS ARE DELETED, not overwritten-if-lucky. See the header.
	const std::filesystem::path xDir = IPSCaptureDir();
	std::error_code xError;
	std::filesystem::create_directories(xDir, xError);

	for (u_int u = 0u; u < uIPS_SHOT_COUNT; ++u)
	{
		std::string strKey;
		if (u == uIPS_SHOT_PLAYER_VIEW)      { strKey = "player_view"; }
		else if (u == uIPS_SHOT_UNCLAMPED)   { strKey = "player_view_unclamped"; }
		else if (u == uIPS_SHOT_ROOM_WIDE)   { strKey = "room_wide"; }
		else
		{
			const u_int uOffset = u - uIPS_FIXED_SHOT_COUNT;
			strKey = std::string(axIPS_SUBJECTS[uOffset / 2u].m_szKey)
				+ ((uOffset % 2u) == 0u ? "_three_quarter" : "_detail");
		}
		g_axIPSShots[u] = IPSShotRecord();
		g_axIPSShots[u].m_strPath = (xDir / ("prop_" + strKey + ".tga")).string();
		std::filesystem::remove(g_axIPSShots[u].m_strPath, xError);
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fIPS_FIXED_DT);

	g_xEngine.Scenes().RegisterSceneBuildIndex(
		static_cast<int>(ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME).m_uBuildIndex),
		GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(
		static_cast<int>(ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME).m_uBuildIndex),
		SCENE_LOAD_SINGLE);

	g_bIPSActive = true;
}

static bool Step_ZMImportedPropShowcase(int)
{
	if (!g_bIPSActive || g_bIPSFailed || g_eIPSPhase == IPSPhase::Done)
	{
		return false;
	}
	++g_iIPSPhaseFrames;

	switch (g_eIPSPhase)
	{
	case IPSPhase::AwaitRoom:
	{
		if (!IPSResolveRoom())
		{
			if (g_iIPSPhaseFrames > iIPS_ROOM_DEADLINE_FRAMES)
			{
				FailIPS("PlayerHome never became the settled active scene with an "
					"authored camera and every roster subject in it");
				return false;
			}
			return true;
		}
		g_eIPSPhase = IPSPhase::LiveSettle;
		g_iIPSPhaseFrames = 0;
		return true;
	}

	case IPSPhase::LiveSettle:
	{
		if (g_iIPSPhaseFrames < iIPS_SETTLE_FRAMES)
		{
			return true;
		}

		// ---- Everything structural, measured ONCE, AFTER the settle -----------
		//
		// ★★ AFTER, NOT AT SCENE-RESOLVE, AND THE DIFFERENCE IS THE WHOLE
		// MEASUREMENT. A scene has data -- entities, names, transforms -- one frame
		// before its components' OnStart has run, and BOTH things this reads are
		// written by an OnStart: ZM_InteriorFurniture::OnStart is what adds the
		// ModelComponent and loads the model, and ZM_FollowCamera::OnStart is what
		// resolves the ceiling. Measured at resolve time, every prop reports NO
		// model and the camera reports NO ceiling -- and both read exactly like the
		// real failures they are supposed to detect (a bake-less clone, an
		// unclamped boom), which is the worst possible shape for a diagnostic.
		for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
		{
			IPSMeasureSubject(u);
		}

		Zenith_Entity xFollowEntity = g_xEngine.Scenes().ResolveEntity(g_xIPSCameraID);
		const ZM_FollowCamera* pxFollow =
			xFollowEntity.TryGetComponent<ZM_FollowCamera>();
		if (pxFollow != nullptr)
		{
			g_fIPSResolvedCeiling = pxFollow->GetCeilingY();
		}

		// ★ THE REFERENCE POSE, derived from the SHIPPED seams -- the live
		// component's own authored yaw and ComputeDesiredPosition -- rather than
		// typed here, so it stays the honest "before" if the constants are ever
		// re-tuned.
		Zenith_Entity xPlayer = IPSFindEntity("Player");
		Zenith_TransformComponent* pxPlayerTransform = xPlayer.IsValid()
			? xPlayer.TryGetComponent<Zenith_TransformComponent>()
			: nullptr;
		if (pxPlayerTransform != nullptr && pxFollow != nullptr)
		{
			Zenith_Maths::Vector3 xPlayerPos(0.0f);
			pxPlayerTransform->GetPosition(xPlayerPos);
			g_xIPSPivot = xPlayerPos
				+ Zenith_Maths::Vector3(0.0f, ZM_FollowCamera::GetPivotHeight(), 0.0f);
			g_xIPSUnclampedEye = ZM_FollowCamera::ComputeDesiredPosition(
				xPlayerPos, pxFollow->GetAuthoredYaw());
		}

		// The LIVE follow-camera height, read after the spring has settled. This
		// is the number the ceiling clamp exists to bound.
		if (Zenith_CameraComponent* pxCamera = IPSResolveCamera())
		{
			Zenith_Maths::Vector3 xEye(0.0f);
			pxCamera->GetPosition(xEye);
			g_fIPSLiveCameraY = xEye.y;
		}

		if (!IPSTakeShot(uIPS_SHOT_PLAYER_VIEW, g_xIPSPivot, /*bAimed*/ false))
		{
			if (g_iIPSPhaseFrames > iIPS_SETTLE_FRAMES * 3)
			{
				FailIPS("the live-camera shot could not obtain a valid viewport");
				return false;
			}
			return true;
		}
		g_eIPSPhase = IPSPhase::LiveHold;
		g_iIPSPhaseFrames = 0;
		return true;
	}

	case IPSPhase::LiveHold:
	{
		if (g_iIPSPhaseFrames < iIPS_HOLD_FRAMES)
		{
			return true;
		}
		// ★ THE FREEZE. From here ZM_FollowCamera::OnLateUpdate stops writing the
		// camera, so a pose set below survives into the frame that renders it.
		g_xEngine.Scenes().SetScenePaused(g_xIPSScene, true);
		g_bIPSScenePaused = true;

		// ★★ THE "BEFORE" FRAME, TAKEN DELIBERATELY. The lens goes to the pose
		// ComputeDesiredPosition returns with NO ceiling clamp -- what the shipped
		// boom did until this change -- looking at the same pivot. It is here
		// because "the camera was too high" is a claim a reader should be able to
		// SEE rather than take on trust, and because a before/after pair taken in
		// one run cannot drift apart the way two runs a week apart do. Nothing
		// asserts anything about this frame; it is a reference.
		Zenith_CameraComponent* pxCamera = IPSResolveCamera();
		if (pxCamera == nullptr)
		{
			FailIPS("the camera vanished before the unclamped reference shot");
			return false;
		}
		IPSPointCameraAt(*pxCamera, g_xIPSUnclampedEye, g_xIPSPivot);
		g_eIPSPhase = IPSPhase::UnclampedSettle;
		g_iIPSPhaseFrames = 0;
		return true;
	}

	case IPSPhase::UnclampedSettle:
	{
		if (g_iIPSPhaseFrames < iIPS_SETTLE_FRAMES)
		{
			return true;
		}
		Zenith_CameraComponent* pxCamera = IPSResolveCamera();
		if (pxCamera == nullptr)
		{
			FailIPS("the camera vanished mid-capture");
			return false;
		}
		IPSPointCameraAt(*pxCamera, g_xIPSUnclampedEye, g_xIPSPivot);
		if (!IPSTakeShot(uIPS_SHOT_UNCLAMPED, g_xIPSPivot, /*bAimed*/ false))
		{
			if (g_iIPSPhaseFrames > iIPS_SETTLE_FRAMES * 3)
			{
				FailIPS("the unclamped reference shot could not obtain a valid viewport");
				return false;
			}
			return true;
		}
		g_eIPSPhase = IPSPhase::UnclampedHold;
		g_iIPSPhaseFrames = 0;
		return true;
	}

	case IPSPhase::UnclampedHold:
	{
		if (g_iIPSPhaseFrames < iIPS_HOLD_FRAMES)
		{
			return true;
		}
		Zenith_CameraComponent* pxCamera = IPSResolveCamera();
		if (pxCamera == nullptr)
		{
			FailIPS("the camera vanished before the first aimed shot");
			return false;
		}
		g_uIPSAimedIndex = 0u;   // room_wide
		IPSAimRoomWide(*pxCamera);
		g_eIPSPhase = IPSPhase::AimedSettle;
		g_iIPSPhaseFrames = 0;
		return true;
	}

	case IPSPhase::AimedSettle:
	{
		if (g_iIPSPhaseFrames < iIPS_SETTLE_FRAMES)
		{
			return true;
		}
		// ★ THE FREEZE IS ASSERTED, NOT ASSUMED: re-aim and re-read, and fail if
		// anything moved the lens while the scene was paused.
		Zenith_CameraComponent* pxCamera = IPSResolveCamera();
		if (pxCamera == nullptr)
		{
			FailIPS("the camera vanished mid-capture");
			return false;
		}
		Zenith_Maths::Vector3 xBefore(0.0f);
		pxCamera->GetPosition(xBefore);

		u_int uSubject = 0u;
		bool bDetail = false;
		const bool bHasSubject =
			IPSAimedIndexToSubject(g_uIPSAimedIndex, uSubject, bDetail);
		if (bHasSubject)
		{
			if (!IPSAimAtSubject(*pxCamera, uSubject, bDetail))
			{
				std::snprintf(g_aszIPSDetail, sizeof(g_aszIPSDetail),
					"could not aim the lens at '%s' -- its measured world size is "
					"degenerate, so there is nothing to frame",
					axIPS_SUBJECTS[uSubject].m_szEntityName);
				FailIPS(g_aszIPSDetail);
				return false;
			}
		}
		else
		{
			IPSAimRoomWide(*pxCamera);
		}

		Zenith_Maths::Vector3 xAfter(0.0f);
		pxCamera->GetPosition(xAfter);
		if (glm::length(xAfter - xBefore) > fIPS_POSE_DRIFT_EPSILON)
		{
			FailIPS("the paused camera drifted between aiming and shooting -- the "
				"scene pause is no longer gating ZM_FollowCamera::OnLateUpdate, so "
				"every aimed shot would be of a pose this test did not choose");
			return false;
		}

		const Zenith_Maths::Vector3 xSubjectPoint = bHasSubject
			? g_axIPSSubjects[uSubject].m_xWorldCentre
			: Zenith_Maths::Vector3(0.0f, fIPS_WIDE_LOOK_Y, fIPS_WIDE_LOOK_Z);

		if (!IPSTakeShot(g_uIPSAimedIndex + uIPS_SHOT_ROOM_WIDE, xSubjectPoint,
			/*bAimed*/ true))
		{
			if (g_iIPSPhaseFrames > iIPS_SETTLE_FRAMES * 3)
			{
				FailIPS("an aimed shot could not obtain a valid viewport");
				return false;
			}
			return true;
		}
		g_eIPSPhase = IPSPhase::AimedHold;
		g_iIPSPhaseFrames = 0;
		return true;
	}

	case IPSPhase::AimedHold:
	{
		if (g_iIPSPhaseFrames < iIPS_HOLD_FRAMES)
		{
			return true;
		}
		++g_uIPSAimedIndex;
		if (g_uIPSAimedIndex + uIPS_SHOT_ROOM_WIDE >= uIPS_SHOT_COUNT)
		{
			g_eIPSPhase = IPSPhase::Done;
			return false;
		}

		Zenith_CameraComponent* pxCamera = IPSResolveCamera();
		if (pxCamera == nullptr)
		{
			FailIPS("the camera vanished between aimed shots");
			return false;
		}
		u_int uSubject = 0u;
		bool bDetail = false;
		if (IPSAimedIndexToSubject(g_uIPSAimedIndex, uSubject, bDetail))
		{
			if (!IPSAimAtSubject(*pxCamera, uSubject, bDetail))
			{
				std::snprintf(g_aszIPSDetail, sizeof(g_aszIPSDetail),
					"could not aim the lens at '%s' for a later shot",
					axIPS_SUBJECTS[uSubject].m_szEntityName);
				FailIPS(g_aszIPSDetail);
				return false;
			}
		}
		else
		{
			IPSAimRoomWide(*pxCamera);
		}
		g_eIPSPhase = IPSPhase::AimedSettle;
		g_iIPSPhaseFrames = 0;
		return true;
	}

	case IPSPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ZMImportedPropShowcase()
{
	bool bPassed = !g_bIPSFailed;
	if (g_bIPSFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_ImportedPropShowcase] %s", g_szIPSFailure);
	}

	// ---- Every subject is really there, at the size the fit says ------------
	for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
	{
		const IPSSubjectState& xState = g_axIPSSubjects[u];
		const IPSSubject& xRow = axIPS_SUBJECTS[u];

		if (!xState.m_bModelLoaded)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] '%s' reports NO loaded model, so every "
				"capture of it is a photograph of empty floor. On a clone with no "
				"asset bake this is expected and this test cannot run.",
				xRow.m_szEntityName);
			bPassed = false;
			continue;
		}
		if (!xState.m_bFitMatches)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] '%s': the AUTHORED transform does not match "
				"ZM_ComputePropFit over its own mesh bounds -- the scene was authored "
				"before the fit existed, or by a boot that could not read the mesh. "
				"Re-author from a _True boot before trusting these shots.",
				xRow.m_szEntityName);
			bPassed = false;
		}

		const ZM_PropData& xData = ZM_GetPropData(xRow.m_eProp);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_ImportedPropShowcase] OBSERVED '%s' model %.4f x %.4f x %.4f m -> "
			"scale %.4f, ground y %.4f -> %.4f x %.4f x %.4f m at (%.3f, %.3f, %.3f) "
			"(roster %.2f x %.2f x %.2f)",
			xRow.m_szEntityName,
			(double)xState.m_xModelSize.x, (double)xState.m_xModelSize.y,
			(double)xState.m_xModelSize.z, (double)xState.m_fAuthoredScale,
			(double)xState.m_fAuthoredGroundY,
			(double)xState.m_xWorldSize.x, (double)xState.m_xWorldSize.y,
			(double)xState.m_xWorldSize.z,
			(double)xState.m_xWorldCentre.x, (double)xState.m_xWorldCentre.y,
			(double)xState.m_xWorldCentre.z,
			(double)xData.m_fWidth, (double)xData.m_fDepth, (double)xData.m_fHeight);

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_ImportedPropShowcase] OBSERVED '%s' authored quat (w %.5f, y %.5f) "
			"= %.1f deg yaw; model +X faces (%.3f, %.3f, %.3f); world footprint "
			"%.3f x %.3f x %.3f m",
			xRow.m_szEntityName,
			(double)xState.m_xRotation.w, (double)xState.m_xRotation.y,
			(double)xState.m_fAuthoredYawDegrees,
			(double)xState.m_xFacing.x, (double)xState.m_xFacing.y,
			(double)xState.m_xFacing.z,
			(double)xState.m_xWorldFootprint.x, (double)xState.m_xWorldFootprint.y,
			(double)xState.m_xWorldFootprint.z);
	}

	// ---- The room is visible from the shipped camera ------------------------
	//
	// ★ THIS IS THE PICTURE-SIDE HALF OF THE CEILING CLAMP. The pure clamp is
	// pinned headlessly by ZM_Tests_FollowCameraCeiling; this clause is what
	// proves the LIVE camera in the REAL room actually ended up under the slab.
	const float fCeilingCap =
		g_fIPSResolvedCeiling - ZM_FollowCamera::GetCeilingClearance();
	if (g_fIPSResolvedCeiling <= 0.0f
		|| g_fIPSResolvedCeiling >= ZM_FollowCamera::GetNoCeiling())
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_ImportedPropShowcase] the PlayerHome follow camera resolved NO "
			"ceiling (%.3f), so its boom is unbounded indoors and the live shot is "
			"of the ceiling slab", (double)g_fIPSResolvedCeiling);
		bPassed = false;
	}
	else if (g_fIPSLiveCameraY > fCeilingCap + 1.0e-3f)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_ImportedPropShowcase] the live camera settled at y=%.3f m, above the "
			"%.3f m cap for this room's %.3f m ceiling -- the player cannot see inside "
			"the house", (double)g_fIPSLiveCameraY, (double)fCeilingCap,
			(double)g_fIPSResolvedCeiling);
		bPassed = false;
	}

	// ---- Every shot landed, and the aimed ones contain their subject --------
	for (u_int u = 0u; u < uIPS_SHOT_COUNT; ++u)
	{
		const IPSShotRecord& xShot = g_axIPSShots[u];
		if (!xShot.m_bRequested)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] shot %u was never requested: %s",
				u, xShot.m_strPath.c_str());
			bPassed = false;
			continue;
		}

		Zenith_TestTGAImage xImage;
		if (!Zenith_TestLoadTGA(xShot.m_strPath.c_str(), xImage) || !xImage.IsValid())
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] shot %u did not land on disk as a readable "
				"TGA: %s", u, xShot.m_strPath.c_str());
			bPassed = false;
			continue;
		}

		if (xShot.m_bAimed && !xShot.m_bSubjectInFrame)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] the subject does NOT project into the safe "
				"viewport interior of aimed shot %u (ndc %.3f, %.3f) -- the file "
				"exists but is not a picture of it", u,
				(double)xShot.m_xSubjectNdc.x, (double)xShot.m_xSubjectNdc.y);
			bPassed = false;
		}

		// LOGGED, never asserted: an absolute framebuffer reading tracks the
		// scene's lighting rather than the asset.
		Zenith_Maths::Vector3 xRGB(0.0f);
		const float fPatchX = xShot.m_xViewportPos.x
			+ (xShot.m_xSubjectNdc.x * 0.5f + 0.5f) * xShot.m_xViewportSize.x;
		const float fPatchY = xShot.m_xViewportPos.y
			+ (1.0f - (xShot.m_xSubjectNdc.y * 0.5f + 0.5f)) * xShot.m_xViewportSize.y;
		const bool bSampled = IPSReadMeanRGB(xImage, fPatchX, fPatchY, xRGB);

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_ImportedPropShowcase] OBSERVED shot %u eye (%.2f, %.2f, %.2f) "
			"ndc (%.3f, %.3f) RGB=%s(%.4f, %.4f, %.4f) | capture %ux%u | %s",
			u, (double)xShot.m_xEye.x, (double)xShot.m_xEye.y, (double)xShot.m_xEye.z,
			(double)xShot.m_xSubjectNdc.x, (double)xShot.m_xSubjectNdc.y,
			bSampled ? "" : "unsampled ",
			(double)xRGB.x, (double)xRGB.y, (double)xRGB.z,
			xImage.m_uWidth, xImage.m_uHeight, xShot.m_strPath.c_str());
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_ImportedPropShowcase] OBSERVED room ceiling %.2f m, live lens y %.3f m",
		(double)g_fIPSResolvedCeiling, (double)g_fIPSLiveCameraY);

	// ★ THE LINES A WORK LOG IS BUILT FROM, printed pass or fail, one per shot so
	// adding a subject cannot leave a capture unmentioned.
	for (u_int u = 0u; u < uIPS_SHOT_COUNT; ++u)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_ImportedPropShowcase] CAPTURE -> %s (copy it elsewhere before the "
			"NEXT run, which deletes it at Setup)", g_axIPSShots[u].m_strPath.c_str());
	}

	return bPassed;
}

static void Teardown_ZMImportedPropShowcase()
{
	if (g_bIPSScenePaused && g_xIPSScene.IsValid())
	{
		g_xEngine.Scenes().SetScenePaused(g_xIPSScene, false);
	}
	g_bIPSScenePaused = false;
	Zenith_InputSimulator::ClearFixedDt();
	Zenith_InputSimulator::ResetAllInputState();
	g_xIPSCameraID = INVALID_ENTITY_ID;
	g_bIPSActive = false;
}

static const Zenith_AutomatedTest g_xZMImportedPropShowcaseTest = {
	"ZM_ImportedPropShowcase_Test",
	&Setup_ZMImportedPropShowcase,
	&Step_ZMImportedPropShowcase,
	&Verify_ZMImportedPropShowcase,
	// A 600-frame room budget plus (3 + 2 per subject) shots at
	// (120 settle + 12 hold), with the aimed ones allowed three settles' worth of
	// viewport retries. Every stage owns a deadline that FAILS with a diagnostic;
	// this is only a backstop, and it is generous enough that adding a roster row
	// does not silently truncate the run.
	/* maxFrames */ 6000,
	true /* m_bRequiresGraphics */,
	false /* m_bManualOnly */,
	&Teardown_ZMImportedPropShowcase,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMImportedPropShowcaseTest);

#endif // ZENITH_INPUT_SIMULATOR
