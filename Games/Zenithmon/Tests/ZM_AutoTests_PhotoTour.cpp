#include "Zenith.h"
#ifdef ZENITH_INPUT_SIMULATOR
// ZM_PhotoTour -- the game's LOOK, photographed.
//
// Manual-only (no per-commit signal, minutes long) and graphics-required. Walks
// Dawnmere, PlayerHome and ProfLab, parks a probe camera at a fixed list of
// poses, lets TAA + auto-exposure settle, and dumps one swapchain TGA per pose
// into Build/artifacts/zenithmon/phototour/<tag>/. Beside every TGA it writes a
// .rect sidecar holding the editor viewport rectangle, so Tools/phototour_crop.py
// can cut the scene out of the editor chrome and save PNGs + a contact sheet.
//
//   zenithmon.exe --automated-test ZM_PhotoTour_Test --skip-unit-tests \
//       --phototour-tag=baseline [--phototour-settle=150]
//
// Every pose is derived from the placement headers (shell centres, room
// extents, NPC anchors) and the LIVE terrain height, never from an absolute
// world Y, so a layout move re-frames the shot instead of burying the camera.
//
// Nothing here asserts on pixels: two identical runs before and after a
// renderer change are the whole point, and the judgement is a person's.
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"   // terrain shadow-casting A/B for the tour
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"
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
	constexpr float fPT_FIXED_DT = 1.0f / 60.0f;
	constexpr int   iPT_DEFAULT_SETTLE = 150;    // frames per pose before the dump
	constexpr int   iPT_SCENE_SETTLE = 360;      // extra frames after a scene becomes active (streaming, IBL)
	constexpr int   iPT_SCENE_DEADLINE = 3000;   // frames a scene may take to become active
	constexpr int   iPT_POST_DUMP = 3;           // frames between the dump request and the next pose

	enum PTScene : u_int { PT_SCENE_DAWNMERE = 0u, PT_SCENE_PLAYERHOME, PT_SCENE_PROFLAB, PT_SCENE_COUNT };

	struct PTPose
	{
		const char*           m_szName;
		Zenith_Maths::Vector3 m_xEye;      // world; Y is ADDED to the sampled ground height when m_bGroundRelative
		Zenith_Maths::Vector3 m_xTarget;   // world; same rule
		float                 m_fFovDeg;
		bool                  m_bGroundRelative;
	};

	// ---- Dawnmere poses: shell-relative + ground-relative --------------------
	// Ground Y is sampled at the EYE's XZ from the live terrain, and the target's
	// Y is sampled at ITS OWN XZ, so a shot across a slope still frames the ground.
	PTPose g_axPTDawnmere[8];
	u_int  g_uPTDawnmereCount = 0u;

	void PTBuildDawnmerePoses()
	{
		const ZM_DawnmereBlockout xHome = ZM_GetDawnmereHomeShell();
		const ZM_DawnmereBlockout xLab = ZM_GetDawnmereLabShell();
		const float fHomeX = xHome.m_xCenter.x;
		const float fHomeFrontZ = xHome.Min().z;            // the -Z entrance face
		const float fLabX = xLab.m_xCenter.x;
		const float fLabFrontZ = xLab.Min().z;
		const float fTownX = fZM_DAWNMERE_TOWN_CENTER_X;
		const float fTownZ = fZM_DAWNMERE_TOWN_CENTER_Z;

		u_int u = 0u;
		// 1. Walking up to the front door of home, eye height.
		g_axPTDawnmere[u++] = { "dawnmere_home_approach",
			{ fHomeX - 2.0f, 1.65f, fHomeFrontZ - 13.0f },
			{ fHomeX, 2.4f, fHomeFrontZ + 2.0f }, 60.0f, true };
		// 2. Aster's lab from the street.
		g_axPTDawnmere[u++] = { "dawnmere_lab_approach",
			{ fLabX + 3.0f, 1.65f, fLabFrontZ - 16.0f },
			{ fLabX, 3.0f, fLabFrontZ + 3.0f }, 60.0f, true };
		// 3. Street wide: both buildings in one frame from the town centre.
		g_axPTDawnmere[u++] = { "dawnmere_street_wide",
			{ fTownX - 12.0f, 1.65f, fTownZ + 6.0f },
			{ fTownX + 8.0f, 2.0f, fTownZ + 44.0f }, 70.0f, true };
		// 4. Establishing shot: elevated, looking over the whole town.
		g_axPTDawnmere[u++] = { "dawnmere_establishing",
			{ fTownX, 16.0f, fTownZ - 34.0f },
			{ fTownX, 0.0f, fTownZ + 36.0f }, 60.0f, true };
		// 5. Rival Vesper at conversational distance.
		g_axPTDawnmere[u++] = { "dawnmere_npc_eye",
			{ fZM_DAWNMERE_VESPER_X + 2.6f, 1.55f, fZM_DAWNMERE_VESPER_Z - 3.4f },
			{ fZM_DAWNMERE_VESPER_X, 1.15f, fZM_DAWNMERE_VESPER_Z }, 45.0f, true };
		// 6. Ground detail: grass, path, terrain material at a metre.
		g_axPTDawnmere[u++] = { "dawnmere_ground_detail",
			{ fTownX - 10.0f, 1.1f, fTownZ + 2.0f },
			{ fTownX - 7.0f, 0.0f, fTownZ + 5.5f }, 60.0f, true };
		// 7. Toward the horizon / sky: the atmosphere and the far terrain.
		g_axPTDawnmere[u++] = { "dawnmere_horizon",
			{ fTownX + 20.0f, 1.65f, fTownZ - 10.0f },
			{ fTownX + 120.0f, 12.0f, fTownZ - 180.0f }, 65.0f, true };
		// 8. Sun-averted side of home: ambient/GI quality, shadowed wall + ground.
		g_axPTDawnmere[u++] = { "dawnmere_home_shade",
			{ xHome.Min().x - 9.0f, 1.65f, xHome.m_xCenter.z + 4.0f },
			{ xHome.Min().x, 1.5f, xHome.m_xCenter.z - 2.0f }, 60.0f, true };
		g_uPTDawnmereCount = u;
	}

	// ---- Interiors ---------------------------------------------------------
	PTPose g_axPTHome[3];
	PTPose g_axPTLab[4];

	void PTBuildInteriorPoses()
	{
		{
			const ZM_PlayerHomeBlockout xFloor = ZM_GetPlayerHomeBlock(ZM_PLAYERHOME_BLOCK_FLOOR);
			const float fHW = xFloor.HalfExtent().x;
			const float fHD = xFloor.HalfExtent().z;
			const float fTop = xFloor.Max().y;
			const float fH = fZM_PLAYERHOME_WALL_HEIGHT;
			g_axPTHome[0] = { "home_interior_wide",
				{ 0.0f, fTop + fH * 0.55f, -fHD * 0.86f }, { 0.0f, fTop + 0.6f, fHD * 0.5f }, 65.0f, false };
			g_axPTHome[1] = { "home_interior_corner",
				{ fHW * 0.82f, fTop + 1.6f, fHD * 0.82f }, { -fHW * 0.45f, fTop + 0.7f, -fHD * 0.55f }, 62.0f, false };
			g_axPTHome[2] = { "home_interior_detail",
				{ -fHW * 0.2f, fTop + 1.3f, fHD * 0.1f }, { -fHW * 0.8f, fTop + 0.5f, -fHD * 0.6f }, 50.0f, false };
		}
		{
			const ZM_ProfLabBlockout xFloor = ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR);
			const float fHW = xFloor.HalfExtent().x;
			const float fHD = xFloor.HalfExtent().z;
			const float fTop = xFloor.Max().y;
			const float fH = fZM_PROFLAB_WALL_HEIGHT;
			g_axPTLab[0] = { "lab_interior_wide",
				{ 0.0f, fTop + fH * 0.55f, -fHD * 0.86f }, { 0.0f, fTop + 0.6f, fHD * 0.5f }, 65.0f, false };
			g_axPTLab[1] = { "lab_interior_corner",
				{ fHW * 0.82f, fTop + 1.6f, fHD * 0.82f }, { -fHW * 0.45f, fTop + 0.7f, -fHD * 0.55f }, 62.0f, false };
			g_axPTLab[2] = { "lab_aster",
				{ fZM_PROFLAB_ASTER_X + 1.4f, fTop + 1.55f, fZM_PROFLAB_ASTER_Z + 2.4f },
				{ fZM_PROFLAB_ASTER_X, fTop + 1.25f, fZM_PROFLAB_ASTER_Z }, 42.0f, false };
			g_axPTLab[3] = { "lab_interior_detail",
				{ -fHW * 0.3f, fTop + 1.2f, 0.0f }, { -fHW * 0.85f, fTop + 0.8f, -fHD * 0.7f }, 50.0f, false };
		}
	}

	// ---- run state -----------------------------------------------------------
	// ★ AltFlip / AltRestore are ONE-FRAME phases and they are load-bearing.
	// Zenith_AutomatedTestRunner::Tick() runs BEFORE this frame's render and
	// Flux_Screenshot's pending dump is consumed at EndFrame of the SAME frame,
	// so a render-state change made after PTRequestDump in one Update lands in
	// the very frame being captured. Flipping shadows straight after queuing the
	// base shot put a shadows-OFF frame in the base file and a shadows-ON frame
	// in __noshadow -- the pair was inverted, and the mean-diff magnitude that
	// made it look right is symmetric, so no measurement could reveal it.
	enum class PTPhase { LoadScene, AwaitScene, SceneSettle, PoseSettle, AltFlip, AltSettle, AltRestore, PostDump, Done };

	// A/B mode captures the SAME pose twice in ONE run, a settle apart, with terrain
	// shadow casting flipped between them. Two separate runs cannot do this: the
	// wind phase is driven by GetTimeSeconds(), which accumulates from process
	// start, and the boot frame count varies with what a boot has to bake — so the
	// grass sways differently run to run and the noise floor is as large as the
	// effect. Within one run the two captures are ~a settle apart.
	constexpr int iPT_ALT_SETTLE = 90;   // TAA history has to reconverge after the flip

	PTPhase         g_ePTPhase = PTPhase::Done;
	u_int           g_uPTScene = 0u;
	u_int           g_uPTPose = 0u;
	int             g_iPTPhaseFrames = 0;
	int             g_iPTSettle = iPT_DEFAULT_SETTLE;
	bool            g_bPTActive = false;
	bool            g_bPTFailed = false;
	bool            g_bPTSkipped = false;
	const char*     g_szPTFailure = nullptr;
	u_int           g_uPTShots = 0u;
	// Every path handed to RequestDump, checked on DISK in Verify.
	std::vector<std::string> g_axPTShotPaths;
	Zenith_EntityID g_xPTCameraID = INVALID_ENTITY_ID;
	std::string     g_strPTOutDir;
	bool            g_bPTSavedQuads = true;
	bool            g_bPTSavedText = true;
	bool            g_bPTSavedPrimitives = true;
	bool            g_bPTSavedTerrainShadows = true;
	bool            g_bPTSavedAllShadows = true;
	// Which feature the in-run A/B second capture turns OFF. NONE captures one
	// frame per pose; the other two capture a second frame from the SAME pose
	// with exactly that feature disabled, which is the only way to attribute a
	// pixel difference to it (two separate tour runs differ by wind phase alone
	// at 0.9-1.3x the effect being measured).
	enum class PTAltMode { NONE, TERRAIN_SHADOW, ALL_SHADOWS };
	PTAltMode       g_ePTAltMode = PTAltMode::NONE;

	void PTFail(const char* szWhy)
	{
		if (g_szPTFailure == nullptr)
		{
			g_szPTFailure = szWhy;
		}
		g_bPTFailed = true;
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] FAIL: %s", szWhy);
	}

	const char* PTArg(const char* szPrefix)
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

	ZM_SCENE_ID PTSceneId(u_int uScene)
	{
		switch (uScene)
		{
		case PT_SCENE_DAWNMERE:   return ZM_SCENE_DAWNMERE;
		case PT_SCENE_PLAYERHOME: return ZM_SCENE_PLAYERHOME;
		default:                  return ZM_SCENE_PROFLAB;
		}
	}
	const char* PTSceneStem(u_int uScene)
	{
		switch (uScene)
		{
		case PT_SCENE_DAWNMERE:   return "Dawnmere";
		case PT_SCENE_PLAYERHOME: return "PlayerHome";
		default:                  return "ProfLab";
		}
	}
	int PTBuildIndex(u_int uScene)
	{
		return static_cast<int>(ZM_GetWorldSpec(PTSceneId(uScene)).m_uBuildIndex);
	}
	const PTPose* PTPoses(u_int uScene, u_int& uCountOut)
	{
		switch (uScene)
		{
		case PT_SCENE_DAWNMERE:   uCountOut = g_uPTDawnmereCount; return g_axPTDawnmere;
		case PT_SCENE_PLAYERHOME: uCountOut = 3u; return g_axPTHome;
		default:                  uCountOut = 4u; return g_axPTLab;
		}
	}

	bool PTSceneIsActive(u_int uScene)
	{
		return g_xEngine.Scenes().GetActiveScene().IsValid()
			&& g_xEngine.Scenes().GetSceneInfo(g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex == PTBuildIndex(uScene)
			&& g_xEngine.Physics().HasActiveSimulation();
	}

	// Live terrain height at (x, z), or false when no terrain has streamed a
	// height there yet.
	bool PTGroundAt(float fX, float fZ, float& fOut)
	{
		bool bFound = false;
		float fBest = 0.0f;
		g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
			[&](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
			{
				float fH = 0.0f;
				if (!bFound && xTerrain.TryGetGroundHeightAt(fX, fZ, fH))
				{
					bFound = true;
					fBest = fH;
				}
			});
		fOut = fBest;
		return bFound;
	}

	Zenith_CameraComponent* PTResolveCamera()
	{
		const Zenith_Entity xCamera = g_xEngine.Scenes().ResolveEntity(g_xPTCameraID);
		return xCamera.IsValid() ? xCamera.TryGetComponent<Zenith_CameraComponent>() : nullptr;
	}

	bool PTInstallCamera()
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
		xCam.SetFarPlane(2000.0f);
		xCam.SetAspectRatio(16.0f / 9.0f);
		g_xPTCameraID = xCamera.GetEntityID();
		Zenith_UnitTests::SetMainCameraForTest(pxSceneData, g_xPTCameraID);
		return true;
	}

	// Aims the probe camera at the pose. Returns false while a ground-relative
	// pose cannot be resolved yet (terrain not streamed).
	bool PTApplyPose(const PTPose& xPose)
	{
		Zenith_CameraComponent* pxCam = PTResolveCamera();
		if (pxCam == nullptr)
		{
			PTFail("the probe camera disappeared");
			return false;
		}
		Zenith_Maths::Vector3 xEye = xPose.m_xEye;
		Zenith_Maths::Vector3 xTarget = xPose.m_xTarget;
		if (xPose.m_bGroundRelative)
		{
			float fEyeGround = 0.0f;
			float fTargetGround = 0.0f;
			if (!PTGroundAt(xEye.x, xEye.z, fEyeGround))
			{
				return false;
			}
			// A target past the terrain edge (the horizon shot) borrows the eye's
			// ground height rather than stalling the tour.
			if (!PTGroundAt(xTarget.x, xTarget.z, fTargetGround))
			{
				fTargetGround = fEyeGround;
			}
			xEye.y += fEyeGround;
			xTarget.y += fTargetGround;
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

	// The one place the A/B's feature is turned on or off, so the call sites cannot
	// drift apart on which flag an alt mode owns.
	void PTSetAltFeatureEnabled(bool bEnabled)
	{
		switch (g_ePTAltMode)
		{
		case PTAltMode::TERRAIN_SHADOW: g_xEngine.Terrain().SetCastsShadows(bEnabled); break;
		case PTAltMode::ALL_SHADOWS:    Zenith_GraphicsOptions::Get().m_bShadowsEnabled = bEnabled; break;
		case PTAltMode::NONE:           break;
		}
	}

	// The alt phases hold the camera still; the pose list is scene-indexed, so this
	// is the guarded re-apply all three of them share.
	void PTReapplyCurrentPose()
	{
		u_int uCount = 0u;
		const PTPose* pxPoses = PTPoses(g_uPTScene, uCount);
		if (g_uPTPose < uCount)
		{
			PTApplyPose(pxPoses[g_uPTPose]);
		}
	}

	void PTRequestDump(const PTPose& xPose, const char* szSuffix = "")
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
		const std::string strTga = g_strPTOutDir + "/" + xPose.m_szName + szSuffix + ".tga";
		const std::string strRect = g_strPTOutDir + "/" + xPose.m_szName + szSuffix + ".rect";
		std::remove(strTga.c_str());
		{
			std::ofstream xRect(strRect);
			xRect << static_cast<int>(xPos.x) << ' ' << static_cast<int>(xPos.y) << ' '
				<< static_cast<int>(xSize.x) << ' ' << static_cast<int>(xSize.y) << '\n';
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] SHOT %s -> %s viewport=%d,%d,%d,%d",
			xPose.m_szName, strTga.c_str(),
			static_cast<int>(xPos.x), static_cast<int>(xPos.y),
			static_cast<int>(xSize.x), static_cast<int>(xSize.y));
		Flux_Screenshot::RequestDump(strTga.c_str());
		++g_uPTShots;
		g_axPTShotPaths.push_back(strTga);
	}
}

