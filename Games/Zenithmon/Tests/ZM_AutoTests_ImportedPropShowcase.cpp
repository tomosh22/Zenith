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
// ★★ AND THE ROSTER IS KEYED BY ROOM, which the FIFTH import forced. Every
// subject through the shelf stood in PlayerHome, so a single scene load at Setup
// served all of them. ZM_PROP_COUNTER stands only in ProfLab, and the two
// alternatives to a second room were both worse than the load it costs: moving a
// lab bench into a bedroom changes the GAME to suit the harness, and a second
// showcase file duplicates this one's phase machine, capture, projection and
// verification -- leaving two places to add the sixth asset to. The rooms are
// captured in ORDER, so PlayerHome is finished before ProfLab is loaded and the
// first four subjects are framed exactly as they were. See axIPS_ROOMS.
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
//                 "Can you see the room when you are standing in it?" PlayerHome
//                 only -- see IPSRoom::m_bFollowCameraPair.
//   player_view_unclamped
//                 the SAME pivot from the pose the boom would have taken with no
//                 ceiling clamp -- the lens ~3.9 m up in a room whose ceiling
//                 slab starts at 3.0 m. Nothing is asserted about it; it is the
//                 "before" half of the pair, taken in the SAME run so the two
//                 cannot drift apart the way two runs a week apart do. See
//                 ZM_FollowCamera::ClampBoomBelowCeiling.
//   room_wide_<room>
//                 from that room's doorway, looking in. The imported props IN
//                 CONTEXT -- is each the right size against the room, the door
//                 and the generated furniture beside it? NOT every prop: a
//                 subject hard against a side wall falls outside a 65-degree
//                 lens, and PlayerHome's shelf does. The numbers, and why no pose
//                 fixes it, are at the room-wide block below. ProfLab's is the
//                 shot the two benches were commissioned for: it is the only
//                 frame in the run that holds both of them, which is what makes
//                 their two DIFFERENT authored yaws checkable by eye.
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
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"   // szZM_PROFLAB_CAMERA_ENTITY_NAME
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

	// ---- The rooms ---------------------------------------------------------
	//
	// ★★ THE ROSTER IS KEYED BY ROOM NOW, and the fifth import is what forced it.
	// Every subject up to the shelf stood in PlayerHome, so this file loaded that
	// one scene at Setup and resolved every row by name inside it. ZM_PROP_COUNTER
	// appears ONLY in ProfLab -- it is the lab's own furniture, and putting a bench
	// in a bedroom to make a test simpler would be changing the GAME to suit the
	// harness. Added as a plain row it would have failed IPSResolveRoom and burned
	// the 600-frame room deadline before saying why, because "the subject is not in
	// this scene" and "the scene never settled" were the same code path.
	//
	// ★ THE OTHER FOUR SUBJECTS PAY NOTHING FOR IT. The rooms are captured in
	// order, and PlayerHome is finished -- its live pair, its wide, and all eight
	// of its aimed shots -- before ProfLab is loaded at all. What the run costs is
	// one extra scene load and that room's own shots; what it does NOT cost is any
	// change to how the first four are framed. The alternative -- a second copy of
	// this file's phase machine, capture, projection and verification, for one prop
	// -- would have left TWO places to add the sixth asset to, which is the shape
	// this roster exists to avoid.
	//
	// ★ ROWS MUST BE GROUPED BY ROOM, and Setup ASSERTS that rather than sorting.
	// A room is loaded once, when the plan first reaches it; an interleaved roster
	// would silently reload a scene mid-capture and photograph a freshly-spawned
	// world. Sorting would hide the mistake, so it fails on it instead.
	// ★★ AND A ROOM NEED NOT BE A ROOM. The sixth import is placed OUTDOORS as
	// well as in a bedroom -- four barrels against Dawnmere's two building walls
	// (ZM_DawnmereDressing.h) -- and this file's own claim is that "adding a row
	// to axIPS_SUBJECTS is the whole cost of a new asset". That was false for the
	// first subject standing on terrain, so the two things an interior supplies
	// and a town does not are now per-room flags rather than assumptions:
	//
	//   m_bClampIntoRoom  an interior has four walls and a ceiling to keep the
	//                     lens inside; Dawnmere has none, and reading a room spec
	//                     for it would clamp every eye into a 15.5 x 11.5 m box
	//                     centred on the town's ORIGIN, 100 m from the subject.
	//   m_bGroundIsZero   an interior floor IS y = 0, so an authored prop's Y is
	//                     exactly ZM_ComputePropFit's ground lift and the fit
	//                     check can assert it. Outdoors the lift is added to a
	//                     SAMPLED terrain height, so that clause checks the scale
	//                     and reports the implied ground rather than asserting a
	//                     number it cannot re-derive without the heightmap.
	struct IPSRoom
	{
		ZM_INTERIOR_ROOM m_eRoom;        // read only when m_bClampIntoRoom
		ZM_SCENE_ID      m_eScene;
		const char*      m_szSceneStem;    // under GAME_ASSETS_DIR "Scenes/"
		const char*      m_szCameraName;
		// ★ THE WIDE POSE IS SIX NUMBERS, not the three it was. While every room
		// was an interior the eye's X was 0 and its Y was the room's wall height
		// less a margin, so only Z varied and the other two were computed. Neither
		// holds for a town: Dawnmere's shot stands off ONE BUILDING at x = 92, and
		// its height is a terrain elevation rather than a ceiling. Both interior
		// rows below carry exactly what those expressions produced (PlayerHome
		// 3.00 - 0.40 = 2.60, ProfLab 3.50 - 0.40 = 3.10), so no interior framing
		// moves.
		float            m_fWideEyeX;
		float            m_fWideEyeY;
		float            m_fWideEyeZ;
		float            m_fWideLookX;
		float            m_fWideLookY;
		float            m_fWideLookZ;
		// ★ ONLY THE FIRST ROOM TAKES THE FOLLOW-CAMERA PAIR. Those two shots are
		// about ZM_FollowCamera::ClampBoomBelowCeiling, not about any asset, and
		// the clamp is one behaviour rather than one per room -- a second copy in
		// ProfLab would spend 264 frames photographing the same seam to assert the
		// same thing. ProfLab's own arrival pose already has headless coverage in
		// ZM_WorldTraversal/ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw.
		bool             m_bFollowCameraPair;
		bool             m_bClampIntoRoom;
		bool             m_bGroundIsZero;
	};

	// ★ PROFLAB'S WIDE EYE SITS DEEPER INTO THE ROOM THAN PLAYERHOME'S, and that
	// is arithmetic rather than taste. Its two benches are at x = +/-8.20, against
	// a 65-degree lens whose horizontal half-frame is 48.6 degrees at 16:9, so an
	// eye at z holds BOTH only while 8.20 / (z + 3.00) < tan(48.6 deg) = 1.135 --
	// i.e. z > 4.22. At 6.60 they sit 40.5 degrees off the view axis in plan (39.6
	// in three dimensions, once the eye's height above them is counted), and the
	// eye is still 0.75 m inside the room clamp. This is the shot the counters were
	// commissioned for: it is where a reader can see that the two benches stand
	// against OPPOSITE walls facing each other, which is exactly what their two
	// disagreeing yaws claim and the one thing a per-subject shot cannot show.
	constexpr IPSRoom axIPS_ROOMS[] =
	{
		{ ZM_INTERIOR_ROOM_PLAYER_HOME, ZM_SCENE_PLAYERHOME, "PlayerHome",
			"PlayerHomeCamera",
			/* eye */ 0.00f, 2.60f, 4.60f, /* look */ 0.00f, 0.80f, -2.00f,
			/* follow pair */ true, /* clamp */ true, /* ground is 0 */ true },
		{ ZM_INTERIOR_ROOM_PROF_LAB, ZM_SCENE_PROFLAB, "ProfLab",
			szZM_PROFLAB_CAMERA_ENTITY_NAME,
			/* eye */ 0.00f, 3.10f, 6.60f, /* look */ 0.00f, 0.90f, -3.00f,
			false, true, true },
		// ★ DAWNMERE'S WIDE IS AIMED AT THE HOUSE, NOT AT THE TOWN. Every other
		// wide looks down a room's axis from its doorway; a town has no such shot,
		// and a 256 m map framed whole shows nothing about a 1 m barrel. So this
		// one stands off the home's frontage looking at its wall, which is the
		// context the outdoor placement is actually making a claim about: that a
		// barrel reads as standing against a building. The eye Z is on the
		// approach side of the entrance face (z = 100), the look-at is the wall.
		//
		// m_eRoom is PLAYER_HOME and is NEVER READ -- m_bClampIntoRoom is false.
		// It is set to a real value rather than a sentinel because the field is a
		// plain enum with no "none", and a sentinel would be a second thing to
		// remember; the flag beside it is what decides.
		{ ZM_INTERIOR_ROOM_PLAYER_HOME, ZM_SCENE_DAWNMERE, "Dawnmere",
			"DawnmerePreviewCamera",
			/* eye */ 92.00f, 27.00f, 88.00f, /* look */ 92.00f, 25.20f, 99.40f,
			false, /* clamp */ false, /* ground is 0 */ false },

		// ★★ A SECOND DAWNMERE POSE, SHARING THE SCENE, aimed at the town's own
		// dressing rather than at the house. It exists because the ruling that
		// every generated prop is placed put 28 more of them on this map -- the
		// notice board and signage on the plaza's north edge, lantern posts up the
		// route lane -- and the row above photographs none of them.
		//
		// ★ WHAT IT IS FOR IS THE THING THE UNITS CANNOT SEE. The headless clauses
		// prove every roster row is placed, clears the keep-out and is inside the
		// terrain; none of them can tell whether a prop RENDERS, is the right size
		// against a building, or is half-sunk in the ground. That needs a picture,
		// and before this row there was no picture of this content at all.
		{ ZM_INTERIOR_ROOM_PLAYER_HOME, ZM_SCENE_DAWNMERE, "DawnmereTown",
			"DawnmerePreviewCamera",
			/* eye */ 104.00f, 29.00f, 78.00f, /* look */ 106.00f, 25.50f, 96.00f,
			false, /* clamp */ false, /* ground is 0 */ false },
	};
	constexpr u_int uIPS_ROOM_COUNT =
		(u_int)(sizeof(axIPS_ROOMS) / sizeof(axIPS_ROOMS[0]));

	// ---- The roster --------------------------------------------------------
	//
	// One row per IMPORTED prop standing in an interior. Adding an asset means
	// adding a row; nothing else in this file is per-prop.
	//
	// The two poses are spherical offsets from the prop, in multiples of its own
	// LONGEST SCALED AXIS -- so re-exporting an asset at a different scale moves
	// the camera with it instead of leaving two photographs of a wall. Azimuth 0
	// looks along +Z at the subject and increases counter-clockwise about +Y, so
	// +90 puts the eye at +X of the subject and -90 at -X.
	struct IPSPose
	{
		float m_fAzimuthDegrees;
		float m_fElevationDegrees;
		float m_fDistanceScale;
	};

	struct IPSSubject
	{
		const char*      m_szKey;        // filename stem for its two shots
		const char*      m_szEntityName; // the authored entity, from a dressing header
		// ★ AN INDEX INTO axIPS_ROOMS, not a ZM_INTERIOR_ROOM. Keying on the enum
		// worked only while every subject was indoors, and there is no enumerator
		// for "outside, in Dawnmere". The room ROW is the thing a subject actually
		// belongs to -- it is what carries the scene, the camera and the two flags.
		u_int            m_uRoom;
		ZM_PROP_ID       m_eProp;
		IPSPose          m_xThreeQuarter;
		IPSPose          m_xDetail;
	};

	constexpr IPSSubject axIPS_SUBJECTS[] =
	{
		// The bed stands in the -X/-Z corner, so the lens swings toward +X/+Z.
		{ "bed", "HomeBed", 0u, ZM_PROP_BED,
			/* three-quarter */ {  38.0f, 22.0f, 1.55f },
			/* detail        */ { 118.0f, 20.0f, 0.95f } },

		// The table stands against +X, so its azimuths are NEGATIVE -- the mirror
		// of the bed's. A shared value would aim this one straight into the +X
		// wall, which the room clamp would then slide along it.
		{ "table", "HomeTable", 0u, ZM_PROP_TABLE,
			/* three-quarter */ { -50.0f, 24.0f, 1.55f },
			/* detail        */ { -115.0f, 26.0f, 0.95f } },

		// The chair shares the table's +X side, so its azimuths are negative too.
		// It is the SMALLEST subject, and its distances are multiples of its own
		// longest axis -- which is the whole reason the roster stores a scale
		// rather than a distance in metres: the same 1.55/0.95 that frame a 2 m
		// bed frame a 1 m chair.
		{ "chair", "HomeChair", 0u, ZM_PROP_CHAIR,
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
		{ "shelf", "HomeShelf", 0u, ZM_PROP_SHELF,
			/* three-quarter */ {   50.0f, 14.0f, 1.55f },
			/* detail        */ {  115.0f,  8.0f, 0.95f } },

		// ★★ THE BARREL IS THE FIRST SUBJECT WITH NO FACING TO GET RIGHT, and that
		// was MEASURED rather than inferred from the word "barrel" -- see the note
		// on its row in ZM_InteriorDressing.h. Its azimuths are still its own,
		// because where it STANDS is as constraining as ever: it is jammed into the
		// +X/-Z corner at (6.60, -4.60), so the open floor is toward -X and +Z and
		// both lenses swing that way. A positive azimuth here puts the eye in the
		// +X wall, 1.15 m outside the room, and the clamp would then slide it along
		// that wall into a picture of the corner.
		//
		// ★ THE THREE-QUARTER IS THE STEEPEST IN THIS TABLE, at 24 degrees. A
		// barrel's LID is a third of what there is to look at and it is horizontal,
		// so it reads only from above; the detail pose then drops to 14 to rake
		// across the staves and the iron hoops, which are vertical and read only
		// from the side. Same subject, two features, opposite requirements.
		{ "barrel", "HomeBarrel", 0u, ZM_PROP_BARREL,
			/* three-quarter */ {  -55.0f, 24.0f, 1.55f },
			/* detail        */ { -100.0f, 14.0f, 0.95f } },

		// ---- ProfLab ---------------------------------------------------------
		//
		// ★★ BOTH BENCHES ARE PHOTOGRAPHED, AND THE PAIR IS THE POINT. Every
		// earlier asset stands twice in the game and appears once here, because a
		// second picture of the same mesh at the same yaw is the same picture.
		// These two are the first copies of one prop that DISAGREE -- YAW0 on the
		// -X wall, YAW180 on the +X wall (ZM_InteriorDressing.h carries the
		// measurement) -- so one subject would sign off exactly half of that claim,
		// and nothing else in the run looks at the other half.
		//
		// Their azimuths are mirrored for the same reason the shelf's and the
		// table's are: each lens has to be on its OWN bench's open side, which is
		// +X for the west one and -X for the east. Aim either at the other's sign
		// and it photographs a back panel against a wall.
		//
		// ★ THEIR ELEVATIONS SIT BETWEEN THE TABLE'S AND THE SHELF'S, which is
		// again the subject's height: fitted, a bench is 1.34 m tall -- above the
		// waist-high furniture, below the shelf. The detail pose is the SHALLOWER
		// of the two deliberately. What it exists to show is the resin worktop and
		// the steel frame under raking light, and the frame is under the worktop:
		// a steeper lens looks down onto the top and loses it.
		{ "counter_west", "LabCounterWest", 1u, ZM_PROP_COUNTER,
			/* three-quarter */ {   52.0f, 20.0f, 1.55f },
			/* detail        */ {  118.0f, 16.0f, 0.95f } },

		{ "counter_east", "LabCounterEast", 1u, ZM_PROP_COUNTER,
			/* three-quarter */ {  -52.0f, 20.0f, 1.55f },
			/* detail        */ { -118.0f, 16.0f, 0.95f } },

		// ---- Dawnmere, outdoors ----------------------------------------------
		//
		// ★★ THE SAME MESH THE "barrel" ROW ABOVE PHOTOGRAPHS, AND IT IS STILL
		// WORTH A ROW. That one signs off the ASSET; this one signs off the
		// PLACEMENT, which is a different claim and the one nothing else in the
		// run looks at -- Dawnmere is photographed by no other capture. The two
		// questions it answers are whether the barrel sits ON the ground the
		// authoring sampled rather than in or above it, and whether it reads as
		// standing against a wall rather than dropped near one.
		//
		// ★ ITS AZIMUTH IS NEGATIVE, toward -X and +Z of the subject -- which for
		// this barrel (the home's WEST corner, at the -X end of a wall running
		// along X) is the open approach. A positive azimuth puts the eye inside
		// the house, and outdoors NOTHING CLAMPS IT BACK OUT: the room clamp that
		// would have caught it indoors is off for this scene by construction, so
		// the shot would be of the inside of a wall and every assertion would pass.
		{ "barrel_outdoor", "DawnmereHomeBarrelWest", 2u, ZM_PROP_BARREL,
			/* three-quarter */ {  -46.0f, 18.0f, 2.20f },
			/* detail        */ {  -95.0f, 12.0f, 1.20f } },

		// ★★ THE LAMP POST, AB-PROP-07 -- the first subject whose ENTITY OWNS A
		// LIGHT. What these two frames are for is the thing no assertion in this
		// file can make: that the bulb is inside the lantern head and not at the
		// entity origin down by the foot of the post. The numbers are checked
		// elsewhere (ZM_Dressing/PropBulbsSitInsideTheirOwnModel headless, and the
		// authoring logs the resolved world position); these are the picture.
		//
		// ★ ITS ELEVATIONS ARE THE LOWEST IN THE TABLE, for the same reason its
		// distances are among the largest: it is 3 m tall and thin. Every other row
		// looks DOWN on waist-to-chest furniture, and looking down on a lamp post
		// foreshortens it into its own base. 8 and 6 degrees look very slightly up,
		// which is how a street lamp is seen.
		//
		// ★ AND THE AZIMUTHS PUT THE HOUSE BEHIND IT. The post stands off the
		// home's west corner, so open ground is -X and -Z; a lens to the southwest
		// gets the lantern against the sky with the building for scale, which is
		// the composition that shows a 3 m post is 3 m.
		{ "lamppost", "DawnmereHomeLampWest", 2u, ZM_PROP_LAMP_POST,
			/* three-quarter */ { -135.0f, 8.0f, 1.55f },
			/* detail        */ { -110.0f, 6.0f, 0.85f } },
	};
	constexpr u_int uIPS_SUBJECT_COUNT =
		(u_int)(sizeof(axIPS_SUBJECTS) / sizeof(axIPS_SUBJECTS[0]));

	// ---- The room-wide shot, which belongs to no single subject -------------
	//
	// A FIXED pose rather than an offset from a prop: its job is to show a room's
	// imported props at once against the room, and "3 m from the bed" frames the
	// bed. From just inside the doorway, looking down the room's axis. Each room
	// carries its own three numbers in axIPS_ROOMS above.
	//
	// ★★ PLAYERHOME'S DOES NOT HOLD ALL OF THEM, AND IT CANNOT. This comment
	// claimed the "65-degree lens covers both long walls", which was true only
	// while every subject sat within about |x| <= 5.6. MEASURED from that pose:
	// the bed is 33.4 degrees off the view axis, the table 36.4 and the chair
	// 42.7, against a horizontal half-frame of 48.6 degrees (65 vertical at 16:9)
	// -- but the shelf, hard against the -X wall at x = -6.90 and only 4 m deep
	// into the shot, sits at 60.0 degrees and is a clear 11 degrees outside frame.
	//
	// ★ NO AXIAL POSE FIXES THAT, so nothing here was retuned to chase it. The
	// room is 15.5 m wide and 11.5 m deep; seeing x = -6.9 at z = +0.9 from
	// anywhere on the centre line needs a ~114-degree horizontal lens, and moving
	// the eye toward either wall throws the opposite wall's props out instead.
	// Widening the FOV is refused on this file's own terms (NO GRAPHICS OPTION IS
	// TOUCHED, above): the question this shot asks is what a PLAYER sees, and a
	// player standing in the doorway genuinely cannot see the shelf either.
	//
	// So a wide is the CONTEXT shot, not a coverage guarantee. Coverage is the two
	// aimed shots each subject owns, and those are the ones that assert a subject
	// actually projects into frame. A wide checks only that its own look-at point
	// landed where the lens was pointed, which is a self-consistency check on the
	// aiming rather than a claim about what is in the picture.

	// ---- The capture plan ---------------------------------------------------
	//
	// One entry per shot, in the order they are taken, built at Setup from the two
	// tables above. It replaces the index arithmetic this file used while there
	// was exactly one room: with two, neither "which subject is aimed shot N" nor
	// "which scene has to be loaded for it" is derivable from a single integer.
	enum class IPSShotKind
	{
		PlayerView,      // the LIVE follow camera, scene running, nothing overridden
		Unclamped,       // the pose the boom would take with no ceiling clamp
		RoomWide,
		ThreeQuarter,
		Detail,
	};

	struct IPSPlanEntry
	{
		IPSShotKind m_eKind = IPSShotKind::RoomWide;
		u_int       m_uRoom = 0u;      // index into axIPS_ROOMS
		u_int       m_uSubject = 0u;   // index into axIPS_SUBJECTS, for the aimed kinds
	};

	// ★ THE BOUND ASSUMES EVERY ROOM TAKES THE PAIR, and the arithmetic matters
	// because it is currently EXACT. Only PlayerHome sets m_bFollowCameraPair, so
	// the plan is 2 + 2 wides + 12 aimed = 16 -- and "2 + rooms + 2 * subjects" is
	// also 16, so a bound written that way is not a bound at all: turning the pair
	// on for a second room would have written two entries past the end of a
	// fixed-size array, silently. Three per room covers any setting of the flag.
	constexpr u_int uIPS_MAX_SHOTS =
		uIPS_ROOM_COUNT * 3u + uIPS_SUBJECT_COUNT * 2u;

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
		// The floor this subject's authored Y is measured from: 0 indoors, a
		// sampled terrain height outdoors. See the fit clause in IPSMeasureSubject.
		float m_fImpliedGroundY = 0.0f;
		bool m_bGroundIsZero = true;
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
	// Where the run is in the capture plan. g_uIPSShot indexes both the plan and
	// the shot records, so there is one cursor rather than an index and a
	// derivation from it.
	u_int g_uIPSShot = 0u;
	u_int g_uIPSPlanCount = 0u;
	// The room the plan cursor is inside, and the one actually loaded. They differ
	// for exactly the frames between "the plan moved on" and "the new scene has
	// settled", which is what AwaitRoom is waiting for.
	u_int g_uIPSLoadedRoom = 0u;
	bool g_bIPSActive = false;
	bool g_bIPSFailed = false;
	bool g_bIPSScenePaused = false;
	char g_aszIPSDetail[512] = {};
	const char* g_szIPSFailure = "test did not reach verification";

	Zenith_Scene g_xIPSScene;
	Zenith_EntityID g_xIPSCameraID = INVALID_ENTITY_ID;

	IPSPlanEntry g_axIPSPlan[uIPS_MAX_SHOTS];
	IPSSubjectState g_axIPSSubjects[uIPS_SUBJECT_COUNT];
	IPSShotRecord g_axIPSShots[uIPS_MAX_SHOTS];

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

	// Clamp a lens into a room's interior. See the header: a camera outside the
	// wall still PROJECTS the subject into frame, because projection knows nothing
	// about occlusion, so the shot passes every check and shows a blank wall.
	//
	// ★ THE ROOM IS A PARAMETER, and it was PlayerHome's spec inline until this
	// file grew a second room. ProfLab is 20 x 16 x 3.5 m against PlayerHome's
	// 15.5 x 11.5 x 3.0, so the old constant would have clamped every ProfLab eye
	// to a box 2.25 m too narrow and slid each bench shot along an imaginary wall
	// -- silently, since a clamped pose still points at its subject and still
	// projects it into frame.
	Zenith_Maths::Vector3 IPSClampIntoRoom(u_int uRoom,
		const Zenith_Maths::Vector3& xEye)
	{
		// ★ AN OUTDOOR SCENE HAS NOTHING TO CLAMP INTO. Reading a room spec for
		// Dawnmere would fold every eye into a 15.5 x 11.5 m box centred on the
		// town's ORIGIN -- a hundred metres from the subject, underground, and
		// still pointing at it, which is the exact failure the clamp exists to
		// prevent indoors.
		if (!axIPS_ROOMS[uRoom].m_bClampIntoRoom)
		{
			return xEye;
		}
		const ZM_InteriorRoomSpec xRoom =
			ZM_GetInteriorRoomSpec(axIPS_ROOMS[uRoom].m_eRoom);
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

		const Zenith_Maths::Vector3 xEye = IPSClampIntoRoom(
			axIPS_SUBJECTS[uSubject].m_uRoom, xState.m_xWorldCentre + xOffset);
		if (!(glm::length(xState.m_xWorldCentre - xEye) > 1.0e-3f))
		{
			return false;
		}
		IPSPointCameraAt(xCamera, xEye, xState.m_xWorldCentre);
		return true;
	}

	void IPSAimRoomWide(Zenith_CameraComponent& xCamera, u_int uRoom)
	{
		const IPSRoom& xRow = axIPS_ROOMS[uRoom];
		const Zenith_Maths::Vector3 xEye = IPSClampIntoRoom(uRoom,
			Zenith_Maths::Vector3(xRow.m_fWideEyeX, xRow.m_fWideEyeY, xRow.m_fWideEyeZ));
		IPSPointCameraAt(xCamera, xEye, Zenith_Maths::Vector3(
			xRow.m_fWideLookX, xRow.m_fWideLookY, xRow.m_fWideLookZ));
	}

	// The point a shot's in-frame check is made against: a subject's centre for
	// the two aimed poses, and a room wide's own look-at for the rest.
	Zenith_Maths::Vector3 IPSShotTarget(const IPSPlanEntry& xEntry)
	{
		switch (xEntry.m_eKind)
		{
		case IPSShotKind::ThreeQuarter:
		case IPSShotKind::Detail:
			return g_axIPSSubjects[xEntry.m_uSubject].m_xWorldCentre;
		case IPSShotKind::RoomWide:
			return Zenith_Maths::Vector3(
				axIPS_ROOMS[xEntry.m_uRoom].m_fWideLookX,
				axIPS_ROOMS[xEntry.m_uRoom].m_fWideLookY,
				axIPS_ROOMS[xEntry.m_uRoom].m_fWideLookZ);
		default:
			return g_xIPSPivot;   // the follow-camera pair frames the player
		}
	}

	// Park the lens for one plan entry. Returns false only when a subject cannot
	// be framed at all, which the caller turns into a named failure.
	bool IPSAimForPlanEntry(Zenith_CameraComponent& xCamera, const IPSPlanEntry& xEntry)
	{
		switch (xEntry.m_eKind)
		{
		case IPSShotKind::ThreeQuarter:
			return IPSAimAtSubject(xCamera, xEntry.m_uSubject, /*bDetail*/ false);
		case IPSShotKind::Detail:
			return IPSAimAtSubject(xCamera, xEntry.m_uSubject, /*bDetail*/ true);
		case IPSShotKind::RoomWide:
			IPSAimRoomWide(xCamera, xEntry.m_uRoom);
			return true;
		default:
			return true;   // the follow-camera pair is aimed by its own phases
		}
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
		// ★★ THE Y CLAUSE ONLY APPLIES WHERE THE FLOOR IS y = 0. Indoors the
		// authored Y IS the fit's ground lift and asserting it catches a scene
		// authored before the fit existed. Outdoors the lift is added to a terrain
		// height this test cannot re-derive without the heightmap, so the clause
		// would fail every time on a correct placement. The SCALE half applies
		// everywhere and is the half that catches a stale authoring; what replaces
		// the Y assertion is the implied ground, LOGGED below, which a reader can
		// compare against the authoring's own DAWNMERE PROP line.
		xState.m_bGroundIsZero = axIPS_ROOMS[xRow.m_uRoom].m_bGroundIsZero;
		xState.m_fImpliedGroundY = xPosition.y - xExpected.m_fGroundY;
		xState.m_bFitMatches =
			std::fabs(xScale.x - xExpected.m_fScale) <= fIPS_FIT_EPSILON
			&& std::fabs(xScale.y - xExpected.m_fScale) <= fIPS_FIT_EPSILON
			&& std::fabs(xScale.z - xExpected.m_fScale) <= fIPS_FIT_EPSILON
			&& (!xState.m_bGroundIsZero
				|| std::fabs(xPosition.y - xExpected.m_fGroundY) <= fIPS_FIT_EPSILON);
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
			FailIPS("the room's authored camera vanished before a shot");
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

	// Everything a room has to yield before a picture taken in it means anything:
	// it is the settled active scene, it has its authored camera, and every roster
	// subject that lives in it exists.
	//
	// ★ EVERY subject OF THAT ROOM, not just the first. A scene that authored one
	// prop and dropped another would otherwise produce one good picture and one
	// silent nothing, and a reviewer comparing files would never learn which of
	// them had never existed.
	//
	// ★★ AND ONLY THAT ROOM'S. Walking the whole roster here would make PlayerHome
	// wait forever for a bench that is two scenes away -- the exact failure the
	// counter would have hit if it had been added as a plain row.
	bool IPSResolveRoom(u_int uRoom)
	{
		if (uRoom >= uIPS_ROOM_COUNT)
		{
			return false;
		}
		const IPSRoom& xRoomRow = axIPS_ROOMS[uRoom];

		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxData == nullptr)
		{
			return false;
		}
		const int iWanted =
			static_cast<int>(ZM_GetWorldSpec(xRoomRow.m_eScene).m_uBuildIndex);
		if (g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex != iWanted)
		{
			return false;
		}

		const Zenith_Entity xCamera = IPSFindEntity(xRoomRow.m_szCameraName);
		if (!xCamera.IsValid()
			|| xCamera.TryGetComponent<Zenith_CameraComponent>() == nullptr)
		{
			return false;
		}

		for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
		{
			if (axIPS_SUBJECTS[u].m_uRoom != uRoom)
			{
				continue;
			}
			Zenith_Entity xEntity = IPSFindEntity(axIPS_SUBJECTS[u].m_szEntityName);
			if (!xEntity.IsValid())
			{
				return false;
			}
			g_axIPSSubjects[u].m_xEntityID = xEntity.GetEntityID();
		}

		g_xIPSScene = xScene;
		g_xIPSCameraID = xCamera.GetEntityID();
		g_uIPSLoadedRoom = uRoom;
		return true;
	}

	// Register and load one room's scene. PlayerHome is loaded at Setup; this is
	// what moves the run into every LATER room.
	//
	// ★ THE PREVIOUS ROOM IS UNPAUSED FIRST. The aimed shots freeze the scene so
	// ZM_FollowCamera::OnLateUpdate stops overwriting the lens, and a scene left
	// paused as it is torn down leaves the pause flag set against a scene handle
	// this file no longer owns -- Teardown would then unpause whatever had been
	// loaded next.
	void IPSLoadRoom(u_int uRoom)
	{
		if (g_bIPSScenePaused && g_xIPSScene.IsValid())
		{
			g_xEngine.Scenes().SetScenePaused(g_xIPSScene, false);
		}
		g_bIPSScenePaused = false;
		g_xIPSScene = Zenith_Scene();
		g_xIPSCameraID = INVALID_ENTITY_ID;

		const IPSRoom& xRoomRow = axIPS_ROOMS[uRoom];
		const int iIndex =
			static_cast<int>(ZM_GetWorldSpec(xRoomRow.m_eScene).m_uBuildIndex);
		const std::string strPath =
			std::string(GAME_ASSETS_DIR) + "Scenes/" + xRoomRow.m_szSceneStem
			+ ZENITH_SCENE_EXT;
		g_xEngine.Scenes().RegisterSceneBuildIndex(iIndex, strPath.c_str());
		g_xEngine.Scenes().LoadSceneByIndex(iIndex, SCENE_LOAD_SINGLE);
	}
}

