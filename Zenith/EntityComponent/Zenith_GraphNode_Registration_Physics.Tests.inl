//------------------------------------------------------------------------------
// Routable-FAILURE coverage for the Physics node TU. Included at the bottom of
// Zenith_GraphNode_Registration_Physics.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes are still in scope.
//
// ONE table-driven test per owning TU. What each row proves, why the wired
// fail-probe is the positive control, and why the anchor/probes are engine
// nodes addressed by name all live ONCE, in the shared harness:
// Zenith_GraphNodeFailurePin.TestHarness.inl. This file carries only what is
// specific to this TU - the rows, their failing configuration, and the
// non-opted node.
//
// CROSS-REFERENCES, deliberately not duplicated here:
//   * the runtime SEMANTICS of the pin are the FailurePin_* tests in
//     Zenith/Scripting/Zenith_Scripting.Tests.inl;
//   * the DEEP CAUSE of a Raycast miss - a real body, a real world, a ray
//     aimed away from it - is fixtured in
//     Zenith_GraphComponent.Tests.inl:2349-2366.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphNodeFailurePin.TestHarness.inl"

ZENITH_TEST(GraphNodeFailurePin, PhysicsOptIns)
{
	const Zenith_FailurePinCase axCases[] =
	{
		// Raycast: FAILURE on NO HIT - the DOMINANT runtime branch, deliberately
		// not one of the two misconfiguration guards above it. A declared
		// VECTOR3 variable feeds m_strOriginVar (so the "" = self path, and its
		// unresolvable-origin guard, is never entered) and the direction is
		// non-zero (so the zero-direction guard is never entered); the ray then
		// leaves a unit-test world holding nothing at 1000 m up.
		{
			"Raycast",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				Zenith_PropertyValue xOrigin;
				xOrigin.SetVector3(Zenith_Maths::Vector3(0.0f, 1000.0f, 0.0f));
				xBuilder.Variable("failPinCastOrigin", xOrigin);
				xBuilder.ParamString(uNode, "m_strOriginVar", "failPinCastOrigin");
				xBuilder.ParamVec3(uNode, "m_xDirection", Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
				xBuilder.ParamFloat(uNode, "m_fMaxDistance", 10.0f);
			}
		},
	};

	// No scene: nothing in this row's failing path resolves an entity (self is
	// deliberately invalid, which only turns the ignore-self filter off).
	Zenith_CheckFailurePinTable(axCases, static_cast<u_int>(sizeof(axCases) / sizeof(axCases[0])), Zenith_Entity());

	// SetEntityPosition only ever fails on an author mistake (no target, no
	// transform, unresolvable position ref) - exactly the shape the opt-in rule
	// excludes.
	Zenith_CheckNodeIsNotOptedIn("SetEntityPosition");
}

