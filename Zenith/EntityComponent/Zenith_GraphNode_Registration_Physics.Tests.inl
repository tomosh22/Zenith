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

#endif // ZENITH_TESTING
