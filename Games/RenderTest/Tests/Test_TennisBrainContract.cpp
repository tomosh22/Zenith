#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_TennisBrainContract - the HERMETIC gate on the TennisBrain behaviour
 * graph's RNG-determinism contract (risk R2). Replaces the retired
 * RT_TennisDeterminismDigest.
 *
 * WHY THIS EXISTS
 *
 * RT_TennisDeterminismDigest folded an FNV-1a digest over 2400 fixed-dt frames
 * of the live tennis match and compared it to a hard-pinned constant. It cost
 * ~52 s per run and pinned an ENTIRE physics simulation, so any legitimate
 * gameplay/physics tweak broke it with no signal about which clause of the
 * contract had actually moved - which trained people to re-pin rather than
 * investigate (Q-2026-07-21-002).
 *
 * The contract it CLAIMED to protect is narrow and needs none of that:
 *
 *   1. TICK CADENCE   - the authored accumulator reproduces the retired
 *                       AIAgent 0.08 s interval exactly, resets to zero (not a
 *                       subtractive carry), and FREEZES rather than resets
 *                       while the referee has the agent parked.
 *   2. GATE ORDER     - the tick spine's node order, and the 3-pin Selector's
 *                       serve > rally > recover priority, evaluated in pin
 *                       order with first-success-wins.
 *   3. RNG DRAW COUNTS- the decide nodes draw from the brain's TennisRng only
 *                       on FIRING ticks and only while UN-ARMED, and each
 *                       decision consumes an exact number of draws.
 *
 * None of that needs a match, terrain, a navmesh, physics or a ball, so these
 * tests use none. Each builds a one-entity scene (no physics, no terrain),
 * instantiates the PRODUCTION graph definition straight from
 * BuildGraph_RenderTestTennisBrain (the same function the tools boot writes the
 * .bgraph from - so there is no disk asset to go stale and no dependency on a
 * prior tools boot), and drives it with scripted dt + a scripted blackboard,
 * observing the tick chain through Zenith_BehaviourGraph::GetRecentlyExecuted.
 *
 * Every assertion is a NAMED property with an expected value, so a failure says
 * which clause moved and by how much instead of "digest mismatch".
 *
 * WHAT IS DELIBERATELY NOT COVERED HERE
 *
 * That the match as a whole plays correctly - phases, serves, the receiver
 * standing in, points resolving, the ball epoch advancing - is RT_TennisMatchFlow's
 * job, and it remains a live-simulation test. These tests gate the brain's
 * decision cadence, not the simulation's trajectory. The rally branch's
 * RTTennisBallReachable gate needs live perception, which is not reachable
 * hermetically, so the rally decide node's draw count is pinned by executing the
 * node directly (the sibling unit-test pattern in RenderTest_Tennis.Tests.inl)
 * while the SERVE branch is driven end-to-end through the graph.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_GraphNodes.h"
