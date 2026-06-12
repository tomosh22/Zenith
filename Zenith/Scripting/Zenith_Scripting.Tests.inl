//------------------------------------------------------------------------------
// Behaviour Graph runtime unit tests (Phase 1 of the Behaviour Graphs program).
// Included at the bottom of Zenith_BehaviourGraph.cpp.
//
// These tests register their own scratch node types directly with the registry
// (they never disturb the engine registrar - the registry is already
// initialized by the time tests run, and scratch types use Test-prefixed names).
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	//--------------------------------------------------------------------------
	// Scratch node types
	//--------------------------------------------------------------------------

	// Plain event source: fires its chain every dispatch.
	class GraphTestOnUpdateSource : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_OnUpdate"; }
	};

	// Counts executions; configurable status; writes its count to the blackboard.
	class GraphTestCounterNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphTestCounterNode)
	public:
		ZENITH_PROPERTY(std::string, m_strCounterName, "count")
		ZENITH_PROPERTY(int32_t, m_iReturnStatus, 0)	// 0=SUCCESS 1=FAILURE 2=RUNNING

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			++m_uExecuteCount;
			Zenith_PropertyValue xValue;
			xValue.SetInt32(static_cast<int32_t>(m_uExecuteCount));
			xContext.m_pxBlackboard->SetValue(m_strCounterName, xValue);
			return static_cast<GraphNodeStatus>(m_iReturnStatus);
		}
		const char* GetTypeName() const override { return "Test_Counter"; }

		u_int m_uExecuteCount = 0;
	};

	// RUNNING for m_iTicks executions, then SUCCESS.
	class GraphTestWaitTicksNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphTestWaitTicksNode)
	public:
		ZENITH_PROPERTY(int32_t, m_iTicks, 2)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			++m_iElapsed;
			if (m_iElapsed < m_iTicks)
			{
				return GRAPH_NODE_STATUS_RUNNING;
			}
			m_iElapsed = 0;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Test_WaitTicks"; }

	private:
		int32_t m_iElapsed = 0;
	};

	// Flow node: runs pin 0 when the blackboard bool is true, else pin 1.
	class GraphTestBranchNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphTestBranchNode)
	public:
		ZENITH_PROPERTY(std::string, m_strConditionVar, "condition")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const u_int uPin = xContext.m_pxBlackboard->GetBool(m_strConditionVar, false) ? 0u : 1u;
			return xContext.m_pxGraph->RunChainFromPin(GetNodeID(), uPin, xContext);
		}
		const char* GetTypeName() const override { return "Test_Branch"; }
	};

	// Custom-event source matching its configured name.
	class GraphTestCustomSource : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphTestCustomSource)
	public:
		ZENITH_PROPERTY(std::string, m_strEventName, "evt")

		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_CustomSource"; }
		bool MatchesCustomEvent(const char* szName) const override { return m_strEventName == szName; }
	};

	// Registers the scratch types once (idempotent via the registry dup guard,
	// which logs - so gate on a static instead).
	void EnsureTestNodesRegistered()
	{
		static bool ls_bRegistered = false;
		if (ls_bRegistered)
		{
			return;
		}
		ls_bRegistered = true;
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		xRegistry.RegisterNodeType<GraphTestOnUpdateSource>("Test_OnUpdate", GRAPH_EVENT_ON_UPDATE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphTestCounterNode>("Test_Counter", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphTestWaitTicksNode>("Test_WaitTicks", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphTestBranchNode>("Test_Branch", GRAPH_EVENT_NONE, 2, true, "Test");
		xRegistry.RegisterNodeType<GraphTestCustomSource>("Test_CustomSource", GRAPH_EVENT_CUSTOM, 1, false, "Test");
	}

	Zenith_GraphContext MakeTestContext(Zenith_BehaviourGraph& xGraph, float fDt = 0.016f)
	{
		Zenith_GraphContext xContext;
		xContext.m_fDt = fDt;
		xContext.m_pxGraph = &xGraph;
		xContext.m_pxBlackboard = &xGraph.GetBlackboard();
		return xContext;
	}
}

ZENITH_TEST(BehaviourGraph, ChainExecutionOrderAndParams)
{
	EnsureTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uCounterA = xDef.AddNode("Test_Counter");
	const u_int uCounterB = xDef.AddNode("Test_Counter");
	ZENITH_ASSERT_NE(uSource, 0u);

	// Configure params through the blob round-trip (the same path the editor
	// and asset loading use).
	{
		GraphTestCounterNode xTemp;
		xTemp.m_strCounterName = "a";
		xDef.SetNodeParamsFromInstance(uCounterA, &xTemp);
		xTemp.m_strCounterName = "b";
		xDef.SetNodeParamsFromInstance(uCounterB, &xTemp);
	}

	ZENITH_ASSERT_TRUE(xDef.AddEdge(uSource, 0, uCounterA, 0));
	ZENITH_ASSERT_TRUE(xDef.AddEdge(uCounterA, 0, uCounterB, 0));
	// One-edge-per-pin rule.
	ZENITH_ASSERT_FALSE(xDef.AddEdge(uCounterA, 0, uSource, 0));

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
	ZENITH_ASSERT_TRUE(xGraph.HasEventSource(GRAPH_EVENT_ON_UPDATE));

	Zenith_GraphContext xContext = MakeTestContext(xGraph);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("a"), 1);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("b"), 1);

	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("a"), 2);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("b"), 2);
}

