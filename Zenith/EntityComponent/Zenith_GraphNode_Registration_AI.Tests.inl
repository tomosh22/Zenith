//------------------------------------------------------------------------------
// Routable-FAILURE coverage for the AI node TU. Included at the bottom of
// Zenith_GraphNode_Registration_AI.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes are still in scope.
//
// ONE table-driven test per owning TU. What each row proves, why the wired
// fail-probe is the positive control, and why the anchor/probes are engine
// nodes addressed by name all live ONCE, in the shared harness:
// Zenith_GraphNodeFailurePin.TestHarness.inl. This file carries only what is
// specific to this TU - the scene fixture, the rows, and the non-opted node.
//
// The harness's positive control earns its keep hardest HERE: EnsureNavAgent's
// other non-SUCCESS status is RUNNING, which looks identical to a routed
// FAILURE from pin 0's side (neither successor runs). Only the fail-probe
// separates them.
//
// CROSS-REFERENCES, deliberately not duplicated here:
//   * the runtime SEMANTICS of the pin are the FailurePin_* tests in
//     Zenith/Scripting/Zenith_Scripting.Tests.inl;
//   * EnsureNavAgent's full status table without a mesh - including the
//     RUNNING row this file cannot reach - is
//     Zenith_GraphComponent.Tests.inl:3929-3977, and the perception fixtures
//     with a REGISTERED agent are :3101-3136.
//
// NavMoveTo is deliberately NOT in this table: it is a RUNNING/suspending node
// whose failure handling interacts with the chain cursor, so it stays out of
// the opt-in set until that interaction is designed.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphNodeFailurePin.TestHarness.inl"
#include "UnitTests/Zenith_TempScene.h"

ZENITH_TEST(GraphNodeFailurePin, AIOptIns)
{
	// Self is a REAL agent entity, so every row below fails on its own named
	// branch rather than on the invalid-target guard each node opens with.
	Zenith_TempScene xTempScene("FailurePinAIScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("FailurePinAgent");
	xSelf.AddComponent<Zenith_AIAgentComponent>();

	// ★ AN UNCONFIGURED NavMeshComponent, NEVER A CONFIGURED-BUT-UNLOADED ONE.
	// EnsureNavAgent reads the ASSET REF to tell "nothing is coming" (FAILURE) from
	// "it is coming next frame" (RUNNING). An empty ref is the first; a real ref
	// whose deferred OnStart has not run is the second, and wiring a failure
	// handler for THAT would fire on a mesh that then loads.
	Zenith_Entity xNavHolder = xTempScene.CreateEntity("FailurePinNavHolder");
	Zenith_NavMeshComponent& xNavMesh = xNavHolder.AddComponent<Zenith_NavMeshComponent>();
	ZENITH_ASSERT_TRUE(xNavMesh.GetAssetRef().empty(), "the fixture needs an UNCONFIGURED navmesh ref");

	const Zenith_FailurePinCase axCases[] =
	{
		// EnsureNavAgent: FAILURE on the UNCONFIGURED ref - nothing is coming.
		// The target has a Zenith_AIAgentComponent and the active scene has a
		// Zenith_NavMeshComponent, so the no-target / no-AIAgentComponent /
		// no-NavMeshComponent misconfiguration guards are all cleared first.
		//
		// ★ NO LINE NUMBERS. Every citation in this file used to name one and
		// three of them were already wrong; the guard's CONDITION does not rot.
		{ "EnsureNavAgent", nullptr },
		// SetNavDestination: FAILURE on NO BOUND AGENT. Nothing in this fixture
		// ever binds one - EnsureNavAgent returns above its allocation - so the
		// node reaches its own gate.
		//
		// ★ The configure fn exists only to DECLARE what the node READS.
		// m_strDestinationVar defaults to "target" and its pin is a
		// TARGET_POSITION ref, so without a declaration the harness graph
		// carries an UNDECLARED_READ - an ERROR since A-8 - and
		// Zenith_BuildFailurePinGraph's Build() would return false. VECTOR3 is
		// one of the two types that mask accepts. The tested branch is
		// untouched: the node FAILs at the no-agent guard before it ever
		// resolves the reference.
		{
			"SetNavDestination",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				(void)uNode;
				Zenith_PropertyValue xDestination;
				xDestination.SetVector3(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
				xBuilder.Variable("target", xDestination);
			}
		},
		// FindRandomReachablePoint: FAILURE on "no bound agent, hence no mesh" -
		// the shallowest of its three, and the only one this table's agent-less
		// fixture can reach. The "no reachable point in the radius" branch needs a
		// navmesh: the pin-runtime rows at the bottom of this file HAND-BUILD one
		// (a 10x10 quad, no bake and no asset) and cover it, as does
		// GraphComponent.AINavPerceptionNodesExecution.
		{ "FindRandomReachablePoint", nullptr },
		// QueryPrimaryPerceivedTarget: FAILURE on "nothing perceived". Self is a
		// valid entity that was never registered as a perception agent, so
		// GetPrimaryTarget answers INVALID_ENTITY_ID without the system being
		// initialised at all.
		{ "QueryPrimaryPerceivedTarget", nullptr },
		// QueryLastHeardSound: FAILURE on "nothing heard" - the same
		// unregistered-agent route, via a default-constructed
		// Zenith_LastHeardSound whose m_bValid is false.
		{ "QueryLastHeardSound", nullptr },
	};

	Zenith_CheckFailurePinTable(axCases, static_cast<u_int>(sizeof(axCases) / sizeof(axCases[0])), xSelf);

	// Nothing in the fixture may have allocated an agent behind our back - a
	// bound agent would make three of the rows above pass for the wrong reason.
	ZENITH_ASSERT_NULL(xSelf.GetComponent<Zenith_AIAgentComponent>().GetNavMeshAgent(),
		"a failing EnsureNavAgent must not have allocated an agent");

	// StopNav's only FAILURE is "no agent to stop", which a teardown chain has
	// no handling for.
	Zenith_CheckNodeIsNotOptedIn("StopNav");
}

//------------------------------------------------------------------------------
// Pin-table coverage for this TU. What the totality walk proves, why the
// registry is SWAPPED rather than filtered, and why the restore is RAII all live
// ONCE, in Zenith_GraphPinTotality.TestHarness.inl; only this TU's registrar and
// its representative pins are here.
//------------------------------------------------------------------------------

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, AITotality)
{
	// No exemptions: every m_str*Var* property in this TU is expressible as a
	// pin - m_strListVar names ONE list, not a comma-separated set.
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_AI, "_AI.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, AIRoleSpotCheck)
{
	// LIST: the blackboard's parallel list store, which holds no
	// Zenith_PropertyValue and is therefore never typed. Count beside it is an
	// ordinary computed OUTPUT.
	Zenith_CheckGraphPin("QueryPerceivedTargets", "List", GRAPH_PIN_ROLE_LIST, eGRAPH_PIN_TYPE_ANY, "m_strListVar");
	Zenith_CheckGraphPin("QueryPerceivedTargets", "Count", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "m_strCountVar");

	// INPUT_VAR_OR_CONST with both halves bound.
	Zenith_CheckGraphPin("SetNavSpeed", "Speed", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "m_strSpeedVar");
	const Zenith_GraphPinDesc* pxSpeed = Zenith_FindGraphPin("SetNavSpeed", "Speed");
	ZENITH_ASSERT_NOT_NULL(pxSpeed, "SetNavSpeed must declare a Speed pin");
	if (pxSpeed != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxSpeed->m_szConstProperty, "m_fSpeed",
			"SetNavSpeed.Speed lost its inline-constant half");
	}

	// ★ The two TARGET flavours differ ONLY in the accepted-type mask: a
	// destination may be an EntityID (follow) or a VECTOR3 (a fixed point),
	// while the mover itself must be an EntityID.
	const Zenith_GraphPinDesc* pxDestination = Zenith_FindGraphPin("NavMoveTo", "Destination");
	const Zenith_GraphPinDesc* pxMover = Zenith_FindGraphPin("NavMoveTo", "Target");
	ZENITH_ASSERT_NOT_NULL(pxDestination, "NavMoveTo must declare a Destination pin");
	ZENITH_ASSERT_NOT_NULL(pxMover, "NavMoveTo must declare a Target pin");
	if (pxDestination != nullptr && pxMover != nullptr)
	{
		ZENITH_ASSERT_EQ(static_cast<int>(pxDestination->m_eRole), static_cast<int>(GRAPH_PIN_ROLE_TARGET_REF),
			"NavMoveTo.Destination is a runtime-resolved reference, not a value input");
		ZENITH_ASSERT_EQ(pxDestination->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_POSITION,
			"NavMoveTo.Destination must accept ENTITY_ID or VECTOR3");
		ZENITH_ASSERT_EQ(pxMover->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY,
			"NavMoveTo.Target must accept a packed ENTITY_ID and nothing else");
	}

	// Nav state reads are the node's own computed OUTPUTs - m_strStateVar holds
	// the 0-3 code this node derives, not a state NAME.
	Zenith_CheckGraphPin("ReadNavState", "State", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "m_strStateVar");

	// ★ SAME PROPERTY NAME, OPPOSITE ROLES: QueryLastHeardSound WRITES the heard
	// position; EmitSoundStimulus RESOLVES one to emit at. The role follows the
	// Execute body, never the spelling.
	Zenith_CheckGraphPin("QueryLastHeardSound", "Position", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR3, "m_strPositionVar");
	Zenith_CheckGraphPin("EmitSoundStimulus", "Position", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strPositionVar");
}

//==============================================================================
// PIN RUNTIME for this TU (B-6.5) - the pins are LIVE.
//
// Every node above now reads its INPUT descriptors through
// Zenith_GraphNode::GetInput and writes its OUTPUT descriptors through SetOutput.
// The three tests ABOVE, plus GraphComponent.AINavPerceptionNodesExecution (whose
// navState / navLeft / wanderPoint / perceivedCount / ptgt / awareness / heardSrc
// all arrive through the dual-write, and whose SetNavSpeed instance binds NO var
// and therefore takes the CONST path), are the proof that an UNCONNECTED node is
// unchanged. The rows below are the proof that a WIRE now carries a value.
//
// ★ EVERY ROW ASSERTS THE EXECUTE STATUS FIRST. Every node here opens with a
// resolver guard, so a row that checked a slot without checking the status would
// pass just as happily on a node that did nothing at all.
//
// ★ TWO FAILURE SHAPES, and the rows say which they pin.
//   SHAPE A - ReadNavState, QueryPerceivedTargets, QueryPrimaryPerceivedTarget,
//   QueryLastHeardSound, QueryAwarenessOf: the FAILURE is above every accessor,
//   so a failed directly-constructed instance never self-bound and
//   GetOutputForTest reads NULL, not a stamped zero.
//   SHAPE B - FindRandomReachablePoint's no-reachable-point FAILURE alone: it runs
//   AFTER the Radius GetInput, so pin state HAS been built and Result reads its
//   stamped (0,0,0). Its no-mesh and unresolvable-centre FAILUREs are Shape A.
//
// ★ ORDERING RULE (B-6.1): pin state is built ONCE, on the first accessor call,
// from the properties as they read THEN. Assign every property before the first
// Execute, and use a FRESH node for a leg that changes a var NAME. A const
// property is read LIVE through its reflected pointer.
//
// ★ THE FIXTURE IS REAL AND HEADLESS. The navmesh is the 10x10 quad
// GraphComponent.AINavPerceptionNodesExecution hand-builds (four AddVertex, one
// AddPolygon, ComputeSpatialData, BuildSpatialGrid - no bake, no asset, no
// ComputeAdjacency), and the perception system is the ordinary static one.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

// One row of the index contract: the constant (or the literal, for a pin no
// Execute addresses) names the pin it is documented as, with the role the
// migration assumed.
inline void AIPin_Check(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
	Zenith_GraphPinRole eRole, const char* szClass)
{
	ZENITH_ASSERT_LT(uIndex, xPins.GetPinCount(), "%s: pin index %u is past the end of the table", szClass, uIndex);
	if (uIndex >= xPins.GetPinCount())
	{
		return;
	}
	const Zenith_GraphPinDesc& xDesc = xPins.GetPinAt(uIndex);
	ZENITH_ASSERT_STREQ(xDesc.m_szName, szName,
		"%s: pin %u is '%s', not '%s' - the table was REORDERED, so every uPIN_ constant now addresses the wrong pin",
		szClass, uIndex, xDesc.m_szName != nullptr ? xDesc.m_szName : "(null)", szName);
	ZENITH_ASSERT_EQ(static_cast<int>(xDesc.m_eRole), static_cast<int>(eRole),
		"%s: pin '%s' changed ROLE - its runtime accessor would bad-access", szClass, szName);
}

inline void AIPin_SeedFloat(Zenith_GraphBlackboard& xBB, const char* szName, float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	xBB.SetValue(szName, xValue);
}

inline void AIPin_SeedInt(Zenith_GraphBlackboard& xBB, const char* szName, int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	xBB.SetValue(szName, xValue);
}

inline void AIPin_SeedVec3(Zenith_GraphBlackboard& xBB, const char* szName, const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	xBB.SetValue(szName, xValue);
}

inline void AIPin_SeedEntity(Zenith_GraphBlackboard& xBB, const char* szName, u_int64 ulPacked)
{
	Zenith_PropertyValue xValue;
	xValue.SetPackedEntityID(ulPacked);
	xBB.SetValue(szName, xValue);
}

inline Zenith_PropertyValue AIPin_WireFloat(float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	return xValue;
}

inline Zenith_PropertyValue AIPin_WireVec3(const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	return xValue;
}

class Zenith_GraphNode_AITestCountingRadiusProducer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AITestCountingRadiusProducer)
public:
	ZENITH_PROPERTY(std::string, m_strUnusedOutput, "")
	static constexpr u_int uPIN_Value = 0u;
	ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_AITestCountingRadiusProducer)
	ZENITH_GRAPH_PIN_OUTPUT(Value, "m_strUnusedOutput", PROPERTY_TYPE_FLOAT)
	ZENITH_GRAPH_PINS_END