#include "RenderTest/RenderTest_Tennis.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
	// ------------------------------------------------------------------------
	// Failure accumulator. Every check runs and reports, so ONE run names EVERY
	// clause that moved (the digest could only ever say "mismatch").
	// ------------------------------------------------------------------------
	int g_iContractChecks = 0;
	int g_iContractFailures = 0;

	void ResetContractResults()
	{
		g_iContractChecks = 0;
		g_iContractFailures = 0;
	}

	void CheckTrue(bool bCondition, const char* szWhat)
	{
		++g_iContractChecks;
		if (!bCondition)
		{
			++g_iContractFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisContract] FAILED: %s", szWhat);
		}
	}

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iContractChecks;
		if (iActual != iExpected)
		{
			++g_iContractFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisContract] FAILED: %s (expected %d, got %d)",
				szWhat, iExpected, iActual);
		}
	}

	void CheckEqStr(const char* szActual, const char* szExpected, const char* szWhat)
	{
		++g_iContractChecks;
		if (std::strcmp(szActual, szExpected) != 0)
		{
			++g_iContractFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisContract] FAILED: %s\n  expected: %s\n  actual:   %s",
				szWhat, szExpected, szActual);
		}
	}

	bool ReportContract(const char* szTest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TennisContract] %s: %d checks, %d failed",
			szTest, g_iContractChecks, g_iContractFailures);
		// A test that asserted nothing is a test that cannot fail - treat an empty
		// run as a failure so a broken rig can never masquerade as green.
		return g_iContractFailures == 0 && g_iContractChecks > 0;
	}

	// ------------------------------------------------------------------------
	// Exact TennisRng draw count between two observed states. The LCG is
	// injective, so stepping forward from the earlier state reaches the later one
	// at exactly one offset. Returns -1 when it does not within the bound - a
	// reportable failure, never a silent zero.
	// ------------------------------------------------------------------------
	constexpr int k_iMaxCountedDraws = 256;

	int RngDrawsBetween(uint32_t uBefore, uint32_t uAfter)
	{
		RenderTest_Tennis::TennisRng xProbe(uBefore);
		for (int i = 0; i <= k_iMaxCountedDraws; ++i)
		{
			if (xProbe.m_uState == uAfter)
				return i;
			xProbe.Next();
		}
		return -1;
	}

	// ------------------------------------------------------------------------
	// The rig: one NPC entity in an empty additive scene (no physics, no terrain,
	// no navmesh, no ball, no referee) plus a live instance of the PRODUCTION
	// brain-graph definition, driven by hand.
	// ------------------------------------------------------------------------
	struct TennisBrainRig
	{
		Zenith_Scene xPrevActiveScene;
		Zenith_Scene xScene;
		Zenith_SceneData* pxSceneData = nullptr;
		Zenith_EntityID xNpcID = INVALID_ENTITY_ID;
		Zenith_GraphDefinition xDefinition;
		Zenith_BehaviourGraph xGraph;
		bool bRigValid = false;

		TennisBrainRig()
		{
			// The harness reloads the boot scene between tests; restore whatever was
			// active so this rig leaves no trace beyond its own scope.
			xPrevActiveScene = g_xEngine.Scenes().GetActiveScene();
			xScene = g_xEngine.Scenes().LoadScene("TennisBrainContractScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			g_xEngine.Scenes().SetActiveScene(xScene);
			pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
			if (pxSceneData == nullptr)
				return;

			// The near-side NPC: AIAgent (the tick gate reads its enable flag),
			// the body (the position/arm nodes need it) and the brain (owns the
			// RNG stream + the decided-shot arm guard). No animator, so
			// RequestServe/RequestSwing return false and the brain stays UNARMED
			// unless a test arms it deliberately - exactly the state in which the
			// decide nodes are supposed to draw.
			Zenith_Entity xNpc = g_xEngine.Scenes().CreateEntity(pxSceneData, "Tennis_NPC_Near");
			xNpc.AddComponent<Zenith_AIAgentComponent>();
			xNpc.AddComponent<RenderTest_TennisPlayerComponent>().Init(/*near*/true);
			xNpc.AddComponent<RenderTest_TennisAgentComponent>();
			xNpcID = xNpc.GetEntityID();

			// Pin the body somewhere fixed so the decision cores see a stable
			// TennisPlayerState (branch selection inside SelectServe/SelectShot is
			// state-driven, and the draw counts follow the branch).
			const RenderTest_Tennis::TennisCourt xCourt = RenderTest_Tennis::DefaultCourt();
			xNpc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(
				xCourt.m_fCenterX, xCourt.m_fSurfaceY, xCourt.BaselineZ(RenderTest_Tennis::TENNIS_SIDE_NEAR)));

			// THE graph under test, built from the production authoring function -
			// not loaded from a .bgraph, so a stale/absent asset can neither mask a
			// regression nor fail the test for the wrong reason.
			Zenith_GraphBuilder xBuilder(xDefinition);
			BuildGraph_RenderTestTennisBrain(xBuilder);
			const bool bBuilt = xBuilder.Build();
			const bool bInstanced = xGraph.InitialiseFromDefinition(xDefinition);

			Brain().OnStart();
			bRigValid = bBuilt && bInstanced && xGraph.GetUnresolvedCount() == 0;
		}

		~TennisBrainRig()
		{
			xGraph.Shutdown();
			g_xEngine.Scenes().UnloadSceneForced(xScene);
			if (xPrevActiveScene.IsValid())
				g_xEngine.Scenes().SetActiveScene(xPrevActiveScene);
		}

		TennisBrainRig(const TennisBrainRig&) = delete;
		TennisBrainRig& operator=(const TennisBrainRig&) = delete;

		Zenith_Entity Npc() { return pxSceneData->GetEntity(xNpcID); }
		RenderTest_TennisAgentComponent& Brain() { return Npc().GetComponent<RenderTest_TennisAgentComponent>(); }
		Zenith_AIAgentComponent& Agent() { return Npc().GetComponent<Zenith_AIAgentComponent>(); }
		uint32_t RngState() { return Brain().Rng().m_uState; }

		// ---- blackboard scripting (what the referee publishes in production) ----
		void SetBBInt(const char* szKey, int32_t iValue)
		{
			Zenith_PropertyValue xValue;
			xValue.SetInt32(iValue);
			xGraph.GetBlackboard().SetValue(szKey, xValue);
		}
		void SetBBBool(const char* szKey, bool bValue)
		{
			Zenith_PropertyValue xValue;
			xValue.SetBool(bValue);
			xGraph.GetBlackboard().SetValue(szKey, xValue);
		}

		// The referee's SERVING publish: our side is serving and the ball is parked
		// on the toss point, so Selector pin 0 (serve) opens.
		void ScriptServePhase(bool bSecondServe)
		{
			SetBBInt(RenderTest_TennisBB::k_szPhase, static_cast<int32_t>(RenderTest_Tennis::POINT_PHASE_SERVING));
			SetBBInt(RenderTest_TennisBB::k_szBallEpoch, 1);
			SetBBInt(RenderTest_TennisBB::k_szMySide, static_cast<int32_t>(RenderTest_Tennis::TENNIS_SIDE_NEAR));
			SetBBBool(RenderTest_TennisBB::k_szIsServer, true);
			SetBBBool(RenderTest_TennisBB::k_szServeBallParked, true);
			SetBBBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
			SetBBBool(RenderTest_TennisBB::k_szIsSecondServe, bSecondServe);
			SetBBBool(RenderTest_TennisBB::k_szIsMyBall, false);
		}

		// The referee's LIVE publish with the ball on our side: pin 0 must close
		// and pin 1 (rally) must open.
		void ScriptRallyPhase()
		{
			SetBBInt(RenderTest_TennisBB::k_szPhase, static_cast<int32_t>(RenderTest_Tennis::POINT_PHASE_LIVE));
			SetBBInt(RenderTest_TennisBB::k_szBallEpoch, 1);
			SetBBInt(RenderTest_TennisBB::k_szMySide, static_cast<int32_t>(RenderTest_Tennis::TENNIS_SIDE_NEAR));
			SetBBBool(RenderTest_TennisBB::k_szIsServer, false);
			SetBBBool(RenderTest_TennisBB::k_szServeBallParked, false);
			SetBBBool(RenderTest_TennisBB::k_szIsMyBall, true);
		}

		// A non-play phase: both gated pins close and only the recover fallback runs.
		void ScriptPointOverPhase()
		{
			SetBBInt(RenderTest_TennisBB::k_szPhase, static_cast<int32_t>(RenderTest_Tennis::POINT_PHASE_POINT_OVER));
			SetBBBool(RenderTest_TennisBB::k_szIsServer, false);
			SetBBBool(RenderTest_TennisBB::k_szServeBallParked, false);
			SetBBBool(RenderTest_TennisBB::k_szIsMyBall, false);
		}

		Zenith_GraphContext MakeContext(float fDt)
		{
			Zenith_GraphContext xContext;
			xContext.m_xSelf = Npc();
			xContext.m_fDt = fDt;
			xContext.m_pxGraph = &xGraph;
			xContext.m_pxBlackboard = &xGraph.GetBlackboard();
			return xContext;
		}

		// One engine frame's ON_UPDATE dispatch at component order 60.
		void Tick(float fDt)
		{
			Zenith_GraphContext xContext = MakeContext(fDt);
			xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
		}

		// ---- last-tick execution trace ----------------------------------------
		// GetRecentlyExecuted is cleared at the top of every ON_UPDATE dispatch, so
		// after a Tick it holds exactly that tick's chain walk, in execution order.
		const char* TypeOfNode(u_int uNodeID) const
		{
			const Zenith_GraphNodeDef* pxDef = xDefinition.FindNodeDef(uNodeID);
			return pxDef != nullptr ? pxDef->m_strTypeName.c_str() : "<unknown>";
		}
		int TraceIndexOfNode(u_int uNodeID) const
		{
			const Zenith_Vector<u_int>& auTrace = xGraph.GetRecentlyExecuted();
			for (u_int u = 0; u < auTrace.GetSize(); ++u)
			{
				if (auTrace.Get(u) == uNodeID)
					return static_cast<int>(u);
			}
			return -1;
		}
		int TraceIndexOfType(const char* szType) const
		{
			const Zenith_Vector<u_int>& auTrace = xGraph.GetRecentlyExecuted();
			for (u_int u = 0; u < auTrace.GetSize(); ++u)
			{
				if (std::strcmp(TypeOfNode(auTrace.Get(u)), szType) == 0)
					return static_cast<int>(u);
			}
			return -1;
		}
		bool TraceHasType(const char* szType) const { return TraceIndexOfType(szType) >= 0; }

		// True when the tick chain got all the way past the accumulator gate to the
		// Selector - i.e. this frame was a brain TICK, not just an accumulate frame.
		bool SelectorFired() const { return TraceHasType("Selector"); }

		// ---- structural queries over the definition ----------------------------
		u_int FindOnlyNodeOfType(const char* szType) const
		{
			u_int uFound = 0;
			for (u_int u = 0; u < xDefinition.GetNodeCount(); ++u)
			{
				const Zenith_GraphNodeDef& xNode = xDefinition.GetNodeAt(u);
				if (xNode.m_strTypeName == szType)
				{
					if (uFound != 0)
						return 0;   // ambiguous - the caller's assumption is wrong
					uFound = xNode.m_uNodeID;
				}
			}
			return uFound;
		}
		u_int SuccessorOf(u_int uNodeID, u_int uPin) const
		{
			for (u_int u = 0; u < xDefinition.GetEdgeCount(); ++u)
			{
				const Zenith_GraphEdge& xEdge = xDefinition.GetEdgeAt(u);
				if (xEdge.m_uSrcNodeID == uNodeID && xEdge.m_uSrcPin == uPin)
					return xEdge.m_uDstNodeID;
			}
			return 0;
		}

		// Renders the pin-0 exec chain hanging off (uFrom, uPin) as "A>B>C" so a
		// structural break reports the WHOLE authored shape, not just a bool. The
		// walk stops AT a flow node exactly as RunChainFromPin does: a flow node's
		// SUCCESS ends the linear chain, and its sub-chains hang off its own pins
		// (so the Selector terminates the tick spine rather than leaking its serve
		// branch into it).
		void FormatChain(u_int uFrom, u_int uPin, char* pcOut, size_t uCapacity) const
		{
			pcOut[0] = '\0';
			u_int uCurrent = SuccessorOf(uFrom, uPin);
			bool bFirst = true;
			// Bounded by the node count: a malformed definition can never spin here.
			for (u_int uGuard = 0; uCurrent != 0 && uGuard <= xDefinition.GetNodeCount(); ++uGuard)
			{
				const char* szType = TypeOfNode(uCurrent);
				const size_t uLength = std::strlen(pcOut);
				std::snprintf(pcOut + uLength, uCapacity - uLength, "%s%s", bFirst ? "" : ">", szType);
				bFirst = false;

				const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(szType);
				if (pxInfo != nullptr && pxInfo->m_bFlowNode)
					break;
				uCurrent = SuccessorOf(uCurrent, 0);
			}
		}
	};

	// The authored shapes the contract fixes. Written out in full so a diff in the
	// failure log reads as the graph edit that caused it.
	constexpr const char* k_szTickSpineChain =
		"RTTennisTickGate>AddBlackboardFloat>CompareBlackboardFloat>Gate>SetBlackboardFloat>Selector";
	constexpr const char* k_szServePinChain =
		"CompareBlackboardInt>Gate>Gate>Gate>RTTennisDecideServe>RTTennisPositionForServe>RTTennisArmServe";
	constexpr const char* k_szRallyPinChain =
		"CompareBlackboardInt>Gate>Gate>RTTennisBallReachable>RTTennisMoveToIntercept>RTTennisDecideShot>RTTennisArmSwing";
	constexpr const char* k_szRecoverPinChain = "RTTennisRecoverToReady";

	// A dt comfortably past ANY plausible accumulator threshold, so a tick fires
	// on the first dispatch. The gate-order and draw-count clauses are about what
	// a FIRING tick does, not about when one happens - driving them with a dt that
	// straddles the threshold would make a cadence edit redden all three tests and
	// destroy exactly the per-clause attribution this redesign is for. Only
	// RT_TennisBrainTickCadence pins the threshold; the others must survive it
	// moving.
	constexpr float k_fAlwaysFiringDt = 1.0f;
}

