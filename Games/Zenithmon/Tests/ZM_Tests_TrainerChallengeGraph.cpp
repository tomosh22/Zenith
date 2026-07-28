#include "Zenith.h"

// ============================================================================
// ZM_Tests_TrainerChallengeGraph -- S7 item 3 SC7 CONTRACT units for Zenithmon's
// FIRST .bgraph: the trainer challenge bark
// (OnCustomEvent("ZM_TrainerSpotted","zmTrainerId") -> ZMPushTrainerChallenge).
//
// EVERY UNIT BUILDS THE PRODUCTION DEFINITION IN-PROCESS, FROM
// BuildGraph_ZM_TrainerChallenge, AND NEVER TOUCHES DISK. That is the RenderTest
// Test_TennisBrainContract.cpp:195-204 doctrine and it buys two things at once: a
// stale or absent .bgraph can neither mask a regression nor fail a unit for the
// wrong reason, and there is nothing to RequestSkip on -- so this feature can never
// be silenced by the skip-counts-as-a-pass hole. `.bgraph` files are gitignored and
// are authored only by a TOOLS boot, so a disk-reading unit would skip on a fresh
// CI checkout, which is exactly the failure mode these units exist to rule out.
//
// Legal at boot: Project_RegisterGameComponents (Zenith_Engine.cpp:775) runs BEFORE
// RunAllTests (:799), so ZM_RegisterGraphNodes has already registered
// ZMPushTrainerChallenge by the time these run.
//
// Category ZM_Interaction -- the SAME category as the SC1/SC2/SC3/SC6 interaction
// units, because this is the same feature area: the graph owns one BEAT of the
// trainer-sight flow and nothing else.
//
// NODE TYPE NAMES, EVENT NAMES AND BLACKBOARD VARIABLE NAMES HAVE NO COMPILE-TIME
// CHECKING ANYWHERE IN THE ENGINE. In THIS rig the clauses that catch a typo'd
// node-type name are Build() == true and HasErrors() == false -- NOT the unresolved
// count. Zenith_GraphBuilder::Node (Zenith/Scripting/Zenith_GraphBuilder.cpp:27-37)
// looks the name up in the registry and, on a miss, logs "unknown node type '%s'",
// latches m_bErrors and returns node id 0 WITHOUT ever adding the node to the
// definition. A definition built IN-PROCESS -- which is all any unit in this file
// ever does -- therefore cannot instantiate with a non-zero GetUnresolvedCount(): the
// builder refused the type long before Zenith_BehaviourGraph::InitialiseFromDefinition
// could fail to resolve it. GetUnresolvedCount() == 0u is asserted below anyway,
// because it costs one comparison and it is the clause that WOULD have teeth the day a
// unit here instantiates from a LOADED .bgraph (unknown types survive a load verbatim
// as unresolved nodes -- the preservation contract in Zenith/Scripting/CLAUDE.md), but
// it is not what makes a mis-spelled type red today, and no future edit should shed
// Build()/HasErrors() believing that it is.
//
// s_uChallengePushAttempts is the only thing that catches a transposed chain or a
// mis-named payload variable. Do not weaken it.
//
// There is no ZM_MenuRoot singleton at boot, so TryPushDialogue always refuses and
// s_uChallengePushSucceeded stays 0u. Asserting that EXPLICITLY is what keeps these
// units honest about how far they reach.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"                      // the totality proof
#include "Core/Zenith_PropertySystem.h"                          // Zenith_PropertyValue (the event payload)
#include "Scripting/Zenith_BehaviourGraph.h"                     // definition + live instance
#include "Scripting/Zenith_GraphBuilder.h"                       // the authoring surface the builder writes through
#include "Scripting/Zenith_GraphNode.h"                          // Zenith_GraphContext
#include "Zenithmon/Components/ZM_GraphNodes.h"                  // ZM_GraphNodeTestCounters -- the anti-vacuity handle
#include "Zenithmon/Source/Data/ZM_TrainerData.h"                // ZM_TRAINER_RIVAL_VESPER / ZM_TRAINER_NONE
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"            // the shared name constants + the builder

namespace
{
	// An event name the graph must NOT answer to. Spelled here rather than derived
	// from szZM_GRAPH_EVENT_TRAINER_SPOTTED so a rename of the real event cannot
	// silently turn the negative half of the anti-vacuity unit into a tautology.
	constexpr const char* szNOT_THE_TRAINER_EVENT = "ZM_NotTheTrainerEvent";