//------------------------------------------------------------------------------
// Pin-table coverage for this TU. What the totality walk proves, why the
// registry is SWAPPED rather than filtered, and why the restore is RAII all live
// ONCE, in Zenith_GraphPinTotality.TestHarness.inl; only this TU's registrar and
// its representative pins are here.
//------------------------------------------------------------------------------

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, PhysicsTotality)
{
	// No exemptions: every m_str*Var* property in this TU is expressible as a
	// pin (there is no comma-separated name list here).
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Physics, "_Physics.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, PhysicsRoleSpotCheck)
{
	// INPUT_VAR_OR_CONST: the var wins when named, the inline vec3 otherwise -
	// the descriptor has to carry BOTH halves or the validator cannot tell the
	// "no variable at all" configuration from a missing writer.
	Zenith_CheckGraphPin("ApplyImpulse", "Impulse", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_VECTOR3, "m_strImpulseVar");
	const Zenith_GraphPinDesc* pxImpulse = Zenith_FindGraphPin("ApplyImpulse", "Impulse");
	ZENITH_ASSERT_NOT_NULL(pxImpulse, "ApplyImpulse must declare an Impulse pin");
	if (pxImpulse != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxImpulse->m_szConstProperty, "m_xImpulse",
			"ApplyImpulse.Impulse lost its inline-constant half - an unnamed var would read as unwritten");
	}

	// TARGET_ENTITY, reached through this TU's ResolveTargetBody wrapper.
	Zenith_CheckGraphPin("ApplyImpulse", "Target", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strTargetVar");

	// ★ TARGET_ENTITY and TARGET_POSITION share a role AND a type; only the
	// accepted-type MASK separates them, so a spot check that skips the mask
	// cannot tell "entity only" from "entity or vec3".
	const Zenith_GraphPinDesc* pxTarget = Zenith_FindGraphPin("SetEntityPosition", "Target");
	const Zenith_GraphPinDesc* pxPosition = Zenith_FindGraphPin("SetEntityPosition", "Position");
	ZENITH_ASSERT_NOT_NULL(pxTarget, "SetEntityPosition must declare a Target pin");
	ZENITH_ASSERT_NOT_NULL(pxPosition, "SetEntityPosition must declare a Position pin");
	if (pxTarget != nullptr && pxPosition != nullptr)
	{
		ZENITH_ASSERT_EQ(pxTarget->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY,
			"SetEntityPosition.Target must accept a packed ENTITY_ID and nothing else");
		ZENITH_ASSERT_EQ(pxPosition->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_POSITION,
			"SetEntityPosition.Position is a polymorphic position ref - ENTITY_ID or VECTOR3");
	}

	// OUTPUTs: the node's own computed results, typed by the
	// Zenith_PropertyValue::Set* each Execute actually calls.
	Zenith_CheckGraphPin("ReadVelocity", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR3, "m_strResultVar");
	Zenith_CheckGraphPin("Raycast", "HitEntity", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_ENTITY_ID, "m_strHitEntityVar");
	Zenith_CheckGraphPin("Raycast", "HitDistance", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_FLOAT, "m_strHitDistanceVar");
}

//==============================================================================
// PIN RUNTIME for this TU (B-6.3) - the pins are LIVE.
//
// Every node above now reads its INPUT descriptors through
// Zenith_GraphNode::GetInput and writes its OUTPUT descriptors through SetOutput.
// The three tests ABOVE, plus GraphComponent.PhysicsNodeFamilyExecution (whose
// ApplyImpulse/SetVelocity/SetAngularVelocity/ApplyForce instances bind NO var and
// therefore take the CONST path, and whose `vel`/`vel2`/`hitEntity`/`hitDist` all
// arrive through the dual-write), are the proof that an UNCONNECTED node is
// unchanged. The rows below are the proof that a WIRE now carries a value.
//
// ★ EVERY ROW ASSERTS THE EXECUTE STATUS FIRST. ResolveTargetBody fails before any
// physics accessor runs, so a row that checked a velocity without checking the
// status would pass just as happily on a node that did nothing at all.
//
// ★ THE FIXTURE IS REAL. Jolt runs under the Null backend (HasActiveSimulation is
// "the system exists"), so a DYNAMIC sphere built through
// Zenith_ColliderComponent::AddCollider has a live body in a headless unit boot -
// the recipe is GraphComponent.PhysicsNodeFamilyExecution's. AddImpulse is a
// velocity DELTA, and SetVelocity/SetAngularVelocity are assignments, so all three
// are observable with NO stepping; only ApplyForce needs
// g_xEngine.Physics().Update.
//
// ★ ORDERING RULE (B-6.1): pin state is built ONCE, on the first accessor call,
// from the properties as they read THEN. Assign every property before the first
// Execute. A const property is read LIVE through its reflected pointer, which is
// what lets the Raycast hit-then-miss row flip m_xDirection on one instance.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

#include "UnitTests/Zenith_TempScene.h"

// One row of the index contract: the constant (or the literal, for a pin no
// Execute addresses) names the pin it is documented as, with the role the
// migration assumed.
static void CheckPhysicsPin(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
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

static void SeedVec3(Zenith_GraphBlackboard& xBB, const char* szName, const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	xBB.SetValue(szName, xValue);
}

static Zenith_PropertyValue WireVec3(const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	return xValue;
}

// The three slot readers are TAG-CHECKED: Zenith_PropertyValue's typed getters
// Zenith_Assert on a mismatch, and a wrong slot type must read as a test FAILURE
// rather than a DebugBreak.
static Zenith_Maths::Vector3 SlotVec3(const Zenith_PropertyValue* pxSlot, const char* szWhat)
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

static float SlotFloat(const Zenith_PropertyValue* pxSlot, const char* szWhat)
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

static u_int64 SlotPackedEntity(const Zenith_PropertyValue* pxSlot, const char* szWhat)
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

// ★ OFF THE ORIGIN IN ALL THREE AXES, and not by accident. A body another suite
// leaked near the origin would be hit by a cast from (0,10,0) - and
// GraphComponent.PhysicsNodeFamilyExecution casts from exactly there - so every
// position here is somewhere no other fixture puts anything. It also makes a
// HitPoint with three NON-ZERO components, which a stamped zero cannot imitate.
//
// ★ AND EVERY FIXTURE INSTANCE GETS ITS OWN X LANE. If a scene unload ever failed
// to destroy a body, a second fixture casting down the SAME column would hit the
// FIRST one's floor and the HitEntity assertion would compare against the wrong
// entity - the kind of pass/fail that depends on id recycling. A monotonic lane
// makes that impossible instead of unlikely.
//
// The floor is a default-scale AABB with no model, so ComputeBoxDimensionsAndOffset
// falls back to the unit cube: half-extents 0.5, top face at y = +0.5. A cast from
// y = 10 straight down therefore travels 9.5 m.
namespace
{
	// A TempScene holding the three entities every row below needs: a STATIC
	// floor to be hit, a DYNAMIC sphere to be driven, and a transform-only entity
	// with NO collider - the guarded-FAILURE leg's target.
	struct Zenith_PhysicsPinFixture
	{
		explicit Zenith_PhysicsPinFixture(const char* szSceneName)
			: m_xScene(szSceneName)
		{
			static u_int ls_uLane = 0;
			const float fX = 500.3f + 40.0f * static_cast<float>(ls_uLane++);
			m_xFloorPosition = Zenith_Maths::Vector3(fX, 0.0f, 7.1f);
			m_xCastOrigin = Zenith_Maths::Vector3(fX, 10.0f, 7.1f);
			m_xHitPoint = Zenith_Maths::Vector3(fX, 0.5f, 7.1f);

			m_xFloor = m_xScene.CreateEntity("PinFloor");
			// The transform must be final BEFORE AddCollider: the static body is
			// created at the transform it reads then.
			m_xFloor.GetComponent<Zenith_TransformComponent>().SetPosition(m_xFloorPosition);
			m_xFloor.AddComponent<Zenith_ColliderComponent>()
				.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

			// The sphere sits in a different x AND z column from the floor, so a
			// downward cast can never reach it - the rows below drive it directly.
			m_xSphere = m_xScene.CreateEntity("PinSphere");
			m_xSphere.GetComponent<Zenith_TransformComponent>()
				.SetPosition(Zenith_Maths::Vector3(fX + 18.6f, 3.5f, 20.9f));
			Zenith_ColliderComponent& xCollider = m_xSphere.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
			// BOTH halves are asserted, not just recorded: m_bReady exists so a row
			// stops early rather than dereferencing a dead body, and a silent skip
			// would be a green test that proved nothing.
			ZENITH_ASSERT_TRUE(xCollider.HasValidBody(),
				"the dynamic sphere has no live body - every wired row below would be vacuous");
			ZENITH_ASSERT_TRUE(g_xEngine.Physics().HasActiveSimulation(),
				"no active Jolt simulation - ResolveTargetBody would FAILURE every row below");
			m_bReady = xCollider.HasValidBody() && g_xEngine.Physics().HasActiveSimulation();

			m_xNoBody = m_xScene.CreateEntity("PinNoBody");
		}

		Zenith_PhysicsBodyID BodyID() const
		{
			return m_xSphere.GetComponent<Zenith_ColliderComponent>().GetBodyID();
		}

		// Both velocities to exactly zero, so a delta assertion is an equality.
		void ResetVelocities() const
		{
			g_xEngine.Physics().SetLinearVelocity(BodyID(), Zenith_Maths::Vector3(0.0f));
			g_xEngine.Physics().SetAngularVelocity(BodyID(), Zenith_Maths::Vector3(0.0f));
		}

		Zenith_Maths::Vector3 LinearVelocity() const { return g_xEngine.Physics().GetLinearVelocity(BodyID()); }
		Zenith_Maths::Vector3 AngularVelocity() const { return g_xEngine.Physics().GetAngularVelocity(BodyID()); }

		Zenith_TempScene m_xScene;
		Zenith_Entity m_xFloor;
		Zenith_Entity m_xSphere;
		Zenith_Entity m_xNoBody;
		Zenith_Maths::Vector3 m_xFloorPosition{ 0.0f };
		Zenith_Maths::Vector3 m_xCastOrigin{ 0.0f };
		Zenith_Maths::Vector3 m_xHitPoint{ 0.0f };
		bool m_bReady = false;
	};
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses, so a
// reorder - or an inserted pin - silently re-points every uPIN_ constant in this TU
// at a different descriptor. ReadVelocity is the reason this test exists in the
// shape it does: its OUTPUT is index 1, because Target is declared first, and a
// SetOutput aimed at a TARGET_REF pin is a SILENT no-op. A static_assert is
// impossible: the tables are filled at static init.
ZENITH_TEST(GraphPinTable, PhysicsPinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xImpulse = Zenith_GraphNode_ApplyImpulse::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xImpulse.GetPinCount(), 2u, "ApplyImpulse gained or lost a pin");
	CheckPhysicsPin(xImpulse, Zenith_GraphNode_ApplyImpulse::uPIN_Impulse, "Impulse", GRAPH_PIN_ROLE_INPUT,
		"ApplyImpulse");
	CheckPhysicsPin(xImpulse, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "ApplyImpulse");

	const Zenith_GraphPinTable& xForce = Zenith_GraphNode_ApplyForce::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xForce.GetPinCount(), 2u, "ApplyForce gained or lost a pin");
	CheckPhysicsPin(xForce, Zenith_GraphNode_ApplyForce::uPIN_Force, "Force", GRAPH_PIN_ROLE_INPUT, "ApplyForce");
	CheckPhysicsPin(xForce, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "ApplyForce");

	const Zenith_GraphPinTable& xSetVel = Zenith_GraphNode_SetVelocity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetVel.GetPinCount(), 2u, "SetVelocity gained or lost a pin");
	CheckPhysicsPin(xSetVel, Zenith_GraphNode_SetVelocity::uPIN_Velocity, "Velocity", GRAPH_PIN_ROLE_INPUT,
		"SetVelocity");
	CheckPhysicsPin(xSetVel, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetVelocity");

	// ★ THE ONE THAT WOULD HAVE BEEN WRONG: Result is index 1, Target index 0.
	const Zenith_GraphPinTable& xReadVel = Zenith_GraphNode_ReadVelocity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xReadVel.GetPinCount(), 2u, "ReadVelocity gained or lost a pin");
	CheckPhysicsPin(xReadVel, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "ReadVelocity");
	CheckPhysicsPin(xReadVel, Zenith_GraphNode_ReadVelocity::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadVelocity");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_ReadVelocity::uPIN_Result, 1u,
		"ReadVelocity.Result is pin 1 - a 0 here would address the TARGET pin, and a wrong-role SetOutput is silent");

	const Zenith_GraphPinTable& xAngular = Zenith_GraphNode_SetAngularVelocity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAngular.GetPinCount(), 2u, "SetAngularVelocity gained or lost a pin");
	CheckPhysicsPin(xAngular, Zenith_GraphNode_SetAngularVelocity::uPIN_Velocity, "Velocity", GRAPH_PIN_ROLE_INPUT,
		"SetAngularVelocity");
	CheckPhysicsPin(xAngular, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetAngularVelocity");

	// The four classes whose Execute addresses NO pin declare no uPIN_ constant at
	// all - their only pin is a reference resolved directly. Asserting the count
	// and index 0 here is what would notice a value pin being ADDED to one without
	// its Execute being migrated.
	const Zenith_GraphPinTable& xLock = Zenith_GraphNode_LockRotation::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xLock.GetPinCount(), 1u, "LockRotation gained a pin; nothing in its Execute addresses one");
	CheckPhysicsPin(xLock, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "LockRotation");

	const Zenith_GraphPinTable& xGravity = Zenith_GraphNode_SetGravityEnabled::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xGravity.GetPinCount(), 1u, "SetGravityEnabled gained a pin");
	CheckPhysicsPin(xGravity, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetGravityEnabled");

	const Zenith_GraphPinTable& xSensor = Zenith_GraphNode_SetSensor::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSensor.GetPinCount(), 1u, "SetSensor gained a pin");
	CheckPhysicsPin(xSensor, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetSensor");

	const Zenith_GraphPinTable& xCast = Zenith_GraphNode_Raycast::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xCast.GetPinCount(), 6u, "Raycast gained or lost a pin");
	CheckPhysicsPin(xCast, 0u, "Origin", GRAPH_PIN_ROLE_TARGET_REF, "Raycast");
	CheckPhysicsPin(xCast, Zenith_GraphNode_Raycast::uPIN_Direction, "Direction", GRAPH_PIN_ROLE_INPUT, "Raycast");
	CheckPhysicsPin(xCast, Zenith_GraphNode_Raycast::uPIN_HitEntity, "HitEntity", GRAPH_PIN_ROLE_OUTPUT, "Raycast");
	CheckPhysicsPin(xCast, Zenith_GraphNode_Raycast::uPIN_HitPoint, "HitPoint", GRAPH_PIN_ROLE_OUTPUT, "Raycast");
	CheckPhysicsPin(xCast, Zenith_GraphNode_Raycast::uPIN_HitNormal, "HitNormal", GRAPH_PIN_ROLE_OUTPUT, "Raycast");
	CheckPhysicsPin(xCast, Zenith_GraphNode_Raycast::uPIN_HitDistance, "HitDistance", GRAPH_PIN_ROLE_OUTPUT,
		"Raycast");

	const Zenith_GraphPinTable& xSetPos = Zenith_GraphNode_SetEntityPosition::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetPos.GetPinCount(), 2u, "SetEntityPosition gained or lost a pin");
	CheckPhysicsPin(xSetPos, 0u, "Position", GRAPH_PIN_ROLE_TARGET_REF, "SetEntityPosition");
	CheckPhysicsPin(xSetPos, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetEntityPosition");
}

