#pragma once

//------------------------------------------------------------------------------
// Shared harness for the routable-FAILURE ("On Failure" exec pin) tests of the
// engine node TUs. Included by each
// Zenith_GraphNode_Registration_<TU>.Tests.inl; carries NO node classes and
// makes NO registry registrations, so including it from five TUs is safe - the
// hazard that keeps test NODE types per-TU is duplicate name-keyed
// registration, which a harness has none of.
//
// Everything here is `inline` at namespace scope and NOT in an anonymous
// namespace, deliberately: an anonymous namespace in a header included by five
// translation units is five copies again, which is the duplication this file
// exists to remove (and `static` free functions in a header are the C4505
// build break the moment one TU uses a subset).
//
// WHAT THE HARNESS PROVES, per row:
//   (i)   the registered type still carries the flag, at the expected pin
//         index (m_uExecOutputCount == 1, so the failure pin is index 1);
//   (ii)  WIRED - a real graph walk routes the node's FAILURE down the failure
//         edge and does NOT take pin 0;
//   (iii) UNWIRED - the same graph aborts, running neither successor.
//
// (ii) is also the POSITIVE CONTROL. RUNNING and never-ran both leave pin 0
// unset exactly like a routed FAILURE does, so without the fail-probe the
// unwired leg would pass on a dead node. Every assertion message names the
// node type, because a failure reported from this shared file points at the
// harness line, not at the caller's row.
//
// Anchor and probes are REGISTERED ENGINE nodes addressed by NAME
// (OnCustomEvent / SetBlackboardBool): the scratch node types in
// Zenith_Scripting.Tests.inl are unreachable from these TUs, and a same-named
// second registration is refused as a duplicate.
//
// The runtime SEMANTICS of the pin (same chain key, suspending handlers,
// aborts, the cycle cap, refusal on flow / dynamic-pin / source types) are the
// FailurePin_* tests in Zenith/Scripting/Zenith_Scripting.Tests.inl and are
// not re-proved here.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"

// Drives the subject node into the FAILURE branch its row comment names.
// Captureless lambdas convert to this, so a row stays a one-expression entry.
typedef void (*Zenith_FailurePinConfigureFn)(Zenith_GraphBuilder&, u_int);

struct Zenith_FailurePinCase
{
	const char* m_szTypeName;
	Zenith_FailurePinConfigureFn m_pfnConfigure;	// null = the node fails on its defaults
};

// OnCustomEvent("Fail") -> <subject>, a SetBlackboardBool probe on the
// subject's pin 0, and (when bWireFail) a second probe on its failure pin.
// Probe variables are named per type so a failed assert names the node.
inline bool Zenith_BuildFailurePinGraph(Zenith_GraphDefinition& xDefinition, const Zenith_FailurePinCase& xCase, bool bWireFail)
{
	const std::string strPin0Var = std::string(xCase.m_szTypeName) + "_pin0";
	const std::string strFailVar = std::string(xCase.m_szTypeName) + "_fail";

	Zenith_GraphBuilder xBuilder(xDefinition);
	const u_int uSource = xBuilder.Node("OnCustomEvent");
	xBuilder.ParamString(uSource, "m_strEventName", "Fail");
	const u_int uSubject = xBuilder.Node(xCase.m_szTypeName);
	xBuilder.Chain(uSource, uSubject);
	if (xCase.m_pfnConfigure != nullptr)
	{
		xCase.m_pfnConfigure(xBuilder, uSubject);
	}

	const u_int uPin0Probe = xBuilder.Node("SetBlackboardBool");
	xBuilder.ParamString(uPin0Probe, "m_strVariable", strPin0Var.c_str());
	xBuilder.Edge(uSubject, 0, uPin0Probe);

	if (bWireFail)
	{
		// FailPin resolves through the builder's pending state, so it must be
		// asked BEFORE Build() - and it latches the error state on an unflagged
		// type, which is what makes a lost flag fail the build rather than wire
		// a second edge onto pin 0.
		const u_int uFailProbe = xBuilder.Node("SetBlackboardBool");
		xBuilder.ParamString(uFailProbe, "m_strVariable", strFailVar.c_str());
		xBuilder.Edge(uSubject, xBuilder.FailPin(uSubject), uFailProbe);
	}
	return xBuilder.Build();
}

