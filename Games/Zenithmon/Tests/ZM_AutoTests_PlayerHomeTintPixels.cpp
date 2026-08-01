#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_PlayerHomeTintPixels (ZM-D-176) -- the ONLY test in the game that
// reads a PlayerHome PIXEL.
//
// ★ WHY THIS EXISTS AT ALL, STATED HONESTLY. ZM-D-176 is a VISUAL ruling: the
// player's bedroom must stop reading as the same room as Aster's lab. Every other
// test for it stops at a Zenith_MaterialAsset struct. A tint set on a material
// that is never the material drawn -- or one the tonemapper crushes -- would
// leave the ruling unimplemented with a fully green suite.
//
// ★ AND WHY IT IS NOT THE PRIMARY GATE, ALSO HONESTLY. The material -> framebuffer
// path is shared with every other greybox body and is already pinned windowed by
// ZM_NpcRenderedPalette_Test and ZM_ShellLighting_Test, so this probe mostly
// re-proves shared plumbing. Worse, it is m_bRequiresGraphics = true, and in the
// headless zm-tests gate a graphics skip counts as a PASS -- so this test is
// SILENT exactly where CI looks. ZM_InteriorTint_Test (headless, material-level)
// is the CI-visible proof; this one must be RUN WINDOWED and its numbers
// RECORDED, or it has told nobody anything.
//
// ---- THE MEASUREMENT ------------------------------------------------------
// One swapchain dump per room, of the FLOOR's top face at the same
// proportionally-derived point in each, and the statistic is the patch's
// RED / BLUE. A ratio, not a luminance: exposure is a scalar that largely
// cancels out of a chromaticity ratio, so the two rooms are comparable even
// though each adapted to its own auto-exposure.
//
// Why it can fail, i.e. why it is not decorative: the asserted statistic is a
// DIFFERENCE between the two rooms, so the only way to pass is for the tinted
// room to render measurably warmer than the untinted one. A tint that never
// reaches the framebuffer collapses the gap to ~0 and reds. A tint that LEAKED
// into ProfLab warms both rooms together, also collapsing the gap, and also reds.
// Neither mistake can pass.
//
// ★★ THE RETRACTED PREMISE -- READ THIS BEFORE ADDING AN ABSOLUTE BOUND BACK.
// This test originally ALSO asserted two ABSOLUTE red/blue bounds, reasoning:
// "the shipped blockout grey is COOL (B > G > R), therefore a grey room lands
// BELOW 1, and a tinted room lands ABOVE 1". That reasoning is WRONG at the
// framebuffer, and the first windowed run disproved it: ProfLab's UNTINTED floor
// measured red/blue 1.0742 -- above the 1.0 the bound demanded it stay under.
//
// The bound silently assumed ALBEDO ORDERING SURVIVES TO THE FRAMEBUFFER. It does
// not. A rendered pixel is albedo TIMES illuminant, and under ZM-D-171's
// physically-grounded lighting the illuminant is WARM: a sun key derived from the
// atmosphere transmittance, plus a ground-bounce IBL. A neutral-to-cool albedo
// under a warm illuminant still renders WARM. And this surface gets the full
// force of it -- both rooms are OPEN-TOPPED seven-block shells (floor, four
// walls, lintel; there is NO ceiling block in either placement header), so the
// floor's top face is the MOST sun-exposed surface in the room, N.L ~ 0.72
// against the shipped sun. The 0.887 figure the old bound leaned on came from
// ZM_ShellLighting_Test's SUN-AVERTED, ambient-only face -- the OPPOSITE lighting
// regime -- and never applied to a sun-lit floor at all.
//
// So that bound would have FAILED ON AN UNTINTED ROOM, before ZM-D-176 existed.
// It never encoded a property of the tint; it encoded an unstated assumption
// about the scene's lighting.
//
// ★ ABSOLUTE FRAMEBUFFER RATIOS TRACK THE SCENE'S LIGHTING. ONLY THE RELATIVE
// SEPARATION BETWEEN THE TWO ROOMS IS A PROPERTY OF THE TINT. Pinning an absolute
// ratio makes this test red every time the atmosphere is re-tuned -- a
// maintenance trap, not coverage. Do not add one back.
//
// Every sample point and every camera pose is DERIVED from the two placement
// headers, never re-spelled, so the same fractions frame both rooms and the two
// measurements are comparable by construction rather than by coincidence.
// ============================================================================

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"

#include "ZM_TestTGAHelpers.h"

namespace
{
	constexpr float fPT_FIXED_DT = 1.0f / 60.0f;

