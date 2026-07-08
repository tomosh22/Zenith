#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_GraphReload.h"
#endif

#include <cfloat>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - Flow domain (state machines + BT
// parity).
//
// These are the multi-way / preemptive flow constructs built on the chain
// lifecycle primitives (OnEnter/OnExit/OnAbort + Zenith_BehaviourGraph::
// AbortChain). Dynamic pin counts come from GetDynamicExecOutputCount (pin
// index <= 255 by the chain-cursor key layout).
//
// BT-parity notes:
//   - Sequence is deliberately ABSENT: a linear exec chain IS a sequence
//     (SUCCESS auto-continues, FAILURE aborts, RUNNING resumes at the node).
//   - Selector + the reactive StateMachine are the two preemption constructs;
//     both abort a RUNNING lower-priority/old-state body via AbortChain, which
//     cascades OnAbort through suspended nodes (per-run state resets, nav
//     stops, ...).
//------------------------------------------------------------------------------

namespace
{
	// Shared: split a comma-separated property into a token list.
	void ParseCommaList(const std::string& strList, Zenith_Vector<std::string>& axOut)
	{
		axOut.Clear();
		size_t uStart = 0;
		while (uStart <= strList.size())
		{
			size_t uComma = strList.find(',', uStart);
			if (uComma == std::string::npos)
			{
				uComma = strList.size();
			}
			if (uComma > uStart)
			{
				axOut.PushBack(strList.substr(uStart, uComma - uStart));
			}
			if (uComma == strList.size())
			{
				break;
			}
			uStart = uComma + 1;
		}
	}

