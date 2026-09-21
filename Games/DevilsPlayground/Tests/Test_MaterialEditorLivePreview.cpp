#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_CommandLine.h"
#include "Input/Zenith_InputSimulator.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Panels/Zenith_EditorPanel_MaterialEditor.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"
#include "Flux/Flux_Screenshot.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

#include <filesystem>
#include <cstring>
#include <string>

// ============================================================================
// Test_MaterialEditorLivePreview -- the UE5-style material editor + live IBL
// preview, driven through the panel's atomic Action_* verbs (windowed).
//
// Proves end-to-end: create a material, drive its parameters through the same
// Action verbs the EditorAutomation steps use, switch the preview mesh, rotate
// the preview light, and confirm the panel + offscreen IBL preview are live.
// Emits [MatEditShot] markers so an external watcher can screenshot the editor
// showing the lit preview at each stage; also asserts the underlying material
// state so it is a real regression test (requiresGraphics keeps it windowed-
// only; headless counts as passed-skip).
// ============================================================================

namespace
{
	constexpr const char* szPREVIEW_ASSET_PATH = "game:Materials/EditorPreviewTest.zmtrl";

	// Four capture slots, in Step order: default / red_metal / emissive / cube.
	constexpr u_int uMATEDIT_SHOT_COUNT = 4u;
	constexpr size_t ulMATEDIT_PATH_MAX = 512u;

	// ★ FIXED BUFFERS, NOT std::string, AND THAT IS LOAD-BEARING HERE.
	// A namespace-scope std::string array registers a destructor per element to
	// run at process exit, and this game's late teardown is exactly where a
	// windowed DevilsPlayground run already crashes intermittently: the test
	// PASSES, every capture is written, and then it SIGSEGVs immediately after
	// "[TaskSystem] Task system shutdown complete" (ZEN-91 filed that as a
	// pre-existing shortfall).
	//
	// MEASURED on ZEN-92, same exe, same command line, 5 runs each:
	//   std::string array here ......... 5 of 5 crashed
	//   this file unchanged (control) ... 2 of 5 crashed
	//   these fixed buffers ............ back to the pre-existing rate
	//
	// So the array did not CAUSE the crash -- it made an intermittent one
	// deterministic. The underlying teardown bug is untouched and still open.
	// Do not "modernise" these back to std::string until it is fixed.
	char g_aszShotPath[uMATEDIT_SHOT_COUNT][ulMATEDIT_PATH_MAX] = {};

	void SetShotPath(char (&aszDest)[ulMATEDIT_PATH_MAX], const std::filesystem::path& xPath)
	{
		const std::string strPath = xPath.string();
		if (strPath.size() >= ulMATEDIT_PATH_MAX)
		{
			aszDest[0] = '\0';
			return;
		}
		std::memcpy(aszDest, strPath.c_str(), strPath.size() + 1u);
	}

	// Absolute Build/artifacts/devilsplayground/materialpreview/<tag> dir, derived
	// from GAME_ASSETS_DIR (<repo>/Games/DevilsPlayground/Assets/ -> up three ->
	// <repo>), so it resolves regardless of the process working directory -- the
	// same derivation RT_PhotoTour and ZM_AnimEditorShowcase use.
	// `--capture-tag` (Zenith_CommandLine::GetCaptureTag) selects the run tag;
	// absent, "run" is used so a bare run still lands somewhere predictable
	// rather than silently overwriting a previous one at a fixed C:/tmp path.
	std::filesystem::path CaptureDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		const char* szTag = Zenith_CommandLine::GetCaptureTag();
		if (szTag == nullptr || szTag[0] == '\0')
		{
			szTag = "run";
		}
		return xRepoRoot / "Build" / "artifacts" / "devilsplayground" / "materialpreview" / szTag;
	}

	struct PreviewTestState
	{
		bool m_bCreated = false;
		bool m_bParamsApplied = false;
		bool m_bEmissiveApplied = false;
		bool m_bMeshSwitched = false;
		bool m_bPanelWasOpen = false;
		bool m_bPreviewWasActive = false;
		bool m_bSaved = false;
		bool m_bFailedHard = false;
		const char* m_szFailure = "";
	};
	PreviewTestState g_xPrev;

	void FailHard(const char* szWhy)
	{
		g_xPrev.m_bFailedHard = true;
		g_xPrev.m_szFailure = szWhy;
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] FAIL: %s", szWhy);
	}
}