	// The ShellLighting patch size, reused so the two probes are talking about
	// the same kind of measurement.
	constexpr u_int uPT_SAMPLE_RADIUS = 4u;

	// Auto-exposure runs AS SHIPPED (no graphics option is touched anywhere in
	// this file). Capture only after it has fully adapted: speed 4/s over 2 s of
	// fixed dt leaves e^-8 of the initial error. Both figures are ShellLighting's
	// measured ones.
	constexpr int iPT_CAPTURE_EARLIEST_FRAME = 120;
	constexpr int iPT_CAPTURE_TIMEOUT_FRAME = 220;
	constexpr int iPT_FINISH_HOLD_FRAMES = 12;
	constexpr int iPT_ROOM_READY_DEADLINE = 240;

	// ---- The framing fractions ----------------------------------------------
	// PROPORTIONS OF EACH ROOM, so PlayerHome (16 x 12 m, 3.0 m walls) and ProfLab
	// (20 x 16 m, 3.5 m walls) are framed the same way despite differing sizes.
	// The camera is aimed exactly AT the sample point, so the patch lands dead
	// centre of frame and no clause depends on a lucky projection.
	constexpr float fPT_CAMERA_HEIGHT_FRACTION = 0.85f;   // of the wall height
	constexpr float fPT_CAMERA_DEPTH_FRACTION = 0.83f;    // of the -Z half depth
	constexpr float fPT_SAMPLE_DEPTH_FRACTION = 0.55f;    // of the +Z half depth
	constexpr float fPT_PANEL_HEIGHT_FRACTION = 0.50f;    // of the wall height

	constexpr float fPT_CAMERA_FOV_DEGREES = 65.0f;   // the shipped interior FOV
	constexpr float fPT_CAMERA_NEAR = 0.1f;
	constexpr float fPT_CAMERA_FAR = 200.0f;

	// ---- The threshold ------------------------------------------------------
	//
	// ★ ONE ASSERTED PROPERTY: THE SEPARATION. See the retracted-premise note in
	// the header block. Absolute framebuffer ratios are a function of the scene's
	// lighting, so the relative gap is the only bound here that says anything
	// about the TINT.
	//
	// CALIBRATED, first windowed run (2026-08-01, on the ZM-D-173 tip): the two
	// floors measured red/blue 1.3045 (PlayerHome) and 1.0742 (ProfLab), an
	// OBSERVED GAP of 0.2303. Note BOTH sit above 1.0 -- that is the measurement
	// that disproved the old absolute bounds and left this one standing.
	//
	// ★ PROVENANCE OF THOSE THREE NUMBERS, BECAUSE THEY ARE NOT THIS PROBE'S OWN.
	// They were measured by the orchestrator OUT OF BAND, off this test's two
	// dumped TGAs, by averaging a floor BAND across the viewport -- not by the
	// 9x9 patch at the projected sample point that the code below actually reads.
	// They are therefore corroborating evidence for the RELATIVE property (the
	// tint renders warmer, by a wide margin) and NOT a baseline for this probe's
	// absolutes. Do not treat a different figure on this test's own OBSERVED line
	// as a regression: expect it, and prefer the OBSERVED line as the authority.
	// The reason they had to be taken out of band at all is recorded as a real
	// shortfall -- the harness swallows the child process's stdout, so a windowed
	// test's Zenith_Log/Zenith_Error output is not readable from
	// Build/artifacts/test_results/..., whose JSON carries "failures": [] even on
	// a red run (first booked at ZM-D-174, hit a second time here). When that is
	// fixed, replace this paragraph with this probe's own printed figures.
	//
	// ★ 0.15 IS KEPT DELIBERATELY -- NEITHER WEAKENED NOR TIGHTENED. It is not
	// weakened because it already passes the observed gap with ~1.5x headroom, so
	// there is nothing to nudge. It is not RAISED to hug 0.2303 either: a floor
	// set just under today's measurement is a tripwire for ordinary lighting
	// drift, and would red this test for a reason that has nothing to do with the
	// tint. A floor this far below the observed gap still reds hard if the two
	// rooms ever render alike, which is the entire point of the clause.
	//
	// SIGNED, so direction is asserted too: the margin is PlayerHome MINUS
	// ProfLab. A lab that somehow rendered warmer than the bedroom goes negative
	// and fails.
	constexpr float fPT_MIN_RATIO_MARGIN = 0.15f;