// ============================================================================
// RT_TennisBrainTickCadence - contract clause 1.
//
// Every dt below is an exact binary fraction, so the accumulator arithmetic is
// exact and no assertion sits on a float-rounding knife edge.
// ============================================================================

namespace
{
	bool g_bCadenceRan = false;

	// Ticks until the Selector next runs (1 = it ran on the very next tick).
	// Returns -1 if it did not fire within the bound.
	int TicksToNextFire(TennisBrainRig& xRig, float fDt, int iMaxTicks)
	{
		for (int i = 1; i <= iMaxTicks; ++i)
		{
			xRig.Tick(fDt);
			if (xRig.SelectorFired())
				return i;
		}
		return -1;
	}

	void RunTickCadenceChecks()
	{
		// ---- the 0.08 s threshold, to ~0.001 s ------------------------------
		// dt = 1/1024 is exact, and so is every partial sum, so the first tick at
		// or past 0.08 is unambiguously the 82nd (82/1024 = 0.080078125; the 81st
		// is 0.0791015625). That pins the authored threshold to
		// (0.0791015625, 0.080078125] - a mutation to 0.07 or 0.09 moves it.
		{
			TennisBrainRig xRig;
			CheckTrue(xRig.bRigValid, "rig built the production brain graph with no unresolved nodes");
			xRig.ScriptServePhase(/*second*/false);
			CheckEqInt(TicksToNextFire(xRig, 1.0f / 1024.0f, 200), 82,
				"tick fires on the 82nd tick at dt=1/1024 (0.08 s threshold)");
		}

		// ---- the >= boundary, exactly ---------------------------------------
		// 0.04f doubles EXACTLY to the float 0.08f the compare node holds, so a
		// GREATER_EQUAL fires on tick 2 and a GREATER would fire on tick 3.
		{
			TennisBrainRig xRig;
			xRig.ScriptServePhase(/*second*/false);
			CheckEqInt(TicksToNextFire(xRig, 0.04f, 10), 2,
				"tick fires on the 2nd tick at dt=0.04 (the >= 0.08 boundary is inclusive)");
		}

		// ---- reset-to-zero, not a subtractive carry -------------------------
		// At dt = 1/64 the accumulator overshoots to 0.09375 on the firing tick.
		// Resetting to zero keeps the period at a constant 6; carrying the 0.01375
		// remainder forward would shorten every subsequent period to 5.
		{
			TennisBrainRig xRig;
			xRig.ScriptServePhase(/*second*/false);
			CheckEqInt(TicksToNextFire(xRig, 1.0f / 64.0f, 20), 6, "first fire at dt=1/64");
			CheckEqInt(TicksToNextFire(xRig, 1.0f / 64.0f, 20), 6, "second fire at dt=1/64 (reset-to-zero)");
			CheckEqInt(TicksToNextFire(xRig, 1.0f / 64.0f, 20), 6, "third fire at dt=1/64 (reset-to-zero)");
		}

		// ---- freeze (not reset, not keep-accumulating) while parked ----------
		// Accumulate 40/1024, park the agent for 200 ticks, then unpark. The three
		// candidate semantics are distinguishable by the ticks-to-fire that follows:
		//   freeze      -> 82 - 40 = 42   (the contract)
		//   reset       -> 82
		//   accumulate  -> 1              (240/1024 already past the threshold)
		{
			TennisBrainRig xRig;
			xRig.ScriptServePhase(/*second*/false);
			const float fDt = 1.0f / 1024.0f;
			for (int i = 0; i < 40; ++i)
				xRig.Tick(fDt);
			CheckTrue(!xRig.SelectorFired(), "no fire before the accumulator reaches the threshold");

			xRig.Agent().SetEnabled(false);
			for (int i = 0; i < 200; ++i)
				xRig.Tick(fDt);
			CheckTrue(!xRig.SelectorFired(), "a parked agent never reaches the Selector");

			xRig.Agent().SetEnabled(true);
			CheckEqInt(TicksToNextFire(xRig, fDt, 200), 42,
				"parked ticks FREEZE the accumulator (42 = 82 - 40; reset would be 82, accumulating 1)");
		}
	}