// Instantiates the definition, fires "Fail" once, and reports both probes.
inline void Zenith_RunFailurePinGraph(const Zenith_GraphDefinition& xDefinition, const Zenith_Entity& xSelf,
	const char* szTypeName, bool& bPin0Ran, bool& bFailRan)
{
	Zenith_BehaviourGraph xGraph;
	xGraph.InitialiseFromDefinition(xDefinition);
	Zenith_GraphContext xContext;
	xContext.m_xSelf = xSelf;
	xContext.m_pxGraph = &xGraph;
	xContext.m_pxBlackboard = &xGraph.GetBlackboard();
	xGraph.FireCustomEvent("Fail", xContext);
	bPin0Ran = xGraph.GetBlackboard().GetBool(std::string(szTypeName) + "_pin0", false);
	bFailRan = xGraph.GetBlackboard().GetBool(std::string(szTypeName) + "_fail", false);
}

// The whole (i)/(ii)/(iii) check for one table, against one self entity
// (default-constructed for the TUs whose nodes need no scene).
inline void Zenith_CheckFailurePinTable(const Zenith_FailurePinCase* paxCases, u_int uCaseCount, const Zenith_Entity& xSelf)
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();

	for (u_int uCase = 0; uCase < uCaseCount; ++uCase)
	{
		const Zenith_FailurePinCase& xCase = paxCases[uCase];

		// (i) the registered shape: flagged, and the failure pin sits at index 1.
		const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(xCase.m_szTypeName);
		ZENITH_ASSERT_NOT_NULL(pxInfo, "%s is not registered", xCase.m_szTypeName);
		if (pxInfo == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(pxInfo->m_bHasFailurePin, "%s lost its On Failure pin", xCase.m_szTypeName);
		ZENITH_ASSERT_EQ(pxInfo->m_uExecOutputCount, 1u, "%s is no longer a single-pin node", xCase.m_szTypeName);

		// (ii) WIRED: the failure edge is taken, pin 0 is not.
		bool bPin0 = false;
		bool bFail = false;
		{
			Zenith_GraphDefinition xDefinition;
			ZENITH_ASSERT_TRUE(Zenith_BuildFailurePinGraph(xDefinition, xCase, true),
				"%s wired graph did not build", xCase.m_szTypeName);
			Zenith_RunFailurePinGraph(xDefinition, xSelf, xCase.m_szTypeName, bPin0, bFail);
		}
		// POSITIVE CONTROL: without this the unwired leg below would pass on a
		// node that never ran (or returned RUNNING) just as happily.
		ZENITH_ASSERT_TRUE(bFail, "%s did not route its FAILURE down the wired On Failure pin", xCase.m_szTypeName);
		ZENITH_ASSERT_FALSE(bPin0, "%s took its SUCCESS pin", xCase.m_szTypeName);

		// (iii) UNWIRED: today's abort - neither successor runs.
		{
			Zenith_GraphDefinition xDefinition;
			ZENITH_ASSERT_TRUE(Zenith_BuildFailurePinGraph(xDefinition, xCase, false),
				"%s unwired graph did not build", xCase.m_szTypeName);
			Zenith_RunFailurePinGraph(xDefinition, xSelf, xCase.m_szTypeName, bPin0, bFail);
		}
		ZENITH_ASSERT_FALSE(bPin0, "%s unwired: the SUCCESS successor ran", xCase.m_szTypeName);
		ZENITH_ASSERT_FALSE(bFail, "%s unwired: a failure successor ran with no edge", xCase.m_szTypeName);
	}
}

// The other half of every TU's test: a node of the SAME TU that was NOT opted
// in must not have grown a pin.
inline void Zenith_CheckNodeIsNotOptedIn(const char* szTypeName)
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(szTypeName);
	ZENITH_ASSERT_NOT_NULL(pxInfo, "%s is not registered", szTypeName);
	if (pxInfo != nullptr)
	{
		ZENITH_ASSERT_FALSE(pxInfo->m_bHasFailurePin, "%s was opted in by accident", szTypeName);
	}
}

#endif // ZENITH_TESTING