	// ---- A capture-sanity smoke band, NOT the tint's proof --------------------
	// Applied to BOTH rooms IDENTICALLY. (The retracted pair was asymmetric --
	// "grey below 1, tint above 1" -- which is precisely how it smuggled in an
	// assumption about lighting. A symmetric band cannot.) This asserts only that
	// each patch is a plausible reading off a lit interior floor beneath a warm
	// sun, and is deliberately generous: it is NOT evidence of the tint.
	//
	// It earns its place by closing one real hole in a difference-only check: if a
	// framing regression put ONE room's patch on open sky (strongly BLUE) the gap
	// could widen and pass FALSELY. A sky, black, or otherwise failed patch lands
	// far below this floor. The observed 1.0742 / 1.3045 sit well inside.
	//
	// ★ If a lighting re-tune ever moves a room outside this band, WIDEN THE BAND.
	// Do not touch the margin above -- these bounds track lighting, that one does
	// not.
	constexpr float fPT_MIN_PLAUSIBLE_RED_OVER_BLUE = 0.70f;
	constexpr float fPT_MAX_PLAUSIBLE_RED_OVER_BLUE = 3.00f;

	enum PTRoom : u_int
	{
		PT_ROOM_PLAYERHOME,
		PT_ROOM_PROFLAB,
		PT_ROOM_COUNT
	};
	const char* g_aszPTRoomNames[PT_ROOM_COUNT] = { "PlayerHome", "ProfLab" };

	enum class PTPhase
	{
		AwaitRoom,
		Settle,
		Hold,
		Done,
	};

	struct PTRoomProbe
	{
		bool m_bShotRequested = false;
		bool m_bFloorProjected = false;
		bool m_bPanelProjected = false;
		std::string m_strShotPath;
		Zenith_Maths::Vector2 m_xFloorNdc = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector2 m_xPanelNdc = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector2 m_xViewportPos = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector2 m_xViewportSize = Zenith_Maths::Vector2(0.0f);
		Zenith_Maths::Vector3 m_xFloorWorld = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xPanelWorld = Zenith_Maths::Vector3(0.0f);
	};

	PTPhase g_ePTPhase = PTPhase::Done;
	int g_iPTPhaseFrames = 0;
	u_int g_uPTRoom = PT_ROOM_PLAYERHOME;
	bool g_bPTActive = false;
	bool g_bPTSkipped = false;
	bool g_bPTFailed = false;
	const char* g_szPTFailure = "test did not reach verification";
	Zenith_EntityID g_xPTCameraID = INVALID_ENTITY_ID;
	PTRoomProbe g_axPTRooms[PT_ROOM_COUNT];

	void PTFail(const char* szReason)
	{
		if (!g_bPTFailed)
		{
			g_szPTFailure = szReason;
		}
		g_bPTFailed = true;
		g_ePTPhase = PTPhase::Done;
	}