	// The PRODUCTION graph, built in-process and instantiated. No scene, no entity,
	// no ZM_MenuRoot, no disk.
	struct TrainerChallengeGraphRig
	{
		Zenith_GraphDefinition m_xDefinition;
		Zenith_BehaviourGraph m_xGraph;
		bool m_bBuilt = false;
		bool m_bBuilderClean = false;
		bool m_bInstanced = false;

		TrainerChallengeGraphRig()
		{
			Zenith_GraphBuilder xBuilder(m_xDefinition);
			BuildGraph_ZM_TrainerChallenge(xBuilder);
			m_bBuilt = xBuilder.Build();
			m_bBuilderClean = !xBuilder.HasErrors();
			m_bInstanced = m_xGraph.InitialiseFromDefinition(m_xDefinition);
		}

		~TrainerChallengeGraphRig() { m_xGraph.Shutdown(); }

		TrainerChallengeGraphRig(const TrainerChallengeGraphRig&) = delete;
		TrainerChallengeGraphRig& operator=(const TrainerChallengeGraphRig&) = delete;

		// Everything the shape units assert, in one predicate, so the behavioural
		// units can refuse to run against a rig that never built.
		bool IsUsable() const
		{
			return m_bBuilt && m_bBuilderClean && m_bInstanced
				&& m_xGraph.GetUnresolvedCount() == 0u;
		}

		// The context ZM_Interactable's fire site hands the graph: self is deliberately
		// INVALID here (the push node reaches no component), the blackboard is the
		// instance's own, and the payload is the trainer id -- or null, for the
		// no-payload arm of the totality unit.
		Zenith_GraphContext MakeContext(const Zenith_PropertyValue* pxPayload)
		{
			Zenith_GraphContext xContext;
			xContext.m_pxGraph = &m_xGraph;
			xContext.m_pxBlackboard = &m_xGraph.GetBlackboard();
			xContext.m_pxEventPayload = pxPayload;
			return xContext;
		}

		// TARGETED, never BroadcastCustomEvent: a broadcast would reach every
		// Zenith_GraphComponent in every loaded scene, including the 31 engine
		// UnitTest_*.bgraph components.
		void Fire(const char* szEventName, const Zenith_PropertyValue* pxPayload)
		{
			Zenith_GraphContext xContext = MakeContext(pxPayload);
			m_xGraph.FireCustomEvent(szEventName, xContext);
		}

		void FireTrainerSpotted(const Zenith_PropertyValue* pxPayload)
		{
			Fire(szZM_GRAPH_EVENT_TRAINER_SPOTTED, pxPayload);
		}
	};

	Zenith_PropertyValue MakeInt32Payload(int32_t iValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(iValue);
		return xValue;
	}

	Zenith_PropertyValue MakeTrainerPayload(ZM_TRAINER_ID eId)
	{
		return MakeInt32Payload((int32_t)(u_int)eId);
	}
}

// ---- The shape: it builds, it instantiates, and EVERY node type resolves --------

