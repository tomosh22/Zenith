#include "Zenith.h"

#if defined(ZENITH_INPUT_SIMULATOR) && defined(ZENITH_TOOLS)

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"   // g_xEngine (previously reached transitively via the removed Flux_CommandList.h)
#include "Editor/Zenith_Editor.h"
#include "ZenithECS/Zenith_SceneSystem.h"

// ============================================================================
// Test_EditorSceneCycle
//
// Windowed-only regression cover for the editor's per-frame deferred scene
// operations (Zenith_Editor::ProcessDeferredSceneOperations), which no longer
// wait for GPU idle before tearing scenes down — they rely on the same
// QueueVRAMDeletion MAX_FRAMES_IN_FLIGHT+1 grace period the runtime LoadScene
// path uses mid-play. This drives the two live paths with real GPU frames in
// flight:
//   1. Registered scene load (toolbar path): FrontEnd -> ProcLevel ->
//      FrontEnd -> ProcLevel, each a full SCENE_LOAD_SINGLE teardown of the
//      procgen world's GPU resources while previous frames are still on the
//      GPU.
//   2. Play -> Stop backup restore (HandlePendingSceneLoad): two full
//      Stop/Play cycles, each restoring the backup scene via
//      ResetAllRenderSystems + force-unload + Reset + deserialize.
// Run it with the Vulkan validation layer enabled to catch any
// use-after-free on buffers/images freed by the teardown.
// ============================================================================

namespace
{
	int g_iEditorCycleFailures = 0;
}

static void Setup_EditorSceneCycle()
{
	g_iEditorCycleFailures = 0;
}

static bool Step_EditorSceneCycle(int iFrame)
{
	// Generous gaps between operations so the deferred-deletion queue fully
	// drains (MAX_FRAMES_IN_FLIGHT+1 frames) between teardowns and the
	// procgen scene gets real rendered frames before being destroyed.
	switch (iFrame)
	{
		case 30:  g_xEngine.Editor().RequestLoadRegisteredScene(1); break; // ProcLevel
		case 150: g_xEngine.Editor().RequestLoadRegisteredScene(0); break; // FrontEnd
		case 240: g_xEngine.Editor().RequestLoadRegisteredScene(1); break; // ProcLevel again
		case 360: g_xEngine.Editor().SetEditorMode(EditorMode::Stopped); break; // queue backup restore
		case 420: g_xEngine.Editor().SetEditorMode(EditorMode::Playing); break; // re-enter play (new backup)
		case 480: g_xEngine.Editor().SetEditorMode(EditorMode::Stopped); break; // second restore cycle
		case 540: g_xEngine.Editor().SetEditorMode(EditorMode::Playing); break; // leave harness-default mode
	}
	return iFrame < 600;
}

static bool Verify_EditorSceneCycle()
{
	// Reaching Verify at all means no crash/assert across 4 full scene
	// teardowns with frames in flight. Sanity-check the end state.
	if (g_xEngine.Editor().GetEditorMode() != EditorMode::Playing)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_EditorSceneCycle: expected Playing mode at end");
		++g_iEditorCycleFailures;
	}

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	if (!xActiveScene.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_EditorSceneCycle: active scene invalid after cycles");
		++g_iEditorCycleFailures;
	}

	return g_iEditorCycleFailures == 0;
}

static const Zenith_AutomatedTest g_xEditorSceneCycleTest = {
	"Test_EditorSceneCycle",
	&Setup_EditorSceneCycle,
	&Step_EditorSceneCycle,
	&Verify_EditorSceneCycle,
	/*maxFrames*/ 700,
	true // m_bRequiresGraphics: the point is teardown with real GPU frames in flight
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xEditorSceneCycleTest);

#endif // ZENITH_INPUT_SIMULATOR && ZENITH_TOOLS