//-----------------------------------------------------------------------------

// Append one plan entry, refusing to run off the end of the array rather than
// trusting uIPS_MAX_SHOTS to have been kept in step with the tables.
static bool IPSPushPlan(const IPSPlanEntry& xEntry)
{
	if (g_uIPSPlanCount >= uIPS_MAX_SHOTS)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_ImportedPropShowcase] the capture plan wants more than "
			"uIPS_MAX_SHOTS (%u) entries -- that bound is derived from the room and "
			"subject tables and one of them has outgrown it", uIPS_MAX_SHOTS);
		return false;
	}
	g_axIPSPlan[g_uIPSPlanCount++] = xEntry;
	return true;
}

// Build the capture plan from the two tables, and check the one thing the tables
// cannot express: that the roster is GROUPED by room. Returns false on a roster
// that would reload a scene mid-capture.
static bool IPSBuildPlan()
{
	g_uIPSPlanCount = 0u;
	bool bSeen[uIPS_ROOM_COUNT] = {};

	// A subject naming a room row that does not exist would index off the end of
	// axIPS_ROOMS in every walk below, so it is refused here rather than caught by
	// the reached-the-plan sweep at the bottom.
	for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
	{
		if (axIPS_SUBJECTS[u].m_uRoom >= uIPS_ROOM_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] subject '%s' names room row %u, and "
				"axIPS_ROOMS holds %u", axIPS_SUBJECTS[u].m_szEntityName,
				axIPS_SUBJECTS[u].m_uRoom, uIPS_ROOM_COUNT);
			return false;
		}
	}

	for (u_int uRoom = 0u; uRoom < uIPS_ROOM_COUNT; ++uRoom)
	{
		const IPSRoom& xRoomRow = axIPS_ROOMS[uRoom];

		if (xRoomRow.m_bFollowCameraPair)
		{
			if (!IPSPushPlan({ IPSShotKind::PlayerView, uRoom, 0u })
				|| !IPSPushPlan({ IPSShotKind::Unclamped, uRoom, 0u }))
			{
				return false;
			}
		}
		if (!IPSPushPlan({ IPSShotKind::RoomWide, uRoom, 0u }))
		{
			return false;
		}

		// ★ THE GROUPING CHECK. A row for a room already walked past means the
		// roster interleaves rooms, which this plan would answer by loading that
		// scene a second time -- so the shots taken before it would be of a world
		// that no longer exists, and every one of them would still pass.
		bool bInRun = false;
		for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
		{
			if (axIPS_SUBJECTS[u].m_uRoom != uRoom)
			{
				bInRun = false;   // this room's run of rows, if any, has ended
				continue;
			}
			if (!bInRun && bSeen[uRoom])
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ImportedPropShowcase] subject '%s' resumes a room the roster "
					"had already left. Rows must be GROUPED by room -- see the note at "
					"axIPS_ROOMS.", axIPS_SUBJECTS[u].m_szEntityName);
				return false;
			}
			bInRun = true;
			bSeen[uRoom] = true;

			if (!IPSPushPlan({ IPSShotKind::ThreeQuarter, uRoom, u })
				|| !IPSPushPlan({ IPSShotKind::Detail, uRoom, u }))
			{
				return false;
			}
		}
	}

	// Every subject reached the plan. A row naming a room no table lists would
	// otherwise be silently dropped, which reads exactly like a delivered asset.
	for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
	{
		bool bFound = false;
		for (u_int p = 0u; p < g_uIPSPlanCount; ++p)
		{
			if (g_axIPSPlan[p].m_eKind == IPSShotKind::ThreeQuarter
				&& g_axIPSPlan[p].m_uSubject == u)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] subject '%s' never reached the capture "
				"plan, so it would be authored and never photographed",
				axIPS_SUBJECTS[u].m_szEntityName);
			return false;
		}
	}
	return true;
}