// AddImpulse is a mass-independent velocity DELTA, so from a zeroed body the
// resulting velocity IS the impulse - no stepping, no mass, no tolerance games.
ZENITH_TEST(PhysicsPinRuntime, Wired_ApplyImpulseFromWire)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinImpulseScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	xFixture.ResetVelocities();

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "imp", Zenith_Maths::Vector3(0.0f, 7.0f, 0.0f));	// leg 2: the var

	Zenith_GraphNode_ApplyImpulse xNode;
	xNode.m_xImpulse = Zenith_Maths::Vector3(9.0f, 0.0f, 0.0f);		// leg 1: the const
	xNode.m_strImpulseVar = "imp";
	xNode.m_strTargetVar = "";										// self
	xNode.SetInputForTest(Zenith_GraphNode_ApplyImpulse::uPIN_Impulse,
		WireVec3(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f)));			// leg 3: the wire

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSphere;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// THREE DISTINCT DIRECTIONS, so "which leg won" is not a magnitude question:
	// +Z is the wire, +X the const, +Y the blackboard.
	ZENITH_ASSERT_NEAR_VEC3(xFixture.LinearVelocity(), Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.01f);
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_ApplyImpulse::uPIN_Impulse), 0u);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

ZENITH_TEST(PhysicsPinRuntime, Wired_SetVelocityFromWire)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinSetVelScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uVelocity = Zenith_GraphNode_SetVelocity::uPIN_Velocity;

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "vel", Zenith_Maths::Vector3(0.0f, 7.0f, 0.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSphere;

	// (a) all three axes SET: the assignment is the wire, whole.
	{
		xFixture.ResetVelocities();
		Zenith_GraphNode_SetVelocity xNode;
		xNode.m_xVelocity = Zenith_Maths::Vector3(9.0f, 0.0f, 0.0f);
		xNode.m_strVelocityVar = "vel";
		xNode.SetInputForTest(uVelocity, WireVec3(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f)));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_NEAR_VEC3(xFixture.LinearVelocity(), Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.01f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uVelocity), 0u);
	}

	// (b) THE READ SITS ABOVE THE PER-AXIS BRANCH. Y is preserved, so the wired
	//     value's y is discarded AFTER being fetched - the fetch itself is
	//     unconditional, which is what keeps a wired producer pulled on an
	//     instance that preserves every axis.
	{
		xFixture.ResetVelocities();
		g_xEngine.Physics().SetLinearVelocity(xFixture.BodyID(), Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
		Zenith_GraphNode_SetVelocity xNode;
		xNode.m_xVelocity = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
		xNode.m_strVelocityVar = "vel";
		xNode.m_bSetY = false;
		xNode.SetInputForTest(uVelocity, WireVec3(Zenith_Maths::Vector3(0.0f, 99.0f, 5.0f)));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		// x and z from the wire; y preserved at 2, neither 99 nor 9.
		ZENITH_ASSERT_NEAR_VEC3(xFixture.LinearVelocity(), Zenith_Maths::Vector3(0.0f, 2.0f, 5.0f), 0.01f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uVelocity), 0u);
	}

	// (c) EVERY AXIS PRESERVED: the fetch still happens. A var-bound Velocity on an
	//     instance that preserves x, y AND z logs its census line (count 1) while the
	//     body keeps the velocity it had - which is what proves the read is not
	//     folded into the preserve branch.
	{
		xFixture.ResetVelocities();
		g_xEngine.Physics().SetLinearVelocity(xFixture.BodyID(), Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
		Zenith_GraphNode_SetVelocity xNode;
		xNode.m_strVelocityVar = "vel";
		xNode.m_bSetX = false;
		xNode.m_bSetY = false;
		xNode.m_bSetZ = false;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_NEAR_VEC3(xFixture.LinearVelocity(), Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 0.01f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uVelocity), 1u, "the read happens before the preserve branch");
	}
}