public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		++s_uPullCount;
		SetOutput<float>(xContext, uPIN_Value, s_fValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "Test_AICountingRadiusProducer"; }
	inline static u_int s_uPullCount = 0u;
	inline static float s_fValue = 0.0f;
};

static void EnsureAICountingRadiusProducerRegistered()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	if (xRegistry.Find("Test_AICountingRadiusProducer") == nullptr)
	{
		xRegistry.RegisterNodeType<Zenith_GraphNode_AITestCountingRadiusProducer>(
			"Test_AICountingRadiusProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	}
}

static void AIPin_ClearOutputs(Zenith_GraphNode_ReadNavState& xNode)
{ xNode.m_strStateVar = ""; xNode.m_strRemainingVar = ""; xNode.m_strVelocityVar = ""; }
static void AIPin_ClearOutputs(Zenith_GraphNode_FindRandomReachablePoint& xNode)
{ xNode.m_strResultVar = ""; }
static void AIPin_ClearOutputs(Zenith_GraphNode_QueryPerceivedTargets& xNode)
{ xNode.m_strCountVar = ""; }
static void AIPin_ClearOutputs(Zenith_GraphNode_QueryPrimaryPerceivedTarget& xNode)
{ xNode.m_strResultVar = ""; }
static void AIPin_ClearOutputs(Zenith_GraphNode_QueryLastHeardSound& xNode)
{ xNode.m_strPositionVar = ""; xNode.m_strSourceVar = ""; xNode.m_strAgeVar = ""; }
static void AIPin_ClearOutputs(Zenith_GraphNode_QueryAwarenessOf& xNode)
{ xNode.m_strResultVar = ""; }

// The slot readers are TAG-CHECKED: Zenith_PropertyValue's typed getters
// Zenith_Assert on a mismatch, and a wrong slot type must read as a test FAILURE
// rather than a DebugBreak.
inline int32_t AIPin_SlotInt(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_INT32),
		"%s: the slot holds type %u, not INT32", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_INT32 ? pxSlot->GetInt32() : 0;
}

inline float AIPin_SlotFloat(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0.0f;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_FLOAT),
		"%s: the slot holds type %u, not FLOAT", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_FLOAT ? pxSlot->GetFloat() : 0.0f;
}

inline Zenith_Maths::Vector3 AIPin_SlotVec3(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return Zenith_Maths::Vector3(0.0f);
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_VECTOR3),
		"%s: the slot holds type %u, not VECTOR3", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_VECTOR3 ? pxSlot->GetVector3() : Zenith_Maths::Vector3(0.0f);
}

inline u_int64 AIPin_SlotEntity(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0ull;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_ENTITY_ID),
		"%s: the slot holds type %u, not ENTITY_ID", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_ENTITY_ID ? pxSlot->GetPackedEntityID() : 0ull;
}

namespace
{
	// The navigation fixture: a hand-built 10x10 navmesh quad, a scene, an agent
	// entity whose Zenith_AIAgentComponent BORROWS a stack Zenith_NavMeshAgent
	// bound to that mesh, and a plain entity with NO Zenith_AIAgentComponent (the
	// guarded-FAILURE leg's target).
	//
	// ★ MEMBER ORDER IS LOAD-BEARING, and it is the GraphComponent fixture's own.
	// m_xNavMesh and m_xNavAgent are declared BEFORE m_xScene, so they are
	// DESTROYED AFTER it: the component only borrows the agent pointer, and the
	// scene holding that component has to go first.
	//
	// ★ A FRESH FIXTURE MEANS A FRESH MESH, which matters: the mesh's sampling RNG
	// is a per-INSTANCE fixed-seed xorshift that ADVANCES on every
	// GetRandomReachablePointInRadius call, so two legs sharing one mesh do not
	// sample the same stream.
	struct Zenith_AIPinFixture
	{
		explicit Zenith_AIPinFixture(const char* szSceneName)
			: m_xScene(szSceneName)
		{
			m_xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
			m_xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
			m_xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
			m_xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));
			Zenith_Vector<uint32_t> axIndices;
			axIndices.PushBack(0);
			axIndices.PushBack(1);
			axIndices.PushBack(2);
			axIndices.PushBack(3);
			m_xNavMesh.AddPolygon(axIndices);
			m_xNavMesh.ComputeSpatialData();
			m_xNavMesh.BuildSpatialGrid();