// The filename stem one plan entry writes to.
static std::string IPSShotKey(const IPSPlanEntry& xEntry)
{
	switch (xEntry.m_eKind)
	{
	case IPSShotKind::PlayerView:   return "player_view";
	case IPSShotKind::Unclamped:    return "player_view_unclamped";
	// ★ THE WIDE IS NAMED AFTER ITS ROOM. It was "room_wide" while there was one
	// room; two of them writing that name would leave one file, the second run
	// over the first, and a reviewer looking at a bedroom labelled as a lab.
	case IPSShotKind::RoomWide:
		return std::string("room_wide_") + axIPS_ROOMS[xEntry.m_uRoom].m_szSceneStem;
	case IPSShotKind::ThreeQuarter:
		return std::string(axIPS_SUBJECTS[xEntry.m_uSubject].m_szKey) + "_three_quarter";
	default:
		return std::string(axIPS_SUBJECTS[xEntry.m_uSubject].m_szKey) + "_detail";
	}
}

static void Setup_ZMImportedPropShowcase()
{
	g_eIPSPhase = IPSPhase::AwaitRoom;
	g_iIPSPhaseFrames = 0;
	g_uIPSShot = 0u;
	g_uIPSPlanCount = 0u;
	g_uIPSLoadedRoom = 0u;
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
	for (u_int u = 0u; u < uIPS_MAX_SHOTS; ++u)
	{
		g_axIPSShots[u] = IPSShotRecord();
	}

	g_bIPSActive = true;
	if (!IPSBuildPlan())
	{
		g_uIPSPlanCount = 0u;
		FailIPS("the capture plan could not be built -- see the error above: the "
			"roster is not grouped by room, names a room no table lists, or has "
			"outgrown uIPS_MAX_SHOTS");
		return;
	}

	// ★ STALE SHOTS ARE DELETED, not overwritten-if-lucky. See the header.
	//
	// ★★ EVERY prop_*.tga IN THE DIRECTORY, not just the ones this run is about to
	// write -- which is a wider sweep than it looks and the rename that introduced
	// room_wide_<room> is why. Deleting only the planned names leaves a shot whose
	// FILENAME changed sitting beside its replacement, undated, looking exactly
	// like a fresh capture; "room_wide" survived one run that way. A shot removed
	// from the roster leaves the same orphan. The directory holds captures from
	// other tests too, so the sweep is confined to this file's own prefix.
	const std::filesystem::path xDir = IPSCaptureDir();
	std::error_code xError;
	std::filesystem::create_directories(xDir, xError);

	for (const std::filesystem::directory_entry& xFile :
		std::filesystem::directory_iterator(xDir, xError))
	{
		const std::filesystem::path xPath = xFile.path();
		if (xPath.extension() == ".tga"
			&& xPath.filename().string().rfind("prop_", 0u) == 0u)
		{
			std::filesystem::remove(xPath, xError);
		}
	}

	for (u_int u = 0u; u < g_uIPSPlanCount; ++u)
	{
		g_axIPSShots[u].m_strPath =
			(xDir / ("prop_" + IPSShotKey(g_axIPSPlan[u]) + ".tga")).string();
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fIPS_FIXED_DT);

	IPSLoadRoom(0u);
}

