#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_ShellLighting -- the AS-SHIPPED ambient-lighting pixel probe
// (ZM-D-171).
//
// ZM-D-168's visual audit found every vertical face a player actually looks at
// rendering near-black (the DawnmereHomeShell's sun-averted face measured
// luminance 0.061, red 0.016, blue/red ~7 -- deep blue-black). The root causes
// were engine-side: a sky-only irradiance cube (no ground bounce), a 0.5 IBL
// intensity fudge, and a hand-authored sun decoupled from the atmosphere.
// ZM-D-171 fixed those IN THE ENGINE; this test pins the result ON PIXELS,
// under conditions deliberately AS SHIPPED -- no graphics option is touched,
// auto-exposure stays on, the committed Dawnmere is loaded verbatim, and the
// only scene mutation is a dedicated camera entity.
//
// One swapchain dump, three face patches of the same authored shell in the
// same frame (so exposure and tonemap cancel out of the ratios):
//   * UNLIT-X -- the vertical -X face (sun travels {-0.4,-0.7,-0.55}: N.L<0)
//   * LIT+Z   -- the vertical +Z face (N.L>0)
//   * TOP     -- the up face
// Sample points are DERIVED from the live entity's transform (never respelled
// from the authoring constants) and projected through the test camera; every
// region is mapped through the tools viewport rect (ZM-D-169 convention 3).
//
// The thresholds are centred between the measured healthy state and the
// measured DEFECT state (the ZM-D-168/audit numbers above), so an ambient
// regression -- re-halving IBL, losing the ground bounce, decoupling the sun
// -- pushes the unlit face back toward the audit's blue-black band and reds
// this test. A legitimate lighting re-tune re-derives them from this test's
// own logged numbers, never by nudging until green.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_Screenshot.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"   // ZM-D-173: the shell-relative framing

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "ZM_TestTGAHelpers.h"

namespace
{
	constexpr int iSL_DAWNMERE_BUILD_INDEX = 2;
	constexpr float fSL_FIXED_DT = 1.0f / 60.0f;
	constexpr u_int uSL_SAMPLE_RADIUS = 4u;
	// Auto-exposure runs as shipped; capture only after it has fully adapted
	// (speed 4/s over 2 s of fixed dt leaves e^-8 of the initial error).
	constexpr int iSL_CAPTURE_EARLIEST_FRAME = 120;
	constexpr int iSL_CAPTURE_TIMEOUT_FRAME = 220;
	constexpr int iSL_FINISH_HOLD_FRAMES = 12;

	// --- Thresholds: each carries its measured PASS and FAIL band ------------
	// FAIL states are the ZM-D-168 audit / ZM-D-171 pre-fix probe: unlit-face
	// luminance 0.061 with blue/red ~7.0 and unlit/lit ratio 0.124 as shipped.
	// PASS states: this test's OBSERVED line on the fixed engine (2026-07-30):
	// unlit=0.1672 lit=0.4534 top=0.4699 unlit/lit=0.3688 blueOverRed=1.1283.
	// Thresholds sit between the two bands; a legitimate lighting change
	// re-derives them from a fresh run's logged numbers, never by nudging
	// until green.
	constexpr float fSL_MIN_UNLIT_LUMA = 0.13f;         // fail 0.061 | pass 0.1672
	constexpr float fSL_MIN_UNLIT_TO_LIT = 0.25f;       // fail 0.124 | pass 0.3688
	constexpr float fSL_MAX_UNLIT_TO_LIT = 0.92f;       // ambient must not drown the sun
	constexpr float fSL_MAX_UNLIT_BLUE_OVER_RED = 3.0f; // fail ~7.0 | pass 1.1283

	const char* szSL_SHELL_ENTITY = "DawnmereHomeShell";

	// ★ THE FACES ARE NAMED BY THEIR RELATIONSHIP TO THE SUN, NOT BY AN AXIS.
	// ZM-D-173 moved the Home's entrance from the +Z face to the -Z face, and the
	// shipped sun travels (-0.41, -0.72, -0.56) -- i.e. it comes FROM +X/+Y/+Z, so
	// it lights the +X and +Z faces and averts the -X and -Z ones. The old framing
	// happened to pair an unlit -X face with a lit +Z face; naming them by axis
	// made that coincidence look like a rule, and re-pointing the camera at the new
	// entrance produced TWO sun-averted faces and an unlit/lit ratio of 1.0004.
	// Which axis is lit is now DERIVED from the live sun direction below.
	enum SLFace : u_int
	{
		SL_FACE_UNLIT,
		SL_FACE_LIT,
		SL_FACE_TOP,
		SL_FACE_COUNT
	};
	const char* g_aszSLFaceNames[SL_FACE_COUNT] = { "UNLIT", "LIT", "TOP" };