static void Setup_ZMPhotoTour()
{
	g_ePTPhase = PTPhase::Done;
	g_uPTScene = 0u;
	g_uPTPose = 0u;
	g_iPTPhaseFrames = 0;
	g_bPTActive = false;
	g_bPTFailed = false;
	g_bPTSkipped = false;
	g_szPTFailure = nullptr;
	g_uPTShots = 0u;
	g_axPTShotPaths.clear();
	g_xPTCameraID = INVALID_ENTITY_ID;
	g_iPTSettle = iPT_DEFAULT_SETTLE;
	if (const char* szSettle = PTArg("--phototour-settle="))
	{
		const int iSettle = std::atoi(szSettle);
		if (iSettle > 0)
		{
			g_iPTSettle = iSettle;
		}
	}
	const char* szTag = PTArg("--phototour-tag=");
	if (szTag == nullptr || szTag[0] == '\0')
	{
		szTag = "run";
	}

	std::error_code xError;
	const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
		std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
	const std::filesystem::path xOut = xRepoRoot / "Build" / "artifacts" / "zenithmon" / "phototour" / szTag;
	std::filesystem::create_directories(xOut, xError);
	if (xError)
	{
		PTFail("could not create the phototour output directory");
		return;
	}
	g_strPTOutDir = xOut.string();

	for (u_int u = 0u; u < PT_SCENE_COUNT; ++u)
	{
		const std::string strScene = std::string(GAME_ASSETS_DIR) + "Scenes/" + PTSceneStem(u) + ZENITH_SCENE_EXT;
		if (!std::filesystem::exists(strScene, xError))
		{
			g_bPTSkipped = true;
			Zenith_AutomatedTestRunner::RequestSkip("[ZM_PhotoTour] a scene file is absent (fresh clone, no bake)");
			return;
		}
	}

	PTBuildDawnmerePoses();
	PTBuildInteriorPoses();

	// The tour photographs the WORLD; the HUD/menus would sit on top of every shot.
	Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	g_bPTSavedQuads = xOpts.m_bQuadsEnabled;
	g_bPTSavedText = xOpts.m_bTextEnabled;
	xOpts.m_bQuadsEnabled = false;
	xOpts.m_bTextEnabled = false;
	g_bPTSavedPrimitives = xOpts.m_bPrimitivesEnabled;
	xOpts.m_bPrimitivesEnabled = false;   // debug/gameplay primitives are not photo content
	// --phototour-terrain-shadows=0 captures the SAME poses with terrain
	// casting disabled, so an A/B pair isolates exactly that feature. The
	// tours are deterministic (fixed dt, derived poses), so two runs pair up.
	g_bPTSavedTerrainShadows = g_xEngine.Terrain().GetCastsShadows();
	g_bPTSavedAllShadows = xOpts.m_bShadowsEnabled;
	g_ePTAltMode = PTAltMode::NONE;
	if (const char* szTerrainShadows = PTArg("--phototour-terrain-shadows="))
	{
		if (std::strcmp(szTerrainShadows, "ab") == 0)
		{
			g_ePTAltMode = PTAltMode::TERRAIN_SHADOW;
			g_xEngine.Terrain().SetCastsShadows(true);
		}
		else
		{
			g_xEngine.Terrain().SetCastsShadows(std::atoi(szTerrainShadows) != 0);
		}
	}
	// --phototour-shadows=ab is the WHOLE shadow system, casters and all: it is
	// what separates "this object casts nothing" from "its shadow is off-frame".
	if (const char* szShadows = PTArg("--phototour-shadows="))
	{
		if (std::strcmp(szShadows, "ab") == 0)
		{
			g_ePTAltMode = PTAltMode::ALL_SHADOWS;
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

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fPT_FIXED_DT);

	for (u_int u = 0u; u < PT_SCENE_COUNT; ++u)
	{
		const std::string strScene = std::string(GAME_ASSETS_DIR) + "Scenes/" + PTSceneStem(u) + ZENITH_SCENE_EXT;
		g_xEngine.Scenes().RegisterSceneBuildIndex(PTBuildIndex(u), strScene);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] tag='%s' settle=%d out=%s", szTag, g_iPTSettle, g_strPTOutDir.c_str());
	g_ePTPhase = PTPhase::LoadScene;
	g_bPTActive = true;
}

static bool Step_ZMPhotoTour(int)
{
	if (!g_bPTActive || g_bPTFailed || g_bPTSkipped || g_ePTPhase == PTPhase::Done)
	{
		return false;
	}
	++g_iPTPhaseFrames;

	switch (g_ePTPhase)
	{
	case PTPhase::LoadScene:
	{
		if (g_uPTScene >= PT_SCENE_COUNT)
		{
			g_ePTPhase = PTPhase::Done;
			return false;
		}
		g_xPTCameraID = INVALID_ENTITY_ID;
		g_xEngine.Scenes().LoadSceneByIndex(PTBuildIndex(g_uPTScene), SCENE_LOAD_SINGLE);
		g_ePTPhase = PTPhase::AwaitScene;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::AwaitScene:
	{
		if (!PTSceneIsActive(g_uPTScene))
		{
			if (g_iPTPhaseFrames > iPT_SCENE_DEADLINE)
			{
				PTFail("a scene never became the settled active scene");
				return false;
			}
			return true;
		}
		if (!PTInstallCamera())
		{
			PTFail("could not install the probe camera");
			return false;
		}
		g_uPTPose = 0u;
		g_ePTPhase = PTPhase::SceneSettle;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::SceneSettle:
	{
		u_int uCount = 0u;
		const PTPose* pxPoses = PTPoses(g_uPTScene, uCount);
		// Keep re-applying the first pose so streaming terrain can resolve the
		// ground height and the camera looks at the right place while it settles.
		PTApplyPose(pxPoses[0]);
		if (g_iPTPhaseFrames < iPT_SCENE_SETTLE)
		{
			return true;
		}
		g_ePTPhase = PTPhase::PoseSettle;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::PoseSettle:
	{
		u_int uCount = 0u;
		const PTPose* pxPoses = PTPoses(g_uPTScene, uCount);
		if (g_uPTPose >= uCount)
		{
			++g_uPTScene;
			g_ePTPhase = PTPhase::LoadScene;
			g_iPTPhaseFrames = 0;
			return true;
		}
		const PTPose& xPose = pxPoses[g_uPTPose];
		if (!PTApplyPose(xPose))
		{
			if (g_bPTFailed)
			{
				return false;
			}
			if (g_iPTPhaseFrames > iPT_SCENE_DEADLINE)
			{
				PTFail("a ground-relative pose never resolved a terrain height");
				return false;
			}
			return true;
		}
		if (g_iPTPhaseFrames < g_iPTSettle)
		{
			return true;
		}
		// This frame renders with the feature ON and EndFrame writes the base shot.
		// NOTHING may change render state below this line.
		PTRequestDump(xPose);
		g_ePTPhase = (g_ePTAltMode != PTAltMode::NONE) ? PTPhase::AltFlip : PTPhase::PostDump;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::AltFlip:
	{
		// One frame after the base capture, so the base shot is already written
		// from a frame that still had the feature on.
		PTReapplyCurrentPose();
		PTSetAltFeatureEnabled(false);
		g_ePTPhase = PTPhase::AltSettle;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::AltSettle:
	{
		PTReapplyCurrentPose();
		if (g_iPTPhaseFrames < iPT_ALT_SETTLE)
		{
			return true;
		}
		u_int uAltCount = 0u;
		const PTPose* pxAltPoses = PTPoses(g_uPTScene, uAltCount);
		if (g_uPTPose < uAltCount)
		{
			// Same rule: this frame renders with the feature OFF and EndFrame writes
			// the suffixed shot, so the restore waits for AltRestore.
			PTRequestDump(pxAltPoses[g_uPTPose],
				g_ePTAltMode == PTAltMode::TERRAIN_SHADOW ? "__noterrainshadow" : "__noshadow");
		}
		g_ePTPhase = PTPhase::AltRestore;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::AltRestore:
	{
		PTReapplyCurrentPose();
		PTSetAltFeatureEnabled(true);
		g_ePTPhase = PTPhase::PostDump;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::PostDump:
	{
		if (g_iPTPhaseFrames < iPT_POST_DUMP)
		{
			return true;
		}
		++g_uPTPose;
		g_ePTPhase = PTPhase::PoseSettle;
		g_iPTPhaseFrames = 0;
		return true;
	}
	case PTPhase::Done:
	default:
		return false;
	}
}

static bool Verify_ZMPhotoTour()
{
	if (g_bPTSkipped)
	{
		return true;
	}
	if (g_bPTFailed)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] FAILED: %s (shots=%u)", g_szPTFailure ? g_szPTFailure : "?", g_uPTShots);
		return false;
	}
	if (g_ePTPhase != PTPhase::Done)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] FAILED: the tour did not finish (phase=%d scene=%u pose=%u shots=%u)",
			static_cast<int>(g_ePTPhase), g_uPTScene, g_uPTPose, g_uPTShots);
		return false;
	}
// ★ A QUEUED DUMP IS NOT A WRITTEN FILE. Flux_Screenshot::RequestDump only sets
// a pending flag that EndFrame consumes, so the shot counter proves the tour
// ASKED for N captures, never that N landed. An I/O failure (a full disk, a path
// the backend could not open, a frame that never reached EndFrame) used to come
// back as a clean PASS with missing files, and the crop step downstream was
// equally happy with nothing to crop. Verify against the DISK.
u_int uMissing = 0u;
for (const std::string& strPath : g_axPTShotPaths)
{
	std::error_code xErr;
	const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xErr);
	if (xErr || ulSize == 0u)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] MISSING CAPTURE: %s (%s)",
			strPath.c_str(), xErr ? xErr.message().c_str() : "zero bytes");
		++uMissing;
	}
}
if (uMissing != 0u)
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] FAILED: %u of %zu requested captures are missing or empty",
		uMissing, g_axPTShotPaths.size());
	return false;
}
	Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_PhotoTour] DONE: %u shots in %s", g_uPTShots, g_strPTOutDir.c_str());
	return true;
}