ZENITH_TEST(ZM_Interaction, TrainerChallengeGraph_BuildsAndResolvesEveryNodeType)
{
	TrainerChallengeGraphRig xRig;

	// ★ THESE TWO ARE THE TYPO CATCHERS, and they are the reason this unit exists.
	// Node type names are name strings with no compile-time checking anywhere, but the
	// BUILDER is name-keyed and strict: Zenith_GraphBuilder::Node looks the type up in
	// Zenith_GraphNodeRegistry, and a miss logs "unknown node type '%s'", sets
	// m_bErrors and returns id 0 -- the definition never receives the node, the
	// subsequent Chain() on a 0 id latches too, and Build() returns false. Mis-spell
	// szZM_GRAPH_NODE_PUSH_TRAINER_CHALLENGE (or the OnCustomEvent type) and BOTH of
	// these red. DO NOT DROP EITHER.
	ZENITH_ASSERT_TRUE(xRig.m_bBuilt,
		"BuildGraph_ZM_TrainerChallenge's Build() failed -- the authored chain was "
		"rejected, so the tools boot would write a broken .bgraph");
	ZENITH_ASSERT_TRUE(xRig.m_bBuilderClean,
		"the builder latched an error while authoring the trainer challenge graph "
		"(an unknown node type, an unknown property, or a rejected edge)");
	ZENITH_ASSERT_TRUE(xRig.m_bInstanced,
		"InitialiseFromDefinition failed on the production trainer challenge "
		"definition");

	// NOT a typo catcher -- the two clauses above are. For an IN-PROCESS build this
	// count is structurally 0: the builder rejected an unregistered type up front, so
	// nothing unresolvable ever reached the definition for InitialiseFromDefinition to
	// stub out. It is kept because it is one comparison and it is the ONLY clause here
	// that would catch a STALE OR HAND-EDITED LOADED definition -- a .bgraph naming a
	// type this build no longer registers loads, preserves the node verbatim, warns
	// once and silently fails its chain. The day a unit in this file instantiates from
	// szZM_GRAPH_TRAINER_CHALLENGE_ASSET rather than from BuildGraph_ZM_TrainerChallenge,
	// this is the clause that reds it; until then it is cheap insurance, not the guard.
	ZENITH_ASSERT_EQ(xRig.m_xGraph.GetUnresolvedCount(), 0u,
		"the trainer challenge graph instantiated with unresolved nodes -- a node "
		"type name string does not match anything ZM_RegisterGraphNodes (or the "
		"engine registry) registered");

	// TWO nodes, deliberately: a three-node Query -> Branch -> Bark shape was
	// REJECTED because the only branchable condition ("does this trainer have
	// lines?") is already decided in C++, where the FSM needs the same answer to
	// skip its confirm window. This pins the count so a third node cannot drift in
	// without the ruling being revisited.
	ZENITH_ASSERT_EQ(xRig.m_xDefinition.GetNodeCount(), 2u,
		"the trainer challenge graph is a TWO-node chain "
		"(OnCustomEvent -> ZMPushTrainerChallenge)");
	ZENITH_ASSERT_EQ(xRig.m_xDefinition.GetEdgeCount(), 1u,
		"the trainer challenge graph has exactly ONE exec edge");
}

// ---- The beat: firing the real event reaches the push node with the right id ----

ZENITH_TEST(ZM_Interaction, TrainerChallengeGraph_FiringTheEventReachesThePushNode)
{
	TrainerChallengeGraphRig xRig;
	ZENITH_ASSERT_TRUE(xRig.IsUsable(),
		"the rig failed to build the production graph -- "
		"TrainerChallengeGraph_BuildsAndResolvesEveryNodeType owns that failure");
	if (!xRig.IsUsable())
	{
		return;
	}

	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 0u,
		"the counters did not start cleared -- every clause below is a DELTA from "
		"zero");

	const Zenith_PropertyValue xPayload = MakeTrainerPayload(ZM_TRAINER_RIVAL_VESPER);
	xRig.FireTrainerSpotted(&xPayload);

	// The chain walked: the OnCustomEvent source matched the name, stashed the
	// payload under szZM_GRAPH_VAR_TRAINER_ID, and pin 0 carried into the push node.
	// A transposed chain (Chain(uPush, uSource)) leaves this at 0.
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 1u,
		"firing \"%s\" did not reach ZMPushTrainerChallenge -- the .bgraph is "
		"decoration rather than the owner of the beat",
		szZM_GRAPH_EVENT_TRAINER_SPOTTED);
	// ...and it arrived carrying the RIGHT trainer. A mis-named store-payload var
	// leaves this at ZM_TRAINER_NONE while the attempt count still moves.
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer,
		ZM_TRAINER_RIVAL_VESPER,
		"the push node did not resolve the fired trainer id -- the source's "
		"store-payload variable and the node's m_strTrainerIdVar must both be \"%s\"",
		szZM_GRAPH_VAR_TRAINER_ID);

	// How far this unit reaches, stated explicitly: there is no ZM_MenuRoot
	// singleton at boot, so TryPushDialogue refuses and no bark is ever shown.
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushSucceeded, 0u,
		"a push SUCCEEDED with no ZM_MenuRoot singleton in existence -- either a "
		"scene leaked into the boot units or TryPushDialogue stopped resolving the "
		"singleton");

	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
}

// ---- The anti-vacuity twin: a WRONG event name reaches nothing ------------------