	std::filesystem::path PTVisualAuditDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "visual_audit";
	}

	bool PTSceneFilePresent(const char* szPath)
	{
		std::error_code xError;
		return std::filesystem::is_regular_file(szPath, xError) && !xError
			&& std::filesystem::file_size(szPath, xError) > 0u && !xError;
	}

	u_int PTBuildIndex(u_int uRoom)
	{
		return ZM_GetWorldSpec(
			uRoom == PT_ROOM_PLAYERHOME ? ZM_SCENE_PLAYERHOME : ZM_SCENE_PROFLAB)
			.m_uBuildIndex;
	}

	// ---- The two rooms' geometry, behind ONE shape ---------------------------
	// Both placement headers expose the same accessors over the same block enum,
	// but they are DISTINCT types, so the sample derivation is written once here
	// against a small per-room summary rather than duplicated per room.
	struct PTRoomGeometry
	{
		Zenith_Maths::Vector3 m_xFloorMin = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xFloorMax = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xPanelCentre = Zenith_Maths::Vector3(0.0f);
		float m_fPanelInnerZ = 0.0f;
		float m_fWallHeight = 0.0f;
	};

	PTRoomGeometry PTResolveGeometry(u_int uRoom)
	{
		PTRoomGeometry xOut;
		if (uRoom == PT_ROOM_PLAYERHOME)
		{
			const ZM_PlayerHomeBlockout xFloor =
				ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FLOOR);
			const ZM_PlayerHomeBlockout xPanel =
				ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FRONT_LEFT);
			xOut.m_xFloorMin = xFloor.Min();
			xOut.m_xFloorMax = xFloor.Max();
			xOut.m_xPanelCentre = xPanel.m_xCenter;
			xOut.m_fPanelInnerZ = xPanel.Min().z;
			xOut.m_fWallHeight = fZM_PLAYERHOME_WALL_HEIGHT;
			return xOut;
		}
		const ZM_ProfLabBlockout xFloor =
			ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR);
		const ZM_ProfLabBlockout xPanel =
			ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FRONT_LEFT);
		xOut.m_xFloorMin = xFloor.Min();
		xOut.m_xFloorMax = xFloor.Max();
		xOut.m_xPanelCentre = xPanel.m_xCenter;
		xOut.m_fPanelInnerZ = xPanel.Min().z;
		xOut.m_fWallHeight = fZM_PROFLAB_WALL_HEIGHT;
		return xOut;
	}

	// THE PRIMARY SAMPLE: the floor's top face, on the room's centreline, at the
	// same proportional depth in both rooms. The floor is chosen over a wall
	// because it is the surface that fills the frame a player actually looks at,
	// and because both rooms present it identically.
	//
	// The authored Player capsule stands near this column but renders NOTHING
	// (PlayerHome authors it with a collider and ZM_PlayerController only -- no
	// model, no greybox visual), so there is no body between the camera and the
	// patch. That is why the centreline is usable at all.
	Zenith_Maths::Vector3 PTFloorSamplePoint(const PTRoomGeometry& xGeo)
	{
		return Zenith_Maths::Vector3(
			0.0f,
			xGeo.m_xFloorMax.y,
			xGeo.m_xFloorMax.z * fPT_SAMPLE_DEPTH_FRACTION);
	}

	// THE SECONDARY, LOGGED-ONLY SAMPLE: the inner face of the -X doorway stub.
	// Sun-averted, so it is lit by ambient alone -- useful context for a session
	// re-deriving the margin, and deliberately NOT asserted on: it may not project
	// into the safe viewport interior in every framing, and a logged-only sample
	// that occasionally goes unread costs nothing.
	Zenith_Maths::Vector3 PTPanelSamplePoint(const PTRoomGeometry& xGeo)
	{
		return Zenith_Maths::Vector3(
			xGeo.m_xPanelCentre.x,
			xGeo.m_xFloorMax.y + xGeo.m_fWallHeight * fPT_PANEL_HEIGHT_FRACTION,
			xGeo.m_fPanelInnerZ);
	}

	// The camera: on the centreline, behind the sample toward -Z, at a fraction of
	// the wall height, aimed exactly at the floor patch. Same fractions in both
	// rooms.
	Zenith_Maths::Vector3 PTCameraPoint(const PTRoomGeometry& xGeo)
	{
		return Zenith_Maths::Vector3(
			0.0f,
			xGeo.m_xFloorMax.y + xGeo.m_fWallHeight * fPT_CAMERA_HEIGHT_FRACTION,
			xGeo.m_xFloorMin.z * fPT_CAMERA_DEPTH_FRACTION);
	}

	Zenith_CameraComponent* PTResolveCamera()
	{
		const Zenith_Entity xCamera = g_xEngine.Scenes().ResolveEntity(g_xPTCameraID);
		return xCamera.IsValid()
			? xCamera.TryGetComponent<Zenith_CameraComponent>()
			: nullptr;
	}

	// Project a world point and reject anything that is behind the camera or
	// outside the safe viewport interior, so a patch is never read off the frame
	// edge where the tonemapper's vignette and the editor chrome live.
	bool PTProjectPoint(Zenith_CameraComponent& xCamera,
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
		if (fNdcX <= -0.95f || fNdcX >= 0.95f
			|| fNdcY <= -0.95f || fNdcY >= 0.95f)
		{
			return false;
		}
		xNdcOut = Zenith_Maths::Vector2(fNdcX, fNdcY);
		return true;
	}

	bool PTReadMeanRGB(const ZM_TestTGAImage& xImage,
		float fCenterX, float fCenterY, Zenith_Maths::Vector3& xOut)
	{
		if (!xImage.IsValid() || !std::isfinite(fCenterX) || !std::isfinite(fCenterY))
		{
			return false;
		}
		const int64_t iCenterX = static_cast<int64_t>(std::lround(fCenterX));
		const int64_t iCenterY = static_cast<int64_t>(std::lround(fCenterY));
		const int64_t iRadius = static_cast<int64_t>(uPT_SAMPLE_RADIUS);
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

	float PTRedOverBlue(const Zenith_Maths::Vector3& xRGB)
	{
		return xRGB.x / (xRGB.z > 1.0e-4f ? xRGB.z : 1.0e-4f);
	}

	// Install the probe camera in the room that just became active, and derive
	// both sample points from that room's placement header.
	bool PTInstallProbeCamera(u_int uRoom)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			return false;
		}

		const PTRoomGeometry xGeo = PTResolveGeometry(uRoom);
		PTRoomProbe& xProbe = g_axPTRooms[uRoom];
		xProbe.m_xFloorWorld = PTFloorSamplePoint(xGeo);
		xProbe.m_xPanelWorld = PTPanelSamplePoint(xGeo);

		const Zenith_Maths::Vector3 xCameraPos = PTCameraPoint(xGeo);
		const Zenith_Maths::Vector3 xAim = xProbe.m_xFloorWorld - xCameraPos;
		const float fAimLength = std::sqrt(
			xAim.x * xAim.x + xAim.y * xAim.y + xAim.z * xAim.z);
		if (!(fAimLength > 1.0e-3f))
		{
			return false;
		}
		const Zenith_Maths::Vector3 xDir = xAim / fAimLength;

		Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
			pxSceneData, "InteriorTintProbeCamera");
		Zenith_CameraComponent& xCameraComponent =
			xCamera.AddComponent<Zenith_CameraComponent>();
		xCameraComponent.SetPosition(xCameraPos);
		// GetFacingDir convention: pitch = asin(dir.y), yaw = atan2(-dir.x, dir.z).
		xCameraComponent.SetPitch(std::asin(xDir.y));
		xCameraComponent.SetYaw(std::atan2(-xDir.x, xDir.z));
		xCameraComponent.SetFOV(glm::radians(fPT_CAMERA_FOV_DEGREES));
		xCameraComponent.SetNearPlane(fPT_CAMERA_NEAR);
		xCameraComponent.SetFarPlane(fPT_CAMERA_FAR);
		xCameraComponent.SetAspectRatio(16.0f / 9.0f);
		g_xPTCameraID = xCamera.GetEntityID();
		Zenith_UnitTests::SetMainCameraForTest(pxSceneData, g_xPTCameraID);
		return true;
	}

	bool PTTryRequestCapture(u_int uRoom)
	{
		Zenith_CameraComponent* pxCamera = PTResolveCamera();
		if (pxCamera == nullptr)
		{
			PTFail("the probe camera disappeared before capture");
			return false;
		}
		PTRoomProbe& xProbe = g_axPTRooms[uRoom];

#ifdef ZENITH_TOOLS
		if (g_xEditorQuery.m_pfnGetViewportPos == nullptr
			|| g_xEditorQuery.m_pfnGetViewportSize == nullptr)
		{
			PTFail("the tools viewport query seam is not installed");
			return false;
		}
		xProbe.m_xViewportPos = g_xEditorQuery.m_pfnGetViewportPos();
		xProbe.m_xViewportSize = g_xEditorQuery.m_pfnGetViewportSize();
		if (xProbe.m_xViewportSize.x < 320.0f || xProbe.m_xViewportSize.y < 180.0f)
		{
			return false;   // editor layout has not reached a sampleable size yet
		}
		pxCamera->SetAspectRatio(
			xProbe.m_xViewportSize.x / xProbe.m_xViewportSize.y);
#else
		const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		if (xOpts.m_uWindowWidth == 0u || xOpts.m_uWindowHeight == 0u)
		{
			return false;
		}
		pxCamera->SetAspectRatio(
			static_cast<float>(xOpts.m_uWindowWidth)
			/ static_cast<float>(xOpts.m_uWindowHeight));
		xProbe.m_xViewportPos = Zenith_Maths::Vector2(0.0f);
		xProbe.m_xViewportSize = Zenith_Maths::Vector2(
			static_cast<float>(xOpts.m_uWindowWidth),
			static_cast<float>(xOpts.m_uWindowHeight));
#endif

		xProbe.m_bFloorProjected =
			PTProjectPoint(*pxCamera, xProbe.m_xFloorWorld, xProbe.m_xFloorNdc);
		if (!xProbe.m_bFloorProjected)
		{
			PTFail("the floor sample point did not project into the safe viewport "
				"interior -- the derived camera framing is wrong for this room");
			return false;
		}
		// Best-effort only: a secondary sample that will not project is logged as
		// absent rather than failing the run.
		xProbe.m_bPanelProjected =
			PTProjectPoint(*pxCamera, xProbe.m_xPanelWorld, xProbe.m_xPanelNdc);

		// METHODOLOGY: log the state the engine is actually in, so a lever that
		// silently failed to take is visible rather than inferred.
		const Zenith_GraphicsOptions& xLiveOpts = Zenith_GraphicsOptions::Get();
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_InteriorTintPixels] %s capture: camera aimed at "
			"(%.3f, %.3f, %.3f), IBL=%d autoExposure=%d, TGA=%s",
			g_aszPTRoomNames[uRoom],
			xProbe.m_xFloorWorld.x, xProbe.m_xFloorWorld.y, xProbe.m_xFloorWorld.z,
			xLiveOpts.m_bIBLEnabled ? 1 : 0,
			xLiveOpts.m_bHDRAutoExposureEnabled ? 1 : 0,
			xProbe.m_strShotPath.c_str());

		Flux_Screenshot::RequestDump(xProbe.m_strShotPath.c_str());
		xProbe.m_bShotRequested = true;
		return true;
	}

	bool PTRoomIsActive(u_int uRoom)
	{
		return g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex
				== static_cast<int>(PTBuildIndex(uRoom))
			&& g_xEngine.Physics().HasActiveSimulation();
	}
}

