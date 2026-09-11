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
		// Raycast: FAILURE at Registration_Physics.cpp:302 (NO HIT) - the
		// DOMINANT runtime branch, deliberately not one of the two
		// misconfiguration guards above it. A declared VECTOR3 variable feeds
		// m_strOriginVar (so the "" = self path, and its :286 guard, is never
		// entered) and the direction is non-zero (so :293 is never entered);
		// the ray then leaves a unit-test world holding nothing at 1000 m up.
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

#endif // ZENITH_TESTING