// Measure every subject standing in one room, once, after its settle. See the
// note in IPSMeasureSubject for why this cannot happen at scene-resolve time.
static void IPSMeasureRoom(u_int uRoom)
{
	for (u_int u = 0u; u < uIPS_SUBJECT_COUNT; ++u)
	{
		if (axIPS_SUBJECTS[u].m_uRoom == uRoom)
		{
			IPSMeasureSubject(u);
		}
	}
}

// Advance to the next plan entry, loading a scene when the plan crosses into a
// new room. Returns false when the plan is finished.
static bool IPSAdvanceShot()
{
	++g_uIPSShot;
	if (g_uIPSShot >= g_uIPSPlanCount)
	{
		g_eIPSPhase = IPSPhase::Done;
		return false;
	}

	const IPSPlanEntry& xEntry = g_axIPSPlan[g_uIPSShot];
	// ★★ A ROW IS A POSE, NOT A SCENE, and two rows may share one. Dawnmere wants
	// two context shots -- the house frontage the imported props stand against,
	// and the town's own dressing a hundred metres away -- and neither is the
	// other's job. Comparing SCENES rather than row indices is what makes the
	// second one free: the first reload cost 600 deadline frames plus a settle,
	// and paying that twice for one scene would be paying it for nothing.
	if (axIPS_ROOMS[xEntry.m_uRoom].m_eScene
		!= axIPS_ROOMS[g_uIPSLoadedRoom].m_eScene)
	{
		// ★ BACK TO AwaitRoom, not straight to the shot. The new scene is not
		// there yet: LoadSceneByIndex is a request, and the components that add
		// the models and resolve the camera have not run. Aiming now would park
		// the lens using the PREVIOUS room's measurements.
		IPSLoadRoom(xEntry.m_uRoom);
		g_eIPSPhase = IPSPhase::AwaitRoom;
		g_iIPSPhaseFrames = 0;
		return true;
	}
	// Same scene, different row: nothing to load or settle, and the scene is
	// already frozen. Adopt the row and aim.
	g_uIPSLoadedRoom = xEntry.m_uRoom;

	Zenith_CameraComponent* pxCamera = IPSResolveCamera();
	if (pxCamera == nullptr)
	{
		FailIPS("the authored camera vanished between shots");
		return false;
	}
	if (!IPSAimForPlanEntry(*pxCamera, xEntry))
	{
		std::snprintf(g_aszIPSDetail, sizeof(g_aszIPSDetail),
			"could not aim the lens at '%s' -- its measured world size is "
			"degenerate, so there is nothing to frame",
			axIPS_SUBJECTS[xEntry.m_uSubject].m_szEntityName);
		FailIPS(g_aszIPSDetail);
		return false;
	}
	g_eIPSPhase = IPSPhase::AimedSettle;
	g_iIPSPhaseFrames = 0;
	return true;
}