ZENITH_TEST(PhysicsPinRuntime, Wired_SetAngularVelocityFromWire)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinAngularScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	xFixture.ResetVelocities();

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "spin", Zenith_Maths::Vector3(0.0f, 7.0f, 0.0f));

	// The const half is m_xAngularVelocity, the var half m_strVelocityVar - both
	// behind ONE pin named Velocity.
	Zenith_GraphNode_SetAngularVelocity xNode;
	xNode.m_xAngularVelocity = Zenith_Maths::Vector3(9.0f, 0.0f, 0.0f);
	xNode.m_strVelocityVar = "spin";
	xNode.SetInputForTest(Zenith_GraphNode_SetAngularVelocity::uPIN_Velocity,
		WireVec3(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSphere;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_NEAR_VEC3(xFixture.AngularVelocity(), Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.01f);
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetAngularVelocity::uPIN_Velocity), 0u);
}

// The one node in the family that needs the simulation STEPPED: Jolt consumes an
// accumulated force on the next step. Gravity is switched off on the body first,
// so the assertion is per-AXIS and exact - +Z moved (the wire), +X did not (the
// const) and +Y did not (the blackboard). With gravity on, the Y axis would be
// dominated by g and could not separate the legs at all.
ZENITH_TEST(PhysicsPinRuntime, Wired_ApplyForceFromWire)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinForceScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	xFixture.ResetVelocities();
	g_xEngine.Physics().SetGravityEnabled(xFixture.BodyID(), false);

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "force", Zenith_Maths::Vector3(0.0f, 700.0f, 0.0f));

	Zenith_GraphNode_ApplyForce xNode;
	xNode.m_xForce = Zenith_Maths::Vector3(900.0f, 0.0f, 0.0f);
	xNode.m_strForceVar = "force";
	xNode.SetInputForTest(Zenith_GraphNode_ApplyForce::uPIN_Force,
		WireVec3(Zenith_Maths::Vector3(0.0f, 0.0f, 500.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSphere;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	for (u_int u = 0; u < 2u; ++u)
	{
		g_xEngine.Physics().Update(1.0f / 60.0f);
	}

	const Zenith_Maths::Vector3 xVelocity = xFixture.LinearVelocity();
	ZENITH_ASSERT_TRUE(xVelocity.z > 0.001f,
		"the wired +Z force never reached the body (vz = %f)", xVelocity.z);
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.x, 0.0f, 0.001f, "the CONST +X force was applied instead of the wire");
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.y, 0.0f, 0.001f, "the blackboard's +Y force was applied instead of the wire");
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_ApplyForce::uPIN_Force), 0u);
}