			m_xAgent = m_xScene.CreateEntity("AIPinAgent");
			m_xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
			m_xNavAgent.SetNavMesh(&m_xNavMesh);
			m_xAgent.AddComponent<Zenith_AIAgentComponent>().SetNavMeshAgent(&m_xNavAgent);

			// A transform, no Zenith_AIAgentComponent: every nav node's resolver
			// FAILS on it, above every accessor.
			m_xNoAgent = m_xScene.CreateEntity("AIPinNoAgent");
		}

		Zenith_AIPinFixture(const Zenith_AIPinFixture&) = delete;
		Zenith_AIPinFixture& operator=(const Zenith_AIPinFixture&) = delete;

		Zenith_NavMesh m_xNavMesh;
		Zenith_NavMeshAgent m_xNavAgent;
		Zenith_TempScene m_xScene;
		Zenith_Entity m_xAgent;
		Zenith_Entity m_xNoAgent;
	};

	// ★ RAII, and not as a nicety. Initialise() and Shutdown() each CLEAR every
	// perception bucket, so a row must own both ends: a row that returned early
	// between them would leave its agents and sounds visible to the next row, and
	// "Count == 1" only means anything on a freshly-initialised system.
	struct Zenith_AIPerceptionScope
	{
		Zenith_AIPerceptionScope() { Zenith_PerceptionSystem::Initialise(); }
		~Zenith_AIPerceptionScope() { Zenith_PerceptionSystem::Shutdown(); }
		Zenith_AIPerceptionScope(const Zenith_AIPerceptionScope&) = delete;
		Zenith_AIPerceptionScope& operator=(const Zenith_AIPerceptionScope&) = delete;
	};

	// Seer at the origin, prey 5 m along +Z. A default entity faces +Z, which is
	// the only reason the prey is inside a 90-degree FOV - it is a property of the
	// fixture, not of the node.
	struct Zenith_AIPerceptionFixture
	{
		explicit Zenith_AIPerceptionFixture(const char* szSceneName)
			: m_xScene(szSceneName)
		{
			m_xSeer = m_xScene.CreateEntity("AIPinSeer");
			m_xPrey = m_xScene.CreateEntity("AIPinPrey");
			m_xSeer.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
			m_xPrey.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));
		}

		Zenith_AIPerceptionFixture(const Zenith_AIPerceptionFixture&) = delete;
		Zenith_AIPerceptionFixture& operator=(const Zenith_AIPerceptionFixture&) = delete;

		// Registers the seer as an agent that can SEE (LOS off - a unit boot has
		// no world to raycast against) and HEAR, and the prey as a hostile TARGET,
		// then ticks once. The GraphComponent suite's recipe, verbatim. MUST be
		// called INSIDE a Zenith_AIPerceptionScope: Initialise() clears every
		// bucket, so registering before it registers nothing.
		void RegisterSeerAndPrey() const
		{
			Zenith_PerceptionSystem::RegisterAgent(m_xSeer.GetEntityID());
			Zenith_SightConfig xSight;
			xSight.m_fMaxRange = 20.0f;
			xSight.m_fFOVAngle = 90.0f;
			xSight.m_bRequireLineOfSight = false;
			Zenith_PerceptionSystem::SetSightConfig(m_xSeer.GetEntityID(), xSight);
			Zenith_HearingConfig xHearing;	// defaults: 20 m range, 0.1 loudness threshold
			Zenith_PerceptionSystem::SetHearingConfig(m_xSeer.GetEntityID(), xHearing);
			Zenith_PerceptionSystem::RegisterTarget(m_xPrey.GetEntityID(), true);
			Zenith_PerceptionSystem::Update(0.1f);
		}

		Zenith_TempScene m_xScene;
		Zenith_Entity m_xSeer;
		Zenith_Entity m_xPrey;
	};
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses, so a
// reorder - or an inserted pin - silently re-points every uPIN_ constant in this TU
// at a different descriptor, and a SetOutput aimed at a TARGET_REF pin is a SILENT
// no-op. SIX of the thirteen classes declare no constant at all (their Execute
// addresses no pin); asserting their COUNT and their index 0 here is what would
// notice a value pin being ADDED to one without its Execute being migrated. A
// static_assert is impossible: the tables are filled at static init.
ZENITH_TEST(GraphPinTable, AIPinIndicesMatchTables)
{
	// ---- the six that declare no uPIN_ constant --------------------------------
	const Zenith_GraphPinTable& xEnsure = Zenith_GraphNode_EnsureNavAgent::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xEnsure.GetPinCount(), 2u, "EnsureNavAgent gained or lost a pin");
	AIPin_Check(xEnsure, 0u, "NavMesh", GRAPH_PIN_ROLE_TARGET_REF, "EnsureNavAgent");
	AIPin_Check(xEnsure, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "EnsureNavAgent");

	const Zenith_GraphPinTable& xMoveTo = Zenith_GraphNode_NavMoveTo::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMoveTo.GetPinCount(), 2u, "NavMoveTo gained or lost a pin");
	AIPin_Check(xMoveTo, 0u, "Destination", GRAPH_PIN_ROLE_TARGET_REF, "NavMoveTo");
	AIPin_Check(xMoveTo, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "NavMoveTo");

	const Zenith_GraphPinTable& xSetDest = Zenith_GraphNode_SetNavDestination::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetDest.GetPinCount(), 2u, "SetNavDestination gained or lost a pin");
	AIPin_Check(xSetDest, 0u, "Destination", GRAPH_PIN_ROLE_TARGET_REF, "SetNavDestination");
	AIPin_Check(xSetDest, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetNavDestination");

	const Zenith_GraphPinTable& xStop = Zenith_GraphNode_StopNav::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xStop.GetPinCount(), 1u, "StopNav gained a pin; nothing in its Execute addresses one");
	AIPin_Check(xStop, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "StopNav");

	const Zenith_GraphPinTable& xEmit = Zenith_GraphNode_EmitSoundStimulus::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xEmit.GetPinCount(), 2u, "EmitSoundStimulus gained or lost a pin");
	AIPin_Check(xEmit, 0u, "Position", GRAPH_PIN_ROLE_TARGET_REF, "EmitSoundStimulus");
	AIPin_Check(xEmit, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "EmitSoundStimulus");

	const Zenith_GraphPinTable& xRegister = Zenith_GraphNode_RegisterPerceptionTarget::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRegister.GetPinCount(), 1u, "RegisterPerceptionTarget gained a pin");
	AIPin_Check(xRegister, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "RegisterPerceptionTarget");

	// ---- the seven that do -----------------------------------------------------
	const Zenith_GraphPinTable& xReadNav = Zenith_GraphNode_ReadNavState::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xReadNav.GetPinCount(), 4u, "ReadNavState gained or lost a pin");
	AIPin_Check(xReadNav, Zenith_GraphNode_ReadNavState::uPIN_State, "State", GRAPH_PIN_ROLE_OUTPUT, "ReadNavState");
	AIPin_Check(xReadNav, Zenith_GraphNode_ReadNavState::uPIN_Remaining, "Remaining", GRAPH_PIN_ROLE_OUTPUT,
		"ReadNavState");
	AIPin_Check(xReadNav, Zenith_GraphNode_ReadNavState::uPIN_Velocity, "Velocity", GRAPH_PIN_ROLE_OUTPUT,
		"ReadNavState");
	AIPin_Check(xReadNav, 3u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "ReadNavState");

	const Zenith_GraphPinTable& xSpeed = Zenith_GraphNode_SetNavSpeed::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSpeed.GetPinCount(), 2u, "SetNavSpeed gained or lost a pin");
	AIPin_Check(xSpeed, Zenith_GraphNode_SetNavSpeed::uPIN_Speed, "Speed", GRAPH_PIN_ROLE_INPUT, "SetNavSpeed");
	AIPin_Check(xSpeed, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetNavSpeed");

	// ★ Radius is index 1 and Result index 2: Center is declared FIRST, so a copy
	// of a sibling TU's `uPIN_Result = 0u` would address a TARGET_REF pin.
	const Zenith_GraphPinTable& xWander = Zenith_GraphNode_FindRandomReachablePoint::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xWander.GetPinCount(), 4u, "FindRandomReachablePoint gained or lost a pin");
	AIPin_Check(xWander, 0u, "Center", GRAPH_PIN_ROLE_TARGET_REF, "FindRandomReachablePoint");
	AIPin_Check(xWander, Zenith_GraphNode_FindRandomReachablePoint::uPIN_Radius, "Radius", GRAPH_PIN_ROLE_INPUT,
		"FindRandomReachablePoint");
	AIPin_Check(xWander, Zenith_GraphNode_FindRandomReachablePoint::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"FindRandomReachablePoint");
	AIPin_Check(xWander, 3u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "FindRandomReachablePoint");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_FindRandomReachablePoint::uPIN_Result, 2u,
		"FindRandomReachablePoint.Result is pin 2 - a 0 here would address the Center TARGET pin");

	// Count is index 1: the LIST pin occupies index 0 without owning a constant.
	const Zenith_GraphPinTable& xPerceived = Zenith_GraphNode_QueryPerceivedTargets::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xPerceived.GetPinCount(), 3u, "QueryPerceivedTargets gained or lost a pin");
	AIPin_Check(xPerceived, 0u, "List", GRAPH_PIN_ROLE_LIST, "QueryPerceivedTargets");
	AIPin_Check(xPerceived, Zenith_GraphNode_QueryPerceivedTargets::uPIN_Count, "Count", GRAPH_PIN_ROLE_OUTPUT,
		"QueryPerceivedTargets");
	AIPin_Check(xPerceived, 2u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "QueryPerceivedTargets");

	const Zenith_GraphPinTable& xPrimary = Zenith_GraphNode_QueryPrimaryPerceivedTarget::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xPrimary.GetPinCount(), 2u, "QueryPrimaryPerceivedTarget gained or lost a pin");
	AIPin_Check(xPrimary, Zenith_GraphNode_QueryPrimaryPerceivedTarget::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"QueryPrimaryPerceivedTarget");
	AIPin_Check(xPrimary, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "QueryPrimaryPerceivedTarget");

	const Zenith_GraphPinTable& xHeard = Zenith_GraphNode_QueryLastHeardSound::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xHeard.GetPinCount(), 4u, "QueryLastHeardSound gained or lost a pin");
	AIPin_Check(xHeard, Zenith_GraphNode_QueryLastHeardSound::uPIN_Position, "Position", GRAPH_PIN_ROLE_OUTPUT,
		"QueryLastHeardSound");
	AIPin_Check(xHeard, Zenith_GraphNode_QueryLastHeardSound::uPIN_Source, "Source", GRAPH_PIN_ROLE_OUTPUT,
		"QueryLastHeardSound");
	AIPin_Check(xHeard, Zenith_GraphNode_QueryLastHeardSound::uPIN_Age, "Age", GRAPH_PIN_ROLE_OUTPUT,
		"QueryLastHeardSound");
	AIPin_Check(xHeard, 3u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "QueryLastHeardSound");

	// ★ Result is index 1: the `Of` reference is declared first.
	const Zenith_GraphPinTable& xAware = Zenith_GraphNode_QueryAwarenessOf::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAware.GetPinCount(), 3u, "QueryAwarenessOf gained or lost a pin");
	AIPin_Check(xAware, 0u, "Of", GRAPH_PIN_ROLE_TARGET_REF, "QueryAwarenessOf");
	AIPin_Check(xAware, Zenith_GraphNode_QueryAwarenessOf::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"QueryAwarenessOf");
	AIPin_Check(xAware, 2u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "QueryAwarenessOf");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_QueryAwarenessOf::uPIN_Result, 1u,
		"QueryAwarenessOf.Result is pin 1 - a 0 here would address the Of TARGET pin");
}

