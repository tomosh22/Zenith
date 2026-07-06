//------------------------------------------------------------------------------
// Behaviour Graph runtime unit tests (Phase 1 of the Behaviour Graphs program).
// Included at the bottom of Zenith_BehaviourGraph.cpp.
//
// These tests register their own scratch node types directly with the registry
// (they never disturb the engine registrar - the registry is already
// initialized by the time tests run, and scratch types use Test-prefixed names).
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Scripting/Zenith_GraphBuilder.h"

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

	// Shared log the lifecycle probe appends to: "+t" OnEnter, "*t" Execute,
	// "-t" OnExit, "!t" OnAbort (t = the node's configured tag). Tests reset it.
	std::string g_strLifecycleLog;

	// Lifecycle probe: RUNNING for m_iRunningTicks executes (0 = immediate
	// SUCCESS), logging every hook. OnAbort resets the tick counter - the
	// per-run-state-reset contract every suspended node must honour.
	class GraphTestLifecycleProbe : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphTestLifecycleProbe)
	public:
		ZENITH_PROPERTY(std::string, m_strTag, "n")
		ZENITH_PROPERTY(int32_t, m_iRunningTicks, 0)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			g_strLifecycleLog += "*" + m_strTag;
			if (m_iElapsed + 1 < m_iRunningTicks || m_iRunningTicks < 0)
			{
				++m_iElapsed;
				return GRAPH_NODE_STATUS_RUNNING;
			}
			m_iElapsed = 0;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		void OnEnter(Zenith_GraphContext&) override { g_strLifecycleLog += "+" + m_strTag; }
		void OnExit(Zenith_GraphContext&) override { g_strLifecycleLog += "-" + m_strTag; }
		void OnAbort(Zenith_GraphContext&) override
		{
			g_strLifecycleLog += "!" + m_strTag;
			m_iElapsed = 0;
		}
		const char* GetTypeName() const override { return "Test_LifecycleProbe"; }

	private:
		int32_t m_iElapsed = 0;
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
		xRegistry.RegisterNodeType<GraphTestLifecycleProbe>("Test_LifecycleProbe", GRAPH_EVENT_NONE, 1, false, "Test");
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

ZENITH_TEST(BehaviourGraph, BlackboardTypedGetters)
{
	Zenith_GraphBlackboard xBlackboard;
	Zenith_PropertyValue xValue;

	xValue.SetVector2(Zenith_Maths::Vector2(1.0f, 2.0f));
	xBlackboard.SetValue("v2", xValue);
	xValue.SetVector3(Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f));
	xBlackboard.SetValue("v3", xValue);
	xValue.SetVector4(Zenith_Maths::Vector4(6.0f, 7.0f, 8.0f, 9.0f));
	xBlackboard.SetValue("v4", xValue);
	xValue.SetString("hello");
	xBlackboard.SetValue("s", xValue);
	xValue.SetPackedEntityID(0x1234567800000042ull);
	xBlackboard.SetValue("e", xValue);

	// Present + correct type.
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector2("v2").y, 2.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("v3").z, 5.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector4("v4").w, 9.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xBlackboard.GetString("s").c_str(), "hello");
	ZENITH_ASSERT_EQ(xBlackboard.GetPackedEntityID("e"), 0x1234567800000042ull);

	// Missing -> caller default.
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("missing", Zenith_Maths::Vector3(-1.0f)).x, -1.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xBlackboard.GetString("missing", "fallback").c_str(), "fallback");
	ZENITH_ASSERT_EQ(xBlackboard.GetPackedEntityID("missing", 7ull), 7ull);

	// Present but wrong type -> default, stored value untouched.
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("s", Zenith_Maths::Vector3(-2.0f)).x, -2.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xBlackboard.GetString("v3", "typed").c_str(), "typed");
	ZENITH_ASSERT_EQ(xBlackboard.GetPackedEntityID("v2", 9ull), 9ull);
	ZENITH_ASSERT_STREQ(xBlackboard.GetString("s").c_str(), "hello");

	// Migration carries the richer types name+type-matched.
	Zenith_GraphBlackboard xNew;
	xValue.SetVector3(Zenith_Maths::Vector3(0.0f));
	xNew.SetValue("v3", xValue);
	xValue.SetString("");
	xNew.SetValue("s", xValue);
	xValue.SetPackedEntityID(0);
	xNew.SetValue("e", xValue);
	const u_int uDropped = xNew.CopyMatchingFrom(xBlackboard);
	ZENITH_ASSERT_EQ(uDropped, 2u);	// v2 + v4 not declared in xNew
	ZENITH_ASSERT_EQ_FLOAT(xNew.GetVector3("v3").y, 4.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xNew.GetString("s").c_str(), "hello");
	ZENITH_ASSERT_EQ(xNew.GetPackedEntityID("e"), 0x1234567800000042ull);
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

