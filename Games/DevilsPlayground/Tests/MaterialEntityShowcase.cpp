#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_CommandLine.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// MaterialEntityShowcase -- windowed capture sweep of the DevilsPlayground
// procgen village, proving every entity now renders with an appropriate
// material (stone walls, a glowing pentagram, an ember-lit forge, wooden
// doors/chests, a brass noise machine, archetype-robed villagers, the priest's
// dark robe, and tag-coloured items). Loads ProcLevel (build index 1), runs in
// Stopped mode so the editor free-camera frames the village (default GenConfig
// = 100x100 m centred at (50,50)), and dumps a swapchain TGA at each waypoint.
// requiresGraphics -> windowed only (headless counts as passed-skip).
// ============================================================================

namespace
{
	constexpr float fCX = 50.0f;	// village centre X (GenConfig default bounds 0..100)
	constexpr float fCZ = 50.0f;	// village centre Z
	bool g_bLoaded = false;

	void SetCam(float fX, float fY, float fZ, double fPitch, double fYaw, float fFOV)
	{
		Zenith_EditorCameraState& xCam = g_xEngine.Editor().m_xEditorState.m_xCamera;
		xCam.m_xPosition = Zenith_Maths::Vector3(fX, fY, fZ);
		xCam.m_fPitch = fPitch;
		xCam.m_fYaw = fYaw;
		xCam.m_fFOV = fFOV;
		xCam.m_fNear = 0.1f;
		xCam.m_fFar = 6000.0f;
	}

	// Aim the editor camera at a world target. Matches BuildViewMatrix's
	// forward = (-cos p sin y, sin p, cos p cos y).
	void AimAt(float cx, float cy, float cz, float tx, float ty, float tz, float fFOV)
	{
		const float dx = tx - cx, dy = ty - cy, dz = tz - cz;
		const float fHoriz = sqrtf(dx * dx + dz * dz);
		SetCam(cx, cy, cz, atan2((double)dy, (double)fHoriz), atan2((double)(-dx), (double)dz), fFOV);
	}

	void Shot(const char* szName)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "[DPMaterialShot] %s", szName);
		char szPath[256];
		snprintf(szPath, sizeof(szPath), "C:/tmp/dp_mat_%s.tga", szName);
		Flux_Screenshot::RequestDump(szPath);
	}
}

static void Setup_MaterialEntityShowcase()
{
	g_bLoaded = false;
	// Editor free-camera renders the world scene (no possessed-villager orbit cam).
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
}

static bool Step_MaterialEntityShowcase(int iFrame)
{
	if (Zenith_CommandLine::IsHeadless()) return false;	// requiresGraphics: skip in headless

	// DP's fog-of-war + volumetric fog washes out the scene in Stopped mode (no
	// villager/light tick registers reveal holes). For a clean material
	// showcase, force-disable the DP fog pass entirely — DP already force-disables
	// the engine fog, so this leaves NO fog. The force-disable overlay persists
	// across graph rebuilds; re-asserting each frame is harmless and survives a
	// feature re-registration.
	if (iFrame >= 8)
	{
		g_xEngine.FluxRenderer().GetRenderGraph().SetPassForceDisabled("DP_Fog", true);
		// The DP night scene leans on fog to dim a bright starfield-IBL + four
		// 2000-lumen lights; with fog off it blows out. Dial the IBL down, pin the
		// HDR exposure low (defeats auto-exposure), and dim the corner lights so
		// the materials read at their true albedo for the showcase.
		g_xEngine.IBL().SetIntensity(0.06f);
		g_xEngine.HDR().SetExposure(0.5f);
		g_xEngine.HDR().SetExposureRange(0.5f, 0.5f);
		g_xEngine.Scenes().QueryAllScenes<Zenith_LightComponent>().ForEach(
			[](Zenith_EntityID, Zenith_LightComponent& xLight) { xLight.SetIntensity(600.0f); });
	}

	switch (iFrame)
	{
	// Load ProcLevel AFTER the harness's deferred Play->Stop restore has run
	// (it reloads the FrontEnd backup ~frame 1, which would otherwise clobber a
	// Setup-time load). The bootstrap then generates + materialises the village.
	case 6:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		break;

	case 40:
		g_bLoaded = true;
		// High angled top-down: the whole village layout — stone walls + the
		// emissive pentagram/forge/items glowing through the dark.
		AimAt(fCX, 95.0f, 14.0f, fCX, 2.0f, fCZ + 6.0f, 66.0f);
		Shot("00_overview_top"); break;
	case 70:
		// Lower three-quarter angle for perspective on the walls + characters.
		AimAt(fCX, 58.0f, -14.0f, fCX, 3.0f, fCZ - 4.0f, 66.0f);
		Shot("01_overview_angled"); break;

	// Four quadrant close-ups (camera above each quarter, looking inward+down)
	// so robes, the priest, doors and items read clearly.
	case 100: AimAt(28.0f, 38.0f, 24.0f, 38.0f, 2.0f, 40.0f, 58.0f); Shot("02_quadrant_sw"); break;
	case 130: AimAt(72.0f, 38.0f, 24.0f, 62.0f, 2.0f, 40.0f, 58.0f); Shot("03_quadrant_se"); break;
	case 160: AimAt(28.0f, 38.0f, 76.0f, 38.0f, 2.0f, 60.0f, 58.0f); Shot("04_quadrant_nw"); break;
	case 190: AimAt(72.0f, 38.0f, 76.0f, 62.0f, 2.0f, 60.0f, 58.0f); Shot("05_quadrant_ne"); break;

	// A near-straight-down map shot (emissive elements pop as glowing markers).
	case 220:
		AimAt(fCX, 100.0f, fCZ + 0.5f, fCX, 1.0f, fCZ, 70.0f);
		Shot("06_map_top"); break;

	case 246:
		return false;

	default:
		break;
	}
	return true;
}

static bool Verify_MaterialEntityShowcase()
{
	if (Zenith_CommandLine::IsHeadless()) return true;	// passed-skip
	if (!g_bLoaded)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEntityShowcase] capture sequence did not run");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMaterialEntityShowcaseTest = {
	"MaterialEntityShowcase",
	&Setup_MaterialEntityShowcase,
	&Step_MaterialEntityShowcase,
	&Verify_MaterialEntityShowcase,
	/*maxFrames*/ 260,
	/*requiresGraphics*/ true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMaterialEntityShowcaseTest);

#endif // ZENITH_TOOLS