// The 9/7/5 discipline: the const says 9, a blackboard variable says 7, the wire
// says 5, and the agent's move speed is 5.
ZENITH_TEST(AIPinRuntime, Wired_SetNavSpeedFromWire)
{
	Zenith_AIPinFixture xFixture("TestAIPinSpeedScene");
	const u_int uSpeed = Zenith_GraphNode_SetNavSpeed::uPIN_Speed;

	Zenith_GraphBlackboard xBB;
	AIPin_SeedFloat(xBB, "speed", 7.0f);

	Zenith_GraphNode_SetNavSpeed xNode;
	xNode.m_fSpeed = 9.0f;
	xNode.m_strSpeedVar = "";
	xNode.m_strTargetVar = "";		// self
	xNode.SetInputForTest(uSpeed, AIPin_WireFloat(5.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAgent;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xNavAgent.GetMoveSpeed(), 5.0f, 0.001f,
		"the wire lost to the const (9) or the variable (7)");
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uSpeed), 0u, "a WIRED pin must never log a census fallback");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

// ★ SUCCESS ITSELF IS THE DISCRIMINATOR HERE, because the returned point is
// RANDOM: GetRandomReachablePointInRadius rejection-samples UNIFORMLY over the
// whole picked polygon with a 16-attempt budget, so the VALUE cannot separate the
// legs but the STATUS can.
//
// ★ THE FAILING RADIUS IS 0.01, NOT 0.1, and the two zeros are the difference
// between a deterministic row and a 1-in-200 flake: on this 10x10 quad a disc of
// radius 0.1 is 0.031 in 100, which 16 uniform samples hit about 0.5% of the time.
// At 0.01 it is 3e-6 per attempt - 5e-5 over the budget. (m_fRadius's declared
// editor range floors at 0.1; if anything ever clamped a const read to it, these
// legs would degrade to that same 99.5%, never to a wrong answer.) The succeeding
// radius is 6.0, which succeeds on ~95% of attempts, so the 16-attempt budget
// makes a FAILURE impossible in practice (~1e-21).
//
// Each leg gets its own FIXTURE, hence its own mesh, because the sampling RNG is
// per-instance and advances per call.
ZENITH_TEST(AIPinRuntime, Wired_FindRandomReachablePointRadius)
{
	const u_int uRadius = Zenith_GraphNode_FindRandomReachablePoint::uPIN_Radius;
	const u_int uResult = Zenith_GraphNode_FindRandomReachablePoint::uPIN_Result;
	const Zenith_Maths::Vector3 xCentre(5.0f, 0.0f, 5.0f);

	// (a) THE POSITIVE CONTROL: the const alone, at a radius that succeeds. Without
	//     it, the two failing legs below could mean "the node is broken".
	{
		Zenith_AIPinFixture xFixture("TestAIPinRadiusConstScene");
		Zenith_GraphBlackboard xBB;
		AIPin_SeedVec3(xBB, "centre", xCentre);
		Zenith_GraphNode_FindRandomReachablePoint xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strCenterVar = "centre";
		xNode.m_fRadius = 6.0f;
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xFixture.m_xAgent;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
			"radius 6.0 on a 10x10 quad must find a point");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uRadius), 0u, "no var is bound on this leg");
	}

	// (b) A resolved wire beats the contrary const: 0.01 must fail even though
	//     the node's own constant is the successful 6.0.
	{
		Zenith_AIPinFixture xFixture("TestAIPinRadiusVarScene");
		Zenith_GraphBlackboard xBB;
		AIPin_SeedVec3(xBB, "centre", xCentre);
		Zenith_GraphNode_FindRandomReachablePoint xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strCenterVar = "centre";
		xNode.m_fRadius = 6.0f;
		xNode.m_strRadiusVar = "";
		xNode.SetInputForTest(uRadius, AIPin_WireFloat(0.01f));
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xFixture.m_xAgent;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE),
			"the CONST 6.0 was used instead of the variable's 0.01");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uRadius), 0u);
	}

	// (c) THE WIRE BEATS BOTH: const 0.01, var 0.01, wire 6.0 -> SUCCESS, and the
	//     point is inside the WIRE's radius.
	{
		Zenith_AIPinFixture xFixture("TestAIPinRadiusWireScene");
		Zenith_GraphBlackboard xBB;
		AIPin_SeedVec3(xBB, "centre", xCentre);
		Zenith_GraphNode_FindRandomReachablePoint xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strCenterVar = "centre";
		xNode.m_fRadius = 0.01f;
		xNode.m_strRadiusVar = "";
		xNode.SetInputForTest(uRadius, AIPin_WireFloat(6.0f));
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xFixture.m_xAgent;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
			"only the WIRED radius can reach a point - a FAILURE means the pin was not read");

		const Zenith_Maths::Vector3 xPoint = AIPin_SlotVec3(xNode.GetOutputForTest(uResult),
			"FindRandomReachablePoint.Result");
		const float fDX = xPoint.x - xCentre.x;
		const float fDZ = xPoint.z - xCentre.z;
		ZENITH_ASSERT_TRUE(fDX * fDX + fDZ * fDZ <= 6.0f * 6.0f + 0.01f,
			"the point (%f, %f, %f) is outside the wired 6.0 XZ radius", xPoint.x, xPoint.y, xPoint.z);
		ZENITH_ASSERT_EQ(xBB.GetCount(), 1u);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uRadius), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// All three outputs by VALUE. ★ SetDestination alone proves almost nothing: it
