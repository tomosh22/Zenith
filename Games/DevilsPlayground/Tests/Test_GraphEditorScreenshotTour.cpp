#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Panels/Zenith_EditorPanel_GraphEditor.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

#include <filesystem>

// ============================================================================
// Test_GraphEditorScreenshotTour -- editor-UI scenario tour (windowed).
//
// Companion to Test_GraphEditorLiveAuthoring (which proves the authored-
// graph -> play -> live-reload loop and screenshots marker1..3). This test
// covers the remaining editor scenarios the screenshot tour needs:
//
//   tour1_unresolved      The graph editor showing a .bgraph containing a
//                         node type this build does not have (the unresolved-
//                         node preservation contract, rendered error-red).
//   tour2_component_panel The entity Properties panel showing a
//                         Zenith_GraphComponent with a bound slot.
//
// [GraphShot] markers are emitted for C:\tmp\capture_graph_editor.ps1, which
// screenshots the window when each marker appears in the log. The test also
// asserts the underlying state so it is a real regression test headless
// (requiresGraphics keeps it windowed-only; headless counts as passed-skip).
// ============================================================================

namespace
{
	constexpr const char* szTOUR_ASSET_PATH = "game:Graphs/Tour_Unresolved.bgraph";

	struct TourState
	{
		Zenith_EntityID m_xEntityID;
		bool m_bAuthored = false;
		bool m_bBound = false;
		bool m_bFailedHard = false;
		// Panel state recorded just before the panel closes for the
		// component-panel screenshot (Verify runs after the close).
		bool m_bPanelWasOpen = false;
		u_int m_uNodesSeen = 0;
		u_int m_uEdgesSeen = 0;
		const char* m_szFailure = "";
	};
	TourState g_xTour;

	void FailHard(const char* szWhy)
	{
		g_xTour.m_bFailedHard = true;
		g_xTour.m_szFailure = szWhy;
		Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorScreenshotTour] FAIL: %s", szWhy);
	}

	// Author a graph that deliberately contains an unknown node type, so the
	// editor renders it as unresolved. OnUpdate -> (missing) -> DebugLog.
	bool AuthorUnresolvedAsset()
	{
		Zenith_BehaviourGraphAsset* pxAsset = new Zenith_BehaviourGraphAsset();
		Zenith_GraphDefinition& xDef = pxAsset->GetDefinition();
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uMissing = xDef.AddNode("Imaginary_FutureNode_v9");
		const u_int uLog = xDef.AddNode("DebugLog");
		if (uSource == 0 || uMissing == 0 || uLog == 0)
		{
			delete pxAsset;
			return false;
		}
		xDef.AddEdge(uSource, 0, uMissing, 0);
		xDef.AddEdge(uMissing, 0, uLog, 0);
		xDef.SetNodeEditorPos(uSource, Zenith_Maths::Vector2(40.0f, 80.0f));
		xDef.SetNodeEditorPos(uMissing, Zenith_Maths::Vector2(260.0f, 80.0f));
		xDef.SetNodeEditorPos(uLog, Zenith_Maths::Vector2(480.0f, 80.0f));

		const bool bSaved = Zenith_AssetRegistry::Save(pxAsset, szTOUR_ASSET_PATH);
		delete pxAsset;
		return bSaved;
	}
}

static void Setup_GraphEditorScreenshotTour()
{
	g_xTour = TourState();
	Zenith_InputSimulator::Enable();
	// A deferred play-backup restore may still be pending; entity creation
	// happens in an early Step frame, after the restore has processed.
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);

	std::error_code xEC;
	std::filesystem::remove(Zenith_AssetRegistry::ResolvePath(szTOUR_ASSET_PATH), xEC);
}