// ★ NO INPUT DEFAULT IN THIS TU IS NON-EMPTY, so a row that wants the var leg must
// ASSIGN a var name; m_strDirectionVar defaults to "".
ZENITH_TEST(PhysicsPinRuntime, Wired_RaycastDirectionFromWire)
{
	const u_int uDirection = Zenith_GraphNode_Raycast::uPIN_Direction;

	// (a) NO SCENE NEEDED. Origin comes from a seeded VECTOR3 variable, so the
	//     "" = self path (and its unresolvable-origin guard) is never entered, and
	//     the Direction read completes BEFORE the miss exit - which is the whole
	//     point of where that read sits.
	{
		Zenith_GraphBlackboard xBB;
		SeedVec3(xBB, "castOrigin", Zenith_Maths::Vector3(500.3f, 1000.0f, 7.1f));
		SeedVec3(xBB, "dir", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));

		Zenith_GraphNode_Raycast xNode;
		xNode.m_strOriginVar = "castOrigin";
		xNode.m_xDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		xNode.m_strDirectionVar = "dir";
		xNode.m_fMaxDistance = 10.0f;
		xNode.SetInputForTest(uDirection, WireVec3(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		// 1000 m up and 10 m of reach: every leg misses, so the STATUS is FAILURE
		// and the observable is that the wired pin was read without logging a
		// census fallback line.
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDirection), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) THE VALUE-LEVEL LEG: in the fixture scene the WIRE alone decides hit from
	//     miss. The const points up and the variable sideways (both miss the floor);
	//     only the wired -Y reaches it.
	{
		Zenith_PhysicsPinFixture xFixture("TestPhysicsPinCastDirScene");
		if (!xFixture.m_bReady)
		{
			return;
		}
		Zenith_GraphBlackboard xBB;
		SeedVec3(xBB, "castOrigin", xFixture.m_xCastOrigin);
		SeedVec3(xBB, "dir", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));

		Zenith_GraphNode_Raycast xNode;
		xNode.m_strOriginVar = "castOrigin";
		xNode.m_xDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		xNode.m_strDirectionVar = "dir";
		xNode.SetInputForTest(uDirection, WireVec3(Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f)));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
			"only the WIRED direction can hit the floor - a miss means the pin was not read");
		ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_Raycast::uPIN_HitDistance),
			"Raycast.HitDistance"), 9.5f, 0.05f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDirection), 0u);
	}
}