// only marks the path PENDING and CLEARS the waypoints, so GetDistanceToGo reads 0
// and GetVelocity is the zero vector. ONE agent tick computes the path
// synchronously, and only then are Remaining and Velocity distinguishable from
// their stamped zeros.
ZENITH_TEST(AIPinRuntime, Output_ReadNavStateCarriesValues)
{
	Zenith_AIPinFixture xFixture("TestAIPinNavStateScene");
	const u_int uState = Zenith_GraphNode_ReadNavState::uPIN_State;
	const u_int uRemaining = Zenith_GraphNode_ReadNavState::uPIN_Remaining;
	const u_int uVelocity = Zenith_GraphNode_ReadNavState::uPIN_Velocity;

	ZENITH_ASSERT_TRUE(xFixture.m_xNavAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f)),
		"the agent has no navmesh - every leg below would be vacuous");

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xFixture.m_xAgent;

	// (a) PRE-TICK: the one non-zero fact available without a tick is the STATE
	//     code 1, "path pending".
	{
		Zenith_GraphBlackboard xBB;
		xCtx.m_pxBlackboard = &xBB;
		Zenith_GraphNode_ReadNavState xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strStateVar = "";
		xNode.m_strRemainingVar = "";
		xNode.m_strVelocityVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(AIPin_SlotInt(xNode.GetOutputForTest(uState), "pre-tick ReadNavState.State"), 1);
	}

	// One tick computes the path synchronously (the agent has no collider, so it
	// takes the transform-write path) and starts the agent accelerating.
	xFixture.m_xNavAgent.Update(0.02f, xFixture.m_xAgent.GetEntityID());

	// (b) NAMED: Remaining and Velocity are given names, so all three dual-write.
	{
		Zenith_GraphBlackboard xBB;
		xCtx.m_pxBlackboard = &xBB;
		Zenith_GraphNode_ReadNavState xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strRemainingVar = "";
		xNode.m_strVelocityVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

		const int32_t iState = AIPin_SlotInt(xNode.GetOutputForTest(uState), "ReadNavState.State");
		ZENITH_ASSERT_EQ(iState, 2, "a computed path with the agent still en route decodes as 2 = moving");
		ZENITH_ASSERT_NE(iState, 0, "0 is the INT32 slot's stamped zero - the row must not read it");

		// ★ AN INDEPENDENT BOUND, never a re-call of the same getter: (1,0,1) to
		// (9,0,9) is 11.3 m in a straight line, and any path is at least that.
		const float fRemaining = AIPin_SlotFloat(xNode.GetOutputForTest(uRemaining), "ReadNavState.Remaining");
		ZENITH_ASSERT_TRUE(fRemaining > 10.0f,
			"remaining distance %f is below the straight-line 11.3 m - there is no path", fRemaining);

		const Zenith_Maths::Vector3 xVelocity = AIPin_SlotVec3(xNode.GetOutputForTest(uVelocity),
			"ReadNavState.Velocity");
		ZENITH_ASSERT_TRUE(glm::dot(xVelocity, xVelocity) > 0.0001f, "the agent is not moving after a tick");

		// The dual-write lands exactly where today's SetValue did.
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	}

	// (c) THE PARITY LEG, on a FRESH node and a FRESH blackboard: m_strRemainingVar
	//     and m_strVelocityVar default to EMPTY, so those two writes used to be
	//     skipped entirely. They still create no blackboard variable - what is new
	//     is that both SLOTS carry the value, which is the only reason a wire can
	//     come off an unnamed Remaining.
	{
		Zenith_GraphBlackboard xFreshBB;
		xCtx.m_pxBlackboard = &xFreshBB;
		Zenith_GraphNode_ReadNavState xFresh;
		AIPin_ClearOutputs(xFresh);
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(AIPin_SlotFloat(xFresh.GetOutputForTest(uRemaining), "unnamed Remaining") > 10.0f);
		const Zenith_Maths::Vector3 xVelocity = AIPin_SlotVec3(xFresh.GetOutputForTest(uVelocity),
			"unnamed Velocity");
		ZENITH_ASSERT_TRUE(glm::dot(xVelocity, xVelocity) > 0.0001f,
			"the unnamed Velocity slot must still latch a moving value");
		ZENITH_ASSERT_NULL(xFreshBB.TryGetValue("left"), "a fresh node wrote a name it does not carry");
		ZENITH_ASSERT_NULL(xFreshBB.TryGetValue(""), "an EMPTY output name created a blackboard variable");
		ZENITH_ASSERT_EQ(xFreshBB.GetCount(), 0u, "cleared output names create no blackboard entries");
	}
}

// SHAPE A, stated once for the five nodes that share it. The resolver FAILURE is
// above every accessor, so the node never even self-bound: the slot reads as
// ABSENT, not as a stamped zero, and nothing was written.
ZENITH_TEST(AIPinRuntime, Output_ReadNavStateFailureBuildsNoSlotsAndWritesNothing)
{
	Zenith_AIPinFixture xFixture("TestAIPinNavStateFailScene");
	const u_int uState = Zenith_GraphNode_ReadNavState::uPIN_State;

	// A pending destination, so the preserved State is 1 and NOT the INT32 slot's
	// stamped zero - otherwise "the FAILURE did not overwrite it" would be
	// indistinguishable from "the FAILURE zeroed it".
	ZENITH_ASSERT_TRUE(xFixture.m_xNavAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f)));

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// SUCCESS first, on ONE instance, so the FAILURE below has something to fail
	// to preserve.
	Zenith_GraphNode_ReadNavState xNode;
	AIPin_ClearOutputs(xNode);
	xCtx.m_xSelf = xFixture.m_xAgent;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(AIPin_SlotInt(xNode.GetOutputForTest(uState), "ReadNavState.State"), 1);
	const u_int uBlackboardCount = xBB.GetCount();
	ZENITH_ASSERT_EQ(uBlackboardCount, 0u, "all output names are cleared");

	// The SAME instance, now aimed at an entity with no Zenith_AIAgentComponent.
	xCtx.m_xSelf = xFixture.m_xNoAgent;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(AIPin_SlotInt(xNode.GetOutputForTest(uState), "post-FAILURE State"), 1,
		"the FAILURE overwrote a slot it should not touch");
	ZENITH_ASSERT_EQ(xBB.GetCount(), uBlackboardCount, "the FAILURE wrote a blackboard variable");

	// THE SHAPE-A LEG, labelled as such: a FRESH instance that only ever fails has
	// built NO pin state at all - there is no slot to read, stamped zero or not.
	{
		Zenith_GraphNode_ReadNavState xFresh;
		AIPin_ClearOutputs(xFresh);
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(uState),
			"the FAILURE is above every accessor, so no pin state was ever built");
	}
}

// ★ ONE OF THE THREE DELIBERATE DIVERGENCES in this TU. FindRandomReachablePoint's
// Result write was ALWAYS unconditional, so an empty m_strResultVar used to create
// a blackboard variable literally named "". It no longer does; the slot still
// carries the point. (ReadNavState's three and QueryLastHeardSound's three were
// GUARDED, so their empty-name rows are PARITY, not divergence.)
ZENITH_TEST(AIPinRuntime, Output_FindRandomReachablePointEmptyResultVarCreatesNoBlackboardEntry)
{
	Zenith_AIPinFixture xFixture("TestAIPinWanderEmptyNameScene");
	const u_int uResult = Zenith_GraphNode_FindRandomReachablePoint::uPIN_Result;

	// ★ THE CENTRE IS THE MESH MIDDLE AND THE RADIUS ITS INSCRIBED 5.0, so every
	// legal answer is at least 2.07 m from the ORIGIN - which matters because
	// (0,0,0) is BOTH the VECTOR3 stamped zero AND a corner of this mesh. Without
	// that separation the row could not tell a real point from an unwritten slot.
	const Zenith_Maths::Vector3 xCentre(5.0f, 0.0f, 5.0f);

	Zenith_GraphBlackboard xBB;
	AIPin_SeedVec3(xBB, "centre", xCentre);

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAgent;

	Zenith_GraphNode_FindRandomReachablePoint xNode;
	AIPin_ClearOutputs(xNode);
	xNode.m_strCenterVar = "centre";
	xNode.m_fRadius = 5.0f;
	xNode.m_strResultVar = "";
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const Zenith_Maths::Vector3 xPoint = AIPin_SlotVec3(xNode.GetOutputForTest(uResult),
		"FindRandomReachablePoint.Result");
	ZENITH_ASSERT_TRUE(xPoint.x * xPoint.x + xPoint.z * xPoint.z > 1.0f,
		"the point (%f, %f, %f) is at the origin - that is the stamped zero, not an answer",
		xPoint.x, xPoint.y, xPoint.z);
	const float fDX = xPoint.x - xCentre.x;
	const float fDZ = xPoint.z - xCentre.z;
	ZENITH_ASSERT_TRUE(fDX * fDX + fDZ * fDZ <= 5.0f * 5.0f + 0.01f, "the point is outside the 5.0 XZ radius");

	ZENITH_ASSERT_NULL(xBB.TryGetValue(""), "an EMPTY result name created a blackboard variable");
	ZENITH_ASSERT_EQ(xBB.GetCount(), 1u, "only the permanent Centre selector is present");

	// A fresh instance independently produces a valid point; random samples need
	// not match one another.
	Zenith_GraphNode_FindRandomReachablePoint xNamed;
	AIPin_ClearOutputs(xNamed);
	xNamed.m_strCenterVar = "centre";
	xNamed.m_strResultVar = "";
	xNamed.m_fRadius = 5.0f;
	ZENITH_ASSERT_EQ(static_cast<int>(xNamed.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const Zenith_Maths::Vector3 xSecondPoint = AIPin_SlotVec3(xNamed.GetOutputForTest(uResult), "second Result");
	ZENITH_ASSERT_TRUE(xSecondPoint.x * xSecondPoint.x + xSecondPoint.z * xSecondPoint.z > 1.0f);
	const float fSecondDX = xSecondPoint.x - xCentre.x;
	const float fSecondDZ = xSecondPoint.z - xCentre.z;
	ZENITH_ASSERT_TRUE(fSecondDX * fSecondDX + fSecondDZ * fSecondDZ <= 25.0f + 0.01f);
}

// ★ SHAPE B, and this node is the ONLY place it occurs in the TU. The
// no-reachable-point FAILURE runs BELOW the Radius GetInput, so that execution HAS
// built pin state and Result reads its stamped (0,0,0) - the Raycast-miss shape
// from B-6.3. The two guards ABOVE the read are Shape A and build nothing. The
// difference is what a consumer sees, so it is pinned rather than described.
ZENITH_TEST(AIPinRuntime, Output_FindRandomReachablePointFailureShapesDiffer)
{
	Zenith_AIPinFixture xFixture("TestAIPinWanderFailShapeScene");
	const u_int uResult = Zenith_GraphNode_FindRandomReachablePoint::uPIN_Result;

	Zenith_GraphBlackboard xBB;
	AIPin_SeedVec3(xBB, "centre", Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f));
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// SHAPE B: the mesh and the centre both resolve, the Radius pin IS read, and
	// 0.01 exhausts the sampling budget (see Wired_FindRandomReachablePointRadius
	// for why it is two zeros and not one).
	{
		Zenith_GraphNode_FindRandomReachablePoint xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strCenterVar = "centre";
		xNode.m_fRadius = 0.01f;
		xNode.m_strResultVar = "";
		xCtx.m_xSelf = xFixture.m_xAgent;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NEAR_VEC3(AIPin_SlotVec3(xNode.GetOutputForTest(uResult), "no-point Result"),
			Zenith_Maths::Vector3(0.0f), 0.0001f,
			"the no-point FAILURE is BELOW the Radius read, so the slot exists and holds its stamped zero");
		ZENITH_ASSERT_NULL(xBB.TryGetValue("wander"), "a FAILED wander wrote its result var");
	}

	// SHAPE A: no bound agent, hence no mesh - the guard is ABOVE the read, so
	// nothing was built and there is no slot at all.
	{
		Zenith_GraphNode_FindRandomReachablePoint xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strCenterVar = "centre";
		xNode.m_fRadius = 5.0f;
		xCtx.m_xSelf = xFixture.m_xNoAgent;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xNode.GetOutputForTest(uResult),
			"the no-mesh FAILURE precedes every accessor, so no pin state was built");
	}
}