static void Teardown_ZMPhotoTour()
{
	Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	xOpts.m_bQuadsEnabled = g_bPTSavedQuads;
	xOpts.m_bTextEnabled = g_bPTSavedText;
	xOpts.m_bPrimitivesEnabled = g_bPTSavedPrimitives;
	g_xEngine.Terrain().SetCastsShadows(g_bPTSavedTerrainShadows);
	xOpts.m_bShadowsEnabled = g_bPTSavedAllShadows;
#ifdef ZENITH_TOOLS
	g_xEngine.Editor().m_xEditorState.m_bViewportOverlaysHidden = false;
#endif
	if (g_bPTActive)
	{
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
	}
	Zenith_InputSimulator::ClearFixedDt();
	Zenith_InputSimulator::ResetAllInputState();
	g_xPTCameraID = INVALID_ENTITY_ID;
	g_bPTActive = false;
}

static const Zenith_AutomatedTest g_xZMPhotoTourTest = {
	"ZM_PhotoTour_Test",
	&Setup_ZMPhotoTour,
	&Step_ZMPhotoTour,
	&Verify_ZMPhotoTour,
	/* maxFrames */ 3 * iPT_SCENE_DEADLINE + 3 * iPT_SCENE_SETTLE + 16 * 600,
	true /* m_bRequiresGraphics */,
	true /* m_bManualOnly */,
	&Teardown_ZMPhotoTour,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMPhotoTourTest);

#endif // ZENITH_INPUT_SIMULATOR