	//==========================================================================
	// SwitchOnInt - 1-of-N dispatch on an int32 blackboard variable.
	// Pin i = value (m_iCaseBase + i); pin m_iCaseCount = default.
	//==========================================================================
	class Zenith_GraphNode_SwitchOnInt : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SwitchOnInt)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(int32_t, m_iCaseBase, 0)
		ZENITH_PROPERTY_RANGED(int32_t, m_iCaseCount, 4, 1, 254)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// While a taken case is suspended, keep re-driving THAT pin (the
			// Branch pattern).
			if (m_iActivePin < 0)
			{
				const int32_t iValue = xContext.m_pxBlackboard->GetInt32(m_strVar);
				const int32_t iCase = iValue - m_iCaseBase;
				m_iActivePin = (iCase >= 0 && iCase < m_iCaseCount) ? iCase : m_iCaseCount;
			}
			const GraphNodeStatus eStatus = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), static_cast<u_int>(m_iActivePin), xContext);
			if (eStatus != GRAPH_NODE_STATUS_RUNNING)
			{
				m_iActivePin = -1;
			}
			return eStatus;
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			if (m_iActivePin >= 0)
			{
				xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iActivePin), xContext);
				m_iActivePin = -1;
			}
		}
		int32_t GetDynamicExecOutputCount() const override { return m_iCaseCount + 1; }
		const char* GetTypeName() const override { return "SwitchOnInt"; }

	private:
		int32_t m_iActivePin = -1;
	};

	//==========================================================================
	// SwitchOnString - 1-of-N dispatch on a string blackboard variable.
	// Pin i = i-th name in the comma-separated case list; last pin = default.
	//==========================================================================
	class Zenith_GraphNode_SwitchOnString : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SwitchOnString)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "state")
		ZENITH_PROPERTY(std::string, m_strCases, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			EnsureCasesParsed();
			if (m_iActivePin < 0)
			{
				const std::string strValue = xContext.m_pxBlackboard->GetString(m_strVar);
				int32_t iPin = static_cast<int32_t>(m_axCases.GetSize());	// default
				for (u_int u = 0; u < m_axCases.GetSize(); ++u)
				{
					if (m_axCases.Get(u) == strValue)
					{
						iPin = static_cast<int32_t>(u);
						break;
					}
				}
				m_iActivePin = iPin;
			}
			const GraphNodeStatus eStatus = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), static_cast<u_int>(m_iActivePin), xContext);
			if (eStatus != GRAPH_NODE_STATUS_RUNNING)
			{
				m_iActivePin = -1;
			}
			return eStatus;
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			if (m_iActivePin >= 0)
			{
				xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iActivePin), xContext);
				m_iActivePin = -1;
			}
		}
		int32_t GetDynamicExecOutputCount() const override
		{
			EnsureCasesParsed();
			return static_cast<int32_t>(m_axCases.GetSize()) + 1;
		}
		const char* GetTypeName() const override { return "SwitchOnString"; }

	private:
		void EnsureCasesParsed() const
		{
			if (!m_bCasesParsed)
			{
				ParseCommaList(m_strCases, m_axCases);
				m_bCasesParsed = true;
			}
		}

		int32_t m_iActivePin = -1;
		mutable Zenith_Vector<std::string> m_axCases;
		mutable bool m_bCasesParsed = false;
	};

	//==========================================================================
	// StateMachine - variable-keyed reactive 1-of-N dispatcher. Pin i = state i;
	// the state variable is the single source of truth (any node/shim writing
	// it causes a transition on the next fire). On transition the old state's
	// RUNNING body is aborted (OnAbort cascade), then optional custom events
	// "<prefix>Exit_<old>" / "<prefix>Enter_<new>" fire on the host entity's
	// GraphComponent so enter/exit chains are fully graph-authorable. No exit
	// event fires when the machine itself is aborted (deterministic teardown).
	//==========================================================================
	class Zenith_GraphNode_StateMachine : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_StateMachine)
	public:
		ZENITH_PROPERTY(std::string, m_strStateVar, "state")
		ZENITH_PROPERTY_RANGED(int32_t, m_iStateCount, 4, 1, 255)
		ZENITH_PROPERTY(std::string, m_strStateNames, "")
		ZENITH_PROPERTY(std::string, m_strEventPrefix, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			int32_t iState = xContext.m_pxBlackboard->GetInt32(m_strStateVar, 0);
			iState = iState < 0 ? 0 : (iState >= m_iStateCount ? m_iStateCount - 1 : iState);

			if (iState != m_iCurrentState)
			{
				if (m_iCurrentState >= 0)
				{
					xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iCurrentState), xContext);
					FireTransitionEvent(xContext, "Exit_", m_iCurrentState);
				}
				FireTransitionEvent(xContext, "Enter_", iState);
				m_iCurrentState = iState;
			}
			// The state body is a per-fire chain; RUNNING bodies resume through
			// the normal cursor, and because this node then returns RUNNING too,
			// the machine re-executes every fire - transitions stay reactive.
			return xContext.m_pxGraph->RunChainFromPin(GetNodeID(), static_cast<u_int>(m_iCurrentState), xContext);
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			if (m_iCurrentState >= 0)
			{
				xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iCurrentState), xContext);
				m_iCurrentState = -1;
			}
		}
		int32_t GetDynamicExecOutputCount() const override { return m_iStateCount; }
		const char* GetTypeName() const override { return "StateMachine"; }

	private:
		void FireTransitionEvent(Zenith_GraphContext& xContext, const char* szKind, int32_t iState)
		{
			if (m_strEventPrefix.empty() || !xContext.m_xSelf.IsValid())
			{
				return;
			}
			Zenith_GraphComponent* pxComponent = xContext.m_xSelf.TryGetComponent<Zenith_GraphComponent>();
			if (pxComponent == nullptr)
			{
				return;
			}
			EnsureNamesParsed();
			std::string strEvent = m_strEventPrefix;
			strEvent += szKind;
			if (iState >= 0 && static_cast<u_int>(iState) < m_axNames.GetSize())
			{
				strEvent += m_axNames.Get(static_cast<u_int>(iState));
			}
			else
			{
				strEvent += std::to_string(iState);
			}
			// Reentrant dispatch is supported (depth-counted) - the enter/exit
			// chains run synchronously at the transition point.
			pxComponent->FireCustomEvent(strEvent.c_str());
		}
		void EnsureNamesParsed()
		{
			if (!m_bNamesParsed)
			{
				ParseCommaList(m_strStateNames, m_axNames);
				m_bNamesParsed = true;
			}
		}

		int32_t m_iCurrentState = -1;
		Zenith_Vector<std::string> m_axNames;
		bool m_bNamesParsed = false;
	};

	//==========================================================================
	// Selector - BT priority selector over N branch pins (pin 0 = highest).
	// Reactive (default): re-scans from pin 0 every fire, so a higher-priority
	// branch coming true preempts a RUNNING lower branch (aborted via
	// AbortChain -> OnAbort cascade). Non-reactive: a RUNNING branch resumes
	// in place without re-evaluating higher pins (BT memory-selector).
	//==========================================================================
	class Zenith_GraphNode_Selector : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Selector)
	public:
		ZENITH_PROPERTY_RANGED(int32_t, m_iBranchCount, 2, 1, 255)
		ZENITH_PROPERTY(bool, m_bReactive, true)
		// When false, a preempted lower-priority branch is NOT aborted: its
		// cursor + per-node state stay suspended and resume (stale) when the
		// selector falls back to it. This is the BT memory-composite parity
		// mode - crucial when branches drive a SHARED effector (one nav
		// agent): the preempting branch's side effects (SetDestination) run
		// BEFORE the abort would, so an OnAbort->Stop() on the old branch
		// clobbers the new branch's work and ping-pongs the selector
		// (priest W3, risk R3). Default true = preemption aborts (the safe
		// semantics when branches own independent state).
		ZENITH_PROPERTY(bool, m_bAbortPreempted, true)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			int32_t iScanStart = 0;
			if (!m_bReactive && m_iRunningPin >= 0)
			{
				iScanStart = m_iRunningPin;	// memory semantics: resume in place
			}

			for (int32_t iPin = iScanStart; iPin < m_iBranchCount; ++iPin)
			{
				const GraphNodeStatus eStatus = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), static_cast<u_int>(iPin), xContext);
				if (eStatus == GRAPH_NODE_STATUS_RUNNING)
				{
					if (m_iRunningPin >= 0 && m_iRunningPin != iPin && m_bAbortPreempted)
					{
						// A higher-priority branch took over - preempt the old one.
						xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iRunningPin), xContext);
					}
					m_iRunningPin = iPin;
					return GRAPH_NODE_STATUS_RUNNING;
				}
				if (eStatus == GRAPH_NODE_STATUS_SUCCESS)
				{
					if (m_iRunningPin >= 0 && m_iRunningPin != iPin && m_bAbortPreempted)
					{
						xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iRunningPin), xContext);
					}
					m_iRunningPin = -1;
					return GRAPH_NODE_STATUS_SUCCESS;
				}
				// FAILURE: a branch that was running has now finished.
				if (m_iRunningPin == iPin)
				{
					m_iRunningPin = -1;
				}
			}
			m_iRunningPin = -1;
			return GRAPH_NODE_STATUS_FAILURE;
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			if (m_iRunningPin >= 0)
			{
				xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iRunningPin), xContext);
				m_iRunningPin = -1;
			}
		}
		int32_t GetDynamicExecOutputCount() const override { return m_iBranchCount; }
		const char* GetTypeName() const override { return "Selector"; }

	private:
		int32_t m_iRunningPin = -1;
	};

	//==========================================================================
	// Repeat - ticked repetition of the body pin (0); done chain on pin 1.
	// One body iteration per fire (RUNNING between iterations) - unlike Loop,
	// which runs its N iterations synchronously in one fire. m_iCount -1 =
	// forever. m_bUntilFailure: body FAILURE = normal completion -> done pin.
	//==========================================================================
	class Zenith_GraphNode_Repeat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Repeat)
	public:
		ZENITH_PROPERTY_RANGED(int32_t, m_iCount, -1, -1, 100000)
		ZENITH_PROPERTY(bool, m_bUntilFailure, false)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_iRemaining == iUNSTARTED)
			{
				m_iRemaining = m_iCount;
			}

			const GraphNodeStatus eStatus = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 0, xContext);
			if (eStatus == GRAPH_NODE_STATUS_RUNNING)
			{
				return GRAPH_NODE_STATUS_RUNNING;	// body mid-iteration
			}
			if (eStatus == GRAPH_NODE_STATUS_FAILURE)
			{
				m_iRemaining = iUNSTARTED;
				if (m_bUntilFailure)
				{
					return xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 1, xContext);
				}
				return GRAPH_NODE_STATUS_FAILURE;
			}

			// Body SUCCESS = one iteration done.
			if (m_iRemaining > 0)
			{
				--m_iRemaining;
				if (m_iRemaining == 0)
				{
					m_iRemaining = iUNSTARTED;
					return xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 1, xContext);
				}
			}
			return GRAPH_NODE_STATUS_RUNNING;	// next iteration next fire
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			xContext.m_pxGraph->AbortChain(GetNodeID(), 0, xContext);
			m_iRemaining = iUNSTARTED;
		}
		const char* GetTypeName() const override { return "Repeat"; }

	private:
		static constexpr int32_t iUNSTARTED = -2;
		int32_t m_iRemaining = iUNSTARTED;
	};

	//==========================================================================
	// ForEach - iterates a blackboard LIST: body (pin 0) once per element with
	// the element (and optional index) published to blackboard vars; done chain
	// (pin 1) after the last element. Body RUNNING suspends at that element;
	// body FAILURE aborts the loop. Iterates by index, re-reading the list
	// every step - mutation-safe (never overruns a shrunk list).
	//==========================================================================
	class Zenith_GraphNode_ForEach : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ForEach)
	public:
		ZENITH_PROPERTY(std::string, m_strListVar, "list")
		ZENITH_PROPERTY(std::string, m_strElementVar, "item")
		ZENITH_PROPERTY(std::string, m_strIndexVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_iCursor < 0)
			{
				m_iCursor = 0;
			}
			while (true)
			{
				const Zenith_Vector<Zenith_PropertyValue>* pxList = xContext.m_pxBlackboard->TryGetList(m_strListVar);
				if (!pxList || m_iCursor >= static_cast<int32_t>(pxList->GetSize()))
				{
					m_iCursor = -1;
					return xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 1, xContext);
				}
				xContext.m_pxBlackboard->SetValue(m_strElementVar, pxList->Get(static_cast<u_int>(m_iCursor)));
				if (!m_strIndexVar.empty())
				{
					Zenith_PropertyValue xIndex;
					xIndex.SetInt32(m_iCursor);
					xContext.m_pxBlackboard->SetValue(m_strIndexVar, xIndex);
				}
				const GraphNodeStatus eStatus = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 0, xContext);
				if (eStatus == GRAPH_NODE_STATUS_RUNNING)
				{
					return GRAPH_NODE_STATUS_RUNNING;	// resume THIS element next fire
				}
				if (eStatus == GRAPH_NODE_STATUS_FAILURE)
				{
					m_iCursor = -1;
					return GRAPH_NODE_STATUS_FAILURE;
				}
				++m_iCursor;
			}
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			xContext.m_pxGraph->AbortChain(GetNodeID(), 0, xContext);
			m_iCursor = -1;
		}
		const char* GetTypeName() const override { return "ForEach"; }

	private:
		int32_t m_iCursor = -1;
	};

	//==========================================================================
	// Cooldown - gate: SUCCESS (chain continues) when at least m_fSeconds of
	// engine time passed since the last pass; FAILURE otherwise. Starts ready.
	// Uses context wall-clock, so it works under 0-dt anchors (custom events).
	//==========================================================================
	class Zenith_GraphNode_Cooldown : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Cooldown)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fSeconds, 1.0f, 0.0f, 3600.0f)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (xContext.m_fTimeSeconds - m_fLastPassTime < m_fSeconds)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			m_fLastPassTime = xContext.m_fTimeSeconds;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Cooldown"; }

	private:
		float m_fLastPassTime = -FLT_MAX;
	};

	//==========================================================================
	// CallGraph - runs another .bgraph asset's OnGraphCall entry chains as a
	// sub-graph (factoring reusable decision logic). Semantics:
	//   - SHARED parent blackboard: the child executes against the CALLER's
	//     blackboard (the child asset's declared variables double as its
	//     parameter list - defaults are seeded into the parent where absent).
	//   - Child node instances are cached per CallGraph node instance (their
	//     members are per-host per-call-site state), lazily built on first use.
	//   - RUNNING propagates: a suspended child chain suspends this node; the
	//     next fire resumes the child in place.
	//   - Recursion (self/mutual) is cut at depth 8 with an error + FAILURE.
	//   - TOOLS: a graph hot reload re-resolves the child on the next Execute
	//     (per-instance child node state drops; shared blackboard survives).
	//==========================================================================
	int g_iGraphCallDepth = 0;

	class Zenith_GraphNode_CallGraph : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CallGraph)
	public:
		ZENITH_PROPERTY(std::string, m_strGraphAssetPath, "")

		~Zenith_GraphNode_CallGraph() override
		{
			delete m_pxChild;
		}

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (g_iGraphCallDepth >= 8)
			{
				if (!m_bWarned)
				{
					m_bWarned = true;
					Zenith_Error(LOG_CATEGORY_CORE, "CallGraph: depth cap hit calling '%s' (recursive sub-graph?)", m_strGraphAssetPath.c_str());
				}
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (!EnsureChildResolved(xContext))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			// Child runs against the CALLER's blackboard (shared scope).
			Zenith_GraphContext xChildContext = xContext;
			xChildContext.m_pxGraph = m_pxChild;

			++g_iGraphCallDepth;
			const GraphNodeStatus eStatus = m_pxChild->RunGraphCall(xChildContext);
			--g_iGraphCallDepth;
			return eStatus;
		}

		void OnAbort(Zenith_GraphContext& xContext) override
		{
			if (m_pxChild)
			{
				Zenith_GraphContext xChildContext = xContext;
				xChildContext.m_pxGraph = m_pxChild;
				m_pxChild->AbortAllChains(xChildContext);
			}
		}

		const char* GetTypeName() const override { return "CallGraph"; }

	private:
		bool EnsureChildResolved(Zenith_GraphContext& xContext)
		{
#ifdef ZENITH_TOOLS
			// A drained hot-reload batch may have replaced the asset definition
			// - re-instantiate the child (blackboard is shared, so variables
			// survive; child per-instance node state intentionally drops).
			if (m_pxChild && m_uReloadStamp != Zenith_GraphReload::GetReloadCount())
			{
				delete m_pxChild;
				m_pxChild = nullptr;
				m_bResolveAttempted = false;
			}
#endif
			if (m_bResolveAttempted)
			{
				return m_pxChild != nullptr;
			}
			m_bResolveAttempted = true;
#ifdef ZENITH_TOOLS
			m_uReloadStamp = Zenith_GraphReload::GetReloadCount();
#endif
			if (m_strGraphAssetPath.empty())
			{
				return false;
			}
			Zenith_BehaviourGraphAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(m_strGraphAssetPath);
			if (!pxAsset || !pxAsset->LoadedOk())
			{
				if (!m_bWarned)
				{
					m_bWarned = true;
					Zenith_Error(LOG_CATEGORY_CORE, "CallGraph: asset '%s' missing or invalid; call fails", m_strGraphAssetPath.c_str());
				}
				return false;
			}
			m_pxChild = new Zenith_BehaviourGraph();
			m_pxChild->InitialiseFromDefinition(pxAsset->GetDefinition());

			// The child's declared variables are its parameter list: seed their
			// defaults into the SHARED (caller) blackboard where absent, so the
			// child's tunables exist without stomping caller-provided values.
			const Zenith_GraphDefinition& xDefinition = pxAsset->GetDefinition();
			for (u_int u = 0; u < xDefinition.GetVariableCount(); ++u)
			{
				const Zenith_GraphVariableDecl& xDecl = xDefinition.GetVariableAt(u);
				if (!xContext.m_pxBlackboard->HasValue(xDecl.m_strName))
				{
					xContext.m_pxBlackboard->SetValue(xDecl.m_strName, xDecl.m_xDefault);
				}
			}
			return true;
		}

		Zenith_BehaviourGraph* m_pxChild = nullptr;
		bool m_bResolveAttempted = false;
		bool m_bWarned = false;