	// +1 when the sun lights the shell's +X face, -1 when it lights the -X face.
	// The sun TRAVELS along GetSunDir(), so it arrives from the opposite side.
	float SLLitXSign()
	{
		const Zenith_Maths::Vector3 xSunDir = g_xEngine.FluxGraphics().GetSunDir();
		return xSunDir.x < 0.0f ? 1.0f : -1.0f;
	}

	// -1 when the entrance is the shell's MIN-Z face, +1 when it is the MAX-Z face.
	float SLEntranceZSign(float fShellCentreZ)
	{
		return fZM_DAWNMERE_HOME_ENTRANCE_Z < fShellCentreZ ? -1.0f : 1.0f;
	}

	bool g_bSLSceneLoaded = false;
	bool g_bSLFailed = false;
	bool g_bSLShellResolved = false;
	bool g_bSLShotRequested = false;
	const char* g_szSLFailure = "test did not reach verification";
	int g_iSLShotFrame = -1;
	std::string g_strSLShotPath;
	Zenith_Scene g_xSLPreviousScene;
	Zenith_Scene g_xSLDawnmereScene;
	Zenith_EntityID g_xSLCameraID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_axSLFaceWorld[SL_FACE_COUNT] = {};
	Zenith_Maths::Vector2 g_axSLFaceNdc[SL_FACE_COUNT] = {};
	Zenith_Maths::Vector2 g_xSLViewportPos(0.0f);
	Zenith_Maths::Vector2 g_xSLViewportSize(0.0f);

	void SLFail(const char* szReason)
	{
		if (!g_bSLFailed)
		{
			g_szSLFailure = szReason;
		}
		g_bSLFailed = true;
	}