// All four outputs, by VALUE, against a fixture whose every component is distinct
// from a stamped zero. HitEntity is the floor's EXACT packed id - asserted non-zero
// first, because packed 0 is the ENTITY_ID slot's own stamped zero and would make
// the comparison unfalsifiable.
ZENITH_TEST(PhysicsPinRuntime, Output_RaycastHitCarriesAllFourValues)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinCastHitScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int64 ulFloor = xFixture.m_xFloor.GetEntityID().GetPacked();
	ZENITH_ASSERT_NE(ulFloor, 0ull,
		"the floor's packed id is 0, which is also the ENTITY_ID slot's stamped zero - the row cannot prove anything");

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "castOrigin", xFixture.m_xCastOrigin);

	Zenith_GraphNode_Raycast xNode;
	xNode.m_strOriginVar = "castOrigin";
	xNode.m_xDirection = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	// The defaults: hitEntity / hitPoint are NAMED (so they dual-write), hitNormal /
	// hitDistance are EMPTY (slot only) - the 2/2 split is deliberate here.
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_EQ(SlotPackedEntity(xNode.GetOutputForTest(Zenith_GraphNode_Raycast::uPIN_HitEntity),
		"Raycast.HitEntity"), ulFloor);
	ZENITH_ASSERT_NEAR_VEC3(SlotVec3(xNode.GetOutputForTest(Zenith_GraphNode_Raycast::uPIN_HitPoint),
		"Raycast.HitPoint"), xFixture.m_xHitPoint, 0.05f);
	ZENITH_ASSERT_NEAR_VEC3(SlotVec3(xNode.GetOutputForTest(Zenith_GraphNode_Raycast::uPIN_HitNormal),
		"Raycast.HitNormal"), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.01f);
	ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_Raycast::uPIN_HitDistance),
		"Raycast.HitDistance"), 9.5f, 0.05f);

	// PARITY on the blackboard: exactly the two named outputs, exactly as today.
	ZENITH_ASSERT_EQ(xBB.GetPackedEntityID("hitEntity"), ulFloor);
	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("hitPoint"), xFixture.m_xHitPoint, 0.05f);
	ZENITH_ASSERT_NULL(xBB.TryGetValue("hitNormal"),
		"an EMPTY HitNormal var name must still create no blackboard entry - only the slot is latched");
	ZENITH_ASSERT_NULL(xBB.TryGetValue("hitDistance"));
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