namespace
{
	// Shared builder for the flow-node tests: OnUpdate source -> (built by the
	// caller). Returns the source node ID.
	u_int BuildProbe(Zenith_GraphDefinition& xDef, u_int uAnchorID, u_int uPin, const char* szTag, int32_t iRunningTicks)
	{
		const u_int uProbe = xDef.AddNode("Test_LifecycleProbe");
		GraphTestLifecycleProbe xTemp;
		xTemp.m_strTag = szTag;
		xTemp.m_iRunningTicks = iRunningTicks;
		xDef.SetNodeParamsFromInstance(uProbe, &xTemp);
		xDef.AddEdge(uAnchorID, uPin, uProbe, 0);
		return uProbe;
	}
}

ZENITH_TEST(BehaviourGraph, LifecycleEnterExitOrderingAndResume)
{
	EnsureTestNodesRegistered();

	// a completes immediately; b suspends for 2 ticks. One OnEnter per run of
	// a node, no OnExit on RUNNING, no re-OnEnter on resume.
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uA = BuildProbe(xDef, uSource, 0, "a", 0);
	BuildProbe(xDef, uA, 0, "b", 2);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+a*a-a+b*b");	// b RUNNING: no -b
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+a*a-a+b*b*b-b");	// resume: no +b, exit on completion
}