	std::filesystem::path SLVisualAuditDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "visual_audit";
	}

	void SLCleanup()
	{
		Zenith_InputSimulator::ClearFixedDt();
		if (g_xSLPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xSLPreviousScene);
		}
		if (g_xSLDawnmereScene.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(g_xSLDawnmereScene);
			g_xSLDawnmereScene = Zenith_Scene();
		}
		g_bSLSceneLoaded = false;
		g_xSLCameraID = INVALID_ENTITY_ID;
	}

	// Face sample points DERIVED from the live shell transform: centre of each
	// face, pulled toward the camera-visible half so the patch sits well inside
	// the face regardless of small authoring shifts.
	bool SLResolveShellFacePoints()
	{
		Zenith_EntityID xShellID = INVALID_ENTITY_ID;
		g_xEngine.Scenes().QueryActiveScene<Zenith_TransformComponent>().ForEach(
			[&xShellID](Zenith_EntityID xID, Zenith_TransformComponent&)
			{
				if (xShellID != INVALID_ENTITY_ID)
				{
					return;
				}
				const Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
				if (xEntity.IsValid() && xEntity.GetName() == szSL_SHELL_ENTITY)
				{
					xShellID = xID;
				}
			});
		if (xShellID == INVALID_ENTITY_ID)
		{
			return false;
		}

		const Zenith_Entity xShell = g_xEngine.Scenes().ResolveEntity(xShellID);
		Zenith_TransformComponent* pxTransform =
			xShell.IsValid() ? xShell.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxTransform == nullptr)
		{
			return false;
		}

		Zenith_Maths::Vector3 xPos(0.0f);
		Zenith_Maths::Vector3 xScale(1.0f);
		pxTransform->GetPosition(xPos);
		pxTransform->GetScale(xScale);
		const Zenith_Maths::Vector3 xHalf = xScale * 0.5f;

		// The camera sits on the SUN-LIT X side and on the ENTRANCE side in Z (see
		// Setup), so one frame holds a sun-lit vertical face, a sun-averted one and
		// the top. Both signs are DERIVED -- from the live sun and from the authored
		// entrance -- so neither the ZM-D-173 relocation nor a future sun change can
		// leave this probe sampling a face the camera cannot see, or two faces on
		// the same side of the terminator.
		//
		// UNLIT is the ENTRANCE face, which is the one that matters: it is what a
		// player walking up to their own front door actually looks at.
		const float fEntranceSign = SLEntranceZSign(xPos.z);
		const float fLitXSign = SLLitXSign();
		g_axSLFaceWorld[SL_FACE_UNLIT] = Zenith_Maths::Vector3(
			xPos.x + xHalf.x * 0.5f * fLitXSign, xPos.y,
			xPos.z + xHalf.z * fEntranceSign);
		g_axSLFaceWorld[SL_FACE_LIT] = Zenith_Maths::Vector3(
			xPos.x + xHalf.x * fLitXSign, xPos.y,
			xPos.z + xHalf.z * 0.5f * fEntranceSign);
		g_axSLFaceWorld[SL_FACE_TOP] = Zenith_Maths::Vector3(
			xPos.x + xHalf.x * 0.5f * fLitXSign, xPos.y + xHalf.y,
			xPos.z + xHalf.z * 0.5f * fEntranceSign);

		g_bSLShellResolved = true;
		return true;
	}

	Zenith_CameraComponent* SLResolveCamera()
	{
		const Zenith_Entity xCamera = g_xEngine.Scenes().ResolveEntity(g_xSLCameraID);
		return xCamera.IsValid()
			? xCamera.TryGetComponent<Zenith_CameraComponent>()
			: nullptr;
	}

	bool SLProjectFacePoints(Zenith_CameraComponent& xCamera)
	{
		Zenith_Maths::Matrix4 xView;
		Zenith_Maths::Matrix4 xProjection;
		xCamera.BuildViewMatrix(xView);
		xCamera.BuildProjectionMatrix(xProjection);

		for (u_int u = 0u; u < SL_FACE_COUNT; ++u)
		{
			const Zenith_Maths::Vector4 xClip = xProjection * xView
				* Zenith_Maths::Vector4(g_axSLFaceWorld[u], 1.0f);
			if (!std::isfinite(xClip.x) || !std::isfinite(xClip.y)
				|| !std::isfinite(xClip.w) || xClip.w <= 1.0e-4f)
			{
				SLFail("a shell face point did not project in front of the camera");
				return false;
			}
			const float fNdcX = xClip.x / xClip.w;
			const float fNdcY = xClip.y / xClip.w;
			if (fNdcX <= -0.95f || fNdcX >= 0.95f
				|| fNdcY <= -0.95f || fNdcY >= 0.95f)
			{
				SLFail("a shell face point projected outside the safe viewport interior");
				return false;
			}
			g_axSLFaceNdc[u] = Zenith_Maths::Vector2(fNdcX, fNdcY);
		}
		return true;
	}

	bool SLTryRequestCapture(int iFrame)
	{
		Zenith_CameraComponent* pxCamera = SLResolveCamera();
		if (pxCamera == nullptr)
		{
			SLFail("the probe camera disappeared before capture");
			return false;
		}

#ifdef ZENITH_TOOLS
		if (g_xEditorQuery.m_pfnGetViewportPos == nullptr
			|| g_xEditorQuery.m_pfnGetViewportSize == nullptr)
		{
			SLFail("the tools viewport query seam is not installed");
			return false;
		}
		g_xSLViewportPos = g_xEditorQuery.m_pfnGetViewportPos();
		g_xSLViewportSize = g_xEditorQuery.m_pfnGetViewportSize();
		if (g_xSLViewportSize.x < 320.0f || g_xSLViewportSize.y < 180.0f)
		{
			return false; // editor layout has not reached a sampleable size yet
		}
		pxCamera->SetAspectRatio(g_xSLViewportSize.x / g_xSLViewportSize.y);
#else
		const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		if (xOpts.m_uWindowWidth == 0u || xOpts.m_uWindowHeight == 0u)
		{
			return false;
		}
		pxCamera->SetAspectRatio(
			static_cast<float>(xOpts.m_uWindowWidth)
			/ static_cast<float>(xOpts.m_uWindowHeight));
		g_xSLViewportPos = Zenith_Maths::Vector2(0.0f);
		g_xSLViewportSize = Zenith_Maths::Vector2(
			static_cast<float>(xOpts.m_uWindowWidth),
			static_cast<float>(xOpts.m_uWindowHeight));
#endif

		if (!SLProjectFacePoints(*pxCamera))
		{
			return false;
		}

		// METHODOLOGY: every probe run logs the state the engine is actually
		// in, so a lever that silently failed to take is visible, not inferred.
		const Zenith_GraphicsOptions& xLiveOpts = Zenith_GraphicsOptions::Get();
		const Zenith_Maths::Vector4& xSunColour =
			g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunColour_Pad;
		const Zenith_Maths::Vector3 xSunDir = g_xEngine.FluxGraphics().GetSunDir();
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_ShellLighting] engine state: sunDir=(%.3f,%.3f,%.3f) "
			"sunColourRadiance=(%.4f,%.4f,%.4f | %.2f) IBL=%d autoExposure=%d",
			xSunDir.x, xSunDir.y, xSunDir.z,
			xSunColour.x, xSunColour.y, xSunColour.z, xSunColour.w,
			xLiveOpts.m_bIBLEnabled ? 1 : 0,
			xLiveOpts.m_bHDRAutoExposureEnabled ? 1 : 0);

		Flux_Screenshot::RequestDump(g_strSLShotPath.c_str());
		g_bSLShotRequested = true;
		g_iSLShotFrame = iFrame;
		return true;
	}

	bool SLReadMeanRGB(const ZM_TestTGAImage& xImage,
		float fCenterX, float fCenterY, Zenith_Maths::Vector3& xOut)
	{
		if (!xImage.IsValid() || !std::isfinite(fCenterX) || !std::isfinite(fCenterY))
		{
			return false;
		}
		const int64_t iCenterX = static_cast<int64_t>(std::lround(fCenterX));
		const int64_t iCenterY = static_cast<int64_t>(std::lround(fCenterY));
		const int64_t iRadius = static_cast<int64_t>(uSL_SAMPLE_RADIUS);
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

	float SLLuminance(const Zenith_Maths::Vector3& xRGB)
	{
		return 0.2126f * xRGB.x + 0.7152f * xRGB.y + 0.0722f * xRGB.z;
	}
}