static void Setup_ZMInteriorTintPixels()
{
	g_ePTPhase = PTPhase::Done;
	g_iPTPhaseFrames = 0;
	g_uPTRoom = PT_ROOM_PLAYERHOME;
	g_bPTActive = false;
	g_bPTSkipped = false;
	g_bPTFailed = false;
	g_szPTFailure = "test did not reach verification";
	g_xPTCameraID = INVALID_ENTITY_ID;
	for (u_int u = 0u; u < PT_ROOM_COUNT; ++u)
	{
		g_axPTRooms[u] = PTRoomProbe{};
	}

	const std::string strPlayerHome =
		std::string(GAME_ASSETS_DIR) + "Scenes/PlayerHome" + ZENITH_SCENE_EXT;
	const std::string strProfLab =
		std::string(GAME_ASSETS_DIR) + "Scenes/ProfLab" + ZENITH_SCENE_EXT;
	if (!PTSceneFilePresent(strPlayerHome.c_str())
		|| !PTSceneFilePresent(strProfLab.c_str()))
	{
		g_bPTSkipped = true;
		Zenith_AutomatedTestRunner::RequestSkip(
			"[ZM_InteriorTintPixels] PlayerHome/ProfLab scene files are absent");
		return;
	}

	const std::filesystem::path xVisualDir = PTVisualAuditDir();
	std::error_code xDirError;
	std::filesystem::create_directories(xVisualDir, xDirError);
	if (xDirError)
	{
		PTFail("could not create Build/artifacts/zenithmon/visual_audit");
		g_bPTActive = true;
		return;
	}
	g_axPTRooms[PT_ROOM_PLAYERHOME].m_strShotPath =
		(xVisualDir / "playerhome_interior_tint.tga").string();
	g_axPTRooms[PT_ROOM_PROFLAB].m_strShotPath =
		(xVisualDir / "proflab_interior_grey.tga").string();
	for (u_int u = 0u; u < PT_ROOM_COUNT; ++u)
	{
		std::remove(g_axPTRooms[u].m_strShotPath.c_str());
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fPT_FIXED_DT);
	// DELIBERATELY NO graphics-option changes: this probe measures the game
	// exactly as shipped, auto-exposure included.

	g_xEngine.Scenes().RegisterSceneBuildIndex(
		static_cast<int>(PTBuildIndex(PT_ROOM_PLAYERHOME)),
		GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(
		static_cast<int>(PTBuildIndex(PT_ROOM_PROFLAB)),
		GAME_ASSETS_DIR "Scenes/ProfLab" ZENITH_SCENE_EXT);

	g_xEngine.Scenes().LoadSceneByIndex(
		static_cast<int>(PTBuildIndex(PT_ROOM_PLAYERHOME)), SCENE_LOAD_SINGLE);
	g_ePTPhase = PTPhase::AwaitRoom;
	g_bPTActive = true;
}

static bool Step_ZMInteriorTintPixels(int)
{
	if (!g_bPTActive || g_bPTFailed || g_ePTPhase == PTPhase::Done)
	{
		return false;
	}
	++g_iPTPhaseFrames;

	switch (g_ePTPhase)
	{
	case PTPhase::AwaitRoom:
		if (PTRoomIsActive(g_uPTRoom))
		{
			if (!PTInstallProbeCamera(g_uPTRoom))
			{
				PTFail("could not install the probe camera in the loaded interior");
				return false;
			}
			g_ePTPhase = PTPhase::Settle;
			g_iPTPhaseFrames = 0;
			return true;
		}
		if (g_iPTPhaseFrames > iPT_ROOM_READY_DEADLINE)
		{
			PTFail("an interior never became the settled active scene");
			return false;
		}
		return true;

	case PTPhase::Settle:
		if (g_iPTPhaseFrames >= iPT_CAPTURE_EARLIEST_FRAME)
		{
			PTTryRequestCapture(g_uPTRoom);
			if (g_bPTFailed)
			{
				return false;
			}
			if (g_axPTRooms[g_uPTRoom].m_bShotRequested)
			{
				g_ePTPhase = PTPhase::Hold;
				g_iPTPhaseFrames = 0;
				return true;
			}
		}
		if (g_iPTPhaseFrames > iPT_CAPTURE_TIMEOUT_FRAME)
		{
			PTFail("the framebuffer capture could not obtain a valid viewport");
			return false;
		}
		return true;

	case PTPhase::Hold:
		if (g_iPTPhaseFrames < iPT_FINISH_HOLD_FRAMES)
		{
			return true;
		}
		if (g_uPTRoom == PT_ROOM_PLAYERHOME)
		{
			g_uPTRoom = PT_ROOM_PROFLAB;
			g_xPTCameraID = INVALID_ENTITY_ID;
			g_xEngine.Scenes().LoadSceneByIndex(
				static_cast<int>(PTBuildIndex(PT_ROOM_PROFLAB)), SCENE_LOAD_SINGLE);
			g_ePTPhase = PTPhase::AwaitRoom;
			g_iPTPhaseFrames = 0;
			return true;
		}
		g_ePTPhase = PTPhase::Done;
		return false;

	case PTPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ZMInteriorTintPixels()
{
	if (g_bPTSkipped)
	{
		return true;
	}

	bool bPassed = !g_bPTFailed;
	if (g_bPTFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_InteriorTintPixels] %s", g_szPTFailure);
	}

	float afRedOverBlue[PT_ROOM_COUNT] = { 0.0f, 0.0f };
	bool abSampled[PT_ROOM_COUNT] = { false, false };

	for (u_int u = 0u; u < PT_ROOM_COUNT; ++u)
	{
		PTRoomProbe& xProbe = g_axPTRooms[u];
		ZM_TestTGAImage xImage;
		if (!xProbe.m_bShotRequested
			|| !ZM_TestLoadTGA(xProbe.m_strShotPath.c_str(), xImage))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTintPixels] %s framebuffer TGA missing/invalid: %s",
				g_aszPTRoomNames[u], xProbe.m_strShotPath.c_str());
			bPassed = false;
			continue;
		}

#ifndef ZENITH_TOOLS
		xProbe.m_xViewportPos = Zenith_Maths::Vector2(0.0f);
		xProbe.m_xViewportSize = Zenith_Maths::Vector2(
			static_cast<float>(xImage.m_uWidth),
			static_cast<float>(xImage.m_uHeight));
#endif

		const float fFloorX = xProbe.m_xViewportPos.x
			+ (xProbe.m_xFloorNdc.x * 0.5f + 0.5f) * xProbe.m_xViewportSize.x;
		const float fFloorY = xProbe.m_xViewportPos.y
			+ (xProbe.m_xFloorNdc.y * 0.5f + 0.5f) * xProbe.m_xViewportSize.y;
		Zenith_Maths::Vector3 xFloorRGB(0.0f);
		if (!PTReadMeanRGB(xImage, fFloorX, fFloorY, xFloorRGB))
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTintPixels] %s floor patch (%.1f, %.1f) was unreadable",
				g_aszPTRoomNames[u], fFloorX, fFloorY);
			bPassed = false;
			continue;
		}
		afRedOverBlue[u] = PTRedOverBlue(xFloorRGB);
		abSampled[u] = true;

		// The secondary, ambient-only face. LOGGED, never asserted.
		Zenith_Maths::Vector3 xPanelRGB(0.0f);
		bool bPanelRead = false;
		if (xProbe.m_bPanelProjected)
		{
			const float fPanelX = xProbe.m_xViewportPos.x
				+ (xProbe.m_xPanelNdc.x * 0.5f + 0.5f) * xProbe.m_xViewportSize.x;
			const float fPanelY = xProbe.m_xViewportPos.y
				+ (xProbe.m_xPanelNdc.y * 0.5f + 0.5f) * xProbe.m_xViewportSize.y;
			bPanelRead = PTReadMeanRGB(xImage, fPanelX, fPanelY, xPanelRGB);
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_InteriorTintPixels] %s floor RGB=(%.4f, %.4f, %.4f) "
			"red/blue=%.4f at (%.1f, %.1f); panel %s RGB=(%.4f, %.4f, %.4f) "
			"red/blue=%.4f",
			g_aszPTRoomNames[u], xFloorRGB.x, xFloorRGB.y, xFloorRGB.z,
			afRedOverBlue[u], fFloorX, fFloorY,
			bPanelRead ? "read" : "UNREAD",
			xPanelRGB.x, xPanelRGB.y, xPanelRGB.z, PTRedOverBlue(xPanelRGB));
	}

	if (abSampled[PT_ROOM_PLAYERHOME] && abSampled[PT_ROOM_PROFLAB])
	{
		const float fTint = afRedOverBlue[PT_ROOM_PLAYERHOME];
		const float fGrey = afRedOverBlue[PT_ROOM_PROFLAB];
		const float fMargin = fTint - fGrey;

		// ★ THE LINE THAT MADE THE FIRST RUN DIAGNOSABLE IN ONE PASS -- KEEP IT.
		// Both rooms' absolute ratios, the measured gap, and the asserted floor,
		// on one line. The absolutes are logged as CONTEXT ONLY (they move with
		// the scene's lighting and nothing here asserts them); the gap is the
		// number that carries the ruling.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_InteriorTintPixels] OBSERVED PlayerHome red/blue=%.4f "
			"ProfLab red/blue=%.4f gap=%.4f (asserted floor %.2f; first-run "
			"calibration was 1.3045 / 1.0742 gap 0.2303). The absolutes are "
			"CONTEXT, not a threshold -- they track the scene's lighting. If the "
			"two bands OVERLAP, the suite cannot see the tint on pixels: RECORD "
			"THAT, do NOT shrink the floor until it passes",
			fTint, fGrey, fMargin, fPT_MIN_RATIO_MARGIN);

		// THE RULING, and the only clause that speaks about the tint.
		if (fMargin < fPT_MIN_RATIO_MARGIN)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTintPixels] the two interiors' rendered red/blue "
				"ratios differ by only %.4f (floor %.2f): PlayerHome %.4f vs "
				"ProfLab %.4f. On PIXELS the two rooms still read the same, which "
				"is the complaint ZM-D-176 exists to answer. Causes, in order of "
				"likelihood: the tint never reached the framebuffer (gap ~0), or "
				"it LEAKED into ProfLab and warmed both rooms together (gap ~0 "
				"with both absolutes high), or the bedroom is somehow the COOLER "
				"room (gap NEGATIVE). Note what this is NOT: a change to the "
				"scene's lighting moves BOTH absolutes together and leaves this "
				"gap intact, so an atmosphere re-tune does not reach this clause",
				fMargin, fPT_MIN_RATIO_MARGIN, fTint, fGrey);
			bPassed = false;
		}

		// SMOKE ONLY -- see the band's declaration. This is not the tint's proof;
		// it catches a patch that never landed on a lit floor at all (open sky is
		// strongly blue, a failed capture reads ~0), which a difference-only check
		// can otherwise absorb into a wider gap and pass falsely.
		for (u_int u = 0u; u < PT_ROOM_COUNT; ++u)
		{
			const float fRatio = afRedOverBlue[u];
			if (fRatio < fPT_MIN_PLAUSIBLE_RED_OVER_BLUE
				|| fRatio > fPT_MAX_PLAUSIBLE_RED_OVER_BLUE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_InteriorTintPixels] %s's floor reads red/blue %.4f, "
					"outside the plausible band %.2f-%.2f. This is a CAPTURE "
					"sanity check, not a tint bound: a reading this far out means "
					"the patch is probably not on a lit interior floor at all "
					"(open sky reads strongly blue, a failed capture ~0). Confirm "
					"the framing from the dumped TGA before touching anything -- "
					"and if the lighting was legitimately re-tuned, WIDEN THIS "
					"BAND rather than the margin",
					g_aszPTRoomNames[u], fRatio,
					fPT_MIN_PLAUSIBLE_RED_OVER_BLUE,
					fPT_MAX_PLAUSIBLE_RED_OVER_BLUE);
				bPassed = false;
			}
		}
	}
	else
	{
		bPassed = false;
	}

	return bPassed;
}

static void Teardown_ZMInteriorTintPixels()
{
	if (g_bPTActive)
	{
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
	}
	Zenith_InputSimulator::ClearFixedDt();
	Zenith_InputSimulator::ResetAllInputState();
	g_xPTCameraID = INVALID_ENTITY_ID;
	g_bPTActive = false;
}

static const Zenith_AutomatedTest g_xZMInteriorTintPixelsTest = {
	"ZM_InteriorTintPixels_Test",
	&Setup_ZMInteriorTintPixels,
	&Step_ZMInteriorTintPixels,
	&Verify_ZMInteriorTintPixels,
	// Two rooms, each a 240-frame ready budget + a 220-frame capture budget + a
	// 12-frame hold, with headroom so a named phase deadline rather than this
	// backstop diagnoses an ordinary stall.
	/* maxFrames */ 1100,
	true /* m_bRequiresGraphics */,
	false /* m_bManualOnly */,
	&Teardown_ZMInteriorTintPixels,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMInteriorTintPixelsTest);

#endif // ZENITH_INPUT_SIMULATOR
