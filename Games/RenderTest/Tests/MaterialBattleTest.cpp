#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_CommandLine.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include "Flux/Flux_Screenshot.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// MaterialBattleTest -- windowed capture sweep of the RenderTest material
// showcase platform (RenderTest_SpawnMaterialShowcase), proving every shape and
// material renders through the real deferred path. Runs in Stopped mode so the
// editor free-camera (which we drive directly) frames the scene instead of the
// player camera; dumps a swapchain TGA at each waypoint via Flux_Screenshot.
// requiresGraphics -> windowed only (headless counts as passed-skip).
// ============================================================================

// Spawns the showcase platform/grid into the active scene (defined in RenderTest.cpp).
void RenderTest_SpawnMaterialShowcase();

// Showcase grid extents, filled by RenderTest_SpawnMaterialShowcase.
namespace RenderTest_Showcase
{
	extern int   g_iColumns;
	extern float g_fGridMinX;
	extern float g_fGridMaxX;
}

namespace
{
	// Campus recentred from the (256,256) corner to the terrain centre (2048,2048);
	// every world XZ below is the legacy layout shifted by fSHIFT.
	constexpr float fSHIFT = 1792.0f;
	constexpr float fCZ    = 300.0f + fSHIFT;	// platform centre Z (matches RenderTest_Showcase)
	constexpr float fTOP_Y = 48.75f;	// platform top (matches RenderTest_Showcase::fPLATFORM_TOP_Y)
	constexpr float fGridY = fTOP_Y + 0.8f;	// approx shape-centre height
	bool g_bShowcasePresent = false;

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

	// Place the editor camera at (cx,cy,cz) aimed at (tx,ty,tz). Derives the
	// pitch/yaw the editor's BuildViewMatrix expects: forward =
	// (-cos p sin y, sin p, cos p cos y), so pitch=atan2(dy,horiz),
	// yaw=atan2(-dx,dz).
	void AimAt(float cx, float cy, float cz, float tx, float ty, float tz, float fFOV)
	{
		const float dx = tx - cx, dy = ty - cy, dz = tz - cz;
		const float fHoriz = sqrtf(dx * dx + dz * dz);
		const double fPitch = atan2((double)dy, (double)fHoriz);
		const double fYaw = atan2((double)(-dx), (double)dz);
		SetCam(cx, cy, cz, fPitch, fYaw, fFOV);
	}

	void Shot(const char* szName)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "[MaterialBattleShot] %s", szName);
		char szPath[256];
		snprintf(szPath, sizeof(szPath), "C:/tmp/mat_battle_%s.tga", szName);
		Flux_Screenshot::RequestDump(szPath);
	}

	// X centre of column-band b of 4, derived from the spawned grid extents.
	float BandX(int b)
	{
		const float fMin = RenderTest_Showcase::g_fGridMinX;
		const float fMax = RenderTest_Showcase::g_fGridMaxX;
		return fMin + (fMax - fMin) * ((b + 0.5f) / 4.0f);
	}
}

static void Setup_MaterialBattleTest()
{
	// Stopped -> editor free-camera renders the world scene (player camera idle).
	// The harness runs requiresGraphics tests through a Play->Stop cycle whose
	// deferred Stop-restore strips the boot-spawned procedural showcase (meshes
	// don't serialize), so we re-spawn it from Step once the restore has settled.
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
}

static bool Step_MaterialBattleTest(int iFrame)
{
	if (Zenith_CommandLine::IsHeadless()) return false;	// requiresGraphics: skip in headless

	switch (iFrame)
	{
	// Re-spawn the showcase AFTER the deferred Play->Stop scene restore (queued
	// for ~frame 1) has completed, so the procedural meshes are live for capture.
	case 8:
		RenderTest_SpawnMaterialShowcase();
		g_bShowcasePresent = (RenderTest_Showcase::g_iColumns > 0);
		break;

	// Two full-grid overviews: a steep near-top-down (spreads every cell out so
	// none occludes) and a shallow raking one (sweeps IBL highlights across the
	// spheres).
	// Camera Y is kept relative to the platform top (fTOP_Y) so the framing
	// follows the showcase wherever it sits (it moved 120 -> 48.75 to be
	// co-planar with the campus). The original absolute Ys were platform-top + {28,7,14,3,10}.
	case 40:
		AimAt(256.0f + fSHIFT, fTOP_Y + 28.0f, 286.0f + fSHIFT, 256.0f + fSHIFT, fGridY, fCZ + 1.0f, 60.0f);
		Shot("00_overview_top"); break;
	case 70:
		AimAt(256.0f + fSHIFT, fTOP_Y + 7.0f, 274.0f + fSHIFT, 256.0f + fSHIFT, fTOP_Y + 1.0f, fCZ, 62.0f);
		Shot("01_overview_raking"); break;

	// Four column-band close-ups (each ~2-3 columns x all 5 rows), above-front.
	case 100: AimAt(BandX(0), fTOP_Y + 14.0f, 290.0f + fSHIFT, BandX(0), fGridY, fCZ + 1.0f, 44.0f); Shot("02_band_left");     break;
	case 130: AimAt(BandX(1), fTOP_Y + 14.0f, 290.0f + fSHIFT, BandX(1), fGridY, fCZ + 1.0f, 44.0f); Shot("03_band_midleft");  break;
	case 160: AimAt(BandX(2), fTOP_Y + 14.0f, 290.0f + fSHIFT, BandX(2), fGridY, fCZ + 1.0f, 44.0f); Shot("04_band_midright"); break;
	case 190: AimAt(BandX(3), fTOP_Y + 14.0f, 290.0f + fSHIFT, BandX(3), fGridY, fCZ + 1.0f, 44.0f); Shot("05_band_right");    break;

	// A low grazing front shot so specular/clearcoat highlights pop, plus a
	// side-angle so the metals reflect the environment off-axis.
	case 220:
		AimAt(256.0f + fSHIFT, fTOP_Y + 3.0f, 285.0f + fSHIFT, 256.0f + fSHIFT, fTOP_Y + 1.0f, fCZ, 60.0f);
		Shot("06_grazing_front"); break;
	case 250:
		AimAt(236.0f + fSHIFT, fTOP_Y + 10.0f, 292.0f + fSHIFT, 258.0f + fSHIFT, fGridY, fCZ + 1.0f, 60.0f);
		Shot("07_angled_side"); break;

	case 276:
		return false;

	default:
		break;
	}
	return true;
}

static bool Verify_MaterialBattleTest()
{
	if (Zenith_CommandLine::IsHeadless()) return true;	// passed-skip
	if (!g_bShowcasePresent || RenderTest_Showcase::g_iColumns <= 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialBattleTest] showcase grid was not spawned (g_iColumns=%d)", RenderTest_Showcase::g_iColumns);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMaterialBattleTest = {
	"MaterialBattleTest",
	&Setup_MaterialBattleTest,
	&Step_MaterialBattleTest,
	&Verify_MaterialBattleTest,
	/*maxFrames*/ 300,
	/*requiresGraphics*/ true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMaterialBattleTest);

#endif // ZENITH_TOOLS