static bool Step_GraphEditorScreenshotTour(int iFrame)
{
	if (g_xTour.m_bFailedHard)
	{
		return false;
	}

	switch (iFrame)
	{
	// Author the unresolved-node asset + open it in the graph editor.
	case 10:
	{
		g_xTour.m_bAuthored = AuthorUnresolvedAsset();
		if (!g_xTour.m_bAuthored)
		{
			FailHard("could not author the unresolved-node asset");
			break;
		}
		Zenith_GraphEditorPanel::OpenAsset(szTOUR_ASSET_PATH);
		break;
	}

	// Create the host entity, bind the graph to its GraphComponent, select it
	// so the Properties panel shows the component's slot list.
	case 16:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "GraphTourTarget");
		Zenith_GraphComponent& xGraphs = xEntity.AddComponent<Zenith_GraphComponent>();
		g_xTour.m_bBound = xGraphs.AddGraphByAssetPath(szTOUR_ASSET_PATH) != nullptr;
		g_xTour.m_xEntityID = xEntity.GetEntityID();
		if (!g_xTour.m_bBound)
		{
			FailHard("AddGraphByAssetPath failed");
			break;
		}
		g_xEngine.Editor().SelectEntityByName("GraphTourTarget");
		break;
	}

	// Settle frames, then the two capture markers.
	case 40:
		Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] tour1_unresolved");
		break;

	// Record the panel state, then close it so the entity Properties panel
	// (with the GraphComponent slot list) is visible for the second shot.
	case 55:
		g_xTour.m_bPanelWasOpen = Zenith_GraphEditorPanel::IsOpen();
		g_xTour.m_uNodesSeen = Zenith_GraphEditorPanel::GetNodeCount();
		g_xTour.m_uEdgesSeen = Zenith_GraphEditorPanel::GetEdgeCount();
		Zenith_GraphEditorPanel::Close();
		break;

	case 100:
		Zenith_Log(LOG_CATEGORY_CORE, "[GraphShot] tour2_component_panel");
		break;

	case 160:
		return false;

	default:
		break;
	}
	return true;
}

static bool Verify_GraphEditorScreenshotTour()
{
	if (g_xTour.m_bFailedHard)
	{
		return false;
	}
	if (!g_xTour.m_bAuthored || !g_xTour.m_bBound)
	{
		return false;
	}

	// The editor really had the asset open with 3 nodes / 2 edges (recorded
	// just before the deliberate close for the component-panel shot).
	if (!g_xTour.m_bPanelWasOpen)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorScreenshotTour] panel was not open");
		return false;
	}
	if (g_xTour.m_uNodesSeen != 3u || g_xTour.m_uEdgesSeen != 2u)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorScreenshotTour] expected 3 nodes / 2 edges, got %u / %u",
			g_xTour.m_uNodesSeen, g_xTour.m_uEdgesSeen);
		return false;
	}

	// The unresolved node survived load AND the runtime reports it unresolved
	// (the preservation contract the editor renders error-red).
	Zenith_BehaviourGraphAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(szTOUR_ASSET_PATH);
	if (pxAsset == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorScreenshotTour] asset missing from registry");
		return false;
	}
	Zenith_BehaviourGraph xProbe;
	xProbe.InitialiseFromDefinition(pxAsset->GetDefinition());
	if (xProbe.GetUnresolvedCount() != 1u)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[GraphEditorScreenshotTour] expected 1 unresolved node, got %u",
			xProbe.GetUnresolvedCount());
		return false;
	}

	// The component panel scenario had a real slot behind it.
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xTour.m_xEntityID);
	if (pxScene == nullptr) return false;
	Zenith_Entity xEnt = pxScene->TryGetEntity(g_xTour.m_xEntityID);
	if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_GraphComponent>()) return false;
	if (xEnt.GetComponent<Zenith_GraphComponent>().GetGraphCount() != 1u) return false;

	// Teardown: delete the tour asset + entity (panel already closed).
	xEnt.DestroyImmediate();
	std::error_code xEC;
	std::filesystem::remove(Zenith_AssetRegistry::ResolvePath(szTOUR_ASSET_PATH), xEC);
	return true;
}

static const Zenith_AutomatedTest g_xGraphEditorScreenshotTourTest = {
	"Test_GraphEditorScreenshotTour",
	&Setup_GraphEditorScreenshotTour,
	&Step_GraphEditorScreenshotTour,
	&Verify_GraphEditorScreenshotTour,
	/*maxFrames*/ 200,
	/*requiresGraphics*/ true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGraphEditorScreenshotTourTest);

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