static void Setup_MaterialEditorLivePreview()
{
	g_xPrev = PreviewTestState();
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	g_xEngine.Editor().GetMaterialEditorShowFlag() = true;

	std::error_code xEC;
	std::filesystem::remove(Zenith_AssetRegistry::ResolvePath(szPREVIEW_ASSET_PATH), xEC);

	const std::filesystem::path xCaptureDir = CaptureDir();
	std::error_code xDirError;
	std::filesystem::create_directories(xCaptureDir, xDirError);
	if (xDirError)
	{
		FailHard("could not create the material preview output directory");
		return;
	}
	SetShotPath(g_aszShotPath[0], xCaptureDir / "mat_shot_default.tga");
	SetShotPath(g_aszShotPath[1], xCaptureDir / "mat_shot_red_metal.tga");
	SetShotPath(g_aszShotPath[2], xCaptureDir / "mat_shot_emissive.tga");
	SetShotPath(g_aszShotPath[3], xCaptureDir / "mat_shot_cube.tga");
}

static bool Step_MaterialEditorLivePreview(int iFrame)
{
	if (g_xPrev.m_bFailedHard) return false;

	switch (iFrame)
	{
	// Create the material via the panel's atomic verb + open the editor.
	case 10:
		g_xPrev.m_bCreated = Zenith_MaterialEditorPanel::Action_CreateMaterial(szPREVIEW_ASSET_PATH);
		if (!g_xPrev.m_bCreated) { FailHard("Action_CreateMaterial failed"); }
		g_xEngine.Editor().GetMaterialEditorShowFlag() = true;
		break;

	// Default grey-plastic sphere under IBL.
	case 35:
		Zenith_Log(LOG_CATEGORY_CORE, "[MatEditShot] preview_default");
		Flux_Screenshot::RequestDump(g_aszShotPath[0]);
		break;

	// Shiny red metal: red base colour, low roughness, full metallic. Same
	// Action verbs the AddStep_Material* automation steps drive.
	case 45:
	{
		bool bOk = true;
		bOk &= Zenith_MaterialEditorPanel::Action_SetParamColor("BaseColor", 0.85f, 0.08f, 0.06f, 1.0f);
		bOk &= Zenith_MaterialEditorPanel::Action_SetParamFloat("Metallic", 1.0f);
		bOk &= Zenith_MaterialEditorPanel::Action_SetParamFloat("Roughness", 0.15f);
		g_xPrev.m_bParamsApplied = bOk;
		if (!bOk) FailHard("Action_SetParam* (red metal) failed");
		break;
	}

	case 75:
		Zenith_Log(LOG_CATEGORY_CORE, "[MatEditShot] preview_red_metal");
		Flux_Screenshot::RequestDump(g_aszShotPath[1]);
		break;

	// HDR emissive glow (feeds the bloom pipeline).
	case 85:
	{
		bool bOk = true;
		bOk &= Zenith_MaterialEditorPanel::Action_SetParamColor("EmissiveColor", 0.1f, 0.8f, 1.0f, 0.0f);
		bOk &= Zenith_MaterialEditorPanel::Action_SetParamFloat("EmissiveIntensity", 8.0f);
		g_xPrev.m_bEmissiveApplied = bOk;
		if (!bOk) FailHard("Action_SetParam* (emissive) failed");
		break;
	}

	case 115:
		Zenith_Log(LOG_CATEGORY_CORE, "[MatEditShot] preview_emissive");
		Flux_Screenshot::RequestDump(g_aszShotPath[2]);
		break;

	// Switch the preview mesh to the cube + rotate the light (UE L-drag verb).
	case 125:
		g_xPrev.m_bMeshSwitched = Zenith_MaterialEditorPanel::Action_SetPreviewMesh(MATERIAL_PREVIEW_MESH_CUBE);
		Zenith_MaterialEditorPanel::Action_SetPreviewLight(1.4f, 0.5f);
		if (!g_xPrev.m_bMeshSwitched) FailHard("Action_SetPreviewMesh failed");
		break;

	case 155:
		// Record live panel/preview state for Verify before saving.
		g_xPrev.m_bPanelWasOpen = Zenith_MaterialEditorPanel::IsOpen();
		g_xPrev.m_bPreviewWasActive = g_xEngine.MaterialPreview().IsActive();
		g_xPrev.m_bSaved = Zenith_MaterialEditorPanel::Action_SaveMaterial(nullptr);
		Zenith_Log(LOG_CATEGORY_CORE, "[MatEditShot] preview_cube");
		Flux_Screenshot::RequestDump(g_aszShotPath[3]);
		break;

	case 180:
		return false;

	default:
		break;
	}
	return true;
}