ZENITH_TEST(BehaviourGraph, RunningSuspendsAndResumesWithoutReexecuting)
{
	EnsureTestNodesRegistered();

	// OnUpdate -> counter "pre" -> wait(3 ticks) -> counter "post"
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uPre = xDef.AddNode("Test_Counter");
	const u_int uWait = xDef.AddNode("Test_WaitTicks");
	const u_int uPost = xDef.AddNode("Test_Counter");
	{
		GraphTestCounterNode xTemp;
		xTemp.m_strCounterName = "pre";
		xDef.SetNodeParamsFromInstance(uPre, &xTemp);
		xTemp.m_strCounterName = "post";
		xDef.SetNodeParamsFromInstance(uPost, &xTemp);
		GraphTestWaitTicksNode xWait;
		xWait.m_iTicks = 3;
		xDef.SetNodeParamsFromInstance(uWait, &xWait);
	}
	xDef.AddEdge(uSource, 0, uPre, 0);
	xDef.AddEdge(uPre, 0, uWait, 0);
	xDef.AddEdge(uWait, 0, uPost, 0);

	Zenith_BehaviourGraph xGraph;
	xGraph.InitialiseFromDefinition(xDef);
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	// Tick 1: pre executes once, wait suspends, post not reached.
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("pre"), 1);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("post", -1), -1);

	// Tick 2: chain RESUMES at the wait - "pre" must NOT re-execute.
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("pre"), 1);

	// Tick 3: wait completes, post runs.
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("pre"), 1);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("post"), 1);

	// Tick 4: chain restarts from the top.
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("pre"), 2);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("post"), 1);
}

ZENITH_TEST(BehaviourGraph, BranchFlowNodeRunsCorrectPinAndStopsChain)
{
	EnsureTestNodesRegistered();

	// OnUpdate -> Branch(condition) ; pin0 -> counter "true" ; pin1 -> counter "false"
	// A counter wired AFTER the branch on the source chain must never run
	// (flow nodes end their chain).
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uBranch = xDef.AddNode("Test_Branch");
	const u_int uTrue = xDef.AddNode("Test_Counter");
	const u_int uFalse = xDef.AddNode("Test_Counter");
	{
		GraphTestCounterNode xTemp;
		xTemp.m_strCounterName = "true";
		xDef.SetNodeParamsFromInstance(uTrue, &xTemp);
		xTemp.m_strCounterName = "false";
		xDef.SetNodeParamsFromInstance(uFalse, &xTemp);
	}
	xDef.AddEdge(uSource, 0, uBranch, 0);
	xDef.AddEdge(uBranch, 0, uTrue, 0);
	xDef.AddEdge(uBranch, 1, uFalse, 0);
	xDef.DeclareVariable("condition", Zenith_PropertyValue());

	Zenith_BehaviourGraph xGraph;
	xGraph.InitialiseFromDefinition(xDef);
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	Zenith_PropertyValue xTrueValue;
	xTrueValue.SetBool(true);
	xGraph.GetBlackboard().SetValue("condition", xTrueValue);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("true"), 1);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("false", -1), -1);

	Zenith_PropertyValue xFalseValue;
	xFalseValue.SetBool(false);
	xGraph.GetBlackboard().SetValue("condition", xFalseValue);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("true"), 1);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("false"), 1);
}