	void Setup_TennisBrainTickCadence()
	{
		ResetContractResults();
		g_bCadenceRan = false;
	}

	bool Step_TennisBrainTickCadence(int /*iFrame*/)
	{
		RunTickCadenceChecks();
		g_bCadenceRan = true;
		return false;   // one frame is all this needs - the graph is driven by hand
	}

	bool Verify_TennisBrainTickCadence()
	{
		CheckTrue(g_bCadenceRan, "the cadence checks ran");
		return ReportContract("RT_TennisBrainTickCadence");
	}
}

static const Zenith_AutomatedTest g_xTennisBrainTickCadenceTest = {
	"RT_TennisBrainTickCadence",
	&Setup_TennisBrainTickCadence,
	&Step_TennisBrainTickCadence,
	&Verify_TennisBrainTickCadence,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTennisBrainTickCadenceTest);

// ============================================================================
// RT_TennisBrainGateOrder - contract clause 2.
// ============================================================================

namespace
{
	bool g_bGateOrderRan = false;

	void RunGateOrderChecks()
	{
		TennisBrainRig xRig;
		CheckTrue(xRig.bRigValid, "rig built the production brain graph with no unresolved nodes");

		// ---- authored structure ---------------------------------------------
		const u_int uOnUpdate = xRig.FindOnlyNodeOfType("OnUpdate");
		const u_int uSelector = xRig.FindOnlyNodeOfType("Selector");
		CheckTrue(uOnUpdate != 0, "exactly one OnUpdate anchor");
		CheckTrue(uSelector != 0, "exactly one Selector");
		if (uOnUpdate == 0 || uSelector == 0)
			return;

		char acChain[512];
		xRig.FormatChain(uOnUpdate, 0, acChain, sizeof(acChain));
		CheckEqStr(acChain, k_szTickSpineChain,
			"tick spine: gate BEFORE accumulate (freeze semantics), then compare/gate/reset/Selector");

		// The Selector's pin ASSIGNMENT is the priority order: pin 0 serve,
		// pin 1 rally, pin 2 recover. Swapping the authored pin indices moves a
		// whole chain here.
		const u_int uServeHead = xRig.SuccessorOf(uSelector, 0);
		const u_int uRallyHead = xRig.SuccessorOf(uSelector, 1);
		const u_int uRecoverHead = xRig.SuccessorOf(uSelector, 2);
		CheckTrue(uServeHead != 0 && uRallyHead != 0 && uRecoverHead != 0, "all three Selector pins are wired");
		if (uServeHead == 0 || uRallyHead == 0 || uRecoverHead == 0)
			return;

		xRig.FormatChain(uSelector, 0, acChain, sizeof(acChain));
		CheckEqStr(acChain, k_szServePinChain, "Selector pin 0 is the SERVE branch");
		xRig.FormatChain(uSelector, 1, acChain, sizeof(acChain));
		CheckEqStr(acChain, k_szRallyPinChain, "Selector pin 1 is the RALLY branch");
		xRig.FormatChain(uSelector, 2, acChain, sizeof(acChain));
		CheckEqStr(acChain, k_szRecoverPinChain, "Selector pin 2 is the RECOVER fallback");

		// ---- runtime evaluation order ---------------------------------------
		// One firing tick per scenario, at a dt that fires regardless of where the
		// threshold sits (that value is RT_TennisBrainTickCadence's business).
		const float fFiringDt = k_fAlwaysFiringDt;

		// SERVE: pin 0 succeeds, so the lower-priority pins are never evaluated.
		// That is the first-success-wins half of the contract.
		xRig.ScriptServePhase(/*second*/false);
		xRig.Tick(fFiringDt);
		CheckTrue(xRig.SelectorFired(), "serve scenario reached the Selector");
		CheckTrue(xRig.TraceHasType("RTTennisDecideServe"), "serve scenario ran the serve branch");
		CheckEqInt(xRig.TraceIndexOfNode(uRallyHead), -1, "a successful serve pin never evaluates the rally pin");
		CheckEqInt(xRig.TraceIndexOfNode(uRecoverHead), -1, "a successful serve pin never evaluates the recover pin");

		// RALLY: pin 0's gate closes and pin 1 is tried next. Its
		// RTTennisBallReachable fails with no ball published, so pin 2 runs. All
		// three heads execute, and the ORDER is the priority order.
		xRig.ScriptRallyPhase();
		xRig.Tick(fFiringDt);
		const int iServeAt = xRig.TraceIndexOfNode(uServeHead);
		const int iRallyAt = xRig.TraceIndexOfNode(uRallyHead);
		const int iRecoverAt = xRig.TraceIndexOfNode(uRecoverHead);
		CheckTrue(iServeAt >= 0, "rally scenario evaluated the serve pin first");
		CheckTrue(iRallyAt >= 0, "rally scenario evaluated the rally pin");
		CheckTrue(iRecoverAt >= 0, "rally scenario fell through to the recover pin");
		CheckTrue(iServeAt >= 0 && iRallyAt > iServeAt, "serve pin is evaluated BEFORE the rally pin");
		CheckTrue(iRallyAt >= 0 && iRecoverAt > iRallyAt, "rally pin is evaluated BEFORE the recover pin");
		CheckTrue(xRig.TraceHasType("RTTennisBallReachable"), "rally branch reached the reachability gate");
		CheckTrue(!xRig.TraceHasType("RTTennisDecideServe"), "a closed serve gate never reaches DecideServe");

		// NON-PLAY phase: both gated pins close, only the fallback acts.
		xRig.ScriptPointOverPhase();
		xRig.Tick(fFiringDt);
		CheckTrue(xRig.TraceHasType("RTTennisRecoverToReady"), "point-over falls through to recover");
		CheckTrue(!xRig.TraceHasType("RTTennisDecideServe"), "point-over never decides a serve");
		CheckTrue(!xRig.TraceHasType("RTTennisDecideShot"), "point-over never decides a shot");

		// A PARKED agent runs nothing at all - the gate is the first node in the
		// spine, so not even the accumulator executes.
		xRig.Agent().SetEnabled(false);
		xRig.ScriptServePhase(/*second*/false);
		xRig.Tick(fFiringDt);
		CheckEqInt(static_cast<int>(xRig.xGraph.GetRecentlyExecuted().GetSize()), 1,
			"a parked agent executes ONLY the tick gate");
		CheckTrue(!xRig.SelectorFired(), "a parked agent never reaches the Selector");
	}