static bool Verify_MaterialEditorLivePreview()
{
	if (g_xPrev.m_bFailedHard)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] hard failure: %s", g_xPrev.m_szFailure);
		return false;
	}
	if (!g_xPrev.m_bCreated || !g_xPrev.m_bParamsApplied || !g_xPrev.m_bEmissiveApplied || !g_xPrev.m_bMeshSwitched)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] an authoring step did not run");
		return false;
	}
	if (!g_xPrev.m_bPanelWasOpen)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] material editor panel was not open");
		return false;
	}
	if (!g_xPrev.m_bPreviewWasActive)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] preview renderer was not active while the panel was open");
		return false;
	}
	if (!g_xPrev.m_bSaved)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] save failed");
		return false;
	}

	// The Action verbs really mutated the selected material.
	Zenith_MaterialAsset* pxMat = g_xEngine.Editor().GetSelectedMaterial();
	if (!pxMat) { Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] no material selected"); return false; }
	const Zenith_MaterialParams& xP = pxMat->GetParams();
	const bool bRedBase = xP.m_xBaseColor.x > 0.8f && xP.m_xBaseColor.y < 0.2f && xP.m_xBaseColor.z < 0.2f;
	if (!bRedBase || xP.m_fMetallic < 0.99f || xP.m_fRoughness > 0.2f || xP.m_fEmissiveIntensity < 7.9f)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] material params not applied: base(%.2f,%.2f,%.2f) metal=%.2f rough=%.2f emInt=%.2f",
			xP.m_xBaseColor.x, xP.m_xBaseColor.y, xP.m_xBaseColor.z, xP.m_fMetallic, xP.m_fRoughness, xP.m_fEmissiveIntensity);
		return false;
	}

	// Saved file round-trips.
	const std::string strResolved = Zenith_AssetRegistry::ResolvePath(szPREVIEW_ASSET_PATH);
	if (!std::filesystem::exists(strResolved))
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[MaterialEditorLivePreview] saved .zmtrl missing on disk");
		return false;
	}

	// Teardown: clear selection, close the panel, delete the asset on disk.
	g_xEngine.Editor().ClearMaterialSelection();
	g_xEngine.Editor().GetMaterialEditorShowFlag() = false;
	g_xEngine.MaterialPreview().SetActive(false);
	std::error_code xEC;
	std::filesystem::remove(strResolved, xEC);
	return true;
}

// Setup's Play -> Stopped is GLOBAL editor state that outlives this test: it is
// immediately followed by LifeTimer_Test in batch order, which FAILED (life never
// drained, because Stopped disables game logic) until this was fixed. The restore
// lives in DevilsPlayground.cpp's between-tests hook, NOT in a Teardown here --
// see Tests/CLAUDE.md, "Editor mode leaks between tests".
static const Zenith_AutomatedTest g_xMaterialEditorLivePreviewTest = {
	"Test_MaterialEditorLivePreview",
	&Setup_MaterialEditorLivePreview,
	&Step_MaterialEditorLivePreview,
	&Verify_MaterialEditorLivePreview,
	/*maxFrames*/ 220,
	/*requiresGraphics*/ true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMaterialEditorLivePreviewTest);

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