// Count by VALUE against a fixture with exactly ONE registered target, on a
// freshly-initialised perception system (both legs of the RAII scope CLEAR every
// bucket, so the 1 is this row's own and not a predecessor's leftover).
ZENITH_TEST(AIPinRuntime, Output_QueryPerceivedTargetsCountByValue)
{
	Zenith_AIPerceptionFixture xFixture("TestAIPinPerceivedScene");
	Zenith_AIPerceptionScope xPerception;
	xFixture.RegisterSeerAndPrey();

	const u_int uCount = Zenith_GraphNode_QueryPerceivedTargets::uPIN_Count;

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSeer;

	// (a) THE EMPTY-NAME PARITY LEG: the Count write used to sit inside
	//     `if (!m_strCountVar.empty())`. It no longer does, and the blackboard is
	//     unchanged - the SLOT is what is new.
	{
		Zenith_GraphNode_QueryPerceivedTargets xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strCountVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(AIPin_SlotInt(xNode.GetOutputForTest(uCount), "QueryPerceivedTargets.Count"), 1);
		ZENITH_ASSERT_NULL(xBB.TryGetValue(""), "an EMPTY count name created a blackboard variable");
		// The LIST store is a parallel container, so the VALUE count stays 0.
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
		ZENITH_ASSERT_EQ(xBB.GetListCount(), 1u, "the list name is still written directly, not through a pin");
	}

	// (b) THE NAMED LEG: m_strCountVar defaults to "perceivedCount", so this one
	//     dual-writes exactly as today.
	{
		Zenith_GraphNode_QueryPerceivedTargets xNamed;
		AIPin_ClearOutputs(xNamed);
		ZENITH_ASSERT_EQ(static_cast<int>(xNamed.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(AIPin_SlotInt(xNamed.GetOutputForTest(uCount), "named Count"), 1);
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
		ZENITH_ASSERT_NOT_NULL(xBB.TryGetList("perceived"));
		if (xBB.TryGetList("perceived") != nullptr)
		{
			ZENITH_ASSERT_EQ(xBB.TryGetList("perceived")->GetSize(), 1u);
			if (xBB.TryGetList("perceived")->GetSize() > 0u)
			{
				ZENITH_ASSERT_EQ(AIPin_SlotEntity(&xBB.TryGetList("perceived")->Get(0), "perceived entity"),
					xFixture.m_xPrey.GetEntityID().GetPacked());
			}
		}

		const u_int uBlackboardCount = xBB.GetCount();
		xNamed.m_strTargetVar = "__missing_target__";
		ZENITH_ASSERT_EQ(static_cast<int>(xNamed.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(AIPin_SlotInt(xNamed.GetOutputForTest(uCount), "post-FAILURE Count"), 1,
			"the FAILURE overwrote the earlier count");
		ZENITH_ASSERT_EQ(AIPin_SlotInt(xNamed.GetOutputForTest(uCount), "retained Count"), 1);
		ZENITH_ASSERT_EQ(xBB.GetCount(), uBlackboardCount, "the FAILURE wrote a blackboard variable");
		ZENITH_ASSERT_NOT_NULL(xBB.TryGetList("perceived"));
		if (xBB.TryGetList("perceived") != nullptr)
		{
			ZENITH_ASSERT_EQ(xBB.TryGetList("perceived")->GetSize(), 1u,
				"the FAILURE changed the perceived list size");
			if (xBB.TryGetList("perceived")->GetSize() > 0u)
			{
				ZENITH_ASSERT_EQ(AIPin_SlotEntity(&xBB.TryGetList("perceived")->Get(0), "preserved perceived entity"),
					xFixture.m_xPrey.GetEntityID().GetPacked(), "the FAILURE changed the perceived list contents");
			}
		}

		Zenith_GraphBlackboard xFreshBB;
		xCtx.m_pxBlackboard = &xFreshBB;
		Zenith_GraphNode_QueryPerceivedTargets xFresh;
		AIPin_ClearOutputs(xFresh);
		xFresh.m_strTargetVar = "__missing_target__";
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(uCount),
			"the target FAILURE is above every accessor, so no pin state was built");
		ZENITH_ASSERT_EQ(xFreshBB.GetCount(), 0u, "a fresh FAILURE wrote a blackboard variable");
		ZENITH_ASSERT_EQ(xFreshBB.GetListCount(), 0u, "a fresh FAILURE wrote a blackboard list");
	}
}

// The prey's EXACT packed id, asserted non-zero first: packed 0 is the ENTITY_ID
// slot's own stamped zero (and a LEGAL id - {index 0, generation 0} - not
// INVALID_ENTITY_ID), so the comparison would otherwise be unfalsifiable.
ZENITH_TEST(AIPinRuntime, Output_QueryPrimaryPerceivedTargetResultByValue)
{
	Zenith_AIPerceptionFixture xFixture("TestAIPinPrimaryScene");
	Zenith_AIPerceptionScope xPerception;
	xFixture.RegisterSeerAndPrey();

	const u_int uResult = Zenith_GraphNode_QueryPrimaryPerceivedTarget::uPIN_Result;
	const u_int64 ulPrey = xFixture.m_xPrey.GetEntityID().GetPacked();
	ZENITH_ASSERT_NE(ulPrey, 0ull, "the prey's packed id is the slot's stamped zero - the row proves nothing");

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSeer;

	// (a) the default name "target" -> dual-written.
	{
		Zenith_GraphNode_QueryPrimaryPerceivedTarget xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strResultVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(AIPin_SlotEntity(xNode.GetOutputForTest(uResult), "QueryPrimaryPerceivedTarget.Result"),
			ulPrey);
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	}

	// (b) THE `""` DIVERGENCE LEG: this write was always unconditional, so an empty
	//     name used to create a variable named "". It no longer does.
	{
		Zenith_GraphNode_QueryPrimaryPerceivedTarget xEmpty;
		AIPin_ClearOutputs(xEmpty);
		xEmpty.m_strResultVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xEmpty.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(AIPin_SlotEntity(xEmpty.GetOutputForTest(uResult), "unnamed Result"), ulPrey);
		ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	}
}

// SHAPE A again, on the node whose FAILURE a graph is most likely to route: no
// primary target means nothing was read, nothing was written, and a fresh instance
// has no slot at all.
ZENITH_TEST(AIPinRuntime, Output_QueryPrimaryPerceivedTargetFailureBuildsNoSlotsAndWritesNothing)
{
	Zenith_AIPerceptionFixture xFixture("TestAIPinPrimaryFailScene");
	Zenith_AIPerceptionScope xPerception;
	xFixture.RegisterSeerAndPrey();

	const u_int uResult = Zenith_GraphNode_QueryPrimaryPerceivedTarget::uPIN_Result;
	const u_int64 ulPrey = xFixture.m_xPrey.GetEntityID().GetPacked();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_QueryPrimaryPerceivedTarget xNode;
	AIPin_ClearOutputs(xNode);
	xCtx.m_xSelf = xFixture.m_xSeer;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const u_int uBlackboardCount = xBB.GetCount();

	// The SAME instance, aimed at the PREY - a valid entity that was never
	// registered as an AGENT, so GetPrimaryTarget answers INVALID_ENTITY_ID.
	xCtx.m_xSelf = xFixture.m_xPrey;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(AIPin_SlotEntity(xNode.GetOutputForTest(uResult), "post-FAILURE Result"), ulPrey,
		"the FAILURE overwrote the earlier result");
	ZENITH_ASSERT_EQ(xBB.GetCount(), uBlackboardCount, "the FAILURE wrote a blackboard variable");

	{
		Zenith_GraphNode_QueryPrimaryPerceivedTarget xFresh;
		AIPin_ClearOutputs(xFresh);
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(uResult),
			"the no-primary FAILURE is above the write, so no pin state was ever built");
	}
}

// ★ HEARING NEEDS NO RegisterTarget: the perceived-target entry is CREATED from the
// sound's source. So the prey is registered as nothing at all here, which is also
// what keeps SIGHT out of this row - the sight pass walks registered TARGETS only.
// Age is set to 0 on hearing and then advanced by the SAME Update's memory-decay
// pass, so it reads exactly the dt that was passed.
ZENITH_TEST(AIPinRuntime, Output_QueryLastHeardSoundCarriesAllThreeValues)
{
	Zenith_AIPerceptionFixture xFixture("TestAIPinHeardScene");
	Zenith_AIPerceptionScope xPerception;

	const u_int uPosition = Zenith_GraphNode_QueryLastHeardSound::uPIN_Position;
	const u_int uSource = Zenith_GraphNode_QueryLastHeardSound::uPIN_Source;
	const u_int uAge = Zenith_GraphNode_QueryLastHeardSound::uPIN_Age;
	const u_int64 ulPrey = xFixture.m_xPrey.GetEntityID().GetPacked();
	ZENITH_ASSERT_NE(ulPrey, 0ull, "the prey's packed id is the ENTITY_ID slot's stamped zero");

	Zenith_PerceptionSystem::RegisterAgent(xFixture.m_xSeer.GetEntityID());
	Zenith_HearingConfig xHearing;	// 20 m range, 0.1 loudness threshold
	Zenith_PerceptionSystem::SetHearingConfig(xFixture.m_xSeer.GetEntityID(), xHearing);

	// ★ EXPLICITLY OFF-ORIGIN, and 5 m from the seer: loudness 0.5 with a 10 m
	// radius falls off to 0.25 there, comfortably over the 0.1 threshold, and
	// (3,0,4) cannot be confused with the VECTOR3 stamped zero.
	const Zenith_Maths::Vector3 xSoundPos(3.0f, 0.0f, 4.0f);
	Zenith_PerceptionSystem::EmitSoundStimulus(xSoundPos, 0.5f, 10.0f, xFixture.m_xPrey.GetEntityID());
	Zenith_PerceptionSystem::Update(0.1f);

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSeer;

	// (a) DEFAULTS: Position is named "heardPos" (dual-written), Source and Age are
	//     EMPTY - the PARITY half, since both writes were guarded before.
	{
		Zenith_GraphNode_QueryLastHeardSound xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strPositionVar = "";
		xNode.m_strSourceVar = "";
		xNode.m_strAgeVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
			"the seer heard nothing - the stimulus never reached it");
		ZENITH_ASSERT_NEAR_VEC3(AIPin_SlotVec3(xNode.GetOutputForTest(uPosition), "QueryLastHeardSound.Position"),
			xSoundPos, 0.001f);
		ZENITH_ASSERT_EQ(AIPin_SlotEntity(xNode.GetOutputForTest(uSource), "QueryLastHeardSound.Source"), ulPrey);
		ZENITH_ASSERT_EQ_FLOAT(AIPin_SlotFloat(xNode.GetOutputForTest(uAge), "QueryLastHeardSound.Age"), 0.1f, 0.001f);
		ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	}

	// (b) NAMED: giving Source and Age names dual-writes them too - the same
	//     non-empty rule the deleted guards applied.
	{
		Zenith_GraphNode_QueryLastHeardSound xNamed;
		AIPin_ClearOutputs(xNamed);
		xNamed.m_strPositionVar = "";
		xNamed.m_strSourceVar = "";
		xNamed.m_strAgeVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNamed.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_NEAR_VEC3(AIPin_SlotVec3(xNamed.GetOutputForTest(uPosition), "named Position"), xSoundPos, 0.001f);
		ZENITH_ASSERT_EQ(AIPin_SlotEntity(xNamed.GetOutputForTest(uSource), "named Source"), ulPrey);
		ZENITH_ASSERT_EQ_FLOAT(AIPin_SlotFloat(xNamed.GetOutputForTest(uAge), "named Age"), 0.1f, 0.001f);
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);

		const u_int uBlackboardCount = xBB.GetCount();
		xCtx.m_xSelf = xFixture.m_xPrey;
		ZENITH_ASSERT_EQ(static_cast<int>(xNamed.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NEAR_VEC3(AIPin_SlotVec3(xNamed.GetOutputForTest(uPosition), "post-FAILURE Position"),
			xSoundPos, 0.001f, "the FAILURE overwrote the earlier Position");
		ZENITH_ASSERT_EQ(AIPin_SlotEntity(xNamed.GetOutputForTest(uSource), "post-FAILURE Source"), ulPrey,
			"the FAILURE overwrote the earlier Source");
		ZENITH_ASSERT_EQ_FLOAT(AIPin_SlotFloat(xNamed.GetOutputForTest(uAge), "post-FAILURE Age"), 0.1f, 0.001f,
			"the FAILURE overwrote the earlier Age");
		ZENITH_ASSERT_EQ(xBB.GetCount(), uBlackboardCount);
		ZENITH_ASSERT_EQ(xBB.GetCount(), uBlackboardCount, "the FAILURE wrote a blackboard variable");
	}

	// (c) SHAPE A: the prey is not a registered agent, so it has heard nothing.
	{
		Zenith_GraphBlackboard xFreshBB;
		xCtx.m_pxBlackboard = &xFreshBB;
		Zenith_GraphNode_QueryLastHeardSound xFresh;
		AIPin_ClearOutputs(xFresh);
		xCtx.m_xSelf = xFixture.m_xPrey;
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(uPosition),
			"the nothing-heard FAILURE is above every write, so no pin state was built");
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(uSource),
			"the nothing-heard FAILURE is above every write, so no pin state was built");
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(uAge),
			"the nothing-heard FAILURE is above every write, so no pin state was built");
		ZENITH_ASSERT_EQ(xFreshBB.GetCount(), 0u, "a fresh FAILURE wrote a blackboard variable");
	}
}