ZENITH_TEST(BehaviourGraph, AbortChainResetsSuspendedNode)
{
	EnsureTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	BuildProbe(xDef, uSource, 0, "w", 2);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);	// tick 1 of 2 -> RUNNING
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+w*w");

	xGraph.AbortChain(uSource, 0, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+w*w!w");

	// Idempotent: nothing suspended now.
	xGraph.AbortChain(uSource, 0, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+w*w!w");

	// Fresh run restarts from the chain head with reset per-run state: it must
	// take 2 more ticks (an unreset counter would complete on the first).
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+w*w!w+w*w");
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+w*w!w+w*w*w-w");
}

ZENITH_TEST(BehaviourGraph, SelectorPriorityAndReactivePreemption)
{
	EnsureTestNodesRegistered();

	// pin 0 (high): Gate("open") -> probe hi. pin 1 (low): probe lo, long-RUNNING.
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uSelector = xDef.AddNode("Selector");
	xDef.AddEdge(uSource, 0, uSelector, 0);
	const u_int uGate = xDef.AddNode("Gate");	// engine node: FAILURE while "open" false
	xDef.AddEdge(uSelector, 0, uGate, 0);
	BuildProbe(xDef, uGate, 0, "h", 0);
	BuildProbe(xDef, uSelector, 1, "l", -1);	// RUNNING forever

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	// Gate closed: pin 0 fails, pin 1 runs + suspends.
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+l*l");

	// Still closed: reactive selector re-scans pin 0 (fails again), resumes pin 1.
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+l*l*l");

	// Gate opens: higher-priority branch wins and the RUNNING pin-1 chain is
	// preempted - OnAbort fires on the suspended probe.
	Zenith_PropertyValue xOpen;
	xOpen.SetBool(true);
	xGraph.GetBlackboard().SetValue("open", xOpen);
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+h*h-h!l");

	// Gate closes again: pin 1 starts FRESH (OnEnter again, tick state reset).
	xOpen.SetBool(false);
	xGraph.GetBlackboard().SetValue("open", xOpen);
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+l*l");
}

ZENITH_TEST(BehaviourGraph, SwitchOnIntRoutesCasesAndDefault)
{
	EnsureTestNodesRegistered();

	// 3 cases (base 10) + default pin 3.
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uSwitch = xDef.AddNode("SwitchOnInt");
	{
		Zenith_GraphNode* pxTemp = Zenith_GraphNodeRegistry::Get().Find("SwitchOnInt")->m_pfnCreate();
		Zenith_PropertyValue xValue;
		xValue.SetString("sel");
		Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("SwitchOnInt")->m_pfnGetPropertyTable()->FindProperty("m_strVar"), xValue);
		xValue.SetInt32(10);
		Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("SwitchOnInt")->m_pfnGetPropertyTable()->FindProperty("m_iCaseBase"), xValue);
		xValue.SetInt32(3);
		Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("SwitchOnInt")->m_pfnGetPropertyTable()->FindProperty("m_iCaseCount"), xValue);
		xDef.SetNodeParamsFromInstance(uSwitch, pxTemp);
		// Configured instance reports its real pin count (3 cases + default).
		ZENITH_ASSERT_EQ(pxTemp->GetDynamicExecOutputCount(), 4);
		delete pxTemp;
	}
	xDef.AddEdge(uSource, 0, uSwitch, 0);
	BuildProbe(xDef, uSwitch, 0, "0", 0);
	BuildProbe(xDef, uSwitch, 1, "1", 0);
	BuildProbe(xDef, uSwitch, 2, "2", 0);
	BuildProbe(xDef, uSwitch, 3, "d", 0);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	Zenith_PropertyValue xSel;
	xSel.SetInt32(11);	// case 1
	xGraph.GetBlackboard().SetValue("sel", xSel);
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+1*1-1");

	xSel.SetInt32(99);	// out of range -> default
	xGraph.GetBlackboard().SetValue("sel", xSel);
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+d*d-d");
}

ZENITH_TEST(BehaviourGraph, StateMachineTransitionAbortsOldState)
{
	EnsureTestNodesRegistered();

	// 2 states: state 0 body long-RUNNING; state 1 body immediate.
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uMachine = xDef.AddNode("StateMachine");
	{
		Zenith_GraphNode* pxTemp = Zenith_GraphNodeRegistry::Get().Find("StateMachine")->m_pfnCreate();
		Zenith_PropertyValue xValue;
		xValue.SetString("st");
		Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("StateMachine")->m_pfnGetPropertyTable()->FindProperty("m_strStateVar"), xValue);
		xValue.SetInt32(2);
		Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("StateMachine")->m_pfnGetPropertyTable()->FindProperty("m_iStateCount"), xValue);
		xDef.SetNodeParamsFromInstance(uMachine, pxTemp);
		delete pxTemp;
	}
	xDef.AddEdge(uSource, 0, uMachine, 0);
	BuildProbe(xDef, uMachine, 0, "s0", -1);	// RUNNING forever
	BuildProbe(xDef, uMachine, 1, "s1", 0);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	// st defaults 0: body 0 runs + suspends; stays suspended across fires.
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+s0*s0*s0");

	// Transition 0 -> 1: old state's RUNNING body aborted, new body runs.
	Zenith_PropertyValue xState;
	xState.SetInt32(1);
	xGraph.GetBlackboard().SetValue("st", xState);
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "!s0+s1*s1-s1");

	// Back to 0: fresh entry (reset tick state).
	xState.SetInt32(0);
	xGraph.GetBlackboard().SetValue("st", xState);
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+s0*s0");
}