ZENITH_TEST(BehaviourGraph, DefinitionSerializationRoundTrip)
{
	EnsureTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	Zenith_PropertyValue xSpeedDefault;
	xSpeedDefault.SetFloat(7.5f);
	xDef.DeclareVariable("speed", xSpeedDefault);

	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uCounter = xDef.AddNode("Test_Counter");
	{
		GraphTestCounterNode xTemp;
		xTemp.m_strCounterName = "roundtrip";
		xDef.SetNodeParamsFromInstance(uCounter, &xTemp);
	}
	xDef.AddEdge(uSource, 0, uCounter, 0);
	xDef.SetNodeEditorPos(uCounter, Zenith_Maths::Vector2(120.0f, 40.0f));

	Zenith_DataStream xStream;
	xDef.WriteToDataStream(xStream);
	const u_int uSentinel = 0xCAFEF00Du;
	xStream << uSentinel;

	Zenith_GraphDefinition xLoaded;
	xStream.SetCursor(0);
	ZENITH_ASSERT_TRUE(xLoaded.ReadFromDataStream(xStream));
	u_int uReadSentinel = 0;
	xStream >> uReadSentinel;
	ZENITH_ASSERT_EQ(uReadSentinel, 0xCAFEF00Du);

	ZENITH_ASSERT_EQ(xLoaded.GetVariableCount(), 1u);
	ZENITH_ASSERT_EQ(xLoaded.GetNodeCount(), 2u);
	ZENITH_ASSERT_EQ(xLoaded.GetEdgeCount(), 1u);
	Zenith_Maths::Vector2 xPos;
	ZENITH_ASSERT_TRUE(xLoaded.GetNodeEditorPos(uCounter, xPos));
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 120.0f, 0.0001f);

	// The loaded definition runs identically - params survived the blob.
	Zenith_BehaviourGraph xGraph;
	xGraph.InitialiseFromDefinition(xLoaded);
	ZENITH_ASSERT_EQ_FLOAT(xGraph.GetBlackboard().GetFloat("speed"), 7.5f, 0.0001f);
	Zenith_GraphContext xContext = MakeTestContext(xGraph);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("roundtrip"), 1);
}

ZENITH_TEST(BehaviourGraph, UnresolvedNodePreservedAndFailsChainGracefully)
{
	EnsureTestNodesRegistered();

	// Hand-craft a definition containing a node type this build doesn't have.
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uMissing = xDef.AddNode("Test_DoesNotExistInThisBuild");
	const u_int uAfter = xDef.AddNode("Test_Counter");
	ZENITH_ASSERT_NE(uMissing, 0u);	// added as unresolved, not dropped
	xDef.AddEdge(uSource, 0, uMissing, 0);
	xDef.AddEdge(uMissing, 0, uAfter, 0);

	// Serialization round-trips the unresolved node verbatim.
	Zenith_DataStream xStream;
	xDef.WriteToDataStream(xStream);
	Zenith_GraphDefinition xLoaded;
	xStream.SetCursor(0);
	ZENITH_ASSERT_TRUE(xLoaded.ReadFromDataStream(xStream));
	ZENITH_ASSERT_EQ(xLoaded.GetNodeCount(), 3u);
	ZENITH_ASSERT_NOT_NULL(xLoaded.FindNodeDef(uMissing));
	ZENITH_ASSERT_STREQ(xLoaded.FindNodeDef(uMissing)->m_strTypeName.c_str(), "Test_DoesNotExistInThisBuild");

	// Instantiation reports it; the chain through it fails without crashing,
	// and nodes after it never run.
	Zenith_BehaviourGraph xGraph;
	xGraph.InitialiseFromDefinition(xLoaded);
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 1u);
	Zenith_GraphContext xContext = MakeTestContext(xGraph);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("count", -1), -1);
}