// Awareness by VALUE - a sighting drives it above zero, which the FLOAT stamped
// zero cannot imitate.
ZENITH_TEST(AIPinRuntime, Output_QueryAwarenessOfResultByValue)
{
	Zenith_AIPerceptionFixture xFixture("TestAIPinAwarenessScene");
	Zenith_AIPerceptionScope xPerception;
	xFixture.RegisterSeerAndPrey();

	const u_int uResult = Zenith_GraphNode_QueryAwarenessOf::uPIN_Result;

	Zenith_GraphBlackboard xBB;
	// m_strOfVar defaults to "target" - the SAME name QueryPrimaryPerceivedTarget
	// writes by default, which is the C-1 ledger note on that node.
	AIPin_SeedEntity(xBB, "target", xFixture.m_xPrey.GetEntityID().GetPacked());

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSeer;

	float fAwareness = 0.0f;
	{
		Zenith_GraphNode_QueryAwarenessOf xNode;
		AIPin_ClearOutputs(xNode);
		xNode.m_strResultVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		fAwareness = AIPin_SlotFloat(xNode.GetOutputForTest(uResult), "QueryAwarenessOf.Result");
		ZENITH_ASSERT_TRUE(fAwareness > 0.0f, "awareness is 0 - the slot is indistinguishable from unwritten");
		ZENITH_ASSERT_EQ(xBB.GetCount(), 1u);

		// SUCCESS-then-FAILURE on the SAME instance (Shape A): an `Of` reference that
		// resolves to nothing fails before the write, the earlier Result stays
		// latched and the blackboard gains nothing.
		const u_int uCountBefore = xBB.GetCount();
		xNode.m_strOfVar = "__nobody__";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ_FLOAT(AIPin_SlotFloat(xNode.GetOutputForTest(uResult), "QueryAwarenessOf.Result after FAILURE"), fAwareness, 0.0001f,
			"a FAILURE must leave the previously latched Result untouched");
		ZENITH_ASSERT_EQ(xBB.GetCount(), uCountBefore, "a FAILURE must write no variable");
	}

	// A FRESH Shape-A failure builds no Result slot. The seer remains valid and
	// __nobody__ is absent, so this isolates the unresolved Of reference.
	{
		const u_int uCountBefore = xBB.GetCount();
		Zenith_GraphNode_QueryAwarenessOf xFresh;
		AIPin_ClearOutputs(xFresh);
		xFresh.m_strOfVar = "__nobody__";
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(uResult),
			"the unresolved Of FAILURE is above every accessor, so no pin state was built");
		ZENITH_ASSERT_EQ(xBB.GetPackedEntityID("target"), xFixture.m_xPrey.GetEntityID().GetPacked());
		ZENITH_ASSERT_EQ(xBB.GetCount(), uCountBefore);
		ZENITH_ASSERT_EQ(xBB.GetCount(), uCountBefore, "a fresh FAILURE wrote a blackboard variable");
	}

	// THE `""` DIVERGENCE LEG: this write was always unconditional.
	{
		Zenith_GraphNode_QueryAwarenessOf xEmpty;
		AIPin_ClearOutputs(xEmpty);
		xEmpty.m_strResultVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xEmpty.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(AIPin_SlotFloat(xEmpty.GetOutputForTest(uResult), "unnamed Result") > 0.0f);
		ZENITH_ASSERT_NULL(xBB.TryGetValue(""), "an EMPTY result name created a blackboard variable");
	}
}