ZENITH_TEST(BehaviourGraph, RepeatTickedIterationsAndUntilFailure)
{
	EnsureTestNodesRegistered();

	// Counted: 2 body iterations (one per fire), then the done chain.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("Test_OnUpdate");
		const u_int uRepeat = xDef.AddNode("Repeat");
		{
			Zenith_GraphNode* pxTemp = Zenith_GraphNodeRegistry::Get().Find("Repeat")->m_pfnCreate();
			Zenith_PropertyValue xValue;
			xValue.SetInt32(2);
			Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("Repeat")->m_pfnGetPropertyTable()->FindProperty("m_iCount"), xValue);
			xDef.SetNodeParamsFromInstance(uRepeat, pxTemp);
			delete pxTemp;
		}
		xDef.AddEdge(uSource, 0, uRepeat, 0);
		BuildProbe(xDef, uRepeat, 0, "b", 0);
		BuildProbe(xDef, uRepeat, 1, "d", 0);

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		Zenith_GraphContext xContext = MakeTestContext(xGraph);

		g_strLifecycleLog.clear();
		xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);	// iteration 1
		ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+b*b-b");
		xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);	// iteration 2 -> done
		ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+b*b-b+b*b-b+d*d-d");
	}

	// Until-failure: body = Gate("open"); closing the gate completes the repeat.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("Test_OnUpdate");
		const u_int uRepeat = xDef.AddNode("Repeat");
		{
			Zenith_GraphNode* pxTemp = Zenith_GraphNodeRegistry::Get().Find("Repeat")->m_pfnCreate();
			Zenith_PropertyValue xValue;
			xValue.SetBool(true);
			Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("Repeat")->m_pfnGetPropertyTable()->FindProperty("m_bUntilFailure"), xValue);
			xDef.SetNodeParamsFromInstance(uRepeat, pxTemp);
			delete pxTemp;
		}
		xDef.AddEdge(uSource, 0, uRepeat, 0);
		const u_int uGate = xDef.AddNode("Gate");
		xDef.AddEdge(uRepeat, 0, uGate, 0);
		BuildProbe(xDef, uGate, 0, "b", 0);
		BuildProbe(xDef, uRepeat, 1, "d", 0);

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		Zenith_GraphContext xContext = MakeTestContext(xGraph);
		Zenith_PropertyValue xOpen;
		xOpen.SetBool(true);
		xGraph.GetBlackboard().SetValue("open", xOpen);

		g_strLifecycleLog.clear();
		xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
		xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
		ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+b*b-b+b*b-b");	// iterating

		xOpen.SetBool(false);
		xGraph.GetBlackboard().SetValue("open", xOpen);
		g_strLifecycleLog.clear();
		xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
		ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+d*d-d");	// body failed -> done chain
	}
}

ZENITH_TEST(BehaviourGraph, CooldownGatesOnContextTime)
{
	EnsureTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uCooldown = xDef.AddNode("Cooldown");	// 1.0 s default
	xDef.AddEdge(uSource, 0, uCooldown, 0);
	BuildProbe(xDef, uCooldown, 0, "c", 0);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	g_strLifecycleLog.clear();
	xContext.m_fTimeSeconds = 100.0f;	// starts ready
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	xContext.m_fTimeSeconds = 100.5f;	// gated
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	xContext.m_fTimeSeconds = 101.0f;	// exactly one interval - passes
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	xContext.m_fTimeSeconds = 101.2f;	// gated again
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+c*c-c+c*c-c");
}

ZENITH_TEST(BehaviourGraph, WaitForConditionSuspendsUntilTrue)
{
	EnsureTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uWait = xDef.AddNode("WaitForCondition");	// var "ready", no reset
	xDef.AddEdge(uSource, 0, uWait, 0);
	BuildProbe(xDef, uWait, 0, "g", 0);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "");	// suspended before the probe

	Zenith_PropertyValue xReady;
	xReady.SetBool(true);
	xGraph.GetBlackboard().SetValue("ready", xReady);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+g*g-g");
}