#ifdef ZENITH_TOOLS
		u_int m_uReloadStamp = 0;
#endif
	};

	//==========================================================================
	// WaitForCondition - RUNNING until a blackboard bool turns true (async
	// sequencing: animation-complete flags, load-complete flags, task flags).
	//==========================================================================
	class Zenith_GraphNode_WaitForCondition : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_WaitForCondition)
	public:
		ZENITH_PROPERTY(std::string, m_strConditionVar, "ready")
		ZENITH_PROPERTY(bool, m_bResetOnPass, false)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_pxBlackboard->GetBool(m_strConditionVar, false))
			{
				return GRAPH_NODE_STATUS_RUNNING;
			}
			if (m_bResetOnPass)
			{
				Zenith_PropertyValue xFalse;
				xFalse.SetBool(false);
				xContext.m_pxBlackboard->SetValue(m_strConditionVar, xFalse);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "WaitForCondition"; }
	};
}

void Zenith_RegisterEngineGraphNodes_Flow()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	// Static pin counts are the defaults for freshly-placed nodes; configured
	// instances report their real counts via GetDynamicExecOutputCount.
	xRegistry.RegisterNodeType<Zenith_GraphNode_SwitchOnInt>("SwitchOnInt", GRAPH_EVENT_NONE, 5, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SwitchOnString>("SwitchOnString", GRAPH_EVENT_NONE, 1, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_StateMachine>("StateMachine", GRAPH_EVENT_NONE, 4, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Selector>("Selector", GRAPH_EVENT_NONE, 2, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Repeat>("Repeat", GRAPH_EVENT_NONE, 2, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ForEach>("ForEach", GRAPH_EVENT_NONE, 2, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CallGraph>("CallGraph", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Cooldown>("Cooldown", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_WaitForCondition>("WaitForCondition", GRAPH_EVENT_NONE, 1, false, "Flow");
}