ZENITH_TEST(ZM_Interaction, TrainerChallengeGraph_WrongEventNameReachesNothing)
{
	TrainerChallengeGraphRig xRig;
	ZENITH_ASSERT_TRUE(xRig.IsUsable(),
		"the rig failed to build the production graph -- "
		"TrainerChallengeGraph_BuildsAndResolvesEveryNodeType owns that failure");
	if (!xRig.IsUsable())
	{
		return;
	}

	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();

	// HALF ONE: the negative. A custom event this graph does not answer to must not
	// walk the chain, however well-formed its payload is.
	const Zenith_PropertyValue xPayload = MakeTrainerPayload(ZM_TRAINER_RIVAL_VESPER);
	xRig.Fire(szNOT_THE_TRAINER_EVENT, &xPayload);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 0u,
		"firing \"%s\" reached ZMPushTrainerChallenge -- the OnCustomEvent source is "
		"answering to a name it must not",
		szNOT_THE_TRAINER_EVENT);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer, ZM_TRAINER_NONE,
		"a wrong-named event moved the last-trainer observation");

	// HALF TWO: the positive, IN THE SAME UNIT, so "nothing happened" above can never
	// be mistaken for "the graph is inert". Both halves live together deliberately.
	xRig.FireTrainerSpotted(&xPayload);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 1u,
		"the REAL event \"%s\" reached nothing either -- this graph is inert, and "
		"the negative half above proves nothing",
		szZM_GRAPH_EVENT_TRAINER_SPOTTED);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer,
		ZM_TRAINER_RIVAL_VESPER,
		"the real event reached the push node without the fired trainer id");

	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
}

// ---- Totality: no MenuRoot, garbage payloads, and NEVER an assert ---------------
//
// In the ZM_Tests_TrainerData local-hit-count form, for the two mandatory reasons:
// (1) NO ZENITH_ASSERT_* may appear INSIDE the scope -- while a capture scope is
// active Zenith_TestRunner::HandleFailure swallows framework failures and merely
// bumps the hit count, so an in-scope assertion could never red this unit; (2) the
// count MUST be copied to a local before the closing brace, because
// ~Zenith_AssertCaptureScope restores the previous hit count. Scopes do not nest.
//
// This unit exists because Zenith_PropertyValue::GetInt32 ASSERTS on a type
// mismatch (Zenith_PropertySystem.h:85) and Zenith_Assert breaks the process in
// EVERY configuration: one hit here would not fail one test, it would end the whole
// boot unit run.

ZENITH_TEST(ZM_Interaction, TrainerChallengeGraph_PushIsTotalWithNoMenuRootAndNeverAsserts)
{
	TrainerChallengeGraphRig xRig;
	ZENITH_ASSERT_TRUE(xRig.IsUsable(),
		"the rig failed to build the production graph -- "
		"TrainerChallengeGraph_BuildsAndResolvesEveryNodeType owns that failure");
	if (!xRig.IsUsable())
	{
		return;
	}

	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();

	// The seven payloads, in this order. NO PAYLOAD comes FIRST on purpose: the
	// blackboard starts empty (the graph declares no variables), so that arm proves
	// the MISSING-variable path rather than reading a value a previous fire left.
	Zenith_PropertyValue xFloat;   xFloat.SetFloat(1.0f);
	Zenith_PropertyValue xString;  xString.SetString("ZM_TRAINER_RIVAL_VESPER");
	Zenith_PropertyValue xBool;    xBool.SetBool(true);
	const Zenith_PropertyValue xNegative   = MakeInt32Payload(-1);
	const Zenith_PropertyValue xSentinel   = MakeTrainerPayload(ZM_TRAINER_NONE);
	const Zenith_PropertyValue xOutOfRange = MakeInt32Payload(99999);

	constexpr u_int uEXPECTED_FIRES = 7u;

	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;

		xRig.FireTrainerSpotted(nullptr);        // no payload at all
		xRig.FireTrainerSpotted(&xFloat);        // wrong type: FLOAT
		xRig.FireTrainerSpotted(&xString);       // wrong type: STRING
		xRig.FireTrainerSpotted(&xBool);         // wrong type: BOOL
		xRig.FireTrainerSpotted(&xNegative);     // INT32, negative
		xRig.FireTrainerSpotted(&xSentinel);     // INT32, the ZM_TRAINER_NONE sentinel
		xRig.FireTrainerSpotted(&xOutOfRange);   // INT32, outright garbage

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the challenge push node asserted on a payload -- Zenith_Assert breaks in "
		"EVERY config and the whole unit suite runs at boot, so this would kill the "
		"whole boot unit run rather than fail one test");

	// THE ANTI-VACUITY HANDLE: every one of the seven fires REACHED the node. Without
	// this clause a chain that stopped reaching the node entirely would pass the
	// never-asserts assertion trivially.
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, uEXPECTED_FIRES,
		"the push node was not reached by every degenerate fire -- the "
		"never-asserts clause above would then be vacuous");
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushSucceeded, 0u,
		"a garbage payload produced a bark");
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer, ZM_TRAINER_NONE,
		"a garbage payload resolved to a registered trainer");

	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
}