ZENITH_TEST(BehaviourGraph, NeedsUpdateDispatchGating)
{
	EnsureTestNodesRegistered();

	// A graph anchored only on a custom event is idle for ON_UPDATE dispatch -
	// until one of its chains suspends, then it needs resumption driving.
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_CustomSource");	// matches "evt"
	BuildProbe(xDef, uSource, 0, "w", 3);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	ZENITH_ASSERT_FALSE(xGraph.NeedsUpdateDispatch());

	Zenith_GraphContext xContext = MakeTestContext(xGraph);
	xGraph.FireCustomEvent("evt", xContext);	// suspends (3-tick probe)
	ZENITH_ASSERT_TRUE(xGraph.NeedsUpdateDispatch());

	// ON_UPDATE resumption drains it back to idle.
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_FALSE(xGraph.NeedsUpdateDispatch());

	// Graphs WITH periodic sources always need the dispatch.
	Zenith_GraphDefinition xTickingDef;
	xTickingDef.AddNode("Test_OnUpdate");
	Zenith_BehaviourGraph xTicking;
	ZENITH_ASSERT_TRUE(xTicking.InitialiseFromDefinition(xTickingDef));
	ZENITH_ASSERT_TRUE(xTicking.NeedsUpdateDispatch());
}

ZENITH_TEST(BehaviourGraph, BlackboardListsStoreSerializationAndMigration)
{
	Zenith_GraphBlackboard xBlackboard;
	Zenith_PropertyValue xValue;
	xValue.SetFloat(1.5f);
	xBlackboard.SetValue("plain", xValue);

	Zenith_Vector<Zenith_PropertyValue>& axList = xBlackboard.GetOrCreateList("ids");
	xValue.SetPackedEntityID(11);
	axList.PushBack(xValue);
	xValue.SetPackedEntityID(22);
	axList.PushBack(xValue);
	ZENITH_ASSERT_EQ(xBlackboard.GetListCount(), 1u);
	ZENITH_ASSERT_NULL(xBlackboard.TryGetList("missing"));

	// Full round-trip (values + list section).
	Zenith_DataStream xStream;
	xBlackboard.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	Zenith_GraphBlackboard xLoaded;
	xLoaded.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetFloat("plain"), 1.5f, 0.0001f);
	const Zenith_Vector<Zenith_PropertyValue>* pxLoadedList = xLoaded.TryGetList("ids");
	ZENITH_ASSERT_NOT_NULL(pxLoadedList);
	if (!pxLoadedList) return;
	ZENITH_ASSERT_EQ(pxLoadedList->GetSize(), 2u);
	ZENITH_ASSERT_EQ(pxLoadedList->Get(1).GetPackedEntityID(), 22ull);

	// Pre-list-era stream (values only, the v1 override-blob shape): parses
	// cleanly with bWithLists = false and yields no lists.
	Zenith_DataStream xOldStream;
	const u_int uOldCount = 1;
	xOldStream << uOldCount;
	xOldStream << std::string("plain");
	xValue.SetFloat(9.0f);
	xOldStream << xValue;
	xOldStream.SetCursor(0);
	Zenith_GraphBlackboard xOld;
	xOld.ReadFromDataStream(xOldStream, false);
	ZENITH_ASSERT_EQ_FLOAT(xOld.GetFloat("plain"), 9.0f, 0.0001f);
	ZENITH_ASSERT_EQ(xOld.GetListCount(), 0u);

	// Hot-reload migration carries lists verbatim.
	Zenith_GraphBlackboard xMigrated;
	xMigrated.CopyMatchingFrom(xBlackboard);
	ZENITH_ASSERT_NOT_NULL(xMigrated.TryGetList("ids"));

	// Clear drops lists too.
	xBlackboard.Clear();
	ZENITH_ASSERT_EQ(xBlackboard.GetListCount(), 0u);
}

