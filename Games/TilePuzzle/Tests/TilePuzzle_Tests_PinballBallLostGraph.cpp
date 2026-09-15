#include "Zenith.h"

#if defined(ZENITH_INPUT_SIMULATOR) && defined(ZENITH_TOOLS)

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"

namespace
{
	constexpr const char* szASSET = "game:Graphs/Pinball_BallLostFlow.bgraph";
	int g_iChecks = 0;
	int g_iFailures = 0;
	bool g_bQueued = false;
	bool g_bChecked = false;

	void Check(bool bValue, const char* szWhat)
	{
		++g_iChecks;
		if (!bValue)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[PinballBallLostGraph] FAILED: %s", szWhat);
		}
	}

	void Setup_PinballBallLostGraph()
	{
		g_iChecks = 0;
		g_iFailures = 0;
		g_bQueued = false;
		g_bChecked = false;
		Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
		xAuto.Reset();
		xAuto.AddStep_GraphOpenFresh(szASSET);
		xAuto.AddStep_GraphAddNode("OnCustomEvent");
		xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
		xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "BallLost");
		xAuto.AddStep_GraphAddNode("PinballHandleBallLost");
		xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "PinballHandleBallLost", 0);
		xAuto.AddStep_GraphSave();
		xAuto.AddStep_GraphClose();
		xAuto.Begin();
		g_bQueued = true;
	}

	bool Step_PinballBallLostGraph(int /*iFrame*/)
	{
		Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
		if (xAuto.IsRunning() && !xAuto.IsComplete())
		{
			xAuto.ExecuteNextStep();
			return true;
		}
		if (g_bChecked) return false;
		g_bChecked = true;
		Check(g_bQueued && xAuto.IsComplete(), "this fixture drained its own graph-authoring queue");
		Zenith_BehaviourGraphAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(szASSET);
		Check(pxAsset != nullptr, "the automation-authored Pinball_BallLostFlow asset is available");
		if (pxAsset == nullptr) return false;
		const Zenith_GraphDefinition& xDefinition = pxAsset->GetDefinition();
		Check(xDefinition.GetNodeCount() == 2u, "BallLost flow has exactly two nodes");
		Check(xDefinition.GetEdgeCount() == 1u, "BallLost flow has exactly one exec edge");
		Check(xDefinition.GetDataEdgeCount() == 0u, "BallLost flow has zero data edges");
		if (xDefinition.GetNodeCount() == 2u)
		{
			const Zenith_GraphNodeDef& xEvent = xDefinition.GetNodeAt(0u);
			const Zenith_GraphNodeDef& xHandler = xDefinition.GetNodeAt(1u);
			Check(xEvent.m_strTypeName == "OnCustomEvent", "first node is OnCustomEvent");
			Check(xHandler.m_strTypeName == "PinballHandleBallLost", "second node is PinballHandleBallLost");
			if (xEvent.m_strTypeName == "OnCustomEvent")
			{
				Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
				xRegistry.EnsureInitialized();
				const Zenith_GraphNodeTypeInfo* pxEventInfo = xRegistry.Find("OnCustomEvent");
				Zenith_GraphNode* pxEvent = pxEventInfo ? pxEventInfo->m_pfnCreate() : nullptr;
				Check(pxEvent != nullptr, "OnCustomEvent is registered for the saved-event check");
				if (pxEvent != nullptr)
				{
					xDefinition.ApplyNodeParams(xEvent.m_uNodeID, pxEvent, *pxEventInfo);
					const Zenith_PropertyTable* pxProperties = pxEventInfo->m_pfnGetPropertyTable();
					const Zenith_ReflectedProperty* pxName = pxProperties ? pxProperties->FindProperty("m_strEventName") : nullptr;
					Zenith_PropertyValue xName;
					if (pxName != nullptr) pxName->m_pfnGet(pxEvent, xName);
					Check(pxName != nullptr && xName.GetType() == PROPERTY_TYPE_STRING && xName.GetString() == "BallLost",
						"the saved OnCustomEvent event name is BallLost");
					delete pxEvent;
				}
			}
			if (xDefinition.GetEdgeCount() == 1u)
			{
				const Zenith_GraphEdge& xEdge = xDefinition.GetEdgeAt(0u);
				Check(xEdge.m_uSrcNodeID == xEvent.m_uNodeID && xEdge.m_uSrcPin == 0u && xEdge.m_uDstNodeID == xHandler.m_uNodeID,
					"the one exec edge connects OnCustomEvent pin zero to PinballHandleBallLost");
			}
		}
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		Zenith_GraphDefinitionValidator::Validate(xDefinition, xRegistry, szASSET, axFindings);
		u_int uErrors = 0u;
		for (u_int u = 0u; u < axFindings.GetSize(); ++u)
		{
			if (axFindings.Get(u).m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR) ++uErrors;
		}
		Check(uErrors == 0u, "the saved BallLost graph has zero full-validator errors");
		Zenith_BehaviourGraph xGraph;
		const bool bInitialised = xGraph.InitialiseFromDefinition(xDefinition);
		Check(bInitialised, "the authored BallLost graph initializes");
		if (bInitialised)
		{
			Check(xGraph.GetResolutionSkipCountForTest() == 0u, "the authored BallLost graph resolves zero skipped edges");
		}
		xGraph.Shutdown();
		return false;
	}

	bool Verify_PinballBallLostGraph()
	{
		Check(g_bChecked, "the BallLost graph inspection ran after automation completed");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PinballBallLostGraph] %d checks, %d failed", g_iChecks, g_iFailures);
		return g_iChecks > 0 && g_iFailures == 0;
	}
}

static const Zenith_AutomatedTest g_xPinballBallLostGraphTest = {
	"Pinball_BallLostGraph_ValidatesClean", &Setup_PinballBallLostGraph,
	&Step_PinballBallLostGraph, &Verify_PinballBallLostGraph, 32, false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPinballBallLostGraphTest);

#endif