static bool Step_ZMImportedPropShowcase(int)
{
	if (!g_bIPSActive || g_bIPSFailed || g_eIPSPhase == IPSPhase::Done)
	{
		return false;
	}
	++g_iIPSPhaseFrames;

	const IPSPlanEntry& xEntry = g_axIPSPlan[g_uIPSShot];

	switch (g_eIPSPhase)
	{
	case IPSPhase::AwaitRoom:
	{
		if (!IPSResolveRoom(xEntry.m_uRoom))
		{
			if (g_iIPSPhaseFrames > iIPS_ROOM_DEADLINE_FRAMES)
			{
				std::snprintf(g_aszIPSDetail, sizeof(g_aszIPSDetail),
					"%s never became the settled active scene with its authored "
					"camera '%s' and every roster subject standing in it",
					axIPS_ROOMS[xEntry.m_uRoom].m_szSceneStem,
					axIPS_ROOMS[xEntry.m_uRoom].m_szCameraName);
				FailIPS(g_aszIPSDetail);
				return false;
			}
			return true;
		}

		// ★ THE FIRST ROOM RUNS LIVE FIRST; a later one has nothing to run live
		// FOR, so it settles and goes straight to its aimed shots. The distinction
		// is the room's own m_bFollowCameraPair, not its index.
		if (axIPS_ROOMS[xEntry.m_uRoom].m_bFollowCameraPair)
		{
			g_eIPSPhase = IPSPhase::LiveSettle;
		}
		else
		{
			g_eIPSPhase = IPSPhase::AimedSettle;
		}
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
		IPSMeasureRoom(xEntry.m_uRoom);

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

		if (!IPSTakeShot(g_uIPSShot, g_xIPSPivot, /*bAimed*/ false))
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
		++g_uIPSShot;   // on to the Unclamped entry, in the same room
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
		if (!IPSTakeShot(g_uIPSShot, g_xIPSPivot, /*bAimed*/ false))
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
		return IPSAdvanceShot();
	}

	case IPSPhase::AimedSettle:
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

		// ★ A ROOM ENTERED MID-PLAN IS FROZEN HERE, on its first aimed settle.
		// PlayerHome is paused after its live pair, which a later room does not
		// take -- so without this the benches would be photographed through a
		// running ZM_FollowCamera that overwrites the lens every frame.
		//
		// ★★ AND THEN IT SETTLES AGAIN BEFORE ANYTHING IS CHECKED, which costs 120
		// frames once per such room and buys the only thing that makes the drift
		// clause below mean anything. Pausing, aiming and comparing in ONE frame
		// compares a pose against itself: nothing has had a frame in which to move
		// it, so the check passes on a scene whose pause never took. Every other
		// shot is aimed by IPSAdvanceShot a full settle earlier, and that gap IS
		// the measurement.
		if (!g_bIPSScenePaused)
		{
			g_xEngine.Scenes().SetScenePaused(g_xIPSScene, true);
			g_bIPSScenePaused = true;
			IPSMeasureRoom(xEntry.m_uRoom);
			if (!IPSAimForPlanEntry(*pxCamera, xEntry))
			{
				std::snprintf(g_aszIPSDetail, sizeof(g_aszIPSDetail),
					"could not aim the lens at '%s' -- its measured world size is "
					"degenerate, so there is nothing to frame",
					axIPS_SUBJECTS[xEntry.m_uSubject].m_szEntityName);
				FailIPS(g_aszIPSDetail);
				return false;
			}
			g_iIPSPhaseFrames = 0;
			return true;
		}

		// ★ THE FREEZE IS ASSERTED, NOT ASSUMED: re-aim and re-read, and fail if
		// anything moved the lens while the scene was paused.
		Zenith_Maths::Vector3 xBefore(0.0f);
		pxCamera->GetPosition(xBefore);

		if (!IPSAimForPlanEntry(*pxCamera, xEntry))
		{
			std::snprintf(g_aszIPSDetail, sizeof(g_aszIPSDetail),
				"could not re-aim the lens at '%s' before its shot",
				axIPS_SUBJECTS[xEntry.m_uSubject].m_szEntityName);
			FailIPS(g_aszIPSDetail);
			return false;
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

		if (!IPSTakeShot(g_uIPSShot, IPSShotTarget(xEntry), /*bAimed*/ true))
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
		return IPSAdvanceShot();
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

		if (!xState.m_bGroundIsZero)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_ImportedPropShowcase] OBSERVED '%s' stands on terrain: authored "
				"y %.4f less the fit's %.4f lift implies a ground height of %.4f m "
				"(the authoring's own DAWNMERE PROP line is where to check it)",
				xRow.m_szEntityName, (double)xState.m_fAuthoredGroundY,
				(double)(xState.m_fAuthoredGroundY - xState.m_fImpliedGroundY),
				(double)xState.m_fImpliedGroundY);
		}

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
	//
	// ★ THE PLAN, not a compile-time count. uIPS_MAX_SHOTS is an upper bound
	// (PlayerHome takes the follow-camera pair and ProfLab does not), so walking
	// it would report the unused tail as "never requested" on every green run.
	for (u_int u = 0u; u < g_uIPSPlanCount; ++u)
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
	for (u_int u = 0u; u < g_uIPSPlanCount; ++u)
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
	// A 600-frame room budget PER ROOM, plus (2 + one wide per room + 2 per
	// subject) shots at (120 settle + 12 hold), plus one extra settle for each
	// room entered mid-plan (the freeze needs a frame gap before the drift clause
	// can see anything), with the aimed ones allowed three settles' worth of
	// viewport retries. That is ~3400 frames for the roster as it stands. Every stage owns a deadline that FAILS with a diagnostic; this is
	// only a backstop, and it is deliberately generous enough that adding a roster
	// row -- or a third room, which now costs another 600 up front -- does not
	// silently truncate the run instead of naming what went wrong.
	/* maxFrames */ 9000,
	true /* m_bRequiresGraphics */,
	false /* m_bManualOnly */,
	&Teardown_ZMImportedPropShowcase,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMImportedPropShowcaseTest);

#endif // ZENITH_INPUT_SIMULATOR