ZENITH_TEST(BehaviourGraph, ForEachIteratesSuspendsAndFails)
{
	EnsureTestNodesRegistered();

	// Body probe suspends 2 ticks per element - proves per-element resumption.
	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_OnUpdate");
	const u_int uForEach = xDef.AddNode("ForEach");
	{
		Zenith_GraphNode* pxTemp = Zenith_GraphNodeRegistry::Get().Find("ForEach")->m_pfnCreate();
		Zenith_PropertyValue xValue;
		xValue.SetString("idx");
		Zenith_PropertySystem::SetPropertyValue(pxTemp, *Zenith_GraphNodeRegistry::Get().Find("ForEach")->m_pfnGetPropertyTable()->FindProperty("m_strIndexVar"), xValue);
		xDef.SetNodeParamsFromInstance(uForEach, pxTemp);
		delete pxTemp;
	}
	xDef.AddEdge(uSource, 0, uForEach, 0);
	BuildProbe(xDef, uForEach, 0, "b", 2);
	BuildProbe(xDef, uForEach, 1, "d", 0);

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext = MakeTestContext(xGraph);

	// Empty/absent list -> straight to the done chain.
	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+d*d-d");

	// Two elements, 2-tick body: element 0 spans fires 1-2, element 1 spans
	// fires 2-3 (the loop continues within a fire when a body completes).
	Zenith_Vector<Zenith_PropertyValue>& axList = xGraph.GetBlackboard().GetOrCreateList("list");
	Zenith_PropertyValue xElement;
	xElement.SetInt32(100);
	axList.PushBack(xElement);
	xElement.SetInt32(200);
	axList.PushBack(xElement);

	g_strLifecycleLog.clear();
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);	// element 0 tick 1
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+b*b");
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("item"), 100);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("idx"), 0);

	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);	// element 0 done, element 1 tick 1
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+b*b*b-b+b*b");
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("item"), 200);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("idx"), 1);

	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);	// element 1 done -> done chain
	ZENITH_ASSERT_STREQ(g_strLifecycleLog.c_str(), "+b*b*b-b+b*b*b-b+d*d-d");
}

ZENITH_TEST(BehaviourGraph, BuilderRoundTripEquivalence)
{
	EnsureTestNodesRegistered();

	// Author with the fluent builder, round-trip through serialization, run -
	// the built graph must behave exactly like a hand-authored one.
	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		Zenith_PropertyValue xSeed;
		xSeed.SetFloat(1.5f);
		xBuilder.Variable("seed", xSeed);

		const u_int uSource = xBuilder.Node("Test_OnUpdate");
		const u_int uCounter = xBuilder.Node("Test_Counter");
		xBuilder.ParamString(uCounter, "m_strCounterName", "built")
			.Chain(uSource, uCounter);
		ZENITH_ASSERT_TRUE(xBuilder.Build());
		ZENITH_ASSERT_FALSE(xBuilder.HasErrors());
	}

	// Editor layout assigned for every node.
	for (u_int u = 0; u < xDef.GetNodeCount(); ++u)
	{
		Zenith_Maths::Vector2 xPos;
		ZENITH_ASSERT_TRUE(xDef.GetNodeEditorPos(xDef.GetNodeAt(u).m_uNodeID, xPos));
	}

	Zenith_DataStream xStream;
	xDef.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	Zenith_GraphDefinition xLoaded;
	ZENITH_ASSERT_TRUE(xLoaded.ReadFromDataStream(xStream));

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xLoaded));
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
	ZENITH_ASSERT_EQ_FLOAT(xGraph.GetBlackboard().GetFloat("seed"), 1.5f, 0.0001f);

	Zenith_GraphContext xContext = MakeTestContext(xGraph);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	ZENITH_ASSERT_EQ(xGraph.GetBlackboard().GetInt32("built"), 1);
}