// ★ SetNavSpeed's read sits AFTER its nav-agent guard, so a FAILURE-before-read
// execution logs NO census fallback line for a var-BOUND pin. The positive control
// is the identical configuration on an entity that HAS an agent - without it, a
// zero count would be satisfied by a node that never reads anything.
//
// ★ BOTH INPUT VAR-NAME DEFAULTS IN THIS TU ARE EMPTY, and an unbound pin never
// logs, so every fallback row here ASSIGNS its var name.
ZENITH_TEST(AIPinRuntime, Fallback_GuardedFailureDoesNotReadInputs)
{
	EnsureAICountingRadiusProducerRegistered();
	Zenith_AIPinFixture xFixture("TestAIPinGuardScene");
	const auto Run = [](Zenith_Entity xSelf, bool bExpectSuccess)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSpeed = xDef.AddNode("SetNavSpeed");
		const u_int uProducer = xDef.AddNode("Test_AICountingRadiusProducer");
		const u_int uSentinel = xDef.AddNode("SetBlackboardBool");
		Zenith_GraphNode_SetNavSpeed xParams;
		xParams.m_strSpeedVar = "";
		xDef.SetNodeParamsFromInstance(uSpeed, &xParams);
		xDef.AddEdge(uSource, 0u, uSpeed);
		xDef.AddEdge(uSpeed, 0u, uSentinel);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uSpeed, "Speed"));
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		Zenith_GraphContext xCtx;
		xCtx.m_xSelf = xSelf;
		xCtx.m_pxGraph = &xGraph; xCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xCtx);
		ZENITH_ASSERT_EQ(xGraph.GetBlackboard().HasValue("flag"), bExpectSuccess);
	};
	Zenith_GraphNode_AITestCountingRadiusProducer::s_fValue = 7.0f;
	Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount = 0u;
	Run(xFixture.m_xNoAgent, false);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount, 0u,
		"the missing-agent guard must run before Speed is pulled");
	Run(xFixture.m_xAgent, true);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount, 1u);
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xNavAgent.GetMoveSpeed(), 7.0f, 0.001f);
}

// ★ FindRandomReachablePoint IS THE TU'S EXCEPTION to "the read is after every
// guard", pinned rather than left to be rediscovered. Radius is read after the
// mesh and centre resolves but BEFORE the no-reachable-point FAILURE, which is
// exactly where its blackboard read has always been - so a wander that found
// nothing has already read the pin (and, in a graph, already pulled Radius's
// producer), while a missing agent or an unresolvable centre has not.
ZENITH_TEST(AIPinRuntime, Fallback_RadiusReadBeforeNoPointFailure)
{
	EnsureAICountingRadiusProducerRegistered();
	Zenith_AIPinFixture xFixture("TestAIPinRadiusOrderScene");
	const auto Run = [](Zenith_Entity xSelf, const char* szCenter, const Zenith_Maths::Vector3& xCenter, float fRadius)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uFind = xDef.AddNode("FindRandomReachablePoint");
		const u_int uProducer = xDef.AddNode("Test_AICountingRadiusProducer");
		const u_int uSentinel = xDef.AddNode("SetBlackboardBool");
		ZENITH_ASSERT_NE(uFind, 0u);
		ZENITH_ASSERT_NE(uProducer, 0u);
		if (uFind == 0u || uProducer == 0u) return;
		Zenith_GraphNode_FindRandomReachablePoint xParams;
		xParams.m_strCenterVar = szCenter;
		xParams.m_strRadiusVar = "";
		xParams.m_strResultVar = "";
		xDef.SetNodeParamsFromInstance(uFind, &xParams);
		xDef.AddEdge(uSource, 0u, uFind);
		xDef.AddEdge(uFind, 0u, uSentinel);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uFind, "Radius"));
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		if (std::string(szCenter) != "noSuchCentre") xGraph.GetBlackboard().SetValue(szCenter, AIPin_WireVec3(xCenter));
		Zenith_GraphNode_AITestCountingRadiusProducer::s_fValue = fRadius;
		Zenith_GraphContext xCtx;
		xCtx.m_xSelf = xSelf;
		xCtx.m_pxGraph = &xGraph; xCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xCtx);
		ZENITH_ASSERT_FALSE(xGraph.GetBlackboard().HasValue("flag"), "the radius-order legs are FAILURE paths");
	};
	Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount = 0u;
	Run(xFixture.m_xAgent, "centre", Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f), 0.0f);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount, 1u);
	Run(xFixture.m_xAgent, "farCentre", Zenith_Maths::Vector3(500.0f, 0.0f, 500.0f), 2.0f);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount, 2u);
	Run(xFixture.m_xNoAgent, "centre", Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f), 2.0f);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount, 2u,
		"the missing-agent guard must run before Radius is pulled");
	Run(xFixture.m_xAgent, "noSuchCentre", Zenith_Maths::Vector3(0.0f), 2.0f);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AITestCountingRadiusProducer::s_uPullCount, 2u,
		"the unresolved Centre guard must run before Radius is pulled");
}

// The CENSUS observable. Only a MIGRATED node can reach the transitional var-name
// fallback, and it logs ONE line per (instance, pin) however hot the chain is -
// which is what makes "zero FALLBACK lines in a SUITE boot log" C-1's precondition
// rather than a guess.
ZENITH_TEST(AIPinRuntime, Fallback_CountsOncePerPin)
{
	Zenith_AIPinFixture xFixture("TestAIPinCountOnceScene");
	const u_int uSpeed = Zenith_GraphNode_SetNavSpeed::uPIN_Speed;

	Zenith_GraphBlackboard xBB;
	AIPin_SeedFloat(xBB, "speed", 7.0f);

	Zenith_GraphNode_SetNavSpeed xNode;
	xNode.m_fSpeed = 9.0f;
	xNode.m_strSpeedVar = "speed";		// ASSIGNED: no INPUT default here is non-empty

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAgent;
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uSpeed), 0u);

	for (u_int u = 0; u < 3u; ++u)
	{
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	}
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uSpeed), 1u, "three reads must log ONE census line");
	// A TARGET_REF pin is not an INPUT and has no fallback counter of its own.
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(1u), 0u);
}

// An unconnected pin takes its const; an actual wire with the wrong type also takes
// that const, reporting the pin mismatch. A fresh node is used for each leg because
// input state is latched after its first accessor call.
ZENITH_TEST(AIPinRuntime, Fallback_VarBoundButAbsentTakesTheConst)
{
	Zenith_AIPinFixture xFixture("TestAIPinAbsentVarScene");
	const u_int uSpeed = Zenith_GraphNode_SetNavSpeed::uPIN_Speed;

	Zenith_GraphBlackboard xBB;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAgent;

	// (a) the variable does not exist.
	{
		xFixture.m_xNavAgent.SetMoveSpeed(0.0f);
		Zenith_GraphNode_SetNavSpeed xNode;
		xNode.m_fSpeed = 3.0f;
		xNode.m_strSpeedVar = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xNavAgent.GetMoveSpeed(), 3.0f, 0.001f);
	}

	// (b) an actual INT32 wired into the FLOAT pin must reject the wire and take
	//     the const, reporting one mismatch.
	{
		xFixture.m_xNavAgent.SetMoveSpeed(0.0f);
		Zenith_GraphNode_SetNavSpeed xNode;
		xNode.m_fSpeed = 3.0f;
		xNode.m_strSpeedVar = "";
		Zenith_PropertyValue xWrongType;
		xWrongType.SetInt32(42);
		xNode.SetInputForTest(uSpeed, xWrongType);
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xNavAgent.GetMoveSpeed(), 3.0f, 0.001f,
			"a wrongly-tagged wire must take the CONST, not the type zero and not 42");
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uSpeed), 1u);
	}
}

#endif // ZENITH_TESTING