// HIT, then MISS, on ONE instance: a miss must touch NOTHING, so the four slots
// still hold the hit and the blackboard is unchanged. A consumer wire off any hit
// output has to be gated on SUCCESS (that is what the FAILURE exec pin is for) -
// this row is the proof that the alternative reading ("a miss clears the outputs")
// is not what the node does.
ZENITH_TEST(PhysicsPinRuntime, Output_RaycastMissKeepsTheHitValuesAndWritesNothing)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinCastMissScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int64 ulFloor = xFixture.m_xFloor.GetEntityID().GetPacked();
	const u_int uHitEntity = Zenith_GraphNode_Raycast::uPIN_HitEntity;
	const u_int uHitPoint = Zenith_GraphNode_Raycast::uPIN_HitPoint;
	const u_int uHitNormal = Zenith_GraphNode_Raycast::uPIN_HitNormal;
	const u_int uHitDistance = Zenith_GraphNode_Raycast::uPIN_HitDistance;

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "castOrigin", xFixture.m_xCastOrigin);

	Zenith_GraphNode_Raycast xNode;
	xNode.m_strOriginVar = "castOrigin";
	xNode.m_xDirection = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(SlotPackedEntity(xNode.GetOutputForTest(uHitEntity), "hit HitEntity"), ulFloor);
	const u_int uBlackboardCount = xBB.GetCount();

	// The same instance now misses. m_xDirection is read LIVE through its reflected
	// pointer, so flipping the const after the bind is legitimate - it is the pin
	// state (var names, slot types) that is latched once, not the const's value.
	xNode.m_xDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));

	ZENITH_ASSERT_EQ(SlotPackedEntity(xNode.GetOutputForTest(uHitEntity), "miss HitEntity"), ulFloor,
		"the miss overwrote a hit output");
	ZENITH_ASSERT_NEAR_VEC3(SlotVec3(xNode.GetOutputForTest(uHitPoint), "miss HitPoint"), xFixture.m_xHitPoint, 0.05f);
	ZENITH_ASSERT_NEAR_VEC3(SlotVec3(xNode.GetOutputForTest(uHitNormal), "miss HitNormal"),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.01f);
	ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(uHitDistance), "miss HitDistance"), 9.5f, 0.05f);
	ZENITH_ASSERT_EQ(xBB.GetCount(), uBlackboardCount, "the miss wrote a blackboard variable");
	ZENITH_ASSERT_NULL(xBB.TryGetValue("hitNormal"));

	// SECONDARY LEG, labelled as such: a FRESH instance that only ever misses reads
	// its STAMPED ZEROS. ★ The ENTITY_ID zero is packed 0 = {index 0, generation 0},
	// a LEGAL id - not INVALID_ENTITY_ID - so an ungated consumer sees a plausible
	// entity rather than an obviously invalid one.
	{
		Zenith_GraphBlackboard xFreshBB;
		// The SAME column, deliberately: this instance would hit the floor if its
		// direction were down, so the stamped zeros below are a property of the
		// MISS and not of an origin that reaches nothing.
		SeedVec3(xFreshBB, "castOrigin", xFixture.m_xCastOrigin);
		Zenith_GraphNode_Raycast xFresh;
		xFresh.m_strOriginVar = "castOrigin";
		xFresh.m_xDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		Zenith_GraphContext xFreshCtx;
		xFreshCtx.m_pxBlackboard = &xFreshBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xFreshCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(SlotPackedEntity(xFresh.GetOutputForTest(uHitEntity), "fresh HitEntity"), 0ull);
		ZENITH_ASSERT_NEAR_VEC3(SlotVec3(xFresh.GetOutputForTest(uHitPoint), "fresh HitPoint"),
			Zenith_Maths::Vector3(0.0f), 0.0001f);
		ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xFresh.GetOutputForTest(uHitDistance), "fresh HitDistance"), 0.0f, 0.0001f);
		ZENITH_ASSERT_NULL(xFreshBB.TryGetValue("hitEntity"), "a miss dual-wrote its stamped zero");
	}
}

// ★ THE ONE DELIBERATE DIVERGENCE from today's behaviour in this TU, and it lives
// on ReadVelocity because that is the TU's only UNGUARDED writer: an OUTPUT whose
// var name reads EMPTY no longer creates a blackboard variable literally named "".
// Today's unconditional SetValue(m_strResultVar) did. Nothing can have depended on
// it - a graph cannot declare, wire or read an empty name. Raycast takes NO
// divergence: its four writes were already guarded on a non-empty name.
ZENITH_TEST(PhysicsPinRuntime, Output_ReadVelocityEmptyResultVarCreatesNoBlackboardEntry)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinReadVelScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	// A NON-ZERO, three-distinct-component velocity: a stamped zero cannot imitate
	// it. AddImpulse is a velocity delta, so from a zeroed body this IS the value.
	const Zenith_Maths::Vector3 xVelocity(1.5f, 7.0f, -2.5f);
	xFixture.ResetVelocities();
	g_xEngine.Physics().AddImpulse(xFixture.BodyID(), xVelocity);

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xSphere;

	Zenith_GraphNode_ReadVelocity xNode;
	xNode.m_strResultVar = "";
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// The SLOT carries the value - it is not lost, it simply has no dual-write.
	ZENITH_ASSERT_NEAR_VEC3(SlotVec3(xNode.GetOutputForTest(Zenith_GraphNode_ReadVelocity::uPIN_Result),
		"ReadVelocity.Result"), xVelocity, 0.01f);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u, "an unnamed OUTPUT created a blackboard variable");

	// The NAMED twin, as the positive control: the dual-write lands exactly where
	// today's SetValue did.
	Zenith_GraphNode_ReadVelocity xNamed;
	xNamed.m_strResultVar = "velocity";
	ZENITH_ASSERT_EQ(static_cast<int>(xNamed.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("velocity"), xVelocity, 0.01f);
}