ZENITH_TEST(BehaviourGraph, BuilderParamTypesReachInstances)
{
	// Every Param* helper writes through the registry property table into the
	// engine's P1a blackboard-set node family; firing the chain proves the
	// params landed in the blobs.
	Zenith_GraphDefinition xDef;
	Zenith_GraphBuilder xBuilder(xDef);
	const u_int uSource = xBuilder.Node("Test_OnUpdate");
	const u_int uFloat = xBuilder.Node("SetBlackboardFloat");
	const u_int uBool = xBuilder.Node("SetBlackboardBool");
	const u_int uVec = xBuilder.Node("SetBlackboardVector3");
	const u_int uStr = xBuilder.Node("SetBlackboardString");
	const u_int uInt = xBuilder.Node("SetBlackboardInt");

	xBuilder.ParamString(uFloat, "m_strVariable", "f").ParamFloat(uFloat, "m_fValue", 2.5f)
		.ParamString(uBool, "m_strVariable", "b").ParamBool(uBool, "m_bValue", false)
		.ParamString(uVec, "m_strVariable", "v").ParamVec3(uVec, "m_xValue", Zenith_Maths::Vector3(9.0f, 8.0f, 7.0f))
		.ParamString(uStr, "m_strVariable", "s").ParamString(uStr, "m_strValue", "built")
		.ParamString(uInt, "m_strVariable", "i").ParamInt(uInt, "m_iValue", 42)
		.Chain(uSource, uFloat).Chain(uFloat, uBool).Chain(uBool, uVec).Chain(uVec, uStr).Chain(uStr, uInt);
	ZENITH_ASSERT_TRUE(xBuilder.Build());

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
	Zenith_GraphContext xContext = MakeTestContext(xGraph);
	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);

	const Zenith_GraphBlackboard& xBlackboard = xGraph.GetBlackboard();
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("f"), 2.5f, 0.0001f);
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("b", true));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("v").x, 9.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xBlackboard.GetString("s").c_str(), "built");
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("i"), 42);
}

ZENITH_TEST(BehaviourGraph, BuilderErrorLatching)
{
	EnsureTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	Zenith_GraphBuilder xBuilder(xDef);

	// Unknown type: ID 0 + latched; downstream calls on the 0 ID stay safe.
	const u_int uBad = xBuilder.Node("Test_DoesNotExist");
	ZENITH_ASSERT_EQ(uBad, 0u);
	ZENITH_ASSERT_TRUE(xBuilder.HasErrors());
	xBuilder.ParamFloat(uBad, "m_fValue", 1.0f);
	xBuilder.Chain(uBad, uBad);

	// Unknown property on a valid node latches too.
	const u_int uCounter = xBuilder.Node("Test_Counter");
	ZENITH_ASSERT_NE(uCounter, 0u);
	xBuilder.ParamFloat(uCounter, "m_fNoSuchProperty", 1.0f);

	// Duplicate outgoing edge on the same (node, pin) is rejected + latched.
	const u_int uSource = xBuilder.Node("Test_OnUpdate");
	xBuilder.Chain(uSource, uCounter);
	const u_int uCounter2 = xBuilder.Node("Test_Counter");
	xBuilder.Chain(uSource, uCounter2);

	ZENITH_ASSERT_FALSE(xBuilder.Build());
}

ZENITH_TEST(BehaviourGraph, BuilderParamEqualToDefaultIsLegalNoOp)
{
	EnsureTestNodesRegistered();

	// Setting a param to a value equal to its declared default is a legal
	// authoring no-op, NOT an error (SetPropertyValue reports no-change with
	// the same false a type mismatch uses - the builder must distinguish;
	// regression: W1 Test conversion set m_iSlot=0 / m_fDelta=default and
	// the whole GraphBuild step asserted).
	Zenith_GraphDefinition xDef;
	Zenith_GraphBuilder xBuilder(xDef);
	const u_int uSource = xBuilder.Node("Test_OnUpdate");
	const u_int uCounter = xBuilder.Node("Test_Counter");
	xBuilder.ParamString(uCounter, "m_strCounterName", "count");	// == default
	xBuilder.ParamInt(uCounter, "m_iReturnStatus", 0);				// == default
	xBuilder.Chain(uSource, uCounter);
	ZENITH_ASSERT_FALSE(xBuilder.HasErrors());
	ZENITH_ASSERT_TRUE(xBuilder.Build());

	// A genuine type mismatch still latches.
	Zenith_GraphDefinition xBadDef;
	Zenith_GraphBuilder xBadBuilder(xBadDef);
	const u_int uBadCounter = xBadBuilder.Node("Test_Counter");
	xBadBuilder.ParamFloat(uBadCounter, "m_iReturnStatus", 1.0f);	// float into int32
	ZENITH_ASSERT_TRUE(xBadBuilder.HasErrors());
}

#endif // ZENITH_TESTING