	void Setup_TennisBrainGateOrder()
	{
		ResetContractResults();
		g_bGateOrderRan = false;
	}

	bool Step_TennisBrainGateOrder(int /*iFrame*/)
	{
		RunGateOrderChecks();
		g_bGateOrderRan = true;
		return false;
	}

	bool Verify_TennisBrainGateOrder()
	{
		CheckTrue(g_bGateOrderRan, "the gate-order checks ran");
		return ReportContract("RT_TennisBrainGateOrder");
	}
}

static const Zenith_AutomatedTest g_xTennisBrainGateOrderTest = {
	"RT_TennisBrainGateOrder",
	&Setup_TennisBrainGateOrder,
	&Step_TennisBrainGateOrder,
	&Verify_TennisBrainGateOrder,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTennisBrainGateOrderTest);

// ============================================================================
// RT_TennisBrainRngDraws - contract clause 3.
//
// The draw counts below are the STRUCTURE of each decision, not magic numbers:
//   SelectServe, first serve  = 3  (placement coin + NextInDisc aim scatter x2)
//   SelectServe, second serve = 2  (no placement coin: the body serve is fixed)
//   SelectShot,  neutral      = 1  (the NextSigned placement noise)
// Adding or removing a draw in either decision core moves exactly one of them.
// ============================================================================