static void Setup_ZMShellLighting()
{
	g_bSLSceneLoaded = false;
	g_bSLFailed = false;
	g_bSLShellResolved = false;
	g_bSLShotRequested = false;
	g_szSLFailure = "test did not reach verification";
	g_iSLShotFrame = -1;
	g_strSLShotPath.clear();
	g_xSLPreviousScene = Zenith_Scene();
	g_xSLDawnmereScene = Zenith_Scene();
	g_xSLCameraID = INVALID_ENTITY_ID;
	g_xSLViewportPos = Zenith_Maths::Vector2(0.0f);
	g_xSLViewportSize = Zenith_Maths::Vector2(0.0f);
	for (u_int u = 0u; u < SL_FACE_COUNT; ++u)
	{
		g_axSLFaceWorld[u] = Zenith_Maths::Vector3(0.0f);
		g_axSLFaceNdc[u] = Zenith_Maths::Vector2(0.0f);
	}

	const std::filesystem::path xVisualDir = SLVisualAuditDir();
	std::error_code xDirError;
	std::filesystem::create_directories(xVisualDir, xDirError);
	if (xDirError)
	{
		SLFail("could not create Build/artifacts/zenithmon/visual_audit");
		return;
	}
	g_strSLShotPath = (xVisualDir / "shell_ambient_lighting.tga").string();
	std::remove(g_strSLShotPath.c_str());

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fSL_FIXED_DT);
	// DELIBERATELY NO graphics-option changes: this probe measures the game
	// exactly as shipped, auto-exposure included.

	g_xEngine.Scenes().RegisterSceneBuildIndex(
		iSL_DAWNMERE_BUILD_INDEX,
		GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
	g_xSLPreviousScene = g_xEngine.Scenes().GetActiveScene();
	g_xSLDawnmereScene = g_xEngine.Scenes().LoadSceneByIndex(
		iSL_DAWNMERE_BUILD_INDEX, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxSceneData =
		g_xEngine.Scenes().GetSceneData(g_xSLDawnmereScene);
	if (!g_xSLDawnmereScene.IsValid() || pxSceneData == nullptr
		|| !g_xEngine.Scenes().SetActiveScene(g_xSLDawnmereScene))
	{
		SLFail("could not additively load and activate committed Dawnmere");
		return;
	}
	g_bSLSceneLoaded = true;

	// Camera: off the shell's SUN-LIT X side and above, so ONE frame holds the
	// sun-averted ENTRANCE face, the sun-lit X face, and the top.
	// The aim is computed from the direction (GetFacingDir convention:
	// pitch = asin(dir.y), yaw = atan2(-dir.x, dir.z)).
	//
	// ★ ZM-D-173: BOTH POINTS ARE NOW SHELL-RELATIVE, AND BOTH SIGNS ARE DERIVED.
	// The framing distances are the ones this probe shipped with -- camera
	// (+/-28, +17.56, +/-40), target (0, -0.44, +/-4) from the shell centre. The Z
	// sign follows the ENTRANCE (which moved from the +Z face to the -Z face when
	// the Home was relocated) and the X sign follows the SUN, so the frame always
	// contains one lit and one averted vertical face. Deriving from
	// ZM_GetDawnmereHomeShell() rather than restating absolutes is what stops the
	// next Home move from silently pointing this camera at the back of the
	// building and re-measuring a different face.
	const ZM_DawnmereBlockout xShellPlacement = ZM_GetDawnmereHomeShell();
	const float fSLEntranceSign = SLEntranceZSign(xShellPlacement.m_xCenter.z);
	const float fSLLitXSign = SLLitXSign();
	const Zenith_Maths::Vector3 xCameraPos = xShellPlacement.m_xCenter
		+ Zenith_Maths::Vector3(
			28.0f * fSLLitXSign, 17.559015f, 40.0f * fSLEntranceSign);
	const Zenith_Maths::Vector3 xLookTarget = xShellPlacement.m_xCenter
		+ Zenith_Maths::Vector3(
			0.0f, -0.440985f, 4.0f * fSLEntranceSign);
	const Zenith_Maths::Vector3 xDir = glm::normalize(xLookTarget - xCameraPos);

	Zenith_Entity xCamera =
		g_xEngine.Scenes().CreateEntity(pxSceneData, "ShellLightingProbeCamera");
	Zenith_CameraComponent& xCameraComponent =
		xCamera.AddComponent<Zenith_CameraComponent>();
	xCameraComponent.SetPosition(xCameraPos);
	xCameraComponent.SetPitch(std::asin(xDir.y));
	xCameraComponent.SetYaw(std::atan2(-xDir.x, xDir.z));
	xCameraComponent.SetFOV(glm::radians(55.0f));
	xCameraComponent.SetNearPlane(0.1f);
	xCameraComponent.SetFarPlane(500.0f);
	xCameraComponent.SetAspectRatio(16.0f / 9.0f);
	g_xSLCameraID = xCamera.GetEntityID();
	Zenith_UnitTests::SetMainCameraForTest(pxSceneData, g_xSLCameraID);
}

static bool Step_ZMShellLighting(int iFrame)
{
	if (g_bSLFailed || !g_bSLSceneLoaded)
	{
		return false;
	}

	if (!g_bSLShellResolved)
	{
		SLResolveShellFacePoints();
		if (!g_bSLShellResolved)
		{
			if (iFrame >= iSL_CAPTURE_EARLIEST_FRAME)
			{
				SLFail("the DawnmereHomeShell entity never resolved from the committed scene");
				return false;
			}
			return true;
		}
	}

	if (!g_bSLShotRequested && iFrame >= iSL_CAPTURE_EARLIEST_FRAME)
	{
		SLTryRequestCapture(iFrame);
		if (!g_bSLShotRequested && iFrame >= iSL_CAPTURE_TIMEOUT_FRAME)
		{
			SLFail("the framebuffer capture could not obtain a valid viewport");
			return false;
		}
	}

	return !g_bSLShotRequested
		|| iFrame < g_iSLShotFrame + iSL_FINISH_HOLD_FRAMES;
}

static bool Verify_ZMShellLighting()
{
	bool bPassed = !g_bSLFailed;
	if (g_bSLFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_ShellLighting] %s", g_szSLFailure);
	}
	if (!g_bSLShellResolved || !g_bSLShotRequested)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_ShellLighting] shell/capture incomplete (shell=%s shot=%s)",
			g_bSLShellResolved ? "true" : "false",
			g_bSLShotRequested ? "true" : "false");
		bPassed = false;
	}

	ZM_TestTGAImage xImage;
	if (!g_bSLShotRequested
		|| !ZM_TestLoadTGA(g_strSLShotPath.c_str(), xImage))
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"[ZM_ShellLighting] actual framebuffer TGA missing/invalid: %s",
			g_strSLShotPath.c_str());
		bPassed = false;
	}
	else
	{
#ifndef ZENITH_TOOLS
		g_xSLViewportPos = Zenith_Maths::Vector2(0.0f);
		g_xSLViewportSize = Zenith_Maths::Vector2(
			static_cast<float>(xImage.m_uWidth),
			static_cast<float>(xImage.m_uHeight));
#endif
		Zenith_Maths::Vector3 axFaceRGB[SL_FACE_COUNT] = {};
		bool abSampled[SL_FACE_COUNT] = {};
		for (u_int u = 0u; u < SL_FACE_COUNT; ++u)
		{
			const float fPixelX = g_xSLViewportPos.x
				+ (g_axSLFaceNdc[u].x * 0.5f + 0.5f) * g_xSLViewportSize.x;
			const float fPixelY = g_xSLViewportPos.y
				+ (g_axSLFaceNdc[u].y * 0.5f + 0.5f) * g_xSLViewportSize.y;
			abSampled[u] = SLReadMeanRGB(xImage, fPixelX, fPixelY, axFaceRGB[u]);
			if (!abSampled[u])
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShellLighting] %s patch (%.1f, %.1f) was unreadable",
					g_aszSLFaceNames[u], fPixelX, fPixelY);
				bPassed = false;
				continue;
			}
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_ShellLighting] %s framebuffer RGB=(%.4f, %.4f, %.4f) "
				"luminance=%.4f at (%.1f, %.1f)",
				g_aszSLFaceNames[u],
				axFaceRGB[u].x, axFaceRGB[u].y, axFaceRGB[u].z,
				SLLuminance(axFaceRGB[u]), fPixelX, fPixelY);
		}

		if (abSampled[SL_FACE_UNLIT] && abSampled[SL_FACE_LIT]
			&& abSampled[SL_FACE_TOP])
		{
			const float fUnlit = SLLuminance(axFaceRGB[SL_FACE_UNLIT]);
			const float fLit = SLLuminance(axFaceRGB[SL_FACE_LIT]);
			const float fTop = SLLuminance(axFaceRGB[SL_FACE_TOP]);
			const float fRatio = fUnlit / glm::max(fLit, 1.0e-4f);
			const float fBlueOverRed = axFaceRGB[SL_FACE_UNLIT].z
				/ glm::max(axFaceRGB[SL_FACE_UNLIT].x, 1.0e-4f);
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_ShellLighting] OBSERVED unlit=%.4f lit=%.4f top=%.4f "
				"unlit/lit=%.4f unlitBlueOverRed=%.4f, TGA=%s",
				fUnlit, fLit, fTop, fRatio, fBlueOverRed,
				g_strSLShotPath.c_str());

			if (fUnlit < fSL_MIN_UNLIT_LUMA)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShellLighting] unlit-face luminance %.4f < %.2f -- the "
					"sun-averted face has collapsed back toward the ZM-D-168 "
					"blue-black band (defect state measured 0.061)",
					fUnlit, fSL_MIN_UNLIT_LUMA);
				bPassed = false;
			}
			if (fRatio < fSL_MIN_UNLIT_TO_LIT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShellLighting] unlit/lit ratio %.4f < %.2f -- ambient "
					"fill lost against the sun (defect state measured 0.124)",
					fRatio, fSL_MIN_UNLIT_TO_LIT);
				bPassed = false;
			}
			if (fRatio > fSL_MAX_UNLIT_TO_LIT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShellLighting] unlit/lit ratio %.4f > %.2f -- ambient "
					"has drowned the sun key (flat 'ambient soup')",
					fRatio, fSL_MAX_UNLIT_TO_LIT);
				bPassed = false;
			}
			if (fBlueOverRed > fSL_MAX_UNLIT_BLUE_OVER_RED)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_ShellLighting] unlit-face blue/red %.4f > %.2f -- the "
					"warm ground bounce is missing (sky-only ambient measured ~7)",
					fBlueOverRed, fSL_MAX_UNLIT_BLUE_OVER_RED);
				bPassed = false;
			}
		}
		else
		{
			bPassed = false;
		}
	}

	SLCleanup();
	return bPassed;
}

static void Teardown_ZMShellLighting()
{
	SLCleanup();
	Zenith_InputSimulator::ResetAllInputState();
}

static const Zenith_AutomatedTest g_xZMShellLightingTest = {
	"ZM_ShellLighting_Test",
	&Setup_ZMShellLighting,
	&Step_ZMShellLighting,
	&Verify_ZMShellLighting,
	/* maxFrames */ 300,
	true /* m_bRequiresGraphics */,
	false /* m_bManualOnly */,
	&Teardown_ZMShellLighting,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMShellLightingTest);

#endif // ZENITH_INPUT_SIMULATOR
