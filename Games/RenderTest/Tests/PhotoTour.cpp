#include "Zenith.h"
#ifdef ZENITH_INPUT_SIMULATOR
// RT_PhotoTour -- the campus, photographed.
//
// Manual-only and graphics-required. Parks a probe camera at a fixed list of
// poses across the RenderTest campus (player portrait + face, the tree grove,
// the campus overview, the material showcase, the tennis court, the horizon),
// lets TAA + auto-exposure settle, and dumps one swapchain TGA per pose into
// Build/artifacts/rendertest/phototour/<tag>/ with a .rect sidecar carrying the
// editor viewport rectangle (Tools/phototour_crop.py cuts the scene out of the
// editor chrome and writes PNGs + a contact sheet).
//
//   rendertest.exe --automated-test RT_PhotoTour --skip-unit-tests \
//       --phototour-tag=baseline [--phototour-settle=150]
//
// Poses around the player are PLAYER-RELATIVE (read from the live transform);
// the campus landmarks are the same constants the authoring uses. Nothing here
// asserts on pixels -- the before/after pair is the deliverable.
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"   // terrain shadow-casting A/B for the tour
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"
#include "RenderTest/RenderTest_Tennis.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"   // m_bViewportOverlaysHidden: no badge/stats in the shots
#endif
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	constexpr float fRT_PT_FIXED_DT = 1.0f / 60.0f;
	constexpr int   iRT_PT_DEFAULT_SETTLE = 150;
	constexpr int   iRT_PT_SCENE_SETTLE = 240;
	constexpr int   iRT_PT_PLAYER_DEADLINE = 1200;
	constexpr int   iRT_PT_POST_DUMP = 3;

	// Campus constants (RenderTest.cpp: fCAMPUS_CX/CZ = 512, fCAMPUS_SHIFT = 256;
	// the grove is planted around (215..325, 255..340) + shift).
	constexpr float fRT_PT_CAMPUS_CX = 512.0f;
	constexpr float fRT_PT_CAMPUS_CZ = 512.0f;
	constexpr float fRT_PT_SHIFT = 256.0f;
	constexpr float fRT_PT_DECK_Y = 48.75f;
	constexpr float fRT_PT_SHOWCASE_CZ = fRT_PT_CAMPUS_CZ + 44.0f;

	struct RTPose
	{
		const char*           m_szName;
		Zenith_Maths::Vector3 m_xEye;
		Zenith_Maths::Vector3 m_xTarget;
		float                 m_fFovDeg;
		bool                  m_bPlayerRelative;   // eye/target are offsets from the player's feet
	};

	const RTPose g_axRTPoses[] = {
		// Player, three-quarter, sunlit side (+X), looking back at the figure.
		{ "rt_player_portrait", { 1.7f, 1.55f, 1.7f }, { 0.0f, 1.0f, 0.0f }, 42.0f, true },
		// Face close-up.
		{ "rt_player_face", { 0.42f, 1.52f, 0.72f }, { 0.0f, 1.45f, 0.0f }, 32.0f, true },
		// Feet on the deck: contact, IK, ground material.
		{ "rt_player_feet", { 1.1f, 0.6f, 1.3f }, { 0.0f, 0.15f, 0.0f }, 45.0f, true },
		// Landscape from the player's eye: terrain, grass, trees, sky.
		{ "rt_landscape_eye", { 0.0f, 1.65f, 0.0f }, { -40.0f, -2.0f, 160.0f }, 65.0f, true },
		// Inside the grove at eye level (the --rendertest-tree-closeup vantage).
		{ "rt_grove_eye", { 213.0f + fRT_PT_SHIFT, 53.5f, 286.0f + fRT_PT_SHIFT },
		  { 213.0f + fRT_PT_SHIFT, 51.7f, 316.0f + fRT_PT_SHIFT }, 60.0f, false },
		// Tree canopy against the sky.
		{ "rt_grove_canopy", { 240.0f + fRT_PT_SHIFT, 51.0f, 300.0f + fRT_PT_SHIFT },
		  { 256.0f + fRT_PT_SHIFT, 62.0f, 340.0f + fRT_PT_SHIFT }, 55.0f, false },
		// Campus overview (the default editor vantage).
		{ "rt_campus_overview", { fRT_PT_CAMPUS_CX, 95.0f, 215.0f + fRT_PT_SHIFT },
		  { fRT_PT_CAMPUS_CX, 53.0f, 283.0f + fRT_PT_SHIFT }, 60.0f, false },
		// Material showcase platform (5 rows, 3.6 m apart, north of the deck).
		{ "rt_material_showcase", { fRT_PT_CAMPUS_CX - 1.0f, fRT_PT_DECK_Y + 2.6f, fRT_PT_SHOWCASE_CZ - 12.0f },
		  { fRT_PT_CAMPUS_CX, fRT_PT_DECK_Y + 0.6f, fRT_PT_SHOWCASE_CZ + 2.0f }, 55.0f, false },
		// Material showcase, one row close.
		{ "rt_material_closeup", { fRT_PT_CAMPUS_CX - 4.0f, fRT_PT_DECK_Y + 1.5f, fRT_PT_SHOWCASE_CZ - 9.5f },
		  { fRT_PT_CAMPUS_CX - 1.5f, fRT_PT_DECK_Y + 0.9f, fRT_PT_SHOWCASE_CZ - 6.4f }, 45.0f, false },
		// Tennis: the spectator overlook.
		{ "rt_tennis_spectator", { RenderTest_Tennis::fCOURT_CX, RenderTest_Tennis::fSURFACE_Y + 16.0f, RenderTest_Tennis::fBASELINE_NEAR_Z - 14.0f },
		  { RenderTest_Tennis::fCOURT_CX, RenderTest_Tennis::fSURFACE_Y + 1.0f, RenderTest_Tennis::fCOURT_CZ }, 60.0f, false },
		// Tennis: courtside, low.
		{ "rt_tennis_courtside", { RenderTest_Tennis::fCOURT_CX - 7.0f, RenderTest_Tennis::fSURFACE_Y + 1.4f, RenderTest_Tennis::fBASELINE_NEAR_Z - 5.0f },
		  { RenderTest_Tennis::fCOURT_CX + 1.0f, RenderTest_Tennis::fSURFACE_Y + 0.8f, RenderTest_Tennis::fCOURT_CZ + 4.0f }, 50.0f, false },
	};
	constexpr u_int uRT_PT_POSE_COUNT = static_cast<u_int>(sizeof(g_axRTPoses) / sizeof(g_axRTPoses[0]));

	// ★ AltFlip / AltRestore are ONE-FRAME phases and they are load-bearing, not
	// bookkeeping. Zenith_AutomatedTestRunner::Tick() runs BEFORE this frame's
	// render, and Flux_Screenshot's pending dump is consumed at EndFrame of the
	// SAME frame -- so a state change made after RTRequestDump in one Update
	// lands in the very frame being captured. Flipping shadows immediately after
	// queuing the base shot put a shadows-OFF frame in the base file and a
	// shadows-ON frame in __noshadow: the A/B was inverted, and the mean-diff
	// magnitude that made it look right is symmetric, so it could not show this.
	// The flip therefore happens one frame LATER than the capture it follows.
	enum class RTPhase { AwaitPlayer, SceneSettle, PoseSettle, AltFlip, AltSettle, AltRestore, PostDump, Done };

	// A/B mode captures the SAME pose twice in ONE run, a settle apart, with terrain
	// shadow casting flipped between them. Two separate runs cannot do this: the
	// wind phase is driven by GetTimeSeconds(), which accumulates from process
	// start, and the boot frame count varies with what a boot has to bake — so the
	// grass sways differently run to run and the noise floor (measured: mean |diff|
	// 0.011-0.032 on grass-heavy poses) is as large as the effect being measured.
	// Within one run the two captures are ~a settle apart, so everything except the
	// toggle is nearly identical.
	constexpr int iRT_PT_ALT_SETTLE = 90;   // TAA history has to reconverge after the flip

	RTPhase         g_eRTPhase = RTPhase::Done;
	u_int           g_uRTPose = 0u;
	int             g_iRTPhaseFrames = 0;
	int             g_iRTSettle = iRT_PT_DEFAULT_SETTLE;
	bool            g_bRTActive = false;
	bool            g_bRTFailed = false;
	const char*     g_szRTFailure = nullptr;
	u_int           g_uRTShots = 0u;
	// Every path handed to RequestDump, checked on DISK in Verify.
	std::vector<std::string> g_axRTShotPaths;
	Zenith_EntityID g_xRTCameraID = INVALID_ENTITY_ID;
	std::string     g_strRTOutDir;
	bool            g_bRTSavedQuads = true;
	bool            g_bRTSavedText = true;
	bool            g_bRTSavedPrimitives = true;
	bool            g_bRTSavedTerrainShadows = true;
	bool            g_bRTSavedAllShadows = true;
	// Which feature the in-run A/B second capture turns OFF. NONE captures one
	// frame per pose; the other two capture a second frame from the SAME pose
	// with exactly that feature disabled, which is the only way to attribute a
	// pixel difference to it (two separate tour runs differ by wind phase alone
	// at 0.9-1.3x the effect being measured). Kept in step with Zenithmon's
	// PTAltMode -- the instrument has to mean the same thing in both games.
	enum class RTAltMode { NONE, TERRAIN_SHADOW, ALL_SHADOWS };
	RTAltMode       g_eRTAltMode = RTAltMode::NONE;

	void RTFail(const char* szWhy)
	{
		if (g_szRTFailure == nullptr)
		{
			g_szRTFailure = szWhy;
		}
		g_bRTFailed = true;
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] FAIL: %s", szWhy);
	}

	const char* RTArg(const char* szPrefix)
	{
#ifdef ZENITH_WINDOWS
		const size_t ulLen = std::strlen(szPrefix);
		for (int i = 1; i < __argc; i++)
		{
			if (std::strncmp(__argv[i], szPrefix, ulLen) == 0)
			{
				return __argv[i] + ulLen;
			}
		}
#else
		(void)szPrefix;
#endif
		return nullptr;
	}

	bool RTPlayerPosition(Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			return false;
		}
		Zenith_Entity xPlayer = pxSceneData->FindEntityByName("Player");
		if (!xPlayer.IsValid())
		{
			return false;
		}
		Zenith_TransformComponent* pxTransform = xPlayer.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xOut);
		return true;
	}

	Zenith_CameraComponent* RTResolveCamera()
	{
		const Zenith_Entity xCamera = g_xEngine.Scenes().ResolveEntity(g_xRTCameraID);
		return xCamera.IsValid() ? xCamera.TryGetComponent<Zenith_CameraComponent>() : nullptr;
	}

	bool RTInstallCamera()
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			return false;
		}
		Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(pxSceneData, "PhotoTourCamera");
		xCamera.SetTransient(true);
		Zenith_CameraComponent& xCam = xCamera.AddComponent<Zenith_CameraComponent>();
		xCam.SetNearPlane(0.1f);
		xCam.SetFarPlane(4000.0f);
		xCam.SetAspectRatio(16.0f / 9.0f);
		g_xRTCameraID = xCamera.GetEntityID();
		Zenith_UnitTests::SetMainCameraForTest(pxSceneData, g_xRTCameraID);
		return true;
	}

	bool RTApplyPose(const RTPose& xPose)
	{
		Zenith_CameraComponent* pxCam = RTResolveCamera();
		if (pxCam == nullptr)
		{
			RTFail("the probe camera disappeared");
			return false;
		}
		Zenith_Maths::Vector3 xEye = xPose.m_xEye;
		Zenith_Maths::Vector3 xTarget = xPose.m_xTarget;
		if (xPose.m_bPlayerRelative)
		{
			Zenith_Maths::Vector3 xPlayer;
			if (!RTPlayerPosition(xPlayer))
			{
				return false;
			}
			xEye += xPlayer;
			xTarget += xPlayer;
		}
		const Zenith_Maths::Vector3 xDir = glm::normalize(xTarget - xEye);
		pxCam->SetPosition(xEye);
		pxCam->SetPitch(std::asin(glm::clamp(xDir.y, -1.0f, 1.0f)));
		pxCam->SetYaw(std::atan2(-xDir.x, xDir.z));
		pxCam->SetFOV(glm::radians(xPose.m_fFovDeg));
#ifdef ZENITH_TOOLS
		if (g_xEditorQuery.m_pfnGetViewportSize != nullptr)
		{
			const Zenith_Maths::Vector2 xSize = g_xEditorQuery.m_pfnGetViewportSize();
			if (xSize.x >= 64.0f && xSize.y >= 64.0f)
			{
				pxCam->SetAspectRatio(xSize.x / xSize.y);
			}
		}
#else
		const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		if (xOpts.m_uWindowWidth != 0u && xOpts.m_uWindowHeight != 0u)
		{
			pxCam->SetAspectRatio(static_cast<float>(xOpts.m_uWindowWidth) / static_cast<float>(xOpts.m_uWindowHeight));
		}
#endif
		return true;
	}

	void RTRequestDump(const RTPose& xPose, const char* szSuffix = "")
	{
		Zenith_Maths::Vector2 xPos(0.0f);
		Zenith_Maths::Vector2 xSize(0.0f);
#ifdef ZENITH_TOOLS
		if (g_xEditorQuery.m_pfnGetViewportPos != nullptr && g_xEditorQuery.m_pfnGetViewportSize != nullptr)
		{
			xPos = g_xEditorQuery.m_pfnGetViewportPos();
			xSize = g_xEditorQuery.m_pfnGetViewportSize();
		}
#else
		const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		xSize = Zenith_Maths::Vector2(static_cast<float>(xOpts.m_uWindowWidth), static_cast<float>(xOpts.m_uWindowHeight));
#endif
		const std::string strTga = g_strRTOutDir + "/" + xPose.m_szName + szSuffix + ".tga";
		const std::string strRect = g_strRTOutDir + "/" + xPose.m_szName + szSuffix + ".rect";
		std::remove(strTga.c_str());
		{
			std::ofstream xRect(strRect);
			xRect << static_cast<int>(xPos.x) << ' ' << static_cast<int>(xPos.y) << ' '
				<< static_cast<int>(xSize.x) << ' ' << static_cast<int>(xSize.y) << '\n';
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] SHOT %s -> %s viewport=%d,%d,%d,%d",
			xPose.m_szName, strTga.c_str(),
			static_cast<int>(xPos.x), static_cast<int>(xPos.y),
			static_cast<int>(xSize.x), static_cast<int>(xSize.y));
		Flux_Screenshot::RequestDump(strTga.c_str());
		++g_uRTShots;
		g_axRTShotPaths.push_back(strTga);
	}

	// The one place the A/B's feature is turned on or off, so the two call sites
	// cannot drift apart on which flag an alt mode owns.
	void RTSetAltFeatureEnabled(bool bEnabled)
	{
		switch (g_eRTAltMode)
		{
		case RTAltMode::TERRAIN_SHADOW: g_xEngine.Terrain().SetCastsShadows(bEnabled); break;
		case RTAltMode::ALL_SHADOWS:    Zenith_GraphicsOptions::Get().m_bShadowsEnabled = bEnabled; break;
		case RTAltMode::NONE:           break;
		}
	}

	void Setup_RTPhotoTour()
	{
		g_eRTPhase = RTPhase::Done;
		g_uRTPose = 0u;
		g_iRTPhaseFrames = 0;
		g_bRTActive = false;
		g_bRTFailed = false;
		g_szRTFailure = nullptr;
		g_uRTShots = 0u;
		g_axRTShotPaths.clear();
		g_xRTCameraID = INVALID_ENTITY_ID;
		g_iRTSettle = iRT_PT_DEFAULT_SETTLE;
		if (const char* szSettle = RTArg("--phototour-settle="))
		{
			const int iSettle = std::atoi(szSettle);
			if (iSettle > 0)
			{
				g_iRTSettle = iSettle;
			}
		}
		const char* szTag = RTArg("--phototour-tag=");
		if (szTag == nullptr || szTag[0] == '\0')
		{
			szTag = "run";
		}

		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		const std::filesystem::path xOut = xRepoRoot / "Build" / "artifacts" / "rendertest" / "phototour" / szTag;
		std::filesystem::create_directories(xOut, xError);
		if (xError)
		{
			RTFail("could not create the phototour output directory");
			return;
		}
		g_strRTOutDir = xOut.string();

		Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		g_bRTSavedQuads = xOpts.m_bQuadsEnabled;
		g_bRTSavedText = xOpts.m_bTextEnabled;
		xOpts.m_bQuadsEnabled = false;
		xOpts.m_bTextEnabled = false;
		g_bRTSavedPrimitives = xOpts.m_bPrimitivesEnabled;
		xOpts.m_bPrimitivesEnabled = false;   // debug/gameplay primitives are not photo content
		// --phototour-terrain-shadows=0 captures the SAME poses with terrain
		// casting disabled, so an A/B pair isolates exactly that feature. The
		// tours are deterministic (fixed dt, derived poses), so two runs pair up.
		g_bRTSavedTerrainShadows = g_xEngine.Terrain().GetCastsShadows();
		g_bRTSavedAllShadows = xOpts.m_bShadowsEnabled;
		g_eRTAltMode = RTAltMode::NONE;
		if (const char* szTerrainShadows = RTArg("--phototour-terrain-shadows="))
		{
			if (std::strcmp(szTerrainShadows, "ab") == 0)
			{
				g_eRTAltMode = RTAltMode::TERRAIN_SHADOW;
				g_xEngine.Terrain().SetCastsShadows(true);
			}
			else
			{
				g_xEngine.Terrain().SetCastsShadows(std::atoi(szTerrainShadows) != 0);
			}
		}
		// --phototour-shadows=ab is the WHOLE shadow system, casters and all: it is
		// what separates "this object casts nothing" from "its shadow is off-frame".
		if (const char* szShadows = RTArg("--phototour-shadows="))
		{
			if (std::strcmp(szShadows, "ab") == 0)
			{
				g_eRTAltMode = RTAltMode::ALL_SHADOWS;
				xOpts.m_bShadowsEnabled = true;
			}
			else
			{
				xOpts.m_bShadowsEnabled = std::atoi(szShadows) != 0;
			}
		}
#ifdef ZENITH_TOOLS
		g_xEngine.Editor().m_xEditorState.m_bViewportOverlaysHidden = true;
#endif

		// Photo mode freezes the player component's per-frame animation writes
		// and ground IK so the figure holds a clean idle for the portraits.
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		RenderTest_GameplayState::s_fPhotoOffsetX = 2.0f;
		RenderTest_GameplayState::s_fPhotoOffsetY = 1.5f;
		RenderTest_GameplayState::s_fPhotoOffsetZ = 2.0f;
		RenderTest_GameplayState::s_fPhotoYaw = 2.3562f;
		RenderTest_GameplayState::s_fPhotoPitch = -0.2f;

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fRT_PT_FIXED_DT);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] tag='%s' settle=%d out=%s", szTag, g_iRTSettle, g_strRTOutDir.c_str());
		g_eRTPhase = RTPhase::AwaitPlayer;
		g_bRTActive = true;
	}

	bool Step_RTPhotoTour(int)
	{
		if (!g_bRTActive || g_bRTFailed || g_eRTPhase == RTPhase::Done)
		{
			return false;
		}
		++g_iRTPhaseFrames;

		switch (g_eRTPhase)
		{
		case RTPhase::AwaitPlayer:
		{
			Zenith_Maths::Vector3 xPlayer;
			if (!RTPlayerPosition(xPlayer))
			{
				if (g_iRTPhaseFrames > iRT_PT_PLAYER_DEADLINE)
				{
					RTFail("no Player entity in the active scene");
					return false;
				}
				return true;
			}
			if (!RTInstallCamera())
			{
				RTFail("could not install the probe camera");
				return false;
			}
			g_eRTPhase = RTPhase::SceneSettle;
			g_iRTPhaseFrames = 0;
			return true;
		}
		case RTPhase::SceneSettle:
		{
			RTApplyPose(g_axRTPoses[0]);
			if (g_iRTPhaseFrames < iRT_PT_SCENE_SETTLE)
			{
				return true;
			}
			g_eRTPhase = RTPhase::PoseSettle;
			g_iRTPhaseFrames = 0;
			return true;
		}
		case RTPhase::PoseSettle:
		{
			if (g_uRTPose >= uRT_PT_POSE_COUNT)
			{
				g_eRTPhase = RTPhase::Done;
				return false;
			}
			const RTPose& xPose = g_axRTPoses[g_uRTPose];
			if (!RTApplyPose(xPose))
			{
				if (g_bRTFailed)
				{
					return false;
				}
				return true;
			}
			if (g_iRTPhaseFrames < g_iRTSettle)
			{
				return true;
			}
			// This frame renders with the feature ON and EndFrame writes the base
			// shot. NOTHING may change render state below this line.
			RTRequestDump(xPose);
			g_eRTPhase = (g_eRTAltMode != RTAltMode::NONE) ? RTPhase::AltFlip : RTPhase::PostDump;
			g_iRTPhaseFrames = 0;
			return true;
		}
		case RTPhase::AltFlip:
		{
			// One frame after the base capture, so the base shot is already written
			// from a frame that still had the feature on.
			RTApplyPose(g_axRTPoses[g_uRTPose]);
			RTSetAltFeatureEnabled(false);
			g_eRTPhase = RTPhase::AltSettle;
			g_iRTPhaseFrames = 0;
			return true;
		}
		case RTPhase::AltSettle:
		{
			RTApplyPose(g_axRTPoses[g_uRTPose]);
			if (g_iRTPhaseFrames < iRT_PT_ALT_SETTLE)
			{
				return true;
			}
			// Same rule: this frame renders with the feature OFF and EndFrame writes
			// the suffixed shot, so the restore waits for AltRestore.
			RTRequestDump(g_axRTPoses[g_uRTPose],
				g_eRTAltMode == RTAltMode::TERRAIN_SHADOW ? "__noterrainshadow" : "__noshadow");
			g_eRTPhase = RTPhase::AltRestore;
			g_iRTPhaseFrames = 0;
			return true;
		}
		case RTPhase::AltRestore:
		{
			RTApplyPose(g_axRTPoses[g_uRTPose]);
			RTSetAltFeatureEnabled(true);
			g_eRTPhase = RTPhase::PostDump;
			g_iRTPhaseFrames = 0;
			return true;
		}
		case RTPhase::PostDump:
		{
			if (g_iRTPhaseFrames < iRT_PT_POST_DUMP)
			{
				return true;
			}
			++g_uRTPose;
			g_eRTPhase = RTPhase::PoseSettle;
			g_iRTPhaseFrames = 0;
			return true;
		}
		case RTPhase::Done:
		default:
			return false;
		}
	}

	bool Verify_RTPhotoTour()
	{
		if (g_bRTFailed)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] FAILED: %s (shots=%u)", g_szRTFailure ? g_szRTFailure : "?", g_uRTShots);
			return false;
		}
		if (g_eRTPhase != RTPhase::Done)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] FAILED: the tour did not finish (pose=%u shots=%u)", g_uRTPose, g_uRTShots);
			return false;
		}
	// ★ A QUEUED DUMP IS NOT A WRITTEN FILE. Flux_Screenshot::RequestDump only sets
	// a pending flag that EndFrame consumes, so the shot counter proves the tour
	// ASKED for N captures, never that N landed. An I/O failure (a full disk, a path
	// the backend could not open, a frame that never reached EndFrame) used to come
	// back as a clean PASS with missing files, and the crop step downstream was
	// equally happy with nothing to crop. Verify against the DISK.
	u_int uMissing = 0u;
	for (const std::string& strPath : g_axRTShotPaths)
	{
		std::error_code xErr;
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xErr);
		if (xErr || ulSize == 0u)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] MISSING CAPTURE: %s (%s)",
				strPath.c_str(), xErr ? xErr.message().c_str() : "zero bytes");
			++uMissing;
		}
	}
	if (uMissing != 0u)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] FAILED: %u of %zu requested captures are missing or empty",
			uMissing, g_axRTShotPaths.size());
		return false;
	}
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RT_PhotoTour] DONE: %u shots in %s", g_uRTShots, g_strRTOutDir.c_str());
		return true;
	}

	void Teardown_RTPhotoTour()
	{
		Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		xOpts.m_bQuadsEnabled = g_bRTSavedQuads;
		xOpts.m_bTextEnabled = g_bRTSavedText;
		xOpts.m_bPrimitivesEnabled = g_bRTSavedPrimitives;
		g_xEngine.Terrain().SetCastsShadows(g_bRTSavedTerrainShadows);
		xOpts.m_bShadowsEnabled = g_bRTSavedAllShadows;
#ifdef ZENITH_TOOLS
		g_xEngine.Editor().m_xEditorState.m_bViewportOverlaysHidden = false;
#endif
		RenderTest_GameplayState::s_bPhotoModeActive = false;
		Zenith_InputSimulator::ClearFixedDt();
		Zenith_InputSimulator::ResetAllInputState();
		g_xRTCameraID = INVALID_ENTITY_ID;
		g_bRTActive = false;
	}

	const Zenith_AutomatedTest g_xRTPhotoTour = {
		"RT_PhotoTour",
		&Setup_RTPhotoTour,
		&Step_RTPhotoTour,
		&Verify_RTPhotoTour,
		/* maxFrames */ iRT_PT_PLAYER_DEADLINE + iRT_PT_SCENE_SETTLE + static_cast<int>(uRT_PT_POSE_COUNT) * 700,
		true /* m_bRequiresGraphics */,
		true /* m_bManualOnly */,
		&Teardown_RTPhotoTour,
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xRTPhotoTour);
}

#endif // ZENITH_INPUT_SIMULATOR