namespace
{
	bool g_bRngDrawsRan = false;

	void RunRngDrawChecks()
	{
		// ---- draws happen ONLY on firing ticks -------------------------------
		// The sub-threshold dt and the tick period are DISCOVERED, not pinned, so
		// this clause survives a deliberate cadence change - the invariant is "an
		// accumulate-only tick draws nothing", whatever the period happens to be.
		{
			TennisBrainRig xRig;
			CheckTrue(xRig.bRigValid, "rig built the production brain graph with no unresolved nodes");
			xRig.ScriptServePhase(/*second*/false);

			const float fDt = 1.0f / 1024.0f;
			const uint32_t uSeeded = xRig.RngState();
			int iAccumulateOnlyTicks = 0;
			int iStrayDrawAtTick = -1;
			while (iAccumulateOnlyTicks < 400)
			{
				xRig.Tick(fDt);
				if (xRig.SelectorFired())
					break;
				++iAccumulateOnlyTicks;
				// Checked every tick rather than once at the end: a single stray draw
				// on any accumulate frame is the regression, and this names the tick.
				if (RngDrawsBetween(uSeeded, xRig.RngState()) != 0)
				{
					iStrayDrawAtTick = iAccumulateOnlyTicks;
					break;
				}
			}
			CheckEqInt(iStrayDrawAtTick, -1, "no accumulate-only tick draws from the brain RNG");
			CheckTrue(iAccumulateOnlyTicks > 0, "the brain accumulates over several sub-threshold ticks");
			CheckTrue(xRig.SelectorFired(), "a tick eventually fires");

			// The next period draws nothing until it fires again.
			const uint32_t uAfterFire = xRig.RngState();
			for (int i = 0; i < iAccumulateOnlyTicks; ++i)
				xRig.Tick(fDt);
			CheckTrue(!xRig.SelectorFired(), "still accumulating one tick short of the next fire");
			CheckEqInt(RngDrawsBetween(uAfterFire, xRig.RngState()), 0,
				"no draws between firing ticks");
		}

		// ---- the first-serve decision's exact draw count ---------------------
		{
			TennisBrainRig xRig;
			xRig.ScriptServePhase(/*second*/false);
			const uint32_t uBefore = xRig.RngState();
			xRig.Tick(k_fAlwaysFiringDt);
			CheckTrue(xRig.SelectorFired(), "first-serve scenario fired");
			CheckEqInt(RngDrawsBetween(uBefore, xRig.RngState()), 3,
				"a first-serve decide tick draws exactly 3 (placement coin + aim disc)");
		}

		// ---- the second serve takes a different, fixed number of draws --------
		{
			TennisBrainRig xRig;
			xRig.ScriptServePhase(/*second*/true);
			const uint32_t uBefore = xRig.RngState();
			xRig.Tick(k_fAlwaysFiringDt);
			CheckTrue(xRig.SelectorFired(), "second-serve scenario fired");
			CheckEqInt(RngDrawsBetween(uBefore, xRig.RngState()), 2,
				"a second-serve decide tick draws exactly 2 (aim disc only)");
		}

		// ---- an ARMED brain draws nothing (the don't-re-roll-mid-swing guard) --
		{
			TennisBrainRig xRig;
			xRig.ScriptServePhase(/*second*/false);
			RenderTest_Tennis::TennisShotDecision xCommitted;
			xCommitted.m_xAim = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
			xRig.Brain().SetDecidedShot(xCommitted);
			xRig.Brain().ArmDecidedShot(1u);
			CheckTrue(xRig.Brain().IsArmed(), "brain is armed for the epoch on the blackboard");

			const uint32_t uBefore = xRig.RngState();
			for (int i = 0; i < 5; ++i)
				xRig.Tick(k_fAlwaysFiringDt);   // five FIRING ticks
			CheckEqInt(RngDrawsBetween(uBefore, xRig.RngState()), 0,
				"an armed brain draws NOTHING across five firing ticks");
			CheckTrue(xRig.Brain().IsArmed(), "the committed decision survives");
		}

		// ---- a parked agent draws nothing ------------------------------------
		{
			TennisBrainRig xRig;
			xRig.ScriptServePhase(/*second*/false);
			xRig.Agent().SetEnabled(false);
			const uint32_t uBefore = xRig.RngState();
			for (int i = 0; i < 50; ++i)
				xRig.Tick(k_fAlwaysFiringDt);
			CheckEqInt(RngDrawsBetween(uBefore, xRig.RngState()), 0,
				"a parked agent draws NOTHING however long it is parked");
		}

		// ---- the rally decide node's draw count ------------------------------
		// The rally branch's RTTennisBallReachable gate needs live perception, which
		// no hermetic rig can supply, so the node is executed directly against the
		// same blackboard the graph would hand it - the sibling-unit-test pattern.
		{
			TennisBrainRig xRig;
			xRig.ScriptRallyPhase();
			Zenith_GraphContext xContext = xRig.MakeContext(k_fAlwaysFiringDt);

			const uint32_t uBefore = xRig.RngState();
			RTNode_TennisDecideShot xDecideShot;
			const GraphNodeStatus eStatus = xDecideShot.Execute(xContext);
			CheckEqInt(static_cast<int>(eStatus), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
				"DecideShot succeeds for an un-armed brain");
			CheckEqInt(RngDrawsBetween(uBefore, xRig.RngState()), 1,
				"a neutral rally decision draws exactly 1 (the placement noise)");
			CheckTrue(!xRig.Brain().IsArmed(), "DecideShot stores the decision UNARMED");

			// Armed: the same early-SUCCESS guard, zero draws.
			xRig.Brain().ArmDecidedShot(1u);
			const uint32_t uArmed = xRig.RngState();
			xDecideShot.Execute(xContext);
			CheckEqInt(RngDrawsBetween(uArmed, xRig.RngState()), 0,
				"an armed brain's DecideShot draws NOTHING");
		}

		// ---- the seed itself --------------------------------------------------
		// Every draw count above is relative; this pins the absolute start of the
		// near side's stream, so a re-seed cannot slide the whole match unnoticed.
		{
			TennisBrainRig xRig;
			CheckEqInt(static_cast<int>(xRig.RngState()), static_cast<int>(0x1234567u),
				"the near-side brain re-derives its seed on OnStart");
		}
	}

	void Setup_TennisBrainRngDraws()
	{
		ResetContractResults();
		g_bRngDrawsRan = false;
	}

	bool Step_TennisBrainRngDraws(int /*iFrame*/)
	{
		RunRngDrawChecks();
		g_bRngDrawsRan = true;
		return false;
	}

	bool Verify_TennisBrainRngDraws()
	{
		CheckTrue(g_bRngDrawsRan, "the RNG-draw checks ran");
		return ReportContract("RT_TennisBrainRngDraws");
	}
}

static const Zenith_AutomatedTest g_xTennisBrainRngDrawsTest = {
	"RT_TennisBrainRngDraws",
	&Setup_TennisBrainRngDraws,
	&Step_TennisBrainRngDraws,
	&Verify_TennisBrainRngDraws,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTennisBrainRngDrawsTest);

#endif // ZENITH_INPUT_SIMULATOR