ZENITH_TEST(BehaviourGraph, CustomEventsMatchByName)
{
	EnsureTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	const u_int uSourceA = xDef.AddNode("Test_CustomSource");
	const u_int uSourceB = xDef.AddNode("Test_CustomSource");
	const u_int uCounterA = xDef.AddNode("Test_Counter");
	const u_int uCounterB = xDef.AddNode("Test_Counter");
	{
		GraphTestCustomSource xSource;
		xSource.m_strEventName = "door_open";
		xDef.SetNodeParamsFromInstance(uSourceA, &xSource);
		xSource.m_strEventName = "door_close";
		xDef.SetNodeParamsFromInstance(uSourceB, &xSource);
		GraphTestCounterNode xCounter;
		xCounter.m_strCounterName = "opened";
		xDef.SetNodeParamsFromInstance(uCounterA, &xCounter);
		xCounter.m_strCounterName = "closed";
		xDef.SetNodeParamsFromInstance(uCounterB, &xCounter);
	}
	xDef.AddEdge(uSourceA, 0, uCounterA, 0);
	xDef.AddEdge(uSourceB, 0, uCounterB, 0);

	Zenith_BehaviourGraph xGraph;
	xGraph.InitialiseFromDefinition(xDef);
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	xGraph.FireCustomEvent("door_open", xContext);
	xGraph.FireCustomEvent("door_open", xContext);
	xGraph.FireCustomEvent("door_close", xContext);
	xGraph.FireCustomEvent("unknown_event", xContext);

	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("opened"), 2);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("closed"), 1);
}

ZENITH_TEST(BehaviourGraph, BlackboardTypeSafeMigration)
{
	Zenith_GraphBlackboard xOld;
	Zenith_PropertyValue xValue;
	xValue.SetInt32(42);
	xOld.SetValue("kept", xValue);
	xValue.SetFloat(3.14f);
	xOld.SetValue("typeChanged", xValue);
	xValue.SetBool(true);
	xOld.SetValue("removed", xValue);

	Zenith_GraphBlackboard xNew;
	xValue.SetInt32(0);
	xNew.SetValue("kept", xValue);
	xValue.SetInt32(7);
	xNew.SetValue("typeChanged", xValue);	// int now - float must be DROPPED, not reinterpreted
	xValue.SetFloat(1.0f);
	xNew.SetValue("added", xValue);

	const u_int uDropped = xNew.CopyMatchingFrom(xOld);
	ZENITH_ASSERT_EQ(uDropped, 2u);	// typeChanged + removed
	ZENITH_ASSERT_EQ(xNew.GetInt32("kept"), 42);
	ZENITH_ASSERT_EQ(xNew.GetInt32("typeChanged"), 7);	// new declaration default kept
	ZENITH_ASSERT_EQ_FLOAT(xNew.GetFloat("added"), 1.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(xNew.HasValue("removed"));
}

ZENITH_TEST(BehaviourGraph, CorruptDefinitionRejected)
{
	Zenith_DataStream xStream;
	const u_int uGarbage = 0x11223344u;
	xStream << uGarbage;
	xStream << uGarbage;
	xStream.SetCursor(0);

	Zenith_GraphDefinition xDef;
	ZENITH_ASSERT_FALSE(xDef.ReadFromDataStream(xStream));
	ZENITH_ASSERT_EQ(xDef.GetNodeCount(), 0u);
}

#endif // ZENITH_TESTING