// ReadVelocity's FAILURE shape, stated once for it and Raycast both: the guard is
// ABOVE the write, so a bodyless target writes nothing at all - and because no
// accessor ran, the node never even self-bound, which is why the slot reads as
// absent rather than as a zero.
ZENITH_TEST(PhysicsPinRuntime, Output_ReadVelocityFailureWritesNothing)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinReadVelFailScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xNoBody;		// a transform, no collider

	Zenith_GraphNode_ReadVelocity xNode;
	xNode.m_strResultVar = "velocity";
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_NULL(xBB.TryGetValue("velocity"), "a FAILED ReadVelocity wrote its result var");
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	ZENITH_ASSERT_NULL(xNode.GetOutputForTest(Zenith_GraphNode_ReadVelocity::uPIN_Result),
		"the FAILURE is above every accessor, so no pin state was ever built - there is no slot to read");
}

// ★ EVERY INPUT READ IN THIS TU SITS AFTER ITS BODY GUARD (Raycast's Direction is
// the documented exception, covered by the row below). A FAILURE-before-read
// execution must therefore log NO census fallback line for a var-BOUND pin, and the
// positive control is the identical configuration on a body that exists.
ZENITH_TEST(PhysicsPinRuntime, Fallback_GuardedFailureDoesNotReadInputs)
{
	Zenith_PhysicsPinFixture xFixture("TestPhysicsPinGuardScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uImpulse = Zenith_GraphNode_ApplyImpulse::uPIN_Impulse;

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "imp", Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// LEG A: a transform but NO collider -> FAILURE, and the var-bound Impulse pin
	// was never read.
	{
		Zenith_GraphNode_ApplyImpulse xNode;
		xNode.m_strImpulseVar = "imp";
		xCtx.m_xSelf = xFixture.m_xNoBody;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uImpulse), 0u,
			"the Impulse read moved ABOVE the body guard - a failed node pulled its input");
	}

	// LEG B, THE POSITIVE CONTROL: the same configuration on the sphere reads the
	// pin, takes the var-name fallback, and logs exactly one line.
	{
		xFixture.ResetVelocities();
		Zenith_GraphNode_ApplyImpulse xNode;
		xNode.m_strImpulseVar = "imp";
		xCtx.m_xSelf = xFixture.m_xSphere;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uImpulse), 1u);
		ZENITH_ASSERT_NEAR_VEC3(xFixture.LinearVelocity(), Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.01f);
	}
}

// ★ RAYCAST IS THE EXCEPTION, pinned rather than left to be rediscovered. Direction
// is read AFTER the origin resolve but BEFORE the zero-direction and NO-HIT exits,
// which is exactly where its blackboard read has always been - so a miss and a zero
// direction have BOTH already read the pin (and, in a graph, already pulled
// Direction's producer), while an unresolvable ORIGIN has not.
ZENITH_TEST(PhysicsPinRuntime, Fallback_RaycastReadsDirectionBeforeMissAndZeroDirection)
{
	const u_int uDirection = Zenith_GraphNode_Raycast::uPIN_Direction;

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "castOrigin", Zenith_Maths::Vector3(500.3f, 1000.0f, 7.1f));
	SeedVec3(xBB, "dir", Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// (a) a MISS: FAILURE, and the pin WAS read.
	{
		Zenith_GraphNode_Raycast xNode;
		xNode.m_strOriginVar = "castOrigin";
		xNode.m_strDirectionVar = "dir";
		xNode.m_fMaxDistance = 10.0f;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDirection), 1u,
			"a MISS must have read Direction already - the read sits above the miss exit");
	}

	// (b) a ZERO DIRECTION: FAILURE at the guard immediately BELOW the read, so the
	//     pin was read too.
	{
		SeedVec3(xBB, "zeroDir", Zenith_Maths::Vector3(0.0f));
		Zenith_GraphNode_Raycast xNode;
		xNode.m_strOriginVar = "castOrigin";
		xNode.m_strDirectionVar = "zeroDir";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDirection), 1u);
	}

	// (c) an UNRESOLVABLE ORIGIN: the one Raycast guard ABOVE the read, so the pin
	//     was NOT read.
	{
		Zenith_GraphNode_Raycast xNode;
		xNode.m_strOriginVar = "noSuchOrigin";
		xNode.m_strDirectionVar = "dir";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDirection), 0u,
			"the origin guard is above the Direction read, so an unresolvable origin must read no pin");
	}
}

// The CENSUS observable. Only a MIGRATED node can reach the transitional var-name
// fallback, and it logs ONE line per (instance, pin) however hot the chain is -
// which is what makes "zero FALLBACK lines in a SUITE boot log" C-1's precondition
// rather than a guess. Raycast carries it here because it needs no scene: a miss
// still completes the read.
ZENITH_TEST(PhysicsPinRuntime, Fallback_CountsOncePerPin)
{
	const u_int uDirection = Zenith_GraphNode_Raycast::uPIN_Direction;

	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "castOrigin", Zenith_Maths::Vector3(500.3f, 1000.0f, 7.1f));
	SeedVec3(xBB, "dir", Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

	Zenith_GraphNode_Raycast xNode;
	xNode.m_strOriginVar = "castOrigin";
	xNode.m_xDirection = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xNode.m_strDirectionVar = "dir";		// ASSIGNED: no INPUT default here is non-empty
	xNode.m_fMaxDistance = 10.0f;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDirection), 0u);

	for (u_int u = 0; u < 3u; ++u)
	{
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	}
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDirection), 1u, "three reads must log ONE census line");
}

#endif // ZENITH_TESTING
